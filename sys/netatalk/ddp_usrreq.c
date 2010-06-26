/*	$OpenBSD: ddp_usrreq.c,v 1.13 2010/06/26 23:24:45 guenther Exp $	*/

/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
 * All Rights Reserved.  See COPYRIGHT.
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

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>

#include <machine/endian.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

int ddp_usrreq(struct socket *, int, struct mbuf *,
			struct mbuf *, struct mbuf *, struct proc *);
static void at_sockaddr( struct ddpcb *, struct mbuf * );
static int at_pcbsetaddr( struct ddpcb *, struct mbuf *,
			struct proc * );
static int at_pcbconnect( struct ddpcb *, struct mbuf *,
			struct proc *);
static void at_pcbdisconnect( struct ddpcb * );
static int at_pcballoc( struct socket * );
static void at_pcbdetach( struct socket *, struct ddpcb * );
struct ddpcb *ddp_search( struct sockaddr_at *,
			struct sockaddr_at *, struct at_ifaddr * );
void ddp_init(void);

struct at_ifaddr	*at_ifaddr;
struct ifqueue		atintrq1, atintrq2;
int			atdebug;

struct ddpcb		*ddp_ports[ ATPORT_LAST ];
struct ddpstat		ddpstat;

struct ddpcb	*ddpcb = NULL;
u_long		ddp_sendspace = DDP_MAXSZ; /* Max ddp size + 1 (ddp_type) */
u_long		ddp_recvspace = 10 * ( 587 + sizeof( struct sockaddr_at ));

/*ARGSUSED*/
int
ddp_usrreq( so, req, m, addr, rights, p )
    struct socket	*so;
    int			req;
    struct mbuf		*m, *addr, *rights;
    struct proc		*p;
{
    struct ddpcb	*ddp;
    int			error = 0;

    ddp = sotoddpcb( so );

    if ( req == PRU_CONTROL ) {
	return( at_control( (u_long) m, (caddr_t) addr,
		(struct ifnet *) rights, p ));
    }

    if ( rights && rights->m_len ) {
	error = EINVAL;
	goto release;
    }

    if ( ddp == NULL && req != PRU_ATTACH ) {
	error = EINVAL;
	goto release;
    }

    switch ( req ) {
    case PRU_ATTACH :
	if ( ddp != NULL ) {
	    error = EINVAL;
	    break;
	}
	if (( error = at_pcballoc( so )) != 0 ) {
	    break;
	}
	error = soreserve( so, ddp_sendspace, ddp_recvspace );
	break;

    case PRU_DETACH :
	at_pcbdetach( so, ddp );
	break;

    case PRU_BIND :
	error = at_pcbsetaddr( ddp, addr, p );
	break;
    
    case PRU_SOCKADDR :
	at_sockaddr( ddp, addr );
	break;

    case PRU_CONNECT:
	if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
	    error = EISCONN;
	    break;
	}

	error = at_pcbconnect( ddp, addr, p );
	if ( error == 0 )
	    soisconnected( so );
	break;

    case PRU_DISCONNECT:
	if ( ddp->ddp_fsat.sat_addr.s_node == ATADDR_ANYNODE ) {
	    error = ENOTCONN;
	    break;
	}
	at_pcbdisconnect( ddp );
	soisdisconnected( so );
	break;

    case PRU_SHUTDOWN:
	socantsendmore( so );
	break;

    case PRU_SEND: {
	int	s;

	if ( addr ) {
	    if ( ddp->ddp_fsat.sat_port != ATADDR_ANYPORT ) {
		error = EISCONN;
		break;
	    }

	    s = splnet();
	    error = at_pcbconnect( ddp, addr, p );
	    if ( error ) {
		splx( s );
		break;
	    }
	} else {
	    if ( ddp->ddp_fsat.sat_port == ATADDR_ANYPORT ) {
		error = ENOTCONN;
		break;
	    }
	}

	error = ddp_output( m, ddp );
	m = NULL;
	if ( addr ) {
	    at_pcbdisconnect( ddp );
	    splx( s );
	}
	}
	break;

    case PRU_ABORT:
	soisdisconnected( so );
	at_pcbdetach( so, ddp );
	break;

