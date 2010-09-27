/*	$OpenBSD: at_control.c,v 1.17 2010/09/27 20:16:17 guenther Exp $	*/

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 */

/*
 * The following is the contents of the COPYRIGHT file from the
 * netatalk-1.4a2 distribution, from which this file is derived.
 */
/*
 * Copyright (c) 1990,1996 Regents of The University of Michigan.
 *
 * All Rights Reserved.
 *
 *    Permission to use, copy, modify, and distribute this software and
 *    its documentation for any purpose and without fee is hereby granted,
 *    provided that the above copyright notice appears in all copies and
 *    that both that copyright notice and this permission notice appear
 *    in supporting documentation, and that the name of The University
 *    of Michigan not be used in advertising or publicity pertaining to
 *    distribution of the software without specific, written prior
 *    permission. This software is supplied as is without expressed or
 *    implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 * Solaris code is encumbered by the following:
 *
 *     Copyright (C) 1996 by Sun Microsystems Computer Co.
 *
 *     Permission to use, copy, modify, and distribute this software and
 *     its documentation for any purpose and without fee is hereby
 *     granted, provided that the above copyright notice appear in all
 *     copies and that both that copyright notice and this permission
 *     notice appear in supporting documentation.  This software is
 *     provided "as is" without express or implied warranty.
 *
 * Research Systems Unix Group
 * The University of Michigan
 * c/o Wesley Craig
 * 535 W. William Street
 * Ann Arbor, Michigan
 * +1-313-764-2278
 * netatalk@umich.edu
 */
