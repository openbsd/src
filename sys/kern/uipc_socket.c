/*	$OpenBSD: uipc_socket.c,v 1.94 2011/07/04 00:33:36 mikeb Exp $	*/
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
#include <sys/resourcevar.h>
#include <net/route.h>
#include <sys/pool.h>

int	sosplice(struct socket *, int, off_t, struct timeval *);
void	sounsplice(struct socket *, struct socket *, int);
int	somove(struct socket *, int);
void	soidle(void *);

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

void
soinit(void)
{

	pool_init(&socket_pool, sizeof(struct socket), 0, 0, 0, "sockpl", NULL);
}

/*
 * Socket operation routines.
 * These routines are called by the routines in
 * sys_socket.c or from a system process, and
 * implement the semantics of socket operations by
 * switching out to the protocol specific routines.
 */
/*ARGSUSED*/
int
socreate(int dom, struct socket **aso, int type, int proto)
{
	struct proc *p = curproc;		/* XXX */
	struct protosw *prp;
	struct socket *so;
	int error, s;

	if (proto)
		prp = pffindproto(dom, proto, type);
	else
		prp = pffindtype(dom, type);
	if (prp == NULL || prp->pr_usrreq == 0)
		return (EPROTONOSUPPORT);
	if (prp->pr_type != type)
		return (EPROTOTYPE);
	s = splsoftnet();
	so = pool_get(&socket_pool, PR_WAITOK | PR_ZERO);
	TAILQ_INIT(&so->so_q0);
	TAILQ_INIT(&so->so_q);
	so->so_type = type;
	if (suser(p, 0) == 0)
		so->so_state = SS_PRIV;
	so->so_ruid = p->p_cred->p_ruid;
	so->so_euid = p->p_ucred->cr_uid;
	so->so_rgid = p->p_cred->p_rgid;
	so->so_egid = p->p_ucred->cr_gid;
	so->so_cpid = p->p_pid;
	so->so_proto = prp;
	error = (*prp->pr_usrreq)(so, PRU_ATTACH, NULL,
	    (struct mbuf *)(long)proto, NULL, p);
	if (error) {
		so->so_state |= SS_NOFDREF;
		sofree(so);
		splx(s);
		return (error);
	}
	splx(s);
	*aso = so;
	return (0);
}

int
sobind(struct socket *so, struct mbuf *nam, struct proc *p)
{
	int s = splsoftnet();
	int error;

	error = (*so->so_proto->pr_usrreq)(so, PRU_BIND, NULL, nam, NULL, p);
	splx(s);
	return (error);
}

int
solisten(struct socket *so, int backlog)
{
	int s, error;

#ifdef SOCKET_SPLICE
	if (so->so_splice || so->so_spliceback)
		return (EOPNOTSUPP);
#endif /* SOCKET_SPLICE */
	s = splsoftnet();
	error = (*so->so_proto->pr_usrreq)(so, PRU_LISTEN, NULL, NULL, NULL,
	    curproc);
	if (error) {
		splx(s);
		return (error);
	}
	if (TAILQ_FIRST(&so->so_q) == NULL)
		so->so_options |= SO_ACCEPTCONN;
	if (backlog < 0 || backlog > somaxconn)
		backlog = somaxconn;
	if (backlog < sominconn)
		backlog = sominconn;
	so->so_qlimit = backlog;
	splx(s);
	return (0);
}

/*
 *  Must be called at splsoftnet()
 */

void
sofree(struct socket *so)
{
	splsoftassert(IPL_SOFTNET);

	if (so->so_pcb || (so->so_state & SS_NOFDREF) == 0)
		return;
	if (so->so_head) {
		/*
		 * We must not decommission a socket that's on the accept(2)
		 * queue.  If we do, then accept(2) may hang after select(2)
		 * indicated that the listening socket was ready.
		 */
		if (!soqremque(so, 0))
			return;
	}
#ifdef SOCKET_SPLICE
	if (so->so_spliceback)
		sounsplice(so->so_spliceback, so, so->so_spliceback != so);
	if (so->so_splice)
		sounsplice(so, so->so_splice, 0);
#endif /* SOCKET_SPLICE */
	sbrelease(&so->so_snd);
	sorflush(so);
	pool_put(&socket_pool, so);
}

/*
 * Close a socket on last file table reference removal.
 * Initiate disconnect if connected.
 * Free socket when disconnect complete.
 */
