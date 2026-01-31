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

#ifndef _DEV_PCI_PCI_IOV_H_
#define _DEV_PCI_PCI_IOV_H_

/* DragonFly PCI I/O Virtualization definitions for LinuxKPI */

#include <sys/types.h>
#include <dev/pci/pcivar.h>

/*
 * PCI I/O Virtualization (IOV) provides support for SR-IOV
 * (Single Root I/O Virtualization) and MR-IOV (Multi-Root I/O Virtualization).
 *
 * SR-IOV allows a single PCIe device to appear as multiple separate
 * physical devices. This is commonly used for network and storage
 * virtualization.
 *
 * This is a minimal stub implementation for LinuxKPI compatibility.
 */

/* SR-IOV capability definitions */
#define PCI_SRIOV_CAP_ID        0x10  /* SR-IOV capability ID */
#define PCI_SRIOV_TOTAL_VFS     0x0E  /* Offset to TotalVFs */
#define PCI_SRIOV_NUM_VFS       0x10  /* Offset to NumVFs */
#define PCI_SRIOV_VF_OFFSET     0x14  /* Offset to VF offset */
#define PCI_SRIOV_VF_STRIDE     0x16  /* Offset to VF stride */
#define PCI_SRIOV_CAP           0x1C  /* Offset to capability */
#define PCI_SRIOV_CTRL          0x18  /* Offset to control */

/* SR-IOV control flags */
#define PCI_SRIOV_CTRL_VFE      0x01  /* VF Enable */
#define PCI_SRIOV_CTRL_VF_MSE   0x02  /* VF Memory Space Enable */
#define PCI_SRIOV_CTRL_ARI      0x04  /* ARI Capable Hierarchy */

/* SR-IOV status flags */
#define PCI_SRIOV_STATUS_VFM    0x01  /* VF Migration Status */

/* Maximum number of VFs supported */
#define PCI_SRIOV_MAX_VF        256
#define PCI_SRIOV_MAX_VF_PER_PF   256

/* SR-IOV capability structure */
struct pci_sriov_cap {
    uint16_t    sr_cap;         /* Capability ID and next pointer */
    uint16_t    sr_cap_flags;   /* Capability flags */
    uint16_t    sr_total_vfs;   /* Total number of VFs */
    uint16_t    sr_num_vfs;     /* Number of enabled VFs */
    uint8_t     sr_func_dep;    /* Function dependency link */
    uint8_t     sr_rsvd1;       /* Reserved */
    uint16_t    sr_vf_offset;   /* First VF device number offset */
    uint16_t    sr_vf_stride;   /* VF device number stride */
    uint16_t    sr_rsvd2;       /* Reserved */
    uint16_t    sr_vf_did;      /* VF device ID */
    uint32_t    sr_sup_pages;   /* Supported page sizes */
    uint32_t    sr_sys_pages;   /* System page size */
    uint32_t    sr_bar[6];      /* VF BARs */
    uint16_t    sr_vf_mig_sta;  /* VF migration state array */
} __packed;

/* SR-IOV PF (Physical Function) structure */
struct pci_iov_pf {
    device_t            pf_dev;         /* PF device */
    struct pci_sriov_cap *pf_cap;       /* SR-IOV capability */
    uint16_t            pf_total_vfs;   /* Total VFs supported */
    uint16_t            pf_num_vfs;     /* Number of VFs configured */
    uint16_t            pf_vf_offset;   /* VF device offset */
    uint16_t            pf_vf_stride;   /* VF stride */
    uint32_t            pf_flags;       /* PF flags */
    void                *pf_softc;      /* Driver softc */
};

/* SR-IOV VF (Virtual Function) structure */
struct pci_iov_vf {
    device_t            vf_dev;         /* VF device */
    device_t            vf_pf;          /* Parent PF device */
    uint16_t            vf_index;       /* VF index (0-based) */
    uint16_t            vf_flags;       /* VF flags */
    void                *vf_softc;      /* Driver softc */
};

/* SR-IOV allocation parameters */
struct pci_iov_alloc {
    uint16_t            ia_num_vfs;     /* Number of VFs to allocate */
    uint16_t            ia_flags;       /* Allocation flags */
    uint64_t            ia_bar_sizes[6]; /* BAR sizes for VFs */
};

/* SR-IOV assignment parameters */
struct pci_iov_assign {
    uint16_t            as_vf_index;    /* VF index */
    uint16_t            as_domain;      /* Target domain */
    uint16_t            as_bus;         /* Target bus */
    uint16_t            as_slot;        /* Target slot */
    uint16_t            as_function;    /* Target function */
};