/*
 * None of the Solaris code mentioned is included in OpenBSD.
 * This code also relies heavily on previous effort in FreeBSD and NetBSD.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/timeout.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>
#include <net/if_llc.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/aarp.h>
#include <netatalk/phase2.h>
#include <netatalk/at_extern.h>

#include <dev/rndvar.h>

int	at_control( u_long, caddr_t, struct ifnet *, struct proc * );
static int at_scrub( struct ifnet *, struct at_ifaddr * );
static int at_ifinit( struct ifnet *, struct at_ifaddr *,
				struct sockaddr_at * );
int at_broadcast( struct sockaddr_at * );

static int aa_dorangeroute(struct ifaddr *, u_int, u_int, int);
static int aa_addsingleroute(struct ifaddr *, struct at_addr *,
					struct at_addr *);
static int aa_delsingleroute(struct ifaddr *, struct at_addr *,
					struct at_addr *);
static int aa_dosingleroute(struct ifaddr *, struct at_addr *,
					struct at_addr *, int, int );

# define sateqaddr(a,b)	((a)->sat_len == (b)->sat_len && \
		    (a)->sat_family == (b)->sat_family && \
		    (a)->sat_addr.s_net == (b)->sat_addr.s_net && \
		    (a)->sat_addr.s_node == (b)->sat_addr.s_node )

extern struct timeout aarpprobe_timeout;

int
at_control( cmd, data, ifp, p )
    u_long		cmd;
    caddr_t		data;
    struct ifnet	*ifp;
    struct proc		*p;
{
    struct ifreq	*ifr = (struct ifreq *)data;
    struct sockaddr_at	*sat;
    struct netrange	*nr;
    struct at_aliasreq	*ifra = (struct at_aliasreq *)data;
    struct at_ifaddr	*aa0;
    struct at_ifaddr	*aa = 0;
    struct ifaddr	*ifa0;

    if ( ifp ) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) break;
	}
    }

    switch ( cmd ) {
    case SIOCAIFADDR:
    case SIOCDIFADDR:
	if ( ifra->ifra_addr.sat_family == AF_APPLETALK ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			sateqaddr( &aa->aa_addr, &ifra->ifra_addr )) {
		    break;
		}
	    }
	}
	if ( cmd == SIOCDIFADDR && aa == 0 ) {
	    return( EADDRNOTAVAIL );
	}
	/*FALLTHROUGH*/

    case SIOCSIFADDR:
	/*
	 * What a great idea this is: Let's reverse the meaning of
	 * the return...
	 */
	if ( suser( p, 0 )) {
	    return( EPERM );
	}

	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {		/* default to phase 2 */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp && ( aa->aa_flags & AFA_PHASE2 )) {
		    break;
		}
	    }
	}

	if ( ifp == 0 )
	    panic( "at_control" );

	if ( aa == (struct at_ifaddr *) 0 ) {
	    aa0 = malloc(sizeof(*aa0), M_IFADDR, M_WAITOK | M_ZERO);

	    if (( aa = at_ifaddr ) != NULL ) {
		/*
		 * Don't let the loopback be first, since the first
		 * address is the machine's default address for
		 * binding.
		 */
		if ( at_ifaddr->aa_ifp->if_flags & IFF_LOOPBACK ) {
		    aa = aa0;
		    aa->aa_next = at_ifaddr;
		    at_ifaddr = aa;
		} else {
		    for ( ; aa->aa_next; aa = aa->aa_next )
		        ;
		    aa->aa_next = aa0;
		}
	    } else {
	        at_ifaddr = aa0;
	    }

	    aa = aa0;

	    /* FreeBSD found this. Whew */
	    aa->aa_ifa.ifa_refcnt++;

	    aa->aa_ifa.ifa_addr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_dstaddr = (struct sockaddr *)&aa->aa_addr;
	    aa->aa_ifa.ifa_netmask = (struct sockaddr *)&aa->aa_netmask;

	    /*
	     * Set/clear the phase 2 bit.
	     */
	    if ( nr->nr_phase == 1 ) {
		aa->aa_flags &= ~AFA_PHASE2;
	    } else {
		aa->aa_flags |= AFA_PHASE2;
	    }
	    aa->aa_ifp = ifp;

	    ifa_add(ifp, (struct ifaddr *)aa);
	} else {
	    at_scrub( ifp, aa );
	}
	break;

    case SIOCGIFADDR :
	sat = satosat( &ifr->ifr_addr );
	nr = (struct netrange *)sat->sat_zero;
	if ( nr->nr_phase == 1 ) {
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		    break;
		}
	    }
	} else {		/* default to phase 2 */
	    for ( ; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp && ( aa->aa_flags & AFA_PHASE2 )) {
		    break;
		}
	    }
	}

	if ( aa == (struct at_ifaddr *) 0 )
	    return( EADDRNOTAVAIL );
	break;
    }

    switch ( cmd ) {
    case SIOCGIFADDR:
	*(struct sockaddr_at *)&ifr->ifr_addr = aa->aa_addr;

	/* from FreeBSD : some cleanups about netranges */
	((struct netrange *)&sat->sat_zero)->nr_phase
		= (aa->aa_flags & AFA_PHASE2) ? 2 : 1;
	((struct netrange *)&sat->sat_zero)->nr_firstnet = aa->aa_firstnet;
	((struct netrange *)&sat->sat_zero)->nr_lastnet = aa->aa_lastnet;
	break;

    case SIOCSIFADDR:
	return( at_ifinit( ifp, aa, (struct sockaddr_at *)&ifr->ifr_addr ));

    case SIOCAIFADDR:
	if ( sateqaddr( &ifra->ifra_addr, &aa->aa_addr )) {
	    return( 0 );
	}
	return( at_ifinit( ifp, aa, (struct sockaddr_at *)&ifr->ifr_addr ));

    case SIOCDIFADDR:
	at_scrub( ifp, aa );
	ifa0 = (struct ifaddr *)aa;
	ifa_del(ifp, ifa0);

	/* FreeBSD */
	IFAFREE(ifa0);

	aa0 = aa;
	if ( aa0 == ( aa = at_ifaddr )) {
	    at_ifaddr = aa->aa_next;
	} else {
	    while ( aa->aa_next && ( aa->aa_next != aa0 )) {
	    	aa = aa->aa_next;
	    }
	    if ( aa->aa_next ) {
	    	aa->aa_next = aa0->aa_next;
	    } else {
	    	panic( "at_control" );
	    }
	}

	/* FreeBSD */
	IFAFREE(ifa0);
	break;

    default:
	if ( ifp == 0 || ifp->if_ioctl == 0 )
	    return( EOPNOTSUPP );
	return( (*ifp->if_ioctl)( ifp, cmd, data ));
    }
    return( 0 );
}

