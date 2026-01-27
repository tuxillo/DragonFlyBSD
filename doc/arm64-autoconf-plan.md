# ARM64 Autoconfiguration Plan

This document describes the plan for implementing device autoconfiguration on ARM64, including the minimal stubs needed for the MVP and the future FDT/ACPI work.

## Status: PHASE 1 COMPLETE ✅

**The ARM64 kernel now successfully boots to the `mountroot>` prompt.**

Phase 1 (minimal stubs with kenv-based VirtIO enumeration) is complete. The kernel reaches mountroot with VirtIO block device detection. FDT support (Phase 2) is **not required** for the current MVP - the kenv approach works for QEMU `-M virt`.

## Overview

DragonFly BSD's kernel boot process follows a sequence of SYSINIT stages. Phase 1 provides the minimal infrastructure to pass `SI_SUB_CONFIGURE` and enumerate devices via kernel environment variables.

### Boot Flow (Relevant Stages)

| Order | Constant | What Happens |
|-------|----------|--------------|
| 0x23C0000 | `SI_SUB_PRE_DRIVERS` | Watchdog init (last thing we see) |
| 0x2400000 | `SI_SUB_DRIVERS` | Driver module initialization |
| 0x3800000 | **`SI_SUB_CONFIGURE`** | **Device configuration - MISSING** |
| 0x4000000 | `SI_SUB_VFS` | VFS initialization |
| 0xB400000 | `SI_SUB_MOUNT_ROOT` | Mount root filesystem |

## Required Files

### 1. autoconf.c - Platform Autoconfiguration

**Location:** `sys/platform/arm64/aarch64/autoconf.c`

**Purpose:** Registers SYSINIT hooks at `SI_SUB_CONFIGURE` that trigger device enumeration.

**Key Functions:**
- `configure_first()` - SI_ORDER_FIRST - Early configuration (empty for now)
- `configure()` - SI_ORDER_THIRD - Calls `root_bus_configure()`
- `configure_final()` - SI_ORDER_ANY - Reinitializes console
- `cpu_rootconf()` - SI_SUB_ROOT_CONF - Root filesystem discovery

**Reference:** `sys/platform/pc64/x86_64/autoconf.c`

### 2. nexus.c - Root Nexus Driver

**Location:** `sys/platform/arm64/aarch64/nexus.c`

**Purpose:** The root bus driver that serves as attachment point for all buses and manages common resources (memory, interrupts).

**Key Functions:**
- `nexus_probe()` - Initializes resource managers
- `nexus_attach()` - Probes and attaches child devices
- `nexus_add_child()` - Adds child devices to the bus
- `nexus_alloc_resource()` - Allocates IRQ/memory resources
- `nexus_activate_resource()` - Maps memory resources into kernel

**Reference:** 
- DragonFly: `sys/platform/pc64/x86_64/nexus.c`
- FreeBSD: `.freebsd.orig/sys/arm64/arm64/nexus.c`

---

## Phase 1: Minimal Stubs (MVP)

The minimal implementation to unblock boot and reach `mountroot>`.

### autoconf.c (Stub)

```c
/*
 * ARM64 autoconfiguration support
 *
 * Triggers device enumeration at SI_SUB_CONFIGURE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/cons.h>

static void configure_first(void *);
static void configure(void *);
static void configure_final(void *);

SYSINIT(configure1, SI_SUB_CONFIGURE, SI_ORDER_FIRST, configure_first, NULL);
SYSINIT(configure2, SI_SUB_CONFIGURE, SI_ORDER_THIRD, configure, NULL);
SYSINIT(configure3, SI_SUB_CONFIGURE, SI_ORDER_ANY, configure_final, NULL);

cdev_t rootdev = NULL;
cdev_t dumpdev = NULL;

static void
configure_first(void *dummy)
{
    /* Placeholder for early configuration */
}

static void
configure(void *dummy)
{
    /*
     * This will configure all devices, starting with nexus.
     */
    root_bus_configure();

    safepri = TDPRI_KERN_USER;
}

static void
configure_final(void *dummy)
{
    cninit();
    cninit_finish();
}

void
cpu_rootconf(void)
{
    /* Root filesystem discovery - empty for now */
}
SYSINIT(cpu_rootconf, SI_SUB_ROOT_CONF, SI_ORDER_FIRST, cpu_rootconf, NULL);
```

### nexus.c (Stub)

