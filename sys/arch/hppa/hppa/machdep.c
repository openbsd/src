/*	$OpenBSD: machdep.c,v 1.14 1999/09/20 21:40:14 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
 *
 * follows are the copyrights of other sources used in this file.
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

#undef	BTLBDEBUG

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
#include <uvm/uvm_page.h>
#include <uvm/uvm.h>

#include <dev/cons.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/cpufunc.h>
#include <machine/autoconf.h>
#include <machine/kcore.h>

#ifdef COMPAT_HPUX
#include <compat/hpux/hpux.h>
#endif

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <hppa/dev/cpudevs.h>
#include <hppa/dev/cpudevs_data.h>

/*
 * Patchable buffer cache parameters
 */
int nswbuf = 0;
#ifdef NBUF
int nbuf = NBUF;
#else
int nbuf = 0;
#endif
#ifdef BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif

/*
 * Different kinds of flags used throughout the kernel.
 */
int cold = 1;		/* unset when engine is up to go */
int kernelmapped;	/* set when kernel is mapped */
int msgbufmapped;	/* set when safe to use msgbuf */
int hppa_malloc_ok;	/* set when safe to use malloc */
int intr_recurse;	/* interrupt/trap recursion level */

/*
 * used in locore.S
 */
int icache_stride;
int dcache_stride;
int dcache_line_mask;
int dcache_size;
double fpu_zero = 0.0;

/*
 * CPU params
 */
struct pdc_cache pdc_cache PDC_ALIGNMENT;
struct pdc_btlb pdc_btlb PDC_ALIGNMENT;

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE_ARCH;
char	cpu_model[128];
#ifdef COMPAT_HPUX
int	cpu_model_hpux; /* contains HPUX_SYSCONF_CPU* kind of value */
#endif

u_int	cpu_ticksnum, cpu_ticksdenom, cpu_hzticks;
dev_t	bootdev;
int	totalphysmem, physmem, resvmem, esym;

struct user *proc0paddr;
struct proc *fpu_curproc;
int copr_sfu_config;

#ifdef TLB_STATS
struct dtlb_stats dtlb_stats;
struct itlb_stats itlb_stats;
struct tlbd_stats tlbd_stats;
#endif

vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;

struct extent *hppa_ex;
static long mem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];

void delay_init __P((void));
static __inline void fall __P((int, int, int, int, int)); 
void dumpsys __P((void));
int bus_mem_add_mapping __P((bus_addr_t bpa, bus_size_t size, int cacheable,
			     bus_space_handle_t *bshp));

/* wide used hardware params */
struct pdc_hwtlb pdc_hwtlb PDC_ALIGNMENT;
struct pdc_coproc pdc_coproc PDC_ALIGNMENT;
struct pdc_coherence pdc_coherence PDC_ALIGNMENT;

void
hppa_init(start)
	paddr_t start;
{
	extern int kernel_text;
	vaddr_t v, vstart, vend;
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
#ifdef DEBUG
                printf("Warning: PDC_CACHE call Ret'd %d\n", pdcerr);
#endif
	}

	dcache_line_mask = pdc_cache.dc_conf.cc_line * 16 - 1;
	dcache_size = pdc_cache.dc_size;
	dcache_stride = pdc_cache.dc_stride;
	icache_stride = pdc_cache.ic_stride;

	/*
	 * get cache coherence parameters
	 */
	pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_SETCS,
			  &pdc_coherence, 1, 1, 1, 1);
#ifdef DEBUG
	printf ("PDC_CACHE_SETCS: %d, %d, %d, %d (%d)\n",
		pdc_coherence.ia_cst, pdc_coherence.da_cst,
		pdc_coherence.ita_cst, pdc_coherence.dta_cst,
		pdcerr);
#endif

	/*
	 * Fetch BTLB params
	 */
	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,
			       PDC_BTLB_DEFAULT, &pdc_btlb)) < 0) {
#ifdef DEBUG
                printf("WARNING: PDC_BTLB call Ret'd %d\n", pdcerr);
#endif
	}

	/*
	 * purge TLBs and flush caches
	 */
	if (pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB, PDC_BTLB_PURGE_ALL) < 0)
		printf("WARNING: BTLB purge failed\n");
	ptlball();
	fcacheall();

	totalphysmem = PAGE0->imm_max_mem / NBPG;
	resvmem = ((vm_offset_t)&kernel_text) / NBPG;

	/* calculate buffer cache size */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