/* replaced this routine with the one from FreeBSD */
static int
at_scrub( ifp, aa )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
{
    int			error;

    if ( aa->aa_flags & AFA_ROUTE ) {
	if (ifp->if_flags & IFF_LOOPBACK) {
		if ((error = aa_delsingleroute(&aa->aa_ifa,
					&aa->aa_addr.sat_addr,
					&aa->aa_netmask.sat_addr))) {
			return( error );
		}
	} else if (ifp->if_flags & IFF_POINTOPOINT) {
		if ((error = rtinit( &aa->aa_ifa, RTM_DELETE, RTF_HOST)) != 0)
			return( error );
	} else if (ifp->if_flags & IFF_BROADCAST) {
		error = aa_dorangeroute(&aa->aa_ifa,
				ntohs(aa->aa_firstnet),
				ntohs(aa->aa_lastnet),
				RTM_DELETE );
	}
	aa->aa_ifa.ifa_flags &= ~IFA_ROUTE;
	aa->aa_flags &= ~AFA_ROUTE;
    }
    return( 0 );
}

static int
at_ifinit( ifp, aa, sat )
    struct ifnet	*ifp;
    struct at_ifaddr	*aa;
    struct sockaddr_at	*sat;
{
    struct netrange	nr, onr;
    struct sockaddr_at	oldaddr;
    int			s = splnet(), error = 0, i, j, netinc, nodeinc, nnets;
    u_int16_t		net;

    oldaddr = aa->aa_addr;
    bzero( AA_SAT( aa ), sizeof( struct sockaddr_at ));
    bcopy( sat->sat_zero, &nr, sizeof( struct netrange ));
    bcopy( sat->sat_zero, AA_SAT( aa )->sat_zero, sizeof( struct netrange ));
    nnets = ntohs( nr.nr_lastnet ) - ntohs( nr.nr_firstnet ) + 1;

    onr.nr_firstnet = aa->aa_firstnet;
    onr.nr_lastnet = aa->aa_lastnet;
    aa->aa_firstnet = nr.nr_firstnet;
    aa->aa_lastnet = nr.nr_lastnet;

    /*
     * We could eliminate the need for a second phase 1 probe (post
     * autoconf) if we check whether we're resetting the node. Note
     * that phase 1 probes use only nodes, not net.node pairs.  Under
     * phase 2, both the net and node must be the same.
     */
    if ( ifp->if_flags & IFF_LOOPBACK ) {
	AA_SAT( aa )->sat_len = sat->sat_len;
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	AA_SAT( aa )->sat_addr.s_net = sat->sat_addr.s_net;
	AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
    } else {
	aa->aa_flags |= AFA_PROBING;
	AA_SAT( aa )->sat_len = sizeof(struct sockaddr_at);
	AA_SAT( aa )->sat_family = AF_APPLETALK;
	if ( aa->aa_flags & AFA_PHASE2 ) {
	    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
		if ( nnets != 1 ) {
		    net = ntohs( nr.nr_firstnet ) +
		    	arc4random_uniform( nnets - 1 );
		} else {
		    net = ntohs( nr.nr_firstnet );
		}
	    } else {
		if ( ntohs( sat->sat_addr.s_net ) < ntohs( nr.nr_firstnet ) ||
			ntohs( sat->sat_addr.s_net ) > ntohs( nr.nr_lastnet )) {
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    splx(s);
		    return( EINVAL );
		}
		net = ntohs( sat->sat_addr.s_net );
	    }
	} else {
	    net = ntohs( sat->sat_addr.s_net );
	}

	if ( sat->sat_addr.s_node == ATADDR_ANYNODE ) {
	    AA_SAT( aa )->sat_addr.s_node = arc4random();
	} else {
	    AA_SAT( aa )->sat_addr.s_node = sat->sat_addr.s_node;
	}

