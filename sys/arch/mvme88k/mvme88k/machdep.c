/* $OpenBSD: machdep.c,v 1.96 2002/10/12 02:03:45 krw Exp $	*/
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
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/ioctl.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/extent.h>
#include <sys/core.h>
#include <sys/kcore.h>

#include <net/netisr.h>

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/mmu.h>
#include <machine/board.h>
#include <machine/bug.h>
#include <machine/bugio.h>
#include <machine/cmmu.h>		/* CMMU stuff	*/
#include <machine/cpu.h>
#include <machine/cpu_number.h>
#include <machine/kcore.h>
#include <machine/locore.h>
#include <machine/prom.h>
#include <machine/reg.h>
#include <machine/trap.h>
#ifdef M88100
#include <machine/m88100.h>		/* DMT_VALID	*/
#endif 

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <mvme88k/dev/sysconreg.h>
#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/busswreg.h>

#include "assym.h"			/* EF_EPSR, etc. */
#include "ksyms.h"
#if DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>		/* db_printf()		*/
#endif /* DDB */

#if DDB
#define DEBUG_MSG db_printf
#else
#define DEBUG_MSG printf
#endif /* DDB */

struct intrhand *intr_handlers[256];
vm_offset_t interrupt_stack[MAX_CPUS] = {0};

/* machine dependant function pointers. */
struct md_p md;

/* prototypes */
void m88100_Xfp_precise(void);
void m88110_Xfp_precise(void);
void setupiackvectors(void);
void regdump(struct trapframe *f);
void dumpsys(void);
void consinit(void);
vm_offset_t size_memory(void);
int getcpuspeed(void);
int getscsiid(void);
void identifycpu(void);
void save_u_area(struct proc *, vm_offset_t);
void load_u_area(struct proc *);
void dumpconf(void);
void m187_ext_int(u_int v, struct m88100_saved_state *eframe);
void m188_ext_int(u_int v, struct m88100_saved_state *eframe);
void m197_ext_int(u_int v, struct m88100_saved_state *eframe);

unsigned char *volatile ivec[] = {
	(unsigned char *)0xFFFE0003, /* not used, no such thing as int 0 */
	(unsigned char *)0xFFFE0007,
	(unsigned char *)0xFFFE000B,
	(unsigned char *)0xFFFE000F,
	(unsigned char *)0xFFFE0013,
	(unsigned char *)0xFFFE0017,
	(unsigned char *)0xFFFE001B,
	(unsigned char *)0xFFFE001F,
	(unsigned char *)0x00000000,
};

#ifdef MVME188
/*
 * *int_mask_reg[CPU]
 * Points to the hardware interrupt status register for each CPU.
 */
unsigned int *volatile int_mask_reg[MAX_CPUS] = {
	(unsigned int *)IEN0_REG,
	(unsigned int *)IEN1_REG,
	(unsigned int *)IEN2_REG,
	(unsigned int *)IEN3_REG
};
#endif 

volatile vm_offset_t bugromva;
volatile vm_offset_t sramva;
volatile vm_offset_t obiova;
#ifdef MVME188
volatile vm_offset_t utilva;
#endif

int ssir;
int want_ast;
int want_resched;

int physmem;	  /* available physical memory, in pages */
int longformat = 1;  /* for regdump() */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = PSR_SUPERVISOR;

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
int   nbuf = NBUF;
#else
int   nbuf = 0;
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef	BUFPAGES
int   bufpages = BUFPAGES;
#else
int   bufpages = 0;
#endif
int   bufcachepercent = BUFCACHEPERCENT;

caddr_t allocsys(caddr_t);

/*
 * Info for CTL_HW
 */
char  machine[] = MACHINE;	 /* cpu "architecture" */
char  cpu_model[120];

struct bugenv bugargs;

struct kernel {
	void *entry;
	void *symtab;
	void *esym;
	int   bflags;
	int   bdev;
	char *kname;
	void *smini;
	void *emini;
	void *end_load;
}kflags;
#if defined(DDB) || NKSYMS > 0
extern char *esym;
#endif

int boothowto;	/* set in locore.S */
int bootdev;	/* set in locore.S */
int cputyp;	/* set in locore.S */
int brdtyp;	/* set in locore.S */
int cpumod = 0; /* set in mvme_bootstrap() */
int cpuspeed = 25;   /* 25 MHZ XXX should be read from NVRAM */

vm_offset_t first_addr = 0;
vm_offset_t last_addr = 0;

vm_offset_t avail_start, avail_end;
vm_offset_t virtual_avail, virtual_end;

pcb_t    curpcb;
extern struct user *proc0paddr;

/* 
 *  XXX this is to fake out the console routines, while 
 *  booting. New and improved! :-) smurph
 */
void bootcnprobe(struct consdev *);
void bootcninit(struct consdev *);
void bootcnputc(dev_t, int);
int  bootcngetc(dev_t);
extern void nullcnpollc(dev_t, int);

#define bootcnpollc nullcnpollc

static struct consdev bootcons = {
	NULL, 
	NULL, 
	bootcngetc, 
	bootcnputc,
	bootcnpollc,
	NULL,
	makedev(14,0),
	1
};

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	extern struct consdev *cn_tab;

	/*
	 * Initialize the console before we print anything out.
	 */
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
 * Figure out how much real memory is available.
 * Start looking from the megabyte after the end of the kernel data,
 * until we find non-memory.
 */
vm_offset_t
size_memory()
{
	unsigned int *volatile look;
	unsigned int *max;
	extern char *end;
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
	max = (void *)MAXPHYSMEM;
	for (look = (void *)Roundup(end, STRIDE); look < max;
	    look = (int *)((unsigned)look + STRIDE)) {
		unsigned save;

		/* if can't access, we've reached the end */
		if (badwordaddr((vaddr_t)look)) {
#if defined(DEBUG)
			printf("%x\n", look);
#endif
			look = (int *)((int)look - STRIDE);
			break;
		}

		/*
		 * If we write a value, we expect to read the same value back.
		 * We'll do this twice, the 2nd time with the opposite bit
		 * pattern from the first, to make sure we check all bits.
		 */
		save = *look;
		if (*look = PATTERN, *look != PATTERN)
			break;
		if (*look = ~PATTERN, *look != ~PATTERN)
			break;
		*look = save;
	}
	if ((look > (unsigned int *)0x01FFF000) && (brdtyp == BRD_188)) {
                /* temp hack to fake 32Meg on MVME188 */
		look = (unsigned int *)0x01FFF000; 
	}
	
	physmem = btoc(trunc_page((unsigned)look)); /* in pages */
	return (trunc_page((unsigned)look));
}

