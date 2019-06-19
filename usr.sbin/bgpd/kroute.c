/*	$OpenBSD: kroute.c,v 1.236 2019/05/06 09:49:26 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netmpls/mpls.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "log.h"

struct ktable		**krt;
u_int			  krt_size;

struct {
	u_int32_t		rtseq;
	pid_t			pid;
	int			fd;
} kr_state;

struct kroute_node {
	RB_ENTRY(kroute_node)	 entry;
	struct kroute		 r;
	struct kroute_node	*next;
};

struct kroute6_node {
	RB_ENTRY(kroute6_node)	 entry;
	struct kroute6		 r;
	struct kroute6_node	*next;
};

struct knexthop_node {
	RB_ENTRY(knexthop_node)	 entry;
	struct bgpd_addr	 nexthop;
	void			*kroute;
};

struct kredist_node {
	RB_ENTRY(kredist_node)	 entry;
	struct bgpd_addr	 prefix;
	u_int64_t		 rd;
	u_int8_t		 prefixlen;
	u_int8_t		 dynamic;
};

struct kif_kr {
	LIST_ENTRY(kif_kr)	 entry;
	struct kroute_node	*kr;
};

struct kif_kr6 {
	LIST_ENTRY(kif_kr6)	 entry;
	struct kroute6_node	*kr;
};

LIST_HEAD(kif_kr_head, kif_kr);
LIST_HEAD(kif_kr6_head, kif_kr6);

struct kif_node {
	RB_ENTRY(kif_node)	 entry;
	struct kif		 k;
	struct kif_kr_head	 kroute_l;
	struct kif_kr6_head	 kroute6_l;
};

int	ktable_new(u_int, u_int, char *, int, u_int8_t);
void	ktable_free(u_int, u_int8_t);
void	ktable_destroy(struct ktable *, u_int8_t);
struct ktable	*ktable_get(u_int);

int	kr4_change(struct ktable *, struct kroute_full *, u_int8_t);
int	kr6_change(struct ktable *, struct kroute_full *, u_int8_t);
int	krVPN4_change(struct ktable *, struct kroute_full *, u_int8_t);
int	krVPN6_change(struct ktable *, struct kroute_full *, u_int8_t);
int	kr4_delete(struct ktable *, struct kroute_full *, u_int8_t);
int	kr6_delete(struct ktable *, struct kroute_full *, u_int8_t);
int	krVPN4_delete(struct ktable *, struct kroute_full *, u_int8_t);
int	krVPN6_delete(struct ktable *, struct kroute_full *, u_int8_t);
void	kr_net_delete(struct network *);
int	kr_net_match(struct ktable *, struct network_config *, u_int16_t);
struct network *kr_net_find(struct ktable *, struct network *);
void	kr_net_clear(struct ktable *);
void	kr_redistribute(int, struct ktable *, struct kroute *);
void	kr_redistribute6(int, struct ktable *, struct kroute6 *);
struct kroute_full *kr_tofull(struct kroute *);
struct kroute_full *kr6_tofull(struct kroute6 *);
int	kroute_compare(struct kroute_node *, struct kroute_node *);
int	kroute6_compare(struct kroute6_node *, struct kroute6_node *);
int	knexthop_compare(struct knexthop_node *, struct knexthop_node *);
int	kredist_compare(struct kredist_node *, struct kredist_node *);
int	kif_compare(struct kif_node *, struct kif_node *);
void	kr_fib_update_prio(u_int, u_int8_t);

struct kroute_node	*kroute_find(struct ktable *, in_addr_t, u_int8_t,
			    u_int8_t);
struct kroute_node	*kroute_matchgw(struct kroute_node *,
			    struct sockaddr_in *);
int			 kroute_insert(struct ktable *, struct kroute_node *);
int			 kroute_remove(struct ktable *, struct kroute_node *);
void			 kroute_clear(struct ktable *);

struct kroute6_node	*kroute6_find(struct ktable *, const struct in6_addr *,
			    u_int8_t, u_int8_t);
struct kroute6_node	*kroute6_matchgw(struct kroute6_node *,
			    struct sockaddr_in6 *);
int			 kroute6_insert(struct ktable *, struct kroute6_node *);
int			 kroute6_remove(struct ktable *, struct kroute6_node *);
void			 kroute6_clear(struct ktable *);

struct knexthop_node	*knexthop_find(struct ktable *, struct bgpd_addr *);
int			 knexthop_insert(struct ktable *,
			    struct knexthop_node *);
int			 knexthop_remove(struct ktable *,
			    struct knexthop_node *);
void			 knexthop_clear(struct ktable *);

struct kif_node		*kif_find(int);
int			 kif_insert(struct kif_node *);
int			 kif_remove(struct kif_node *, u_int);
void			 kif_clear(u_int);

int			 kif_kr_insert(struct kroute_node *);
int			 kif_kr_remove(struct kroute_node *);

int			 kif_kr6_insert(struct kroute6_node *);
int			 kif_kr6_remove(struct kroute6_node *);

int			 kroute_validate(struct kroute *);
int			 kroute6_validate(struct kroute6 *);
void			 knexthop_validate(struct ktable *,
			    struct knexthop_node *);
void			 knexthop_track(struct ktable *, void *);
void			 knexthop_send_update(struct knexthop_node *);
struct kroute_node	*kroute_match(struct ktable *, in_addr_t, int);
struct kroute6_node	*kroute6_match(struct ktable *, struct in6_addr *, int);
void			 kroute_detach_nexthop(struct ktable *,
			    struct knexthop_node *);

int		protect_lo(struct ktable *);
u_int8_t	prefixlen_classful(in_addr_t);
u_int8_t	mask2prefixlen(in_addr_t);
u_int8_t	mask2prefixlen6(struct sockaddr_in6 *);
uint64_t	ift2ifm(uint8_t);
const char	*get_media_descr(uint64_t);
const char	*get_linkstate(uint8_t, int);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		if_change(u_short, int, struct if_data *, u_int);
void		if_announce(void *, u_int);

int		send_rtmsg(int, int, struct ktable *, struct kroute *,
		    u_int8_t);
int		send_rt6msg(int, int, struct ktable *, struct kroute6 *,
		    u_int8_t);
int		dispatch_rtmsg(u_int);
int		fetchtable(struct ktable *, u_int8_t);
int		fetchifs(int);
int		dispatch_rtmsg_addr(struct rt_msghdr *,
		    struct sockaddr *[RTAX_MAX], struct ktable *);

RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare)
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare)

RB_PROTOTYPE(kroute6_tree, kroute6_node, entry, kroute6_compare)
RB_GENERATE(kroute6_tree, kroute6_node, entry, kroute6_compare)

RB_PROTOTYPE(knexthop_tree, knexthop_node, entry, knexthop_compare)
RB_GENERATE(knexthop_tree, knexthop_node, entry, knexthop_compare)

RB_PROTOTYPE(kredist_tree, kredist_node, entry, kredist_compare)
RB_GENERATE(kredist_tree, kredist_node, entry, kredist_compare)

RB_HEAD(kif_tree, kif_node)		kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare)
RB_GENERATE(kif_tree, kif_node, entry, kif_compare)

#define KT2KNT(x)	(&(ktable_get((x)->nhtableid)->knt))

/*
 * exported functions
 */

