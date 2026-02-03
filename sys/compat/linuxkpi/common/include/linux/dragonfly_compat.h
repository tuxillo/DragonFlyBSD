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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_DRAGONFLY_COMPAT_H_
#define _LINUX_DRAGONFLY_COMPAT_H_

#ifdef __DragonFly__

/*
 * DragonFly compatibility layer for LinuxKPI
 * 
 * This header provides DragonFly equivalents for FreeBSD kernel APIs
 * that LinuxKPI depends on but which differ or don't exist in DragonFly.
 */

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/thread2.h>
#include <sys/bus.h>
#include <bus/pci/pcivar.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/bitstring.h>
#include <sys/rman.h>
#include <sys/mutex.h>
#include <sys/mutex2.h>
#include <sys/lock.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <machine/thread.h>
#include <machine/pmap.h>
#include <machine/cpufunc.h>
#include <machine/atomic.h>
#include <machine/specialreg.h>

#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#ifndef _LINUXKPI_RMAN_RES_T
typedef u_long rman_res_t;
#define _LINUXKPI_RMAN_RES_T
#endif
#include <sys/vmmeter.h>

/*
 * FreeBSD mutex flags - DragonFly uses different flags.
 * These are used by LinuxKPI but we need to ignore/adapt them.
 */
#ifndef MTX_DEF
#define MTX_DEF         0x0000  /* Default (sleep) mutex */
#endif
#ifndef MTX_SPIN
#define MTX_SPIN        0x0001  /* Spin mutex */
#endif
#ifndef MTX_RECURSE
#define MTX_RECURSE     0x0004  /* Recursive mutex */
#endif
#ifndef MTX_NOWITNESS
#define MTX_NOWITNESS   0x0008  /* Don't do WITNESS checking */
#endif
#ifndef MTX_DUPOK
#define MTX_DUPOK       0x0020  /* Don't check for duplicate acquires */
#endif

/*
 * FreeBSD mtx_init compatibility.
 * FreeBSD: void mtx_init(mtx, name, type, opts)
 * DragonFly: void mtx_init(mtx, ident) - defined as inline in mutex2.h
 *
 * NOTE: We do NOT globally override mtx_init here because:
 * 1. LinuxKPI spinlock_t on DragonFly is struct spinlock, not struct mtx
 * 2. Code using spinlock_t should use spin_lock_init() not mtx_init()
 * 3. Code needing real mutexes should use DragonFly's native mtx API
 *
 * For code that explicitly needs FreeBSD-style mtx_init on a real mutex,
 * use lkpi_mtx_init() wrapper function instead.
 */
static __inline void
lkpi_mtx_init(struct mtx *mtx, const char *name, const char *type __unused,
    int opts __unused)
{
	mtx->mtx_lock = 0;
	mtx->mtx_flags = 0;
	mtx->mtx_owner = NULL;
	mtx->mtx_exlink = NULL;
	mtx->mtx_shlink = NULL;
	mtx->mtx_ident = name;
}

/*
 * Map FreeBSD-style mtx_init() to lkpi_mtx_init().
 */
#ifdef mtx_init
#undef mtx_init
#endif
#define mtx_init(mtx, name, type, opts)	\
	lkpi_mtx_init((mtx), (name), (type), (opts))

/*
 * FreeBSD mtx_destroy compatibility.
 * DragonFly mutexes don't need explicit destruction - they're just memory.
 * Provide a no-op.
 */
#ifndef mtx_destroy
#define mtx_destroy(mtx) do { } while(0)
#endif

/*
 * mtx_initialized - FreeBSD function to check if mutex was initialized.
 * DragonFly doesn't have this. Check if ident pointer is set.
 */
#ifndef mtx_initialized
#define mtx_initialized(mtx) ((mtx)->mtx_ident != NULL)
#endif

/*
 * MPASS - FreeBSD assertion helper.
 * Map to DragonFly's KASSERT.
 */
#ifndef MPASS
#define MPASS(exp) KASSERT((exp), ("MPASS(%s)", #exp))
#endif

/*
 * WOULD_OVERFLOW - FreeBSD overflow check helper.
 * Used for nmemb * size style checks.
 */
#ifndef WOULD_OVERFLOW
#define WOULD_OVERFLOW(a, b) ((a) != 0 && (b) > SIZE_MAX / (a))
#endif

/*
 * VM_OBJECT_ASSERT_WLOCKED - FreeBSD VM object lock assert.
 * DragonFly does not provide a direct equivalent.
 */
#ifndef VM_OBJECT_ASSERT_WLOCKED
#define VM_OBJECT_ASSERT_WLOCKED(object) do { } while (0)
#endif

/*
 * Kernel printf/format helpers.
 */
#ifndef printf
#define printf		kprintf
#endif
#ifndef vprintf
#define vprintf		kvprintf
#endif
#ifndef snprintf
#define snprintf	ksnprintf
#endif
#ifndef vsnprintf
#define vsnprintf	kvsnprintf
#endif

/*
 * Kernel malloc helpers.
 */
#ifndef malloc
#define malloc(size, type, flags)	_kmalloc((size), (type), (flags))
#endif
#ifndef free
#define free(addr, type)		_kfree((addr), (type))
#endif
#ifndef realloc
#define realloc(addr, size, type, flags)	krealloc((addr), (size), (type), (flags))
#endif
#ifndef malloc_usable_size
#define malloc_usable_size(ptr)	kmalloc_usable_size((ptr))
#endif

/*
 * realloc() compatibility using kmalloc/kfree.
 */
static __inline void *
lkpi_realloc(void *ptr, size_t size, struct malloc_type *type, int flags)
{
	void *newptr;
	size_t oldsize;

	if (ptr == NULL)
		return (_kmalloc(size, type, flags));
	if (size == 0) {
		_kfree(ptr, type);
		return (NULL);
	}
	newptr = _kmalloc(size, type, flags);
	if (newptr == NULL)
		return (NULL);
	oldsize = kmalloc_usable_size(ptr);
	if (oldsize > size)
		oldsize = size;
	memcpy(newptr, ptr, oldsize);
	_kfree(ptr, type);
	return (newptr);
}
#undef realloc
#define realloc(ptr, size, type, flags)	\
	lkpi_realloc((ptr), (size), (type), (flags))

/*
 * kern_getenv - map to DragonFly's kgetenv.
 */
#ifndef kern_getenv
#define kern_getenv	kgetenv
#endif

/*
 * sched_relinquish - yield CPU.
 */
static __inline void
sched_relinquish(struct thread *td __unused)
{
	lwkt_yield();
}

/*
 * strchrnul - return pointer to terminator if not found.
 */
static __inline char *
strchrnul(const char *s, int c)
{
	const char *p = s;

	while (*p && *p != (char)c)
		p++;
	return (__DECONST(char *, p));
}

/*
 * bitcountl - number of set bits in unsigned long.
 */
#ifndef bitcountl
#define bitcountl(x)	__builtin_popcountl((unsigned long)(x))
#endif

#ifndef PROC_LOCK
#define PROC_LOCK(p) lwkt_gettoken(&(p)->p_token)
#define PROC_UNLOCK(p) lwkt_reltoken(&(p)->p_token)
#define PROC_LOCK_ASSERT(p, what) do { \
	if ((what) == MA_OWNED) \
		ASSERT_LWKT_TOKEN_HELD(&(p)->p_token); \
} while (0)
#endif

/*
 * thread lookup and signal helpers.
 */
static __inline struct thread *
tdfind(pid_t tid, int flags __unused)
{
	if (curthread != NULL && curthread->td_tid == tid) {
		PROC_LOCK(curthread->td_proc);
		return (curthread);
	}
	return (NULL);
}

static __inline void
tdsignal(struct thread *td, int sig)
{
	if (td != NULL)
		ksignal(td->td_proc, sig);
}

static __inline void
thread_reap_barrier(void)
{
}

/*
 * PCI helpers missing on DragonFly.
 */
static __inline uintptr_t
pci_get_rid(device_t dev)
{
	return ((uintptr_t)device_get_unit(dev));
}

static __inline int
pci_find_cap(device_t dev __unused, int cap __unused, int *capreg __unused)
{
	return (ENXIO);
}

static __inline device_t
pci_find_class_from(uint8_t class __unused, uint8_t subclass __unused,
    device_t dev __unused)
{
	return (NULL);
}

static __inline device_t
pci_find_base_class_from(uint8_t class __unused, device_t dev __unused)
{
	return (NULL);
}

static __inline device_t
pci_find_pcie_root_port(device_t dev __unused)
{
	return (NULL);
}

static __inline struct resource *
pci_reserve_map(device_t parent __unused, device_t dev, int type, int rid,
    rman_res_t start __unused, rman_res_t end __unused, u_long count __unused,
    u_int align __unused, u_int flags __unused)
{
	return (bus_alloc_resource_any(dev, type, &rid, RF_SHAREABLE));
}

/*
 * Scheduler state helpers.
 * DragonFly does not expose FreeBSD's scheduler stopped flag.
 */
#ifndef SCHEDULER_STOPPED
#define SCHEDULER_STOPPED() (0)
#endif

