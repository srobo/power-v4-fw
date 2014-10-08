#include <stdlib.h>
#include <libopencm3/usb/usbd.h>

usbd_device *usbd_dev;

const struct usb_device_descriptor dev_descr = {
        .bLength = USB_DT_DEVICE_SIZE,
        .bDescriptorType = USB_DT_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0,
        .bDeviceSubClass = 0,
        .bDeviceProtocol = 0,
        .bMaxPacketSize0 = 64,
        .idVendor = 0x1BDA,     // ECS VID
        .idProduct = 0x1,       // Our power board PID is pending
        .bcdDevice = 0x04,      // 4th power boarda design
        .iManufacturer = 1,
        .iProduct = 2,
        .iSerialNumber = 3,
        .bNumConfigurations = 1,
};

const struct usb_config_descriptor usb_config = {
        .bLength = USB_DT_CONFIGURATION_SIZE,
        .bDescriptorType = USB_DT_CONFIGURATION,
        .wTotalLength = 0,
        .bNumInterfaces = 0,   // No interfaces, we only use default ctrl pipe
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = 0xC0,  // Bit 6 -> self powered
        .bMaxPower = 5,        // Will consume 10ma from USB (a guess)
};

static const char *usb_strings[] = {
        "Student Robotics",
        "Power board v4",
        "0123456789",          // XXX serial numbers
};

static uint8_t usb_data_buffer[128];

void
usb_init()
{

  usbd_dev = usbd_init(&stm32f103_usb_driver, &usb_descr, &usb_config,
		 usb_strings, 3, usb_data_buffer, sizeof(usb_data_buffer));
}
