/*	$OpenBSD: tcp_output.c,v 1.13 1998/11/18 21:13:06 provos Exp $	*/
/*	$NetBSD: tcp_output.c,v 1.16 1997/06/03 16:17:09 kml Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)tcp_output.c	8.3 (Berkeley) 12/30/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#define	TCPOUTFLAGS
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#ifdef TUBA
#include <netiso/iso.h>
#include <netiso/tuba_table.h>
#endif

#ifdef notyet
extern struct mbuf *m_copypack();
#endif

#ifdef TCP_SACK
extern int tcprexmtthresh;
#endif

#ifdef TCP_SACK
#define MAX_TCPOPTLEN	40	/* need 40 at least for 3 SACKs + TIMESTAMP */
#else
#define MAX_TCPOPTLEN	32	/* max # bytes that go in options */
#endif

#ifdef TCP_SACK
#ifdef TCP_SACK_DEBUG
void
tcp_print_holes(tp)
struct tcpcb *tp;
{
	struct sackhole *p = tp->snd_holes;
	if (p == 0)
		return;
	printf("Hole report: start--end dups rxmit\n");
	while (p) {
		printf("%x--%x d %d r %x\n",  p->start, p->end, p->dups,
                    p->rxmit);
		p = p->next;
	}
	printf("\n");
}
#endif /* TCP_SACK_DEBUG */

/*
 * Returns pointer to a sackhole if there are any pending retransmissions;
 * NULL otherwise.
 */
struct sackhole *
tcp_sack_output(tp)
register struct tcpcb *tp;
{
	struct sackhole *p;
	if (tp->sack_disable)
		return 0;
	p = tp->snd_holes;
	while (p) {
		if (p->dups >= tcprexmtthresh && SEQ_LT(p->rxmit, p->end)) {
			if (SEQ_LT(p->rxmit, tp->snd_una)) {/* old SACK hole */
				p = p->next;
				continue;
			}
#ifdef TCP_SACK_DEBUG
			if (p)
				tcp_print_holes(tp);
#endif
			return p;
		}
        	p = p->next;
	}
	return 0;
}

/*
 * After a timeout, the SACK list may be rebuilt.  This SACK information
 * should be used to avoid retransmitting SACKed data.  This function
 * traverses the SACK list to see if snd_nxt should be moved forward.
 */
void
tcp_sack_adjust(tp)
	struct tcpcb *tp;
{
	int i;

	for (i = 0; i < tp->rcv_numsacks; i++) {
		if (SEQ_LT(tp->snd_nxt, tp->sackblks[i].start))
			break;
		if (SEQ_LEQ(tp->sackblks[i].end, tp->snd_nxt))
			continue;
		if (tp->sackblks[i].start == 0 && tp->sackblks[i].end == 0)
			continue;
		/* snd_nxt must be in middle of block of SACKed data */
		tp->snd_nxt = tp->sackblks[i].end;
		break;
	}
}
#endif /* TCP_SACK */

/*
 * Tcp output routine: figure out what should be sent and send it.
 */
