/*	$OpenBSD: pfvar.h,v 1.103 2002/11/23 09:37:02 deraadt Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _NET_PFVAR_H_
#define _NET_PFVAR_H_

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>

enum	{ PF_IN=0, PF_OUT=1 };
enum	{ PF_PASS=0, PF_DROP=1, PF_SCRUB=2 };
enum	{ PF_OP_IRG=1, PF_OP_EQ=2, PF_OP_NE=3, PF_OP_LT=4,
	  PF_OP_LE=5, PF_OP_GT=6, PF_OP_GE=7, PF_OP_XRG=8 };
enum	{ PF_DEBUG_NONE=0, PF_DEBUG_URGENT=1, PF_DEBUG_MISC=2 };
enum	{ PF_CHANGE_ADD_HEAD=1, PF_CHANGE_ADD_TAIL=2,
	  PF_CHANGE_ADD_BEFORE=3, PF_CHANGE_ADD_AFTER=4,
	  PF_CHANGE_REMOVE=5 };
enum	{ PFTM_TCP_FIRST_PACKET=0, PFTM_TCP_OPENING=1, PFTM_TCP_ESTABLISHED=2,
	  PFTM_TCP_CLOSING=3, PFTM_TCP_FIN_WAIT=4, PFTM_TCP_CLOSED=5,
	  PFTM_UDP_FIRST_PACKET=6, PFTM_UDP_SINGLE=7, PFTM_UDP_MULTIPLE=8,
	  PFTM_ICMP_FIRST_PACKET=9, PFTM_ICMP_ERROR_REPLY=10,
	  PFTM_OTHER_FIRST_PACKET=11, PFTM_OTHER_SINGLE=12,
	  PFTM_OTHER_MULTIPLE=13, PFTM_FRAG=14, PFTM_INTERVAL=15, PFTM_MAX=16 };
enum	{ PF_FASTROUTE=1, PF_ROUTETO=2, PF_DUPTO=3, PF_REPLYTO=4 };
enum	{ PF_LIMIT_STATES=0, PF_LIMIT_FRAGS=1, PF_LIMIT_MAX=2 };
enum    { PF_POOL_RULE_RT=0, PF_POOL_NAT_R=1, PF_POOL_RDR_R=2 };
#define PF_POOL_IDMASK		0x0f
#define PF_POOL_LAST		0x10
enum	{ PF_POOL_NONE=0, PF_POOL_BITMASK=1, PF_POOL_RANDOM=2,
	  PF_POOL_SRCHASH=3, PF_POOL_SRCKEYHASH=4, PF_POOL_ROUNDROBIN=5 };
#define PF_POOL_TYPEMASK	0x0f
#define PF_POOL_STATICPORT	0x10

struct pf_addr {
	union {
		struct in_addr		v4;
		struct in6_addr		v6;
		u_int8_t		addr8[16];
		u_int16_t		addr16[8];
		u_int32_t		addr32[4];
		char			ifname[IFNAMSIZ];
	} pfa;		    /* 128-bit address */
#define v4	pfa.v4
#define v6	pfa.v6
#define addr8	pfa.addr8
#define addr16	pfa.addr16
#define addr32	pfa.addr32
};

struct pf_addr_wrap {
	struct pf_addr		 addr;
	struct pf_addr		 mask;
	struct pf_addr_dyn	*addr_dyn;
};

struct pf_addr_dyn {
	char			 ifname[IFNAMSIZ];
	struct ifnet		*ifp;
	struct pf_addr		*addr;
	sa_family_t		 af;
	void			*hook_cookie;
	u_int8_t		 undefined;
};

/*
 * Address manipulation macros
 */

#ifdef _KERNEL

#ifdef INET
#ifndef INET6
#define PF_INET_ONLY
#endif /* ! INET6 */
#endif /* INET */

#ifdef INET6
#ifndef INET
#define PF_INET6_ONLY
#endif /* ! INET */
#endif /* INET6 */

#ifdef INET
#ifdef INET6
#define PF_INET_INET6
#endif /* INET6 */
#endif /* INET */

#else

