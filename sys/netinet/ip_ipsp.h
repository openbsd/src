/*	$OpenBSD: ip_ipsp.h,v 1.28 1999/03/27 21:04:19 provos Exp $	*/

/*
 * The authors of this code are John Ioannidis (ji@tla.org),
 * Angelos D. Keromytis (kermit@csd.uch.gr) and 
 * Niels Provos (provos@physnet.uni-hamburg.de).
 *
 * This code was written by John Ioannidis for BSD/OS in Athens, Greece, 
 * in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis.
 *
 * Additional transforms and features in 1997 and 1998 by Angelos D. Keromytis
 * and Niels Provos.
 *
 * Additional features in 1999 by Angelos D. Keromytis.
 *
 * Copyright (C) 1995, 1996, 1997, 1998, 1999 by John Ioannidis,
 * Angelos D. Keromytis and Niels Provos.
 *	
 * Permission to use, copy, and modify this software without fee
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

/*
 * IPSP global definitions.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <sys/md5k.h>
#include <netinet/ip_sha1.h>
#include <netinet/ip_rmd160.h>
#include <netinet/ip_blf.h>
#include <netinet/ip_cast.h>
#include <netinet/ip_skipjack.h>

union sockaddr_union
{
    struct sockaddr     sa;
    struct sockaddr_in  sin;
    struct sockaddr_in6 sin6;
    char  __maxsize[128];
};

/* HMAC key sizes */
#define MD5HMAC96_KEYSIZE       16
#define SHA1HMAC96_KEYSIZE      20
#define RIPEMD160HMAC96_KEYSIZE 20

/* IV lengths */
#define ESP_DES_IVS		8
#define ESP_3DES_IVS		8
#define ESP_BLF_IVS             8
#define ESP_CAST_IVS            8
#define ESP_SKIPJACK_IVS	8
#define ESP_MAX_IVS		8       /* Keep updated */

/* Block sizes -- it is assumed that they're powers of 2 */
#define ESP_DES_BLKS		8
#define ESP_3DES_BLKS		8
#define ESP_BLF_BLKS            8
#define ESP_CAST_BLKS           8
#define ESP_SKIPJACK_BLKS	8
#define ESP_MAX_BLKS            8       /* Keep updated */

#define HMAC_BLOCK_LEN		64

#define AH_HMAC_HASHLEN		12	/* 96 bits of authenticator */
#define AH_HMAC_RPLENGTH        4	/* 32 bits of replay counter */
#define AH_HMAC_INITIAL_RPL	1	/* Replay counter initial value */

/* HMAC definitions */
#define HMAC_IPAD_VAL           0x36
#define HMAC_OPAD_VAL           0x5C
#define HMAC_BLOCK_LEN          64

/* Authenticator lengths */
#define AH_MD5_ALEN		16
#define AH_SHA1_ALEN		20
#define AH_RMD160_ALEN		20
#define AH_ALEN_MAX		20 	/* Keep updated */

struct sockaddr_encap
{
    u_int8_t	sen_len;		/* length */
    u_int8_t	sen_family;		/* PF_KEY */
    u_int16_t	sen_type;		/* see SENT_* */
    union
    {
	u_int8_t	Data[16];	/* other stuff mapped here */

	struct				/* SENT_IP4 */
	{
	    struct in_addr Src;
	    struct in_addr Dst;
	    u_int16_t Sport;
	    u_int16_t Dport;
	    u_int8_t Proto;
	    u_int8_t Filler[3];
	} Sip4;

	struct				/* SENT_IPSP */
	{
	    struct in_addr Dst;
	    u_int32_t Spi;
	    u_int8_t Sproto;
	    u_int8_t Filler[7];
	} Sipsp;
    } Sen;
};

#define sen_data	Sen.Data
#define sen_ip_src	Sen.Sip4.Src
#define sen_ip_dst	Sen.Sip4.Dst
#define sen_proto	Sen.Sip4.Proto
#define sen_sport	Sen.Sip4.Sport
#define sen_dport	Sen.Sip4.Dport
#define sen_ipsp_dst	Sen.Sipsp.Dst
#define sen_ipsp_spi	Sen.Sipsp.Spi
#define sen_ipsp_sproto	Sen.Sipsp.Sproto