int
kr_init(void)
{
	int		opt = 0, rcvbuf, default_rcvbuf;
	unsigned int	tid = RTABLE_ANY;
	socklen_t	optlen;

	if ((kr_state.fd = socket(AF_ROUTE,
	    SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) == -1) {
		log_warn("%s: socket", __func__);
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_warn("%s: setsockopt", __func__);	/* not fatal */

	/* grow receive buffer, don't wanna miss messages */
	optlen = sizeof(default_rcvbuf);
	if (getsockopt(kr_state.fd, SOL_SOCKET, SO_RCVBUF,
	    &default_rcvbuf, &optlen) == -1)
		log_warn("%s: getsockopt SOL_SOCKET SO_RCVBUF", __func__);
	else
		for (rcvbuf = MAX_RTSOCK_BUF;
		    rcvbuf > default_rcvbuf &&
		    setsockopt(kr_state.fd, SOL_SOCKET, SO_RCVBUF,
		    &rcvbuf, sizeof(rcvbuf)) == -1 && errno == ENOBUFS;
		    rcvbuf /= 2)
			;	/* nothing */

	if (setsockopt(kr_state.fd, AF_ROUTE, ROUTE_TABLEFILTER, &tid,
	    sizeof(tid)) == -1) {
		log_warn("%s: setsockopt AF_ROUTE ROUTE_TABLEFILTER", __func__);
		return (-1);
	}

	kr_state.pid = getpid();
	kr_state.rtseq = 1;

	RB_INIT(&kit);

	if (fetchifs(0) == -1)
		return (-1);

	return (kr_state.fd);
}

int
ktable_new(u_int rtableid, u_int rdomid, char *name, int fs, u_int8_t fib_prio)
{
	struct ktable	**xkrt;
	struct ktable	 *kt;
	size_t		  oldsize;

	/* resize index table if needed */
	if (rtableid >= krt_size) {
		oldsize = sizeof(struct ktable *) * krt_size;
		if ((xkrt = reallocarray(krt, rtableid + 1,
		    sizeof(struct ktable *))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		krt = xkrt;
		krt_size = rtableid + 1;
		bzero((char *)krt + oldsize,
		    krt_size * sizeof(struct ktable *) - oldsize);
	}

	if (krt[rtableid])
		fatalx("ktable_new: table already exists.");

	/* allocate new element */
	kt = krt[rtableid] = calloc(1, sizeof(struct ktable));
	if (kt == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}

	/* initialize structure ... */
	strlcpy(kt->descr, name, sizeof(kt->descr));
	RB_INIT(&kt->krt);
	RB_INIT(&kt->krt6);
	RB_INIT(&kt->knt);
	TAILQ_INIT(&kt->krn);
	kt->fib_conf = kt->fib_sync = fs;
	kt->rtableid = rtableid;
	kt->nhtableid = rdomid;
	/* bump refcount of rdomain table for the nexthop lookups */
	ktable_get(kt->nhtableid)->nhrefcnt++;

	/* ... and load it */
	if (fetchtable(kt, fib_prio) == -1)
		return (-1);
	if (protect_lo(kt) == -1)
		return (-1);

	/* everything is up and running */
	kt->state = RECONF_REINIT;
	log_debug("%s: %s for rtableid %d", __func__, name, rtableid);
	return (0);
}

void
ktable_free(u_int rtableid, u_int8_t fib_prio)
{
	struct ktable	*kt, *nkt;

	if ((kt = ktable_get(rtableid)) == NULL)
		return;

	/* decouple from kernel, no new routes will be entered from here */
	kr_fib_decouple(kt->rtableid, fib_prio);

	/* first unhook from the nexthop table */
	nkt = ktable_get(kt->nhtableid);
	nkt->nhrefcnt--;

	/*
	 * Evil little details:
	 *   If kt->nhrefcnt > 0 then kt == nkt and nothing needs to be done.
	 *   If kt != nkt then kt->nhrefcnt must be 0 and kt must be killed.
	 *   If nkt is no longer referenced it must be killed (possible double
	 *   free so check that kt != nkt).
	 */
	if (kt != nkt && nkt->nhrefcnt <= 0)
		ktable_destroy(nkt, fib_prio);
	if (kt->nhrefcnt <= 0)
		ktable_destroy(kt, fib_prio);
}

void
ktable_destroy(struct ktable *kt, u_int8_t fib_prio)
{
	/* decouple just to be sure, does not hurt */
	kr_fib_decouple(kt->rtableid, fib_prio);

	log_debug("%s: freeing ktable %s rtableid %u", __func__, kt->descr,
	    kt->rtableid);
	knexthop_clear(kt);
	kroute_clear(kt);
	kroute6_clear(kt);
	kr_net_clear(kt);

	krt[kt->rtableid] = NULL;
	free(kt);
}

struct ktable *
ktable_get(u_int rtableid)
{
	if (rtableid >= krt_size)
		return (NULL);
	return (krt[rtableid]);
}

int
ktable_update(u_int rtableid, char *name, int flags, u_int8_t fib_prio)
{
	struct ktable	*kt, *rkt;
	u_int		 rdomid;

	if (!ktable_exists(rtableid, &rdomid))
		fatalx("King Bula lost a table");	/* may not happen */

	if (rdomid != rtableid || flags & F_RIB_NOFIB) {
		rkt = ktable_get(rdomid);
		if (rkt == NULL) {
			char buf[32];
			snprintf(buf, sizeof(buf), "rdomain_%d", rdomid);
			if (ktable_new(rdomid, rdomid, buf, 0, fib_prio))
				return (-1);
		} else {
			/* there is no need for full fib synchronisation if
			 * the table is only used for nexthop lookups.
			 */
			if (rkt->state == RECONF_DELETE) {
				rkt->fib_conf = 0;
				rkt->state = RECONF_KEEP;
			}
		}
	}

	if (flags & F_RIB_HASNOFIB)
		/* only rdomain table must exist */
		return (0);

	kt = ktable_get(rtableid);
	if (kt == NULL) {
		if (ktable_new(rtableid, rdomid, name,
		    !(flags & F_RIB_NOFIBSYNC), fib_prio))
			return (-1);
	} else {
		/* fib sync has higher preference then no sync */
		if (kt->state == RECONF_DELETE) {
			kt->fib_conf = !(flags & F_RIB_NOFIBSYNC);
			kt->state = RECONF_KEEP;
		} else if (!kt->fib_conf)
			kt->fib_conf = !(flags & F_RIB_NOFIBSYNC);

		strlcpy(kt->descr, name, sizeof(kt->descr));
	}
	return (0);
}

int
ktable_exists(u_int rtableid, u_int *rdomid)
{
	size_t			 len;
	struct rt_tableinfo	 info;
	int			 mib[6];

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_TABLE;
	mib[5] = rtableid;

	len = sizeof(info);
	if (sysctl(mib, 6, &info, &len, NULL, 0) == -1) {
		if (errno == ENOENT)
			/* table nonexistent */
			return (0);
		log_warn("%s: sysctl", __func__);
		/* must return 0 so that the table is considered non-existent */
		return (0);
	}
	if (rdomid)
		*rdomid = info.rti_domainid;
	return (1);
}

int
kr_change(u_int rtableid, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct ktable		*kt;

	if ((kt = ktable_get(rtableid)) == NULL)
		/* too noisy during reloads, just ignore */
		return (0);
	switch (kl->prefix.aid) {
	case AID_INET:
		return (kr4_change(kt, kl, fib_prio));
	case AID_INET6:
		return (kr6_change(kt, kl, fib_prio));
	case AID_VPN_IPv4:
		return (krVPN4_change(kt, kl, fib_prio));
	case AID_VPN_IPv6:
		return (krVPN6_change(kt, kl, fib_prio));
	}
	log_warnx("%s: not handled AID", __func__);
	return (-1);
}

int
kr4_change(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute_node	*kr;
	int			 action = RTM_ADD;
	u_int16_t		 labelid;

	/* for blackhole and reject routes nexthop needs to be 127.0.0.1 */
	if (kl->flags & (F_BLACKHOLE|F_REJECT))
		kl->nexthop.v4.s_addr = htonl(INADDR_LOOPBACK);
	/* nexthop within 127/8 -> ignore silently */
	else if ((kl->nexthop.v4.s_addr & htonl(IN_CLASSA_NET)) ==
	    htonl(INADDR_LOOPBACK & IN_CLASSA_NET))
		return (0);

	labelid = rtlabel_name2id(kl->label);

	if ((kr = kroute_find(kt, kl->prefix.v4.s_addr, kl->prefixlen,
	    fib_prio)) != NULL)
		action = RTM_CHANGE;

	if (action == RTM_ADD) {
		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		kr->r.prefix.s_addr = kl->prefix.v4.s_addr;
		kr->r.prefixlen = kl->prefixlen;
		kr->r.nexthop.s_addr = kl->nexthop.v4.s_addr;
		kr->r.flags = kl->flags | F_BGPD_INSERTED;
		kr->r.priority = fib_prio;
		kr->r.labelid = labelid;

		if (kroute_insert(kt, kr) == -1) {
			free(kr);
			return (-1);
		}
	} else {
		kr->r.nexthop.s_addr = kl->nexthop.v4.s_addr;
		rtlabel_unref(kr->r.labelid);
		kr->r.labelid = labelid;
		if (kl->flags & F_BLACKHOLE)
			kr->r.flags |= F_BLACKHOLE;
		else
			kr->r.flags &= ~F_BLACKHOLE;
		if (kl->flags & F_REJECT)
			kr->r.flags |= F_REJECT;
		else
			kr->r.flags &= ~F_REJECT;
	}

	if (send_rtmsg(kr_state.fd, action, kt, &kr->r, fib_prio) == -1)
		return (-1);

	return (0);
}

int
kr6_change(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute6_node	*kr6;
	struct in6_addr		 lo6 = IN6ADDR_LOOPBACK_INIT;
	int			 action = RTM_ADD;
	u_int16_t		 labelid;

	/* for blackhole and reject routes nexthop needs to be ::1 */
	if (kl->flags & (F_BLACKHOLE|F_REJECT))
		bcopy(&lo6, &kl->nexthop.v6, sizeof(kl->nexthop.v6));
	/* nexthop to loopback -> ignore silently */
	else if (IN6_IS_ADDR_LOOPBACK(&kl->nexthop.v6))
		return (0);

	labelid = rtlabel_name2id(kl->label);

	if ((kr6 = kroute6_find(kt, &kl->prefix.v6, kl->prefixlen, fib_prio)) !=
	    NULL)
		action = RTM_CHANGE;

	if (action == RTM_ADD) {
		if ((kr6 = calloc(1, sizeof(struct kroute6_node))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		memcpy(&kr6->r.prefix, &kl->prefix.v6, sizeof(struct in6_addr));
		kr6->r.prefixlen = kl->prefixlen;
		memcpy(&kr6->r.nexthop, &kl->nexthop.v6,
		    sizeof(struct in6_addr));
		kr6->r.flags = kl->flags | F_BGPD_INSERTED;
		kr6->r.priority = fib_prio;
		kr6->r.labelid = labelid;

		if (kroute6_insert(kt, kr6) == -1) {
			free(kr6);
			return (-1);
		}
	} else {
		memcpy(&kr6->r.nexthop, &kl->nexthop.v6,
		    sizeof(struct in6_addr));
		rtlabel_unref(kr6->r.labelid);
		kr6->r.labelid = labelid;
		if (kl->flags & F_BLACKHOLE)
			kr6->r.flags |= F_BLACKHOLE;
		else
			kr6->r.flags &= ~F_BLACKHOLE;
		if (kl->flags & F_REJECT)
			kr6->r.flags |= F_REJECT;
		else
			kr6->r.flags &= ~F_REJECT;
	}

	if (send_rt6msg(kr_state.fd, action, kt, &kr6->r, fib_prio) == -1)
		return (-1);

	return (0);
}

int
krVPN4_change(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute_node	*kr;
	int			 action = RTM_ADD;
	u_int32_t		 mplslabel = 0;
	u_int16_t		 labelid;

	/* nexthop within 127/8 -> ignore silently */
	if ((kl->nexthop.v4.s_addr & htonl(IN_CLASSA_NET)) ==
	    htonl(INADDR_LOOPBACK & IN_CLASSA_NET))
		return (0);

	/* only single MPLS label are supported for now */
	if (kl->prefix.vpn4.labellen != 3) {
		log_warnx("%s: %s/%u has not a single label", __func__,
		    log_addr(&kl->prefix), kl->prefixlen);
		return (0);
	}
	mplslabel = (kl->prefix.vpn4.labelstack[0] << 24) |
	    (kl->prefix.vpn4.labelstack[1] << 16) |
	    (kl->prefix.vpn4.labelstack[2] << 8);
	mplslabel = htonl(mplslabel);

	labelid = rtlabel_name2id(kl->label);

	/* for blackhole and reject routes nexthop needs to be 127.0.0.1 */
	if (kl->flags & (F_BLACKHOLE|F_REJECT))
		kl->nexthop.v4.s_addr = htonl(INADDR_LOOPBACK);

	if ((kr = kroute_find(kt, kl->prefix.vpn4.addr.s_addr, kl->prefixlen,
	    fib_prio)) != NULL)
		action = RTM_CHANGE;

	if (action == RTM_ADD) {
		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		kr->r.prefix.s_addr = kl->prefix.vpn4.addr.s_addr;
		kr->r.prefixlen = kl->prefixlen;
		kr->r.nexthop.s_addr = kl->nexthop.v4.s_addr;
		kr->r.flags = kl->flags | F_BGPD_INSERTED | F_MPLS;
		kr->r.priority = fib_prio;
		kr->r.labelid = labelid;
		kr->r.mplslabel = mplslabel;
		kr->r.ifindex = kl->ifindex;

		if (kroute_insert(kt, kr) == -1) {
			free(kr);
			return (-1);
		}
	} else {
		kr->r.mplslabel = mplslabel;
		kr->r.ifindex = kl->ifindex;
		kr->r.nexthop.s_addr = kl->nexthop.v4.s_addr;
		rtlabel_unref(kr->r.labelid);
		kr->r.labelid = labelid;
		if (kl->flags & F_BLACKHOLE)
			kr->r.flags |= F_BLACKHOLE;
		else
			kr->r.flags &= ~F_BLACKHOLE;
		if (kl->flags & F_REJECT)
			kr->r.flags |= F_REJECT;
		else
			kr->r.flags &= ~F_REJECT;
	}

	if (send_rtmsg(kr_state.fd, action, kt, &kr->r, fib_prio) == -1)
		return (-1);

	return (0);
}

int
krVPN6_change(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute6_node	*kr6;
	struct in6_addr		 lo6 = IN6ADDR_LOOPBACK_INIT;
	int			 action = RTM_ADD;
	u_int32_t		 mplslabel = 0;
	u_int16_t		 labelid;

	/* nexthop to loopback -> ignore silently */
	if (IN6_IS_ADDR_LOOPBACK(&kl->nexthop.v6))
		return (0);

	/* only single MPLS label are supported for now */
	if (kl->prefix.vpn6.labellen != 3) {
		log_warnx("%s: %s/%u has not a single label", __func__,
		    log_addr(&kl->prefix), kl->prefixlen);
		return (0);
	}
	mplslabel = (kl->prefix.vpn6.labelstack[0] << 24) |
	    (kl->prefix.vpn6.labelstack[1] << 16) |
	    (kl->prefix.vpn6.labelstack[2] << 8);
	mplslabel = htonl(mplslabel);

	/* for blackhole and reject routes nexthop needs to be ::1 */
	if (kl->flags & (F_BLACKHOLE|F_REJECT))
		bcopy(&lo6, &kl->nexthop.v6, sizeof(kl->nexthop.v6));

	labelid = rtlabel_name2id(kl->label);

	if ((kr6 = kroute6_find(kt, &kl->prefix.vpn6.addr, kl->prefixlen,
	    fib_prio)) != NULL)
		action = RTM_CHANGE;

	if (action == RTM_ADD) {
		if ((kr6 = calloc(1, sizeof(struct kroute6_node))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		memcpy(&kr6->r.prefix, &kl->prefix.vpn6.addr,
		    sizeof(struct in6_addr));
		kr6->r.prefixlen = kl->prefixlen;
		memcpy(&kr6->r.nexthop, &kl->nexthop.v6,
		    sizeof(struct in6_addr));
		kr6->r.flags = kl->flags | F_BGPD_INSERTED | F_MPLS;
		kr6->r.priority = fib_prio;
		kr6->r.labelid = labelid;
		kr6->r.mplslabel = mplslabel;
		kr6->r.ifindex = kl->ifindex;

		if (kroute6_insert(kt, kr6) == -1) {
			free(kr6);
			return (-1);
		}
	} else {
		kr6->r.mplslabel = mplslabel;
		kr6->r.ifindex = kl->ifindex;
		memcpy(&kr6->r.nexthop, &kl->nexthop.v6,
		    sizeof(struct in6_addr));
		rtlabel_unref(kr6->r.labelid);
		kr6->r.labelid = labelid;
		if (kl->flags & F_BLACKHOLE)
			kr6->r.flags |= F_BLACKHOLE;
		else
			kr6->r.flags &= ~F_BLACKHOLE;
		if (kl->flags & F_REJECT)
			kr6->r.flags |= F_REJECT;
		else
			kr6->r.flags &= ~F_REJECT;
	}

	if (send_rt6msg(kr_state.fd, action, kt, &kr6->r, fib_prio) == -1)
		return (-1);

	return (0);
}

int
kr_delete(u_int rtableid, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct ktable		*kt;

	if ((kt = ktable_get(rtableid)) == NULL)
		/* too noisy during reloads, just ignore */
		return (0);

	switch (kl->prefix.aid) {
	case AID_INET:
		return (kr4_delete(kt, kl, fib_prio));
	case AID_INET6:
		return (kr6_delete(kt, kl, fib_prio));
	case AID_VPN_IPv4:
		return (krVPN4_delete(kt, kl, fib_prio));
	case AID_VPN_IPv6:
		return (krVPN6_delete(kt, kl, fib_prio));
	}
	log_warnx("%s: not handled AID", __func__);
	return (-1);
}

int
kr4_delete(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute_node	*kr;

	if ((kr = kroute_find(kt, kl->prefix.v4.s_addr, kl->prefixlen,
	    fib_prio)) == NULL)
		return (0);

	if (!(kr->r.flags & F_BGPD_INSERTED))
		return (0);

	if (send_rtmsg(kr_state.fd, RTM_DELETE, kt, &kr->r, fib_prio) == -1)
		return (-1);

	rtlabel_unref(kr->r.labelid);

	if (kroute_remove(kt, kr) == -1)
		return (-1);

	return (0);
}

int
kr6_delete(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute6_node	*kr6;

	if ((kr6 = kroute6_find(kt, &kl->prefix.v6, kl->prefixlen, fib_prio)) ==
	    NULL)
		return (0);

	if (!(kr6->r.flags & F_BGPD_INSERTED))
		return (0);

	if (send_rt6msg(kr_state.fd, RTM_DELETE, kt, &kr6->r, fib_prio) == -1)
		return (-1);

	rtlabel_unref(kr6->r.labelid);

	if (kroute6_remove(kt, kr6) == -1)
		return (-1);

	return (0);
}

int
krVPN4_delete(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute_node	*kr;

	if ((kr = kroute_find(kt, kl->prefix.vpn4.addr.s_addr, kl->prefixlen,
	    fib_prio)) == NULL)
		return (0);

	if (!(kr->r.flags & F_BGPD_INSERTED))
		return (0);

	if (send_rtmsg(kr_state.fd, RTM_DELETE, kt, &kr->r, fib_prio) == -1)
		return (-1);

	rtlabel_unref(kr->r.labelid);

	if (kroute_remove(kt, kr) == -1)
		return (-1);

	return (0);
}

int
krVPN6_delete(struct ktable *kt, struct kroute_full *kl, u_int8_t fib_prio)
{
	struct kroute6_node	*kr6;

	if ((kr6 = kroute6_find(kt, &kl->prefix.vpn6.addr, kl->prefixlen,
	    fib_prio)) == NULL)
		return (0);

	if (!(kr6->r.flags & F_BGPD_INSERTED))
		return (0);

	if (send_rt6msg(kr_state.fd, RTM_DELETE, kt, &kr6->r, fib_prio) == -1)
		return (-1);

	rtlabel_unref(kr6->r.labelid);

	if (kroute6_remove(kt, kr6) == -1)
		return (-1);

	return (0);
}

void
kr_shutdown(u_int8_t fib_prio, u_int rdomain)
{
	u_int	i;

	for (i = krt_size; i > 0; i--)
		ktable_free(i - 1, fib_prio);
	kif_clear(rdomain);
	free(krt);
}

void
kr_fib_couple(u_int rtableid, u_int8_t fib_prio)
{
	struct ktable		*kt;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	if ((kt = ktable_get(rtableid)) == NULL)  /* table does not exist */
		return;

	if (kt->fib_sync)	/* already coupled */
		return;

	kt->fib_sync = 1;

	RB_FOREACH(kr, kroute_tree, &kt->krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_ADD, kt, &kr->r, fib_prio);
	RB_FOREACH(kr6, kroute6_tree, &kt->krt6)
		if ((kr6->r.flags & F_BGPD_INSERTED))
			send_rt6msg(kr_state.fd, RTM_ADD, kt, &kr6->r,
			    fib_prio);

	log_info("kernel routing table %u (%s) coupled", kt->rtableid,
	    kt->descr);
}

void
kr_fib_couple_all(u_int8_t fib_prio)
{
	u_int	 i;

	for (i = krt_size; i > 0; i--)
		kr_fib_couple(i - 1, fib_prio);
}

void
kr_fib_decouple(u_int rtableid, u_int8_t fib_prio)
{
	struct ktable		*kt;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	if ((kt = ktable_get(rtableid)) == NULL)  /* table does not exist */
		return;

	if (!kt->fib_sync)	/* already decoupled */
		return;

	RB_FOREACH(kr, kroute_tree, &kt->krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_DELETE, kt, &kr->r,
			    fib_prio);
	RB_FOREACH(kr6, kroute6_tree, &kt->krt6)
		if ((kr6->r.flags & F_BGPD_INSERTED))
			send_rt6msg(kr_state.fd, RTM_DELETE, kt, &kr6->r,
			    fib_prio);

	kt->fib_sync = 0;

	log_info("kernel routing table %u (%s) decoupled", kt->rtableid,
	    kt->descr);
}

void
kr_fib_decouple_all(u_int8_t fib_prio)
{
	u_int	 i;

	for (i = krt_size; i > 0; i--)
		kr_fib_decouple(i - 1, fib_prio);
}

void
kr_fib_update_prio(u_int rtableid, u_int8_t fib_prio)
{
	struct ktable		*kt;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	if ((kt = ktable_get(rtableid)) == NULL)  /* table does not exist */
		return;

	RB_FOREACH(kr, kroute_tree, &kt->krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			kr->r.priority = fib_prio;

	RB_FOREACH(kr6, kroute6_tree, &kt->krt6)
		if ((kr6->r.flags & F_BGPD_INSERTED))
			kr6->r.priority = fib_prio;
}

void
kr_fib_update_prio_all(u_int8_t fib_prio)
{
	u_int	 i;

	for (i = krt_size; i > 0; i--)
		kr_fib_update_prio(i - 1, fib_prio);
}

int
kr_dispatch_msg(u_int rdomain)
{
	return (dispatch_rtmsg(rdomain));
}

int
kr_nexthop_add(u_int rtableid, struct bgpd_addr *addr, struct bgpd_config *conf)
{
	struct ktable		*kt;
	struct knexthop_node	*h;

	if (rtableid == 0)
		rtableid = conf->default_tableid;

	if ((kt = ktable_get(rtableid)) == NULL) {
		log_warnx("%s: non-existent rtableid %d", __func__, rtableid);
		return (0);
	}
	if ((h = knexthop_find(kt, addr)) != NULL) {
		/* should not happen... this is actually an error path */
		knexthop_send_update(h);
	} else {
		if ((h = calloc(1, sizeof(struct knexthop_node))) == NULL) {
			log_warn("%s", __func__);
			return (-1);
		}
		memcpy(&h->nexthop, addr, sizeof(h->nexthop));

		if (knexthop_insert(kt, h) == -1)
			return (-1);
	}

	return (0);
}

void
kr_nexthop_delete(u_int rtableid, struct bgpd_addr *addr,
    struct bgpd_config *conf)
{
	struct ktable		*kt;
	struct knexthop_node	*kn;

	if (rtableid == 0)
		rtableid = conf->default_tableid;

	if ((kt = ktable_get(rtableid)) == NULL) {
		log_warnx("%s: non-existent rtableid %d", __func__,
		    rtableid);
		return;
	}
	if ((kn = knexthop_find(kt, addr)) == NULL)
		return;

	knexthop_remove(kt, kn);
}

static struct ctl_show_interface *
kr_show_interface(struct kif *kif)
{
	static struct ctl_show_interface iface;
	uint64_t ifms_type;

	bzero(&iface, sizeof(iface));
	strlcpy(iface.ifname, kif->ifname, sizeof(iface.ifname));

	snprintf(iface.linkstate, sizeof(iface.linkstate),
	    "%s", get_linkstate(kif->if_type, kif->link_state));

	if ((ifms_type = ift2ifm(kif->if_type)) != 0)
		snprintf(iface.media, sizeof(iface.media),
		    "%s", get_media_descr(ifms_type));

	iface.baudrate = kif->baudrate;
	iface.rdomain = kif->rdomain;
	iface.nh_reachable = kif->nh_reachable;
	iface.is_up = (kif->flags & IFF_UP) == IFF_UP;

	return &iface;
}

void
kr_show_route(struct imsg *imsg)
{
	struct ktable		*kt;
	struct kroute_node	*kr, *kn;
	struct kroute6_node	*kr6, *kn6;
	struct bgpd_addr	*addr;
	int			 flags;
	sa_family_t		 af;
	struct ctl_show_nexthop	 snh;
	struct knexthop_node	*h;
	struct kif_node		*kif;
	u_int			 i;
	u_short			 ifindex = 0;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(flags) +
		    sizeof(af)) {
			log_warnx("%s: wrong imsg len", __func__);
			break;
		}
		kt = ktable_get(imsg->hdr.peerid);
		if (kt == NULL) {
			log_warnx("%s: table %u does not exist", __func__,
			    imsg->hdr.peerid);
			break;
		}
		memcpy(&flags, imsg->data, sizeof(flags));
		memcpy(&af, (char *)imsg->data + sizeof(flags), sizeof(af));
		if (!af || af == AF_INET)
			RB_FOREACH(kr, kroute_tree, &kt->krt) {
				if (flags && (kr->r.flags & flags) == 0)
					continue;
				kn = kr;
				do {
					send_imsg_session(IMSG_CTL_KROUTE,
					    imsg->hdr.pid, kr_tofull(&kn->r),
					    sizeof(struct kroute_full));
				} while ((kn = kn->next) != NULL);
			}
		if (!af || af == AF_INET6)
			RB_FOREACH(kr6, kroute6_tree, &kt->krt6) {
				if (flags && (kr6->r.flags & flags) == 0)
					continue;
				kn6 = kr6;
				do {
					send_imsg_session(IMSG_CTL_KROUTE,
					    imsg->hdr.pid, kr6_tofull(&kn6->r),
					    sizeof(struct kroute_full));
				} while ((kn6 = kn6->next) != NULL);
			}
		break;
	case IMSG_CTL_KROUTE_ADDR:
		if (imsg->hdr.len != IMSG_HEADER_SIZE +
		    sizeof(struct bgpd_addr)) {
			log_warnx("%s: wrong imsg len", __func__);
			break;
		}
		kt = ktable_get(imsg->hdr.peerid);
		if (kt == NULL) {
			log_warnx("%s: table %u does not exist", __func__,
			    imsg->hdr.peerid);
			break;
		}
		addr = imsg->data;
		kr = NULL;
		switch (addr->aid) {
		case AID_INET:
			kr = kroute_match(kt, addr->v4.s_addr, 1);
			if (kr != NULL)
				send_imsg_session(IMSG_CTL_KROUTE,
				    imsg->hdr.pid, kr_tofull(&kr->r),
				    sizeof(struct kroute_full));
			break;
		case AID_INET6:
			kr6 = kroute6_match(kt, &addr->v6, 1);
			if (kr6 != NULL)
				send_imsg_session(IMSG_CTL_KROUTE,
				    imsg->hdr.pid, kr6_tofull(&kr6->r),
				    sizeof(struct kroute_full));
			break;
		}
		break;
	case IMSG_CTL_SHOW_NEXTHOP:
		kt = ktable_get(imsg->hdr.peerid);
		if (kt == NULL) {
			log_warnx("%s: table %u does not exist", __func__,
			    imsg->hdr.peerid);
			break;
		}
		RB_FOREACH(h, knexthop_tree, KT2KNT(kt)) {
			bzero(&snh, sizeof(snh));
			memcpy(&snh.addr, &h->nexthop, sizeof(snh.addr));
			if (h->kroute != NULL) {
				switch (h->nexthop.aid) {
				case AID_INET:
					kr = h->kroute;
					snh.valid = kroute_validate(&kr->r);
					snh.krvalid = 1;
					memcpy(&snh.kr.kr4, &kr->r,
					    sizeof(snh.kr.kr4));
					ifindex = kr->r.ifindex;
					break;
				case AID_INET6:
					kr6 = h->kroute;
					snh.valid = kroute6_validate(&kr6->r);
					snh.krvalid = 1;
					memcpy(&snh.kr.kr6, &kr6->r,
					    sizeof(snh.kr.kr6));
					ifindex = kr6->r.ifindex;
					break;
				}
				if ((kif = kif_find(ifindex)) != NULL)
					memcpy(&snh.iface,
					    kr_show_interface(&kif->k),
					    sizeof(snh.iface));
			}
			send_imsg_session(IMSG_CTL_SHOW_NEXTHOP, imsg->hdr.pid,
			    &snh, sizeof(snh));
		}
		break;
	case IMSG_CTL_SHOW_INTERFACE:
		RB_FOREACH(kif, kif_tree, &kit)
			send_imsg_session(IMSG_CTL_SHOW_INTERFACE,
			    imsg->hdr.pid, kr_show_interface(&kif->k),
			    sizeof(struct ctl_show_interface));
		break;
	case IMSG_CTL_SHOW_FIB_TABLES:
		for (i = 0; i < krt_size; i++) {
			struct ktable	ktab;

			if ((kt = ktable_get(i)) == NULL)
				continue;

			ktab = *kt;
			/* do not leak internal information */
			RB_INIT(&ktab.krt);
			RB_INIT(&ktab.krt6);
			RB_INIT(&ktab.knt);
			TAILQ_INIT(&ktab.krn);

			send_imsg_session(IMSG_CTL_SHOW_FIB_TABLES,
			    imsg->hdr.pid, &ktab, sizeof(ktab));
		}
		break;
	default:	/* nada */
		break;
	}

	send_imsg_session(IMSG_CTL_END, imsg->hdr.pid, NULL, 0);
}

void
kr_ifinfo(char *ifname)
{
	struct kif_node	*kif;

	RB_FOREACH(kif, kif_tree, &kit)
		if (!strcmp(ifname, kif->k.ifname)) {
			send_imsg_session(IMSG_IFINFO, 0,
			    &kif->k, sizeof(kif->k));
			return;
		}
}

void
kr_net_delete(struct network *n)
{
	filterset_free(&n->net.attrset);
	free(n);
}

static int
kr_net_redist_add(struct ktable *kt, struct network_config *net,
    struct filter_set_head *attr, int dynamic)
{
	struct kredist_node *r, *xr;

	if ((r = calloc(1, sizeof(*r))) == NULL)
		fatal("%s", __func__);
	r->prefix = net->prefix;
	r->prefixlen = net->prefixlen;
	r->rd = net->rd;
	r->dynamic = dynamic;

	xr = RB_INSERT(kredist_tree, &kt->kredist, r);
	if (xr != NULL) {
		free(r);

		if (dynamic != xr->dynamic && dynamic) {
			/*
			 * ignore update a non-dynamic announcement is
			 * already present which has preference.
			 */
			return 0;
		}
		/*
		 * only equal or non-dynamic announcement ends up here.
		 * In both cases reset the dynamic flag (nop for equal) and
		 * redistribute.
		 */
		xr->dynamic = dynamic;
	}

	if (send_network(IMSG_NETWORK_ADD, net, attr) == -1)
		log_warnx("%s: faild to send network update", __func__);
	return 1;
}

static void
kr_net_redist_del(struct ktable *kt, struct network_config *net, int dynamic)
{
	struct kredist_node *r, node;

	bzero(&node, sizeof(node));
	node.prefix = net->prefix;
	node.prefixlen = net->prefixlen;
	node.rd = net->rd;

	r = RB_FIND(kredist_tree, &kt->kredist, &node);
	if (r == NULL || dynamic != r->dynamic)
		return;

	if (RB_REMOVE(kredist_tree, &kt->kredist, r) == NULL) {
		log_warnx("%s: failed to remove network %s/%u", __func__,
		    log_addr(&node.prefix), node.prefixlen);
		return;
	}
	free(r);

	if (send_network(IMSG_NETWORK_REMOVE, net, NULL) == -1)
		log_warnx("%s: faild to send network removal", __func__);
}

int
kr_net_match(struct ktable *kt, struct network_config *net, u_int16_t flags)
{
	struct network		*xn;

	TAILQ_FOREACH(xn, &kt->krn, entry) {
		if (xn->net.prefix.aid != net->prefix.aid)
			continue;
		switch (xn->net.type) {
		case NETWORK_DEFAULT:
			/* static match already redistributed */
			continue;
		case NETWORK_STATIC:
			if (flags & F_STATIC)
				break;
			continue;
		case NETWORK_CONNECTED:
			if (flags & F_CONNECTED)
				break;
			continue;
		case NETWORK_RTLABEL:
			if (net->rtlabel == xn->net.rtlabel)
				break;
			continue;
		case NETWORK_PRIORITY:
			if (net->priority == xn->net.priority)
				break;
			continue;
		case NETWORK_MRTCLONE:
		case NETWORK_PREFIXSET:
			/* must not happen */
			log_warnx("%s: found a NETWORK_PREFIXSET, "
			    "please send a bug report", __func__);
			continue;
		}

		net->rd = xn->net.rd;
		if (kr_net_redist_add(kt, net, &xn->net.attrset, 1))
			return (1);
	}
	return (0);
}

struct network *
kr_net_find(struct ktable *kt, struct network *n)
{
	struct network		*xn;

	TAILQ_FOREACH(xn, &kt->krn, entry) {
		if (n->net.type != xn->net.type ||
		    n->net.prefixlen != xn->net.prefixlen ||
		    n->net.rd != xn->net.rd)
			continue;
		if (memcmp(&n->net.prefix, &xn->net.prefix,
		    sizeof(n->net.prefix)) == 0)
			return (xn);
	}
	return (NULL);
}

void
kr_net_reload(u_int rtableid, u_int64_t rd, struct network_head *nh)
{
	struct network		*n, *xn;
	struct ktable		*kt;

	if ((kt = ktable_get(rtableid)) == NULL)
		fatalx("%s: non-existent rtableid %d", __func__, rtableid);

	while ((n = TAILQ_FIRST(nh)) != NULL) {
		TAILQ_REMOVE(nh, n, entry);
		n->net.old = 0;
		n->net.rd = rd;
		xn = kr_net_find(kt, n);
		if (xn) {
			xn->net.old = 0;
			filterset_free(&xn->net.attrset);
			filterset_move(&n->net.attrset, &xn->net.attrset);
			kr_net_delete(n);
		} else
			TAILQ_INSERT_TAIL(&kt->krn, n, entry);
	}
}

void
kr_net_clear(struct ktable *kt)
{
	struct network *n, *xn;

	TAILQ_FOREACH_SAFE(n, &kt->krn, entry, xn) {
		TAILQ_REMOVE(&kt->krn, n, entry);
		if (n->net.type == NETWORK_DEFAULT)
			kr_net_redist_del(kt, &n->net, 0);
		kr_net_delete(n);
	}
}

void
kr_redistribute(int type, struct ktable *kt, struct kroute *kr)
{
	struct network_config	 net;
	u_int32_t		 a;

	bzero(&net, sizeof(net));
	net.prefix.aid = AID_INET;
	net.prefix.v4.s_addr = kr->prefix.s_addr;
	net.prefixlen = kr->prefixlen;
	net.rtlabel = kr->labelid;
	net.priority = kr->priority;

	/* shortcut for removals */
	if (type == IMSG_NETWORK_REMOVE) {
		kr_net_redist_del(kt, &net, 1);
		return;
	}

	if (!(kr->flags & F_KERNEL))
		return;

	/* Dynamic routes are not redistributable. */
	if (kr->flags & F_DYNAMIC)
		return;

	/*
	 * We consider the loopback net, multicast and experimental addresses
	 * as not redistributable.
	 */
	a = ntohl(kr->prefix.s_addr);
	if (IN_MULTICAST(a) || IN_BADCLASS(a) ||
	    (a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		return;

	/* Consider networks with nexthop loopback as not redistributable. */
	if (kr->nexthop.s_addr == htonl(INADDR_LOOPBACK))
		return;

	/*
	 * never allow 0.0.0.0/0 the default route can only be redistributed
	 * with announce default.
	 */
	if (kr->prefix.s_addr == INADDR_ANY && kr->prefixlen == 0)
		return;

	if (kr_net_match(kt, &net, kr->flags) == 0)
		/* no longer matches, if still present remove it */
		kr_net_redist_del(kt, &net, 1);
}

void
kr_redistribute6(int type, struct ktable *kt, struct kroute6 *kr6)
{
	struct network_config	 net;

	bzero(&net, sizeof(net));
	net.prefix.aid = AID_INET6;
	memcpy(&net.prefix.v6, &kr6->prefix, sizeof(struct in6_addr));
	net.prefixlen = kr6->prefixlen;
	net.rtlabel = kr6->labelid;
	net.priority = kr6->priority;

	/* shortcut for removals */
	if (type == IMSG_NETWORK_REMOVE) {
		kr_net_redist_del(kt, &net, 1);
		return;
	}

	if (!(kr6->flags & F_KERNEL))
		return;

	/* Dynamic routes are not redistributable. */
	if (kr6->flags & F_DYNAMIC)
		return;

	/*
	 * We consider unspecified, loopback, multicast, link- and site-local,
	 * IPv4 mapped and IPv4 compatible addresses as not redistributable.
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&kr6->prefix) ||
	    IN6_IS_ADDR_LOOPBACK(&kr6->prefix) ||
	    IN6_IS_ADDR_MULTICAST(&kr6->prefix) ||
	    IN6_IS_ADDR_LINKLOCAL(&kr6->prefix) ||
	    IN6_IS_ADDR_SITELOCAL(&kr6->prefix) ||
	    IN6_IS_ADDR_V4MAPPED(&kr6->prefix) ||
	    IN6_IS_ADDR_V4COMPAT(&kr6->prefix))
		return;

	/*
	 * Consider networks with nexthop loopback as not redistributable.
	 */
	if (IN6_IS_ADDR_LOOPBACK(&kr6->nexthop))
		return;

	/*
	 * never allow ::/0 the default route can only be redistributed
	 * with announce default.
	 */
	if (kr6->prefixlen == 0 &&
	    memcmp(&kr6->prefix, &in6addr_any, sizeof(struct in6_addr)) == 0)
		return;

	if (kr_net_match(kt, &net, kr6->flags) == 0)
		/* no longer matches, if still present remove it */
		kr_net_redist_del(kt, &net, 1);
}

void
ktable_preload(void)
{
	struct ktable	*kt;
	struct network	*n;
	u_int		 i;

	for (i = 0; i < krt_size; i++) {
		if ((kt = ktable_get(i)) == NULL)
			continue;
		kt->state = RECONF_DELETE;

		/* mark all networks as old */
		TAILQ_FOREACH(n, &kt->krn, entry)
			n->net.old = 1;
	}
}

void
ktable_postload(u_int8_t fib_prio)
{
	struct ktable	*kt;
	struct network	*n, *xn;
	u_int		 i;

	for (i = krt_size; i > 0; i--) {
		if ((kt = ktable_get(i - 1)) == NULL)
			continue;
		if (kt->state == RECONF_DELETE) {
			ktable_free(i - 1, fib_prio);
			continue;
		} else if (kt->state == RECONF_REINIT)
			kt->fib_sync = kt->fib_conf;

		/* cleanup old networks */
		TAILQ_FOREACH_SAFE(n, &kt->krn, entry, xn) {
			if (n->net.old) {
				TAILQ_REMOVE(&kt->krn, n, entry);
				if (n->net.type == NETWORK_DEFAULT)
					kr_net_redist_del(kt, &n->net, 0);
				kr_net_delete(n);
			}
		}
	}
}

int
kr_reload(void)
{
	struct ktable		*kt;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;
	struct knexthop_node	*nh;
	struct network		*n;
	u_int			 rid;
	int			 hasdyn = 0;

	for (rid = 0; rid < krt_size; rid++) {
		if ((kt = ktable_get(rid)) == NULL)
			continue;

		RB_FOREACH(nh, knexthop_tree, KT2KNT(kt))
			knexthop_validate(kt, nh);

		TAILQ_FOREACH(n, &kt->krn, entry)
			if (n->net.type == NETWORK_DEFAULT) {
				kr_net_redist_add(kt, &n->net,
				    &n->net.attrset, 0);
			} else
				hasdyn = 1;

		if (hasdyn) {
			/* only evaluate the full tree if we need */
			RB_FOREACH(kr, kroute_tree, &kt->krt)
				kr_redistribute(IMSG_NETWORK_ADD, kt, &kr->r);
			RB_FOREACH(kr6, kroute6_tree, &kt->krt6)
				kr_redistribute6(IMSG_NETWORK_ADD, kt, &kr6->r);
		}
	}

	return (0);
}

struct kroute_full *
kr_tofull(struct kroute *kr)
{
	static struct kroute_full	kf;

	bzero(&kf, sizeof(kf));

	kf.prefix.aid = AID_INET;
	kf.prefix.v4.s_addr = kr->prefix.s_addr;
	kf.nexthop.aid = AID_INET;
	kf.nexthop.v4.s_addr = kr->nexthop.s_addr;
	strlcpy(kf.label, rtlabel_id2name(kr->labelid), sizeof(kf.label));
	kf.labelid = kr->labelid;
	kf.flags = kr->flags;
	kf.ifindex = kr->ifindex;
	kf.prefixlen = kr->prefixlen;
	kf.priority = kr->priority;

	return (&kf);
}

struct kroute_full *
kr6_tofull(struct kroute6 *kr6)
{
	static struct kroute_full	kf;

	bzero(&kf, sizeof(kf));

	kf.prefix.aid = AID_INET6;
	memcpy(&kf.prefix.v6, &kr6->prefix, sizeof(struct in6_addr));
	kf.nexthop.aid = AID_INET6;
	memcpy(&kf.nexthop.v6, &kr6->nexthop, sizeof(struct in6_addr));
	strlcpy(kf.label, rtlabel_id2name(kr6->labelid), sizeof(kf.label));
	kf.labelid = kr6->labelid;
	kf.flags = kr6->flags;
	kf.ifindex = kr6->ifindex;
	kf.prefixlen = kr6->prefixlen;
	kf.priority = kr6->priority;

	return (&kf);
}

/*
 * RB-tree compare functions
 */

int
kroute_compare(struct kroute_node *a, struct kroute_node *b)
{
	if (ntohl(a->r.prefix.s_addr) < ntohl(b->r.prefix.s_addr))
		return (-1);
	if (ntohl(a->r.prefix.s_addr) > ntohl(b->r.prefix.s_addr))
		return (1);
	if (a->r.prefixlen < b->r.prefixlen)
		return (-1);
	if (a->r.prefixlen > b->r.prefixlen)
		return (1);

	/* if the priority is RTP_ANY finish on the first address hit */
	if (a->r.priority == RTP_ANY || b->r.priority == RTP_ANY)
		return (0);
	if (a->r.priority < b->r.priority)
		return (-1);
	if (a->r.priority > b->r.priority)
		return (1);
	return (0);
}

int
kroute6_compare(struct kroute6_node *a, struct kroute6_node *b)
{
	int i;

	for (i = 0; i < 16; i++) {
		if (a->r.prefix.s6_addr[i] < b->r.prefix.s6_addr[i])
			return (-1);
		if (a->r.prefix.s6_addr[i] > b->r.prefix.s6_addr[i])
			return (1);
	}

	if (a->r.prefixlen < b->r.prefixlen)
		return (-1);
	if (a->r.prefixlen > b->r.prefixlen)
		return (1);

	/* if the priority is RTP_ANY finish on the first address hit */
	if (a->r.priority == RTP_ANY || b->r.priority == RTP_ANY)
		return (0);
	if (a->r.priority < b->r.priority)
		return (-1);
	if (a->r.priority > b->r.priority)
		return (1);
	return (0);
}

int
knexthop_compare(struct knexthop_node *a, struct knexthop_node *b)
{
	int	i;

	if (a->nexthop.aid != b->nexthop.aid)
		return (b->nexthop.aid - a->nexthop.aid);

	switch (a->nexthop.aid) {
	case AID_INET:
		if (ntohl(a->nexthop.v4.s_addr) < ntohl(b->nexthop.v4.s_addr))
			return (-1);
		if (ntohl(a->nexthop.v4.s_addr) > ntohl(b->nexthop.v4.s_addr))
			return (1);
		break;
	case AID_INET6:
		for (i = 0; i < 16; i++) {
			if (a->nexthop.v6.s6_addr[i] < b->nexthop.v6.s6_addr[i])
				return (-1);
			if (a->nexthop.v6.s6_addr[i] > b->nexthop.v6.s6_addr[i])
				return (1);
		}
		break;
	default:
		fatalx("%s: unknown AF", __func__);
	}

	return (0);
}

int
kredist_compare(struct kredist_node *a, struct kredist_node *b)
{
	int	i;

	if (a->prefix.aid != b->prefix.aid)
		return (b->prefix.aid - a->prefix.aid);

	if (a->prefixlen < b->prefixlen)
		return (-1);
	if (a->prefixlen > b->prefixlen)
		return (1);

	switch (a->prefix.aid) {
	case AID_INET:
		if (ntohl(a->prefix.v4.s_addr) < ntohl(b->prefix.v4.s_addr))
			return (-1);
		if (ntohl(a->prefix.v4.s_addr) > ntohl(b->prefix.v4.s_addr))
			return (1);
		break;
	case AID_INET6:
		for (i = 0; i < 16; i++) {
			if (a->prefix.v6.s6_addr[i] < b->prefix.v6.s6_addr[i])
				return (-1);
			if (a->prefix.v6.s6_addr[i] > b->prefix.v6.s6_addr[i])
				return (1);
		}
		break;
	default:
		fatalx("%s: unknown AF", __func__);
	}

	if (a->rd < b->rd)
		return (-1);
	if (a->rd > b->rd)
		return (1);

	return (0);
}

int
kif_compare(struct kif_node *a, struct kif_node *b)
{
	return (b->k.ifindex - a->k.ifindex);
}


/*
 * tree management functions
 */

struct kroute_node *
kroute_find(struct ktable *kt, in_addr_t prefix, u_int8_t prefixlen,
    u_int8_t prio)
{
	struct kroute_node	s;
	struct kroute_node	*kn, *tmp;

	s.r.prefix.s_addr = prefix;
	s.r.prefixlen = prefixlen;
	s.r.priority = prio;

	kn = RB_FIND(kroute_tree, &kt->krt, &s);
	if (kn && prio == RTP_ANY) {
		tmp = RB_PREV(kroute_tree, &kt->krt, kn);
		while (tmp) {
			if (kroute_compare(&s, tmp) == 0)
				kn = tmp;
			else
				break;
			tmp = RB_PREV(kroute_tree, &kt->krt, kn);
		}
	}
	return (kn);
}

struct kroute_node *
kroute_matchgw(struct kroute_node *kr, struct sockaddr_in *sa_in)
{
	in_addr_t	nexthop;

	if (sa_in == NULL) {
		log_warnx("%s: no nexthop defined", __func__);
		return (NULL);
	}
	nexthop = sa_in->sin_addr.s_addr;

	while (kr) {
		if (kr->r.nexthop.s_addr == nexthop)
			return (kr);
		kr = kr->next;
	}

	return (NULL);
}

int
kroute_insert(struct ktable *kt, struct kroute_node *kr)
{
	struct kroute_node	*krm;
	struct knexthop_node	*h;
	in_addr_t		 mask, ina;

	if ((krm = RB_INSERT(kroute_tree, &kt->krt, kr)) != NULL) {
		/* multipath route, add at end of list */
		while (krm->next != NULL)
			krm = krm->next;
		krm->next = kr;
		kr->next = NULL; /* to be sure */
	}

	/* XXX this is wrong for nexthop validated via BGP */
	if (kr->r.flags & F_KERNEL) {
		mask = prefixlen2mask(kr->r.prefixlen);
		ina = ntohl(kr->r.prefix.s_addr);
		RB_FOREACH(h, knexthop_tree, KT2KNT(kt))
			if (h->nexthop.aid == AID_INET &&
			    (ntohl(h->nexthop.v4.s_addr) & mask) == ina)
				knexthop_validate(kt, h);

		if (kr->r.flags & F_CONNECTED)
			if (kif_kr_insert(kr) == -1)
				return (-1);

		if (krm == NULL)
			/* redistribute multipath routes only once */
			kr_redistribute(IMSG_NETWORK_ADD, kt, &kr->r);
	}
	return (0);
}


int
kroute_remove(struct ktable *kt, struct kroute_node *kr)
{
	struct kroute_node	*krm;
	struct knexthop_node	*s;

	if ((krm = RB_FIND(kroute_tree, &kt->krt, kr)) == NULL) {
		log_warnx("%s: failed to find %s/%u", __func__,
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	if (krm == kr) {
		/* head element */
		if (RB_REMOVE(kroute_tree, &kt->krt, kr) == NULL) {
			log_warnx("%s: failed for %s/%u", __func__,
			    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
			return (-1);
		}
		if (kr->next != NULL) {
			if (RB_INSERT(kroute_tree, &kt->krt, kr->next) !=
			    NULL) {
				log_warnx("%s: failed to add %s/%u", __func__,
				    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
				return (-1);
			}
		}
	} else {
		/* somewhere in the list */
		while (krm->next != kr && krm->next != NULL)
			krm = krm->next;
		if (krm->next == NULL) {
			log_warnx("%s: multipath list corrupted "
			    "for %s/%u", inet_ntoa(kr->r.prefix), __func__,
			    kr->r.prefixlen);
			return (-1);
		}
		krm->next = kr->next;
	}

	/* check whether a nexthop depends on this kroute */
	if (kr->r.flags & F_NEXTHOP)
		RB_FOREACH(s, knexthop_tree, KT2KNT(kt))
			if (s->kroute == kr)
				knexthop_validate(kt, s);

	if (kr->r.flags & F_KERNEL && kr == krm && kr->next == NULL)
		/* again remove only once */
		kr_redistribute(IMSG_NETWORK_REMOVE, kt, &kr->r);

	if (kr->r.flags & F_CONNECTED)
		if (kif_kr_remove(kr) == -1) {
			free(kr);
			return (-1);
		}

	free(kr);
	return (0);
}

void
kroute_clear(struct ktable *kt)
{
	struct kroute_node	*kr;

	while ((kr = RB_MIN(kroute_tree, &kt->krt)) != NULL)
		kroute_remove(kt, kr);
}

struct kroute6_node *
kroute6_find(struct ktable *kt, const struct in6_addr *prefix,
    u_int8_t prefixlen, u_int8_t prio)
{
	struct kroute6_node	s;
	struct kroute6_node	*kn6, *tmp;

	memcpy(&s.r.prefix, prefix, sizeof(struct in6_addr));
	s.r.prefixlen = prefixlen;
	s.r.priority = prio;

	kn6 = RB_FIND(kroute6_tree, &kt->krt6, &s);
	if (kn6 && prio == RTP_ANY) {
		tmp = RB_PREV(kroute6_tree, &kt->krt6, kn6);
		while (tmp) {
			if (kroute6_compare(&s, tmp) == 0)
				kn6 = tmp;
			else
				break;
			tmp = RB_PREV(kroute6_tree, &kt->krt6, kn6);
		}
	}
	return (kn6);
}

struct kroute6_node *
kroute6_matchgw(struct kroute6_node *kr, struct sockaddr_in6 *sa_in6)
{
	struct in6_addr	nexthop;

	if (sa_in6 == NULL) {
		log_warnx("%s: no nexthop defined", __func__);
		return (NULL);
	}
	memcpy(&nexthop, &sa_in6->sin6_addr, sizeof(nexthop));

	while (kr) {
		if (memcmp(&kr->r.nexthop, &nexthop, sizeof(nexthop)) == 0)
			return (kr);
		kr = kr->next;
	}

	return (NULL);
}

int
kroute6_insert(struct ktable *kt, struct kroute6_node *kr)
{
	struct kroute6_node	*krm;
	struct knexthop_node	*h;
	struct in6_addr		 ina, inb;

	if ((krm = RB_INSERT(kroute6_tree, &kt->krt6, kr)) != NULL) {
		/* multipath route, add at end of list */
		while (krm->next != NULL)
			krm = krm->next;
		krm->next = kr;
		kr->next = NULL; /* to be sure */
	}

	/* XXX this is wrong for nexthop validated via BGP */
	if (kr->r.flags & F_KERNEL) {
		inet6applymask(&ina, &kr->r.prefix, kr->r.prefixlen);
		RB_FOREACH(h, knexthop_tree, KT2KNT(kt))
			if (h->nexthop.aid == AID_INET6) {
				inet6applymask(&inb, &h->nexthop.v6,
				    kr->r.prefixlen);
				if (memcmp(&ina, &inb, sizeof(ina)) == 0)
					knexthop_validate(kt, h);
			}

		if (kr->r.flags & F_CONNECTED)
			if (kif_kr6_insert(kr) == -1)
				return (-1);

		if (krm == NULL)
			/* redistribute multipath routes only once */
			kr_redistribute6(IMSG_NETWORK_ADD, kt, &kr->r);
	}

	return (0);
}

int
kroute6_remove(struct ktable *kt, struct kroute6_node *kr)
{
	struct kroute6_node	*krm;
	struct knexthop_node	*s;

	if ((krm = RB_FIND(kroute6_tree, &kt->krt6, kr)) == NULL) {
		log_warnx("%s: failed for %s/%u", __func__,
		    log_in6addr(&kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	if (krm == kr) {
		/* head element */
		if (RB_REMOVE(kroute6_tree, &kt->krt6, kr) == NULL) {
			log_warnx("%s: failed for %s/%u", __func__,
			    log_in6addr(&kr->r.prefix), kr->r.prefixlen);
			return (-1);
		}
		if (kr->next != NULL) {
			if (RB_INSERT(kroute6_tree, &kt->krt6, kr->next) !=
			    NULL) {
				log_warnx("%s: failed to add %s/%u", __func__,
				    log_in6addr(&kr->r.prefix),
				    kr->r.prefixlen);
				return (-1);
			}
		}
	} else {
		/* somewhere in the list */
		while (krm->next != kr && krm->next != NULL)
			krm = krm->next;
		if (krm->next == NULL) {
			log_warnx("%s: multipath list corrupted "
			    "for %s/%u", __func__, log_in6addr(&kr->r.prefix),
			    kr->r.prefixlen);
			return (-1);
		}
		krm->next = kr->next;
	}

	/* check whether a nexthop depends on this kroute */
	if (kr->r.flags & F_NEXTHOP)
		RB_FOREACH(s, knexthop_tree, KT2KNT(kt))
			if (s->kroute == kr)
				knexthop_validate(kt, s);

	if (kr->r.flags & F_KERNEL && kr == krm && kr->next == NULL)
		/* again remove only once */
		kr_redistribute6(IMSG_NETWORK_REMOVE, kt, &kr->r);

	if (kr->r.flags & F_CONNECTED)
		if (kif_kr6_remove(kr) == -1) {
			free(kr);
			return (-1);
		}

	free(kr);
	return (0);
}

void
kroute6_clear(struct ktable *kt)
{
	struct kroute6_node	*kr;

	while ((kr = RB_MIN(kroute6_tree, &kt->krt6)) != NULL)
		kroute6_remove(kt, kr);
}

struct knexthop_node *
knexthop_find(struct ktable *kt, struct bgpd_addr *addr)
{
	struct knexthop_node	s;

	bzero(&s, sizeof(s));
	memcpy(&s.nexthop, addr, sizeof(s.nexthop));

	return (RB_FIND(knexthop_tree, KT2KNT(kt), &s));
}

int
knexthop_insert(struct ktable *kt, struct knexthop_node *kn)
{
	if (RB_INSERT(knexthop_tree, KT2KNT(kt), kn) != NULL) {
		log_warnx("%s: failed for %s", __func__,
		    log_addr(&kn->nexthop));
		free(kn);
		return (-1);
	}

	knexthop_validate(kt, kn);

	return (0);
}

int
knexthop_remove(struct ktable *kt, struct knexthop_node *kn)
{
	kroute_detach_nexthop(kt, kn);

	if (RB_REMOVE(knexthop_tree, KT2KNT(kt), kn) == NULL) {
		log_warnx("%s: failed for %s", __func__,
		    log_addr(&kn->nexthop));
		return (-1);
	}

	free(kn);
	return (0);
}

void
knexthop_clear(struct ktable *kt)
{
	struct knexthop_node	*kn;

	while ((kn = RB_MIN(knexthop_tree, KT2KNT(kt))) != NULL)
		knexthop_remove(kt, kn);
}

struct kif_node *
kif_find(int ifindex)
{
	struct kif_node	s;

	bzero(&s, sizeof(s));
	s.k.ifindex = ifindex;

	return (RB_FIND(kif_tree, &kit, &s));
}

int
kif_insert(struct kif_node *kif)
{
	LIST_INIT(&kif->kroute_l);
	LIST_INIT(&kif->kroute6_l);

	if (RB_INSERT(kif_tree, &kit, kif) != NULL) {
		log_warnx("RB_INSERT(kif_tree, &kit, kif)");
		free(kif);
		return (-1);
	}

	return (0);
}

int
kif_remove(struct kif_node *kif, u_int rdomain)
{
	struct ktable	*kt;
	struct kif_kr	*kkr;
	struct kif_kr6	*kkr6;

	if (RB_REMOVE(kif_tree, &kit, kif) == NULL) {
		log_warnx("RB_REMOVE(kif_tree, &kit, kif)");
		return (-1);
	}

	if ((kt = ktable_get(rdomain)) == NULL)
		goto done;

	while ((kkr = LIST_FIRST(&kif->kroute_l)) != NULL) {
		LIST_REMOVE(kkr, entry);
		kkr->kr->r.flags &= ~F_NEXTHOP;
		kroute_remove(kt, kkr->kr);
		free(kkr);
	}

	while ((kkr6 = LIST_FIRST(&kif->kroute6_l)) != NULL) {
		LIST_REMOVE(kkr6, entry);
		kkr6->kr->r.flags &= ~F_NEXTHOP;
		kroute6_remove(kt, kkr6->kr);
		free(kkr6);
	}
done:
	free(kif);
	return (0);
}

void
kif_clear(u_int rdomain)
{
	struct kif_node	*kif;

	while ((kif = RB_MIN(kif_tree, &kit)) != NULL)
		kif_remove(kif, rdomain);
}

int
kif_kr_insert(struct kroute_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr	*kkr;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		if (kr->r.ifindex)
			log_warnx("%s: interface with index %u not found",
			    __func__, kr->r.ifindex);
		return (0);
	}

	if (kif->k.nh_reachable)
		kr->r.flags &= ~F_DOWN;
	else
		kr->r.flags |= F_DOWN;

	if ((kkr = calloc(1, sizeof(struct kif_kr))) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}

	kkr->kr = kr;

	LIST_INSERT_HEAD(&kif->kroute_l, kkr, entry);

	return (0);
}

int
kif_kr_remove(struct kroute_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr	*kkr;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		if (kr->r.ifindex)
			log_warnx("%s: interface with index %u not found",
			    __func__, kr->r.ifindex);
		return (0);
	}

	for (kkr = LIST_FIRST(&kif->kroute_l); kkr != NULL && kkr->kr != kr;
	    kkr = LIST_NEXT(kkr, entry))
		;	/* nothing */

	if (kkr == NULL) {
		log_warnx("%s: can't remove connected route from interface "
		    "with index %u: not found", __func__, kr->r.ifindex);
		return (-1);
	}

	LIST_REMOVE(kkr, entry);
	free(kkr);

	return (0);
}

int
kif_kr6_insert(struct kroute6_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr6	*kkr6;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		if (kr->r.ifindex)
			log_warnx("%s: interface with index %u not found",
			    __func__, kr->r.ifindex);
		return (0);
	}

	if (kif->k.nh_reachable)
		kr->r.flags &= ~F_DOWN;
	else
		kr->r.flags |= F_DOWN;

	if ((kkr6 = calloc(1, sizeof(struct kif_kr6))) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}

	kkr6->kr = kr;

	LIST_INSERT_HEAD(&kif->kroute6_l, kkr6, entry);

	return (0);
}

int
kif_kr6_remove(struct kroute6_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr6	*kkr6;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		if (kr->r.ifindex)
			log_warnx("%s: interface with index %u not found",
			    __func__, kr->r.ifindex);
		return (0);
	}

	for (kkr6 = LIST_FIRST(&kif->kroute6_l); kkr6 != NULL && kkr6->kr != kr;
	    kkr6 = LIST_NEXT(kkr6, entry))
		;	/* nothing */

	if (kkr6 == NULL) {
		log_warnx("%s: can't remove connected route from interface "
		    "with index %u: not found", __func__, kr->r.ifindex);
		return (-1);
	}

	LIST_REMOVE(kkr6, entry);
	free(kkr6);

	return (0);
}

/*
 * nexthop validation
 */

static int
kif_validate(struct kif *kif)
{
	if (!(kif->flags & IFF_UP))
		return (0);

	/*
	 * we treat link_state == LINK_STATE_UNKNOWN as valid,
	 * not all interfaces have a concept of "link state" and/or
	 * do not report up
	 */

	if (kif->link_state == LINK_STATE_DOWN)
		return (0);

	return (1);
}

/*
 * return 1 when the interface is up and the link state is up or unknwown
 * except when this is a carp interface, then return 1 only when link state
 * is up
 */
static int
kif_depend_state(struct kif *kif)
{
	if (!(kif->flags & IFF_UP))
		return (0);


	if (kif->if_type == IFT_CARP &&
	    kif->link_state == LINK_STATE_UNKNOWN)
		return (0);

	return LINK_STATE_IS_UP(kif->link_state);
}


int
kroute_validate(struct kroute *kr)
{
	struct kif_node		*kif;

	if (kr->flags & (F_REJECT | F_BLACKHOLE))
		return (0);

	if ((kif = kif_find(kr->ifindex)) == NULL) {
		if (kr->ifindex)
			log_warnx("%s: interface with index %d not found, "
			    "referenced from route for %s/%u", __func__,
			    kr->ifindex, inet_ntoa(kr->prefix),
			    kr->prefixlen);
		return (1);
	}

	return (kif->k.nh_reachable);
}

int
kroute6_validate(struct kroute6 *kr)
{
	struct kif_node		*kif;

	if (kr->flags & (F_REJECT | F_BLACKHOLE))
		return (0);

	if ((kif = kif_find(kr->ifindex)) == NULL) {
		if (kr->ifindex)
			log_warnx("%s: interface with index %d not found, "
			    "referenced from route for %s/%u", __func__,
			    kr->ifindex, log_in6addr(&kr->prefix),
			    kr->prefixlen);
		return (1);
	}

	return (kif->k.nh_reachable);
}

void
knexthop_validate(struct ktable *kt, struct knexthop_node *kn)
{
	void			*oldk;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	oldk = kn->kroute;
	kroute_detach_nexthop(kt, kn);

	switch (kn->nexthop.aid) {
	case AID_INET:
		kr = kroute_match(kt, kn->nexthop.v4.s_addr, 0);

		if (kr) {
			kn->kroute = kr;
			kr->r.flags |= F_NEXTHOP;
		}

		/*
		 * Send update if nexthop route changed under us if
		 * the route remains the same then the NH state has not
		 * changed. State changes are tracked by knexthop_track().
		 */
		if (kr != oldk)
			knexthop_send_update(kn);
		break;
	case AID_INET6:
		kr6 = kroute6_match(kt, &kn->nexthop.v6, 0);

		if (kr6) {
			kn->kroute = kr6;
			kr6->r.flags |= F_NEXTHOP;
		}

		if (kr6 != oldk)
			knexthop_send_update(kn);
		break;
	}
}

void
knexthop_track(struct ktable *kt, void *krp)
{
	struct knexthop_node	*kn;

	RB_FOREACH(kn, knexthop_tree, KT2KNT(kt))
		if (kn->kroute == krp)
			knexthop_send_update(kn);
}

void
knexthop_send_update(struct knexthop_node *kn)
{
	struct kroute_nexthop	 n;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	bzero(&n, sizeof(n));
	memcpy(&n.nexthop, &kn->nexthop, sizeof(n.nexthop));

	if (kn->kroute == NULL) {
		n.valid = 0;	/* NH is not valid */
		send_nexthop_update(&n);
		return;
	}

	switch (kn->nexthop.aid) {
	case AID_INET:
		kr = kn->kroute;
		n.valid = kroute_validate(&kr->r);
		n.connected = kr->r.flags & F_CONNECTED;
		if (kr->r.nexthop.s_addr != 0) {
			n.gateway.aid = AID_INET;
			n.gateway.v4.s_addr = kr->r.nexthop.s_addr;
		}
		if (n.connected) {
			n.net.aid = AID_INET;
			n.net.v4.s_addr = kr->r.prefix.s_addr;
			n.netlen = kr->r.prefixlen;
		}
		break;
	case AID_INET6:
		kr6 = kn->kroute;
		n.valid = kroute6_validate(&kr6->r);
		n.connected = kr6->r.flags & F_CONNECTED;
		if (memcmp(&kr6->r.nexthop, &in6addr_any,
		    sizeof(struct in6_addr)) != 0) {
			n.gateway.aid = AID_INET6;
			memcpy(&n.gateway.v6, &kr6->r.nexthop,
			    sizeof(struct in6_addr));
		}
		if (n.connected) {
			n.net.aid = AID_INET6;
			memcpy(&n.net.v6, &kr6->r.prefix,
			    sizeof(struct in6_addr));
			n.netlen = kr6->r.prefixlen;
		}
		break;
	}
	send_nexthop_update(&n);
}

struct kroute_node *
kroute_match(struct ktable *kt, in_addr_t key, int matchall)
{
	int			 i;
	struct kroute_node	*kr;
	in_addr_t		 ina;

	ina = ntohl(key);

	/* we will never match the default route */
	for (i = 32; i > 0; i--)
		if ((kr = kroute_find(kt, htonl(ina & prefixlen2mask(i)), i,
		    RTP_ANY)) != NULL)
			if (matchall || bgpd_filternexthop(&kr->r, NULL) == 0)
			    return (kr);

	/* if we don't have a match yet, try to find a default route */
	if ((kr = kroute_find(kt, 0, 0, RTP_ANY)) != NULL)
		if (matchall || bgpd_filternexthop(&kr->r, NULL) == 0)
			return (kr);

	return (NULL);
}

struct kroute6_node *
kroute6_match(struct ktable *kt, struct in6_addr *key, int matchall)
{
	int			 i;
	struct kroute6_node	*kr6;
	struct in6_addr		 ina;

	/* we will never match the default route */
	for (i = 128; i > 0; i--) {
		inet6applymask(&ina, key, i);
		if ((kr6 = kroute6_find(kt, &ina, i, RTP_ANY)) != NULL)
			if (matchall || bgpd_filternexthop(NULL, &kr6->r) == 0)
				return (kr6);
	}

	/* if we don't have a match yet, try to find a default route */
	if ((kr6 = kroute6_find(kt, &in6addr_any, 0, RTP_ANY)) != NULL)
		if (matchall || bgpd_filternexthop(NULL, &kr6->r) == 0)
			return (kr6);

	return (NULL);
}

void
kroute_detach_nexthop(struct ktable *kt, struct knexthop_node *kn)
{
	struct knexthop_node	*s;
	struct kroute_node	*k;
	struct kroute6_node	*k6;

	if (kn->kroute == NULL)
		return;

	/*
	 * check whether there's another nexthop depending on this kroute
	 * if not remove the flag
	 */
	RB_FOREACH(s, knexthop_tree, KT2KNT(kt))
		if (s->kroute == kn->kroute && s != kn)
			break;

	if (s == NULL) {
		switch (kn->nexthop.aid) {
		case AID_INET:
			k = kn->kroute;
			k->r.flags &= ~F_NEXTHOP;
			break;
		case AID_INET6:
			k6 = kn->kroute;
			k6->r.flags &= ~F_NEXTHOP;
			break;
		}
	}

	kn->kroute = NULL;
}

/*
 * misc helpers
 */

int
protect_lo(struct ktable *kt)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	/* special protection for 127/8 */
	if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}
	kr->r.prefix.s_addr = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);
	kr->r.prefixlen = 8;
	kr->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute_tree, &kt->krt, kr) != NULL)
		free(kr);	/* kernel route already there, no problem */

	/* special protection for loopback */
	if ((kr6 = calloc(1, sizeof(struct kroute6_node))) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}
	memcpy(&kr6->r.prefix, &in6addr_loopback, sizeof(kr6->r.prefix));
	kr6->r.prefixlen = 128;
	kr6->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute6_tree, &kt->krt6, kr6) != NULL)
		free(kr6);	/* kernel route already there, no problem */

	return (0);
}

u_int8_t
prefixlen_classful(in_addr_t ina)
{
	/* it hurt to write this. */

	if (ina >= 0xf0000000U)		/* class E */
		return (32);
	else if (ina >= 0xe0000000U)	/* class D */
		return (4);
	else if (ina >= 0xc0000000U)	/* class C */
		return (24);
	else if (ina >= 0x80000000U)	/* class B */
		return (16);
	else				/* class A */
		return (8);
}

u_int8_t
mask2prefixlen(in_addr_t ina)
{
	if (ina == 0)
		return (0);
	else
		return (33 - ffs(ntohl(ina)));
}

u_int8_t
mask2prefixlen6(struct sockaddr_in6 *sa_in6)
{
	u_int8_t	*ap, *ep;
	u_int		 l = 0;

	/*
	 * sin6_len is the size of the sockaddr so substract the offset of
	 * the possibly truncated sin6_addr struct.
	 */
	ap = (u_int8_t *)&sa_in6->sin6_addr;
	ep = (u_int8_t *)sa_in6 + sa_in6->sin6_len;
	for (; ap < ep; ap++) {
		/* this "beauty" is adopted from sbin/route/show.c ... */
		switch (*ap) {
		case 0xff:
			l += 8;
			break;
		case 0xfe:
			l += 7;
			goto done;
		case 0xfc:
			l += 6;
			goto done;
		case 0xf8:
			l += 5;
			goto done;
		case 0xf0:
			l += 4;
			goto done;
		case 0xe0:
			l += 3;
			goto done;
		case 0xc0:
			l += 2;
			goto done;
		case 0x80:
			l += 1;
			goto done;
		case 0x00:
			goto done;
		default:
			fatalx("non contiguous inet6 netmask");
		}
	}

 done:
	if (l > sizeof(struct in6_addr) * 8)
		fatalx("%s: prefixlen %d out of bound", __func__, l);
	return (l);
}

struct in6_addr *
prefixlen2mask6(u_int8_t prefixlen)
{
	static struct in6_addr	mask;
	int			i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	return (&mask);
}

const struct if_status_description
		if_status_descriptions[] = LINK_STATE_DESCRIPTIONS;
const struct ifmedia_description
		ifm_type_descriptions[] = IFM_TYPE_DESCRIPTIONS;

uint64_t
ift2ifm(uint8_t if_type)
{
	switch (if_type) {
	case IFT_ETHER:
		return (IFM_ETHER);
	case IFT_FDDI:
		return (IFM_FDDI);
	case IFT_CARP:
		return (IFM_CARP);
	case IFT_IEEE80211:
		return (IFM_IEEE80211);
	default:
		return (0);
	}
}

const char *
get_media_descr(uint64_t media_type)
{
	const struct ifmedia_description	*p;

	for (p = ifm_type_descriptions; p->ifmt_string != NULL; p++)
		if (media_type == p->ifmt_word)
			return (p->ifmt_string);

	return ("unknown media");
}

const char *
get_linkstate(uint8_t if_type, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, if_type, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return (buf);
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
if_change(u_short ifindex, int flags, struct if_data *ifd,
    u_int rdomain)
{
	struct ktable		*kt;
	struct kif_node		*kif;
	struct kif_kr		*kkr;
	struct kif_kr6		*kkr6;
	u_int8_t		 reachable;

	if ((kif = kif_find(ifindex)) == NULL) {
		log_warnx("%s: interface with index %u not found",
		    __func__, ifindex);
		return;
	}

	log_info("%s: %s: rdomain %u %s, %s, %s, %s",
	    __func__, kif->k.ifname, ifd->ifi_rdomain,
	    flags & IFF_UP ? "UP" : "DOWN",
	    get_media_descr(ift2ifm(ifd->ifi_type)),
	    get_linkstate(ifd->ifi_type, ifd->ifi_link_state),
	    get_baudrate(ifd->ifi_baudrate, "bps"));

	kif->k.flags = flags;
	kif->k.link_state = ifd->ifi_link_state;
	kif->k.if_type = ifd->ifi_type;
	kif->k.rdomain = ifd->ifi_rdomain;
	kif->k.baudrate = ifd->ifi_baudrate;
	kif->k.depend_state = kif_depend_state(&kif->k);

	send_imsg_session(IMSG_IFINFO, 0, &kif->k, sizeof(kif->k));

	if ((reachable = kif_validate(&kif->k)) == kif->k.nh_reachable)
		return;		/* nothing changed wrt nexthop validity */

	kif->k.nh_reachable = reachable;

	kt = ktable_get(rdomain);

	LIST_FOREACH(kkr, &kif->kroute_l, entry) {
		if (reachable)
			kkr->kr->r.flags &= ~F_DOWN;
		else
			kkr->kr->r.flags |= F_DOWN;

		if (kt == NULL)
			continue;

		knexthop_track(kt, kkr->kr);
	}
	LIST_FOREACH(kkr6, &kif->kroute6_l, entry) {
		if (reachable)
			kkr6->kr->r.flags &= ~F_DOWN;
		else
			kkr6->kr->r.flags |= F_DOWN;

		if (kt == NULL)
			continue;

		knexthop_track(kt, kkr6->kr);
	}
}

void
if_announce(void *msg, u_int rdomain)
{
	struct if_announcemsghdr	*ifan;
	struct kif_node			*kif;

	ifan = msg;

	switch (ifan->ifan_what) {
	case IFAN_ARRIVAL:
		if ((kif = calloc(1, sizeof(struct kif_node))) == NULL) {
			log_warn("%s", __func__);
			return;
		}

		kif->k.ifindex = ifan->ifan_index;
		strlcpy(kif->k.ifname, ifan->ifan_name, sizeof(kif->k.ifname));
		kif_insert(kif);
		break;
	case IFAN_DEPARTURE:
		kif = kif_find(ifan->ifan_index);
		kif_remove(kif, rdomain);
		break;
	}
}

int
get_mpe_config(const char *name, u_int *rdomain, u_int *label)
{
	struct  ifreq	ifr;
	struct shim_hdr	shim;
	int		s;

	*label = 0;
	*rdomain = 0;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return (-1);

	bzero(&shim, sizeof(shim));
	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&shim;

	if (ioctl(s, SIOCGETLABEL, (caddr_t)&ifr) == -1) {
		close(s);
		return (-1);
	}

	ifr.ifr_data = NULL;
	if (ioctl(s, SIOCGIFRDOMAIN, (caddr_t)&ifr) == -1) {
		close(s);
		return (-1);
	}

	close(s);

	*rdomain = ifr.ifr_rdomainid;
	*label = shim.shim_label;

	return (0);
}

/*
 * rtsock related functions
 */

int
send_rtmsg(int fd, int action, struct ktable *kt, struct kroute *kroute,
    u_int8_t fib_prio)
{
	struct iovec		iov[7];
	struct rt_msghdr	hdr;
	struct sockaddr_in	prefix;
	struct sockaddr_in	nexthop;
	struct sockaddr_in	mask;
	struct {
		struct sockaddr_dl	dl;
		char			pad[sizeof(long)];
	}			ifp;
	struct sockaddr_mpls	mpls;
	struct sockaddr_rtlabel	label;
	int			iovcnt = 0;

	if (!kt->fib_sync)
		return (0);

	/* initialize header */
	bzero(&hdr, sizeof(hdr));
	hdr.rtm_version = RTM_VERSION;
	hdr.rtm_type = action;
	hdr.rtm_tableid = kt->rtableid;
	hdr.rtm_priority = fib_prio;
	if (kroute->flags & F_BLACKHOLE)
		hdr.rtm_flags |= RTF_BLACKHOLE;
	if (kroute->flags & F_REJECT)
		hdr.rtm_flags |= RTF_REJECT;
	if (action == RTM_CHANGE)	/* reset these flags on change */
		hdr.rtm_fmask = RTF_REJECT|RTF_BLACKHOLE;
	hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	hdr.rtm_msglen = sizeof(hdr);
	/* adjust iovec */
	iov[iovcnt].iov_base = &hdr;
	iov[iovcnt++].iov_len = sizeof(hdr);

	bzero(&prefix, sizeof(prefix));
	prefix.sin_len = sizeof(prefix);
	prefix.sin_family = AF_INET;
	prefix.sin_addr.s_addr = kroute->prefix.s_addr;
	/* adjust header */
	hdr.rtm_addrs |= RTA_DST;
	hdr.rtm_msglen += sizeof(prefix);
	/* adjust iovec */
	iov[iovcnt].iov_base = &prefix;
	iov[iovcnt++].iov_len = sizeof(prefix);

	if (kroute->nexthop.s_addr != 0) {
		bzero(&nexthop, sizeof(nexthop));
		nexthop.sin_len = sizeof(nexthop);
		nexthop.sin_family = AF_INET;
		nexthop.sin_addr.s_addr = kroute->nexthop.s_addr;
		/* adjust header */
		hdr.rtm_flags |= RTF_GATEWAY;
		hdr.rtm_addrs |= RTA_GATEWAY;
		hdr.rtm_msglen += sizeof(nexthop);
		/* adjust iovec */
		iov[iovcnt].iov_base = &nexthop;
		iov[iovcnt++].iov_len = sizeof(nexthop);
	}

	bzero(&mask, sizeof(mask));
	mask.sin_len = sizeof(mask);
	mask.sin_family = AF_INET;
	mask.sin_addr.s_addr = htonl(prefixlen2mask(kroute->prefixlen));
	/* adjust header */
	hdr.rtm_addrs |= RTA_NETMASK;
	hdr.rtm_msglen += sizeof(mask);
	/* adjust iovec */
	iov[iovcnt].iov_base = &mask;
	iov[iovcnt++].iov_len = sizeof(mask);

	if (kroute->flags & F_MPLS) {
		/* need to force interface for mpe(4) routes */
		bzero(&ifp, sizeof(ifp));
		ifp.dl.sdl_len = sizeof(struct sockaddr_dl);
		ifp.dl.sdl_family = AF_LINK;
		ifp.dl.sdl_index = kroute->ifindex;
		/* adjust header */
		hdr.rtm_addrs |= RTA_IFP;
		hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_dl));
		/* adjust iovec */
		iov[iovcnt].iov_base = &ifp;
		iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_dl));

		bzero(&mpls, sizeof(mpls));
		mpls.smpls_len = sizeof(mpls);
		mpls.smpls_family = AF_MPLS;
		mpls.smpls_label = kroute->mplslabel;
		/* adjust header */
		hdr.rtm_flags |= RTF_MPLS;
		hdr.rtm_mpls = MPLS_OP_PUSH;
		hdr.rtm_addrs |= RTA_SRC;
		hdr.rtm_msglen += sizeof(mpls);
		/* clear gateway flag since this is for mpe(4) */
		hdr.rtm_flags &= ~RTF_GATEWAY;
		/* adjust iovec */
		iov[iovcnt].iov_base = &mpls;
		iov[iovcnt++].iov_len = sizeof(mpls);
	}

	if (kroute->labelid) {
		bzero(&label, sizeof(label));
		label.sr_len = sizeof(label);
		strlcpy(label.sr_label, rtlabel_id2name(kroute->labelid),
		    sizeof(label.sr_label));
		/* adjust header */
		hdr.rtm_addrs |= RTA_LABEL;
		hdr.rtm_msglen += sizeof(label);
		/* adjust iovec */
		iov[iovcnt].iov_base = &label;
		iov[iovcnt++].iov_len = sizeof(label);
	}

retry:
	if (writev(fd, iov, iovcnt) == -1) {
		if (errno == ESRCH) {
			if (hdr.rtm_type == RTM_CHANGE) {
				hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (hdr.rtm_type == RTM_DELETE) {
				log_info("route %s/%u vanished before delete",
				    inet_ntoa(kroute->prefix),
				    kroute->prefixlen);
				return (0);
			}
		}
		log_warn("%s: action %u, prefix %s/%u", __func__, hdr.rtm_type,
		    inet_ntoa(kroute->prefix), kroute->prefixlen);
		return (0);
	}

	return (0);
}

int
send_rt6msg(int fd, int action, struct ktable *kt, struct kroute6 *kroute,
    u_int8_t fib_prio)
{
	struct iovec		iov[7];
	struct rt_msghdr	hdr;
	struct pad {
		struct sockaddr_in6	addr;
		char			pad[sizeof(long)];
	} prefix, nexthop, mask;
	struct {
		struct sockaddr_dl	dl;
		char			pad[sizeof(long)];
	}			ifp;
	struct sockaddr_rtlabel	label;
	struct sockaddr_mpls	mpls;
	int			iovcnt = 0;

	if (!kt->fib_sync)
		return (0);

	/* initialize header */
	bzero(&hdr, sizeof(hdr));
	hdr.rtm_version = RTM_VERSION;
	hdr.rtm_type = action;
	hdr.rtm_tableid = kt->rtableid;
	hdr.rtm_priority = fib_prio;
	if (kroute->flags & F_BLACKHOLE)
		hdr.rtm_flags |= RTF_BLACKHOLE;
	if (kroute->flags & F_REJECT)
		hdr.rtm_flags |= RTF_REJECT;
	if (action == RTM_CHANGE)	/* reset these flags on change */
		hdr.rtm_fmask = RTF_REJECT|RTF_BLACKHOLE;
	hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	hdr.rtm_msglen = sizeof(hdr);
	/* adjust iovec */
	iov[iovcnt].iov_base = &hdr;
	iov[iovcnt++].iov_len = sizeof(hdr);

	bzero(&prefix, sizeof(prefix));
	prefix.addr.sin6_len = sizeof(struct sockaddr_in6);
	prefix.addr.sin6_family = AF_INET6;
	memcpy(&prefix.addr.sin6_addr, &kroute->prefix,
	    sizeof(struct in6_addr));
	/* XXX scope does not matter or? */
	/* adjust header */
	hdr.rtm_addrs |= RTA_DST;
	hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_in6));
	/* adjust iovec */
	iov[iovcnt].iov_base = &prefix;
	iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_in6));

	if (memcmp(&kroute->nexthop, &in6addr_any, sizeof(struct in6_addr))) {
		bzero(&nexthop, sizeof(nexthop));
		nexthop.addr.sin6_len = sizeof(struct sockaddr_in6);
		nexthop.addr.sin6_family = AF_INET6;
		memcpy(&nexthop.addr.sin6_addr, &kroute->nexthop,
		    sizeof(struct in6_addr));
		/* adjust header */
		hdr.rtm_flags |= RTF_GATEWAY;
		hdr.rtm_addrs |= RTA_GATEWAY;
		hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_in6));
		/* adjust iovec */
		iov[iovcnt].iov_base = &nexthop;
		iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_in6));
	}

	bzero(&mask, sizeof(mask));
	mask.addr.sin6_len = sizeof(struct sockaddr_in6);
	mask.addr.sin6_family = AF_INET6;
	memcpy(&mask.addr.sin6_addr, prefixlen2mask6(kroute->prefixlen),
	    sizeof(struct in6_addr));
	/* adjust header */
	hdr.rtm_addrs |= RTA_NETMASK;
	hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_in6));
	/* adjust iovec */
	iov[iovcnt].iov_base = &mask;
	iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_in6));

	if (kroute->flags & F_MPLS) {
		/* need to force interface for mpe(4) routes */
		bzero(&ifp, sizeof(ifp));
		ifp.dl.sdl_len = sizeof(struct sockaddr_dl);
		ifp.dl.sdl_family = AF_LINK;
		ifp.dl.sdl_index = kroute->ifindex;
		/* adjust header */
		hdr.rtm_addrs |= RTA_IFP;
		hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_dl));
		/* adjust iovec */
		iov[iovcnt].iov_base = &ifp;
		iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_dl));

		bzero(&mpls, sizeof(mpls));
		mpls.smpls_len = sizeof(mpls);
		mpls.smpls_family = AF_MPLS;
		mpls.smpls_label = kroute->mplslabel;
		/* adjust header */
		hdr.rtm_flags |= RTF_MPLS;
		hdr.rtm_mpls = MPLS_OP_PUSH;
		hdr.rtm_addrs |= RTA_SRC;
		hdr.rtm_msglen += ROUNDUP(sizeof(struct sockaddr_mpls));
		/* clear gateway flag since this is for mpe(4) */
		hdr.rtm_flags &= ~RTF_GATEWAY;
		/* adjust iovec */
		iov[iovcnt].iov_base = &mpls;
		iov[iovcnt++].iov_len = ROUNDUP(sizeof(struct sockaddr_mpls));
	}

	if (kroute->labelid) {
		bzero(&label, sizeof(label));
		label.sr_len = sizeof(label);
		strlcpy(label.sr_label, rtlabel_id2name(kroute->labelid),
		    sizeof(label.sr_label));
		/* adjust header */
		hdr.rtm_addrs |= RTA_LABEL;
		hdr.rtm_msglen += sizeof(label);
		/* adjust iovec */
		iov[iovcnt].iov_base = &label;
		iov[iovcnt++].iov_len = sizeof(label);
	}

