/*	$OpenBSD: tcp_var.h,v 1.15 1999/01/11 02:01:36 deraadt Exp $	*/
/*	$NetBSD: tcp_var.h,v 1.17 1996/02/13 23:44:24 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993, 1994
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
 *	@(#)tcp_var.h	8.3 (Berkeley) 4/10/94
 */

struct sackblk {   
	tcp_seq start;		/* start seq no. of sack block */
	tcp_seq end; 		/* end seq no. */
};  
    
struct sackhole {   
	tcp_seq start;      	/* start seq no. of hole */ 
	tcp_seq end;        	/* end seq no. */
	int dups;      		/* number of dup(s)acks for this hole */
	tcp_seq rxmit;      	/* next seq. no in hole to be retransmitted */
	struct sackhole *next;  /* next in list */
};

/*
 * Kernel variables for tcp.
 */

/*
 * Tcp control block, one per tcp; fields:
 */
struct tcpcb {
	struct ipqehead segq;		/* sequencing queue */
	short	t_state;		/* state of this connection */
	short	t_timer[TCPT_NTIMERS];	/* tcp timers */
	short	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	t_rxtcur;		/* current retransmit value */
	short	t_dupacks;		/* consecutive dup acks recd */
	u_short	t_maxseg;		/* maximum segment size */
	char	t_force;		/* 1 if forcing out a byte */
	u_short	t_flags;
#define	TF_ACKNOW	0x0001		/* ack peer immediately */
#define	TF_DELACK	0x0002		/* ack, but try to delay it */
#define	TF_NODELAY	0x0004		/* don't delay packets to coalesce */
#define	TF_NOOPT	0x0008		/* don't use tcp options */
#define	TF_SENTFIN	0x0010		/* have sent FIN */
#define	TF_REQ_SCALE	0x0020		/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x0040		/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x0080		/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x0100		/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x0200		/* other side said I could SACK */

	struct	tcpiphdr *t_template;	/* skeletal packet for transmit, will
					 * be either tcpiphdr or tcpipv6hdr */
	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
/*
 * The following fields are used as in the protocol specification.
 * See RFC783, Dec. 1981, page 21.
 */
/* send sequence variables */
	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	u_long	snd_wnd;		/* send window */
#ifdef TCP_SACK
	int sack_disable;            	/* disable SACK for this connection */
	int snd_numholes; 		/* number of holes seen by sender */
	struct sackhole *snd_holes;     /* linked list of holes (sorted) */
#if defined(TCP_SACK) && defined(TCP_FACK)
	tcp_seq snd_fack;		/* for FACK congestion control */
	u_long	snd_awnd;		/* snd_nxt - snd_fack + */
					/* retransmitted data */
	int retran_data;		/* amount of outstanding retx. data  */
#endif /* TCP_FACK */
#endif /* TCP_SACK */
#if defined(TCP_SACK) || defined(TCP_NEWRENO)
	tcp_seq snd_last;		/* for use in fast recovery */
#endif
/* receive sequence variables */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	tcp_seq	irs;			/* initial receive sequence number */
#ifdef TCP_SACK
	tcp_seq rcv_laststart;          /* start of last segment recd. */
	tcp_seq rcv_lastend;            /* end of ... */
	tcp_seq rcv_lastsack;           /* last seq number(+1) sack'd by rcv'r*/
	int rcv_numsacks;           	/* # distinct sack blks present */
	struct sackblk sackblks[MAX_SACK_BLKS];  /* seq nos. of sack blocks */
#endif

/*
 * Additional variables for this implementation.
 */
/* receive variables */
	tcp_seq	rcv_adv;		/* advertised window */
/* retransmit variables */
	tcp_seq	snd_max;		/* highest sequence number sent;
					 * used to recognize retransmits
					 */
/* congestion control (for slow start, source quench, retransmit after loss) */
	u_long	snd_cwnd;		/* congestion-controlled window */
	u_long	snd_ssthresh;		/* snd_cwnd size threshhold for
					 * for slow start exponential to
					 * linear switch
					 */
	u_int	t_maxopd;		/* mss plus options */

/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
	short	t_idle;			/* inactivity time */
	short	t_rtt;			/* round trip time */
	tcp_seq	t_rtseq;		/* sequence number being timed */
	short	t_srtt;			/* smoothed round-trip time */
	short	t_rttvar;		/* variance in round-trip time */
	u_short	t_rttmin;		/* minimum rtt allowed */
	u_long	max_sndwnd;		/* largest window peer has offered */

/* out-of-band data */
	char	t_oobflags;		/* have some */
	char	t_iobc;			/* input character */
#define	TCPOOB_HAVEDATA	0x01
#define	TCPOOB_HADDATA	0x02
	short	t_softerror;		/* possible error not yet reported */

/* RFC 1323 variables */
	u_char	snd_scale;		/* window scaling for send window */
	u_char	rcv_scale;		/* window scaling for recv window */
	u_char	request_r_scale;	/* pending window scaling */
	u_char	requested_s_scale;
	u_int32_t ts_recent;		/* timestamp echo data */
	u_int32_t ts_recent_age;		/* when last updated */
	tcp_seq	last_ack_sent;

/* TUBA stuff */
	caddr_t	t_tuba_pcb;		/* next level down pcb for TCP over z */