#define PF_INET_INET6

#endif /* _KERNEL */

/* Both IPv4 and IPv6 */
#ifdef PF_INET_INET6

#define PF_AEQ(a, b, c) \
	((c == AF_INET && (a)->addr32[0] == (b)->addr32[0]) || \
	((a)->addr32[0] == (b)->addr32[0] && \
	(a)->addr32[1] == (b)->addr32[1] && \
	(a)->addr32[2] == (b)->addr32[2] && \
	(a)->addr32[3] == (b)->addr32[3])) \

#define PF_ANEQ(a, b, c) \
	((c == AF_INET && (a)->addr32[0] != (b)->addr32[0]) || \
	((a)->addr32[0] != (b)->addr32[0] || \
	(a)->addr32[1] != (b)->addr32[1] || \
	(a)->addr32[2] != (b)->addr32[2] || \
	(a)->addr32[3] != (b)->addr32[3])) \

#define PF_AZERO(a, c) \
	((c == AF_INET && !(a)->addr32[0]) || \
	(!(a)->addr32[0] && !(a)->addr32[1] && \
	!(a)->addr32[2] && !(a)->addr32[3] )) \

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	pf_addrcpy(a, b, f)

#define PF_AINC(a, f) \
	pf_addr_inc(a, f)

#define PF_POOLMASK(a, b, c, d, f) \
	pf_poolmask(a, b, c, d, f)

#else

/* Just IPv6 */

#ifdef PF_INET6_ONLY

#define PF_AEQ(a, b, c) \
	((a)->addr32[0] == (b)->addr32[0] && \
	(a)->addr32[1] == (b)->addr32[1] && \
	(a)->addr32[2] == (b)->addr32[2] && \
	(a)->addr32[3] == (b)->addr32[3]) \

#define PF_ANEQ(a, b, c) \
	((a)->addr32[0] != (b)->addr32[0] || \
	(a)->addr32[1] != (b)->addr32[1] || \
	(a)->addr32[2] != (b)->addr32[2] || \
	(a)->addr32[3] != (b)->addr32[3]) \

#define PF_AZERO(a, c) \
	(!(a)->addr32[0] && \
	!(a)->addr32[1] && \
	!(a)->addr32[2] && \
	!(a)->addr32[3] ) \

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	pf_addrcpy(a, b, f)

#define PF_AINC(a, f) \
	pf_addr_inc(a, f)

#define PF_POOLMASK(a, b, c, d, f) \
	pf_poolmask(a, b, c, d, f)

#else

/* Just IPv4 */
#ifdef PF_INET_ONLY

#define PF_AEQ(a, b, c) \
	((a)->addr32[0] == (b)->addr32[0])

#define PF_ANEQ(a, b, c) \
	((a)->addr32[0] != (b)->addr32[0])

#define PF_AZERO(a, c) \
	(!(a)->addr32[0])

#define PF_MATCHA(n, a, m, b, f) \
	pf_match_addr(n, a, m, b, f)

#define PF_ACPY(a, b, f) \
	(a)->v4.s_addr = (b)->v4.s_addr

#define PF_AINC(a, f) \
	do { \
		(a)->addr32[0] = htonl(ntohl((a)->addr32[0]) + 1); \
	} while (0)

#define PF_POOLMASK(a, b, c, d, f) \
	do { \
		(a)->addr32[0] = ((b)->addr32[0] & (c)->addr32[0]) | \
		(((c)->addr32[0] ^ 0xffffffff ) & (d)->addr32[0]); \
	} while (0)

#endif /* PF_INET_ONLY */
#endif /* PF_INET6_ONLY */
#endif /* PF_INET_INET6 */

struct pf_rule_uid {
	uid_t		 uid[2];
	u_int8_t	 op;
};

struct pf_rule_gid {
	uid_t		 gid[2];
	u_int8_t	 op;
};

struct pf_rule_addr {
	struct pf_addr_wrap	 addr;
	u_int16_t		 port[2];
	u_int8_t		 not;
	u_int8_t		 port_op;
	u_int8_t		 noroute;
};

