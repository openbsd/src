/*	$OpenBSD: tcp_usrreq.c,v 1.84 2004/04/26 18:16:09 markus Exp $	*/
/*	$NetBSD: tcp_usrreq.c,v 1.20 1996/02/13 23:44:16 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/domain.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

/*
 * TCP protocol interface to socket abstraction.
 */
extern	char *tcpstates[];
extern	int tcptv_keep_init;

extern int tcp_rst_ppslim;

/* from in_pcb.c */
extern	struct baddynamicports baddynamicports;

#ifndef TCP_SENDSPACE
#define	TCP_SENDSPACE	1024*16
#endif
u_int	tcp_sendspace = TCP_SENDSPACE;
#ifndef TCP_RECVSPACE
#define	TCP_RECVSPACE	1024*16
#endif
u_int	tcp_recvspace = TCP_RECVSPACE;

int *tcpctl_vars[TCPCTL_MAXID] = TCPCTL_VARS;

struct	inpcbtable tcbtable;

int tcp_ident(void *, size_t *, void *, size_t, int);

#ifdef INET6
int
tcp6_usrreq(so, req, m, nam, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
	struct proc *p;
{

	return tcp_usrreq(so, req, m, nam, control);
}
#endif

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/*ARGSUSED*/
int
tcp_usrreq(so, req, m, nam, control)
	struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	struct sockaddr_in *sin;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	int s;
	int error = 0;
	int ostate;

	if (req == PRU_CONTROL) {
#ifdef INET6
		if (sotopf(so) == PF_INET6)
			return in6_control(so, (u_long)m, (caddr_t)nam,
			    (struct ifnet *)control, 0);
		else
#endif /* INET6 */
			return (in_control(so, (u_long)m, (caddr_t)nam,
			    (struct ifnet *)control));
	}
	if (control && control->m_len) {
		m_freem(control);
		if (m)
			m_freem(m);
		return (EINVAL);
	}

	s = splsoftnet();
	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidiary (struct tcpcb).
	 */
	if (inp == 0 && req != PRU_ATTACH) {
		splx(s);
		/*
		 * The following corrects an mbuf leak under rare
		 * circumstances
		 */
		if (m && (req == PRU_SEND || req == PRU_SENDOOB))
			m_freem(m);
		return (EINVAL);		/* XXX */
	}
	if (inp) {
		tp = intotcpcb(inp);
		/* WHAT IF TP IS 0? */
#ifdef KPROF
		tcp_acounts[tp->t_state][req]++;
#endif
		ostate = tp->t_state;
	} else
		ostate = 0;
	switch (req) {

	/*
	 * TCP attaches to socket via PRU_ATTACH, reserving space,
	 * and an internet control block.
	 */
	case PRU_ATTACH:
		if (inp) {
			error = EISCONN;
			break;
		}
		error = tcp_attach(so);
		if (error)
			break;
		if ((so->so_options & SO_LINGER) && so->so_linger == 0)
			so->so_linger = TCP_LINGERTIME;
		tp = sototcpcb(so);
		break;

	/*
	 * PRU_DETACH detaches the TCP protocol from the socket.
	 * If the protocol state is non-embryonic, then can't
	 * do this directly: have to initiate a PRU_DISCONNECT,
	 * which may finish later; embryonic TCB's can just
	 * be discarded here.
	 */
	case PRU_DETACH:
		tp = tcp_disconnect(tp);
		break;

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			error = in6_pcbbind(inp, nam);
		else
#endif
			error = in_pcbbind(inp, nam);
		if (error)
			break;
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
		if (inp->inp_lport == 0) {
#ifdef INET6
			if (inp->inp_flags & INP_IPV6)
				error = in6_pcbbind(inp, NULL);
			else
#endif
				error = in_pcbbind(inp, NULL);
		}
		/* If the in_pcbbind() above is called, the tp->pf
		   should still be whatever it was before. */
		if (error == 0)
			tp->t_state = TCPS_LISTEN;
		break;

	/*
	 * Initiate connection to peer.
	 * Create a template for use in transmissions on this connection.
	 * Enter SYN_SENT state, and mark socket as connecting.
	 * Start keep-alive timer, and seed output sequence space.
	 * Send initial segment on connection.
	 */
	case PRU_CONNECT:
		sin = mtod(nam, struct sockaddr_in *);

#ifdef INET6
		if (sin->sin_family == AF_INET6) {
			struct in6_addr *in6_addr = &mtod(nam,
			    struct sockaddr_in6 *)->sin6_addr;

			if (IN6_IS_ADDR_UNSPECIFIED(in6_addr) ||
			    IN6_IS_ADDR_MULTICAST(in6_addr) ||
			    (IN6_IS_ADDR_V4MAPPED(in6_addr) &&
			    ((in6_addr->s6_addr32[3] == INADDR_ANY) ||
			    IN_MULTICAST(in6_addr->s6_addr32[3]) ||
			    in_broadcast(sin->sin_addr, NULL)))) {
				error = EINVAL;
				break;
			}

			if (inp->inp_lport == 0) {
				error = in6_pcbbind(inp, NULL);
				if (error)
					break;
			}
			error = in6_pcbconnect(inp, nam);
		} else if (sin->sin_family == AF_INET)
#endif /* INET6 */
		{
			if ((sin->sin_addr.s_addr == INADDR_ANY) ||
			    IN_MULTICAST(sin->sin_addr.s_addr) ||
			    in_broadcast(sin->sin_addr, NULL)) {
				error = EINVAL;
				break;
			}

			if (inp->inp_lport == 0) {
				error = in_pcbbind(inp, NULL);
				if (error)
					break;
			}
			error = in_pcbconnect(inp, nam);
		}

		if (error)
			break;

		tp->t_template = tcp_template(tp);
		if (tp->t_template == 0) {
			in_pcbdisconnect(inp);
			error = ENOBUFS;
			break;
		}

		so->so_state |= SS_CONNECTOUT;
		/* Compute window scaling to request.  */
		tcp_rscale(tp, so->so_rcv.sb_hiwat);

		soisconnecting(so);
		tcpstat.tcps_connattempt++;
		tp->t_state = TCPS_SYN_SENT;
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcptv_keep_init);
#ifdef TCP_COMPAT_42
		tp->iss = tcp_iss;
		tcp_iss += TCP_ISSINCR/2;
#else  /* TCP_COMPAT_42 */
		tp->iss = tcp_rndiss_next();
#endif /* !TCP_COMPAT_42 */
		tcp_sendseqinit(tp);
#if defined(TCP_SACK)
		tp->snd_last = tp->snd_una;
#endif
#if defined(TCP_SACK) && defined(TCP_FACK)
		tp->snd_fack = tp->snd_una;
		tp->retran_data = 0;
		tp->snd_awnd = 0;
#endif
		error = tcp_output(tp);
		break;

	/*
	 * Create a TCP connection between two sockets.
	 */
	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Initiate disconnect from peer.
	 * If connection never passed embryonic stage, just drop;
	 * else if don't need to let data drain, then can just drop anyways,
	 * else have to begin TCP shutdown process: mark socket disconnecting,
	 * drain unread data, state switch to reflect user close, and
	 * send segment (e.g. FIN) to peer.  Socket will be really disconnected
	 * when peer sends FIN and acks ours.
	 *
	 * SHOULD IMPLEMENT LATER PRU_CONNECT VIA REALLOC TCPCB.
	 */
	case PRU_DISCONNECT:
		tp = tcp_disconnect(tp);
		break;

	/*
	 * Accept a connection.  Essentially all the work is
	 * done at higher levels; just return the address
	 * of the peer, storing through addr.
	 */
	case PRU_ACCEPT:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			in6_setpeeraddr(inp, nam);
		else
#endif
			in_setpeeraddr(inp, nam);
		break;

	/*
	 * Mark the connection as being incapable of further output.
	 */
	case PRU_SHUTDOWN:
		if (so->so_state & SS_CANTSENDMORE)
			break;
		socantsendmore(so);
		tp = tcp_usrclosed(tp);
		if (tp)
			error = tcp_output(tp);
		break;

	/*
	 * After a receive, possibly send window update to peer.
	 */
	case PRU_RCVD:
		/*
		 * soreceive() calls this function when a user receives
		 * ancillary data on a listening socket. We don't call
		 * tcp_output in such a case, since there is no header
		 * template for a listening socket and hence the kernel
		 * will panic.
		 */
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONNECTING)) != 0)
			(void) tcp_output(tp);
		break;

	/*
	 * Do a send by putting data in output queue and updating urgent
	 * marker if URG set.  Possibly send more data.
	 */
	case PRU_SEND:
		sbappendstream(&so->so_snd, m);
		error = tcp_output(tp);
		break;

	/*
	 * Abort the TCP.
	 */
	case PRU_ABORT:
		tp = tcp_drop(tp, ECONNABORTED);
		break;

	case PRU_SENSE:
		((struct stat *) m)->st_blksize = so->so_snd.sb_hiwat;
		splx(s);
		return (0);

	case PRU_RCVOOB:
		if ((so->so_oobmark == 0 &&
		    (so->so_state & SS_RCVATMARK) == 0) ||
		    so->so_options & SO_OOBINLINE ||
		    tp->t_oobflags & TCPOOB_HADDATA) {
			error = EINVAL;
			break;
		}
		if ((tp->t_oobflags & TCPOOB_HAVEDATA) == 0) {
			error = EWOULDBLOCK;
			break;
		}
		m->m_len = 1;
		*mtod(m, caddr_t) = tp->t_iobc;
		if (((long)nam & MSG_PEEK) == 0)
			tp->t_oobflags ^= (TCPOOB_HAVEDATA | TCPOOB_HADDATA);
		break;

	case PRU_SENDOOB:
		if (sbspace(&so->so_snd) < -512) {
			m_freem(m);
			error = ENOBUFS;
			break;
		}
		/*
		 * According to RFC961 (Assigned Protocols),
		 * the urgent pointer points to the last octet
		 * of urgent data.  We continue, however,
		 * to consider it to indicate the first octet
		 * of data past the urgent section.
		 * Otherwise, snd_up should be one lower.
		 */
		sbappendstream(&so->so_snd, m);
		tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
		tp->t_force = 1;
		error = tcp_output(tp);
		tp->t_force = 0;
		break;

	case PRU_SOCKADDR:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			in6_setsockaddr(inp, nam);
		else
