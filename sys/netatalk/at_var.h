/*	$OpenBSD: at_var.h,v 1.2 2007/05/26 12:09:40 claudio Exp $	*/

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

#ifndef _NETATALK_AT_VAR_H_
#define _NETATALK_AT_VAR_H_ 1

/*
 * For phase2, we need to keep not only our address on an interface,
 * but also the legal networks on the interface.
 */
struct at_ifaddr {
    struct ifaddr	aa_ifa;
# define aa_ifp			aa_ifa.ifa_ifp
    struct sockaddr_at	aa_addr;
    struct sockaddr_at	aa_broadaddr;
    struct sockaddr_at	aa_netmask;
    int			aa_flags;
    u_short		aa_firstnet, aa_lastnet;
    int			aa_probcnt;
    struct at_ifaddr	*aa_next;
};

struct at_aliasreq {
	char	ifra_name[IFNAMSIZ];		/* if name, e.g. "en0" */
	struct	sockaddr_at ifra_addr;
	struct	sockaddr_at ifra_broadaddr;
#define ifra_dstaddr ifra_broadaddr
	struct	sockaddr_at ifra_mask;
};

#define AA_SAT(aa) \
    ((struct sockaddr_at *)&((struct at_ifaddr *)(aa))->aa_addr)
#define satosat(sa)	((struct sockaddr_at *)(sa))

#define AFA_ROUTE	0x0001
#define AFA_PROBING	0x0002
#define AFA_PHASE2	0x0004

#ifdef _KERNEL
extern struct at_ifaddr	*at_ifaddr;
extern struct ifqueue	atintrq1, atintrq2;
extern int		atdebug;
#endif

#endif /* _NETATALK_AT_VAR_H_ */
