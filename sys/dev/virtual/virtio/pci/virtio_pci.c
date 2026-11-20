/*-
 * Copyright (c) 2011, Bryan Venteicher <bryanv@daemoninthecloset.org>
 * Copyright (c) 2017, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * The support for modern (VirtIO 1.0) devices is derived from FreeBSD's
 * sys/dev/virtio/pci/virtio_pci_modern.c.
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
 *
 * $FreeBSD: src/sys/dev/virtio/pci/virtio_pci.c,v 1.3 2012/04/14 05:48:04 grehan Exp $
 */

/* Driver for the VirtIO PCI interface. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/serialize.h>

#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>

#include <sys/rman.h>

#include <dev/virtual/virtio/virtio/virtio.h>
#include <dev/virtual/virtio/virtio/virtqueue.h>
#include "virtio_pci.h"
#include "virtio_bus_if.h"

struct vqentry {
	int what;
	struct virtqueue *vq;
	driver_intr_t *handler;
	void *arg;
	TAILQ_ENTRY(vqentry) entries;
};

TAILQ_HEAD(vqirq_list, vqentry);

#define VTPCI_MAX_BARS			6

/* A register block of a modern device, inside one of its BARs. */
struct vtpci_region {
	struct resource			*res;
	bus_size_t			 off;
	bus_size_t			 len;
};

struct vtpci_softc {
	device_t			 vtpci_dev;
	struct resource			*vtpci_res;
	struct resource			*vtpci_msix_res;
	uint64_t			 vtpci_features;
	uint32_t			 vtpci_flags;
#define VIRTIO_PCI_FLAG_MSI		 0x0001
#define VIRTIO_PCI_FLAG_MSIX		 0x0010
#define VIRTIO_PCI_FLAG_MODERN		 0x0100
	int				 vtpci_devtype;

	/*
	 * Modern devices have no legacy I/O header.  Their registers are
	 * found through vendor-specific PCI capabilities, each naming a BAR
	 * and a range in it; several usually share one BAR.
	 */
	struct resource			*vtpci_bar_res[VTPCI_MAX_BARS];
	int				 vtpci_bar_type[VTPCI_MAX_BARS];
	struct vtpci_region		 vtpci_common;
	struct vtpci_region		 vtpci_notify;
	struct vtpci_region		 vtpci_isr;
	struct vtpci_region		 vtpci_device;
	uint32_t			 vtpci_notify_mult;
	bus_size_t			 vtpci_notify_off[VIRTIO_MAX_VIRTQUEUES];

	device_t			 vtpci_child_dev;
	struct virtio_feature_desc	*vtpci_child_feat_desc;

	/*
	 * Ideally, each virtqueue that the driver provides a callback for
	 * will receive its own MSIX vector. If there are not sufficient
	 * vectors available, we will then attempt to have all the VQs
	 * share one vector. Note that when using MSIX, the configuration
	 * changed notifications must be on their own vector.
	 *
	 * If MSIX is not available, we will attempt to have the whole
	 * device share one MSI vector, and then, finally, one legacy
	 * interrupt.
	 */
	int				 vtpci_nvqs;
	struct vtpci_virtqueue {
		struct virtqueue *vq;

		/* Index into vtpci_intr_res[] below. -1 if no IRQ assigned. */
		int		  ires_idx;
	} vtpci_vqx[VIRTIO_MAX_VIRTQUEUES];

	/*
	 * When using MSIX interrupts, the first element of vtpci_intr_res[]
	 * is always the configuration changed notifications. The remaining
	 * element(s) are used for the virtqueues.
	 *
	 * With MSI and legacy interrupts, only the first element of
	 * vtpci_intr_res[] is used.
	 */
	int				 vtpci_nintr_res;
	int				 vtpci_irq_flags;
	struct vtpci_intr_resource {
		struct vtpci_softc *ires_sc;
		struct resource	*irq;
		int		 rid;
		void		*intrhand;
		struct vqirq_list ls;
	} vtpci_intr_res[1 + VIRTIO_MAX_VIRTQUEUES];

	int				 vtpci_config_irq;
};

static int	vtpci_probe(device_t);
static int	vtpci_attach(device_t);
static int	vtpci_detach(device_t);
static int	vtpci_suspend(device_t);
static int	vtpci_resume(device_t);
static int	vtpci_shutdown(device_t);
static void	vtpci_driver_added(device_t, driver_t *);
static void	vtpci_child_detached(device_t, device_t);
static int	vtpci_read_ivar(device_t, device_t, int, uintptr_t *);
static int	vtpci_write_ivar(device_t, device_t, int, uintptr_t);

static uint64_t	vtpci_negotiate_features(device_t, uint64_t);
static int	vtpci_with_feature(device_t, uint64_t);
static int	vtpci_intr_count(device_t dev);
static int	vtpci_intr_alloc(device_t dev, int *cnt, int use_config,
		    int *cpus);
static int	vtpci_intr_release(device_t dev);
static int	vtpci_alloc_virtqueues(device_t, int, struct vq_alloc_info *);
static int	vtpci_setup_intr(device_t, uint irq, lwkt_serialize_t);
static int	vtpci_teardown_intr(device_t, uint irq);
static int	vtpci_bind_intr(device_t, uint, int, driver_intr_t, void *);
static int	vtpci_unbind_intr(device_t, int);
static void	vtpci_stop(device_t);
static int	vtpci_reinit(device_t, uint64_t);
static void	vtpci_reinit_complete(device_t);
static void	vtpci_notify_virtqueue(device_t, uint16_t);
static uint8_t	vtpci_get_status(device_t);
static void	vtpci_set_status(device_t, uint8_t);
static void	vtpci_read_dev_config(device_t, bus_size_t, void *, int);
static void	vtpci_write_dev_config(device_t, bus_size_t, void *, int);

static void	vtpci_describe_features(struct vtpci_softc *, const char *,
		    uint64_t);
static void	vtpci_probe_and_attach_child(struct vtpci_softc *);

static int	vtpci_register_msix_vector(struct vtpci_softc *, int, int);

static struct resource *vtpci_modern_bar(struct vtpci_softc *, int);
static int	vtpci_modern_map_region(struct vtpci_softc *, uint8_t,
		    struct vtpci_region *, bus_size_t, int *);
static int	vtpci_modern_map(struct vtpci_softc *);
static int	vtpci_modern_features_ok(struct vtpci_softc *);
static void	vtpci_select_queue(struct vtpci_softc *, int);
static uint16_t	vtpci_get_queue_size(struct vtpci_softc *);
static void	vtpci_set_queue(struct vtpci_softc *, int, struct virtqueue *);

