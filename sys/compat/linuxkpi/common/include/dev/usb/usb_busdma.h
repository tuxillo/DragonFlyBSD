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

#ifndef _USB_BUSDMA_H_
#define _USB_BUSDMA_H_

/*
 * DragonFly USB busdma compatibility for LinuxKPI
 * 
 * DragonFly uses bus/u4b/ for USB instead of dev/usb/
 * This header provides compatibility by redirecting to DragonFly's location.
 */

/* Include the DragonFly USB busdma header from its actual location */
#include <bus/u4b/usb_busdma.h>

/* If the above include doesn't exist, provide minimal stubs */
#ifndef _BUS_U4B_USB_BUSDMA_H_

#include <sys/types.h>
#include <sys/bus_dma.h>

/* USB bus DMA tag - simplified */
struct usb_dma_tag {
    bus_dma_tag_t bus_tag;
    void *cookie;
};

/* USB DMA memory block */
struct usb_dma_block {
    bus_dmamap_t dmamap;
    bus_dma_segment_t segs[1];
    int nsegs;
    caddr_t kaddr;
    bus_size_t size;
    uint8_t aligned;
};

/* USB DMA callback - stub */
typedef void (usb_dma_callback_t)(void *arg, bus_dma_segment_t *segs, int nsegs, int error);

/* USB busdma operations - stubs */
static __inline int
usb_dma_tag_create(struct usb_dma_tag *utag, bus_size_t alignment,
                   bus_size_t boundary, bus_addr_t lowaddr,
                   bus_addr_t highaddr, bus_dma_filter_t *filter,
                   void *filterarg, bus_size_t maxsize, int nsegments,
                   bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
                   void *lockarg, struct usb_dma_tag **out_tag)
{
    return -1;  /* Stub */
}

static __inline void
usb_dma_tag_destroy(struct usb_dma_tag *utag)
{
    /* Stub */
}

static __inline int
usb_dma_memory_alloc(struct usb_dma_tag *utag, bus_size_t size,
                     int flags, struct usb_dma_block *block)
{
    return -1;  /* Stub */
}

static __inline void
usb_dma_memory_free(struct usb_dma_block *block)
{
    /* Stub */
}

static __inline void
usb_dma_memory_sync(struct usb_dma_block *block, bus_dmasync_op_t op)
{
    /* Stub */
}

static __inline bus_addr_t
usb_dma_get_physical(struct usb_dma_block *block)
{
    return 0;  /* Stub */
}

static __inline caddr_t
usb_dma_get_virtual(struct usb_dma_block *block)
{
    return block ? block->kaddr : NULL;
}

#endif /* _BUS_U4B_USB_BUSDMA_H_ */

#endif /* _USB_BUSDMA_H_ */