retry:
	if (writev(fd, iov, iovcnt) == -1) {
		if (errno == ESRCH) {
			if (hdr.rtm_type == RTM_CHANGE) {
				hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (hdr.rtm_type == RTM_DELETE) {
				log_info("route %s/%u vanished before delete",
				    log_in6addr(&kroute->prefix),
				    kroute->prefixlen);
				return (0);
			}
		}
		log_warn("%s: action %u, prefix %s/%u", __func__, hdr.rtm_type,
		    log_in6addr(&kroute->prefix), kroute->prefixlen);
		return (0);
	}

	return (0);
}

int
fetchtable(struct ktable *kt, u_int8_t fib_prio)
{
	size_t			 len;
	int			 mib[7];
	char			*buf = NULL, *next, *lim;
	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *gw, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct sockaddr_rtlabel	*label;
	struct kroute_node	*kr = NULL;
	struct kroute6_node	*kr6 = NULL;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = kt->rtableid;

	if (sysctl(mib, 7, NULL, &len, NULL, 0) == -1) {
		if (kt->rtableid != 0 && errno == EINVAL)
			/* table nonexistent */
			return (0);
		log_warn("%s: sysctl", __func__);
		return (-1);
	}
	if (len > 0) {
		if ((buf = malloc(len)) == NULL) {
			log_warn("%s: fetchtable", __func__);
			return (-1);
		}
		if (sysctl(mib, 7, buf, &len, NULL, 0) == -1) {
			log_warn("%s: sysctl2", __func__);
			free(buf);
			return (-1);
		}
	}

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		if ((sa = rti_info[RTAX_DST]) == NULL)
			continue;

		/* Skip ARP/ND cache and broadcast routes. */
		if (rtm->rtm_flags & (RTF_LLINFO|RTF_BROADCAST))
			continue;

		switch (sa->sa_family) {
		case AF_INET:
			if ((kr = calloc(1, sizeof(struct kroute_node))) ==
			    NULL) {
				log_warn("%s", __func__);
				free(buf);
				return (-1);
			}

			kr->r.flags = F_KERNEL;
			kr->r.priority = rtm->rtm_priority;
			kr->r.ifindex = rtm->rtm_index;
			kr->r.prefix.s_addr =
			    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
			if (rtm->rtm_flags & RTF_STATIC)
				kr->r.flags |= F_STATIC;
			if (rtm->rtm_flags & RTF_BLACKHOLE)
				kr->r.flags |= F_BLACKHOLE;
			if (rtm->rtm_flags & RTF_REJECT)
				kr->r.flags |= F_REJECT;
			if (rtm->rtm_flags & RTF_DYNAMIC)
				kr->r.flags |= F_DYNAMIC;
			if (sa_in != NULL) {
				if (sa_in->sin_len == 0)
					break;
				kr->r.prefixlen =
				    mask2prefixlen(sa_in->sin_addr.s_addr);
			} else if (rtm->rtm_flags & RTF_HOST)
				kr->r.prefixlen = 32;
			else
				kr->r.prefixlen =
				    prefixlen_classful(kr->r.prefix.s_addr);
			rtlabel_unref(kr->r.labelid);
			kr->r.labelid = 0;
			if ((label = (struct sockaddr_rtlabel *)
			    rti_info[RTAX_LABEL]) != NULL) {
				kr->r.flags |= F_RTLABEL;
				kr->r.labelid =
				    rtlabel_name2id(label->sr_label);
			}
			break;
		case AF_INET6:
			if ((kr6 = calloc(1, sizeof(struct kroute6_node))) ==
			    NULL) {
				log_warn("%s", __func__);
				free(buf);
				return (-1);
			}

			kr6->r.flags = F_KERNEL;
			kr6->r.priority = rtm->rtm_priority;
			kr6->r.ifindex = rtm->rtm_index;
			memcpy(&kr6->r.prefix,
			    &((struct sockaddr_in6 *)sa)->sin6_addr,
			    sizeof(kr6->r.prefix));

			sa_in6 = (struct sockaddr_in6 *)rti_info[RTAX_NETMASK];
			if (rtm->rtm_flags & RTF_STATIC)
				kr6->r.flags |= F_STATIC;
			if (rtm->rtm_flags & RTF_BLACKHOLE)
				kr6->r.flags |= F_BLACKHOLE;
			if (rtm->rtm_flags & RTF_REJECT)
				kr6->r.flags |= F_REJECT;
			if (rtm->rtm_flags & RTF_DYNAMIC)
				kr6->r.flags |= F_DYNAMIC;
			if (sa_in6 != NULL) {
				if (sa_in6->sin6_len == 0)
					break;
				kr6->r.prefixlen = mask2prefixlen6(sa_in6);
			} else if (rtm->rtm_flags & RTF_HOST)
				kr6->r.prefixlen = 128;
			else
				fatalx("INET6 route without netmask");
			rtlabel_unref(kr6->r.labelid);
			kr6->r.labelid = 0;
			if ((label = (struct sockaddr_rtlabel *)
			    rti_info[RTAX_LABEL]) != NULL) {
				kr6->r.flags |= F_RTLABEL;
				kr6->r.labelid =
				    rtlabel_name2id(label->sr_label);
			}
			break;
		default:
			continue;
		}

		if ((gw = rti_info[RTAX_GATEWAY]) != NULL)
			switch (gw->sa_family) {
			case AF_INET:
				if (kr == NULL)
					fatalx("v4 gateway for !v4 dst?!");

				if (rtm->rtm_flags & RTF_CONNECTED) {
					kr->r.flags |= F_CONNECTED;
					break;
				}

				kr->r.nexthop.s_addr =
				    ((struct sockaddr_in *)gw)->sin_addr.s_addr;
				break;
			case AF_INET6:
				if (kr6 == NULL)
					fatalx("v6 gateway for !v6 dst?!");

				if (rtm->rtm_flags & RTF_CONNECTED) {
					kr6->r.flags |= F_CONNECTED;
					break;
				}

				memcpy(&kr6->r.nexthop,
				    &((struct sockaddr_in6 *)gw)->sin6_addr,
				    sizeof(kr6->r.nexthop));
				break;
			case AF_LINK:
				/*
				 * Traditional BSD connected routes have
				 * a gateway of type AF_LINK.
				 */
				if (sa->sa_family == AF_INET)
					kr->r.flags |= F_CONNECTED;
				else if (sa->sa_family == AF_INET6)
					kr6->r.flags |= F_CONNECTED;
				break;
			}

		if (sa->sa_family == AF_INET) {
			if (rtm->rtm_priority == fib_prio)  {
				send_rtmsg(kr_state.fd, RTM_DELETE, kt, &kr->r,
				    fib_prio);
				free(kr);
			} else
				kroute_insert(kt, kr);
		} else if (sa->sa_family == AF_INET6) {
			if (rtm->rtm_priority == fib_prio)  {
				send_rt6msg(kr_state.fd, RTM_DELETE, kt,
				    &kr6->r, fib_prio);
				free(kr6);
			} else
				kroute6_insert(kt, kr6);
		}
	}
	free(buf);
	return (0);
}

