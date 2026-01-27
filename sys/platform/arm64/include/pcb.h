/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 2026 The DragonFly Project.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#ifndef _SYS_TYPES_H_
#include <sys/types.h>
#endif

struct pcb {
	/*
	 * Callee-saved registers (x19-x28).
	 * These must be preserved across function calls per AAPCS64.
	 */
	register_t	pcb_x19;
	register_t	pcb_x20;
	register_t	pcb_x21;
	register_t	pcb_x22;
	register_t	pcb_x23;
	register_t	pcb_x24;
	register_t	pcb_x25;
	register_t	pcb_x26;
	register_t	pcb_x27;
	register_t	pcb_x28;

	/*
	 * Frame pointer and link register.
	 */
	register_t	pcb_x29;	/* Frame pointer (fp) */
	register_t	pcb_lr;		/* Link register (x30) - return address */

	/*
	 * Stack pointer.
	 */
	register_t	pcb_sp;

	/*
	 * Fault handler address for copyin/copyout.
	 */
	register_t	pcb_onfault;

	/*
	 * PCB flags.
	 */
	uint32_t	pcb_flags;
#define	PCB_SINGLE_STEP_SHIFT	0
#define	PCB_SINGLE_STEP		(1 << PCB_SINGLE_STEP_SHIFT)
};

#ifdef _KERNEL
void	savectx(struct pcb *);
#endif

#endif /* _MACHINE_PCB_H_ */
