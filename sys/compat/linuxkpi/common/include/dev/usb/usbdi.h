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

#ifndef _DEV_USB_USBDI_H_
#define _DEV_USB_USBDI_H_

/* DragonFly USB device interface for LinuxKPI (FreeBSD USB DI) */

#include <sys/types.h>
#include <dev/usb/usb.h>

/*
 * USB Device Interface (USBDI) definitions.
 * This is the programming interface for USB device drivers.
 */

/* USB device handle */
struct usbd_device;
typedef struct usbd_device *usbd_device_handle;

/* USB interface handle */
struct usbd_interface;
typedef struct usbd_interface *usbd_interface_handle;

/* USB pipe (endpoint) handle */
struct usbd_pipe;
typedef struct usbd_pipe *usbd_pipe_handle;

/* USB transfer handle */
struct usbd_xfer;
typedef struct usbd_xfer *usbd_xfer_handle;

/* USB device request status callback */
typedef void (*usbd_callback)(usbd_xfer_handle xfer, usbd_private_handle priv,
                               usbd_status status);

/* USB device private handle - driver context */
typedef void *usbd_private_handle;

/* USB device status codes - extended from usb.h */
#define USBD_NORMAL_COMPLETION  USB_ERR_NORMAL_COMPLETION
#define USBD_IN_PROGRESS        USB_ERR_PENDING_REQUESTS
#define USBD_PENDING_REQUESTS   USB_ERR_PENDING_REQUESTS
#define USBD_NOT_STARTED        USB_ERR_NOT_STARTED
#define USBD_INVAL              USB_ERR_INVAL
#define USBD_NOMEM              USB_ERR_NOMEM
#define USBD_CANCELLED          USB_ERR_CANCELLED
#define USBD_BAD_ADDRESS        USB_ERR_BAD_ADDRESS
#define USBD_BAD_BUFSIZE        USB_ERR_BAD_BUFSIZE
#define USBD_BAD_FLAG           USB_ERR_BAD_FLAG
#define USBD_NO_CALLBACK        USB_ERR_NO_CALLBACK
#define USBD_IN_USE             USB_ERR_IN_USE
#define USBD_NO_PIPE            USB_ERR_NO_PIPE
#define USBD_ZERO_NOOK          USB_ERR_ZERO_NOOK
#define USBD_NO_RESOURCES       USB_ERR_NO_RESOURCES
#define USBD_TIMEOUT            14
#define USBD_SHORT_XFER         15
#define USBD_STALLED            16
#define USBD_IOERROR            17
#define USBD_NOT_CONFIGURED     18
#define USBD_UNEXPECTED_XFER    19

/* USB device speed - matches usb.h */
#define USBD_SPEED_LOW          USB_SPEED_LOW
#define USBD_SPEED_FULL         USB_SPEED_FULL
#define USBD_SPEED_HIGH         USB_SPEED_HIGH
#define USBD_SPEED_SUPER        USB_SPEED_SUPER
#define USBD_SPEED_VARIABLE     USB_SPEED_VARIABLE
#define USBD_SPEED_SUPER_PLUS   USB_SPEED_SUPER_PLUS

/* USB request types */
#define USBD_DEVICE_REQ         0x00
#define USBD_INTERFACE_REQ      0x01
#define USBD_ENDPOINT_REQ       0x02
#define USBD_OTHER_REQ          0x03

/* USB transfer types */
#define USBD_TRANSFER_CTRL      0x00
#define USBD_TRANSFER_ISO       0x01
#define USBD_TRANSFER_BULK      0x02
#define USBD_TRANSFER_INTR      0x03

/* USBD transfer flags */
#define USBD_SYNCHRONOUS        USB_FORCE_SHORT_XFER
#define USBD_NO_COPY            USB_AUTO_SHORT_XFER
#define USBD_FORCE_SHORT_XFER   USB_FORCE_SHORT_XFER
#define USBD_SHORT_XFER_OK      USB_AUTO_SHORT_XFER
#define USBD_DELAY_STATUS_STAGE USB_DELAY_STATUS_STAGE
#define USBD_USE_DMA            USB_USE_DMA
#define USBD_NO_TIMEOUT         USB_NO_TIMEOUT

/* USB device operations - interface for drivers */
struct usbd_bus_methods {
    usbd_status (*open_pipe)(struct usbd_pipe *pipe);
    void        (*close_pipe)(struct usbd_pipe *pipe);
    usbd_status (*transfer)(usbd_xfer_handle xfer);
    usbd_status (*start)(struct usbd_pipe *pipe);
    void        (*abort)(usbd_xfer_handle xfer);
    void        (*done)(usbd_xfer_handle xfer);
};

