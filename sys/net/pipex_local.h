/*	$OpenBSD: pipex_local.h,v 1.7 2010/07/09 08:36:31 yasuoka Exp $	*/

/*
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef __OpenBSD__
#define Static
#else
#define Static static
#endif

#define	PIPEX_PPTP	1
#define	PIPEX_PPPOE	1
#define	PIPEX_MPPE	1

#define PIPEX_ENABLED			0x0001

#ifndef	LOG_PPPAC
#define	LOG_PPPAC	LOG_KERN
#endif

/* compile time option constants */
#ifndef	PIPEX_MAX_SESSION
#define PIPEX_MAX_SESSION		512
#endif
#define PIPEX_HASH_DIV			8
#define PIPEX_HASH_SIZE			(PIPEX_MAX_SESSION/PIPEX_HASH_DIV)
#define PIPEX_HASH_MASK			(PIPEX_HASH_SIZE-1)	
#define PIPEX_CLOSE_TIMEOUT		30
#define PIPEX_DEQUEUE_LIMIT		(IFQ_MAXLEN >> 1)
#define	PIPEX_PPPMINLEN			5
	/* minimum PPP header length is 1 and minimum ppp payload length is 4 */

#ifndef	NNBY		/* usually defined on the <sys/types.h> */
#define	NNBY	8	/* number of bits of a byte */
#endif

#ifdef PIPEX_MPPE
/* mppe rc4 key */
struct pipex_mppe {
	int16_t	stateless:1,			/* key change mode */
		resetreq:1,
		reserved:14;
	int16_t	keylenbits;			/* key length */
	int16_t keylen;
	uint16_t coher_cnt;			/* cohency counter */
	struct  rc4_ctx rc4ctx;
	u_char master_key[PIPEX_MPPE_KEYLEN];	/* master key of MPPE */
	u_char session_key[PIPEX_MPPE_KEYLEN];	/* session key of MPPE */
};
#endif /* PIPEX_MPPE */

#ifdef PIPEX_PPPOE
struct pipex_pppoe_session {
	struct ifnet *over_ifp;			/* ether inteface */
	struct sockaddr peer_addr;		/* peer's sockaddr */
};
#endif /* PIPEX_PPPOE */

#ifdef PIPEX_PPTP
struct pipex_pptp_session {
	/* sequence number gap between pipex and userland */
	int32_t	snd_gap;			/* gap of our sequence */ 
	int32_t rcv_gap;			/* gap of peer's sequence */
	int32_t ul_snd_una;			/* userland send acked seq */

	uint32_t snd_nxt;			/* send next */
	uint32_t rcv_nxt;			/* receive next */
	uint32_t snd_una;			/* send acked sequence */
	uint32_t rcv_acked;			/* recv acked sequence */

	int winsz;				/* windows size */
	int maxwinsz;				/* max windows size */
	int peer_maxwinsz;			/* peer's max windows size */

	struct in_addr peer_address;		/* inet destination address */
	struct in_addr our_address;		/* inet source address */
};
#endif /* PIPEX_PPTP */

/* pppac ip-extension sessoin table */
struct pipex_session {
	struct radix_node	ps4_rn[2];  /* tree glue, and other values */
	LIST_ENTRY(pipex_session) session_list;	/* all session chain */
	LIST_ENTRY(pipex_session) state_list;	/* state list chain */
	LIST_ENTRY(pipex_session) id_chain;	/* id hash chain */
	LIST_ENTRY(pipex_session) peer_addr_chain;
						/* peer's address hash chain */
	uint16_t	state;			/* pipex session state */
#define PIPEX_STATE_INITIAL		0x0000
#define PIPEX_STATE_OPENED		0x0001
#define PIPEX_STATE_CLOSE_WAIT		0x0002
#define PIPEX_STATE_CLOSED		0x0003

	uint16_t	ip_forward:1,		/* {en|dis}ableIP forwarding */
			is_multicast:1,		/* virtual entry for mutlicast */
			reserved:14;
	uint16_t	protocol;		/* tunnel protocol (PK) */
	uint16_t	session_id;		/* session-id (PK) */
	uint16_t	peer_session_id;	/* peer's session-id */
	uint16_t	peer_mru;		/* peer's MRU */
	uint32_t	timeout_sec;		/* idle timeout */
	int		ppp_id;			/* PPP id */

	struct sockaddr_in ip_address;		/* remote address (AK) */
	struct sockaddr_in ip_netmask;		/* remote address mask (AK) */

	struct pipex_iface_context* pipex_iface;/* context for interface */