int
soclose(struct socket *so)
{
	struct socket *so2;
	int s = splsoftnet();		/* conservative */
	int error = 0;

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
	if (so->so_pcb == 0)
		goto discard;
	if (so->so_state & SS_ISCONNECTED) {
		if ((so->so_state & SS_ISDISCONNECTING) == 0) {
			error = sodisconnect(so);
			if (error)
				goto drop;
		}
		if (so->so_options & SO_LINGER) {
			if ((so->so_state & SS_ISDISCONNECTING) &&
			    (so->so_state & SS_NBIO))
				goto drop;
			while (so->so_state & SS_ISCONNECTED) {
				error = tsleep(&so->so_timeo,
				    PSOCK | PCATCH, "netcls",
				    so->so_linger * hz);
				if (error)
					break;
			}
		}
	}
drop:
	if (so->so_pcb) {
		int error2 = (*so->so_proto->pr_usrreq)(so, PRU_DETACH, NULL,
		    NULL, NULL, curproc);
		if (error == 0)
			error = error2;
	}
discard:
	if (so->so_state & SS_NOFDREF)
		panic("soclose: NOFDREF");
	so->so_state |= SS_NOFDREF;
	sofree(so);
	splx(s);
	return (error);
}

/*
 * Must be called at splsoftnet.
 */
int
soabort(struct socket *so)
{
	splsoftassert(IPL_SOFTNET);

	return (*so->so_proto->pr_usrreq)(so, PRU_ABORT, NULL, NULL, NULL,
	   curproc);
}

int
soaccept(struct socket *so, struct mbuf *nam)
{
	int s = splsoftnet();
	int error = 0;

	if ((so->so_state & SS_NOFDREF) == 0)
		panic("soaccept: !NOFDREF");
	so->so_state &= ~SS_NOFDREF;
	if ((so->so_state & SS_ISDISCONNECTED) == 0 ||
	    (so->so_proto->pr_flags & PR_ABRTACPTDIS) == 0)
		error = (*so->so_proto->pr_usrreq)(so, PRU_ACCEPT, NULL,
		    nam, NULL, curproc);
	else
		error = ECONNABORTED;
	splx(s);
	return (error);
}

int
soconnect(struct socket *so, struct mbuf *nam)
{
	int s;
	int error;

	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	s = splsoftnet();
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
	splx(s);
	return (error);
}

int
soconnect2(struct socket *so1, struct socket *so2)
{
	int s = splsoftnet();
	int error;

	error = (*so1->so_proto->pr_usrreq)(so1, PRU_CONNECT2, NULL,
	    (struct mbuf *)so2, NULL, curproc);
	splx(s);
	return (error);
}

int
sodisconnect(struct socket *so)
{
	int s = splsoftnet();
	int error;

	if ((so->so_state & SS_ISCONNECTED) == 0) {
		error = ENOTCONN;
		goto bad;
	}
	if (so->so_state & SS_ISDISCONNECTING) {
		error = EALREADY;
		goto bad;
	}
	error = (*so->so_proto->pr_usrreq)(so, PRU_DISCONNECT, NULL, NULL,
	    NULL, curproc);
bad:
	splx(s);
	return (error);
}

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
	struct mbuf **mp;
	struct mbuf *m;
	long space, len, mlen, clen = 0;
	quad_t resid;
	int error, s, dontroute;
	int atomic = sosendallatonce(so) || top;

	if (uio)
		resid = uio->uio_resid;
	else
		resid = top->m_pkthdr.len;
	/*
	 * In theory resid should be unsigned (since uio->uio_resid is).
	 * However, space must be signed, as it might be less than 0
	 * if we over-committed, and we must use a signed comparison
	 * of space and resid.  On the other hand, a negative resid
	 * causes us to loop sending 0-length segments to the protocol.
	 * MSG_EOR on a SOCK_STREAM socket is also invalid.
	 */
	if (resid < 0 ||
	    (so->so_type == SOCK_STREAM && (flags & MSG_EOR))) {
		error = EINVAL;
		goto out;
	}
	dontroute =
	    (flags & MSG_DONTROUTE) && (so->so_options & SO_DONTROUTE) == 0 &&
	    (so->so_proto->pr_flags & PR_ATOMIC);
	if (uio && uio->uio_procp)
		uio->uio_procp->p_stats->p_ru.ru_msgsnd++;
	if (control)
		clen = control->m_len;
#define	snderr(errno)	{ error = errno; splx(s); goto release; }

