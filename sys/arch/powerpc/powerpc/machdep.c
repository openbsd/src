/*	$OpenBSD: machdep.c,v 1.20 1998/10/09 02:13:56 rahnds Exp $	*/
/*	$NetBSD: machdep.c,v 1.4 1996/10/16 19:33:11 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "machine/ipkdb.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/callout.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/map.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <net/netisr.h>

#include <machine/bat.h>
#include <machine/pmap.h>
#include <machine/powerpc.h>
#include <machine/trap.h>
#include <machine/autoconf.h>
#include <machine/pio.h>

#include <powerpc/pci/mpc106reg.h>
/*
 * Global variables used here and there
 */
struct pcb *curpcb;
struct pmap *curpm;
struct proc *fpuproc;

extern struct user *proc0paddr;
extern int cold;

struct bat battable[16];

int astpending;

#ifndef SYS_TYPE
/* XXX Hardwire it for now */
#define SYS_TYPE POWER4e
#endif

int system_type = SYS_TYPE;	/* XXX Hardwire it for now */

char ofw_eth_addr[6];		/* Save address of first network ifc found */
char *bootpath;
char bootpathbuf[512];


/*
 * We use the page just above the interrupt vector as message buffer
 */
struct msgbuf *msgbufp = (struct msgbuf *)0x3000;
int msgbufmapped = 1;		/* message buffer is always mapped */

caddr_t allocsys __P((caddr_t));
int power4e_get_eth_addr __P((void));


void
initppc(startkernel, endkernel, args)
	u_int startkernel, endkernel;
	char *args;
{
	int phandle, qhandle;
	char name[32];
	struct machvec *mp;
	extern trapcode, trapsize;
	extern dsitrap, dsisize;
	extern isitrap, isisize;
	extern decrint, decrsize;
	extern tlbimiss, tlbimsize;
	extern tlbdlmiss, tlbdlmsize;
	extern tlbdsmiss, tlbdsmsize;
#if NIPKDB > 0
	extern ipkdblow, ipkdbsize;
#endif
	extern void consinit __P((void));
	extern void callback __P((void *));
	int exc, scratch;


	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);
	
	curpcb = &proc0paddr->u_pcb;
	
	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();


	/*
	 * Initialize BAT registers to unmapped to not generate
	 * overlapping mappings below.
	 */
	__asm__ volatile ("mtibatu 0,%0" :: "r"(0));
	__asm__ volatile ("mtibatu 1,%0" :: "r"(0));
	__asm__ volatile ("mtibatu 2,%0" :: "r"(0));
	__asm__ volatile ("mtibatu 3,%0" :: "r"(0));
	__asm__ volatile ("mtdbatu 0,%0" :: "r"(0));
	__asm__ volatile ("mtdbatu 1,%0" :: "r"(0));
	__asm__ volatile ("mtdbatu 2,%0" :: "r"(0));
	__asm__ volatile ("mtdbatu 3,%0" :: "r"(0));
	
	/*
	 * Set up initial BAT table to only map the lowest 256 MB area
	 */
	battable[0].batl = BATL(0x00000000, BAT_M);
	battable[0].batu = BATU(0x00000000);

	if(system_type == POWER4e) {
		/* Map ISA I/O */
		addbatmap(MPC106_V_ISA_IO_SPACE, MPC106_P_ISA_IO_SPACE, BAT_I);
		battable[1].batl = BATL(0xbfffe000, BAT_I);
		battable[1].batu = BATU(0xbfffe000);
	}

	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* IBAT0 used for initial 256 MB segment */
	__asm__ volatile ("mtibatl 0,%0; mtibatu 0,%1"
		      :: "r"(battable[0].batl), "r"(battable[0].batu));
	/* DBAT0 used similar */
	__asm__ volatile ("mtdbatl 0,%0; mtdbatu 0,%1"
		      :: "r"(battable[0].batl), "r"(battable[0].batu));

#if 1
	__asm__ volatile ("mtdbatl 1,%0; mtdbatu 1,%1"
		      :: "r"(battable[1].batl), "r"(battable[1].batu));
#endif
	
	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100)
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			/*
			 * This one is (potentially) installed during autoconf
			 */
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)EXC_DSI, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)EXC_ISI, (size_t)&isisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)EXC_IMISS, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)EXC_DLMISS, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)EXC_DSMISS, (size_t)&tlbdsmsize);
			break;
#if NIPKDB > 0
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
			bcopy(&ipkdblow, (void *)exc, (size_t)&ipkdbsize);
			break;
#endif
		}

	syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	__asm__ volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0; isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

	ofwconprobe();

	/*
	 * Now we can set up the console as mapping is enabled.
         */
	consinit();

	/*                                                              
	 * Look at arguments passed to us and compute boothowto.      
	 * Default to SINGLE and ASKNAME if no args or
	 * SINGLE and DFLTROOT if this is a ramdisk kernel.                     
	 */                                                               
