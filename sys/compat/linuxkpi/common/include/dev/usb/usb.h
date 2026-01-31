/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_USB_USB_H_
#define _DEV_USB_USB_H_

/* DragonFly USB compatibility for LinuxKPI (FreeBSD USB subsystem) */

#include <sys/types.h>

/* USB version numbers */
#define USB_VERSION_1_0 0x0100
#define USB_VERSION_1_1 0x0110
#define USB_VERSION_2_0 0x0200
#define USB_VERSION_2_1 0x0210
#define USB_VERSION_3_0 0x0300
#define USB_VERSION_3_1 0x0310
#define USB_VERSION_3_2 0x0320

/* USB device states */
#define USB_STATE_DETACHED 0
#define USB_STATE_ATTACHED 1
#define USB_STATE_POWERED 2
#define USB_STATE_RECOVERING 3
#define USB_STATE_DEFAULT 4
#define USB_STATE_ADDRESS 5
#define USB_STATE_CONFIGURED 6
#define USB_STATE_SUSPENDED 7

/* USB speeds */
#define USB_SPEED_LOW 0
#define USB_SPEED_FULL 1
#define USB_SPEED_HIGH 2
#define USB_SPEED_VARIABLE 3
#define USB_SPEED_SUPER 4
#define USB_SPEED_SUPER_PLUS 5

/* USB endpoint types */
#define USB_ENDPOINT_CONTROL 0x00
#define USB_ENDPOINT_ISOCHRONOUS 0x01
#define USB_ENDPOINT_BULK 0x02
#define USB_ENDPOINT_INTERRUPT 0x03

/* USB request types */
#define USB_REQUEST_TYPE_STANDARD 0x00
#define USB_REQUEST_TYPE_CLASS 0x20
#define USB_REQUEST_TYPE_VENDOR 0x40
#define USB_REQUEST_TYPE_RESERVED 0x60

/* USB standard requests */
#define USB_REQUEST_GET_STATUS 0x00
#define USB_REQUEST_CLEAR_FEATURE 0x01
#define USB_REQUEST_SET_FEATURE 0x03
#define USB_REQUEST_SET_ADDRESS 0x05
#define USB_REQUEST_GET_DESCRIPTOR 0x06
#define USB_REQUEST_SET_DESCRIPTOR 0x07
#define USB_REQUEST_GET_CONFIGURATION 0x08
#define USB_REQUEST_SET_CONFIGURATION 0x09
#define USB_REQUEST_GET_INTERFACE 0x0A
#define USB_REQUEST_SET_INTERFACE 0x0B
#define USB_REQUEST_SYNCH_FRAME 0x0C

/* USB descriptor types */
#define USB_DESCRIPTOR_TYPE_DEVICE 0x01
#define USB_DESCRIPTOR_TYPE_CONFIGURATION 0x02
#define USB_DESCRIPTOR_TYPE_STRING 0x03
#define USB_DESCRIPTOR_TYPE_INTERFACE 0x04
#define USB_DESCRIPTOR_TYPE_ENDPOINT 0x05
#define USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER 0x06
#define USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION 0x07
#define USB_DESCRIPTOR_TYPE_INTERFACE_POWER 0x08
#define USB_DESCRIPTOR_TYPE_OTG 0x09
#define USB_DESCRIPTOR_TYPE_DEBUG 0x0A
#define USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION 0x0B
#define USB_DESCRIPTOR_TYPE_BOS 0x0F
#define USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY 0x10
#define USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION 0x30

/* USB device descriptor */
struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

/* USB configuration descriptor */
struct usb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
};

/* USB interface descriptor */
struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
};

/* USB endpoint descriptor */
struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
};

/* USB request structure */
struct usb_device_request {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
};

/* USB device softc structure - stub */
struct usb_softc {
    struct device sc_dev;
    int sc_port;
    int sc_addr;
    int sc_conf;
    int sc_iface;
    int sc_alt_iface;
};

/* USB attach/detach - stubs */
static __inline int
usb_probe(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_attach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_detach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_suspend(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_resume(device_t dev)
{
    return 0;  /* Stub */
}

/* USB device operations - stubs */
static __inline int
usbd_transfer(struct usb_xfer *xfer)
{
    return USB_ERR_NORMAL_COMPLETION;  /* Stub */
}

static __inline void
usbd_transfer_start(struct usb_xfer *xfer)
{
    /* Stub */
}

static __inline void
usbd_transfer_stop(struct usb_xfer *xfer)
{
    /* Stub */
}

static __inline struct usb_xfer *
usbd_alloc_xfer(struct usb_device *udev, unsigned int nframes)
{
    return NULL;  /* Stub */
}

static __inline void
usbd_free_xfer(struct usb_xfer *xfer)
{
    /* Stub */
}

/* USB control request - stub */
static __inline int
usbd_do_request(struct usb_device *udev, struct mtx *mtx,
                struct usb_device_request *req, void *data)
{
    return USB_ERR_NORMAL_COMPLETION;  /* Stub */
}

/* USB clear stall - stub */
static __inline int
usbd_clear_stall(struct usb_device *udev, struct usb_endpoint *ep)
{
    return USB_ERR_NORMAL_COMPLETION;  /* Stub */
}

/* USB error codes */
#define USB_ERR_NORMAL_COMPLETION 0
#define USB_ERR_PENDING_REQUESTS 1
#define USB_ERR_NOT_STARTED 2
#define USB_ERR_INVAL 3
#define USB_ERR_NOMEM 4
#define USB_ERR_CANCELLED 5
#define USB_ERR_BAD_ADDRESS 6
#define USB_ERR_BAD_BUFSIZE 7
#define USB_ERR_BAD_FLAG 8
#define USB_ERR_NO_CALLBACK 9
#define USB_ERR_IN_USE 10
#define USB_ERR_NO_PIPE 11
#define USB_ERR_ZERO_NOOK 12
#define USB_ERR_NO_RESOURCES 13

/* USB transfer flags */
#define USB_FORCE_SHORT_XFER 0x0001
#define USB_AUTO_SHORT_XFER 0x0002
#define USB_DELAY_STATUS_STAGE 0x0004
#define USB_USER_DATA_PTR 0x0008
#define USB_NO_TIMEOUT 0x0010
#define USB_USE_DMA 0x0020

#endif /* _DEV_USB_USB_H_ */
