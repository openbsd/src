/*	$OpenBSD: kroute.c,v 1.59 2004/01/09 13:47:07 henning Exp $ */

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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/route.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

struct {
	u_int32_t		rtseq;
	pid_t			pid;
	int			fib_sync;
	int			fd;
} kr_state;

struct kroute_node {
	RB_ENTRY(kroute_node)	 entry;
	struct kroute		 r;
};

struct knexthop_node {
	RB_ENTRY(knexthop_node)	 entry;
	in_addr_t		 nexthop;
	struct kroute_node	*kroute;
};

struct kif_kr {
	LIST_ENTRY(kif_kr)	 entry;
	struct kroute_node	*kr;
};

LIST_HEAD(kif_kr_head, kif_kr);

struct kif_node {
	RB_ENTRY(kif_node)	 entry;
	u_short			 ifindex;
	int			 flags;
	struct kif_kr_head	 kroute_l;
};

int	kroute_compare(struct kroute_node *, struct kroute_node *);
int	knexthop_compare(struct knexthop_node *, struct knexthop_node *);
int	kif_compare(struct kif_node *, struct kif_node *);

struct kroute_node	*kroute_find(in_addr_t, u_int8_t);
int			 kroute_insert(struct kroute_node *);
int			 kroute_remove(struct kroute_node *);

struct knexthop_node	*knexthop_find(in_addr_t);
int			 knexthop_insert(struct knexthop_node *);
int			 knexthop_remove(struct knexthop_node *);

struct kif_node		*kif_find(int);
int			 kif_insert(struct kif_node *kif);

int			 kif_kr_insert(struct kroute_node *);
int			 kif_kr_remove(struct kroute_node *);

void			 knexthop_validate(struct knexthop_node *);
struct kroute_node	*kroute_match(in_addr_t);
void			 kroute_attach_nexthop(struct knexthop_node *,
			    struct kroute_node *);
void			 kroute_detach_nexthop(struct knexthop_node *);

int		protect_lo(void);
u_int8_t	prefixlen_classful(in_addr_t);
u_int8_t	mask2prefixlen(in_addr_t);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		if_change(u_short, int);

int		send_rtmsg(int, int, struct kroute *);
int		dispatch_rtmsg(void);
int		fetchtable(void);
int		fetchifs(int);

RB_HEAD(kroute_tree, kroute_node)	kroute_tree, krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare);
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare);

RB_HEAD(knexthop_tree, knexthop_node)	knexthop_tree, knt;
RB_PROTOTYPE(knexthop_tree, knexthop_node, entry, knexthop_compare);
RB_GENERATE(knexthop_tree, knexthop_node, entry, knexthop_compare);

RB_HEAD(kif_tree, kif_node)		kif_tree, kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare);
RB_GENERATE(kif_tree, kif_node, entry, kif_compare);

/*
 * exported functions
 */

