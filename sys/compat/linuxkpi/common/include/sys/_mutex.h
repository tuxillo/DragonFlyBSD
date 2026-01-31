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

#ifndef _SYS__MUTEX_H_
#define _SYS__MUTEX_H_

/* DragonFly _mutex compatibility for LinuxKPI (FreeBSD mutex internals) */

/* Just include the regular mutex.h - DragonFly doesn't have _mutex.h */
#include <sys/mutex.h>

/* FreeBSD uses these internal types that we need to define for compatibility */
struct mtx;

/* Mutex type constants (already defined in DragonFly's mutex.h, but ensure they're here) */
#ifndef MTX_DEF
#define MTX_DEF 0
#endif

#ifndef MTX_SPIN
#define MTX_SPIN 1
#endif

#ifndef MTX_RECURSE
#define MTX_RECURSE 2
#endif

#ifndef MTX_NOWITNESS
#define MTX_NOWITNESS 4
#endif

#ifndef MTX_DUPOK
#define MTX_DUPOK 8
#endif

#ifndef MTX_QUIET
#define MTX_QUIET 0
#endif

#endif /* _SYS__MUTEX_H_ */
