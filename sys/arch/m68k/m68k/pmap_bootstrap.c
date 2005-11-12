/*	$OpenBSD: pmap_bootstrap.c,v 1.14 2005/11/12 23:11:37 miod Exp $	*/

/* 
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1999 Steve Murphree, Jr. (68060 support)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)pmap_bootstrap.c	8.1 (Berkeley) 6/10/93
 */

/*
 * NOTICE: This is not a standalone file.  To use it, #include it in
 * your port's pmap_bootstrap.c, like so:
 *
 * #include <m68k/m68k/pmap_bootstrap.c>
 *
 * after having defined the following macros:
 * RELOC		relocate a variable
 * PA2VA		simple crude mapping for bootstraping
 * PMAP_MD_LOCALS	local variable declaration
 * PMAP_MD_RELOC1()	early variable relocation
 * PMAP_MD_RELOC2()	internal IO space variable relocation
 * PMAP_MD_RELOC3()	general purpose kernel virtual addresses relocation
 * PMAP_MD_MAPIOSPACE()	setup machine-specific internal iospace components
 * PMAP_MD_MEMSIZE()	compute avail_end
 * PMAP_MD_RWLOW	number of pages to keep writeable, starting at address 0
 */

extern char *etext;
extern int Sysptsize;
extern char *proc0paddr;
extern st_entry_t *Sysseg;
extern pt_entry_t *Sysptmap, *Sysmap;

extern int physmem;
extern paddr_t avail_start, avail_end;
extern vaddr_t virtual_avail, virtual_end;
extern vsize_t mem_size;
#ifdef M68K_MMU_HP
extern int pmap_aliasmask;
#endif

void  pmap_bootstrap(paddr_t, paddr_t);

/*
 * Special purpose kernel virtual addresses, used for mapping
 * physical pages for a variety of temporary or permanent purposes:
 *
 *	CADDR1, CADDR2:	pmap zero/copy operations
 *	vmmap:		/dev/mem, crash dumps, parity error checking
 */
caddr_t		CADDR1, CADDR2, vmmap;

/*
 * Bootstrap the VM system.
 *
 * Ideally called with MMU off, but not necessarily.  All global references
 * are relocated by `firstpa' to ensure this works.  Of course, it is not
 * possible to call any other functions from there.  `nextpa' is the first
 * available physical memory address.  Returns an updated first PA reflecting
 * the memory we have allocated.  MMU is still in the same state when we
 * return.
 *
 * XXX assumes sizeof(u_int) == sizeof(pt_entry_t)
 * XXX a PIC compiler would make this much easier.
 */
