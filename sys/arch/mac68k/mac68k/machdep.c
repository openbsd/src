/*	$OpenBSD: machdep.c,v 1.56 1999/02/15 08:56:05 todd Exp $	*/
/*	$NetBSD: machdep.c,v 1.134 1997/02/14 06:15:30 scottr Exp $	*/

/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * from: Utah $Hdr: machdep.c 1.63 91/04/24$
 *
 *	@(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/extent.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
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

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <ddb/db_var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/bus.h>
#include <net/netisr.h>

void netintr __P((void));

#define	MAXMEM	64*1024*CLSIZE	/* XXX - from cmap.h */
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <dev/cons.h>
#include <arch/mac68k/mac68k/macrom.h>
#include <arch/mac68k/dev/adbvar.h>

#include <machine/viareg.h>
#include "ether.h"

/* The following is used externally (sysctl_hw) */
char    machine[] = "mac68k";	/* cpu "architecture" */

struct mac68k_machine_S mac68k_machine;

volatile u_char *Via1Base, *Via2Base, *PSCBase = NULL;
u_long  NuBusBase = NBBASE;
u_long  IOBase;

vm_offset_t SCSIBase;

/* These are used to map kernel space: */
extern int numranges;
extern u_long low[8];
extern u_long high[8];

/* These are used to map NuBus space: */
#define	NBMAXRANGES	16
int     nbnumranges;		/* = 0 == don't use the ranges */
u_long  nbphys[NBMAXRANGES];	/* Start physical addr of this range */
u_long  nblog[NBMAXRANGES];	/* Start logical addr of this range */
long    nblen[NBMAXRANGES];	/* Length of this range If the length is */
				/* negative, all phys addrs are the same. */

extern u_long videoaddr;	/* Addr used in kernel for video. */
extern u_long videorowbytes;	/* Used in kernel for video. */

/*
 * Values for IIvx-like internal video
 * -- should be zero if it is not used (usual case).
 */
u_int32_t mac68k_vidlog;	/* logical addr */
u_int32_t mac68k_vidphys;	/* physical addr */
u_int32_t mac68k_vidlen;	/* mem length */

/* Callback and cookie to run bell */
int	(*mac68k_bell_callback) __P((void *, int, int, int));
caddr_t	mac68k_bell_cookie;

vm_map_t buffer_map;

/*
 * Declare these as initialized data so we can patch them.
 */
int     nswbuf = 0;
#ifdef	NBUF
int     nbuf = NBUF;
#else
int     nbuf = 0;
#endif
#ifdef	BUFPAGES
int     bufpages = BUFPAGES;
#else
int     bufpages = 0;
#endif

int     msgbufmapped;		/* set when safe to use msgbuf */
int     maxmem;			/* max memory per process */
int     physmem = MAXMEM;	/* max supported memory, changes to actual */

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int     safepri = PSL_LOWIPL;

/*
 * Some of the below are not used yet, but might be used someday on the
 * Q700/900/950 where the interrupt controller may be reprogrammed to
 * interrupt on different levels as listed in locore.s
 */
unsigned short  mac68k_ttyipl = PSL_S | PSL_IPL1;
unsigned short  mac68k_bioipl = PSL_S | PSL_IPL2;
unsigned short  mac68k_netipl = PSL_S | PSL_IPL2;
unsigned short  mac68k_impipl = PSL_S | PSL_IPL2;
unsigned short  mac68k_clockipl = PSL_S | PSL_IPL2;
unsigned short  mac68k_statclockipl = PSL_S | PSL_IPL2;
unsigned short  mac68k_schedipl = PSL_S | PSL_IPL3;


/*
 * Extent maps to manage all memory space, including I/O ranges.  Allocate
 * storage for 8 regions in each, initially.  Later, iomem_malloc_safe
 * will indicate that it's safe to use malloc() to dynamically allocate
 * region descriptors.
 *
 * The extent maps are not static!  Machine-dependent NuBus and on-board
 * I/O routines need access to them for bus address space allocation.
 */
static	long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
struct	extent *iomem_ex;
int	iomem_malloc_safe;

/* XXX should be in locore.s for consistency */
int	astpending=0;
int	want_resched=0;

static void	identifycpu __P((void));
static u_long	get_physical __P((u_int, u_long *));
void		dumpsys __P((void));

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit(void)
{
	/*
	 * Generic console: sys/dev/cons.c
	 *	Initializes either ite or ser as console.
	 *	Can be called from locore.s and init_main.c.
	 */
	static int init;	/* = 0 */

	if (!init) {
		cninit();
#ifdef  DDB
		/*
		 * Initialize kernel debugger, if compiled in.
		 */
		ddb_init();
#endif
		init = 1;
	} else {
		mac68k_calibrate_delay();
	}
}

#define CURRENTBOOTERVER	108

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup(void)
{
	register caddr_t	v, firstaddr;
	register unsigned	i;
	int     	vers;
	int     	base, residual;
	vm_offset_t	minaddr, maxaddr;
	vm_size_t	size = 0;	/* To avoid compiler warning */
	int     	delay;

	/*
	 * Initialize error message buffer (at end of core).
	 * high[numranges-1] was decremented in pmap_bootstrap.
	 */
	for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t) msgbufp,
		    high[numranges - 1] + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();

	vers = mac68k_machine.booter_version;
	if (vers < CURRENTBOOTERVER) {
		/* fix older booters with indicies, not versions */
		if (vers < 100)
			vers += 99;

		printf("\nYou booted with booter version %d.%d.\n",
		    vers / 100, vers % 100);
		printf("Booter version %d.%d is necessary to fully support\n",
		    CURRENTBOOTERVER / 100, CURRENTBOOTERVER % 100);
		printf("this kernel.\n\n");
		for (delay = 0; delay < 1000000; delay++);
	}
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
	 * The first available real memory address is in "firstaddr".
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */
	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t) firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
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
	/*
	 * Determine how many buffers to allocate.
	 * Use 10% of memory for the first 2 Meg, 5% of the remaining
	 * memory. Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0) {
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / 10 / CLSIZE;
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) / 20 / CLSIZE;
	}
	bufpages = min(NKMEMCLUSTERS * 2 / 5, bufpages);

	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;	/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t) (v - firstaddr);
		firstaddr = (caddr_t) kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}
	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t) (v - firstaddr) != size)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *) & buffers,
	    &maxaddr, size, TRUE);
	minaddr = (vm_offset_t) buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t) 0,
		&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* Don't want to alloc more physical mem than needed. */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t) buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *) malloc(NMBCLUSTERS + CLBYTES / MCLBYTES,
	    M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS + CLBYTES / MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *) & mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i - 1].c_next = &callout[i];

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
	iomem_malloc_safe = 1;
	configure();
}

void doboot __P((void))
	__attribute__((__noreturn__));
void via_shutdown __P((void));

/*
 * Set registers on exec.
 * XXX Should clear registers except sp, pc,
 * but would break init; should be fixed soon.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long  stack;
	register_t *retval;
{
#ifdef COMPAT_SUNOS
	extern struct emul emul_sunos;
#endif
	struct frame *frame = (struct frame *) p->p_md.md_regs;

	frame->f_sr = PSL_USERSET;
	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[D0] = 0;
	frame->f_regs[D1] = 0;
	frame->f_regs[D2] = 0;
	frame->f_regs[D3] = 0;
	frame->f_regs[D4] = 0;
	frame->f_regs[D5] = 0;
	frame->f_regs[D6] = 0;
	frame->f_regs[D7] = 0;
	frame->f_regs[A0] = 0;
	frame->f_regs[A1] = 0;
	frame->f_regs[A2] = (int)PS_STRINGS;
	frame->f_regs[A3] = 0;
	frame->f_regs[A4] = 0;
	frame->f_regs[A5] = 0;
	frame->f_regs[A6] = 0;
	frame->f_regs[SP] = stack;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;

	if (fputype) {
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);
	}

#ifdef COMPAT_SUNOS
	/*
	 * SunOS' ld.so does self-modifying code without knowing
	 * about the 040's cache purging needs.  So we need to uncache
	 * writeable executable pages.
	 */
	if (p->p_emul == &emul_sunos)
		p->p_md.md_flags |= MDP_UNCACHE_WX;
	else
		p->p_md.md_flags &= ~MDP_UNCACHE_WX;
