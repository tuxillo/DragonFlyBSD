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

#ifndef _DEV_USB_USBDI_UTIL_H_
#define _DEV_USB_USBDI_UTIL_H_

/* DragonFly USB Device Interface Utilities for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usbdi.h>

/*
 * USBDI Utilities - Helper functions for USB device drivers.
 * These functions provide convenient wrappers and utilities
 * for common USB operations.
 */

/* USB interface alternate setting */
struct usbd_altset {
    uint8_t     alt_no;         /* Alternate setting number */
    uint8_t     num_endpoints;  /* Number of endpoints */
    uint8_t     class;          /* Interface class */
    uint8_t     subclass;       /* Interface subclass */
    uint8_t     protocol;       /* Interface protocol */
};

/* USB driver info structure */
struct usbd_driver_info {
    const char  *udi_name;          /* Driver name */
    const struct usb_device_id *udi_id_table;  /* Device ID table */
    void        *udi_priv;          /* Driver private data */
};

/*
 * USB device utility functions
 */

/* Get device vendor ID */
static __inline uint16_t
usbd_get_vendor(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? UGETW(dd->idVendor) : 0;
}

/* Get device product ID */
static __inline uint16_t
usbd_get_product(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? UGETW(dd->idProduct) : 0;
}

/* Get device release number */
static __inline uint16_t
usbd_get_release(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? UGETW(dd->bcdDevice) : 0;
}

/* Get device class */
static __inline uint8_t
usbd_get_class(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? dd->bDeviceClass : 0;
}

/* Get device subclass */
static __inline uint8_t
usbd_get_subclass(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? dd->bDeviceSubClass : 0;
}

/* Get device protocol */
static __inline uint8_t
usbd_get_protocol(usbd_device_handle dev)
{
    const struct usb_device_descriptor *dd;
    dd = usbd_get_device_descriptor(dev);
    return dd ? dd->bDeviceProtocol : 0;
}

/* Get device speed string */
static __inline const char *
usbd_get_speedstr(usbd_device_handle dev)
{
    int speed = usbd_get_speed(dev);
    switch (speed) {
    case USB_SPEED_LOW:
        return "low";
    case USB_SPEED_FULL:
        return "full";
    case USB_SPEED_HIGH:
        return "high";
    case USB_SPEED_SUPER:
        return "super";
    case USB_SPEED_SUPER_PLUS:
        return "super plus";
    default:
        return "unknown";
    }
}

/*
 * USB descriptor utility functions
 */

/* Get descriptor length by type */
static __inline int
usbd_get_desc_size(int type)
{
    switch (type) {
    case UDESC_DEVICE:
        return USB_DEVICE_DESCRIPTOR_SIZE;
    case UDESC_CONFIG:
        return USB_CONFIG_DESCRIPTOR_SIZE;
    case UDESC_STRING:
        return 2;  /* Minimum string descriptor size */
    case UDESC_INTERFACE:
        return USB_INTERFACE_DESCRIPTOR_SIZE;
    case UDESC_ENDPOINT:
        return USB_ENDPOINT_DESCRIPTOR_SIZE;
    case UDESC_DEVICE_QUALIFIER:
        return USB_DEVICE_QUALIFIER_SIZE;
    case UDESC_OTHER_SPEED_CONFIG:
        return USB_CONFIG_DESCRIPTOR_SIZE;
    case UDESC_INTERFACE_POWER:
        return USB_INTERFACE_POWER_SIZE;
    case UDESC_OTG:
        return USB_OTG_DESCRIPTOR_SIZE;
    case UDESC_CS_DEVICE:
    case UDESC_CS_CONFIG:
    case UDESC_CS_STRING:
    case UDESC_CS_INTERFACE:
    case UDESC_CS_ENDPOINT:
        return -1;  /* Class-specific, variable size */
    default:
        return -1;
    }
}

/* Check if endpoint is IN direction */
static __inline int
usbd_is_ep_in(uint8_t addr)
{
    return (addr & UE_DIR_IN) != 0;
}

/* Check if endpoint is OUT direction */
static __inline int
usbd_is_ep_out(uint8_t addr)
{
    return (addr & UE_DIR_IN) == 0;
}

/* Get endpoint number from address */
static __inline uint8_t
usbd_ep_num(uint8_t addr)
{
    return UE_GET_ADDR(addr);
}

/* Get endpoint direction string */
static __inline const char *
usbd_ep_dirstr(uint8_t addr)
{
    return usbd_is_ep_in(addr) ? "in" : "out";
}

