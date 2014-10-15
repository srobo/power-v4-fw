FW_VER=1

PREFIX = arm-none-eabi
CC = $(PREFIX)-gcc
LD = $(PREFIX)-gcc
SIZE = $(PREFIX)-size
GDB = $(PREFIX)-gdb
OBJCOPY = $(PREFIX)-objcopy
OOCD = openocd

LDSCRIPT = stm32-pbv4.ld
OOCD_BOARD = oocd/pbv4.cfg

CFLAGS += -mcpu=cortex-m3 -mthumb -msoft-float -DSTM32F1 \
	  -Wall -Wextra -Os -std=gnu99 -g -fno-common \
	  -Ilibopencm3/include -DFW_VER=$(FW_VER)
LDFLAGS += -lc -lm -Llibopencm3/lib \
	   -Llibopencm3/lib/stm32/f1 -lnosys -T$(LDSCRIPT) \
	   -nostartfiles -Wl,--gc-sections,-Map=pbv4.map -mcpu=cortex-m3 \
	   -mthumb -march=armv7-m -mfix-cortex-m3-ldrd -msoft-float

O_FILES = dfu-bootloader/usbdfu.o main.o led.o output.o usart.o analogue.o usb.o fan.o smps.o piezo.o button.o battery.o pswitch.o i2c.o
TEST_O_FILES = test.o led.o output.o fan.o smps.o piezo.o button.o battery.o usart.o pswitch.o cdcacm.o analogue.o i2c.o

all: bootloader pbv4.bin pbv4_test.bin

test: pbv4_test.bin

include depend

bootloader:
	$(MAKE) -C dfu-bootloader

pbv4.elf: $(O_FILES) $(LD_SCRIPT)
	$(LD) -o $@ $(O_FILES) $(LDFLAGS) -lopencm3_stm32f1
	$(SIZE) $@

pbv4_test.elf: $(TEST_O_FILES) $(LD_SCRIPT)
	$(LD) -o $@ $(TEST_O_FILES) $(LDFLAGS) -lopencm3_stm32f1
	$(SIZE) $@

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

depend: *.c
	rm -f depend
	for file in $^; do \
		$(CC) $(CFLAGS) -MM $$file -o - >> $@ ; \
	done ;

.PHONY: all test clean flash bootloader

flash: pbv4.elf
	$(OOCD) -f "$(OOCD_BOARD)" \
	        -c "init" \
	        -c "reset init" \
	        -c "stm32f1x mass_erase 0" \
	        -c "flash write_image $<" \
	        -c "reset" \
	        -c "shutdown"

flash_test: pbv4_test.elf
	$(OOCD) -f "$(OOCD_BOARD)" \
	        -c "init" \
	        -c "reset init" \
	        -c "stm32f1x mass_erase 0" \
	        -c "flash write_image $<" \
	        -c "reset" \
	        -c "shutdown"

flashUart: pbv4_test.bin
	 ~/src/stm32flash/stm32flash -b 115200 -w pbv4_test.bin /dev/ttyUSB1

debug: pbv4_test.elf
	$(OOCD) -f "$(OOCD_BOARD)" \
	        -c "init" \
	        -c "reset halt" &
	$(GDB)  $^ -ex "target remote localhost:3333" -ex "mon reset halt" && killall openocd

clean:
	-rm -f pbv4.elf depend *.o
