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

#define IPSEC_IPSP_C
#include <netinet/ip_ipsp.h>
#undef IPSEC_IPSP_C
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>

#ifdef ENCDEBUG
int encdebug = 1;
#endif

/*
 * This is the proper place to define the various encapsulation transforms.
 * CAUTION: the human-readable string should be LESS than 200 bytes if the
 * kernfs is to work properly.
 */

struct xformsw xformsw[] = {
{ XF_IP4,		0,		"IPv4 Simple Encapsulation",
  ipe4_attach,		ipe4_init,	ipe4_zeroize,
  (struct mbuf * (*)(struct mbuf *, struct tdb *))ipe4_input,		ipe4_output, },
{ XF_AHMD5,		XFT_AUTH,	"Keyed MD5 Authentication",
  ahmd5_attach,		ahmd5_init,	ahmd5_zeroize,
  ahmd5_input,		ahmd5_output, },
{ XF_ESPDES,		XFT_CONF,	"DES-CBC Encryption",
  espdes_attach,	espdes_init,	espdes_zeroize,
  espdes_input,	espdes_output, },
{ XF_AHHMACMD5,		XFT_AUTH,	"HMAC MD5 Authentication",
  ahhmacmd5_attach,	ahhmacmd5_init,	ahhmacmd5_zeroize,
  ahhmacmd5_input,	ahhmacmd5_output, },
{ XF_AHHMACSHA1,	XFT_AUTH,	"HMAC SHA1 Authentication",
  ahhmacsha1_attach,	ahhmacsha1_init, ahhmacsha1_zeroize,
  ahhmacsha1_input,	ahhmacsha1_output, },
{ XF_ESPDESMD5,		XFT_CONF,     "DES-CBC Encryption + MD5 Authentication",
  espdesmd5_attach,	espdesmd5_init,	espdesmd5_zeroize,
  espdesmd5_input,	espdesmd5_output, },
{ XF_ESP3DESMD5,	XFT_CONF,     "3DES-CBC Encryption + MD5 Authentication",
  esp3desmd5_attach,	esp3desmd5_init,	esp3desmd5_zeroize,
  esp3desmd5_input,	esp3desmd5_output, },
};

struct xformsw *xformswNXFORMSW = &xformsw[sizeof(xformsw)/sizeof(xformsw[0])];

unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */ 

static char *ipspkernfs = NULL;
static int ipspkernfs_len = 0;
int ipspkernfs_dirty = 1;

/*
 * An IPSP SAID is really the concatenation of the SPI found in the 
 * packet and the destination address of the packet. When we receive
 * an IPSP packet, we need to look up its tunnel descriptor block, 
 * based on the SPI in the packet and the destination address (which is
 * really one of our addresses if we received the packet!
 */

struct tdb *
gettdb(u_long spi, struct in_addr dst)
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

	printf("tdbinit: no alg %d for spi %x, addr %x\n", alg, tdbp->tdb_spi, ntohl(tdbp->tdb_dst.s_addr));
	
	m_freem(m);
	return EINVAL;
}


int
ipsp_kern(int off, char **bufp, int len)
{
    struct tdb *tdbp;
    int i, k;
    char *b, buf[512];

    if (off != 0)
      return 0;

    if ((!ipspkernfs_dirty) && (ipspkernfs))
    {
	*bufp = ipspkernfs;
	return ipspkernfs_len;
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

	  if (strlen(tdbp->tdb_xform->xf_name) >= 200)
	    return 0;

	  b = (char *)&(tdbp->tdb_dst.s_addr);
	  k += sprintf(buf, 
		"SPI=%x, destination=%d.%d.%d.%d, interface=%s\n algorithm=%d (%s)\n next SPI=%x, previous SPI=%x\n", 
		ntohl(tdbp->tdb_spi), ((int)b[0] & 0xff), ((int)b[1] & 0xff), 
		((int)b[2] & 0xff), ((int)b[3] & 0xff), 
		(tdbp->tdb_rcvif ? tdbp->tdb_rcvif->if_xname : "none"),
		tdbp->tdb_xform->xf_type, tdbp->tdb_xform->xf_name,
		(tdbp->tdb_onext ? ntohl(tdbp->tdb_onext->tdb_spi) : 0),
		(tdbp->tdb_inext ? ntohl(tdbp->tdb_inext->tdb_spi) : 0));
      }

    if (k == 0)
      return 0;

    MALLOC(ipspkernfs, char *, k + 1, M_XDATA, M_DONTWAIT);
    if (!ipspkernfs)
      return 0;

    ipspkernfs_len = k + 1;

    for (i = 0, k = 0; i < TDB_HASHMOD; i++)
      for (tdbp = tdbh[i]; tdbp != (struct tdb *) NULL; tdbp = tdbp->tdb_hnext)
      {
	  b = (char *)&(tdbp->tdb_dst.s_addr);
	  k += sprintf(ipspkernfs + k, 
		"SPI=%x, destination=%d.%d.%d.%d, interface=%s\n algorithm=%d (%s)\n next SPI=%x, previous SPI=%x\n", 
		ntohl(tdbp->tdb_spi), ((int)b[0] & 0xff), ((int)b[1] & 0xff), 
		((int)b[2] & 0xff), ((int)b[3] & 0xff), 
		(tdbp->tdb_rcvif ? tdbp->tdb_rcvif->if_xname : "none"),
		tdbp->tdb_xform->xf_type, tdbp->tdb_xform->xf_name,
		(tdbp->tdb_onext ? ntohl(tdbp->tdb_onext->tdb_spi) : 0),
		(tdbp->tdb_inext ? ntohl(tdbp->tdb_inext->tdb_spi) : 0));
      }

    ipspkernfs[k] = '\0';
    *bufp = ipspkernfs;
    return ipspkernfs_len;
}
