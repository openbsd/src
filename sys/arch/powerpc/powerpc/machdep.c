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

#include <net/netisr.h>

#include <machine/bat.h>
#include <machine/pmap.h>
#include <machine/powerpc.h>
#include <machine/trap.h>

/*
 * Global variables used here and there
 */
struct pcb *curpcb;
struct pmap *curpm;
struct proc *fpuproc;

extern struct user *proc0paddr;

struct bat battable[16];

int astpending;

char *bootpath;
char bootpathbuf[512];

/*
 * We use the page just above the interrupt vector as message buffer
 */
struct msgbuf *msgbufp = (struct msgbuf *)0x3000;
int msgbufmapped = 1;		/* message buffer is always mapped */

caddr_t allocsys __P((caddr_t));

static void fake_splx __P((int));
static void fake_irq_establish __P((int, int, void (*)(void *), void *));

struct machvec machine_interface = {
	fake_splx,
	fake_irq_establish,
};

int cold = 1;

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
	 * i386 port says, that this shouldn't be here,
	 * but I really think the console should be initialized
	 * as early as possible.
	 */
	consinit();

#ifdef	__notyet__		/* Needs some rethinking regarding real/virtual OFW */
	OF_set_callback(callback);
#endif
	/*
	 * Initialize BAT registers to unmapped to not generate
	 * overlapping mappings below.
	 */
	asm volatile ("mtibatu 0,%0" :: "r"(0));
	asm volatile ("mtibatu 1,%0" :: "r"(0));
	asm volatile ("mtibatu 2,%0" :: "r"(0));
	asm volatile ("mtibatu 3,%0" :: "r"(0));
	asm volatile ("mtdbatu 0,%0" :: "r"(0));
	asm volatile ("mtdbatu 1,%0" :: "r"(0));
	asm volatile ("mtdbatu 2,%0" :: "r"(0));
	asm volatile ("mtdbatu 3,%0" :: "r"(0));
	
	/*
	 * Set up initial BAT table to only map the lowest 256 MB area
	 */
	battable[0].batl = BATL(0x00000000, BAT_M);
	battable[0].batu = BATU(0x00000000);

	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* IBAT0 used for initial 256 MB segment */
	asm volatile ("mtibatl 0,%0; mtibatu 0,%1"
		      :: "r"(battable[0].batl), "r"(battable[0].batu));
	/* DBAT0 used similar */
	asm volatile ("mtdbatl 0,%0; mtdbatu 0,%1"
		      :: "r"(battable[0].batl), "r"(battable[0].batu));
	
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
	asm volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0; isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

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
#endif

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);
}

/*
 * This should probably be in autoconf!				XXX
 */
int cpu;
char cpu_model[80];
char machine[] = "powerpc";	/* cpu architecture */

void
identifycpu()
{
	int phandle, pvr;
	char name[32];

	/*
	 * Find cpu type (Do it by OpenFirmware?)
	 */
	asm ("mfpvr %0" : "=r"(pvr));
	cpu = pvr >> 16;
	switch (cpu) {
	case 1:
		sprintf(cpu_model, "601");
		break;
	case 3:
		sprintf(cpu_model, "603");
		break;
	case 4:
		sprintf(cpu_model, "604");
		break;
	case 5:
		sprintf(cpu_model, "602");
		break;
	case 6:
		sprintf(cpu_model, "603e");
		break;
	case 7:
		sprintf(cpu_model, "603ev");
		break;
	case 9:
		sprintf(cpu_model, "604ev");
		break;
	case 20:
		sprintf(cpu_model, "620");
		break;
	default:
		sprintf(cpu_model, "Version %x", cpu);
		break;
	}
	sprintf(cpu_model + strlen(cpu_model), " (Revision %x)", pvr & 0xffff);
	printf("CPU: %s\n", cpu_model);
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
	asm volatile ("mfmsr %0; andi. %1, %0, %2; mtmsr %1"
		      : "=r"(omsr), "=r"(msr) : "K"((u_short)~PSL_EE));
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	syncicache((void *)&extint_call, sizeof extint_call);
	syncicache((void *)EXC_EXI, (int)&extsize);
	asm volatile ("mtmsr %0" :: "r"(omsr));
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
	identifycpu();
	
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
	 * Now allow hardware interrupts.
	 */
	{
		int msr;
		
		splhigh();
		asm volatile ("mfmsr %0; ori %0, %0, %1; mtmsr %0"
			      : "=r"(msr) : "K"(PSL_EE));
	}
	
	/*
	 * Configure devices.
	 */
	configure();
	
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

	/*
	 * Decide on buffer space to use.
	 */
	if (bufpages == 0)
		bufpages = (physmem / 20) / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
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
	static int initted;
	
	if (initted)
		return;
	initted = 1;
	cninit();
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
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oldonstack;
	
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
	
	frame.sf_code = code;
	
	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_onstack = oldonstack;
	frame.sf_sc.sc_mask = mask;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);
	
	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (int)code;
	tf->fixreg[5] = (int)&frame.sf_sc;
	tf->srr0 = (int)(((char *)PS_STRINGS)
			 - (p->p_emul->e_esigcode - p->p_emul->e_sigcode));
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