void
pmap_bootstrap(nextpa, firstpa)
	paddr_t nextpa;
	paddr_t firstpa;
{
	paddr_t kstpa, kptpa, iiopa, eiopa, kptmpa, lkptpa, p0upa;
	vaddr_t iiobase, eiobase;
	u_int nptpages, kstsize;
	st_entry_t protoste, *ste;
	pt_entry_t protopte, *pte, *epte;
	int num;
	PMAP_MD_LOCALS

	/*
	 * Calculate important physical addresses:
	 *
	 *	kstpa		kernel segment table	1 page (020/030)
	 *						N pages (040/060)
	 *
	 *	kptpa		statically allocated
	 *			kernel PT pages		Sysptsize+ pages
	 *
	 *	iiopa		internal IO space
	 *			PT pages		MACHINE_IIOMAPSIZE pages
	 *
	 *	eiopa		external IO space
	 *			PT pages		MACHINE_EIOMAPSIZE pages
	 *
	 * [ Sysptsize is the number of pages of PT, MACHINE_IIOMAPSIZE and
	 *   MACHINE_EIOMAPSIZE are the number of PTEs, hence we need to round
	 *   the total to a page boundary with IO maps at the end. ]
	 *
	 *	kptmpa		kernel PT map		1 page
	 *
	 *	lkptpa		last kernel PT page	1 page
	 *
	 *	p0upa		proc 0 u-area		UPAGES pages
	 *
	 * The KVA corresponding to any of these PAs is:
	 *	(PA - firstpa + KERNBASE).
	 */
	if (RELOC(mmutype, int) <= MMU_68040)
		kstsize = MAXKL2SIZE / (NPTEPG/SG4_LEV2SIZE);
	else
		kstsize = 1;
	kstpa = nextpa;
	nextpa += kstsize * NBPG;
	kptpa = nextpa;

	iiopa = nextpa + RELOC(Sysptsize, int) * NBPG;
	iiobase = ptoa(RELOC(Sysptsize, int) * NPTEPG);
	eiopa = iiopa + MACHINE_IIOMAPSIZE * sizeof(pt_entry_t);
	eiobase = iiobase + ptoa(MACHINE_IIOMAPSIZE);

	/*
	 * Compute how many PT pages we will need to have initialized.
	 * We need to have enough of them for the vm system to initialize
	 * up to the point we can use it to allocate more PT pages - i.e.
	 * when we can afford using pmap_enter_ptpage().
	 *
	 * Aside from the IO maps, we need to be able to successfully
	 * allocate:
	 * - nkmempages_max pages in kmeminit().
	 * - PAGER_MAP_SIZE bytes in uvm_pager_init().
	 * - 93.75 % of physmem anons in amap_init().
	 * - 4 * uvm_km_pages_lowat pages in uvm_km_page_init().
	 *
	 * We'll compute this size in bytes, then round it to pages,
	 * then to a multiple of NPTEPG.
	 */

	nptpages = ptoa(MACHINE_IIOMAPSIZE + MACHINE_EIOMAPSIZE);

	num = RELOC(physmem, int) / 4;
	if (num > NKMEMPAGES_MAX_DEFAULT)
		num = NKMEMPAGES_MAX_DEFAULT;
	nptpages += ptoa(num);

	nptpages += PAGER_MAP_SIZE;

	nptpages += (RELOC(physmem, int) * 15 * sizeof(struct vm_anon)) / 16;

	{
		extern int uvm_km_pages_lowat;

		if ((num = RELOC(uvm_km_pages_lowat, int)) == 0) {
			num = RELOC(physmem, int) / 256;
			if (num < 128)
				num = 128;
		}
	}
	nptpages += ptoa(num);

	nptpages = (atop(round_page(nptpages)) + NPTEPG - 1) / NPTEPG;

	nextpa += nptpages * NBPG;

	kptmpa = nextpa;
	nextpa += NBPG;
	lkptpa = nextpa;
	nextpa += NBPG;
	p0upa = nextpa;
	nextpa += USPACE;

	PMAP_MD_RELOC1();

	/*
	 * Initialize segment table and kernel page table map.
	 *
	 * On 68030s and earlier MMUs the two are identical except for
	 * the valid bits so both are initialized with essentially the
	 * same values.  On the 680[46]0, which have a mandatory 3-level
	 * structure, the segment table holds the level 1 table and part
	 * (or all) of the level 2 table and hence is considerably
	 * different.  Here the first level consists of 128 descriptors
	 * (512 bytes) each mapping 32mb of address space.  Each of these
	 * points to blocks of 128 second level descriptors (512 bytes)
	 * each mapping 256kb.  Note that there may be additional "segment
	 * table" pages depending on how large MAXKL2SIZE is.
	 *
	 * Portions of the last segment of KVA space (0xFFF00000 -
	 * 0xFFFFFFFF) are mapped for a couple of purposes.  0xFFF00000
	 * for UPAGES is used for mapping the current process u-area
	 * (u + kernel stack).  The very last page (0xFFFFF000) is mapped
	 * to the last physical page of RAM to give us a region in which
	 * PA == VA.  We use the first part of this page for enabling
	 * and disabling mapping.  The last part of this page also contains
	 * info left by the boot ROM.
	 *
	 * XXX cramming two levels of mapping into the single "segment"
	 * table on the 68040 is intended as a temporary hack to get things
	 * working.  The 224mb of address space that this allows will most
	 * likely be insufficient in the future (at least for the kernel).
	 */
	if (RELOC(mmutype, int) <= MMU_68040) {
		/*
		 * First invalidate the entire "segment table" pages
		 * (levels 1 and 2 have the same "invalid" value).
		 */
		pte = PA2VA(kstpa, u_int *);
		epte = &pte[kstsize * NPTEPG];
		while (pte < epte)
			*pte++ = SG_NV;
		/*
		 * Initialize level 2 descriptors (which immediately
		 * follow the level 1 table).  We need:
		 *	NPTEPG / SG4_LEV3SIZE
		 * level 2 descriptors to map each of the nptpages+1
		 * pages of PTEs.  Note that we set the "used" bit
		 * now to save the HW the expense of doing it.
		 */
		num = (nptpages + 1) * (NPTEPG / SG4_LEV3SIZE);
		pte = &(PA2VA(kstpa, u_int *))[SG4_LEV1SIZE];
		epte = &pte[num];
		protoste = kptpa | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV3SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize level 1 descriptors.  We need:
		 *	roundup(num, SG4_LEV2SIZE) / SG4_LEV2SIZE
		 * level 1 descriptors to map the `num' level 2's.
		 */
		pte = PA2VA(kstpa, u_int *);
		epte = &pte[roundup(num, SG4_LEV2SIZE) / SG4_LEV2SIZE];
		protoste = (u_int)&pte[SG4_LEV1SIZE] | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV2SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize the final level 1 descriptor to map the last
		 * block of level 2 descriptors.
		 */
		ste = &(PA2VA(kstpa, u_int *))[SG4_LEV1SIZE-1];
		pte = &(PA2VA(kstpa, u_int *))[kstsize*NPTEPG - SG4_LEV2SIZE];
		*ste = (u_int)pte | SG_U | SG_RW | SG_V;
		/*
		 * Now initialize the final portion of that block of
		 * descriptors to map the "last PT page".
		 */
		pte = &(PA2VA(kstpa, u_int *))
		    [kstsize*NPTEPG - NPTEPG/SG4_LEV3SIZE];
		epte = &pte[NPTEPG/SG4_LEV3SIZE];
		protoste = lkptpa | SG_U | SG_RW | SG_V;
		while (pte < epte) {
			*pte++ = protoste;
			protoste += (SG4_LEV3SIZE * sizeof(st_entry_t));
		}
		/*
		 * Initialize Sysptmap
		 */
		pte = PA2VA(kptmpa, u_int *);
		epte = &pte[nptpages+1];
		protopte = kptpa | PG_RW | PG_CI | PG_V | PG_U;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += NBPG;
		}
		/*
		 * Invalidate all but the last remaining entry.
		 */
		epte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		while (pte < epte) {
			*pte++ = PG_NV;
		}
		/*
		 * Initialize the last to point to the page
		 * table page allocated earlier.
		 */
		*pte = lkptpa | PG_RW | PG_CI | PG_V | PG_U;
	} else {
		/*
		 * Map the page table pages in both the HW segment table
		 * and the software Sysptmap.  Note that Sysptmap is also
		 * considered a PT page hence the +1.
		 */
		ste = PA2VA(kstpa, u_int *);
		pte = PA2VA(kptmpa, u_int *);
		epte = &pte[nptpages+1];
		protoste = kptpa | SG_RW | SG_V;
		protopte = kptpa | PG_RW | PG_CI | PG_V;
		while (pte < epte) {
			*ste++ = protoste;
			*pte++ = protopte;
			protoste += NBPG;
			protopte += NBPG;
		}
		/*
		 * Invalidate all but the last remaining entries in both.
		 */
		epte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		while (pte < epte) {
			*ste++ = SG_NV;
			*pte++ = PG_NV;
		}
		/*
		 * Initialize the last to point to point to the page
		 * table page allocated earlier.
		 */
		*ste = lkptpa | SG_RW | SG_V;
		*pte = lkptpa | PG_RW | PG_CI | PG_V;
	}
	/*
	 * Invalidate all but the final entry in the last kernel PT page
	 * (u-area PTEs will be validated later).  The final entry maps
	 * the last page of physical memory.
	 */
	pte = PA2VA(lkptpa, u_int *);
	epte = &pte[NPTEPG-1];
	while (pte < epte)
		*pte++ = PG_NV;
#ifdef MAXADDR
	/*
	 * Temporary double-map for machines with physmem at the end of
	 * memory
	 */
	*pte = MAXADDR | PG_RW | PG_CI | PG_V | PG_U;
#else
	*pte = PG_NV;
#endif
	/*
	 * Initialize kernel page table.
	 * Start by invalidating the `nptpages' that we have allocated.
	 */
	pte = PA2VA(kptpa, u_int *);
	epte = &pte[nptpages * NPTEPG];
	while (pte < epte)
		*pte++ = PG_NV;

	/*
	 * Validate PTEs for kernel text (RO).  The first page
	 * of kernel text will remain invalid to force *NULL in the
	 * kernel to fault.
	 */
	pte = &(PA2VA(kptpa, u_int *))[atop(KERNBASE)];
	epte = &pte[atop(trunc_page((vaddr_t)&etext))];

#if defined(KGDB) || defined(DDB)
	protopte = firstpa | PG_RW | PG_V | PG_U; /* XXX RW for now */
#else
	protopte = firstpa | PG_RO | PG_V | PG_U;
#endif
#ifdef	PMAP_MD_RWLOW
	for (num = PMAP_MD_RWLOW; num != 0; num--) {
		*pte++ = (protopte & ~PG_RO) | PG_RW;
		protopte += PAGE_SIZE;
	}
#else
	*pte++ = PG_NV;		/* make *NULL fail in the kernel */
	protopte += PAGE_SIZE;
#endif
	while (pte < epte) {
		*pte++ = protopte;
		protopte += PAGE_SIZE;
	}
	/*
	 * Validate PTEs for kernel data/bss, dynamic data allocated
	 * by us so far (nextpa - firstpa bytes), and pages for proc0
	 * u-area and page table allocated below (RW).
	 */
	epte = &(PA2VA(kptpa, u_int *))[atop(nextpa - firstpa)];
	protopte = (protopte & ~PG_PROT) | PG_RW | PG_U;
	/*
	 * Enable copy-back caching of data pages on 040, and write-through
	 * caching on 060
	 */
	if (RELOC(mmutype, int) == MMU_68040)
		protopte |= PG_CCB;
#ifdef M68060
	else if (RELOC(mmutype, int) == MMU_68060)
		protopte |= PG_CWT;
#endif
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}

	/*
	 * Finally, validate the internal IO space PTEs (RW+CI).
	 * We do this here since on hp300 machines with the HP MMU, the
	 * the MMU registers (also used, but to a lesser extent, on other
	 * models) are mapped in this range and it would be nice to be able
	 * to access them after the MMU is turned on.
	 */
	pte = PA2VA(iiopa, u_int *);
	epte = PA2VA(eiopa, u_int *);
	protopte = MACHINE_INTIOBASE | PG_RW | PG_CI | PG_V | PG_U;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}
	PMAP_MD_MAPIOSPACE();

	/*
	 * Calculate important exported kernel virtual addresses
	 */
	/*
	 * Sysseg: base of kernel segment table
	 */
	RELOC(Sysseg, st_entry_t *) =
	    (st_entry_t *)(kstpa - firstpa);
	/*
	 * Sysptmap: base of kernel page table map
	 */
	RELOC(Sysptmap, pt_entry_t *) =
	    (pt_entry_t *)(kptmpa - firstpa);
	/*
	 * Sysmap: kernel page table (as mapped through Sysptmap)
	 * Immediately follows `nptpages' of static kernel page table.
	 */
	RELOC(Sysmap, pt_entry_t *) =
	    (pt_entry_t *)ptoa(nptpages * NPTEPG);

	PMAP_MD_RELOC2();

	/*
	 * Setup u-area for process 0.
	 */

	/* Zero the u-area (`pte' is not really a PTE here) */
	pte = PA2VA(p0upa, u_int *);
	for (num = USPACE / sizeof(u_int); num != 0; num--)
		*pte++ = 0;

	/*
	 * Remember the u-area address so it can be loaded in the
	 * proc struct p_addr field later.
	 */
	RELOC(proc0paddr, char *) = (char *)(p0upa - firstpa);

	/*
	 * VM data structures are now initialized, set up data for
	 * the pmap module.
	 *
	 * Note about avail_end: msgbuf is initialized just after
	 * avail_end in machdep.c.  Since the last page is used
	 * for rebooting the system (code is copied there and
	 * excution continues from copied code before the MMU
	 * is disabled), the msgbuf will get trounced between
	 * reboots if it's placed in the last physical page.
	 * To work around this, we move avail_end back one more
	 * page so the msgbuf can be preserved.
	 */
	RELOC(avail_start, paddr_t) = nextpa;
	PMAP_MD_MEMSIZE();
	RELOC(mem_size, vsize_t) = ptoa(RELOC(physmem, int));
	RELOC(virtual_avail, vaddr_t) =
	    VM_MIN_KERNEL_ADDRESS + (nextpa - firstpa);
	RELOC(virtual_end, vaddr_t) = VM_MAX_KERNEL_ADDRESS;

