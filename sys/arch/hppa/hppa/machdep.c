/*	$OpenBSD: machdep.c,v 1.24 2000/03/23 09:59:54 art Exp $	*/

/*
 * Copyright (c) 1999-2000 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef BTLBDEBUG

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
#include <sys/timeout.h>
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
int msgbufmapped;	/* set when safe to use msgbuf */

/*
 * things to kill
 */
int icache_stride;
int dcache_stride;
int dcache_line_mask;

/*
 * things to not kill
 */
volatile u_int8_t *machine_ledaddr;
int machine_ledword, machine_leds;

/*
 * CPU params (should be the same for all cpus in the system)
 */
struct pdc_cache pdc_cache PDC_ALIGNMENT;
struct pdc_btlb pdc_btlb PDC_ALIGNMENT;
	/* w/ a little deviation should be the same for all installed cpus */
u_int	cpu_ticksnum, cpu_ticksdenom, cpu_hzticks;
	/* exported info */
char	machine[] = MACHINE_ARCH;
char	cpu_model[128];
#ifdef COMPAT_HPUX
int	cpu_model_hpux;	/* contains HPUX_SYSCONF_CPU* kind of value */
#endif

dev_t	bootdev;
int	totalphysmem, resvmem, physmem, esym;

/*
 * Things for MI glue to stick on.
 */
struct user *proc0paddr;
long mem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
struct extent *hppa_ex;

vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;


void delay_init __P((void));
static __inline void fall __P((int, int, int, int, int));
void dumpsys __P((void));
void hpmc_dump __P((void));

/*
 * wide used hardware params
 */
struct pdc_hwtlb pdc_hwtlb PDC_ALIGNMENT;
struct pdc_coproc pdc_coproc PDC_ALIGNMENT;
struct pdc_coherence pdc_coherence PDC_ALIGNMENT;

#ifdef DEBUG
int sigdebug = 0xff;
pid_t sigpid = 0;
#define SDB_FOLLOW	0x01
#endif


void
hppa_init(start)
	paddr_t start;
{
	extern int kernel_text;
	vaddr_t v, vstart, vend;
	register int error;
	int hptsize;	/* size of HPT table if supported */

	boothowto |= RB_SINGLE;	/* XXX always go into single-user while debug */

	pdc_init();	/* init PDC iface, so we can call em easy */

	cpu_hzticks = (PAGE0->mem_10msec * 100) / hz;
	delay_init();	/* calculate cpu clock ratio */

	/* cache parameters */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_DFLT,
	    &pdc_cache)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_CACHE error %d\n", error);
#endif
	}

	/* XXX these gonna die */
	dcache_line_mask = pdc_cache.dc_conf.cc_line * 16 - 1;
	dcache_stride = pdc_cache.dc_stride;
	icache_stride = pdc_cache.ic_stride;

	/* cache coherence params (pbably available for 8k only) */
	error = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_SETCS,
	    &pdc_coherence, 1, 1, 1, 1);
#ifdef DEBUG
	printf ("PDC_CACHE_SETCS: %d, %d, %d, %d (%d)\n",
	    pdc_coherence.ia_cst, pdc_coherence.da_cst,
	    pdc_coherence.ita_cst, pdc_coherence.dta_cst, error);
