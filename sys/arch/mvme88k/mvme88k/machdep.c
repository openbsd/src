/* $OpenBSD: machdep.c,v 1.11 1999/02/09 06:36:29 smurph Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
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
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/callout.h>
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
#include <net/netisr.h>

#include <mvme88k/dev/pcctworeg.h>
#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/board.h>
#include <machine/trap.h>
#include <machine/bug.h>

#include <dev/cons.h>

#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#define __IS_MACHDEP_C__
#include <assym.s>			  /* EF_EPSR, etc. */
#include <machine/m88100.h>  			/* DMT_VALID        */
#include <machine/m882xx.h>  			/* CMMU stuff       */
#if DDB
#  include <machine/db_machdep.h>
#endif /* DDB */

#if DDB
#define DEBUG_MSG db_printf
#else
#define DEBUG_MSG printf
#endif /* DDB */
static int waittime = -1;

struct intrhand *intr_handlers[256];

unsigned char *ivec[] = {

	(unsigned char *)0xFFFE0003, /* not used, no such thing as int 0 */
	(unsigned char *)0xFFFE0007,
	(unsigned char *)0xFFFE000B,
	(unsigned char *)0xFFFE000F,
	(unsigned char *)0xFFFE0013,
	(unsigned char *)0xFFFE0017,
	(unsigned char *)0xFFFE001B,
	(unsigned char *)0xFFFE001F,
};

u_char *int_mask_level = (u_char *)INT_MASK_LEVEL;
u_char *int_pri_level = (u_char *)INT_PRI_LEVEL;
u_char *iackaddr;
volatile u_char *pcc2intr_mask;
volatile u_char *pcc2intr_ipl;
volatile vm_offset_t bugromva;
volatile vm_offset_t sramva;
volatile vm_offset_t obiova;


int physmem;		/* available physical memory, in pages */
int cold;
vm_offset_t avail_end, avail_start, avail_next;
int msgbufmapped = 0;
int foodebug = 0;
int longformat = 1;
int BugWorks = 0;
/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

#if 0 /*XXX_FUTURE*/
/*
 * iomap stuff is for managing chunks of virtual address space that
 * can be allocated to IO devices.
 * XXX none of the drivers use this at this time. IO address is mapped
 * so that pa == va. XXX nivas
 */
vm_offset_t iomapbase;
struct map *iomap;
vm_map_t   iomap_map;
int	   niomap;
#endif

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int *nofault;

caddr_t allocsys __P((caddr_t));
  
/*
 * Info for CTL_HW
 */
char	machine[] = "mvme88k";		/* cpu "architecture" */
char	cpu_model[120];
extern	char version[];

