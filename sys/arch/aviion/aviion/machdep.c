/* $OpenBSD: machdep.c,v 1.11 2007/10/10 15:53:51 art Exp $	*/
/*
 * Copyright (c) 1998, 1999, 2000, 2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/kcore.h>

#include <machine/asm.h>
#include <machine/asm_macro.h>
#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/prom.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include "ksyms.h"
#if DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>
#endif /* DDB */

caddr_t	allocsys(caddr_t);
void	aviion_bootstrap(void);
int	aviion_identify(void);
void	consinit(void);
__dead void doboot(void);
void	dumpconf(void);
void	dumpsys(void);
u_int	getipl(void);
void	identifycpu(void);
void	savectx(struct pcb *);
void	secondary_main(void);
void	secondary_pre_main(void);

intrhand_t intr_handlers[NVMEINTR];

int physmem;	  /* available physical memory, in pages */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

#ifdef MULTIPROCESSOR
__cpu_simple_lock_t cpu_mutex = __SIMPLELOCK_UNLOCKED;
#endif

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef	BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

/*
 * Info for CTL_HW
 */
char  machine[] = MACHINE;	 /* cpu "architecture" */
char  cpu_model[120];

#if defined(DDB) || NKSYMS > 0
extern vaddr_t esym;
#endif

const char *prom_bootargs;			/* set in locore.S */
char bootargs[256];				/* local copy */
u_int bootdev, bootunit, bootpart;		/* set in locore.S */

int cputyp;					/* set in locore.S */
int cpuspeed = 20;				/* safe guess */
int avtyp;
const struct board *platform;

vaddr_t first_addr;
vaddr_t last_addr;

vaddr_t avail_start, avail_end;
vaddr_t virtual_avail, virtual_end;

extern struct user *proc0paddr;

/*
 * This is to fake out the console routines, while booting.
 * We could use directly the bugtty console, but we want to be able to
 * configure a kernel without bugtty since we do not necessarily need a
 * full-blown console driver.
 */
cons_decl(boot);

struct consdev bootcons = {
	NULL,
	NULL,
	bootcngetc,
	bootcnputc,
	nullcnpollc,
	NULL,
	makedev(14, 0),
	CN_NORMAL
};

/*
 * Early console initialization: called early on from main, before vm init.
 * We want to stick to the BUG routines for now, and we'll switch to the
 * real console in cpu_startup().
 */
