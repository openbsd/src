/*	$OpenBSD: uipc_socket.c,v 1.231 2018/12/17 16:46:59 bluhm Exp $	*/
/*	$NetBSD: uipc_socket.c,v 1.21 1996/02/04 02:17:52 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)uipc_socket.c	8.3 (Berkeley) 4/15/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/event.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/unpcb.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h>
#include <net/if.h>
#include <sys/pool.h>
#include <sys/atomic.h>
#include <sys/rwlock.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

void	sbsync(struct sockbuf *, struct mbuf *);

int	sosplice(struct socket *, int, off_t, struct timeval *);
void	sounsplice(struct socket *, struct socket *, int);
void	soidle(void *);
void	sotask(void *);
void	soreaper(void *);
void	soput(void *);
int	somove(struct socket *, int);

void	filt_sordetach(struct knote *kn);
int	filt_soread(struct knote *kn, long hint);
void	filt_sowdetach(struct knote *kn);
int	filt_sowrite(struct knote *kn, long hint);
int	filt_solisten(struct knote *kn, long hint);

struct filterops solisten_filtops =
	{ 1, NULL, filt_sordetach, filt_solisten };
struct filterops soread_filtops =
	{ 1, NULL, filt_sordetach, filt_soread };
struct filterops sowrite_filtops =
	{ 1, NULL, filt_sowdetach, filt_sowrite };


#ifndef SOMINCONN
#define SOMINCONN 80
#endif /* SOMINCONN */

int	somaxconn = SOMAXCONN;
int	sominconn = SOMINCONN;

struct pool socket_pool;
#ifdef SOCKET_SPLICE
struct pool sosplice_pool;
struct taskq *sosplice_taskq;
struct rwlock sosplice_lock = RWLOCK_INITIALIZER("sosplicelk");
#endif

void
soinit(void)
{
	pool_init(&socket_pool, sizeof(struct socket), 0, IPL_SOFTNET, 0,
	    "sockpl", NULL);
#ifdef SOCKET_SPLICE
	pool_init(&sosplice_pool, sizeof(struct sosplice), 0, IPL_SOFTNET, 0,
	    "sosppl", NULL);
#endif
}

/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sys_socket.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */
int
socreate(int dom, struct socket **aso, int type, int proto)
{
	struct proc *p = curproc;		/* XXX */
	const struct protosw *prp;
	struct socket *so;
	int error, s;

	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == NULL || prp->pr_attach == NULL)
		return (EPROTONOSUPPORT);
	if (prp->pr_type != type)
		return (EPROTOTYPE);
	so = pool_get(&socket_pool, PR_WAITOK | PR_ZERO);
	sigio_init(&so->so_sigio);
	TAILQ_INIT(&so->so_q0);
	TAILQ_INIT(&so->so_q);
	so->so_type = type;
	if (suser(p) == 0)
		so->so_state = SS_PRIV;
	so->so_ruid = p->p_ucred->cr_ruid;
	so->so_euid = p->p_ucred->cr_uid;
	so->so_rgid = p->p_ucred->cr_rgid;
	so->so_egid = p->p_ucred->cr_gid;
	so->so_cpid = p->p_p->ps_pid;
	so->so_proto = prp;

	s = solock(so);
	error = (*prp->pr_attach)(so, proto);
	if (error) {
		so->so_state |= SS_NOFDREF;
		/* sofree() calls sounlock(). */
		sofree(so, s);
		return (error);
	}
	sounlock(so, s);
	*aso = so;
	return (0);
}

int
sobind(struct socket *so, struct mbuf *nam, struct proc *p)
{
	int error;

	soassertlocked(so);

	error = (*so->so_proto->pr_usrreq)(so, PRU_BIND, NULL, nam, NULL, p);
	return (error);
}

int
solisten(struct socket *so, int backlog)
{
	int s, error;

	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING|SS_ISDISCONNECTING))
		return (EOPNOTSUPP);
#ifdef SOCKET_SPLICE
	if (isspliced(so) || issplicedback(so))
		return (EOPNOTSUPP);
#endif /* SOCKET_SPLICE */
	s = solock(so);
	error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN, NULL, NULL, NULL,
	    curproc);
	if (error) {
		sounlock(so, s);
		return (error);
	}
	if (TAILQ_FIRST(&so->so_q) == NULL)
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0 || backlog > somaxconn)
		backlog = somaxconn;
	if (backlog < sominconn)
		backlog = sominconn;
	so->so_qlimit = backlog;
	sounlock(so, s);
	return (0);
}

void
sofree(struct socket *so, int s)
{
	soassertlocked(so);

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0) {
		sounlock(so, s);
		return;
	}
	if (so->so_head) {
		/*
		 * We must not decommission a socket that's on the accept(2)
		 * queue.  If we do, then accept(2) may hang after select(2)
		 * indicated that the listening socket was ready.
		 */
		if (!soqremque(so, 0)) {
			sounlock(so, s);
			return;
		}
	}
	sigio_free(&so->so_sigio);
#ifdef SOCKET_SPLICE
	if (so->so_sp) {
		if (issplicedback(so))
			sounsplice(so->so_sp->ssp_soback, so,
			    so->so_sp->ssp_soback != so);
		if (isspliced(so))
			sounsplice(so, so->so_sp->ssp_socket, 0);
	}
#endif /* SOCKET_SPLICE */
	sbrelease(so, &so->so_snd);
	sorflush(so);
	sounlock(so, s);
#ifdef SOCKET_SPLICE
	if (so->so_sp) {
		/* Reuse splice idle, sounsplice() has been called before. */
		timeout_set_proc(&so->so_sp->ssp_idleto, soreaper, so);
		timeout_add(&so->so_sp->ssp_idleto, 0);
	} else 
#endif /* SOCKET_SPLICE */
	{
		pool_put(&socket_pool, so);
	}
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
int
soclose(struct socket *so, int flags)
{
	struct socket *so2;
	int s, error = 0;

	s = solock(so);
	/* Revoke async IO early. There is a final revocation in sofree(). */
	sigio_free(&so->so_sigio);
	if (so->so_options & SO_ACCEPTCONN) {
		while ((so2 = TAILQ_FIRST(&so->so_q0)) != NULL) {
			(void) soqremque(so2, 0);
			(void) soabort(so2);
		}
		while ((so2 = TAILQ_FIRST(&so->so_q)) != NULL) {
			(void) soqremque(so2, 1);
			(void) soabort(so2);
		}
	}
	if (so->so_pcb == NULL)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (flags & MSG_DONTWAIT))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				error = sosleep(so, &so->so_timeo,
				    PSOCK | PCATCH, "netcls",
				    so->so_linger * hz);
				if (error)
					break;
			}
		}
	}
drop:
	if (so->so_pcb) {
		int error2;
		KASSERT(so->so_proto->pr_detach);
		error2 = (*so->so_proto->pr_detach)(so);
		if (error == 0)
			error = error2;
	}
discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose NOFDREF: so %p, so_type %d", so, so->so_type);
	so->so_state |= SS_NOFDREF;
	/* sofree() calls sounlock(). */
	sofree(so, s);
	return (error);
}

int
soabort(struct socket *so)
{
	soassertlocked(so);

	return (*so->so_proto->pr_usrreq)(so, PRU_ABORT, NULL, NULL, NULL,
	   curproc);
}

int
soaccept(struct socket *so, struct mbuf *nam)
{
	int error = 0;

	soassertlocked(so);

	if ((so->so_state & SS_NOFDREF) == 0)
		panic("soaccept !NOFDREF: so %p, so_type %d", so, so->so_type);
	so->so_state &= ~SS_NOFDREF;
	if ((so->so_state & SS_ISDISCONNECTED) == 0 ||
	    (so->so_proto->pr_flags & PR_ABRTACPTDIS) == 0)
		error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT, NULL,
		    nam, NULL, curproc);
	else
		error = ECONNABORTED;
	return (error);
}