	uint32_t	ppp_flags;		/* configure flags */
#ifdef PIPEX_MPPE
	int ccp_id;				/* CCP packet id */
	struct pipex_mppe
	    mppe_recv,				/* MPPE context for incoming */
	    mppe_send;				/* MPPE context for outgoing */ 
#endif /*PIPEXMPPE */
	struct pipex_statistics stat;		/* statistics */
	union {
#ifdef PIPEX_PPPOE
		struct pipex_pppoe_session pppoe;	/* context for PPPoE */
#endif /* PIPEX_PPPOE */
#ifdef PIPEX_PPTP
		struct pipex_pptp_session pptp;		/* context for PPTP */
#endif /* PIPEX_PPTP */
		char _proto_unknown[0];
	} proto;
};

/* gre header */
struct pipex_gre_header {
	uint16_t flags;				/* flags and version*/
#define PIPEX_GRE_KFLAG			0x2000	/* keys present */
#define PIPEX_GRE_SFLAG			0x1000	/* seq present */
#define PIPEX_GRE_AFLAG			0x0080	/* ack present */
#define PIPEX_GRE_VER			0x0001	/* gre version code */
#define PIPEX_GRE_VERMASK		0x0003	/* gre version mask */

	uint16_t type;
#define PIPEX_GRE_PROTO_PPP		0x880b	/* gre/ppp */

	uint16_t len;			/* length not include gre header */
	uint16_t call_id;			/* call_id */
} __attribute__((__packed__));

/* pppoe header */
struct pipex_pppoe_header {
	uint8_t vertype;			/* version and type */
#define PIPEX_PPPOE_VERTYPE		0x11	/* version and type code */

	uint8_t code;				/* code */
#define PIPEX_PPPOE_CODE_SESSION	0x00	/* code session */

	uint16_t session_id;			/* session id */
	uint16_t length;			/* length */
} __attribute__((__packed__));

#ifdef PIPEX_DEBUG
#define PIPEX_DBG(a) if (pipex_debug & 1) pipex_session_log a
/* #define PIPEX_MPPE_DBG(a) if (pipex_debug & 1) pipex_session_log a */
#define PIPEX_MPPE_DBG(a)
#else
#define PIPEX_DBG(a)
#define PIPEX_MPPE_DBG(a)
#endif /* PIPEX_DEBUG */

#define PIPEX_ID_HASHTABLE(key)						\
	(&pipex_id_hashtable[(key) & PIPEX_HASH_MASK])
#define PIPEX_PEER_ADDR_HASHTABLE(key)					\
	(&pipex_peer_addr_hashtable[ntohl((key)) & PIPEX_HASH_MASK])
#define PIPEX_ADDR_HASHTABLE(key)					\
	(&pipex_addr_hashtable[ntohl((key)) & PIPEX_HASH_MASK])

#define GETCHAR(c, cp) do {						\
	(c) = *(cp)++;							\
} while (0)

#define PUTCHAR(s, cp) do {						\
	*(cp)++ = (u_char)(s);						\
} while (0)

#define GETSHORT(s, cp) do { 						\
	(s) = *(cp)++ << 8;						\
	(s) |= *(cp)++;							\
} while (0)

#define PUTSHORT(s, cp) do {						\
	*(cp)++ = (u_char) ((s) >> 8); 					\
	*(cp)++ = (u_char) (s);						\
} while (0)

#define GETLONG(l, cp) do {						\
	(l) = *(cp)++ << 8;						\
	(l) |= *(cp)++; (l) <<= 8;					\
	(l) |= *(cp)++; (l) <<= 8;					\
	(l) |= *(cp)++;							\
} while (0)

#define PUTLONG(l, cp) do {						\
	*(cp)++ = (u_char) ((l) >> 24);					\
	*(cp)++ = (u_char) ((l) >> 16);					\
	*(cp)++ = (u_char) ((l) >> 8);					\
	*(cp)++ = (u_char) (l);						\
} while (0)

#define PIPEX_PULLUP(m0, l)						\
	if ((m0)->m_len < (l)) {					\
		if ((m0)->m_pkthdr.len < (l)) {				\
			PIPEX_DBG((NULL, LOG_DEBUG,			\
			    "<%s> received packet is too short.",	\
			    __func__));					\
			m_freem(m0);					\
			(m0) = NULL;					\
		} else  {						\
			(m0) = m_pullup((m0), (l));			\
			KASSERT((m0) != NULL);				\
		}							\
	}
#define PIPEX_SEEK_NEXTHDR(ptr, len, t)					\
    ((t) (((char *)ptr) + len))