#endif /* BUFCACHEPERCENT */
	if (bufpages == 0) {
		if (totalphysmem <= 0x1000) /* 16M */
			bufpages = totalphysmem / 100 * 5;
		else
			bufpages = totalphysmem / 100 * BUFCACHEPERCENT;
	}
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
		nswbuf = (nbuf / 2) &~ 1;
		if (nswbuf > 256)
			nswbuf = 256;
	}

	/* calculate HPT size */
	for (usehpt = 1; usehpt < totalphysmem; usehpt *= 2);
	mtctl(usehpt - 1, CR_HPTMASK);

	/*
	 * If we want to use the HW TLB support, ensure that it exists.
	 */
	if (pdc_call((iodcio_t)pdc, 0, PDC_TLB, PDC_TLB_INFO, &pdc_hwtlb) &&
	    !pdc_hwtlb.min_size && !pdc_hwtlb.max_size) {
		printf("WARNING: no HW tlb walker\n");
		usehpt = 0;
	} else {
#ifdef PMAPDEBUG
		printf("hwtlb: %u-%u, %u/",
		       pdc_hwtlb.min_size, pdc_hwtlb.max_size, usehpt);
#endif
		if (usehpt > pdc_hwtlb.max_size)
			usehpt = pdc_hwtlb.max_size;
		else if (usehpt < pdc_hwtlb.min_size)
			usehpt = pdc_hwtlb.min_size;
#ifdef PMAPDEBUG
		printf("%u\n", usehpt);
#endif
		mtctl(usehpt - 1, CR_HPTMASK);
	}
	
	vstart = hppa_round_page(start);
	vend = VM_MAX_KERNEL_ADDRESS;

	/* we hope this won't fail */
	hppa_ex = extent_create("mem", 0x0, 0xffffffff, M_DEVBUF,
				(caddr_t)mem_ex_storage,
				sizeof(mem_ex_storage),
				EX_NOCOALESCE|EX_NOWAIT);
	if (extent_alloc_region(hppa_ex, 0, (vm_offset_t)PAGE0->imm_max_mem,
				EX_NOWAIT))
		panic("cannot reserve main memory");

	v = vstart;
#define	valloc(name, type, num)	(name) = (type *)v; v = (vaddr_t)((name)+(num))

#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
	valloc(buf, struct buf, nbuf);

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
#undef valloc
	v = hppa_round_page(v);
	bzero ((void *)vstart, (v - vstart));
	vstart = v;

	pmap_bootstrap(&vstart, &vend);
	physmem = totalphysmem - btoc(vstart);

	/* alloc msgbuf */
	if (!(msgbufp = (void *)pmap_steal_memory(sizeof(struct msgbuf),
						  NULL, NULL)))
		panic("cannot allocate msgbuf");
	msgbufmapped = 1;

#ifdef PMAPDEBUG
	printf("mem: %x+%x, %x\n", physmem, resvmem, totalphysmem);
#endif
	/* Turn on the HW TLB assist */
	if (usehpt) {
		int hpt, hptsize;
		mfctl(CR_VTOP, hpt);
		mfctl(CR_HPTMASK, hptsize);
		hptsize++;
		if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_TLB,
				       PDC_TLB_CONFIG, &pdc_hwtlb, hpt,
				       hptsize, PDC_TLB_CURRPDE)) < 0) {
			printf("Warning: HW TLB init failed (%d), disabled\n",
			       pdcerr);
		} else
#ifdef PMAPDEBUG
			printf("HW TLB(%d entries at 0x%x) initialized (%d)\n",
			       hptsize / sizeof(struct hpt_entry), hpt, pdcerr);
