/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>
#include <netinet6/icmpv6_var.h>

#if __OpenBSD__ && defined(NRL_IPSEC)
#define IPSEC 1
#endif /* __OpenBSD__ && defined(NRL_IPSEC) */

#ifdef IPSEC
#include <sys/osdep.h>
#include <net/netproc.h>
#include <net/netproc_var.h>

extern struct netproc_security fixedencrypt;
extern struct netproc_auth fixedauth;
#endif /* IPSEC */

#ifdef DEBUG_NRL_SYS
#include <sys/debug.h>
#endif /* DEBUG_NRL_SYS */
#ifdef DEBUG_NRL_NETINET6
#include <netinet6/debug.h>
#endif /* DEBUG_NRL_NETINET6 */

#if __FreeBSD__
#include <sys/sysctl.h>
#endif /* __FreeBSD__ */

/*
 * Globals.
 */

static struct sockaddr_in6 icmpsrc = { sizeof (struct sockaddr_in6), 
					 AF_INET6 };
static struct sockaddr_in6 icmpdst = { sizeof (struct sockaddr_in6), 
					 AF_INET6 };
struct icmpv6stat icmpv6stat;

/*
 * External globals.
 */

extern struct in6_ifaddr *in6_ifaddr;
extern u_char ipv6_protox[];
extern struct protosw inet6sw[];

/*
 * Functions and macros (and one global that needs to be near the function).
 */

uint8_t ipv6_saved_routing[384]; /* If a routing header has been processed,
				   then it will have been bcopy()d into
				   this buffer. If not, the first byte
				   will be set to zero (which would be
				   a nexthdr of hop-by-hop, and is not
				   valid). */

struct mbuf *ipv6_srcrt __P((void));
void ipv6_icmp_reflect __P((struct mbuf *, int));
void update_pathmtu __P((struct in6_addr *, uint32_t));

/* This is broken as of getting rid of hdrindex and the mbuf structure it
   creates. While this needs to be fixed, it's a Real Shame that source
   route reflection couldn't be reimplemented for this release... - cmetz */
#if 0
/*----------------------------------------------------------------------
 * Reverse a saved IPv6 source route, for possible use on replies.
 ----------------------------------------------------------------------*/

struct mbuf *ipv6_srcrt()
{
      struct ipv6_srcroute0 *sr, *osr = (struct ipv6_srcroute0 *)ipv6_saved_routing;
      struct in6_addr *sra, *osra;
      struct mbuf *srm;
      int i, j;

      if (!osr->i6sr_nexthdr)
        return NULL;
      if (osr->i6sr_type)
	return NULL;
      if (!(srm = m_get(M_DONTWAIT, MT_DATA)))
        return NULL;

      sr = mtod(srm, struct ipv6_srcroute0 *);
      bzero(sr, sizeof(struct ipv6_srcroute0));
      sr->i6sr_nexthdr = IPPROTO_ICMPV6;
      sr->i6sr_len = osr->i6sr_len;
      j = sr->i6sr_left = sr->i6sr_len/2;
/* We probably should reverse the bit mask, but it's painful, and defaulting
   to loose source routing might be preferable anyway. */
      sra = (struct in6_addr *)((caddr_t)sr + sizeof(struct ipv6_srcroute0));
      osra = (struct in6_addr *)((caddr_t)osr + sizeof(struct ipv6_srcroute0));
      srm->m_len = sizeof(struct ipv6_srcroute0) + sizeof(struct in6_addr) * j;
      for (i = 0; i < sr->i6sr_len/2; sra[i++] = osra[j--]);
      return srm;
}
#endif /* 0 */

/*----------------------------------------------------------------------
 * Reflect an IPv6 ICMP packet back to the source.
 ----------------------------------------------------------------------*/