struct pf_pooladdr {
	struct pf_addr_wrap		 addr;
	TAILQ_ENTRY(pf_pooladdr)	 entries;
	char				 ifname[IFNAMSIZ];
	struct ifnet			*ifp;
};

TAILQ_HEAD(pf_palist, pf_pooladdr);

struct pf_poolhashkey {
	union {
		u_int8_t		key8[16];
		u_int16_t		key16[8];
		u_int32_t		key32[4];
	} pfk;		    /* 128-bit hash key */
#define key8	pfk.key8
#define key16	pfk.key16
#define key32	pfk.key32
};

struct pf_pool {
	struct pf_palist	 list;
	struct pf_pooladdr	*cur;
	struct pf_poolhashkey	 key;
	struct pf_addr		 counter;
	u_int8_t		 opts;
};

struct pf_rule {
	struct pf_rule_addr	 src;
	struct pf_rule_addr	 dst;
#define PF_SKIP_ACTION		0
#define PF_SKIP_IFP		1
#define PF_SKIP_DIR		2
#define PF_SKIP_AF		3
#define PF_SKIP_PROTO		4
#define PF_SKIP_SRC_ADDR	5
#define PF_SKIP_SRC_PORT	6
#define PF_SKIP_DST_ADDR	7
#define PF_SKIP_DST_PORT	8
#define PF_SKIP_COUNT		9
	struct pf_rule		*skip[PF_SKIP_COUNT];
#define PF_RULE_LABEL_SIZE	 64
	char			 label[PF_RULE_LABEL_SIZE];
	u_int32_t		 timeout[PFTM_MAX];
#define PF_QNAME_SIZE		 16
	char			 ifname[IFNAMSIZ];
	char			 qname[PF_QNAME_SIZE];
	TAILQ_ENTRY(pf_rule)	 entries;
	struct pf_pool		 rt_pool;

	u_int64_t		 evaluations;
	u_int64_t		 packets;
	u_int64_t		 bytes;

	struct ifnet		*ifp;

	u_int32_t		 states;
	u_int32_t		 max_states;
	u_int32_t		 qid;
	u_int32_t		 rt_listid;

	u_int16_t		 nr;
	u_int16_t		 return_icmp;
	u_int16_t		 return_icmp6;
	u_int16_t		 max_mss;

	struct pf_rule_uid	 uid;
	struct pf_rule_gid	 gid;

	u_int8_t		 action;
	u_int8_t		 direction;
	u_int8_t		 log;
	u_int8_t		 quick;
	u_int8_t		 ifnot;

#define PF_STATE_NORMAL		0x1
#define PF_STATE_MODULATE	0x2
	u_int8_t		 keep_state;
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 type;
	u_int8_t		 code;

	u_int8_t		 flags;
	u_int8_t		 flagset;
	u_int8_t		 rule_flag;
	u_int8_t		 min_ttl;
	u_int8_t		 allow_opts;
	u_int8_t		 rt;
	u_int8_t		 return_ttl;
	u_int8_t		 tos;
};

#define	PFRULE_DROP		0x00
#define	PFRULE_RETURNRST	0x01
#define	PFRULE_NODF		0x02
#define	PFRULE_FRAGMENT		0x04
#define	PFRULE_RETURNICMP	0x08
#define	PFRULE_FRAGCROP		0x10	/* non-buffering frag cache */
#define	PFRULE_FRAGDROP		0x20	/* drop funny fragments */
#define	PFRULE_RETURN		0x40

struct pf_state_host {
	struct pf_addr	addr;
	u_int16_t	port;
	u_int16_t	pad;
};

struct pf_state_peer {
	u_int32_t	seqlo;		/* Max sequence number sent	*/
	u_int32_t	seqhi;		/* Max the other end ACKd + win	*/
	u_int32_t	seqdiff;	/* Sequence number modulator	*/
	u_int16_t	max_win;
	u_int8_t	state;
	u_int8_t	pad;
};

