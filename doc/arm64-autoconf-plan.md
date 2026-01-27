# ARM64 Autoconfiguration Plan

This document describes the plan for implementing device autoconfiguration on ARM64, including the minimal stubs needed for the MVP and the future FDT/ACPI work.

## Overview

DragonFly BSD's kernel boot process follows a sequence of SYSINIT stages. The boot currently stalls after `SI_SUB_PRE_DRIVERS` (watchdog init) because ARM64 is missing the infrastructure that runs at `SI_SUB_CONFIGURE`.

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

## Phase 2: FDT Support Implementation

**Current Status:** Phase 1 is complete - autoconf.c and nexus.c are implemented, kernel boots through `SI_SUB_CONFIGURE` but stalls at USB delay loop because nexus has no children (no device enumeration).

**Blocking Issue:** Kernel can't reach `mountroot>` because no storage devices are attached. QEMU virt machine provides devices via FDT, but we haven't implemented FDT parsing.

**Goal:** Implement FDT support to enumerate devices and reach `mountroot>` prompt.

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

| Aspect | Phase 1 (Stubs) - **COMPLETE** | Phase 2 (FDT) - **IN PROGRESS** | Phase 3 (ACPI) - **FUTURE** |
|--------|-----------------|------------------|------------------|
| **Device enumeration** | None | FDT parsing from QEMU virt | ACPI table parsing |
| **Child devices** | None attached | Enumerated from FDT | Enumerated from ACPI |
| **Interrupt mapping** | Basic rman | GIC integration via FDT | APIC integration via ACPI |
| **Resource discovery** | Manual | From device tree | From ACPI tables |
| **Boot result** | Stalls at USB loop | Reaches `mountroot>` prompt | Full server support |
| **Current status** | ✅ Working | 🔄 Implementation needed | 📋 Planned |

### **Phase 1 Verification (Completed)**
- ✅ `configure_first()`, `configure()`, `configure_final()` execute in order
- ✅ `nexus_probe()` and `nexus_attach()` successful
- ✅ Kernel passes `SI_SUB_CONFIGURE` (0x3800000)
- ✅ Reaches `SI_SUB_INT_CONFIG_HOOKS` (0xa800000)
- ❌ No storage drivers (nexus has no children)

### **Phase 2 Requirements**
- 🔄 Port OFW/FDT infrastructure from FreeBSD
- 🔄 Create `ofwbus` driver for FDT walking
- 🔄 Create `simplebus` driver for "simple-bus" nodes
- 🔄 Integrate FDT into nexus driver
- 🔄 Add VirtIO drivers for storage (optional)

---

## Testing

### Phase 1 Status (COMPLETE)

**Current State:** Phase 1 is implemented and working:
- ✅ Kernel boots through `SI_SUB_CONFIGURE` (0x3800000)
- ✅ Nexus driver attaches successfully
- ✅ Boot reaches `SI_SUB_INT_CONFIG_HOOKS` (0xa800000)
- ❌ Stalls at USB delay loop (no storage drivers attached)

**Test Command:**
```bash
# In tools/arm64-test/
make copy-all    # Get artifacts from VM
make test        # Run with timeout

# Expected output:
# ... boot messages ...
# nexus0: <Motherboard> on motherboard
# (stalls at USB delay loop - no devices enumerated)
```

### Phase 2 Test (Implementation Required)

**Goal:** Test FDT device enumeration and reach `mountroot>` prompt.

**Test Setup:**
```bash
# Add virtio disk to QEMU for testing storage
qemu-system-aarch64 -M virt -cpu cortex-a72 -m 512M \
    -drive if=pflash,file=QEMU_EFI.fd,format=raw,readonly=on \
    -drive if=pflash,file=QEMU_VARS.fd,format=raw \
    -drive file=fat:rw:esp,format=raw \
    -drive file=disk.img,if=none,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -nographic -serial stdio

# Expected after Phase 2:
# nexus0: <Motherboard> on motherboard
# ofwbus0: <Open Firmware Device Tree> on nexus0
# simplebus0: <Flattened device tree simple bus> on ofwbus0
# gic0: <ARM Generic Interrupt Controller> on simplebus0
# uart0: <ARM PL011 UART> on simplebus0
# virtio_blk0: <VirtIO Block Device> on virtio_mmio0
# mountroot>
```

**Testing Strategy:**
1. **After Phase 2A/B:** Test FDT parsing - verify `ofwbus0` attaches
2. **After Phase 2C:** Test simplebus - verify child devices appear
3. **After Phase 2D:** Test full integration - verify device tree walk
4. **After Phase 2E:** Test storage - try to reach `mountroot>`

---

## References

- DragonFly x86_64 autoconf: `sys/platform/pc64/x86_64/autoconf.c`
- DragonFly x86_64 nexus: `sys/platform/pc64/x86_64/nexus.c`
- FreeBSD ARM64 nexus: `.freebsd.orig/sys/arm64/arm64/nexus.c`
- FreeBSD OFW bus: `.freebsd.orig/sys/dev/ofw/`
- FreeBSD simplebus: `.freebsd.orig/sys/dev/fdt/simplebus.c`

---

## **Current Status Summary**

**Phase 1: COMPLETE** - Minimal autoconfiguration stubs are implemented and working. Kernel boots through `SI_SUB_CONFIGURE` but stalls at USB delay loop because no devices are enumerated.

**Phase 2: READY FOR IMPLEMENTATION** - Detailed plan created for FDT support. Next step is to port OFW infrastructure from FreeBSD and implement device enumeration from QEMU's Flattened Device Tree.

**Blocking Issue:** Kernel cannot reach `mountroot>` because nexus has no child devices. FDT support is required to enumerate VirtIO devices from QEMU virt machine.

**Next Action:** Begin Phase 2A - Port OFW/FDT infrastructure from `.freebsd.orig/sys/dev/ofw/`.

---
*Last updated: 2026-01-27 (Phase 2 plan consolidated)*
