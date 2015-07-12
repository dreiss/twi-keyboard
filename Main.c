#include <avr/wdt.h>
#include <avr/power.h>

#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>
#include <LUFA/Drivers/USB/USB.h>


#define SERIAL_SPEED (115200)


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

    USB_USBTask();
  }
}

static void setup_hardware(void) {
  MCUSR = 0;
  wdt_disable();
  clock_prescale_set(clock_div_1);

  LEDs_Init();

  Serial_Init(SERIAL_SPEED, /* double speed */ false);

  // TODO: Maybe force re-enumeration like the Trinket HidCombo demo?
  //Delay_MS(500);
  USB_Init();
  GlobalInterruptEnable();
}
