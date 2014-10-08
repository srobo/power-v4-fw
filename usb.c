#include <stdlib.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/stm32/gpio.h>

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

static uint16_t tmp_wvalue;
static uint16_t tmp_windex;

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
	// modifying what those point at. Initially, just "ping" by returning
	// the same data that was put in.

	if (req->bmRequestType & USB_REQ_TYPE_IN) { // i.e., input to host
		*len = 4;
		uint16_t *foo = (uint16_t*)*buf; // Yay type punning
		*foo++ = tmp_wvalue;
		*foo++ = tmp_windex;
	} else {
		tmp_wvalue = req->wValue;
		tmp_windex = req->wIndex;
	}

	return USBD_REQ_HANDLED;
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
