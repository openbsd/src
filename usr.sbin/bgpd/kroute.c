/*	$OpenBSD: kroute.c,v 1.24 2003/12/26 00:14:04 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <net/route.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

struct kroute_node {
	RB_ENTRY(kroute_node)	 entry;
	struct kroute		 r;
	int			 flags;
};

struct knexthop_node {
	RB_ENTRY(knexthop_node)	 entry;
	in_addr_t		 nexthop;
	struct kroute_node	*kroute;
};

int		kroute_msg(int, int, struct kroute *);
int		kroute_compare(struct kroute_node *, struct kroute_node *);
void		get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
u_int8_t	prefixlen_classful(in_addr_t);
u_int8_t	mask2prefixlen(in_addr_t);
int		kroute_fetchtable(void);
int		knexthop_compare(struct knexthop_node *,
		    struct knexthop_node *);
void		kroute_remove(struct kroute_node *);

struct kroute_node * kroute_match(in_addr_t);

RB_HEAD(kroute_tree, kroute_node)	kroute_tree, krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare);
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare);

RB_HEAD(knexthop_tree, knexthop_node)	knexthop_tree, knt;
RB_PROTOTYPE(knexthop_tree, knexthop_node, entry, knexthop_compare);
RB_GENERATE(knexthop_tree, knexthop_node, entry, knexthop_compare);

u_int32_t		rtseq = 1;
pid_t			pid;

#define	F_BGPD_INSERTED		0x0001
#define	F_KERNEL		0x0002
#define	F_CONNECTED		0x0004
#define	F_NEXTHOP		0x0008

int
kroute_init(void)
{
	int	s, opt;

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) < 0)
		fatal("route socket", errno);

	/* not intrested in my own messages */
	if (setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, &opt, sizeof(opt)) == -1)
		fatal("route setsockopt", errno);

	pid = getpid();

	RB_INIT(&krt);
	RB_INIT(&knt);
	kroute_fetchtable();

	return (s);
}

int
kroute_msg(int fd, int action, struct kroute *kroute)
{
	struct {
		struct rt_msghdr	hdr;
		struct sockaddr_in	prefix;
		struct sockaddr_in	nexthop;
		struct sockaddr_in	mask;
	} r;
	ssize_t	n;

	bzero(&r, sizeof(r));
	r.hdr.rtm_msglen = sizeof(r);
	r.hdr.rtm_version = RTM_VERSION;
	r.hdr.rtm_type = action;
	r.hdr.rtm_flags = RTF_GATEWAY|RTF_PROTO1;
	r.hdr.rtm_seq = rtseq++;	/* overflow doesn't matter */
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
				logit(LOG_INFO, "route vanished before delete");
				return (0);
			} else			/* nexthop invalid */
				return (-1);
			break;
		default:
			logit(LOG_INFO, "kroute_msg: %s", strerror(errno));
			return (-1);
		}
	}

	return (0);
}

int
kroute_change(int fd, struct kroute *kroute)
{
	struct kroute_node	*kr, s;
	int			 n;
	int			 action = RTM_ADD;

	s.r.prefix = kroute->prefix;
	s.r.prefixlen = kroute->prefixlen;

	if ((kr = RB_FIND(kroute_tree, &krt, &s)) != NULL) {
		if (kr->flags & F_BGPD_INSERTED)
			action = RTM_CHANGE;
		else
			return (0);
	}

	if ((n = kroute_msg(fd, action, kroute)) == -1)
		return (-1);

	if (action == RTM_ADD) {
		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL)
			fatal(NULL, errno);

		kr->r.prefix = kroute->prefix;
		kr->r.prefixlen = kroute->prefixlen;
		kr->r.nexthop = kroute->nexthop;
		kr->flags = F_BGPD_INSERTED;

		if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
			logit(LOG_CRIT, "RB_INSERT failed!");
			return (-1);
		}
	} else
		kr->r.nexthop = kroute->nexthop;

	return (n);
}

int
kroute_delete(int fd, struct kroute *kroute)
{
	struct kroute_node	*kr, s;
	int			 n;

	s.r.prefix = kroute->prefix;
	s.r.prefixlen = kroute->prefixlen;

	if ((kr = RB_FIND(kroute_tree, &krt, &s)) == NULL)
		return (0);

	if (!(kr->flags & F_BGPD_INSERTED))
		return (0);

	if ((n = kroute_msg(fd, RTM_DELETE, kroute)) == -1)
		return (-1);

	RB_REMOVE(kroute_tree, &krt, kr);
	free(kr);

	return (0);
}

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

