/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_BUS_H_
#define _MACHINE_BUS_H_

/* DragonFly bus.h compatibility for LinuxKPI */

#include <sys/types.h>

/* 
 * DragonFly has bus.h in sys/bus.h, but LinuxKPI includes it as machine/bus.h
 * This is a compatibility wrapper.
 */

/* Include the DragonFly bus header */
#include <sys/bus.h>

/* Additional bus types that might be referenced */
#ifndef BUS_SPACE_MEMIO
#define BUS_SPACE_MEMIO 0
#endif

#ifndef BUS_SPACE_PIO
#define BUS_SPACE_PIO 1
#endif

/* Bus tag and handle types - already defined in DragonFly's bus.h */
/* bus_space_tag_t and bus_space_handle_t */

/* Bus barrier operations - ensure they're defined */
#ifndef bus_space_barrier
#define bus_space_barrier(t, h, o, l, f) \
    bus_space_sync(t, h, o, l, (f) & BUS_SPACE_BARRIER_WRITE)
#endif

/* Bus read/write stream operations - aliases for regular operations */
#ifndef bus_space_read_stream_1
#define bus_space_read_stream_1 bus_space_read_1
#endif
#ifndef bus_space_read_stream_2
#define bus_space_read_stream_2 bus_space_read_2
#endif
#ifndef bus_space_read_stream_4
#define bus_space_read_stream_4 bus_space_read_4
#endif
#ifndef bus_space_read_stream_8
#define bus_space_read_stream_8 bus_space_read_8
#endif

#ifndef bus_space_write_stream_1
#define bus_space_write_stream_1 bus_space_write_1
#endif
#ifndef bus_space_write_stream_2
#define bus_space_write_stream_2 bus_space_write_2
#endif
#ifndef bus_space_write_stream_4
#define bus_space_write_stream_4 bus_space_write_4
#endif
#ifndef bus_space_write_stream_8
#define bus_space_write_stream_8 bus_space_write_8
#endif

/* Bus multi-stream operations */
#ifndef bus_space_read_multi_stream_1
#define bus_space_read_multi_stream_1 bus_space_read_multi_1
#endif
#ifndef bus_space_read_multi_stream_2
#define bus_space_read_multi_stream_2 bus_space_read_multi_2
#endif
#ifndef bus_space_read_multi_stream_4
#define bus_space_read_multi_stream_4 bus_space_read_multi_4
#endif
#ifndef bus_space_read_multi_stream_8
#define bus_space_read_multi_stream_8 bus_space_read_multi_8
#endif

#ifndef bus_space_write_multi_stream_1
#define bus_space_write_multi_stream_1 bus_space_write_multi_1
#endif
#ifndef bus_space_write_multi_stream_2
#define bus_space_write_multi_stream_2 bus_space_write_multi_2
#endif
#ifndef bus_space_write_multi_stream_4
#define bus_space_write_multi_stream_4 bus_space_write_multi_4
#endif
#ifndef bus_space_write_multi_stream_8
#define bus_space_write_multi_stream_8 bus_space_write_multi_8
#endif

/* Bus region stream operations */
#ifndef bus_space_read_region_stream_1
#define bus_space_read_region_stream_1 bus_space_read_region_1
#endif
#ifndef bus_space_read_region_stream_2
#define bus_space_read_region_stream_2 bus_space_read_region_2
#endif
#ifndef bus_space_read_region_stream_4
#define bus_space_read_region_stream_4 bus_space_read_region_4
#endif
#ifndef bus_space_read_region_stream_8
#define bus_space_read_region_stream_8 bus_space_read_region_8
#endif

#ifndef bus_space_write_region_stream_1
#define bus_space_write_region_stream_1 bus_space_write_region_1
#endif
#ifndef bus_space_write_region_stream_2
#define bus_space_write_region_stream_2 bus_space_write_region_2
#endif
#ifndef bus_space_write_region_stream_4
#define bus_space_write_region_stream_4 bus_space_write_region_4
#endif
#ifndef bus_space_write_region_stream_8
#define bus_space_write_region_stream_8 bus_space_write_region_8
#endif

/* Bus set region stream operations */
#ifndef bus_space_set_region_stream_1
#define bus_space_set_region_stream_1 bus_space_set_region_1
#endif
#ifndef bus_space_set_region_stream_2
#define bus_space_set_region_stream_2 bus_space_set_region_2
#endif
#ifndef bus_space_set_region_stream_4
#define bus_space_set_region_stream_4 bus_space_set_region_4
#endif
#ifndef bus_space_set_region_stream_8
#define bus_space_set_region_stream_8 bus_space_set_region_8
#endif

/* Bus copy region stream operations */
#ifndef bus_space_copy_region_stream_1
#define bus_space_copy_region_stream_1 bus_space_copy_region_1
#endif
#ifndef bus_space_copy_region_stream_2
#define bus_space_copy_region_stream_2 bus_space_copy_region_2
#endif
#ifndef bus_space_copy_region_stream_4
#define bus_space_copy_region_stream_4 bus_space_copy_region_4
#endif
#ifndef bus_space_copy_region_stream_8
#define bus_space_copy_region_stream_8 bus_space_copy_region_8
#endif

#endif /* _MACHINE_BUS_H_ */
