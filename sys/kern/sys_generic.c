/*	$OpenBSD: sys_generic.c,v 1.122 2018/08/20 16:00:22 mpi Exp $	*/
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
 *	@(#)sys_generic.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
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
#include <sys/sched.h>
#include <sys/pledge.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

int selscan(struct proc *, fd_set *, fd_set *, int, int, register_t *);
void pollscan(struct proc *, struct pollfd *, u_int, register_t *);
int pollout(struct pollfd *, struct pollfd *, u_int);
int dopselect(struct proc *, int, fd_set *, fd_set *, fd_set *,
    const struct timespec *, const sigset_t *, register_t *);
int doppoll(struct proc *, struct pollfd *, u_int, const struct timespec *,
    const sigset_t *, register_t *);

int
iovec_copyin(const struct iovec *uiov, struct iovec **iovp, struct iovec *aiov,
    unsigned int iovcnt, size_t *residp)
{
#ifdef KTRACE
	struct proc *p = curproc;
#endif
	struct iovec *iov;
	int error, i;
	size_t resid = 0;

	if (iovcnt > UIO_SMALLIOV) {
		if (iovcnt > IOV_MAX)
			return (EINVAL);
		iov = mallocarray(iovcnt, sizeof(*iov), M_IOV, M_WAITOK);
	} else if (iovcnt > 0) {
		iov = aiov;
	} else {
		return (EINVAL);
	}
	*iovp = iov;

	if ((error = copyin(uiov, iov, iovcnt * sizeof(*iov))))
		return (error);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktriovec(p, iov, iovcnt);
#endif

	for (i = 0; i < iovcnt; i++) {
		resid += iov->iov_len;
		/*
		 * Writes return ssize_t because -1 is returned on error.
		 * Therefore we must restrict the length to SSIZE_MAX to
		 * avoid garbage return values.  Note that the addition is
		 * guaranteed to not wrap because SSIZE_MAX * 2 < SIZE_MAX.
		 */
		if (iov->iov_len > SSIZE_MAX || resid > SSIZE_MAX)
			return (EINVAL);
		iov++;
	}

	if (residp != NULL)
		*residp = resid;

	return (0);
}

void
iovec_free(struct iovec *iov, unsigned int iovcnt)
{
	if (iovcnt > UIO_SMALLIOV)
		free(iov, M_IOV, iovcnt * sizeof(*iov));
}

/*
 * Read system call.
 */
int
sys_read(struct proc *p, void *v, register_t *retval)
{
	struct sys_read_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	struct iovec iov;
	struct uio auio;

	iov.iov_base = SCARG(uap, buf);
	iov.iov_len = SCARG(uap, nbyte);
	if (iov.iov_len > SSIZE_MAX)
		return (EINVAL);

	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = iov.iov_len;

	return (dofilereadv(p, SCARG(uap, fd), &auio, 0, retval));
}

/*
 * Scatter read system call.
 */
int
sys_readv(struct proc *p, void *v, register_t *retval)
{
	struct sys_readv_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
	} */ *uap = v;
	struct iovec aiov[UIO_SMALLIOV], *iov = NULL;
	int error, iovcnt = SCARG(uap, iovcnt);
	struct uio auio;
	size_t resid;

	error = iovec_copyin(SCARG(uap, iovp), &iov, aiov, iovcnt, &resid);
	if (error)
		goto done;

	auio.uio_iov = iov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = resid;

	error = dofilereadv(p, SCARG(uap, fd), &auio, 0, retval);
 done:
	iovec_free(iov, iovcnt);
	return (error);
}

int
dofilereadv(struct proc *p, int fd, struct uio *uio, int flags,
    register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	long cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	KASSERT(uio->uio_iov != NULL && uio->uio_iovcnt > 0);
	iovlen = uio->uio_iovcnt * sizeof(struct iovec);

	if ((fp = fd_getfile_mode(fdp, fd, FREAD)) == NULL)
		return (EBADF);

	/* Checks for positioned read. */
	if (flags & FO_POSITION) {
		struct vnode *vp = fp->f_data;

		if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO ||
		    (vp->v_flag & VISTTY)) {
			error = ESPIPE;
			goto done;
		}

		if (uio->uio_offset < 0 && vp->v_type != VCHR) {
			error = EINVAL;
			goto done;
		}
	}

	uio->uio_rw = UIO_READ;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO)) {
		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		memcpy(ktriov, uio->uio_iov, iovlen);
	}
