/*	$OpenBSD: sys_generic.c,v 1.14 1998/07/28 19:47:07 millert Exp $	*/
/*	$NetBSD: sys_generic.c,v 1.24 1996/03/29 00:25:32 cgd Exp $	*/

/*
 * Copyright (c) 1996 Theo de Raadt
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)sys_generic.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

int selscan __P((struct proc *, fd_set *, fd_set *, int, register_t *));
int seltrue __P((dev_t, int, struct proc *));
void pollscan __P((struct proc *, struct pollfd *, int, register_t *));

/*
 * Read system call.
 */
/* ARGSUSED */
int
sys_read(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_read_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((u_int)SCARG(uap, fd)) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);
	/* Don't allow nbyte to be larger than max return val */
	if (SCARG(uap, nbyte) > SSIZE_MAX)
		return(EINVAL);
	aiov.iov_base = (caddr_t)SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, nbyte);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = SCARG(uap, nbyte);
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif
	cnt = SCARG(uap, nbyte);
	error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	if (error)
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, SCARG(uap, fd), UIO_READ, &ktriov,
		    cnt, error);
#endif
	*retval = cnt;
	return (error);
}

/*
 * Scatter read system call.
 */
int
sys_readv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_readv_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(int) iovcnt;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	register struct iovec *iov;
	struct iovec *needfree;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((u_int)SCARG(uap, fd)) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL ||
	    (fp->f_flag & FREAD) == 0)
		return (EBADF);
	if (SCARG(uap, iovcnt) <= 0)
		return (EINVAL);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = SCARG(uap, iovcnt) * sizeof (struct iovec);
	if (SCARG(uap, iovcnt) > UIO_SMALLIOV) {
		if (SCARG(uap, iovcnt) > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		needfree = iov;
	} else {
		iov = aiov;
		needfree = NULL;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = SCARG(uap, iovcnt);
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	error = copyin((caddr_t)SCARG(uap, iovp), (caddr_t)iov, iovlen);
	if (error)
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < SCARG(uap, iovcnt); i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if ((ssize_t)(auio.uio_resid += iov->iov_len) <= 0) {
			error = EINVAL;
			goto done;
		}
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_read)(fp, &auio, fp->f_cred);
	if (error)
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, SCARG(uap, fd), UIO_READ, ktriov,
			    cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	*retval = cnt;
done:
	if (needfree)
		FREE(needfree, M_IOV);
	return (error);
}

/*
 * Write system call
 */
int
sys_write(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_write_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	struct iovec aiov;
	long cnt, error = 0;
#ifdef KTRACE
	struct iovec ktriov;
#endif

	if (((u_int)SCARG(uap, fd)) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);
	/* Don't allow nbyte to be larger than max return val */
	if (SCARG(uap, nbyte) > SSIZE_MAX)
		return(EINVAL);
	aiov.iov_base = (caddr_t)SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, nbyte);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = SCARG(uap, nbyte);
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))
		ktriov = aiov;
#endif
	cnt = SCARG(uap, nbyte);
	error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred);
	if (error) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO) && error == 0)
		ktrgenio(p->p_tracep, SCARG(uap, fd), UIO_WRITE,
		    &ktriov, cnt, error);
#endif
	*retval = cnt;
	return (error);
}

/*
 * Gather write system call
 */
int
sys_writev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_writev_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp = p->p_fd;
	struct uio auio;
	register struct iovec *iov;
	struct iovec *needfree;
	struct iovec aiov[UIO_SMALLIOV];
	long i, cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if (((u_int)SCARG(uap, fd)) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL ||
	    (fp->f_flag & FWRITE) == 0)
		return (EBADF);
	if (SCARG(uap, iovcnt) <= 0)
		return (EINVAL);
	/* note: can't use iovlen until iovcnt is validated */
	iovlen = SCARG(uap, iovcnt) * sizeof (struct iovec);
	if (SCARG(uap, iovcnt) > UIO_SMALLIOV) {
		if (SCARG(uap, iovcnt) > UIO_MAXIOV)
			return (EINVAL);
		MALLOC(iov, struct iovec *, iovlen, M_IOV, M_WAITOK);
		needfree = iov;
	} else {
		iov = aiov;
		needfree = NULL;
	}
	auio.uio_iov = iov;
	auio.uio_iovcnt = SCARG(uap, iovcnt);
	auio.uio_rw = UIO_WRITE;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_procp = p;
	error = copyin((caddr_t)SCARG(uap, iovp), (caddr_t)iov, iovlen);
	if (error)
		goto done;
	auio.uio_resid = 0;
	for (i = 0; i < SCARG(uap, iovcnt); i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if ((ssize_t)(auio.uio_resid += iov->iov_len) <= 0) {
			error = EINVAL;
			goto done;
		}
	}
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO))  {
		MALLOC(ktriov, struct iovec *, iovlen, M_TEMP, M_WAITOK);
		bcopy((caddr_t)auio.uio_iov, (caddr_t)ktriov, iovlen);
	}
