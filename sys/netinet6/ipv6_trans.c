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
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>

#if __OpenBSD__
#undef IPSEC
#ifdef NRL_IPSEC
#define IPSEC 1
#endif /* NRL_IPSEC */
#endif /* __OpenBSD__ */

#ifdef DEBUG_NRL
#include <sys/debug.h>
#else /* DEBUG_NRL */
#if __OpenBSD__
#include <netinet6/debug.h>
#else /* __OpenBSD__ */
#include <sys/debug.h>
#endif /* __OpenBSD__ */
#endif /* DEBUG_NRL */

/*
 * External globals.
 */

extern struct in6_ifaddr *in6_ifaddr;
extern int ipv6_defhoplmt;

int ipv6_trans_mtu __P((struct mbuf **, int, int));
int ipv6_trans_output __P((struct mbuf *, struct sockaddr_in *, struct rtentry *));
int ipv6_encaps_output __P((struct mbuf *, struct sockaddr_in6 *, struct rtentry *));
int ipv6_tunnel_output __P((struct mbuf *, struct sockaddr_in6 *, struct rtentry *));
int ipv4_trans_output __P((struct mbuf *, struct sockaddr_in6 *, struct rtentry *));
int ipv4_encaps_output __P((struct mbuf *, struct sockaddr_in *, struct rtentry *));
int ipv4_tunnel_output __P((struct mbuf *, struct sockaddr_in *, struct rtentry *));

/*----------------------------------------------------------------------
 * Called from ip_icmp.c, this function will reduce the tunnel path MTU
 * precisely.  I know I have enough to reconstruct the IPv6 header, which
 * is all I care about for this case.  Return 1 if m0 is intact, and 0 if
 * m0 is corrupted somehow.  Don't forget to update m0.
 ----------------------------------------------------------------------*/

int
ipv6_trans_mtu(m0, newmtu, len)
     struct mbuf **m0;
     int newmtu,len;
{
  struct ip *ip,*iip;
  struct ipv6 *ipv6;
  struct icmp *icp;
  struct rtentry *rt;
  struct sockaddr_in6 sin6;
  struct in6_ifaddr *i6a;

  /*
   * Make packet contiguous into one block of memory.  If the IPv6 header is
   * beyond MCLBYTES into the packet, then I'm in big trouble.
   */
  *m0 = m_pullup2(*m0,min(len,MCLBYTES));
  if (*m0 == NULL)
    return 0;

  ip = mtod(*m0,struct ip *);
  icp = (struct icmp *) ((caddr_t)ip + (ip->ip_hl << 2));
  iip = &icp->icmp_ip;
  ipv6 = (struct ipv6 *) ((caddr_t)iip + (iip->ip_hl << 2));

  /*
   * Verify source is one of mine?
   */
  for (i6a = in6_ifaddr; i6a != NULL; i6a = i6a->i6a_next)
    if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &ipv6->ipv6_src))
      break;
  if (i6a == NULL)
    {
      /* Packet didn't originate with me.  Drop it. */
      return 1;
    }

  /*
   * Find route for this destination and update it.
   */
  sin6.sin6_family = AF_INET6;
  sin6.sin6_len = sizeof(sin6);
  sin6.sin6_port = 0;
  sin6.sin6_flowinfo = 0;
  sin6.sin6_addr = ipv6->ipv6_dst;

#ifdef __FreeBSD__
  rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
#else /* __FreeBSD__ */
  rt = rtalloc1((struct sockaddr *)&sin6, 0);
#endif /* __FreeBSD__ */

  if (rt == NULL)
    return 1;
  rt->rt_refcnt--;
  /*
   * Update path MTU.
   */
  if (!(rt->rt_flags & RTF_HOST))
    return 1;   /* Can't update path MTU on non-host-route. */
  if (rt->rt_rmx.rmx_mtu < newmtu - sizeof(struct ip))
    panic("MTU WEIRDNESS !!!");
  rt->rt_rmx.rmx_mtu = newmtu - sizeof(struct ip);
  return 1;
}

/*----------------------------------------------------------------------
 * Handle ICMP errors for IPv6-in-IPv4 tunnels.
 *
 * Security processing should be put in here, as it was with the other
 * ctlinput() functions, but with current ICMP implementations returning
 * only sizeof(struct ip) + 64 bits of offending packet.
 ----------------------------------------------------------------------*/
