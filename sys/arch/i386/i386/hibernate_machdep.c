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

#include <dev/ata/atavar.h>
#include <dev/ata/wdvar.h>

#ifdef MULTIPROCESSOR
#include <machine/mpbiosvar.h>
#endif /* MULTIPROCESSOR */

#include "acpi.h"
#include "wd.h"

#ifndef SMALL_KERNEL
/* Hibernate support */
int	hibernate_write_image(void);
int	hibernate_read_image(void);
void	hibernate_unpack_image(void);
void	hibernate_enter_resume_pte(vaddr_t, paddr_t);
void	hibernate_populate_resume_pt(paddr_t *, paddr_t *);
int	get_hibernate_info_md(union hibernate_info *);
int	hibernate_write_signature(void);
int	hibernate_clear_signature(void);
union 	hibernate_info *global_hiber_info;
paddr_t global_image_start;

extern	void hibernate_resume_machine(void);
extern	void hibernate_activate_resume_pt(void);
extern	void hibernate_switch_stack(void);
extern	char *disk_readlabel(struct disklabel *, dev_t, char *, size_t);
extern	caddr_t start, end;
extern	int ndumpmem;
extern  struct dumpmem dumpmem[];


/*
 * i386 MD Hibernate functions
 */

/*
 * get_hibernate_io_function
 *
 * Returns the hibernate write I/O function to use on this machine
 *
 */
void *
get_hibernate_io_function()
{
#if NWD > 0
	/* XXX - Only support wd hibernate presently */
	if (strcmp(findblkname(major(swdevt[0].sw_dev)), "wd") == 0)
		return wd_hibernate_io;
	else
		return NULL;
#else
	return NULL;
#endif
}

/*
 * get_hibernate_info_md
 *
 * Gather MD-specific data and store into hiber_info
 */