void
kroute_shutdown(int fd)
{
	struct kroute_node	*kr;

	RB_FOREACH(kr, kroute_tree, &krt)
		if ((kr->flags & F_BGPD_INSERTED))
			kroute_msg(fd, RTM_DELETE, &kr->r);
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

u_int8_t
prefixlen_classful(in_addr_t ina)
{
	if (ina >= 0xf0000000)		/* class E */
		return (32);
	else if (ina >= 0xe0000000)	/* class D */
		return (4);
	else if (ina >= 0xc0000000)	/* class C */
		return (24);
	else if (ina >= 0x80000000)	/* class B */
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

int
kroute_fetchtable(void)
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

	if (sysctl(mib, 6, NULL, &len, NULL, 0) == -1)
		fatal("sysctl", errno);
	if ((buf = malloc(len)) == NULL)
		fatal("kroute_fetchtable", errno);
	if (sysctl(mib, 6, buf, &len, NULL, 0) == -1)
		fatal("sysctl", errno);

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_DST]) == NULL)
			continue;

		if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
			continue;

		if ((kr = calloc(1, sizeof(struct kroute_node))) == NULL)
			fatal(NULL, errno);

		kr->flags = F_KERNEL;
		kr->r.prefix = sa_in->sin_addr.s_addr;
		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK]) !=
		    NULL) {
			kr->r.prefixlen =
			    mask2prefixlen(sa_in->sin_addr.s_addr);
		} else if (rtm->rtm_flags & RTF_HOST)
			kr->r.prefixlen = 32;
		else
			kr->r.prefixlen = prefixlen_classful(kr->r.prefix);

		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_GATEWAY]) !=
		    NULL) {
			if (sa_in->sin_family == AF_INET)
				kr->r.nexthop = sa_in->sin_addr.s_addr;
			else if (sa_in->sin_family == AF_LINK)
				kr->flags |= F_CONNECTED;
		}

		if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
			logit(LOG_CRIT, "RB_INSERT failed!");
			return (-1);
		}
	}
	free(buf);
	return (0);
}

void
kroute_dispatch_msg(int fd)
{
	char			 buf[RT_BUF_SIZE];
	ssize_t			 n;
	char			*next, *lim;
	struct rt_msghdr	*rtm;
	struct sockaddr		*sa, *rti_info[RTAX_MAX];
	struct sockaddr_in	*sa_in;
	struct kroute_node	*kr, s;
	in_addr_t		 nexthop;
	int			 flags;

	if ((n = read(fd, &buf, sizeof(buf))) == -1)
		fatal("read error on routing socket", errno);
	if (n == 0)
		fatal("routing socket closed", 0);

	lim = buf + n;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sa = (struct sockaddr *)(rtm + 1);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
		flags = F_KERNEL;
		nexthop = 0;

		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_DST]) == NULL)
			continue;

		if (rtm->rtm_flags & RTF_LLINFO)	/* arp cache */
			continue;

		if (rtm->rtm_pid == pid)		/* cause by us */
			continue;

		if (rtm->rtm_errno)			/* failed attempts... */
			continue;

		s.r.prefix = sa_in->sin_addr.s_addr;
		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK]) !=
		    NULL) {
			s.r.prefixlen =
			    mask2prefixlen(sa_in->sin_addr.s_addr);
		} else if (rtm->rtm_flags & RTF_HOST)
			s.r.prefixlen = 32;
		else
			s.r.prefixlen = prefixlen_classful(s.r.prefix);

		if ((sa_in = (struct sockaddr_in *)rti_info[RTAX_GATEWAY]) !=
		    NULL) {
			if (sa_in->sin_family == AF_INET)
				nexthop = sa_in->sin_addr.s_addr;
			else if (sa_in->sin_family == AF_LINK)
				flags |= F_CONNECTED;
		}

		switch (rtm->rtm_type) {
		case RTM_ADD:
		case RTM_CHANGE:
			if (nexthop == 0)
				fatal("nexthop is 0 in kroute_dispatch!", 0);

			if ((kr = RB_FIND(kroute_tree, &krt, &s)) != NULL) {
				if (kr->flags & F_KERNEL) {
					kr->r.nexthop = nexthop;
					if (kr->flags & F_NEXTHOP) /* XXX */
						flags |= F_NEXTHOP;
					kr->flags = flags;
				}
			} else {
				if ((kr = calloc(1,
				    sizeof(struct kroute_node))) == NULL)
					fatal(NULL, errno);
				kr->r.prefix = s.r.prefix;
				kr->r.prefixlen = s.r.prefixlen;
				kr->r.nexthop = nexthop;
				kr->flags = flags;

				if (RB_INSERT(kroute_tree, &krt, kr) != NULL) {
					logit(LOG_CRIT, "RB_INSERT failed!");
					continue;
				}
			}
			break;
		case RTM_DELETE:
			if ((kr = RB_FIND(kroute_tree, &krt, &s)) == NULL)
				continue;
			if (!(kr->flags & F_KERNEL))
				continue;
			kroute_remove(kr);
			break;
		default:
			/* ingnore for now */
			break;
		}

	}
}

