/*	$OpenBSD: hpux_machdep.c,v 1.13 2002/07/20 19:24:55 art Exp $	*/
/*	$NetBSD: hpux_machdep.c,v 1.19 1998/02/16 20:58:30 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Machinde-dependent bits for HP-UX binary compatibility.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/poll.h> 
#include <sys/proc.h> 
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/wait.h> 

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/vmparam.h>

#include <uvm/uvm_extern.h>

#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hpux_syscall.h>
#include <compat/hpux/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>

extern	short exframesize[];

struct valtostr {
	int	val;
	const char *str;
};

/*
 * 6.0 and later context.
 * XXX what are the HP-UX "localroot" semantics?  Should we handle
 * XXX diskless systems here?
 */
static const char context_040[] =
    "standalone HP-MC68040 HP-MC68881 HP-MC68020 HP-MC68010 localroot default";

static const char context_fpu[] =
    "standalone HP-MC68881 HP-MC68020 HP-MC68010 localroot default";

static const char context_nofpu[] =
    "standalone HP-MC68020 HP-MC68010 localroot default";

static struct valtostr context_table[] = {
	{ FPU_68060,	&context_040[0] },
	{ FPU_68040,	&context_040[0] },
	{ FPU_68882,	&context_fpu[0] },
	{ FPU_68881,	&context_fpu[0] },
	{ 0, NULL },
};

#define UOFF(f)		((int)&((struct user *)0)->f)
#define HPUOFF(f)	((int)&((struct hpux_user *)0)->f)

/* simplified FP structure */
struct bsdfp {
	int save[54];
	int reg[24];
	int ctrl[3];
};

/*
 * m68k-specific setup for HP-UX executables.
 */
int
hpux_cpu_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	/* struct hpux_exec *hpux_ep = epp->ep_hdr; */

	/* set up command for exec header */
	NEW_VMCMD(&epp->ep_vmcmds, hpux_cpu_vmcmd,
	    sizeof(struct hpux_exec), (long)epp->ep_hdr, NULLVP, 0, 0);
	return (0);
}

/*
 * We need to stash the exec header in the pcb, so we define
 * this vmcmd to do it for us, since vmcmds are executed once
 * we're committed to the exec (i.e. the old program has been unmapped).
 *
 * The address of the header is in ev->ev_addr and the length is
 * in ev->ev_len.
 */
int
hpux_cpu_vmcmd(p, ev)
	struct proc *p;
	struct exec_vmcmd *ev;
{
	struct hpux_exec *execp = (struct hpux_exec *)ev->ev_addr;

	/* Make sure we have room. */
	if (ev->ev_len <= sizeof(p->p_addr->u_md.md_exec))
		bcopy((caddr_t)ev->ev_addr, p->p_addr->u_md.md_exec,
		    ev->ev_len);

	/* Deal with misc. HP-UX process attributes. */
	if (execp->ha_trsize & HPUXM_VALID) {
		if (execp->ha_trsize & HPUXM_DATAWT)
			p->p_md.md_flags &= ~MDP_CCBDATA;

		if (execp->ha_trsize & HPUXM_STKWT)
			p->p_md.md_flags &= ~MDP_CCBSTACK;
	}

	return (0);
}

/*
 * Return arch-type for hpux_sys_sysconf()
 */
int
hpux_cpu_sysconf_arch()
{

	switch (cputype) {
	case CPU_68020:
		return (HPUX_SYSCONF_CPUM020);

	case CPU_68030:
		return (HPUX_SYSCONF_CPUM030);

	default:
		return (HPUX_SYSCONF_CPUM040);
	}
	/* NOTREACHED */
}

/*
 * HP-UX advise(2) system call.
 */
