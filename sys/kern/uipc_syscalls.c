/*	$OpenBSD: uipc_syscalls.c,v 1.180 2018/11/19 16:12:06 tedu Exp $	*/
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
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/event.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <sys/pledge.h>
#include <sys/unpcb.h>
#include <sys/un.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <sys/domain.h>
#include <netinet/in.h>
#include <net/route.h>

/*
 * System call interface to the socket abstraction.
 */
extern	struct fileops socketops;

int	copyaddrout(struct proc *, struct mbuf *, struct sockaddr *, socklen_t,
	    socklen_t *);

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
	int type = SCARG(uap, type);
	int domain = SCARG(uap, domain);
	int fd, cloexec, nonblock, fflag, error;
	unsigned int ss = 0;

	if ((type & SOCK_DNS) && !(domain == AF_INET || domain == AF_INET6))
		return (EINVAL);

	if (ISSET(type, SOCK_DNS))
		ss |= SS_DNS;
	error = pledge_socket(p, domain, ss);
	if (error)
		return (error);

	type &= ~(SOCK_CLOEXEC | SOCK_NONBLOCK | SOCK_DNS);
	cloexec = (SCARG(uap, type) & SOCK_CLOEXEC) ? UF_EXCLOSE : 0;
	nonblock = SCARG(uap, type) & SOCK_NONBLOCK;
	fflag = FREAD | FWRITE | (nonblock ? FNONBLOCK : 0);

	error = socreate(SCARG(uap, domain), &so, type, SCARG(uap, protocol));
	if (error)
		return (error);

	KERNEL_LOCK();
	fdplock(fdp);
	error = falloc(p, &fp, &fd);
	if (error) {
		fdpunlock(fdp);
		soclose(so, MSG_DONTWAIT);
	} else {
		fp->f_flag = fflag;
		fp->f_type = DTYPE_SOCKET;
		fp->f_ops = &socketops;
		so->so_state |= ss;
		fp->f_data = so;
		fdinsert(fdp, fd, cloexec, fp);
		fdpunlock(fdp);
		FRELE(fp, p);
		*retval = fd;
	}
	KERNEL_UNLOCK();
	return (error);
}

static inline int
isdnssocket(struct socket *so)
{
	return (so->so_state & SS_DNS);
}

/* For SS_DNS sockets, only allow port DNS (port 53) */ 
static int
dns_portcheck(struct proc *p, struct socket *so, void *nam, u_int *namelen)
{
	int error = EINVAL;

	switch (so->so_proto->pr_domain->dom_family) {
	case AF_INET:
		if (*namelen < sizeof(struct sockaddr_in))
			break;
		if (((struct sockaddr_in *)nam)->sin_port == htons(53))
			error = 0;
		break;
#ifdef INET6
	case AF_INET6:
		if (*namelen < sizeof(struct sockaddr_in6))
			break;
		if (((struct sockaddr_in6 *)nam)->sin6_port == htons(53))
			error = 0;
#endif
	}
	if (error && p->p_p->ps_flags & PS_PLEDGE)
		return (pledge_fail(p, EPERM, PLEDGE_DNS));
	return error;
}

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
	struct socket *so;
	int s, error;

	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	so = fp->f_data;
	error = pledge_socket(p, so->so_proto->pr_domain->dom_family,
	    so->so_state);
	if (error)
		goto out;
	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME);
	if (error)
		goto out;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrsockaddr(p, mtod(nam, caddr_t), SCARG(uap, namelen));
#endif
	s = solock(so);
	error = sobind(so, nam, p);
	sounlock(so, s);
	m_freem(nam);
out:
	FRELE(fp, p);
	return (error);
}

int
sys_listen(struct proc *p, void *v, register_t *retval)
{
	struct sys_listen_args /* {
		syscallarg(int) s;
		syscallarg(int) backlog;
	} */ *uap = v;
	struct file *fp;
	struct socket *so;
	int error;

	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	so = fp->f_data;
	error = solisten(so, SCARG(uap, backlog));
	FRELE(fp, p);
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

	return (doaccept(p, SCARG(uap, s), SCARG(uap, name),
	    SCARG(uap, anamelen), SOCK_NONBLOCK_INHERIT, retval));
}

