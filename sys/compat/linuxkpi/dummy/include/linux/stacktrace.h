/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The DragonFly Project
 *
 * Stub implementation of Linux stacktrace API for DragonFly BSD.
 * This allows code that captures stack traces for debugging to compile,
 * but no actual stack traces are captured.
 */

#ifndef _LINUXKPI_LINUX_STACKTRACE_H_
#define _LINUXKPI_LINUX_STACKTRACE_H_

/*
 * stack_trace_save - Save current stack trace into an array
 * @store: Pointer to storage array
 * @size: Size of the storage array
 * @skipnr: Number of entries to skip at the start
 *
 * Returns: Number of entries stored (0 in this stub)
 */
static inline unsigned int
stack_trace_save(unsigned long *store __unused, unsigned int size __unused,
    unsigned int skipnr __unused)
{
	return 0;	/* No stack trace captured */
}

#endif /* _LINUXKPI_LINUX_STACKTRACE_H_ */
