/*	$OpenBSD: netstat.h,v 1.31 2005/02/10 14:25:08 itojun Exp $	*/
/*	$NetBSD: netstat.h,v 1.6 1996/05/07 02:55:05 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>

/* What is the max length of a pointer printed with %p (including 0x)? */
#define PLEN	(LONG_BIT / 4 + 2)

int	Aflag;		/* show addresses of protocol control block */
int	aflag;		/* show all sockets (including servers) */
int	bflag;		/* show bytes instead of packets */
int	dflag;		/* show i/f dropped packets */
int	gflag;		/* show group (multicast) routing or stats */
int	iflag;		/* show interfaces */
int	lflag;		/* show routing table with use and ref */
int	mflag;		/* show memory stats */
int	nflag;		/* show addresses numerically */
int	pflag;		/* show given protocol */
int	qflag;		/* only display non-zero values for output */
int	rflag;		/* show routing tables (or routing stats) */
int	Sflag;		/* show source address in routing table */
int	sflag;		/* show protocol statistics */
int	tflag;		/* show i/f watchdog timers */
int	vflag;		/* be verbose */

int	interval;	/* repeat interval for i/f stats */

char	*interface;	/* desired i/f for stats, or NULL for all i/fs */

int	af;		/* address family */

extern	char *__progname; /* program name, from crt0.o */


int	kread(u_long addr, char *buf, int size);
char	*plural(int);
char	*plurales(int);

void	protopr(u_long, char *);
#ifdef INET6
void	ip6protopr(u_long, char *);
#endif
void	tcp_stats(u_long, char *);
void	udp_stats(u_long, char *);
void	ip_stats(u_long, char *);
void	icmp_stats(u_long, char *);
void	igmp_stats(u_long, char *);
void	pim_stats(u_long, char *);
void	ah_stats(u_long, char *);
void	esp_stats(u_long, char *);
void	ipip_stats(u_long, char *);
void	carp_stats (u_long, char *);
void	pfsync_stats (u_long, char *);
void	etherip_stats(u_long, char *);
void	protopr(u_long, char *);
void	ipcomp_stats(u_long, char *);

void	mbpr(u_long, u_long, u_long);

void	hostpr(u_long, u_long);
void	impstats(u_long, u_long);

void	intpr(int, u_long);

void	pr_rthdr(int);
void	pr_encaphdr(void);
void	pr_family(int);
void	rt_stats(u_long);
char	*ns_phost(struct sockaddr *);
char	*ipx_phost(struct sockaddr *);
void	upHex(char *);

#ifdef INET6
struct in6_addr;
struct sockaddr_in6;
void	ip6protopr(u_long, char *);
void	ip6_stats(u_long, char *);
void	ip6_ifstats(char *);
void	icmp6_stats(u_long, char *);
void	icmp6_ifstats(char *);
void	pim6_stats(u_long, char *);
void	rip6_stats(u_long, char *);
void	mroute6pr(u_long, u_long, u_long);
void	mrt6_stats(u_long, u_long);
char	*routename6(struct sockaddr_in6 *);
char	*netname6(struct sockaddr_in6 *, struct in6_addr *);
#endif /*INET6*/

char	*routename(in_addr_t);
char	*netname(in_addr_t, in_addr_t);
char	*ns_print(struct sockaddr *);
char	*ipx_print(struct sockaddr *);
void	routepr(u_long);

void	nsprotopr(u_long, char *);
void	spp_stats(u_long, char *);
void	idp_stats(u_long, char *);
void	nserr_stats(u_long, char *);

void	ipxprotopr(u_long, char *);
void	spx_stats(u_long, char *);
void	ipx_stats(u_long, char *);
void	ipxerr_stats(u_long, char *);

void	intpr(int, u_long);

void	unixpr(u_long);

void	esis_stats(u_long, char *);
void	clnp_stats(u_long, char *);
void	cltp_stats(u_long, char *);
void	iso_protopr(u_long, char *);
void	iso_protopr1(u_long, int);
void	tp_protopr(u_long, char *);
void	tp_inproto(u_long);
void	tp_stats(u_long, char *);

void	mroutepr(u_long, u_long, u_long, u_long);
void	mrt_stats(u_long, u_long);

void	atalkprotopr(u_long, char *);
void	ddp_stats(u_long, char *);
char	*atalk_print(const struct sockaddr *, int);
char	*atalk_print2(const struct sockaddr *, const struct sockaddr *, int);
