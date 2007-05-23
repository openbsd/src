/*	$OpenBSD: machdep.c,v 1.101 2007/05/23 20:33:46 pvalchev Exp $ */

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
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <sys/evcount.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/prom.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/reg.h>

#ifdef MVME147
#include <mvme68k/dev/pccreg.h>
#endif
 
#include <dev/cons.h>

#include <net/netisr.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_var.h>
#endif

#include <uvm/uvm_extern.h>

/* the following is used externally (sysctl_hw) */
char machine[] = MACHINE;		/* cpu "architecture" */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

extern vaddr_t avail_end;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif

#ifndef	BUFCACHEPERCENT
#define	BUFCACHEPERCENT	5
#endif

#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	bufcachepercent = BUFCACHEPERCENT;

int   physmem;			/* size of physical memory, in pages */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = PSL_LOWIPL;

#ifdef COMPAT_SUNOS
extern struct emul emul_sunos;
#endif

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
caddr_t allocsys(caddr_t);

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
	    atop(avail_start), atop(avail_end), VM_FREELIST_DEFAULT);

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
	caddr_t v;
	int base, residual;
	
	vaddr_t minaddr, maxaddr;
	vsize_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in pmap_bootstrap to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_kenter_pa((vaddr_t)msgbufp + i * PAGE_SIZE,
		    avail_end + i * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s", version);
	identifycpu();
	printf("real mem = %u (%uMB)\n", ctob(physmem),
	    ctob(physmem) / 1024 / 1024);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	size = (vsize_t)allocsys((caddr_t)0);
	if ((v = (caddr_t) uvm_km_zalloc(kernel_map, round_page(size))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != size)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
		    NULL, UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
				UVM_ADV_NORMAL, 0)))
		panic("cpu_startup: cannot allocate VM for buffers");
	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;

	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t)buffers + (i * MAXBSIZE);
		curbufsize = PAGE_SIZE * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				      "buffer cache");

			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
			    VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
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
	printf("using %d buffers containing %d bytes of memory\n",
			 nbuf, bufpages * PAGE_SIZE);

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
 * You call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(caddr_t v)
{

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name) + (num))
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem * bufcachepercent / 100;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless. */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);
	return (v);
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
#ifdef MVME147
	case CPU_147:
		snprintf(suffix, sizeof suffix, "MVME%x", brdid.model);
		cpuspeed = pccspeed((struct pccreg *)IIOV(0xfffe1000));
		snprintf(speed, sizeof speed, "%02d", cpuspeed);
		break;
#endif
#if defined(MVME162) || defined(MVME167) || defined(MVME172) || defined(MVME177)
	case CPU_162:
	case CPU_167:
	case CPU_172:
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

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("System halted. Press any key to reboot...\n\n");
		cngetc();
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
dumpconf()
{
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = physmem;

	/* mvme68k only uses a single segment. */
	cpu_kcore_hdr.ram_segs[0].start = 0;
	cpu_kcore_hdr.ram_segs[0].size = ctob(physmem);
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
	daddr_t blkno;			/* current block to write */
					/* dump routine */
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
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
	typedef void trapfun(void);
	extern trapfun *vectab[256];
#if defined(M68060)
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

int netisr;

void
netintr(arg)
	void *arg;
{
#define DONETISR(bit, fn) \
	do { \
		if (netisr & (1 << (bit))) { \
			netisr &= ~(1 << (bit)); \
			(fn)(); \
		} \
	} while (0)
#include <net/netisr_dispatch.h>
#undef DONETISR
}

/*
 * Level 7 interrupts are normally caused by the ABORT switch,
 * drop into ddb.
 */
void
nmihand(frame)
	void *frame;
{
#ifdef DDB
	printf("NMI ... going to debugger\n");
	Debugger();
#else
	/* panic?? */
	printf("unexpected level 7 interrupt ignored\n");
#endif
}

/*
 * cpu_exec_aout_makecmds():
 *	cpu-dependent a.out format hook for execve().
 * 
 * Determine of the given exec package refers to something which we
 * understand and, if so, set up the vmcmds for it.
 */
int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_SUNOS
	{
		extern int sunos_exec_aout_makecmds(struct proc *, struct exec_package *);
		if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
			return (0);
	}
#endif
	return (error);
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
