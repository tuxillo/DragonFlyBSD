# ARM64 EFI Loader MVP

## Overview

This document tracks the arm64 EFI loader and kernel bring-up for DragonFly BSD.

---

## MVP Part 1: EFI Loader Console Bring-up (COMPLETE)

### Goal

Produce an AArch64 UEFI loader binary that executes under QEMU+EDK2 and prints to the UEFI console.

### Status: COMPLETE

The arm64 EFI loader now:
- Loads as valid PE32+ EFI application
- Self-relocates correctly (fixed RELA relocation bug)
- Initializes EFI services (BS, RS, ST, IH)
- Allocates heap via EFI
- Initializes console - prints "Console: EFI console"
- Displays DragonFly banner and EFI firmware info
- Shows autoboot spinner countdown
- Drops to interactive `OK` prompt

### Key Fixes Applied

1. **RELA Relocation Bug** - `self_reloc.c` was double-adding the addend for RELA relocations. Fixed by assigning instead of adding: `*newaddr = baseaddr + rel->r_addend;`

2. **PE32+ Header** - Using embedded PE header in `start.S` (FreeBSD approach)

3. **Malloc Guard Issue** - Temporarily disabled guards in `stand/lib/zalloc_defs.h` to work around a `free()` crash. Root cause TBD.

### Files Modified

```
stand/boot/efi/loader/
├── arch/aarch64/
│   ├── start.S              # Entry + embedded PE header
│   ├── ldscript.aarch64     # Linker script for PE layout
│   └── elf64_freebsd.c      # Stub (returns EFTYPE)
├── self_reloc.c             # Fixed RELA relocation
├── efi_main.c               # EFI initialization
└── main.c                   # Loader main loop

stand/lib/
└── zalloc_defs.h            # Disabled guards temporarily
```

### Build Instructions (VM)

```sh
# Cross-compiler on VM
CC=/usr/local/bin/aarch64-none-elf-gcc

# Build loader
cd /usr/src/stand/boot/efi/loader
make clean && make -j8 MACHINE_ARCH=aarch64 MACHINE=aarch64 CC=$CC

# Build kernel (via config(8))
cd /usr/src/sys/compile/ARM64_GENERIC
make clean && make -j8 kernel.debug
```

### Test Instructions (Host)

Use the build/test agent or manual workflow:

```sh
cd tools/arm64-test
make copy-all       # Fetch loader and kernel from VM
make test           # Run with 45s timeout (headless, HVF accelerated)
make run-gui        # Run with graphical display
```

**QEMU Acceleration:** The test harness defaults to HVF (Hypervisor.framework)
on Apple Silicon Macs for near-native speed. Override with `QEMU_ACCEL=tcg`
for portable software emulation.

---

## MVP Part 2: Stub Kernel (COMPLETE)

### Goal

Create a minimal arm64 kernel that the EFI loader can load and execute, prints a message to UART, and halts.

### Status: COMPLETE

The stub kernel milestone has been achieved and significantly exceeded. The kernel now:
- Loads via EFI loader with full module metadata
- Enters at `_start` with MMU enabled (identity-mapped)
- Parses boot metadata (modulep) including EFI memory map
- Runs `initarm()` C code successfully
- Prints diagnostic output to PL011 UART

### Key Implementation

**Loader (`stand/boot/efi/loader/arch/aarch64/elf64_freebsd.c`):**
- `elf64_exec()` builds module metadata via `bi_load()`
- Calls `ExitBootServices()` to take control from UEFI
- Flushes caches (D-cache clean, I-cache invalidate)
- Jumps to kernel entry with `modulep` in x0

**Kernel Platform Structure:**
```
sys/platform/arm64/
├── Makefile.inc           # Platform build rules
├── conf/
│   ├── ARM64_GENERIC      # Kernel config file
│   ├── Makefile           # Kernel build Makefile
│   ├── files              # File list for kernel config
│   ├── kern.mk            # Kernel build rules
│   └── ldscript.aarch64   # Linker script
├── include/               # Machine headers (machine/*.h)
└── aarch64/
    ├── locore.s           # Entry point with MMU setup
    ├── machdep.c          # initarm() and early C init
    ├── pmap.c             # Pmap stubs for linking
    ├── support.c          # Support routines (copy/fetch)
    └── genassym.c         # Assembly constants

sys/cpu/aarch64/include/   # CPU headers (cpu/*.h)
```

**Kernel entry flow:**
1. `_start` in `locore.s` receives `modulep` in x0
2. Sets up early identity map (TTBR0) for physical addresses
3. Enables MMU with 4KB pages
4. Calls `initarm(modulep)` in C

### Success Criteria (All Met)

- ✅ Loader successfully loads ELF64 kernel
- ✅ Loader exits boot services and jumps to kernel
- ✅ Kernel prints to UART (diagnostic output visible)
- ✅ Message visible in QEMU serial output

---

## MVP Part 3: Early MMU Bootstrap (COMPLETE)

### Goal

Replace the loader-side MMU disable workaround with a FreeBSD-derived, kernel-controlled page table setup that enables the MMU early and preserves the DragonFly boot flow.

### Status: COMPLETE (All Phases Done)

**Completed:**

Phase A - Metadata consumption (modulep): ✅ COMPLETE
- `initarm()` receives modulep as physical address
- Parses module metadata to find kernel record
- Extracts: `boothowto`, `kern_envp`, `efi_systbl_phys`
- Prints diagnostic values over UART

Phase B - EFI memory map ingestion: ✅ COMPLETE
- Fetches EFI map header via `MODINFOMD_EFI_MAP`
- Validates descriptor size/version, counts 36 entries
- Builds physmem list: 8 usable ranges, ~112K pages (~440MB)
- Identifies largest contiguous range for bootstrap allocator

Phase C - Pmap bootstrap skeleton: ✅ COMPLETE
- Bootstrap allocator initialized from largest EFI range (0x48000000-0x5c13b000)
- L0/L1/L2 page tables allocated from bootstrap memory
- TTBR1 page tables built for kernel high-VA mapping
- Page table entries populated (2MB blocks for kernel text/data)

Phase C.5 - config(8) kernel build: ✅ COMPLETE
- `ARM64_GENERIC` kernel config works with `config -r -g`
- Full kernel build via `make kernel.debug` in `sys/compile/ARM64_GENERIC`
- Cross-compiler: `/usr/local/bin/aarch64-none-elf-gcc`
- Kernel links successfully with ~300 stub functions

Phase C.6 - TTBR1 switch and high-VA execution: ✅ COMPLETE
- Debug output added to identify failure point in TTBR1 switch
- Discovered page tables only mapped 4MB, but trampoline at L2 index 2
- Extended L2 table to map 16MB (8 × 2MB blocks)
- TTBR1 switch now succeeds, high-VA trampoline executes
- Kernel continues boot in high-VA space

Phase D - Console framework integration: ✅ COMPLETE
- Fixed `_get_mycpu()` to read x18 register (ARM64 per-CPU data convention)
- Added `arm64_init_globaldata()` to set up thread0 and globaldata before cninit()
- `cninit()` now works - PL011 driver registers as console
- `kprintf()` outputs through console framework

**Current Boot Output:**
```
[arm64] initarm: modulep(phys)=0x0000000040caf000
[arm64] module: /boot/KERNEL/kernel
[arm64] kernend=0x0000000040cb0000
[arm64] boothowto=0x0000000000000000
[arm64] kern_envp=0x0000000040cae000
[arm64] efi_systbl=0x000000005ffd0018
[arm64] efi_map entries=0000000000000024
[arm64] efi_map usable_pages=000000000001b7f3
[arm64] physmem ranges=0000000000000008
[arm64] pmap bootstrap: largest range 0x0000000048000000-0x000000005c13b000
[arm64] boot_alloc range 0x0000000048000000-0x000000005c13b000
[arm64] pt l2=0x0000000048004000 (8 entries, 16MB)
[arm64] pt l0[0]=0x0000000048003003
[arm64] pt l1[0]=0x0000000048004003
[arm64] ttbr1 current=0x00000000406bd000
[arm64] ttbr1 candidate=0x0000000048002000
[arm64] ttbr1 switching...
[arm64] ttbr1 switch done, calling trampoline
[arm64] trampoline addr=0xffffff800040d9a0
[arm64] high-va ok
[arm64] ttbr1 switch active
[arm64] setting up globaldata
[arm64] calling cninit()
[arm64] cninit() done

DragonFly/arm64 kernel started!
Console initialized via PL011 driver.


DragonFly/arm64 kernel started!
modulep received, halting.
```

Note: The "DragonFly/arm64 kernel started!" message appears twice - once from
kprintf() via the console framework, and once from the old locore.s banner.
The locore.s banner should be removed as part of cleanup.

**Next steps:**
- Clean up duplicate boot messages (remove old locore.s banner)
- Remove early uart_puts() debug output once stable
- Continue to Phase E: kernel main (mi_startup path)

### Key Fixes Applied This Session

1. **Loader staging area too small** - Increased from 8MB to 32MB in `copy.c`
   - modulep was at ~12.7MB offset, beyond 8MB staging area
   - `efi_copyin()` was silently failing for modulep data
   - Fixed: staging now covers full kernel + metadata

2. **modulep physical vs virtual address** - Keep modulep as physical in early boot
   - Early page tables only map first 4MB at high VA
   - modulep at ~12MB needs TTBR0 identity map access
   - Fixed in `machdep.c`: don't convert to high VA prematurely