	for ( i = nnets, netinc = 1; i > 0; net = ntohs( nr.nr_firstnet ) +
		(( net - ntohs( nr.nr_firstnet ) + netinc ) % nnets ), i-- ) {
	    AA_SAT( aa )->sat_addr.s_net = htons( net );

	    for ( j = 0, nodeinc = arc4random() | 1; j < 256;
		    j++, AA_SAT( aa )->sat_addr.s_node += nodeinc ) {
		if ( AA_SAT( aa )->sat_addr.s_node > 253 ||
			AA_SAT( aa )->sat_addr.s_node < 1 ) {
		    continue;
		}
		aa->aa_probcnt = 10;
		timeout_set(&aarpprobe_timeout, aarpprobe, ifp);
		/* XXX don't use hz so badly */
		timeout_add_msec(&aarpprobe_timeout, 200);
		if ( tsleep( aa, PPAUSE|PCATCH, "at_ifinit", 0 )) {
		    printf( "at_ifinit why did this happen?!\n" );
		    aa->aa_addr = oldaddr;
		    aa->aa_firstnet = onr.nr_firstnet;
		    aa->aa_lastnet = onr.nr_lastnet;
		    splx( s );
		    return( EINTR );
		}
		if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		    break;
		}
	    }
	    if (( aa->aa_flags & AFA_PROBING ) == 0 ) {
		break;
	    }
	    /* reset node for next network */
	    AA_SAT( aa )->sat_addr.s_node = arc4random();
	}

	if ( aa->aa_flags & AFA_PROBING ) {
	    aa->aa_addr = oldaddr;
	    aa->aa_firstnet = onr.nr_firstnet;
	    aa->aa_lastnet = onr.nr_lastnet;
	    splx( s );
	    return( EADDRINUSE );
	}
    }

    if ( ifp->if_ioctl &&
	    ( error = (*ifp->if_ioctl)( ifp, SIOCSIFADDR, (caddr_t) aa ))) {
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

    bzero(&aa->aa_netmask, sizeof(aa->aa_netmask));
    aa->aa_netmask.sat_len = sizeof(struct sockaddr_at);
    aa->aa_netmask.sat_family = AF_APPLETALK;
    aa->aa_netmask.sat_addr.s_net = 0xffff;
    aa->aa_netmask.sat_addr.s_node = 0;
    /* XXX From FreeBSD. Why does it do this? */
    aa->aa_ifa.ifa_netmask =(struct sockaddr *) &(aa->aa_netmask);

    /* This block came from FreeBSD too */
    /*
     * Initialize broadcast (or remote p2p) address
     */
    bzero(&aa->aa_broadaddr, sizeof(aa->aa_broadaddr));
    aa->aa_broadaddr.sat_len = sizeof(struct sockaddr_at);
    aa->aa_broadaddr.sat_family = AF_APPLETALK;

    aa->aa_ifa.ifa_metric = ifp->if_metric;
    if (ifp->if_flags & IFF_BROADCAST) {
	aa->aa_broadaddr.sat_addr.s_net = htons(0);
	aa->aa_broadaddr.sat_addr.s_node = 0xff;
	aa->aa_ifa.ifa_broadaddr = (struct sockaddr *) &aa->aa_broadaddr;
	/* add the range of routes needed */
	error = aa_dorangeroute(&aa->aa_ifa,
		ntohs(aa->aa_firstnet), ntohs(aa->aa_lastnet), RTM_ADD );
    }
    else if (ifp->if_flags & IFF_POINTOPOINT) {
	struct at_addr  rtaddr, rtmask;

	bzero(&rtaddr, sizeof(rtaddr));
	bzero(&rtmask, sizeof(rtmask));
	/* fill in the far end if we know it here XXX */
	aa->aa_ifa.ifa_dstaddr = (struct sockaddr *) &aa->aa_broadaddr;
	error = aa_addsingleroute(&aa->aa_ifa, &rtaddr, &rtmask);
    }
    else if ( ifp->if_flags & IFF_LOOPBACK ) {
	struct at_addr  rtaddr, rtmask;

	bzero(&rtaddr, sizeof(rtaddr));
	bzero(&rtmask, sizeof(rtmask));
	rtaddr.s_net = AA_SAT( aa )->sat_addr.s_net;
	rtaddr.s_node = AA_SAT( aa )->sat_addr.s_node;
	rtmask.s_net = 0xffff;
	rtmask.s_node = 0x0; /* XXX should not be so.. should be HOST route */
	error = aa_addsingleroute(&aa->aa_ifa, &rtaddr, &rtmask);
    }

    if ( error ) {
	at_scrub( ifp, aa );
	aa->aa_addr = oldaddr;
	aa->aa_firstnet = onr.nr_firstnet;
	aa->aa_lastnet = onr.nr_lastnet;
	splx( s );
	return( error );
    }

    aa->aa_ifa.ifa_flags |= IFA_ROUTE;
    aa->aa_flags |= AFA_ROUTE;
    splx( s );
    return( 0 );
}

int
at_broadcast( sat )
    struct sockaddr_at	*sat;
{
    struct at_ifaddr	*aa;

