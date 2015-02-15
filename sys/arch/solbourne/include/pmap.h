/*	$OpenBSD: pmap.h,v 1.10 2015/02/15 21:34:33 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#include <machine/pte.h>

/*
 * PMAP structure
 */
struct pmap {
	pd_entry_t		*pm_segtab;	/* first level table */
	paddr_t			pm_psegtab;	/* pa of above */

	int			pm_refcount;	/* reference count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
};

typedef struct pmap *pmap_t;

/*
 * Extra constants passed in the low bits of pa in pmap_enter() to
 * request specific memory attributes.
 */

#define	PMAP_NC		1
#define	PMAP_OBIO	PMAP_NC
#define	PMAP_BWS	2

/*
 * Macro to pass iospace bits in the low bits of pa in pmap_enter().
 * Provided for source code compatibility - we don't need such bits.
 */

#define	PMAP_IOENC(x)	0

#ifdef _KERNEL

extern struct pmap kernel_pmap_store;

#define	kvm_recache(addr, npages) 	kvm_setcache(addr, npages, 1)
#define	kvm_uncache(addr, npages) 	kvm_setcache(addr, npages, 0)
#define	pmap_copy(a,b,c,d,e)		do { /* nothing */ } while (0)
#define	pmap_deactivate(p)		do { /* nothing */ } while (0)
#define	pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(p)		((p)->pm_stats.resident_count)
#define	pmap_update(p)			do { /* nothing */ } while (0)
#define	pmap_wired_count(p)		((p)->pm_stats.wired_count)
#define	pmap_remove_holes(vm)		do { /* nothing */ } while (0)

#define PMAP_PREFER(fo, ap)		pmap_prefer((fo), (ap))

struct proc;
void		kvm_setcache(caddr_t, int, int);
void		switchexit(struct proc *);		/* locore.s */
void		pmap_bootstrap(size_t);
void		pmap_cache_enable(void);
void		pmap_changeprot(pmap_t, vaddr_t, vm_prot_t, int);
vaddr_t		pmap_map(vaddr_t, paddr_t, paddr_t, int);
int		pmap_pa_exists(paddr_t);
vaddr_t		pmap_prefer(vaddr_t, vaddr_t);
void		pmap_release(pmap_t);
void		pmap_redzone(void);
void		pmap_virtual_space(vaddr_t *, vaddr_t *);
void		pmap_writetext(unsigned char *, int);

#endif /* _KERNEL */

struct pvlist {
	struct		pvlist *pv_next;	/* next pvlist, if any */
	struct		pmap *pv_pmap;		/* pmap of this va */
	vaddr_t		pv_va;			/* virtual address */
	int		pv_flags;		/* flags (below) */
};

struct vm_page_md {
	struct pvlist pv_head;
};

#define VM_MDPAGE_INIT(pg) do {			\
	(pg)->mdpage.pv_head.pv_next = NULL;	\
	(pg)->mdpage.pv_head.pv_pmap = NULL;	\
	(pg)->mdpage.pv_head.pv_va = 0;		\
	(pg)->mdpage.pv_head.pv_flags = 0;	\
} while (0)

#endif /* _MACHINE_PMAP_H_ */
