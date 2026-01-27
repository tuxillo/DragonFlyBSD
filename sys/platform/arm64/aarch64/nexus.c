/*-
 * Copyright 1998 Massachusetts Institute of Technology
 * Copyright (c) 2003-2026 The DragonFly Project.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * ARM64 root nexus driver.
 *
 * This code implements a `root nexus' for ARM64 machines. The function
 * of the root nexus is to serve as an attachment point for both processors
 * and buses, and to manage resources which are common to all of them.
 * In particular, this code implements the core resource managers for
 * interrupt requests and I/O memory address space.
 *
 * This is a minimal implementation for the ARM64 port MVP. Future work
 * will add FDT (Flattened Device Tree) and ACPI device enumeration support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/gic.h>

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

struct nexus_device {
	struct resource_list	nx_resources;
};

#define DEVTONX(dev)	((struct nexus_device *)device_get_ivars(dev))

static struct rman mem_rman;
static struct rman irq_rman;

static int	nexus_probe(device_t);
static int	nexus_attach(device_t);
static int	nexus_print_child(device_t, device_t);
static device_t	nexus_add_child(device_t, device_t, int, const char *, int);
static struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
		    u_long, u_long, u_long, u_int, int);
static int	nexus_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_deactivate_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_release_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_set_resource(device_t, device_t, int, int, u_long,
		    u_long, int);
static int	nexus_get_resource(device_t, device_t, int, int, u_long *,
		    u_long *);
static void	nexus_delete_resource(device_t, device_t, int, int);
static int	nexus_setup_intr(device_t, device_t, struct resource *, int,
		    void (*)(void *), void *, void **, lwkt_serialize_t,
		    const char *);
static int	nexus_teardown_intr(device_t, device_t, struct resource *,
		    void *);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	bus_generic_identify),
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	nexus_print_child),
	DEVMETHOD(bus_add_child,	nexus_add_child),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_activate_resource, nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),
	DEVMETHOD(bus_set_resource,	nexus_set_resource),
	DEVMETHOD(bus_get_resource,	nexus_get_resource),
	DEVMETHOD(bus_delete_resource,	nexus_delete_resource),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),

	DEVMETHOD_END
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	1,			/* no softc */
};

static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, NULL, NULL);

static int
nexus_probe(device_t dev)
{
	device_quiet(dev);	/* suppress attach message for neatness */

	/*
	 * Initialize memory resource manager.
	 * ARM64 uses memory-mapped I/O exclusively (no I/O ports).
	 */
	mem_rman.rm_start = 0;
	mem_rman.rm_end = ~0ul;
	mem_rman.rm_type = RMAN_ARRAY;
	mem_rman.rm_descr = "I/O memory addresses";
	if (rman_init(&mem_rman, -1) ||
	    rman_manage_region(&mem_rman, 0, ~0ul))
		panic("nexus_probe mem_rman");

	/*
	 * Initialize IRQ resource manager.
	 * This manages interrupt numbers for the GIC.
	 */
	irq_rman.rm_start = 0;
	irq_rman.rm_end = ~0u;
	irq_rman.rm_type = RMAN_ARRAY;
	irq_rman.rm_descr = "Interrupts";
	if (rman_init(&irq_rman, -1) ||
	    rman_manage_region(&irq_rman, 0, ~0u))
		panic("nexus_probe irq_rman");

	return bus_generic_probe(dev);
}

static int
nexus_attach(device_t dev)
{
	/*
	 * First, let our child driver's identify any child devices that
	 * they can find. Once that is done attach any devices that we
	 * found.
	 *
	 * In the future, this will:
	 * - For FDT systems: add "ofwbus" as a child to enumerate devices
	 *   from the Flattened Device Tree
	 * - For ACPI systems: add "acpi" as a child to enumerate devices
	 *   from ACPI tables
	 *
	 * For now, just call generic attach which will probe any drivers
	 * that have registered with device_identify().
	 */
	bus_generic_attach(dev);

	return 0;
}

static int
nexus_print_child(device_t bus, device_t child)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list *rl = &ndev->nx_resources;
	int retval = 0;

	retval += bus_print_child_header(bus, child);

	if (SLIST_FIRST(rl))
		retval += kprintf(" at");

	retval += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");

	retval += kprintf(" on motherboard\n");

	return retval;
}

