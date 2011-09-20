/*	$OpenBSD: machdep.c,v 1.134 2011/09/20 09:49:38 miod Exp $	*/
/*	$NetBSD: machdep.c,v 1.121 1999/03/26 23:41:29 mycroft Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
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
 * from: Utah $Hdr: machdep.c 1.74 92/12/20$
 *
 *	@(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/extent.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/syslog.h>

#include <machine/db_machdep.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>
#include <machine/kcore.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>

#include <dev/cons.h>

#include <net/if.h>
#include <uvm/uvm_extern.h>
#include <uvm/uvm_swap.h>

#ifdef USELEDS
#include <hp300/hp300/leds.h>
#endif

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;	/* from <machine/param.h> */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

extern paddr_t avail_start, avail_end;

int	physmem;		/* size of physical memory, in pages */

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

extern	u_int lowram;
extern	short exframesize[];

/*
 * Some storage space must be allocated statically because of the
 * early console initialization.
 */
char	extiospace[EXTENT_FIXED_STORAGE_SIZE(8)];
extern int eiomapsize;

/* prototypes for local functions */
void	parityenable(void);
int	parityerror(struct frame *);
int	parityerrorfind(void);
void    identifycpu(void);
void    initcpu(void);
void	dumpmem(int *, int, int);
char	*hexstr(int, int);

/* functions called from locore.s */
void    dumpsys(void);
void	hp300_init(void);
void    straytrap(int, u_short);
void	nmihand(struct frame);

/*
 * Select code of console.  Set to CONSCODE_INTERNAL if console is on
 * "internal" framebuffer.
 */
int	conscode;
caddr_t	conaddr;		/* for drivers in cn_init() */
int	convasize;		/* size of mapped console device */

/*
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuspeed (where cpuspeed is in MHz) on 68020
 * and 68030 systems.  See clock.c for the delay
 * calibration algorithm.
 */
int	cpuspeed;		/* relative cpu speed */
int	delay_divisor;		/* delay constant */

 /*
 * Early initialization, before main() is called.
 */
void
hp300_init()
{
	/*
	 * Tell the VM system about available physical memory.  The
	 * hp300 only has one segment.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), 0);

	/* Initialize the interrupt handlers. */
	intr_init();

	/* Calibrate the delay loop. */
	hp300_calibrate_delay();
}

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	extern struct extent *extio;
	extern char *extiobase;

	/*
	 * Initialize some variables for sanity.
	 */
	convasize = 0;
	conscode = CONSCODE_INVALID;

	/*
	 * Initialize the bus resource map.
	 */
	extio = extent_create("extio",
	    (u_long)extiobase, (u_long)extiobase + ptoa(eiomapsize),
	    M_DEVBUF, extiospace, sizeof(extiospace), EX_NOWAIT);

	/*
	 * Initialize the console before we print anything out.
	 */
	hp300_cninit();

#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	extern char *etext;
	unsigned i;
	vaddr_t minaddr, maxaddr;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Now that VM services are available, give another chance at
	 * console devices to initialize, if they could not before.
	 */
	hp300_cninit();

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < atop(MSGBUFSIZE); i++)
		pmap_enter(pmap_kernel(), (vaddr_t)msgbufp + i * NBPG,
		    avail_end + i * NBPG, VM_PROT_READ|VM_PROT_WRITE,
		    VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

	/*
	 * Tell the VM system that page 0 isn't mapped.
	 *
	 * XXX This is bogus; should just fix KERNBASE and
	 * XXX VM_MIN_KERNEL_ADDRESS, but not right now.
	 */
	if (uvm_map_protect(kernel_map, 0, NBPG, UVM_PROT_NONE, TRUE))
		panic("can't mark page 0 off-limits");

	/*
	 * Tell the VM system that writing to kernel text isn't allowed.
	 * If we don't, we might end up COW'ing the text segment!
	 *
	 * XXX Should be trunc_page(&kernel_text) instead
	 * XXX of NBPG.
	 */
	if (uvm_map_protect(kernel_map, NBPG, round_page((vaddr_t)&etext),
	    UVM_PROT_READ|UVM_PROT_EXEC, TRUE))
		panic("can't protect kernel text");

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
}

/*
 * Info for CTL_HW
 */
char	cpu_model[120];

/*
 * Text description of models we support, indexed by machineid.
 */
const char *hp300_models[] = {
	"320",		/* HP_320 */
	"318/319/330",	/* HP_330 */
	"350",		/* HP_350 */
	"360",		/* HP_36X */
	"370",		/* HP_370 */
	"340",		/* HP_340 */
	"345",		/* HP_345 */
	"375",		/* HP_375 */
	"400",		/* HP_400 */
	"380",		/* HP_380 */
	"425",		/* HP_425 */
	"433",		/* HP_433 */
	"385",		/* HP_385 */
	"382",		/* HP_382 */
};