static void	vtpci_free_interrupts(struct vtpci_softc *);
static void	vtpci_free_virtqueues(struct vtpci_softc *);
static void	vtpci_release_child_resources(struct vtpci_softc *);
static void	vtpci_reset(struct vtpci_softc *);

static void	vtpci_legacy_intr(void *);
static void	vtpci_msix_intr(void *);

/*
 * I/O port read/write wrappers.
 */
#define vtpci_read_config_1(sc, o)	bus_read_1((sc)->vtpci_res, (o))
#define vtpci_read_config_2(sc, o)	bus_read_2((sc)->vtpci_res, (o))
#define vtpci_read_config_4(sc, o)	bus_read_4((sc)->vtpci_res, (o))
#define vtpci_write_config_1(sc, o, v)	bus_write_1((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_2(sc, o, v)	bus_write_2((sc)->vtpci_res, (o), (v))
#define vtpci_write_config_4(sc, o, v)	bus_write_4((sc)->vtpci_res, (o), (v))

/*
 * Register block accessors for modern devices.
 */
#define vtpci_region_read_1(r, o)	bus_read_1((r)->res, (r)->off + (o))
#define vtpci_region_read_2(r, o)	bus_read_2((r)->res, (r)->off + (o))
#define vtpci_region_read_4(r, o)	bus_read_4((r)->res, (r)->off + (o))
#define vtpci_region_write_1(r, o, v)	bus_write_1((r)->res, (r)->off + (o), (v))
#define vtpci_region_write_2(r, o, v)	bus_write_2((r)->res, (r)->off + (o), (v))
#define vtpci_region_write_4(r, o, v)	bus_write_4((r)->res, (r)->off + (o), (v))

#define vtpci_common_read_1(sc, o)	vtpci_region_read_1(&(sc)->vtpci_common, (o))
#define vtpci_common_read_2(sc, o)	vtpci_region_read_2(&(sc)->vtpci_common, (o))
#define vtpci_common_read_4(sc, o)	vtpci_region_read_4(&(sc)->vtpci_common, (o))
#define vtpci_common_write_1(sc, o, v)	vtpci_region_write_1(&(sc)->vtpci_common, (o), (v))
#define vtpci_common_write_2(sc, o, v)	vtpci_region_write_2(&(sc)->vtpci_common, (o), (v))
#define vtpci_common_write_4(sc, o, v)	vtpci_region_write_4(&(sc)->vtpci_common, (o), (v))

#define vtpci_is_modern(sc)	(((sc)->vtpci_flags & VIRTIO_PCI_FLAG_MODERN) != 0)

/* Tunables. */
static int vtpci_disable_msix = 0;
TUNABLE_INT("hw.virtio.pci.disable_msix", &vtpci_disable_msix);

static device_method_t vtpci_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			  vtpci_probe),
	DEVMETHOD(device_attach,		  vtpci_attach),
	DEVMETHOD(device_detach,		  vtpci_detach),
	DEVMETHOD(device_suspend,		  vtpci_suspend),
	DEVMETHOD(device_resume,		  vtpci_resume),
	DEVMETHOD(device_shutdown,		  vtpci_shutdown),

	/* Bus interface. */
	DEVMETHOD(bus_driver_added,		  vtpci_driver_added),
	DEVMETHOD(bus_child_detached,		  vtpci_child_detached),
	DEVMETHOD(bus_read_ivar,		  vtpci_read_ivar),
	DEVMETHOD(bus_write_ivar,		  vtpci_write_ivar),

	/* VirtIO bus interface. */
	DEVMETHOD(virtio_bus_negotiate_features,  vtpci_negotiate_features),
	DEVMETHOD(virtio_bus_with_feature,	  vtpci_with_feature),
	DEVMETHOD(virtio_bus_intr_count,	  vtpci_intr_count),
	DEVMETHOD(virtio_bus_intr_alloc,	  vtpci_intr_alloc),
	DEVMETHOD(virtio_bus_intr_release,	  vtpci_intr_release),
	DEVMETHOD(virtio_bus_alloc_virtqueues,	  vtpci_alloc_virtqueues),
	DEVMETHOD(virtio_bus_setup_intr,	  vtpci_setup_intr),
	DEVMETHOD(virtio_bus_teardown_intr,	  vtpci_teardown_intr),
	DEVMETHOD(virtio_bus_bind_intr,		  vtpci_bind_intr),
	DEVMETHOD(virtio_bus_unbind_intr,	  vtpci_unbind_intr),
	DEVMETHOD(virtio_bus_stop,		  vtpci_stop),
	DEVMETHOD(virtio_bus_reinit,		  vtpci_reinit),
	DEVMETHOD(virtio_bus_reinit_complete,	  vtpci_reinit_complete),
	DEVMETHOD(virtio_bus_notify_vq,		  vtpci_notify_virtqueue),
	DEVMETHOD(virtio_bus_read_device_config,  vtpci_read_dev_config),
	DEVMETHOD(virtio_bus_write_device_config, vtpci_write_dev_config),

	DEVMETHOD_END
};

static driver_t vtpci_driver = {
	"virtio_pci",
	vtpci_methods,
	sizeof(struct vtpci_softc)
};

devclass_t vtpci_devclass;

DRIVER_MODULE(virtio_pci, pci, vtpci_driver, vtpci_devclass, NULL, NULL);
MODULE_VERSION(virtio_pci, 1);
MODULE_DEPEND(virtio_pci, pci, 1, 1, 1);
MODULE_DEPEND(virtio_pci, virtio, 1, 1, 1);

static int
vtpci_probe(device_t dev)
{
	char desc[36];
	const char *name;
	uint16_t devid;
	int type;

	if (pci_get_vendor(dev) != VIRTIO_PCI_VENDORID)
		return (ENXIO);

	/*
	 * Legacy and transitional devices are driven through the legacy
	 * interface.  Modern-only devices (0x1040 plus the device type)
	 * have no legacy interface at all.
	 */
	devid = pci_get_device(dev);
	if (devid >= VIRTIO_PCI_DEVICEID_MIN &&
	    devid <= VIRTIO_PCI_DEVICEID_MAX) {
		if (pci_get_revid(dev) != VIRTIO_PCI_ABI_VERSION)
			return (ENXIO);
		type = pci_get_subdevice(dev);
	} else if (devid >= VIRTIO_PCI_DEVICEID_MODERN_MIN &&
	    devid <= VIRTIO_PCI_DEVICEID_MODERN_MAX) {
		type = devid - VIRTIO_PCI_DEVICEID_MODERN_MIN;
	} else {
		return (ENXIO);
	}

	name = virtio_device_name(type);
	if (name == NULL)
		name = "Unknown";

	ksnprintf(desc, sizeof(desc), "VirtIO PCI %s adapter", name);
	device_set_desc_copy(dev, desc);

	return (BUS_PROBE_DEFAULT);
}

