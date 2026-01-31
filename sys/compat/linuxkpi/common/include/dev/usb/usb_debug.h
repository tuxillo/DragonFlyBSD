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

#ifndef _USB_DEBUG_H_
#define _USB_DEBUG_H_

/* DragonFly USB debugging utilities for LinuxKPI */

#include <sys/types.h>
#include <sys/systm.h>
#include <dev/usb/usb.h>

/* USB debug levels */
#define USB_DEBUG_LEVEL_NONE		0
#define USB_DEBUG_LEVEL_ERROR		1
#define USB_DEBUG_LEVEL_WARNING		2
#define USB_DEBUG_LEVEL_INFO		3
#define USB_DEBUG_LEVEL_TRACE		4
#define USB_DEBUG_LEVEL_VERBOSE		5

/* USB debug categories */
#define USB_DEBUG_CATEGORY_GENERAL	0x0001
#define USB_DEBUG_CATEGORY_TRANSFER	0x0002
#define USB_DEBUG_CATEGORY_DEVICE	0x0004
#define USB_DEBUG_CATEGORY_HUB		0x0008
#define USB_DEBUG_CATEGORY_DESCRIPTOR	0x0010
#define USB_DEBUG_CATEGORY_ENUM		0x0020
#define USB_DEBUG_CATEGORY_POWER	0x0040
#define USB_DEBUG_CATEGORY_ALL		0xFFFF

/* Default USB debug level */
#ifndef USB_DEBUG_LEVEL
#define USB_DEBUG_LEVEL			USB_DEBUG_LEVEL_NONE
#endif

#ifndef USB_DEBUG_MASK
#define USB_DEBUG_MASK			USB_DEBUG_CATEGORY_ALL
#endif

/* USB debug state */
struct usb_debug_state {
	int		level;
	uint16_t	mask;
	uint32_t	flags;
};

/* USB debug flags */
#define USB_DEBUG_FLAG_TIMESTAMP	0x0001
#define USB_DEBUG_FLAG_FUNCTION		0x0002
#define USB_DEBUG_FLAG_THREAD		0x0004
#define USB_DEBUG_FLAG_FILE		0x0008

/* USB debug strings */
#define USB_DEBUG_STR_ERROR		"ERROR"
#define USB_DEBUG_STR_WARNING		"WARNING"
#define USB_DEBUG_STR_INFO		"INFO"
#define USB_DEBUG_STR_TRACE		"TRACE"
#define USB_DEBUG_STR_VERBOSE		"VERBOSE"

/* USB debug level to string - inline function */
static __inline const char *
usb_debug_level_str(int level)
{
	switch (level) {
	case USB_DEBUG_LEVEL_ERROR:
		return USB_DEBUG_STR_ERROR;
	case USB_DEBUG_LEVEL_WARNING:
		return USB_DEBUG_STR_WARNING;
	case USB_DEBUG_LEVEL_INFO:
		return USB_DEBUG_STR_INFO;
	case USB_DEBUG_LEVEL_TRACE:
		return USB_DEBUG_STR_TRACE;
	case USB_DEBUG_LEVEL_VERBOSE:
		return USB_DEBUG_STR_VERBOSE;
	default:
		return "UNKNOWN";
	}
}

/* USB debug category to string - inline function */
static __inline const char *
usb_debug_category_str(uint16_t category)
{
	switch (category) {
	case USB_DEBUG_CATEGORY_GENERAL:
		return "GENERAL";
	case USB_DEBUG_CATEGORY_TRANSFER:
		return "TRANSFER";
	case USB_DEBUG_CATEGORY_DEVICE:
		return "DEVICE";
	case USB_DEBUG_CATEGORY_HUB:
		return "HUB";
	case USB_DEBUG_CATEGORY_DESCRIPTOR:
		return "DESCRIPTOR";
	case USB_DEBUG_CATEGORY_ENUM:
		return "ENUM";
	case USB_DEBUG_CATEGORY_POWER:
		return "POWER";
	default:
		return "UNKNOWN";
	}
}

/* USB error code to string - inline function */
static __inline const char *
usb_debug_error_str(int err)
{
	switch (err) {
	case USB_ERR_NORMAL_COMPLETION:
		return "OK";
	case USB_ERR_PENDING_REQUESTS:
		return "PENDING";
	case USB_ERR_NOT_STARTED:
		return "NOT_STARTED";
	case USB_ERR_INVAL:
		return "INVAL";
	case USB_ERR_NOMEM:
		return "NOMEM";
	case USB_ERR_CANCELLED:
		return "CANCELLED";
	case USB_ERR_BAD_ADDRESS:
		return "BAD_ADDRESS";
	case USB_ERR_BAD_BUFSIZE:
		return "BAD_BUFSIZE";
	case USB_ERR_BAD_FLAG:
		return "BAD_FLAG";
	case USB_ERR_NO_CALLBACK:
		return "NO_CALLBACK";
	case USB_ERR_IN_USE:
		return "IN_USE";
	case USB_ERR_NO_PIPE:
		return "NO_PIPE";
	case USB_ERR_ZERO_NOOK:
		return "ZERO_NOOK";
	case USB_ERR_NO_RESOURCES:
		return "NO_RESOURCES";
	default:
		return "UNKNOWN";
	}
}

/* USB speed to string - inline function */
static __inline const char *
usb_debug_speed_str(int speed)
{
	switch (speed) {
	case USB_SPEED_LOW:
		return "low";
	case USB_SPEED_FULL:
		return "full";
	case USB_SPEED_HIGH:
		return "high";
	case USB_SPEED_VARIABLE:
		return "variable";
	case USB_SPEED_SUPER:
		return "super";
	case USB_SPEED_SUPER_PLUS:
		return "super-plus";
	default:
		return "unknown";
	}
}

