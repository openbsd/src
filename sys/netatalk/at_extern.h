/*	$OpenBSD: at_extern.h,v 1.3 2002/03/14 01:27:10 millert Exp $	*/
/*      $NetBSD: at_extern.h,v 1.3 1997/04/03 18:38:23 christos Exp $   */

/*
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
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
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
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
 * This file in particular came from NetBSD.
 */

#ifndef _NETATALK_AT_EXTERN_H_
#define _NETATALK_AT_EXTERN_H_

struct ifnet;
struct mbuf;
struct sockaddr_at;
struct proc;
struct at_ifaddr;
struct route;
struct socket;

void		atintr(void);
void		aarpprobe(void *);
int		aarpresolve(struct arpcom *, struct mbuf *,
				struct sockaddr_at *, u_int8_t *);
void		aarpinput(struct arpcom *, struct mbuf *);
int		at_broadcast(struct sockaddr_at  *);
void		aarp_clean(void);
int		at_control(u_long, caddr_t, struct ifnet *,
				struct proc *);
u_int16_t	at_cksum(struct mbuf *, int);
int		ddp_usrreq(struct socket *, int,
				struct mbuf *, struct mbuf *,
				struct mbuf *);
void		ddp_init(void );
struct ifaddr 	*at_ifawithnet(struct sockaddr_at *, struct ifaddr *);
int		ddp_output(struct mbuf *, ...);
struct ddpcb  	*ddp_search(struct sockaddr_at *,
				struct sockaddr_at *, struct at_ifaddr *);
int		ddp_route(struct mbuf *, struct route *);

#endif /* _NETATALK_AT_EXTERN_H_ */