static int
vtpci_attach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int msix_cap, rid;

	sc = device_get_softc(dev);
	sc->vtpci_dev = dev;
	sc->vtpci_config_irq = -1;

	pci_enable_busmaster(dev);

	if (pci_get_device(dev) >= VIRTIO_PCI_DEVICEID_MODERN_MIN) {
		sc->vtpci_flags |= VIRTIO_PCI_FLAG_MODERN;
		sc->vtpci_devtype = pci_get_device(dev) -
		    VIRTIO_PCI_DEVICEID_MODERN_MIN;
		if (vtpci_modern_map(sc) != 0) {
			vtpci_detach(dev);
			return (ENXIO);
		}
	} else {
		sc->vtpci_devtype = pci_get_subdevice(dev);
		rid = PCIR_BAR(0);
		sc->vtpci_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &rid, RF_ACTIVE);
		if (sc->vtpci_res == NULL) {
			device_printf(dev, "cannot map I/O space\n");
			return (ENXIO);
		}
	}

	if (pci_find_extcap(dev, PCIY_MSIX, &msix_cap) == 0) {
		uint32_t val;
		val = pci_read_config(dev, msix_cap + PCIR_MSIX_TABLE, 4);
		if (vtpci_is_modern(sc)) {
			/* The table may share a BAR with the registers. */
			sc->vtpci_msix_res = vtpci_modern_bar(sc,
			    val & PCIM_MSIX_BIR_MASK);
		} else {
			rid = PCIR_BAR(val & PCIM_MSIX_BIR_MASK);
			sc->vtpci_msix_res = bus_alloc_resource_any(dev,
			    SYS_RES_MEMORY, &rid, RF_ACTIVE);
		}
	}

	vtpci_reset(sc);

	/* Tell the host we've noticed this device. */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);

	if ((child = device_add_child(dev, NULL, -1)) == NULL) {
		device_printf(dev, "cannot create child device\n");
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_detach(dev);
		return (ENOMEM);
	}

	sc->vtpci_child_dev = child;
	vtpci_probe_and_attach_child(sc);

	return (0);
}

static int
vtpci_detach(device_t dev)
{
	struct vtpci_softc *sc;
	device_t child;
	int error, i;

	sc = device_get_softc(dev);

	if ((child = sc->vtpci_child_dev) != NULL) {
		error = device_delete_child(dev, child);
		if (error)
			return (error);
		sc->vtpci_child_dev = NULL;
	}

	vtpci_reset(sc);

	if (vtpci_is_modern(sc)) {
		/* The MSI-X table BAR, if any, is one of these. */
		sc->vtpci_msix_res = NULL;
		for (i = 0; i < VTPCI_MAX_BARS; i++) {
			if (sc->vtpci_bar_res[i] == NULL)
				continue;
			bus_release_resource(dev, sc->vtpci_bar_type[i],
			    PCIR_BAR(i), sc->vtpci_bar_res[i]);
			sc->vtpci_bar_res[i] = NULL;
		}
		bzero(&sc->vtpci_common, sizeof(sc->vtpci_common));
		bzero(&sc->vtpci_notify, sizeof(sc->vtpci_notify));
		bzero(&sc->vtpci_isr, sizeof(sc->vtpci_isr));
		bzero(&sc->vtpci_device, sizeof(sc->vtpci_device));
	}

	if (sc->vtpci_msix_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->vtpci_msix_res), sc->vtpci_msix_res);
		sc->vtpci_msix_res = NULL;
	}

	if (sc->vtpci_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_BAR(0),
		    sc->vtpci_res);
		sc->vtpci_res = NULL;
	}

	return (0);
}

static int
vtpci_suspend(device_t dev)
{

	return (bus_generic_suspend(dev));
}

static int
vtpci_resume(device_t dev)
{

	return (bus_generic_resume(dev));
}

static int
vtpci_shutdown(device_t dev)
{

	(void) bus_generic_shutdown(dev);
	/* Forcibly stop the host device. */
	vtpci_stop(dev);

	return (0);
}

static void
vtpci_driver_added(device_t dev, driver_t *driver)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_probe_and_attach_child(sc);
}

static void
vtpci_child_detached(device_t dev, device_t child)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	vtpci_reset(sc);
	vtpci_release_child_resources(sc);
}

static int
vtpci_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_DEVTYPE:
		*result = sc->vtpci_devtype;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static int
vtpci_write_ivar(device_t dev, device_t child, int index, uintptr_t value)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (sc->vtpci_child_dev != child)
		return (ENOENT);

	switch (index) {
	case VIRTIO_IVAR_FEATURE_DESC:
		sc->vtpci_child_feat_desc = (void *) value;
		break;
	default:
		return (ENOENT);
	}

	return (0);
}

static uint64_t
vtpci_negotiate_features(device_t dev, uint64_t child_features)
{
	struct vtpci_softc *sc;
	uint64_t host_features, features;

	sc = device_get_softc(dev);

	if (vtpci_is_modern(sc)) {
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_DFSELECT, 0);
		host_features = vtpci_common_read_4(sc, VIRTIO_PCI_COMMON_DF);
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_DFSELECT, 1);
		host_features |= (uint64_t)vtpci_common_read_4(sc,
		    VIRTIO_PCI_COMMON_DF) << 32;
		/* A modern device is driven as one. */
		child_features |= VIRTIO_F_VERSION_1;
	} else {
		host_features = vtpci_read_config_4(sc,
		    VIRTIO_PCI_HOST_FEATURES);
	}
	vtpci_describe_features(sc, "host", host_features);

	/*
	 * Limit negotiated features to what the driver, virtqueue, and
	 * host all support.
	 */
	features = host_features & child_features;
	features = virtqueue_filter_features(features);
	sc->vtpci_features = features;

	vtpci_describe_features(sc, "negotiated", features);
	if (vtpci_is_modern(sc)) {
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_GFSELECT, 0);
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_GF,
		    (uint32_t)features);
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_GFSELECT, 1);
		vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_GF,
		    (uint32_t)(features >> 32));
	} else {
		vtpci_write_config_4(sc, VIRTIO_PCI_GUEST_FEATURES, features);
	}

	return (features);
}