int
kr_init(int fs)
{
	int opt;

	kr_state.fib_sync = fs;

	if ((kr_state.fd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1) {
		log_err("kr_init: socket");
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_err("kr_init: setsockopt");	/* not fatal */

	kr_state.pid = getpid();
	kr_state.rtseq = 1;

	RB_INIT(&krt);
	RB_INIT(&knt);
	RB_INIT(&kit);

	if (fetchifs(0) == -1)
		return (-1);

	if (fetchtable() == -1)
		return (-1);

	if (protect_lo() == -1)
		return (-1);

	return (kr_state.fd);
}

int
kr_change(struct kroute *kroute)
{
	struct kroute_node	*kr;
	int			 action = RTM_ADD;

	if ((kr = kroute_find(kroute->prefix, kroute->prefixlen)) !=
	    NULL) {
		if (kr->r.flags & F_BGPD_INSERTED)
			action = RTM_CHANGE;
		else	/* a non-bgp route already exists. not a problem */
			return (0);
	}

	if (send_rtmsg(kr_state.fd, action, kroute) == -1)
		return (-1);

	if (action == RTM_ADD) {
		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_err("kr_change");
			return (-1);
		}
		kr->r.prefix = kroute->prefix;
		kr->r.prefixlen = kroute->prefixlen;
		kr->r.nexthop = kroute->nexthop;
		kr->r.flags = F_BGPD_INSERTED;

		if (kroute_insert(kr) == -1)
			free(kr);
	} else
		kr->r.nexthop = kroute->nexthop;

	return (0);
}

int
kr_delete(struct kroute *kroute)
{
	struct kroute_node	*kr;

	if ((kr = kroute_find(kroute->prefix, kroute->prefixlen)) == NULL)
		return (0);

	if (!(kr->r.flags & F_BGPD_INSERTED))
		return (0);

	if (send_rtmsg(kr_state.fd, RTM_DELETE, kroute) == -1)
		return (-1);

	if (kroute_remove(kr) == -1)
		return (-1);

	return (0);
}

void
kr_shutdown(void)
{
	kr_fib_decouple();
}

void
kr_fib_couple(void)
{
	struct kroute_node	*kr;

	if (kr_state.fib_sync == 1)	/* already coupled */
		return;

	kr_state.fib_sync = 1;

	RB_FOREACH(kr, kroute_tree, &krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_ADD, &kr->r);

	logit(LOG_INFO, "kernel routing table coupled");
}

void
kr_fib_decouple(void)
{
	struct kroute_node	*kr;

	if (kr_state.fib_sync == 0)	/* already decoupled */
		return;

	RB_FOREACH(kr, kroute_tree, &krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_DELETE, &kr->r);

	kr_state.fib_sync = 0;

	logit(LOG_INFO, "kernel routing table decoupled");
}

int
kr_dispatch_msg(void)
{
	return (dispatch_rtmsg());
}

int
kr_nexthop_add(in_addr_t key)
{
	struct knexthop_node	*h;

	if ((h = knexthop_find(key)) != NULL) {
		/* should not happen... this is really an error path */
		struct kroute_nexthop	 nh;

		bzero(&nh, sizeof(nh));
		nh.nexthop = key;
		if (h->kroute != NULL) {
			nh.valid = 1;
			nh.connected = h->kroute->r.flags & F_CONNECTED;
			nh.gateway = h->kroute->r.nexthop;
		}
		send_nexthop_update(&nh);
	} else {
		if ((h = calloc(1, sizeof(struct knexthop_node))) == NULL) {
			log_err("kr_nexthop_add");
			return (-1);
		}
		h->nexthop = key;

		if (knexthop_insert(h) == -1)
			return (-1);
	}

	return (0);
}

void
kr_nexthop_delete(in_addr_t key)
{
	struct knexthop_node	*kn;

	if ((kn = knexthop_find(key)) == NULL)
		return;

	knexthop_remove(kn);
}

void
kr_show_route(pid_t pid)
{
	struct kroute_node	*kr;

	RB_FOREACH(kr, kroute_tree, &krt)
		send_imsg_session(IMSG_CTL_KROUTE, pid, &kr->r, sizeof(kr->r));

	send_imsg_session(IMSG_CTL_END, pid, NULL, 0);
}

/*
 * RB-tree compare functions
 */

int
kroute_compare(struct kroute_node *a, struct kroute_node *b)
{
	if (a->r.prefix < b->r.prefix)
		return (-1);
	if (a->r.prefix > b->r.prefix)
		return (1);
	if (a->r.prefixlen < b->r.prefixlen)
		return (-1);
	if (a->r.prefixlen > b->r.prefixlen)
		return (1);
	return (0);
}

int
knexthop_compare(struct knexthop_node *a, struct knexthop_node *b)
{
	return (b->nexthop - a->nexthop);
}

int
kif_compare(struct kif_node *a, struct kif_node *b)
{
	return (b->ifindex - a->ifindex);
}


/*
 * tree management functions
 */

struct kroute_node *
kroute_find(in_addr_t prefix, u_int8_t prefixlen)
{
	struct kroute_node	s;

	s.r.prefix = prefix;
	s.r.prefixlen = prefixlen;

	return (RB_FIND(kroute_tree, &krt, &s));
}

int
kroute_insert(struct kroute_node *kr)
{
	struct knexthop_node	*h;
	in_addr_t		 mask, ina;

	if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
		logit(LOG_CRIT, "kroute_tree insert failed for %s/%u",
		    log_ntoa(kr->r.prefix), kr->r.prefixlen);
		free(kr);
		return (-1);
	}

	if (kr->r.flags & F_KERNEL) {
		mask = 0xffffffff << (32 - kr->r.prefixlen);
		ina = ntohl(kr->r.prefix);
		RB_FOREACH(h, knexthop_tree, &knt)
			if ((ntohl(h->nexthop) & mask) == ina)
				knexthop_validate(h);

		if (kr->r.flags & F_CONNECTED)
			if (kif_kr_insert(kr) == -1)
				return (-1);
	}
	return (0);
}