#if __OpenBSD__
#ifdef NRL_IPSEC
void *ipv6_trans_ctlinput(int cmd, struct sockaddr *sa, void *vp, struct mbuf *incoming)
#else /* NRL_IPSEC */
void *ipv6_trans_ctlinput(int cmd, struct sockaddr *sa, void *vp)
#endif /* NRL_IPSEC */
#else /* __OpenBSD__ */
#ifdef IPSEC
void ipv6_trans_ctlinput(int cmd, struct sockaddr *sa, register struct ip *ip, struct mbuf *incoming)
#else /* IPSEC */
void ipv6_trans_ctlinput(int cmd, struct sockaddr *sa, register struct ip *ip)
#endif /* IPSEC */
#endif /* __OpenBSD__ */
{
  struct sockaddr_in *sin = (struct sockaddr_in *)sa;
  struct sockaddr_in6 sin6;
  struct ipv6 *ipv6;
  struct rtentry *rt;
  struct in6_ifaddr *i6a;
#if __OpenBSD__
  struct ip *ip = (struct ip *)vp;
#endif /* __OpenBSD__ */

  sin6.sin6_family = AF_INET6;
  sin6.sin6_len = sizeof(sin6);
  sin6.sin6_port = 0;
  sin6.sin6_flowinfo = 0;
  DPRINTF(IDL_EVENT,("Entered ipv6_trans_ctlinput().\n"));

  /*
   * Do standard checks to see that all parameters are here.
   */
  if ((unsigned)cmd > PRC_NCMDS || sa->sa_family != AF_INET ||
      sin->sin_addr.s_addr == INADDR_ANY || ip == NULL)
    {
      DPRINTF(IDL_EVENT,("Failed one of the four checks.  Returning.\n"));
#ifdef __OpenBSD__
      return NULL;
#else /* __OpenBSD__ */
      return;
#endif /* __OpenBSD__ */
    }

  /*
   * Okay, at this point I have a contiguous IPv6 in IPv4 datagram.
   * I achieved this effect by convincing ip_icmp.[ch] to pull up
   * more than the first 64 bits.
   */

  ipv6 = (struct ipv6 *) ((caddr_t)ip + (ip->ip_hl << 2));
  /*
   * Verify source address is one of mine.
   */
  for (i6a = in6_ifaddr; i6a != NULL; i6a = i6a->i6a_next)
    if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &ipv6->ipv6_src))
      break;
  if (i6a == NULL)
    {
      /*
       * Packet didn't originate with me.  Drop it.
       */
#ifdef __OpenBSD__
      return NULL;
#else /* __OpenBSD__ */
      return;
#endif /* __OpenBSD__ */
    }

  sin6.sin6_addr = ipv6->ipv6_dst;
#ifdef __FreeBSD__
  rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
#else /* __FreeBSD__ */
  rt = rtalloc1((struct sockaddr *)&sin6, 0);
#endif /* __FreeBSD__ */
  if (rt == NULL)
#ifdef __OpenBSD__
    return NULL;
#else /* __OpenBSD__ */
    return;
#endif /* __OpenBSD__ */
  rt->rt_refcnt--;

  switch (cmd)
    {
    case PRC_MSGSIZE:
      /*
       * This function was called because the actual MTU wasn't grokked 
       * from the ICMP packet.
       *
       * If I get this, drop to IPV6_MINMTU.  If the actual MTU was in the
       * ICMP packet and was read correctly, it went up a different codepath.
       *
       * RFC 1191 talks about a plateau table.   Here's the place to do it,
       * either that, or on increase.
       */
      if (rt->rt_flags & RTF_HOST)
	{
	  /*
	   * Only attempt path MTU update if I'm a host.
	   */
	  if (rt->rt_rmx.rmx_mtu == IPV6_MINMTU)
	    panic("Too big on v6 MTU of 576!!!");
	  rt->rt_rmx.rmx_mtu = IPV6_MINMTU;
	}
      break;
    case PRC_UNREACH_NET:
      rt->rt_flags &= ~RTF_HOST;  /* Is this wise?  I'm doing this to return
				     the right error on future requests. */
      /* FALLTHROUGH */
    case PRC_UNREACH_HOST:
    case PRC_UNREACH_PROTOCOL:
      /*
       * Other end isn't a v6/v4 node.
       */
      rt->rt_flags |= RTF_REJECT;  /* Don't want to send any packets. */
      break;
    default:
      break;
    }