static int
vtpci_with_feature(device_t dev, uint64_t feature)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	return ((sc->vtpci_features & feature) != 0);
}

static int
vtpci_intr_count(device_t dev)
{
	struct vtpci_softc *sc = device_get_softc(dev);

	if (vtpci_disable_msix != 0 || sc->vtpci_msix_res == NULL)
		return 1;
	else
		return pci_msix_count(dev);
}

/* Will never return 0, with *cnt <= 0. */
static int
vtpci_intr_alloc(device_t dev, int *cnt, int use_config, int *cpus)
{
	struct vtpci_softc *sc = device_get_softc(dev);
	int i;

	if (sc->vtpci_nintr_res > 0)
		return (EINVAL);

	if (*cnt <= 0)
		return (EINVAL);

	if (vtpci_disable_msix == 0 && sc->vtpci_msix_res != NULL) {
		int nmsix = pci_msix_count(dev);
		if (nmsix < *cnt)
			*cnt = nmsix;
	}

	if ((*cnt > 1 || use_config == 0) &&
	    vtpci_disable_msix == 0 && sc->vtpci_msix_res != NULL) {
		if (pci_setup_msix(dev) != 0) {
			device_printf(dev, "pci_setup_msix failed\n");
			/* Just fallthrough to legacy IRQ code instead. */
		} else {
			for (i = 0; i < *cnt; i++) {
				int cpu, rid;

				if (cpus != NULL && cpus[i] >= 0 &&
				    cpus[i] < ncpus) {
					cpu = cpus[i];
				} else {
					cpu = device_get_unit(dev) + i;
					cpu %= ncpus;
				}
				if (pci_alloc_msix_vector(dev, i, &rid, cpu)
				    != 0) {
					if (i > 1 || (i == 1 && !use_config)) {
						*cnt = i;
						/* Got some MSI-X vectors. */
						sc->vtpci_irq_flags = RF_ACTIVE;
						sc->vtpci_flags |=
						    VIRTIO_PCI_FLAG_MSIX;
						goto finish;
					}
					/*
					 * Allocate the legacy IRQ instead.
					 */
					if (i == 1) {
						pci_release_msix_vector(dev, 0);
					}
					pci_teardown_msix(dev);
					break;
				}
				sc->vtpci_intr_res[i].rid = rid;
			}
			/* Got all the MSI-X vectors we wanted. */
			sc->vtpci_irq_flags = RF_ACTIVE;
			sc->vtpci_flags |= VIRTIO_PCI_FLAG_MSIX;
			/* Successfully allocated all MSI-X vectors */
			goto finish;
		}
	}

	/* Legacy IRQ code: */
	*cnt = 1;
	/*
	 * Use MSI interrupts if available. Otherwise, we fallback
	 * to legacy interrupts.
	 */
	sc->vtpci_intr_res[0].rid = 0;
	if (pci_alloc_1intr(sc->vtpci_dev, 1,
	    &sc->vtpci_intr_res[0].rid,
	    &sc->vtpci_irq_flags) == PCI_INTR_TYPE_MSI) {
		sc->vtpci_flags |= VIRTIO_PCI_FLAG_MSI;
	}

finish:
	KKASSERT(!((sc->vtpci_flags & VIRTIO_PCI_FLAG_MSI) != 0 &&
		   (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) != 0));

	sc->vtpci_nintr_res = *cnt;
	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		struct resource *irq;

		TAILQ_INIT(&sc->vtpci_intr_res[i].ls);
		sc->vtpci_intr_res[i].ires_sc = sc;
		irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->vtpci_intr_res[i].rid, sc->vtpci_irq_flags);
		if (irq == NULL)
			return (ENXIO);
		if (cpus != NULL)
			cpus[i] = rman_get_cpuid(irq);

		sc->vtpci_intr_res[i].irq = irq;
	}

	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		device_printf(dev, "using %d MSI-X vectors\n", *cnt);
		pci_enable_msix(dev);
	}

	return (0);
}

static int
vtpci_intr_release(device_t dev)
{
	struct vtpci_softc *sc = device_get_softc(dev);
	struct vtpci_intr_resource *ires;
	int i;

	if (sc->vtpci_nintr_res == 0)
		return (EINVAL);

	/* XXX Make sure none of the interrupts is used at the moment. */

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		ires = &sc->vtpci_intr_res[i];

		KKASSERT(TAILQ_EMPTY(&ires->ls));
		if (ires->irq != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, ires->rid,
			    ires->irq);
			ires->irq = NULL;
		}
		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX)
			pci_release_msix_vector(dev, ires->rid);
		ires->rid = 0;
	}
	sc->vtpci_nintr_res = 0;
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSI) {
		pci_release_msi(dev);
		sc->vtpci_flags &= ~VIRTIO_PCI_FLAG_MSI;
	}
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		pci_teardown_msix(dev);
		sc->vtpci_flags &= ~VIRTIO_PCI_FLAG_MSIX;
	}
	return (0);
}

static int
vtpci_alloc_virtqueues(device_t dev, int nvqs, struct vq_alloc_info *vq_info)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	struct vq_alloc_info *info;
	int queue, error;
	uint16_t vq_size;

	sc = device_get_softc(dev);

	if (sc->vtpci_nvqs != 0 || nvqs <= 0 ||
	    nvqs > VIRTIO_MAX_VIRTQUEUES)
		return (EINVAL);

	if (vtpci_is_modern(sc)) {
		/*
		 * The features are final once the queues are set up.  Drivers
		 * may negotiate more than once before this, so FEATURES_OK is
		 * set here rather than in vtpci_negotiate_features().
		 */
		error = vtpci_modern_features_ok(sc);
		if (error)
			return (error);
		if (nvqs > vtpci_common_read_2(sc, VIRTIO_PCI_COMMON_NUMQ))
			return (EINVAL);
	}

	for (queue = 0; queue < nvqs; queue++) {
		vqx = &sc->vtpci_vqx[queue];
		info = &vq_info[queue];

		vqx->ires_idx = -1;
		vtpci_select_queue(sc, queue);

		vq_size = vtpci_get_queue_size(sc);
		error = virtqueue_alloc(dev, queue, vq_size,
		    VIRTIO_PCI_VRING_ALIGN,
		    vtpci_is_modern(sc) ? BUS_SPACE_MAXADDR : 0xFFFFFFFFUL,
		    info, &vqx->vq);
		if (error)
			return (error);

		vtpci_set_queue(sc, queue, vqx->vq);

		*info->vqai_vq = vqx->vq;
		sc->vtpci_nvqs++;
	}

	return (0);
}