struct bugenv bugargs;
struct kernel{
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
char *esym;

int boothowto;	/* read in kern/bootstrap */
int cputyp;
int cpuspeed = 33;	/* 25 MHZ XXX should be read from NVRAM */

#ifndef roundup
#define roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))
#endif /* roundup */

vm_size_t	mem_size;
vm_size_t	rawmem_size;
vm_offset_t	first_addr = 0;
vm_offset_t	last_addr = 0;

vm_offset_t	avail_start, avail_next, avail_end;
vm_offset_t	virtual_avail, virtual_end;
vm_offset_t	pcc2consvaddr, clconsvaddr;
vm_offset_t	miniroot;

void		*end_loaded;
int		bootdev;
int		no_symbols = 1;

struct proc	*lastproc;
pcb_t		curpcb;


extern struct user *proc0paddr;

/* XXX this is to fake out the console routines, while booting. */
#include "bugtty.h"
#if NBUGTTY > 0
    int bugttycnprobe __P((struct consdev *));
    int bugttycninit __P((struct consdev *));
    void bugttycnputc __P((dev_t, int));
    int bugttycngetc __P((dev_t));
    extern void nullcnpollc __P((dev_t, int));
    static struct consdev bugcons =
		{ NULL, NULL, bugttycngetc, bugttycnputc,
		    nullcnpollc, makedev(14,0), 1 };
#endif /* NBUGTTY */

void	cmmu_init(void);
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

#if defined (DDB)
        kdb_init();
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
size_memory(void)
{
    volatile unsigned int *look;
    unsigned int *max;
    extern char *end;
#define PATTERN   0x5a5a5a5a
#define STRIDE    (4*1024) 	/* 4k at a time */
#define Roundup(value, stride) (((unsigned)(value) + (stride) - 1) & ~((stride)-1))

    /*
     * count it up.
     */
    max = (void*)MAXPHYSMEM;
    for (look = (void*)Roundup(end, STRIDE); look < max;
			look = (int*)((unsigned)look + STRIDE)) {
	unsigned save;

	/* if can't access, we've reached the end */
	if (foodebug) printf("%x\n", look);
	if (badwordaddr((vm_offset_t)look)) {
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

    physmem = btoc(trunc_page((unsigned)look));	/* in pages */
    return(trunc_page((unsigned)look));
}

void
identifycpu()
{
	/* XXX -take this one out. It can be done in m187_bootstrap() */
	strcpy(cpu_model, "Motorola M88K");
	printf("\nModel: %s\n", cpu_model);
}

/* The following two functions assume UPAGES == 3 */
#if	UPAGES != 3
#error "UPAGES changed?"
#endif

#if	USPACE != (UPAGES * NBPG)
#error "USPACE changed?"
#endif

/*
 *	Setup u area ptes for u area double mapping.
 */

void
save_u_area(struct proc *p, vm_offset_t va)
{
    p->p_md.md_upte[0] = kvtopte(va)->bits;
    p->p_md.md_upte[1] = kvtopte(va + NBPG)->bits;
    p->p_md.md_upte[2] = kvtopte(va + NBPG + NBPG)->bits;
}

void
load_u_area(struct proc *p)
{
    pte_template_t *t;

    t = kvtopte(UADDR);
    t->bits = p->p_md.md_upte[0];
    t = kvtopte(UADDR + NBPG);
    t->bits = p->p_md.md_upte[1];
    t = kvtopte(UADDR + NBPG + NBPG);
    t->bits = p->p_md.md_upte[2];
    cmmu_flush_tlb(1, UADDR, NBPG);
    cmmu_flush_tlb(1, UADDR + NBPG, NBPG);
    cmmu_flush_tlb(1, UADDR + NBPG + NBPG, NBPG);
}

void
cpu_startup()
{
	caddr_t v;
	int sz, i;
	vm_size_t size;    
	int base, residual;
	vm_offset_t minaddr, maxaddr, uarea_pages;
	extern vm_offset_t miniroot;
	/*
	 * Initialize error message buffer (at end of core).
	 * avail_end was pre-decremented in m1x7_bootstrap().
	 */

	for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
		pmap_enter(kernel_pmap, (vm_offset_t)msgbufp,
			avail_end + i * NBPG, VM_PROT_ALL, TRUE);

	msgbufmapped = 1;

	printf(version);
	identifycpu();
	printf("real mem  = %d\n", ctob(physmem));
	
	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Grab UADDR virtual address
	 */
	
	uarea_pages = UADDR;

	vm_map_find(kernel_map, vm_object_allocate(USPACE), 0,
		(vm_offset_t *)&uarea_pages, USPACE, TRUE);

	if (uarea_pages != UADDR) {
		printf("uarea_pages %x: UADDR not free\n", uarea_pages);
		panic("bad UADDR");
	}

	/*
	 * Grab the BUGROM space that we hardwired in pmap_bootstrap
	 */

	bugromva = BUGROM_START;

	vm_map_find(kernel_map, vm_object_allocate(BUGROM_SIZE), 0,
		(vm_offset_t *)&bugromva, BUGROM_SIZE, TRUE);

	if (bugromva != BUGROM_START) {
		printf("bugromva %x: BUGROM not free\n", bugromva);
		panic("bad bugromva");
	}

	/*
	 * Grab the SRAM space that we hardwired in pmap_bootstrap
	 */

	sramva = SRAM_START;

	vm_map_find(kernel_map, vm_object_allocate(SRAM_SIZE), 0,
		(vm_offset_t *)&sramva, SRAM_SIZE, TRUE);

	if (sramva != SRAM_START) {
		printf("sramva %x: SRAM not free\n", sramva);
		panic("bad sramva");
	}

	/*
	 * Grab the OBIO space that we hardwired in pmap_bootstrap
	 */

	obiova = OBIO_START;

	vm_map_find(kernel_map, vm_object_allocate(OBIO_SIZE), 0,
		(vm_offset_t *)&obiova, OBIO_SIZE, TRUE);

	if (obiova != OBIO_START) {
		printf("obiova %x: OBIO not free\n", obiova);
		panic("bad OBIO");
	}

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */

	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
			       &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
		    (vm_offset_t *)&minaddr, size, FALSE) != KERN_SUCCESS) {
		panic("startup: cannot allocate buffers");
	}
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
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
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);

