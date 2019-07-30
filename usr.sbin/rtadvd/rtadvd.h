/*	$OpenBSD: rtadvd.h,v 1.29 2016/09/25 13:54:39 florian Exp $	*/
/*	$KAME: rtadvd.h,v 1.20 2002/05/29 10:13:10 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
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

#define RTADVD_USER     "_rtadvd"

#define ALLNODES "ff02::1"
#define ALLROUTERS_LINK "ff02::2"

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

#define MAX_ROUTERLIFETIME 9000
#define MIN_MAXINTERVAL 4
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL	3
#define MAX_REACHABLETIME 3600000

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      3
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                 500000 /* usec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

struct rtadvd_timer {
	struct event ev;
	struct timeval tm;
};

struct prefix {
	TAILQ_ENTRY(prefix) entry;

	u_int32_t validlifetime; /* AdvValidLifetime */
	time_t	vltimeexpire;	/* expiration of vltime; decrement case only */
	u_int32_t preflifetime;	/* AdvPreferredLifetime */
	time_t	pltimeexpire;	/* expiration of pltime; decrement case only */
	u_int onlinkflg;	/* bool: AdvOnLinkFlag */
	u_int autoconfflg;	/* bool: AdvAutonomousFlag */
	int prefixlen;
	int origin;		/* from kernel or config */
	struct in6_addr prefix;
};

struct rtinfo {
	TAILQ_ENTRY(rtinfo) entry;

	uint32_t lifetime;
	int rtpref;
	int prefixlen;
	struct in6_addr prefix;
};

/*
 * `struct rdnss` may contain an arbitrary number of `servers` and `struct
 * dnssldom` will contain a variable-sized `domain`. Space required for these
 * elements will be dynamically allocated. We do not use flexible array members
 * here because this breaks compile on some architectures using gcc2. Instead,
 * we just have an array with a single (unused) element.
 */

struct rdnss {
	TAILQ_ENTRY(rdnss) entry;

	u_int32_t lifetime;
	int servercnt;
	struct in6_addr servers[1];
};

struct dnssldom {
	TAILQ_ENTRY(dnssldom) entry;

	u_int32_t length;
	char domain[1];
};

struct dnssl {
	TAILQ_ENTRY(dnssl) entry;

	u_int32_t lifetime;
	TAILQ_HEAD(dnssldomlist, dnssldom) dnssldoms;
};

struct	rainfo {
	/* pointer for list */
	SLIST_ENTRY(rainfo) entry;

	/* timer related parameters */
	struct rtadvd_timer timer;
	unsigned int initcounter; /* counter for the first few advertisements */
	struct timeval lastsent; /* timestamp when the latest RA was sent */
	unsigned int waiting;	/* number of RS waiting for RA */

	/* interface information */
	int	ifindex;
	int	advlinkopt;	/* bool: whether include link-layer addr opt */
	struct sockaddr_dl *sdl;
	char	ifname[IF_NAMESIZE];
	int	phymtu;		/* mtu of the physical interface */

	/* Router configuration variables */
	u_short lifetime;	/* AdvDefaultLifetime */
	u_int	maxinterval;	/* MaxRtrAdvInterval */
	u_int	mininterval;	/* MinRtrAdvInterval */
	int 	managedflg;	/* AdvManagedFlag */
	int	otherflg;	/* AdvOtherConfigFlag */
	int	rtpref;		/* router preference */
	u_int32_t linkmtu;	/* AdvLinkMTU */
	u_int32_t reachabletime; /* AdvReachableTime */
	u_int32_t retranstimer;	/* AdvRetransTimer */
	u_int	hoplimit;	/* AdvCurHopLimit */
	TAILQ_HEAD(prefixlist, prefix) prefixes; /* AdvPrefixList(link head) */
	int	pfxs;		/* number of prefixes */
	TAILQ_HEAD(rtinfolist, rtinfo) rtinfos;
	TAILQ_HEAD(rdnsslist, rdnss) rdnsss; /* advertised recursive dns servers */
	TAILQ_HEAD(dnssllist, dnssl) dnssls;
	long	clockskew;	/* used for consistency check of lifetimes */


	/* actual RA packet data and its length */
	size_t ra_datalen;
	u_char *ra_data;

	/* statistics */
	uint64_t raoutput;	/* number of RAs sent */
	uint64_t rainput;	/* number of RAs received */
	uint64_t rainconsistent; /* number of RAs inconsistent with ours */
	uint64_t rsinput;	/* number of RSs received */
};
SLIST_HEAD(ralist, rainfo);

void ra_timer_update(struct rainfo *);

struct prefix *find_prefix(struct rainfo *, struct in6_addr *, int);
