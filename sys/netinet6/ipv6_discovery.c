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
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>

#if __OpenBSD__
#ifdef IPSEC   
#undef IPSEC
#endif /* IPSEC */
#ifdef NRL_IPSEC
#define IPSEC 1
#endif /* NRL_IPSEC */
#endif /* __OpenBSD__ */

#ifdef IPSEC
/* #include <netsec/ipsec.h> */
#include <net/netproc.h>
#include <net/netproc_var.h>
#endif /* IPSEC */

#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
#include <machine/pcpu.h>
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */

#ifdef DEBUG_NRL
#include <sys/debug.h>
#else /* DEBUG_NRL */
#if __OpenBSD__
#include <netinet6/debug.h>
#else /* __OpenBSD__ */
#include <sys/debug.h>
#endif /* __OpenBSD__ */
#endif /* DEBUG_NRL */

#ifdef IPSEC
extern struct netproc_security fixedencrypt;
extern struct netproc_auth fixedauth;
#endif /* IPSEC */

/*
 * Globals (and forward function declarations).
 */

struct discq dqhead;
struct v6router defrtr,nondefrtr;
struct sockaddr_in6 in6_allones;

void ipv6_nsolicit __P((struct ifnet *, struct mbuf *, struct rtentry *));
void ipv6_uni_nsolicit __P((struct rtentry *));

/*
 * These should be sysctl-tweakable.  See ipv6_var.h for units. 
 * They share the common prefix v6d_.
 */

u_long v6d_maxinitrai	= MAX_INITIAL_RTR_ADVERT_INTERVAL;
u_long v6d_maxinitra	= MAX_INITIAL_RTR_ADVERTISEMENTS;
u_long v6d_maxrtrdel	= MAX_RTR_RESPONSE_DELAY;

u_long v6d_maxrtsoldel	= MAX_RTR_SOLICITATION_DELAY;
u_long v6d_rtsolint	= RTR_SOLICITATION_INTERVAL;    
u_long v6d_maxrtsol	= MAX_RTR_SOLICITATIONS;     

u_long v6d_maxmcastsol	= MAX_MULTICAST_SOLICIT;
u_long v6d_maxucastsol	= MAX_UNICAST_SOLICIT;
u_long v6d_maxacastdel  = MAX_ANYCAST_DELAY_TIME;
u_long v6d_maxnadv	= MAX_NEIGHBOR_ADVERTISEMENTS;
u_long v6d_minnadvint	= MIN_NEIGHBOR_ADVERT_INTERVAL;
u_long v6d_reachtime	= REACHABLE_TIME;
u_long v6d_retranstime	= RETRANS_TIMER;
u_long v6d_delfirstprobe= DELAY_FIRST_PROBE_TIME;  
/* Need to somehow define random factors. */

u_long v6d_nexthopclean	= NEXTHOP_CLEAN_INTERVAL;
u_long v6d_toolong = NEXTHOP_CLEAN_INTERVAL >> 1;  /* Half the cleaning
						      interval. */

u_long v6d_down = REJECT_TIMER;  /* Dead node routes are marked REJECT
				    for this amt. */

/*
 * External globals.
 */

extern struct in6_ifaddr *in6_ifaddr;
extern struct in6_ifnet *in6_ifnet;

extern int ipv6forwarding;
extern int ipv6rsolicit;
extern int ipv6_defhoplmt;

#if __NetBSD__ || __OpenBSD__
#define if_name if_xname
#endif /* __NetBSD__ || __OpenBSD__ */

/*
 * General notes:
 *
 * Currently this module does not support encryption/authentication 
 * of ND messages.  That support is probably needed in some environments.
 * NRL intends to add it in a later release.
 *
 * Please keep this in mind.
 *    danmcd rja
 */
int ipv6_enabled __P((struct ifnet *));

void ipv6_neighbor_timer __P((void *));
int ipv6_clean_nexthop __P((struct radix_node *, void *));
void ipv6_nexthop_timer __P((void *));
void ipv6_discovery_init __P((void));
struct mbuf *get_discov_cluster __P((void));
void send_nsolicit __P((struct rtentry *, struct ifnet *, struct in6_addr *, int));
struct mbuf *ipv6_discov_pullup __P((struct mbuf *, int));
struct v6router *ipv6_add_defrouter_advert __P((struct in6_addr *, int lifetime, struct ifnet *));
struct v6router *ipv6_add_defrouter_rtrequest __P((struct rtentry *));
void ipv6_nadvert __P((struct in6_ifaddr *, struct ifnet *, struct rtentry *, uint32_t));
static int update_defrouter __P((struct v6router *, struct ipv6_icmp *));
int ipv6_delete_defrouter __P((struct v6router *));
static void prefix_concat __P((struct in6_addr *, struct in6_addr *, struct in6_addr *));
struct rtentry *ipv6_new_neighbor __P((struct sockaddr_in6 *, struct ifnet *));
int ipv6_discov_resolve __P((struct ifnet *, struct rtentry *, struct mbuf *, struct sockaddr *, u_char *));
void tunnel_parent __P((struct rtentry *));
void tunnel_parent_clean __P((struct rtentry *));
void tunnel_child __P((struct rtentry *));
void tunnel_child_clean __P((struct rtentry *));

/*
 * Functions and macros.
 */

/*----------------------------------------------------------------------
  void padto(u_char *ptr, u_long mask, int alignto)

  Examples: padto(foo, 0x3, 0) will get 4-byte alignment
             padto(foo, 0x7, 6) will get to 2 bytes before an 8-byte
                                boundary.

  Padding generated will be most efficient.

  This is now deprecated for discovery, but it still has
  uses in generating IPv6 options (the option bags).

----------------------------------------------------------------------*/
#define padto(ptr,mask,alignto)\
{\
  int difference = 0;\
  \
  if (((u_long)(ptr) & (mask)) != (alignto))\
    {\
      difference = (((u_long)(ptr) & ~(mask)) + (alignto)) - (long)(ptr);\
      if (difference < 0)\
	difference += ((mask) +1);\
      if (difference == 1)\
        (ptr)[0] = EXT_PAD1;\
      else\
	{\
	  (ptr)[0] = EXT_PADN;\
	  (ptr)[1] = (difference) - 2; /* difference >= 2 always here. */\
	  bzero((ptr)+2,(ptr)[1]);\
	}\
      (ptr) += difference;\
    }\
}


/*---------------------------------------------------------------------- 
 * ipv6_neighbor_timer():
 *	Scan neighbor list and resend (1) mcast ND solicits for all 
 *      neighbors in INCOMPLETE state and (2) unicast ND solicits 
 *      for all neighbors in PROBE state
 ----------------------------------------------------------------------*/
void
ipv6_neighbor_timer(whocares)
     void *whocares;
{
  struct discq *dq;
  int s = splnet();
  struct rtentry *rt;

  /*
   * 1. Scan all neighbors (go down dq list) and retransmit those which
   *    are still in INCOMPLETE.
   * 2. Also, those in PROBE state should have unicast solicits sent.
   * 3. ...
   */

  dq = dqhead.dq_next;
  while (dq != &dqhead)
    {
      struct discq *current = dq;

      dq = dq->dq_next;  /* Before any rtrequest */

      rt = current->dq_rt;
#ifdef __FreeBSD__
      if (rt->rt_rmx.rmx_expire <= time_second)
#else /* __FreeBSD__ */
      if (rt->rt_rmx.rmx_expire <= time.tv_sec)
#endif /* __FreeBSD__ */
	{
	  struct sockaddr_dl *sdl = (struct sockaddr_dl *)rt->rt_gateway;

	  /*
	   * Gotta do SOMETHING...
	   */
	  if (sdl->sdl_alen == 0) {
	    /* If RTF_REJECT, then delete... */
	    if (rt->rt_flags & RTF_REJECT)
	      {
		DPRINTF(GROSSEVENT,
			("neighbor_timer() deleting rejected route\n"));
		rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);
		continue;
	      }

	    /* Am in INCOMPLETE mode... */
	    
	    if (current->dq_unanswered >= 0) { /* If not newly created... */
	      if (current->dq_unanswered >= v6d_maxmcastsol)
		{
		  /* Dead node.  For now, just delete.  
		     May want to do an RTF_REJECT entry.  
		     May also want to send ICMP errors on enqueued packet(s). 
		     May also want to do routing cleanup if a defult router.
		     XXX */
		  DPRINTF(GROSSEVENT,("Deleting dead node... "));
		  rt->rt_flags |= RTF_REJECT;
		  /*
		   * If there's no ifa, add an aribitrary one.
		   * (May be unnecessary if rtfree() ifa != NULL check
		   * is in place.)
		   *
		   *  It turns out that adding a dummy ifa causes
		   *  problems for subsequent packets that successfully
		   *  rtalloc-ed this route (since rtalloc1 doesn't
		   *  check if a route is marked RTF_REJECT... the
		   *  check is done at a further down the stack, e.g.,
		   *  ether_output().  This causes ipv6_output to
		   *  call ipv6_nsolicit (with an incomplete route that 
		   *  has an ifa but no ifp).  ipv6_nsolicit then uses
		   *  the address of the dummy ifa as the source address
		   *  to be used in send_nsolicit which may panic 
		   *  if the address turns out to not have a link-local
		   *  address (e.g. loopback).
		   *
		   *  The solution, for now, is to check for RTF_REJECT
		   *  in ipv6_nsolicit.  This may change in the future.
		   */
		  if (rt->rt_ifa == NULL)
		    {
		      rt->rt_ifa = (struct ifaddr *)in6_ifaddr;
		      rt->rt_ifa->ifa_refcnt++;
		    }
#ifdef __FreeBSD__ 
		  rt->rt_rmx.rmx_expire = time_second + v6d_down;
#else /* __FreeBSD__ */
		  rt->rt_rmx.rmx_expire = time.tv_sec + v6d_down;
#endif /* __FreeBSD__ */

		  /* ICMPv6 error part. */
		  while (current->dq_queue != NULL)
		    {
		      struct mbuf *badpack = current->dq_queue;

		      current->dq_queue = badpack->m_nextpkt;
		      /* Set rcvif for icmp_error, is this good? */
		      badpack->m_pkthdr.rcvif = current->dq_rt->rt_ifp;
		      ipv6_icmp_error(badpack,ICMPV6_UNREACH,
				      ICMPV6_UNREACH_ADDRESS,0);
		    }
		}
	      else
		{
		  /* Send another solicit.  The ipv6_nsolicit call will
		     ensure the rate-limitation.  Send the queued packet
		     to ipv6_nsolicit for source address determination. */
		  ipv6_nsolicit(rt->rt_ifp,NULL,rt);
		}
	    }
	  }
	  else
	    /*
	     * Am in either PROBE mode or REACHABLE mode.  
             *
	     * Either way, that will (in the case of REACHABLE->PROBE)
	     * or might (in the case of PROBE->INCOMPLETE) change.
	     */
	    if (current->dq_unanswered >= 0) { /* PROBE */
	      if (current->dq_unanswered >= v6d_maxucastsol) {
		/* PROBE -> INCOMPLETE */
		if (rt->rt_refcnt > 0) {
		  /* The code below is an optimization for the case
		   * when someone is using this route : 
		   * - defer actual deletion until mcast solicits fail.
		   *
		   * Q: Do we still want to do this?
		   */
		  sdl->sdl_alen = 0;
		  current->dq_unanswered = 0;
#ifdef __FreeBSD__ 
		  rt->rt_rmx.rmx_expire = time_second + v6d_retranstime;
#else /* __FreeBSD__ */
		  rt->rt_rmx.rmx_expire = time.tv_sec + v6d_retranstime;
#endif /* __FreeBSD__ */
		} else {
		  /*
		   * Unicast probes failed and no one is using this route.  
		   * Delete and let address resolution take its course.
		   *
		   * At this point, I have an ifa, so I don't need to add
		   * one.
		   */
		  rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);
		}
	      } else {
		/* Retry PROBE */
		ipv6_uni_nsolicit(rt);
	      }
	    } else {
	      /*
	       * Do nothing if REACHABLE expires.  Only on output,
	       * BUT... some of these might hang around for too long.
	       * See ipv6_clean_nexthop for details on a solution.
	       *
	       * I am now in the STALE state.
	       */
	    }
	}
    }

  timeout(ipv6_neighbor_timer,NULL,v6d_retranstime * hz);
  splx(s);
}

/*----------------------------------------------------------------------
 * ipv6_clean_nexthop:
 *	Delete requested route, if either the route is both GATEWAY and HOST  
 *      and not held (or the caller REALLY requests it).
 ----------------------------------------------------------------------*/
int
ipv6_clean_nexthop(rn, arg)
     struct radix_node *rn;
     void *arg;
{
  struct rtentry *rt = (struct rtentry *)rn;
#ifdef	__alpha__
  long hardcore = (long)arg; 
#else
  int hardcore = (int)arg; 
#endif
  /* If hardcore is not zero, then the caller REALLY insists that we
     delete the route. A drain function would call this with hardcore != 0. */

  DPRINTF(GROSSEVENT,("Entering ipv6_clean_nexthop... "));
  if (!(rt->rt_flags & RTF_HOST) ||
      !(rt->rt_flags & (RTF_LLINFO|RTF_TUNNEL|RTF_GATEWAY)) ||
      /*  Keep the static host routes; unless told to delete?  */
      ( (rt->rt_flags & (RTF_STATIC)) && !hardcore) ||
      (!hardcore && rt->rt_refcnt > 0))
    {
      /*
       * Unless asked (i.e. a hardcore clean :), don't delete held routes. 
       * Only delete host routes (that aren't my own addresses) this way.
       */
      
      DPRINTF(GROSSEVENT,("not deleting.\n"));
      return 0;
    }

  if ((rt->rt_flags & RTF_LLINFO) && rt->rt_refcnt == 0)
    {
      struct discq *dq = (struct discq *)rt->rt_llinfo;
      /*
       * This is a neighbor cache entry.  Delete only if not held, and
       * if in STALE state (i.e. pre-PROBE) for too long.
       */
#ifdef __FreeBSD__ 
      if (rt->rt_rmx.rmx_expire + v6d_toolong >= time_second &&
#else /* __FreeBSD__ */
      if (rt->rt_rmx.rmx_expire + v6d_toolong >= time.tv_sec &&
#endif /* __FreeBSD__ */
	  dq->dq_unanswered <= 0)
	return 0;
      /*
       * In case clean_nexthop catches one of these non-determinate
       * neighbor entries...
       * (May be unnecessary if rtfree() ifa != NULL check is in place.)
       */
      if (rt->rt_ifa == NULL)
	{
	  rt->rt_ifa = (struct ifaddr *)in6_ifaddr;
	  rt->rt_ifa->ifa_refcnt++;
	}
    }

  /*
   * At this point, the route is RTF_HOST, and is either a
   * force or a legitimate node to be cleaned.  Call RTM_DELETE...
   */
  DPRINTF(GROSSEVENT,("deleting.\n"));

  return rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);
}

/*----------------------------------------------------------------------
 * ipv6_nexthop_timer():
 *      Keeps routing table from getting too filled up with off-net host
 *      routes.
 * 
 * NOTES:
 * 	Later might want to put some intelligence in here like __FreeBSD__ does, 
 * but for now just do an rnh->rnh_walktree() every so often.
 *
 * 	A smarter function might:
 *
 *         - Keep a count of how many off-net host routes we have.
 *         - Maintain upper bounds on said count.
 *         - Avoid deletion if said count is low.
 *         - Etc.
 ----------------------------------------------------------------------*/

