/*	$OpenBSD: machdep.c,v 1.36 2010/04/24 18:44:25 miod Exp $	*/
/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/device.h>

#include <machine/asm.h>
#include <machine/asm_macro.h>
#include <machine/autoconf.h>
#include <machine/avcommon.h>
#include <machine/board.h>
#include <machine/bus.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/prom.h>
#include <machine/reg.h>
#include <machine/trap.h>
#ifdef M88100
#include <machine/m88100.h>
#endif

#include <aviion/dev/vmevar.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_swap.h>

#include "ksyms.h"
#if DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>
#endif /* DDB */

void	aviion_bootstrap(void);
void	aviion_identify(void);
void	consinit(void);
__dead void doboot(void);
void	dumpconf(void);
void	dumpsys(void);
void	savectx(struct pcb *);
void	secondary_main(void);
vaddr_t	secondary_pre_main(void);

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

int32_t cpuid;

int cputyp;					/* set in locore.S */
int avtyp;
const struct board *platform;

/* multiplication factor for delay() */
u_int	aviion_delay_const = 33;

vaddr_t first_addr;
vaddr_t last_addr;

vaddr_t avail_start, avail_end;
vaddr_t virtual_avail, virtual_end;

extern struct user *proc0paddr;

/*
 * Interrupt masks, one per IPL level.
 */
u_int32_t int_mask_val[NIPLS];
u_int32_t ext_int_mask_val[NIPLS];

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
	CN_LOWPRI
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
	int i;
	vaddr_t minaddr, maxaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in aviion_bootstrap() to compensate.
	 */
	for (i = 0; i < atop(MSGBUFSIZE); i++)
		pmap_kenter_pa((paddr_t)msgbufp + i * PAGE_SIZE,
		    avail_end + i * PAGE_SIZE, VM_PROT_READ | VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Grab machine dependent memory spaces
	 */
	platform->startup();

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

	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

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

	uvm_shutdown();
	splhigh();		/* Disable interrupts. */

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
	cpu_kcore_hdr.ram_segs[0].size = ptoa(physmem);
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

#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif

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
 * Determine CPU number and set it, then allocate its startup stack.
 *
 * Running on a minimal stack here, with interrupts disabled; do nothing fancy.
 */
vaddr_t
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
	 * Allocate UPAGES contiguous pages for the startup stack.
	 */
	init_stack = uvm_km_zalloc(kernel_map, USPACE);
	if (init_stack == (vaddr_t)NULL) {
		printf("cpu%d: unable to allocate startup stack\n",
		    ci->ci_cpuid);
		for (;;) ;
	}

	return (init_stack);
}

/*
 * Further secondary CPU initialization.
 *
 * We are now running on our startup stack, with proper page tables.
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

	set_psr(get_psr() & ~PSR_IND);
	spl0();

	SCHED_LOCK(s);
	cpu_switchto(NULL, sched_chooseproc());
}

#endif	/* MULTIPROCESSOR */

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
	case CPU_CPUTYPE:
		return (sysctl_rdint(oldp, oldlenp, newp, cputyp));
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

	aviion_identify();

	cn_tab = &bootcons;
	platform->bootstrap();
	/* we can use printf() from here. */

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

	setup_board_config();
	master_cpu = cmmu_init();
	set_cpu_number(master_cpu);
	SET(curcpu()->ci_flags, CIF_ALIVE | CIF_PRIMARY);

#ifdef M88100
	if (CPU_IS88100) {
		m88100_apply_patches();
	}
#endif

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
	pmap_bootstrap((vaddr_t)trunc_page((vaddr_t)&kernelstart));

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

	for (cpu = 0; cpu < ncpusfound; cpu++) {
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
	cp->cn_pri = CN_LOWPRI;
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

int
getipl(void)
{
	return (int)platform->getipl();
}

int
setipl(int level)
{
	return (int)platform->setipl((u_int)level);
}

int
raiseipl(int level)
{
	return (int)platform->raiseipl((u_int)level);
}

void
intsrc_enable(u_int intsrc, int ipl)
{
	u_int32_t psr;
	u_int64_t intmask = platform->intsrc(intsrc);
	int i;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = IPL_NONE; i < ipl; i++) {
		int_mask_val[i] |= (u_int32_t)intmask;
		ext_int_mask_val[i] |= (u_int32_t)(intmask >> 32);
	}
	setipl(getipl());

	set_psr(psr);
}

