#include <stdbool.h>
#include <stdlib.h>

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <util/twi.h>

#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/USB/USB.h>


#define SERIAL_SPEED (115200)


static const uint8_t DEFAULT_TWI_ADDRESS  = 'k';
static uint8_t eep_address_space[1<<5] EEMEM;

// Convert ms to Timer/Counter1 ticks.  1024 factor is from prescaler.
#define MS_TO_TICKS(ms) (ms * (F_CPU / 1000) / 1024)
static const uint16_t KEY_RELEASE_TIME_TICKS = MS_TO_TICKS(50);
static const uint16_t NORMAL_KEYPRESS_TIME_TICKS = MS_TO_TICKS(50);
static const uint16_t LONG_KEYPRESS_TIME_TICKS = MS_TO_TICKS(2000);
#undef MS_TO_TICKS

enum {
  // Idle = waiting for command over I2C.
  STATE_IDLE = 0,
  // Currently pressing a key.  I2C input ignored.
  STATE_PRESSING = 1,
  // Waiting with keys released for a short time.  I2C input ignored.
  STATE_RELEASING = 2,
};

// We could disable I2C ACKs during STATE_PRESSING and STATE_RELEASING,
// but I don't think it's important.

// Current execution state.
static uint8_t g_state;
// Command received over I2C.  Only set by I2C ISR, and only cleared
// in drive_logic.
static uint8_t twi_cmd;
// Key code currently being pressed.
static uint8_t pressed_code;
// Timestamp in Timer/Counter1 at which we should transition states.
static uint16_t transition_time;
// Track progress of "blink LEDs" command.
static uint16_t leds_iter;

// HID device config and state.  Defined in HidSetup.c.
extern USB_ClassInfo_HID_Device_t kbd_device;


// Forward declarations.
static void setup_hardware(void);
static uint8_t determine_twi_address(void);
static void check_usb_device_state(void);
static void drive_leds(void);
static void drive_logic(void);
static bool process_cmd(void);


int main(void) {
  setup_hardware();

  for (;;) {
    check_usb_device_state();
    drive_leds();
    drive_logic();
    HID_Device_USBTask(&kbd_device);
    USB_USBTask();
  }
}

static void setup_hardware(void) {
  MCUSR = 0;
  wdt_disable();
  clock_prescale_set(clock_div_1);

  LEDs_Init();

  Serial_Init(SERIAL_SPEED, /* double speed */ false);

  // Force re-enumeration like the Trinket HidCombo demo.
  // Without the sleep, the computer still thinks the bootloader is attached.
  // Blink the LEDs during the sleep so we know the device is ready.
  LEDs_SetAllLEDs(LEDS_ALL_LEDS);
  Delay_MS(200);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);
  Delay_MS(100);
  LEDs_SetAllLEDs(LEDS_ALL_LEDS);
  Delay_MS(200);
  LEDs_SetAllLEDs(LEDS_NO_LEDS);

  // Set up Timer1 to run at CLK/1024, or 1 tick per 128 us at 8 MHz.
  TCCR1A = 0;
  TCCR1B = _BV(CS12) | _BV(CS10);

  // Make sure the TWI pins are set as inputs with pull-ups off.
  // This should be the default, but there's no harm in re-setting them.
  // Of course, if I were really afraid of damaging a Raspberry Pi,
  // I'd probably do this in the bootloader, or at least before
  // the 500 ms sleep.
  DDRD &= ~_BV(PD0);
  DDRD &= ~_BV(PD1);
  PORTD &= ~_BV(PD0);
  PORTD &= ~_BV(PD1);
  // Set up TWI as a slave with ACKs and interrupts enabled,
  // but ignoring General Calls.
  uint8_t twi_address = determine_twi_address();
  TWAR = (twi_address << 1);
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);

  USB_Init();
  GlobalInterruptEnable();
}

static void reset_to_bootloader(void) {
  // Stop all interrupts.  We want total control for the reboot procedure.
  cli();
  // The Leonardo bootloader uses this as a flag to indicate that it should
  // not jump directly into user code.
  // The proper thing to do would be to use linker tricks to allocate
  // a global variable at this address, but since we've stopped
  // all other functions, this won't do any harm.
  (*(uint16_t*)0x0800) = 0x7777;
  // Enable the watchdog with the minimum timeout and wait for reset.
  wdt_enable(WDTO_15MS);
  for (;;);
}

