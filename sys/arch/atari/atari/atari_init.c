/*	$NetBSD: atari_init.c,v 1.9 1995/12/16 21:40:28 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1993 Markus Wild
 * All rights reserved.
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
 *      This product includes software developed by Markus Wild.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/msgbuf.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/dkbad.h>
#include <sys/reboot.h>
#include <sys/exec.h>
#include <vm/pmap.h>
#include <machine/vmparam.h>
#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/iomap.h>
#include <machine/mfp.h>
#include <machine/scu.h>
#include <machine/video.h>
#include <atari/atari/misc.h>

extern u_int 	lowram;
extern u_int	Sysptmap, Sysptsize, Sysseg, proc0paddr;
u_int		*Sysmap;
int		machineid, mmutype, cpu040, astpending;
char		*vmmap;
pv_entry_t	pv_table;

/*
 * Need-to-know for kernel reload code.
 */
u_long	boot_ttphystart, boot_ttphysize, boot_stphysize;

extern char	*esym;

/*
 * This is the virtual address of physical page 0. Used by 'do_boot()'.
 */
vm_offset_t	page_zero;

/*
 * Crude support for allocation in ST-ram. Currently only used to allocate
 * video ram.
 * The physical address is also returned because the video init needs it to
 * setup the controller at the time the vm-system is not yet operational so
 * 'kvtop()' cannot be used.
 */
#ifndef ST_POOL_SIZE
#define	ST_POOL_SIZE	40			/* XXX: enough? */
#endif

u_long	st_pool_size = ST_POOL_SIZE * NBPG;	/* Patchable	*/
u_long	st_pool_virt, st_pool_phys;

/*
 * this is the C-level entry function, it's called from locore.s.
 * Preconditions:
 *	Interrupts are disabled
 *	PA == VA, we don't have to relocate addresses before enabling
 *		the MMU
 * 	Exec is no longer available (because we're loaded all over 
 *		low memory, no ExecBase is available anymore)
 *
 * It's purpose is:
 *	Do the things that are done in locore.s in the hp300 version, 
 *		this includes allocation of kernel maps and enabling the MMU.
 * 
 * Some of the code in here is `stolen' from Amiga MACH, and was 
 * written by Bryan Ford and Niklas Hallqvist.
 * 
 * Very crude 68040 support by Michael L. Hitch.
 */