    case PRU_LISTEN:
    case PRU_CONNECT2:
    case PRU_ACCEPT:
    case PRU_SENDOOB:
    case PRU_FASTTIMO:
    case PRU_SLOWTIMO:
    case PRU_PROTORCV:
    case PRU_PROTOSEND:
	error = EOPNOTSUPP;
	break;

    case PRU_RCVD:
    case PRU_RCVOOB:
	/*
	 * Don't mfree. Good architecture...
	 */
	return( EOPNOTSUPP );

    case PRU_SENSE:
	/*
	 * 1. Don't return block size.
	 * 2. Don't mfree.
	 */
	return( 0 );

    default:
	error = EOPNOTSUPP;
    }

release:
    if ( m != NULL ) {
	m_freem( m );
    }
    return( error );
}

static void
at_sockaddr( ddp, addr )
    struct ddpcb	*ddp;
    struct mbuf		*addr;
{
    struct sockaddr_at	*sat;

    addr->m_len = sizeof( struct sockaddr_at );
    sat = mtod( addr, struct sockaddr_at *);
    *sat = ddp->ddp_lsat;
}

static int
at_pcbsetaddr( ddp, addr, p )
    struct ddpcb	*ddp;
    struct mbuf		*addr;
    struct proc		*p;
{
    struct sockaddr_at	lsat, *sat;
    struct at_ifaddr	*aa;
    struct ddpcb	*ddpp;

    if ( ddp->ddp_lsat.sat_port != ATADDR_ANYPORT ) { /* shouldn't be bound */
	return( EINVAL );
    }

    if ( addr != 0 ) {			/* validate passed address */
	sat = mtod( addr, struct sockaddr_at *);
	if ( addr->m_len != sizeof( *sat )) {
	    return( EINVAL );
	}
	if ( sat->sat_family != AF_APPLETALK ) {
	    return( EAFNOSUPPORT );
	}

	if ( sat->sat_addr.s_node != ATADDR_ANYNODE ||
		sat->sat_addr.s_net != ATADDR_ANYNET ) {
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if (( sat->sat_addr.s_net == AA_SAT( aa )->sat_addr.s_net ) &&
		 ( sat->sat_addr.s_node == AA_SAT( aa )->sat_addr.s_node )) {
		    break;
		}
	    }
	    if ( !aa ) {
		return( EADDRNOTAVAIL );
	    }
	}

	if ( sat->sat_port != ATADDR_ANYPORT ) {
	    if ( sat->sat_port < ATPORT_FIRST ||
		    sat->sat_port >= ATPORT_LAST ) {
		return( EINVAL );
	    }
	    if ( sat->sat_port < ATPORT_RESERVED &&
		    suser( p, 0 )) {
		return( EACCES );
	    }
	}
    } else {
	bzero( (caddr_t)&lsat, sizeof( struct sockaddr_at ));
	lsat.sat_family = AF_APPLETALK;
	sat = &lsat;
    }

    if ( sat->sat_addr.s_node == ATADDR_ANYNODE &&
	    sat->sat_addr.s_net == ATADDR_ANYNET ) {
	if ( at_ifaddr == NULL ) {
	    return( EADDRNOTAVAIL );
	}
	sat->sat_addr = AA_SAT( at_ifaddr )->sat_addr;
    }
    ddp->ddp_lsat = *sat;

    /*
     * Choose port.
     */
    if ( sat->sat_port == ATADDR_ANYPORT ) {
	for ( sat->sat_port = ATPORT_RESERVED;
		sat->sat_port < ATPORT_LAST; sat->sat_port++ ) {
	    if ( ddp_ports[ sat->sat_port - 1 ] == 0 ) {
		break;
	    }
	}
	if ( sat->sat_port == ATPORT_LAST ) {
	    return( EADDRNOTAVAIL );
	}
	ddp->ddp_lsat.sat_port = sat->sat_port;
	ddp_ports[ sat->sat_port - 1 ] = ddp;
    } else {
	for ( ddpp = ddp_ports[ sat->sat_port - 1 ]; ddpp;
		ddpp = ddpp->ddp_pnext ) {
	    if ( ddpp->ddp_lsat.sat_addr.s_net == sat->sat_addr.s_net &&
		    ddpp->ddp_lsat.sat_addr.s_node == sat->sat_addr.s_node ) {
		break;
	    }
	}
	if ( ddpp != NULL ) {
	    return( EADDRINUSE );
	}
	ddp->ddp_pnext = ddp_ports[ sat->sat_port - 1 ];
	ddp_ports[ sat->sat_port - 1 ] = ddp;
	if ( ddp->ddp_pnext ) {
	    ddp->ddp_pnext->ddp_pprev = ddp;
	}
    }

    return( 0 );
}

