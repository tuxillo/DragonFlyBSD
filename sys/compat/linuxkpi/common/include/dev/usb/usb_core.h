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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_USB_USB_CORE_H_
#define _DEV_USB_USB_CORE_H_

/* DragonFly USB core definitions for LinuxKPI */

#include <sys/types.h>
#include <bus/u4b/usb_core.h>
#include <dev/usb/usb.h>

/*
 * USB core interface for LinuxKPI compatibility.
 *
 * This header provides compatibility wrappers around DragonFly's native
 * USB stack (u4b) for use by LinuxKPI USB drivers.
 *
 * DragonFly uses the u4b (USB4BSD) stack which is compatible with FreeBSD's
 * USB stack. This stub provides additional compatibility definitions.
 */

/* USB device speed definitions (if not in native header) */
#ifndef USB_SPEED_LOW
#define USB_SPEED_LOW		1
#define USB_SPEED_FULL		2
#define USB_SPEED_HIGH		3
#define USB_SPEED_SUPER		4
#define USB_SPEED_SUPER_PLUS	5
#endif

/* USB transfer types */
#ifndef USB_TRANSFER_CONTROL
#define USB_TRANSFER_CONTROL	0
#define USB_TRANSFER_ISOC	1
#define USB_TRANSFER_BULK	2
#define USB_TRANSFER_INTERRUPT	3
#endif

/* USB endpoint direction */
#ifndef UE_DIR_IN
#define UE_DIR_IN		0x80
#define UE_DIR_OUT		0x00
#define UE_DIR_MASK		0x80
#endif

/* USB endpoint attributes */
#ifndef UE_CONTROL
#define UE_CONTROL		0x00
#define UE_ISOCHRONOUS		0x01
#define UE_BULK			0x02
#define UE_INTERRUPT		0x03
#define UE_XFERTYPE		0x03
#endif

/* USB config constants */
#ifndef USB_MAX_DEVICES
#define USB_MAX_DEVICES		128
#define USB_MAX_PORTS		255
#endif

/* USB hub constants */
#ifndef USB_HUB_MAX_PORTS
#define USB_HUB_MAX_PORTS	255
#endif

/* USB string descriptor constants */
#ifndef USB_LANGUAGE_TABLE
#define USB_LANGUAGE_TABLE	0x0000
#define USB_MAX_STRING_LEN	128
#endif

/* USB power constants */
#ifndef USB_MIN_POWER
#define USB_MIN_POWER		0
#define USB_MAX_POWER		500
#define USB_SUPER_MAX_POWER	900
#endif

/* USB timeout constants */
#define USB_DEFAULT_TIMEOUT	5000	/* ms */
#define USB_SHORT_TIMEOUT	1000	/* ms */
#define USB_LONG_TIMEOUT	10000	/* ms */

/* USB error codes (LinuxKPI compatibility) */
#define USB_ERR_OK		0
#define USB_ERR_PENDING		1
#define USB_ERR_BUSY		16
#define USB_ERR_BAD_REQUEST	22
#define USB_ERR_TIMEOUT		62
#define USB_ERR_SHORT_XFER	145
#define USB_ERR_STALLED		70
#define USB_ERR_NO_MEMORY	12
#define USB_ERR_CANCELLED	89
#define USB_ERR_IOERROR		5

/*
 * USB core helper functions (stubs)
 */

/* Get USB device speed string */
static __inline const char *
usb_speed_string(int speed)
{
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
		return "super-plus";
	default:
		return "unknown";
	}
}

/* Check if USB device is at high speed or above */
static __inline int
usb_is_high_speed(struct usb_device *udev)
{
	(void)udev;
	return 0;
}

/* Check if USB device is at super speed or above */
static __inline int
usb_is_super_speed(struct usb_device *udev)
{
	(void)udev;
	return 0;
}

/* Get USB device endpoint by address */
static __inline struct usb_endpoint *
usb_get_endpoint(struct usb_device *udev, uint8_t ep_addr)
{
	(void)udev;
	(void)ep_addr;
	return NULL;
}

/* USB string descriptor helpers */
static __inline int
usb_string_id(struct usb_device *udev, const char *str)
{
	(void)udev;
	(void)str;
	return -1;
}

static __inline int
usb_make_str_desc(void *ptr, int size, const char *str)
{
	(void)ptr;
	(void)size;
	(void)str;
	return 0;
}

/* USB configuration helpers */
static __inline struct usb_config_descriptor *
usb_get_config_descriptor(struct usb_device *udev)
{
	(void)udev;
	return NULL;
}

/* USB power management stubs */
static __inline int
usb_set_power_state(struct usb_device *udev, int state)
{
	(void)udev;
	(void)state;
	return 0;
}

static __inline int
usb_get_power_state(struct usb_device *udev)
{
	(void)udev;
	return 0;
}

/* USB quirks handling */
#define USB_QUIRK_NONE			0x0000
#define USB_QUIRK_RESET			0x0001
#define USB_QUIRK_NO_STRINGS		0x0002
#define USB_QUIRK_SET_ADDR_BEFORE	0x0004
#define USB_QUIRK_TEST			0x0008
#define USB_QUIRK_DELAY_INIT		0x0010
#define USB_QUIRK_DELAY_ATTACH		0x0020

static __inline uint32_t
usb_get_quirks(struct usb_device *udev)
{
	(void)udev;
	return USB_QUIRK_NONE;
}

static __inline void
usb_set_quirks(struct usb_device *udev, uint32_t quirks)
{
	(void)udev;
	(void)quirks;
}

/* USB transfer flags */
#define USB_TRANSFER_FLAG_NONE		0x0000
#define USB_TRANSFER_FLAG_SHORT_OK	0x0001
#define USB_TRANSFER_FLAG_EXT_BUFFER	0x0002
#define USB_TRANSFER_FLAG_MANUAL_STATUS	0x0004

#endif /* _DEV_USB_USB_CORE_H_ */
