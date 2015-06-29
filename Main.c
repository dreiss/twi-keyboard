#include <avr/wdt.h>
#include <LUFA/Drivers/Board/LEDs.h>
#include <LUFA/Drivers/Peripheral/Serial.h>


#define SERIAL_SPEED (115200)


static void setup_hardware(void);


int main(void) {
  setup_hardware();

  for (;;) {
    LEDs_SetAllLEDs(LEDS_LED1);
    Delay_MS(250);
    LEDs_SetAllLEDs(LEDS_LED2);
    Delay_MS(250);
    LEDs_SetAllLEDs(LEDS_LED3);
    Delay_MS(500);

    Serial_SendString_P(PSTR("Hello, Serial!\n"));
  }
}

static void setup_hardware(void) {
  MCUSR = 0;
  wdt_disable();

  LEDs_Init();

  Serial_Init(SERIAL_SPEED, /* double speed */ false);
}