void
ipv6_nexthop_timer(whocares)
     void *whocares;
{
  struct radix_node_head *rnh = rt_tables[AF_INET6];
  int s, rc;

  DPRINTF(IDL_EVENT,("Entering ipv6_nexthop_timer().\n"));
  s = splnet();
  rc = rnh->rnh_walktree(rnh, ipv6_clean_nexthop, (void *)0);
  splx(s);

  DDO(IDL_ERROR,if (rc != 0)\
                     printf("walktree rc is %d in nexthop_timer.\n", rc));

  timeout(ipv6_nexthop_timer,NULL,v6d_nexthopclean * hz);
}

/*----------------------------------------------------------------------
 * ipv6_discovery_init():
 *	Initializes ND data structures.
 ----------------------------------------------------------------------*/
void
ipv6_discovery_init()
{
  dqhead.dq_next = dqhead.dq_prev = &dqhead;
  defrtr.v6r_next = defrtr.v6r_prev = &defrtr;
  nondefrtr.v6r_next = nondefrtr.v6r_prev = &nondefrtr;

  in6_allones.sin6_family = AF_INET6;
  in6_allones.sin6_len = sizeof(in6_allones);

  /* Other fields are bzeroed. */
  in6_allones.sin6_addr.in6a_words[0] = 0xffffffff;
  in6_allones.sin6_addr.in6a_words[1] = 0xffffffff;
  in6_allones.sin6_addr.in6a_words[2] = 0xffffffff;
  in6_allones.sin6_addr.in6a_words[3] = 0xffffffff;
  
  timeout(ipv6_nexthop_timer,NULL,hz);
  timeout(ipv6_neighbor_timer,NULL,hz);
}

/*----------------------------------------------------------------------
 * get_discov_cluster():
 * 	Allocates a single-cluster mbuf and sets it up for use by ND.
 *
 ----------------------------------------------------------------------*/

struct mbuf *
get_discov_cluster()
{
  struct mbuf *rc;

  MGET(rc, M_DONTWAIT, MT_HEADER);
  if (rc != NULL)
    {
      MCLGET(rc,M_DONTWAIT);
      if ((rc->m_flags & M_EXT) == 0)
	{
	  m_free(rc);
	  rc = NULL;
	}
    }

  /* Make it a pkthdr appropriately. */
  if (rc) 
    {
      rc->m_flags |= M_PKTHDR;
      rc->m_pkthdr.rcvif = NULL;
      rc->m_pkthdr.len = 0;
      rc->m_len = 0;
    }
  return rc;
}

/*----------------------------------------------------------------------
 * send_nsolicit():
 * 	Send a neighbor solicit for destination in route entry rt,
 * across interface pointed by ifp.  Use source address in src (see below),
 * and either unicast or multicast depending on the mcast flag.
 *
 * NOTES:	The entry pointed to by rt MUST exist.
 *   If the caller has to, it may cruft up a dummy rtentry with at least
 *   a valid rt_key() for the neighbor's IPv6 address, and maybe
 *   a dummy rt_gateway.
 *
 *   If src points to 0::0 address, set M_DAD flag so ipv6_output() doesn't
 *   try and fill in a source address.
 ----------------------------------------------------------------------*/
void
send_nsolicit(rt,ifp,src,mcast)
     struct rtentry *rt;
     struct ifnet *ifp;
     struct in6_addr *src;    /* Source address of invoking packet... */
     int mcast;
{
  struct mbuf *solicit = NULL;
  struct ipv6 *header;
  struct ipv6_icmp *icmp;
  struct ipv6_moptions i6mo,*i6mop = NULL;
  struct in6_ifaddr *i6a;
  struct sockaddr_in6 *neighbor  = (struct sockaddr_in6 *)rt_key(rt);
  struct sockaddr_dl *sdl = (struct sockaddr_dl *)rt->rt_gateway;

  DPRINTF(IDL_EVENT,("Entering send_nsolicit with\n"));
  DPRINTF(IDL_EVENT,("rt=0x%lx\n",(unsigned long)rt));
  DPRINTF(IDL_EVENT,("ifp:\n"));
  DDO(IDL_EVENT,dump_ifp(ifp));
  DPRINTF(IDL_EVENT,("src:\n"));
  DDO(IDL_EVENT,dump_in6_addr(src));
  DPRINTF(IDL_EVENT,("mcast=%d\n",mcast));

  if ((solicit = get_discov_cluster()) == NULL)
    {
      DPRINTF(IDL_ERROR, ("Can't allocate mbuf in send_gsolicit().\n"));
      return;
    }

  header = mtod(solicit,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(header + 1);/* I want the bytes after the hdr. */

  if (mcast)
    {
      bzero(&i6mo,sizeof(struct ipv6_moptions));
      i6mo.i6mo_multicast_ifp = ifp;
      i6mo.i6mo_multicast_ttl = 255;  /* Must set, hoplimit is otherwise
					 overridden! */
      i6mop = &i6mo;

      header->ipv6_dst.in6a_words[0] = htonl(0xff020000);
      header->ipv6_dst.in6a_words[1] = 0;
      header->ipv6_dst.in6a_words[2] = htonl(1);
      header->ipv6_dst.in6a_words[3] = neighbor->sin6_addr.in6a_words[3] | htonl(0xff000000);
    }
  else
    {
      if (sdl == NULL || sdl->sdl_alen == 0)
	{
	  DPRINTF(IDL_ERROR, ("Can't unicast if I don't know address.\n"));
	  m_freem(solicit);
	  return;
	}
      header->ipv6_dst = neighbor->sin6_addr;
      /* If rmx_expire is not already set, set it to avoid chicken-egg loop. */
    }

  header->ipv6_versfl = htonl(0x6f000000);
  header->ipv6_hoplimit = 255;  /* Guaranteed to be intra-link if arrives with
				   255. */
  header->ipv6_nexthdr = IPPROTO_ICMPV6;
  header->ipv6_length = ICMPV6_NSOLMINLEN;  /* For now. */

  /*
   * Now find source address for solicit packet.
   *
   * Rules on src address selection:
   *       if NULL find link-local for i/f.  
   *              If no link-local, then use unspec source.  
   *              (If unspec source and !mcast, fail)
   *       if UNSPEC src
   *              if not mcast, fail.
   *       if a real address, sanity check to see if it's indeed my address.
   *              Additionally check to see if it matches the
   *              outbound ifp requested.
   */

  if (src == NULL)
    {
      /* Find source link-local or use unspec. */
      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
	if (i6a->i6a_ifp == ifp && (i6a->i6a_addrflags & I6AF_LINKLOC) &&
	    !(i6a->i6a_addrflags & I6AF_NOTSURE))
	  break;
      if (i6a == NULL) {
	if (mcast)
	  header->ipv6_src = in6addr_any;
	else {
          DPRINTF(IDL_ERROR, ("send_nsolicit: Unicast solicit w/o any known source address.\n"));
          m_freem(solicit);
          return;
	}
      } else
        header->ipv6_src = i6a->i6a_addr.sin6_addr;
    }
  else
    if (IN6_IS_ADDR_UNSPECIFIED(src)) {
      if (!mcast) {
	DPRINTF(IDL_ERROR, ("send_nsolicit: Unicast DAD solicit.\n"));
	m_freem(solicit);
	return;
      } else {
	DPRINTF(GROSSEVENT, ("Sending DAD solicit.\n"));
	solicit->m_flags |= M_DAD;
	header->ipv6_src = in6addr_any;
      }
    } else {
      struct in6_ifaddr *llsave = NULL;

      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
	{
	  /*
	   * Might want to put llsave code where it's actually needed.
	   * (i.e. take it out of this loop and put it in i6a == NULL case.)
	   */
	  if (i6a->i6a_ifp == ifp && IN6_IS_ADDR_LINKLOCAL(&i6a->i6a_addr.sin6_addr))
	    llsave = i6a;
	  if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, src))
	    break;
	}
      if (i6a == NULL)
	{
	  /*
	   * This path is entered by a router forwarding packets to a yet
	   * undiscovered neighbor.
	   */
#ifdef DIAGNOSTIC
	  if (llsave == NULL)
	    panic("No link-local for this address.");
#endif /* DIAGNOSTIC */
	  header->ipv6_src = llsave->i6a_addr.sin6_addr;
	}
      else if (i6a->i6a_ifp != ifp)
	{
	  /*
	   * Q:  Is this a reason to be panicking?
	   * A:  For now, no.
	   */
	  DDO(IDL_ERROR,\
	      printf("WARNING:  Src addr fubar Addr, ifp, i6a: ");\
	      dump_in6_addr(src);dump_ifp(ifp);\
	      dump_ifa((struct ifaddr *)i6a));
#ifdef DIAGNOSTIC
	  if (llsave == NULL)
	    panic("No link-local for this address.");
#endif /* DIAGNOSTIC */
	  header->ipv6_src = llsave->i6a_addr.sin6_addr;
	}
      else header->ipv6_src = *src;
    }

  /* Have source address, now create outbound packet */
  icmp->icmp_type = ICMPV6_NEIGHBORSOL;
  icmp->icmp_code = 0;
  icmp->icmp_unused = 0;
  icmp->icmp_cksum = 0;
  icmp->icmp_nsoltarg = neighbor->sin6_addr;

  /*
   * Put ND extensions here, if any.
   * This next code fragment could be its own function if there
   * were enough callers of the fragment to make that sensible.
   *
   * This might also want to be its own function because of variations in
   * link (ifp) type.
   */
  if (ifp->if_addrlen != 0 && !IN6_IS_ADDR_UNSPECIFIED(&header->ipv6_src))
    {
      struct icmp_exthdr *ext = (struct icmp_exthdr *)&(icmp->icmp_nsolext);
      struct ifaddr *ifa;
      struct sockaddr_dl *lsdl;

      ext->ext_id = EXT_SOURCELINK;

      switch (ifp->if_type)
	{
	case IFT_ETHER:
	  ext->ext_length = 1 + ((ifp->if_addrlen + 1) >> 3);
#ifdef __FreeBSD__
	  for (ifa = ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	  for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
	  for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	    if (ifa->ifa_addr->sa_family == AF_LINK)
	      break;
	  if (ifa == NULL)
	    {
	      DPRINTF(IDL_ERROR, ("Can't find link addr. in nsolicit().\n"));
	      m_freem(solicit);
	      return;
	    }
	  lsdl = (struct sockaddr_dl *)ifa->ifa_addr;
	  bzero(ext->ext_data,ext->ext_length * 8 - 2);
	  bcopy(LLADDR(lsdl),ext->ext_data,ifp->if_addrlen);
	  break;
	default:
	  DPRINTF(IDL_ERROR,("DANGER: unk. link type for n. sol.\n"));
	  break;
	}

      header->ipv6_length += ext->ext_length <<3;
    }

  solicit->m_len = solicit->m_pkthdr.len = 
                       header->ipv6_length + sizeof(*header);
  icmp->icmp_cksum = in6_cksum(solicit,IPPROTO_ICMPV6,header->ipv6_length,
			       sizeof(struct ipv6));

  /*
   * NOTE: We pass in a NULL instead of a valid
   * socket ptr.  When ipv6_output() calls ipsec_output_policy(),
   * this socket ptr will STILL be NULL.  Sooo, the security
   * policy on outbound packets from here will == system security
   * level (set in ipsec_init()).  If your
   * system security level is paranoid, then you won't move packets
   * unless you have _preloaded_ keys for at least the ND addresses. 
   *  - danmcd rja
   */

  ipv6_output(solicit, NULL, IPV6_RAWOUTPUT, i6mop, NULL, NULL);
}

/*----------------------------------------------------------------------
 * ipv6_nsolicit:   
 *	Send an IPv6 Neighbor Solicit message
 *
 * NOTES:
 * State checking is needed so that a neighbor can be declared unreachable.
 *
 * newrt == NULL iff this is a virgin packet with no known i/f,
 * otherwise valid newrt MUST be passed in.
 *
 * If ifp == NULL, ipv6_nsolicit() executes potentially multihomed code.
 * (For now, I guess I should pick an arbitrary interface.  For single-homed
 * nodes, this is the optimal behavior.)
 * This is called when a neighbor is in INCOMPLETE state.
 *
 * The NUD INCOMPLETE and new-neighbor detection state belongs here.
 *
 * Not yet clear how will implement duplicate address detection.
 *                       (See send_nsolicit.)
 *                  Should I splnet() inside here?
 ----------------------------------------------------------------------*/
void
ipv6_nsolicit(outifp,outgoing,newrt)
     struct ifnet *outifp;
     struct mbuf *outgoing;
     struct rtentry *newrt;
{
  struct discq *dq;
  struct ipv6 *ipv6 = (outgoing == NULL) ? NULL : 
                     mtod(outgoing,struct ipv6 *);  

  /*
   * ASSERT: The header is pulled up, and has either the
   * unspecified address or one of MY valid ipv6 addresses.
   */
#if __NetBSD__ || __FreeBSD__ || __OpenBSD__ 
  struct ifnet *ifp = (outifp == NULL)?ifnet.tqh_first:outifp;
#else /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */
  struct ifnet *ifp = (outifp == NULL)?ifnet:outifp;
#endif /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */

  DPRINTF(IDL_EVENT,("Entering IPV6_NSOLICIT.\n"));
  DPRINTF(IDL_EVENT,("outifp:\n"));
  DDO(IDL_EVENT,dump_ifp(outifp));
  DPRINTF(IDL_EVENT,("outgoing=0x%lx\n",(unsigned long)outgoing));
  DPRINTF(IDL_EVENT,("newrt:\n"));
  DDO(IDL_EVENT,dump_rtentry(newrt));
  DPRINTF(IDL_EVENT,("ifp:\n"));
  DDO(IDL_EVENT,dump_ifp(ifp));

  if (newrt == NULL)
    {
      DPRINTF(IDL_ERROR, 
	      ("ipv6_nsolicit() called with newrt == NULL.\n"));
      m_freem(outgoing);
      return;
    }

  /*
   *  Route with RTF_REJECT has dummy ifa assigned to it in
   *  ipv6_neighbor_timer().  We shouldn't need to send out
   *  another solicit if the route is already marked REJECT.
   */
  if (newrt->rt_flags & RTF_REJECT)
    {
      DPRINTF(ERROR, ("ipv6_nsolicit passed a RTF_REJECT route\n"));
      m_freem(outgoing);
      return;
    }

  dq = (struct discq *)newrt->rt_llinfo;
  DPRINTF(GROSSEVENT,("dq:\n"));
  DDO(GROSSEVENT,dump_discq(dq));
  DDO(IDL_ERROR,if (dq == NULL) \
                        panic("dq == NULL in nsolicit()."); \
                      if (dq->dq_rt != newrt)\
                        panic("dq <-> rt mismatch in nsolicit.");\
                      {\
		       struct sockaddr_dl *sdl = (struct sockaddr_dl *)\
			                          newrt->rt_gateway;\
			 \
		       if (sdl->sdl_alen != 0)\
			 panic("sdl_alen not 0 in nsolicit!");
                      });
  DDO(GROSSEVENT,if (outifp == NULL)
                        printf("nsolicit passed in outifp of NULL.\n"));

  /*
   * If new entry, set up certain variables.
   */
  if (newrt->rt_rmx.rmx_expire == 0)
    dq->dq_unanswered = 0;