int
fetchifs(int ifindex)
{
	size_t			 len;
	int			 mib[6];
	char			*buf, *next, *lim;
	struct if_msghdr	 ifm;
	struct kif_node		*kif;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_dl	*sdl;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;	/* AF does not matter but AF_INET is shorter */
	mib[4] = NET_RT_IFLIST;
	mib[5] = ifindex;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_warn("%s: sysctl", __func__);
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_warn("%s: sysctl2", __func__);
		free(buf);
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += ifm.ifm_msglen) {
		memcpy(&ifm, next, sizeof(ifm));
		if (ifm.ifm_version != RTM_VERSION)
			continue;
		if (ifm.ifm_type != RTM_IFINFO)
			continue;

		sa = (struct sockaddr *)(next + sizeof(ifm));
		get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

		if ((kif = calloc(1, sizeof(struct kif_node))) == NULL) {
			log_warn("%s", __func__);
			free(buf);
			return (-1);
		}

		kif->k.ifindex = ifm.ifm_index;
		kif->k.flags = ifm.ifm_flags;
		kif->k.link_state = ifm.ifm_data.ifi_link_state;
		kif->k.if_type = ifm.ifm_data.ifi_type;
		kif->k.rdomain = ifm.ifm_data.ifi_rdomain;
		kif->k.baudrate = ifm.ifm_data.ifi_baudrate;
		kif->k.nh_reachable = kif_validate(&kif->k);
		kif->k.depend_state = kif_depend_state(&kif->k);

		if ((sa = rti_info[RTAX_IFP]) != NULL)
			if (sa->sa_family == AF_LINK) {
				sdl = (struct sockaddr_dl *)sa;
				if (sdl->sdl_nlen >= sizeof(kif->k.ifname))
					memcpy(kif->k.ifname, sdl->sdl_data,
					    sizeof(kif->k.ifname) - 1);
				else if (sdl->sdl_nlen > 0)
					memcpy(kif->k.ifname, sdl->sdl_data,
					    sdl->sdl_nlen);
				/* string already terminated via calloc() */
			}

		kif_insert(kif);
	}
	free(buf);
	return (0);
}