#endif
}

int     waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	register int howto;
{
	extern u_long MacOSROMBase;

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx((struct pcb *) curproc->p_addr);

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* kill the panic on that boot away */
		if (curproc == NULL)
			curproc = &proc0;

		waittime = 0;

		/*
		 * Release inodes, sync and unmount the filesystems.
		 */
		vfs_shutdown();

#ifdef notyet
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
#else
# ifdef DIAGNOSTIC
		printf("OpenBSD/mac68k does not trust itself to update the clock on shutdown.\n");
# endif
#endif
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP) {
		savectx(&dumppcb);	/* XXX this goes away soon */
		dumpsys();
	}

	/* Run any shutdown hooks. */
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("System halted.\n\n");
		via_shutdown();
#ifndef MRG_ADB                 /* adb_poweroff is available only when
                                 * the MRG_ADB method isn't used.       */
                adb_poweroff(); /* Shut down machines whose power functions
                                 * are accessed via modified ADB calls. */
#endif
		printf("You may turn the machine off,");
		printf(" or hit any key to reboot.\n");
		(void)cngetc();
	}

	/*
	 * Map ROM where the MacOS likes it, so we can reboot,
	 * hopefully.
	 */
	pmap_map(MacOSROMBase, MacOSROMBase,
		 MacOSROMBase + 4 * 1024 * 1024,
		 VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

	
	printf("rebooting...\n");
	DELAY(1000000);
	doboot();
	/* NOTREACHED */
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long  dumpmag = 0x8fca0101;	/* magic number */
int     dumpsize = 0;		/* pages */
long    dumplo = 0;		/* blocks */

static int	get_max_page __P((void));

static int
get_max_page()
{
	int     i, max = 0;

	for (i = 0; i < numranges; i++) {
		if (high[i] > max)
			max = high[i];
	}
	return max;
}

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space in
 * case there might be a disk label stored there.  If there
 * is extra space, put dump at the end to reduce the chance
 * that swapping trashes it.
 */
void
dumpconf()
{
	int     nblks;
	int     maj;

	if (dumpdev == NODEV)
		return;

	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize) (dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = btoc(get_max_page());

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP NBPG	/* Must be a multiple of pagesize XXX small */
static vm_offset_t dumpspace;

vm_offset_t	reserve_dumppages __P((vm_offset_t));
static int	find_range __P((vm_offset_t));
static int	find_next_range __P((vm_offset_t));

vm_offset_t
reserve_dumppages(p)
	vm_offset_t p;
{
	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

static int
find_range(pa)
	vm_offset_t pa;
{
	int     i;

	for (i = 0; i < numranges; i++) {
		if (low[i] <= pa && pa < high[i])
			return i;
	}
	return -1;
}

static int
find_next_range(pa)
	vm_offset_t pa;
{
	int     i, near, best, t;

	near = -1;
	best = 0x7FFFFFFF;
	for (i = 0; i < numranges; i++) {
		if (low[i] <= pa && pa < high[i])
			return i;
		t = low[i] - pa;
		if (t > 0) {
			if (t < best) {
				near = i;
				best = t;
			}
		}
	}
	return near;
}

void
dumpsys()
{
	unsigned bytes, i, n;
	int     range;
	int     maddr, psize;
	daddr_t blkno;
	int     (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int     error = 0;

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize) (dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable.\n");
		return;
	}
	bytes = get_max_page();
	maddr = 0;
	range = find_range(0);
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	for (i = 0; i < bytes; i += n) {
		/*
		 * Avoid dumping "holes."
		 */
		if ((range == -1) || (i >= high[range])) {
			range = find_next_range(i);
			if (range == -1) {
				error = EIO;
				break;
			}
			n = low[range] - i;
			maddr += n;
			blkno += btodb(n);
			continue;
		}
		/* Print out how many MBs we have to go. */
		n = bytes - i;
		if (n && (n % (1024 * 1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n = BYTES_PER_DUMP;

		(void) pmap_map(dumpspace, maddr, maddr + n, VM_PROT_READ);
		error = (*dump) (dumpdev, blkno, (caddr_t) dumpspace, n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);	/* XXX? */
	}

	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt (clock.c:clkread).
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for fun,
 * we guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int     s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec += clkread();
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

void straytrap __P((int, int));

void
straytrap(pc, evec)
	int     pc;
	int     evec;
{
	printf("unexpected trap; vector offset 0x%x from 0x%x.\n",
	    (int) (evec & 0xfff), pc);
#ifdef DDB
	Debugger();
#endif
}

int    *nofault;

int badaddr __P((caddr_t));

int
badaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile short *) addr;
	nofault = (int *) 0;
	return (0);
}

int
badbaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile u_int8_t *) addr;
	nofault = (int *) 0;
	return (0);
}

int
badwaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile u_int16_t *) addr;
	nofault = (int *) 0;
	return (0);
}

int
badladdr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile u_int32_t *) addr;
	nofault = (int *) 0;
	return (0);
}

void
netintr()
{
#ifdef INET
#if NETHER
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
#endif
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef INET6
	if (netisr & (1 << NETISR_IPV6)) {
		netisr &= ~(1 << NETISR_IPV6);
		ipv6intr();
	}
#endif
#ifdef NETATALK
	if (netisr & (1 << NETISR_ATALK)) {
		netisr &= ~(1 << NETISR_ATALK);
		atintr();
	}
#endif
#ifdef NS
	if (netisr & (1 << NETISR_NS)) {
		netisr &= ~(1 << NETISR_NS);
		nsintr();
	}
#endif
#ifdef ISO
	if (netisr & (1 << NETISR_ISO)) {
		netisr &= ~(1 << NETISR_ISO);
		clnlintr();
	}
#endif
#include "ppp.h"
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_PPP);
		pppintr();
	}
#endif
}

/*
 * Level 7 interrupts can be caused by the keyboard or parity errors.
 */
void	nmihand __P((struct frame));

void
nmihand(frame)
	struct frame frame;
{
	static int nmihanddeep = 0;

	if (nmihanddeep++)
		return;
/*	regdump(&frame, 128);
	dumptrace(); */
#ifdef DIAGNOSTIC
	printf("Panic switch: PC is 0x%x.\n", frame.f_pc);
#endif
#ifdef DDB
	if (db_console)
		Debugger();
#endif
	nmihanddeep = 0;
}

void	dumpmem __P((u_int *, int));

void
regdump(frame, sbytes)
	struct frame *frame;
	int     sbytes;
{
	static int doingdump = 0;
	register int i;
	int     s;

	if (doingdump)
		return;
	s = splhigh();
	doingdump = 1;
	printf("pid = %d, pc = 0x%08x, ", curproc->p_pid, frame->f_pc);
	printf("ps = 0x%08x, ", frame->f_sr);
	printf("sfc = 0x%08x, ", getsfc());
	printf("dfc = 0x%08x\n", getdfc());
	printf("Registers:\n     ");
	for (i = 0; i < 8; i++)
		printf("        %d", i);
	printf("\ndreg:");
	for (i = 0; i < 8; i++)
		printf(" %08x", frame->f_regs[i]);
	printf("\nareg:");
	for (i = 0; i < 8; i++)
		printf(" %08x", frame->f_regs[i + 8]);
	if (sbytes > 0) {
		if (1) {	/* (frame->f_sr & PSL_S) *//* BARF - BG */
			printf("\n\nKernel stack (%08x):",
			    (int) (((int *) frame) - 1));
			dumpmem(((int *) frame) - 1, sbytes);
		} else {
			printf("\n\nUser stack (%08x):", frame->f_regs[15]);
			dumpmem((int *) frame->f_regs[15], sbytes);
		}
	}
	doingdump = 0;
	splx(s);
}

void	dumpmem __P((u_int *, int));

