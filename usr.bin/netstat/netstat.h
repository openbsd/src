/*	$OpenBSD: netstat.h,v 1.67 2015/02/09 12:25:03 claudio Exp $	*/
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

#include <kvm.h>

/* What is the max length of a pointer printed with %p (including 0x)? */
#define PLEN	(LONG_BIT / 4 + 2)

int	Aflag;		/* show addresses of protocol control block */
int	aflag;		/* show all sockets (including servers) */
int	Bflag;		/* show TCP send and receive buffer sizes */
int	bflag;		/* show bytes instead of packets */
int	dflag;		/* show i/f dropped packets */
int	Fflag;		/* show routes whose gateways are in specified AF */
int	gflag;		/* show group (multicast) routing or stats */
int	hflag;		/* print human numbers */
int	iflag;		/* show interfaces */
int	lflag;		/* show routing table with use and ref */
int	mflag;		/* show memory stats */
int	nflag;		/* show addresses numerically */
int	pflag;		/* show given protocol */
int	Pflag;		/* show given PCB */
int	qflag;		/* only display non-zero values for output */
int	rflag;		/* show routing tables (or routing stats) */
int	sflag;		/* show protocol statistics */
int	tflag;		/* show i/f watchdog timers */
int	vflag;		/* be verbose */
int	Wflag;		/* show net80211 protocol statistics */

int	interval;	/* repeat interval for i/f stats */

char	*interface;	/* desired i/f for stats, or NULL for all i/fs */

int	af;		/* address family */

extern	char *__progname; /* program name, from crt0.o */

extern int hideroot;

int	kread(u_long addr, void *buf, int size);
char	*plural(u_int64_t);
char	*plurales(u_int64_t);

void	protopr(u_long, char *, int, u_int, u_long);
void	tcp_stats(char *);
void	udp_stats(char *);
void	ip_stats(char *);
void	div_stats(char *);
void	icmp_stats(char *);
void	igmp_stats(char *);
void	pim_stats(char *);
void	ah_stats(char *);
void	esp_stats(char *);
void	ipip_stats(char *);
void	carp_stats (char *);
void	pfsync_stats (char *);
void	pflow_stats (char *);
void	etherip_stats(char *);
void	ipcomp_stats(char *);

void	net80211_ifstats(char *);

void	socket_dump(u_long);
void	unpcb_dump(u_long);

void	mbpr(void);

void	hostpr(u_long, u_long);
void	impstats(u_long, u_long);

void	rt_stats(void);
void	pr_rthdr(int, int);
void	pr_encaphdr(void);
void	pr_family(int);

struct in6_addr;
struct sockaddr_in6;
void	ip6_stats(char *);
void	ip6_ifstats(char *);
void	icmp6_stats(char *);
void	icmp6_ifstats(char *);
void	pim6_stats(char *);
void	div6_stats(char *);
void	rip6_stats(char *);
void	mroute6pr(void);
void	mrt6_stats(void);
char	*routename6(struct sockaddr_in6 *);
char	*netname6(struct sockaddr_in6 *, struct sockaddr_in6 *);

size_t	get_sysctl(const int *, u_int, char **);
void	p_rttables(int, u_int);
void	p_flags(int, char *);
void	p_addr(struct sockaddr *, struct sockaddr *, int);
void	p_gwaddr(struct sockaddr *, int);
void	p_sockaddr(struct sockaddr *, struct sockaddr *, int, int);
char	*routename(struct sockaddr *);
char	*routename4(in_addr_t);
char	*netname(struct sockaddr *, struct sockaddr *);
char	*netname4(in_addr_t, in_addr_t);
char	*mpls_op(u_int32_t);
void	routepr(u_long, u_long, u_long, u_long, u_int);

void	nsprotopr(u_long, char *);

void	intpr(int, int);

void	unixpr(kvm_t *, u_long);

void	mroutepr(void);
void	mrt_stats(void);
