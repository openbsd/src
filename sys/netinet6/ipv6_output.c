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
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_var.h>

#if __OpenBSD__ && NRL_IPSEC
#define IPSEC 1
#endif /* __OpenBSD__ && NRL_IPSEC */

#if IPSEC
#include <sys/osdep.h>
#include <net/netproc.h>
#include <net/netproc_var.h>
#include <sys/nbuf.h>
#endif /* IPSEC */

#include <sys/debug.h>

/*
 * Globals and function definitions.
 */

uint32_t outfragid = 0;  /* Outbound fragment groups have unique id's. */
struct mbuf *ipv6_fragment __P((struct mbuf *,int));

/*
 * External globals.
 */

extern struct ipv6stat ipv6stat;
extern struct in6_ifaddr *in6_ifaddr;
extern struct in6_ifnet *in6_ifnet;
extern struct ifnet *mcastdefault;
extern int ipv6_defhoplmt;

#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
extern struct ifnet *loifp;
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */

void ipv6_nsolicit __P((struct ifnet *, struct mbuf *, struct rtentry *));
int ipv6_tunnel_output __P((struct mbuf *, struct sockaddr_in6 *, struct rtentry *));

int ipv6_setmoptions __P((int, struct inpcb *, void *, size_t));
int ipv6_getmoptions __P((int, struct ipv6_moptions *, int *));
void ipv6_mloopback __P((struct ifnet *, struct mbuf *, struct sockaddr_in6 *));
int ipv6_finddivpoint __P((struct mbuf *, uint8_t *));

int ipv6_controltoheader(struct mbuf **m, struct mbuf *control, struct ifnet **forceifp, int *);

/*----------------------------------------------------------------------
 * IPv6 output routine.  The mbuf chain contains a near-complete IPv6 header,
 * and an already-inserted list of options.  (I figure it's something for
 * the code with PCB access to handle.)  The options should have their
 * fields in network order.  The header should have its fields in host order.
 * (Save the addresses, which IMHO are always in network order.  Weird.)
 ----------------------------------------------------------------------*/

int
ipv6_output(outgoing,ro,flags,i6mo, forceifp, socket)
     struct mbuf *outgoing;
     struct route6 *ro;
     int flags;
     struct ipv6_moptions *i6mo;
     struct ifnet *forceifp;
     struct socket *socket;
{
  struct ipv6 *header;
  struct route6 ipv6route;
  struct sockaddr_in6 *dst;
  struct in6_ifaddr *i6a = NULL;
  struct ifnet *ifp = NULL;
  int error=0;
  uint32_t outmtu = 0;

#ifdef DIAGNOSTIC
  if ((outgoing->m_flags & M_PKTHDR) == 0)
    panic("ipv6_output() no HDR");
#endif

  /*
   * Assume the IPv6 header is already contiguous.
   */
  header = mtod(outgoing, struct ipv6 *);
  
  DDO(IDL_FINISHED,printf("ipv6_output:\n");dump_ipv6(header));
  DPRINTF(IDL_FINISHED,("\n"));

  /* 
   * Fill in v6 header.  Assume flow id/version field is in network order,
   * and that the high 4 bits are 0's. 
   */
  
  if ((flags & (IPV6_FORWARDING|IPV6_RAWOUTPUT)) == 0)
    {
      header->ipv6_versfl = htonl(0x60000000) | 
	(header->ipv6_versfl & htonl(0x0fffffff));
      ipv6stat.ips_localout++;
    }
  
  /*
   * Determine interface and physical destination to send datagram out
   * towards.  Do this by looking up a route, or using the route we were
   * passed.
   */

  DPRINTF(IDL_FINISHED,("route passed to ipv6_output is:\n"));
  DDO(IDL_FINISHED,if (ro) dump_rtentry(ro->ro_rt));
  if (ro == 0)
    {
      ro = &ipv6route;
      bzero((caddr_t)ro,sizeof(struct route6));
    }
  dst = &ro->ro_dst;
  
  if (ro->ro_rt && ((ro->ro_rt->rt_flags & RTF_UP) == 0 ||
		    !IN6_ARE_ADDR_EQUAL(&dst->sin6_addr, &header->ipv6_dst)))
    {
      RTFREE(ro->ro_rt);
      ro->ro_rt = NULL;
    }
  
  if (ro->ro_rt == NULL)
    {
      dst->sin6_family = AF_INET6;
      dst->sin6_len = sizeof(struct sockaddr_in6);
      dst->sin6_addr = header->ipv6_dst;
      dst->sin6_port = 0;
      dst->sin6_flowinfo = 0;
    }
  
#define ifatoi6a(ifa) ((struct in6_ifaddr *)(ifa))
#define sin6tosa(sin6) ((struct sockaddr *)(sin6))
  
  if (flags & IPV6_ROUTETOIF)
    {
      /*
       * Check for route to interface only.  (i.e. the user doesn't want
       * to rely on the routing tables, so send it out an interface).
       */
      if ((i6a = ifatoi6a(ifa_ifwithdstaddr(sin6tosa(dst)))) == 0 &&
	  (i6a = ifatoi6a(ifa_ifwithnet(sin6tosa(dst)))) == 0 )
	{
	  /*
	   * Q:  Do we want to assume that if a user specifies this option,
	   *     the user doesn't want ANYTHING to do with the routing tables?
	   */

	  ipv6stat.ips_noroute++;
	  error = ENETUNREACH;
	  goto bad;
	}
      ifp = i6a->i6a_ifp;
      header->ipv6_hoplimit = 1;
      outmtu = ifp->if_mtu;
    }
  else
    {
      /*
       * Do normal next-hop determination with the help of the routing tree.
       */
      if (ro->ro_rt == 0)
	rtalloc((struct route *)ro);  /* Initial route lookup. */

      if (ro->ro_rt == 0)
	{
	  /*
	   * No route of any kind, so spray neighbor solicits out all
	   * interfaces, unless it's a multicast address.
	   */
	  if (IN6_IS_ADDR_MULTICAST(&header->ipv6_dst))
	    goto mcast;
	  DPRINTF(IDL_FINISHED, ("v6_output doesn't have a route...calling onlink_query!\n"));
	  ipv6_onlink_query(dst);
	  rtalloc((struct route *)ro);
	}
      if (ro->ro_rt == NULL)
	{
	  /*
	   * ipv6_onlink_query() should've added a route.  Probably
	   * failed.
	   */
	  DPRINTF(IDL_GROSS_EVENT, ("v6_output: onlink_query didn't add route!\n"));
	  ipv6stat.ips_noroute++;
	  error = ENETUNREACH;
	  goto bad;
	}

      if (ro->ro_rt->rt_ifa == NULL)
	{
	  /*
	   * We have a route where we don't quite know which interface 
	   * the neighbor belongs to yet.  If I get here, I know that this
	   * route is not pre-allocated (such as done by in6_pcbconnect()),
	   * because those pre-allocators will do the same ipv6_onlink_query()
	   * and ipv6_verify_onlink() in advance.
	   *
	   * I can therefore free the route, and get it again.
	   * Multicast dgrams should NEVER be in this code path.
	   */

	  RTFREE(ro->ro_rt);
	  ro->ro_rt = NULL;
	  DPRINTF(IDL_FINISHED,("v6_output calling ipv6_verify_onlink\n"));
	  if ((error = ipv6_verify_onlink(dst)) != 0)
	    {
	      if (error == -1)
		{
		  DPRINTF(IDL_ERROR,("verify_onlink() failed in v6_out.\n"));
		  error = ENETUNREACH;
		}
	      ipv6stat.ips_noroute++;   /* Better stat needed, because
					   error might not be
					   E{NET,HOST}UNREACH. */
	      
	      goto bad;
	    }
	  rtalloc((struct route *)ro);
	  if (ro->ro_rt == NULL || ro->ro_rt->rt_ifa == NULL)
	    panic("Oops, I'm forgetting something after verify_onlink().");
	}

      /*
       * Exploit properties of route.
       */
      
      ifp = ro->ro_rt->rt_ifp;            /* Q:  Is this REALLY the ifp
					     for the route?
					     
					     A:  Maybe.  If multi-homed,
					         and we attempt to
						 intelligently figure out
						 where link-locals are
						 destined, then we're
						 in trouble. */
      /*
       * On-net route exists, but no destination as of yet.  This can
       * be snipped if an ifp is just selected.  (Depends on multihomed
       * experience.)
       *
       * Currently, this code never executes, because we guarantee rt_ifp is
       * set.  This may, however, change in later versions of this code as
       * we gain multihomed experience.
       */
      if (ifp == NULL && ro->ro_rt->rt_gateway->sa_family == AF_LINK)
	{
	  DPRINTF(IDL_EVENT,\
		  ("ipv6_output() calling ipv6_nsolicit(2)\n"));
	  ipv6_nsolicit(NULL, outgoing, ro->ro_rt);
	  DPRINTF(IDL_EVENT,\
		  ("ipv6_output() attempted to send neighbor solicit(2), returning.\n"));
	  goto done;
	}

      /*
       * Q:  What if address has expired?  Perhaps I should use ifp to
       * obtain optimal i6a value.   There's also the question of using
       * link-local source addresses for off-link communication.  (or for
       * that matter, on-link, but not link-local communication.
       *
       * Q2:  Perhaps use this time to reset the route's ifa?
       * Q3:  Perhaps it is better to use the ipv6_rtrequest()?
       * 
       * Regardless, i6a's only use in this function is to determine the
       * source address of the packet.
       *
       * Currently, ipv6_rtrequest() attempts to store a decent in6_ifaddr
       * in rt_ifa.  This also may change with experience.
       */

      i6a = ifatoi6a(ro->ro_rt->rt_ifa);
      if (i6a->i6a_addrflags & I6AF_NOTSURE) 
	if (!(outgoing->m_flags & M_DAD))
	  {
	    /*
	     * 1. Think of a better error.
	     *
	     * 2. Keep some sort of statistic.
	     */
	    DPRINTF(IDL_ERROR,("Using NOTSURE source address.\n"));
	    error = EADDRNOTAVAIL;
	    goto bad;
	  }
	else i6a = NULL;

      /*
       * More source address selection goes here.
       */

      ro->ro_rt->rt_use++;
      /*
       * Path MTU comes from the route entry.
       */
      outmtu = ro->ro_rt->rt_rmx.rmx_mtu;
      
      if (ro->ro_rt->rt_flags & RTF_GATEWAY)  /* Gateway/router/whatever. */
	dst = (struct sockaddr_in6 *)ro->ro_rt->rt_gateway;
    }

