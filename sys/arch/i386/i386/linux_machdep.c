/*	$OpenBSD: linux_machdep.c,v 1.23 2002/07/20 19:24:56 art Exp $	*/
/*	$NetBSD: linux_machdep.c,v 1.29 1996/05/03 19:42:11 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>
#include <sys/filedesc.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_ioctl.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/vm86.h>
#include <machine/linux_machdep.h>

/*
 * To see whether wsdisplay is configured (for virtual console ioctl calls).
 */
#include "wsdisplay.h"
#include <sys/ioctl.h>
#if NWSDISPLAY > 0 && defined(WSDISPLAY_COMPAT_USL)
#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplay_usl_io.h>
#endif

#ifdef USER_LDT
#include <machine/cpu.h>
int linux_read_ldt(struct proc *, struct linux_sys_modify_ldt_args *,
    register_t *);
int linux_write_ldt(struct proc *, struct linux_sys_modify_ldt_args *,
    register_t *);
#endif

/*
 * Deal with some i386-specific things in the Linux emulation code.
 * This means just signals for now, will include stuff like
 * I/O map permissions and V86 mode sometime.
 */

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */

void
linux_sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct linux_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;

	tf = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct linux_sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct linux_sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct linux_sigframe *)tf->tf_esp - 1;
	}

	frame.sf_handler = catcher;
	frame.sf_sig = bsd_to_linux_sig[sig];

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_mask   = mask;
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		frame.sf_sc.sc_gs = tf->tf_vm86_gs;
		frame.sf_sc.sc_fs = tf->tf_vm86_fs;
		frame.sf_sc.sc_es = tf->tf_vm86_es;
		frame.sf_sc.sc_ds = tf->tf_vm86_ds;
		frame.sf_sc.sc_eflags = get_vflags(p);
	} else
#endif
	{
		__asm("movl %%gs,%w0" : "=r" (frame.sf_sc.sc_gs));
		__asm("movl %%fs,%w0" : "=r" (frame.sf_sc.sc_fs));
		frame.sf_sc.sc_es = tf->tf_es;
		frame.sf_sc.sc_ds = tf->tf_ds;
		frame.sf_sc.sc_eflags = tf->tf_eflags;
	}
	frame.sf_sc.sc_edi = tf->tf_edi;
	frame.sf_sc.sc_esi = tf->tf_esi;
	frame.sf_sc.sc_ebp = tf->tf_ebp;
	frame.sf_sc.sc_ebx = tf->tf_ebx;
	frame.sf_sc.sc_edx = tf->tf_edx;
	frame.sf_sc.sc_ecx = tf->tf_ecx;
	frame.sf_sc.sc_eax = tf->tf_eax;
	frame.sf_sc.sc_eip = tf->tf_eip;
	frame.sf_sc.sc_cs = tf->tf_cs;
	frame.sf_sc.sc_esp_at_signal = tf->tf_esp;
	frame.sf_sc.sc_ss = tf->tf_ss;
	frame.sf_sc.sc_err = tf->tf_err;
	frame.sf_sc.sc_trapno = tf->tf_trapno;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = p->p_sigcode;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);
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
int
linux_sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_sigreturn_args /* {
		syscallarg(struct linux_sigcontext *) scp;
	} */ *uap = v;
	struct linux_sigcontext *scp, context;
	struct trapframe *tf;

	tf = p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, scp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Restore signal context.
	 */
#ifdef VM86
	if (context.sc_eflags & PSL_VM) {
		tf->tf_vm86_gs = context.sc_gs;
		tf->tf_vm86_fs = context.sc_fs;
		tf->tf_vm86_es = context.sc_es;
		tf->tf_vm86_ds = context.sc_ds;
		set_vflags(p, context.sc_eflags);
	} else
