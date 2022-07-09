# Power Board v4 Firmware

The Power Board distributes power to the SR kit from the battery. It
provides six individual general-purpose power outputs along with two low
current 5V power outputs.

It also holds the internal On\|Off switch for the whole robot as well as
the Start button which is used to start your robot code running.

## Instructions

Using a posix system, you require `make`, the `arm-none-eabi` toolchain and `git`.
Before attempting to build anything initialise all the submodules.
```shell
$ git submodule update --init --recursive
```

To build the main binary, run:
```shell
$ make
```
The binary will then be at `src/main.bin`.
This will also build the library libopencm3 the first time you run it.

This can be flashed to an attached power board that has a bootloader using:
```shell
$ make -C src dfu
```

To build the bootloader, run:
```shell
$ make -C bootloader
```
The bootloader binary will then be at `bootloader/usb_dfu.bin`


## USB Interface

The Vendor ID is `1bda` (University of Southampton) and the product ID
is `0010`.

The Power Board is controlled over USB serial, each command is its own line.
The list of commands is TBC.

### udev Rule

If you are connecting the Power Board to a Linux computer with udev, the
following rule can be added in order to access the Power Board interface
without root privileges:

`SUBSYSTEM=="usb", ATTRS{idVendor}=="1bda", ATTRS{idProduct}=="0010", GROUP="plugdev", MODE="0666"`

It should be noted that `plugdev` can be changed to any Unix group of
your preference.

This should only be necessary when trying to access the bootloader.

## Designs

You can access the schematics and source code of the hardware in the following places.
-   [Full Schematics](https://www.studentrobotics.org/resources/kit/power-schematic.pdf)
-   [Competitor Facing Docs](https://www.studentrobotics.org/docs/kit/power_board)
-   [Hardware designs](https://github.com/srobo/power-v4-hw)
