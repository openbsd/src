/*	$OpenBSD: machdep.c,v 1.2 1999/01/03 17:55:13 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 */
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */
/*
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: model_dep.c 1.34 94/12/14$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/extent.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <dev/cons.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpufunc.h>
#include <machine/autoconf.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <hppa/dev/boards.h>
#include <hppa/dev/boards_data.h>

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;


vm_offset_t istackptr;
int cold = 1;
int kernelmapped;		/* set when kernel is mapped */
int msgbufmapped;		/* set when safe to use msgbuf */

/*
 * used in locore.S
 */
struct pdc_cache pdc_cache PDC_ALIGNMENT;
int icache_stride;
int dcache_stride;
int dcache_line_mask;
int dcache_size;
double fpu_zero = 0.0;

/* the following is used externally (sysctl_hw) */
char	machine[] = "hppa";
char	cpu_model[128];
u_int	cpu_ticksnum, cpu_ticksdenom, cpu_hzticks;
dev_t	bootdev;
int	totalphysmem, physmem, resvmem, esym;
struct user *proc0paddr;
int copr_sfu_config;
int fpcopr_version;

#ifdef TLB_STATS
struct dtlb_stats dtlb_stats;
struct itlb_stats itlb_stats;
struct tlbd_stats tlbd_stats;
#endif

int hppa_malloc_ok;
struct extent *hppa_ex;
static long mem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];

void delay_init __P((void));
static __inline void fall __P((int, int, int, int, int)); 
int bus_mem_add_mapping __P((bus_addr_t bpa, bus_size_t size, int cacheable,
			     bus_space_handle_t *bshp));

void
hppa_init()
{
	extern int kernel_text, end;
	struct pdc_hwtlb pdc_hwtlb PDC_ALIGNMENT;
	struct pdc_coproc pdc_coproc PDC_ALIGNMENT;
	vm_offset_t v, vstart, vend;
	register int pdcerr;
	int usehpt;

	/* init PDC iface, so we can call em easy */
	pdc_init();

	/* calculate cpu speed */
	cpu_hzticks = (PAGE0->mem_10msec * 100) / hz;
	delay_init();

	/*
	 * get cache parameters from the PDC
	 */
	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_DFLT,
			       &pdc_cache)) < 0) {
#ifdef DIAGNOSTIC
                printf("Warning: PDC_CACHE call Ret'd %d\n", pdcerr);
#endif
	}

	dcache_line_mask = pdc_cache.dc_conf.cc_line * 16 - 1;
	dcache_size = pdc_cache.dc_size;
	dcache_stride = pdc_cache.dc_stride;
	icache_stride = pdc_cache.ic_stride;

	/*
	 * purge TLBs and flush caches
	 */
	if (pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB, PDC_BTLB_PURGE_ALL) < 0)
		printf("WARNING: BTLB purge failed\n");
	ptlball();
	fcacheall();

	/* calculate HPT size */
	hpt_hashsize = PAGE0->imm_max_mem / NBPG;
	mtctl(hpt_hashsize - 1, CR_HPTMASK);

	/*
	 * If we want to use the HW TLB support, ensure that it exists.
	 */
	if (pdc_call((iodcio_t)pdc, 0, PDC_TLB, PDC_TLB_INFO, &pdc_hwtlb) &&
	    !pdc_hwtlb.min_size && !pdc_hwtlb.max_size) {
		printf("WARNING: no HW tlb walker\n");
		usehpt = 0;
	} else {
		usehpt = 1;
#ifdef DEBUG
		printf("hwtlb: %u-%u, %u/",
		       pdc_hwtlb.min_size, pdc_hwtlb.max_size, hpt_hashsize);
#endif
		if (hpt_hashsize > pdc_hwtlb.max_size)
			hpt_hashsize = pdc_hwtlb.max_size;
		else if (hpt_hashsize < pdc_hwtlb.min_size)
			hpt_hashsize = pdc_hwtlb.min_size;
#ifdef DEBUG
		printf("%u (0x%x)\n", hpt_hashsize,
		       hpt_hashsize * sizeof(struct hpt_entry));
#endif
	}
	
	totalphysmem = PAGE0->imm_max_mem / NBPG;
	resvmem = ((vm_offset_t)&kernel_text) / NBPG;

	vstart = hppa_round_page(&end);
	vend = VM_MAX_KERNEL_ADDRESS;

	/* we hope this won't fail */
	hppa_ex = extent_create("mem", 0x0, 0xffffffff, M_DEVBUF,
				(caddr_t)mem_ex_storage,
				sizeof(mem_ex_storage),
				EX_NOCOALESCE|EX_NOWAIT);
	if (extent_alloc_region(hppa_ex, 0, (vm_offset_t)PAGE0->imm_max_mem,
				EX_NOWAIT))
		panic("cannot reserve main memory");

	/*
	 * Allocate space for system data structures.  We are given
	 * a starting virtual address and we return a final virtual
	 * address; along the way we set each data structure pointer.
	 *
	 * We call allocsys() with 0 to find out how much space we want,
	 * allocate that much and fill it with zeroes, and the call
	 * allocsys() again with the correct base virtual address.
	 */

	v = vstart;