int
get_hibernate_info_md(union hibernate_info *hiber_info)
{
	int i;

	/* Calculate memory ranges */
	hiber_info->nranges = ndumpmem;
	hiber_info->image_size = 0;

	for(i=0; i<ndumpmem; i++) {
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
 * Enter a 4MB PTE mapping for the supplied VA/PA
 * into the resume-time page table.
 */
void
hibernate_enter_resume_pte(vaddr_t va, paddr_t pa)
{
	pt_entry_t *pte, npte;

	pte = s4pde_4m(va);
	npte = (pa & PMAP_PA_MASK_4M) | PG_RW | PG_V | PG_U | PG_M | PG_PS;
	*pte = npte;
}

/*
 * Create the resume-time page table. This table maps the image(pig) area,
 * the kernel text area, and various utility pages located in low memory for
 * use during resume, since we cannot overwrite the resuming kernel's 
 * page table and expect things to work properly.
 */
void
hibernate_populate_resume_pt(paddr_t *image_start, paddr_t *image_end)
{
	int phys_page_number;
	paddr_t pa, pig_start, pig_end;
	psize_t pig_sz;
	vaddr_t kern_start_4m_va, kern_end_4m_va, page;

	/* Get the pig (largest contiguous physical range) from uvm */
	if (uvm_pmr_alloc_pig(&pig_start, pig_sz) == ENOMEM)
		panic("Insufficient memory for resume");

	*image_start = pig_start;
	*image_end = pig_end;

	bzero((caddr_t)HIBERNATE_PT_PAGE, PAGE_SIZE);

	/*
	 * Identity map first 4M physical for tramps and special utility 
	 * pages
	 */
	hibernate_enter_resume_pte(0, 0);	
	
	/*
	 * Map current kernel VA range using 4M pages
	 */
	kern_start_4m_va = (paddr_t)&start & ~(PAGE_MASK_4M);
	kern_end_4m_va = (paddr_t)&end & ~(PAGE_MASK_4M);
	phys_page_number = 0; 

	for (page = kern_start_4m_va ; page <= kern_end_4m_va ; 
	    page += NBPD, phys_page_number++) {

		pa = (paddr_t)(phys_page_number * NBPD);
		hibernate_enter_resume_pte(page, pa);
	}

	/*
	 * Identity map the image (pig) area
	 */
	phys_page_number = pig_start / NBPD;
	pig_start &= ~(PAGE_MASK_4M);
	pig_end &= ~(PAGE_MASK_4M);
	for (page = pig_start; page <= pig_end ;
	    page += NBPD, phys_page_number++) {

		pa = (paddr_t)(phys_page_number * NBPD);
		hibernate_enter_resume_pte(page, pa);
	}
}
	
int
hibernate_write_image()
{
	union hibernate_info hiber_info;
	int i, j;
	paddr_t range_base, range_end, addr;
	daddr_t blkctr;

	/* Get current running machine's hibernate info */
	if (get_hibernate_info(&hiber_info))
		return (1);

	pmap_kenter_pa(HIBERNATE_TEMP_PAGE, HIBERNATE_TEMP_PAGE, VM_PROT_ALL);	
	pmap_kenter_pa(HIBERNATE_ALLOC_PAGE, HIBERNATE_ALLOC_PAGE, VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_IO_PAGE, HIBERNATE_IO_PAGE, VM_PROT_ALL);

	blkctr = hiber_info.image_offset;

	for (i=0; i < hiber_info.nranges; i++) {
		range_base = hiber_info.ranges[i].base;
		range_end = hiber_info.ranges[i].end;

		for (j=0; j < (range_end - range_base);
		    blkctr += (NBPG/512), j += NBPG) {
			addr = range_base + j;
			pmap_kenter_pa(HIBERNATE_TEMP_PAGE, addr,
				VM_PROT_ALL);
			bcopy((caddr_t)HIBERNATE_TEMP_PAGE,
				(caddr_t)HIBERNATE_IO_PAGE,
				NBPG);
			hiber_info.io_func(hiber_info.device, blkctr,
				(vaddr_t)HIBERNATE_IO_PAGE, NBPG, 1,
				(void *)HIBERNATE_ALLOC_PAGE);
		}
	}
	
	/* Image write complete, write the signature and return */	
	return hibernate_write_signature();
}

int
hibernate_read_image()
{
	union hibernate_info hiber_info;
	int i, j;
	paddr_t range_base, range_end, addr, image_start, image_end;
	daddr_t blkctr;

	/* Get current running machine's hibernate info */
	if (get_hibernate_info(&hiber_info))
		return (1);

	pmap_kenter_pa(HIBERNATE_TEMP_PAGE, HIBERNATE_TEMP_PAGE, VM_PROT_ALL);	
	pmap_kenter_pa(HIBERNATE_ALLOC_PAGE, HIBERNATE_ALLOC_PAGE, VM_PROT_ALL);

	blkctr = hiber_info.image_offset;

	/* Prepare the resume-time pagetable */
	hibernate_populate_resume_pt(&image_start, &image_end);

	for (i=0; i < hiber_info.nranges; i++) {
		range_base = hiber_info.ranges[i].base;
		range_end = hiber_info.ranges[i].end;

		for (j=0; j < (range_end - range_base)/NBPG;
		    blkctr += (NBPG/512), j += NBPG) {
			addr = range_base + j;
			pmap_kenter_pa(HIBERNATE_TEMP_PAGE, addr,
				VM_PROT_ALL);
			hiber_info.io_func(hiber_info.device, blkctr,
				(vaddr_t)HIBERNATE_IO_PAGE, NBPG, 1,
				(void *)HIBERNATE_ALLOC_PAGE);
			bcopy((caddr_t)HIBERNATE_IO_PAGE,
				(caddr_t)HIBERNATE_TEMP_PAGE,
				NBPG);
		
		}
	}

	/* Read complete, clear the signature and return */
	return hibernate_clear_signature();
}

int
hibernate_suspend()
{
	/*
	 * On i386, the only thing to do on hibernate suspend is
	 * to write the image.
	 */

	return hibernate_write_image();
}

/* Unpack image from resumed image to real location */
void
hibernate_unpack_image()
{
	union hibernate_info *hiber_info = global_hiber_info;
	int i, j;
	paddr_t base, end, pig_base;

	hibernate_activate_resume_pt();

	for (i=0; i<hiber_info->nranges; i++) {
		base = hiber_info->ranges[i].base;
		end = hiber_info->ranges[i].end;
		pig_base = base + global_image_start;

		for (j=base; j< (end - base)/NBPD; j++) {
			hibernate_enter_resume_pte(base, base);
			bcopy((caddr_t)pig_base, (caddr_t)base, NBPD);
		}
	}
}

void
hibernate_resume()
{
	union hibernate_info hiber_info, disk_hiber_info;
	u_int8_t *io_page;
	int s;
	paddr_t image_start, image_end;

	/* Get current running machine's hibernate info */
	if (get_hibernate_info(&hiber_info))
		return;

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return;
	
	/* Read hibernate info from disk */
	s = splbio();
	hiber_info.io_func(hiber_info.device, hiber_info.sig_offset,
		(vaddr_t)&disk_hiber_info, 512, 0, io_page);

	free(io_page, M_DEVBUF);

	if (memcmp(&hiber_info, &disk_hiber_info,
	    sizeof(union hibernate_info)) !=0) {
		return;
	}

	/*
	 * On-disk and in-memory hibernate signatures match,
	 * this means we should do a resume from hibernate.
	 */

	disable_intr();
	cold = 1;

	/*
	 * Add mappings for resume stack and PT page tables
	 * into the "resuming" kernel. We use these mappings
	 * during image read and copy
	 */
	pmap_activate(curproc);
	pmap_kenter_pa((vaddr_t)HIBERNATE_STACK_PAGE,
		(paddr_t)HIBERNATE_STACK_PAGE,
		VM_PROT_ALL);
	pmap_kenter_pa((vaddr_t)HIBERNATE_PT_PAGE,
		(paddr_t)HIBERNATE_PT_PAGE,
		VM_PROT_ALL);

	/*
	 * Create the resume-time page table (ahead of when we actually
	 * need it)
	 */
	hibernate_populate_resume_pt(&image_start, &image_end);
	

	/* 
	 * We can't access any of this function's local variables (via 
	 * stack) after we switch stacks, so we stash hiber_info and
	 * the image start area into temporary global variables first.
	 */
	global_hiber_info = &hiber_info;
	global_image_start = image_start;

	/* Switch stacks */
	hibernate_switch_stack();

	/* Read the image from disk into the image (pig) area */
	if (hibernate_read_image())
		panic("Failed to restore the hibernate image");

	/*
	 * Image is now in high memory (pig area), copy to "correct" 
	 * location in memory. We'll eventually end up copying on top
	 * of ourself, but we are assured the kernel code here is
	 * the same between the hibernated and resuming kernel, 
	 * and we are running on our own stack
	 */
	hibernate_unpack_image();	
	
	/*
	 * Resume the loaded kernel by jumping to the S3 resume vector
	 */
	hibernate_resume_machine();
}

int
hibernate_write_signature()
{
	union hibernate_info hiber_info;
	u_int8_t *io_page;

	/* Get current running machine's hibernate info */
	if (get_hibernate_info(&hiber_info))
		return (1);

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return (1);
	
	/* Write hibernate info to disk */
	hiber_info.io_func(hiber_info.device, hiber_info.sig_offset,
		(vaddr_t)&hiber_info, 512, 1, io_page);

	free(io_page, M_DEVBUF);

	return (0);
}

int
hibernate_clear_signature()
{
	union hibernate_info hiber_info;
	u_int8_t *io_page;

	/* Zero out a blank hiber_info */
	bzero(&hiber_info, sizeof(hiber_info));

	io_page = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (!io_page)
		return (1);
	
	/* Write (zeroed) hibernate info to disk */
	hiber_info.io_func(hiber_info.device, hiber_info.sig_offset,
		(vaddr_t)&hiber_info, 512, 1, io_page);

	free(io_page, M_DEVBUF);

	return (0);
}
#endif /* !SMALL_KERNEL */
