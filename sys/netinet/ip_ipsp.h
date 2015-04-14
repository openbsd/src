/*	$OpenBSD: ip_ipsp.h,v 1.164 2015/04/14 12:22:15 mikeb Exp $	*/
/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr),
 * Niels Provos (provos@physnet.uni-hamburg.de) and
 * Niklas Hallqvist (niklas@appli.se).
 *
 * The original version of this code was written by John Ioannidis
 * for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis and Niklas Hallqvist.
 *
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 * Copyright (c) 1999 Niklas Hallqvist.
 * Copyright (c) 2001, Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _NETINET_IPSP_H_
#define _NETINET_IPSP_H_

struct m_tag;

/* IPSP global definitions. */

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/timeout.h>
#endif
#include <sys/queue.h>
#include <netinet/in.h>

union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};

/* HMAC key sizes */
#define	MD5HMAC96_KEYSIZE	16
#define	SHA1HMAC96_KEYSIZE	20
#define	RIPEMD160HMAC96_KEYSIZE	20
#define	SHA2_256HMAC96_KEYSIZE	32
#define	SHA2_384HMAC96_KEYSIZE	48
#define	SHA2_512HMAC96_KEYSIZE	64

#define	AH_HMAC_MAX_HASHLEN	32	/* 256 bits of authenticator for SHA512 */
#define	AH_HMAC_RPLENGTH	4	/* 32 bits of replay counter */
#define	AH_HMAC_INITIAL_RPL	1	/* Replay counter initial value */

/* Authenticator lengths */
#define	AH_MD5_ALEN		16
#define	AH_SHA1_ALEN		20
#define	AH_RMD160_ALEN		20
#define	AH_SHA2_256_ALEN	32
#define	AH_SHA2_384_ALEN	48
#define	AH_SHA2_512_ALEN	64
#define	AH_ALEN_MAX		64 	/* Keep updated */

/* Reserved SPI numbers */
#define	SPI_LOCAL_USE		0
#define	SPI_RESERVED_MIN	1
#define	SPI_RESERVED_MAX	255

/* Reserved CPI numbers */
#define CPI_RESERVED_MIN	1
#define CPI_RESERVED_MAX	255
#define CPI_PRIVATE_MIN		61440
#define CPI_PRIVATE_MAX		65535

/* sysctl default values */
#define	IPSEC_DEFAULT_EMBRYONIC_SA_TIMEOUT	60	/* 1 minute */
#define	IPSEC_DEFAULT_PFS			1
#define	IPSEC_DEFAULT_SOFT_ALLOCATIONS		0
#define	IPSEC_DEFAULT_EXP_ALLOCATIONS		0
#define	IPSEC_DEFAULT_SOFT_BYTES		0
#define	IPSEC_DEFAULT_EXP_BYTES			0
#define	IPSEC_DEFAULT_SOFT_TIMEOUT		80000
#define	IPSEC_DEFAULT_EXP_TIMEOUT		86400
#define	IPSEC_DEFAULT_SOFT_FIRST_USE		3600
#define	IPSEC_DEFAULT_EXP_FIRST_USE		7200
#define	IPSEC_DEFAULT_DEF_ENC			"aes"
#define	IPSEC_DEFAULT_DEF_AUTH			"hmac-sha1"
#define	IPSEC_DEFAULT_EXPIRE_ACQUIRE		30
#define	IPSEC_DEFAULT_DEF_COMP			"deflate"

struct sockaddr_encap {
	u_int8_t	sen_len;		/* length */
	u_int8_t	sen_family;		/* PF_KEY */
	u_int16_t	sen_type;		/* see SENT_* */
	union {
		struct {				/* SENT_IP4 */
			u_int8_t	Direction;
			struct in_addr	Src;
			struct in_addr	Dst;
			u_int8_t	Proto;
			u_int16_t	Sport;
			u_int16_t	Dport;
		} Sip4;

