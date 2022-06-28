/*	$OpenBSD: route.c,v 1.109 2022/06/28 15:17:23 bluhm Exp $	*/
/*	$NetBSD: route.c,v 1.15 1996/05/07 02:55:06 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_types.h>
#define _KERNEL
#include <net/route.h>
#undef _KERNEL
#include <netinet/ip_ipsp.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/sysctl.h>

#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "netstat.h"

static union {
	struct		sockaddr u_sa;
	u_int32_t	u_data[64];
	int		u_dummy;	/* force word-alignment */
} pt_u;

struct	rtentry rtentry;

static struct sockaddr *kgetsa(struct sockaddr *);
static struct sockaddr *plentosa(sa_family_t, int, struct sockaddr *);
static struct art_node *getdefault(struct art_table *);
static void p_table(struct art_table *);
static void p_artnode(struct art_node *);
static void p_krtentry(struct rtentry *);

/*
 * Print routing tables.
 */
void
routepr(u_long afmap, u_long af2idx, u_long af2idx_max, u_int tableid)
{
	struct art_root ar;
	struct art_node *node;
	struct srp *afm_head, *afm;
	struct {
		unsigned int	limit;
		void	      **tbl;
	} map;
	void **tbl;
	int i;
	uint8_t af2i[AF_MAX+1];
	uint8_t af2i_max;

	printf("Routing tables\n");

	if (afmap == 0 || af2idx == 0 || af2idx_max == 0) {
		printf("symbol not in namelist\n");
		return;
	}

	kread(afmap, &afm_head, sizeof(afm_head));
	kread(af2idx, af2i, sizeof(af2i));
	kread(af2idx_max, &af2i_max, sizeof(af2i_max));

	if ((afm = calloc(af2i_max + 1, sizeof(*afm))) == NULL)
		err(1, NULL);

	kread((u_long)afm_head, afm, (af2i_max + 1) * sizeof(*afm));

	for (i = 1; i <= AF_MAX; i++) {
		if (af != AF_UNSPEC && af != i)
			continue;
		if (af2i[i] == 0 || afm[af2i[i]].ref == NULL)
			continue;

		kread((u_long)afm[af2i[i]].ref, &map, sizeof(map));
		if (tableid >= map.limit)
			continue;

		if ((tbl = calloc(map.limit, sizeof(*tbl))) == NULL)
			err(1, NULL);

		kread((u_long)map.tbl, tbl, map.limit * sizeof(*tbl));
		if (tbl[tableid] == NULL)
			continue;

		kread((u_long)tbl[tableid], &ar, sizeof(ar));

		free(tbl);

		if (ar.ar_root.ref == NULL)
			continue;

		pr_family(i);
		pr_rthdr(i, Aflag);

		node = getdefault(ar.ar_root.ref);
		if (node != NULL)
			p_artnode(node);

		p_table(ar.ar_root.ref);
	}

	free(afm);
}

