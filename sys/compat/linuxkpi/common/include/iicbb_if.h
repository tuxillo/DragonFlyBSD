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

#ifndef _IICBB_IF_H_
#define _IICBB_IF_H_

/* DragonFly I2C bit-bang interface (auto-generated) for LinuxKPI */

#include <sys/types.h>
#include <dev/iicbus/iicbus.h>

/* I2C bit-bang bus interface */
#define IICBB_SET_SCL(device, val)
#define IICBB_SET_SDA(device, val)
#define IICBB_GET_SCL(device) 0
#define IICBB_GET_SDA(device) 0
#define IICBB_RESET(device)
#define IICBB_TRANSFER(device, msgs, nmsgs) 0

/* I2C bit-bang callbacks - stubs */
static __inline int
iicbb_attach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
iicbb_detach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline void
iicbb_set_scl(device_t dev, int val)
{
    /* Stub */
}

static __inline void
iicbb_set_sda(device_t dev, int val)
{
    /* Stub */
}

static __inline int
iicbb_get_scl(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
iicbb_get_sda(device_t dev)
{
    return 0;  /* Stub */
}

static __inline void
iicbb_reset(device_t dev)
{
    /* Stub */
}

#endif /* _IICBB_IF_H_ */