#endif
			in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			in6_setpeeraddr(inp, nam);
		else
#endif
			in_setpeeraddr(inp, nam);
		break;

	default:
		panic("tcp_usrreq");
	}
	if (tp && (so->so_options & SO_DEBUG))
		tcp_trace(TA_USER, ostate, tp, (caddr_t)0, req, 0);
	splx(s);
	return (error);
}

int
tcp_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
{
	int error = 0, s;
	struct inpcb *inp;
	struct tcpcb *tp;
	struct mbuf *m;
	int i;

	s = splsoftnet();
	inp = sotoinpcb(so);
	if (inp == NULL) {
		splx(s);
		if (op == PRCO_SETOPT && *mp)
			(void) m_free(*mp);
		return (ECONNRESET);
	}
#ifdef INET6
	tp = intotcpcb(inp);
#endif /* INET6 */
	if (level != IPPROTO_TCP) {
		switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, level, optname, mp);
			break;
#endif /* INET6 */
		case PF_INET:
			error = ip_ctloutput(op, so, level, optname, mp);
			break;
		default:
			error = EAFNOSUPPORT;	/*?*/
			break;
		}
		splx(s);
		return (error);
	}
#ifndef INET6
	tp = intotcpcb(inp);
#endif /* !INET6 */

	switch (op) {

	case PRCO_SETOPT:
		m = *mp;
		switch (optname) {

		case TCP_NODELAY:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_MAXSEG:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				break;
			}

			i = *mtod(m, int *);
			if (i > 0 && i <= tp->t_maxseg)
				tp->t_maxseg = i;
			else
				error = EINVAL;
			break;

#ifdef TCP_SACK
		case TCP_SACK_ENABLE:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				break;
			}

			if (TCPS_HAVEESTABLISHED(tp->t_state)) {
				error = EPERM;
				break;
			}

			if (tp->t_flags & TF_SIGNATURE) {
				error = EPERM;
				break;
			}

			if (*mtod(m, int *))
				tp->sack_enable = 1;
			else
				tp->sack_enable = 0;
			break;
