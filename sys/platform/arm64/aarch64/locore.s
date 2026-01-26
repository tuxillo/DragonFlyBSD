/*-
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

.equ	BOOT_STACK_SIZE, 16384
/*
 * ARM64 Kernel Entry Point (Stub)
 *
 * Minimal kernel entry for MVP Part 2/3. It receives control from the EFI
 * loader, sets up a basic identity-mapped page table, enables the MMU,
 * prints a message to the PL011 UART, and halts.
 *
 * Entry:
 *   x0 = modulep (pointer to preload metadata)
 *
 * QEMU virt machine PL011 UART is at 0x09000000
 */

.equ	SCTLR_M, 0x00000001
.equ	SCTLR_C, 0x00000004
.equ	SCTLR_I, 0x00001000
.equ	SCTLR_RES1, 0x30d00900
.equ	SCTLR_MMU_OFF, SCTLR_RES1
.equ	SCTLR_MMU_ON, 0x30d01905

.equ	TCR_VALUE, 0xb5103510

/*
 * MAIR_EL1 - Memory Attribute Indirection Register
 * Must match VM_MEMATTR_* indices in machine/vm.h:
 *   Index 0: Device-nGnRnE (0x00)
 *   Index 1: Normal Non-cacheable (0x44)
 *   Index 2: Normal Write-back (0xff)
 *   Index 3: Normal Write-through (0xbb)
 *   Index 4: Device-nGnRE (0x04)
 */
.equ	MAIR_VALUE, 0x000004bbff4400

.equ	PTE_BLOCK, 0x1
.equ	PTE_TABLE, 0x3
.equ	PTE_AF, 0x400
.equ	PTE_SH_INNER, 0x300
.equ	PTE_AP_RO, 0x80
.equ	PTE_ATTRIDX_NORMAL, 0x8		/* Index 2 (write-back) << 2 */
.equ	PTE_PXN, 0x0020000000000000
.equ	PTE_UXN, 0x0040000000000000

/*
 * PTE flags for block mappings (using MAIR index 2 = write-back):
 *   Bits [1:0]  = 0x1  (block descriptor)
 *   Bits [4:2]  = 0x2  (MAIR index 2 = write-back)
 *   Bits [9:8]  = 0x3  (inner shareable)
 *   Bit  [10]   = 0x1  (access flag)
 *   Bit  [53]   = PXN (privileged execute never)
 *   Bit  [54]   = UXN (user execute never)
 */
.equ	PTE_BLOCK_DEVICE, 0x0060000000000701	/* MAIR idx 0 (device), PXN+UXN */
.equ	PTE_BLOCK_NORMAL, 0x0000000040000709	/* MAIR idx 2 (WB), SH=IS, AF */
.equ	PTE_L2_TEXT, 0x0000000040000789		/* MAIR idx 2 (WB), SH=IS, AF, AP=RO */
.equ	PTE_L2_DATA, 0x0060000040200709		/* MAIR idx 2 (WB), SH=IS, AF, PXN+UXN */

.equ	KERNBASE, 0xffffff8000000000
.equ	KERN_LOAD, 0x0000000040000000
.equ	KERNBASE_OFFSET, KERNBASE - KERN_LOAD

	.text
	.globl	_start
	.type	_start, @function

_start:
	/*
	 * Save modulep in a callee-saved register in case we need it later.
	 * For now, we just print and halt.
	 */
	mov	x19, x0			/* Save modulep */

	/* Ensure MMU and caches are off before reprogramming TTBRs */
	mrs	x1, sctlr_el1
	bic	x1, x1, #SCTLR_M
	bic	x1, x1, #SCTLR_C
	bic	x1, x1, #SCTLR_I
	msr	sctlr_el1, x1
	isb

	/* Build minimal identity page tables and enable MMU */
	bl	create_pagetables
	bl	start_mmu

	/* Set up a temporary stack for early C code */
	adrp	x1, initstack_end
	add	x1, x1, :lo12:initstack_end
	mov	sp, x1

	/*
	 * Zero the BSS section.
	 * This is critical - global variables in BSS must be zero-initialized
	 * before any C code runs. Without this, structures like vm_page_queues[]
	 * will contain garbage, causing crashes in vm_page_startup().
	 */
	ldr	x15, .Lbss_start
	ldr	x14, .Lbss_end
1:
	cmp	x15, x14
	b.hs	2f			/* Exit if x15 >= x14 */
	stp	xzr, xzr, [x15], #16	/* Store 16 bytes of zeros, post-increment */
	b	1b