#endif

	/* setup hpmc handler */
	{
		extern u_int hpmc_v;	/* from locore.s */
		register u_int *p = &hpmc_v;

		if (pdc_call((iodcio_t)pdc, 0, PDC_INSTR, PDC_INSTR_DFLT, p))
			*p = 0;	/* XXX nop is more appropriate? */

		p[5] = -(p[0] + p[1] + p[2] + p[3] + p[4] + p[6] + p[7]);
	}

	/* BTLB params */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,
	    PDC_BTLB_DEFAULT, &pdc_btlb)) < 0)
		panic("WARNING: PDC_BTLB error %d", error);

	/* purge TLBs and caches */
	if (pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB, PDC_BTLB_PURGE_ALL) < 0)
		printf("WARNING: BTLB purge failed\n");

	ptlball();
	fcacheall();

	totalphysmem = PAGE0->imm_max_mem / NBPG;
	resvmem = ((vaddr_t)&kernel_text) / NBPG;

	/* calculate HPT size */
	for (hptsize = 1; hptsize < totalphysmem; hptsize *= 2);
	mtctl(hptsize - 1, CR_HPTMASK);

	if (pdc_call((iodcio_t)pdc, 0, PDC_TLB, PDC_TLB_INFO, &pdc_hwtlb) &&
	    !pdc_hwtlb.min_size && !pdc_hwtlb.max_size) {
		printf("WARNING: no HPT support, fine!\n");
		hptsize = 0;
	} else {
		if (hptsize > pdc_hwtlb.max_size)
			hptsize = pdc_hwtlb.max_size;
		else if (hptsize < pdc_hwtlb.min_size)
			hptsize = pdc_hwtlb.min_size;
		/* have to reload after adjustment */
		mtctl(hptsize - 1, CR_HPTMASK);
	}

	/* we hope this won't fail */
	hppa_ex = extent_create("mem", 0x0, 0xffffffff, M_DEVBUF,
	    (caddr_t)mem_ex_storage, sizeof(mem_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);
	if (extent_alloc_region(hppa_ex, 0, (vaddr_t)PAGE0->imm_max_mem,
	    EX_NOWAIT))
		panic("cannot reserve main memory");

	vstart = hppa_round_page(start);
	vend = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Now allocate kernel dynamic variables
	 */

	/* buffer cache parameters */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
#endif /* BUFCACHEPERCENT */
	if (bufpages == 0)
		bufpages = totalphysmem / 100 *
		    (totalphysmem <= 0x1000? 5 : BUFCACHEPERCENT);

	if (nbuf == 0)
		nbuf = bufpages < 16? 16 : bufpages;

	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless. */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES)
		bufpages = nbuf * MAXBSIZE / CLBYTES;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;
		if (nswbuf > 256)
			nswbuf = 256;
	}
	
	v = vstart;
#define valloc(name, type, num) (name) = (type *)v; v = (vaddr_t)((name)+(num))

#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif

	valloc(timeouts, struct timeout, ntimeout);
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
	if (!(msgbufp = (void *)pmap_steal_memory(MSGBUFSIZE, NULL, NULL)))
		panic("cannot allocate msgbuf");
	msgbufmapped = 1;

	/* Turn on the HW TLB assist */
	if (hptsize) {
		u_int hpt;

		mfctl(CR_VTOP, hpt);
		if ((error = pdc_call((iodcio_t)pdc, 0, PDC_TLB,
		    PDC_TLB_CONFIG, &pdc_hwtlb, hpt, hptsize,
		    PDC_TLB_CURRPDE)) < 0) {
#ifdef DEBUG
			printf("WARNING: HPT init error %d\n", error);
#endif
		} else {
#ifdef PMAPDEBUG
			printf("HPT: %d entries @ 0x%x\n",
			    hptsize / sizeof(struct hpt_entry), hpt);
#endif
		}
	}

	/* locate coprocessors and SFUs */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_COPROC, PDC_COPROC_DFLT,
	    &pdc_coproc)) < 0)
		printf("WARNING: PDC_COPROC error %d\n", error);
	else {
#ifdef DEBUG
		printf("pdc_coproc: %x, %x\n", pdc_coproc.ccr_enable,
		    pdc_coproc.ccr_present);
#endif
		mtctl(pdc_coproc.ccr_enable & CCR_MASK, CR_CCR);
	}

	/* they say PDC_COPROC might turn fault light on */
	pdc_call((iodcio_t)pdc, PDC_CHASSIS, PDC_CHASSIS_DISP,
	    PDC_OSTAT(PDC_OSTAT_RUN) | 0xCEC0);

#ifdef DDB
	ddb_init();
#endif
}

void
cpu_startup()
{
	struct pdc_model pdc_model PDC_ALIGNMENT;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
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
		printf("WARNING: PDC_MODEL error %d\n", err);
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
	 * Kludge for STI graphics, which may have rom outside the IO space
	 */
#include "sti.h"
#if NSTI > 0
	{
		vaddr_t addr = PAGE0->pd_resv2[1];

		/* reserve virtual address space for the sti rom */
		if (addr && addr < 0xf0000000 &&
		    uvm_map(kernel_map, &addr, 0x1000000, NULL,
			UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_NONE,
			    UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_NORMAL,
			    UVM_FLAG_FIXED)) != KERN_SUCCESS)
			printf("WARNING: don't have space for stinger @0x%x\n",
			    addr);
	}
#endif

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
	    NULL, UVM_UNKNOWN_OFFSET, UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE,
	    UVM_INH_NONE, UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
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
			    VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
			    TRUE, VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
	    M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = uvm_km_suballoc(kernel_map, (vaddr_t *)&mbutl, &maxaddr,
	    VM_MBUF_SIZE, VM_MAP_INTRSAFE, FALSE, NULL);

	/*
	 * Initialize timeouts
	 */
	timeout_init();

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
	configure();
}

