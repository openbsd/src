/*	$OpenBSD: machdep.c,v 1.5 1999/05/22 21:22:32 weingart Exp $	*/
/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)machdep.c	8.3 (Berkeley) 1/12/94
 *      $Id: machdep.c,v 1.5 1999/05/22 21:22:32 weingart Exp $
 */

/* from: Utah Hdr: machdep.c 1.63 91/04/24 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif

#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/autoconf.h>
#include <machine/memconf.h>

#include <sys/exec_ecoff.h>

#include <dev/cons.h>

#include <wgrisc/wgrisc/wgrisctype.h>
#include <wgrisc/riscbus/riscbus.h>

extern struct consdev *cn_tab;

/* the following is used externally (sysctl_hw) */
char	machine[] = "wgrisc";	/* cpu "architecture" */
char	cpu_model[30];

vm_map_t buffer_map;

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
int	msgbufmapped = 0;	/* set when safe to use msgbuf */
int	physmem;		/* max supported memory, changes to actual */
int	cpucfg;			/* Value of processor config register */
int	cputype;		/* Mother board type */
int	ncpu = 1;		/* At least one cpu in the system */
int	isa_io_base;		/* Base address of ISA io port space */
int	isa_mem_base;		/* Base address of ISA memory space */

struct mem_descriptor mem_layout[MAXMEMSEGS];

extern	int Mach_spl0(), Mach_spl1(), Mach_spl2(), Mach_spl3();
extern	int Mach_spl4(), Mach_spl5(), splhigh();
int	(*Mach_splnet)() = splhigh;
int	(*Mach_splbio)() = splhigh;
int	(*Mach_splimp)() = splhigh;
int	(*Mach_spltty)() = splhigh;
int	(*Mach_splclock)() = splhigh;
int	(*Mach_splstatclock)() = splhigh;

static void tlb_init_pica();
static void tlb_init_tyne();


/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

struct	user *proc0paddr;
struct	proc nullproc;		/* for use by swtch_exit() */

/*
 * Do all the stuff that locore normally does before calling main().
 * Process arguments passed to us by the BOOT.
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 * Return the first page address following the system.
 */
