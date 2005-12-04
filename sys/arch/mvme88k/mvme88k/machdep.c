/* $OpenBSD: machdep.c,v 1.173 2005/12/04 15:00:26 miod Exp $	*/
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

#include <net/netisr.h>

#include <machine/asm.h>
#include <machine/asm_macro.h>
#include <machine/bug.h>
#include <machine/bugio.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>
#include <machine/kcore.h>
#include <machine/locore.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include "ksyms.h"
#if DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#endif /* DDB */

typedef struct {
	unsigned word_one, word_two;
} m88k_exception_vector_area;

caddr_t	allocsys(caddr_t);
void	consinit(void);
void	dosoftint(void);
void	dumpconf(void);
void	dumpsys(void);
int	getcpuspeed(struct mvmeprom_brdid *);
vaddr_t	get_slave_stack(void);
void	identifycpu(void);
void	mvme_bootstrap(void);
void	savectx(struct pcb *);
void	setupiackvectors(void);
void	slave_pre_main(void);
int	slave_main(void);
void	vector_init(m88k_exception_vector_area *, unsigned *);
void	_doboot(void);

extern void setlevel(unsigned int);

extern void m187_bootstrap(void);
extern vaddr_t m187_memsize(void);
extern void m187_setupiackvectors(void);
extern void m187_startup(void);
extern void m188_bootstrap(void);
extern vaddr_t m188_memsize(void);
extern void m188_setupiackvectors(void);
extern void m188_startup(void);
extern void m197_bootstrap(void);
extern vaddr_t m197_memsize(void);
extern void m197_setupiackvectors(void);
extern void m197_startup(void);

intrhand_t intr_handlers[NVMEINTR];

/* board dependent pointers */
void (*md_interrupt_func_ptr)(u_int, struct trapframe *);
void (*md_init_clocks)(void);
u_int (*md_getipl)(void);
u_int (*md_setipl)(u_int);
u_int (*md_raiseipl)(u_int);

#ifdef MVME188
volatile u_int8_t *ivec[8 + 1];
#else
volatile u_int8_t *ivec[8];
#endif

int ssir;

int physmem;	  /* available physical memory, in pages */

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

/*
 * iomap stuff is for managing chunks of virtual address space that
 * can be allocated to IO devices.
 * VMEbus drivers use this at this now. Only on-board IO devices' addresses
 * are mapped so that pa == va. XXX smurph.
 */

vaddr_t iomapbase;

struct extent *iomap_extent;
struct vm_map *iomap_map;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifdef	NBUF
int nbuf = NBUF;
#else
int nbuf = 0;
#endif

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
extern char *esym;
#endif

int boothowto;					/* set in locore.S */
int bootdev;					/* set in locore.S */
int cputyp;					/* set in locore.S */
int brdtyp;					/* set in locore.S */
int cpumod;					/* set in mvme_bootstrap() */
int cpuspeed = 25;				/* safe guess */

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
#define bootcnpollc nullcnpollc

struct consdev bootcons = {
	NULL,
	NULL,
	bootcngetc,
	bootcnputc,
	bootcnpollc,
	NULL,
	makedev(14, 0),
	CN_NORMAL,
};

/*
 * Early console initialization: called early on from main, before vm init.
 * We want to stick to the BUG routines for now, and we'll switch to the
 * real console in cpu_startup().
 */
void
consinit()
{
	cn_tab = &bootcons;

#if defined(DDB)
	db_machine_init();
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
}

int
getcpuspeed(struct mvmeprom_brdid *brdid)
{
	int speed = 0;
	u_int i, c;

	for (i = 0; i < 4; i++) {
		c = (u_int)brdid->speed[i];
		if (c == ' ')
			c = '0';
		else if (c > '9' || c < '0') {
			speed = 0;
			break;
		}
		speed = speed * 10 + (c - '0');
	}
	speed = speed / 100;

	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		if (speed == 25 || speed == 33)
			return speed;
		speed = 25;
		break;
#endif
#ifdef MVME188
	case BRD_188:
		/*
		 * If BUG version prior to 5.x, there is no CNFG block and
		 * speed can be found in the environment.
		 * XXX We don't process ENV data yet - assume 20MHz in this
		 * case.
		 */
		if ((u_int)brdid->rev < 0x50) {
			speed = 20;
		} else {
			if (speed == 20 || speed == 25)
				return speed;
			speed = 25;
		}
		break;
#endif
#ifdef MVME197
	case BRD_197:
		if (speed == 40 || speed == 50)
			return speed;
		speed = 50;
		break;
#endif
	}

	/*
	 * If we end up here, the board information block is damaged and
	 * we can't trust it.
	 * Suppose we are running at the most common speed for our board,
	 * and hope for the best (this really only affects osiop).
	 */
	printf("WARNING: Board Configuration Data invalid, "
	    "replace NVRAM and restore values\n");
	return speed;
}

