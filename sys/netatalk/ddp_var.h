/*	$OpenBSD: ddp_var.h,v 1.2 2007/05/26 12:09:40 claudio Exp $	*/

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

#ifndef _NETATALK_DDP_VAR_H_
#define _NETATALK_DDP_VAR_H_
struct ddpcb {
    struct sockaddr_at	ddp_fsat, ddp_lsat;
    struct route	ddp_route;
    struct socket	*ddp_socket;
    struct ddpcb	*ddp_prev, *ddp_next;
    struct ddpcb	*ddp_pprev, *ddp_pnext;
};

#define sotoddpcb(so)	((struct ddpcb *)(so)->so_pcb)

struct ddpstat {
    u_long	ddps_short;		/* short header packets received */
    u_long	ddps_long;		/* long header packets received */
    u_long	ddps_nosum;		/* no checksum */
    u_long	ddps_badsum;		/* bad checksum */
    u_long	ddps_tooshort;		/* packet too short */
    u_long	ddps_toosmall;		/* not enough data */
    u_long	ddps_forward;		/* packets forwarded */
    u_long	ddps_cantforward;	/* packets rcvd for unreachable dest */
    u_long	ddps_nosockspace;	/* no space in sockbuf for packet */
};

#ifdef _KERNEL
extern struct ddpcb		*ddp_ports[ ATPORT_LAST ];
extern struct ddpcb		*ddpcb;
extern struct ddpstat		ddpstat;
#endif

#endif /* _NETATALK_DDP_VAR_H_ */
