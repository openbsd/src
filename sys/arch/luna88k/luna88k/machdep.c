/* $OpenBSD: machdep.c,v 1.1.1.1 2004/04/21 15:24:08 aoyama Exp $	*/
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

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/mmu.h>
#include <machine/board.h>
#include <machine/cmmu.h>		/* CMMU stuff	*/
#include <machine/cpu.h>
#include <machine/cpu_number.h>
#include <machine/kcore.h>
#include <machine/locore.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/m88100.h>		/* DMT_VALID	*/

#include <luna88k/luna88k/isr.h>

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

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

vaddr_t interrupt_stack[MAX_CPUS];

/* machine dependent function pointers. */
struct md_p md;

/* prototypes */
void regdump(struct trapframe *f);
void dumpsys(void);
void consinit(void);
vaddr_t size_memory(void);
int getcpuspeed(void);
void identifycpu(void);
void save_u_area(struct proc *, vaddr_t);
void load_u_area(struct proc *);
void dumpconf(void);
void luna88k_ext_int(u_int v, struct trapframe *eframe);
void powerdown(void);

/*
 * *int_mask_reg[CPU]
 * Points to the hardware interrupt status register for each CPU.
 */
unsigned int *volatile int_mask_reg[MAX_CPUS] = {
	(unsigned int *)INT_ST_MASK0,
	(unsigned int *)INT_ST_MASK1,
	(unsigned int *)INT_ST_MASK2,
	(unsigned int *)INT_ST_MASK3
};

/*
 * *clock_reg[CPU]
 */
unsigned int *volatile clock_reg[MAX_CPUS] = {
	(unsigned int *)OBIO_CLOCK0,
	(unsigned int *)OBIO_CLOCK1,
	(unsigned int *)OBIO_CLOCK2,
	(unsigned int *)OBIO_CLOCK3
};

volatile vaddr_t obiova;

int ssir;
int want_ast;
int want_resched;

int physmem;	  /* available physical memory, in pages */
int longformat = 1;  /* for regdump() */
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = IPL_NONE;

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

#if defined(DDB) || NKSYMS > 0
extern char *esym;
#endif

int machtype = LUNA_88K2;	/* XXX: aoyama */
int cputyp = CPU_88100;		/* XXX: aoyama */
int boothowto = RB_ASKNAME;	/* XXX: should be set in boot loader and locore.S */
int bootdev;			/* XXX: should be set in boot loader and locore.S */
int cpuspeed;
double cycles_per_microsecond;	/* used in locore.S:delay() */
int sysconsole = 1;		/* 0 = ttya, 1 = keyboard/mouse, used in dev/sio.c */
u_int16_t dipswitch = 0;	/* set in locore.S */
int hwplanemask;		/* set in luna88k_bootstrap() */

int netisr;

extern char *etext;
extern char *edata;
extern char *end;
extern struct consdev syscons;	/* in dev/siotty.c */

extern void greeting(void);	/* in dev/lcd.c */
extern void syscnattach(int);	/* in dev/siotty.c */
extern int omfb_cnattach(void);	/* in dev/lunafb.c */
extern void ws_cnattach(void);	/* in dev/lunaws.c */

vaddr_t first_addr = 0;
vaddr_t last_addr = 0;

vaddr_t avail_start, avail_end;
vaddr_t virtual_avail, virtual_end;

extern struct pcb *curpcb;
extern struct user *proc0paddr;

/*
 * This is to fake out the console routines, while booting.
 * We could use directly the romtty console, but we want to be able to
 * configure a kernel without romtty since we do not necessarily need a
 * full-blown console driver.
 */
void romttycnprobe(struct consdev *);
void romttycninit(struct consdev *);
void romttycnputc(dev_t, int);
int  romttycngetc(dev_t);
extern void nullcnpollc(dev_t, int);

struct consdev romttycons = {
	NULL, 
	NULL, 
	romttycngetc, 
	romttycnputc,
	nullcnpollc,
	NULL,
	makedev(14,0),
	CN_NORMAL,
};

/*
 * Early console initialization: called early on from main, before vm init.
 */
