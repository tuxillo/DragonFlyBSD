# ARM64 PL011 UART Driver Implementation Plan

## Executive Summary

This document outlines a three-phase plan to evolve the ARM64 PL011 UART driver from its current minimal early console implementation to a full-featured, interrupt-driven serial driver with proper device integration. The immediate goal is to fix the `cn_init_fini` NULL pointer issue, while the long-term vision is to provide a production-quality serial driver following DragonFly BSD conventions.

## Current State Analysis

### Existing PL011 Driver (`sys/dev/serial/pl011/pl011_cons.c`)

The current driver is a **minimal early console only** with significant limitations:

- **Only 3 functions implemented**: `cn_probe`, `cn_init`, `cn_putc`
- **Missing critical functions**: `cn_init_fini`, `cn_getc`, `cn_checkc`
- **No device tie-in**: Doesn't create `/dev/ttya` node for userland access
- **Polling mode only**: No interrupt support, wasteful CPU usage
- **Hardcoded QEMU address**: Fixed at 0x09000000 with no dynamic discovery
- **Console warning**: "Unable to hook console!" appears due to missing `cn_init_fini`

### DragonFly Console Framework Expectations

All complete console drivers in DragonFly implement the full `struct consdev` interface:

| Driver | `cn_init_fini` Implementation | Device Created | Userland Node |
|--------|----------------------------|----------------|---------------|
| `sio` (x86_64 serial) | `siocninit_fini()` | Finds existing `ttydX` | `/dev/ttydX`, `/dev/cuaaX` |
| `syscons` (x86_64 VGA) | `sccninit_fini()` | Assigns `cctl_dev` | Control device |
| `vcons` (vkernel) | `vconsinit_fini()` | Creates `ttyv0-ttyv7` | `/dev/ttyvX` |
| `dcons` (debug) | `dcons_cninit_fini()` | Creates `dcons` | `/dev/dcons` |
| `pl011` (current) | **NULL** (missing) | No device | None |

**Root Cause of Crash**: The function `cninit_finish()` in `sys/kern/tty_cons.c` calls `cn_tab->cn_init_fini(cn_tab)` without checking for NULL. Our temporary fix added a NULL check, but the proper solution is to implement `cn_init_fini`.

### FreeBSD Reference Comparison

FreeBSD has a comprehensive UART framework (`sys/dev/uart/`) with:
- **UART bus abstraction**: `uart_bus`, `uart_core`, `uart_tty`
- **PL011 as UART device**: `uart_dev_pl011.c` implements UART ops
- **Early printf support**: `EARLY_PRINTF=pl011` option
- **Full interrupt-driven I/O**: With FIFO support and hardware flow control

## Three-Phase Implementation Plan

### Phase 1: Minimal Complete Console Driver (Immediate)
**Goal**: Fix `cn_init_fini` and add basic missing functions while maintaining polling mode.

**Files to modify:**
1. `sys/dev/serial/pl011/pl011_cons.c`
2. `sys/dev/serial/pl011/pl011_reg.h`

**Implementation details:**

```c
/* Add to pl011_reg.h */
#define PL011_IMSC    0x010  /* Interrupt Mask Set/Clear Register */
#define PL011_RIS     0x014  /* Raw Interrupt Status Register */
#define PL011_MIS     0x018  /* Masked Interrupt Status Register */
#define PL011_ICR     0x01C  /* Interrupt Clear Register */

/* Add to pl011_cons.c */
static void
pl011_cninit_fini(struct consdev *cp)
{
    /*
     * Create /dev/ttya for userland access.
     * In Phase 2, this will find the device created by the bus driver.
     */
    cp->cn_dev = make_dev(&pl011_ops, 0, 
                         UID_ROOT, GID_WHEEL, 0600, "ttya");
}

static int
pl011_cngetc(void *arg)
{
    volatile uint32_t *base = (volatile uint32_t *)arg;
    
    /* Wait for data in RX FIFO */
    while (base[PL011_FR / 4] & PL011_FR_RXFE)
        cpu_pause();
    
    return base[PL011_DR / 4] & 0xFF;
}

static int
pl011_cncheckc(void *arg)
{
    volatile uint32_t *base = (volatile uint32_t *)arg;
    
    /* Check if RX FIFO has data */
    return !(base[PL011_FR / 4] & PL011_FR_RXFE);
}

/* Update CONS_DRIVER macro */
CONS_DRIVER(pl011, pl011_cnprobe, pl011_cninit, pl011_cninit_fini,
    NULL, pl011_cngetc, pl011_cncheckc, pl011_cnputc, NULL, NULL);
```

**Phase 1 Success Criteria:**
- ✅ `/dev/ttya` device created and accessible
- ✅ No "Unable to hook console!" warning
- ✅ Basic input via polling (`cngetc`/`cncheckc`)
- ✅ `getty` and login work on serial console
- ✅ No regression in boot output

### Phase 2: Full PL011 Bus Driver (Medium-term)
**Goal**: Create proper bus-attached PL011 driver following sio/serial pattern.

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

### Phase 3: Advanced Features and Framework Integration (Long-term)
**Goal**: Port FreeBSD UART framework or implement equivalent advanced features.

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

### Timeline Estimate
| Phase | Duration | Key Deliverables |
|-------|----------|------------------|
| Phase 1 | 1-2 days | Fixed `cn_init_fini`, `/dev/ttya`, polling input |
| Phase 2 | 2-4 weeks | Interrupt driver, TTY integration, FDT probing |
| Phase 3 | 1-3 months | Advanced features or framework port |

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

The three-phase plan provides a pragmatic approach to evolving the PL011 driver from minimal early console to production-quality serial driver. Phase 1 addresses the immediate NULL pointer issue with minimal risk. Phase 2 follows established DragonFly patterns for device drivers. Phase 3 offers a decision point between enhancing the standalone driver or porting the FreeBSD UART framework based on long-term needs.

The recommended approach is to proceed with Phase 1 immediately, then implement Phase 2 following the successful sio driver pattern. Phase 3 should be evaluated based on the broader ARM64 port requirements for supporting multiple UART types.

---

*Document Version: 1.0*  
*Last Updated: 2026-01-27*  
*Author: AI Coding Agent*  
*Status: Draft - For Review*