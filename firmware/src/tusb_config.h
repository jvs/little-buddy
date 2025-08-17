#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device and host
// On RP2040: 0=native USB, 1=PIO USB
#define BOARD_TUD_RHPORT      0   // Native USB for device
#define BOARD_TUH_RHPORT      1   // PIO USB for host

#define BOARD_TUD_MAX_SPEED   OPT_MODE_FULL_SPEED
#define BOARD_TUH_MAX_SPEED   OPT_MODE_FULL_SPEED

//--------------------------------------------------------------------+
// COMMON CONFIGURATION
//--------------------------------------------------------------------+

#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE       OPT_MODE_HOST

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS                 OPT_OS_NONE
#endif

#define CFG_TUSB_DEBUG              0

//--------------------------------------------------------------------+
// DEVICE CONFIGURATION
//--------------------------------------------------------------------+

#ifndef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED             1
#endif

#define CFG_TUD_MAX_SPEED           BOARD_TUD_MAX_SPEED
#define CFG_TUD_ENDPOINT0_SIZE      64

//------------- CLASS -------------//
#define CFG_TUD_CDC                 1
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE      64
#define CFG_TUD_CDC_TX_BUFSIZE      64

//--------------------------------------------------------------------+
// HOST CONFIGURATION  
//--------------------------------------------------------------------+

#ifndef CFG_TUH_ENABLED
#define CFG_TUH_ENABLED             1
#endif

#define CFG_TUH_MAX_SPEED           BOARD_TUH_MAX_SPEED
#define CFG_TUH_ENDPOINT0_SIZE      64

//------------- CLASS -------------//
#define CFG_TUH_HUB                 1
#define CFG_TUH_CDC                 1
#define CFG_TUH_HID                 1
#define CFG_TUH_MSC                 0
#define CFG_TUH_VENDOR              0

#define CFG_TUH_DEVICE_MAX          4
#define CFG_TUH_HUB_PORT_MAX        4
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// HID Host
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64

//--------------------------------------------------------------------+
// For board without native USB peripheral, enable PIO USB
//--------------------------------------------------------------------+
#if BOARD_TUH_RHPORT == 1
#define CFG_TUH_RPI_PIO_USB         1
#define BOARD_TUH_PIO_USB_DP_PIN    PICO_DEFAULT_PIO_USB_DP_PIN
#endif

#endif