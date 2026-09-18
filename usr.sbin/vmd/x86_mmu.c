/*	$OpenBSD: x86_mmu.c,v 1.2 2026/09/18 04:25:01 dv Exp $ */
/*
 * Copyright (c) 2024 Mike Larkin <mlarkin@openbsd.org>
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

#include <sys/mman.h>
#include <sys/types.h>

#include <machine/pte.h>
#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include <errno.h>
#include <stdint.h>

#include "vmd.h"
#include "x86_vm.h"

#define	I386_FRAME	0xfffff000ULL

#ifdef DPRINTF
#undef DPRINTF
#endif
#if MMU_DEBUG
#define DPRINTF		log_debug
#else
#define DPRINTF(x...)	do {} while(0)
#endif	/* MMU_DEBUG */

/*
 * translate_gva
 *
 * Translates a guest virtual address to a guest physical address by walking
 * the currently active page table (if needed).
 *
 * XXX ensure translate_gva respects segment base and limits in i386 mode
 * XXX ensure translate_gva respects segment wraparound in i8086 mode
 * XXX ensure translate_gva updates the A bit in the segment selector
 * XXX ensure translate_gva respects CR4.LMSLE if available
 *
 * Parameters:
 *  exit: The VCPU this translation should be performed for (guest MMU settings
 *   are gathered from this VCPU)
 *  va: virtual address to translate
 *  pa: pointer to paddr_t variable that will receive the translated physical
 *   address. 'pa' is unchanged on error.
 *  mode: one of PROT_READ, PROT_WRITE, PROT_EXEC indicating the mode in which
 *   the address should be translated
 *
 * Return values:
 *  0: the address was successfully translated - 'pa' contains the physical
 *     address currently mapped by 'va'.
 *  EFAULT: the PTE for 'VA' is unmapped.
 *  EINVAL: an error occurred reading paging table structures.
 */
int
translate_gva(struct vm_exit *exit, uint64_t va, uint64_t *pa, int mode)
{
	int large_page, level, pae32, shift, pdidx;
	uint64_t pte, pt_paddr, pte_paddr, mask, low_mask, frame_mask;
	uint64_t shift_width, pte_size;
	struct vcpu_reg_state *vrs;

	vrs = &exit->vrs;

	if (pa == NULL)
		return (EINVAL);

	if (!(vrs->vrs_crs[VCPU_REGS_CR0] & CR0_PG)) {
		log_debug("%s: unpaged, va=pa=0x%llx", __func__, va);
		*pa = va;
		return (0);
	}

	pt_paddr = vrs->vrs_crs[VCPU_REGS_CR3];

	DPRINTF("%s: guest %%cr0=0x%llx, %%cr3=0x%llx", __func__,
	    vrs->vrs_crs[VCPU_REGS_CR0], pt_paddr);

	if (!(vrs->vrs_crs[VCPU_REGS_CR0] & CR0_PE))
		return (EINVAL);

	if (vrs->vrs_crs[VCPU_REGS_CR4] & CR4_PAE) {
		pte_size = sizeof(uint64_t);
		frame_mask = PG_FRAME;
		shift_width = 9;

		if (vrs->vrs_msrs[VCPU_REGS_EFER] & EFER_LMA) {
			/* Four-level long-mode paging. */
			pae32 = 0;
			level = 4;
			mask = L4_MASK;
			shift = L4_SHIFT;
			pt_paddr &= PG_FRAME;
		} else {
			/* Three-level 32-bit PAE paging. */
			pae32 = 1;
			level = 3;
			mask = L3_MASK;
			shift = L3_SHIFT;
			pt_paddr &= ~0x1fULL;
		}
	} else {
		/* Two-level 32-bit paging. */
		pae32 = 0;
		pte_size = sizeof(uint32_t);
		frame_mask = I386_FRAME;
		shift_width = 10;
		level = 2;
		mask = 0xffc00000ULL;
		shift = 22;
		pt_paddr &= I386_FRAME;
	}

	for (; level > 0; level--) {
		pdidx = (va & mask) >> shift;
		pte_paddr = pt_paddr + pdidx * pte_size;

		DPRINTF("%s: read pte level %d @ GPA 0x%llx", __func__,
		    level, pte_paddr);

		/* A 32-bit read must not leave stale upper bits in pte */
		pte = 0;
		if (read_mem(pte_paddr, &pte, pte_size)) {
			log_warn("%s: failed to read pte", __func__);
			return (EFAULT);
		}

		DPRINTF("%s: PTE @ 0x%llx = 0x%llx", __func__, pte_paddr,
		    pte);

		if (!(pte & PG_V))
			return (EFAULT);

		large_page = (pte & PG_PS) != 0;
		if (!(pae32 && level == 3)) {
			if (mode == PROT_WRITE && !(pte & PG_RW))
				return (EPERM);
			if (exit->cpl > 0 && !(pte & PG_u))
				return (EPERM);

			/* A/D bits */
			pte |= PG_U;
			if (mode == PROT_WRITE && (level == 1 || large_page))
				pte |= PG_M;
			if (write_mem(pte_paddr, &pte, pte_size)) {
				log_warn("%s: failed to write back flags to pte",
				    __func__);
				return (EIO);
			}
		}

		/* XXX validate CR4.PSE for a non-PAE 4 MiB mapping. */
		if (large_page)
			break;

		if (level > 1) {
			pt_paddr = pte & frame_mask;
			shift -= shift_width;
			mask >>= shift_width;
		}
	}

	low_mask = (1ULL << shift) - 1;
	*pa = (pte & frame_mask & ~low_mask) | (va & low_mask);

	DPRINTF("%s: final GPA for GVA 0x%llx = 0x%llx", __func__, va,
	    *pa);

	return (0);
}
