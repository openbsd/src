/*	$OpenBSD: kroute.c,v 1.4 2003/12/23 16:17:15 henning Exp $ */

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

int	kroute_msg(int, int, struct kroute *);

u_int32_t	rtseq = 1;

int
kroute_init(void)
{
	int	s, opt;

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) < 0)
		fatal("route socket", errno);

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		fatal(NULL, errno);

	/* not intrested in my own messages */
	if (setsockopt(s, SOL_SOCKET, SO_USELOOPBACK, &opt, sizeof(opt)) == -1)
		fatal("route setsockopt", errno);

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
	r.hdr.rtm_flags = RTF_GATEWAY; /* XXX */
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
		case EEXIST:	/* connected route. ignore */
			return (0);
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
	return (kroute_msg(fd, RTM_ADD, kroute));
}

int
kroute_change(int fd, struct kroute *kroute)
{
	return (kroute_msg(fd, RTM_CHANGE, kroute));
}

int
kroute_delete(int fd, struct kroute *kroute)
{
	return (kroute_msg(fd, RTM_DELETE, kroute));
}