mips_init(argc, argv, code)
	int argc;
	char *argv[];
	u_int code;
{
	register char *cp;
	register int i;
	register unsigned firstaddr;
	register caddr_t sysend;
	caddr_t start;
	struct tlb tlb;
	extern char _ftext[], edata[], end[];
	int realmemsize;
	char nvdata[32];

	/* clear the BSS segment in OpenBSD code */
	sysend = (caddr_t)mips_round_page(end);
	bzero(edata, sysend - edata);

	/* Extract this from BBRAM environment variable */
	ReadNVram(&nvdata);
	realmemsize = nvdata[2];
	if (realmemsize > 36 || realmemsize < 8) {
	    realmemsize = 36;
	}

	/* Initialize the CPU type */
	cputype = WGRISC9100;
	mem_layout[0].mem_start = 0;
	mem_layout[0].mem_size = mips_trunc_page(CACHED_TO_PHYS(_ftext));
	mem_layout[1].mem_start = CACHED_TO_PHYS((int)sysend);
	mem_layout[1].mem_size = 0x400000 - (int)(CACHED_TO_PHYS(sysend));
	physmem = 4096 * 1024;

	switch (realmemsize) {
	default:
	case 8:       /* 8 MB  (0 MB simm) */
 	    mem_layout[2].mem_start = 0x400000;
	    mem_layout[2].mem_size =  0x400000;
	    physmem += 4096 * 1024;
	    break;
	case 12:       /* 12 MB  (4 MB simm) */
 	    mem_layout[2].mem_start = 0x400000;
	    mem_layout[2].mem_size =  0x800000;
	    physmem += 8192 * 1024;
	    break;
	case 20:       /* 20 MB (16MB SIMM + 0 in onboard Bank2) */
 	    mem_layout[2].mem_start =0x1000000;
	    mem_layout[2].mem_size = 0x1000000;
	    physmem += 16 * 1024 * 1024;
	    break;
	case 24:       /* 24 MB (16MB SIMM + 4 in onboard Bank2) */
 	    mem_layout[2].mem_start = 0x400000;
	    mem_layout[2].mem_size =  0x400000;
 	    mem_layout[3].mem_start =0x1000000;
	    mem_layout[3].mem_size = 0x1000000;
	    physmem += 20 * 1024 * 1024;
	    break;
        case 36:       /* 36 MB (32MB SIMM and ignore bank2 onboard) */
 	    mem_layout[2].mem_start = 0x400000;
	    mem_layout[2].mem_size = 0x2000000;
	    physmem += 32768 * 1024;
	    break;
	}

	switch (cputype) {
	case WGRISC9100:
		strcpy(cpu_model, "Willowglen RISC PC 9100");
		isa_io_base = RISC_ISA_IO_BASE;
		isa_mem_base = RISC_ISA_MEM_BASE;

		/*
		 * Set up interrupt handling and I/O addresses.
		 */
#if 0 /* XXX FIXME */
		Mach_splnet = Mach_spl1;
		Mach_splbio = Mach_spl0;
		Mach_splimp = Mach_spl1;
		Mach_spltty = Mach_spl2;
		Mach_splstatclock = Mach_spl3;
#endif
		break;

	default:
		boot(RB_HALT | RB_NOSYNC);
	}
	physmem = btoc(physmem);

	/* look at argv[0] and compute bootdev for autoconfig setup */
	makebootdev(argv[0]);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 * Default to SINGLE and ASKNAME if no args.
	 */
	boothowto = RB_SINGLE | RB_ASKNAME;
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			if(strncasecmp("osloadoptions=",argv[i],14) == 0) {
				for (cp = argv[i]+14; *cp; cp++) {
					switch (*cp) {
					case 'a': /* autoboot */
						boothowto &= ~RB_SINGLE;
						break;

					case 'd': /* use compiled in default root */
						boothowto |= RB_DFLTROOT;
						break;

					case 'm': /* mini root present in memory */
						boothowto |= RB_MINIROOT;
						break;

					case 'n': /* ask for names */
						boothowto |= RB_ASKNAME;
						break;

					case 'N': /* don't ask for names */
						boothowto &= ~RB_ASKNAME;
						break;
					}

				}
			}
		}
	}

#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	if (boothowto & RB_MINIROOT) {
		boothowto |= RB_DFLTROOT;
		sysend += mfs_initminiroot(sysend);
	}
#endif

#ifdef R4K
	R4K_SetWIRED(0);
	R4K_TLBFlush();
	R4K_SetWIRED(R4K_NUM_WIRED_ENTRIES);

	switch (cputype) {
	case ACER_PICA_61:
		tlb_init_pica();
		break;
	}

#else
	R3K_TLBFlush();