  /* 
   * Currently queue the last packet sent.  May want to
   * queue up last n packets. 
   */

  if (outgoing != NULL)
    {
      if (dq->dq_queue != NULL)
	m_freem(dq->dq_queue);
      dq->dq_queue = outgoing;
    }

  /*
   * We want to rate-limit these, so only send out if the time has
   * expired.
   */
#ifdef __FreeBSD__ 
  if (newrt->rt_rmx.rmx_expire < time_second)
#else /* __FreeBSD__ */
  if (newrt->rt_rmx.rmx_expire < time.tv_sec)
#endif /* __FreeBSD__ */
    {
      DPRINTF(IDL_EVENT,
	      ("ipv6_nsolict: solicit time expired -- send another one!\n"));
#ifdef __FreeBSD__
      newrt->rt_rmx.rmx_expire = time_second + v6d_retranstime;
#else /* __FreeBSD__ */
      newrt->rt_rmx.rmx_expire = time.tv_sec + v6d_retranstime;
#endif /* __FreeBSD__ */
      dq->dq_unanswered++;   /* Overload for "number of m-cast probes sent". */
      do
	{
	  if (ifp->if_type != IFT_LOOP && ipv6_enabled(ifp))
	    {
	      if (ipv6 == NULL && dq->dq_queue != NULL) {
		ipv6 = mtod(dq->dq_queue,struct ipv6 *);
		DPRINTF(IDL_EVENT,("v6_solicit: grabbing src from dq!\n"));
	      }
	      DPRINTF(IDL_EVENT,("dq b/f calling send_nsolicit:\n"));
	      DDO(IDL_EVENT,dump_discq(dq));
	      DPRINTF(IDL_EVENT,("ipv6 hdr b/f calling send_nsolicit:\n"));
	      DDO(IDL_EVENT,dump_ipv6(ipv6));
	      send_nsolicit(newrt,ifp,(ipv6 == NULL) ? NULL : &ipv6->ipv6_src,
			    1);
	    }

	  if (outifp == NULL)
#ifdef __FreeBSD__
	    ifp = ifp->if_link.tqe_next;
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	    ifp = ifp->if_list.tqe_next;
#else /* __NetBSD__ || __OpenBSD__ */
	    ifp = ifp->if_next;
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	  else ifp = NULL;
	}
      while (ifp != NULL);
    }
}

/*----------------------------------------------------------------------
 * ipv6_onlink_query():
 *     If I have no route, and now assume that a destination is on-link,
 *     then I should create a route entry (I'll have to do it using raw
 *     radix code, because I don't know which interface the destination is
 *     for yet.)
 *
 *     I should probably optimize this for single-homed hosts, such that
 *     the route lookup after doing this will return with an rt_ifa.
 ----------------------------------------------------------------------*/

void
ipv6_onlink_query(dst)
     struct sockaddr_in6 *dst;       /* Dest. sockaddr_in6 */
{
  struct rtentry *rt;
  struct sockaddr_dl *sdl;
  struct in6_ifnet *i6ifp = in6_ifnet;
  struct ifnet *ifp;
  int s = splnet();

  /*
   * Pick an interface, trick ipv6_new_neighbor() into adding a route,
   * then blank out the interface-specific stuff.
   */

  while (i6ifp != NULL && (i6ifp->i6ifp_ifp->if_flags & IFF_LOOPBACK))
    i6ifp = i6ifp->i6ifp_next;
  
  if (i6ifp == NULL)
    {
      DPRINTF(IDL_ERROR,("Oooh boy.  No non-loopback i6ifp.\n"));
      splx(s);
      return;
    }
  ifp = i6ifp->i6ifp_ifp;

  rt = ipv6_new_neighbor(dst,ifp);
  if (rt == NULL)
    {
      DPRINTF(IDL_ERROR,("ipv6_new_neighbor failed in onlink_query.\n"));
      splx(s);
      return;
    }

  if (rt->rt_gateway->sa_family != AF_LINK) {
    DPRINTF(IDL_ERROR,("onlink_query returns route with non AF_LINK gateway.\n"));
    splx(s);
    return;
  }

  sdl = (struct sockaddr_dl *)rt->rt_gateway;
  sdl->sdl_index = 0;
  sdl->sdl_nlen = 0;
  sdl->sdl_alen = 0;
  sdl->sdl_slen = 0;

  rt->rt_ifp = NULL;
  rt->rt_ifa->ifa_refcnt--;
  rt->rt_ifa = NULL;
  rt->rt_rmx.rmx_mtu = 0;

  /*
   * I think I'm cool, now.  So send a multicast nsolicit.
   */
  ipv6_nsolicit(NULL,NULL,rt);
  splx(s);
}

/*----------------------------------------------------------------------
 * ipv6_verify_onlink():
 *      Verify sockaddr_in6 dst has been determined to be a neighbor
 *      or not.  Will only work on an output/route lookup caused by
 *      a process that's trapped in the kernel.  I do a tsleep of the
 *      current process while neighbor discovery runs its course.
 ----------------------------------------------------------------------*/

int
ipv6_verify_onlink(dst)
     struct sockaddr_in6 *dst;
{
  struct rtentry *rt = NULL;
#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
  struct proc *p = curproc; /* XXX */
#else /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
  struct proc *p = PCPU(curproc);               /* XXX */
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */

  if (!p) /* i.e. if I'm an interrupt-caused output... */
    return 1;

  do
    {
      if (rt != NULL)
	rt->rt_refcnt--;
      /*
       * Sleep this process, then lookup neighbor route to see if it's
       * been updated.
       */
      switch (tsleep((caddr_t)&rt, PCATCH , "mhomed", hz))
	{
	case 0:
	  DPRINTF(IDL_ERROR,("How did I awaken?\n"));
	  /* Fallthrough to... */
	case EWOULDBLOCK:
	  break;
	case EINTR:
	case ERESTART:
	  return EINTR;
	}
#ifdef __FreeBSD__ 
      rt = rtalloc1((struct sockaddr *)dst,0, 0UL);
#else /* __FreeBSD__ */
      rt = rtalloc1((struct sockaddr *)dst,0);
#endif /* __FreeBSD__ */
    }
  while (rt && rt->rt_ifa == NULL);

  DPRINTF(FINISHED, ("verify_onlink came up with rt:\n"));
  DDO(FINISHED, dump_rtentry(rt));
  if (rt == NULL)
    return -1;
  rt->rt_refcnt--;
  return 0;
}

/*----------------------------------------------------------------------
 * ipv6_uni_nsolicit():
 * 	Send a unicast neighbor solicit to neighbor specified in rt.
 *      Also update the next-probe time.
 *
 * NOTES:
 *    This is so small that it might be better inlined.
 ----------------------------------------------------------------------*/

void
ipv6_uni_nsolicit(rt)
     struct rtentry *rt;
{
  struct discq *dq = (struct discq *)rt->rt_llinfo;

#ifdef __FreeBSD__
  rt->rt_rmx.rmx_expire = time_second + v6d_retranstime;
#else /* __FreeBSD__ */
  rt->rt_rmx.rmx_expire = time.tv_sec + v6d_retranstime;
#endif /* __FreeBSD__ */
  /* If ifa->ifa_addr doesn't work, revert to NULL. */
  send_nsolicit(rt,rt->rt_ifp,&(I6A_SIN(rt->rt_ifa)->sin6_addr),0);
  dq->dq_unanswered ++;
}


/*----------------------------------------------------------------------
 *ipv6_nadvert():
 *   Construct an IPv6 neighbor advertisement, 
 *   and send it out via either unicast or multicast.
 * 
 * NOTES:
 *   Might later add a proxy advertisement bit or anycast bit to the flags
 *   parameter.
 ----------------------------------------------------------------------*/
