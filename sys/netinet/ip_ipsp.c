/*	$OpenBSD: ip_ipsp.c,v 1.15 1997/07/18 18:09:56 provos Exp $	*/

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
#include <sys/syslog.h>

int	tdb_init __P((struct tdb *, struct mbuf *));
int	ipsp_kern __P((int, char **, int));

int encdebug = 0;

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
    { XF_IP4,	         0,               "IPv4 Simple Encapsulation",
      ipe4_attach,       ipe4_init,       ipe4_zeroize,
      (struct mbuf * (*)(struct mbuf *, struct tdb *))ipe4_input, 
      ipe4_output, },
    { XF_OLD_AH,         XFT_AUTH,	  "Keyed Authentication, RFC 1828/1852",
      ah_old_attach,     ah_old_init,     ah_old_zeroize,
      ah_old_input,      ah_old_output, },
    { XF_OLD_ESP,        XFT_CONF,        "Simple Encryption, RFC 1829/1851",
      esp_old_attach,    esp_old_init,    esp_old_zeroize,
      esp_old_input,     esp_old_output, },
    { XF_NEW_AH,	 XFT_AUTH,	  "HMAC Authentication",
      ah_new_attach,	 ah_new_init,     ah_new_zeroize,
      ah_new_input,	 ah_new_output, },
    { XF_NEW_ESP,	 XFT_CONF|XFT_AUTH,
      "Encryption + Authentication + Replay Protection",
      esp_new_attach,	 esp_new_init,  esp_new_zeroize,
      esp_new_input,	 esp_new_output, },
};

struct xformsw *xformswNXFORMSW = &xformsw[sizeof(xformsw)/sizeof(xformsw[0])];

unsigned char ipseczeroes[IPSEC_ZEROES_SIZE]; /* zeroes! */ 


/*
 * Reserve an SPI; the SA is not valid yet though. Zero is reserved as
 * an error return value. If tspi is not zero, we try to allocate that
 * SPI. SPIs less than 255 are reserved, so we check for those too.
 */

u_int32_t
reserve_spi(u_int32_t tspi, struct in_addr src, u_int8_t proto, int *errval)
{
    struct tdb *tdbp;
    u_int32_t spi = tspi;		/* Don't change */
    
    while (1)
    {
	while (spi <= 255)		/* Get a new SPI */
	  get_random_bytes((void *) &spi, sizeof(spi));
	
	/* Check whether we're using this SPI already */
	if (gettdb(spi, src, proto) != (struct tdb *) NULL)
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

	bzero((caddr_t) tdbp, sizeof(*tdbp));
	
	tdbp->tdb_spi = spi;
	tdbp->tdb_dst = src;
	tdbp->tdb_sproto = proto;
	tdbp->tdb_flags |= TDBF_INVALID;

	puttdb(tdbp);
	
	return spi;
    }
}

/*
 * An IPSP SAID is really the concatenation of the SPI found in the 
 * packet, the destination address of the packet and the IPsec protocol.
 * When we receive an IPSP packet, we need to look up its tunnel descriptor
 * block, based on the SPI in the packet and the destination address (which
 * is really one of our addresses if we received the packet!
 */

struct tdb *
gettdb(u_int32_t spi, struct in_addr dst, u_int8_t proto)
{
    int hashval;
    struct tdb *tdbp;
	
    hashval = (spi + dst.s_addr + proto) % TDB_HASHMOD;
	
    for (tdbp = tdbh[hashval]; tdbp; tdbp = tdbp->tdb_hnext)
      if ((tdbp->tdb_spi == spi) && (tdbp->tdb_dst.s_addr == dst.s_addr)
	  && (tdbp->tdb_sproto == proto))
	break;
	
    return tdbp;
}

struct flow *
get_flow(void)
{
    struct flow *flow;

    MALLOC(flow, struct flow *, sizeof(struct flow), M_TDB, M_WAITOK);
    if (flow == (struct flow *) NULL)
      return (struct flow *) NULL;

    bzero(flow, sizeof(struct flow));

    return flow;
}