#endif
	{
		/*
		 * Check for security violations.  If we're returning to
		 * protected mode, the CPU will validate the segment registers
		 * automatically and generate a trap on violations.  We handle
		 * the trap, rather than doing all of the checking here.
		 */
		if (((context.sc_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(context.sc_cs, context.sc_eflags))
			return (EINVAL);

		/* %fs and %gs were restored by the trampoline. */
		tf->tf_es = context.sc_es;
		tf->tf_ds = context.sc_ds;
		tf->tf_eflags = context.sc_eflags;
	}
	tf->tf_edi = context.sc_edi;
	tf->tf_esi = context.sc_esi;
	tf->tf_ebp = context.sc_ebp;
	tf->tf_ebx = context.sc_ebx;
	tf->tf_edx = context.sc_edx;
	tf->tf_ecx = context.sc_ecx;
	tf->tf_eax = context.sc_eax;
	tf->tf_eip = context.sc_eip;
	tf->tf_cs = context.sc_cs;
	tf->tf_esp = context.sc_esp_at_signal;
	tf->tf_ss = context.sc_ss;

	p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = context.sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
}

int
linux_sys_rt_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return(ENOSYS);
}

#ifdef USER_LDT

int
linux_read_ldt(p, uap, retval)
	struct proc *p;
	struct linux_sys_modify_ldt_args /* {
		syscallarg(int) func;
		syscallarg(void *) ptr;
		syscallarg(size_t) bytecount;
	} */ *uap;
	register_t *retval;
{
	struct i386_get_ldt_args gl;
	int error;
	caddr_t sg;
	char *parms;

	sg = stackgap_init(p->p_emul);

	gl.start = 0;
	gl.desc = SCARG(uap, ptr);
	gl.num = SCARG(uap, bytecount) / sizeof(union descriptor);

	parms = stackgap_alloc(&sg, sizeof(gl));

	if ((error = copyout(&gl, parms, sizeof(gl))) != 0)
		return (error);

	if ((error = i386_get_ldt(p, parms, retval)) != 0)
		return (error);

	*retval *= sizeof(union descriptor);
	return (0);
}

struct linux_ldt_info {
	u_int entry_number;
	u_long base_addr;
	u_int limit;
	u_int seg_32bit:1;
	u_int contents:2;
	u_int read_exec_only:1;
	u_int limit_in_pages:1;
	u_int seg_not_present:1;
};

int
linux_write_ldt(p, uap, retval)
	struct proc *p;
	struct linux_sys_modify_ldt_args /* {
		syscallarg(int) func;
		syscallarg(void *) ptr;
		syscallarg(size_t) bytecount;
	} */ *uap;
	register_t *retval;
{
	struct linux_ldt_info ldt_info;
	struct segment_descriptor sd;
	struct i386_set_ldt_args sl;
	int error;
	caddr_t sg;
	char *parms;

	if (SCARG(uap, bytecount) != sizeof(ldt_info))
		return (EINVAL);
	if ((error = copyin(SCARG(uap, ptr), &ldt_info, sizeof(ldt_info))) != 0)
		return error;
	if (ldt_info.contents == 3)
		return (EINVAL);

	sg = stackgap_init(p->p_emul);

	sd.sd_lobase = ldt_info.base_addr & 0xffffff;
	sd.sd_hibase = (ldt_info.base_addr >> 24) & 0xff;
	sd.sd_lolimit = ldt_info.limit & 0xffff;
	sd.sd_hilimit = (ldt_info.limit >> 16) & 0xf;
	sd.sd_type =
	    16 | (ldt_info.contents << 2) | (!ldt_info.read_exec_only << 1);
	sd.sd_dpl = SEL_UPL;
	sd.sd_p = !ldt_info.seg_not_present;
	sd.sd_def32 = ldt_info.seg_32bit;
	sd.sd_gran = ldt_info.limit_in_pages;

	sl.start = ldt_info.entry_number;
	sl.desc = stackgap_alloc(&sg, sizeof(sd));
	sl.num = 1;

#if 0
	printf("linux_write_ldt: idx=%d, base=%x, limit=%x\n",
	    ldt_info.entry_number, ldt_info.base_addr, ldt_info.limit);
#endif

	parms = stackgap_alloc(&sg, sizeof(sl));

	if ((error = copyout(&sd, sl.desc, sizeof(sd))) != 0)
		return (error);
	if ((error = copyout(&sl, parms, sizeof(sl))) != 0)
		return (error);

	if ((error = i386_set_ldt(p, parms, retval)) != 0)
		return (error);

	*retval = 0;
	return (0);
}