struct pf_state {
	struct pf_state_host lan;
	struct pf_state_host gwy;
	struct pf_state_host ext;
	struct pf_state_peer src;
	struct pf_state_peer dst;
	union {
		struct pf_rule	*ptr;
		u_int16_t	 nr;
	} rule;
	struct pf_addr	 rt_addr;
	struct ifnet	*rt_ifp;
	u_int32_t	 creation;
	u_int32_t	 expire;
	u_int32_t	 packets;
	u_int32_t	 bytes;
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 direction;
	u_int8_t	 log;
	u_int8_t	 allow_opts;
	u_int8_t	 pad[3];
};

struct pf_tree_node {
	RB_ENTRY(pf_tree_node) entry;
	struct pf_state	*state;
	struct pf_addr	 addr[2];
	u_int16_t	 port[2];
	sa_family_t	 af;
	u_int8_t	 proto;
};


struct pf_nat {
	struct pf_rule_addr	 src;
	struct pf_rule_addr	 dst;
	struct pf_pool		 rpool;
	struct pf_pooladdr	*rcur;
	char			 ifname[IFNAMSIZ];
	struct ifnet		*ifp;
	TAILQ_ENTRY(pf_nat)	 entries;
	u_int32_t		 rlistid;
	u_int16_t		 proxy_port[2];
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 ifnot;
	u_int8_t		 no;
};

struct pf_binat {
	char			 ifname[IFNAMSIZ];
	struct ifnet		*ifp;
	TAILQ_ENTRY(pf_binat)	 entries;
	struct pf_addr_wrap	 saddr;
	struct pf_addr_wrap	 daddr;
	struct pf_addr_wrap	 raddr;
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 dnot;
	u_int8_t		 no;
};

struct pf_rdr {
	char			 ifname[IFNAMSIZ];
	struct ifnet		*ifp;
	TAILQ_ENTRY(pf_rdr)	 entries;
	struct pf_addr_wrap	 saddr;
	struct pf_addr_wrap	 daddr;
	struct pf_pool		 rpool;
	struct pf_pooladdr	*rcur;
	u_int32_t		 rlistid;
	u_int16_t		 dport;
	u_int16_t		 dport2;
	u_int16_t		 rport;
	sa_family_t		 af;
	u_int8_t		 proto;
	u_int8_t		 snot;
	u_int8_t		 dnot;
	u_int8_t		 ifnot;
	u_int8_t		 opts;
	u_int8_t		 no;
};

TAILQ_HEAD(pf_rulequeue, pf_rule);

struct pf_pdesc {
	u_int64_t	 tot_len;	/* Make Mickey money */
	union {
		struct tcphdr		*tcp;
		struct udphdr		*udp;
		struct icmp		*icmp;
#ifdef INET6
		struct icmp6_hdr	*icmp6;
#endif /* INET6 */
		void			*any;
	} hdr;
	struct pf_addr	*src;
	struct pf_addr	*dst;
	u_int16_t	*ip_sum;
	u_int32_t	 p_len;		/* total length of payload */
	u_int16_t	 flags;		/* Let SCRUB trigger behavior in
					 * state code. Easier than tags */
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 tos;
};

/* flags for RDR options */
#define PF_DPORT_RANGE	0x01		/* Dest port uses range */
#define PF_RPORT_RANGE	0x02		/* RDR'ed port uses range */

/* Reasons code for passing/dropping a packet */
#define PFRES_MATCH	0		/* Explicit match of a rule */
#define PFRES_BADOFF	1		/* Bad offset for pull_hdr */
#define PFRES_FRAG	2		/* Dropping following fragment */
#define PFRES_SHORT	3		/* Dropping short packet */
#define PFRES_NORM	4		/* Dropping by normalizer */
#define PFRES_MEMORY	5		/* Dropped due to lacking mem */
#define PFRES_MAX	6		/* total+1 */

#define PFRES_NAMES { \
	"match", \
	"bad-offset", \
	"fragment", \
	"short", \
	"normalize", \
	"memory", \
	NULL \
}

/* UDP state enumeration */
#define PFUDPS_NO_TRAFFIC	0
#define PFUDPS_SINGLE		1
#define PFUDPS_MULTIPLE		2

