/*      $OpenBSD: route_src.c,v 1.1 2004/06/06 16:49:09 cedric Exp $ */

/*
 * Copyright (c) 2004 Cedric Berger <cedric@berger.to>
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
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/socket.h>

#include <net/route.h>
#include <netinet/in.h>

void		 mask_set(void *, struct sockaddr *, int, int);
struct sockaddr *mask_trim(struct sockaddr *, int, int);

/*
 * verify that the netmask is NULL or has all 1s for the destination part.
 * setup a corrected mask otherwise.
 */
void
sroute_verify_host(struct rt_addrinfo *ai)
{
	static struct sockaddr_rtin netmask4;

	if (ai->rti_info[RTAX_NETMASK] == NULL)
		return;	/* no mask is a host mask */
	if (ai->rti_info[RTAX_DST]->sa_family == AF_INET)
		goto _inet;
	/* just clear the netmask */
	ai->rti_info[RTAX_NETMASK] = NULL;
	return;

_inet:
	if (satortin(ai->rti_info[RTAX_NETMASK])->rtin_dst.s_addr ==
	    (in_addr_t)-1)
		return; /* all 1s is a host mask */
	netmask4.rtin_dst.s_addr = (in_addr_t)-1;
	netmask4.rtin_src = satortin(ai->rti_info[RTAX_NETMASK])->rtin_src;
	ai->rti_info[RTAX_NETMASK] = mask_trim(sintosa(&netmask4),
	    offsetof(struct sockaddr_rtin, rtin_dst), 8);
}

/*
 * set the netmask of a cloned route. take the source part of the
 * old mask and use the genmask (or NULL) for the destination part.
 */
void
sroute_clone_route(struct rt_addrinfo *ai, struct sockaddr *oldmask,
    struct sockaddr *genmask)
{
	if (ai->rti_info[RTAX_DST]->sa_family == AF_INET)
		goto _inet;
	if ((ai->rti_info[RTAX_NETMASK] = genmask) == NULL)
		ai->rti_flags |= RTF_HOST;
	return;

_inet:
	ai->rti_info[RTAX_NETMASK] = sroute_clone_mask4(oldmask, genmask);
	if (ai->rti_info[RTAX_NETMASK] == NULL || satortin(
	    ai->rti_info[RTAX_NETMASK])->rtin_dst.s_addr == (in_addr_t)-1)
		ai->rti_flags |= RTF_HOST;
}

/*
 * get the netmask of an IPv4 cloned route. take the source part of the
 * old mask and use the genmask (or NULL) for the destination part.
 */
struct sockaddr *
sroute_clone_mask4(struct sockaddr *oldmask, struct sockaddr *genmask)
{
	static struct sockaddr_rtin netmask4;

	mask_set(&netmask4.rtin_dst, genmask,
	    offsetof(struct sockaddr_in, sin_addr), 4);
	mask_set(&netmask4.rtin_src, oldmask,
	    offsetof(struct sockaddr_rtin, rtin_src), 4);
	return mask_trim(sintosa(&netmask4),
	    offsetof(struct sockaddr_rtin, rtin_dst), 8);
}

/*
 * userland provides RTAX_SRC and RTAX_SRCMASK entries, but the kernel wants
 * that info packed inside RTAX_DST and RTAX_NETMASK themselves.
 */ 
void
sroute_compact(struct rt_addrinfo *ai, int type)
{
	static struct sockaddr_rtin dst4 = { sizeof(dst4), AF_INET };
	static struct sockaddr_rtin netmask4;

	if (ai->rti_info[RTAX_DST]->sa_family == AF_INET)
		goto _inet;
	return;

_inet:
	dst4.rtin_dst = satosin(ai->rti_info[RTAX_DST])->sin_addr;
	mask_set(&netmask4.rtin_dst, ai->rti_info[RTAX_NETMASK],
	    offsetof(struct sockaddr_in, sin_addr), 4);
	if (ai->rti_info[RTAX_SRC] != NULL) {
		dst4.rtin_src = satosin(ai->rti_info[RTAX_SRC])->sin_addr;
		mask_set(&netmask4.rtin_src, ai->rti_info[RTAX_SRCMASK],
		    offsetof(struct sockaddr_in, sin_addr), 4);
	} else
		dst4.rtin_src.s_addr = netmask4.rtin_src.s_addr = 0;
	ai->rti_info[RTAX_DST] = (struct sockaddr *)&dst4;
	/*
	 * do not generate a netmask artificially for RTM_GET or it
	 * will break the loose-matching behaviour that is expected.
	 */
	if (type != RTM_GET || ai->rti_info[RTAX_NETMASK] != NULL ||
	    ai->rti_info[RTAX_SRCMASK] != NULL)
		ai->rti_info[RTAX_NETMASK] = mask_trim(sintosa(&netmask4),
		    offsetof(struct sockaddr_rtin, rtin_dst), 8);
}

/*
 * opposite of sroute_compact, when sending a routing message to userland.
 */
void
sroute_expand(struct rt_addrinfo *ai)
{
	static struct sockaddr_in dst4 = { sizeof(dst4), AF_INET };
	static struct sockaddr_in src4 = { sizeof(src4), AF_INET };
	static struct sockaddr_in netmask4, srcmask4;

	if (ai->rti_info[RTAX_DST]->sa_family == AF_INET)
		goto _inet;
	return;

_inet:
	dst4.sin_addr = satortin(ai->rti_info[RTAX_DST])->rtin_dst;
	src4.sin_addr = satortin(ai->rti_info[RTAX_DST])->rtin_src;
	ai->rti_info[RTAX_DST] = sintosa(&dst4);
	ai->rti_info[RTAX_SRC] = sintosa(&src4);
	mask_set(&netmask4.sin_addr, ai->rti_info[RTAX_NETMASK],
	    offsetof(struct sockaddr_rtin, rtin_dst), 4);
	mask_set(&srcmask4.sin_addr, ai->rti_info[RTAX_NETMASK],
	    offsetof(struct sockaddr_rtin, rtin_src), 4);
	ai->rti_info[RTAX_NETMASK] = mask_trim(sintosa(&netmask4),
	    offsetof(struct sockaddr_in, sin_addr), 4);
	ai->rti_info[RTAX_SRCMASK] = mask_trim(sintosa(&srcmask4),
	    offsetof(struct sockaddr_in, sin_addr), 4);
	if (!src4.sin_addr.s_addr && !srcmask4.sin_addr.s_addr) {
		ai->rti_info[RTAX_SRC] = NULL;
		ai->rti_info[RTAX_SRCMASK] = NULL;
	}
}

/*
 * set a netmask from a potentially NULL or truncated mask.
 */
void
mask_set(void *mask, struct sockaddr *sa, int off, int sz)
{
	int i, len = off + sz;

	if (sa == NULL) {
		memset(mask, -1, sz);
		return;
	}
	bzero(mask, sz);
	for (i = off; i < len; i++) {
		if (i >= sa->sa_len)
			break;
		*((char *)mask)++ = ((char *)sa)[i];
	}
}

/*
 * set length and trim unused bytes from a netmask.
 * return NULL if the mask is all 1s (host route)
 */
struct sockaddr *
mask_trim(struct sockaddr *sa, int off, int sz)
{
	int i, len = off + sz;

	if (sa == NULL)
		return (NULL);
	for (i = len; i > 1 && ((char *)sa)[i-1] == 0; i--)
		;
	sa->sa_len = i;
	if (i < len)
		return (sa);
	for (i = off; i < len; i++)
		if (((char *)sa)[i] != (char)-1)
			break;
	return ((i < len) ? sa : NULL);
}
