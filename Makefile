# Kingdom Kart: Road to Glory - Wii U build (devkitPro + wut). Produces kingdomkart.wuhb
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Install devkitPro and export DEVKITPRO=/opt/devkitpro)
endif

DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
WUT_ROOT  := $(DEVKITPRO)/wut

TARGET := kingdomkart
# `make tour` builds kingdomkart-tour.rpx: the same game, but it presses its own buttons on a timer
# so the emulator test in CI can photograph every screen without a controller.
CC     := $(DEVKITPPC)/bin/powerpc-eabi-gcc

SRCS := $(wildcard src/*/*.c)
HDRS := $(wildcard src/*/*.h)

CFLAGS  := -O2 -ffast-math -fno-math-errno -Wall -Wextra -Wno-unused-parameter -std=gnu11 \
           -mcpu=750 -meabi -mhard-float -ffunction-sections -fdata-sections \
           -D__WIIU__ -D__WUT__ -I$(WUT_ROOT)/include
LDFLAGS := -specs=$(WUT_ROOT)/share/wut.specs -Wl,--gc-sections -L$(WUT_ROOT)/lib
LIBS    := -lwut -lm

all: $(TARGET).wuhb

tour: $(TARGET)-tour.rpx

$(TARGET)-tour.elf: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -DKK_TOUR $(SRCS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET)-tour.rpx: $(TARGET)-tour.elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).elf: $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) $(LIBS) -o $@

$(TARGET).rpx: $(TARGET).elf
	$(DEVKITPRO)/tools/bin/elf2rpl $< $@

$(TARGET).wuhb: $(TARGET).rpx content/hello.txt content/icon.png
	$(DEVKITPRO)/tools/bin/wuhbtool --content content --name "Kingdom Kart: Road to Glory" \
		--short-name "Kingdom Kart" --author "Sean" --icon content/icon.png $< $@

clean:
	rm -f $(TARGET).elf $(TARGET).rpx $(TARGET).wuhb $(TARGET)-tour.elf $(TARGET)-tour.rpx

.PHONY: all clean tour
