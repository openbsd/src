/*	$OpenBSD: machdep.c,v 1.44 2014/11/16 12:30:58 deraadt Exp $	*/
/*	OpenBSD: machdep.c,v 1.105 2005/04/11 15:13:01 deraadt Exp 	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)machdep.c	8.6 (Berkeley) 1/14/94
 */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/extent.h>

#include <net/if.h>
#include <uvm/uvm.h>

#include <dev/rndvar.h>

#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/bsd_openprom.h>

#include <machine/idt.h>
#include <machine/kap.h>
#include <machine/prom.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/cpuvar.h>

#include "auxreg.h"

#include <sparc/sparc/intreg.h>

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

int	physmem;

/* sysctl settable */
int	sparc_led_blink = 1;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

/*
 * dvmamap is used to manage DVMA memory. Note: this coincides with
 * the memory range in `phys_map' (which is mostly a place-holder).
 */
vaddr_t dvma_base, dvma_end;
struct extent *dvmamap_extent;

void	dumpsys(void);
static int kap_maskcheck(void);

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vaddr_t minaddr, maxaddr;
	extern struct user *proc0paddr;

#ifdef DEBUG
	pmapdebug = 0;
#endif

	/*
	 * fix message buffer mapping
	 */
	pmap_map(MSGBUF_VA, MSGBUF_PA, MSGBUF_PA + MSGBUFSIZE,
	    PROT_READ | PROT_WRITE);
	initmsgbuf((caddr_t)(MSGBUF_VA + (CPU_ISSUN4 ? 4096 : 0)), MSGBUFSIZE);

	proc0.p_addr = proc0paddr;

	/* I would print this earlier, but I want it in the message buffer */
	if (kap_maskcheck() == 0) {
		printf("WARNING: KAP M2C3 or earlier mask detected.\n"
"THE PROCESSOR IN THIS MACHINE SUFFERS FROM SEVERE HARDWARE ISSUES.\n"
"M2C3 PROCESSORS MAY RUN RELIABLY ENOUGH, OLDER WILL DEFINITELY NOT.\n\n");
	}

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	/*identifycpu();*/
	printf("real mem = %d (%dMB)\n", ptoa(physmem),
	    ptoa(physmem) / 1024 / 1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a map for physio.  Others use a submap of the kernel
	 * map, but we want one completely separate, even though it uses
	 * the same pmap.
	 */
	dvma_base = CPU_ISSUN4M ? DVMA4M_BASE : DVMA_BASE;
	dvma_end = CPU_ISSUN4M ? DVMA4M_END : DVMA_END;
	phys_map = uvm_map_create(pmap_kernel(), dvma_base, dvma_end,
	    VM_MAP_INTRSAFE);
	if (phys_map == NULL)
		panic("unable to create DVMA map");

	/*
	 * Allocate DVMA space and dump into a privately managed
	 * extent for double mappings which is usable from
	 * interrupt contexts.
	 */
	if (uvm_km_valloc_wait(phys_map, (dvma_end-dvma_base)) != dvma_base)
		panic("unable to allocate from DVMA map");
	dvmamap_extent = extent_create("dvmamap", dvma_base, dvma_end,
				       M_DEVBUF, NULL, 0, EX_NOWAIT);
	if (dvmamap_extent == 0)
		panic("unable to allocate extent for dvma");

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024 / 1024);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/* Early interrupt handlers initialization */
	intr_init();
}

/*
 * Set up registers on exec.
 *
 * XXX this entire mess must be fixed
 */
/* ARGSUSED */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct trapframe *tf = p->p_md.md_tf;
	struct fpstate *fs;
	int psr;

	/*
	 * Setup the process StackGhost cookie which will be XORed into
	 * the return pointer as register windows are over/underflowed
	 */
	p->p_addr->u_pcb.pcb_wcookie = arc4random();

	/* The cookie needs to guarantee invalid alignment after the XOR */
	switch (p->p_addr->u_pcb.pcb_wcookie % 3) {
	case 0: /* Two lsb's already both set except if the cookie is 0 */
		p->p_addr->u_pcb.pcb_wcookie |= 0x3;
		break;
	case 1: /* Set the lsb */
		p->p_addr->u_pcb.pcb_wcookie = 1 |
			(p->p_addr->u_pcb.pcb_wcookie & ~0x3);
		break;
	case 2: /* Set the second most lsb */
		p->p_addr->u_pcb.pcb_wcookie = 2 |
			(p->p_addr->u_pcb.pcb_wcookie & ~0x3);
		break;
	}

	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/*
	 * The syscall will ``return'' to npc or %g7 or %g2; set them all.
	 * Set the rest of the registers to 0 except for %o6 (stack pointer,
	 * built in exec()) and psr (retain CWP and PSR_S bits).
	 */
	psr = tf->tf_psr & (PSR_S | PSR_CWP);
	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == cpuinfo.fpproc) {
			savefpstate(fs);
			cpuinfo.fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC, 0);
		p->p_md.md_fpstate = NULL;
	}
	bzero((caddr_t)tf, sizeof *tf);
	tf->tf_psr = psr;
	tf->tf_npc = pack->ep_entry & ~3;
	tf->tf_global[1] = (int)PS_STRINGS;
	tf->tf_global[2] = tf->tf_global[7] = tf->tf_npc;
	/* XXX exec of init(8) returns via proc_trampoline() */
	if (p->p_pid == 1) {
		tf->tf_pc = tf->tf_npc;
		tf->tf_npc += 4;
	}
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack;
	retval[1] = 0;
}

