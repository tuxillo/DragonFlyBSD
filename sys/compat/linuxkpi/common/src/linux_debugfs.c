/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The DragonFly Project
 *
 * Stub implementations for debugfs functions.
 * These are no-op stubs until lindebugfs is ported to DragonFly.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <linux/debugfs.h>
#include <linux/fs.h>

MALLOC_DEFINE(M_DFSINT, "debugfsint", "Linux debugfs internal");

struct dentry *
debugfs_create_file(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops)
{
	return (NULL);
}

struct dentry *
debugfs_create_file_size(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops,
    loff_t file_size)
{
	return (NULL);
}

struct dentry *
debugfs_create_file_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops)
{
	return (NULL);
}

struct dentry *
debugfs_create_mode_unsafe(const char *name, umode_t mode,
    struct dentry *parent, void *data,
    const struct file_operations *fops,
    const struct file_operations *fops_ro,
    const struct file_operations *fops_wo)
{
	return (NULL);
}

struct dentry *
debugfs_create_dir(const char *name, struct dentry *parent)
{
	return (NULL);
}

struct dentry *
debugfs_create_symlink(const char *name, struct dentry *parent,
    const char *dest)
{
	return (NULL);
}

struct dentry *
debugfs_lookup(const char *name, struct dentry *parent)
{
	return (NULL);
}

void
debugfs_remove(struct dentry *dentry)
{
}

void
debugfs_remove_recursive(struct dentry *dentry)
{
}

void
debugfs_create_bool(const char *name, umode_t mode, struct dentry *parent,
    bool *value)
{
}

void
debugfs_create_u8(const char *name, umode_t mode, struct dentry *parent,
    uint8_t *value)
{
}

void
debugfs_create_u16(const char *name, umode_t mode, struct dentry *parent,
    uint16_t *value)
{
}

void
debugfs_create_u32(const char *name, umode_t mode, struct dentry *parent,
    uint32_t *value)
{
}

void
debugfs_create_u64(const char *name, umode_t mode, struct dentry *parent,
    uint64_t *value)
{
}

void
debugfs_create_x8(const char *name, umode_t mode, struct dentry *parent,
    uint8_t *value)
{
}

void
debugfs_create_x16(const char *name, umode_t mode, struct dentry *parent,
    uint16_t *value)
{
}

void
debugfs_create_x32(const char *name, umode_t mode, struct dentry *parent,
    uint32_t *value)
{
}

void
debugfs_create_x64(const char *name, umode_t mode, struct dentry *parent,
    uint64_t *value)
{
}

void
debugfs_create_ulong(const char *name, umode_t mode, struct dentry *parent,
    unsigned long *value)
{
}

void
debugfs_create_atomic_t(const char *name, umode_t mode, struct dentry *parent,
    atomic_t *value)
{
}

void
debugfs_create_str(const char *name, umode_t mode, struct dentry *parent,
    char **value)
{
}

ssize_t
debugfs_attr_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	return (-ENODEV);
}

ssize_t
debugfs_attr_write(struct file *file, const char __user *buf, size_t len,
    loff_t *ppos)
{
	return (-ENODEV);
}

ssize_t
debugfs_attr_write_signed(struct file *file, const char __user *buf, size_t len,
    loff_t *ppos)
{
	return (-ENODEV);
}

struct dentry *
debugfs_create_blob(const char *name, umode_t mode, struct dentry *parent,
    struct debugfs_blob_wrapper *blob)
{
	return (NULL);
}

void
debugfs_create_regset32(const char *name, umode_t mode, struct dentry *parent,
    struct debugfs_regset32 *regset)
{
}
