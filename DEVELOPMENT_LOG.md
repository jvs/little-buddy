# Little Buddy USB Bridge Development Log

## Project Goal
Build a dual-mode USB bridge on RP2040 (Adafruit Feather RP2040 with USB Type A Host) that:
1. Receives input from a trackpoint keyboard (host mode)
2. Applies custom processing logic
3. Outputs modified events as keyboard/mouse to computer (device mode)

## Current Status ✅
- **Dual-mode USB working**: Device + Host modes running simultaneously
- **Device mode**: Presents as CDC + HID Keyboard + HID Mouse to computer
- **Host mode**: Detects and receives reports from trackpoint keyboard
- **Trackpoint parsing**: Implemented Linux kernel approach for Lenovo trackpoint keyboards
- **Display dashboard**: Shows device counts and last events
- **CDC command interface**: For debugging and control

## Hardware Setup
- **Board**: Adafruit Feather RP2040 with USB Type A Host
- **Display**: SH1107 OLED (128x128) via I2C
- **Test device**: Lenovo trackpoint keyboard
- **USB routing**: 
  - Native USB (port 0) → Device mode → Computer
  - PIO USB (port 1, pins 16/18) → Host mode → Trackpoint keyboard

## Key Discoveries

### USB Architecture
- **Native USB**: Device mode (CDC + HID Keyboard + HID Mouse)
- **PIO USB**: Host mode (can connect keyboards, mice, etc.)
- **TinyUSB**: Handles both stacks simultaneously
- **Board header**: `feather_host.h` defines PIO USB pins

### Trackpoint Keyboard Behavior
- **Single HID interface**: Not separate keyboard + mouse interfaces
- **Embedded mouse data**: Trackpoint data hidden in 8-byte keyboard reports
- **Detection**: `report[0] == 0x01` indicates trackpoint data
- **Format**: `[0x01, buttons, x_low, y_low, x_high, y_high, ?, ?]`
- **Coordinates**: 12-bit values reconstructed from multiple bytes

### Debug Architecture
- **Display**: Primary status dashboard (device counts, last events)
- **CDC**: Command interface + data dumps (not debug spam)
- **Commands**: `H` (help), `D` (toggle raw dumps), `S` (show devices)

## Code Structure

### Main Components
- `main.c`: Core dual-mode USB implementation
- `tusb_config.h`: TinyUSB configuration for dual mode
- `feather_host.h`: Board-specific pin definitions
- `sh1107_display.c/h`: OLED display driver

### Key Functions
- `tuh_hid_mount_cb()`: Host device detection with heuristic parsing
- `tuh_hid_report_received_cb()`: Linux kernel-based trackpoint parsing
- `send_keyboard_report()` / `send_mouse_report()`: Device mode output
- CDC command processing for debugging

### Build System
- **GitHub Actions**: Automated builds with ARM GCC toolchain
- **Local Docker**: `build-docker.sh` replicates CI environment
- **Output**: `little_buddy.uf2` for flashing

## Next Steps

### Immediate (Current Session)
- [x] Test trackpoint mouse detection with latest build
- [x] Verify `Track: btn=X x=Y y=Z` appears on display
- [ ] Add USB VID/PID detection for Lenovo devices

### Near Term
- [ ] Implement the "complicated logic" processing pipeline
- [ ] Bridge trackpoint events to device mode outputs  
- [ ] Add configuration interface via CDC commands
- [ ] Handle edge cases (device disconnect, multiple devices)

### Future Enhancements
- [ ] Support other trackpoint keyboard models
- [ ] Cute face animations on display
- [ ] More sophisticated HID descriptor parsing
- [ ] Multiple device support
- [ ] Configuration persistence

## Technical Notes

### Trackpoint Detection Logic
Currently using heuristic: any "OTHER" protocol device assumed to support both keyboard and mouse. Should enhance to:

```c
// Check USB VID/PID for known Lenovo devices
#define USB_VENDOR_ID_LENOVO     0x17ef
#define USB_DEVICE_ID_LENOVO_TPIIUSBKBD  0x60ee
#define USB_DEVICE_ID_LENOVO_TPIIBTKBD   0x60e1
```

### Display Update Strategy
- **500ms refresh rate**: Prevents flicker, good responsiveness
- **Event-driven**: Only shows meaningful state changes
- **Three lines**: Device counts, last event, CDC activity

### CDC Command Interface
Clean separation between:
- **Status queries**: Device info, help
- **Debug tools**: Raw report dumps, test commands  
- **Control**: Mode changes, configuration

## Lessons Learned

### Design Philosophy
- **Display first**: Visual feedback more reliable than CDC debugging
- **Pragmatic parsing**: Simple heuristics > complex descriptor parsing
- **Follow Linux**: Kernel drivers are battle-tested references

### Development Flow
- **GitHub Actions**: Reliable, consistent builds
- **Local Docker**: Fast iteration when needed
- **Display debugging**: Much better than CDC log spam

### USB Gotchas
- **Power-on timing**: Different behavior between flash vs cold boot
- **CDC buffer limits**: Can cause corruption with debug spam
- **TinyUSB task scheduling**: Must call `tud_task()` and `tuh_task()` regularly

## References
- [Linux hid-lenovo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-lenovo.c)
- [HID Remapper descriptor parser](https://github.com/jfedor2/hid-remapper/blob/master/firmware/src/descriptor_parser.cc)
- [TinyUSB documentation](https://docs.tinyusb.org/)
- [Pico SDK documentation](https://raspberrypi.github.io/pico-sdk-doxygen/)
