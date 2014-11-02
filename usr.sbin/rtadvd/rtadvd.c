/*	$OpenBSD: rtadvd.c,v 1.49 2014/11/02 02:33:33 deraadt Exp $	*/
/*	$KAME: rtadvd.c,v 1.66 2002/05/29 14:18:36 itojun Exp $	*/

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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <util.h>
#include <pwd.h>

#include "rtadvd.h"
#include "rrenum.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"
#include "dump.h"
#include "log.h"

struct msghdr rcvmhdr;
static u_char *rcvcmsgbuf;
static size_t rcvcmsgbuflen;
static u_char *sndcmsgbuf = NULL;
static size_t sndcmsgbuflen;
volatile sig_atomic_t do_dump;
volatile sig_atomic_t do_die;
struct msghdr sndmhdr;
struct iovec rcviov[2];
struct iovec sndiov[2];
struct sockaddr_in6 from;
struct sockaddr_in6 sin6_allnodes = {sizeof(sin6_allnodes), AF_INET6};
struct in6_addr in6a_site_allrouters;
static char *mcastif;
int sock;
int rtsock = -1;
int accept_rr = 0;
int dflag = 0, sflag = 0;
int log_perror = 0;

u_char *conffile = NULL;

struct ralist ralist;

struct nd_opt {
	SLIST_ENTRY(nd_opt)	 entry;
	struct nd_opt_hdr	*opt;
};

union nd_opts {
	struct nd_opt_hdr *nd_opt_array[9];
	struct {
		struct nd_opt_hdr *zero;
		struct nd_opt_hdr *src_lladdr;
		struct nd_opt_hdr *tgt_lladdr;
		struct nd_opt_prefix_info *pi;
		struct nd_opt_rd_hdr *rh;
		struct nd_opt_mtu *mtu;
		SLIST_HEAD(nd_optlist, nd_opt)	list;
	} nd_opt_each;
};
#define nd_opts_src_lladdr	nd_opt_each.src_lladdr
#define nd_opts_tgt_lladdr	nd_opt_each.tgt_lladdr
#define nd_opts_pi		nd_opt_each.pi
#define nd_opts_rh		nd_opt_each.rh
#define nd_opts_mtu		nd_opt_each.mtu
#define nd_opts_list		nd_opt_each.list

#define NDOPT_FLAG_SRCLINKADDR	(1 << 0)
#define NDOPT_FLAG_TGTLINKADDR	(1 << 1)
#define NDOPT_FLAG_PREFIXINFO	(1 << 2)
#define NDOPT_FLAG_RDHDR	(1 << 3)
#define NDOPT_FLAG_MTU		(1 << 4)
#define NDOPT_FLAG_RDNSS	(1 << 5)
#define NDOPT_FLAG_DNSSL	(1 << 6)
#define NDOPT_FLAG_ROUTE_INFO	(1 << 7)

u_int32_t ndopt_flags[] = {
	[ND_OPT_SOURCE_LINKADDR]	= NDOPT_FLAG_SRCLINKADDR,
	[ND_OPT_TARGET_LINKADDR]	= NDOPT_FLAG_TGTLINKADDR,
	[ND_OPT_PREFIX_INFORMATION]	= NDOPT_FLAG_PREFIXINFO,
	[ND_OPT_REDIRECTED_HEADER]	= NDOPT_FLAG_RDHDR,
	[ND_OPT_MTU]			= NDOPT_FLAG_MTU,
	[ND_OPT_ROUTE_INFO]		= NDOPT_FLAG_ROUTE_INFO,
	[ND_OPT_RDNSS]			= NDOPT_FLAG_RDNSS,
	[ND_OPT_DNSSL]			= NDOPT_FLAG_DNSSL,
};

int main(int, char *[]);
static void set_die(int);
static void die(void);
static void sock_open(void);
static void rtsock_open(void);
static void rtadvd_input(void);
static void rs_input(int, struct nd_router_solicit *,
    struct in6_pktinfo *, struct sockaddr_in6 *);
static void ra_input(int, struct nd_router_advert *,
    struct in6_pktinfo *, struct sockaddr_in6 *);
static int prefix_check(struct nd_opt_prefix_info *, struct rainfo *,
    struct sockaddr_in6 *);
static int nd6_options(struct nd_opt_hdr *, int,
    union nd_opts *, u_int32_t);
static void free_ndopts(union nd_opts *);
static void ra_output(struct rainfo *);
static void rtmsg_input(void);
static void rtadvd_set_dump(int);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct pollfd pfd[2];
	struct timeval *timeout;
	struct passwd *pw;
	int i, ch, npfd;

	log_init(1);		/* log to stderr until daemonized */

	closefrom(3);

	/* get command line options and arguments */