int
sys_accept4(struct proc *p, void *v, register_t *retval)
{
	struct sys_accept4_args /* {
		syscallarg(int) s;
		syscallarg(struct sockaddr *) name;
		syscallarg(socklen_t *) anamelen;
		syscallarg(socklen_t *) int flags;
	} */ *uap = v;

	if (SCARG(uap, flags) & ~(SOCK_CLOEXEC | SOCK_NONBLOCK))
		return (EINVAL);

	return (doaccept(p, SCARG(uap, s), SCARG(uap, name),
	    SCARG(uap, anamelen), SCARG(uap, flags), retval));
}

int
doaccept(struct proc *p, int sock, struct sockaddr *name, socklen_t *anamelen,
    int flags, register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct file *fp, *headfp;
	struct mbuf *nam;
	socklen_t namelen;
	int error, s, tmpfd;
	struct socket *head, *so;
	int cloexec, nflag;

	cloexec = (flags & SOCK_CLOEXEC) ? UF_EXCLOSE : 0;

	if (name && (error = copyin(anamelen, &namelen, sizeof (namelen))))
		return (error);
	if ((error = getsock(p, sock, &fp)) != 0)
		return (error);

	headfp = fp;

	fdplock(fdp);
	error = falloc(p, &fp, &tmpfd);
	fdpunlock(fdp);
	if (error) {
		FRELE(headfp, p);
		return (error);
	}

	nam = m_get(M_WAIT, MT_SONAME);

	head = headfp->f_data;
	s = solock(head);
	if (isdnssocket(head) || (head->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto out;
	}
	if ((headfp->f_flag & FNONBLOCK) && head->so_qlen == 0) {
		if (head->so_state & SS_CANTRCVMORE)
			error = ECONNABORTED;
		else
			error = EWOULDBLOCK;
		goto out;
	}
	while (head->so_qlen == 0 && head->so_error == 0) {
		if (head->so_state & SS_CANTRCVMORE) {
			head->so_error = ECONNABORTED;
			break;
		}
		error = sosleep(head, &head->so_timeo, PSOCK | PCATCH, "netcon",
		    0);
		if (error)
			goto out;
	}
	if (head->so_error) {
		error = head->so_error;
		head->so_error = 0;
		goto out;
	}

	/*
	 * Do not sleep after we have taken the socket out of the queue.
	 */
	so = TAILQ_FIRST(&head->so_q);
	if (soqremque(so, 1) == 0)
		panic("accept");

	/* Figure out whether the new socket should be non-blocking. */
	nflag = flags & SOCK_NONBLOCK_INHERIT ? (headfp->f_flag & FNONBLOCK)
	    : (flags & SOCK_NONBLOCK ? FNONBLOCK : 0);

	/* connection has been removed from the listen queue */
	KNOTE(&head->so_rcv.sb_sel.si_note, NOTE_SUBMIT);

	fp->f_type = DTYPE_SOCKET;
	fp->f_flag = FREAD | FWRITE | nflag;
	fp->f_ops = &socketops;
	fp->f_data = so;
	error = soaccept(so, nam);
	if (!error && name != NULL)
		error = copyaddrout(p, nam, name, namelen, anamelen);
out:
	if (!error) {
		sounlock(head, s);
		fdplock(fdp);
		fdinsert(fdp, tmpfd, cloexec, fp);
		fdpunlock(fdp);
		FRELE(fp, p);
		*retval = tmpfd;
	} else {
		sounlock(head, s);
		fdplock(fdp);
		fdremove(fdp, tmpfd);
		fdpunlock(fdp);
		closef(fp, p);
	}

	m_freem(nam);
	FRELE(headfp, p);
	return (error);
}

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
	int error, s, interrupted = 0;

	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	so = fp->f_data;
	s = solock(so);
	if (so->so_state & SS_ISCONNECTING) {
		error = EALREADY;
		goto out;
	}
	error = sockargs(&nam, SCARG(uap, name), SCARG(uap, namelen),
	    MT_SONAME);
	if (error)
		goto out;
	error = pledge_socket(p, so->so_proto->pr_domain->dom_family,
	    so->so_state);
	if (error)
		goto out;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrsockaddr(p, mtod(nam, caddr_t), SCARG(uap, namelen));
#endif

	if (isdnssocket(so)) {
		u_int namelen = nam->m_len;
		error = dns_portcheck(p, so, mtod(nam, void *), &namelen);
		if (error)
			goto out;
		nam->m_len = namelen;
	}

	error = soconnect(so, nam);
	if (error)
		goto bad;
	if ((fp->f_flag & FNONBLOCK) && (so->so_state & SS_ISCONNECTING)) {
		error = EINPROGRESS;
		goto out;
	}
	while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
		error = sosleep(so, &so->so_timeo, PSOCK | PCATCH,
		    "netcon2", 0);
		if (error) {
			if (error == EINTR || error == ERESTART)
				interrupted = 1;
			break;
		}
	}
	if (error == 0) {
		error = so->so_error;
		so->so_error = 0;
	}