int
soconnect(struct socket *so, struct mbuf *nam)
{
	int error;

	soassertlocked(so);

	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	/*
	 * If protocol is connection-based, can only connect once.
	 * Otherwise, if connected, try to disconnect first.
	 * This allows user to disconnect by connecting to, e.g.,
	 * a null address.
	 */
	if (so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING) &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) ||
	    (error = sodisconnect(so))))
		error = EISCONN;
	else
		error = (*so->so_proto->pr_usrreq)(so, PRU_CONNECT,
		    NULL, nam, NULL, curproc);
	return (error);
}

int
soconnect2(struct socket *so1, struct socket *so2)
{
	int s, error;

	s = solock(so1);
	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2, NULL,
	    (struct mbuf *)so2, NULL, curproc);
	sounlock(so1, s);
	return (error);
}

int
sodisconnect(struct socket *so)
{
	int error;

	soassertlocked(so);

	if ((so->so_state & SS_ISCONNECTED) == 0)
		return (ENOTCONN);
	if (so->so_state & SS_ISDISCONNECTING)
		return (EALREADY);
	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT, NULL, NULL,
	    NULL, curproc);
	return (error);
}

int m_getuio(struct mbuf **, int, long, struct uio *);

#define	SBLOCKWAIT(f)	(((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)
/*
 * Send on a socket.
 * If send must go all at once and message is larger than
 * send buffering, then hard error.
 * Lock against other senders.
 * If must go all at once and not enough room now, then
 * inform user that this would block and do nothing.
 * Otherwise, if nonblocking, send as much as possible.
 * The data to be sent is described by "uio" if nonzero,
 * otherwise by the mbuf chain "top" (which must be null
 * if uio is not).  Data provided in mbuf chain must be small
 * enough to send all at once.
 *
 * Returns nonzero on error, timeout or signal; callers
 * must check for short counts if EINTR/ERESTART are returned.
 * Data and control buffers are freed on return.
 */