void
consinit()
{
	cn_tab = NULL;
	cninit();

#if defined(DDB)
	db_machine_init();
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

void
identifycpu()
{
#if 0
	/* XXX FILL ME */
	cpuspeed = getcpuspeed(&brdid);
#endif

	strlcpy(cpu_model, platform->descr, sizeof cpu_model);
}

/*
 * Set up real-time clocks.
 * These function pointers are set in dev/clock.c.
 */
void
cpu_initclocks()
{
	platform->init_clocks();
}

void
setstatclockrate(int newhz)
{
	/* function stub */
}


void
cpu_startup()
{
	caddr_t v;
	int sz, i;
	vaddr_t minaddr, maxaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in aviion_bootstrap() to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_kenter_pa((paddr_t)msgbufp + i * PAGE_SIZE,
		    avail_end + i * PAGE_SIZE, VM_PROT_READ | VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);

	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Grab machine dependent memory spaces
	 */
	platform->startup();

	/*
	 * Determine how many buffers to allocate.
	 * We allocate bufcachepercent% of memory for buffer space.
	 */
	if (bufpages == 0)
		bufpages = physmem * bufcachepercent / 100;

	/* Restrict to at most 25% filled kvm */
	if (bufpages >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / PAGE_SIZE / 4) 
		bufpages = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    PAGE_SIZE / 4;

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate map for physio.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail mem = %ld (%d pages)\n", ptoa(uvmexp.free), uvmexp.free);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Set up interrupt handlers.
	 */
	for (i = 0; i < NVMEINTR; i++)
		SLIST_INIT(&intr_handlers[i]);

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
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	return v;
}

__dead void
doboot()
{
	printf("Rebooting system...\n\n");
	cmmu_shutdown();
	scm_reboot(NULL);
	/*NOTREACHED*/
	for (;;);		/* appease gcc */
}

__dead void
boot(howto)
	int howto;
{
	/* take a snapshot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(curpcb);

	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0) {
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0)
			resettodr();
		else
			printf("WARNING: not updating battery clock\n");
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("System halted.\n\n");
		cmmu_shutdown();
		scm_halt();
	}

	doboot();

	for (;;);
	/*NOTREACHED*/
}

unsigned dumpmag = 0x8fca0101;	 /* magic number for savecore */
int   dumpsize = 0;	/* also for savecore */
long  dumplo = 0;
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

	dumpsize = physmem;

	/* aviion only uses a single segment. */
	cpu_kcore_hdr.ram_segs[0].start = 0;
	cpu_kcore_hdr.ram_segs[0].size = ctob(physmem);
	cpu_kcore_hdr.cputype = cputyp;

	/*
	 * Don't dump on the first block
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize + 1 > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo) - 1;
	if (dumplo < nblks - ctod(dumpsize) - 1)
		dumplo = nblks - ctod(dumpsize) - 1;
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{
	int maj;
	int psize;
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

	msgbufmapped = 0;

	/* Make sure dump device is valid. */
	if (dumpdev == NODEV)
		return;
	if (dumpsize == 0) {
		dumpconf();
		if (dumpsize == 0)
			return;
	}
	maj = major(dumpdev);
	if (dumplo < 0) {
		printf("\ndump to dev %u,%u not possible\n", maj,
		    minor(dumpdev));
		return;
	}
	dump = bdevsw[maj].d_dump;
	blkno = dumplo;

	printf("\ndumping to dev %u,%u offset %ld\n", maj,
	    minor(dumpdev), dumplo);

	/* Setup the dump header */
	kseg_p = (kcore_seg_t *)dump_hdr;
	chdr_p = (cpu_kcore_hdr_t *)&dump_hdr[ALIGN(sizeof(*kseg_p))];
	bzero(dump_hdr, sizeof(dump_hdr));

	CORE_SETMAGIC(*kseg_p, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	kseg_p->c_size = dbtob(1) - ALIGN(sizeof(*kseg_p));
	*chdr_p = cpu_kcore_hdr;

	printf("dump ");
	psize = (*bdevsw[maj].d_psize)(dumpdev);
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	/* Dump the header. */
	error = (*dump)(dumpdev, blkno++, (caddr_t)dump_hdr, dbtob(1));
	if (error != 0)
		goto abort;

	maddr = (paddr_t)0;
	for (pg = 0; pg < dumpsize; pg++) {
#define NPGMB	(1024 * 1024 / PAGE_SIZE)
		/* print out how many MBs we have dumped */
		if (pg != 0 && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef NPGMB
		pmap_enter(pmap_kernel(), (vaddr_t)vmmap, maddr,
		    VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);

		error = (*dump)(dumpdev, blkno, vmmap, PAGE_SIZE);
		if (error == 0) {
			maddr += PAGE_SIZE;
			blkno += btodb(PAGE_SIZE);
		} else
			break;
	}
abort:
	switch (error) {
	case 0:
		printf("succeeded\n");
		break;

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

	default:
		printf("error %d\n", error);
		break;
	}
}

#ifdef MULTIPROCESSOR

/*
 * Secondary CPU early initialization routine.
 * Determine CPU number and set it, then allocate the idle pcb (and stack).
 *
 * Running on a minimal stack here, with interrupts disabled; do nothing fancy.
 */
void
secondary_pre_main()
{
	struct cpu_info *ci;

	set_cpu_number(cmmu_cpu_number()); /* Determine cpu number by CMMU */
	ci = curcpu();
	ci->ci_curproc = &proc0;

	splhigh();

	/*
	 * Setup CMMUs and translation tables (shared with the master cpu).
	 */
	pmap_bootstrap_cpu(ci->ci_cpuid);

	/*
	 * Allocate UPAGES contiguous pages for the idle PCB and stack.
	 */
	ci->ci_idle_pcb = (struct pcb *)uvm_km_zalloc(kernel_map, USPACE);
	if (ci->ci_idle_pcb == NULL) {
		printf("cpu%d: unable to allocate idle stack\n", ci->ci_cpuid);
	}
}

/*
 * Further secondary CPU initialization.
 *
 * We are now running on our idle stack, with proper page tables.
 * There is nothing to do but display some details about the CPU and its CMMUs.
 */
void
secondary_main()
{
	struct cpu_info *ci = curcpu();
	int s;

	cpu_configuration_print(0);
	sched_init_cpu(ci);
	ncpus++;
	__cpu_simple_unlock(&cpu_mutex);

	microuptime(&ci->ci_schedstate.spc_runtime);
	ci->ci_curproc = NULL;

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

#endif	/* MULTIPROCESSOR */

/*
 * Try to insert ihand in the list of handlers for vector vec.
 */
int
intr_establish(int vec, struct intrhand *ihand, const char *name)
{
	struct intrhand *intr;
	intrhand_t *list;

	if (vec < 0 || vec >= NVMEINTR) {
#ifdef DIAGNOSTIC
		printf("intr_establish: vec (0x%x) not between 0x00 and 0xff\n",
		      vec);
#endif /* DIAGNOSTIC */
		return (EINVAL);
	}

	list = &intr_handlers[vec];
	if (!SLIST_EMPTY(list)) {
		intr = SLIST_FIRST(list);
		if (intr->ih_ipl != ihand->ih_ipl) {
#ifdef DIAGNOSTIC
			printf("intr_establish: there are other handlers with "
			    "vec (0x%x) at ipl %x, but you want it at %x\n",
			    vec, intr->ih_ipl, ihand->ih_ipl);
#endif /* DIAGNOSTIC */
			return (EINVAL);
		}
	}

	evcount_attach(&ihand->ih_count, name, (void *)&ihand->ih_ipl,
	    &evcount_intr);
	SLIST_INSERT_HEAD(list, ihand, ih_link);
	return (0);
}

void
nmihand(void *frame)
{
#ifdef DDB
	printf("Abort switch pressed\n");
	if (db_console) {
		/*
		 * We can't use Debugger() here, as we are coming from an
		 * exception, and can't assume anything on the state we are
		 * in. Invoke the post-trap ddb entry directly.
		 */
		extern void m88k_db_trap(int, struct trapframe *);
		m88k_db_trap(T_KDB_ENTRY, (struct trapframe *)frame);
	}
#endif
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{

	return (ENOEXEC);
}

int
sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct sys_sysarch_args	/* {
	   syscallarg(int) op;
	   syscallarg(char *) parm;
	} */ *uap = v;
#endif

	return (ENOSYS);
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

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR); /* overloaded */

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
	/*NOTREACHED*/
}

/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */
void
aviion_bootstrap()
{
	extern int kernelstart;
	extern char *end;
#ifndef MULTIPROCESSOR
	cpuid_t master_cpu;
#endif

	/* Save a copy of our commandline before it gets overwritten. */
	strlcpy(bootargs, prom_bootargs, sizeof bootargs);

	avtyp = aviion_identify();

	/* Set up interrupt and fp exception handlers based on the machine. */
	switch (avtyp) {
#ifdef AV400
	case AV_400:
		platform = &board_av400;
		break;
#endif
#ifdef AV530
	case AV_530:
		platform = &board_av530;
		break;
#endif
#ifdef AV5000
	case AV_5000:
		platform = &board_av5000;
		break;
#endif
#ifdef AV6280
	case AV_6280:
		platform = &board_av6280;
		break;
#endif
	default:
		scm_printf("Sorry, OpenBSD/" MACHINE
		    " does not support this model.\n");
		scm_halt();
		break;
	};

	cn_tab = &bootcons;
	/* we can use printf() from here. */

	platform->bootstrap();

	/* Parse the commandline */
	cmdline_parse();

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

#if defined(DDB) || NKSYMS > 0
	if (esym != 0)
		first_addr = esym;
	else
#endif
		first_addr = (vaddr_t)&end;
	first_addr = round_page(first_addr);

	last_addr = platform->memsize();
	physmem = btoc(last_addr);

	setup_board_config();
	master_cpu = cmmu_init();
	set_cpu_number(master_cpu);

	/*
	 * Now that set_cpu_number() set us with a valid cpu_info pointer,
	 * we need to initialize p_addr and curpcb before autoconf, for the
	 * fault handler to behave properly [except for badaddr() faults,
	 * which can be taken care of without a valid curcpu()].
	 */
	proc0.p_addr = proc0paddr;
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	avail_start = round_page(first_addr);
	avail_end = last_addr;

	/* Steal MSGBUFSIZE at the top of physical memory for msgbuf. */
	avail_end -= round_page(MSGBUFSIZE);
	pmap_bootstrap((vaddr_t)trunc_page((unsigned)&kernelstart));

	/*
	 * Tell the VM system about available physical memory.
	 * The aviion systems only have one contiguous area.
	 *
	 * XXX However, on series 5000, SRAM overlaps a low memory range,
	 * XXX so we will need to upload two ranges of pages on them.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), VM_FREELIST_DEFAULT);

	/* Initialize the "u-area" pages. */
	bzero((caddr_t)curpcb, USPACE);
}

#ifdef MULTIPROCESSOR
void
cpu_boot_secondary_processors()
{
	cpuid_t cpu;
	int rc;
	extern void secondary_start(void);

	for (cpu = 0; cpu < max_cpus; cpu++) {
		if (cpu != curcpu()->ci_cpuid) {
			rc = scm_spincpu(cpu, (vaddr_t)secondary_start);
			if (rc != 0)
				printf("cpu%d: spin_cpu error %d\n", cpu, rc);
		}
	}
}
#endif

/*
 * Boot console routines:
 * Enables printing of boot messages before consinit().
 */
void
bootcnprobe(cp)
	struct consdev *cp;
{
	cp->cn_dev = makedev(0, 0);
	cp->cn_pri = CN_NORMAL;
}

void
bootcninit(cp)
	struct consdev *cp;
{
	/* Nothing to do */
}

int
bootcngetc(dev)
	dev_t dev;
{
	return (scm_getc());
}

void
bootcnputc(dev, c)
	dev_t dev;
	int c;
{
	if (c == '\n')
		scm_putcrlf();
	else
		scm_putc(c);
}

u_int
getipl(void)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = platform->getipl();
	set_psr(psr);
	return curspl;
}

u_int
setipl(u_int level)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = platform->setipl(level);

	/*
	 * The flush pipeline is required to make sure the above change gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}

u_int
raiseipl(u_int level)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = platform->raiseipl(level);

	/*
	 * The flush pipeline is required to make sure the above change gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}

u_char hostaddr[6];

void
myetheraddr(u_char *cp)
{
	bcopy(hostaddr, cp, 6);
}

/*
 * Attempt to identify which AViiON flavour we are running on.
 * The only thing we can do at this point is peek at random addresses and
 * see if they cause bus errors, or not.
 *
 * These heuristics are probably not the best; feel free to come with better
 * ones...
 */
int
aviion_identify()
{
	/*
	 * We don't know anything about 88110-based models.
	 * Note that we can't use CPU_IS81x0 here since these are optimized
	 * if the kernel you're running is compiled for only one processor
	 * type, and we want to check against the real hardware.
	 */
	if (cputyp == CPU_88110)
		return (0);

	/*
	 * Series 100/200/300/400/3000/4000/4300 do not have the VIRQLV
	 * register at 0xfff85000.
	 */
	if (badaddr(0xfff85000, 4) != 0)
		return (AV_400);

	/*
	 * Series 5000 and 6000 do not have an RTC counter at 0xfff8f084.
	 */
	if (badaddr(0xfff8f084, 4) != 0)
		return (AV_5000);

	/*
	 * Series 4600/530 have IOFUSEs at 0xfffb0040 and 0xfffb00c0.
	 */
	if (badaddr(0xfffb0040, 1) == 0 && badaddr(0xfffb00c0, 1) == 0)
		return (AV_530);

	/*
	 * Series 6280/8000-8 fall here.
	 */
	return (AV_6280);
}