#define PFUDPS_NSTATES		3	/* number of state levels */

#define PFUDPS_NAMES { \
	"NO TRAFFIC", \
	"SINGLE", \
	"MULTIPLE", \
	NULL \
}

/* Other protocol state enumeration */
#define PFOTHERS_NO_TRAFFIC	0
#define PFOTHERS_SINGLE		1
#define PFOTHERS_MULTIPLE	2

#define PFOTHERS_NSTATES	3	/* number of state levels */

#define PFOTHERS_NAMES { \
	"NO TRAFFIC", \
	"SINGLE", \
	"MULTIPLE", \
	NULL \
}

#define FCNT_STATE_SEARCH	0
#define FCNT_STATE_INSERT	1
#define FCNT_STATE_REMOVALS	2
#define FCNT_MAX		3


#define ACTION_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
	} while (0)

#define REASON_SET(a, x) \
	do { \
		if ((a) != NULL) \
			*(a) = (x); \
		if (x < PFRES_MAX) \
			pf_status.counters[x]++; \
	} while (0)

struct pf_status {
	u_int64_t	counters[PFRES_MAX];
	u_int64_t	fcounters[FCNT_MAX];
	u_int64_t	pcounters[2][2][3];
	u_int64_t	bcounters[2][2];
	u_int32_t	running;
	u_int32_t	states;
	u_int32_t	since;
	u_int32_t	debug;
	char		ifname[IFNAMSIZ];
};

struct cbq_opts {
	u_int		minburst;
	u_int		maxburst;
	u_int		pktsize;
	u_int		maxpktsize;
	u_int		ns_per_byte;
	u_int		maxidle;
	int		minidle;
	u_int		offtime;
	int		flags;
};

struct hfsc_opts {
	u_int		rt_m1;
	u_int		rt_d;
	u_int		rt_m2;
	u_int		ls_m1;
	u_int		ls_d;
	u_int		ls_m2;
	int		flags;
};

struct pf_altq {
	char			 ifname[IFNAMSIZ];

	void			*altq_disc;	/* discipline-specific state */
	TAILQ_ENTRY(pf_altq)	 entries;

	/* scheduler spec */
	u_int8_t		 scheduler;	/* scheduler type */
	u_int16_t		 tbrsize;	/* tokenbuket regulator size */
	u_int32_t		 ifbandwidth;	/* interface bandwidth */

	/* queue spec */
	char			 qname[PF_QNAME_SIZE];	/* queue name */
	char			 parent[PF_QNAME_SIZE];	/* parent name */
	u_int32_t		 parent_qid;	/* parent queue id */
	u_int32_t		 bandwidth;	/* queue bandwidth */
	u_int8_t		 priority;	/* priority */
	u_int16_t		 qlimit;	/* queue size limit */
	u_int16_t		 flags;		/* misc flags */
	union {
		struct cbq_opts		 cbq_opts;
		struct hfsc_opts	 hfsc_opts;
		/* and other discipline specific options */
	} pq_u;

	u_int32_t		 qid;		/* return value */
};

#define PFFRAG_FRENT_HIWAT	5000	/* Number of fragment entries */
#define PFFRAG_FRAG_HIWAT	1000	/* Number of fragmented packets */
#define PFFRAG_FRCENT_HIWAT	50000	/* Number of fragment cache entries */
#define PFFRAG_FRCACHE_HIWAT	10000	/* Number of fragment descriptors */

/*
 * ioctl parameter structures
 */

struct pfioc_pooladdr {
	u_int32_t		 ticket;
	u_int32_t		 nr;
	u_int32_t		 r_num;
	u_int8_t		 r_id;
	u_int8_t		 af;
	struct pf_pooladdr	 addr;
};

struct pfioc_changeaddr {
	u_int32_t		 action;
	u_int32_t		 r_num;
	u_int8_t		 r_id;
	u_int8_t		 af;
	struct pf_pooladdr	 newaddr;
	struct pf_pooladdr	 oldaddr;
};

struct pfioc_rule {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_rule	 rule;
};

