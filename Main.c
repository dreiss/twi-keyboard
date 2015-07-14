#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>

#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/USB/USB.h>


#define SERIAL_SPEED (115200)


// HID device config and state.  Defined in HidSetup.c.
extern USB_ClassInfo_HID_Device_t kbd_device;


static void setup_hardware(void);


int main(void) {
  setup_hardware();

  uint8_t last_state = 0xff;

  for (;;) {
    uint8_t state = USB_DeviceState;
    if (state != last_state) {
      last_state = state;
      Serial_SendByte('S');
      Serial_SendByte(state + '0');
      Serial_SendByte('\n');
    }

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
  Delay_MS(500);
  USB_Init();
  GlobalInterruptEnable();
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

  PORTB |= _BV(PB4) | _BV(PB5) | _BV(PB6) | _BV(PB7);
  PORTC |= _BV(PC6);
  PORTD |= _BV(PD0) | _BV(PD1) | _BV(PD4) | _BV(PD6) | _BV(PD7);
  PORTE |= _BV(PE6);

#define key(port, bit, code) do { \
    if (!(CONCAT_EXPANDED(PIN, port) & \
          _BV(CONCAT_EXPANDED(CONCAT_EXPANDED(P, port), bit)))) { \
      kbd_report->KeyCode[0] = CONCAT_EXPANDED(HID_KEYBOARD_SC_, code); \
    } \
} while (0)

  key(B, 4, UP_ARROW);
  key(B, 5, DOWN_ARROW);
  key(B, 6, LEFT_ARROW);
  key(B, 7, RIGHT_ARROW);
  key(D, 6, KEYPAD_ENTER);

  key(D, 1, ESCAPE);
  key(D, 0, F12);
  key(D, 4, APPLICATION);
  key(C, 6, MEDIA_PREVIOUS_TRACK);
  key(D, 7, SPACE);
  key(E, 6, MEDIA_NEXT_TRACK);

#undef key

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
