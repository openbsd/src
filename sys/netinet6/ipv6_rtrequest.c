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
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/errno.h>
/*#include <sys/ioctl.h>*/
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>

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
 * Globals (and forward function declarations).
 */

/*
 * External globals.
 */
extern struct sockaddr_in6 in6_allones;
extern struct discq    dqhead;
extern struct v6router defrtr;
extern struct v6router nondefrtr;

extern struct in6_ifaddr *in6_ifaddr;

extern int ipv6forwarding;

/*
 * External function prototypes.
 */

/*
 * General notes:
 *
 * These functions are mainly called from ipv6_discovery.c but are
 * fairly generic and generally useful and so live in their own file.
 *     danmcd rja
 */
void tunnel_parent __P((struct rtentry *));
struct v6router *ipv6_add_defrouter_rtrequest(struct rtentry *);
void tunnel_child __P((struct rtentry *));
void ipv6_nsolicit __P((struct ifnet *, struct mbuf *, struct rtentry *));
void tunnel_parent_clean __P((struct rtentry *));
int ipv6_delete_defrouter __P((struct v6router *));
void tunnel_child_clean __P((struct rtentry *));

static int add_defchild __P((struct rtentry *));
static void add_netchild __P((struct rtentry *));
void ipv6_setrtifa __P((struct rtentry *));
static void add_non_default __P((struct rtentry *));

/*
 * Functions and macros.
 */


/*----------------------------------------------------------------------
 * add_defchild():
 * 	Find the best default router out of our list and use it
 *    for this destination route.
 ----------------------------------------------------------------------*/
static int
add_defchild(rt)
     struct rtentry *rt;
{
  struct v6router *v6r = defrtr.v6r_next;
  struct sockaddr_in6 *dst;

  /*
   * What if this child turns out to be a tunneling route?  OUCH!
   *
   * One other thing:  If rt_key(rt) is link-local, but it hit from
   *                   the default router, then perhaps it should
   *                   go with the ipv6_onlink_query() route.
   */

  dst = (struct sockaddr_in6 *)rt_key(rt);

  if (!IN6_IS_ADDR_LINKLOCAL(&dst->sin6_addr)) {
    /*
     * Since there are no priorities, just pick one.  For now, I guess I'll
     * Just pick v6r_next.
     */
    if (v6r != &defrtr)
      {
	struct v6child *v6c;
	int rc = 0;

	DPRINTF(IDL_EVENT,("About to do rt_setgate. rt before is:\n"));
	DDO(IDL_EVENT,dump_rtentry(rt));
	DPRINTF(IDL_EVENT,("Args are (dst, parent->rt_gateway, v6r_rt):\n"));
	DDO(IDL_EVENT,dump_smart_sockaddr((struct sockaddr *)dst);\
	    dump_smart_sockaddr(rt->rt_parent->rt_gateway);\
	    dump_rtentry(v6r->v6r_rt));

	if (rt_key(v6r->v6r_rt)->sa_family != AF_INET6)
	  {
	    /*
	     * Hmmm, we must be a tunneling default route.  Current
	     * conventional wisdom is that we can only see one in our
	     * "default router list," and that it was added manually.  Using
	     * that assumption, get things right, including chasing the
	     * parent and using its rt_gateway sockaddr for rt_setgate.
	     */
	    rt_setgate(rt,(struct sockaddr *)dst,rt->rt_parent->rt_gateway);
	    rc = 1;
	  }
	else
	  {
	    rt_setgate(rt,(struct sockaddr *)dst,rt_key(v6r->v6r_rt));
	    rt->rt_flags &= ~RTF_TUNNEL;  /* In case cloned off an initially
					     tunnelling default route. */
	  }

	DDO(IDL_GROSS_EVENT,\
	    printf("After rt_setgate, rt is:\n");dump_rtentry(rt));

	rt->rt_llinfo = malloc(sizeof(*v6c),M_DISCQ,M_NOWAIT);
	if (rt->rt_llinfo == NULL)
	  {
	    DPRINTF(IDL_ERROR, ("add_defchild(): malloc failed.\n"));
	    /* Perhaps route should be freed here */
	  }
	else
	  {
	    v6c = (struct v6child *)rt->rt_llinfo;
	    insque(v6c,&v6r->v6r_children);
	    v6c->v6c_route = rt;
	  }
	return rc;
      }
    else
      {
	/*
	 * Default route hit, but no default router children.
	 *
	 * Perhaps delete it?
	 */
	DPRINTF(IDL_ERROR,\
		("Default route hit, but no default routers in list.\n"));
      }
  }

