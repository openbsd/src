/*	$NetBSD: svr4_fcntl.c,v 1.14 1995/10/14 20:24:24 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_fcntl.h>
#include <compat/svr4/svr4_poll.h>

static u_long svr4_to_bsd_cmd __P((u_long));
static int svr4_to_bsd_flags __P((int));
static int bsd_to_svr4_flags __P((int));
static void bsd_to_svr4_flock __P((struct flock *, struct svr4_flock *));
static void svr4_to_bsd_flock __P((struct svr4_flock *, struct flock *));
static void svr4_pollscan __P((struct proc *, struct svr4_pollfd *, int,
			       register_t *));

static u_long
svr4_to_bsd_cmd(cmd)
	u_long	cmd;
{
	switch (cmd) {
	case SVR4_F_DUPFD:
		return F_DUPFD;
	case SVR4_F_GETFD:
		return F_GETFD;
	case SVR4_F_SETFD:
		return F_SETFD;
	case SVR4_F_GETFL:
		return F_GETFL;
	case SVR4_F_SETFL:
		return F_SETFL;
	case SVR4_F_GETLK:
		return F_GETLK;
	case SVR4_F_SETLK:
		return F_SETLK;
	case SVR4_F_SETLKW:
		return F_SETLKW;
	default:
		return -1;
	}
}


static int
svr4_to_bsd_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & SVR4_O_RDONLY) ? O_RDONLY : 0;
	r |= (l & SVR4_O_WRONLY) ? O_WRONLY : 0;
	r |= (l & SVR4_O_RDWR) ? O_RDWR : 0;
	r |= (l & SVR4_O_NDELAY) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_APPEND) ? O_APPEND : 0;
	r |= (l & SVR4_O_SYNC) ? O_FSYNC : 0;
	r |= (l & SVR4_O_RAIOSIG) ? O_ASYNC : 0;
	r |= (l & SVR4_O_NONBLOCK) ? O_NONBLOCK : 0;
	r |= (l & SVR4_O_PRIV) ? O_EXLOCK : 0;
	r |= (l & SVR4_O_CREAT) ? O_CREAT : 0;
	r |= (l & SVR4_O_TRUNC) ? O_TRUNC : 0;
	r |= (l & SVR4_O_EXCL) ? O_EXCL : 0;
	r |= (l & SVR4_O_NOCTTY) ? O_NOCTTY : 0;
	return r;
}


static int
bsd_to_svr4_flags(l)
	int	l;
{
	int	r = 0;
	r |= (l & O_RDONLY) ? SVR4_O_RDONLY : 0;
	r |= (l & O_WRONLY) ? SVR4_O_WRONLY : 0;
	r |= (l & O_RDWR) ? SVR4_O_RDWR : 0;
	r |= (l & O_NDELAY) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_APPEND) ? SVR4_O_APPEND : 0;
	r |= (l & O_FSYNC) ? SVR4_O_SYNC : 0;
	r |= (l & O_ASYNC) ? SVR4_O_RAIOSIG : 0;
	r |= (l & O_NONBLOCK) ? SVR4_O_NONBLOCK : 0;
	r |= (l & O_EXLOCK) ? SVR4_O_PRIV : 0;
	r |= (l & O_CREAT) ? SVR4_O_CREAT : 0;
	r |= (l & O_TRUNC) ? SVR4_O_TRUNC : 0;
	r |= (l & O_EXCL) ? SVR4_O_EXCL : 0;
	r |= (l & O_NOCTTY) ? SVR4_O_NOCTTY : 0;
	return r;
}

static void
bsd_to_svr4_flock(iflp, oflp)
	struct flock		*iflp;
	struct svr4_flock	*oflp;
{
	switch (iflp->l_type) {
	case F_RDLCK:
		oflp->l_type = SVR4_F_RDLCK;
		break;
	case F_WRLCK:
		oflp->l_type = SVR4_F_WRLCK;
		break;
	case F_UNLCK:
		oflp->l_type = SVR4_F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = (short) iflp->l_whence;
	oflp->l_start = (svr4_off_t) iflp->l_start;
	oflp->l_len = (svr4_off_t) iflp->l_len;
	oflp->l_sysid = 0;
	oflp->l_pid = (svr4_pid_t) iflp->l_pid;
}


static void
svr4_to_bsd_flock(iflp, oflp)
	struct svr4_flock	*iflp;
	struct flock		*oflp;
{
	switch (iflp->l_type) {
	case SVR4_F_RDLCK:
		oflp->l_type = F_RDLCK;
		break;
	case SVR4_F_WRLCK:
		oflp->l_type = F_WRLCK;
		break;
	case SVR4_F_UNLCK:
		oflp->l_type = F_UNLCK;
		break;
	default:
		oflp->l_type = -1;
		break;
	}

	oflp->l_whence = iflp->l_whence;
	oflp->l_start = (off_t) iflp->l_start;
	oflp->l_len = (off_t) iflp->l_len;
	oflp->l_pid = (pid_t) iflp->l_pid;

}

int
svr4_sys_open(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_open_args	*uap = v;
	int			error;
	struct sys_open_args	cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, flags) = svr4_to_bsd_flags(SCARG(uap, flags));
	SCARG(&cup, mode) = SCARG(uap, mode);
	error = sys_open(p, &cup, retval);

	if (error)
		return error;

	if ((SCARG(&cup, flags) & O_NOCTTY) && SESS_LEADER(p) &&
	    !(p->p_flag & P_CONTROLT)) {
		struct filedesc	*fdp = p->p_fd;
		struct file	*fp = fdp->fd_ofiles[*retval];

		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl) (fp, TIOCSCTTY, (caddr_t) 0, p);
	}
	return 0;
}


int
svr4_sys_creat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_creat_args *uap = v;
	struct sys_open_args cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, mode) = SCARG(uap, mode);
	SCARG(&cup, flags) = O_WRONLY | O_CREAT | O_TRUNC;

	return sys_open(p, &cup, retval);
}


int
svr4_sys_access(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_access_args *uap = v;
	struct sys_access_args cup;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, flags) = SCARG(uap, flags);

	return sys_access(p, &cup, retval);
}


int
svr4_sys_fcntl(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fcntl_args	*uap = v;
	int				error;
	struct sys_fcntl_args		fa;

	SCARG(&fa, fd) = SCARG(uap, fd);
	SCARG(&fa, cmd) = svr4_to_bsd_cmd(SCARG(uap, cmd));

	switch (SCARG(&fa, cmd)) {
	case F_DUPFD:
	case F_GETFD:
	case F_SETFD:
		SCARG(&fa, arg) = SCARG(uap, arg);
		return sys_fcntl(p, &fa, retval);

	case F_GETFL:
		SCARG(&fa, arg) = SCARG(uap, arg);
		error = sys_fcntl(p, &fa, retval);
		if (error)
			return error;
		*retval = bsd_to_svr4_flags(*retval);
		return error;

	case F_SETFL:
		SCARG(&fa, arg) =
			(void *) svr4_to_bsd_flags((u_long) SCARG(uap, arg));
		return sys_fcntl(p, &fa, retval);

	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
		{
			struct svr4_flock	 ifl;
			struct flock		*flp, fl;
			caddr_t sg = stackgap_init(p->p_emul);

			flp = stackgap_alloc(&sg, sizeof(struct flock));
			SCARG(&fa, arg) = (void *) flp;

			error = copyin(SCARG(uap, arg), &ifl, sizeof ifl);
			if (error)
				return error;

			svr4_to_bsd_flock(&ifl, &fl);

			error = copyout(&fl, flp, sizeof fl);
			if (error)
				return error;

			error = sys_fcntl(p, &fa, retval);
			if (error || SCARG(&fa, cmd) != F_GETLK)
				return error;

			error = copyin(flp, &fl, sizeof fl);
			if (error)
				return error;

			bsd_to_svr4_flock(&fl, &ifl);

			return copyout(&ifl, SCARG(uap, arg), sizeof ifl);
		}
	default:
		return ENOSYS;
	}
}


static void
svr4_pollscan(p, pl, nfd, retval)
	struct proc *p;
	struct svr4_pollfd *pl;
	int nfd;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register int msk, i;
	struct file *fp;
	int n = 0;
	static int flag[3] = { FREAD, FWRITE, 0 };
	static int pflag[3] = { SVR4_POLLIN|SVR4_POLLRDNORM, 
				SVR4_POLLOUT, SVR4_POLLERR };

	/* 
	 * XXX: We need to implement the rest of the flags.
	 */
	for (i = 0; i < nfd; i++) {
		fp = fdp->fd_ofiles[pl[i].fd];
		if (fp == NULL) {
			if (pl[i].events & SVR4_POLLNVAL) {
			    pl[i].revents |= SVR4_POLLNVAL;
			    n++;
			}
			continue;
		}
		for (msk = 0; msk < 3; msk++) {
			if (pl[i].events & pflag[msk]) {
				if ((*fp->f_ops->fo_select)(fp, flag[msk], p)) {
					pl[i].revents |= 
						pflag[msk] & pl[i].events;
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
svr4_sys_poll(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_poll_args *uap = v;
	int i, s;
	int error, error2;
	size_t sz = sizeof(struct svr4_pollfd) * SCARG(uap, nfds);
	struct svr4_pollfd *pl;
	int msec = SCARG(uap, timeout);
	struct timeval atv;
	int timo;
	int ncoll;
	extern int nselcoll, selwait;

	pl = (struct svr4_pollfd *) malloc(sz, M_TEMP, M_WAITOK);

	if ((error = copyin(SCARG(uap, fds), pl, sz)) != 0)
		goto bad;

	for (i = 0; i < SCARG(uap, nfds); i++) {
		DPRINTF(("pollfd %d, %x\n", pl[i].fd, pl[i].events));
		pl[i].revents = 0;
	}

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
	svr4_pollscan(p, pl, SCARG(uap, nfds), retval);
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
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "svr4_poll", timo);
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