/* XXX Add argument to specify the callback function here. */
static int
vtpci_setup_intr(device_t dev, uint irq, lwkt_serialize_t slz)
{
	struct vtpci_softc *sc;
	struct vtpci_intr_resource *ires;
	int flags, error;

	sc = device_get_softc(dev);
	flags = INTR_MPSAFE;

	if ((int)irq >= sc->vtpci_nintr_res)
		return (EINVAL);
	ires = &sc->vtpci_intr_res[irq];

	if ((sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) == 0) {
		error = bus_setup_intr(dev, ires->irq, flags,
				       vtpci_legacy_intr,
				       ires, &ires->intrhand, slz);
	} else {
		error = bus_setup_intr(dev, ires->irq, flags,
				       vtpci_msix_intr,
				       ires, &ires->intrhand, slz);
	}
	return (error);
}

static int
vtpci_teardown_intr(device_t dev, uint irq)
{
	struct vtpci_softc *sc = device_get_softc(dev);
	struct vtpci_intr_resource *ires;

	if ((int)irq >= sc->vtpci_nintr_res)
		return (EINVAL);

	ires = &sc->vtpci_intr_res[irq];

	if (ires->intrhand == NULL)
		return (ENXIO);

	bus_teardown_intr(dev, ires->irq, ires->intrhand);
	ires->intrhand = NULL;
	return (0);
}

static void
vtpci_add_irqentry(struct vtpci_intr_resource *intr_res, int what,
    driver_intr_t handler, void *arg)
{
	struct vqentry *e;

	TAILQ_FOREACH(e, &intr_res->ls, entries) {
		if (e->what == what)
			return;
	}
	e = kmalloc(sizeof(*e), M_DEVBUF, M_WAITOK | M_ZERO);
	e->what = what;
	if (e->what == -1) {
		e->vq = NULL;
	} else {
		e->vq = intr_res->ires_sc->vtpci_vqx[e->what].vq;
	}
	e->handler = handler;
	e->arg = arg;
	TAILQ_INSERT_TAIL(&intr_res->ls, e, entries);
}

static void
vtpci_del_irqentry(struct vtpci_intr_resource *intr_res, int what)
{
	struct vqentry *e;

	TAILQ_FOREACH(e, &intr_res->ls, entries) {
		if (e->what == what)
			break;
	}
	if (e != NULL) {
		TAILQ_REMOVE(&intr_res->ls, e, entries);
		kfree(e, M_DEVBUF);
	}
}

/*
 * Config intr can be bound after intr_alloc, virtqueue intrs can be bound
 * after intr_alloc and alloc_virtqueues.
 */
static int
vtpci_bind_intr(device_t dev, uint irq, int what,
    driver_intr_t handler, void *arg)
{
	struct vtpci_softc *sc = device_get_softc(dev);
	struct vtpci_virtqueue *vqx;
	int error;

	if (irq >= sc->vtpci_nintr_res)
		return (EINVAL);

	if (what == -1) {
		if (sc->vtpci_config_irq != -1)
			return (EINVAL);

		sc->vtpci_config_irq = irq;
		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
			error = vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_CONFIG_VECTOR, irq);
			if (error)
				return (error);
		}
		goto done;
	}

	if (sc->vtpci_nvqs <= what || what < 0)
		return (EINVAL);

	vqx = &sc->vtpci_vqx[what];
	if (vqx->ires_idx != -1)
		return (EINVAL);

	vqx->ires_idx = irq;
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		vtpci_select_queue(sc, what);
		error = vtpci_register_msix_vector(sc, VIRTIO_MSI_QUEUE_VECTOR,
		    irq);
		if (error)
			return (error);
	}
done:
	vtpci_add_irqentry(&sc->vtpci_intr_res[irq], what, handler, arg);
	return (0);
}

static int
vtpci_unbind_intr(device_t dev, int what)
{
	struct vtpci_softc *sc = device_get_softc(dev);
	struct vtpci_virtqueue *vqx;
	uint irq;

	if (what == -1) {
		if (sc->vtpci_config_irq == -1)
			return (EINVAL);

		irq = sc->vtpci_config_irq;
		sc->vtpci_config_irq = -1;
		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
			vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_CONFIG_VECTOR, -1);
		}
		goto done;
	}

	if (sc->vtpci_nvqs <= what || what < 0)
		return (EINVAL);

	vqx = &sc->vtpci_vqx[what];
	if (vqx->ires_idx == -1)
		return (EINVAL);

	irq = vqx->ires_idx;
	vqx->ires_idx = -1;
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		vtpci_select_queue(sc, what);
		vtpci_register_msix_vector(sc, VIRTIO_MSI_QUEUE_VECTOR, -1);
	}
done:
	KKASSERT(irq >= 0 && irq < sc->vtpci_nintr_res);
	vtpci_del_irqentry(&sc->vtpci_intr_res[irq], what);
	return (0);
}

static void
vtpci_stop(device_t dev)
{
	vtpci_reset(device_get_softc(dev));
}

static int
vtpci_reinit(device_t dev, uint64_t features)
{
	struct vtpci_softc *sc;
	struct vtpci_virtqueue *vqx;
	struct virtqueue *vq;
	int queue, error;
	uint16_t vq_size;

	sc = device_get_softc(dev);

	/*
	 * Redrive the device initialization. This is a bit of an abuse
	 * of the specification, but both VirtualBox and QEMU/KVM seem
	 * to play nice. We do not allow the host device to change from
	 * what was originally negotiated beyond what the guest driver
	 * changed (MSIX state should not change, number of virtqueues
	 * and their size remain the same, etc).
	 */

	if (vtpci_get_status(dev) != VIRTIO_CONFIG_STATUS_RESET)
		vtpci_stop(dev);

	/*
	 * Quickly drive the status through ACK and DRIVER. The device
	 * does not become usable again until vtpci_reinit_complete().
	 */
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);

	vtpci_negotiate_features(dev, features);
	if (vtpci_is_modern(sc)) {
		error = vtpci_modern_features_ok(sc);
		if (error)
			return (error);
	}

	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		pci_enable_msix(dev);
		if (sc->vtpci_config_irq != -1) {
			error = vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_CONFIG_VECTOR, sc->vtpci_config_irq);
			if (error)
				return (error);
		}
	}

	for (queue = 0; queue < sc->vtpci_nvqs; queue++) {
		vqx = &sc->vtpci_vqx[queue];
		vq = vqx->vq;

		KASSERT(vq != NULL, ("vq %d not allocated", queue));
		vtpci_select_queue(sc, queue);

		vq_size = vtpci_get_queue_size(sc);
		error = virtqueue_reinit(vq, vq_size);
		if (error)
			return (error);

		if (vqx->ires_idx != -1 &&
		    (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX)) {
			error = vtpci_register_msix_vector(sc,
			    VIRTIO_MSI_QUEUE_VECTOR, vqx->ires_idx);
			if (error)
				return (error);
		}

		vtpci_set_queue(sc, queue, vqx->vq);
	}

	return (0);
}