int
sosend(struct socket *so, struct mbuf *addr, struct uio *uio, struct mbuf *top,
    struct mbuf *control, int flags)
{
	long space, clen = 0;
	size_t resid;
	int error, s;
	int atomic = sosendallatonce(so) || top;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;
	/* MSG_EOR on a SOCK_STREAM socket is invalid. */
	if (so->so_type == SOCK_STREAM && (flags & MSG_EOR)) {
		m_freem(top);
		m_freem(control);
		return (EINVAL);
	}
	if (uio && uio->uio_procp)
		uio->uio_procp->p_ru.ru_msgsnd++;
	if (control) {
		/*
		 * In theory clen should be unsigned (since control->m_len is).
		 * However, space must be signed, as it might be less than 0
		 * if we over-committed, and we must use a signed comparison
		 * of space and clen.
		 */
		clen = control->m_len;
		/* reserve extra space for AF_UNIX's internalize */
		if (so->so_proto->pr_domain->dom_family == AF_UNIX &&
		    clen >= CMSG_ALIGN(sizeof(struct cmsghdr)) &&
		    mtod(control, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
			clen = CMSG_SPACE(
			    (clen - CMSG_ALIGN(sizeof(struct cmsghdr))) *
			    (sizeof(struct fdpass) / sizeof(int)));
	}

#define	snderr(errno)	{ error = errno; goto release; }

	s = solock(so);
restart:
	if ((error = sblock(so, &so->so_snd, SBLOCKWAIT(flags))) != 0)
		goto out;
	so->so_state |= SS_ISSENDING;
	do {
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			snderr(error);
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if (!(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}
		space = sbspace(so, &so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if (so->so_proto->pr_domain->dom_family == AF_UNIX) {
			if (atomic && resid > so->so_snd.sb_hiwat)
				snderr(EMSGSIZE);
		} else {
			if (clen > so->so_snd.sb_hiwat ||
			    (atomic && resid > so->so_snd.sb_hiwat - clen))
				snderr(EMSGSIZE);
		}
		if (space < clen ||
		    (space - clen < resid &&
		    (atomic || space < so->so_snd.sb_lowat))) {
			if (flags & MSG_DONTWAIT)
				snderr(EWOULDBLOCK);
			sbunlock(so, &so->so_snd);
			error = sbwait(so, &so->so_snd);
			so->so_state &= ~SS_ISSENDING;
			if (error)
				goto out;
			goto restart;
		}
		space -= clen;
		do {
			if (uio == NULL) {
				/*
				 * Data is prepackaged in "top".
				 */
				resid = 0;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			} else {
				sounlock(so, s);
				error = m_getuio(&top, atomic, space, uio);
				s = solock(so);
				if (error)
					goto release;
				space -= top->m_pkthdr.len;
				resid = uio->uio_resid;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			}
			if (resid == 0)
				so->so_state &= ~SS_ISSENDING;
			if (top && so->so_options & SO_ZEROIZE)
				top->m_flags |= M_ZEROIZE;
			error = (*so->so_proto->pr_usrreq)(so,
			    (flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			    top, addr, control, curproc);
			clen = 0;
			control = NULL;
			top = NULL;
			if (error)
				goto release;
		} while (resid && space > 0);
	} while (resid);

release:
	so->so_state &= ~SS_ISSENDING;
	sbunlock(so, &so->so_snd);
out:
	sounlock(so, s);
	m_freem(top);
	m_freem(control);
	return (error);
}

int
m_getuio(struct mbuf **mp, int atomic, long space, struct uio *uio)
{
	struct mbuf *m, *top = NULL;
	struct mbuf **nextp = &top;
	u_long len, mlen;
	size_t resid = uio->uio_resid;
	int error;

	do {
		if (top == NULL) {
			MGETHDR(m, M_WAIT, MT_DATA);
			mlen = MHLEN;
			m->m_pkthdr.len = 0;
			m->m_pkthdr.ph_ifidx = 0;
		} else {
			MGET(m, M_WAIT, MT_DATA);
			mlen = MLEN;
		}
		/* chain mbuf together */
		*nextp = m;
		nextp = &m->m_next;

		resid = ulmin(resid, space);
		if (resid >= MINCLSIZE) {
			MCLGETI(m, M_NOWAIT, NULL, ulmin(resid, MAXMCLBYTES));
			if ((m->m_flags & M_EXT) == 0)
				MCLGETI(m, M_NOWAIT, NULL, MCLBYTES);
			if ((m->m_flags & M_EXT) == 0)
				goto nopages;
			mlen = m->m_ext.ext_size;
			len = ulmin(mlen, resid);
			/*
			 * For datagram protocols, leave room
			 * for protocol headers in first mbuf.
			 */
			if (atomic && m == top && len < mlen - max_hdr)
				m->m_data += max_hdr;
		} else {
nopages:
			len = ulmin(mlen, resid);
			/*
			 * For datagram protocols, leave room
			 * for protocol headers in first mbuf.
			 */
			if (atomic && m == top && len < mlen - max_hdr)
				m_align(m, len);
		}

		error = uiomove(mtod(m, caddr_t), len, uio);
		if (error) {
			m_freem(top);
			return (error);
		}

		/* adjust counters */
		resid = uio->uio_resid;
		space -= len;
		m->m_len = len;
		top->m_pkthdr.len += len;

		/* Is there more space and more data? */
	} while (space > 0 && resid > 0);

	*mp = top;
	return 0;
}

/*
 * Following replacement or removal of the first mbuf on the first
 * mbuf chain of a socket buffer, push necessary state changes back
 * into the socket buffer so that other consumers see the values
 * consistently.  'nextrecord' is the callers locally stored value of
 * the original value of sb->sb_mb->m_nextpkt which must be restored
 * when the lead mbuf changes.  NOTE: 'nextrecord' may be NULL.
 */
void
sbsync(struct sockbuf *sb, struct mbuf *nextrecord)
{

	/*
	 * First, update for the new value of nextrecord.  If necessary,
	 * make it the first record.
	 */
	if (sb->sb_mb != NULL)
		sb->sb_mb->m_nextpkt = nextrecord;
	else
		sb->sb_mb = nextrecord;

	/*
	 * Now update any dependent socket buffer fields to reflect
	 * the new state.  This is an inline of SB_EMPTY_FIXUP, with
	 * the addition of a second clause that takes care of the
	 * case where sb_mb has been updated, but remains the last
	 * record.
	 */
	if (sb->sb_mb == NULL) {
		sb->sb_mbtail = NULL;
		sb->sb_lastrecord = NULL;
	} else if (sb->sb_mb->m_nextpkt == NULL)
		sb->sb_lastrecord = sb->sb_mb;
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network for the entire time here, we release
 * the solock() while doing the actual copy to user space.
 * Although the sockbuf is locked, new data may still be appended,
 * and thus we must maintain consistency of the sockbuf during that time.
 *
 * The caller may receive the data as a single mbuf chain by supplying
 * an mbuf **mp0 for use in returning the chain.  The uio is then used
 * only for the count in uio_resid.
 */
int
soreceive(struct socket *so, struct mbuf **paddr, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp,
    socklen_t controllen)
{
	struct mbuf *m, **mp;
	struct mbuf *cm;
	u_long len, offset, moff;
	int flags, error, s, type, uio_error = 0;
	const struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	size_t resid, orig_resid = uio->uio_resid;

	mp = mp0;
	if (paddr)
		*paddr = NULL;
	if (controlp)
		*controlp = NULL;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		s = solock(so);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m,
		    (struct mbuf *)(long)(flags & MSG_PEEK), NULL, curproc);
		sounlock(so, s);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, caddr_t),
			    ulmin(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		m_freem(m);
		return (error);
	}
	if (mp)
		*mp = NULL;

	s = solock(so);
restart:
	if ((error = sblock(so, &so->so_rcv, SBLOCKWAIT(flags))) != 0) {
		sounlock(so, s);
		return (error);
	}

	m = so->so_rcv.sb_mb;
#ifdef SOCKET_SPLICE
	if (isspliced(so))
		m = NULL;
#endif /* SOCKET_SPLICE */
	/*
	 * If we have less data than requested, block awaiting more
	 * (subject to any timeout) if:
	 *   1. the current count is less than the low water mark,
	 *   2. MSG_WAITALL is set, and it is possible to do the entire
	 *	receive operation at once if we block (resid <= hiwat), or
	 *   3. MSG_DONTWAIT is not set.
	 * If MSG_WAITALL is set but resid is larger than the receive buffer,
	 * we have to do the receive in sections, and thus risk returning
	 * a short count if a timeout or signal occurs after we start.
	 */
	if (m == NULL || (((flags & MSG_DONTWAIT) == 0 &&
	    so->so_rcv.sb_cc < uio->uio_resid) &&
	    (so->so_rcv.sb_cc < so->so_rcv.sb_lowat ||
	    ((flags & MSG_WAITALL) && uio->uio_resid <= so->so_rcv.sb_hiwat)) &&
	    m->m_nextpkt == NULL && (pr->pr_flags & PR_ATOMIC) == 0)) {
#ifdef DIAGNOSTIC
		if (m == NULL && so->so_rcv.sb_cc)
#ifdef SOCKET_SPLICE
		    if (!isspliced(so))
#endif /* SOCKET_SPLICE */
			panic("receive 1: so %p, so_type %d, sb_cc %lu",
			    so, so->so_type, so->so_rcv.sb_cc);
#endif
		if (so->so_error) {
			if (m)
				goto dontblock;
			error = so->so_error;
			if ((flags & MSG_PEEK) == 0)
				so->so_error = 0;
			goto release;
		}
		if (so->so_state & SS_CANTRCVMORE) {
			if (m)
				goto dontblock;
			else if (so->so_rcv.sb_cc == 0)
				goto release;
		}
		for (; m; m = m->m_next)
			if (m->m_type == MT_OOBDATA  || (m->m_flags & M_EOR)) {
				m = so->so_rcv.sb_mb;
				goto dontblock;
			}
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
		    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
			error = ENOTCONN;
			goto release;
		}
		if (uio->uio_resid == 0 && controlp == NULL)
			goto release;
		if (flags & MSG_DONTWAIT) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 1");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 1");
		sbunlock(so, &so->so_rcv);
		error = sbwait(so, &so->so_rcv);
		if (error) {
			sounlock(so, s);
			return (error);
		}
		goto restart;
	}
dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * From this point onward, we maintain 'nextrecord' as a cache of the
	 * pointer to the next record in the socket buffer.  We must keep the
	 * various socket buffer pointers and local stack versions of the
	 * pointers in sync, pushing out modifications before operations that
	 * may sleep, and re-reading them afterwards.
	 *
	 * Otherwise, we will race with the network stack appending new data
	 * or records onto the socket buffer by using inconsistent/stale
	 * versions of the field, possibly resulting in socket buffer
	 * corruption.
	 */
	if (uio->uio_procp)
		uio->uio_procp->p_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a: so %p, so_type %d, m %p, m_type %d",
			    so, so->so_type, m, m->m_type);
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copym(m, 0, m->m_len, M_NOWAIT);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				so->so_rcv.sb_mb = m_free(m);
				m = so->so_rcv.sb_mb;
			}
			sbsync(&so->so_rcv, nextrecord);
		}
	}
	while (m && m->m_type == MT_CONTROL && error == 0) {
		int skip = 0;
		if (flags & MSG_PEEK) {
			if (mtod(m, struct cmsghdr *)->cmsg_type ==
			    SCM_RIGHTS) {
				/* don't leak internalized SCM_RIGHTS msgs */
				skip = 1;
			} else if (controlp)
				*controlp = m_copym(m, 0, m->m_len, M_NOWAIT);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			so->so_rcv.sb_mb = m->m_next;
			m->m_nextpkt = m->m_next = NULL;
			cm = m;
			m = so->so_rcv.sb_mb;
			sbsync(&so->so_rcv, nextrecord);
			if (controlp) {
				if (pr->pr_domain->dom_externalize) {
					error =
					    (*pr->pr_domain->dom_externalize)
					    (cm, controllen, flags);
				}
				*controlp = cm;
			} else {
				/*
				 * Dispose of any SCM_RIGHTS message that went
				 * through the read path rather than recv.
				 */
				if (pr->pr_domain->dom_dispose)
					pr->pr_domain->dom_dispose(cm);
				m_free(cm);
			}
		}
		if (m != NULL)
			nextrecord = so->so_rcv.sb_mb->m_nextpkt;
		else
			nextrecord = so->so_rcv.sb_mb;
		if (controlp && !skip) {
			orig_resid = 0;
			controlp = &(*controlp)->m_next;
		}
	}

	/* If m is non-NULL, we have some data to read. */
	if (m) {
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
		if (m->m_flags & M_BCAST)
			flags |= MSG_BCAST;
		if (m->m_flags & M_MCAST)
			flags |= MSG_MCAST;
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA) {
			break;
		} else if (m->m_type == MT_CONTROL) {
			/*
			 * If there is more than one control message in the
			 * stream, we do a short read.  Next can be received
			 * or disposed by another system call.
			 */
			break;
#ifdef DIAGNOSTIC
		} else if (m->m_type != MT_DATA && m->m_type != MT_HEADER) {
			panic("receive 3: so %p, so_type %d, m %p, m_type %d",
			    so, so->so_type, m, m->m_type);
#endif
		}
		so->so_state &= ~SS_RCVATMARK;
		len = uio->uio_resid;
		if (so->so_oobmark && len > so->so_oobmark - offset)
			len = so->so_oobmark - offset;
		if (len > m->m_len - moff)
			len = m->m_len - moff;
		/*
		 * If mp is set, just pass back the mbufs.
		 * Otherwise copy them out via the uio, then free.
		 * Sockbuf must be consistent here (points to current mbuf,
		 * it points to next record) when we drop priority;
		 * we must note any additions to the sockbuf when we
		 * block interrupts again.
		 */
		if (mp == NULL && uio_error == 0) {
			SBLASTRECORDCHK(&so->so_rcv, "soreceive uiomove");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive uiomove");
			resid = uio->uio_resid;
			sounlock(so, s);
			uio_error = uiomove(mtod(m, caddr_t) + moff, len, uio);
			s = solock(so);
			if (uio_error)
				uio->uio_resid = resid - len;
		} else
			uio->uio_resid -= len;
		if (len == m->m_len - moff) {
			if (m->m_flags & M_EOR)
				flags |= MSG_EOR;
			if (flags & MSG_PEEK) {
				m = m->m_next;
				moff = 0;
			} else {
				nextrecord = m->m_nextpkt;
				sbfree(&so->so_rcv, m);
				if (mp) {
					*mp = m;
					mp = &m->m_next;
					so->so_rcv.sb_mb = m = m->m_next;
					*mp = NULL;
				} else {
					so->so_rcv.sb_mb = m_free(m);
					m = so->so_rcv.sb_mb;
				}
				/*
				 * If m != NULL, we also know that
				 * so->so_rcv.sb_mb != NULL.
				 */
				KASSERT(so->so_rcv.sb_mb == m);
				if (m) {
					m->m_nextpkt = nextrecord;
					if (nextrecord == NULL)
						so->so_rcv.sb_lastrecord = m;
				} else {
					so->so_rcv.sb_mb = nextrecord;
					SB_EMPTY_FIXUP(&so->so_rcv);
				}
				SBLASTRECORDCHK(&so->so_rcv, "soreceive 3");
				SBLASTMBUFCHK(&so->so_rcv, "soreceive 3");
			}
		} else {
			if (flags & MSG_PEEK)
				moff += len;
			else {
				if (mp)
					*mp = m_copym(m, 0, len, M_WAIT);
				m->m_data += len;
				m->m_len -= len;
				so->so_rcv.sb_cc -= len;
				so->so_rcv.sb_datacc -= len;
			}
		}
		if (so->so_oobmark) {
			if ((flags & MSG_PEEK) == 0) {
				so->so_oobmark -= len;
				if (so->so_oobmark == 0) {
					so->so_state |= SS_RCVATMARK;
					break;
				}
			} else {
				offset += len;
				if (offset == so->so_oobmark)
					break;
			}
		}
		if (flags & MSG_EOR)
			break;
		/*
		 * If the MSG_WAITALL flag is set (for non-atomic socket),
		 * we must not quit until "uio->uio_resid == 0" or an error
		 * termination.  If a signal/timeout occurs, return
		 * with a short count but without error.
		 * Keep sockbuf locked against other readers.
		 */
		while (flags & MSG_WAITALL && m == NULL && uio->uio_resid > 0 &&
		    !sosendallatonce(so) && !nextrecord) {
			if (so->so_error || so->so_state & SS_CANTRCVMORE)
				break;
			SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 2");
			SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 2");
			error = sbwait(so, &so->so_rcv);
			if (error) {
				sbunlock(so, &so->so_rcv);
				sounlock(so, s);
				return (0);
			}
			if ((m = so->so_rcv.sb_mb) != NULL)
				nextrecord = m->m_nextpkt;
		}
	}

	if (m && pr->pr_flags & PR_ATOMIC) {
		flags |= MSG_TRUNC;
		if ((flags & MSG_PEEK) == 0)
			(void) sbdroprecord(&so->so_rcv);
	}
	if ((flags & MSG_PEEK) == 0) {
		if (m == NULL) {
			/*
			 * First part is an inline SB_EMPTY_FIXUP().  Second
			 * part makes sure sb_lastrecord is up-to-date if
			 * there is still data in the socket buffer.
			 */
			so->so_rcv.sb_mb = nextrecord;
			if (so->so_rcv.sb_mb == NULL) {
				so->so_rcv.sb_mbtail = NULL;
				so->so_rcv.sb_lastrecord = NULL;
			} else if (nextrecord->m_nextpkt == NULL)
				so->so_rcv.sb_lastrecord = nextrecord;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive 4");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive 4");
		if (pr->pr_flags & PR_WANTRCVD && so->so_pcb)
			(*pr->pr_usrreq)(so, PRU_RCVD, NULL,
			    (struct mbuf *)(long)flags, NULL, curproc);
	}
	if (orig_resid == uio->uio_resid && orig_resid &&
	    (flags & MSG_EOR) == 0 && (so->so_state & SS_CANTRCVMORE) == 0) {
		sbunlock(so, &so->so_rcv);
		goto restart;
	}

	if (uio_error)
		error = uio_error;

	if (flagsp)
		*flagsp |= flags;
