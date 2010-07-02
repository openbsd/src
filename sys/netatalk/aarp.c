/*	$OpenBSD: aarp.c,v 1.10 2010/07/02 05:45:25 blambert Exp $	*/

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

#include <machine/endian.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/aarp.h>
#include <netatalk/ddp_var.h>
#include <netatalk/phase2.h>
#include <netatalk/at_extern.h>

static void aarptimer(void *);
struct ifaddr *at_ifawithnet(struct sockaddr_at *, struct ifaddr *);
static void aarpwhohas(struct arpcom *, struct sockaddr_at *);
int aarpresolve(struct arpcom *, struct mbuf *,
					struct sockaddr_at *, u_int8_t *);
void aarpinput(struct arpcom *, struct mbuf *);
static void at_aarpinput(struct arpcom *, struct mbuf *);
static void aarptfree(struct aarptab *);
struct aarptab *aarptnew(struct at_addr *);
void aarpprobe(void *);
void aarp_clean(void);

#ifdef GATEWAY
#define AARPTAB_BSIZ	16
#define AARPTAB_NB	37
#else
#define AARPTAB_BSIZ	9
#define AARPTAB_NB	19
#endif /* GATEWAY */

#define AARPTAB_SIZE	(AARPTAB_BSIZ * AARPTAB_NB)
struct aarptab		aarptab[AARPTAB_SIZE];
int			aarptab_size = AARPTAB_SIZE;

struct timeout	aarpprobe_timeout;
struct timeout	aarptimer_timeout;

#define AARPTAB_HASH(a) \
    ((((a).s_net << 8 ) + (a).s_node ) % AARPTAB_NB )

#define AARPTAB_LOOK(aat,addr) { \
    int		n; \
    aat = &aarptab[ AARPTAB_HASH(addr) * AARPTAB_BSIZ ]; \
    for ( n = 0; n < AARPTAB_BSIZ; n++, aat++ ) \
	if ( aat->aat_ataddr.s_net == (addr).s_net && \
	     aat->aat_ataddr.s_node == (addr).s_node ) \
	    break; \
	if ( n >= AARPTAB_BSIZ ) \
	    aat = 0; \
}

#define AARPT_AGE	(60 * 1)
#define AARPT_KILLC	20
#define AARPT_KILLI	3

u_int8_t	atmulticastaddr[ 6 ] = {
    0x09, 0x00, 0x07, 0xff, 0xff, 0xff,
};

u_int8_t	at_org_code[ 3 ] = {
    0x08, 0x00, 0x07,
};
u_int8_t	aarp_org_code[ 3 ] = {
    0x00, 0x00, 0x00,
};

/*ARGSUSED*/
static void
aarptimer(v)
	void *v;
{
    struct aarptab	*aat;
    int			i, s;

    timeout_add_sec(&aarptimer_timeout, AARPT_AGE);
    aat = aarptab;
    for ( i = 0; i < AARPTAB_SIZE; i++, aat++ ) {
	if ( aat->aat_flags == 0 || ( aat->aat_flags & ATF_PERM ))
	    continue;
	if ( ++aat->aat_timer < (( aat->aat_flags & ATF_COM ) ?
		AARPT_KILLC : AARPT_KILLI ))
	    continue;
	s = splnet();
	aarptfree( aat );
	splx( s );
    }
}

struct ifaddr *
at_ifawithnet( sat, ifa )
    struct sockaddr_at	*sat;
    struct ifaddr	*ifa;
{
    struct sockaddr_at  *sat2;
    struct netrange     *nr;

    for (; ifa; ifa = TAILQ_NEXT(ifa, ifa_list)) {
	if ( ifa->ifa_addr->sa_family != AF_APPLETALK ) {
	    continue;
	}
	sat2 = satosat( ifa->ifa_addr );
	if ( sat2->sat_addr.s_net == sat->sat_addr.s_net ) {
	    break;
	}
	nr = (struct netrange *)(sat2->sat_zero);
	if( (nr->nr_phase == 2 )
	 && (ntohs(nr->nr_firstnet) <= ntohs(sat->sat_addr.s_net))
	 && (ntohs(nr->nr_lastnet) >= ntohs(sat->sat_addr.s_net))) {
	    break;
	}
    }
    return( ifa );
}

