/*	$OpenBSD: hibernate_var.h,v 1.2 2026/09/19 17:27:09 kettenis Exp $ */

/*
 * Copyright (c) 2011 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LOCORE
extern vaddr_t global_piglet_va;
extern paddr_t global_piglet_pa;
#endif

#define PIGLET_PAGE_MASK	(~((1ULL << 21) - 1))

/*
 * On arm64 the resume page tables use both TTBR0 and TTBR1.  The
 * TTBR0 (PT0) page tables start at L0 and cover the first 4MB of the
 * piglet with 4KB pages (such that pages in the piglet can be
 * accessed using their offset) and also covers identy mappings for
 * the pig, the piglet and kernel using 2MB mappings.  The TTBR1 (PT1)
 * page tables covers mappings for the piglet and kernel using normal
 * kernel VAs.
 */
#define HIBERNATE_PT0_L0_PAGE	(PAGE_SIZE * 512)	/* TTBR0 root */
#define HIBERNATE_PT0_L1_LOW	(PAGE_SIZE * 513)
#define HIBERNATE_PT0_L2_LOW	(PAGE_SIZE * 514)
#define HIBERNATE_PT0_L3_LOW	(PAGE_SIZE * 515)	/* 2 pages */
#define HIBERNATE_PT0_L1_HI	(PAGE_SIZE * 517)
#define HIBERNATE_PT1_L1_PAGE	(PAGE_SIZE * 518)	/* TTBR1 root */

/* Pool of L2 pages per distinct PT0_L1_LOW/PT0_L1_HI/PT1_L1 slot */
#define HIBERNATE_PT0_L2_LOW_POOL	(PAGE_SIZE * 520)
#define HIBERNATE_PT0_L2_LOW_POOL_COUNT	16
#define HIBERNATE_PT0_L2_HI_POOL	(PAGE_SIZE * 536)
#define HIBERNATE_PT0_L2_HI_POOL_COUNT	16
#define HIBERNATE_PT1_L2_POOL		(PAGE_SIZE * 552)
#define HIBERNATE_PT1_L2_POOL_COUNT	16

/* 3 pages for stack */
#define HIBERNATE_STACK_PAGE	(PAGE_SIZE * 570)

#define HIBERNATE_INFLATE_PAGE	(PAGE_SIZE * 571)

/*
 * On arm64 we can't access low VA using the normal kernel page
 * tables.  Since the hiballoc page is used before we install the
 * resume page tables, use the normal kernel VA for access.
 */
#define HIBERNATE_HIBALLOC_PAGE	(global_piglet_va + PAGE_SIZE * 572)

/* Use 4MB hibernation chunks */
#define HIBERNATE_CHUNK_SIZE		0x400000

#define HIBERNATE_CHUNK_TABLE_SIZE	0x200000

#define HIBERNATE_STACK_OFFSET		0x0F00

/*
 * Minimum amount of memory for hibernate support. This is used in early boot
 * when deciding if we can preallocate the piglet. If the machine does not
 * have at least HIBERNATE_MIN_MEMORY RAM, we won't support hibernate. This
 * avoids late allocation issues due to fragmented memory and failure to
 * hibernate. We need to be able to allocate 32MB contiguous memory, aligned
 * to 2MB.
 *
 * The default minimum required memory is 512MB (1ULL << 29).
 */
#define HIBERNATE_MIN_MEMORY	(1ULL << 29)