void
ipv6_icmp_reflect(m, extra)
     struct mbuf *m;
     int extra;
{
  struct in6_addr tmp;
  struct ipv6 *ipv6;
  struct in6_ifaddr *i6a;
  struct ipv6_icmp *icmp;
#if 0
  struct mbuf *routing = NULL;
#endif /* 0 */
#ifdef IPSEC
  struct socket *socket, fake;
#endif /* IPSEC */

  /*
   * Hmmm, we potentially have authentication, routing, and hop-by-hop 
   * headers behind this.  OUCH.   For now, however, assume only IPv6
   * header, followed by ICMP.
   */

  DP(FINISHED, extra, d);

  ipv6 = mtod(m, struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(m, caddr_t) + extra);

  tmp = ipv6->ipv6_dst;
  ipv6->ipv6_dst = ipv6->ipv6_src;

  /*
   * If the incoming packet was addressed directly to us,
   * use dst as the src for the reply.  Otherwise (multicast
   * or anonymous), use the address which corresponds
   * to the incoming interface.
   */

  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
    {
      /* Find first (non-local if possible) address for
	 source usage.  If multiple locals, use last one found. */
      
      if (IN6_ARE_ADDR_EQUAL(&tmp, &I6A_SIN(i6a)->sin6_addr))
	break;
    }
  icmpdst.sin6_addr = tmp;

  if (i6a == NULL && m->m_pkthdr.rcvif != NULL)
    i6a = (struct in6_ifaddr *)ifaof_ifpforaddr((struct sockaddr *)&icmpdst,
						m->m_pkthdr.rcvif);

  if (i6a == NULL)
    {
      /* Want globally-routable if I can help it. */
      i6a = in6_ifaddr;
    }
  
  ipv6->ipv6_src = I6A_SIN(i6a)->sin6_addr;

  ipv6->ipv6_nexthdr = IPPROTO_ICMPV6;

  if (extra > sizeof(struct ipv6)) {
    DP(FINISHED, extra, 08x);
    DP(FINISHED, m->m_pkthdr.len, 08x);
    ipv6_stripoptions(m, extra);
    DP(FINISHED, m->m_pkthdr.len, 08x);
    extra = sizeof(struct ipv6);
#if 0
    if ((routing = ipv6_srcrt())) {
      ipv6->ipv6_nexthdr = IPPROTO_ROUTING;
      mtod(routing, struct ipv6_srcroute0 *)->i6sr_nexthdr = IPPROTO_ICMPV6;
      routing->m_next = m->m_next;
      m->m_next = routing;
      extra += routing->m_len;
    } else
      DPRINTF(IDL_ERROR, ("icmp_reflect() got options but can't strip them\n"));
#endif /* 0 */
  }

  m->m_flags &= ~(M_BCAST|M_MCAST);

  DP(FINISHED, m->m_pkthdr.len, d);

  /* For errors, anything over the 576 byte mark we discard. */
  if (!(ICMPV6_INFOTYPE(icmp->icmp_type)))
    if (m->m_pkthdr.len > ICMPV6_MAXLEN)
      m_adj(m, -(m->m_pkthdr.len - ICMPV6_MAXLEN));

  DP(FINISHED, m->m_pkthdr.len - extra, d);

  icmp->icmp_cksum = 0;
  DPRINTF(IDL_EVENT,("ipv6_icmp_reflect() calling in6_cksum().\n"));
  icmp->icmp_cksum = in6_cksum(m,IPPROTO_ICMPV6, m->m_pkthdr.len - extra,
			       extra);
  DP(FINISHED, icmp->icmp_cksum, 04x);

  ipv6->ipv6_hoplimit = 255;  
  /* Version 6, priority 12 (info) or 15 (error), flow zero */
  ipv6->ipv6_versfl = htonl(((6) << 28) | 
		((ICMPV6_INFOTYPE(icmp->icmp_type) ? 12 : 15) << 24));

#if 0 /* def IPSEC */
  /*
   *  Packet sent should be authenticated/encrypted if responding to
   *  received packet that was authenticated/encrypted.
   */
  if (m->m_flags & (M_AUTHENTIC | M_DECRYPTED)) {
    struct netproc_requestandstate *r;

    bzero(&fake, sizeof(struct socket));

    if (netproc_alloc(&fake)) {
      DPRINTF(ERROR, ("icmp_send: netproc_alloc failed\n"));
      m_freem(m);
      return;
    }

    r = &((struct netproc_socketdata *)fake.so_netproc)->requests[0];
    r->requestlen = ((m->m_flags & M_AUTHENTIC) ?
 		   sizeof(struct netproc_auth) : 0) +
	                  ((m->m_flags & M_DECRYPTED) ?
			   sizeof(struct netproc_security) : 0);
    if (!(r->request = OSDEP_MALLOC(r->requestlen))) {
      DPRINTF(ERROR, ("icmp_send: malloc(%d) failed\n",
		    r->requestlen));
      netproc_free(&fake);
      m_freem(m);
      return;
    }

    {
      void *p = r->request;
	    
      if (m->m_flags & M_AUTHENTIC) {
        memcpy(p, &fixedauth, sizeof(struct netproc_auth));
        p += sizeof(fixedauth);
      }
      if (m->m_flags & M_DECRYPTED)
        memcpy(p, &fixedencrypt, sizeof(struct netproc_security));
    }

    socket = &fake;
  } else
    socket = NULL;
#endif /* IPSEC */

  icmpv6stat.icps_outhist[icmp->icmp_type]++;

#ifdef IPSEC
  ipv6_output(m, NULL, IPV6_RAWOUTPUT, NULL, NULL, socket);

  if (socket)
    netproc_free(socket);
#else /* IPSEC */
  ipv6_output(m, NULL, IPV6_RAWOUTPUT, NULL, NULL, NULL);
#endif /* IPSEC */
}

/*----------------------------------------------------------------------
 * Given a bad packet (badpack), generate an ICMP error packet in response.
 * We assume that ipv6_preparse() has been run over badpack.
 *
 * Add rate-limiting code to this function, on a timer basis.
 * (i.e. if t(current) - t(lastsent) < limit, then don't send a message.
 ----------------------------------------------------------------------*/