void
dumpmem(ptr, sz)
	register u_int *ptr;
	int     sz;
{
	register int i;

	sz /= 4;
	for (i = 0; i < sz; i++) {
		if ((i & 7) == 0)
			printf("\n%08x: ", (u_int) ptr);
		else
			printf(" ");
		printf("%08x ", *ptr++);
	}
	printf("\n");
}

/*
 * It should be possible to probe for the top of RAM, but Apple has
 * memory structured so that in at least some cases, it's possible
 * for RAM to be aliased across all memory--or for it to appear that
 * there is more RAM than there really is.
 */
int	get_top_of_ram __P((void));

int
get_top_of_ram()
{
	return ((mac68k_machine.mach_memsize * (1024 * 1024)) - 4096);
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int    *name;
	u_int   namelen;
	void   *oldp;
	size_t *oldlenp;
	void   *newp;
	size_t  newlen;
	struct proc *p;
{
	dev_t   consdev;

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

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int     error = ENOEXEC;

#ifdef COMPAT_NOMID
	/* Check to see if MID == 0. */
	if (((struct exec *) epp->ep_hdr)->a_midmag == ZMAGIC)
		return exec_aout_prep_oldzmagic(p, epp);
#endif

#ifdef COMPAT_SUNOS
	{
		extern int sunos_exec_aout_makecmds __P((struct proc *,
			        struct exec_package *));
		if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
			return 0;
	}
#endif
	return error;
}

static char *envbuf = NULL;

/*
 * getenvvars: Grab a few useful variables
 */
void		getenvvars __P((u_long, char *));
static long	getenv __P((char *));

void
getenvvars(flag, buf)
	u_long  flag;
	char   *buf;
{
	extern u_long bootdev, videobitdepth, videosize;
	extern u_long end, esym;
	extern u_long macos_boottime, MacOSROMBase;
	extern long macos_gmtbias;
	int     root_scsi_id;

	/*
         * If flag & 0x80000000 == 0, then we're booting with the old booter
         * and we should freak out.
         */
	if ((flag & 0x80000000) == 0) {
		/* Freak out; print something if that becomes available */
	} else
		envbuf = buf;

	root_scsi_id = getenv("ROOT_SCSI_ID");
	/*
         * For now, we assume that the boot device is off the first controller.
         */
	if (bootdev == 0)
		bootdev = MAKEBOOTDEV(4, 0, 0, root_scsi_id, 0);

	if (boothowto == 0)
		boothowto = getenv("SINGLE_USER");

	/* These next two should give us mapped video & serial */
	/* We need these for pre-mapping graybars & echo, but probably */
	/* only on MacII or LC.  --  XXX */
	/* videoaddr = getenv("MACOS_VIDEO"); */
	/* sccaddr = getenv("MACOS_SCC"); */

	/*
         * The following are not in a structure so that they can be
         * accessed more quickly.
         */
	videoaddr = getenv("VIDEO_ADDR");
	videorowbytes = getenv("ROW_BYTES");
	videobitdepth = getenv("SCREEN_DEPTH");
	videosize = getenv("DIMENSIONS");

	/*
         * More misc stuff from booter.
         */
	mac68k_machine.machineid = getenv("MACHINEID");
	mac68k_machine.mach_processor = getenv("PROCESSOR");
	mac68k_machine.mach_memsize = getenv("MEMSIZE");
	mac68k_machine.do_graybars = getenv("GRAYBARS");
	mac68k_machine.serial_boot_echo = getenv("SERIALECHO");
	mac68k_machine.serial_console = getenv("SERIALCONSOLE");

	mac68k_machine.modem_flags = getenv("SERIAL_MODEM_FLAGS");
	mac68k_machine.modem_cts_clk = getenv("SERIAL_MODEM_HSKICLK");
	mac68k_machine.modem_dcd_clk = getenv("SERIAL_MODEM_GPICLK");
	mac68k_machine.print_flags = getenv("SERIAL_PRINT_FLAGS");
	mac68k_machine.print_cts_clk = getenv("SERIAL_PRINT_HSKICLK");
	mac68k_machine.print_dcd_clk = getenv("SERIAL_PRINT_GPICLK");
	/* Should probably check this and fail if old */
	mac68k_machine.booter_version = getenv("BOOTERVER");

	/*
         * Get end of symbols for kernel debugging
         */
	esym = getenv("END_SYM");
#ifndef SYMTAB_SPACE
	if (esym == 0)
#endif
		esym = (long) &end;

	/* Get MacOS time */
	macos_boottime = getenv("BOOTTIME");

	/* Save GMT BIAS saved in Booter parameters dialog box */
	macos_gmtbias = getenv("GMTBIAS");

	/*
         * Save globals stolen from MacOS
         */

	ROMBase = (caddr_t) getenv("ROMBASE");
	if (ROMBase == (caddr_t) 0) {
		ROMBase = (caddr_t) ROMBASE;
	}
	MacOSROMBase = (unsigned long) ROMBase;
	TimeDBRA = getenv("TIMEDBRA");
	ADBDelay = (u_short) getenv("ADBDELAY");
	HwCfgFlags  = getenv("HWCFGFLAGS");
	HwCfgFlags2 = getenv("HWCFGFLAG2");
	HwCfgFlags3 = getenv("HWCFGFLAG3");
 	ADBReInit_JTBL = getenv("ADBREINIT_JTBL");
 	mrg_ADBIntrPtr = (caddr_t) getenv("ADBINTERRUPT");
}

static char	toupper __P((char));

static char
toupper(c)
	char    c;
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 'A';
	} else {
		return c;
	}
}

static long
getenv(str)
	char   *str;
{
	/*
         * Returns the value of the environment variable "str".
         *
         * Format of the buffer is "var=val\0var=val\0...\0var=val\0\0".
         *
         * Returns 0 if the variable is not there, and 1 if the variable is there
         * without an "=val".
         */

	char   *s, *s1;
	int     val, base;

	s = envbuf;
	while (1) {
		for (s1 = str; *s1 && *s && *s != '='; s1++, s++) {
			if (toupper(*s1) != toupper(*s)) {
				break;
			}
		}
		if (*s1) {	/* No match */
			while (*s) {
				s++;
			}
			s++;
			if (*s == '\0') {	/* Not found */
				/* Boolean flags are FALSE (0) if not there */
				return 0;
			}
			continue;
		}
		if (*s == '=') {/* Has a value */
			s++;
			val = 0;
			base = 10;
			if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
				base = 16;
				s += 2;
			} else
				if (*s == '0') {
					base = 8;
				}
			while (*s) {
				if (toupper(*s) >= 'A' && toupper(*s) <= 'F') {
					val = val * base + toupper(*s) - 'A' + 10;
				} else {
					val = val * base + (*s - '0');
				}
				s++;
			}
			return val;
		} else {	/* TRUE (1) */
			return 1;
		}
	}
}

