# DragonFly ARM64 VirtIO MMIO v1 (Legacy) Plan

## Purpose

Implement VirtIO MMIO legacy (v1) transport support on DragonFly ARM64 so QEMU
`-M virt` can boot with VirtIO MMIO devices without FDT. Use the FreeBSD
snapshot under `.freebsd.orig` as a reference, while conforming to DragonFly's
virtio bus APIs and conventions.

## Scope and Constraints

- Support VirtIO MMIO **v1 (legacy) only**.
- **Do not implement FDT** (enumeration is via kernel environment variables).
- Follow DragonFly's existing VirtIO stack and bus interface.
- Keep changes minimal and targeted to VirtIO MMIO.
- Use `.freebsd.orig` for reference without changing DragonFly directory layout.
- Do not implement or stub vkernel functionality.

## Implementation Status

Implemented:
- MMIO transport driver (legacy v1) and kenv identify shim.
- DragonFly virtio bus interface methods for MMIO (features, queues, notify, interrupts).
- Build integration for MMIO sources.
- Plan reference in `doc/arm64-efi-loader-mvp-part1.md`.

Pending:
- Kernel build/test on ARM64: blocked by missing `machine/bus.h` in arm64 headers.

## Naming (DragonFly-Specific)

- Kernel environment variables for enumeration:
  - `hw.virtio.mmio.device`
  - `hw.virtio.mmio.device_1` ... `hw.virtio.mmio.device_9999`
  - Value format: `<size>@<baseaddr>:<irq>[:<unit>]`
- Identify shim module name: `virtio_mmio_kenv`
- Transport attachment name: `virtio_mmio` (device added under `nexus`)

## Reference Sources

FreeBSD snapshot (read-only reference):
- `.freebsd.orig/sys/dev/virtio/mmio/virtio_mmio.c`
- `.freebsd.orig/sys/dev/virtio/mmio/virtio_mmio.h`
- `.freebsd.orig/sys/dev/virtio/mmio/virtio_mmio_cmdline.c`

DragonFly VirtIO PCI transport and core (pattern to follow):
- `sys/dev/virtual/virtio/pci/virtio_pci.c`
- `sys/dev/virtual/virtio/virtio/virtio.c`
- `sys/dev/virtual/virtio/virtio/virtqueue.[ch]`
- `sys/dev/virtual/virtio/virtio/virtio_bus_if.m`

ARM64 nexus enumeration path (no FDT):
- `sys/platform/arm64/aarch64/nexus.c`

## Driver Architecture

Create a new MMIO transport driver modeled after `virtio_pci`, implementing
the DragonFly `virtio_bus_if` methods and using DragonFly virtqueue APIs.

### New Files (DragonFly)

- `sys/dev/virtual/virtio/mmio/virtio_mmio.c`
- `sys/dev/virtual/virtio/mmio/virtio_mmio.h`
- `sys/dev/virtual/virtio/mmio/virtio_mmio_kenv.c`

### Existing Files to Modify

- `sys/conf/files` (add new MMIO transport and kenv identify sources)
- ARM64 kernel config as required to include the new drivers
- `doc/arm64-efi-loader-mvp-part1.md` (update to reference this plan)

## Transport Driver Details (virtio_mmio)

### Probe

1. Allocate `SYS_RES_MEMORY` rid 0 (MMIO window).
2. Read `VIRTIO_MMIO_MAGIC_VALUE` and require `VIRTIO_MMIO_MAGIC_VIRT`.
3. Read `VIRTIO_MMIO_VERSION` and require `== 1` (legacy only).
4. Read `VIRTIO_MMIO_DEVICE_ID` and require non-zero.
5. Release memory resource and return `BUS_PROBE_DEFAULT`.

### Attach

1. Allocate `SYS_RES_MEMORY` rid 0, store in softc.
2. Read and store `vtmmio_version` (expect 1).
3. Reset device (write `VIRTIO_CONFIG_STATUS_RESET`).
4. Set status `VIRTIO_CONFIG_STATUS_ACK`.
5. Add a single child with `device_add_child(dev, NULL, -1)`.
6. Probe/attach child similarly to `virtio_pci`:
   - Set status `DRIVER`.
   - `device_probe_and_attach(child)`.
   - On failure, set status `FAILED`, reset, release resources, set status `ACK`.
   - On success, set status `DRIVER_OK`.

### Ivars

Implement `bus_read_ivar` and `bus_write_ivar` for:

- `VIRTIO_IVAR_DEVTYPE` -> `VIRTIO_MMIO_DEVICE_ID`.
- `VIRTIO_IVAR_FEATURE_DESC` -> store pointer for `virtio_describe()`.

