/*	$OpenBSD: at_proto.c,v 1.2 2007/12/14 18:33:40 deraadt Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#undef s_net
#include <netinet/if_ether.h>

#include <netatalk/at.h>
#include <netatalk/ddp_var.h>
#include <netatalk/at_extern.h>

struct protosw		atalksw[] = {
    {
	/* Identifiers */
	SOCK_DGRAM,	&atalkdomain,	ATPROTO_DDP,	PR_ATOMIC|PR_ADDR,
	/*
	 * protocol-protocol interface.
	 * fields are pr_input, pr_output, pr_ctlinput, and pr_ctloutput.
	 * pr_input can be called from the udp protocol stack for iptalk
	 * packets bound for a local socket.
	 * pr_output can be used by higher level appletalk protocols, should
	 * they be included in the kernel.
	 */
	0,		ddp_output,	0,		0,
	/* socket-protocol interface. */
	ddp_usrreq,
	/* utility routines. */
	ddp_init,	0,		0,		0,	ddp_sysctl
    },
};

struct domain		atalkdomain = {
    AF_APPLETALK,	"appletalk",	0,	0,	0,	atalksw,
    &atalksw[ sizeof( atalksw ) / sizeof( atalksw[ 0 ] ) ],
    0, rn_inithead,
    8 * (u_long) &((struct sockaddr_at *) 0)->sat_addr,	/* dom_rtoffset */
    sizeof(struct sockaddr_at)
};