	int pf;
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 3 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 2 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SCALE		8	/* multiplier for srtt; 3 bits frac. */
#define	TCP_RTT_SHIFT		3	/* shift for srtt; 3 bits frac. */
#define	TCP_RTTVAR_SCALE	4	/* multiplier for rttvar; 2 bits */
#define	TCP_RTTVAR_SHIFT	2	/* multiplier for rttvar; 2 bits */

/*
 * The initial retransmission should happen at rtt + 4 * rttvar.
 * Because of the way we do the smoothing, srtt and rttvar
 * will each average +1/2 tick of bias.  When we compute
 * the retransmit timer, we want 1/2 tick of rounding and
 * 1 extra tick because of +-1/2 tick uncertainty in the
 * firing of the timer.  The bias will give us exactly the
 * 1.5 tick we need.  But, because the bias is
 * statistical, we have to test that we don't drop below
 * the minimum feasible timer (which is 2 ticks).
 * This macro assumes that the value of TCP_RTTVAR_SCALE
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	((((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar) >> 2)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	u_long	tcps_connattempt;	/* connections initiated */
	u_long	tcps_accepts;		/* connections accepted */
	u_long	tcps_connects;		/* connections established */
	u_long	tcps_drops;		/* connections dropped */
	u_long	tcps_conndrops;		/* embryonic connections dropped */
	u_long	tcps_closed;		/* conn. closed (includes drops) */
	u_long	tcps_segstimed;		/* segs where we tried to get rtt */
	u_long	tcps_rttupdated;	/* times we succeeded */
	u_long	tcps_delack;		/* delayed acks sent */
	u_long	tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_long	tcps_rexmttimeo;	/* retransmit timeouts */
	u_long	tcps_persisttimeo;	/* persist timeouts */
	u_long	tcps_persistdrop;	/* connections dropped in persist */
	u_long	tcps_keeptimeo;		/* keepalive timeouts */
	u_long	tcps_keepprobe;		/* keepalive probes sent */
	u_long	tcps_keepdrops;		/* connections dropped in keepalive */

	u_long	tcps_sndtotal;		/* total packets sent */
	u_long	tcps_sndpack;		/* data packets sent */
	u_quad_t tcps_sndbyte;		/* data bytes sent */
	u_long	tcps_sndrexmitpack;	/* data packets retransmitted */
	u_quad_t tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_quad_t tcps_sndrexmitfast;	/* Fast retransmits */
	u_long	tcps_sndacks;		/* ack-only packets sent */
	u_long	tcps_sndprobe;		/* window probes sent */
	u_long	tcps_sndurg;		/* packets sent with URG only */
	u_long	tcps_sndwinup;		/* window update-only packets sent */
	u_long	tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u_long	tcps_rcvtotal;		/* total packets received */
	u_long	tcps_rcvpack;		/* packets received in sequence */
	u_quad_t tcps_rcvbyte;		/* bytes received in sequence */
	u_long	tcps_rcvbadsum;		/* packets received with ccksum errs */
	u_long	tcps_rcvbadoff;		/* packets received with bad offset */
	u_long	tcps_rcvmemdrop;	/* packets dropped for lack of memory */
	u_long	tcps_rcvshort;		/* packets received too short */
	u_long	tcps_rcvduppack;	/* duplicate-only packets received */
	u_quad_t tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_long	tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_quad_t tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_long	tcps_rcvoopack;		/* out-of-order packets received */
	u_quad_t tcps_rcvoobyte;	/* out-of-order bytes received */
	u_long	tcps_rcvpackafterwin;	/* packets with data after window */
	u_quad_t tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_long	tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_long	tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_long	tcps_rcvdupack;		/* rcvd duplicate acks */
	u_long	tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_long	tcps_rcvackpack;	/* rcvd ack packets */
	u_quad_t tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_long	tcps_rcvwinupd;		/* rcvd window update packets */
	u_long	tcps_pawsdrop;		/* segments dropped due to PAWS */
	u_long	tcps_predack;		/* times hdr predict ok for acks */
	u_long	tcps_preddat;		/* times hdr predict ok for data pkts */

	u_long	tcps_pcbhashmiss;	/* input packets missing pcb hash */
	u_long	tcps_noport;		/* no socket on port */
	u_long	tcps_badsyn;		/* SYN packet with src==dst rcv'ed */
};

/*
 * Names for TCP sysctl objects.
 */
			
