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

#ifndef _NETINET_TCP_LRO_H_
#define _NETINET_TCP_LRO_H_

/* DragonFly TCP LRO (Large Receive Offload) compatibility for LinuxKPI */

#include <sys/types.h>
#include <sys/mbuf.h>

/* LRO entry structure - simplified */
struct lro_entry {
    struct mbuf *le_mbuf;
    uint32_t le_seq;
    uint32_t le_ack_seq;
    uint16_t le_window;
    uint16_t le_mss;
    uint8_t le_flags;
};

/* LRO control structure */
struct lro_ctrl {
    struct lro_entry *lro_entries;
    int lro_count;
    int lro_max_count;
    uint32_t lro_queued;
    uint32_t lro_flushed;
    uint32_t lro_bad_csum;
};

/* LRO flags */
#define LRO_ENABLED 0x01
#define LRO_ALL_ENTRIES 0x02

/* LRO operations - stub implementations */
static __inline void
lro_init(struct lro_ctrl *ctrl)
{
    ctrl->lro_entries = NULL;
    ctrl->lro_count = 0;
    ctrl->lro_max_count = 0;
    ctrl->lro_queued = 0;
    ctrl->lro_flushed = 0;
    ctrl->lro_bad_csum = 0;
}

static __inline void
lro_free(struct lro_ctrl *ctrl)
{
    /* Stub - would free LRO entries */
}

static __inline int
tcp_lro_rx(struct lro_ctrl *ctrl, struct mbuf *m, uint32_t csum)
{
    /* Stub - would perform LRO aggregation */
    return 0;
}

static __inline void
tcp_lro_flush_all(struct lro_ctrl *ctrl)
{
    /* Stub - would flush all LRO entries */
}

static __inline void
tcp_lro_flush(struct lro_ctrl *ctrl, struct lro_entry *entry)
{
    /* Stub - would flush a single LRO entry */
}

/* LRO queue limit */
#define LRO_MAX_ENTRIES 64

#endif /* _NETINET_TCP_LRO_H_ */
