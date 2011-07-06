/*	$OpenBSD: tcp_timer.c,v 1.46 2011/07/06 23:44:20 sthen Exp $	*/
/*	$NetBSD: tcp_timer.c,v 1.14 1996/02/13 23:44:09 christos Exp $	*/

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
 *	@(#)tcp_timer.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/pool.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp_seq.h>

int	tcp_always_keepalive;
int	tcp_keepidle;
int	tcp_keepintvl;
int	tcp_maxpersistidle;	/* max idle time in persist */
int	tcp_maxidle;

/*
 * Time to delay the ACK.  This is initialized in tcp_init(), unless
 * its patched.
 */
int	tcp_delack_ticks;

void	tcp_timer_rexmt(void *);
void	tcp_timer_persist(void *);
void	tcp_timer_keep(void *);
void	tcp_timer_2msl(void *);

const tcp_timer_func_t tcp_timer_funcs[TCPT_NTIMERS] = {
	tcp_timer_rexmt,
	tcp_timer_persist,
	tcp_timer_keep,
	tcp_timer_2msl,
};

/*
 * Timer state initialization, called from tcp_init().
 */
void
tcp_timer_init(void)
{

	if (tcp_keepidle == 0)
		tcp_keepidle = TCPTV_KEEP_IDLE;

	if (tcp_keepintvl == 0)
		tcp_keepintvl = TCPTV_KEEPINTVL;

	if (tcp_maxpersistidle == 0)
		tcp_maxpersistidle = TCPTV_KEEP_IDLE;

	if (tcp_delack_ticks == 0)
		tcp_delack_ticks = TCP_DELACK_TICKS;
}

/*
 * Callout to process delayed ACKs for a TCPCB.
 */
void
tcp_delack(void *arg)
{
	struct tcpcb *tp = arg;
	int s;

	/*
	 * If tcp_output() wasn't able to transmit the ACK
	 * for whatever reason, it will restart the delayed
	 * ACK callout.
	 */

	s = splsoftnet();
	if (tp->t_flags & TF_DEAD) {
		splx(s);
		return;
	}
	tp->t_flags |= TF_ACKNOW;
	(void) tcp_output(tp);
	splx(s);
}

/*
 * Tcp protocol timeout routine called every 500 ms.
 * Updates the timers in all active tcb's and
 * causes finite state machine actions if timers expire.
 */
void
tcp_slowtimo()
{
	int s;

	s = splsoftnet();
	tcp_maxidle = TCPTV_KEEPCNT * tcp_keepintvl;
	tcp_iss += TCP_ISSINCR2/PR_SLOWHZ;		/* increment iss */
	tcp_now++;					/* for timestamps */
	splx(s);
}

/*
 * Cancel all timers for TCP tp.
 */
void
tcp_canceltimers(tp)
	struct tcpcb *tp;
{
	int i;

	for (i = 0; i < TCPT_NTIMERS; i++)
		TCP_TIMER_DISARM(tp, i);
}

int	tcp_backoff[TCP_MAXRXTSHIFT + 1] =
    { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

int tcp_totbackoff = 511;	/* sum of tcp_backoff[] */

/*
 * TCP timer processing.
 */

#ifdef TCP_SACK
void	tcp_timer_freesack(struct tcpcb *);

void
tcp_timer_freesack(struct tcpcb *tp)
{
	struct sackhole *p, *q;
	/*
	 * Free SACK holes for 2MSL and REXMT timers.
	 */
	q = tp->snd_holes;
	while (q != NULL) {
		p = q;
		q = q->next;
		pool_put(&sackhl_pool, p);
	}
	tp->snd_holes = 0;
#ifdef TCP_FACK
	tp->snd_fack = tp->snd_una;
	tp->retran_data = 0;
	tp->snd_awnd = 0;
#endif /* TCP_FACK */
}
#endif /* TCP_SACK */

void
tcp_timer_rexmt(void *arg)
{
	struct tcpcb *tp = arg;
	uint32_t rto;
	int s;

	s = splsoftnet();
	if (tp->t_flags & TF_DEAD) {
		splx(s);
		return;
	}

	if ((tp->t_flags & TF_PMTUD_PEND) && tp->t_inpcb &&
	    SEQ_GEQ(tp->t_pmtud_th_seq, tp->snd_una) &&
	    SEQ_LT(tp->t_pmtud_th_seq, (int)(tp->snd_una + tp->t_maxseg))) {
		extern struct sockaddr_in icmpsrc;
		struct icmp icmp;

		tp->t_flags &= ~TF_PMTUD_PEND;

		/* XXX create fake icmp message with relevant entries */
		icmp.icmp_nextmtu = tp->t_pmtud_nextmtu;
		icmp.icmp_ip.ip_len = tp->t_pmtud_ip_len;
		icmp.icmp_ip.ip_hl = tp->t_pmtud_ip_hl;
		icmpsrc.sin_addr = tp->t_inpcb->inp_faddr;
		icmp_mtudisc(&icmp, tp->t_inpcb->inp_rtableid);

		/*
		 * Notify all connections to the same peer about
		 * new mss and trigger retransmit.
		 */
		in_pcbnotifyall(&tcbtable, sintosa(&icmpsrc),
		    tp->t_inpcb->inp_rtableid, EMSGSIZE, tcp_mtudisc);
		splx(s);
		return;
	}

#ifdef TCP_SACK
	tcp_timer_freesack(tp);
#endif
	if (++tp->t_rxtshift > TCP_MAXRXTSHIFT) {
		tp->t_rxtshift = TCP_MAXRXTSHIFT;
		tcpstat.tcps_timeoutdrop++;
		(void)tcp_drop(tp, tp->t_softerror ?
		    tp->t_softerror : ETIMEDOUT);
		goto out;
	}
	tcpstat.tcps_rexmttimeo++;
	rto = TCP_REXMTVAL(tp);
	if (rto < tp->t_rttmin)
		rto = tp->t_rttmin;
	TCPT_RANGESET(tp->t_rxtcur,
	    rto * tcp_backoff[tp->t_rxtshift],
	    tp->t_rttmin, TCPTV_REXMTMAX);
	TCP_TIMER_ARM(tp, TCPT_REXMT, tp->t_rxtcur);

	/*
	 * If we are losing and we are trying path MTU discovery,
	 * try turning it off.  This will avoid black holes in
	 * the network which suppress or fail to send "packet
	 * too big" ICMP messages.  We should ideally do
	 * lots more sophisticated searching to find the right
	 * value here...
	 */
	if (ip_mtudisc && tp->t_inpcb &&
	    TCPS_HAVEESTABLISHED(tp->t_state) &&
	    tp->t_rxtshift > TCP_MAXRXTSHIFT / 6) {
		struct inpcb *inp = tp->t_inpcb;
		struct rtentry *rt = NULL;
		struct sockaddr_in sin;

		/* No data to send means path mtu is not a problem */
		if (!inp->inp_socket->so_snd.sb_cc)
			goto leave;

		rt = in_pcbrtentry(inp);
		/* Check if path MTU discovery is disabled already */
		if (rt && (rt->rt_flags & RTF_HOST) &&
		    (rt->rt_rmx.rmx_locks & RTV_MTU))
			goto leave;

		rt = NULL;
		switch(tp->pf) {
#ifdef INET6
		case PF_INET6:
			/*
			 * We can not turn off path MTU for IPv6.
			 * Do nothing for now, maybe lower to
			 * minimum MTU.
			 */
			break;
#endif
		case PF_INET:
			bzero(&sin, sizeof(struct sockaddr_in));
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof(struct sockaddr_in);
			sin.sin_addr = inp->inp_faddr;
			rt = icmp_mtudisc_clone(sintosa(&sin), 
			    inp->inp_rtableid);
			break;
		}
		if (rt != NULL) {
			/* Disable path MTU discovery */
			if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0) {
				rt->rt_rmx.rmx_locks |= RTV_MTU;
				in_rtchange(inp, 0);
			}

			rtfree(rt);
		}
	leave:
		;
	}

	/*
	 * If losing, let the lower level know and try for
	 * a better route.  Also, if we backed off this far,
	 * our srtt estimate is probably bogus.  Clobber it
	 * so we'll take the next rtt measurement as our srtt;
	 * move the current srtt into rttvar to keep the current
	 * retransmit times until then.
	 */
	if (tp->t_rxtshift > TCP_MAXRXTSHIFT / 4) {
		in_losing(tp->t_inpcb);
		tp->t_rttvar += (tp->t_srtt >> TCP_RTT_SHIFT);
		tp->t_srtt = 0;
	}
	tp->snd_nxt = tp->snd_una;
#if defined(TCP_SACK)
	/*
	 * Note:  We overload snd_last to function also as the
	 * snd_last variable described in RFC 2582
	 */
	tp->snd_last = tp->snd_max;
#endif /* TCP_SACK */
	/*
	 * If timing a segment in this window, stop the timer.
	 */
	tp->t_rtttime = 0;
#ifdef TCP_ECN
	/*
	 * if ECN is enabled, there might be a broken firewall which
	 * blocks ecn packets.  fall back to non-ecn.
	 */
	if ((tp->t_state == TCPS_SYN_SENT || tp->t_state == TCPS_SYN_RECEIVED)
	    && tcp_do_ecn && !(tp->t_flags & TF_DISABLE_ECN))
		tp->t_flags |= TF_DISABLE_ECN;
#endif
	/*
	 * Close the congestion window down to one segment
	 * (we'll open it by one segment for each ack we get).
	 * Since we probably have a window's worth of unacked
	 * data accumulated, this "slow start" keeps us from
	 * dumping all that data as back-to-back packets (which
	 * might overwhelm an intermediate gateway).
	 *
	 * There are two phases to the opening: Initially we
	 * open by one mss on each ack.  This makes the window
	 * size increase exponentially with time.  If the
	 * window is larger than the path can handle, this
	 * exponential growth results in dropped packet(s)
	 * almost immediately.  To get more time between
	 * drops but still "push" the network to take advantage
	 * of improving conditions, we switch from exponential
	 * to linear window opening at some threshold size.
	 * For a threshold, we use half the current window
	 * size, truncated to a multiple of the mss.
	 *
	 * (the minimum cwnd that will give us exponential
	 * growth is 2 mss.  We don't allow the threshold
	 * to go below this.)
	 */
	{
		u_long win = ulmin(tp->snd_wnd, tp->snd_cwnd) / 2 / tp->t_maxseg;
		if (win < 2)
			win = 2;
		tp->snd_cwnd = tp->t_maxseg;
		tp->snd_ssthresh = win * tp->t_maxseg;
		tp->t_dupacks = 0;
#ifdef TCP_ECN
		tp->snd_last = tp->snd_max;
		tp->t_flags |= TF_SEND_CWR;
#endif
#if 1 /* TCP_ECN */
		tcpstat.tcps_cwr_timeout++;
#endif
	}
	(void) tcp_output(tp);

 out:
	splx(s);
}

void
tcp_timer_persist(void *arg)
{
	struct tcpcb *tp = arg;
	uint32_t rto;
	int s;

	s = splsoftnet();
	if ((tp->t_flags & TF_DEAD) ||
            TCP_TIMER_ISARMED(tp, TCPT_REXMT)) {
		splx(s);
		return;
	}
	tcpstat.tcps_persisttimeo++;
	/*
	 * Hack: if the peer is dead/unreachable, we do not
	 * time out if the window is closed.  After a full
	 * backoff, drop the connection if the idle time
	 * (no responses to probes) reaches the maximum
	 * backoff that we would use if retransmitting.
	 */
	rto = TCP_REXMTVAL(tp);
	if (rto < tp->t_rttmin)
		rto = tp->t_rttmin;
	if (tp->t_rxtshift == TCP_MAXRXTSHIFT &&
	    ((tcp_now - tp->t_rcvtime) >= tcp_maxpersistidle ||
	    (tcp_now - tp->t_rcvtime) >= rto * tcp_totbackoff)) {
		tcpstat.tcps_persistdrop++;
		tp = tcp_drop(tp, ETIMEDOUT);
		goto out;
	}
	tcp_setpersist(tp);
	tp->t_force = 1;
	(void) tcp_output(tp);
	tp->t_force = 0;
 out:
	splx(s);
}

void
tcp_timer_keep(void *arg)
{
	struct tcpcb *tp = arg;
	int s;

	s = splsoftnet();
	if (tp->t_flags & TF_DEAD) {
		splx(s);
		return;
	}

	tcpstat.tcps_keeptimeo++;
	if (TCPS_HAVEESTABLISHED(tp->t_state) == 0)
		goto dropit;
	if ((tcp_always_keepalive ||
	    tp->t_inpcb->inp_socket->so_options & SO_KEEPALIVE) &&
	    tp->t_state <= TCPS_CLOSING) {
		if ((tcp_maxidle > 0) &&
		    ((tcp_now - tp->t_rcvtime) >= tcp_keepidle + tcp_maxidle))
			goto dropit;
		/*
		 * Send a packet designed to force a response
		 * if the peer is up and reachable:
		 * either an ACK if the connection is still alive,
		 * or an RST if the peer has closed the connection
		 * due to timeout or reboot.
		 * Using sequence number tp->snd_una-1
		 * causes the transmitted zero-length segment
		 * to lie outside the receive window;
		 * by the protocol spec, this requires the
		 * correspondent TCP to respond.
		 */
		tcpstat.tcps_keepprobe++;
		tcp_respond(tp, mtod(tp->t_template, caddr_t),
		    NULL, tp->rcv_nxt, tp->snd_una - 1, 0, 0);
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcp_keepintvl);
	} else
		TCP_TIMER_ARM(tp, TCPT_KEEP, tcp_keepidle);

	splx(s);
	return;

 dropit:
	tcpstat.tcps_keepdrops++;
	tp = tcp_drop(tp, ETIMEDOUT);

	splx(s);
}

void
tcp_timer_2msl(void *arg)
{
	struct tcpcb *tp = arg;
	int s;

	s = splsoftnet();
	if (tp->t_flags & TF_DEAD) {
		splx(s);
		return;
	}

#ifdef TCP_SACK
	tcp_timer_freesack(tp);
#endif

	if (tp->t_state != TCPS_TIME_WAIT &&
	    ((tcp_maxidle == 0) || ((tcp_now - tp->t_rcvtime) <= tcp_maxidle)))
		TCP_TIMER_ARM(tp, TCPT_2MSL, tcp_keepintvl);
	else
		tp = tcp_close(tp);

	splx(s);
}