void
identifycpu()
{
	struct mvmeprom_brdid brdid;
	char suffix[4];
	u_int i;

	bzero(&brdid, sizeof(brdid));
	bugbrdid(&brdid);

	cpuspeed = getcpuspeed(&brdid);

	i = 0;
	if (brdid.suffix[0] >= ' ' && brdid.suffix[0] < 0x7f) {
		if (brdid.suffix[0] != '-')
			suffix[i++] = '-';
		suffix[i++] = brdid.suffix[0];
	}
	if (brdid.suffix[1] >= ' ' && brdid.suffix[1] < 0x7f)
		suffix[i++] = brdid.suffix[1];
	suffix[i++] = '\0';

	snprintf(cpu_model, sizeof cpu_model,
	    "Motorola MVME%x%s, %dMHz", brdtyp, suffix, cpuspeed);
}

/*
 * Set up real-time clocks.
 * These function pointers are set in dev/clock.c.
 */
void
cpu_initclocks()
{
	(*md_init_clocks)();
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
	vsize_t size;
	int base, residual;
	vaddr_t minaddr, maxaddr;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in mvme_bootstrap() to compensate.
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
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		m187_startup();
		break;
#endif
#ifdef MVME188
	case BRD_188:
		m188_startup();
		break;
#endif
#ifdef MVME197
	case BRD_197:
		m197_startup();
		break;
#endif
	}

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
	    NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE,
	      UVM_PROT_NONE, UVM_INH_NONE, UVM_ADV_NORMAL, 0)))
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
			    VM_PROT_READ | VM_PROT_WRITE);
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
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate map for physio.
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * Allocate map for external I/O.
	 */
	iomap_map = uvm_km_suballoc(kernel_map, &iomapbase, &maxaddr,
	    IOMAP_SIZE, 0, FALSE, NULL);

	iomap_extent = extent_create("iomap", iomapbase,
	    iomapbase + IOMAP_SIZE, M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (iomap_extent == NULL)
		panic("unable to allocate extent for iomap");

	printf("avail mem = %ld (%d pages)\n", ptoa(uvmexp.free), uvmexp.free);
	printf("using %d buffers containing %d bytes of memory\n", nbuf,
	    bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Set up interrupt handlers.
	 */
	for (i = 0; i < NVMEINTR; i++)
		SLIST_INIT(&intr_handlers[i]);
	setupiackvectors();

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

	/*
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0) {
		bufpages = (btoc(2 * 1024 * 1024) + physmem) *
		    bufcachepercent / 100;
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 70% filled kvm */
	if (nbuf >
	    (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);

	return v;
}

__dead void
_doboot()
{
	cmmu_shutdown();
	bugreturn();
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
		printf("halted\n\n");
	} else {
		doboot();
	}

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

	/* mvme88k only uses a single segment. */
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
	daddr_t blkno;		/* current block to write */
				/* dump routine */
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
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

/*
 * fill up ivec array with interrupt response vector addresses.
 */
void
setupiackvectors()
{
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		m187_setupiackvectors();
		break;
#endif
#ifdef MVME188
	case BRD_188:
		m188_setupiackvectors();
		break;
#endif
#ifdef MVME197
	case BRD_197:
		m197_setupiackvectors();
		break;
#endif
	}
}

/* gets an interrupt stack for slave processors */
vaddr_t
get_slave_stack()
{
	vaddr_t addr;

	addr = (vaddr_t)uvm_km_zalloc(kernel_map, INTSTACK_SIZE);

	if (addr == NULL)
		panic("Cannot allocate slave stack for cpu %d",
		    cpu_number());

	return addr;
}

/*
 * Slave CPU pre-main routine.
 * Determine CPU number and set it.
 *
 * Running on an interrupt stack here; do nothing fancy.
 */
void
slave_pre_main()
{
	set_cpu_number(cmmu_cpu_number()); /* Determine cpu number by CMMU */
	splhigh();
	set_psr(get_psr() & ~PSR_IND);
}

/* dummy main routine for slave processors */
int
slave_main()
{
	printf("slave CPU%d started\n", cpu_number());
	while (1); /* spin forever */
	return 0;
}

/*
 * Search for the first available interrupt vector in the range start, end.
 * This should really only be used by VME devices.
 */
int
intr_findvec(int start, int end, int skip)
{
	int vec;

#ifdef DEBUG
	if (start < 0 || end >= NVMEINTR || start > end)
		panic("intr_findvec(%d,%d): bad parameters", start, end);
#endif

	for (vec = start; vec <= end; vec++) {
		if (vec == skip)
			continue;
		if (SLIST_EMPTY(&intr_handlers[vec]))
			return vec;
	}
#ifdef DIAGNOSTIC
	printf("intr_findvec(%d,%d,%d): no vector available\n",
	    start, end, skip);
#endif
	return -1;
}

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
		panic("intr_establish: vec (0x%x) not between 0x00 and 0xff",
		      vec);
#endif /* DIAGNOSTIC */
		return (EINVAL);
	}

	list = &intr_handlers[vec];
	if (!SLIST_EMPTY(list)) {
		intr = SLIST_FIRST(list);
		if (intr->ih_ipl != ihand->ih_ipl) {
#ifdef DIAGNOSTIC
			panic("intr_establish: there are other handlers with "
			    "vec (0x%x) at ipl %x, but you want it at %x",
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

void
myetheraddr(cp)
	u_char *cp;
{
	struct mvmeprom_brdid brdid;

	bugbrdid(&brdid);
	bcopy(&brdid.etheraddr, cp, 6);
}

int netisr;

void
dosoftint()
{
	if (ssir & SIR_NET) {
		siroff(SIR_NET);
		uvmexp.softs++;
#define DONETISR(bit, fn) \
	do { \
		if (netisr & (1 << bit)) { \
			netisr &= ~(1 << bit); \
			fn(); \
		} \
	} while (0)
#include <net/netisr_dispatch.h>
#undef DONETISR
	}

	if (ssir & SIR_CLOCK) {
		siroff(SIR_CLOCK);
		uvmexp.softs++;
		softclock();
	}
}

int
spl0()
{
	int x;
	x = splsoftclock();

	if (ssir) {
		dosoftint();
	}

	setipl(0);

	return (x);
}

/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */
void
mvme_bootstrap()
{
	extern int kernelstart;
	extern struct consdev *cn_tab;
	struct mvmeprom_brdid brdid;
#ifndef MULTIPROCESSOR
	cpuid_t master_cpu;
#endif

	buginit();
	bugbrdid(&brdid);
	brdtyp = brdid.model;

	/*
	 * Set up interrupt and fp exception handlers based on the machine.
	 */
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		m187_bootstrap();
		break;
#endif
#ifdef MVME188
	case BRD_188:
		m188_bootstrap();
		break;
#endif
#ifdef MVME197
	case BRD_197:
		m197_bootstrap();
		break;
#endif
	default:
		panic("Sorry, this kernel does not support MVME%x", brdtyp);
	}

	/*
	 * Use the BUG as console for now. After autoconf, we'll switch to
	 * real hardware.
	 */
	cn_tab = &bootcons;

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();
	first_addr = round_page(first_addr);

	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		last_addr = m187_memsize();
		break;
#endif
#ifdef MVME188
	case BRD_188:
		last_addr = m188_memsize();
		break;
#endif
#ifdef MVME197
	case BRD_197:
		last_addr = m197_memsize();
		break;
#endif
	}
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

	/*
	 * If we have more than one CPU, mention which one is the master.
	 * We will also want to spin up slave CPUs on the long run...
	 */
	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		printf("CPU%d is master CPU\n", master_cpu);

#if 0
		int i;
		for (i = 0; i < MAX_CPUS; i++) {
			if (!spin_cpu(i))
				printf("CPU%d started\n", i);
		}
#endif
		break;
#endif
#ifdef MVME197
	case BRD_197:
		/*
		 * In the 197DP case, mention which CPU is the master
		 * there too...
		 * XXX TBD
		 */
		break;
#endif
#ifdef MVME187
	default:
		break;
#endif
	}

	avail_start = first_addr;
	avail_end = last_addr;

	/*
	 * Steal MSGBUFSIZE at the top of physical memory for msgbuf
	 */
	avail_end -= round_page(MSGBUFSIZE);

#ifdef DEBUG
	printf("MVME%x boot: memory from 0x%x to 0x%x\n",
	    brdtyp, avail_start, avail_end);
#endif
	pmap_bootstrap((vaddr_t)trunc_page((unsigned)&kernelstart));

	/*
	 * Tell the VM system about available physical memory.
	 *
	 * The mvme88k boards only have one contiguous area, although BUG
	 * could be set up to configure a non-contiguous scheme; also, we
	 * might want to register ECC memory separately later on...
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
	    atop(avail_start), atop(avail_end), VM_FREELIST_DEFAULT);

	/* Initialize the "u-area" pages. */
	bzero((caddr_t)curpcb, USPACE);
#ifdef DEBUG
	printf("leaving mvme_bootstrap()\n");
#endif
}