  if (forceifp) {
    DPRINTF(IDL_EVENT, ("ipv6_output: in forceifp case\n"));
    ifp = forceifp;
    if (outmtu > ifp->if_mtu)
      outmtu = ifp->if_mtu;
  };

  /*
   * Handle case of a multicast destination.
   */
 mcast:
  if (IN6_IS_ADDR_MULTICAST(&header->ipv6_dst))
    {
      struct in6_multi *in6m;

      outgoing->m_flags |= M_MCAST;
      
      dst = &ro->ro_dst;

      if (i6mo != NULL)
	{
	  /*
	   * As we gain more multicast experience, use i6mo fields to alter
	   * properties of outgoing packet.  (Including, quite possibly,
	   * the source address.)
	   */
	  if (i6mo->i6mo_multicast_ifp != NULL)
	    ifp = i6mo->i6mo_multicast_ifp;
	  header->ipv6_hoplimit = i6mo->i6mo_multicast_ttl;
	}
      else
	{
	  /*
	   * Use default values, since there are no multicast options to
	   * use.
	   */
	  if (ifp == NULL)
	    ifp = mcastdefault;
	  header->ipv6_hoplimit = IPV6_DEFAULT_MCAST_HOPS;
	}

      if (outmtu == 0)         /* But what about mcast Path MTU? */
	outmtu = ifp->if_mtu;

      if ((ifp->if_flags & IFF_MULTICAST) == 0)
	{
	  ipv6stat.ips_noroute++;
	  error = ENETUNREACH;
	  goto bad;
	}

      if ((IN6_IS_ADDR_UNSPECIFIED(&header->ipv6_src) && !(outgoing->m_flags & M_DAD)) ||
	  (IN6_IS_ADDR_LINKLOCAL(&header->ipv6_src) && 
	   GET_IN6_MCASTSCOPE(header->ipv6_dst) > IN6_INTRA_LINK))
	{
	  register struct in6_ifaddr *i6a;
	  
	  /*
	   * Source address selection for multicast datagrams.
	   * If link-local source, get in here too, because you don't want
	   * link-local addresses going on non-local multicast.
	   *
	   * Eventually should fix this to perform best source address
	   * selection.  Probably should separate this out into a function.
	   */
	  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
	    if (i6a->i6a_ifp == ifp)
	      {
		header->ipv6_src = I6A_SIN(i6a)->sin6_addr;
		break;
              }
        }

      IN6_LOOKUP_MULTI(&header->ipv6_dst, ifp, in6m);
      DPRINTF(IDL_GROSS_EVENT,("in6m == 0x%lx, i6mo == 0x%lx\n", (unsigned long)in6m, (unsigned long)i6mo));
      if (in6m != NULL &&
	  (i6mo == NULL || i6mo->i6mo_multicast_loop))
	{

	  /*
	   * See ipv6_mloopback for details, but that function will tag
	   * the packet with the actual interface the multicast is
	   * supposed to go out.  This makes duplicate address detection
	   * harder to implement, because the inbound mbuf SHOULD be tagged
	   * as coming from me for the case of solicits.  (Perhaps burning
	   * another flag...)
	   */
	  DPRINTF(IDL_GROSS_EVENT,("Calling ipv6_mloopback().\n"));
	  ipv6_mloopback(ifp, outgoing, dst);
	}

#ifdef MROUTING
      /*
       * Do m-cast routing even if I can't find it in my m-cast list.
       */
#endif

      /*
       * If intra-node scope.  I've already hit it with ipv6_mloopback above.
       */

      if (GET_IN6_MCASTSCOPE(header->ipv6_dst) == IN6_INTRA_NODE || (ifp->if_flags & IFF_LOOPBACK))
	goto bad;  /* Not really bad, y'know, just getting out of here. */
    }

  if (ro->ro_rt == NULL && outmtu == 0)
    panic("ipv6_output: How did I get here without a route or MTU?");

  /*
   * Specify source address.  Use i6a, for now.
   */

  if (IN6_IS_ADDR_UNSPECIFIED(&header->ipv6_src) && i6a != NULL && 
      !(outgoing->m_flags & M_DAD))
    header->ipv6_src = I6A_SIN(i6a)->sin6_addr;

  DPRINTF(IDL_FINISHED,("header & chain before security check are:\n"));
  DDO(IDL_FINISHED,dump_ipv6(header));
  DDO(IDL_FINISHED,dump_mchain(outgoing));

#ifdef IPSEC
  if (!(flags & IPV6_FORWARDING)) {
	  size_t preoverhead, postoverhead;
	  void *state;

	  /* NB: If there exists a configured secure tunnel, then
	     the packet being tunneled will have been encapsulated
	     inside an IP packet with (src=me, dest=tunnel-end-point)
	     PRIOR to ip_output() being called, so the above
	     check doesn't preclude secure tunnelling.  rja */
	  /*
	   * I would like to just pass in &ia->ia_addr, but there is a small
	   * chance that the source address doesn't match ia->ia_addr.
	   *
	   * Also, if you need a dest. port, fill in ro->ro_dst with it.
	   */
	  {
	    struct sockaddr_in6 srcsa, dstsa;

	    bzero(&srcsa, sizeof(struct sockaddr_in6));
	    srcsa.sin6_family = AF_INET6;
	    srcsa.sin6_len = sizeof(struct sockaddr_in6);
	    /* XXX - port */
	    srcsa.sin6_addr = header->ipv6_src;

	    bzero(&dstsa, sizeof(struct sockaddr_in6));
	    dstsa.sin6_family = AF_INET6;
	    dstsa.sin6_len = sizeof(struct sockaddr_in6);
	    /* XXX - port */
	    dstsa.sin6_addr = header->ipv6_dst;

	    /* XXX - get the ULP protocol number */
	    if (error = netproc_outputpolicy(socket, (struct sockaddr *)&srcsa,
	        (struct sockaddr *)&dstsa, header->ipv6_nexthdr, &preoverhead,
	        &postoverhead, &state)) {
	      if (error == EACCES) /* XXX - means fail silently */
		error = 0;
	      goto bad;
	    }
	  }

	  if (state) {
	    struct nbuf *nbuf;

	    DP(FINISHED, preoverhead, d);
	    DP(FINISHED, postoverhead, d);
	    {
	      struct netproc_policycache *policycache =
		(struct netproc_policycache *)state;

	      DP(FINISHED, policycache->doah, d);
	      DP(FINISHED, policycache->doesp, d);
	      DP(FINISHED, policycache->docombinedesp, d);
	    }

	    if (!(nbuf = mton(outgoing, preoverhead, postoverhead))) {
	      netproc_outputfree(state);
	      error = ENOMEM;
	      goto bad;
	    }

	    outgoing = NULL;

	    if (error = netproc_output(state, nbuf)) {
	      if (error == EACCES)
		error = 0;
	    }

	    /* If successful, netproc_output actually does the output.
	       Either way, it frees the nbuf. */
	    goto done;
	  }
    }
#endif /* defined(IPSEC) || defined(NRL_IPSEC) */