static void
aarpwhohas( ac, sat )
    struct arpcom	*ac;
    struct sockaddr_at	*sat;
{
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    if (( m = m_gethdr( M_DONTWAIT, MT_DATA )) == NULL ) {
	return;
    }
    m->m_len = sizeof( *ea );
    m->m_pkthdr.len = sizeof( *ea );
    MH_ALIGN( m, sizeof( *ea ));

    ea = mtod( m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof( *ea ));

    ea->aarp_hrd = htons( AARPHRD_ETHER );
    ea->aarp_pro = htons( ETHERTYPE_AT );
    ea->aarp_hln = sizeof( ea->aarp_sha );
    ea->aarp_pln = sizeof( ea->aarp_spu );
    ea->aarp_op = htons( AARPOP_REQUEST );
    bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    if (( aa = (struct at_ifaddr *)
    	    at_ifawithnet( sat, TAILQ_FIRST(&ac->ac_if.if_addrlist))) == NULL ) {
	m_freem( m );
	return;
    }

    eh = (struct ether_header *)sa.sa_data;

    if ( aa->aa_flags & AFA_PHASE2 ) {
	bcopy((caddr_t)atmulticastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons(AT_LLC_SIZE + sizeof(struct ether_aarp));
	M_PREPEND( m, AT_LLC_SIZE, M_DONTWAIT );
	if (!m)
	    return;

	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );
	
	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_spnet,
		sizeof( ea->aarp_spnet ));
	ea->aarp_spnode = AA_SAT( aa )->sat_addr.s_node;
	bcopy( &sat->sat_addr.s_net, ea->aarp_tpnet,
		sizeof( ea->aarp_tpnet ));
	ea->aarp_tpnode = sat->sat_addr.s_node;
    } else {
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( ETHERTYPE_AARP );

	ea->aarp_spa = AA_SAT( aa )->sat_addr.s_node;
	ea->aarp_tpa = sat->sat_addr.s_node;
    }

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    /* XXX The NULL should be a struct rtentry. TBD */
    (*ac->ac_if.if_output)(&ac->ac_if, m, &sa , NULL);
}

int
aarpresolve( ac, m, destsat, desten )
    struct arpcom	*ac;
    struct mbuf		*m;
    struct sockaddr_at	*destsat;
    u_int8_t		*desten;
{
    struct at_ifaddr	*aa;
    struct aarptab	*aat;
    int			s;

    if ( at_broadcast( destsat )) {
	if (( aa = (struct at_ifaddr *)at_ifawithnet( destsat,
		TAILQ_FIRST(&((struct ifnet *)ac)->if_addrlist))) == NULL ) {
	    m_freem( m );
	    return( 0 );
	}
	if ( aa->aa_flags & AFA_PHASE2 ) {
	    bcopy( (caddr_t)atmulticastaddr, (caddr_t)desten,
		    sizeof( atmulticastaddr ));
	} else {
	    bcopy( (caddr_t)etherbroadcastaddr, (caddr_t)desten,
		    sizeof( etherbroadcastaddr ));
	}
	return( 1 );
    }

    s = splnet();
    AARPTAB_LOOK( aat, destsat->sat_addr );
    if ( aat == 0 ) {			/* No entry */
	aat = aarptnew( &destsat->sat_addr );
	if ( aat == 0 ) { /* XXX allocate more */
	    panic( "aarpresolve: no free entry" );
	}
	aat->aat_hold = m;
	aarpwhohas( ac, destsat );
	splx( s );
	return( 0 );
    }
    /* found an entry */
    aat->aat_timer = 0;
    if ( aat->aat_flags & ATF_COM ) {	/* entry is COMplete */
	bcopy( (caddr_t)aat->aat_enaddr, (caddr_t)desten,
		sizeof( aat->aat_enaddr ));
	splx( s );
	return( 1 );
    }
    /* entry has not completed */
    if ( aat->aat_hold ) {
	m_freem( aat->aat_hold );
    }
    aat->aat_hold = m;
    aarpwhohas( ac, destsat );
    splx( s );
    return( 0 );
}

void
aarpinput( ac, m )
    struct arpcom	*ac;
    struct mbuf		*m;
{
    struct arphdr	*ar;

    if ( ac->ac_if.if_flags & IFF_NOARP )
	goto out;

    if ( m->m_len < sizeof( struct arphdr )) {
	goto out;
    }

    ar = mtod( m, struct arphdr *);
    if ( ntohs( ar->ar_hrd ) != AARPHRD_ETHER ) {
	goto out;
    }
    