int
kroute_remove(struct kroute_node *kr)
{
	struct knexthop_node	*s;

	if (RB_REMOVE(kroute_tree, &krt, kr) == NULL) {
		logit(LOG_CRIT, "kroute_remove failed for %s/%u",
		    log_ntoa(kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	/* check wether a nexthop depends on this kroute */
	if ((kr->r.flags & F_KERNEL) && (kr->r.flags & F_NEXTHOP))
		RB_FOREACH(s, knexthop_tree, &knt)
			if (s->kroute == kr)
				knexthop_validate(s);

	if (kr->r.flags & F_CONNECTED)
		if (kif_kr_remove(kr) == -1) {
			free(kr);
			return (-1);
		}

	free(kr);
	return (0);
}

struct knexthop_node *
knexthop_find(in_addr_t key)
{
	struct knexthop_node	s;

	bzero(&s, sizeof(s));
	s.nexthop = key;

	return (RB_FIND(knexthop_tree, &knt, &s));
}

int
knexthop_insert(struct knexthop_node *kn)
{
	if (RB_INSERT(knexthop_tree, &knt, kn) != NULL) {
		logit(LOG_CRIT, "knexthop_tree insert failed for %s",
			    log_ntoa(kn->nexthop));
		free(kn);
		return (-1);
	}

	knexthop_validate(kn);

	return (0);
}

int
knexthop_remove(struct knexthop_node *kn)
{
	kroute_detach_nexthop(kn);

	if (RB_REMOVE(knexthop_tree, &knt, kn) == NULL) {
		logit(LOG_CRIT, "knexthop_remove failed for %s",
		    log_ntoa(kn->nexthop));
		return (-1);
	}

	free(kn);
	return (0);
}

struct kif_node *
kif_find(int ifindex)
{
	struct kif_node	*kif, s;

	bzero(&s, sizeof(s));
	s.ifindex = ifindex;

	if ((kif = RB_FIND(kif_tree, &kit, &s)) != NULL)
		return (kif);

	/* check wether the interface showed up now */
	fetchifs(ifindex);
	return (RB_FIND(kif_tree, &kit, &s));
}

int
kif_insert(struct kif_node *kif)
{
	if (RB_INSERT(kif_tree, &kit, kif) != NULL) {
		logit(LOG_CRIT, "RB_INSERT(kif_tree, &kit, kif)");
		free(kif);
		return (-1);
	}

	return (0);
}


int
kif_kr_insert(struct kroute_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr	*kkr;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		logit(LOG_CRIT, "interface with index %u not found",
		    kr->r.ifindex);
		return (0);
	}

	if ((kkr = calloc(1, sizeof(struct kif_kr))) == NULL) {
		log_err("kif_kr_insert");
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
		logit(LOG_CRIT, "interface with index %u not found",
		    kr->r.ifindex);
		return (0);
	}

	for (kkr = LIST_FIRST(&kif->kroute_l); kkr != NULL && kkr->kr != kr;
	    kkr = LIST_NEXT(kkr, entry))
		;	/* nothing */

	if (kkr == NULL) {
		logit(LOG_CRIT, "can't remove connected route from interface "
		    "with index %u: not found", kr->r.ifindex);
		return (-1);
	}

	LIST_REMOVE(kkr, entry);
	free(kkr);

	return (0);
}

/*
 * nexthop validation
 */

void
knexthop_validate(struct knexthop_node *kn)
{
	struct kroute_node	*kr;
	struct kroute_nexthop	 n;
	int			 was_valid = 0;

	if (kn->kroute != NULL && (!(kn->kroute->r.flags & F_DOWN)))
		was_valid = 1;

	bzero(&n, sizeof(n));
	n.nexthop = kn->nexthop;
	kroute_detach_nexthop(kn);

	if ((kr = kroute_match(kn->nexthop)) == NULL) {	/* no match */
		if (was_valid)
			send_nexthop_update(&n);
	} else {					/* found match */
		if (kr->r.flags & F_DOWN) {		/* but is down */
			if (was_valid)
				send_nexthop_update(&n);
		} else {				/* valid route */
			if (!was_valid) {
				n.valid = 1;
				n.connected = kr->r.flags & F_CONNECTED;
				n.gateway = kr->r.nexthop;
				send_nexthop_update(&n);
			}
		}
		kroute_attach_nexthop(kn, kr);
	}
}

struct kroute_node *
kroute_match(in_addr_t key)
{
	int			 i;
	struct kroute_node	*kr;
	in_addr_t		 ina;

	ina = ntohl(key);

	/* we will never match the default route */
	for (i = 32; i > 0; i--)
		if ((kr = kroute_find(
		    htonl(ina & (0xffffffff << (32 - i))), i)) != NULL)
			return (kr);

	/* if we don't have a match yet, try to find a default route */
	if ((kr = kroute_find(0, 0)) != NULL)
			return (kr);

	return (NULL);
}

void
kroute_attach_nexthop(struct knexthop_node *kn, struct kroute_node *kr)
{
	kn->kroute = kr;
	kr->r.flags |= F_NEXTHOP;
}

void
kroute_detach_nexthop(struct knexthop_node *kn)
{
	struct knexthop_node	*s;

	/*
	 * check wether there's another nexthop depending on this kroute
	 * if not remove the flag
	 */

	if (kn->kroute == NULL)
		return;

	for (s = RB_MIN(knexthop_tree, &knt); s != NULL &&
	    s->kroute != kn->kroute; s = RB_NEXT(knexthop_tree, &knt, s))
		;	/* nothing */

	if (s == NULL)
		kn->kroute->r.flags &= ~F_NEXTHOP;

	kn->kroute = NULL;
}


/*
 * misc helpers
 */

int
protect_lo(void)
{
	struct kroute_node	*kr;

	/* special protection for 127/8 */
	if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
		log_err("protect_lo");
		return (-1);
	}
	kr->r.prefix = inet_addr("127.0.0.1");
	kr->r.prefixlen = 8;
	kr->r.nexthop = 0;
	kr->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute_tree, &krt, kr) != NULL)
		free(kr);	/* kernel route already there, no problem */

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

