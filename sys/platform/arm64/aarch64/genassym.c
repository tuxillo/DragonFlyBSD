/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Derived from FreeBSD: /sys/arm64/arm64/genassym.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/assym.h>
#include <sys/proc.h>
#include <sys/globaldata.h>

#include <machine/frame.h>
#include <machine/globaldata.h>
#include <machine/pcb.h>

/*
 * Thread structure offsets (DragonFly LWKT)
 */
ASSYM(TD_PCB, offsetof(struct thread, td_pcb));
ASSYM(TD_SP, offsetof(struct thread, td_sp));
ASSYM(TD_PRI, offsetof(struct thread, td_pri));
ASSYM(TD_CRITCOUNT, offsetof(struct thread, td_critcount));
ASSYM(TD_FLAGS, offsetof(struct thread, td_flags));
ASSYM(TD_WCHAN, offsetof(struct thread, td_wchan));
ASSYM(TD_NEST_COUNT, offsetof(struct thread, td_nest_count));
ASSYM(TD_LWP, offsetof(struct thread, td_lwp));
ASSYM(TD_PROC, offsetof(struct thread, td_proc));
ASSYM(TD_GD, offsetof(struct thread, td_gd));

ASSYM(TDF_RUNNING, TDF_RUNNING);

/*
 * Globaldata structure offsets
 * Note: DragonFly uses mdglobaldata which embeds globaldata as 'mi'
 */
ASSYM(GD_CURTHREAD, offsetof(struct mdglobaldata, mi.gd_curthread));
ASSYM(GD_CPUID, offsetof(struct mdglobaldata, mi.gd_cpuid));
ASSYM(GD_CNT, offsetof(struct mdglobaldata, mi.gd_cnt));
ASSYM(GD_INTR_NESTING_LEVEL, offsetof(struct mdglobaldata, mi.gd_intr_nesting_level));

/*
 * PCB structure offsets (callee-saved registers for context switch)
 */
ASSYM(PCB_X19, offsetof(struct pcb, pcb_x19));
ASSYM(PCB_X20, offsetof(struct pcb, pcb_x20));
ASSYM(PCB_X21, offsetof(struct pcb, pcb_x21));
ASSYM(PCB_X22, offsetof(struct pcb, pcb_x22));
ASSYM(PCB_X23, offsetof(struct pcb, pcb_x23));
ASSYM(PCB_X24, offsetof(struct pcb, pcb_x24));
ASSYM(PCB_X25, offsetof(struct pcb, pcb_x25));
ASSYM(PCB_X26, offsetof(struct pcb, pcb_x26));
ASSYM(PCB_X27, offsetof(struct pcb, pcb_x27));
ASSYM(PCB_X28, offsetof(struct pcb, pcb_x28));
ASSYM(PCB_X29, offsetof(struct pcb, pcb_x29));
ASSYM(PCB_LR, offsetof(struct pcb, pcb_lr));
ASSYM(PCB_SP, offsetof(struct pcb, pcb_sp));
ASSYM(PCB_ONFAULT, offsetof(struct pcb, pcb_onfault));
ASSYM(PCB_FLAGS, offsetof(struct pcb, pcb_flags));
ASSYM(PCB_SIZE, sizeof(struct pcb));