release:
	sbunlock(so, &so->so_rcv);
	sounlock(so, s);
	return (error);
}

int
soshutdown(struct socket *so, int how)
{
	const struct protosw *pr = so->so_proto;
	int s, error = 0;

	s = solock(so);
	switch (how) {
	case SHUT_RD:
		sorflush(so);
		break;
	case SHUT_RDWR:
		sorflush(so);
		/* FALLTHROUGH */
	case SHUT_WR:
		error = (*pr->pr_usrreq)(so, PRU_SHUTDOWN, NULL, NULL, NULL,
		    curproc);
		break;
	default:
		error = EINVAL;
		break;
	}
	sounlock(so, s);

	return (error);
}

void
sorflush(struct socket *so)
{
	struct sockbuf *sb = &so->so_rcv;
	const struct protosw *pr = so->so_proto;
	struct socket aso;
	int error;

	sb->sb_flags |= SB_NOINTR;
	error = sblock(so, sb, M_WAITOK);
	/* with SB_NOINTR and M_WAITOK sblock() must not fail */
	KASSERT(error == 0);
	socantrcvmore(so);
	sbunlock(so, sb);
	aso.so_proto = pr;
	aso.so_rcv = *sb;
	memset(&sb->sb_startzero, 0,
	     (caddr_t)&sb->sb_endzero - (caddr_t)&sb->sb_startzero);
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(aso.so_rcv.sb_mb);
	sbrelease(&aso, &aso.so_rcv);
}

#ifdef SOCKET_SPLICE

#define so_splicelen	so_sp->ssp_len
#define so_splicemax	so_sp->ssp_max
#define so_idletv	so_sp->ssp_idletv
#define so_idleto	so_sp->ssp_idleto
#define so_splicetask	so_sp->ssp_task