/*
 * compute cpu clock ratio such as:
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
	register int loop;

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
	fall(pdc_cache.ic_base, pdc_cache.ic_count, pdc_cache.ic_loop,
	    pdc_cache.ic_stride, 0);
	sync_caches();
	fall(pdc_cache.dc_base, pdc_cache.dc_count, pdc_cache.dc_loop,
	    pdc_cache.dc_stride, 1);
	sync_caches();
}

void
ptlball()
{
	register pa_space_t sp;
	register int i, j, k;

	/* instruction TLB */
	sp = pdc_cache.it_sp_base;
	for (i = 0; i < pdc_cache.it_sp_count; i++) {
		register vaddr_t off = pdc_cache.it_off_base;
		for (j = 0; j < pdc_cache.it_off_count; j++) {
			for (k = 0; k < pdc_cache.it_loop; k++)
				pitlbe(sp, off);
			off += pdc_cache.it_off_stride;
		}
		sp += pdc_cache.it_sp_stride;
	}

	/* data TLB */
	sp = pdc_cache.dt_sp_base;
	for (i = 0; i < pdc_cache.dt_sp_count; i++) {
		register vaddr_t off = pdc_cache.dt_off_base;
		for (j = 0; j < pdc_cache.dt_off_count; j++) {
			for (k = 0; k < pdc_cache.dt_loop; k++)
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
	vsize_t *lenp;
	u_int prot;
{
	static u_int32_t mask;
	register vsize_t len;
	register int error, i;

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
	printf("btlb_insert(%d): %x:%x=%x[%x,%x]\n", i, space, va, pa, len, prot);
#endif
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,PDC_BTLB_INSERT,
	    space, va, pa, len, prot, i)) < 0) {
#ifdef BTLBDEBUG
		printf("WARNING: BTLB insert failed (%d)\n", error);
#endif
		return -(EINVAL);
	}
	*lenp = len << PGSHIFT;

	return i;
}

int waittime = -1;

void
boot(howto)
	int howto;
{
	if (cold)
		/* XXX howto |= RB_HALT */;
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
		    :: "r" (CMD_STOP), "r" (LBCAST_ADDR + iomod_command));
	} else {
		printf("rebooting...");
		DELAY(1000000);
		__asm __volatile("stwas %0, 0(%1)"
		    :: "r" (CMD_RESET), "r" (LBCAST_ADDR + iomod_command));
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

/*
 * Called from HPMC handler in locore
 */
void
hpmc_dump()
{

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
		/* TODO block map the whole physical memory */
		for (i = 0; i < bytes; i += n) {
		
			/* Print out how many MBs we are to go. */
			n = bytes - i;
			if (n && (n % (1024*1024)) == 0)
				printf("%d ", n / (1024 * 1024));

			/* Limit size for next transfer. */

			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;

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
	register u_int oldh = curproc->p_addr->u_pcb.pcb_onfault;

	curproc->p_addr->u_pcb.pcb_onfault = (u_int)&copy_on_fault;
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
	return spstrcpy(HPPA_SID_KERNEL, src, HPPA_SID_KERNEL, dst, size, lenp);
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
	register struct trapframe *tf = p->p_md.md_regs;
	/* register struct pcb *pcb = &p->p_addr->u_pcb; */
#ifdef DEBUG
	/*extern int pmapdebug;*/
	/*pmapdebug = 13;*/
	printf("setregs(%p, %p, %x, %p), ep=%x, cr30=%x\n",
	    p, pack, stack, retval, pack->ep_entry, tf->tf_cr30);
#endif

	tf->tf_iioq_tail = 4 +
	    (tf->tf_iioq_head = pack->ep_entry | HPPA_PC_PRIV_USER);
	tf->tf_rp = 0;
	tf->tf_arg0 = (u_long)PS_STRINGS;
	tf->tf_arg1 = tf->tf_arg2 = 0; /* XXX dynload stuff */

	/* setup terminal stack frame */
	stack += HPPA_FRAME_SIZE;
	suword((caddr_t)(stack + HPPA_FRAME_PSP), 0);
	tf->tf_sp = stack;

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
	struct proc *p = curproc;

#ifdef DEBUG
	if ((sigdebug | SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig: %s[%d] sig %d catcher %p\n",
		    p->p_comm, p->p_pid, sig, catcher);
#endif

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
		return (ENOTDIR);	/* overloaded */
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