/* USB pipe operations */
struct usbd_pipe_methods {
    usbd_status (*transfer)(usbd_xfer_handle xfer);
    usbd_status (*start)(struct usbd_pipe *pipe);
    void        (*abort)(usbd_xfer_handle xfer);
    void        (*close)(struct usbd_pipe *pipe);
    void        (*cleartoggle)(struct usbd_pipe *pipe);
    void        (*done)(usbd_xfer_handle xfer);
};

/* USB bus structure - minimal stub */
struct usbd_bus {
    struct device         bus_dev;
    struct usbd_device    *root_hub;
    int                   bus_power;
    int                   bus_speed;
    int                   bus_address;
    void                  *usb_softc;
    struct usbd_bus_methods *methods;
};
typedef struct usbd_bus *usbd_bus_handle;

/* USB configuration information */
struct usbd_config {
    int                 id;
    int                 attrib;
    int                 max_power;
    int                 num_interfaces;
};

/* USB interface information */
struct usbd_interface_info {
    uint8_t             class;
    uint8_t             subclass;
    uint8_t             protocol;
    uint8_t             num_endpoints;
};

/* USB endpoint information */
struct usbd_endpoint_info {
    uint8_t             address;
    uint8_t             attributes;
    uint16_t            max_packet_size;
    uint8_t             interval;
};

/* USBD device operations - stubs */
static __inline usbd_device_handle
usbd_get_device(usbd_device_handle dev)
{
    return dev;
}

static __inline usbd_bus_handle
usbd_get_bus(usbd_device_handle dev)
{
    return NULL;
}

static __inline int
usbd_get_speed(usbd_device_handle dev)
{
    return USBD_SPEED_HIGH;
}

static __inline int
usbd_is_usb2(usbd_device_handle dev)
{
    return 1;
}

static __inline int
usbd_is_super_speed(usbd_device_handle dev)
{
    return 0;
}

/* USBD configuration operations - stubs */
static __inline usbd_status
usbd_set_config(usbd_device_handle dev, int config)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline usbd_status
usbd_get_config(usbd_device_handle dev, int *config)
{
    *config = 0;
    return USBD_NORMAL_COMPLETION;
}

static __inline const struct usbd_config *
usbd_get_config_descriptor(usbd_device_handle dev)
{
    return NULL;
}

/* USBD interface operations - stubs */
static __inline usbd_status
usbd_device2interface_handle(usbd_device_handle dev, uint8_t ifaceno,
                              usbd_interface_handle *iface)
{
    *iface = NULL;
    return USBD_NORMAL_COMPLETION;
}

static __inline const struct usbd_interface_info *
usbd_get_interface_descriptor(usbd_interface_handle iface)
{
    return NULL;
}

static __inline uint8_t
usbd_get_interface_number(usbd_interface_handle iface)
{
    return 0;
}

static __inline usbd_status
usbd_set_interface(usbd_interface_handle iface, int altidx)
{
    return USBD_NORMAL_COMPLETION;
}

/* USBD endpoint operations - stubs */
static __inline usbd_status
usbd_open_pipe(usbd_interface_handle iface, uint8_t address, uint8_t flags,
               usbd_pipe_handle *pipe)
{
    *pipe = NULL;
    return USBD_NORMAL_COMPLETION;
}

static __inline void
usbd_close_pipe(usbd_pipe_handle pipe)
{
}

static __inline const struct usbd_endpoint_info *
usbd_get_endpoint_descriptor(usbd_pipe_handle pipe)
{
    return NULL;
}

static __inline int
usbd_endpoint_address(usbd_pipe_handle pipe)
{
    return 0;
}

/* USBD transfer operations - stubs */
static __inline usbd_xfer_handle
usbd_alloc_xfer(usbd_device_handle dev)
{
    return NULL;
}

static __inline void
usbd_free_xfer(usbd_xfer_handle xfer)
{
}

static __inline void
usbd_setup_xfer(usbd_xfer_handle xfer, usbd_pipe_handle pipe,
                usbd_private_handle priv, void *buffer, uint32_t length,
                uint16_t flags, uint32_t timeout, usbd_callback callback)
{
}

static __inline void
usbd_setup_default_xfer(usbd_xfer_handle xfer, usbd_device_handle dev,
                        usbd_private_handle priv, uint32_t timeout,
                        struct usb_device_request *req, void *buffer,
                        uint32_t length, uint16_t flags, usbd_callback callback)
{
}

