/*	$OpenBSD: rthdr.c,v 1.7 2005/03/25 13:24:12 otto Exp $	*/

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
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/ip6.h>

#include <string.h>
#include <stdio.h>

size_t
inet6_rthdr_space(int type, int seg)
{
	switch (type) {
	case IPV6_RTHDR_TYPE_0:
		if (seg < 1 || seg > 23)
			return (0);
		return (CMSG_SPACE(sizeof(struct in6_addr) * seg +
		    sizeof(struct ip6_rthdr0)));
	default:
		return (0);
	}
}

struct cmsghdr *
inet6_rthdr_init(void *bp, int type)
{
	struct cmsghdr *ch = (struct cmsghdr *)bp;
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(ch);

	ch->cmsg_level = IPPROTO_IPV6;
	ch->cmsg_type = IPV6_RTHDR;

	switch (type) {
	case IPV6_RTHDR_TYPE_0:
		ch->cmsg_len = CMSG_LEN(sizeof(struct ip6_rthdr0));
		bzero(rthdr, sizeof(struct ip6_rthdr0));
		rthdr->ip6r_type = IPV6_RTHDR_TYPE_0;
		return (ch);
	default:
		return (NULL);
	}
}

int
inet6_rthdr_add(struct cmsghdr *cmsg, const struct in6_addr *addr, u_int flags)
{
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
		if (flags != IPV6_RTHDR_LOOSE)
			return (-1);
		if (rt0->ip6r0_segleft == 23)
			return (-1);
		rt0->ip6r0_segleft++;
		bcopy(addr, (caddr_t)rt0 + ((rt0->ip6r0_len + 1) << 3),
		    sizeof(struct in6_addr));
		rt0->ip6r0_len += sizeof(struct in6_addr) >> 3;
		cmsg->cmsg_len = CMSG_LEN((rt0->ip6r0_len + 1) << 3);
		break;
	}
	default:
		return (-1);
	}

	return (0);
}

int
inet6_rthdr_lasthop(struct cmsghdr *cmsg, unsigned int flags)
{
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
		if (flags != IPV6_RTHDR_LOOSE)
			return (-1);
		if (rt0->ip6r0_segleft > 23)
			return (-1);
		break;
	}
	default:
		return (-1);
	}

	return (0);
}

#if 0
int
inet6_rthdr_reverse(in, out)
	const struct cmsghdr *in;
	struct cmsghdr *out;
{

	return (-1);
}
#endif

int
inet6_rthdr_segments(const struct cmsghdr *cmsg)
{
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return (-1);

		return (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
	}

	default:
		return (-1);
	}
}

struct in6_addr *
inet6_rthdr_getaddr(struct cmsghdr *cmsg, int index)
{
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
		int naddr;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return NULL;
		naddr = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
		if (index <= 0 || naddr < index)
			return NULL;
		return ((struct in6_addr *)(rt0 + 1)) + index;
	}

	default:
		return NULL;
	}
}

int
inet6_rthdr_getflags(const struct cmsghdr *cmsg, int index)
{
	struct ip6_rthdr *rthdr;

	rthdr = (struct ip6_rthdr *)CMSG_DATA(cmsg);

	switch (rthdr->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
	{
		struct ip6_rthdr0 *rt0 = (struct ip6_rthdr0 *)rthdr;
		int naddr;

		if (rt0->ip6r0_len % 2 || 46 < rt0->ip6r0_len)
			return (-1);
		naddr = (rt0->ip6r0_len * 8) / sizeof(struct in6_addr);
		if (index < 0 || naddr < index)
			return (-1);
		return IPV6_RTHDR_LOOSE;
	}

	default:
		return (-1);
	}
}