int
hpux_sys_advise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_advise_args *uap = v;
	int error = 0;

	switch (SCARG(uap, arg)) {
	case 0:
		p->p_md.md_flags |= MDP_HPUXMMAP; 
		break;

	case 1:
		ICIA();
		break;

	case 2:
		DCIA();
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * HP-UX getcontext(2) system call.
 * Man page lies, behaviour here is based on observed behaviour.
 */
int
hpux_sys_getcontext(p, v, retval)
	struct proc *p; 
	void *v;
	register_t *retval; 
{
	struct hpux_sys_getcontext_args *uap = v;
	const char *str;
	int l, i, error = 0;
	int len; 

	for (i = 0; context_table[i].str != NULL; i++)
		if (context_table[i].val == fputype)
			break;
	if (context_table[i].str == NULL)
		str = &context_nofpu[0];
	else
		str = context_table[i].str;

	/* + 1 ... count the terminating \0. */
	l = strlen(str) + 1;
	len = min(SCARG(uap, len), l);

	if (len)
		error = copyout(str, SCARG(uap, buf), len);
	if (error == 0)
		*retval = l;
	return (0);
}

/*
 * Brutal hack!  Map HP-UX u-area offsets into BSD k-stack offsets.
 * XXX This probably doesn't work anymore, BTW.  --thorpej
 */
int
hpux_to_bsd_uoff(off, isps, p)
	int *off, *isps; 
	struct proc *p;
{
	int *ar0 = p->p_md.md_regs;
	struct hpux_fp *hp; 
	struct bsdfp *bp;
	u_int raddr;

	*isps = 0;

	/* u_ar0 field; procxmt puts in U_ar0 */
	if ((int)off == HPUOFF(hpuxu_ar0))
		return(UOFF(U_ar0)); 

	if (fputype) {
		/* FP registers from PCB */
		hp = (struct hpux_fp *)HPUOFF(hpuxu_fp);
		bp = (struct bsdfp *)UOFF(u_pcb.pcb_fpregs);

		if (off >= hp->hpfp_ctrl && off < &hp->hpfp_ctrl[3])
			return((int)&bp->ctrl[off - hp->hpfp_ctrl]);

		if (off >= hp->hpfp_reg && off < &hp->hpfp_reg[24])
			return((int)&bp->reg[off - hp->hpfp_reg]);
	}

	/*
	 * Everything else we recognize comes from the kernel stack,
	 * so we convert off to an absolute address (if not already)
	 * for simplicity.
	 */
	if (off < (int *)ctob(UPAGES))
		off = (int *)((u_int)off + (u_int)p->p_addr);	/* XXX */

	/*
	 * General registers.
	 * We know that the HP-UX registers are in the same order as ours.
	 * The only difference is that their PS is 2 bytes instead of a
	 * padded 4 like ours throwing the alignment off.
	 */
	if (off >= ar0 && off < &ar0[18]) {
		/*
		 * PS: return low word and high word of PC as HP-UX would
		 * (e.g. &u.u_ar0[16.5]).
		 *
		 * XXX we don't do this since HP-UX adb doesn't rely on
		 * it and passing such an offset to procxmt will cause
		 * it to fail anyway.  Instead, we just set the offset
		 * to PS and let hpux_ptrace() shift up the value returned.
		 */
		if (off == &ar0[PS]) {
#if 0
			raddr = (u_int) &((short *)ar0)[PS*2+1];
#else
			raddr = (u_int) &ar0[(int)(off - ar0)];
#endif
			*isps = 1;
		}
		/*
		 * PC: off will be &u.u_ar0[16.5] since HP-UX saved PS
		 * is only 16 bits.
		 */
		else if (off == (int *)&(((short *)ar0)[PS*2+1]))
			raddr = (u_int) &ar0[PC];
		/*
		 * D0-D7, A0-A7: easy
		 */
		else
			raddr = (u_int) &ar0[(int)(off - ar0)];
		return((int)(raddr - (u_int)p->p_addr));	/* XXX */
	}

	/* everything else */
	return (-1);
}

#define	HSS_RTEFRAME	0x01
#define	HSS_FPSTATE	0x02
#define	HSS_USERREGS	0x04

struct hpuxsigstate {
	int	hss_flags;		/* which of the following are valid */
	struct	frame hss_frame;	/* original exception frame */
	struct	fpframe hss_fpstate;	/* 68881/68882 state info */
};

/*
 * WARNING: code in locore.s assumes the layout shown here for hsf_signum
 * thru hsf_handler so... don't screw with them!
 */
struct hpuxsigframe {
	int	hsf_signum;		   /* signo for handler */
	int	hsf_code;		   /* additional info for handler */
	struct	hpuxsigcontext *hsf_scp;   /* context ptr for handler */
	sig_t	hsf_handler;		   /* handler addr for u_sigc */
	struct	hpuxsigstate hsf_sigstate; /* state of the hardware */
	struct	hpuxsigcontext hsf_sc;	   /* actual context */
};

#ifdef DEBUG
int hpuxsigdebug = 0;
int hpuxsigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
/* ARGSUSED */
void
hpux_sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	struct proc *p = curproc;
	struct hpuxsigframe *kfp, *fp;
	struct frame *frame;
	struct sigacts *psp = p->p_sigacts;
	short ft;
	int oonstack, fsize;

	frame = (struct frame *)p->p_md.md_regs;
	ft = frame->f_format;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct hpuxsigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct hpuxsigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct hpuxsigframe *)(frame->f_regs[SP] - fsize);
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)uvm_grow(p, (unsigned)fp);

