#!/bin/sed -f
# Modify lufa/Demos/Device/ClassDriver/Keyboard/Descriptors.c
# to have the right values for this project.

# Avoid an unused parameter warning.
/DescriptorType *=/i\
	(void)wIndex;\

#

# This VID/PID is provided by the V-USB library for shared use.
# We use the default OS driver, so there are no extra requirements.
/VendorID *=/s/0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]/0x16c0/
/ProductID *=/s/0x[0-9A-F][0-9A-F][0-9A-F][0-9A-F]/0x27db/
# Update manufacturer and product name.
/ManufacturerString = USB_STRING_DESCRIPTOR(/s/"[^"]*"/"David Reiss"/
/ProductString = USB_STRING_DESCRIPTOR(/s/"[^"]*"/"TwiKeyboard"/
# Unlike the keyboard demo, this device is not self-powered.
s/USB_CONFIG_ATTR_SELFPOWERED/0/