static int
at_pcbconnect( ddp, addr, p )
    struct ddpcb	*ddp;
    struct mbuf		*addr;
    struct proc		*p;
{
    struct sockaddr_at	*sat = mtod( addr, struct sockaddr_at *);
    struct route	*ro;
    struct at_ifaddr	*aa = 0;
    struct ifnet	*ifp;
    u_int16_t		hintnet = 0, net;

    if ( addr->m_len != sizeof( *sat ))
	return( EINVAL );
    if ( sat->sat_family != AF_APPLETALK ) {
	return( EAFNOSUPPORT );
    }

    /*
     * Under phase 2, network 0 means "the network".  We take "the
     * network" to mean the network the control block is bound to.
     * If the control block is not bound, there is an error.
     */
    if ( sat->sat_addr.s_net == 0 && sat->sat_addr.s_node != ATADDR_ANYNODE ) {
	if ( ddp->ddp_lsat.sat_port == ATADDR_ANYPORT ) {
	    return( EADDRNOTAVAIL );
	}
	hintnet = ddp->ddp_lsat.sat_addr.s_net;
    }

    ro = &ddp->ddp_route;
    /*
     * If we've got an old route for this pcb, check that it is valid.
     * If we've changed our address, we may have an old "good looking"
     * route here.  Attempt to detect it.
     */
    if ( ro->ro_rt ) {
	if ( hintnet ) {
	    net = hintnet;
	} else {
	    net = sat->sat_addr.s_net;
	}
	aa = 0;
	if ( (ifp = ro->ro_rt->rt_ifp) != NULL ) {
	    for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
		if ( aa->aa_ifp == ifp &&
			ntohs( net ) >= ntohs( aa->aa_firstnet ) &&
			ntohs( net ) <= ntohs( aa->aa_lastnet )) {
		    break;
		}
	    }
	}
	if ( aa == NULL || ( satosat( &ro->ro_dst )->sat_addr.s_net !=
		( hintnet ? hintnet : sat->sat_addr.s_net ) ||
		satosat( &ro->ro_dst )->sat_addr.s_node !=
		sat->sat_addr.s_node )) {
	    RTFREE( ro->ro_rt );
	    ro->ro_rt = (struct rtentry *)0;
	}
    }

    /*
     * If we've got no route for this interface, try to find one.
     */
    if ( ro->ro_rt == (struct rtentry *)0 ||
	 ro->ro_rt->rt_ifp == (struct ifnet *)0 ) {
	ro->ro_dst.sa_len = sizeof( struct sockaddr_at );
	ro->ro_dst.sa_family = AF_APPLETALK;
	if ( hintnet != 0 ) {
	    satosat( &ro->ro_dst )->sat_addr.s_net = hintnet;
	} else {
	    satosat( &ro->ro_dst )->sat_addr.s_net = sat->sat_addr.s_net;
	}
	satosat( &ro->ro_dst )->sat_addr.s_node = sat->sat_addr.s_node;
	rtalloc( ro );
    }

    /*
     * Make sure any route that we have has a valid interface.
     */
    aa = 0;
    if ( ro->ro_rt && ( ifp = ro->ro_rt->rt_ifp )) {
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp ) {
		break;
	    }
	}
    }
    if ( aa == 0 ) {
	return( ENETUNREACH );
    }

    ddp->ddp_fsat = *sat;
    if ( ddp->ddp_lsat.sat_port == ATADDR_ANYPORT ) {
	return( at_pcbsetaddr( ddp, (struct mbuf *)0, p ));
    }
    return( 0 );
}

static void
at_pcbdisconnect( ddp )
    struct ddpcb	*ddp;
{
    ddp->ddp_fsat.sat_addr.s_net = ATADDR_ANYNET;
    ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
    ddp->ddp_fsat.sat_port = ATADDR_ANYPORT;
}