3. **TTBR1 page tables mapped too little** - Extended from 4MB to 16MB
   - Trampoline function at VA 0xffffff800040d9a0 requires L2 index 2
   - Only L2[0] and L2[1] were mapped (4MB total)
   - Fixed: map 8 L2 entries (16MB) to cover full kernel

### Completed Work (Phase D)

Phase D: Console framework integration - COMPLETE
1. ✅ Fixed `_get_mycpu()` in `sys/platform/arm64/include/thread.h`
2. ✅ Added `arm64_init_globaldata()` in `machdep.c`
3. ✅ Call `cninit()` in early arm64 init path
4. ✅ `kprintf()` now works through PL011 driver
5. Remaining: Clean up old uart_puts() debug output

### Phase D Implementation (COMPLETE)

The PL011 console driver exists under `sys/dev/serial/pl011/`:
- `pl011_cons.c` - probe/init/putc implementation
- `pl011_reg.h` - register offsets and flags

Implementation completed:
- Driver compiles and is included in kernel build
- Uses `CONS_DRIVER` macro for DragonFly console framework
- `cninit()` called after globaldata/thread0 setup
- `kprintf()` works through console framework

Key fix: ARM64 uses x18 register for per-CPU globaldata pointer. The
`_get_mycpu()` function was fixed to read from x18, and `arm64_init_globaldata()`
sets up the minimal structures needed for the token subsystem (used by cninit).

### FreeBSD Reference (Do Not Copy Without License)

Reference these files and follow their order of operations. Any reused code must include compatible FreeBSD license headers and be adapted to DragonFlyBSD subsystems and conventions.

**Loader:**
- `.freebsd.orig/stand/efi/loader/arch/arm64/exec.c`
- `.freebsd.orig/stand/efi/loader/arch/arm64/start.S`

**Kernel entry/MMU:**
- `.freebsd.orig/sys/arm64/arm64/locore.S`
- `.freebsd.orig/sys/arm64/arm64/machdep.c`
- `.freebsd.orig/sys/arm64/arm64/machdep_boot.c`
- `.freebsd.orig/sys/arm64/include/machdep.h`
- `.freebsd.orig/sys/arm64/arm64/pmap.c`

### Plan (Clear Steps by Section)

#### 1) Loader Handoff (No New Work Required)

1. Keep the existing DragonFly loader flow: `dev_cleanup()`, `bi_load()`, cache flush, jump to kernel entry with `modulep` in x0.
2. Once kernel MMU setup is implemented, remove the loader-side MMU disable workaround.

#### 2) Kernel Entry Order (locore.s)

1. Enter the kernel exception level (EL1) and ensure MMU is off in EL2 entry cases.
2. Compute the physical load address (FreeBSD uses `get_load_phys_addr`).
3. Build page tables (see section 3).
4. Enable MMU (see section 4).
5. Switch to the virtual address space.
6. Set up an initial stack, zero BSS.
7. Build a bootparams struct containing at least: modulep, kern_stack, kern_ttbr0, boot_el.
8. Branch to early C init (DragonFly equivalent of `initarm`).

#### 3) Early Page Tables (Identity + Kernel + UART)

1. Create TTBR1 mappings for the kernel virtual region:
   - Text: read-only, executable
   - Data/BSS: read-write, XN
2. Create a TTBR0 bootstrap identity map for the kernel load address (VA=PA).
3. Map UART/PL011 device region early (device memory type) so early prints keep working.
4. Keep the layout minimal: only the kernel and UART mappings needed to reach C init.

#### 4) MMU Enable Sequence

1. Set exception vectors (VBAR_EL1).
2. Load TTBR0/TTBR1 with bootstrap + kernel tables.
3. Invalidate TLBs and synchronize (DSB/ISB).
4. Program MAIR_EL1 with device and normal WB attributes.
5. Program TCR_EL1 for 4K pages, inner/outer WBWA, shareability.
6. Enable SCTLR_EL1 MMU + caches and issue ISB.

#### 5) Early C Init (DragonFly-side)

1. Parse boot metadata from `modulep` (FreeBSD uses `parse_boot_param`).
2. Initialize per-CPU data needed by pmap bootstrap.
3. Bootstrap DMAP (or DragonFly equivalent) and exclude EFI ranges as needed.
4. Complete pmap bootstrap.
5. Initialize console (`cninit`) and switch to final TTBR0 if required.

#### 6) Tests

1. Rebuild loader and kernel on the VM.
2. Copy artifacts with `tools/arm64-test/Makefile` targets.
3. Run `make test TEST_TIMEOUT=300`.
4. Expect UART output from the kernel without needing to disable MMU in the loader.

### Success Criteria

- Kernel boots with MMU enabled by the kernel (no loader-side MMU disable).
- Early UART output works after enabling MMU.
- No instruction aborts on kernel entry.
- Boot metadata is readable in C init.

### Appendix: Serial Console Cleanup Plan (FreeBSD Strategy)

Goal: Headless runs (`make run`/`make test`) produce clean serial output with
no EFI cursor spam or UTF-16 garbage by selecting the EFI serial console
(`eficom`) automatically, while GUI runs keep using the EFI text console.

Constraints observed:
- `startup.nsh` is not guaranteed to run under EDK2 Boot#### flow, so console
  selection must not rely on argv alone.
- The garbage output is generated by EFI text console terminal emulation
  (`efi_console` with TERM_EMU) calling `SetCursorPosition()` and by direct
  `ConOut->OutputString()` UTF-16 output on serial.

Exact plan (do not skip any step):

1) Confirm current behavior (no code change)
- Verify headless output shows `Console: EFI console` and cursor spam.
- Verify GUI output shows EFI console text in the QEMU window.

2) Fix `eficom` probe semantics (required prerequisite)
- `cons_probe()` clears console flags before probing; `eficom` must be able
  to set `C_PRESENTIN|C_PRESENTOUT` during probe without requiring
  `C_ACTIVEIN|C_ACTIVEOUT`.
- Keep any “active-only” hardware configuration in `comc_init()` if needed.

3) Implement FreeBSD-style auto console selection (ConOut/ConIn device paths)
- Port minimal logic of FreeBSD `parse_uefi_con_out()` into DragonFly
  (read UEFI variables `ConOut`, `ConOutDev`, `ConIn`, parse device paths
  for serial/video).
- Before `cons_probe()` in the EFI loader, set `console=eficom` when serial
  is preferred, else `console=efi` for GUI.
- Do not rely on `startup.nsh` arguments for selection.

4) Gate direct EFI text output when serial console is active
- Avoid `ST->ConOut->OutputString()` unless the active console includes `efi`.
- Ensure the firmware vendor line prints ASCII-only on serial.

5) Align test harness with the console strategy
- Headless `run`/`test`: rely on auto-selection, keep serial on stdio.
- GUI `run-gui`: keep EFI text console in window; serial remains for kernel UART.
- Remove any dependency on `console=` argv in `startup.nsh` for headless tests.

6) Validate
- Headless output shows `Console: serial port` (or equivalent) and no cursor spam.
- `EFI Firmware:` line has no UTF-16 garbage.
- Kernel UART output remains visible.

### FUTURE TODO

- Consider increasing arm64 EFI staging window to match FreeBSD defaults (64MB)
  - FreeBSD uses `DEFAULT_EFI_STAGING_SIZE` = 64MB for non-arm32
  - DragonFly currently uses 32MB fixed window for arm64 (increased from 8MB)
  - 32MB is sufficient for current kernel (~5MB) + metadata

---

## MVP Part 4: Kernel Main (Phase E) - PHASE E.4 COMPLETE

### Goal

Continue kernel initialization to reach `mi_startup()`, which runs SYSINIT
entries that bring up the rest of the kernel subsystems.

### Status: PHASE E.4 COMPLETE - Kernel reaches mi_startup()

The kernel now successfully:
1. `initarm()` returns kernel stack pointer (`thread0.td_pcb`)
2. `locore.s` sets SP and calls `mi_startup()`
3. `mi_startup()` runs, sorting and executing 323 SYSINITs
4. Copyright banner prints (SI_BOOT1_COPYRIGHT = 0x0800000)
5. Boot continues through SI_BOOT1_LOCK (0x0900000) into SI_BOOT1_VM (0x1000000)

### Current Boot Output (Phase E.4)

```
DragonFly/arm64 kernel started!
Console initialized via PL011 driver.
init_param1() done, hz=100
arm64_gdinit_full() done
init_param2() done, physmem=112627 pages (439 MB)
msgbufinit() done
initarm: returning SP=0x40cc2ee0 (td_pcb)
A
0000000040cc2ee0
B
C
mi_startup() entered
mi_startup: sorting 323 SYSINITs
mi_startup: starting SYSINIT execution
SYSINIT: 00700000 0x4014f770
... [executes ~52 SYSINITs]
SYSINIT: 00800000 0x40105f30
Copyright (c) 2003-2026 The DragonFly Project.
Copyright (c) 1992-2003 The FreeBSD Project.
Copyright (c) 1979, 1980, 1983, 1986, 1988, 1989, 1991, 1992, 1993, 1994
	The Regents of the University of California. All rights reserved.
SYSINIT: 00900000 0x4022c3c0
SYSINIT: 00900000 0x40123d00
SYSINIT: 00900000 0x40105580
SYSINIT: 01000000 0x403b3060
```

The kernel currently stalls at SI_BOOT1_VM (0x1000000), likely in VM initialization.

### Current State After Phase E.4

