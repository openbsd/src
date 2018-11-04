/*	$OpenBSD: tcp_usrreq.c,v 1.170 2018/11/04 19:36:25 bluhm Exp $	*/
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
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
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>

#ifdef INET6
#include <netinet6/in6_var.h>
#endif

#ifndef TCP_SENDSPACE
#define	TCP_SENDSPACE	1024*16
#endif
u_int	tcp_sendspace = TCP_SENDSPACE;
#ifndef TCP_RECVSPACE
#define	TCP_RECVSPACE	1024*16
#endif
u_int	tcp_recvspace = TCP_RECVSPACE;
u_int	tcp_autorcvbuf_inc = 16 * 1024;

int *tcpctl_vars[TCPCTL_MAXID] = TCPCTL_VARS;

struct	inpcbtable tcbtable;

int tcp_ident(void *, size_t *, void *, size_t, int);

/*
 * Process a TCP user request for TCP tb.  If this is a send request
 * then m is the mbuf chain of send data.  If this is a timer expiration
 * (called from the software clock routine), then timertype tells which timer.
 */
/*ARGSUSED*/
int
tcp_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp;
	struct tcpcb *otp = NULL, *tp = NULL;
	int error = 0;
	short ostate;

	if (req == PRU_CONTROL) {
#ifdef INET6
		if (sotopf(so) == PF_INET6)
			return in6_control(so, (u_long)m, (caddr_t)nam,
			    (struct ifnet *)control);
		else
#endif /* INET6 */
			return (in_control(so, (u_long)m, (caddr_t)nam,
			    (struct ifnet *)control));
	}

	soassertlocked(so);

	if (control && control->m_len) {
		m_freem(control);
		m_freem(m);
		return (EINVAL);
	}

	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidiary (struct tcpcb).
	 */
	if (inp == NULL) {
		error = so->so_error;
		if (error == 0)
			error = EINVAL;
		/*
		 * The following corrects an mbuf leak under rare
		 * circumstances
		 */
		if (req == PRU_SEND || req == PRU_SENDOOB)
			m_freem(m);
		return (error);
	}
	tp = intotcpcb(inp);
	/* tp might get 0 when using socket splicing */
	if (tp == NULL)
		return (0);
	if (so->so_options & SO_DEBUG) {
		otp = tp;
		ostate = tp->t_state;
	}

	switch (req) {

	/*
	 * Give the socket an address.
	 */
	case PRU_BIND:
		error = in_pcbbind(inp, nam, p);
		break;

	/*
	 * Prepare to accept connections.
	 */
	case PRU_LISTEN:
		if (inp->inp_lport == 0)
			error = in_pcbbind(inp, NULL, p);
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
#ifdef INET6
		if (inp->inp_flags & INP_IPV6) {
			struct sockaddr_in6 *sin6;

			if ((error = in6_nam2sin6(nam, &sin6)))
				break;
			if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) ||
			    IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
				error = EINVAL;
				break;
			}
			error = in6_pcbconnect(inp, nam);
		} else