static __inline usbd_status
usbd_transfer(usbd_xfer_handle xfer)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline void
usbd_start_transfer(usbd_xfer_handle xfer)
{
}

static __inline void
usbd_abort_xfer(usbd_xfer_handle xfer)
{
}

static __inline void
usbd_clear_endpoint_toggle(usbd_pipe_handle pipe)
{
}

static __inline void
usbd_clear_endpoint_stall(usbd_pipe_handle pipe)
{
}

static __inline usbd_status
usbd_endpoint_count(usbd_interface_handle iface, uint8_t *count)
{
    *count = 0;
    return USBD_NORMAL_COMPLETION;
}

/* USBD sync transfer - stubs */
static __inline usbd_status
usbd_sync_transfer(usbd_xfer_handle xfer)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline int
usbd_get_xfer_status(usbd_xfer_handle xfer, void **priv, void **buffer,
                     uint32_t *count, usbd_status *status)
{
    if (status)
        *status = USBD_NORMAL_COMPLETION;
    return 0;
}

/* USBD descriptor operations - stubs */
static __inline usbd_status
usbd_get_desc(usbd_device_handle dev, int type, int index, int len, void *desc)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline usbd_status
usbd_get_string_desc(usbd_device_handle dev, int si, void *str)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline const struct usb_device_descriptor *
usbd_get_device_descriptor(usbd_device_handle dev)
{
    return NULL;
}

/* USBD string operations - stubs */
static __inline char *
usbd_get_string(usbd_device_handle dev, int si, char *buf, int len)
{
    if (buf && len > 0)
        buf[0] = '\0';
    return buf;
}

static __inline char *
usbd_get_interface_string(usbd_interface_handle iface, char *buf, int len)
{
    if (buf && len > 0)
        buf[0] = '\0';
    return buf;
}

/* USBD utility functions - stubs */
static __inline void
usbd_delay_ms(usbd_device_handle dev, u_int ms)
{
}

static __inline void
usbd_reset_device(usbd_device_handle dev)
{
}

static __inline void
usbd_get_lock(usbd_device_handle dev, struct lock **lock)
{
    *lock = NULL;
}

/* USBD pipe halt/clear - stubs */
static __inline usbd_status
usbd_halt_pipe(usbd_pipe_handle pipe)
{
    return USBD_NORMAL_COMPLETION;
}

static __inline usbd_status
usbd_clear_pipe_feature(usbd_pipe_handle pipe, int feature)
{
    return USBD_NORMAL_COMPLETION;
}

/* USBD error strings */
static __inline const char *
usbd_errstr(usbd_status err)
{
    switch (err) {
    case USBD_NORMAL_COMPLETION: return "normal completion";
    case USBD_IN_PROGRESS: return "in progress";
    case USBD_PENDING_REQUESTS: return "pending requests";
    case USBD_NOT_STARTED: return "not started";
    case USBD_INVAL: return "invalid argument";
    case USBD_NOMEM: return "no memory";
    case USBD_CANCELLED: return "cancelled";
    case USBD_BAD_ADDRESS: return "bad address";
    case USBD_BAD_BUFSIZE: return "bad buffer size";
    case USBD_BAD_FLAG: return "bad flag";
    case USBD_NO_CALLBACK: return "no callback";
    case USBD_IN_USE: return "in use";
    case USBD_NO_PIPE: return "no pipe";
    case USBD_ZERO_NOOK: return "zero not ok";
    case USBD_NO_RESOURCES: return "no resources";
    case USBD_TIMEOUT: return "timeout";
    case USBD_SHORT_XFER: return "short xfer";
    case USBD_STALLED: return "stalled";
    case USBD_IOERROR: return "I/O error";
    case USBD_NOT_CONFIGURED: return "not configured";
    case USBD_UNEXPECTED_XFER: return "unexpected xfer";
    default: return "unknown error";
    }
}

/* USBD probe/attach - stubs for driver methods */
static __inline int
usbd_probe(device_t dev)
{
    return 0;
}

static __inline int
usbd_attach(device_t dev)
{
    return 0;
}

static __inline int
usbd_detach(device_t dev)
{
    return 0;
}

static __inline int
usbd_suspend(device_t dev)
{
    return 0;
}

static __inline int
usbd_resume(device_t dev)
{
    return 0;
}

/* USBD match helper - stub */
static __inline int
usbd_match_device(const struct usb_device_id *id, device_t dev)
{
    return 0;
}

#endif /* _DEV_USB_USBDI_H_ */
