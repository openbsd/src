/*	$OpenBSD: kroute.c,v 1.94 2004/04/28 01:13:18 deraadt Exp $ */

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

struct knexthop_node {
	RB_ENTRY(knexthop_node)	 entry;
	struct bgpd_addr	 nexthop;
	struct kroute_node	*kroute;
};

struct kif_kr {
	LIST_ENTRY(kif_kr)	 entry;
	struct kroute_node	*kr;
};

LIST_HEAD(kif_kr_head, kif_kr);

struct kif_node {
	RB_ENTRY(kif_node)	 entry;
	struct kif		 k;
	struct kif_kr_head	 kroute_l;
};

int	kroute_compare(struct kroute_node *, struct kroute_node *);
int	knexthop_compare(struct knexthop_node *, struct knexthop_node *);
int	kif_compare(struct kif_node *, struct kif_node *);

struct kroute_node	*kroute_find(in_addr_t, u_int8_t);
int			 kroute_insert(struct kroute_node *);
int			 kroute_remove(struct kroute_node *);
void			 kroute_clear(void);

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

void			 knexthop_validate(struct knexthop_node *);
struct kroute_node	*kroute_match(in_addr_t);
void			 kroute_attach_nexthop(struct knexthop_node *,
			    struct kroute_node *);
void			 kroute_detach_nexthop(struct knexthop_node *);

int		protect_lo(void);
u_int8_t	prefixlen_classful(in_addr_t);
u_int8_t	mask2prefixlen(in_addr_t);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		if_change(u_short, int, struct if_data *);
void		if_announce(void *);

int		send_rtmsg(int, int, struct kroute *);
int		dispatch_rtmsg(void);
int		fetchtable(void);
int		fetchifs(int);

RB_HEAD(kroute_tree, kroute_node)	kroute_tree, krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare)
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare)

RB_HEAD(knexthop_tree, knexthop_node)	knexthop_tree, knt;
RB_PROTOTYPE(knexthop_tree, knexthop_node, entry, knexthop_compare)
RB_GENERATE(knexthop_tree, knexthop_node, entry, knexthop_compare)

RB_HEAD(kif_tree, kif_node)		kif_tree, kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare)
RB_GENERATE(kif_tree, kif_node, entry, kif_compare)

/*
 * exported functions
 */

int
kr_init(int fs)
{
	int opt;

	kr_state.fib_sync = fs;

	if ((kr_state.fd = socket(AF_ROUTE, SOCK_RAW, 0)) == -1) {
		log_warn("kr_init: socket");
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_warn("kr_init: setsockopt");	/* not fatal */

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

	if ((kr = kroute_find(kroute->prefix.s_addr, kroute->prefixlen)) !=
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
			log_warn("kr_change");
			return (-1);
		}
		kr->r.prefix.s_addr = kroute->prefix.s_addr;
		kr->r.prefixlen = kroute->prefixlen;
		kr->r.nexthop.s_addr = kroute->nexthop.s_addr;
		kr->r.flags = F_BGPD_INSERTED;

		if (kroute_insert(kr) == -1)
			free(kr);
	} else
		kr->r.nexthop.s_addr = kroute->nexthop.s_addr;

	return (0);
}