/*
 * The "type" is really part of the address as far as the routing
 * system is concerned. By using only one bit in the type field
 * for each type, we sort-of make sure that different types of
 * encapsulation addresses won't be matched against the wrong type.
 * 
 */

#define SENT_IP4	0x0001		/* data is two struct in_addr */
#define SENT_IPSP	0x0002		/* data as in IP4 plus SPI */

/*
 * SENT_HDRLEN is the length of the "header"
 * SENT_*_LEN are the lengths of various forms of sen_data
 * SENT_*_OFF are the offsets in the sen_data array of various fields
 */

#define SENT_HDRLEN	(2 * sizeof(u_int8_t) + sizeof(u_int16_t))

#define SENT_IP4_SRCOFF	(0)
#define SENT_IP4_DSTOFF (sizeof (struct in_addr))

#define SENT_IP4_LEN	20
#define SENT_IPSP_LEN	20

#define NOTIFY_SOFT_EXPIRE      0       /* Soft expiration of SA */
#define NOTIFY_HARD_EXPIRE      1       /* Hard expiration of SA */
#define NOTIFY_REQUEST_SA       2       /* Establish an SA */
  
#define NOTIFY_SATYPE_CONF      1       /* SA should do encryption */
#define NOTIFY_SATYPE_AUTH      2       /* SA should do authentication */
#define NOTIFY_SATYPE_TUNNEL    4       /* SA should use tunneling */

/*
 * For encapsulation routes are possible not only for the destination
 * address but also for the protocol, source and destination ports
 * if available
 */

struct route_enc {
    struct rtentry *re_rt;
    struct sockaddr_encap re_dst;
};

struct expiration
{
    u_int32_t             exp_timeout;
    union sockaddr_union  exp_dst;
    u_int32_t             exp_spi;
    u_int8_t              exp_sproto;
    struct expiration    *exp_next;
    struct expiration    *exp_prev;
};

struct flow
{
    struct flow           *flow_next;	/* Next in flow chain */
    struct flow           *flow_prev;	/* Previous in flow chain */
    struct tdb            *flow_sa;	/* Pointer to the SA */
    union sockaddr_union  flow_src;   	/* Source address */
    union sockaddr_union  flow_srcmask; /* Source netmask */
    union sockaddr_union  flow_dst;	/* Destination address */
    union sockaddr_union  flow_dstmask;	/* Destination netmask */
    u_int8_t	          flow_proto;	/* Transport protocol, if applicable */
    u_int8_t	          foo[3];	/* Alignment */
};

struct tdb				/* tunnel descriptor block */
{
    struct tdb	     *tdb_hnext;  	/* Next in hash chain */
    struct tdb	     *tdb_onext;        /* Next in output */
    struct tdb	     *tdb_inext;        /* Previous in output */

    struct xformsw   *tdb_xform;	/* Transformation to use */
    struct enc_xform *tdb_encalgxform;  /* Encryption algorithm xform */
    struct auth_hash *tdb_authalgxform; /* Authentication algorithm xform */

#define TDBF_UNIQUE	      0x00001	/* This should not be used by others */
#define TDBF_TIMER            0x00002	/* Absolute expiration timer in use */
#define TDBF_BYTES            0x00004	/* Check the byte counters */
#define TDBF_ALLOCATIONS      0x00008	/* Check the flows counters */
#define TDBF_INVALID          0x00010	/* This SPI is not valid yet/anymore */
#define TDBF_FIRSTUSE         0x00020	/* Expire after first use */
#define TDBF_HALFIV           0x00040   /* Use half-length IV (ESP old only) */
#define TDBF_SOFT_TIMER       0x00080	/* Soft expiration */
#define TDBF_SOFT_BYTES       0x00100	/* Soft expiration */
#define TDBF_SOFT_ALLOCATIONS 0x00200	/* Soft expiration */
#define TDBF_SOFT_FIRSTUSE    0x00400	/* Soft expiration */
#define TDBF_PFS              0x00800	/* Ask for PFS from Key Mgmt. */
#define TDBF_TUNNELING        0x01000	/* Force IP-IP encapsulation */
    u_int32_t	      tdb_flags;  	/* Flags related to this TDB */