#endif /* INET6 */
		{
			struct sockaddr_in *sin;

			if ((error = in_nam2sin(nam, &sin)))
				break;
			if ((sin->sin_addr.s_addr == INADDR_ANY) ||
			    (sin->sin_addr.s_addr == INADDR_BROADCAST) ||
			    IN_MULTICAST(sin->sin_addr.s_addr) ||
			    in_broadcast(sin->sin_addr, inp->inp_rtableid)) {
				error = EINVAL;
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
		tcp_rscale(tp, sb_max);

		soisconnecting(so);
		tcpstat_inc(tcps_connattempt);
		tp->t_state = TCPS_SYN_SENT;
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcptv_keep_init);
		tcp_set_iss_tsm(tp);
		tcp_sendseqinit(tp);
		tp->snd_last = tp->snd_una;
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
		sbappendstream(so, &so->so_snd, m);
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
		if (sbspace(so, &so->so_snd) < -512) {
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
		sbappendstream(so, &so->so_snd, m);
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
	if (otp)
		tcp_trace(TA_USER, ostate, tp, otp, NULL, req, 0);
	return (error);
}

int
tcp_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf *m)
{
	int error = 0;
	struct inpcb *inp;
	struct tcpcb *tp;
	int i;

	inp = sotoinpcb(so);
	if (inp == NULL)
		return (ECONNRESET);
	if (level != IPPROTO_TCP) {
		switch (so->so_proto->pr_domain->dom_family) {
#ifdef INET6
		case PF_INET6:
			error = ip6_ctloutput(op, so, level, optname, m);
			break;
#endif /* INET6 */
		case PF_INET:
			error = ip_ctloutput(op, so, level, optname, m);
			break;
		default:
			error = EAFNOSUPPORT;	/*?*/
			break;
		}
		return (error);
	}
	tp = intotcpcb(inp);

	switch (op) {

	case PRCO_SETOPT:
		switch (optname) {

		case TCP_NODELAY:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NODELAY;
			else
				tp->t_flags &= ~TF_NODELAY;
			break;

		case TCP_NOPUSH:
			if (m == NULL || m->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(m, int *))
				tp->t_flags |= TF_NOPUSH;
			else if (tp->t_flags & TF_NOPUSH) {
				tp->t_flags &= ~TF_NOPUSH;
				if (TCPS_HAVEESTABLISHED(tp->t_state))
					error = tcp_output(tp);
			}
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
				tp->sack_enable = 0;
			} else
				tp->t_flags &= ~TF_SIGNATURE;
			break;
#endif /* TCP_SIGNATURE */
		default:
			error = ENOPROTOOPT;
			break;
		}
		break;

	case PRCO_GETOPT:
		m->m_len = sizeof(int);

		switch (optname) {
		case TCP_NODELAY:
			*mtod(m, int *) = tp->t_flags & TF_NODELAY;
			break;
		case TCP_NOPUSH:
			*mtod(m, int *) = tp->t_flags & TF_NOPUSH;
			break;
		case TCP_MAXSEG:
			*mtod(m, int *) = tp->t_maxseg;
			break;
		case TCP_SACK_ENABLE:
			*mtod(m, int *) = tp->sack_enable;
			break;
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
	return (error);
}

/*
 * Attach TCP protocol to socket, allocating
 * internet protocol control block, tcp control block,
 * buffer space, and entering LISTEN state to accept connections.
 */
int
tcp_attach(struct socket *so, int proto)
{
	struct tcpcb *tp;
	struct inpcb *inp;
	int error;

	if (so->so_pcb)
		return EISCONN;
	if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0 ||
	    sbcheckreserve(so->so_snd.sb_wat, tcp_sendspace) ||
	    sbcheckreserve(so->so_rcv.sb_wat, tcp_recvspace)) {
		error = soreserve(so, tcp_sendspace, tcp_recvspace);
		if (error)
			return (error);
	}

	NET_ASSERT_LOCKED();
	error = in_pcballoc(so, &tcbtable);
	if (error)
		return (error);
	inp = sotoinpcb(so);
	tp = tcp_newtcpcb(inp);
	if (tp == NULL) {
		unsigned int nofd = so->so_state & SS_NOFDREF;	/* XXX */

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
	if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		so->so_linger = TCP_LINGERTIME;

	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_USER, TCPS_CLOSED, tp, tp, NULL, PRU_ATTACH, 0);
	return (0);
}