    if ( m->m_len < sizeof( struct arphdr ) + 2 * ar->ar_hln +
	    2 * ar->ar_pln ) {
	goto out;
    }
    
    switch( ntohs( ar->ar_pro )) {
    case ETHERTYPE_AT :
	at_aarpinput( ac, m );
	return;

    default:
	break;
    }

out:
    m_freem( m );
}


static void
at_aarpinput( ac, m )
    struct arpcom	*ac;
    struct mbuf		*m;
{
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct aarptab	*aat;
    struct ether_header	*eh;
    struct llc		*llc;
    struct sockaddr_at	sat;
    struct sockaddr	sa;
    struct at_addr	spa, tpa, ma;
    int			op;
    u_int16_t		net;

    ea = mtod( m, struct ether_aarp *);

    /* Check to see if from my hardware address */
    if ( !bcmp(( caddr_t )ea->aarp_sha, ( caddr_t )ac->ac_enaddr,
	    sizeof( ac->ac_enaddr ))) {
	m_freem( m );
	return;
    }

    /*
     * Check if from broadcast address.  This could be a more robust
     * check, since we could look for multicasts. XXX
     */
    if ( !bcmp(( caddr_t )ea->aarp_sha, ( caddr_t )etherbroadcastaddr,
	    sizeof( etherbroadcastaddr ))) {
	log( LOG_ERR,
		"aarp: source is broadcast!\n" );
	m_freem( m );
	return;
    }

    op = ntohs( ea->aarp_op );
    bcopy( ea->aarp_tpnet, &net, sizeof( net ));

    if ( net != 0 ) {
	sat.sat_len = sizeof(struct sockaddr_at);
	sat.sat_family = AF_APPLETALK;
	sat.sat_addr.s_net = net;
	if (( aa = (struct at_ifaddr *)at_ifawithnet( &sat,
		TAILQ_FIRST(&ac->ac_if.if_addrlist))) == NULL ) {
	    m_freem( m );
	    return;
	}
	bcopy( ea->aarp_spnet, &spa.s_net, sizeof( spa.s_net ));
	bcopy( ea->aarp_tpnet, &tpa.s_net, sizeof( tpa.s_net ));
    } else {
	/*
	 * Since we don't know the net, we just look for the first
	 * phase 1 address on the interface.
	 */
	for ( aa = (struct at_ifaddr *)TAILQ_FIRST(&ac->ac_if.if_addrlist); aa;
		aa = (struct at_ifaddr *)TAILQ_NEXT(&aa->aa_ifa, ifa_list)) {
	    if ( AA_SAT( aa )->sat_family == AF_APPLETALK &&
		    ( aa->aa_flags & AFA_PHASE2 ) == 0 ) {
		break;
	    }
	}
	if ( aa == NULL ) {
	    m_freem( m );
	    return;
	}
	tpa.s_net = spa.s_net = AA_SAT( aa )->sat_addr.s_net;
    }

    spa.s_node = ea->aarp_spnode;
    tpa.s_node = ea->aarp_tpnode;
    ma.s_net = AA_SAT( aa )->sat_addr.s_net;
    ma.s_node = AA_SAT( aa )->sat_addr.s_node;

    /*
     * This looks like it's from us.
     */
    if ( spa.s_net == ma.s_net && spa.s_node == ma.s_node ) {
	if ( aa->aa_flags & AFA_PROBING ) {
	    /*
	     * We're probing, someone either responded to our probe, or
	     * probed for the same address we'd like to use. Change the
	     * address we're probing for.
	     */
	    timeout_del(&aarpprobe_timeout);
	    wakeup( aa );
	    m_freem( m );
	    return;
	} else if ( op != AARPOP_PROBE ) {
	    /*
	     * This is not a probe, and we're not probing. This means
	     * that someone's saying they have the same source address
	     * as the one we're using. Get upset...
	     */
	    /* XXX use ether_ntoa */
	    log( LOG_ERR,
		    "aarp: duplicate AT address!! %x:%x:%x:%x:%x:%x\n",
	    	    ea->aarp_sha[ 0 ], ea->aarp_sha[ 1 ], ea->aarp_sha[ 2 ],
		    ea->aarp_sha[ 3 ], ea->aarp_sha[ 4 ], ea->aarp_sha[ 5 ]);
	    m_freem( m );
	    return;
	}
    }

