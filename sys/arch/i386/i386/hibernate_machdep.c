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
void    hibernate_enter_resume_4k_pte(vaddr_t, paddr_t);
void    hibernate_enter_resume_4k_pde(vaddr_t);
void    hibernate_enter_resume_4m_pde(vaddr_t, paddr_t);
void	hibernate_populate_resume_pt(paddr_t, paddr_t);
int	get_hibernate_info_md(union hibernate_info *);

union 	hibernate_info *global_hiber_info;
paddr_t global_image_start;

extern	void hibernate_resume_machine(void);
extern	void hibernate_activate_resume_pt(void);
extern	void hibernate_switch_stack(void);
extern	void hibernate_flush(void);
extern	char *disk_readlabel(struct disklabel *, dev_t, char *, size_t);
extern	caddr_t start, end;
extern	int ndumpmem;
extern  struct dumpmem dumpmem[];
extern	struct hibernate_state *hibernate_state;

/*
 * i386 MD Hibernate functions
 */

/*
 * hibernate_zlib_reset
 *
 * Reset the zlib stream state and allocate a new hiballoc area for either
 * inflate or deflate. This function is called once for each hibernate chunk
 * Calling hiballoc_init multiple times is acceptable since the memory it is
 * provided is unmanaged memory (stolen).
 *
 */
int
hibernate_zlib_reset(int deflate)
{
	hibernate_state = (struct hibernate_state *)HIBERNATE_ZLIB_SCRATCH;

	bzero((caddr_t)HIBERNATE_ZLIB_START, HIBERNATE_ZLIB_SIZE);
	bzero((caddr_t)HIBERNATE_ZLIB_SCRATCH, PAGE_SIZE);

	/* Set up stream structure */
	hibernate_state->hib_stream.zalloc = (alloc_func)hibernate_zlib_alloc;
	hibernate_state->hib_stream.zfree = (free_func)hibernate_zlib_free;

	/* Initialize the hiballoc arena for zlib allocs/frees */
	hiballoc_init(&hibernate_state->hiballoc_arena,
		(caddr_t)HIBERNATE_ZLIB_START, HIBERNATE_ZLIB_SIZE);

	if (deflate) {
		return deflateInit(&hibernate_state->hib_stream,
			Z_DEFAULT_COMPRESSION);
	}
	else
		return inflateInit(&hibernate_state->hib_stream);
}

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
 * Enter a mapping for va->pa in the resume pagetable, using
 * the specified size hint.
 *
 * hint : 0 if a 4KB mapping is desired
 *        1 if a 4MB mapping is desired
 */