		struct {				/* SENT_IP6 */
			u_int8_t	Direction;
			struct in6_addr	Src;
			struct in6_addr	Dst;
			u_int8_t	Proto;
			u_int16_t	Sport;
			u_int16_t	Dport;
		} Sip6;

		struct ipsec_policy	*PolicyHead;	/* SENT_IPSP */
	} Sen;
};

#define	IPSP_DIRECTION_IN	0x1
#define	IPSP_DIRECTION_OUT	0x2

#ifdef _KERNEL

#define	sen_data		Sen.Data
#define	sen_ip_src		Sen.Sip4.Src
#define	sen_ip_dst		Sen.Sip4.Dst
#define	sen_proto		Sen.Sip4.Proto
#define	sen_sport		Sen.Sip4.Sport
#define	sen_dport		Sen.Sip4.Dport
#define	sen_direction		Sen.Sip4.Direction
#define	sen_ip6_src		Sen.Sip6.Src
#define	sen_ip6_dst		Sen.Sip6.Dst
#define	sen_ip6_proto		Sen.Sip6.Proto
#define	sen_ip6_sport		Sen.Sip6.Sport
#define	sen_ip6_dport		Sen.Sip6.Dport
#define	sen_ip6_direction	Sen.Sip6.Direction
#define	sen_ipsp		Sen.PolicyHead

/*
 * The "type" is really part of the address as far as the routing
 * system is concerned. By using only one bit in the type field
 * for each type, we sort-of make sure that different types of
 * encapsulation addresses won't be matched against the wrong type.
 *
 */

#define	SENT_IP4	0x0001		/* data is two struct in_addr */
#define	SENT_IPSP	0x0002		/* data as in IP4/6 plus SPI */
#define	SENT_IP6	0x0004

#define	SENT_LEN	sizeof(struct sockaddr_encap)

struct ipsec_ref {
	u_int16_t	ref_type;	/* Subtype of data */
	int16_t		ref_len;	/* Length of data following */
	int		ref_count;	/* Reference count */
	int		ref_malloctype;	/* malloc(9) type, for freeing */
};

struct ipsec_acquire {
	union sockaddr_union		ipa_addr;
	u_int32_t			ipa_seq;
	struct sockaddr_encap		ipa_info;
	struct sockaddr_encap		ipa_mask;
	struct timeout			ipa_timeout;
	struct ipsec_policy		*ipa_policy;
	struct inpcb                    *ipa_pcb;
	TAILQ_ENTRY(ipsec_acquire)	ipa_ipo_next;
	TAILQ_ENTRY(ipsec_acquire)	ipa_next;
	TAILQ_ENTRY(ipsec_acquire)      ipa_inp_next;
};

struct ipsec_policy {
	struct sockaddr_encap	ipo_addr;
	struct sockaddr_encap	ipo_mask;

	union sockaddr_union	ipo_src;	/* Local address to use */
	union sockaddr_union	ipo_dst;	/* Remote gateway -- if it's zeroed:
						 * - on output, we try to
						 * contact the remote host
						 * directly (if needed).  
						 * - on input, we accept on if
						 * the inner source is the
						 * same as the outer source
						 * address, or if transport
						 * mode was used.
						 */

	u_int64_t		ipo_last_searched;	/* Timestamp of last lookup */

	u_int8_t		ipo_flags;	/* See IPSP_POLICY_* definitions */
	u_int8_t		ipo_type;	/* USE/ACQUIRE/... */
	u_int8_t		ipo_sproto;	/* ESP/AH; if zero, use system dflts */
	u_int			ipo_rdomain;

	int                     ipo_ref_count;

	struct tdb		*ipo_tdb;		/* Cached entry */

	struct ipsec_ref	*ipo_srcid;
	struct ipsec_ref	*ipo_dstid;

	TAILQ_HEAD(ipo_acquires_head, ipsec_acquire) ipo_acquires; /* List of acquires */
	TAILQ_ENTRY(ipsec_policy)	ipo_tdb_next;	/* List TDB policies */
	TAILQ_ENTRY(ipsec_policy)	ipo_list;	/* List of all policies */
};