int
dispatch_rtmsg(u_int rdomain)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;
	char			*next, *lim;
	struct rt_msghdr	*rtm;
	struct if_msghdr	 ifm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct ktable		*kt;

	if ((n = read(kr_state.fd, &buf, sizeof(buf))) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		log_warn("%s: read error", __func__);
		return (-1);
	}

	if (n == 0) {
		log_warnx("routing socket closed");
		return (-1);
	}

	lim = buf + n;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (lim < next + sizeof(u_short) ||
		    lim < next + rtm->rtm_msglen)
			fatalx("%s: partial rtm in buffer", __func__);
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
		case RTM_DELETE:
			sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
			get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

			if (rtm->rtm_pid == kr_state.pid) /* cause by us */
				continue;

			if (rtm->rtm_errno)		 /* failed attempts */
				continue;

			if (rtm->rtm_flags & RTF_LLINFO) /* arp cache */
				continue;

			if ((kt = ktable_get(rtm->rtm_tableid)) == NULL)
				continue;

			if (dispatch_rtmsg_addr(rtm, rti_info, kt) == -1)
				return (-1);
			break;
		case RTM_IFINFO:
			memcpy(&ifm, next, sizeof(ifm));
			if_change(ifm.ifm_index, ifm.ifm_flags,
			    &ifm.ifm_data, rdomain);
			break;
		case RTM_IFANNOUNCE:
			if_announce(next, rdomain);
			break;
		default:
			/* ignore for now */
			break;
		}
	}
	return (0);
}

