/*	$OpenBSD: atalk.c,v 1.10 2003/02/01 01:51:31 deraadt Exp $	*/
/*	$NetBSD: atalk.c,v 1.2 1997/05/22 17:21:26 christos Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
static char sccsid[] = "from @(#)atalk.c	1.1 (Whistle) 6/6/96";
#else
static char rcsid[] = "$OpenBSD: atalk.c,v 1.10 2003/02/01 01:51:31 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <netdb.h>

#include <net/route.h>
#include <net/if.h>

/* #include <netinet/tcp_fsm.h> */

#include <netatalk/at.h>
#include <netatalk/ddp_var.h>

#include <nlist.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "netstat.h"

struct ddpcb    ddpcb;
struct socket   sockb;

static int first = 1;

static char *at_pr_net(struct sockaddr_at *, int);
static char *at_pr_host(struct sockaddr_at *, int);
static char *at_pr_range(struct sockaddr_at *);
static char *at_pr_port(struct sockaddr_at *);

/*
 * Print a summary of connections related to a Network Systems
 * protocol.  For XXX, also give state of connection.
 * Listening processes (aflag) are suppressed unless the
 * -a (all) flag is specified.
 */

static char *
at_pr_net(struct sockaddr_at *sat, int numeric)
{
	static char mybuf[50];

	if (!numeric) {
		switch (sat->sat_addr.s_net) {
		case 0xffff:
			return "????";
		case ATADDR_ANYNET:
			return ("*");
		}
	}
	(void) snprintf(mybuf, sizeof(mybuf), "%hu",
	    ntohs(sat->sat_addr.s_net));
	return mybuf;
}

static char *
at_pr_host(struct sockaddr_at *sat, int numeric)
{
	static char mybuf[50];

	if (!numeric) {
		switch (sat->sat_addr.s_node) {
		case ATADDR_BCAST:
			return "bcast";
		case ATADDR_ANYNODE:
			return ("*");
		}
	}
	(void) snprintf(mybuf, sizeof(mybuf), "%d",
	    (unsigned int) sat->sat_addr.s_node);
	return mybuf;
}

static char *
at_pr_port(struct sockaddr_at *sat)
{
	static char mybuf[50];
	struct servent *serv;

	switch (sat->sat_port) {
	case ATADDR_ANYPORT:
		return ("*");
	case 0xff:
		return "????";
	default:
		if (nflag)
			(void) snprintf(mybuf, sizeof(mybuf), "%d",
			    (unsigned int) sat->sat_port);
		else {
			serv = getservbyport(sat->sat_port, "ddp");
			if (serv == NULL)
				(void) snprintf(mybuf, sizeof(mybuf), "%d",
				    (unsigned int) sat->sat_port);
			else
				(void) snprintf(mybuf, sizeof(mybuf), "%s",
				    serv->s_name);
		}

		return mybuf;
	}
}

static char *
at_pr_range(struct sockaddr_at *sat)
{
	static char mybuf[50];

	if (sat->sat_range.r_netrange.nr_firstnet !=
	    sat->sat_range.r_netrange.nr_lastnet) {
		(void) snprintf(mybuf, sizeof(mybuf), "%d-%d",
		    ntohs(sat->sat_range.r_netrange.nr_firstnet),
		    ntohs(sat->sat_range.r_netrange.nr_lastnet));
	} else {
		(void) snprintf(mybuf, sizeof(mybuf), "%d",
		    ntohs(sat->sat_range.r_netrange.nr_firstnet));
	}
	return mybuf;
}


/* what == 0 for addr only == 3
 *	1 for net
 *	2 for host
 *	4 for port
 *	8 for numeric only
 */
char *
atalk_print(const struct sockaddr *sa, int what)
{
	struct sockaddr_at *sat = (struct sockaddr_at *) sa;
	static char mybuf[50];
	int numeric = (what & 0x08);

	mybuf[0] = 0;
	switch (what & 0x13) {
	case 0:
		mybuf[0] = 0;
		break;
	case 1:
		(void) snprintf(mybuf, sizeof(mybuf), "%s",
		    at_pr_net(sat, numeric));
		break;
	case 2:
		(void) snprintf(mybuf, sizeof(mybuf), "%s",
		    at_pr_host(sat, numeric));
		break;
	case 3:
		(void) snprintf(mybuf, sizeof(mybuf), "%s.%s",
		    at_pr_net(sat, numeric),
		    at_pr_host(sat, numeric));
		break;
	case 0x10:
		(void) snprintf(mybuf, sizeof(mybuf), "%s", at_pr_range(sat));
	}
	if (what & 4) {
		(void) snprintf(mybuf + strlen(mybuf),
		    sizeof(mybuf) - strlen(mybuf), ".%s",
		    at_pr_port(sat));
	}
	return mybuf;
}