		/* this faults in the required physical pages */
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);

		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
			     16*NCARGS, TRUE);

	/*
	 * Allocate map for physio.
	 */

	phys_map = vm_map_create(kernel_pmap, PHYSIO_MAP_START,
		PHYSIO_MAP_START + PHYSIO_MAP_SIZE, TRUE);
	if (phys_map == NULL) {
		panic("cpu_startup: unable to create phys_map");
	}

#if 0 /*XXX_FUTURE*/
	iomap_map = vm_map_create(kernel_pmap, IOMAP_MAP_START,
			IOMAP_MAP_START + IOMAP_SIZE, TRUE);
	if (iomap_map == NULL) {
		panic("cpu_startup: unable to create iomap_map");
	}

	/*
	 * Allocate space from iomap for a (privately managed) pool
	 * of addresses for IO mappings.
	 */

	iomapbase = kmem_alloc_wait(iomap_map, IOMAP_SIZE);
	rminit(iomap, IOMAP_SIZE, (u_long)iomapbase, "iomap", niomap);
#endif

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
			       M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			   VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
	callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
	   nbuf, bufpages * CLBYTES);

#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	{
		extern void *smini;

		if (miniroot && (boothowto & RB_MINIROOT)) {
			boothowto |= RB_DFLTROOT;
			mfs_initminiroot(miniroot);
		}
	}
#endif

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	nofault = NULL;

	/*
	 * zero out intr_handlers
	 */
	bzero((void *)intr_handlers, 256 * sizeof(struct intrhand *));

	configure();
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
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
	    if (physmem < btoc(2 * 1024 * 1024))
		bufpages = (physmem / 10) / CLSIZE;
	    else
		bufpages = (physmem / 20) / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

#if 0 /*XXX_FUTURE*/
	/*
	 * Arbitrarily limit the number of devices mapping
	 * the IO space at a given time to NIOPMAP (= 32, default).
	 */
	valloc(iomap, struct map, niomap = NIOPMAP);
#endif
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

/*	printf("stack at %x\n", stack);
	printf("%x - %x\n", USRSTACK - MAXSSIZ, USRSTACK);
*/
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
	tf->epsr = 0x3f0;  /* user mode, interrupts enabled, fp enabled */
/*	tf->epsr = 0x3f4;*/  /* user mode, interrupts enabled, fp enabled, MXM Mask */

	/*
	 * We want to start executing at pack->ep_entry. The way to
	 * do this is force the processor to fetch from ep_entry. Set
	 * NIP to something bogus and invalid so that it will be a NOOP.
	 * And set sfip to ep_entry with valid bit on so that it will be
	 * fetched.
	 */

	tf->snip = pack->ep_entry & ~3;
	tf->sfip = (pack->ep_entry & ~3) | FIP_V;
	tf->r[2] = stack;
	tf->r[31] = stack;
	retval[1] = 0;
}

struct sigstate {
	int	ss_flags;		/* which of the following are valid */
	struct 	trapframe ss_frame;	/* original exception frame */
};

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signo
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signo;		/* signo for handler */
	siginfo_t *sf_sip;
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigcontext sf_sc;	/* actual context */
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
	extern char sigcode[], esigcode[];