#ifdef __OpenBSD__
    return NULL;
#endif /* __OpenBSD__ */
}

/*----------------------------------------------------------------------
 *"IPv6 in IPv4 tunnelling."
 *
 * Output routine for IPv6 in IPv4.  Uses M_PREPEND to prepend an IPv4
 * header, and call ip_output().
 *
 * Called in: (only) ipv6_tunnel_output() from this file.
 ----------------------------------------------------------------------*/

int
ipv6_trans_output(outgoing, v4dst, v6rt)
     struct mbuf *outgoing;
     struct sockaddr_in *v4dst;
     struct rtentry *v6rt;
{
  struct route v4route;
  struct ip *ip;
  struct ipv6 *ipv6 = mtod(outgoing,struct ipv6 *);
  int rc;
#ifdef IPSEC
  struct socket *socket;
#endif /* IPSEC */

  /*
   * Like all below-IP(v6) output routines, check RTF_REJECT flag.
   */
  if (v6rt->rt_flags & RTF_REJECT)
    {
      m_freem(outgoing);
      return (v6rt->rt_flags & RTF_HOST) ? EHOSTUNREACH : ENETUNREACH;
    }

  if (v6rt->rt_gwroute)
    v6rt->rt_gwroute->rt_refcnt++;
  v4route.ro_rt = v6rt->rt_gwroute;
  bcopy(v4dst,&v4route.ro_dst,v4dst->sin_len);

  /*
   * Prepend IPv4 header.
   */
  M_PREPEND(outgoing,sizeof(struct ip), M_DONTWAIT);
  if (outgoing == NULL)
    return ENOBUFS;

  ip = mtod(outgoing,struct ip *);
  bzero(ip,sizeof(*ip));

  /*
   * Following four lines are done here rather than ip_output() because of
   * the *&*&%^^& don't fragment bit.
   */
  ip->ip_v = IPVERSION;
#if __OpenBSD__
  ip->ip_id = ip_randomid();
#else /* __OpenBSD__ */
  ip->ip_id = htons(ip_id++);
#endif /* __OpenBSD__ */
  ip->ip_hl = sizeof(*ip)>>2;
  if (v6rt->rt_rmx.rmx_mtu > IPV6_MINMTU)
    ip->ip_off |= IP_DF;
  ipstat.ips_localout++;

  if (v6rt->rt_flags & (RTF_HOST|RTF_GATEWAY))
    ip->ip_dst = v4dst->sin_addr;
  else
    {
      /*
       * If I'm in here, this means I'm not a host route, but when I was
       * supposed to clone, I was supposed to change the v4dst addr.
       *
       * This will only happen if I'm a v6-in-v4 router-to-host route,
       * in which case I have to do the translation on the fly, based on
       * the data in the IPv6 header.
       */
      if (!IN6_IS_ADDR_V4COMPAT(&ipv6->ipv6_dst))
	{
	  printf("Oooh boy, v6-in-v4 tunnel ( trans_output() ) trouble!!!\n");
	}
      ip->ip_dst.s_addr = ipv6->ipv6_dst.in6a_words[3];
    }

  ip->ip_src.s_addr = INADDR_ANY;
  ip->ip_p = IPPROTO_IPV6;
  ip->ip_ttl = ip_defttl;
  ip->ip_len = outgoing->m_pkthdr.len;

#ifdef IPSEC
  if (v6rt->rt_flags & (RTF_CRYPT|RTF_AUTH)) {
    /*
     * A secure route has hanging off its rt_netproc field something which
     * can be tagged onto an outgoing mbuf such that ipv4_output can
     * secure the IPv6-in-IPv4 packet.
     */
    DPRINTF(IDL_EVENT,("Secure route, sending cheesy socket.\n"));
    socket = v6rt->rt_netproc;
  } else
    socket = NULL;

  rc = ip_output(outgoing, NULL, &v4route, IP_RAWOUTPUT, NULL, socket);
#else /* IPSEC */
#if __bsdi__
  rc = ip_output(outgoing, NULL, &v4route, IP_RAWOUTPUT, NULL, NULL);
#else /* __bsdi__ */
  rc = ip_output(outgoing, NULL, &v4route, IP_RAWOUTPUT, NULL);
#endif /* __bsdi__ */
#endif /* IPSEC */