#endif
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
	struct pdc_model pdc_model PDC_ALIGNMENT;
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
		const char *p, *q;
		i = pdc_model.hvers >> 4;
		p = hppa_mod_info(HPPA_TYPE_BOARD, i);
		switch (pdc_model.arch_rev) {
		default:
		case 0:
			q = "1.0";
#ifdef COMPAT_HPUX
			cpu_model_hpux = HPUX_SYSCONF_CPUPA10;
#endif
			break;
		case 4:
			q = "1.1";
#ifdef COMPAT_HPUX
			cpu_model_hpux = HPUX_SYSCONF_CPUPA11;
#endif
			break;
		case 8:
			q = "2.0";
#ifdef COMPAT_HPUX
			cpu_model_hpux = HPUX_SYSCONF_CPUPA20;
#endif
			break;
		}

		if (p)
			sprintf(cpu_model, "HP9000/%s PA-RISC %s", p, q);
		else
			sprintf(cpu_model, "HP9000/(UNKNOWN %x) PA-RISC %s",
				i, q);
		printf("%s\n", cpu_model);
	}

	printf("real mem = %d (%d reserved for PROM, %d used by OpenBSD)\n",
	       ctob(totalphysmem), ctob(resvmem), ctob(physmem));

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
		panic("cpu_startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = CLBYTES * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			if ((pg = uvm_pagealloc(NULL, 0, NULL, 0)) == NULL)
				panic("cpu_startup: not enough memory for "
				      "buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
				   VM_PAGE_TO_PHYS(pg),
				   VM_PROT_READ|VM_PROT_WRITE, TRUE,
				   VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, TRUE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, TRUE, FALSE, NULL);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = uvm_km_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			         VM_MBUF_SIZE, FALSE, FALSE, NULL);

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
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
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
		n = min(1000, us);
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
	vaddr_t va;
	paddr_t pa;
	vm_size_t *lenp;
	u_int prot;
{
	static u_int32_t mask;
	register vm_size_t len;
	register int pdcerr, i;

	/* align size */
	for (len = pdc_btlb.min_size << PGSHIFT; len < *lenp; len <<= 1);
	len >>= PGSHIFT;
	i = ffs(~mask) - 1;
	if (len > pdc_btlb.max_size || i < 0) {
#ifdef BTLBDEBUG
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

#ifdef BTLBDEBUG
	printf ("btlb_insert(%d): %x:%x=%x[%x,%x]\n",
		i, space, va, pa, len, prot);
#endif
	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,PDC_BTLB_INSERT,
				space, va, pa, len, prot, i)) < 0) {
#ifdef BTLBDEBUG
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
	register int error;

	bpa += HPPA_BUS_TAG_BASE(t);
	if ((error = extent_alloc_region(hppa_ex, bpa, size, EX_NOWAIT |
					 (hppa_malloc_ok? EX_MALLOCOK : 0))))
		return (error);

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
		uvm_km_free(kernel_map, sva, eva - sva);

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
	u_long bpa;
	int error;

	if (rstart < hppa_ex->ex_start || rend > hppa_ex->ex_end)
		panic("bus_space_alloc: bad region start/end");

	if ((error = extent_alloc_subregion(hppa_ex, rstart, rend, size,
					    align, bndary, EX_NOWAIT | 
					    (hppa_malloc_ok? EX_MALLOCOK:0),
					    &bpa)))
		return (error);

	if ((error = bus_mem_add_mapping(bpa, size, cacheable, bshp))) {
		if (extent_free(hppa_ex, bpa, size, EX_NOWAIT |
				(hppa_malloc_ok ? EX_MALLOCOK : 0))) {
			printf("bus_space_alloc: pa 0x%lx, size 0x%lx\n",
				bpa, size);
			printf("bus_space_alloc: can't free region\n");
		}
	}

	*addrp = bpa;

	return error;
}

void
bus_space_free(t, bsh, size)
	bus_space_tag_t t;
	bus_space_handle_t bsh;
	bus_size_t size;
{
	/* bus_space_unmap() does all that we need to do. */
	bus_space_unmap(t, bsh, size);
}

