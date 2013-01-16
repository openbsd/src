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

/* amd64 hibernate support definitions */

#define PAGE_MASK_2M (NBPD_L2 - 1)
#define PAGE_MASK_1G (NBPD_L3 - 1)
#define PAGE_MASK_512G (NBPD_L4 - 1)

#define PIGLET_PAGE_MASK ~((paddr_t)PAGE_MASK_2M)

/*
 * PML4 table for resume
 */
#define HIBERNATE_PML4T		(PAGE_SIZE * 5)

/*
 * amd64 uses a PDPT to map the first 512GB phys mem plus one more
 * to map any ranges of phys mem past 512GB (if needed)
 */
#define HIBERNATE_PDPT_LOW	(PAGE_SIZE * 6)
#define HIBERNATE_PDPT_HI	(PAGE_SIZE * 7)

/*
 * amd64 uses one PD to map the first 1GB phys mem plus one more to map any
 * other 1GB ranges within the first 512GB phys, plus one more to map any
 * 1GB range in any subsequent 512GB range
 */
#define HIBERNATE_PD_LOW	(PAGE_SIZE * 8)
#define HIBERNATE_PD_LOW2	(PAGE_SIZE * 9)
#define HIBERNATE_PD_HI		(PAGE_SIZE * 10)

/*
 * amd64 uses one PT to map the first 2MB phys mem plus one more to map any
 * other 2MB range within the first 1GB, plus one more to map any 2MB range
 * in any subsequent 512GB range.
 */
#define HIBERNATE_PT_LOW	(PAGE_SIZE * 11)
#define HIBERNATE_PT_LOW2	(PAGE_SIZE * 12)
#define HIBERNATE_PT_HI		(PAGE_SIZE * 13)

#define HIBERNATE_SELTABLE	(PAGE_SIZE * 14)

/* 3 pages for stack */
#define HIBERNATE_STACK_PAGE	(PAGE_SIZE * 15)

#define HIBERNATE_INFLATE_PAGE	(PAGE_SIZE * 18)
#define HIBERNATE_COPY_PAGE	(PAGE_SIZE * 19)
/* HIBERNATE_HIBALLOC_PAGE must be the last stolen page (see machdep.c) */
#define HIBERNATE_HIBALLOC_PAGE (PAGE_SIZE * 20)

/* Use 4MB hibernation chunks */
#define HIBERNATE_CHUNK_SIZE		0x400000

#define HIBERNATE_CHUNK_TABLE_SIZE	0x100000

#define HIBERNATE_STACK_OFFSET	0x0F00