static uint8_t determine_twi_address(void) {
  // The eep_address_space array occupies 2N bytes, which make up N "cells".
  // Each cell consists of an "indicator" byte at offset k,
  // and a "value" byte at offset k+N.
  // Our I2C address is the "value" of the last cell
  // whose "indicator" is neither 0 nor 0xff.
  // This approach gives us a 16x increase in write durability.
  // We can start out with indicator 0 = 0x33 (or whatever)
  // and write thousands of addresses to value 0.
  // When the value 0 cell wears out,
  // we just set indicator 1 to 0x33
  // and start writing our address to value 1.
  const uint8_t num_cells = sizeof(eep_address_space) >> 1;

  uint8_t address_offset = 0xff;

  // Find the last active indicator.
  // We don't break out early, so the runtime of this loop
  // is mostly constant, regardless of which cell we are using.
  for (uint8_t addr = 0; addr < num_cells; ++addr) {
    uint8_t flag = eeprom_read_byte(&eep_address_space[addr]);
    if (flag == 0 || flag == 0xff) {
      continue;
    }
    address_offset = addr;
  }

  // If we found an active indicator, read its value.
  // (This does create startup variance between
  // default and non-defaul addresses.)
  if (address_offset != 0xff) {
    return eeprom_read_byte(&eep_address_space[address_offset + num_cells]);
  }
  return DEFAULT_TWI_ADDRESS;
}

static void check_usb_device_state(void) {
  // Dump USB connection state changes to the serial connection.
  static uint8_t last_state = 0xff;
  uint8_t state = USB_DeviceState;
  if (state != last_state) {
    last_state = state;
    Serial_SendByte('S');
    Serial_SendByte(state + '0');
    Serial_SendByte('\n');
  }
}

// Driver for the blink LEDs command.
// Rather than depending on a hardware timer,
// just use the number of iterations of our main loop as a coarse timer.
// Setting leds_iter to 1 will start the blinks.
static void drive_leds(void) {
  if (leds_iter == 0) {
    return;
  }
  switch (leds_iter++) {
    case 1:
      LEDs_SetAllLEDs(LEDS_LED1);
      break;
    case 15000:
      LEDs_SetAllLEDs(LEDS_LED2);
      break;
    case 22000:
      LEDs_SetAllLEDs(LEDS_LED1);
      break;
    case 29000:
      LEDs_SetAllLEDs(LEDS_LED2);
      break;
    case 44000:
      LEDs_SetAllLEDs(LEDS_NO_LEDS);
      leds_iter = 0;
      break;
  }
}

static inline void reset_timer(void) {
  TCNT1 = 0;
  TIFR1 = _BV(TOV1);
}

static inline bool timer_expired(void) {
  return (TCNT1 > transition_time) || (TIFR1 & _BV(TOV1));
}

static void drive_logic(void) {
  switch (g_state) {
    case STATE_IDLE:
      if (process_cmd()) {
        g_state = STATE_PRESSING;
        reset_timer();
        Serial_SendByte('a' - 1 + twi_cmd);
      }
      break;
    case STATE_PRESSING:
      if (timer_expired()) {
        g_state = STATE_RELEASING;
        pressed_code = 0;
        reset_timer();
        transition_time = KEY_RELEASE_TIME_TICKS;
      }
      break;
    case STATE_RELEASING:
      if (timer_expired()) {
        g_state = STATE_IDLE;
        twi_cmd = 0;
      }
      break;
  }
}

