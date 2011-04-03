/*	$OpenBSD: uvm_pmemrange.h,v 1.7 2011/04/03 22:07:37 ariane Exp $	*/

/*
 * Copyright (c) 2009 Ariane van der Steldt <ariane@stack.nl>
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

/*
 * uvm_pmemrange.h: describe and manage free physical memory.
 */

#ifndef _UVM_UVM_PMEMRANGE_H_
#define _UVM_UVM_PMEMRANGE_H_

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

RB_HEAD(uvm_pmr_addr, vm_page);
RB_HEAD(uvm_pmr_size, vm_page);

/*
 * Page types available:
 * - DIRTY: this page may contain random data.
 * - ZERO: this page has been zeroed.
 */
#define UVM_PMR_MEMTYPE_DIRTY	0
#define UVM_PMR_MEMTYPE_ZERO	1
#define UVM_PMR_MEMTYPE_MAX	2

/*
 * An address range of memory.
 */
struct uvm_pmemrange {
	struct	uvm_pmr_addr addr;	/* Free page chunks, sorted by addr. */
	struct	uvm_pmr_size size[UVM_PMR_MEMTYPE_MAX];
					/* Free page chunks, sorted by size. */
	TAILQ_HEAD(, vm_page) single[UVM_PMR_MEMTYPE_MAX];
					/* single page regions (uses pageq) */

	paddr_t	low;			/* Start of address range (pgno). */
	paddr_t	high;			/* End +1 (pgno). */
	int	use;			/* Use counter. */
	psize_t	nsegs;			/* Current range count. */

	TAILQ_ENTRY(uvm_pmemrange) pmr_use;
					/* pmr, sorted by use */
	RB_ENTRY(uvm_pmemrange) pmr_addr;
					/* pmr, sorted by address */
};

RB_HEAD(uvm_pmemrange_addr, uvm_pmemrange);
TAILQ_HEAD(uvm_pmemrange_use, uvm_pmemrange);

/*
 * pmr control structure. Contained in uvm.pmr_control.
 */
struct uvm_pmr_control {
	struct	uvm_pmemrange_addr addr;
	struct	uvm_pmemrange_use use;
};

void	uvm_pmr_freepages(struct vm_page *, psize_t);
void	uvm_pmr_freepageq(struct pglist *pgl);
int	uvm_pmr_getpages(psize_t, paddr_t, paddr_t, paddr_t, paddr_t,
	    int, int, struct pglist *);
void	uvm_pmr_init(void);

#if defined(DDB) || defined(DEBUG)
int	uvm_pmr_isfree(struct vm_page *pg);
#endif

#ifndef SMALL_KERNEL
void	uvm_pmr_zero_everything(void);
int	uvm_pmr_alloc_pig(paddr_t*, psize_t*);
#endif /* SMALL_KERNEL */

#endif /* _UVM_UVM_PMEMRANGE_H_ */