#endif
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			if (m == NULL || m->m_len < sizeof (int)) {
				error = EINVAL;
				break;
			}

			if (TCPS_HAVEESTABLISHED(tp->t_state)) {
				error = EPERM;
				break;
			}

			if (*mtod(m, int *)) {
				tp->t_flags |= TF_SIGNATURE;
#ifdef TCP_SACK
				tp->sack_enable = 0;
#endif /* TCP_SACK */
			} else
				tp->t_flags &= ~TF_SIGNATURE;
			break;
#endif /* TCP_SIGNATURE */
		default:
			error = ENOPROTOOPT;
			break;
		}
		if (m)
			(void) m_free(m);
		break;

	case PRCO_GETOPT:
		*mp = m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sizeof(int);

		switch (optname) {
		case TCP_NODELAY:
			*mtod(m, int *) = tp->t_flags & TF_NODELAY;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_maxseg;
			break;
#ifdef TCP_SACK
		case TCP_SACK_ENABLE:
			*mtod(m, int *) = tp->sack_enable;
			break;
#endif
#ifdef TCP_SIGNATURE
		case TCP_MD5SIG:
			*mtod(m, int *) = tp->t_flags & TF_SIGNATURE;
			break;
#endif
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}
	splx(s);
	return (error);
}

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * bufer space, and entering LISTEN state if to accept connections.
 */
