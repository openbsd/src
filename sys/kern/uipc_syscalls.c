/*	$OpenBSD: uipc_syscalls.c,v 1.80 2011/07/08 05:01:27 matthew Exp $	*/
/*	$NetBSD: uipc_syscalls.c,v 1.19 1996/02/09 19:00:48 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
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
 *	@(#)uipc_syscalls.c	8.4 (Berkeley) 2/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/event.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/unpcb.h>
#include <sys/un.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <net/route.h>

/*
 * System call interface to the socket abstraction.
 */
extern	struct fileops socketops;

int
sys_socket(struct proc *p, void *v, register_t *retval)
{
	struct sys_socket_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct socket *so;
	struct file *fp;
	int fd, error;

	fdplock(fdp);

	if ((error = falloc(p, &fp, &fd)) != 0)
		goto out;
	fp->f_flag = FREAD|FWRITE;
	fp->f_type = DTYPE_SOCKET;
	fp->f_ops = &socketops;
	error = socreate(SCARG(uap, domain), &so, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error) {
		fdremove(fdp, fd);
		closef(fp, p);
	} else {
		fp->f_data = so;
		FILE_SET_MATURE(fp);
		*retval = fd;
	}
out:
	fdpunlock(fdp);
	return (error);
}

/* ARGSUSED */
int
sys_bind(struct proc *p, void *v, register_t *retval)
{
	struct sys_bind_args /* {
		syscallarg(int) s;
		syscallarg(const struct sockaddr *) name;
		syscallarg(socklen_t) namelen;
	} */ *uap = v;
	struct file *fp;
	struct mbuf *nam;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
			 MT_SONAME);
	if (error == 0) {
		error = sobind(fp->f_data, nam, p);
		m_freem(nam);
	}
	FRELE(fp);
	return (error);
}

/* ARGSUSED */
int
sys_listen(struct proc *p, void *v, register_t *retval)
{
	struct sys_listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap = v;
	struct file *fp;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = solisten(fp->f_data, SCARG(uap, backlog));
	FRELE(fp);
	return (error);
}

int
sys_accept(struct proc *p, void *v, register_t *retval)
{
	struct sys_accept_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) name;
		syscallarg(socklen_t *) anamelen;
	} */ *uap = v;
	struct file *fp, *headfp;
	struct mbuf *nam;
	socklen_t namelen;
	int error, s, tmpfd;
	struct socket *head, *so;
	int nflag;

	if (SCARG(uap, name) && (error = copyin(SCARG(uap, anamelen),
	    &namelen, sizeof (namelen))))
		return (error);
	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	headfp = fp;
	s = splsoftnet();
	head = fp->f_data;
	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto bad;
	}
	if ((head->so_state & SS_NBIO) && head->so_qlen == 0) {
		if (head->so_state & SS_CANTRCVMORE)
			error = ECONNABORTED;
		else
			error = EWOULDBLOCK;
		goto bad;
	}
	while (head->so_qlen == 0 && head->so_error == 0) {
		if (head->so_state & SS_CANTRCVMORE) {
			head->so_error = ECONNABORTED;
			break;
		}
		error = tsleep(&head->so_timeo, PSOCK | PCATCH, "netcon", 0);
		if (error) {
			goto bad;
		}
	}
	if (head->so_error) {
		error = head->so_error;
		head->so_error = 0;
		goto bad;
	}
	
	/*
	 * At this point we know that there is at least one connection
	 * ready to be accepted. Remove it from the queue prior to
	 * allocating the file descriptor for it since falloc() may
	 * block allowing another process to accept the connection
	 * instead.
	 */
	so = TAILQ_FIRST(&head->so_q);
	if (soqremque(so, 1) == 0)
		panic("accept");

	/* Take note if socket was non-blocking. */
	nflag = (fp->f_flag & FNONBLOCK);

	fdplock(p->p_fd);
	if ((error = falloc(p, &fp, &tmpfd)) != 0) {
		/*
		 * Probably ran out of file descriptors. Put the
		 * unaccepted connection back onto the queue and
		 * do another wakeup so some other process might
		 * have a chance at it.
		 */
		soqinsque(head, so, 1);
		wakeup_one(&head->so_timeo);
		goto unlock;
	}
	*retval = tmpfd;

	/* connection has been removed from the listen queue */
	KNOTE(&head->so_rcv.sb_sel.si_note, 0);

	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = FREAD | FWRITE | nflag;
	fp->f_ops = &socketops;
	fp->f_data = so;
	nam = m_get(M_WAIT, MT_SONAME);
	error = soaccept(so, nam);
	if (!error && SCARG(uap, name)) {
		if (namelen > nam->m_len)
			namelen = nam->m_len;
		/* SHOULD COPY OUT A CHAIN HERE */
		if ((error = copyout(mtod(nam, caddr_t),
		    SCARG(uap, name), namelen)) == 0)
			error = copyout(&namelen, SCARG(uap, anamelen),
			    sizeof (*SCARG(uap, anamelen)));
	}
	/* if an error occurred, free the file descriptor */
	if (error) {
		fdremove(p->p_fd, tmpfd);
		closef(fp, p);
	} else {
		FILE_SET_MATURE(fp);
	}
	m_freem(nam);
