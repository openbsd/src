/*	$OpenBSD: machdep.c,v 1.125 2011/06/05 19:41:07 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * Copyright (c) 1999 Steve Murphree, Jr. (68060 support)
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
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
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/mount.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/evcount.h>

#include <machine/atomic.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/prom.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/reg.h>

#ifdef MVME141
#include <mvme68k/dev/ofobioreg.h>
#endif
#ifdef MVME147
#include <mvme68k/dev/pccreg.h>
#endif
#ifdef MVME165
#include <mvme68k/dev/lrcreg.h>
#endif
 
#include <dev/cons.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>
#endif

#include <uvm/uvm.h>

/* the following is used externally (sysctl_hw) */
char machine[] = MACHINE;		/* cpu "architecture" */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

extern vaddr_t avail_end;

int   physmem;			/* size of physical memory, in pages */

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = PSL_LOWIPL;

void dumpsys(void);
void initvectors(void);
void mvme68k_init(void);
void identifycpu(void);
int cpu_sysctl(int *, u_int, void *, size_t *, void *, size_t, struct proc *);
void dumpconf(void);
void straytrap(int, u_short);
void netintr(void *);
void myetheraddr(u_char *);
int fpu_gettype(void);
int memsize162(void);
int memsize1x7(void);	/* in locore */
int memsize(void);

void
mvme68k_init()
{
	extern vaddr_t avail_start;

	/*
	 * Tell the VM system about available physical memory.  The
	 * mvme68k only has one segment.
	 */

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), 0);

	/* 
	 * Put machine specific exception vectors in place.
	 */
	initvectors();
}

/*
 * Console initialization: called early on from main,
 * before vm init or startup, but already running virtual.
 * Do enough configuration to choose and initialize a console.
 */
