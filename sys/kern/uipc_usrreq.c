/*	$OpenBSD: uipc_usrreq.c,v 1.78 2014/11/03 03:08:00 deraadt Exp $	*/
/*	$NetBSD: uipc_usrreq.c,v 1.18 1996/02/09 19:00:50 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
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
 *	@(#)uipc_usrreq.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/unpcb.h>
#include <sys/un.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mbuf.h>

void	uipc_setaddr(const struct unpcb *, struct mbuf *);

/*
 * Unix communications domain.
 *
 * TODO:
 *	RDM
 *	rethink name space problems
 *	need a proper out-of-band
 */
struct	sockaddr sun_noname = { sizeof(sun_noname), AF_UNIX };
ino_t	unp_ino;			/* prototype for fake inode numbers */

void
uipc_setaddr(const struct unpcb *unp, struct mbuf *nam)
{
	if (unp != NULL && unp->unp_addr != NULL) {
		nam->m_len = unp->unp_addr->m_len;
		bcopy(mtod(unp->unp_addr, caddr_t), mtod(nam, caddr_t),
		    nam->m_len);
	} else {
		nam->m_len = sizeof(sun_noname);
		bcopy(&sun_noname, mtod(nam, struct sockaddr *),
		    nam->m_len);
	}
}

/*ARGSUSED*/
int
uipc_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct unpcb *unp = sotounpcb(so);
	struct socket *so2;
	int error = 0;

	if (req == PRU_CONTROL)
		return (EOPNOTSUPP);
	if (req != PRU_SEND && control && control->m_len) {
		error = EOPNOTSUPP;
		goto release;
	}
	if (unp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_ATTACH:
		if (unp) {
			error = EISCONN;
			break;
		}
		error = unp_attach(so);
		break;

	case PRU_DETACH:
		unp_detach(unp);
		break;

	case PRU_BIND:
		error = unp_bind(unp, nam, p);
		break;

	case PRU_LISTEN:
		if (unp->unp_vnode == NULL)
			error = EINVAL;
		break;

	case PRU_CONNECT:
		error = unp_connect(so, nam, p);
		break;

	case PRU_CONNECT2:
		error = unp_connect2(so, (struct socket *)nam);
		break;

	case PRU_DISCONNECT:
		unp_disconnect(unp);
		break;

	case PRU_ACCEPT:
		/*
		 * Pass back name of connected socket,
		 * if it was bound and we are still connected
		 * (our peer may have closed already!).
		 */
		uipc_setaddr(unp->unp_conn, nam);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		unp_shutdown(unp);
		break;

	case PRU_RCVD:
		switch (so->so_type) {

		case SOCK_DGRAM:
			panic("uipc 1");
			/*NOTREACHED*/

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
#define	rcv (&so->so_rcv)
#define snd (&so2->so_snd)
			if (unp->unp_conn == NULL)
				break;
			so2 = unp->unp_conn->unp_socket;
			/*
			 * Adjust backpressure on sender
			 * and wakeup any waiting to write.
			 */
			snd->sb_mbmax += unp->unp_mbcnt - rcv->sb_mbcnt;
			unp->unp_mbcnt = rcv->sb_mbcnt;
			snd->sb_hiwat += unp->unp_cc - rcv->sb_cc;
			unp->unp_cc = rcv->sb_cc;
			sowwakeup(so2);
#undef snd
#undef rcv
			break;

		default:
			panic("uipc 2");
		}
		break;

	case PRU_SEND:
		if (control && (error = unp_internalize(control, p)))
			break;
		switch (so->so_type) {

		case SOCK_DGRAM: {
			struct sockaddr *from;

			if (nam) {
				if (unp->unp_conn) {
					error = EISCONN;
					break;
				}
				error = unp_connect(so, nam, p);
				if (error)
					break;
			} else {
				if (unp->unp_conn == NULL) {
					error = ENOTCONN;
					break;
				}
			}
			so2 = unp->unp_conn->unp_socket;
			if (unp->unp_addr)
				from = mtod(unp->unp_addr, struct sockaddr *);
			else
				from = &sun_noname;
			if (sbappendaddr(&so2->so_rcv, from, m, control)) {
				sorwakeup(so2);
				m = NULL;
				control = NULL;
			} else
				error = ENOBUFS;
			if (nam)
				unp_disconnect(unp);
			break;
		}

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
#define	rcv (&so2->so_rcv)
#define	snd (&so->so_snd)
			if (so->so_state & SS_CANTSENDMORE) {
				error = EPIPE;
				break;
			}
			if (unp->unp_conn == NULL) {
				error = ENOTCONN;
				break;
			}
			so2 = unp->unp_conn->unp_socket;
			/*
			 * Send to paired receive port, and then reduce
			 * send buffer hiwater marks to maintain backpressure.
			 * Wake up readers.
			 */
			if (control) {
				if (sbappendcontrol(rcv, m, control))
					control = NULL;
			} else if (so->so_type == SOCK_SEQPACKET)
				sbappendrecord(rcv, m);
			else
				sbappend(rcv, m);
			snd->sb_mbmax -=
			    rcv->sb_mbcnt - unp->unp_conn->unp_mbcnt;
			unp->unp_conn->unp_mbcnt = rcv->sb_mbcnt;
			snd->sb_hiwat -= rcv->sb_cc - unp->unp_conn->unp_cc;
			unp->unp_conn->unp_cc = rcv->sb_cc;
			sorwakeup(so2);
			m = NULL;
#undef snd
#undef rcv
			break;

		default:
			panic("uipc 4");
		}
		/* we need to undo unp_internalize in case of errors */
		if (control && error)
			unp_dispose(control);
		break;

	case PRU_ABORT:
		unp_drop(unp, ECONNABORTED);
		break;

	case PRU_SENSE: {
		struct stat *sb = (struct stat *)m;

		sb->st_blksize = so->so_snd.sb_hiwat;
		switch (so->so_type) {
		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			if (unp->unp_conn != NULL) {
				so2 = unp->unp_conn->unp_socket;
				sb->st_blksize += so2->so_rcv.sb_cc;
			}
			break;
		default:
			break;
		}
		sb->st_dev = NODEV;
		if (unp->unp_ino == 0)
			unp->unp_ino = unp_ino++;
		sb->st_atim.tv_sec =
		    sb->st_mtim.tv_sec =
		    sb->st_ctim.tv_sec = unp->unp_ctime.tv_sec;
		sb->st_atim.tv_nsec =
		    sb->st_mtim.tv_nsec =
		    sb->st_ctim.tv_nsec = unp->unp_ctime.tv_nsec;
		sb->st_ino = unp->unp_ino;
		return (0);
	}

	case PRU_RCVOOB:
		return (EOPNOTSUPP);

	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		uipc_setaddr(unp, nam);
		break;

	case PRU_PEERADDR:
		uipc_setaddr(unp->unp_conn, nam);
		break;

	case PRU_SLOWTIMO:
		break;

	default:
		panic("piusrreq");
	}
