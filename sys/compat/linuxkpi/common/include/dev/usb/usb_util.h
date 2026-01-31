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

#ifndef _USB_UTIL_H_
#define _USB_UTIL_H_

/* DragonFly USB utilities compatibility for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usb.h>

/* USB device information structure */
struct usb_device_info {
    uint8_t bus;
    uint8_t addr;
    uint8_t hub_addr;
    uint8_t hub_port;
    uint16_t vendor;
    uint16_t product;
    uint16_t release;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    char productname[128];
    char serial[64];
};

/* USB configuration info */
struct usb_config_info {
    uint8_t config_index;
    uint8_t config_num;
    uint16_t attributes;
    uint8_t max_power;
    char description[128];
};

/* USB interface info */
struct usb_interface_info {
    uint8_t config_index;
    uint8_t interface_index;
    uint8_t alt_index;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    char description[128];
};

/* USB endpoint info */
struct usb_endpoint_info {
    uint8_t config_index;
    uint8_t interface_index;
    uint8_t alt_index;
    uint8_t endpoint_index;
    uint8_t endpoint_address;
    uint8_t direction;
    uint8_t attributes;
    uint8_t transfer_type;
    uint16_t max_packet_size;
    uint8_t interval;
};

/* USB device enumeration - stubs */
static __inline int
usb_get_device_info(struct usb_device *udev, struct usb_device_info *udi)
{
    return -1;  /* Stub */
}

static __inline int
usb_get_config_info(struct usb_device *udev, struct usb_config_info *uci)
{
    return -1;  /* Stub */
}

static __inline int
usb_get_interface_info(struct usb_device *udev, struct usb_interface_info *uii)
{
    return -1;  /* Stub */
}

static __inline int
usb_get_endpoint_info(struct usb_device *udev, struct usb_endpoint_info *uei)
{
    return -1;  /* Stub */
}

/* USB string descriptor utilities - stubs */
static __inline int
usbd_get_string_desc(struct usb_device *udev, int si, char *buf, int buflen)
{
    return -1;  /* Stub */
}

static __inline const char *
usbd_get_string(struct usb_device *udev, int si, char *buf, int buflen)
{
    return NULL;  /* Stub */
}

/* USB vendor/product string lookup - stubs */
static __inline const char *
usbd_get_manufacturer(struct usb_device *udev, char *buf, int buflen)
{
    return NULL;  /* Stub */
}

static __inline const char *
usbd_get_product_string(struct usb_device *udev, char *buf, int buflen)
{
    return NULL;  /* Stub */
}

static __inline const char *
usbd_get_serial(struct usb_device *udev, char *buf, int buflen)
{
    return NULL;  /* Stub */
}

/* USB power utilities - stubs */
static __inline int
usbd_set_power_mode(struct usb_device *udev, int power_mode)
{
    return 0;  /* Stub */
}

static __inline int
usbd_get_power_mode(struct usb_device *udev)
{
    return 0;  /* Stub */
}

/* USB device state utilities - stubs */
static __inline int
usbd_get_device_state(struct usb_device *udev)
{
    return USB_STATE_DETACHED;  /* Stub */
}

static __inline int
usbd_get_bus_index(struct usb_device *udev)
{
    return 0;  /* Stub */
}

static __inline int
usbd_get_device_address(struct usb_device *udev)
{
    return 0;  /* Stub */
}

/* USB debug utilities - stubs */
static __inline void
usbd_dump_device(struct usb_device *udev)
{
    /* Stub */
}

static __inline void
usbd_dump_config(struct usb_config_descriptor *cd)
{
    /* Stub */
}

static __inline void
usbd_dump_interface(struct usb_interface_descriptor *id)
{
    /* Stub */
}

static __inline void
usbd_dump_endpoint(struct usb_endpoint_descriptor *ed)
{
    /* Stub */
}

/* USB error code utilities - stubs */
static __inline const char *
usbd_errstr(int error)
{
    switch (error) {
    case USB_ERR_NORMAL_COMPLETION:
        return "Normal completion";
    case USB_ERR_PENDING_REQUESTS:
        return "Pending requests";
    case USB_ERR_NOT_STARTED:
        return "Not started";
    case USB_ERR_INVAL:
        return "Invalid argument";
    case USB_ERR_NOMEM:
        return "No memory";
    case USB_ERR_CANCELLED:
        return "Cancelled";
    case USB_ERR_BAD_ADDRESS:
        return "Bad address";
    default:
        return "Unknown error";
    }
}

#endif /* _USB_UTIL_H_ */
