# Power Board v4 Firmware

The Power Board distributes power to the SR kit from the battery. It
provides six individual general-purpose power outputs along with two low
current 5V power outputs.

It also holds the internal On\|Off switch for the whole robot as well as
the Start button which is used to start your robot code running.

## Instructions

Using a POSIX system, you require `make`, the `arm-none-eabi` toolchain, `git` and `python3` version 3.7+.
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
To use the `dfu` command you need to install dfu-utils. This is a cross-platform utility.

To build the bootloader, run:
```shell
$ make -C bootloader
```
The bootloader binary will then be at `bootloader/usb_dfu.bin`


## USB Interface

The Vendor ID is `1bda` (University of Southampton) and the product ID
is `0010`.

The Power Board is controlled over USB serial, each command is its own line.

Since the serial port is virtual, the baudrate is unused and can be set to any value.

### Serial Commands

Action | Description | Command | Parameter Description | Return | Return Parameters
--- | --- | --- | --- | --- | ---
Identify | Get the board type and version | *IDN? | - | Student Robotics:PBv4B:\<asset tag>:\<software version> | \<asset tag> <br>\<software version>
Status | Get board status | *STATUS? | - | \<port overcurrents>:\<temp>:\<fan>:\<reg voltage> | \<port overcurrents> - comma seperated list of 1/0s indicating if a port has reached overcurrent e.g. 1,0,0,0,0,0,0 -> \<H0>,\<H1>,\<L0>,\<L1>,\<L2>,\<L3>,\<Reg> -> H0 has overcurrent<br>\<temp> - board temperature in degrees celcius<br>\<fan> - fan is running, int, 0-1<br>\<reg voltage> - Voltage of 5 Volt regulator in mV
Reset | Reset board to safe startup state<br>- Turn off all outputs<br>- Reset the lights, turn off buzzer | *RESET | - | ACK | -
Start button | Detect if the internal and external start button has been pressed since this command was last invoked | BTN:START:GET? | - | \<int start pressed>:\<ext start pressed> | \<pressed> - button pressed, int, 0-1
enable/disable output | Turn a power board output on or off | OUT:\<n>:SET:\<state> | \<n> port number, int,  0-6<br>\<state> int, 0-1 | ACK | - |
output on/off state | Get the on/off state for a power board output | OUT:\<n>:GET? | \<n> port number, int, 0-6 | \<state> | \<state> - output state, int, 0-1
read output current | Read the output current for a single output | OUT:\<n>:I? | \<n> port number, int, 0-6 | \<current> | \<current> - current, int, measured in mA
read battery voltage | Read the battery voltage | BATT:V? | - | \<voltage> | \<voltage> - battery voltage, measured in mV
read battery current | Read the global current draw | BATT:I? | - | \<current> | \<current> - current, int, measured in mA
Run LED | Set Run LED output | LED:RUN:SET:\<value> | \<value> LED value, enum, 0,1,F (flash) | ACK | -
Error LED | Set Error LED output | LED:ERR:SET:\<value> | \<value> LED value, int, 0,1,F (flash) | ACK | -
Get run LED state | Get current Run LED output state | LED:RUN:GET? | - | \<value> | \<value> - LED value, enum, 0,1,F (flash)
Get error LED state | Get current Error LED output state | LED:ERR:GET? | - | \<value> | \<value> - LED value, enum, 0,1,F (flash)
play note | Play note on the power board buzzer<br>Overwrites previous note | NOTE:\<note>:\<dur> | \<note> what note to play, int, 8-10,000Hz<br>\<dur> duration to play, int32, >0ms | ACK | -
get current note |  | NOTE:GET? | - | \<freq>:\<remaining> | \<freq> - current tome frequency in Hz, int<br>\<remaining> - remaining tone duration in ms, int32 <br> Note: Both values are 0 if the buzzer is not running
Force fan on | Override temperature control and runt the fan continually | *SYS:FAN:SET:\<value> | \<value> Enable/disable fan control override | ACK | - |
enable/disable brain output | Turn the brain output on or off | *SYS:BRAIN:SET:\<state> | \<state> int, 0-1 | ACK | - |
Modify overcurrent holdoff periods | | *SYS:DELAY_COEFF:SET:\<adc_oc>:\<batt_oc>:\<reg_oc>:\<uvlo_oc>:\<neg_batt_oc> | \<adc_oc> Holdoff in ms of an overcurrent reading on the individual 12V outputs, uint32<br>\<batt_oc> Holdoff in ms of an overcurrent reading on the global input, uint32<br>\<reg_oc> Holdoff in ms of an overcurrent reading on the 5V regulator output, uint32<br>\<uvlo_oc> Holdoff in ms of an undervoltage reading on the global input, uint32<br>\<neg_batt_oc> Holdoff in ms of a negative overcurrent reading on the global input, uint32 | ACK | - |
Read current overcurrent holdoff periods | | *SYS:DELAY_COEFF:GET? | - | \<adc_oc>:\<batt_oc>:\<reg_oc>:\<uvlo_oc>:\<neg_batt_oc> |\<adc_oc> Holdoff in ms of an overcurrent reading on the individual 12V outputs, uint32<br>\<batt_oc> Holdoff in ms of an overcurrent reading on the global input, uint32<br>\<reg_oc> Holdoff in ms of an overcurrent reading on the 5V regulator output, uint32<br>\<uvlo_oc> Holdoff in ms of an undervoltage reading on the global input, uint32<br>\<neg_batt_oc> Holdoff in ms of a negative overcurrent reading on the global input, uint32 |

The *SYS commands are for internal use and are not intended for end-users.

The output numbers are:

Num | Output
--- | ---
0 | H0
1 | H1
2 | L0
3 | L1
4 | L2
5 | L3
6 | 5V Regulator

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