static void
vtpci_reinit_complete(device_t dev)
{

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static void
vtpci_notify_virtqueue(device_t dev, uint16_t queue)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (vtpci_is_modern(sc)) {
		vtpci_region_write_2(&sc->vtpci_notify,
		    sc->vtpci_notify_off[queue], queue);
		return;
	}
	vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_NOTIFY, queue);
}

static uint8_t
vtpci_get_status(device_t dev)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (vtpci_is_modern(sc))
		return (vtpci_common_read_1(sc, VIRTIO_PCI_COMMON_STATUS));
	return (vtpci_read_config_1(sc, VIRTIO_PCI_STATUS));
}

static void
vtpci_set_status(device_t dev, uint8_t status)
{
	struct vtpci_softc *sc;

	sc = device_get_softc(dev);

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vtpci_get_status(dev);

	if (vtpci_is_modern(sc))
		vtpci_common_write_1(sc, VIRTIO_PCI_COMMON_STATUS, status);
	else
		vtpci_write_config_1(sc, VIRTIO_PCI_STATUS, status);
}

/*
 * The device-specific configuration of a modern device is in its own
 * region.  Fields must be accessed at their natural size and alignment
 * (wider fields 32 bits at a time), and a multi-access read is redone if
 * the device changed the configuration meanwhile.
 */
static void
vtpci_modern_rw_dev_config(struct vtpci_softc *sc, bus_size_t off,
    uint8_t *p, int length, int write)
{
	struct vtpci_region *r;
	uint8_t gen;
	int i, size;

	r = &sc->vtpci_device;
	if (r->res == NULL || off + length > r->len) {
		device_printf(sc->vtpci_dev, "device config %s out of range: "
		    "offset %ju, length %d\n", write ? "write" : "read",
		    (uintmax_t)off, length);
		if (!write)
			bzero(p, length);
		return;
	}

	do {
		gen = vtpci_common_read_1(sc, VIRTIO_PCI_COMMON_CFGGENERATION);
		for (i = 0; i < length; i += size) {
			if (length - i >= 4 && ((off + i) & 3) == 0)
				size = 4;
			else if (length - i >= 2 && ((off + i) & 1) == 0)
				size = 2;
			else
				size = 1;
			if (write) {
				if (size == 4)
					vtpci_region_write_4(r, off + i,
					    *(uint32_t *)(p + i));
				else if (size == 2)
					vtpci_region_write_2(r, off + i,
					    *(uint16_t *)(p + i));
				else
					vtpci_region_write_1(r, off + i, p[i]);
			} else {
				if (size == 4)
					*(uint32_t *)(p + i) =
					    vtpci_region_read_4(r, off + i);
				else if (size == 2)
					*(uint16_t *)(p + i) =
					    vtpci_region_read_2(r, off + i);
				else
					p[i] = vtpci_region_read_1(r, off + i);
			}
		}
	} while (!write && gen !=
	    vtpci_common_read_1(sc, VIRTIO_PCI_COMMON_CFGGENERATION));
}

static void
vtpci_read_dev_config(device_t dev, bus_size_t offset,
    void *dst, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *d;
	int size;

	sc = device_get_softc(dev);
	if (vtpci_is_modern(sc)) {
		vtpci_modern_rw_dev_config(sc, offset, dst, length, 0);
		return;
	}
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (d = dst; length > 0; d += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			*(uint32_t *)d = vtpci_read_config_4(sc, off);
		} else if (length >= 2) {
			size = 2;
			*(uint16_t *)d = vtpci_read_config_2(sc, off);
		} else {
			size = 1;
			*d = vtpci_read_config_1(sc, off);
		}
	}
}

static void
vtpci_write_dev_config(device_t dev, bus_size_t offset,
    void *src, int length)
{
	struct vtpci_softc *sc;
	bus_size_t off;
	uint8_t *s;
	int size;

	sc = device_get_softc(dev);
	if (vtpci_is_modern(sc)) {
		vtpci_modern_rw_dev_config(sc, offset, src, length, 1);
		return;
	}
	off = VIRTIO_PCI_CONFIG(sc) + offset;

	for (s = src; length > 0; s += size, off += size, length -= size) {
		if (length >= 4) {
			size = 4;
			vtpci_write_config_4(sc, off, *(uint32_t *)s);
		} else if (length >= 2) {
			size = 2;
			vtpci_write_config_2(sc, off, *(uint16_t *)s);
		} else {
			size = 1;
			vtpci_write_config_1(sc, off, *s);
		}
	}
}

static void
vtpci_describe_features(struct vtpci_softc *sc, const char *msg,
    uint64_t features)
{
	device_t dev, child;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (device_is_attached(child) && bootverbose == 0)
		return;

	virtio_describe(dev, msg, features, sc->vtpci_child_feat_desc);
}

static void
vtpci_probe_and_attach_child(struct vtpci_softc *sc)
{
	device_t dev, child;
	int error;

	dev = sc->vtpci_dev;
	child = sc->vtpci_child_dev;

	if (child == NULL)
		return;

	if (device_get_state(child) != DS_NOTPRESENT)
		return;

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER);
	error = device_probe_and_attach(child);
	if (error != 0 || device_get_state(child) == DS_NOTPRESENT) {
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		vtpci_reset(sc);
		vtpci_release_child_resources(sc);

		/* Reset status for future attempt. */
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_ACK);
	} else
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_DRIVER_OK);
}