    AARPTAB_LOOK( aat, spa );
    if ( aat ) {
	if ( op == AARPOP_PROBE ) {
	    /*
	     * Someone's probing for spa, dealocate the one we've got,
	     * so that if the prober keeps the address, we'll be able
	     * to arp for him.
	     */
	    aarptfree( aat );
	    m_freem( m );
	    return;
	}

	bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )aat->aat_enaddr,
		sizeof( ea->aarp_sha ));
	aat->aat_flags |= ATF_COM;
	if ( aat->aat_hold ) {
	    sat.sat_len = sizeof(struct sockaddr_at);
	    sat.sat_family = AF_APPLETALK;
	    sat.sat_addr = spa;
	    /* XXX the NULL should be a struct rtentry */
	    (*ac->ac_if.if_output)( &ac->ac_if, aat->aat_hold,
		    (struct sockaddr *)&sat, NULL );
	    aat->aat_hold = 0;
	}
    }

    if ( aat == 0 && tpa.s_net == ma.s_net && tpa.s_node == ma.s_node
	    && op != AARPOP_PROBE ) {
	if ( (aat = aarptnew( &spa ))) {
	    bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )aat->aat_enaddr,
		    sizeof( ea->aarp_sha ));
	    aat->aat_flags |= ATF_COM;
	}
    }

    /*
     * Don't respond to responses, and never respond if we're
     * still probing.
     */
    if ( tpa.s_net != ma.s_net || tpa.s_node != ma.s_node ||
	    op == AARPOP_RESPONSE || ( aa->aa_flags & AFA_PROBING )) {
	m_freem( m );
	return;
    }

    bcopy(( caddr_t )ea->aarp_sha, ( caddr_t )ea->aarp_tha,
	    sizeof( ea->aarp_sha ));
    bcopy(( caddr_t )ac->ac_enaddr, ( caddr_t )ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    /* XXX FreeBSD has an 'XXX' here but no comment as to why. */
    eh = (struct ether_header *)sa.sa_data;
    bcopy(( caddr_t )ea->aarp_tha, ( caddr_t )eh->ether_dhost,
	    sizeof( eh->ether_dhost ));

    if ( aa->aa_flags & AFA_PHASE2 ) {
	eh->ether_type = htons( AT_LLC_SIZE +
		sizeof( struct ether_aarp ));
	M_PREPEND( m, AT_LLC_SIZE, M_DONTWAIT );
	if ( m == NULL ) {
	    return;
	}
	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );

	bcopy( ea->aarp_spnet, ea->aarp_tpnet, sizeof( ea->aarp_tpnet ));
	bcopy( &ma.s_net, ea->aarp_spnet, sizeof( ea->aarp_spnet ));
    } else {
	eh->ether_type = htons( ETHERTYPE_AARP );
    }

    ea->aarp_tpnode = ea->aarp_spnode;
    ea->aarp_spnode = ma.s_node;
    ea->aarp_op = htons( AARPOP_RESPONSE );

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    /* XXX the NULL should be a struct rtentry */
    (*ac->ac_if.if_output)( &ac->ac_if, m, &sa, NULL );
    return;
}

static void
aarptfree( aat )
    struct aarptab	*aat;
{

    if ( aat->aat_hold )
	m_freem( aat->aat_hold );
    aat->aat_hold = 0;
    aat->aat_timer = aat->aat_flags = 0;
    aat->aat_ataddr.s_net = 0;
    aat->aat_ataddr.s_node = 0;
}

struct aarptab *
aarptnew( addr )
    struct at_addr	*addr;
{
    int			n;
    int			oldest = -1;
    struct aarptab	*aat, *aato = NULL;
    static int		first = 1;

    if ( first ) {
	first = 0;
	timeout_set(&aarptimer_timeout, aarptimer, NULL);
	timeout_add_sec(&aarptimer_timeout, 1);
    }
    aat = &aarptab[ AARPTAB_HASH( *addr ) * AARPTAB_BSIZ ];
    for ( n = 0; n < AARPTAB_BSIZ; n++, aat++ ) {
	if ( aat->aat_flags == 0 )
	    goto out;
	if ( aat->aat_flags & ATF_PERM )
	    continue;
	if ((int) aat->aat_timer > oldest ) {
	    oldest = aat->aat_timer;
	    aato = aat;
	}
    }
    if ( aato == NULL )
	return( NULL );
    aat = aato;
    aarptfree( aat );
out:
    aat->aat_ataddr = *addr;
    aat->aat_flags = ATF_INUSE;
    return( aat );
}