void
start_c(id, ttphystart, ttphysize, stphysize, esym_addr)
int	id;			/* Machine id				*/
u_int	ttphystart, ttphysize;	/* Start address and size of TT-ram	*/
u_int	stphysize;		/* Size of ST-ram	 		*/
char	*esym_addr;		/* Address of kernel '_esym' symbol	*/
{
	extern char	end[];
	extern void	etext();
	extern u_long	protorp[2];
	u_int		pstart;		/* Next available physical address*/
	u_int		vstart;		/* Next available virtual address */
	u_int		avail;
	u_int		pt, ptsize;
	u_int		tc, i;
	u_int		*sg, *pg;
	u_int		sg_proto, pg_proto;
	u_int		end_loaded;
	u_int		ptextra;
	u_long		kbase;

	boot_ttphystart = ttphystart;
	boot_ttphysize  = ttphysize;
	boot_stphysize  = stphysize;

	/*
	 * The following is a hack. We do not know how much ST memory we
	 * really need until after configuration has finished. At this
	 * time I have no idea how to grab ST memory at that time.
	 * The round_page() call is ment to correct errors made by
	 * binpatching!
	 */
	st_pool_size   = atari_round_page(st_pool_size);
	st_pool_phys   = stphysize - st_pool_size;
	stphysize      = st_pool_phys;

	machineid      = id;
	esym           = esym_addr;

	/* 
	 * the kernel ends at end() or esym.
	 */
	if(esym == NULL)
		end_loaded = (u_int)end;
	else end_loaded = (u_int)esym;


	/*
	 * If we have enough fast-memory to put the kernel in, do it!
	 */
	if(ttphysize >= end_loaded)
		kbase = ttphystart;
	else kbase = 0;

	/*
	 * update these as soon as possible!
	 */
	PAGE_SIZE  = NBPG;
	PAGE_MASK  = NBPG-1;
	PAGE_SHIFT = PG_SHIFT;

	/*
	 * We run the kernel from ST memory at the moment.
	 * The kernel segment table is put just behind the loaded image.
	 * pstart: start of usable ST memory
	 * avail : size of ST memory available.
	 */
	pstart = (u_int)end_loaded;
	pstart = atari_round_page(pstart);
	avail  = stphysize - pstart;
  
	/*
	 * allocate the kernel segment table
	 */
	Sysseg  = pstart;
	pstart += NBPG;
	avail  -= NBPG;
  
	/*
	 * We only allocate 1 extra pagetable. it currently only contains
	 * entries for Sysmap.
	 */
	ptextra = 0;

	/*
	 * allocate initial page table pages
	 */
	pt      = pstart;
	ptsize  = (Sysptsize + howmany(ptextra, NPTEPG)) << PGSHIFT;
	pstart += ptsize;
	avail  -= ptsize;
  
	/*
	 * allocate kernel page table map
	 */
	Sysptmap = pstart;
	pstart  += NBPG;
	avail   -= NBPG;

	/*
	 * Set Sysmap; mapped after page table pages. Because I too (LWP)
	 * didn't understand the reason for this, I borrowed the following
	 * (sligthly modified) comment from mac68k/locore.s:
	 * LAK:  There seems to be some confusion here about the next line,
	 * so I'll explain.  The kernel needs some way of dynamically modifying
	 * the page tables for its own virtual memory.  What it does is that it
	 * has a page table map.  This page table map is mapped right after the
	 * kernel itself (in our implementation; in HP's it was after the I/O
	 * space). Therefore, the first three (or so) entries in the segment
	 * table point to the first three pages of the page tables (which
	 * point to the kernel) and the next entry in the segment table points
	 * to the page table map (this is done later).  Therefore, the value
	 * of the pointer "Sysmap" will be something like 16M*3 = 48M.  When
	 * the kernel addresses this pointer (e.g., Sysmap[0]), it will get
	 * the first longword of the first page map (== pt[0]).  Since the
	 * page map mirrors the segment table, addressing any index of Sysmap
	 * will give you a PTE of the page maps which map the kernel.
	 */
	Sysmap = (u_int *)(ptsize << (SEGSHIFT - PGSHIFT));

	/*
	 * Initialize segment table and page table map.
	 */
	sg_proto = (pt + kbase) | SG_RW | SG_V;
	pg_proto = (pt + kbase) | PG_RW | PG_CI | PG_V;
	/*
	 * map so many segs
	 */
	sg = (u_int *)Sysseg;
	pg = (u_int *)Sysptmap; 
	while(sg_proto < (pstart + kbase)) {
		*sg++ = sg_proto;
		*pg++ = pg_proto;
		sg_proto += NBPG;
		pg_proto += NBPG;
	}
	/* 
	 * invalidate the remainder of the tables
	 */
	do {
		*sg++ = SG_NV;
		*pg++ = PG_NV;
	} while(sg < (u_int *)(Sysseg + ATARI_STSIZE));

	/*
	 * initialize kernel page table page(s).
	 * Assume load at VA 0.
	 * - Text pages are RO
	 */
	pg_proto = (0 + kbase) | PG_RO | PG_V;
	pg       = (u_int *) pt;
	for(i = 0; i < (u_int)etext; i += NBPG, pg_proto += NBPG)
		*pg++ = pg_proto;

	/* 
	 * data, bss and dynamic tables are read/write
	 */
	pg_proto = (pg_proto & PG_FRAME) | PG_RW | PG_V;

	/*
	 * go till end of data allocated so far
	 * plus proc0 u-area (to be allocated)
	 */
	for(; i < pstart + USPACE; i += NBPG, pg_proto += NBPG)
		*pg++ = pg_proto;

	/*
	 * invalidate remainder of kernel PT
	 */
	while(pg < (u_int *)(pt + ptsize))
		*pg++ = PG_NV;

	/*
	 * Go back and validate internal IO PTEs. They MUST be Cache inhibited!
	 */
	pg       = (u_int *) pt + (AD_IO / NBPG);
	pg_proto = AD_IO | PG_RW | PG_CI | PG_V;
	while(pg_proto < AD_EIO) {
		*pg++     = pg_proto;
		pg_proto += NBPG;
	}

	/*
	 * Clear proc0 user-area
	 */
	bzero((u_char *)pstart, USPACE);

	/*
	 * Save KVA of proc0 user-area and allocate it
	 */
	proc0paddr = pstart;
	pstart    += USPACE;
	avail     -= USPACE;

	/*
	 * At this point, virtual and physical allocation starts to divert.
	 */
	vstart     = pstart;

	/*
	 * Map the allocated space in ST-ram now. In the contig-case, there
	 * is no need to make a distinction between virtual and physical
	 * adresses. But I make it anyway to be prepared.
	 * Physcal space is already reserved!
	 */
	st_pool_virt = vstart;
	pg           = (u_int *) pt + (vstart / NBPG);
	pg_proto     = st_pool_phys | PG_RW | PG_CI | PG_V;
	vstart      += st_pool_size;
	while(pg_proto < (st_pool_phys + st_pool_size)) {
		*pg++     = pg_proto;
		pg_proto += NBPG;
	}

	/*
	 * Map physical page zero (First ST-ram page)
	 */
	page_zero  = vstart;
	pg         = (u_int *) pt + (vstart / NBPG);
	*pg        = PG_RW | PG_CI | PG_V;
	vstart    += NBPG;

	lowram  = 0 >> PGSHIFT; /* XXX */

	/*
	 * Fill in segments. The page indexes will be initialized
	 * later when all reservations are made.
	 */
	phys_segs[0].start       = 0;
	phys_segs[0].end         = stphysize;
	phys_segs[1].start       = ttphystart;
	phys_segs[1].end         = ttphystart + ttphysize;
	phys_segs[2].start       = 0; /* End of segments! */

	if(kbase) {
		/*
		 * First page of ST-ram is unusable, reserve the space
		 * for the kernel in the TT-ram segment.
		 */
		phys_segs[0].start  = NBPG;
		phys_segs[1].start += pstart;
	}
	else {
		/*
		 * Because the first 8 addresses of ST-memory are mapped to
		 * ROM, we remap them. This makes the debugger stack-trace
		 * work.
		 */
		extern	u_long	first_8_bytes[];
			u_long	*sp, *dp;

		/*
		 * Copy page zero and set first 8 bytes.
		 */
		sp = (u_long *)0;
		dp = (u_long *)pstart;
		while(dp < (u_long *)(pstart+NBPG))
			*dp++ = *sp++;
		dp    = (u_long *)pstart;
		*dp++ = first_8_bytes[0];
		*dp   = first_8_bytes[1];

		/*
		 * Enter into the page-table
		 */
		pg  = (u_int *)pt;
		*pg = pstart | PG_RO | PG_V;


		/*
		 * Reserve space for page 0, and allocate the kernel
		 * space from the ST-ram segment.
		 */
		pstart += NBPG;
		phys_segs[0].start += pstart;
	}

	/*
	 * As all segment sizes are now valid, calculate page indexes and
	 * available physical memory.
	 */
	phys_segs[0].first_page = 0;
	for (i = 1; phys_segs[i].start; i++) {
		phys_segs[i].first_page  = phys_segs[i-1].first_page;
		phys_segs[i].first_page +=
			(phys_segs[i-1].end - phys_segs[i-1].start) / NBPG;
	}
	for (i = 0, physmem = 0; phys_segs[i].start; i++)
		physmem += phys_segs[i].end - phys_segs[i].start;
	physmem >>= PGSHIFT;
  
	/*
	 * get the pmap module in sync with reality.
	 */
	pmap_bootstrap(vstart);

	/*
	 * Prepare to enable the MMU.
	 * Setup and load SRP nolimit, share global, 4 byte PTE's
	 */
	protorp[0] = 0x80000202;
	protorp[1] = Sysseg + kbase;	/* + segtable address */

	/*
	 * copy over the kernel (and all now initialized variables) 
	 * to fastram.  DONT use bcopy(), this beast is much larger 
	 * than 128k !
	 */
	if(kbase) {
		register u_long	*lp, *le, *fp;
		extern	 u_long	first_8_bytes[];

		lp = (u_long *)0;
		le = (u_long *)pstart;
		fp = (u_long *)kbase;
		while(lp < le)
			*fp++ = *lp++;

		/*
		 * Fill in reset stuff
		 */
		fp    = (u_long *)kbase;
		*fp++ = first_8_bytes[0];
		*fp   = first_8_bytes[1];
	}

	asm volatile ("pmove %0@,srp" : : "a" (&protorp[0]));
	/*
	 * setup and load TC register.
	 * enable_cpr, enable_srp, pagesize=8k,
	 * A = 8 bits, B = 11 bits
	 */
	tc = 0x82d08b00;
	asm volatile ("pmove %0@,tc" : : "a" (&tc));

	/* Is this to fool the optimizer?? */
	i = *(int *)proc0paddr;
	*(volatile int *)proc0paddr = i;

	/*
	 * Initialize the sound-chip YM2149:
	 *   All sounds off, both ports output.
	 */
	SOUND->sd_selr = YM_MFR;
	SOUND->sd_wdat = 0xff;

	/*
	 * Initialize both MFP chips (if both present!) to generate
	 * auto-vectored interrupts with EOI. The active-edge registers are
	 * set up. The interrupt enable registers are set to disable all
	 * interrupts.
	 * A test on presence on the second MFP determines if this is a
	 * TT030 or a Falcon. This is added to 'machineid'.
	 */
	MFP->mf_iera  = MFP->mf_ierb = 0;
	MFP->mf_imra  = MFP->mf_imrb = 0;
	MFP->mf_aer   = 0;
	MFP->mf_vr    = 0x40;
	if(!badbaddr(&MFP2->mf_gpip)) {
		machineid |= ATARI_TT;
		MFP2->mf_iera = MFP2->mf_ierb = 0;
		MFP2->mf_imra = MFP2->mf_imrb = 0;
		MFP2->mf_aer  = 0x80;
		MFP2->mf_vr   = 0x50;

		/*
		 * Initialize the SCU, to enable interrupts on the SCC (ipl5),
		 * MFP (ipl6) and softints (ipl1).
		 */
		SCU->sys_mask = SCU_MFP | SCU_SCC | SCU_SYS_SOFT;
#ifdef DDB
		/*
		 * This allows people with the correct hardware modification
		 * to drop into the debugger from an NMI.
		 */
		SCU->sys_mask |= SCU_IRQ7;
#endif
	}
	else machineid |= ATARI_FALCON;

	/*
	 * Initialize stmem allocator
	 */
	init_stmem();
}

#ifdef DEBUG
void
dump_segtable(stp)
	u_int *stp;
{
	u_int *s, *es;
	int shift, i;

	s = stp;
	{
		es = s + (ATARI_STSIZE >> 2);
		shift = SG_ISHIFT;
	}

	/* 
	 * XXX need changes for 68040 
	 */
	for (i = 0; s < es; s++, i++)
		if (*s & SG_V)
			printf("$%08lx: $%08lx\t", i << shift, *s & SG_FRAME);
	printf("\n");
}

void
dump_pagetable(ptp, i, n)
	u_int *ptp, i, n;
{
	u_int *p, *ep;

	p = ptp + i;
	ep = p + n;
	for (; p < ep; p++, i++)
		if (*p & PG_V)
			printf("$%08lx -> $%08lx\t", i, *p & PG_FRAME);
	printf("\n");
}

u_int
vmtophys(ste, vm)
	u_int *ste, vm;
{
	ste = (u_int *) (*(ste + (vm >> SEGSHIFT)) & SG_FRAME);
		ste += (vm & SG_PMASK) >> PGSHIFT;
	return((*ste & -NBPG) | (vm & (NBPG - 1)));
}

#endif
