/*	$OpenBSD: machdep.c,v 1.158 2011/06/05 19:41:07 deraadt Exp $	*/
/*	$NetBSD: machdep.c,v 1.207 1998/07/08 04:39:34 thorpej Exp $	*/

/*
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
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/vnode.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <sys/mount.h>
#include <sys/extent.h>
#include <sys/syscallargs.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#include <ddb/db_var.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/kcore.h>
#include <machine/bus.h>
#include <machine/pmap.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_swap.h>

#include <sys/sysctl.h>

#include <dev/cons.h>
#include <mac68k/dev/adbvar.h>

#include <machine/iop.h>
#include <machine/psc.h>
#include <machine/viareg.h>

#include "wsdisplay.h"

/* The following is used externally (sysctl_hw) */
char	machine[] = MACHINE;	/* from <machine/param.h> */

struct mac68k_machine_S mac68k_machine;

volatile u_char *Via1Base, *Via2Base, *PSCBase = NULL;
u_long  NuBusBase = NBBASE;
u_long  IOBase;

vaddr_t SCSIBase;

/* These are used to map kernel space: */
#define	NBMEMRANGES	8
extern int numranges;
extern u_long low[NBMEMRANGES];
extern u_long high[NBMEMRANGES];

/* These are used to map NuBus space: */
#define	NBMAXRANGES	16
int	nbnumranges;		/* = 0 == don't use the ranges */
u_long	nbphys[NBMAXRANGES];	/* Start physical addr of this range */
u_long	nblog[NBMAXRANGES];	/* Start logical addr of this range */
long	nblen[NBMAXRANGES];	/* Length of this range If the length is */
				/* negative, all phys addrs are the same. */

/* From Booter via locore */
long	videoaddr;		/* Addr used in kernel for video. */
long	videorowbytes;		/* Used in kernel for video. */
long	videobitdepth;		/* Number of bihs per pixel */
u_long	videosize;		/* height = 31:16, width 15:0 */

/*
 * Values for IIvx-like internal video
 * -- should be zero if it is not used (usual case).
 */
u_int32_t mac68k_vidlog;	/* logical addr */
u_int32_t mac68k_vidphys;	/* physical addr */
u_int32_t mac68k_vidlen;	/* mem length */

/* Callback and cookie to run bell */
int	(*mac68k_bell_callback)(void *, int, int, int);
caddr_t	mac68k_bell_cookie;

struct vm_map *exec_map = NULL;  
struct vm_map *phys_map = NULL;

int	physmem;		/* size of physical memory, in pages */

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

/*
 * Extent maps to manage all memory space, including I/O ranges.  Allocate
 * storage for 8 regions in each, initially.  Later, iomem_malloc_safe
 * will indicate that it's safe to use malloc() to dynamically allocate
 * region descriptors.
 *
 * The extent maps are not static!  Machine-dependent NuBus and on-board
 * I/O routines need access to them for bus address space allocation.
 */
long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
struct	extent *iomem_ex;
int	iomem_malloc_safe;

/* XXX should be in locore.s for consistency */
int	astpending = 0;

void	identifycpu(void);
u_long	get_physical(u_int, u_long *);

void	initcpu(void);
int	cpu_dumpsize(void);
int	cpu_dump(int (*)(dev_t, daddr64_t, caddr_t, size_t), daddr64_t *);
void	cpu_init_kcore_hdr(void);
int	fpu_probe(void);

/* functions called from locore.s */
void	dumpsys(void);
void	mac68k_init(void);
void	straytrap(int, int);
void	nmihand(struct frame);

/*
 * Machine-dependent crash dump header info.
 */
cpu_kcore_hdr_t cpu_kcore_hdr;

 /*
 * Early initialization, before main() is called.
 */
void
mac68k_init()
{
	int i;
	extern vaddr_t avail_start;

	/*
	 * Tell the VM system about available physical memory.
	 */
	for (i = 0; i < numranges; i++) {
		if (low[i] <= avail_start && avail_start < high[i])
			uvm_page_physload(atop(avail_start), atop(high[i]),
			    atop(avail_start), atop(high[i]), 0);
		else
			uvm_page_physload(atop(low[i]), atop(high[i]),
			    atop(low[i]), atop(high[i]), 0);
	}

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

	/* Initialize the interrupt handlers. */
	intr_init();

	/* Initialize the VIAs */
	via_init();

	/* Initialize the PSC (if present) */
	psc_init();
}

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
	 *	Can be called from locore.s and init_main.c.  (Ugh.)
	 */
	static int init;	/* = 0 */

	if (!init) {
		cninit();
#ifdef DDB
		/*
		 * Initialize kernel debugger, if compiled in.
		 */
		ddb_init();
#endif
		init = 1;
	} else {
#if NWSDISPLAY > 0
		/*
		 * XXX  This is an evil hack on top of an evil hack!
		 *
		 * With the graybar stuff, we've got a catch-22:  we need
		 * to do at least some console setup really early on, even
		 * before we're running with the mappings we need.  On
		 * the other hand, we're not nearly ready to do anything
		 * with wscons or the ADB driver at that point.
		 *
		 * To get around this, wscninit() ignores the first call
		 * it gets (from cninit(), if not on a serial console).
		 * Once we're here, we call wscninit() again, which sets
		 * up the console devices and does the appropriate wscons
		 * initialization.
		 */
		if (mac68k_machine.serial_console == 0) {
			cons_decl(ws);
			wscninit(NULL);
		}
#endif

		mac68k_calibrate_delay();

#if NZSC > 0 && defined(KGDB)
		zs_kgdb_init();
#endif

		if (boothowto & RB_KDB) {
#ifdef KGDB
			/* XXX - Ask on console for kgdb_dev? */
			/* Note: this will just return if kgdb_dev==NODEV */
			kgdb_connect(1);
#else	/* KGDB */
#ifdef DDB
			/* Enter DDB.  We don't have a monitor PROM. */
			Debugger();
#endif /* DDB */
#endif	/* KGDB */
		}
	}
}

