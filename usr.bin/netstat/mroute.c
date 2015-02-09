/*	$OpenBSD: mroute.c,v 1.24 2015/02/09 12:25:03 claudio Exp $	*/
/*	$NetBSD: mroute.c,v 1.10 1996/05/11 13:51:27 mycroft Exp $	*/

/*
 * Copyright (c) 1989 Stephen Deering
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *
 *	from: @(#)mroute.c	8.1 (Berkeley) 6/6/93
 */

/*
 * Print multicast routing structures and statistics.
 *
 * MROUTING 1.0
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/igmp.h>
#include <netinet/ip_mroute.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <util.h>
#include "netstat.h"

void
mroutepr(void)
{
	u_int mrtproto;
	struct vifinfo *v;
	struct mfcinfo *m;
	size_t needed, numvifs, nummfcs, vifi, mfci;
	char *buf = NULL;
	char fmtbuf[FMT_SCALED_STRSIZE];
	vifi_t maxvif = 0;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_MRTPROTO };
	size_t len = sizeof(int);
	int banner_printed = 0, saved_nflag;

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &mrtproto, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("mroute");
		return;
	}
	switch (mrtproto) {
	case 0:
		printf("no multicast routing compiled into this system\n");
		return;
	case IGMP_DVMRP:
		break;
	default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	saved_nflag = nflag;
	nflag = 1;

	mib[3] = IPCTL_MRTVIF;
	needed = get_sysctl(mib, sizeof(mib) / sizeof(mib[0]), &buf);
	numvifs = needed / sizeof(*v);
	v = (struct vifinfo *)buf;
	if (numvifs)
		maxvif = v[numvifs - 1].v_vifi;

	for (vifi = 0; vifi < numvifs; ++vifi, ++v) {
		if (!banner_printed) {
			printf("\nVirtual Interface Table\n %s%s",
			    "Vif  Thresh  Limit  Local-Address    ",
			    "Remote-Address   Pkt_in  Pkt_out\n");
			banner_printed = 1;
		}

		printf(" %3u     %3u  %-15.15s",
		    v->v_vifi, v->v_threshold,
		    routename4(v->v_lcl_addr.s_addr));
		printf("  %-15.15s  %6lu  %7lu\n", (v->v_flags & VIFF_TUNNEL) ?
		    routename4(v->v_rmt_addr.s_addr) : "",
		    v->v_pkt_in, v->v_pkt_out);
	}
	if (!banner_printed)
		printf("Virtual Interface Table is empty\n");

	banner_printed = 0;

	mib[3] = IPCTL_MRTMFC;
	needed = get_sysctl(mib, sizeof(mib) / sizeof(mib[0]), &buf);
	nummfcs = needed / sizeof(*m);
	m = (struct mfcinfo *)buf;

	for (mfci = 0; mfci < nummfcs; ++mfci, ++m) {
		if (!banner_printed) {
			printf("\nMulticast Forwarding Cache\n %s%s",
			    "Hash  Origin           Mcastgroup       ",
			    "Traffic  In-Vif  Out-Vifs/Forw-ttl\n");
			banner_printed = 1;
		}

		printf("  %3zu  %-15.15s",
		    mfci, routename4(m->mfc_origin.s_addr));
		fmt_scaled(m->mfc_pkt_cnt, fmtbuf);
		printf("  %-15.15s  %7s     %3u ",
		    routename4(m->mfc_mcastgrp.s_addr),
		    buf, m->mfc_parent);
		for (vifi = 0; vifi <= maxvif; ++vifi)
			if (m->mfc_ttls[vifi])
				printf(" %zu/%u", vifi, m->mfc_ttls[vifi]);

		printf("\n");
	}
	if (!banner_printed)
		printf("Multicast Forwarding Cache is empty\n");
	else
		printf("\nTotal no. of entries in cache: %zu\n", nummfcs);

	printf("\n");
	nflag = saved_nflag;
}

void
mrt_stats(void)
{
	u_int mrtproto;
	struct mrtstat mrtstat;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_MRTPROTO };
	int mib2[] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_MRTSTATS };
	size_t len = sizeof(int);

	if (sysctl(mib, sizeof(mib) / sizeof(mib[0]),
	    &mrtproto, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("mroute");
		return;
	}
	switch (mrtproto) {
	case 0:
		printf("no multicast routing compiled into this system\n");
		return;

	case IGMP_DVMRP:
		break;

	default:
		printf("multicast routing protocol %u, unknown\n", mrtproto);
		return;
	}

	len = sizeof(mrtstat);
	if (sysctl(mib2, sizeof(mib2) / sizeof(mib2[0]),
	    &mrtstat, &len, NULL, 0) == -1) {
		if (errno != ENOPROTOOPT)
			warn("mroute");
		return;
	}

	printf("multicast routing:\n");
	printf("\t%lu datagram%s with no route for origin\n",
	    mrtstat.mrts_no_route, plural(mrtstat.mrts_no_route));
	printf("\t%lu upcall%s made to mrouted\n",
	    mrtstat.mrts_upcalls, plural(mrtstat.mrts_upcalls));
	printf("\t%lu datagram%s with malformed tunnel options\n",
	    mrtstat.mrts_bad_tunnel, plural(mrtstat.mrts_bad_tunnel));
	printf("\t%lu datagram%s with no room for tunnel options\n",
	    mrtstat.mrts_cant_tunnel, plural(mrtstat.mrts_cant_tunnel));
	printf("\t%lu datagram%s arrived on wrong interface\n",
	    mrtstat.mrts_wrong_if, plural(mrtstat.mrts_wrong_if));
	printf("\t%lu datagram%s dropped due to upcall Q overflow\n",
	    mrtstat.mrts_upq_ovflw, plural(mrtstat.mrts_upq_ovflw));
	printf("\t%lu datagram%s dropped due to upcall socket overflow\n",
	    mrtstat.mrts_upq_sockfull, plural(mrtstat.mrts_upq_sockfull));
	printf("\t%lu datagram%s cleaned up by the cache\n",
	    mrtstat.mrts_cache_cleanups, plural(mrtstat.mrts_cache_cleanups));
	printf("\t%lu datagram%s dropped selectively by ratelimiter\n",
	    mrtstat.mrts_drop_sel, plural(mrtstat.mrts_drop_sel));
	printf("\t%lu datagram%s dropped - bucket Q overflow\n",
	    mrtstat.mrts_q_overflow, plural(mrtstat.mrts_q_overflow));
	printf("\t%lu datagram%s dropped - larger than bkt size\n",
	    mrtstat.mrts_pkt2large, plural(mrtstat.mrts_pkt2large));
}
