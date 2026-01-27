/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/boot/efi/loader/copy.c 293724 2016-01-12 02:17:39Z smh $");

#include <sys/param.h>

#include <stand.h>
#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>

#include "loader_efi.h"

#ifndef EFI_STAGING_SIZE
#define	EFI_STAGING_SIZE	96
#endif

#define	STAGE_PAGES	EFI_SIZE_TO_PAGES((EFI_STAGING_SIZE) * 1024 * 1024)

EFI_PHYSICAL_ADDRESS	staging, staging_end;
int			stage_offset_set = 0;
ssize_t			stage_offset;

/*
 * On aarch64, the kernel may be linked at a high virtual address (e.g.,
 * 0xffffff80xxxxxxxx). The loader needs to:
 * 1. Allocate physical memory for the staging area
 * 2. Use stage_offset to translate between kernel VA and staging PA
 * 3. efi_translate() converts kernel VA to physical staging address
 *
 * For aarch64, we limit allocation to 48-bit physical addresses to support
 * older kernels that don't handle larger physical address spaces.
 */
#if defined(__aarch64__)
#define	EFI_STAGING_MAX		(1UL << 48)
#define	EFI_ALLOC_METHOD	AllocateMaxAddress
#else
#define	EFI_ALLOC_METHOD	AllocateAnyPages
#endif

int
efi_copy_init(void)
{
	EFI_STATUS	status;
	size_t		pages = STAGE_PAGES;
	EFI_MEMORY_TYPE	alloc_type = EfiLoaderData;

#if defined(__aarch64__)
	/*
	 * For aarch64, allocate staging area with a maximum address limit.
	 * The staging area will be used to hold the kernel which is linked
	 * at a high VA. The stage_offset will translate between VA and PA.
	 */
	staging = EFI_STAGING_MAX;
	status = BS->AllocatePages(EFI_ALLOC_METHOD, EfiLoaderCode,
	    pages, &staging);
	if (EFI_ERROR(status)) {
		printf("failed to allocate %uMB staging area: %lu\n",
		    EFI_STAGING_SIZE, (unsigned long)status);
		printf("retrying with smaller %uMB allocation\n",
		    EFI_STAGING_SIZE/2);
		pages /= 2;
		staging = EFI_STAGING_MAX;
		status = BS->AllocatePages(EFI_ALLOC_METHOD, EfiLoaderCode,
		    pages, &staging);
	}
	if (EFI_ERROR(status)) {
		printf("failed to allocate staging area: %lu\n",
		    (unsigned long)status);
		return (status);
	}
	staging_end = staging + pages * EFI_PAGE_SIZE;

	/*
	 * Round the kernel load address to a 2MiB value. This is needed
	 * because the kernel builds a page table based on where it has
	 * been loaded in physical address space. As the kernel will use
	 * either a 1MiB or 2MiB page for this we need to make sure it
	 * is correctly aligned for both cases.
	 */
	staging = roundup2(staging, 2 * 1024 * 1024);
	return (0);
#endif

	status = BS->AllocatePages(AllocateAnyPages, alloc_type,
	    pages, &staging);
	if (EFI_ERROR(status)) {
		printf("failed to allocate %uMB staging area: %llu\n",
		    EFI_STAGING_SIZE, status);
		printf("retrying with smaller %uMB allocation\n",
		    EFI_STAGING_SIZE/2);
		pages /= 2;
		status = BS->AllocatePages(AllocateAnyPages, alloc_type,
		    pages, &staging);
	}
	if (EFI_ERROR(status)) {
		printf("failed to allocate staging area: %llu\n",
		    status);
		return (status);
	}
	staging_end = staging + pages * EFI_PAGE_SIZE;

	return (0);
}

/*
 * Translate a kernel virtual address to its physical staging location.
 * This is used when the kernel is linked at a high VA but loaded at a
 * lower physical address.
 */
void *
efi_translate(vm_offset_t ptr)
{

	return ((void *)(ptr + stage_offset));
}

ssize_t
efi_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	vm_offset_t phys_dest;

	if (!stage_offset_set) {
		/*
		 * Calculate the offset between staging (physical) and dest (VA).
		 * For a kernel linked at VA 0xffffff8040100000 and staging at
		 * PA 0x40000000, stage_offset will be negative, translating
		 * high VA to low PA.
		 */
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	phys_dest = dest + stage_offset;

	/* XXX: Callers do not check for failure. */
	if (phys_dest + len > staging_end) {
		printf("efi_copyin: BOUNDS FAIL dest=0x%lx phys=0x%lx len=0x%lx end=0x%llx\n",
		    (unsigned long)dest, (unsigned long)phys_dest,
		    (unsigned long)len, (unsigned long long)staging_end);
		errno = ENOMEM;
		return (-1);
	}

	bcopy(src, (void *)phys_dest, len);
	return (len);
}

ssize_t
efi_copyout(const vm_offset_t src, void *dest, const size_t len)
{

	/* XXX: Callers do not check for failure. */
	if (src + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy((void *)(src + stage_offset), dest, len);
	return (len);
}


ssize_t
efi_readin(const int fd, vm_offset_t dest, const size_t len)
{
	vm_offset_t phys_dest;

	if (!stage_offset_set) {
		/*
		 * Calculate the offset between staging (physical) and dest (VA).
		 */
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	phys_dest = dest + stage_offset;

	if (phys_dest + len > staging_end) {
		printf("efi_readin: BOUNDS FAIL dest=0x%lx phys=0x%lx len=0x%lx end=0x%llx\n",
		    (unsigned long)dest, (unsigned long)phys_dest,
		    (unsigned long)len, (unsigned long long)staging_end);
		errno = ENOMEM;
		return (-1);
	}
	return (read(fd, (void *)phys_dest, len));
}

/*
 * Copy kernel from staging area to its final physical location.
 * For aarch64 with high VA kernels, this copies from staging to
 * the physical address derived from the kernel's virtual address.
 *
 * Note: This is called after ExitBootServices, so we cannot use
 * any EFI services here.
 */
void
efi_copy_finish(void)
{
	uint64_t	*src, *dst, *last;

	src = (uint64_t *)staging;
	dst = (uint64_t *)(staging - stage_offset);
	last = (uint64_t *)staging_end;

	while (src < last)
		*dst++ = *src++;
}