int
tcp_attach(so)
	struct socket *so;
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return (error);
	}
	error = in_pcballoc(so, &tcbtable);
	if (error)
		return (error);
	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp);
	if (tp == NULL) {
		int nofd = so->so_state & SS_NOFDREF;	/* XXX */

		so->so_state &= ~SS_NOFDREF;	/* don't free the socket yet */
		in_pcbdetach(inp);
		so->so_state |= nofd;
		return (ENOBUFS);
	}
	tp->t_state = TCPS_CLOSED;
#ifdef INET6
	/* we disallow IPv4 mapped address completely. */
	if (inp->inp_flags & INP_IPV6)
		tp->pf = PF_INET6;
	else
		tp->pf = PF_INET;
#else
	tp->pf = PF_INET;
#endif
	return (0);
}

/*
 * Initiate (or continue) disconnect.
 * If embryonic state, just send reset (once).
 * If in ``let data drain'' option and linger null, just drop.
 * Otherwise (hard), mark socket disconnecting and drop
 * current input data; switch states based on user close, and
 * send segment to peer (with FIN).
 */
struct tcpcb *
tcp_disconnect(tp)
	struct tcpcb *tp;
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(&so->so_rcv);
		tp = tcp_usrclosed(tp);
		if (tp)
			(void) tcp_output(tp);
	}
	return (tp);
}

/*
 * User issued close, and wish to trail through shutdown states:
 * if never received SYN, just forget it.  If got a SYN from peer,
 * but haven't sent FIN, then go to FIN_WAIT_1 state to send peer a FIN.
 * If already got a FIN from peer, then almost done; go to LAST_ACK
 * state.  In all other cases, have already sent FIN to peer (e.g.
 * after PRU_SHUTDOWN), and just have to play tedious game waiting
 * for peer to send FIN or not respond to keep-alives, etc.
 * We can let the user exit from the close as soon as the FIN is acked.
 */
struct tcpcb *
tcp_usrclosed(tp)
	struct tcpcb *tp;
{

	switch (tp->t_state) {

	case TCPS_CLOSED:
	case TCPS_LISTEN:
	case TCPS_SYN_SENT:
		tp->t_state = TCPS_CLOSED;
		tp = tcp_close(tp);
		break;

	case TCPS_SYN_RECEIVED:
	case TCPS_ESTABLISHED:
		tp->t_state = TCPS_FIN_WAIT_1;
		break;

	case TCPS_CLOSE_WAIT:
		tp->t_state = TCPS_LAST_ACK;
		break;
	}
	if (tp && tp->t_state >= TCPS_FIN_WAIT_2) {
		soisdisconnected(tp->t_inpcb->inp_socket);
		/*
		 * If we are in FIN_WAIT_2, we arrived here because the
		 * application did a shutdown of the send side.  Like the
		 * case of a transition from FIN_WAIT_1 to FIN_WAIT_2 after
		 * a full close, we start a timer to make sure sockets are
		 * not left in FIN_WAIT_2 forever.
		 */
		if (tp->t_state == TCPS_FIN_WAIT_2)
			TCP_TIMER_ARM(tp, TCPT_2MSL, tcp_maxidle);
	}
	return (tp);
}

/*
 * Look up a socket for ident or tcpdrop, ...
 */
