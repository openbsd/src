/*	$OpenBSD: ip_ipsp.c,v 1.35 1999/02/25 22:37:29 angelos Exp $	*/

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
#include <sys/proc.h>

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
#include <net/pfkeyv2.h>

#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>

#include <dev/rndvar.h>

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

int		ipsp_kern __P((int, char **, int));
u_int8_t       	get_sa_require  __P((struct inpcb *));
int		check_ipsec_policy  __P((struct inpcb *, u_int32_t));

extern int	ipsec_auth_default_level;
extern int	ipsec_esp_trans_default_level;
extern int	ipsec_esp_network_default_level;

int encdebug = 0;
int ipsec_in_use = 0;
u_int32_t kernfs_epoch = 0;

u_int8_t hmac_ipad_buffer[64] = {
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 
    0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36 };

u_int8_t hmac_opad_buffer[64] = {
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C,
    0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C, 0x5C };

/*
 * This is the proper place to define the various encapsulation transforms.
 */

struct xformsw xformsw[] = {
    { XF_IP4,	         0,               "IPv4 Simple Encapsulation",
      ipe4_attach,       ipe4_init,       ipe4_zeroize,
      (struct mbuf * (*)(struct mbuf *, struct tdb *))ipe4_input, 
      ipe4_output, },
    { XF_OLD_AH,         XFT_AUTH,	 "Keyed Authentication, RFC 1828/1852",
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
 * Check which transformationes are required
 */

u_int8_t
get_sa_require(struct inpcb *inp)
{
    u_int8_t sareq = 0;
       
    if (inp != NULL)
    {
	sareq |= inp->inp_seclevel[SL_AUTH] >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_AUTH : 0;
	sareq |= inp->inp_seclevel[SL_ESP_TRANS] >= IPSEC_LEVEL_USE ?
		 NOTIFY_SATYPE_CONF : 0;
	sareq |= inp->inp_seclevel[SL_ESP_NETWORK] >= IPSEC_LEVEL_USE ?
		 NOTIFY_SATYPE_TUNNEL : 0;
    }
    else
    {
	sareq |= ipsec_auth_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_AUTH : 0;
	sareq |= ipsec_esp_trans_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_CONF : 0;
	sareq |= ipsec_esp_network_default_level >= IPSEC_LEVEL_USE ? 
		 NOTIFY_SATYPE_TUNNEL : 0;
    }
    
    return (sareq);
}

/*
 * Check the socket policy and request a new SA with a key management
 * daemon. Sometime the inp does not contain the destination address
 * in that case use dst.
 */

int
check_ipsec_policy(struct inpcb *inp, u_int32_t daddr)
{
    union sockaddr_union sunion;
    struct socket *so;
    struct route_enc re0, *re = &re0;
    struct sockaddr_encap *dst; 
    u_int8_t sa_require, sa_have;
    int error, i;

    if (inp == NULL || ((so = inp->inp_socket) == 0))
      return (EINVAL);

    /* If IPSEC is not required just use what we got */
    if (!(sa_require = inp->inp_secrequire))
      return 0;

    bzero((caddr_t) re, sizeof(*re));
    dst = (struct sockaddr_encap *) &re->re_dst;
    dst->sen_family = PF_KEY;
    dst->sen_len = SENT_IP4_LEN;
    dst->sen_type = SENT_IP4;
    dst->sen_ip_src = inp->inp_laddr;
    dst->sen_ip_dst.s_addr = inp->inp_faddr.s_addr ? 
			     inp->inp_faddr.s_addr : daddr;
    dst->sen_proto = so->so_proto->pr_protocol;
    switch (dst->sen_proto)
    {
	case IPPROTO_UDP:
	case IPPROTO_TCP:
	    dst->sen_sport = inp->inp_lport;
	    dst->sen_dport = inp->inp_fport;
	    break;
	default:
	    dst->sen_sport = 0;
	    dst->sen_dport = 0;
    }
    
    /* Try to find a flow */
    rtalloc((struct route *) re);
    
    if (re->re_rt != NULL)
    {
	struct tdb *tdb;
	struct sockaddr_encap *gw;
		
	gw = (struct sockaddr_encap *) (re->re_rt->rt_gateway);

	if (gw->sen_type == SENT_IPSP) {
	    sunion.sin.sin_family = AF_INET;
	    sunion.sin.sin_len = sizeof(struct sockaddr_in);
	    sunion.sin.sin_addr = gw->sen_ipsp_dst;
	    
	    tdb = (struct tdb *) gettdb(gw->sen_ipsp_spi, &sunion,
					gw->sen_ipsp_sproto);

	    SPI_CHAIN_ATTRIB(sa_have, tdb_onext, tdb);
	}
	else 
	  sa_have = 0;
	
	RTFREE(re->re_rt);
	
	/* Check if our requirements are met */
	if (!(sa_require & ~sa_have))
	  return 0;
    }
    else
      sa_have = 0;

    error = i = 0;

    inp->inp_secresult = SR_WAIT;

    /* If necessary try to notify keymanagement three times */
    while (i < 3)
    {
	/* XXX address */
	DPRINTF(("ipsec: send SA request (%d), remote ip: %s, SA type: %d\n",
		 i + 1, inet_ntoa4(dst->sen_ip_dst), sa_require));

	/* Send notify */
	/* XXX PF_KEYv2 Notify */
	 
	/* 
	 * Wait for the keymanagement daemon to establich a new SA,
	 * even on error check again, perhaps some other process
	 * already established the necessary SA.
	 */
	error = tsleep((caddr_t)inp, PSOCK|PCATCH, "ipsecnotify", 30*hz);
	DPRINTF(("check_ipsec: sleep %d\n", error));

	if (error && error != EWOULDBLOCK)
	  break;
	/* 
	 * A Key Management daemon returned an apropriate SA back
	 * to the kernel, the kernel noted that state in the waiting
	 * socket.
	 */
	if (inp->inp_secresult == SR_SUCCESS)
	  return (0);
	/*
	 * Key Management returned a permanent failure, we do not
	 * need to retry again. XXX - when more than one key
	 * management daemon is available we can not do that.
	 */
	if (inp->inp_secresult == SR_FAILED)
	  break;
	i++;
    }

    return (error ? error : EWOULDBLOCK);
}

/*
 * Reserve an SPI; the SA is not valid yet though. Zero is reserved as
 * an error return value. If tspi is not zero, we try to allocate that
 * SPI.
 */

u_int32_t
reserve_spi(u_int32_t sspi, u_int32_t tspi, union sockaddr_union *src,
	    union sockaddr_union *dst, u_int8_t sproto, int *errval)
{
    struct tdb *tdbp;
    u_int32_t spi;
    int nums;

    if (tspi <= 255)   /* We don't reserve 0 < SPI <= 255 */
    {
	(*errval) = EEXIST;
	return 0;
    }
    
    if ((sspi == tspi) && (sspi != 0))   /* Asking for a specific SPI */
      nums = 1;
    else
      nums = 50;  /* XXX figure out some good value */

    while (nums--)
    {
	if (tspi != 0) /* SPIRANGE was defined */
	{
	    if (sspi == tspi)  /* Specific SPI asked */
	      spi = tspi;
	    else    /* Range specified */
	    {
		get_random_bytes((void *) &spi, sizeof(spi));
		spi = sspi + (spi % (tspi - sspi));
	    }
	}
	else  /* Some SPI */
	  get_random_bytes((void *) &spi, sizeof(spi));
	  
	if (spi <= 255) /* Don't allocate SPI <= 255, they're reserved */
	  continue;
	else
	  spi = htonl(spi);

	/* Check whether we're using this SPI already */
	if (gettdb(spi, dst, sproto) != (struct tdb *) NULL)
	  continue;
	
	MALLOC(tdbp, struct tdb *, sizeof(struct tdb), M_TDB, M_WAITOK);
	bzero((caddr_t) tdbp, sizeof(struct tdb));
	
	tdbp->tdb_spi = spi;
	bcopy(&dst->sa, &tdbp->tdb_dst.sa, SA_LEN(&dst->sa));
	bcopy(&src->sa, &tdbp->tdb_src.sa, SA_LEN(&src->sa));
	tdbp->tdb_sproto = sproto;
	tdbp->tdb_flags |= TDBF_INVALID;       /* Mark SA as invalid for now */
	tdbp->tdb_established = time.tv_sec;
	tdbp->tdb_epoch = kernfs_epoch - 1;
	puttdb(tdbp);

	/* XXX Should set up a silent expiration for this */
	
	return spi;
    }

    (*errval) = EEXIST;
    return 0;
}

/*
 * An IPSP SAID is really the concatenation of the SPI found in the 
 * packet, the destination address of the packet and the IPsec protocol.
 * When we receive an IPSP packet, we need to look up its tunnel descriptor
 * block, based on the SPI in the packet and the destination address (which
 * is really one of our addresses if we received the packet!
 */

struct tdb *
gettdb(u_int32_t spi, union sockaddr_union *dst, u_int8_t proto)
{
    u_int8_t *ptr = (u_int8_t *) dst;
    u_int32_t hashval = proto + spi;
    struct tdb *tdbp;
    int i;

    for (i = 0; i < SA_LEN(&dst->sa); i++)
      hashval += ptr[i];
    
    hashval %= TDB_HASHMOD;

    for (tdbp = tdbh[hashval]; tdbp; tdbp = tdbp->tdb_hnext)
      if ((tdbp->tdb_spi == spi) && 
	  !bcmp(&tdbp->tdb_dst, dst, SA_LEN(&dst->sa)) &&
	  (tdbp->tdb_sproto == proto))
	break;
	
    return tdbp;
}

struct flow *
get_flow(void)
{
    struct flow *flow;

    MALLOC(flow, struct flow *, sizeof(struct flow), M_TDB, M_WAITOK);
    bzero(flow, sizeof(struct flow));

    return flow;
}

struct expiration *
get_expiration(void)
{
    struct expiration *exp;
    
    MALLOC(exp, struct expiration *, sizeof(struct expiration), M_TDB,
	   M_WAITOK);

    bzero(exp, sizeof(struct expiration));
    
    return exp;
}

void
cleanup_expirations(union sockaddr_union *dst, u_int32_t spi, u_int8_t sproto)
{
    struct expiration *exp, *nexp;

    for (exp = explist; exp; exp = (exp ? exp->exp_next : explist))
      if (!bcmp(&exp->exp_dst, dst, SA_LEN(&dst->sa)) &&
	  (exp->exp_spi == spi) && (exp->exp_sproto == sproto))
      {
	  /* Link previous to next */
	  if (exp->exp_prev == (struct expiration *) NULL)
	    explist = exp->exp_next;
	  else
	    exp->exp_prev->exp_next = exp->exp_next;
	  
	  /* Link next (if it exists) to previous */
	  if (exp->exp_next != (struct expiration *) NULL)
	    exp->exp_next->exp_prev = exp->exp_prev;

	  nexp = exp;
	  exp = exp->exp_prev;
	  FREE(nexp, M_TDB);
      }
}

void 
handle_expirations(void *arg)
{
    struct expiration *exp;
    struct tdb *tdb;
    
    if (explist == (struct expiration *) NULL)
      return;
    
    while (1)
    {
	exp = explist;

	if (exp == (struct expiration *) NULL)
	  return;
	else
	  if (exp->exp_timeout > time.tv_sec)
	    break;
	
	/* Advance pointer */
	explist = explist->exp_next;
	if (explist)
	  explist->exp_prev = NULL;
	
	tdb = gettdb(exp->exp_spi, &exp->exp_dst, exp->exp_sproto);
	if (tdb == (struct tdb *) NULL)
	{
	    free(exp, M_TDB);
	    continue;			/* TDB is gone, ignore this */
	}
	
	/* Hard expirations first */
	if ((tdb->tdb_flags & TDBF_TIMER) &&
	    (tdb->tdb_exp_timeout <= time.tv_sec))
	{
/* XXX
	      encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
*/
	    tdb_delete(tdb, 0);
	}
	else
	  if ((tdb->tdb_flags & TDBF_FIRSTUSE) &&
	      (tdb->tdb_first_use + tdb->tdb_exp_first_use <= time.tv_sec))
	  {
/* XXX
   encap_sendnotify(NOTIFY_HARD_EXPIRE, tdb, NULL);
*/
	      tdb_delete(tdb, 0);
	  }

	/* Soft expirations */
	if ((tdb->tdb_flags & TDBF_SOFT_TIMER) &&
	    (tdb->tdb_soft_timeout <= time.tv_sec))
	{
/* XXX
   encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
*/
	    tdb->tdb_flags &= ~TDBF_SOFT_TIMER;
	}
	else
	  if ((tdb->tdb_flags & TDBF_SOFT_FIRSTUSE) &&
	      (tdb->tdb_first_use + tdb->tdb_soft_first_use <=
	       time.tv_sec))
	  {
/* XXX
   encap_sendnotify(NOTIFY_SOFT_EXPIRE, tdb, NULL);
*/
	      tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
	  }

	free(exp, M_TDB);
    }

    if (explist)
      timeout(handle_expirations, (void *) NULL, 
	      hz * (explist->exp_timeout - time.tv_sec));
}

void
put_expiration(struct expiration *exp)
{
    struct expiration *expt;
    int reschedflag = 0;
    
    if (exp == (struct expiration *) NULL)
    {
	DPRINTF(("put_expiration(): NULL argument\n"));
	return;
    }
    
    if (explist == (struct expiration *) NULL)
    {
	explist = exp;
	reschedflag = 1;
    }
    else
      if (explist->exp_timeout > exp->exp_timeout)
      {
	  exp->exp_next = explist;
	  explist->exp_prev = exp;
	  explist = exp;
	  reschedflag = 2;
      }
      else
      {
	  for (expt = explist; expt->exp_next; expt = expt->exp_next)
	    if (expt->exp_next->exp_timeout > exp->exp_timeout)
	    {
		expt->exp_next->exp_prev = exp;
		exp->exp_next = expt->exp_next;
		expt->exp_next = exp;
		exp->exp_prev = expt;
		break;
	    }

	  if (expt->exp_next == (struct expiration *) NULL)
	  {
	      expt->exp_next = exp;
	      exp->exp_prev = expt;
	  }
      }

    switch (reschedflag)
    {
	case 1:
	    timeout(handle_expirations, (void *) NULL, 
		    hz * (explist->exp_timeout - time.tv_sec));
	    break;
	    
	case 2:
	    untimeout(handle_expirations, (void *) NULL);
	    timeout(handle_expirations, (void *) NULL,
		    hz * (explist->exp_timeout - time.tv_sec));
	    break;
	    
	default:
	    break;
    }
}

struct flow *
find_flow(union sockaddr_union *src, union sockaddr_union *srcmask,
	  union sockaddr_union * dst, union sockaddr_union *dstmask,
	  u_int8_t proto, struct tdb *tdb)
{
    struct flow *flow;

    for (flow = tdb->tdb_flow; flow; flow = flow->flow_next)
      if (!bcmp(&src->sa, &flow->flow_src.sa, SA_LEN(&src->sa)) &&
	  !bcmp(&dst->sa, &flow->flow_dst.sa, SA_LEN(&dst->sa)) &&
	  !bcmp(&srcmask->sa, &flow->flow_srcmask.sa, SA_LEN(&srcmask->sa)) &&
	  !bcmp(&dstmask->sa, &flow->flow_dstmask.sa, SA_LEN(&dstmask->sa)) &&
	  (proto == flow->flow_proto))
	return flow;

    return (struct flow *) NULL;
}

struct flow *
find_global_flow(union sockaddr_union *src, union sockaddr_union *srcmask,
		 union sockaddr_union *dst, union sockaddr_union *dstmask,
		 u_int8_t proto)
{
    struct flow *flow;
    struct tdb *tdb;
    int i;

    for (i = 0; i < TDB_HASHMOD; i++)
      for (tdb = tdbh[i]; tdb; tdb = tdb->tdb_hnext)
	if ((flow = find_flow(src, srcmask, dst, dstmask, proto, tdb)) !=
	    (struct flow *) NULL)
	  return flow;

    return (struct flow *) NULL;
}

void
puttdb(struct tdb *tdbp)
{
    u_int8_t *ptr = (u_int8_t *) &tdbp->tdb_dst;
    u_int32_t hashval = tdbp->tdb_sproto + tdbp->tdb_spi, i;

    for (i = 0; i < SA_LEN(&tdbp->tdb_dst.sa); i++)
      hashval += ptr[i];
    
    hashval %= TDB_HASHMOD;
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
    if (tdb)
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
    }

    FREE(flow, M_TDB);
}

int
tdb_delete(struct tdb *tdbp, int delchain)
{
    u_int8_t *ptr = (u_int8_t *) &tdbp->tdb_dst;
    struct tdb *tdbpp;
    struct flow *flow;
    u_int32_t hashval = tdbp->tdb_sproto + tdbp->tdb_spi, i;

    for (i = 0; i < SA_LEN(&tdbp->tdb_dst.sa); i++)
      hashval += ptr[i];

    hashval %= TDB_HASHMOD;

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

    /*
     * If there was something before us in the chain pointing to us,
     * make it point nowhere
     */
    if ((tdbp->tdb_inext) &&
	(tdbp->tdb_inext->tdb_onext == tdbp))
      tdbp->tdb_inext->tdb_onext = NULL;

    /* 
     * If there was something after us in the chain pointing to us,
     * make it point nowhere
     */
    if ((tdbp->tdb_onext) &&
	(tdbp->tdb_onext->tdb_inext == tdbp))
      tdbp->tdb_onext->tdb_inext = NULL;
    
    tdbpp = tdbp->tdb_onext;
    
    if (tdbp->tdb_xform)
      (*(tdbp->tdb_xform->xf_zeroize))(tdbp);

    for (flow = tdbp->tdb_flow; flow; flow = tdbp->tdb_flow)
    {
	delete_flow(flow, tdbp);
	ipsec_in_use--;
    }

    cleanup_expirations(&tdbp->tdb_dst, tdbp->tdb_spi, tdbp->tdb_sproto);

    if (tdbp->tdb_srcid)
      FREE(tdbp->tdb_srcid, M_XDATA);

    if (tdbp->tdb_dstid)
      FREE(tdbp->tdb_dstid, M_XDATA);

    FREE(tdbp, M_TDB);

    if (delchain && tdbpp)
      return tdb_delete(tdbpp, delchain);
    else
      return 0;
}

int
tdb_init(struct tdb *tdbp, u_int16_t alg, struct ipsecinit *ii)
{
    struct xformsw *xsp;

    /* Record establishment time */
    tdbp->tdb_established = time.tv_sec;
    tdbp->tdb_epoch = kernfs_epoch - 1;

    for (xsp = xformsw; xsp < xformswNXFORMSW; xsp++)
      if (xsp->xf_type == alg)
	return (*(xsp->xf_init))(tdbp, xsp, ii);

    DPRINTF(("tdb_init(): no alg %d for spi %08x, addr %s, proto %d\n", 
	     alg, ntohl(tdbp->tdb_spi), ipsp_address(tdbp->tdb_dst),
	     tdbp->tdb_sproto));
    
    return EINVAL;
}

/*
 * Used by kernfs
 */
int
ipsp_kern(int off, char **bufp, int len)
{
    static char buffer[IPSEC_KERNFS_BUFSIZE];
    struct flow *flow;
    struct tdb *tdb;
    int l, i;

    if (off == 0)
      kernfs_epoch++;
    
    if (bufp == NULL)
      return 0;

    bzero(buffer, IPSEC_KERNFS_BUFSIZE);

    *bufp = buffer;
    
    for (i = 0; i < TDB_HASHMOD; i++)
      for (tdb = tdbh[i]; tdb; tdb = tdb->tdb_hnext)
	if (tdb->tdb_epoch != kernfs_epoch)
	{
	    tdb->tdb_epoch = kernfs_epoch;

	    l = sprintf(buffer, "SPI = %08x, Destination = %s, Sproto = %u\n",
			ntohl(tdb->tdb_spi),
			ipsp_address(tdb->tdb_dst), tdb->tdb_sproto);

	    l += sprintf(buffer + l, "\tEstablished %d seconds ago\n",
			 time.tv_sec - tdb->tdb_established);

	    l += sprintf(buffer + l, "\tSource = %s",
			 ipsp_address(tdb->tdb_src));

	    if (tdb->tdb_proxy.sa.sa_family)
	      l += sprintf(buffer + l, ", Proxy = %s\n",
			   ipsp_address(tdb->tdb_proxy));
	    else
	      l += sprintf(buffer + l, "\n");

	    l += sprintf(buffer + l, "\tFlags (%08x) = <", tdb->tdb_flags);

	    if ((tdb->tdb_flags & ~(TDBF_TIMER | TDBF_BYTES |
				    TDBF_ALLOCATIONS | TDBF_FIRSTUSE |
				    TDBF_SOFT_TIMER | TDBF_SOFT_BYTES |
				    TDBF_SOFT_FIRSTUSE |
				    TDBF_SOFT_ALLOCATIONS)) == 0)
	      l += sprintf(buffer + l, "none>\n");
	    else
	    {
		/* We can reuse variable 'i' here, since we're not looping */
		i = 0;

		if (tdb->tdb_flags & TDBF_UNIQUE)
		{
		    if (i)
		      l += sprintf(buffer + l, ", ");
		    else
		      i = 1;

		    l += sprintf(buffer + l, "unique");
		    i = 1;
		}

		if (tdb->tdb_flags & TDBF_INVALID)
		{
		    if (i)
		      l += sprintf(buffer + l, ", ");
		    else
		      i = 1;

		    l += sprintf(buffer + l, "invalid");
		}

		if (tdb->tdb_flags & TDBF_HALFIV)
		{
		    if (i)
		      l += sprintf(buffer + l, ", ");
		    else
		      i = 1;

		    l += sprintf(buffer + l, "halviv");
		}

		if (tdb->tdb_flags & TDBF_PFS)
		{
		    if (i)
		      l += sprintf(buffer + l, ", ");
		    else
		      i = 1;

		    l += sprintf(buffer + l, "pfs");
		}

		if (tdb->tdb_flags & TDBF_TUNNELING)
		{
		    if (i)
		      l += sprintf(buffer + l, ", ");
		    else
		      i = 1;

		    l += sprintf(buffer + l, "tunneling");
		}

		l += sprintf(buffer + l, ">\n");
	    }

	    if (tdb->tdb_xform)
	      l += sprintf(buffer + l, "\txform = <%s>\n", 
			   tdb->tdb_xform->xf_name);

	    if (tdb->tdb_encalgxform)
	      l += sprintf(buffer + l, "\t\tEncryption = <%s>\n",
			   tdb->tdb_encalgxform->name);

	    if (tdb->tdb_authalgxform)
	      l += sprintf(buffer + l, "\t\tAuthentication = <%s>\n",
			   tdb->tdb_authalgxform->name);

	    if (tdb->tdb_onext)
	      l += sprintf(buffer + l,
			   "\tNext SA: SPI = %08x, "
			   "Destination = %s, Sproto = %u\n",
			   ntohl(tdb->tdb_onext->tdb_spi),
			   ipsp_address(tdb->tdb_onext->tdb_dst),
			   tdb->tdb_onext->tdb_sproto);

	    if (tdb->tdb_inext)
	      l += sprintf(buffer + l,
			   "\tPrevious SA: SPI = %08x, "
			   "Destination = %s, Sproto = %u\n",
			   ntohl(tdb->tdb_inext->tdb_spi),
			   ipsp_address(tdb->tdb_inext->tdb_dst),
			   tdb->tdb_inext->tdb_sproto);

	    for (i = 0, flow = tdb->tdb_flow; flow; flow = flow->flow_next)
	      i++;

	    l+= sprintf(buffer + l, "\tCurrently used by %d flows\n", i);

	    l += sprintf(buffer + l, "\t%u flows have used this SA\n",
			 tdb->tdb_cur_allocations);
	    
	    l += sprintf(buffer + l, "\t%qu bytes processed by this SA\n",
			 tdb->tdb_cur_bytes);
	    
	    l += sprintf(buffer + l, "\tExpirations:\n");

	    if (tdb->tdb_flags & TDBF_TIMER)
	      l += sprintf(buffer + l,
			   "\t\tHard expiration(1) in %qu seconds\n",
			   tdb->tdb_exp_timeout - time.tv_sec);
	    
	    if (tdb->tdb_flags & TDBF_SOFT_TIMER)
	      l += sprintf(buffer + l,
			   "\t\tSoft expiration(1) in %qu seconds\n",
			   tdb->tdb_soft_timeout - time.tv_sec);
	    
	    if (tdb->tdb_flags & TDBF_BYTES)
	      l += sprintf(buffer + l, "\t\tHard expiration after %qu bytes\n",
			   tdb->tdb_exp_bytes);
	    
	    if (tdb->tdb_flags & TDBF_SOFT_BYTES)
	      l += sprintf(buffer + l, "\t\tSoft expiration after %qu bytes\n",
			   tdb->tdb_soft_bytes);

	    if (tdb->tdb_flags & TDBF_ALLOCATIONS)
	      l += sprintf(buffer + l,
			   "\t\tHard expiration after %u flows\n",
			   tdb->tdb_exp_allocations);
	    
	    if (tdb->tdb_flags & TDBF_SOFT_ALLOCATIONS)
	      l += sprintf(buffer + l,
			   "\t\tSoft expiration after %u flows\n",
			   tdb->tdb_soft_allocations);

	    if (tdb->tdb_flags & TDBF_FIRSTUSE)
	    {
		if (tdb->tdb_first_use)
		l += sprintf(buffer + l,
			     "\t\tHard expiration(2) in %qu seconds\n",
			     (tdb->tdb_first_use + tdb->tdb_exp_first_use) -
			     time.tv_sec);
		else
		l += sprintf(buffer + l,
			     "\t\tHard expiration in %qu seconds after first "
			     "use\n", tdb->tdb_exp_first_use);
	    }

	    if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
	    {
	        if (tdb->tdb_first_use)
		  l += sprintf(buffer + l,
			       "\t\tSoft expiration(2) in %qu seconds\n",
			       (tdb->tdb_first_use + tdb->tdb_soft_first_use) -
			       time.tv_sec);
	        else
		  l += sprintf(buffer + l,
			       "\t\tSoft expiration in %qu seconds after first "
			       "use\n", tdb->tdb_soft_first_use);
	    }

	    if (!(tdb->tdb_flags & (TDBF_TIMER | TDBF_SOFT_TIMER | TDBF_BYTES |
				    TDBF_SOFT_ALLOCATIONS | TDBF_ALLOCATIONS |
				    TDBF_SOFT_BYTES | TDBF_FIRSTUSE |
				    TDBF_SOFT_FIRSTUSE)))
	      l += sprintf(buffer + l, "\t\t(none)\n");

	    l += sprintf(buffer + l, "\n");
	    
	    return l;
	}
    
    return 0;
}

char *
inet_ntoa4(struct in_addr ina)
{
    static char buf[4][4 * sizeof "123"];
    unsigned char *ucp = (unsigned char *) &ina;
    static int i = 1;
 
    i = (i + 1) % 2;
    sprintf(buf[i], "%d.%d.%d.%d", ucp[0] & 0xff, ucp[1] & 0xff,
            ucp[2] & 0xff, ucp[3] & 0xff);
    return (buf[i]);
}

char *
ipsp_address(union sockaddr_union sa)
{
    switch (sa.sa.sa_family)
    {
	case AF_INET:
	    return inet_ntoa4(sa.sin.sin_addr);

#if INET6
	    /* XXX Add AF_INET6 support here */
#endif /* INET6 */

	default:
	    return "(unknown address family)";
    }
}
