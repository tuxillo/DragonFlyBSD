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

#ifndef _USB_TRANSFER_H_
#define _USB_TRANSFER_H_

/* DragonFly USB transfer definitions for LinuxKPI */

#include <sys/types.h>
#include <dev/usb/usb.h>

/* USB transfer states */
#define USB_TRANSFER_STATE_IDLE			0
#define USB_TRANSFER_STATE_SETUP		1
#define USB_TRANSFER_STATE_TRANSFERRING		2
#define USB_TRANSFER_STATE_TRANSFERRED		3
#define USB_TRANSFER_STATE_COMPLETE		4
#define USB_TRANSFER_STATE_ERROR		5
#define USB_TRANSFER_STATE_CANCELLED		6

/* USB transfer flags */
#define USB_TRANSFER_FLAG_READ			0x0001
#define USB_TRANSFER_FLAG_WRITE			0x0002
#define USB_TRANSFER_FLAG_SHORT_OK		0x0004
#define USB_TRANSFER_FLAG_ALLOW_ZERO		0x0008
#define USB_TRANSFER_FLAG_FORCE_SHORT		0x0010
#define USB_TRANSFER_FLAG_DELAY_STATUS		0x0020
#define USB_TRANSFER_FLAG_KEEPALIVE		0x0040

/* USB transfer timeout default (ms) */
#define USB_TRANSFER_TIMEOUT_DEFAULT		5000
#define USB_TRANSFER_TIMEOUT_SHORT		1000
#define USB_TRANSFER_TIMEOUT_LONG		10000
#define USB_TRANSFER_TIMEOUT_BULK		0

/* USB frame count limits */
#define USB_MAX_FRAME_COUNT			128
#define USB_MAX_FRAME_SIZE			65535

/* USB transfer structure - minimal stub */
struct usb_xfer {
	struct usb_device	*xfer_udev;
	struct usb_endpoint	*xfer_endpoint;
	void			*xfer_buffer;
	uint32_t		xfer_length;
	uint32_t		xfer_actual;
	uint32_t		xfer_timeout;
	uint16_t		xfer_flags;
	uint8_t			xfer_state;
	uint8_t			xfer_status;
	uint8_t			xfer_nframes;
	void			(*xfer_callback)(struct usb_xfer *);
	void			*xfer_priv;
	struct mtx		xfer_mtx;
};

/* USB transfer queue structure */
struct usb_xfer_queue {
	struct usb_xfer		*head;
	struct usb_xfer		*tail;
	int			count;
	struct mtx		mtx;
};

/* USB transfer result structure */
struct usb_xfer_result {
	uint32_t	actual;
	int		status;
};

/* USB transfer callback type */
typedef void (*usb_xfer_callback_t)(struct usb_xfer *xfer);

/* USB transfer initialization - stub */
static __inline int
usb_transfer_init(struct usb_xfer *xfer, struct usb_device *udev,
    struct usb_endpoint *ep, uint8_t nframes)
{
	memset(xfer, 0, sizeof(*xfer));
	xfer->xfer_udev = udev;
	xfer->xfer_endpoint = ep;
	xfer->xfer_nframes = nframes;
	xfer->xfer_state = USB_TRANSFER_STATE_IDLE;
	xfer->xfer_timeout = USB_TRANSFER_TIMEOUT_DEFAULT;
	mtx_init(&xfer->xfer_mtx, "usb_xfer", NULL, MTX_DEF);
	return 0;
}

/* USB transfer cleanup - stub */
static __inline void
usb_transfer_uninit(struct usb_xfer *xfer)
{
	if (xfer)
		mtx_destroy(&xfer->xfer_mtx);
}

/* USB transfer start - stub */
static __inline int
usb_transfer_start(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return USB_ERR_INVAL;
	xfer->xfer_state = USB_TRANSFER_STATE_TRANSFERRING;
	return 0;
}

/* USB transfer stop - stub */
static __inline void
usb_transfer_stop(struct usb_xfer *xfer)
{
	if (xfer)
		xfer->xfer_state = USB_TRANSFER_STATE_CANCELLED;
}