int
sosplice(struct socket *so, int fd, off_t max, struct timeval *tv)
{
	struct file	*fp;
	struct socket	*sosp;
	struct sosplice	*sp;
	struct taskq	*tq;
	int		 error = 0;

	soassertlocked(so);

	if (sosplice_taskq == NULL) {
		rw_enter_write(&sosplice_lock);
		if (sosplice_taskq == NULL) {
			tq = taskq_create("sosplice", 1, IPL_SOFTNET,
			    TASKQ_MPSAFE);
			/* Ensure the taskq is fully visible to other CPUs. */
			membar_producer();
			sosplice_taskq = tq;
		}
		rw_exit_write(&sosplice_lock);
	}
	if (sosplice_taskq == NULL)
		return (ENOMEM);

	if ((so->so_proto->pr_flags & PR_SPLICE) == 0)
		return (EPROTONOSUPPORT);
	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0 &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED))
		return (ENOTCONN);
	if (so->so_sp == NULL) {
		sp = pool_get(&sosplice_pool, PR_WAITOK | PR_ZERO);
		if (so->so_sp == NULL)
			so->so_sp = sp;
		else
			pool_put(&sosplice_pool, sp);
	}

	/* If no fd is given, unsplice by removing existing link. */
	if (fd < 0) {
		/* Lock receive buffer. */
		if ((error = sblock(so, &so->so_rcv, M_WAITOK)) != 0) {
			return (error);
		}
		if (so->so_sp->ssp_socket)
			sounsplice(so, so->so_sp->ssp_socket, 1);
		sbunlock(so, &so->so_rcv);
		return (0);
	}

	if (max && max < 0)
		return (EINVAL);

	if (tv && (tv->tv_sec < 0 || tv->tv_usec < 0))
		return (EINVAL);

	/* Find sosp, the drain socket where data will be spliced into. */
	if ((error = getsock(curproc, fd, &fp)) != 0)
		return (error);
	sosp = fp->f_data;
	if (sosp->so_sp == NULL) {
		sp = pool_get(&sosplice_pool, PR_WAITOK | PR_ZERO);
		if (sosp->so_sp == NULL)
			sosp->so_sp = sp;
		else
			pool_put(&sosplice_pool, sp);
	}

	/* Lock both receive and send buffer. */
	if ((error = sblock(so, &so->so_rcv, M_WAITOK)) != 0) {
		goto frele;
	}
	if ((error = sblock(so, &sosp->so_snd, M_WAITOK)) != 0) {
		sbunlock(so, &so->so_rcv);
		goto frele;
	}

	if (so->so_sp->ssp_socket || sosp->so_sp->ssp_soback) {
		error = EBUSY;
		goto release;
	}
	if (sosp->so_proto->pr_usrreq != so->so_proto->pr_usrreq) {
		error = EPROTONOSUPPORT;
		goto release;
	}
	if (sosp->so_options & SO_ACCEPTCONN) {
		error = EOPNOTSUPP;
		goto release;
	}
	if ((sosp->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0) {
		error = ENOTCONN;
		goto release;
	}

	/* Splice so and sosp together. */
	so->so_sp->ssp_socket = sosp;
	sosp->so_sp->ssp_soback = so;
	so->so_splicelen = 0;
	so->so_splicemax = max;
	if (tv)
		so->so_idletv = *tv;
	else
		timerclear(&so->so_idletv);
	timeout_set_proc(&so->so_idleto, soidle, so);
	task_set(&so->so_splicetask, sotask, so);

	/*
	 * To prevent softnet interrupt from calling somove() while
	 * we sleep, the socket buffers are not marked as spliced yet.
	 */
	if (somove(so, M_WAIT)) {
		so->so_rcv.sb_flags |= SB_SPLICE;
		sosp->so_snd.sb_flags |= SB_SPLICE;
	}

 release:
	sbunlock(sosp, &sosp->so_snd);
	sbunlock(so, &so->so_rcv);
 frele:
	FRELE(fp, curproc);
	return (error);
}

void
sounsplice(struct socket *so, struct socket *sosp, int wakeup)
{
	soassertlocked(so);

	task_del(sosplice_taskq, &so->so_splicetask);
	timeout_del(&so->so_idleto);
	sosp->so_snd.sb_flags &= ~SB_SPLICE;
	so->so_rcv.sb_flags &= ~SB_SPLICE;
	so->so_sp->ssp_socket = sosp->so_sp->ssp_soback = NULL;
	if (wakeup && soreadable(so))
		sorwakeup(so);
}

void
soidle(void *arg)
{
	struct socket *so = arg;
	int s;

	s = solock(so);
	if (so->so_rcv.sb_flags & SB_SPLICE) {
		so->so_error = ETIMEDOUT;
		sounsplice(so, so->so_sp->ssp_socket, 1);
	}
	sounlock(so, s);
}

void
sotask(void *arg)
{
	struct socket *so = arg;
	int s;

	s = solock(so);
	if (so->so_rcv.sb_flags & SB_SPLICE) {
		/*
		 * We may not sleep here as sofree() and unsplice() may be
		 * called from softnet interrupt context.  This would remove
		 * the socket during somove().
		 */
		somove(so, M_DONTWAIT);
	}
	sounlock(so, s);

	/* Avoid user land starvation. */
	yield();
}

/*
 * The socket splicing task or idle timeout may sleep while grabbing the net
 * lock.  As sofree() can be called anytime, sotask() or soidle() could access
 * the socket memory of a freed socket after wakeup.  So delay the pool_put()
 * after all pending socket splicing tasks or timeouts have finished.  Do this
 * by scheduling it on the same threads.
 */
void
soreaper(void *arg)
{
	struct socket *so = arg;

	/* Reuse splice task, sounsplice() has been called before. */
	task_set(&so->so_sp->ssp_task, soput, so);
	task_add(sosplice_taskq, &so->so_sp->ssp_task);
}

void
soput(void *arg)
{
	struct socket *so = arg;

	pool_put(&sosplice_pool, so->so_sp);
	pool_put(&socket_pool, so);
}

/*
 * Move data from receive buffer of spliced source socket to send
 * buffer of drain socket.  Try to move as much as possible in one
 * big chunk.  It is a TCP only implementation.
 * Return value 0 means splicing has been finished, 1 continue.
 */