/*
 * ROM Vector information for calling drivers in ROMs
 *
 * According to information published on the Web by Apple, there have
 * been 9 different ROM families used in the Mac since the introduction
 * of the Lisa/XL through the latest PowerMacs (May 96).  Each family
 * has zero or more version variants and in some cases a version variant
 * may exist in one than one length format.  Generally any one specific
 * Mac will use a common set of routines within the ROM and a model-specific
 * set also in the ROM.  Luckily most of the routines used by BSD fall
 * into the common set and can therefore be defined in the ROM Family.
 * The offset addresses (address minus the ROM Base) of these common routines
 * is the same for all machines which use that ROM.  The offset addresses of
 * the machine-specific routines is generally different for each machine.
 * The machine-specific routines currently used by BSD/mac68k include:
 *       ADB_interrupt, PM_interrpt, ADBBase+130_interrupt,
 *       PMgrOp, jClkNoMem, Egret, InitEgret, and ADBReInit_JTBL
 *
 * It is possible that the routine at "jClkNoMem" is a common routine, but
 * some variation in addresses has been seen.  Also, execept for the very
 * earliest machines which used Egret, the machine-specific value of the
 * Egret routine may be unimportant as the machine-specific InitEgret code
 * seems to always set the OS Trap vector for Egret.
 *
 * Only three of the nine different ROMs are important to BSD/mac68k.
 * All other ROMs are used in early model Macs which are unable to run
 * BSD due to other hardware limitations such as 68000 CPU, no MMU
 * capability, or used only in PowerMacs.  The three that we are interested
 * in are:
 *
 * ROM Family $0178 - used in the II, IIx, IIcx, and SE/30
 *            All machines which use this ROM are now supported by BSD.
 *            There are no machine-dependent routines in these ROMs used by
 *            BSD/mac68k.  This ROM is always 256K in length.
 *
 * ROM Family $067c - used in Classic, Color Classic, Color Classic II,
 *                      IIci, IIsi, IIvi, IIvx, IIfx, LC, LC II, LC III,
 *                      LC III+, LC475, LC520, LC550, LC575, LC580, LC630,
 *                      MacTV, P200, P250, P275, P400/405/410/430, P450,
 *                      P460/466/467, P475/476, P520, P550/560, P575/577/578,
 *                      P580/588, P600, P630/631/635/636/637/638/640, Q605,
 *                      Q610, C610, Q630, C650, Q650, Q700, Q800, Q900, Q950,
 *                      PB140, PB145/145B, PB150, PB160, PB165, PB165c, PB170,
 *                      PB180, PB180c, Duo 210, Duo 230, Duo 250, Duo 270c,
 *                      Duo280, Duo 280c, PB 520/520c/540/540c/550
 *             This is the so-called "Universal" ROM used in almost all 68K
 *             machines. There are machine-dependent and machine-independent
 *             routines used by BSD/mac68k in this ROM, and except for the
 *             PowerBooks and the Duos, this ROM seems to be fairly well
 *             known by BSD/mac68k.  Desktop machines listed here that are
 *             not yet running BSD probably only lack the necessary
 *             addresses for the machine-dependent routines, or are waiting
 *             for IDE disk support.  This ROM is generally 1Meg in length,
 *             however when used in the IIci, IIfx, IIsi, LC, Classic II, and
 *             P400/405/410/430 it is 512K in length, and when used in the
 *             PB 520/520c/540/540c/550 it is 2Meg in length.
 *
 * ROM Family - $077d - used in C660AV/Q660AV, Q840AV
 *             The "Universal" ROM used on the PowerMacs and used in the
 *             68K line for the AV Macs only.  When used in the 68K AV
 *             machines the ROM is 2Meg in length; all uses in the PowerMac
 *             use a length of 4Meg.
 *
 *		Bob Nestor - <rnestor@metronet.com>
 */