#define	IPSP_POLICY_NONE	0x0000	/* No flags set */
#define	IPSP_POLICY_SOCKET	0x0001	/* Socket-attached policy */
#define	IPSP_POLICY_STATIC	0x0002	/* Static policy */

#define	IPSP_IPSEC_USE		0	/* Use if existing, don't acquire */
#define	IPSP_IPSEC_ACQUIRE	1	/* Try acquire, let packet through */
#define	IPSP_IPSEC_REQUIRE	2	/* Require SA */
#define	IPSP_PERMIT		3	/* Permit traffic through */
#define	IPSP_DENY		4	/* Deny traffic */
#define	IPSP_IPSEC_DONTACQ	5	/* Require, but don't acquire */

/* Notification types */
#define	NOTIFY_SOFT_EXPIRE	0	/* Soft expiration of SA */
#define	NOTIFY_HARD_EXPIRE	1	/* Hard expiration of SA */
#define	NOTIFY_REQUEST_SA	2	/* Establish an SA */

#define	NOTIFY_SATYPE_CONF	1	/* SA should do encryption */
#define	NOTIFY_SATYPE_AUTH	2	/* SA should do authentication */
#define	NOTIFY_SATYPE_TUNNEL	4	/* SA should use tunneling */
#define NOTIFY_SATYPE_COMP	5       /* SA (IPCA) should use compression */

/* Identity types */
#define	IPSP_IDENTITY_NONE		0
#define	IPSP_IDENTITY_PREFIX		1
#define	IPSP_IDENTITY_FQDN		2
#define	IPSP_IDENTITY_USERFQDN		3
#define	IPSP_IDENTITY_CONNECTION	4

struct tdb {				/* tunnel descriptor block */
	/*
	 * Each TDB is on three hash tables: one keyed on dst/spi/sproto,
	 * one keyed on dst/sproto, and one keyed on src/sproto. The first
	 * is used for finding a specific TDB, the second for finding TDBs
	 * for outgoing policy matching, and the third for incoming
	 * policy matching. The following three fields maintain the hash
	 * queues in those three tables.
	 */
	struct tdb	*tdb_hnext;	/* dst/spi/sproto table */
	struct tdb	*tdb_dnext;	/* dst/sproto table */
	struct tdb	*tdb_snext;	/* src/sproto table */
	struct tdb	*tdb_inext;
	struct tdb	*tdb_onext;

	struct xformsw		*tdb_xform;		/* Transform to use */
	struct enc_xform	*tdb_encalgxform;	/* Enc algorithm */
	struct auth_hash	*tdb_authalgxform;	/* Auth algorithm */
	struct comp_algo	*tdb_compalgxform;	/* Compression algo */

#define	TDBF_UNIQUE		0x00001	/* This should not be used by others */
#define	TDBF_TIMER		0x00002	/* Absolute expiration timer in use */
#define	TDBF_BYTES		0x00004	/* Check the byte counters */
#define	TDBF_ALLOCATIONS	0x00008	/* Check the flows counters */
#define	TDBF_INVALID		0x00010	/* This SPI is not valid yet/anymore */
#define	TDBF_FIRSTUSE		0x00020	/* Expire after first use */
#define	TDBF_SOFT_TIMER		0x00080	/* Soft expiration */
#define	TDBF_SOFT_BYTES		0x00100	/* Soft expiration */
#define	TDBF_SOFT_ALLOCATIONS	0x00200	/* Soft expiration */
#define	TDBF_SOFT_FIRSTUSE	0x00400	/* Soft expiration */
#define	TDBF_PFS		0x00800	/* Ask for PFS from Key Mgmt. */
#define	TDBF_TUNNELING		0x01000	/* Force IP-IP encapsulation */
#define	TDBF_SKIPCRYPTO		0x08000	/* Skip actual crypto processing */
#define	TDBF_USEDTUNNEL		0x10000	/* Appended a tunnel header in past */
#define	TDBF_UDPENCAP		0x20000	/* UDP encapsulation */
#define	TDBF_PFSYNC		0x40000	/* TDB will be synced */
#define	TDBF_PFSYNC_RPL		0x80000	/* Replay counter should be bumped */
#define	TDBF_ESN		0x100000 /* 64-bit sequence numbers (ESN) */

