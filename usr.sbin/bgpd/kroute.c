/*	$OpenBSD: kroute.c,v 1.8 2003/12/24 21:14:22 henning Exp $ */

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

#include <sys/types.h>
#include <sys/socket.h>
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
	RB_ENTRY(kroute_node)	entry;
	struct kroute		r;
	int			flags;
};

int	kroute_msg(int, int, struct kroute *);
int	kroute_compare(struct kroute_node *, struct kroute_node *);

RB_HEAD(kroute_tree, kroute_node)	kroute_tree, krt;
RB_PROTOTYPE(kroute_tree, kroute_node, entry, kroute_compare);
RB_GENERATE(kroute_tree, kroute_node, entry, kroute_compare);

u_int32_t		rtseq = 1;

#define	F_BGPD_INSERTED		0x0001

int
kroute_init(void)
{
	int	s, opt;

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) < 0)
		fatal("route socket", errno);

	/* not intrested in my own messages */
	if (setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, &opt, sizeof(opt)) == -1)
		fatal("route setsockopt", errno);

	RB_INIT(&krt);

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
	r.hdr.rtm_flags = RTF_GATEWAY|RTF_PROTO1; /* XXX */
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
			}
			break;
		case EEXIST:	/* connected route */
			return (-2);
		default:
			logit(LOG_INFO, "kroute_msg: %s", strerror(errno));
			return (-1);
		}
	}
	if (n == sizeof(r))
		return (0);

	/* XXX we could not write everything... bad bad bad. cope. */
	return (n);
}

int
kroute_add(int fd, struct kroute *kroute)
{
	struct kroute_node	*kr;
	int			 n;

	if ((n = kroute_msg(fd, RTM_ADD, kroute)) == -2) /* connected route */
		return (0);

	if (n == -1)
		return (-1);

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

	return (n);
}

int
kroute_change(int fd, struct kroute *kroute)
{
	struct kroute_node	*kr, s;

	s.r.prefix = kroute->prefix;
	s.r.prefixlen = kroute->prefixlen;

	if ((kr = RB_FIND(kroute_tree, &krt, &s)) == NULL) {
		log_kroute(LOG_CRIT, "kroute_change: no match for", kroute);
		return (-1);
	}

	if (!(kr->flags & F_BGPD_INSERTED)) {
		logit(LOG_CRIT, "trying to change route not inserted by bgpd");
		return (0);
	}

	kr->r.nexthop = kroute->nexthop;

	return (kroute_msg(fd, RTM_CHANGE, kroute));
}

int
kroute_delete(int fd, struct kroute *kroute)
{
	struct kroute_node	*kr, s;
	int			 n;

	s.r.prefix = kroute->prefix;
	s.r.prefixlen = kroute->prefixlen;

	if ((kr = RB_FIND(kroute_tree, &krt, &s)) == NULL) {
		/* at the moment this is totally valid... */
		log_kroute(LOG_CRIT, "kroute_delete: no match for", kroute);
		return (0);
	}

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
