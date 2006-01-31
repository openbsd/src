/*	$OpenBSD: kroute.c,v 1.143 2006/01/31 15:22:15 claudio Exp $ */

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
#include <net/if_dl.h>
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

struct kroute6_node {
	RB_ENTRY(kroute6_node)	 entry;
	struct kroute6		 r;
};

struct knexthop_node {
	RB_ENTRY(knexthop_node)	 entry;
	struct bgpd_addr	 nexthop;
	void			*kroute;
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

int	kr_redistribute(int, struct kroute *);
int	kr_redistribute6(int, struct kroute6 *);
int	kroute_compare(struct kroute_node *, struct kroute_node *);
int	kroute6_compare(struct kroute6_node *, struct kroute6_node *);
int	knexthop_compare(struct knexthop_node *, struct knexthop_node *);
int	kif_compare(struct kif_node *, struct kif_node *);

struct kroute_node	*kroute_find(in_addr_t, u_int8_t);
int			 kroute_insert(struct kroute_node *);
int			 kroute_remove(struct kroute_node *);
void			 kroute_clear(void);

struct kroute6_node	*kroute6_find(const struct in6_addr *, u_int8_t);
int			 kroute6_insert(struct kroute6_node *);
int			 kroute6_remove(struct kroute6_node *);
void			 kroute6_clear(void);

struct knexthop_node	*knexthop_find(struct bgpd_addr *);
int			 knexthop_insert(struct knexthop_node *);
int			 knexthop_remove(struct knexthop_node *);
void			 knexthop_clear(void);

struct kif_node		*kif_find(int);
int			 kif_insert(struct kif_node *);
int			 kif_remove(struct kif_node *);
void			 kif_clear(void);

int			 kif_kr_insert(struct kroute_node *);
int			 kif_kr_remove(struct kroute_node *);

int			 kif_kr6_insert(struct kroute6_node *);
int			 kif_kr6_remove(struct kroute6_node *);

int			 kif_validate(struct kif *);
int			 kroute_validate(struct kroute *);
int			 kroute6_validate(struct kroute6 *);
void			 knexthop_validate(struct knexthop_node *);
struct kroute_node	*kroute_match(in_addr_t);
struct kroute6_node	*kroute6_match(struct in6_addr *);
void			 kroute_detach_nexthop(struct knexthop_node *);

int		protect_lo(void);
u_int8_t	prefixlen_classful(in_addr_t);
u_int8_t	mask2prefixlen(in_addr_t);
u_int8_t	mask2prefixlen6(struct sockaddr_in6 *);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		if_change(u_short, int, struct if_data *);
void		if_announce(void *);

int		send_rtmsg(int, int, struct kroute *);
int		send_rt6msg(int, int, struct kroute6 *);
int		dispatch_rtmsg(void);
int		fetchtable(void);
int		fetchifs(int);
int		dispatch_rtmsg_addr(struct rt_msghdr *,
		    struct sockaddr *[RTAX_MAX]);

RB_HEAD(kroute_tree, kroute_node)	krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare)
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare)

RB_HEAD(kroute6_tree, kroute6_node)	krt6;
RB_PROTOTYPE(kroute6_tree, kroute6_node, entry, kroute6_compare)
RB_GENERATE(kroute6_tree, kroute6_node, entry, kroute6_compare)

RB_HEAD(knexthop_tree, knexthop_node)	knt;
RB_PROTOTYPE(knexthop_tree, knexthop_node, entry, knexthop_compare)
RB_GENERATE(knexthop_tree, knexthop_node, entry, knexthop_compare)

RB_HEAD(kif_tree, kif_node)		kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare)
RB_GENERATE(kif_tree, kif_node, entry, kif_compare)

/*
 * exported functions
 */