/* USB state to string - inline function */
static __inline const char *
usb_debug_state_str(int state)
{
	switch (state) {
	case USB_STATE_DETACHED:
		return "detached";
	case USB_STATE_ATTACHED:
		return "attached";
	case USB_STATE_POWERED:
		return "powered";
	case USB_STATE_RECOVERING:
		return "recovering";
	case USB_STATE_DEFAULT:
		return "default";
	case USB_STATE_ADDRESS:
		return "address";
	case USB_STATE_CONFIGURED:
		return "configured";
	case USB_STATE_SUSPENDED:
		return "suspended";
	default:
		return "unknown";
	}
}

/* USB endpoint type to string - inline function */
static __inline const char *
usb_debug_ep_type_str(int type)
{
	switch (type) {
	case USB_ENDPOINT_CONTROL:
		return "control";
	case USB_ENDPOINT_ISOCHRONOUS:
		return "isochronous";
	case USB_ENDPOINT_BULK:
		return "bulk";
	case USB_ENDPOINT_INTERRUPT:
		return "interrupt";
	default:
		return "unknown";
	}
}

/* USB descriptor type to string - inline function */
static __inline const char *
usb_debug_desc_type_str(int type)
{
	switch (type) {
	case USB_DESCRIPTOR_TYPE_DEVICE:
		return "device";
	case USB_DESCRIPTOR_TYPE_CONFIGURATION:
		return "configuration";
	case USB_DESCRIPTOR_TYPE_STRING:
		return "string";
	case USB_DESCRIPTOR_TYPE_INTERFACE:
		return "interface";
	case USB_DESCRIPTOR_TYPE_ENDPOINT:
		return "endpoint";
	case USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
		return "device_qualifier";
	case USB_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION:
		return "other_speed_configuration";
	case USB_DESCRIPTOR_TYPE_INTERFACE_POWER:
		return "interface_power";
	case USB_DESCRIPTOR_TYPE_OTG:
		return "otg";
	case USB_DESCRIPTOR_TYPE_DEBUG:
		return "debug";
	case USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
		return "interface_association";
	case USB_DESCRIPTOR_TYPE_BOS:
		return "bos";
	case USB_DESCRIPTOR_TYPE_DEVICE_CAPABILITY:
		return "device_capability";
	case USB_DESCRIPTOR_TYPE_SS_ENDPOINT_COMPANION:
		return "ss_endpoint_companion";
	default:
		return "unknown";
	}
}

/* USB debug print function - stub */
static __inline void
usb_debug_print(int level, uint16_t category, const char *fmt, ...)
{
	(void)level;
	(void)category;
	(void)fmt;
}

/* USB debug dump buffer - stub */
static __inline void
usb_debug_dump_buffer(const char *label, const void *buf, size_t len)
{
	(void)label;
	(void)buf;
	(void)len;
}

/* USB debug dump descriptor - stub */
static __inline void
usb_debug_dump_descriptor(const void *desc, int type)
{
	(void)desc;
	(void)type;
}

/* USB debug dump request - stub */
static __inline void
usb_debug_dump_request(const struct usb_device_request *req)
{
	(void)req;
}

/* USB debug dump device - stub */
static __inline void
usb_debug_dump_device(struct usb_device *udev)
{
	(void)udev;
}

/* USB debug dump endpoint - stub */
static __inline void
usb_debug_dump_endpoint(struct usb_endpoint *ep)
{
	(void)ep;
}

/* USB debug enable/disable - stubs */
static __inline void
usb_debug_enable(void)
{
}

static __inline void
usb_debug_disable(void)
{
}

static __inline int
usb_debug_set_level(int level)
{
	(void)level;
	return 0;
}

static __inline int
usb_debug_get_level(void)
{
	return USB_DEBUG_LEVEL_NONE;
}

static __inline int
usb_debug_set_mask(uint16_t mask)
{
	(void)mask;
	return 0;
}

static __inline uint16_t
usb_debug_get_mask(void)
{
	return USB_DEBUG_MASK;
}

/* USB debug check - stub */
static __inline int
usb_debug_enabled(int level, uint16_t category)
{
	(void)level;
	(void)category;
	return 0;
}

/* USB tracing macros - minimal stubs */
#define USB_TRACE(msg)				do { } while (0)
#define USB_TRACE1(msg, arg1)			do { } while (0)
#define USB_TRACE2(msg, arg1, arg2)		do { } while (0)
#define USB_TRACE3(msg, arg1, arg2, arg3)	do { } while (0)

#define USB_DEBUG(msg)				do { } while (0)
#define USB_DEBUG1(msg, arg1)			do { } while (0)
#define USB_DEBUG2(msg, arg1, arg2)		do { } while (0)
#define USB_DEBUG3(msg, arg1, arg2, arg3)	do { } while (0)

#define USB_INFO(msg)				do { } while (0)
#define USB_INFO1(msg, arg1)			do { } while (0)
#define USB_INFO2(msg, arg1, arg2)		do { } while (0)

#define USB_WARNING(msg)			do { } while (0)
#define USB_WARNING1(msg, arg1)			do { } while (0)
#define USB_WARNING2(msg, arg1, arg2)		do { } while (0)

#define USB_ERROR(msg)				do { } while (0)
#define USB_ERROR1(msg, arg1)			do { } while (0)
#define USB_ERROR2(msg, arg1, arg2)		do { } while (0)

#endif /* _USB_DEBUG_H_ */