#ifdef RAMDISK_HOOKS                                         
	boothowto = RB_SINGLE | RB_DFLTROOT;
#else
	boothowto = RB_AUTOBOOT;
#endif /* RAMDISK_HOOKS */

	/*
	 * Parse arg string.
	 */

	/* make a copy of the args! */
	strncpy(bootpathbuf, args, 512);
	bootpath= &bootpathbuf[0];
	args = bootpath;
	while ( *++args && *args != ' ');
	if (*args) {
		*args++ = 0;
		while (*args) {
			switch (*args++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			}
		}
	}			

#if NIPKDB > 0
	/*
	 * Now trap to IPKDB
	 */
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#else
#ifdef DDB
	if (boothowto & RB_KDB)
		Debugger();
#endif
#endif

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);

	/*
	 * Figure out ethernet address.
	 */
	(void)power4e_get_eth_addr();
}


void
install_extint(handler)
	void (*handler) __P((void));
{
	extern extint, extsize;
	extern u_long extint_call;
	u_long offset = (u_long)handler - (u_long)&extint_call;
	int omsr, msr;
	
#ifdef	DIAGNOSTIC
	if (offset > 0x1ffffff)
		panic("install_extint: too far away");
#endif
	__asm__ volatile ("mfmsr %0; andi. %1, %0, %2; mtmsr %1"
		      : "=r"(omsr), "=r"(msr) : "K"((u_short)~PSL_EE));
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	syncicache((void *)&extint_call, sizeof extint_call);
	syncicache((void *)EXC_EXI, (int)&extsize);
	__asm__ volatile ("mtmsr %0" :: "r"(omsr));
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	int sz, i;
	caddr_t v;
	vm_offset_t minaddr, maxaddr;
	int base, residual;
	
	proc0.p_addr = proc0paddr;
	v = (caddr_t)proc0paddr + USPACE;

	printf("%s", version);
	
	printf("real mem = %d\n", ctob(physmem));
	
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
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	sz = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr, sz, TRUE);
	buffers = (char *)minaddr;
	if (vm_map_find(buffer_map, vm_object_allocate(sz), (vm_offset_t)0,
			&minaddr, sz, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* Don't want to alloc more physical mem than ever needed */
		base = MAXBSIZE;
		residual = 0;
	}
	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;
		
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);
	
	/*
	 * Allocate mbuf pool.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS + CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS + CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
	
	/*
	 * Initialize callouts.
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i - 1].c_next = &callout[i];
	
	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
	       nbuf, bufpages * CLBYTES);
	
	/*
	 * Set up the buffers.
	 */
	bufinit();

	/*
	 * Configure devices.
	 */
	configure();
	
	/*
	 * Now allow hardware interrupts.
	 */
	{
		int msr;
		
		splhigh();
		__asm__ volatile ("mfmsr %0; ori %0, %0, %1; mtmsr %0"
			      : "=r"(msr) : "K"(PSL_EE));
	}
}
	

/*
 * Allocate space for system data structures.
 */
caddr_t
allocsys(v)
	caddr_t v;
{
#define	valloc(name, type, num) \
	v = (caddr_t)(((name) = (type *)v) + (num))

	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
#ifdef	SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef	SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef	SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif
	/*
	 * Decide on buffer space to use.
	 */
	if (bufpages == 0)
		bufpages = (physmem / ((100/BUFCACHEPERCENT) / CLSIZE));
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES)
		bufpages = nbuf * MAXBSIZE / CLBYTES;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;
		if (nswbuf > 256)
			nswbuf = 256;
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	
	return v;
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	static int cons_initted = 0;

	if (cons_initted)
		return;
	cninit();
	cons_initted = 1;
}

/*
 * Clear registers on exec
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	u_int32_t newstack;
	u_int32_t pargs;
	u_int32_t       args[4];

	struct trapframe *tf = trapframe(p);
	pargs = -roundup(-stack + 8, 16);
	newstack = (u_int32_t)(pargs - 32);

	copyin ((void*)(VM_MAX_ADDRESS-0x10), &args, 0x10);
	
	bzero(tf, sizeof *tf);
	tf->fixreg[1] = newstack;
	tf->fixreg[3] = retval[0] = args[1];	/* XXX */
	tf->fixreg[4] = retval[1] = args[0];	/* XXX */
	tf->fixreg[5] = args[2];		/* XXX */
	tf->fixreg[6] = args[3];		/* XXX */
	tf->srr0 = pack->ep_entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