void
ipv6_nadvert(i6a,ifp,dstrt,flags)
     struct in6_ifaddr *i6a;
     struct ifnet *ifp;
     struct rtentry *dstrt;     /* If null, send m-cast neighbor advert. */
     uint32_t flags;
{
  struct ipv6_moptions i6mo,*i6mop = NULL;
  struct mbuf *advert;
  struct ipv6 *ipv6;
  struct ipv6_icmp *icmp;

  advert = get_discov_cluster();
  if (advert == NULL)
    return;

  ipv6 = mtod(advert,struct ipv6 *);
  icmp = (struct ipv6_icmp *)&(advert->m_data[sizeof(struct ipv6)]);

  ipv6->ipv6_src        = i6a->i6a_addr.sin6_addr;  /* May be different for
						       proxy. */
  ipv6->ipv6_versfl	= htonl(0x6f000000);
  ipv6->ipv6_hoplimit	= 255;  /* Guaranteed to be intra-link if arrives with
				   255. */
  ipv6->ipv6_nexthdr	= IPPROTO_ICMPV6;
  ipv6->ipv6_length	= ICMPV6_NADVMINLEN; /* For now */
  icmp->icmp_type	= ICMPV6_NEIGHBORADV;
  icmp->icmp_code	= 0;
  icmp->icmp_cksum	= 0;
  icmp->icmp_nadvbits	= flags;

  /* Flags in ipv6_icmp.h are in endian-specific #ifdefs, so
     there is no need to HTONL(icmp->icmp_nadvbits);   */ 

  /* If proxy advert, set proxy bits. */
  icmp->icmp_nadvaddr = i6a->i6a_addr.sin6_addr;

  if (dstrt == NULL)
    {
      struct in6_addr addr = IN6ADDR_ALLNODES_INIT;

      i6mo.i6mo_multicast_ifp = ifp;
      i6mo.i6mo_multicast_ttl = 255;  /* Must set. */
      i6mop = &i6mo;

      ipv6->ipv6_dst = addr;
    }
  else ipv6->ipv6_dst = ((struct sockaddr_in6 *)rt_key(dstrt))->sin6_addr;

  /*
   * Set up extensions (if any)
   */

  /*
   * Perhaps create a separate function to look through interface's address
   * list to find my data link address, but if this would really be called
   * enough other places...
   */
  if (i6a->i6a_ifp->if_addrlen != 0)    
    {
      struct icmp_exthdr *ext = (struct icmp_exthdr *)&(icmp->icmp_nadvext);
      struct ifaddr *ifa;
      struct sockaddr_dl *lsdl;   /* Target's Local Link-layer Sockaddr */

      ext->ext_id = EXT_TARGETLINK;
      switch (i6a->i6a_ifp->if_type)
	{
	case IFT_ETHER:
	  ext->ext_length = 1 + ((ifp->if_addrlen +1) >> 3);
#ifdef __FreeBSD__
	  for (ifa = ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	  for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
	  for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	    if (ifa->ifa_addr->sa_family == AF_LINK)
	      break;
	  if (ifa == NULL)
	    {
	      DPRINTF(IDL_ERROR, ("Can't find link addr. in nadvert().\n"));
	      m_freem(advert);
	      return;
	    }
	  lsdl = (struct sockaddr_dl *)ifa->ifa_addr;
	  bzero(ext->ext_data,ext->ext_length*8 - 2);
	  bcopy(LLADDR(lsdl),ext->ext_data,ifp->if_addrlen);
	  break;
	default:
	  DPRINTF(IDL_ERROR,("DANGER: sending n. adv on unk. link type.\n"));
	  break;
	}
      ipv6->ipv6_length += ext->ext_length <<3;
    }

  advert->m_len = advert->m_pkthdr.len = ipv6->ipv6_length + sizeof(*ipv6);
  icmp->icmp_cksum = in6_cksum(advert,IPPROTO_ICMPV6,ipv6->ipv6_length,
			       sizeof(struct ipv6));
  ipv6_output(advert,NULL,IPV6_RAWOUTPUT,i6mop, NULL, NULL);
}

/*----------------------------------------------------------------------
 * ipv6_routersol_input():
 *	Handle reception of Router Solicit messages.
 *
 ----------------------------------------------------------------------*/
void
ipv6_routersol_input(incoming, extra)
     struct mbuf *incoming;
     int extra;
{
  /*
   * Return and let user-level process deal with it.
   */
}

/* Add a default router in response to a route request (RTM_ADD/RTM_RESOLVE) */

struct v6router *ipv6_add_defrouter_rtrequest(struct rtentry *inrt)
{
#if 0
  struct rtentry *rt, *defrt=NULL;
  struct v6router *v6r;
  struct sockaddr_in6 sin6;
#endif /* 0 */

  DPRINTF(GROSSEVENT, ("ipv6_add_defrouter_rtrequest(inrt=%08x)\n", OSDEP_PCAST(inrt)));
  DDO(GROSSEVENT, dump_rtentry(inrt));

#if 0
  sin6.sin6_family = AF_INET6;
  sin6.sin6_len = sizeof(sin6);
  sin6.sin6_port = 0;
  sin6.sin6_flowinfo = 0;

  if (!(rt = inrt->rt_gwroute))
#ifdef __FreeBSD__
    if (!(rt = inrt->rt_gwroute = rtalloc1(inrt->rt_gateway, 1, 0UL)))
#else /* __FreeBSD__ */
    if (!(rt = inrt->rt_gwroute = rtalloc1(inrt->rt_gateway, 1)))
#endif /* __FreeBSD__ */
      return NULL;

  if (!(v6r = malloc(sizeof(*v6r),M_DISCQ,M_NOWAIT))) {
    DPRINTF(IDL_ERROR, ("ipv6_add_defrouter_rtrequest: Can't allocate router list entry.\n"));
    /* Q: Do I want to delete the neighbor entry? */
    /*rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);*/
    return NULL;
  };

  /*
   * Assertion:
   *              Tunnel default routes are added only by manual
   *              (i.e. user-level) adds and deletes.
   * If that's true, then the route in the v6r structure is "held" by
   * the default route.  Oooh, I'd better make sure that default router
   * entries hold their... wait a minute, when I delete neighbors, the
   * default router neigbors are searched in the list (and nuked...).
   */

  bzero(v6r,sizeof(*v6r));
  v6r->v6r_rt = rt;
  v6r->v6r_children.v6c_next = v6r->v6r_children.v6c_prev = &v6r->v6r_children;

  if (!inrt->rt_gwroute) {
    /*
     * First default router added, and not a manual add.  Add
     * default route.
     */
    struct sockaddr_in6 mask;

    bzero(&mask,sizeof(mask));
    mask.sin6_family = AF_INET6;
    mask.sin6_len = sizeof(struct sockaddr_in6);
    
    sin6.sin6_addr = in6addr_any;
    
    DDO(IDL_EVENT,printf("------(Before rtrequest)----------\n");\
	dump_smart_sockaddr((struct sockaddr *)&sin6);\
	printf("----------------\n"));

    insque(v6r,&defrtr);   /* To prevent double-adds. */
    if ((rc = rtrequest(RTM_ADD,(struct sockaddr *)&sin6,rt_key(rt),
			(struct sockaddr *)&mask,RTF_DEFAULT|RTF_GATEWAY,
			&defrt))) {
      remque(v6r);
      DPRINTF(IDL_ERROR, ("ipv6_add_defrouter_rtrequest: Default route add failed (%d).\n",rc));
      free(v6r, M_DISCQ);
      /* Q: Do I want do delete the neighbor entry? */
      /* rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);*/
      return NULL;
    }
    defrt->rt_refcnt--;
    /*defrt->rt_flags |= RTF_CLONING;*/
  } else
    insque(v6r,&defrtr);    /* If double-add prevention not needed. */

  return v6r;
#else /* 0 */

  if (!inrt->rt_gwroute)
#if __FreeBSD__ 
    if (!(inrt->rt_gwroute = rtalloc1(inrt->rt_gateway, 1, 0)))
#else /* __FreeBSD__ */
    if (!(inrt->rt_gwroute = rtalloc1(inrt->rt_gateway, 1)))
#endif /* __FreeBSD__ */
      return NULL;

  return &defrtr;
#endif /* 0 */
};

/* Add a default router in response to a router advertisement.
   ipv6_add_defrouter_rtrequest ends up getting called underneath because of
   the rtalloc1(). */

struct v6router *ipv6_add_defrouter_advert(struct in6_addr *addr, int lifetime, struct ifnet *ifp)
{
  struct rtentry *rt, *defrt=NULL;
  struct v6router *v6r;
  struct sockaddr_in6 sin6;
  int rc;

  DPRINTF(IDL_EVENT, ("ipv6_add_defrouter_advert(addr=%08x, lifetime=%d, ifp=%08x)\n", OSDEP_PCAST(addr), lifetime, OSDEP_PCAST(ifp)));

  sin6.sin6_family = AF_INET6;
  sin6.sin6_len = sizeof(sin6);
  sin6.sin6_port = 0;
  sin6.sin6_flowinfo = 0;
  sin6.sin6_addr = *addr;

  /*
   * Find it if it already exists.  Router adverts come from
   * link-local addresses, so doing rtalloc1() will be safe.
   * If this function is called because of a manual add, inrt should
   * be non-NULL, so this codepath won't be executed.
   */
#if __FreeBSD__
  rt = rtalloc1((struct sockaddr *)&sin6, 0, 0);
#else /* __FreeBSD__ */
  rt = rtalloc1((struct sockaddr *)&sin6, 0);
#endif /* __FreeBSD__ */
  if (rt == NULL || !(rt->rt_flags & RTF_LLINFO)) {
    if (rt != NULL)
      RTFREE(rt);
    rt = ipv6_new_neighbor(&sin6,ifp);
  } else
    rt->rt_refcnt--;

  DDO(GROSSEVENT,printf("After new_neighbor:\n"); dump_smart_sockaddr((struct sockaddr *)&sin6));
  if (rt == NULL) {
    DPRINTF(IDL_ERROR, ("ipv6_add_defrouter's new neighbor failed.\n"));
    return NULL;
  };
  rt->rt_flags |= RTF_DEFAULT|RTF_ISAROUTER;

  if (!(v6r = malloc(sizeof(struct v6router), M_DISCQ, M_NOWAIT))) {
    DPRINTF(IDL_ERROR,("ipv6_add_defrouter_advert: Can't allocate router list entry.\n"));
    /* Q: Do I want to delete the neighbor entry? */
    /*rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);*/
    return NULL;
  };

  /*
   * Assertion:
   *              Tunnel default routes are added only by manual
   *              (i.e. user-level) adds and deletes.
   * If that's true, then the route in the v6r structure is "held" by
   * the default route.  Oooh, I'd better make sure that default router
   * entries hold their... wait a minute, when I delete neighbors, the
   * default router neigbors are searched in the list (and nuked...).
   */

  bzero(v6r,sizeof(*v6r));
  v6r->v6r_rt = rt;
  v6r->v6r_children.v6c_next = v6r->v6r_children.v6c_prev = &v6r->v6r_children;
#ifdef __FreeBSD__
  v6r->v6r_expire = time_second + lifetime;
#else /* __FreeBSD__ */
  v6r->v6r_expire = time.tv_sec + lifetime;
#endif /* __FreeBSD__ */

  if (defrtr.v6r_next == &defrtr) {
    /*
     * First default router added, and not a manual add.  Add
     * default route.
     */
    struct sockaddr_in6 mask;

    bzero(&mask,sizeof(mask));
    mask.sin6_family = AF_INET6;
    mask.sin6_len = sizeof(struct sockaddr_in6);
    
    sin6.sin6_addr = in6addr_any;
    
    DDO(GROSSEVENT,printf("------(Before rtrequest)----------\n");\
	dump_smart_sockaddr((struct sockaddr *)&sin6);\
	printf("----------------\n"));

    insque(v6r, &defrtr); /* To prevent double-adds. */
    if ((rc = rtrequest(RTM_ADD, (struct sockaddr *)&sin6, rt_key(rt),
			(struct sockaddr *)&mask, RTF_DEFAULT|RTF_GATEWAY,
			&defrt))) {
      remque(v6r);
      DPRINTF(IDL_ERROR, ("ipv6_add_defrouter_advert: Default route add failed (%d).\n", rc));
      free(v6r, M_DISCQ);
      /* Q: Do I want do delete the neighbor entry? */
      /* rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);*/
      return NULL;
    };
    defrt->rt_refcnt--;
    /*defrt->rt_flags |= RTF_CLONING;*/
  } else
    insque(v6r, &defrtr);    /* If double-add prevention not needed. */

  return v6r;
}

/*----------------------------------------------------------------------
 * Update a default router entry.
 ----------------------------------------------------------------------*/

static int
update_defrouter(v6r,icmp)
     struct v6router *v6r;
     struct ipv6_icmp *icmp;
{
  /* Is this it?   What if someone deleted the default route? */
#ifdef __FreeBSD__
  v6r->v6r_expire = time_second + ntohs(icmp->icmp_radvlifetime);
#else /* __FreeBSD__ */
  v6r->v6r_expire = time.tv_sec + ntohs(icmp->icmp_radvlifetime);
#endif /* __FreeBSD__ */
  return 0;
}

/*----------------------------------------------------------------------
 * Delete a default router entry.  This function may even delete the
 * default route itself.
 ----------------------------------------------------------------------*/

int
ipv6_delete_defrouter(v6r)
     struct v6router *v6r;
{
  struct sockaddr_in6 sin6,mask;
  struct rtentry *rt = v6r->v6r_rt;
  u_long flags = rt->rt_flags;

  DPRINTF(FINISHED, ("In ipv6_delete_defrouter, v6r = 0x%lx\n", (unsigned long)v6r));

  remque(v6r);
  rt->rt_flags |= RTF_UP;  /* To prevent double-freeing of the route. */
  while (v6r->v6r_children.v6c_next != &v6r->v6r_children)
    {
      DDO(IDL_ERROR,printf("Deleting route (0x%lx):\n",\
			      (unsigned long)v6r->v6r_children.v6c_next->v6c_route);\
	  dump_rtentry(v6r->v6r_children.v6c_next->v6c_route));
      rtrequest(RTM_DELETE,rt_key(v6r->v6r_children.v6c_next->v6c_route),NULL,
		NULL,0,NULL);
    }
  rt->rt_flags = flags;
  /*
   * Do I RTFREE() or do I rtrequest(RTM_DELETE) or do neither?
   * For now, neither, because this is called from either a r. advert.
   * advertising 0, or from an RTM_DELETE request for the neighbor.
   */
  /*RTFREE(v6r->v6r_rt);*/
  free(v6r,M_DISCQ);

  /*
   * What if I'm the last router in the default router list?  If so, then
   * delete the default route.
   */

  if (defrtr.v6r_next == &defrtr)
    {
      DPRINTF(IDL_ERROR,\
	      ("Last router on list deleted.  Deleting default route"));
      bzero(&mask,sizeof(mask));
      bzero(&sin6,sizeof(sin6));
      mask.sin6_family = AF_INET6;
      mask.sin6_len = sizeof(struct sockaddr_in6);
      sin6.sin6_family = AF_INET6;
      sin6.sin6_len = sizeof(struct sockaddr_in6);
      rt->rt_flags |= RTF_UP;   /* To prevent double-freeing. */
      rtrequest(RTM_DELETE,(struct sockaddr *)&sin6,NULL,
		(struct sockaddr *)&mask,0,NULL);
      rt->rt_flags = flags;
    }

  return 1;
}

/*----------------------------------------------------------------------
 * Given an advertised prefix, a mask, and a link-local, create the
 * new address.  Write result into "prefix" argument space.
 * (Should inline this.)
 ----------------------------------------------------------------------*/

static void
prefix_concat(prefix,linkloc,mask)
     struct in6_addr *prefix,*linkloc,*mask;
{
  prefix->in6a_words[0] = (prefix->in6a_words[0] & mask->in6a_words[0]) | (linkloc->in6a_words[0] & ~mask->in6a_words[0]);
  prefix->in6a_words[1] = (prefix->in6a_words[1] & mask->in6a_words[1]) | (linkloc->in6a_words[1] & ~mask->in6a_words[1]);
  prefix->in6a_words[2] = (prefix->in6a_words[2] & mask->in6a_words[2]) | (linkloc->in6a_words[2] & ~mask->in6a_words[2]);
  prefix->in6a_words[3] = (prefix->in6a_words[3] & mask->in6a_words[3]) | (linkloc->in6a_words[3] & ~mask->in6a_words[3]);
}

/*----------------------------------------------------------------------
 * ipv6_routeradv_input():
 *	Handle reception of Router Advertisement messages.
 *
 ----------------------------------------------------------------------*/
void
ipv6_routeradv_input(incoming, extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6 *ipv6;
  struct ipv6_icmp *icmp;
  struct v6router *v6r;
  struct icmp_exthdr *ext;
  int howbig;

  /*
   * Hmmmm, do want to handle some things down here in the kernel, but
   * what about other things, like addrconf?
   */

  DPRINTF(IDL_EVENT, ("OK, got a router advert.\n"));
  if (!ipv6rsolicit) /* If I'm not soliciting routers, ignore this */
    return;

  /* Verify that length looks OK */
  howbig = incoming->m_pkthdr.len - extra;
  if (howbig < ICMPV6_RADVMINLEN)
    return;

  /* XXX - Assumes that the entire packet fits within MCLBYTES. */
  if (incoming->m_len < incoming->m_pkthdr.len)
    if ((incoming = m_pullup2(incoming, incoming->m_pkthdr.len)))
      return;

  ipv6 = mtod(incoming,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(incoming, caddr_t) + extra);

  /* Check to see hop count is 255. */
  if (ipv6->ipv6_hoplimit != 255)
    {
      /*
       * Keep stats on this!
       */
      DPRINTF(IDL_ERROR, ("Received Router Advertisement with hoplimit != 255.\n"));
      return;
    }

  /* Verify source address is link-local */
  if (!IN6_IS_ADDR_LINKLOCAL(&ipv6->ipv6_src))
    {
      DPRINTF(IDL_ERROR, ("Received Router Advertisement with non link-local source address.\n"));
      return;
    }

  /*
   * I now have a router advertisement.
   */

  /*
   * First, find the entry, if available.
   */
  for (v6r = defrtr.v6r_next; v6r != &defrtr; v6r = v6r->v6r_next)
    if (IN6_ARE_ADDR_EQUAL(&ipv6->ipv6_src, &V6R_SIN6(v6r)->sin6_addr))
      break;

  if (v6r == &defrtr) {
    /* Not found. */
    if (icmp->icmp_radvlifetime) {
      if (!(v6r = ipv6_add_defrouter_advert(&ipv6->ipv6_src, ntohs(icmp->icmp_radvlifetime), incoming->m_pkthdr.rcvif))) {
	printf("Problems adding to default router list.\n");
	v6r = &defrtr;
      }
    }
    /* else fallthrough if no lifetime, and not found. */
  } else {
    /* Found. */
    if (icmp->icmp_radvlifetime) {
      /*
       * Perhaps do some reality checking here.  Was the radv snarfed
       * off the same interface?  Is it the same link address?  Is it
       * marked M_AUTHENTIC or M_DECRYPTED?
       */

      if (incoming->m_pkthdr.rcvif != v6r->v6r_rt->rt_ifp) {
	printf("WARNING: radv for router off different interface.\n");
	v6r = &defrtr;
      } else
	if (update_defrouter(v6r,icmp)) {
	  printf("update_defrouter failed on radv_input.\n");
	  v6r = &defrtr;
	}
    } else {
      if (ipv6_delete_defrouter(v6r)) /* XXX */
	printf("ipv6_delete_defrouter failed on radv_input.\n");
      v6r = &defrtr;
    }
  };

  if (icmp->icmp_radvhop)
    ipv6_defhoplmt = icmp->icmp_radvhop;

  if (icmp->icmp_radvbits)
    {
      /*
       * Kick DHCP in the pants to do the right thing(s).
       */
    }

  if (icmp->icmp_radvreach)
    v6d_reachtime = max(1,ntohl(icmp->icmp_radvreach)/1000);

  if (icmp->icmp_radvretrans)
    v6d_retranstime = max(1,ntohl(icmp->icmp_radvretrans)/1000);

  /*
   * Handle extensions/options.
   */
  ext = (struct icmp_exthdr *)icmp->icmp_radvext;

  DPRINTF(IDL_EVENT,\
	  ("Parsing exensions.  ext = 0x%lx, icmp = 0x%lx, howbig = %d.\n",\
	   (unsigned long)ext, (unsigned long)icmp,howbig));

  while ((u_long)ext - (u_long)icmp < howbig)
    {
      struct ext_prefinfo *pre = (struct ext_prefinfo *)ext;

      DPRINTF(GROSSEVENT,("In loop, ext_id = %d.\n",ext->ext_id));

      switch (ext->ext_id)
	{
	case EXT_SOURCELINK:
	  /*
	   * We already have a v6r that may point to the neighbor.  If so,
	   * fill in the sockaddr_dl part of it, and set the neighbor state
	   * STALE.
	   */
	  if (v6r != &defrtr)
	    {
	      /*
	       * i.e. if I have a neighbor cache entry...
	       */
	      struct rtentry *rt = v6r->v6r_rt;
	      struct sockaddr_dl *sdl = (struct sockaddr_dl *)rt->rt_gateway;
	      struct discq *dq = (struct discq *)rt->rt_llinfo;

	      switch (rt->rt_ifp->if_type)
		{
		case IFT_ETHER:
		  if (sdl->sdl_alen == 0)
		    {
		      sdl->sdl_alen = 6;
		      bcopy(ext->ext_data,LLADDR(sdl),6);
		    }
		  else if (bcmp(LLADDR(sdl),ext->ext_data,6))
		    {
		      DPRINTF(IDL_ERROR,\
			      ("WARNING:  R. adv is saying lladdr is new.\n"));
		    }
#ifdef __FreeBSD__
		  rt->rt_rmx.rmx_expire = time_second;
#else /* __FreeBSD__ */
		  rt->rt_rmx.rmx_expire = time.tv_sec;
#endif /* __FreeBSD__ */
		  dq->dq_unanswered = -1;    /* in STALE state. 
					      (STALE => expired, 
					      but unans = -1. */
		  break;
		default:
		  DPRINTF(IDL_ERROR,\
			  ("I haven't a clue what if_type this is.\n"));
		  break;
		}
	    }
	  break;
	case EXT_PREFIX:
	  {
	    /*
	     * NOTE:  We really should handle this section of code off in
	     *        ipv6_addrconf.c, but I didn't have time to move it
	     *        over.
	     *
	     * 1. If L bit is set, and A bit is not, then simply add an on-link
	     *    route for this prefix, and be done with it.  (If the on-link
	     *    isn't already there, of course!)
	     *    
	     *    But what if I have an i6a that matches this?
	     *
	     * 2. Okay, if A bit is set, then do the following:
	     *    a. If prefix is link-local, bail.
	     *    b. Sanity check that preferred <= valid.  If not, bail.
	     *    c. Check in6_ifaddr list, see if prefix is present.  If so,
	     *       check if autoconf. (i.e. lifetime != 0).  If so, update
	     *       lifetimes. (then bail.)
	     *    d. Okay, so it's not present this means we have to:
	     *       i. If L bit is set:
	     *          - If route present (exact prefix match), delete it.
	     *          - Call in6_control, just like ifconfig(8) does.
	     *          - Look up i6a and set expiration times.
	     *          ELSE (no L-bit)
	     *          - Set up i6a manually (gag me).
	     *          - See if there exists an exact prefix match, if so,
	     *            then change its ifa to the new i6a.
	     */
	    struct inet6_aliasreq ifra;
	    struct sockaddr_in6 *dst = &ifra.ifra_addr,*mask = &ifra.ifra_mask;
	                        
#if 0
	    u_char *cp,*cp2,*cp3;
#else /* 0 */
	    u_char *cp;
#endif /* 0 */
	    int i;

	    /* Sanity check prefix length and lifetimes. */
	    if (pre->pre_prefixsize == 0 || pre->pre_prefixsize >= 128)
	      {
		DPRINTF(IDL_ERROR,("Prefix size failed sanity check.\n"));
		break;
	      }
	    if (ntohl(pre->pre_valid) < ntohl(pre->pre_preferred))
	      {
		DPRINTF(IDL_ERROR,("Lifetimes failed sanity check.\n"));
		break;
	      }

	    /*
	     * Set up dst and mask.
	     */
	    bzero(&ifra,sizeof(ifra));
	    dst->sin6_family = AF_INET6;
	    dst->sin6_len = sizeof(*dst);
	    dst->sin6_addr = pre->pre_prefix;
	    mask->sin6_len = sizeof(*mask) - sizeof(struct in6_addr) + 
	      ((pre->pre_prefixsize - 1) / 8) + 1;
	    cp = (u_char *)&mask->sin6_addr;
	    for (i = 0; i <= ((pre->pre_prefixsize - 1) / 8) ; i ++)
	      cp[i] = 0xff; 
	    cp[--i] <<= (8 - (pre->pre_prefixsize % 8)) % 8;

	    DDO(GROSSEVENT,printf("mask and prefix are:\n");\
		dump_sockaddr_in6(dst);dump_sockaddr_in6(mask));

	    DP(GROSSEVENT, pre->pre_bits, d);

	    if (pre->pre_bits & ICMPV6_PREFIX_AUTO)
	      {
		struct in6_ifaddr *i6a;

		if (IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr)) {
		  DPRINTF(IDL_ERROR, ("ipv6_routeradv_input: Received router advertisement for link-local prefix\n"));
		  break;  /* Bail. */
		};

		DPRINTF(GROSSEVENT, ("ipv6_routeradv_input: Scanning interface addresses for the received prefix\n"));

#if 0
		for (i6a = in6_ifaddr; i6a != NULL; i6a = i6a->i6a_next)
		  if (bcmp(&i6a->i6a_sockmask.sin6_addr,&mask->sin6_addr,
			   sizeof(struct in6_addr)) == 0)
		    {
		      cp = (u_char *)&i6a->i6a_addr.sin6_addr;
		      cp2 = (u_char *)&i6a->i6a_sockmask.sin6_addr;
		      cp3 = (u_char *)&dst->sin6_addr;
		      for (i = 0; (i >=0 && i < sizeof(struct in6_addr)); i++)
			if ((cp[i] ^ cp3[i]) & cp2[i])
			  i = -2;

		      if (i >= 0 && i6a->i6a_expire != 0 &&
			  ((i6a->i6a_addrflags & I6AF_NOTSURE) == 0))
#else /* 0 */
		for (i6a = in6_ifaddr; i6a != NULL; i6a = i6a->i6a_next) {
		  if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_sockmask.sin6_addr, &mask->sin6_addr))
		    continue;
		  if (
(i6a->i6a_addr.sin6_addr.in6a_words[0] ^ dst->sin6_addr.in6a_words[0]) & i6a->i6a_sockmask.sin6_addr.in6a_words[0] ||
(i6a->i6a_addr.sin6_addr.in6a_words[1] ^ dst->sin6_addr.in6a_words[1]) & i6a->i6a_sockmask.sin6_addr.in6a_words[1] ||
(i6a->i6a_addr.sin6_addr.in6a_words[2] ^ dst->sin6_addr.in6a_words[2]) & i6a->i6a_sockmask.sin6_addr.in6a_words[2] ||
(i6a->i6a_addr.sin6_addr.in6a_words[3] ^ dst->sin6_addr.in6a_words[3]) & i6a->i6a_sockmask.sin6_addr.in6a_words[3])
		    continue;

		  if (i6a->i6a_expire && !(i6a->i6a_addrflags & I6AF_NOTSURE))
#endif /* 0 */
			{
			  /*
			   * Found an autoconfigured i6a.
			   *
			   * WARNING:  For now, this path will not attempt to
			   *           do route table repair/updating.
			   *           This means if the link bit was off
			   *           before, but on now, you are in trouble.
			   */
			  DDO(FINISHED, printf("Found i6a of:\n");\
			      dump_ifa((struct ifaddr *)i6a));

			  if (ntohl(pre->pre_preferred) == 0xffffffff)
			    i6a->i6a_preferred = ~0;
#ifdef __FreeBSD__
			  else i6a->i6a_preferred = time_second + 
#else /* __FreeBSD__ */
			  else i6a->i6a_preferred = time.tv_sec + 
#endif /* __FreeBSD__ */
				 ntohl(pre->pre_preferred);

			  if (ntohl(pre->pre_valid) == 0xffffffff)
			    i6a->i6a_expire = ~0;
#ifdef __FreeBSD__
			  else i6a->i6a_expire = time_second + 
#else /* __FreeBSD__ */
			  else i6a->i6a_expire = time.tv_sec + 
#endif /* __FreeBSD__ */
				 ntohl(pre->pre_valid);
			  break;
			}
		    }

		DP(GROSSEVENT, OSDEP_PCAST(i6a), 08x);

		if (i6a == NULL)
		  {
		    struct rtentry *rt;
		    struct socket so;
#if __NetBSD__ || __FreeBSD__
		    struct proc proc;
		    struct pcred pcred;
		    struct ucred ucred;
#endif /* __NetBSD__ || __FreeBSD__ */
		    /*
		     * Need to create new one.
		     */
		    DPRINTF(GROSSEVENT,("Creating new i6a.\n"));

#if __NetBSD__ || __FreeBSD__
		    ucred.cr_uid = 0;
		    proc.p_cred  = &pcred;
	            proc.p_ucred = &ucred;
#else /* __NetBSD__ || __FreeBSD__ */
		    /*
		     * Do in6_control, like ifconfig does.  If L bit is not
		     * set, delete on-link route.
		     */
		    so.so_state = SS_PRIV;
#endif /* __NetBSD__ || __FreeBSD__ */

		    /*
		     * Construct address.
		     */

		    for (i6a = in6_ifaddr; i6a != NULL; i6a = i6a->i6a_next)
		      if (i6a->i6a_ifp == incoming->m_pkthdr.rcvif &&
			  IN6_IS_ADDR_LINKLOCAL(&i6a->i6a_addr.sin6_addr))
			break;
		    if (i6a == NULL)
		      {
			DPRINTF(IDL_ERROR,
				("Can't find link-local for this if!\n"));
			break;
		      }

		    if (i6a->i6a_preflen != pre->pre_prefixsize 
			|| i6a->i6a_preflen == 0)
		      {
			DPRINTF(IDL_ERROR,\
				("Prefix size problems, i6a_preflen = %d, adv. size = %d.\n",\
				 i6a->i6a_preflen,pre->pre_prefixsize));
			break;
		      }

		    prefix_concat(&dst->sin6_addr,&i6a->i6a_addr.sin6_addr,
				  &mask->sin6_addr);

		    if (pre->pre_bits & ICMPV6_PREFIX_ONLINK)
		      {
			/*
			 * If route already exists, delete it.
			 */
#ifdef __FreeBSD__ 
			rt = rtalloc1((struct sockaddr *)dst,0,0UL);
#else /* __FreeBSD__ */
			rt = rtalloc1((struct sockaddr *)dst,0);
#endif /* __FreeBSD__ */
			if (rt != NULL &&
			    !(rt->rt_flags & (RTF_GATEWAY|RTF_TUNNEL)))
			  if ((rt_mask(rt) && bcmp(&mask->sin6_addr,
				 &((struct sockaddr_in6 *)rt_mask(rt))->sin6_addr,
				 sizeof(struct in6_addr)) == 0) ||
			      (!rt_mask(rt) && (bcmp(&mask->sin6_addr, &in6_allones, 
			             sizeof(mask->sin6_addr)) == 0)) )
			  {
			    /*
			     * Making sure the route is THIS ONE is as
			     * simple as checking the masks' equality.
			     */
			    DPRINTF(IDL_EVENT, ("Deleting existing on-link route before autoconfiguring new interface.\n"));
			    rt->rt_refcnt--;
			    rtrequest(RTM_DELETE,rt_key(rt),NULL,NULL,0,NULL);
			  }
			if (rt != NULL)
			  rt->rt_refcnt--;
		      }

#if __NetBSD__ || __FreeBSD__
		    if (in6_control(&so,SIOCAIFADDR_INET6,(caddr_t)&ifra, incoming->m_pkthdr.rcvif,1, &proc))
#else /* __NetBSD__ || __FreeBSD__ */
		    if (in6_control(&so,SIOCAIFADDR_INET6,(caddr_t)&ifra, incoming->m_pkthdr.rcvif,1))
#endif /* __NetBSD__ || __FreeBSD__ */
		      {
			DPRINTF(IDL_ERROR,
				("DANGER:  in6_control failed.\n"));
		      }
		    else if (!(pre->pre_bits & ICMPV6_PREFIX_ONLINK))
		      {
			/*
			 * Router advert didn't specify the prefix as
			 * being all on-link, so nuke the route.
			 */
			rtrequest(RTM_DELETE,(struct sockaddr *)dst,NULL,
				  (struct sockaddr *)mask,0,NULL);
		      }
		    for(i6a = in6_ifaddr;i6a != NULL; i6a = i6a->i6a_next)
		      if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &dst->sin6_addr))
			break;
		    if (i6a != NULL)
		      {
			if (ntohl(pre->pre_preferred) == 0xffffffff)
			  i6a->i6a_preferred = 0xffffffff;
			else i6a->i6a_preferred =
#ifdef __FreeBSD__
			       time_second + ntohl(pre->pre_preferred);
#else /* __FreeBSD__ */
			       time.tv_sec + ntohl(pre->pre_preferred);
#endif /* __FreeBSD__ */
			if (ntohl(pre->pre_valid) == 0xffffffff)
			  i6a->i6a_expire = 0xffffffff;
			else i6a->i6a_expire = 
#ifdef __FreeBSD__
			       time_second + ntohl(pre->pre_valid);
#else /* __FreeBSD__ */
			       time.tv_sec + ntohl(pre->pre_valid);
#endif /* __FreeBSD__ */
		      }
		  }
	      }
	    else if (pre->pre_bits & ICMPV6_PREFIX_ONLINK)
	      {
		/*
		 * Construct on-link prefix route and add.
		 *
		 * WARNING:  According to the discovery document, prefixes
		 *           should be kept in a list, though I only do that
		 *           if I have an address on that list.
		 *           The falllout from this is that prefixes don't have
		 *           their lifetimes enforced.
		 *
		 *           Also, what if I have an i6a for this already?
		 */
		struct sockaddr_dl sdl;
		
		bzero(&sdl,sizeof(sdl));
		sdl.sdl_family = AF_LINK;
		sdl.sdl_len = sizeof(sdl);
		sdl.sdl_index = incoming->m_pkthdr.rcvif->if_index;
		rtrequest(RTM_ADD,(struct sockaddr *)dst,
			  (struct sockaddr *)&sdl, (struct sockaddr *)mask,
			  0,NULL);
	      }
	  }
	  break;
	case EXT_MTU:
	  /*
	   * I'm going to ignore for now.
	   *
	   * Processing would include:
	   *
	   *  1. Possibly change the ifp->if_mtu
	   *  2. Traversing all IPv6 routes with this MTU and updating.
	   *     (This could cause TCP pcb's to be updated too.)
	   */
	  break;
	default:
	  /*
	   * And I quote:
	   *
	   * Future version of this protocol may define new option types.
	   * Receivers MUST silently ignore any options they do not recognize
	   * and continue processing the message.
	   */
	  break;
	}
      (u_long)ext += (ext->ext_length << 3);
    }
}

