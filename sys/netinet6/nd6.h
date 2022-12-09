/*	$OpenBSD: nd6.h,v 1.93 2022/12/09 17:32:53 claudio Exp $	*/
/*	$KAME: nd6.h,v 1.95 2002/06/08 11:31:06 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET6_ND6_H_
#define _NETINET6_ND6_H_

#include <sys/task.h>

#define ND6_LLINFO_PURGE	-3
#define ND6_LLINFO_NOSTATE	-2
#define ND6_LLINFO_INCOMPLETE	0
#define ND6_LLINFO_REACHABLE	1
#define ND6_LLINFO_STALE	2
#define ND6_LLINFO_DELAY	3
#define ND6_LLINFO_PROBE	4

/*
 *  Locks used to protect struct members in this file:
 *	N	net lock
 */

struct nd_ifinfo {
	u_int32_t reachable;		/* [N] Reachable Time */
	int recalctm;			/* [N] BaseReachable recalc timer */
};

struct in6_nbrinfo {
	char ifname[IFNAMSIZ];	/* if name, e.g. "en0" */
	struct in6_addr addr;	/* IPv6 address of the neighbor */
	time_t	expire;		/* lifetime for NDP state transition */
	long	asked;		/* number of queries already sent for addr */
	int	isrouter;	/* if it acts as a router */
	int	state;		/* reachability state */
};

struct	in6_ndireq {
	char ifname[IFNAMSIZ];
	struct nd_ifinfo ndi;
};

/* protocol constants */
#define MAX_RTR_SOLICITATION_DELAY	1	/*1sec*/
#define RTR_SOLICITATION_INTERVAL	4	/*4sec*/
#define MAX_RTR_SOLICITATIONS		3

#define ND6_INFINITE_LIFETIME		0xffffffff

#ifdef _KERNEL

#include <sys/queue.h>

struct	llinfo_nd6 {
	TAILQ_ENTRY(llinfo_nd6)	ln_list;
	struct	rtentry *ln_rt;
	struct	mbuf *ln_hold;	/* last packet until resolved/timeout */
	long	ln_asked;	/* number of queries already sent for addr */
	int	ln_byhint;	/* # of times we made it reachable by UL hint */
	short	ln_state;	/* reachability state */
	short	ln_router;	/* 2^0: ND6 router bit */
};

#define ND6_LLINFO_PERMANENT(n)	((n)->ln_rt->rt_expire == 0)

/* node constants */
#define REACHABLE_TIME			30000	/* msec */
#define RETRANS_TIMER			1000	/* msec */
#define MIN_RANDOM_FACTOR		512	/* 1024 * 0.5 */
#define MAX_RANDOM_FACTOR		1536	/* 1024 * 1.5 */
#define ND_COMPUTE_RTIME(x) \
		(((MIN_RANDOM_FACTOR * (x >> 10)) + (arc4random() & \
		((MAX_RANDOM_FACTOR - MIN_RANDOM_FACTOR) * (x >> 10)))) /1000)

extern int nd6_delay;
extern int nd6_umaxtries;
extern int nd6_mmaxtries;
extern int nd6_maxnudhint;
extern int nd6_gctimer;
extern int nd6_debug;

#define nd6log(x)	do { if (nd6_debug) log x; } while (0)

struct nd_opts {
	struct nd_opt_hdr *nd_opts_src_lladdr;
	struct nd_opt_hdr *nd_opts_tgt_lladdr;
	struct nd_opt_hdr *nd_opts_search;	/* multiple opts */
	struct nd_opt_hdr *nd_opts_last;	/* multiple opts */
	int nd_opts_done;
};

void nd6_init(void);
void nd6_ifattach(struct ifnet *);
void nd6_ifdetach(struct ifnet *);
int nd6_is_addr_neighbor(const struct sockaddr_in6 *, struct ifnet *);
void nd6_option_init(void *, int, struct nd_opts *);
int nd6_options(struct nd_opts *);
struct	rtentry *nd6_lookup(const struct in6_addr *, int, struct ifnet *,
    u_int);
void nd6_llinfo_settimer(const struct llinfo_nd6 *, unsigned int);
void nd6_purge(struct ifnet *);
void nd6_nud_hint(struct rtentry *);
void nd6_rtrequest(struct ifnet *, int, struct rtentry *);
int nd6_ioctl(u_long, caddr_t, struct ifnet *);
void nd6_cache_lladdr(struct ifnet *, const struct in6_addr *, char *,
    int, int, int);
int nd6_resolve(struct ifnet *, struct rtentry *, struct mbuf *,
	 struct sockaddr *, u_char *);
int nd6_need_cache(struct ifnet *);

void nd6_na_input(struct mbuf *, int, int);
void nd6_na_output(struct ifnet *, const struct in6_addr *,
	const struct in6_addr *, u_long, int, struct sockaddr *);
void nd6_ns_input(struct mbuf *, int, int);
void nd6_ns_output(struct ifnet *, const struct in6_addr *,
	const struct in6_addr *, const struct llinfo_nd6 *, int);
caddr_t nd6_ifptomac(struct ifnet *);
void nd6_dad_start(struct ifaddr *);
void nd6_dad_stop(struct ifaddr *);

void nd6_rtr_cache(struct mbuf *, int, int, int);

int rt6_flush(struct in6_addr *, struct ifnet *);

void nd6_expire_timer_update(struct in6_ifaddr *);
#endif /* _KERNEL */

#endif /* _NETINET6_ND6_H_ */