    u_int32_t         tdb_exp_allocations;  /* Expire after so many flows */
    u_int32_t         tdb_soft_allocations; /* Expiration warning */ 
    u_int32_t         tdb_cur_allocations;  /* Total number of allocations */

    u_int64_t         tdb_exp_bytes;    /* Expire after so many bytes passed */
    u_int64_t         tdb_soft_bytes;	/* Expiration warning */
    u_int64_t         tdb_cur_bytes;	/* Current count of bytes */

    u_int64_t         tdb_exp_timeout;	/* When does the SPI expire */
    u_int64_t         tdb_soft_timeout;	/* Send a soft-expire warning */
    u_int64_t         tdb_established;	/* When was the SPI established */

    u_int64_t	      tdb_first_use;	  /* When was it first used */
    u_int64_t         tdb_soft_first_use; /* Soft warning */
    u_int64_t         tdb_exp_first_use;  /* Expire if tdb_first_use +
					   * tdb_exp_first_use <= curtime */
    u_int32_t	      tdb_spi;    	/* SPI */
    u_int16_t         tdb_amxkeylen;    /* AH-old only */
    u_int16_t         tdb_ivlen;        /* IV length */
    u_int8_t	      tdb_sproto;	/* IPsec protocol */
    u_int8_t          tdb_wnd;          /* Replay window */
    u_int16_t         tdb_FILLER;       /* Padding */
    
    union sockaddr_union tdb_dst;	/* Destination address for this SA */
    union sockaddr_union tdb_src;	/* Source address for this SA */
    union sockaddr_union tdb_proxy;

    u_int8_t         *tdb_key;          /* Key material (schedules) */
    u_int8_t         *tdb_ictx;         /* Authentication contexts */
    u_int8_t         *tdb_octx;
    u_int8_t         *tdb_srcid;        /* Source ID for this SA */
    u_int8_t         *tdb_dstid;        /* Destination ID for this SA */
    u_int8_t         *tdb_amxkey;       /* AH-old only */

    union
    {
	u_int8_t  Iv[ESP_3DES_IVS];     /* That's enough space */
	u_int32_t Ivl;        	        /* Make sure this is 4 bytes */
	u_int64_t Ivq; 		        /* Make sure this is 8 bytes! */
    }IV;
#define tdb_iv  IV.Iv
#define tdb_ivl IV.Ivl
#define tdb_ivq IV.Ivq

    u_int32_t         tdb_rpl;	        /* Replay counter */
    u_int32_t         tdb_bitmap;       /* Used for replay sliding window */
    u_int32_t         tdb_initial;	/* Initial replay value */

    u_int32_t         tdb_epoch;	/* Used by the kernfs interface */
    u_int16_t         tdb_srcid_len;
    u_int16_t         tdb_dstid_len;
    u_int16_t         tdb_srcid_type;
    u_int16_t         tdb_dstid_type;

    struct flow	     *tdb_flow; 	/* Which flows use this SA */

    struct tdb       *tdb_bind_out;	/* Outgoing SA to use */
    TAILQ_HEAD(tdb_bind_head, tdb) tdb_bind_in;
    TAILQ_ENTRY(tdb)  tdb_bind_in_next;	/* Refering Incoming SAs */
    TAILQ_HEAD(tdb_inp_head, inpcb) tdb_inp;
};

#define TDB_HASHMOD	257

struct tdb_ident {
    u_int32_t spi;
    union sockaddr_union dst;
    u_int8_t proto;
};