static int
vtpci_register_msix_vector(struct vtpci_softc *sc, int offset, int res_idx)
{
	device_t dev;
	uint16_t vector, readback;
	bus_size_t reg;

	dev = sc->vtpci_dev;

	if (offset != VIRTIO_MSI_CONFIG_VECTOR &&
	    offset != VIRTIO_MSI_QUEUE_VECTOR)
		return (EINVAL);

	if (res_idx != -1) {
		/* Map from rid to host vector. */
		vector = res_idx;
	} else {
		vector = VIRTIO_MSI_NO_VECTOR;
	}

	if (vtpci_is_modern(sc)) {
		/* The queue vector is that of the selected queue, as above. */
		reg = (offset == VIRTIO_MSI_CONFIG_VECTOR) ?
		    VIRTIO_PCI_COMMON_MSIX : VIRTIO_PCI_COMMON_Q_MSIX;
		vtpci_common_write_2(sc, reg, vector);
		readback = vtpci_common_read_2(sc, reg);
	} else {
		vtpci_write_config_2(sc, offset, vector);
		readback = vtpci_read_config_2(sc, offset);
	}

	if (readback != vector) {
		device_printf(dev, "insufficient host resources for "
		    "MSIX interrupts\n");
		return (ENODEV);
	}

	return (0);
}

static void
vtpci_free_interrupts(struct vtpci_softc *sc)
{
	device_t dev = sc->vtpci_dev;
	struct vtpci_intr_resource *ires;
	int i;

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		ires = &sc->vtpci_intr_res[i];

		if (ires->intrhand != NULL) {
			bus_teardown_intr(dev, ires->irq, ires->intrhand);
			ires->intrhand = NULL;
		}
		if (ires->irq != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, ires->rid,
			    ires->irq);
			ires->irq = NULL;
		}
	}

	vtpci_unbind_intr(sc->vtpci_dev, -1);
	for (i = 0; i < sc->vtpci_nvqs; i++)
		vtpci_unbind_intr(sc->vtpci_dev, i);

	for (i = 0; i < sc->vtpci_nintr_res; i++) {
		ires = &sc->vtpci_intr_res[i];

		if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX)
			pci_release_msix_vector(dev, ires->rid);
		ires->rid = 0;
	}
	sc->vtpci_nintr_res = 0;
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSI) {
		pci_release_msi(dev);
		sc->vtpci_flags &= ~VIRTIO_PCI_FLAG_MSI;
	}
	if (sc->vtpci_flags & VIRTIO_PCI_FLAG_MSIX) {
		pci_disable_msix(dev);
		pci_teardown_msix(dev);
		sc->vtpci_flags &= ~VIRTIO_PCI_FLAG_MSIX;
	}

}

static void
vtpci_free_virtqueues(struct vtpci_softc *sc)
{
	struct vtpci_virtqueue *vqx;
	int i;

	sc->vtpci_nvqs = 0;

	for (i = 0; i < VIRTIO_MAX_VIRTQUEUES; i++) {
		vqx = &sc->vtpci_vqx[i];

		if (vqx->vq != NULL) {
			virtqueue_free(vqx->vq);
			vqx->vq = NULL;
		}
	}
}

static void
vtpci_release_child_resources(struct vtpci_softc *sc)
{
	vtpci_free_interrupts(sc);
	vtpci_free_virtqueues(sc);
}

static void
vtpci_reset(struct vtpci_softc *sc)
{
	int i;

	if (vtpci_is_modern(sc) && sc->vtpci_common.res == NULL)
		return;

	/*
	 * Setting the status to RESET sets the host device to
	 * the original, uninitialized state.
	 */
	vtpci_set_status(sc->vtpci_dev, VIRTIO_CONFIG_STATUS_RESET);

	/*
	 * A modern device may take a while: it is reset once the status
	 * reads back as zero.
	 */
	if (vtpci_is_modern(sc)) {
		for (i = 0; i < 1000; i++) {
			if (vtpci_get_status(sc->vtpci_dev) ==
			    VIRTIO_CONFIG_STATUS_RESET)
				break;
			DELAY(1000);
		}
		if (i == 1000)
			device_printf(sc->vtpci_dev, "reset timed out\n");
	}
}

/*
 * Modern devices.
 */

/*
 * Return the resource of a BAR that capabilities point to, mapping the
 * BAR the first time.
 */
static struct resource *
vtpci_modern_bar(struct vtpci_softc *sc, int bar)
{
	device_t dev;
	uint32_t val;
	int rid, type;

	dev = sc->vtpci_dev;

	if (bar < 0 || bar >= VTPCI_MAX_BARS)
		return (NULL);
	if (sc->vtpci_bar_res[bar] != NULL)
		return (sc->vtpci_bar_res[bar]);

	val = pci_read_config(dev, PCIR_BAR(bar), 4);
	type = PCI_BAR_IO(val) ? SYS_RES_IOPORT : SYS_RES_MEMORY;
	rid = PCIR_BAR(bar);
	sc->vtpci_bar_res[bar] = bus_alloc_resource_any(dev, type, &rid,
	    RF_ACTIVE);
	if (sc->vtpci_bar_res[bar] == NULL) {
		device_printf(dev, "cannot map BAR %d\n", bar);
		return (NULL);
	}
	sc->vtpci_bar_type[bar] = type;

	return (sc->vtpci_bar_res[bar]);
}

/*
 * Find the first vendor-specific capability of the given type that is
 * usable (spec 4.1.4: later ones are alternatives), and map its region.
 */
static int
vtpci_modern_map_region(struct vtpci_softc *sc, uint8_t cfg_type,
    struct vtpci_region *r, bus_size_t minlen, int *capp)
{
	device_t dev;
	struct resource *res;
	uint32_t off, len;
	int ptr, bar;

	dev = sc->vtpci_dev;

	if (pci_find_extcap(dev, PCIY_VENDOR, &ptr) != 0)
		return (ENOENT);

	for (; ptr != 0;
	    ptr = pci_read_config(dev, ptr + PCICAP_NEXTPTR, 1) & ~3) {
		if (pci_read_config(dev, ptr + PCICAP_ID, 1) != PCIY_VENDOR ||
		    pci_read_config(dev, ptr + VIRTIO_PCI_CAP_CFG_TYPE, 1) !=
		    cfg_type)
			continue;

		bar = pci_read_config(dev, ptr + VIRTIO_PCI_CAP_BAR, 1);
		off = pci_read_config(dev, ptr + VIRTIO_PCI_CAP_OFFSET, 4);
		len = pci_read_config(dev, ptr + VIRTIO_PCI_CAP_LENGTH, 4);
		if (bar >= VTPCI_MAX_BARS || len < minlen)
			continue;

		res = vtpci_modern_bar(sc, bar);
		if (res == NULL)
			return (ENXIO);
		if ((bus_size_t)off + len > rman_get_size(res)) {
			device_printf(dev, "capability type %d outside "
			    "BAR %d\n", cfg_type, bar);
			continue;
		}

		r->res = res;
		r->off = off;
		r->len = len;
		if (capp != NULL)
			*capp = ptr;
		return (0);
	}