	u_int32_t	tdb_flags;	/* Flags related to this TDB */

	struct timeout	tdb_timer_tmo;
	struct timeout	tdb_first_tmo;
	struct timeout	tdb_stimer_tmo;
	struct timeout	tdb_sfirst_tmo;

	u_int32_t	tdb_seq;		/* Tracking number for PFKEY */
	u_int32_t	tdb_exp_allocations;	/* Expire after so many flows */
	u_int32_t	tdb_soft_allocations;	/* Expiration warning */
	u_int32_t	tdb_cur_allocations;	/* Total number of allocs */

	u_int64_t	tdb_exp_bytes;	/* Expire after so many bytes passed */
	u_int64_t	tdb_soft_bytes;	/* Expiration warning */
	u_int64_t	tdb_cur_bytes;	/* Current count of bytes */

	u_int64_t	tdb_exp_timeout;	/* When does the SPI expire */
	u_int64_t	tdb_soft_timeout;	/* Send soft-expire warning */
	u_int64_t	tdb_established;	/* When was SPI established */

	u_int64_t	tdb_first_use;		/* When was it first used */
	u_int64_t	tdb_soft_first_use;	/* Soft warning */
	u_int64_t	tdb_exp_first_use;	/* Expire if tdb_first_use +
						 * tdb_exp_first_use <= curtime
						 */

	u_int64_t	tdb_last_used;	/* When was this SA last used */
	u_int64_t	tdb_last_marked;/* Last SKIPCRYPTO status change */

	u_int64_t	tdb_cryptoid;	/* Crypto session ID */

	u_int32_t	tdb_spi;	/* SPI */
	u_int16_t	tdb_amxkeylen;	/* Raw authentication key length */
	u_int16_t	tdb_emxkeylen;	/* Raw encryption key length */
	u_int16_t	tdb_ivlen;	/* IV length */
	u_int8_t	tdb_sproto;	/* IPsec protocol */
	u_int8_t	tdb_wnd;	/* Replay window */
	u_int8_t	tdb_satype;	/* SA type (RFC2367, PF_KEY) */
	u_int8_t	tdb_updates;	/* pfsync update counter */

	union sockaddr_union	tdb_dst;	/* Destination address */
	union sockaddr_union	tdb_src;	/* Source address */

	u_int8_t	*tdb_amxkey;	/* Raw authentication key */
	u_int8_t	*tdb_emxkey;	/* Raw encryption key */

#define TDB_REPLAYWASTE	32
#define TDB_REPLAYMAX	(2100+TDB_REPLAYWASTE)

	u_int64_t	tdb_rpl;	/* Replay counter */
	u_int32_t	tdb_seen[howmany(TDB_REPLAYMAX, 32)]; /* Anti-replay window */

	u_int8_t	tdb_iv[4];	/* Used for HALF-IV ESP */

	struct ipsec_ref	*tdb_srcid;	/* Source ID for this SA */
	struct ipsec_ref	*tdb_dstid;	/* Destination ID for this SA */

	u_int32_t	tdb_mtu;	/* MTU at this point in the chain */
	u_int64_t	tdb_mtutimeout;	/* When to ignore this entry */

	u_int16_t	tdb_udpencap_port;	/* Peer UDP port */

	u_int16_t	tdb_tag;		/* Packet filter tag */
	u_int32_t	tdb_tap;		/* Alternate enc(4) interface */

	u_int		tdb_rdomain;		/* Routing domain */