  /*
   * Assume above three return a contiguous and UPDATED IPv6 header.
   */
  header = mtod(outgoing,struct ipv6 *);

  /*
   * Determine the outbound i6a to record statistics.  
   */
  if (flags & IPV6_FORWARDING)
    i6a = NULL;
  else if (i6a == NULL ||
	!IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &header->ipv6_src))
    {
      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
	if (i6a->i6a_ifp == ifp &&
	    IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &header->ipv6_src))
	  break;
    }

  /*
   * If small enough for path MTU, send, otherwise, fragment.
   */

  DPRINTF(IDL_FINISHED,("Before output, path mtu = %d, header is:\n",\
			   (int)outmtu));
  DDO(IDL_FINISHED,dump_ipv6(header));
  DDO(IDL_FINISHED,printf("Chain is:\n");dump_mchain(outgoing));

#if 0
/* DEBUG tunnel */
  DPRINTF(IDL_EVENT,("ROUTE passed to ipv6_output is:\n"));
  DDO(IDL_EVENT,if (ro) dump_rtentry(ro->ro_rt));
      if (ro->ro_rt && (ro->ro_rt->rt_flags & RTF_TUNNEL))
	  DPRINTF(IDL_FINISHED,("HEY !!  I see the tunnel!!!\n"));
      else {
	    DPRINTF(IDL_FINISHED,("HEY !!  I can't see the tunnel!!!\n"));
            if (ro->ro_rt == NULL)
                DPRINTF(IDL_FINISHED,("ro->ro_rt is null!!\n"));
            else
                 {
                   DPRINTF(IDL_FINISHED,("ro->ro_rt is not null!!\n"));
                   if (ro->ro_rt->rt_flags & RTF_TUNNEL)
                     DPRINTF(IDL_FINISHED,("HEY, I can see RTFTUNNEL!\n"));
                   else
                     DPRINTF(IDL_FINISHED,("HEY, I can't see RTFTUNNEL!\n"));
                 }
      }
/* END OF DEBUG tunnel */
#endif /* 0 */

  if (outgoing->m_pkthdr.len <= outmtu)
    {
DPRINTF(IDL_EVENT,("IPV6_OUTPUT():    Not entering fragmenting engine.\n"));
      header->ipv6_length = htons(outgoing->m_pkthdr.len - 
				  sizeof(struct ipv6));

      /*
       * If there is a route, and its TUNNEL bit is turned on, do not send
       * out the interface, but send through a tunneling routine, which will,
       * given information from the route, encapsulate the packet accordingly.
       *
       * Keith Sklower suggested a "rt_output() method" which would save
       * the checking here.
       */
      if (ro->ro_rt && (ro->ro_rt->rt_flags & RTF_TUNNEL)) {
	DPRINTF(IDL_EVENT,("ipv6_output():-Sending out IPV6 in IPV4/6 tunnel.\n"));
	error = ipv6_tunnel_output(outgoing, dst, ro->ro_rt);
      } else {
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
	if (i6a) {
	      i6a->i6a_ifa.ifa_opackets++;
	      i6a->i6a_ifa.ifa_obytes += outgoing->m_pkthdr.len;
	}
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
	error = (*ifp->if_output)(ifp, outgoing, (struct sockaddr *)dst, ro->ro_rt);
      }
      DPRINTF(IDL_FINISHED,("Lone IPv6 went out if (error = %d).\n",error));
      goto done;
    }


  /*
   * If I make it here, then the packet is too big for the path MTU, and must
   * be fragmented.
   */

  DPRINTF(IDL_EVENT,("IPV6_OUTPUT():    Entering fragmenting engine.\n"));

  if (flags & IPV6_FORWARDING)
    {
      error = EMSGSIZE;
      goto bad;
    }

  if (outgoing->m_pkthdr.len > 0xffff) {
    DPRINTF(IDL_ERROR,("Jumbogram needs fragmentation, something that can't be done\n"));
    ipv6stat.ips_odropped++;  /* ?!? */
    error = EINVAL;
    goto bad;
  }

  /*
   * The following check should never really take place.
   */
#ifdef DIAGNOSTIC
  if (outmtu < IPV6_MINMTU)
    {
      DPRINTF(IDL_ERROR,("Outbound MTU is less than IPV6_MINMTU (%d).\n",\
			    IPV6_MINMTU));
      error = ENETUNREACH;  /* Can you think of a better idea? */
      goto bad;
    }
#endif

  /*
   * ipv6_fragment returns a chain of outgoing packets.  It returns NULL
   * if something went wrong.
   */
  outgoing = ipv6_fragment(outgoing,outmtu);
  if (outgoing == NULL)
    error = ENOBUFS;    /* Can you think of a better idea? */

  DPRINTF(IDL_FINISHED,\
       ("ipv6_fragment() returned, attempting to send fragments out.\n"));

  /*
   * Walk through chain of fragments, and send them out.
   */
  while (outgoing != NULL)
    {
      struct mbuf *current = outgoing;

      DPRINTF(IDL_FINISHED,("In fragment-sending loop, error == %d.\n",\
			       error));
      outgoing = current->m_nextpkt;
      current->m_nextpkt = NULL;

      DDO(IDL_FINISHED,printf("Current (0x%lx) 1st mbuf is:\n", (unsigned long)current);\
	  dump_mbuf(current));

      if (error != 0)
	m_freem(current);
      else
	if (ro->ro_rt && (ro->ro_rt->rt_flags & RTF_TUNNEL)) {
	  DPRINTF(IDL_EVENT,("Sending fragments out tunnel.\n"));
	  error = ipv6_tunnel_output(current, dst, ro->ro_rt);
	} else {
          DPRINTF(IDL_EVENT,("After if_output(), error == %d.\n",error));
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
          if (i6a) {
            i6a->i6a_ifa.ifa_opackets++;
            i6a->i6a_ifa.ifa_obytes += current->m_pkthdr.len;
          }
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
          error = (*ifp->if_output)(ifp, current,(struct sockaddr *)dst, ro->ro_rt);
	}
    }

  if (error == 0)
    ipv6stat.ips_fragmented++;

done:
  if (ro == &ipv6route && (flags & IPV6_ROUTETOIF) == 0 && ro->ro_rt)
    RTFREE(ro->ro_rt);
  return (error);

bad:
  if (outgoing != NULL)
    m_freem(outgoing);
  goto done;
}

#define INDEX_TO_IFP(index, ifp)\
{\
   struct in6_ifnet *i6ifp; \
   for (i6ifp = in6_ifnet; i6ifp; i6ifp = i6ifp->i6ifp_next) \
     if (i6ifp->i6ifp_ifp->if_index == index) { \
       (ifp) = i6ifp->i6ifp_ifp; \
       break; \
     }; \
} \

/*----------------------------------------------------------------------
 * Set IPv6 multicast options.
 ----------------------------------------------------------------------*/