void
ipv6_icmp_error(badpack, type, code, paramptr)
     struct mbuf *badpack;
     int type,code;
     uint32_t paramptr;
{
  struct ipv6 *oipv6;
  int divpoint = sizeof(struct ipv6);
  struct ipv6_icmp *icmp;
  struct mbuf *outgoing;

  if ((badpack->m_flags & M_MCAST) /*&& type != ICMPV6_TOOBIG */) {
      m_freem(badpack);
      return;
    }

  /*if (type != ICMPV6_REDIRECT)*/
    icmpv6stat.icps_error++;

  /*
   * Since MTU and max ICMP packet size is less than a cluster (so far...)
   * pull the offending packet into a single cluster.
   *
   * If option-stripping becomes required, here might be the place to do it.
   * (The current design decision is to not strip options.  Besides, one of
   *  the callers of this function is ipv6_hop(), which does hop-by-hop
   *  option processing.)
   */

  oipv6 = mtod(badpack,struct ipv6 *);

  DDO(GROSSEVENT,printf("oipv6 (0x%lx) is:\n",(unsigned long)oipv6);dump_ipv6(oipv6));

  DP(FINISHED, badpack->m_pkthdr.len, d);

  /*
   * Get a new cluster mbuf for ICMP error message.  Since IPv6 ICMP messages
   * have a length limit that should be less than MCLBYTES, one cluster should
   * work nicely.
   */

  if (!(outgoing = m_gethdr(M_DONTWAIT,MT_HEADER))) {
    m_freem(badpack);
    return;
  };

  MCLGET(outgoing, M_DONTWAIT);
  if (!(outgoing->m_flags & M_EXT)) {
    m_freem(badpack);
    m_free(outgoing);
    return;
  };

  outgoing->m_len = sizeof(struct ipv6) + ICMPV6_MINLEN;
  bcopy(oipv6, mtod(outgoing, caddr_t), sizeof(struct ipv6));
  icmp = (struct ipv6_icmp *)(mtod(outgoing, caddr_t) + sizeof(struct ipv6));

  {
    int i = badpack->m_pkthdr.len;

    if (i > ICMPV6_MAXLEN - sizeof(struct ipv6) - sizeof(struct icmpv6hdr))
      i = ICMPV6_MAXLEN - sizeof(struct ipv6) - sizeof(struct icmpv6hdr);

    outgoing->m_len += i;
    /* Copies are expensive, but linear buffers are nice. Luckily, the data
       is bounded, short, and ICMP errors aren't that performance critical. */
    m_copydata(badpack, 0, i, mtod(outgoing, caddr_t) +
	       sizeof(struct ipv6) + sizeof(struct icmpv6hdr));
  }

  /* Need rcvif to do source address selection later */
  outgoing->m_pkthdr.rcvif = badpack->m_pkthdr.rcvif; 

  outgoing->m_pkthdr.len = outgoing->m_len;

#if 0 /* defined(IPSEC) || defined(NRL_IPSEC) */
  /*
   * Copy over the DECRYPTED and AUTHENTIC flag.
   * NB: If the inbound packet was sent to us in an authenticated
   * or encrypted tunnel, these flags are cleared when we get here.
   * We don't have a way to preserve tunnel state at the moment.
   * This is a hole we need to fix soon.
   */
  outgoing->m_flags |= badpack->m_flags & (M_DECRYPTED | M_AUTHENTIC);
  DDO(IDL_ERROR,if (outgoing->m_flags & M_AUTHENTIC) printf("icmpv6_error processing authentic pkt\n"));
  DDO(IDL_ERROR,if (outgoing->m_flags & M_DECRYPTED) printf("icmpv6_error processing encrypted pkt\n"));
#endif /* defined(IPSEC) || defined(NRL_IPSEC) */

  m_freem(badpack);

  icmp->icmp_type = type;
  icmp->icmp_code = code;
  icmp->icmp_unused = 0;
  if (type == ICMPV6_PARAMPROB || type == ICMPV6_TOOBIG)
      icmp->icmp_paramptr = htonl(paramptr);

  ipv6_icmp_reflect(outgoing, divpoint);
}

/*----------------------------------------------------------------------
 * Update path MTU for an IPv6 destination.  This function may have to go
 * scan for TCP control blocks and give them hints to scale down.
 * There is a small denial-of-service attack if MTU messages are
 * unauthenticated.  I can lower MTU to 576.
 ----------------------------------------------------------------------*/

