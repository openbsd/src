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
	struct tdb	*tdb_hnext;	/* next in hash chain */
	struct tdb	*tdb_onext;	/* next in output */
	struct tdb	*tdb_inext;	/* next in input (prev!) */
	u_long		tdb_spi;	/* SPI to use */
	struct in_addr	tdb_dst;	/* dest address for this SPI */
	struct ifnet	*tdb_rcvif;	/* related rcv encap interface */
	struct xformsw	*tdb_xform;	/* transformation to use */
	caddr_t		tdb_xdata;	/* transformation data (opaque) */
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

extern struct tdb *gettdb(u_long, struct in_addr);
extern void puttdb(struct tdb *);
extern int tdb_delete(struct tdb *, int);

extern int ipe4_attach(void), ipe4_init(struct tdb *, struct xformsw *, struct mbuf *), ipe4_zeroize(struct tdb *);
extern int ipe4_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern void ipe4_input __P((struct mbuf *, ...));

extern int ahmd5_attach(void), ahmd5_init(struct tdb *, struct xformsw *, struct mbuf *), ahmd5_zeroize(struct tdb *);
extern int ahmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahmd5_input(struct mbuf *, struct tdb *);

extern int ahhmacmd5_attach(void), ahhmacmd5_init(struct tdb *, struct xformsw *, struct mbuf *), ahhmacmd5_zeroize(struct tdb *);
extern int ahhmacmd5_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahhmacmd5_input(struct mbuf *, struct tdb *);

extern int ahhmacsha1_attach(void), ahhmacsha1_init(struct tdb *, struct xformsw *, struct mbuf *), ahhmacsha1_zeroize(struct tdb *);
extern int ahhmacsha1_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *ahhmacsha1_input(struct mbuf *, struct tdb *);

extern int espdes_attach(void), espdes_init(struct tdb *, struct xformsw *, struct mbuf *), espdes_zeroize(struct tdb *);
extern int espdes_output(struct mbuf *, struct sockaddr_encap *, struct tdb *, struct mbuf **);
extern struct mbuf *espdes_input(struct mbuf *, struct tdb *);

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
