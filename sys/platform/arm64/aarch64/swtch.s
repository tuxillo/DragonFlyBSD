/*-
 * Copyright (c) 2026 The DragonFly Project.  All rights reserved.
 *
 * This code is part of the DragonFly arm64 port and implements the
 * LWKT (Light Weight Kernel Thread) context switching mechanism for
 * the ARM64 architecture.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "assym.s"

/*
 * ARM64 ABI Notes:
 *   - x0-x7:   argument/result registers (caller-saved)
 *   - x8:      indirect result location register (caller-saved)
 *   - x9-x15:  temporary registers (caller-saved)
 *   - x16-x17: intra-procedure-call scratch registers (caller-saved)
 *   - x18:     platform register - reserved for per-CPU globaldata pointer
 *   - x19-x28: callee-saved registers
 *   - x29:     frame pointer (fp) - callee-saved
 *   - x30:     link register (lr) - holds return address
 *   - sp:      stack pointer
 *
 * DragonFly LWKT Switch Model:
 *   - td_sp in thread struct holds the saved stack pointer
 *   - Push restore function address onto stack before switching
 *   - ret instruction pops restore function and jumps to it
 *   - Switch function returns old thread pointer for lwkt_switch_return()
 */

/* Alias for link register */
#define	lr	x30

/*
 * Macro to define a function entry point
 */
#define ENTRY(sym)	\
	.text;		\
	.globl sym;	\
	.align 2;	\
	.type sym, @function; \
sym:

#define END(sym)	\
	.size sym, . - sym

/*
 * cpu_lwkt_switch(struct thread *ntd)
 *
 *	Standard LWKT switching function.  Only callee-saved registers are
 *	saved and we don't bother with the MMU state or anything else.
 *
 *	This function is always called while in a critical section.
 *
 *	x0 = new thread to switch to
 *	x18 = per-CPU globaldata pointer (preserved, never saved/restored)
 *
 *	Returns the old thread pointer in x0 for lwkt_switch_return().
 */