#define szsigcode (esigcode - sigcode)

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
		(void)grow(p, (unsigned)fp);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
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
	sf.sf_sc.sc_xip = tf->sxip & ~3;
	sf.sf_sc.sc_nip = tf->snip & ~3;
	sf.sf_sc.sc_fip = tf->sfip & ~3;
	sf.sf_sc.sc_ps = tf->epsr;
	sf.sf_sc.sc_sp  = tf->r[31];
	sf.sf_sc.sc_fpsr = tf->fpsr;
	sf.sf_sc.sc_fpcr = tf->fpcr;
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
	addr = (int)PS_STRINGS - szsigcode;
	tf->snip = (addr & ~3) | NIP_V;
	tf->sfip = (tf->snip + 4) | FIP_V;
	tf->r[31] = (unsigned)fp;
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
/* ARGSUSED */
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
	int error;

	scp = (struct sigcontext *)SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0 ||
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
	if (((scp->sc_xip | scp->sc_nip | scp->sc_fip) & 3) != 0)
		printf("xip %x nip %x fip %x\n",
			scp->sc_xip, scp->sc_nip, scp->sc_fip);


	/*
	 * this can be improved by doing
	 *	 bcopy(sc_reg to tf, sizeof sigcontext - 2 words)
	 * XXX nivas
	 */

	bcopy((caddr_t)scp->sc_regs, (caddr_t)tf->r,
		 sizeof(scp->sc_regs));
	tf->sxip = (scp->sc_xip) | XIP_V;
	tf->snip = (scp->sc_nip) | NIP_V;
	tf->sfip = (scp->sc_fip) | FIP_V;
	tf->epsr = scp->sc_ps;
	tf->r[31] = scp->sc_sp;
	tf->fpsr = scp->sc_fpsr;
	tf->fpcr = scp->sc_fpcr;
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
	tf->fpecr = scp->sc_fpecr;
	tf->fphs1 = scp->sc_fphs1;
	tf->fpls1 = scp->sc_fpls1;
	tf->fphs2 = scp->sc_fphs2;
	tf->fpls2 = scp->sc_fpls2;
	tf->fppt = scp->sc_fppt;
	tf->fprh = scp->sc_fprh;
	tf->fprl = scp->sc_fprl;
	tf->fpit = scp->sc_fpit;

	tf->epsr = scp->sc_ps;
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

_doboot()
{
	cmmu_shutdown_now();
	bugreturn();
}

void
boot(howto)
	register int howto;
{
	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr, 0);

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {

		extern struct proc proc0;

		/* protect against curproc->p_stats.foo refs in sync()   XXX */
		if (curproc == NULL)
			curproc = &proc0;

		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();			/* extreme priority */
	if (howto & RB_HALT) {
		printf("halted\n\n");
		bugreturn();
	} else {
		if (howto & RB_DUMP)
			dumpsys();
		doboot();
		/*NOTREACHED*/
	}
	/*NOTREACHED*/
	while (1);  /* to keep compiler happy, and me from going crazy */
}

unsigned	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

dumpconf()
{
	int nblks;

	dumpsize = physmem;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(physmem));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?)
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
dumpsys()
{

	msgbufmapped = 0;
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
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {

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

	default:
		printf("succeeded\n");
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
#ifdef XXX_FUTURE
	extern vm_offset_t iomap_mapin(vm_offset_t, vm_size_t,  boolean_t);
#endif

	/*
	 * map a page in for phys address 0xfffe0000 and set the
	 * addresses for various levels.
	 */
#ifdef XXX_FUTURE
	vaddr = (u_char *)iomap_mapin(0xfffe0000, NBPG, 1);
#else
	vaddr = (u_char *)0xfffe0000;
#endif
#if 0
	(unsigned char *)0xFFFE0003, /* not used, no such thing as int 0 */
	(unsigned char *)0xFFFE0007,
	(unsigned char *)0xFFFE000B,
	(unsigned char *)0xFFFE000F,
	(unsigned char *)0xFFFE0013,
	(unsigned char *)0xFFFE0017,
	(unsigned char *)0xFFFE001B,
	(unsigned char *)0xFFFE001F,
#endif
	ivec[0] = vaddr + 0x03;
	ivec[1] = vaddr + 0x07;
	ivec[2] = vaddr + 0x0b;
	ivec[3] = vaddr + 0x0f;
	ivec[4] = vaddr + 0x13;
	ivec[5] = vaddr + 0x17;
	ivec[6] = vaddr + 0x1b;
	ivec[7] = vaddr + 0x1f;
}

/*
 * find a useable interrupt vector in the range start, end. It starts at
 * the end of the range, and searches backwards (to increase the chances
 * of not conflicting with more normal users)
 */
int
intr_findvec(start, end)
	int start, end;
{
	extern u_long *vector_list[], interrupt_handler, unknown_handler;
	int vec;

	if (start < 0 || end > 255 || start > end)
		return (-1);
	for (vec = end; vec > start; --vec)
		if (vector_list[vec] == &unknown_handler 
/*		 || vector_list[vec] == &interrupt_handler */)
			return (vec);
	return (-1);
}

/*
 * Chain the interrupt handler in. But first check if the vector
 * offset chosen is legal. It either must be a badtrap (not allocated
 * for a `system' purpose), or it must be a hardtrap (ie. already
 * allocated to deal with chained interrupt handlers).
 */
#if 0

int
intr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	extern u_long *vector_list[], interrupt_handler, unknown_handler;
	struct intrhand *ihx;

	if (vector_list[vec] != &interrupt_handler && vector_list[vec] != &unknown_handler) {
		printf("intr_establish: vec %d unavailable\n", vec);
		return (-1);
	}
	vector_list[vec] = &interrupt_handler;
#if DIAGNOSTIC
	printf("assigning vec %x to interrupt_handler\n", vec);
#endif
	ih->ih_next = NULL;	/* just in case */

	/* attach at tail */
	if (ihx = intr_handlers[vec]) {
		while (ihx->ih_next)
			ihx = ihx->ih_next;
		ihx->ih_next = ih;
	} else
		intr_handlers[vec] = ih;
	return (INTR_EST_SUCC);
}

