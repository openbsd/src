/*	$OpenBSD: kern_ktrace.c,v 1.70 2014/10/26 20:34:37 guenther Exp $	*/
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

void	ktrinitheaderraw(struct ktr_header *, uint, pid_t, pid_t);
void	ktrinitheader(struct ktr_header *, struct proc *, int);
void	ktrstart(struct proc *, struct vnode *, struct ucred *);
void	ktremulraw(struct proc *, struct process *, pid_t);
int	ktrops(struct proc *, struct process *, int, int, struct vnode *,
	    struct ucred *);
int	ktrsetchildren(struct proc *, struct process *, int, int,
	    struct vnode *, struct ucred *);
int	ktrwrite(struct proc *, struct ktr_header *, void *);
int	ktrwriteraw(struct proc *, struct vnode *, struct ucred *,
	    struct ktr_header *, void *);
int	ktrcanset(struct proc *, struct process *);

/*
 * Clear the trace settings in a correct way (to avoid races).
 */
void
ktrcleartrace(struct process *pr)
{
	struct vnode *vp;
	struct ucred *cred;

	if (pr->ps_tracevp != NULL) {
		vp = pr->ps_tracevp;
		cred = pr->ps_tracecred;

		pr->ps_traceflag = 0;
		pr->ps_tracevp = NULL;
		pr->ps_tracecred = NULL;

		vrele(vp);
		crfree(cred);
	}
}

/*
 * Change the trace setting in a correct way (to avoid races).
 */
void
ktrsettrace(struct process *pr, int facs, struct vnode *newvp,
    struct ucred *newcred)
{
	struct vnode *oldvp;
	struct ucred *oldcred;

	KASSERT(newvp != NULL);
	KASSERT(newcred != NULL);

	pr->ps_traceflag |= facs;

	/* nothing to change about where the trace goes? */
	if (pr->ps_tracevp == newvp && pr->ps_tracecred == newcred)
		return;	

	vref(newvp);
	crhold(newcred);

	oldvp = pr->ps_tracevp;
	oldcred = pr->ps_tracecred;

	pr->ps_tracevp = newvp;
	pr->ps_tracecred = newcred;

	if (oldvp != NULL) {
		vrele(oldvp);
		crfree(oldcred);
	}
}

void
ktrinitheaderraw(struct ktr_header *kth, uint type, pid_t pid, pid_t tid)
{
	memset(kth, 0, sizeof(struct ktr_header));
	kth->ktr_type = type;
	nanotime(&kth->ktr_time);
	kth->ktr_pid = pid;
	kth->ktr_tid = tid;
}

void
ktrinitheader(struct ktr_header *kth, struct proc *p, int type)
{
	ktrinitheaderraw(kth, type, p->p_p->ps_pid,
	    p->p_pid + THREAD_PID_OFFSET);
	bcopy(p->p_comm, kth->ktr_comm, MAXCOMLEN);
}

void
ktrstart(struct proc *p, struct vnode *vp, struct ucred *cred)
{
	struct ktr_header kth;

	ktrinitheaderraw(&kth, htobe32(KTR_START), -1, -1);
	ktrwriteraw(p, vp, cred, &kth, NULL);
}