#endif
	cnt = auio.uio_resid;
	error = (*fp->f_ops->fo_write)(fp, &auio, fp->f_cred);
	if (error) {
		if (auio.uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			psignal(p, SIGPIPE);
	}
	cnt -= auio.uio_resid;
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p->p_tracep, SCARG(uap, fd), UIO_WRITE,
				ktriov, cnt, error);
		FREE(ktriov, M_TEMP);
	}
#endif
	*retval = cnt;
done:
	if (needfree)
		FREE(needfree, M_IOV);
	return (error);
}

/*
 * Ioctl system call
 */
/* ARGSUSED */
int
sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	register struct file *fp;
	register struct filedesc *fdp;
	register u_long com;
	register int error;
	register u_int size;
	caddr_t data, memp;
	int tmp;
#define STK_PARAMS	128
	char stkbuf[STK_PARAMS];

	fdp = p->p_fd;
	if ((u_int)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);

	if ((fp->f_flag & (FREAD | FWRITE)) == 0)
		return (EBADF);

	switch (com = SCARG(uap, com)) {
	case FIONCLEX:
		fdp->fd_ofileflags[SCARG(uap, fd)] &= ~UF_EXCLOSE;
		return (0);
	case FIOCLEX:
		fdp->fd_ofileflags[SCARG(uap, fd)] |= UF_EXCLOSE;
		return (0);
	}

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX)
		return (ENOTTY);
	memp = NULL;
	if (size > sizeof (stkbuf)) {
		memp = (caddr_t)malloc((u_long)size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	} else
		data = stkbuf;
	if (com&IOC_IN) {
		if (size) {
			error = copyin(SCARG(uap, data), data, (u_int)size);
			if (error) {
				if (memp)
					free(memp, M_IOCTLOPS);
				return (error);
			}
		} else
			*(caddr_t *)data = SCARG(uap, data);
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(data, size);
	else if (com&IOC_VOID)
		*(caddr_t *)data = SCARG(uap, data);

	switch (com) {

	case FIONBIO:
		if ((tmp = *(int *)data) != 0)
			fp->f_flag |= FNONBLOCK;
		else
			fp->f_flag &= ~FNONBLOCK;
		error = (*fp->f_ops->fo_ioctl)(fp, FIONBIO, (caddr_t)&tmp, p);
		break;

	case FIOASYNC:
		if ((tmp = *(int *)data) != 0)
			fp->f_flag |= FASYNC;
		else
			fp->f_flag &= ~FASYNC;
		error = (*fp->f_ops->fo_ioctl)(fp, FIOASYNC, (caddr_t)&tmp, p);
		break;

	case FIOSETOWN:
		tmp = *(int *)data;
		if (fp->f_type == DTYPE_SOCKET) {
			struct socket *so = (struct socket *)fp->f_data;

			so->so_pgid = tmp;
			so->so_siguid = p->p_cred->p_ruid;
			so->so_sigeuid = p->p_ucred->cr_uid;
			error = 0;
			break;
		}
		if (tmp <= 0) {
			tmp = -tmp;
		} else {
			struct proc *p1 = pfind(tmp);
			if (p1 == 0) {
				error = ESRCH;
				break;
			}
			tmp = p1->p_pgrp->pg_id;
		}
		error = (*fp->f_ops->fo_ioctl)
			(fp, TIOCSPGRP, (caddr_t)&tmp, p);
		break;

	case FIOGETOWN:
		if (fp->f_type == DTYPE_SOCKET) {
			error = 0;
			*(int *)data = ((struct socket *)fp->f_data)->so_pgid;
			break;
		}
		error = (*fp->f_ops->fo_ioctl)(fp, TIOCGPGRP, data, p);
		*(int *)data = -*(int *)data;
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, p);
		/*
		 * Copy any data to user, size was
		 * already set and checked above.
		 */
		if (error == 0 && (com&IOC_OUT) && size)
			error = copyout(data, SCARG(uap, data), (u_int)size);
		break;
	}
	if (memp)
		free(memp, M_IOCTLOPS);
	return (error);
}

int	selwait, nselcoll;

/*
 * Select system call.
 */
