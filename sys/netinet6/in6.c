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
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#if __NetBSD__ || __FreeBSD__
#include <sys/proc.h>
#endif /* __NetBSD__ || __FreeBSD__ */

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>

#include <sys/debug.h>

/*
 * Globals
 */

struct ifnet *mcastdefault = NULL;   /* Should be changeable by sysctl(). */

const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
const struct in6_addr in6addr_loopback = IN6ADDR_LOOPBACK_INIT;

/*
 * External globals
 */

extern struct sockaddr_in6 in6_allones;
extern struct in6_ifaddr *in6_ifaddr;
extern struct in6_ifnet *in6_ifnet;
extern int ipv6forwarding;

static void setmcastdef __P((register struct ifnet *));
void del_in6_ifnet __P((struct ifnet *));
struct in6_ifnet *add_in6_ifnet __P((struct ifnet *, int *));
int in6_ifscrub __P((struct ifnet *, struct in6_ifaddr *));
int in6_ifinit __P((register struct ifnet *, register struct in6_ifaddr *, struct sockaddr_in6 *, int, int));
void addrconf_dad __P((struct in6_ifaddr *));

/*----------------------------------------------------------------------
 * Set the default multicast interface.  In single-homed case, this will
 * always be the non-loopback interface.  In multi-homed cases, the function
 * should be able to set one accordingly.  The multicast route entry
 * (ff00::/8) will have its rt_ifp point to this interface, and its rt_ifa
 * point to whatever rtrequest() does.  The rt_ifa should be more intelligently
 * set eventually.
 ----------------------------------------------------------------------*/

static void
setmcastdef(ifp)
     register struct ifnet *ifp;
{
#ifdef __FreeBSD__
struct ifaddr *ifa = ifp->if_addrhead.tqh_first;
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
  struct ifaddr *ifa = ifp->if_addrlist.tqh_first;
#else /* __NetBSD__ || __OpenBSD__ */
  struct ifaddr *ifa = ifp->if_addrlist;
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
  struct sockaddr_dl lsdl;
  struct sockaddr_in6 lsin6;
  struct rtentry *newrt=NULL;
  int s;

  if (ifp == mcastdefault)
    return;

  /*
   * If NULL, nuke any mcast entry.
   */

  /*
   * Find link addr for ifp.
   */

  while (ifa != NULL && ifa->ifa_addr->sa_family != AF_LINK)
#ifdef __FreeBSD__
    ifa = ifa->ifa_link.tqe_next;
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
    ifa = ifa->ifa_list.tqe_next;
#else /* __NetBSD__ || __OpenBSD__ */
    ifa = ifa->ifa_next;
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */

  if (ifa == NULL)
    panic("Can't find AF_LINK for new multicast default interface.");

  bcopy(ifa->ifa_addr,&lsdl,ifa->ifa_addr->sa_len);
  DDO(IDL_EVENT,dump_smart_sockaddr((struct sockaddr *)&lsdl));
  lsdl.sdl_alen = 0;
  lsdl.sdl_slen = 0;
  lsdl.sdl_nlen = 0;

  /*
   * Delete old route, and add new one.
   */

  bzero(&lsin6,sizeof(lsin6));
  lsin6.sin6_family = AF_INET6;
  lsin6.sin6_len = sizeof(lsin6);
  lsin6.sin6_addr.s6_addr[0]=0xff;

  /* Neat property, mask and value are identical! */

  s = splnet();
  rtrequest(RTM_DELETE,(struct sockaddr *)&lsin6,NULL,
	    (struct sockaddr *)&lsin6,0,NULL);
  /*
   *
   * NB: If we clone, we have mcast dests being on a route.  
   *     Consider multihomed system with processes talking to the 
   *     same mcast group, but out different interfaces.
   *
   * Also, the RTM_ADD will do its best to find a "source address" to stick
   * in the rt_ifa field.  (See ipv6_rtrequest.c for this code.)
   */
  rtrequest(RTM_ADD,(struct sockaddr *)&lsin6,(struct sockaddr *)&lsdl,
	    (struct sockaddr *)&lsin6,0,&newrt);
  if (newrt == NULL)
    panic("Assigning default multicast if.");
  newrt->rt_rmx.rmx_mtu = ifp->if_mtu;
  newrt->rt_refcnt--;
  mcastdefault = ifp;
  splx(s);
}