#endif /* USER_LDT */

int
linux_sys_modify_ldt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_modify_ldt_args /* {
		syscallarg(int) func;
		syscallarg(void *) ptr;
		syscallarg(size_t) bytecount;
	} */ *uap = v;

	switch (SCARG(uap, func)) {
#ifdef USER_LDT
	case 0:
		return (linux_read_ldt(p, uap, retval));

	case 1:
		return (linux_write_ldt(p, uap, retval));
#endif /* USER_LDT */

	default:
		return (ENOSYS);
	}
}

/*
 * XXX Pathetic hack to make svgalib work. This will fake the major
 * device number of an opened VT so that svgalib likes it. grmbl.
 * Should probably do it 'wrong the right way' and use a mapping
 * array for all major device numbers, and map linux_mknod too.
 */
dev_t
linux_fakedev(dev)
	dev_t dev;
{

	if (major(dev) == NATIVE_CONS_MAJOR)
		return makedev(LINUX_CONS_MAJOR, (minor(dev) + 1));
	return dev;
}

/*
 * We come here in a last attempt to satisfy a Linux ioctl() call
 */
int
linux_machdepioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct sys_ioctl_args bia;
	u_long com;
	int error;
#if (NWSDISPLAY > 0 && defined(WSDISPLAY_COMPAT_USL))
	struct vt_mode lvt;
	caddr_t bvtp, sg;