struct pfioc_changerule {
	u_int32_t		 action;
	struct pf_rule		 oldrule;
	struct pf_rule		 newrule;
};

struct pfioc_nat {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_nat	 nat;
};

struct pfioc_changenat {
	u_int32_t	 action;
	struct pf_nat	 oldnat;
	struct pf_nat	 newnat;
};

struct pfioc_natlook {
	struct pf_addr	 saddr;
	struct pf_addr	 daddr;
	struct pf_addr	 rsaddr;
	struct pf_addr	 rdaddr;
	u_int16_t	 sport;
	u_int16_t	 dport;
	u_int16_t	 rsport;
	u_int16_t	 rdport;
	sa_family_t	 af;
	u_int8_t	 proto;
	u_int8_t	 direction;
};

struct pfioc_binat {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_binat	 binat;
};

struct pfioc_changebinat {
	u_int32_t	action;
	struct pf_binat	oldbinat;
	struct pf_binat	newbinat;
};

struct pfioc_rdr {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_rdr	 rdr;
};

struct pfioc_changerdr {
	u_int32_t	 action;
	struct pf_rdr	 oldrdr;
	struct pf_rdr	 newrdr;
};

struct pfioc_state {
	u_int32_t	 nr;
	struct pf_state	 state;
};

struct pfioc_state_kill {
	/* XXX returns the number of states killed in psk_af */
	sa_family_t		psk_af;
	int			psk_proto;
	struct pf_rule_addr	psk_src;
	struct pf_rule_addr	psk_dst;
};

struct pfioc_states {
	int	ps_len;
	union {
		caddr_t psu_buf;
		struct pf_state *psu_states;
	} ps_u;
#define ps_buf		ps_u.psu_buf
#define ps_states	ps_u.psu_states
};

struct pfioc_if {
	char		 ifname[IFNAMSIZ];
};

struct pfioc_tm {
	int		 timeout;
	int		 seconds;
};

struct pfioc_limit {
	int		 index;
	unsigned	 limit;
};

struct pfioc_altq {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	struct pf_altq	 altq;
};

struct pfioc_changealtq {
	u_int32_t	 action;
	struct pf_altq	 oldaltq;
	struct pf_altq	 newaltq;
};

struct pfioc_qstats {
	u_int32_t	 ticket;
	u_int32_t	 nr;
	void		*buf;
	int		 nbytes;
	u_int8_t	 scheduler;
};

/*
 * ioctl operations
 */