#else
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
		panic("intr_establish: vec (%x) not between 0 and 0xff",
			vec);
#endif /* DIAGNOSTIC */
		return (INTR_EST_BADVEC);
	}

	if (intr = intr_handlers[vec]) {
		if (intr->ih_ipl != ihand->ih_ipl) {
#if DIAGNOSTIC
			panic("intr_establish: there are other handlers with vec (%x) at ipl %x, but you want it at %x",
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
#endif

/*
 *	Device interrupt handler
 *
 *      when we enter, interrupts are disabled;
 *      when we leave, they should be disabled,
 *      but they need not be disabled throughout
 *      the routine.
 */

void
ext_int(u_int v, struct m88100_saved_state *eframe)
{
	register u_char mask, level, xxxvec;
	register struct intrhand *intr;
	int ret;
	u_char vec;

	/* get level and mask */

	asm volatile("ld.b	%0,%1" : "=r" (mask) : "" (*pcc2intr_mask));
	asm volatile("ld.b	%0,%1" : "=r" (level) : "" (*pcc2intr_ipl));

	/*
	 * It is really bizarre for the mask and level to the be the same.
	 * pcc2 for 187 blocks all interrupts at and below the mask value,
	 * so we should not be getting an interrupt at the level that is
	 * already blocked. I can't explain this case XXX nivas
	 */

	if ((mask == level) && level) {
		printf("mask == level, %d\n", level);
		goto beatit;
	}

	/*
	 * Interrupting level cannot be 0--0 doesn't produce an interrupt.
	 * Weird! XXX nivas
	 */

	if (level == 0) {
		printf("Bogons... level %x and mask %x\n", level, mask);
		goto beatit;
	}

	/* and block interrupts at level or lower */
	setipl((u_char)level);
	/* and stash it away in the trap frame */
	eframe->mask = mask;
#if 0
	asm volatile("st.b	%1,%0" : "=m" (*pcc2intr_mask) :  "r" (level));
#endif
	if (level > 7 || (char)level < 0) {
		panic("int level (%x) is not between 0 and 7", level);
	}

	/* generate IACK and get the vector */

#if XXX
	asm volatile("ld.b 	%0,%1" : "=r" (vec) : "" (*ivec[level]));
	asm volatile("tb1	0, r0, 0");	
	asm volatile("tb1	0, r0, 0");	
	asm volatile("tb1	0, r0, 0");	

	asm volatile("tb1	0, r0, 0");	

	if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
		printf("Unable to get vector for this interrupt (level %x)\n",
					level);
		goto out;
	}	
#endif XXX

	asm volatile("tb1	0, r0, 0");	
	if (guarded_access(ivec[level], 1, &vec) == EFAULT) {
		printf("Unable to get vector for this interrupt (level %x)\n",
					level);
		goto out;
	}
	asm volatile("tb1	0, r0, 0");	
	asm volatile("tb1	0, r0, 0");	
	asm volatile("tb1	0, r0, 0");	
	/*vec = xxxvec;*/

	if (vec > 0xFF) {
		panic("interrupt vector %x greater than 255", vec);
	}

	enable_interrupt();

	if ((intr = intr_handlers[vec]) == 0) {
		printf("Spurious interrupt (level %x and vec %x)\n",
			level, vec);
	}
	if (intr && intr->ih_ipl != level) {
		panic("Handler ipl %x not the same as level %x",
			intr->ih_ipl, level);
	}

	/*
	 * Walk through all interrupt handlers in the chain for the
	 * given vector, calling each handler in turn, till some handler
	 * returns a value != 0.
	 */

	for (ret = 0; intr; intr = intr->ih_next) {
		if (intr->ih_wantframe)
			ret = (*intr->ih_fn)(intr->ih_arg, (void *)eframe);
		else
			ret = (*intr->ih_fn)(intr->ih_arg);
		if (ret)
			break;
	}

	if (ret == 0) {
		printf("Unclaimed interrupt (level %x and vec %x)\n",
			level, vec);
	}

	/*
	 * process any remaining data access exceptions before
	 * returning to assembler
	 */
	disable_interrupt();

out:
	if (eframe->dmt0 & DMT_VALID) {
		trap(T_DATAFLT, eframe);
		data_access_emulation(eframe);
		eframe->dmt0 &= ~DMT_VALID;
	}
	mask = eframe->mask;

	/*
	 * Restore the mask level to what it was when the interrupt
	 * was taken.
	 */
	setipl((u_char)mask);
#if 0
	asm volatile("st.b	%1,%0" : "=m" (*pcc2intr_mask) :  "r" (mask));
#endif
#if 0
	splx((u_char)mask);
#endif /* 0 */

beatit:
	return;
}

cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return ENOEXEC;
}

sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sysarch_args /* {
		syscallarg(int) op;
		syscallarg(char *) parm;
	} */ *uap = v;
	int error = 0;

	switch((int)SCARG(uap, op)) {
	default:
		error = EINVAL;
		break;
	}
	return(error);
}

