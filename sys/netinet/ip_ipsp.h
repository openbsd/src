/*	$OpenBSD: ip_ipsp.h,v 1.20 1998/11/25 11:47:17 niklas Exp $	*/

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
 * Copyright (C) 1995, 1996, 1997, 1998 by John Ioannidis, Angelos D. Keromytis
 * and Niels Provos.
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

/*
 * IPSP global definitions.
 */

struct expiration
{
    u_int32_t          exp_timeout;
    struct in_addr     exp_dst;
    u_int32_t          exp_spi;
    u_int8_t           exp_sproto;
    struct expiration *exp_next;
    struct expiration *exp_prev;
};

struct flow
{
    struct flow     *flow_next;		/* Next in flow chain */
    struct flow     *flow_prev;		/* Previous in flow chain */
    struct tdb      *flow_sa;		/* Pointer to the SA */
    struct in_addr   flow_src;   	/* Source address */
    struct in_addr   flow_srcmask;	/* Source netmask */
    struct in_addr   flow_dst;		/* Destination address */
    struct in_addr   flow_dstmask;	/* Destination netmask */
    u_int16_t	     flow_sport;	/* Source port, if applicable */
    u_int16_t	     flow_dport;	/* Destination port, if applicable */
    u_int8_t	     flow_proto;	/* Transport protocol, if applicable */
    u_int8_t	     foo[3];		/* Alignment */
};

struct tdb				/* tunnel descriptor block */
{
    struct tdb	   *tdb_hnext;  	/* next in hash chain */
    struct tdb	   *tdb_onext;	        /* next in output */
    struct tdb	   *tdb_inext;  	/* next in input (prev!) */
    struct xformsw *tdb_xform;	        /* transformation to use */
    u_int32_t	    tdb_spi;    	/* SPI to use */
    u_int32_t	    tdb_flags;  	/* Flags related to this TDB */
#define TDBF_UNIQUE	   0x00001	/* This should not be used by others */
#define TDBF_TIMER         0x00002	/* Absolute expiration timer in use */
#define TDBF_BYTES         0x00004	/* Check the byte counters */
#define TDBF_PACKETS       0x00008	/* Check the packet counters */
#define TDBF_INVALID       0x00010	/* This SPI is not valid yet/anymore */
#define TDBF_FIRSTUSE      0x00020	/* Expire after first use */
#define TDBF_TUNNELING     0x00040	/* Do IP-in-IP encapsulation */
#define TDBF_SOFT_TIMER    0x00080	/* Soft expiration */
#define TDBF_SOFT_BYTES    0x00100	/* Soft expiration */
#define TDBF_SOFT_PACKETS  0x00200	/* Soft expiration */
#define TDBF_SOFT_FIRSTUSE 0x00400	/* Soft expiration */
#define TDBF_SAME_TTL      0x00800	/* Keep the packet TTL, in tunneling */
    u_int64_t       tdb_exp_packets;	/* Expire after so many packets s|r */
    u_int64_t       tdb_soft_packets;	/* Expiration warning */ 
    u_int64_t       tdb_cur_packets;    /* Current number of packets s|r'ed */
    u_int64_t       tdb_exp_bytes;	/* Expire after so many bytes passed */
    u_int64_t       tdb_soft_bytes;	/* Expiration warning */
    u_int64_t       tdb_cur_bytes;	/* Current count of bytes */
    u_int64_t       tdb_exp_timeout;	/* When does the SPI expire */
    u_int64_t       tdb_soft_timeout;	/* Send a soft-expire warning */
    u_int64_t       tdb_established;	/* When was the SPI established */
    u_int64_t	    tdb_first_use;	/* When was it first used */
    u_int64_t       tdb_soft_first_use; /* Soft warning */
    u_int64_t       tdb_exp_first_use;	/* Expire if tdb_first_use +
					   tdb_exp_first_use <= curtime */
    struct in_addr  tdb_dst;	        /* dest address for this SPI */
    struct in_addr  tdb_src;	        /* source address for this SPI,
					 * used when tunneling */
    struct in_addr  tdb_osrc;
    struct in_addr  tdb_odst;		/* Source and destination addresses
					 * of outer IP header if we're doing
					 * tunneling */
    caddr_t	    tdb_xdata;	        /* transformation data (opaque) */
    struct flow	   *tdb_flow; 		/* Which flows use this SA */

    u_int8_t	    tdb_ttl;		/* TTL used in tunneling */
    u_int8_t	    tdb_sproto;		/* IPsec protocol */
    u_int16_t       tdb_satype;		/* Alignment */
    u_int32_t       tdb_epoch;		/* Used by the kernfs interface */
    u_int8_t       *tdb_confname;       /* Used by the kernfs interface */
    u_int8_t       *tdb_authname;       /* Used by the kernfs interface */
};

#define TDB_HASHMOD	257

struct xformsw
{
    u_short		xf_type;	/* Unique ID of xform */
    u_short		xf_flags;	/* flags (see below) */
    char		*xf_name;	/* human-readable name */
    int		(*xf_attach)(void);	/* called at config time */
    int		(*xf_init)(struct tdb *, struct xformsw *, struct mbuf *);	/* xform initialization */
    int		(*xf_zeroize)(struct tdb *); /* termination */
    struct mbuf 	*(*xf_input)(struct mbuf *, struct tdb *);	/* called when packet received */
    int		(*xf_output)(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);	/* called when packet sent */
};

