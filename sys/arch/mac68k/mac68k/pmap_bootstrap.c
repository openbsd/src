/*	$NetBSD: pmap_bootstrap.c,v 1.18 1996/02/10 23:12:46 briggs Exp $	*/

/* 
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/msgbuf.h>
#include <sys/reboot.h>
#include <machine/pte.h>
#include <mac68k/mac68k/clockreg.h>
#include <machine/vmparam.h>
#include <machine/cpu.h>

#include <vm/vm.h>

#define PA2VA(v, t)	(t)((u_int)(v) - firstpa)

extern char *etext;
extern int Sysptsize;
extern char *extiobase, *proc0paddr;
extern st_entry_t *Sysseg;
extern pt_entry_t *Sysptmap, *Sysmap;

extern int maxmem, physmem;
extern int avail_remaining, avail_range, avail_end;
extern vm_offset_t avail_start, avail_next;
extern vm_offset_t virtual_avail, virtual_end;
extern vm_size_t mem_size;
extern int protection_codes[];

extern vm_offset_t reserve_dumppages __P((vm_offset_t));

/*
 * These are used to map the RAM:
 */
int		numranges; /* = 0 == don't use the ranges */
u_long	low[8];
u_long	high[8];
extern int		nbnumranges;
extern u_long	nbphys[];
extern u_long	nblog[];
extern   signed long	nblen[];
#define VIDMAPSIZE	btoc(mac68k_round_page(mac68k_vidlen))
extern u_int32_t	mac68k_vidlen;
extern u_int32_t	mac68k_vidlog;
extern u_int32_t	mac68k_vidphys;

extern caddr_t	ROMBase;

/*
 * Special purpose kernel virtual addresses, used for mapping
 * physical pages for a variety of temporary or permanent purposes:
 *
 *	CADDR1, CADDR2:	pmap zero/copy operations
 *	vmmap:		/dev/mem, crash dumps, parity error checking
 *	msgbufp:	kernel message buffer
 */
caddr_t		CADDR1, CADDR2, vmmap;
struct msgbuf	*msgbufp;

/*
 * Bootstrap the VM system.
 *
 * This is called with the MMU either on or off.  If it's on, we assume
 * that it's mapped with the same PA <=> LA mapping that we eventually
 * want.  The page sizes and the protections will be wrong, anyway.
 */