  /* Either  on-link or  in trouble. Needs to be coded. */
  DPRINTF(IDL_ERROR, ("On-link or in trouble in ipv6_rtrequest\n"));

  /*
   * Convert off-link entry to indeterminate on-link entry.
   * Subsequent code after a call to this function will do the right thing.
   * (ipv6_rtrequest code to handle new on-link neighbors follows this.)
   */
  rt->rt_flags = RTF_UP|RTF_LLINFO|RTF_HOST;
  rt->rt_llinfo = NULL;
  rt->rt_ifa->ifa_refcnt--;
  rt->rt_ifa = NULL;
  rt->rt_ifp = NULL;
  bzero(rt->rt_gateway,rt->rt_gateway->sa_len);
  rt->rt_gateway->sa_family = AF_LINK;
  rt->rt_gateway->sa_len = sizeof(struct sockaddr_dl);
  rt->rt_rmx.rmx_mtu = 0;

  /*
   * QUESTION:  If I do this, should I call ipv6_nsolicit() to complete
   *            this conversion from default route child to non-deterministic
   *            on-link neighbor?
   *
   * ANSWER:    Yes, but not here.  Just return 1, and let later code take
   *            care of the nsolicit.
   */
  return 1;
}

/*----------------------------------------------------------------------
 * add_netchild():
 * 	Given a network route child (i.e. non-default), 
 *    put this child in the parent's descendant list.
 ----------------------------------------------------------------------*/

static void
add_netchild(rt)
     struct rtentry *rt;
{
  struct v6router *v6r;
  struct v6child *v6c;

  if (rt->rt_parent == NULL)
    {
      DPRINTF(IDL_ERROR,
	      ("add_netchild: No parent route. (Must be manually added.)\n"));
      return;
    }
  v6r = (struct v6router *)rt->rt_parent->rt_llinfo;

  v6c = malloc(sizeof(*v6c),M_DISCQ,M_NOWAIT);
  if (v6c == NULL)
    {
      DPRINTF(IDL_ERROR, ("add_netchild(): malloc failed.\n"));
      /* Perhaps should free route here */
      return;
    }
  insque(v6c,v6r->v6r_children.v6c_next);
  v6c->v6c_route = rt;
  v6c->v6c_parent = v6r;
  rt->rt_llinfo = (caddr_t)v6c;
}

/*----------------------------------------------------------------------
 * add_non_default():
 *	add a non-default routing entry.
 *
 ----------------------------------------------------------------------*/
static void
add_non_default(rt)
     struct rtentry *rt;
{
  struct v6router *newbie;

  if (rt_key(rt)->sa_family != AF_INET6)
    {
      DPRINTF(IDL_ERROR, 
	      ("IPv6 off-net non-tunnel route w/o IPv6 gwaddr.\n"));
      return;
    }
    
  newbie = malloc(sizeof(*newbie),M_DISCQ,M_NOWAIT);
  if (newbie == NULL)
    {
      DPRINTF(IDL_ERROR, ("add_non_default(): malloc failed.\n"));
      /* Should probably free route */
      return;
    }
  bzero(newbie,sizeof(*newbie));
  newbie->v6r_children.v6c_next = newbie->v6r_children.v6c_prev = 
    &newbie->v6r_children;
  insque(newbie,&nondefrtr);

  /*
   * On creation of rt, rt_setgate() was called, therefore we take on blind
   * faith that an appropriate neighbor cache entry was created.  If not,
   * we're in deep trouble.
   */
  newbie->v6r_rt = rt->rt_gwroute;
  rt->rt_llinfo = (caddr_t)newbie;
}


/*----------------------------------------------------------------------
 * ipv6_setrtifa():
 *	Set route's interface address.  Source address selection for
 *      a route.
 *
 ----------------------------------------------------------------------*/