```c
/*
 * ARM64 root nexus driver
 *
 * Serves as attachment point for buses and manages common resources.
 * This is a minimal stub - does not enumerate devices from FDT/ACPI yet.
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

static MALLOC_DEFINE(M_NEXUSDEV, "nexusdev", "Nexus device");

struct nexus_device {
    struct resource_list nx_resources;
};

#define DEVTONX(dev)    ((struct nexus_device *)device_get_ivars(dev))

static struct rman mem_rman;
static struct rman irq_rman;

static int nexus_probe(device_t);
static int nexus_attach(device_t);
static int nexus_print_child(device_t, device_t);
static device_t nexus_add_child(device_t, device_t, int, const char *, int);
static struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int, int);
static int nexus_activate_resource(device_t, device_t, int, int, struct resource *);
static int nexus_deactivate_resource(device_t, device_t, int, int, struct resource *);
static int nexus_release_resource(device_t, device_t, int, int, struct resource *);

static device_method_t nexus_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,      bus_generic_identify),
    DEVMETHOD(device_probe,         nexus_probe),
    DEVMETHOD(device_attach,        nexus_attach),
    DEVMETHOD(device_detach,        bus_generic_detach),
    DEVMETHOD(device_shutdown,      bus_generic_shutdown),
    DEVMETHOD(device_suspend,       bus_generic_suspend),
    DEVMETHOD(device_resume,        bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_print_child,      nexus_print_child),
    DEVMETHOD(bus_add_child,        nexus_add_child),
    DEVMETHOD(bus_alloc_resource,   nexus_alloc_resource),
    DEVMETHOD(bus_activate_resource, nexus_activate_resource),
    DEVMETHOD(bus_deactivate_resource, nexus_deactivate_resource),
    DEVMETHOD(bus_release_resource, nexus_release_resource),

    DEVMETHOD_END
};

static driver_t nexus_driver = {
    "nexus",
    nexus_methods,
    1,  /* no softc */
};

static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, NULL, NULL);

static int
nexus_probe(device_t dev)
{
    device_quiet(dev);

    /* Initialize memory resource manager */
    mem_rman.rm_start = 0;
    mem_rman.rm_end = ~0ul;
    mem_rman.rm_type = RMAN_ARRAY;
    mem_rman.rm_descr = "I/O memory addresses";
    if (rman_init(&mem_rman, -1) ||
        rman_manage_region(&mem_rman, 0, ~0ul))
        panic("nexus_probe mem_rman");

    /* Initialize IRQ resource manager */
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
     * Attach child devices. In the future, this will enumerate
     * devices from FDT or ACPI. For now, just call generic attach.
     */
    bus_generic_attach(dev);
    return 0;
}

static int
nexus_print_child(device_t bus, device_t child)
{
    int retval = 0;

    retval += bus_print_child_header(bus, child);
    retval += kprintf(" on motherboard\n");

    return retval;
}

static device_t
nexus_add_child(device_t bus, device_t parent, int order,
    const char *name, int unit)
{
    device_t child;
    struct nexus_device *ndev;

    ndev = kmalloc(sizeof(*ndev), M_NEXUSDEV, M_INTWAIT | M_ZERO);
    resource_list_init(&ndev->nx_resources);

    child = device_add_child_ordered(parent, order, name, unit);
    device_set_ivars(child, ndev);

    return child;
}

static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags, int cpuid)
{
    struct rman *rm;
    struct resource *rv;
    int needactivate = flags & RF_ACTIVE;

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
    if (type == SYS_RES_MEMORY) {
        void *vaddr;
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
    if (type == SYS_RES_MEMORY) {
        pmap_unmapdev((vm_offset_t)rman_get_virtual(r), rman_get_size(r));
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
```

### Build System Update

Add to `sys/platform/arm64/conf/files`:
```
platform/arm64/aarch64/autoconf.c   standard
platform/arm64/aarch64/nexus.c      standard
```

### Expected Result

After implementing Phase 1:
1. Kernel boots through `SI_SUB_CONFIGURE`
2. Nexus driver attaches (no children yet)
3. Boot continues to `SI_SUB_MOUNT_ROOT`
4. Kernel reaches `mountroot>` prompt
5. Mount fails (no disk driver) - but this is progress!

---

## Phase 2: FDT Support (FUTURE - Not Required for MVP)

**NOTE:** Phase 2 (FDT) is NOT required for the current MVP. The kernel successfully boots to mountroot using the kenv-based VirtIO MMIO enumeration approach described in `arm64-virtio-mmio-v1-plan.md`.

**Current Status:** Phase 1 is complete. The kernel boots through all SYSINITs, detects VirtIO block device (`vbd0`), and reaches `mountroot>` prompt.

FDT support would be beneficial for:
- Real hardware (not QEMU)
- PCIe enumeration
- Dynamic device detection
- Full QEMU virt machine support

For now, the kenv approach is sufficient for development and testing.

### **Phase 2A: Port OFW/FDT Infrastructure**

