#include "usb_device.h"
#include "tusb.h"
#include <stdio.h>
#include <stdarg.h>

static char printf_buffer[256];

// Custom printf that outputs to USB CDC
int debug_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(printf_buffer, sizeof(printf_buffer), format, args);
    va_end(args);
    
    if (tud_cdc_connected()) {
        tud_cdc_write(printf_buffer, len);
        tud_cdc_write_flush();
    }
    
    return len;
}

void usb_device_init(void) {
    tud_init(BOARD_TUD_RHPORT);
}

void usb_device_task(void) {
    tud_task();
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
    // Device is mounted and ready
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    // Device is unmounted
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void) remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    (void) itf;
    (void) rts;

    // DTR = false is counted as disconnected
    if ( !dtr )
    {
        // Terminal disconnected
    }
}

// Invoked when CDC interface received data from host
void tud_cdc_rx_cb(uint8_t itf)
{
    (void) itf;
    // Data received from host - we don't need to handle this for debug output
}