void
update_pathmtu(dst, newmtu)
     struct in6_addr *dst;
     uint32_t newmtu;       /* ntohl()'ed by caller. */
{
  int s = splnet();
  struct rtentry *rt;
  struct sockaddr_in6 sin6;

  DDO(IDL_EVENT,printf("Entering update_pathmtu with:\n");\
      dump_in6_addr(dst);printf("And newmtu of %d.\n",(unsigned int)newmtu));

  if (IN6_IS_ADDR_MULTICAST(dst))
    {
      /* Multicast MTU Discovery not yet implemented */
      DPRINTF(IDL_ERROR, ("Multicast MTU too big message.\n"));
      splx(s);
      return;
    }

  sin6.sin6_family = AF_INET6;
  sin6.sin6_len = sizeof(struct sockaddr_in6);
  sin6.sin6_addr = *dst;

  /*
   * Since I'm doing a rtalloc, no need to zero-out the port and flowlabel.  
   */
#ifdef __FreeBSD__
  if ((rt = rtalloc1((struct sockaddr *)&sin6,0,0UL)) != NULL)
#else /* __FreeBSD__ */
  if ((rt = rtalloc1((struct sockaddr *)&sin6,0)) != NULL)
#endif /* __FreeBSD__ */
    {
      if (rt->rt_flags & RTF_HOST) {
	  if (rt->rt_rmx.rmx_mtu < newmtu) {
	    DPRINTF(IDL_ERROR, 
	    ("DANGER:  New MTU message is LARGER than current MTU.\n"));
          };

	  rt->rt_rmx.rmx_mtu = newmtu;   /* This should be enough for HLP's to
					    know MTU has changed, IMHO. */
	  rt->rt_refcnt--;
      } else {
	DPRINTF(IDL_ERROR, 
	  ("Got path MTU message for non-cloned destination route.\n"));
      }
      /*
       * Find all active tcp connections, and indicate they need path MTU
       * updating as well.
       *
       * Also, find RTF_TUNNEL routes that point to this updated route,
       * because they need their path MTU lowered.  Perhaps decapsulating
       * the message, and sending TOOBIG messages back.
       */
    }

  splx(s);
  return;
}

/*----------------------------------------------------------------------
 * ICMPv6 input routine.  Handles inbound ICMPv6 packets, including
 * direct handling of some packets.
 ----------------------------------------------------------------------*/