struct auth_hash {
    int type;
    char *name;
    u_int16_t keysize;
    u_int16_t hashsize; 
    u_int16_t ctxsize;
    void (*Init)(void *);
    void (*Update)(void *, u_int8_t *, u_int16_t);
    void (*Final)(u_int8_t *, void *);
};

struct enc_xform {
    int type;
    char *name;
    u_int16_t blocksize, ivsize;
    u_int16_t minkey, maxkey;
    u_int32_t ivmask;           /* Or all possible modes, zero iv = 1 */ 
    void (*encrypt)(struct tdb *, u_int8_t *);
    void (*decrypt)(struct tdb *, u_int8_t *);
};

struct ipsecinit
{
    u_int8_t       *ii_enckey;
    u_int8_t       *ii_authkey;
    u_int16_t       ii_enckeylen;
    u_int16_t       ii_authkeylen;
    u_int8_t        ii_encalg;
    u_int8_t        ii_authalg;
};
	  
struct xformsw
{
    u_short		xf_type;	/* Unique ID of xform */
    u_short		xf_flags;	/* flags (see below) */
    char		*xf_name;	/* human-readable name */
    int		(*xf_attach)(void);	/* called at config time */
    int		(*xf_init)(struct tdb *, struct xformsw *, struct ipsecinit *);
    int		(*xf_zeroize)(struct tdb *); /* termination */
    struct mbuf 	*(*xf_input)(struct mbuf *, struct tdb *); /* input */
    int		(*xf_output)(struct mbuf *, struct sockaddr_encap *,
			     struct tdb *, struct mbuf **);        /* output */
};

/* xform IDs */
#define XF_IP4		1	/* IP inside IP */
#define XF_OLD_AH	2	/* RFCs 1828 & 1852 */
#define XF_OLD_ESP	3	/* RFCs 1829 & 1851 */
#define XF_NEW_AH	4	/* AH HMAC 96bits */
#define XF_NEW_ESP	5	/* ESP + auth 96bits + replay counter */

/* xform attributes */
#define XFT_AUTH	0x0001
#define XFT_CONF	0x0100

#define IPSEC_ZEROES_SIZE	64
#define IPSEC_KERNFS_BUFSIZE    4096

#if BYTE_ORDER == LITTLE_ENDIAN
static __inline u_int64_t
htonq(u_int64_t q)
{
    register u_int32_t u, l;
    u = q >> 32;
    l = (u_int32_t) q;
        
    return htonl(u) | ((u_int64_t)htonl(l) << 32);
}

#define ntohq(_x) htonq(_x)

#elif BYTE_ORDER == BIG_ENDIAN

#define htonq(_x) (_x)
#define ntohq(_x) htonq(_x)

#else
#error  "Please fix <machine/endian.h>"
#endif                                          

/*
 * Names for IPsec sysctl objects
 */
#define IPSECCTL_PFKEY  		0
#define IPSECCTL_MAXID			1

#define CTL_IPSEC_NAMES {\
	{ "pfkey", CTLTYPE_NODE }, \
}

#define PFKEYCTL_ENCDEBUG	1
#define PFKEYCTL_MAXID		2

#define PFKEYCTL_NAMES {\
	{ 0, 0 }, \
	{ "encdebug", CTLTYPE_INT }, \
}

#ifdef _KERNEL
extern int encdebug;
extern int ipsec_in_use;
extern u_int8_t hmac_ipad_buffer[64];
extern u_int8_t hmac_opad_buffer[64];

struct tdb *tdbh[TDB_HASHMOD];
struct expiration *explist;
extern struct xformsw xformsw[], *xformswNXFORMSW;

/* Check if a given tdb has encryption, authentication and/or tunneling */
#define TDB_ATTRIB(x) (((x)->tdb_encalgxform ? NOTIFY_SATYPE_CONF : 0)| \
		       ((x)->tdb_authalgxform ? NOTIFY_SATYPE_AUTH : 0))

/* Traverse spi chain and get attributes */