/*----------------------------------------------------------------------
 * Delete an "IPv6 interface".  Only called inside splnet().
 ----------------------------------------------------------------------*/

void
del_in6_ifnet(ifp)
     struct ifnet *ifp;
{
  struct in6_ifnet *i6ifp,*prev = NULL;

  for (i6ifp = in6_ifnet; i6ifp != NULL; i6ifp = i6ifp->i6ifp_next)
    {
      if (i6ifp->i6ifp_ifp == ifp)
	break;
      prev = i6ifp;
    }

  if (i6ifp == NULL)
    panic("Ooooh boy, consistency mismatch in del_in6_ifnet!");

  if (--(i6ifp->i6ifp_numaddrs) == 0)
    {
      while (i6ifp->i6ifp_multiaddrs != NULL)
	{
	  i6ifp->i6ifp_multiaddrs->in6m_refcount = 1;
	  in6_delmulti(i6ifp->i6ifp_multiaddrs);
	}
      if (prev == NULL)
	in6_ifnet = i6ifp->i6ifp_next;
      else prev->i6ifp_next = i6ifp->i6ifp_next;
      free(i6ifp,M_I6IFP);
    }
}

/*----------------------------------------------------------------------
 * Add a new "IPv6 interface".  Only called inside splnet().
 * Perhaps send router adverts when this gets called.  For now, they
 * are issued when duplicate address detection succeeds on link-locals.
 * See ipv6_addrconf.c for details.
 ----------------------------------------------------------------------*/

struct in6_ifnet *
add_in6_ifnet(ifp, new)
     struct ifnet *ifp;  /* Assume an in6_ifaddr with this ifp is already
			    allocated and linked into the master list. */
     int *new;           /* XXX */
{
  struct in6_ifnet *i6ifp;

  *new = 0;
  for (i6ifp = in6_ifnet; i6ifp != NULL; i6ifp = i6ifp->i6ifp_next)
    if (i6ifp->i6ifp_ifp == ifp)
      break;

  if (i6ifp == NULL)
    {
      i6ifp = malloc(sizeof(*i6ifp),M_I6IFP,M_NOWAIT);
      if (i6ifp == NULL)
	{
	  printf("DANGER!  Malloc for i6ifp failed.\n");
	  return NULL;
	}
      i6ifp->i6ifp_ifp = ifp;
      i6ifp->i6ifp_multiaddrs = NULL;
      i6ifp->i6ifp_numaddrs = 1;
      /* Other inits... */
      i6ifp->i6ifp_next = in6_ifnet;
      in6_ifnet = i6ifp;
      *new = 1;
    }

  return i6ifp;
}

/*----------------------------------------------------------------------
 * This function is called by the PRU_CONTROL handlers in both TCP and UDP.
 * (Actually raw_ipv6 might need a PRU_CONTROL handler, but raw_ip doesn't
 * have one.)
 ----------------------------------------------------------------------*/

int
#if __NetBSD__ || __FreeBSD__
in6_control(so, cmd, data, ifp, internal, p)
#else /* __NetBSD__ || __FreeBSD__ */
in6_control(so, cmd, data, ifp, internal)
#endif /* __NetBSD__ || __FreeBSD__ */
     struct socket *so;
#if __NetBSD__
     u_long cmd;
#else /* __NetBSD__ */
     int cmd;
#endif /* __NetBSD__ */
     caddr_t data;
     register struct ifnet *ifp;
     int internal;
#if __NetBSD__ || __FreeBSD__
     struct proc *p;
