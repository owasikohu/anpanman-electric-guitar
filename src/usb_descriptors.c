/* Based on TinyUSB's examples/device/audio_test descriptor (MIT licensed). */
#include <string.h>
#include "pico/unique_id.h"
#include "tusb.h"

#define USB_VID 0xCafe
#define USB_PID 0x4010

static tusb_desc_device_t const device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&device_descriptor;
}

enum { ITF_AUDIO_CONTROL, ITF_AUDIO_STREAMING, ITF_COUNT };
#if TUSB_VERSION_NUMBER >= 2100
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO20_MIC_ONE_CH_DESC_LEN)
#define PICO_MIC_DESCRIPTOR TUD_AUDIO20_MIC_ONE_CH_DESCRIPTOR
#else
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_AUDIO_MIC_ONE_CH_DESC_LEN)
#define PICO_MIC_DESCRIPTOR TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR
#endif

static uint8_t const configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, CONFIG_TOTAL_LEN, 0, 100),
    PICO_MIC_DESCRIPTOR(ITF_AUDIO_CONTROL, 0,
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, 16,
        0x81, CFG_TUD_AUDIO_EP_SZ_IN),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return configuration_descriptor;
}

static char const *const strings[] = {
    (char const[]){0x09, 0x04},
    "Raspberry Pi",
    "Pico ADC Microphone",
};
static uint16_t string_descriptor[33];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t count = 0;
    if (index == 0) {
        memcpy(&string_descriptor[1], strings[0], 2);
        count = 1;
    } else if (index == 3) {
        char serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
        pico_get_unique_board_id_string(serial, sizeof(serial));
        count = strlen(serial);
        for (size_t i = 0; i < count; ++i) string_descriptor[1 + i] = (uint8_t)serial[i];
    } else {
        if (index >= sizeof(strings) / sizeof(strings[0])) return NULL;
        count = strlen(strings[index]);
        if (count > 32) count = 32;
        for (size_t i = 0; i < count; ++i) string_descriptor[1 + i] = (uint8_t)strings[index][i];
    }
    string_descriptor[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
    return string_descriptor;
}
