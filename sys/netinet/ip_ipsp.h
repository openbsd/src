/*	$OpenBSD: ip_ipsp.h,v 1.10 1997/07/02 06:58:43 provos Exp $	*/

/*
 * The author of this code is John Ioannidis, ji@tla.org,
 * 	(except when noted otherwise).
 *
 * This code was written for BSD/OS in Athens, Greece, in November 1995.
 *
 * Ported to OpenBSD and NetBSD, with additional transforms, in December 1996,
 * by Angelos D. Keromytis, kermit@forthnet.gr.
 *
 * Copyright (C) 1995, 1996, 1997 by John Ioannidis and Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NEITHER AUTHOR MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 * IPSP global definitions.
 */

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
#define TDBF_RELATIVE      0x00040	/* Expire after X secs from establ. */
#define TDBF_SOFT_TIMER    0x00080	/* Soft expiration */
#define TDBF_SOFT_BYTES    0x00100	/* Soft expiration */
#define TDBF_SOFT_PACKETS  0x00200	/* Soft expiration */
#define TDBF_SOFT_FIRSTUSE 0x00400	/* Soft expiration */
#define TDBF_SOFT_RELATIVE 0x00800	/* Soft expiration */
#define TDBF_TUNNELING     0x01000	/* Do IP-in-IP encapsulation */
#define TDBF_SAME_TTL      0x02000	/* Keep the packet TTL, in tunneling */
    u_int64_t       tdb_exp_packets;	/* Expire after so many packets s|r */
    u_int64_t       tdb_soft_packets;	/* Expiration warning */ 
    u_int64_t       tdb_cur_packets;    /* Current number of packets s|r'ed */
    u_int64_t       tdb_exp_bytes;	/* Expire after so many bytes passed */
    u_int64_t       tdb_soft_bytes;	/* Expiration warning */
    u_int64_t       tdb_cur_bytes;	/* Current count of bytes */
    u_int64_t       tdb_exp_timeout;	/* When does the SPI expire */
    u_int64_t       tdb_soft_timeout;	/* Send a soft-expire warning */
    u_int64_t       tdb_established;	/* When was the SPI established */
    u_int64_t	    tdb_soft_relative ; /* Soft warning */
    u_int64_t       tdb_exp_relative;   /* Expire if tdb_established +
					    tdb_exp_relative <= curtime */
    u_int64_t	    tdb_first_use;	/* When was it first used */
    u_int64_t       tdb_soft_first_use; /* Soft warning */
    u_int64_t       tdb_exp_first_use;	/* Expire if tdb_first_use +
					   tdb_exp_first_use <= curtime */
    struct in_addr  tdb_dst;	        /* dest address for this SPI */
    struct in_addr  tdb_src;	        /* source address for this SPI,
					 * used when tunneling */
    struct in_addr  tdb_osrc;
    struct in_addr  tdb_odst;		/* Source and destination addresses
					 * of outter IP header if we're doing
					 * tunneling */
    caddr_t	    tdb_xdata;	        /* transformation data (opaque) */
    u_int16_t	    tdb_sport;		/* Source port, if applicable */
    u_int16_t       tdb_dport;		/* Destination port, if applicable */

    u_int8_t	    tdb_ttl;		/* TTL used in tunneling */
    u_int8_t	    tdb_proto;		/* Protocol carried */
    u_int16_t	    tdb_foo;		/* alignment */
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
#define XF_AHMD5	2		/* AH MD5 */
#define XF_AHSHA1	3		/* AH SHA */
#define XF_ESPDES	4		/* ESP DES-CBC */
#define XF_ESP3DES	5		/* ESP DES3-CBC */
#define XF_AHHMACMD5	6		/* AH-HMAC-MD5 with opt replay prot */
#define XF_AHHMACSHA1	7		/* AH-HMAC-SHA1 with opt replay prot */
#define XF_ESPDESMD5	8		/* ESP DES-CBC + MD5 */
#define XF_ESP3DESMD5	9		/* ESP 3DES-CBC + MD5 */
#define XF_NEWESP       10		/* The new ESP transforms */
#define XF_NEWAH        11		/* The new AH transforms */

#define XFT_AUTH	0x0001
#define XFT_CONF	0x0100

#define IPSEC_ZEROES_SIZE	64

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

#ifdef _KERNEL
#undef ENCDEBUG	
extern int encdebug;

struct tdb *tdbh[TDB_HASHMOD];
extern struct xformsw xformsw[], *xformswNXFORMSW;

extern u_int32_t reserve_spi(u_int32_t, struct in_addr, int *);
extern struct tdb *gettdb(u_int32_t, struct in_addr);
extern void puttdb(struct tdb *);
extern int tdb_delete(struct tdb *, int);

extern int ipe4_attach(void), ipe4_init(struct tdb *, struct xformsw *, struct mbuf *), ipe4_zeroize(struct tdb *);
extern int ipe4_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern void ipe4_input __P((struct mbuf *, ...));

extern int ahmd5_attach(void), ahmd5_init(struct tdb *, struct xformsw *, struct mbuf *), ahmd5_zeroize(struct tdb *);
extern int ahmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahmd5_input(struct mbuf *, struct tdb *);

extern int ahsha1_attach(void), ahsha1_init(struct tdb *, struct xformsw *, struct mbuf *), ahsha1_zeroize(struct tdb *);
extern int ahsha1_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahsha1_input(struct mbuf *, struct tdb *);

extern int ahhmacmd5_attach(void), ahhmacmd5_init(struct tdb *, struct xformsw *, struct mbuf *), ahhmacmd5_zeroize(struct tdb *);
extern int ahhmacmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahhmacmd5_input(struct mbuf *, struct tdb *);

extern int ahhmacsha1_attach(void), ahhmacsha1_init(struct tdb *, struct xformsw *, struct mbuf *), ahhmacsha1_zeroize(struct tdb *);
extern int ahhmacsha1_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahhmacsha1_input(struct mbuf *, struct tdb *);

extern int espdes_attach(void), espdes_init(struct tdb *, struct xformsw *, struct mbuf *), espdes_zeroize(struct tdb *);
extern int espdes_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *espdes_input(struct mbuf *, struct tdb *);

extern int esp3des_attach(void), esp3des_init(struct tdb *, struct xformsw *, struct mbuf *), esp3des_zeroize(struct tdb *);
extern int esp3des_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *esp3des_input(struct mbuf *, struct tdb *);

extern int espdesmd5_attach(void), espdesmd5_init(struct tdb *, struct xformsw *, struct mbuf *), espdesmd5_zeroize(struct tdb *);
extern int espdesmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *espdesmd5_input(struct mbuf *, struct tdb *);

extern int esp3desmd5_attach(void), esp3desmd5_init(struct tdb *, struct xformsw *, struct mbuf *), esp3desmd5_zeroize(struct tdb *);
extern int esp3desmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *esp3desmd5_input(struct mbuf *, struct tdb *);

extern caddr_t m_pad(struct mbuf *, int);
extern int checkreplaywindow32(u_int32_t, u_int32_t, u_int32_t *, u_int32_t, u_int32_t *);
extern int checkreplaywindow64(u_int64_t, u_int64_t *, u_int64_t, u_int64_t *);
#endif