#define SEQ32_LT(a,b)	((int)((a) - (b)) <  0)
#define SEQ32_LE(a,b)	((int)((a) - (b)) <= 0)
#define SEQ32_GT(a,b)	((int)((a) - (b)) >  0)
#define SEQ32_GE(a,b)	((int)((a) - (b)) >= 0)
#define SEQ32_SUB(a,b)	((int32_t)((a) - (b)))

#define	pipex_session_is_acfc_accepted(s)				\
    (((s)->ppp_flags & PIPEX_PPP_ACFC_ACCEPTED)? 1 : 0)
#define	pipex_session_is_pfc_accepted(s)				\
    (((s)->ppp_flags & PIPEX_PPP_PFC_ACCEPTED)? 1 : 0)
#define	pipex_session_is_acfc_enabled(s)				\
    (((s)->ppp_flags & PIPEX_PPP_ACFC_ENABLED)? 1 : 0)
#define	pipex_session_is_pfc_enabled(s)					\
    (((s)->ppp_flags & PIPEX_PPP_PFC_ENABLED)? 1 : 0)
#define	pipex_session_has_acf(s)					\
    (((s)->ppp_flags & PIPEX_PPP_HAS_ACF)? 1 : 0)
#define	pipex_session_is_mppe_accepted(s)				\
    (((s)->ppp_flags & PIPEX_PPP_MPPE_ACCEPTED)? 1 : 0)
#define	pipex_session_is_mppe_enabled(s)				\
    (((s)->ppp_flags & PIPEX_PPP_MPPE_ENABLED)? 1 : 0)
#define	pipex_session_is_mppe_required(s)				\
    (((s)->ppp_flags & PIPEX_PPP_MPPE_REQUIRED)? 1 : 0)
#define pipex_mppe_rc4_keybits(r) ((r)->keylen << 3)
#define PIPEX_IPGRE_HDRLEN (sizeof(struct ip) + sizeof(struct pipex_gre_header))
#define PIPEX_TCP_OPTLEN 40

/*
 * static function prototypes
 */
Static int                   pipex_add_session (struct pipex_session_req *, struct pipex_iface_context *);
Static int                   pipex_close_session (struct pipex_session_close_req *);
Static int                   pipex_config_session (struct pipex_session_config_req *);
Static int                   pipex_get_stat (struct pipex_session_stat_req *);
Static int                   pipex_get_closed (struct pipex_session_list_req *);
Static int                   pipex_destroy_session (struct pipex_session *);
Static struct pipex_session  *pipex_lookup_by_ip_address (struct in_addr);
Static struct pipex_session  *pipex_lookup_by_session_id (int, int);
Static void                  pipex_ip_output (struct mbuf *, struct pipex_session *);
Static void                  pipex_ppp_output (struct mbuf *, struct pipex_session *, int);
Static inline int            pipex_ppp_proto (struct mbuf *, struct pipex_session *, int, int *);
Static void                  pipex_ppp_input (struct mbuf *, struct pipex_session *, int);
Static void                  pipex_ip_input (struct mbuf *, struct pipex_session *);

#ifdef PIPEX_PPPOE
Static void                  pipex_pppoe_output (struct mbuf *, struct pipex_session *);
#endif

#ifdef PIPEX_PPTP
Static void                  pipex_pptp_output (struct mbuf *, struct pipex_session *, int, int);
#endif

#ifdef PIPEX_MPPE
Static void                  pipex_mppe_req_init (struct pipex_mppe_req *, struct pipex_mppe *);
Static void                  GetNewKeyFromSHA (u_char *, u_char *, int, u_char *);
Static void                  pipex_mppe_reduce_key (struct pipex_mppe *);
Static void                  mppe_key_change (struct pipex_mppe *);
Static void                  pipex_mppe_input (struct mbuf *, struct pipex_session *);
Static void                  pipex_mppe_output (struct mbuf *, struct pipex_session *, uint16_t);
Static void                  pipex_ccp_input (struct mbuf *, struct pipex_session *);
Static int                   pipex_ccp_output (struct pipex_session *, int, int);
#endif

Static struct mbuf           *adjust_tcp_mss (struct mbuf *, int);
Static struct mbuf           *ip_is_idle_packet (struct mbuf *, int *);
Static void                  pipex_session_log (struct pipex_session *, int, const char *, ...)  __attribute__((__format__(__printf__,3,4)));
Static int                   pipex_ppp_enqueue (struct mbuf *, struct pipex_session *, struct ifqueue *);
Static void                  pipex_ppp_dequeue (void);
Static void                  pipex_timer_start (void);
Static void                  pipex_timer_stop (void);
Static void                  pipex_timer (void *);
