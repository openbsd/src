/*	$OpenBSD: netstat.h,v 1.14 2000/01/21 03:24:06 angelos Exp $	*/
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
 *	from: @(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#include <sys/cdefs.h>

/* What is the max length of a pointer printed with %p (including 0x)? */
#define PLEN	(LONG_BIT / 4 + 2)

int	Aflag;		/* show addresses of protocol control block */
int	aflag;		/* show all sockets (including servers) */
int	dflag;		/* show i/f dropped packets */
int	gflag;		/* show group (multicast) routing or stats */
int	iflag;		/* show interfaces */
int	lflag;		/* show routing table with use and ref */
int	mflag;		/* show memory stats */
int	nflag;		/* show addresses numerically */
int	pflag;		/* show given protocol */
int	rflag;		/* show routing tables (or routing stats) */
int	sflag;		/* show protocol statistics */
int	tflag;		/* show i/f watchdog timers */
int	vflag;		/* be verbose */

int	interval;	/* repeat interval for i/f stats */

char	*interface;	/* desired i/f for stats, or NULL for all i/fs */

int	af;		/* address family */

extern	char *__progname; /* program name, from crt0.o */


int	kread __P((u_long addr, char *buf, int size));
char	*plural __P((int));
char	*plurales __P((int));

void	protopr __P((u_long, char *));
void	tcp_stats __P((u_long, char *));
void	udp_stats __P((u_long, char *));
void	ip_stats __P((u_long, char *));
void	icmp_stats __P((u_long, char *));
void	igmp_stats __P((u_long, char *));
void	ah_stats __P((u_long, char *));
void	esp_stats __P((u_long, char *));
void	ipip_stats __P((u_long, char *));
void	etherip_stats __P((u_long, char *));
void	protopr __P((u_long, char *));

void	mbpr(u_long);

void	hostpr __P((u_long, u_long));
void	impstats __P((u_long, u_long));

void	intpr __P((int, u_long));

void	pr_rthdr __P((int));
void	pr_encaphdr __P(());
void	pr_family __P((int));
void	rt_stats __P((u_long));
char	*ns_phost __P((struct sockaddr *));
char	*ipx_phost __P((struct sockaddr *));
void	upHex __P((char *));

#ifdef INET6
struct in6_addr;
struct sockaddr_in6;
void	ip6protopr __P((u_long, char *));
void	ip6_stats __P((u_long, char *));
void	ip6_ifstats __P((char *));
void	icmp6_stats __P((u_long, char *));
void	icmp6_ifstats __P((char *));
void	pim6_stats __P((u_long, char *));
void	mroute6pr __P((u_long, u_long, u_long));
void	mrt6_stats __P((u_long, u_long));
char	*routename6 __P((struct sockaddr_in6 *));
char	*netname6 __P((struct sockaddr_in6 *, struct in6_addr *));
#endif /*INET6*/

char	*routename __P((in_addr_t));
char	*netname __P((in_addr_t, in_addr_t));
char	*ns_print __P((struct sockaddr *));
char	*ipx_print __P((struct sockaddr *));
void	routepr __P((u_long));

void	nsprotopr __P((u_long, char *));
void	spp_stats __P((u_long, char *));
void	idp_stats __P((u_long, char *));
void	nserr_stats __P((u_long, char *));

void	ipxprotopr __P((u_long, char *));
void	spx_stats __P((u_long, char *));
void	ipx_stats __P((u_long, char *));
void	ipxerr_stats __P((u_long, char *));

void	intpr __P((int, u_long));

void	unixpr __P((u_long));

void	esis_stats __P((u_long, char *));
void	clnp_stats __P((u_long, char *));
void	cltp_stats __P((u_long, char *));
void	iso_protopr __P((u_long, char *));
void	iso_protopr1 __P((u_long, int));
void	tp_protopr __P((u_long, char *));
void	tp_inproto __P((u_long));
void	tp_stats __P((caddr_t, caddr_t));

void	mroutepr __P((u_long, u_long, u_long, u_long));
void	mrt_stats __P((u_long, u_long));

void	atalkprotopr __P((u_long, char *));
void	ddp_stats __P((u_long, char *));
char	*atalk_print __P((const struct sockaddr *, int));
char	*atalk_print2 __P((const struct sockaddr *,
		const struct sockaddr *, int));