void
ktrsyscall(struct proc *p, register_t code, size_t argsize, register_t args[])
{
	struct	ktr_header kth;
	struct	ktr_syscall *ktp;
	size_t len = sizeof(struct ktr_syscall) + argsize;
	register_t *argp;
	u_int nargs = 0;
	int i;

	if (code == SYS___sysctl && (p->p_p->ps_emul->e_flags & EMUL_NATIVE)) {
		/*
		 * The native sysctl encoding stores the mib[]
		 * array because it is interesting.
		 */
		if (args[1] > 0)
			nargs = lmin(args[1], CTL_MAXNAME);
		len += nargs * sizeof(int);
	}
	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_SYSCALL);
	ktp = malloc(len, M_TEMP, M_WAITOK);
	ktp->ktr_code = code;
	ktp->ktr_argsize = argsize;
	argp = (register_t *)((char *)ktp + sizeof(struct ktr_syscall));
	for (i = 0; i < (argsize / sizeof *argp); i++)
		*argp++ = args[i];
	if (nargs && copyin((void *)args[0], argp, nargs * sizeof(int)))
		memset(argp, 0, nargs * sizeof(int));
	kth.ktr_len = len;
	ktrwrite(p, &kth, ktp);
	free(ktp, M_TEMP, len);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrsysret(struct proc *p, register_t code, int error, register_t retval)
{
	struct ktr_header kth;
	struct ktr_sysret ktp;

	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_SYSRET);
	ktp.ktr_code = code;
	ktp.ktr_error = error;
	ktp.ktr_retval = error == 0 ? retval : 0;

	kth.ktr_len = sizeof(struct ktr_sysret);

	ktrwrite(p, &kth, &ktp);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrnamei(struct proc *p, char *path)
{
	struct ktr_header kth;

	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_NAMEI);
	kth.ktr_len = strlen(path);

	ktrwrite(p, &kth, path);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktremulraw(struct proc *curp, struct process *pr, pid_t tid)
{
	struct ktr_header kth;
	char *emul = pr->ps_emul->e_name;

	ktrinitheaderraw(&kth, KTR_EMUL, pr->ps_pid, tid);
	kth.ktr_len = strlen(emul);

	ktrwriteraw(curp, pr->ps_tracevp, pr->ps_tracecred, &kth, emul);
}

