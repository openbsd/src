/*	$OpenBSD: kroute.c,v 1.36 2015/02/10 08:25:51 claudio Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
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
#include <limits.h>

#include "ldpd.h"
#include "log.h"

struct {
	u_int32_t		rtseq;
	pid_t			pid;
	int			fib_sync;
	int			fd;
	struct event		ev;
} kr_state;

struct kroute_node {
	RB_ENTRY(kroute_node)	 entry;
	struct kroute		 r;
	struct kroute_node	*next;
};

struct kif_addr {
	TAILQ_ENTRY(kif_addr)	 entry;
	struct kaddr		 addr;
};

struct kif_node {
	RB_ENTRY(kif_node)	 entry;
	TAILQ_HEAD(, kif_addr)	 addrs;
	struct kif		 k;
};

void	kr_redist_remove(struct kroute *);
int	kr_redist_eval(struct kroute *);
void	kr_redistribute(struct kroute_node *);
int	kroute_compare(struct kroute_node *, struct kroute_node *);
int	kif_compare(struct kif_node *, struct kif_node *);

struct kroute_node	*kroute_find_fec(in_addr_t, u_int8_t, struct in_addr);
struct kroute_node	*kroute_find_any(in_addr_t, u_int8_t);
struct kroute_node	*kroute_matchprio(struct kroute_node *, u_int8_t);
int			 kroute_insert(struct kroute_node *);
int			 kroute_remove(struct kroute_node *);
void			 kroute_clear(void);

struct kif_node		*kif_find(u_short);
struct kif_node		*kif_insert(u_short);
int			 kif_remove(struct kif_node *);
void			 kif_clear(void);
struct kif_node		*kif_update(u_short, int, struct if_data *,
			    struct sockaddr_dl *, int *);

struct kroute_node	*kroute_match(in_addr_t);

int		protect_lo(void);
u_int8_t	prefixlen_classful(in_addr_t);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		if_change(u_short, int, struct if_data *, struct sockaddr_dl *);
void		if_newaddr(u_short, struct sockaddr_in *, struct sockaddr_in *,
		    struct sockaddr_in *);
void		if_deladdr(u_short, struct sockaddr_in *, struct sockaddr_in *,
		    struct sockaddr_in *);
void		if_announce(void *);

int		send_rtmsg(int, int, struct kroute *, u_int32_t);
int		dispatch_rtmsg(void);
int		fetchtable(void);
int		fetchifs(u_short);
int		rtmsg_process(char *, size_t);

RB_HEAD(kroute_tree, kroute_node)	krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare)
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare)

RB_HEAD(kif_tree, kif_node)		kit;
RB_PROTOTYPE(kif_tree, kif_node, entry, kif_compare)
RB_GENERATE(kif_tree, kif_node, entry, kif_compare)

int		flag_implicit_null = 0;

int
kif_init(void)
{
	RB_INIT(&kit);
	/* init also krt tree so that we can call kr_shutdown() */
	RB_INIT(&krt);
	kr_state.fib_sync = 0;	/* decoupled */

	if (fetchifs(0) == -1)
		return (-1);

	return (0);
}

void
kif_redistribute(void)
{
	struct kif_node		*kif;
	struct kif_addr		*ka;

	RB_FOREACH(kif, kif_tree, &kit) {
		main_imsg_compose_ldpe(IMSG_IFUP, 0, &kif->k,
		    sizeof(struct kif));

		TAILQ_FOREACH(ka, &kif->addrs, entry)
			main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka->addr,
			    sizeof(struct kaddr));
	}
}

int
kr_init(int fs)
{
	int		opt = 0, rcvbuf, default_rcvbuf;
	socklen_t	optlen;
	unsigned int	rtfilter;

	kr_state.fib_sync = fs;

	if ((kr_state.fd = socket(AF_ROUTE,
	    SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)) == -1) {
		log_warn("kr_init: socket");
		return (-1);
	}

	/* not interested in my own messages */
	if (setsockopt(kr_state.fd, SOL_SOCKET, SO_USELOOPBACK,
	    &opt, sizeof(opt)) == -1)
		log_warn("kr_init: setsockopt(SO_USELOOPBACK)");

	/* filter out unwanted messages */
	rtfilter = ROUTE_FILTER(RTM_ADD) | ROUTE_FILTER(RTM_GET) |
	    ROUTE_FILTER(RTM_CHANGE) | ROUTE_FILTER(RTM_DELETE) |
	    ROUTE_FILTER(RTM_IFINFO) | ROUTE_FILTER(RTM_NEWADDR) |
	    ROUTE_FILTER(RTM_DELADDR) | ROUTE_FILTER(RTM_IFANNOUNCE);

	if (setsockopt(kr_state.fd, PF_ROUTE, ROUTE_MSGFILTER,
	    &rtfilter, sizeof(rtfilter)) == -1)
		log_warn("kr_init: setsockopt(ROUTE_MSGFILTER)");

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

	if (fetchtable() == -1)
		return (-1);

	if (protect_lo() == -1)
		return (-1);

	event_set(&kr_state.ev, kr_state.fd, EV_READ | EV_PERSIST,
	    kr_dispatch_msg, NULL);
	event_add(&kr_state.ev, NULL);

	return (0);
}

