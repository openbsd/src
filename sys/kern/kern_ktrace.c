/*	$OpenBSD: kern_ktrace.c,v 1.38 2005/11/28 00:14:28 jsg Exp $	*/
/*	$NetBSD: kern_ktrace.c,v 1.23 1996/02/09 18:59:36 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)kern_ktrace.c	8.2 (Berkeley) 9/23/93
 */

#ifdef KTRACE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/ktrace.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

void ktrinitheader(struct ktr_header *, struct proc *, int);
int ktrops(struct proc *, struct proc *, int, int, struct vnode *);
int ktrsetchildren(struct proc *, struct proc *, int, int,
			struct vnode *);
int ktrwrite(struct proc *, struct ktr_header *);
int ktrcanset(struct proc *, struct proc *);

/*
 * Change the trace vnode in a correct way (to avoid races).
 */
void
ktrsettracevnode(struct proc *p, struct vnode *newvp)
{
	struct vnode *vp;

	if (p->p_tracep == newvp)	/* avoid work */
		return;

	if (newvp != NULL)
		VREF(newvp);

	vp = p->p_tracep;
	p->p_tracep = newvp;

	if (vp != NULL)
		vrele(vp);
}

void
ktrinitheader(struct ktr_header *kth, struct proc *p, int type)
{
	bzero(kth, sizeof (struct ktr_header));
	kth->ktr_type = type;
	microtime(&kth->ktr_time);
	kth->ktr_pid = p->p_pid;
	bcopy(p->p_comm, kth->ktr_comm, MAXCOMLEN);	
}