  if (rc == EMSGSIZE)
    {
      DPRINTF(IDL_ERROR,("Path MTU adjustment needed in trans_output().\n"));
    }
  if (v4route.ro_rt != NULL)
    RTFREE(v4route.ro_rt);
  return rc;
}

/*----------------------------------------------------------------------
 * "IPv6 in IPv6 tunnelling."
 *
 * Encapsulate IPv6 packet in another IPv6 packet.  This, in combination
 * with passing on a fake socket with a security request, can enable a
 * configured secure tunnel.
 *
 * Called in: (only) ipv6_tunnel_output() from this file.
 ----------------------------------------------------------------------*/

int
ipv6_encaps_output(outgoing, tundst, tunrt)
     struct mbuf *outgoing;
     struct sockaddr_in6 *tundst;
     struct rtentry *tunrt;
{
  struct route6 actroute;
  struct ipv6 *ipv6;
  int rc;
#ifdef IPSEC
  struct socket *socket;
#endif /* IPSEC */

DPRINTF(IDL_GROSS_EVENT,("\n\nipv6_encaps_output():0000-Hey!  I'm in IPV6_in_IPV6 tunnelling code!!\n"));
  if (tunrt->rt_flags & RTF_REJECT)
    {
      m_freem(outgoing);
      return (tunrt->rt_flags & RTF_HOST) ? EHOSTUNREACH : ENETUNREACH;
    }

  if (tunrt->rt_gwroute)
    tunrt->rt_gwroute->rt_refcnt++;
  actroute.ro_rt = tunrt->rt_gwroute;
  bcopy(tundst,&actroute.ro_dst,tundst->sin6_len);

  M_PREPEND(outgoing,sizeof(struct ipv6), M_DONTWAIT);
  if (outgoing == NULL)
    return ENOBUFS;

  ipv6 = mtod(outgoing,struct ipv6 *);
  bzero(ipv6,sizeof(*ipv6));
  
  ipv6->ipv6_versfl = htonl(0x60000000);
  ipv6->ipv6_length = outgoing->m_pkthdr.len - sizeof(struct ipv6);
  ipv6->ipv6_nexthdr = IPPROTO_IPV6;
  ipv6->ipv6_hoplimit = ipv6_defhoplmt;
  ipv6->ipv6_dst = tundst->sin6_addr;

#ifdef IPSEC
  if (tunrt->rt_flags & (RTF_CRYPT|RTF_AUTH)) {
    /*
     * A secure route has hanging off its rt_netproc field something which
     * can be tagged onto an outgoing mbuf such that ipv6_output can
     * secure the IPv6-in-IPv6 packet.
     */
    DPRINTF(IDL_EVENT,("ipv6_encaps_output():0500-Secure route, sending cheesy socket.\n"));
    socket = tunrt->rt_netproc;
  } else 
    socket = NULL;

  rc = ipv6_output(outgoing, &actroute, IPV6_RAWOUTPUT, NULL, NULL, socket);
#else /* IPSEC */
  rc = ipv6_output(outgoing, &actroute, IPV6_RAWOUTPUT, NULL, NULL, NULL);
#endif /* IPSEC */

  if (rc == EMSGSIZE)
    {
      DPRINTF(IDL_ERROR,("Path MTU adjustment needed in trans_output().\n"));
    }
  if (actroute.ro_rt != NULL)
    RTFREE(actroute.ro_rt);
  return rc;
  
}
/*----------------------------------------------------------------------
 * "IPv4 in IPv4 tunnelling"
 *
 * Output routine for IPv4 in IPv4.  Uses M_PREPEND to prepend an IPv4
 * header (i.e. the tunnel header), and then call ip_output() to
 * send the tunnel packet.  The v4 in v4 tunnel seems redundant, but is
 * useful for setting up secure tunnels.
 *
 * Called in: (only) ipv4_tunnel_output() from this file.
 ----------------------------------------------------------------------*/

