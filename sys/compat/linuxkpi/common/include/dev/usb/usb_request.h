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

#ifndef _USB_REQUEST_H_
#define _USB_REQUEST_H_

/* DragonFly USB request handling for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usb.h>

/* USB request types */
#define USB_REQUEST_TYPE_DIRECTION		0x80
#define USB_REQUEST_TYPE_DIRECTION_IN		0x80
#define USB_REQUEST_TYPE_DIRECTION_OUT		0x00
#define USB_REQUEST_TYPE_TYPE			0x60
#define USB_REQUEST_TYPE_TYPE_STANDARD		0x00
#define USB_REQUEST_TYPE_TYPE_CLASS		0x20
#define USB_REQUEST_TYPE_TYPE_VENDOR		0x40
#define USB_REQUEST_TYPE_TYPE_RESERVED		0x60
#define USB_REQUEST_TYPE_RECIPIENT		0x1F
#define USB_REQUEST_TYPE_RECIPIENT_DEVICE	0x00
#define USB_REQUEST_TYPE_RECIPIENT_INTERFACE	0x01
#define USB_REQUEST_TYPE_RECIPIENT_ENDPOINT	0x02
#define USB_REQUEST_TYPE_RECIPIENT_OTHER	0x03

/* USB standard request codes */
#define USB_REQUEST_GET_STATUS			0x00
#define USB_REQUEST_CLEAR_FEATURE		0x01
#define USB_REQUEST_SET_FEATURE			0x03
#define USB_REQUEST_SET_ADDRESS			0x05
#define USB_REQUEST_GET_DESCRIPTOR		0x06
#define USB_REQUEST_SET_DESCRIPTOR		0x07
#define USB_REQUEST_GET_CONFIGURATION		0x08
#define USB_REQUEST_SET_CONFIGURATION		0x09
#define USB_REQUEST_GET_INTERFACE		0x0A
#define USB_REQUEST_SET_INTERFACE		0x0B
#define USB_REQUEST_SYNCH_FRAME			0x0C
#define USB_REQUEST_SET_SEL			0x30
#define USB_REQUEST_SET_ISOCH_DELAY		0x31

/* USB feature selectors */
#define USB_FEATURE_ENDPOINT_HALT		0x00
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP	0x01
#define USB_FEATURE_TEST_MODE			0x02
#define USB_FEATURE_DEVICE_BATTERY		0x02  /* Wireless */
#define USB_FEATURE_DEVICE_B_HNP_ENABLE		0x03
#define USB_FEATURE_DEVICE_WUSB_DEVICE		0x03  /* Wireless */
#define USB_FEATURE_DEVICE_A_HNP_SUPPORT	0x04
#define USB_FEATURE_DEVICE_A_ALT_HNP_SUPPORT	0x05
#define USB_FEATURE_DEVICE_DEBUG_MODE		0x06
#define USB_FEATURE_U1_ENABLE			0x30
#define USB_FEATURE_U2_ENABLE			0x31
#define USB_FEATURE_LTM_ENABLE			0x32

/* USB test mode selectors */
#define USB_TEST_MODE_TEST_J			0x01
#define USB_TEST_MODE_TEST_K			0x02
#define USB_TEST_MODE_TEST_SE0_NAK		0x03
#define USB_TEST_MODE_TEST_PACKET		0x04
#define USB_TEST_MODE_TEST_FORCE_ENABLE		0x05

/* USB request flags */
#define USB_REQUEST_FLAG_SYNC			0x0001
#define USB_REQUEST_FLAG_NO_TIMEOUT		0x0002
#define USB_REQUEST_FLAG_MEMORY_BUFFER		0x0004
#define USB_REQUEST_FLAG_EXT_BUFFER		0x0008

/* USB request completion status */
#define USB_REQUEST_STATUS_PENDING		0
#define USB_REQUEST_STATUS_COMPLETE		1
#define USB_REQUEST_STATUS_ERROR		2
#define USB_REQUEST_STATUS_CANCELLED		3
#define USB_REQUEST_STATUS_TIMEOUT		4

/* USB request structure - extended */
struct usb_request {
	struct usb_device_request	ur_req;
	void				*ur_data;
	uint32_t			ur_length;
	uint32_t			ur_actual;
	uint32_t			ur_timeout;
	uint16_t			ur_flags;
	uint8_t				ur_status;
	int				ur_error;
	void				(*ur_callback)(struct usb_request *);
	void				*ur_priv;
	struct mtx			*ur_mtx;
};

