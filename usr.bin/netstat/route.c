/*	$OpenBSD: route.c,v 1.101 2016/09/15 01:01:07 dlg Exp $	*/
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
#include <sys/select.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
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

#include "netstat.h"

/* alignment constraint for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

struct radix_node_head ***rt_head;
struct radix_node_head ***rnt;
struct radix_node_head *rt_tables[AF_MAX+1];	/* provides enough space */
u_int8_t		  af2rtafidx[AF_MAX+1];

static union {
	struct		sockaddr u_sa;
	u_int32_t	u_data[64];
	int		u_dummy;	/* force word-alignment */
} pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

static struct sockaddr *kgetsa(struct sockaddr *);
static void p_tree(struct radix_node *);
static void p_rtnode(void);
static void p_rtflags(u_char);
static void p_krtentry(struct rtentry *);

/*
 * Print routing tables.
 */
void
routepr(u_long rtree, u_long mtree, u_long af2idx, u_long rtbl_id_max,
    u_int tableid)
{
	struct radix_node_head *rnh, head;
	int i, idxmax = 0;
	u_int rtidxmax;

	printf("Routing tables\n");

	if (rtree == 0 || af2idx == 0) {
		printf("rt_tables: symbol not in namelist\n");
		return;
	}

	kread((u_long)rtree, &rt_head, sizeof(rt_head));
	kread((u_long)rtbl_id_max, &rtidxmax, sizeof(rtidxmax));
	kread((long)af2idx, &af2rtafidx, sizeof(af2rtafidx));

	for (i = 0; i <= AF_MAX; i++) {
		if (af2rtafidx[i] > idxmax)
			idxmax = af2rtafidx[i];
	}

	if ((rnt = calloc(rtidxmax + 1, sizeof(struct radix_node_head **))) ==
	    NULL)
		err(1, NULL);

	kread((u_long)rt_head, rnt, (rtidxmax + 1) *
	    sizeof(struct radix_node_head **));
	if (tableid > rtidxmax || rnt[tableid] == NULL) {
		printf("Bad table %u\n", tableid);
		return;
	}
	kread((u_long)rnt[tableid], rt_tables, (idxmax + 1) * sizeof(rnh));

	for (i = 0; i <= AF_MAX; i++) {
		if (i == AF_UNSPEC) {
			if (Aflag && (af == AF_UNSPEC || af == 0xff)) {
				kread(mtree, &rnh, sizeof(rnh));
				kread((u_long)rnh, &head, sizeof(head));
				printf("Netmasks:\n");
				p_tree(head.rnh_treetop);
			}
			continue;
		}
		if (af2rtafidx[i] == 0)
			/* no table for this AF */
			continue;
		if ((rnh = rt_tables[af2rtafidx[i]]) == NULL)
			continue;
		kread((u_long)rnh, &head, sizeof(head));
		if (af == AF_UNSPEC || af == i) {
			pr_family(i);
			do_rtent = 1;
			pr_rthdr(i, Aflag);
			p_tree(head.rnh_treetop);
		}
	}
}

static struct sockaddr *
kgetsa(struct sockaddr *dst)
{

	kread((u_long)dst, &pt_u.u_sa, sizeof(pt_u.u_sa));
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static void
p_tree(struct radix_node *rn)
{

again:
	kread((u_long)rn, &rnode, sizeof(rnode));
	if (rnode.rn_b < 0) {
		if (Aflag)
			printf("%-16p ", rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kread((u_long)rn, &rtentry, sizeof(rtentry));
			p_krtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
			    0, 0, 44);
			putchar('\n');
		}
		if ((rn = rnode.rn_dupedkey))
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-16p ", rn);
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}

static void
p_rtflags(u_char flags)
{
	putchar('<');
	if (flags & RNF_NORMAL)
		putchar('N');
	if (flags & RNF_ROOT)
		putchar('R');
	if (flags & RNF_ACTIVE)
		putchar('A');
	if (flags & ~(RNF_NORMAL | RNF_ROOT | RNF_ACTIVE))
		printf("/0x%02x", flags);
	putchar('>');
}

char	nbuf[25];

static void
p_rtnode(void)
{
	struct radix_mask *rm = rnode.rn_mklist;

	if (rnode.rn_b < 0) {
		snprintf(nbuf, sizeof nbuf, " => %p", rnode.rn_dupedkey);
		printf("\t  (%p)%s", rnode.rn_p, rnode.rn_dupedkey ? nbuf : "");
		if (rnode.rn_mask) {
			printf(" mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
			    0, 0, -1);
		} else if (rm == NULL) {
			putchar('\n');
			return;
		}
	} else {
		snprintf(nbuf, sizeof nbuf, "(%d)", rnode.rn_b);
		printf("%6.6s (%p) %16p : %16p", nbuf,
		    rnode.rn_p, rnode.rn_l, rnode.rn_r);
	}

	putchar(' ');
	p_rtflags(rnode.rn_flags);

	while (rm) {
		kread((u_long)rm, &rmask, sizeof(rmask));
		snprintf(nbuf, sizeof nbuf, " %d refs, ", rmask.rm_refs);
		printf("\n\tmk = %p {(%d),%s", rm, -1 - rmask.rm_b,
		    rmask.rm_refs ? nbuf : " ");
		p_rtflags(rmask.rm_flags);
		printf(", ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;

			printf("leaf = %p ", rmask.rm_leaf);
			kread((u_long)rmask.rm_leaf, &rnode_aux, sizeof(rnode_aux));
			p_sockaddr(kgetsa((struct sockaddr *)rnode_aux.rn_mask),
			    0, 0, -1);
		} else
			p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask),
			    0, 0, -1);
		putchar('}');
		if ((rm = rmask.rm_mklist))
			printf(" ->");
	}
	putchar('\n');
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

	if (rt_mask(rt)) {
		bcopy(kgetsa(rt_mask(rt)), mask, sizeof(struct sockaddr));
		if (sa->sa_len > sizeof(struct sockaddr))
			bcopy(kgetsa(rt_mask(rt)), mask, sa->sa_len);
	} else
		mask = 0;

	p_addr(sa, mask, rt->rt_flags);
	p_gwaddr(kgetsa(rt->rt_gateway), sa->sa_family);
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%5u %8lld ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_rmx.rmx_mtu)
		printf("%5u ", rt->rt_rmx.rmx_mtu);
	else
		printf("%5s ", "-");
	putchar((rt->rt_rmx.rmx_locks & RTV_MTU) ? 'L' : ' ');
	printf("  %2d", rt->rt_priority);

	if (rt->rt_ifidx != 0) {
		printf(" if%d%s", rt->rt_ifidx,
		    rt->rt_nodes[0].rn_dupedkey ? " =>" : "");
	}
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

	if (sysctl(mib, 6, &rtstat, &size, NULL, 0) < 0) {
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