int
dispatch_rtmsg_addr(struct rt_msghdr *rtm, struct sockaddr *rti_info[RTAX_MAX],
    struct ktable *kt)
{
	struct sockaddr		*sa;
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct sockaddr_rtlabel	*label;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;
	struct bgpd_addr	 prefix;
	int			 flags, oflags, mpath = 0, changed = 0;
	int			 rtlabel_changed = 0;
	u_int16_t		 ifindex, new_labelid;
	u_int8_t		 prefixlen;
	u_int8_t		 prio;

	flags = F_KERNEL;
	ifindex = 0;
	prefixlen = 0;
	bzero(&prefix, sizeof(prefix));

	if ((sa = rti_info[RTAX_DST]) == NULL) {
		log_warnx("empty route message");
		return (0);
	}

	if (rtm->rtm_flags & RTF_STATIC)
		flags |= F_STATIC;
	if (rtm->rtm_flags & RTF_BLACKHOLE)
		flags |= F_BLACKHOLE;
	if (rtm->rtm_flags & RTF_REJECT)
		flags |= F_REJECT;
	if (rtm->rtm_flags & RTF_DYNAMIC)
		flags |= F_DYNAMIC;
#ifdef RTF_MPATH
	if (rtm->rtm_flags & RTF_MPATH)
		mpath = 1;
#endif