int
kr_init(int fs)
{
	int		opt = 0, rcvbuf, default_rcvbuf;
	socklen_t	optlen;

	kr_state.fib_sync = fs;

	if ((kr_state.fd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1) {
		log_warn("kr_init: socket");
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_warn("kr_init: setsockopt");	/* not fatal */

	/* grow receive buffer, don't wanna miss messages */
	optlen = sizeof(default_rcvbuf);
	if (getsockopt(kr_state.fd, SOL_SOCKET, SO_RCVBUF,
	    &default_rcvbuf, &optlen) == -1)
		log_warn("kr_init getsockopt SOL_SOCKET SO_RCVBUF");
	else
		for (rcvbuf = MAX_RTSOCK_BUF;
		    rcvbuf > default_rcvbuf &&
		    setsockopt(kr_state.fd, SOL_SOCKET, SO_RCVBUF,
		    &rcvbuf, sizeof(rcvbuf)) == -1 && errno == ENOBUFS;
		    rcvbuf /= 2)
			;	/* nothing */

	kr_state.pid = getpid();
	kr_state.rtseq = 1;

	RB_INIT(&krt);
	RB_INIT(&krt6);
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
kr_change(struct kroute_label *kl)
{
	struct kroute_node	*kr;
	int			 action = RTM_ADD;

	if ((kr = kroute_find(kl->kr.prefix.s_addr, kl->kr.prefixlen)) !=
	    NULL) {
		if (kr->r.flags & F_BGPD_INSERTED)
			action = RTM_CHANGE;
		else	/* a non-bgp route already exists. not a problem */
			return (0);
	}

	/* nexthop within 127/8 -> ignore silently */
	if ((kl->kr.nexthop.s_addr & htonl(IN_CLASSA_NET)) ==
	    htonl(INADDR_LOOPBACK & IN_CLASSA_NET))
		return (0);

	if (kr)
		rtlabel_unref(kr->r.labelid);
	kl->kr.labelid = rtlabel_name2id(kl->label);

	if (send_rtmsg(kr_state.fd, action, &kl->kr) == -1)
		return (-1);

	if (action == RTM_ADD) {
		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_warn("kr_change");
			return (-1);
		}
		kr->r.prefix.s_addr = kl->kr.prefix.s_addr;
		kr->r.prefixlen = kl->kr.prefixlen;
		kr->r.nexthop.s_addr = kl->kr.nexthop.s_addr;
		kr->r.flags = kl->kr.flags | F_BGPD_INSERTED;
		kr->r.labelid = kl->kr.labelid;

		if (kroute_insert(kr) == -1)
			free(kr);
	} else {
		kr->r.nexthop.s_addr = kl->kr.nexthop.s_addr;
		kr->r.labelid = kl->kr.labelid;
		if (kl->kr.flags & F_BLACKHOLE)
			kr->r.flags |= F_BLACKHOLE;
		else
			kr->r.flags &= ~F_BLACKHOLE;
		if (kl->kr.flags & F_REJECT)
			kr->r.flags |= F_REJECT;
		else
			kr->r.flags &= ~F_REJECT;
	}

	return (0);
}

int
kr_delete(struct kroute_label *kl)
{
	struct kroute_node	*kr;

	if ((kr = kroute_find(kl->kr.prefix.s_addr, kl->kr.prefixlen)) ==
	    NULL)
		return (0);

	if (!(kr->r.flags & F_BGPD_INSERTED))
		return (0);

	/* nexthop within 127/8 -> ignore silently */
	if ((kl->kr.nexthop.s_addr & htonl(IN_CLASSA_NET)) ==
	    htonl(INADDR_LOOPBACK & IN_CLASSA_NET))
		return (0);

	if (send_rtmsg(kr_state.fd, RTM_DELETE, &kl->kr) == -1)
		return (-1);

	rtlabel_unref(kl->kr.labelid);

	if (kroute_remove(kr) == -1)
		return (-1);

	return (0);
}

int
kr6_change(struct kroute6_label *kl)
{
	struct kroute6_node	*kr6;
	int			 action = RTM_ADD;

	if ((kr6 = kroute6_find(&kl->kr.prefix, kl->kr.prefixlen)) != NULL) {
		if (kr6->r.flags & F_BGPD_INSERTED)
			action = RTM_CHANGE;
		else	/* a non-bgp route already exists. not a problem */
			return (0);
	}

	/* nexthop to loopback -> ignore silently */
	if (IN6_IS_ADDR_LOOPBACK(&kl->kr.nexthop))
		return (0);

	if (kr6)
		rtlabel_unref(kr6->r.labelid);
	kl->kr.labelid = rtlabel_name2id(kl->label);

	if (send_rt6msg(kr_state.fd, action, &kl->kr) == -1)
		return (-1);

	if (action == RTM_ADD) {
		if ((kr6 = calloc(1, sizeof(struct kroute6_node))) == NULL) {
			log_warn("kr_change");
			return (-1);
		}
		memcpy(&kr6->r.prefix, &kl->kr.prefix,
		    sizeof(struct in6_addr));
		kr6->r.prefixlen = kl->kr.prefixlen;
		memcpy(&kr6->r.nexthop, &kl->kr.nexthop,
		    sizeof(struct in6_addr));
		kr6->r.flags = kl->kr.flags | F_BGPD_INSERTED;
		kr6->r.labelid = kl->kr.labelid;

		if (kroute6_insert(kr6) == -1)
			free(kr6);
	} else {
		memcpy(&kr6->r.nexthop, &kl->kr.nexthop,
		    sizeof(struct in6_addr));
		kr6->r.labelid = kl->kr.labelid;
		if (kl->kr.flags & F_BLACKHOLE)
			kr6->r.flags |= F_BLACKHOLE;
		else
			kr6->r.flags &= ~F_BLACKHOLE;
		if (kl->kr.flags & F_REJECT)
			kr6->r.flags |= F_REJECT;
		else
			kr6->r.flags &= ~F_REJECT;
	}

	return (0);
}

int
kr6_delete(struct kroute6_label *kl)
{
	struct kroute6_node	*kr6;

	if ((kr6 = kroute6_find(&kl->kr.prefix, kl->kr.prefixlen)) == NULL)
		return (0);

	if (!(kr6->r.flags & F_BGPD_INSERTED))
		return (0);

	/* nexthop to loopback -> ignore silently */
	if (IN6_IS_ADDR_LOOPBACK(&kl->kr.nexthop))
		return (0);

	if (send_rt6msg(kr_state.fd, RTM_DELETE, &kl->kr) == -1)
		return (-1);

	rtlabel_unref(kl->kr.labelid);

	if (kroute6_remove(kr6) == -1)
		return (-1);

	return (0);
}

void
kr_shutdown(void)
{
	kr_fib_decouple();
	knexthop_clear();
	kroute_clear();
	kroute6_clear();
	kif_clear();
}

void
kr_fib_couple(void)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	if (kr_state.fib_sync == 1)	/* already coupled */
		return;

	kr_state.fib_sync = 1;

	RB_FOREACH(kr, kroute_tree, &krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_ADD, &kr->r);
	RB_FOREACH(kr6, kroute6_tree, &krt6)
		if ((kr6->r.flags & F_BGPD_INSERTED))
			send_rt6msg(kr_state.fd, RTM_ADD, &kr6->r);

	log_info("kernel routing table coupled");
}

void
kr_fib_decouple(void)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	if (kr_state.fib_sync == 0)	/* already decoupled */
		return;

	RB_FOREACH(kr, kroute_tree, &krt)
		if ((kr->r.flags & F_BGPD_INSERTED))
			send_rtmsg(kr_state.fd, RTM_DELETE, &kr->r);
	RB_FOREACH(kr6, kroute6_tree, &krt6)
		if ((kr6->r.flags & F_BGPD_INSERTED))
			send_rt6msg(kr_state.fd, RTM_DELETE, &kr6->r);

	kr_state.fib_sync = 0;

	log_info("kernel routing table decoupled");
}

int
kr_dispatch_msg(void)
{
	return (dispatch_rtmsg());
}

int
kr_nexthop_add(struct bgpd_addr *addr)
{
	struct knexthop_node	*h;

	if ((h = knexthop_find(addr)) != NULL) {
		/* should not happen... this is acctually an error path */
		struct kroute_nexthop	 nh;
		struct kroute_node	*k;
		struct kroute6_node	*k6;

		bzero(&nh, sizeof(nh));
		memcpy(&nh.nexthop, addr, sizeof(nh.nexthop));
		nh.valid = 1;
		if (h->kroute != NULL && addr->af == AF_INET) {
			k = h->kroute;
			nh.connected = k->r.flags & F_CONNECTED;
			if (k->r.nexthop.s_addr != 0) {
				nh.gateway.af = AF_INET;
				nh.gateway.v4.s_addr =
				    k->r.nexthop.s_addr;
			}
			memcpy(&nh.kr.kr4, &k->r, sizeof(nh.kr.kr4));
		} else if (h->kroute != NULL && addr->af == AF_INET6) {
			k6 = h->kroute;
			nh.connected = k6->r.flags & F_CONNECTED;
			if (memcmp(&k6->r.nexthop, &in6addr_any,
			    sizeof(struct in6_addr)) != 0) {
				nh.gateway.af = AF_INET6;
				memcpy(&nh.gateway.v6, &k6->r.nexthop,
				    sizeof(struct in6_addr));
			}
			memcpy(&nh.kr.kr6, &k6->r, sizeof(nh.kr.kr6));
		}

		send_nexthop_update(&nh);
	} else {
		if ((h = calloc(1, sizeof(struct knexthop_node))) == NULL) {
			log_warn("kr_nexthop_add");
			return (-1);
		}
		memcpy(&h->nexthop, addr, sizeof(h->nexthop));

		if (knexthop_insert(h) == -1)
			return (-1);
	}

	return (0);
}

void
kr_nexthop_delete(struct bgpd_addr *addr)
{
	struct knexthop_node	*kn;

	if ((kn = knexthop_find(addr)) == NULL)
		return;

	knexthop_remove(kn);
}