int ipv6_setmoptions(int optname, struct inpcb *inp, void *p, size_t len)
{
  register int error = 0;
  register int i;
  register struct ipv6_mreq *mreq;
  register struct ifnet *ifp = NULL;
  register struct ipv6_moptions *imo = inp->inp_moptions6;
  struct route6 ro;

  if (imo == NULL)
    {
      imo = (struct ipv6_moptions *)malloc(sizeof(*imo), M_IPMOPTS,M_WAITOK);
      if (imo == NULL)
	return ENOBUFS;
      inp->inp_moptions6 = imo;
      inp->inp_flags |= INP_IPV6_MCAST;
      imo->i6mo_multicast_ifp = NULL;
      imo->i6mo_multicast_ttl = IPV6_DEFAULT_MCAST_HOPS;
      imo->i6mo_multicast_loop = IPV6_DEFAULT_MCAST_LOOP;
      imo->i6mo_num_memberships = 0;
    }
  else   /* Only if points to v6 moptions can I set them! */
    if (!(inp->inp_flags & INP_IPV6_MCAST))
      return EEXIST;

  switch (optname)
    {
    case IPV6_MULTICAST_IF:
      {
	unsigned int index;
	if (!p || (len != sizeof(unsigned int))) {
	  error = EINVAL;
	  break;
	}

	index = *((int *)p);

	if (!index) {
	  imo->i6mo_multicast_ifp = NULL;
	  break;
	}

	INDEX_TO_IFP(index, ifp);
	if (!ifp || !(ifp->if_flags & IFF_MULTICAST))
	  error = EADDRNOTAVAIL;
	else
	  imo->i6mo_multicast_ifp = ifp;
      };
      break;

    case IPV6_MULTICAST_HOPS:
      /*
       * Set the IPv6 hop limit for outgoing multicast packets.
       */
      if (!p || (len != sizeof(int))) {
	error = EINVAL;
	break;
      }
      if (*((int *)p) == -1)
	imo->i6mo_multicast_ttl = IPV6_DEFAULT_MCAST_HOPS;
      else
	if ((*((int *)p) > -1) && (*((int *)p) < 256))
	  imo->i6mo_multicast_ttl = *((int *)p);
	else
	  error = EINVAL;
      break;
      
    case IPV6_MULTICAST_LOOP:
      /*
       * Set the loopback flag for outgoing multicast packets.
       * Must be zero or one.
       */
      if (!p || (len != sizeof(int))) {
	error = EINVAL;
	break;
      }
      switch(*((int *)p)) {
        case 0:
        case 1:
	  imo->i6mo_multicast_loop = *((int *)p);
	  break;
        case -1:
	  imo->i6mo_multicast_loop = IPV6_DEFAULT_MCAST_LOOP;
	  break;
        default:
	  error = EINVAL;
	  break;
      };
      break;
    case IPV6_ADD_MEMBERSHIP:
      /*
       * Add a multicast group membership.
       * Group must be a valid IP multicast address.
       */
      if (!p || (len != sizeof(struct ipv6_mreq))) {
	error = EINVAL;
	break;
      }
      mreq = (struct ipv6_mreq *)p;
      if (!IN6_IS_ADDR_MULTICAST(&mreq->ipv6mr_multiaddr)) {
	error = EINVAL;
	break;
      }
      /*
       * If no interface address was provided, use the interface of
       * the route to the given multicast address.
       */
      if (!mreq->ipv6mr_interface) {
	  ro.ro_rt = NULL;
	  ro.ro_dst.sin6_family = AF_INET6;
	  ro.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
	  ro.ro_dst.sin6_addr = mreq->ipv6mr_multiaddr;
	  rtalloc((struct route *)&ro);
	  if (ro.ro_rt == NULL)
	    {
	      error = EADDRNOTAVAIL;
	      break;
	    }
	  ifp = ro.ro_rt->rt_ifp;
	  rtfree(ro.ro_rt);
	}
      else {
	INDEX_TO_IFP(mreq->ipv6mr_interface, ifp);
      }
      /*
       * See if we found an interface, and confirm that it
       * supports multicast.
       */
      if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0) {
	error = EADDRNOTAVAIL;
	break;
      }
      /*
       * See if the membership already exists or if all the
       * membership slots are full.
       */
      for (i = 0; i < imo->i6mo_num_memberships; ++i) {
	if (imo->i6mo_membership[i]->in6m_ifp == ifp &&
	    IN6_ARE_ADDR_EQUAL(&mreq->ipv6mr_multiaddr,
			       &imo->i6mo_membership[i]->in6m_addr))
	  break;
      }
      if (i < imo->i6mo_num_memberships) {
	error = EADDRINUSE;
	break;
      }
      if (i == IN6_MAX_MEMBERSHIPS) {
	error = ETOOMANYREFS;
	break;
      }
      /*
       * Everything looks good; add a new record to the multicast
       * address list for the given interface.
       */
      if ((imo->i6mo_membership[i] = in6_addmulti(&mreq->ipv6mr_multiaddr, ifp))
	  == NULL)
	{
	  error = ENOBUFS;
	  break;
	}
      ++imo->i6mo_num_memberships;
      break;

    case IPV6_DROP_MEMBERSHIP:
      /*
       * Drop a multicast group membership.
       * Group must be a valid IP multicast address.
       */
      if (!p || (len != sizeof(struct ipv6_mreq))) {
	error = EINVAL;
	break;
      }
      mreq = (struct ipv6_mreq *)p;
      if (!IN6_IS_ADDR_MULTICAST(&mreq->ipv6mr_multiaddr)) {
	error = EINVAL;
	break;
      }

      /*
       * If an interface index was specified, get a pointer to its ifnet
       * structure.
       */
      if (!mreq->ipv6mr_interface)
	ifp = NULL;
      else {
	INDEX_TO_IFP(mreq->ipv6mr_interface, ifp);
	if (ifp == NULL) {
	  error = EADDRNOTAVAIL;
	  break;
	}
      }

      /*
       * Find the membership in the membership array.
       */
      for (i = 0; i < imo->i6mo_num_memberships; ++i) {
	if ((ifp == NULL ||
	     imo->i6mo_membership[i]->in6m_ifp == ifp) &&
	    IN6_ARE_ADDR_EQUAL(&imo->i6mo_membership[i]->in6m_addr,
			       &mreq->ipv6mr_multiaddr))
	  break;
      }

      if (i == imo->i6mo_num_memberships) {
	error = EADDRNOTAVAIL;
	break;
      }
      /*
       * Give up the multicast address record to which the
       * membership points.
       */
      in6_delmulti(imo->i6mo_membership[i]);
      /*
       * Remove the gap in the membership array.
       */
      for (++i; i < imo->i6mo_num_memberships; ++i)
	imo->i6mo_membership[i-1] = imo->i6mo_membership[i];

      --imo->i6mo_num_memberships;

      break;
    default:
      error = EOPNOTSUPP;
      break;
    }
  
  if (imo->i6mo_multicast_ifp == NULL &&
      imo->i6mo_multicast_ttl == IPV6_DEFAULT_MCAST_HOPS &&
      imo->i6mo_multicast_loop == IPV6_DEFAULT_MCAST_LOOP &&
      imo->i6mo_num_memberships == 0) {
    free(inp->inp_moptions6, M_IPMOPTS);
    inp->inp_moptions6 = NULL;
    inp->inp_flags &= ~INP_IPV6_MCAST;
  }

  return (error);
}

#define IFP_TO_INDEX(ifp, index) \
{\
   (index) = ifp->if_index; \
}

/*----------------------------------------------------------------------
 * Get IPv6 multicast options.
 ----------------------------------------------------------------------*/
/* ... now assumes all returned values are ints... */
int ipv6_getmoptions(int optname, struct ipv6_moptions *i6mo, int *mp)
{
  switch (optname)
    {
    case IPV6_MULTICAST_IF:
      if (!i6mo == NULL || !i6mo->i6mo_multicast_ifp)
	*mp = 0;
      else {
	IFP_TO_INDEX(i6mo->i6mo_multicast_ifp, *(unsigned int *)mp);
      }
      return (0);

    case IPV6_MULTICAST_HOPS:
      *mp = i6mo ? IPV6_DEFAULT_MCAST_HOPS : i6mo->i6mo_multicast_ttl;
      return (0);

    case IPV6_MULTICAST_LOOP:
      *mp = i6mo ? IPV6_DEFAULT_MCAST_LOOP : i6mo->i6mo_multicast_loop;
      return (0);

    default:
      return (EOPNOTSUPP);
    }
}

/*----------------------------------------------------------------------
 * Free IPv6 multicast options.
 ----------------------------------------------------------------------*/
void
ipv6_freemoptions(i6mo)
     register struct ipv6_moptions *i6mo;
{
  register int i;

  if (i6mo != NULL)
    {
      for (i = 0 ; i < i6mo->i6mo_num_memberships ; i++)
	in6_delmulti(i6mo->i6mo_membership[i]);
      free(i6mo, M_IPMOPTS);
    }
}

/*----------------------------------------------------------------------
 * Handler for IPV6 [gs]etsockopt() calls.  One problem arises when an
 * AF_INET6 socket actually wants to set IPv4 options.
 *
 * The decision to call this or to call ip_ctloutput() is best left in
 * the hands of TCP/UDP/etc., which have information about which IP is
 * in use.
 *
 ----------------------------------------------------------------------*/