/*
 * Send a signal to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oldonstack;
	int pa;
	
	frame.sf_signum = sig;
	
	tf = trapframe(p);
	oldonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	
	/*
	 * Allocate stack space for signal handler.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK)
	    && !oldonstack
	    && (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp
					 + psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)tf->fixreg[1];
	fp = (struct sigframe *)((int)(fp - 1) & ~0xf);
	
	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_onstack = oldonstack;
	frame.sf_sc.sc_mask = mask;
	frame.sf_sip = NULL;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (psp->ps_siginfo & sigmask(sig)) {
		frame.sf_sip = &fp->sf_si;
		initsiginfo(&frame.sf_si, sig, code, type, val);
	}
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);
	

	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : NULL;
	tf->fixreg[5] = (int)&frame.sf_sc;
	tf->srr0 = (int)(((char *)PS_STRINGS)
			 - (p->p_emul->e_esigcode - p->p_emul->e_sigcode));

#if WHEN_WE_ONLY_FLUSH_DATA_WHEN_DOING_PMAP_ENTER
	pa = pmap_extract(vm_map_pmap(&p->p_vmspace->vm_map),tf->srr0);
	syncicache(pa, (p->p_emul->e_esigcode - p->p_emul->e_sigcode));
#endif
}

/*
 * System call to cleanup state after a signal handler returns.
 */
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc;
	struct trapframe *tf;
	int error;
	
	if (error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc))
		return error;
	tf = trapframe(p);
	if ((sc.sc_frame.srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC))
		return EINVAL;
	bcopy(&sc.sc_frame, tf, sizeof *tf);
	if (sc.sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = sc.sc_mask & ~sigcantmask;
	return EJUSTRETURN;
}

/*
 * Machine dependent system variables.
 * None for now.
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
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return ENOTDIR;
	switch (name[0]) {
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Crash dump handling.
 */
u_long dumpmag = 0x8fca0101;		/* magic number */
int dumpsize = 0;			/* size of dump in pages */
long dumplo = -1;			/* blocks */

void
dumpsys()
{
	printf("dumpsys: TBD\n");
}

/*
 * Soft networking interrupts.
 */
void
softnet(isr)
	int isr;
{
#ifdef	INET
#include "ether.h"
#if NETHER > 0
	if (isr & (1 << NETISR_ARP))
		arpintr();
#endif
	if (isr & (1 << NETISR_IP))
		ipintr();
#endif
#ifdef NETATALK
	if (isr & (1 << NETISR_ATALK))
		atintr();
#endif
#ifdef	IMP
	if (isr & (1 << NETISR_IMP))
		impintr();
#endif
#ifdef	NS
	if (isr & (1 << NETISR_NS))
		nsintr();
#endif
#ifdef	ISO
	if (isr & (1 << NETISR_ISO))
		clnlintr();
#endif
#ifdef	CCITT
	if (isr & (1 << NETISR_CCITT))
		ccittintr();
#endif
#include "ppp.h"
#if NPPP > 0
	if (isr & (1 << NETISR_PPP))
		pppintr();
#endif
}

void
lcsplx(ipl)
	int ipl;
{
	splx(ipl);
}

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
void
boot(howto)
	int howto;
#if 0
	char *what;
#endif
{
	static int syncing;
	static char str[256];
	char *ap = str, *ap1 = ap;

	boothowto = howto;
	if (!cold && !(howto & RB_NOSYNC) && !syncing) {
		syncing = 1;
		vfs_shutdown();		/* sync */
#if 0
		/* resettodr does not currently do anything, address
		 * this later
		 */
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
#endif
	}
	splhigh();
	if (howto & RB_HALT) {
		doshutdownhooks();
		printf("halted\n\n");
		ppc_exit();
	}
	if (!cold && (howto & RB_DUMP))
		dumpsys();
	doshutdownhooks();
	printf("rebooting\n\n");
#if 0
	if (what && *what) {
		if (strlen(what) > sizeof str - 5)
			printf("boot string too large, ignored\n");
		else {
			strcpy(str, what);
			ap1 = ap = str + strlen(str);
			*ap++ = ' ';
		}
	}
	*ap++ = '-';
	if (howto & RB_SINGLE)
		*ap++ = 's';
	if (howto & RB_KDB)
		*ap++ = 'd';
	*ap++ = 0;
	if (ap[-2] == '-')
		*ap1 = 0;
#endif
	ppc_boot(str);
}

/*
 *  Get Ethernet address for the onboard ethernet chip.
 */
int
power4e_get_eth_addr()
{
	int qhandle, phandle;
	char name[32];

	for (qhandle = OF_peer(0); qhandle; qhandle = phandle) {
		if (OF_getprop(qhandle, "device_type", name, sizeof name) >= 0
		    && !strcmp(name, "network")
		    && OF_getprop(qhandle, "local-mac-address",
				  &ofw_eth_addr, sizeof ofw_eth_addr) >= 0) {
			return(0);
		}
		if (phandle = OF_child(qhandle))
			continue;
		while (qhandle) {
			if (phandle = OF_peer(qhandle))
				break;
			qhandle = OF_parent(qhandle);
		}
	}
	return(-1);
}