void
intsrc_disable(u_int intsrc)
{
	u_int32_t psr;
	u_int64_t intmask = platform->intsrc(intsrc);
	int i;

	psr = get_psr();
	set_psr(psr | PSR_IND);

	for (i = 0; i < NIPLS; i++) {
		int_mask_val[i] &= ~((u_int32_t)intmask);
		ext_int_mask_val[i] &= ~((u_int32_t)(intmask >> 32));
	}
	setipl(getipl());

	set_psr(psr);
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

struct aviion_system {
	int32_t			 cpuid;
	const char		*model;
	const struct board	*platform;
	const char		*kernel_option;
};
static const struct aviion_system aviion_systems[] = {
#define	BOARD_UNSUPPORTED	NULL, NULL
#ifdef AV400
#define	BOARD_AV400		&board_av400, NULL
#else
#define	BOARD_AV400		NULL, "AV400"
#endif
#ifdef AV530
#define	BOARD_AV530		&board_av530, NULL
#else
#define	BOARD_AV530		NULL, "AV530"
#endif
#ifdef AV5000
#define	BOARD_AV5000		&board_av5000, NULL
#else
#define	BOARD_AV5000		BOARD_UNSUPPORTED /* NULL, "AV5000" */
#endif
#ifdef AV6280
#define	BOARD_AV6280		&board_av6280, NULL
#else
#define	BOARD_AV6280		BOARD_UNSUPPORTED /* NULL, "AV6280" */
#endif
	{ AVIION_300_310,		"300/310",	BOARD_AV400 },
	{ AVIION_5100_6100,		"5100/6100",	BOARD_UNSUPPORTED },
	{ AVIION_400_4000,		"400/4000",	BOARD_AV400 },
	{ AVIION_410_4100,		"410/4100",	BOARD_AV400 },
	{ AVIION_300C_310C,		"300C/310C",	BOARD_AV400 },
	{ AVIION_5200_6200,		"5200/6200",	BOARD_AV5000 },
	{ AVIION_5240_6240,		"5240/6240",	BOARD_AV5000 },
	{ AVIION_300CD_310CD,		"300CD/310CD",	BOARD_AV400 },
	{ AVIION_300D_310D,		"300D/310D",	BOARD_AV400 },
	{ AVIION_4600_530,		"4600/530",	BOARD_AV530 },
	{ AVIION_4300_25,		"4300-25",	BOARD_AV400 },
	{ AVIION_4300_20,		"4300-20",	BOARD_AV400 },
	{ AVIION_4300_16,		"4300-16",	BOARD_AV400 },
	{ AVIION_5255_6255,		"5255/6255",	BOARD_AV5000 },
	{ AVIION_350,			"350",		BOARD_UNSUPPORTED },
	{ AVIION_6280,			"6280",		BOARD_AV6280 },
	{ AVIION_8500_9500,		"8500/9500",	BOARD_UNSUPPORTED },
	{ AVIION_9500_HA,		"9500HA",	BOARD_UNSUPPORTED },
	{ AVIION_500,			"500",		BOARD_UNSUPPORTED },
	{ AVIION_5500,			"5500",		BOARD_UNSUPPORTED },
	{ AVIION_450,			"450",		BOARD_UNSUPPORTED },
	{ AVIION_8500_9500_45_1MB,	"8500/9500-45",	BOARD_UNSUPPORTED },
	{ AVIION_10000,			"10000",	BOARD_UNSUPPORTED },
	{ AVIION_10000_QT,		"10000QT",	BOARD_UNSUPPORTED },
	{ AVIION_5500PLUS,		"5500+",	BOARD_UNSUPPORTED },
	{ AVIION_450PLUS,		"450+",		BOARD_UNSUPPORTED },
	{ AVIION_8500_9500_50_1MB,	"8500/9500-50",	BOARD_UNSUPPORTED },
	{ AVIION_8500_9500_50_2MB,	"8500/9500-50d", BOARD_UNSUPPORTED },

	{ AVIION_UNKNOWN1,		"\"Montezuma\"", BOARD_UNSUPPORTED },
	{ AVIION_UNKNOWN2,		"\"Montezuma\"", BOARD_UNSUPPORTED },
	{ AVIION_UNKNOWN3,		"\"Flintstone\"", BOARD_UNSUPPORTED },
	{ AVIION_UNKNOWN1_DIS,		"\"Montezuma-\"", BOARD_UNSUPPORTED },
	{ AVIION_UNKNOWN2_DIS,		"\"Montezuma-\"", BOARD_UNSUPPORTED },

	{ 0 }
#undef	BOARD_AV6280
#undef	BOARD_AV5000
#undef	BOARD_AV530
#undef	BOARD_AV400
};

void
aviion_identify()
{
	const struct aviion_system *system;
	char excuse[512];
	extern char *hw_vendor, *hw_prod;

	cpuid = scm_cpuid();
	hostid = scm_sysid();

	for (system = aviion_systems; ; system++) {
		if (system->cpuid != 0 && system->cpuid != cpuid)
			continue;

		hw_vendor = "Data General";
		hw_prod = "AViiON";
		strlcpy(cpu_model, system->model, sizeof cpu_model);

		if (system->platform != NULL) {
			platform = system->platform;
			return;
		}

		if (system->kernel_option != NULL) {
			/* unconfigured system */
			snprintf(excuse, sizeof excuse, "\n"
			    "Sorry, support for the %s system is not present\n"
			    "in this OpenBSD/" MACHINE " kernel.\n"
			    "Please recompile your kernel with\n"
			    "\toption\t%s\n"
			    "in the kernel configuration file.\n",
			    system->model, system->kernel_option);
		} else if (system->cpuid != 0) {
			/* unsupported system */
			snprintf(excuse, sizeof excuse, "\n"
			    "Sorry, OpenBSD/" MACHINE
			    " does not support the %s system"
			    " (cpuid %04x) yet.\n\n"
			    "Please contact <m88k@openbsd.org>\n",
			    system->model, cpuid);
		} else {
			/* unrecgonized system */
			snprintf(excuse, sizeof excuse, "\n"
			    "Sorry, OpenBSD/" MACHINE
			    " does not recognize this system (cpuid %04x).\n\n"
			    "Please contact <m88k@openbsd.org>\n",
			    cpuid);
		}
	}

	scm_printf(excuse);
	scm_halt();
}