void
aarpprobe( arg )
    void	*arg;
{
    struct arpcom	*ac = (struct arpcom *) arg;
    struct mbuf		*m;
    struct ether_header	*eh;
    struct ether_aarp	*ea;
    struct at_ifaddr	*aa;
    struct llc		*llc;
    struct sockaddr	sa;

    /*
     * We need to check whether the output ethernet type should
     * be phase 1 or 2. We have the interface that we'll be sending
     * the aarp out. We need to find an AppleTalk network on that
     * interface with the same address as we're looking for. If the
     * net is phase 2, generate an 802.2 and SNAP header.
     */
    for ( aa = (struct at_ifaddr *)TAILQ_FIRST(&ac->ac_if.if_addrlist); aa;
	    aa = (struct at_ifaddr *)TAILQ_NEXT(&aa->aa_ifa, ifa_list)) {
	if ( AA_SAT( aa )->sat_family == AF_APPLETALK &&
		( aa->aa_flags & AFA_PROBING )) {
	    break;
	}
    }
    if ( aa == NULL ) {		/* serious error XXX */
	printf( "aarpprobe why did this happen?!\n" );
	return;
    }

    if ( aa->aa_probcnt <= 0 ) {
	aa->aa_flags &= ~AFA_PROBING;
	wakeup( aa );
	return;
    } else {
	timeout_set(&aarpprobe_timeout, aarpprobe, ac);
	timeout_add_msec(&aarpprobe_timeout, 200);
    }

    if (( m = m_gethdr( M_DONTWAIT, MT_DATA )) == NULL ) {
	return;
    }
    m->m_len = sizeof( *ea );
    m->m_pkthdr.len = sizeof( *ea );
    MH_ALIGN( m, sizeof( *ea ));

    ea = mtod( m, struct ether_aarp *);
    bzero((caddr_t)ea, sizeof( *ea ));

    ea->aarp_hrd = htons( AARPHRD_ETHER );
    ea->aarp_pro = htons( ETHERTYPE_AT );
    ea->aarp_hln = sizeof( ea->aarp_sha );
    ea->aarp_pln = sizeof( ea->aarp_spu );
    ea->aarp_op = htons( AARPOP_PROBE );
    bcopy((caddr_t)ac->ac_enaddr, (caddr_t)ea->aarp_sha,
	    sizeof( ea->aarp_sha ));

    eh = (struct ether_header *)sa.sa_data;

    if ( aa->aa_flags & AFA_PHASE2 ) {
	bcopy((caddr_t)atmulticastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( AT_LLC_SIZE +
		sizeof( struct ether_aarp ));
	M_PREPEND( m, AT_LLC_SIZE, M_DONTWAIT );
	if (!m)
	    return;

	llc = mtod( m, struct llc *);
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	bcopy( aarp_org_code, llc->llc_org_code, sizeof( aarp_org_code ));
	llc->llc_ether_type = htons( ETHERTYPE_AARP );

	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_spnet,
		sizeof( ea->aarp_spnet ));
	bcopy( &AA_SAT( aa )->sat_addr.s_net, ea->aarp_tpnet,
		sizeof( ea->aarp_tpnet ));
	ea->aarp_spnode = ea->aarp_tpnode = AA_SAT( aa )->sat_addr.s_node;
    } else {
	bcopy((caddr_t)etherbroadcastaddr, (caddr_t)eh->ether_dhost,
		sizeof( eh->ether_dhost ));
	eh->ether_type = htons( ETHERTYPE_AARP );
	ea->aarp_spa = ea->aarp_tpa = AA_SAT( aa )->sat_addr.s_node;
    }

    sa.sa_len = sizeof( struct sockaddr );
    sa.sa_family = AF_UNSPEC;
    /* XXX the NULL should be a struct rtentry */
    (*ac->ac_if.if_output)(&ac->ac_if, m, &sa, NULL );
    aa->aa_probcnt--;
}

void
aarp_clean(void)
{
    struct aarptab	*aat;
    int			i;

    timeout_del(&aarptimer_timeout);
    for ( i = 0, aat = aarptab; i < AARPTAB_SIZE; i++, aat++ ) {
	if ( aat->aat_hold ) {
	    m_freem( aat->aat_hold );
	}
    }
}