int
ipv4_encaps_output(outgoing, v4tundst, v4rt)
     struct mbuf *outgoing;
     struct sockaddr_in *v4tundst;   /* Goto to destination tunnel endpoint. */
     struct rtentry *v4rt;           /* The encapsulated (i.e. passenger) IPv4
                                      *   packet's route.                 */
{
  struct route v4tunroute; /* The tunnel- (i.e. actual) route to the tunnel's
                            *   destination endpoint (i.e. to the other
                            *   side of the tunnel). */
  struct ip *ip;           /* For setting up the IPv4 data needed for the
                            *   tunnel IPv4 packet. */
  int rc;                  /* Return Code */
#ifdef IPSEC
  struct socket *socket;
#endif /* IPSEC */

  /*
   * Like all below-IP(v4) output routines, check RTF_REJECT flag.
   * Why?  We need to make sure the logical (i.e. encapsulated packet's)
   * route to the final destination is reachable.
   */
  if (v4rt->rt_flags & RTF_REJECT)
    {
      m_freem(outgoing);
      return (v4rt->rt_flags & RTF_HOST) ? EHOSTUNREACH : ENETUNREACH;
    }

  if (v4rt->rt_gwroute)
    v4rt->rt_gwroute->rt_refcnt++;
  v4tunroute.ro_rt = v4rt->rt_gwroute;
  bcopy(v4tundst,&v4tunroute.ro_dst,v4tundst->sin_len);

  /*
   * Prepend IPv4 header.
   */
  M_PREPEND(outgoing,sizeof(struct ip), M_DONTWAIT);
  if (outgoing == NULL)
    return ENOBUFS;

  ip = mtod(outgoing,struct ip *);
  bzero(ip,sizeof(*ip));

  /*
   *  Initialization of IP header:  We'll let  ip_output()  fill in
   *  IP's version, ip, offset, and header length as well as update
   *  ipstat.ips_localout.
   *
   *  Of course, we need to do the rest ...
   */

  ip->ip_src.s_addr = INADDR_ANY;
  ip->ip_p = IPPROTO_IPV4;
  ip->ip_ttl = ip_defttl;
  ip->ip_len = outgoing->m_pkthdr.len;

  ip->ip_dst = v4tundst->sin_addr;

#ifdef IPSEC
  if (v4rt->rt_flags & (RTF_CRYPT|RTF_AUTH)) {
    /*
     * A secure route has hanging off its rt_netproc field something which
     * can be tagged onto an outgoing mbuf such that ipv4_output can
     * secure the IPv4-in-IPv4 packet.
     */
    DPRINTF(IDL_EVENT,("ipv4_encaps_output():0500-Secure route, sending cheesy socket.\n"));
    socket = v4rt->rt_netproc;
  } else
    socket = NULL;

  rc = ip_output(outgoing, NULL, &v4tunroute, 0, NULL, socket);
#else /* IPSEC */
#if __bsdi__
  rc = ip_output(outgoing, NULL, &v4tunroute, 0, NULL, NULL);
#else /* __bsdi__ */
  rc = ip_output(outgoing, NULL, &v4tunroute, 0, NULL);
#endif /* __bsdi__ */
#endif /* IPSEC */

  if (v4tunroute.ro_rt != NULL)
    RTFREE(v4tunroute.ro_rt);
  return rc;
}

/*----------------------------------------------------------------------
 *"IPv4 in IPv6"  
 *
 * Encapsulate IPv4 packet in a IPv6 packet.
 *
 * Called in: (only) ipv4_tunnel_output() from this file.
 *---------------------------------------------------------------------*/

int
ipv4_trans_output(outgoing, tunv6dst, v4rt)
     struct mbuf *outgoing;
     struct sockaddr_in6 *tunv6dst;
     struct rtentry *v4rt;
{
  struct route6 tunv6route;
  struct ipv6 *ipv6;
  int rc;
#ifdef IPSEC
  struct socket *socket;
#endif /* IPSEC */

  if (v4rt->rt_flags & RTF_REJECT)
    {
      m_freem(outgoing);
      return (v4rt->rt_flags & RTF_HOST) ? EHOSTUNREACH : ENETUNREACH;
    }

  if (v4rt->rt_gwroute)
    v4rt->rt_gwroute->rt_refcnt++;
  tunv6route.ro_rt = v4rt->rt_gwroute;
  bcopy(tunv6dst,&tunv6route.ro_dst,tunv6dst->sin6_len);

  M_PREPEND(outgoing,sizeof(struct ipv6), M_DONTWAIT);
  if (outgoing == NULL)
    return ENOBUFS;

