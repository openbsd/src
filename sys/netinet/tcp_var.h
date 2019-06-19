/*	$OpenBSD: tcp_var.h,v 1.133 2018/06/11 07:40:26 bluhm Exp $	*/
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
 *	@(#)tcp_var.h	8.3 (Berkeley) 4/10/94
 */

#ifndef _NETINET_TCP_VAR_H_
#define _NETINET_TCP_VAR_H_

#include <sys/timeout.h>

/*
 * Kernel variables for tcp.
 */

struct sackblk {
	tcp_seq start;		/* start seq no. of sack block */
	tcp_seq end; 		/* end seq no. */
};

struct sackhole {
	tcp_seq start;		/* start seq no. of hole */
	tcp_seq end;		/* end seq no. */
	int	dups;		/* number of dup(s)acks for this hole */
	tcp_seq rxmit;		/* next seq. no in hole to be retransmitted */
	struct sackhole *next;	/* next in list */
};

/*
 * TCP sequence queue structures.
 */
TAILQ_HEAD(tcpqehead, tcpqent);
struct tcpqent {
	TAILQ_ENTRY(tcpqent) tcpqe_q;
	struct tcphdr	*tcpqe_tcp;
	struct mbuf	*tcpqe_m;	/* mbuf contains packet */
};

/*
 * Tcp control block, one per tcp; fields:
 */
struct tcpcb {
	struct tcpqehead t_segq;		/* sequencing queue */
	struct timeout t_timer[TCPT_NTIMERS];	/* tcp timers */
	short	t_state;		/* state of this connection */
	short	t_rxtshift;		/* log(2) of rexmt exp. backoff */
	short	t_rxtcur;		/* current retransmit value */
	short	t_dupacks;		/* consecutive dup acks recd */
	u_short	t_maxseg;		/* maximum segment size */
	char	t_force;		/* 1 if forcing out a byte */
	u_int	t_flags;
#define	TF_ACKNOW	0x0001		/* ack peer immediately */
#define	TF_NODELAY	0x0004		/* don't delay packets to coalesce */
#define	TF_NOOPT	0x0008		/* don't use tcp options */
#define	TF_SENTFIN	0x0010		/* have sent FIN */
#define	TF_REQ_SCALE	0x0020		/* have/will request window scaling */
#define	TF_RCVD_SCALE	0x0040		/* other side has requested scaling */
#define	TF_REQ_TSTMP	0x0080		/* have/will request timestamps */
#define	TF_RCVD_TSTMP	0x0100		/* a timestamp was received in SYN */
#define	TF_SACK_PERMIT	0x0200		/* other side said I could SACK */
#define	TF_SIGNATURE	0x0400		/* require TCP MD5 signature */
#ifdef TCP_ECN
#define TF_ECN_PERMIT	0x00008000	/* other side said I could ECN */
#define TF_RCVD_CE	0x00010000	/* send ECE in subsequent segs */
#define TF_SEND_CWR	0x00020000	/* send CWR in next seg */
#define TF_DISABLE_ECN	0x00040000	/* disable ECN for this connection */
#endif
#define TF_LASTIDLE	0x00100000	/* no outstanding ACK on last send */
#define TF_PMTUD_PEND	0x00400000	/* Path MTU Discovery pending */
#define TF_NEEDOUTPUT	0x00800000	/* call tcp_output after tcp_input */
#define TF_BLOCKOUTPUT	0x01000000	/* avert tcp_output during tcp_input */
#define TF_NOPUSH	0x02000000	/* don't push */
#define TF_TMR_REXMT	0x04000000	/* retransmit timer armed */
#define TF_TMR_PERSIST	0x08000000	/* retransmit persistence timer armed */
#define TF_TMR_KEEP	0x10000000	/* keep alive timer armed */
#define TF_TMR_2MSL	0x20000000	/* 2*msl quiet time timer armed */
#define TF_TMR_REAPER	0x40000000	/* delayed cleanup timer armed, dead */
#define TF_TMR_DELACK	0x80000000	/* delayed ack timer armed */
#define TF_TIMER	TF_TMR_REXMT	/* used to shift with TCPT values */

	struct	mbuf *t_template;	/* skeletal packet for transmit */
	struct	inpcb *t_inpcb;		/* back pointer to internet pcb */
/*
 * The following fields are used as in the protocol specification.
 * See RFC793, Dec. 1981, page 21.
 */
/* send sequence variables */
	tcp_seq	snd_una;		/* send unacknowledged */
	tcp_seq	snd_nxt;		/* send next */
	tcp_seq	snd_up;			/* send urgent pointer */
	tcp_seq	snd_wl1;		/* window update seg seq number */
	tcp_seq	snd_wl2;		/* window update seg ack number */
	tcp_seq	iss;			/* initial send sequence number */
	u_long	snd_wnd;		/* send window */
	int	sack_enable;		/* enable SACK for this connection */
	int	snd_numholes;		/* number of holes seen by sender */
	struct sackhole *snd_holes;	/* linked list of holes (sorted) */
	tcp_seq snd_last;		/* for use in fast recovery */
/* receive sequence variables */
	u_long	rcv_wnd;		/* receive window */
	tcp_seq	rcv_nxt;		/* receive next */
	tcp_seq	rcv_up;			/* receive urgent pointer */
	tcp_seq	irs;			/* initial receive sequence number */
	tcp_seq rcv_lastsack;		/* last seq number(+1) sack'd by rcv'r*/
	int	rcv_numsacks;		/* # distinct sack blks present */
	struct sackblk sackblks[MAX_SACK_BLKS]; /* seq nos. of sack blocks */

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
	u_long	snd_ssthresh;		/* snd_cwnd size threshold for
					 * for slow start exponential to
					 * linear switch
					 */

/* auto-sizing variables */
	u_int	rfbuf_cnt;	/* recv buffer autoscaling byte count */
	u_int32_t rfbuf_ts;	/* recv buffer autoscaling time stamp */

	u_short	t_maxopd;		/* mss plus options */
	u_short	t_peermss;		/* peer's maximum segment size */

/*
 * transmit timing stuff.  See below for scale of srtt and rttvar.
 * "Variance" is actually smoothed difference.
 */
	uint32_t t_rcvtime;		/* time last segment received */
	uint32_t t_rtttime;		/* time we started measuring rtt */
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
	u_int32_t ts_modulate;		/* modulation on timestamp */
	u_int32_t ts_recent_age;		/* when last updated */
	tcp_seq	last_ack_sent;

/* pointer for syn cache entries*/
	LIST_HEAD(, syn_cache) t_sc;	/* list of entries by this tcb */

/* Path-MTU Discovery Information */
	u_int	t_pmtud_mss_acked;	/* MSS acked, lower bound for MTU */
	u_int	t_pmtud_mtu_sent;	/* MTU used, upper bound for MTU */
	tcp_seq	t_pmtud_th_seq;		/* TCP SEQ from ICMP payload */
	u_int	t_pmtud_nextmtu;	/* Advertised Next-Hop MTU from ICMP */
	u_short	t_pmtud_ip_len;		/* IP length from ICMP payload */
	u_short	t_pmtud_ip_hl;		/* IP header length from ICMP payload */

	int pf;
};

#define	intotcpcb(ip)	((struct tcpcb *)(ip)->inp_ppcb)
#define	sototcpcb(so)	(intotcpcb(sotoinpcb(so)))

#ifdef _KERNEL
/*
 * Handy way of passing around TCP option info.
 */
struct tcp_opt_info {
	int		ts_present;
	u_int32_t	ts_val;
	u_int32_t	ts_ecr;
	u_int16_t	maxseg;
};

/*
 * Data for the TCP compressed state engine.
 */

#define	TCP_SYN_HASH_SIZE	293
#define	TCP_SYN_BUCKET_SIZE	35

union syn_cache_sa {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
};

struct syn_cache {
	TAILQ_ENTRY(syn_cache) sc_bucketq;	/* link on bucket list */
	struct timeout sc_timer;		/* rexmt timer */
	union {					/* cached route */
		struct route route4;
#ifdef INET6
		struct route_in6 route6;
#endif
	} sc_route_u;
#define sc_route4	sc_route_u.route4
#ifdef INET6
#define sc_route6	sc_route_u.route6
#endif
	long sc_win;				/* advertised window */
	struct syn_cache_head *sc_buckethead;	/* our bucket index */
	struct syn_cache_set *sc_set;		/* our syn cache set */
	u_int32_t sc_hash;
	u_int32_t sc_timestamp;			/* timestamp from SYN */
	u_int32_t sc_modulate;			/* our timestamp modulator */
#if 0
	u_int32_t sc_timebase;			/* our local timebase */
#endif
	union syn_cache_sa sc_src;
	union syn_cache_sa sc_dst;
	tcp_seq sc_irs;
	tcp_seq sc_iss;
	u_int sc_rtableid;
	u_int sc_rxtcur;			/* current rxt timeout */
	u_int sc_rxttot;			/* total time spend on queues */
	u_short sc_rxtshift;			/* for computing backoff */
	u_short sc_flags;

#define	SCF_UNREACH		0x0001		/* we've had an unreach error */
#define	SCF_TIMESTAMP		0x0002		/* peer will do timestamps */
#define	SCF_DEAD		0x0004		/* this entry to be released */
#define	SCF_SACK_PERMIT		0x0008		/* permit sack */
#define	SCF_ECN_PERMIT		0x0010		/* permit ecn */
#define	SCF_SIGNATURE		0x0020		/* enforce tcp signatures */

	struct mbuf *sc_ipopts;			/* IP options */
	u_int16_t sc_peermaxseg;
	u_int16_t sc_ourmaxseg;
	u_int     sc_request_r_scale	: 4,
		  sc_requested_s_scale	: 4;

	struct tcpcb *sc_tp;			/* tcb for listening socket */
	LIST_ENTRY(syn_cache) sc_tpq;		/* list of entries by same tp */
};

struct syn_cache_head {
	TAILQ_HEAD(, syn_cache) sch_bucket;	/* bucket entries */
	u_short sch_length;			/* # entries in bucket */
};

struct syn_cache_set {
	struct		syn_cache_head *scs_buckethead;
	int		scs_size;
	int		scs_count;
	int		scs_use;
	u_int32_t	scs_random[5];
};

#endif /* _KERNEL */

/*
 * The smoothed round-trip time and estimated variance
 * are stored as fixed point numbers scaled by the values below.
 * For convenience, these scales are also used in smoothing the average
 * (smoothed = (1/scale)sample + ((scale-1)/scale)smoothed).
 * With these scales, srtt has 5 bits to the right of the binary point,
 * and thus an "ALPHA" of 0.875.  rttvar has 4 bits to the right of the
 * binary point, and is smoothed with an ALPHA of 0.75.
 */
#define	TCP_RTT_SHIFT		3	/* shift for srtt; 5 bits frac. */
#define	TCP_RTTVAR_SHIFT	2	/* shift for rttvar; 4 bits */
#define	TCP_RTT_BASE_SHIFT	2	/* remaining 2 bit shift */
#define	TCP_RTT_MAX		(1<<9)	/* maximum rtt */

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
 * This macro assumes that the value of (1 << TCP_RTTVAR_SHIFT)
 * is the same as the multiplier for rttvar.
 */
#define	TCP_REXMTVAL(tp) \
	((((tp)->t_srtt >> TCP_RTT_SHIFT) + (tp)->t_rttvar) >> TCP_RTT_BASE_SHIFT)

/*
 * TCP statistics.
 * Many of these should be kept per connection,
 * but that's inconvenient at the moment.
 */
struct	tcpstat {
	u_int32_t tcps_connattempt;	/* connections initiated */
	u_int32_t tcps_accepts;		/* connections accepted */
	u_int32_t tcps_connects;	/* connections established */
	u_int32_t tcps_drops;		/* connections dropped */
	u_int32_t tcps_conndrops;	/* embryonic connections dropped */
	u_int32_t tcps_closed;		/* conn. closed (includes drops) */
	u_int32_t tcps_segstimed;	/* segs where we tried to get rtt */
	u_int32_t tcps_rttupdated;	/* times we succeeded */
	u_int32_t tcps_delack;		/* delayed acks sent */
	u_int32_t tcps_timeoutdrop;	/* conn. dropped in rxmt timeout */
	u_int32_t tcps_rexmttimeo;	/* retransmit timeouts */
	u_int32_t tcps_persisttimeo;	/* persist timeouts */
	u_int32_t tcps_persistdrop;	/* connections dropped in persist */
	u_int32_t tcps_keeptimeo;	/* keepalive timeouts */
	u_int32_t tcps_keepprobe;	/* keepalive probes sent */
	u_int32_t tcps_keepdrops;	/* connections dropped in keepalive */

	u_int32_t tcps_sndtotal;		/* total packets sent */
	u_int32_t tcps_sndpack;		/* data packets sent */
	u_int64_t tcps_sndbyte;		/* data bytes sent */
	u_int32_t tcps_sndrexmitpack;	/* data packets retransmitted */
	u_int64_t tcps_sndrexmitbyte;	/* data bytes retransmitted */
	u_int64_t tcps_sndrexmitfast;	/* Fast retransmits */
	u_int32_t tcps_sndacks;		/* ack-only packets sent */
	u_int32_t tcps_sndprobe;	/* window probes sent */
	u_int32_t tcps_sndurg;		/* packets sent with URG only */
	u_int32_t tcps_sndwinup;	/* window update-only packets sent */
	u_int32_t tcps_sndctrl;		/* control (SYN|FIN|RST) packets sent */

	u_int32_t tcps_rcvtotal;	/* total packets received */
	u_int32_t tcps_rcvpack;		/* packets received in sequence */
	u_int64_t tcps_rcvbyte;		/* bytes received in sequence */
	u_int32_t tcps_rcvbadsum;	/* packets received with ccksum errs */
	u_int32_t tcps_rcvbadoff;	/* packets received with bad offset */
	u_int32_t tcps_rcvmemdrop;	/* packets dropped for lack of memory */
	u_int32_t tcps_rcvnosec;	/* packets dropped for lack of ipsec */
	u_int32_t tcps_rcvshort;	/* packets received too short */
	u_int32_t tcps_rcvduppack;	/* duplicate-only packets received */
	u_int64_t tcps_rcvdupbyte;	/* duplicate-only bytes received */
	u_int32_t tcps_rcvpartduppack;	/* packets with some duplicate data */
	u_int64_t tcps_rcvpartdupbyte;	/* dup. bytes in part-dup. packets */
	u_int32_t tcps_rcvoopack;	/* out-of-order packets received */
	u_int64_t tcps_rcvoobyte;	/* out-of-order bytes received */
	u_int32_t tcps_rcvpackafterwin;	/* packets with data after window */
	u_int64_t tcps_rcvbyteafterwin;	/* bytes rcvd after window */
	u_int32_t tcps_rcvafterclose;	/* packets rcvd after "close" */
	u_int32_t tcps_rcvwinprobe;	/* rcvd window probe packets */
	u_int32_t tcps_rcvdupack;	/* rcvd duplicate acks */
	u_int32_t tcps_rcvacktoomuch;	/* rcvd acks for unsent data */
	u_int32_t tcps_rcvacktooold;	/* rcvd acks for old data */
	u_int32_t tcps_rcvackpack;	/* rcvd ack packets */
	u_int64_t tcps_rcvackbyte;	/* bytes acked by rcvd acks */
	u_int32_t tcps_rcvwinupd;	/* rcvd window update packets */
	u_int32_t tcps_pawsdrop;	/* segments dropped due to PAWS */
	u_int32_t tcps_predack;		/* times hdr predict ok for acks */
	u_int32_t tcps_preddat;		/* times hdr predict ok for data pkts */

	u_int32_t tcps_pcbhashmiss;	/* input packets missing pcb hash */
	u_int32_t tcps_noport;		/* no socket on port */
	u_int32_t tcps_badsyn;		/* SYN packet with src==dst rcv'ed */
	u_int32_t tcps_dropsyn;		/* SYN packet dropped */

	u_int32_t tcps_rcvbadsig;	/* rcvd bad/missing TCP signatures */
	u_int64_t tcps_rcvgoodsig;	/* rcvd good TCP signatures */
	u_int32_t tcps_inswcsum;	/* input software-checksummed packets */
	u_int32_t tcps_outswcsum;	/* output software-checksummed packets */

	/* ECN stats */
	u_int32_t tcps_ecn_accepts;	/* ecn connections accepted */
	u_int32_t tcps_ecn_rcvece;	/* # of rcvd ece */
	u_int32_t tcps_ecn_rcvcwr;	/* # of rcvd cwr */
	u_int32_t tcps_ecn_rcvce;	/* # of rcvd ce in ip header */
	u_int32_t tcps_ecn_sndect;	/* # of cwr sent */
	u_int32_t tcps_ecn_sndece;	/* # of ece sent */
	u_int32_t tcps_ecn_sndcwr;	/* # of cwr sent */
	u_int32_t tcps_cwr_ecn;		/* # of cwnd reduced by ecn */
	u_int32_t tcps_cwr_frecovery;	/* # of cwnd reduced by fastrecovery */
	u_int32_t tcps_cwr_timeout;	/* # of cwnd reduced by timeout */

	/* These statistics deal with the SYN cache. */
	u_int64_t tcps_sc_added;	/* # of entries added */
	u_int64_t tcps_sc_completed;	/* # of connections completed */
	u_int64_t tcps_sc_timed_out;	/* # of entries timed out */
	u_int64_t tcps_sc_overflowed;	/* # dropped due to overflow */
	u_int64_t tcps_sc_reset;	/* # dropped due to RST */
	u_int64_t tcps_sc_unreach;	/* # dropped due to ICMP unreach */
	u_int64_t tcps_sc_bucketoverflow;/* # dropped due to bucket overflow */
	u_int64_t tcps_sc_aborted;	/* # of entries aborted (no mem) */
	u_int64_t tcps_sc_dupesyn;	/* # of duplicate SYNs received */
	u_int64_t tcps_sc_dropped;	/* # of SYNs dropped (no route/mem) */
	u_int64_t tcps_sc_collisions;	/* # of hash collisions */
	u_int64_t tcps_sc_retransmitted;/* # of retransmissions */
	u_int64_t tcps_sc_seedrandom;	/* # of syn cache seeds with random */
	u_int64_t tcps_sc_hash_size;	/* hash buckets in current syn cache */
	u_int64_t tcps_sc_entry_count;	/* # of entries in current syn cache */
	u_int64_t tcps_sc_entry_limit;	/* limit of syn cache entries */
	u_int64_t tcps_sc_bucket_maxlen;/* maximum # of entries in any bucket */
	u_int64_t tcps_sc_bucket_limit;	/* limit of syn cache bucket list */
	u_int64_t tcps_sc_uses_left;	/* use counter of current syn cache */

	u_int64_t tcps_conndrained;	/* # of connections drained */

	u_int64_t tcps_sack_recovery_episode;	/* SACK recovery episodes */
	u_int64_t tcps_sack_rexmits;		/* SACK rexmit segments */
	u_int64_t tcps_sack_rexmit_bytes;	/* SACK rexmit bytes */
	u_int64_t tcps_sack_rcv_opts;		/* SACK options received */
	u_int64_t tcps_sack_snd_opts;		/* SACK options sent */
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
#define	TCPCTL_IDENT		9 /* get connection owner */
#define	TCPCTL_SACK	       10 /* selective acknowledgement, rfc 2018 */
#define TCPCTL_MSSDFLT	       11 /* Default maximum segment size */
#define	TCPCTL_RSTPPSLIMIT     12 /* RST pps limit */
#define	TCPCTL_ACK_ON_PUSH     13 /* ACK immediately on PUSH */
#define	TCPCTL_ECN	       14 /* RFC3168 ECN */
#define	TCPCTL_SYN_CACHE_LIMIT 15 /* max size of comp. state engine */
#define	TCPCTL_SYN_BUCKET_LIMIT	16 /* max size of hash bucket */
#define	TCPCTL_RFC3390	       17 /* enable/disable RFC3390 increased cwnd */
#define	TCPCTL_REASS_LIMIT     18 /* max entries for tcp reass queues */
#define	TCPCTL_DROP	       19 /* drop tcp connection */
#define	TCPCTL_SACKHOLE_LIMIT  20 /* max entries for tcp sack queues */
#define	TCPCTL_STATS	       21 /* TCP statistics */
#define	TCPCTL_ALWAYS_KEEPALIVE 22 /* assume SO_KEEPALIVE is always set */
#define	TCPCTL_SYN_USE_LIMIT   23 /* number of uses before reseeding hash */
#define TCPCTL_ROOTONLY	       24 /* return root only port bitmap */
#define	TCPCTL_SYN_HASH_SIZE   25 /* number of buckets in the hash */
#define	TCPCTL_MAXID	       26

#define	TCPCTL_NAMES { \
	{ 0, 0 }, \
	{ "rfc1323",	CTLTYPE_INT }, \
	{ "keepinittime",	CTLTYPE_INT }, \
	{ "keepidle",	CTLTYPE_INT }, \
	{ "keepintvl",	CTLTYPE_INT }, \
	{ "slowhz",	CTLTYPE_INT }, \
	{ "baddynamic", CTLTYPE_STRUCT }, \
	{ NULL,	0 }, \
	{ NULL,	0 }, \
	{ "ident", 	CTLTYPE_STRUCT }, \
	{ "sack",	CTLTYPE_INT }, \
	{ "mssdflt",	CTLTYPE_INT }, \
	{ "rstppslimit",	CTLTYPE_INT }, \
	{ "ackonpush",	CTLTYPE_INT }, \
	{ "ecn", 	CTLTYPE_INT }, \
	{ "syncachelimit", 	CTLTYPE_INT }, \
	{ "synbucketlimit", 	CTLTYPE_INT }, \
	{ "rfc3390", 	CTLTYPE_INT }, \
	{ "reasslimit", 	CTLTYPE_INT }, \
	{ "drop", 	CTLTYPE_STRUCT }, \
	{ "sackholelimit", 	CTLTYPE_INT }, \
	{ "stats",	CTLTYPE_STRUCT }, \
	{ "always_keepalive",	CTLTYPE_INT }, \
	{ "synuselimit", 	CTLTYPE_INT }, \
	{ "rootonly", CTLTYPE_STRUCT }, \
	{ "synhashsize", 	CTLTYPE_INT }, \
}

#define	TCPCTL_VARS { \
	NULL, \
	&tcp_do_rfc1323, \
	&tcptv_keep_init, \
	&tcp_keepidle, \
	&tcp_keepintvl, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	&tcp_mssdflt, \
	&tcp_rst_ppslim, \
	&tcp_ack_on_push, \
	NULL, \
	&tcp_syn_cache_limit, \
	&tcp_syn_bucket_limit, \
	&tcp_do_rfc3390, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL, \
	NULL \
}