#define CURRENTBOOTERVER	111

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup(void)
{
	unsigned i;
	int vers;
	vaddr_t minaddr, maxaddr;
	int delay;

	/*
	 * Initialize the kernel crash dump header.
	 */
	cpu_init_kcore_hdr();

	/*
	 * Initialize error message buffer (at end of core).
	 * high[numranges-1] was decremented in pmap_bootstrap.
	 */
	for (i = 0; i < atop(MSGBUFSIZE); i++)
		pmap_enter(pmap_kernel(), (vaddr_t)msgbufp + i * NBPG,
		    high[numranges - 1] + i * NBPG,
		    VM_PROT_READ|VM_PROT_WRITE,
		    VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();

	vers = mac68k_machine.booter_version;
	if (vers < CURRENTBOOTERVER) {
		/* fix older booters with indices, not versions */
		if (vers < 100)
			vers += 99;

		printf("\nYou booted with booter version %d.%d.\n",
		    vers / 100, vers % 100);
		printf("Booter version %d.%d is necessary to fully support\n",
		    CURRENTBOOTERVER / 100, CURRENTBOOTERVER % 100);
		printf("this kernel.\n\n");
		for (delay = 0; delay < 1000000; delay++);
	}
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024 / 1024);

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

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

	/* Safe for extent allocation to use malloc now. */
	iomem_malloc_safe = 1;
}

void
initcpu()
{
#if defined(M68040)
	extern void (*vectab[256])(void);
	void addrerr4060(void);
#endif
#ifdef M68040
	void buserr40(void);
#endif
#ifdef FPSP
	extern u_long fpvect_tab, fpvect_end, fpsp_tab;
#endif

	switch (cputype) {
#ifdef M68040
	case CPU_68040:
		vectab[2] = buserr40;
		vectab[3] = addrerr4060;
#ifdef FPSP
		bcopy(&fpsp_tab, &fpvect_tab,
		    (&fpvect_end - &fpvect_tab) * sizeof (fpvect_tab));
#endif
		break;
#endif
	default:
		break;
	}

	DCIS();
}

void doboot(void)
	__attribute__((__noreturn__));

int	waittime = -1;

void
boot(howto)
	int howto;
{
	extern u_long maxaddr;

	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(&curproc->p_addr->u_pcb);

	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();

		if (mac68k_machine.aux_interrupts != 0) {
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
		} else {
#ifdef DIAGNOSTIC
			printf("OpenBSD/mac68k does not trust itself to update"
			    " the clock on shutdown on this machine.\n");
#endif
		}
	}

	uvm_shutdown();
	splhigh();			/* Disable interrupts. */

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP) {
		dumpsys();
	}

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN) {
			printf("\nAttempting to power down...\n");
			via_powerdown();
			/*
			 * Shut down machines whose power functions
			 * are accessed via modified ADB calls.
			 */
			adb_poweroff();
		}
		printf("\nThe operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cnpollc(1);
		(void)cngetc();
		cnpollc(0);
	}

	/* Map the last physical page VA = PA for doboot() */
	pmap_enter(pmap_kernel(), (vaddr_t)maxaddr, (vaddr_t)maxaddr,
	    VM_PROT_ALL, VM_PROT_ALL|PMAP_WIRED);
	pmap_update(pmap_kernel());

	printf("rebooting...\n");
	DELAY(1000000);
	doboot();
	/* NOTREACHED */
}

/*
 * Initialize the kernel crash dump header.
 */
void
cpu_init_kcore_hdr()
{
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	int i;

	bzero(&cpu_kcore_hdr, sizeof(cpu_kcore_hdr));

	h->mmutype = mmutype;
	h->kernel_pa = low[0];
	h->sysseg_pa = pmap_kernel()->pm_stpa;

	/*
	 * mac68k has multiple RAM segments on some models.
	 */
	for (i = 0; i < numranges; i++) {
		h->ram_segs[i].start = low[i];
		h->ram_segs[i].size  = high[i] - low[i];
	}
}

/*
 * Compute the size of the machine-dependent crash dump header.
 * Returns size in disk blocks.
 */
int
cpu_dumpsize()
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	return (btodb(roundup(size, dbtob(1))));
}

/*
 * Called by dumpsys() to dump the machine-dependent header.
 */