void
ipv6_setrtifa(rt)
     struct rtentry *rt;
{
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)rt_key(rt);
  struct sockaddr_in6 *src  = (struct sockaddr_in6 *)rt->rt_ifa->ifa_addr;
  struct in6_ifaddr *src_in6ifa = (struct in6_ifaddr *)rt->rt_ifa;
  struct sockaddr_in6 *mask = (struct sockaddr_in6 *)rt_mask(rt);
  struct ifaddr *ifa = NULL;

  DPRINTF(IDL_EVENT,("Entering ipv6_setrtifa.\n"));

  /*
   * If I can't find a "better" source address, stick with the one I got.
   *
   * ASSUMES:  rt_ifp is set PROPERLY.  This NEEDS to be true.
   */

  /*
   * If link-local, use link-local source.
   */

  if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
    {
      if (IN6_IS_ADDR_LINKLOCAL(&src->sin6_addr))
	return;
#if __FreeBSD__
      for (ifa = rt->rt_ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
      for (ifa = rt->rt_ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
      for (ifa = rt->rt_ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	{
	  struct sockaddr_in6 *current = (struct sockaddr_in6 *)ifa->ifa_addr;

	  if ((current->sin6_family != AF_INET6) || 
	      IS_DEPRECATED((struct in6_ifaddr *)ifa))
	    continue;  /* For loop. */
	  if (IN6_IS_ADDR_LINKLOCAL(&current->sin6_addr))
	    break;   /* For loop. */
	}
      if (ifa == NULL)
	return;         /* We're in real trouble. */
    }

  /*
   * If v4-compatible, use v4-compatible source address.
   */
  if (ifa == NULL && (sin6->sin6_addr.in6a_words[0] == 0 && 
		      sin6->sin6_addr.in6a_words[1] == 0 &&
		      sin6->sin6_addr.in6a_words[2] == 0 &&
		      sin6->sin6_addr.in6a_words[3] != htonl(1) &&
		      (mask == NULL || mask->sin6_len >= 
		            sizeof(*mask) - sizeof(struct in_addr))))
    {
      if (IN6_IS_ADDR_V4COMPAT(&src->sin6_addr) && !IS_DEPRECATED(src_in6ifa))
	return;
#if __FreeBSD__
      for (ifa = rt->rt_ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
      for (ifa = rt->rt_ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
      for (ifa = rt->rt_ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	{
	  struct sockaddr_in6 *current = (struct sockaddr_in6 *)ifa->ifa_addr;

	  if ((current->sin6_family != AF_INET6) 
	      || IS_DEPRECATED((struct in6_ifaddr *)ifa))
	    continue;  /* For loop. */
	  if (IN6_IS_ADDR_V4COMPAT(&current->sin6_addr))
	    break;   /* For loop. */
	}
      /*if (ifa == NULL)
	return;*/        /* if ifa == NULL here, pretend it's global, because
			    it is global! */
    }

  /*
   * If site-local, use a site-local source address.
   */

  if (ifa == NULL && IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))
    {
      if (IN6_IS_ADDR_SITELOCAL(&src->sin6_addr) && !IS_DEPRECATED(src_in6ifa))
	return;
#ifdef __FreeBSD__
      for (ifa = rt->rt_ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
      for (ifa = rt->rt_ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
      for (ifa = rt->rt_ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	{
	  struct sockaddr_in6 *current = (struct sockaddr_in6 *)ifa->ifa_addr;

	  if ((current->sin6_family != AF_INET6) 
	      || IS_DEPRECATED((struct in6_ifaddr *)ifa))
	    continue;  /* For loop. */
	  if (IN6_IS_ADDR_SITELOCAL(&current->sin6_addr))
	    break;   /* For loop. */
	}
      if (ifa == NULL)
	return;         /* We don't want to potentially pollute the global
			   internet with site-local traffic.  If you feel
			   differently, comment out this ifa == NULL check
			   and fallthrough. */
    }

  if (ifa == NULL)
    {
      /*
       * At this point, the address _could_ be anything, but is most likely
       * a globally routable address that didn't fit into the previous
       * categories.
       *
       * Do gyrations iff rt->rt_ifa's address is link-local and the dest
       * isn't or the src address previously picked is deprecated.
       */
      DPRINTF(IDL_GROSS_EVENT,("In default case of ipv6_setrtifa().\n"));
      if ((!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) &&
	   IN6_IS_ADDR_LINKLOCAL(&src->sin6_addr)) || IS_DEPRECATED(src_in6ifa))
	{
	  struct ifaddr *ifa_compat = 0, *ifa_site = 0;

	  /*
	   * For now, pick a non-link-local address using the following
	   * order of preference: global, compatible, site-local.
	   *
	   */
#ifdef __FreeBSD__
	  for (ifa = rt->rt_ifp->if_addrhead.tqh_first; ifa; ifa = ifa->ifa_link.tqe_next)
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	  for (ifa = rt->rt_ifp->if_addrlist.tqh_first; ifa; ifa = ifa->ifa_list.tqe_next)
#else /* __NetBSD__ || __OpenBSD__ */
	  for (ifa = rt->rt_ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	    if (ifa->ifa_addr->sa_family == AF_INET6 &&
                !IS_DEPRECATED((struct in6_ifaddr *)ifa) &&
		!IN6_IS_ADDR_LINKLOCAL(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr))
	      if (IN6_IS_ADDR_V4COMPAT(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr))
		ifa_compat = ifa;
	      else if (IN6_IS_ADDR_SITELOCAL(&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr))
		ifa_site = ifa;
	      else /* globally routable address */
		break;
	  if (!ifa)
	    ifa = ifa_compat ? ifa_compat : ifa_site;
	}
    }

  if (ifa != NULL)
    {
      /*
       * Q: Do I call ipv6_rtrequest (through ifa_rtrequest) if I
       *    change ifa's on a route?
       */
      /*if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
	rt->rt_ifa->ifa_rtrequest(RTM_DELETE,rt,rt->rt_gateway);*/
      IFAFREE(rt->rt_ifa);
      ifa->ifa_refcnt++;
      rt->rt_ifa = ifa;
      rt->rt_ifp = ifa->ifa_ifp;   /* Is this desirable? */
      /*if (ifa->ifa_rtrequest)
	ifa->ifa_rtrequest(RTM_ADD, rt, rt->rt_gateway);*/
    }
}


/*----------------------------------------------------------------------
 * ipv6_rtrequest():
 *	IPv6-specific route ADD, RESOLVE, and DELETE function.
 * Typically called from route.c's rtrequest() function to handle
 * the IPv6 unique things.
 ----------------------------------------------------------------------*/

void
ipv6_rtrequest(req, rt, sa)
     int req;
     register struct rtentry *rt;
     struct sockaddr *sa;  /* Ignored parameter, I believe. */
{
  static struct sockaddr_dl null_sdl = {sizeof(null_sdl), AF_LINK}, *sdlp;
  int spray = 0;

  DPRINTF(IDL_EVENT,("ipv6_rtrequest():0000-Starting.\n"));
  DPRINTF(IDL_EVENT,("req = %d, rt = 0x%lx, sa = 0x%lx\n",req,\
			   (unsigned long)rt, (unsigned long)sa));
  DDO(IDL_EVENT,dump_rtentry(rt));

  switch (req)
    {
    case RTM_ADD:
      DPRINTF(IDL_EVENT,("ipv6_rtrequest():0100-case RTM_ADD:.\n"));

      /*
       * Set route Path MTU if not already set on brand new route.
       */
      if (rt->rt_rmx.rmx_mtu == 0)
	rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;

      /*
       * An explicitly added "all-ones" mask or no mask at all should be
       * a host route.
       */

      if (rt_mask(rt) == NULL ||
	  (rt_mask(rt)->sa_len == sizeof(struct sockaddr_in6) &&
	   IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)rt_mask(rt))->sin6_addr,
			      &in6_allones.sin6_addr))
	  )
	rt->rt_flags |= RTF_HOST;

      /*
       * Explicitly add the cloning bit to the route entry.
       * (Do we still want to do this?)
       *
       * One fallout from this is that if someone wants to disable cloning,
       * the disabling will have to be explicitly done after adding the route.
       */
      if ((rt->rt_flags & RTF_HOST) == 0)
	{
	  /*
	   * All non-host routes that aren't m-cast are cloning!
	   */
	  if (!IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)rt_key(rt))->sin6_addr))
	    rt->rt_flags |= RTF_CLONING;

	  if (rt->rt_flags & RTF_TUNNEL)
	    {
              DPRINTF(IDL_EVENT,("ipv6_rtrequest():0150-Calling tunnel_parent().\n"));
	      tunnel_parent(rt);
	    }

	  if (rt->rt_flags & RTF_GATEWAY) {
	    if (rt_mask(rt)->sa_len == 0)
	      {
		if (rt->rt_flags & RTF_TUNNEL)
		  {
		    /*
		     * Add tunnel to default router list.
		     */
		    ipv6_add_defrouter_rtrequest(rt);
		  }
		else
		  {
		    struct v6router *v6r;
		    /*
		     * The "default" route has just been added.
		     * Do default router stuff here.
		     *
		     * Search router list to see if I'm already there.
		     * this avoids double-adding.
		     */
		    
		    for (v6r = defrtr.v6r_next; v6r != &defrtr;
			 v6r = v6r->v6r_next)
		      if (v6r->v6r_rt == rt->rt_gwroute)
			break;
		    
		    if (v6r == &defrtr)
		      {
			DPRINTF(IDL_ERROR,
				("Calling ipv6_add_defrouter from RTM_ADD.\n"));
			ipv6_add_defrouter_rtrequest(rt);
		      }
		    
		    rt->rt_gwroute->rt_flags |= RTF_ISAROUTER;
		  }
		rt->rt_flags |= RTF_DEFAULT;
	      }
	    else
	      if (rt->rt_flags & RTF_TUNNEL)
		{
		  /*
		   * Perhaps if the tunnel bit is already set here, don't
		   * do anything.
		   */
		}
	      else
		{
		  /*
		   * A non-default network route has just been added.
		   * Do non-default router stuff here.
		   */
		  add_non_default(rt);
		}
	  } else if (!(rt->rt_flags & RTF_TUNNEL))
	    {
	      /*
	       * Interface route (i.e. on-link non-host prefix).
	       */
              DPRINTF(IDL_EVENT,("Setting up i/f route.\n"));
              rt_setgate(rt, rt_key(rt), (struct sockaddr *)&null_sdl);
              sdlp = (struct sockaddr_dl *)rt->rt_gateway;
              sdlp->sdl_type = rt->rt_ifp->if_type;
              sdlp->sdl_index = rt->rt_ifp->if_index;
	    }
	}

      DPRINTF(IDL_EVENT,("ipv6_rtrequest():0199-Falling out of RTM_ADD:.\n"));
      /* FALLTHROUGH... */
    case RTM_RESOLVE:
      {
#ifdef __FreeBSD__
      extern struct ifnet loif[];
      struct ifnet *loifp = loif; 
#else /* __FreeBSD__ */
#if (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)) 
      extern struct ifnet loif;
#else /* (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)) */
      extern struct ifnet *loifp;
#endif /* (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)) */
#endif /* __FreeBSD__ */

      DPRINTF(IDL_EVENT,("ipv6_rtrequest():0200-Now in case RTM_RESOLVE:.\n"));

      /*
       * First, check if ifa addr is same as route addr, 
       * If yes, then wire the loopback interface as the destination.
       */
      if ((rt->rt_flags & RTF_HOST) && rt->rt_ifa &&
	  IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)rt_key(rt))->sin6_addr,
	  &I6A_SIN(rt->rt_ifa)->sin6_addr)
#if (defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802))
	  && loifp
#endif /* (defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)) */
         )
	{
	  /* Change ifp to loopback and gateway to itself. */
#ifdef __FreeBSD__
	  rt->rt_ifp = loifp;
	  rt->rt_gateway = rt_key(rt);
	  rt->rt_rmx.rmx_mtu = loifp->if_mtu;
#else /* __FreeBSD__ */
#if (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802))
	  rt->rt_ifp = &loif;
	  rt->rt_gateway = rt_key(rt);
	  rt->rt_rmx.rmx_mtu = loif.if_mtu;
#else /* (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)) */
	  rt->rt_ifp = loifp;
	  rt->rt_gateway = rt_key(rt);
	  rt->rt_rmx.rmx_mtu = loifp->if_mtu;
#endif /* (!defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)) */
#endif /* __FreeBSD__ */

	  if (rt->rt_parent)
            rt->rt_flags &= ~(RTF_STATIC | RTF_TUNNEL);

	  break;
	  };
	}

      /*
       * Second, check the accuracy of the route's interface address (ifa).
       * If possible, update the route's ifa such that it is the "best"
       * source address for packets bound for this destination.
       * 
       * This is done to any routes and is subtle enough
       * to merit its own separate function:
       */

      ipv6_setrtifa(rt);

      if (!(rt->rt_flags & RTF_HOST))
	{
	  /*
	   * This should be the "none of the above" case.  For now,
	   * do nothing and just return.
	   */
	  DPRINTF(IDL_GROSS_EVENT,("In RESOLVE/lateADD, but not RTF_HOST.\n"));
	  break;
	}

      /*
       * At this point, I know  (1) I'm a host route which has either been
       * added manually or  (2) I have cloned off to become either:
       *
       * a tunnelling route child,
       *      RTF_TUNNEL is set on this entry.
       *      I should be okay, although tunnel state may become necessary.
       *      I'm a next-hop cache entry.
       *
       * a default router child,
       * a non-default network route child,
       *      RTF_GATEWAY is set on this entry.
       *      I have to insert this child into the router's children list.
       *      My child entry hangs off rt_llinfo.  I'm a next-hop cache
       *      entry.
       *
       * an interface (i.e. neighbor) route child,
       * a link-local (i.e. off link-local mask) route child
       *      rt->rt_gateway is AF_LINK
       *      I should set RTF_LLINFO, and set up a discq entry because I'm
       *      a neighbor cache entry.  (Neighbor caches double as next-hop
       *      cache entries.)
       */

      if (rt->rt_gateway == NULL)
	panic("ipv6_rtrequest():  No rt_gateway at the RTM_RESOLVE code.");

      /*
       * Order of bit checking is very important.  I check TUNNEL first,
       * because it's REALLY special.  Then I check GATEWAY or not.
       */

      if (rt->rt_parent != NULL && rt_mask(rt->rt_parent)->sa_len == 0)
	{
	  DPRINTF(IDL_EVENT,("Cloning off default route!\n"));
	  /*
	   * Find a default router out of the list, and assign it to this
	   * child.  Clear or add tunnel bits if necessary.
	   */

	  rt->rt_flags &= ~RTF_STATIC;
	  /*
	   * If add_defchild returns 0, then either it's a tunneling
	   * default route, or a link-local converted to a neighbor entry.
	   *
	   * Set 'spray' to the result of add_defchild, because if it is
	   * a neighbor entry (see AF_LINK case below), then spray will
	   * indicate the need to do some pre-emptive neighbor adverts.
	   */
	  if (!(spray = add_defchild(rt)))  /* If an actual off-net default
					       route... */
	    break;
DPRINTF(IDL_ERROR,("add_defchild returned 1, either on-link or tunnel.\n"));
	  /* Otherwise, either tunnel or converted into LINK address. */
	}
      else rt->rt_llinfo = NULL;  /* Is this already done? */

      if (rt->rt_flags & RTF_TUNNEL)
	{
          DPRINTF(IDL_EVENT,("ipv6_rtrequest():0250-Calling tunnel_child().\n"));
	  tunnel_child(rt);  /* Be careful if manually added RTF_TUNNEL. */
	  break;   /* We're done with this Tunnel host route. */
	}

      if (rt->rt_flags & RTF_GATEWAY)
	{
	  add_netchild(rt);
	  if (req == RTM_RESOLVE)
	    rt->rt_flags &= ~RTF_STATIC;
	  break;
	}

      if (rt->rt_gateway->sa_family == AF_LINK)
	{
	  /*
	   * I may enter here even after RTF_GATEWAY check because
	   * get_defrouter() may convert the route entry into an on-link one!
	   */
	  struct discq *dq;

	  rt->rt_llinfo = malloc(sizeof(*dq),M_DISCQ,M_NOWAIT);
	  dq = (struct discq *)rt->rt_llinfo;
	  if (dq == NULL)
	    {
	      DPRINTF(IDL_ERROR,
		      ("ipv6_rtrequest(): malloc for rt_llinfo failed.\n"));
	      /* Probably should free route */
	      break;
	    }
	  bzero(dq,sizeof(*dq));
	  dq->dq_rt = rt;
	  dq->dq_unanswered = -1;
	  rt->rt_flags |= RTF_LLINFO;
	  insque(dq,&dqhead);
	  /*
	   * State is already INCOMPLETE, because of link stuff.
	   *
	   * If this neigbor entry is caused by add_defchild (i.e. link-local)
	   * spray nsolicit out all interfaces.
	   */

	  if (spray)
	    ipv6_nsolicit(NULL,NULL,rt);

	  break;
	}
      else