restart:
	if ((error = sblock(&so->so_snd, SBLOCKWAIT(flags))) != 0)
		goto out;
	so->so_state |= SS_ISSENDING;
	do {
		s = splsoftnet();
		if (so->so_state & SS_CANTSENDMORE)
			snderr(EPIPE);
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			goto release;
		}
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
				if ((so->so_state & SS_ISCONFIRMING) == 0 &&
				    !(resid == 0 && clen != 0))
					snderr(ENOTCONN);
			} else if (addr == 0)
				snderr(EDESTADDRREQ);
		}
		space = sbspace(&so->so_snd);
		if (flags & MSG_OOB)
			space += 1024;
		if ((atomic && resid > so->so_snd.sb_hiwat) ||
		    clen > so->so_snd.sb_hiwat)
			snderr(EMSGSIZE);
		if (space < resid + clen &&
		    (atomic || space < so->so_snd.sb_lowat || space < clen)) {
			if (so->so_state & SS_NBIO)
				snderr(EWOULDBLOCK);
			sbunlock(&so->so_snd);
			error = sbwait(&so->so_snd);
			so->so_state &= ~SS_ISSENDING;
			splx(s);
			if (error)
				goto out;
			goto restart;
		}
		splx(s);
		mp = &top;
		space -= clen;
		do {
			if (uio == NULL) {
				/*
				 * Data is prepackaged in "top".
				 */
				resid = 0;
				if (flags & MSG_EOR)
					top->m_flags |= M_EOR;
			} else do {
				if (top == 0) {
					MGETHDR(m, M_WAIT, MT_DATA);
					mlen = MHLEN;
					m->m_pkthdr.len = 0;
					m->m_pkthdr.rcvif = (struct ifnet *)0;
				} else {
					MGET(m, M_WAIT, MT_DATA);
					mlen = MLEN;
				}
				if (resid >= MINCLSIZE && space >= MCLBYTES) {
					MCLGET(m, M_NOWAIT);
					if ((m->m_flags & M_EXT) == 0)
						goto nopages;
					mlen = MCLBYTES;
					if (atomic && top == 0) {
						len = lmin(MCLBYTES - max_hdr, resid);
						m->m_data += max_hdr;
					} else
						len = lmin(MCLBYTES, resid);
					space -= len;
				} else {
nopages:
					len = lmin(lmin(mlen, resid), space);
					space -= len;
					/*
					 * For datagram protocols, leave room
					 * for protocol headers in first mbuf.
					 */
					if (atomic && top == 0 && len < mlen)
						MH_ALIGN(m, len);
				}
				error = uiomove(mtod(m, caddr_t), (int)len,
				    uio);
				resid = uio->uio_resid;
				m->m_len = len;
				*mp = m;
				top->m_pkthdr.len += len;
				if (error)
					goto release;
				mp = &m->m_next;
				if (resid <= 0) {
					if (flags & MSG_EOR)
						top->m_flags |= M_EOR;
					break;
				}
			} while (space > 0 && atomic);
			if (dontroute)
				so->so_options |= SO_DONTROUTE;
			s = splsoftnet();		/* XXX */
			if (resid <= 0)
				so->so_state &= ~SS_ISSENDING;
			error = (*so->so_proto->pr_usrreq)(so,
			    (flags & MSG_OOB) ? PRU_SENDOOB : PRU_SEND,
			    top, addr, control, curproc);
			splx(s);
			if (dontroute)
				so->so_options &= ~SO_DONTROUTE;
			clen = 0;
			control = 0;
			top = 0;
			mp = &top;
			if (error)
				goto release;
		} while (resid && space > 0);
	} while (resid);

release:
	so->so_state &= ~SS_ISSENDING;
	sbunlock(&so->so_snd);
out:
	if (top)
		m_freem(top);
	if (control)
		m_freem(control);
	return (error);
}