int
sys_select(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	register struct sys_select_args /* {
		syscallarg(int) nd;
		syscallarg(fd_set *) in;
		syscallarg(fd_set *) ou;
		syscallarg(fd_set *) ex;
		syscallarg(struct timeval *) tv;
	} */ *uap = v;
	fd_set bits[6], *pibits[3], *pobits[3];
	struct timeval atv;
	int s, ncoll, error = 0, timo;
	u_int ni;

	if (SCARG(uap, nd) > p->p_fd->fd_nfiles) {
		/* forgiving; slightly wrong */
		SCARG(uap, nd) = p->p_fd->fd_nfiles;
	}
	ni = howmany(SCARG(uap, nd), NFDBITS) * sizeof(fd_mask);
	if (SCARG(uap, nd) > FD_SETSIZE) {
		caddr_t mbits;

		if ((mbits = malloc(ni * 6, M_TEMP, M_WAITOK)) == NULL) {
			error = EINVAL;
			goto cleanup;
		}
		bzero(mbits, ni * 6);
		pibits[0] = (fd_set *)&mbits[ni * 0];
		pibits[1] = (fd_set *)&mbits[ni * 1];
		pibits[2] = (fd_set *)&mbits[ni * 2];
		pobits[0] = (fd_set *)&mbits[ni * 3];
		pobits[1] = (fd_set *)&mbits[ni * 4];
		pobits[2] = (fd_set *)&mbits[ni * 5];
	} else {
		bzero((caddr_t)bits, sizeof(bits));
		pibits[0] = &bits[0];
		pibits[1] = &bits[1];
		pibits[2] = &bits[2];
		pobits[0] = &bits[3];
		pobits[1] = &bits[4];
		pobits[2] = &bits[5];
	}

#define	getbits(name, x) \
	if (SCARG(uap, name) && (error = copyin((caddr_t)SCARG(uap, name), \
	    (caddr_t)pibits[x], ni))) \
		goto done;
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits

	if (SCARG(uap, tv)) {
		error = copyin((caddr_t)SCARG(uap, tv), (caddr_t)&atv,
			sizeof (atv));
		if (error)
			goto done;
		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		s = splclock();
		timeradd(&atv, &time, &atv);
		timo = hzto(&atv);
		/*
		 * Avoid inadvertently sleeping forever.
		 */
		if (timo == 0)
			timo = 1;
		splx(s);
	} else
		timo = 0;
retry:
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	error = selscan(p, pibits[0], pobits[0], SCARG(uap, nd), retval);
	if (error || *retval)
		goto done;
	s = splhigh();
	/* this should be timercmp(&time, &atv, >=) */
	if (SCARG(uap, tv) && (time.tv_sec > atv.tv_sec ||
	    (time.tv_sec == atv.tv_sec && time.tv_usec >= atv.tv_usec))) {
		splx(s);
		goto done;
	}
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "select", timo);
	splx(s);
	if (error == 0)
		goto retry;
done:
	p->p_flag &= ~P_SELECT;
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
#define	putbits(name, x) \
	if (SCARG(uap, name) && (error2 = copyout((caddr_t)pobits[x], \
	    (caddr_t)SCARG(uap, name), ni))) \
		error = error2;
	if (error == 0) {
		int error2;

		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
#undef putbits
	}
	
cleanup:
	if (pibits[0] != &bits[0])
		free(pibits[0], M_TEMP);
	return (error);
}

int
selscan(p, ibits, obits, nfd, retval)
	struct proc *p;
	fd_set *ibits, *obits;
	int nfd;
	register_t *retval;
{
	caddr_t cibits = (caddr_t)ibits, cobits = (caddr_t)obits;
	register struct filedesc *fdp = p->p_fd;
	register int msk, i, j, fd;
	register fd_mask bits;
	struct file *fp;
	int ni, n = 0;
	static int flag[3] = { FREAD, FWRITE, 0 };

	/*
	 * if nfd > FD_SETSIZE then the fd_set's contain nfd bits (rounded
	 * up to the next byte) otherwise the fd_set's are normal sized.
	 */
	ni = sizeof(fd_set);
	if (nfd > FD_SETSIZE)
		ni = howmany(nfd, NFDBITS) * sizeof(fd_mask);

	for (msk = 0; msk < 3; msk++) {
		fd_set *pibits = (fd_set *)&cibits[msk*ni];
		fd_set *pobits = (fd_set *)&cobits[msk*ni];

		for (i = 0; i < nfd; i += NFDBITS) {
			bits = pibits->fds_bits[i/NFDBITS];
			while ((j = ffs(bits)) && (fd = i + --j) < nfd) {
				bits &= ~(1 << j);
				fp = fdp->fd_ofiles[fd];
				if (fp == NULL)
					return (EBADF);
				if ((*fp->f_ops->fo_select)(fp, flag[msk], p)) {
					FD_SET(fd, pobits);
					n++;
				}
			}
		}
	}
	*retval = n;
	return (0);
}