int
somove(struct socket *so, int wait)
{
	struct socket	*sosp = so->so_sp->ssp_socket;
	struct mbuf	*m, **mp, *nextrecord;
	u_long		 len, off, oobmark;
	long		 space;
	int		 error = 0, maxreached = 0;
	unsigned int	 state;

	soassertlocked(so);

 nextpkt:
	if (so->so_error) {
		error = so->so_error;
		goto release;
	}
	if (sosp->so_state & SS_CANTSENDMORE) {
		error = EPIPE;
		goto release;
	}
	if (sosp->so_error && sosp->so_error != ETIMEDOUT &&
	    sosp->so_error != EFBIG && sosp->so_error != ELOOP) {
		error = sosp->so_error;
		goto release;
	}
	if ((sosp->so_state & SS_ISCONNECTED) == 0)
		goto release;

	/* Calculate how many bytes can be copied now. */
	len = so->so_rcv.sb_datacc;
	if (so->so_splicemax) {
		KASSERT(so->so_splicelen < so->so_splicemax);
		if (so->so_splicemax <= so->so_splicelen + len) {
			len = so->so_splicemax - so->so_splicelen;
			maxreached = 1;
		}
	}
	space = sbspace(sosp, &sosp->so_snd);
	if (so->so_oobmark && so->so_oobmark < len &&
	    so->so_oobmark < space + 1024)
		space += 1024;
	if (space <= 0) {
		maxreached = 0;
		goto release;
	}
	if (space < len) {
		maxreached = 0;
		if (space < sosp->so_snd.sb_lowat)
			goto release;
		len = space;
	}
	sosp->so_state |= SS_ISSENDING;

	SBLASTRECORDCHK(&so->so_rcv, "somove 1");
	SBLASTMBUFCHK(&so->so_rcv, "somove 1");
	m = so->so_rcv.sb_mb;
	if (m == NULL)
		goto release;
	nextrecord = m->m_nextpkt;

	/* Drop address and control information not used with splicing. */
	if (so->so_proto->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("somove soname: so %p, so_type %d, m %p, "
			    "m_type %d", so, so->so_type, m, m->m_type);
#endif
		m = m->m_next;
	}
	while (m && m->m_type == MT_CONTROL)
		m = m->m_next;
	if (m == NULL) {
		sbdroprecord(&so->so_rcv);
		if (so->so_proto->pr_flags & PR_WANTRCVD && so->so_pcb)
			(so->so_proto->pr_usrreq)(so, PRU_RCVD, NULL,
			    NULL, NULL, NULL);
		goto nextpkt;
	}

	/*
	 * By splicing sockets connected to localhost, userland might create a
	 * loop.  Dissolve splicing with error if loop is detected by counter.
	 */
	if ((m->m_flags & M_PKTHDR) && m->m_pkthdr.ph_loopcnt++ >= M_MAXLOOP) {
		error = ELOOP;
		goto release;
	}

	if (so->so_proto->pr_flags & PR_ATOMIC) {
		if ((m->m_flags & M_PKTHDR) == 0)
			panic("somove !PKTHDR: so %p, so_type %d, m %p, "
			    "m_type %d", so, so->so_type, m, m->m_type);
		if (sosp->so_snd.sb_hiwat < m->m_pkthdr.len) {
			error = EMSGSIZE;
			goto release;
		}
		if (len < m->m_pkthdr.len)
			goto release;
		if (m->m_pkthdr.len < len) {
			maxreached = 0;
			len = m->m_pkthdr.len;
		}
		/*
		 * Throw away the name mbuf after it has been assured
		 * that the whole first record can be processed.
		 */
		m = so->so_rcv.sb_mb;
		sbfree(&so->so_rcv, m);
		so->so_rcv.sb_mb = m_free(m);
		sbsync(&so->so_rcv, nextrecord);
	}
	/*
	 * Throw away the control mbufs after it has been assured
	 * that the whole first record can be processed.
	 */
	m = so->so_rcv.sb_mb;
	while (m && m->m_type == MT_CONTROL) {
		sbfree(&so->so_rcv, m);
		so->so_rcv.sb_mb = m_free(m);
		m = so->so_rcv.sb_mb;
		sbsync(&so->so_rcv, nextrecord);
	}

	SBLASTRECORDCHK(&so->so_rcv, "somove 2");
	SBLASTMBUFCHK(&so->so_rcv, "somove 2");

	/* Take at most len mbufs out of receive buffer. */
	for (off = 0, mp = &m; off <= len && *mp;
	    off += (*mp)->m_len, mp = &(*mp)->m_next) {
		u_long size = len - off;

#ifdef DIAGNOSTIC
		if ((*mp)->m_type != MT_DATA && (*mp)->m_type != MT_HEADER)
			panic("somove type: so %p, so_type %d, m %p, "
			    "m_type %d", so, so->so_type, *mp, (*mp)->m_type);
#endif
		if ((*mp)->m_len > size) {
			/*
			 * Move only a partial mbuf at maximum splice length or
			 * if the drain buffer is too small for this large mbuf.
			 */
			if (!maxreached && so->so_snd.sb_datacc > 0) {
				len -= size;
				break;
			}
			*mp = m_copym(so->so_rcv.sb_mb, 0, size, wait);
			if (*mp == NULL) {
				len -= size;
				break;
			}
			so->so_rcv.sb_mb->m_data += size;
			so->so_rcv.sb_mb->m_len -= size;
			so->so_rcv.sb_cc -= size;
			so->so_rcv.sb_datacc -= size;
		} else {
			*mp = so->so_rcv.sb_mb;
			sbfree(&so->so_rcv, *mp);
			so->so_rcv.sb_mb = (*mp)->m_next;
			sbsync(&so->so_rcv, nextrecord);
		}
	}
	*mp = NULL;

	SBLASTRECORDCHK(&so->so_rcv, "somove 3");
	SBLASTMBUFCHK(&so->so_rcv, "somove 3");
	SBCHECK(&so->so_rcv);
	if (m == NULL)
		goto release;
	m->m_nextpkt = NULL;
	if (m->m_flags & M_PKTHDR) {
		m_resethdr(m);
		m->m_pkthdr.len = len;
	}

	/* Send window update to source peer as receive buffer has changed. */
	if (so->so_proto->pr_flags & PR_WANTRCVD && so->so_pcb)
		(so->so_proto->pr_usrreq)(so, PRU_RCVD, NULL,
		    NULL, NULL, NULL);

	/* Receive buffer did shrink by len bytes, adjust oob. */
	state = so->so_state;
	so->so_state &= ~SS_RCVATMARK;
	oobmark = so->so_oobmark;
	so->so_oobmark = oobmark > len ? oobmark - len : 0;
	if (oobmark) {
		if (oobmark == len)
			so->so_state |= SS_RCVATMARK;
		if (oobmark >= len)
			oobmark = 0;
	}

	/*
	 * Handle oob data.  If any malloc fails, ignore error.
	 * TCP urgent data is not very reliable anyway.
	 */
	while (((state & SS_RCVATMARK) || oobmark) &&
	    (so->so_options & SO_OOBINLINE)) {
		struct mbuf *o = NULL;

		if (state & SS_RCVATMARK) {
			o = m_get(wait, MT_DATA);
			state &= ~SS_RCVATMARK;
		} else if (oobmark) {
			o = m_split(m, oobmark, wait);
			if (o) {
				error = (*sosp->so_proto->pr_usrreq)(sosp,
				    PRU_SEND, m, NULL, NULL, NULL);
				if (error) {
					if (sosp->so_state & SS_CANTSENDMORE)
						error = EPIPE;
					m_freem(o);
					goto release;
				}
				len -= oobmark;
				so->so_splicelen += oobmark;
				m = o;
				o = m_get(wait, MT_DATA);
			}
			oobmark = 0;
		}
		if (o) {
			o->m_len = 1;
			*mtod(o, caddr_t) = *mtod(m, caddr_t);
			error = (*sosp->so_proto->pr_usrreq)(sosp, PRU_SENDOOB,
			    o, NULL, NULL, NULL);
			if (error) {
				if (sosp->so_state & SS_CANTSENDMORE)
					error = EPIPE;
				m_freem(m);
				goto release;
			}
			len -= 1;
			so->so_splicelen += 1;
			if (oobmark) {
				oobmark -= 1;
				if (oobmark == 0)
					state |= SS_RCVATMARK;
			}
			m_adj(m, 1);
		}
	}

	/* Append all remaining data to drain socket. */
	if (so->so_rcv.sb_cc == 0 || maxreached)
		sosp->so_state &= ~SS_ISSENDING;
	error = (*sosp->so_proto->pr_usrreq)(sosp, PRU_SEND, m, NULL, NULL,
	    NULL);
	if (error) {
		if (sosp->so_state & SS_CANTSENDMORE)
			error = EPIPE;
		goto release;
	}
	so->so_splicelen += len;

	/* Move several packets if possible. */
	if (!maxreached && nextrecord)
		goto nextpkt;

 release:
	sosp->so_state &= ~SS_ISSENDING;
	if (!error && maxreached && so->so_splicemax == so->so_splicelen)
		error = EFBIG;
	if (error)
		so->so_error = error;
	if (((so->so_state & SS_CANTRCVMORE) && so->so_rcv.sb_cc == 0) ||
	    (sosp->so_state & SS_CANTSENDMORE) || maxreached || error) {
		sounsplice(so, sosp, 1);
		return (0);
	}
	if (timerisset(&so->so_idletv))
		timeout_add_tv(&so->so_idleto, &so->so_idletv);
	return (1);
}

#endif /* SOCKET_SPLICE */

void
sorwakeup(struct socket *so)
{
	soassertlocked(so);

#ifdef SOCKET_SPLICE
	if (so->so_rcv.sb_flags & SB_SPLICE) {
		/*
		 * TCP has a sendbuffer that can handle multiple packets
		 * at once.  So queue the stream a bit to accumulate data.
		 * The sosplice thread will call somove() later and send
		 * the packets calling tcp_output() only once.
		 * In the UDP case, send out the packets immediately.
		 * Using a thread would make things slower.
		 */
		if (so->so_proto->pr_flags & PR_WANTRCVD)
			task_add(sosplice_taskq, &so->so_splicetask);
		else
			somove(so, M_DONTWAIT);
	}
	if (isspliced(so))
		return;
#endif
	sowakeup(so, &so->so_rcv);
	if (so->so_upcall)
		(*(so->so_upcall))(so, so->so_upcallarg, M_DONTWAIT);
}