#define OPTIONS "c:dM:Rs"
	while ((ch = getopt(argc, argv, OPTIONS)) != -1) {
#undef OPTIONS
		switch (ch) {
		case 'c':
			conffile = optarg;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'M':
			mcastif = optarg;
			break;
		case 'R':
			fprintf(stderr, "rtadvd: "
				"the -R option is currently ignored.\n");
			/* accept_rr = 1; */
			/* run anyway... */
			break;
		case 's':
			sflag = 1;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		fprintf(stderr,
			"usage: rtadvd [-dMRs] [-c configfile] "
			"interface ...\n");
		exit(1);
	}

	SLIST_INIT(&ralist);

	/* get iflist block from kernel */
	init_iflist();

	if (conffile == NULL)
		log_init(dflag);

	while (argc--)
		getconfig(*argv++);

	if (inet_pton(AF_INET6, ALLNODES, &sin6_allnodes.sin6_addr) != 1)
		fatal("inet_pton failed");

	if (conffile != NULL)
		log_init(dflag);

	if (!dflag)
		daemon(1, 0);

	sock_open();

	/* record the current PID */
	if (pidfile(NULL) < 0)
		log_warnx("failed to open the pid log file, run anyway.");

	if (sflag == 0) {
		rtsock_open();
	}

	if ((pw = getpwnam(RTADVD_USER)) == NULL)
		fatal("getpwnam(" RTADVD_USER ")");
	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");
	if (setgroups(1, &pw->pw_gid) == -1 ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("cannot drop privileges");

	npfd = 1;
	pfd[0].fd = sock;
	pfd[0].events = POLLIN;
	if (rtsock >= 0) {
		pfd[1].fd = rtsock;
		pfd[1].events = POLLIN;
		npfd++;
	}

	signal(SIGTERM, set_die);
	signal(SIGUSR1, rtadvd_set_dump);

	while (1) {
		if (do_dump) {	/* SIGUSR1 */
			do_dump = 0;
			rtadvd_dump();
		}

		if (do_die) {
			die();
			/*NOTREACHED*/
		}

		/* timer expiration check and reset the timer */
		timeout = rtadvd_check_timer();

		if (timeout != NULL)
			log_debug("set timer to %lld.%ld. waiting for "
			    "inputs or timeout",
			    (long long)timeout->tv_sec,
			    timeout->tv_usec);
		else
			log_debug("there's no timer. waiting for inputs");

		if ((i = poll(pfd, npfd,
		    timeout->tv_sec * 1000 + timeout->tv_usec / 1000)) < 0) {
			/* EINTR would occur upon SIGUSR1 for status dump */
			if (errno != EINTR)
				log_warn("select");
			continue;
		}
		if (i == 0)	/* timeout */
			continue;
		if (rtsock != -1 && (pfd[1].revents & POLLIN))
			rtmsg_input();
		if (pfd[0].revents & POLLIN)
			rtadvd_input();
	}
	exit(0);		/* NOTREACHED */
}

static void
rtadvd_set_dump(int signo)
{
	do_dump = 1;
}

static void
set_die(int signo)
{
	do_die = 1;
}

static void
die()
{
	struct rainfo *ra;
	int i;
	const int retrans = MAX_FINAL_RTR_ADVERTISEMENTS;

	if (dflag)
		log_debug("cease to be an advertising router");

	SLIST_FOREACH(ra, &ralist, entry) {
		ra->lifetime = 0;
		make_packet(ra);
	}
	for (i = 0; i < retrans; i++) {
		SLIST_FOREACH(ra, &ralist, entry)
			ra_output(ra);
		sleep(MIN_DELAY_BETWEEN_RAS);
	}
	exit(0);
	/*NOTREACHED*/
}

static void
rtmsg_input()
{
	int n, type, ifindex = 0, plen;
	size_t len;
	char msg[2048], *next, *lim;
	u_char ifname[IF_NAMESIZE];
	struct prefix *prefix;
	struct rainfo *rai;
	struct in6_addr *addr;
	char addrbuf[INET6_ADDRSTRLEN];

	n = read(rtsock, msg, sizeof(msg));
	if (dflag)
		log_debug("received a routing message "
		    "(type = %d, len = %d)", rtmsg_type(msg), n);
	if (n > rtmsg_len(msg)) {
		/*
		 * This usually won't happen for messages received on 
		 * a routing socket.
		 */
		if (dflag)
			log_debug("received data length is larger than "
			    "1st routing message len. multiple messages? "
			    "read %d bytes, but 1st msg len = %d",
			    n, rtmsg_len(msg));
#if 0
		/* adjust length */
		n = rtmsg_len(msg);
#endif
	}

	lim = msg + n;
	for (next = msg; next < lim; next += len) {
		int oldifflags;

		next = get_next_msg(next, lim, 0, &len,
				    RTADV_TYPE2BITMASK(RTM_ADD) |
				    RTADV_TYPE2BITMASK(RTM_DELETE) |
				    RTADV_TYPE2BITMASK(RTM_NEWADDR) |
				    RTADV_TYPE2BITMASK(RTM_DELADDR) |
				    RTADV_TYPE2BITMASK(RTM_IFINFO));
		if (len == 0)
			break;
		type = rtmsg_type(next);
		switch (type) {
		case RTM_ADD:
		case RTM_DELETE:
			ifindex = get_rtm_ifindex(next);
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifindex = get_ifam_ifindex(next);
			break;
		case RTM_IFINFO:
			ifindex = get_ifm_ifindex(next);
			break;
		default:
			/* should not reach here */
			if (dflag)
				log_debug("unknown rtmsg %d on %s",
				    type, if_indextoname(ifindex, ifname));
			continue;
		}

		if ((rai = if_indextorainfo(ifindex)) == NULL) {
			if (dflag)
				log_debug("route changed on "
				    "non advertising interface(%s)",
				    if_indextoname(ifindex, ifname));
			continue;
		}
		oldifflags = iflist[ifindex]->ifm_flags;

		switch (type) {
		case RTM_ADD:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);

			if (sflag)
				break;	/* we aren't interested in prefixes  */

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				log_info("new interface route's"
				    " plen %d is invalid for a prefix", plen);
				break;
			}
			prefix = find_prefix(rai, addr, plen);
			if (prefix) {
				if (dflag)
					log_debug("new prefix(%s/%d) "
					    "added on %s, "
					    "but it was already in list",
					    inet_ntop(AF_INET6, addr,
					    (char *)addrbuf, INET6_ADDRSTRLEN),
					    plen, rai->ifname);
				break;
			}
			make_prefix(rai, ifindex, addr, plen);
			break;
		case RTM_DELETE:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);

			if (sflag)
				break;

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				log_info("deleted interface route's "
				    "plen %d is invalid for a prefix", plen);
				break;
			}
			prefix = find_prefix(rai, addr, plen);
			if (prefix == NULL) {
				if (dflag)
					log_debug("prefix(%s/%d) was "
					    "deleted on %s, "
					    "but it was not in list",
					    inet_ntop(AF_INET6, addr,
					    (char *)addrbuf, INET6_ADDRSTRLEN),
					    plen, rai->ifname);
				break;
			}
			delete_prefix(rai, prefix);
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);
			break;
		case RTM_IFINFO:
			iflist[ifindex]->ifm_flags = get_ifm_flags(next);
			break;
		default:
			/* should not reach here */
			if (dflag)
				log_debug("unknown rtmsg %d on %s",
				    type, if_indextoname(ifindex, ifname));
			return;
		}

		/* check if an interface flag is changed */
		if ((oldifflags & IFF_UP) != 0 &&	/* UP to DOWN */
		    (iflist[ifindex]->ifm_flags & IFF_UP) == 0) {
			log_info("interface %s becomes down. stop timer.",
			    rai->ifname);
			rtadvd_remove_timer(&rai->timer);
		} else if ((oldifflags & IFF_UP) == 0 && /* DOWN to UP */
			 (iflist[ifindex]->ifm_flags & IFF_UP) != 0) {
			log_info("interface %s becomes up. restart timer.",
			    rai->ifname);

			rai->initcounter = 0; /* reset the counter */
			rai->waiting = 0; /* XXX */
			rai->timer = rtadvd_add_timer(ra_timeout,
			    ra_timer_update, rai, rai);
			ra_timer_update((void *)rai, &rai->timer->tm);
			rtadvd_set_timer(&rai->timer->tm, rai->timer);
		}
	}

	return;
}