  ipv6 = mtod(outgoing,struct ipv6 *);
  bzero(ipv6,sizeof(*ipv6));

  ipv6->ipv6_versfl = htonl(0x60000000);
  ipv6->ipv6_length = outgoing->m_pkthdr.len - sizeof(struct ipv6);
  ipv6->ipv6_nexthdr = IPPROTO_IPV4;
  ipv6->ipv6_hoplimit = ipv6_defhoplmt;
  ipv6->ipv6_dst = tunv6dst->sin6_addr;

#ifdef IPSEC
  if (v4rt->rt_flags & (RTF_CRYPT|RTF_AUTH)) {
    /*
     * A secure route has hanging off its rt_netproc field something which
     * can be tagged onto an outgoing mbuf such that ipv6_output can
     * secure the IPv4-in-IPv6 packet.
     */
    DPRINTF(IDL_EVENT,("ipv4_trans_output():0500-Secure route, sending cheesy socket.\n"));
    socket = v4rt->rt_netproc;
  } else
    socket = NULL;

  rc = ipv6_output(outgoing, &tunv6route, IPV6_RAWOUTPUT, NULL, NULL, socket);
#else /* IPSEC */
  rc = ipv6_output(outgoing, &tunv6route, IPV6_RAWOUTPUT, NULL, NULL, NULL);
#endif /* IPSEC */

  if (rc == EMSGSIZE)
    {
      DPRINTF(IDL_ERROR,("Path MTU adjustment needed in trans_output().\n"));
    }
  if (tunv6route.ro_rt != NULL)
    RTFREE(tunv6route.ro_rt);
  return rc;
}
  

/*----------------------------------------------------------------------
 * Called by ipv6_output if the RTF_TUNNEL bit is set on a route,
 * this function examines the route, and sees what sort of encapsulation is
 * needed.  Often, the rt->rt_gateway sockaddr is used to figure this out.
 ----------------------------------------------------------------------*/

int
ipv6_tunnel_output(outgoing, dst, rt)
     struct mbuf *outgoing;
     struct sockaddr_in6 *dst;
     struct rtentry *rt;
{
  DPRINTF(IDL_EVENT,("\n\nipv6_tunnel_output():0000-Just entered.\n"));

  /*
   * Determine what type of tunnel it is with rt.  Perform correct kind
   * of encapsulation (in IPv4, ESP, etc.) and call output routine of
   * what you want encapsulated.
   */

  /* IPv6 in IPv4. */
  if (rt->rt_gateway != NULL && rt->rt_gateway->sa_family == AF_INET)
    return ipv6_trans_output(outgoing,(struct sockaddr_in *)rt->rt_gateway,rt);

  /* IPv6 in IPv6. */
  if (rt->rt_gateway != NULL && rt->rt_gateway->sa_family == AF_INET6)
    return ipv6_encaps_output(outgoing,(struct sockaddr_in6 *)rt->rt_gateway,rt);

  m_freem(outgoing);
  return EHOSTUNREACH;
}


/*----------------------------------------------------------------------
 * Called by ip_output if the RTF_TUNNEL bit is set on a route,
 * this function examines the route, and sees what sort of encapsulation is
 * needed.  Often, the rt->rt_gateway sockaddr is used to figure this out.
 ----------------------------------------------------------------------*/

int
ipv4_tunnel_output(outgoing, dst, rt)
     struct mbuf *outgoing;
     struct sockaddr_in *dst;
     struct rtentry *rt;
{
  DPRINTF(IDL_EVENT,("\n\nipv4_tunnel_output():0000-Just entered.\n"));

  /*
   * Determine what type of tunnel it is with rt.  Perform correct kind
   * of encapsulation (in IPv4, ESP, etc.) and call output routine of
   * what you want encapsulated.
   */

  /* IPv4 in IPv6. */
  if (rt->rt_gateway != NULL && rt->rt_gateway->sa_family == AF_INET6)
    return ipv4_trans_output(outgoing,(struct sockaddr_in6 *)rt->rt_gateway,rt);

  /* IPv4 in IPv4. */
  if (rt->rt_gateway != NULL && rt->rt_gateway->sa_family == AF_INET)
    return ipv4_encaps_output(outgoing,(struct sockaddr_in *)rt->rt_gateway,rt);

  m_freem(outgoing);
  return EHOSTUNREACH;
}