void
kroute_remove(struct kroute_node *kr)
{
	struct knexthop_node	*s;
	struct kroute_nexthop	 nh;

	/*
	 * the foreach is suboptimal, but:
	 * -routes disappering that have nexthops attached should not
	 *  happen often; except for some ibgp and multihop that should
	 *  be connected routes anyway
	 * -maintaining an SLIST per kroute_node is probably worse
	 * -it is easy to change if needed ;-)
	 */

	RB_REMOVE(kroute_tree, &krt, kr);

	if ((kr->flags & F_KERNEL) && (kr->flags & F_NEXTHOP))
		RB_FOREACH(s, knexthop_tree, &knt)
			if (s->kroute == kr) {
				/*
				 * XXX check again wether there's another
				 * non-bgp route. if not, notify RDE
				 * that this nexthop is now invalid
				 */
				bzero(&nh, sizeof(nh));
				kroute_validate_nexthop(s->nexthop, &nh);
				if (nh.valid == 0)	/* no alternate route */
					send_nexthop_update(&nh);
			}

	free(kr);
}

struct kroute_node *
kroute_match(in_addr_t key)
{
	int			i;
	struct kroute_node	s, *kr;
	in_addr_t		ina;

	ina = ntohl(key);

	for (i = 32; i >= 0; i--) {
		s.r.prefix = htonl(ina & (0xffffffff << (32 - i)));
		s.r.prefixlen = i;
		if ((kr = RB_FIND(kroute_tree, &krt, &s)) != NULL)
			return (kr);
	}

	return (NULL);
}

void
kroute_nexthop_check(in_addr_t key)
{
	struct kroute_nexthop	 nh;
	struct knexthop_node	*h, s;

	s.nexthop = key;

	bzero(&nh, sizeof(nh));
	nh.nexthop = key;

	if ((h = RB_FIND(knexthop_tree, &knt, &s)) != NULL) {
		if (h->kroute != NULL) {
			nh.valid = 1;
			nh.connected = h->kroute->flags & F_CONNECTED;
			nh.gateway = h->kroute->r.nexthop;
		}
	} else
		kroute_validate_nexthop(key, &nh);
	send_nexthop_update(&nh);
}

void
kroute_validate_nexthop(in_addr_t key, struct kroute_nexthop *nh)
{
	struct kroute_node	*kr;
	struct knexthop_node	*h;

	if ((h = calloc(1, sizeof(struct knexthop_node))) == NULL)
		fatal(NULL, errno);

	h->nexthop = nh->nexthop = key;

	if ((kr = kroute_match(key)) != NULL)
		if (kr->flags & F_KERNEL) {	/* must be non-bgp! */
			h->kroute = kr;
			kr->flags |= F_NEXTHOP;
			nh->valid = 1;
			nh->connected = kr->flags & F_CONNECTED;
			nh->gateway = kr->r.nexthop;
		}

	if (RB_INSERT(knexthop_tree, &knt, h) != NULL)
		fatal("RB_INSERT(knexthop_tree, &knt, h) failed!", 0);
}

int
knexthop_compare(struct knexthop_node *a, struct knexthop_node *b)
{
	return (b->nexthop - a->nexthop);
}