unlock:
	fdpunlock(p->p_fd);
bad:
	splx(s);
	FRELE(headfp);
	return (error);
}

/* ARGSUSED */
int
sys_connect(struct proc *p, void *v, register_t *retval)
{
	struct sys_connect_args /* {
		syscallarg(int) s;
		syscallarg(const struct sockaddr *) name;
		syscallarg(socklen_t) namelen;
	} */ *uap = v;
	struct file *fp;
	struct socket *so;
	struct mbuf *nam = NULL;
	int error, s;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	so = fp->f_data;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		FRELE(fp);
		return (EALREADY);
	}
	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
			 MT_SONAME);
	if (error)
		goto bad;
	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((so->so_state & SS_NBIO) && (so->so_state & SS_ISCONNECTING)) {
		FRELE(fp);
		m_freem(nam);
		return (EINPROGRESS);
	}
	s = splsoftnet();
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = tsleep(&so->so_timeo, PSOCK | PCATCH,
		    "netcon2", 0);
		if (error)
			break;
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
	splx(s);
bad:
	so->so_state &= ~SS_ISCONNECTING;
	FRELE(fp);
	if (nam)
		m_freem(nam);
	if (error == ERESTART)
		error = EINTR;
	return (error);
}

int
sys_socketpair(struct proc *p, void *v, register_t *retval)
{
	struct sys_socketpair_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
		syscallarg(int *) rsv;
	} */ *uap = v;
	struct filedesc *fdp = p->p_fd;
	struct file *fp1, *fp2;
	struct socket *so1, *so2;
	int fd, error, sv[2];

	error = socreate(SCARG(uap, domain), &so1, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error)
		return (error);
	error = socreate(SCARG(uap, domain), &so2, SCARG(uap, type),
			 SCARG(uap, protocol));
	if (error)
		goto free1;

	fdplock(fdp);
	if ((error = falloc(p, &fp1, &fd)) != 0)
		goto free2;
	sv[0] = fd;
	fp1->f_flag = FREAD|FWRITE;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = so1;
	if ((error = falloc(p, &fp2, &fd)) != 0)
		goto free3;
	fp2->f_flag = FREAD|FWRITE;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = so2;
	sv[1] = fd;
	if ((error = soconnect2(so1, so2)) != 0)
		goto free4;
	if (SCARG(uap, type) == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		 if ((error = soconnect2(so2, so1)) != 0)
			goto free4;
	}
	error = copyout(sv, SCARG(uap, rsv), 2 * sizeof (int));
	if (error == 0) {
		FILE_SET_MATURE(fp1);
		FILE_SET_MATURE(fp2);
		fdpunlock(fdp);
		return (0);
	}
free4:
	fdremove(fdp, sv[1]);
	closef(fp2, p);
	so2 = NULL;
free3:
	fdremove(fdp, sv[0]);
	closef(fp1, p);
	so1 = NULL;
free2:
	if (so2 != NULL)
		(void)soclose(so2);
	fdpunlock(fdp);
free1:
	if (so1 != NULL)
		(void)soclose(so1);
	return (error);
}