int
bus_mem_add_mapping(bpa, size, cacheable, bshp)
	bus_addr_t bpa;
	bus_size_t size;
	int cacheable;
	bus_space_handle_t *bshp;
{
	extern u_int virtual_avail;
	register u_int64_t spa, epa;
	int bank, off;

	if (bpa > 0 && bpa < virtual_avail)
		*bshp = bpa;
	else if ((bank = vm_physseg_find(atop(bpa), &off)) < 0) {
		/*
		 * determine if we are mapping IO space, or beyond the physmem
		 * region. use block mapping then
		 *
		 * we map the whole bus module (there are 1024 of those max)
		 * so, check here if it's mapped already, map if needed.
		 * all mappings a equal mappings.
		 */
		static u_int8_t bmm[1024/8];
		int flex = HPPA_FLEX(bpa);

		/* need a new mapping */
		if (!(bmm[flex / 8] & (1 << (flex & 3)))) {
			spa = bpa & FLEX_MASK;
			epa = ((u_long)((u_int64_t)bpa + size +
				~FLEX_MASK - 1) & FLEX_MASK) - 1;
#ifdef BTLBDEBUG
			printf ("bus_mem_add_mapping: adding flex=%x "
				"%qx-%qx, ", flex, spa, epa);
#endif
			while (spa < epa) {
				vm_size_t len = epa - spa;
				u_int64_t pa;
				if (len > pdc_btlb.max_size << PGSHIFT)
					len = pdc_btlb.max_size << PGSHIFT;
				if (btlb_insert(kernel_pmap->pmap_space, spa,
						spa, &len,
						kernel_pmap->pmap_pid |
					    	pmap_prot(kernel_pmap,
							  VM_PROT_ALL)) < 0)
					return -1;
				pa = spa + len - 1;
#ifdef BTLBDEBUG
				printf ("------ %d/%d, %qx, %qx-%qx",
					flex, HPPA_FLEX(pa), pa, spa, epa);
#endif
				/* do the mask */
				for (; flex <= HPPA_FLEX(pa); flex++) {
#ifdef BTLBDEBUG
					printf ("mask %x ", flex);
#endif
					bmm[flex / 8] |= (1 << (flex & 3));
				}
				spa = pa;
			}
#ifdef BTLBDEBUG
			printf ("\n");
#endif
		}
		*bshp = bpa;
	} else {
		register vm_offset_t va;

#ifdef PMAPDEBUG
		printf ("%d, %d, %x\n", bank, off, vm_physmem[0].end);
#endif
		spa = hppa_trunc_page(bpa);
		epa = hppa_round_page(bpa + size);

#ifdef DIAGNOSTIC
		if (epa <= spa)
			panic("bus_mem_add_mapping: overflow");
#endif

		if (!(va = uvm_km_valloc(kernel_map, epa - spa)))
			return (ENOMEM);

		*bshp = (bus_space_handle_t)(va + (bpa & PGOFSET));

		for (; spa < epa; spa += NBPG, va += NBPG) {
			pmap_enter(pmap_kernel(), va, spa,
				   VM_PROT_READ | VM_PROT_WRITE, TRUE, 0);
			if (!cacheable)
				pmap_changebit(spa, TLB_UNCACHEABLE, ~0);
			else
				pmap_changebit(spa, 0, ~TLB_UNCACHEABLE);
		}
	}
 
	return 0;
}

#if 0
void
flush_cache(tag, h, off, l, op)
	bus_space_tag_t tag;
	bus_space_handle_t h;
        bus_addr_t off;
	bus_size_t l;
	int op;
{
	if (l) {
		register u_int32_t p = h + off; 

		do {
			if (op == BUS_SPACE_BARRIER_READ)
				__asm __volatile ("pdc (%%sr0,%0)":: "r" (p));
			else
				__asm __volatile ("fdc (%%sr0,%0)":: "r" (p));
			__asm __volatile ("fic,m %2(%%sr0,%0)": "=r" (p)
					  : "0" (p), "r" (dcache_stride));
		} while (p < (h + off + l));
		sync_caches();
	}
}
#endif

int waittime = -1;

void
boot(howto)
	int howto;
{
	if (cold)
		howto |= RB_HALT;
	else {
		boothowto = howto | (boothowto & RB_HALT);

		if (!(howto & RB_NOSYNC) && waittime < 0) {
			extern struct proc proc0;

			/* protect against curproc->p_stats refs in sync XXX */
			if (curproc == NULL)
				curproc = &proc0;

			waittime = 0;
			vfs_shutdown();
			if ((howto & RB_TIMEBAD) == 0)
				resettodr();
			else
				printf("WARNING: not updating battery clock\n");
		}
	}

	/* XXX probably save howto into stable storage */

	splhigh();

	if ((howto & (RB_DUMP /* | RB_HALT */)) == RB_DUMP)
		dumpsys();

	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("System halted!\n");
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

u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return -1;

	return 1;
}

int
cpu_dump()
{
	long buf[dbtob(1) / sizeof (long)];
	kcore_seg_t	*segp;
	cpu_kcore_hdr_t	*cpuhdrp;

	segp = (kcore_seg_t *)buf;
	cpuhdrp = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp)) / sizeof (long)];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info
	 */
	/* nothing for now */

	return (bdevsw[major(dumpdev)].d_dump)
			(dumpdev, dumplo, (caddr_t)buf, dbtob(1));
}

/*
 * Dump the kernel's image to the swap partition.
 */
