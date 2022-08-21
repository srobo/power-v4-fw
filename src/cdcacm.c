/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/dfu.h>

#include "cdcacm.h"
#include "msg_handler.h"
#include "output.h"
#include "global_vars.h"
#include "led.h"

#define SERIALNUM_BOOTLOADER_LOC 0x08001FE0

static usbd_device *g_usbd_dev;
bool re_enter_bootloader = false;

static const struct usb_device_descriptor dev = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = USB_CLASS_CDC,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = SR_DEV_VID,
    .idProduct = SR_DEV_PID,
    .bcdDevice = SR_DEV_REV,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

const struct usb_dfu_descriptor sr_dfu_function = {
        .bLength = sizeof(struct usb_dfu_descriptor),
        .bDescriptorType = DFU_FUNCTIONAL,
        .bmAttributes = USB_DFU_CAN_DOWNLOAD | USB_DFU_WILL_DETACH,
        .wDetachTimeout = 255,
        .wTransferSize = 128,
        .bcdDFUVersion = 0x011A,
};

/*
 * This notification endpoint isn't implemented. According to CDC spec its
 * optional, but its absence causes a NULL pointer dereference in Linux
 * cdc_acm driver.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x83,
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 16,
    .bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x01,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x82,
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64,
    .bInterval = 1,
}};

static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call_mgmt;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
    .header = {
        .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC = 0x0110,
    },
    .call_mgmt = {
        .bFunctionLength =
            sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = 1,
    },
    .acm = {
        .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities = 0,
    },
    .cdc_union = {
        .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bControlInterface = 0,
        .bSubordinateInterface0 = 1,
     }
};

static const struct usb_interface_descriptor comm_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
    .iInterface = 4,

    .endpoint = comm_endp,

    .extra = &cdcacm_functional_descriptors,
    .extralen = sizeof(cdcacm_functional_descriptors)
}};

static const struct usb_interface_descriptor data_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = 4,

    .endpoint = data_endp,
}};

const struct usb_interface_descriptor dfu_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 2,
    .bAlternateSetting = 0,
    .bNumEndpoints = 0,
    .bInterfaceClass = USB_CLASS_DFU,
    .bInterfaceSubClass = 0x01, // DFU
    .bInterfaceProtocol = 0x01, // Protocol 1.0
    .iInterface = 5,
    .extra = &sr_dfu_function,
    .extralen = sizeof(sr_dfu_function),
}};

static const struct usb_interface ifaces[] = {{
    .num_altsetting = 1,
    .altsetting = comm_iface,
}, {
    .num_altsetting = 1,
    .altsetting = data_iface,
}, {
    .num_altsetting = 1,
    .altsetting = dfu_iface,
}};

static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = 3,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = 0xC0,   // self powered
    .bMaxPower = 50,        // 100mA from USB

    .interface = ifaces,
};

static const char *usb_strings[] = {
    "Student Robotics",
    "Power Board v4",
    (const char *)SERIALNUM_BOOTLOADER_LOC,
	"Student Robotics Power Board v4", // Iface 1
	"Student Robotics Power Board DFU loader", // IFace 2, DFU
};

/* Buffer to be used for control requests. */
uint8_t usbd_control_buffer[128];

static enum usbd_request_return_codes cdcacm_control_request(usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
        uint16_t *len, void (**complete)(usbd_device *usbd_dev, struct usb_setup_data *req))
{
    (void)complete;
    (void)buf;
    (void)usbd_dev;

    switch(req->bRequest) {
    case USB_CDC_REQ_SET_CONTROL_LINE_STATE: {
        /*
         * This Linux cdc_acm driver requires this to be implemented
         * even though it's optional in the CDC spec, and we don't
         * advertise it in the ACM functional descriptor.
         */
        char local_buf[10];
        struct usb_cdc_notification *notif = (void *)local_buf;

        /* We echo signals back to host as notification. */
        notif->bmRequestType = 0xA1;
        notif->bNotification = USB_CDC_NOTIFY_SERIAL_STATE;
        notif->wValue = 0;
        notif->wIndex = 0;
        notif->wLength = 2;
        local_buf[8] = req->wValue & 3;
        local_buf[9] = 0;
        // usbd_ep_write_packet(0x83, buf, 10);
        return USBD_REQ_HANDLED;
        }
    case USB_CDC_REQ_SET_LINE_CODING:
        if(*len < sizeof(struct usb_cdc_line_coding))
            return USBD_REQ_NOTSUPP;

        return USBD_REQ_HANDLED;
    case DFU_GETSTATUS:
        *len = 6;
        (*buf)[0] = STATE_DFU_IDLE;
        (*buf)[1] = 100; // ms
        (*buf)[2] = 0;
        (*buf)[3] = 0;
        (*buf)[4] = DFU_STATUS_OK;
        (*buf)[5] = 0;
        return USBD_REQ_HANDLED;
    case DFU_DETACH:
        re_enter_bootloader = true;
        return USBD_REQ_HANDLED;
    }
    return USBD_REQ_NOTSUPP;
}