int
tcp_output(tp)
	register struct tcpcb *tp;
{
	register struct socket *so = tp->t_inpcb->inp_socket;
	register long len, win;
	int off, flags, error;
	register struct mbuf *m;
	register struct tcpiphdr *ti;
	u_char opt[MAX_TCPOPTLEN];
	unsigned int optlen, hdrlen;
	int idle, sendalot;
#ifdef TCP_SACK
	int i, sack_rxmit = 0;
	struct sackhole *p;
#endif
#if defined(TCP_SACK) || defined(TCP_NEWRENO)
	int maxburst = TCP_MAXBURST;
#endif

	/*
	 * Determine length of data that should be transmitted,
	 * and flags that will be used.
	 * If there is some data or critical controls (SYN, RST)
	 * to send, then transmit; otherwise, investigate further.
	 */
	idle = (tp->snd_max == tp->snd_una);
	if (idle && tp->t_idle >= tp->t_rxtcur)
		/*
		 * We have been idle for "a while" and no acks are
		 * expected to clock out any data we send --
		 * slow start to get ack "clock" running again.
		 */
		tp->snd_cwnd = tp->t_maxseg;
again:
	sendalot = 0;
#ifdef TCP_SACK
	/*
	 * If we've recently taken a timeout, snd_max will be greater than
	 * snd_nxt.  There may be SACK information that allows us to avoid
	 * resending already delivered data.  Adjust snd_nxt accordingly.
	 */
	if (!tp->sack_disable && SEQ_LT(tp->snd_nxt, tp->snd_max))
		tcp_sack_adjust(tp);
#endif
	off = tp->snd_nxt - tp->snd_una;
	win = min(tp->snd_wnd, tp->snd_cwnd);

	flags = tcp_outflags[tp->t_state];
	/*
	 * If in persist timeout with window of 0, send 1 byte.
	 * Otherwise, if window is small but nonzero
	 * and timer expired, we will send what we can
	 * and go to transmit state.
	 */

#ifdef TCP_SACK
	/* 
	 * Send any SACK-generated retransmissions.  If we're explicitly trying
	 * to send out new data (when sendalot is 1), bypass this function.
	 * If we retransmit in fast recovery mode, decrement snd_cwnd, since
	 * we're replacing a (future) new transmission with a retransmission 
	 * now, and we previously incremented snd_cwnd in tcp_input().
	 */
	if (!tp->sack_disable && !sendalot) {
		if ((p = tcp_sack_output(tp))) {
			off = p->rxmit - tp->snd_una;
			sack_rxmit = 1;
#if 0
			/* Coalesce holes into a single retransmission */
#endif
			len = min(tp->t_maxseg, p->end - p->rxmit);
#ifndef TCP_FACK
			/* in FACK, hold snd_cwnd constant during recovery */
			if (SEQ_LT(tp->snd_una, tp->snd_last))
				tp->snd_cwnd -= tp->t_maxseg;
#endif
    		}
	}
#endif /* TCP_SACK */

	if (tp->t_force) {
		if (win == 0) {
			/*
			 * If we still have some data to send, then
			 * clear the FIN bit.  Usually this would
			 * happen below when it realizes that we
			 * aren't sending all the data.  However,
			 * if we have exactly 1 byte of unset data,
			 * then it won't clear the FIN bit below,
			 * and if we are in persist state, we wind
			 * up sending the packet without recording
			 * that we sent the FIN bit.
			 *
			 * We can't just blindly clear the FIN bit,
			 * because if we don't have any more data
			 * to send then the probe will be the FIN
			 * itself.
			 */
			if (off < so->so_snd.sb_cc)
				flags &= ~TH_FIN;
			win = 1;
		} else {
			tp->t_timer[TCPT_PERSIST] = 0;
			tp->t_rxtshift = 0;
		}
	}

#ifdef TCP_SACK
	if (!sack_rxmit) {
#endif
	len = min(so->so_snd.sb_cc, win) - off;

#if defined(TCP_SACK) && defined(TCP_FACK)
	/* 
	 * If we're in fast recovery (SEQ_GT(tp->snd_last, tp->snd_una)), and 
	 * amount of outstanding data (snd_awnd) is >= snd_cwnd, then
	 * do not send data (like zero window conditions)
	 */
	if (!tp->sack_disable && len && SEQ_GT(tp->snd_last, tp->snd_una) && 
	    (tp->snd_awnd >= tp->snd_cwnd)) 
		len = 0;
#endif /* TCP_FACK */
#ifdef TCP_SACK
	}
#endif

	if (len < 0) {
		/*
		 * If FIN has been sent but not acked,
		 * but we haven't been called to retransmit,
		 * len will be -1.  Otherwise, window shrank
		 * after we sent into it.  If window shrank to 0,
		 * cancel pending retransmit and pull snd_nxt
		 * back to (closed) window.  We will enter persist
		 * state below.  If the window didn't close completely,
		 * just wait for an ACK.
		 */
		len = 0;
		if (win == 0) {
			tp->t_timer[TCPT_REXMT] = 0;
			tp->snd_nxt = tp->snd_una;
		}
	}
	if (len > tp->t_maxseg) {
		len = tp->t_maxseg;
		sendalot = 1;
	}
	if (SEQ_LT(tp->snd_nxt + len, tp->snd_una + so->so_snd.sb_cc))
		flags &= ~TH_FIN;

	win = sbspace(&so->so_rcv);

	/*
	 * Sender silly window avoidance.  If connection is idle
	 * and can send all data, a maximum segment,
	 * at least a maximum default-size segment do it,
	 * or are forced, do it; otherwise don't bother.
	 * If peer's buffer is tiny, then send
	 * when window is at least half open.
	 * If retransmitting (possibly after persist timer forced us
	 * to send into a small window), then must resend.
	 */
	if (len) {
		if (len == tp->t_maxseg)
			goto send;
		if ((idle || tp->t_flags & TF_NODELAY) &&
		    len + off >= so->so_snd.sb_cc)
			goto send;
		if (tp->t_force)
			goto send;
		if (len >= tp->max_sndwnd / 2 && tp->max_sndwnd > 0)
			goto send;
		if (SEQ_LT(tp->snd_nxt, tp->snd_max))
			goto send;
#ifdef TCP_SACK
		if (sack_rxmit)
			goto send;
#endif
	}

	/*
	 * Compare available window to amount of window
	 * known to peer (as advertised window less
	 * next expected input).  If the difference is at least two
	 * max size segments, or at least 50% of the maximum possible
	 * window, then want to send a window update to peer.
	 */
	if (win > 0) {
		/* 
		 * "adv" is the amount we can increase the window,
		 * taking into account that we are limited by
		 * TCP_MAXWIN << tp->rcv_scale.
		 */
		long adv = min(win, (long)TCP_MAXWIN << tp->rcv_scale) -
			(tp->rcv_adv - tp->rcv_nxt);

		if (adv >= (long) (2 * tp->t_maxseg))
			goto send;
		if (2 * adv >= (long) so->so_rcv.sb_hiwat)
			goto send;
	}

	/*
	 * Send if we owe peer an ACK.
	 */
	if (tp->t_flags & TF_ACKNOW)
		goto send;
	if (flags & (TH_SYN|TH_RST))
		goto send;
	if (SEQ_GT(tp->snd_up, tp->snd_una))
		goto send;
	/*
	 * If our state indicates that FIN should be sent
	 * and we have not yet done so, or we're retransmitting the FIN,
	 * then we need to send.
	 */
	if (flags & TH_FIN &&
	    ((tp->t_flags & TF_SENTFIN) == 0 || tp->snd_nxt == tp->snd_una))
		goto send;

	/*
	 * TCP window updates are not reliable, rather a polling protocol
	 * using ``persist'' packets is used to insure receipt of window
	 * updates.  The three ``states'' for the output side are:
	 *	idle			not doing retransmits or persists
	 *	persisting		to move a small or zero window
	 *	(re)transmitting	and thereby not persisting
	 *
	 * tp->t_timer[TCPT_PERSIST]
	 *	is set when we are in persist state.
	 * tp->t_force
	 *	is set when we are called to send a persist packet.
	 * tp->t_timer[TCPT_REXMT]
	 *	is set when we are retransmitting
	 * The output side is idle when both timers are zero.
	 *
	 * If send window is too small, there is data to transmit, and no
	 * retransmit or persist is pending, then go to persist state.
	 * If nothing happens soon, send when timer expires:
	 * if window is nonzero, transmit what we can,
	 * otherwise force out a byte.
	 */
	if (so->so_snd.sb_cc && tp->t_timer[TCPT_REXMT] == 0 &&
	    tp->t_timer[TCPT_PERSIST] == 0) {
		tp->t_rxtshift = 0;
		tcp_setpersist(tp);
	}

	/*
	 * No reason to send a segment, just return.
	 */
	return (0);

send:
	/*
	 * Before ESTABLISHED, force sending of initial options
	 * unless TCP set not to do any options.
	 * NOTE: we assume that the IP/TCP header plus TCP options
	 * always fit in a single mbuf, leaving room for a maximum
	 * link header, i.e.
	 *	max_linkhdr + sizeof (struct tcpiphdr) + optlen <= MHLEN
	 */
	optlen = 0;
	hdrlen = sizeof (struct tcpiphdr);
	if (flags & TH_SYN) {
		tp->snd_nxt = tp->iss;
		if ((tp->t_flags & TF_NOOPT) == 0) {
			u_int16_t mss;

			opt[0] = TCPOPT_MAXSEG;
			opt[1] = 4;
			mss = htons((u_int16_t) tcp_mss(tp, 0));
			bcopy((caddr_t)&mss, (caddr_t)(opt + 2), sizeof(mss));
			optlen = 4;
#ifdef TCP_SACK
			/* 
			 * If this is the first SYN of connection (not a SYN 
			 * ACK), include SACK_PERMIT_HDR option.  If this is a 
			 * SYN ACK, include SACK_PERMIT_HDR option if peer has 
			 * already done so.
			 */
			if (!tp->sack_disable && ((flags & TH_ACK) == 0 ||
			    (tp->t_flags & TF_SACK_PERMIT))) {
				*((u_int32_t *) (opt + optlen)) =
				    htonl(TCPOPT_SACK_PERMIT_HDR);
				optlen += 4;
			}
#endif
	 
			if ((tp->t_flags & TF_REQ_SCALE) &&
			    ((flags & TH_ACK) == 0 ||
			    (tp->t_flags & TF_RCVD_SCALE))) {
				*((u_int32_t *) (opt + optlen)) = htonl(
					TCPOPT_NOP << 24 |
					TCPOPT_WINDOW << 16 |
					TCPOLEN_WINDOW << 8 |
					tp->request_r_scale);
				optlen += 4;
			}
		}
	}
 
	/*
	 * Send a timestamp and echo-reply if this is a SYN and our side 
	 * wants to use timestamps (TF_REQ_TSTMP is set) or both our side
	 * and our peer have sent timestamps in our SYN's.
	 */
	if ((tp->t_flags & (TF_REQ_TSTMP|TF_NOOPT)) == TF_REQ_TSTMP &&
	     (flags & TH_RST) == 0 &&
	    ((flags & (TH_SYN|TH_ACK)) == TH_SYN ||
	     (tp->t_flags & TF_RCVD_TSTMP))) {
		u_int32_t *lp = (u_int32_t *)(opt + optlen);
 
		/* Form timestamp option as shown in appendix A of RFC 1323. */
		*lp++ = htonl(TCPOPT_TSTAMP_HDR);
		*lp++ = htonl(tcp_now);
		*lp   = htonl(tp->ts_recent);
		optlen += TCPOLEN_TSTAMP_APPA;
	}

#ifdef TCP_SACK
	/*
	 * Send SACKs if necessary.  This should be the last option processed.
	 * Only as many SACKs are sent as are permitted by the maximum options
	 * size.  No more than three SACKs are sent.
	 */
	if (!tp->sack_disable && tp->t_state == TCPS_ESTABLISHED &&
	    (tp->t_flags & (TF_SACK_PERMIT|TF_NOOPT)) == TF_SACK_PERMIT &&
	    tp->rcv_numsacks) {
		u_int32_t *lp = (u_int32_t *)(opt + optlen);
		u_int32_t *olp = lp++;
		int count = 0;  /* actual number of SACKs inserted */
		int maxsack = (MAX_TCPOPTLEN - (optlen + 4))/TCPOLEN_SACK;

		maxsack = min(maxsack, TCP_MAX_SACK);
		for (i = 0; (i < tp->rcv_numsacks && count < maxsack); i++) {
			struct sackblk sack = tp->sackblks[i];
			if (sack.start == 0 && sack.end == 0)
				continue;
			*lp++ = htonl(sack.start);
			*lp++ = htonl(sack.end);
			count++;
		}
		*olp = htonl(TCPOPT_SACK_HDR|(TCPOLEN_SACK*count+2));
		optlen += TCPOLEN_SACK*count + 4; /* including leading NOPs */
	}
#endif /* TCP_SACK */

	hdrlen += optlen;
 
	/*
	 * Adjust data length if insertion of options will
	 * bump the packet length beyond the t_maxopd length.
	 */
	if (len > tp->t_maxopd - optlen) {
		len = tp->t_maxopd - optlen;
		sendalot = 1;
		flags &= ~TH_FIN;
	 }

#ifdef DIAGNOSTIC
	if (max_linkhdr + hdrlen > MHLEN)
		panic("tcphdr too big");
#endif

	/*
	 * Grab a header mbuf, attaching a copy of data to
	 * be transmitted, and initialize the header from
	 * the template for sends on this connection.
	 */
	if (len) {
		if (tp->t_force && len == 1)
			tcpstat.tcps_sndprobe++;
		else if (SEQ_LT(tp->snd_nxt, tp->snd_max)) {
			tcpstat.tcps_sndrexmitpack++;
			tcpstat.tcps_sndrexmitbyte += len;
		} else {
			tcpstat.tcps_sndpack++;
			tcpstat.tcps_sndbyte += len;
		}
#ifdef notyet
		if ((m = m_copypack(so->so_snd.sb_mb, off,
		    (int)len, max_linkhdr + hdrlen)) == 0) {
			error = ENOBUFS;
			goto out;
		}
		/*
		 * m_copypack left space for our hdr; use it.
		 */
		m->m_len += hdrlen;
		m->m_data -= hdrlen;
#else
		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
		if (len <= MHLEN - hdrlen - max_linkhdr) {
			m_copydata(so->so_snd.sb_mb, off, (int) len,
			    mtod(m, caddr_t) + hdrlen);
			m->m_len += len;
		} else {
			m->m_next = m_copy(so->so_snd.sb_mb, off, (int) len);
			if (m->m_next == 0) {
				(void) m_free(m);
				error = ENOBUFS;
				goto out;
			}
		}
#endif
		/*
		 * If we're sending everything we've got, set PUSH.
		 * (This will keep happy those implementations which only
		 * give data to the user when a buffer fills or
		 * a PUSH comes in.)
		 */
		if (off + len == so->so_snd.sb_cc)
			flags |= TH_PUSH;
	} else {
		if (tp->t_flags & TF_ACKNOW)
			tcpstat.tcps_sndacks++;
		else if (flags & (TH_SYN|TH_FIN|TH_RST))
			tcpstat.tcps_sndctrl++;
		else if (SEQ_GT(tp->snd_up, tp->snd_una))
			tcpstat.tcps_sndurg++;
		else
			tcpstat.tcps_sndwinup++;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			error = ENOBUFS;
			goto out;
		}
		m->m_data += max_linkhdr;
		m->m_len = hdrlen;
	}
	m->m_pkthdr.rcvif = (struct ifnet *)0;
	ti = mtod(m, struct tcpiphdr *);
	if (tp->t_template == 0)
		panic("tcp_output");
	bcopy((caddr_t)tp->t_template, (caddr_t)ti, sizeof (struct tcpiphdr));

	/*
	 * Fill in fields, remembering maximum advertised
	 * window for use in delaying messages about window sizes.
	 * If resending a FIN, be sure not to use a new sequence number.
	 */
	if ((flags & TH_FIN) && (tp->t_flags & TF_SENTFIN) && 
	    (tp->snd_nxt == tp->snd_max))
		tp->snd_nxt--;
	/*
	 * If we are doing retransmissions, then snd_nxt will
	 * not reflect the first unsent octet.  For ACK only
	 * packets, we do not want the sequence number of the
	 * retransmitted packet, we want the sequence number
	 * of the next unsent octet.  So, if there is no data
	 * (and no SYN or FIN), use snd_max instead of snd_nxt
	 * when filling in ti_seq.  But if we are in persist
	 * state, snd_max might reflect one byte beyond the
	 * right edge of the window, so use snd_nxt in that
	 * case, since we know we aren't doing a retransmission.
	 * (retransmit and persist are mutually exclusive...)
	 */
	if (len || (flags & (TH_SYN|TH_FIN)) || tp->t_timer[TCPT_PERSIST])
		ti->ti_seq = htonl(tp->snd_nxt);
	else
		ti->ti_seq = htonl(tp->snd_max);