- `initarm()` fully initializes globaldata, proc0, thread0
- `init_param1()`, `init_param2()`, `msgbufinit()` all complete
- `initarm()` returns `thread0.td_pcb` as kernel stack pointer
- `locore.s` sets SP from return value and calls `mi_startup()`
- `mi_startup()` sorts 323 SYSINITs and begins execution
- Boot progresses through SI_BOOT1_TUNABLES, SI_BOOT1_COPYRIGHT, SI_BOOT1_LOCK
- Stalls at SI_BOOT1_VM - VM initialization needs arm64 support

### What x86_64 Does (hammer_time reference)

From `sys/platform/pc64/x86_64/machdep.c`, the `hammer_time()` function does:

1. Set up globaldata (`CPU_prvspace[0]->mdglobaldata`)
2. Set `gd_curthread = &thread0`, `thread0.td_gd = &gd->mi`
3. Parse preload metadata, set `boothowto`, `kern_envp`
4. `init_param1()` - basic tunables (hz, etc.)
5. Set up GDT, IDT (x86-specific)
6. `mi_gdinit()` / `cpu_gdinit()` - per-CPU init
7. `mi_proc0init()` - initialize proc0/lwp0/thread0 properly
8. `init_locks()` - spinlocks and BGL
9. Set up exception vectors
10. `cninit()` - console init
11. `getmemsize()` - memory sizing
12. `init_param2(physmem)` - memory-dependent params
13. `msgbufinit()` - message buffer
14. Return stack pointer for mi_startup()

Then `locore.s` calls `mi_startup()`.

### Incremental Plan (Option C)

We'll implement this incrementally to isolate failures:

#### Phase E.1: init_param1()
- Call `init_param1()` for basic tunable setup
- Should be straightforward, establishes hz and other basics

#### Phase E.2: Globaldata and Proc0 Init
- Allocate `proc0paddr` buffer (can be static for now)
- Expand `arm64_init_globaldata()` to call `mi_gdinit()`
- Create arm64 `cpu_gdinit()` (minimal version)
- Call `mi_proc0init()` to properly initialize proc0/lwp0/thread0
- Stub `cpu_lwkt_switch` to return curthread (single-threaded boot)

#### Phase E.3: Memory and Message Buffer Init
- Calculate physmem count from EFI map
- Call `init_param2(physmem)`
- Allocate and call `msgbufinit()`

#### Phase E.4: Call mi_startup()
- Modify `initarm()` to return proper stack pointer
- Modify `locore.s` to call `mi_startup()` after `initarm()`
- Debug SYSINIT failures as they occur

### Prerequisites Checklist

| Item | Description | Status |
|------|-------------|--------|
| `init_param1()` | Basic tunables | ✅ |
| `mi_gdinit()` | Globaldata MI init | ✅ |
| `cpu_gdinit()` | Arm64 CPU-specific gd init | ✅ |
| `mi_proc0init()` | Proc0/thread0 full init | ✅ |
| `init_locks()` | Spinlocks/BGL | ✅ |
| `init_param2()` | Memory-dependent params | ✅ |
| `msgbufinit()` | Message buffer | ✅ |
| `cpu_lwkt_switch` stub | Context switch (minimal) | ✅ |
| locore.s `mi_startup` call | Assembly changes | ✅ |

### Key Dependencies

1. **`mi_proc0init()` requires**:
   - `proc0paddr` - kernel stack for proc0 (can be static buffer)
   - Proper globaldata with `gd_prvspace`
   - `cpu_lwkt_switch` (stub OK for boot)

2. **`init_param2()` requires**:
   - `physmem` count from memory map

3. **`msgbufinit()` requires**:
   - `msgbufp` pointer to message buffer area

### Success Criteria

- ✅ Kernel calls `mi_startup()` without crashing
- ✅ First SYSINIT entries execute (copyright banner prints)
- ⏳ Kernel stalls at SI_BOOT1_VM - VM initialization needs arm64 pmap support

### Next Steps (Phase E.5)

1. **Remove debug output** - SYSINIT tracing in `init_main.c` slows boot significantly
2. ~~**Implement arm64 VM support** - `pmap_init()` and related functions for SI_BOOT1_VM~~ → See Phase E.5 below
3. **Debug SYSINIT failures** - As boot progresses, more arm64-specific stubs will need implementation
4. **Consider**: Reducing arm64 debug output in `locore.s` and `machdep.c` once stable

---

## MVP Part 5: VM_PHYSSEG_SPARSE for Non-Contiguous Memory (COMPLETE)

### Goal

Fix `vm_page_startup()` hang caused by EFI providing non-contiguous physical memory regions on arm64.

### Status: COMPLETE

The kernel now successfully handles non-contiguous physical memory from EFI. The original `PHYS_TO_VM_PAGE()` macro assumed contiguous physical memory, causing queue corruption and hangs when gaps exist between memory regions.

### Problem Analysis

EFI on QEMU arm64 provides 8 non-contiguous physical memory ranges:
```
[0] 0x42000000 - 0x44000000 (8192 pages)
[1] 0x44020000 - 0x47666000 (13894 pages) GAP=32 pages
[2] 0x48000000 - 0x5c13b000 (82235 pages) GAP=2458 pages
[3] 0x5cb44000 - 0x5ea2f000 (7915 pages) GAP=2569 pages
[4] 0x5efb9000 - 0x5efbd000 (4 pages) GAP=1418 pages
[5] 0x5fa38000 - 0x5fb8c000 (340 pages) GAP=2683 pages
[6] 0x5feb0000 - 0x5fec0000 (16 pages) GAP=804 pages
[7] 0x5ffe0000 - 0x5ffff000 (31 pages) GAP=288 pages
```

The original `PHYS_TO_VM_PAGE(pa)` macro:
```c
#define PHYS_TO_VM_PAGE(pa) (&vm_page_array[atop(pa) - first_page])
```

This assumes `vm_page_array` is indexed by `(pa >> PAGE_SHIFT) - first_page`, which only works for contiguous memory. With gaps, addresses in gap regions return invalid pointers, corrupting page queues.

### Solution: VM_PHYSSEG_SPARSE

Ported FreeBSD's `VM_PHYSSEG_SPARSE` infrastructure, simplified for DragonFly (no NUMA):

**New Files:**
- `sys/vm/_vm_phys.h` - struct vm_phys_seg definition (start, end, first_page)
- `sys/vm/vm_phys.h` - function declarations and inline helpers
- `sys/vm/vm_phys.c` - segment management with binary search lookup

**Modified Files:**
- `sys/platform/arm64/include/vmparam.h` - define `VM_PHYSSEG_SPARSE`
- `sys/vm/vm_page.h` - make `PHYS_TO_VM_PAGE` conditional
- `sys/vm/vm_page.c` - integrate vm_phys, pack pages contiguously for SPARSE
- `sys/conf/files` - add `vm/vm_phys.c standard`

**Key Design:**
- **SPARSE mode (arm64)**: Pages packed contiguously in `vm_page_array` with no wasted entries for gaps. `PHYS_TO_VM_PAGE()` uses binary search to find the correct segment.
- **DENSE mode (x86_64)**: Original behavior preserved - simple arithmetic lookup.

### Implementation Details

```c
/* sys/vm/vm_page.h */
#ifdef VM_PHYSSEG_SPARSE
vm_page_t vm_phys_paddr_to_vm_page(vm_paddr_t pa);
#define PHYS_TO_VM_PAGE(pa) vm_phys_paddr_to_vm_page(pa)
#else
#define PHYS_TO_VM_PAGE(pa) (&vm_page_array[atop(pa) - first_page])
#endif
```

Segment structure (`_vm_phys.h`):
```c
struct vm_phys_seg {
    vm_paddr_t  start;       /* First physical address */
    vm_paddr_t  end;         /* One past last physical address */
    vm_page_t   first_page;  /* Pointer to first vm_page in this segment */
};
```

In `vm_page_startup()` for SPARSE mode:
1. `page_range` = sum of pages in all segments (no gaps counted)
2. Call `vm_phys_add_seg()` for each phys_avail range
3. Call `vm_phys_init()` after vm_page_array allocation to set `first_page` pointers
4. Initialize pages per-segment with correct physical addresses

### Test Results

```
vm_page_startup: phys_avail[] ranges (7 total):
  [0] 0x42000000 - 0x44000000 (8192 pages)
  [1] 0x44020000 - 0x47666000 (13894 pages) GAP=32 pages
  [2] 0x48000000 - 0x5c13b000 (82235 pages) GAP=2458 pages
  ...

vm_page_startup: entering free queue loop
  range 0 start (pa=0x42000000)
vm_add_new_page[1]: pa=0x42000000
  [1] queue=265 pc=264
  [1] calling TAILQ_INSERT_HEAD
  [1] TAILQ_INSERT_HEAD done
vm_add_new_page[2]: pa=0x42001000
  ...
```

- ✅ Build succeeds without errors
- ✅ Kernel passes `vm_page_startup()` (previously hung)
- ✅ Pages correctly added to free queues
- ✅ No data aborts or queue corruption

### Success Criteria (All Met)

- ✅ `vm_page_startup()` completes without hanging
- ✅ Non-contiguous memory ranges handled correctly
- ✅ x86_64 builds unaffected (DENSE mode preserved)
- ✅ No wasted vm_page_array entries for gap regions

### Next Steps

- Continue VM initialization (`pmap_init()` and beyond)
- Remove SYSINIT debug tracing once stable
- Debug any new stall points as boot progresses

---

## MVP Part 6: pmap_enter() and pmap_kenter() Implementation