#define USB_MSG_MAXLEN 64
char usb_msg_buffer[USB_MSG_MAXLEN];
int usb_msg_len = 0;

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
    (void)ep;

    int remaining_len = (USB_MSG_MAXLEN - 1) - usb_msg_len;
    char* new_data_ptr = usb_msg_buffer + usb_msg_len;

    int len = usbd_ep_read_packet(usbd_dev, 0x01, new_data_ptr, remaining_len);

    if (len) {
        usb_msg_len += len;
        usb_msg_buffer[usb_msg_len] = '\0'; // add null terminator to make it a string

        char* end_of_msg = strchr(usb_msg_buffer, '\n'); // test if \n in buffer
        char response_buffer[64];
        char* response_ptr = response_buffer;
        int full_response_len = 0;

        while (end_of_msg != NULL) {
            *end_of_msg = '\0'; // replace newline with null terminator
            char* carriage_return = strchr(usb_msg_buffer, '\r');
            if (carriage_return) {
                *carriage_return = '\0';  // remove a \r
            }

            int msg_len = end_of_msg - usb_msg_buffer + 1;

            int usb_response_len = parse_msg(usb_msg_buffer, response_ptr, 63 - full_response_len);
            response_ptr[usb_response_len++] = '\n';  // replace null-terminator with newline
            full_response_len += usb_response_len;
            response_ptr += usb_response_len;

            usb_msg_len -= msg_len;
            if (usb_msg_len < 0){
                usb_msg_len = 0;
            }
            if (usb_msg_len > 0) {
                // move remaining data to start of buffer
                memmove(usb_msg_buffer, end_of_msg + 1, usb_msg_len);
            }

            // repeat if \n in buffer
            end_of_msg = strchr(usb_msg_buffer, '\n'); // test if \n in buffer
        }

        // drop a full buffer without newlines
        if (usb_msg_len == (USB_MSG_MAXLEN - 1)) {
            usb_msg_len = 0;
        }

        if (full_response_len > 0) {
            usbd_ep_write_packet(usbd_dev, 0x82, response_buffer, full_response_len);
        }
    }
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
    (void)wValue;

    usbd_ep_setup(usbd_dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, cdcacm_data_rx_cb);
    usbd_ep_setup(usbd_dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, NULL);
    usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);

    usbd_register_control_callback(
                usbd_dev,
                USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
                USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
                cdcacm_control_request);

    // Indicate we've enumerated
    clear_led(LED_ERROR);
}

void usb_init(void)
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_clear(GPIOA, GPIO8);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ,
              GPIO_CNF_OUTPUT_PUSHPULL, GPIO8);

    g_usbd_dev = usbd_init(&st_usbfs_v1_usb_driver, &dev, &config, usb_strings, 5, usbd_control_buffer, sizeof(usbd_control_buffer));
    usbd_register_set_config_callback(g_usbd_dev, cdcacm_set_config);
    // Callback run when the host resets the connection
    usbd_register_reset_callback(g_usbd_dev, usb_reset_callback);
    // Callback run when bus has no activity for 3ms, i.e. a disconnection
    usbd_register_suspend_callback(g_usbd_dev, usb_reset_callback);

    gpio_set(GPIOA, GPIO8);  // enable ext USB enable
}

void usb_deinit(void)
{
    // Clear ext USB enable; this will cause a reset for us and the host.
    gpio_clear(GPIOA, GPIO8);

    // Wait a few ms, then poll a few times to ensure that the driver
    // has reset itself
    delay(2);
    usb_poll();
    usb_poll();
    usb_poll();
    usb_poll();
}

void usb_poll(void)
{
    usbd_poll(g_usbd_dev);
}