typedef void  (void_f) (void);
void_f *pending_int_f = NULL;

/* call the bus/interrupt controller specific pending interrupt handler
 * would be nice if the offlevel interrupt code was handled here
 * instead of being in each of the specific handler code
 */
void
do_pending_int()
{
	if (pending_int_f != NULL) {
		(*pending_int_f)();
	}
}

/*
 * set system type from string
 */
void
systype(char *name)
{
	/* this table may be order specific if substrings match several
	 * computers but a longer string matches a specific 
	 */
	int i;
	struct systyp {
		char *name;
		char *systypename;
		int type;
	} systypes[] = {
		{ "MOT,",	"(PWRSTK) MCG powerstack family", PWRSTK },
		{ "V-I Power",	"(POWER4e) V-I ppc vme boards ",  POWER4e},
		{ NULL,"",0}
	};
	for (i = 0; systypes[i].name != NULL; i++) {
		if (strncmp( name , systypes[i].name,
			strlen (systypes[i].name)) == 0)
		{
			system_type = systypes[i].type;
			printf("recognized system type of %s as %s\n",
				name, systypes[i].systypename);
			break;
		}
	}
	if (system_type == OFWMACH) {
		printf("System type not recognized, good luck\n");
	}
}
/* 
 * one attempt at interrupt stuff..
 *
 */
#include <dev/pci/pcivar.h>
typedef void     *(intr_establish_t) __P((void *, pci_intr_handle_t,
            int, int (*func)(void *), void *, char *));
typedef void     (intr_disestablish_t) __P((void *, void *));

void *
ppc_intr_establish(lcv, ih, level, func, arg, name)
	void *lcv;
	pci_intr_handle_t ih;
	int level;
	int (*func) __P((void *));
	void *arg;
	char *name;
{
	panic("ppc_intr_establish called before interrupt controller configured: driver %s\n", name);
}

intr_establish_t *intr_establish_func = ppc_intr_establish;;
intr_disestablish_t *intr_disestablish_func;

void
ppc_intr_setup(intr_establish_t *establish, intr_disestablish_t *disestablish)
{
	intr_establish_func = establish;
	intr_disestablish_func = disestablish;
}

/*
 * General functions to enable and disable interrupts
 * without having inlined assembly code in many functions,
 * should be moved into a header file for inlining the function
 * so it is faster
 */
void
ppc_intr_enable(void)
{
	u_int32_t emsr, dmsr;
	__asm__ volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr | PSL_EE;
	__asm__ volatile("mtmsr %0" :: "r"(dmsr));
}

void
ppc_intr_disable(void)
{
	u_int32_t emsr, dmsr;
	__asm__ volatile("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	__asm__ volatile("mtmsr %0" :: "r"(dmsr));
}

/*
 * probably should be ppc_space_copy
 */

#define _CONCAT(A,B) A ## B
#define __C(A,B)	_CONCAT(A,B)

#define BUS_SPACE_COPY_N(BYTES,TYPE) 					\
void 									\
__C(bus_space_copy_,BYTES)(v, h1, o1, h2, o2, c)			\
	void *v;							\
	bus_space_handle_t h1, h2;					\
	bus_size_t o1, o2, c;						\
{									\
	TYPE val;							\
	TYPE *src, *dst;						\
	int i;								\
									\
	src = (TYPE *) (h1+o1);						\
	dst = (TYPE *) (h2+o2);						\
									\
	if (h1 == h2 && o2 > o1) {					\
		for (i = c; i > 0; i--) {				\
			dst[i] = src[i];				\
		}							\
	} else {							\
		for (i = 0; i < c; i++) {				\
			dst[i] = src[i];				\
		}							\
	}								\
}
BUS_SPACE_COPY_N(1,u_int8_t)
BUS_SPACE_COPY_N(2,u_int16_t)
BUS_SPACE_COPY_N(4,u_int32_t)

#define BUS_SPACE_SET_REGION_N(BYTES,TYPE)				\
void									\
__C(bus_space_set_region_,BYTES)(v, h, o, val, c)					\
{									\
	TYPE *src, *dst;						\
	int i;								\
									\
	dst = (TYPE *) (h+o);						\
	for (i = 0; i < c; i++) {					\
		dst[i] = val;						\
	}								\
}

BUS_SPACE_SET_REGION_N(1,u_int8_t)
BUS_SPACE_SET_REGION_N(2,u_int16_t)
BUS_SPACE_SET_REGION_N(4,u_int32_t)