2:

	/* Call early C entry with modulep */
	mov	x0, x19
	bl	initarm

	/*
	 * initarm() returns the kernel stack pointer (thread0.td_pcb).
	 * Set SP to this value before calling mi_startup().
	 */
	mov	x20, x0			/* Save returned SP in x20 */

	/* Debug: print a marker character 'A' to UART directly */
	ldr	x1, =0x09000000		/* PL011 UART base */
	mov	w2, #'A'
	strb	w2, [x1]
	mov	w2, #'\r'
	strb	w2, [x1]
	mov	w2, #'\n'
	strb	w2, [x1]

	/*
	 * Print the SP value (x20) directly using nibble extraction.
	 * This avoids any memory loads from hex_digits table.
	 */
	mov	x3, #60			/* Start from top nibble */
2:
	lsr	x4, x20, x3		/* Shift value right */
	and	x4, x4, #0xf		/* Extract nibble */
	cmp	x4, #10
	blt	3f
	add	x4, x4, #('a' - 10)	/* a-f */
	b	4f
3:
	add	x4, x4, #'0'		/* 0-9 */
4:
	strb	w4, [x1]		/* Print to UART */
	subs	x3, x3, #4
	bge	2b

	mov	w2, #'\r'
	strb	w2, [x1]
	mov	w2, #'\n'
	strb	w2, [x1]

	/* Debug: print another marker 'B' */
	mov	w2, #'B'
	strb	w2, [x1]
	mov	w2, #'\r'
	strb	w2, [x1]
	mov	w2, #'\n'
	strb	w2, [x1]

	mov	sp, x20			/* Switch to new stack */

	/* Debug: print marker 'C' after stack switch */
	mov	w2, #'C'
	strb	w2, [x1]
	mov	w2, #'\r'
	strb	w2, [x1]
	mov	w2, #'\n'
	strb	w2, [x1]

	/*
	 * Call mi_startup() to run SYSINIT entries.
	 * This never returns - it eventually calls cpu_idle().
	 */
	bl	mi_startup

	/* Should not reach here */
	b	halt

	/*
	 * Halt: infinite loop with WFI (Wait For Interrupt)
	 * This is the proper way to halt on ARM64
	 */
halt:
	wfi
	b	halt

exc_sync_msg:
	.asciz	"\r\n[arm64] sync exception: ESR=0x"
exc_far_msg:
	.asciz	" FAR=0x"
exc_elr_msg:
	.asciz	" ELR=0x"
exc_end_msg:
	.asciz	"\r\n"
sp_msg:
	.asciz	"[arm64] SP from initarm: 0x"
mi_startup_msg:
	.asciz	"[arm64] calling mi_startup()\r\n"
newline_msg:
	.asciz	"\r\n"
hex_digits:
	.asciz	"0123456789abcdef"

	/* Literal pool for BSS start/end addresses */
	.align	3
.Lbss_start:
	.quad	__bss_start
.Lbss_end:
	.quad	__bss_end

	.align	11
exception_vectors:
	/* Current EL with SP0 (not used) */
	b	exception_sync		/* Synchronous */
	.align 7
	b	exception_irq		/* IRQ */
	.align 7
	b	exception_fiq		/* FIQ */
	.align 7
	b	exception_serror	/* SError */
	.align 7
	/* Current EL with SPx (our normal case) */
	b	exception_sync		/* Synchronous */
	.align 7
	b	exception_irq		/* IRQ */
	.align 7
	b	exception_fiq		/* FIQ */
	.align 7
	b	exception_serror	/* SError */
	.align 7
	/* Lower EL using AArch64 (not used yet) */
	b	exception_sync
	.align 7
	b	exception_irq
	.align 7
	b	exception_fiq
	.align 7
	b	exception_serror
	.align 7
	/* Lower EL using AArch32 (not used) */
	b	exception_spin
	.align 7
	b	exception_spin
	.align 7
	b	exception_spin
	.align 7
	b	exception_spin