#endif
	cnt = uio->uio_resid;
	error = (*fp->f_ops->fo_read)(fp, uio, flags);
	if (error) {
		if (uio->uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
	cnt -= uio->uio_resid;

	mtx_enter(&fp->f_mtx);
	fp->f_rxfer++;
	fp->f_rbytes += cnt;
	mtx_leave(&fp->f_mtx);
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, fd, UIO_READ, ktriov, cnt);
		free(ktriov, M_TEMP, iovlen);
	}
#endif
	*retval = cnt;
 done:
	FRELE(fp, p);
	return (error);
}

/*
 * Write system call
 */
int
sys_write(struct proc *p, void *v, register_t *retval)
{
	struct sys_write_args /* {
		syscallarg(int) fd;
		syscallarg(const void *) buf;
		syscallarg(size_t) nbyte;
	} */ *uap = v;
	struct iovec iov;
	struct uio auio;

	iov.iov_base = (void *)SCARG(uap, buf);
	iov.iov_len = SCARG(uap, nbyte);
	if (iov.iov_len > SSIZE_MAX)
		return (EINVAL);

	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_resid = iov.iov_len;

	return (dofilewritev(p, SCARG(uap, fd), &auio, 0, retval));
}

/*
 * Gather write system call
 */
int
sys_writev(struct proc *p, void *v, register_t *retval)
{
	struct sys_writev_args /* {
		syscallarg(int) fd;
		syscallarg(const struct iovec *) iovp;
		syscallarg(int) iovcnt;
	} */ *uap = v;
	struct iovec aiov[UIO_SMALLIOV], *iov = NULL;
	int error, iovcnt = SCARG(uap, iovcnt);
	struct uio auio;
	size_t resid;

	error = iovec_copyin(SCARG(uap, iovp), &iov, aiov, iovcnt, &resid);
	if (error)
		goto done;

	auio.uio_iov = iov;
	auio.uio_iovcnt = iovcnt;
	auio.uio_resid = resid;

	error = dofilewritev(p, SCARG(uap, fd), &auio, 0, retval);
 done:
	iovec_free(iov, iovcnt);
 	return (error);
}

int
dofilewritev(struct proc *p, int fd, struct uio *uio, int flags,
    register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	long cnt, error = 0;
	u_int iovlen;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	KASSERT(uio->uio_iov != NULL && uio->uio_iovcnt > 0);
	iovlen = uio->uio_iovcnt * sizeof(struct iovec);

	if ((fp = fd_getfile_mode(fdp, fd, FWRITE)) == NULL)
		return (EBADF);

	/* Checks for positioned write. */
	if (flags & FO_POSITION) {
		struct vnode *vp = fp->f_data;

		if (fp->f_type != DTYPE_VNODE || vp->v_type == VFIFO ||
		    (vp->v_flag & VISTTY)) {
			error = ESPIPE;
			goto done;
		}

		if (uio->uio_offset < 0 && vp->v_type != VCHR) {
			error = EINVAL;
			goto done;
		}
	}

	uio->uio_rw = UIO_WRITE;
	uio->uio_segflg = UIO_USERSPACE;
	uio->uio_procp = p;
#ifdef KTRACE
	/*
	 * if tracing, save a copy of iovec
	 */
	if (KTRPOINT(p, KTR_GENIO)) {
		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		memcpy(ktriov, uio->uio_iov, iovlen);
	}
#endif
	cnt = uio->uio_resid;
	error = (*fp->f_ops->fo_write)(fp, uio, flags);
	if (error) {
		if (uio->uio_resid != cnt && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			ptsignal(p, SIGPIPE, STHREAD);
	}
	cnt -= uio->uio_resid;

	mtx_enter(&fp->f_mtx);
	fp->f_wxfer++;
	fp->f_wbytes += cnt;
	mtx_leave(&fp->f_mtx);
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, fd, UIO_WRITE, ktriov, cnt);
		free(ktriov, M_TEMP, iovlen);
	}
#endif
	*retval = cnt;
 done:
	FRELE(fp, p);
	return (error);
}

/*
 * Ioctl system call
 */
int
sys_ioctl(struct proc *p, void *v, register_t *retval)
{
	struct sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(void *) data;
	} */ *uap = v;
	struct file *fp;
	struct filedesc *fdp = p->p_fd;
	u_long com = SCARG(uap, com);
	int error = 0;
	u_int size = 0;
	caddr_t data, memp = NULL;
	int tmp;