	struct sockaddr_encap   tdb_filter; /* What traffic is acceptable */
	struct sockaddr_encap   tdb_filtermask; /* And the mask */

	TAILQ_HEAD(tdb_inp_head_in, inpcb)	tdb_inp_in;
	TAILQ_HEAD(tdb_inp_head_out, inpcb)	tdb_inp_out;
	TAILQ_HEAD(tdb_policy_head, ipsec_policy)	tdb_policy_head;
	TAILQ_ENTRY(tdb)	tdb_sync_entry;
};

#endif /* _KERNEL */

struct tdb_ident {
	u_int32_t spi;
	union sockaddr_union dst;
	u_int8_t proto;
	u_int rdomain;
};

struct tdb_crypto {
	u_int32_t		tc_spi;
	union sockaddr_union	tc_dst;
	u_int8_t		tc_proto;
	int			tc_protoff;
	int			tc_skip;
	caddr_t			tc_ptr;
	u_int			tc_rdomain;
};

struct ipsecinit {
	u_int8_t	*ii_enckey;
	u_int8_t	*ii_authkey;
	u_int16_t	ii_enckeylen;
	u_int16_t	ii_authkeylen;
	u_int8_t	ii_encalg;
	u_int8_t	ii_authalg;
	u_int8_t	ii_compalg;
};

/* xform IDs */
#define	XF_IP4		1	/* IP inside IP */
#define	XF_AH		2	/* AH */
#define	XF_ESP		3	/* ESP */
#define	XF_TCPSIGNATURE	5	/* TCP MD5 Signature option, RFC 2358 */
#define	XF_IPCOMP	6	/* IPCOMP */

/* xform attributes */
#define	XFT_AUTH	0x0001
#define	XFT_CONF	0x0100
#define	XFT_COMP	0x1000

#define	IPSEC_ZEROES_SIZE	256	/* Larger than an IP6 extension hdr. */

#ifdef _KERNEL

struct xformsw {
	u_short	xf_type;		/* Unique ID of xform */
	u_short	xf_flags;		/* flags (see below) */
	char	*xf_name;		/* human-readable name */
	int	(*xf_attach)(void);	/* called at config time */
	int	(*xf_init)(struct tdb *, struct xformsw *, struct ipsecinit *);
	int	(*xf_zeroize)(struct tdb *); /* termination */
	int	(*xf_input)(struct mbuf *, struct tdb *, int, int); /* input */
	int	(*xf_output)(struct mbuf *, struct tdb *, struct mbuf **,
	    int, int);        /* output */
};

extern int ipsec_in_use;
extern u_int64_t ipsec_last_added;
extern int ipsec_policy_pool_initialized;

extern int ipsec_keep_invalid;		/* lifetime of embryonic SAs (in sec) */
extern int ipsec_require_pfs;		/* use Perfect Forward Secrecy */
extern int ipsec_expire_acquire;	/* wait for security assoc. (in sec) */
extern int ipsec_soft_allocations;	/* flows/SA before renegotiation */
extern int ipsec_exp_allocations;	/* num. of flows/SA before it expires */
extern int ipsec_soft_bytes;		/* bytes/SA before renegotiation */
extern int ipsec_exp_bytes;		/* num of bytes/SA before it expires */
extern int ipsec_soft_timeout;		/* seconds/SA before renegotiation */
extern int ipsec_exp_timeout;		/* seconds/SA before it expires */
extern int ipsec_soft_first_use;	/* seconds between 1st asso & renego */
extern int ipsec_exp_first_use;		/* seconds between 1st asso & expire */	

extern char ipsec_def_enc[];
extern char ipsec_def_auth[];
extern char ipsec_def_comp[];

extern struct enc_xform enc_xform_des;
extern struct enc_xform enc_xform_3des;
extern struct enc_xform enc_xform_blf;
extern struct enc_xform enc_xform_cast5;

