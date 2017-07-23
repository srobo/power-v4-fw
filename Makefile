FW_VER=3

PREFIX = arm-none-eabi
CC = $(PREFIX)-gcc
LD = $(PREFIX)-gcc
SIZE = $(PREFIX)-size
GDB = $(PREFIX)-gdb
OBJCOPY = $(PREFIX)-objcopy
OOCD = openocd

# Directory containing include/ and lib/ subdirectories of libopencm3 installation.
LIBOPENCM3 ?= libopencm3

LDSCRIPT = stm32-pbv4.ld
OOCD_BOARD = oocd/pbv4.cfg

# Export these facts to bootloader
SR_BOOTLOADER_VID=0x1BDA  # ECS VID
SR_BOOTLOADER_PID=0x0010  # Power board PID
SR_BOOTLOADER_REV=0x0403  # BCD version, board 4.3.
SR_BOOTLOADER_FLASHSIZE=0x8000 # 32kb of onboard flash.
export SR_BOOTLOADER_VID SR_BOOTLOADER_PID SR_BOOTLOADER_REV SR_BOOTLOADER_FLASHSIZE

CFLAGS += -mcpu=cortex-m3 -mthumb -msoft-float -DSTM32F1 \
	  -Wall -Wextra -Os -std=gnu99 -g -fno-common \
	  -I$(LIBOPENCM3)/include -DFW_VER=$(FW_VER) -g \
	  -DSR_DEV_VID=$(SR_BOOTLOADER_VID) \
	  -DSR_DEV_PID=$(SR_BOOTLOADER_PID) \
	  -DSR_DEV_REV=$(SR_BOOTLOADER_REV)
BASE_LDFLAGS += -lc -lm -L$(LIBOPENCM3)/lib \
	   -L$(LIBOPENCM3)/lib/stm32/f1 -lnosys \
	   -nostartfiles -Wl,--gc-sections,-Map=pbv4.map -mcpu=cortex-m3 \
	   -mthumb -march=armv7-m -mfix-cortex-m3-ldrd -msoft-float
LDFLAGS = $(BASE_LDFLAGS) -T$(LDSCRIPT)

BOOTLOADER_OBJS = dfu-bootloader/usb_dfu_blob.o dfu-bootloader/usbdfu.o
O_FILES = main.o led.o output.o usart.o analogue.o pbusb.o fan.o smps.o piezo.o button.o battery.o pswitch.o i2c.o clock.o $(BOOTLOADER_OBJS)
TEST_O_FILES = test.o led.o output.o fan.o smps.o piezo.o button.o battery.o usart.o pswitch.o cdcacm.o analogue.o i2c.o clock.o

all: button.o bootloader $(O_FILES) pbv4.bin pbv4_test.bin pbv4_noboot.bin

test: pbv4_test.bin

include depend

bootloader:
	FORCE_BOOTLOADER_OBJ="`pwd`/button.o `pwd`/smps.o" $(MAKE) -C dfu-bootloader

pbv4.elf: $(O_FILES) $(LDSCRIPT)
	$(LD) -o $@ $(O_FILES) $(LDFLAGS) -lopencm3_stm32f1
	$(SIZE) $@

pbv4_test.elf: $(TEST_O_FILES)
	$(LD) -o $@ $(TEST_O_FILES) $(BASE_LDFLAGS) -lopencm3_stm32f1 -Tstm32-pbv4_test.ld
	$(SIZE) $@

pbv4.bin: pbv4.elf
	$(OBJCOPY) -O binary $< $@
	dfu-bootloader/crctool -S 8192 -w $@  # Ignore the first 8192 bytes (the bootloader blob).

pbv4_test.bin: pbv4_test.elf
	$(OBJCOPY) -O binary $< $@

# Produce a no-bootloader binary, suitable for shunting straight into the app
# segment of flash, by droping the first 8k of the flat image.
pbv4_noboot.bin: pbv4.elf
	tmpfile=`mktemp /tmp/sr-pbv4-XXXXXXXX`; $(OBJCOPY) -O binary $< $$tmpfile; dd if=$$tmpfile of=$@ bs=4k skip=2; rm $$tmpfile
	dfu-bootloader/crctool -w $@

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
	$(MAKE) -C dfu-bootloader clean