#define	BYTES_PER_DUMP	NBPG

void
dumpsys()
{
	int psize, bytes, i, n;
	register caddr_t maddr;
	register daddr_t blkno;
	register int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	register int error;

	/* Save registers
	savectx(&dumppcb); */

	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %x not possible\n", dumpdev);
		return;
	}
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	if (!(error = cpu_dump())) {

		bytes = ctob(physmem);
		maddr = NULL;
		blkno = dumplo + cpu_dumpsize();
		dump = bdevsw[major(dumpdev)].d_dump;
		/* TODO block map the whole memory */
		for (i = 0; i < bytes; i += n) {
		
			/* Print out how many MBs we to go. */
			n = bytes - i;
			if (n && (n % (1024*1024)) == 0)
				printf("%d ", n / (1024 * 1024));

			/* Limit size for next transfer. */

			if (n > BYTES_PER_DUMP)
				n =  BYTES_PER_DUMP;

			if ((error = (*dump)(dumpdev, blkno, maddr, n)))
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}

	switch (error) {
	case ENXIO:	printf("device bad\n");			break;
	case EFAULT:	printf("device not ready\n");		break;
	case EINVAL:	printf("area improper\n");		break;
	case EIO:	printf("i/o error\n");			break;
	case EINTR:	printf("aborted from console\n");	break;
	case 0:		printf("succeeded\n");			break;
	default:	printf("error %d\n", error);		break;
	}
}

/* bcopy(), error on fault */
int
kcopy(from, to, size)
	const void *from;
	void *to;
	size_t size;
{
	register void *oldh = curproc->p_addr->u_pcb.pcb_onfault;

	curproc->p_addr->u_pcb.pcb_onfault = &copy_on_fault;
	bcopy(from, to, size);
	curproc->p_addr->u_pcb.pcb_onfault = oldh;

	return 0;
}

int
copystr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return spstrcpy(HPPA_SID_KERNEL, src,
			HPPA_SID_KERNEL, dst, size, lenp);
}

int
copyinstr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return spstrcpy(curproc->p_addr->u_pcb.pcb_space, src,
			HPPA_SID_KERNEL, dst, size, lenp);
}


int
copyoutstr(src, dst, size, lenp)
	const void *src;
	void *dst;
	size_t size;
	size_t *lenp;
{
	return spstrcpy(HPPA_SID_KERNEL, src,
			curproc->p_addr->u_pcb.pcb_space, dst, size, lenp);
}


int
copyin(src, dst, size)
	const void *src;
	void *dst;
	size_t size;
{
	return spcopy(curproc->p_addr->u_pcb.pcb_space, src,
		      HPPA_SID_KERNEL, dst, size);
}

int
copyout(src, dst, size)
	const void *src;
	void *dst;
	size_t size;
{
	return spcopy(HPPA_SID_KERNEL, src,
		      curproc->p_addr->u_pcb.pcb_space, dst, size);
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
	register struct trapframe *tf;

	/* FPU: setup regs */

	tf = p->p_md.md_regs;
	/* tf->tf_r??? = PS_STRINGS */
	tf->tf_ipsw = PSW_C | PSW_Q | PSW_P | PSW_D | PSW_I;
	tf->tf_iioq_head = tf->tf_iioq_tail = pack->ep_entry;
	tf->tf_iisq_head = tf->tf_iisq_tail = p->p_addr->u_pcb.pcb_space;
	tf->tf_sp = stack;
	tf->tf_rp = 0;
	tf->tf_eiem = 0;
	tf->tf_sr4 = p->p_addr->u_pcb.pcb_space;
	tf->tf_sr5 = p->p_addr->u_pcb.pcb_space;
	tf->tf_sr6 = p->p_addr->u_pcb.pcb_space;
	tf->tf_sr7 = HPPA_SID_KERNEL;
	tf->tf_pidr1 = p->p_vmspace->vm_map.pmap->pmap_pid;
	tf->tf_pidr2 = p->p_vmspace->vm_map.pmap->pmap_pid;
	tf->tf_pidr3 = p->p_vmspace->vm_map.pmap->pmap_pid;
	tf->tf_pidr4 = p->p_vmspace->vm_map.pmap->pmap_pid;

	retval[1] = 0;
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
	/* TODO send signal */
}

int
sys_sigreturn(p, v, retval)
        struct proc *p;
	void *v;
	register_t *retval;
{
	/* TODO sigreturn */
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