/* Map mmuid to single letter designation in 4xx models (e.g. 425s, 425t) */
char hp300_designations[] = "    ttss e";

void
identifycpu()
{
	const char *t;
	char mc, *td;
	int len;
#ifdef FPSP
	extern u_long fpvect_tab, fpvect_end, fpsp_tab;
#endif

	/*
	 * Map machineid to model name.
	 */
	if (machineid >= sizeof(hp300_models) / sizeof(char *)) {
		printf("\nunknown machineid %d\n", machineid);
		goto lose;
	}
	t = hp300_models[machineid];

	/*
	 * Look up special designation (425s, 425t, etc) by mmuid.
	 */
	if (mmuid < strlen(hp300_designations) &&
	    hp300_designations[mmuid] != ' ') {
		td = &hp300_designations[mmuid];
		td[1] = '\0';
	} else
		td = "";

	/*
	 * ...and the CPU type
	 */
	switch (cputype) {
	case CPU_68040:
		mc = '4';
		/* adjust cpuspeed by 3/8 on '040 boxes */
		cpuspeed *= 3;
		cpuspeed /= 8;
		break;
	case CPU_68030:
		mc = '3';
		break;
	case CPU_68020:
		mc = '2';
		break;
	default:
		printf("\nunknown cputype %d\n", cputype);
		goto lose;
	}
	snprintf(cpu_model, sizeof cpu_model,
	    "HP 9000/%s%s (%dMHz MC680%c0 CPU", t, td, cpuspeed, mc);

	/*
	 * ...and the MMU type.
	 */
	switch (mmutype) {
	case MMU_68040:
	case MMU_68030:
		strlcat(cpu_model, "+MMU", sizeof cpu_model);
		break;
	case MMU_68851:
		strlcat(cpu_model, ", MC68851 MMU", sizeof cpu_model);
		break;
	case MMU_HP:
		strlcat(cpu_model, ", HP MMU", sizeof cpu_model);
		break;
	default:
		printf("%s\nunknown MMU type %d\n", cpu_model, mmutype);
		panic("startup");
	}

	/*
	 * ...and the FPU type.
	 */
	switch (fputype) {
	case FPU_68040:
		strlcat(cpu_model, "+FPU", sizeof cpu_model);
		break;
	case FPU_68882:
		len = strlen(cpu_model);
		snprintf(cpu_model + len, sizeof cpu_model - len,
		    ", %dMHz MC68882 FPU", cpuspeed);
		break;
	case FPU_68881:
		len = strlen(cpu_model);
		snprintf(cpu_model + len, sizeof cpu_model - len,
		    ", %dMHz MC68881 FPU", machineid == HP_350 ? 20 : 16);
		break;
	default:
		strlcat(cpu_model, ", unknown FPU", sizeof cpu_model);
	}

	/*
	 * ...and finally, the cache type.
	 */
	if (cputype == CPU_68040)
		strlcat(cpu_model, ", 4k on-chip physical I/D caches",
		    sizeof cpu_model);
	else {
		len = strlen(cpu_model);
		switch (ectype) {
		case EC_VIRT:
			snprintf(cpu_model + len, sizeof cpu_model - len,
			    ", %dK virtual-address cache",
			    machineid == HP_320 ? 16 : 32);
			break;
		case EC_PHYS:
			snprintf(cpu_model + len, sizeof cpu_model - len,
			    ", %dK physical-address cache",
			    machineid == HP_370 ? 64 : 32);
			break;
		}
	}

	printf("%s)\n", cpu_model);
#ifdef DEBUG
	printf("cpu: delay divisor %d", delay_divisor);
	if (mmuid)
		printf(", mmuid %d", mmuid);
	printf("\n");
#endif

	/*
	 * Now that we have told the user what they have,
	 * let them know if that machine type isn't configured.
	 */
	switch (machineid) {
	case -1:		/* keep compilers happy */
#if !defined(HP320)
	case HP_320:
#endif
#if !defined(HP330)
	case HP_330:
#endif
#if !defined(HP340)
	case HP_340:
#endif
#if !defined(HP345)
	case HP_345:
#endif
#if !defined(HP350)
	case HP_350:
#endif
#if !defined(HP360) && !defined(HP_362)
	case HP_36X:
#endif
#if !defined(HP370)
	case HP_370:
#endif
#if !defined(HP375)
	case HP_375:
#endif
#if !defined(HP380)
	case HP_380:
#endif
#if !defined(HP382)
	case HP_382:
#endif
#if !defined(HP385)
	case HP_385:
#endif
#if !defined(HP400)
	case HP_400:
#endif
#if !defined(HP425)
	case HP_425:
#endif
#if !defined(HP433)
	case HP_433:
#endif
		panic("SPU type not configured for machineid %d", machineid);
	default:
		break;
	}

#ifdef FPSP
	if (cputype == CPU_68040) {
		bcopy(&fpsp_tab, &fpvect_tab,
		    (&fpvect_end - &fpvect_tab) * sizeof (fpvect_tab));
	}
#endif

	return;
lose:
	panic("startup");
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
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));
	case CPU_CPUSPEED:
		return (sysctl_rdint(oldp, oldlenp, newp, cpuspeed));
	case CPU_MACHINEID:
		return (sysctl_rdint(oldp, oldlenp, newp, machineid));
	case CPU_MMUID:
		return (sysctl_rdint(oldp, oldlenp, newp, mmuid));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int	waittime = -1;