int
tcp_detach(struct socket *so)
{
	struct inpcb *inp;
	struct tcpcb *otp = NULL, *tp = NULL;
	int error = 0;
	short ostate;

	soassertlocked(so);

	inp = sotoinpcb(so);
	/*
	 * When a TCP is attached to a socket, then there will be
	 * a (struct inpcb) pointed at by the socket, and this
	 * structure will point at a subsidiary (struct tcpcb).
	 */
	if (inp == NULL) {
		error = so->so_error;
		if (error == 0)
			error = EINVAL;
		return (error);
	}
	tp = intotcpcb(inp);
	/* tp might get 0 when using socket splicing */
	if (tp == NULL)
		return (0);
	if (so->so_options & SO_DEBUG) {
		otp = tp;
		ostate = tp->t_state;
	}

	/*
	 * Detach the TCP protocol from the socket.
	 * If the protocol state is non-embryonic, then can't
	 * do this directly: have to initiate a PRU_DISCONNECT,
	 * which may finish later; embryonic TCB's can just
	 * be discarded here.
	 */
	tp = tcp_disconnect(tp);

	if (otp)
		tcp_trace(TA_USER, ostate, tp, otp, NULL, PRU_DETACH, 0);
	return (error);
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
tcp_disconnect(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;

	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		tp = tcp_close(tp);
	else if ((so->so_options & SO_LINGER) && so->so_linger == 0)
		tp = tcp_drop(tp, 0);
	else {
		soisdisconnecting(so);
		sbflush(so, &so->so_rcv);
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
tcp_usrclosed(struct tcpcb *tp)
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
tcp_ident(void *oldp, size_t *oldlenp, void *newp, size_t newlen, int dodrop)
{
	int error = 0;
	struct tcp_ident_mapping tir;
	struct inpcb *inp;
	struct tcpcb *tp = NULL;
	struct sockaddr_in *fin, *lin;
#ifdef INET6
	struct sockaddr_in6 *fin6, *lin6;
	struct in6_addr f6, l6;
#endif

	NET_ASSERT_LOCKED();

	if (dodrop) {
		if (oldp != NULL || *oldlenp != 0)
			return (EINVAL);
		if (newp == NULL)
			return (EPERM);
		if (newlen < sizeof(tir))
			return (ENOMEM);
		if ((error = copyin(newp, &tir, sizeof (tir))) != 0 )
			return (error);
	} else {
		if (oldp == NULL)
			return (EINVAL);
		if (*oldlenp < sizeof(tir))
			return (ENOMEM);
		if (newp != NULL || newlen != 0)
			return (EINVAL);
		if ((error = copyin(oldp, &tir, sizeof (tir))) != 0 )
			return (error);
	}
	switch (tir.faddr.ss_family) {
#ifdef INET6
	case AF_INET6:
		fin6 = (struct sockaddr_in6 *)&tir.faddr;
		error = in6_embedscope(&f6, fin6, NULL);
		if (error)
			return EINVAL;	/*?*/
		lin6 = (struct sockaddr_in6 *)&tir.laddr;
		error = in6_embedscope(&l6, lin6, NULL);
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

	switch (tir.faddr.ss_family) {
#ifdef INET6
	case AF_INET6:
		inp = in6_pcbhashlookup(&tcbtable, &f6,
		    fin6->sin6_port, &l6, lin6->sin6_port, tir.rdomain);
		break;
#endif
	case AF_INET:
		inp = in_pcbhashlookup(&tcbtable, fin->sin_addr,
		    fin->sin_port, lin->sin_addr, lin->sin_port, tir.rdomain);
		break;
	default:
		unhandled_af(tir.faddr.ss_family);
	}

	if (dodrop) {
		if (inp && (tp = intotcpcb(inp)) &&
		    ((inp->inp_socket->so_options & SO_ACCEPTCONN) == 0))
			tp = tcp_drop(tp, ECONNABORTED);
		else
			error = ESRCH;
		return (error);
	}

	if (inp == NULL) {
		tcpstat_inc(tcps_pcbhashmiss);
		switch (tir.faddr.ss_family) {
#ifdef INET6
		case AF_INET6:
			inp = in6_pcblookup_listen(&tcbtable,
			    &l6, lin6->sin6_port, NULL, tir.rdomain);
			break;
#endif
		case AF_INET:
			inp = in_pcblookup_listen(&tcbtable,
			    lin->sin_addr, lin->sin_port, NULL, tir.rdomain);
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

	*oldlenp = sizeof (tir);
	error = copyout((void *)&tir, oldp, sizeof (tir));
	return (error);
}

int
tcp_sysctl_tcpstat(void *oldp, size_t *oldlenp, void *newp)
{
	uint64_t counters[tcps_ncounters];
	struct tcpstat tcpstat;
	struct syn_cache_set *set;
	int i = 0;

#define ASSIGN(field)	do { tcpstat.field = counters[i++]; } while (0)

	memset(&tcpstat, 0, sizeof tcpstat);
	counters_read(tcpcounters, counters, nitems(counters));
	ASSIGN(tcps_connattempt);
	ASSIGN(tcps_accepts);
	ASSIGN(tcps_connects);
	ASSIGN(tcps_drops);
	ASSIGN(tcps_conndrops);
	ASSIGN(tcps_closed);
	ASSIGN(tcps_segstimed);
	ASSIGN(tcps_rttupdated);
	ASSIGN(tcps_delack);
	ASSIGN(tcps_timeoutdrop);
	ASSIGN(tcps_rexmttimeo);
	ASSIGN(tcps_persisttimeo);
	ASSIGN(tcps_persistdrop);
	ASSIGN(tcps_keeptimeo);
	ASSIGN(tcps_keepprobe);
	ASSIGN(tcps_keepdrops);
	ASSIGN(tcps_sndtotal);
	ASSIGN(tcps_sndpack);
	ASSIGN(tcps_sndbyte);
	ASSIGN(tcps_sndrexmitpack);
	ASSIGN(tcps_sndrexmitbyte);
	ASSIGN(tcps_sndrexmitfast);
	ASSIGN(tcps_sndacks);
	ASSIGN(tcps_sndprobe);
	ASSIGN(tcps_sndurg);
	ASSIGN(tcps_sndwinup);
	ASSIGN(tcps_sndctrl);
	ASSIGN(tcps_rcvtotal);
	ASSIGN(tcps_rcvpack);
	ASSIGN(tcps_rcvbyte);
	ASSIGN(tcps_rcvbadsum);
	ASSIGN(tcps_rcvbadoff);
	ASSIGN(tcps_rcvmemdrop);
	ASSIGN(tcps_rcvnosec);
	ASSIGN(tcps_rcvshort);
	ASSIGN(tcps_rcvduppack);
	ASSIGN(tcps_rcvdupbyte);
	ASSIGN(tcps_rcvpartduppack);
	ASSIGN(tcps_rcvpartdupbyte);
	ASSIGN(tcps_rcvoopack);
	ASSIGN(tcps_rcvoobyte);
	ASSIGN(tcps_rcvpackafterwin);
	ASSIGN(tcps_rcvbyteafterwin);
	ASSIGN(tcps_rcvafterclose);
	ASSIGN(tcps_rcvwinprobe);
	ASSIGN(tcps_rcvdupack);
	ASSIGN(tcps_rcvacktoomuch);
	ASSIGN(tcps_rcvacktooold);
	ASSIGN(tcps_rcvackpack);
	ASSIGN(tcps_rcvackbyte);
	ASSIGN(tcps_rcvwinupd);
	ASSIGN(tcps_pawsdrop);
	ASSIGN(tcps_predack);
	ASSIGN(tcps_preddat);
	ASSIGN(tcps_pcbhashmiss);
	ASSIGN(tcps_noport);
	ASSIGN(tcps_badsyn);
	ASSIGN(tcps_dropsyn);
	ASSIGN(tcps_rcvbadsig);
	ASSIGN(tcps_rcvgoodsig);
	ASSIGN(tcps_inswcsum);
	ASSIGN(tcps_outswcsum);
	ASSIGN(tcps_ecn_accepts);
	ASSIGN(tcps_ecn_rcvece);
	ASSIGN(tcps_ecn_rcvcwr);
	ASSIGN(tcps_ecn_rcvce);
	ASSIGN(tcps_ecn_sndect);
	ASSIGN(tcps_ecn_sndece);
	ASSIGN(tcps_ecn_sndcwr);
	ASSIGN(tcps_cwr_ecn);
	ASSIGN(tcps_cwr_frecovery);
	ASSIGN(tcps_cwr_timeout);
	ASSIGN(tcps_sc_added);
	ASSIGN(tcps_sc_completed);
	ASSIGN(tcps_sc_timed_out);
	ASSIGN(tcps_sc_overflowed);
	ASSIGN(tcps_sc_reset);
	ASSIGN(tcps_sc_unreach);
	ASSIGN(tcps_sc_bucketoverflow);
	ASSIGN(tcps_sc_aborted);
	ASSIGN(tcps_sc_dupesyn);
	ASSIGN(tcps_sc_dropped);
	ASSIGN(tcps_sc_collisions);
	ASSIGN(tcps_sc_retransmitted);
	ASSIGN(tcps_sc_seedrandom);
	ASSIGN(tcps_sc_hash_size);
	ASSIGN(tcps_sc_entry_count);
	ASSIGN(tcps_sc_entry_limit);
	ASSIGN(tcps_sc_bucket_maxlen);
	ASSIGN(tcps_sc_bucket_limit);
	ASSIGN(tcps_sc_uses_left);
	ASSIGN(tcps_conndrained);
	ASSIGN(tcps_sack_recovery_episode);
	ASSIGN(tcps_sack_rexmits);
	ASSIGN(tcps_sack_rexmit_bytes);
	ASSIGN(tcps_sack_rcv_opts);
	ASSIGN(tcps_sack_snd_opts);

#undef ASSIGN

	set = &tcp_syn_cache[tcp_syn_cache_active];
	tcpstat.tcps_sc_hash_size = set->scs_size;
	tcpstat.tcps_sc_entry_count = set->scs_count;
	tcpstat.tcps_sc_entry_limit = tcp_syn_cache_limit;
	tcpstat.tcps_sc_bucket_maxlen = 0;
	for (i = 0; i < set->scs_size; i++) {
		if (tcpstat.tcps_sc_bucket_maxlen <
		    set->scs_buckethead[i].sch_length)
			tcpstat.tcps_sc_bucket_maxlen =
				set->scs_buckethead[i].sch_length;
	}
	tcpstat.tcps_sc_bucket_limit = tcp_syn_bucket_limit;
	tcpstat.tcps_sc_uses_left = set->scs_use;

	return (sysctl_rdstruct(oldp, oldlenp, newp,
	    &tcpstat, sizeof(tcpstat)));
}

/*
 * Sysctl for tcp variables.
 */
int
tcp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error, nval;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case TCPCTL_SACK:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &tcp_do_sack);
		NET_UNLOCK();
		return (error);

	case TCPCTL_SLOWHZ:
		return (sysctl_rdint(oldp, oldlenp, newp, PR_SLOWHZ));

	case TCPCTL_BADDYNAMIC:
		NET_LOCK();
		error = sysctl_struct(oldp, oldlenp, newp, newlen,
		    baddynamicports.tcp, sizeof(baddynamicports.tcp));
		NET_UNLOCK();
		return (error);

	case TCPCTL_ROOTONLY:
		if (newp && securelevel > 0)
			return (EPERM);
		NET_LOCK();
		error = sysctl_struct(oldp, oldlenp, newp, newlen,
		    rootonlyports.tcp, sizeof(rootonlyports.tcp));
		NET_UNLOCK();
		return (error);

	case TCPCTL_IDENT:
		NET_LOCK();
		error = tcp_ident(oldp, oldlenp, newp, newlen, 0);
		NET_UNLOCK();
		return (error);

	case TCPCTL_DROP:
		NET_LOCK();
		error = tcp_ident(oldp, oldlenp, newp, newlen, 1);
		NET_UNLOCK();
		return (error);

	case TCPCTL_ALWAYS_KEEPALIVE:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &tcp_always_keepalive);
		NET_UNLOCK();
		return (error);

#ifdef TCP_ECN
	case TCPCTL_ECN:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		   &tcp_do_ecn);
		NET_UNLOCK();
		return (error);
#endif
	case TCPCTL_REASS_LIMIT:
		NET_LOCK();
		nval = tcp_reass_limit;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &nval);
		if (!error && nval != tcp_reass_limit) {
			error = pool_sethardlimit(&tcpqe_pool, nval, NULL, 0);
			if (!error)
				tcp_reass_limit = nval;
		}
		NET_UNLOCK();
		return (error);

	case TCPCTL_SACKHOLE_LIMIT:
		NET_LOCK();
		nval = tcp_sackhole_limit;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &nval);
		if (!error && nval != tcp_sackhole_limit) {
			error = pool_sethardlimit(&sackhl_pool, nval, NULL, 0);
			if (!error)
				tcp_sackhole_limit = nval;
		}
		NET_UNLOCK();
		return (error);

	case TCPCTL_STATS:
		return (tcp_sysctl_tcpstat(oldp, oldlenp, newp));

	case TCPCTL_SYN_USE_LIMIT:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &tcp_syn_use_limit);
		if (!error && newp != NULL) {
			/*
			 * Global tcp_syn_use_limit is used when reseeding a
			 * new cache.  Also update the value in active cache.
			 */
			if (tcp_syn_cache[0].scs_use > tcp_syn_use_limit)
				tcp_syn_cache[0].scs_use = tcp_syn_use_limit;
			if (tcp_syn_cache[1].scs_use > tcp_syn_use_limit)
				tcp_syn_cache[1].scs_use = tcp_syn_use_limit;
		}
		NET_UNLOCK();
		return (error);

	case TCPCTL_SYN_HASH_SIZE:
		NET_LOCK();
		nval = tcp_syn_hash_size;
		error = sysctl_int(oldp, oldlenp, newp, newlen, &nval);
		if (!error && nval != tcp_syn_hash_size) {
			if (nval < 1 || nval > 100000) {
				error = EINVAL;
			} else {
				/*
				 * If global hash size has been changed,
				 * switch sets as soon as possible.  Then
				 * the actual hash array will be reallocated.
				 */
				if (tcp_syn_cache[0].scs_size != nval)
					tcp_syn_cache[0].scs_use = 0;
				if (tcp_syn_cache[1].scs_size != nval)
					tcp_syn_cache[1].scs_use = 0;
				tcp_syn_hash_size = nval;
			}
		}
		NET_UNLOCK();
		return (error);

	default:
		if (name[0] < TCPCTL_MAXID) {
			NET_LOCK();
			error = sysctl_int_arr(tcpctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);
			NET_UNLOCK();
			return (error);
		}
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}

