#ifndef LUFA_CONFIG_H_
#define LUFA_CONFIG_H_

#if (ARCH != ARCH_AVR8)
  #error "Unsupported architecture for this LUFA configuration file."
#endif

// -- Active options

// TwiKeyboard is only a device.
#define USB_DEVICE_ONLY
// Always use full speed, and let LUFA handle whatever it can.
#define USE_STATIC_OPTIONS ( 0 \
    | USB_DEVICE_OPT_FULLSPEED \
    | USB_OPT_REG_ENABLED \
    | USB_OPT_AUTO_PLL\
    )
// Store descriptors in flash to save RAM.
#define USE_FLASH_DESCRIPTORS
// Use GPIOR2 as the device state enum to lower code size.
#define DEVICE_STATE_AS_GPIOR 2
// Device is always bus powered.
#define NO_DEVICE_SELF_POWER
// Descriptors from the Keyboard demo depend on this.
#define FIXED_CONTROL_ENDPOINT_SIZE 8
#define FIXED_NUM_CONFIGURATIONS 1


// -- Options to possibly use in the future

// Maybe let LUFA handle servicing USB?
//#define INTERRUPT_CONTROL_ENDPOINT

// ATmega32U4 has a VBUS interrupt, so maybe I don't need this?
//#define NO_LIMITED_CONTROLLER_CONNECT

// These features might not be necessary.
//#define NO_DEVICE_REMOTE_WAKEUP
//#define NO_SOF_EVENTS

// Keyboard demo descriptor doesn't use this, so maybe drop the code?
//#define NO_INTERNAL_SERIAL

#endif