void
boot(howto)
	int howto;
{
	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(&curproc->p_addr->u_pcb);

	/*
	 * Prevent mi code from spinning disks off, in case the operator
	 * changes his mind and prefers to reboot - we can't power down
	 * the machine, and it will not send a spin up command to the
	 * disks.
	 */
	howto &= ~RB_POWERDOWN;

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
	}
	if_downall();

	uvm_shutdown();
	splhigh();		/* Disable interrupts. */

	/* If rebooting and a dump is requested do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	/* Finally, halt/reboot the system. */
	if (howto & RB_HALT) {
		printf("System halted.  Hit any key to reboot.\n\n");
		cnpollc(1);
		while (cngetc() == 0);
		cnpollc(0);
	}

	printf("rebooting...\n");
	DELAY(1000000);
	doboot();
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */
cpu_kcore_hdr_t cpu_kcore_hdr;

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf(void)
{
	int nblks;	/* size of dump area */

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	/*
	 * Since lowram starts two pages after the beginning of memory,
	 * we're not dumping exactly all the memory.
	 */
	dumpsize = physmem - 2;

	/* hp300 only uses a single segment. */
	cpu_kcore_hdr.ram_segs[0].start = lowram;
	cpu_kcore_hdr.ram_segs[0].size = ptoa(dumpsize);
	cpu_kcore_hdr.mmutype = mmutype;
	cpu_kcore_hdr.kernel_pa = lowram;
	cpu_kcore_hdr.sysseg_pa = pmap_kernel()->pm_stpa;

	/* Always skip the first block, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Dump physical memory onto the dump device.  Called by doadump()
 * in locore.s or by boot() here in machdep.c
 */
void
dumpsys()
{
	daddr64_t blkno;	/* current block to write */
				/* dump routine */
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	int pg;			/* page being dumped */
	paddr_t maddr;		/* PA being dumped */
	int error;		/* error code from (*dump)() */
	kcore_seg_t *kseg_p;
	cpu_kcore_hdr_t *chdr_p;
	char dump_hdr[dbtob(1)];	/* XXX assume hdr fits in 1 block */
	extern int msgbufmapped;

	/* XXX initialized here because of gcc lossage */
	maddr = lowram;
	pg = 0;

	/* Don't put dump messages in msgbuf. */
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

	kseg_p = (kcore_seg_t *)dump_hdr;
	chdr_p = (cpu_kcore_hdr_t *)&dump_hdr[ALIGN(sizeof(*kseg_p))];
	bzero(dump_hdr, sizeof(dump_hdr));

	/*
	 * Generate a segment header
	 */
	CORE_SETMAGIC(*kseg_p, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg_p->c_size = dbtob(1) - ALIGN(sizeof(*kseg_p));

	/*
	 * Add the md header
	 */

	*chdr_p = cpu_kcore_hdr;

	printf("dump ");
	maddr = cpu_kcore_hdr.ram_segs[0].start;
	/* Dump the header. */
	error = (*dump) (dumpdev, blkno++, (caddr_t)dump_hdr, dbtob(1));
	switch (error) {
	case 0:
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
	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many MBs we have dumped */
		if (pg && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		pmap_enter(pmap_kernel(), (vaddr_t)vmmap, maddr,
		    VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);

		pmap_update(pmap_kernel());
		error = (*dump)(dumpdev, blkno, vmmap, NBPG);
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

void
initcpu()
{

	parityenable();
#ifdef USELEDS
	ledinit();
#endif
}

void
straytrap(pc, evec)
	int pc;
	u_short evec;
{
	printf("unexpected trap (vector offset %x) from %x\n",
	       evec & 0xFFF, pc);
}

/* XXX should change the interface, and make one badaddr() function */

int	*nofault;

int
badaddr(addr)
	caddr_t addr;
{
	int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile short *)addr;
	nofault = (int *) 0;
	return(0);
}

int
badbaddr(addr)
	caddr_t addr;
{
	int i;
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		return(1);
	}
	i = *(volatile char *)addr;
	nofault = (int *) 0;
	return(0);
}

static int innmihand;	/* simple mutex */

/*
 * Level 7 interrupts can be caused by HIL keyboards (in cooked mode only,
 * but we run them in raw mode) or parity errors.
 */
void
nmihand(frame)
	struct frame frame;
{

	/* Prevent unwanted recursion. */
	if (innmihand)
		return;
	innmihand = 1;

	if (parityerror(&frame)) {
		innmihand = 0;
		return;
	}

	/* panic?? */
	printf("unexpected level 7 interrupt ignored\n");

	innmihand = 0;
}

/*
 * Parity error section.  Contains magic.
 */
#define PARREG		((volatile short *)IIOV(0x5B0000))
static int gotparmem = 0;
#ifdef DEBUG
int ignorekperr = 0;	/* ignore kernel parity errors */
#endif

/*
 * Enable parity detection
 */
void
parityenable()
{
	label_t	faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *) 0;
		printf("No parity memory\n");
		return;
	}
	*PARREG = 1;
	nofault = (int *) 0;
	gotparmem = 1;
	printf("Parity detection enabled\n");
}

/*
 * Determine if level 7 interrupt was caused by a parity error
 * and deal with it if it was.  Returns 1 if it was a parity error.
 */
int
parityerror(fp)
	struct frame *fp;
{
	if (!gotparmem)
		return(0);
	*PARREG = 0;
	DELAY(10);
	*PARREG = 1;
	if (panicstr) {
		printf("parity error after panic ignored\n");
		return (1);
	}
	if (!parityerrorfind())
		printf("WARNING: transient parity error ignored\n");
	else if (USERMODE(fp->f_sr)) {
		log(LOG_ERR, "pid %d was killed: memory parity error\n",
		    curproc->p_pid);
		uprintf("sorry, pid %d killed: memory parity error\n",
		    curproc->p_pid);
		psignal(curproc, SIGKILL);
#ifdef DEBUG
	} else if (ignorekperr) {
		printf("WARNING: kernel parity error ignored\n");
#endif
	} else {
		regdump(&(fp->F_t), 128);
		panic("kernel parity error");
	}
	return (1);
}

/*
 * Yuck!  There has got to be a better way to do this!
 * Searching all of memory with interrupts blocked can lead to disaster.
 */
int
parityerrorfind()
{
	static label_t parcatch;
	static int looking = 0;
	volatile int pg, o, s;
	volatile int *ip;
	int i;
	int found;

#ifdef lint
	i = o = pg = 0; if (i) return(0);
#endif
	/*
	 * If looking is true we are searching for a known parity error
	 * and it has just occurred.  All we do is return to the higher
	 * level invocation.
	 */
	if (looking)
		longjmp(&parcatch);
	s = splhigh();
	/*
	 * If setjmp returns true, the parity error we were searching
	 * for has just occurred (longjmp above) at the current pg+o
	 */
	if (setjmp(&parcatch)) {
		printf("Parity error at 0x%x\n", ptoa(pg)|o);
		found = 1;
		goto done;
	}
	/*
	 * If we get here, a parity error has occurred for the first time
	 * and we need to find it.  We turn off any external caches and
	 * loop thru memory, testing every longword til a fault occurs and
	 * we regain control at setjmp above.  Note that because of the
	 * setjmp, pg and o need to be volatile or their values will be lost.
	 */
	looking = 1;
	ecacheoff();
	for (pg = atop(lowram); pg < atop(lowram)+physmem; pg++) {
		pmap_kenter_pa((vaddr_t)vmmap, ptoa(pg), VM_PROT_READ);
		pmap_update(pmap_kernel());
		ip = (int *)vmmap;
		for (o = 0; o < PAGE_SIZE; o += sizeof(int))
			i = *ip++;
	}
	/*
	 * Getting here implies no fault was found.  Should never happen.
	 */
	printf("Couldn't locate parity error\n");
	found = 0;
done:
	looking = 0;
	ecacheon();	/* pmap_kremove() may cause a cache flush */
	pmap_kremove((vaddr_t)vmmap, PAGE_SIZE);
	pmap_update(pmap_kernel());
	splx(s);
	return(found);
}