/*----------------------------------------------------------------------
 * ipv6_new_neighbor():
 * 	Return a new neighbor-cache entry for address sin6 on interface ifp.
 *      A front-end for rtrequest(RTM_ADD, ...).  This returns NULL
 *      if there is a problem.
 * 
 * NOTES:  
 *	May want to handle case of ifp == NULL.  
 *	ipv6_discov_rtrequest() will handle ancillary structure setup.  
 ----------------------------------------------------------------------*/

struct rtentry *
ipv6_new_neighbor(sin6, ifp)
     struct sockaddr_in6 *sin6;   /* Neighbor's IPv6 address. */
     struct ifnet *ifp;           /* Interface neighbor lies off this. */
{
  struct sockaddr_dl lsdl;	/* Target's Link-local Address sockaddr */
  struct sockaddr *dst = (struct sockaddr *)sin6;
  struct sockaddr *gateway = (struct sockaddr *)&lsdl;
  struct sockaddr *netmask = (struct sockaddr *)&in6_allones;
  struct rtentry *newrt = NULL;
  struct ifaddr *ifa;
  int flags = RTF_HOST;

#ifdef __FreeBSD__
  for (ifa = ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
  for (ifa = ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
  for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
    if (ifa->ifa_addr->sa_family == AF_LINK)
      break;
  if (ifa == NULL)
    {
      DPRINTF(IDL_ERROR, 
	      ("ipv6_new_neighbor() can't find AF_LINK for ifp.\n"));
      return NULL;
    }
  
  bcopy(ifa->ifa_addr,&lsdl,min(sizeof(lsdl),ifa->ifa_addr->sa_len));
  lsdl.sdl_alen = 0;
  lsdl.sdl_nlen = 0;
  lsdl.sdl_slen = 0;

#ifdef DIAGNOSTIC
  if (lsdl.sdl_index == 0)
    panic("sdl_index is 0 in ipv6_new_neighbor().");
#endif

  /* ASSUMES:  there is enough room for the link address shoved in here */
  if (rtrequest(RTM_ADD,dst,gateway,netmask,flags,&newrt) == EEXIST) {
    DPRINTF(FINISHED,("Can't add neighbor that already exists?\n"));
    DDO(FINISHED, dump_smart_sockaddr(dst));
    DDO(FINISHED, dump_smart_sockaddr(gateway));
    DDO(FINISHED, dump_smart_sockaddr(netmask));
    DP(FINISHED, flags, d);
  }

  if (newrt != NULL)
    {
      /* Fill in any other goodies, especially MTU. */
      DDO(IDL_EVENT, printf("New route okay, before MTU setup...\n");\
	  dump_rtentry(newrt));
      newrt->rt_rmx.rmx_mtu = newrt->rt_ifp->if_mtu;
      newrt->rt_refcnt = 0; /* XXX - should decrement instead? */
      DDO(IDL_EVENT,printf("New route okay, after MTU setup...\n");\
	  dump_rtentry(newrt));
    }
  return newrt;
}

/*----------------------------------------------------------------------
 * ipv6_neighborsol_input():
 *	Handle input processing for Neighbor Solicit messages.
 *
 ----------------------------------------------------------------------*/
void
ipv6_neighborsol_input(incoming,extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6 *ipv6;
  struct ipv6_icmp *icmp;
  struct rtentry *rt;
  struct discq *dq;
  struct sockaddr_in6 sin6;
  struct sockaddr_dl *sdl;
  struct in6_ifaddr *i6a;
  struct icmp_exthdr *ext = NULL;

  /* Thanks to ipv6_icmp.h, ICMP_NEIGHBORADV_* are already in network order */
  uint32_t flags = (ipv6forwarding) ? ICMPV6_NEIGHBORADV_RTR : 0;

  if (incoming->m_flags & M_DAD)  /* Incoming DAD solicit from me.  Ignore. */
    return;

  /* Verify that length looks OK */
  if (incoming->m_pkthdr.len - extra < ICMPV6_NSOLMINLEN)
    return;

  /* XXX - Assumes that the entire packet fits within MCLBYTES. */
  if (incoming->m_len < incoming->m_pkthdr.len)
    if ((incoming = m_pullup2(incoming, incoming->m_pkthdr.len)))
      return;

  ipv6 = mtod(incoming,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(incoming, caddr_t) + extra);

  /* Check to see hop count is 255. */
  if (ipv6->ipv6_hoplimit != 255)
    { 
      /*
       * Keep stats on this!
       */
      DPRINTF(IDL_ERROR,
	      ("Received Neighbor Solicit with hoplimit != 255.\n"));
      return;
    }

  if (IN6_IS_ADDR_MULTICAST(&icmp->icmp_nsoltarg))
    {
      DPRINTF(IDL_EVENT, ("Received multicast address in solicit!\n"));
      return;
    }
  /*
   * Have a Neighbor Solicit message. 
   */

  /* Verify this is for me.   */
  /* Eventually proxy & anycast checking will go here. */
  for (i6a = in6_ifaddr ; i6a ; i6a = i6a->i6a_next)
    if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &icmp->icmp_nsoltarg))
      break;
  if (i6a == NULL)
    {
      /* Not for me. */
      return;
    }