### Feature Negotiation

Use DragonFly `virtqueue_filter_features()`:

1. Read host features:
   - Write `VIRTIO_MMIO_HOST_FEATURES_SEL = 1`, read upper 32 bits.
   - Write `VIRTIO_MMIO_HOST_FEATURES_SEL = 0`, read lower 32 bits.
2. Compute `features = host & child_features`.
3. Filter with `virtqueue_filter_features(features)`.
4. Cache `features` in softc.
5. Write guest features using `VIRTIO_MMIO_GUEST_FEATURES_SEL` (1 then 0).

### Virtqueue Allocation (Legacy v1)

For each queue index:

1. Select queue: `VIRTIO_MMIO_QUEUE_SEL`.
2. Read max size: `VIRTIO_MMIO_QUEUE_NUM_MAX`.
3. Call DragonFly `virtqueue_alloc(dev, idx, size, VIRTIO_MMIO_VRING_ALIGN,
   ~(vm_paddr_t)0, info, &vq)`.
4. Program v1 registers:
   - `VIRTIO_MMIO_GUEST_PAGE_SIZE = PAGE_SIZE`.
   - `VIRTIO_MMIO_QUEUE_NUM = size`.
   - `VIRTIO_MMIO_QUEUE_ALIGN = VIRTIO_MMIO_VRING_ALIGN` (4096).
   - `VIRTIO_MMIO_QUEUE_PFN = virtqueue_paddr(vq) >> PAGE_SHIFT`.

Note: do not program v2 descriptors or READY register.

### Notify

Implement `notify_vq` by writing queue index to
`VIRTIO_MMIO_QUEUE_NOTIFY`.

### Device Config Read/Write

Use `VIRTIO_MMIO_CONFIG + offset`.

- For v1, follow DragonFly practice (byte-wise access) unless alignment
  guarantees are explicit.
- No `config_generation` handling for v1.

### Stop / Reinit

Mirror `virtio_pci` semantics:

- `stop`: reset the device.
- `reinit`: set ACK/DRIVER, renegotiate features, reprogram queues and PFNs.
- `reinit_complete`: set `DRIVER_OK`.

## Interrupt Model (DragonFly VirtIO Bus Interface)

DragonFly virtio child drivers use:

- `virtio_intr_count()`
- `virtio_intr_alloc()`
- `virtio_setup_intr()`
- `virtio_bind_intr()` / `virtio_unbind_intr()`

Implement a simple single-IRQ model for MMIO:

1. `intr_count` returns 1.
2. `intr_alloc` clamps `*cnt = 1` and allocates `SYS_RES_IRQ` rid 0.
3. `setup_intr` installs an interrupt handler on that resource.
4. `bind_intr` registers handlers by `what`:
   - `what == -1` -> config change handler.
   - `what >= 0` -> virtqueue handler.
   - Use a small list of `vqentry` entries like `virtio_pci` does.
5. `unbind_intr` removes the handler.

Interrupt handler behavior:

- Read `VIRTIO_MMIO_INTERRUPT_STATUS`.
- Write same value to `VIRTIO_MMIO_INTERRUPT_ACK`.
- If config bit set, invoke config handler (if bound).
- If vring bit set, invoke all bound virtqueue handlers.

This mirrors DragonFly’s PCI legacy handler behavior and keeps the
virtqueue pending checks in the child driver.

## Enumeration (KenV Identify Driver)

Create `virtio_mmio_kenv` as a `device_identify` driver under `nexus`:

1. Read `hw.virtio.mmio.device` and `hw.virtio.mmio.device_N`.
2. Parse `<size>@<baseaddr>:<irq>[:<unit>]`.
3. Add child with `BUS_ADD_CHILD(parent, 0, "virtio_mmio", unit_or_-1)`.
4. Set resources:
   - `bus_set_resource(child, SYS_RES_MEMORY, 0, baseaddr, size)`
   - `bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1)`
5. Optionally set driver explicitly for the child.

Parsing should follow FreeBSD cmdline logic, updated for DragonFly `kgetenv()`
and `kfreeenv()`.

## Build Integration

1. Add new MMIO transport and kenv identify sources to `sys/conf/files`.
2. Update ARM64 kernel config to include:
   - `device virtio`
   - `device virtio_mmio`
   - `device virtio_mmio_kenv`
   - `device virtio_blk`

## QEMU Usage (Legacy Only)

Use `force-legacy=on` to ensure MMIO version 1.

Example (block device):

