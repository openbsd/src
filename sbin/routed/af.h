/*	$OpenBSD: af.h,v 1.2 1996/06/23 14:32:25 deraadt Exp $	*/
/*	$NetBSD: af.h,v 1.8 1995/06/20 22:26:45 christos Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)af.h	8.1 (Berkeley) 6/5/93
 */

/*
 * Routing table management daemon.
 */

/*
 * Structure returned by af_hash routines.
 */
struct afhash {
	u_int	afh_hosthash;		/* host based hash */
	u_int	afh_nethash;		/* network based hash */
};

/*
 * Per address family routines.
 */
struct afswitch {
	/* returns keys based on address */
	void	(*af_hash) __P((struct sockaddr *, struct afhash *));
	/* verifies net # matching */
	int	(*af_netmatch) __P((struct sockaddr *, struct sockaddr *));
	/* interprets address for sending */
	void	(*af_output) __P((int, int, struct sockaddr *, int));
	/* packet from some other router? */
	int	(*af_portmatch) __P((struct sockaddr *));
	/* packet from privileged peer? */
	int	(*af_portcheck) __P((struct sockaddr *));
	/* tells if address is valid */
	int	(*af_checkhost) __P((struct sockaddr *));
	/* get flags for route (host or net) */
	int	(*af_rtflags) __P((struct sockaddr *));
	/* check bounds of subnet broadcast */
	int	(*af_sendroute) __P((struct rt_entry *, struct sockaddr *));
	/* canonicalize address for compares */
	void	(*af_canon) __P((struct sockaddr *));
	/* convert address to string */
	char	*(*af_format) __P((struct sockaddr *, char *, size_t));
	/* get address from packet */
#define DESTINATION	0
#define	GATEWAY		1
#define NETMASK		2
	int	(*af_get) __P((int, void *, struct sockaddr *));
	/* put address to packet */
	void	(*af_put) __P((void *, struct sockaddr *));
};

extern struct	afswitch afswitch[];	/* table proper */
extern int	af_max;			/* number of entries in table */