	prio = rtm->rtm_priority;
	label = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];

	switch (sa->sa_family) {
	case AF_INET:
		prefix.aid = AID_INET;
		prefix.v4.s_addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
		sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
		if (sa_in != NULL) {
			if (sa_in->sin_len != 0)
				prefixlen = mask2prefixlen(
				    sa_in->sin_addr.s_addr);
		} else if (rtm->rtm_flags & RTF_HOST)
			prefixlen = 32;
		else
			prefixlen =
			    prefixlen_classful(prefix.v4.s_addr);
		break;
	case AF_INET6:
		prefix.aid = AID_INET6;
		memcpy(&prefix.v6, &((struct sockaddr_in6 *)sa)->sin6_addr,
		    sizeof(struct in6_addr));
		sa_in6 = (struct sockaddr_in6 *)rti_info[RTAX_NETMASK];
		if (sa_in6 != NULL) {
			if (sa_in6->sin6_len != 0)
				prefixlen = mask2prefixlen6(sa_in6);
		} else if (rtm->rtm_flags & RTF_HOST)
			prefixlen = 128;
		else
			fatalx("in6 net addr without netmask");
		break;
	default:
		return (0);
	}

	if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
		switch (sa->sa_family) {
		case AF_LINK:
			flags |= F_CONNECTED;
			ifindex = rtm->rtm_index;
			sa = NULL;
			mpath = 0;	/* link local stuff can't be mpath */
			break;
		case AF_INET:
		case AF_INET6:
			if (rtm->rtm_flags & RTF_CONNECTED) {
				flags |= F_CONNECTED;
				ifindex = rtm->rtm_index;
				sa = NULL;
				mpath = 0; /* link local stuff can't be mpath */
			}
			break;
		}

	if (rtm->rtm_type == RTM_DELETE) {
		switch (prefix.aid) {
		case AID_INET:
			sa_in = (struct sockaddr_in *)sa;
			if ((kr = kroute_find(kt, prefix.v4.s_addr,
			    prefixlen, prio)) == NULL)
				return (0);
			if (!(kr->r.flags & F_KERNEL))
				return (0);

			if (mpath)
				/* get the correct route */
				if ((kr = kroute_matchgw(kr, sa_in)) == NULL) {
					log_warnx("%s[delete]: "
					    "mpath route not found", __func__);
					return (0);
				}

			if (kroute_remove(kt, kr) == -1)
				return (-1);
			break;
		case AID_INET6:
			sa_in6 = (struct sockaddr_in6 *)sa;
			if ((kr6 = kroute6_find(kt, &prefix.v6, prefixlen,
			    prio)) == NULL)
				return (0);
			if (!(kr6->r.flags & F_KERNEL))
				return (0);

			if (mpath)
				/* get the correct route */
				if ((kr6 = kroute6_matchgw(kr6, sa_in6)) ==
				    NULL) {
					log_warnx("%s[delete]: IPv6 mpath "
					    "route not found", __func__);
					return (0);
				}

			if (kroute6_remove(kt, kr6) == -1)
				return (-1);
			break;
		}
		return (0);
	}

	if (sa == NULL && !(flags & F_CONNECTED)) {
		log_warnx("%s: no nexthop for %s/%u",
		    __func__, log_addr(&prefix), prefixlen);
		return (0);
	}

	switch (prefix.aid) {
	case AID_INET:
		sa_in = (struct sockaddr_in *)sa;
		if ((kr = kroute_find(kt, prefix.v4.s_addr, prefixlen,
		    prio)) != NULL) {
			if (kr->r.flags & F_KERNEL) {
				/* get the correct route */
				if (mpath && rtm->rtm_type == RTM_CHANGE &&
				    (kr = kroute_matchgw(kr, sa_in)) == NULL) {
					log_warnx("%s[change]: "
					    "mpath route not found", __func__);
					goto add4;
				} else if (mpath && rtm->rtm_type == RTM_ADD)
					goto add4;

				if (sa_in != NULL) {
					if (kr->r.nexthop.s_addr !=
					    sa_in->sin_addr.s_addr)
						changed = 1;
					kr->r.nexthop.s_addr =
					    sa_in->sin_addr.s_addr;
				} else {
					if (kr->r.nexthop.s_addr != 0)
						changed = 1;
					kr->r.nexthop.s_addr = 0;
				}

				if (kr->r.flags & F_NEXTHOP)
					flags |= F_NEXTHOP;

				if (label != NULL) {
					new_labelid =
					    rtlabel_name2id(label->sr_label);
					if (kr->r.labelid != new_labelid) {
						rtlabel_unref(kr->r.labelid);
						kr->r.labelid = 0;
						flags |= F_RTLABEL;
						kr->r.labelid = new_labelid;
						rtlabel_changed = 1;
					}
				} else if (kr->r.labelid && label == NULL) {
					rtlabel_unref(kr->r.labelid);
					kr->r.labelid = 0;
					flags &= ~F_RTLABEL;
					rtlabel_changed = 1;
				}

				oflags = kr->r.flags;
				if (flags != oflags)
					changed = 1;
				kr->r.flags = flags;

				if (rtlabel_changed)
					kr_redistribute(IMSG_NETWORK_ADD,
					    kt, &kr->r);

				if ((oflags & F_CONNECTED) &&
				    !(flags & F_CONNECTED)) {
					kif_kr_remove(kr);
					kr_redistribute(IMSG_NETWORK_ADD,
					    kt, &kr->r);
				}
				if ((flags & F_CONNECTED) &&
				    !(oflags & F_CONNECTED)) {
					kif_kr_insert(kr);
					kr_redistribute(IMSG_NETWORK_ADD,
					    kt, &kr->r);
				}
				if (kr->r.flags & F_NEXTHOP && changed)
					knexthop_track(kt, kr);
			}
		} else if (rtm->rtm_type == RTM_CHANGE) {
			log_warnx("%s: change req for %s/%u: not in table",
			    __func__, log_addr(&prefix), prefixlen);
			return (0);
		} else {
add4:
			if ((kr = calloc(1,
			    sizeof(struct kroute_node))) == NULL) {
				log_warn("%s", __func__);
				return (-1);
			}
			kr->r.prefix.s_addr = prefix.v4.s_addr;
			kr->r.prefixlen = prefixlen;
			if (sa_in != NULL)
				kr->r.nexthop.s_addr = sa_in->sin_addr.s_addr;
			else
				kr->r.nexthop.s_addr = 0;
			kr->r.flags = flags;
			kr->r.ifindex = ifindex;
			kr->r.priority = prio;

			if (label) {
				kr->r.flags |= F_RTLABEL;
				kr->r.labelid =
				    rtlabel_name2id(label->sr_label);
			}
			kroute_insert(kt, kr);
		}
		break;
	case AID_INET6:
		sa_in6 = (struct sockaddr_in6 *)sa;
		if ((kr6 = kroute6_find(kt, &prefix.v6, prefixlen, prio)) !=
		    NULL) {
			if (kr6->r.flags & F_KERNEL) {
				/* get the correct route */
				if (mpath && rtm->rtm_type == RTM_CHANGE &&
				    (kr6 = kroute6_matchgw(kr6, sa_in6)) ==
				    NULL) {
					log_warnx("%s[change]: IPv6 mpath "
					    "route not found", __func__);
					goto add6;
				} else if (mpath && rtm->rtm_type == RTM_ADD)
					goto add6;

				if (sa_in6 != NULL) {
					if (memcmp(&kr6->r.nexthop,
					    &sa_in6->sin6_addr,
					    sizeof(struct in6_addr)))
						changed = 1;
					memcpy(&kr6->r.nexthop,
					    &sa_in6->sin6_addr,
					    sizeof(struct in6_addr));
				} else {
					if (memcmp(&kr6->r.nexthop,
					    &in6addr_any,
					    sizeof(struct in6_addr)))
						changed = 1;
					memcpy(&kr6->r.nexthop,
					    &in6addr_any,
					    sizeof(struct in6_addr));
				}

				if (kr6->r.flags & F_NEXTHOP)
					flags |= F_NEXTHOP;

				if (label != NULL) {
					new_labelid =
					    rtlabel_name2id(label->sr_label);
					if (kr6->r.labelid != new_labelid) {
						rtlabel_unref(kr6->r.labelid);
						kr6->r.labelid = 0;
						flags |= F_RTLABEL;
						kr6->r.labelid = new_labelid;
						rtlabel_changed = 1;
					}
				} else if (kr6->r.labelid && label == NULL) {
					rtlabel_unref(kr6->r.labelid);
					kr6->r.labelid = 0;
					flags &= ~F_RTLABEL;
					rtlabel_changed = 1;
				}

				oflags = kr6->r.flags;
				if (flags != oflags)
					changed = 1;
				kr6->r.flags = flags;

				if (rtlabel_changed)
					kr_redistribute6(IMSG_NETWORK_ADD,
					    kt, &kr6->r);

				if ((oflags & F_CONNECTED) &&
				    !(flags & F_CONNECTED)) {
					kif_kr6_remove(kr6);
					kr_redistribute6(IMSG_NETWORK_ADD,
					    kt, &kr6->r);
				}
				if ((flags & F_CONNECTED) &&
				    !(oflags & F_CONNECTED)) {
					kif_kr6_insert(kr6);
					kr_redistribute6(IMSG_NETWORK_ADD,
					    kt, &kr6->r);
				}

				if (kr6->r.flags & F_NEXTHOP && changed)
					knexthop_track(kt, kr6);
			}
		} else if (rtm->rtm_type == RTM_CHANGE) {
			log_warnx("%s: change req for %s/%u: not in table",
			    __func__, log_addr(&prefix), prefixlen);
			return (0);
		} else {
add6:
			if ((kr6 = calloc(1,
			    sizeof(struct kroute6_node))) == NULL) {
				log_warn("%s", __func__);
				return (-1);
			}
			memcpy(&kr6->r.prefix, &prefix.v6,
			    sizeof(struct in6_addr));
			kr6->r.prefixlen = prefixlen;
			if (sa_in6 != NULL)
				memcpy(&kr6->r.nexthop, &sa_in6->sin6_addr,
				    sizeof(struct in6_addr));
			else
				memcpy(&kr6->r.nexthop, &in6addr_any,
				    sizeof(struct in6_addr));
			kr6->r.flags = flags;
			kr6->r.ifindex = ifindex;
			kr6->r.priority = prio;

			if (label) {
				kr6->r.flags |= F_RTLABEL;
				kr6->r.labelid =
				    rtlabel_name2id(label->sr_label);
			}
			kroute6_insert(kt, kr6);
		}
		break;
	}

	return (0);
}