    if ( sat->sat_addr.s_node != ATADDR_BCAST ) {
	return( 0 );
    }
    if ( sat->sat_addr.s_net == ATADDR_ANYNET ) {
	return( 1 );
    } else {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if (( aa->aa_ifp->if_flags & IFF_BROADCAST ) &&
		 ( ntohs( sat->sat_addr.s_net ) >= ntohs( aa->aa_firstnet ) &&
		 ntohs( sat->sat_addr.s_net ) <= ntohs( aa->aa_lastnet ))) {
		return( 1 );
	    }
	}
    }
    return( 0 );
}

/* Yet another bunch of routines from FreeBSD. Those guys are good */
/*
 * aa_dorangeroute()
 *
 * Add a route for a range of networks from bot to top - 1.
 * Algorithm:
 *
 * Split the range into two subranges such that the middle
 * of the two ranges is the point where the highest bit of difference
 * between the two addresses, makes its transition
 * Each of the upper and lower ranges might not exist, or might be 
 * representable by 1 or more netmasks. In addition, if both
 * ranges can be represented by the same netmask, then they can be merged
 * by using the next higher netmask..
 */

static int
aa_dorangeroute(struct ifaddr *ifa, u_int bot, u_int top, int cmd)
{
	u_int mask1;
	struct at_addr addr;
	struct at_addr mask;
	int error;

	/*
	 * slight sanity check
	 */
	if (bot > top) return (EINVAL);

	addr.s_node = 0;
	mask.s_node = 0;
	/*
	 * just start out with the lowest boundary
	 * and keep extending the mask till it's too big.
	 */
	
	 while (bot <= top) {
	 	mask1 = 1;
	 	while ((( bot & ~mask1) >= bot)
		   && (( bot | mask1) <= top)) {
			mask1 <<= 1;
			mask1 |= 1;
		}
		mask1 >>= 1;
		mask.s_net = htons(~mask1);
		addr.s_net = htons(bot);
		if(cmd == RTM_ADD) {
		error =	 aa_addsingleroute(ifa,&addr,&mask);
			if (error) {
				/* XXX clean up? */
				return (error);
			}
		} else {
			error =	 aa_delsingleroute(ifa,&addr,&mask);
		}
		bot = (bot | mask1) + 1;
	}
	return 0;
}

static int
aa_addsingleroute(struct ifaddr *ifa,
	struct at_addr *addr, struct at_addr *mask)
{
  int	error;

#if 0
  printf("aa_addsingleroute: %x.%x mask %x.%x ...\n",
    ntohs(addr->s_net), addr->s_node,
    ntohs(mask->s_net), mask->s_node);
#endif

  error = aa_dosingleroute(ifa, addr, mask, RTM_ADD, RTF_UP);
  if (error)
    printf("aa_addsingleroute: error %d\n", error);
  return(error);
}

static int
aa_delsingleroute(struct ifaddr *ifa,
	struct at_addr *addr, struct at_addr *mask)
{
  int	error;

  error = aa_dosingleroute(ifa, addr, mask, RTM_DELETE, 0);
  if (error)
  	printf("aa_delsingleroute: error %d\n", error);
  return(error);
}

static int
aa_dosingleroute(struct ifaddr *ifa,
	struct at_addr *at_addr, struct at_addr *at_mask, int cmd, int flags)
{
  struct sockaddr_at	addr, mask;
  struct rt_addrinfo	info;

  bzero(&addr, sizeof(addr));
  bzero(&mask, sizeof(mask));
  bzero(&info, sizeof(info));
  addr.sat_family = AF_APPLETALK;
  addr.sat_len = sizeof(struct sockaddr_at);
  addr.sat_addr.s_net = at_addr->s_net;
  addr.sat_addr.s_node = at_addr->s_node;
  mask.sat_family = AF_APPLETALK;
  mask.sat_len = sizeof(struct sockaddr_at);
  mask.sat_addr.s_net = at_mask->s_net;
  mask.sat_addr.s_node = at_mask->s_node;
  info.rti_info[RTAX_DST] = (struct sockaddr *)&addr;
  info.rti_info[RTAX_NETMASK] = (struct sockaddr *)&mask;
  if (at_mask->s_node)
    flags |= RTF_HOST;
  info.rti_flags = flags;
  if (flags & RTF_HOST)
    info.rti_info[RTAX_GATEWAY] = ifa->ifa_dstaddr;
  else
    info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
  return(rtrequest1(cmd, &info, RTP_DEFAULT, NULL, 0));
}