/*
 * Scale the send buffer so that inflight data is not accounted against
 * the limit. The buffer will scale with the congestion window, if the
 * the receiver stops acking data the window will shrink and therefor
 * the buffer size will shrink as well.
 * In low memory situation try to shrink the buffer to the initial size
 * disabling the send buffer scaling as long as the situation persists.
 */
void
tcp_update_sndspace(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	u_long nmax = so->so_snd.sb_hiwat;

	if (sbchecklowmem()) {
		/* low on memory try to get rid of some */
		if (tcp_sendspace < nmax)
			nmax = tcp_sendspace;
	} else if (so->so_snd.sb_wat != tcp_sendspace)
		/* user requested buffer size, auto-scaling disabled */
		nmax = so->so_snd.sb_wat;
	else
		/* automatic buffer scaling */
		nmax = MIN(sb_max, so->so_snd.sb_wat + tp->snd_max -
		    tp->snd_una);

	/* a writable socket must be preserved because of poll(2) semantics */
	if (sbspace(so, &so->so_snd) >= so->so_snd.sb_lowat) {
		if (nmax < so->so_snd.sb_cc + so->so_snd.sb_lowat)
			nmax = so->so_snd.sb_cc + so->so_snd.sb_lowat;
		/* keep in sync with sbreserve() calculation */
		if (nmax * 8 < so->so_snd.sb_mbcnt + so->so_snd.sb_lowat)
			nmax = (so->so_snd.sb_mbcnt+so->so_snd.sb_lowat+7) / 8;
	}

	/* round to MSS boundary */
	nmax = roundup(nmax, tp->t_maxseg);

	if (nmax != so->so_snd.sb_hiwat)
		sbreserve(so, &so->so_snd, nmax);
}