int
sys_sendto(struct proc *p, void *v, register_t *retval)
{
	struct sys_sendto_args /* {
		syscallarg(int) s;
		syscallarg(const void *) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(const struct sockaddr *) to;
		syscallarg(socklen_t) tolen;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov;

	msg.msg_name = (caddr_t)SCARG(uap, to);
	msg.msg_namelen = SCARG(uap, tolen);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	msg.msg_control = 0;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	aiov.iov_base = (char *)SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	return (sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval));
}

int
sys_sendmsg(struct proc *p, void *v, register_t *retval)
{
	struct sys_sendmsg_args /* {
		syscallarg(int) s;
		syscallarg(const struct msghdr *) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *iov;
	int error;

	error = copyin(SCARG(uap, msg), &msg, sizeof (msg));
	if (error)
		return (error);
	if (msg.msg_iovlen > IOV_MAX)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		iov = malloc(sizeof(struct iovec) * msg.msg_iovlen,
		    M_IOV, M_WAITOK);
	else
		iov = aiov;
	if (msg.msg_iovlen &&
	    (error = copyin(msg.msg_iov, iov,
		    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)))))
		goto done;
	msg.msg_iov = iov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = 0;
#endif
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		free(iov, M_IOV);
	return (error);
}

int
sendit(struct proc *p, int s, struct msghdr *mp, int flags, register_t *retsize)
{
	struct file *fp;
	struct uio auio;
	struct iovec *iov;
	int i;
	struct mbuf *to, *control;
	size_t len;
	int error;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	to = NULL;

	if ((error = getsock(p->p_fd, s, &fp)) != 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_procp = p;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if (iov->iov_len > SSIZE_MAX ||
		    (auio.uio_resid += iov->iov_len) > SSIZE_MAX) {
			error = EINVAL;
			goto bad;
		}
	}
	if (mp->msg_name) {
		error = sockargs(&to, mp->msg_name, mp->msg_namelen,
				 MT_SONAME);
		if (error)
			goto bad;
	}
	if (mp->msg_control) {
		if (mp->msg_controllen < CMSG_ALIGN(sizeof(struct cmsghdr))
#ifdef COMPAT_OLDSOCK
		    && mp->msg_flags != MSG_COMPAT
#endif
		) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
				 mp->msg_controllen, MT_CONTROL);
		if (error)
			goto bad;
#ifdef COMPAT_OLDSOCK
		if (mp->msg_flags == MSG_COMPAT) {
			struct cmsghdr *cm;

			M_PREPEND(control, sizeof(*cm), M_WAIT);
			cm = mtod(control, struct cmsghdr *);
			cm->cmsg_len = control->m_len;
			cm->cmsg_level = SOL_SOCKET;
			cm->cmsg_type = SCM_RIGHTS;
		}
#endif
	} else
		control = 0;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		bcopy(auio.uio_iov, ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	error = sosend(fp->f_data, to, &auio, NULL, control, flags);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE)
			ptsignal(p, SIGPIPE, STHREAD);
	}
	if (error == 0) {
		*retsize = len - auio.uio_resid;
		fp->f_wxfer++;
		fp->f_wbytes += *retsize;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, s, UIO_WRITE, ktriov, *retsize, error);
		free(ktriov, M_TEMP);
	}
#endif
bad:
	FRELE(fp);
	if (to)
		m_freem(to);
	return (error);
}

int
sys_recvfrom(struct proc *p, void *v, register_t *retval)
{
	struct sys_recvfrom_args /* {
		syscallarg(int) s;
		syscallarg(void *) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(struct sockaddr *) from;
		syscallarg(socklen_t *) fromlenaddr;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov;
	int error;

	if (SCARG(uap, fromlenaddr)) {
		error = copyin(SCARG(uap, fromlenaddr),
		    &msg.msg_namelen, sizeof (msg.msg_namelen));
		if (error)
			return (error);
	} else
		msg.msg_namelen = 0;
	msg.msg_name = (caddr_t)SCARG(uap, from);
	msg.msg_iov = &aiov;
	msg.msg_iovlen = 1;
	aiov.iov_base = SCARG(uap, buf);
	aiov.iov_len = SCARG(uap, len);
	msg.msg_control = 0;
	msg.msg_flags = SCARG(uap, flags);
	return (recvit(p, SCARG(uap, s), &msg,
	    (caddr_t)SCARG(uap, fromlenaddr), retval));
}

int
sys_recvmsg(struct proc *p, void *v, register_t *retval)
{
	struct sys_recvmsg_args /* {
		syscallarg(int) s;
		syscallarg(struct msghdr *) msg;
		syscallarg(int) flags;
	} */ *uap = v;
	struct msghdr msg;
	struct iovec aiov[UIO_SMALLIOV], *uiov, *iov;
	int error;

	error = copyin(SCARG(uap, msg), &msg, sizeof (msg));
	if (error)
		return (error);
	if (msg.msg_iovlen > IOV_MAX)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		iov = malloc(sizeof(struct iovec) * msg.msg_iovlen,
		    M_IOV, M_WAITOK);
	else
		iov = aiov;
#ifdef COMPAT_OLDSOCK
	msg.msg_flags = SCARG(uap, flags) &~ MSG_COMPAT;
#else
	msg.msg_flags = SCARG(uap, flags);
#endif
	if (msg.msg_iovlen > 0) {
		error = copyin(msg.msg_iov, iov,
		    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)));
		if (error)
			goto done;
	}
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	if ((error = recvit(p, SCARG(uap, s), &msg, NULL, retval)) == 0) {
		msg.msg_iov = uiov;
		error = copyout(&msg, SCARG(uap, msg), sizeof(msg));
	}