#define STK_PARAMS	128
	long long stkbuf[STK_PARAMS / sizeof(long long)];

	if ((fp = fd_getfile_mode(fdp, SCARG(uap, fd), FREAD|FWRITE)) == NULL)
		return (EBADF);

	if (fp->f_type == DTYPE_SOCKET) {
		struct socket *so = fp->f_data;

		if (so->so_state & SS_DNS) {
			error = EINVAL;
			goto out;
		}
	}

	error = pledge_ioctl(p, com, fp);
	if (error)
		goto out;

	switch (com) {
	case FIONCLEX:
	case FIOCLEX:
		fdplock(fdp);
		if (com == FIONCLEX)
			fdp->fd_ofileflags[SCARG(uap, fd)] &= ~UF_EXCLOSE;
		else
			fdp->fd_ofileflags[SCARG(uap, fd)] |= UF_EXCLOSE;
		fdpunlock(fdp);
		goto out;
	}

	/*
	 * Interpret high order word to find amount of data to be
	 * copied to/from the user's address space.
	 */
	size = IOCPARM_LEN(com);
	if (size > IOCPARM_MAX) {
		error = ENOTTY;
		goto out;
	}
	if (size > sizeof (stkbuf)) {
		memp = malloc(size, M_IOCTLOPS, M_WAITOK);
		data = memp;
	} else
		data = (caddr_t)stkbuf;
	if (com&IOC_IN) {
		if (size) {
			error = copyin(SCARG(uap, data), data, size);
			if (error) {
				goto out;
			}
		} else
			*(caddr_t *)data = SCARG(uap, data);
	} else if ((com&IOC_OUT) && size)
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		memset(data, 0, size);
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

		if (fp->f_type == DTYPE_SOCKET || fp->f_type == DTYPE_PIPE) {
			/* nothing */
		} else if (tmp <= 0) {
			tmp = -tmp;
		} else {
			struct process *pr = prfind(tmp);
			if (pr == NULL) {
				error = ESRCH;
				break;
			}
			tmp = pr->ps_pgrp->pg_id;
		}
		error = (*fp->f_ops->fo_ioctl)
		    (fp, TIOCSPGRP, (caddr_t)&tmp, p);
		break;

	case FIOGETOWN:
		error = (*fp->f_ops->fo_ioctl)(fp, TIOCGPGRP, data, p);
		*(int *)data = -*(int *)data;
		break;

	default:
		error = (*fp->f_ops->fo_ioctl)(fp, com, data, p);
		break;
	}
	/*
	 * Copy any data to user, size was
	 * already set and checked above.
	 */
	if (error == 0 && (com&IOC_OUT) && size)
		error = copyout(data, SCARG(uap, data), size);
out:
	FRELE(fp, p);
	free(memp, M_IOCTLOPS, size);
	return (error);
}

int	selwait, nselcoll;

/*
 * Select system call.
 */
int
sys_select(struct proc *p, void *v, register_t *retval)
{
	struct sys_select_args /* {
		syscallarg(int) nd;
		syscallarg(fd_set *) in;
		syscallarg(fd_set *) ou;
		syscallarg(fd_set *) ex;
		syscallarg(struct timeval *) tv;
	} */ *uap = v;

	struct timespec ts, *tsp = NULL;
	int error;

	if (SCARG(uap, tv) != NULL) {
		struct timeval tv;
		if ((error = copyin(SCARG(uap, tv), &tv, sizeof tv)) != 0)
			return (error);
		if ((error = itimerfix(&tv)) != 0)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimeval(p, &tv);
#endif
		TIMEVAL_TO_TIMESPEC(&tv, &ts);
		tsp = &ts;
	}

	return (dopselect(p, SCARG(uap, nd), SCARG(uap, in), SCARG(uap, ou),
	    SCARG(uap, ex), tsp, NULL, retval));
}

int
sys_pselect(struct proc *p, void *v, register_t *retval)
{
	struct sys_pselect_args /* {
		syscallarg(int) nd;
		syscallarg(fd_set *) in;
		syscallarg(fd_set *) ou;
		syscallarg(fd_set *) ex;
		syscallarg(const struct timespec *) ts;
		syscallarg(const sigset_t *) mask;
	} */ *uap = v;

	struct timespec ts, *tsp = NULL;
	sigset_t ss, *ssp = NULL;
	int error;

	if (SCARG(uap, ts) != NULL) {
		if ((error = copyin(SCARG(uap, ts), &ts, sizeof ts)) != 0)
			return (error);
		if ((error = timespecfix(&ts)) != 0)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		tsp = &ts;
	}
	if (SCARG(uap, mask) != NULL) {
		if ((error = copyin(SCARG(uap, mask), &ss, sizeof ss)) != 0)
			return (error);
		ssp = &ss;
	}

	return (dopselect(p, SCARG(uap, nd), SCARG(uap, in), SCARG(uap, ou),
	    SCARG(uap, ex), tsp, ssp, retval));
}