int
getcpuspeed()
{
	struct mvmeprom_brdid brdid;
	int speed = 0;
	int i, c;
	bugbrdid(&brdid);
	for (i=0; i<4; i++) {
		c=(unsigned char)brdid.speed[i];
		c-= '0';
		speed *=10;
		speed +=c;
	}
	speed = speed / 100;
	return (speed);
}

int
getscsiid()
{
	struct mvmeprom_brdid brdid;
	int scsiid = 0;
	int i, c;
	bugbrdid(&brdid);
	for (i=0; i<2; i++) {
		c=(unsigned char)brdid.scsiid[i];
		scsiid *=10;
		c-= '0';
		scsiid +=c;
	}
	printf("SCSI ID = %d\n", scsiid);
	return (7); /* hack! */
}

void
identifycpu()
{
	cpuspeed = getcpuspeed();
	printf("\nModel: Motorola MVME%x %dMhz\n", brdtyp, cpuspeed);
}

/*
 *	Setup u area ptes for u area double mapping.
 */

void
save_u_area(struct proc *p, vm_offset_t va)
{
	int i; 
	for (i=0; i<UPAGES; i++) {
		p->p_md.md_upte[i] = *((pt_entry_t *)kvtopte((va + (i * NBPG))));
	}
}

void
load_u_area(struct proc *p)
{
	pt_entry_t *t;

	int i; 
	for (i=0; i<UPAGES; i++) {
		t = kvtopte((UADDR + (i * NBPG)));
		*t = p->p_md.md_upte[i];
	}
	for (i=0; i<UPAGES; i++) {
		cmmu_flush_tlb(1, (UADDR + (i * NBPG)), NBPG);
	}
}

/*
 * Set up real-time clocks.
 * These function pointers are set in dev/clock.c and dev/sclock.c
 */
void 
cpu_initclocks()
{
#ifdef DEBUG
	printf("cpu_initclocks(): ");
#endif 
	if (md.clock_init_func != NULL) {
#ifdef DEBUG
		printf("[interval clock] ");
#endif 
		(*md.clock_init_func)();
	}
	if (md.statclock_init_func != NULL) {
#ifdef DEBUG
		printf("[statistics clock]");
#endif 
		(*md.statclock_init_func)();
	}
#ifdef DEBUG
	printf("\n");
#endif 
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
	vm_size_t size;    
	int base, residual;
	vaddr_t minaddr, maxaddr, uarea_pages;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in mvme_bootstrap() to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_kenter_pa((vm_offset_t)msgbufp, 
			   avail_end + i * NBPG, VM_PROT_READ|VM_PROT_WRITE);
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
	 * Grab UADDR virtual address
	 */
	uarea_pages = UADDR;
	uvm_map(kernel_map, (vaddr_t *)&uarea_pages, USPACE,
		NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE, 
						     UVM_PROT_NONE,
						     UVM_INH_NONE,
						     UVM_ADV_NORMAL, 0));
	if (uarea_pages != UADDR) {
		printf("uarea_pages %x: UADDR not free\n", uarea_pages);
		panic("bad UADDR");
	}
	
	/* 
	 * Grab machine dependant memory spaces
	 */
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
		/*
		 * Grab the SRAM space that we hardwired in pmap_bootstrap
		 */
		sramva = SRAM_START;
		uvm_map(kernel_map, (vaddr_t *)&sramva, SRAM_SIZE,
			NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE, 
							     UVM_PROT_NONE,
							     UVM_INH_NONE,
							     UVM_ADV_NORMAL, 0));

		if (sramva != SRAM_START) {
			printf("sramva %x: SRAM not free\n", sramva);
			panic("bad sramva");
		}
#endif 
#ifdef MVME197
	case BRD_197:
#endif 
#if defined(MVME187) || defined(MVME197)
		/*
		 * Grab the BUGROM space that we hardwired in pmap_bootstrap
		 */
		bugromva = BUGROM_START;

		uvm_map(kernel_map, (vaddr_t *)&bugromva, BUGROM_SIZE,
			NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE, 
							     UVM_PROT_NONE,
							     UVM_INH_NONE,
							     UVM_ADV_NORMAL, 0));
		if (bugromva != BUGROM_START) {
			printf("bugromva %x: BUGROM not free\n", bugromva);
			panic("bad bugromva");
		}
		
		/*
		 * Grab the OBIO space that we hardwired in pmap_bootstrap
		 */
		obiova = OBIO_START;
		uvm_map(kernel_map, (vaddr_t *)&obiova, OBIO_SIZE,
			NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE, 
							     UVM_PROT_NONE,
							     UVM_INH_NONE,
							     UVM_ADV_NORMAL, 0));
		if (obiova != OBIO_START) {
			printf("obiova %x: OBIO not free\n", obiova);
			panic("bad OBIO");
		}
		break;
#endif 
#ifdef MVME188
	case BRD_188:
		/*
		 * Grab the UTIL space that we hardwired in pmap_bootstrap
		 */
		utilva = MVME188_UTILITY;
		uvm_map(kernel_map, (vaddr_t *)&utilva, MVME188_UTILITY_SIZE,
			NULL, UVM_UNKNOWN_OFFSET, 0, UVM_MAPFLAG(UVM_PROT_NONE, 
							     UVM_PROT_NONE,
							     UVM_INH_NONE,
							     UVM_ADV_NORMAL, 0));
		if (utilva != MVME188_UTILITY) {
			printf("utilva %x: UTILITY area not free\n", utilva);
			panic("bad utilva");
		}
		break;
#endif
	}

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
		curbuf = (vm_offset_t) buffers + (i * MAXBSIZE);
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

	printf("avail mem = %ld (%ld pages)\n", ptoa(uvmexp.free), uvmexp.free);
	printf("using %d buffers containing %d bytes of memory\n", nbuf,
	    bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * zero out intr_handlers
	 */
	bzero((void *)intr_handlers, 256 * sizeof(struct intrhand *));
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
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef SYSVSHM
	shminfo.shmmax = shmmaxpgs;
	shminfo.shmall = shmmaxpgs;
	shminfo.shmseg = shmseg;
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
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0) {
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / 10;
		else
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
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10) 
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);

	return v;
}

/*
 * Set registers on exec.
 * Clear all except sp and pc.
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	int retval[2];
{
	register struct trapframe *tf = USER_REGS(p);

	/*
	 * The syscall will ``return'' to snip; set it.
	 * argc, argv, envp are placed on the stack by copyregs.
	 * Point r2 to the stack. crt0 should extract envp from
	 * argc & argv before calling user's main.
	 */