/* USB transfer wait - stub */
static __inline int
usb_transfer_wait(struct usb_xfer *xfer, uint32_t timeout)
{
	(void)timeout;
	if (xfer == NULL)
		return USB_ERR_INVAL;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB transfer status - stub */
static __inline int
usb_transfer_get_status(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return USB_ERR_INVAL;
	return xfer->xfer_status;
}

/* USB transfer state - stub */
static __inline int
usb_transfer_get_state(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return USB_TRANSFER_STATE_ERROR;
	return xfer->xfer_state;
}

/* USB transfer is complete - stub */
static __inline int
usb_transfer_is_complete(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return 0;
	return xfer->xfer_state == USB_TRANSFER_STATE_COMPLETE;

}

/* USB transfer set buffer - stub */
static __inline void
usb_transfer_set_buffer(struct usb_xfer *xfer, void *buf, uint32_t len)
{
	if (xfer) {
		xfer->xfer_buffer = buf;
		xfer->xfer_length = len;
	}
}

/* USB transfer set timeout - stub */
static __inline void
usb_transfer_set_timeout(struct usb_xfer *xfer, uint32_t timeout)
{
	if (xfer)
		xfer->xfer_timeout = timeout;
}

/* USB transfer set flags - stub */
static __inline void
usb_transfer_set_flags(struct usb_xfer *xfer, uint16_t flags)
{
	if (xfer)
		xfer->xfer_flags = flags;
}

/* USB transfer set callback - stub */
static __inline void
usb_transfer_set_callback(struct usb_xfer *xfer, usb_xfer_callback_t cb)
{
	if (xfer)
		xfer->xfer_callback = cb;
}

/* USB transfer get actual length - stub */
static __inline uint32_t
usb_transfer_get_actual(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return 0;
	return xfer->xfer_actual;
}

/* USB transfer queue init - stub */
static __inline void
usb_xfer_queue_init(struct usb_xfer_queue *queue)
{
	memset(queue, 0, sizeof(*queue));
	mtx_init(&queue->mtx, "usb_xferq", NULL, MTX_DEF);
}

/* USB transfer queue cleanup - stub */
static __inline void
usb_xfer_queue_uninit(struct usb_xfer_queue *queue)
{
	if (queue)
		mtx_destroy(&queue->mtx);
}

/* USB transfer enqueue - stub */
static __inline void
usb_xfer_enqueue(struct usb_xfer_queue *queue, struct usb_xfer *xfer)
{
	if (queue == NULL || xfer == NULL)
		return;
	mtx_lock(&queue->mtx);
	xfer->xfer_state = USB_TRANSFER_STATE_SETUP;
	if (queue->tail) {
		queue->tail = xfer;
	} else {
		queue->head = queue->tail = xfer;
	}
	queue->count++;
	mtx_unlock(&queue->mtx);
}

/* USB transfer dequeue - stub */
static __inline struct usb_xfer *
usb_xfer_dequeue(struct usb_xfer_queue *queue)
{
	struct usb_xfer *xfer;
	if (queue == NULL)
		return NULL;
	mtx_lock(&queue->mtx);
	xfer = queue->head;
	if (xfer) {
		queue->head = NULL;  /* Simple implementation */
		queue->count--;
		if (queue->count == 0)
			queue->tail = NULL;
	}
	mtx_unlock(&queue->mtx);
	return xfer;
}

/* USB transfer queue is empty - stub */
static __inline int
usb_xfer_queue_is_empty(struct usb_xfer_queue *queue)
{
	int empty;
	if (queue == NULL)
		return 1;
	mtx_lock(&queue->mtx);
	empty = (queue->count == 0);
	mtx_unlock(&queue->mtx);
	return empty;
}

/* USB control transfer - stub */
static __inline int
usb_control_transfer(struct usb_device *udev, struct mtx *mtx,
    struct usb_device_request *req, void *data, uint32_t len,
    uint32_t timeout, uint32_t flags)
{
	(void)udev;
	(void)mtx;
	(void)req;
	(void)data;
	(void)len;
	(void)timeout;
	(void)flags;
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB bulk transfer - stub */
static __inline int
usb_bulk_transfer(struct usb_xfer *xfer, void *buf, uint32_t len,
    uint32_t *actual, uint32_t timeout)
{
	(void)buf;
	(void)len;
	if (actual)
		*actual = 0;
	(void)timeout;
	if (xfer) {
		xfer->xfer_state = USB_TRANSFER_STATE_COMPLETE;
		xfer->xfer_status = USB_ERR_NORMAL_COMPLETION;
		xfer->xfer_actual = 0;
	}
	return USB_ERR_NORMAL_COMPLETION;
}

/* USB interrupt transfer - stub */
static __inline int
usb_interrupt_transfer(struct usb_xfer *xfer, void *buf, uint32_t len,
    uint32_t *actual, uint32_t timeout)
{
	return usb_bulk_transfer(xfer, buf, len, actual, timeout);
}

/* USB isochronous transfer - stub */
static __inline int
usb_isochronous_transfer(struct usb_xfer *xfer, void *buf, uint32_t len,
    uint32_t *actual)
{
	return usb_bulk_transfer(xfer, buf, len, actual, 0);
}

/* USB transfer abort - stub */
static __inline void
usb_transfer_abort(struct usb_xfer *xfer)
{
	if (xfer)
		xfer->xfer_state = USB_TRANSFER_STATE_CANCELLED;
}

/* USB transfer drain - stub */
static __inline void
usb_transfer_drain(struct usb_xfer *xfer)
{
	if (xfer && xfer->xfer_state == USB_TRANSFER_STATE_TRANSFERRING)
		xfer->xfer_state = USB_TRANSFER_STATE_COMPLETE;
}

/* USB transfer clear stall - stub */
static __inline int
usb_transfer_clear_stall(struct usb_xfer *xfer)
{
	if (xfer == NULL)
		return USB_ERR_INVAL;
	xfer->xfer_status = USB_ERR_NORMAL_COMPLETION;
	return USB_ERR_NORMAL_COMPLETION;
}

#endif /* _USB_TRANSFER_H_ */