struct tcp_ident_mapping {
	struct sockaddr_storage faddr, laddr;
	int euid, ruid;
	u_int rdomain;
};

#ifdef _KERNEL

#include <sys/percpu.h>

enum tcpstat_counters {
	tcps_connattempt,
	tcps_accepts,
	tcps_connects,
	tcps_drops,
	tcps_conndrops,
	tcps_closed,
	tcps_segstimed,
	tcps_rttupdated,
	tcps_delack,
	tcps_timeoutdrop,
	tcps_rexmttimeo,
	tcps_persisttimeo,
	tcps_persistdrop,
	tcps_keeptimeo,
	tcps_keepprobe,
	tcps_keepdrops,
	tcps_sndtotal,
	tcps_sndpack,
	tcps_sndbyte,
	tcps_sndrexmitpack,
	tcps_sndrexmitbyte,
	tcps_sndrexmitfast,
	tcps_sndacks,
	tcps_sndprobe,
	tcps_sndurg,
	tcps_sndwinup,
	tcps_sndctrl,
	tcps_rcvtotal,
	tcps_rcvpack,
	tcps_rcvbyte,
	tcps_rcvbadsum,
	tcps_rcvbadoff,
	tcps_rcvmemdrop,
	tcps_rcvnosec,
	tcps_rcvshort,
	tcps_rcvduppack,
	tcps_rcvdupbyte,
	tcps_rcvpartduppack,
	tcps_rcvpartdupbyte,
	tcps_rcvoopack,
	tcps_rcvoobyte,
	tcps_rcvpackafterwin,
	tcps_rcvbyteafterwin,
	tcps_rcvafterclose,
	tcps_rcvwinprobe,
	tcps_rcvdupack,
	tcps_rcvacktoomuch,
	tcps_rcvacktooold,
	tcps_rcvackpack,
	tcps_rcvackbyte,
	tcps_rcvwinupd,
	tcps_pawsdrop,
	tcps_predack,
	tcps_preddat,
	tcps_pcbhashmiss,
	tcps_noport,
	tcps_badsyn,
	tcps_dropsyn,
	tcps_rcvbadsig,
	tcps_rcvgoodsig,
	tcps_inswcsum,
	tcps_outswcsum,
	tcps_ecn_accepts,
	tcps_ecn_rcvece,
	tcps_ecn_rcvcwr,
	tcps_ecn_rcvce,
	tcps_ecn_sndect,
	tcps_ecn_sndece,
	tcps_ecn_sndcwr,
	tcps_cwr_ecn,
	tcps_cwr_frecovery,
	tcps_cwr_timeout,
	tcps_sc_added,
	tcps_sc_completed,
	tcps_sc_timed_out,
	tcps_sc_overflowed,
	tcps_sc_reset,
	tcps_sc_unreach,
	tcps_sc_bucketoverflow,
	tcps_sc_aborted,
	tcps_sc_dupesyn,
	tcps_sc_dropped,
	tcps_sc_collisions,
	tcps_sc_retransmitted,
	tcps_sc_seedrandom,
	tcps_sc_hash_size,
	tcps_sc_entry_count,
	tcps_sc_entry_limit,
	tcps_sc_bucket_maxlen,
	tcps_sc_bucket_limit,
	tcps_sc_uses_left,
	tcps_conndrained,
	tcps_sack_recovery_episode,
	tcps_sack_rexmits,
	tcps_sack_rexmit_bytes,
	tcps_sack_rcv_opts,
	tcps_sack_snd_opts,
	tcps_ncounters,
};

