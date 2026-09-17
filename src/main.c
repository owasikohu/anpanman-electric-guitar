#include <stdbool.h>
#include <stdint.h>

#include "bsp/board_api.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "pico/stdlib.h"
#include "tusb.h"

#if TUSB_VERSION_NUMBER >= 2100
#define audio_control_range_4_n_t audio20_control_range_4_n_t
#define audio_control_range_2_n_t audio20_control_range_2_n_t
#define audio_control_cur_1_t audio20_control_cur_1_t
#define audio_control_cur_2_t audio20_control_cur_2_t
#define audio_desc_channel_cluster_t audio20_desc_channel_cluster_t
#define AUDIO_CS_REQ_CUR AUDIO20_CS_REQ_CUR
#define AUDIO_CS_REQ_RANGE AUDIO20_CS_REQ_RANGE
#define AUDIO_FU_CTRL_MUTE AUDIO20_FU_CTRL_MUTE
#define AUDIO_FU_CTRL_VOLUME AUDIO20_FU_CTRL_VOLUME
#define AUDIO_TE_CTRL_CONNECTOR AUDIO20_TE_CTRL_CONNECTOR
#define AUDIO_CS_CTRL_SAM_FREQ AUDIO20_CS_CTRL_SAM_FREQ
#define AUDIO_CS_CTRL_CLK_VALID AUDIO20_CS_CTRL_CLK_VALID
#endif

#define SAMPLE_RATE       48000u
#define SAMPLES_PER_MS    (SAMPLE_RATE / 1000u)
#define ADC_GPIO          26u
#define ADC_INPUT         0u
#define ADC_CENTER        2048
#define DMA_BLOCK_COUNT   16u

static uint16_t adc_samples[DMA_BLOCK_COUNT][SAMPLES_PER_MS] __attribute__((aligned(4)));
static int dma_channel;
static volatile uint32_t produced_blocks;
static uint32_t consumed_blocks;
static bool streaming;

static uint32_t sample_frequency = SAMPLE_RATE;
static uint8_t clock_valid = 1;
static audio_control_range_4_n_t(1) sample_frequency_range;
static bool mute[2];
static int16_t volume[2];

static void dma_irq_handler(void) {
    dma_hw->ints0 = 1u << (uint)dma_channel;
    ++produced_blocks;

    uint32_t next = produced_blocks & (DMA_BLOCK_COUNT - 1u);
    dma_channel_set_write_addr((uint)dma_channel, adc_samples[next], false);
    dma_channel_set_trans_count((uint)dma_channel, SAMPLES_PER_MS, true);
}

static void adc_dma_init(void) {
    adc_init();
    adc_gpio_init(ADC_GPIO);
    adc_select_input(ADC_INPUT);
    adc_fifo_setup(true, true, 1, false, false);
    adc_fifo_drain();

    /* RP2040 ADC clock is 48 MHz. One conversion every 1000 clocks = 48 kHz.
       1000 is safely above the converter's 96-clock minimum conversion time. */
    adc_set_clkdiv(48000000.0f / (float)SAMPLE_RATE);

    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config((uint)dma_channel);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);
    channel_config_set_dreq(&cfg, DREQ_ADC);
    dma_channel_configure((uint)dma_channel, &cfg, adc_samples[0], &adc_hw->fifo,
                          SAMPLES_PER_MS, false);
    dma_channel_set_irq0_enabled((uint)dma_channel, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_channel_start((uint)dma_channel);
    adc_run(true);
}

static void audio_task(void) {
    static int16_t packet[SAMPLES_PER_MS];
    static uint32_t packet_bytes;
    static uint32_t sent_bytes;

    /* Do not touch the TinyUSB audio FIFO until the host selected AS alt 1. */
    if (!streaming) {
        packet_bytes = sent_bytes = 0;
        consumed_blocks = produced_blocks;
        return;
    }

    if (packet_bytes != 0) {
        uint16_t sent = tud_audio_write((uint8_t *)packet + sent_bytes,
                                        (uint16_t)(packet_bytes - sent_bytes));
        sent_bytes += sent;
        if (sent_bytes == packet_bytes) packet_bytes = sent_bytes = 0;
        return;
    }

    if (consumed_blocks == produced_blocks) return;

    /* Drop old blocks if USB servicing was delayed for more than the ring depth. */
    if (produced_blocks - consumed_blocks > DMA_BLOCK_COUNT)
        consumed_blocks = produced_blocks - DMA_BLOCK_COUNT;

    uint16_t const *block = adc_samples[consumed_blocks & (DMA_BLOCK_COUNT - 1u)];
    for (uint32_t i = 0; i < SAMPLES_PER_MS; ++i) {
        int32_t centered = (int32_t)block[i] - ADC_CENTER;
        packet[i] = (int16_t)(centered << 4); /* 12-bit ADC to signed 16-bit PCM */
    }
    ++consumed_blocks;
    packet_bytes = sizeof(packet);
}