#define	valloc(name, type, num)	\
	    (name) = (type *)v; v = (vm_offset_t)((name)+(num))

#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
	nswapmap = maxproc * 2;
	valloc(swapmap, struct map, nswapmap);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM 
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns); 
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
#endif /* BUFCACHEPERCENT */

	if (bufpages == 0)
		bufpages = totalphysmem / BUFCACHEPERCENT / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES)
		bufpages = nbuf * MAXBSIZE / CLBYTES;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
#undef valloc
	bzero ((void *)vstart, (v - vstart));
	vstart = v;

	pmap_bootstrap(&vstart, &vend);
	physmem = totalphysmem - btoc(vstart);

	/* alloc msgbuf */
	if (!(msgbufp = (void *)pmap_steal_memory(sizeof(struct msgbuf),
						  NULL, NULL)))
		panic("cannot allocate msgbuf");
	msgbufmapped = 1;

#ifdef DEBUG
	printf("mem: %x+%x, %x\n", physmem, resvmem, totalphysmem);
#endif
	/* Turn on the HW TLB assist */
	if (usehpt) {
		if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_TLB,
				       PDC_TLB_CONFIG, &pdc_hwtlb, hpt_table,
				       sizeof(struct hpt_entry) * hpt_hashsize,
				       PDC_TLB_WORD3)) < 0) {
			printf("Warning: HW TLB init failed (%d), disabled\n",
			       pdcerr);
			usehpt = 0;
		} else
			printf("HW TLB(%d entries at 0x%x) initialized (%d)\n",
			       hpt_hashsize, hpt_table, pdcerr);
	}

        /*
         * Locate any coprocessors and enable them by setting up the CCR.
         * SFU's are ignored (since we dont have any).  Also, initialize
         * the floating point registers here.
         */
        if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_COPROC, PDC_COPROC_DFLT,
			       &pdc_coproc)) < 0)
                printf("WARNING: PDC_COPROC call Ret'd %d\n", pdcerr);
	else {
#ifdef DEBUG
		printf("pdc_coproc: %x, %x\n", pdc_coproc.ccr_enable,
		       pdc_coproc.ccr_present);
#endif
	}
        copr_sfu_config = pdc_coproc.ccr_enable;
        mtctl(copr_sfu_config & CCR_MASK, CR_CCR);
/*
        fprinit(&fpcopr_version);
	fpcopr_version = (fpcopr_version & 0x003ff800) >> 11;
        mtctl(CR_CCR, 0);
*/
        /*
         * Clear the FAULT light (so we know when we get a real one)
         * PDC_COPROC apparently turns it on (for whatever reason).
         */
        pdcerr = PDC_OSTAT(PDC_OSTAT_RUN) | 0xCEC0;
        (void) (*pdc)(PDC_CHASSIS, PDC_CHASSIS_DISP, pdcerr);

#ifdef DDB
	ddb_init();
#endif
#ifdef DEBUG
	printf("hppa_init: leaving\n");
#endif
	kernelmapped++;
}

void
cpu_startup()
{
	struct pdc_model pdc_model;
	register const struct hppa_board_info *bip;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	int base, residual;
	int err, i;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/* good night */
	printf(version);

	/* identify system type */
	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_INFO,
			    &pdc_model)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_MODEL failed (%d)\n", err);