static struct sockaddr *
kgetsa(struct sockaddr *dst)
{

	kread((u_long)dst, &pt_u.u_sa, sizeof(pt_u.u_sa));
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static struct sockaddr *
plentosa(sa_family_t af, int plen, struct sockaddr *sa_mask)
{
	struct sockaddr_in *sin = (struct sockaddr_in *)sa_mask;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa_mask;
	uint8_t *p;
	int i;

	if (plen < 0)
		return (NULL);

	memset(sa_mask, 0, sizeof(struct sockaddr_storage));

	switch (af) {
	case AF_INET:
		if (plen > 32)
			return (NULL);
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(struct sockaddr_in);
		memset(&sin->sin_addr, 0, sizeof(sin->sin_addr));
		p = (uint8_t *)&sin->sin_addr;
		break;
	case AF_INET6:
		if (plen > 128)
			return (NULL);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		memset(&sin6->sin6_addr.s6_addr, 0, sizeof(sin6->sin6_addr.s6_addr));
		p = sin6->sin6_addr.s6_addr;
		break;
	default:
		return (NULL);
	}

	for (i = 0; i < plen / 8; i++)
		p[i] = 0xff;
	if (plen % 8)
		p[i] = (0xff00 >> (plen % 8)) & 0xff;

	return (sa_mask);
}

static struct art_node *
getdefault(struct art_table *at)
{
	struct art_node *node;
	struct art_table table;
	union {
		struct srp		 node;
		unsigned long		 count;
	} *heap;

	kread((u_long)at, &table, sizeof(table));
	heap = calloc(1, AT_HEAPSIZE(table.at_bits));
	kread((u_long)table.at_heap, heap, AT_HEAPSIZE(table.at_bits));

	node = heap[1].node.ref;

	free(heap);

	return (node);
}

static void
p_table(struct art_table *at)
{
	struct art_node *next, *node;
	struct art_table *nat, table;
	union {
		struct srp		 node;
		unsigned long		 count;
	} *heap;
	int i, j;

	kread((u_long)at, &table, sizeof(table));
	heap = calloc(1, AT_HEAPSIZE(table.at_bits));
	kread((u_long)table.at_heap, heap, AT_HEAPSIZE(table.at_bits));

	for (j = 1; j < table.at_minfringe; j += 2) {
		for (i = (j > 2) ? j : 2; i < table.at_minfringe; i <<= 1) {
			next = heap[i >> 1].node.ref;
			node = heap[i].node.ref;
			if (node != NULL && node != next)
				p_artnode(node);
		}
	}

	for (i = table.at_minfringe; i < table.at_minfringe << 1; i++) {
		next = heap[i >> 1].node.ref;
		node = heap[i].node.ref;
		if (!ISLEAF(node)) {
			nat = SUBTABLE(node);
			node = getdefault(nat);
		} else
			nat = NULL;

		if (node != NULL && node != next)
			p_artnode(node);

		if (nat != NULL)
			p_table(nat);
	}

	free(heap);
}

static void
p_artnode(struct art_node *an)
{
	struct art_node node;
	struct rtentry *rt;

	kread((u_long)an, &node, sizeof(node));
	rt = node.an_rtlist.sl_head.ref;

	while (rt != NULL) {
		kread((u_long)rt, &rtentry, sizeof(rtentry));
		if (Aflag)
			printf("%-16p ", rt);
		p_krtentry(&rtentry);
		rt = rtentry.rt_next.se_next.ref;
	}
}

static void
p_krtentry(struct rtentry *rt)
{
	struct sockaddr_storage sock1, sock2;
	struct sockaddr *sa = (struct sockaddr *)&sock1;
	struct sockaddr *mask = (struct sockaddr *)&sock2;

	bcopy(kgetsa(rt_key(rt)), sa, sizeof(struct sockaddr));
	if (sa->sa_len > sizeof(struct sockaddr))
		bcopy(kgetsa(rt_key(rt)), sa, sa->sa_len);

	if (sa->sa_family == PF_KEY) {
		/* Ignore PF_KEY entries */
		return;
	}

	mask = plentosa(sa->sa_family, rt_plen(rt), mask);

	p_addr(sa, mask, rt->rt_flags);
	p_gwaddr(kgetsa(rt->rt_gateway), sa->sa_family);
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%5u %8lld ", rt->rt_refcnt.r_refs - 1, rt->rt_use);
	if (rt->rt_rmx.rmx_mtu)
		printf("%5u ", rt->rt_rmx.rmx_mtu);
	else
		printf("%5s ", "-");
	putchar((rt->rt_rmx.rmx_locks & RTV_MTU) ? 'L' : ' ');
	printf("  %2d", rt->rt_priority & RTP_MASK);

	if (rt->rt_ifidx != 0)
		printf(" if%d", rt->rt_ifidx);
	putchar('\n');
	if (vflag)
		printf("\texpire   %10lld%c\n",
		    (long long)rt->rt_rmx.rmx_expire,
		    (rt->rt_rmx.rmx_locks & RTV_EXPIRE) ? 'L' : ' ');
}

/*
 * Print routing statistics
 */
void
rt_stats(void)
{
	struct rtstat rtstat;
	int mib[6];
	size_t size;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_STATS;
	mib[5] = 0;
	size = sizeof (rtstat);

	if (sysctl(mib, 6, &rtstat, &size, NULL, 0) == -1) {
		perror("sysctl of routing table statistics");
		exit(1);
	}

	printf("routing:\n");
	printf("\t%u bad routing redirect%s\n",
	    rtstat.rts_badredirect, plural(rtstat.rts_badredirect));
	printf("\t%u dynamically created route%s\n",
	    rtstat.rts_dynamic, plural(rtstat.rts_dynamic));
	printf("\t%u new gateway%s due to redirects\n",
	    rtstat.rts_newgateway, plural(rtstat.rts_newgateway));
	printf("\t%u destination%s found unreachable\n",
	    rtstat.rts_unreach, plural(rtstat.rts_unreach));
	printf("\t%u use%s of a wildcard route\n",
	    rtstat.rts_wildcard, plural(rtstat.rts_wildcard));
}

/*
 * Print rdomain and rtable summary
 */

void
rdomainpr(void)
{
	struct if_data		 *ifd;
	struct ifaddrs		 *ifap, *ifa;
	struct rt_tableinfo	  info;

	int	 rtt_dom[RT_TABLEID_MAX+1];
	int	 rdom_rttcnt[RT_TABLEID_MAX+1] = { };
	int	 mib[6], rdom, rtt;
	size_t	 len;
	char	*old, *rdom_if[RT_TABLEID_MAX+1] = { };

	getifaddrs(&ifap);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL ||
		    ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		ifd = ifa->ifa_data;
		if (rdom_if[ifd->ifi_rdomain] == NULL) {
			if (asprintf(&rdom_if[ifd->ifi_rdomain], "%s",
			    ifa->ifa_name) == -1)
				exit(1);
		} else {
			old = rdom_if[ifd->ifi_rdomain];
			if (asprintf(&rdom_if[ifd->ifi_rdomain], "%s %s",
			    old, ifa->ifa_name) == -1)
				exit(1);
			free(old);
		}
	}
	freeifaddrs(ifap);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_TABLE;

	len = sizeof(info);
	for (rtt = 0; rtt <= RT_TABLEID_MAX; rtt++) {
		mib[5] = rtt;
		if (sysctl(mib, 6, &info, &len, NULL, 0) == -1)
			rtt_dom[rtt] = -1;
		else {
			rtt_dom[rtt] = info.rti_domainid;
			rdom_rttcnt[info.rti_domainid]++;
		}
	}

	for (rdom = 0; rdom <= RT_TABLEID_MAX; rdom++) {
		if (rdom_if[rdom] == NULL)
			continue;
		printf("Rdomain %i\n", rdom);
		printf("  Interface%s %s\n",
		    (strchr(rdom_if[rdom], ' ') == NULL) ? ":" : "s:",
		    rdom_if[rdom]);
		printf("  Routing table%s",
		    (rdom_rttcnt[rdom] == 1) ? ":" : "s:");
		for (rtt = 0; rtt <= RT_TABLEID_MAX; rtt++) {
			if (rtt_dom[rtt] == rdom)
				printf(" %i", rtt);
		}
		printf("\n\n");
		free(rdom_if[rdom]);
	}
}