int
kr_change(struct kroute *kroute)
{
	struct kroute_node	*kr;
	int			 action = RTM_ADD;

	kr = kroute_find_fec(kroute->prefix.s_addr, kroute->prefixlen,
	    kroute->nexthop);

	if (kr == NULL) {
		log_warnx("kr_change: lost FEC %s/%d",
		    inet_ntoa(kroute->prefix), kroute->prefixlen);
		return (-1);
	}

	if (kr->r.flags & F_LDPD_INSERTED)
		action = RTM_CHANGE;

	kr->r.local_label = kroute->local_label;
	kr->r.remote_label = kroute->remote_label;
	kr->r.flags = kr->r.flags | F_LDPD_INSERTED;

	/* send update */
	if (send_rtmsg(kr_state.fd, action, &kr->r, AF_MPLS) == -1)
		return (-1);

	if (kr->r.nexthop.s_addr != INADDR_ANY &&
	    kr->r.remote_label != NO_LABEL) {
		if (send_rtmsg(kr_state.fd, RTM_CHANGE, &kr->r, AF_INET) == -1)
			return (-1);
	}

	return  (0);
}

int
kr_delete(struct kroute *kroute)
{
	struct kroute_node	*kr;
	int			 update = 0;

	kr = kroute_find_fec(kroute->prefix.s_addr, kroute->prefixlen,
	    kroute->nexthop);
	if (kr == NULL)
		return (0);

	if (!(kr->r.flags & F_LDPD_INSERTED))
		return (0);
	if (kr->r.nexthop.s_addr != INADDR_ANY &&
	    kr->r.remote_label != NO_LABEL)
		update = 1;

	/* kill MPLS LSP */
	if (send_rtmsg(kr_state.fd, RTM_DELETE, &kr->r, AF_MPLS) == -1)
		return (-1);

	kr->r.flags &= ~F_LDPD_INSERTED;
	kr->r.local_label = NO_LABEL;
	kr->r.remote_label = NO_LABEL;

	if (update &&
	    send_rtmsg(kr_state.fd, RTM_CHANGE, &kr->r, AF_INET) == -1)
		return (-1);

	return (0);
}

void
kr_shutdown(void)
{
	kr_fib_decouple();

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
		if (kr->r.flags & F_LDPD_INSERTED) {
			send_rtmsg(kr_state.fd, RTM_ADD, &kr->r, AF_MPLS);

			if (kr->r.nexthop.s_addr != INADDR_ANY &&
			    kr->r.remote_label != NO_LABEL) {
				send_rtmsg(kr_state.fd, RTM_CHANGE,
				    &kr->r, AF_INET);
			}
		}

	log_info("kernel routing table coupled");
}

void
kr_fib_decouple(void)
{
	struct kroute_node	*kr;
	u_int32_t		 rl;

	if (kr_state.fib_sync == 0)	/* already decoupled */
		return;

	RB_FOREACH(kr, kroute_tree, &krt) {
		if (kr->r.flags & F_LDPD_INSERTED) {
			send_rtmsg(kr_state.fd, RTM_DELETE,
			    &kr->r, AF_MPLS);

			if (kr->r.nexthop.s_addr != INADDR_ANY &&
			    kr->r.remote_label != NO_LABEL) {
				rl = kr->r.remote_label;
				kr->r.remote_label = NO_LABEL;
				send_rtmsg(kr_state.fd, RTM_CHANGE,
				    &kr->r, AF_INET);
				kr->r.remote_label = rl;
			}
		}
	}

	kr_state.fib_sync = 0;

	log_info("kernel routing table decoupled");
}

/* ARGSUSED */
void
kr_dispatch_msg(int fd, short event, void *bula)
{
	dispatch_rtmsg();
}