/*
 * CPU_ABSENT - FreeBSD CPU presence check.
 * DragonFly does not provide an equivalent; assume present.
 */
#ifndef CPU_ABSENT
#define CPU_ABSENT(cpu) (0)
#endif

/*
 * THREAD_SLEEPING_OK/THREAD_NO_SLEEPING - FreeBSD thread sleep guards.
 * DragonFly does not expose these; treat as no-ops.
 */
#ifndef THREAD_SLEEPING_OK
#define THREAD_SLEEPING_OK() do { } while (0)
#endif
#ifndef THREAD_NO_SLEEPING
#define THREAD_NO_SLEEPING() do { } while (0)
#endif

/*
 * critical_enter/critical_exit - map to DragonFly's crit_enter/crit_exit.
 */
#ifndef critical_enter
#define critical_enter()	crit_enter()
#endif
#ifndef critical_exit
#define critical_exit()		crit_exit()
#endif

/*
 * qsort_r - map to DragonFly's kqsort_r.
 */
#ifndef qsort_r
#define qsort_r(base, nmemb, size, thunk, cmp)	\
	kqsort_r((base), (nmemb), (size), (thunk), (cmp))
#endif

/*
 * pmap_remove_all - use pmap_page_protect as a replacement.
 */
#ifndef pmap_remove_all
#define pmap_remove_all(m)	pmap_page_protect((m), VM_PROT_NONE)
#endif

/*
 * pmap_extract_and_hold - DragonFly replacement.
 */
static __inline vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot __unused)
{
	vm_paddr_t pa;
	vm_page_t m;
	void *handle = NULL;

	pa = pmap_extract(pmap, va, &handle);
	if (pa == 0) {
		if (handle != NULL)
			pmap_extract_done(handle);
		return (NULL);
	}
	m = PHYS_TO_VM_PAGE(pa);
	if (m != NULL)
		vm_page_hold(m);
	if (handle != NULL)
		pmap_extract_done(handle);
	return (m);
}

/*
 * vm_map_range_valid - DragonFly does not expose this helper.
 */
static __inline int
vm_map_range_valid(vm_map_t map __unused, vm_offset_t start __unused,
    vm_offset_t end __unused)
{
	return (1);
}

/*
 * vm_fault_quick_hold_pages - map to vm_fault_page_quick loop.
 */
static __inline int
vm_fault_quick_hold_pages(vm_map_t map __unused, vm_offset_t start,
    size_t len __unused, vm_prot_t prot, vm_page_t *pages, int nr_pages)
{
	vm_offset_t va;
	int i;
	int error;

	for (i = 0, va = start; i < nr_pages; i++, va += PAGE_SIZE) {
		pages[i] = vm_fault_page_quick(va, prot, &error, NULL);
		if (pages[i] == NULL)
			return (-1);
	}
	return (nr_pages);
}

/*
 * vm_page allocation helpers.
 */
static __inline vm_page_t
vm_page_alloc_noobj(int req)
{
	return (vm_page_alloc(NULL, 0, req));
}

static __inline vm_page_t
vm_page_alloc_noobj_contig(int req, unsigned long npages, vm_paddr_t low,
    vm_paddr_t high, unsigned long alignment, unsigned long boundary,
    vm_memattr_t memattr)
{
	(void)req;
	return (vm_page_alloc_contig(low, high, alignment, boundary,
	    npages * PAGE_SIZE, memattr));
}

static __inline int
vm_page_reclaim_contig(int req __unused, unsigned long npages __unused,
    vm_paddr_t low __unused, vm_paddr_t high __unused,
    unsigned long alignment __unused, unsigned long boundary __unused)
{
	return (ENOMEM);
}

static __inline int
vm_page_unwire_noq(vm_page_t m)
{
	vm_page_unwire(m, 0);
	return (m->wire_count == 0);
}

#ifndef vm_page_flag_set
#define vm_page_flag_set(m, flagset)	atomic_set_int(&(m)->flags, (flagset))
#endif

#ifndef vm_page_free
#define vm_page_free(m)			vm_page_free_toq((m))
#endif

#ifndef vm_page_valid
static __inline void
vm_page_valid(vm_page_t m)
{
	m->valid = VM_PAGE_BITS_ALL;
}
#endif

/*
 * vm_free_count - return global free page count.
 */
static __inline long
vm_free_count(void)
{
	return (vmstats.v_free_count);
}

/*
 * kva_alloc/kva_free - map to kmem_alloc/kmem_free on kernel_map.
 */
static __inline vm_offset_t
kva_alloc(vm_size_t size)
{
	return (kmem_alloc(kernel_map, size, VM_SUBSYS_DRM_VMAP));
}

static __inline void
kva_free(vm_offset_t addr, vm_size_t size)
{
	kmem_free(kernel_map, addr, size);
}

/*
 * malloc_domainset/contigmalloc_domainset - ignore domainset.
 */
#ifndef malloc_domainset
#define malloc_domainset(size, type, domain, flags, ...)	\
	malloc((size), (type), (flags))
#endif
#ifndef contigmalloc_domainset
#define contigmalloc_domainset(size, type, domain, flags, low, high, align, boundary, ...)	\
	contigmalloc((size), (type), (flags), (low), (high), (align), (boundary))
#endif

/*
 * atomic_thread_fence_* - map to DragonFly fence primitives.
 */
#ifndef atomic_thread_fence_seq_cst
#define atomic_thread_fence_seq_cst()	cpu_mfence()
#endif
#ifndef atomic_thread_fence_rel
#define atomic_thread_fence_rel()	cpu_sfence()
#endif
#ifndef atomic_thread_fence_acq
#define atomic_thread_fence_acq()	cpu_lfence()
#endif

/*
 * VOP_STAT/VOP_UNLOCK compatibility.
 */
#ifndef VOP_STAT
#define VOP_STAT(vp, sb, cred, fcred) vn_stat((vp), (sb), (cred))
#endif
#ifndef VOP_UNLOCK
#define VOP_UNLOCK(vp) vn_unlock((vp))
#endif

/*
 * _fdrop - map to DragonFly's fdrop.
 */
#ifndef _fdrop
#define _fdrop(fp, td) fdrop((fp))
#endif

/*
 * bus_bind_intr - no-op on DragonFly.
 */
static __inline int
bus_bind_intr(device_t dev __unused, struct resource *irq __unused,
    int cpu __unused)
{
	return (0);
}

/*
 * makedev - ensure macro is available.
 */
#ifndef makedev
#define makedev(x, y)	((dev_t)(((x) << 8) | (y)))
#endif

/*
 * refcount_acquire_if_not_zero - FreeBSD helper.
 */
static __inline int
refcount_acquire_if_not_zero(volatile u_int *countp)
{
	u_int old;

	for (;;) {
		old = *countp;
		if (old == 0)
			return (0);
		if (atomic_cmpset_int(countp, old, old + 1))
			return (1);
	}
}

/*
 * callout_init compatibility.
 * FreeBSD: callout_init(c, flags) - takes 2 arguments
 * DragonFly: callout_init(c) - takes 1 argument (it's a macro in sys/callout.h)
 *
 * We need to override callout_init to accept 2 arguments and ignore the flags.
 * Use callout_init_mp() which is the multi-processor safe version.
 *
 * NOTE: This must come AFTER sys/callout.h is included to override the macro.
 */
#undef callout_init
#define callout_init(c, flags) callout_init_mp(c)

/*
 * callout_init_mtx - FreeBSD callout init with mutex.
 * DragonFly has callout_init_lk() with a struct lock, but not mtx.
 * For now, just use basic callout_init_mp() and ignore the mutex.
 * This is not fully correct but allows compilation.
 * XXX: May need proper locking integration later.
 */
#ifndef callout_init_mtx
#define callout_init_mtx(c, mtx, flags) callout_init_mp(c)
#endif

/*
 * taskqueue_poll_is_busy and taskqueue_drain_all are now implemented in
 * sys/kern/subr_taskqueue.c and declared in sys/sys/taskqueue.h.
 * No stubs needed here.
 */

/*
 * SMP compatibility - include our sys/smp.h for CPU_FOREACH etc.
 * Note: We have our own sys/smp.h in the linuxkpi include path that provides
 * FreeBSD-compatible definitions.
 */
#include <sys/smp.h>
#include <machine/smp.h>  /* For smp_active_mask */

/*
 * all_cpus - FreeBSD cpuset_t containing all CPUs.
 * DragonFly has smp_active_mask which serves the same purpose.
 * Declare as extern and use smp_active_mask.
 */
#ifndef all_cpus
#define all_cpus smp_active_mask
#endif

/*
 * FreeBSD bitset compatibility for cpuset operations.
 *
 * FreeBSD uses sys/bitset.h with _BITSET_BITS, __bitset_words, etc.
 * DragonFly's cpumask_t uses 'ary' array with different macros.
 * We provide compatible definitions here.
 */

/* Bits per word in a bitset (64-bit on DragonFly x86_64) */
#ifndef _BITSET_BITS
#define _BITSET_BITS	64
#endif

/*
 * __bitset_words - number of words needed to store n bits.
 * FreeBSD: howmany(n, _BITSET_BITS)
 */