#ifdef DEBUG
int sigdebug = 0;
pid_t sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	siginfo_t *sf_sip;		/* points to siginfo_t */
	int	sf_xxx;			/* placeholder */
	caddr_t	sf_addr;		/* SunOS compat */
	struct	sigcontext sf_sc;	/* actual sigcontext */
	siginfo_t sf_si;
};

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
#if (NLED > 0) || (NAUXREG > 0) || (NSCF > 0)
	int oldval;
	int ret;
#endif
	extern int v8mul;

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	case CPU_LED_BLINK:
#if (NLED > 0) || (NAUXREG > 0) || (NSCF > 0)
		oldval = sparc_led_blink;
		ret = sysctl_int(oldp, oldlenp, newp, newlen,
		    &sparc_led_blink);

		/*
		 * If we were false and are now true, call led_blink().
		 * led_blink() itself will catch the other case.
		 */
		if (!oldval && sparc_led_blink > oldval) {
#if NAUXREG > 0
			led_blink((caddr_t *)0);
#endif
#if NLED > 0
			led_cycle((caddr_t *)led_sc);
#endif
#if NSCF > 0
			scfblink((caddr_t *)0);
#endif
		}

		return (ret);
#else
		return (EOPNOTSUPP);
#endif
	case CPU_CPUTYPE:
		return (sysctl_rdint(oldp, oldlenp, newp, cputyp));
	case CPU_V8MUL:
		return (sysctl_rdint(oldp, oldlenp, newp, v8mul));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

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
	struct proc *p = curproc;
	struct sigacts *psp = p->p_p->ps_sigacts;
	struct sigframe *fp;
	struct trapframe *tf;
	int caddr, oldsp, newsp;
	struct sigframe sf;

	tf = p->p_md.md_tf;
	oldsp = tf->tf_out[6];

	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 &&
	    !sigonstack(oldsp) && (psp->ps_sigonstack & sigmask(sig)))
		fp = (struct sigframe *)(p->p_sigstk.ss_sp +
					 p->p_sigstk.ss_size);
	else
		fp = (struct sigframe *)oldsp;
	fp = (struct sigframe *)((int)(fp - 1) & ~7);

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: %s[%d] sig %d newusp %p scp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc);
#endif
	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	bzero(&sf, sizeof(sf));
	sf.sf_signo = sig;
	sf.sf_sip = NULL;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_mask = mask;
	sf.sf_sc.sc_sp = oldsp;
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_psr = tf->tf_psr;
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];

	if (psp->ps_siginfo & sigmask(sig)) {
		sf.sf_sip = &fp->sf_si;
		initsiginfo(&sf.sf_si, sig, code, type, val);
	}

	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (int)fp - sizeof(struct rwindow);
	write_user_windows();
	/* XXX do not copyout siginfo if not needed */
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) ||
	    copyout(&oldsp, &((struct rwindow *)newsp)->rw_in[6],
	      sizeof(register_t)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
#endif
	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
	caddr = p->p_p->ps_sigcode;
	tf->tf_global[1] = (int)catcher;
	tf->tf_pc = caddr;
	tf->tf_npc = caddr + 4;
	tf->tf_out[6] = newsp;
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: about to return to catcher\n");
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
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
	struct sigcontext ksc;
	struct trapframe *tf;
	int error;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
#endif
	if ((error = copyin(SCARG(uap, sigcntxp), &ksc, sizeof(ksc))) != 0)
		return (error);
	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((ksc.sc_pc | ksc.sc_npc) & 3) != 0)
		return (EINVAL);
	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (ksc.sc_psr & PSR_ICC);
	tf->tf_pc = ksc.sc_pc;
	tf->tf_npc = ksc.sc_npc;
	tf->tf_global[1] = ksc.sc_g1;
	tf->tf_out[0] = ksc.sc_o0;
	tf->tf_out[6] = ksc.sc_sp;
	p->p_sigmask = ksc.sc_mask & ~sigcantmask;
	return (EJUSTRETURN);
}

int	waittime = -1;

