#!/bin/sh
# Builds LiveTest.hex without Keil (arm-none-eabi-gcc). Needs the CMSIS headers
# LPC17xx.h, core_cm3.h etc. in $CMSIS. Flash LiveTest.hex with Flash Magic.
set -e
cd "$(dirname "$0")"
CMSIS=${CMSIS:-cmsis}
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Os -ffunction-sections -fdata-sections -Wall \
  -I"$CMSIS" -I.. startup.c system.c ../livetest.c ../hw.c ../features.c ../classify.c \
  -T lpc1768.ld -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections -lm -o lt.elf
arm-none-eabi-objcopy -O binary lt.elf lt.bin
# LPC17xx boot ROM needs word 7 = minus the sum of words 0..6
python3 -c "import struct;b=bytearray(open('lt.bin','rb').read());struct.pack_into('<I',b,28,(-sum(struct.unpack('<7I',b[:28])))&0xffffffff);open('lt.bin','wb').write(b)"
arm-none-eabi-objcopy -I binary -O ihex lt.bin LiveTest.hex
arm-none-eabi-size lt.elf
