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

#ifndef _IICBUS_IF_H_
#define _IICBUS_IF_H_

/* DragonFly I2C bus interface (auto-generated) for LinuxKPI */

#include <sys/types.h>
#include <dev/iicbus/iicbus.h>

/* I2C bus interface methods */
#define IICBUS_TRANSFER(device, msgs, nmsgs) \
    iicbus_transfer(device, msgs, nmsgs)

#define IICBUS_RESET(device, how) \
    iicbus_reset(device, how)

#define IICBUS_REPEATED_START(device, addr, flags) \
    iicbus_repeated_start(device, addr, flags)

/* I2C bus callback functions - stubs */
static __inline int
iicbus_callback(device_t dev, int index, void *data)
{
    return 0;  /* Stub */
}

static __inline int
iicbus_set_clock(device_t dev, int speed)
{
    return 0;  /* Stub */
}

static __inline int
iicbus_get_clock(device_t dev, int *speed)
{
    *speed = 100000;  /* Default 100kHz */
    return 0;
}

/* I2C bus attachment - stubs */
static __inline int
iicbus_device_attach(device_t dev)
{
    return 0;  /* Stub */
}

static __inline int
iicbus_device_detach(device_t dev)
{
    return 0;  /* Stub */
}

/* I2C bus probe - stub */
static __inline int
iicbus_probe(device_t dev)
{
    return 0;  /* Stub */
}

#endif /* _IICBUS_IF_H_ */
