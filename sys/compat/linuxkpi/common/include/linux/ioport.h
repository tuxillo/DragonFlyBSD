/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Serenity Cyber Security, LLC.
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
 */

#ifndef _LINUXKPI_LINUX_IOPORT_H
#define _LINUXKPI_LINUX_IOPORT_H

#include <linux/compiler.h>
#include <linux/types.h>
#ifdef __DragonFly__
#include <sys/rman.h>
#endif

/*
 * Linux uses 'struct resource' with 'start' and 'end' members.
 * DragonFly's struct resource uses 'r_start' and 'r_end'.
 * We define accessor macros instead of redefining 'start'/'end'
 * which would break other code (e.g., drm_mm_node.start).
 */
#ifdef __DragonFly__
#define DEFINE_RES_MEM(_start, _size)		\
	(struct resource) {			\
		.r_start = (_start),		\
		.r_end = (_start) + (_size) - 1,	\
	}

static inline resource_size_t
resource_start(const struct resource *r)
{
	return (r->r_start);
}

static inline resource_size_t
resource_end(const struct resource *r)
{
	return (r->r_end);
}

static inline void
resource_set_start(struct resource *r, resource_size_t val)
{
	r->r_start = val;
}

static inline void
resource_set_end(struct resource *r, resource_size_t val)
{
	r->r_end = val;
}
#else
#define DEFINE_RES_MEM(_start, _size)		\
	(struct resource) {			\
		.start = (_start),		\
		.end = (_start) + (_size) - 1,	\
	}

struct resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
};

static inline resource_size_t
resource_start(const struct resource *r)
{
	return (r->start);
}

static inline resource_size_t
resource_end(const struct resource *r)
{
	return (r->end);
}

static inline void
resource_set_start(struct resource *r, resource_size_t val)
{
	r->start = val;
}

static inline void
resource_set_end(struct resource *r, resource_size_t val)
{
	r->end = val;
}
#endif

static inline resource_size_t
resource_size(const struct resource *r)
{
	return (resource_end(r) - resource_start(r) + 1);
}

static inline bool
resource_contains(struct resource *a, struct resource *b)
{
	return (resource_start(a) <= resource_start(b) &&
		resource_end(a) >= resource_end(b));
}

#endif