done:
	if (iov != aiov)
		free(iov, M_IOV);
	return (error);
}

int
recvit(struct proc *p, int s, struct msghdr *mp, caddr_t namelenp,
    register_t *retsize)
{
	struct file *fp;
	struct uio auio;
	struct iovec *iov;
	int i;
	size_t len;
	int error;
	struct mbuf *from = NULL, *control = NULL;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
#endif

	if ((error = getsock(p->p_fd, s, &fp)) != 0)
		return (error);
	auio.uio_iov = mp->msg_iov;
	auio.uio_iovcnt = mp->msg_iovlen;
	auio.uio_segflg = UIO_USERSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_procp = p;
	auio.uio_offset = 0;			/* XXX */
	auio.uio_resid = 0;
	iov = mp->msg_iov;
	for (i = 0; i < mp->msg_iovlen; i++, iov++) {
		/* Don't allow sum > SSIZE_MAX */
		if (iov->iov_len > SSIZE_MAX ||
		    (auio.uio_resid += iov->iov_len) > SSIZE_MAX) {
			error = EINVAL;
			goto out;
		}
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		int iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		ktriov = malloc(iovlen, M_TEMP, M_WAITOK);
		bcopy(auio.uio_iov, ktriov, iovlen);
	}
#endif
	len = auio.uio_resid;
	error = soreceive(fp->f_data, &from, &auio, NULL,
			  mp->msg_control ? &control : NULL,
			  &mp->msg_flags,
			  mp->msg_control ? mp->msg_controllen : 0);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, s, UIO_READ,
				ktriov, len - auio.uio_resid, error);
		free(ktriov, M_TEMP);
	}