void
ktremul(struct proc *p)
{
	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktremulraw(p, p->p_p, p->p_pid + THREAD_PID_OFFSET);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrgenio(struct proc *p, int fd, enum uio_rw rw, struct iovec *iov,
    ssize_t len)
{
	struct ktr_header kth;
	struct ktr_genio *ktp;
	caddr_t cp;
	int count;
	int mlen, buflen;

	atomic_setbits_int(&p->p_flag, P_INKTR);

	/* beware overflow */
	if (len > PAGE_SIZE - sizeof(struct ktr_genio))
		buflen = PAGE_SIZE;
	else
		buflen = len + sizeof(struct ktr_genio);

	ktrinitheader(&kth, p, KTR_GENIO);
	mlen = buflen;
	ktp = malloc(mlen, M_TEMP, M_WAITOK);
	ktp->ktr_fd = fd;
	ktp->ktr_rw = rw;

	cp = (caddr_t)((char *)ktp + sizeof (struct ktr_genio));
	buflen -= sizeof(struct ktr_genio);

	while (len > 0) {
		/*
		 * Don't allow this process to hog the cpu when doing
		 * huge I/O.
		 */
		if (curcpu()->ci_schedstate.spc_schedflags & SPCF_SHOULDYIELD)
			preempt(NULL);

		count = lmin(iov->iov_len, buflen);
		if (count > len)
			count = len;
		if (copyin(iov->iov_base, cp, count))
			break;

		kth.ktr_len = count + sizeof(struct ktr_genio);

		if (ktrwrite(p, &kth, ktp) != 0)
			break;

		iov->iov_len -= count;
		iov->iov_base = (caddr_t)iov->iov_base + count;

		if (iov->iov_len == 0)
			iov++;

		len -= count;
	}

	free(ktp, M_TEMP, mlen);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrpsig(struct proc *p, int sig, sig_t action, int mask, int code,
    siginfo_t *si)
{
	struct ktr_header kth;
	struct ktr_psig kp;

	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_PSIG);
	kp.signo = (char)sig;
	kp.action = action;
	kp.mask = mask;
	kp.code = code;
	kp.si = *si;
	kth.ktr_len = sizeof(struct ktr_psig);

	ktrwrite(p, &kth, &kp);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrcsw(struct proc *p, int out, int user)
{
	struct ktr_header kth;
	struct	ktr_csw kc;

	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_CSW);
	kc.out = out;
	kc.user = user;
	kth.ktr_len = sizeof(struct ktr_csw);

	ktrwrite(p, &kth, &kc);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

void
ktrstruct(struct proc *p, const char *name, const void *data, size_t datalen)
{
	struct ktr_header kth;
	void *buf;
	size_t buflen;

	KERNEL_ASSERT_LOCKED();
	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_STRUCT);
	
	if (data == NULL)
		datalen = 0;
	buflen = strlen(name) + 1 + datalen;
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	strlcpy(buf, name, buflen);
	bcopy(data, buf + strlen(name) + 1, datalen);
	kth.ktr_len = buflen;

	ktrwrite(p, &kth, buf);
	free(buf, M_TEMP, buflen);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
}

int
ktruser(struct proc *p, const char *id, const void *addr, size_t len)
{
	struct ktr_header kth;
	struct ktr_user *ktp;
	int error;
	void *memp;
	size_t size;
#define	STK_PARAMS	128
	long long stkbuf[STK_PARAMS / sizeof(long long)];

	if (!KTRPOINT(p, KTR_USER))
		return (0);
	if (len > KTR_USER_MAXLEN)
		return EINVAL;

	atomic_setbits_int(&p->p_flag, P_INKTR);
	ktrinitheader(&kth, p, KTR_USER);
	size = sizeof(*ktp) + len;
	memp = NULL;
	if (size > sizeof(stkbuf)) {
		memp = malloc(sizeof(*ktp) + len, M_TEMP, M_WAITOK);
		ktp = (struct ktr_user *)memp;
	} else
		ktp = (struct ktr_user *)stkbuf;
	memset(ktp->ktr_id, 0, KTR_USER_MAXIDLEN);
	error = copyinstr(id, ktp->ktr_id, KTR_USER_MAXIDLEN, NULL);
	if (error)
	    goto out;

	error = copyin(addr, (void *)(ktp + 1), len);
	if (error)
		goto out;
	kth.ktr_len = sizeof(*ktp) + len;
	ktrwrite(p, &kth, ktp);
out:
	if (memp != NULL)
		free(memp, M_TEMP, sizeof(*ktp) + len);
	atomic_clearbits_int(&p->p_flag, P_INKTR);
	return (error);
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
	struct process *pr = NULL;
	struct ucred *cred = NULL;
	struct pgrp *pg;
	int facs = SCARG(uap, facs) & ~((unsigned) KTRFAC_ROOT);
	int ops = KTROP(SCARG(uap, ops));
	int descend = SCARG(uap, ops) & KTRFLAG_DESCEND;
	int ret = 0;
	int error = 0;
	struct nameidata nd;

	if (ops != KTROP_CLEAR) {
		/*
		 * an operation which requires a file argument.
		 */
		cred = curp->p_ucred;
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, fname),
		    curp);
		if ((error = vn_open(&nd, FREAD|FWRITE|O_NOFOLLOW, 0)) != 0)
			goto done;
		vp = nd.ni_vp;

		VOP_UNLOCK(vp, 0, curp);
		if (vp->v_type != VREG) {
			error = EACCES;
			goto done;
		}
	}
	/*
	 * Clear all uses of the tracefile
	 */
	if (ops == KTROP_CLEARFILE) {
		LIST_FOREACH(pr, &allprocess, ps_list) {
			if (pr->ps_tracevp == vp) {
				if (ktrcanset(curp, pr))
					ktrcleartrace(pr);
				else
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
	if (ops == KTROP_SET) {
		if (suser(curp, 0) == 0)
			facs |= KTRFAC_ROOT;
		ktrstart(curp, vp, cred);
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
		LIST_FOREACH(pr, &pg->pg_members, ps_pglist) {
			if (descend)
				ret |= ktrsetchildren(curp, pr, ops, facs, vp,
				    cred);
			else
				ret |= ktrops(curp, pr, ops, facs, vp, cred);
		}
	} else {
		/*
		 * by pid
		 */
		pr = prfind(SCARG(uap, pid));
		if (pr == NULL) {
			error = ESRCH;
			goto done;
		}
		if (descend)
			ret |= ktrsetchildren(curp, pr, ops, facs, vp, cred);
		else
			ret |= ktrops(curp, pr, ops, facs, vp, cred);
	}
	if (!ret)
		error = EPERM;
done:
	if (vp != NULL)
		(void) vn_close(vp, FREAD|FWRITE, cred, curp);
	return (error);
}

int
ktrops(struct proc *curp, struct process *pr, int ops, int facs,
    struct vnode *vp, struct ucred *cred)
{
	if (!ktrcanset(curp, pr))
		return (0);
	if (ops == KTROP_SET)
		ktrsettrace(pr, facs, vp, cred);
	else {	
		/* KTROP_CLEAR */
		pr->ps_traceflag &= ~facs;
		if ((pr->ps_traceflag & KTRFAC_MASK) == 0) {
			/* cleared all the facility bits, so stop completely */
			ktrcleartrace(pr);
		}
	}

	/*
	 * Emit an emulation record every time there is a ktrace
	 * change/attach request.
	 */
	if (pr->ps_traceflag & KTRFAC_EMUL)
		ktremulraw(curp, pr, -1);

	return (1);
}

int
ktrsetchildren(struct proc *curp, struct process *top, int ops, int facs,
    struct vnode *vp, struct ucred *cred)
{
	struct process *pr;
	int ret = 0;

	pr = top;
	for (;;) {
		ret |= ktrops(curp, pr, ops, facs, vp, cred);
		/*
		 * If this process has children, descend to them next,
		 * otherwise do any siblings, and if done with this level,
		 * follow back up the tree (but not past top).
		 */
		if (!LIST_EMPTY(&pr->ps_children))
			pr = LIST_FIRST(&pr->ps_children);
		else for (;;) {
			if (pr == top)
				return (ret);
			if (LIST_NEXT(pr, ps_sibling) != NULL) {
				pr = LIST_NEXT(pr, ps_sibling);
				break;
			}
			pr = pr->ps_pptr;
		}
	}
	/*NOTREACHED*/
}

int
ktrwrite(struct proc *p, struct ktr_header *kth, void *aux)
{
	struct vnode *vp = p->p_p->ps_tracevp;
	struct ucred *cred = p->p_p->ps_tracecred;
	int error;

	if (vp == NULL)
		return 0;
	crhold(cred);
	error = ktrwriteraw(p, vp, cred, kth, aux);
	crfree(cred);
	return (error);
}

int
ktrwriteraw(struct proc *curp, struct vnode *vp, struct ucred *cred,
    struct ktr_header *kth, void *aux)
{
	struct uio auio;
	struct iovec aiov[2];
	struct process *pr;
	int error;

	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)kth;
	aiov[0].iov_len = sizeof(struct ktr_header);
	auio.uio_resid = sizeof(struct ktr_header);
	auio.uio_iovcnt = 1;
	auio.uio_procp = curp;
	if (kth->ktr_len > 0) {
		auio.uio_iovcnt++;
		aiov[1].iov_base = aux;
		aiov[1].iov_len = kth->ktr_len;
		auio.uio_resid += kth->ktr_len;
	}
	vget(vp, LK_EXCLUSIVE | LK_RETRY, curp);
	error = VOP_WRITE(vp, &auio, IO_UNIT|IO_APPEND, cred);
	if (!error) {
		vput(vp);
		return (0);
	}
	/*
	 * If error encountered, give up tracing on this vnode.
	 */
	log(LOG_NOTICE, "ktrace write failed, errno %d, tracing stopped\n",
	    error);
	LIST_FOREACH(pr, &allprocess, ps_list)
		if (pr->ps_tracevp == vp && pr->ps_tracecred == cred)
			ktrcleartrace(pr);

	vput(vp);
	return (error);
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
ktrcanset(struct proc *callp, struct process *targetpr)
{
	struct ucred *caller = callp->p_ucred;
	struct ucred *target = targetpr->ps_ucred;

	if ((caller->cr_uid == target->cr_ruid &&
	    target->cr_ruid == target->cr_svuid &&
	    caller->cr_rgid == target->cr_rgid &&	/* XXX */
	    target->cr_rgid == target->cr_svgid &&
	    (targetpr->ps_traceflag & KTRFAC_ROOT) == 0 &&
	    !ISSET(targetpr->ps_flags, PS_SUGID)) ||
	    caller->cr_uid == 0)
		return (1);

	return (0);
}
