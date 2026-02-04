/*-
 * Copyright (c) 2026 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VM_VM_RADIX_H_
#define _VM_VM_RADIX_H_

/*
 * DragonFly VM radix compatibility for LinuxKPI.
 *
 * Locking: callers must hold the vm_object lock while iterating or
 * performing lookups. The iterator is not safe against concurrent
 * insert/remove without the object lock held.
 */

#include <sys/types.h>
#include <sys/tree.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>

struct vm_radix {
	int _unused;
};

struct pctrie_iter {
	vm_object_t obj;
	vm_page_t cur;
	vm_pindex_t limit;
	int have_limit;
};

static __inline void
vm_radix_init(struct vm_radix *rtree)
{
	(void)rtree;
}

static __inline int
vm_radix_is_empty(struct vm_radix *rtree)
{
	(void)rtree;
	return (1);
}

static __inline void
vm_page_iter_init(struct pctrie_iter *pages, vm_object_t obj)
{
	pages->obj = obj;
	pages->cur = NULL;
	pages->have_limit = 0;
	pages->limit = 0;
}

static __inline void
vm_page_iter_limit_init(struct pctrie_iter *pages, vm_object_t obj,
    vm_pindex_t limit)
{
	pages->obj = obj;
	pages->cur = NULL;
	pages->have_limit = 1;
	pages->limit = limit;
}

static __inline void
pctrie_iter_reset(struct pctrie_iter *pages)
{
	pages->cur = NULL;
}

static __inline vm_page_t
vm_radix_rb_lookup_ge(vm_object_t obj, vm_pindex_t index)
{
	vm_page_t node;
	vm_page_t res;

	res = NULL;
	for (node = obj->rb_memq.rbh_root; node != NULL;) {
		if (index <= node->pindex) {
			res = node;
			node = RB_LEFT(node, rb_entry);
		} else {
			node = RB_RIGHT(node, rb_entry);
		}
	}
	return (res);
}

static __inline vm_page_t
vm_radix_rb_lookup_le(vm_object_t obj, vm_pindex_t index)
{
	vm_page_t node;
	vm_page_t res;

	res = NULL;
	for (node = obj->rb_memq.rbh_root; node != NULL;) {
		if (index < node->pindex) {
			node = RB_LEFT(node, rb_entry);
		} else {
			res = node;
			node = RB_RIGHT(node, rb_entry);
		}
	}
	return (res);
}

static __inline vm_page_t
vm_radix_iter_lookup(struct pctrie_iter *pages, vm_pindex_t index)
{
	vm_page_t page;

	page = vm_page_lookup(pages->obj, index);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_lookup_ge(struct pctrie_iter *pages, vm_pindex_t index)
{
	vm_page_t page;

	page = vm_radix_rb_lookup_ge(pages->obj, index);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_lookup_le(struct pctrie_iter *pages, vm_pindex_t index)
{
	vm_page_t page;

	page = vm_radix_rb_lookup_le(pages->obj, index);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_lookup_lt(struct pctrie_iter *pages, vm_pindex_t index)
{
	if (index == 0)
		return (NULL);
	return (vm_radix_iter_lookup_le(pages, index - 1));
}

static __inline vm_page_t
vm_radix_iter_next(struct pctrie_iter *pages)
{
	vm_page_t page;

	if (pages->cur == NULL)
		return (NULL);
	page = vm_page_next(pages->cur);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_step(struct pctrie_iter *pages)
{
	vm_page_t page;

	if (pages->cur == NULL)
		return (NULL);
	page = RB_NEXT(vm_page_rb_tree, &pages->obj->rb_memq, pages->cur);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_prev(struct pctrie_iter *pages)
{
	vm_page_t page;

	if (pages->cur == NULL)
		return (NULL);
	page = RB_PREV(vm_page_rb_tree, &pages->obj->rb_memq, pages->cur);
	if (page != NULL && pages->have_limit && page->pindex > pages->limit)
		page = NULL;
	pages->cur = page;
	return (page);
}

static __inline vm_page_t
vm_radix_iter_page(struct pctrie_iter *pages)
{
	return (pages->cur);
}

/* Iterate over each non-NULL page from page 'start' to the end of object. */
#define VM_RADIX_FOREACH_FROM(m, pages, start) \
	for ((m) = vm_radix_iter_lookup_ge((pages), (start)); (m) != NULL; \
	    (m) = vm_radix_iter_step((pages)))

#define VM_RADIX_FOREACH(m, pages) VM_RADIX_FOREACH_FROM((m), (pages), 0)

/* Iterate over consecutive non-NULL pages from position 'start'. */
#define VM_RADIX_FORALL_FROM(m, pages, start) \
	for ((m) = vm_radix_iter_lookup((pages), (start)); (m) != NULL; \
	    (m) = vm_radix_iter_next((pages)))

#define VM_RADIX_FORALL(m, pages) VM_RADIX_FORALL_FROM((m), (pages), 0)

#endif /* _VM_VM_RADIX_H_ */