  if (i6a->i6a_ifp != incoming->m_pkthdr.rcvif)
    {
      DPRINTF(IDL_ERROR,\
	      ("Received off-link Neighbor Solicit for self\n"));
      return;
    }

  /*
   * Can't process solicits for addresses in DAD phase.
   * Furthermore, if solicit comes from all 0's, (and if I made it this far,
   * it's not from me), then there's a duplicate.
   */
  if (i6a->i6a_addrflags & I6AF_NOTSURE)
    {
      if (IN6_IS_ADDR_UNSPECIFIED(&ipv6->ipv6_src))
	{
	  struct socket so;
	  struct inet6_aliasreq ifra;
#if __NetBSD__ || __FreeBSD__
	  struct proc proc;
	  struct pcred pcred;
	  struct ucred ucred;
#endif /* __NetBSD__ || __FreeBSD__ */
	  printf("Duplicate address detected.\n");  /* NEED to print this. */
	  /*
	   * Delete in6_ifaddr.
	   */
#if __NetBSD__ || __FreeBSD__
	  ucred.cr_uid = 0;
	  proc.p_cred  = &pcred;
	  proc.p_ucred = &ucred;
#else /* __NetBSD__ || __FreeBSD__ */
	  so.so_state = SS_PRIV;
#endif /* __NetBSD__ || __FreeBSD__ */
	  strncpy(ifra.ifra_name,i6a->i6a_ifp->if_name,IFNAMSIZ);
	  ifra.ifra_addr = i6a->i6a_addr;
	  ifra.ifra_dstaddr = i6a->i6a_dstaddr;
	  ifra.ifra_mask = i6a->i6a_sockmask;
#if __NetBSD__ || __FreeBSD__
	  in6_control(&so,SIOCDIFADDR_INET6,(caddr_t)&ifra,i6a->i6a_ifp,1, &proc);
#else /* __NetBSD__ || __FreeBSD__ */
	  in6_control(&so,SIOCDIFADDR_INET6,(caddr_t)&ifra,i6a->i6a_ifp,1);
#endif /* __NetBSD__ || __FreeBSD__ */ 
	}
      return;
    }

  /*
   * Create neighbor cache entry for neighbor to send back advertisement.
   */

  if (!IN6_IS_ADDR_UNSPECIFIED(&ipv6->ipv6_src))
    {
      sin6.sin6_family = AF_INET6;
      sin6.sin6_len = sizeof(sin6);
      sin6.sin6_port = 0;
      sin6.sin6_flowinfo = 0;
      sin6.sin6_addr = ipv6->ipv6_src;

#ifdef __FreeBSD__
      rt = rtalloc1((struct sockaddr *)&sin6, 0, 0UL);
#else /* __FreeBSD */
      rt = rtalloc1((struct sockaddr *)&sin6, 0);
#endif /* __FreeBSD */

      /*
       * I just did a passive route lookup.  I'll either get:
       *
       * 1. No route, meaning I have to create one.
       * 2. A host neighbor (RTF_LLINFO) route, meaning I might update it.
       * 3. A host off-link (RTF_GATEWAY) route, meaning I possibly missed a
       *    redirect.
       * 4. An on-link prefix (RTF_CLONING, no RTF_GATEWAY) route, meaning
       *    I have to create one, like no route.
       * 5. The default route (RTF_DEFAULT), meaning the same as no route.
       * 6. A network route, meaning either a subset of that prefix is on-link,
       *    or my routing table is bogus.  I'll create one.
       *
       * In any case I actually get one, I should decrement the rt_refcnt.
       *
       * Future support for RTF_TUNNEL needed here.
       */
      
      DPRINTF(GROSSEVENT,("After rtalloc1().\n"));
      if (rt == NULL  ||  !(rt->rt_flags & RTF_HOST)  )
	{
	  /*
	   * No available host route, create a new entry.
	   */
	  if (rt != NULL)
	    rt->rt_refcnt--;

          DPRINTF(GROSSEVENT,("Creating new neighbor.\n"));
	  rt = ipv6_new_neighbor(&sin6,incoming->m_pkthdr.rcvif);
	  if (rt == NULL)
	    {
	      DPRINTF(IDL_ERROR, 
		      ("Can't allocate soliciting neighbor route.\n"));
	      return;
	    }
	}
      else if (rt->rt_flags & RTF_LLINFO)
	{
	  rt->rt_refcnt--;
	  if (rt->rt_gateway->sa_family != AF_LINK) {
	    DPRINTF(IDL_ERROR, ("LLINFO but gateway != AF_LINK."));
	    return;
	  };
	}
      else
	{
	  /* 
	   * Received Neighbor Solicit from an address which I have
	   * an off-net host route for.  For now, bail.
	   */
	  DPRINTF(FINISHED,
		  ("Received Neighbor Solicit from unknown target.\n"));
	  return;
	}
      
      /*
       * If new, or inactive, set to probe.
       */
      
      /*
       * All of this data will fit in one mbuf as long as the upper limit
       * for ICMP message size <= 576 <= MCLBYTES.
       */
      if (incoming->m_pkthdr.len > extra + ICMPV6_NSOLMINLEN)
	{
	  u_char *data = (u_char *)&icmp->icmp_nsolext;
	  u_char *bounds = data + incoming->m_pkthdr.len - extra -
	    ICMPV6_NSOLMINLEN;

	  /* Only possible extension (so far) in a neighbor advert 
	     is a source link-layer address, but be careful anyway. */

	  ext = (struct icmp_exthdr *)data;
	  while (ext->ext_id != EXT_SOURCELINK && (data<bounds))
	    {
	      DPRINTF(FINISHED,("Got extension other than source link.\n"));
	      data += ext->ext_length<<3;
	      ext = (struct icmp_exthdr *)data;
	    }
	  if (data >= bounds) {
	    DPRINTF(IDL_ERROR, ("couldn't find SOURCELINK"));
	    return;
	  };
	}

      sdl = (struct sockaddr_dl *)rt->rt_gateway;
      rt->rt_flags &= ~RTF_REJECT;
      if (sdl->sdl_alen == 0)  /* New or inactive */
	{
	  if (rt->rt_ifa == NULL)
	    {
	      /*
	       * If multihomed and not sure of interface, take ifp and ifa
	       * for "My destination," where it does not have to be link-
	       * local.
	       */
	      rt->rt_ifa = (struct ifaddr *)i6a;
	      rt->rt_ifp = i6a->i6a_ifp;
	      rt->rt_ifa->ifa_refcnt++;
	      sdl->sdl_index = rt->rt_ifp->if_index;
	      sdl->sdl_type = rt->rt_ifp->if_type;
	      sdl->sdl_nlen = strlen(rt->rt_ifp->if_name);
	      bcopy(rt->rt_ifp->if_name,sdl->sdl_data,sdl->sdl_nlen);
	      rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
	    }

#ifdef __FreeBSD__
	  rt->rt_rmx.rmx_expire = time_second;
#else /* __FreeBSD__ */
	  rt->rt_rmx.rmx_expire = time.tv_sec;
#endif /* __FreeBSD__ */
	  dq = (struct discq *)rt->rt_llinfo;
	  dq->dq_unanswered = -1;  /* In STALE state */
	  /*
	   * Checks for non-broadcast multiple-access (NBMA) links
	   * such as PPP, Frame Relay, and ATM are probably needed here.
	   */
	  if (ext != NULL)
	    switch (rt->rt_ifp->if_type)
	      {
	      case IFT_ETHER:
		sdl->sdl_alen = 6;  
		bcopy(ext->ext_data,LLADDR(sdl),sdl->sdl_alen);
		break;
	      default:
		DPRINTF(IDL_ERROR,("DANGER:  Non-ethernet n. adv.\n"));
		break;
	      }
	}
      else
	{
	  /*
	   * For now, ignore if I already have somewhat valid entry.
	   * Only adverts can affect changes on PROBE or STALE entries.
	   * (And then, only maybe.)
	   */
	}

      /*
       * Neighbor Cache is now updated.  
       * Now, send out my unicast advertisement.
       *
       * NB: ICMPV6_* symbol was already htonl()'d in the header file.
       */
      flags |= ICMPV6_NEIGHBORADV_SOL|ICMPV6_NEIGHBORADV_OVERRIDE;
      ipv6_nadvert(i6a,i6a->i6a_ifp,rt,flags);
    }
  else
    {
      /*
       * Send multicast advertisement rather than unicast advertisement
       * because the solicit contained the unspecified address.
       *
       * DAD code may need to be executed in here as well.
       */
      ipv6_nadvert(i6a,incoming->m_pkthdr.rcvif,NULL,flags);
    }
}


/*----------------------------------------------------------------------
 * ipv6_neighboradv_input():
 *	Handle reception of a Neighbor Advertisement message.
 *
 ----------------------------------------------------------------------*/
void
ipv6_neighboradv_input(incoming,extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6 *ipv6;
  struct ipv6_icmp *icmp;
  struct rtentry *rt;
  struct sockaddr_in6 sin6;
  struct in6_ifaddr *i6a;
  int s;

  /* Verify that incoming length looks plausible */
  if (incoming->m_pkthdr.len - extra < ICMPV6_NADVMINLEN)
    return;

  /* XXX - Assumes that the entire packet fits within MCLBYTES. */
  if (incoming->m_len < incoming->m_pkthdr.len)
    if ((incoming = m_pullup2(incoming, incoming->m_pkthdr.len)))
      return;

  ipv6 = mtod(incoming,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(incoming, caddr_t) + extra);

  /* Verify hoplimit == 255 */
  if (ipv6->ipv6_hoplimit != 255)
    {
      /*
       * Keep stats on this!
       */
      DPRINTF(IDL_ERROR,
	      ("Received Neighbor Solicit with hoplimit != 255.\n"));
      return;
    }

#if 0
  /* Verify quickly src/node matching. Causes way too many false alarms. */
  if (!IN6_ARE_ADDR_EQUAL(&ipv6->ipv6_src, &icmp->icmp_nadvaddr))
    {
      DPRINTF(IDL_ERROR, ("WARNING:  Possible proxy, addrs are unequal.\n"));
      DPRINTF(IDL_ERROR, ("src=%08x %08x %08x %08x\n", ntohl(ipv6->ipv6_src.in6a_u.words[0]), ntohl(ipv6->ipv6_src.in6a_u.words[1]), ntohl(ipv6->ipv6_src.in6a_u.words[2]), ntohl(ipv6->ipv6_src.in6a_u.words[3])));
      DPRINTF(IDL_ERROR, ("nadvaddr=%08x %08x %08x %08x\n", ntohl(icmp->icmp_nadvaddr.in6a_u.words[0]), ntohl(icmp->icmp_nadvaddr.in6a_u.words[1]), ntohl(icmp->icmp_nadvaddr.in6a_u.words[2]), ntohl(icmp->icmp_nadvaddr.in6a_u.words[3])));
    }
#endif /* 0 */

  if (IN6_IS_ADDR_MULTICAST(&icmp->icmp_nadvaddr))
    {
      DPRINTF(ERROR, ("Received Neighbor Advert with multicast address.\n"));
      return;  /* For now... */
    }

  /*
   * Have a Neighbor Advertisement.
   */

  s = splnet();

  /* Look to see if it's for one of my addresses. */
  for (i6a = in6_ifaddr ; i6a ; i6a = i6a->i6a_next)
    if (IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &icmp->icmp_nadvaddr))
      break;
  if (i6a != NULL)
    {
      if (i6a->i6a_addrflags & I6AF_NOTSURE)
	{
	  struct socket so;
	  struct inet6_aliasreq ifra;
#if __NetBSD__ || __FreeBSD__
	  struct proc proc;
	  struct pcred pcred;
	  struct ucred ucred;
#endif /* __NetBSD__ || __FreeBSD__ */
	  printf("Duplicate address detected.\n");  /* NEED to print this. */
	  /*
	   * Delete in6_ifaddr.
	   */
#if __NetBSD__ || __FreeBSD__
	  ucred.cr_uid = 0;
	  proc.p_cred  = &pcred;
	  proc.p_ucred = &ucred;
#else /* __NetBSD__ || __FreeBSD__ */
	  so.so_state = SS_PRIV;
#endif /* __NetBSD__ || __FreeBSD__ */
	  strncpy(ifra.ifra_name,i6a->i6a_ifp->if_name,IFNAMSIZ);
	  ifra.ifra_addr = i6a->i6a_addr;
	  ifra.ifra_dstaddr = i6a->i6a_dstaddr;
	  ifra.ifra_mask = i6a->i6a_sockmask;
#if __NetBSD__ || __FreeBSD__
	  in6_control(&so,SIOCDIFADDR_INET6,(caddr_t)&ifra,i6a->i6a_ifp,1, &proc);
#else /* __NetBSD__ || __FreeBSD__ */
	  in6_control(&so,SIOCDIFADDR_INET6,(caddr_t)&ifra,i6a->i6a_ifp,1);
#endif /* __NetBSD__ || __FreeBSD__ */
	}
      else
	{
	  /* For now, ignore advert which is for me. */
	}

      splx(s);
      return;
    }

  /* Lookup and see if I have something waiting for it... */
  sin6.sin6_len = sizeof(sin6);
  sin6.sin6_family = AF_INET6;
  sin6.sin6_addr = icmp->icmp_nadvaddr;

  /* Next 2 lines might not be strictly needed since this is an rtalloc, 
     but they're included to be safe.  */
  sin6.sin6_port = 0;
  sin6.sin6_flowinfo = 0;