	return (ENOENT);
}

static int
vtpci_modern_map(struct vtpci_softc *sc)
{
	device_t dev;
	int cap, error;

	dev = sc->vtpci_dev;

	error = vtpci_modern_map_region(sc, VIRTIO_PCI_CAP_COMMON_CFG,
	    &sc->vtpci_common, VIRTIO_PCI_COMMON_CFG_SIZE, NULL);
	if (error) {
		device_printf(dev, "no usable common configuration\n");
		return (error);
	}

	error = vtpci_modern_map_region(sc, VIRTIO_PCI_CAP_NOTIFY_CFG,
	    &sc->vtpci_notify, 2, &cap);
	if (error) {
		device_printf(dev, "no usable notify configuration\n");
		return (error);
	}
	sc->vtpci_notify_mult = pci_read_config(dev,
	    cap + VIRTIO_PCI_NOTIFY_CAP_MULT, 4);

	error = vtpci_modern_map_region(sc, VIRTIO_PCI_CAP_ISR_CFG,
	    &sc->vtpci_isr, 1, NULL);
	if (error) {
		device_printf(dev, "no usable ISR configuration\n");
		return (error);
	}

	/* Not every device has a device-specific configuration. */
	error = vtpci_modern_map_region(sc, VIRTIO_PCI_CAP_DEVICE_CFG,
	    &sc->vtpci_device, 0, NULL);
	if (error == ENXIO)
		return (error);

	return (0);
}

/*
 * Finish the feature negotiation of a modern device (spec 3.1.1): the
 * device may refuse the features the driver wrote.
 */
static int
vtpci_modern_features_ok(struct vtpci_softc *sc)
{
	device_t dev;

	dev = sc->vtpci_dev;

	if (vtpci_get_status(dev) & VIRTIO_CONFIG_STATUS_FEATURES_OK)
		return (0);

	if ((sc->vtpci_features & VIRTIO_F_VERSION_1) == 0) {
		device_printf(dev, "device does not offer VIRTIO_F_VERSION_1\n");
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		return (ENXIO);
	}

	vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FEATURES_OK);
	if ((vtpci_get_status(dev) & VIRTIO_CONFIG_STATUS_FEATURES_OK) == 0) {
		device_printf(dev, "device refused the negotiated features\n");
		vtpci_set_status(dev, VIRTIO_CONFIG_STATUS_FAILED);
		return (ENXIO);
	}

	return (0);
}

static void
vtpci_select_queue(struct vtpci_softc *sc, int queue)
{
	if (vtpci_is_modern(sc))
		vtpci_common_write_2(sc, VIRTIO_PCI_COMMON_Q_SELECT, queue);
	else
		vtpci_write_config_2(sc, VIRTIO_PCI_QUEUE_SEL, queue);
}

/* Size of the selected queue. */
static uint16_t
vtpci_get_queue_size(struct vtpci_softc *sc)
{
	if (vtpci_is_modern(sc))
		return (vtpci_common_read_2(sc, VIRTIO_PCI_COMMON_Q_SIZE));
	return (vtpci_read_config_2(sc, VIRTIO_PCI_QUEUE_NUM));
}

/*
 * Give the device the selected queue's ring.  A modern device takes the
 * three parts separately and the queue must then be enabled; its
 * notifications go to an offset of its own in the notify region.
 */
static void
vtpci_set_queue(struct vtpci_softc *sc, int queue, struct virtqueue *vq)
{
	uint64_t pa;
	bus_size_t noff;

	if (!vtpci_is_modern(sc)) {
		vtpci_write_config_4(sc, VIRTIO_PCI_QUEUE_PFN,
		    virtqueue_paddr(vq) >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);
		return;
	}

	pa = virtqueue_desc_paddr(vq);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_DESCLO, (uint32_t)pa);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_DESCHI, pa >> 32);
	pa = virtqueue_avail_paddr(vq);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_AVAILLO, (uint32_t)pa);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_AVAILHI, pa >> 32);
	pa = virtqueue_used_paddr(vq);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_USEDLO, (uint32_t)pa);
	vtpci_common_write_4(sc, VIRTIO_PCI_COMMON_Q_USEDHI, pa >> 32);

	noff = (bus_size_t)vtpci_common_read_2(sc, VIRTIO_PCI_COMMON_Q_NOFF) *
	    sc->vtpci_notify_mult;
	if (noff + 2 > sc->vtpci_notify.len) {
		device_printf(sc->vtpci_dev, "queue %d notify offset %ju "
		    "outside the notify region\n", queue, (uintmax_t)noff);
		noff = 0;
	}
	sc->vtpci_notify_off[queue] = noff;

	vtpci_common_write_2(sc, VIRTIO_PCI_COMMON_Q_ENABLE, 1);
}

static void
vtpci_legacy_intr(void *arg)
{
	struct vtpci_intr_resource *ires;
	struct vtpci_softc *sc;
	struct vqentry *e;
	uint8_t isr;

	ires = arg;
	sc = ires->ires_sc;

	/* Reading the ISR also clears it. */
	if (vtpci_is_modern(sc))
		isr = vtpci_region_read_1(&sc->vtpci_isr, 0);
	else
		isr = vtpci_read_config_1(sc, VIRTIO_PCI_ISR);

	TAILQ_FOREACH(e, &ires->ls, entries) {
		/*
		 * The lwkt_serialize_handler_call API doesn't seem to fit
		 * properly here. Instead move the virtqueue pending check
		 * into the driver, who can then properly implement masking
		 * of the handler itself.
		 */
		if (e->what == -1) {
			if (isr & VIRTIO_PCI_ISR_CONFIG)
				e->handler(e->arg);
		} else if (isr & VIRTIO_PCI_ISR_INTR) {
			e->handler(e->arg);
		}
	}
}

static void
vtpci_msix_intr(void *arg)
{
	struct vtpci_intr_resource *ires;
	struct vtpci_softc *sc;
	struct vqentry *e;

	ires = arg;
	sc = ires->ires_sc;
	TAILQ_FOREACH(e, &ires->ls, entries) {
		/*
		 * The lwkt_serialize_handler_call API doesn't seem to fit
		 * properly here. Instead move the virtqueue pending check
		 * into the driver, who can then properly implement masking
		 * of the handler itself.
		 */
		e->handler(e->arg);
	}
}