#endif
	struct filedesc *fdp;
	struct file *fp;
	int fd;
	int (*ioctlf)(struct file *, u_long, caddr_t, struct proc *);
	struct ioctl_pt pt;

	fd = SCARG(uap, fd);
	SCARG(&bia, fd) = SCARG(uap, fd);
	SCARG(&bia, data) = SCARG(uap, data);
	com = SCARG(uap, com);

	fdp = p->p_fd;
	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return (EBADF);

	switch (com) {
#if (NWSDISPLAY > 0 && defined(WSDISPLAY_COMPAT_USL))
	case LINUX_KDGKBMODE:
		com = KDGKBMODE;
		break;
	case LINUX_KDSKBMODE:
		com = KDSKBMODE;
		if ((unsigned)SCARG(uap, data) == LINUX_K_MEDIUMRAW)
			SCARG(&bia, data) = (caddr_t)K_RAW;
		break;
	case LINUX_KIOCSOUND:
		SCARG(&bia, data) =
			(caddr_t)(((unsigned long)SCARG(&bia, data)) & 0xffff);
		/* fall through */
	case LINUX_KDMKTONE:
		com = KDMKTONE;
		break;
	case LINUX_KDSETMODE:
		com = KDSETMODE;
		break;
	case LINUX_KDGETMODE:
#if NWSDISPLAY > 0 && defined(WSDISPLAY_COMPAT_USL)
		com = WSDISPLAYIO_GMODE;
#else
		com = KDGETMODE;
#endif
		break;
	case LINUX_KDENABIO:
		com = KDENABIO;
		break;
	case LINUX_KDDISABIO:
		com = KDDISABIO;
		break;
	case LINUX_KDGETLED:
		com = KDGETLED;
		break;
	case LINUX_KDSETLED:
		com = KDSETLED;
		break;
	case LINUX_VT_OPENQRY:
		com = VT_OPENQRY;
		break;
	case LINUX_VT_GETMODE: {
		int sig;

		SCARG(&bia, com) = VT_GETMODE;
		if ((error = sys_ioctl(p, &bia, retval)))
			return error;
		if ((error = copyin(SCARG(uap, data), (caddr_t)&lvt,
		    sizeof (struct vt_mode))))
			return error;
		/* We need to bounds check here in case there
		   is a race with another thread */
		if ((error = bsd_to_linux_signal(lvt.relsig, &sig)))
			return error;
		lvt.relsig = sig;

		if ((error = bsd_to_linux_signal(lvt.acqsig, &sig)))
			return error;
		lvt.acqsig = sig;
		
		if ((error = bsd_to_linux_signal(lvt.frsig, &sig)))
			return error;
		lvt.frsig = sig;

		return copyout((caddr_t)&lvt, SCARG(uap, data),
		    sizeof (struct vt_mode));
	}
	case LINUX_VT_SETMODE: {
		int sig;

		com = VT_SETMODE;
		if ((error = copyin(SCARG(uap, data), (caddr_t)&lvt,
		    sizeof (struct vt_mode))))
			return error;
		if ((error = linux_to_bsd_signal(lvt.relsig, &sig)))
			return error;
		lvt.relsig = sig;

		if ((error = linux_to_bsd_signal(lvt.acqsig, &sig)))
			return error;
		lvt.acqsig = sig;

		if ((error = linux_to_bsd_signal(lvt.frsig, &sig)))
			return error;
		lvt.frsig = sig;

		sg = stackgap_init(p->p_emul);
		bvtp = stackgap_alloc(&sg, sizeof (struct vt_mode));
		if ((error = copyout(&lvt, bvtp, sizeof (struct vt_mode))))
			return error;
		SCARG(&bia, data) = bvtp;
		break;
	}
	case LINUX_VT_DISALLOCATE:
		/* XXX should use WSDISPLAYIO_DELSCREEN */
		return 0;
	case LINUX_VT_RELDISP:
		com = VT_RELDISP;
		break;
	case LINUX_VT_ACTIVATE:
		com = VT_ACTIVATE;
		break;
	case LINUX_VT_WAITACTIVE:
		com = VT_WAITACTIVE;
		break;
	case LINUX_VT_GETSTATE:
		com = VT_GETSTATE;
		break;
	case LINUX_KDGKBTYPE:
		/* This is what Linux does */
		return (subyte(SCARG(uap, data), KB_101));
#endif
	default:
		/*
		 * Unknown to us. If it's on a device, just pass it through
		 * using PTIOCLINUX, the device itself might be able to
		 * make some sense of it.
		 * XXX hack: if the function returns EJUSTRETURN,
		 * it has stuffed a sysctl return value in pt.data.
		 */
		FREF(fp);
		ioctlf = fp->f_ops->fo_ioctl;
		pt.com = SCARG(uap, com);
		pt.data = SCARG(uap, data);
		error = ioctlf(fp, PTIOCLINUX, (caddr_t)&pt, p);
		FRELE(fp);
		if (error == EJUSTRETURN) {
			retval[0] = (register_t)pt.data;
			error = 0;
		}

		if (error == ENOTTY)
			printf("linux_machdepioctl: invalid ioctl %08lx\n",
			    com);
		return (error);
	}
	SCARG(&bia, com) = com;
	return sys_ioctl(p, &bia, retval);
}

/*
 * Set I/O permissions for a process. Just set the maximum level
 * right away (ignoring the argument), otherwise we would have
 * to rely on I/O permission maps, which are not implemented.
 */
int
linux_sys_iopl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
#if 0
	struct linux_sys_iopl_args /* {
		syscallarg(int) level;
	} */ *uap = v;
#endif
	struct trapframe *fp = p->p_md.md_regs;

	if (suser(p->p_ucred, &p->p_acflag) != 0)
		return EPERM;
	if (securelevel > 0)
		return EPERM;
	fp->tf_eflags |= PSL_IOPL;
	*retval = 0;
	return 0;
}

/*
 * See above. If a root process tries to set access to an I/O port,
 * just let it have the whole range.
 */
int
linux_sys_ioperm(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ioperm_args /* {
		syscallarg(unsigned int) lo;
		syscallarg(unsigned int) hi;
		syscallarg(int) val;
	} */ *uap = v;
	struct trapframe *fp = p->p_md.md_regs;

	if (suser(p->p_ucred, &p->p_acflag) != 0)
		return EPERM;
	if (securelevel > 0)
		return EPERM;
	if (SCARG(uap, val))
		fp->tf_eflags |= PSL_IOPL;
	*retval = 0;
	return 0;
}