#endif
	if (error)
		goto out;
	*retsize = len - auio.uio_resid;
	if (mp->msg_name) {
		socklen_t alen;

		if (from == NULL)
			alen = 0;
		else {
			/* save sa_len before it is destroyed by MSG_COMPAT */
			alen = mp->msg_namelen;
			if (alen > from->m_len)
				alen = from->m_len;
			/* else if alen < from->m_len ??? */
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				mtod(from, struct osockaddr *)->sa_family =
				    mtod(from, struct sockaddr *)->sa_family;
#endif
			error = copyout(mtod(from, caddr_t), mp->msg_name, alen);
			if (error)
				goto out;
		}
		mp->msg_namelen = alen;
		if (namelenp &&
		    (error = copyout(&alen, namelenp, sizeof(alen)))) {
#ifdef COMPAT_OLDSOCK
			if (mp->msg_flags & MSG_COMPAT)
				error = 0;	/* old recvfrom didn't check */
			else
#endif
			goto out;
		}
	}
	if (mp->msg_control) {
#ifdef COMPAT_OLDSOCK
		/*
		 * We assume that old recvmsg calls won't receive access
		 * rights and other control info, esp. as control info
		 * is always optional and those options didn't exist in 4.3.
		 * If we receive rights, trim the cmsghdr; anything else
		 * is tossed.
		 */
		if (control && mp->msg_flags & MSG_COMPAT) {
			if (mtod(control, struct cmsghdr *)->cmsg_level !=
			    SOL_SOCKET ||
			    mtod(control, struct cmsghdr *)->cmsg_type !=
			    SCM_RIGHTS) {
				mp->msg_controllen = 0;
				goto out;
			}
			control->m_len -= sizeof (struct cmsghdr);
			control->m_data += sizeof (struct cmsghdr);
		}
#endif
		len = mp->msg_controllen;
		if (len <= 0 || control == NULL)
			len = 0;
		else {
			struct mbuf *m = control;
			caddr_t p = mp->msg_control;

			do {
				i = m->m_len;
				if (len < i) {
					mp->msg_flags |= MSG_CTRUNC;
					i = len;
				}
				error = copyout(mtod(m, caddr_t), p,
				    (unsigned)i);
				if (m->m_next)
					i = ALIGN(i);
				p += i;
				len -= i;
				if (error != 0 || len <= 0)
					break;
			} while ((m = m->m_next) != NULL);
			len = p - (caddr_t)mp->msg_control;
		}
		mp->msg_controllen = len;
	}
	if (!error) {
		fp->f_rxfer++;
		fp->f_rbytes += *retsize;
	}
out:
	FRELE(fp);
	if (from)
		m_freem(from);
	if (control)
		m_freem(control);
	return (error);
}

/* ARGSUSED */
int
sys_shutdown(struct proc *p, void *v, register_t *retval)
{
	struct sys_shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap = v;
	struct file *fp;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = soshutdown(fp->f_data, SCARG(uap, how));
	FRELE(fp);
	return (error);
}

/* ARGSUSED */
int
sys_setsockopt(struct proc *p, void *v, register_t *retval)
{
	struct sys_setsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(const void *) val;
		syscallarg(socklen_t) valsize;
	} */ *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	if (SCARG(uap, valsize) > MCLBYTES) {
		error = EINVAL;
		goto bad;
	}
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (SCARG(uap, valsize) > MLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				error = ENOBUFS;
				goto bad;
			}
		}
		if (m == NULL) {
			error = ENOBUFS;
			goto bad;
		}
		error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    SCARG(uap, valsize));
		if (error) {
			goto bad;
		}
		m->m_len = SCARG(uap, valsize);
	}
	error = sosetopt(fp->f_data, SCARG(uap, level),
			 SCARG(uap, name), m);
	m = NULL;
bad:
	if (m)
		m_freem(m);
	FRELE(fp);
	return (error);
}

/* ARGSUSED */
int
sys_getsockopt(struct proc *p, void *v, register_t *retval)
{
	struct sys_getsockopt_args /* {
		syscallarg(int) s;
		syscallarg(int) level;
		syscallarg(int) name;
		syscallarg(void *) val;
		syscallarg(socklen_t *) avalsize;
	} */ *uap = v;
	struct file *fp;
	struct mbuf *m = NULL;
	socklen_t valsize;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, s), &fp)) != 0)
		return (error);
	if (SCARG(uap, val)) {
		error = copyin(SCARG(uap, avalsize),
		    &valsize, sizeof (valsize));
		if (error)
			goto out;
	} else
		valsize = 0;
	if ((error = sogetopt(fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), &m)) == 0 && SCARG(uap, val) && valsize &&
	    m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), SCARG(uap, val), valsize);
		if (error == 0)
			error = copyout(&valsize,
			    SCARG(uap, avalsize), sizeof (valsize));
	}
out:
	FRELE(fp);
	if (m != NULL)
		(void)m_free(m);
	return (error);
}

/*
 * Get socket name.
 */
