#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// Port 0: Native USB for device mode
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

// Port 1: PIO USB for host mode  
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define BOARD_TUH_RHPORT 1

// Enable PIO USB for host
#define CFG_TUH_RPI_PIO_USB 1

//--------------------------------------------------------------------+
// Common Configuration
//--------------------------------------------------------------------+

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

//--------------------------------------------------------------------+
// Device Configuration
//--------------------------------------------------------------------+

#define CFG_TUD_ENDPOINT0_SIZE 64

// Device classes
#define CFG_TUD_HID 2
#define CFG_TUD_CDC 0
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_HID_EP_BUFSIZE 64

//--------------------------------------------------------------------+
// Host Configuration
//--------------------------------------------------------------------+

#define CFG_TUH_ENUMERATION_BUFSIZE 512

#define CFG_TUH_HUB 1
#define CFG_TUH_DEVICE_MAX 16

// Host classes
#define CFG_TUH_HID 16
#define CFG_TUH_CDC 0
#define CFG_TUH_MSC 0
#define CFG_TUH_VENDOR 0

#define CFG_TUH_HID_EPIN_BUFSIZE 64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#endif