#endif
	
	/*
	 * Init mapping for u page(s) for proc[0], pm_tlbpid 1.
	 */
	sysend = (caddr_t)((int)sysend + 3 & -4);
	start = sysend;
	curproc->p_addr = proc0paddr = (struct user *)sysend;
	curproc->p_md.md_regs = proc0paddr->u_pcb.pcb_regs;
	firstaddr = CACHED_TO_PHYS(sysend);
	for (i = 0; i < UPAGES;) {
#ifdef R4K
		tlb.tlb_mask = PG_SIZE_4K;
		tlb.tlb_hi = vad_to_vpn((UADDR + (i << PGSHIFT))) | 1;
		tlb.tlb_lo0 = vad_to_pfn(firstaddr) | PG_V | PG_M | PG_CACHED;
		tlb.tlb_lo1 = vad_to_pfn(firstaddr + NBPG) | PG_V | PG_M | PG_CACHED;
		curproc->p_md.md_upte[i] = tlb.tlb_lo0;
		curproc->p_md.md_upte[i+1] = tlb.tlb_lo1;
		R4K_TLBWriteIndexed(i,&tlb);
		firstaddr += NBPG * 2;
		i += 2;
		R4K_SetPID(1);
#else
		R3K_TLBWriteIndexed(i,
		   (UADDR + (i << PGSHIFT)) | (1 << R3K_PID_SHIFT),
		    (curproc->p_md.md_upte[i] = firstaddr | PG_V | PG_M));
		firstaddr += NBPG;
		i += 1;
		R3K_SetPID(1);
#endif
	}
	sysend += UPAGES * NBPG;
	sysend = (caddr_t)((int)sysend+3 & -4);

	/*
	 * init nullproc for swtch_exit().
	 * init mapping for u page(s), pm_tlbpid 0
	 * This could be used for an idle process.
	 */
	nullproc.p_addr = (struct user *)sysend;
	nullproc.p_md.md_regs = nullproc.p_addr->u_pcb.pcb_regs;
	bcopy("nullproc", nullproc.p_comm, sizeof("nullproc"));
	firstaddr = CACHED_TO_PHYS(sysend);
	for (i = 0; i < UPAGES; i+=2) {
		nullproc.p_md.md_upte[i] = vad_to_pfn(firstaddr) | PG_V | PG_M | PG_CACHED;
		nullproc.p_md.md_upte[i+1] = vad_to_pfn(firstaddr + NBPG) | PG_V | PG_M | PG_CACHED;
		firstaddr += NBPG * 2;
	}
	sysend += UPAGES * NBPG;

	/* clear pages for u areas */
	bzero(start, sysend - start);

	/*
	 * Copy down exception vector code.
	 */
	{
#ifdef R4K
		extern char R4K_TLBMiss[], R4K_TLBMissEnd[];
		extern char R4K_Exception[], R4K_ExceptionEnd[];

		if (R4K_TLBMissEnd - R4K_TLBMiss > 0x80)
			panic("startup: TLB code too large");
		bcopy(R4K_TLBMiss, (char *)R4K_TLB_MISS_EXC_VEC,
			R4K_TLBMissEnd - R4K_TLBMiss);
		bcopy(R4K_Exception, (char *)R4K_GEN_EXC_VEC,
			R4K_ExceptionEnd - R4K_Exception);

		cpucfg = R4K_ConfigCache();
		R4K_FlushCache();
#else
		extern char R3K_UTLBMiss[], R3K_UTLBMissEnd[];
		extern char R3K_Exception[], R3K_ExceptionEnd[];
		if (R3K_UTLBMissEnd - R3K_UTLBMiss > 0x80)
			panic("startup: TLB code too large");
		bcopy(R3K_UTLBMiss, (char *)R3K_TLB_MISS_EXC_VEC,
			R3K_UTLBMissEnd - R3K_UTLBMiss);
		bcopy(R3K_Exception, (char *)R3K_GEN_EXC_VEC,
			R3K_ExceptionEnd - R3K_Exception);

		cpucfg = R3K_ConfigCache();
		R3K_FlushCache();
#endif
	}

	/*
	 * Initialize error message buffer.
	 */
	msgbufp = (struct msgbuf *)(sysend);
	sysend = (caddr_t)(sysend + (sizeof(struct msgbuf)));
	msgbufmapped = 1;

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "sysend".
	 * As pages of kernel virtual memory are allocated, "sysend"
	 * is incremented.
	 *
	 * These data structures are allocated here instead of cpu_startup()
	 * because physical memory is directly addressable. We don't have
	 * to map these into virtual address space.
	 */
	start = sysend;