/* ARGSUSED */
int
sys_getsockname(struct proc *p, void *v, register_t *retval)
{
	struct sys_getsockname_args /* {
		syscallarg(int) fdes;
		syscallarg(struct sockaddr *) asa;
		syscallarg(socklen_t *) alen;
	} */ *uap = v;
	struct file *fp;
	struct socket *so;
	struct mbuf *m = NULL;
	socklen_t len;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	error = copyin(SCARG(uap, alen), &len, sizeof (len));
	if (error)
		goto bad;
	so = fp->f_data;
	m = m_getclr(M_WAIT, MT_SONAME);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0, p);
	if (error)
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
	error = copyout(mtod(m, caddr_t), SCARG(uap, asa), len);
	if (error == 0)
		error = copyout(&len, SCARG(uap, alen), sizeof (len));
bad:
	FRELE(fp);
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Get name of peer for connected socket.
 */
/* ARGSUSED */
int
sys_getpeername(struct proc *p, void *v, register_t *retval)
{
	struct sys_getpeername_args /* {
		syscallarg(int) fdes;
		syscallarg(struct sockaddr *) asa;
		syscallarg(socklen_t *) alen;
	} */ *uap = v;
	struct file *fp;
	struct socket *so;
	struct mbuf *m = NULL;
	socklen_t len;
	int error;

	if ((error = getsock(p->p_fd, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	so = fp->f_data;
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0) {
		FRELE(fp);
		return (ENOTCONN);
	}
	error = copyin(SCARG(uap, alen), &len, sizeof (len));
	if (error)
		goto bad;
	m = m_getclr(M_WAIT, MT_SONAME);
	error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0, p);
	if (error)
		goto bad;
	if (len > m->m_len)
		len = m->m_len;
	error = copyout(mtod(m, caddr_t), SCARG(uap, asa), len);
	if (error == 0)
		error = copyout(&len, SCARG(uap, alen), sizeof (len));
bad:
	FRELE(fp);
	m_freem(m);
	return (error);
}

int
sockargs(struct mbuf **mp, const void *buf, size_t buflen, int type)
{
	struct sockaddr *sa;
	struct mbuf *m;
	int error;

	/*
	 * We can't allow socket names > UCHAR_MAX in length, since that
	 * will overflow sa_len. Also, control data more than MCLBYTES in
	 * length is just too much.
	 */
	if (buflen > (type == MT_SONAME ? UCHAR_MAX : MCLBYTES))
		return (EINVAL);

	/* Allocate an mbuf to hold the arguments. */
	m = m_get(M_WAIT, type);
	if ((u_int)buflen > MLEN) {
		MCLGET(m, M_WAITOK);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			return ENOBUFS;
		}
	}
	m->m_len = buflen;
	error = copyin(buf, mtod(m, caddr_t), buflen);
	if (error) {
		(void) m_free(m);
		return (error);
	}
	*mp = m;
	if (type == MT_SONAME) {
		sa = mtod(m, struct sockaddr *);
#if defined(COMPAT_OLDSOCK) && BYTE_ORDER != BIG_ENDIAN
		if (sa->sa_family == 0 && sa->sa_len < AF_MAX)
			sa->sa_family = sa->sa_len;
#endif
		sa->sa_len = buflen;
	}
	return (0);
}

int
getsock(struct filedesc *fdp, int fdes, struct file **fpp)
{
	struct file *fp;

	if ((fp = fd_getfile(fdp, fdes)) == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET)
		return (ENOTSOCK);
	*fpp = fp;
	FREF(fp);

	return (0);
}

/* ARGSUSED */
int
sys_setrtable(struct proc *p, void *v, register_t *retval)
{
	struct sys_setrtable_args /* {
		syscallarg(int) rtableid;
	} */ *uap = v;
	int rtableid, error;

	rtableid = SCARG(uap, rtableid);

	if (p->p_p->ps_rtableid == (u_int)rtableid)
		return (0);
	if (p->p_p->ps_rtableid != 0 && (error = suser(p, 0)) != 0)
		return (error);
	if (rtableid < 0 || !rtable_exists((u_int)rtableid))
		return (EINVAL);

	p->p_p->ps_rtableid = (u_int)rtableid;
	return (0);
}

/* ARGSUSED */
int
sys_getrtable(struct proc *p, void *v, register_t *retval)
{
	*retval = (int)p->p_p->ps_rtableid;
	return (0);
}