#ifdef DEBUG
	if ((hpuxsigdebug & SDB_KSTACK) && p->p_pid == hpuxsigpid)
		printf("hpux_sendsig(%d): sig %d ssp %p usp %p scp %p ft %d\n",
		       p->p_pid, sig, &oonstack, fp, &fp->hsf_sc, ft);
#endif

	if (uvm_useracc((caddr_t)fp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((hpuxsigdebug & SDB_KSTACK) && p->p_pid == hpuxsigpid)
			printf("hpux_sendsig(%d): useracc failed on sig %d\n",
			       p->p_pid, sig);
#endif
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
	kfp = (struct hpuxsigframe *)malloc((u_long)fsize, M_TEMP, M_WAITOK);

	/* 
	 * Build the argument list for the signal handler.
	 */
	kfp->hsf_signum = bsdtohpuxsig(sig);
	kfp->hsf_code = code;
	kfp->hsf_scp = &fp->hsf_sc;
	kfp->hsf_handler = catcher;

	/*
	 * Save necessary hardware state.  Currently this includes:
	 *	- general registers
	 *	- original exception frame (if not a "normal" frame)
	 *	- FP coprocessor state
	 */
	kfp->hsf_sigstate.hss_flags = HSS_USERREGS;
	bcopy((caddr_t)frame->f_regs,
	    (caddr_t)kfp->hsf_sigstate.hss_frame.f_regs, sizeof frame->f_regs);
	if (ft >= FMT7) {
#ifdef DEBUG
		if (ft > 15 || exframesize[ft] < 0)
			panic("hpux_sendsig: bogus frame type");
#endif
		kfp->hsf_sigstate.hss_flags |= HSS_RTEFRAME;
		kfp->hsf_sigstate.hss_frame.f_format = frame->f_format;
		kfp->hsf_sigstate.hss_frame.f_vector = frame->f_vector;
		bcopy((caddr_t)&frame->F_u,
		    (caddr_t)&kfp->hsf_sigstate.hss_frame.F_u, exframesize[ft]);

		/*
		 * Leave an indicator that we need to clean up the kernel
		 * stack.  We do this by setting the "pad word" above the
		 * hardware stack frame to the amount the stack must be
		 * adjusted by.
		 *
		 * N.B. we increment rather than just set f_stackadj in
		 * case we are called from syscall when processing a
		 * sigreturn.  In that case, f_stackadj may be non-zero.
		 */
		frame->f_stackadj += exframesize[ft];
		frame->f_format = frame->f_vector = 0;
#ifdef DEBUG
		if (hpuxsigdebug & SDB_FOLLOW)
			printf("hpux_sendsig(%d): copy out %d of frame %d\n",
			       p->p_pid, exframesize[ft], ft);
#endif
	}
	if (fputype) {
		kfp->hsf_sigstate.hss_flags |= HSS_FPSTATE;
		m68881_save(&kfp->hsf_sigstate.hss_fpstate);
	}

#ifdef DEBUG
	if ((hpuxsigdebug & SDB_FPSTATE) &&
	    *(char *)&kfp->hsf_sigstate.hss_fpstate)
		printf("hpux_sendsig(%d): copy out FP state (%x) to %p\n",
		       p->p_pid, *(u_int *)&kfp->hsf_sigstate.hss_fpstate,
		       &kfp->hsf_sigstate.hss_fpstate);
#endif

	/*
	 * Build the signal context to be used by hpux_sigreturn.
	 */
	kfp->hsf_sc.hsc_syscall	= 0;		/* XXX */
	kfp->hsf_sc.hsc_action	= 0;		/* XXX */
	kfp->hsf_sc.hsc_pad1	= kfp->hsf_sc.hsc_pad2 = 0;
	kfp->hsf_sc.hsc_onstack	= oonstack;
	kfp->hsf_sc.hsc_mask	= mask;
	kfp->hsf_sc.hsc_sp	= frame->f_regs[SP];
	kfp->hsf_sc.hsc_ps	= frame->f_sr;
	kfp->hsf_sc.hsc_pc	= frame->f_pc;

	/* How amazingly convenient! */
	kfp->hsf_sc._hsc_pad	= 0;
	kfp->hsf_sc._hsc_ap	= (int)&fp->hsf_sigstate;

	(void) copyout((caddr_t)kfp, (caddr_t)fp, fsize);
	frame->f_regs[SP] = (int)fp;

#ifdef DEBUG
	if (hpuxsigdebug & SDB_FOLLOW) {
		printf(
		  "hpux_sendsig(%d): sig %d scp %p fp %p sc_sp %x sc_ap %x\n",
		   p->p_pid, sig, kfp->hsf_scp, fp,
		   kfp->hsf_sc.hsc_sp, kfp->hsf_sc._hsc_ap);
	}
#endif

	/*
	 * Signal trampoline code is at base of user stack.
	 */
	frame->f_pc = p->p_sigcode;
#ifdef DEBUG
	if ((hpuxsigdebug & SDB_KSTACK) && p->p_pid == hpuxsigpid)
		printf("hpux_sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
	free((caddr_t)kfp, M_TEMP);
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
hpux_sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_sigreturn_args /* {
		syscallarg(struct hpuxsigcontext *) sigcntxp;
	} */ *uap = v;
	struct hpuxsigcontext *scp;
	struct frame *frame;
	int rf;
	struct hpuxsigcontext tsigc;
	struct hpuxsigstate tstate;
	int flags;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (hpuxsigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif
	if ((int)scp & 1)
		return (EINVAL);

	/*
	 * Fetch and test the HP-UX context structure.
	 * We grab it all at once for speed.
	 */
	if (uvm_useracc((caddr_t)scp, sizeof (*scp), B_WRITE) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&tsigc, sizeof tsigc))
		return (EINVAL);
	scp = &tsigc;
	if ((scp->hsc_ps & (PSL_MBZ|PSL_IPL|PSL_S)) != 0)
		return (EINVAL);

	/*
	 * Restore the user supplied information
	 */
	if (scp->hsc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->hsc_mask &~ sigcantmask;
	frame = (struct frame *) p->p_md.md_regs;
	frame->f_regs[SP] = scp->hsc_sp;
	frame->f_pc = scp->hsc_pc;
	frame->f_sr = scp->hsc_ps;

	/*
	 * Grab a pointer to the hpuxsigstate.
	 * If zero, the user is probably doing a longjmp.
	 * (This will never happen, really, since HP-UX doesn't
	 * know/care about the state pointer.)
	 */
	if ((rf = scp->_hsc_ap) == 0)
		return (EJUSTRETURN);

	/*
	 * See if there is anything to do before we go to the
	 * expense of copying in close to 1/2K of data
	 */
	flags = fuword((caddr_t)rf);
#ifdef DEBUG
	if (hpuxsigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): sc_ap %x flags %x\n",
		       p->p_pid, rf, flags);
#endif
	/*
	 * fuword failed (bogus _hsc_ap value).
	 */
	if (flags == -1)
		return (EINVAL);
	if (flags == 0 || copyin((caddr_t)rf, (caddr_t)&tstate, sizeof tstate))
		return (EJUSTRETURN);
#ifdef DEBUG
	if ((hpuxsigdebug & SDB_KSTACK) && p->p_pid == hpuxsigpid)
		printf("sigreturn(%d): ssp %p usp %x scp %p ft %d\n",
		       p->p_pid, &flags, scp->hsc_sp, SCARG(uap, sigcntxp),
		       (flags & HSS_RTEFRAME) ? tstate.hss_frame.f_format : -1);
#endif
	/*
	 * Restore most of the users registers except for A6 and SP
	 * which were handled above.
	 */
	if (flags & HSS_USERREGS)
		bcopy((caddr_t)tstate.hss_frame.f_regs,
		    (caddr_t)frame->f_regs, sizeof(frame->f_regs)-2*NBPW);

	/*
	 * Restore long stack frames.  Note that we do not copy
	 * back the saved SR or PC, they were picked up above from
	 * the sigcontext structure.
	 */
	if (flags & HSS_RTEFRAME) {
		int sz;
		
		/* grab frame type and validate */
		sz = tstate.hss_frame.f_format;
		if (sz > 15 || (sz = exframesize[sz]) < 0)
			return (EINVAL);
		frame->f_stackadj -= sz;
		frame->f_format = tstate.hss_frame.f_format;
		frame->f_vector = tstate.hss_frame.f_vector;
		bcopy((caddr_t)&tstate.hss_frame.F_u,
		    (caddr_t)&frame->F_u, sz);
#ifdef DEBUG
		if (hpuxsigdebug & SDB_FOLLOW)
			printf("sigreturn(%d): copy in %d of frame type %d\n",
			       p->p_pid, sz, tstate.hss_frame.f_format);
#endif
	}

	/*
	 * Finally we restore the original FP context
	 */
	if (flags & HSS_FPSTATE)
		m68881_restore(&tstate.hss_fpstate);

#ifdef DEBUG
	if ((hpuxsigdebug & SDB_FPSTATE) && *(char *)&tstate.hss_fpstate)
		printf("sigreturn(%d): copied in FP state (%x) at %p\n",
		       p->p_pid, *(u_int *)&tstate.hss_fpstate,
		       &tstate.hss_fpstate);

	if ((hpuxsigdebug & SDB_FOLLOW) ||
	    ((hpuxsigdebug & SDB_KSTACK) && p->p_pid == hpuxsigpid))
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

/*
 * Set registers on exec.
 * XXX Should clear registers except sp, pc.
 */
void
hpux_setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	struct frame *frame = (struct frame *)p->p_md.md_regs;

	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[SP] = stack;
	frame->f_regs[A2] = (int)PS_STRINGS;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;
	if (fputype)
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);

	p->p_md.md_flags &= ~MDP_HPUXMMAP;
	frame->f_regs[A0] = 0;	/* not 68010 (bit 31), no FPA (30) */
	retval[0] = 0;		/* no float card */
	if (fputype)
		retval[1] = 1;	/* yes 68881 */
	else
		retval[1] = 0;	/* no 68881 */
}