#endif
	} else {
		i = pdc_model.hvers >> 4; /* board type */
		for (bip = hppa_knownboards;
		     bip->bi_id >= 0 && bip->bi_id != i; bip++);
		if (bip->bi_id >= 0) {
			/* my babe said: 6010, 481, 0, 0, 77b657b1, 0, 4 */
			sprintf(cpu_model, "HP9000/%s rev %x",
				bip->bi_name, pdc_model.arch_rev);
		} else
			sprintf(cpu_model, "HP9000/(UNKNOWN %x)", i);
		printf("%s\n", cpu_model);
	}

	printf("real mem = %d (%d reserved for PROM, %d used by OpenBSD)\n",
	       ctob(totalphysmem), ctob(resvmem), ctob(physmem));

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("cpu_startup: cannot allocate buffers");

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		vm_map_pageable(buffer_map, minaddr, minaddr +
				CLBYTES * (base + (i < residual)), FALSE);
		vm_map_simplify(buffer_map, minaddr);
		minaddr += MAXBSIZE;
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
	hppa_malloc_ok = 1;
	configure();
}

/*
 * compute cpu_ticksdenom and cpu_ticksnum such as:
 *
 *	cpu_ticksnum / cpu_ticksdenom = t + delta
 *	delta -> 0
 */
void
delay_init(void)
{
	register u_int num, denom, delta, mdelta;

	mdelta = UINT_MAX;
	for (denom = 1; denom < 1000; denom++) {
		num = (PAGE0->mem_10msec * denom) / 10000;
		delta = num * 10000 / denom - PAGE0->mem_10msec;
		if (!delta) {
			cpu_ticksdenom = denom;
			cpu_ticksnum = num;
			break;
		} else if (delta < mdelta) {
			cpu_ticksdenom = denom;
			cpu_ticksnum = num;
		}
	}
}

void
delay(us)
	u_int us;
{
	register u_int start, end, n;

	mfctl(CR_ITMR, start);
	while (us) {
		n = min(100, us);
		end = start + n * cpu_ticksnum / cpu_ticksdenom;

		/* N.B. Interval Timer may wrap around */
		if (end < start)
			do
				mfctl(CR_ITMR, start);
			while (start > end);

		do
			mfctl(CR_ITMR, start);
		while (start < end);

		us -= n;
		mfctl(CR_ITMR, start);
	}
}

static __inline void
fall(c_base, c_count, c_loop, c_stride, data)
	int c_base, c_count, c_loop, c_stride, data; 
{
        register int loop;                  /* Internal vars */

        for (; c_count--; c_base += c_stride)
                for (loop = c_loop; loop--; )
			if (data)
				fdce(0, c_base);
			else
				fice(0, c_base);
        
}

void
fcacheall()
{
        /*
         * Flush the instruction, then data cache.
         */
        fall (pdc_cache.ic_base, pdc_cache.ic_count,
              pdc_cache.ic_loop, pdc_cache.ic_stride, 0);
	sync_caches();
        fall (pdc_cache.dc_base, pdc_cache.dc_count,
              pdc_cache.dc_loop, pdc_cache.dc_stride, 1);
	sync_caches();
}

void
ptlball()
{
        register pa_space_t sp;
        register vm_offset_t off;
	register int six, oix, lix;
	int sixend, oixend, lixend;

	/* instruction TLB */
        sixend = pdc_cache.it_sp_count;
        oixend = pdc_cache.it_off_count;
        lixend = pdc_cache.it_loop;
	sp = pdc_cache.it_sp_base;
	for (six = 0; six < sixend; six++) {
		off = pdc_cache.it_off_base;
		for (oix = 0; oix < oixend; oix++) {
			for (lix = 0; lix < lixend; lix++)
				pitlbe(sp, off);
			off += pdc_cache.it_off_stride;
		}
		sp += pdc_cache.it_sp_stride;
	}

	/* data TLB */
        sixend = pdc_cache.dt_sp_count;
        oixend = pdc_cache.dt_off_count;
        lixend = pdc_cache.dt_loop;
	sp = pdc_cache.dt_sp_base;
	for (six = 0; six < sixend; six++) {
		off = pdc_cache.dt_off_base;
		for (oix = 0; oix < oixend; oix++) {
			for (lix = 0; lix < lixend; lix++)
				pdtlbe(sp, off);
			off += pdc_cache.dt_off_stride;
		}
		sp += pdc_cache.dt_sp_stride;
	}
}