#if __FreeBSD__
int ipv6_ctloutput(struct socket *so, struct sockopt *sopt)
{
  register struct inpcb *inp;
  int op;
  int level;
  int optname;
  int optval;
  int error = 0;

  DPRINTF(FINISHED, ("ipv6_ctloutput(so=%08x, sopt=%08x)\n", OSDEP_PCAST(so),
         OSDEP_PCAST(sopt)));

  inp = sotoinpcb(so);

  switch(sopt->sopt_dir) {
    case SOPT_GET:
      op = PRCO_GETOPT;
      break;
    case SOPT_SET:
      op = PRCO_SETOPT;
      break;
    default:
      return EINVAL;
  };

  level = sopt->sopt_level;
  optname = sopt->sopt_name;

  DS();
#else /* __FreeBSD__ */
int
ipv6_ctloutput (op, so, level, optname, mp)
     int op;
     struct socket *so;
     int level;
     int optname;
     struct mbuf **mp;
{
  register struct inpcb *inp = sotoinpcb(so);
  struct mbuf *m = *mp;
  int error = 0;

  DPRINTF(IDL_EVENT, ("ipv6_ctloutput(op=%x,so=%08lx,level=%x,optname=%x,mp)\n", op, (unsigned long)so, level, optname));
#endif /* __FreeBSD__ */

  if ((level != IPPROTO_IP) && (level != IPPROTO_IPV6) && (level != IPPROTO_ROUTING) && (level != IPPROTO_ICMPV6)) {
#if !__FreeBSD__
    if (op == PRCO_SETOPT && *mp)
      m_free(*mp);
#endif /* !__FreeBSD__ */
    return EINVAL;
  }

  DS();
  switch (op) {
    case PRCO_SETOPT:
      switch(optname) {
	case IPV6_UNICAST_HOPS:
	  DPRINTF(IDL_GROSS_EVENT, ("ipv6_ctloutput: Reached IPV6_UNICAST_HOPS\n"));
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int))
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int))
#endif /* __FreeBSD__ */
	    error = EINVAL;
	  else {
	    struct tcpcb *tp;
#if __FreeBSD__
	    int i;

	    if (error = sooptcopyin(sopt, &i, sizeof(int), sizeof(int)))
	      break;
#else /* __FreeBSD__ */
            int i = *mtod(m, int *);
#endif /* __FreeBSD__ */

            if (i == -1)
              i = ipv6_defhoplmt;

            if ((i < 0) || (i > 255)) {
              error = EINVAL;
              break;
            };

	    inp->inp_ipv6.ipv6_hoplimit = i;

	    /*
	     *  Minor optimization for TCP.  We change the hoplimit  
	     *  in the template here so we don't have to do the extra
	     *  load before the ipv6_output() call in tcp_output() for 
	     *  every single packet (as is the case for IPv4).
	     */
	    DS();
#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
	     if ((so->so_type == SOCK_STREAM) && (tp = intotcpcb(inp)) &&
		  tp->t_template)
#if __FreeBSD__
		(mtod(tp->t_template, struct ipv6 *))->ipv6_hoplimit = i;
#else /* __FreeBSD__ */
		((struct ipv6 *)(tp->t_template))->ipv6_hoplimit = i;
#endif /* __FreeBSD__ */
#else /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
	     if ((so->so_type == SOCK_STREAM) && (tp = intotcpcb(inp)))
		((struct ipv6 *)(tp->t_tcpiphdr))->ipv6_hoplimit = i;
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
	  }
	  break;
	case IPV6_MULTICAST_IF:
	case IPV6_MULTICAST_HOPS:
	case IPV6_MULTICAST_LOOP:
        case IPV6_DROP_MEMBERSHIP:
        case IPV6_ADD_MEMBERSHIP:
#if __FreeBSD__
	  {
	    void *val;

	    if (!(val = OSDEP_MALLOC(sopt->sopt_valsize))) {
	      error = ENOMEM;
	      break;
	    };

	    if (error = sooptcopyin(sopt, val, sopt->sopt_valsize,
				    sopt->sopt_valsize)) {
	      OSDEP_FREE(val);
	      break;
	    };

	    error = ipv6_setmoptions(optname, inp, val, sopt->sopt_valsize);
	    OSDEP_FREE(val);
	  };
#else /* __FreeBSD__ */
          error = ipv6_setmoptions(optname, inp, mtod(m, void *), m->m_len);
#endif /* __FreeBSD__ */
	  break;
	case IPV6_ADDRFORM:
	  {
	    int newpf;
	    int oldpf = sotopf(inp->inp_socket);
	    union inpaddru new_faddru;
	    union inpaddru new_laddru;
	    int new_flags;
	    struct protosw *new_proto;
            int s;

#if __FreeBSD__
	    if (sopt->sopt_valsize != sizeof(int)) {
              DPRINTF(IDL_ERROR, ("addrform: valsize = %d\n",
				  sopt->sopt_valsize));
#else /* __FreeBSD__ */
	    if (m->m_len != sizeof(int)) {
              DPRINTF(IDL_ERROR, ("addrform: m->m_len = %d\n", m->m_len));
#endif /* __FreeBSD__ */
	      error = EINVAL;
              break;
            };

#if __FreeBSD__
	    if (error = sooptcopyin(sopt, &newpf, sizeof(int), sizeof(int)))
	      break;
#else /* __FreeBSD__ */
            newpf = *(mtod(m, int *));
#endif /* __FreeBSD__ */
	    
	    DPRINTF(IDL_ERROR, ("newpf = %d, oldpf = %d", newpf, oldpf));
	    
	    if (((newpf != AF_INET) && (newpf != AF_INET6)) ||
		((oldpf != AF_INET) && (oldpf != AF_INET6)))
	      return EINVAL;
	    
            DP(ERROR, __LINE__, d);
	    
	    if (newpf == oldpf)
	      return 0;
	    
	    DP(ERROR, inp->inp_flags, 08x);

	    if (newpf == AF_INET6)
	      if (!(inp->inp_flags & INP_IPV6_UNDEC) &&
		  !(inp->inp_flags & INP_IPV6_MAPPED))
		return EINVAL;

            DP(ERROR, __LINE__, d);

	    if (!(new_proto = pffindproto(newpf,
	      so->so_proto->pr_protocol,
              so->so_proto->pr_type)))
	      return EINVAL;

            DP(ERROR, new_proto->pr_domain->dom_family, d);

	    new_flags = inp->inp_flags;
	    new_faddru = inp->inp_faddru;
	    new_laddru = inp->inp_laddru;

	    if (newpf == AF_INET) {
	      if (new_flags & INP_IPV6_UNDEC) {
		new_flags &= ~(INP_IPV6 | INP_IPV6_MAPPED | INP_IPV6_UNDEC);
		new_laddru.iau_a4u.inaddr.s_addr = INADDR_ANY;
		new_faddru.iau_a4u.inaddr.s_addr = INADDR_ANY;
	      } else {
		new_flags &= ~(INP_IPV6 | INP_IPV6_MAPPED);
	      }
	    } else {
	      new_faddru.iau_addr6.in6a_words[0] = 0;
	      new_faddru.iau_addr6.in6a_words[1] = 0;
	      new_laddru.iau_addr6.in6a_words[0] = 0;
	      new_laddru.iau_addr6.in6a_words[1] = 0;
	      
	      if (new_laddru.iau_a4u.inaddr.s_addr == INADDR_ANY) {
		new_flags |= (INP_IPV6 | INP_IPV6_MAPPED | INP_IPV6_UNDEC);
	        new_faddru.iau_addr6.in6a_words[2] = 0;
	        new_laddru.iau_addr6.in6a_words[2] = 0;
	      } else {
		new_flags |= (INP_IPV6 | INP_IPV6_MAPPED);
	        new_faddru.iau_addr6.in6a_words[2] = htonl(0xffff);
	        new_faddru.iau_addr6.in6a_words[2] = htonl(0xffff);
	      }
	    }

	    s = splnet();

	    inp->inp_flags = new_flags;
            inp->inp_faddru = new_faddru;
            inp->inp_laddru = new_laddru;
            so->so_proto = new_proto;

            splx(s);
	  }
	  break;
	case IPV6_PKTINFO:
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int)) {
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int)) {
#endif /* __FreeBSD__ */
	    error = EINVAL;
            break;
          };

#if __FreeBSD__
	  if (error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int)))
	    break;

	  if (optval)
#else /* __FreeBSD__ */
	  if (*(mtod(m, int *)))
#endif /* __FreeBSD__ */
	    inp->inp_flags |= INP_RXINFO;
	  else
	    inp->inp_flags &= ~INP_RXINFO;
	  break;
	case IPV6_HOPOPTS:
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int)) {
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int)) {
#endif /* __FreeBSD__ */
	    error = EINVAL;
            break;
          };

#if __FreeBSD__
	  if (error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int)))
	    break;

	  if (optval)
#else /* __FreeBSD__ */
	  if (*(mtod(m, int *)))
#endif /* __FreeBSD__ */
	    inp->inp_flags |= INP_RXHOPOPTS;
	  else
	    inp->inp_flags &= ~INP_RXHOPOPTS;
	  break;
	case IPV6_DSTOPTS:
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int)) {
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int)) {
#endif /* __FreeBSD__ */
	    error = EINVAL;
            break;
          };

#if __FreeBSD__
	  if (error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int)))
	    break;

	  if (optval)
#else /* __FreeBSD__ */
	  if (*(mtod(m, int *)))
#endif /* __FreeBSD__ */
	    inp->inp_flags |= INP_RXDSTOPTS;
	  else
	    inp->inp_flags &= ~INP_RXDSTOPTS;
	  break;
	case IPV6_RTHDR:
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int)) {
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int)) {
#endif /* __FreeBSD__ */
	    error = EINVAL;
            break;
          };

#if __FreeBSD__
	  if (error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int)))
	    break;

	  if (optval)
#else /* __FreeBSD__ */
	  if (*(mtod(m, int *)))
#endif /* __FreeBSD__ */
	    inp->inp_flags |= INP_RXSRCRT;
	  else
	    inp->inp_flags &= ~INP_RXSRCRT;
	  break;
        case IPV6_HOPLIMIT:
#if __FreeBSD__
	  if (sopt->sopt_valsize != sizeof(int)) {
#else /* __FreeBSD__ */
	  if (m->m_len != sizeof(int)) {
#endif /* __FreeBSD__ */
	    error = EINVAL;
            break;
          };

#if __FreeBSD__
	  if (error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int)))
	    break;

	  if (optval)
#else /* __FreeBSD__ */
	  if (*(mtod(m, int *)))
#endif /* __FreeBSD__ */
	    inp->inp_flags |= INP_HOPLIMIT;
	  else
	    inp->inp_flags &= ~INP_HOPLIMIT;
	  break;
	default:
	  error = ENOPROTOOPT;
	  break;
	}
#if !__FreeBSD__
      if (m)
	m_free(m);
#endif /* !__FreeBSD__ */
      break;
    case PRCO_GETOPT:
      switch(optname)
	{
	case IPV6_ADDRFORM:
	  {
	    int pf = sotopf(inp->inp_socket);
            DP(ERROR, pf, d);
	    if ((pf != PF_INET) && (pf != PF_INET6))
	      return EINVAL;
            DP(ERROR, __LINE__, d);
#if __FreeBSD__
	    error = sooptcopyout(sopt, &pf, sizeof(int));
#else /* __FreeBSD__ */
	    if (!(m = m_get(M_NOWAIT, MT_SOOPTS))) {
	      error = ENOBUFS;
	      break;
	    };
	    *mp = m;
	    m->m_len = sizeof(int);
	    *mtod(m, int *) = pf;
#endif /* __FreeBSD__ */
	  }
	  break;
	case IPV6_UNICAST_HOPS:
	  DPRINTF(IDL_GROSS_EVENT,("ipv6_ctloutput(): Reached IP_UNICAST_HOPS\n"));
#if __FreeBSD__
	  error = sooptcopyout(sopt, &inp->inp_ipv6.ipv6_hoplimit,
			       sizeof(int));
#else /* __FreeBSD__ */
	  if (!(m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  *mp = m;
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = inp->inp_ipv6.ipv6_hoplimit;
#endif /* __FreeBSD__ */
	  break;
	case IPV6_MULTICAST_IF:
	case IPV6_MULTICAST_HOPS:
	case IPV6_MULTICAST_LOOP:
	case IPV6_DROP_MEMBERSHIP:
	case IPV6_ADD_MEMBERSHIP:
#if __FreeBSD__
  	  error = ipv6_getmoptions(optname, inp->inp_moptions6, &optval);
	  if (error)
	    break;

	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(*mp = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };

  	  error = ipv6_getmoptions(optname, inp->inp_moptions6,
				   mtod(*mp, int *));
#endif /* __FreeBSD__ */
	  break;
	case IPV6_PKTINFO:
#if __FreeBSD__
	  optval = (inp->inp_flags & INP_RXINFO) ? 1 : 0;
	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  *mp = m;
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = (inp->inp_flags & INP_RXINFO) ? 1 : 0;
#endif /* __FreeBSD__ */
	  break;
	case IPV6_HOPOPTS:
#if __FreeBSD__
	  optval = (inp->inp_flags & INP_RXHOPOPTS) ? 1 : 0;
	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(*mp = m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = (inp->inp_flags & INP_RXHOPOPTS) ? 1 : 0;
#endif /* __FreeBSD__ */
	  break;
	case IPV6_DSTOPTS:
#if __FreeBSD__
	  optval = (inp->inp_flags & INP_RXDSTOPTS) ? 1 : 0;
	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(*mp = m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = (inp->inp_flags & INP_RXDSTOPTS) ? 1 : 0;
#endif /* __FreeBSD__ */
	  break;
	case IPV6_RTHDR:
#if __FreeBSD__
	  optval = (inp->inp_flags & INP_RXSRCRT) ? 1 : 0;
	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(*mp = m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = (inp->inp_flags & INP_RXSRCRT) ? 1 : 0;
#endif /* __FreeBSD__ */
	  break;
	case IPV6_HOPLIMIT:
#if __FreeBSD__
	  optval = (inp->inp_flags & INP_HOPLIMIT) ? 1 : 0;
	  error = sooptcopyout(sopt, &optval, sizeof(int));
#else /* __FreeBSD__ */
	  if (!(*mp = m = m_get(M_NOWAIT, MT_SOOPTS))) {
	    error = ENOBUFS;
	    break;
	  };
	  m->m_len = sizeof(int);
	  *mtod(m, int *) = (inp->inp_flags & INP_HOPLIMIT) ? 1 : 0;
#endif /* __FreeBSD__ */
	  break;
	default:
	  error = ENOPROTOOPT;
	  break;
	}
      break;
    default:
      error = ENOPROTOOPT;
      break;
    }

  return error;
}

/*----------------------------------------------------------------------
 * Loops back multicast packets to groups of which I'm a member.
 ----------------------------------------------------------------------*/

void
ipv6_mloopback(ifp, m, dst)
     struct ifnet *ifp;
     register struct mbuf *m;
     register struct sockaddr_in6 *dst;
{
  struct mbuf *copym;
  register struct ipv6 *header;

#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
  if (!loifp)
    return;
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */

  /*
   * Copy mbuf chain in m, and send to loopback interface.
   */

  copym=m_copym(m,0,M_COPYALL,M_DONTWAIT);
  if (copym != NULL)
    {
      header=mtod(copym,struct ipv6 *);
      /* Jumbogram? */
      header->ipv6_length = htons(header->ipv6_length);
      /*
       * Also, there's an issue about address collision.  You may want to
       * check the ipv6 destination (or the dst address) and set the ifp
       * argument to looutput to be the loopback interface itself iff
       * it is to a solicited nodes multicast.
       *
       * Then again, it may be easier for the soliciting code to burn another
       * m_flags bit, and look for it on loopback.
       */
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
      (*loifp->if_output)(ifp, copym, (struct sockaddr *)dst, NULL);
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
#if __FreeBSD__
      loif->if_output(ifp,copym,(struct sockaddr *)dst,NULL);
#else /* __FreeBSD__ */
      looutput(ifp,copym,(struct sockaddr *)dst,NULL);
#endif /* __FreeBSD__ */
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    }
  else 
    DPRINTF(IDL_GROSS_EVENT,("m_copym() failed in ipv6_mloopback.\n"));
}

/*----------------------------------------------------------------------
 * Fragment IPv6 datagram.
 *
 * Given a large mbuf chain m, with only its ipv6->ipv6_length field in
 * host order, fragment into mtu sized chunks, and return a meta-chain
 * with m->m_nextpkt being the subsequent fragments.  If there's a problem,
 * m_freem all fragments, and return NULL.  Also, all ipv6->ipv6_length
 * fields are in network order, i.e. ready-to-transmit.
 *
 * Note that there is an unusually large amount of calls to m_pullup,
 * m_copym2etc. here.  This will cause performance hits.
 *
 * A redesign of this is in order, but that will have to wait.
 ----------------------------------------------------------------------*/

struct mbuf *
ipv6_fragment(m,mtu)
     struct mbuf *m;
     int mtu;
{
  uint8_t nextopt = IPPROTO_FRAGMENT;
  uint divpoint = sizeof(struct ipv6), chunksize, sofar = 0, goal;
  struct mbuf *retval = NULL, *newfrag = NULL;
  struct ipv6_fraghdr *frag = NULL;

  outfragid++;

  DPRINTF(IDL_FINISHED,\
	  ("Entering ipv6_fragment, m_pkthdr.len = %d, mtu = %d\n",\
	   m->m_pkthdr.len, mtu));

  /*
   * Find the dividing point between pre-fragment and post-fragment options.
   */
  divpoint = ipv6_finddivpoint(m, &nextopt);

  /*
   * Options being larger than MTU can happen, especially given large routing
   * headers and large options bags.
   */
  if (divpoint + sizeof(struct ipv6_fraghdr) >= mtu)
    {
      DPRINTF(IDL_ERROR, 
	      ("ipv6_fragment(): Options are larger than passed-in MTU.\n"));
      m_freem(m);
      return NULL;
    }

#ifdef DIAGNOSTIC
  if (divpoint & 0x7)
    panic("divpoint not a multiple of 8!");
#endif

  /*
   * sofar keeps track of how much I've fragmented, chunksize is how
   * much per fragment, goal is how much data to actually fragment.
   */
  chunksize = mtu - divpoint - sizeof(struct ipv6_fraghdr);
  chunksize &= ~7;
  goal = m->m_pkthdr.len - divpoint;

  DPRINTF(IDL_FINISHED, \
	  ("Found divpoint (%d), nextopt (%d), chunksize(%d) goal(%d)\n",\
	   divpoint, nextopt, chunksize,goal));

  while (sofar < goal)
    if ((newfrag = m_copym2(m, 0, divpoint, M_DONTWAIT)) != NULL)
      {
	struct mbuf *fraghdrmbuf = NULL;
	struct ipv6 *ipv6 = NULL;
	uint tocopy = (chunksize <= (goal - sofar))?chunksize:(goal - sofar);

	DPRINTF(IDL_FINISHED,("tocopy == %d\n",tocopy));

	/*
	 * Create a new IPv6 fragment, using the header that was slightly
	 * munged by ipv6_finddivpoint().
	 *
	 * The above m_copym2() creates a copy of the first
	 */

	newfrag->m_nextpkt = retval;
	retval = newfrag;

	/*
	 * Append IPv6 fragment header to pre-fragment
	 */
	for(fraghdrmbuf = retval;fraghdrmbuf->m_next != NULL;)
	  fraghdrmbuf = fraghdrmbuf->m_next;
	MGET(fraghdrmbuf->m_next,M_DONTWAIT,MT_DATA);
	if (fraghdrmbuf->m_next == NULL)
	  {
	    DPRINTF(IDL_ERROR,("couldn't get new mbuf for frag hdr\n"));
	    ipv6stat.ips_odropped++;
	    goto bail;  
	  }

	fraghdrmbuf = fraghdrmbuf->m_next;
	fraghdrmbuf->m_len = sizeof(struct ipv6_fraghdr);
	retval->m_pkthdr.len += sizeof(struct ipv6_fraghdr);
	frag = mtod(fraghdrmbuf,struct ipv6_fraghdr *);
	frag->frag_nexthdr = nextopt;
	frag->frag_reserved = 0;
	frag->frag_bitsoffset = htons(sofar | ((sofar + tocopy) < goal));
	frag->frag_id = outfragid;

	/*
	 * Copy off (rather than just m_split()) the portion of data that
	 * goes with this fragment.
	 */
	if ((fraghdrmbuf->m_next = m_copym2(m,divpoint + sofar,tocopy,
					    M_DONTWAIT)) == NULL)
	  {
	    DPRINTF(IDL_ERROR,("couldn't copy segment.\n"));
	    goto bail;  
	  }
	retval->m_pkthdr.len += tocopy;

	/*
	 * Update fragment's IPv6 header appropriately.
	 */
	ipv6 = mtod(retval,struct ipv6 *);
	ipv6->ipv6_length = htons(retval->m_pkthdr.len - sizeof(struct ipv6));
	ipv6->ipv6_nexthdr = IPPROTO_FRAGMENT;
	ipv6stat.ips_ofragments++;
	sofar += tocopy;
      }
    else
      {
	/*
	 * Creation of new fragment failed.
	 */
	DPRINTF(IDL_ERROR,("m_copym2() failed in fragmentation loop.\n"));
	ipv6stat.ips_odropped++;
      bail:   
	DPRINTF(IDL_ERROR,("Bailing out of ipv6_fragment()\n"));
	while (retval != NULL)
	  {
	    newfrag = retval;
	    retval = retval->m_nextpkt;
	    m_freem(newfrag);
	  }
	m_freem(m);
	return NULL;
      }

  m_freem(m);

  /* Dump mbuf chain list constructed for debugging purposes. */
  DDO(IDL_FINISHED,\
      for (newfrag = retval; newfrag != NULL; newfrag = newfrag->m_nextpkt)\
        dump_mbuf(newfrag) );

  return retval;
}

/*----------------------------------------------------------------------
 * Find the dividing point between pre-fragment and post-fragment options.
 * The argument nexthdr is read/write, on input, it is the next header
 * value that should be written into the previous header's "next hdr" field,
 * and what is written back is what used to be in the previous field's
 * "next hdr" field.  For example:
 *
 * IP (next hdr = routing)       becomes -->     IP (next hdr = routing)
 * Routing (next hdr = TCP)                      Routing (next hdr = fragment)
 * TCP                                           TCP
 * argument nexthdr = fragment                   argument nexthdr = TCP
 *
 * This function returns the length of the pre-fragment options, ideal for
 * calls to m_split.
 *
 * As in ipv6_fragment, too many calls to m_pullup/m_pullup2 are performed
 * here.  Another redesign is called for, but not now.
 ----------------------------------------------------------------------*/

int
ipv6_finddivpoint(m, nexthdr)
     struct mbuf *m;
     uint8_t *nexthdr;
{
  uint8_t iprevopt, *prevopt = &(iprevopt), new = *nexthdr;
  uint8_t *nextopt;
  uint divpoint,maybe = 0;

  /*
   * IPv4 authentication code calls this function too.  It is likely that
   * v4 will just return almost immedately, after determining options
   * length. (i.e. never go through the while loop.)
   */
  if (mtod(m, struct ip *)->ip_v == 4)
    {
      iprevopt = IPPROTO_IPV4;
      nextopt = &(mtod(m, struct ip *)->ip_p);
      divpoint = sizeof(struct ip);
    }
  else
    {
      iprevopt = IPPROTO_IPV6;
      nextopt = &(mtod(m, struct ipv6 *)->ipv6_nexthdr);
      divpoint = sizeof(struct ipv6);
    }

  /*
   * Scan through options finding dividing point.  Dividing point
   * for authentication and fragmentation is the same place.
   *
   * Some weirdness here is that there MIGHT be a "Destination options bag"
   * which is actually a "per source-route-hop" bag.  There is a strong
   * argument for giving this particular options bag a separate type, but
   * for now, kludge around it.
   *
   * The "maybe" variable takes into account the length of this options bag.
   */
  while (IS_PREFRAG(*nextopt) && *prevopt != IPPROTO_ROUTING)
    {
      struct ipv6_srcroute0 *i6sr;
      struct ipv6_opthdr *oh;
      
      /*
       * ASSUMES: both nextopt and length will be in the first
       *          8 bytes of ANY pre-fragment header.
       */

      if ((divpoint + maybe + 8) > MHLEN)
	{
	  /* 
	   * This becomes complicated.  Try and collect invariant part into
	   * first (now cluster) mbuf on chain.  m_pullup() doesn't work with
	   * clusters, so either write m_pullup2() or inline it here.
	   *
	   * m_pullup2(), unlike m_pullup() will only collect exactly
	   * how many bytes the user requested.  This is to avoid problems
	   * with m_copym() and altering data that is merely referenced
	   * multiple times, rather than actually copied.  (We may eliminate
	   * the Net/2 hack of adding m_copym2().)
	   */
	  if ((m = m_pullup2(m,divpoint + maybe + 8)) == NULL)
	    {
	      DPRINTF(IDL_ERROR,\
		      ("m_pullup2(%d) failed in ipv6_fragment().\n",\
		       divpoint + maybe + 8));
	      return 0;
	    }
	}
      else
	{
	  if ((m = m_pullup(m,divpoint + maybe + 8)) == NULL)
	    {
	      DPRINTF(IDL_ERROR,\
		      ("m_pullup() failed in ipv6_fragment().\n"));
	      return 0;
	    }
	}

      /*
       * Find nextopt, and advance accordingly.
       */
      switch (*nextopt)
	{
	case IPPROTO_HOPOPTS:
	  /*
	   * Hop-by-hops should be right after IPv6 hdr.  If extra is nonzero,
	   * then there was a destination options bag.  If divpoint is not
	   * only the size of the IPv6 header, then something came before
	   * hop-by-hop options.  This is not good.
	   */
	  if (maybe || divpoint != sizeof(struct ipv6))
	    {
	      DPRINTF(IDL_ERROR, 
		      ("ipv6_input():  Weird ordering in headers.\n"));
	      m_freem(m);
	      return 0;
	    }
	  oh = (struct ipv6_opthdr *)(m->m_data + divpoint);
	  prevopt = nextopt;
	  nextopt = &(oh->oh_nexthdr);
	  divpoint += 8 + (oh->oh_extlen << 3);
	  if (oh->oh_extlen)
	    if (divpoint > MHLEN)
	      {
		if ((m = m_pullup2(m,divpoint)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup2(%d) failed in IPPROTO_HOPOPTS nextopt.\n",\
			     divpoint));
		    return 0;
		  }
	      }
	    else
	      {
		if ((m = m_pullup(m,divpoint)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup() failed in IPPROTO_HOPOPTS nextopt.\n"));
		    return 0;
		  }
	      }
	  break;
	case IPPROTO_DSTOPTS:
	  oh = (struct ipv6_opthdr *)(m->m_data + divpoint);	
	  prevopt = nextopt;
	  nextopt = &(oh->oh_nexthdr);
	  maybe = 8 + (oh->oh_extlen << 3);
	  if (oh->oh_extlen)
	    if ( divpoint + maybe > MHLEN)
	      {
		if ((m = m_pullup2(m,divpoint + maybe)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup2(%d) failed in IPPROTO_DSTOPTS nextopt.\n",\
			     divpoint+maybe));
		    return 0;
		  }
	      }
	    else
	      {
		if ((m = m_pullup(m,divpoint + maybe)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup() failed in IPPROTO_DSTOPTS nextopt.\n"));
		    return 0;
		  }
	      }
	  break;
	case IPPROTO_ROUTING:
	  if (maybe)  /* i.e. if we had a destination options bag before
			 this routing header, we should advance the dividing
			 point. */
	    divpoint += maybe;
	  maybe = 0;
	  i6sr = (struct ipv6_srcroute0 *)(m->m_data + divpoint);
	  prevopt = nextopt;
	  nextopt = &(i6sr->i6sr_nexthdr);
	  switch (i6sr->i6sr_type)
	    {
	    case 0:
	      divpoint += 8 + (i6sr->i6sr_len * 8);
	      break;
	    default:
	      DPRINTF(IDL_ERROR, 
		      ("ipv6_input():  Unknown outbound routing header.\n"));
	      break;
	    }
	  if (divpoint > MHLEN)
	      {
		if ((m = m_pullup2(m,divpoint)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup2(%d) failed in IPPROTO_ROUTING nextopt.\n",\
			     divpoint));
		    return 0;
		  }
	      }
	    else
	      {
		if ((m = m_pullup(m,divpoint)) == NULL)
		  {
		    DPRINTF(IDL_EVENT,\
			    ("m_pullup() failed in IPPROTO_ROUTING nextopt.\n"));
		    return 0;
		  }
	      }
	  break;
	}  /* End of switch statement. */
    };  /* End of while loop. */
    *nexthdr = *nextopt;
    *nextopt = new;
    return divpoint;
}

int ipv6_controltoheader(struct mbuf **m, struct mbuf *control, struct ifnet **forceifp, int *payload)
{
  struct cmsghdr *cmsghdr;
  int error = EINVAL;
  struct mbuf *srcrtm = NULL;

  DPRINTF(IDL_EVENT, ("ipv6_controltoheader(m=%08lx, control=%08lx, forceif=%08lx, payload=%08lx)\n", (unsigned long)m, (unsigned long)control, (unsigned long)forceifp, (unsigned long)payload));
  DDO(IDL_EVENT, dump_mchain(control));

  while((control = m_pullup2(control, sizeof(struct cmsghdr))) &&
	(cmsghdr = mtod(control, struct cmsghdr *)) &&
	(control = m_pullup2(control, cmsghdr->cmsg_len))) {
    cmsghdr = mtod(control, struct cmsghdr *);
    switch(cmsghdr->cmsg_level) {
      case IPPROTO_IPV6:
	switch(cmsghdr->cmsg_type) {
	  case IPV6_PKTINFO:
	    {
	      struct in6_pktinfo in6_pktinfo;
	      struct in6_ifnet *i6ifp;
	      struct ifaddr *ifa;

	      if (cmsghdr->cmsg_len != sizeof(struct cmsghdr) + sizeof(struct in6_pktinfo))
		goto ret;

	      bcopy((caddr_t)cmsghdr + sizeof(struct cmsghdr), &in6_pktinfo, sizeof(struct in6_pktinfo));

              if (!in6_pktinfo.ipi6_ifindex) {
                if (IN6_IS_ADDR_UNSPECIFIED(&in6_pktinfo.ipi6_addr)) {
                  DPRINTF(IDL_EVENT, ("ipv6_controltoheader: in degenerate IPV6_PKTINFO case\n"));
                  break;
                } else {
                  struct in6_ifaddr *i6a;
                  DPRINTF(IDL_EVENT, ("ipv6_controltoheader: in index = unspec, addr = spec case\n"));
                  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
                    if (IN6_ARE_ADDR_EQUAL(&I6A_SIN(i6a)->sin6_addr, &in6_pktinfo.ipi6_addr))
                      goto l2;
                  goto ret;
                };
              };

              DPRINTF(FINISHED, ("ipv6_controltoheader: in index = spec case\n"));

	      for (i6ifp = in6_ifnet; i6ifp && (i6ifp->i6ifp_ifp->if_index != in6_pktinfo.ipi6_ifindex); i6ifp = i6ifp->i6ifp_next);

              if (!i6ifp)
                goto ret;

              if (IN6_IS_ADDR_UNSPECIFIED(&in6_pktinfo.ipi6_addr)) {
                DPRINTF(FINISHED, ("ipv6_controltoheader: in index = spec, addr = unspec case\n"));
                goto l1;
              };

              DPRINTF(FINISHED, ("ipv6_controltoheader: in index = spec, addr = spec case\n"));

#ifdef __FreeBSD__
	      for (ifa = i6ifp->i6ifp_ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next) {
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	      for (ifa = i6ifp->i6ifp_ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next) {
#else /* __NetBSD__ || __OpenBSD__ */
	      for (ifa = i6ifp->i6ifp_ifp->if_addrlist; ifa; ifa = ifa->ifa_next) {
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
		if ((ifa->ifa_addr->sa_family == AF_INET6) && !bcmp(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr, &in6_pktinfo.ipi6_addr, sizeof(struct in6_addr)))
		  goto l1;
              };
	      goto ret;

l1:           *forceifp = i6ifp->i6ifp_ifp;
l2:	      bcopy(&in6_pktinfo.ipi6_addr, &(mtod(*m, struct ipv6 *)->ipv6_src), sizeof(struct in6_addr));
	    };
	    break;
          case IPV6_HOPLIMIT:
            {
            int i;

	    if (cmsghdr->cmsg_len != sizeof(struct cmsghdr) + sizeof(int))
              goto ret;

            i = *((int *)((caddr_t)cmsghdr + sizeof(struct cmsghdr)));

            if (i == -1)
              if (IN6_IS_ADDR_MULTICAST(&mtod(*m, struct ipv6 *)->ipv6_dst))
                i = IPV6_DEFAULT_MCAST_HOPS;
              else
                i = ipv6_defhoplmt;

            if ((i < 0) || (i > 255))
              goto ret;

            mtod(*m, struct ipv6 *)->ipv6_hoplimit = i;
            };
            break;
	  default:
	    goto ret;
	};
	break;
      case IPPROTO_ROUTING:
	MGET(srcrtm, M_DONTWAIT, MT_DATA);
	if (!srcrtm) {
	  error = ENOBUFS;
	  goto ret;
	};

	srcrtm->m_len = cmsghdr->cmsg_len - sizeof(struct cmsghdr) + 3;
	if (srcrtm->m_len > MLEN) {
	  DPRINTF(IDL_ERROR, ("ipv6_controltoheader: requested source route that we can't fit in an mbuf (length %d)\n", srcrtm->m_len));
	  goto ret;
	};

	if (srcrtm->m_len & 7) {
	  DPRINTF(IDL_ERROR, ("ipv6_controltoheader: requested source route has an invalid length; %d needs to be a multiple of eight bytes\n", srcrtm->m_len));
          goto ret;
        };

	*(mtod(srcrtm, uint8_t *) + 1) = (srcrtm->m_len >> 3) - 1;
        *(mtod(srcrtm, uint8_t *) + 2) = cmsghdr->cmsg_type;
	bcopy((caddr_t *)cmsghdr + sizeof(struct cmsghdr), mtod(srcrtm, uint8_t *) + 3, cmsghdr->cmsg_len - sizeof(struct cmsghdr));
        break;
      case IPPROTO_HOPOPTS:
      case IPPROTO_DSTOPTS:
	/* XXX */
	goto ret;
      default:
	goto ret;
    };
    m_adj(control, cmsghdr->cmsg_len);
    if (!control->m_len)
      goto finish;
  };

  DPRINTF(IDL_ERROR, ("ipv6_controltoheader: pullups failed\n"));
  goto ret;

finish:
  if (srcrtm) {
    struct mbuf *m2;
    DPRINTF(IDL_EVENT, ("ipv6_controltoheader: in srcrtm case\n"));
    if (!(m2 = m_split(*m, sizeof(struct ipv6), M_DONTWAIT)))
      goto ret;
    (*m)->m_next = srcrtm;
    srcrtm->m_next = m2;
    *mtod(srcrtm, uint8_t *) = mtod(*m, struct ipv6 *)->ipv6_nexthdr;
    mtod(*m, struct ipv6 *)->ipv6_nexthdr = IPPROTO_ROUTING;

    *payload += srcrtm->m_len;
    (*m)->m_pkthdr.len += srcrtm->m_len;
  };
  m_freem(control);
  DPRINTF(IDL_FINISHED, ("ipv6_controltoheader: returning\n"));
  return 0;

ret:
  DPRINTF(IDL_ERROR, ("ipv6_controltoheader: returning error %d\n", error));
  if (srcrtm)
    m_free(srcrtm);
  if (control)
    m_freem(control);
  return error;
};