exception_sync:
	/*
	 * Synchronous exception handler - print diagnostic info.
	 * We must be very careful here - the system is in an unknown state.
	 * Save all registers we use to stack first, use only inline UART I/O.
	 */
	
	/* Save registers we'll use (x0-x7, x30) */
	sub	sp, sp, #80
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	str	x30, [sp, #64]
	
	/* Read exception registers immediately before anything else */
	mrs	x4, esr_el1		/* Exception Syndrome Register */
	mrs	x5, far_el1		/* Fault Address Register */
	mrs	x6, elr_el1		/* Exception Link Register */
	mrs	x7, spsr_el1		/* Saved PSTATE */
	
	/* UART base address */
	mov	x1, #0x09000000
	
	/* Print newline and header */
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'X'
	strb	w0, [x1]
	mov	w0, #'C'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	
	/* Print "ESR=" */
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'S'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print ESR value (x4) as hex - inline */
	mov	x2, x4			/* Value to print */
	mov	x3, #60			/* Start from bit 60 (top nibble) */
1:
	lsr	x0, x2, x3		/* Shift right */
	and	x0, x0, #0xf		/* Extract nibble */
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	/* Print " FAR=" */
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'F'
	strb	w0, [x1]
	mov	w0, #'A'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print FAR value (x5) as hex */
	mov	x2, x5
	mov	x3, #60
1:
	lsr	x0, x2, x3
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	/* Print " ELR=" */
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'L'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print ELR value (x6) as hex */
	mov	x2, x6
	mov	x3, #60
1:
	lsr	x0, x2, x3
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	/* Print " SPSR=" */
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'S'
	strb	w0, [x1]
	mov	w0, #'P'
	strb	w0, [x1]
	mov	w0, #'S'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print SPSR value (x7) as hex */
	mov	x2, x7
	mov	x3, #60
1:
	lsr	x0, x2, x3
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	/* Print newline */
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	
	/* 
	 * Decode ESR exception class (bits 31:26) and print human-readable info.
	 * Common exception classes:
	 *   0x00 = Unknown
	 *   0x21 = Instruction abort from lower EL
	 *   0x22 = Instruction abort from same EL
	 *   0x24 = Data abort from lower EL
	 *   0x25 = Data abort from same EL
	 *   0x26 = SP alignment fault
	 *   0x2c = FP exception
	 *   0x3c = BRK instruction
	 */
	lsr	x2, x4, #26		/* Extract EC field */
	and	x2, x2, #0x3f
	
	/* Print "EC=" */
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'C'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print EC as 2-digit hex */
	lsr	x0, x2, #4
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	4f
	add	x0, x0, #('a' - 10)
	b	5f
4:
	add	x0, x0, #'0'
5:
	strb	w0, [x1]
	and	x0, x2, #0xf
	cmp	x0, #10
	blt	6f
	add	x0, x0, #('a' - 10)
	b	7f
6:
	add	x0, x0, #'0'
7:
	strb	w0, [x1]
	
	/* Print exception type based on EC */
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'('
	strb	w0, [x1]
	
	/* Check for common exception types */
	cmp	x2, #0x25		/* Data abort same EL? */
	beq	print_data_abort
	cmp	x2, #0x24		/* Data abort lower EL? */
	beq	print_data_abort
	cmp	x2, #0x22		/* Instruction abort same EL? */
	beq	print_insn_abort
	cmp	x2, #0x21		/* Instruction abort lower EL? */
	beq	print_insn_abort
	cmp	x2, #0x00		/* Unknown? */
	beq	print_unknown
	b	print_other

print_data_abort:
	mov	w0, #'D'
	strb	w0, [x1]
	mov	w0, #'A'
	strb	w0, [x1]
	mov	w0, #'B'
	strb	w0, [x1]
	mov	w0, #'T'
	strb	w0, [x1]
	b	print_ec_done

print_insn_abort:
	mov	w0, #'I'
	strb	w0, [x1]
	mov	w0, #'A'
	strb	w0, [x1]
	mov	w0, #'B'
	strb	w0, [x1]
	mov	w0, #'T'
	strb	w0, [x1]
	b	print_ec_done

print_unknown:
	mov	w0, #'U'
	strb	w0, [x1]
	mov	w0, #'N'
	strb	w0, [x1]
	mov	w0, #'K'
	strb	w0, [x1]
	mov	w0, #'N'
	strb	w0, [x1]
	b	print_ec_done

print_other:
	mov	w0, #'?'
	strb	w0, [x1]
	mov	w0, #'?'
	strb	w0, [x1]
	mov	w0, #'?'
	strb	w0, [x1]
	mov	w0, #'?'
	strb	w0, [x1]

print_ec_done:
	mov	w0, #')'
	strb	w0, [x1]
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	
	/* Fall through to exception_spin */
	b	exception_spin

/*
 * IRQ exception handler - just print marker and spin
 */
exception_irq:
	mov	x1, #0x09000000
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'I'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'Q'
	strb	w0, [x1]
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	b	exception_spin

/*
 * FIQ exception handler - just print marker and spin
 */
exception_fiq:
	mov	x1, #0x09000000
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'F'
	strb	w0, [x1]
	mov	w0, #'I'
	strb	w0, [x1]
	mov	w0, #'Q'
	strb	w0, [x1]
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	b	exception_spin

/*
 * SError exception handler - print info and spin
 * SError is typically a hardware error (e.g., bus error)
 */
exception_serror:
	mov	x1, #0x09000000
	mrs	x4, esr_el1		/* Get ESR for info */
	mrs	x5, far_el1
	mrs	x6, elr_el1
	
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #'!'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'S'
	strb	w0, [x1]
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'S'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print ESR as hex */
	mov	x2, x4
	mov	x3, #60
1:
	lsr	x0, x2, x3
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	mov	w0, #' '
	strb	w0, [x1]
	mov	w0, #'E'
	strb	w0, [x1]
	mov	w0, #'L'
	strb	w0, [x1]
	mov	w0, #'R'
	strb	w0, [x1]
	mov	w0, #'='
	strb	w0, [x1]
	
	/* Print ELR as hex */
	mov	x2, x6
	mov	x3, #60
1:
	lsr	x0, x2, x3
	and	x0, x0, #0xf
	cmp	x0, #10
	blt	2f
	add	x0, x0, #('a' - 10)
	b	3f
2:
	add	x0, x0, #'0'
3:
	strb	w0, [x1]
	subs	x3, x3, #4
	bge	1b
	
	mov	w0, #'\r'
	strb	w0, [x1]
	mov	w0, #'\n'
	strb	w0, [x1]
	b	exception_spin

exception_spin:
	wfi
	b	exception_spin

uart_putc:
	ldr	x1, =0x09000000
	strb	w0, [x1]
	ret

uart_puts:
	ldrb	w1, [x0], #1
	cbz	w1, 1f
	mov	w0, w1
	bl	uart_putc
	b	uart_puts
1:
	ret

uart_puthex:
	ldr	x2, =hex_digits
	mov	x3, #60
1:
	lsr	x4, x0, x3
	and	x4, x4, #0xf
	ldrb	w4, [x2, x4]
	mov	w0, w4
	bl	uart_putc
	subs	x3, x3, #4
	b.ge	1b
	ret

create_pagetables:
	adrp	x1, ttbr0_l0
	add	x1, x1, :lo12:ttbr0_l0
	adrp	x2, ttbr0_l1
	add	x2, x2, :lo12:ttbr0_l1
	mov	x3, xzr
	mov	x4, #0

	/* Clear L0/L1 tables for TTBR0/TTBR1 (4 pages) */
1:
	stp	x3, x3, [x1], #16
	add	x4, x4, #16
	cmp	x4, #(4096 * 5)
	b.lo	1b

	/* L0 entry 0 -> L1 table */
	adrp	x1, ttbr0_l0
	add	x1, x1, :lo12:ttbr0_l0
	adrp	x2, ttbr0_l1
	add	x2, x2, :lo12:ttbr0_l1
	orr	x3, x2, #PTE_TABLE
	str	x3, [x1]

	/* L1 entry 0: device map for 0x00000000-0x3fffffff */
	ldr	x3, =PTE_BLOCK_DEVICE
	str	x3, [x2]

	/* L1 entry 1: normal map for 0x40000000-0x7fffffff */
	ldr	x3, =PTE_BLOCK_NORMAL
	str	x3, [x2, #8]

	/* TTBR1 L0 entry 511 -> L1 table */
	adrp	x1, ttbr1_l0
	add	x1, x1, :lo12:ttbr1_l0
	adrp	x2, ttbr1_l1
	add	x2, x2, :lo12:ttbr1_l1
	orr	x3, x2, #PTE_TABLE
	str	x3, [x1, #4088]

	/* TTBR1 L1 entry 0 -> L2 table */
	adrp	x1, ttbr1_l1
	add	x1, x1, :lo12:ttbr1_l1
	adrp	x2, ttbr1_l2
	add	x2, x2, :lo12:ttbr1_l2
	orr	x3, x2, #PTE_TABLE
	str	x3, [x1]

	/* TTBR1 L2 entry 0: RX text at 0x40000000 */
	ldr	x3, =PTE_L2_TEXT
	str	x3, [x2]

	/* TTBR1 L2 entry 1: RW/XN data at 0x40200000 */
	ldr	x3, =PTE_L2_DATA
	str	x3, [x2, #8]

	ret

start_mmu:
	dsb	sy

	ldr	x0, =exception_vectors
	msr	vbar_el1, x0

	adrp	x0, ttbr0_l0
	add	x0, x0, :lo12:ttbr0_l0
	msr	ttbr0_el1, x0
	adrp	x0, ttbr1_l0
	add	x0, x0, :lo12:ttbr1_l0
	msr	ttbr1_el1, x0
	isb

	tlbi	vmalle1
	dsb	sy
	isb

	ldr	x0, =MAIR_VALUE
	msr	mair_el1, x0
	ldr	x0, =TCR_VALUE
	msr	tcr_el1, x0
	isb

	ldr	x0, =SCTLR_MMU_ON
	msr	sctlr_el1, x0
	isb

	ret

	.size	_start, . - _start

	.bss
	.p2align	12
initstack:
	.space	BOOT_STACK_SIZE
initstack_end:
	.p2align	12
ttbr0_l0:
	.space	4096
ttbr0_l1:
	.space	4096
ttbr1_l0:
	.space	4096
ttbr1_l1:
	.space	4096
ttbr1_l2:
	.space	4096
