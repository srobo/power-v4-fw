Power Board v4 Firmware
=======================

The Power Board distributes power to the SR kit from the battery. It
provides six individual general-purpose power outputs along with two low
current 5V power outputs.

It also holds the internal On\|Off switch for the whole robot as well as
the Start button which is used to start your robot code running.

USB Interface
-------------

The Vendor ID is `1bda` (University of Southampton) and the product ID
is `0010`.

The Power Board is controlled over USB by sending requests to the
control endpoint.

```python
ctrl_transfer(
    0x00,
    64,
    wValue=req_val,
    wIndex=command.code,
    data_or_wLength=req_data,
)
```

| Parameter     | Value |
|---------------|-------|
| bmRequestType | 0x00  |
| bRequest      | 64    |

There are a list of ids defined in the firmware of the power board that
will let you read and write values to it.

It is recommended to read the source to further understand how to
control this device.

It should also be noted that as the control endpoint `0x00` is used to
send data to this device, it is not actually compliant with the USB 2.0
specification.

udev Rule
---------

If you are connecting the Power Board to a Linux computer with udev, the
following rule can be added in order to access the Power Board interface
without root privileges:

`SUBSYSTEM=="usb", ATTRS{idVendor}=="1bda", ATTRS{idProduct}=="0010", GROUP="plugdev", MODE="0666"`

It should be noted that `plugdev` can be changed to any Unix group of
your preference.

Designs
-------

You can access the schematics and source code of the hardware in the following places.
-   [Full
    Schematics](https://www.studentrobotics.org/resources/kit/power-schematic.pdf)
-   [Student Facing Docs](https://studentrobotics.org/docs/kit/power_board)
-   [Hardware designs](https://github.com/srobo/power-v4-hw)