void
sowwakeup(struct socket *so)
{
	soassertlocked(so);

#ifdef SOCKET_SPLICE
	if (so->so_snd.sb_flags & SB_SPLICE)
		task_add(sosplice_taskq, &so->so_sp->ssp_soback->so_splicetask);
#endif
	sowakeup(so, &so->so_snd);
}

int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m)
{
	int error = 0;

	soassertlocked(so);

	if (level != SOL_SOCKET) {
		if (so->so_proto->pr_ctloutput) {
			error = (*so->so_proto->pr_ctloutput)(PRCO_SETOPT, so,
			    level, optname, m);
			return (error);
		}
		error = ENOPROTOOPT;
	} else {
		switch (optname) {
		case SO_BINDANY:
			if ((error = suser(curproc)) != 0)	/* XXX */
				return (error);
			break;
		}

		switch (optname) {

		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof (struct linger) ||
			    mtod(m, struct linger *)->l_linger < 0 ||
			    mtod(m, struct linger *)->l_linger > SHRT_MAX)
				return (EINVAL);
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* FALLTHROUGH */

		case SO_BINDANY:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
		case SO_ZEROIZE:
			if (m == NULL || m->m_len < sizeof (int))
				return (EINVAL);
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

		case SO_DONTROUTE:
			if (m == NULL || m->m_len < sizeof (int))
				return (EINVAL);
			if (*mtod(m, int *))
				error = EOPNOTSUPP;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		    {
			u_long cnt;

			if (m == NULL || m->m_len < sizeof (int))
				return (EINVAL);
			cnt = *mtod(m, int *);
			if ((long)cnt <= 0)
				cnt = 1;
			switch (optname) {

			case SO_SNDBUF:
				if (so->so_state & SS_CANTSENDMORE)
					return (EINVAL);
				if (sbcheckreserve(cnt, so->so_snd.sb_wat) ||
				    sbreserve(so, &so->so_snd, cnt))
					return (ENOBUFS);
				so->so_snd.sb_wat = cnt;
				break;

			case SO_RCVBUF:
				if (so->so_state & SS_CANTRCVMORE)
					return (EINVAL);
				if (sbcheckreserve(cnt, so->so_rcv.sb_wat) ||
				    sbreserve(so, &so->so_rcv, cnt))
					return (ENOBUFS);
				so->so_rcv.sb_wat = cnt;
				break;

			case SO_SNDLOWAT:
				so->so_snd.sb_lowat =
				    (cnt > so->so_snd.sb_hiwat) ?
				    so->so_snd.sb_hiwat : cnt;
				break;
			case SO_RCVLOWAT:
				so->so_rcv.sb_lowat =
				    (cnt > so->so_rcv.sb_hiwat) ?
				    so->so_rcv.sb_hiwat : cnt;
				break;
			}
			break;
		    }

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			struct timeval tv;
			int val;

			if (m == NULL || m->m_len < sizeof (tv))
				return (EINVAL);
			memcpy(&tv, mtod(m, struct timeval *), sizeof tv);
			val = tvtohz(&tv);
			if (val > USHRT_MAX)
				return (EDOM);

			switch (optname) {

			case SO_SNDTIMEO:
				so->so_snd.sb_timeo = val;
				break;
			case SO_RCVTIMEO:
				so->so_rcv.sb_timeo = val;
				break;
			}
			break;
		    }

		case SO_RTABLE:
			if (so->so_proto->pr_domain &&
			    so->so_proto->pr_domain->dom_protosw &&
			    so->so_proto->pr_ctloutput) {
				struct domain *dom = so->so_proto->pr_domain;

				level = dom->dom_protosw->pr_protocol;
				error = (*so->so_proto->pr_ctloutput)
				    (PRCO_SETOPT, so, level, optname, m);
				return (error);
			}
			error = ENOPROTOOPT;
			break;

#ifdef SOCKET_SPLICE
		case SO_SPLICE:
			if (m == NULL) {
				error = sosplice(so, -1, 0, NULL);
			} else if (m->m_len < sizeof(int)) {
				return (EINVAL);
			} else if (m->m_len < sizeof(struct splice)) {
				error = sosplice(so, *mtod(m, int *), 0, NULL);
			} else {
				error = sosplice(so,
				    mtod(m, struct splice *)->sp_fd,
				    mtod(m, struct splice *)->sp_max,
				   &mtod(m, struct splice *)->sp_idle);
			}
			break;
#endif /* SOCKET_SPLICE */

		default:
			error = ENOPROTOOPT;
			break;
		}
		if (error == 0 && so->so_proto->pr_ctloutput) {
			(*so->so_proto->pr_ctloutput)(PRCO_SETOPT, so,
			    level, optname, m);
		}
	}

	return (error);
}

int
sogetopt(struct socket *so, int level, int optname, struct mbuf *m)
{
	int error = 0;

	soassertlocked(so);

	if (level != SOL_SOCKET) {
		if (so->so_proto->pr_ctloutput) {
			m->m_len = 0;

			error = (*so->so_proto->pr_ctloutput)(PRCO_GETOPT, so,
			    level, optname, m);
			if (error)
				return (error);
			return (0);
		} else
			return (ENOPROTOOPT);
	} else {
		m->m_len = sizeof (int);

		switch (optname) {

		case SO_LINGER:
			m->m_len = sizeof (struct linger);
			mtod(m, struct linger *)->l_onoff =
				so->so_options & SO_LINGER;
			mtod(m, struct linger *)->l_linger = so->so_linger;
			break;

		case SO_BINDANY:
		case SO_USELOOPBACK:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_TIMESTAMP:
		case SO_ZEROIZE:
			*mtod(m, int *) = so->so_options & optname;
			break;

		case SO_DONTROUTE:
			*mtod(m, int *) = 0;
			break;

		case SO_TYPE:
			*mtod(m, int *) = so->so_type;
			break;

		case SO_ERROR:
			*mtod(m, int *) = so->so_error;
			so->so_error = 0;
			break;

		case SO_SNDBUF:
			*mtod(m, int *) = so->so_snd.sb_hiwat;
			break;

		case SO_RCVBUF:
			*mtod(m, int *) = so->so_rcv.sb_hiwat;
			break;

		case SO_SNDLOWAT:
			*mtod(m, int *) = so->so_snd.sb_lowat;
			break;

		case SO_RCVLOWAT:
			*mtod(m, int *) = so->so_rcv.sb_lowat;
			break;

		case SO_SNDTIMEO:
		case SO_RCVTIMEO:
		    {
			struct timeval tv;
			int val = (optname == SO_SNDTIMEO ?
			    so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			m->m_len = sizeof(struct timeval);
			memset(&tv, 0, sizeof(tv));
			tv.tv_sec = val / hz;
			tv.tv_usec = (val % hz) * tick;
			memcpy(mtod(m, struct timeval *), &tv, sizeof tv);
			break;
		    }

		case SO_RTABLE:
			if (so->so_proto->pr_domain &&
			    so->so_proto->pr_domain->dom_protosw &&
			    so->so_proto->pr_ctloutput) {
				struct domain *dom = so->so_proto->pr_domain;

				level = dom->dom_protosw->pr_protocol;
				error = (*so->so_proto->pr_ctloutput)
				    (PRCO_GETOPT, so, level, optname, m);
				if (error)
					return (error);
				break;
			}
			return (ENOPROTOOPT);

#ifdef SOCKET_SPLICE
		case SO_SPLICE:
		    {
			off_t len;

			m->m_len = sizeof(off_t);
			len = so->so_sp ? so->so_sp->ssp_len : 0;
			memcpy(mtod(m, off_t *), &len, sizeof(off_t));
			break;
		    }
#endif /* SOCKET_SPLICE */

		case SO_PEERCRED:
			if (so->so_proto->pr_protocol == AF_UNIX) {
				struct unpcb *unp = sotounpcb(so);

				if (unp->unp_flags & UNP_FEIDS) {
					m->m_len = sizeof(unp->unp_connid);
					memcpy(mtod(m, caddr_t),
					    &(unp->unp_connid), m->m_len);
					break;
				}
				return (ENOTCONN);
			}
			return (EOPNOTSUPP);

		default:
			return (ENOPROTOOPT);
		}
		return (0);
	}
}

void
sohasoutofband(struct socket *so)
{
	KERNEL_LOCK();
	pgsigio(&so->so_sigio, SIGURG, 0);
	selwakeup(&so->so_rcv.sb_sel);
	KERNEL_UNLOCK();
}

int
soo_kqfilter(struct file *fp, struct knote *kn)
{
	struct socket *so = kn->kn_fp->f_data;
	struct sockbuf *sb;

	KERNEL_ASSERT_LOCKED();

	switch (kn->kn_filter) {
	case EVFILT_READ:
		if (so->so_options & SO_ACCEPTCONN)
			kn->kn_fop = &solisten_filtops;
		else
			kn->kn_fop = &soread_filtops;
		sb = &so->so_rcv;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &sowrite_filtops;
		sb = &so->so_snd;
		break;
	default:
		return (EINVAL);
	}

	SLIST_INSERT_HEAD(&sb->sb_sel.si_note, kn, kn_selnext);
	sb->sb_flagsintr |= SB_KNOTE;

	return (0);
}

void
filt_sordetach(struct knote *kn)
{
	struct socket *so = kn->kn_fp->f_data;

	KERNEL_ASSERT_LOCKED();

	SLIST_REMOVE(&so->so_rcv.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.si_note))
		so->so_rcv.sb_flagsintr &= ~SB_KNOTE;
}

