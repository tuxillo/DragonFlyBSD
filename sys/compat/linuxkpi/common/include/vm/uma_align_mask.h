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

#ifndef _VM_UMA_ALIGN_MASK_H_
#define _VM_UMA_ALIGN_MASK_H_

/* DragonFly UMA (Universal Memory Allocator) alignment mask for LinuxKPI */

/*
 * UMA alignment mask definitions.
 * These define the alignment constraints for UMA zone allocations.
 */

/* Basic alignment masks (power of 2 minus 1) */
#define UMA_ALIGN_MASK_1       0x00  /* 1-byte alignment (no alignment) */
#define UMA_ALIGN_MASK_2       0x01  /* 2-byte alignment */
#define UMA_ALIGN_MASK_4       0x03  /* 4-byte alignment */
#define UMA_ALIGN_MASK_8       0x07  /* 8-byte alignment */
#define UMA_ALIGN_MASK_16      0x0F  /* 16-byte alignment */
#define UMA_ALIGN_MASK_32      0x1F  /* 32-byte alignment */
#define UMA_ALIGN_MASK_64      0x3F  /* 64-byte alignment */
#define UMA_ALIGN_MASK_128     0x7F  /* 128-byte alignment */
#define UMA_ALIGN_MASK_256     0xFF  /* 256-byte alignment */
#define UMA_ALIGN_MASK_512     0x1FF /* 512-byte alignment */
#define UMA_ALIGN_MASK_1024    0x3FF /* 1024-byte alignment (1KB) */
#define UMA_ALIGN_MASK_2048    0x7FF /* 2048-byte alignment (2KB) */
#define UMA_ALIGN_MASK_4096    0xFFF /* 4096-byte alignment (4KB / page) */

/* Cache line alignment (typical 64 bytes) */
#define UMA_ALIGN_CACHE        UMA_ALIGN_MASK_64

/* Page alignment */
#define UMA_ALIGN_PAGE         UMA_ALIGN_MASK_4096

/*
 * UMA alignment flags.
 * These can be OR'd with alignment masks for special alignment behavior.
 */
#define UMA_ALIGN_NONE         0x00000000
#define UMA_ALIGN_POW2         0x80000000  /* Align to power of 2 */
#define UMA_ALIGN_OFLOW        0x40000000  /* Overflow detection */
#define UMA_ALIGN_FIRSTFIT     0x20000000  /* First fit allocation */

/*
 * Macro to compute alignment mask from alignment value.
 * alignment must be a power of 2.
 */
#define UMA_ALIGN_VALUE_TO_MASK(alignment)    ((alignment) - 1)

/*
 * Macro to compute alignment value from mask.
 */
#define UMA_ALIGN_MASK_TO_VALUE(mask)         (((mask) + 1) & ~(mask))

/*
 * Check if an address is aligned according to mask.
 */
#define UMA_ALIGN_CHECK(addr, mask)           (((addr) & (mask)) == 0)

/*
 * Align an address up to the specified mask.
 */
#define UMA_ALIGN_UP(addr, mask)              (((addr) + (mask)) & ~(mask))

/*
 * Align an address down to the specified mask.
 */
#define UMA_ALIGN_DOWN(addr, mask)            ((addr) & ~(mask))

#endif /* _VM_UMA_ALIGN_MASK_H_ */