void
kr_show_route(struct imsg *imsg)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;
	struct bgpd_addr	*addr;
	int			 flags;
	sa_family_t		 af;
	struct ctl_show_nexthop	 snh;
	struct knexthop_node	*h;
	struct kif_node		*kif;
	u_short			 ifindex = 0;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(flags) +
		    sizeof(af)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		memcpy(&flags, imsg->data, sizeof(flags));
		memcpy(&af, (char *)imsg->data + sizeof(flags), sizeof(af));
		if (!af || af == AF_INET)
			RB_FOREACH(kr, kroute_tree, &krt)
				if (!flags || kr->r.flags & flags)
					send_imsg_session(IMSG_CTL_KROUTE,
					    imsg->hdr.pid, &kr->r,
					    sizeof(kr->r));
		if (!af || af == AF_INET6)
			RB_FOREACH(kr6, kroute6_tree, &krt6)
				if (!flags || kr6->r.flags & flags)
					send_imsg_session(IMSG_CTL_KROUTE6,
					    imsg->hdr.pid, &kr6->r,
					    sizeof(kr6->r));
		break;
	case IMSG_CTL_KROUTE_ADDR:
		if (imsg->hdr.len != IMSG_HEADER_SIZE +
		    sizeof(struct bgpd_addr)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		addr = imsg->data;
		kr = NULL;
		switch (addr->af) {
		case AF_INET:
			kr = kroute_match(addr->v4.s_addr);
			if (kr != NULL)
				send_imsg_session(IMSG_CTL_KROUTE,
				    imsg->hdr.pid, &kr->r, sizeof(kr->r));
			break;
		case AF_INET6:
			kr6 = kroute6_match(&addr->v6);
			if (kr6 != NULL)
				send_imsg_session(IMSG_CTL_KROUTE6,
				    imsg->hdr.pid, &kr6->r, sizeof(kr6->r));
			break;
		}
		break;
	case IMSG_CTL_SHOW_NEXTHOP:
		RB_FOREACH(h, knexthop_tree, &knt) {
			bzero(&snh, sizeof(snh));
			memcpy(&snh.addr, &h->nexthop, sizeof(snh.addr));
			if (h->kroute != NULL) {
				switch (h->nexthop.af) {
				case AF_INET:
					kr = h->kroute;
					snh.valid = kroute_validate(&kr->r);
					ifindex = kr->r.ifindex;
					break;
				case AF_INET6:
					kr6 = h->kroute;
					snh.valid = kroute6_validate(&kr6->r);
					ifindex = kr6->r.ifindex;
					break;
				}
				if ((kif = kif_find(ifindex)) != NULL)
					memcpy(&snh.kif, &kif->k,
					    sizeof(snh.kif));
			}
			send_imsg_session(IMSG_CTL_SHOW_NEXTHOP, imsg->hdr.pid,
			    &snh, sizeof(snh));
		}
		break;
	case IMSG_CTL_SHOW_INTERFACE:
		RB_FOREACH(kif, kif_tree, &kit)
			send_imsg_session(IMSG_CTL_SHOW_INTERFACE,
			    imsg->hdr.pid, &kif->k, sizeof(kif->k));
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

struct redist_node {
	LIST_ENTRY(redist_node)	 entry;
	struct kroute		*kr;
	struct kroute6		*kr6;
};


LIST_HEAD(, redist_node) redistlist;

int
kr_redistribute(int type, struct kroute *kr)
{
	struct redist_node	*rn;
	u_int32_t		 a;

	if (!(kr->flags & F_KERNEL))
		return (0);

	/* Dynamic routes are not redistributable. */
	if (kr->flags & F_DYNAMIC)
		return (0);

	/*
	 * We consider the loopback net, multicast and experimental addresses
	 * as not redistributable.
	 */
	a = ntohl(kr->prefix.s_addr);
	if (IN_MULTICAST(a) || IN_BADCLASS(a) ||
	    (a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		return (0);

	/* Consider networks with nexthop loopback as not redistributable. */
	if (kr->nexthop.s_addr == htonl(INADDR_LOOPBACK))
		return (0);

	/*
	 * never allow 0.0.0.0/0 the default route can only be redistributed
	 * with announce default.
	 */
	if (kr->prefix.s_addr == INADDR_ANY && kr->prefixlen == 0)
		return (0);

	/* Add or delete kr from list ... */
	LIST_FOREACH(rn, &redistlist, entry)
	    if (rn->kr == kr)
		    break;

	switch (type) {
	case IMSG_NETWORK_ADD:
		if (rn == NULL) {
			if ((rn = calloc(1, sizeof(struct redist_node))) ==
			    NULL) {
				log_warn("kr_redistribute");
				return (-1);
			}
			rn->kr = kr;
			LIST_INSERT_HEAD(&redistlist, rn, entry);
		}
		break;
	case IMSG_NETWORK_REMOVE:
		if (rn != NULL) {
			LIST_REMOVE(rn, entry);
			free(rn);
		}
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	return (bgpd_redistribute(type, kr, NULL));
}

int
kr_redistribute6(int type, struct kroute6 *kr6)
{
	struct redist_node	*rn;

	if (!(kr6->flags & F_KERNEL))
		return (0);

	/* Dynamic routes are not redistributable. */
	if (kr6->flags & F_DYNAMIC)
		return (0);

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
		return (0);

	/*
	 * Consider networks with nexthop loopback as not redistributable.
	 */
	if (IN6_IS_ADDR_LOOPBACK(&kr6->nexthop))
		return (0);

	/*
	 * never allow ::/0 the default route can only be redistributed
	 * with announce default.
	 */
	if (memcmp(&kr6->prefix, &in6addr_any, sizeof(struct in6_addr)) == 0 &&
	    kr6->prefixlen == 0)
		return (0);

	/* Add or delete kr from list ...
	 * using a linear list to store the redistributed networks will hurt
	 * as soon as redistribute ospf comes but until then keep it simple.
	 */
	LIST_FOREACH(rn, &redistlist, entry)
	    if (rn->kr6 == kr6)
		    break;

	switch (type) {
	case IMSG_NETWORK_ADD:
		if (rn == NULL) {
			if ((rn = calloc(1, sizeof(struct redist_node))) ==
			    NULL) {
				log_warn("kr_redistribute");
				return (-1);
			}
			rn->kr6 = kr6;
			LIST_INSERT_HEAD(&redistlist, rn, entry);
		}
		break;
	case IMSG_NETWORK_REMOVE:
		if (rn != NULL) {
			LIST_REMOVE(rn, entry);
			free(rn);
		}
		break;
	default:
		errno = EINVAL;
		return (-1);
	}

	return (bgpd_redistribute(type, NULL, kr6));
}

int
kr_redist_reload(void)
{
	struct redist_node	*rn;

	LIST_FOREACH(rn, &redistlist, entry)
		if (bgpd_redistribute(IMSG_NETWORK_ADD, rn->kr, rn->kr6) == -1)
			return (-1);
	return (0);
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
	return (0);
}

int
knexthop_compare(struct knexthop_node *a, struct knexthop_node *b)
{
	u_int32_t	r;

	if (a->nexthop.af != b->nexthop.af)
		return (b->nexthop.af - a->nexthop.af);

	switch (a->nexthop.af) {
	case AF_INET:
		if ((r = b->nexthop.addr32[0] - a->nexthop.addr32[0]) != 0)
			return (r);
		break;
	case AF_INET6:
		if ((r = b->nexthop.addr32[3] - a->nexthop.addr32[3]) != 0)
			return (r);
		if ((r = b->nexthop.addr32[2] - a->nexthop.addr32[2]) != 0)
			return (r);
		if ((r = b->nexthop.addr32[1] - a->nexthop.addr32[1]) != 0)
			return (r);
		if ((r = b->nexthop.addr32[0] - a->nexthop.addr32[0]) != 0)
			return (r);
		break;
	}

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
kroute_find(in_addr_t prefix, u_int8_t prefixlen)
{
	struct kroute_node	s;

	s.r.prefix.s_addr = prefix;
	s.r.prefixlen = prefixlen;

	return (RB_FIND(kroute_tree, &krt, &s));
}

int
kroute_insert(struct kroute_node *kr)
{
	struct knexthop_node	*h;
	in_addr_t		 mask, ina;

	if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
		log_warnx("kroute_tree insert failed for %s/%u",
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
		free(kr);
		return (-1);
	}

	if (kr->r.flags & F_KERNEL) {
		mask = prefixlen2mask(kr->r.prefixlen);
		ina = ntohl(kr->r.prefix.s_addr);
		RB_FOREACH(h, knexthop_tree, &knt)
			if (h->nexthop.af == AF_INET &&
			    (ntohl(h->nexthop.v4.s_addr) & mask) == ina)
				knexthop_validate(h);

		if (kr->r.flags & F_CONNECTED)
			if (kif_kr_insert(kr) == -1)
				return (-1);

		kr_redistribute(IMSG_NETWORK_ADD, &kr->r);
	}
	return (0);
}

int
kroute_remove(struct kroute_node *kr)
{
	struct knexthop_node	*s;

	if (RB_REMOVE(kroute_tree, &krt, kr) == NULL) {
		log_warnx("kroute_remove failed for %s/%u",
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	/* check wether a nexthop depends on this kroute */
	if ((kr->r.flags & F_KERNEL) && (kr->r.flags & F_NEXTHOP))
		RB_FOREACH(s, knexthop_tree, &knt)
			if (s->kroute == kr)
				knexthop_validate(s);

	if (kr->r.flags & F_KERNEL)
		kr_redistribute(IMSG_NETWORK_REMOVE, &kr->r);

	if (kr->r.flags & F_CONNECTED)
		if (kif_kr_remove(kr) == -1) {
			free(kr);
			return (-1);
		}

	free(kr);
	return (0);
}

void
kroute_clear(void)
{
	struct kroute_node	*kr;

	while ((kr = RB_MIN(kroute_tree, &krt)) != NULL)
		kroute_remove(kr);
}

struct kroute6_node *
kroute6_find(const struct in6_addr *prefix, u_int8_t prefixlen)
{
	struct kroute6_node	s;

	memcpy(&s.r.prefix, prefix, sizeof(struct in6_addr));
	s.r.prefixlen = prefixlen;

	return (RB_FIND(kroute6_tree, &krt6, &s));
}

int
kroute6_insert(struct kroute6_node *kr)
{
	struct knexthop_node	*h;
	struct in6_addr		 ina, inb;

	if (RB_INSERT(kroute6_tree, &krt6, kr) != NULL) {
		log_warnx("kroute_tree insert failed for %s/%u",
		    log_in6addr(&kr->r.prefix), kr->r.prefixlen);
		free(kr);
		return (-1);
	}

	if (kr->r.flags & F_KERNEL) {
		inet6applymask(&ina, &kr->r.prefix, kr->r.prefixlen);
		RB_FOREACH(h, knexthop_tree, &knt)
			if (h->nexthop.af == AF_INET6) {
				inet6applymask(&inb, &h->nexthop.v6,
				    kr->r.prefixlen);
				if (memcmp(&ina, &inb, sizeof(ina)) == 0)
					knexthop_validate(h);
			}

		if (kr->r.flags & F_CONNECTED)
			if (kif_kr6_insert(kr) == -1)
				return (-1);

		kr_redistribute6(IMSG_NETWORK_ADD, &kr->r);
	}

	return (0);
}

int
kroute6_remove(struct kroute6_node *kr)
{
	struct knexthop_node	*s;

	if (RB_REMOVE(kroute6_tree, &krt6, kr) == NULL) {
		log_warnx("kroute_remove failed for %s/%u",
		    log_in6addr(&kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	/* check wether a nexthop depends on this kroute */
	if ((kr->r.flags & F_KERNEL) && (kr->r.flags & F_NEXTHOP))
		RB_FOREACH(s, knexthop_tree, &knt)
			if (s->kroute == kr)
				knexthop_validate(s);

	if (kr->r.flags & F_KERNEL)
		kr_redistribute6(IMSG_NETWORK_REMOVE, &kr->r);

	if (kr->r.flags & F_CONNECTED)
		if (kif_kr6_remove(kr) == -1) {
			free(kr);
			return (-1);
		}

	free(kr);
	return (0);
}

void
kroute6_clear(void)
{
	struct kroute6_node	*kr;

	while ((kr = RB_MIN(kroute6_tree, &krt6)) != NULL)
		kroute6_remove(kr);
}

struct knexthop_node *
knexthop_find(struct bgpd_addr *addr)
{
	struct knexthop_node	s;

	memcpy(&s.nexthop, addr, sizeof(s.nexthop));

	return (RB_FIND(knexthop_tree, &knt, &s));
}

int
knexthop_insert(struct knexthop_node *kn)
{
	if (RB_INSERT(knexthop_tree, &knt, kn) != NULL) {
		log_warnx("knexthop_tree insert failed for %s",
			    log_addr(&kn->nexthop));
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
		log_warnx("knexthop_remove failed for %s",
		    log_addr(&kn->nexthop));
		return (-1);
	}

	free(kn);
	return (0);
}

void
knexthop_clear(void)
{
	struct knexthop_node	*kn;

	while ((kn = RB_MIN(knexthop_tree, &knt)) != NULL)
		knexthop_remove(kn);
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
kif_remove(struct kif_node *kif)
{
	struct kif_kr	*kkr;
	struct kif_kr6	*kkr6;

	if (RB_REMOVE(kif_tree, &kit, kif) == NULL) {
		log_warnx("RB_REMOVE(kif_tree, &kit, kif)");
		return (-1);
	}

	while ((kkr = LIST_FIRST(&kif->kroute_l)) != NULL) {
		LIST_REMOVE(kkr, entry);
		kkr->kr->r.flags &= ~F_NEXTHOP;
		kroute_remove(kkr->kr);
		free(kkr);
	}

	while ((kkr6 = LIST_FIRST(&kif->kroute6_l)) != NULL) {
		LIST_REMOVE(kkr6, entry);
		kkr6->kr->r.flags &= ~F_NEXTHOP;
		kroute6_remove(kkr6->kr);
		free(kkr6);
	}

	free(kif);
	return (0);
}

void
kif_clear(void)
{
	struct kif_node	*kif;

	while ((kif = RB_MIN(kif_tree, &kit)) != NULL)
		kif_remove(kif);
}

int
kif_kr_insert(struct kroute_node *kr)
{
	struct kif_node	*kif;
	struct kif_kr	*kkr;

	if ((kif = kif_find(kr->r.ifindex)) == NULL) {
		if (kr->r.ifindex)
			log_warnx("interface with index %u not found",
			    kr->r.ifindex);
		return (0);
	}

	if (kif->k.nh_reachable)
		kr->r.flags &= ~F_DOWN;
	else
		kr->r.flags |= F_DOWN;

	if ((kkr = calloc(1, sizeof(struct kif_kr))) == NULL) {
		log_warn("kif_kr_insert");
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
			log_warnx("interface with index %u not found",
			    kr->r.ifindex);
		return (0);
	}

	for (kkr = LIST_FIRST(&kif->kroute_l); kkr != NULL && kkr->kr != kr;
	    kkr = LIST_NEXT(kkr, entry))
		;	/* nothing */

	if (kkr == NULL) {
		log_warnx("can't remove connected route from interface "
		    "with index %u: not found", kr->r.ifindex);
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
			log_warnx("interface with index %u not found",
			    kr->r.ifindex);
		return (0);
	}

	if (kif->k.nh_reachable)
		kr->r.flags &= ~F_DOWN;
	else
		kr->r.flags |= F_DOWN;

	if ((kkr6 = calloc(1, sizeof(struct kif_kr6))) == NULL) {
		log_warn("kif_kr6_insert");
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
			log_warnx("interface with index %u not found",
			    kr->r.ifindex);
		return (0);
	}

	for (kkr6 = LIST_FIRST(&kif->kroute6_l); kkr6 != NULL && kkr6->kr != kr;
	    kkr6 = LIST_NEXT(kkr6, entry))
		;	/* nothing */

	if (kkr6 == NULL) {
		log_warnx("can't remove connected route from interface "
		    "with index %u: not found", kr->r.ifindex);
		return (-1);
	}

	LIST_REMOVE(kkr6, entry);
	free(kkr6);

	return (0);
}

/*
 * nexthop validation
 */

int
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

int
kroute_validate(struct kroute *kr)
{
	struct kif_node		*kif;

	if (kr->flags & (F_REJECT | F_BLACKHOLE))
		return (0);

	if ((kif = kif_find(kr->ifindex)) == NULL) {
		if (kr->ifindex)
			log_warnx("interface with index %d not found, "
			    "referenced from route for %s/%u",
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
			log_warnx("interface with index %d not found, "
			    "referenced from route for %s/%u",
			    kr->ifindex, log_in6addr(&kr->prefix),
			    kr->prefixlen);
		return (1);
	}

	return (kif->k.nh_reachable);
}

void
knexthop_validate(struct knexthop_node *kn)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;
	struct kroute_nexthop	 n;
	int			 was_valid = 0;

	if (kn->nexthop.af == AF_INET && (kr = kn->kroute) != NULL)
		was_valid = kroute_validate(&kr->r);
	if (kn->nexthop.af == AF_INET6 && (kr6 = kn->kroute) != NULL)
		was_valid = kroute6_validate(&kr6->r);

	bzero(&n, sizeof(n));
	memcpy(&n.nexthop, &kn->nexthop, sizeof(n.nexthop));
	kroute_detach_nexthop(kn);

	switch (kn->nexthop.af) {
	case AF_INET:
		if ((kr = kroute_match(kn->nexthop.v4.s_addr)) == NULL) {
			if (was_valid)
				send_nexthop_update(&n);
		} else {					/* match */
			if (kroute_validate(&kr->r)) {		/* valid */
				n.valid = 1;
				n.connected = kr->r.flags & F_CONNECTED;
				if ((n.gateway.v4.s_addr =
				    kr->r.nexthop.s_addr) != 0)
					n.gateway.af = AF_INET;
				memcpy(&n.kr.kr4, &kr->r, sizeof(n.kr.kr4));
				send_nexthop_update(&n);
			} else					/* down */
				if (was_valid)
					send_nexthop_update(&n);

			kn->kroute = kr;
			kr->r.flags |= F_NEXTHOP;
		}
		break;
	case AF_INET6:
		if ((kr6 = kroute6_match(&kn->nexthop.v6)) == NULL) {
			if (was_valid)
				send_nexthop_update(&n);
		} else {					/* match */
			if (kroute6_validate(&kr6->r)) {	/* valid */
				n.valid = 1;
				n.connected = kr6->r.flags & F_CONNECTED;
				if (memcmp(&kr6->r.nexthop, &in6addr_any,
				    sizeof(struct in6_addr)) != 0) {
					n.gateway.af = AF_INET6;
					memcpy(&n.gateway.v6, &kr6->r.nexthop,
					    sizeof(struct in6_addr));
				}
				memcpy(&n.kr.kr6, &kr6->r, sizeof(n.kr.kr6));
				send_nexthop_update(&n);
			} else					/* down */
				if (was_valid)
					send_nexthop_update(&n);

			kn->kroute = kr6;
			kr6->r.flags |= F_NEXTHOP;
		}
		break;
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
		if ((kr =
		    kroute_find(htonl(ina & prefixlen2mask(i)), i)) != NULL)
			return (kr);

	/* if we don't have a match yet, try to find a default route */
	if ((kr = kroute_find(0, 0)) != NULL)
			return (kr);

	return (NULL);
}

struct kroute6_node *
kroute6_match(struct in6_addr *key)
{
	int			 i;
	struct kroute6_node	*kr6;
	struct in6_addr		 ina;

	/* we will never match the default route */
	for (i = 128; i > 0; i--) {
		inet6applymask(&ina, key, i);
		if ((kr6 = kroute6_find(&ina, i)) != NULL)
			return (kr6);
	}

	/* if we don't have a match yet, try to find a default route */
	if ((kr6 = kroute6_find(&in6addr_any, 0)) != NULL)
			return (kr6);

	return (NULL);
}

void
kroute_detach_nexthop(struct knexthop_node *kn)
{
	struct knexthop_node	*s;
	struct kroute_node	*k;
	struct kroute6_node	*k6;

	/*
	 * check wether there's another nexthop depending on this kroute
	 * if not remove the flag
	 */

	if (kn->kroute == NULL)
		return;

	for (s = RB_MIN(knexthop_tree, &knt); s != NULL &&
	    s->kroute != kn->kroute; s = RB_NEXT(knexthop_tree, &knt, s))
		;	/* nothing */

	if (s == NULL) {
		switch (kn->nexthop.af) {
		case AF_INET:
			k = kn->kroute;
			k->r.flags &= ~F_NEXTHOP;
			break;
		case AF_INET6:
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
protect_lo(void)
{
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;

	/* special protection for 127/8 */
	if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
		log_warn("protect_lo");
		return (-1);
	}
	kr->r.prefix.s_addr = htonl(INADDR_LOOPBACK);
	kr->r.prefixlen = 8;
	kr->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute_tree, &krt, kr) != NULL)
		free(kr);	/* kernel route already there, no problem */

	/* special protection for loopback */
	if ((kr6 = calloc(1, sizeof(struct kroute6_node))) == NULL) {
		log_warn("protect_lo");
		return (-1);
	}
	memcpy(&kr6->r.prefix, &in6addr_loopback, sizeof(kr6->r.prefix));
	kr6->r.prefixlen = 128;
	kr->r.flags = F_KERNEL|F_CONNECTED;

	if (RB_INSERT(kroute6_tree, &krt6, kr6) != NULL)
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
	u_int8_t	 l = 0, i, len;

	/*
	 * sin6_len is the size of the sockaddr so substract the offset of
	 * the possibly truncated sin6_addr struct.
	 */
	len = sa_in6->sin6_len -
	    (u_int8_t)(&((struct sockaddr_in6 *)NULL)->sin6_addr);
	for (i = 0; i < len; i++) {
		/* this "beauty" is adopted from sbin/route/show.c ... */
		switch (sa_in6->sin6_addr.s6_addr[i]) {
		case 0xff:
			l += 8;
			break;
		case 0xfe:
			l += 7;
			return (l);
		case 0xfc:
			l += 6;
			return (l);
		case 0xf8:
			l += 5;
			return (l);
		case 0xf0:
			l += 4;
			return (l);
		case 0xe0:
			l += 3;
			return (l);
		case 0xc0:
			l += 2;
			return (l);
		case 0x80:
			l += 1;
			return (l);
		case 0x00:
			return (l);
		default:
			fatalx("non continguous inet6 netmask");
		}
	}

	return (l);
}

in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (0xffffffff << (32 - prefixlen));
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

void
inet6applymask(struct in6_addr *dest, const struct in6_addr *src, int prefixlen)
{
	struct in6_addr	mask;
	int		i;

	bzero(&mask, sizeof(mask));
	for (i = 0; i < prefixlen / 8; i++)
		mask.s6_addr[i] = 0xff;
	i = prefixlen % 8;
	if (i)
		mask.s6_addr[prefixlen / 8] = 0xff00 >> i;

	for (i = 0; i < 16; i++)
		dest->s6_addr[i] = src->s6_addr[i] & mask.s6_addr[i];
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
if_change(u_short ifindex, int flags, struct if_data *ifd)
{
	struct kif_node		*kif;
	struct kif_kr		*kkr;
	struct kif_kr6		*kkr6;
	struct kroute_nexthop	 nh;
	struct knexthop_node	*n;
	u_int8_t		 reachable;

	if ((kif = kif_find(ifindex)) == NULL) {
		log_warnx("interface with index %u not found",
		    ifindex);
		return;
	}

	kif->k.flags = flags;
	kif->k.link_state = ifd->ifi_link_state;
	kif->k.media_type = ifd->ifi_type;
	kif->k.baudrate = ifd->ifi_baudrate;

	send_imsg_session(IMSG_IFINFO, 0, &kif->k, sizeof(kif->k));

	if ((reachable = kif_validate(&kif->k)) == kif->k.nh_reachable)
		return;		/* nothing changed wrt nexthop validity */

	kif->k.nh_reachable = reachable;

	LIST_FOREACH(kkr, &kif->kroute_l, entry) {
		if (reachable)
			kkr->kr->r.flags &= ~F_DOWN;
		else
			kkr->kr->r.flags |= F_DOWN;

		RB_FOREACH(n, knexthop_tree, &knt)
			if (n->kroute == kkr->kr) {
				bzero(&nh, sizeof(nh));
				memcpy(&nh.nexthop, &n->nexthop,
				    sizeof(nh.nexthop));
				if (kroute_validate(&kkr->kr->r)) {
					nh.valid = 1;
					nh.connected = 1;
					if ((nh.gateway.v4.s_addr =
					    kkr->kr->r.nexthop.s_addr) != 0)
						nh.gateway.af = AF_INET;
				}
				memcpy(&nh.kr.kr4, &kkr->kr->r,
				    sizeof(nh.kr.kr4));
				send_nexthop_update(&nh);
			}
	}
	LIST_FOREACH(kkr6, &kif->kroute6_l, entry) {
		if (reachable)
			kkr6->kr->r.flags &= ~F_DOWN;
		else
			kkr6->kr->r.flags |= F_DOWN;

		RB_FOREACH(n, knexthop_tree, &knt)
			if (n->kroute == kkr6->kr) {
				bzero(&nh, sizeof(nh));
				memcpy(&nh.nexthop, &n->nexthop,
				    sizeof(nh.nexthop));
				if (kroute6_validate(&kkr6->kr->r)) {
					nh.valid = 1;
					nh.connected = 1;
					if (memcmp(&kkr6->kr->r.nexthop,
					    &in6addr_any, sizeof(struct
					    in6_addr))) {
						nh.gateway.af = AF_INET6;
						memcpy(&nh.gateway.v6,
						    &kkr6->kr->r.nexthop,
						    sizeof(struct in6_addr));
					}
				}
				memcpy(&nh.kr.kr6, &kkr6->kr->r,
				    sizeof(nh.kr.kr6));
				send_nexthop_update(&nh);
			}
	}
}

void
if_announce(void *msg)
{
	struct if_announcemsghdr	*ifan;
	struct kif_node			*kif;

	ifan = msg;

	switch (ifan->ifan_what) {
	case IFAN_ARRIVAL:
		if ((kif = calloc(1, sizeof(struct kif_node))) == NULL) {
			log_warn("if_announce");
			return;
		}

		kif->k.ifindex = ifan->ifan_index;
		strlcpy(kif->k.ifname, ifan->ifan_name, sizeof(kif->k.ifname));
		kif_insert(kif);
		break;
	case IFAN_DEPARTURE:
		kif = kif_find(ifan->ifan_index);
		kif_remove(kif);
		break;
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
		struct sockaddr_rtlabel	label;
	} r;

	if (kr_state.fib_sync == 0)
		return (0);

	bzero(&r, sizeof(r));
	r.hdr.rtm_msglen = sizeof(r);
	r.hdr.rtm_version = RTM_VERSION;
	r.hdr.rtm_type = action;
	r.hdr.rtm_flags = RTF_PROTO1;
	if (kroute->flags & F_BLACKHOLE)
		r.hdr.rtm_flags |= RTF_BLACKHOLE;
	if (kroute->flags & F_REJECT)
		r.hdr.rtm_flags |= RTF_REJECT;
	r.hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	r.hdr.rtm_addrs = RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_LABEL;
	r.prefix.sin_len = sizeof(r.prefix);
	r.prefix.sin_family = AF_INET;
	r.prefix.sin_addr.s_addr = kroute->prefix.s_addr;

	r.nexthop.sin_len = sizeof(r.nexthop);
	r.nexthop.sin_family = AF_INET;
	r.nexthop.sin_addr.s_addr = kroute->nexthop.s_addr;
	if (kroute->nexthop.s_addr != 0)
		r.hdr.rtm_flags |= RTF_GATEWAY;

	r.mask.sin_len = sizeof(r.mask);
	r.mask.sin_family = AF_INET;
	r.mask.sin_addr.s_addr = htonl(prefixlen2mask(kroute->prefixlen));

	r.label.sr_len = sizeof(r.label);
	strlcpy(r.label.sr_label, rtlabel_id2name(kroute->labelid),
	    sizeof(r.label.sr_label));

retry:
	if (write(fd, &r, sizeof(r)) == -1) {
		switch (errno) {
		case ESRCH:
			if (r.hdr.rtm_type == RTM_CHANGE) {
				r.hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (r.hdr.rtm_type == RTM_DELETE) {
				log_info("route %s/%u vanished before delete",
				    inet_ntoa(kroute->prefix),
				    kroute->prefixlen);
				return (0);
			} else {
				log_warnx("send_rtmsg: action %u, "
				    "prefix %s/%u: %s", r.hdr.rtm_type,
				    inet_ntoa(kroute->prefix),
				    kroute->prefixlen, strerror(errno));
				return (0);
			}
			break;
		default:
			log_warnx("send_rtmsg: action %u, prefix %s/%u: %s",
			    r.hdr.rtm_type, inet_ntoa(kroute->prefix),
			    kroute->prefixlen, strerror(errno));
			return (0);
		}
	}

	return (0);
}

int
send_rt6msg(int fd, int action, struct kroute6 *kroute)
{
	struct {
		struct rt_msghdr	hdr;
		struct sockaddr_in6	prefix;
		struct sockaddr_in6	nexthop;
		struct sockaddr_in6	mask;
		struct sockaddr_rtlabel	label;
	} r;

	if (kr_state.fib_sync == 0)
		return (0);

	bzero(&r, sizeof(r));
	r.hdr.rtm_msglen = sizeof(r);
	r.hdr.rtm_version = RTM_VERSION;
	r.hdr.rtm_type = action;
	r.hdr.rtm_flags = RTF_PROTO1;
	if (kroute->flags & F_BLACKHOLE)
		r.hdr.rtm_flags |= RTF_BLACKHOLE;
	if (kroute->flags & F_REJECT)
		r.hdr.rtm_flags |= RTF_REJECT;
	r.hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	r.hdr.rtm_addrs = RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_LABEL;
	r.prefix.sin6_len = sizeof(r.prefix);
	r.prefix.sin6_family = AF_INET6;
	memcpy(&r.prefix.sin6_addr, &kroute->prefix, sizeof(struct in6_addr));
	/* XXX scope does not matter or? */

	r.nexthop.sin6_len = sizeof(r.nexthop);
	r.nexthop.sin6_family = AF_INET6;
	memcpy(&r.nexthop.sin6_addr, &kroute->nexthop, sizeof(struct in6_addr));
	if (memcmp(&kroute->nexthop, &in6addr_any, sizeof(struct in6_addr)))
		r.hdr.rtm_flags |= RTF_GATEWAY;

	r.mask.sin6_len = sizeof(r.mask);
	r.mask.sin6_family = AF_INET6;
	memcpy(&r.mask.sin6_addr, prefixlen2mask6(kroute->prefixlen),
	    sizeof(struct in6_addr));

	r.label.sr_len = sizeof(r.label);
	strlcpy(r.label.sr_label, rtlabel_id2name(kroute->labelid),
	    sizeof(r.label.sr_label));

retry:
	if (write(fd, &r, sizeof(r)) == -1) {
		switch (errno) {
		case ESRCH:
			if (r.hdr.rtm_type == RTM_CHANGE) {
				r.hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (r.hdr.rtm_type == RTM_DELETE) {
				log_info("route %s/%u vanished before delete",
				    log_in6addr(&kroute->prefix),
				    kroute->prefixlen);
				return (0);
			} else {
				log_warnx("send_rtmsg: action %u, "
				    "prefix %s/%u: %s", r.hdr.rtm_type,
				    log_in6addr(&kroute->prefix),
				    kroute->prefixlen, strerror(errno));
				return (0);
			}
			break;
		default:
			log_warnx("send_rtmsg: action %u, prefix %s/%u: %s",
			    r.hdr.rtm_type, log_in6addr(&kroute->prefix),
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
	struct sockaddr		*sa, *gw, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct kroute_node	*kr = NULL;
	struct kroute6_node	*kr6 = NULL;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("fetchtable");
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		free(buf);
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

#ifdef RTF_MPATH
		if (rtm->rtm_flags & RTF_MPATH)		/* multipath */
			continue;
#endif
		switch (sa->sa_family) {
		case AF_INET:
			if ((kr = calloc(1, sizeof(struct kroute_node))) ==
			    NULL) {
				log_warn("fetchtable");
				free(buf);
				return (-1);
			}

			kr->r.flags = F_KERNEL;
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
			break;
		case AF_INET6:
			if ((kr6 = calloc(1, sizeof(struct kroute6_node))) ==
			    NULL) {
				log_warn("fetchtable");
				free(buf);
				return (-1);
			}

			kr6->r.flags = F_KERNEL;
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
			break;
		default:
			continue;
		}

		if ((gw = rti_info[RTAX_GATEWAY]) != NULL)
			switch (gw->sa_family) {
			case AF_INET:
				if (kr == NULL)
					fatalx("v4 gateway for !v4 dst?!");
				kr->r.nexthop.s_addr =
				    ((struct sockaddr_in *)gw)->sin_addr.s_addr;
				break;
			case AF_INET6:
				if (kr6 == NULL)
					fatalx("v6 gateway for !v6 dst?!");
				memcpy(&kr6->r.nexthop,
				    &((struct sockaddr_in6 *)gw)->sin6_addr,
				    sizeof(kr6->r.nexthop));
				break;
			case AF_LINK:
				if (sa->sa_family == AF_INET)
					kr->r.flags |= F_CONNECTED;
				else if (sa->sa_family == AF_INET6)
					kr6->r.flags |= F_CONNECTED;
				break;
			}

		if (sa->sa_family == AF_INET)
			kroute_insert(kr);
		else if (sa->sa_family == AF_INET6)
			kroute6_insert(kr6);

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
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_IFLIST;
	mib[5] = ifindex;

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("fetchif");
		return (-1);
	}
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		free(buf);
		return (-1);
	}

	lim = buf + len;
	for (next = buf; next < lim; next += ifm.ifm_msglen) {
		memcpy(&ifm, next, sizeof(ifm));
		if (ifm.ifm_type != RTM_IFINFO)
			continue;

		sa = (struct sockaddr *)(next + sizeof(ifm));
		get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

		if ((kif = calloc(1, sizeof(struct kif_node))) == NULL) {
			log_warn("fetchifs");
			free(buf);
			return (-1);
		}

		kif->k.ifindex = ifm.ifm_index;
		kif->k.flags = ifm.ifm_flags;
		kif->k.link_state = ifm.ifm_data.ifi_link_state;
		kif->k.media_type = ifm.ifm_data.ifi_type;
		kif->k.baudrate = ifm.ifm_data.ifi_baudrate;
		kif->k.nh_reachable = kif_validate(&kif->k);

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
dispatch_rtmsg(void)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;
	char			*next, *lim;
	struct rt_msghdr	*rtm;
	struct if_msghdr	 ifm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];

	if ((n = read(kr_state.fd, &buf, sizeof(buf))) == -1) {
		log_warn("dispatch_rtmsg: read error");
		return (-1);
	}

	if (n == 0) {
		log_warnx("routing socket closed");
		return (-1);
	}

	lim = buf + n;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		if (rtm->rtm_pid == kr_state.pid)	/* cause by us */
			continue;

		if (rtm->rtm_errno)			/* failed attempts... */
			continue;

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
		case RTM_DELETE:
			if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
				continue;
			if (dispatch_rtmsg_addr(rtm, rti_info) == -1)
				return (-1);
			break;
		case RTM_IFINFO:
			memcpy(&ifm, next, sizeof(ifm));
			if_change(ifm.ifm_index, ifm.ifm_flags,
			    &ifm.ifm_data);
			break;
		case RTM_IFANNOUNCE:
			if_announce(next);
			break;
		default:
			/* ignore for now */
			break;
		}
	}
	return (0);
}

int
dispatch_rtmsg_addr(struct rt_msghdr *rtm, struct sockaddr *rti_info[RTAX_MAX])
{
	struct sockaddr		*sa;
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct kroute_node	*kr;
	struct kroute6_node	*kr6;
	struct bgpd_addr	 prefix;
	int			 flags, oflags;
	u_int16_t		 ifindex;
	u_int8_t		 prefixlen;

	flags = F_KERNEL;
	ifindex = 0;
	prefixlen = 0;
	bzero(&prefix, sizeof(prefix));

	if ((sa = rti_info[RTAX_DST]) == NULL)
		return (-1);

	if (rtm->rtm_flags & RTF_STATIC)
		flags |= F_STATIC;
	if (rtm->rtm_flags & RTF_BLACKHOLE)
		flags |= F_BLACKHOLE;
	if (rtm->rtm_flags & RTF_REJECT)
		flags |= F_REJECT;
	if (rtm->rtm_flags & RTF_DYNAMIC)
		flags |= F_DYNAMIC;

	prefix.af = sa->sa_family;
	switch (prefix.af) {
	case AF_INET:
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

	if (rtm->rtm_type == RTM_DELETE) {
		switch (prefix.af) {
		case AF_INET:
			if ((kr = kroute_find(prefix.v4.s_addr,
			    prefixlen)) == NULL)
				return (0);
			if (!(kr->r.flags & F_KERNEL))
				return (0);
			if (kroute_remove(kr) == -1)
				return (-1);
			break;
		case AF_INET6:
			if ((kr6 = kroute6_find(&prefix.v6, prefixlen)) == NULL)
				return (0);
			if (!(kr6->r.flags & F_KERNEL))
				return (0);
			if (kroute6_remove(kr6) == -1)
				return (-1);
			break;
		}
		return (0);
	}

	if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
		switch (sa->sa_family) {
		case AF_LINK:
			flags |= F_CONNECTED;
			ifindex = rtm->rtm_index;
			sa = NULL;
			break;
		}

	if (sa == NULL && !(flags & F_CONNECTED)) {
		log_warnx("dispatch_rtmsg no nexthop for %s/%u",
		    log_addr(&prefix), prefixlen);
		return (0);
	}

	switch (prefix.af) {
	case AF_INET:
		sa_in = (struct sockaddr_in *)sa;
		if ((kr = kroute_find(prefix.v4.s_addr, prefixlen)) != NULL) {
			if (kr->r.flags & F_KERNEL) {
				if (sa_in != NULL)
					kr->r.nexthop.s_addr =
					    sa_in->sin_addr.s_addr;
				else
					kr->r.nexthop.s_addr = 0;

				if (kr->r.flags & F_NEXTHOP)
					flags |= F_NEXTHOP;
				oflags = kr->r.flags;
				kr->r.flags = flags;
				if ((oflags & F_CONNECTED) &&
				    !(flags & F_CONNECTED)) {
					kif_kr_remove(kr);
					kr_redistribute(IMSG_NETWORK_REMOVE,
					    &kr->r);
				}
				if ((flags & F_CONNECTED) &&
				    !(oflags & F_CONNECTED)) {
					kif_kr_insert(kr);
					kr_redistribute(IMSG_NETWORK_ADD,
					    &kr->r);
				}
			}
		} else if (rtm->rtm_type == RTM_CHANGE) {
			log_warnx("change req for %s/%u: not in table",
			    log_addr(&prefix), prefixlen);
			return (0);
		} else {
			if ((kr = calloc(1,
			    sizeof(struct kroute_node))) == NULL) {
				log_warn("dispatch_rtmsg");
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

			kroute_insert(kr);
		}
		break;
	case AF_INET6:
		sa_in6 = (struct sockaddr_in6 *)sa;
		if ((kr6 = kroute6_find(&prefix.v6, prefixlen)) != NULL) {
			if (kr6->r.flags & F_KERNEL) {
				if (sa_in6 != NULL)
					memcpy(&kr6->r.nexthop,
					    &sa_in6->sin6_addr,
					    sizeof(struct in6_addr));
				else
					memcpy(&kr6->r.nexthop,
					    &in6addr_any,
					    sizeof(struct in6_addr));

				if (kr6->r.flags & F_NEXTHOP)
					flags |= F_NEXTHOP;
				oflags = kr6->r.flags;
				kr6->r.flags = flags;
				if ((oflags & F_CONNECTED) &&
				    !(flags & F_CONNECTED)) {
					kif_kr6_remove(kr6);
					kr_redistribute6(IMSG_NETWORK_REMOVE,
					    &kr6->r);
				}
				if ((flags & F_CONNECTED) &&
				    !(oflags & F_CONNECTED)) {
					kif_kr6_insert(kr6);
					kr_redistribute6(IMSG_NETWORK_ADD,
					    &kr6->r);
				}
			}
		} else if (rtm->rtm_type == RTM_CHANGE) {
			log_warnx("change req for %s/%u: not in table",
			    log_addr(&prefix), prefixlen);
			return (0);
		} else {
			if ((kr6 = calloc(1,
			    sizeof(struct kroute6_node))) == NULL) {
				log_warn("dispatch_rtmsg");
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

			kroute6_insert(kr6);
		}
		break;
	}

	return (0);
}