#ifndef __bitset_words
#define __bitset_words(n)	(((n) + (_BITSET_BITS - 1)) / _BITSET_BITS)
#endif

/*
 * __bitset_word - which word contains bit n.
 * FreeBSD: (n) / _BITSET_BITS
 */
#ifndef __bitset_word
#define __bitset_word(n, _siz)	((n) / _BITSET_BITS)
#endif

/*
 * CPU_SETSIZE - maximum number of CPUs.
 * FreeBSD defines this, DragonFly uses MAXCPU.
 */
#ifndef CPU_SETSIZE
#define CPU_SETSIZE	MAXCPU
#endif

/*
 * DragonFly cpumask_t.__bits compatibility.
 *
 * FreeBSD cpuset_t has __bits[] array.
 * DragonFly cpumask_t has ary[] array.
 * For LinuxKPI code that accesses __bits directly, we need a workaround.
 *
 * IMPORTANT: Code that directly accesses ((cpuset_t *)ptr)->__bits[i]
 * must be modified to use ((cpumask_t *)ptr)->ary[i] on DragonFly.
 * This is handled via conditional compilation in linux_compat.c.
 */

/*
 * mallocarray - Safe array memory allocation with overflow checking.
 * FreeBSD: void *mallocarray(size_t nmemb, size_t size, struct malloc_type *type, int flags)
 * DragonFly: Doesn't have this - provide inline implementation.
 *
 * Note: We use DragonFly's native kmalloc here which takes (size, type, flags).
 * This is different from LinuxKPI's kmalloc which takes (size, flags).
 */
#ifndef mallocarray
#include <sys/malloc.h>

static __inline void *
mallocarray(size_t nmemb, size_t size, struct malloc_type *type, int flags)
{
	/* Check for overflow */
	if (size != 0 && nmemb > (SIZE_MAX / size))
		return (NULL);
	/* Use DragonFly's native kmalloc(size, type, flags) */
	return (kmalloc(nmemb * size, type, flags));
}
#endif

/*
 * Note: kmalloc_array is defined in linux/slab.h for LinuxKPI.
 * Do NOT define it here to avoid conflicts.
 */

/* Ensure sys/taskqueue.h is included for taskqueue support */
#include <sys/taskqueue.h>

/*
 * taskqueue_thread_enqueue compatibility.
 * FreeBSD's taskqueue_thread_enqueue is defined in sys/taskqueue.h.
 * DragonFly also has it - no mapping needed.
 *
 * NOTE: Do NOT define _SYS_TASKQUEUE_H_ here as it would prevent the
 * real sys/taskqueue.h from being included.
 */

/*
 * taskqueue_drain_all is now implemented in sys/kern/subr_taskqueue.c
 * and declared in sys/sys/taskqueue.h. No stub needed here.
 */

/* Missing sys/kdb.h - DragonFly has different debugger interface */
#ifndef _SYS_KDB_H_
#define _SYS_KDB_H_

/* Stub for now - DragonFly doesn't use kdb in the same way */
#define KDB_WHY_UNSET 0
#define KDB_WHY_BOOT 1

/* 
 * kdb_active - return whether kernel debugger is active.
 * DragonFly doesn't have an equivalent, so always return 0.
 * Define as macro to be compatible with spinlock.h which may also define it.
 */
#ifndef kdb_active
#define kdb_active 0
#endif

#endif /* _SYS_KDB_H_ */

/* Missing sys/domainset.h - FreeBSD NUMA domain sets */
#ifndef _SYS_DOMAINSET_H_
#define _SYS_DOMAINSET_H_

#include <sys/_cpuset.h>

typedef cpuset_t domainset_t;

#define DOMAINSET_NULL NULL
#define DOMAINSET_SIZE(n) CPUSET_SIZE
#define DOMAINSET_SET(n, p) CPU_SET(n, p)
#define DOMAINSET_CLR(n, p) CPU_CLR(n, p)
#define DOMAINSET_ISSET(n, p) CPU_ISSET(n, p)
#define DOMAINSET_COPY(f, t) CPU_COPY(f, t)
#define DOMAINSET_ZERO(p) CPU_ZERO(p)
#define DOMAINSET_FSET(p) CPU_FSET(p)
#define DOMAINSET_PCNT(p) CPU_COUNT(p)
#define DOMAINSET_SUB(s1, s2, d) CPU_SUB(s1, s2, d)
#define DOMAINSET_CMP(s1, s2) CPU_CMP(s1, s2)

#define domainset_first setfirst

#endif /* _SYS_DOMAINSET_H_ */

/* Missing sys/mutex2.h - FreeBSD internal mutex details */
#ifndef _SYS_MUTEX2_H_
#define _SYS_MUTEX2_H_

/* DragonFly doesn't have mutex2.h - use regular mutex.h */
#include <sys/mutex.h>

#endif /* _SYS_MUTEX2_H_ */

/* Missing sys/lock2.h - FreeBSD internal lock details */
#ifndef _SYS_LOCK2_H_
#define _SYS_LOCK2_H_

/* DragonFly doesn't have lock2.h - use regular lock.h */
#include <sys/lock.h>

#endif /* _SYS_LOCK2_H_ */

/* Missing opt_stack.h - generated header in FreeBSD */
#ifndef _OPT_STACK_H_
#define _OPT_STACK_H_

/* Stack protector options - default to enabled */
#ifndef STACK_PROTECTOR
#define STACK_PROTECTOR 1
#endif

#endif /* _OPT_STACK_H_ */

/* Additional compatibility macros */

/* FreeBSD curthread vs DragonFly curthread - same name, should work */
/* FreeBSD curproc vs DragonFly curproc - same name, should work */

/* CPU time ticks - DragonFly uses different fields */
#define ticks ticks

/* Process time tracking */
#define p_timetick td_sticks  /* DragonFly uses td_sticks in thread */

#ifndef CTLFLAG_RDTUN
#define CTLFLAG_RDTUN CTLFLAG_RD
#endif
#ifndef CTLFLAG_NOFETCH
#define CTLFLAG_NOFETCH 0
#endif
#ifndef CTLFLAG_MPSAFE
/* DragonFly doesn't need MPSAFE flag - sysctl is already MP-safe */
#define CTLFLAG_MPSAFE 0
#endif

#ifndef SI_SUB_EVENTHANDLER
#define SI_SUB_EVENTHANDLER SI_SUB_PSEUDO
#endif

/* Additional SI_SUB_* constants used by LinuxKPI that DragonFly doesn't have */
#ifndef SI_SUB_LOCK
#define SI_SUB_LOCK SI_SUB_PRE_DRIVERS  /* Early, for lock initialization */
#endif
#ifndef SI_SUB_CPU
#define SI_SUB_CPU SI_SUB_PRE_DRIVERS   /* Early, for per-CPU initialization */
#endif

/*
 * SYSCTL_FOREACH - iterate over sysctl children.
 * FreeBSD has this macro, DragonFly doesn't.
 * DragonFly uses SLIST for sysctl_oid list.
 */
#ifndef SYSCTL_FOREACH
#define SYSCTL_FOREACH(oid, list) \
    SLIST_FOREACH(oid, list, oid_link)
#endif

#ifndef MA_OWNED
#define MA_OWNED 1
#endif
#ifndef MA_NOTOWNED
#define MA_NOTOWNED 2
#endif

#ifndef mtx_assert
/*
 * DragonFly struct mtx uses 'mtx_lock' field.
 * MTX_EXCLUSIVE is the flag for exclusive/write lock.
 */
#define mtx_assert(_m, _what) do { \
	if ((_what) == MA_OWNED) \
		KKASSERT(((_m)->mtx_lock & MTX_EXCLUSIVE) != 0); \
	else if ((_what) == MA_NOTOWNED) \
		KKASSERT(((_m)->mtx_lock & MTX_EXCLUSIVE) == 0); \
} while (0)
#endif

#ifndef PROC_LOCK
#define PROC_LOCK(p) lwkt_gettoken(&(p)->p_token)
#define PROC_UNLOCK(p) lwkt_reltoken(&(p)->p_token)
#define PROC_LOCK_ASSERT(p, what) do { \
	if ((what) == MA_OWNED) \
		ASSERT_LWKT_TOKEN_HELD(&(p)->p_token); \
} while (0)
#endif

#ifndef sched_pin
static __inline void
sched_pin(void)
{
}

static __inline void
sched_unpin(void)
{
}
#endif

#ifndef FOREACH_THREAD_IN_PROC
#define FOREACH_THREAD_IN_PROC(p, td) \
	for (struct lwp *__lp = RB_FIRST(lwp_rb_tree, &(p)->p_lwp_tree); \
	    __lp != NULL && ((td) = __lp->lwp_thread, 1); \
	    __lp = RB_NEXT(lwp_rb_tree, &(p)->p_lwp_tree, __lp))
#endif

#ifndef p_flag
#define p_flag p_flags
#endif