int
tcp_ident(oldp, oldlenp, newp, newlen, dodrop)
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	int dodrop;
{
	int error = 0, s;
	struct tcp_ident_mapping tir;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct sockaddr_in *fin, *lin;
#ifdef INET6
	struct sockaddr_in6 *fin6, *lin6;
	struct in6_addr f6, l6;
#endif
	if (oldp == NULL)
		return (EINVAL);
	if (*oldlenp < sizeof(tir))
		return (ENOMEM);
	if (dodrop) {
		if (newp == NULL)
			return (EPERM);
		if (newlen < sizeof(tir))
			return (ENOMEM);
		if ((error = copyin(newp, &tir, sizeof (tir))) != 0 )
			return (error);
	} else {
		if (newp != NULL || newlen != 0)
			return (EINVAL);
		if ((error = copyin(oldp, &tir, sizeof (tir))) != 0 )
			return (error);
	}
	switch (tir.faddr.ss_family) {
#ifdef INET6
	case AF_INET6:
		fin6 = (struct sockaddr_in6 *)&tir.faddr;
		error = in6_embedscope(&f6, fin6, NULL, NULL);
		if (error)
			return EINVAL;	/*?*/
		lin6 = (struct sockaddr_in6 *)&tir.laddr;
		error = in6_embedscope(&l6, lin6, NULL, NULL);
		if (error)
			return EINVAL;	/*?*/
		break;
#endif
	case AF_INET:
	  	fin = (struct sockaddr_in *)&tir.faddr;
		lin = (struct sockaddr_in *)&tir.laddr;
		break;
	default:
		return (EINVAL);
	}

	s = splsoftnet();
	switch (tir.faddr.ss_family) {
	case AF_INET6:
#ifdef INET6
		inp = in6_pcbhashlookup(&tcbtable, &f6,
		    fin6->sin6_port, &l6, lin6->sin6_port);
		break;
#endif
	case AF_INET:
		inp = in_pcbhashlookup(&tcbtable,  fin->sin_addr,
		    fin->sin_port, lin->sin_addr, lin->sin_port);
		break;
	}

	if (dodrop) {
		if (inp && (tp = intotcpcb(inp)) &&
		    ((inp->inp_socket->so_options & SO_ACCEPTCONN) == 0))
			tp = tcp_drop(tp, ECONNABORTED);
		else
			error = ESRCH;
		splx(s);
		return (error);
	}

	if (inp == NULL) {
		++tcpstat.tcps_pcbhashmiss;
		switch (tir.faddr.ss_family) {
#ifdef INET6
		case AF_INET6:
			inp = in6_pcblookup_listen(&tcbtable,
			    &l6, lin6->sin6_port, 0);
			break;
#endif
		case AF_INET:
			inp = in_pcblookup_listen(&tcbtable, 
			    lin->sin_addr, lin->sin_port, 0);
			break;
		}
	}

	if (inp != NULL && (inp->inp_socket->so_state & SS_CONNECTOUT)) {
		tir.ruid = inp->inp_socket->so_ruid;
		tir.euid = inp->inp_socket->so_euid;
	} else {
		tir.ruid = -1;
		tir.euid = -1;
	}
	splx(s);

	*oldlenp = sizeof (tir);
	error = copyout((void *)&tir, oldp, sizeof (tir));
	return (error);
}

/*
 * Sysctl for tcp variables.
 */
int
tcp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	int error, nval;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
#ifdef TCP_SACK
	case TCPCTL_SACK:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &tcp_do_sack));
#endif
	case TCPCTL_SLOWHZ:
		return (sysctl_rdint(oldp, oldlenp, newp, PR_SLOWHZ));

	case TCPCTL_BADDYNAMIC:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    baddynamicports.tcp, sizeof(baddynamicports.tcp)));

	case TCPCTL_IDENT:
		return (tcp_ident(oldp, oldlenp, newp, newlen, 0));

	case TCPCTL_DROP:
		return (tcp_ident(oldp, oldlenp, newp, newlen, 1));

#ifdef TCP_ECN
	case TCPCTL_ECN:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		   &tcp_do_ecn));
#endif
	case TCPCTL_REASS_LIMIT:
		nval = tcp_reass_limit;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &nval);
		if (error)
			return (error);
		if (nval != tcp_reass_limit) {
			error = pool_sethardlimit(&tcpqe_pool, nval, NULL, 0);
			if (error)
				return (error);
			tcp_reass_limit = nval;
		}
		return (0);
	default:
		if (name[0] < TCPCTL_MAXID)
			return (sysctl_int_arr(tcpctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