static device_t
nexus_add_child(device_t bus, device_t parent, int order,
		const char *name, int unit)
{
	device_t child;
	struct nexus_device *ndev;

	ndev = kmalloc(sizeof(struct nexus_device), M_NEXUSDEV,
		       M_INTWAIT | M_ZERO);
	resource_list_init(&ndev->nx_resources);

	child = device_add_child_ordered(parent, order, name, unit);

	/* should we free this in nexus_child_detached? */
	device_set_ivars(child, ndev);

	return child;
}

/*
 * Allocate a resource on behalf of child.  NB: child is usually going to be a
 * child of one of our descendants, not a direct child of nexus0.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags, int cpuid)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource *rv;
	struct resource_list_entry *rle;
	struct rman *rm;
	int needactivate = flags & RF_ACTIVE;

	/*
	 * If this is an allocation of the "default" range for a given RID,
	 * and we know what the resources for this device are (ie. they aren't
	 * maintained by a child bus), then work out the start/end values.
	 */
	if ((start == 0UL) && (end == ~0UL) && (count == 1)) {
		if (ndev == NULL)
			return NULL;
		rle = resource_list_find(&ndev->nx_resources, type, *rid);
		if (rle == NULL)
			return NULL;
		start = rle->start;
		end = rle->end;
		count = rle->count;
		cpuid = rle->cpuid;
	}

	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &irq_rman;
		break;

	case SYS_RES_MEMORY:
		rm = &mem_rman;
		break;

	default:
		return NULL;
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return NULL;
	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return NULL;
		}
	}

	return rv;
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
			struct resource *r)
{
	/*
	 * If this is a memory resource, map it into the kernel.
	 */
	if (type == SYS_RES_MEMORY) {
		caddr_t vaddr;
		vm_paddr_t paddr;
		vm_size_t psize;

		paddr = rman_get_start(r);
		psize = rman_get_size(r);
		vaddr = pmap_mapdev(paddr, psize);
		rman_set_virtual(r, vaddr);
		rman_set_bushandle(r, (bus_space_handle_t)vaddr);
	}
	return rman_activate_resource(r);
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
			  struct resource *r)
{
	/*
	 * If this is a memory resource, unmap it.
	 */
	if (type == SYS_RES_MEMORY) {
		pmap_unmapdev((vm_offset_t)rman_get_virtual(r),
			      rman_get_size(r));
	}

	return rman_deactivate_resource(r);
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	if (rman_get_flags(r) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return rman_release_resource(r);
}

static int
nexus_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count, int cpuid)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list *rl = &ndev->nx_resources;

	/* XXX this should return a success/failure indicator */
	resource_list_add(rl, type, rid, start, start + count - 1, count,
	    cpuid);
	return 0;
}

static int
nexus_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list *rl = &ndev->nx_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;
	return 0;
}

static void
nexus_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct nexus_device *ndev = DEVTONX(child);
	struct resource_list *rl = &ndev->nx_resources;

	resource_list_delete(rl, type, rid);
}

/*
 * Setup an interrupt handler for a device.
 * This interfaces with the GIC driver to register the handler.
 */
static int
nexus_setup_intr(device_t bus, device_t child, struct resource *irq,
    int flags, void (*ihand)(void *), void *arg, void **cookiep,
    lwkt_serialize_t serializer, const char *desc)
{
	struct gic_irq_entry *entry;
	int irqnum;

	if (irq == NULL)
		panic("%s: NULL irq resource!", __func__);

	*cookiep = NULL;

	/*
	 * Get the IRQ number from the resource.
	 */
	irqnum = rman_get_start(irq);

	/*
	 * Activate the resource if needed.
	 */
	if ((rman_get_flags(irq) & RF_ACTIVE) == 0) {
		int error = rman_activate_resource(irq);
		if (error)
			return error;
	}

	/*
	 * Register with the GIC.
	 */
	entry = gic_register_irq(irqnum, ihand, arg, serializer);
	if (entry == NULL)
		return ENXIO;

	*cookiep = entry;
	return 0;
}

/*
 * Teardown an interrupt handler.
 */
static int
nexus_teardown_intr(device_t bus, device_t child, struct resource *irq,
    void *cookie)
{
	if (cookie != NULL) {
		gic_unregister_irq((struct gic_irq_entry *)cookie);
		return 0;
	}
	return EINVAL;
}