int main(void) {
    board_init();
    tusb_rhport_init_t usb_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO,
    };
    tusb_init(BOARD_TUD_RHPORT, &usb_init);
    board_init_after_tusb();

    sample_frequency_range.wNumSubRanges = 1;
    sample_frequency_range.subrange[0].bMin = SAMPLE_RATE;
    sample_frequency_range.subrange[0].bMax = SAMPLE_RATE;
    sample_frequency_range.subrange[0].bRes = 0;

    adc_dma_init();
    while (true) {
        tud_task();
        audio_task();
    }
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) {
    (void)rhport; (void)request; (void)buffer;
    return false;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) {
    (void)rhport; (void)request; (void)buffer;
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request, uint8_t *buffer) {
    (void)rhport;
    uint8_t channel = TU_U16_LOW(request->wValue);
    uint8_t selector = TU_U16_HIGH(request->wValue);
    uint8_t entity = TU_U16_HIGH(request->wIndex);
    if (request->bRequest != AUDIO_CS_REQ_CUR || entity != 2 || channel > 1) return false;
    if (selector == AUDIO_FU_CTRL_MUTE && request->wLength == sizeof(audio_control_cur_1_t)) {
        mute[channel] = ((audio_control_cur_1_t *)buffer)->bCur != 0;
        return true;
    }
    if (selector == AUDIO_FU_CTRL_VOLUME && request->wLength == sizeof(audio_control_cur_2_t)) {
        volume[channel] = ((audio_control_cur_2_t *)buffer)->bCur;
        return true;
    }
    return false;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *request) {
    (void)rhport; (void)request;
    return false;
}

bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *request) {
    (void)rhport; (void)request;
    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *request) {
    uint8_t channel = TU_U16_LOW(request->wValue);
    uint8_t selector = TU_U16_HIGH(request->wValue);
    uint8_t entity = TU_U16_HIGH(request->wIndex);

    if (entity == 1 && selector == AUDIO_TE_CTRL_CONNECTOR) {
        audio_desc_channel_cluster_t cluster = { .bNrChannels = 1, .bmChannelConfig = 0, .iChannelNames = 0 };
        return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &cluster, sizeof(cluster));
    }
    if (entity == 2 && channel <= 1) {
        if (selector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR)
            return tud_control_xfer(rhport, request, &mute[channel], sizeof(mute[channel]));
        if (selector == AUDIO_FU_CTRL_VOLUME && request->bRequest == AUDIO_CS_REQ_CUR)
            return tud_control_xfer(rhport, request, &volume[channel], sizeof(volume[channel]));
        if (selector == AUDIO_FU_CTRL_VOLUME && request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_2_n_t(1) range = {
                .wNumSubRanges = 1,
                .subrange = {{ .bMin = 0, .bMax = 0, .bRes = 0 }}
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &range, sizeof(range));
        }
    }
    if (entity == 4) {
        if (selector == AUDIO_CS_CTRL_SAM_FREQ && request->bRequest == AUDIO_CS_REQ_CUR)
            return tud_audio_buffer_and_schedule_control_xfer(rhport, request, &sample_frequency, sizeof(sample_frequency));
        if (selector == AUDIO_CS_CTRL_SAM_FREQ && request->bRequest == AUDIO_CS_REQ_RANGE)
            return tud_control_xfer(rhport, request, &sample_frequency_range, sizeof(sample_frequency_range));
        if (selector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR)
            return tud_control_xfer(rhport, request, &clock_valid, sizeof(clock_valid));
    }
    return false;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const *request) {
    (void)rhport; (void)request;
    streaming = false;
    return true;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *request) {
    (void)rhport;
    /* wValue contains the selected alternate setting. Alt 1 owns the IN EP. */
    streaming = TU_U16_LOW(request->wValue) == 1;
    consumed_blocks = produced_blocks;
    return true;
}