#define XF_IP4		1		/* IP inside IP */
#define XF_OLD_AH	2		/* RFCs 1828 & 1852 */
#define XF_OLD_ESP	3		/* RFCs 1829 & 1851 */
#define XF_NEW_AH	4		/* AH HMAC 96bits */
#define XF_NEW_ESP	5		/* ESP + auth 96bits + replay counter */

/* Supported key hash algorithms */
#define ALG_AUTH_MD5	1
#define ALG_AUTH_SHA1	2
#define ALG_AUTH_RMD160 3

/* Supported encryption algorithms */
#define ALG_ENC_DES	1
#define ALG_ENC_3DES	2
#define ALG_ENC_BLF     3
#define ALG_ENC_CAST    4

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

extern unsigned char ipseczeroes[];

/*
 * Names for IPsec sysctl objects
 */
#define IPSECCTL_ENCAP			0
#define IPSECCTL_MAXID			1

#define CTL_IPSEC_NAMES {\
	{ "encap", CTLTYPE_NODE }, \
}

#ifdef _KERNEL
extern int encdebug;

struct tdb *tdbh[TDB_HASHMOD];
struct expiration *explist;
extern struct xformsw xformsw[], *xformswNXFORMSW;
u_int32_t notify_msgids;

/* Check if a given tdb has encryption, authentication and/or tunneling */
#define TDB_ATTRIB(x) (((x)->tdb_confname != NULL ? NOTIFY_SATYPE_CONF : 0)| \
		       ((x)->tdb_authname != NULL ? NOTIFY_SATYPE_AUTH : 0)| \
		       ((x)->tdb_confname != NULL && \
			((x)->tdb_flags & TDBF_TUNNELING) ? NOTIFY_SATYPE_TUNNEL : 0))

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

/* TDB management routines */
extern u_int32_t reserve_spi(u_int32_t, struct in_addr, u_int8_t, int *);
extern struct tdb *gettdb(u_int32_t, struct in_addr, u_int8_t);
extern void puttdb(struct tdb *);
extern int tdb_delete(struct tdb *, int);

/* Expiration management routines */
extern struct expiration *get_expiration(void);
extern void put_expiration(struct expiration *);
extern void handle_expirations(void *);
extern void cleanup_expirations(struct in_addr, u_int32_t, u_int8_t);

/* Flow management routines */
extern struct flow *get_flow(void);
extern void put_flow(struct flow *, struct tdb *);
extern void delete_flow(struct flow *, struct tdb *);
extern struct flow *find_flow(struct in_addr, struct in_addr, struct in_addr,
			      struct in_addr, u_int8_t, u_int16_t, u_int16_t,
			      struct tdb *);
extern struct flow *find_global_flow(struct in_addr, struct in_addr,
				     struct in_addr, struct in_addr, u_int8_t,
				     u_int16_t, u_int16_t);

/* XF_IP4 */
extern int ipe4_attach(void);
extern int ipe4_init(struct tdb *, struct xformsw *, struct mbuf *);
extern int ipe4_zeroize(struct tdb *);
extern int ipe4_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
		       struct mbuf **);
extern void ipe4_input __P((struct mbuf *, ...));
extern void ip4_input __P((struct mbuf *, ...));

/* XF_OLD_AH */
extern int ah_old_attach(void);
extern int ah_old_init(struct tdb *, struct xformsw *, struct mbuf *);
extern int ah_old_zeroize(struct tdb *);
extern int ah_old_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			 struct mbuf **);
extern struct mbuf *ah_old_input(struct mbuf *, struct tdb *);

/* XF_NEW_AH */
extern int ah_new_attach(void);
extern int ah_new_init(struct tdb *, struct xformsw *, struct mbuf *);
extern int ah_new_zeroize(struct tdb *);
extern int ah_new_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			 struct mbuf **);
extern struct mbuf *ah_new_input(struct mbuf *, struct tdb *);

/* XF_OLD_ESP */
extern int esp_old_attach(void);
extern int esp_old_init(struct tdb *, struct xformsw *, struct mbuf *);
extern int esp_old_zeroize(struct tdb *);
extern int esp_old_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			  struct mbuf **);
extern struct mbuf *esp_old_input(struct mbuf *, struct tdb *);

/* XF_NEW_ESP */
extern int esp_new_attach(void);
extern int esp_new_init(struct tdb *, struct xformsw *, struct mbuf *);
extern int esp_new_zeroize(struct tdb *);
extern int esp_new_output(struct mbuf *, struct sockaddr_encap *, struct tdb *,
			  struct mbuf **);
extern struct mbuf *esp_new_input(struct mbuf *, struct tdb *);

/* Padding */
extern caddr_t m_pad(struct mbuf *, int);

/* Replay window */
extern int checkreplaywindow32(u_int32_t, u_int32_t, u_int32_t *, u_int32_t,
			       u_int32_t *);
#endif
