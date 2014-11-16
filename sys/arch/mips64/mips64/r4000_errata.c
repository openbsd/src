/*	$OpenBSD: r4000_errata.c,v 1.5 2014/11/16 12:30:58 deraadt Exp $	*/

/*
 * Copyright (c) 2014 Miodrag Vallat.
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
 * The following routines attempt to workaround the `end-of-page' errata
 * affecting R4000 processors rev < 3.
 *
 * This particular errata, scarcely documented as errata #4 and #14 in the
 * `R4000PC, R4000SC Errata, Processor Revision 2.2 and 3.0' document,
 * is not recoverable.
 *
 *
 * This errata is triggered by branch instructions in the last word of a
 * page, when the next page (containing the delay slot instruction) causes
 * a TLB miss.  The only safe way to avoid it is to have the toolchain
 * force all branch instructions to be aligned on 8-byte boundaries, but
 * that wouldn't prevent a rogue binary (or just-in-time compilation) to
 * fail this requirement.
 *
 * The following logic is a ``best effort'' (well, ok, ``lazy man's effort'')
 * at trying to prevent the errata from triggering. It will not be enough
 * when confronted to a carefully crafted binary (but then, there are easier
 * way to get kernel mode privileges from userland, when running on the R4000
 * processors vulnerable to the end-of-page errata, so why bother?). Yet,
 * experience has shown this code is surprisingly good enough to allow for
 * regular binaries to run, with a minimal performance hit.
 *
 *
 * The idea behind this code is simple:
 * - executable pages are checked - with eop_page_check() - for a branch in
 *   their last word. If they are vulnerable to this errata, page table entries
 *   for these pages get the `special' bit set.
 * - tlb miss handlers will check for the `special' bit set in the pte and
 *   will always defer to the C code in trap() in that case. trap() will
 *   then invoke eop_tlb_miss_handler(), which will 1) force the next page
 *   to be faulted in, and 2) set up wired TLB entries for both the vulnerable
 *   page and the next page (and their neighbors if they do not share the same
 *   TLB pair), so that there is no risk of a TLB miss when the branch
 *   instruction is reached.
 * - context switches will remove these wired entries.
 * - tlb modification handlers will check for the current exception PC, and
 *   will remove the wired entries if the exception PC is no longer in the
 *   vulnerable page.
 *
 *
 * There are a few limitations:
 * - heavy paging may cause the page next to a vulnerable page to be swapped
 *   out (this code does not attempt to wire the vm_page). It would be worth
 *   mapping a page full of special break instructions when the page gets
 *   swapped out.
 * - there might be other vulnerable pages in the wired tlb entries being
 *   set up. It should be simple enough to walk the next pages until the last
 *   would-be-wired TLB pair contains two safe pages.  However, the amount of
 *   TLB is quite limited, so a limit has to be set at some point.
 * - no effort has been put to catch executable pages being temporarily made
 *   writable, then vulnerable (by putting a branch instruction in the last
 *   word). This is unlikely to happen (except for just-in-time compilation).
 *
 *
 * Note that, by using 16KB page sizes, the number of vulnerable pages is
 * reduced.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

int	r4000_errata;

static inline void eop_undo(struct pcb *);

static inline void
eop_undo(struct pcb *pcb)
{
	tlb_set_wired(UPAGES / 2);
	tlb_flush((UPAGES / 2) + pcb->pcb_nwired);
	pcb->pcb_nwired = 0;
}

/*
 * Check for an R4000 end-of-page errata condition in an executable code page.
 * Returns a bitmask to set in the given page pg_flags.
 */
u_int
eop_page_check(paddr_t pa)
{
	uint32_t insn;

	insn = *(uint32_t *)PHYS_TO_XKPHYS(pa + PAGE_SIZE - 4, CCA_CACHED);
	if (classify_insn(insn) != INSNCLASS_NEUTRAL)
		return PGF_EOP_VULN;

	return 0;
}

/*
 * Invalidate a TLB entry. If it is part of a wired pair, drop all wired
 * entries.
 *
 * Note that, in case of heavy swapping, this can cause the page following
 * a vulnerable page to be swapped out and immediately faulted back in,
 * iff the userland pc is in the vulnerable page. Help me, Obi Wan LRU.
 * You are my only hope.
 */