int
cpu_dump(dump, blknop)
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	daddr64_t *blknop;
{
	int buf[dbtob(1) / sizeof(int)];
	cpu_kcore_hdr_t *chdr;
	kcore_seg_t *kseg;
	int error;

	kseg = (kcore_seg_t *)buf;
	chdr = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(kcore_seg_t)) /
	    sizeof(int)];

	/* Create the segment header. */
	CORE_SETMAGIC(*kseg, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg->c_size = dbtob(1) - ALIGN(sizeof(kcore_seg_t));

	bcopy(&cpu_kcore_hdr, chdr, sizeof(cpu_kcore_hdr_t));
	error = (*dump)(dumpdev, *blknop, (caddr_t)buf, sizeof(buf));
	*blknop += btodb(sizeof(buf));
	return (error);
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first block of disk space in
 * case there might be a disk label stored there.  If there
 * is extra space, put dump at the end to reduce the chance
 * that swapping trashes it.
 */
void
dumpconf(void)
{
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	int nblks;	/* size of dump area */
	int i;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpsize = 0;
	for (i = 0; h->ram_segs[i].size && i < NPHYS_RAM_SEGS; i++)
		dumpsize += atop(h->ram_segs[i].size);
	dumpsize += cpu_dumpsize();

	/* Always skip the first block, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

void
dumpsys()
{
	cpu_kcore_hdr_t *h = &cpu_kcore_hdr;
	daddr64_t blkno;	/* current block to write */
				/* dump routine */
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	int pg;			/* page being dumped */
	vaddr_t maddr;	/* PA being dumped */
	int seg;		/* RAM segment being dumped */
	int error;		/* error code from (*dump)() */
	extern int msgbufmapped;

	/* XXX initialized here because of gcc lossage */
	seg = 0;
	maddr = h->ram_segs[seg].start;
	pg = 0;

	/* Don't record dump msgs in msgbuf. */
	msgbufmapped = 0;

	/* Make sure dump device is valid. */
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	dump = bdevsw[major(dumpdev)].d_dump;
	blkno = dumplo;

	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif

	printf("dump ");

	/* Write the dump header. */
	error = cpu_dump(dump, &blkno);
	if (error)
		goto bad;

	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many MBs we have dumped */
		if (pg && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		while (maddr >=
		    (h->ram_segs[seg].start + h->ram_segs[seg].size)) {
			if (++seg >= NPHYS_RAM_SEGS ||
			    h->ram_segs[seg].size == 0) {
				error = EINVAL;		/* XXX ?? */
				goto bad;
			}
			maddr = h->ram_segs[seg].start;
		}
		pmap_enter(pmap_kernel(), (vaddr_t)vmmap, maddr,
		    VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);
		pmap_update(pmap_kernel());

		error = (*dump)(dumpdev, blkno, vmmap, NBPG);
bad:
		switch (error) {
		case 0:
			maddr += NBPG;
			blkno += btodb(NBPG);
			break;

		case ENXIO:
			printf("device bad\n");
			return;

		case EFAULT:
			printf("device not ready\n");
			return;

		case EINVAL:
			printf("area improper\n");
			return;

		case EIO:
			printf("i/o error\n");
			return;

		case EINTR:
			printf("aborted from console\n");
			return;

		default:
			printf("error %d\n", error);
			return;
		}
	}
	printf("succeeded\n");
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
	int s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec += clkread();
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

void
straytrap(pc, evec)
	int pc;
	int evec;
{
	printf("unexpected trap; vector offset 0x%x from 0x%x.\n",
	    (int) (evec & 0xfff), pc);
#ifdef DEBUG
#ifdef DDB
	Debugger();
#endif
#endif
}

int	*nofault;

/*
 * Level 7 interrupts can be caused by the keyboard or parity errors.
 */
void	nmihand(struct frame);

void
nmihand(frame)
	struct frame frame;
{
	static int nmihanddeep = 0;

	if (nmihanddeep)
		return;
	nmihanddeep = 1;

#ifdef DDB
	if (db_console)
		Debugger();
#endif

	nmihanddeep = 0;
}

/*
 * It should be possible to probe for the top of RAM, but Apple has
 * memory structured so that in at least some cases, it's possible
 * for RAM to be aliased across all memory--or for it to appear that
 * there is more RAM than there really is.
 */
int	get_top_of_ram(void);

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
	int *name;
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

static char *envbuf = NULL;

/*
 * getenvvars: Grab a few useful variables
 */
void	getenvvars(u_long, char *);
long	getenv(char *);

void
getenvvars(flag, buf)
	u_long  flag;
	char   *buf;
{
	extern u_long bootdev;
#if defined(DDB) || NKSYMS > 0
	extern u_long end, esym;
#endif
	extern u_long macos_boottime;
	extern vaddr_t MacOSROMBase;
	extern long macos_gmtbias;
	extern u_short ADBDelay;
	extern u_int32_t HwCfgFlags3;
	int root_scsi_id;
	vaddr_t ROMBase;

	/*
	 * If flag & 0x80000000 == 0, then we're booting with the old booter
	 * and we should freak out.
	 */
	if ((flag & 0x80000000) == 0) {
		/* Freak out; print something if that becomes available */
	} else
		envbuf = buf;

	/* These next two should give us mapped video & serial */
	/* We need these for pre-mapping graybars & echo, but probably */
	/* only on MacII or LC.  --  XXX */
	/* videoaddr = getenv("MACOS_VIDEO"); */

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
	mac68k_machine.mach_memsize = getenv("MEMSIZE");
	mac68k_machine.do_graybars = getenv("GRAYBARS");
	mac68k_machine.serial_boot_echo = getenv("SERIALECHO");
	mac68k_machine.serial_console = getenv("SERIALCONSOLE");

	mac68k_machine.modem_flags = getenv("SERIAL_MODEM_FLAGS");
	mac68k_machine.modem_cts_clk = getenv("SERIAL_MODEM_HSKICLK");
	mac68k_machine.modem_dcd_clk = getenv("SERIAL_MODEM_GPICLK");
        mac68k_machine.modem_flags = getenv("SERIAL_MODEM_DSPEED");
	mac68k_machine.print_flags = getenv("SERIAL_PRINT_FLAGS");
	mac68k_machine.print_cts_clk = getenv("SERIAL_PRINT_HSKICLK");
	mac68k_machine.print_dcd_clk = getenv("SERIAL_PRINT_GPICLK");
	mac68k_machine.print_d_speed = getenv("SERIAL_PRINT_DSPEED");
	mac68k_machine.booter_version = getenv("BOOTERVER");

	/*
	 * For now, we assume that the boot device is off the first controller.
	 * Booter versions 1.11.0 and later set a flag to tell us to construct
	 * bootdev using the SCSI ID passed in via the environment.
	 */
	root_scsi_id = getenv("ROOT_SCSI_ID");
	if (((mac68k_machine.booter_version < CURRENTBOOTERVER) ||
	    (flag & 0x40000)) && bootdev == 0)
		bootdev = MAKEBOOTDEV(4, 0, 0, root_scsi_id, 0);

	/*
	 * Booter 1.11.3 and later pass a BOOTHOWTO variable with the
	 * appropriate bits set.
	 */
	boothowto = getenv("BOOTHOWTO");
	if (boothowto == 0)
		boothowto = getenv("SINGLE_USER");

	/*
	 * Get end of symbols for kernel debugging
	 */
#if defined(DDB) || NKSYMS > 0
	esym = getenv("END_SYM");
	if (esym == 0)
		esym = (long) &end;
#endif

	/* Get MacOS time */
	macos_boottime = getenv("BOOTTIME");

	/* Save GMT BIAS saved in Booter parameters dialog box */
	macos_gmtbias = getenv("GMTBIAS");

	/*
	 * Save globals stolen from MacOS
	 */

	ROMBase = (vaddr_t)getenv("ROMBASE");
	if (ROMBase != 0)
		MacOSROMBase = ROMBase;
	ADBDelay = (u_short) getenv("ADBDELAY");
	HwCfgFlags3 = getenv("HWCFGFLAG3");
}

char	toupper(char);

char
toupper(c)
	char c;
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 'A';
	} else {
		return c;
	}
}