#define	valloc(name, type, num) \
	    (name) = (type *)sysend; sysend = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)sysend; sysend = (caddr_t)((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
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
	 * Determine how many buffers to allocate.
	 * We allocate more buffer space than the BSD standard of
	 * using 10% of memory for the first 2 Meg, 5% of remaining.
	 * We just allocate a flat 10%. Ensure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = physmem / 10 / CLSIZE;
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

	/*
	 * Clear allocated memory.
	 */
	bzero(start, sysend - start);

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap((vm_offset_t)sysend);
}

#if keep_for_future_r4k_machines
void
tlb_init_pica()
{
	struct tlb tlb;

	tlb.tlb_mask = PG_SIZE_256K;
	tlb.tlb_hi = vad_to_vpn(R4030_V_LOCAL_IO_BASE);
	tlb.tlb_lo0 = vad_to_pfn(R4030_P_LOCAL_IO_BASE) | PG_IOPAGE;
	tlb.tlb_lo1 = vad_to_pfn(PICA_P_INT_SOURCE) | PG_IOPAGE;
	R4K_TLBWriteIndexed(1, &tlb);
}
#endif

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;
	cninit();
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	register unsigned i;
	register caddr_t v;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;		/* Shut up pmap debug during bootstrap */
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
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
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, TRUE);
	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

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

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);
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
	configure();

	spl0();		/* safe to turn interrupts on now */
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

/*
 * Set registers on exec.
 * Clear all registers except sp, pc.
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	extern struct proc *cpuFPCurProcPtr;

	bzero((caddr_t)p->p_md.md_regs, (FSR + 1) * sizeof(int));
	p->p_md.md_regs[SP] = stack;
	p->p_md.md_regs[PC] = pack->ep_entry & ~3;
	p->p_md.md_regs[T9] = pack->ep_entry & ~3; /* abicall req */
#ifdef R4K
	p->p_md.md_regs[PS] = R4K_PSL_USERSET;
#else
	p->p_md.md_regs[PS] = R3K_PSL_USERSET;