#ifdef TCP_SACK
	if (sack_rxmit) {
		/* 
		 * If sendalot was turned on (due to option stuffing), turn it 
		 * off. Properly set th_seq field.  Advance the ret'x pointer 
		 * by len.  
		 */
		if (sendalot)
			sendalot = 0;
		ti->ti_seq = htonl(p->rxmit);
		p->rxmit += len;
#if defined(TCP_SACK) && defined(TCP_FACK)
		tp->retran_data += len;
#endif /* TCP_FACK */
	}
#endif /* TCP_SACK */

	ti->ti_ack = htonl(tp->rcv_nxt);
	if (optlen) {
		bcopy((caddr_t)opt, (caddr_t)(ti + 1), optlen);
		ti->ti_off = (sizeof (struct tcphdr) + optlen) >> 2;
	}
	ti->ti_flags = flags;
	/*
	 * Calculate receive window.  Don't shrink window,
	 * but avoid silly window syndrome.
	 */
	if (win < (long)(so->so_rcv.sb_hiwat / 4) && win < (long)tp->t_maxseg)
		win = 0;
	if (win > (long)TCP_MAXWIN << tp->rcv_scale)
		win = (long)TCP_MAXWIN << tp->rcv_scale;
	if (win < (long)(tp->rcv_adv - tp->rcv_nxt))
		win = (long)(tp->rcv_adv - tp->rcv_nxt);
	if (flags & TH_RST)
		win = 0;
	ti->ti_win = htons((u_int16_t) (win>>tp->rcv_scale));
	if (SEQ_GT(tp->snd_up, tp->snd_nxt)) {
		u_int32_t urp = tp->snd_up - tp->snd_nxt;
		if (urp > IP_MAXPACKET)
			urp = IP_MAXPACKET;
		ti->ti_urp = htons((u_int16_t)urp);
		ti->ti_flags |= TH_URG;
	} else
		/*
		 * If no urgent pointer to send, then we pull
		 * the urgent pointer to the left edge of the send window
		 * so that it doesn't drift into the send window on sequence
		 * number wraparound.
		 */
		tp->snd_up = tp->snd_una;		/* drag it along */

	/*
	 * Put TCP length in extended header, and then
	 * checksum extended header and data.
	 */
	if (len + optlen)
		ti->ti_len = htons((u_int16_t)(sizeof (struct tcphdr) +
		    optlen + len));
	ti->ti_sum = in_cksum(m, (int)(hdrlen + len));

	/*
	 * In transmit state, time the transmission and arrange for
	 * the retransmit.  In persist state, just set snd_max.
	 */
	if (tp->t_force == 0 || tp->t_timer[TCPT_PERSIST] == 0) {
		tcp_seq startseq = tp->snd_nxt;

		/*
		 * Advance snd_nxt over sequence space of this segment.
		 */
		if (flags & (TH_SYN|TH_FIN)) {
			if (flags & TH_SYN)
				tp->snd_nxt++;
			if (flags & TH_FIN) {
				tp->snd_nxt++;
				tp->t_flags |= TF_SENTFIN;
			}
		}
#ifdef TCP_SACK
		if (!tp->sack_disable) {
			if (sack_rxmit && (p->rxmit != tp->snd_nxt)) {
				goto timer;
			}
		}
#endif
		tp->snd_nxt += len;
		if (SEQ_GT(tp->snd_nxt, tp->snd_max)) {
			tp->snd_max = tp->snd_nxt;
			/*
			 * Time this transmission if not a retransmission and
			 * not currently timing anything.
			 */
			if (tp->t_rtt == 0) {
				tp->t_rtt = 1;
				tp->t_rtseq = startseq;
				tcpstat.tcps_segstimed++;
			}
		}

		/*
		 * Set retransmit timer if not currently set,
		 * and not doing an ack or a keep-alive probe.
		 * Initial value for retransmit timer is smoothed
		 * round-trip time + 2 * round-trip time variance.
		 * Initialize shift counter which is used for backoff
		 * of retransmit time.
		 */
#ifdef TCP_SACK
 timer:
		if (!tp->sack_disable && sack_rxmit &&
		    tp->t_timer[TCPT_REXMT] == 0 &&
		    tp->snd_nxt != tp->snd_max) {
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
			if (tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}
#endif

		if (tp->t_timer[TCPT_REXMT] == 0 &&
		    tp->snd_nxt != tp->snd_una) {
			tp->t_timer[TCPT_REXMT] = tp->t_rxtcur;
			if (tp->t_timer[TCPT_PERSIST]) {
				tp->t_timer[TCPT_PERSIST] = 0;
				tp->t_rxtshift = 0;
			}
		}
	} else
		if (SEQ_GT(tp->snd_nxt + len, tp->snd_max))
			tp->snd_max = tp->snd_nxt + len;

	/*
	 * Trace.
	 */
	if (so->so_options & SO_DEBUG)
		tcp_trace(TA_OUTPUT, tp->t_state, tp, ti, 0);

	/*
	 * Fill in IP length and desired time to live and
	 * send to IP level.  There should be a better way
	 * to handle ttl and tos; we could keep them in
	 * the template, but need a way to checksum without them.
	 */
	m->m_pkthdr.len = hdrlen + len;
#ifdef TUBA
	if (tp->t_tuba_pcb)
		error = tuba_output(m, tp);
	else
#endif
    {
	((struct ip *)ti)->ip_len = m->m_pkthdr.len;
	((struct ip *)ti)->ip_ttl = tp->t_inpcb->inp_ip.ip_ttl;	/* XXX */
	((struct ip *)ti)->ip_tos = tp->t_inpcb->inp_ip.ip_tos;	/* XXX */
#if BSD >= 43
	error = ip_output(m, tp->t_inpcb->inp_options, &tp->t_inpcb->inp_route,
	    so->so_options & SO_DONTROUTE, 0, tp->t_inpcb);
#else
	error = ip_output(m, (struct mbuf *)0, &tp->t_inpcb->inp_route, 
	    so->so_options & SO_DONTROUTE);
#endif
#if defined(TCP_SACK) && defined(TCP_FACK)
	/* Update snd_awnd to reflect the new data that was sent.  */
        tp->snd_awnd = tcp_seq_subtract(tp->snd_max, tp->snd_fack) +
            tp->retran_data;                
#endif
    }
	if (error) {
out:
		if (error == ENOBUFS) {
			tcp_quench(tp->t_inpcb, 0);
			return (0);
		}
		if ((error == EHOSTUNREACH || error == ENETDOWN)
		    && TCPS_HAVERCVDSYN(tp->t_state)) {
			tp->t_softerror = error;
			return (0);
		}
		return (error);
	}
	tcpstat.tcps_sndtotal++;

	/*
	 * Data sent (as far as we can tell).
	 * If this advertises a larger window than any other segment,
	 * then remember the size of the advertised window.
	 * Any pending ACK has now been sent.
	 */
	if (win > 0 && SEQ_GT(tp->rcv_nxt+win, tp->rcv_adv))
		tp->rcv_adv = tp->rcv_nxt + win;
	tp->last_ack_sent = tp->rcv_nxt;
	tp->t_flags &= ~(TF_ACKNOW|TF_DELACK);
#if defined(TCP_SACK) || defined(TCP_NEWRENO)
	if (sendalot && --maxburst)
#else
	if (sendalot)
#endif
		goto again;
	return (0);
}

void
tcp_setpersist(tp)
	register struct tcpcb *tp;
{
	register int t = ((tp->t_srtt >> 2) + tp->t_rttvar) >> 1;

	if (tp->t_timer[TCPT_REXMT])
		panic("tcp_output REXMT");
	/*
	 * Start/restart persistance timer.
	 */
	TCPT_RANGESET(tp->t_timer[TCPT_PERSIST],
	    t * tcp_backoff[tp->t_rxtshift],
	    TCPTV_PERSMIN, TCPTV_PERSMAX);
	if (tp->t_rxtshift < TCP_MAXRXTSHIFT)
		tp->t_rxtshift++;
}