__dead void
boot(int howto)
{
	int i;
	static char str[4];	/* room for "-sd\0" */

	if (cold) {
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	fb_unblank();
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();

		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	if_downall();

	uvm_shutdown();
	splhigh();
	cold = 1;

	if ((howto & RB_DUMP) != 0)
		dumpsys();

haltsys:
	doshutdownhooks();
	config_suspend_all(DVACT_POWERDOWN);

	if ((howto & RB_HALT) != 0 || (howto & RB_POWERDOWN) != 0) {
		printf("halted\n\n");
		romhalt();
	}

	printf("rebooting\n\n");
	i = 1;
	if ((howto & RB_SINGLE) != 0)
		str[i++] = 's';
	if ((howto & RB_KDB) != 0)
		str[i++] = 'd';
	if (i > 1) {
		str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	romboot(str);
	for (;;) ;
	/* NOTREACHED */
}

/* XXX - needs to be written */
void
dumpconf(void)
{
}

/*
 * Write a crash dump.
 */
void
dumpsys()
{
	printf("dump: TBD\n");
}

/*
 * Map an I/O device given physical address and size in bytes, e.g.,
 *
 *	mydev = (struct mydev *)mapdev(myioaddr, 0,
 *				       0, sizeof(struct mydev));
 *
 * See also machine/autoconf.h.
 *
 * XXXART - verify types (too tired now).
 */
void *
mapdev(phys, virt, offset, size)
	struct rom_reg *phys;
	int offset, virt, size;
{
	vaddr_t va;
	paddr_t pa;
	void *ret;
	static vaddr_t iobase;
	unsigned int pmtype;

	if (iobase == NULL)
		iobase = IODEV_BASE;

	size = round_page(size);
	if (size == 0)
		panic("mapdev: zero size");

	if (virt)
		va = trunc_page(virt);
	else {
		va = iobase;
		iobase += size;
		if (iobase > IODEV_END)	/* unlikely */
			panic("mapiodev");
	}
	ret = (void *)(va | (((u_long)phys->rr_paddr + offset) & PGOFSET));
			/* note: preserve page offset */

	pa = trunc_page((vaddr_t)phys->rr_paddr + offset);
	pmtype = PMAP_IOENC(phys->rr_iospace);

	do {
		pmap_kenter_pa(va, pa | pmtype | PMAP_NC,
		    PROT_READ | PROT_WRITE);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	pmap_update(pmap_kernel());
	return (ret);
}

/*
 * Soft interrupt handling
 */

int	kap_sir;

void
ienab_bis(int bis)
{
	int s;
	int mask = 1 << (bis - 1);
	u_int32_t icr;

	s = splhigh();
	if (kap_sir < mask) {
		/*
		 * We become the most important bit in kap_sir. Reprogram
		 * the GLU_ICR soft interrupt dispatcher.
		 */
		icr = lda(GLU_ICR, ASI_PHYS_IO) >> 24;
		icr = (icr & ~GICR_DISPATCH_MASK) | bis;
		sta(GLU_ICR, ASI_PHYS_IO, icr << 24);
	}
	kap_sir |= mask;
	splx(s);
}

/*
 * minimal console routines
 */

#include <sys/conf.h>
#include <dev/cons.h>

cons_decl(early);

struct consdev consdev_early = {
	earlycnprobe,
	earlycninit,
	earlycngetc,
	earlycnputc,
	nullcnpollc
};

struct consdev *cn_tab = &consdev_early;

void
earlycnprobe(struct consdev *cn)
{
	cn->cn_dev = makedev(0, 0);
	cn->cn_pri = CN_MIDPRI;
}

void
earlycninit(struct consdev *cn)
{
}

/* getc, putc in locore.s */

int	kapmask_m2c4;
static void kap_maskfault(void);

void
kap_maskfault()
{
	kapmask_m2c4 = 1;
}

/*
 * This routine checks whether we are running on a M2C4 or later mask, by
 * checking for M2C4 behaviour.
 *
 * After mapping a kernel text page with the ``byte-writeable shared''
 * memory attribute, we will attempt to execute code from the new mapping.
 *
 * On M2C4 and later masks, this will cause a text fault, supposedly for
 * us to be able to invalidate the instruction cache first, before resuming
 * execution; while M2C3 and earlier masks will not fault.
 *
 * Since OpenBSD does not use BWS pages and does explicit instruction cache
 * invalidation in ddb and the ptrace interface, this fault never happens in
 * real life. mem_access_fault() in trap.c knows this and will direct
 * execution to kap_maskfault(). Since the test code we have been invoking
 * is a simple empty function, kap_maskfault() will return here.
 *
 * XXX Find some way to identify M2C3, which _should_ run.
 */
int
kap_maskcheck()
{
	extern void masktest(void);
	void (*test)(void);

	pmap_enter(pmap_kernel(), TMPMAP_VA,
	    trunc_page((vaddr_t)masktest) | PMAP_BWS, PROT_READ, 0);
	test = (void (*)(void))(TMPMAP_VA + ((vaddr_t)masktest & PAGE_MASK));

	cpcb->pcb_onfault = (caddr_t)kap_maskfault;
	(*test)();
	cpcb->pcb_onfault = NULL;

	pmap_remove(pmap_kernel(), TMPMAP_VA, TMPMAP_VA + PAGE_SIZE);

	return (kapmask_m2c4);
}
