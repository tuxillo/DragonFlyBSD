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

#ifndef _DEV_PCI_PCIREG_H_
#define _DEV_PCI_PCIREG_H_

/* DragonFly PCI register definitions for LinuxKPI */

#include <sys/types.h>

/*
 * PCI configuration space register offsets and masks.
 * These definitions are used by LinuxKPI PCI compatibility layer.
 *
 * This is a minimal stub that provides common PCI register definitions
 * needed by DRM drivers and other LinuxKPI consumers.
 */

/* PCI standard register offsets */
#define PCIR_VENDOR		0x00
#define PCIR_DEVICE		0x02
#define PCIR_COMMAND		0x04
#define PCIR_STATUS		0x06
#define PCIR_REVID		0x08
#define PCIR_PROGIF		0x09
#define PCIR_SUBCLASS		0x0a
#define PCIR_CLASS		0x0b
#define PCIR_CACHELNSZ		0x0c
#define PCIR_LATTIMER		0x0d
#define PCIR_HEADERTYPE		0x0e
#define PCIR_BIST		0x0f
#define PCIR_BAR_0		0x10
#define PCIR_BAR_1		0x14
#define PCIR_BAR_2		0x18
#define PCIR_BAR_3		0x1c
#define PCIR_BAR_4		0x20
#define PCIR_BAR_5		0x24
#define PCIR_CARDBUSCIS		0x28
#define PCIR_SUBVEND_0		0x2c
#define PCIR_SUBDEV_0		0x2e
#define PCIR_ROMBAR_0		0x30
#define PCIR_CAP_PTR		0x34
#define PCIR_INTLINE		0x3c
#define PCIR_INTPIN		0x3d
#define PCIR_MINGNT		0x3e
#define PCIR_MAXLAT		0x3f

/* PCI-to-PCI bridge registers */
#define PCIR_PRIBUS_1		0x18
#define PCIR_SECBUS_1		0x19
#define PCIR_SUBBUS_1		0x1a
#define PCIR_SECLAT_1		0x1b
#define PCIR_IOBASEL_1		0x1c
#define PCIR_IOLIMITL_1		0x1d
#define PCIR_IOBASEH_1		0x30
#define PCIR_IOLIMITH_1		0x32
#define PCIR_MEMBASE_1		0x20
#define PCIR_MEMLIMIT_1		0x22
#define PCIR_PMBASEL_1		0x24
#define PCIR_PMLIMITL_1		0x26
#define PCIR_PMBASEH_1		0x28
#define PCIR_PMLIMITH_1		0x2c

/* Command register bits */
#define PCIM_CMD_PORTEN		0x0001
#define PCIM_CMD_MEMEN		0x0002
#define PCIM_CMD_BUSMASTEREN	0x0004
#define PCIM_CMD_SPECIALEN	0x0008
#define PCIM_CMD_MWRICEN	0x0010
#define PCIM_CMD_PERRESPEN	0x0040
#define PCIM_CMD_SERRESPEN	0x0100
#define PCIM_CMD_BACKTOBACK	0x0200
#define PCIM_CMD_INTxDIS	0x0400

/* Status register bits */
#define PCIM_STATUS_INTxSTATE	0x0008
#define PCIM_STATUS_CAPPRESENT	0x0010
#define PCIM_STATUS_66CAPABLE	0x0020
#define PCIM_STATUS_BACKTOBACK	0x0080
#define PCIM_STATUS_PERRREPORT	0x0100
#define PCIM_STATUS_SEL_FAST	0x0000
#define PCIM_STATUS_SEL_MEDIMUM	0x0200
#define PCIM_STATUS_SEL_SLOW	0x0400
#define PCIM_STATUS_SEL_MASK	0x0600
#define PCIM_STATUS_STABORT	0x0800
#define PCIM_STATUS_RTABORT	0x1000
#define PCIM_STATUS_RMABORT	0x2000
#define PCIM_STATUS_SERR	0x4000
#define PCIM_STATUS_PERR	0x8000

/* Header type register bits */
#define PCIM_HDRTYPE		0x7f
#define PCIM_HDRTYPE_NORMAL	0x00
#define PCIM_HDRTYPE_BRIDGE	0x01
#define PCIM_HDRTYPE_CARDBUS	0x02
#define PCIM_MFDEV		0x80

/* BAR-related macros */
#define PCIR_BARS		PCIR_BAR_0
#define PCIR_MAX_BAR_0		5
#define PCI_MAPREG_START	PCIR_BAR_0
#define PCI_MAPREG_END		0x28
#define PCI_MAPREG_TYPE		0x00000001
#define PCI_MAPREG_TYPE_MEM	0x00000000
#define PCI_MAPREG_TYPE_IO	0x00000001
#define PCI_MAPREG_MEM_TYPE	0x00000006
#define PCI_MAPREG_MEM_TYPE_32BIT	0x00000000
#define PCI_MAPREG_MEM_TYPE_32BIT_1M	0x00000002
#define PCI_MAPREG_MEM_TYPE_64BIT	0x00000004
#define PCI_MAPREG_MEM_PREFETCHABLE	0x00000008
#define PCI_MAPREG_MEM_ADDR_MASK	0xfffffff0
#define PCI_MAPREG_IO_ADDR_MASK		0xfffffffc