void
ipv6_icmp_input(incoming, extra)
     register struct mbuf *incoming;
     int extra;
{
  struct ipv6_icmp *icmp;
  struct ipv6 *ipv6;
  int icmplen,code;
  void (*ctlfunc) __P((int, struct sockaddr *, struct ipv6 *, struct mbuf *));

  /*
   * Q:  Any address validity checks beyond those in ipv6_input()?
   */

  DPRINTF(FINISHED, ("ipv6_icmp_input -- pkthdr.len = %d, extra = %d\n", 
		     incoming->m_pkthdr.len, extra));

  DDO(FINISHED, dump_mbuf_tcpdump(incoming));

  icmplen = incoming->m_pkthdr.len - extra;
  if (icmplen < ICMPV6_MINLEN)
    {
      /* Not enough for full ICMP packet. */
      icmpv6stat.icps_tooshort++;
      m_freem(incoming);
      return;
    }

  if (incoming->m_len < extra + ICMPV6_MINLEN)
    if (!(incoming = m_pullup2(incoming, extra + ICMPV6_MINLEN)))
      return;

  DDO(FINISHED, dump_mbuf_tcpdump(incoming));

  ipv6 = mtod(incoming, struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(incoming, caddr_t) + extra);

  /*
   * Verify checksum with IPv6 header at the top of this chain.
   */

  DPRINTF(IDL_EVENT,("ipv6_icmp_input() calling in6_cksum().\n"));
  DPRINTF(IDL_EVENT,("icmplen = %d\n", icmplen));
  {
    unsigned int cksum;

    if ((cksum = in6_cksum(incoming, IPPROTO_ICMPV6, icmplen, extra)))
      {
	DPRINTF(IDL_ERROR,("ipv6_icmp_input() -- checksum returned %08x.\n", cksum));
	m_freem(incoming);
	icmpv6stat.icps_checksum++;
	return;
      }
  }

#ifdef IPSEC
  /* Perform input-side policy check. Drop packet if policy says to drop it. */
  {
    struct sockaddr_in6 srcsa, dstsa;
    
    bzero(&srcsa, sizeof(struct sockaddr_in6));
    srcsa.sin6_family = AF_INET6;
    srcsa.sin6_len = sizeof(struct sockaddr_in6);
    srcsa.sin6_addr = ipv6->ipv6_src;
    
    bzero(&dstsa, sizeof(struct sockaddr_in6));
    dstsa.sin6_family = AF_INET6;
    dstsa.sin6_len = sizeof(struct sockaddr_in6);
    dstsa.sin6_addr = ipv6->ipv6_dst;
    
    /* XXX - state arg should NOT be NULL, it should be the netproc state
       carried up the stack - cmetz */
    if (netproc_inputpolicy(NULL, (struct sockaddr *)&srcsa,
			    (struct sockaddr *)&dstsa, IPPROTO_ICMPV6,
			    incoming, NULL, NULL)) {
      m_freem(incoming);
      return;
    }
  }
#endif /* IPSEC */

  code = icmp->icmp_code;
  DPRINTF(IDL_EVENT, ("icmp->icmp_type = %d\n", icmp->icmp_type));

  if (icmp->icmp_type < ICMPV6_MAXTYPE + 1)
    {
      icmpv6stat.icps_inhist[icmp->icmp_type]++;

      /*
       * Deal with the appropriate ICMPv6 message type/code.
       */
      switch(icmp->icmp_type)
	{
	case ICMPV6_ECHO:
	  icmp->icmp_type = ICMPV6_ECHOREPLY;
	  icmpv6stat.icps_reflect++;
	  ipv6_icmp_reflect(incoming, extra);
	  return;
	case ICMPV6_UNREACH:
	  /*
	   * The pair of <type,code> should map into a PRC_*
	   * value such that I don't have to rewrite in_pcb.c.
	   */
	  switch (code)
	    {
	    case ICMPV6_UNREACH_NOROUTE:
	      code = PRC_UNREACH_NET;
	      break;
	    case ICMPV6_UNREACH_ADMIN:
	      /* Subject to change */
	      code = PRC_UNREACH_HOST;
	      break;
	    case ICMPV6_UNREACH_NOTNEIGHBOR:
	      /* Subject to change */
	      code = PRC_UNREACH_HOST;
	      break;
	    case ICMPV6_UNREACH_ADDRESS:
	      code = PRC_HOSTDEAD;
	      break;
	    case ICMPV6_UNREACH_PORT:
	      code = PRC_UNREACH_PORT;
	      break;
	    default:
	      goto badcode;
	    }
	  goto deliver;
	  break;
	case ICMPV6_TIMXCEED:
	  if (code >1)
	    goto badcode;
	  code += PRC_TIMXCEED_INTRANS;
	  goto deliver;
	  
	case ICMPV6_PARAMPROB:
	  if (code >2)
	    goto badcode;
	  code = PRC_PARAMPROB;

	case ICMPV6_TOOBIG:
	deliver:
	  /*
	   * Problem with datagram, advice HLP's.
	   */
	  DPRINTF(IDL_EVENT, ("delivering\n"));
	  if (icmplen < ICMPV6_HLPMINLEN)
	    {
	      icmpv6stat.icps_badlen++;
	      m_freem(incoming);
	      return;
	    }
 
	  /* May want to pullup more than this */
	  if (!(incoming = m_pullup2(incoming, extra + ICMPV6_HLPMINLEN)))
	    return;

	  /*
	   * If cannot determine HLP, discard packet.
	   *
	   * For now, I assume that ICMP messages will be generated such that
	   * the enclosed header contains only IPv6+<HLP header>.  This is not
	   * a good assumption to make in light of all sorts of options between
	   * IPv6 and the relevant place to deliver this message.
	   */
	  {
	    struct ipv6 *ipv6 = (struct ipv6 *)(mtod(incoming, caddr_t) + extra + ICMPV6_MINLEN);
	    icmpsrc.sin6_addr = ipv6->ipv6_dst;

	    if (icmp->icmp_type == ICMPV6_TOOBIG)
	      {
		update_pathmtu(&ipv6->ipv6_dst,htonl(icmp->icmp_nexthopmtu));
		/* If I want to deliver to HLP, remove the break, and
		   set code accordingly. */
		break;
	      }
	    DPRINTF(IDL_EVENT, ("Finding control function for %d\n",
				      ipv6->ipv6_nexthdr));
	    if ((ctlfunc = (void *)inet6sw[ipv6_protox[ipv6->ipv6_nexthdr]].pr_ctlinput)) {
	      DPRINTF(IDL_EVENT, ("Calling control function for %d\n", 
					ipv6->ipv6_nexthdr));
	      (*ctlfunc)(code, (struct sockaddr *)&icmpsrc, ipv6,incoming);
	    }
	  }
	  break;

	badcode:
	  DPRINTF(IDL_EVENT, ("Bad code!\n"));
	  icmpv6stat.icps_badcode++;
	  break;
	  
	  /*
	   * IPv6 multicast group messages.
	   */
	case ICMPV6_GRPQUERY:
	case ICMPV6_GRPREPORT:
	case ICMPV6_GRPTERM:
	  break;
	  
	  /*
	   * IPv6 discovery messages.
	   */
	case ICMPV6_ROUTERSOL:
	  ipv6_routersol_input(incoming, extra);
	  break;
	case ICMPV6_ROUTERADV:
	  ipv6_routeradv_input(incoming, extra);
	  break;
	case ICMPV6_NEIGHBORSOL:
	  ipv6_neighborsol_input(incoming, extra);
	  break;
	case ICMPV6_NEIGHBORADV:
	  ipv6_neighboradv_input(incoming, extra);
	  break;
	case ICMPV6_REDIRECT:
	  ipv6_redirect_input(incoming, extra);
	  break;
	default:
	  /* Allow delivery to raw socket. */
	  break;
	}
    }
  DPRINTF(IDL_EVENT, ("Delivering ICMPv6 to raw socket\n"));
  DP(FINISHED, incoming->m_pkthdr.len, d);

  ripv6_input(incoming, extra); /* Deliver to raw socket. */
}


/*
 * The following functions attempt to address ICMP deficiencies in IPv6.
 * Mostly these are part of a hack to keep the user-level program from
 * computing checksums.  :-P
 */

