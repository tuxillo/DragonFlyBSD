# ARM64 PL011 UART Driver Implementation Plan

## Status: PHASE 1 COMPLETE ✅

**Phase 1 (Minimal Complete Console Driver) is fully implemented and working.**

The PL011 console driver now provides:
- Full console framework integration (`cn_init_fini`, `cn_getc`, `cn_checkc`)
- `/dev/ttya` device node for userland access
- Proper DMAP-based UART mapping after MMU enable
- No "Unable to hook console!" warning
- Clean `mountroot>` prompt with working input

### Key Fixes Applied

1. **`cn_init_fini` implemented** - Creates `/dev/ttya` device node
2. **`cn_getc` implemented** - Polling receive with FIFO wait
3. **`cn_checkc` fixed** (commit `eb531d26ca`) - Was returning boolean (0/1) instead of character/-1, causing spurious characters after prompts
4. **DMAP mapping** - Uses `PHYS_TO_DMAP()` for UART access after MMU enable

## Current State Analysis

### Existing PL011 Driver (`sys/dev/serial/pl011/pl011_cons.c`)

The driver is now a **functional minimal console** with all required functions:

### DragonFly Console Framework Expectations

All complete console drivers in DragonFly implement the full `struct consdev` interface:

| Driver | `cn_init_fini` Implementation | Device Created | Userland Node |
|--------|----------------------------|----------------|---------------|
| `sio` (x86_64 serial) | `siocninit_fini()` | Finds existing `ttydX` | `/dev/ttydX`, `/dev/cuaaX` |
| `syscons` (x86_64 VGA) | `sccninit_fini()` | Assigns `cctl_dev` | Control device |
| `vcons` (vkernel) | `vconsinit_fini()` | Creates `ttyv0-ttyv7` | `/dev/ttyvX` |
| `dcons` (debug) | `dcons_cninit_fini()` | Creates `dcons` | `/dev/dcons` |
| `pl011` (ARM64) | `pl011_cninit_fini()` ✅ | Creates `ttya` | `/dev/ttya` |

**Root Cause of Crash**: The function `cninit_finish()` in `sys/kern/tty_cons.c` calls `cn_tab->cn_init_fini(cn_tab)` without checking for NULL. Our temporary fix added a NULL check, but the proper solution is to implement `cn_init_fini`.

### FreeBSD Reference Comparison

FreeBSD has a comprehensive UART framework (`sys/dev/uart/`) with:
- **UART bus abstraction**: `uart_bus`, `uart_core`, `uart_tty`
- **PL011 as UART device**: `uart_dev_pl011.c` implements UART ops
- **Early printf support**: `EARLY_PRINTF=pl011` option
- **Full interrupt-driven I/O**: With FIFO support and hardware flow control

## Three-Phase Implementation Plan

### Phase 1: Minimal Complete Console Driver - **COMPLETE** ✅
**Goal**: Fix `cn_init_fini` and add basic missing functions while maintaining polling mode.

**Status**: All Phase 1 objectives achieved.

**Files modified:**
1. `sys/dev/serial/pl011/pl011_cons.c`
2. `sys/dev/serial/pl011/pl011_reg.h`

**Implementation completed:**

- ✅ `pl011_cninit_fini()` - Creates `/dev/ttya` device node
- ✅ `pl011_cngetc()` - Polling receive with RX FIFO wait
- ✅ `pl011_cncheckc()` - Returns -1 when no char, or the character value (fixed in commit `eb531d26ca`)
- ✅ DMAP-based UART mapping after MMU enable

**Phase 1 Success Criteria (All Met):**
- ✅ `/dev/ttya` device created and accessible
- ✅ No "Unable to hook console!" warning
- ✅ Basic input via polling (`cngetc`/`cncheckc`)
- ✅ No spurious characters in console output
- ✅ Clean `mountroot>` prompt with working keyboard input
- ✅ No regression in boot output

### Phase 2: Full PL011 Bus Driver (FUTURE)
**Goal**: Create proper bus-attached PL011 driver following sio/serial pattern.
**Status**: Not started - Phase 1 is sufficient for current MVP needs.

**New files:**
1. `sys/dev/serial/pl011/pl011.c` - Main bus driver
2. `sys/dev/serial/pl011/pl011_fdt.c` - FDT attachment (future)
3. `sys/dev/serial/pl011/pl011_isa.c` - ISA-style attachment for hardcoded