long
getenv(str)
	char *str;
{
	/*
	 * Returns the value of the environment variable "str".
	 *
	 * Format of the buffer is "var=val\0var=val\0...\0var=val\0\0".
	 *
	 * Returns 0 if the variable is not there, and 1 if the variable is
	 * there without an "=val".
	 */

	char *s, *s1;
	int val, base;

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

struct cpu_model_info cpu_models[] = {

/* The first four. */
	{ MACH_MACII,		"II",			MACH_CLASSII },
	{ MACH_MACIIX,		"IIx",			MACH_CLASSII },
	{ MACH_MACIICX,		"IIcx",			MACH_CLASSII },
	{ MACH_MACSE30,		"SE/30",		MACH_CLASSII },

/* The rest of the II series... */
	{ MACH_MACIICI,		"IIci",			MACH_CLASSIIci },
	{ MACH_MACIISI,		"IIsi",			MACH_CLASSIIsi },
	{ MACH_MACIIVI,		"IIvi",			MACH_CLASSIIvx },
	{ MACH_MACIIVX,		"IIvx",			MACH_CLASSIIvx },
	{ MACH_MACIIFX,		"IIfx",			MACH_CLASSIIfx },

/* The Centris/Quadra series. */
	{ MACH_MACQ700,		"Quadra 700",		MACH_CLASSQ },
	{ MACH_MACQ900,		"Quadra 900",		MACH_CLASSQ },
	{ MACH_MACQ950,		"Quadra 950",		MACH_CLASSQ },
	{ MACH_MACQ800,		"Quadra 800",		MACH_CLASSQ },
	{ MACH_MACQ650,		"Quadra 650",		MACH_CLASSQ },
	{ MACH_MACC650,		"Centris 650",		MACH_CLASSQ },
	{ MACH_MACQ605,		"Quadra 605",		MACH_CLASSQ },
	{ MACH_MACQ605_33,	"Quadra 605/33",	MACH_CLASSQ },
	{ MACH_MACC610,		"Centris 610",		MACH_CLASSQ },
	{ MACH_MACQ610,		"Quadra 610",		MACH_CLASSQ },
	{ MACH_MACQ630,		"Quadra 630",		MACH_CLASSQ2 },
	{ MACH_MACC660AV,	"Centris 660AV",	MACH_CLASSAV },
	{ MACH_MACQ840AV,	"Quadra 840AV",		MACH_CLASSAV },

/* The Powerbooks/Duos... */
	{ MACH_MACPB100,	"PowerBook 100",	MACH_CLASSPB },
	/* PB 100 has no MMU! */
	{ MACH_MACPB140,	"PowerBook 140",	MACH_CLASSPB },
	{ MACH_MACPB145,	"PowerBook 145",	MACH_CLASSPB },
	{ MACH_MACPB150,	"PowerBook 150",	MACH_CLASSDUO },
	{ MACH_MACPB160,	"PowerBook 160",	MACH_CLASSPB },
	{ MACH_MACPB165,	"PowerBook 165",	MACH_CLASSPB },
	{ MACH_MACPB165C,	"PowerBook 165c",	MACH_CLASSPB },
	{ MACH_MACPB170,	"PowerBook 170",	MACH_CLASSPB },
	{ MACH_MACPB180,	"PowerBook 180",	MACH_CLASSPB },
	{ MACH_MACPB180C,	"PowerBook 180c",	MACH_CLASSPB },
	{ MACH_MACPB190,	"PowerBook 190",	MACH_CLASSPB },
	{ MACH_MACPB190CS,	"PowerBook 190cs",	MACH_CLASSPB },
	{ MACH_MACPB500,	"PowerBook 500",	MACH_CLASSPB },

/* The Duos */
	{ MACH_MACPB210,	"PowerBook Duo 210",	MACH_CLASSDUO },
	{ MACH_MACPB230,	"PowerBook Duo 230",	MACH_CLASSDUO },
	{ MACH_MACPB250,	"PowerBook Duo 250",	MACH_CLASSDUO },
	{ MACH_MACPB270,	"PowerBook Duo 270C",	MACH_CLASSDUO },
	{ MACH_MACPB280,	"PowerBook Duo 280",	MACH_CLASSDUO },
	{ MACH_MACPB280C,	"PowerBook Duo 280C",	MACH_CLASSDUO },

/* The Performas... */
	{ MACH_MACP600,		"Performa 600",		MACH_CLASSIIvx },
	{ MACH_MACP460,		"Performa 460",		MACH_CLASSLC },
	{ MACH_MACP550,		"Performa 550",		MACH_CLASSLC },
	{ MACH_MACP580,		"Performa 580",		MACH_CLASSQ2 },
	{ MACH_MACTV,		"TV",			MACH_CLASSLC },

/* The LCs... */
	{ MACH_MACLCII,		"LC II",		MACH_CLASSLC },
	{ MACH_MACLCIII,	"LC III",		MACH_CLASSLC },
	{ MACH_MACLC475,	"LC 475",		MACH_CLASSQ },
	{ MACH_MACLC475_33,	"LC 475/33",		MACH_CLASSQ },
	{ MACH_MACLC520,	"LC 520",		MACH_CLASSLC },
	{ MACH_MACLC575,	"LC 575",		MACH_CLASSQ2 },
	{ MACH_MACCCLASSIC,	"Color Classic",	MACH_CLASSLC },
	{ MACH_MACCCLASSICII,	"Color ClassicII",	MACH_CLASSLC },
/* Does this belong here? */
	{ MACH_MACCLASSICII,	"Classic II",		MACH_CLASSLC },

/* The unknown one and the end... */
	{ 0,			"Unknown",		MACH_CLASSII }
};				/* End of cpu_models[] initialization. */

struct intvid_info_t {
	int	machineid;
	u_long	fbbase;
	u_long	fbmask;
	u_long	fblen;
} intvid_info[] = {
	{ MACH_MACCLASSICII,	0x009f9a80,	0x0,		21888 },
	{ MACH_MACPB140,	0xfee08000,	0x0,		32 * 1024 },
	{ MACH_MACPB145,	0xfee08000,	0x0,		32 * 1024 },
	{ MACH_MACPB170,	0xfee08000,	0x0,		32 * 1024 },
	{ MACH_MACPB150,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB160,	0x60000000,	0x0ffe0000,	128 * 1024 },
	{ MACH_MACPB165,	0x60000000,	0x0ffe0000,	128 * 1024 },
	{ MACH_MACPB180,	0x60000000,	0x0ffe0000,	128 * 1024 },
	{ MACH_MACPB210,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB230,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB250,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB270,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB280,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACPB280C,	0x60000000,	0x0,		128 * 1024 },
	{ MACH_MACIICI,		0x0,		0x0,		320 * 1024 },
	{ MACH_MACIISI,		0x0,		0x0,		320 * 1024 },
	{ MACH_MACCCLASSIC,	0x50f40000,	0x0,		512 * 1024 },
/*??*/	{ MACH_MACLCII,		0x50f40000,	0x0,		512 * 1024 },
	{ MACH_MACPB165C,	0xfc040000,	0x0,		512 * 1024 },
	{ MACH_MACPB180C,	0xfc040000,	0x0,		512 * 1024 },
	{ MACH_MACPB190,	0x60000000,	0x0,		512 * 1024 },
	{ MACH_MACPB190CS,	0x60000000,	0x0,		512 * 1024 },
	{ MACH_MACPB500,	0x60000000,	0x0,		512 * 1024 },
	{ MACH_MACLCIII,	0x60b00000,	0x0,		768 * 1024 },
	{ MACH_MACLC520,	0x60000000,	0x0,		1024 * 1024 },
	{ MACH_MACP550,		0x60000000,	0x0,		1024 * 1024 },
	{ MACH_MACTV,		0x60000000,	0x0,		1024 * 1024 },
	{ MACH_MACLC475,	0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACLC475_33,	0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACLC575,	0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACC610,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACC650,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACP580,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ605,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ605_33,	0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ610,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ630,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ650,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACC660AV,	0x50100000,	0x0,		1024 * 1024 },
	{ MACH_MACQ700,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ800,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ900,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ950,		0xf9000000,	0x0,		1024 * 1024 },
	{ MACH_MACQ840AV,	0x50100000,	0x0,		2048 * 1024 },
	{ 0,			0x0,		0x0,		0 },
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

char	cpu_model[120];		/* for sysctl() */

int
fpu_probe()
{
	/*
	 * A 68881 idle frame is 28 bytes and a 68882's is 60 bytes.
	 * We, of course, need to have enough room for either.
	 */
	int	fpframe[60 / sizeof(int)];
	label_t	faultbuf;
	u_char	b;

	nofault = (int *) &faultbuf;
	if (setjmp(&faultbuf)) {
		nofault = (int *) 0;
		return (FPU_NONE);
	}

	/*
	 * Synchronize FPU or cause a fault.
	 * This should leave the 881/882 in the IDLE state,
	 * state, so we can determine which we have by
	 * examining the size of the FP state frame
	 */
	asm("fnop");

	nofault = (int *) 0;

	/*
	 * Presumably, if we're an 040 and did not take exception
	 * above, we have an FPU.  Don't bother probing.
	 */
	if (mmutype == MMU_68040)
		return (FPU_68040);

	/*
	 * Presumably, this will not cause a fault--the fnop should
	 * have if this will.  We save the state in order to get the
	 * size of the frame.
	 */
	asm("movl %0, a0; fsave a0@" : : "a" (fpframe) : "a0" );

	b = *((u_char *) fpframe + 1);

	/*
	 * Now, restore a NULL state to reset the FPU.
	 */
	fpframe[0] = fpframe[1] = 0;
	m68881_restore((struct fpframe *) fpframe);

	/*
	 * The size of a 68881 IDLE frame is 0x18
	 *         and a 68882 frame is 0x38
	 */
	if (b == 0x18)
		return (FPU_68881);
	if (b == 0x38)
		return (FPU_68882);

	/*
	 * If it's not one of the above, we have no clue what it is.
	 */
	return (FPU_UNKNOWN);
}

void
identifycpu()
{
#ifdef DEBUG
	extern u_int delay_factor;
#endif

	/*
	 * Print the machine type...
	 */
	snprintf(cpu_model, sizeof cpu_model, "Apple Macintosh %s",
	    cpu_models[mac68k_machine.cpu_model_index].model);

	/*
	 * ... and the CPU type...
	 */
	switch (cputype) {
	case CPU_68040:
		strlcat(cpu_model, ", 68040 CPU", sizeof cpu_model);
		break;
	case CPU_68030:
		strlcat(cpu_model, ", 68030 CPU", sizeof cpu_model);
		break;
	case CPU_68020:
		strlcat(cpu_model, ", 68020 CPU", sizeof cpu_model);
		break;
	default:
		strlcat(cpu_model, ", unknown CPU", sizeof cpu_model);
		break;
	}

	/*
	 * ... and the MMU type...
	 */
	switch (mmutype) {
	case MMU_68040:
	case MMU_68030:
		strlcat(cpu_model, "+MMU", sizeof cpu_model);
		break;
	case MMU_68851:
		strlcat(cpu_model, ", MC68851 MMU", sizeof cpu_model);
		break;
	default:
		printf("%s\n", cpu_model);
		panic("unknown MMU type %d", mmutype);
		/* NOTREACHED */
	}

	/*
	 * ... and the FPU type...
	 */
	fputype = fpu_probe();	/* should eventually move to locore */

	switch (fputype) {
	case FPU_68040:
		strlcat(cpu_model, "+FPU", sizeof cpu_model);
		break;
	case FPU_68882:
		strlcat(cpu_model, ", MC6882 FPU", sizeof cpu_model);
		break;
	case FPU_68881:
		strlcat(cpu_model, ", MC6881 FPU", sizeof cpu_model);
		break;
	case FPU_UNKNOWN:
		strlcat(cpu_model, ", unknown FPU", sizeof cpu_model);
		break;
	default:
		/*strlcat(cpu_model, ", no FPU", sizeof cpu_model);*/
		break;
	}

	/*
	 * ... and finally, the cache type.
	 */
	if (cputype == CPU_68040)
		strlcat(cpu_model, ", 4k on-chip physical I/D caches", sizeof cpu_model);

	printf("%s\n", cpu_model);
#ifdef DEBUG
	printf("cpu: delay factor %d\n", delay_factor);
#endif
}

void	get_machine_info(void);

void
get_machine_info()
{
	int i;

	for (i = 0; cpu_models[i].machineid != 0; i++)
		if (mac68k_machine.machineid == cpu_models[i].machineid)
			break;

	mac68k_machine.cpu_model_index = i;
}

const struct cpu_model_info *current_mac_model;

/*
 * Sets a bunch of machine-specific variables
 */
void	setmachdep(void);

void
setmachdep()
{
	struct cpu_model_info *cpui;

	/*
	 * First, set things that need to be set on the first pass only
	 * Ideally, we'd only call this once, but for some reason, the
	 * VIAs need interrupts turned off twice !?
	 */
	get_machine_info();

	load_addr = 0;
	cpui = &(cpu_models[mac68k_machine.cpu_model_index]);
	current_mac_model = cpui;

	mac68k_machine.via1_ipl = 1;
	mac68k_machine.aux_interrupts = 0;

	/*
	 * Set up any machine specific stuff that we have to before
	 * ANYTHING else happens
	 */
	switch (cpui->class) {	/* Base this on class of machine... */
	case MACH_CLASSII:
		VIA2 = VIA2OFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSPB:
		VIA2 = VIA2OFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
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
		VIA2 = RBVOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		/* Disable everything but PM; we need it. */
		via_reg(VIA1, vIER) = 0x6f;	/* disable VIA1 int */
		/* Are we disabling something important? */
		via_reg(VIA2, rIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSQ:
	case MACH_CLASSQ2:
		VIA2 = VIA2OFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.sonic = 1;
		mac68k_machine.scsi96 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */

		/* Enable A/UX interrupt scheme */
		mac68k_machine.aux_interrupts = 1;
		via_reg(VIA1, vBufB) &= (0xff ^ DB1O_AuxIntEnb);
		via_reg(VIA1, vDirB) |= DB1O_AuxIntEnb;
		mac68k_machine.via1_ipl = 6;

		break;
	case MACH_CLASSAV:
		VIA2 = VIA2OFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi96 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSIIci:
		VIA2 = RBVOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIsi:
		VIA2 = RBVOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIvx:
		VIA2 = RBVOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSLC:
		VIA2 = RBVOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIfx:
		VIA2 = OSSOFF;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *)IOBase;
		mac68k_machine.scsi80 = 1;
		via_reg(VIA1, vIER) = 0x7f;  /* disable VIA1 int */
		break;
	default:
	case MACH_CLASSH:
		break;
	}
}

/*
 * Set IO offsets.
 */
void
mac68k_set_io_offsets(base)
	vaddr_t base;
{
	extern volatile u_char *sccA;

	switch (current_mac_model->class) {
	case MACH_CLASSQ:
		Via1Base = (volatile u_char *)base;

		/* The following two may be overridden. */
		sccA = (volatile u_char *)base + 0xc000;
		SCSIBase = base + 0xf000;

		switch (current_mac_model->machineid) {
		case MACH_MACQ900:
		case MACH_MACQ950:
			sccA = (volatile u_char *)base + 0xc020;
			iop_serial_compatible();
			mac68k_machine.scsi96_2 = 1;
			break;
		case MACH_MACQ700:
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
		Via1Base = (volatile u_char *)base;
		sccA = (volatile u_char *)base + 0xc020;
		SCSIBase = base + 0x10000;
		break;
	case MACH_CLASSAV:
		Via1Base = (volatile u_char *)base;
		sccA = (volatile u_char *)base + 0x4000;
		SCSIBase = base + 0x18000;
		PSCBase = (volatile u_char *)base + 0x31000;
		break;
	case MACH_CLASSII:
	case MACH_CLASSPB:
	case MACH_CLASSDUO:
	case MACH_CLASSIIci:
	case MACH_CLASSIIsi:
	case MACH_CLASSIIvx:
	case MACH_CLASSLC:
		Via1Base = (volatile u_char *)base;
		sccA = (volatile u_char *)base + 0x4000;
		SCSIBase = base;
		break;
	case MACH_CLASSIIfx:
		Via1Base = (volatile u_char *)base;
		sccA = (volatile u_char *)base + 0x4020;
		iop_serial_compatible();
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
u_long gray_nextaddr = 0;

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
			((u_long *)videoaddr)[gray_nextaddr++] = 0xaaaaaaaa;
		for (i = 0; i < 2 * videorowbytes / 4; i++)
			((u_long *)videoaddr)[gray_nextaddr++] = 0x00000000;
	}

	__asm __volatile ("	movl sp@+,d1;
				movl sp@+,d0;
				movl sp@+,a1;
				movl sp@+,a0");
}
#endif

/* in locore */
extern u_long ptest040(caddr_t addr, u_int fc);
extern int get_pte(u_int addr, u_long pte[2], u_short * psr);

/*
 * LAK (7/24/94): given a logical address, puts the physical address
 *  in *phys and return 1, or returns 0 on failure.  This is intended
 *  to look through MacOS page tables.
 */

u_long
get_physical(u_int addr, u_long * phys)
{
	extern u_int macos_tc;
	u_long pte[2], ph, mask;
	u_short psr;
	int i, numbits;

	if (mmutype == MMU_68040) {
		ph = ptest040((caddr_t)addr, FC_SUPERD);
		if ((ph & MMU40_RES) == 0) {
			ph = ptest040((caddr_t)addr, FC_USERD);
			if ((ph & MMU40_RES) == 0)
				return 0;
		}
		if ((ph & MMU40_TTR) != 0)
			ph = addr;

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

void	check_video(char *, u_long, u_long);

void
check_video(id, limit, maxm)
	char *id;
	u_long limit, maxm;
{
	u_long addr, phys;

	if (!get_physical(videoaddr, &phys)) {
		if (mac68k_machine.do_graybars)
			printf("get_mapping(): %s.  False start.\n", id);
	} else {
		mac68k_vidlog = videoaddr;
		mac68k_vidphys = phys;
		mac68k_vidlen = 32768;
		addr = videoaddr + 32768;
		while (get_physical(addr, &phys)) {
			if ((phys - mac68k_vidphys) != mac68k_vidlen)
				break;
			if (mac68k_vidlen + 32768 > limit) {
				if (mac68k_machine.do_graybars) {
					printf("mapping: %s.  Does it never end?\n",
					    id);
					printf("    Forcing VRAM size ");
					printf("to a conservative %ldK.\n",
					    maxm/1024);
				}
				mac68k_vidlen = maxm;
				break;
			}
			mac68k_vidlen += 32768;
			addr += 32768;
		}
		if (mac68k_machine.do_graybars) {
			printf("  %s internal video at paddr 0x%x, len 0x%x.\n",
			    id, mac68k_vidphys, mac68k_vidlen);
		}
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
	struct intvid_info_t *iip;
	u_long addr, lastpage, phys, len, limit;
	int i, last, same;

	numranges = 0;
	for (i = 0; i < NBMEMRANGES; i++) {
		low[i] = 0;
		high[i] = 0;
	}

	lastpage = get_top_of_ram();

	get_physical(0, &load_addr);

	last = 0;
	for (addr = 0; addr <= lastpage && get_physical(addr, &phys);
	    addr += PAGE_SIZE) {
		if (numranges > 0 && phys != high[last]) {
			/*
			 * Attempt to find if this page is already
			 * accounted for in an existing physical segment.
			 */
			for (i = 0; i < numranges; i++) {
				if (low[i] <= phys && phys <= high[i]) {
					last = i;
					break;
				}
			}
			if (i >= numranges)
				last = numranges - 1;

			if (low[last] <= phys && phys < high[last])
				continue;	/* Skip pages we've seen. */
		}

		if (numranges > 0 && phys == high[last]) {
			/* Common case: extend existing segment on high end */
			high[last] += PAGE_SIZE;
		} else if (numranges < NBMEMRANGES - 1) {
			/* This is a new physical segment. */
			for (last = 0; last < numranges; last++)
				if (phys < low[last])
					break;

			/* Create space for segment, if necessary */
			if (last < numranges && phys < low[last]) {
				for (i = numranges; i > last; i--) {
					low[i] = low[i - 1];
					high[i] = high[i - 1];
				}
			}

			numranges++;
			low[last] = phys;
			high[last] = phys + PAGE_SIZE;
		} else {
			/* Not enough ranges. Display a warning message? */
			continue;
		}

		/* Coalesce adjoining segments as appropriate */
		if (last < (numranges - 1) && high[last] == low[last + 1] &&
		    low[last + 1] != load_addr) {
			high[last] = high[last + 1];
			for (i = last + 1; i < numranges; i++) {
				low[i] = low[i + 1];
				high[i] = high[i + 1];
			}
			--numranges;
		}
	}
	if (mac68k_machine.do_graybars) {
		printf("System RAM: %ld bytes in %ld pages.\n",
		    addr, addr / PAGE_SIZE);
		for (i = 0; i < numranges; i++) {
			printf("     Low = 0x%lx, high = 0x%lx\n",
			    low[i], high[i]);
		}
	}

	/*
	 * If we can't figure out the PA of the frame buffer by groveling
	 * the page tables, assume that we already have the correct
	 * address.  This is the case on several of the PowerBook 1xx
	 * series, in particular.
	 */
	if (!get_physical(videoaddr, &phys))
		phys = videoaddr;

	/*
	 * Find on-board video, if we have an idea of where to look
	 * on this system.
	 */
	for (iip = intvid_info; iip->machineid != 0; iip++)
		if (mac68k_machine.machineid == iip->machineid)
			break;

	if (mac68k_machine.machineid == iip->machineid &&
	    (phys & ~iip->fbmask) >= iip->fbbase &&
	    (phys & ~iip->fbmask) < (iip->fbbase + iip->fblen)) {
		mac68k_vidphys = phys & ~iip->fbmask;
		mac68k_vidlen = 32768 - (phys & 0x7fff);

		limit = iip->fbbase + iip->fblen - mac68k_vidphys;
		if (mac68k_vidlen > limit) {
			mac68k_vidlen = limit;
		} else {
			addr = videoaddr + mac68k_vidlen;
			while (get_physical(addr, &phys)) {
				if ((phys - mac68k_vidphys) != mac68k_vidlen)
					break;
				if (mac68k_vidlen + 32768 > limit) {
					mac68k_vidlen = limit;
					break;
				}
				mac68k_vidlen += 32768;
				addr += 32768;
			}
		}
	}

	if (mac68k_vidlen > 0) {
		/*
		 * We've already figured out where internal video is.
		 * Tell the user what we know.
		 */
		if (mac68k_machine.do_graybars)
			printf("On-board video at addr 0x%lx (phys 0x%x), len 0x%x.\n",
			    videoaddr, mac68k_vidphys, mac68k_vidlen);
	} else {
		/*
	 	* We should now look through all of NuBus space to find where
	 	* the internal video is being mapped.  Just to be sure we
	 	* handle all the cases, we simply map our NuBus space exactly
	 	* how MacOS did it.  As above, we find a bunch of ranges that
	 	* are contiguously mapped.  Since there are a lot of pages
	 	* that are all mapped to 0, we handle that as a special case
	 	* where the length is negative.  We search in increments of
	 	* 32768 because that's the page size that MacOS uses.
		*/
		nbnumranges = 0;
		for (i = 0; i < NBMAXRANGES; i++) {
			nbphys[i] = 0;
			nblog[i] = 0;
			nblen[i] = 0;
		}

		same = 0;
		for (addr = NBBASE; addr < NBTOP; addr += 32768) {
			if (!get_physical(addr, &phys)) {
				continue;
			}
			len = nbnumranges == 0 ? 0 : nblen[nbnumranges - 1];

#if DEBUG
			if (mac68k_machine.do_graybars)
				printf ("0x%lx --> 0x%lx\n", addr, phys);
#endif
			if (nbnumranges > 0
			    && addr == nblog[nbnumranges - 1] + len
			    && phys == nbphys[nbnumranges - 1]) {
				/* Same as last one */
				nblen[nbnumranges - 1] += 32768;
				same = 1;
			} else {
				if ((nbnumranges > 0)
				    && !same
				    && (addr == nblog[nbnumranges - 1] + len)
				    && (phys == nbphys[nbnumranges - 1] + len))
					nblen[nbnumranges - 1] += 32768;
				else {
					if (same) {
						nblen[nbnumranges - 1] = -len;
						same = 0;
					}
					if (nbnumranges == NBMAXRANGES) {
						if (mac68k_machine.do_graybars)
							printf("get_mapping(): "
							    "Too many NuBus ranges.\n");
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
		if (mac68k_machine.do_graybars) {
			printf("Non-system RAM (nubus, etc.):\n");
			for (i = 0; i < nbnumranges; i++) {
				printf("     Log = 0x%lx, Phys = 0x%lx, Len = 0x%lx (%lu)\n",
				    nblog[i], nbphys[i], nblen[i], nblen[i]);
			}
		}

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
				if (mac68k_machine.do_graybars)
					printf("Checking for Internal Video ");
				/*
				 * Kludge for IIvx internal video (60b0 0000).
				 * PB 520 (6000 0000)
				 */
				check_video("PB/IIvx (0x60?00000)",
				    1 * 1024 * 1024, 1 * 1024 * 1024);
			} else if (0x50F40000 <= videoaddr
			    && videoaddr < 0x50FBFFFF) {
				/*
				 * Kludge for LC internal video
				 */
				check_video("LC video (0x50f40000)",
				    512 * 1024, 512 * 1024);
			} else if (0x50100100 <= videoaddr
			    && videoaddr < 0x50400000) {
				/*
				 * Kludge for AV internal video
				 */
				check_video("AV video (0x50100100)",
				    1 * 1024 * 1024, 1 * 1024 * 1024);
			} else {
				if (mac68k_machine.do_graybars)
					printf( "  no internal video at address 0 -- "
						"videoaddr is 0x%lx.\n", videoaddr);
			}
		} else {
			if (mac68k_machine.do_graybars) {
				printf("  Video address = 0x%lx\n", videoaddr);
				printf("  Int video starts at 0x%x\n",
				    mac68k_vidlog);
				printf("  Length = 0x%x (%d) bytes\n",
				    mac68k_vidlen, mac68k_vidlen);
			}
		}
	}

	return load_addr;	/* Return physical address of logical 0 */
}

#ifdef DEBUG
/*
 * Debugging code for locore page-traversal routine.
 */
void printstar(void);
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
#endif

/*
 * Console bell callback; modularizes the console terminal emulator
 * and the audio system, so neither requires direct knowledge of the
 * other.
 */

void
mac68k_set_bell_callback(callback, cookie)
	int (*callback)(void *, int, int, int);
	void *cookie;
{
	mac68k_bell_callback = callback;
	mac68k_bell_cookie = (caddr_t)cookie;
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