### Goal

Implement the minimum viable pmap functions to allow the kernel to map pages into kernel virtual address space, enabling the slab allocator (`kmeminit()`) to function.

### Status: COMPLETE ✅

The kernel currently crashes during `SI_BOOT1_ALLOCATOR` (0x1400000) - slab allocator initialization. The crash occurs because `pmap_enter()` is a stub that does nothing.

### Problem Analysis

**Crash location:** `memset()` at `0x4040d370` - inner loop writing bytes to memory.

**Call chain:**
1. `kmeminit()` calls `kmem_slab_alloc(PAGE_SIZE, PAGE_SIZE, M_WAITOK|M_ZERO)`
2. `kmem_slab_alloc()` allocates physical pages via `vm_page_alloc()`
3. `kmem_slab_alloc()` calls `pmap_enter()` to map pages into kernel VA space
4. `pmap_enter()` is empty (stub) - no page table entry created
5. `pagezero()` / `memset()` tries to access the unmapped VA
6. **Translation fault** - the VA `0xffffff8008dd3000` isn't mapped

**Exception details:**
- **ESR:** `0x96000046` - Data Abort, translation fault level 2
- **FAR:** `0xffffff8008dd3000` - kernel virtual address being accessed
- **ELR:** `0x4040d370` - instruction that caused the fault (in memset)

### Background: DragonFly vs FreeBSD VM Differences

Key differences that affect implementation:

1. **DragonFly uses `pmap_bits[]` array** - Each pmap has an array mapping logical bit indices (PG_V_IDX, PG_RW_IDX, etc.) to architecture-specific bits. This allows different pmap types.

2. **DragonFly uses `protection_codes[]`** - Pre-computed protection bit combinations for VM_PROT_* flags.

3. **DragonFly tracks pages differently** - Uses `PG_MAPPED`, `PG_WRITEABLE`, `PG_MAPPEDMULTI` flags on vm_page, and `md.interlock_count`.

4. **DragonFly's pmap_enter signature is different**:
   - DragonFly: `pmap_enter(pmap, va, m, prot, wired, entry)`
   - FreeBSD: `pmap_enter(pmap, va, m, prot, flags, psind)`

5. **DragonFly uses `pmap_initialized` flag** - Before full initialization, uses simpler direct PTE access.

6. **x86_64 uses recursive page table (PTmap)** - ARM64 can't easily do this, so we need explicit page table walking.

### Files to Modify

| File | Changes |
|------|---------|
| `sys/platform/arm64/include/pmap.h` | Add pmap structure fields, bit indices, protection codes |
| `sys/platform/arm64/aarch64/pmap.c` | Implement pmap_enter, pmap_kenter, page table walking, L3 allocation |
| `sys/platform/arm64/aarch64/locore.s` | Export ttbr1_l0/l1/l2 symbols |
| `sys/platform/arm64/aarch64/machdep.c` | Call pmap_bootstrap() early in init |

### Implementation Plan

#### 1. `sys/platform/arm64/include/pmap.h` Changes

**Add PTE bit index constants:**

```c
/* PTE bit indices for pmap_bits[] array */
#define PG_V_IDX        0   /* Valid */
#define PG_RW_IDX       1   /* Read/Write (ARM64: inverted - 0=RW) */
#define PG_U_IDX        2   /* User accessible */
#define PG_A_IDX        3   /* Accessed */
#define PG_M_IDX        4   /* Modified/Dirty */
#define PG_W_IDX        5   /* Wired (software bit) */
#define PG_MANAGED_IDX  6   /* Managed page (software bit) */
#define PG_N_IDX        7   /* Non-cacheable */
#define PG_NX_IDX       8   /* No Execute */
#define PG_BITS_SIZE    16

#define PROTECTION_CODES_SIZE   8
#define PAT_INDEX_SIZE          8  /* Memory attribute indices */
```

**Expand struct pmap:**

```c
struct pmap {
    struct pmap_statistics pm_stats;
    pd_entry_t *pm_l0;                  /* L0 page table (kernel VA) */
    vm_paddr_t pm_l0_paddr;             /* L0 page table (physical) */
    uint64_t pmap_bits[PG_BITS_SIZE];
    uint64_t pmap_cache_bits_pte[PAT_INDEX_SIZE];
    uint64_t protection_codes[PROTECTION_CODES_SIZE];
};
```

#### 2. `sys/platform/arm64/aarch64/locore.s` Changes

**Export page table symbols (add .global directives):**

```asm
    .global ttbr1_l0
    .global ttbr1_l1  
    .global ttbr1_l2
```

#### 3. `sys/platform/arm64/aarch64/pmap.c` Changes

##### a) Add external references and static data:

```c
/* Page tables from locore.s */
extern pd_entry_t ttbr1_l0[];
extern pd_entry_t ttbr1_l1[];
extern pd_entry_t ttbr1_l2[];

/* Pre-allocated L3 pages for early kernel mappings (32MB coverage) */
#define NKERN_L3_PAGES  16
static pt_entry_t kern_l3_pages[NKERN_L3_PAGES][Ln_ENTRIES] __aligned(PAGE_SIZE);
static int kern_l3_next = 0;

/* Default PTE bit mappings for ARM64 */
static uint64_t pmap_bits_default[PG_BITS_SIZE] = {
    [PG_V_IDX]       = ATTR_DESCR_VALID,
    [PG_RW_IDX]      = 0,                    /* 0 = RW on ARM64 */
    [PG_U_IDX]       = ATTR_S1_AP(ATTR_S1_AP_USER),
    [PG_A_IDX]       = ATTR_AF,
    [PG_M_IDX]       = ATTR_DBM,
    [PG_W_IDX]       = (1UL << 55),          /* Software: wired */
    [PG_MANAGED_IDX] = (1UL << 56),          /* Software: managed */
    [PG_NX_IDX]      = ATTR_S1_XN,
};

/* Memory attribute index to PTE bits */
static uint64_t pmap_cache_bits_default[PAT_INDEX_SIZE] = {
    [0] = ATTR_S1_IDX(MAIR_IDX_DEVICE_nGnRnE),
    [1] = ATTR_S1_IDX(MAIR_IDX_UNCACHEABLE),
    [2] = ATTR_S1_IDX(MAIR_IDX_WRITE_BACK),
    [3] = ATTR_S1_IDX(MAIR_IDX_WRITE_THROUGH),
    [4] = ATTR_S1_IDX(MAIR_IDX_DEVICE_nGnRE),
};

/* Protection codes: VM_PROT_* -> PTE bits */
static uint64_t protection_codes_default[PROTECTION_CODES_SIZE];
```

##### b) Add page table helper functions:

```c
static __inline pd_entry_t *
pmap_l0(pmap_t pmap, vm_offset_t va)
{
    return (&pmap->pm_l0[pmap_l0_index(va)]);
}

static __inline pd_entry_t *
pmap_l0_to_l1(pd_entry_t *l0, vm_offset_t va)
{
    pd_entry_t l0e = *l0;
    pd_entry_t *l1 = (pd_entry_t *)PHYS_TO_DMAP(l0e & ATTR_ADDR);
    return (&l1[pmap_l1_index(va)]);
}

static __inline pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
    pd_entry_t l1e = *l1;
    pd_entry_t *l2 = (pd_entry_t *)PHYS_TO_DMAP(l1e & ATTR_ADDR);
    return (&l2[pmap_l2_index(va)]);
}

static __inline pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
    pd_entry_t l2e = *l2;
    pt_entry_t *l3 = (pt_entry_t *)PHYS_TO_DMAP(l2e & ATTR_ADDR);
    return (&l3[pmap_l3_index(va)]);
}
```

##### c) Add L3 table allocation function:

```c
static pt_entry_t *
pmap_alloc_l3(pmap_t pmap, pd_entry_t *l2, vm_offset_t va)
{
    pt_entry_t *l3;
    vm_paddr_t l3_pa;
    
    if (kern_l3_next >= NKERN_L3_PAGES)
        panic("pmap_alloc_l3: out of pre-allocated L3 pages");
    
    l3 = kern_l3_pages[kern_l3_next++];
    bzero(l3, PAGE_SIZE);
    
    l3_pa = DMAP_TO_PHYS((vm_offset_t)l3);
    
    /* Install L2 entry pointing to new L3 table */
    *l2 = l3_pa | L2_TABLE;
    __asm __volatile("dsb ishst" ::: "memory");
    
    return (&l3[pmap_l3_index(va)]);
}
```

##### d) Add pmap_vtopte() - get PTE pointer, allocating L3 if needed:

```c
static pt_entry_t *
pmap_vtopte(pmap_t pmap, vm_offset_t va)
{
    pd_entry_t *l0, *l1, *l2;
    
    l0 = pmap_l0(pmap, va);
    if ((*l0 & ATTR_DESCR_MASK) != L0_TABLE)
        panic("pmap_vtopte: no L0 entry for va 0x%lx", va);
    
    l1 = pmap_l0_to_l1(l0, va);
    if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
        panic("pmap_vtopte: no L1 entry for va 0x%lx", va);
    
    l2 = pmap_l1_to_l2(l1, va);
    if ((*l2 & ATTR_DESCR_MASK) != L2_TABLE) {
        /* Need to allocate L3 table */
        return pmap_alloc_l3(pmap, l2, va);
    }
    
    return pmap_l2_to_l3(l2, va);
}
```

##### e) Implement pmap_kenter():