/* USB request queue structure */
struct usb_request_queue {
	struct usb_request	*head;
	struct usb_request	*tail;
	int			count;
	struct mtx		mtx;
};

/* USB request callback type */
typedef void (*usb_request_callback_t)(struct usb_request *req);

/* USB request type helpers - inline functions */
static __inline uint8_t
usb_request_type(uint8_t dir, uint8_t type, uint8_t recip)
{
	return (dir & USB_REQUEST_TYPE_DIRECTION) |
	       (type & USB_REQUEST_TYPE_TYPE) |
	       (recip & USB_REQUEST_TYPE_RECIPIENT);
}

static __inline int
usb_request_is_in(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_DIRECTION) ==
	       USB_REQUEST_TYPE_DIRECTION_IN;
}

static __inline int
usb_request_is_out(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_DIRECTION) ==
	       USB_REQUEST_TYPE_DIRECTION_OUT;
}

static __inline int
usb_request_is_standard(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_TYPE) ==
	       USB_REQUEST_TYPE_TYPE_STANDARD;
}

static __inline int
usb_request_is_class(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_TYPE) ==
	       USB_REQUEST_TYPE_TYPE_CLASS;
}

static __inline int
usb_request_is_vendor(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_TYPE) ==
	       USB_REQUEST_TYPE_TYPE_VENDOR;
}

static __inline int
usb_request_is_device_recipient(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_RECIPIENT) ==
	       USB_REQUEST_TYPE_RECIPIENT_DEVICE;
}

static __inline int
usb_request_is_interface_recipient(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_RECIPIENT) ==
	       USB_REQUEST_TYPE_RECIPIENT_INTERFACE;
}

static __inline int
usb_request_is_endpoint_recipient(uint8_t request_type)
{
	return (request_type & USB_REQUEST_TYPE_RECIPIENT) ==
	       USB_REQUEST_TYPE_RECIPIENT_ENDPOINT;
}

/* USB request initialization - stub */
static __inline void
usb_request_init(struct usb_request *req, uint8_t type, uint8_t request,
    uint16_t value, uint16_t index, uint16_t length, void *data)
{
	if (req == NULL)
		return;
	memset(req, 0, sizeof(*req));
	req->ur_req.bmRequestType = type;
	req->ur_req.bRequest = request;
	req->ur_req.wValue = value;
	req->ur_req.wIndex = index;
	req->ur_req.wLength = length;
	req->ur_data = data;
	req->ur_length = length;
	req->ur_timeout = 5000;  /* Default 5 seconds */
	req->ur_status = USB_REQUEST_STATUS_PENDING;
}

/* USB request cleanup - stub */
static __inline void
usb_request_cleanup(struct usb_request *req)
{
	(void)req;
}

/* USB request set callback - stub */
static __inline void
usb_request_set_callback(struct usb_request *req, usb_request_callback_t cb)
{
	if (req)
		req->ur_callback = cb;
}

/* USB request set timeout - stub */
static __inline void
usb_request_set_timeout(struct usb_request *req, uint32_t timeout)
{
	if (req)
		req->ur_timeout = timeout;
}

/* USB request set flags - stub */
static __inline void
usb_request_set_flags(struct usb_request *req, uint16_t flags)
{
	if (req)
		req->ur_flags = flags;
}