int
kr_delete(struct kroute *kroute)
{
	struct kroute_node	*kr;

	if ((kr = kroute_find(kroute->prefix.s_addr, kroute->prefixlen)) ==
	    NULL)
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
	knexthop_clear();
	kroute_clear();
	kif_clear();
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

	log_info("kernel routing table coupled");
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
		/* should not happen... this is really an error path */
		struct kroute_nexthop	 nh;

		bzero(&nh, sizeof(nh));
		memcpy(&nh.nexthop, addr, sizeof(nh.nexthop));
		if (h->kroute != NULL) {
			nh.valid = 1;
			nh.connected = h->kroute->r.flags & F_CONNECTED;
			if (h->kroute->r.nexthop.s_addr != 0) {
				nh.gateway.af = AF_INET;
				nh.gateway.v4.s_addr =
				    h->kroute->r.nexthop.s_addr;
			}
			memcpy(&nh.kr, &h->kroute->r, sizeof(nh.kr));
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
	struct bgpd_addr	*addr;
	int			 flags;
	struct ctl_show_nexthop	 snh;
	struct knexthop_node	*h;
	struct kif_node		*kif;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(flags)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		memcpy(&flags, imsg->data, sizeof(flags));
		RB_FOREACH(kr, kroute_tree, &krt)
			if (!flags || kr->r.flags & flags)
				send_imsg_session(IMSG_CTL_KROUTE,
				    imsg->hdr.pid, &kr->r, sizeof(kr->r));
		break;
	case IMSG_CTL_KROUTE_ADDR:
		if (imsg->hdr.len != IMSG_HEADER_SIZE +
		    sizeof(struct bgpd_addr)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		addr = imsg->data;
		kr = NULL;
		if (addr->af == AF_INET)
			kr = kroute_match(addr->v4.s_addr);
		if (kr != NULL)
			send_imsg_session(IMSG_CTL_KROUTE, imsg->hdr.pid,
			    &kr->r, sizeof(kr->r));
		break;
	case IMSG_CTL_SHOW_NEXTHOP:
		RB_FOREACH(h, knexthop_tree, &knt) {
			bzero(&snh, sizeof(snh));
			memcpy(&snh.addr, &h->nexthop, sizeof(snh.addr));
			if (h->kroute != NULL)
				if (!(h->kroute->r.flags & F_DOWN))
					snh.valid = 1;
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
		if (!(kr->r.flags & F_CONNECTED))
			kr->r.flags |= F_STATIC;

		mask = 0xffffffff << (32 - kr->r.prefixlen);
		ina = ntohl(kr->r.prefix.s_addr);
		RB_FOREACH(h, knexthop_tree, &knt)
			if (h->nexthop.af == AF_INET &&
			    (ntohl(h->nexthop.v4.s_addr) & mask) == ina)
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
		log_warnx("kroute_remove failed for %s/%u",
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
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

void
kroute_clear(void)
{
	struct kroute_node	*kr;

	while ((kr = RB_MIN(kroute_tree, &krt)) != NULL)
		kroute_remove(kr);
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
	memcpy(&n.nexthop, &kn->nexthop, sizeof(n.nexthop));
	kroute_detach_nexthop(kn);

	switch (kn->nexthop.af) {
	case AF_INET:
		if ((kr = kroute_match(kn->nexthop.v4.s_addr)) == NULL) {
			if (was_valid)
				send_nexthop_update(&n);
		} else {					/* match */
			if (kr->r.flags & F_DOWN) {		/* is down */
				if (was_valid)
					send_nexthop_update(&n);
			} else {				/* valid */
				if (!was_valid) {
					n.valid = 1;
					n.connected = kr->r.flags & F_CONNECTED;
					if ((n.gateway.v4.s_addr =
					    kr->r.nexthop.s_addr) != 0)
						n.gateway.af = AF_INET;
					memcpy(&n.kr, &kr->r, sizeof(n.kr));
					send_nexthop_update(&n);
				}
			}
			kroute_attach_nexthop(kn, kr);
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
		log_warn("protect_lo");
		return (-1);
	}
	kr->r.prefix.s_addr = inet_addr("127.0.0.1");
	kr->r.prefixlen = 8;
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
if_change(u_short ifindex, int flags, struct if_data *ifd)
{
	struct kif_node		*kif;
	struct kif_kr		*kkr;
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

	if ((reachable = (flags & IFF_UP) &&
	    (ifd->ifi_link_state != LINK_STATE_DOWN)) == kif->k.nh_reachable)
		return;		/* nothing changed wrt nexthop validity */

	kif->k.nh_reachable = reachable;

	LIST_FOREACH(kkr, &kif->kroute_l, entry) {
		/*
		 * we treat link_state == LINK_STATE_UNKNOWN as valid
		 * not all interfaces have a conecpt of "link state" and/or
		 * do not report up
		 */
		if (reachable)
			kkr->kr->r.flags &= ~F_DOWN;
		else
			kkr->kr->r.flags |= F_DOWN;

		RB_FOREACH(n, knexthop_tree, &knt)
			if (n->kroute == kkr->kr) {
				bzero(&nh, sizeof(nh));
				memcpy(&nh.nexthop, &n->nexthop,
				    sizeof(nh.nexthop));
				if (!(kkr->kr->r.flags & F_DOWN)) {
					nh.valid = 1;
					nh.connected = 1;
					if ((nh.gateway.v4.s_addr =
					    kkr->kr->r.nexthop.s_addr) != 0)
						nh.gateway.af = AF_INET;
				}
				memcpy(&nh.kr, &kkr->kr->r, sizeof(nh.kr));
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
	} r;

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
	r.prefix.sin_addr.s_addr = kroute->prefix.s_addr;
	r.nexthop.sin_len = sizeof(r.nexthop);
	r.nexthop.sin_family = AF_INET;
	r.nexthop.sin_addr.s_addr = kroute->nexthop.s_addr;
	r.mask.sin_len = sizeof(r.mask);
	r.mask.sin_family = AF_INET;
	r.mask.sin_addr.s_addr = htonl(0xffffffff << (32 - kroute->prefixlen));

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

		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
			log_warn("fetchtable");
			free(buf);
			return (-1);
		}

		kr->r.flags = F_KERNEL;

		switch (sa->sa_family) {
		case AF_INET:
			kr->r.prefix.s_addr =
			    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
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
		default:
			free(kr);
			continue;
			/* not reached */
		}

		if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
			switch (sa->sa_family) {
			case AF_INET:
				kr->r.nexthop.s_addr =
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
		sa = (struct sockaddr *)(next + sizeof(ifm));
		get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

		if (ifm.ifm_type != RTM_IFINFO)
			continue;

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
		kif->k.nh_reachable = (kif->k.flags & IFF_UP) &&
		    (ifm.ifm_data.ifi_link_state != LINK_STATE_DOWN);

		if ((sa = rti_info[RTAX_IFP]) != NULL)
			if (sa->sa_family == AF_LINK) {
				sdl = (struct sockaddr_dl *)sa;
				if (sdl->sdl_nlen > 0)
					strlcpy(kif->k.ifname, sdl->sdl_data,
					    sizeof(kif->k.ifname));
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
	struct sockaddr_in	*sa_in;
	struct kroute_node	*kr;
	struct in_addr		 prefix, nexthop;
	u_int8_t		 prefixlen;
	int			 flags;
	u_short			 ifindex;

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

		prefix.s_addr = 0;
		prefixlen = 0;
		flags = F_KERNEL;
		nexthop.s_addr = 0;
		ifindex = 0;

		if (rtm->rtm_pid == kr_state.pid)	/* cause by us */
			continue;

		if (rtm->rtm_errno)			/* failed attempts... */
			continue;

		if (rtm->rtm_type == RTM_ADD || rtm->rtm_type == RTM_CHANGE ||
		    rtm->rtm_type == RTM_DELETE) {
			if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
				continue;
			switch (sa->sa_family) {
			case AF_INET:
				prefix.s_addr =
				    ((struct sockaddr_in *)sa)->sin_addr.s_addr;
				sa_in = (struct sockaddr_in *)
				    rti_info[RTAX_NETMASK];
				if (sa_in != NULL) {
					if (sa_in->sin_len != 0)
						prefixlen = mask2prefixlen(
						    sa_in->sin_addr.s_addr);
				} else if (rtm->rtm_flags & RTF_HOST)
					prefixlen = 32;
				else
					prefixlen =
					    prefixlen_classful(prefix.s_addr);
				break;
			default:
				continue;
				/* not reached */
			}
		}

		if ((sa = rti_info[RTAX_GATEWAY]) != NULL)
			switch (sa->sa_family) {
			case AF_INET:
				nexthop.s_addr =
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
			if (nexthop.s_addr == 0 && !(flags & F_CONNECTED)) {
				log_warnx("dispatch_rtmsg no nexthop for %s/%u",
				    inet_ntoa(prefix), prefixlen);
				continue;
			}

			if ((kr = kroute_find(prefix.s_addr, prefixlen)) !=
			    NULL) {
				if (kr->r.flags & F_KERNEL) {
					kr->r.nexthop.s_addr = nexthop.s_addr;
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
			} else if (rtm->rtm_type == RTM_CHANGE) {
				log_warnx("change req for %s/%u: not "
				    "in table", inet_ntoa(prefix),
				    prefixlen);
				continue;
			} else {
				if ((kr = calloc(1,
				    sizeof(struct kroute_node))) == NULL) {
					log_warn("dispatch_rtmsg");
					return (-1);
				}
				kr->r.prefix.s_addr = prefix.s_addr;
				kr->r.prefixlen = prefixlen;
				kr->r.nexthop.s_addr = nexthop.s_addr;
				kr->r.flags = flags;
				kr->r.ifindex = ifindex;

				kroute_insert(kr);
			}
			break;
		case RTM_DELETE:
			if ((kr = kroute_find(prefix.s_addr, prefixlen)) ==
			    NULL)
				continue;
			if (!(kr->r.flags & F_KERNEL))
				continue;
			if (kroute_remove(kr) == -1)
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
