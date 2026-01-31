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

#ifndef _FS_PSEUDOFS_PSEUDOFS_H_
#define _FS_PSEUDOFS_PSEUDOFS_H_

/* DragonFly Pseudo Filesystem definitions for LinuxKPI */

#include <sys/types.h>
#include <sys/vnode.h>

/*
 * Pseudofs is a generic filesystem layer for creating virtual filesystems
 * like procfs, linprocfs, and similar.
 */

/* Pseudofs node types */
enum pfs_type {
    PFS_ROOT = 0,      /* Root node */
    PFS_DIR,           /* Directory node */
    PFS_FILE,          /* Regular file node */
    PFS_LINK,          /* Symbolic link node */
    PFS_PROTODIR,      /* Prototype directory (for per-process dirs) */
    PFS_PROTONODE,     /* Prototype node */
    PFS_MAXTYPE
};

/* Pseudofs node flags */
#define PFS_RD      0x00000001  /* Node is readable */
#define PFS_WR      0x00000002  /* Node is writable */
#define PFS_MWR     0x00000004  /* Node is writable by owner only */
#define PFS_IMMUTABLE   0x00000008  /* Node cannot be removed */
#define PFS_HIDDEN  0x00000010  /* Node is hidden */
#define PFS_DYNAMIC 0x00000020  /* Node is dynamically created */
#define PFS_INPROG  0x00000040  /* Node is being created */

/* Pseudofs node structure */
struct pfs_node {
    const char          *pn_name;       /* Node name */
    enum pfs_type       pn_type;        /* Node type */
    uint32_t            pn_flags;       /* Node flags */
    uint32_t            pn_fileno;      /* File number */
    mode_t              pn_mode;        /* File mode */
    uid_t               pn_uid;         /* Owner UID */
    gid_t               pn_gid;         /* Owner GID */
    struct pfs_node     *pn_parent;     /* Parent node */
    struct pfs_node     *pn_next;       /* Next sibling */
    struct pfs_node     *pn_children;   /* First child */
    void                *pn_data;       /* Private data */
    /* Function pointers for operations */
    int                 (*pn_fill)(struct pfs_node *node, void *data);
    int                 (*pn_read)(struct pfs_node *node, struct uio *uio);
    int                 (*pn_write)(struct pfs_node *node, struct uio *uio);
    int                 (*pn_getattr)(struct pfs_node *node, struct vattr *vap);
};

/* Pseudofs mount structure */
struct pfs_mount {
    struct mount        *pm_mount;      /* VFS mount structure */
    struct pfs_node     *pm_root;       /* Root node */
    uint32_t            pm_flags;       /* Mount flags */
    uint32_t            pm_nextfileno;  /* Next file number */
    void                *pm_data;       /* Private data */
};

/*
 * Pseudofs vnode operations
 */
struct pfs_vnop_args {
    struct vnode        *pv_vp;         /* Vnode */
    struct pfs_node     *pv_node;       /* Pseudofs node */
    struct pfs_mount    *pv_mount;      /* Mount point */
    void                *pv_data;       /* Private data */
};

/*
 * Pseudofs creation functions (stubs)
 */

/* Create a pseudofs node */
static __inline struct pfs_node *
pfs_create_node(struct pfs_node *parent, const char *name, enum pfs_type type,
                uint32_t flags, mode_t mode)
{
    /* Stub - return NULL */
    return NULL;
}

/* Create a directory node */
static __inline struct pfs_node *
pfs_create_dir(struct pfs_node *parent, const char *name, uint32_t flags)
{
    return pfs_create_node(parent, name, PFS_DIR, flags, 0555);
}

/* Create a file node */
static __inline struct pfs_node *
pfs_create_file(struct pfs_node *parent, const char *name, 
                int (*read)(struct pfs_node *, struct uio *),
                int (*write)(struct pfs_node *, struct uio *),
                void *data, uint32_t flags)
{
    return pfs_create_node(parent, name, PFS_FILE, flags, 0444);
}

/* Create a symlink node */
static __inline struct pfs_node *
pfs_create_link(struct pfs_node *parent, const char *name, const char *target,
                uint32_t flags)
{
    return pfs_create_node(parent, name, PFS_LINK, flags, 0777);
}

/* Remove a pseudofs node */
static __inline void
pfs_destroy_node(struct pfs_node *node)
{
    /* Stub */
}

/* Find a child node by name */
static __inline struct pfs_node *
pfs_find_node(struct pfs_node *parent, const char *name)
{
    /* Stub - return NULL */
    return NULL;
}

/* Get the root node */
static __inline struct pfs_node *
pfs_get_root(struct pfs_mount *mount)
{
    if (mount)
        return mount->pm_root;
    return NULL;
}

/*
 * Pseudofs mount operations (stubs)
 */

/* Initialize a pseudofs mount */
static __inline int
pfs_mount_init(struct pfs_mount *mount, void *data)
{
    /* Stub - return success */
    return 0;
}

/* Cleanup a pseudofs mount */
static __inline void
pfs_mount_cleanup(struct pfs_mount *mount)
{
    /* Stub */
}

/*
 * Helper macros
 */
#define PFS_NODE_FOREACH(child, parent) \
    for ((child) = (parent)->pn_children; (child) != NULL; (child) = (child)->pn_next)

#define PFS_NODE_IS_DIR(node) \
    ((node)->pn_type == PFS_DIR || (node)->pn_type == PFS_PROTODIR)

#define PFS_NODE_IS_FILE(node) \
    ((node)->pn_type == PFS_FILE || (node)->pn_type == PFS_PROTONODE)

#endif /* _FS_PSEUDOFS_PSEUDOFS_H_ */