release:
	if (control)
		m_freem(control);
	if (m)
		m_freem(m);
	return (error);
}

/*
 * Both send and receive buffers are allocated PIPSIZ bytes of buffering
 * for stream sockets, although the total for sender and receiver is
 * actually only PIPSIZ.
 * Datagram sockets really use the sendspace as the maximum datagram size,
 * and don't really want to reserve the sendspace.  Their recvspace should
 * be large enough for at least one max-size datagram plus address.
 */
#define	PIPSIZ	4096
u_long	unpst_sendspace = PIPSIZ;
u_long	unpst_recvspace = PIPSIZ;
u_long	unpdg_sendspace = 2*1024;	/* really max datagram size */
u_long	unpdg_recvspace = 4*1024;

int	unp_rights;			/* file descriptors in flight */

int
unp_attach(struct socket *so)
{
	struct unpcb *unp;
	int error;
	
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		switch (so->so_type) {

		case SOCK_STREAM:
		case SOCK_SEQPACKET:
			error = soreserve(so, unpst_sendspace, unpst_recvspace);
			break;

		case SOCK_DGRAM:
			error = soreserve(so, unpdg_sendspace, unpdg_recvspace);
			break;

		default:
			panic("unp_attach");
		}
		if (error)
			return (error);
	}
	unp = malloc(sizeof(*unp), M_PCB, M_NOWAIT|M_ZERO);
	if (unp == NULL)
		return (ENOBUFS);
	unp->unp_socket = so;
	so->so_pcb = unp;
	getnanotime(&unp->unp_ctime);
	return (0);
}