void
kr_show_route(struct imsg *imsg)
{
	struct kroute_node	*kr, *kn;
	int			 flags;
	struct in_addr		 addr;

	switch (imsg->hdr.type) {
	case IMSG_CTL_KROUTE:
		if (imsg->hdr.len != IMSG_HEADER_SIZE + sizeof(flags)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		memcpy(&flags, imsg->data, sizeof(flags));
		RB_FOREACH(kr, kroute_tree, &krt)
			if (!flags || kr->r.flags & flags) {
				kn = kr;
				do {
					main_imsg_compose_ldpe(IMSG_CTL_KROUTE,
					    imsg->hdr.pid,
					    &kn->r, sizeof(kn->r));
				} while ((kn = kn->next) != NULL);
			}
		break;
	case IMSG_CTL_KROUTE_ADDR:
		if (imsg->hdr.len != IMSG_HEADER_SIZE +
		    sizeof(struct in_addr)) {
			log_warnx("kr_show_route: wrong imsg len");
			return;
		}
		memcpy(&addr, imsg->data, sizeof(addr));
		kr = NULL;
		kr = kroute_match(addr.s_addr);
		if (kr != NULL)
			main_imsg_compose_ldpe(IMSG_CTL_KROUTE, imsg->hdr.pid,
			    &kr->r, sizeof(kr->r));
		break;
	default:
		log_debug("kr_show_route: error handling imsg");
		break;
	}

	main_imsg_compose_ldpe(IMSG_CTL_END, imsg->hdr.pid, NULL, 0);
}

void
kr_ifinfo(char *ifname, pid_t pid)
{
	struct kif_node	*kif;

	RB_FOREACH(kif, kif_tree, &kit)
		if (ifname == NULL || !strcmp(ifname, kif->k.ifname)) {
			main_imsg_compose_ldpe(IMSG_CTL_IFINFO,
			    pid, &kif->k, sizeof(kif->k));
		}

	main_imsg_compose_ldpe(IMSG_CTL_END, pid, NULL, 0);
}

void
kr_redist_remove(struct kroute *kr)
{
	/* was the route redistributed? */
	if ((kr->flags & F_REDISTRIBUTED) == 0)
		return;

	/* remove redistributed flag */
	kr->flags &= ~F_REDISTRIBUTED;
	main_imsg_compose_lde(IMSG_NETWORK_DEL, 0, kr,
	    sizeof(struct kroute));
}

int
kr_redist_eval(struct kroute *kr)
{
	u_int32_t	 a;

	/* Dynamic routes are not redistributable. */
	if (kr->flags & F_DYNAMIC)
		goto dont_redistribute;

	/*
	 * We consider the loopback net, default route, multicast and
	 * experimental addresses as not redistributable.
	 */
	a = ntohl(kr->prefix.s_addr);
	if (IN_MULTICAST(a) || IN_BADCLASS(a) ||
	    (kr->prefixlen == 0) ||
	    (a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		goto dont_redistribute;
	/*
	 * Consider networks with nexthop loopback as not redistributable
	 * unless it is a reject or blackhole route.
	 */
	if (kr->nexthop.s_addr == htonl(INADDR_LOOPBACK) &&
	    !(kr->flags & (F_BLACKHOLE|F_REJECT)))
		goto dont_redistribute;

	/* prefix should be redistributed */
	kr->flags |= F_REDISTRIBUTED;
	main_imsg_compose_lde(IMSG_NETWORK_ADD, 0, kr, sizeof(struct kroute));
	return (1);

dont_redistribute:
	kr_redist_remove(kr);
	return (1);
}

void
kr_redistribute(struct kroute_node *kh)
{
	struct kroute_node	*kn;

	/* only the highest prio route can be redistributed */
	if (kroute_find_fec(kh->r.prefix.s_addr, kh->r.prefixlen,
	    kh->r.nexthop) != kh)
		return;

	for (kn = kh; kn; kn = kn->next)
		kr_redist_eval(&kn->r);
}

void
kr_reload(void)
{
	struct kroute_node	*kr;

	/* XXX this does not make sense in ldpd */
	RB_FOREACH(kr, kroute_tree, &krt) {
		if (kr->r.flags & F_REDISTRIBUTED)
			kr_redistribute(kr);
	}
}

/* rb-tree compare */
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

	if (ntohl(a->r.nexthop.s_addr) < ntohl(b->r.nexthop.s_addr))
		return (-1);
	if (ntohl(a->r.nexthop.s_addr) > ntohl(b->r.nexthop.s_addr))
		return (1);
	return (0);
}

int
kif_compare(struct kif_node *a, struct kif_node *b)
{
	return (b->k.ifindex - a->k.ifindex);
}

/* tree management */
struct kroute_node *
kroute_find_fec(in_addr_t prefix, u_int8_t prefixlen, struct in_addr nexthop)
{
	struct kroute_node	s;

	s.r.prefix.s_addr = prefix;
	s.r.prefixlen = prefixlen;
	s.r.nexthop.s_addr = nexthop.s_addr;

	return (RB_FIND(kroute_tree, &krt, &s));
}

struct kroute_node *
kroute_find_any(in_addr_t prefix, u_int8_t prefixlen)
{
	struct kroute_node	s;
	struct kroute_node	*kn, *best = NULL;

	s.r.prefix.s_addr = prefix;
	s.r.prefixlen = prefixlen;
	s.r.nexthop.s_addr = 0;

	kn = RB_NFIND(kroute_tree, &krt, &s);
	while (kn) {
		if (!best || best->r.priority > kn->r.priority)
			best = kn;
		kn = RB_NEXT(kroute_tree, &krt, kn);
		if (kn == NULL || kn->r.prefix.s_addr != prefix ||
		    kn->r.prefixlen != prefixlen)
			break;
	}
	return (best);
}

struct kroute_node *
kroute_matchprio(struct kroute_node *kr, u_int8_t prio)
{
	while (kr) {
		if (kr->r.priority == prio)
			return (kr);
		kr = kr->next;
	}

	return (NULL);
}

int
kroute_insert(struct kroute_node *kr)
{
	struct kroute_node	*krm, *krh;

	if ((krh = RB_INSERT(kroute_tree, &krt, kr)) != NULL) {
		/*
		 * Multiple FEC, add to ordered list
		 */
		if (kr->r.priority < krh->r.priority) {
			/* head element */
			if (RB_REMOVE(kroute_tree, &krt, krh) == NULL) {
				log_warnx("kroute_insert failed to del %s/%u",
				    inet_ntoa(krh->r.prefix), krh->r.prefixlen);
				return (-1);
			}
			if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
				log_warnx("kroute_insert failed to add %s/%u",
				    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
				return (-1);
			}
			kr->next = krh;
			krh = kr;
		} else {
			for (krm = krh; krm->next != NULL &&
			    krm->next->r.priority < kr->r.priority;
			    krm = krm->next)
				;
			kr->next = krm->next;
			krm->next = kr;
		}
	} else
		krh = kr;

	kr_redistribute(krh);
	return (0);
}

int
kroute_remove(struct kroute_node *kr)
{
	struct kroute_node	*krm;

	if ((krm = RB_FIND(kroute_tree, &krt, kr)) == NULL) {
		log_warnx("kroute_remove failed to find %s/%u",
		    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
		return (-1);
	}

	if (krm == kr) {
		/* head element */
		if (RB_REMOVE(kroute_tree, &krt, kr) == NULL) {
			log_warnx("kroute_remove failed for %s/%u",
			    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
			return (-1);
		}
		if (kr->next != NULL) {
			if (RB_INSERT(kroute_tree, &krt, kr->next) != NULL) {
				log_warnx("kroute_remove failed to add %s/%u",
				    inet_ntoa(kr->r.prefix), kr->r.prefixlen);
				return (-1);
			}
		}
	} else {
		/* somewhere in the list */
		while (krm->next != kr && krm->next != NULL)
			krm = krm->next;
		if (krm->next == NULL) {
			log_warnx("kroute_remove multipath list corrupted "
			    "for %s/%u", inet_ntoa(kr->r.prefix),
			    kr->r.prefixlen);
			return (-1);
		}
		krm->next = kr->next;
	}

	kr_redist_remove(&kr->r);

	/* kill MPLS LSP if one was installed */
	if (kr->r.flags & F_LDPD_INSERTED)
		if (send_rtmsg(kr_state.fd, RTM_DELETE, &kr->r, AF_MPLS) ==
		    -1) {
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

struct kif_node *
kif_find(u_short ifindex)
{
	struct kif_node	s;

	bzero(&s, sizeof(s));
	s.k.ifindex = ifindex;

	return (RB_FIND(kif_tree, &kit, &s));
}

struct kif *
kif_findname(char *ifname)
{
	struct kif_node	*kif;

	RB_FOREACH(kif, kif_tree, &kit)
		if (!strcmp(ifname, kif->k.ifname))
			return (&kif->k);

	return (NULL);
}

struct kif_node *
kif_insert(u_short ifindex)
{
	struct kif_node	*kif;

	if ((kif = calloc(1, sizeof(struct kif_node))) == NULL)
		return (NULL);

	kif->k.ifindex = ifindex;
	TAILQ_INIT(&kif->addrs);

	if (RB_INSERT(kif_tree, &kit, kif) != NULL)
		fatalx("kif_insert: RB_INSERT");

	return (kif);
}

int
kif_remove(struct kif_node *kif)
{
	struct kif_addr	*ka;

	if (RB_REMOVE(kif_tree, &kit, kif) == NULL) {
		log_warnx("RB_REMOVE(kif_tree, &kit, kif)");
		return (-1);
	}

	while ((ka = TAILQ_FIRST(&kif->addrs)) != NULL) {
		TAILQ_REMOVE(&kif->addrs, ka, entry);
		free(ka);
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

struct kif_node *
kif_update(u_short ifindex, int flags, struct if_data *ifd,
    struct sockaddr_dl *sdl, int *link_old)
{
	struct kif_node		*kif;

	if ((kif = kif_find(ifindex)) == NULL) {
		if ((kif = kif_insert(ifindex)) == NULL)
			return (NULL);
	} else
		*link_old = (kif->k.flags & IFF_UP) &&
		    LINK_STATE_IS_UP(kif->k.link_state);

	kif->k.flags = flags;
	kif->k.link_state = ifd->ifi_link_state;
	kif->k.media_type = ifd->ifi_type;
	kif->k.baudrate = ifd->ifi_baudrate;
	kif->k.mtu = ifd->ifi_mtu;

	if (sdl && sdl->sdl_family == AF_LINK) {
		if (sdl->sdl_nlen >= sizeof(kif->k.ifname))
			memcpy(kif->k.ifname, sdl->sdl_data,
			    sizeof(kif->k.ifname) - 1);
		else if (sdl->sdl_nlen > 0)
			memcpy(kif->k.ifname, sdl->sdl_data,
			    sdl->sdl_nlen);
		/* string already terminated via calloc() */
	}

	return (kif);
}

struct kroute_node *
kroute_match(in_addr_t key)
{
	int			 i;
	struct kroute_node	*kr;

	/* we will never match the default route */
	for (i = 32; i > 0; i--)
		if ((kr = kroute_find_any(key & prefixlen2mask(i), i)) != NULL)
			return (kr);

	/* if we don't have a match yet, try to find a default route */
	if ((kr = kroute_find_any(0, 0)) != NULL)
			return (kr);
	return (NULL);
}

/* misc */
int
protect_lo(void)
{
	struct kroute_node	*kr;

	/* special protection for 127/8 */
	if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL) {
		log_warn("protect_lo");
		return (-1);
	}
	kr->r.prefix.s_addr = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);
	kr->r.prefixlen = 8;
	kr->r.flags = F_CONNECTED;
	kr->r.local_label = NO_LABEL;
	kr->r.remote_label = NO_LABEL;

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

in_addr_t
prefixlen2mask(u_int8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	return (htonl(0xffffffff << (32 - prefixlen)));
}

#define	ROUNDUP(a)	\
    (((a) & (sizeof(long) - 1)) ? (1 + ((a) | (sizeof(long) - 1))) : (a))

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
    struct sockaddr_dl *sdl)
{
	struct kif_node		*kif;
	struct kif_addr		*ka;
	int			 link_old = 0, link_new;

	kif = kif_update(ifindex, flags, ifd, sdl, &link_old);
	if (!kif) {
		log_warn("if_change: kif_update(%u)", ifindex);
		return;
	}
	link_new = (kif->k.flags & IFF_UP) &&
	    LINK_STATE_IS_UP(kif->k.link_state);

	if (link_new == link_old) {
		main_imsg_compose_ldpe(IMSG_IFSTATUS, 0, &kif->k,
		    sizeof(struct kif));
		return;
	} else if (link_new) {
		main_imsg_compose_ldpe(IMSG_IFUP, 0, &kif->k,
		    sizeof(struct kif));
		TAILQ_FOREACH(ka, &kif->addrs, entry)
			main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka->addr,
			    sizeof(struct kaddr));
	} else {
		main_imsg_compose_ldpe(IMSG_IFDOWN, 0, &kif->k,
		    sizeof(struct kif));
		TAILQ_FOREACH(ka, &kif->addrs, entry)
			main_imsg_compose_ldpe(IMSG_DELADDR, 0, &ka->addr,
			    sizeof(struct kaddr));
	}
}

void
if_newaddr(u_short ifindex, struct sockaddr_in *ifa, struct sockaddr_in *mask,
    struct sockaddr_in *brd)
{
	struct kif_node *kif;
	struct kif_addr *ka;
	u_int32_t	 a;

	if (ifa == NULL || ifa->sin_family != AF_INET)
		return;
	if ((kif = kif_find(ifindex)) == NULL) {
		log_warnx("if_newaddr: corresponding if %d not found", ifindex);
		return;
	}
	a = ntohl(ifa->sin_addr.s_addr);
	if (IN_MULTICAST(a) || IN_BADCLASS(a) ||
	    (a >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET)
		return;

	if ((ka = calloc(1, sizeof(struct kif_addr))) == NULL)
		fatal("if_newaddr");
	ka->addr.ifindex = ifindex;
	ka->addr.addr.s_addr = ifa->sin_addr.s_addr;
	if (mask)
		ka->addr.mask.s_addr = mask->sin_addr.s_addr;
	else
		ka->addr.mask.s_addr = INADDR_NONE;
	if (brd)
		ka->addr.dstbrd.s_addr = brd->sin_addr.s_addr;
	else
		ka->addr.dstbrd.s_addr = INADDR_NONE;

	TAILQ_INSERT_TAIL(&kif->addrs, ka, entry);

	/* notify ldpe about new address */
	main_imsg_compose_ldpe(IMSG_NEWADDR, 0, &ka->addr,
	    sizeof(struct kaddr));
}

void
if_deladdr(u_short ifindex, struct sockaddr_in *ifa, struct sockaddr_in *mask,
    struct sockaddr_in *brd)
{
	struct kif_node *kif;
	struct kif_addr *ka, *nka;

	if (ifa == NULL || ifa->sin_family != AF_INET)
		return;
	if ((kif = kif_find(ifindex)) == NULL) {
		log_warnx("if_deladdr: corresponding if %d not found", ifindex);
		return;
	}

	for (ka = TAILQ_FIRST(&kif->addrs); ka != NULL; ka = nka) {
		nka = TAILQ_NEXT(ka, entry);

		if (ka->addr.addr.s_addr == ifa->sin_addr.s_addr) {
			TAILQ_REMOVE(&kif->addrs, ka, entry);

			/* notify ldpe about removed address */
			main_imsg_compose_ldpe(IMSG_DELADDR, 0, &ka->addr,
			    sizeof(struct kaddr));

			free(ka);
			return;
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
		kif = kif_insert(ifan->ifan_index);
		strlcpy(kif->k.ifname, ifan->ifan_name, sizeof(kif->k.ifname));
		break;
	case IFAN_DEPARTURE:
		kif = kif_find(ifan->ifan_index);
		kif_remove(kif);
		break;
	}
}

/* rtsock */
int
send_rtmsg(int fd, int action, struct kroute *kroute, u_int32_t family)
{
	struct iovec		iov[5];
	struct rt_msghdr	hdr;
	struct sockaddr_mpls	label_in, label_out;
	struct sockaddr_in	dst, mask, nexthop;
	int			iovcnt = 0;

	if (kr_state.fib_sync == 0)
		return (0);

	/* Implicit NULL label should not be added to the FIB */
	if (family == AF_MPLS && kroute->local_label == MPLS_LABEL_IMPLNULL)
		return (0);

	/* initialize header */
	bzero(&hdr, sizeof(hdr));
	hdr.rtm_version = RTM_VERSION;

	hdr.rtm_type = action;
	hdr.rtm_flags = RTF_UP;
	hdr.rtm_fmask = RTF_MPLS;
	hdr.rtm_seq = kr_state.rtseq++;	/* overflow doesn't matter */
	hdr.rtm_msglen = sizeof(hdr);
	hdr.rtm_hdrlen = sizeof(struct rt_msghdr);
	hdr.rtm_priority = kroute->priority;
	/* adjust iovec */
	iov[iovcnt].iov_base = &hdr;
	iov[iovcnt++].iov_len = sizeof(hdr);

	if (family == AF_MPLS) {
		bzero(&label_in, sizeof(label_in));
		label_in.smpls_len = sizeof(label_in);
		label_in.smpls_family = AF_MPLS;
		label_in.smpls_label =
		    htonl(kroute->local_label << MPLS_LABEL_OFFSET);
		/* adjust header */
		hdr.rtm_flags |= RTF_MPLS | RTF_MPATH;
		hdr.rtm_addrs |= RTA_DST;
		hdr.rtm_msglen += sizeof(label_in);
		/* adjust iovec */
		iov[iovcnt].iov_base = &label_in;
		iov[iovcnt++].iov_len = sizeof(label_in);
	} else {
		bzero(&dst, sizeof(dst));
		dst.sin_len = sizeof(dst);
		dst.sin_family = AF_INET;
		dst.sin_addr.s_addr = kroute->prefix.s_addr;
		/* adjust header */
		hdr.rtm_addrs |= RTA_DST;
		hdr.rtm_msglen += sizeof(dst);
		/* adjust iovec */
		iov[iovcnt].iov_base = &dst;
		iov[iovcnt++].iov_len = sizeof(dst);
	}

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

	if (family == AF_INET) {
		bzero(&mask, sizeof(mask));
		mask.sin_len = sizeof(mask);
		mask.sin_family = AF_INET;
		mask.sin_addr.s_addr = prefixlen2mask(kroute->prefixlen);
		/* adjust header */
		hdr.rtm_addrs |= RTA_NETMASK;
		hdr.rtm_msglen += sizeof(mask);
		/* adjust iovec */
		iov[iovcnt].iov_base = &mask;
		iov[iovcnt++].iov_len = sizeof(mask);
	}

	/* If action is RTM_DELETE we have to get rid of MPLS infos */
	if (kroute->remote_label != NO_LABEL && action != RTM_DELETE) {
		bzero(&label_out, sizeof(label_out));
		label_out.smpls_len = sizeof(label_out);
		label_out.smpls_family = AF_MPLS;
		label_out.smpls_label =
		    htonl(kroute->remote_label << MPLS_LABEL_OFFSET);
		/* adjust header */
		hdr.rtm_addrs |= RTA_SRC;
		hdr.rtm_flags |= RTF_MPLS;
		hdr.rtm_msglen += sizeof(label_out);
		/* adjust iovec */
		iov[iovcnt].iov_base = &label_out;
		iov[iovcnt++].iov_len = sizeof(label_out);

		if (kroute->remote_label == MPLS_LABEL_IMPLNULL) {
			if (family == AF_MPLS)
				hdr.rtm_mpls = MPLS_OP_POP;
			else
				return (0);
		} else {
			if (family == AF_MPLS)
				hdr.rtm_mpls = MPLS_OP_SWAP;
			else
				hdr.rtm_mpls = MPLS_OP_PUSH;
		}
	}

retry:
	if (writev(fd, iov, iovcnt) == -1) {
		if (errno == ESRCH) {
			if (hdr.rtm_type == RTM_CHANGE && family == AF_MPLS) {
				hdr.rtm_type = RTM_ADD;
				goto retry;
			} else if (hdr.rtm_type == RTM_DELETE) {
				log_info("route %s/%u vanished before delete",
				    inet_ntoa(kroute->prefix),
				    kroute->prefixlen);
				return (0);
			}
		}
		log_warn("send_rtmsg: action %u, AF %d, prefix %s/%u",
		    hdr.rtm_type, family, inet_ntoa(kroute->prefix),
		    kroute->prefixlen);
		return (0);
	}

	return (0);
}

int
fetchtable(void)
{
	size_t			 len;
	int			 mib[7];
	char			*buf;
	int			 rv;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = 0;	/* rtableid */

	if (sysctl(mib, 7, NULL, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		return (-1);
	}
	if ((buf = malloc(len)) == NULL) {
		log_warn("fetchtable");
		return (-1);
	}
	if (sysctl(mib, 7, buf, &len, NULL, 0) == -1) {
		log_warn("sysctl");
		free(buf);
		return (-1);
	}

	rv = rtmsg_process(buf, len);
	free(buf);

	return (rv);
}

int
fetchifs(u_short ifindex)
{
	size_t			 len;
	int			 mib[6];
	char			*buf;
	int			 rv;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
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

	rv = rtmsg_process(buf, len);
	free(buf);

	return (rv);
}

int
dispatch_rtmsg(void)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;

	if ((n = read(kr_state.fd, &buf, sizeof(buf))) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return (0);
		log_warn("dispatch_rtmsg: read error");
		return (-1);
	}

	if (n == 0) {
		log_warnx("routing socket closed");
		return (-1);
	}

	return (rtmsg_process(buf, n));
}

int
rtmsg_process(char *buf, size_t len)
{
	struct rt_msghdr	*rtm;
	struct if_msghdr	 ifm;
	struct ifa_msghdr	*ifam;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct kroute_node	*kr, *okr;
	struct in_addr		 prefix, nexthop;
	u_int8_t		 prefixlen, prio;
	int			 flags, mpath;
	u_short			 ifindex = 0;
	size_t			 offset;
	char			*next;

	for (offset = 0; offset < len; offset += rtm->rtm_msglen) {
		next = buf + offset;
		rtm = (struct rt_msghdr *)next;
		if (len < offset + sizeof(*rtm) ||
		    len < offset + rtm->rtm_msglen)
			fatalx("rtmsg_process: partial rtm in buffer");
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		log_rtmsg(rtm->rtm_type);

		prefix.s_addr = 0;
		prefixlen = 0;
		flags = 0;
		nexthop.s_addr = 0;
		mpath = 0;
		prio = 0;

		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_GET:
		case RTM_CHANGE:
		case RTM_DELETE:
			prefix.s_addr = 0;
			prefixlen = 0;
			nexthop.s_addr = 0;
			mpath = 0;
			prio = 0;

			if (rtm->rtm_errno)		/* failed attempts... */
				continue;

			if (rtm->rtm_tableid != 0)
				continue;

			if ((sa = rti_info[RTAX_DST]) == NULL)
				continue;

			if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
				continue;

			if (rtm->rtm_flags & RTF_MPATH)
				mpath = 1;
			prio = rtm->rtm_priority;

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
				if (rtm->rtm_flags & RTF_STATIC)
					flags |= F_STATIC;
				if (rtm->rtm_flags & RTF_BLACKHOLE)
					flags |= F_BLACKHOLE;
				if (rtm->rtm_flags & RTF_REJECT)
					flags |= F_REJECT;
				if (rtm->rtm_flags & RTF_DYNAMIC)
					flags |= F_DYNAMIC;
				break;
			default:
				continue;
			}

			ifindex = rtm->rtm_index;
			if ((sa = rti_info[RTAX_GATEWAY]) != NULL) {
				switch (sa->sa_family) {
				case AF_INET:
					nexthop.s_addr = ((struct
					    sockaddr_in *)sa)->sin_addr.s_addr;
					break;
				case AF_LINK:
					flags |= F_CONNECTED;
					break;
				}
			}
		}

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_GET:
		case RTM_CHANGE:
			if (nexthop.s_addr == 0 && !(flags & F_CONNECTED)) {
				log_warnx("no nexthop for %s/%u",
				    inet_ntoa(prefix), prefixlen);
				continue;
			}

			/* routes attached to loopback interfaces */
			if (prefix.s_addr == nexthop.s_addr)
				flags |= F_CONNECTED;

			if ((okr = kroute_find_fec(prefix.s_addr, prefixlen,
			    nexthop))
			    != NULL) {
				/* get the correct route */
				kr = okr;
				if ((kr = kroute_matchprio(okr, prio)) ==
				    NULL) {
					log_warnx("mpath route not found");
					/* add routes we missed out earlier */
					goto add;
				}

				if (kr->r.flags & F_LDPD_INSERTED)
					flags |= F_LDPD_INSERTED;
				kr->r.nexthop.s_addr = nexthop.s_addr;
				kr->r.flags = flags;
				kr->r.ifindex = ifindex;

				/* just readd, the RDE will care */
				kr_redistribute(okr);
			} else {
add:
				if ((kr = calloc(1,
				    sizeof(struct kroute_node))) == NULL) {
					log_warn("dispatch calloc");
					return (-1);
				}
				kr->r.prefix.s_addr = prefix.s_addr;
				kr->r.prefixlen = prefixlen;
				kr->r.nexthop.s_addr = nexthop.s_addr;
				kr->r.flags = flags;
				kr->r.ifindex = ifindex;
				kr->r.priority = prio;
				kr->r.local_label = NO_LABEL;
				kr->r.remote_label = NO_LABEL;

				kroute_insert(kr);
			}
			break;
		case RTM_DELETE:
			if ((kr = kroute_find_fec(prefix.s_addr, prefixlen,
			    nexthop)) == NULL)
				continue;
			/* get the correct route */
			okr = kr;
			if ((kr = kroute_matchprio(kr, prio)) == NULL) {
				log_warnx("dispatch_rtmsg mpath route"
				    " not found");
				return (-1);
			}
			if (kroute_remove(kr) == -1)
				return (-1);
			break;
		case RTM_IFINFO:
			memcpy(&ifm, next, sizeof(ifm));
			if_change(ifm.ifm_index, ifm.ifm_flags, &ifm.ifm_data,
			    (struct sockaddr_dl *)rti_info[RTAX_IFP]);
			break;
		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;

			if_newaddr(ifam->ifam_index,
			    (struct sockaddr_in *)rti_info[RTAX_IFA],
			    (struct sockaddr_in *)rti_info[RTAX_NETMASK],
			    (struct sockaddr_in *)rti_info[RTAX_BRD]);
			break;
		case RTM_DELADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;

			if_deladdr(ifam->ifam_index,
			    (struct sockaddr_in *)rti_info[RTAX_IFA],
			    (struct sockaddr_in *)rti_info[RTAX_NETMASK],
			    (struct sockaddr_in *)rti_info[RTAX_BRD]);
			break;
		case RTM_IFANNOUNCE:
			if_announce(next);
			break;
		default:
			/* ignore for now */
			break;
		}
	}

	return (offset);
}