#if 0
	/*
	 * I don't think I need to mess with fpstate on 88k because
	 * we make sure the floating point pipeline is drained in
	 * the trap handlers. Should check on this later. XXX Nivas.
	 */

	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
		p->p_md.md_fpstate = NULL;
	}
#endif /* 0 */
	bzero((caddr_t)tf, sizeof *tf);
	
	if (cputyp == CPU_88110) {
		/* 
		 * user mode, serialize mem, interrupts enabled, 
		 * graphics unit, fp enabled 
		 */
		tf->epsr = PSR_SRM | PSR_SFD;  
	} else {
		/* 
		 * user mode, interrupts enabled, 
		 * no graphics unit, fp enabled 
		 */
		tf->epsr = PSR_SFD | PSR_SFD2;
	}

	/*
	 * We want to start executing at pack->ep_entry. The way to
	 * do this is force the processor to fetch from ep_entry. Set
	 * NIP to something bogus and invalid so that it will be a NOOP.
	 * And set sfip to ep_entry with valid bit on so that it will be
	 * fetched.  mc88110 - just set exip to pack->ep_entry.
	 */
	if (cputyp == CPU_88110) {
		tf->exip = pack->ep_entry & ~3;
		printf("exec @ 0x%x\n", tf->exip);
	} else {
	tf->snip = pack->ep_entry & ~3;
	tf->sfip = (pack->ep_entry & ~3) | FIP_V;
	}
	tf->r[2] = stack;
	tf->r[31] = stack;
	retval[1] = 0;
}

struct sigstate {
	int   ss_flags;	     /* which of the following are valid */
	struct   trapframe ss_frame;  /* original exception frame */
};

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signo
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int   sf_signo;	     /* signo for handler */
	siginfo_t *sf_sip;
	struct   sigcontext *sf_scp;  /* context ptr for handler */
	sig_t sf_handler;    /* handler addr for u_sigc */
	struct   sigcontext sf_sc; /* actual context */
	siginfo_t sf_si;
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
   #define SDB_FOLLOW	0x01
   #define SDB_KSTACK	0x02
   #define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	unsigned long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	register struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	int oonstack, fsize;
	struct sigframe sf;
	int addr;

	tf = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) &&
	    (psp->ps_sigstk.ss_flags & SA_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else
		fp = (struct sigframe *)(tf->r[31] - fsize);
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)uvm_grow(p, (unsigned)fp);

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && (p->p_pid == sigpid)))
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc);
#endif
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_signo = sig;
	sf.sf_scp = &fp->sf_sc;
	sf.sf_handler = catcher;
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;

	if (psp->ps_siginfo & sigmask(sig)) {
		sf.sf_sip = &fp->sf_si;
		initsiginfo(&sf.sf_si, sig, code, type, val);
	}

	/*
	 * Copy the whole user context into signal context that we
	 * are building.
	 */
	bcopy((caddr_t)tf->r, (caddr_t)sf.sf_sc.sc_regs,
	      sizeof(sf.sf_sc.sc_regs));
	if (cputyp != CPU_88110) {
		/* mc88100 */
	sf.sf_sc.sc_xip = tf->sxip & ~3;
	sf.sf_sc.sc_nip = tf->snip & ~3;
	sf.sf_sc.sc_fip = tf->sfip & ~3;
	} else {
		/* mc88110 */
		sf.sf_sc.sc_xip = tf->exip & ~3;
		sf.sf_sc.sc_nip = tf->enip & ~3;
		sf.sf_sc.sc_fip = 0;
	}
	sf.sf_sc.sc_ps = tf->epsr;
	sf.sf_sc.sc_sp  = tf->r[31];
	sf.sf_sc.sc_fpsr = tf->fpsr;
	sf.sf_sc.sc_fpcr = tf->fpcr;
	if (cputyp != CPU_88110) {
		/* mc88100 */
	sf.sf_sc.sc_ssbr = tf->ssbr;
	sf.sf_sc.sc_dmt0 = tf->dmt0;
	sf.sf_sc.sc_dmd0 = tf->dmd0;
	sf.sf_sc.sc_dma0 = tf->dma0;
	sf.sf_sc.sc_dmt1 = tf->dmt1;
	sf.sf_sc.sc_dmd1 = tf->dmd1;
	sf.sf_sc.sc_dma1 = tf->dma1;
	sf.sf_sc.sc_dmt2 = tf->dmt2;
	sf.sf_sc.sc_dmd2 = tf->dmd2;
	sf.sf_sc.sc_dma2 = tf->dma2;
	} else {
		/* mc88110 */
		sf.sf_sc.sc_dsr  = tf->dsr;
		sf.sf_sc.sc_dlar = tf->dlar;
		sf.sf_sc.sc_dpar = tf->dpar;
		sf.sf_sc.sc_isr  = tf->isr;
		sf.sf_sc.sc_ilar = tf->ilar;
		sf.sf_sc.sc_ipar = tf->ipar;
		sf.sf_sc.sc_isap = tf->isap;
		sf.sf_sc.sc_dsap = tf->dsap;
		sf.sf_sc.sc_iuap = tf->iuap;
		sf.sf_sc.sc_duap = tf->duap;
	}
	sf.sf_sc.sc_fpecr = tf->fpecr;
	sf.sf_sc.sc_fphs1 = tf->fphs1;
	sf.sf_sc.sc_fpls1 = tf->fpls1;
	sf.sf_sc.sc_fphs2 = tf->fphs2;
	sf.sf_sc.sc_fpls2 = tf->fpls2;
	sf.sf_sc.sc_fppt = tf->fppt;
	sf.sf_sc.sc_fprh = tf->fprh;
	sf.sf_sc.sc_fprl = tf->fprl;
	sf.sf_sc.sc_fpit = tf->fpit;
	if (copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}
	/* 
	 * Build the argument list for the signal handler.
	 * Signal trampoline code is at base of user stack.
	 */
	addr = p->p_sigcode;
	if (cputyp != CPU_88110) {
		/* mc88100 */
	tf->snip = (addr & ~3) | NIP_V;
	tf->sfip = (tf->snip + 4) | FIP_V;
	} else {
		/* mc88110 */
		tf->exip = (addr & ~3);
		tf->enip = (tf->exip + 4);
	}
	tf->r[31] = (unsigned)fp;
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sendsig(%d): sig %d returns\n", p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */

/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
struct proc *p;
void *v;
register_t *retval;
{
	struct sys_sigreturn_args /* {
	   syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct trapframe *tf;
	struct sigcontext ksc;

	scp = (struct sigcontext *)SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int)scp & 3 || uvm_useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(struct sigcontext)))
		return (EINVAL);

	tf = p->p_md.md_tf;
	scp = &ksc;
	/*
	 * xip, nip and fip must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
#if 0
	if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0)
		return (EINVAL);
#endif /* 0 */
#if DIAGNOSTIC
	if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0){
		printf("xip %x nip %x fip %x\n",
		       scp->sc_xip, scp->sc_nip, scp->sc_fip);
		return (EINVAL);
	}
#endif 
	/*
	 * this can be improved by doing
	 *	 bcopy(sc_reg to tf, sizeof sigcontext - 2 words)
	 * XXX nivas
	 */
	bcopy((caddr_t)scp->sc_regs, (caddr_t)tf->r, sizeof(scp->sc_regs));
	if (cputyp != CPU_88110) {
		/* mc88100 */
	tf->sxip = (scp->sc_xip) | XIP_V;
	tf->snip = (scp->sc_nip) | NIP_V;
	tf->sfip = (scp->sc_fip) | FIP_V;
	} else {
		/* mc88110 */
		tf->exip = (scp->sc_xip);
		tf->enip = (scp->sc_nip);
		tf->sfip = 0;
	}
	tf->epsr = scp->sc_ps;
	tf->r[31] = scp->sc_sp;
	tf->fpsr = scp->sc_fpsr;
	tf->fpcr = scp->sc_fpcr;
	if (cputyp != CPU_88110) {
		/* mc88100 */
	tf->ssbr = scp->sc_ssbr;
	tf->dmt0 = scp->sc_dmt0;
	tf->dmd0 = scp->sc_dmd0;
	tf->dma0 = scp->sc_dma0;
	tf->dmt1 = scp->sc_dmt1;
	tf->dmd1 = scp->sc_dmd1;
	tf->dma1 = scp->sc_dma1;
	tf->dmt2 = scp->sc_dmt2;
	tf->dmd2 = scp->sc_dmd2;
	tf->dma2 = scp->sc_dma2;
	} else {
		/* mc88110 */
		tf->dsr  = scp->sc_dsr;
		tf->dlar = scp->sc_dlar;
		tf->dpar = scp->sc_dpar;
		tf->isr  = scp->sc_isr;
		tf->ilar = scp->sc_ilar;
		tf->ipar = scp->sc_ipar;
		tf->isap = scp->sc_isap;
		tf->dsap = scp->sc_dsap;
		tf->iuap = scp->sc_iuap;
		tf->duap = scp->sc_duap;
	}
	tf->fpecr = scp->sc_fpecr;
	tf->fphs1 = scp->sc_fphs1;
	tf->fpls1 = scp->sc_fpls1;
	tf->fphs2 = scp->sc_fphs2;
	tf->fpls2 = scp->sc_fpls2;
	tf->fppt = scp->sc_fppt;
	tf->fprh = scp->sc_fprh;
	tf->fprl = scp->sc_fprl;
	tf->fpit = scp->sc_fpit;

	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	return (EJUSTRETURN);
}

__dead void
_doboot()
{
	cmmu_shutdown_now();
	bugreturn();
	/*NOTREACHED*/
	for (;;);		/* appease gcc */
}

__dead void
boot(howto)
	register int howto;
{
	/* take a snap shot before clobbering any registers */
	if (curproc && curproc->p_addr)
		savectx(curpcb);

	/* If system is cold, just halt. */
	if (cold) {
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

	for (;;);  /* to keep compiler happy, and me from going crazy */
	/*NOTREACHED*/
}

#ifdef MVME188
void 
m188_reset()
{
	volatile int cnt;

	*sys_syscon->ien0 = 0;
	*sys_syscon->ien1 = 0;
	*sys_syscon->ien2 = 0;
	*sys_syscon->ien3 = 0;
	*sys_syscon->glbres = 1;  /* system reset */
	*sys_syscon->ucsr |= 0x2000; /* clear SYSFAIL* */
	for (cnt = 0; cnt < 5*1024*1024; cnt++)
		;
	*sys_syscon->ucsr |= 0x2000; /* clear SYSFAIL* */
}
#endif   /* MVME188 */

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
	register u_char *vaddr;
#undef MAP_VEC /* Switching to new virtual addresses XXX smurph */
#ifdef MAP_VEC
	extern vm_offset_t iomap_mapin(vm_offset_t, vm_size_t,  boolean_t);
#endif
	/*
	 * map a page in for phys address 0xfffe0000 (M187) and set the
	 * addresses for various levels.
	 */
	switch (brdtyp) {
#ifdef MVME187
	case BRD_187:
#ifdef MAP_VEC /* do for MVME188 too */
		vaddr = (u_char *)iomap_mapin(M187_IACK, NBPG, 1);
#else
		vaddr = (u_char *)M187_IACK;
#endif
		break;
#endif /* MVME187 */
#ifdef MVME188
	case BRD_188:
#ifdef MAP_VEC /* do for MVME188 too */
		vaddr = (u_char *)iomap_mapin(M188_IACK, NBPG, 1);
#else
		vaddr = (u_char *)M188_IACK;
#endif
		ivec[0] = vaddr;	/* We dont use level 0 */
		ivec[1] = vaddr + 0x04;
		ivec[2] = vaddr + 0x08;
		ivec[3] = vaddr + 0x0c;
		ivec[4] = vaddr + 0x10;
		ivec[5] = vaddr + 0x14;
		ivec[6] = vaddr + 0x18;
		ivec[7] = vaddr + 0x1c;
		ivec[8] = vaddr + 0x20;	/* for self inflicted interrupts */
		*ivec[8] = M188_IVEC;	/* supply a vector base for m188ih */
		break;
#endif /* MVME188 */
#ifdef MVME197
	case BRD_197:
#ifdef MAP_VEC /* do for MVME188 too */
		vaddr = (u_char *)iomap_mapin(M197_IACK, NBPG, 1);
#else
		vaddr = (u_char *)M197_IACK;
#endif
		break;
#endif /* MVME197 */
	}
#ifdef DEBUG
	printf("interrupt ACK address mapped at 0x%x\n", vaddr);
#endif 

#if defined(MVME187) || defined(MVME197)
	if (brdtyp != BRD_188) {
		ivec[0] = vaddr + 0x03;	/* We dont use level 0 */
		ivec[1] = vaddr + 0x07;
		ivec[2] = vaddr + 0x0b;
		ivec[3] = vaddr + 0x0f;
		ivec[4] = vaddr + 0x13;
		ivec[5] = vaddr + 0x17;
		ivec[6] = vaddr + 0x1b;
		ivec[7] = vaddr + 0x1f;
	}
#endif
}