#define SPI_CHAIN_ATTRIB(have, TDB_DIR, TDBP) {\
	struct tdb *tmptdb = (TDBP); \
	(have) = 0; \
	\
	while (tmptdb && tmptdb->tdb_xform) { \
	        if (tmptdb == NULL || tmptdb->tdb_flags & TDBF_INVALID) \
	                break; \
                (have) |= TDB_ATTRIB(tmptdb); \
                tmptdb = tmptdb->TDB_DIR; \
        } \
}

/* Misc. */
extern char *inet_ntoa4(struct in_addr);
extern char *ipsp_address(union sockaddr_union);

/* TDB management routines */
extern void tdb_add_inp(struct tdb *tdb, struct inpcb *inp);
extern u_int32_t reserve_spi(u_int32_t, u_int32_t, union sockaddr_union *,
			     union sockaddr_union *, u_int8_t, int *);
extern struct tdb *gettdb(u_int32_t, union sockaddr_union *, u_int8_t);
extern void puttdb(struct tdb *);
extern int tdb_delete(struct tdb *, int);
extern int tdb_init (struct tdb *, u_int16_t, struct ipsecinit *);

/* Expiration management routines */
extern struct expiration *get_expiration(void);
extern void put_expiration(struct expiration *);
extern void handle_expirations(void *);
extern void cleanup_expirations(union sockaddr_union *, u_int32_t, u_int8_t);

/* Flow management routines */
extern struct flow *get_flow(void);
extern void put_flow(struct flow *, struct tdb *);
extern void delete_flow(struct flow *, struct tdb *);
extern struct flow *find_flow(union sockaddr_union *, union sockaddr_union *,
			      union sockaddr_union *, union sockaddr_union *,
			      u_int8_t, struct tdb *);
extern struct flow *find_global_flow(union sockaddr_union *,
				     union sockaddr_union *,
				     union sockaddr_union *,
				     union sockaddr_union *, u_int8_t);

/* XF_IP4 */
extern int ipe4_attach(void);
extern int ipe4_init(struct tdb *, struct xformsw *, struct ipsecinit *);
extern int ipe4_zeroize(struct tdb *);
extern int ipe4_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
		       struct mbuf **);
extern void ipe4_input __P((struct mbuf *, ...));
extern void ip4_input __P((struct mbuf *, ...));

/* XF_OLD_AH */
extern int ah_old_attach(void);
extern int ah_old_init(struct tdb *, struct xformsw *, struct ipsecinit *);
extern int ah_old_zeroize(struct tdb *);
extern int ah_old_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			 struct mbuf **);
extern struct mbuf *ah_old_input(struct mbuf *, struct tdb *);

/* XF_NEW_AH */
extern int ah_new_attach(void);
extern int ah_new_init(struct tdb *, struct xformsw *, struct ipsecinit *);
extern int ah_new_zeroize(struct tdb *);
extern int ah_new_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			 struct mbuf **);
extern struct mbuf *ah_new_input(struct mbuf *, struct tdb *);

/* XF_OLD_ESP */
extern int esp_old_attach(void);
extern int esp_old_init(struct tdb *, struct xformsw *, struct ipsecinit *);
extern int esp_old_zeroize(struct tdb *);
extern int esp_old_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			  struct mbuf **);
extern struct mbuf *esp_old_input(struct mbuf *, struct tdb *);

/* XF_NEW_ESP */
extern int esp_new_attach(void);
extern int esp_new_init(struct tdb *, struct xformsw *, struct ipsecinit *);
extern int esp_new_zeroize(struct tdb *);
extern int esp_new_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			  struct mbuf **);
extern struct mbuf *esp_new_input(struct mbuf *, struct tdb *);

/* Padding */
extern caddr_t m_pad(struct mbuf *, int, int);

/* Replay window */
extern int checkreplaywindow32(u_int32_t, u_int32_t, u_int32_t *, u_int32_t,
                               u_int32_t *);

extern unsigned char ipseczeroes[];
#endif /* _KERNEL */
#endif /* _NETINET_IPSP_H_ */