/* PCI capabilities */
#define PCIY_PMG		0x01
#define PCIY_AGP		0x02
#define PCIY_VPD		0x03
#define PCIY_SLOTID		0x04
#define PCIY_MSI		0x05
#define PCIY_CHSWP		0x06
#define PCIY_PCIX		0x07
#define PCIY_HT			0x08
#define PCIY_VENDOR		0x09
#define PCIY_DEBUG		0x0a
#define PCIY_CRES		0x0b
#define PCIY_HOTPLUG		0x0c
#define PCIY_AGP8X		0x0e
#define PCIY_SECDEV		0x0f
#define PCIY_EXP		0x10
#define PCIY_MSIX		0x11
#define PCIY_SATA		0x12
#define PCIY_PCIFORCE		0x13
#define PCIY_PCIEXP		PCIY_EXP

/* MSI capability */
#define PCIR_MSI_CTRL		0x02
#define PCIR_MSI_ADDR		0x04
#define PCIR_MSI_ADDR_HIGH	0x08
#define PCIR_MSI_DATA		0x08
#define PCIR_MSI_DATA_64BIT	0x0c
#define PCIM_MSICTRL_ENABLE	0x0001
#define PCIM_MSICTRL_64BIT	0x0080
#define PCIM_MSICTRL_VECMASK	0x0100

/* MSI-X capability */
#define PCIR_MSIX_CTRL		0x02
#define PCIR_MSIX_TABLE		0x04
#define PCIR_MSIX_PBA		0x08
#define PCIM_MSIXCTRL_ENABLE	0x8000
#define PCIM_MSIXCTRL_FUNCMASK	0x4000

/* Power Management capability */
#define PCIR_PM_CTRL		0x04
#define PCIR_PM_DATA		0x07
#define PCIM_PMCTRL_STATE_MASK	0x0003
#define PCIM_PMCTRL_STATE_D0	0x0000
#define PCIM_PMCTRL_STATE_D1	0x0001
#define PCIM_PMCTRL_STATE_D2	0x0002
#define PCIM_PMCTRL_STATE_D3	0x0003
#define PCIM_PMCTRL_PME_ENABLE	0x0100
#define PCIM_PMCTRL_PME_STATUS	0x8000

/* PCI Express capability */
#define PCIR_EXPRESS_FLAGS	0x02
#define PCIR_EXPRESS_DEVICE_CAPS	0x04
#define PCIR_EXPRESS_DEVICE_CTL		0x08
#define PCIR_EXPRESS_DEVICE_STA		0x0a
#define PCIR_EXPRESS_LINK_CAPS		0x0c
#define PCIR_EXPRESS_LINK_CTL		0x10
#define PCIR_EXPRESS_LINK_STA		0x12
#define PCIR_EXPRESS_SLOT_CAPS		0x14
#define PCIR_EXPRESS_SLOT_CTL		0x18
#define PCIR_EXPRESS_SLOT_STA		0x1a
#define PCIR_EXPRESS_ROOT_CTL		0x1c
#define PCIR_EXPRESS_ROOT_STA		0x20

/* AER (Advanced Error Reporting) capability offsets */
#define PCIR_AER_UC_STATUS	0x04
#define PCIR_AER_UC_MASK	0x08
#define PCIR_AER_UC_SEVERITY	0x0c
#define PCIR_AER_COR_STATUS	0x10
#define PCIR_AER_COR_MASK	0x14
#define PCIR_AER_CAP_CONTROL	0x18
#define PCIR_AER_HEADER_LOG	0x1c
#define PCIR_AER_ROOTERR_CMD	0x2c
#define PCIR_AER_ROOTERR_STA	0x30
#define PCIR_AER_COR_SOURCE_ID	0x34
#define PCIR_AER_TLP_PREFIX_LOG	0x38

/* PCI error registers (for AER) */
#define PCI_ERR_UNCOR_STATUS	PCIR_AER_UC_STATUS
#define PCI_ERR_COR_STATUS	PCIR_AER_COR_STATUS
#define PCI_ERR_ROOT_COMMAND	PCIR_AER_ROOTERR_CMD
#define PCI_ERR_ROOT_ERR_SRC	PCIR_AER_COR_SOURCE_ID

/* PCI constants */
#define PCI_BUSMAX		255
#define PCI_SLOTMAX		31
#define PCI_FUNCMAX		7
#define PCI_REGMAX		255
#define PCIE_REGMAX		4095

/* Common PCI vendor IDs */
#define PCIV_INVALID		0xffff

#endif /* _DEV_PCI_PCIREG_H_ */