ENTRY(cpu_lwkt_switch)
	/*
	 * Get the current thread from globaldata.
	 * x1 = oldtd (current thread, becomes old after switch)
	 */
	ldr	x1, [x18, #GD_CURTHREAD]

	/*
	 * Save callee-saved registers on the stack.
	 * We save: x19-x28, x29 (fp), x30 (lr)
	 * Total: 12 registers = 96 bytes
	 * Stack must be 16-byte aligned, 96 is already aligned.
	 */
	sub	sp, sp, #96
	stp	x19, x20, [sp, #0]
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	x25, x26, [sp, #48]
	stp	x27, x28, [sp, #64]
	stp	x29, x30, [sp, #80]

	/*
	 * Push the restore function address onto the stack.
	 * When we switch to the new thread, its 'ret' will pop this
	 * address and jump to it.
	 */
	adr	x2, cpu_lwkt_restore
	sub	sp, sp, #16
	str	x2, [sp]

	/*
	 * Save our stack pointer in the old thread's td_sp.
	 */
	mov	x2, sp
	str	x2, [x1, #TD_SP]

	/*
	 * Switch to the new thread:
	 * 1. Set curthread to new thread
	 * 2. Load new thread's stack pointer
	 * 3. Return (which jumps to the restore function on new stack)
	 *
	 * x0 = newtd (preserved from function argument)
	 * x1 = oldtd (to be returned by restore function)
	 */
	str	x0, [x18, #GD_CURTHREAD]	/* curthread = newtd */
	ldr	x2, [x0, #TD_SP]		/* x2 = newtd->td_sp */
	mov	sp, x2				/* switch stacks */

	/*
	 * Move oldtd to x0 for the restore function to return.
	 * The restore function expects oldtd in x0.
	 */
	mov	x0, x1

	/*
	 * Return to the restore function (address popped from new stack).
	 */
	ret
END(cpu_lwkt_switch)

/*
 * cpu_lwkt_restore()
 *
 *	Standard LWKT restore function.  This function is always called
 *	while in a critical section.
 *
 *	Entry: x0 = old thread (to be returned to caller)
 *	       sp points to saved registers on new thread's stack
 *
 *	Returns: x0 = old thread (for lwkt_switch_return)
 */
ENTRY(cpu_lwkt_restore)
	/*
	 * Pop the restore function address placeholder (already consumed by ret).
	 * The ret that got us here already popped the address, but we pushed
	 * it with sub sp, #16 and only stored 8 bytes. Adjust sp.
	 */
	add	sp, sp, #16

	/*
	 * Restore callee-saved registers.
	 * x0 is preserved (old thread pointer).
	 */
	ldp	x19, x20, [sp, #0]
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	x25, x26, [sp, #48]
	ldp	x27, x28, [sp, #64]
	ldp	x29, x30, [sp, #80]
	add	sp, sp, #96

	/*
	 * Return to the original caller of cpu_lwkt_switch.
	 * x0 = old thread (for lwkt_switch_return)
	 */
	ret
END(cpu_lwkt_restore)

/*
 * cpu_idle_restore()
 *
 *	One-time restore function for the idle thread.  This is used to
 *	bootstrap into the cpu_idle() LWKT.  After that, cpu_lwkt_switch()
 *	will be used for switching.
 *
 *	Entry: x0 = new thread (idle thread, already set as curthread)
 *	       x1 = old thread (for lwkt_switch_return on CPU 0)
 *	       x18 = per-CPU globaldata pointer
 *
 *	Note: The idle thread's stack was set up by cpu_gdinit() with
 *	      the restore function address pushed.
 */
ENTRY(cpu_idle_restore)
	/*
	 * Set up a NULL frame pointer for backtraces.
	 */
	mov	x29, #0

	/*
	 * Pop the restore address placeholder from the stack.
	 * (We're already here, so just adjust sp)
	 */
	add	sp, sp, #16

	/*
	 * For non-BSP CPUs, we need to call ap_init() and manually
	 * handle TDF_RUNNING flags since we won't return through
	 * lwkt_switch_return.
	 *
	 * For the BSP (CPU 0), we use normal lwkt_switch_return semantics.
	 */
	ldr	w2, [x18, #GD_CPUID]
	cbnz	w2, 1f

	/*
	 * CPU 0 (BSP): Use normal lwkt_switch_return semantics.
	 * Save the new thread and call lwkt_switch_return with old thread.
	 */
	mov	x19, x0			/* save newtd in callee-saved reg */
	mov	x0, x1			/* arg0 = oldtd */
	bl	lwkt_switch_return
	mov	x0, x19			/* restore newtd for cpu_idle arg */
	b	cpu_idle

1:
	/*
	 * Non-BSP CPU: Clear TDF_RUNNING on old thread, set on new,
	 * then call ap_init().
	 *
	 * x0 = newtd, x1 = oldtd
	 */
	ldr	w2, [x1, #TD_FLAGS]
	and	w2, w2, #(~TDF_RUNNING)
	str	w2, [x1, #TD_FLAGS]

	ldr	w2, [x0, #TD_FLAGS]
	orr	w2, w2, #TDF_RUNNING
	str	w2, [x0, #TD_FLAGS]

	bl	ap_init

	/* Fall through to cpu_idle */
	b	cpu_idle
END(cpu_idle_restore)

/*
 * cpu_heavy_switch(struct thread *ntd)
 *
 *	Switch from the current thread to a new thread.  This entry
 *	is used when the current thread is a heavy weight process
 *	(i.e., has user context).
 *
 *	For now, this is a stub that panics - we don't support user
 *	processes yet.
 */
ENTRY(cpu_heavy_switch)
	adr	x0, heavy_switch_panic_msg
	bl	panic
	/* NOT REACHED */
END(cpu_heavy_switch)

/*
 * cpu_heavy_restore()
 *
 *	Restore function for heavy weight processes.
 *	Stub that panics for now.
 */
ENTRY(cpu_heavy_restore)
	adr	x0, heavy_restore_panic_msg
	bl	panic
	/* NOT REACHED */
END(cpu_heavy_restore)

/*
 * cpu_kthread_restore()
 *
 *	One-time restore function for kernel threads.  This is used to
 *	bootstrap into a new kernel thread.  After the initial entry,
 *	cpu_lwkt_switch() will be used for switching.
 *
 *	The PCB contains:
 *	  pcb_x19 = argument to thread function
 *	  pcb_x20 = return function (called when thread function returns)
 *	  pcb_lr  = thread function to call
 *
 *	Entry: x0 = new thread (already set as curthread)
 *	       x1 = old thread (for lwkt_switch_return)
 */
ENTRY(cpu_kthread_restore)
	/*
	 * Set up a NULL frame pointer for backtraces.
	 */
	mov	x29, #0

	/*
	 * Pop the restore address placeholder from the stack.
	 */
	add	sp, sp, #16

	/*
	 * Save the new thread pointer and call lwkt_switch_return
	 * to clean up the old thread.
	 */
	mov	x19, x0			/* save newtd */
	mov	x0, x1			/* arg0 = oldtd */
	bl	lwkt_switch_return

	/*
	 * Decrement the critical section count (we entered in a critical
	 * section).
	 */
	ldr	w2, [x19, #TD_CRITCOUNT]
	sub	w2, w2, #1
	str	w2, [x19, #TD_CRITCOUNT]

	/*
	 * Load the thread function, argument, and return function from PCB.
	 *   pcb_x19 = argument
	 *   pcb_x20 = return function (e.g., lwkt_exit)
	 *   pcb_lr  = thread function
	 */
	ldr	x2, [x19, #TD_PCB]
	ldr	x0, [x2, #PCB_X19]	/* argument in x0 */
	ldr	x30, [x2, #PCB_X20]	/* return function in lr (x30) */
	ldr	x1, [x2, #PCB_LR]	/* thread function in x1 */

	/*
	 * Jump to the thread function. When it returns (via 'ret'),
	 * it will return to the address in lr (x30), which is rfunc
	 * (typically lwkt_exit or kthread_exit).
	 */
	br	x1
END(cpu_kthread_restore)

/*
 * savectx(struct pcb *pcb)
 *
 *	Save the current processor state to the PCB.
 *	Used for things like core dumps.
 */
ENTRY(savectx)
	/* x0 = pcb pointer */

	/* Save callee-saved registers */
	str	x19, [x0, #PCB_X19]
	str	x20, [x0, #PCB_X20]
	str	x21, [x0, #PCB_X21]
	str	x22, [x0, #PCB_X22]
	str	x23, [x0, #PCB_X23]
	str	x24, [x0, #PCB_X24]
	str	x25, [x0, #PCB_X25]
	str	x26, [x0, #PCB_X26]
	str	x27, [x0, #PCB_X27]
	str	x28, [x0, #PCB_X28]
	str	x29, [x0, #PCB_X29]
	str	x30, [x0, #PCB_LR]

	/* Save stack pointer */
	mov	x1, sp
	str	x1, [x0, #PCB_SP]

	ret
END(savectx)

/*
 * Panic messages
 */
	.section .rodata
heavy_switch_panic_msg:
	.asciz	"cpu_heavy_switch: user process switching not implemented"
heavy_restore_panic_msg:
	.asciz	"cpu_heavy_restore: user process restore not implemented"