bad:
	if (!interrupted)
		so->so_state &= ~SS_ISCONNECTING;
out:
	sounlock(so, s);
	FRELE(fp, p);
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
	int type, cloexec, nonblock, fflag, error, sv[2];

	type  = SCARG(uap, type) & ~(SOCK_CLOEXEC | SOCK_NONBLOCK);
	cloexec = (SCARG(uap, type) & SOCK_CLOEXEC) ? UF_EXCLOSE : 0;
	nonblock = SCARG(uap, type) & SOCK_NONBLOCK;
	fflag = FREAD | FWRITE | (nonblock ? FNONBLOCK : 0);

	error = socreate(SCARG(uap, domain), &so1, type, SCARG(uap, protocol));
	if (error)
		return (error);
	error = socreate(SCARG(uap, domain), &so2, type, SCARG(uap, protocol));
	if (error)
		goto free1;

	error = soconnect2(so1, so2);
	if (error != 0)
		goto free2;

	if ((SCARG(uap, type) & SOCK_TYPE_MASK) == SOCK_DGRAM) {
		/*
		 * Datagram socket connection is asymmetric.
		 */
		error = soconnect2(so2, so1);
		if (error != 0)
			goto free2;
	}
	KERNEL_LOCK();
	fdplock(fdp);
	if ((error = falloc(p, &fp1, &sv[0])) != 0)
		goto free3;
	fp1->f_flag = fflag;
	fp1->f_type = DTYPE_SOCKET;
	fp1->f_ops = &socketops;
	fp1->f_data = so1;
	if ((error = falloc(p, &fp2, &sv[1])) != 0)
		goto free4;
	fp2->f_flag = fflag;
	fp2->f_type = DTYPE_SOCKET;
	fp2->f_ops = &socketops;
	fp2->f_data = so2;
	error = copyout(sv, SCARG(uap, rsv), 2 * sizeof (int));
	if (error == 0) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrfds(p, sv, 2);
#endif
		fdinsert(fdp, sv[0], cloexec, fp1);
		fdinsert(fdp, sv[1], cloexec, fp2);
		fdpunlock(fdp);
		FRELE(fp1, p);
		FRELE(fp2, p);
		KERNEL_UNLOCK();
		return (0);
	}
	fdremove(fdp, sv[1]);
	closef(fp2, p);
	so2 = NULL;
free4:
	fdremove(fdp, sv[0]);
	closef(fp1, p);
	so1 = NULL;
free3:
	fdpunlock(fdp);
	KERNEL_UNLOCK();
free2:
	if (so2 != NULL)
		(void)soclose(so2, 0);
free1:
	if (so1 != NULL)
		(void)soclose(so1, 0);
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
	msg.msg_flags = 0;
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
#ifdef KTRACE
	if (KTRPOINT(p, KTR_STRUCT))
		ktrmsghdr(p, &msg);