void
consinit()
{
	/*
	 * Initialize the console before we print anything out.
	 */
	cninit();

#ifdef DDB
	db_machine_init();
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
	unsigned i;
	vaddr_t minaddr, maxaddr;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < atop(MSGBUFSIZE); i++)
		pmap_kenter_pa((vaddr_t)msgbufp + i * PAGE_SIZE,
		    avail_end + i * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s", version);
	identifycpu();
	printf("real mem = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem) / 1024 / 1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	printf("avail mem = %u (%uMB)\n",
	    ptoa(uvmexp.free), ptoa(uvmexp.free) / 1024 / 1024);

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
char  cpu_model[120];

int   cputyp;
int   cpuspeed;

struct   mvmeprom_brdid brdid;

void
identifycpu()
{
	char mc;
	char speed[6];
	char suffix[30];
	int len;

	bzero(suffix, sizeof suffix);

	switch (mmutype) {
	case MMU_68060:
		mc = '6';
		break;
	case MMU_68040:
		mc = '4';
		break;
	case MMU_68030:
		mc = '3';
		break;
	default:
		mc = '2';
	}

	switch (cputyp) {
#ifdef MVME141
	case CPU_141:
		snprintf(suffix, sizeof suffix, "MVME%x", brdid.model);
#if 0
		cpuspeed = ofobiospeed((struct ofobioreg *)IIOV(0xfffb0000));
#else
		cpuspeed = 50;
#endif
		snprintf(speed, sizeof speed, "%02d", cpuspeed);
		break;
#endif
#ifdef MVME147
	case CPU_147:
		snprintf(suffix, sizeof suffix, "MVME%x", brdid.model);
		cpuspeed = pccspeed((struct pccreg *)IIOV(0xfffe1000));
		snprintf(speed, sizeof speed, "%02d", cpuspeed);
		break;
#endif
#if defined(MVME162) || defined(MVME167) || defined(MVME172) || defined(MVME177)
	case CPU_162:
	case CPU_166:
	case CPU_167:
	case CPU_172:
	case CPU_176:
	case CPU_177:
		bzero(speed, sizeof speed);
		speed[0] = brdid.speed[0];
		speed[1] = brdid.speed[1];
		if (brdid.speed[2] != '0' &&
			 brdid.speed[3] != '0') {
			speed[2] = '.';
			speed[3] = brdid.speed[2];
			speed[4] = brdid.speed[3];
		}
		cpuspeed = (speed[0] - '0') * 10 + (speed[1] - '0');
		bcopy(brdid.longname, suffix, sizeof(brdid.longname));
		for (len = strlen(suffix)-1; len; len--) {
			if (suffix[len] == ' ')
				suffix[len] = '\0';
			else
				break;
		}
		break;
#endif
#ifdef MVME165
	case CPU_165:
		snprintf(suffix, sizeof suffix, "MVME%x", brdid.model);
		cpuspeed = lrcspeed((struct lrcreg *)IIOV(0xfff90000));
		snprintf(speed, sizeof speed, "%02d", cpuspeed);
		break;
#endif
	}
	snprintf(cpu_model, sizeof cpu_model,
	    "Motorola %s: %sMHz MC680%c0 CPU", suffix, speed, mc);
	switch (mmutype) {
#if defined(M68040)
	case MMU_68040:
		/* FALLTHROUGH */
#endif
#if defined(M68060)
	case MMU_68060:
		/* FALLTHROUGH */
#endif
	case MMU_68030:
		strlcat(cpu_model, "+MMU", sizeof cpu_model);
		break;
	case MMU_68851:
		strlcat(cpu_model, ", MC68851 MMU", sizeof cpu_model);
		break;
	default:
		printf("%s\n", cpu_model);
		panic("unknown MMU type %d", mmutype);
	}

	switch (mmutype) {
#if defined(M68060)
	case MMU_68060:
		strlcat(cpu_model,"+FPU, 8k on-chip physical I/D caches",
		    sizeof cpu_model);
		break;
#endif
#if defined(M68040)
	case MMU_68040:
		strlcat(cpu_model, "+FPU, 4k on-chip physical I/D caches",
		    sizeof cpu_model);
		break;
#endif
#if defined(M68030) || defined(M68020)
	default:
		fputype = fpu_gettype();

		switch (fputype) {
		case FPU_NONE:
			break;
		case FPU_68881:
		case FPU_68882:
			len = strlen (cpu_model);
			snprintf(cpu_model + len, sizeof cpu_model - len,
			    ", MC6888%d FPU", fputype);
			break;
		default:
			strlcat(cpu_model, ", unknown FPU", sizeof cpu_model);
			break;
		}
		break;
#endif
	}
	printf("%s\n", cpu_model);
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
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int   waittime = -1;

__dead void
boot(howto)
	int howto;
{
	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(&curproc->p_addr->u_pcb);

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* do that another panic fly away */
		if (curproc == NULL)
			curproc = &proc0;
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

	uvm_shutdown();
	splhigh();			/* Disable interrupts. */

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("System halted. Press any key to reboot...\n\n");
		cnpollc(1);
		cngetc();
		cnpollc(0);
	}

	doboot();

	for (;;);
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long   dumpmag = 0x8fca0101;	/* magic number */
int   dumpsize = 0;		/* pages */
long  dumplo = 0;		/* blocks */
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

	/* mvme68k only uses a single segment. */
	cpu_kcore_hdr.ram_segs[0].start = 0;
	cpu_kcore_hdr.ram_segs[0].size = ptoa(physmem);
	cpu_kcore_hdr.mmutype = mmutype;
	cpu_kcore_hdr.kernel_pa = 0;
	cpu_kcore_hdr.sysseg_pa = pmap_kernel()->pm_stpa;

	/* Always skip the first block, in case there is a label there. */
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
	daddr64_t blkno;		/* current block to write */
					/* dump routine */
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	int pg;				/* page being dumped */
	paddr_t maddr;			/* PA being dumped */
	int error;			/* error code from (*dump)() */
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
	psize = (*bdevsw[maj].d_psize)(dumpdev);
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	/* Dump the header. */
	error = (*dump) (dumpdev, blkno++, (caddr_t)dump_hdr, dbtob(1));
	if (error != 0)
		goto abort;

	maddr = (paddr_t)0;
	for (pg = 0; pg < dumpsize; pg++) {
#define	NPGMB	(1024 * 1024 / PAGE_SIZE)
		/* print out how many MBs we have dumped */
		if (pg != 0 && (pg % NPGMB) == 0)
			printf("%d ", pg / NPGMB);
#undef	NPGMB
		pmap_kenter_pa((vaddr_t)vmmap, maddr, VM_PROT_READ);
		pmap_update(pmap_kernel());
		error = (*dump)(dumpdev, blkno, vmmap, PAGE_SIZE);
		pmap_kremove((vaddr_t)vmmap, PAGE_SIZE);
		pmap_update(pmap_kernel());

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

#if defined(M68060)
int m68060_pcr_init = 0x20 | PCR_SUPERSCALAR;	/* make this patchable */
#endif

void
initvectors()
{
#if defined(M68060)
	typedef void trapfun(void);
	extern trapfun *vectab[256];
#if defined(M060SP)
	extern trapfun intemu60, fpiemu60, fpdemu60, fpeaemu60;
	extern u_int8_t FP_CALL_TOP[];
#else
	extern trapfun illinst;
#endif
	extern trapfun fpfault;
#endif
#if defined(M68040) && defined(FPSP)
	extern u_long fpvect_tab, fpvect_end, fpsp_tab;
#endif

	switch (cputype) {
#ifdef M68060
	case CPU_68060:
		asm volatile ("movl %0,d0; .word 0x4e7b,0x0808" : : 
						  "d"(m68060_pcr_init):"d0" );

#if defined(M060SP)
		/* integer support */
		vectab[61] = intemu60/*(trapfun *)&I_CALL_TOP[128 + 0x00]*/;

		/* floating point support */
		/*
		 * XXX maybe we really should run-time check for the
		 * stack frame format here:
		 */
		vectab[11] = fpiemu60/*(trapfun *)&FP_CALL_TOP[128 + 0x30]*/;

		vectab[55] = fpdemu60/*(trapfun *)&FP_CALL_TOP[128 + 0x38]*/;
		vectab[60] = fpeaemu60/*(trapfun *)&FP_CALL_TOP[128 + 0x40]*/;

		vectab[54] = (trapfun *)&FP_CALL_TOP[128 + 0x00];
		vectab[52] = (trapfun *)&FP_CALL_TOP[128 + 0x08];
		vectab[53] = (trapfun *)&FP_CALL_TOP[128 + 0x10];
		vectab[51] = (trapfun *)&FP_CALL_TOP[128 + 0x18];
		vectab[50] = (trapfun *)&FP_CALL_TOP[128 + 0x20];
		vectab[49] = (trapfun *)&FP_CALL_TOP[128 + 0x28];
#else
		vectab[61] = illinst;
#endif
		vectab[48] = fpfault;
		break;
#endif
#if defined(M68040) && defined(FPSP)
	case CPU_68040:
		bcopy(&fpsp_tab, &fpvect_tab,
		    (&fpvect_end - &fpvect_tab) * sizeof (fpvect_tab));
		break;
#endif
	default:
		break;
	}
}

void
straytrap(pc, evec)
	int pc;
	u_short evec;
{
	printf("unexpected trap (vector 0x%x) from %x\n",
	    (evec & 0xFFF) >> 2, pc);
}

int   *nofault;

int
badpaddr(addr, size)
	paddr_t addr;
	int size;
{
	int off = (int)addr & PGOFSET;
	vaddr_t v;
	paddr_t p = trunc_page(addr);
	int x;

	v = mapiodev(p, NBPG);
	if (v == 0)
		return (1);
	x = badvaddr(v + off, size);
	unmapiodev(v, NBPG);
	return (x);
}

int
badvaddr(addr, size)
	vaddr_t addr;
	int size;
{
	int i;
	label_t  faultbuf;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *)0;
		return (1);
	}
	switch (size) {
		case 1:
			i = *(volatile char *)addr;
			break;
		case 2:
			i = *(volatile short *)addr;
			break;
		case 4:
			i = *(volatile long *)addr;
			break;
	}
	nofault = (int *)0;
	return (0);
}

/*
 * Level 7 interrupts are normally caused by the ABORT switch,
 * drop into ddb.
 */
void
nmihand(frame)
	void *frame;
{
	printf("Abort switch pressed\n");
#ifdef DDB
	if (db_console)
		Debugger();
#endif
}

u_char   myea[6] = { 0x08, 0x00, 0x3e, 0xff, 0xff, 0xff};

void
myetheraddr(ether)
	u_char *ether;
{
	bcopy(myea, ether, sizeof myea);
}

#if defined(M68030) || defined(M68020)
int
fpu_gettype()
{
	/*
	 * A 68881 idle frame is 28 bytes and a 68882's is 60 bytes.
	 * We, of course, need to have enough room for either.
	 */
	int   fpframe[60 / sizeof(int)];
	label_t  faultbuf;
	u_char   b;

	nofault = (int *) &faultbuf;
	if (setjmp((label_t *)nofault)) {
		nofault = (int *)0;
		return (0);		/* no FPU */
	}

	/*
	 * Synchronize FPU or cause a fault.
	 * This should leave the 881/882 in the IDLE state,
	 * state, so we can determine which we have by
	 * examining the size of the FP state frame
	 */
	asm("fnop");

	nofault = (int *)0;

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
	m68881_restore((struct fpframe *)fpframe);

	if (b == 0x18)
		return (FPU_68881);	/* The size of a 68881 IDLE frame is 0x18 */
	if (b == 0x38)
		return (FPU_68882);	/* 68882 frame is 0x38 bytes long */
	return (FPU_UNKNOWN);		/* unknown FPU type */
}
#endif


#if defined(MVME162) || defined(MVME172)
#include <mvme68k/dev/mcreg.h>
/*
 * XXX
 * used by locore.s to figure out how much memory is on the machine.
 * At this stage we only know that our machine is a 162. It is very
 * unfortunate that the MCchip's address must be encoded here.
 */
int
memsize162()
{
	struct mcreg *mc = (struct mcreg *)0xfff42000;

	switch (mc->mc_memoptions & MC_MEMOPTIONS_DRAMMASK) {
	case MC_MEMOPTIONS_DRAM1M:
		return (1*1024*1024);
	case MC_MEMOPTIONS_DRAM2M:
		return (2*1024*1024);
	case MC_MEMOPTIONS_DRAM4M:
		return (4*1024*1024);
	case MC_MEMOPTIONS_DRAM4M2:
		return (4*1024*1024);
	case MC_MEMOPTIONS_DRAM8M:
		return (8*1024*1024);
	case MC_MEMOPTIONS_DRAM16M:
		return (16*1024*1024);
	default:
		/*
		 * XXX if the machine has no MC-controlled memory,
		 * perhaps it has a MCECC or MEMC040 controller?
		 */
		return (memsize1x7());
	}
}
#endif

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl;

	__asm __volatile ("movew sr,%0" : "=&d" (oldipl));

	oldipl = PSLTOIPL(oldipl);

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		_spl(PSL_S | IPLTOPSL(wantipl));
	}
}
#endif
