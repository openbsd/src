/*	$OpenBSD: rtadvd.h,v 1.11 2008/06/09 22:53:24 rainer Exp $	*/
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
#define ALLROUTERS_SITE "ff05::2"
#define ANY "::"
#define RTSOLLEN 8

/* protocol constants and default values */
#define DEF_MAXRTRADVINTERVAL 600
#define DEF_ADVLINKMTU 0
#define DEF_ADVREACHABLETIME 0
#define DEF_ADVRETRANSTIMER 0
#define DEF_ADVCURHOPLIMIT 64
#define DEF_ADVVALIDLIFETIME 2592000
#define DEF_ADVPREFERREDLIFETIME 604800

/*XXX int-to-double comparison for INTERVAL items */
#define mobileip6 0

#define MAXROUTERLIFETIME 9000
#define MIN_MAXINTERVAL (mobileip6 ? 1.5 : 4.0)
#define MAX_MAXINTERVAL 1800
#define MIN_MININTERVAL	(mobileip6 ? 0.05 : 3.0)
#define MAXREACHABLETIME 3600000

#undef miobileip6

#define MAX_INITIAL_RTR_ADVERT_INTERVAL  16
#define MAX_INITIAL_RTR_ADVERTISEMENTS    3
#define MAX_FINAL_RTR_ADVERTISEMENTS      3
#define MIN_DELAY_BETWEEN_RAS             3
#define MAX_RA_DELAY_TIME                 500000 /* usec */

#define PREFIX_FROM_KERNEL 1
#define PREFIX_FROM_CONFIG 2
#define PREFIX_FROM_DYNAMIC 3

struct prefix {
	TAILQ_ENTRY(prefix) entry;

	u_int32_t validlifetime; /* AdvValidLifetime */
	long	vltimeexpire;	/* expiration of vltime; decrement case only */
	u_int32_t preflifetime;	/* AdvPreferredLifetime */
	long	pltimeexpire;	/* expiration of pltime; decrement case only */
	u_int onlinkflg;	/* bool: AdvOnLinkFlag */
	u_int autoconfflg;	/* bool: AdvAutonomousFlag */
	int prefixlen;
	int origin;		/* from kernel or cofig */
	struct in6_addr prefix;
};


struct soliciter {
	SLIST_ENTRY(soliciter) entry;
	struct sockaddr_in6 addr;
};

struct	rainfo {
	/* pointer for list */
	SLIST_ENTRY(rainfo) entry;

	/* timer related parameters */
	struct rtadvd_timer *timer;
	int initcounter; /* counter for the first few advertisements */
	struct timeval lastsent; /* timestamp when the latest RA was sent */
	int waiting;		/* number of RS waiting for RA */

	/* interface information */
	int	ifindex;
	int	advlinkopt;	/* bool: whether include link-layer addr opt */
	struct sockaddr_dl *sdl;
	char	ifname[16];
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
	long	clockskew;	/* used for consisitency check of lifetimes */


	/* actual RA packet data and its length */
	size_t ra_datalen;
	u_char *ra_data;

	/* statistics */
	u_quad_t raoutput;	/* number of RAs sent */
	u_quad_t rainput;	/* number of RAs received */
	u_quad_t rainconsistent; /* number of RAs inconsistent with ours */
	u_quad_t rsinput;	/* number of RSs received */

	/* info about soliciter */
	SLIST_HEAD(, soliciter) soliciters; /* recent solication source */
};
SLIST_HEAD(ralist, rainfo);

void ra_timeout(void *);
void ra_timer_update(void *, struct timeval *);

int prefix_match(struct in6_addr *, int, struct in6_addr *, int);
struct rainfo *if_indextorainfo(int);
struct prefix *find_prefix(struct rainfo *, struct in6_addr *, int);

extern struct in6_addr in6a_site_allrouters;
