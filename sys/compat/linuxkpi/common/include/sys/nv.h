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

#ifndef _SYS_NV_H_
#define _SYS_NV_H_

/* DragonFly Name/Value pairs for PCI for LinuxKPI */

#include <sys/types.h>

/*
 * NV (Name/Value) pairs are used for passing configuration data
 * between kernel components, particularly for PCI passthrough
 * and SR-IOV configurations.
 */

/* NV types */
enum nv_type {
    NV_TYPE_NULL = 0,
    NV_TYPE_BOOL,
    NV_TYPE_NUMBER,
    NV_TYPE_STRING,
    NV_TYPE_NVLIST,
    NV_TYPE_BINARY,
    NV_TYPE_BOOL_ARRAY,
    NV_TYPE_NUMBER_ARRAY,
    NV_TYPE_STRING_ARRAY,
    NV_TYPE_NVLIST_ARRAY,
    NV_TYPE_MAX
};

/* NV flags */
#define NV_FLAG_NONE        0x00000000
#define NV_FLAG_PUBLIC      0x00000001  /* Public/exportable data */
#define NV_FLAG_PRIVATE     0x00000002  /* Private/internal data */
#define NV_FLAG_PERSISTENT  0x00000004  /* Persistent across reboots */

/* NV list handle (opaque) */
struct nvlist;
typedef struct nvlist *nvlist_t;

/* NV pair handle (opaque) */
struct nvpair;
typedef struct nvpair *nvpair_t;

/*
 * NV list creation and destruction
 */

/* Create a new NV list */
static __inline nvlist_t
nvlist_create(int flags)
{
    /* Stub - return NULL */
    (void)flags;
    return NULL;
}

/* Destroy an NV list */
static __inline void
nvlist_destroy(nvlist_t nvl)
{
    (void)nvl;
}

/* Clear all entries in an NV list */
static __inline void
nvlist_clear(nvlist_t nvl)
{
    (void)nvl;
}

/* Get the number of entries in an NV list */
static __inline int
nvlist_count(nvlist_t nvl)
{
    (void)nvl;
    return 0;
}

/*
 * NV list addition functions
 */

/* Add a boolean value */
static __inline int
nvlist_add_bool(nvlist_t nvl, const char *name, int value)
{
    (void)nvl; (void)name; (void)value;
    return 0;
}

/* Add a number value */
static __inline int
nvlist_add_number(nvlist_t nvl, const char *name, uint64_t value)
{
    (void)nvl; (void)name; (void)value;
    return 0;
}

/* Add a string value */
static __inline int
nvlist_add_string(nvlist_t nvl, const char *name, const char *value)
{
    (void)nvl; (void)name; (void)value;
    return 0;
}

/* Add binary data */
static __inline int
nvlist_add_binary(nvlist_t nvl, const char *name, const void *value, size_t size)
{
    (void)nvl; (void)name; (void)value; (void)size;
    return 0;
}

/* Add a nested NV list */
static __inline int
nvlist_add_nvlist(nvlist_t nvl, const char *name, nvlist_t value)
{
    (void)nvl; (void)name; (void)value;
    return 0;
}

/*
 * NV list retrieval functions
 */

/* Check if a name exists */
static __inline int
nvlist_exists(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
    return 0;
}

/* Get boolean value */
static __inline int
nvlist_get_bool(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
    return 0;
}

/* Get number value */
static __inline uint64_t
nvlist_get_number(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
    return 0;
}

/* Get string value */
static __inline const char *
nvlist_get_string(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
    return NULL;
}

/* Get binary data */
static __inline const void *
nvlist_get_binary(nvlist_t nvl, const char *name, size_t *size)
{
    (void)nvl; (void)name;
    if (size)
        *size = 0;
    return NULL;
}

/* Get nested NV list */
static __inline nvlist_t
nvlist_get_nvlist(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
    return NULL;
}

/*
 * NV list modification functions
 */

/* Remove an entry */
static __inline void
nvlist_remove(nvlist_t nvl, const char *name)
{
    (void)nvl; (void)name;
}

/*
 * NV list iteration
 */

/* Get the first pair */
static __inline nvpair_t
nvlist_first_nvpair(nvlist_t nvl)
{
    (void)nvl;
    return NULL;
}

/* Get the next pair */
static __inline nvpair_t
nvpair_next(nvpair_t nvp)
{
    (void)nvp;
    return NULL;
}

/* Get pair name */
static __inline const char *
nvpair_name(nvpair_t nvp)
{
    (void)nvp;
    return NULL;
}

/* Get pair type */
static __inline enum nv_type
nvpair_type(nvpair_t nvp)
{
    (void)nvp;
    return NV_TYPE_NULL;
}

/*
 * Packing and unpacking
 */

typedef unsigned char *nvlist_packer_t;

/* Pack an NV list into binary format */
static __inline int
nvlist_pack(nvlist_t nvl, nvlist_packer_t *ptr, size_t *size)
{
    (void)nvl; (void)ptr; (void)size;
    return 0;
}

/* Unpack binary data into an NV list */
static __inline nvlist_t
nvlist_unpack(nvlist_packer_t ptr, size_t size)
{
    (void)ptr; (void)size;
    return NULL;
}

/* Free packed data */
static __inline void
nvlist_packer_free(nvlist_packer_t ptr)
{
    (void)ptr;
}

/*
 * Error handling
 */

/* NV error codes */
#define NV_ERR_NONE     0
#define NV_ERR_NOENT    1   /* Entry not found */
#define NV_ERR_EXISTS   2   /* Entry already exists */
#define NV_ERR_NOMEM    3   /* Out of memory */
#define NV_ERR_INVAL    4   /* Invalid argument */
#define NV_ERR_TYPE     5   /* Type mismatch */
#define NV_ERR_SIZE     6   /* Size too large */

/* Get last error */
static __inline int
nvlist_error(nvlist_t nvl)
{
    (void)nvl;
    return NV_ERR_NONE;
}

/* Clear error */
static __inline void
nvlist_clear_error(nvlist_t nvl)
{
    (void)nvl;
}

/*
 * Convenience macros for PCI configuration
 */
#define NV_PCI_BDF_BUS(bdf)     (((bdf) >> 8) & 0xFF)
#define NV_PCI_BDF_DEV(bdf)     (((bdf) >> 3) & 0x1F)
#define NV_PCI_BDF_FUNC(bdf)    ((bdf) & 0x07)
#define NV_PCI_MAKE_BDF(bus, dev, func) (((bus) << 8) | ((dev) << 3) | (func))

#endif /* _SYS_NV_H_ */