extern struct cpumem *tcpcounters;

static inline void
tcpstat_inc(enum tcpstat_counters c)
{
	counters_inc(tcpcounters, c);
}

static inline void
tcpstat_add(enum tcpstat_counters c, uint64_t v)
{
	counters_add(tcpcounters, c, v);
}

static inline void
tcpstat_pkt(enum tcpstat_counters pcounter, enum tcpstat_counters bcounter,
    uint64_t v)
{
	counters_pkt(tcpcounters, pcounter, bcounter, v);
}

extern	struct pool tcpcb_pool;
extern	struct inpcbtable tcbtable;	/* head of queue of active tcpcb's */
extern	u_int32_t tcp_now;		/* for RFC 1323 timestamps */
extern	int tcp_do_rfc1323;	/* enabled/disabled? */
extern	int tcptv_keep_init;	/* time to keep alive the initial SYN packet */
extern	int tcp_mssdflt;	/* default maximum segment size */
extern	int tcp_rst_ppslim;	/* maximum outgoing RST packet per second */
extern	int tcp_ack_on_push;	/* ACK immediately on PUSH */
extern	int tcp_do_sack;	/* SACK enabled/disabled */
extern	struct pool sackhl_pool;
extern	int tcp_sackhole_limit;	/* max entries for tcp sack queues */
extern	int tcp_do_ecn;		/* RFC3168 ECN enabled/disabled? */
extern	int tcp_do_rfc3390;	/* RFC3390 Increasing TCP's Initial Window */

