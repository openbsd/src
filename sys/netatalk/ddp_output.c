/*	$OpenBSD: ddp_output.c,v 1.6 2003/06/06 09:45:08 itojun Exp $	*/

/*
 * Copyright (c) 1990,1991 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Mike Clark
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-763-0525
 *	netatalk@itd.umich.edu
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include <machine/endian.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

int ddp_output( struct mbuf *, ... );
u_int16_t at_cksum( struct mbuf *, int );
int ddp_route(struct mbuf *, struct route * );

int	ddp_cksum = 1;

int
ddp_output(struct mbuf *m, ...)
{
    struct ddpcb	*ddp;
    struct ddpehdr	*deh;
    va_list		ap;

    va_start(ap, m);
    ddp = va_arg(ap, struct ddpcb *);
    va_end(ap);

    M_PREPEND( m, sizeof( struct ddpehdr ), M_DONTWAIT );
    if (!m)
	return (ENOBUFS);

    deh = mtod( m, struct ddpehdr *);
    deh->deh_pad = 0;
    deh->deh_hops = 0;

    deh->deh_len = m->m_pkthdr.len;

    deh->deh_dnet = ddp->ddp_fsat.sat_addr.s_net;
    deh->deh_dnode = ddp->ddp_fsat.sat_addr.s_node;
    deh->deh_dport = ddp->ddp_fsat.sat_port;
    deh->deh_snet = ddp->ddp_lsat.sat_addr.s_net;
    deh->deh_snode = ddp->ddp_lsat.sat_addr.s_node;
    deh->deh_sport = ddp->ddp_lsat.sat_port;

    /*
     * The checksum calculation is done after all of the other bytes have
     * been filled in.
     */
    if ( ddp_cksum ) {
	deh->deh_sum = at_cksum( m, sizeof( int ));
    } else {
	deh->deh_sum = 0;
    }
    deh->deh_bytes = htonl( deh->deh_bytes );

    return( ddp_route( m, &ddp->ddp_route ));
}

u_int16_t
at_cksum( m, skip )
    struct mbuf	*m;
    int		skip;
{
    u_int8_t	*data, *end;
    u_long	cksum = 0;

    for (; m; m = m->m_next ) {
	for ( data = mtod( m, u_int8_t * ), end = data + m->m_len; data < end;
		data++ ) {
	    if ( skip ) {
		skip--;
		continue;
	    }
	    cksum = ( cksum + *data ) << 1;
	    if ( cksum & 0x00010000 ) {
		cksum++;
	    }
	    cksum &= 0x0000ffff;
	}
    }

    if ( cksum == 0 ) {
	cksum = 0x0000ffff;
    }
    return( (u_int16_t)cksum );
}

int
ddp_route( m, ro )
    struct mbuf		*m;
    struct route	*ro;
{
    struct sockaddr_at	gate;
    struct elaphdr	*elh;
    struct at_ifaddr	*aa = NULL;
    struct ifnet	*ifp;
    u_int16_t		net;

    if ( ro->ro_rt && ( ifp = ro->ro_rt->rt_ifp )) {
	net = satosat( ro->ro_rt->rt_gateway )->sat_addr.s_net;
	for ( aa = at_ifaddr; aa; aa = aa->aa_next ) {
	    if ( aa->aa_ifp == ifp &&
		    ntohs( net ) >= ntohs( aa->aa_firstnet ) &&
		    ntohs( net ) <= ntohs( aa->aa_lastnet )) {
		break;
	    }
	}
    }
    if ( aa == NULL ) {
	m_freem( m );
	return( EINVAL );
    }

    /*
     * There are several places in the kernel where data is added to
     * an mbuf without ensuring that the mbuf pointer is aligned.
     * This is bad for transition routing, since phase 1 and phase 2
     * packets end up poorly aligned due to the three byte elap header.
     */
    if ( aa->aa_flags & AFA_PHASE2 ) {
	if (( m = m_pullup( m, MIN( MHLEN, m->m_pkthdr.len ))) == 0 ) {
	    return( ENOBUFS );
	}
    } else {
	M_PREPEND(m, SZ_ELAPHDR, M_DONTWAIT);
	if (!m)
	    return (ENOBUFS);

	elh = mtod( m, struct elaphdr *);
	elh->el_snode = satosat( &aa->aa_addr )->sat_addr.s_node;
	elh->el_type = ELAP_DDPEXTEND;
	if ( ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) >=
		ntohs( aa->aa_firstnet ) &&
		ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) <=
		ntohs( aa->aa_lastnet )) {
	    elh->el_dnode = satosat( &ro->ro_dst )->sat_addr.s_node;
	} else {
	    elh->el_dnode = satosat( ro->ro_rt->rt_gateway )->sat_addr.s_node;
	}
    }

    if ( ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) >=
	    ntohs( aa->aa_firstnet ) &&
	    ntohs( satosat( &ro->ro_dst )->sat_addr.s_net ) <=
	    ntohs( aa->aa_lastnet )) {
	gate = *satosat( &ro->ro_dst );
    } else {
	gate = *satosat( ro->ro_rt->rt_gateway );
    }
    ro->ro_rt->rt_use++;

    /* XXX The NULL should be a struct rtentry */
    return((*ifp->if_output)( ifp, m, (struct sockaddr *) &gate, NULL ));
}