#ifdef M68K_MMU_HP
	/*
	 * Determine VA aliasing distance if any
	 */
	if (RELOC(ectype, int) == EC_VIRT) {
		if (RELOC(machineid, int) == HP_320)
			RELOC(pmap_aliasmask, int) = 0x3fff;	/* 16k */
		else if (RELOC(machineid, int) == HP_350)
			RELOC(pmap_aliasmask, int) = 0x7fff;	/* 32k */
	}
#endif

	/*
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	{
		struct pmap *kpm = &RELOC(kernel_pmap_store, struct pmap);

		kpm->pm_stab = RELOC(Sysseg, st_entry_t *);
		kpm->pm_ptab = RELOC(Sysmap, pt_entry_t *);
		simple_lock_init(&kpm->pm_lock);
		kpm->pm_count = 1;
		kpm->pm_stpa = (st_entry_t *)kstpa;
		/*
		 * For the 040 and 060 we also initialize the free level 2
		 * descriptor mask noting that we have used:
		 *	0:		level 1 table
		 *	1 to `num':	map page tables
		 *	MAXKL2SIZE-1:	maps last-page page table
		 */
		if (RELOC(mmutype, int) <= MMU_68040) {
			int num;
			
			kpm->pm_stfree = ~l2tobm(0);
			num = roundup((nptpages + 1) * (NPTEPG / SG4_LEV3SIZE),
			    SG4_LEV2SIZE) / SG4_LEV2SIZE;
			while (num)
				kpm->pm_stfree &= ~l2tobm(num--);
			kpm->pm_stfree &= ~l2tobm(MAXKL2SIZE-1);
			for (num = MAXKL2SIZE;
			     num < sizeof(kpm->pm_stfree)*NBBY;
			     num++)
				kpm->pm_stfree &= ~l2tobm(num);
		}
	}

	/*
	 * Allocate some fixed, special purpose kernel virtual addresses
	 */
	{
		vaddr_t va = RELOC(virtual_avail, vaddr_t);

		RELOC(CADDR1, caddr_t) = (caddr_t)va;
		va += NBPG;
		RELOC(CADDR2, caddr_t) = (caddr_t)va;
		va += NBPG;
		RELOC(vmmap, caddr_t) = (caddr_t)va;
		va += NBPG;

		PMAP_MD_RELOC3();

		RELOC(msgbufp, struct msgbuf *) = (struct msgbuf *)va;
		va += MSGBUFSIZE;
		RELOC(virtual_avail, vaddr_t) = va;
	}
}