/* PF/VF flags */
#define PCI_IOV_ENABLED     0x0001  /* IOV is enabled */
#define PCI_IOV_CONFIGURED  0x0002  /* IOV is configured */
#define PCI_IOV_ALLOCATED   0x0004  /* VFs are allocated */
#define PCI_IOV_MIGRATING   0x0008  /* VF is migrating */
#define PCI_IOV_HIDDEN      0x0010  /* VF is hidden */

/*
 * SR-IOV operation functions (stubs)
 */

/* Initialize SR-IOV capability for a PF */
static __inline int
pci_iov_init(device_t dev, struct pci_iov_pf **pf)
{
    (void)dev; (void)pf;
    return 0;  /* Not supported in stub */
}

/* Uninitialize SR-IOV capability */
static __inline void
pci_iov_uninit(device_t dev)
{
    (void)dev;
}

/* Add VFs to a PF */
static __inline int
pci_iov_add_vfs(device_t dev, uint16_t num_vfs)
{
    (void)dev; (void)num_vfs;
    return 0;
}

/* Delete VFs from a PF */
static __inline int
pci_iov_delete_vfs(device_t dev, uint16_t num_vfs)
{
    (void)dev; (void)num_vfs;
    return 0;
}

/* Get the number of VFs */
static __inline int
pci_iov_get_num_vfs(device_t dev, uint16_t *num_vfs)
{
    (void)dev;
    if (num_vfs)
        *num_vfs = 0;
    return 0;
}

/* Get VF device by index */
static __inline device_t
pci_iov_get_vf_device(device_t dev, uint16_t vf_index)
{
    (void)dev; (void)vf_index;
    return NULL;
}

/* Get parent PF of a VF */
static __inline device_t
pci_iov_get_pf(device_t vf_dev)
{
    (void)vf_dev;
    return NULL;
}

/* Get VF index */
static __inline int
pci_iov_get_vf_index(device_t vf_dev, uint16_t *vf_index)
{
    (void)vf_dev;
    if (vf_index)
        *vf_index = 0;
    return 0;
}

/* Check if device is a VF */
static __inline int
pci_iov_is_vf(device_t dev)
{
    (void)dev;
    return 0;  /* Not a VF in stub */
}

/* Check if device is a PF with SR-IOV */
static __inline int
pci_iov_is_pf(device_t dev)
{
    (void)dev;
    return 0;  /* Not a PF in stub */
}

/* Get the SR-IOV capability */
static __inline struct pci_sriov_cap *
pci_iov_get_cap(device_t dev)
{
    (void)dev;
    return NULL;
}

/* Allocate VF resources */
static __inline int
pci_iov_alloc_vfs(device_t dev, struct pci_iov_alloc *alloc)
{
    (void)dev; (void)alloc;
    return 0;
}

/* Free VF resources */
static __inline int
pci_iov_free_vfs(device_t dev)
{
    (void)dev;
    return 0;
}

/* Assign VF to a domain */
static __inline int
pci_iov_assign_vf(device_t dev, struct pci_iov_assign *assign)
{
    (void)dev; (void)assign;
    return 0;
}

/* Unassign VF */
static __inline int
pci_iov_unassign_vf(device_t dev, uint16_t vf_index)
{
    (void)dev; (void)vf_index;
    return 0;
}

/*
 * Legacy compatibility macros
 */
#define pci_find_extcap(dev, cap, offset)   (-1)
#define pci_read_config(dev, reg, width)    0
#define pci_write_config(dev, reg, val, width)  ((void)0)

/*
 * SR-IOV capability read/write helpers
 */
static __inline uint16_t
pci_iov_read_total_vfs(device_t dev)
{
    (void)dev;
    return 0;
}

static __inline uint16_t
pci_iov_read_num_vfs(device_t dev)
{
    (void)dev;
    return 0;
}

static __inline int
pci_iov_write_num_vfs(device_t dev, uint16_t num_vfs)
{
    (void)dev; (void)num_vfs;
    return 0;
}

static __inline uint32_t
pci_iov_read_control(device_t dev)
{
    (void)dev;
    return 0;
}

static __inline int
pci_iov_write_control(device_t dev, uint32_t control)
{
    (void)dev; (void)control;
    return 0;
}

#endif /* _DEV_PCI_PCI_IOV_H_ */
