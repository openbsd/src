/*	$NetBSD: af.c,v 1.12 1995/07/24 13:03:25 ws Exp $	*/

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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)af.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$NetBSD: af.c,v 1.12 1995/07/24 13:03:25 ws Exp $";
#endif
#endif /* not lint */

#include "defs.h"

/*
 * Address family support routines
 */
static void inet_canon __P((struct sockaddr *));
static int inet_checkhost __P((struct sockaddr *));
static char *inet_format __P((struct sockaddr *, char *buf, size_t sz));
static void inet_hash __P((struct sockaddr *, struct afhash *));
static int inet_netmatch __P((struct sockaddr *, struct sockaddr *));
static int inet_portcheck __P((struct sockaddr *));
static int inet_portmatch __P((struct sockaddr *));
static void inet_output __P((int, int, struct sockaddr *, int));
static int inet_get __P((int, void *, struct sockaddr *));
static void inet_put __P((void *, struct sockaddr *));

#define NIL	{ 0 }
#define	INET \
	{ inet_hash,		inet_netmatch,		inet_output, \
	  inet_portmatch,	inet_portcheck,		inet_checkhost, \
	  inet_rtflags,		inet_sendroute,		inet_canon, \
	  inet_format,		inet_get,		inet_put \
	}

struct afswitch afswitch[AF_MAX] = {
	NIL,		/* 0- unused */
	NIL,		/* 1- Unix domain, unused */
	INET,		/* Internet */
};

int af_max = sizeof(afswitch) / sizeof(afswitch[0]);

struct sockaddr_in inet_default = {
#ifdef RTM_ADD
	sizeof (inet_default),
#endif
	AF_INET, INADDR_ANY };

static void
inet_hash(sa, hp)
	struct sockaddr *sa;
	struct afhash *hp;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
	register u_long n;

	n = inet_netof_subnet(sin->sin_addr);
	if (n)
	    while ((n & 0xff) == 0)
		n >>= 8;
	hp->afh_nethash = n;
	hp->afh_hosthash = ntohl(sin->sin_addr.s_addr);
	hp->afh_hosthash &= 0x7fffffff;
}

static int
inet_netmatch(sa1, sa2)
	struct sockaddr *sa1, *sa2;
{
	struct sockaddr_in *sin1 = (struct sockaddr_in *) sa1;
	struct sockaddr_in *sin2 = (struct sockaddr_in *) sa2;

	return (inet_netof_subnet(sin1->sin_addr) ==
		inet_netof_subnet(sin2->sin_addr));
}

/*
 * Verify the message is from the right port.
 */
static int
inet_portmatch(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
	
	return (sin->sin_port == sp->s_port);
}

/*
 * Verify the message is from a "trusted" port.
 */
static int
inet_portcheck(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	return (ntohs(sin->sin_port) <= IPPORT_RESERVED);
}

/*
 * Internet output routine.
 */
static void
inet_output(s, flags, sa, size)
	int s, flags;
	struct sockaddr *sa;
	int size;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
	struct sockaddr_in dst;

	dst = *sin;
	sin = &dst;
	if (sin->sin_port == 0)
		sin->sin_port = sp->s_port;
	if (sin->sin_len == 0)
		sin->sin_len = sizeof (*sin);
	if (sendto(s, packet, size, flags,
	    (struct sockaddr *)sin, sizeof (*sin)) < 0)
		perror("sendto");
}

/*
 * Return 1 if the address is believed
 * for an Internet host -- THIS IS A KLUDGE.
 */
static int
inet_checkhost(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
	u_long i = ntohl(sin->sin_addr.s_addr);

#ifndef IN_EXPERIMENTAL
#define	IN_EXPERIMENTAL(i)	(((long) (i) & 0xe0000000) == 0xe0000000)
#endif

	if (IN_EXPERIMENTAL(i) || sin->sin_port != 0)
		return (0);
	if (i != 0 && (i & 0xff000000) == 0)
		return (0);
	for (i = 0; i < sizeof(sin->sin_zero)/sizeof(sin->sin_zero[0]); i++)
		if (sin->sin_zero[i])
			return (0);
	return (1);
}

static void
inet_canon(sa)
	struct sockaddr *sa;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	sin->sin_port = 0;
	sin->sin_len = sizeof(*sin);
}

static char *
inet_format(sa, buf, sz)
	struct sockaddr *sa;
	char *buf; size_t sz;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;

	strncpy(buf, inet_ntoa(sin->sin_addr), sz);
	buf[sz - 1] = '\0';
	return buf;
}

static int
inet_get(what, pck, sa)
	int what;
	void *pck;
	struct sockaddr *sa;
{
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
	struct netinfo *n = pck;
	/* XXX: Internet only */
	memset(sin, 0, sizeof(*sin));
	switch (what) {
	case DESTINATION:
		sin->sin_addr.s_addr = n->rip_dst;
		break;
	case NETMASK:
		if (n->rip_netmask == 0)
			return 0;
		sin->sin_addr.s_addr = n->rip_netmask;
		break;
	case GATEWAY:
		if (n->rip_router == 0)
			return 0;
		sin->sin_addr.s_addr = n->rip_router;
		break;
	default:
		abort();
		break;
	}

	sin->sin_family = n->rip_family;
#if BSD >= 198810
	sin->sin_len = sizeof(*sin);
#endif
	return 1;
}

static void
inet_put(pck, sa)
	void *pck;
	struct sockaddr *sa;
{
	struct netinfo *n = pck;
	struct sockaddr_in *sin = (struct sockaddr_in *) sa;
#if BSD >= 198810
	n->rip_family = htons(sin->sin_family);
#else
	n->rip_family = sin->sin_family;
#endif
	n->rip_dst = sin->sin_addr.s_addr;
}