int cpl;
int clockpending, softclockpending, softnetpending;

/*
 * Soft networking interrupts.
 */
void
softnet()
{
	int isr = netisr;

	netisr = 0;
#ifdef	INET
#include "ether.h"
#if NETHER > 0
	if (isr & (1 << NETISR_ARP))
		arpintr();
#endif
	if (isr & (1 << NETISR_IP))
		ipintr();
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

/*
 * Stray interrupts.
 */
void
strayintr(irq)
	int irq;
{
	log(LOG_ERR, "stray interrupt %d\n", irq);
}

int
splraise(bits)
	int bits;
{
	int old;
	
	old = cpl;
	cpl |= bits;

	if ((bits & SPLMACHINE) & ~old)
		(*machine_interface.splx)(cpl & SPLMACHINE);

	return old;
}

int
splx(new)
	int new;
{
	int pending, old = cpl;
	int emsr, dmsr;
	
	asm ("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	
	cpl = new;
	
	if ((new & SPLMACHINE) != (old & SPLMACHINE))
		(*machine_interface.splx)(new & SPLMACHINE);

	while (1) {
		cpl = new;
		
		asm volatile ("mtmsr %0" :: "r"(dmsr));
		if (clockpending && !(cpl & SPLCLOCK)) {
			struct clockframe frame;
			extern int intr_depth;
			
			cpl |= SPLCLOCK;
			clockpending--;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			
			/*
			 * Fake a clock interrupt frame
			 */
			frame.pri = new;
			frame.depth = intr_depth + 1;
			frame.srr1 = 0;
			frame.srr0 = (int)splx;
			/*
			 * Do standard timer interrupt stuff
			 */
			hardclock(&frame);
			continue;
		}
		if (softclockpending && !(cpl & SPLSOFTCLOCK)) {
			
			cpl |= SPLSOFTCLOCK;
			softclockpending = 0;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			
			softclock();
			continue;
		}
		if (softnetpending && !(cpl & SPLSOFTNET)) {
			cpl |= SPLSOFTNET;
			softnetpending = 0;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			softnet();
			continue;
		}
		
		asm volatile ("mtmsr %0" :: "r"(emsr));
		
		return old;
	}
}

/*
 * This one is similar to the above, but returns with interrupts disabled.
 * It is intended for use during interrupt exit (as the name implies :-)).
 */
void
intr_return(level)
	int level;
{
	int pending, old = cpl;
	int emsr, dmsr;
	
	asm ("mfmsr %0" : "=r"(emsr));
	dmsr = emsr & ~PSL_EE;
	
	cpl = level;
	
	if ((level & SPLMACHINE) != (old & SPLMACHINE))
		(*machine_interface.splx)(level & SPLMACHINE);

	while (1) {
		cpl = level;
		
		asm volatile ("mtmsr %0" :: "r"(dmsr));
		if (clockpending && !(cpl & SPLCLOCK)) {
			struct clockframe frame;
			extern int intr_depth;
			
			cpl |= SPLCLOCK;
			clockpending--;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			
			/*
			 * Fake a clock interrupt frame
			 */
			frame.pri = level | (clockpending ? SPLSOFTCLOCK : 0);
			frame.depth = intr_depth + 1;
			frame.srr1 = 0;
			frame.srr0 = (int)splx;
			/*
			 * Do standard timer interrupt stuff
			 */
			hardclock(&frame);
			continue;
		}
		if (softclockpending && !(cpl & SPLSOFTCLOCK)) {
			
			cpl |= SPLSOFTCLOCK;
			softclockpending = 0;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			
			softclock();
			continue;
		}
		if (softnetpending && !(cpl & SPLSOFTNET)) {
			cpl |= SPLSOFTNET;
			softnetpending = 0;
			asm volatile ("mtmsr %0" :: "r"(emsr));
			softnet();
			continue;
		}
		break;
	}
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
		resettodr();		/* set wall clock */
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
 * OpenFirmware callback routine
 */
void
callback(p)
	void *p;
{
	panic("callback");	/* for now			XXX */
}

/*
 * Fake routines for spl/interrupt handling before autoconfig
 */
static void
fake_splx(new)
	int new;
{
}

static void
fake_irq_establish(irq, level, handler, arg)
	int irq, level;
	void (*handler) __P((void *));
	void *arg;
{
	panic("fake_irq_establish");
}