int
dopselect(struct proc *p, int nd, fd_set *in, fd_set *ou, fd_set *ex,
    const struct timespec *tsp, const sigset_t *sigmask, register_t *retval)
{
	fd_mask bits[6];
	fd_set *pibits[3], *pobits[3];
	struct timespec ats, rts, tts;
	int s, ncoll, error = 0, timo;
	u_int ni;

	if (nd < 0)
		return (EINVAL);
	if (nd > p->p_fd->fd_nfiles) {
		/* forgiving; slightly wrong */
		nd = p->p_fd->fd_nfiles;
	}
	ni = howmany(nd, NFDBITS) * sizeof(fd_mask);
	if (ni > sizeof(bits[0])) {
		caddr_t mbits;

		mbits = mallocarray(6, ni, M_TEMP, M_WAITOK|M_ZERO);
		pibits[0] = (fd_set *)&mbits[ni * 0];
		pibits[1] = (fd_set *)&mbits[ni * 1];
		pibits[2] = (fd_set *)&mbits[ni * 2];
		pobits[0] = (fd_set *)&mbits[ni * 3];
		pobits[1] = (fd_set *)&mbits[ni * 4];
		pobits[2] = (fd_set *)&mbits[ni * 5];
	} else {
		memset(bits, 0, sizeof(bits));
		pibits[0] = (fd_set *)&bits[0];
		pibits[1] = (fd_set *)&bits[1];
		pibits[2] = (fd_set *)&bits[2];
		pobits[0] = (fd_set *)&bits[3];
		pobits[1] = (fd_set *)&bits[4];
		pobits[2] = (fd_set *)&bits[5];
	}

#define	getbits(name, x) \
	if (name && (error = copyin(name, pibits[x], ni))) \
		goto done;
	getbits(in, 0);
	getbits(ou, 1);
	getbits(ex, 2);
#undef	getbits
#ifdef KTRACE
	if (ni > 0 && KTRPOINT(p, KTR_STRUCT)) {
		if (in) ktrfdset(p, pibits[0], ni);
		if (ou) ktrfdset(p, pibits[1], ni);
		if (ex) ktrfdset(p, pibits[2], ni);
	}
#endif

	if (tsp) {
		getnanouptime(&rts);
		timespecadd(tsp, &rts, &ats);
	} else {
		ats.tv_sec = 0;
		ats.tv_nsec = 0;
	}
	timo = 0;

	if (sigmask)
		dosigsuspend(p, *sigmask &~ sigcantmask);

retry:
	ncoll = nselcoll;
	atomic_setbits_int(&p->p_flag, P_SELECT);
	error = selscan(p, pibits[0], pobits[0], nd, ni, retval);
	if (error || *retval)
		goto done;
	if (tsp) {
		getnanouptime(&rts);
		if (timespeccmp(&rts, &ats, >=))
			goto done;
		timespecsub(&ats, &rts, &tts);
		timo = tts.tv_sec > 24 * 60 * 60 ?
			24 * 60 * 60 * hz : tstohz(&tts);
	}
	s = splhigh();
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	atomic_clearbits_int(&p->p_flag, P_SELECT);
	error = tsleep(&selwait, PSOCK | PCATCH, "select", timo);
	splx(s);
	if (error == 0)
		goto retry;
done:
	atomic_clearbits_int(&p->p_flag, P_SELECT);
	/* select is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;
#define	putbits(name, x) \
	if (name && (error2 = copyout(pobits[x], name, ni))) \
		error = error2;
	if (error == 0) {
		int error2;

		putbits(in, 0);
		putbits(ou, 1);
		putbits(ex, 2);
#undef putbits
#ifdef KTRACE
		if (ni > 0 && KTRPOINT(p, KTR_STRUCT)) {
			if (in) ktrfdset(p, pobits[0], ni);
			if (ou) ktrfdset(p, pobits[1], ni);
			if (ex) ktrfdset(p, pobits[2], ni);
		}
#endif
	}

	if (pibits[0] != (fd_set *)&bits[0])
		free(pibits[0], M_TEMP, 6 * ni);
	return (error);
}

int
selscan(struct proc *p, fd_set *ibits, fd_set *obits, int nfd, int ni,
    register_t *retval)
{
	caddr_t cibits = (caddr_t)ibits, cobits = (caddr_t)obits;
	struct filedesc *fdp = p->p_fd;
	int msk, i, j, fd;
	fd_mask bits;
	struct file *fp;
	int n = 0;
	static const int flag[3] = { POLLIN, POLLOUT|POLL_NOHUP, POLLPRI };

	for (msk = 0; msk < 3; msk++) {
		fd_set *pibits = (fd_set *)&cibits[msk*ni];
		fd_set *pobits = (fd_set *)&cobits[msk*ni];

		for (i = 0; i < nfd; i += NFDBITS) {
			bits = pibits->fds_bits[i/NFDBITS];
			while ((j = ffs(bits)) && (fd = i + --j) < nfd) {
				bits &= ~(1 << j);
				if ((fp = fd_getfile(fdp, fd)) == NULL)
					return (EBADF);
				if ((*fp->f_ops->fo_poll)(fp, flag[msk], p)) {
					FD_SET(fd, pobits);
					n++;
				}
				FRELE(fp, p);
			}
		}
	}
	*retval = n;
	return (0);
}

int
seltrue(dev_t dev, int events, struct proc *p)
{

	return (events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

int
selfalse(dev_t dev, int events, struct proc *p)
{

	return (0);
}

/*
 * Record a select request.
 */
void
selrecord(struct proc *selector, struct selinfo *sip)
{
	struct proc *p;
	pid_t mytid;

	mytid = selector->p_tid;
	if (sip->si_seltid == mytid)
		return;
	if (sip->si_seltid && (p = tfind(sip->si_seltid)) &&
	    p->p_wchan == (caddr_t)&selwait)
		sip->si_flags |= SI_COLL;
	else
		sip->si_seltid = mytid;
}

/*
 * Do a wakeup when a selectable event occurs.
 */
void
selwakeup(struct selinfo *sip)
{
	struct proc *p;
	int s;

	KNOTE(&sip->si_note, NOTE_SUBMIT);
	if (sip->si_seltid == 0)
		return;
	if (sip->si_flags & SI_COLL) {
		nselcoll++;
		sip->si_flags &= ~SI_COLL;
		wakeup(&selwait);
	}
	p = tfind(sip->si_seltid);
	sip->si_seltid = 0;
	if (p != NULL) {
		SCHED_LOCK(s);
		if (p->p_wchan == (caddr_t)&selwait) {
			if (p->p_stat == SSLEEP)
				setrunnable(p);
			else
				unsleep(p);
		} else if (p->p_flag & P_SELECT)
			atomic_clearbits_int(&p->p_flag, P_SELECT);
		SCHED_UNLOCK(s);
	}
}

void
pollscan(struct proc *p, struct pollfd *pl, u_int nfd, register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp;
	u_int i;
	int n = 0;

	for (i = 0; i < nfd; i++, pl++) {
		/* Check the file descriptor. */
		if (pl->fd < 0) {
			pl->revents = 0;
			continue;
		}
		if ((fp = fd_getfile(fdp, pl->fd)) == NULL) {
			pl->revents = POLLNVAL;
			n++;
			continue;
		}
		pl->revents = (*fp->f_ops->fo_poll)(fp, pl->events, p);
		FRELE(fp, p);
		if (pl->revents != 0)
			n++;
	}
	*retval = n;
}

/*
 * Only copyout the revents field.
 */
int
pollout(struct pollfd *pl, struct pollfd *upl, u_int nfds)
{
	int error = 0;
	u_int i = 0;

	while (!error && i++ < nfds) {
		error = copyout(&pl->revents, &upl->revents,
		    sizeof(upl->revents));
		pl++;
		upl++;
	}

	return (error);
}

/*
 * We are using the same mechanism as select only we encode/decode args
 * differently.
 */
int
sys_poll(struct proc *p, void *v, register_t *retval)
{
	struct sys_poll_args /* {
		syscallarg(struct pollfd *) fds;
		syscallarg(u_int) nfds;
		syscallarg(int) timeout;
	} */ *uap = v;

	struct timespec ts, *tsp = NULL;
	int msec = SCARG(uap, timeout);

	if (msec != INFTIM) {
		if (msec < 0)
			return (EINVAL);
		ts.tv_sec = msec / 1000;
		ts.tv_nsec = (msec - (ts.tv_sec * 1000)) * 1000000;
		tsp = &ts;
	}

	return (doppoll(p, SCARG(uap, fds), SCARG(uap, nfds), tsp, NULL,
	    retval));
}

int
sys_ppoll(struct proc *p, void *v, register_t *retval)
{
	struct sys_ppoll_args /* {
		syscallarg(struct pollfd *) fds;
		syscallarg(u_int) nfds;
		syscallarg(const struct timespec *) ts;
		syscallarg(const sigset_t *) mask;
	} */ *uap = v;

	int error;
	struct timespec ts, *tsp = NULL;
	sigset_t ss, *ssp = NULL;

	if (SCARG(uap, ts) != NULL) {
		if ((error = copyin(SCARG(uap, ts), &ts, sizeof ts)) != 0)
			return (error);
		if ((error = timespecfix(&ts)) != 0)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		tsp = &ts;
	}

	if (SCARG(uap, mask) != NULL) {
		if ((error = copyin(SCARG(uap, mask), &ss, sizeof ss)) != 0)
			return (error);
		ssp = &ss;
	}

	return (doppoll(p, SCARG(uap, fds), SCARG(uap, nfds), tsp, ssp,
	    retval));
}

int
doppoll(struct proc *p, struct pollfd *fds, u_int nfds,
    const struct timespec *tsp, const sigset_t *sigmask, register_t *retval)
{
	size_t sz;
	struct pollfd pfds[4], *pl = pfds;
	struct timespec ats, rts, tts;
	int timo, ncoll, i, s, error;

	/* Standards say no more than MAX_OPEN; this is possibly better. */
	if (nfds > min((int)p->p_rlimit[RLIMIT_NOFILE].rlim_cur, maxfiles))
		return (EINVAL);

	/* optimize for the default case, of a small nfds value */
	if (nfds > nitems(pfds)) {
		pl = mallocarray(nfds, sizeof(*pl), M_TEMP,
		    M_WAITOK | M_CANFAIL);
		if (pl == NULL)
			return (EINVAL);
	}

	sz = nfds * sizeof(*pl);

	if ((error = copyin(fds, pl, sz)) != 0)
		goto bad;

	for (i = 0; i < nfds; i++) {
		pl[i].events &= ~POLL_NOHUP;
		pl[i].revents = 0;
	}

	if (tsp != NULL) {
		getnanouptime(&rts);
		timespecadd(tsp, &rts, &ats);
	} else {
		ats.tv_sec = 0;
		ats.tv_nsec = 0;
	}
	timo = 0;

	if (sigmask)
		dosigsuspend(p, *sigmask &~ sigcantmask);

retry:
	ncoll = nselcoll;
	atomic_setbits_int(&p->p_flag, P_SELECT);
	pollscan(p, pl, nfds, retval);
	if (*retval)
		goto done;
	if (tsp != NULL) {
		getnanouptime(&rts);
		if (timespeccmp(&rts, &ats, >=))
			goto done;
		timespecsub(&ats, &rts, &tts);
		timo = tts.tv_sec > 24 * 60 * 60 ?
			24 * 60 * 60 * hz : tstohz(&tts);
	}
	s = splhigh();
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	atomic_clearbits_int(&p->p_flag, P_SELECT);
	error = tsleep(&selwait, PSOCK | PCATCH, "poll", timo);
	splx(s);
	if (error == 0)
		goto retry;

done:
	atomic_clearbits_int(&p->p_flag, P_SELECT);
	/*
	 * NOTE: poll(2) is not restarted after a signal and EWOULDBLOCK is
	 *       ignored (since the whole point is to see what would block).
	 */
	switch (error) {
	case ERESTART:
		error = pollout(pl, fds, nfds);
		if (error == 0)
			error = EINTR;
		break;
	case EWOULDBLOCK:
	case 0:
		error = pollout(pl, fds, nfds);
		break;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrpollfd(p, pl, nfds);
#endif /* KTRACE */
bad:
	if (pl != pfds)
		free(pl, M_TEMP, sz);
	return (error);
}

/*
 * utrace system call
 */
int
sys_utrace(struct proc *curp, void *v, register_t *retval)
{
#ifdef KTRACE
	struct sys_utrace_args /* {
		syscallarg(const char *) label;
		syscallarg(const void *) addr;
		syscallarg(size_t) len;
	} */ *uap = v;
	return (ktruser(curp, SCARG(uap, label), SCARG(uap, addr),
	    SCARG(uap, len)));
#else
	return (0);
#endif
}