static romvec_t romvecs[] =
{
	/* Vectors verified for II, IIx, IIcx, SE/30 */
	{			/* 0 */
		"Mac II class ROMs",
		(caddr_t) 0x40807002,	/* where does ADB interrupt */
		(caddr_t) 0x0,		/* PM interrupt (?) */
		(caddr_t) 0x4080a4d8,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x40807778,	/* CountADBs */
		(caddr_t) 0x40807792,	/* GetIndADB */
		(caddr_t) 0x408077be,	/* GetADBInfo */
		(caddr_t) 0x408077c4,	/* SetADBInfo */
		(caddr_t) 0x40807704,	/* ADBReInit */
		(caddr_t) 0x408072fa,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080d6d0,	/* WriteParam */
		(caddr_t) 0x4080d6fa,	/* SetDateTime */
		(caddr_t) 0x4080dbe8,	/* InitUtil */
		(caddr_t) 0x4080dd78,	/* ReadXPRam */
		(caddr_t) 0x4080dd82,	/* WriteXPRam */
		(caddr_t) 0x4080ddd6,	/* jClkNoMem */
		(caddr_t) 0x0,		/* ADBAlternateInit */
		(caddr_t) 0x0,		/* Egret */
		(caddr_t) 0x0,		/* InitEgret */
		(caddr_t) 0x0,		/* ADBReInit_JTBL */
		(caddr_t) 0x0,		/* ROMResourceMap List Head */
		(caddr_t) 0x40814c58,	/* FixDiv */
		(caddr_t) 0x40814b64,	/* FixMul */
	},
	/*
	 * Vectors verified for PB 140, PB 145, PB 170
	 * (PB 100?)
	 */
	{			/* 1 */
		"Powerbook class ROMs",
		(caddr_t) 0x4088ae5e,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x4088ae0e,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x0,		/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for IIsi, IIvx, IIvi
	 */
	{			/* 2 */
		"Mac IIsi class ROMs",
		(caddr_t) 0x40814912,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x408150f0,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x0,		/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for Mac Classic II and LC II
	 * (Other LC's?  680x0 Performas?)
	 */
	{			/* 3 */
		"Mac Classic II ROMs",
		(caddr_t) 0x40a14912,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x40a150f0,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x40a0a360,	/* CountADBs */
		(caddr_t) 0x40a0a37a,	/* GetIndADB */
		(caddr_t) 0x40a0a3a6,	/* GetADBInfo */
		(caddr_t) 0x40a0a3ac,	/* SetADBInfo */
		(caddr_t) 0x40a0a752,	/* ADBReInit */
		(caddr_t) 0x40a0a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x40a0c05c,	/* WriteParam */
		(caddr_t) 0x40a0c086,	/* SetDateTime */
		(caddr_t) 0x40a0c5cc,	/* InitUtil */
		(caddr_t) 0x40a0b186,	/* ReadXPRam */
		(caddr_t) 0x40a0b190,	/* WriteXPRam */
		(caddr_t) 0x40a0b1e4,	/* jClkNoMem */
		(caddr_t) 0x40a0a818,	/* ADBAlternateInit */
		(caddr_t) 0x40a14800,	/* Egret */
		(caddr_t) 0x40a147c4,	/* InitEgret */
		(caddr_t) 0x40a03ba6,	/* ADBReInit_JTBL */
		(caddr_t) 0x40a7eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x40a1c406,	/* FixDiv, wild guess */
		(caddr_t) 0x40a1c312,	/* FixMul, wild guess */
	},
	/*
	 * Vectors verified for IIci, Q700
	 */
	{			/* 4 */
		"Mac IIci/Q700 ROMs",
		(caddr_t) 0x4080a700,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x4080a5aa,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x0,		/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x0,		/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for Duo 230, PB 180, PB 160, PB 165/165C
	 * (Duo 210?  Duo 250?  Duo 270?)
	 */
	{			/* 5 */
		"2nd Powerbook class ROMs",
		(caddr_t) 0x408b2eec,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x408b2e76,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b39b2,	/* jClkNoMem */	/* From PB180 */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x40888400,	/* InitPwrMgr */ /* From PB180 */
		(caddr_t) 0x408cce28,	/* ADBReInit_JTBL -- from PB160*/
		(caddr_t) 0x4087eb90,	/* ROMRsrcMap List Head -- from PB160*/
		(caddr_t) 0x4081c406,	/* FixDiv, wild guess */
		(caddr_t) 0x4081c312,	/* FixMul, wild guess */
	},
	/*
	 * Vectors verified for the Quadra, Centris 650
	 *  (610, Q800?)
	 */
	{			/* 6 */
		"Quadra/Centris ROMs",
		(caddr_t) 0x408b2dea,	/* ADB int */
		(caddr_t) 0x0,		/* PM intr */
 		(caddr_t) 0x408b2c72,	/* ADBBase + 130 */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x40809ae6,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b39b6,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408d2b64,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv, wild guess */
		(caddr_t) 0x4081c312,	/* FixMul, wild guess */
	},
	/*
	 * Vectors verified for the Quadra 660AV
	 *  (Quadra 840AV?)
	 */
	{			/* 7 */
		"Quadra AV ROMs",
		(caddr_t) 0x4080cac6,	/* ADB int */
		(caddr_t) 0x0,		/* PM int */
		(caddr_t) 0x40805cd4,	/* ADBBase + 130 */
		(caddr_t) 0x40839600,	/* CountADBs */
		(caddr_t) 0x4083961a,	/* GetIndADB */
		(caddr_t) 0x40839646,	/* GetADBInfo */
		(caddr_t) 0x4083964c,	/* SetADBInfo */
		(caddr_t) 0x408397b8,	/* ADBReInit */
		(caddr_t) 0x4083967c,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4081141c,	/* WriteParam */
		(caddr_t) 0x4081144e,	/* SetDateTime */
		(caddr_t) 0x40811930,	/* InitUtil */
		(caddr_t) 0x4080b624,	/* ReadXPRam */
		(caddr_t) 0x4080b62e,	/* WriteXPRam */
		(caddr_t) 0x40806884,	/* jClkNoMem */
		(caddr_t) 0x408398c2,	/* ADBAlternateInit */
		(caddr_t) 0x4080cada,	/* Egret */
		(caddr_t) 0x4080de14,	/* InitEgret */
		(caddr_t) 0x408143b8,	/* ADBReInit_JTBL */
		(caddr_t) 0x409bdb60,	/* ROMResourceMap List Head */
		(caddr_t) 0x4083b3d8,	/* FixDiv */
		(caddr_t) 0x4083b2e4,	/* FixMul */
	},
	/*
	 * PB 540, PB 550
	 * (PB 520?  Duo 280?)
	 */
	{			/* 8 */
		"68040 PowerBook ROMs",
		(caddr_t) 0x400b2efc,	/* ADB int */
		(caddr_t) 0x400d8e66,	/* PM int */
		(caddr_t) 0x400b2e86,	/* ADBBase + 130 */
		(caddr_t) 0x4000a360,	/* CountADBs */
		(caddr_t) 0x4000a37a,	/* GetIndADB */
		(caddr_t) 0x4000a3a6,	/* GetADBInfo */
		(caddr_t) 0x4000a3ac,	/* SetADBInfo */
		(caddr_t) 0x4000a752,	/* ADBReInit */
		(caddr_t) 0x4000a3dc,	/* ADBOp */
		(caddr_t) 0x400d9302,	/* PmgrOp */
		(caddr_t) 0x4000c05c,	/* WriteParam */
		(caddr_t) 0x4000c086,	/* SetDateTime */
		(caddr_t) 0x4000c5cc,	/* InitUtil */
		(caddr_t) 0x4000b186,	/* ReadXPRam */
		(caddr_t) 0x4000b190,	/* WriteXPRam */
		(caddr_t) 0x400b3c08,	/* jClkNoMem */
		(caddr_t) 0x4000a818,	/* ADBAlternateInit */
		(caddr_t) 0x40009ae6,	/* Egret */ /* From PB520 */
		(caddr_t) 0x400147c4,	/* InitEgret */
		(caddr_t) 0x400a7a5c,	/* ADBReInit_JTBL */
		(caddr_t) 0x4007eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4001c406,	/* FixDiv, wild guess */
		(caddr_t) 0x4001c312,	/* FixMul, wild guess */
	},
	/*
	 * Verified for the Q605
	 */
	{			/* 9 */
		"Quadra/Centris 605 ROMs",
		(caddr_t) 0x408a9b56,	/* ADB int */
		(caddr_t) 0x0,		/* PM int */
		(caddr_t) 0x408b2f94,	/* ADBBase + 130 */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PmgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b3bf8,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408a99c0,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408a82c0,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for Duo 270c, PB150
	 */
	{			/* 10 */
		"Duo 270C ROMs",
		(caddr_t) 0x408b2efc,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x408b2e86,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b3bf8,	/* jClkNoMem */ /* from PB 150 */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x0,		/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv, wild guess */
		(caddr_t) 0x4081c312,	/* FixMul, wild guess */
	},
	/*
	 * Vectors verified for Performa/LC 550
	 */
	{			/* 11 */
		"P/LC 550 ROMs",
		(caddr_t) 0x408d16d6,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PB ADB interrupt */
		(caddr_t) 0x408b2f84,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b3c04,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408d1450,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408d24a4,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv for P550 */
		(caddr_t) 0x4081c312,	/* FixMul for P550 */
	},
	/*
	 * Vectors verified for the MacTV
	 */
	{			/* 12 */
		"MacTV ROMs",
		(caddr_t) 0x40acfed6,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PB ADB interrupt */
		(caddr_t) 0x40ab2f84,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x40a0a360,	/* CountADBs */
		(caddr_t) 0x40a0a37a,	/* GetIndADB */	
		(caddr_t) 0x40a0a3a6,	/* GetADBInfo */
		(caddr_t) 0x40a0a3ac,	/* SetADBInfo */
		(caddr_t) 0x40a0a752,	/* ADBReInit */
		(caddr_t) 0x40a0a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x40a0c05c,	/* WriteParam */
		(caddr_t) 0x40a0c086,	/* SetDateTime */
		(caddr_t) 0x40a0c5cc,	/* InitUtil */
		(caddr_t) 0x40a0b186,	/* ReadXPRam */
		(caddr_t) 0x40a0b190,	/* WriteXPRam */
		(caddr_t) 0x40ab3bf4,	/* jClkNoMem */
		(caddr_t) 0x40a0a818,	/* ADBAlternateInit */
		(caddr_t) 0x40acfd40,	/* Egret */
		(caddr_t) 0x40a147c4,	/* InitEgret */
		(caddr_t) 0x40a038a0,	/* ADBReInit_JTBL */
		(caddr_t) 0x40a7eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x40a1c406,	/* FixDiv */
		(caddr_t) 0x40a1c312,	/* FixMul */
	},
	/*
	 * Vectors verified for the Quadra630
	 */
	{			/* 13 */
		"Quadra630 ROMs",
		(caddr_t) 0x408a9bd2,	/* ADB int */
		(caddr_t) 0x0,		/* PM intr */
 		(caddr_t) 0x408b2f94,	/* ADBBase + 130 */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* Wild guess at ReadXPRam */
		(caddr_t) 0x4080b190,	/* Wild guess at WriteXPRam */
		(caddr_t) 0x408b39f4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408a99c0,	/* Egret */
		(caddr_t) 0x408147c8,	/* InitEgret */
		(caddr_t) 0x408a7ef8,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for LC III
	 */
	{			/* 14 */
		"LC III ROMs",
		(caddr_t) 0x40814912,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x408b2f94,	/* ADBBase + 130 interupt */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b39b6,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408d2918,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for the LC520
	 */
	{			/* 15 */
		"MacLC520 ROMs",
		(caddr_t) 0x408d16d6,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PB ADB interrupt */
		(caddr_t) 0x408b2f84,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b3c04,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408d1450,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408d2460,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv for P520 */
		(caddr_t) 0x4081c312,	/* FixMul for P520 */
	},
	/*
	 * Vectors verified for the LC 575/577/578
	 */
	{			/* 16 */
		"MacLC575 ROMs",
		(caddr_t) 0x408a9b56,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PB ADB interrupt */
		(caddr_t) 0x408b2f94,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b3bf8,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408a99c0,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408a81a0,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv for P520 */
		(caddr_t) 0x4081c312,	/* FixMul for P520 */
	},
	/*
	 * Vectors verified for the Quadra 950
	 */
	{			/* 17 */
		"Quadra950 class ROMs",
		(caddr_t) 0x40814912,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x4080a4d8,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x40814800,	/* Egret */
		(caddr_t) 0x408147c4,	/* InitEgret */
		(caddr_t) 0x408038bc,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/*
	 * Vectors verified for the Mac IIfx
	 */
	{			/* 18 */
		"Mac IIfx ROMs",
		(caddr_t) 0x40809f4a,	/* ADB interrupt */
		(caddr_t) 0x0,		/* PM ADB interrupt */
		(caddr_t) 0x4080a4d8,	/* ADBBase + 130 interupt */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x0,		/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x0,		/* Egret */
		(caddr_t) 0x0,		/* InitEgret */
		(caddr_t) 0x408037c0,	/* ADBReInit_JTBL */
		(caddr_t) 0x4087eb90,	/* ROMResourceMap List Head */
		(caddr_t) 0x4081c406,	/* FixDiv */
		(caddr_t) 0x4081c312,	/* FixMul */
	},
	/* Please fill these in! -BG */
};


struct cpu_model_info cpu_models[] = {

/* The first four. */
	{MACH_MACII, "II ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACIIX, "IIx ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACIICX, "IIcx ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACSE30, "SE/30 ", "", MACH_CLASSII, &romvecs[0]},

/* The rest of the II series... */
	{MACH_MACIICI, "IIci ", "", MACH_CLASSIIci, &romvecs[4]},
	{MACH_MACIISI, "IIsi ", "", MACH_CLASSIIsi, &romvecs[2]},
	{MACH_MACIIVI, "IIvi ", "", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACIIVX, "IIvx ", "", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACIIFX, "IIfx ", "", MACH_CLASSIIfx, &romvecs[18]},

/* The Centris/Quadra series. */
	{MACH_MACQ700, "Quadra", " 700 ", MACH_CLASSQ, &romvecs[4]},
	{MACH_MACQ900, "Quadra", " 900 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ950, "Quadra", " 950 ", MACH_CLASSQ, &romvecs[17]},
	{MACH_MACQ800, "Quadra", " 800 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ650, "Quadra", " 650 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACC650, "Centris", " 650 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ605, "Quadra", " 605 ", MACH_CLASSQ, &romvecs[9]},
	{MACH_MACQ605_33, "Quadra", " 605/33 ", MACH_CLASSQ, &romvecs[9]},
	{MACH_MACC610, "Centris", " 610 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ610, "Quadra", " 610 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ630, "Quadra", " 630 ", MACH_CLASSQ2, &romvecs[13]},
	{MACH_MACC660AV, "Centris", " 660AV ", MACH_CLASSAV, &romvecs[7]},
	{MACH_MACQ840AV, "Quadra", " 840AV ", MACH_CLASSAV, &romvecs[7]},

/* The Powerbooks/Duos... */
	{MACH_MACPB100, "PowerBook", " 100 ", MACH_CLASSPB, &romvecs[1]},
	/* PB 100 has no MMU! */
	{MACH_MACPB140, "PowerBook", " 140 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB145, "PowerBook", " 145 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB150, "PowerBook", " 150 ", MACH_CLASSDUO, &romvecs[10]},
	{MACH_MACPB160, "PowerBook", " 160 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB165, "PowerBook", " 165 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB165C, "PowerBook", " 165c ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB170, "PowerBook", " 170 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB180, "PowerBook", " 180 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB180C, "PowerBook", " 180c ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB500, "PowerBook", " 500 ", MACH_CLASSPB, &romvecs[8]},

/* The Duos */
	{MACH_MACPB210, "PowerBook Duo", " 210 ", MACH_CLASSDUO, &romvecs[5]},
	{MACH_MACPB230, "PowerBook Duo", " 230 ", MACH_CLASSDUO, &romvecs[5]},
	{MACH_MACPB250, "PowerBook Duo", " 250 ", MACH_CLASSDUO, &romvecs[5]},
	{MACH_MACPB270, "PowerBook Duo", " 270C ", MACH_CLASSDUO, &romvecs[5]},
	{MACH_MACPB280, "PowerBook Duo", " 280 ", MACH_CLASSDUO, &romvecs[5]},
	{MACH_MACPB280C, "PowerBook Duo", " 280C ", MACH_CLASSDUO, &romvecs[5]},

/* The Performas... */
	{MACH_MACP600, "Performa", " 600 ", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACP460, "Performa", " 460 ", MACH_CLASSLC, &romvecs[14]},
	{MACH_MACP550, "Performa", " 550 ", MACH_CLASSLC, &romvecs[11]},
	{MACH_MACTV,   "TV ",      "",      MACH_CLASSLC, &romvecs[12]},

/* The LCs... */
	{MACH_MACLCII,  "LC", " II ",  MACH_CLASSLC, &romvecs[3]},
	{MACH_MACLCIII, "LC", " III ", MACH_CLASSLC, &romvecs[14]},
	{MACH_MACLC475, "LC", " 475 ", MACH_CLASSQ,  &romvecs[9]},
	{MACH_MACLC475_33, "LC", " 475/33 ", MACH_CLASSQ,  &romvecs[9]},
	{MACH_MACLC520, "LC", " 520 ", MACH_CLASSLC, &romvecs[15]},
	{MACH_MACLC575, "LC", " 575 ", MACH_CLASSQ2, &romvecs[16]},
	{MACH_MACCCLASSIC, "Color Classic ", "", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACCCLASSICII, "Color Classic"," II ", MACH_CLASSLC, &romvecs[3]},
/* Does this belong here? */
	{MACH_MACCLASSICII, "Classic", " II ", MACH_CLASSLC, &romvecs[3]},

/* The unknown one and the end... */
	{0, "Unknown", "", MACH_CLASSII, NULL},
	{0, NULL, NULL, 0, NULL},
};				/* End of cpu_models[] initialization. */

struct {
	int	machineid;
	caddr_t	fbbase;
	u_long	fblen;
} intvid_info[] =  {
	{ MACH_MACCLASSICII,	(caddr_t)0xfee09a80,	21888 },
	{ MACH_MACPB140,	(caddr_t)0xfee00000,	32 * 1024 },
	{ MACH_MACPB145,	(caddr_t)0xfee00000,	32 * 1024 },
	{ MACH_MACPB170,	(caddr_t)0xfee00000,	32 * 1024 },
	{ MACH_MACPB150,	(caddr_t)0x60000000,	128 * 1024 },
	{ MACH_MACPB160,	(caddr_t)0x60000000,	128 * 1024 },
	{ MACH_MACPB165,	(caddr_t)0x60000000,	128 * 1024 },
	{ MACH_MACPB180,	(caddr_t)0x60000000,	128 * 1024 },
	{ MACH_MACCCLASSIC,	(caddr_t)0x50f40000,	512 * 1024 },
	{ MACH_MACPB165C,	(caddr_t)0xfc040000,	512 * 1024 },
	{ MACH_MACPB180C,	(caddr_t)0xfc040000,	512 * 1024 },
	{ MACH_MACPB500,	(caddr_t)0x60000000,	512 * 1024 },
	{ MACH_MACLC520,	(caddr_t)0x60000000,	1024 * 1024 },
	{ MACH_MACLC475,	(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACLC475_33,	(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACLC575,	(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACC610,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACC650,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ605,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ605_33,	(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ610,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ630,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ650,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ700,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ800,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ900,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ MACH_MACQ950,		(caddr_t)0xf9000000,	1024 * 1024 },
	{ 0,			(caddr_t)0x0,		0 },
};				/* End of intvid_info[] initialization. */

/*
 * Missing Mac Models:
 *	PowerMac 6100
 *	PowerMac 7100
 *	PowerMac 8100
 *	PowerBook 540
 *	PowerBook 520
 *	PowerBook 150
 *	Duo 280
 *	Performa 6000s
 * 	...?
 */

char    cpu_model[120];		/* for sysctl() */

int	mach_cputype __P((void));

int
mach_cputype()
{
	return (mac68k_machine.mach_processor);
}

static void
identifycpu()
{
	extern u_int delay_factor;
	char *mpu;

	switch (cputype) {
	case CPU_68020:
		mpu = ("(68020)");
		break;
	case CPU_68030:
		mpu = ("(68030)");
		break;
	case CPU_68040:
		mpu = ("(68040)");
		break;
	default:
		mpu = ("(unknown processor)");
		break;
	}
	sprintf(cpu_model, "Apple Macintosh %s%s %s",
	    cpu_models[mac68k_machine.cpu_model_index].model_major,
	    cpu_models[mac68k_machine.cpu_model_index].model_minor,
	    mpu);
	printf("%s\n", cpu_model);
	printf("cpu: delay factor %d\n", delay_factor);
}

static void	get_machine_info __P((void));

static void
get_machine_info()
{
	int     i;

	for (i = 0; cpu_models[i].model_major; i++)
		if (mac68k_machine.machineid == cpu_models[i].machineid)
			break;

	if (cpu_models[i].model_major == NULL)
		i--;

	mac68k_machine.cpu_model_index = i;
}

struct cpu_model_info *current_mac_model;
romvec_t *mrg_MacOSROMVectors = 0;

/*
 * Sets a bunch of machine-specific variables
 */
void	setmachdep __P((void));

void
setmachdep()
{
	static int firstpass = 1;
	int setup_mrg_vectors = 0;
	struct cpu_model_info *cpui;
	int i;

	/*
	 * First, set things that need to be set on the first pass only
	 * Ideally, we'd only call this once, but for some reason, the
	 * VIAs need interrupts turned off twice !?
	 */
	if (firstpass) {
		get_machine_info();

		load_addr = 0;
	}
	cpui = &(cpu_models[mac68k_machine.cpu_model_index]);
	current_mac_model = cpui;

	if (!firstpass)
		return;

	/*
	 * Set up any machine specific stuff that we have to before
	 * ANYTHING else happens
	 */
	switch (cpui->class) {	/* Base this on class of machine... */
	case MACH_CLASSII:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		setup_mrg_vectors = 1;
		break;
	case MACH_CLASSPB:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		/* Disable everything but PM; we need it. */
		via_reg(VIA1, vIER) = 0x6f;	/* disable VIA1 int */
		/* Are we disabling something important? */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		if (cputype == CPU_68040)
			mac68k_machine.sonic = 1;
		break;
	case MACH_CLASSDUO:
		/*
		 * The Duo definitely does not use a VIA2, but it looks
		 * like the VIA2 functions might be on the MSC at the RBV
		 * locations.  The rest is copied from the Powerbooks.
		 */
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		/* Disable everything but PM; we need it. */
		via_reg(VIA1, vIER) = 0x6f;	/* disable VIA1 int */
		/* Are we disabling something important? */
		via_reg(VIA2, rIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSQ:
        case MACH_CLASSQ2:
		mac68k_machine.sonic = 1;
	case MACH_CLASSAV:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi96 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSIIci:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIsi:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIvx:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSLC:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIfx:
		VIA2 = 0xd;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.zs_chip = 0;
		mac68k_machine.sccClkConst = 115200;	/* XXX unverified */
		via_reg(VIA1, vIER) = 0x7f;  /* disable VIA1 int */
		break;
	default:
	case MACH_CLASSH:
		break;
	}

	/*
	 * Set `internal' framebuffer location and length, if we know 
	 * what they are.
	 */
	for (i = 0; intvid_info[i].machineid; i++) {
		if (mac68k_machine.machineid == intvid_info[i].machineid) {
			mac68k_vidlog = mac68k_vidphys =
			    (u_int32_t) intvid_info[i].fbbase;
			mac68k_vidlen = (u_int32_t) intvid_info[i].fblen;
			break;
		}
	}

	/*
	 * Set up current ROM Glue vectors.  Actually now all we do
	 * is save the address of the ROM Glue Vector table. This gets
	 * used later when we re-map the vectors from MacOS Address
	 * Space to BSD Address Space.
	 */
	if ((mac68k_machine.serial_console & 0x03) == 0 || setup_mrg_vectors)
		mrg_MacOSROMVectors = cpui->rom_vectors;

	firstpass = 0;
}

/*
 * Set IO offsets.
 */
void
mac68k_set_io_offsets(base)
	vm_offset_t base;
{
	extern volatile u_char *sccA;

	/*
	 * Initialize the I/O mem extent map.
	 * Note: we don't have to check the return value since
	 * creation of a fixed extent map will never fail (since
	 * descriptor storage has already been allocated).
	 *
	 * N.B. The iomem extent manages _all_ physical addresses
	 * on the machine.  When the amount of RAM is found, all
	 * extents of RAM are allocated from the map.
	 */
	iomem_ex = extent_create("iomem", 0x0, 0xffffffff, M_DEVBUF,
	    (caddr_t)iomem_ex_storage, sizeof(iomem_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);

	switch (current_mac_model->class) {
	case MACH_CLASSQ:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0xc000;
		switch (current_mac_model->machineid) {
		case MACH_MACQ900:
		case MACH_MACQ950:
			mac68k_machine.scsi96_2 = 1;
		case MACH_MACQ700:
			SCSIBase = base + 0xf000;
			break;
		default:
			SCSIBase = base + 0x10000;
			break;
		}
		break;
	case MACH_CLASSQ2:
		/*
		 * Note the different offset for sccA for this class of
		 * machines.  This seems to be common on many of the
		 * Quadra-type machines.
		 */
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0xc020;
		SCSIBase = base + 0x10000;
		break;
	case MACH_CLASSAV:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		SCSIBase = base + 0x18000;
		PSCBase = (volatile u_char *) base + 0x31000;
		mac68k_bioipl = PSL_S | PSL_IPL4;
		mac68k_netipl = PSL_S | PSL_IPL4;
		mac68k_impipl = PSL_S | PSL_IPL4;
		mac68k_statclockipl = PSL_S | PSL_IPL4;
		break;
	case MACH_CLASSII:
	case MACH_CLASSPB:
	case MACH_CLASSDUO:
	case MACH_CLASSIIci:
	case MACH_CLASSIIsi:
	case MACH_CLASSIIvx:
	case MACH_CLASSLC:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		SCSIBase = base;
		break;
	case MACH_CLASSIIfx:
		/*
		 * Note that sccA base address is based on having
		 * the serial port in `compatible' mode (set in
		 * the Serial Switch control panel before booting).
		 */
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4020;
		SCSIBase = base;
		break;
	default:
	case MACH_CLASSH:
		panic("Unknown/unsupported machine class (%d).",
		    current_mac_model->class);
		break;
	}
	Via2Base = Via1Base + 0x2000 * VIA2;
}

#if GRAYBARS
static u_long gray_nextaddr = 0;

void
gray_bar()
{
	static int i = 0;
	static int flag = 0;

/* MF basic premise as I see it:
	1) Save the scratch regs as they are not saved by the compilier.
   	2) Check to see if we want gray bars, if so,
		display some lines of gray,
		a couple of lines of white(about 8),
		and loop to slow this down.
   	3) restore regs
*/

	__asm __volatile ("	movl a0,sp@-;
				movl a1,sp@-;
				movl d0,sp@-;
				movl d1,sp@-");

/* check to see if gray bars are turned off */
	if (mac68k_machine.do_graybars) {
		/* MF the 10*rowbytes/4 is done lots, but we want this to be
		 * slow */
		for (i = 0; i < 10 * videorowbytes / 4; i++)
			((u_long *) videoaddr)[gray_nextaddr++] = 0xaaaaaaaa;
		for (i = 0; i < 2 * videorowbytes / 4; i++)
			((u_long *) videoaddr)[gray_nextaddr++] = 0x00000000;
	}

	__asm __volatile ("	movl sp@+,d1;
				movl sp@+,d0;
				movl sp@+,a1;
				movl sp@+,a0");
}
#endif

/* in locore */
extern u_long ptest040 __P((caddr_t addr, u_int fc));
extern int get_pte __P((u_int addr, u_long pte[2], u_short * psr));

/*
 * LAK (7/24/94): given a logical address, puts the physical address
 *  in *phys and return 1, or returns 0 on failure.  This is intended
 *  to look through MacOS page tables.
 */

static u_long
get_physical(u_int addr, u_long * phys)
{
	u_long  pte[2], ph, mask;
	u_short psr;
	int     i, numbits;
	extern u_int macos_tc;

	if (mmutype == MMU_68040) {
		ph = ptest040((caddr_t) addr, FC_SUPERD);
		if ((ph & MMU40_RES) == 0) {
			ph = ptest040((caddr_t) addr, FC_USERD);
			if ((ph & MMU40_RES) == 0)
				return 0;
		}

		mask = (macos_tc & 0x4000) ? 0x00001fff : 0x00000fff;
		ph &= (~mask);
	} else {
		i = get_pte(addr, pte, &psr);

		switch (i) {
		case -1:
			return 0;
		case 0:
			ph = pte[0] & 0xFFFFFF00;
			break;
		case 1:
			ph = pte[1] & 0xFFFFFF00;
			break;
		default:
			panic("get_physical(): bad get_pte()");
		}

		/*
		 * We must now figure out how many levels down we went and
		 * mask the bits appropriately -- the returned value may only
		 * be the upper n bits, and we have to take the rest from addr.
		 */
		numbits = 0;
		psr &= 0x0007;		/* Number of levels we went */
		for (i = 0; i < psr; i++)
			numbits += (macos_tc >> (12 - i * 4)) & 0x0f;

		/*
		 * We have to take the most significant "numbits" from
		 * the returned value "ph", and the rest from our addr.
		 * Assume that numbits != 0.
		 */
		mask = (1 << (32 - numbits)) - 1;
	}
	*phys = ph + (addr & mask);

	return 1;
}

static void	check_video __P((char *, u_long, u_long));

static void
check_video(id, limit, maxm)
	char    *id;
	u_long  limit, maxm;
{
	u_long  addr, phys;

	if (!get_physical(videoaddr, &phys))
		printf("get_mapping(): %s.  False start.\n", id);
	else {
		mac68k_vidlog = videoaddr;
		mac68k_vidphys = phys;
		mac68k_vidlen = 32768;
		addr = videoaddr + 32768;
		while (get_physical(addr, &phys)) {
			if ((phys - mac68k_vidphys)
			    != mac68k_vidlen)
				break;
			if (mac68k_vidlen + 32768 > limit) {
				printf("mapping: %s.  Does it never end?\n",
				    id);
				printf("               Forcing VRAM size ");
				printf("to a conservative %ldK.\n", maxm/1024);
				mac68k_vidlen = maxm;
				break;
			}
			mac68k_vidlen += 32768;
			addr += 32768;
		}
		printf("  %s internal video at addr 0x%x (phys 0x%x), ",
		    id, mac68k_vidlog, mac68k_vidphys);
		printf("len 0x%x.\n", mac68k_vidlen);
	}
}

/*
 * Find out how MacOS has mapped itself so we can do the same thing.
 * Returns the address of logical 0 so that locore can map the kernel
 * properly.
 */
u_int
get_mapping(void)
{
	int     i, same;
	u_long  addr, lastpage, phys, len;

	numranges = 0;
	for (i = 0; i < 8; i++) {
		low[i] = 0;
		high[i] = 0;
	}

	lastpage = get_top_of_ram();

	get_physical(0, &load_addr);

	for (addr = 0; addr <= lastpage && get_physical(addr, &phys);
	    addr += NBPG) {
		if (numranges > 0 && phys == high[numranges - 1]) {
			high[numranges - 1] += NBPG;
		} else {
			numranges++;
			low[numranges - 1] = phys;
			high[numranges - 1] = phys + NBPG;
		}
	}
#if 1
	printf("System RAM: %ld bytes in %ld pages.\n", addr, addr / NBPG);
	for (i = 0; i < numranges; i++) {
		printf("     Low = 0x%lx, high = 0x%lx\n", low[i], high[i]);
	}
#endif

	/*
	 * We should now look through all of NuBus space to find where
	 * the internal video is being mapped.  Just to be sure we handle
	 * all the cases, we simply map our NuBus space exactly how
	 * MacOS did it.  As above, we find a bunch of ranges that are
	 * contiguously mapped.  Since there are a lot of pages that
	 * are all mapped to 0, we handle that as a special case where
	 * the length is negative.  We search in increments of 32768
	 * because that's the page size that MacOS uses.
	 */

	nbnumranges = 0;
	for (i = 0; i < NBMAXRANGES; i++) {
		nbphys[i] = 0;
		nblog[i] = 0;
		nblen[i] = 0;
	}

	same = 0;
	for (addr = 0xF9000000; addr < 0xFF000000; addr += 32768) {
		if (!get_physical(addr, &phys)) {
			continue;
		}
		len = nbnumranges == 0 ? 0 : nblen[nbnumranges - 1];

#if 0
		printf ("0x%lx --> 0x%lx\n", addr, phys);
#endif
		if (nbnumranges > 0
		    && addr == nblog[nbnumranges - 1] + len
		    && phys == nbphys[nbnumranges - 1]) {	/* Same as last one */
			nblen[nbnumranges - 1] += 32768;
			same = 1;
		} else {
			if ((nbnumranges > 0)
			    && !same
			    && (addr == nblog[nbnumranges - 1] + len)
			    && (phys == nbphys[nbnumranges - 1] + len)) {
				nblen[nbnumranges - 1] += 32768;
			} else {
				if (same) {
					nblen[nbnumranges - 1] = -len;
					same = 0;
				}
				if (nbnumranges == NBMAXRANGES) {
					printf("get_mapping(): Too many NuBus "
					    "ranges.\n");
					break;
				}
				nbnumranges++;
				nblog[nbnumranges - 1] = addr;
				nbphys[nbnumranges - 1] = phys;
				nblen[nbnumranges - 1] = 32768;
			}
		}
	}
	if (same) {
		nblen[nbnumranges - 1] = -nblen[nbnumranges - 1];
		same = 0;
	}
#if 0
	printf("Non-system RAM (nubus, etc.):\n");
	for (i = 0; i < nbnumranges; i++) {
		printf("     Log = 0x%lx, Phys = 0x%lx, Len = 0x%lx (%lu)\n",
		    nblog[i], nbphys[i], nblen[i], nblen[i]);
	}
#endif

	/*
	 * We must now find the logical address of internal video in the
	 * ranges we made above.  Internal video is at physical 0, but
	 * a lot of pages map there.  Instead, we look for the logical
	 * page that maps to 32768 and go back one page.
	 */

	for (i = 0; i < nbnumranges; i++) {
		if (nblen[i] > 0
		    && nbphys[i] <= 32768
		    && 32768 <= nbphys[i] + nblen[i]) {
			mac68k_vidlog = nblog[i] - nbphys[i];
			mac68k_vidlen = nblen[i] + nbphys[i];
			mac68k_vidphys = 0;
			break;
		}
	}
	if (i == nbnumranges) {
		if (0x60000000 <= videoaddr && videoaddr < 0x70000000) {
			printf("Checking for Internal Video ");
			/*
			 * Kludge for IIvx internal video (60b0 0000).
			 * PB 520 (6000 0000)
			 */
			check_video("PB/IIvx (0x60?00000)", 1 * 1024 * 1024,
						   1 * 1024 * 1024);
		} else if (0x50F40000 <= videoaddr && videoaddr < 0x50FBFFFF) {
			/*
			 * Kludge for LC internal video
			 */
			check_video("LC video (0x50f40000)",
					512 * 1024, 512 * 1024);
		} else if (0x50100100 <= videoaddr && videoaddr < 0x50400000) {
			/*
			 * Kludge for AV internal video
			 */
			check_video("AV video (0x50100100)", 1 * 1024 * 1024,
						1 * 1024 * 1024);
		} else {
			printf( "  no internal video at address 0 -- "
				"videoaddr is 0x%lx.\n", videoaddr);
		}
	} else {
		printf("  Video address = 0x%lx\n", videoaddr);
		printf("  Int video starts at 0x%x\n",
		    mac68k_vidlog);
		printf("  Length = 0x%x (%d) bytes\n",
		    mac68k_vidlen, mac68k_vidlen);
	}

	return low[0];		/* Return physical address of logical 0 */
}

/*
 * Debugging code for locore page-traversal routine.
 */
void printstar __P((void));
void
printstar(void)
{
	/*
	 * Be careful as we assume that no registers are clobbered
	 * when we call this from assembly.
	 */
	__asm __volatile ("	movl a0,sp@-;
				movl a1,sp@-;
				movl d0,sp@-;
				movl d1,sp@-");

	/* printf("*"); */

	__asm __volatile ("	movl sp@+,d1;
				movl sp@+,d0;
				movl sp@+,a1;
				movl sp@+,a0");
}

/*
 * Console bell callback; modularizes the console terminal emulator
 * and the audio system, so neither requires direct knowledge of the
 * other.
 */

void
mac68k_set_bell_callback(callback, cookie)
	int (*callback) __P((void *, int, int, int));
	void *cookie;
{
	mac68k_bell_callback = callback;
	mac68k_bell_cookie = (caddr_t) cookie;
}

int
mac68k_ring_bell(freq, length, volume)
	int freq, length, volume;
{
	if (mac68k_bell_callback)
		return ((*mac68k_bell_callback)(mac68k_bell_cookie,
		    freq, length, volume));
	else
		return (ENXIO);
}