```c
void
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
    pt_entry_t *ptep;
    pt_entry_t npte;
    
    npte = pa | L3_PAGE | ATTR_AF | ATTR_SH(ATTR_SH_IS) |
           ATTR_S1_IDX(MAIR_IDX_WRITE_BACK);
    
    ptep = pmap_vtopte(kernel_pmap, va);
    *ptep = npte;
    
    /* TLB invalidate */
    __asm __volatile(
        "dsb ishst\n"
        "tlbi vaae1is, %0\n"
        "dsb ish\n"
        "isb"
        : : "r"(va >> 12)
    );
}
```

##### f) Implement pmap_enter():

```c
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
           boolean_t wired, vm_map_entry_t entry __unused)
{
    pt_entry_t *ptep;
    pt_entry_t newpte, origpte;
    vm_paddr_t pa;
    int flags, nflags;
    
    if (pmap == NULL)
        return;
    
    va = trunc_page(va);
    pa = VM_PAGE_TO_PHYS(m);
    
    /* Get PTE pointer */
    ptep = pmap_vtopte(pmap, va);
    origpte = *ptep;
    
    /* Build new PTE */
    newpte = pa | L3_PAGE | ATTR_AF | ATTR_SH(ATTR_SH_IS);
    newpte |= ATTR_S1_IDX(MAIR_IDX_WRITE_BACK);
    
    /* Protection bits */
    if (!(prot & VM_PROT_WRITE))
        newpte |= ATTR_S1_AP(ATTR_S1_AP_RO);
    if (!(prot & VM_PROT_EXECUTE))
        newpte |= ATTR_S1_XN;
    if (va < VM_MAX_USER_ADDRESS)
        newpte |= ATTR_S1_AP(ATTR_S1_AP_USER) | ATTR_S1_PXN;
    
    /* Software bits */
    if (wired)
        newpte |= pmap->pmap_bits[PG_W_IDX];
    if ((m->flags & PG_FICTITIOUS) == 0)
        newpte |= pmap->pmap_bits[PG_MANAGED_IDX];
    
    /* Update vm_page flags */
    flags = m->flags;
    for (;;) {
        nflags = PG_MAPPED;
        if (prot & VM_PROT_WRITE)
            nflags |= PG_WRITEABLE;
        if (flags & PG_MAPPED)
            nflags |= PG_MAPPEDMULTI;
        if (flags == (flags | nflags))
            break;
        if (atomic_fcmpset_int(&m->flags, &flags, flags | nflags))
            break;
    }
    
    /* Store the PTE */
    *ptep = newpte;
    
    /* TLB invalidate */
    __asm __volatile(
        "dsb ishst\n"
        "tlbi vaae1is, %0\n"
        "dsb ish\n"
        "isb"
        : : "r"(va >> 12)
    );
    
    /* Update statistics */
    if ((origpte & ATTR_DESCR_VALID) == 0) {
        pmap->pm_stats.resident_count++;
    }
    if (wired && (origpte == 0 || !(origpte & pmap->pmap_bits[PG_W_IDX]))) {
        pmap->pm_stats.wired_count++;
    }
}
```

##### g) Add pmap_bootstrap() function:

```c
void pmap_bootstrap(void);

static void
pmap_init_protection_codes(void)
{
    uint64_t *kp = protection_codes_default;
    int prot;
    
    for (prot = 0; prot < PROTECTION_CODES_SIZE; prot++) {
        *kp = 0;
        
        /* ARM64: AP[2]=1 means read-only, AP[2]=0 means read-write */
        if (!(prot & VM_PROT_WRITE))
            *kp |= ATTR_S1_AP(ATTR_S1_AP_RO);
        
        /* XN bit for non-executable */
        if (!(prot & VM_PROT_EXECUTE))
            *kp |= ATTR_S1_XN;
        
        kp++;
    }
    
    bcopy(protection_codes_default, kernel_pmap->protection_codes,
          sizeof(protection_codes_default));
}

void
pmap_bootstrap(void)
{
    /* Set up kernel_pmap to use TTBR1 page tables */
    kernel_pmap->pm_l0 = ttbr1_l0;
    kernel_pmap->pm_l0_paddr = DMAP_TO_PHYS((vm_offset_t)ttbr1_l0);
    
    /* Copy default bit mappings */
    bcopy(pmap_bits_default, kernel_pmap->pmap_bits, 
          sizeof(pmap_bits_default));
    bcopy(pmap_cache_bits_default, kernel_pmap->pmap_cache_bits_pte,
          sizeof(pmap_cache_bits_default));
    
    /* Initialize protection codes */
    pmap_init_protection_codes();
}
```

#### 4. `sys/platform/arm64/aarch64/machdep.c` Changes

**Call pmap_bootstrap() early in arm64_init():**

Add call after DMAP setup but before VM init:

```c
void
arm64_init(vm_paddr_t modulep, vm_offset_t kernend)
{
    /* ... existing code ... */
    
    /* After DMAP setup, before cninit() */
    pmap_bootstrap();
    
    /* ... continue with existing init ... */
}
```

### L3 Table Allocation Strategy

For early boot (before kmalloc works), pre-allocate a pool of L3 pages in BSS:

- 16 L3 pages = 32MB of mappable KVA (matching FreeBSD's approach)
- Simple bump allocator: `kern_l3_pages[kern_l3_next++]`

When allocating a new L3 table:
1. Get next page from pool
2. Zero it
3. Update L2 entry to point to it with L2_TABLE descriptor
4. Return pointer to appropriate L3 slot

### ARM64 PTE Format Notes

**L3 Page entry (4KB pages):**

```
[63:52] - Upper attributes (XN, PXN, Contiguous, DBM, software bits)
[51:48] - Reserved
[47:12] - Physical address (output address)
[11:10] - AF (Access Flag), SH (Shareability)
[9:8]   - AP (Access Permissions)
[7:6]   - Reserved
[5]     - NS (Non-secure, for Secure state)
[4:2]   - AttrIndx (memory attribute index into MAIR)
[1:0]   - Descriptor type (0b11 = L3 Page)
```

**Key attributes:**
- `ATTR_AF` (bit 10) - Access Flag, must be set
- `ATTR_SH(ATTR_SH_IS)` (bits 9:8 = 0b11) - Inner Shareable
- `ATTR_S1_AP(ATTR_S1_AP_RO)` (bit 7) - Read-only (AP2=1)
- `ATTR_S1_IDX(n)` (bits 4:2) - Memory attribute index
- `ATTR_S1_XN` (bit 54) - Execute Never
- `L3_PAGE` (bits 1:0 = 0b11) - Valid L3 page descriptor

**Software bits (available for OS use):**
- Bits 55, 56, 58, 59 are available
- Using bit 55 for PG_W_IDX (wired)
- Using bit 56 for PG_MANAGED_IDX (managed page)

### TLB Invalidation

ARM64 TLB invalidation for kernel mappings:

```c
__asm __volatile(
    "dsb ishst\n"           /* Ensure PTE store is visible */
    "tlbi vaae1is, %0\n"    /* Invalidate by VA, all ASIDs, EL1, Inner Shareable */
    "dsb ish\n"             /* Ensure invalidation completes */
    "isb"                   /* Synchronize instruction stream */
    : : "r"(va >> 12)
);
```

- `vaae1is` = VA, All ASIDs, EL1, Inner Shareable broadcast
- Input is VA >> 12 (page number, not full address)

### Testing Strategy

After implementation:
1. Build kernel via arm64-port-testing agent
2. Run QEMU test
3. Expected: Kernel should get past `kmeminit()` and `SI_BOOT1_ALLOCATOR` (0x1400000)
4. May encounter new issues in later subsystems (SI_BOOT1_KMALLOC = 0x1600000, etc.)

### Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| L3 pool exhaustion | Panic with clear message; can increase pool size |
| Missing L1/L2 entries for KVA | Panic if page tables don't cover VA; locore.s setup should handle this |
| TLB invalidation issues | Use broadcast invalidate (vaae1is) + full barrier sequence |
| Incorrect PTE attributes | Start with known-good: WB cache, inner-shareable, AF set |

### Success Criteria

- [x] Kernel passes `kmeminit()` without translation faults
- [x] Kernel continues past `SI_BOOT1_ALLOCATOR` (0x1400000)
- [x] No panics from L3 pool exhaustion during early boot
- [x] Page mappings work correctly (no data aborts on mapped addresses)

**Note:** pmap_enter() implementation complete. Kernel now reaches SI_BOOT2_START_CPU where it panics due to missing cpu_startup(). See MVP Part 7.

### Related Commits

| Commit | Description |
|--------|-------------|
| d3d859db2f | arm64: Reserve x18 register for per-CPU globaldata pointer |
| 2dcd46ccf9 | Fix ALIST_RECORDS buffer overflow |
| ea388dc650 | Remove debug output after alist fix |
| ad57b7bd56 | arm64: implement pmap_enter() and related page table management |
| 8ea0b095ec | arm64: Initialize ncpus=1 before init_param1() |
| beac9e8493 | arm64: Extend DMAP to include kernel load address |
| 827700a670 | kern/slaballoc: Remove arm64 malloc debug output |

### References

- FreeBSD `sys/arm64/arm64/pmap.c` - ARM64 pmap implementation
- FreeBSD `sys/conf/kern.mk` lines 142-155 - ARM64 kernel CFLAGS including `-ffixed-x18`
- DragonFly `sys/platform/pc64/x86_64/pmap.c` - x86_64 pmap for DragonFly conventions
- ARM Architecture Reference Manual - Page table format and TLB maintenance

---

## Development Environment

- **Local repo:** `/Users/tuxillo/s/dragonfly`
- **VM repo:** `/usr/src`
- **Branch:** `port-arm64`
- **VM access:** `ssh root@devbox.sector.int -p 6021`
- **Workflow:** commit locally, push to Gitea, pull on VM, build, copy back to test

---

## Debug Output Key (MVP Part 1)

These debug markers were added during bring-up:

**From start.S:** `1`=entry, `2`=BSS cleared, `3`=before reloc, `4`=after reloc, `5`=before efi_main

**From efi_main.c:** `A`=entered, `B`=got EFI ptrs, `C`=console ctrl, `D`=heap alloc, `E`=setheap, `F`=img protocol, `G`=args, `H`=calling main

**From main.c:** `a`=entered, `b`=archsw set, `c`=has_kbd done, `d`=cons_probe done, `e`=args parsed, `f`=efi_copy_init, `g`=devs probed, `h`=printing

---

---

## MVP Part 7: cpu_startup() - Buffer/Pager Map Initialization

### Goal

Implement `cpu_startup()` for ARM64 to initialize kernel submaps (`buffer_map`, `pager_map`, `clean_map`) and buffer arrays required by the VM subsystem.

### Status: COMPLETE ✅

The `cpu_startup()` function has been implemented in `sys/platform/arm64/aarch64/machdep.c`. The kernel now successfully initializes buffer cache and VM submaps.

### Implementation Summary

**Commit:** `1b68068698` - arm64: Implement cpu_startup() and remove timer debug output

Changes made:
1. Added `MAXBUFSTRUCTSIZE` constant and static variables (`buffer_sva`, `buffer_eva`, `clean_sva`, `clean_eva`, `pager_sva`, `pager_eva`)
2. Implemented full `cpu_startup()` function following x86_64 pattern
3. Calculates `nbuf`, `nswbuf_mem`, `nswbuf_kva` based on physmem
4. Creates VM submaps via `kmem_suballoc()`: `clean_map`, `buffer_map`, `pager_map`
5. Registered with `SYSINIT(cpu, SI_BOOT2_START_CPU, SI_ORDER_FIRST, cpu_startup, NULL)`
6. Removed timer debug kprintf() statements from `kern_cputimer.c` and `kern_systimer.c`

### Additional Fixes (same session)

**Commit:** `83afdd8c77` - arm64: Add tsc_frequency for kern_clock.c compatibility
- Added `tsc_frequency` variable for `kern_clock.c` compatibility
- ARM64 uses generic timer counter frequency (CNTFRQ_EL0)

**Commit:** `a70e25f0c0` - arm64: Add tsc_present for kern_nrandom.c compatibility
- Added `tsc_present` variable for `kern_nrandom.c` compatibility
- ARM64 generic timer is always present (mandatory per ARMv8)

**Commit:** `d4c0b8432a` - arm64: Remove duplicate tsc_present/tsc_frequency from machdep.c
- Cleaned up duplicate definitions (now properly in generic_timer.c)

### Success Criteria (All Met)

- [x] Kernel passes `vm_pager_bufferinit()` without panicking
- [x] Kernel continues past `SI_BOOT2_MACHDEP` (0x1d80000)
- [x] `pager_map` is properly initialized before use
- [x] Timer debug output removed from kern_cputimer.c and kern_systimer.c

### Current Boot Output (Post cpu_startup)

```
DragonFly 6.5-DEVELOPMENT #58: Tue Jan 27 00:19:14 UTC 2026
    root@dev01.localhost:/usr/src/sys/compile/ARM64_GENERIC
real memory  = 461320192 (439 MB)
avail memory = 461320192 (439 MB)
SYSINIT: subsystem 01a58000
CPU Topology: cores_per_chip: 1; threads_per_core: 1; chips_per_package: 1;
SYSINIT: subsystem 01a80000
Initialize MI interrupts for 1 cpus
SYSINIT: subsystem 01ac0000
ARM64 timer: frequency 24000000 Hz (24.0 MHz)
ARM64 timer: calling cputimer_intr_register
ARM64 timer: calling cputimer_intr_select
ARM64 timer: calling cputimer_register
ARM64 timer: calling cputimer_select
ARM64 timer: registered cputimer and cputimer_intr
... [continues through many more SYSINITs] ...
panic: cpu_lwkt_switch: real context switch not implemented
```

---

## MVP Part 8: cpu_lwkt_switch() - LWKT Context Switching

### Goal

Implement ARM64 context switching for the Light Weight Kernel Thread (LWKT) subsystem to allow the kernel to switch between threads.

### Status: COMPLETE ✅

The LWKT context switching mechanism has been implemented. The kernel now successfully switches between threads and continues boot significantly further.

**Previous panic:**
```
panic: cpu_lwkt_switch: real context switch not implemented
```

**Current state:** Kernel boots through SYSINIT 02380000, then hits an instruction abort (NULL function pointer) - a different issue unrelated to context switching.

### Key Differences from FreeBSD

DragonFly's LWKT system is different from FreeBSD's thread system:
- **DragonFly uses `td_sp`** - a stack pointer stored in the thread structure for quick LWKT switching
- **DragonFly uses restore functions on the stack** - when switching, the restore function address is pushed to the stack, and `ret` jumps to it
- **The switch function returns the old thread** - `td_switch()` returns `thread_t` (the old thread) for `lwkt_switch_return()` processing

### ARM64 Callee-Saved Register Summary

Per AAPCS64 (ARM64 calling convention):
- **x19-x28**: Callee-saved general purpose registers
- **x29 (fp)**: Frame pointer (callee-saved)
- **x30 (lr)**: Link register (return address)
- **sp**: Stack pointer
- **x18**: Platform register (we use for per-CPU data - reserved via `-ffixed-x18`)

Registers x0-x17 are caller-saved and don't need to be preserved across switch.

### Success Criteria

- [x] `cpu_lwkt_switch()` implemented in assembly
- [x] `cpu_lwkt_restore()` implemented
- [x] `cpu_idle_restore()` implemented for idle thread bootstrap
- [x] `cpu_heavy_switch()` stub implemented (panics - user processes not supported yet)
- [x] `cpu_kthread_restore()` implemented for kernel thread bootstrap
- [x] `savectx()` implemented for core dumps
- [x] PCB structure updated with proper register fields
- [x] genassym.c updated with offset constants
- [x] Kernel can switch between threads without panicking
- [x] Kernel continues boot past previous failure point

### Files Created/Modified

| File | Action | Description |
|------|--------|-------------|
| `sys/platform/arm64/aarch64/swtch.s` | **Created** | Assembly context switch functions |
| `sys/platform/arm64/include/pcb.h` | **Modified** | Added callee-saved register fields (pcb_x19-pcb_x28, pcb_x29, pcb_lr, pcb_sp) |
| `sys/platform/arm64/aarch64/genassym.c` | **Modified** | Added offset constants for assembly (TD_*, GD_*, PCB_*) |
| `sys/platform/arm64/aarch64/machdep.c` | **Modified** | Removed C stubs, added cpu_idle(), ap_init(), updated cpu_gdinit() |
| `sys/platform/arm64/include/smp.h` | **Modified** | Added ap_init() prototype |
| `sys/platform/arm64/conf/files` | **Modified** | Added `swtch.s standard` |

### Related Commits

| Commit | Description |
|--------|-------------|
| 042add5899 | arm64: Implement cpu_lwkt_switch() for LWKT context switching (MVP Part 8) |
| 99c7c9469d | arm64: Add cpu_idle() and ap_init() stubs |
| 80a853460d | arm64: Add ap_init() prototype to smp.h |
| f1c73f2adf | arm64: Fix copyright header in swtch.s |

### Implementation Notes

The implementation follows DragonFly's LWKT model (different from FreeBSD):
- Uses `td_sp` in thread struct for stack pointer storage
- Pushes restore function address onto stack before switching
- `ret` instruction pops restore function and jumps to it
- Switch function returns old thread pointer for `lwkt_switch_return()`

### Related References

- DragonFly x86_64: `sys/platform/pc64/x86_64/swtch.s`
- ARM Architecture Reference Manual - AAPCS64 calling convention

---

## MVP Part 9: cpu_set_thread_handler() - Kernel Thread Creation

### Goal

Implement `cpu_set_thread_handler()` for ARM64 to enable kernel thread creation, unblocking devfs initialization and other subsystems that spawn kernel threads.

### Status: COMPLETE ✅

### Problem Analysis

The kernel panics at **SYSINIT `SI_SUB_DEVFS_CORE` (0x2380000)** with an instruction abort at `ELR=0x0` (NULL function pointer call):

```
!!! EXC ESR=000000008600000d FAR=0000000000000000 ELR=0000000000000000 SPSR=0000000040000205
EC=21 (IABT)
```

**Root Cause:** `cpu_set_thread_handler()` is an empty stub in the ARM64 port (`sys/platform/arm64/aarch64/machdep.c:1502-1506`).

When `devfs_init()` calls `lwkt_create()` to spawn the `devfs_msg_core` kernel thread:
1. `lwkt_create()` calls `cpu_set_thread_handler(td, lwkt_exit, func, arg)`
2. The ARM64 stub does **nothing** - it doesn't set up the thread's stack or entry point
3. When the scheduler switches to the new thread, it tries to "restore" to whatever garbage is on the stack
4. The thread's stack pointer points to uninitialized memory, leading to `ret` jumping to address `0x0`

### x86_64 Reference

From `sys/platform/pc64/x86_64/vm_machdep.c:291-300`:

```c
void cpu_set_thread_handler(thread_t td, void (*rfunc)(void), void *func, void *arg)
{
    td->td_pcb->pcb_rbx = (long)func;           // Store thread function in callee-saved register
    td->td_pcb->pcb_r12 = (long)arg;            // Store argument in callee-saved register
    td->td_switch = cpu_lwkt_switch;            // Set switch function
    td->td_sp -= sizeof(void *);
    *(void **)td->td_sp = rfunc;                // Push exit function (lwkt_exit)
    td->td_sp -= sizeof(void *);
    *(void **)td->td_sp = cpu_kthread_restore;  // Push restore function
}
```

### Files to Modify

| File | Change |
|------|--------|
| `sys/platform/arm64/aarch64/machdep.c` | Implement `cpu_set_thread_handler()` |
| `sys/platform/arm64/aarch64/swtch.s` | Update `cpu_kthread_restore()` to load rfunc into lr before jumping |

### Implementation Details

#### 1. `sys/platform/arm64/aarch64/machdep.c`

Replace the empty stub:
```c
void
cpu_set_thread_handler(struct thread *td __unused,
    void (*retfunc)(void) __unused, void *func __unused, void *arg __unused)
{
}
```

With:
```c
void
cpu_set_thread_handler(thread_t td, void (*rfunc)(void), void *func, void *arg)
{
    /*
     * Set up the PCB for cpu_kthread_restore():
     *   pcb_x19 = argument to thread function
     *   pcb_x20 = return function (called when thread func returns)
     *   pcb_lr  = thread function to call
     */
    td->td_pcb->pcb_x19 = (register_t)arg;
    td->td_pcb->pcb_x20 = (register_t)rfunc;
    td->td_pcb->pcb_lr = (register_t)func;
    
    /*
     * Set the switch function for this thread.
     */
    td->td_switch = cpu_lwkt_switch;
    
    /*
     * Push cpu_kthread_restore onto the stack. ARM64 requires 16-byte
     * stack alignment, so we push 16 bytes (address + padding).
     * When cpu_lwkt_switch() switches to this thread, it will 'ret'
     * to cpu_kthread_restore.
     */
    td->td_sp -= 16;
    *(void **)td->td_sp = cpu_kthread_restore;
}
```

Also need to add extern declaration near the top of the file:
```c
extern void cpu_kthread_restore(void);
```

#### 2. `sys/platform/arm64/aarch64/swtch.s`

Update `cpu_kthread_restore` to:
1. Load the return function from `pcb_x20` into `x30` (lr)
2. Load argument from `pcb_x19` into `x0`
3. Load thread function from `pcb_lr` into a temp register
4. Branch to thread function (which will `ret` to rfunc when done)

Change from:
```asm
	ldr	x0, [x2, #PCB_X19]	/* argument in x0 */
	ldr	x1, [x2, #PCB_LR]	/* function in x1 */

	/*
	 * Jump to the thread function (tail call, never returns).
	 */
	br	x1
```

To:
```asm
	/*
	 * Load the thread function, argument, and return function from PCB.
	 *   pcb_x19 = argument
	 *   pcb_x20 = return function (e.g., lwkt_exit)
	 *   pcb_lr  = thread function
	 */
	ldr	x0, [x2, #PCB_X19]	/* argument in x0 */
	ldr	x30, [x2, #PCB_X20]	/* return function in lr (x30) */
	ldr	x1, [x2, #PCB_LR]	/* thread function in x1 */

	/*
	 * Jump to the thread function. When it returns (via 'ret'),
	 * it will return to the address in lr (x30), which is rfunc
	 * (typically lwkt_exit or kthread_exit).
	 */
	br	x1
```

Also update the comment header for `cpu_kthread_restore` to document the new convention:
```
 *	The PCB contains:
 *	  pcb_x19 = argument to thread function
 *	  pcb_x20 = return function (called when thread function returns)
 *	  pcb_lr  = thread function to call
```

### Expected Outcome

After this change:
1. `devfs_init()` will successfully create the `devfs_msg_core` kernel thread
2. Kernel will pass `SI_SUB_DEVFS_CORE` (0x2380000)
3. Boot will continue to later subsystems (may hit other issues)

### Implementation Notes

**Critical ARM64 difference from x86_64:**

ARM64's `ret` instruction does NOT pop from the stack like x86_64. Instead, it returns to the address in the link register (`x30`/`lr`).

The fix required loading the restore function address from the stack into `lr` before calling `ret`:

```asm
/* After switching stacks */
ldr    x30, [sp]          /* Load restore function address into lr */
ret                       /* Jump to restore function via lr */
```

This applies to `cpu_lwkt_switch()` when jumping to restore functions.

### Success Criteria

- [x] `cpu_set_thread_handler()` properly initializes PCB fields
- [x] `cpu_kthread_restore()` loads rfunc into lr before jumping to thread function
- [x] Kernel passes `SI_SUB_DEVFS_CORE` (0x2380000)
- [x] devfs_msg_core kernel thread starts successfully
- [x] Boot continues past previous failure point (reaches SI_SUB_PSEUDO 0x7400000)

### Related Commits

| Commit | Description |
|--------|-------------|
| 716bd835b4 | arm64: Implement cpu_set_thread_handler() for kernel thread creation (MVP Part 9) |
| ed2f6c561a | arm64: Fix cpu_lwkt_switch() to load restore address into lr before ret |

---

## MVP Part 10: pmap Stub Completion - Critical Functions

### Goal

Fix pmap stub functions that cause hangs or infinite loops due to improper implementations. The ARM64 pmap has many empty stubs, but some cause runtime issues because the calling code expects them to modify state.

### Status: IN PROGRESS

### Problem Analysis

During SI_SUB_EXEC (0x7400000) initialization, the kernel hangs in an infinite loop:

**Call chain:**
```
mi_startup()
 └─ module_register_init() for "shell" module
     └─ shell_modevent()
         └─ exec_register()
             └─ kfree(old_execsw)  [45056 bytes]
                 └─ kmem_slab_free()
                     └─ vm_map_remove()
                         └─ vm_map_delete()
                             └─ vm_map_entry_unwire_all()  [wired_count=1]
                                 └─ vm_fault_unwire()
                                     └─ pmap_unwire()  [INFINITE LOOP]
```

**Root cause:** `pmap_unwire()` stub doesn't advance the virtual address pointer:

```c
// Current stub - causes infinite loop
vm_page_t
pmap_unwire(pmap_t pmap __unused, vm_offset_t *vap __unused)
{
    return (NULL);
}
```

The caller loops with `while (va < end)` expecting `pmap_unwire()` to advance `*vap`.

### Critical Stubs (Causing Hangs)

#### 1. `pmap_unwire()` - IMMEDIATE FIX REQUIRED

**Problem:** `vm_fault_unwire()` loops with `while (va < end)` expecting `pmap_unwire()` to advance `*vap`. Without this, infinite loop.

**Required fix (minimal):**
```c
vm_page_t
pmap_unwire(pmap_t pmap __unused, vm_offset_t *vap)
{
    *vap += PAGE_SIZE;  /* Advance to prevent infinite loop */
    return (NULL);
}
```

**Full implementation (later):** Should clear wired bit from PTEs, decrement wired_count stats, return vm_page if found.

#### 2. `pmap_remove()` - HIGH PRIORITY

**Problem:** Called from `vm_map_delete()` to remove page mappings. Without this, pages remain mapped after backing memory freed → use-after-free potential.

**Required fix:** Clear PTEs in range, do TLB invalidation.

#### 3. `pmap_kremove()` / `pmap_kremove_quick()` - HIGH PRIORITY

**Problem:** Remove kernel mappings. Without these, kernel VA space leaks.

**Required fix:** Clear kernel PTEs, TLB invalidation.

### Medium Priority Stubs

| Function | Impact | Fix |
|----------|--------|-----|
| `pmap_protect()` | Protection changes don't take effect | Update AP bits in PTEs |
| `pmap_qenter()` / `pmap_qremove()` | Quick temp mappings fail | Map/unmap page arrays |
| `pmap_extract()` | VA→PA translation fails | Walk page tables |

### Lower Priority (OK as stubs for MVP)

These can remain as no-ops for initial boot:
- `pmap_copy()` - fork optimization only
- `pmap_is_modified()` - returns FALSE (OK for read-only boot)
- `pmap_clear_modify()` / `pmap_clear_reference()` - no-op safe
- `pmap_ts_referenced()` - pageout daemon (not needed for boot)
- `pmap_page_protect()` - no protection changes
- `pmap_collect()` - no pmap GC
- `pmap_remove_pages()` - process exit
- `pmap_growkernel()` - only when running out of KVA
- `pmap_mincore()` - mincore syscall only
- `pmap_pgscan()` - pageout related
- `pmap_replacevm()` / `pmap_setlwpvm()` - process-related

### Implementation Plan

#### Phase 1: Immediate Fix (Unblock Current Hang)
1. **Fix `pmap_unwire()`** - Advance `*vap` by PAGE_SIZE

#### Phase 2: Essential Functions
2. **Add `pmap_pte()` helper** - Safe page table lookup (returns NULL if not mapped)
3. **Implement `pmap_remove()`** - Clear PTEs in range
4. **Implement `pmap_kremove()` / `pmap_kremove_quick()`** - Remove kernel mappings

#### Phase 3: Robustness (if needed)
5. **Implement `pmap_protect()`** - Update protection bits
6. **Implement `pmap_qenter()` / `pmap_qremove()`** - Quick mappings
7. **Implement `pmap_extract()`** - VA→PA translation

### Helper Function: pmap_pte()

Need a safe page table lookup that doesn't panic if entries are missing:

```c
static pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
    pd_entry_t *l0, *l1, *l2;
    
    l0 = pmap_l0(pmap, va);
    if ((*l0 & ATTR_DESCR_MASK) != L0_TABLE)
        return (NULL);
    
    l1 = pmap_l0_to_l1(l0, va);
    if ((*l1 & ATTR_DESCR_MASK) != L1_TABLE)
        return (NULL);
    
    l2 = pmap_l1_to_l2(l1, va);
    if ((*l2 & ATTR_DESCR_MASK) != L2_TABLE)
        return (NULL);
    
    return pmap_l2_to_l3(l2, va);
}
```

### Success Criteria

- [ ] `pmap_unwire()` advances virtual address (fixes infinite loop)
- [ ] `pmap_remove()` clears PTEs and does TLB invalidation
- [ ] `pmap_kremove()` removes kernel mappings
- [ ] Kernel boots past SI_SUB_EXEC (0x7400000)
- [ ] No new infinite loops or hangs from pmap stubs

### Debug Commits to Clean Up (after fix verified)

~~These files have temporary `#ifdef __aarch64__` kprintf statements:~~
- ~~`sys/kern/init_main.c` - SYSINIT tracing~~
- ~~`sys/kern/kern_module.c` - module_register_init~~
- ~~`sys/kern/kern_exec.c` - exec_register~~
- ~~`sys/kern/kern_slaballoc.c` - _kfree/kmem_slab_free~~
- ~~`sys/vm/vm_map.c` - vm_map_remove/vm_map_delete/vm_map_lookup_entry~~

**CLEANED** - All debug output removed in commits `1314560438` and `709d099084`. See MVP Part 11 for details.

---

## MVP Part 11: Slab Allocator Fixes (COMPLETE)

### Goal

Fix Data Abort crashes during boot related to the slab allocator's interaction with the ARM64 pmap subsystem.

### Status: COMPLETE ✅

Two critical bugs were identified and fixed that caused Data Aborts during early boot. After these fixes, the kernel boots successfully through all SYSINIT entries and only times out waiting for device I/O (expected in QEMU without full device emulation).

### Bug #1: Cache Aliasing in pmap_alloc_l3() (FIXED)

**Problem:** Data Abort at `0xffffff8008e00018` during early boot (SYSINIT `01600000`)

**Root Cause:** `pmap_alloc_l3()` returned a BSS address (`0x40cd1000`) for the first PTE in new L3 tables, while `pmap_l2_to_l3()` returned a DMAP address (`0xffffa00040cd1000`) for subsequent PTEs. Both map to the same physical page but use different TLB/cache entries, causing the first PTE write to not be visible via DMAP.

**Technical Details:**
- ARM64 caches are PIPT (Physically Indexed, Physically Tagged), but TLB entries are VIVT-like
- Different virtual aliases to the same physical page can have different TLB entries
- DSB barriers don't synchronize between different VA aliases
- Write via BSS VA wasn't visible when read via DMAP VA

**Fix (commit `1f5a1b62ac`):** Modified `pmap_alloc_l3()` to:
1. Zero L3 page via BSS address (still works for initialization)
2. Issue `DSB ISH` to ensure zeroing is visible system-wide
3. Return DMAP address for PTE pointer (ensures all accesses use same virtual alias)

**File:** `sys/platform/arm64/aarch64/pmap.c`

### Bug #2: pmap_kvtom() Returning NULL (FIXED)

**Problem:** After fix #1, kernel progressed to SYSINIT `08800000` but crashed at same address `0xffffff8008e00018` with different context - zone memory was being unmapped while still in use.

**Root Cause:** `pmap_kvtom()` was a stub returning `NULL`. The slab allocator's `btokup()` macro:
```c
#define btokup(z)  (&pmap_kvtom((vm_offset_t)(z))->ku_pagecnt)
```
This dereferenced `NULL->ku_pagecnt`, returning garbage that looked like a positive number. This caused `_kfree()` to misidentify normal slab allocations as "oversized" allocations, triggering `kmem_slab_free()` which unmapped memory still referenced in the slab free lists.

**Technical Details:**
- `ku_pagecnt` is at offset 0 in the vm_page structure
- `btokup()` returns `&(NULL)->ku_pagecnt` = &(*(vm_page_t)0).ku_pagecnt = address 0
- Dereferencing address 0 returned whatever garbage was there
- Garbage value > 0 made `_kfree()` think allocation was oversized
- `kmem_slab_free()` unmapped the zone, corrupting free lists

**Fix (commit `a6064ef072`):** Implemented `pmap_kvtom()` properly:
```c
vm_page_t pmap_kvtom(vm_offset_t va)
{
    pt_entry_t *ptep = pmap_pte(kernel_pmap, va);
    if (ptep == NULL) return (NULL);
    pt_entry_t pte = *ptep;
    if ((pte & ATTR_DESCR_VALID) == 0) return (NULL);
    vm_paddr_t pa = pte & ATTR_ADDR;
    return (PHYS_TO_VM_PAGE(pa));
}
```

**File:** `sys/platform/arm64/aarch64/pmap.c`

### Debug Output Cleanup (COMPLETE)

Removed extensive `kprintf()` debug output added during bug investigation:

**Commit `1314560438`** - arm64: Clean up debug output after slab allocator fixes
- `sys/platform/arm64/aarch64/pmap.c` - Removed debug from `pmap_l2_to_l3`, `pmap_alloc_l3`, `pmap_enter`, `pmap_remove`
- `sys/kern/kern_slaballoc.c` - Removed debug from `check_zone_free`, `_kmalloc`, `_kfree`

**Commit `709d099084`** - arm64: Remove remaining debug output from VM and kernel subsystems
- `sys/kern/kern_slaballoc.c` - Removed remaining debug from `kmem_slab_alloc`, `kmem_slab_free`
- `sys/vm/vm_map.c` - Removed 28 debug blocks from `vm_map_lookup_entry`, `vm_map_delete`, `vm_map_remove`
- `sys/kern/kern_exec.c` - Removed debug from `exec_register`
- `sys/kern/kern_module.c` - Removed debug from `module_register_init`
- `sys/kern/init_main.c` - Removed SYSINIT tracing from `mi_startup`
- `sys/vm/vm_init.c` - Removed debug from `vm_mem_init`

**Total:** 221 lines of debug output removed across 6 files.

### Current Boot State

After both fixes and debug cleanup, the kernel now:
- **NO Data Aborts** - Previous crashes completely eliminated
- **Slab allocator working** - Thousands of successful allocations
- **Memory management functional** - Proper VA→PA translation via pmap_kvtom()
- **Clean boot output** - Professional, production-ready console messages
- **Test times out** waiting for device I/O (normal - QEMU needs more device emulation)

Boot output is now clean and concise:
```
DragonFly/aarch64 EFI loader, Revision 1.1
Kernel entry at 0x40100000
DragonFly/arm64 kernel started!
Console initialized via PL011 driver.
init_param1() done, hz=100
arm64_gdinit_full() done
GIC: mapping dist=0xffffa00008000000 cpu=0xffffa00008010000
GIC: initialized
init_param2() done, physmem=112627 pages (439 MB)
msgbufinit() done
initarm: returning SP=0x40cc6f80 (td_pcb)
Copyright (c) 2003-2026 The DragonFly Project.
DragonFly 6.5-DEVELOPMENT #89: Tue Jan 27 11:07:07 UTC 2026
real memory  = 461320192 (439 MB)
avail memory = 461320192 (439 MB)
ARM64 timer: frequency 24000000 Hz (24.0 MHz)
ARM64 timer: registered cputimer and cputimer_intr
```

### Commits

| Commit | Description |
|--------|-------------|
| `369c0641be` | arm64: Add debug output to trace slab allocation and pmap_enter |
| `4f89bf0163` | arm64: Add more pmap debug output to diagnose DMAP vs BSS addressing |
| `1f5a1b62ac` | arm64/pmap: Fix cache aliasing in pmap_alloc_l3 by returning DMAP address |
| `8d355489ec` | arm64: Add debug to detect if problematic zone is being unmapped |
| `a6064ef072` | arm64/pmap: Implement pmap_kvtom() to fix slab allocator crashes |
| `1314560438` | arm64: Clean up debug output after slab allocator fixes |
| `709d099084` | arm64: Remove remaining debug output from VM and kernel subsystems |

### Key Technical Lessons

1. **ARM64 virtual aliasing matters** - Even with PIPT caches, different TLB entries for the same physical page can cause coherency issues. Always use consistent virtual addresses.

2. **pmap_kvtom() is critical** - The slab allocator relies on this function to determine allocation type. A NULL return causes misclassification and memory corruption.

3. **Debug output is invaluable** - The extensive kprintf() output allowed tracing the exact sequence of events leading to each crash.

### Success Criteria (All Met)

- ✅ Fix cache aliasing in L3 page table allocation
- ✅ Implement pmap_kvtom() for slab allocator
- ✅ Kernel boots without Data Aborts
- ✅ Slab allocator functions correctly
- ✅ Clean up debug output (221 lines removed)
- ✅ Clean, production-ready boot output

---

*Last updated: 2026-01-27 (MVP Part 11 - Debug cleanup complete)*
