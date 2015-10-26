# LUFA Project Makefile.

ARCH = AVR8
MCU = atmega32u4
BOARD = LEONARDO
F_CPU = 8000000
F_USB = $(F_CPU)
AVRDUDE_PROGRAMMER = avr109

TARGET       = TwiKeyboard
SRC          = Main.c HidSetup.c Descriptors.c $(LUFA_SRC_SERIAL) $(LUFA_SRC_USB) $(LUFA_SRC_USBCLASS)
OPTIMIZATION = s
CC_FLAGS     = -Wall -Wextra -Werror -DUSE_LUFA_CONFIG_HEADER

ifeq ($(LUFA_PATH),)
$(error LUFA_PATH not defined)
endif

all:

include $(LUFA_PATH)/Build/lufa_core.mk
include $(LUFA_PATH)/Build/lufa_sources.mk
include $(LUFA_PATH)/Build/lufa_build.mk
include $(LUFA_PATH)/Build/lufa_cppcheck.mk
include $(LUFA_PATH)/Build/lufa_doxygen.mk
include $(LUFA_PATH)/Build/lufa_dfu.mk
include $(LUFA_PATH)/Build/lufa_hid.mk
include $(LUFA_PATH)/Build/lufa_avrdude.mk
include $(LUFA_PATH)/Build/lufa_atprogram.mk


# Copy descriptors from LUFA sample.
Descriptors.h:
	cp $(LUFA_PATH)/../Demos/Device/ClassDriver/Keyboard/$@ $@

Descriptors.inc: Descriptors.h fix-descriptors.sed
	sed -f fix-descriptors.sed < $(LUFA_PATH)/../Demos/Device/ClassDriver/Keyboard/Descriptors.c > $@

Descriptors.o: Descriptors.inc

clean: clean-generated
clean-generated:
	$(RM) Descriptors.inc Descriptors.h