char *
atalk_print2(const struct sockaddr *sa, const struct sockaddr *mask, int what)
{
	size_t		n, l;
	static char     buf[100];
	struct sockaddr_at *sat1, *sat2;
	struct sockaddr_at thesockaddr;
	struct sockaddr *sa2;

	sat1 = (struct sockaddr_at *) sa;
	sat2 = (struct sockaddr_at *) mask;
	sa2 = (struct sockaddr *) & thesockaddr;

	thesockaddr.sat_addr.s_net = sat1->sat_addr.s_net &
	    sat2->sat_addr.s_net;
	if ((n = snprintf(buf, sizeof(buf), "%s",
	    atalk_print(sa2, 1 | (what & 8)))) >= sizeof(buf))
		n = sizeof(buf) - 1;
	else if (n < 0)
		n = 0;	/* What else can be done ? */
	if (sat2->sat_addr.s_net != 0xFFFF) {
		thesockaddr.sat_addr.s_net = sat1->sat_addr.s_net |
		    ~sat2->sat_addr.s_net;
		if ((l = snprintf(buf + n, sizeof(buf) - n,
		    "-%s", atalk_print(sa2, 1 | (what & 8)))) >= sizeof(buf) - n)
			l = sizeof(buf) - n - 1;
		if (l > 0)
			n += l;
	}
	if (what & 2) {
		l = snprintf(buf + n, sizeof(buf) - n, ".%s",
		    atalk_print(sa, what & (~1)));
		if (l >= sizeof(buf) - n)
			l = sizeof(buf) - n - 1;
		if (l > 0)
			n += l;
	}
	return (buf);
}

void
atalkprotopr(u_long off, char *name)
{
	struct ddpcb    cb;
	struct ddpcb *prev, *next;
	struct ddpcb   *initial;

	if (off == 0)
		return;
	if (kread(off, (char *) &initial, sizeof(struct ddpcb *)) < 0)
		return;
	ddpcb = cb;
	prev = (struct ddpcb *) off;
	for (next = initial; next != NULL; prev = next) {
		u_long	ppcb = (u_long) next;

		if (kread((u_long) next, (char *) &ddpcb, sizeof(ddpcb)) < 0)
			return;
		next = ddpcb.ddp_next;
#if 0
		if (!aflag && atalk_nullhost(ddpcb.ddp_lsat)) {
			continue;
		}
#endif
		if (kread((u_long) ddpcb.ddp_socket,
		    (char *) &sockb, sizeof(sockb)) < 0)
			return;
		if (first) {
			printf("Active ATALK connections");
			if (aflag)
				printf(" (including servers)");
			putchar('\n');
			if (Aflag)
				printf("%-8.8s ", "PCB");
			printf(Aflag ?
			    "%-5.5s %-6.6s %-6.6s  %-18.18s %-18.18s %s\n" :
			    "%-5.5s %-6.6s %-6.6s  %-22.22s %-22.22s %s\n",
			    "Proto", "Recv-Q", "Send-Q",
			    "Local Address", "Foreign Address", "(state)");
			first = 0;
		}
		if (Aflag)
			printf("%8lx ", ppcb);
		printf("%-5.5s %6ld %6ld ", name, sockb.so_rcv.sb_cc,
		    sockb.so_snd.sb_cc);
		printf(Aflag ? " %-18.18s" : " %-22.22s", atalk_print(
		    (struct sockaddr *) & ddpcb.ddp_lsat, 7));
		printf(Aflag ? " %-18.18s" : " %-22.22s", atalk_print(
		    (struct sockaddr *) & ddpcb.ddp_fsat, 7));
		putchar('\n');
	}
}

#define p(f, m) if (ddpstat.f || sflag <= 1) \
	printf(m, ddpstat.f, plural(ddpstat.f))
#define p2(f1, f2, m) if (ddpstat.f1 || ddpstat.f2 || sflag <= 1) \
	printf(m, ddpstat.f1, plural(ddpstat.f1), ddpstat.f2, plural(ddpstat.f2))
#define p3(f, m) if (ddpstat.f || sflag <= 1) \
	printf(m, ddpstat.f, plurales(ddpstat.f))

/*
 * Dump DDP statistics structure.
 */
void
ddp_stats(u_long off, char *name)
{
	struct ddpstat  ddpstat;

	if (off == 0)
		return;
	if (kread(off, (char *) &ddpstat, sizeof(ddpstat)) < 0)
		return;
	printf("%s:\n", name);
	p(ddps_short, "\t%ld packet%s with short headers\n");
	p(ddps_long, "\t%ld packet%s with long headers\n");
	p(ddps_nosum, "\t%ld packet%s with no checksum\n");
	p(ddps_tooshort, "\t%ld packet%s were too short\n");
	p(ddps_badsum, "\t%ld packet%s with bad checksum\n");
	p(ddps_toosmall, "\t%ld packet%s with not enough data\n");
	p(ddps_forward, "\t%ld packet%s forwarded\n");
	p(ddps_cantforward, "\t%ld packet%s rcvd for unreachable dest\n");
	p(ddps_nosockspace, "\t%ld packet%s dropped due to no socket space\n");
}