#define	ROUNDUP(a, size)	\
    (((a) & ((size) - 1)) ? (1 + ((a) | ((size) - 1))) : (a))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len, sizeof(long)));
		} else
			rti_info[i] = NULL;
	}
}

void
if_change(u_short ifindex, int flags)
{
	struct kif_node		*kif;
	struct kif_kr		*kkr;
	struct kroute_nexthop	 nh;
	struct knexthop_node	*n;

	if ((kif = kif_find(ifindex)) == NULL) {
		logit(LOG_CRIT, "interface with index %u not found",
		    ifindex);
		return;
	}

	LIST_FOREACH(kkr, &kif->kroute_l, entry) {
		if (flags & IFF_UP)
			kkr->kr->r.flags &= ~F_DOWN;
		else
			kkr->kr->r.flags |= F_DOWN;

		RB_FOREACH(n, knexthop_tree, &knt)
			if (n->kroute == kkr->kr) {
				bzero(&nh, sizeof(nh));
				nh.nexthop = n->nexthop;
				if (!(kkr->kr->r.flags & F_DOWN)) {
					nh.valid = 1;
					nh.connected = 1;
					nh.gateway = kkr->kr->r.nexthop;
				}
				send_nexthop_update(&nh);
			}
	}
}


/*
 * rtsock related functions
 */