extern	struct pool tcpqe_pool;
extern	int tcp_reass_limit;	/* max entries for tcp reass queues */

extern	int tcp_syn_hash_size;  /* adjustable size of the hash array */
extern	int tcp_syn_cache_limit; /* max entries for compressed state engine */
extern	int tcp_syn_bucket_limit;/* max entries per hash bucket */
extern	int tcp_syn_use_limit;   /* number of uses before reseeding hash */
extern	struct syn_cache_set tcp_syn_cache[];
extern	int tcp_syn_cache_active; /* active syn cache, may be 0 or 1 */

void	 tcp_canceltimers(struct tcpcb *);
struct tcpcb *
	 tcp_close(struct tcpcb *);
int	 tcp_freeq(struct tcpcb *);
#ifdef INET6
void	 tcp6_ctlinput(int, struct sockaddr *, u_int, void *);
#endif
void	 tcp_ctlinput(int, struct sockaddr *, u_int, void *);
int	 tcp_ctloutput(int, struct socket *, int, int, struct mbuf *);
struct tcpcb *
	 tcp_disconnect(struct tcpcb *);
struct tcpcb *
	 tcp_drop(struct tcpcb *, int);
int	 tcp_dooptions(struct tcpcb *, u_char *, int, struct tcphdr *,
		struct mbuf *, int, struct tcp_opt_info *, u_int);
