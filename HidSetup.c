#include <stdlib.h>

#include <LUFA/Drivers/USB/USB.h>

#include "Descriptors.h"

// TODO: Consider dropping this and tracking report changes manually.
static uint8_t previous_report[sizeof(USB_KeyboardReport_Data_t)];

USB_ClassInfo_HID_Device_t kbd_device = {
  .Config = {
    .InterfaceNumber = INTERFACE_ID_Keyboard,
    .ReportINEndpoint = {
      .Address = KEYBOARD_EPADDR,
      .Size = KEYBOARD_EPSIZE,
      .Banks = 1,
    },
    .PrevReportINBuffer = previous_report,
    .PrevReportINBufferSize = sizeof(previous_report),
  },
};

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void) {
  bool success = HID_Device_ConfigureEndpoints(&kbd_device);
  // TODO: Possibly signal this failure better.
  // I don't think it can happen in practice.
  if (!success) {
    abort();
  }

  // Need this to call HID_Device_MillisecondElapsed.
  USB_Device_EnableSOFEvents();
}

void EVENT_USB_Device_ControlRequest(void) {
  HID_Device_ProcessControlRequest(&kbd_device);
}

void EVENT_USB_Device_StartOfFrame(void) {
  HID_Device_MillisecondElapsed(&kbd_device);
}
