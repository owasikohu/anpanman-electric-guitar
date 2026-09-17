#ifndef PICO_USB_MIC_TUSB_CONFIG_H
#define PICO_USB_MIC_TUSB_CONFIG_H

#ifndef CFG_TUSB_MCU
#error CFG_TUSB_MCU must be defined
#endif

#ifndef BOARD_TUD_RHPORT
#define BOARD_TUD_RHPORT 0
#endif
#ifndef BOARD_TUD_MAX_SPEED
#define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

#define CFG_TUD_ENABLED                         1
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                             OPT_OS_NONE
#endif
#define CFG_TUSB_DEBUG                          0
#define CFG_TUD_MAX_SPEED                      BOARD_TUD_MAX_SPEED
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN                     __attribute__((aligned(4)))
#endif
#define CFG_TUD_ENDPOINT0_SIZE                  64

#define CFG_TUD_AUDIO                           1
#define CFG_TUD_CDC                             0
#define CFG_TUD_MSC                             0
#define CFG_TUD_HID                             0
#define CFG_TUD_MIDI                            0
#define CFG_TUD_VENDOR                          0

#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE        48000
#if TUSB_VERSION_NUMBER < 2100
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN           TUD_AUDIO_MIC_ONE_CH_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT           1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ        64
#endif
#define CFG_TUD_AUDIO_ENABLE_EP_IN              1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 2
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX      1
#if TUSB_VERSION_NUMBER >= 2100
#define CFG_TUD_AUDIO_EP_SZ_IN                  TUD_AUDIO_EP_SIZE(TUD_OPT_HIGH_SPEED, 48000, 2, 1)
#else
#define CFG_TUD_AUDIO_EP_SZ_IN                  TUD_AUDIO_EP_SIZE(48000, 2, 1)
#endif
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX       CFG_TUD_AUDIO_EP_SZ_IN
/* Match TinyUSB's official one-channel microphone example at full speed. */
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ    ((TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_EP_SZ_IN)

#endif
