/*-
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Anthony Liguori  <aliguori@us.ibm.com>
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/virtio/pci/virtio_pci.h,v 1.2 2011/12/06 06:28:32 grehan Exp $
 */

#ifndef _VIRTIO_PCI_H
#define _VIRTIO_PCI_H

/* VirtIO PCI vendor/device ID. */
#define VIRTIO_PCI_VENDORID	0x1AF4
#define VIRTIO_PCI_DEVICEID_MIN	0x1000
#define VIRTIO_PCI_DEVICEID_MAX	0x103F

/* VirtIO ABI version, this must match exactly. */
#define VIRTIO_PCI_ABI_VERSION	0

/*
 * VirtIO Header, located in BAR 0.
 */
#define VIRTIO_PCI_HOST_FEATURES  0  /* host's supported features (32bit, RO)*/
#define VIRTIO_PCI_GUEST_FEATURES 4  /* guest's supported features (32, RW) */
#define VIRTIO_PCI_QUEUE_PFN      8  /* physical address of VQ (32, RW) */
#define VIRTIO_PCI_QUEUE_NUM      12 /* number of ring entries (16, RO) */
#define VIRTIO_PCI_QUEUE_SEL      14 /* current VQ selection (16, RW) */
#define VIRTIO_PCI_QUEUE_NOTIFY	  16 /* notify host regarding VQ (16, RW) */
#define VIRTIO_PCI_STATUS         18 /* device status register (8, RW) */
#define VIRTIO_PCI_ISR            19 /* interrupt status register, reading
				      * also clears the register (8, RO) */
/* Only if MSIX is enabled: */
#define VIRTIO_MSI_CONFIG_VECTOR  20 /* configuration change vector (16, RW) */
#define VIRTIO_MSI_QUEUE_VECTOR   22 /* vector for selected VQ notifications
					(16, RW) */

/* The bit of the ISR which indicates a device has an interrupt. */
#define VIRTIO_PCI_ISR_INTR	0x1
/* The bit of the ISR which indicates a device configuration change. */
#define VIRTIO_PCI_ISR_CONFIG	0x2
/* Vector value used to disable MSI for queue. */
#define VIRTIO_MSI_NO_VECTOR	0xFFFF

/*
 * The remaining space is defined by each driver as the per-driver
 * configuration space.
 */
#define VIRTIO_PCI_CONFIG(sc) \
    (((sc)->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) ? 24 : 20)

/*
 * How many bits to shift physical queue address written to QUEUE_PFN.
 * 12 is historical, and due to x86 page size.
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT	12

/* The alignment to use between consumer and producer parts of vring. */
#define VIRTIO_PCI_VRING_ALIGN	4096

/*
 * Modern (VirtIO 1.0) devices.  Their device ID is 0x1040 plus the VirtIO
 * device type, and they have no legacy I/O header: the registers are in
 * BAR regions that vendor-specific PCI capabilities point to.
 */
#define VIRTIO_PCI_DEVICEID_MODERN_MIN	0x1040
#define VIRTIO_PCI_DEVICEID_MODERN_MAX	0x107F

/* cfg_type of the vendor-specific capabilities. */
#define VIRTIO_PCI_CAP_COMMON_CFG	1	/* common configuration */
#define VIRTIO_PCI_CAP_NOTIFY_CFG	2	/* notifications */
#define VIRTIO_PCI_CAP_ISR_CFG		3	/* ISR status */
#define VIRTIO_PCI_CAP_DEVICE_CFG	4	/* device-specific config */
#define VIRTIO_PCI_CAP_PCI_CFG		5	/* PCI config access */

/* Offsets in the capability (struct virtio_pci_cap in the spec). */
#define VIRTIO_PCI_CAP_VNDR		0
#define VIRTIO_PCI_CAP_NEXT		1
#define VIRTIO_PCI_CAP_LEN		2
#define VIRTIO_PCI_CAP_CFG_TYPE		3
#define VIRTIO_PCI_CAP_BAR		4
#define VIRTIO_PCI_CAP_OFFSET		8
#define VIRTIO_PCI_CAP_LENGTH		12
#define VIRTIO_PCI_NOTIFY_CAP_MULT	16	/* notify_off_multiplier */

/* Offsets in the common configuration (struct virtio_pci_common_cfg). */
#define VIRTIO_PCI_COMMON_DFSELECT	0	/* device_feature_select (32) */
#define VIRTIO_PCI_COMMON_DF		4	/* device_feature (32, RO) */
#define VIRTIO_PCI_COMMON_GFSELECT	8	/* guest_feature_select (32) */
#define VIRTIO_PCI_COMMON_GF		12	/* guest_feature (32) */
#define VIRTIO_PCI_COMMON_MSIX		16	/* msix_config (16) */
#define VIRTIO_PCI_COMMON_NUMQ		18	/* num_queues (16, RO) */
#define VIRTIO_PCI_COMMON_STATUS	20	/* device_status (8) */
#define VIRTIO_PCI_COMMON_CFGGENERATION	21	/* config_generation (8, RO) */
#define VIRTIO_PCI_COMMON_Q_SELECT	22	/* queue_select (16) */
#define VIRTIO_PCI_COMMON_Q_SIZE	24	/* queue_size (16) */
#define VIRTIO_PCI_COMMON_Q_MSIX	26	/* queue_msix_vector (16) */
#define VIRTIO_PCI_COMMON_Q_ENABLE	28	/* queue_enable (16) */
#define VIRTIO_PCI_COMMON_Q_NOFF	30	/* queue_notify_off (16, RO) */
#define VIRTIO_PCI_COMMON_Q_DESCLO	32	/* queue_desc (64) */
#define VIRTIO_PCI_COMMON_Q_DESCHI	36
#define VIRTIO_PCI_COMMON_Q_AVAILLO	40	/* queue_driver (64) */
#define VIRTIO_PCI_COMMON_Q_AVAILHI	44
#define VIRTIO_PCI_COMMON_Q_USEDLO	48	/* queue_device (64) */
#define VIRTIO_PCI_COMMON_Q_USEDHI	52
#define VIRTIO_PCI_COMMON_CFG_SIZE	56

#endif /* _VIRTIO_PCI_H */