void
unp_detach(struct unpcb *unp)
{
	struct vnode *vp;
	
	if (unp->unp_vnode) {
		unp->unp_vnode->v_socket = NULL;
		vp = unp->unp_vnode;
		unp->unp_vnode = NULL;
		vrele(vp);
	}
	if (unp->unp_conn)
		unp_disconnect(unp);
	while (unp->unp_refs)
		unp_drop(unp->unp_refs, ECONNRESET);
	soisdisconnected(unp->unp_socket);
	unp->unp_socket->so_pcb = NULL;
	m_freem(unp->unp_addr);
	if (unp_rights) {
		/*
		 * Normally the receive buffer is flushed later,
		 * in sofree, but if our receive buffer holds references
		 * to descriptors that are now garbage, we will dispose
		 * of those descriptor references after the garbage collector
		 * gets them (resulting in a "panic: closef: count < 0").
		 */
		sorflush(unp->unp_socket);
		free(unp, M_PCB, 0);
		unp_gc();
	} else
		free(unp, M_PCB, 0);
}

int
unp_bind(struct unpcb *unp, struct mbuf *nam, struct proc *p)
{
	struct sockaddr_un *soun = mtod(nam, struct sockaddr_un *);
	struct mbuf *nam2;
	struct vnode *vp;
	struct vattr vattr;
	int error;
	struct nameidata nd;
	size_t pathlen;

	if (unp->unp_vnode != NULL)
		return (EINVAL);

	if (soun->sun_len > sizeof(struct sockaddr_un) ||
	    soun->sun_len < offsetof(struct sockaddr_un, sun_path))
		return (EINVAL);
	if (soun->sun_family != AF_UNIX)
		return (EAFNOSUPPORT);

	pathlen = strnlen(soun->sun_path, soun->sun_len -
	    offsetof(struct sockaddr_un, sun_path));
	if (pathlen == sizeof(soun->sun_path))
		return (EINVAL);

	nam2 = m_getclr(M_WAITOK, MT_SONAME);
	nam2->m_len = sizeof(struct sockaddr_un);
	memcpy(mtod(nam2, struct sockaddr_un *), soun,
	    offsetof(struct sockaddr_un, sun_path) + pathlen);
	/* No need to NUL terminate: m_getclr() returns zero'd mbufs. */

	soun = mtod(nam2, struct sockaddr_un *);

	/* Fixup sun_len to keep it in sync with m_len. */
	soun->sun_len = nam2->m_len;

	NDINIT(&nd, CREATE, NOFOLLOW | LOCKPARENT, UIO_SYSSPACE,
	    soun->sun_path, p);
/* SHOULD BE ABLE TO ADOPT EXISTING AND wakeup() ALA FIFO's */
	if ((error = namei(&nd)) != 0) {
		m_freem(nam2);
		return (error);
	}
	vp = nd.ni_vp;
	if (vp != NULL) {
		VOP_ABORTOP(nd.ni_dvp, &nd.ni_cnd);
		if (nd.ni_dvp == vp)
			vrele(nd.ni_dvp);
		else
			vput(nd.ni_dvp);
		vrele(vp);
		m_freem(nam2);
		return (EADDRINUSE);
	}
	VATTR_NULL(&vattr);
	vattr.va_type = VSOCK;
	vattr.va_mode = ACCESSPERMS &~ p->p_fd->fd_cmask;
	error = VOP_CREATE(nd.ni_dvp, &nd.ni_vp, &nd.ni_cnd, &vattr);
	if (error) {
		m_freem(nam2);
		return (error);
	}
	unp->unp_addr = nam2;
	vp = nd.ni_vp;
	vp->v_socket = unp->unp_socket;
	unp->unp_vnode = vp;
	unp->unp_connid.uid = p->p_ucred->cr_uid;
	unp->unp_connid.gid = p->p_ucred->cr_gid;
	unp->unp_connid.pid = p->p_p->ps_pid;
	unp->unp_flags |= UNP_FEIDSBIND;
	VOP_UNLOCK(vp, 0, p);
	return (0);
}