#endif

	if (msg.msg_iovlen > IOV_MAX)
		return (EMSGSIZE);
	if (msg.msg_iovlen > UIO_SMALLIOV)
		iov = mallocarray(msg.msg_iovlen, sizeof(struct iovec),
		    M_IOV, M_WAITOK);
	else
		iov = aiov;
	if (msg.msg_iovlen &&
	    (error = copyin(msg.msg_iov, iov,
		    (unsigned)(msg.msg_iovlen * sizeof (struct iovec)))))
		goto done;
#ifdef KTRACE
	if (msg.msg_iovlen && KTRPOINT(p, KTR_STRUCT))
		ktriovec(p, iov, msg.msg_iovlen);
#endif
	msg.msg_iov = iov;
	msg.msg_flags = 0;
	error = sendit(p, SCARG(uap, s), &msg, SCARG(uap, flags), retval);
done:
	if (iov != aiov)
		free(iov, M_IOV, sizeof(struct iovec) * msg.msg_iovlen);
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
	struct socket *so;
	size_t len;
	int error;
#ifdef KTRACE
	struct iovec *ktriov = NULL;
	int iovlen = 0;
#endif

	to = NULL;

	if ((error = getsock(p, s, &fp)) != 0)
		return (error);
	so = fp->f_data;
	if (fp->f_flag & FNONBLOCK)
		flags |= MSG_DONTWAIT;

	error = pledge_sendit(p, mp->msg_name);
	if (error)
		goto bad;

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
		if (isdnssocket(so)) {
			u_int namelen = mp->msg_namelen;
			error = dns_portcheck(p, so, mtod(to, caddr_t),
			    &namelen);
			if (error)
				goto bad;
			mp->msg_namelen = namelen;
		}
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
		 	ktrsockaddr(p, mtod(to, caddr_t), mp->msg_namelen);
#endif
	}
	if (mp->msg_control) {
		if (mp->msg_controllen < CMSG_ALIGN(sizeof(struct cmsghdr))) {
			error = EINVAL;
			goto bad;
		}
		error = sockargs(&control, mp->msg_control,
		    mp->msg_controllen, MT_CONTROL);
		if (error)
			goto bad;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT) && mp->msg_controllen)
			ktrcmsghdr(p, mtod(control, char *),
			    mp->msg_controllen);
#endif
	} else
		control = 0;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_GENIO)) {
		ktriov = mallocarray(auio.uio_iovcnt, sizeof(struct iovec),
		    M_TEMP, M_WAITOK);
		iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		memcpy(ktriov, auio.uio_iov, iovlen);
	}
#endif
	len = auio.uio_resid;
	error = sosend(so, to, &auio, NULL, control, flags);
	if (error) {
		if (auio.uio_resid != len && (error == ERESTART ||
		    error == EINTR || error == EWOULDBLOCK))
			error = 0;
		if (error == EPIPE && (flags & MSG_NOSIGNAL) == 0) {
			KERNEL_LOCK();
			ptsignal(p, SIGPIPE, STHREAD);
			KERNEL_UNLOCK();
		}
	}
	if (error == 0) {
		*retsize = len - auio.uio_resid;
		mtx_enter(&fp->f_mtx);
		fp->f_wxfer++;
		fp->f_wbytes += *retsize;
		mtx_leave(&fp->f_mtx);
	}
#ifdef KTRACE
	if (ktriov != NULL) {
		if (error == 0)
			ktrgenio(p, s, UIO_WRITE, ktriov, *retsize);
		free(ktriov, M_TEMP, iovlen);
	}
#endif
bad:
	FRELE(fp, p);
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
		iov = mallocarray(msg.msg_iovlen, sizeof(struct iovec),
		    M_IOV, M_WAITOK);
	else
		iov = aiov;
	msg.msg_flags = SCARG(uap, flags);
	if (msg.msg_iovlen > 0) {
		error = copyin(msg.msg_iov, iov,
		    msg.msg_iovlen * sizeof(struct iovec));
		if (error)
			goto done;
	}
	uiov = msg.msg_iov;
	msg.msg_iov = iov;
	if ((error = recvit(p, SCARG(uap, s), &msg, NULL, retval)) == 0) {
		msg.msg_iov = uiov;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT)) {
			ktrmsghdr(p, &msg);
			if (msg.msg_iovlen)
				ktriovec(p, iov, msg.msg_iovlen);
		}