// Examine twi_cmd and handle it.
// Returns true if we received a valid keypress command.
// - If 0, no command has been sent.  Return false.
// - If a valid key command, update pressed_code and transition_time,
//   and return true.
// - Else, clear the invalid (or non-key) command and return false.
static bool process_cmd(void) {
  switch (twi_cmd) {
    case 0:
      return false;

    case 0x01:
      // Enable the LED blink, but then break so this doesn't get treated
      // as a valid command.
      leds_iter = 1;
      break;

    case 0x02:
      reset_to_bootloader();
      // Unreachable.
      break;

#define key(cmd, len, code) \
  case cmd: \
    pressed_code = CONCAT_EXPANDED(HID_KEYBOARD_SC_, code); \
    transition_time = CONCAT_EXPANDED(len, _KEYPRESS_TIME_TICKS); \
    return true

    key(0x03, NORMAL, UP_ARROW);
    key(0x04, NORMAL, DOWN_ARROW);
    key(0x05, NORMAL, LEFT_ARROW);
    key(0x06, NORMAL, RIGHT_ARROW);
    key(0x07, NORMAL, KEYPAD_ENTER);

    key(0x08, NORMAL, ESCAPE);
    key(0x09, NORMAL, F12);
    key(0x0a, NORMAL, APPLICATION);
    key(0x0b, NORMAL, MEDIA_PREVIOUS_TRACK);
    key(0x0c, NORMAL, SPACE);
    key(0x0d, NORMAL, MEDIA_NEXT_TRACK);

    key(0x0e, LONG,   MEDIA_PREVIOUS_TRACK);
    key(0x0f, LONG,   MEDIA_NEXT_TRACK);

#undef key
  }

  twi_cmd = 0;
  return false;
}

ISR(TWI_vect) {
  switch (TW_STATUS) {
    // We have been addressed as the slave in slave-receive (write) mode,
    // and an ACK has been sent.  Continue the transaction.
    // (Note, we don't expect ARB_LOST or GCALL, but whatever.)
    case TW_SR_SLA_ACK:
    case TW_SR_ARB_LOST_SLA_ACK:
    case TW_SR_GCALL_ACK:
    case TW_SR_ARB_LOST_GCALL_ACK:
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
      break;
    // We have received the first byte.  Store it in twi_cmd if it is empty.
    // Clear TWEA to ensure we NACK any further written bytes.
    case TW_SR_DATA_ACK:
    case TW_SR_GCALL_DATA_ACK:
      if (twi_cmd == 0) {
        twi_cmd = TWDR;
      }
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
      break;
    // The master sent us more bytes.  We sent a NACK and
    // the transceiver will automatically switch to "not addressed".
    // Turn TWEA back on so we can ACK the next transmission.
    // (The transceiver won't ACK anything until the next start condition.)
    case TW_SR_DATA_NACK:
    case TW_SR_GCALL_DATA_NACK:
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
      break;
    // We got a stop condition.  Return to our base state.
    case TW_SR_STOP:
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
      break;

    // We have been addressed as the slave in slave-transmit (read) mode,
    // and an ACK has been sent (which is not really ideal).
    // We have nothing to say, so just send a ~0 as the "last data byte".
    // This is (kind of) consistent with a device that does not respond.
    case TW_ST_SLA_ACK:
    case TW_ST_ARB_LOST_SLA_ACK:
    case TW_ST_DATA_ACK:
      TWDR = ~0;
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
      break;
    // We got a NACK on data (or "last data", I think) or
    // an ACK on "last data" (master keeps reading).
    // The transceiver will automatically switch to "not addressed".
    // Turn TWEA back on so we can ACK the next transmission.
    // (The transceiver won't ACK anything until the next start condition.)
    case TW_ST_DATA_NACK:
    case TW_ST_LAST_DATA:
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
      break;

    // In the case of a bus error, we need to set TWSTO to reset everything.
    case TW_BUS_ERROR:
      TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTO);
      break;

    // This should not happen because we never enter master mode.
    default:
      Serial_SendString_P(PSTR("TWms"));
      Serial_SendByte(TW_STATUS);
      abort();
  }
}

bool CALLBACK_HID_Device_CreateHIDReport(
    USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
    uint8_t* const ReportID,
    const uint8_t ReportType,
    void* ReportData,
    uint16_t* const ReportSize) {
  (void)HIDInterfaceInfo;
  (void)ReportID;
  (void)ReportType;

  USB_KeyboardReport_Data_t* kbd_report = (USB_KeyboardReport_Data_t*)ReportData;

  kbd_report->KeyCode[0] = pressed_code;

  *ReportSize = sizeof(USB_KeyboardReport_Data_t);
  return false;
}

void CALLBACK_HID_Device_ProcessHIDReport(
    USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
    const uint8_t ReportID,
    const uint8_t ReportType,
    const void* ReportData,
    const uint16_t ReportSize) {
  (void)HIDInterfaceInfo;
  (void)ReportID;
  (void)ReportType;

  (void)ReportData;
  (void)ReportSize;
}