/*
 * Implement receive operations on a socket.
 * We depend on the way that records are added to the sockbuf
 * by sbappend*.  In particular, each record (mbufs linked through m_next)
 * must begin with an address if the protocol so specifies,
 * followed by an optional mbuf or mbufs containing ancillary data,
 * and then zero or more mbufs of data.
 * In order to avoid blocking network interrupts for the entire time here,
 * we splx() while doing the actual copy to user space.
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
	int flags, len, error, s, offset;
	struct protosw *pr = so->so_proto;
	struct mbuf *nextrecord;
	int moff, type = 0;
	size_t orig_resid = uio->uio_resid;
	int uio_error = 0;
	int resid;

	mp = mp0;
	if (paddr)
		*paddr = 0;
	if (controlp)
		*controlp = 0;
	if (flagsp)
		flags = *flagsp &~ MSG_EOR;
	else
		flags = 0;
	if (so->so_state & SS_NBIO)
		flags |= MSG_DONTWAIT;
	if (flags & MSG_OOB) {
		m = m_get(M_WAIT, MT_DATA);
		error = (*pr->pr_usrreq)(so, PRU_RCVOOB, m,
		    (struct mbuf *)(long)(flags & MSG_PEEK), NULL, curproc);
		if (error)
			goto bad;
		do {
			error = uiomove(mtod(m, caddr_t),
			    (int) min(uio->uio_resid, m->m_len), uio);
			m = m_free(m);
		} while (uio->uio_resid && error == 0 && m);
bad:
		if (m)
			m_freem(m);
		return (error);
	}
	if (mp)
		*mp = NULL;
	if (so->so_state & SS_ISCONFIRMING && uio->uio_resid)
		(*pr->pr_usrreq)(so, PRU_RCVD, NULL, NULL, NULL, curproc);

restart:
	if ((error = sblock(&so->so_rcv, SBLOCKWAIT(flags))) != 0)
		return (error);
	s = splsoftnet();

	m = so->so_rcv.sb_mb;
#ifdef SOCKET_SPLICE
	if (so->so_splice)
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
		    if (so->so_splice == NULL)
#endif /* SOCKET_SPLICE */
			panic("receive 1");
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
		if ((so->so_state & SS_NBIO) || (flags & MSG_DONTWAIT)) {
			error = EWOULDBLOCK;
			goto release;
		}
		SBLASTRECORDCHK(&so->so_rcv, "soreceive sbwait 1");
		SBLASTMBUFCHK(&so->so_rcv, "soreceive sbwait 1");
		sbunlock(&so->so_rcv);
		error = sbwait(&so->so_rcv);
		splx(s);
		if (error)
			return (error);
		goto restart;
	}
