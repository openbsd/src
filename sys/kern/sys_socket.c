/*	$OpenBSD: sys_socket.c,v 1.35 2017/12/10 11:31:54 mpi Exp $	*/
/*	$NetBSD: sys_socket.c,v 1.13 1995/08/12 23:59:09 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *	@(#)sys_socket.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/route.h>

struct	fileops socketops = {
	soo_read, soo_write, soo_ioctl, soo_poll, soo_kqfilter,
	soo_stat, soo_close
};

int
soo_read(struct file *fp, off_t *poff, struct uio *uio, struct ucred *cred)
{

	return (soreceive((struct socket *)fp->f_data, (struct mbuf **)NULL,
		uio, (struct mbuf **)NULL, (struct mbuf **)NULL, (int *)NULL,
		(socklen_t)0));
}

int
soo_write(struct file *fp, off_t *poff, struct uio *uio, struct ucred *cred)
{

	return (sosend((struct socket *)fp->f_data, (struct mbuf *)NULL,
		uio, (struct mbuf *)NULL, (struct mbuf *)NULL, 0));
}

int
soo_ioctl(struct file *fp, u_long cmd, caddr_t data, struct proc *p)
{
	struct socket *so = (struct socket *)fp->f_data;
	int s, error = 0;

	switch (cmd) {

	case FIONBIO:
		s = solock(so);
		if (*(int *)data)
			so->so_state |= SS_NBIO;
		else
			so->so_state &= ~SS_NBIO;
		sounlock(s);
		break;

	case FIOASYNC:
		s = solock(so);
		if (*(int *)data) {
			so->so_state |= SS_ASYNC;
			so->so_rcv.sb_flags |= SB_ASYNC;
			so->so_snd.sb_flags |= SB_ASYNC;
		} else {
			so->so_state &= ~SS_ASYNC;
			so->so_rcv.sb_flags &= ~SB_ASYNC;
			so->so_snd.sb_flags &= ~SB_ASYNC;
		}
		sounlock(s);
		break;

	case FIONREAD:
		*(int *)data = so->so_rcv.sb_datacc;
		break;

	case SIOCSPGRP:
		so->so_pgid = *(int *)data;
		so->so_siguid = p->p_ucred->cr_ruid;
		so->so_sigeuid = p->p_ucred->cr_uid;
		break;

	case SIOCGPGRP:
		*(int *)data = so->so_pgid;
		break;

	case SIOCATMARK:
		*(int *)data = (so->so_state&SS_RCVATMARK) != 0;
		break;

	default:
		/*
		 * Interface/routing/protocol specific ioctls:
		 * interface and routing ioctls should have a
		 * different entry since a socket's unnecessary
		 */
		if (IOCGROUP(cmd) == 'i') {
			error = ifioctl(so, cmd, data, p);
			return (error);
		}
		if (IOCGROUP(cmd) == 'r')
			return (EOPNOTSUPP);
		s = solock(so);
		error = ((*so->so_proto->pr_usrreq)(so, PRU_CONTROL,
		    (struct mbuf *)cmd, (struct mbuf *)data, NULL, p));
		sounlock(s);
		break;
	}

	return (error);
}

int
soo_poll(struct file *fp, int events, struct proc *p)
{
	struct socket *so = fp->f_data;
	int revents = 0;
	int s;

	s = solock(so);
	if (events & (POLLIN | POLLRDNORM)) {
		if (soreadable(so))
			revents |= events & (POLLIN | POLLRDNORM);
	}
	/* NOTE: POLLHUP and POLLOUT/POLLWRNORM are mutually exclusive */
	if (so->so_state & SS_ISDISCONNECTED) {
		revents |= POLLHUP;
	} else if (events & (POLLOUT | POLLWRNORM)) {
		if (sowriteable(so))
			revents |= events & (POLLOUT | POLLWRNORM);
	}
	if (events & (POLLPRI | POLLRDBAND)) {
		if (so->so_oobmark || (so->so_state & SS_RCVATMARK))
			revents |= events & (POLLPRI | POLLRDBAND);
	}
	if (revents == 0) {
		if (events & (POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND)) {
			selrecord(p, &so->so_rcv.sb_sel);
			so->so_rcv.sb_flags |= SB_SEL;
		}
		if (events & (POLLOUT | POLLWRNORM)) {
			selrecord(p, &so->so_snd.sb_sel);
			so->so_snd.sb_flags |= SB_SEL;
		}
	}
	sounlock(s);
	return (revents);
}

int
soo_stat(struct file *fp, struct stat *ub, struct proc *p)
{
	struct socket *so = fp->f_data;
	int s;

	memset(ub, 0, sizeof (*ub));
	ub->st_mode = S_IFSOCK;
	s = solock(so);
	if ((so->so_state & SS_CANTRCVMORE) == 0 || so->so_rcv.sb_cc != 0)
		ub->st_mode |= S_IRUSR | S_IRGRP | S_IROTH;
	if ((so->so_state & SS_CANTSENDMORE) == 0)
		ub->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	ub->st_uid = so->so_euid;
	ub->st_gid = so->so_egid;
	(void) ((*so->so_proto->pr_usrreq)(so, PRU_SENSE,
	    (struct mbuf *)ub, NULL, NULL, p));
	sounlock(s);
	return (0);
}

int
soo_close(struct file *fp, struct proc *p)
{
	int error = 0;

	if (fp->f_data)
		error = soclose(fp->f_data);
	fp->f_data = 0;
	return (error);
}