int
unp_connect(struct socket *so, struct mbuf *nam, struct proc *p)
{
	struct sockaddr_un *soun = mtod(nam, struct sockaddr_un *);
	struct vnode *vp;
	struct socket *so2, *so3;
	struct unpcb *unp, *unp2, *unp3;
	int error;
	struct nameidata nd;

	if (soun->sun_family != AF_UNIX)
		return (EAFNOSUPPORT);

	if (nam->m_len < sizeof(struct sockaddr_un))
		*(mtod(nam, caddr_t) + nam->m_len) = 0;
	else if (nam->m_len > sizeof(struct sockaddr_un))
		return (EINVAL);
	else if (memchr(soun->sun_path, '\0', sizeof(soun->sun_path)) == NULL)
		return (EINVAL);

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE, soun->sun_path, p);
	if ((error = namei(&nd)) != 0)
		return (error);
	vp = nd.ni_vp;
	if (vp->v_type != VSOCK) {
		error = ENOTSOCK;
		goto bad;
	}
	if ((error = VOP_ACCESS(vp, VWRITE, p->p_ucred, p)) != 0)
		goto bad;
	so2 = vp->v_socket;
	if (so2 == NULL) {
		error = ECONNREFUSED;
		goto bad;
	}
	if (so->so_type != so2->so_type) {
		error = EPROTOTYPE;
		goto bad;
	}
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		if ((so2->so_options & SO_ACCEPTCONN) == 0 ||
		    (so3 = sonewconn(so2, 0)) == 0) {
			error = ECONNREFUSED;
			goto bad;
		}
		unp = sotounpcb(so);
		unp2 = sotounpcb(so2);
		unp3 = sotounpcb(so3);
		if (unp2->unp_addr)
			unp3->unp_addr =
			    m_copy(unp2->unp_addr, 0, (int)M_COPYALL);
		unp3->unp_connid.uid = p->p_ucred->cr_uid;
		unp3->unp_connid.gid = p->p_ucred->cr_gid;
		unp3->unp_connid.pid = p->p_p->ps_pid;
		unp3->unp_flags |= UNP_FEIDS;
		so2 = so3;
		if (unp2->unp_flags & UNP_FEIDSBIND) {
			unp->unp_connid = unp2->unp_connid;
			unp->unp_flags |= UNP_FEIDS;
		}
	}
	error = unp_connect2(so, so2);
bad:
	vput(vp);
	return (error);
}

int
unp_connect2(struct socket *so, struct socket *so2)
{
	struct unpcb *unp = sotounpcb(so);
	struct unpcb *unp2;

	if (so2->so_type != so->so_type)
		return (EPROTOTYPE);
	unp2 = sotounpcb(so2);
	unp->unp_conn = unp2;
	switch (so->so_type) {

	case SOCK_DGRAM:
		unp->unp_nextref = unp2->unp_refs;
		unp2->unp_refs = unp;
		soisconnected(so);
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		unp2->unp_conn = unp;
		soisconnected(so);
		soisconnected(so2);
		break;

	default:
		panic("unp_connect2");
	}
	return (0);
}