#define	TCPCTL_RFC1323		1 /* enable/disable RFC1323 timestamps/scaling */
#define	TCPCTL_KEEPINITTIME	2 /* TCPT_KEEP value */
#define TCPCTL_KEEPIDLE		3 /* allow tcp_keepidle to be changed */
#define TCPCTL_KEEPINTVL	4 /* allow tcp_keepintvl to be changed */
#define TCPCTL_SLOWHZ		5 /* return kernel idea of PR_SLOWHZ */ 
#define TCPCTL_BADDYNAMIC	6 /* return bad dynamic port bitmap */ 
#define	TCPCTL_RECVSPACE	7 /* receive buffer space */
#define	TCPCTL_SENDSPACE	8 /* send buffer space */
#define	TCPCTL_IDENT	        9 /* get connection owner */
#define	TCPCTL_SACK	       10 /* selective acknowledgement, rfc 2018 */
#define TCPCTL_MSSDFLT	       11 /* Default maximum segment size */
#define	TCPCTL_MAXID	       12

#define	TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323",	CTLTYPE_INT }, \
	{ "keepinittime",	CTLTYPE_INT }, \
	{ "keepidle",	CTLTYPE_INT }, \
	{ "keepintvl",	CTLTYPE_INT }, \
	{ "slowhz",	CTLTYPE_INT }, \
	{ "baddynamic", CTLTYPE_STRUCT }, \
	{ "recvspace",	CTLTYPE_INT }, \
	{ "sendspace",	CTLTYPE_INT }, \
	{ "ident", CTLTYPE_STRUCT }, \
	{ "sack",	CTLTYPE_INT }, \
	{ "mssdflt",	CTLTYPE_INT }, \
}

struct tcp_ident_mapping {
	struct sockaddr faddr, laddr;
	int euid, ruid;
};

#ifdef _KERNEL
struct	inpcbtable tcbtable;	/* head of queue of active tcpcb's */
struct	tcpstat tcpstat;	/* tcp statistics */
u_int32_t tcp_now;		/* for RFC 1323 timestamps */
extern	int tcp_do_rfc1323;	/* enabled/disabled? */
extern	int tcp_mssdflt;	/* default maximum segment size */
#ifdef TCP_SACK
extern	int tcp_do_sack;	/* SACK enabled/disabled */
#endif

int	 tcp_attach __P((struct socket *));
void	 tcp_canceltimers __P((struct tcpcb *));
struct tcpcb *
	 tcp_close __P((struct tcpcb *));
void	 *tcp_ctlinput __P((int, struct sockaddr *, void *));
int	 tcp_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
struct tcpcb *
	 tcp_disconnect __P((struct tcpcb *));
struct tcpcb *
	 tcp_drop __P((struct tcpcb *, int));
void	 tcp_dooptions __P((struct tcpcb *,
	    u_char *, int, struct tcphdr *, int *, u_int32_t *, u_int32_t *));
void	 tcp_drain __P((void));
void	 tcp_fasttimo __P((void));
void	 tcp_init __P((void));
void	 tcp_input __P((struct mbuf *, ...));
int	 tcp_mss __P((struct tcpcb *, u_int));
struct tcpcb *
	 tcp_newtcpcb __P((struct inpcb *));
void	 tcp_notify __P((struct inpcb *, int));
int	 tcp_output __P((struct tcpcb *));
void	 tcp_pulloutofband __P((struct socket *, u_int, struct mbuf *));
void	 tcp_quench __P((struct inpcb *, int));
int	 tcp_reass __P((struct tcpcb *, struct tcphdr *, struct mbuf *, int *));
void	 tcp_respond __P((struct tcpcb *,
	    struct tcpiphdr *, struct mbuf *, tcp_seq, tcp_seq, int));
void	 tcp_setpersist __P((struct tcpcb *));
void	 tcp_slowtimo __P((void));
struct tcpiphdr *
	 tcp_template __P((struct tcpcb *));
struct tcpcb *
	 tcp_timers __P((struct tcpcb *, int));
void	 tcp_trace __P((int, int, struct tcpcb *, struct tcpiphdr *, int, int));
struct tcpcb *
	 tcp_usrclosed __P((struct tcpcb *));
int	 tcp_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
int	 tcp_usrreq __P((struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *));
void	 tcp_xmit_timer __P((struct tcpcb *, int));
void	 tcpdropoldhalfopen __P((struct tcpcb *, u_int16_t));
#ifdef TCP_SACK
int      tcp_sack_option __P((struct tcpcb *,struct tcpiphdr *,u_char *,int));
void     tcp_update_sack_list __P((struct tcpcb *tp));
void     tcp_del_sackholes __P((struct tcpcb *, struct tcpiphdr *));
void     tcp_clean_sackreport __P((struct tcpcb *tp));
void     tcp_sack_adjust __P((struct tcpcb *tp));
struct sackhole * tcp_sack_output __P((struct tcpcb *tp));
int    	 tcp_sack_partialack __P((struct tcpcb *, struct tcpiphdr *));
#ifdef DEBUG
void     tcp_print_holes __P((struct tcpcb *tp));
#endif
#endif /* TCP_SACK */
#if defined(TCP_NEWRENO) || defined(TCP_SACK)
int	 tcp_newreno __P((struct tcpcb *, struct tcpiphdr *));
u_long	 tcp_seq_subtract  __P((u_long, u_long )); 
#endif /* TCP_NEWRENO || TCP_SACK */

#endif /* _KERNEL */
