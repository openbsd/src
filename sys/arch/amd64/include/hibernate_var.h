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
#define PMAP_PA_MASK_2M ~((paddr_t)PAGE_MASK_2M)

#define PAGE_MASK_1G (NBPD_L3 - 1)
#define PMAP_PA_MASK_1G ~((paddr_t)PAGE_MASK_1G)

#define PAGE_MASK_512G (NBPD_L4 - 1)
#define PMAP_PA_MASK_512G ~((paddr_t)PAGE_MASK_512G)

#define HIBERNATE_512GB ((paddr_t)1 << (paddr_t)39)
#define HIBERNATE_1GB ((paddr_t)1 << (paddr_t)30)

#define PIGLET_PAGE_MASK ~((paddr_t)PAGE_MASK_2M)

/*
 * amd64 uses a fixed PML4E to map the first 512GB phys mem plus one more
 * to map any ranges of phys mem past 512GB (if needed)
 */
#define HIBERNATE_PML4_PAGE	(PAGE_SIZE * 5)
#define HIBERNATE_PML4E_LOW	(PAGE_SIZE * 6)
#define HIBERNATE_PML4E_HI	(PAGE_SIZE * 7)

/*
 * amd64 uses one fixed PDPTE to map the first 1GB phys mem plus one more
 * to map any other 1GB ranges within the first 512GB phys, plus one more to
 * map any 1GB range in any subsequent 512GB range
 */
#define HIBERNATE_PDPTE_LOW	(PAGE_SIZE * 8)
#define HIBERNATE_PDPTE_LOW2	(PAGE_SIZE * 9)
#define HIBERNATE_PDPTE_HI	(PAGE_SIZE * 10)

/*
 * amd64 uses one fixed PDE to map the first 2MB phys mem plus one more
 * to map any other 2MB range within the first 1GB, plus one more to map any
 * 2MB range in any subsequent 512GB range. These PDEs point to 512 PTEs each
 * (4KB pages) or may directly map a 2MB range
 */
#define HIBERNATE_PDE_LOW	(PAGE_SIZE * 11)
#define HIBERNATE_PDE_LOW2	(PAGE_SIZE * 12)
#define HIBERNATE_PDE_HI	(PAGE_SIZE * 13)

#define HIBERNATE_STACK_PAGE	(PAGE_SIZE * 14)
#define HIBERNATE_INFLATE_PAGE	(PAGE_SIZE * 15)
#define HIBERNATE_COPY_PAGE	(PAGE_SIZE * 16)
/* HIBERNATE_HIBALLOC_PAGE must be the last stolen page (see machdep.c) */
#define HIBERNATE_HIBALLOC_PAGE (PAGE_SIZE * 17)

/* Use 4MB hibernation chunks */
#define HIBERNATE_CHUNK_SIZE		0x400000

#define HIBERNATE_CHUNK_TABLE_SIZE	0x100000

#define HIBERNATE_STACK_OFFSET	0x0F00

#define atop_4k(x) ((x) >> L1_SHIFT)
#define atop_2m(x) ((x) >> L2_SHIFT)
#define atop_1g(x) ((x) >> L3_SHIFT)
#define atop_512g(x) ((x) >> L4_SHIFT)

#define s4pte_4k_low(va) ((pt_entry_t *)HIBERNATE_PDE_LOW + atop_4k(va))
#define s4pte_4k_low2(va) ((pt_entry_t *)HIBERNATE_PDE_LOW2 + atop_4k(va))
#define s4pte_4k_hi(va) ((pt_entry_t *)HIBERNATE_PDE_HI + atop_4k(va))

#define s4pde_2m_low(va) ((pt_entry_t *)HIBERNATE_PDPTE_LOW + atop_2m(va))
#define s4pde_2m_low2(va) ((pt_entry_t *)HIBERNATE_PDPTE_LOW2 + atop_2m(va))
#define s4pde_2m_hi(va) ((pt_entry_t *)HIBERNATE_PDPTE_HI + atop_2m(va))
#define s4pde_1g_low(va) ((pt_entry_t *)HIBERNATE_PML4E_LOW + atop_1g(va))
#define s4pde_1g_low2(va) ((pt_entry_t *)HIBERNATE_PML4E_LOW + atop_1g(va))
#define s4pde_1g_hi(va) ((pt_entry_t *)HIBERNATE_PML4E_HI + atop_1g(va))
#define s4pde_512g(va) ((pt_entry_t *)HIBERNATE_PML4_PAGE + atop_512g(va))