int
filt_soread(struct knote *kn, long hint)
{
	struct socket *so = kn->kn_fp->f_data;
	int rv;

	kn->kn_data = so->so_rcv.sb_cc;
#ifdef SOCKET_SPLICE
	if (isspliced(so)) {
		rv = 0;
	} else
#endif /* SOCKET_SPLICE */
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		rv = 1;
	} else if (so->so_error) {	/* temporary udp error */
		rv = 1;
	} else if (kn->kn_sfflags & NOTE_LOWAT) {
		rv = (kn->kn_data >= kn->kn_sdata);
	} else {
		rv = (kn->kn_data >= so->so_rcv.sb_lowat);
	}

	return rv;
}

void
filt_sowdetach(struct knote *kn)
{
	struct socket *so = kn->kn_fp->f_data;

	KERNEL_ASSERT_LOCKED();

	SLIST_REMOVE(&so->so_snd.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.si_note))
		so->so_snd.sb_flagsintr &= ~SB_KNOTE;
}

int
filt_sowrite(struct knote *kn, long hint)
{
	struct socket *so = kn->kn_fp->f_data;
	int rv;

	kn->kn_data = sbspace(so, &so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		rv = 1;
	} else if (so->so_error) {	/* temporary udp error */
		rv = 1;
	} else if (((so->so_state & SS_ISCONNECTED) == 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
		rv = 0;
	} else if (kn->kn_sfflags & NOTE_LOWAT) {
		rv = (kn->kn_data >= kn->kn_sdata);
	} else {
		rv = (kn->kn_data >= so->so_snd.sb_lowat);
	}

	return (rv);
}

int
filt_solisten(struct knote *kn, long hint)
{
	struct socket *so = kn->kn_fp->f_data;

	kn->kn_data = so->so_qlen;

	return (kn->kn_data != 0);
}

#ifdef DDB
void
sobuf_print(struct sockbuf *,
    int (*)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))));

void
sobuf_print(struct sockbuf *sb,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	(*pr)("\tsb_cc: %lu\n", sb->sb_cc);
	(*pr)("\tsb_datacc: %lu\n", sb->sb_datacc);
	(*pr)("\tsb_hiwat: %lu\n", sb->sb_hiwat);
	(*pr)("\tsb_wat: %lu\n", sb->sb_wat);
	(*pr)("\tsb_mbcnt: %lu\n", sb->sb_mbcnt);
	(*pr)("\tsb_mbmax: %lu\n", sb->sb_mbmax);
	(*pr)("\tsb_lowat: %ld\n", sb->sb_lowat);
	(*pr)("\tsb_mb: %p\n", sb->sb_mb);
	(*pr)("\tsb_mbtail: %p\n", sb->sb_mbtail);
	(*pr)("\tsb_lastrecord: %p\n", sb->sb_lastrecord);
	(*pr)("\tsb_sel: ...\n");
	(*pr)("\tsb_flagsintr: %d\n", sb->sb_flagsintr);
	(*pr)("\tsb_flags: %i\n", sb->sb_flags);
	(*pr)("\tsb_timeo: %i\n", sb->sb_timeo);
}

void
so_print(void *v,
    int (*pr)(const char *, ...) __attribute__((__format__(__kprintf__,1,2))))
{
	struct socket *so = v;

	(*pr)("socket %p\n", so);
	(*pr)("so_type: %i\n", so->so_type);
	(*pr)("so_options: 0x%04x\n", so->so_options); /* %b */
	(*pr)("so_linger: %i\n", so->so_linger);
	(*pr)("so_state: 0x%04x\n", so->so_state);
	(*pr)("so_pcb: %p\n", so->so_pcb);
	(*pr)("so_proto: %p\n", so->so_proto);
	(*pr)("so_sigio: %p\n", so->so_sigio.sir_sigio);

	(*pr)("so_head: %p\n", so->so_head);
	(*pr)("so_onq: %p\n", so->so_onq);
	(*pr)("so_q0: @%p first: %p\n", &so->so_q0, TAILQ_FIRST(&so->so_q0));
	(*pr)("so_q: @%p first: %p\n", &so->so_q, TAILQ_FIRST(&so->so_q));
	(*pr)("so_eq: next: %p\n", TAILQ_NEXT(so, so_qe));
	(*pr)("so_q0len: %i\n", so->so_q0len);
	(*pr)("so_qlen: %i\n", so->so_qlen);
	(*pr)("so_qlimit: %i\n", so->so_qlimit);
	(*pr)("so_timeo: %i\n", so->so_timeo);
	(*pr)("so_obmark: %lu\n", so->so_oobmark);

	(*pr)("so_sp: %p\n", so->so_sp);
	if (so->so_sp != NULL) {
		(*pr)("\tssp_socket: %p\n", so->so_sp->ssp_socket);
		(*pr)("\tssp_soback: %p\n", so->so_sp->ssp_soback);
		(*pr)("\tssp_len: %lld\n",
		    (unsigned long long)so->so_sp->ssp_len);
		(*pr)("\tssp_max: %lld\n",
		    (unsigned long long)so->so_sp->ssp_max);
		(*pr)("\tssp_idletv: %lld %ld\n", so->so_sp->ssp_idletv.tv_sec,
		    so->so_sp->ssp_idletv.tv_usec);
		(*pr)("\tssp_idleto: %spending (@%i)\n",
		    timeout_pending(&so->so_sp->ssp_idleto) ? "" : "not ",
		    so->so_sp->ssp_idleto.to_time);
	}

	(*pr)("so_rcv:\n");
	sobuf_print(&so->so_rcv, pr);
	(*pr)("so_snd:\n");
	sobuf_print(&so->so_snd, pr);

	(*pr)("so_upcall: %p so_upcallarg: %p\n",
	    so->so_upcall, so->so_upcallarg);

	(*pr)("so_euid: %d so_ruid: %d\n", so->so_euid, so->so_ruid);
	(*pr)("so_egid: %d so_rgid: %d\n", so->so_egid, so->so_rgid);
	(*pr)("so_cpid: %d\n", so->so_cpid);
}
#endif