#define DIOCSTART	_IO  ('D',  1)
#define DIOCSTOP	_IO  ('D',  2)
#define DIOCBEGINRULES	_IOWR('D',  3, u_int32_t)
#define DIOCADDRULE	_IOWR('D',  4, struct pfioc_rule)
#define DIOCCOMMITRULES	_IOWR('D',  5, u_int32_t)
#define DIOCGETRULES	_IOWR('D',  6, struct pfioc_rule)
#define DIOCGETRULE	_IOWR('D',  7, struct pfioc_rule)
#define DIOCBEGINNATS	_IOWR('D',  8, u_int32_t)
#define DIOCADDNAT	_IOWR('D',  9, struct pfioc_nat)
#define DIOCCOMMITNATS	_IOWR('D', 10, u_int32_t)
#define DIOCGETNATS	_IOWR('D', 11, struct pfioc_nat)
#define DIOCGETNAT	_IOWR('D', 12, struct pfioc_nat)
#define DIOCBEGINRDRS	_IOWR('D', 13, u_int32_t)
#define DIOCADDRDR	_IOWR('D', 14, struct pfioc_rdr)
#define DIOCCOMMITRDRS	_IOWR('D', 15, u_int32_t)
#define DIOCGETRDRS	_IOWR('D', 16, struct pfioc_rdr)
#define DIOCGETRDR	_IOWR('D', 17, struct pfioc_rdr)
#define DIOCCLRSTATES	_IO  ('D', 18)
#define DIOCGETSTATE	_IOWR('D', 19, struct pfioc_state)
#define DIOCSETSTATUSIF _IOWR('D', 20, struct pfioc_if)
#define DIOCGETSTATUS	_IOWR('D', 21, struct pf_status)
#define DIOCCLRSTATUS	_IO  ('D', 22)
#define DIOCNATLOOK	_IOWR('D', 23, struct pfioc_natlook)
#define DIOCSETDEBUG	_IOWR('D', 24, u_int32_t)
#define DIOCGETSTATES	_IOWR('D', 25, struct pfioc_states)
#define DIOCCHANGERULE	_IOWR('D', 26, struct pfioc_changerule)
#define DIOCCHANGENAT	_IOWR('D', 27, struct pfioc_changenat)
#define DIOCCHANGERDR	_IOWR('D', 28, struct pfioc_changerdr)
#define DIOCSETTIMEOUT	_IOWR('D', 29, struct pfioc_tm)
#define DIOCGETTIMEOUT	_IOWR('D', 30, struct pfioc_tm)
#define DIOCBEGINBINATS	_IOWR('D', 31, u_int32_t)
#define DIOCADDBINAT	_IOWR('D', 32, struct pfioc_binat)
#define DIOCCOMMITBINATS _IOWR('D', 33, u_int32_t)
#define DIOCGETBINATS	_IOWR('D', 34, struct pfioc_binat)
#define DIOCGETBINAT	_IOWR('D', 35, struct pfioc_binat)
#define DIOCCHANGEBINAT	_IOWR('D', 36, struct pfioc_changebinat)
#define DIOCADDSTATE	_IOWR('D', 37, struct pfioc_state)
#define DIOCCLRRULECTRS	_IO  ('D', 38)
#define DIOCGETLIMIT	_IOWR('D', 39, struct pfioc_limit)
#define DIOCSETLIMIT	_IOWR('D', 40, struct pfioc_limit)
#define DIOCKILLSTATES	_IOWR('D', 41, struct pfioc_state_kill)
#define DIOCSTARTALTQ	_IO  ('D', 42)
#define DIOCSTOPALTQ	_IO  ('D', 43)
#define DIOCBEGINALTQS	_IOWR('D', 44, u_int32_t)
#define DIOCADDALTQ	_IOWR('D', 45, struct pfioc_altq)
#define DIOCCOMMITALTQS	_IOWR('D', 46, u_int32_t)
#define DIOCGETALTQS	_IOWR('D', 47, struct pfioc_altq)
#define DIOCGETALTQ	_IOWR('D', 48, struct pfioc_altq)
#define DIOCCHANGEALTQ	_IOWR('D', 49, struct pfioc_altq)
#define DIOCGETQSTATS	_IOWR('D', 50, struct pfioc_qstats)
#define DIOCBEGINADDRS	_IOWR('D', 51, u_int32_t)
#define DIOCADDADDR	_IOWR('D', 52, struct pfioc_pooladdr)
#define DIOCGETADDRS	_IOWR('D', 53, struct pfioc_pooladdr)
#define DIOCGETADDR	_IOWR('D', 54, struct pfioc_pooladdr)
#define DIOCCHANGEADDR	_IOWR('D', 55, struct pfioc_pooladdr)


#ifdef _KERNEL
RB_HEAD(pf_state_tree, pf_tree_node);
RB_PROTOTYPE(pf_state_tree, pf_tree_node, entry, pf_state_compare);
extern struct pf_state_tree tree_lan_ext, tree_ext_gwy;

extern struct pf_rulequeue		 pf_rules[2];
TAILQ_HEAD(pf_natqueue, pf_nat);
extern struct pf_natqueue		 pf_nats[2];
TAILQ_HEAD(pf_binatqueue, pf_binat);
extern struct pf_binatqueue		 pf_binats[2];
TAILQ_HEAD(pf_rdrqueue, pf_rdr);
extern struct pf_rdrqueue		 pf_rdrs[2];
TAILQ_HEAD(pf_poolqueue, pf_pool);
extern struct pf_poolqueue		 pf_pools[2];
TAILQ_HEAD(pf_altqqueue, pf_altq);
extern struct pf_altqqueue		 pf_altqs[2];
extern struct pf_palist			 pf_pabuf;


