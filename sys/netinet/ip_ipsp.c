/*	$OpenBSD: ip_ipsp.c,v 1.12 1997/07/02 06:58:42 provos Exp $	*/

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
 * IPSP Processing
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <net/raw_cb.h>
#include <net/encap.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>

#include <dev/rndvar.h>

int	tdb_init __P((struct tdb *, struct mbuf *));
int	ipsp_kern __P((int, char **, int));

#ifdef ENCDEBUG
int encdebug = 1;
#endif

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
    { XF_IP4,	         0,               "IPv4 Simple Encapsulation",
      ipe4_attach,       ipe4_init,       ipe4_zeroize,
      (struct mbuf * (*)(struct mbuf *, struct tdb *))ipe4_input, 
      ipe4_output, },
    { XF_AHMD5,	         XFT_AUTH,	  "Keyed MD5 Authentication",
      ahmd5_attach,      ahmd5_init,      ahmd5_zeroize,
      ahmd5_input,       ahmd5_output, },
    { XF_AHSHA1,         XFT_AUTH,	  "Keyed SHA1 Authentication",
      ahsha1_attach,     ahsha1_init,     ahsha1_zeroize,
      ahsha1_input,      ahsha1_output, },
    { XF_ESPDES,         XFT_CONF,        "DES-CBC Encryption",
      espdes_attach,     espdes_init,     espdes_zeroize,
      espdes_input,      espdes_output, },
    { XF_ESP3DES,        XFT_CONF,        "3DES-CBC Encryption",
      esp3des_attach,    esp3des_init,    esp3des_zeroize,
      esp3des_input,     esp3des_output, },
    { XF_AHHMACMD5,	 XFT_AUTH,	  "HMAC MD5 Authentication",
      ahhmacmd5_attach,	 ahhmacmd5_init,  ahhmacmd5_zeroize,
      ahhmacmd5_input,	 ahhmacmd5_output, },
    { XF_AHHMACSHA1,	 XFT_AUTH,	  "HMAC SHA1 Authentication",
      ahhmacsha1_attach, ahhmacsha1_init, ahhmacsha1_zeroize,
      ahhmacsha1_input,	 ahhmacsha1_output, },
    { XF_ESPDESMD5,	 XFT_CONF|XFT_AUTH,     
      "DES-CBC Encryption + MD5 Authentication",
      espdesmd5_attach,	 espdesmd5_init,  espdesmd5_zeroize,
      espdesmd5_input,	 espdesmd5_output, },
    { XF_ESP3DESMD5,	 XFT_CONF|XFT_AUTH,     
      "3DES-CBC Encryption + MD5 Authentication",
      esp3desmd5_attach, esp3desmd5_init, esp3desmd5_zeroize,
      esp3desmd5_input,	 esp3desmd5_output, },
};

struct xformsw *xformswNXFORMSW = &xformsw[sizeof(xformsw)/sizeof(xformsw[0])];

unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */ 

static char *ipspkernfs = NULL;
int ipspkernfs_dirty = 1;

/*
 * Reserve an SPI; the SA is not valid yet though. Zero is reserved as
 * an error return value. If tspi is not zero, we try to allocate that
 * SPI. SPIs less than 255 are reserved, so we check for those too.
 */

u_int32_t
reserve_spi(u_int32_t tspi, struct in_addr src, int *errval)
{
    struct tdb *tdbp;
    u_int32_t spi = tspi;		/* Don't change */
    
    while (1)
    {
	while (spi <= 255)		/* Get a new SPI */
	  get_random_bytes((void *)&spi, sizeof(spi));
	
	/* Check whether we're using this SPI already */
	if (gettdb(spi, src) != (struct tdb *) NULL)
	{
	    if (tspi != 0)		/* If one was proposed, report error */
	    {
		(*errval) = EEXIST;
	      	return 0;
	    }

	    spi = 0;
	    continue;
	}
	
	MALLOC(tdbp, struct tdb *, sizeof(*tdbp), M_TDB, M_WAITOK);
	if (tdbp == NULL)
	{
	    spi = 0;
	    (*errval) = ENOBUFS;
	} 

	bzero((caddr_t)tdbp, sizeof(*tdbp));
	
	tdbp->tdb_spi = spi;
	tdbp->tdb_dst = src;
	tdbp->tdb_flags |= TDBF_INVALID;

	puttdb(tdbp);
	
	return spi;
    }
}

/*
 * An IPSP SAID is really the concatenation of the SPI found in the 
 * packet and the destination address of the packet. When we receive
 * an IPSP packet, we need to look up its tunnel descriptor block, 
 * based on the SPI in the packet and the destination address (which is
 * really one of our addresses if we received the packet!
 */

struct tdb *
gettdb(u_int32_t spi, struct in_addr dst)
{
    int hashval;
    struct tdb *tdbp;
	
    hashval = (spi+dst.s_addr) % TDB_HASHMOD;
	
    for (tdbp = tdbh[hashval]; tdbp; tdbp = tdbp->tdb_hnext)
      if ((tdbp->tdb_spi == spi) && (tdbp->tdb_dst.s_addr == dst.s_addr))
	break;
	
    return tdbp;
}