#ifdef __FreeBSD__
  rt = rtalloc1((struct sockaddr *)&sin6,0,0UL);
#else /* __FreeBSD__ */
  rt = rtalloc1((struct sockaddr *)&sin6,0);
#endif /* __FreeBSD__ */

  if (rt != NULL)
    rt->rt_refcnt--;

  /*
   * After passive route lookup, I have either:
   *
   * 1. No route, drop the advert.
   * 2. Route with no RTF_HOST, drop the advert.
   * 3. Route with no RTF_LLINFO, for now, drop the advert, this could be
   *    a redirect weirdness?
   * 4. Route with RTF_LLINFO, try and update.
   */

  if (rt == NULL || !(rt->rt_flags & RTF_HOST))   
    {
      /* Cases 1 and 2. */
      splx(s);
      return;
    }

  if (rt->rt_flags & RTF_LLINFO)  
    {
      /* Case 4. */
      struct sockaddr_dl *sdl;
      struct icmp_exthdr *liext = NULL;

      /*
       * Possibly update the link-layer address, and maybe change to
       * REACHABLE state.
       */
      rt->rt_flags &= ~RTF_REJECT;  /* That neighbor talked to me! */

      if (incoming->m_pkthdr.len - extra > ICMPV6_NADVMINLEN)
	{
	  u_char *data = (u_char *)&icmp->icmp_nadvext;
	  u_char *bounds = data + incoming->m_pkthdr.len - extra -
	    ICMPV6_NADVMINLEN;
	  struct icmp_exthdr *ext = (struct icmp_exthdr *)data;
	  
	  /* Only possible extension (so far) in a neighbor advert is a 
	     source link-layer address, but be careful anyway. */

	  while (ext->ext_id != EXT_TARGETLINK && (data < bounds))
	    {
	      DPRINTF(IDL_EVENT,("Got extension other than source link.\n"));
	      data += ext->ext_length<<3;
	      ext = (struct icmp_exthdr *)data;
	    }
	  if (data >= bounds) {
	    DPRINTF(IDL_ERROR,("Received neighbor advertisement with no source link layer address.\n"));
	    splx(s);
	    return;
	  }
	  liext = ext;
	}

      sdl = (struct sockaddr_dl *)rt->rt_gateway;

      if (liext != NULL)
	{
	  if (rt->rt_ifa == NULL)
	    {
	      struct in6_ifaddr *i6a;

	      /*
	       * ifa and ifp for this address are null, because it's an
	       * on-link brute force discovery.  Use link-local as source.
	       */
	      rt->rt_ifp = incoming->m_pkthdr.rcvif;
	      for (i6a = in6_ifaddr ; i6a != NULL; i6a = i6a->i6a_next)
		if (i6a->i6a_ifp == rt->rt_ifp &&
		    (i6a->i6a_flags & I6AF_LINKLOC))
		  break;

	      if (i6a == NULL)
		{
		  DPRINTF(IDL_ERROR,
			  ("Got advert from interface with no link-local.\n"));
		  splx(s);
		  return;
		}
	      rt->rt_ifa = (struct ifaddr *)i6a;
	      rt->rt_ifa->ifa_refcnt++;	
	      sdl->sdl_index = rt->rt_ifp->if_index;
	      sdl->sdl_type = rt->rt_ifp->if_type;
	      sdl->sdl_nlen = strlen(rt->rt_ifp->if_name);
	      bcopy(rt->rt_ifp->if_name,sdl->sdl_data,sdl->sdl_nlen);
	      rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
	    }
	  
	  /*
	   * Update neighbor cache link address.
	   * Should make this code more general than just for Ethernet,
	   * so we switch off the interface type assigned to the route.
	   */
	  switch (rt->rt_ifp->if_type)
	    {
	    case IFT_ETHER:
	      if (sdl->sdl_alen == 0 ||
		  (icmp->icmp_nadvbits & ICMPV6_NEIGHBORADV_OVERRIDE))
		{
		  sdl->sdl_alen = 6;
		  bcopy(&liext->ext_data,LLADDR(sdl),sdl->sdl_alen);
		}
	      else if (bcmp(LLADDR(sdl),&liext->ext_data,6))
		{
		  /*
		   * Override bit not set, and have link address already.
		   * Discard.
		   */
		  DPRINTF(IDL_ERROR,("Danger, got non-override with different link-local address.\n"));
                  splx(s);
		  return;
		}
	      break;
	    }
	  /* The ICMP_NEIGHBORADV_* bits are already machine-specific.  
	     So no HTONLs/NTOHLs need to be done here.  */
	  
	  /* Now in REACHABLE or STALE state, depending on
	     ICMP_NEIGHBORADV_SOL bit. */
	  {
	    struct discq *dq = (struct discq *)rt->rt_llinfo;
	    
#ifdef __FreeBSD__
	    rt->rt_rmx.rmx_expire = time_second + 
#else /* __FreeBSD__ */
	    rt->rt_rmx.rmx_expire = time.tv_sec + 
#endif /* __FreeBSD__ */
	      ((icmp->icmp_nadvbits & ICMPV6_NEIGHBORADV_SOL)?v6d_reachtime:0);
	    dq->dq_unanswered = -1;
	    if (dq->dq_queue != NULL)
	      {
		rt->rt_ifp->if_output(rt->rt_ifp,dq->dq_queue, rt_key(rt),
				      rt);
		dq->dq_queue = NULL;
	      }
	  }
	}

      /*
       * Check for routers becoming hosts, and vice-versa.
       */
      if (icmp->icmp_nadvbits & ICMPV6_NEIGHBORADV_RTR)
	rt->rt_flags |= RTF_ISAROUTER;
      else if (rt->rt_flags & RTF_ISAROUTER)
	{
	  /*
	   * Deal with router becoming host.
	   */
	}
    }
  else
    {
      /* Case 3. */
      /* Should consider adding a counter for these */
      DPRINTF(ERROR, 
	      ("Received Neighbor Advert for off-link host.\n"));
    }
  splx(s);
}

/*----------------------------------------------------------------------
 * ipv6_redirect_input():
 *    Handle reception of a Redirect message.
 *
 ----------------------------------------------------------------------*/
