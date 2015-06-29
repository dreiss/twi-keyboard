# LUFA Project Makefile.

ARCH = AVR8
MCU = atmega32u4
BOARD = LEONARDO
F_CPU = 16000000
F_USB = $(F_CPU)
AVRDUDE_PROGRAMMER = avr109

TARGET       = TwiKeyboard
SRC          = Main.c
OPTIMIZATION = s
CC_FLAGS     = -Wall -Wextra -Werror

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
