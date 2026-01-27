/*-
 * Copyright (c) 2005 Scott Long
 * Copyright (c) 2025 The DragonFly Project
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
 */

#ifndef _CPU_BUS_DMA_H_
#define _CPU_BUS_DMA_H_

#include <sys/types.h>

/*
 * Bus address and size types
 */
typedef uint64_t bus_addr_t;
typedef uint64_t bus_size_t;

typedef uint64_t bus_space_tag_t;
typedef uint64_t bus_space_handle_t;

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFFUL
#define BUS_SPACE_MAXSIZE_32BIT	0xFFFFFFFFUL
#define BUS_SPACE_MAXSIZE	(64 * 1024)	/* Maximum supported size */
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFFUL
#define BUS_SPACE_MAXADDR_32BIT	0xFFFFFFFFUL
#define BUS_SPACE_MAXADDR	0xFFFFFFFFFFFFFFFFUL

#define BUS_SPACE_UNRESTRICTED	(~0)	/* nsegments */

/*
 * ARM64 has only memory-mapped I/O, no separate I/O port space.
 * The bus_space_tag_t is unused for ARM64; all operations are memory.
 */
#define AARCH64_BUS_SPACE_MEM	0

/*
 * Map a region of device bus space into CPU virtual address space.
 */
int bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
		  bus_space_handle_t *);

/*
 * Unmap a region of device bus space.
 */
void bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);

/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */
static __inline int
bus_space_subregion(bus_space_tag_t t __unused, bus_space_handle_t bsh,
		    bus_size_t offset, bus_size_t size __unused,
		    bus_space_handle_t *nbshp)
{
	*nbshp = bsh + offset;
	return (0);
}

static __inline void *
bus_space_kva(bus_space_tag_t tag __unused, bus_space_handle_t handle,
	      bus_size_t offset)
{
	return ((void *)(handle + offset));
}

/*
 * Allocate a region of memory that is accessible to devices in bus space.
 */
int	bus_space_alloc(bus_space_tag_t t, bus_addr_t rstart,
			bus_addr_t rend, bus_size_t size, bus_size_t align,
			bus_size_t boundary, int flags, bus_addr_t *addrp,
			bus_space_handle_t *bshp);

/*
 * Free a region of bus space accessible memory.
 */
static __inline void
bus_space_free(bus_space_tag_t t __unused, bus_space_handle_t bsh __unused,
	       bus_size_t size __unused)
{
}

/*
 * Read a 1, 2, 4, or 8 byte quantity from bus space
 * described by tag/handle/offset.
 *
 * ARM64 is memory-only; all accesses are direct volatile pointer dereferences.
 */
static __inline u_int8_t
bus_space_read_1(bus_space_tag_t tag __unused, bus_space_handle_t handle,
		 bus_size_t offset)
{
	return (*(volatile u_int8_t *)(handle + offset));
}

static __inline u_int16_t
bus_space_read_2(bus_space_tag_t tag __unused, bus_space_handle_t handle,
		 bus_size_t offset)
{
	return (*(volatile u_int16_t *)(handle + offset));
}

static __inline u_int32_t
bus_space_read_4(bus_space_tag_t tag __unused, bus_space_handle_t handle,
		 bus_size_t offset)
{
	return (*(volatile u_int32_t *)(handle + offset));
}

#ifdef _KERNEL
static __inline u_int64_t
bus_space_read_8(bus_space_tag_t tag __unused, bus_space_handle_t handle,
		 bus_size_t offset)
{
	return (*(volatile u_int64_t *)(handle + offset));
}
#endif

/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle/offset and copy into buffer provided.
 */
static __inline void
bus_space_read_multi_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int8_t *addr, size_t count)
{
	while (count--)
		*addr++ = *(volatile u_int8_t *)(bsh + offset);
}

static __inline void
bus_space_read_multi_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t *addr, size_t count)
{
	while (count--)
		*addr++ = *(volatile u_int16_t *)(bsh + offset);
}

static __inline void
bus_space_read_multi_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t *addr, size_t count)
{
	while (count--)
		*addr++ = *(volatile u_int32_t *)(bsh + offset);
}

/*
 * Read `count' 1, 2, 4, or 8 byte quantities from bus space
 * described by tag/handle and starting at `offset' and copy into
 * buffer provided.
 */
static __inline void
bus_space_read_region_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, u_int8_t *addr, size_t count)
{
	bus_space_handle_t src = bsh + offset;

	while (count--) {
		*addr++ = *(volatile u_int8_t *)src;
		src++;
	}
}

static __inline void
bus_space_read_region_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, u_int16_t *addr, size_t count)
{
	bus_space_handle_t src = bsh + offset;

	while (count--) {
		*addr++ = *(volatile u_int16_t *)src;
		src += 2;
	}
}

static __inline void
bus_space_read_region_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, u_int32_t *addr, size_t count)
{
	bus_space_handle_t src = bsh + offset;

	while (count--) {
		*addr++ = *(volatile u_int32_t *)src;
		src += 4;
	}
}