extern struct auth_hash auth_hash_hmac_md5_96;
extern struct auth_hash auth_hash_hmac_sha1_96;
extern struct auth_hash auth_hash_hmac_ripemd_160_96;

extern struct comp_algo comp_algo_deflate;

extern TAILQ_HEAD(ipsec_policy_head, ipsec_policy) ipsec_policy_head;
extern TAILQ_HEAD(ipsec_acquire_head, ipsec_acquire) ipsec_acquire_head;

/* Check if a given tdb has encryption, authentication and/or tunneling */
#define	TDB_ATTRIB(x) (((x)->tdb_encalgxform ? NOTIFY_SATYPE_CONF : 0) | \
		       ((x)->tdb_authalgxform ? NOTIFY_SATYPE_AUTH : 0) | \
		       ((x)->tdb_compalgxform ? NOTIFY_SATYPE_COMP : 0))

/* Traverse spi chain and get attributes */

#define	SPI_CHAIN_ATTRIB(have, TDB_DIR, TDBP)				\
do {									\
	int s = splsoftnet();						\
	struct tdb *tmptdb = (TDBP);					\
									\
	(have) = 0;							\
	while (tmptdb && tmptdb->tdb_xform) {				\
	        if (tmptdb == NULL || tmptdb->tdb_flags & TDBF_INVALID)	\
			break;						\
		(have) |= TDB_ATTRIB(tmptdb);				\
		tmptdb = tmptdb->TDB_DIR;				\
	}								\
	splx(s);							\
} while (/* CONSTCOND */ 0)

/* Misc. */
uint8_t	get_sa_require(struct inpcb *);
#ifdef ENCDEBUG
const char *ipsp_address(union sockaddr_union);
#endif /* ENCDEBUG */

/* TDB management routines */
void	tdb_add_inp(struct tdb *, struct inpcb *, int);
uint32_t reserve_spi(u_int, u_int32_t, u_int32_t, union sockaddr_union *,
		union sockaddr_union *, u_int8_t, int *);
struct	tdb *gettdb(u_int, u_int32_t, union sockaddr_union *, u_int8_t);
struct	tdb *gettdbbydst(u_int, union sockaddr_union *, u_int8_t,
		struct ipsec_ref *, struct ipsec_ref *,
		struct sockaddr_encap *, struct sockaddr_encap *);
struct	tdb *gettdbbysrc(u_int, union sockaddr_union *, u_int8_t,
		struct ipsec_ref *, struct ipsec_ref *,
		struct sockaddr_encap *, struct sockaddr_encap *);
struct	tdb *gettdbbysrcdst(u_int, u_int32_t, union sockaddr_union *,
		union sockaddr_union *, u_int8_t);
void	puttdb(struct tdb *);
void	tdb_delete(struct tdb *);
struct	tdb *tdb_alloc(u_int);
void	tdb_free(struct tdb *);
int	tdb_init(struct tdb *, u_int16_t, struct ipsecinit *);
int	tdb_walk(u_int, int (*)(struct tdb *, void *, int), void *);

/* XF_IP4 */
int	ipe4_attach(void);
int	ipe4_init(struct tdb *, struct xformsw *, struct ipsecinit *);
int	ipe4_zeroize(struct tdb *);
void	ipe4_input(struct mbuf *, ...);
void	ipip_input(struct mbuf *, int, struct ifnet *, int);
int	ipip_output(struct mbuf *, struct tdb *, struct mbuf **, int, int);

void	ip4_input(struct mbuf *, ...);

#ifdef INET6
int	ip4_input6(struct mbuf **, int *, int);
#endif /* INET */

/* XF_AH */
int 	ah_attach(void);
int 	ah_init(struct tdb *, struct xformsw *, struct ipsecinit *);
int 	ah_zeroize(struct tdb *);
int	ah_input(struct mbuf *, struct tdb *, int, int);
int	ah_output(struct mbuf *, struct tdb *, struct mbuf **, int, int);
int	ah_sysctl(int *, u_int, void *, size_t *, void *, size_t);