**Driver structure (following sio pattern):**

```c
/* Similar to struct com_s in sio.c */
struct pl011_softc {
    device_t        sc_dev;          /* Parent device */
    struct tty     *sc_tty;          /* TTY structure */
    struct resource *sc_memres;      /* Memory resource */
    struct resource *sc_irqres;      /* IRQ resource */
    void           *sc_ih;           /* Interrupt handler */
    bus_space_tag_t sc_bst;          /* Bus space tag */
    bus_space_handle_t sc_bsh;       /* Bus space handle */
    int             sc_unit;         /* Unit number */
    uint32_t        sc_hwmtx;        /* Hardware mutex */
    /* PL011-specific state */
    int             sc_fifodepth;    /* FIFO depth (16 for PL011) */
    int             sc_rxinttrig;    /* RX interrupt trigger level */
    int             sc_txinttrig;    /* TX interrupt trigger level */
};
```

**Key components:**

1. **Device methods** (`device_method_t pl011_methods[]`):
   - `pl011_probe()` - Detect PL011 at address/from FDT
   - `pl011_attach()` - Setup resources, create tty, register interrupt
   - `pl011_detach()` - Cleanup resources

2. **TTY operations** (`struct dev_ops pl011_ops`):
   ```c
   static struct dev_ops pl011_ops = {
       { "pl011", 0, D_MPSAFE },
       .d_open = pl011_open,
       .d_close = pl011_close,
       .d_read = pl011_read,
       .d_write = pl011_write,
       .d_ioctl = pl011_ioctl,
       .d_kqfilter = tty_kqfilter,
   };
   ```

3. **Interrupt handler**:
   ```c
   static int
   pl011_intr(void *arg)
   {
       struct pl011_softc *sc = arg;
       uint32_t mis, icr = 0;
       
       mis = PL011_READ(sc, PL011_MIS);
       
       /* RX interrupt - data available */
       if (mis & PL011_RXINTR) {
           pl011_rxintr(sc);
           icr |= PL011_RXINTR;
       }
       
       /* TX interrupt - ready to send */
       if (mis & PL011_TXINTR) {
           pl011_txintr(sc);
           icr |= PL011_TXINTR;
       }
       
       /* Clear handled interrupts */
       if (icr)
           PL011_WRITE(sc, PL011_ICR, icr);
       
       return FILTER_HANDLED;
   }
   ```

4. **FIFO management**:
   - PL011 has 16-byte TX/RX FIFOs
   - Configure interrupt levels via IFLS register
   - Enable/disable FIFO via LCR_H_FEN bit

**Console integration updates:**
- Modify `pl011_cninit_fini()` to find device created by bus driver
- Similar to sio's `siocninit_fini()` using `devfs_find_device_by_name()`

**Phase 2 Success Criteria:**
- ✅ Interrupt-driven I/O (reduces CPU usage)
- ✅ Proper TTY layer integration (termios support)
- ✅ Multiple instance support (`ttya`, `ttyb`, etc.)
- ✅ FIFO utilization and flow control
- ✅ FDT probing when FDT support added

### Phase 3: Advanced Features and Framework Integration (FUTURE)
**Goal**: Port FreeBSD UART framework or implement equivalent advanced features.
**Status**: Not started - Phase 1 polling driver is sufficient for MVP.

**Two possible approaches:**

#### Option A: Port FreeBSD UART Framework
**Files to port from FreeBSD `.freebsd.orig/sys/dev/uart/`:**
- `uart_bus.c`, `uart_core.c`, `uart_tty.c` - Core framework
- `uart_bus_fdt.c`, `uart_bus_acpi.c` - Bus attachments
- `uart_dev_pl011.c` - PL011 backend
- `uart.h`, `uart_bus.h`, `uart_cpu.h` - Headers

**Benefits:**
- Standardized UART interface across architectures
- Early console support (`EARLY_PRINTF`)
- Automatic baud rate detection
- Hardware abstraction for different UART types
- Battle-tested code base

**Challenges:**
- Major porting effort (10,000+ lines of code)
- Integration with DragonFly device subsystem
- Potential conflicts with existing sio driver patterns

#### Option B: Enhance Standalone Driver (DragonFly Way)
**Features to add:**
1. **Full termios support**: All `ioctl()` operations
2. **Hardware flow control**: RTS/CTS implementation
3. **DMA support**: For PL011 variants with DMA capability
4. **Power management**: Clock gating, sleep states
5. **Advanced debugging**: Sysctl nodes for register inspection
6. **Performance optimizations**: Batched FIFO transfers