/* USB request execute - stub */
static __inline int
usb_request_execute(struct usb_device *udev, struct usb_request *req)
{
	(void)udev;
	if (req == NULL)
		return USB_ERR_INVAL;
	req->ur_status = USB_REQUEST_STATUS_COMPLETE;
	req->ur_error = USB_ERR_NORMAL_COMPLETION;
	req->ur_actual = req->ur_length;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB request wait - stub */
static __inline int
usb_request_wait(struct usb_request *req, uint32_t timeout)
{
	(void)timeout;
	if (req == NULL)
		return USB_ERR_INVAL;
	if (req->ur_status == USB_REQUEST_STATUS_PENDING)
		req->ur_status = USB_REQUEST_STATUS_COMPLETE;
	return req->ur_error;
}

/* USB request cancel - stub */
static __inline void
usb_request_cancel(struct usb_request *req)
{
	if (req)
		req->ur_status = USB_REQUEST_STATUS_CANCELLED;
}

/* USB request get status - stub */
static __inline int
usb_request_get_status(struct usb_request *req)
{
	if (req == NULL)
		return USB_REQUEST_STATUS_ERROR;
	return req->ur_status;
}

/* USB request is complete - stub */
static __inline int
usb_request_is_complete(struct usb_request *req)
{
	if (req == NULL)
		return 0;
	return req->ur_status == USB_REQUEST_STATUS_COMPLETE;
}

/* USB request get actual length - stub */
static __inline uint32_t
usb_request_get_actual(struct usb_request *req)
{
	if (req == NULL)
		return 0;
	return req->ur_actual;
}

/* USB request queue init - stub */
static __inline void
usb_request_queue_init(struct usb_request_queue *queue)
{
	if (queue) {
		memset(queue, 0, sizeof(*queue));
		mtx_init(&queue->mtx, "usb_reqq", NULL, MTX_DEF);
	}
}

/* USB request queue cleanup - stub */
static __inline void
usb_request_queue_cleanup(struct usb_request_queue *queue)
{
	if (queue)
		mtx_destroy(&queue->mtx);
}

/* USB request enqueue - stub */
static __inline void
usb_request_enqueue(struct usb_request_queue *queue, struct usb_request *req)
{
	if (queue == NULL || req == NULL)
		return;
	mtx_lock(&queue->mtx);
	req->ur_status = USB_REQUEST_STATUS_PENDING;
	if (queue->tail) {
		queue->tail = req;
	} else {
		queue->head = queue->tail = req;
	}
	queue->count++;
	mtx_unlock(&queue->mtx);
}

/* USB request dequeue - stub */
static __inline struct usb_request *
usb_request_dequeue(struct usb_request_queue *queue)
{
	struct usb_request *req;
	if (queue == NULL)
		return NULL;
	mtx_lock(&queue->mtx);
	req = queue->head;
	if (req) {
		queue->head = NULL;  /* Simple implementation */
		queue->count--;
		if (queue->count == 0)
			queue->tail = NULL;
	}
	mtx_unlock(&queue->mtx);
	return req;
}

/* USB control request helpers - stubs */
static __inline int
usb_request_get_status(struct usb_device *udev, uint8_t recipient,
    uint16_t index, uint16_t *status)
{
	(void)udev;
	(void)recipient;
	(void)index;
	if (status)
		*status = 0;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_clear_feature(struct usb_device *udev, uint8_t recipient,
    uint16_t index, uint16_t feature)
{
	(void)udev;
	(void)recipient;
	(void)index;
	(void)feature;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_set_feature(struct usb_device *udev, uint8_t recipient,
    uint16_t index, uint16_t feature)
{
	(void)udev;
	(void)recipient;
	(void)index;
	(void)feature;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_set_address(struct usb_device *udev, uint16_t address)
{
	(void)udev;
	(void)address;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_get_descriptor(struct usb_device *udev, uint8_t type,
    uint8_t index, uint16_t lang, void *buf, uint32_t len, uint32_t *actual)
{
	(void)udev;
	(void)type;
	(void)index;
	(void)lang;
	(void)buf;
	if (actual)
		*actual = 0;
	(void)len;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_set_descriptor(struct usb_device *udev, uint8_t type,
    uint8_t index, uint16_t lang, void *buf, uint32_t len)
{
	(void)udev;
	(void)type;
	(void)index;
	(void)lang;
	(void)buf;
	(void)len;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_get_config(struct usb_device *udev, uint8_t *config)
{
	(void)udev;
	if (config)
		*config = 0;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_set_config(struct usb_device *udev, uint8_t config)
{
	(void)udev;
	(void)config;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_get_interface(struct usb_device *udev, uint16_t iface, uint8_t *alt)
{
	(void)udev;
	(void)iface;
	if (alt)
		*alt = 0;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_set_interface(struct usb_device *udev, uint16_t iface, uint16_t alt)
{
	(void)udev;
	(void)iface;
	(void)alt;
	return USB_ERR_NORMAL_COMPLETION;
}

static __inline int
usb_request_synch_frame(struct usb_device *udev, uint16_t ep, uint16_t *frame)
{
	(void)udev;
	(void)ep;
	if (frame)
		*frame = 0;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB endpoint halt/clear helpers - stubs */
static __inline int
usb_request_clear_halt(struct usb_device *udev, uint8_t ep)
{
	return usb_request_clear_feature(udev,
	    USB_REQUEST_TYPE_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_ENDPOINT,
	    ep, USB_FEATURE_ENDPOINT_HALT);
}

static __inline int
usb_request_set_halt(struct usb_device *udev, uint8_t ep)
{
	return usb_request_set_feature(udev,
	    USB_REQUEST_TYPE_TYPE_STANDARD | USB_REQUEST_TYPE_RECIPIENT_ENDPOINT,
	    ep, USB_FEATURE_ENDPOINT_HALT);
}

#endif /* _USB_REQUEST_H_ */