/*
 * Write the 1, 2, 4, or 8 byte value `value' to bus space
 * described by tag/handle/offset.
 */
static __inline void
bus_space_write_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		  bus_size_t offset, u_int8_t value)
{
	*(volatile u_int8_t *)(bsh + offset) = value;
}

static __inline void
bus_space_write_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		  bus_size_t offset, u_int16_t value)
{
	*(volatile u_int16_t *)(bsh + offset) = value;
}

static __inline void
bus_space_write_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		  bus_size_t offset, u_int32_t value)
{
	*(volatile u_int32_t *)(bsh + offset) = value;
}

#ifdef _KERNEL
static __inline void
bus_space_write_8(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		  bus_size_t offset, u_int64_t value)
{
	*(volatile u_int64_t *)(bsh + offset) = value;
}
#endif

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer
 * provided to bus space described by tag/handle/offset.
 */
static __inline void
bus_space_write_multi_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, const u_int8_t *addr, size_t count)
{
	while (count--)
		*(volatile u_int8_t *)(bsh + offset) = *addr++;
}

static __inline void
bus_space_write_multi_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, const u_int16_t *addr, size_t count)
{
	while (count--)
		*(volatile u_int16_t *)(bsh + offset) = *addr++;
}

static __inline void
bus_space_write_multi_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			bus_size_t offset, const u_int32_t *addr, size_t count)
{
	while (count--)
		*(volatile u_int32_t *)(bsh + offset) = *addr++;
}

/*
 * Write `count' 1, 2, 4, or 8 byte quantities from the buffer provided
 * to bus space described by tag/handle starting at `offset'.
 */
static __inline void
bus_space_write_region_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int8_t *addr, size_t count)
{
	bus_space_handle_t dst = bsh + offset;

	while (count--) {
		*(volatile u_int8_t *)dst = *addr++;
		dst++;
	}
}

static __inline void
bus_space_write_region_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int16_t *addr, size_t count)
{
	bus_space_handle_t dst = bsh + offset;

	while (count--) {
		*(volatile u_int16_t *)dst = *addr++;
		dst += 2;
	}
}

static __inline void
bus_space_write_region_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
			 bus_size_t offset, const u_int32_t *addr, size_t count)
{
	bus_space_handle_t dst = bsh + offset;

	while (count--) {
		*(volatile u_int32_t *)dst = *addr++;
		dst += 4;
	}
}

/*
 * Write the 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle/offset `count' times.
 */
static __inline void
bus_space_set_multi_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		      bus_size_t offset, u_int8_t value, size_t count)
{
	while (count--)
		*(volatile u_int8_t *)(bsh + offset) = value;
}

static __inline void
bus_space_set_multi_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		      bus_size_t offset, u_int16_t value, size_t count)
{
	while (count--)
		*(volatile u_int16_t *)(bsh + offset) = value;
}

static __inline void
bus_space_set_multi_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		      bus_size_t offset, u_int32_t value, size_t count)
{
	while (count--)
		*(volatile u_int32_t *)(bsh + offset) = value;
}

/*
 * Write `count' 1, 2, 4, or 8 byte value `val' to bus space described
 * by tag/handle starting at `offset'.
 */
static __inline void
bus_space_set_region_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int8_t value, size_t count)
{
	bus_space_handle_t addr = bsh + offset;

	for (; count != 0; count--, addr++)
		*(volatile u_int8_t *)addr = value;
}

static __inline void
bus_space_set_region_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int16_t value, size_t count)
{
	bus_space_handle_t addr = bsh + offset;

	for (; count != 0; count--, addr += 2)
		*(volatile u_int16_t *)addr = value;
}

static __inline void
bus_space_set_region_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh,
		       bus_size_t offset, u_int32_t value, size_t count)
{
	bus_space_handle_t addr = bsh + offset;

	for (; count != 0; count--, addr += 4)
		*(volatile u_int32_t *)addr = value;
}

/*
 * Copy `count' 1, 2, 4, or 8 byte values from bus space starting
 * at tag/bsh1/off1 to bus space starting at tag/bsh2/off2.
 */
static __inline void
bus_space_copy_region_1(bus_space_tag_t tag __unused, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_space_handle_t addr1 = bsh1 + off1;
	bus_space_handle_t addr2 = bsh2 + off2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; count != 0; count--, addr1++, addr2++)
			*(volatile u_int8_t *)addr2 =
			    *(volatile u_int8_t *)addr1;
	} else {
		/* dest after src: copy backwards */
		for (addr1 += (count - 1), addr2 += (count - 1);
		    count != 0; count--, addr1--, addr2--)
			*(volatile u_int8_t *)addr2 =
			    *(volatile u_int8_t *)addr1;
	}
}