void	ah4_input(struct mbuf *, ...);
void	*ah4_ctlinput(int, struct sockaddr *, u_int, void *);
void	*udpencap_ctlinput(int, struct sockaddr *, u_int, void *);

#ifdef INET6
int	ah6_input(struct mbuf **, int *, int);
#endif /* INET6 */

/* XF_ESP */
int	esp_attach(void);
int	esp_init(struct tdb *, struct xformsw *, struct ipsecinit *);
int	esp_zeroize(struct tdb *);
int	esp_input(struct mbuf *, struct tdb *, int, int);
int	esp_output(struct mbuf *, struct tdb *, struct mbuf **, int, int);
int	esp_sysctl(int *, u_int, void *, size_t *, void *, size_t);

void	esp4_input(struct mbuf *, ...);
void	*esp4_ctlinput(int, struct sockaddr *, u_int, void *);

#ifdef INET6
int 	esp6_input(struct mbuf **, int *, int);
#endif /* INET6 */

/* XF_IPCOMP */
int	ipcomp_attach(void);
int	ipcomp_init(struct tdb *, struct xformsw *, struct ipsecinit *);
int	ipcomp_zeroize(struct tdb *);
int	ipcomp_input(struct mbuf *, struct tdb *, int, int);
int	ipcomp_output(struct mbuf *, struct tdb *, struct mbuf **, int, int);
int	ipcomp_sysctl(int *, u_int, void *, size_t *, void *, size_t);

void	ipcomp4_input(struct mbuf *, ...);

#ifdef INET6
int	ipcomp6_input(struct mbuf **, int *, int);
#endif /* INET6 */

/* XF_TCPSIGNATURE */
int	tcp_signature_tdb_attach(void);
int	tcp_signature_tdb_init(struct tdb *, struct xformsw *,
	    struct ipsecinit *);
int	tcp_signature_tdb_zeroize(struct tdb *);
int	tcp_signature_tdb_input(struct mbuf *, struct tdb *, int, int);
int	tcp_signature_tdb_output(struct mbuf *, struct tdb *, struct mbuf **,
	  int, int);

/* Replay window */
int	checkreplaywindow(struct tdb *, u_int32_t, u_int32_t *, int);

/* Packet processing */
int	ipsp_process_packet(struct mbuf *, struct tdb *, int, int);
int	ipsp_process_done(struct mbuf *, struct tdb *);
struct	tdb *ipsp_spd_lookup(struct mbuf *, int, int, int *, int,
	    struct tdb *, struct inpcb *, u_int32_t);
struct	tdb *ipsp_spd_inp(struct mbuf *, int, int, int *, int,
	    struct tdb *, struct inpcb *, struct ipsec_policy *);
int	ipsp_is_unspecified(union sockaddr_union);
int	ipsp_ref_match(struct ipsec_ref *, struct ipsec_ref *);
void	ipsp_reffree(struct ipsec_ref *);
void	ipsp_skipcrypto_mark(struct tdb_ident *);
void	ipsp_skipcrypto_unmark(struct tdb_ident *);
int	ipsp_aux_match(struct tdb *, struct ipsec_ref *, struct ipsec_ref *,
	    struct sockaddr_encap *, struct sockaddr_encap *);

int	ipsec_common_input(struct mbuf *, int, int, int, int, int);
int	ipsec_common_input_cb(struct mbuf *, struct tdb *, int, int,
	    struct m_tag *);
int	ipsec_delete_policy(struct ipsec_policy *);
ssize_t	ipsec_hdrsz(struct tdb *);
void	ipsec_adjust_mtu(struct mbuf *, u_int32_t);
struct	ipsec_policy *ipsec_add_policy(struct inpcb *, int, int);
struct	ipsec_acquire *ipsec_get_acquire(u_int32_t);

#endif /* _KERNEL */
#endif /* _NETINET_IPSP_H_ */