#endif
		error = copyout(&msg, SCARG(uap, msg), sizeof(msg));
	}
done:
	if (iov != aiov)
		free(iov, M_IOV, sizeof(struct iovec) * msg.msg_iovlen);
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
	int iovlen = 0;
#endif

	if ((error = getsock(p, s, &fp)) != 0)
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
		ktriov = mallocarray(auio.uio_iovcnt, sizeof(struct iovec),
		    M_TEMP, M_WAITOK);
		iovlen = auio.uio_iovcnt * sizeof (struct iovec);

		memcpy(ktriov, auio.uio_iov, iovlen);
	}
#endif
	len = auio.uio_resid;
	if (fp->f_flag & FNONBLOCK)
		mp->msg_flags |= MSG_DONTWAIT;
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
			ktrgenio(p, s, UIO_READ, ktriov, len - auio.uio_resid);
		free(ktriov, M_TEMP, iovlen);
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
			alen = from->m_len;
			error = copyout(mtod(from, caddr_t), mp->msg_name,
			    MIN(alen, mp->msg_namelen));
			if (error)
				goto out;
#ifdef KTRACE
			if (KTRPOINT(p, KTR_STRUCT))
				ktrsockaddr(p, mtod(from, caddr_t), alen);
#endif
		}
		mp->msg_namelen = alen;
		if (namelenp &&
		    (error = copyout(&alen, namelenp, sizeof(alen)))) {
			goto out;
		}
	}
	if (mp->msg_control) {
		len = mp->msg_controllen;
		if (len <= 0 || control == NULL)
			len = 0;
		else {
			struct mbuf *m = control;
			caddr_t cp = mp->msg_control;

			do {
				i = m->m_len;
				if (len < i) {
					mp->msg_flags |= MSG_CTRUNC;
					i = len;
				}
				error = copyout(mtod(m, caddr_t), cp, i);
#ifdef KTRACE
				if (KTRPOINT(p, KTR_STRUCT) && error == 0 && i)
					ktrcmsghdr(p, mtod(m, char *), i);
#endif
				if (m->m_next)
					i = ALIGN(i);
				cp += i;
				len -= i;
				if (error != 0 || len <= 0)
					break;
			} while ((m = m->m_next) != NULL);
			len = cp - (caddr_t)mp->msg_control;
		}
		mp->msg_controllen = len;
	}
	if (!error) {
		mtx_enter(&fp->f_mtx);
		fp->f_rxfer++;
		fp->f_rbytes += *retsize;
		mtx_leave(&fp->f_mtx);
	}
out:
	FRELE(fp, p);
	m_freem(from);
	m_freem(control);
	return (error);
}

int
sys_shutdown(struct proc *p, void *v, register_t *retval)
{
	struct sys_shutdown_args /* {
		syscallarg(int) s;
		syscallarg(int) how;
	} */ *uap = v;
	struct file *fp;
	int error;

	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = soshutdown(fp->f_data, SCARG(uap, how));
	FRELE(fp, p);
	return (error);
}

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
	struct socket *so;
	int s, error;


	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = pledge_sockopt(p, 1, SCARG(uap, level), SCARG(uap, name));
	if (error)
		goto bad;
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
	so = fp->f_data;
	s = solock(so);
	error = sosetopt(so, SCARG(uap, level), SCARG(uap, name), m);
	sounlock(so, s);