/*----------------------------------------------------------------------
 * This function should never be called.
 ----------------------------------------------------------------------*/

int
ipv6_icmp_output(m, so, dst)
     struct mbuf *m;
     struct socket *so;
     struct in6_addr *dst;
{
  DPRINTF(IDL_ERROR, 
	  ("ipv6_icmp_output() was called and shouldn't have been.\n"));

  return ripv6_output(m,so,dst,NULL);
}

#if !__FreeBSD__
/*----------------------------------------------------------------------
 * Prepend IPv6 header, and compute IPv6 checksum for PRU_SEND, otherwise,
 * redirect call to ripv6_usrreq().
 ----------------------------------------------------------------------*/
int
#if __NetBSD__
ipv6_icmp_usrreq(so, req, m, nam, control, p)
#else /* __NetBSD__ */
ipv6_icmp_usrreq(so, req, m, nam, control)
#endif /* __NetBSD__ */
     struct socket *so;
     int req;
     struct mbuf *m, *nam, *control;
#if __NetBSD__
     struct proc *p;
#endif /* __NetBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);

  DPRINTF(IDL_EVENT,("Entering ipv6_icmp_usrreq(), req == %d\n",req));

  /*
   * If not sending, or sending with the header included (which IMHO means
   * the user filled in the src & dest on his/her own), do normal raw
   * IPv6 user request.
   */ 

  DPRINTF(IDL_EVENT,("Before check for ripv6_usrreq().\n"));
  if (req != PRU_SEND || inp->inp_flags & INP_HDRINCL)
#if __NetBSD__
    return ripv6_usrreq(so,req,m,nam,control,p);
#else /* __NetBSD__ */
    return ripv6_usrreq(so,req,m,nam,control);
#endif /* __NetBSD__ */

  {
    struct sockaddr *sa;

    if (nam)
      sa = mtod(nam, struct sockaddr *);
    else
      sa = NULL;

#if __NetBSD__
    return ipv6_icmp_send(so, req, m, sa, control, p);
#else /* __NetBSD__ */
    return ipv6_icmp_send(so, req, m, sa, control, NULL);
#endif /* __NetBSD__ */
  };
}
#endif /* !__FreeBSD__ */

int ipv6_icmp_send(struct socket *so, int req, struct mbuf *m,
		   struct sockaddr *addr, struct mbuf *control,
		   struct proc *p)
{
  register struct inpcb *inp = sotoinpcb(so);
  register struct ipv6 *ipv6;
  register struct ipv6_icmp *icp;
  struct in6_addr *dst;
  int tflags = 0, len, rc;
  struct in6_ifaddr *i6a;

  /*
   * redundant check, but necessary since we don't know if we are coming from
   * icmp_usrreq or not. 
   */
  if (inp->inp_flags & INP_HDRINCL)
#if __NetBSD__ || __FreeBSD__
     return ripv6_usrreq_send(so, req, m, addr, control, p);
#else /* __NetBSD__ || __FreeBSD__ */
     return ripv6_usrreq_send(so, req, m, addr, control);
#endif /* __NetBSD__ || __FreeBSD__ */

  if (in6_ifaddr == NULL)
    {
      m_freem(m);
      return EADDRNOTAVAIL;
    }
  len = m->m_pkthdr.len;

  /*
   * If we get here, req == PRU_SEND and flags do not have INP_HDRINCL set.
   * What that means in English is that a user process is sending an ICMPv6
   * datagram without constructing an IPv6 header.
   * We will construct an IPv6 header, fill it in completely, THEN COMPUTE
   * THE ICMPv6 CHECKSUM and tell ipv6_output() that we are raw.
   */