int
send_rtmsg(int fd, int action, struct kroute *kroute)
{
	struct {
		struct rt_msghdr	hdr;
		struct sockaddr_in	prefix;
		struct sockaddr_in	nexthop;
		struct sockaddr_in	mask;
	} r;
	ssize_t	n;

	if (kr_state.fib_sync == 0)
		return (0);

	bzero(&r, sizeof(r));
	r.hdr.rtm_msglen = sizeof(r);
	r.hdr.rtm_version = RTM_VERSION;
	r.hdr.rtm_type = action;
	r.hdr.rtm_flags = RTF_GATEWAY|RTF_PROTO1;
	r.hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	r.hdr.rtm_addrs = RTA_DST|RTA_GATEWAY|RTA_NETMASK;
	r.prefix.sin_len = sizeof(r.prefix);
	r.prefix.sin_family = AF_INET;
	r.prefix.sin_addr.s_addr = kroute->prefix;
	r.nexthop.sin_len = sizeof(r.nexthop);
	r.nexthop.sin_family = AF_INET;
	r.nexthop.sin_addr.s_addr = kroute->nexthop;
	r.mask.sin_len = sizeof(r.mask);
	r.mask.sin_family = AF_INET;
	r.mask.sin_addr.s_addr = htonl(0xffffffff << (32 - kroute->prefixlen));

retry:
	if ((n = write(fd, &r, sizeof(r))) == -1) {
		switch (errno) {
		case ESRCH:
			if (r.hdr.rtm_type == RTM_CHANGE) {
				r.hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (r.hdr.rtm_type == RTM_DELETE) {
				logit(LOG_INFO,
				    "route %s/%u vanished before delete",
				    log_ntoa(kroute->prefix),
				    kroute->prefixlen);
				return (0);
			} else {
				logit(LOG_CRIT,
				    "send_rtmsg: action %u, "
				    "prefix %s/%u: %s", r.hdr.rtm_type,
				    log_ntoa(kroute->prefix), kroute->prefixlen,
				    strerror(errno));
				return (0);
			}
			break;
		default:
			logit(LOG_CRIT,
			    "send_rtmsg: action %u, prefix %s/%u: %s",
			    r.hdr.rtm_type, log_ntoa(kroute->prefix),
			    kroute->prefixlen, strerror(errno));
			return (0);
		}
	}

	return (0);
}

int
fetchtable(void)
{
	size_t			 len;
	int			 mib[6];
	char			*buf, *next, *lim;
	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct kroute_node	*kr;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_err("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_err("fetchtable");
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_err("sysctl");
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		if ((sa = rti_info[RTAX_DST]) == NULL)
			continue;

		if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
			continue;

		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_err("fetchtable");
			return (-1);
		}

		kr->r.flags = F_KERNEL;

		switch (sa->sa_family) {
		case AF_INET:
			kr->r.prefix =
			    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
			if (kr->r.prefix == 0)	/* default route */
				kr->r.prefixlen = 0;
			else if (sa_in != NULL)
				kr->r.prefixlen =
				    mask2prefixlen(sa_in->sin_addr.s_addr);
			else if (rtm->rtm_flags & RTF_HOST)
				kr->r.prefixlen = 32;
			else
				kr->r.prefixlen =
				    prefixlen_classful(kr->r.prefix);
			break;
		default:
			continue;
			/* not reached */
		}

		if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
			switch (sa->sa_family) {
			case AF_INET:
				kr->r.nexthop =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case AF_LINK:
				kr->r.flags |= F_CONNECTED;
				kr->r.ifindex = rtm->rtm_index;
				break;
			}

		kroute_insert(kr);

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
	struct if_msghdr	*ifm;
	struct kif_node		*kif;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = ifindex;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_err("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_err("fetchif");
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_err("sysctl");
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		ifm = (struct if_msghdr *)next;
		if (ifm->ifm_type != RTM_IFINFO)
			continue;

		if ((kif = calloc(1, sizeof(struct kif_node))) == NULL) {
			log_err("fetchifs");
			return (-1);
		}

		LIST_INIT(&kif->kroute_l);
		kif->ifindex = ifm->ifm_index;
		kif->flags = ifm->ifm_flags;
		kif_insert(kif);
	}
	return (0);
}

int
dispatch_rtmsg(void)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;
	char			*next, *lim;
	struct rt_msghdr	*rtm;
	struct if_msghdr	*ifm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct kroute_node	*kr;
	in_addr_t		 prefix, nexthop;
	u_int8_t		 prefixlen;
	int			 flags;
	u_short			 ifindex;

	if ((n = read(kr_state.fd, &buf, sizeof(buf))) == -1) {
		log_err("dispatch_rtmsg: read error");
		return (-1);
	}

	if (n == 0) {
		logit(LOG_CRIT, "routing socket closed");
		return (-1);
	}

	lim = buf + n;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		prefix = 0;
		prefixlen = 0;
		flags = F_KERNEL;
		nexthop = 0;
		ifindex = 0;

		if ((sa = rti_info[RTAX_DST]) == NULL)
			continue;

		if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
			continue;

		if (rtm->rtm_pid == kr_state.pid)	/* cause by us */
			continue;

		if (rtm->rtm_errno)			/* failed attempts... */
			continue;

		if (rtm->rtm_type != RTM_IFINFO)
			switch (sa->sa_family) {
			case AF_INET:
				prefix =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				sa_in = (struct sockaddr_in *)
				    rti_info[RTAX_NETMASK];
				if (sa_in != NULL) {
					if (sa_in->sin_family != AF_INET)
						continue;
					prefixlen = mask2prefixlen(
					    sa_in->sin_addr.s_addr);
				} else if (rtm->rtm_flags & RTF_HOST)
					prefixlen = 32;
				else
					prefixlen = prefixlen_classful(prefix);
				break;
			default:
				continue;
				/* not reached */
			}

		if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
			switch (sa->sa_family) {
			case AF_INET:
				nexthop =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				break;
			case AF_LINK:
				flags |= F_CONNECTED;
				ifindex = rtm->rtm_index;
				break;
			}

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
			if (nexthop == 0 && !(flags & F_CONNECTED)) {
				logit(LOG_CRIT,
				    "dispatch_rtmsg: no nexthop for %s/%u",
				    log_ntoa(prefix), prefixlen);
				continue;
			}

			if ((kr = kroute_find(prefix, prefixlen)) !=
			    NULL) {
				if (kr->r.flags & F_KERNEL) {
					kr->r.nexthop = nexthop;
					if (kr->r.flags & F_NEXTHOP)
						flags |= F_NEXTHOP;
					if ((kr->r.flags & F_CONNECTED) &&
					    !(flags & F_CONNECTED))
						kif_kr_remove(kr);
					if ((flags & F_CONNECTED) &&
					    !(kr->r.flags & F_CONNECTED))
						kif_kr_insert(kr);
					kr->r.flags = flags;
				}
			} else {
				if ((kr = calloc(1,
				    sizeof(struct kroute_node))) == NULL) {
					log_err("dispatch_rtmsg");
					return (-1);
				}
				kr->r.prefix = prefix;
				kr->r.prefixlen = prefixlen;
				kr->r.nexthop = nexthop;
				kr->r.flags = flags;

				kroute_insert(kr);
			}
			break;
		case RTM_DELETE:
			if ((kr = kroute_find(prefix, prefixlen)) == NULL)
				continue;
			if (!(kr->r.flags & F_KERNEL))
				continue;
			if (kroute_remove(kr) == -1)
				return (-1);
			break;
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)next;
			if_change(ifm->ifm_index, ifm->ifm_flags);
			break;
		default:
			/* ignore for now */
			break;
		}
	}
	return (0);
}