/*
 * USB transfer utility functions
 */

/* Calculate timeout in milliseconds */
static __inline uint32_t
usbd_calc_timeout(uint32_t len, uint32_t speed)
{
    /* Simple calculation: 1 second per 64KB at full speed */
    uint32_t timeout;
    
    switch (speed) {
    case USB_SPEED_LOW:
        timeout = (len / 8) + 5000;  /* 5 second minimum */
        break;
    case USB_SPEED_FULL:
        timeout = (len / 64) + 5000;
        break;
    case USB_SPEED_HIGH:
        timeout = (len / 512) + 5000;
        break;
    case USB_SPEED_SUPER:
    case USB_SPEED_SUPER_PLUS:
        timeout = (len / 4096) + 5000;
        break;
    default:
        timeout = 5000;  /* 5 second default */
        break;
    }
    
    return timeout > 60000 ? 60000 : timeout;  /* Cap at 60 seconds */
}

/*
 * USB synchronization utilities
 */

/* USB synchronous control transfer */
static __inline usbd_status
usbd_do_request(usbd_device_handle dev, usb_device_request_t *req, void *data)
{
    usbd_xfer_handle xfer;
    usbd_status err;
    
    xfer = usbd_alloc_xfer(dev);
    if (xfer == NULL)
        return USBD_NOMEM;
    
    usbd_setup_default_xfer(xfer, dev, NULL, 5000, req, data, 
                            UGETW(req->wLength), 0, NULL);
    err = usbd_sync_transfer(xfer);
    usbd_free_xfer(xfer);
    
    return err;
}

/* USB clear endpoint halt */
static __inline usbd_status
usbd_clear_endpoint_stall_async(usbd_pipe_handle pipe)
{
    usbd_clear_endpoint_stall(pipe);
    return USBD_NORMAL_COMPLETION;
}

/* USB set interface alternate setting */
static __inline usbd_status
usbd_set_interface_alt(usbd_interface_handle iface, int alt)
{
    return usbd_set_interface(iface, alt);
}

/*
 * USB debug utilities
 */

/* Get status string */
static __inline const char *
usbd_get_string_status(usbd_status status)
{
    return usbd_errstr(status);
}

/* Print USB device info (stub) */
static __inline void
usbd_devinfo(usbd_device_handle dev, int showclass, char *buf, size_t len)
{
    if (buf && len > 0) {
        snprintf(buf, len, "vid=0x%04x pid=0x%04x",
                 usbd_get_vendor(dev), usbd_get_product(dev));
    }
}

/* Print USB interface info (stub) */
static __inline void
usbd_ifinfo(usbd_interface_handle iface, char *buf, size_t len)
{
    if (buf && len > 0)
        buf[0] = '\0';
}

/*
 * USB match utilities
 */

/* Match device by vendor/product */
static __inline int
usbd_match_device_id(usbd_device_handle dev, const struct usb_device_id *id)
{
    uint16_t vendor, product;
    
    vendor = usbd_get_vendor(dev);
    product = usbd_get_product(dev);
    
    if (id->idVendor != USB_VENDOR_ANY && id->idVendor != vendor)
        return 0;
    if (id->idProduct != USB_PRODUCT_ANY && id->idProduct != product)
        return 0;
    
    return 1;
}

/* Match interface by class/subclass/protocol */
static __inline int
usbd_match_interface_id(usbd_interface_handle iface, 
                        uint8_t class, uint8_t subclass, uint8_t protocol)
{
    const struct usbd_interface_info *info;
    
    info = usbd_get_interface_descriptor(iface);
    if (info == NULL)
        return 0;
    
    if (class != USB_CLASS_ANY && class != info->class)
        return 0;
    if (subclass != USB_SUBCLASS_ANY && subclass != info->subclass)
        return 0;
    if (protocol != USB_PROTOCOL_ANY && protocol != info->protocol)
        return 0;
    
    return 1;
}

/*
 * USB config/alt setting iteration
 */

/* Simple config iterator (stub) */
#define USBD_CONFIG_FOREACH(cfg, dev) \
    for ((cfg) = NULL; (cfg) == NULL; (cfg) = (void *)1)

/* Simple alt setting iterator (stub) */
#define USBD_ALTSET_FOREACH(alt, iface) \
    for ((alt) = NULL; (alt) == NULL; (alt) = (void *)1)

#endif /* _DEV_USB_USBDI_UTIL_H_ */