void
hibernate_enter_resume_mapping(vaddr_t va, paddr_t pa, int hint)
{
	if (hint)
		return hibernate_enter_resume_4m_pde(va, pa);
	else {
		hibernate_enter_resume_4k_pde(va);
		return hibernate_enter_resume_4k_pte(va, pa);			
	}		
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
 * the kernel text area, and various utility pages located in low memory for
 * use during resume, since we cannot overwrite the resuming kernel's 
 * page table during inflate and expect things to work properly.
 */
void
hibernate_populate_resume_pt(paddr_t image_start, paddr_t image_end)
{
	int phys_page_number, i;
	paddr_t pa;
	vaddr_t kern_start_4m_va, kern_end_4m_va, page;

	bzero((caddr_t)HIBERNATE_PT_PAGE, PAGE_SIZE);
	bzero((caddr_t)HIBERNATE_PD_PAGE, PAGE_SIZE);

	/*
	 * Identity map first 640KB physical for tramps and special utility 
	 * pages using 4KB mappings
	 */
	for (i=0; i < 160; i ++) {
		hibernate_enter_resume_mapping(i*PAGE_SIZE, i*PAGE_SIZE, 0);
	}

	/*
	 * Identity map chunk table
	 */
	for (i=0; i < HIBERNATE_CHUNK_TABLE_SIZE/PAGE_SIZE; i ++) {
		hibernate_enter_resume_mapping(
			(vaddr_t)HIBERNATE_CHUNK_TABLE_START + (i*PAGE_SIZE),
			HIBERNATE_CHUNK_TABLE_START + (i*PAGE_SIZE), 0);
	}
	
	/*
	 * Map current kernel VA range using 4M pages
	 */
	kern_start_4m_va = (paddr_t)&start & ~(PAGE_MASK_4M);
	kern_end_4m_va = (paddr_t)&end & ~(PAGE_MASK_4M);
	phys_page_number = 0; 

	for (page = kern_start_4m_va ; page <= kern_end_4m_va ; 
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
}

/*
 * hibernate_write_image
 *
 * Write a compressed version of this machine's memory to disk, at the
 * precalculated swap offset:
 *
 * end of swap - signature block size - chunk table size - memory size
 *
 * The function begins by mapping various pages into the suspending
 * kernel's pmap, then loops through each phys mem range, cutting each
 * one into 4MB chunks. These chunks are then compressed individually
 * and written out to disk, in phys mem order. Some chunks might compress
 * more than others, and for this reason, each chunk's size is recorded
 * in the chunk table, which is written to disk after the image has
 * properly been compressed and written.
 *
 * When this function is called, the machine is nearly suspended - most
 * devices are quiesced/suspended, interrupts are off, and cold has
 * been set. This means that there can be no side effects once the
 * write has started, and the write function itself can also have no
 * side effects.
int
hibernate_write_image()
{
	union hibernate_info hiber_info;
	paddr_t range_base, range_end, inaddr, temp_inaddr;
	vaddr_t zlib_range;
	daddr_t blkctr;
	int i;
	size_t nblocks, out_remaining, used, offset;
	struct hibernate_disk_chunk *chunks;

	/* Get current running machine's hibernate info */
	bzero(&hiber_info, sizeof(hiber_info));
	if (get_hibernate_info(&hiber_info))
		return (1);

	zlib_range = HIBERNATE_ZLIB_START;

	/* Map utility pages */
	pmap_kenter_pa(HIBERNATE_ALLOC_PAGE, HIBERNATE_ALLOC_PAGE,
		VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_IO_PAGE, HIBERNATE_IO_PAGE, VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_TEMP_PAGE, HIBERNATE_TEMP_PAGE,
		VM_PROT_ALL);
	pmap_kenter_pa(HIBERNATE_ZLIB_SCRATCH, HIBERNATE_ZLIB_SCRATCH, VM_PROT_ALL);
	
	/* Map the zlib allocation ranges */
	for(zlib_range = HIBERNATE_ZLIB_START; zlib < HIBERNATE_ZLIB_END;
		zlib_range += PAGE_SIZE) {
		pmap_kenter_pa((vaddr_t)(zlib_range+i),
			(paddr_t)(zlib_range+i),
			VM_PROT_ALL);
	}

	/* Identity map the chunktable */
	for(i=0; i < HIBERNATE_CHUNK_TABLE_SIZE; i += PAGE_SIZE) {
		pmap_kenter_pa((vaddr_t)(HIBERNATE_CHUNK_TABLE+i),
			(paddr_t)(HIBERNATE_CHUNK_TABLE+i),
			VM_PROT_ALL);
	}

	pmap_activate(curproc);

	blkctr = hiber_info.image_offset;
	hiber_info.chunk_ctr = 0;
	offset = 0;
	chunks = (struct hibernate_disk_chunk *)HIBERNATE_CHUNK_TABLE;

	/* Calculate the chunk regions */
	for (i=0; i < hiber_info.nranges; i++) {
                range_base = hiber_info.ranges[i].base;
                range_end = hiber_info.ranges[i].end;

		inaddr = range_base;

		while (inaddr < range_end) {
			chunks[hiber_info.chunk_ctr].base = inaddr;
			if (inaddr + HIBERNATE_CHUNK_SIZE < range_end)
				chunks[hiber_info.chunk_ctr].end = inaddr +
					HIBERNATE_CHUNK_SIZE;
			else
				chunks[hiber_info.chunk_ctr].end = range_end;

			inaddr += HIBERNATE_CHUNK_SIZE;
			hiber_info.chunk_ctr ++;
		}
	} 

	/* Compress and write the chunks in the chunktable */
	for (i=0; i < hiber_info.chunk_ctr; i++) {
		range_base = chunks[i].base;
		range_end = chunks[i].end;

		chunks[i].offset = blkctr; 

		/* Reset zlib for deflate */
		if (hibernate_zlib_reset(1) != Z_OK)
			return (1);

		inaddr = range_base;

		/*
		 * For each range, loop through its phys mem region
		 * and write out 4MB chunks (the last chunk might be
		 * smaller than 4MB).
		 */
		while (inaddr < range_end) {
			out_remaining = PAGE_SIZE;
			while (out_remaining > 0 && inaddr < range_end) {
				pmap_kenter_pa(HIBERNATE_TEMP_PAGE2,
					inaddr & PMAP_PA_MASK, VM_PROT_ALL);
				pmap_activate(curproc);
				bcopy((caddr_t)HIBERNATE_TEMP_PAGE2,
					(caddr_t)HIBERNATE_TEMP_PAGE, PAGE_SIZE);

				/* Adjust for non page-sized regions */
				temp_inaddr = (inaddr & PAGE_MASK) +
					HIBERNATE_TEMP_PAGE;

				/* Deflate from temp_inaddr to IO page */
				inaddr += hibernate_deflate(temp_inaddr,
					&out_remaining);
			}

			if (out_remaining == 0) {
				/* Filled up the page */
				nblocks = PAGE_SIZE / hiber_info.secsize;

				if(hiber_info.io_func(hiber_info.device, blkctr,
					(vaddr_t)HIBERNATE_IO_PAGE, PAGE_SIZE,
					1, (void *)HIBERNATE_ALLOC_PAGE))
						return (1);

				blkctr += nblocks;
			}
			
		}

		if (inaddr != range_end)
			return (1);

		/*
		 * End of range. Round up to next secsize bytes
		 * after finishing compress
		 */
		if (out_remaining == 0)
			out_remaining = PAGE_SIZE;

		/* Finish compress */
		hibernate_state->hib_stream.avail_in = 0;
		hibernate_state->hib_stream.avail_out = out_remaining;
		hibernate_state->hib_stream.next_in = (caddr_t)inaddr;
		hibernate_state->hib_stream.next_out = 
			(caddr_t)HIBERNATE_IO_PAGE + (PAGE_SIZE - out_remaining);

		if (deflate(&hibernate_state->hib_stream, Z_FINISH) !=
			Z_STREAM_END)
				return (1);

		out_remaining = hibernate_state->hib_stream.avail_out;

		used = PAGE_SIZE - out_remaining;
		nblocks = used / hiber_info.secsize;

		/* Round up to next block if needed */
		if (used % hiber_info.secsize != 0)
			nblocks ++;

		/* Write final block(s) for this chunk */
		if( hiber_info.io_func(hiber_info.device, blkctr,
			(vaddr_t)HIBERNATE_IO_PAGE, nblocks*hiber_info.secsize,
			1, (void *)HIBERNATE_ALLOC_PAGE))
				return (1);

		blkctr += nblocks;

		offset = blkctr;
		chunks[i].compressed_size=
			(offset-chunks[i].offset)*hiber_info.secsize;
	}

	/* Image write complete, write the signature+chunk table and return */
	return hibernate_write_signature(&hiber_info);
}

int
hibernate_read_image()
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

	blkctr = hiber_info.image_offset;

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
	 * to zero all the unused pages and write the image.
	 */

	uvm_pmr_zero_everything();

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
			hibernate_enter_resume_mapping(base, base, 0);
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
	paddr_t image_start;

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

/*
 * hibernate_inflate_skip
 *
 * During inflate, certain pages that contain our bookkeeping information
 * (eg, the chunk table, scratch pages, etc) need to be skipped over and
 * not inflated into.
 *
 * Returns 1 if the physical page at dest should be skipped, 0 otherwise
 */
int
hibernate_inflate_skip(paddr_t dest)
{
	/* Chunk Table */
	if (dest >= HIBERNATE_CHUNK_TABLE_START && 
	    dest <= HIBERNATE_CHUNK_TABLE_END)
		return (1);

	/* Contiguous utility pages */
	if (dest >= HIBERNATE_STACK_PAGE &&
	    dest <= HIBERNATE_CHUNKS_PAGE)
		return (1);

	/* libz hiballoc arena */
	if (dest >= HIBERNATE_ZLIB_SCRATCH &&
	    dest <= HIBERNATE_ZLIB_END)
		return (1);

	return (0);
} 
#endif /* !SMALL_KERNEL */