void
eop_tlb_flush_addr(struct pmap *pmap, vaddr_t va, u_long asid)
{
	struct proc *p = curproc;
	struct pcb *pcb;

	if (p->p_vmspace->vm_map.pmap == pmap) {
		pcb = &p->p_addr->u_pcb;
		if (pcb->pcb_nwired != 0 &&
		    (va - pcb->pcb_wiredva) < ptoa(pcb->pcb_nwired * 2)) {
			eop_undo(pcb);
			return;
		}
	}

	tlb_flush_addr(va | asid);
}

/*
 * Handle a TLB miss exception for a page marked as able to trigger the
 * end-of-page errata.
 * Returns nonzero if the exception has been completely serviced, and no
 * further processing in the trap handler is necessary.
 */
int
eop_tlb_miss_handler(struct trap_frame *trapframe, struct cpu_info *ci,
    struct proc *p)
{
	struct pcb *pcb;
	vaddr_t va, faultva;
	struct vmspace *vm;
	vm_map_t map;
	pmap_t pmap;
	pt_entry_t *pte, entry;
	int onfault;
	u_long asid;
	uint i, npairs;
	int64_t tlbidx;

	/*
	 * Check for a valid pte with the `special' bit set (PG_SP)
	 * in order to apply the end-of-page errata workaround.
	 */

	vm = p->p_vmspace;
	map = &vm->vm_map;
	faultva = trunc_page((vaddr_t)trapframe->badvaddr);
	pmap = map->pmap;

	pte = pmap_segmap(pmap, faultva);
	if (pte == NULL)
		return 0;

	pte += uvtopte(faultva);
	entry = *pte;
	if ((entry & PG_SP) == 0)
		return 0;

	pcb = &p->p_addr->u_pcb;
	asid = pmap->pm_asid[ci->ci_cpuid].pma_asid << PG_ASID_SHIFT;

	/*
	 * For now, only allow one EOP vulnerable page to get a wired TLB
	 * entry.  We will aggressively attempt to recycle the wired TLB
	 * entries created for that purpose, as soon as we are no longer
	 * needing the EOP page resident in the TLB.
	 */

	/*
	 * Figure out how many pages to wire in the TLB.
	 */

	if ((faultva & PG_ODDPG) != 0) {
		/* odd page: need two pairs */
		npairs = 2;
	} else {
		/* even page: only need one pair */
		npairs = 1;
	}

	/*
	 * Fault-in the next page.
	 */

	va = faultva + PAGE_SIZE;
	pte = pmap_segmap(pmap, va);
	if (pte != NULL)
		pte += uvtopte(va);

	if (pte == NULL || (*pte & PG_V) == 0) {
		onfault = pcb->pcb_onfault;
		pcb->pcb_onfault = 0;
		KERNEL_LOCK();
		(void)uvm_fault(map, va, 0, PROT_READ | PROT_EXEC);
		KERNEL_UNLOCK();
		pcb->pcb_onfault = onfault;
	}

	/*
	 * Clear possible TLB entries for the pages we're about to wire.
	 */

	for (i = npairs, va = faultva & PG_HVPN; i != 0;
	    i--, va += 2 * PAGE_SIZE) {
		tlbidx = tlb_probe(va | asid);
		if (tlbidx >= 0)
			tlb_update_indexed(CKSEG0_BASE, PG_NV, PG_NV, tlbidx);
	}

	/*
	 * Reserve the extra wired TLB, and fill them with the existing ptes.
	 */

	tlb_set_wired((UPAGES / 2) + npairs);
	for (i = 0, va = faultva & PG_HVPN; i != npairs;
	    i++, va += 2 * PAGE_SIZE) {
		pte = pmap_segmap(pmap, va);
		if (pte == NULL)
			tlb_update_indexed(va | asid,
			    PG_NV, PG_NV, (UPAGES / 2) + i);
		else {
			pte += uvtopte(va);
			tlb_update_indexed(va | asid,
			    pte[0], pte[1], (UPAGES / 2) + i);
		}
	}

	/*
	 * Save the base address of the EOP vulnerable page, to be able to
	 * figure out when the wired entry is no longer necessary.
	 */

	pcb->pcb_nwired = npairs;
	pcb->pcb_wiredva = faultva & PG_HVPN;
	pcb->pcb_wiredpc = faultva;

	return 1;
}

/*
 * Attempt to cleanup the current end-of-page errata workaround, if the
 * current pc is no longer in an errata vulnerable page.
 */
void
eop_cleanup(struct trap_frame *trapframe, struct proc *p)
{
	struct pcb *pcb;

	pcb = &p->p_addr->u_pcb;
	if (pcb->pcb_nwired != 0) {
		if (trunc_page(trapframe->pc) != pcb->pcb_wiredpc)
			eop_undo(pcb);
	}
}