void
unp_disconnect(struct unpcb *unp)
{
	struct unpcb *unp2 = unp->unp_conn;

	if (unp2 == NULL)
		return;
	unp->unp_conn = NULL;
	switch (unp->unp_socket->so_type) {

	case SOCK_DGRAM:
		if (unp2->unp_refs == unp)
			unp2->unp_refs = unp->unp_nextref;
		else {
			unp2 = unp2->unp_refs;
			for (;;) {
				if (unp2 == NULL)
					panic("unp_disconnect");
				if (unp2->unp_nextref == unp)
					break;
				unp2 = unp2->unp_nextref;
			}
			unp2->unp_nextref = unp->unp_nextref;
		}
		unp->unp_nextref = NULL;
		unp->unp_socket->so_state &= ~SS_ISCONNECTED;
		break;

	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		soisdisconnected(unp->unp_socket);
		unp2->unp_conn = NULL;
		soisdisconnected(unp2->unp_socket);
		break;
	}
}

void
unp_shutdown(struct unpcb *unp)
{
	struct socket *so;

	switch (unp->unp_socket->so_type) {
	case SOCK_STREAM:
	case SOCK_SEQPACKET:
		if (unp->unp_conn && (so = unp->unp_conn->unp_socket))
			socantrcvmore(so);
		break;
	default:
		break;
	}
}

void
unp_drop(struct unpcb *unp, int errno)
{
	struct socket *so = unp->unp_socket;

	so->so_error = errno;
	unp_disconnect(unp);
	if (so->so_head) {
		so->so_pcb = NULL;
		sofree(so);
		m_freem(unp->unp_addr);
		free(unp, M_PCB, sizeof(*unp));
	}
}

#ifdef notdef
unp_drain(void)
{

}
#endif

int
unp_externalize(struct mbuf *rights, socklen_t controllen, int flags)
{
	struct proc *p = curproc;		/* XXX */
	struct cmsghdr *cm = mtod(rights, struct cmsghdr *);
	int i, *fdp = NULL;
	struct file **rp;
	struct file *fp;
	int nfds, error = 0;

	nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) /
	    sizeof(struct file *);
	if (controllen < CMSG_ALIGN(sizeof(struct cmsghdr)))
		controllen = 0;
	else
		controllen -= CMSG_ALIGN(sizeof(struct cmsghdr));
	if (nfds > controllen / sizeof(int)) {
		error = EMSGSIZE;
		goto restart;
	}

	rp = (struct file **)CMSG_DATA(cm);

	fdp = mallocarray(nfds, sizeof(int), M_TEMP, M_WAITOK);

	/* Make sure the recipient should be able to see the descriptors.. */
	if (p->p_fd->fd_rdir != NULL) {
		rp = (struct file **)CMSG_DATA(cm);
		for (i = 0; i < nfds; i++) {
			fp = *rp++;
			/*
			 * No to block devices.  If passing a directory,
			 * make sure that it is underneath the root.
			 */
			if (fp->f_type == DTYPE_VNODE) {
				struct vnode *vp = (struct vnode *)fp->f_data;

				if (vp->v_type == VBLK ||
				    (vp->v_type == VDIR &&
				    !vn_isunder(vp, p->p_fd->fd_rdir, p))) {
					error = EPERM;
					break;
				}
			}
		}
	}