/*
 * machine dependent system variables.
 */
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
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
			if(lencopied) *lencopied = tally;
			return(0);
		}
	}

	if (lencopied)
		*lencopied = tally;

	return(ENAMETOOLONG);
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

bugsyscall()
{
}

void
myetheraddr(cp)
	u_char *cp;
{
	struct bugniocall niocall;
	char *cp2 = (char*) 0xFFC1F2C; /* BBRAM Ethernet hw addr */
	
	niocall.clun = 0;
	niocall.dlun = 0;
	niocall.ci = 0;
	niocall.cd = 0;
	niocall.cid = NETCTRL_GETHDW;
	niocall.memaddr = (unsigned long)cp;
	niocall.nbytes = 6;

	bugnetctrl(&niocall);

/*	if (cp[0] == '\0') {
	    strncpy(cp, cp2, 6);
	}    */
}

void netintr()
{
#ifdef INET
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
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
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
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

void
dosoftint()
{
	if (ssir & SIR_NET) {
		siroff(SIR_NET);
		cnt.v_soft++;
		netintr();
	}

	if (ssir & SIR_CLOCK) {
		siroff(SIR_CLOCK);
		cnt.v_soft++;
		softclock();
	}

	return;
}

int
spl0()
{
	int x;
	int level = 0;
	x = splsoftclock();

	if (ssir) {
		dosoftint();
	}

	setipl(0);

	return(x);
}

badwordaddr(void *addr)
{
	return badaddr((vm_offset_t)addr, 4);
}

MY_info(f, p, flags, s)
	struct trapframe 	*f;
	caddr_t 		p;
	int 			flags;
	char			*s;
{
	regdump(f);
	printf("proc %x flags %x type %s\n", p, flags, s);
}	

MY_info_done(f, flags)
	struct trapframe	*f;
	int			flags;
{
	regdump(f);
}	

void
nmihand(void *framep)
{
	struct m88100_saved_state *frame = framep;

#if DDB
	DEBUG_MSG("Abort Pressed\n");
	Debugger();
#else
	DEBUG_MSG("Spurious NMI?\n");
#endif /* DDB */
}

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
    printf("sxip %x snip %x sfip %x\n", f->sxip, f->snip, f->sfip);
    if (f->vector == 0x3) { /* print dmt stuff for data access fault */
	printf("dmt0 %x dmd0 %x dma0 %x\n", f->dmt0, f->dmd0, f->dma0);
	printf("dmt1 %x dmd1 %x dma1 %x\n", f->dmt1, f->dmd1, f->dma1);
	printf("dmt2 %x dmd2 %x dma2 %x\n", f->dmt2, f->dmd2, f->dma2);
    }
	if (longformat) {
		printf("fpsr %x ", f->fpsr);
		printf("fpcr %x ", f->fpcr);
		printf("epsr %x ", f->epsr);
		printf("ssbr %x\n", f->ssbr);
		printf("dmt0 %x ", f->dmt0);
		printf("dmd0 %x ", f->dmd0);
		printf("dma0 %x ", f->dma0);
		printf("dmt1 %x ", f->dmt1);
		printf("dmd1 %x ", f->dmd1);
		printf("dma1 %x ", f->dma1);
		printf("dmt2 %x ", f->dmt2);
		printf("dmd2 %x ", f->dmd2);
		printf("dma2 %x\n", f->dma2);
		printf("fpecr %x ", f->fpecr);
		printf("fphs1 %x ", f->fphs1);
		printf("fpls1 %x ", f->fpls1);
		printf("fphs2 %x ", f->fphs2);
		printf("fpls2 %x ", f->fpls2);
		printf("fppt %x ", f->fppt);
		printf("fprh %x ", f->fprh);
		printf("fprl %x ", f->fprl);
		printf("fpit %x\n", f->fpit);
		printf("vector %x ", f->vector);
		printf("mask %x ", f->mask);
		printf("mode %x ", f->mode);
		printf("scratch1 %x ", f->scratch1);
		printf("pad %x\n", f->pad);
	}
}