  if (so->so_state & SS_ISCONNECTED)
    {
      if (addr)
	{
	  m_freem(m);
	  return EISCONN;
	}
      dst = &(inp->inp_faddr6);
      i6a = (struct in6_ifaddr *)inp->inp_route6.ro_rt->rt_ifa;
    }
  else  /* Not connected */
    {
      if (addr == NULL)
	{
	  m_freem(m);
	  return ENOTCONN;
	}
      DPRINTF(GROSSEVENT,("Sockaddr in nam is:\n"));
      DDO(GROSSEVENT,dump_smart_sockaddr(addr));
      dst = &(((struct sockaddr_in6 *) addr)->sin6_addr);
      inp->inp_route6.ro_dst = *((struct sockaddr_in6 *)addr);
      DPRINTF(EVENT,("In icmpv6_usrreq, Route is:\n"));
      DDO(EVENT, dump_rtentry(inp->inp_route6.ro_rt));
      if (so->so_options & SO_DONTROUTE)
	{
#define ifatoi6a(x)  ((struct in6_ifaddr *)x)
	  if ((i6a = 
	       ifatoi6a(ifa_ifwithdstaddr((struct sockaddr *)addr))) == 0
	      &&
	      (i6a = 
	       ifatoi6a(ifa_ifwithnet((struct sockaddr *)addr))) == 0)
	    {
	      m_freem(m);
	      return ENETUNREACH;
	    }
	  inp->inp_route.ro_rt = NULL;
	}
      else
	{
	  struct route *ro = &inp->inp_route;

	  /*
	   * If there is no route, consider sending out a neighbor advert
	   * across all the nodes.  This is like the ipv6_onlink_query()
	   * call in ipv6_output.c.
	   */
	  if (ro->ro_rt == NULL)
	    rtalloc(ro);
	  if (ro->ro_rt == NULL)
	    {
	      /*
	       * No route of any kind, so spray neighbor solicits out all
	       * interfaces, unless it's a multicast address.
	       */
	      DPRINTF(IDL_FINISHED,("Icmpv6 spraying neigbor solicits.\n"));
	      if (IN6_IS_ADDR_MULTICAST(dst))
		{
		  /*
		   * Select source address for multicast, for now
		   * return an error.
		   */
		  m_freem(m);
		  return ENETUNREACH;
		}
	      ipv6_onlink_query((struct sockaddr_in6 *)&ro->ro_dst);
	      rtalloc(ro);
	    }
	  if (ro->ro_rt == NULL)
	    {
	      m_freem(m);
	      return ENETUNREACH;
	    }
	  DPRINTF(IDL_EVENT,("Route after ipv6_onlink_query:\n"));
	  DDO(IDL_EVENT,dump_route(ro));
	  DDO(IDL_EVENT,if (ro) dump_rtentry(ro->ro_rt));
	  if (ro->ro_rt->rt_ifa == NULL)
	    {
	      /*
	       * We have a route where we don't quite know which interface 
	       * the neighbor belongs to yet.  If I get here, I know that this
	       * route is not pre-allocated (such as done by in6_pcbconnect()),
	       * because those pre-allocators will do the same
	       * ipv6_onlink_query() and ipv6_verify_onlink() in advance.
	       *
	       * I can therefore free the route, and get it again.
	       */
	      int error;
	      
	      DPRINTF(IDL_EVENT,("Okay...rt_ifa==NULL!\n"));
	      RTFREE(ro->ro_rt);
	      ro->ro_rt = NULL;
	      switch (error = ipv6_verify_onlink((struct sockaddr_in6 *)&ro->ro_dst))
		{
		case 0:
		  break;
		case -1:
		  error = ENETUNREACH;
		default:
		  m_freem(m);
		  return error;
		}
	      rtalloc((struct route *)ro);
	      if (ro->ro_rt == NULL || ro->ro_rt->rt_ifa == NULL)
		panic("Oops3, I'm forgetting something after verify_onlink().");
	      DPRINTF(IDL_EVENT,("Route after ipv6_verify_onlink:\n"));
	      DDO(IDL_EVENT,dump_route(ro));
	      DDO(IDL_EVENT,if (ro) dump_rtentry(ro->ro_rt));
	    }
	  
	  i6a = (struct in6_ifaddr *)ro->ro_rt->rt_ifa;
	  DPRINTF(IDL_EVENT,("Okay we got an interface for ipv6_icmp:\n"));
	  DDO(IDL_EVENT,dump_ifa((struct ifaddr *)i6a));
	}

      if (IN6_IS_ADDR_MULTICAST(dst) && inp->inp_moptions6 &&
	  inp->inp_moptions6->i6mo_multicast_ifp &&
	  inp->inp_moptions6->i6mo_multicast_ifp != i6a->i6a_ifp)
	{
	  struct ifaddr *ifa;

#if __FreeBSD__
	  for (ifa = inp->inp_moptions6->i6mo_multicast_ifp->if_addrhead.tqh_first;
	       ifa != NULL; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	  for (ifa = inp->inp_moptions6->i6mo_multicast_ifp->if_addrlist.tqh_first;
	       ifa != NULL; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
	  for (ifa = inp->inp_moptions6->i6mo_multicast_ifp->if_addrlist;
	       ifa != NULL; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	    if (ifa->ifa_addr->sa_family == AF_INET6)
	      {
		i6a = (struct in6_ifaddr *)ifa;
		break;
	      }
	  if (ifa == NULL)
	    {
	      DPRINTF(IDL_ERROR,("mcast inconsitency in icmp PRU_SEND.\n"));
	    }
	}
    }

  /*
   * If PCB has options hanging off of it, insert them here.
   */

  DPRINTF(GROSSEVENT,("ipv6_icmp_usrreq(): dst is "));
  DDO(GROSSEVENT,dump_in6_addr(dst));

  M_PREPEND(m,sizeof(struct ipv6),M_WAIT);
  if (m == NULL)
    panic("M_PREPEND died in ipv6_icmp_usrreq().");

  DPRINTF(EVENT,("Before m_pullup() for %d bytes.\n",\
			   sizeof(struct ipv6) + ICMPV6_MINLEN));
  if ((m = m_pullup(m,sizeof(struct ipv6) + ICMPV6_MINLEN)) == NULL)
    {
      DPRINTF(IDL_ERROR,("m_pullup in ipv6_icmp_usrreq() failed.\n"));
      return ENOBUFS;  /* Any better ideas? */
    }

  ipv6 = mtod(m,struct ipv6 *);
  ipv6->ipv6_length = len;
  ipv6->ipv6_nexthdr = IPPROTO_ICMPV6;
  ipv6->ipv6_hoplimit = 255;
  ipv6->ipv6_dst = *dst;
  ipv6->ipv6_versfl = htonl(0x60000000);  /* Plus flow label stuff. */
  /*
   * i6a pointer should be checked here.
   */
  ipv6->ipv6_src = i6a->i6a_addr.sin6_addr;

  icp = (struct ipv6_icmp *)(m->m_data + sizeof(struct ipv6));
  if (!(sotoinpcb(so)->inp_csumoffset))
    sotoinpcb(so)->inp_csumoffset = 2;

  DPRINTF(GROSSEVENT,("ipv6_icmp_usrreq(): Headers are\n"));
  DDO(GROSSEVENT,dump_ipv6(ipv6));
  DDO(GROSSEVENT,dump_ipv6_icmp(icp));

  /*
   * After this comment block you'd probably insert options,
   * and adjust lengths accordingly.
   */ 

  /*
   * Temporarily tweak INP_HDRINCL to fool ripv6_output().  I still don't
   * know how a user who sets INP_HDRINCL for real will prepare ICMP packets.
   * Also, set up data structures for callback routine in ipv6_output().
   */

  if (!(sotoinpcb(so)->inp_flags & INP_HDRINCL))
    {
      sotoinpcb(so)->inp_flags |= INP_HDRINCL;
      tflags = 1;
    }
  rc = ripv6_output(m,so,dst,control);
  if (!(so->so_state & SS_ISCONNECTED) && !(so->so_options & SO_DONTROUTE))
    {
      RTFREE(inp->inp_route.ro_rt);
      inp->inp_route.ro_rt = NULL;
    }
  if (tflags)
    sotoinpcb(so)->inp_flags &= ~INP_HDRINCL;

  return rc;
}

#if __FreeBSD__
#if __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__
#define MAYBEINLINE __inline__
#else /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__ */
#define MAYBEINLINE
#endif /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__ */

#if 0
MAYBEINLINE int ripv6_usrreq_attach(struct socket *, int , struct proc *);
MAYBEINLINE int ripv6_usrreq_detach(struct socket *);
MAYBEINLINE int ripv6_usrreq_abort(struct socket *);
MAYBEINLINE int ripv6_usrreq_bind(struct socket *, struct sockaddr *, struct proc *);
MAYBEINLINE int ripv6_usrreq_connect(struct socket *, struct sockaddr *, struct proc *);
MAYBEINLINE int ripv6_usrreq_shutdown(struct socket *so);
MAYBEINLINE int ripv6_usrreq_control(struct socket *, u_long, caddr_t,   
		      struct ifnet *, struct proc *);
MAYBEINLINE int ripv6_usrreq_sense(struct socket *, struct stat *);
MAYBEINLINE int ripv6_usrreq_sockaddr(struct socket *, struct sockaddr **);
MAYBEINLINE int ripv6_usrreq_peeraddr(struct socket *, struct sockaddr **);
#endif /* 0 */

struct pr_usrreqs ipv6_icmp_usrreqs = {
  ripv6_usrreq_abort, pru_accept_notsupp, ripv6_usrreq_attach,
  ripv6_usrreq_bind, ripv6_usrreq_connect, pru_connect2_notsupp,
  ripv6_usrreq_control, ripv6_usrreq_detach, ripv6_usrreq_detach,
  pru_listen_notsupp, ripv6_usrreq_peeraddr, pru_rcvd_notsupp,
  pru_rcvoob_notsupp, ipv6_icmp_send, ripv6_usrreq_sense,
  ripv6_usrreq_shutdown, ripv6_usrreq_sockaddr, sosend, soreceive, sopoll
};
#endif /* __FreeBSD__ */

int *icmpv6_sysvars[] = ICMPV6CTL_VARS;

#if __FreeBSD__ 
SYSCTL_STRUCT(_net_inet_icmpv6, ICMPV6CTL_STATS, icmpv6stat, CTLFLAG_RD, &icmpv6stat, icmpv6stat, "");
#else /* __FreeBSD__ */
int
ipv6_icmp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
  int *name;
  u_int namelen;
  void *oldp;
  size_t *oldlenp;
  void *newp;
  size_t newlen;
{
  if (name[0] >= ICMPV6CTL_MAXID)
    return (EOPNOTSUPP);
  switch (name[0]) {
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
    case ICMPV6CTL_STATS:
      return sysctl_rdtrunc(oldp, oldlenp, newp, &icmpv6stat, sizeof(icmpv6stat));
    default:
      return (sysctl_int_arr(icmpv6_sysvars, name, namelen, oldp, oldlenp, newp, newlen));
#else /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
    default:
      return EOPNOTSUPP;
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
  }
};
#endif /* __FreeBSD__ */