restart:
	fdplock(p->p_fd);
	if (error != 0) {
		rp = ((struct file **)CMSG_DATA(cm));
		for (i = 0; i < nfds; i++) {
			fp = *rp;
			/*
			 * zero the pointer before calling unp_discard,
			 * since it may end up in unp_gc()..
			 */
			*rp++ = NULL;
			unp_discard(fp);
		}
		goto out;
	}

	/*
	 * First loop -- allocate file descriptor table slots for the
	 * new descriptors.
	 */
	rp = ((struct file **)CMSG_DATA(cm));
	for (i = 0; i < nfds; i++) {
		if ((error = fdalloc(p, 0, &fdp[i])) != 0) {
			/*
			 * Back out what we've done so far.
			 */
			for (--i; i >= 0; i--)
				fdremove(p->p_fd, fdp[i]);

			if (error == ENOSPC) {
				fdexpand(p);
				error = 0;
			} else {
				/*
				 * This is the error that has historically
				 * been returned, and some callers may
				 * expect it.
				 */
				error = EMSGSIZE;
			}
			fdpunlock(p->p_fd);
			goto restart;
		}

		/*
		 * Make the slot reference the descriptor so that
		 * fdalloc() works properly.. We finalize it all
		 * in the loop below.
		 */
		p->p_fd->fd_ofiles[fdp[i]] = *rp++;

		if (flags & MSG_CMSG_CLOEXEC)
			p->p_fd->fd_ofileflags[fdp[i]] |= UF_EXCLOSE;
	}

	/*
	 * Now that adding them has succeeded, update all of the
	 * descriptor passing state.
	 */
	rp = (struct file **)CMSG_DATA(cm);
	for (i = 0; i < nfds; i++) {
		fp = *rp++;
		fp->f_msgcount--;
		unp_rights--;
	}

	/*
	 * Copy temporary array to message and adjust length, in case of
	 * transition from large struct file pointers to ints.
	 */
	memcpy(CMSG_DATA(cm), fdp, nfds * sizeof(int));
	cm->cmsg_len = CMSG_LEN(nfds * sizeof(int));
	rights->m_len = CMSG_LEN(nfds * sizeof(int));
 out:
	fdpunlock(p->p_fd);
	if (fdp)
		free(fdp, M_TEMP, 0);
	return (error);
}

int
unp_internalize(struct mbuf *control, struct proc *p)
{
	struct filedesc *fdp = p->p_fd;
	struct cmsghdr *cm = mtod(control, struct cmsghdr *);
	struct file **rp, *fp;
	int i, error;
	int nfds, *ip, fd, neededspace;

	/*
	 * Check for two potential msg_controllen values because
	 * IETF stuck their nose in a place it does not belong.
	 */ 
	if (cm->cmsg_type != SCM_RIGHTS || cm->cmsg_level != SOL_SOCKET ||
	    !(cm->cmsg_len == control->m_len ||
	    control->m_len == CMSG_ALIGN(cm->cmsg_len)))
		return (EINVAL);
	nfds = (cm->cmsg_len - CMSG_ALIGN(sizeof(*cm))) / sizeof (int);

	if (unp_rights + nfds > maxfiles / 10)
		return (EMFILE);

	/* Make sure we have room for the struct file pointers */
morespace:
	neededspace = CMSG_SPACE(nfds * sizeof(struct file *)) -
	    control->m_len;
	if (neededspace > M_TRAILINGSPACE(control)) {
		char *tmp;
		/* if we already have a cluster, the message is just too big */
		if (control->m_flags & M_EXT)
			return (E2BIG);

		/* copy cmsg data temporarily out of the mbuf */
		tmp = malloc(control->m_len, M_TEMP, M_WAITOK);
		memcpy(tmp, mtod(control, caddr_t), control->m_len);

		/* allocate a cluster and try again */
		MCLGET(control, M_WAIT);
		if ((control->m_flags & M_EXT) == 0) {
			free(tmp, M_TEMP, control->m_len);
			return (ENOBUFS);       /* allocation failed */
		}

		/* copy the data back into the cluster */
		cm = mtod(control, struct cmsghdr *);
		memcpy(cm, tmp, control->m_len);
		free(tmp, M_TEMP, control->m_len);
		goto morespace;
	}

	/* adjust message & mbuf to note amount of space actually used. */
	cm->cmsg_len = CMSG_LEN(nfds * sizeof(struct file *));
	control->m_len = CMSG_SPACE(nfds * sizeof(struct file *));

	ip = ((int *)CMSG_DATA(cm)) + nfds - 1;
	rp = ((struct file **)CMSG_DATA(cm)) + nfds - 1;
	for (i = 0; i < nfds; i++) {
		bcopy(ip, &fd, sizeof fd);
		ip--;
		if ((fp = fd_getfile(fdp, fd)) == NULL) {
			error = EBADF;
			goto fail;
		}
		if (fp->f_count == LONG_MAX-2 ||
		    fp->f_msgcount == LONG_MAX-2) {
			error = EDEADLK;
			goto fail;
		}
		/* kq and systrace descriptors cannot be copied */
		if (fp->f_type == DTYPE_KQUEUE ||
		    fp->f_type == DTYPE_SYSTRACE) {
			error = EINVAL;
			goto fail;
		}
		bcopy(&fp, rp, sizeof fp);
		rp--;
		fp->f_count++;
		fp->f_msgcount++;
		unp_rights++;
	}
	return (0);
fail:
	/* Back out what we just did. */
	for ( ; i > 0; i--) {
		rp++;
		bcopy(rp, &fp, sizeof(fp));
		fp->f_count--;
		fp->f_msgcount--;
		unp_rights--;
	}

	return (error);
}