#endif /* __NetBSD__ || __FreeBSD__ */
{
  register struct inet6_ifreq *ifr = (struct inet6_ifreq *)data;
  register struct in6_ifaddr *i6a = 0;
  struct in6_ifaddr *oi6a;
  struct inet6_aliasreq *ifra = (struct inet6_aliasreq *)data;
  struct sockaddr_in6 oldaddr;
  int error, hostIsNew, maskIsNew, ifnetIsNew = 0;
#if !__NetBSD__ && !__OpenBSD__ && !__FreeBSD__
  struct ifaddr *ifa;
#endif /* !__NetBSD__ && !__OpenBSD__ && !__FreeBSD__ */

  /*
   * If given an interface, find first IPv6 address on that interface.
   * I may want to change how this is searched.  I also may want to
   * discriminate between link-local, site-local, v4-compatible, etc.
   *
   * This is used by the SIOCGIFADDR_INET6, and other such things.
   * Those ioctls() currently assume only one IPv6 address on an interface.
   * This is not a good assumption, and this code will have to be modified
   * to correct that assumption.
   */
  if (ifp)
    for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
      if (i6a->i6a_ifp == ifp)
	break;

  switch (cmd)
    {
    case SIOCAIFADDR_INET6:
    case SIOCDIFADDR_INET6:
    case SIOCVIFADDR_INET6:
      /*
       * For adding and deleting an address, find an exact match for
       * that address.  Note that ifr_addr and ifra_addr are in the same
       * place, so even though VIFADDR uses a different struct than AIFADDR,
       * the match will still occur.
       */
      if (ifra->ifra_addr.sin6_family == AF_INET6 &&
			(cmd != SIOCDIFADDR_INET6 ||
			!IN6_IS_ADDR_UNSPECIFIED(&ifra->ifra_addr.sin6_addr)))
	for (oi6a = i6a; i6a; i6a = i6a->i6a_next)
	  {
	    if (i6a->i6a_ifp == ifp &&
		IN6_ARE_ADDR_EQUAL(&i6a->i6a_addr.sin6_addr, &ifra->ifra_addr.sin6_addr))
	      break; /* Out of for loop. */
	  }

      /*
       * You can't delete what you don't have...
       */
      if (cmd == SIOCDIFADDR_INET6 && i6a == 0)
	return EADDRNOTAVAIL;

      /*
       * User program requests verification of address.  No harm done in
       * letting ANY program use this ioctl(), so we put code in for it
       * here.
       *
       * If I found the i6a, check if I'm not sure.  Return EWOULDBLOCK if
       * not sure, return 0 if sure.  Return EADDRNOTAVAIL if not available
       * (i.e. DAD failed.).
       */
      if (cmd == SIOCVIFADDR_INET6)
	if (i6a == NULL)
	  return EADDRNOTAVAIL;
	else if (i6a->i6a_addrflags & I6AF_NOTSURE)
	  return EWOULDBLOCK;
	else return 0;

      /* FALLTHROUGH TO... */

    case SIOCSIFDSTADDR_INET6:
#if __NetBSD__ || __FreeBSD__
      if (p == 0 || (error = suser(p->p_ucred, &p->p_acflag)) )
#else /* __NetBSD__ || __FreeBSD__ */
      if ((so->so_state & SS_PRIV) == 0)
#endif /* __NetBSD__ || __FreeBSD__ */
	return EPERM;

      if (ifp==0)
	panic("in6_control, ifp==0");
      if (i6a == NULL)
	{
	  struct in6_ifaddr *tmp;

	  /*
	   * Create new in6_ifaddr (IPv6 interface address) for additions
	   * and destination settings.
	   */
	  if (!(tmp = (struct in6_ifaddr *)malloc(sizeof(struct in6_ifaddr),
						  M_IFADDR,M_NOWAIT)))
	    {
	      return ENOBUFS;
	    }

	  bzero(tmp,sizeof(struct in6_ifaddr));
	  /*
	   * Set NOTSURE addrflag before putting in list.
	   */
	  tmp->i6a_addrflags = I6AF_NOTSURE;
	  if ((i6a = in6_ifaddr))
	    {
	      for (; i6a->i6a_next; i6a=i6a->i6a_next)
		;
	      i6a->i6a_next = tmp;
	    }
	  else in6_ifaddr = tmp;
	  i6a = tmp;
#ifdef __FreeBSD__
	  TAILQ_INSERT_TAIL(&ifp->if_addrhead, (struct ifaddr *)i6a,
			    ifa_link);
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
	  TAILQ_INSERT_TAIL(&ifp->if_addrlist, (struct ifaddr *)i6a,
			    ifa_list);
#else /* __NetBSD__ || __OpenBSD__ */
	  if (ifa = ifp->if_addrlist)
	    {
	      for (; ifa->ifa_next; ifa=ifa->ifa_next)
		;
	      ifa->ifa_next = (struct ifaddr *)i6a;
	    }
	  else ifp->if_addrlist = (struct ifaddr *)i6a;
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
	  i6a->i6a_ifa.ifa_addr = (struct sockaddr *)&i6a->i6a_addr;
	  i6a->i6a_ifa.ifa_dstaddr = (struct sockaddr *)&i6a->i6a_dstaddr;
	  i6a->i6a_ifa.ifa_netmask
	    = (struct sockaddr *)&i6a->i6a_sockmask;
	  i6a->i6a_sockmask.sin6_len = sizeof(struct sockaddr_in6);
	  i6a->i6a_ifp = ifp;

	  /*
	   * Add address to IPv6 interface lists.
	   */
	  i6a->i6a_i6ifp = add_in6_ifnet(ifp, &ifnetIsNew);
	}
      break;
    case SIOCGIFADDR_INET6:
    case SIOCGIFNETMASK_INET6:
    case SIOCGIFDSTADDR_INET6:
      /*
       * Can't get information on what is not there...
       */
      if (i6a == NULL)
	return EADDRNOTAVAIL;
      break;

    default:
      return EOPNOTSUPP;
    }

  switch (cmd)
    {
      /*
       * The following three cases assume that there is only one address per
       * interface; this is not good in IPv6-land.  Unfortunately, the
       * ioctl() interface, is such that I'll have to rewrite the way things
       * work here, either that, or curious user programs will have to troll
       * /dev/kmem (like netstat(8) does).
       */
    case SIOCGIFADDR_INET6:
      bcopy(&(i6a->i6a_addr),&(ifr->ifr_addr),sizeof(struct sockaddr_in6));
      break;

    case SIOCGIFDSTADDR_INET6:
      if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
	return EINVAL;
      bcopy(&(i6a->i6a_dstaddr),&(ifr->ifr_dstaddr),
	    sizeof(struct sockaddr_in6));
      break;

    case SIOCGIFNETMASK_INET6:
      bcopy(&(i6a->i6a_sockmask),&(ifr->ifr_addr),sizeof(struct sockaddr_in6));
      break;

    case SIOCSIFDSTADDR_INET6:
      i6a->i6a_addrflags &= ~I6AF_NOTSURE;
      if ((ifp->if_flags & IFF_POINTOPOINT) == 0)
	return EINVAL;
      oldaddr = i6a->i6a_dstaddr;
      i6a->i6a_dstaddr = *(struct sockaddr_in6 *)&ifr->ifr_dstaddr;
      if (ifp->if_ioctl && (error = (*ifp->if_ioctl)(ifp, SIOCSIFDSTADDR,
						     (caddr_t)i6a)))
	{
	  i6a->i6a_dstaddr = oldaddr;
	  return error;
	}
      if (i6a->i6a_flags & IFA_ROUTE)
	{
	  i6a->i6a_ifa.ifa_dstaddr = (struct sockaddr *)&oldaddr;
	  rtinit(&(i6a->i6a_ifa), RTM_DELETE, RTF_HOST);
	  i6a->i6a_ifa.ifa_dstaddr = (struct sockaddr *)&i6a->i6a_dstaddr;
	  rtinit(&(i6a->i6a_ifa), RTM_ADD, RTF_HOST|RTF_UP);
	}
      break;

      /*
       * For adding new IPv6 addresses to an interface, I stuck to the way
       * that IPv4 uses, pretty much.
       */
    case SIOCAIFADDR_INET6:
      maskIsNew = 0;
      hostIsNew = 1;
      error = 0;
      if (i6a->i6a_addr.sin6_family == AF_INET6)
	if (ifra->ifra_addr.sin6_len == 0)
	  {
	    bcopy(&(i6a->i6a_addr),&(ifra->ifra_addr),
		  sizeof(struct sockaddr_in6));
	    hostIsNew = 0;
	  }
	else if (IN6_ARE_ADDR_EQUAL(&ifra->ifra_addr.sin6_addr, &i6a->i6a_addr.sin6_addr))
	  hostIsNew = 0;

      if (ifra->ifra_mask.sin6_len)
	{
	  in6_ifscrub(ifp,i6a);
	  bcopy(&(ifra->ifra_mask),&(i6a->i6a_sockmask),
		sizeof(struct sockaddr_in6));
	  maskIsNew = 1;
	}

      if ((ifp->if_flags & IFF_POINTOPOINT) &&
	  (ifra->ifra_dstaddr.sin6_family == AF_INET6))
	{
	  in6_ifscrub(ifp,i6a);
	  bcopy(&(ifra->ifra_dstaddr),&(i6a->i6a_dstaddr),
		sizeof(struct sockaddr_in6));
	  maskIsNew = 1;  /* We lie, simply so that in6_ifinit() will be
			     called to initialize the peer's address. */
	}
      if (ifra->ifra_addr.sin6_family == AF_INET6 && (hostIsNew || maskIsNew))
	error = in6_ifinit(ifp,i6a,&ifra->ifra_addr,0,!internal);
      /* else i6a->i6a_addrflags &= ~I6AF_NOTSURE; */

      if (error == EEXIST)   /* XXX, if route exists, we should be ok */
	error = 0;

      if (hostIsNew && !ifnetIsNew /* && (!error || error == EEXIST) */) 
	{
	  if (i6a->i6a_i6ifp)
	    i6a->i6a_i6ifp->i6ifp_numaddrs++;
	  else
	    panic("in6_control: missing i6ifp");
	}
      return error;

    case SIOCDIFADDR_INET6:
      in6_ifscrub(ifp, i6a);
      /*
       * If last address on this interface, delete IPv6 interface record.
       */
      del_in6_ifnet(ifp);

#ifdef __FreeBSD__
      TAILQ_REMOVE(&ifp->if_addrhead, (struct ifaddr *)i6a, ifa_link);
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
      TAILQ_REMOVE(&ifp->if_addrlist, (struct ifaddr *)i6a, ifa_list);
#else /* __NetBSD__ || __OpenBSD__ */
      if ((ifa = ifp->if_addrlist) == (struct ifaddr *)i6a)
	ifp->if_addrlist = ifa->ifa_next;
      else
	{
	  while (ifa->ifa_next &&
		 (ifa->ifa_next != (struct ifaddr *)i6a))
	    ifa=ifa->ifa_next;
	  if (ifa->ifa_next)
	    ifa->ifa_next = i6a->i6a_ifa.ifa_next;
	  else 
	    DPRINTF(IDL_ERROR, ("Couldn't unlink in6_ifaddr from ifp!\n"));
	}
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
      oi6a = i6a;
      if (oi6a == (i6a = in6_ifaddr))
	in6_ifaddr = i6a->i6a_next;
      else
	{
	  while (i6a->i6a_next && (i6a->i6a_next != oi6a))
	    i6a = i6a->i6a_next;
	  if (i6a->i6a_next)
	    i6a->i6a_next = oi6a->i6a_next;
	  else 
	    DPRINTF(IDL_ERROR, ("Didn't unlink in6_ifaddr from list.\n"));
	}
      IFAFREE((&oi6a->i6a_ifa));  /* For the benefit of routes pointing
				     to this ifa. */
      break;

    default:
      DPRINTF(IDL_ERROR, 
	      ("in6_control(): Default case not implemented.\n"));
      return EOPNOTSUPP;
    }

  return 0;
}