void
rtadvd_input()
{
	int i;
	int *hlimp = NULL;
	struct icmp6_hdr *icp;
	int ifindex = 0;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	struct in6_addr dst = in6addr_any;

	/*
	 * Get message. We reset msg_controllen since the field could
	 * be modified if we had received a message before setting
	 * receive options.
	 */
	rcvmhdr.msg_controllen = rcvcmsgbuflen;
	if ((i = recvmsg(sock, &rcvmhdr, 0)) < 0)
		return;

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
			dst = pi->ipi6_addr;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (ifindex == 0) {
		log_warnx("failed to get receiving interface");
		return;
	}
	if (hlimp == NULL) {
		log_warnx("failed to get receiving hop limit");
		return;
	}

	/*
	 * If we happen to receive data on an interface which is now down,
	 * just discard the data.
	 */
	if ((iflist[pi->ipi6_ifindex]->ifm_flags & IFF_UP) == 0) {
		log_info("received data on a disabled interface (%s)",
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	if (i < sizeof(struct icmp6_hdr)) {
		log_warnx("packet size(%d) is too short", i);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;

	switch (icp->icmp6_type) {
	case ND_ROUTER_SOLICIT:
		/*
		 * Message verification - RFC-2461 6.1.1
		 * XXX: these checks must be done in the kernel as well,
		 *      but we can't completely rely on them.
		 */
		if (*hlimp != 255) {
			log_info("RS with invalid hop limit(%d) "
			    "received from %s on %s",
			    *hlimp,
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			log_info("RS with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    icp->icmp6_code,
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (i < sizeof(struct nd_router_solicit)) {
			log_info("RS from %s on %s does not have enough "
			    "length (len = %d)",
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		rs_input(i, (struct nd_router_solicit *)icp, pi, &from);
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Message verification - RFC-2461 6.1.2
		 * XXX: there's a same dilemma as above... 
		 */
		if (*hlimp != 255) {
			log_info("RA with invalid hop limit(%d) "
			    "received from %s on %s",
			    *hlimp,
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			log_info("RA with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    icp->icmp6_code,
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (i < sizeof(struct nd_router_advert)) {
			log_info("RA from %s on %s does not have enough "
			    "length (len = %d)",
			    inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
			    INET6_ADDRSTRLEN),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		ra_input(i, (struct nd_router_advert *)icp, pi, &from);
		break;
	case ICMP6_ROUTER_RENUMBERING:
		if (accept_rr == 0) {
			log_warnx("received a router renumbering "
			    "message, but not allowed to be accepted");
			break;
		}
		rr_input(i, (struct icmp6_router_renum *)icp, pi, &from,
			 &dst);
		break;
	default:
		/*
		 * Note that this case is POSSIBLE, especially just
		 * after invocation of the daemon. This is because we
		 * could receive message after opening the socket and
		 * before setting ICMP6 type filter(see sock_open()).
		 */
		log_warnx("invalid icmp type(%d)", icp->icmp6_type);
		return;
	}

	return;
}

static void
rs_input(int len, struct nd_router_solicit *rs,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	struct rainfo *ra;

	log_debug("RS received from %s on %s",
	    inet_ntop(AF_INET6, &from->sin6_addr,
		ntopbuf, INET6_ADDRSTRLEN),
	    if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	SLIST_INIT(&ndopts.nd_opts_list);
	if (nd6_options((struct nd_opt_hdr *)(rs + 1),
			len - sizeof(struct nd_router_solicit),
			&ndopts, NDOPT_FLAG_SRCLINKADDR)) {
		log_debug("ND option check failed for an RS from %s on %s",
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * If the IP source address is the unspecified address, there
	 * must be no source link-layer address option in the message.
	 * (RFC-2461 6.1.1)
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&from->sin6_addr) &&
	    ndopts.nd_opts_src_lladdr) {
		log_warnx("RS from unspecified src on %s has a link-layer"
		       " address option",
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	SLIST_FOREACH(ra, &ralist, entry) {
		if (pi->ipi6_ifindex == ra->ifindex)
			break;
	}
	if (ra == NULL) {
		log_info("RS received on non advertising interface(%s)",
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	ra->rsinput++;		/* increment statistics */

	/*
	 * Decide whether to send RA according to the rate-limit
	 * consideration.
	 */
	{
		long delay;	/* must not be greater than 1000000 */
		struct timeval interval, now, min_delay, tm_tmp, *rest;
		struct soliciter *sol;

		/*
		 * record sockaddr waiting for RA, if possible
		 */
		sol = (struct soliciter *)malloc(sizeof(*sol));
		if (sol) {
			sol->addr = *from;
			/*XXX RFC2553 need clarification on flowinfo */
			sol->addr.sin6_flowinfo = 0;	
			SLIST_INSERT_HEAD(&ra->soliciters, sol, entry);
		}

		/*
		 * If there is already a waiting RS packet, don't
		 * update the timer.
		 */
		if (ra->waiting++)
			goto done;

		/*
		 * Compute a random delay. If the computed value
		 * corresponds to a time later than the time the next
		 * multicast RA is scheduled to be sent, ignore the random
		 * delay and send the advertisement at the
		 * already-scheduled time. RFC-2461 6.2.6
		 */
		delay = arc4random_uniform(MAX_RA_DELAY_TIME);
		interval.tv_sec = 0;
		interval.tv_usec = delay;
		rest = rtadvd_timer_rest(ra->timer);
		if (timercmp(rest, &interval, <)) {
			log_debug("random delay is larger than "
			    "the rest of normal timer");
			interval = *rest;
		}

		/*
		 * If we sent a multicast Router Advertisement within
		 * the last MIN_DELAY_BETWEEN_RAS seconds, schedule
		 * the advertisement to be sent at a time corresponding to
		 * MIN_DELAY_BETWEEN_RAS plus the random value after the
		 * previous advertisement was sent.
		 */
		gettimeofday(&now, NULL);
		timersub(&now, &ra->lastsent, &tm_tmp);
		min_delay.tv_sec = MIN_DELAY_BETWEEN_RAS;
		min_delay.tv_usec = 0;
		if (timercmp(&tm_tmp, &min_delay, <)) {
			timersub(&min_delay, &tm_tmp, &min_delay);
			timeradd(&min_delay, &interval, &interval);
		}
		rtadvd_set_timer(&interval, ra->timer);
		goto done;
	}

  done:
	free_ndopts(&ndopts);
	return;
}

static void
ra_input(int len, struct nd_router_advert *ra,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	struct rainfo *rai;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	char *on_off[] = {"OFF", "ON"};
	u_int32_t reachabletime, retranstimer, mtu;
	int inconsistent = 0;

	log_debug("RA received from %s on %s",
	    inet_ntop(AF_INET6, &from->sin6_addr,
		ntopbuf, INET6_ADDRSTRLEN),
	    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
	
	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	SLIST_INIT(&ndopts.nd_opts_list);
	if (nd6_options((struct nd_opt_hdr *)(ra + 1),
			len - sizeof(struct nd_router_advert),
			&ndopts, NDOPT_FLAG_SRCLINKADDR | NDOPT_FLAG_PREFIXINFO
			| NDOPT_FLAG_MTU | NDOPT_FLAG_ROUTE_INFO
			| NDOPT_FLAG_RDNSS | NDOPT_FLAG_DNSSL)) {
		log_warnx("ND option check failed for an RA from %s on %s",
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * RA consistency check according to RFC-2461 6.2.7
	 */
	if ((rai = if_indextorainfo(pi->ipi6_ifindex)) == 0) {
		log_info("received RA from %s on non-advertising interface(%s)",
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			INET6_ADDRSTRLEN),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}
	rai->rainput++;		/* increment statistics */
	
	/* Cur Hop Limit value */
	if (ra->nd_ra_curhoplimit && rai->hoplimit &&
	    ra->nd_ra_curhoplimit != rai->hoplimit) {
		log_info("CurHopLimit inconsistent on %s:  %d from %s,"
		    " %d from us",
		    rai->ifname,
		    ra->nd_ra_curhoplimit,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    rai->hoplimit);
		inconsistent++;
	}
	/* M flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) !=
	    rai->managedflg) {
		log_info("M flag inconsistent on %s: %s from %s, %s from us",
		    rai->ifname, on_off[rai->managedflg ? 0 : 1],
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    on_off[rai->managedflg ? 1 : 0]);
		inconsistent++;
	}
	/* O flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) !=
	    rai->otherflg) {
		log_info("O flag inconsistent on %s: %s from %s, %s from us",
		    rai->ifname, on_off[rai->otherflg ? 0 : 1],
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    on_off[rai->otherflg ? 1 : 0]);
		inconsistent++;
	}
	/* Reachable Time */
	reachabletime = ntohl(ra->nd_ra_reachable);
	if (reachabletime && rai->reachabletime &&
	    reachabletime != rai->reachabletime) {
		log_info("ReachableTime inconsistent on %s:"
		    " %d from %s, %d from us",
		    rai->ifname, reachabletime, 
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    rai->reachabletime);
		inconsistent++;
	}
	/* Retrans Timer */
	retranstimer = ntohl(ra->nd_ra_retransmit);
	if (retranstimer && rai->retranstimer &&
	    retranstimer != rai->retranstimer) {
		log_info("RetranceTimer inconsistent on %s:"
		    " %d from %s, %d from us",
		    rai->ifname, retranstimer,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    rai->retranstimer);
		inconsistent++;
	}
	/* Values in the MTU options */
	if (ndopts.nd_opts_mtu) {
		mtu = ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);
		if (mtu && rai->linkmtu && mtu != rai->linkmtu) {
			log_info("MTU option value inconsistent on %s:"
			    " %d from %s, %d from us",
			    rai->ifname, mtu,
			    inet_ntop(AF_INET6, &from->sin6_addr,
				ntopbuf, INET6_ADDRSTRLEN),
			    rai->linkmtu);
			inconsistent++;
		}
	}
	/* Preferred and Valid Lifetimes for prefixes */
	{
		struct nd_opt 	*optp;

		if (ndopts.nd_opts_pi)
			if (prefix_check(ndopts.nd_opts_pi, rai, from))
				inconsistent++;
		SLIST_FOREACH(optp, &ndopts.nd_opts_list, entry) {
			if (prefix_check((struct nd_opt_prefix_info *)optp->opt,
					 rai, from))
				inconsistent++;
		}
	}

	if (inconsistent)
		rai->rainconsistent++;
	
  done:
	free_ndopts(&ndopts);
	return;
}

/* return a non-zero value if the received prefix is inconsitent with ours */
static int
prefix_check(struct nd_opt_prefix_info *pinfo,
	     struct rainfo *rai, struct sockaddr_in6 *from)
{
	time_t preferred_time, valid_time;
	struct prefix *pp;
	int inconsistent = 0;
	u_char ntopbuf[INET6_ADDRSTRLEN], prefixbuf[INET6_ADDRSTRLEN];
	struct timeval now;

#if 0				/* impossible */
	if (pinfo->nd_opt_pi_type != ND_OPT_PREFIX_INFORMATION)
		return(0);
#endif

	/*
	 * log if the adveritsed prefix has link-local scope(sanity check?)
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&pinfo->nd_opt_pi_prefix))
		log_info("link-local prefix %s/%d is advertised "
		    "from %s on %s",
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
			prefixbuf, INET6_ADDRSTRLEN),
		    pinfo->nd_opt_pi_prefix_len,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    rai->ifname);

	if ((pp = find_prefix(rai, &pinfo->nd_opt_pi_prefix,
			      pinfo->nd_opt_pi_prefix_len)) == NULL) {
		log_info("prefix %s/%d from %s on %s is not in our list",
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
			prefixbuf, INET6_ADDRSTRLEN),
		    pinfo->nd_opt_pi_prefix_len,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    rai->ifname);
		return(0);
	}

	preferred_time = ntohl(pinfo->nd_opt_pi_preferred_time);
	if (pp->pltimeexpire) {
		/*
		 * The lifetime is decremented in real time, so we should
		 * compare the expiration time.
		 * (RFC 2461 Section 6.2.7.)
		 * XXX: can we really expect that all routers on the link
		 * have synchronized clocks?
		 */
		gettimeofday(&now, NULL);
		preferred_time += now.tv_sec;

		if (rai->clockskew &&
		    llabs(preferred_time - pp->pltimeexpire) > rai->clockskew) {
			log_info("preferred lifetime for %s/%d"
			    " (decr. in real time) inconsistent on %s:"
			    " %lld from %s, %lld from us",
			    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				prefixbuf, INET6_ADDRSTRLEN),
			    pinfo->nd_opt_pi_prefix_len,
			    rai->ifname, (long long)preferred_time,
			    inet_ntop(AF_INET6, &from->sin6_addr,
				ntopbuf, INET6_ADDRSTRLEN),
			    (long long)pp->pltimeexpire);
			inconsistent++;
		}
	} else if (preferred_time != pp->preflifetime)
		log_info("prefeerred lifetime for %s/%d"
		    " inconsistent on %s:"
		    " %lld from %s, %d from us",
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
			prefixbuf, INET6_ADDRSTRLEN),
		    pinfo->nd_opt_pi_prefix_len,
		    rai->ifname, (long long)preferred_time,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    pp->preflifetime);

	valid_time = ntohl(pinfo->nd_opt_pi_valid_time);
	if (pp->vltimeexpire) {
		gettimeofday(&now, NULL);
		valid_time += now.tv_sec;

		if (rai->clockskew &&
		    llabs(valid_time - pp->vltimeexpire) > rai->clockskew) {
			log_info("valid lifetime for %s/%d"
			    " (decr. in real time) inconsistent on %s:"
			    " %lld from %s, %lld from us",
			    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				prefixbuf, INET6_ADDRSTRLEN),
			    pinfo->nd_opt_pi_prefix_len,
			    rai->ifname, (long long)preferred_time,
			    inet_ntop(AF_INET6, &from->sin6_addr,
				ntopbuf, INET6_ADDRSTRLEN),
			    (long long)pp->vltimeexpire);
			inconsistent++;
		}
	} else if (valid_time != pp->validlifetime) {
		log_info("valid lifetime for %s/%d"
		    " inconsistent on %s:"
		    " %lld from %s, %d from us",
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
			prefixbuf, INET6_ADDRSTRLEN),
		    pinfo->nd_opt_pi_prefix_len,
		    rai->ifname, (long long)valid_time,
		    inet_ntop(AF_INET6, &from->sin6_addr,
			ntopbuf, INET6_ADDRSTRLEN),
		    pp->validlifetime);
		inconsistent++;
	}

	return(inconsistent);
}

struct prefix *
find_prefix(struct rainfo *rai, struct in6_addr *prefix, int plen)
{
	struct prefix *pp;
	int bytelen, bitlen;
	u_char bitmask;

	TAILQ_FOREACH(pp, &rai->prefixes, entry) {
		if (plen != pp->prefixlen)
			continue;
		bytelen = plen / 8;
		bitlen = plen % 8;
		bitmask = 0xff << (8 - bitlen);
		if (memcmp((void *)prefix, (void *)&pp->prefix, bytelen))
			continue;
		if (bitlen == 0 ||
		    ((prefix->s6_addr[bytelen] & bitmask) == 
		     (pp->prefix.s6_addr[bytelen] & bitmask))) {
			return(pp);
		}
	}

	return(NULL);
}

/* check if p0/plen0 matches p1/plen1; return 1 if matches, otherwise 0. */
int
prefix_match(struct in6_addr *p0, int plen0,
	     struct in6_addr *p1, int plen1)
{
	int bytelen, bitlen;
	u_char bitmask;

	if (plen0 < plen1)
		return(0);
	bytelen = plen1 / 8;
	bitlen = plen1 % 8;
	bitmask = 0xff << (8 - bitlen);
	if (memcmp((void *)p0, (void *)p1, bytelen))
		return(0);
	if (bitlen == 0 ||
	    ((p0->s6_addr[bytelen] & bitmask) ==
	     (p1->s6_addr[bytelen] & bitmask))) { 
		return(1);
	}

	return(0);
}

static int
nd6_options(struct nd_opt_hdr *hdr, int limit,
	    union nd_opts *ndopts, u_int32_t optflags)
{
	int optlen = 0;

	for (; limit > 0; limit -= optlen) {
		if (limit < sizeof(struct nd_opt_hdr)) {
			log_info("short option header");
			goto bad;
		}

		hdr = (struct nd_opt_hdr *)((caddr_t)hdr + optlen);
		if (hdr->nd_opt_len == 0) {
			log_warnx("bad ND option length(0) (type = %d)",
			    hdr->nd_opt_type);
			goto bad;
		}
		optlen = hdr->nd_opt_len << 3;
		if (optlen > limit) {
			log_info("short option");
			goto bad;
		}

		if (hdr->nd_opt_type > ND_OPT_MTU &&
		    hdr->nd_opt_type != ND_OPT_ROUTE_INFO &&
		    hdr->nd_opt_type != ND_OPT_RDNSS &&
		    hdr->nd_opt_type != ND_OPT_DNSSL)
		{
			log_info("unknown ND option(type %d)",
			    hdr->nd_opt_type);
			continue;
		}

		if ((ndopt_flags[hdr->nd_opt_type] & optflags) == 0) {
			log_info("unexpected ND option(type %d)",
			    hdr->nd_opt_type);
			continue;
		}

		/*
		 * Option length check.  Do it here for all fixed-length
		 * options.
		 */
		if ((hdr->nd_opt_type == ND_OPT_RDNSS && (optlen < 24 ||
		    ((optlen - sizeof(struct nd_opt_rdnss)) % 16 != 0))) ||
		    (hdr->nd_opt_type == ND_OPT_DNSSL && optlen < 16) ||
		    (hdr->nd_opt_type == ND_OPT_MTU &&
		    (optlen != sizeof(struct nd_opt_mtu))) ||
		    ((hdr->nd_opt_type == ND_OPT_PREFIX_INFORMATION &&
		    optlen != sizeof(struct nd_opt_prefix_info)))) {
			log_info("invalid option length");
			continue;
		}

		switch (hdr->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_ROUTE_INFO:
		case ND_OPT_RDNSS:
		case ND_OPT_DNSSL:
			break;	/* we don't care about these options */
		case ND_OPT_MTU:
			if (ndopts->nd_opt_array[hdr->nd_opt_type]) {
				log_info("duplicated ND option (type = %d)",
				    hdr->nd_opt_type);
			}
			ndopts->nd_opt_array[hdr->nd_opt_type] = hdr;
			break;
		case ND_OPT_PREFIX_INFORMATION:
		{
			struct nd_opt	*pfx;
		
			if (ndopts->nd_opts_pi == 0) {
				ndopts->nd_opts_pi =
				    (struct nd_opt_prefix_info *)hdr;
				continue;
			}
			if ((pfx = malloc(sizeof(*pfx))) == NULL) {
				log_warn("malloc");
				goto bad;
			}

			pfx->opt = hdr;
			SLIST_INSERT_HEAD(&ndopts->nd_opts_list, pfx, entry);

			break;
		}
		default:	/* impossible */
			break;
		}
	}

	return(0);

  bad:
	free_ndopts(ndopts);

	return(-1);
}

static void
free_ndopts(union nd_opts *ndopts)
{
	struct nd_opt *opt;

	while (!SLIST_EMPTY(&ndopts->nd_opts_list)) {
		opt = SLIST_FIRST(&ndopts->nd_opts_list);
		SLIST_REMOVE_HEAD(&ndopts->nd_opts_list, entry);
		free(opt);
	}
}

void
sock_open()
{
	struct rainfo	*ra;
	struct icmp6_filter filt;
	struct ipv6_mreq mreq;
	int on;
	/* XXX: should be max MTU attached to the node */
	static u_char answer[1500];

	rcvcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	rcvcmsgbuf = (u_char *)malloc(rcvcmsgbuflen);
	if (rcvcmsgbuf == NULL)
		fatal("malloc");

	sndcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) + 
	    CMSG_SPACE(sizeof(int));
	sndcmsgbuf = (u_char *)malloc(sndcmsgbuflen);
	if (sndcmsgbuf == NULL)
		fatal("malloc");

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		fatal("socket");

	/* specify to tell receiving interface */
	on = 1;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		       sizeof(on)) < 0)
		fatal("IPV6_RECVPKTINFO");

	on = 1;
	/* specify to tell value of hoplimit field of received IP6 hdr */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
		       sizeof(on)) < 0)
		fatal("IPV6_RECVHOPLIMIT");

	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (accept_rr)
		ICMP6_FILTER_SETPASS(ICMP6_ROUTER_RENUMBERING, &filt);
	if (setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		       sizeof(filt)) < 0)
		fatal("IICMP6_FILTER");

	/*
	 * join all routers multicast address on each advertising interface.
	 */
	if (inet_pton(AF_INET6, ALLROUTERS_LINK,
		      &mreq.ipv6mr_multiaddr.s6_addr)
	    != 1)
		fatal("inet_pton failed(library bug?)");
	SLIST_FOREACH(ra, &ralist, entry) {
		mreq.ipv6mr_interface = ra->ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq,
			       sizeof(mreq)) < 0) {
			log_warn("IPV6_JOIN_GROUP(link) on %s", ra->ifname);
			exit(1);
		}
	}

	ra = SLIST_FIRST(&ralist);

	/*
	 * When attending router renumbering, join all-routers site-local
	 * multicast group. 
	 */
	if (accept_rr) {
		if (inet_pton(AF_INET6, ALLROUTERS_SITE,
			      &in6a_site_allrouters) != 1)
			fatal("inet_pton failed(library bug?)");
		mreq.ipv6mr_multiaddr = in6a_site_allrouters;
		if (mcastif) {
			if ((mreq.ipv6mr_interface = if_nametoindex(mcastif))
			    == 0) {
				log_warn("invalid interface: %s", mcastif);
				exit(1);
			}
		} else
			mreq.ipv6mr_interface = ra->ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
			       &mreq, sizeof(mreq)) < 0) {
			log_warn("IPV6_JOIN_GROUP(site) on %s",
			    mcastif ? mcastif : ra->ifname);
			exit(1);
		}
	}
	
	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = (caddr_t)&from;
	rcvmhdr.msg_namelen = sizeof(from);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsgbuflen;

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsgbuflen;
	
	return;
}

/* open a routing socket to watch the routing table */
static void
rtsock_open()
{
	if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0)
		fatal("socket");
}

struct rainfo *
if_indextorainfo(int index)
{
	struct rainfo *rai;

	SLIST_FOREACH(rai, &ralist, entry) {
		if (rai->ifindex == index)
			return(rai);
	}

	return(NULL);		/* search failed */
}

static void
ra_output(rainfo)
struct rainfo *rainfo;
{
	int i;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi;
	struct soliciter *sol;

	if ((iflist[rainfo->ifindex]->ifm_flags & IFF_UP) == 0) {
		log_debug("%s is not up, skip sending RA", rainfo->ifname);
		return;
	}

	make_packet(rainfo);	/* XXX: inefficient */

	sndmhdr.msg_name = (caddr_t)&sin6_allnodes;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)rainfo->ra_data;
	sndmhdr.msg_iov[0].iov_len = rainfo->ra_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = rainfo->ifindex;

	/* specify the hop limit of the packet */
	{
		int hoplimit = 255;

		cm = CMSG_NXTHDR(&sndmhdr, cm);
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_HOPLIMIT;
		cm->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));
	}

	log_debug("send RA on %s, # of waitings = %d",
	    rainfo->ifname, rainfo->waiting); 

	i = sendmsg(sock, &sndmhdr, 0);

	if (i < 0 || i != rainfo->ra_datalen)
		if (i < 0)
			log_warn("sendmsg on %s", rainfo->ifname);

	/*
	 * unicast advertisements
	 * XXX commented out.  reason: though spec does not forbit it, unicast
	 * advert does not really help
	 */
	while (!SLIST_EMPTY(&rainfo->soliciters)) {
		sol = SLIST_FIRST(&rainfo->soliciters);
		SLIST_REMOVE_HEAD(&rainfo->soliciters, entry);
#if 0
		sndmhdr.msg_name = (caddr_t)&sol->addr;
		i = sendmsg(sock, &sndmhdr, 0);
		if (i < 0 || i != rainfo->ra_datalen)
			if (i < 0)
				log_warn("unicast sendmsg on %s",
				    rainfo->ifname);
#endif
		free(sol);
	}

	/* update counter */
	if (rainfo->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS)
		rainfo->initcounter++;
	rainfo->raoutput++;

	/* update timestamp */
	gettimeofday(&rainfo->lastsent, NULL);

	/* reset waiting conter */
	rainfo->waiting = 0;
}