void
ipv6_redirect_input(incoming,extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6 *ipv6;
  struct ipv6_icmp *icmp;
  struct rtentry *rt;
  struct sockaddr_in6 dst6, gate6, src6;
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
  extern int icmp_redirtimeout;
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */

  /* XXX - Assumes that the entire packet fits within MCLBYTES. */
  if (incoming->m_len < incoming->m_pkthdr.len)
    if ((incoming = m_pullup2(incoming, incoming->m_pkthdr.len)))
      return;

  ipv6 = mtod(incoming,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(mtod(incoming, caddr_t) + extra);

  /* Verify source address is link-local */
  if (!IN6_IS_ADDR_LINKLOCAL(&ipv6->ipv6_src)) {
    DPRINTF(IDL_ERROR, ("Received Redirect with non link-local source address.\n"));
    return;
  };

  /* Verify hoplimit == 255 */
  if (ipv6->ipv6_hoplimit != 255) {
    /*
     * Keep stats on this!
     */
    DPRINTF(IDL_ERROR, ("Received IPv6 Redirect with hoplimit != 255.\n"));
    return;
  };

  /* Verify that incoming length looks plausible */
  if (incoming->m_pkthdr.len - extra < ICMPV6_REDIRMINLEN) {
    DPRINTF(IDL_ERROR, ("Received IPv6 Redirect without enough data.\n"));
    return;
  };

  /* Verify current next hop == IPv6 src addr */
  bzero(&dst6, sizeof(struct sockaddr_in6));
  dst6.sin6_len = sizeof(struct sockaddr_in6);
  dst6.sin6_family = AF_INET6;
  dst6.sin6_addr = icmp->icmp_redirdest;

#ifdef __FreeBSD__
  if (!(rt = rtalloc1((struct sockaddr *)&dst6, 0, 0UL))) {
#else /* __FreeBSD__ */
  if (!(rt = rtalloc1((struct sockaddr *)&dst6, 0))) {
#endif /* __FreeBSD__ */
    DPRINTF(IDL_ERROR, ("Received IPv6 Redirect for unreachable host.\n"));
    return;
  };

  if ((rt->rt_gateway->sa_family != AF_INET6) || !IN6_ARE_ADDR_EQUAL(&ipv6->ipv6_src, &((struct sockaddr_in6 *)rt->rt_gateway)->sin6_addr)) {
    DPRINTF(IDL_ERROR, ("Received IPv6 Redirect from wrong source.\n"));
    return;
  };

  /* No redirects for multicast packets! */
  if (IN6_IS_ADDR_MULTICAST(&icmp->icmp_redirdest)) {
    DPRINTF(IDL_ERROR, ("Received Redirect with multicast address.\n"));
    return;  /* For now... (?) */
  };

  /* Target must be link local, or same as destination */
  if (!IN6_IS_ADDR_LINKLOCAL(&icmp->icmp_redirtarg) && !IN6_ARE_ADDR_EQUAL(&icmp->icmp_redirtarg, &icmp->icmp_redirdest)) {
    DPRINTF(IDL_ERROR, ("Received Redirect with non link-local target addr != dest addr.\n"));
    return;
  };

  /*
   * We have a valid Redirect.
   */

  bzero(&gate6, sizeof(struct sockaddr_in6));
  gate6.sin6_len = sizeof(struct sockaddr_in6);
  gate6.sin6_family = AF_INET6;
  gate6.sin6_addr = icmp->icmp_redirtarg;

  bzero(&src6, sizeof(struct sockaddr_in6));
  src6.sin6_len = sizeof(struct sockaddr_in6);
  src6.sin6_family = AF_INET6;
  src6.sin6_addr = ipv6->ipv6_src;

  rtredirect((struct sockaddr *)&dst6, (struct sockaddr *)&gate6,
	     NULL, RTF_DONE|RTF_GATEWAY|RTF_HOST, (struct sockaddr *)&src6,
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
             icmp_redirtimeout);
#else /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
	     (struct rtentry **)0);
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
  return;
};

/*----------------------------------------------------------------------
 * ipv6_discov_resolve(): 
 *      Called by LAN output routine.  Will either queue outgoing packet
 *   on the discovery structure that hangs off the route and return 0,
 *   OR copies LAN MAC address into edst and returns 1.
 *
 * NOTES:
 *    Neighbor Unreachable Detection (NUD) should be done here.
 *    LAN output routine should handle IPv6 multicast -> link multicast
 *    mapping, unlike arp_resolve, this function doesn't handle multicast.
 ----------------------------------------------------------------------*/

int ipv6_discov_resolve(ifp, rt, outgoing, dst, edst)
     struct ifnet *ifp;
     struct rtentry *rt;
     struct mbuf *outgoing;
     struct sockaddr *dst;
     u_char *edst;
{
  struct discq       *dq  = NULL;
  struct sockaddr_dl *sdl;
  struct ipv6        *ipv6;

  DPRINTF(IDL_EVENT,("Entering ipv6_discov_resolve().\n"));
  if (rt)
    dq = (struct discq *)rt->rt_llinfo;
  else
    {
#ifdef __FreeBSD__
      rt = rtalloc1(dst, 0, 0UL);
#else /* __FreeBSD__ */
      rt = rtalloc1(dst, 0);
#endif /* __FreeBSD__ */
      
      /*
       * I just did a passive route lookup.  I'll either get:
       *
       * 1. No route, meaning I have to create one.
       * 2. A host neighbor (RTF_LLINFO) route, meaning I'm good to go.
       * 3. A host off-link route, meaning I possibly missed a redirect.
       * 4. An on-link prefix (RTF_CLONING, no RTF_GATEWAY) route, meaning
       *    I have to create one, like no route.
       * 5. The default route (RTF_DEFAULT), meaning the same as no route.
       * 6. A network route, meaning either a subset of that prefix is on-link,
       *    or my routing table is invalid.  I'll create a new route entry.
       */
      
      if (rt == NULL  ||  !(rt->rt_flags & RTF_HOST))
	{
	  /*
	   * No available host route, create a new entry.
	   * (Cases 1, 4, 5, 6.)
	   */
	  if (rt != NULL)
	    rt->rt_refcnt--;
	  
	  rt = ipv6_new_neighbor((struct sockaddr_in6 *)dst,ifp);
	  if (rt == NULL)
	    {
	      DPRINTF(IDL_ERROR, 
		      ("Can't allocate soliciting neighbor route.\n"));
	      m_freem(outgoing);
	      return 0;
	    }
	}
      else if (rt->rt_flags & RTF_LLINFO)
	{
	  /* (Case 2) */
	  rt->rt_refcnt--;
	  if (rt->rt_gateway->sa_family != AF_LINK) {
	    DPRINTF(IDL_ERROR, ("LLINFO but gateway != AF_LINK."));
	    m_freem(outgoing);
	    return 0;
	  }
	}
      else
	{
	  /* 
	   * I just got a neighbor solicit from an address which I have
	   * an off-net host route for.  For now, bail.  (Case 3.)
	   */
	  DPRINTF(ERROR,
		  ("Received Neighbor Solicit from unknown target.\n"));
	  return 0;
	}

      dq = (struct discq *)rt->rt_llinfo;
    }

  if (dq == NULL)
    {
      DPRINTF(IDL_ERROR, ("No discq structure hanging off route.\n"));
      m_freem(outgoing);
      return 0;
    }
  if (dq->dq_rt != rt) {
    DPRINTF(IDL_ERROR, ("discov_resolve route passed in (rt) != dq->dq_rt\n"));
    m_freem(outgoing);
    return 0;
  };

  sdl = (struct sockaddr_dl *)rt->rt_gateway;
  if (sdl->sdl_family != AF_LINK) {
    DPRINTF(IDL_ERROR, ("ipv6_discov_resolve called with rt->rt_gateway->sa_family == %d.\n", sdl->sdl_family));
    m_freem(outgoing);
    return 0;
  };

  if (sdl->sdl_alen == 0)
    {
      /*
       * I'm in INCOMPLETE mode or a new entry.
       *
       * Also, if this is a LINK-LOCAL address, (or there is some other 
       * reason that it isn't clear which interface the address is on)
       * I might want to send the solicit out all interfaces.
       */

      rt->rt_flags &= ~RTF_REJECT;  /* Clear RTF_REJECT in case LAN output
				       routine caught expiration before
				       the timer did. */
      ipv6_nsolicit(ifp,outgoing,rt);
      return 0;
    }

#ifdef __FreeBSD__
  if (dq->dq_unanswered < 0 && time_second >= rt->rt_rmx.rmx_expire)
#else /* __FreeBSD__ */
  if (dq->dq_unanswered < 0 && time.tv_sec >= rt->rt_rmx.rmx_expire)
#endif /* __FreeBSD__ */
    {
      /*
       * Timeout on REACHABLE entry.  Process accordingly.
       * Change the timed out REACHABLE entry into PROBE state.
       * ( REACHABLE -> PROBE )
       * PROBE state handling is the job of ipv6_discovery_timer().
       */
#ifdef __FreeBSD__
      rt->rt_rmx.rmx_expire = time_second + v6d_delfirstprobe;
#else /* __FreeBSD__ */
      rt->rt_rmx.rmx_expire = time.tv_sec + v6d_delfirstprobe;
#endif /* __FreeBSD__ */
      dq->dq_unanswered = 0;
      ipv6 = mtod(outgoing,struct ipv6 *);
    }

  DPRINTF(GROSSEVENT,("ipv6_discov_resolve() returning 1.\n"));
  /*
   * Right now, just trust sdl is set up right.  May need to change this
   * later.
   */
  bcopy(LLADDR(sdl),edst, sdl->sdl_alen);
  return 1;
}


/*----------------------------------------------------------------------
 * tunnel_parent():
 * 	Set up tunnel state for a network (cloning?) tunnel route.
 * Right now, there is no tunnel state unless an IPv6 secure tunnel
 * (rt->rt_gateway->sa_family == AF_INET6 && (RTF_CRYPT || RTF_AUTH))
 ----------------------------------------------------------------------*/

void tunnel_parent(rt)
     register struct rtentry *rt;
{
  struct rtentry *chaser = rt;

  DPRINTF(GROSSEVENT,("ipv6_tunnel_parent():0000-Starting.\n"));
  DDO(GROSSEVENT,printf("  rt_flags = 0x%x\n",(unsigned int)rt->rt_flags));

  /*
   * For now, set up master tunnel MTU.  Chase rt_gwroute until no more, and
   * see if there is either rmx_mtu or ifp->mtu to transfer.  This should
   * work on both cloning and non-cloning tunnel routes.
   *
   * Q:  Do I want to chase it all the way?  Or just to the next one?
   * A:  For now go to the next one.  Change the following "if" to a
   *     "while" if you want to switch.
   *
   * Q2: For non-gateway tunnels (i.e. node-to-host tunnels), I may
   *     need to undo some braindamage.  How?
   */

  /* Change "if" to "while" for all-the-way chasing. */
  while (chaser->rt_gwroute != NULL)   
    chaser = chaser->rt_gwroute;

  DDO(GROSSEVENT,printf("Last route in gwroute chain is:\n");\
      dump_rtentry(rt));

  if (chaser == rt)
    {
      /*
       * If non-gateway tunnel, find a route for the gateway address.
       */
#ifdef __FreeBSD__
      chaser = rtalloc1(rt->rt_gateway,0,0UL);
#else /* __FreeBSD__ */
      chaser = rtalloc1(rt->rt_gateway,0);
#endif /* __FreeBSD__ */
      if (chaser == NULL)
	/*
	 * Oooh boy, you're on your own, kid!
	 */
	chaser = rt;
      else
	{
	  chaser->rt_refcnt--;
	  /* else do I want to do that while loop again? */
	}
    }

  if (chaser->rt_rmx.rmx_mtu != 0)
    {
      DPRINTF(GROSSEVENT,("Chaser's route MTU (%d) is set.\n",\
			       (int)chaser->rt_rmx.rmx_mtu));
      rt->rt_rmx.rmx_mtu = chaser->rt_rmx.rmx_mtu;
    }
  else
    {
      DPRINTF(GROSSEVENT,("Chaser's route MTU is not set.  "));
      DPRINTF(GROSSEVENT,("Attempting ifp check.\n"));
      if (chaser->rt_ifp == NULL)
	{
	  DPRINTF(IDL_ERROR,\
		  ("Can't find ifp.  Using IPV6_MINMTU (%d).\n",IPV6_MINMTU));
	  rt->rt_rmx.rmx_mtu = IPV6_MINMTU;
	}
      else
	{
	  DPRINTF(FINISHED,("Found ifp with mtu of (%d).\n",\
				   (int)chaser->rt_ifp->if_mtu));
	  rt->rt_rmx.rmx_mtu = chaser->rt_ifp->if_mtu;
	}
    }

  if (chaser->rt_ifp != rt->rt_ifp)
    {
      /*
       * Somewhere along the way, things got messed up.
       * (IPv4 tends to confuse ifa_ifwithroute(), and loopback happens.)
       *
       * For tunnels, set the rt_ifp to the interface the chaser finds.
       * Hopefully ipv6_setrtifa() will do the right thing with the ifa.
       */
      rt->rt_ifp = chaser->rt_ifp;
    }

  /*
   * Adjust based on any known encapsulations:
   */

  /* Use the rt->rt_gateway->sa_family to determine kind of tunnel */
  if (rt->rt_gateway->sa_family == AF_INET)  /* v6 in v4 tunnel */
    {
      /* WARNING:  v6-in-v4 tunnel may also have secure bits set. */
      rt->rt_rmx.rmx_mtu -= sizeof(struct ip);
    }
  else if (rt->rt_gateway->sa_family == AF_INET6)
    {
      /* Perhaps need to ensure that the rt_gateway is a neighbor. */
      rt->rt_rmx.rmx_mtu -= sizeof(struct ipv6);
    }

#ifdef IPSEC
  if (rt->rt_flags & (RTF_CRYPT|RTF_AUTH))
    {
      struct socket *so;
      
      DPRINTF(GROSSEVENT,("tunnel_parent: Setting up secure tunnel state.\n"));

      if (!(rt->rt_netproc = malloc(sizeof(*so), M_SOCKET, M_NOWAIT))) {
	DPRINTF(IDL_ERROR, ("tunnel_parent: can't allocate fake socket\n"));
	return;
      }

      so = (struct socket *)rt->rt_netproc;
      bzero(so, sizeof(*so));
      so->so_oobmark = 1; /* really the refcount */

      if (netproc_alloc(so)) {
	DPRINTF(IDL_ERROR, ("tunnel_parent: can't allocate netproc state\n"));
	return;
      }

      {
	struct netproc_requestandstate *r =
	  &((struct netproc_socketdata *)so->so_netproc)->requests[0];
	void *p;
	
	r->requestlen = ((rt->rt_flags & RTF_AUTH) ? sizeof(fixedauth) : 0)
	  + ((rt->rt_flags & RTF_CRYPT) ? sizeof(fixedencrypt) : 0);
	
	if (!(r->request = malloc(r->requestlen, M_SOOPTS, M_NOWAIT))) {
	  DPRINTF(IDL_ERROR,("tunnel_child: can't allocate netproc request\n"));
	  r->requestlen = 0;
	  return;
	}

	p = r->request;

	/* XXX - should be determined at runtime */
	if (rt->rt_flags & RTF_AUTH) {
	  memcpy(p, &fixedauth, sizeof(fixedauth));
	  p += sizeof(fixedauth);
	  rt->rt_rmx.rmx_mtu -= IPSEC_AH_WORSTPREOVERHEAD +
	    IPSEC_AH_WORSTPOSTOVERHEAD;
	}

	if (rt->rt_flags & RTF_CRYPT) {
	  memcpy(p, &fixedencrypt, sizeof(fixedencrypt));
          rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPREOVERHEAD;
	  if (rt->rt_flags & RTF_AUTH)
	    rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPOSTOVERHEAD_PLAIN;
	  else
	    rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPOSTOVERHEAD_COMBINED;
	}
      }
    }
#endif /* IPSEC */
  DPRINTF(GROSSEVENT,("ipv6_tunnel_parent():1000-Done.\n"));
}

/*----------------------------------------------------------------------
 * tunnel_parent_clean():
 *	Frees dummy socket of secure tunnel.
 *  Any other tunnel state should also be cleaned up here for the parent.
 * NOTE:  Children won't be cleaned up.
 ----------------------------------------------------------------------*/
void tunnel_parent_clean(rt)
     register struct rtentry *rt;
{
  DPRINTF(GROSSEVENT,("tunnel_parent_clean():0000-Starting.\n"));
#ifdef IPSEC
  if (rt->rt_flags & (RTF_CRYPT|RTF_AUTH))
    {
      struct socket *so = (struct socket *)rt->rt_netproc;

      DPRINTF(GROSSEVENT,("tunnel_parent_clean():0500-It's RTF_CRYPT|RTF_AUTH; now removing.\n"));
      if (so == NULL)
	{
	  DPRINTF(IDL_ERROR,("WARNING: Secure tunnel w/o dummy socket.\n"));
	  return;
	}

      if (so->so_netproc)
	if (--so->so_oobmark)  /* Refcnt != 0 */
	  return;
	else
	  netproc_free(so);

      free(so, M_SOCKET);
      rt->rt_netproc = NULL;
    }
#endif /* IPSEC */
  DPRINTF(GROSSEVENT,("tunnel_parent_clean():1000-Finished.\n"));
}

/*----------------------------------------------------------------------
 * tunnel_child():
 * 	Set up tunnel state for a host or cloned tunnel route.
 * Right now, there is no tunnel state except for secure tunnels
 * ((RTF_CRYPT || RTF_AUTH))
 ----------------------------------------------------------------------*/

void tunnel_child(rt)
     register struct rtentry *rt;
{
  struct sockaddr_in6 *dst = (struct sockaddr_in6 *)rt_key(rt);
  struct sockaddr_in *sin;

  DPRINTF(GROSSEVENT,("tunnel_child():0000-Starting.\n"));

  /* Turn off STATIC flag only if cloned. */
  if (rt->rt_parent != NULL)
    rt->rt_flags &= ~RTF_STATIC;

  /* 
   * If additional tunnel state were needed, it could be hung off rt_llinfo.
   * August 14, 1996:  Actually, we're now using rt_tunsec to hold the
   *                   tunnel security state information.   If we need to
   *                   carry more state information, we'll have to rework
   *                   this.
   */

  if (!(rt->rt_flags & RTF_GATEWAY) && rt->rt_parent != NULL)
    {
      /*
       * If not a gateway route, and cloned, I'll have to do some transforms.
       */
      switch (rt->rt_gateway->sa_family)
	{
	case AF_INET:
	  if (IN6_IS_ADDR_V4COMPAT(&dst->sin6_addr))
	    {
	      /* Create new self-tunneling IPv6 over IPv4 route. */
	      /* DANGER:  If original setgate doesn't work properly, this
		 could be trouble. */
	      sin = (struct sockaddr_in *)rt->rt_gateway;
	      sin->sin_addr.s_addr = dst->sin6_addr.in6a_words[3];
	      if (rt_setgate(rt,rt_key(rt),rt->rt_gateway))
		{
		  DPRINTF(GROSSEVENT, 
			  ("rt_setgate failed in tunnel_child().\n"));
		  rt->rt_rmx.rmx_mtu = 0;
		  /* More should probably be done here. */
		}
	    }
	  /* else we're in BIG trouble. */
	  break;
	}
    }

#ifdef IPSEC
  if (rt->rt_flags & (RTF_CRYPT|RTF_AUTH))
    {
      u_long flagsformtu;

      DPRINTF(GROSSEVENT,("tunnel_child():0300-Setting up secure host tunnel route.\n"));
      if (rt->rt_parent == NULL)
	{
	  struct socket *so;
	  
          DPRINTF(GROSSEVENT,("tunnel_child():400-Need to create a fresh socket.\n"));

	  if (!(rt->rt_netproc = malloc(sizeof(*so), M_SOCKET, M_NOWAIT))) {
	    DPRINTF(IDL_ERROR, ("tunnel_child: can't allocate fake socket\n"));
	    return;
	  }

	  so = (struct socket *)rt->rt_netproc;
	  bzero(so, sizeof(*so));
	  so->so_oobmark = 1; /* really the refcount */

	  if (netproc_alloc(so)) {
	    DPRINTF(IDL_ERROR, ("tunnel_child: can't allocate netproc state\n"));
	    return;
	  }

	  {
	    struct netproc_requestandstate *r =
	      &((struct netproc_socketdata *)so->so_netproc)->requests[0];
	    void *p;

	    r->requestlen = ((rt->rt_flags & RTF_AUTH) ? sizeof(fixedauth) : 0)
	      + ((rt->rt_flags & RTF_CRYPT) ? sizeof(fixedencrypt) : 0);

	    if (!(r->request = malloc(r->requestlen, M_SOOPTS, M_NOWAIT))) {
	      DPRINTF(IDL_ERROR,("tunnel_child: can't allocate netproc request\n"));
	      r->requestlen = 0;
	      return;
	    }

	    p = r->request;

	    if (rt->rt_flags & RTF_AUTH) {
	      memcpy(p, &fixedauth, sizeof(fixedauth));
	      p += sizeof(fixedauth);
	    }

	    if (rt->rt_flags & RTF_CRYPT)
	      memcpy(p, &fixedencrypt, sizeof(fixedencrypt));
	  }

	  flagsformtu = rt->rt_flags;
     } else {
       struct socket *so = (struct socket *)rt->rt_parent->rt_netproc;
	  
       DPRINTF(GROSSEVENT,("tunnel_child: Using parent's socket.\n"));
       if (!so) {
	 DPRINTF(ERROR, ("tunnel_child: No socket in parent!\n"));
	 return;
       }

       rt->rt_netproc = so;

       /* XXX - Bump refcount. This will break if you have more than about
	  four million children, but you'll have bigger problems way before
	  that ever happens - cmetz */
       so->so_oobmark++;

       flagsformtu = rt->rt_parent->rt_flags;
     }

      /* XXX - This block of code was a reasonable hack at the time, but isn't
	 a good idea anymore. - cmetz */
      /* Security Level should be more configurable, possibly using
	 a sysctl or by calling the IPsec security policy engine */
      if (flagsformtu & RTF_AUTH)
	rt->rt_rmx.rmx_mtu -= IPSEC_AH_WORSTPREOVERHEAD +
	  IPSEC_AH_WORSTPOSTOVERHEAD;
      if (flagsformtu & RTF_CRYPT) {
	rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPREOVERHEAD;
	if (rt->rt_flags & RTF_AUTH)
	  rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPOSTOVERHEAD_PLAIN;
	else
	  rt->rt_rmx.rmx_mtu -= IPSEC_ESP_WORSTPOSTOVERHEAD_COMBINED;
      }
    }
#endif /* IPSEC */

  if (rt->rt_gwroute != NULL)
    {
      if (rt->rt_gwroute->rt_ifp != rt->rt_ifp)
	rt->rt_ifp = rt->rt_gwroute->rt_ifp;
      /* Should also handle Path MTU increases. */
    }
  /* else assume ifp is good to go. */
  DPRINTF(GROSSEVENT,("tunnel_child():1000-Finished.\n"));
}

/*----------------------------------------------------------------------
 * tunnel_child_clean():
 *     probably does something like tunnel_parent_clean...
 ----------------------------------------------------------------------*/
void tunnel_child_clean(rt)
     register struct rtentry *rt;
{
  DPRINTF(GROSSEVENT,("tunnel_child_clean():0000-Starting.\n"));

#ifdef IPSEC
  if (rt->rt_flags & (RTF_CRYPT|RTF_AUTH))
    {
      struct socket *so = (struct socket *)rt->rt_netproc;

      DPRINTF(GROSSEVENT,("tunnel_child_clean():-Removing security state.\n"));
      if (so == NULL)
	{
	  DPRINTF(IDL_ERROR,("WARNING: tunnel_child_clean() Secure tunnel w/o dummy socket.\n"));
	  return;
	}

      if (so->so_netproc)
	if (--so->so_oobmark)  /* Refcnt != 0 */
	  return;
	else
	  netproc_free(so);

      free(so, M_SOCKET);
      rt->rt_netproc = NULL;
    }
#endif /* IPSEC */
  DPRINTF(GROSSEVENT,("tunnel_child_clean():1000-Finished.\n"));
}