int
btlb_insert(space, va, pa, lenp, prot)
	pa_space_t space;
	vm_offset_t va, pa;
	vm_size_t *lenp;
	u_int prot;
{
	static u_int32_t mask;
	struct pdc_btlb pdc_btlb PDC_ALIGNMENT;
	register vm_size_t len;
	register int pdcerr, i;

	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,
			       PDC_BTLB_DEFAULT, &pdc_btlb)) < 0) {
#ifdef DEBUG
                printf("WARNING: PDC_BTLB call Ret'd %d\n", pdcerr);
#endif
		return -1;
	}

	/* align size */
	for (len = pdc_btlb.min_size << PGSHIFT; len < *lenp; len <<= 1);
	len >>= PGSHIFT;
	i = ffs(~mask) - 1;
	if (len > pdc_btlb.max_size || i < 0) {
#ifdef DEBUG
		printf("btln_insert: too big (%u < %u < %u)\n",
		       pdc_btlb.min_size, len, pdc_btlb.max_size);
#endif
		return -(ENOMEM);
	}

	mask |= 1 << i;
	pa >>= PGSHIFT;
	va >>= PGSHIFT;
	/* check address alignment */
	if (pa & (len - 1))
		printf("WARNING: BTLB address misaligned\n");

	/* ensure IO space is uncached */
	if ((pa & 0xF0000) == 0xF0000)
		prot |= TLB_UNCACHEABLE;

#ifdef DEBUG
	printf ("btlb_insert(%d): %x:%x, %x, %x, %x\n",
		i, space, va, pa, len, prot);
#endif
	if (pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB, PDC_BTLB_INSERT,
		     space, va, pa, len, prot, i) < 0) {
#ifdef DEBUG
		printf("WARNING: BTLB insert failed (%d)\n", pdcerr);
#endif
		return -(EINVAL);
	}
	*lenp = len << PGSHIFT;

	return i;
}

int
bus_space_map (t, bpa, size, cacheable, bshp)
	bus_space_tag_t t;
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	extern u_int virtual_avail;
	register int error;

	bpa += HPPA_BUS_TAG_BASE(t);
	if ((error = extent_alloc_region(hppa_ex, bpa, size, EX_NOWAIT |
					 (hppa_malloc_ok? EX_MALLOCOK : 0))))
		return (error);

	if ((bpa > 0 && bpa < virtual_avail) ||
	    (bpa > HPPA_IOBEGIN)) {
		*bshp = bpa;
		return 0;
	}

	if ((error = bus_mem_add_mapping(bpa, size, cacheable, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT |
				(hppa_malloc_ok? EX_MALLOCOK : 0))) {
			printf ("bus_space_map: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf ("bus_space_map: can't free region\n");
		}
	}

	return 0;
}

void
bus_space_unmap (t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	register u_long sva, eva;
	register bus_addr_t bpa;

	sva = hppa_trunc_page(bsh);
	eva = hppa_round_page(bsh + size);

#ifdef DIAGNOSTIC
	if (eva <= sva)
		panic("bus_space_unmap: overflow");
#endif

	bpa = kvtop((caddr_t)bsh);
	if (bpa != bsh)
		kmem_free(kernel_map, sva, eva - sva);

	if (extent_free(hppa_ex, bpa, size, EX_NOWAIT |
			(hppa_malloc_ok? EX_MALLOCOK : 0))) {
		printf("bus_space_unmap: ps 0x%lx, size 0x%lx\n",
		       bpa, size);
		printf("bus_space_unmap: can't free region\n");
	}
}

int
bus_space_alloc (t, rstart, rend, size, align, bndary, cacheable, addrp, bshp)
	bus_space_tag_t t;
	bus_addr_t rstart, rend;
	bus_size_t size, align, bndary;
	int cacheable;
	bus_addr_t *addrp;
	bus_space_handle_t *bshp;
{
	return -1;
}

void
bus_space_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

}