#if DDB
inline int
db_splhigh(void)
{
	return (db_setipl(IPL_HIGH));
}

inline int
db_splx(int s)
{
	return (db_setipl(s));
}
#endif /* DDB */	

/*
 * Called from locore.S during boot,
 * this is the first C code that's run.
 */

void
m187_bootstrap(void)
{
    extern char version[];
    extern char *edata, *end;
    extern int cold;
    extern int kernelstart;
    extern vm_offset_t size_memory(void);
    extern struct consdev *cn_tab;
    struct bugbrdid brdid;

    cold = 1;	/* we are still booting */
#if NBUGTTY > 0
    cn_tab = &bugcons;
#endif
    buginit();

    bugbrdid(&brdid);
    cputyp = brdid.brdno;

    vm_set_page_size();

#if 0
    esym  = kflags.esym;
    boothowto = kflags.bflags;
    bootdev = kflags.bdev;
#endif /* 0 */
    
#if 0
    end_loaded = kflags.end_load;
    if (esym != NULL) {
    	end = (char *)((int)(kflags.symtab));
    } else {
    	first_addr = (vm_offset_t)&end;
    }
#endif

    first_addr = m88k_round_page(first_addr);

    if (!no_symbols) boothowto |= RB_KDB;

    last_addr = size_memory();

    cmmu_init();

    avail_start = first_addr;
    avail_end = last_addr;
    /*printf("%s",version);*/
    printf("M187 boot: memory from 0x%x to 0x%x\n", avail_start, avail_end);
    printf("M187 boot: howto 0x%x\n", boothowto);

    /*
     * Steal one page at the top of physical memory for msgbuf
     */
    avail_end -= PAGE_SIZE;

#if 1
    pmap_bootstrap((vm_offset_t)M88K_TRUNC_PAGE((unsigned)&kernelstart) /* = loadpt */, 
		   &avail_start, &avail_end, &virtual_avail,
		   &virtual_end);
#endif

    /*
     * Must initialize p_addr before autoconfig or
     * the fault handler will get a NULL reference.
     */
    proc0.p_addr = proc0paddr;
    curproc = &proc0;
    curpcb = &proc0paddr->u_pcb;

    /* Initialize cached PTEs for u-area mapping. */
    save_u_area(&proc0, (vm_offset_t)proc0paddr);

    /*
     * Map proc0's u-area at the standard address (UADDR).
     */
    load_u_area(&proc0);

    /* Initialize the "u-area" pages. */
    bzero((caddr_t)UADDR, UPAGES*NBPG);
    
}