#if 0
	/*
	 * support for PPP goes here.
	 * Frame Relay, and ATM will probably probably be handled by the
	 * AF_LINK case.  Other AF_* will be covered later.
	 */
	panic("ipv6_rtrequest: Not tunnel, not off-net, and not neighbor.");
#else /* 0 */
	{
	DPRINTF(IDL_ERROR, ("ipv6_rtrequest: Not tunnel, not off-net, and not neighbor. rt is:\n"));
        DDO(IDL_ERROR, dump_rtentry(rt));
	}
#endif /* 0 */

      break;
    case RTM_DELETE:
      DPRINTF(IDL_GROSS_EVENT,("ipv6_rtrequest():0300-Now in case RTM_DELETE.\n"));
      /*
       * The FLUSH call ('route flush...') checks the ifp, perhaps I should
       * fill that just in case.
       */

      if (rt->rt_ifp == NULL)
	rt->rt_ifp = rt->rt_ifa->ifa_ifp;

      if ((rt->rt_flags & RTF_HOST) == 0)
	{
	  /*
	   * Clean up after network routes.
	   */
	  if (rt->rt_flags & RTF_TUNNEL)
	    {
	      DPRINTF(IDL_EVENT,("Cleaning up tunnel route.\n"));
	      tunnel_parent_clean(rt);
	      if (rt_mask(rt)->sa_len != 0)
		return;   /* If default route is RTF_TUNNEL, then continue. */
	    }
	  if (rt->rt_flags & RTF_GATEWAY) {
	    if (rt_mask(rt)->sa_len == 0)
	      {
		struct v6router *v6r;

		DPRINTF(IDL_GROSS_EVENT,\
			("Cleaning up THE default route.\n"));
		/*
		 * Find manually added default route thing, and clean up that
		 * entry.
		 *
		 * If user deletes default route added by receiving router
		 * adverts, then the router is still on the default router
		 * list, and needs to be added back.
		 */
		for (v6r = defrtr.v6r_next; v6r != &defrtr;
		     v6r = v6r->v6r_next)
		  {
		    /*
		     * PROBLEM:  rt->rt_gwroute, which is what I REALLY
		     *           want to check against, is gone at this
		     *           point.
		     *
		     * POSSIBLE SOLUTION:  Do a check with rt_key(v6r->v6r_rt)
		     *                     and rt->rt_gateway, but there
		     *                     is the question of tunneling
		     *                     default routes, etc., which may
		     *                     mean doing a masked match.
		     */
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
		    /*
		     * PRESENTED SOLUTION:  Try checking the expiration
		     *                      (easy giveaway for manually
		     *                      added default routes), followed
		     *                      by blatant sockaddr compares
		     *                      (will nail all other normal
		     *                      cases), followed by sa_family
		     *                      check (may be unneccesary).
		     */
		    if (v6r->v6r_expire == 0 || 
			equal(rt_key(v6r->v6r_rt),rt->rt_gateway) ||
			(rt->rt_gateway->sa_family != AF_INET6 &&
			 rt->rt_gateway->sa_family == 
			 rt_key(v6r->v6r_rt)->sa_family))
		      break;
		  }
		if (v6r != &defrtr)
		  (void)ipv6_delete_defrouter(v6r);
		if (defrtr.v6r_next != &defrtr)
		  {
		    /*
		     * Somehow re-add the default route.
		     *
		     * The default route has been deleted from the radix
		     * tree already, so re-adding should be relatively
		     * straightforward.
		     */
		    DPRINTF(IDL_ERROR,
			    ("Auto-added default routers still there!\n"));
		  }
	      }
	    else
	      {
		struct v6router *v6r = (struct v6router *)rt->rt_llinfo;
		struct v6child *v6c;

		/* Non-default router. */
		DPRINTF(IDL_GROSS_EVENT,\
			("Cleaning up a non-{default,host} route.\n"));
		
		if (v6r == NULL)
		  panic("Non-default w/o v6router entry.");
		
		v6c = v6r->v6r_children.v6c_next;
		while (v6c != v6c->v6c_next)
		  {
		    DPRINTF(IDL_ERROR,("Calling RTM_DELETE of child.\n"));
		    /* rtrequest() should remove child from the linked list */
		    rtrequest(RTM_DELETE,rt_key(v6c->v6c_route),NULL,NULL,0,
			      NULL);
		    v6c = v6r->v6r_children.v6c_next;
		  }
		remque(v6r);
		free(v6r,M_DISCQ);
	      }
          }
	  /* Anything else that isn't a HOST needs no work, so return. */
	  return;
	}

      DPRINTF(IDL_EVENT,("v6_discov_rtrequest() deleting a host\n"));
      DPRINTF(IDL_EVENT,("I'm at a host-route point.\n"));

      if (rt->rt_flags & RTF_TUNNEL)
	{
	  DPRINTF(IDL_EVENT,("Tunneling child.\n"));
	  /*
	   * PROBLEM:  Following check will die if parent went bye-bye.
	   *           See if you can fix in another way.
	   */
	  /*if (rt_mask(rt->rt_parent)->sa_len == 0)*/
	  if (rt->rt_flags & RTF_DEFAULT)
	    {
	      /*
	       * Tunneling default route child.  Clean off meta-state.
	       */
	      struct v6child *v6c = (struct v6child *)rt->rt_llinfo;

	      DPRINTF(IDL_ERROR,("Cleaning tunnel-default child.\n"));
	      remque(v6c);
	      rt->rt_llinfo = NULL;
	      free(v6c,M_DISCQ);
	    }
	  tunnel_child_clean(rt);
	}
      else if (rt->rt_flags & RTF_GATEWAY)
	{
	  struct v6child *v6c = (struct v6child *) rt->rt_llinfo;

	  if (v6c == NULL)
	    {
	      if (rt->rt_flags & RTF_HOST) {
		DPRINTF(IDL_ERROR, 
			("no v6c in RTM_DELETE of RTF_GATEWAY.\n"));
              }
	    }
	  else
	    {
	      remque(v6c);
	      rt->rt_llinfo = NULL;
	      free(v6c,M_DISCQ);
	    }
	}
      else if (rt->rt_flags & RTF_LLINFO)
	{
	  struct discq *dq;

	  /* Neighbor cache entry. */
	  if (rt->rt_flags & RTF_ISAROUTER)
	    {
	      struct v6router *v6r,*head;

	      /* Clean up all children of this router. */
	      
	      DPRINTF(IDL_GROSS_EVENT,("Cleaning up router neighbor.\n"));
	      if (rt->rt_flags & RTF_DEFAULT)
		head = &defrtr;
	      else
		{
		  head = &nondefrtr;
		  /* Q:  Do I want to delete the actual network route too? */
		  /* Q2: Do I want to delete the children? */
		}

	      for (v6r = head->v6r_next; v6r != head; v6r = v6r->v6r_next)
		if (v6r->v6r_rt == rt)
		  break;
	      if (v6r == head)
		{
		  /*
		   * At many addresses per interface, this isn't a huge
		   * problem, because a router's on-link address might not
		   * be tied to a router entry.
		   */
		  DPRINTF(IDL_EVENT,
			  ("Router neighbor inconsistency.\n"));
		}
	      else
		if (head == &defrtr)
		  (void) ipv6_delete_defrouter(v6r);  /* This gets rid of
							 children, too! */
	    }

	  dq = (struct discq *)rt->rt_llinfo;
	  rt->rt_flags &= ~RTF_LLINFO;
	  if (dq == NULL)
	    panic("No discq or other rt_llinfo in RTM_DELETE");
	  remque(dq);
	  rt->rt_llinfo = NULL;
	  if (dq->dq_queue)
	    {
	      /* Send ICMP unreachable error. */
	      ipv6_icmp_error(dq->dq_queue, ICMPV6_UNREACH,
			      ICMPV6_UNREACH_ADDRESS, 0);
	      /* m_freem(dq->dq_queue);*/
	    }
	  free(dq,M_DISCQ);
	}
      else
	{
	  DPRINTF(IDL_GROSS_EVENT,\
		  ("Freeing self-wired address.  Doing nothing.\n"));
	}
      break;
    }
    DPRINTF(IDL_GROSS_EVENT,("ipv6_rtrequest():1000-Finished.\n"));
}

/* EOF */