1. **Create `sys/dev/ofw/` directory** with FreeBSD OFW code
2. **Port essential files** from `.freebsd.orig/sys/dev/ofw/`:
   - `ofw_bus.c` - Core OFW bus implementation
   - `ofw_bus_if.m` - OFW bus interface
   - `ofw_bus_subr.c` - Helper functions
   - `ofw_subr.c` - FDT parsing utilities
   - `ofw_bus.h`, `ofw_subr.h` - Headers

### **Phase 2B: Create ofwbus Driver**

1. **Create `sys/dev/ofw/ofwbus.c`** - Root OFW bus driver
2. **Implement `ofwbus_probe()`** - Detects FDT presence
3. **Implement `ofwbus_attach()`** - Walks FDT, creates child devices
4. **Add to build system** - Update `sys/conf/files`

### **Phase 2C: Create simplebus Driver**

1. **Create `sys/dev/fdt/simplebus.c`** - Handles "simple-bus" nodes
2. **Implement `simplebus_probe()`** - Matches "simple-bus" compatible
3. **Implement `simplebus_attach()`** - Enumerates child nodes
4. **Add to build system**

### **Phase 2D: Integrate FDT into Nexus**

1. **Modify `sys/platform/arm64/aarch64/nexus.c`**:
   - Add FDT detection in `nexus_attach()`
   - Add `nexus_add_child(dev, 10, "ofwbus", 0)` when FDT present
   - Add OFW bus interface methods
2. **Add FDT headers** - Include `dev/ofw/ofw_bus.h`

### **Phase 2E: Add Minimal VirtIO Support (Optional)**

1. **Create `sys/dev/virtio/virtio_mmio.c`** - MMIO transport
2. **Add `sys/dev/virtio/block/virtio_blk.c`** - Block device driver
3. **Add to build system**

### **Phase 2F: Testing and Validation**

1. **Run arm64-port-testing agent** to build and test
2. **Verify FDT parsing** - Check for device enumeration
3. **Test with QEMU virtio-blk** - Try to reach `mountroot>`

### **Detailed File Changes**

#### **New Files to Create:**
```
sys/dev/ofw/ofw_bus.c           (from FreeBSD)
sys/dev/ofw/ofw_bus_if.m        (from FreeBSD)  
sys/dev/ofw/ofw_bus_subr.c      (from FreeBSD)
sys/dev/ofw/ofw_subr.c          (from FreeBSD)
sys/dev/ofw/ofw_bus.h           (from FreeBSD)
sys/dev/ofw/ofw_subr.h          (from FreeBSD)
sys/dev/ofw/ofwbus.c            (new - root OFW bus)
sys/dev/fdt/simplebus.c         (from FreeBSD)
```

#### **Files to Modify:**
```
sys/platform/arm64/aarch64/nexus.c  (add FDT integration)
sys/conf/files                      (add new drivers)
sys/platform/arm64/conf/files       (add new drivers)
```

#### **Build System Updates:**
Add to `sys/conf/files`:
```
dev/ofw/ofw_bus.c          optional ofw
dev/ofw/ofw_bus_if.m       optional ofw
dev/ofw/ofw_bus_subr.c     optional ofw
dev/ofw/ofw_subr.c         optional ofw
dev/ofw/ofwbus.c           optional ofw
dev/fdt/simplebus.c        optional fdt
```

### **Implementation Order**

1. **First:** Port OFW infrastructure (Phase 2A) - Essential for FDT parsing
2. **Second:** Create ofwbus driver (Phase 2B) - Walks device tree
3. **Third:** Create simplebus driver (Phase 2C) - Handles bus nodes
4. **Fourth:** Integrate into nexus (Phase 2D) - Connect to autoconfig
5. **Fifth:** Test with arm64-port-testing agent
6. **Optional:** Add VirtIO drivers if needed for storage

### **Device Enumeration Flow (FDT)**

```
root_bus
  └── nexus0
        └── ofwbus0  (walks FDT)
              ├── gic0 (interrupt controller)
              ├── uart0 (pl011)
              ├── pcie0 (PCIe host bridge)
              │     └── virtio-blk, virtio-net, etc.
              └── virtio_mmio0..31 (VirtIO MMIO devices)
```

### **FreeBSD Reference**

```c
/* From .freebsd.orig/sys/arm64/arm64/nexus.c */

static int
nexus_fdt_attach(device_t dev)
{
    nexus_add_child(dev, 10, "ofwbus", 0);
    return (nexus_attach(dev));
}
```

---

## Phase 3: ACPI Support (Future)

Some ARM64 servers use ACPI instead of FDT. This is lower priority than FDT.

### Required Components

1. **ACPICA Port**
   - The ACPI Component Architecture library
   - Already partially present in DragonFly