int	unp_defer, unp_gcing;
extern	struct domain unixdomain;

void
unp_gc(void)
{
	struct file *fp, *nextfp;
	struct socket *so;
	struct file **extra_ref, **fpp;
	int nunref, i;

	if (unp_gcing)
		return;
	unp_gcing = 1;
	unp_defer = 0;
	LIST_FOREACH(fp, &filehead, f_list)
		fp->f_iflags &= ~(FIF_MARK|FIF_DEFER);
	do {
		LIST_FOREACH(fp, &filehead, f_list) {
			if (fp->f_iflags & FIF_DEFER) {
				fp->f_iflags &= ~FIF_DEFER;
				unp_defer--;
			} else {
				if (fp->f_count == 0)
					continue;
				if (fp->f_iflags & FIF_MARK)
					continue;
				if (fp->f_count == fp->f_msgcount)
					continue;
			}
			fp->f_iflags |= FIF_MARK;

			if (fp->f_type != DTYPE_SOCKET ||
			    (so = fp->f_data) == NULL)
				continue;
			if (so->so_proto->pr_domain != &unixdomain ||
			    (so->so_proto->pr_flags&PR_RIGHTS) == 0)
				continue;
#ifdef notdef
			if (so->so_rcv.sb_flags & SB_LOCK) {
				/*
				 * This is problematical; it's not clear
				 * we need to wait for the sockbuf to be
				 * unlocked (on a uniprocessor, at least),
				 * and it's also not clear what to do
				 * if sbwait returns an error due to receipt
				 * of a signal.  If sbwait does return
				 * an error, we'll go into an infinite
				 * loop.  Delete all of this for now.
				 */
				(void) sbwait(&so->so_rcv);
				goto restart;
			}
#endif
			unp_scan(so->so_rcv.sb_mb, unp_mark, 0);
		}
	} while (unp_defer);
	/*
	 * We grab an extra reference to each of the file table entries
	 * that are not otherwise accessible and then free the rights
	 * that are stored in messages on them.
	 *
	 * The bug in the original code is a little tricky, so I'll describe
	 * what's wrong with it here.
	 *
	 * It is incorrect to simply unp_discard each entry for f_msgcount
	 * times -- consider the case of sockets A and B that contain
	 * references to each other.  On a last close of some other socket,
	 * we trigger a gc since the number of outstanding rights (unp_rights)
	 * is non-zero.  If during the sweep phase the gc code un_discards,
	 * we end up doing a (full) closef on the descriptor.  A closef on A
	 * results in the following chain.  Closef calls soo_close, which
	 * calls soclose.   Soclose calls first (through the switch
	 * uipc_usrreq) unp_detach, which re-invokes unp_gc.  Unp_gc simply
	 * returns because the previous instance had set unp_gcing, and
	 * we return all the way back to soclose, which marks the socket
	 * with SS_NOFDREF, and then calls sofree.  Sofree calls sorflush
	 * to free up the rights that are queued in messages on the socket A,
	 * i.e., the reference on B.  The sorflush calls via the dom_dispose
	 * switch unp_dispose, which unp_scans with unp_discard.  This second
	 * instance of unp_discard just calls closef on B.
	 *
	 * Well, a similar chain occurs on B, resulting in a sorflush on B,
	 * which results in another closef on A.  Unfortunately, A is already
	 * being closed, and the descriptor has already been marked with
	 * SS_NOFDREF, and soclose panics at this point.
	 *
	 * Here, we first take an extra reference to each inaccessible
	 * descriptor.  Then, we call sorflush ourself, since we know
	 * it is a Unix domain socket anyhow.  After we destroy all the
	 * rights carried in messages, we do a last closef to get rid
	 * of our extra reference.  This is the last close, and the
	 * unp_detach etc will shut down the socket.
	 *
	 * 91/09/19, bsy@cs.cmu.edu
	 */
	extra_ref = mallocarray(nfiles, sizeof(struct file *), M_FILE, M_WAITOK);
	for (nunref = 0, fp = LIST_FIRST(&filehead), fpp = extra_ref;
	    fp != NULL; fp = nextfp) {
		nextfp = LIST_NEXT(fp, f_list);
		if (fp->f_count == 0)
			continue;
		if (fp->f_count == fp->f_msgcount &&
		    !(fp->f_iflags & FIF_MARK)) {
			*fpp++ = fp;
			nunref++;
			FREF(fp);
			fp->f_count++;
		}
	}
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp)
	        if ((*fpp)->f_type == DTYPE_SOCKET && (*fpp)->f_data != NULL)
		        sorflush((*fpp)->f_data);
	for (i = nunref, fpp = extra_ref; --i >= 0; ++fpp)
		(void) closef(*fpp, NULL);
	free(extra_ref, M_FILE, 0);
	unp_gcing = 0;
}