dontblock:
	/*
	 * On entry here, m points to the first record of the socket buffer.
	 * While we process the initial mbufs containing address and control
	 * info, we save a copy of m->m_nextpkt into nextrecord.
	 */
	if (uio->uio_procp)
		uio->uio_procp->p_stats->p_ru.ru_msgrcv++;
	KASSERT(m == so->so_rcv.sb_mb);
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 1");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 1");
	nextrecord = m->m_nextpkt;
	if (pr->pr_flags & PR_ADDR) {
#ifdef DIAGNOSTIC
		if (m->m_type != MT_SONAME)
			panic("receive 1a");
#endif
		orig_resid = 0;
		if (flags & MSG_PEEK) {
			if (paddr)
				*paddr = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (paddr) {
				*paddr = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
	}
	while (m && m->m_type == MT_CONTROL && error == 0) {
		if (flags & MSG_PEEK) {
			if (controlp)
				*controlp = m_copy(m, 0, m->m_len);
			m = m->m_next;
		} else {
			sbfree(&so->so_rcv, m);
			if (controlp) {
				if (pr->pr_domain->dom_externalize &&
				    mtod(m, struct cmsghdr *)->cmsg_type ==
				    SCM_RIGHTS)
				   error = (*pr->pr_domain->dom_externalize)(m,
				       controllen);
				*controlp = m;
				so->so_rcv.sb_mb = m->m_next;
				m->m_next = 0;
				m = so->so_rcv.sb_mb;
			} else {
				/*
				 * Dispose of any SCM_RIGHTS message that went
				 * through the read path rather than recv.
				 */
				if (pr->pr_domain->dom_dispose &&
				    mtod(m, struct cmsghdr *)->cmsg_type == SCM_RIGHTS)
					pr->pr_domain->dom_dispose(m);
				MFREE(m, so->so_rcv.sb_mb);
				m = so->so_rcv.sb_mb;
			}
		}
		if (controlp) {
			orig_resid = 0;
			controlp = &(*controlp)->m_next;
		}
	}

	/*
	 * If m is non-NULL, we have some data to read.  From now on,
	 * make sure to keep sb_lastrecord consistent when working on
	 * the last packet on the chain (nextrecord == NULL) and we
	 * change m->m_nextpkt.
	 */
	if (m) {
		if ((flags & MSG_PEEK) == 0) {
			m->m_nextpkt = nextrecord;
			/*
			 * If nextrecord == NULL (this is a single chain),
			 * then sb_lastrecord may not be valid here if m
			 * was changed earlier.
			 */
			if (nextrecord == NULL) {
				KASSERT(so->so_rcv.sb_mb == m);
				so->so_rcv.sb_lastrecord = m;
			}
		}
		type = m->m_type;
		if (type == MT_OOBDATA)
			flags |= MSG_OOB;
		if (m->m_flags & M_BCAST)
			flags |= MSG_BCAST;
		if (m->m_flags & M_MCAST)
			flags |= MSG_MCAST;
	} else {
		if ((flags & MSG_PEEK) == 0) {
			KASSERT(so->so_rcv.sb_mb == m);
			so->so_rcv.sb_mb = nextrecord;
			SB_EMPTY_FIXUP(&so->so_rcv);
		}
	}
	SBLASTRECORDCHK(&so->so_rcv, "soreceive 2");
	SBLASTMBUFCHK(&so->so_rcv, "soreceive 2");

	moff = 0;
	offset = 0;
	while (m && uio->uio_resid > 0 && error == 0) {
		if (m->m_type == MT_OOBDATA) {
			if (type != MT_OOBDATA)
				break;
		} else if (type == MT_OOBDATA)
			break;
#ifdef DIAGNOSTIC
		else if (m->m_type != MT_DATA && m->m_type != MT_HEADER)
			panic("receive 3");
#endif
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
			splx(s);
			uio_error =
				uiomove(mtod(m, caddr_t) + moff, (int)len,
					uio);
			s = splsoftnet();
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
					MFREE(m, so->so_rcv.sb_mb);
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
			error = sbwait(&so->so_rcv);
			if (error) {
				sbunlock(&so->so_rcv);
				splx(s);
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
		sbunlock(&so->so_rcv);
		splx(s);
		goto restart;
	}

	if (uio_error)
		error = uio_error;

	if (flagsp)
		*flagsp |= flags;
release:
	sbunlock(&so->so_rcv);
	splx(s);
	return (error);
}

int
soshutdown(struct socket *so, int how)
{
	struct protosw *pr = so->so_proto;

	switch (how) {
	case SHUT_RD:
	case SHUT_RDWR:
		sorflush(so);
		if (how == SHUT_RD)
			return (0);
		/* FALLTHROUGH */
	case SHUT_WR:
		return (*pr->pr_usrreq)(so, PRU_SHUTDOWN, NULL, NULL, NULL,
		    curproc);
	default:
		return (EINVAL);
	}
}

void
sorflush(struct socket *so)
{
	struct sockbuf *sb = &so->so_rcv;
	struct protosw *pr = so->so_proto;
	int s;
	struct sockbuf asb;

	sb->sb_flags |= SB_NOINTR;
	(void) sblock(sb, M_WAITOK);
	s = splnet();
	socantrcvmore(so);
	sbunlock(sb);
	asb = *sb;
	bzero(sb, sizeof (*sb));
	/* XXX - the bzero stumps all over so_rcv */
	if (asb.sb_flags & SB_KNOTE) {
		sb->sb_sel.si_note = asb.sb_sel.si_note;
		sb->sb_flags = SB_KNOTE;
	}
	splx(s);
	if (pr->pr_flags & PR_RIGHTS && pr->pr_domain->dom_dispose)
		(*pr->pr_domain->dom_dispose)(asb.sb_mb);
	sbrelease(&asb);
}

#ifdef SOCKET_SPLICE
int
sosplice(struct socket *so, int fd, off_t max, struct timeval *tv)
{
	struct file	*fp;
	struct socket	*sosp;
	int		 s, error = 0;

	if ((so->so_proto->pr_flags & PR_SPLICE) == 0)
		return (EPROTONOSUPPORT);
	if (so->so_options & SO_ACCEPTCONN)
		return (EOPNOTSUPP);
	if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) == 0)
		return (ENOTCONN);

	/* If no fd is given, unsplice by removing existing link. */
	if (fd < 0) {
		s = splsoftnet();
		if (so->so_splice)
			sounsplice(so, so->so_splice, 1);
		splx(s);
		return (0);
	}

	if (max && max < 0)
		return (EINVAL);

	if (tv && (tv->tv_sec < 0 || tv->tv_usec < 0))
		return (EINVAL);

	/* Find sosp, the drain socket where data will be spliced into. */
	if ((error = getsock(curproc->p_fd, fd, &fp)) != 0)
		return (error);
	sosp = fp->f_data;

	/* Lock both receive and send buffer. */
	if ((error = sblock(&so->so_rcv,
	    (so->so_state & SS_NBIO) ? M_NOWAIT : M_WAITOK)) != 0) {
		FRELE(fp);
		return (error);
	}
	if ((error = sblock(&sosp->so_snd, M_WAITOK)) != 0) {
		sbunlock(&so->so_rcv);
		FRELE(fp);
		return (error);
	}
	s = splsoftnet();

	if (so->so_splice || sosp->so_spliceback) {
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
	so->so_splice = sosp;
	sosp->so_spliceback = so;
	so->so_splicelen = 0;
	so->so_splicemax = max;
	if (tv)
		so->so_idletv = *tv;
	else
		timerclear(&so->so_idletv);
	timeout_set(&so->so_idleto, soidle, so);

	/*
	 * To prevent softnet interrupt from calling somove() while
	 * we sleep, the socket buffers are not marked as spliced yet.
	 */
	if (somove(so, M_WAIT)) {
		so->so_rcv.sb_flags |= SB_SPLICE;
		sosp->so_snd.sb_flags |= SB_SPLICE;
	}

 release:
	splx(s);
	sbunlock(&sosp->so_snd);
	sbunlock(&so->so_rcv);
	FRELE(fp);
	return (error);
}

void
sounsplice(struct socket *so, struct socket *sosp, int wakeup)
{
	splsoftassert(IPL_SOFTNET);

	timeout_del(&so->so_idleto);
	sosp->so_snd.sb_flags &= ~SB_SPLICE;
	so->so_rcv.sb_flags &= ~SB_SPLICE;
	so->so_splice = sosp->so_spliceback = NULL;
	if (wakeup && soreadable(so))
		sorwakeup(so);
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
	struct socket	*sosp = so->so_splice;
	struct mbuf	*m = NULL, **mp;
	u_long		 len, off, oobmark;
	long		 space;
	int		 error = 0, maxreached = 0;
	short		 state;

	splsoftassert(IPL_SOFTNET);

	if (so->so_error) {
		error = so->so_error;
		goto release;
	}
	if (sosp->so_state & SS_CANTSENDMORE) {
		error = EPIPE;
		goto release;
	}
	if (sosp->so_error) {
		error = sosp->so_error;
		goto release;
	}
	if ((sosp->so_state & SS_ISCONNECTED) == 0)
		goto release;

	/* Calculate how many bytes can be copied now. */
	len = so->so_rcv.sb_cc;
	if (len == 0)
		goto release;
	if (so->so_splicemax) {
		KASSERT(so->so_splicelen < so->so_splicemax);
		if (so->so_splicemax <= so->so_splicelen + len) {
			len = so->so_splicemax - so->so_splicelen;
			maxreached = 1;
		}
	}
	space = sbspace(&sosp->so_snd);
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

	/* Take at most len mbufs out of receive buffer. */
	m = so->so_rcv.sb_mb;
	for (off = 0, mp = &m; off < len;
	    off += (*mp)->m_len, mp = &(*mp)->m_next) {
		u_long size = len - off;

		if ((*mp)->m_len > size) {
			if (!maxreached || (*mp = m_copym(
			    so->so_rcv.sb_mb, 0, size, wait)) == NULL) {
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
		}
	}
	*mp = NULL;
	SB_EMPTY_FIXUP(&so->so_rcv);
	so->so_rcv.sb_lastrecord = so->so_rcv.sb_mb;

	SBLASTRECORDCHK(&so->so_rcv, "somove");
	SBLASTMBUFCHK(&so->so_rcv, "somove");
	KDASSERT(m->m_nextpkt == NULL);
	KASSERT(so->so_rcv.sb_mb == so->so_rcv.sb_lastrecord);
#ifdef SOCKBUF_DEBUG
	sbcheck(&so->so_rcv);
#endif

	/* Send window update to source peer if receive buffer has changed. */
	if (m)
		(so->so_proto->pr_usrreq)(so, PRU_RCVD, NULL,
		    (struct mbuf *)0L, NULL, NULL);

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
	while (m && ((state & SS_RCVATMARK) || oobmark) &&
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
				m = NULL;
				if (error) {
					m_freem(o);
					if (sosp->so_state & SS_CANTSENDMORE)
						error = EPIPE;
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
	if (m) {
		if (so->so_rcv.sb_cc == 0 || maxreached)
			sosp->so_state &= ~SS_ISSENDING;
		error = (*sosp->so_proto->pr_usrreq)(sosp, PRU_SEND, m, NULL,
		    NULL, NULL);
		m = NULL;
		if (error) {
			if (sosp->so_state & SS_CANTSENDMORE)
				error = EPIPE;
			goto release;
		}
		so->so_splicelen += len;
	}

 release:
	if (m)
		m_freem(m);
	sosp->so_state &= ~SS_ISSENDING;
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

void
sorwakeup(struct socket *so)
{
	if (so->so_rcv.sb_flags & SB_SPLICE) {
		(void) somove(so, M_DONTWAIT);
		return;
	}
	_sorwakeup(so);
}

void
sowwakeup(struct socket *so)
{
	if (so->so_snd.sb_flags & SB_SPLICE)
		(void) somove(so->so_spliceback, M_DONTWAIT);
	_sowwakeup(so);
}

void
soidle(void *arg)
{
	struct socket *so = arg;
	int s;

	s = splsoftnet();
	so->so_error = ETIMEDOUT;
	sounsplice(so, so->so_splice, 1);
	splx(s);
}
#endif /* SOCKET_SPLICE */

int
sosetopt(struct socket *so, int level, int optname, struct mbuf *m0)
{
	int error = 0;
	struct mbuf *m = m0;

	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput)
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
		error = ENOPROTOOPT;
	} else {
		switch (optname) {
		case SO_BINDANY:
			if ((error = suser(curproc, 0)) != 0)	/* XXX */
				goto bad;
			break;
		}

		switch (optname) {

		case SO_LINGER:
			if (m == NULL || m->m_len != sizeof (struct linger) ||
			    mtod(m, struct linger *)->l_linger < 0 ||
			    mtod(m, struct linger *)->l_linger > SHRT_MAX) {
				error = EINVAL;
				goto bad;
			}
			so->so_linger = mtod(m, struct linger *)->l_linger;
			/* FALLTHROUGH */

		case SO_BINDANY:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_DONTROUTE:
		case SO_USELOOPBACK:
		case SO_BROADCAST:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_OOBINLINE:
		case SO_JUMBO:
		case SO_TIMESTAMP:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			if (*mtod(m, int *))
				so->so_options |= optname;
			else
				so->so_options &= ~optname;
			break;

		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_SNDLOWAT:
		case SO_RCVLOWAT:
		    {
			u_long cnt;

			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				goto bad;
			}
			cnt = *mtod(m, int *);
			if ((long)cnt <= 0)
				cnt = 1;
			switch (optname) {

			case SO_SNDBUF:
				if (so->so_state & SS_CANTSENDMORE) {
					error = EINVAL;
					goto bad;
				}
				if (sbcheckreserve(cnt, so->so_snd.sb_wat) ||
				    sbreserve(&so->so_snd, cnt)) {
					error = ENOBUFS;
					goto bad;
				}
				so->so_snd.sb_wat = cnt;
				break;

			case SO_RCVBUF:
				if (so->so_state & SS_CANTRCVMORE) {
					error = EINVAL;
					goto bad;
				}
				if (sbcheckreserve(cnt, so->so_rcv.sb_wat) ||
				    sbreserve(&so->so_rcv, cnt)) {
					error = ENOBUFS;
					goto bad;
				}
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
			struct timeval *tv;
			u_short val;

			if (m == NULL || m->m_len < sizeof (*tv)) {
				error = EINVAL;
				goto bad;
			}
			tv = mtod(m, struct timeval *);
			if (tv->tv_sec > (USHRT_MAX - tv->tv_usec / tick) / hz) {
				error = EDOM;
				goto bad;
			}
			val = tv->tv_sec * hz + tv->tv_usec / tick;
			if (val == 0 && tv->tv_usec != 0)
				val = 1;

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
			if (so->so_proto && so->so_proto->pr_domain &&
			    so->so_proto->pr_domain->dom_protosw &&
			    so->so_proto->pr_ctloutput) {
				struct domain *dom = so->so_proto->pr_domain;

				level = dom->dom_protosw->pr_protocol;
				return ((*so->so_proto->pr_ctloutput)
				    (PRCO_SETOPT, so, level, optname, &m0));
			}
			error = ENOPROTOOPT;
			break;

#ifdef SOCKET_SPLICE
		case SO_SPLICE:
			if (m == NULL) {
				error = sosplice(so, -1, 0, NULL);
			} else if (m->m_len < sizeof(int)) {
				error = EINVAL;
				goto bad;
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
		if (error == 0 && so->so_proto && so->so_proto->pr_ctloutput) {
			(void) ((*so->so_proto->pr_ctloutput)
				  (PRCO_SETOPT, so, level, optname, &m0));
			m = NULL;	/* freed by protocol */
		}
	}
bad:
	if (m)
		(void) m_free(m);
	return (error);
}

int
sogetopt(struct socket *so, int level, int optname, struct mbuf **mp)
{
	struct mbuf *m;

	if (level != SOL_SOCKET) {
		if (so->so_proto && so->so_proto->pr_ctloutput) {
			return ((*so->so_proto->pr_ctloutput)
				  (PRCO_GETOPT, so, level, optname, mp));
		} else
			return (ENOPROTOOPT);
	} else {
		m = m_get(M_WAIT, MT_SOOPTS);
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
		case SO_DONTROUTE:
		case SO_DEBUG:
		case SO_KEEPALIVE:
		case SO_REUSEADDR:
		case SO_REUSEPORT:
		case SO_BROADCAST:
		case SO_OOBINLINE:
		case SO_JUMBO:
		case SO_TIMESTAMP:
			*mtod(m, int *) = so->so_options & optname;
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
			int val = (optname == SO_SNDTIMEO ?
			    so->so_snd.sb_timeo : so->so_rcv.sb_timeo);

			m->m_len = sizeof(struct timeval);
			mtod(m, struct timeval *)->tv_sec = val / hz;
			mtod(m, struct timeval *)->tv_usec =
			    (val % hz) * tick;
			break;
		    }

		case SO_RTABLE:
			(void)m_free(m);
			if (so->so_proto && so->so_proto->pr_domain &&
			    so->so_proto->pr_domain->dom_protosw &&
			    so->so_proto->pr_ctloutput) {
				struct domain *dom = so->so_proto->pr_domain;

				level = dom->dom_protosw->pr_protocol;
				return ((*so->so_proto->pr_ctloutput)
				    (PRCO_GETOPT, so, level, optname, mp));
			}
			return (ENOPROTOOPT);
			break;

#ifdef SOCKET_SPLICE
		case SO_SPLICE:
		    {
			int s = splsoftnet();

			m->m_len = sizeof(off_t);
			*mtod(m, off_t *) = so->so_splicelen;
			splx(s);
			break;
		    }
#endif /* SOCKET_SPLICE */

		case SO_PEERCRED:
			if (so->so_proto->pr_protocol == AF_UNIX) {
				struct unpcb *unp = sotounpcb(so);

				if (unp->unp_flags & UNP_FEIDS) {
					m->m_len = sizeof(unp->unp_connid);
					bcopy((caddr_t)(&(unp->unp_connid)),
					    mtod(m, caddr_t),
					    m->m_len);
					break;
				}
				(void)m_free(m);
				return (ENOTCONN);
			}
			(void)m_free(m);
			return (EOPNOTSUPP);
			break;

		default:
			(void)m_free(m);
			return (ENOPROTOOPT);
		}
		*mp = m;
		return (0);
	}
}

void
sohasoutofband(struct socket *so)
{
	csignal(so->so_pgid, SIGURG, so->so_siguid, so->so_sigeuid);
	selwakeup(&so->so_rcv.sb_sel);
}

int
soo_kqfilter(struct file *fp, struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	struct sockbuf *sb;
	int s;

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

	s = splnet();
	SLIST_INSERT_HEAD(&sb->sb_sel.si_note, kn, kn_selnext);
	sb->sb_flags |= SB_KNOTE;
	splx(s);
	return (0);
}

void
filt_sordetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	int s = splnet();

	SLIST_REMOVE(&so->so_rcv.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_rcv.sb_sel.si_note))
		so->so_rcv.sb_flags &= ~SB_KNOTE;
	splx(s);
}

/*ARGSUSED*/
int
filt_soread(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = so->so_rcv.sb_cc;
#ifdef SOCKET_SPLICE
	if (so->so_splice)
		return (0);
#endif /* SOCKET_SPLICE */
	if (so->so_state & SS_CANTRCVMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_rcv.sb_lowat);
}

void
filt_sowdetach(struct knote *kn)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;
	int s = splnet();

	SLIST_REMOVE(&so->so_snd.sb_sel.si_note, kn, knote, kn_selnext);
	if (SLIST_EMPTY(&so->so_snd.sb_sel.si_note))
		so->so_snd.sb_flags &= ~SB_KNOTE;
	splx(s);
}

/*ARGSUSED*/
int
filt_sowrite(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = sbspace(&so->so_snd);
	if (so->so_state & SS_CANTSENDMORE) {
		kn->kn_flags |= EV_EOF;
		kn->kn_fflags = so->so_error;
		return (1);
	}
	if (so->so_error)	/* temporary udp error */
		return (1);
	if (((so->so_state & SS_ISCONNECTED) == 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED))
		return (0);
	if (kn->kn_sfflags & NOTE_LOWAT)
		return (kn->kn_data >= kn->kn_sdata);
	return (kn->kn_data >= so->so_snd.sb_lowat);
}

/*ARGSUSED*/
int
filt_solisten(struct knote *kn, long hint)
{
	struct socket *so = (struct socket *)kn->kn_fp->f_data;

	kn->kn_data = so->so_qlen;
	return (so->so_qlen != 0);
}