2. **ACPI Bus Driver**
   - Enumerates devices from ACPI tables
   - Location: `sys/dev/acpica/`

3. **Nexus ACPI Integration**
   - Modify `nexus_attach()` to add `acpi` as child when ACPI detected

### FreeBSD Reference

```c
/* From .freebsd.orig/sys/arm64/arm64/nexus.c */

static int
nexus_acpi_attach(device_t dev)
{
    nexus_add_child(dev, 10, "acpi", 0);
    return (nexus_attach(dev));
}
```

---

## Implementation Status

| Aspect | Phase 1 (Stubs) - **COMPLETE** | Phase 2 (FDT) - **FUTURE** | Phase 3 (ACPI) - **FUTURE** |
|--------|-----------------|------------------|------------------|
| **Device enumeration** | kenv-based (VirtIO MMIO) | FDT parsing from QEMU virt | ACPI table parsing |
| **Child devices** | VirtIO via kenv | Enumerated from FDT | Enumerated from ACPI |
| **Interrupt mapping** | GIC with kenv IRQs | GIC integration via FDT | APIC integration via ACPI |
| **Resource discovery** | kenv variables | From device tree | From ACPI tables |
| **Boot result** | ✅ Reaches `mountroot>` | Full FDT support | Full server support |
| **Current status** | ✅ Working | 📋 Optional | 📋 Planned |

### **Phase 1 Verification (COMPLETE)**
- ✅ `configure_first()`, `configure()`, `configure_final()` execute in order
- ✅ `nexus_probe()` and `nexus_attach()` successful
- ✅ Kernel passes `SI_SUB_CONFIGURE` (0x3800000)
- ✅ VirtIO MMIO devices enumerated via kenv
- ✅ VirtIO block driver attaches (`vbd0` detected)
- ✅ Kernel reaches `mountroot>` prompt

### **How Devices Are Enumerated (kenv approach)**

Instead of FDT, the ARM64 port uses kernel environment variables set by the loader:
```
hw.virtio.mmio.device=0x200@0x0a000000:48
hw.virtio.mmio.device_1=0x200@0x0a000200:49
...
hw.virtio.mmio.device_31=0x200@0x0a003e00:79
```

The `virtio_mmio_kenv` driver parses these variables and creates child devices under nexus.
See `arm64-virtio-mmio-v1-plan.md` for full details.

---

## Testing

### Phase 1 Status (COMPLETE)

**Current State:** Phase 1 is implemented and working:
- ✅ Kernel boots through all SYSINITs
- ✅ Nexus driver attaches successfully
- ✅ VirtIO MMIO devices enumerated via kenv
- ✅ VirtIO block device attached (`vbd0`)
- ✅ Kernel reaches `mountroot>` prompt

**Test Command:**
```bash
# In tools/arm64-test/
make copy-all    # Get artifacts from VM
make test        # Run with timeout

# Output (abbreviated):
# DragonFly/arm64 kernel started!
# Console initialized via PL011 driver.
# ...
# virtio_mmio31: <VirtIO MMIO adapter> at mem 0xa003e00-0xa003fff irq 79 on motherboard
# virtio_blk0: <VirtIO Block Adapter> on virtio_mmio31
# vtblk0: 67108864 byte (64MB) block device
# vbd0 at vtblk0
# ...
# mountroot>
```

---

## References

- DragonFly x86_64 autoconf: `sys/platform/pc64/x86_64/autoconf.c`
- DragonFly x86_64 nexus: `sys/platform/pc64/x86_64/nexus.c`
- FreeBSD ARM64 nexus: `.freebsd.orig/sys/arm64/arm64/nexus.c`
- FreeBSD OFW bus: `.freebsd.orig/sys/dev/ofw/`
- FreeBSD simplebus: `.freebsd.orig/sys/dev/fdt/simplebus.c`

---

## **Current Status Summary**

**Phase 1: COMPLETE** ✅ - Minimal autoconfiguration with kenv-based VirtIO MMIO enumeration is implemented and working. The kernel boots to `mountroot>` prompt with VirtIO block device detection.

**Phase 2 (FDT): NOT REQUIRED FOR MVP** - The kenv approach successfully provides device enumeration for QEMU `-M virt`. FDT support may be added in the future for:
- Real ARM64 hardware support
- PCIe device enumeration
- Dynamic device detection

**Phase 3 (ACPI): FUTURE** - For ARM64 server platforms.

**MVP Status:** The ARM64 kernel successfully boots to `mountroot>` prompt using the kenv-based device enumeration approach. No FDT support was needed.

---
*Last updated: 2026-01-28 (Phase 1 complete, kernel boots to mountroot)*