**Benefits:**
- Simpler, less code to maintain
- Follows existing sio pattern
- No framework dependency
- Easier to debug and modify

**Recommended path**: Start with Option B, evaluate need for Option A based on:
- Requirement for other ARM64 UART types (NS16550, IMX, etc.)
- Complexity of supporting multiple UART variants
- Maintenance burden vs. framework benefits

**Phase 3 Success Criteria:**
- ✅ Full termios compliance (all `ioctl()` operations work)
- ✅ Hardware flow control (RTS/CTS) functional
- ✅ Power management (clock gating in idle)
- ✅ DMA support if hardware allows
- ✅ Performance benchmarks showing efficiency

## Implementation Timeline and Dependencies

### Current Status Summary

| Phase | Status | Key Deliverables |
|-------|--------|------------------|
| Phase 1 | ✅ **COMPLETE** | Fixed `cn_init_fini`, `/dev/ttya`, polling input, cncheckc fix |
| Phase 2 | 📋 Future | Interrupt driver, TTY integration, FDT probing |
| Phase 3 | 📋 Future | Advanced features or framework port |

### Dependencies
1. **ARM64 FDT support**: Required for dynamic device discovery in Phase 2
2. **GIC interrupt framework**: Already implemented, used for IRQ handling
3. **Device tree parsing**: Needed for FDT attachment in Phase 2
4. **PMAP mapping functions**: Already available via `pmap_mapdev()`

### Risk Assessment and Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Interrupt handling bugs | High | Medium | Start with polling, add interrupts incrementally |
| FDT integration issues | Medium | High | Implement hardcoded fallback, phase FDT support |
| Performance regression | Medium | Low | Benchmark before/after, optimize critical paths |
| TTY layer complexity | High | Medium | Follow sio pattern closely, reuse existing TTY code |
| Framework port complexity | High | High | Evaluate Option B first, only port if necessary |

## Testing Strategy

### Unit Testing
1. **Register-level tests**: Verify PL011 register access works
2. **FIFO tests**: Test RX/TX FIFO functionality
3. **Interrupt tests**: Verify interrupt masking/clearing

### Integration Testing
1. **Console boot test**: Ensure kernel boots with PL011 as console
2. **TTY layer test**: Verify `getty`, `login`, shell work on `/dev/ttya`
3. **Performance test**: Measure interrupt rate, CPU usage, throughput

### System Testing
1. **QEMU virt machine**: Primary test platform (0x09000000)
2. **Real hardware**: Test on Raspberry Pi 3/4 (if PL011 available)
3. **Stress test**: High baud rate (115200+), heavy load

## References

### DragonFly Source Files
- `sys/dev/serial/sio/sio.c` - Reference serial driver implementation
- `sys/kern/tty_cons.c` - Console framework (`cninit_finish()`)
- `sys/platform/vkernel64/platform/console.c` - `vconsinit_fini()` example
- `sys/dev/misc/dcons/dcons_os.c` - `dcons_cninit_fini()` example

### FreeBSD Source Files (`.freebsd.orig/`)
- `sys/dev/uart/uart_dev_pl011.c` - PL011 UART driver
- `sys/dev/uart/uart_bus_fdt.c` - FDT bus attachment
- `sys/dev/uart/uart_core.c` - UART core framework
- `sys/dev/uart/uart_tty.c` - TTY integration

### ARM Architecture References
- ARM PrimeCell UART (PL011) Technical Reference Manual
- ARM System Memory Management Unit Architecture Specification
- QEMU virt machine documentation (PL011 at 0x09000000)

## Conclusion

**Phase 1 is complete.** The PL011 console driver now provides all required console framework functions and the kernel boots cleanly to the `mountroot>` prompt with working keyboard input.

The three-phase plan provides a pragmatic approach to evolving the PL011 driver from minimal early console to production-quality serial driver. Phase 1 (complete) addresses the immediate console needs. Phase 2 and Phase 3 can be implemented as needed for multi-user operation, interrupt-driven I/O, or real hardware support.

For the current MVP milestone (boot to mountroot), the Phase 1 polling driver is sufficient.

---

*Document Version: 1.1*  
*Last Updated: 2026-01-28*  
*Status: Phase 1 Complete*