static int
at_pcballoc( so )
    struct socket	*so;
{
    struct ddpcb	*ddp;

    ddp = malloc(sizeof(*ddp), M_PCB, M_NOWAIT | M_ZERO);
    if ( ddp == NULL ) {
	return (ENOBUFS);
    }

    ddp->ddp_lsat.sat_port = ATADDR_ANYPORT;

    ddp->ddp_next = ddpcb;
    ddp->ddp_prev = NULL;
    ddp->ddp_pprev = NULL;
    ddp->ddp_pnext = NULL;
    if ( ddpcb ) {
	ddpcb->ddp_prev = ddp;
    }
    ddpcb = ddp;

    ddp->ddp_socket = so;
    so->so_pcb = (caddr_t)ddp;
    return( 0 );
}

static void
at_pcbdetach( so, ddp )
    struct socket	*so;
    struct ddpcb	*ddp;
{
    soisdisconnected( so );
    so->so_pcb = 0;
    sofree( so );

    /* remove ddp from ddp_ports list */
    if ( ddp->ddp_lsat.sat_port != ATADDR_ANYPORT &&
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] != NULL ) {
	if ( ddp->ddp_pprev != NULL ) {
	    ddp->ddp_pprev->ddp_pnext = ddp->ddp_pnext;
	} else {
	    ddp_ports[ ddp->ddp_lsat.sat_port - 1 ] = ddp->ddp_pnext;
	}
	if ( ddp->ddp_pnext != NULL ) {
	    ddp->ddp_pnext->ddp_pprev = ddp->ddp_pprev;
	}
    }

    if ( ddp->ddp_route.ro_rt ) {
	rtfree( ddp->ddp_route.ro_rt );
    }

    if ( ddp->ddp_prev ) {
	ddp->ddp_prev->ddp_next = ddp->ddp_next;
    } else {
	ddpcb = ddp->ddp_next;
    }
    if ( ddp->ddp_next ) {
	ddp->ddp_next->ddp_prev = ddp->ddp_prev;
    }

    free( ddp, M_PCB );
}

/*
 * For the moment, this just find the pcb with the correct local address.
 * In the future, this will actually do some real searching, so we can use
 * the sender's address to do de-multiplexing on a single port to many
 * sockets (pcbs).
 */
struct ddpcb *
ddp_search( from, to, aa )
    struct sockaddr_at	*from, *to;
    struct at_ifaddr	*aa;
{
    struct ddpcb	*ddp;

    /*
     * Check for bad ports.
     */
    if ( to->sat_port < ATPORT_FIRST || to->sat_port >= ATPORT_LAST ) {
	return( NULL );
    }

    /*
     * Make sure the local address matches the sent address.  What about
     * the interface?
     */
    for ( ddp = ddp_ports[ to->sat_port - 1 ]; ddp; ddp = ddp->ddp_pnext ) {
	/* XXX should we handle 0.YY? */

	/* XXXX.YY to socket on destination interface */
	if ( to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net &&
		to->sat_addr.s_node == ddp->ddp_lsat.sat_addr.s_node ) {
	    break;
	}

	/* 0.255 to socket on receiving interface */
	if ( to->sat_addr.s_node == ATADDR_BCAST && ( to->sat_addr.s_net == 0 ||
		to->sat_addr.s_net == ddp->ddp_lsat.sat_addr.s_net ) &&
		ddp->ddp_lsat.sat_addr.s_net == AA_SAT( aa )->sat_addr.s_net ) {
	    break;
	}

	/* XXXX.0 to socket on destination interface */
	if ( to->sat_addr.s_net == aa->aa_firstnet &&
		to->sat_addr.s_node == 0 &&
		ntohs( ddp->ddp_lsat.sat_addr.s_net ) >=
		ntohs( aa->aa_firstnet ) &&
		ntohs( ddp->ddp_lsat.sat_addr.s_net ) <=
		ntohs( aa->aa_lastnet )) {
	    break;
	}
    }
    return( ddp );
}

void
ddp_init()
{
    atintrq1.ifq_maxlen = IFQ_MAXLEN;
    atintrq2.ifq_maxlen = IFQ_MAXLEN;
}

/*
 * Sysctl for ddp variables.
 */
int
ddp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DDPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ddpstat, sizeof(ddpstat)));

	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