/*----------------------------------------------------------------------
 * in6_ifscrub:   
 *     Delete any existing route for an IPv6 interface.
 ----------------------------------------------------------------------*/

int
in6_ifscrub(ifp,i6a)
     register struct ifnet *ifp;
     register struct in6_ifaddr *i6a;
{
  if (!(i6a->i6a_flags & IFA_ROUTE))
    return 1;

  if (ifp->if_flags & (IFF_LOOPBACK|IFF_POINTOPOINT))
    rtinit(&(i6a->i6a_ifa), (int)RTM_DELETE, RTF_HOST);
  else
    rtinit(&(i6a->i6a_ifa), (int)RTM_DELETE, 0);
  i6a->i6a_flags &= ~IFA_ROUTE;

  return 0;
}

/*----------------------------------------------------------------------
 * Initialize an IPv6 address for an interface.
 *
 * When I get around to doing duplicate address detection, this is probably
 * the place to do it.
 ----------------------------------------------------------------------*/

int
in6_ifinit(ifp, i6a, sin6, scrub, useDAD)
     register struct ifnet *ifp;
     register struct in6_ifaddr *i6a;
     struct sockaddr_in6 *sin6;
     int scrub;
     int useDAD;
{
  int s, error, flags = RTF_UP;
  struct sockaddr_in6 oldaddr;

  DPRINTF(IDL_EVENT,("Before splimp in in6_ifinit()\n"));
  s = splimp();

  bcopy(&(i6a->i6a_addr),&oldaddr,sizeof(struct sockaddr_in6));
  bcopy(sin6,&(i6a->i6a_addr),sizeof(struct sockaddr_in6));

  /*
   * Give the interface a chance to initialize
   * if this is its first address,
   * and to validate the address if necessary.
   */

  if (ifp->if_ioctl && (error = (*ifp->if_ioctl)(ifp, SIOCSIFADDR,
						 (caddr_t)i6a)))
    { 
      bcopy(&oldaddr,&(i6a->i6a_addr),sizeof(struct sockaddr_in6));
      splx(s);
      return error;
    }

  /*
   * IPv4 in 4.4BSD sets the RTF_CLONING flag here if it's an Ethernet.
   * I delay this until later.
   */

  splx(s);
  DPRINTF(IDL_EVENT,("After splx() in in6_ifinit().\n"));

  sin6->sin6_port = 0;

  if (scrub)
    {
      i6a->i6a_ifa.ifa_addr = (struct sockaddr *)&oldaddr;
      in6_ifscrub(ifp, i6a);
      i6a->i6a_ifa.ifa_addr = (struct sockaddr *)&i6a->i6a_addr;
    }

  /*
   * Adjust the sin6_len such that it only counts mask bytes with
   * 1's in them.
   */

  {
    register char *cpbase = (char *)&(i6a->i6a_sockmask.sin6_addr);
    register char *cp = cpbase + sizeof(struct in6_addr);

    i6a->i6a_sockmask.sin6_len = 0;
    while (--cp >=cpbase)
      if (*cp)
	{
	  i6a->i6a_sockmask.sin6_len = 1 + cp - (char *)&(i6a->i6a_sockmask);
	  break;
	}
  }

  /*
   * Add route.  Also, set some properties of the interface address here.
   * (Properties include permanance, lifetime, etc.)
   */

  i6a->i6a_ifa.ifa_metric = ifp->if_metric;
  i6a->i6a_ifa.ifa_rtrequest = ipv6_rtrequest;  /* Want this to be true
						   for ALL IPv6 ifaddrs. */
  if (ifp->if_flags & IFF_LOOPBACK)
    {
      useDAD = 0;
      i6a->i6a_ifa.ifa_dstaddr = i6a->i6a_ifa.ifa_addr;
      flags |= RTF_HOST;

      /* Loopback is definitely a permanent address. */
      if (IN6_IS_ADDR_LOOPBACK(&i6a->i6a_addr.sin6_addr))
	i6a->i6a_addrflags |= I6AF_PERMANENT;
    }
  else if (ifp->if_flags & IFF_POINTOPOINT)
    {
      useDAD = 0;  /* ??!!?? */
      if (i6a->i6a_dstaddr.sin6_family != AF_INET6)
	return 0;

      flags |= RTF_HOST;
    }
  else 
    {
      /*
       * No b-cast in IPv6, therefore the ifa_broadaddr (concidentally the
       * dest address filled in above...) should be set to NULL!
       */
      i6a->i6a_ifa.ifa_broadaddr = NULL;

      if (IN6_IS_ADDR_LINKLOCAL(&i6a->i6a_addr.sin6_addr))
	{
	  flags |= RTF_HOST;
	  i6a->i6a_ifa.ifa_dstaddr = i6a->i6a_ifa.ifa_addr;

	  /*
	   * Possibly do other stuff specific to link-local addresses, hence
	   * keeping this separate from IFF_LOOPBACK case above.  I may move
	   * the link-local check to || with IFF_LOOPBACK.
	   *
	   * Other stuff includes setting i6a_preflen so when addrconf
	   * needs to know what part of the link-local is used for uniqueness,
	   * it doesn't have to gyrate.
	   */
	  switch(i6a->i6a_ifp->if_type)
	    {
	    case IFT_ETHER:
	      i6a->i6a_preflen = 64;
	      break;
	    default:
	      DPRINTF(IDL_ERROR,("Can't set i6a_preflen for type %d.\n",\
				    i6a->i6a_ifp->if_type));
	      break;
	    }

	  i6a->i6a_addrflags |= (I6AF_LINKLOC | I6AF_PERMANENT);
	}
      else
	{
	  if (!(i6a->i6a_sockmask.sin6_len == sizeof(struct sockaddr_in6) &&
		IN6_ARE_ADDR_EQUAL(&i6a->i6a_sockmask.sin6_addr, &in6_allones.sin6_addr)))
	    flags |= RTF_CLONING;  /* IMHO, ALL network routes
				      have the cloning bit set for next-hop
				      resolution if they aren't loopback or
				      pt. to pt. */
	  i6a->i6a_addrflags |= I6AF_PREFIX;  /* I'm a 'prefix list entry'. */
	}
    }

  if ((error = rtinit(&(i6a->i6a_ifa), RTM_ADD,flags)) == 0)
    {
      i6a->i6a_flags |= IFA_ROUTE;
    }

  /*
   * If the interface supports multicast, join the appropriate
   * multicast groups (all {nodes, routers}) on that interface.
   *
   * Also join the solicited nodes discovery multicast group for that
   * destination.
   */
  if (ifp->if_flags & IFF_MULTICAST)
    {
      struct in6_addr addr;
      struct in6_multi *rc;
      
      /* NOTE2:  Set default multicast interface here.
                 Set up cloning route for ff00::0/8 */
      if (ifp->if_type != IFT_LOOP && mcastdefault == NULL)
	setmcastdef(ifp);

      /* All-nodes. */
      SET_IN6_ALLNODES(addr);
      SET_IN6_MCASTSCOPE(addr,IN6_INTRA_LINK);
      rc = in6_addmulti(&addr, ifp);

      /* All-routers, if forwarding */
      if (ipv6forwarding) {
	SET_IN6_ALLROUTERS(addr);
	SET_IN6_MCASTSCOPE(addr, IN6_INTRA_LINK);
	rc = in6_addmulti(&addr, ifp);
      };

      /* Solicited-nodes. */
      addr.in6a_words[0] = htonl(0xff020000);
      addr.in6a_words[1] = 0;
      addr.in6a_words[2] = htonl(1);
      addr.in6a_words[3] = i6a->i6a_addr.sin6_addr.in6a_words[3] | htonl(0xff000000);

      DDO(IDL_EVENT, dump_in6_addr(&addr));

      rc=in6_addmulti(&addr, ifp);
    }

  if (useDAD /*&& error != 0*/)
    addrconf_dad(i6a);
  else
    i6a->i6a_addrflags &= ~I6AF_NOTSURE;

  return error;
}