void
unp_dispose(struct mbuf *m)
{

	if (m)
		unp_scan(m, unp_discard, 1);
}

void
unp_scan(struct mbuf *m0, void (*op)(struct file *), int discard)
{
	struct mbuf *m;
	struct file **rp, *fp;
	struct cmsghdr *cm;
	int i;
	int qfds;

	while (m0) {
		for (m = m0; m; m = m->m_next) {
			if (m->m_type == MT_CONTROL &&
			    m->m_len >= sizeof(*cm)) {
				cm = mtod(m, struct cmsghdr *);
				if (cm->cmsg_level != SOL_SOCKET ||
				    cm->cmsg_type != SCM_RIGHTS)
					continue;
				qfds = (cm->cmsg_len - CMSG_ALIGN(sizeof *cm))
				    / sizeof(struct file *);
				rp = (struct file **)CMSG_DATA(cm);
				for (i = 0; i < qfds; i++) {
					fp = *rp;
					if (discard)
						*rp = 0;
					(*op)(fp);
					rp++;
				}
				break;		/* XXX, but saves time */
			}
		}
		m0 = m0->m_nextpkt;
	}
}

void
unp_mark(struct file *fp)
{
	if (fp == NULL)
		return;

	if (fp->f_iflags & (FIF_MARK|FIF_DEFER))
		return;

	if (fp->f_type == DTYPE_SOCKET) {
		unp_defer++;
		fp->f_iflags |= FIF_DEFER;
	} else {
		fp->f_iflags |= FIF_MARK;
	}
}

void
unp_discard(struct file *fp)
{

	if (fp == NULL)
		return;
	FREF(fp);
	fp->f_msgcount--;
	unp_rights--;
	(void) closef(fp, NULL);
}