struct flow *
find_flow(struct in_addr src, struct in_addr srcmask, struct in_addr dst,
	  struct in_addr dstmask, u_int8_t proto, u_int16_t sport,
	  u_int16_t dport, struct tdb *tdb)
{
    struct flow *flow;

    for (flow = tdb->tdb_flow; flow; flow = flow->flow_next)
      if ((src.s_addr == flow->flow_src.s_addr) &&
	  (dst.s_addr == flow->flow_dst.s_addr) &&
	  (srcmask.s_addr == flow->flow_srcmask.s_addr) &&
	  (dstmask.s_addr == flow->flow_dstmask.s_addr) &&
	  (proto == flow->flow_proto) &&
	  (sport == flow->flow_sport) && (dport == flow->flow_dport))
	return flow;

    return (struct flow *) NULL;
}

struct flow *
find_global_flow(struct in_addr src, struct in_addr srcmask,
		 struct in_addr dst, struct in_addr dstmask,
		 u_int8_t proto, u_int16_t sport, u_int16_t dport)
{
    struct flow *flow;
    struct tdb *tdb;
    int i;

    for (i = 0; i < TDB_HASHMOD; i++)
      for (tdb = tdbh[i]; tdb; tdb = tdb->tdb_hnext)
	if ((flow = find_flow(src, srcmask, dst, dstmask, proto, sport,
			      dport, tdb)) != (struct flow *) NULL)
	  return flow;

    return (struct flow *) NULL;
}

void
puttdb(struct tdb *tdbp)
{
    int hashval;

    hashval = ((tdbp->tdb_sproto + tdbp->tdb_spi + tdbp->tdb_dst.s_addr)
	       % TDB_HASHMOD);
    tdbp->tdb_hnext = tdbh[hashval];
    tdbh[hashval] = tdbp;
}

void
put_flow(struct flow *flow, struct tdb *tdb)
{
    flow->flow_next = tdb->tdb_flow;
    flow->flow_prev = (struct flow *) NULL;

    tdb->tdb_flow = flow;

    flow->flow_sa = tdb;

    if (flow->flow_next)
      flow->flow_next->flow_prev = flow;
}

void
delete_flow(struct flow *flow, struct tdb *tdb)
{
    if (tdb->tdb_flow == flow)
    {
	tdb->tdb_flow = flow->flow_next;
	if (tdb->tdb_flow)
	  tdb->tdb_flow->flow_prev = (struct flow *) NULL;
    }
    else
    {
	flow->flow_prev->flow_next = flow->flow_next;
	if (flow->flow_next)
	  flow->flow_next->flow_prev = flow->flow_prev;
    }

    FREE(flow, M_TDB);
}

int
tdb_delete(struct tdb *tdbp, int delchain)
{
    struct tdb *tdbpp;
    struct flow *flow;
    int hashval;

    hashval = ((tdbp->tdb_sproto + tdbp->tdb_spi + tdbp->tdb_dst.s_addr)
	       % TDB_HASHMOD);

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

    /* If there was something before us in the chain, make it point nowhere */
    if (tdbp->tdb_inext)
      tdbp->tdb_inext->tdb_onext = NULL;

    tdbpp = tdbp->tdb_onext;

    if (tdbp->tdb_xform)
      (*(tdbp->tdb_xform->xf_zeroize))(tdbp);

    for (flow = tdbp->tdb_flow; flow; flow = tdbp->tdb_flow)
      delete_flow(flow, tdbp);
 
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

    log(LOG_ERR, "tdb_init(): no alg %d for spi %08x, addr %x, proto %d", alg,
	ntohl(tdbp->tdb_spi), tdbp->tdb_dst.s_addr, tdbp->tdb_sproto);

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
    return 0;
}