/*
 * DPCPU (Dynamic Per-CPU) compatibility.
 * 
 * FreeBSD's DPCPU provides per-CPU storage for variables.
 * DragonFly uses globaldata but with a different API.
 * 
 * For now, we use simple static arrays indexed by CPU ID.
 * This is not as efficient as true per-CPU storage but works for compilation.
 */
#ifndef DPCPU_DEFINE_STATIC
#include <machine/globaldata.h>  /* for MAXCPU */

/* Define a static per-CPU variable as an array indexed by CPU ID */
#define DPCPU_DEFINE_STATIC(type, name) \
    static type __dpcpu_##name[MAXCPU]

/* Get pointer to per-CPU variable for a specific CPU */
#define DPCPU_ID_PTR(cpu, name) \
    (&__dpcpu_##name[(cpu)])

/* Get reference to per-CPU variable for current CPU */
#define DPCPU_GET(name) \
    (__dpcpu_##name[mycpuid])

/* Get value from specific CPU */
#define DPCPU_ID_GET(cpu, name) \
    (__dpcpu_##name[(cpu)])

/* Set value for specific CPU */
#define DPCPU_ID_SET(cpu, name, val) \
    do { __dpcpu_##name[(cpu)] = (val); } while (0)

/* Set value for current CPU */
#define DPCPU_SET(name, val) \
    do { __dpcpu_##name[mycpuid] = (val); } while (0)

#endif /* DPCPU_DEFINE_STATIC */

static __inline void
pmap_invalidate_cache(void)
{
	cpu_wbinvd_on_all_cpus();
}

static __inline void
pmap_force_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva)
{
	pmap_invalidate_cache_range(sva, eva);
}

/*
 * PMAP_HAS_DMAP - DragonFly x86_64 always has Direct Map.
 * FreeBSD uses this as a compile-time constant.
 */
#ifndef PMAP_HAS_DMAP
#define PMAP_HAS_DMAP 1
#endif

/*
 * kmem_free compatibility.
 * FreeBSD: void kmem_free(void *addr, vm_size_t size)
 * DragonFly: void kmem_free(struct vm_map *map, vm_offset_t addr, vm_size_t size)
 *
 * DragonFly requires a map argument - use kernel_map for kernel allocations.
 * Note: kernel_map is declared in vm/vm_kern.h
 */
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>  /* For kernel_map */

#ifndef lkpi_kmem_free
#define lkpi_kmem_free(addr, size) kmem_free(kernel_map, (vm_offset_t)(addr), (size))
#endif

/* Missing vm/uma.h components if needed */
#ifdef _VM_UMA_H_
/* DragonFly has uma in sys/uma.h */
#endif

/*
 * THREAD_CAN_SLEEP - Check if the current thread can safely sleep.
 *
 * FreeBSD checks: not in interrupt context, not in epoch, no spinlocks held.
 * DragonFly equivalent: not in interrupt context (gd_intr_nesting_level == 0)
 * and the thread is not marked with TDF_INTTHREAD.
 */
#ifndef THREAD_CAN_SLEEP
#include <sys/globaldata.h>
#include <sys/thread2.h>

static __inline int
THREAD_CAN_SLEEP(void)
{
	struct thread *td = curthread;

	/*
	 * Can't sleep if:
	 * - We're in interrupt context (gd_intr_nesting_level > 0)
	 * - We're an interrupt thread (TDF_INTTHREAD)
	 * - We're in a critical section (td_critcount > 0)
	 */
	if (mycpu->gd_intr_nesting_level > 0)
		return (0);
	if (td->td_flags & TDF_INTTHREAD)
		return (0);
	if (td->td_critcount > 0)
		return (0);
	return (1);
}
#endif

/*
 * File descriptor API compatibility.
 *
 * FreeBSD uses Capsicum capability rights and different function signatures.
 * DragonFly doesn't have Capsicum and has simpler APIs.
 */

/* Capsicum compatibility - DragonFly doesn't have Capsicum */
#ifndef _SYS_CAPSICUM_H_
#define _SYS_CAPSICUM_H_
/* Dummy cap_rights_t for APIs that expect it */
typedef struct cap_rights {
    int dummy;
} cap_rights_t;

/* cap_no_rights - empty capability set (allows nothing but needed for API) */
static const cap_rights_t cap_no_rights = { 0 };

#define cap_rights_init(rights, ...) do { (void)(rights); } while(0)
#define cap_rights_set(rights, ...) do { (void)(rights); } while(0)
#endif /* _SYS_CAPSICUM_H_ */

/*
 * fget_unlocked - get file pointer by fd without locking.
 * FreeBSD: int fget_unlocked(thread *, int fd, cap_rights_t *, file **, seq *)
 * DragonFly: Use holdfp() which does similar thing.
 */
#ifndef fget_unlocked
#include <sys/file.h>
#include <sys/filedesc.h>

static __inline int
fget_unlocked(struct thread *td __unused, int fd, const cap_rights_t *rights __unused,
    struct file **fpp)
{
    struct file *fp;
    
    /* holdfp gets file and adds reference, returns NULL if invalid */
    fp = holdfp(curthread, fd, -1);  /* -1 means any flag */
    if (fp == NULL) {
        *fpp = NULL;
        return (EBADF);
    }
    *fpp = fp;
    return (0);
}
#endif

/*
 * fdrop - drop reference to file.
 * FreeBSD: fdrop(file, thread) - takes 2 args
 * DragonFly: fdrop(file) - takes 1 arg
 * Use macro to absorb the extra argument.
 */
#define lkpi_fdrop(fp, td) fdrop(fp)

/*
 * finit - initialize a file structure.
 * FreeBSD: void finit(file *, flags, type, data, ops)
 * DragonFly: Files are initialized differently, ops set via f_ops.
 */
static __inline void
lkpi_finit(struct file *fp, int flags, int type, void *data,
    const struct fileops *ops)
{
    fp->f_flag = flags;
    fp->f_type = type;
    fp->f_data = data;
    fp->f_ops = ops;
}

/*
 * fdclose - close a file descriptor.
 * FreeBSD: int fdclose(thread *, file *, fd)
 * DragonFly: Use funsetfd() to remove from descriptor table, then fdrop.
 */
static __inline void
lkpi_fdclose(struct thread *td, struct file *fp, int fd)
{
	dropfp(td, fd, fp);
}

/*
 * falloc - allocate a file structure and descriptor.
 * FreeBSD: int falloc(thread *, file **, int *fd, int flags)
 * DragonFly: int falloc(lwp *, file **, int *fd) - no flags
 */
static __inline int
lkpi_falloc(struct thread *td __unused, struct file **resultfp, int *resultfd,
    int flags __unused)
{
    /* DragonFly uses curthread->td_lwp */
    return falloc(curthread->td_lwp, resultfp, resultfd);
}

/* DTYPE_DEV - DragonFly doesn't have this, use DTYPE_VNODE or custom */
#ifndef DTYPE_DEV
#define DTYPE_DEV DTYPE_DMABUF  /* Use DMA buffer type for DRM devices */
#endif

/*
 * CACHE_LINE_SIZE compatibility.
 * FreeBSD uses CACHE_LINE_SIZE, DragonFly uses __VM_CACHELINE_SIZE.
 */
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE __VM_CACHELINE_SIZE
#endif

/*
 * sbintime conversion macros.
 *
 * FreeBSD has these in sys/time.h, DragonFly doesn't.
 * sbintime_t is a 64-bit signed integer representing time in units of
 * 2^-32 seconds (about 233 picoseconds).
 */
#ifndef SBT_1S
#define SBT_1S		((sbintime_t)1 << 32)
#define SBT_1MS		(SBT_1S / 1000)
#define SBT_1US		(SBT_1S / 1000000)
#define SBT_1NS		(SBT_1S / 1000000000)
#define SBT_MAX		0x7fffffffffffffffLL
#endif

#ifndef mstosbt
#define mstosbt(ms)	(((sbintime_t)(ms)) * SBT_1MS)
#define ustosbt(us)	(((sbintime_t)(us)) * SBT_1US)
#define nstosbt(ns)	(((sbintime_t)(ns)) * SBT_1NS)
#define sbttons(sbt)	((int64_t)(1000000000 * (sbt) >> 32))
#define sbttous(sbt)	((int64_t)(1000000 * (sbt) >> 32))
#define sbttoms(sbt)	((int64_t)(1000 * (sbt) >> 32))
#endif

/*
 * tick_sbt - sbintime per system tick.
 * Used by sleepqueue.h's sleepq_set_timeout macro.
 * In FreeBSD this is defined in kern_tc.c; DragonFly doesn't have it.
 */
#ifndef tick_sbt
#define tick_sbt	(SBT_1S / hz)
#endif

/*
 * callout_reset_on/callout_reset_sbt/callout_schedule_sbt compatibility.
 */
static __inline int
lkpi_sbt_to_ticks(sbintime_t sbt)
{
	if (sbt <= 0)
		return (0);
	return ((int)((sbt + tick_sbt - 1) / tick_sbt));
}

#ifndef callout_reset_on
#define callout_reset_on(c, t, f, a, cpu) \
	callout_reset_bycpu((c), (t), (f), (a), (cpu))
#endif
#ifndef callout_reset_sbt
#define callout_reset_sbt(c, sbt, pr, flags, f, a, ...) \
	callout_reset_bycpu((c), lkpi_sbt_to_ticks(sbt), (f), (a), 0)
#endif
#ifndef callout_schedule_sbt
#define callout_schedule_sbt(c, sbt, pr, flags) \
	callout_reset((c), lkpi_sbt_to_ticks(sbt), callout_func(c), callout_arg(c))
#endif

/*
 * Callout flags used by FreeBSD's pause_sbt().
 * DragonFly has different callout implementation, these are no-ops.
 */
#ifndef C_HARDCLOCK
#define C_HARDCLOCK	0x0001	/* Align to hardclock (ignored in DragonFly) */
#endif
#ifndef C_CATCH
#define C_CATCH		0x0002	/* Catch signals (maps to PCATCH) */
#endif
#ifndef C_ABSOLUTE
#define C_ABSOLUTE	0x0004	/* Absolute time (not relative) */
#endif
#ifndef C_PREL
#define C_PREL(x)	0	/* Precision (ignored) */
#endif

/*
 * pause_sbt - Sleep for a specified amount of time.
 *
 * FreeBSD: int pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t pr, int flags)
 * DragonFly: Use tsleep() with PCATCH if C_CATCH is set.
 *
 * Returns 0 on timeout, EWOULDBLOCK on timeout (same as 0), 
 * or signal error if interrupted.
 */
static __inline int
pause_sbt(const char *wmesg, sbintime_t sbt, sbintime_t pr __unused, int flags)
{
    int timo;
    int slpflags;
    int ret;
    
    /* Convert sbintime to ticks, rounding up */
    if (sbt >= SBT_MAX)
        timo = 0;  /* infinite */
    else {
        /* sbt / tick_sbt, but avoid division - multiply by hz and shift */
        timo = (int)((sbt * hz + SBT_1S - 1) >> 32);
        if (timo < 1)
            timo = 1;
    }
    
    /* Map C_CATCH to PCATCH for interruptible sleep */
    slpflags = 0;
    if (flags & C_CATCH)
        slpflags |= PCATCH;
    
    /*
     * tsleep on a dummy wait channel.
     * We use the wmesg pointer itself as the wait channel since
     * nothing will wake us - we rely entirely on the timeout.
     */
    ret = tsleep(&pause_sbt, slpflags, wmesg, timo);
    
    return (ret);
}

/*
 * pause - Sleep for a specified number of ticks.
 * FreeBSD has this, DragonFly doesn't.
 */
#ifndef pause
static __inline int
lkpi_pause(const char *wmesg, int timo)
{
    if (timo < 1)
        timo = 1;
    return tsleep(&lkpi_pause, 0, wmesg, timo);
}
#define pause(wmesg, timo) lkpi_pause(wmesg, timo)
#endif

/*
 * WITNESS_WARN compatibility.
 * FreeBSD uses WITNESS for lock debugging. DragonFly doesn't have it.
 * Make it a no-op.
 */
#ifndef WITNESS_WARN
#define WARN_GIANTOK    0x01
#define WARN_SLEEPOK    0x02
#define WITNESS_WARN(flags, lock, msg, ...) do { } while(0)
#endif

/*
 * Thread priority and scheduler compatibility.
 *
 * FreeBSD uses td_priority (0-255, lower = higher priority).
 * DragonFly uses td_pri (0-31, 31 = highest priority).
 * These are fundamentally different, so we provide stub mappings.
 */

/* td_priority - DragonFly uses td_pri with inverted meaning */
#ifndef td_priority
/* 
 * Map FreeBSD's 0-255 (lower=better) to DragonFly's 0-31 (higher=better).
 * For now, just use td_pri directly - callers should be updated if needed.
 */
#define td_priority td_pri
#endif

/*
 * td_critnest - FreeBSD critical section nesting counter.
 * DragonFly uses td_critcount for the same purpose.
 */
#ifndef td_critnest
#define td_critnest td_critcount
#endif

/*
 * td_pinned - FreeBSD tracks CPU pinning in struct thread.
 * DragonFly uses different mechanism (critical sections, lwkt_migratecpu).
 * For basic compat, we provide a dummy field via a macro.
 * XXX: This is a stub - actual pinning needs different approach.
 */
#ifndef td_pinned
/* DragonFly doesn't track pinning this way - use td_critcount as proxy */
#define td_pinned td_critcount
#endif

/*
 * td_inhibitors - FreeBSD tracks thread inhibit reasons.
 * DragonFly doesn't have this. Use TDF flags as a rough proxy.
 */
#ifndef td_inhibitors
/* Check if thread is blocked by looking at TDF flags */
#define td_inhibitors td_flags
#endif

/*
 * thread_lock / thread_unlock - FreeBSD per-thread spinlock.
 * DragonFly uses LWKT and doesn't have per-thread spinlocks in this way.
 * Use critical sections as a rough equivalent.
 */
#ifndef thread_lock
#define thread_lock(td)   crit_enter()
#define thread_unlock(td) crit_exit()
#endif

/*
 * sched_prio - FreeBSD scheduler priority setting.
 * DragonFly uses different priority system.
 * Stub for now - priority management needs rethinking for DragonFly.
 */
#ifndef sched_prio
#define sched_prio(td, prio) do { (void)(td); (void)(prio); } while(0)
#endif

/*
 * sched_bind / sched_unbind - FreeBSD CPU binding.
 * DragonFly uses lwkt_migratecpu() for CPU migration.
 */
#ifndef sched_bind
static __inline void
sched_bind(struct thread *td __unused, int cpu)
{
    lwkt_migratecpu(cpu);
}
#endif

#ifndef sched_unbind
static __inline void
sched_unbind(struct thread *td __unused)
{
    /* DragonFly doesn't track binding state the same way */
    /* Thread will stay on current CPU until explicitly migrated */
}
#endif

#ifndef sched_is_bound
static __inline int
sched_is_bound(struct thread *td __unused)
{
    /* DragonFly doesn't track binding state this way */
    return (0);
}
#endif

/*
 * mi_switch - FreeBSD context switch with flags.
 * DragonFly uses lwkt_switch() directly.
 * The SW_VOL and SWT_RELINQUISH flags indicate voluntary relinquish.
 */
#ifndef SW_VOL
#define SW_VOL         0x0001  /* Voluntary switch */
#endif
#ifndef SWT_RELINQUISH
#define SWT_RELINQUISH 0x0001  /* Relinquish CPU */
#endif

#ifndef mi_switch
static __inline void
mi_switch(int flags __unused)
{
    /* DragonFly uses lwkt_switch for voluntary context switch */
    lwkt_yield();
}
#endif

/*
 * DROP_GIANT / PICKUP_GIANT - FreeBSD Giant lock handling.
 * DragonFly removed Giant long ago. These are no-ops.
 */
#ifndef DROP_GIANT
#define DROP_GIANT()   do { } while(0)
#endif
#ifndef PICKUP_GIANT
#define PICKUP_GIANT() do { } while(0)
#endif

/*
 * PCPU_GET(cpuid) - get current CPU ID.
 * FreeBSD uses PCPU_GET(cpuid) to get current CPU.
 * DragonFly uses mycpuid directly.
 * Only handle the 'cpuid' case since that's what's commonly used.
 */
#ifndef PCPU_GET
#define PCPU_GET(field) _pcpu_get_##field()
static __inline int _pcpu_get_cpuid(void) { return mycpuid; }
#endif

/*
 * SI_SUB_TASKQ - FreeBSD SYSINIT subsystem for taskqueues.
 * DragonFly doesn't have this; use SI_SUB_HELPER_THREADS instead.
 */
#ifndef SI_SUB_TASKQ
#define SI_SUB_TASKQ SI_SUB_HELPER_THREADS
#endif

/*
 * PWAIT - FreeBSD process wait priority.
 * DragonFly uses different priority system (TDPRI_*).
 * Map to TDPRI_KERN_USER which is reasonable for kernel daemons.
 */
#ifndef PWAIT
#define PWAIT TDPRI_KERN_USER
#endif

/*
 * FreeBSD scheduler constants.
 * These are used by FreeBSD's ULE scheduler but not present in DragonFly.
 */

/* Software interrupt priorities - FreeBSD uses SWI_* for interrupt levels */
#ifndef SWI_NET
#define SWI_NET		4	/* Network software interrupt */
#endif
#ifndef SWI_CLOCK
#define SWI_CLOCK	0	/* Clock software interrupt */
#endif
#ifndef SWI_VM
#define SWI_VM		5	/* VM software interrupt */
#endif
#ifndef SWI_TQ
#define SWI_TQ		6	/* Taskqueue software interrupt */
#endif

/* PI_SWI - Convert software interrupt level to priority */
#ifndef PI_SWI
#define PI_SWI(x)	TDPRI_KERN_USER  /* Map all SWI to kernel user priority */
#endif

/* Scheduler request flags - used by sched_add() in FreeBSD */
#ifndef SRQ_BORING
#define SRQ_BORING	0x0000	/* Not interesting - regular add */
#endif
#ifndef SRQ_YIELDING
#define SRQ_YIELDING	0x0001	/* Thread is yielding */
#endif
#ifndef SRQ_OURSELF
#define SRQ_OURSELF	0x0002	/* Adding ourselves */
#endif
#ifndef SRQ_INTR
#define SRQ_INTR	0x0004	/* Interrupt context */
#endif
#ifndef SRQ_PREEMPTED
#define SRQ_PREEMPTED	0x0008	/* Thread was preempted */
#endif
#ifndef SRQ_BORROWING
#define SRQ_BORROWING	0x0010	/* Priority borrowing */
#endif

/*
 * sched_add - FreeBSD scheduler function to add thread to run queue.
 * DragonFly uses lwkt_schedule() instead.
 * This is a stub that does nothing useful - the code path using this
 * may need rethinking for DragonFly.
 */
#ifndef sched_add
#define sched_add(td, flags) do { (void)(td); (void)(flags); } while(0)
#endif

/*
 * taskqueue_start_threads compatibility.
 * FreeBSD: int taskqueue_start_threads(tqp, count, pri, fmt, ...)
 * DragonFly: int taskqueue_start_threads(tqp, count, pri, ncpu, fmt, ...)
 *
 * DragonFly has an extra ncpu parameter. Callers should use #ifdef __DragonFly__
 * to add the extra -1 parameter (meaning any CPU).
 */

/*
 * taskqueue_create_fast - FreeBSD has this separate function.
 * DragonFly's taskqueue_create is already fast enough.
 */
#ifndef taskqueue_create_fast
#define taskqueue_create_fast taskqueue_create
#endif

/*
 * bus_get_domain - Get NUMA domain for a device.
 * FreeBSD: int bus_get_domain(device_t dev, int *domain)
 * DragonFly: Doesn't have NUMA support, always return failure.
 */
#ifndef bus_get_domain
#include <sys/bus.h>  /* For device_t */

static __inline int
bus_get_domain(device_t dev __unused, int *domain __unused)
{
    /* DragonFly doesn't have NUMA - return error to indicate no domain info */
    return (ENXIO);
}
#endif

/*
 * bus_topo_lock / bus_topo_unlock - Bus topology locking.
 * FreeBSD: Used for synchronizing bus topology changes.
 * DragonFly: Doesn't have these - provide no-op stubs.
 */
#ifndef bus_topo_lock
static __inline void
bus_topo_lock(void)
{
    /* DragonFly doesn't have bus topology locks - no-op */
}
#endif

#ifndef bus_topo_unlock
static __inline void
bus_topo_unlock(void)
{
    /* DragonFly doesn't have bus topology locks - no-op */
}
#endif

/*
 * firmware_get_flags - Get firmware with flags.
 * FreeBSD: const struct firmware *firmware_get_flags(const char *imagename, uint32_t flags)
 * DragonFly: Only has firmware_get(const char *imagename) - no flags parameter.
 * We ignore the flags and call the regular firmware_get.
 */
#ifndef firmware_get_flags
#include <sys/firmware.h>

/* Firmware get flags - used by LinuxKPI firmware code */
#ifndef FIRMWARE_GET_NOWARN
#define FIRMWARE_GET_NOWARN     0x0001  /* Don't warn if not found */
#endif

static __inline const struct firmware *
firmware_get_flags(const char *imagename, uint32_t flags __unused)
{
    /* DragonFly's firmware_get doesn't support flags - just call it directly */
    return firmware_get(imagename);
}
#endif

/*
 * strdup - Duplicate a string with kernel memory allocation.
 * FreeBSD: char *strdup(const char *s, struct malloc_type *type)
 * DragonFly: Also has this in libkern - should be available.
 * But if not, provide a simple implementation.
 */
#ifndef HAVE_STRDUP
#include <sys/libkern.h>
/* DragonFly should have kstrdup or strdup in libkern */
#endif

/*
 * taskqueue_thread - access the system thread taskqueue.
 * FreeBSD: struct taskqueue *taskqueue_thread (single pointer)
 * DragonFly: struct taskqueue *taskqueue_thread[] (array indexed by CPU)
 *
 * For LinuxKPI, we typically want to use the current CPU's taskqueue.
 * Define a macro to get the appropriate taskqueue.
 */
#ifndef LKPI_TASKQUEUE_THREAD
#define LKPI_TASKQUEUE_THREAD()  (taskqueue_thread[mycpuid])
#endif

/*
 * Override mtx_init for LinuxKPI source files.
 *
 * FreeBSD mtx_init takes 4 arguments: (mtx, name, type, opts)
 * DragonFly mtx_init takes 2 arguments: (mtx, ident)
 *
 * This macro redirects 4-argument mtx_init calls to our lkpi_mtx_init wrapper.
 * We use a variadic macro that discards the extra arguments.
 *
 * NOTE: This is safe to do here because:
 * 1. This header is included after sys/mutex2.h (which defines the real mtx_init)
 * 2. This header is only used by LinuxKPI code, not general DragonFly code
 * 3. LinuxKPI code expects FreeBSD semantics
 */
#undef mtx_init
#define mtx_init(mtx, name, type, opts)	lkpi_mtx_init(mtx, name, type, opts)

/*
 * PCI Express register name compatibility.
 *
 * DragonFly uses different names for some PCIe registers and constants.
 * Map FreeBSD names to DragonFly equivalents.
 */

/* PCIEM_FLAGS_TYPE - FreeBSD name for port type mask */
#ifndef PCIEM_FLAGS_TYPE
#define PCIEM_FLAGS_TYPE	PCIEM_CAP_PORT_TYPE	/* 0x00f0 */
#endif

/* PCIEM_CTL_MAX_READ_REQUEST - FreeBSD name for max read request */
#ifndef PCIEM_CTL_MAX_READ_REQUEST
#define PCIEM_CTL_MAX_READ_REQUEST	PCIEM_DEVCTL_MAX_READRQ_MASK
#endif

/* PCIV_INVALID - invalid vendor ID, DragonFly may not define this */
#ifndef PCIV_INVALID
#define PCIV_INVALID	0xFFFF
#endif

/* pci_enable_aspm - FreeBSD sysctl for ASPM (Active State Power Management) */
#ifndef pci_enable_aspm
/* DragonFly doesn't have this sysctl - define as constant false */
#define pci_enable_aspm	0
#endif

/* BUS_PROBE_NOWILDCARD - FreeBSD bus probe constant */
#ifndef BUS_PROBE_NOWILDCARD
#define BUS_PROBE_NOWILDCARD	BUS_PROBE_SPECIFIC
#endif

/* OSRELEASELEN - FreeBSD release string length constant */
#ifndef OSRELEASELEN
#define OSRELEASELEN	32
#endif

/*
 * Missing CPU vendor constants for LinuxKPI.
 *
 * DragonFly only defines CPU_VENDOR_AMD, CPU_VENDOR_INTEL, CPU_VENDOR_IDT,
 * and CPU_VENDOR_CENTAUR in machine/cputypes.h. FreeBSD has more vendors
 * which LinuxKPI uses in a vendor mapping table.
 *
 * Define missing ones with placeholder PCI vendor IDs or unique values.
 * These are primarily used for vendor identification in the Linux x86 boot
 * data structures.
 */
#include <machine/cputypes.h>

#ifndef CPU_VENDOR_CYRIX
#define CPU_VENDOR_CYRIX	0x1078	/* Cyrix (defunct) */
#endif
#ifndef CPU_VENDOR_UMC
#define CPU_VENDOR_UMC		0x1060	/* UMC (defunct) */
#endif
#ifndef CPU_VENDOR_TRANSMETA
#define CPU_VENDOR_TRANSMETA	0x1279	/* Transmeta (defunct) */
#endif
#ifndef CPU_VENDOR_NSC
#define CPU_VENDOR_NSC		0x100b	/* National Semiconductor (defunct) */
#endif
#ifndef CPU_VENDOR_HYGON
#define CPU_VENDOR_HYGON	0x1d94	/* Hygon (AMD licensee) */
#endif

/*
 * SYSCTL_ADD_ROOT_NODE - FreeBSD macro to add a root sysctl node.
 * DragonFly doesn't have this; use SYSCTL_ADD_NODE with NULL parent.
 */
#ifndef SYSCTL_ADD_ROOT_NODE
#define SYSCTL_ADD_ROOT_NODE(ctx, nbr, name, access, handler, descr) \
    SYSCTL_ADD_NODE(ctx, &sysctl__children, nbr, name, access, handler, descr)
#endif

/*
 * rw_init - FreeBSD read/write lock initialization.
 * FreeBSD: void rw_init(struct rwlock *rw, const char *name)
 * DragonFly: void lockinit(struct lock *lkp, const char *wmesg, int timo, int flags)
 *
 * DragonFly uses struct lock instead of struct rwlock.
 * For LinuxKPI, we need to provide a compatible interface.
 * XXX: This is a stub - may need proper rwlock emulation later.
 */
#ifndef rw_init
#include <sys/lock.h>
#define rw_init(rw, name)	lockinit((struct lock *)(rw), (name), 0, LK_CANRECURSE)
#endif

/*
 * DEVICE_UNIT_ANY - FreeBSD constant for auto-assigning device unit number.
 * DragonFly uses -1 directly, but FreeBSD names the constant.
 */
#ifndef DEVICE_UNIT_ANY
#define DEVICE_UNIT_ANY	(-1)
#endif

/*
 * iic2errno - Convert IIC bus error codes to errno values.
 * FreeBSD has this in sys/dev/iicbus/iiconf.h.
 * DragonFly doesn't have it - provide a simple implementation.
 *
 * Note: IIC_* error codes are defined in bus/iicbus/iiconf.h on DragonFly.
 * We include it here to get the definitions.
 */
#ifndef iic2errno
#ifdef _KERNEL
#include <bus/iicbus/iiconf.h>
#endif

static __inline int
iic2errno(int iic_err)
{
	switch (iic_err) {
	case 0:
		return (0);
#ifdef IIC_ENOACK
	case IIC_ENOACK:
		return (ENXIO);
#endif
#ifdef IIC_ETIMEOUT
	case IIC_ETIMEOUT:
		return (ETIMEDOUT);
#endif
#ifdef IIC_EBUSERR
	case IIC_EBUSERR:
		return (EBUSY);
#endif
#ifdef IIC_EBUSBSY
	case IIC_EBUSBSY:
		return (EBUSY);
#endif
#ifdef IIC_ESTATUS
	case IIC_ESTATUS:
		return (EIO);
#endif
#ifdef IIC_EUNDERFLOW
	case IIC_EUNDERFLOW:
		return (EIO);
#endif
#ifdef IIC_EOVERFLOW
	case IIC_EOVERFLOW:
		return (EIO);
#endif
#ifdef IIC_ENOTSUPP
	case IIC_ENOTSUPP:
		return (EOPNOTSUPP);
#endif
#ifdef IIC_ENOADDR
	case IIC_ENOADDR:
		return (EADDRNOTAVAIL);
#endif
#ifdef IIC_ERESOURCE
	case IIC_ERESOURCE:
		return (ENOMEM);
#endif
	default:
		return (EIO);
	}
}
#endif

/*
 * bus_attach_children - FreeBSD function to attach all children.
 * DragonFly doesn't have this - provide a stub.
 * XXX: This may need a proper implementation that iterates children.
 */
#ifndef bus_attach_children
static __inline int
bus_attach_children(device_t dev)
{
	/*
	 * DragonFly doesn't have bus_attach_children().
	 * The bus probing/attachment happens differently.
	 * Return success and hope the children get attached via
	 * the normal bus rescan path.
	 */
	return (0);
}
#endif

/*
 * Additional sysctl flags.
 * CTLFLAG_RWTUN - FreeBSD read-write tunable (runtime changeable).
 * DragonFly doesn't have runtime tunables in the same way.
 */
#ifndef CTLFLAG_RWTUN
#define CTLFLAG_RWTUN	(CTLFLAG_RW | CTLFLAG_TUN)
#endif
#ifndef CTLFLAG_TUN
#define CTLFLAG_TUN	0
#endif

/*
 * Buffer lock flags - FreeBSD uses LA_*, DragonFly uses B_*.
 */
#ifndef LA_LOCKED
#define LA_LOCKED	B_LOCKED
#endif

/*
 * Priority constants - FreeBSD uses PRI_USER, DragonFly uses different scheme.
 */
#ifndef PRI_USER
#define PRI_USER	TDPRI_KERN_USER
#endif

/*
 * VM object flags - FreeBSD uses OBJ_UNMANAGED for non-pageable objects.
 * DragonFly uses different naming.
 */
#ifndef OBJ_UNMANAGED
#define OBJ_UNMANAGED	OBJ_NOSPLIT
#endif

/*
 * VM address constants - FreeBSD uses VM_MAXUSER_ADDRESS, DragonFly uses VM_MAX_USER_ADDRESS.
 */
#ifndef VM_MAXUSER_ADDRESS
#define VM_MAXUSER_ADDRESS	VM_MAX_USER_ADDRESS
#endif

/*
 * Mount flags - DragonFly should have MNT_NOEXEC in sys/mount.h.
 * If not defined, provide a stub.
 */
#ifndef MNT_NOEXEC
#define MNT_NOEXEC	0x00000004	/* From sys/mount.h */
#endif

/*
 * smp_no_rendezvous_barrier - FreeBSD SMP synchronization barrier stub.
 * DragonFly has smp_rendezvous but not the no-op barrier version.
 */
#ifndef smp_no_rendezvous_barrier
static __inline void
smp_no_rendezvous_barrier(void *arg __unused)
{
	/* No-op barrier - does nothing */
}
#endif

/*
 * kinfo_file type constants - used for file descriptor introspection.
 * DragonFly may have these in sys/user.h or equivalent.
 */
#ifndef KF_TYPE_DEV
#define KF_TYPE_DEV	4
#endif
#ifndef KF_TYPE_VNODE
#define KF_TYPE_VNODE	1
#endif

/*
 * f_vnode accessor - DragonFly stores vnode in f_data, FreeBSD has f_vnode.
 * Note: This is for accessing the BSD file's vnode, not the linux_file.
 */
#ifndef BSD_FILE_VNODE
#define BSD_FILE_VNODE(fp)	((struct vnode *)(fp)->f_data)
#endif

/*
 * TAILQ_FOREACH_SAFE - FreeBSD safe iteration macro.
 * DragonFly uses TAILQ_FOREACH_MUTABLE for the same purpose.
 */
#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
	TAILQ_FOREACH_MUTABLE(var, head, field, tvar)
#endif

/*
 * LIST_FOREACH_SAFE - FreeBSD safe iteration macro.
 * DragonFly uses LIST_FOREACH_MUTABLE for the same purpose.
 */
#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, field, tvar) \
	LIST_FOREACH_MUTABLE(var, head, field, tvar)
#endif

/*
 * STAILQ_FOREACH_SAFE - FreeBSD safe iteration macro.
 * DragonFly uses STAILQ_FOREACH_MUTABLE for the same purpose.
 */
#ifndef STAILQ_FOREACH_SAFE
#define STAILQ_FOREACH_SAFE(var, head, field, tvar) \
	STAILQ_FOREACH_MUTABLE(var, head, field, tvar)
#endif

/*
 * OBJT_SG - FreeBSD VM object type for scatter-gather.
 * DragonFly doesn't have this type. Use OBJT_DEFAULT as fallback
 * since it's closest to a generic memory object.
 */
#ifndef OBJT_SG
#define OBJT_SG	OBJT_DEFAULT
#endif

/*
 * DEVFS_IOSIZE_MAX - FreeBSD maximum I/O size for devfs operations.
 * DragonFly doesn't define this. Use a reasonable default.
 */
#ifndef DEVFS_IOSIZE_MAX
#define DEVFS_IOSIZE_MAX	SSIZE_MAX
#endif

/*
 * FIODGNAME - FreeBSD ioctl to get device name.
 * DragonFly uses FIODNAME instead.
 */
#ifndef FIODGNAME
#define FIODGNAME	FIODNAME
#endif

/*
 * struct fiodgname_arg - FreeBSD structure for FIODGNAME ioctl.
 * DragonFly uses a different interface - provide a stub structure.
 */
#ifndef HAVE_FIODGNAME_ARG
struct fiodgname_arg {
	int	len;
	void	*buf;
};
#endif

/*
 * FreeBSD ioctl helper for FIODGNAME.
 */
static __inline void *
fiodgname_buf_get_ptr(struct fiodgname_arg *fgn __unused, u_long cmd __unused)
{
	return (fgn->buf);
}

/*
 * INTR_TYPE_NET - FreeBSD interrupt type for network devices.
 * DragonFly doesn't categorize interrupts this way.
 */
#ifndef INTR_TYPE_NET
#define INTR_TYPE_NET	0
#endif

#ifndef INTR_TYPE_MISC
#define INTR_TYPE_MISC	0
#endif

#ifndef INTR_TYPE_TTY
#define INTR_TYPE_TTY	0
#endif

/*
 * FreeBSD VM_ALLOC_* flags that don't exist in DragonFly.
 * Map them to DragonFly equivalents or ignore.
 */
#ifndef VM_ALLOC_WIRED
/* DragonFly handles wiring differently - ignore for now */
#define VM_ALLOC_WIRED		0
#endif

#ifndef VM_ALLOC_NORECLAIM
/* DragonFly doesn't have reclaim flags - use NORMAL */
#define VM_ALLOC_NORECLAIM	0
#endif

#ifndef VM_ALLOC_NOBUSY
/* DragonFly doesn't have this flag - ignore */
#define VM_ALLOC_NOBUSY		0
#endif

#ifndef VM_ALLOC_WAITFAIL
/* DragonFly doesn't have this flag - ignore */
#define VM_ALLOC_WAITFAIL	0
#endif

#ifndef VM_ALLOC_NOCREAT
/* DragonFly doesn't have this flag */
#define VM_ALLOC_NOCREAT	0
#endif

/*
 * FreeBSD vm_page oflags - DragonFly uses different flags.
 * VPO_UNMANAGED - page not under pmap control.
 * DragonFly uses different mechanism - map to 0.
 */
#ifndef VPO_UNMANAGED
#define VPO_UNMANAGED	0
#endif

/*
 * FreeBSD vm_object page removal flags.
 * OBJPR_CLEANONLY - only remove clean pages.
 * DragonFly vm_object_page_remove takes boolean clean_only.
 */
#ifndef OBJPR_CLEANONLY
#define OBJPR_CLEANONLY	1  /* Will be used as boolean in DragonFly */
#endif

/*
 * counter_u64_t - FreeBSD's 64-bit lockless counter API.
 * DragonFly doesn't have this. Use simple uint64_t with atomic operations.
 * This is less efficient than FreeBSD's per-CPU implementation but works.
 */
#ifndef counter_u64_t
typedef uint64_t *counter_u64_t;

/* counter_u64_alloc - allocate a counter */
static __inline counter_u64_t
counter_u64_alloc(int waitok)
{
	uint64_t *p = kmalloc(sizeof(uint64_t), M_DEVBUF,
	    waitok ? M_WAITOK : M_NOWAIT);
	if (p)
		*p = 0;
	return p;
}

/* counter_u64_free - free a counter */
static __inline void
counter_u64_free(counter_u64_t c)
{
	kfree(c, M_DEVBUF);
}

/* counter_u64_add - add to counter */
static __inline void
counter_u64_add(counter_u64_t c, int64_t v)
{
	atomic_add_64(c, v);
}

/* counter_u64_fetch - read counter value */
static __inline uint64_t
counter_u64_fetch(counter_u64_t c)
{
	return *c;
}

/* counter_u64_zero - reset counter to zero */
static __inline void
counter_u64_zero(counter_u64_t c)
{
	*c = 0;
}
#endif /* counter_u64_t */

/*
 * SYSCTL_COUNTER_U64 - FreeBSD sysctl for counter_u64_t.
 * DragonFly doesn't have this. Use SYSCTL_QUAD as approximation.
 */
#ifndef SYSCTL_COUNTER_U64
#define SYSCTL_COUNTER_U64(parent, nbr, name, access, ptr, descr) \
	SYSCTL_QUAD(parent, nbr, name, access, (ptr), 0, descr)
#endif

/*
 * SR-IOV (Single Root I/O Virtualization) stubs.
 * DragonFly doesn't have SR-IOV support. Provide type stubs.
 */
#ifndef pci_iov_init_t
typedef int pci_iov_init_t(device_t dev, uint16_t num_vfs, void *config);
typedef void pci_iov_uninit_t(device_t dev);
typedef int pci_iov_add_vf_t(device_t dev, uint16_t vf_num, void *config);
#endif

/*
 * bus_dma_tag_create compatibility wrapper.
 *
 * FreeBSD: 14 arguments including filter functions and lock functions
 * DragonFly: 10 arguments - no filter or lock functions
 *
 * We define a macro that converts FreeBSD's call to DragonFly's signature
 * by ignoring the filter and lock arguments.
 *
 * FreeBSD signature:
 *   bus_dma_tag_create(parent, alignment, boundary, lowaddr, highaddr,
 *                      filter, filterarg, maxsize, nsegments, maxsegsz,
 *                      flags, lockfunc, lockarg, dmat)
 *
 * DragonFly signature:
 *   bus_dma_tag_create(parent, alignment, boundary, lowaddr, highaddr,
 *                      maxsize, nsegments, maxsegsz, flags, dmat)
 */
#ifndef LKPI_BUS_DMA_TAG_CREATE
#define LKPI_BUS_DMA_TAG_CREATE(parent, alignment, boundary, lowaddr, highaddr, \
    filter, filterarg, maxsize, nsegments, maxsegsz, flags, lockfunc, lockarg, dmat) \
	bus_dma_tag_create((parent), (alignment), (boundary), (lowaddr), (highaddr), \
	    (maxsize), (nsegments), (maxsegsz), (flags), (dmat))
#endif

/*
 * UMA zone stubs - DragonFly uses objcache instead of UMA.
 * NOTE: The actual UMA implementation is in sys/compat/linuxkpi/common/include/vm/uma.h
 * Do NOT define UMA types here as they will conflict with vm/uma.h.
 */

/*
 * pctrie (radix trie) stubs.
 * DragonFly doesn't have FreeBSD's pctrie. Provide minimal stubs.
 * Code using pctrie may need to be rewritten for DragonFly.
 */
#ifndef pctrie_init
struct pctrie {
	void *root;
};

static __inline void
pctrie_init(struct pctrie *ptree)
{
	ptree->root = NULL;
}

static __inline bool
pctrie_is_empty(struct pctrie *ptree)
{
	return (ptree->root == NULL);
}

/* pctrie_zone_init - FreeBSD's per-zone init for pctrie */
static __inline void
pctrie_zone_init(void *mem __unused, int size __unused, int flags __unused)
{
	/* No-op stub */
}

/* pctrie_node_size - size of a pctrie node */
static __inline size_t
pctrie_node_size(void)
{
	/* Return a reasonable node size for allocation purposes */
	return (sizeof(void *) * 8);
}

/*
 * PCTRIE_DEFINE - macro to generate pctrie functions.
 * On DragonFly, we stub this out to generate no-op functions.
 * The DMA tracking will be disabled but basic DMA operations will work.
 */
#define PCTRIE_DEFINE(name, type, field, allocfn, freefn) \
static __inline void * \
name##_PCTRIE_LOOKUP(struct pctrie *ptree __unused, uint64_t key __unused) \
{ \
	return (NULL); \
} \
static __inline int \
name##_PCTRIE_INSERT(struct pctrie *ptree __unused, struct type *val __unused) \
{ \
	return (0); \
} \
static __inline struct type * \
name##_PCTRIE_REMOVE(struct pctrie *ptree __unused, uint64_t key __unused) \
{ \
	return (NULL); \
} \
static __inline void \
name##_PCTRIE_RECLAIM(struct pctrie *ptree __unused) \
{ \
	(void)allocfn; \
	(void)freefn; \
}

#endif /* pctrie_init */

/*
 * devclass_add_driver compatibility.
 * FreeBSD: int devclass_add_driver(devclass_t dc, driver_t *driver, int flags, devclass_t *dcp)
 * DragonFly: int devclass_add_driver(devclass_t dc, driver_t *driver, devclass_t *dcp)
 *
 * Note: We can't macro-wrap this easily as it's used in device method tables.
 * Code needs to be modified with #ifdef __DragonFly__ blocks.
 */

/* BUS_PASS_DEFAULT - FreeBSD bus pass constant */
#ifndef BUS_PASS_DEFAULT
#define BUS_PASS_DEFAULT	0
#endif

/*
 * pci_alloc_msi compatibility.
 * FreeBSD: int pci_alloc_msi(device_t dev, int *count)
 * DragonFly: int pci_alloc_msi(device_t dev, int *ridp, int count, int cpuid)
 *
 * DragonFly's version has different semantics - needs wrapper.
 */
#ifndef LKPI_PCI_ALLOC_MSI
static __inline int
lkpi_pci_alloc_msi(device_t dev, int *count)
{
	int rid;
	int error;
	
	/* DragonFly pci_alloc_msi returns irq rid, not count */
	/* Try to allocate requested count, -1 means any CPU */
	error = pci_alloc_msi(dev, &rid, *count, -1);
	if (error == 0) {
		/* Success - count stays as requested */
		return 0;
	}
	return error;
}
#endif

/*
 * PCI_ID_RID and PCI_GET_ID - FreeBSD PCI ID retrieval.
 * DragonFly doesn't have these exact APIs.
 */
#ifndef PCI_ID_RID
#define PCI_ID_RID	0
#define PCI_ID_OEM	1
static __inline int
lkpi_pci_get_id(device_t parent __unused, device_t dev, int type __unused,
    uintptr_t *id)
{
	*id = pci_get_rid(dev);
	return (0);
}
#define PCI_GET_ID(parent, dev, type, id) lkpi_pci_get_id(parent, dev, type, id)
#endif

/*
 * pcicfgregs pcie member - DragonFly uses pcix.
 * Provide compatibility access where possible.
 * Note: Full structure access needs code modification.
 */

/*
 * rman_get_bushandle - Get bus handle from resource.
 * FreeBSD: bus_space_handle_t rman_get_bushandle(struct resource *r)
 * DragonFly: Has this in sys/rman.h.
 */
/* Should be available in DragonFly's rman.h */

/*
 * kmem_alloc_contig compatibility.
 * FreeBSD: vm_offset_t kmem_alloc_contig(size, flags, low, high, align, boundary, memattr)
 * DragonFly: vm_offset_t kmem_alloc_contig(size, low, high, align)
 *
 * DragonFly's version has fewer arguments.
 */
#ifndef LKPI_KMEM_ALLOC_CONTIG
#define LKPI_KMEM_ALLOC_CONTIG(size, flags, low, high, align, boundary, memattr) \
	kmem_alloc_contig((size), (low), (high), (align))
#endif

#endif /* __DragonFly__ */

#endif /* _LINUX_DRAGONFLY_COMPAT_H_ */