void
pmap_bootstrap(nextpa, firstpa)
	vm_offset_t nextpa;
	register vm_offset_t firstpa;
{
	vm_offset_t kstpa, kptpa, vidpa, iiopa, nbpa, rompa;
	vm_offset_t kptmpa, lkptpa, p0upa;
	u_int nptpages, kstsize;
	int i;
	register st_entry_t protoste, *ste;
	register pt_entry_t protopte, *pte, *epte;

	/*
	 * Calculate important physical addresses:
	 *
	 *	kstpa		kernel segment table	1 page (!040)
	 *						N pages (040)
	 *
	 *	kptpa		statically allocated
	 *			kernel PT pages		Sysptsize+ pages
	 *
	 *	vidpa		internal video space for some machines
	 *			PT pages		VIDMAPSIZE pages
	 *
	 *	nbpa 		NuBus IO space
	 *			PT pages		NBMAPSIZE pages
	 *
	 *	rompa 		ROM space
	 *			PT pages		ROMMAPSIZE pages
	 *
	 *	iiopa		internal IO space
	 *			PT pages		IIOMAPSIZE pages
	 *
	 * [ Sysptsize is the number of pages of PT, IIOMAPSIZE and
	 *   NBMAPSIZE are the number of PTEs, hence we need to round
	 *   the total to a page boundary with IO maps at the end. ]
	 *
	 *	kptmpa		kernel PT map		1 page
	 *
	 *	lkptpa		last kernel PT page	1 page
	 *
	 *	p0upa		proc 0 u-area		UPAGES pages
	 *
	 */
	if (mmutype == MMU_68040)
		kstsize = MAXKL2SIZE / (NPTEPG/SG4_LEV2SIZE);
	else
		kstsize = 1;
	kstpa = nextpa;
	nextpa += kstsize * NBPG;
	kptpa = nextpa;
	nptpages = Sysptsize +
		(IIOMAPSIZE + NBMAPSIZE + ROMMAPSIZE + VIDMAPSIZE
		 + NPTEPG - 1) / NPTEPG;
	nextpa += nptpages * NBPG;
	vidpa = nextpa - VIDMAPSIZE * sizeof(pt_entry_t);
	nbpa  = vidpa  - NBMAPSIZE  * sizeof(pt_entry_t);
	rompa = nbpa   - ROMMAPSIZE * sizeof(pt_entry_t);
	iiopa = rompa  - IIOMAPSIZE * sizeof(pt_entry_t);
	kptmpa = nextpa;
	nextpa += NBPG;
	lkptpa = nextpa;
	nextpa += NBPG;
	p0upa = nextpa;
	nextpa += USPACE;

	if (nextpa > high[0]) {
		printf("Failure in BSD boot.  nextpa=0x%x, high[0]=0x%x.\n",
			nextpa, high[0]);
		printf("You're hosed!  Try booting with 32-bit addressing ");
		printf("enabled in the memory control panel.\n");
		printf("Older machines may need Mode32 to get that option.\n");
		panic("Cannot work with the current memory mappings.\n");
	}

	/*
	 * Initialize segment table and kernel page table map.
	 *
	 * On 68030s and earlier MMUs the two are identical except for
	 * the valid bits so both are initialized with essentially the
	 * same values.  On the 68040, which has a mandatory 3-level
	 * structure, the segment table holds the level 1 table and part
	 * (or all) of the level 2 table and hence is considerably
	 * different.  Here the first level consists of 128 descriptors
	 * (512 bytes) each mapping 32mb of address space.  Each of these
	 * points to blocks of 128 second level descriptors (512 bytes)
	 * each mapping 256kb.  Note that there may be additional "segment
	 * table" pages depending on how large MAXKL2SIZE is.
	 *
	 * XXX cramming two levels of mapping into the single "segment"
	 * table on the 68040 is intended as a temporary hack to get things
	 * working.  The 224mb of address space that this allows will most
	 * likely be insufficient in the future (at least for the kernel).
	 */
	if (mmutype == MMU_68040) {
		register int num;

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
		ste = &(PA2VA(kstpa, u_int*))[SG4_LEV1SIZE-1];
		pte = &(PA2VA(kstpa, u_int*))[kstsize*NPTEPG - SG4_LEV2SIZE];
		*ste = (u_int)pte | SG_U | SG_RW | SG_V;
		/*
		 * Now initialize the final portion of that block of
		 * descriptors to map the "last PT page".
		 */
		pte = &(PA2VA(kstpa, u_int*))
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
		protopte = kptpa | PG_RW | PG_CI | PG_V;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += NBPG;
		}
		/*
		 * Invalidate all but the last remaining entries in both.
		 */
		epte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		while (pte < epte) {
			*pte++ = PG_NV;
		}
		pte = &(PA2VA(kptmpa, u_int *))[NPTEPG-1];
		*pte = lkptpa | PG_RW | PG_CI | PG_V;
	} else {
		/*
		 * Map the page table pages in both the HW segment table
		 * and the software Sysptmap.  Note that Sysptmap is also
		 * considered a PT page hence the +1.
		 */
		ste = PA2VA(kstpa, u_int*);
		pte = PA2VA(kptmpa, u_int*);
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
	*pte = (0xFFFFF000) | PG_RW | PG_CI | PG_V; /* XXX */

	/*
	 * Initialize kernel page table.
	 * Start by invalidating the `nptpages' that we have allocated.
	 */
	pte = PA2VA(kptpa, u_int *);
	epte = &pte[nptpages * NPTEPG];
	while (pte < epte)
		*pte++ = PG_NV;

	/*
	 * Validate PTEs for kernel text (RO)
	 */
	pte = &(PA2VA(kptpa, u_int *))[mac68k_btop(KERNBASE)];
	epte = &pte[mac68k_btop(mac68k_trunc_page(&etext))];
#if defined(KGDB) || defined(DDB)
	protopte = firstpa | PG_RW | PG_V;	/* XXX RW for now */
#else
	protopte = firstpa | PG_RO | PG_V;
#endif
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}
	/*
	 * Validate PTEs for kernel data/bss, dynamic data allocated
	 * by us so far (nextpa - firstpa bytes), and pages for proc0
	 * u-area and page table allocated below (RW).
	 */
	epte = &(PA2VA(kptpa, u_int *))[mac68k_btop(nextpa - firstpa)];
	protopte = (protopte & ~PG_PROT) | PG_RW;
	/*
	 * Enable copy-back caching of data pages
	 */
	if (mmutype == MMU_68040)
		protopte |= PG_CCB;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}
	/*
	 * Finally, validate the internal IO space PTEs (RW+CI).
	 * We do this here since the 320/350 MMU registers (also
	 * used, but to a lesser extent, on other models) are mapped
	 * in this range and it would be nice to be able to access
	 * them after the MMU is turned on.
	 */
	pte = PA2VA(iiopa, u_int *);
	epte = PA2VA(rompa, u_int *);
	protopte = INTIOBASE | PG_RW | PG_CI | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}

	pte = PA2VA(rompa, u_int *);
	epte = PA2VA(nbpa, u_int *);
	protopte = ((u_int) ROMBase) | PG_RO | PG_V;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}

	pte = PA2VA(nbpa, u_int *);
	epte = pte + NBMAPSIZE;
	protopte = NBBASE | PG_RW | PG_V | PG_CI;
	while (pte < epte) {
		*pte++ = protopte;
		protopte += NBPG;
	}

	if (mac68k_vidlog) {
		pte = PA2VA(vidpa, u_int *);
		epte = pte + VIDMAPSIZE;
		protopte = mac68k_vidphys | PG_RW | PG_V | PG_CI;
		while (pte < epte) {
			*pte++ = protopte;
			protopte += NBPG;
		}
	}

	/*
	 * Calculate important exported kernel virtual addresses
	 */
	/*
	 * Sysseg: base of kernel segment table
	 */
	Sysseg = PA2VA(kstpa, st_entry_t *);
	/*
	 * Sysptmap: base of kernel page table map
	 */
	Sysptmap = PA2VA(kptmpa, pt_entry_t *);
	/*
	 * Sysmap: kernel page table (as mapped through Sysptmap)
	 * Immediately follows `nptpages' of static kernel page table.
	 */
	Sysmap = (pt_entry_t *)mac68k_ptob(nptpages * NPTEPG);

	IOBase = (u_long)mac68k_ptob(nptpages*NPTEPG -
			(IIOMAPSIZE + ROMMAPSIZE + NBMAPSIZE + VIDMAPSIZE));

	ROMBase = (char *)mac68k_ptob(nptpages*NPTEPG -
					(ROMMAPSIZE + NBMAPSIZE + VIDMAPSIZE));

	NuBusBase = (u_long)mac68k_ptob(nptpages*NPTEPG -
						(NBMAPSIZE + VIDMAPSIZE));

	if (mac68k_vidlog)
		mac68k_vidlog = (u_int32_t)
				mac68k_ptob(nptpages*NPTEPG - VIDMAPSIZE)
				+ (mac68k_vidphys & PGOFSET);

	/*
	 * Setup u-area for process 0.
	 */
	/*
	 * Zero the u-area.
	 * NOTE: `pte' and `epte' aren't PTEs here.
	 */
	pte = PA2VA(p0upa, u_int *);
	epte = (u_int *) (PA2VA(p0upa, u_int) + USPACE);
	while (pte < epte)
		*pte++ = 0;
	/*
	 * Remember the u-area address so it can be loaded in the
	 * proc struct p_addr field later.
	 */
	proc0paddr = PA2VA(p0upa, char *);

	/*
	 * VM data structures are now initialized, set up data for
	 * the pmap module.
	 */
	avail_next = avail_start = mac68k_round_page(nextpa);
	avail_remaining = 0;
	avail_range = -1;
	for (i = 0; i < numranges; i++) {
		if (avail_next >= low[i] && avail_next < high[i]) {
			avail_range = i;
			avail_remaining = high[i] - avail_next;
		} else if (avail_range != -1) {
			avail_remaining += (high[i] - low[i]);
		}
	}
	physmem = mac68k_btop(avail_remaining + nextpa - firstpa);
	avail_remaining -= mac68k_round_page(sizeof(struct msgbuf));
	high[numranges - 1] -= mac68k_round_page(sizeof(struct msgbuf));

	/* XXX -- this doesn't look correct to me. */
	while (high[numranges - 1] < low[numranges - 1]) {
		numranges--;
		high[numranges - 1] -= low[numranges] - high[numranges];
	}

	avail_remaining = mac68k_trunc_page(avail_remaining);
	avail_end = avail_start + avail_remaining;
	avail_remaining = mac68k_btop(avail_remaining);

	mem_size = mac68k_ptob(physmem);
	virtual_avail = VM_MIN_KERNEL_ADDRESS + (nextpa - firstpa);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize protection array.
	 * XXX don't use a switch statement, it might produce an
	 * absolute "jmp" table.
	 */
	{
		register int *kp;

		kp = (int *) &protection_codes;
		kp[VM_PROT_NONE|VM_PROT_NONE|VM_PROT_NONE] = 0;
		kp[VM_PROT_READ|VM_PROT_NONE|VM_PROT_NONE] = PG_RO;
		kp[VM_PROT_READ|VM_PROT_NONE|VM_PROT_EXECUTE] = PG_RO;
		kp[VM_PROT_NONE|VM_PROT_NONE|VM_PROT_EXECUTE] = PG_RO;
		kp[VM_PROT_NONE|VM_PROT_WRITE|VM_PROT_NONE] = PG_RW;
		kp[VM_PROT_NONE|VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW;
		kp[VM_PROT_READ|VM_PROT_WRITE|VM_PROT_NONE] = PG_RW;
		kp[VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE] = PG_RW;
	}

	/*
	 * Kernel page/segment table allocated in locore,
	 * just initialize pointers.
	 */
	{
		struct pmap *kpm = (struct pmap *)&kernel_pmap_store;

		kpm->pm_stab = Sysseg;
		kpm->pm_ptab = Sysmap;
		simple_lock_init(&kpm->pm_lock);
		kpm->pm_count = 1;
		kpm->pm_stpa = (st_entry_t *)kstpa;
		/*
		 * For the 040 we also initialize the free level 2
		 * descriptor mask noting that we have used:
		 *	0:		level 1 table
		 *	1 to `num':	map page tables
		 *	MAXKL2SIZE-1:	maps last-page page table
		 */
		if (mmutype == MMU_68040) {
			register int num;
			
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
		vm_offset_t va = virtual_avail;

		CADDR1 = (caddr_t)va;
		va += NBPG;
		CADDR2 = (caddr_t)va;
		va += NBPG;
		vmmap = (caddr_t)va;
		va += NBPG;
		msgbufp = (struct msgbuf *)va;
		va += NBPG;
		virtual_avail = reserve_dumppages(va);
	}
}

void
bootstrap_mac68k(tc)
	int	tc;
{
	extern caddr_t	esym;
	extern u_long	videoaddr, boothowto;
	u_long		newvideoaddr = 0;
	vm_offset_t	nextpa;
	caddr_t		oldROMBase;

	if (mac68k_machine.do_graybars)
		printf("Bootstrapping NetBSD/mac68k.\n");

	oldROMBase = ROMBase;

	if ((tc & 0x80000000) && (mmutype == MMU_68030)) {

		if (mac68k_machine.do_graybars)
			printf("Getting mapping from MMU.\n");
		get_mapping();
		if (mac68k_machine.do_graybars)
			printf("Done.\n");
	} else {
		/* MMU not enabled.  Fake up ranges. */
		nbnumranges = 0;
		numranges = 1;
		low[0] = 0;
		high[0] = mac68k_machine.mach_memsize * (1024 * 1024);
		if (mac68k_machine.do_graybars)
			printf("Faked range to byte 0x%x.\n", high[0]);
	}
	nextpa = load_addr + ((int)esym + NBPG - 1) & PG_FRAME;

#if MFS
	if (boothowto & RB_MINIROOT) {
		int	v;
		boothowto |= RB_DFLTROOT;
		nextpa = mac68k_round_page(nextpa);
		if ((v = mfs_initminiroot(nextpa-load_addr)) == 0) {
			printf("Error loading miniroot.\n");
		}
		printf("Loaded %d byte miniroot.\n", v);
		nextpa += v;
	}
#endif

	if (mac68k_machine.do_graybars)
		printf("Bootstrapping the pmap system.\n");

	pmap_bootstrap(nextpa, load_addr);

	if (mac68k_machine.do_graybars)
		printf("Pmap bootstrapped.\n");

	if (mac68k_vidlog)
		newvideoaddr = mac68k_vidlog;
	else {
		if (NBBASE <= videoaddr && videoaddr <= NBTOP)
			newvideoaddr = videoaddr - NBBASE + NuBusBase;
		else
			panic("Don't know how to relocate video!\n");
	}

	if (mac68k_machine.do_graybars)
		printf("Moving ROMBase from 0x%x to 0x%x.\n",
			oldROMBase, ROMBase);

	mrg_fixupROMBase(oldROMBase, ROMBase);

	if (mac68k_machine.do_graybars)
		printf("Video address 0x%x -> 0x%x.\n",
			videoaddr, newvideoaddr);

	mac68k_set_io_offsets(IOBase);
	videoaddr = newvideoaddr;
}