void
consinit()
{
#ifdef ROM_CONSOLE
	extern struct consdev *cn_tab;
#endif
	/*
	 * Initialize the console before we print anything out.
	 */
#ifdef ROM_CONSOLE
	cn_tab = &romttycons;
	/* cninit(); */
#else /* from NetBSD/luna68k */
	if (sysconsole == 0) {
                syscnattach(0);
        } else {
                omfb_cnattach();
                ws_cnattach();
        }
	/* cninit(); */	/* XXX: this should be later? */
#endif

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
vaddr_t
size_memory()
{
	unsigned int *volatile look;
	unsigned int *max;
#if 0
	extern char *end;
#endif
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
	/*
	 * count it up.
	 */
	max = (void *)MAXPHYSMEM;
#if 0
	for (look = (void *)Roundup(end, STRIDE); look < max;
#else
	for (look = (void *)first_addr; look < max;
#endif
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

	return (trunc_page((unsigned)look));
}

int
getcpuspeed()
{
	double clock_mhz;

	switch(machtype) {
	case LUNA_88K:
		clock_mhz = 25.0;
		break;
	case LUNA_88K2:
		clock_mhz = 33.0;
		break;
	default:
		panic("getcpuspeed: can not determine CPU speed");
		break;
	}

	cycles_per_microsecond = clock_mhz;
	return (int)clock_mhz;
}

void
identifycpu()
{
	cpuspeed = getcpuspeed();
	snprintf(cpu_model, sizeof cpu_model,
	    "OMRON LUNA-88K%s, %dMHz", 
	    machtype == LUNA_88K2 ? "2" : "", cpuspeed);
}

/*
 *	Setup u area ptes for u area double mapping.
 */

void
save_u_area(struct proc *p, vaddr_t va)
{
	int i;

	for (i = 0; i < UPAGES; i++) {
		p->p_md.md_upte[i] = *((pt_entry_t *)kvtopte(va));
		va += NBPG;
	}
}

void
load_u_area(struct proc *p)
{
	int i;
	vaddr_t va;
	pt_entry_t *t;

	for (i = 0, va = UADDR; i < UPAGES; i++) {
		t = kvtopte(va);
		*t = p->p_md.md_upte[i];
		va += NBPG;
	}
	cmmu_flush_tlb(cpu_number(), 1, UADDR, USPACE);
}

void
cpu_startup()
{
	caddr_t v;
	int sz, i;
	vsize_t size;
	int base, residual;
	vaddr_t minaddr, maxaddr, uarea_pages;

	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in mvme_bootstrap() to compensate.
	 */
	for (i = 0; i < btoc(MSGBUFSIZE); i++)
		pmap_kenter_pa((paddr_t)msgbufp + i * NBPG,
		    avail_end + i * NBPG, VM_PROT_READ | VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Check front DIP switch setting
	 */
	printf("dipsw = 0x%x\n", dipswitch);

	/* Check DIP switch 1 - 1 */
	if ((0x8000 & dipswitch) == 0) {
		boothowto |= RB_SINGLE;
	}

	/* Check DIP switch 1 - 3 */
	if ((0x2000 & dipswitch) == 0) {
		boothowto |= RB_ASKNAME;
	}

	/* Check DIP switch 1 - 4 */
	if ((0x1000 & dipswitch) == 0) {
		boothowto |= RB_CONFIG;
	}

	/*
	 * Get frame buffer depth from ROM work area.
	 */
	{
		int depth;

		depth = *((volatile int *)0x00001114);
		printf("frame buffer depth = %d\n", depth);
		switch (depth) {
		case 1:
			hwplanemask = 0x01;
			break;
		case 4:
			hwplanemask = 0x0f;
			break;
		case 8:
			hwplanemask = 0xff;
			break;
		default:
			hwplanemask = 0;	/* No frame buffer */
			break;
		}
	}

#if 0 /* just for test */
	/*
	 * Get boot arguments
	 */
	{
		char buf[256];
		char **p = (volatile char **)0x00001120;

		strncpy(buf, *p, 256);
		if (buf[255] != '\0')
			buf[255] = '\0';

		printf("boot arg: (0x%x) %s\n", *p, buf);
	}
#endif

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
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (uarea_pages != UADDR)
		panic("uarea_pages %lx: UADDR not free", uarea_pages);

	/*
	 * Grab the OBIO space that we hardwired in pmap_bootstrap
	 */
	obiova = OBIO_START;
	uvm_map(kernel_map, (vaddr_t *)&obiova, OBIO_SIZE,
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	      UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	        UVM_ADV_NORMAL, 0));
	if (obiova != OBIO_START)
		panic("obiova %lx: OBIO not free", obiova);

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

	printf("avail mem = %ld (%d pages)\n", ptoa(uvmexp.free), uvmexp.free);
	printf("using %d buffers containing %d bytes of memory\n", nbuf,
	    bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Initialize the autovectored interrupt list.
	 */
	isrinit();

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

	/*
	 * Say hello to the world on LCD.
	 */
	greeting();
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
	    (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
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
	struct trapframe *tf = (struct trapframe *)USER_REGS(p);

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
		tf->tf_epsr = PSR_SRM | PSR_SFD;
		/*
		 * XXX disable OoO for now...
		 */
		tf->tf_epsr |= PSR_SER;
	} else {
		/*
		 * user mode, interrupts enabled,
		 * no graphics unit, fp enabled
		 */
		tf->tf_epsr = PSR_SFD | PSR_SFD2;
	}

	/*
	 * We want to start executing at pack->ep_entry. The way to
	 * do this is force the processor to fetch from ep_entry. Set
	 * NIP to something bogus and invalid so that it will be a NOOP.
	 * And set sfip to ep_entry with valid bit on so that it will be
	 * fetched.  mc88110 - just set exip to pack->ep_entry.
	 */
	if (cputyp == CPU_88110) {
		tf->tf_exip = pack->ep_entry & ~3;
#ifdef DEBUG
		printf("exec @ 0x%x\n", tf->tf_exip);
#endif
	} else {
		tf->tf_snip = pack->ep_entry & ~3;
		tf->tf_sfip = (pack->ep_entry & ~3) | FIP_V;
	}
	tf->tf_r[2] = stack;
	tf->tf_r[31] = stack;
	retval[1] = 0;
}

struct sigstate {
	int   ss_flags;	     /* which of the following are valid */
	struct   trapframe ss_frame;  /* original exception frame */
};

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signo
 * through sf_handler so... don't screw with them!
 */
struct sigframe {
	int			sf_signo;	/* signo for handler */
	siginfo_t *		sf_sip;
	struct sigcontext *	sf_scp;		/* context ptr for handler */
	sig_t			sf_handler;	/* handler addr for u_sigc */
	struct sigcontext	sf_sc;		/* actual context */
	siginfo_t		sf_si;
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
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	int oonstack, fsize;
	struct sigframe sf;
	int addr;

	tf = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) &&
	    (psp->ps_sigstk.ss_flags & SS_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)(tf->tf_r[31] - fsize);

	/* make sure the frame is aligned on a 8 byte boundary */
	if (((vaddr_t)fp & 0x07) != 0)
		fp = (struct sigframe *)((vaddr_t)fp & ~0x07);

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
	bcopy((const void *)&tf->tf_regs, (void *)&sf.sf_sc.sc_regs,
	    sizeof(sf.sf_sc.sc_regs));

	if (copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
	/*
	 * Build the argument list for the signal handler.
	 * Signal trampoline code is at base of user stack.
	 */
	addr = p->p_sigcode;
	if (cputyp != CPU_88110) {
		/* mc88100 */
		tf->tf_snip = (addr & ~3) | NIP_V;
		tf->tf_sfip = (tf->tf_snip + 4) | FIP_V;
	} else {
		/* mc88110 */
		tf->tf_exip = (addr & ~3);
		tf->tf_enip = (tf->tf_exip + 4);
	}
	tf->tf_r[31] = (unsigned)fp;
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
	struct sigcontext *scp;
	struct trapframe *tf;
	struct sigcontext ksc;

	scp = (struct sigcontext *)SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if (((vaddr_t)scp & 3) != 0 ||
	    copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(struct sigcontext)))
		return (EINVAL);

	tf = p->p_md.md_tf;
	scp = &ksc;

	/*
	 * this can be improved by doing
	 *	 bcopy(sc_reg to tf, sizeof sigcontext - 2 words)
	 * XXX nivas
	 */
	bcopy((const void *)&scp->sc_regs, (caddr_t)&tf->tf_regs,
	    sizeof(scp->sc_regs));

	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & SS_ONSTACK)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
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

	/* Luna88k supports automatic powerdown */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
		printf("attempting to power down...\n");
		powerdown();
		/* if failed, fall through. */
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
	} else {
		/* Reset all cpus, which causes reboot */
		*((volatile unsigned *)0x6d000010) = 0;
	}

	for (;;);  /* to keep compiler happy, and me from going crazy */
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

	/* luna88k only uses a single segment. */
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

/* gets an interrupt stack for slave processors */
vaddr_t
get_slave_stack()
{
	vaddr_t addr;

	addr = (vaddr_t)uvm_km_zalloc(kernel_map, INTSTACK_SIZE);

	if (addr == NULL)
		panic("Cannot allocate slave stack for cpu %d",
		    cpu_number());

	interrupt_stack[cpu_number()] = addr;
	return addr;
}

/*
 * Slave CPU pre-main routine.
 * Determine CPU number and set it.
 *
 * Running on an interrupt stack here; do nothing fancy.
 *
 * Called from "luna88k/locore.S"
 */
void
slave_pre_main()
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
 *	Device interrupt handler for LUNA88K
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

#define GET_MASK(cpu, val)	*int_mask_reg[cpu] & (val)
extern unsigned int luna88k_curspl[MAX_CPUS];	/* XXX sould be here? */
extern unsigned int int_mask_val[INT_LEVEL];	/* XXX sould be here? */

void 
luna88k_ext_int(u_int v, struct trapframe *eframe)
{
	int cpu = cpu_number();
	unsigned int cur_mask, cur_int;
	unsigned int level, old_spl;

	cur_mask = *int_mask_reg[cpu];
	old_spl = luna88k_curspl[cpu];
	eframe->tf_mask = old_spl;

	cur_int = cur_mask >> 29;

	if (cur_int == 0) {
		/*
		 * Spurious interrupts - may be caused by debug output clearing
		 * DUART interrupts.
		 */
		printf("luna88k_ext_int(): Spurious interrupts?\n");
		flush_pipeline();
		goto out;
	}
 
	uvmexp.intrs++;

	/* 
	 * We want to service all interrupts marked in the IST register
	 * They are all valid because the mask would have prevented them
	 * from being generated otherwise.  We will service them in order of
	 * priority. 
	 */

	/* XXX: This is very rough. Should be considered more. (aoyama) */
	do {
		level = (cur_int > old_spl ? cur_int : old_spl);
		if (level >= 8) {
			register int i;

			printf("safe level %d <= old level %d\n", level, old_spl);
			printf("cur_int = 0x%x\n", cur_int);

			for (i = 0; i < 4; i++)
				printf("IEN%d = 0x%x  ", i, *int_mask_reg[i]);
			printf("\nCPU0 spl %d  CPU1 spl %d  CPU2 spl %d  CPU3 spl %d\n",
			       luna88k_curspl[0], luna88k_curspl[1],
			       luna88k_curspl[2], luna88k_curspl[3]);
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

		switch(cur_int) {
		case CLOCK_INT_LEVEL:
			/* increment intr counter */
			intrcnt[M88K_CLK_IRQ]++;

			*clock_reg[cpu] = 0xFFFFFFFFU;  /* reset clock */

			/*
			if (clock_enabled[cpu])
				sys_clock_interrupt(USERMODE(eframe[EF_EPSR]));
			*/
			hardclock((void *)eframe);
			break;
		case 5:
		case 4:
		case 3:
			isrdispatch_autovec(cur_int);
			break;
		default:
			printf("luna88k_ext_int(): level %d interrupt.\n", cur_int);
			break;
		}
	} while ((cur_int = (*int_mask_reg[cpu]) >> 29) != 0);

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	disable_interrupt();
out:
	if (eframe->tf_dmt0 & DMT_VALID)
		m88100_trap(T_DATAFLT, eframe);

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl(eframe->tf_mask);
	flush_pipeline();		/* XXX: need this? */
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
 * insert an element into a queue
 */

void
_insque(velement, vhead)
	void *velement, *vhead;
{
	struct prochd *element, *head;
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
	struct prochd *element;
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
	struct proc *p;
{
	struct prochd *q;
	struct proc *oldlast;
	int which = p->p_priority >> 2;

	if (p->p_back != NULL)
		panic("setrunqueue %p", p);
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
	struct proc *p = vp;
	int which = p->p_priority >> 2;
	struct prochd *q;

	if ((whichqs & (1 << which)) == 0)
		panic("remrq %p", p);
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
	struct trapframe *frame = framep;
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
#define R(i) f->tf_r[i]
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
		printf("exip %x enip %x\n", f->tf_exip, f->tf_enip);
	} else {
		printf("sxip %x snip %x sfip %x\n",
		    f->tf_sxip, f->tf_snip, f->tf_sfip);
	}
#ifdef M88100
	if (f->tf_vector == 0x3 && cputyp != CPU_88110) {
		/* print dmt stuff for data access fault */
		printf("dmt0 %x dmd0 %x dma0 %x\n",
		    f->tf_dmt0, f->tf_dmd0, f->tf_dma0);
		printf("dmt1 %x dmd1 %x dma1 %x\n",
		    f->tf_dmt1, f->tf_dmd1, f->tf_dma1);
		printf("dmt2 %x dmd2 %x dma2 %x\n",
		    f->tf_dmt2, f->tf_dmd2, f->tf_dma2);
		printf("fault type %d\n", (f->tf_dpfsr >> 16) & 0x7);
		dae_print((unsigned *)f);
	}
	if (longformat && cputyp != CPU_88110) {
		printf("fpsr %x fpcr %x epsr %x ssbr %x\n",
		    f->tf_fpsr, f->tf_fpcr, f->tf_epsr, f->tf_ssbr);
		printf("fpecr %x fphs1 %x fpls1 %x fphs2 %x fpls2 %x\n",
		    f->tf_fpecr, f->tf_fphs1, f->tf_fpls1,
		    f->tf_fphs2, f->tf_fpls2);
		printf("fppt %x fprh %x fprl %x fpit %x\n",
		    f->tf_fppt, f->tf_fprh, f->tf_fprl, f->tf_fpit);
		printf("vector %d mask %x mode %x scratch1 %x cpu %x\n",
		    f->tf_vector, f->tf_mask, f->tf_mode,
		    f->tf_scratch1, f->tf_cpu);
	}
#endif
#ifdef M88110
	if (longformat && cputyp == CPU_88110) {
		printf("fpsr %x fpcr %x fpecr %x epsr %x\n",
		    f->tf_fpsr, f->tf_fpcr, f->tf_fpecr, f->tf_epsr);
		printf("dsap %x duap %x dsr %x dlar %x dpar %x\n",
		    f->tf_dsap, f->tf_duap, f->tf_dsr, f->tf_dlar, f->tf_dpar);
		printf("isap %x iuap %x isr %x ilar %x ipar %x\n",
		    f->tf_isap, f->tf_iuap, f->tf_isr, f->tf_ilar, f->tf_ipar);
		printf("vector %d mask %x mode %x scratch1 %x cpu %x\n",
		    f->tf_vector, f->tf_mask, f->tf_mode,
		    f->tf_scratch1, f->tf_cpu);
	}
#endif
#ifdef MVME188
	if (brdtyp == BRD_188) {
		unsigned int istr, cur_mask;

		istr = *(int *volatile)IST_REG;
		cur_mask = GET_MASK(0, istr);
		printf("emask = 0x%b\n", f->tf_mask, IST_STRING);
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
luna88k_bootstrap()
{
	extern int kernelstart;
	extern struct consdev *cn_tab;
	extern struct cmmu_p cmmu8820x;

	/*
	 * Must initialize p_addr before autoconfig or
	 * the fault handler will get a NULL reference.
	 * Do this early so that we can take a data or 
	 * instruction fault and survive it. XXX smurph
	 */
	proc0.p_addr = proc0paddr;
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/* zero out the machine dependant function pointers */
	bzero(&md, sizeof(struct md_p));

	/* 
	 * set up interrupt and fp exception handlers 
	 * based on the machine.
	 */
	cmmu = &cmmu8820x;
	md.interrupt_func = &luna88k_ext_int;
	md.intr_mask = NULL;
	md.intr_ipl = NULL;
	md.intr_src = NULL;
	/* clear and disable all interrupts */
	*int_mask_reg[0] = 0;
	*int_mask_reg[1] = 0;
	*int_mask_reg[2] = 0;
	*int_mask_reg[3] = 0;

	/* startup fake console driver.  It will be replaced by consinit() */
	cn_tab = &romttycons;

	uvmexp.pagesize = NBPG;
	uvm_setpagesize();

	first_addr = round_page((vaddr_t) &end);	/* XXX: Is this OK? */
	last_addr = size_memory();
	physmem = btoc(last_addr);

	cmmu_parity_enable();

	setup_board_config();
	cmmu_init();
	master_cpu = cmmu_cpu_number();
	set_cpu_number(master_cpu);

	/*
	 * We may have more than one CPU, so mention which one is the master.
	 * We will also want to spin up slave CPUs on the long run...
	 */
	printf("CPU%d is master CPU\n", master_cpu);

#if 0
	int i;
	for (i = 0; i < MAX_CPUS; i++) {
		if (!spin_cpu(i))
			printf("CPU%d started\n", i);
	}
#endif

	avail_start = first_addr;
	avail_end = last_addr;
	/*
	 * Steal MSGBUFSIZE at the top of physical memory for msgbuf
	 */
	avail_end -= round_page(MSGBUFSIZE);

#ifdef DEBUG
	printf("LUNA88K boot: memory from 0x%x to 0x%x\n", avail_start, avail_end);
#endif
	pmap_bootstrap((vaddr_t)trunc_page((unsigned)&kernelstart) /* = loadpt */,
		       &avail_start, &avail_end, &virtual_avail,
		       &virtual_end);
	/*
	 * Tell the VM system about available physical memory.
	 * luna88k only has one segment.
	 */
	uvm_page_physload(atop(avail_start), atop(avail_end),
			  atop(avail_start), atop(avail_end),VM_FREELIST_DEFAULT);

	/* Initialize cached PTEs for u-area mapping. */
	save_u_area(&proc0, (vaddr_t)proc0paddr);

	/*
	 * Map proc0's u-area at the standard address (UADDR).
	 */
	load_u_area(&proc0);

	/* Initialize the "u-area" pages. */
	bzero((caddr_t)UADDR, UPAGES*NBPG);
#ifdef DEBUG
	printf("leaving luna88k_bootstrap()\n");
#endif
}

/*
 * Rom console routines: 
 * Enables printing of boot messages before consinit().
 */

#define __ROM_FUNC_TABLE	((int **)0x00001100)
#define ROMGETC()	(*(int (*)(void))__ROM_FUNC_TABLE[3])()
#define ROMPUTC(x)	(*(void (*)(int))__ROM_FUNC_TABLE[4])(x)

void
romttycnprobe(cp)
	struct consdev *cp;
{
	cp->cn_dev = makedev(14, 0);
	cp->cn_pri = CN_NORMAL;
}

void
romttycninit(cp)
	struct consdev *cp;
{
	/* Nothing to do */
}

int
romttycngetc(dev)
	dev_t dev;
{
	int s, c;

	do {
		s = splhigh();
		c = ROMGETC();
		splx(s);
	} while (c == -1);
	return c;
}

void
romttycnputc(dev, c)
	dev_t dev;
	int c;
{
	int s;

#if 0
	if ((char)c == '\n')
		ROMPUTC('\r');
#endif
	s = splhigh();
	ROMPUTC(c);
	splx(s);
}

/* taken from NetBSD/luna68k */
void
microtime(tvp)
        register struct timeval *tvp;
{
        int s = splclock();
        static struct timeval lasttime;

        *tvp = time;
#ifdef notdef
        tvp->tv_usec += clkread();
        while (tvp->tv_usec >= 1000000) {
                tvp->tv_sec++;
                tvp->tv_usec -= 1000000;
        }
#endif
        if (tvp->tv_sec == lasttime.tv_sec &&
            tvp->tv_usec <= lasttime.tv_usec &&
            (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
                tvp->tv_sec++;
                tvp->tv_usec -= 1000000;
        }
        lasttime = *tvp;
        splx(s);
}

/* powerdown */

struct pio {
	volatile u_int8_t portA;
	volatile unsigned : 24;
	volatile u_int8_t portB;
	volatile unsigned : 24;
	volatile u_int8_t portC;
	volatile unsigned : 24;
	volatile u_int8_t cntrl;
	volatile unsigned : 24;
};

#define	PIO1_POWER	0x04

#define	PIO1_ENABLE	0x01
#define	PIO1_DISABLE	0x00

void
powerdown(void) 
{
	struct pio *p1 = (struct pio *)OBIO_PIO1_BASE;

	DELAY(100000);
	p1->cntrl = (PIO1_POWER << 1) | PIO1_DISABLE;
	*(volatile u_int8_t *)&p1->portC;
}