#endif
	p->p_md.md_flags & ~MDP_FPUSED;
	if (cpuFPCurProcPtr == p)
		cpuFPCurProcPtr = (struct proc *)0;
	p->p_md.md_ss_addr = 0;
}

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signum;		/* signo for handler */
	siginfo_t *sf_sip;		/* pointer to siginfo_t */
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
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct sigframe *fp;
	register int *regs;
	register struct sigacts *psp = p->p_sigacts;
	int oonstack, fsize;
	struct sigcontext ksc;
	extern char sigcode[], esigcode[];

	regs = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if (!(psp->ps_siginfo & sigmask(sig)))
		fsize -= sizeof(siginfo_t);
	if ((psp->ps_flags & SAS_ALTSTACK) &&
	    (psp->ps_sigstk.ss_flags & SA_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else
		fp = (struct sigframe *)(regs[SP] - fsize);
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
	ksc.sc_onstack = oonstack;
	ksc.sc_mask = mask;
	ksc.sc_pc = regs[PC];
	ksc.mullo = regs[MULLO];
	ksc.mulhi = regs[MULHI];
	ksc.sc_regs[ZERO] = 0xACEDBADE;		/* magic number */
	bcopy((caddr_t)&regs[1], (caddr_t)&ksc.sc_regs[1],
		sizeof(ksc.sc_regs) - sizeof(int));
	ksc.sc_fpused = p->p_md.md_flags & MDP_FPUSED;
	if (ksc.sc_fpused) {
		extern struct proc *cpuFPCurProcPtr;

		/* if FPU has current state, save it first */
		if (p == cpuFPCurProcPtr)
			CPU_SaveCurFPState(p);
		bcopy((caddr_t)&p->p_md.md_regs[F0], (caddr_t)ksc.sc_fpregs,
			sizeof(ksc.sc_fpregs));
	}

	if (psp->ps_siginfo & sigmask(sig)) {
		siginfo_t si;

		initsiginfo(&si, sig, code, type, val);
		if (copyout((caddr_t)&si, (caddr_t)&fp->sf_si, sizeof si))
			goto bail;
	}

	if (copyout((caddr_t)&ksc, (caddr_t)&fp->sf_sc, sizeof(ksc))) {
bail:
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
	 */
	regs[A0] = sig;
	regs[A1] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : NULL;
	regs[A2] = (int)&fp->sf_sc;
	regs[A3] = (int)catcher;

	regs[PC] = (int)catcher;
	regs[T9] = (int)catcher;
	regs[SP] = (int)fp;
	/*
	 * Signal trampoline code is at base of user stack.
	 */
	regs[RA] = (int)PS_STRINGS - (esigcode - sigcode);
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
	register int *regs;
	struct sigcontext ksc;
	int error;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	regs = p->p_md.md_regs;
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	error = copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(ksc));
	if (error || ksc.sc_regs[ZERO] != 0xACEDBADE) {
#ifdef DEBUG
		if (!(sigdebug & SDB_FOLLOW))
			printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
		printf("  old sp %x ra %x pc %x\n",
			regs[SP], regs[RA], regs[PC]);
		printf("  new sp %x ra %x pc %x err %d z %x\n",
			ksc.sc_regs[SP], ksc.sc_regs[RA], ksc.sc_regs[PC],
			error, ksc.sc_regs[ZERO]);
#endif
		return (EINVAL);
	}
	scp = &ksc;
	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = scp->sc_mask &~ sigcantmask;
	regs[PC] = scp->sc_pc;
	regs[MULLO] = scp->mullo;
	regs[MULHI] = scp->mulhi;
	bcopy((caddr_t)&scp->sc_regs[1], (caddr_t)&regs[1],
		sizeof(scp->sc_regs) - sizeof(int));
	if (scp->sc_fpused)
		bcopy((caddr_t)scp->sc_fpregs, (caddr_t)&p->p_md.md_regs[F0],
			sizeof(scp->sc_fpregs));
	return (EJUSTRETURN);
}

int	waittime = -1;

void
boot(howto)
	register int howto;
{

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr, 0);

#ifdef DEBUG
	if (panicstr)
		stacktrace();
#endif

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* fill curproc with live object */
		if (curproc == NULL)
			curproc = &proc0;
		/*
		 * Synchronize the disks....
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	(void) splhigh();		/* extreme priority */
	if (howto & RB_HALT) {
		printf("System halted.\n");
		while(1); /* Forever */
	}
	else {
		if (howto & RB_DUMP)
			dumpsys();
		printf("System restart.\n");
		delay(2000000);
		__asm__(" li $2, 0xbfc00000; jr $2; nop\n");
		while(1); /* Forever */
	}
	/*NOTREACHED*/
}

int	dumpmag = (int)0x8fca0101;	/* magic number for savecore */
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
	int error;

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
	switch (error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev)) {

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
		printf("error %d\n", error);
		break;

	case 0:
		printf("succeeded\n");
	}
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  Unfortunately, we can't read the hardware registers.
 * We guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splclock();
	static struct timeval lasttime;

	*tvp = time;
#ifdef notdef
	tvp->tv_usec += clkread();
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
#endif
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

initcpu()
{

}

/*
 * Convert an ASCII string into an integer.
 */
int
atoi(s)
	char *s;
{
	int c;
	unsigned base = 10, d;
	int neg = 0, val = 0;

	if (s == 0 || (c = *s++) == 0)
		goto out;

	/* skip spaces if any */
	while (c == ' ' || c == '\t')
		c = *s++;

	/* parse sign, allow more than one (compat) */
	while (c == '-') {
		neg = !neg;
		c = *s++;
	}

	/* parse base specification, if any */
	if (c == '0') {
		c = *s++;
		switch (c) {
		case 'X':
		case 'x':
			base = 16;
			c = *s++;
			break;
		case 'B':
		case 'b':
			base = 2;
			c = *s++;
			break;
		default:
			base = 8;
		}
	}

	/* parse number proper */
	for (;;) {
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'z')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			d = c - 'A' + 10;
		else
			break;
		val *= base;
		val += d;
		c = *s++;
	}
	if (neg)
		val = -val;
out:
	return val;	
}
