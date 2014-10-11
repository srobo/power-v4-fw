#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/gpio.h>

#include "usb.h"
#include "output.h"
#include "led.h"
#include "battery.h"

static usbd_device *usbd_dev;

static const struct usb_device_descriptor usb_descr = {
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

// Linux whines if there are no interfaces, so have a dummy, with no endpoints.
// We'll use the default control endpoint for everything.
const struct usb_interface_descriptor dummyiface = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 0,
        .bInterfaceClass = 0,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
};

const struct usb_interface usb_ifaces[] = {{
        .num_altsetting = 1,
        .altsetting = &dummyiface,
}};

static const struct usb_config_descriptor usb_config = {
        .bLength = USB_DT_CONFIGURATION_SIZE,
        .bDescriptorType = USB_DT_CONFIGURATION,
        .wTotalLength = 0,
        .bNumInterfaces = 1,   // One dummy, to appease linux
        .bConfigurationValue = 1,
        .iConfiguration = 0,
        .bmAttributes = 0xC0,  // Bit 6 -> self powered
        .bMaxPower = 5,        // Will consume 10ma from USB (a guess)
	.interface = usb_ifaces,
};

static const char *usb_strings[] = {
        "Student Robotics",
        "Power board v4",
        "0123456789",          // XXX serial numbers
};

static uint8_t usb_data_buffer[128];

static int
read_output(int *len, uint8_t **buf, int output)
{
	if (*len < 4)
		return USBD_REQ_NOTSUPP;

	uint32_t *u32ptr = (uint32_t*)*buf;
	*u32ptr = current_sense_read(output);
	*len = 4;
	return USBD_REQ_HANDLED;
}

static int
handle_read_req(struct usb_setup_data *req, int *len, uint8_t **buf)
{
	int result = USBD_REQ_NOTSUPP; // Will result in a USB stall
	uint16_t *u16ptr;

	// Precise command, as enumerated in usb.h, is in wIndex
	switch (req->wIndex) {
	case POWERBOARD_READ_OUTPUT0:
		result = read_output(len, buf, 0); break;
	case POWERBOARD_READ_OUTPUT1:
		result = read_output(len, buf, 1); break;
	case POWERBOARD_READ_OUTPUT2:
		result = read_output(len, buf, 2); break;
	case POWERBOARD_READ_OUTPUT3:
		result = read_output(len, buf, 3); break;
	case POWERBOARD_READ_OUTPUT4:
		result = read_output(len, buf, 4); break;
	case POWERBOARD_READ_OUTPUT5:
		result = read_output(len, buf, 5); break;
	case POWERBOARD_READ_5VRAIL:
		if (*len < 4)
			break;

		*len = 4;

		// Clocking i2c can take a lot of time!
		u16ptr = (uint16_t*) *buf;
		*u16ptr++ = f_vshunt();
		*u16ptr++ = f_vbus();
		result = USBD_REQ_HANDLED;
		break;

	case POWERBOARD_READ_BATT:
		if (*len < 4)
			break;

		*len = 4;

		// Clocking i2c can take a lot of time!
		u16ptr = (uint16_t*) *buf;
		*u16ptr++ = battery_vshunt();
		*u16ptr++ = battery_vbus();
		result = USBD_REQ_HANDLED;
		break;

	case POWERBOARD_READ_BUTTON:
	case POWERBOARD_READ_FWVER:
	default:
		break;
	}

	return result;
}

static void
write_output(int id, uint16_t param)
{

	if (param == 0) {
		// Set output off
		output_off(id);
	} else {
		output_on(id);
	}
}

static void
write_led(int id, uint16_t param)
{

	if (param == 0) {
		// Set led off
		led_clear(id);
	} else {
		led_set(id);
	}
}

static int
handle_write_req(struct usb_setup_data *req)
{

	switch (req->wIndex) {
	case POWERBOARD_WRITE_OUTPUT0:
		write_output(0, req->wValue); break;
	case POWERBOARD_WRITE_OUTPUT1:
		write_output(1, req->wValue); break;
	case POWERBOARD_WRITE_OUTPUT2:
		write_output(2, req->wValue); break;
	case POWERBOARD_WRITE_OUTPUT3:
		write_output(3, req->wValue); break;
	case POWERBOARD_WRITE_OUTPUT4:
		write_output(4, req->wValue); break;
	case POWERBOARD_WRITE_OUTPUT5:
		write_output(5, req->wValue); break;
	case POWERBOARD_WRITE_RUNLED:
		write_led(LED_RUN, req->wValue); break;
	case POWERBOARD_WRITE_ERRORLED:
		write_led(LED_ERROR, req->wValue); break;
	default:
		return USBD_REQ_NOTSUPP; // Will result in a USB stall
	}

	return USBD_REQ_HANDLED;
}

static int
control(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
	uint16_t *len,
	void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
	// Only respond to a single device request, number 64. USB spec 3.1
	// Section 9.3.1 allows us to define additional requests, and Table 9.5
	// identifies all reserved requests. So, pick 64, it could be any.
	if (req->bRequest != 64)
		return USBD_REQ_NEXT_CALLBACK;

	// Data and length are in *buf and *len respectively. Output occurs by
	// modifying what those point at.

	if (req->bmRequestType & USB_REQ_TYPE_IN) { // i.e., input to host
		return handle_read_req(req, len, buf);
	} else {
		return handle_write_req(req);
	}
}

static void
set_config_cb(usbd_device *usbd_dev, uint16_t wValue)
{

  // We want to handle device requests sent to the default endpoint: match the
  // device request type (0), with zero recpient address. Use type + recipient
  // mask to filter requests.
  usbd_register_control_callback(usbd_dev, USB_REQ_TYPE_DEVICE,
		  USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		  control);
}

void
usb_init()
{

  gpio_clear(GPIOA, GPIO8);
  gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO8);

  usbd_dev = usbd_init(&stm32f103_usb_driver, &usb_descr, &usb_config,
		 usb_strings, 3, usb_data_buffer, sizeof(usb_data_buffer));

  usbd_register_set_config_callback(usbd_dev, set_config_cb);

  gpio_set(GPIOA, GPIO8);
}

void
usb_poll()
{

	usbd_poll(usbd_dev);
}