void
ktrsyscall(struct proc *p, register_t code, size_t argsize, register_t args[])
{
	struct	ktr_header kth;
	struct	ktr_syscall *ktp;
	unsigned int len = sizeof(struct ktr_syscall) + argsize;
	register_t *argp;
	u_int nargs = 0;
	int i;

	if (code == SYS___sysctl) {
		if (args[1] > 0)
			nargs = min(args[1], CTL_MAXNAME);
		len += nargs * sizeof(int);
	}
	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_SYSCALL);
	ktp = malloc(len, M_TEMP, M_WAITOK);
	ktp->ktr_code = code;
	ktp->ktr_argsize = argsize;
	argp = (register_t *)((char *)ktp + sizeof(struct ktr_syscall));
	for (i = 0; i < (argsize / sizeof *argp); i++)
		*argp++ = args[i];
	if (code == SYS___sysctl && nargs &&
	    copyin((void *)args[0], argp, nargs * sizeof(int)))
		bzero(argp, nargs * sizeof(int));
	kth.ktr_buf = (caddr_t)ktp;
	kth.ktr_len = len;
	ktrwrite(p, &kth);
	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrsysret(struct proc *p, register_t code, int error, register_t retval)
{
	struct ktr_header kth;
	struct ktr_sysret ktp;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_SYSRET);
	ktp.ktr_code = code;
	ktp.ktr_error = error;
	ktp.ktr_retval = retval;		/* what about val2 ? */

	kth.ktr_buf = (caddr_t)&ktp;
	kth.ktr_len = sizeof(struct ktr_sysret);

	ktrwrite(p, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrnamei(struct proc *p, char *path)
{
	struct ktr_header kth;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_NAMEI);
	kth.ktr_len = strlen(path);
	kth.ktr_buf = path;

	ktrwrite(p, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktremul(struct proc *p, char *emul)
{
	struct ktr_header kth;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_EMUL);
	kth.ktr_len = strlen(emul);
	kth.ktr_buf = emul;

	ktrwrite(p, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrgenio(struct proc *p, int fd, enum uio_rw rw, struct iovec *iov, int len,
    int error)
{
	struct ktr_header kth;
	struct ktr_genio *ktp;
	caddr_t cp;
	int resid = len, count;
	int buflen;

	if (error)
		return;

	p->p_traceflag |= KTRFAC_ACTIVE;

	buflen = min(PAGE_SIZE, len + sizeof(struct ktr_genio));

	ktrinitheader(&kth, p, KTR_GENIO);
	ktp = malloc(buflen, M_TEMP, M_WAITOK);
	ktp->ktr_fd = fd;
	ktp->ktr_rw = rw;

	kth.ktr_buf = (caddr_t)ktp;

	cp = (caddr_t)((char *)ktp + sizeof (struct ktr_genio));
	buflen -= sizeof(struct ktr_genio);

	while (resid > 0) {
		/*
		 * Don't allow this process to hog the cpu when doing
		 * huge I/O.
		 */
#ifdef __HAVE_CPUINFO
		if (curcpu()->ci_schedstate.spc_schedflags & SPCF_SHOULDYIELD)
#else
		if (p->p_schedflags & PSCHED_SHOULDYIELD)
#endif
			preempt(NULL);

		count = min(iov->iov_len, buflen);
		if (count > resid)
			count = resid;
		if (copyin(iov->iov_base, cp, count))
			break;

		kth.ktr_len = count + sizeof(struct ktr_genio);

		if (ktrwrite(p, &kth) != 0)
			break;

		iov->iov_len -= count;
		iov->iov_base = (caddr_t)iov->iov_base + count;

		if (iov->iov_len == 0)
			iov++;

		resid -= count;
	}

	free(ktp, M_TEMP);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
	
}

void
ktrpsig(struct proc *p, int sig, sig_t action, int mask, int code,
    siginfo_t *si)
{
	struct ktr_header kth;
	struct ktr_psig kp;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_PSIG);
	kp.signo = (char)sig;
	kp.action = action;
	kp.mask = mask;
	kp.code = code;
	kp.si = *si;
	kth.ktr_buf = (caddr_t)&kp;
	kth.ktr_len = sizeof (struct ktr_psig);

	ktrwrite(p, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

void
ktrcsw(struct proc *p, int out, int user)
{
	struct ktr_header kth;
	struct	ktr_csw kc;

	p->p_traceflag |= KTRFAC_ACTIVE;
	ktrinitheader(&kth, p, KTR_CSW);
	kc.out = out;
	kc.user = user;
	kth.ktr_buf = (caddr_t)&kc;
	kth.ktr_len = sizeof (struct ktr_csw);

	ktrwrite(p, &kth);
	p->p_traceflag &= ~KTRFAC_ACTIVE;
}

/* Interface and common routines */

/*
 * ktrace system call
 */
/* ARGSUSED */
int
sys_ktrace(struct proc *curp, void *v, register_t *retval)
{
	struct sys_ktrace_args /* {
		syscallarg(const char *) fname;
		syscallarg(int) ops;
		syscallarg(int) facs;
		syscallarg(pid_t) pid;
	} */ *uap = v;
	struct vnode *vp = NULL;
	struct proc *p = NULL;
	struct pgrp *pg;
	int facs = SCARG(uap, facs) & ~((unsigned) KTRFAC_ROOT);
	int ops = KTROP(SCARG(uap, ops));
	int descend = SCARG(uap, ops) & KTRFLAG_DESCEND;
	int ret = 0;
	int error = 0;
	struct nameidata nd;

	curp->p_traceflag |= KTRFAC_ACTIVE;
	if (ops != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, fname),
		    curp);
		if ((error = vn_open(&nd, FREAD|FWRITE|O_NOFOLLOW, 0)) != 0) {
			curp->p_traceflag &= ~KTRFAC_ACTIVE;
			return (error);
		}
		vp = nd.ni_vp;

		VOP_UNLOCK(vp, 0, curp);
		if (vp->v_type != VREG) {
			(void) vn_close(vp, FREAD|FWRITE, curp->p_ucred, curp);
			curp->p_traceflag &= ~KTRFAC_ACTIVE;
			return (EACCES);
		}
	}
	/*
	 * Clear all uses of the tracefile
	 */
	if (ops == KTROP_CLEARFILE) {
		for (p = LIST_FIRST(&allproc); p; p = LIST_NEXT(p, p_list)) {
			if (p->p_tracep == vp) {
				if (ktrcanset(curp, p)) {
					p->p_traceflag = 0;
					ktrsettracevnode(p, NULL);
				} else
					error = EPERM;
			}
		}
		goto done;
	}
	/*
	 * need something to (un)trace (XXX - why is this here?)
	 */
	if (!facs) {
		error = EINVAL;
		goto done;
	}
	/* 
	 * do it
	 */
	if (SCARG(uap, pid) < 0) {
		/*
		 * by process group
		 */
		pg = pgfind(-SCARG(uap, pid));
		if (pg == NULL) {
			error = ESRCH;
			goto done;
		}
		LIST_FOREACH(p, &pg->pg_members, p_pglist)
			if (descend)
				ret |= ktrsetchildren(curp, p, ops, facs, vp);
			else 
				ret |= ktrops(curp, p, ops, facs, vp);
					
	} else {
		/*
		 * by pid
		 */
		p = pfind(SCARG(uap, pid));
		if (p == NULL) {
			error = ESRCH;
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(curp, p, ops, facs, vp);
		else
			ret |= ktrops(curp, p, ops, facs, vp);
	}
	if (!ret)
		error = EPERM;
done:
	if (vp != NULL)
		(void) vn_close(vp, FWRITE, curp->p_ucred, curp);
	curp->p_traceflag &= ~KTRFAC_ACTIVE;
	return (error);
}

int
ktrops(struct proc *curp, struct proc *p, int ops, int facs, struct vnode *vp)
{

	if (!ktrcanset(curp, p))
		return (0);
	if (ops == KTROP_SET) {
		ktrsettracevnode(p, vp);
		p->p_traceflag |= facs;
		if (curp->p_ucred->cr_uid == 0)
			p->p_traceflag |= KTRFAC_ROOT;
	} else {	
		/* KTROP_CLEAR */
		if (((p->p_traceflag &= ~facs) & KTRFAC_MASK) == 0) {
			/* no more tracing */
			p->p_traceflag = 0;
			ktrsettracevnode(p, NULL);
		}
	}

	/*
	 * Emit an emulation record, every time there is a ktrace
	 * change/attach request. 
	 */
	if (KTRPOINT(p, KTR_EMUL))
		ktremul(p, p->p_emul->e_name);

	return (1);
}

int
ktrsetchildren(struct proc *curp, struct proc *top, int ops, int facs,
    struct vnode *vp)
{
	struct proc *p;
	int ret = 0;

	p = top;
	for (;;) {
		ret |= ktrops(curp, p, ops, facs, vp);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (!LIST_EMPTY(&p->p_children))
			p = LIST_FIRST(&p->p_children);
		else for (;;) {
			if (p == top)
				return (ret);
			if (LIST_NEXT(p, p_sibling) != NULL) {
				p = LIST_NEXT(p, p_sibling);
				break;
			}
			p = p->p_pptr;
		}
	}
	/*NOTREACHED*/
}

int
ktrwrite(struct proc *p, struct ktr_header *kth)
{
	struct uio auio;
	struct iovec aiov[2];
	int error;
	struct vnode *vp = p->p_tracep;

	if (vp == NULL)
		return 0;
	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_procp = p;
	if (kth->ktr_len > 0) {
		auio.uio_iovcnt++;
		aiov[1].iov_base = kth->ktr_buf;
		aiov[1].iov_len = kth->ktr_len;
		auio.uio_resid += kth->ktr_len;
	}
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	error = VOP_WRITE(vp, &auio, IO_UNIT|IO_APPEND, p->p_ucred);
	VOP_UNLOCK(vp, 0, p);
	if (!error)
		return 0;
	/*
	 * If error encountered, give up tracing on this vnode.
	 */
	log(LOG_NOTICE, "ktrace write failed, errno %d, tracing stopped\n",
	    error);
	for (p = LIST_FIRST(&allproc); p != NULL; p = LIST_NEXT(p, p_list)) {
		if (p->p_tracep == vp) {
			p->p_traceflag = 0;
			ktrsettracevnode(p, NULL);
		}
	}

	return error;
}

/*
 * Return true if caller has permission to set the ktracing state
 * of target.  Essentially, the target can't possess any
 * more permissions than the caller.  KTRFAC_ROOT signifies that
 * root previously set the tracing status on the target process, and 
 * so, only root may further change it.
 *
 * TODO: check groups.  use caller effective gid.
 */
int
ktrcanset(struct proc *callp, struct proc *targetp)
{
	struct pcred *caller = callp->p_cred;
	struct pcred *target = targetp->p_cred;

	if ((caller->pc_ucred->cr_uid == target->p_ruid &&
	    target->p_ruid == target->p_svuid &&
	    caller->p_rgid == target->p_rgid &&	/* XXX */
	    target->p_rgid == target->p_svgid &&
	    (targetp->p_traceflag & KTRFAC_ROOT) == 0 &&
	    !ISSET(targetp->p_flag, P_SUGID)) ||
	    caller->pc_ucred->cr_uid == 0)
		return (1);

	return (0);
}

#endif