/*----------------------------------------------------------------------
 * Add IPv6 multicast address.  IPv6 multicast addresses are handled
 * pretty much like IP multicast addresses for now.
 *
 * Multicast addresses hang off in6_ifaddr's.  Eventually, they should hang
 * off the link-local multicast address, this way, there are no ambiguities.
 ----------------------------------------------------------------------*/

struct in6_multi *in6_addmulti(addr,ifp)
     register struct in6_addr *addr;
     struct ifnet *ifp;

{
  register struct in6_multi *in6m;
  struct inet6_ifreq ifr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ifr.ifr_addr;
  struct in6_ifnet *i6ifp;
  int s = splnet();

  /*
   * See if address is already in list..
   */

  IN6_LOOKUP_MULTI(addr,ifp,in6m);

  if (in6m != NULL)
    {
      /* Increment the reference count. */
      in6m->in6m_refcount++;
    }
  else
    {
#if __FreeBSD__
      struct ifmultiaddr *ifma;
#endif /* __FreeBSD__ */
      /*
       * Otherwise, allocate a new m-cast record and link it to
       * the interface's multicast list.
       */
      
      if ((in6m=malloc(sizeof(struct in6_multi),M_IPMADDR,M_NOWAIT)) == NULL)
	{
	  splx(s);
	  return NULL;
	}
      bzero(in6m,sizeof(struct in6_multi));
      in6m->in6m_addr = *addr;
      in6m->in6m_refcount = 1;
      in6m->in6m_ifp = ifp;

      for(i6ifp = in6_ifnet; i6ifp != NULL && i6ifp->i6ifp_ifp != ifp;
	  i6ifp = i6ifp->i6ifp_next)
	;
      if (i6ifp == NULL)
	{
	  free(in6m,M_IPMADDR);
	  splx(s);
	  return NULL;
	}
      in6m->in6m_i6ifp = i6ifp;
      in6m->in6m_next = i6ifp->i6ifp_multiaddrs;
      i6ifp->i6ifp_multiaddrs = in6m;

      /*
       * Ask the network driver to update its multicast reception
       * filter appropriately for the new address.
       */
      sin6->sin6_family=AF_INET6;
      sin6->sin6_len=sizeof(struct sockaddr_in6);
      sin6->sin6_addr = *addr;
      sin6->sin6_port = 0;
      sin6->sin6_flowinfo = 0;


#if __FreeBSD__
      if (if_addmulti(ifp, (struct sockaddr *) sin6, &ifma))
#else /* __FreeBSD */
      if (ifp->if_ioctl == NULL ||
	  (*ifp->if_ioctl)(ifp, SIOCADDMULTI,(caddr_t)&ifr) != 0)
#endif /* __FreeBSD__ */
	{
	  i6ifp->i6ifp_multiaddrs = in6m->in6m_next;
	  free(in6m,M_IPMADDR);
	  splx(s);
	  return NULL;
	}
#ifdef __FreeBSD__
      ifma->ifma_protospec = in6m;
#endif /* __FreeBSD__ */
      
      /* Tell IGMP that we've joined a new group. */
      /*ipv6_igmp_joingroup(in6m);*/
    }
  splx(s);
  return in6m;
}