/*
 * Scale the recv buffer by looking at how much data was transferred in
 * on approximated RTT. If more than a big part of the recv buffer was
 * transferred during that time we increase the buffer by a constant.
 * In low memory situation try to shrink the buffer to the initial size.
 */
void
tcp_update_rcvspace(struct tcpcb *tp)
{
	struct socket *so = tp->t_inpcb->inp_socket;
	u_long nmax = so->so_rcv.sb_hiwat;

	if (sbchecklowmem()) {
		/* low on memory try to get rid of some */
		if (tcp_recvspace < nmax)
			nmax = tcp_recvspace;
	} else if (so->so_rcv.sb_wat != tcp_recvspace)
		/* user requested buffer size, auto-scaling disabled */
		nmax = so->so_rcv.sb_wat;
	else {
		/* automatic buffer scaling */
		if (tp->rfbuf_cnt > so->so_rcv.sb_hiwat / 8 * 7)
			nmax = MIN(sb_max, so->so_rcv.sb_hiwat +
			    tcp_autorcvbuf_inc);
	}

	/* a readable socket must be preserved because of poll(2) semantics */
	if (so->so_rcv.sb_cc >= so->so_rcv.sb_lowat &&
	    nmax < so->so_snd.sb_lowat)
		nmax = so->so_snd.sb_lowat;

	if (nmax == so->so_rcv.sb_hiwat)
		return;

	/* round to MSS boundary */
	nmax = roundup(nmax, tp->t_maxseg);
	sbreserve(so, &so->so_rcv, nmax);
}