void
puttdb(struct tdb *tdbp)
{
    int hashval;

    hashval = ((tdbp->tdb_spi + tdbp->tdb_dst.s_addr) % TDB_HASHMOD);
    tdbp->tdb_hnext = tdbh[hashval];
    tdbh[hashval] = tdbp;

    ipspkernfs_dirty = 1;
}

int
tdb_delete(struct tdb *tdbp, int delchain)
{
    struct tdb *tdbpp;
    int hashval;

    hashval = ((tdbp->tdb_spi + tdbp->tdb_dst.s_addr) % TDB_HASHMOD);

    if (tdbh[hashval] == tdbp)
    {
	tdbpp = tdbp;
	tdbh[hashval] = tdbp->tdb_hnext;
    }
    else
      for (tdbpp = tdbh[hashval]; tdbpp != NULL; tdbpp = tdbpp->tdb_hnext)
	if (tdbpp->tdb_hnext == tdbp)
	{
	    tdbpp->tdb_hnext = tdbp->tdb_hnext;
	    tdbpp = tdbp;
	}

    if (tdbp != tdbpp)
      return EINVAL;		/* Should never happen */
	
    ipspkernfs_dirty = 1;
    tdbpp = tdbp->tdb_onext;

    if (tdbp->tdb_xform)
      (*(tdbp->tdb_xform->xf_zeroize))(tdbp);

    FREE(tdbp, M_TDB);
    if (delchain && tdbpp)
      return tdb_delete(tdbpp, delchain);
    else
      return 0;
}

int
tdb_init(struct tdb *tdbp, struct mbuf *m)
{
    int alg;
    struct encap_msghdr *em;
    struct xformsw *xsp;
	
    em = mtod(m, struct encap_msghdr *);
    alg = em->em_alg;

    for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++)
      if (xsp->xf_type == alg)
	return (*(xsp->xf_init))(tdbp, xsp, m);

#ifdef ENCDEBUG
    if (encdebug)
      printf("tdbinit: no alg %d for spi %x, addr %x\n", alg, tdbp->tdb_spi,
	     ntohl(tdbp->tdb_dst.s_addr));
#endif

    /* Record establishment time */
    tdbp->tdb_established = time.tv_sec;
    
    m_freem(m);
    return EINVAL;
}

/*
 * XXX This should change to something cleaner.
 */
int
ipsp_kern(int off, char **bufp, int len)
{
    struct tdb *tdbp;
    int i, k;
    char *b;

    if (off != 0)
      return 0;

    if ((!ipspkernfs_dirty) && (ipspkernfs))
    {
	*bufp = ipspkernfs;
	return strlen(ipspkernfs);
    }
    else
      ipspkernfs_dirty = 0;

    if (ipspkernfs)
    {
      	FREE(ipspkernfs, M_XDATA);
	ipspkernfs = NULL;
    }

    for (i = 0, k = 0; i < TDB_HASHMOD; i++)
      for (tdbp = tdbh[i]; tdbp != (struct tdb *) NULL; tdbp = tdbp->tdb_hnext)
      {
	  /* Being paranoid to avoid buffer overflows */

	  if (tdbp->tdb_xform)
            k += 126 + strlen(tdbp->tdb_xform->xf_name);
	  else
	    k += 60;
      }

    if (k == 0)
      return 0;

    MALLOC(ipspkernfs, char *, k + 1, M_XDATA, M_DONTWAIT);
    if (!ipspkernfs)
      return 0;

    for (i = 0, k = 0; i < TDB_HASHMOD; i++)
      for (tdbp = tdbh[i]; tdbp != (struct tdb *) NULL; tdbp = tdbp->tdb_hnext)
      {
	  b = (char *)&(tdbp->tdb_dst.s_addr);
	  if (!tdbp->tdb_xform)
	    k += sprintf(ipspkernfs + k, "SPI=%x, destination=%d.%d.%d.%d\n",
			 tdbp->tdb_spi, ((int)b[0] & 0xff), ((int)b[1] & 0xff), ((int)b[2] & 0xff), ((int)b[3] & 0xff));
	  else
	    k += sprintf(ipspkernfs + k, 
		         "SPI=%x, destination=%d.%d.%d.%d\n algorithm=%d (%s)\n next SPI=%x, previous SPI=%x\n", 
		         ntohl(tdbp->tdb_spi), ((int)b[0] & 0xff), ((int)b[1] & 0xff), 
		         ((int)b[2] & 0xff), ((int)b[3] & 0xff), 
		         tdbp->tdb_xform->xf_type, tdbp->tdb_xform->xf_name,
		         (tdbp->tdb_onext ? ntohl(tdbp->tdb_onext->tdb_spi) : 0),
		         (tdbp->tdb_inext ? ntohl(tdbp->tdb_inext->tdb_spi) : 0));
      }

    ipspkernfs[k] = '\0';
    *bufp = ipspkernfs;
    return strlen(ipspkernfs);
}