/* process RA timer */
void
ra_timeout(void *data)
{
	struct rainfo *rai = (struct rainfo *)data;

#ifdef notyet
	/* if necessary, reconstruct the packet. */
#endif

	log_debug("RA timer on %s is expired", rai->ifname);

	ra_output(rai);
}

/* update RA timer */
void
ra_timer_update(void *data, struct timeval *tm)
{
	struct rainfo *rai = (struct rainfo *)data;
	long interval;

	/*
	 * Whenever a multicast advertisement is sent from an interface,
	 * the timer is reset to a uniformly-distributed random value
	 * between the interface's configured MinRtrAdvInterval and
	 * MaxRtrAdvInterval (RFC2461 6.2.4).
	 */
	interval = rai->mininterval; 
	interval += arc4random_uniform(rai->maxinterval - rai->mininterval);

	/*
	 * For the first few advertisements (up to
	 * MAX_INITIAL_RTR_ADVERTISEMENTS), if the randomly chosen interval
	 * is greater than MAX_INITIAL_RTR_ADVERT_INTERVAL, the timer
	 * SHOULD be set to MAX_INITIAL_RTR_ADVERT_INTERVAL instead.
	 * (RFC-2461 6.2.4)
	 */
	if (rai->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS &&
	    interval > MAX_INITIAL_RTR_ADVERT_INTERVAL)
		interval = MAX_INITIAL_RTR_ADVERT_INTERVAL;

	tm->tv_sec = interval;
	tm->tv_usec = 0;

	log_debug("RA timer on %s is set to %lld:%ld", rai->ifname,
	    (long long)tm->tv_sec, tm->tv_usec);
}