void	 tcp_init(void);
int	 tcp_input(struct mbuf **, int *, int, int);
int	 tcp_mss(struct tcpcb *, int);
void	 tcp_mss_update(struct tcpcb *);
u_int	 tcp_hdrsz(struct tcpcb *);
void	 tcp_mtudisc(struct inpcb *, int);
void	 tcp_mtudisc_increase(struct inpcb *, int);
#ifdef INET6
void	tcp6_mtudisc(struct inpcb *, int);
void	tcp6_mtudisc_callback(struct sockaddr_in6 *, u_int);
#endif
struct tcpcb *
	 tcp_newtcpcb(struct inpcb *);
void	 tcp_notify(struct inpcb *, int);
int	 tcp_output(struct tcpcb *);
void	 tcp_pulloutofband(struct socket *, u_int, struct mbuf *, int);
int	 tcp_reass(struct tcpcb *, struct tcphdr *, struct mbuf *, int *);
void	 tcp_rscale(struct tcpcb *, u_long);
void	 tcp_respond(struct tcpcb *, caddr_t, struct tcphdr *, tcp_seq,
		tcp_seq, int, u_int);
void	 tcp_setpersist(struct tcpcb *);
void	 tcp_update_sndspace(struct tcpcb *);
void	 tcp_update_rcvspace(struct tcpcb *);
void	 tcp_slowtimo(void);
struct mbuf *
	 tcp_template(struct tcpcb *);