/* gets an interrupt stack for slave processors */
vm_offset_t 
get_slave_stack()
{
	vm_offset_t addr = 0;
	addr = (vm_offset_t)uvm_km_zalloc(kernel_map, INTSTACK_SIZE + 4096);

	if (addr == NULL)
		panic("Cannot allocate slave stack");

	if (interrupt_stack[0] == 0)
		interrupt_stack[0] = (vm_offset_t) intstack;
	interrupt_stack[cpu_number()] = addr;
	return addr;
}

/*
 * Slave CPU pre-main routine.
 * Determine CPU number and set it.
 *
 * Running on an interrupt stack here; do nothing fancy.
 *
 * Called from "mvme88k/locore.S"
 */
void slave_pre_main()
{
   set_cpu_number(cmmu_cpu_number()); /* Determine cpu number by CMMU */
   splhigh();
   enable_interrupt();
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
 * Search for the first avilable interrupt vector in the range start, end.
 * This should really only be used by VME devices.
 */
int
intr_findvec(start, end)
	int start, end;
{
	int vec;

#ifdef DIAGNOSTIC
	/* Sanity check! */
	if (start < 0 || end > 255 || start > end)
		panic("intr_findvec(): bad parameters");
#endif

	for (vec = start; vec < end; vec++){
		if (intr_handlers[vec] == NULL)
			return (vec);
	}
#ifdef DIAGNOSTIC
	printf("intr_findvec(): uh oh....\n", vec);
#endif
	return (-1);
}

/*
 * Insert ihand in the list of handlers at vector vec.
 * Return return different error codes for the different
 * errors and let the caller decide what to do.
 */
int
intr_establish(int vec, struct intrhand *ihand)
{
	register struct intrhand *intr;

	if (vec < 0 || vec > 255) {
#if DIAGNOSTIC
		panic("intr_establish: vec (0x%x) not between 0x00 and 0xff",
		      vec);
#endif /* DIAGNOSTIC */
		return (INTR_EST_BADVEC);
	}

	if ((intr = intr_handlers[vec]) != NULL) {
		if (intr->ih_ipl != ihand->ih_ipl) {
#if DIAGNOSTIC
			panic("intr_establish: there are other handlers with vec (0x%x) at ipl %x, but you want it at %x",
			      intr->ih_ipl, vec, ihand->ih_ipl);
#endif /* DIAGNOSTIC */
			return (INTR_EST_BADIPL);
		}

		/*
		 * Go to the end of the chain
		 */
		while (intr->ih_next)
			intr = intr->ih_next;
	}

	ihand->ih_next = 0;

	if (intr)
		intr->ih_next = ihand;
	else
		intr_handlers[vec] = ihand;

	return (INTR_EST_SUCC);
}

#ifdef MVME188

/*
 *	Device interrupt handler for MVME188
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

/* Hard coded vector table for onboard devices. */

unsigned obio_vec[32] = {
	0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,
        0,SYSCV_SCC,0,0,SYSCV_SYSF,SYSCV_TIMER2,0,0,
	0,0,0,0,SYSCV_TIMER1,0,SYSCV_ACF,SYSCV_ABRT, 
};

#define GET_MASK(cpu, val)	*int_mask_reg[cpu] & (val)
#define VME_VECTOR_MASK		0x1ff 		/* mask into VIACK register */
#define VME_BERR_MASK		0x100 		/* timeout during VME IACK cycle */

void 
m188_ext_int(u_int v, struct m88100_saved_state *eframe)
{
	register int cpu = cpu_number();
	register unsigned int cur_mask;
	register unsigned int level, old_spl;
	register struct intrhand *intr;
	int ret, intbit;
	unsigned vec;

	cur_mask = ISR_GET_CURRENT_MASK(cpu);
	old_spl = m188_curspl[cpu];
	eframe->mask = old_spl;

	if (cur_mask == 0) {
		/*
		 * Spurious interrupts - may be caused by debug output clearing
		 * DUART interrupts.
		 */
		flush_pipeline();
		return;
	}

	uvmexp.intrs++;

	/* 
	 * We want to service all interrupts marked in the IST register
	 * They are all valid because the mask would have prevented them
	 * from being generated otherwise.  We will service them in order of
	 * priority. 
	 */
	do {
		level = safe_level(cur_mask, old_spl);

		if (old_spl >= level) {
			register int i;

			printf("safe level %d <= old level %d\n", level, old_spl);
			printf("cur_mask = 0x%b\n", cur_mask, IST_STRING);
			for (i = 0; i < 4; i++)
				printf("IEN%d = 0x%b  ", i, *int_mask_reg[i], IST_STRING);
			printf("\nCPU0 spl %d  CPU1 spl %d  CPU2 spl %d  CPU3 spl %d\n",
			       m188_curspl[0], m188_curspl[1],
			       m188_curspl[2], m188_curspl[3]);
			for (i = 0; i < 8; i++)
				printf("int_mask[%d] = 0x%08x\n", i, int_mask_val[i]);
			printf("--CPU %d halted--\n", cpu_number());
			spl7();
			for(;;) ;
		}

		if (level > 7 || (char)level < 0) {
			panic("int level (%x) is not between 0 and 7", level);
		}

		setipl(level);
	  
		enable_interrupt();
		
		/* generate IACK and get the vector */

		/* 
		 * This is tricky.  If you don't catch all the 
		 * interrupts, you die. Game over. Insert coin... 
		 * XXX smurph
		 */

		/* find the first bit set in the current mask */
		intbit = ff1(cur_mask);
		if (OBIO_INTERRUPT_MASK & (1 << intbit)) {
			if (guarded_access(ivec[level], 4, (u_char *)&vec) ==
			    EFAULT) {
				panic("unable to get vector for this vmebus "
				    "interrupt (level %x)", level);
			}
			vec = obio_vec[intbit];
			if (vec == 0) {
				panic("unknown onboard interrupt: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else if (HW_FAILURE_MASK & (1 << intbit)) {
			vec = obio_vec[intbit];
			if (vec == 0) {
				panic("unknown hardware failure: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else if (VME_INTERRUPT_MASK & (1 << intbit)) {
			if (guarded_access(ivec[level], 4, (u_char *)&vec) ==
			    EFAULT) {
				panic("unable to get vector for this vmebus "
				    "interrupt (level %x)", level);
			}
			vec &= VME_VECTOR_MASK;
			if (vec & VME_BERR_MASK) {
				panic("vme vec timeout");
			}
			if (vec == 0) {
				panic("unknown vme interrupt: mask = 0x%b",
				    1 << intbit, IST_STRING);
			}
		} else {
			panic("unknown interrupt: level = %d intbit = 0x%x "
			    "mask = 0x%b",
			    level, intbit, 1 << intbit, IST_STRING);
		}
		if (vec > 0xFF) {
			panic("interrupt vector 0x%x greater than 255!"
			    "level = %d iack = 0x%x", 
			    vec, level, ivec[level]);
		}

		if ((intr = intr_handlers[vec]) == NULL) {
			/* increment intr counter */
			intrcnt[M88K_SPUR_IRQ]++; 
			printf("Spurious interrupt: level = %d vec = 0x%x, "
			    "intbit = %d mask = 0x%b\n",
			    level, vec, intbit, 1 << intbit, IST_STRING);
		} else {
			/*
			 * Walk through all interrupt handlers in the chain
			 * for the given vector, calling each handler in turn,
			 * till some handler returns a value != 0.
			 */
			for (ret = 0; intr; intr = intr->ih_next) {
				if (intr->ih_wantframe != 0)
					ret = (*intr->ih_fn)((void *)eframe);
				else
					ret = (*intr->ih_fn)(intr->ih_arg);
				if (ret != 0) {
					/* increment intr counter */
					intrcnt[level]++; 
					break;
				}
			}
			if (ret == 0) {
				printf("Unclaimed interrupt: level = %d "
				    "vec = 0x%x, intbit = %d mask = 0x%b\n",
				    level, vec, intbit,
				    1 << intbit, IST_STRING);
				break;
			}
		}
	} while ((cur_mask = ISR_GET_CURRENT_MASK(cpu)) != 0);

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	disable_interrupt();
	if (eframe->dmt0 & DMT_VALID) {
		m88100_trap(T_DATAFLT, eframe);
		data_access_emulation((unsigned *)eframe);
		eframe->dmt0 &= ~DMT_VALID;
	}

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl((u_char)eframe->mask);
	flush_pipeline();
}

#endif /* MVME188 */

/*
 *	Device interrupt handler for MVME1x7
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

#ifdef MVME187
void
m187_ext_int(u_int v, struct m88100_saved_state *eframe)
{
	register u_char mask, level;
	register struct intrhand *intr;
	int ret;
	u_char vec;

	/* get level and mask */
	mask = *md.intr_mask;
	level = *md.intr_ipl;

	/*
	 * It is really bizarre for the mask and level to the be the same.
	 * pcc2 for 187 blocks all interrupts at and below the mask value,
	 * so we should not be getting an interrupt at the level that is
	 * already blocked. I can't explain this case XXX nivas
	 */

	if ((mask == level) && level) {
		panic("mask == level, %d", level);
	}

	/*
	 * Interrupting level cannot be 0--0 doesn't produce an interrupt.
	 * Weird! XXX nivas
	 */

	if (level == 0) {
		panic("Bogons... level %x and mask %x", level, mask);
	}

	/* and block interrupts at level or lower */
	setipl((u_char)level);
	/* and stash it away in the trap frame */
	eframe->mask = mask;

	uvmexp.intrs++;

	if (level > 7 || (char)level < 0) {
		panic("int level (%x) is not between 0 and 7", level);
	}

	/* generate IACK and get the vector */
	flush_pipeline();
	if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
		panic("Unable to get vector for this interrupt (level %x)", level);
	}
	flush_pipeline();
	flush_pipeline();
	flush_pipeline();

	if (vec > 0xFF) {
		panic("interrupt vector %x greater than 255", vec);
	}

	enable_interrupt();

	if ((intr = intr_handlers[vec]) == NULL) {
		/* increment intr counter */
		intrcnt[M88K_SPUR_IRQ]++; 
		printf("Spurious interrupt (level %x and vec %x)\n",
		       level, vec);
	} else {
		if (intr && intr->ih_ipl != level) {
			panic("Handler ipl %x not the same as level %x. "
			    "vec = 0x%x",
			    intr->ih_ipl, level, vec);
		}

		/*
		 * Walk through all interrupt handlers in the chain for the
		 * given vector, calling each handler in turn, till some handler
		 * returns a value != 0.
		 */

		for (ret = 0; intr; intr = intr->ih_next) {
			if (intr->ih_wantframe != 0)
				ret = (*intr->ih_fn)((void *)eframe);
			else
				ret = (*intr->ih_fn)(intr->ih_arg);
			if (ret != 0) {
				/* increment intr counter */
				intrcnt[level]++; 
				break;
			}
		}

		if (ret == 0) {
			printf("Unclaimed interrupt (level %x and vec %x)\n",
			    level, vec);
		}
	}

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	disable_interrupt();

	if (eframe->dmt0 & DMT_VALID) {
		m88100_trap(T_DATAFLT, eframe);
		data_access_emulation((unsigned *)eframe);
		eframe->dmt0 &= ~DMT_VALID;
	}
	mask = eframe->mask;

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl((u_char)mask);
}
#endif /* MVME187 */

#ifdef MVME197
void
m197_ext_int(u_int v, struct m88100_saved_state *eframe)
{
	register u_char mask, level, src;
	register struct intrhand *intr;
	int ret;
	u_char vec;

	/* get src and mask */
	mask = *md.intr_mask;
	src = *md.intr_src;
	
	if (v == T_NON_MASK) {
		/* This is the abort switch */
		level = IPL_NMI;
		vec = BS_ABORTVEC;
	} else {
		/* get level  */
		level = *md.intr_ipl;
	}

	/*
	 * Interrupting level cannot be 0--0 doesn't produce an interrupt.
	 * Weird! XXX nivas
	 */

	if (level == 0) {
		panic("Bogons... level %x and mask %x", level, mask);
	}

	/* and block interrupts at level or lower */
	setipl((u_char)level);
	/* and stash it away in the trap frame */
	eframe->mask = mask;

	uvmexp.intrs++;

	if (level > 7 || (char)level < 0) {
		panic("int level (%x) is not between 0 and 7", level);
	}

	if (v != T_NON_MASK) {
		/* generate IACK and get the vector */
		flush_pipeline();
		if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
			panic("Unable to get vector for this interrupt (level %x)", level);
		}
		flush_pipeline();
		flush_pipeline();
		flush_pipeline();
	}

	if (vec > 0xFF) {
		panic("interrupt vector %x greater than 255", vec);
	}

	enable_interrupt();

	if ((intr = intr_handlers[vec]) == NULL) {
		/* increment intr counter */
		intrcnt[M88K_SPUR_IRQ]++; 
		printf("Spurious interrupt (level %x and vec %x)\n",
		       level, vec);
	} else {
		if (intr && intr->ih_ipl != level) {
			panic("Handler ipl %x not the same as level %x. "
			    "vec = 0x%x",
			    intr->ih_ipl, level, vec);
		}

		/*
		 * Walk through all interrupt handlers in the chain for the
		 * given vector, calling each handler in turn, till some handler
		 * returns a value != 0.
		 */
		for (ret = 0; intr; intr = intr->ih_next) {
			if (intr->ih_wantframe != 0)
				ret = (*intr->ih_fn)((void *)eframe);
			else
				ret = (*intr->ih_fn)(intr->ih_arg);
			if (ret != 0) {
				/* increment intr counter */
				intrcnt[level]++; 
				break;
			}
		}

		if (ret == 0) {
			printf("Unclaimed interrupt (level %x and vec %x)\n",
			    level, vec);
		}
	}

	disable_interrupt();

	mask = eframe->mask;

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl((u_char)mask);
}
#endif 

int
cpu_exec_aout_makecmds(p, epp)
struct proc *p;
struct exec_package *epp;
{
#ifdef COMPAT_25
	/*
	 * Keep compatibility with older OpenBSD/mvme88k binaries
	 * for a while, to make transition easier.
	 */
	u_long midmag, magic;
	u_short mid;
	struct exec *execp = epp->ep_hdr;

	midmag = ntohl(execp->a_midmag);
	mid = (midmag >> 16) & 0x3ff;
	magic = midmag & 0xffff;

	midmag = mid << 16 | magic;

	switch (midmag) {
	case (OLD_MID_MACHINE << 16) | ZMAGIC:
		return exec_aout_prep_zmagic(p, epp);
	case (OLD_MID_MACHINE << 16) | NMAGIC:
		return exec_aout_prep_nmagic(p, epp);
	case (OLD_MID_MACHINE << 16) | OMAGIC:
		return exec_aout_prep_omagic(p, epp);
	}
#endif

	return ENOEXEC;
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
 * insert an element into a queue 
 */

void
_insque(velement, vhead)
	void *velement, *vhead;
{
	register struct prochd *element, *head;
	element = velement;
	head = vhead;
	element->ph_link = head->ph_link;
	head->ph_link = (struct proc *)element;
	element->ph_rlink = (struct proc *)head;
	((struct prochd *)(element->ph_link))->ph_rlink=(struct proc *)element;
}

/*
 * remove an element from a queue
 */

void
_remque(velement)
	void *velement;
{
	register struct prochd *element;
	element = velement;
	((struct prochd *)(element->ph_link))->ph_rlink = element->ph_rlink;
	((struct prochd *)(element->ph_rlink))->ph_link = element->ph_link;
	element->ph_rlink = (struct proc *)0;
}

int
copystr(fromaddr, toaddr, maxlength, lencopied)
	const void *fromaddr;
	void *toaddr;
	size_t maxlength;
	size_t *lencopied;
{
	u_int tally;

	tally = 0;

	while (maxlength--) {
		*(u_char *)toaddr = *(u_char *)fromaddr++;
		tally++;
		if (*(u_char *)toaddr++ == 0) {
			if (lencopied) *lencopied = tally;
			return (0);
		}
	}

	if (lencopied)
		*lencopied = tally;

	return (ENAMETOOLONG);
}

void
setrunqueue(p)
	register struct proc *p;
{
	register struct prochd *q;
	register struct proc *oldlast;
	register int which = p->p_priority >> 2;

	if (p->p_back != NULL)
		panic("setrunqueue %x", p);
	q = &qs[which];
	whichqs |= 1 << which;
	p->p_forw = (struct proc *)q;
	p->p_back = oldlast = q->ph_rlink;
	q->ph_rlink = p;
	oldlast->p_forw = p;
}

/*
 * Remove process p from its run queue, which should be the one
 * indicated by its priority.  Calls should be made at splstatclock().
 */
void
remrunqueue(vp)
	struct proc *vp;
{
	register struct proc *p = vp;
	register int which = p->p_priority >> 2;
	register struct prochd *q;

	if ((whichqs & (1 << which)) == 0)
		panic("remrq %x", p);
	p->p_forw->p_back = p->p_back;
	p->p_back->p_forw = p->p_forw;
	p->p_back = NULL;
	q = &qs[which];
	if (q->ph_link == (struct proc *)q)
		whichqs &= ~(1 << which);
}

/* dummys for now */

void
bugsyscall()
{
}

void
myetheraddr(cp)
	u_char *cp;
{
	struct mvmeprom_brdid brdid;

	bugbrdid(&brdid);
	bcopy(&brdid.etheraddr, cp, 6);
}

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

#ifdef EH_DEBUG

void
MY_info(f, p, flags, s)
struct trapframe  *f;
caddr_t     p;
int         flags;
char        *s;
{
	regdump(f);
	printf("proc %x flags %x type %s\n", p, flags, s);
}  

void
MY_info_done(f, flags)
	struct trapframe  *f;
	int         flags;
{
	regdump(f);
} 

#endif

void
nmihand(void *framep)
{
#if 0
	struct m88100_saved_state *frame = framep;
#endif

#if DDB
	DEBUG_MSG("Abort Pressed\n");
	Debugger();
#else
	DEBUG_MSG("Spurious NMI?\n");
#endif /* DDB */
}

void
regdump(struct trapframe *f)
{
#define R(i) f->r[i]
	printf("R00-05: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	       R(0),R(1),R(2),R(3),R(4),R(5));
	printf("R06-11: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	       R(6),R(7),R(8),R(9),R(10),R(11));
	printf("R12-17: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	       R(12),R(13),R(14),R(15),R(16),R(17));
	printf("R18-23: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	       R(18),R(19),R(20),R(21),R(22),R(23));
	printf("R24-29: 0x%08x  0x%08x  0x%08x  0x%08x  0x%08x  0x%08x\n",
	       R(24),R(25),R(26),R(27),R(28),R(29));
	printf("R30-31: 0x%08x  0x%08x\n",R(30),R(31));
	if (cputyp == CPU_88110) {
		printf("exip %x enip %x\n", f->exip, f->enip);
	} else {
		printf("sxip %x snip %x sfip %x\n", f->sxip, f->snip, f->sfip);
	}
#ifdef M88100
	if (f->vector == 0x3 && cputyp != CPU_88110) {
		/* print dmt stuff for data access fault */
		printf("dmt0 %x dmd0 %x dma0 %x\n", f->dmt0, f->dmd0, f->dma0);
		printf("dmt1 %x dmd1 %x dma1 %x\n", f->dmt1, f->dmd1, f->dma1);
		printf("dmt2 %x dmd2 %x dma2 %x\n", f->dmt2, f->dmd2, f->dma2);
		printf("fault type %d\n", (f->dpfsr >> 16) & 0x7);
		dae_print((unsigned *)f);
	}
	if (longformat && cputyp != CPU_88110) {
		printf("fpsr %x fpcr %x epsr %x ssbr %x\n", 
		       f->fpsr, f->fpcr, f->epsr, f->ssbr);
		printf("fpecr %x fphs1 %x fpls1 %x fphs2 %x fpls2 %x\n", 
		       f->fpecr, f->fphs1, f->fpls1, f->fphs2, f->fpls2);
		printf("fppt %x fprh %x fprl %x fpit %x\n", 
		       f->fppt, f->fprh, f->fprl, f->fpit);
		printf("vector %d mask %x mode %x scratch1 %x cpu %x\n", 
		       f->vector, f->mask, f->mode, f->scratch1, f->cpu);
	}
#endif 
#ifdef M88110
	if (longformat && cputyp == CPU_88110) {
		printf("fpsr %x fpcr %x fpecr %x epsr %x\n", 
		       f->fpsr, f->fpcr, f->fpecr, f->epsr);
		printf("dsap %x duap %x dsr %x dlar %x dpar %x\n",
		       f->dsap, f->duap, f->dsr, f->dlar, f->dpar);
		printf("isap %x iuap %x isr %x ilar %x ipar %x\n",
		       f->isap, f->iuap, f->isr, f->ilar, f->ipar);
		printf("vector %d mask %x mode %x scratch1 %x cpu %x\n", 
		       f->vector, f->mask, f->mode, f->scratch1, f->cpu);
	}
#endif
#ifdef MVME188
	if (brdtyp == BRD_188 ) {
		unsigned int istr, cur_mask;

		istr = *(int *volatile)IST_REG;
		cur_mask = GET_MASK(0, istr);
		printf("emask = 0x%b\n", f->mask, IST_STRING);
		printf("istr  = 0x%b\n", istr, IST_STRING);
		printf("cmask = 0x%b\n", cur_mask, IST_STRING);
	}
#endif 
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
	extern void set_tcfp(void);

	struct mvmeprom_brdid brdid;
	
	/*
	 * Must initialize p_addr before autoconfig or
	 * the fault handler will get a NULL reference.
	 * Do this early so that we can take a data or 
	 * instruction fault and survive it. XXX smurph
	 */
	proc0.p_addr = proc0paddr;
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/* zreo out the machine dependant function pointers */
	bzero(&md, sizeof(struct md_p));

	buginit(); /* init the bug routines */
	bugbrdid(&brdid);
	brdtyp = brdid.model;

	/* to support the M8120.  It's based off of MVME187 */
	if (brdtyp == BRD_8120)
		brdtyp = BRD_187;

	/* 
	 * set up interrupt and fp exception handlers 
	 * based on the machine.
	 */
	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		cmmu = &cmmu8820x;
		md.interrupt_func = &m188_ext_int;
		md.intr_mask = NULL;
		md.intr_ipl = NULL;
		md.intr_src = NULL;
		/* clear and disable all interrupts */
		*int_mask_reg[0] = 0;
		*int_mask_reg[1] = 0;
		*int_mask_reg[2] = 0;
		*int_mask_reg[3] = 0;
		break;
#endif /* MVME188 */
#ifdef MVME187
	case BRD_187:
		cmmu = &cmmu8820x;
		md.interrupt_func = &m187_ext_int;
		md.intr_mask = (u_char *)M187_IMASK;
		md.intr_ipl = (u_char *)M187_ILEVEL;
		md.intr_src = NULL;
		break;
#endif /* MVME187 */
#ifdef MVME197
	case BRD_197:
		cmmu = &cmmu88110;
		md.interrupt_func = &m197_ext_int;
		md.intr_mask = (u_char *)M197_IMASK;
		md.intr_ipl = (u_char *)M197_ILEVEL;
		md.intr_src = (u_char *)M197_ISRC;
		set_tcfp(); /* Set Time Critical Floating Point Mode */
		break;
#endif /* MVME197 */
	default:
		panic("mvme_bootstrap: Can't determine cpu type.");
	}

	/* startup fake console driver.  It will be replaced by consinit() */
	cn_tab = &bootcons;

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();
	first_addr = round_page(first_addr);

	last_addr = size_memory();

	cmmu_parity_enable();

	setup_board_config();
	cmmu_init();
	master_cpu = cmmu_cpu_number();
	set_cpu_number(master_cpu);
	printf("CPU%d is master CPU\n", master_cpu);

#ifdef notevenclose
	if (brdtyp == BRD_188 && (boothowto & RB_MINIROOT)) {
		int i;
		for (i=0; i<MAX_CPUS; i++) {
			if (!spin_cpu(i))
				printf("CPU%d started\n", i);
		}
	}
#endif 
	avail_start = first_addr;
	avail_end = last_addr;
	/*
	 * Steal MSGBUFSIZE at the top of physical memory for msgbuf
	 */
	avail_end -= round_page(MSGBUFSIZE);

#ifdef DEBUG
	printf("MVME%x boot: memory from 0x%x to 0x%x\n", brdtyp, avail_start, avail_end);
#endif 
	pmap_bootstrap((vm_offset_t)trunc_page((unsigned)&kernelstart) /* = loadpt */, 
		       &avail_start, &avail_end, &virtual_avail,
		       &virtual_end);
	/*
	 * Tell the VM system about available physical memory.  
	 * mvme88k only has one segment.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
			  atop(avail_start), atop(avail_end),VM_FREELIST_DEFAULT);

	/* Initialize cached PTEs for u-area mapping. */
	save_u_area(&proc0, (vm_offset_t)proc0paddr);

	/*
	 * Map proc0's u-area at the standard address (UADDR).
	 */
	load_u_area(&proc0);

	/* Initialize the "u-area" pages. */
	bzero((caddr_t)UADDR, UPAGES*NBPG);
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
