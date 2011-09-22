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

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/hibernate.h>
#include <sys/timeout.h>
#include <sys/malloc.h>

#include <dev/acpi/acpivar.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_pmemrange.h>

#include <machine/hibernate.h>
#include <machine/hibernate_var.h>
#include <machine/kcore.h>
#include <machine/pmap.h>

#ifdef MULTIPROCESSOR
#include <machine/mpbiosvar.h>
#endif /* MULTIPROCESSOR */

#include "acpi.h"
#include "wd.h"

#if NWD > 0
#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>
#endif

/* Hibernate support */
void    hibernate_enter_resume_4k_pte(vaddr_t, paddr_t);
void    hibernate_enter_resume_4k_pde(vaddr_t);
void    hibernate_enter_resume_4m_pde(vaddr_t, paddr_t);
int	hibernate_read_chunks(union hibernate_info *, paddr_t, paddr_t, size_t);

extern	vaddr_t hibernate_inflate_page;

extern	void hibernate_resume_machdep(void);
extern	void hibernate_flush(void);
extern	caddr_t start, end;
extern	int ndumpmem;
extern  struct dumpmem dumpmem[];
extern	struct hibernate_state *hibernate_state;

/*
 * i386 MD Hibernate functions
 */

/*
 * Returns the hibernate write I/O function to use on this machine
 */
hibio_fn
get_hibernate_io_function(void)
{
#if NWD > 0
	/* XXX - Only support wd hibernate presently */
	if (strcmp(findblkname(major(swdevt[0].sw_dev)), "wd") == 0)
		return wd_hibernate_io;
#endif
	return NULL;
}

/*
 * Gather MD-specific data and store into hiber_info
 */
int
get_hibernate_info_md(union hibernate_info *hiber_info)
{
	int i;

	/* Calculate memory ranges */
	hiber_info->nranges = ndumpmem;
	hiber_info->image_size = 0;

	for(i = 0; i < ndumpmem; i++) {
		hiber_info->ranges[i].base = dumpmem[i].start * PAGE_SIZE;
		hiber_info->ranges[i].end = dumpmem[i].end * PAGE_SIZE;
		hiber_info->image_size += hiber_info->ranges[i].end -
		    hiber_info->ranges[i].base;
	}

#if NACPI > 0
	hiber_info->ranges[hiber_info->nranges].base = ACPI_TRAMPOLINE;
	hiber_info->ranges[hiber_info->nranges].end =
	    hiber_info->ranges[hiber_info->nranges].base + PAGE_SIZE;
	hiber_info->image_size += PAGE_SIZE;
	hiber_info->nranges ++;
#endif
#ifdef MULTIPROCESSOR
	hiber_info->ranges[hiber_info->nranges].base = MP_TRAMPOLINE;
	hiber_info->ranges[hiber_info->nranges].end =
	    hiber_info->ranges[hiber_info->nranges].base + PAGE_SIZE;
	hiber_info->image_size += PAGE_SIZE;
#endif

	return (0);
}

/*
 * Enter a mapping for va->pa in the resume pagetable, using
 * the specified size.
 *
 * size : 0 if a 4KB mapping is desired
 *        1 if a 4MB mapping is desired
 */
void
hibernate_enter_resume_mapping(vaddr_t va, paddr_t pa, int size)
{
	if (size)
		return hibernate_enter_resume_4m_pde(va, pa);
	else
		return hibernate_enter_resume_4k_pte(va, pa);
}

/*
 * Enter a 4MB PDE mapping for the supplied VA/PA into the resume-time pmap
 */
void
hibernate_enter_resume_4m_pde(vaddr_t va, paddr_t pa)
{
	pt_entry_t *pde, npde;

	pde = s4pde_4m(va);
	npde = (pa & PMAP_PA_MASK_4M) | PG_RW | PG_V | PG_u | PG_M | PG_PS;
	*pde = npde;
}

/*
 * Enter a 4KB PTE mapping for the supplied VA/PA into the resume-time pmap.
 */
void
hibernate_enter_resume_4k_pte(vaddr_t va, paddr_t pa)
{
	pt_entry_t *pte, npte;

	pte = s4pte_4k(va);
	npte = (pa & PMAP_PA_MASK) | PG_RW | PG_V | PG_u | PG_M;
	*pte = npte;
}

/*
 * Enter a 4KB PDE mapping for the supplied VA into the resume-time pmap.
 */
void
hibernate_enter_resume_4k_pde(vaddr_t va)
{
	pt_entry_t *pde, npde;

	pde = s4pde_4k(va);
	npde = (HIBERNATE_PT_PAGE & PMAP_PA_MASK) | PG_RW | PG_V | PG_u | PG_M;
	*pde = npde;
}

/*
 * Create the resume-time page table. This table maps the image(pig) area,
 * the kernel text area, and various utility pages for use during resume,
 * since we cannot overwrite the resuming kernel's page table during inflate
 * and expect things to work properly.
 */