void	 tcp_trace(short, short, struct tcpcb *, struct tcpcb *, caddr_t,
		int, int);
struct tcpcb *
	 tcp_usrclosed(struct tcpcb *);
int	 tcp_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int	 tcp_usrreq(struct socket *,
	    int, struct mbuf *, struct mbuf *, struct mbuf *, struct proc *);
int	 tcp_attach(struct socket *, int);
int	 tcp_detach(struct socket *);
void	 tcp_xmit_timer(struct tcpcb *, int);
void	 tcpdropoldhalfopen(struct tcpcb *, u_int16_t);
void	 tcp_sack_option(struct tcpcb *,struct tcphdr *,u_char *,int);
void	 tcp_update_sack_list(struct tcpcb *tp, tcp_seq, tcp_seq);
void	 tcp_del_sackholes(struct tcpcb *, struct tcphdr *);
void	 tcp_clean_sackreport(struct tcpcb *tp);
void	 tcp_sack_adjust(struct tcpcb *tp);
struct sackhole *
	 tcp_sack_output(struct tcpcb *tp);
#ifdef DEBUG
void	 tcp_print_holes(struct tcpcb *tp);
#endif
u_long	 tcp_seq_subtract(u_long, u_long );
#ifdef TCP_SIGNATURE
int	tcp_signature_apply(caddr_t, caddr_t, unsigned int);
int	tcp_signature(struct tdb *, int, struct mbuf *, struct tcphdr *,
	    int, int, char *);
#endif /* TCP_SIGNATURE */
void     tcp_set_iss_tsm(struct tcpcb *);

void	 syn_cache_unreach(struct sockaddr *, struct sockaddr *,
	   struct tcphdr *, u_int);
void	 syn_cache_init(void);
void	 syn_cache_cleanup(struct tcpcb *);

#endif /* _KERNEL */
#endif /* _NETINET_TCP_VAR_H_ */