/*
 * Boot console routines:
 * Enables printing of boot messages before consinit().
 */
void
bootcnprobe(cp)
	struct consdev *cp;
{
	cp->cn_dev = makedev(14, 0);
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
	return (buginchr());
}

void
bootcnputc(dev, c)
	dev_t dev;
	int c;
{
	if ((char)c == '\n')
		bugoutchr('\r');
	bugoutchr((char)c);
}

#define SIGSYS_MAX	501
#define SIGTRAP_MAX	510

#define EMPTY_BR	0xc0000000	/* empty "br" instruction */
#define NO_OP 		0xf4005800	/* "or r0, r0, r0" */

#define BRANCH(FROM, TO) \
	(EMPTY_BR | ((unsigned)(TO) - (unsigned)(FROM)) >> 2)

#define SET_VECTOR(NUM, VALUE) \
	do { \
		vector[NUM].word_one = NO_OP; \
		vector[NUM].word_two = BRANCH(&vector[NUM].word_two, VALUE); \
	} while (0)

/*
 * vector_init(vector, vector_init_list)
 *
 * This routine sets up the m88k vector table for the running processor.
 * It is called with a very little stack, and interrupts disabled,
 * so don't call any other functions!
 */
void
vector_init(m88k_exception_vector_area *vector, unsigned *vector_init_list)
{
	unsigned num;
	unsigned vec;

	for (num = 0; (vec = vector_init_list[num]) != END_OF_VECTOR_LIST;
	    num++) {
		if (vec != UNKNOWN_HANDLER)
			SET_VECTOR(num, vec);
	}

	/* Save BUG vector */
	bugvec[0] = vector[MVMEPROM_VECTOR].word_one;
	bugvec[1] = vector[MVMEPROM_VECTOR].word_two;

#ifdef M88110
	if (CPU_IS88110) {
		for (; num <= SIGSYS_MAX; num++)
			SET_VECTOR(num, m88110_sigsys);

		for (; num <= SIGTRAP_MAX; num++)
			SET_VECTOR(num, m88110_sigtrap);

		SET_VECTOR(450, m88110_syscall_handler);
		SET_VECTOR(451, m88110_cache_flush_handler);
		SET_VECTOR(504, m88110_stepbpt);
		SET_VECTOR(511, m88110_userbpt);
	}
#endif
#ifdef M88100
	if (CPU_IS88100) {
		for (; num <= SIGSYS_MAX; num++)
			SET_VECTOR(num, sigsys);

		for (; num <= SIGTRAP_MAX; num++)
			SET_VECTOR(num, sigtrap);

		SET_VECTOR(450, syscall_handler);
		SET_VECTOR(451, cache_flush_handler);
		SET_VECTOR(504, stepbpt);
		SET_VECTOR(511, userbpt);
	}
#endif

	/* GCC will by default produce explicit trap 503 for division by zero */
	SET_VECTOR(503, vector_init_list[T_ZERODIV]);

	/* Save new BUG vector */
	sysbugvec[0] = vector[MVMEPROM_VECTOR].word_one;
	sysbugvec[1] = vector[MVMEPROM_VECTOR].word_two;
}

unsigned
getipl(void)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = (*md_getipl)();
	set_psr(psr);
	return curspl;
}

unsigned
setipl(unsigned level)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = (*md_setipl)(level);

	/*
	 * The flush pipeline is required to make sure the above change gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}

unsigned
raiseipl(unsigned level)
{
	u_int curspl, psr;

	disable_interrupt(psr);
	curspl = (*md_raiseipl)(level);

	/*
	 * The flush pipeline is required to make sure the above change gets
	 * through the data pipe and to the hardware; otherwise, the next
	 * bunch of instructions could execute at the wrong spl protection.
	 */
	flush_pipeline();

	set_psr(psr);
	return curspl;
}