void
hibernate_populate_resume_pt(union hibernate_info *hib_info,
    paddr_t image_start, paddr_t image_end)
{
	int phys_page_number, i;
	paddr_t pa, piglet_start, piglet_end;
	vaddr_t kern_start_4m_va, kern_end_4m_va, page;

	/* Identity map PD, PT, and stack pages */
	pmap_kenter_pa(HIBERNATE_PT_PAGE, HIBERNATE_PT_PAGE, VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_PD_PAGE, HIBERNATE_PD_PAGE, VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_STACK_PAGE, HIBERNATE_STACK_PAGE, VM_PROT_ALL);
	pmap_activate(curproc);

	hibernate_inflate_page = HIBERNATE_INFLATE_PAGE;

	bzero((caddr_t)HIBERNATE_PT_PAGE, PAGE_SIZE);
	bzero((caddr_t)HIBERNATE_PD_PAGE, PAGE_SIZE);
	bzero((caddr_t)HIBERNATE_STACK_PAGE, PAGE_SIZE);

	/* PDE for low pages */
	hibernate_enter_resume_4k_pde(0);

	/*
	 * Identity map first 640KB physical for tramps and special utility
	 * pages using 4KB mappings
	 */
	for (i = 0; i < 160; i ++) {
		hibernate_enter_resume_mapping(i*PAGE_SIZE, i*PAGE_SIZE, 0);
	}

	/*
	 * Map current kernel VA range using 4M pages
	 */
	kern_start_4m_va = (paddr_t)&start & ~(PAGE_MASK_4M);
	kern_end_4m_va = (paddr_t)&end & ~(PAGE_MASK_4M);
	phys_page_number = 0;

	for (page = kern_start_4m_va; page <= kern_end_4m_va;
	    page += NBPD, phys_page_number++) {
		pa = (paddr_t)(phys_page_number * NBPD);
		hibernate_enter_resume_mapping(page, pa, 1);
	}

	/*
	 * Identity map the image (pig) area
	 */
	phys_page_number = image_start / NBPD;
	image_start &= ~(PAGE_MASK_4M);
	image_end &= ~(PAGE_MASK_4M);
	for (page = image_start; page <= image_end ;
	    page += NBPD, phys_page_number++) {
		pa = (paddr_t)(phys_page_number * NBPD);
		hibernate_enter_resume_mapping(page, pa, 1);
	}

	/*
	 * Map the piglet
	 */
	phys_page_number = hib_info->piglet_pa / NBPD;
	piglet_start = hib_info->piglet_va;
	piglet_end = piglet_start + HIBERNATE_CHUNK_SIZE * 3;
	piglet_start &= ~(PAGE_MASK_4M);
	piglet_end &= ~(PAGE_MASK_4M);
	for (page = piglet_start; page <= piglet_end ;
	    page += NBPD, phys_page_number++) {
		pa = (paddr_t)(phys_page_number * NBPD);
		hibernate_enter_resume_mapping(page, pa, 1);
	}
}

/*
 * MD-specific resume preparation (creating resume time pagetables,
 * stacks, etc).
 *
 * On i386, we use the piglet whose address is contained in hib_info
 * as per the following layout:
 *
 * offset from piglet base      use
 * -----------------------      --------------------
 * 0                            i/o allocation area
 * PAGE_SIZE                    i/o read area
 * 2*PAGE_SIZE                  temp/scratch page
 * 5*PAGE_SIZE			resume stack
 * 6*PAGE_SIZE                  hiballoc arena
 * 7*PAGE_SIZE to 87*PAGE_SIZE  zlib inflate area
 * ...
 * HIBERNATE_CHUNK_SIZE         chunk table
 * 2*HIBERNATE_CHUNK_SIZE	bounce/copy area
 */
void
hibernate_prepare_resume_machdep(union hibernate_info *hib_info)
{
	paddr_t pa, piglet_end;
	vaddr_t va;

	/*
	 * At this point, we are sure that the piglet's phys
	 * space is going to have been unused by the suspending
	 * kernel, but the vaddrs used by the suspending kernel
	 * may or may not be available to us here in the
	 * resuming kernel, so we allocate a new range of VAs
	 * for the piglet. Those VAs will be temporary and will
	 * cease to exist as soon as we switch to the resume
	 * PT, so we need to ensure that any VAs required during
	 * inflate are also entered into that map.
	 */

        hib_info->piglet_va = (vaddr_t)km_alloc(HIBERNATE_CHUNK_SIZE*3,
	    &kv_any, &kp_none, &kd_nowait);
        if (!hib_info->piglet_va)
                panic("Unable to allocate vaddr for hibernate resume piglet\n");

	piglet_end = hib_info->piglet_pa + HIBERNATE_CHUNK_SIZE*3;

	for (pa = hib_info->piglet_pa,va = hib_info->piglet_va;
	    pa <= piglet_end; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_kenter_pa(va, pa, VM_PROT_ALL);

	pmap_activate(curproc);
}

/*
 * During inflate, certain pages that contain our bookkeeping information
 * (eg, the chunk table, scratch pages, etc) need to be skipped over and
 * not inflated into.
 *
 * Returns 1 if the physical page at dest should be skipped, 0 otherwise
 */
int
hibernate_inflate_skip(union hibernate_info *hib_info, paddr_t dest)
{
	if (dest >= hib_info->piglet_pa &&
	    dest <= (hib_info->piglet_pa + 3 * HIBERNATE_CHUNK_SIZE))
		return (1);

	return (0);
}