/*----------------------------------------------------------------------
 * Delete IPv6 multicast address.
 ----------------------------------------------------------------------*/

void
in6_delmulti(in6m)
     register struct in6_multi *in6m;
{
  register struct in6_multi **p;
  struct inet6_ifreq ifr;
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&(ifr.ifr_addr);
  int s = splnet();

  if (--in6m->in6m_refcount == 0)
    {
      /* Tell IGMP that I'm bailing this group. */
      /* ipv6_igmp_leavegroup(in6m);*/

      /* Unlink from list. */
      for (p = &(in6m->in6m_i6ifp->i6ifp_multiaddrs);
	   *p != in6m;
	   p = &(*p)->in6m_next)
	;
      *p = (*p)->in6m_next;

      /*
       * Notify the network driver to update its multicast reception
       * filter.
       */
      sin6->sin6_family = AF_INET6;
      sin6->sin6_len = sizeof(struct sockaddr_in6);
      sin6->sin6_port = 0;
      sin6->sin6_flowinfo = 0;
      sin6->sin6_addr = in6m->in6m_addr;
      (*(in6m->in6m_ifp->if_ioctl))(in6m->in6m_ifp, SIOCDELMULTI,
					 (caddr_t)&ifr);

      free(in6m,M_IPMADDR);
    }
  splx(s);
}