/*ARGSUSED*/
int
seltrue(dev, flag, p)
	dev_t dev;
	int flag;
	struct proc *p;
{

	return (1);
}

/*
 * Record a select request.
 */
void
selrecord(selector, sip)
	struct proc *selector;
	struct selinfo *sip;
{
	struct proc *p;
	pid_t mypid;

	mypid = selector->p_pid;
	if (sip->si_selpid == mypid)
		return;
	if (sip->si_selpid && (p = pfind(sip->si_selpid)) &&
	    p->p_wchan == (caddr_t)&selwait)
		sip->si_flags |= SI_COLL;
	else
		sip->si_selpid = mypid;
}

/*
 * Do a wakeup when a selectable event occurs.
 */
void
selwakeup(sip)
	register struct selinfo *sip;
{
	register struct proc *p;
	int s;

	if (sip->si_selpid == 0)
		return;
	if (sip->si_flags & SI_COLL) {
		nselcoll++;
		sip->si_flags &= ~SI_COLL;
		wakeup((caddr_t)&selwait);
	}
	p = pfind(sip->si_selpid);
	sip->si_selpid = 0;
	if (p != NULL) {
		s = splhigh();
		if (p->p_wchan == (caddr_t)&selwait) {
			if (p->p_stat == SSLEEP)
				setrunnable(p);
			else
				unsleep(p);
		} else if (p->p_flag & P_SELECT)
			p->p_flag &= ~P_SELECT;
		splx(s);
	}
}

void
pollscan(p, pl, nfd, retval)
	struct proc *p;
	struct pollfd *pl;
	int nfd;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register int msk, i;
	struct file *fp;
	int n = 0;
	static int flag[3] = { FREAD, FWRITE, 0 };
	static int pflag[3] = { POLLIN|POLLRDNORM, POLLOUT, POLLERR };

	/* 
	 * XXX: We need to implement the rest of the flags.
	 */
	for (i = 0; i < nfd; i++) {
		fp = fdp->fd_ofiles[pl[i].fd];
		if (fp == NULL) {
			if (pl[i].events & POLLNVAL) {
				pl[i].revents |= POLLNVAL;
				n++;
			}
			continue;
		}
		for (msk = 0; msk < 3; msk++) {
			if (pl[i].events & pflag[msk]) {
				if ((*fp->f_ops->fo_select)(fp, flag[msk], p)) {
					pl[i].revents |= pflag[msk] &
					    pl[i].events;
					n++;
				}
			}
		}
	}
	*retval = n;
}

/*
 * We are using the same mechanism as select only we encode/decode args
 * differently.
 */
int
sys_poll(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_poll_args *uap = v;
	size_t sz = sizeof(struct pollfd) * SCARG(uap, nfds);
	struct pollfd *pl;
	int msec = SCARG(uap, timeout);
	struct timeval atv;
	int timo, ncoll, i, s, error, error2;
	extern int nselcoll, selwait;

	pl = (struct pollfd *) malloc(sz, M_TEMP, M_WAITOK);

	if ((error = copyin(SCARG(uap, fds), pl, sz)) != 0)
		goto bad;

	for (i = 0; i < SCARG(uap, nfds); i++)
		pl[i].revents = 0;

	if (msec != -1) {
		atv.tv_sec = msec / 1000;
		atv.tv_usec = (msec - (atv.tv_sec * 1000)) * 1000;

		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		s = splclock();
		timeradd(&atv, &time, &atv);
		timo = hzto(&atv);
		/*
		 * Avoid inadvertently sleeping forever.
		 */
		if (timo == 0)
			timo = 1;
		splx(s);
	} else
		timo = 0;

retry:
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	pollscan(p, pl, SCARG(uap, nfds), retval);
	if (*retval)
		goto done;
	s = splhigh();
	if (timo && timercmp(&time, &atv, >=)) {
		splx(s);
		goto done;
	}
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "poll", timo);
	splx(s);
	if (error == 0)
		goto retry;

done:
	p->p_flag &= ~P_SELECT;
	/* poll is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
	if ((error2 = copyout(pl, SCARG(uap, fds), sz)) != 0)
		error = error2;
bad:
	free((char *) pl, M_TEMP);
	return (error);
}