extern u_int32_t		 ticket_rules_active;
extern u_int32_t		 ticket_rules_active;
extern u_int32_t		 ticket_rules_inactive;
extern u_int32_t		 ticket_nats_active;
extern u_int32_t		 ticket_nats_inactive;
extern u_int32_t		 ticket_binats_active;
extern u_int32_t		 ticket_binats_inactive;
extern u_int32_t		 ticket_rdrs_active;
extern u_int32_t		 ticket_rdrs_inactive;
extern u_int32_t		 ticket_rules_inactive;
extern u_int32_t		 ticket_altqs_active;
extern u_int32_t		 ticket_altqs_inactive;
extern u_int32_t		 ticket_pabuf;
extern struct pf_rulequeue	*pf_rules_active;
extern struct pf_rulequeue	*pf_rules_inactive;
extern struct pf_natqueue	*pf_nats_active;
extern struct pf_natqueue	*pf_nats_inactive;
extern struct pf_binatqueue	*pf_binats_active;
extern struct pf_binatqueue	*pf_binats_inactive;
extern struct pf_rdrqueue	*pf_rdrs_active;
extern struct pf_rdrqueue	*pf_rdrs_inactive;
extern struct pf_altqqueue	*pf_altqs_active;
extern struct pf_altqqueue	*pf_altqs_inactive;
extern struct pf_poolqueue	*pf_pools_active;
extern struct pf_poolqueue	*pf_pools_inactive;
extern void			 pf_dynaddr_remove(struct pf_addr_wrap *);
extern int			 pf_dynaddr_setup(struct pf_addr_wrap *,
				    u_int8_t);
extern void			 pf_calc_skip_steps(struct pf_rulequeue *);
extern void			 pf_dynaddr_copyout(struct pf_addr_wrap *);
extern struct pool		 pf_tree_pl, pf_rule_pl, pf_nat_pl;
extern struct pool		 pf_rdr_pl, pf_state_pl, pf_binat_pl,
				    pf_addr_pl;
extern struct pool		 pf_altq_pl;
extern struct pool		 pf_pooladdr_pl;
extern void			 pf_purge_timeout(void *);
extern int			 pftm_interval;
extern void			 pf_purge_expired_states(void);
extern int			 pf_insert_state(struct pf_state *);
extern struct pf_state		*pf_find_state(struct pf_state_tree *,
				    struct pf_tree_node *);
extern struct ifnet		*status_ifp;
extern int			*pftm_timeouts[PFTM_MAX];
extern void			 pf_addrcpy(struct pf_addr *, struct pf_addr *,
				    u_int8_t);

#ifdef INET
int	pf_test(int, struct ifnet *, struct mbuf **);
#endif /* INET */

#ifdef INET6
int	pf_test6(int, struct ifnet *, struct mbuf **);
#endif /* INET */

int	pflog_packet(struct ifnet *, struct mbuf *, sa_family_t, u_short,
	    u_short, struct pf_rule *);
int	pf_match_addr(u_int8_t, struct pf_addr *, struct pf_addr *,
	    struct pf_addr *, sa_family_t);
int	pf_match(u_int8_t, u_int16_t, u_int16_t, u_int16_t);
int	pf_match_port(u_int8_t, u_int16_t, u_int16_t, u_int16_t);
int	pf_match_uid(u_int8_t, uid_t, uid_t, uid_t);
int	pf_match_gid(u_int8_t, gid_t, gid_t, gid_t);

void	pf_normalize_init(void);
int	pf_normalize_ip(struct mbuf **, int, struct ifnet *, u_short *);
void	pf_purge_expired_fragments(void);
int	pf_routable(struct pf_addr *addr, sa_family_t af);

extern struct pf_rulequeue *pf_rules_active;
extern struct pf_status pf_status;
extern struct pool pf_frent_pl, pf_frag_pl;
struct pf_pool_limit {
	void		*pp;
	unsigned	 limit;
};
extern struct pf_pool_limit pf_pool_limits[PF_LIMIT_MAX];

#endif /* _KERNEL */

#endif /* _NET_PFVAR_H_ */