bad:
	m_freem(m);
	FRELE(fp, p);
	return (error);
}

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
	struct socket *so;
	int s, error;

	if ((error = getsock(p, SCARG(uap, s), &fp)) != 0)
		return (error);
	error = pledge_sockopt(p, 0, SCARG(uap, level), SCARG(uap, name));
	if (error)
		goto out;
	if (SCARG(uap, val)) {
		error = copyin(SCARG(uap, avalsize),
		    &valsize, sizeof (valsize));
		if (error)
			goto out;
	} else
		valsize = 0;
	m = m_get(M_WAIT, MT_SOOPTS);
	so = fp->f_data;
	s = solock(so);
	error = sogetopt(so, SCARG(uap, level), SCARG(uap, name), m);
	sounlock(so, s);
	if (error == 0 && SCARG(uap, val) && valsize && m != NULL) {
		if (valsize > m->m_len)
			valsize = m->m_len;
		error = copyout(mtod(m, caddr_t), SCARG(uap, val), valsize);
		if (error == 0)
			error = copyout(&valsize,
			    SCARG(uap, avalsize), sizeof (valsize));
	}
	m_free(m);
out:
	FRELE(fp, p);
	return (error);
}

/*
 * Get socket name.
 */
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
	int error, s;

	if ((error = getsock(p, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	error = copyin(SCARG(uap, alen), &len, sizeof (len));
	if (error)
		goto bad;
	so = fp->f_data;
	error = pledge_socket(p, -1, so->so_state);
	if (error)
		goto bad;
	m = m_getclr(M_WAIT, MT_SONAME);
	s = solock(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_SOCKADDR, 0, m, 0, p);
	sounlock(so, s);
	if (error)
		goto bad;
	error = copyaddrout(p, m, SCARG(uap, asa), len, SCARG(uap, alen));
bad:
	FRELE(fp, p);
	m_freem(m);
	return (error);
}

/*
 * Get name of peer for connected socket.
 */
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
	int error, s;

	if ((error = getsock(p, SCARG(uap, fdes), &fp)) != 0)
		return (error);
	so = fp->f_data;
	error = pledge_socket(p, -1, so->so_state);
	if (error)
		goto bad;
	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	error = copyin(SCARG(uap, alen), &len, sizeof (len));
	if (error)
		goto bad;
	m = m_getclr(M_WAIT, MT_SONAME);
	s = solock(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_PEERADDR, 0, m, 0, p);
	sounlock(so, s);
	if (error)
		goto bad;
	error = copyaddrout(p, m, SCARG(uap, asa), len, SCARG(uap, alen));
bad:
	FRELE(fp, p);
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
	 * Memory for sa_len and sa_family must exist.
	 */
	if ((buflen > (type == MT_SONAME ? UCHAR_MAX : MCLBYTES)) ||
	    (type == MT_SONAME && buflen < offsetof(struct sockaddr, sa_data)))
		return (EINVAL);

	/* Allocate an mbuf to hold the arguments. */
	m = m_get(M_WAIT, type);
	if (buflen > MLEN) {
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
		sa->sa_len = buflen;
	}
	return (0);
}

int
getsock(struct proc *p, int fdes, struct file **fpp)
{
	struct file *fp;

	fp = fd_getfile(p->p_fd, fdes);
	if (fp == NULL)
		return (EBADF);
	if (fp->f_type != DTYPE_SOCKET) {
		FRELE(fp, p);
		return (ENOTSOCK);
	}
	*fpp = fp;

	return (0);
}

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
	if (p->p_p->ps_rtableid != 0 && (error = suser(p)) != 0)
		return (error);
	if (rtableid < 0 || !rtable_exists((u_int)rtableid))
		return (EINVAL);

	p->p_p->ps_rtableid = (u_int)rtableid;
	return (0);
}

int
sys_getrtable(struct proc *p, void *v, register_t *retval)
{
	*retval = (int)p->p_p->ps_rtableid;
	return (0);
}

int
copyaddrout(struct proc *p, struct mbuf *name, struct sockaddr *sa,
    socklen_t buflen, socklen_t *outlen)
{
	int error;
	socklen_t namelen = name->m_len;

	/* SHOULD COPY OUT A CHAIN HERE */
	error = copyout(mtod(name, caddr_t), sa, MIN(buflen, namelen));
	if (error == 0) {
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrsockaddr(p, mtod(name, caddr_t), namelen);
#endif
		error = copyout(&namelen, outlen, sizeof(*outlen));
	}

	return (error);
}