- QEMU device:
  - `-device virtio-mmio,force-legacy=on,irq=48,addr=0x0a000000`
  - `-device virtio-blk-device,drive=vd0`

- DragonFly kenv:
  - `hw.virtio.mmio.device="0x200@0x0a000000:48"`

Note: address/irq values must match what QEMU assigns; verify with QEMU
monitor or logs.

## Risk Notes

- v1-only: driver must fail attach if `VIRTIO_MMIO_VERSION != 1`.
- No modern queue support: do not use `QUEUE_DESC/AVAIL/USED` registers.
- Child drivers must continue to use DragonFly virtio bus API.
- Keep MMIO config access conservative (byte or aligned access only).

## Implementation Checklist (Ordered)

1. Done: add `sys/dev/virtual/virtio/mmio/` directory and `virtio_mmio.[ch]`.
2. Done: add `virtio_mmio_kenv.c` identify shim with DragonFly kenv parsing.
3. Done: wire up `virtio_bus_if` methods to the new transport driver.
4. Done: implement legacy virtqueue setup via PFN + ALIGN + PAGE_SIZE.
5. Done: implement single-IRQ interrupt handling and binding lists.
6. Done: update `sys/conf/files` and ARM64 kernel config.
7. Done: update `doc/arm64-efi-loader-mvp-part1.md` to reference this plan.
8. Done: add arm64 `machine/bus.h` support.
9. Done: fix compilation issues (kprintf, strict-aliasing).
10. Done: loader integration to set kenv variables for all 32 VirtIO MMIO slots.
11. Done: test harness integration with VirtIO block device.
12. **In Progress:** virtio_blk attachment - adapter attaches but child driver investigation needed.

---

## Blocker: Missing ARM64 Bus Headers

### Problem

The VirtIO MMIO driver includes `<machine/bus.h>` and `<machine/resource.h>`,
which do not exist for the ARM64 platform. The kernel build fails because
these machine-specific headers are missing.

### Analysis

**DragonFly bus architecture differs from FreeBSD:**

- **FreeBSD** uses a `struct bus_space` with function pointers for all bus
  operations. The `bus_space_tag_t` is a pointer to this structure, and macros
  like `bus_space_read_1()` call through the function pointers.

- **DragonFly** uses a simpler model with direct inline functions:
  - `sys/cpu/*/include/bus_dma.h`: Contains typedefs (`bus_addr_t`,
    `bus_space_tag_t`, etc.) and inline bus space accessor functions.
  - `sys/sys/bus.h`: Contains higher-level `bus_read_*`/`bus_write_*` macros
    that wrap the `bus_space_*` functions.
  - `sys/sys/bus_resource.h`: Contains `SYS_RES_*` constants.

**Current state:**
- `sys/cpu/aarch64/include/bus_dma.h` exists but is a placeholder with only
  typedefs and constants; no inline accessor functions.
- `sys/platform/arm64/include/bus.h` does not exist.
- `sys/platform/arm64/include/resource.h` does not exist.

### Solution

Create ARM64-specific headers that follow DragonFly conventions:

1. **Expand `sys/cpu/aarch64/include/bus_dma.h`** to include inline bus space
   accessor functions similar to x86_64's version, but simplified for ARM64
   (memory-only, no I/O port space).

2. **Create `sys/platform/arm64/include/bus.h`** that includes the cpu-level
   `bus_dma.h` and provides any platform-specific definitions.

3. **Create `sys/platform/arm64/include/resource.h`** that includes
   `sys/sys/bus_resource.h` for `SYS_RES_*` constants.

### ARM64 Bus Space Notes

ARM64 has only memory-mapped I/O (no separate I/O port address space like
x86). The implementation is simpler:
- `bus_space_tag_t` is unused (can be 0 or ignored)
- `bus_space_handle_t` is a virtual address
- All accessors perform direct volatile pointer dereferences
- Memory barriers use ARM64 `dsb` instruction instead of x86 `lock; addl`

---

## Loader Integration

### Goal

The EFI loader must set kernel environment variables for VirtIO MMIO device
discovery. Since we're not implementing FDT parsing, the loader hardcodes
the QEMU `-M virt` VirtIO MMIO device addresses.

### Status: COMPLETE ✅

The loader now registers all 32 QEMU VirtIO MMIO slots. QEMU assigns devices
from the **highest slot downward**, so with one virtio-blk-device, it appears
at slot 31 (address `0x0a003e00`, IRQ 79).

### QEMU `-M virt` VirtIO MMIO Memory Map

QEMU's ARM64 virt machine uses fixed addresses for VirtIO MMIO devices:

| Device # | Base Address | Size | IRQ (GIC SPI) |
|----------|--------------|------|---------------|
| 0 | `0x0a000000` | 0x200 | 48 (SPI 16) |
| 1 | `0x0a000200` | 0x200 | 49 (SPI 17) |
| 2 | `0x0a000400` | 0x200 | 50 (SPI 18) |
| ... | +0x200 each | 0x200 | +1 each |
| 31 | `0x0a003e00` | 0x200 | 79 (SPI 47) |

**Important:** QEMU populates devices from slot 31 downward, not from slot 0!

### Loader Implementation

**File:** `stand/boot/efi/loader/main.c`

The loader registers all 32 VirtIO MMIO slots:

```c
#ifdef __aarch64__
    /*
     * QEMU -M virt provides 32 VirtIO MMIO device slots at fixed addresses.
     * Each slot is 0x200 bytes starting at 0x0a000000.
     * IRQs are 48-79 (SPI 16-47 in GIC terminology).
     *
     * QEMU assigns devices to slots from the HIGHEST slot downward, so
     * with one virtio-blk-device, it will be at slot 31 (0x0a003e00, IRQ 79).
     *
     * We register all 32 slots and let the kernel probe each one.
     * Empty slots will have device_id=0 and probe will skip them.
     */
    {
        char varname[40];
        char varval[40];
        int i;
        
        /* First device uses hw.virtio.mmio.device */
        setenv("hw.virtio.mmio.device", "0x200@0x0a000000:48", 1);
        
        /* Remaining devices use hw.virtio.mmio.device_N */
        for (i = 1; i < 32; i++) {
            snprintf(varname, sizeof(varname),
                "hw.virtio.mmio.device_%d", i);
            snprintf(varval, sizeof(varval),
                "0x200@0x%x:%d", 0x0a000000 + (i * 0x200), 48 + i);
            setenv(varname, varval, 1);
        }
    }
#endif
```

### Commits

| Commit | Description |
|--------|-------------|
| `512279a2bb` | arm64: Add VirtIO MMIO device integration for QEMU testing (single slot) |
| `f4c691c843` | arm64/loader: Register all 32 QEMU VirtIO MMIO slots |

---

## Test Harness Integration

### Goal

Update the QEMU test harness to include a VirtIO block device so the kernel
driver can be tested end-to-end.

### Status: COMPLETE ✅

The test harness now includes VirtIO block device support with automatic test
disk creation.

### QEMU Changes

**File:** `tools/arm64-test/Makefile`

VirtIO block device configuration:

```makefile
# VirtIO test disk for testing virtio-blk driver
VIRTIO_DISK     := $(VM_DIR)/virtio-test.qcow2

# VirtIO MMIO block device arguments
QEMU_VIRTIO     = -device virtio-blk-device,drive=vd0 \
                  -drive id=vd0,file=$(VIRTIO_DISK),format=qcow2,if=none

# Create VirtIO test disk image (64MB qcow2)
$(VIRTIO_DISK):
	@echo "Creating VirtIO test disk at $(VIRTIO_DISK)..."
	@mkdir -p $(VM_DIR)
	qemu-img create -f qcow2 $(VIRTIO_DISK) 64M
```

### Current Kernel Output

The VirtIO MMIO adapter successfully attaches at slot 31:

```
vtmmio_kenv_identify: called, checking kenv
vtmmio_kenv_identify: found hw.virtio.mmio.device=0x200@0x0a000000:48
vtmmio_parsearg: adding child virtio_mmio at 0xa000000 size 0x200 irq 48
vtmmio_parsearg: child added successfully
[... slots 0-30 probe with device_id=0, skipped ...]
vtmmio_probe: called for virtio_mmio31
vtmmio_probe: memory resource allocated
vtmmio_probe: magic=0x74726976 (expected 0x74726976)
vtmmio_probe: version=1 (expected 1)
vtmmio_probe: device detected, releasing memory for attach
virtio_mmio31: <VirtIO MMIO adapter> at mem 0xa003e00-0xa003fff irq 79 on motherboard
```

### Next Steps

The VirtIO MMIO adapter (`virtio_mmio31`) successfully attaches, but we need
to investigate whether the `virtio_blk` child driver is probing and attaching.
The attach log should show:

```
virtio_blk0: <VirtIO Block Adapter> on virtio_mmio31
vtblk0: 64MB block device
```

If these messages don't appear, investigate:
1. Is `vtmmio_attach()` calling `device_add_child()` and `device_probe_and_attach()`?
2. Is the virtio_blk driver being loaded in the kernel config?
3. Are there errors during child device negotiation?
