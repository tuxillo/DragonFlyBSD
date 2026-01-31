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

#ifndef _USB_DEVICE_H_
#define _USB_DEVICE_H_

/* DragonFly USB device header for LinuxKPI */

#include <dev/usb/usb.h>

/* USB device softc structure */
struct usb_device_softc {
    struct device sc_dev;
    struct usb_device *sc_udev;
    int sc_port;
    int sc_addr;
};

/* USB device methods - stubs */
static __inline int
usb_device_attach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_device_detach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_device_probe(device_t dev)
{
    return 0;  /* Stub */
}

/* USB device inquiry - stubs */
static __inline struct usb_device *
usb_device_get_udev(device_t dev)
{
    return NULL;  /* Stub */
}

static __inline int
usb_device_get_port(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_device_get_address(device_t dev)
{
    return 0;  /* Stub */
}

/* USB device power management - stubs */
static __inline int
usb_device_suspend(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
usb_device_resume(device_t dev)
{
    return 0;  /* Stub */
}

/* USB device reset - stub */
static __inline int
usb_device_reset(device_t dev)
{
    return 0;  /* Stub */
}

/* USB device enumeration - stub */
static __inline int
usb_device_enumerate(device_t dev)
{
    return 0;  /* Stub */
}

#endif /* _USB_DEVICE_H_ */