int
bus_mem_add_mapping(bpa, size, cacheable, bshp)
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	register u_long spa, epa;
	register vm_offset_t va;

	spa = hppa_trunc_page(bpa);
	epa = hppa_round_page(bpa + size);

#ifdef DIAGNOSTIC
	if (epa <= spa)
		panic("bus_mem_add_mapping: overflow");
#endif

	if (!(va = kmem_alloc_pageable(kernel_map, epa - spa)))
		return (ENOMEM);

	*bshp = (bus_space_handle_t)(va + (bpa & PGOFSET));

	for (; spa < epa; spa += NBPG, va += NBPG) {
		pmap_enter(pmap_kernel(), va, spa,
			   VM_PROT_READ | VM_PROT_WRITE, TRUE);
		if (!cacheable)
			pmap_changebit(spa, TLB_UNCACHEABLE, ~0);
		else
			pmap_changebit(spa, 0, ~TLB_UNCACHEABLE);
	}
 
	return 0;
}

void
bus_space_barrier(tag, h, off, l, op)
	bus_space_tag_t tag;
	bus_space_handle_t h;
        bus_addr_t off;
	bus_size_t l;
	int op;
{
	register u_int32_t p = h + off; 

	l += p & dcache_line_mask;
	l = (l + dcache_line_mask) & ~dcache_line_mask;
	p &= ~dcache_line_mask;

	do {
		__asm __volatile ("pdc %%r0(%%sr0,%0)":: "r" (p));
		__asm __volatile ("fic %%r0(%%sr0,%0)":: "r" (p));
		p += dcache_line_mask + 1;
		l -= dcache_line_mask + 1;
	} while (l);
}

void
boot(howto)
	int howto;
{
	/* TODO: probably save howto into stable storage */

	if (howto & RB_HALT) {
		printf("System halted!\n");
		splhigh();
		__asm __volatile("stwas %0, 0(%1)"
				 :: "r" (CMD_STOP),
				    "r" (LBCAST_ADDR + iomod_command));
	} else {
		printf("rebooting...");
		DELAY(1000000);
		__asm __volatile("stwas %0, 0(%1)"
				 :: "r" (CMD_RESET),
				    "r" (LBCAST_ADDR + iomod_command));
	}

	for(;;); /* loop while bus reset is comming up */
	/* NOTREACHED */
}

int
copystr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return 0;
}

int
copyinstr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return 0;
}


int
copyoutstr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return 0;
}


int
copyin(src, dst, size)
	const void *src;
	void *dst;
	size_t size;
{
	return 0;
}

int
copyout(src, dst, size)
	const void *src;
	void *dst;
	size_t size;
{
	return 0;
}

int
fubyte(addr)
	const void *addr;
{
	return 0;
}

int
subyte(addr, val)
	void *addr;
	int val;
{
	return 0;
}

int
suibyte(addr, val)
	void *addr;
	int val;
{
	return 0;
}

long
fuword(addr)
	const void *addr;
{
	return 0;
}

long
fuiword(addr)
	const void *addr;
{
	return 0;
}

int
suword(addr, val)
	void *addr;
	long val;
{
	return 0;
}

int
suiword(addr, val)
	void *addr;
	long val;
{
	return 0;
}

int
fuswintr(addr)
	const caddr_t addr;
{
	return 0;
}

int
suswintr(addr, val)
	caddr_t addr;
	u_int val;
{
	return 0;
}


/*
 * setrunqueue(p)
 *      proc *p;
 * 
 * Call should be made at splclock(), and p->p_stat should be SRUN.
 */ 
void
setrunqueue(p)
struct proc *p;
{

}

/*
 * remrunqueue(p)
 * 
 * Call should be made at splclock().
 */
void
remrunqueue(p)
	struct proc *p;
{
}

/*
 * Set registers on exec.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
}

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{

}

int
sys_sigreturn(p, v, retval)
        struct proc *p;
	void *v;
	register_t *retval;
{
	return EINVAL;
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	dev_t consdev;
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);               /* overloaded */
	switch (name[0]) {
	case CPU_CONSDEV: 
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
					sizeof consdev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}


/*
 * consinit:
 * initialize the system console.
 */
void
consinit()
{
	static int initted;

	if (!initted) {
		initted++;
		cninit();
	}
}
