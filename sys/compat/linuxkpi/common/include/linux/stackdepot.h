/*-
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _LINUXKPI_LINUX_STACKDEPOT_H_
#define	_LINUXKPI_LINUX_STACKDEPOT_H_

/*
 * depot_stack_handle_t must be an unsigned type that can hold -1 as a
 * sentinel value (cast to unsigned). Using unsigned int matches Linux.
 */
typedef unsigned int depot_stack_handle_t;

/*
 * Stack depot stubs for DragonFly BSD.
 * These are no-op implementations that allow code using stack depot
 * for debugging to compile, but no actual stack traces are captured.
 */

static inline depot_stack_handle_t
stack_depot_save(unsigned long *entries __unused, unsigned int nr_entries __unused,
    gfp_t gfp_flags __unused)
{
	return 0;	/* No stack saved */
}

static inline void
stack_depot_init(void)
{
	/* Nothing to initialize */
}

static inline int
stack_depot_snprint(depot_stack_handle_t handle __unused, char *buf,
    size_t size, int spaces __unused)
{
	if (buf && size > 0)
		buf[0] = '\0';
	return 0;
}

#endif	/* _LINUXKPI_LINUX_STACKDEPOT_H_ */