static __inline void
bus_space_copy_region_2(bus_space_tag_t tag __unused, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_space_handle_t addr1 = bsh1 + off1;
	bus_space_handle_t addr2 = bsh2 + off2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; count != 0; count--, addr1 += 2, addr2 += 2)
			*(volatile u_int16_t *)addr2 =
			    *(volatile u_int16_t *)addr1;
	} else {
		/* dest after src: copy backwards */
		for (addr1 += 2 * (count - 1), addr2 += 2 * (count - 1);
		    count != 0; count--, addr1 -= 2, addr2 -= 2)
			*(volatile u_int16_t *)addr2 =
			    *(volatile u_int16_t *)addr1;
	}
}

static __inline void
bus_space_copy_region_4(bus_space_tag_t tag __unused, bus_space_handle_t bsh1,
			bus_size_t off1, bus_space_handle_t bsh2,
			bus_size_t off2, size_t count)
{
	bus_space_handle_t addr1 = bsh1 + off1;
	bus_space_handle_t addr2 = bsh2 + off2;

	if (addr1 >= addr2) {
		/* src after dest: copy forward */
		for (; count != 0; count--, addr1 += 4, addr2 += 4)
			*(volatile u_int32_t *)addr2 =
			    *(volatile u_int32_t *)addr1;
	} else {
		/* dest after src: copy backwards */
		for (addr1 += 4 * (count - 1), addr2 += 4 * (count - 1);
		    count != 0; count--, addr1 -= 4, addr2 -= 4)
			*(volatile u_int32_t *)addr2 =
			    *(volatile u_int32_t *)addr1;
	}
}

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
 *			       bus_size_t offset, bus_size_t len, int flags);
 *
 * On ARM64, we use DSB (Data Synchronization Barrier) for memory barriers.
 * DSB SY ensures all memory accesses before the barrier are completed
 * before any memory accesses after the barrier.
 */
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

static __inline void
bus_space_barrier(bus_space_tag_t tag __unused, bus_space_handle_t bsh __unused,
		  bus_size_t offset __unused, bus_size_t len __unused, int flags)
{
	if (flags & (BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE))
		__asm __volatile("dsb sy" : : : "memory");
	else
		__asm __volatile("" : : : "memory");
}

/*
 * Stream accesses are the same as normal accesses on ARM64 in little-endian
 * mode. ARM64 can operate in either endianness but DragonFly assumes LE.
 */
#define	bus_space_read_stream_1(t, h, o)	bus_space_read_1((t), (h), (o))
#define	bus_space_read_stream_2(t, h, o)	bus_space_read_2((t), (h), (o))
#define	bus_space_read_stream_4(t, h, o)	bus_space_read_4((t), (h), (o))

#define	bus_space_read_multi_stream_1(t, h, o, a, c) \
	bus_space_read_multi_1((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_2(t, h, o, a, c) \
	bus_space_read_multi_2((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_4(t, h, o, a, c) \
	bus_space_read_multi_4((t), (h), (o), (a), (c))

#define	bus_space_write_stream_1(t, h, o, v) \
	bus_space_write_1((t), (h), (o), (v))
#define	bus_space_write_stream_2(t, h, o, v) \
	bus_space_write_2((t), (h), (o), (v))
#define	bus_space_write_stream_4(t, h, o, v) \
	bus_space_write_4((t), (h), (o), (v))

#define	bus_space_write_multi_stream_1(t, h, o, a, c) \
	bus_space_write_multi_1((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_2(t, h, o, a, c) \
	bus_space_write_multi_2((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_4(t, h, o, a, c) \
	bus_space_write_multi_4((t), (h), (o), (a), (c))

#define	bus_space_set_multi_stream_1(t, h, o, v, c) \
	bus_space_set_multi_1((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_2(t, h, o, v, c) \
	bus_space_set_multi_2((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_4(t, h, o, v, c) \
	bus_space_set_multi_4((t), (h), (o), (v), (c))

#define	bus_space_read_region_stream_1(t, h, o, a, c) \
	bus_space_read_region_1((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_2(t, h, o, a, c) \
	bus_space_read_region_2((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_4(t, h, o, a, c) \
	bus_space_read_region_4((t), (h), (o), (a), (c))

#define	bus_space_write_region_stream_1(t, h, o, a, c) \
	bus_space_write_region_1((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_2(t, h, o, a, c) \
	bus_space_write_region_2((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_4(t, h, o, a, c) \
	bus_space_write_region_4((t), (h), (o), (a), (c))

#define	bus_space_set_region_stream_1(t, h, o, v, c) \
	bus_space_set_region_1((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_2(t, h, o, v, c) \
	bus_space_set_region_2((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_4(t, h, o, v, c) \
	bus_space_set_region_4((t), (h), (o), (v), (c))

#define	bus_space_copy_region_stream_1(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_2(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_2((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_4(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_4((t), (h1), (o1), (h2), (o2), (c))

#endif /* _CPU_BUS_DMA_H_ */
