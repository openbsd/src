/*	$OpenBSD: qdisc_rio.c,v 1.1.1.1 2001/06/27 18:23:21 kjc Exp $	*/
/*	$KAME: qdisc_rio.c,v 1.3 2001/05/17 08:01:47 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <altq/altq.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>

#include "altqstat.h"

static int avg_scale = 4096;	/* default fixed-point scale */

void
rio_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct rio_stats rio_stats;
	struct timeval cur_time, last_time;
	u_int64_t last_bytes[3];
	double sec;
	int cnt = count;
	
	bzero(&rio_stats, sizeof(rio_stats));
	strcpy(rio_stats.iface.rio_ifname, ifname);

	gettimeofday(&last_time, NULL);
	last_time.tv_sec -= interval;

	while (count == 0 || cnt-- > 0) {
	
		if (ioctl(fd, RIO_GETSTATS, &rio_stats) < 0)
			err(1, "ioctl RIO_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		printf("weight:%d q_limit:%d\n",
		       rio_stats.weight, rio_stats.q_limit);

		printf("\t\t\tLOW DP\t\tMEDIUM DP\t\tHIGH DP\n");

		printf("thresh (prob):\t\t[%d,%d](1/%d)\t[%d,%d](1/%d)\t\t[%d,%d](%d)\n",
		       rio_stats.q_params[0].th_min,
		       rio_stats.q_params[0].th_max,
		       rio_stats.q_params[0].inv_pmax,
		       rio_stats.q_params[1].th_min,
		       rio_stats.q_params[1].th_max,
		       rio_stats.q_params[1].inv_pmax,
		       rio_stats.q_params[2].th_min,
		       rio_stats.q_params[2].th_max,
		       rio_stats.q_params[2].inv_pmax);
		printf("qlen (avg):\t\t%d (%.2f)\t%d (%.2f)\t\t%d (%.2f)\n",
		       rio_stats.q_len[0],
		       ((double)rio_stats.q_stats[0].q_avg)/(double)avg_scale,
		       rio_stats.q_len[1],
		       ((double)rio_stats.q_stats[1].q_avg)/(double)avg_scale,
		       rio_stats.q_len[2],
		       ((double)rio_stats.q_stats[2].q_avg)/(double)avg_scale);
		printf("xmit (drop) pkts:\t%llu (%llu)\t\t%llu (%llu)\t\t\t%llu (%llu)\n",
		       (ull)rio_stats.q_stats[0].xmit_cnt.packets,
		       (ull)rio_stats.q_stats[0].drop_cnt.packets,
		       (ull)rio_stats.q_stats[1].xmit_cnt.packets,
		       (ull)rio_stats.q_stats[1].drop_cnt.packets,
		       (ull)rio_stats.q_stats[2].xmit_cnt.packets,
		       (ull)rio_stats.q_stats[2].drop_cnt.packets);
		printf("(forced:early):\t\t(%u:%u)\t\t(%u:%u)\t\t\t(%u:%u)\n",
		       rio_stats.q_stats[0].drop_forced,
		       rio_stats.q_stats[0].drop_unforced,
		       rio_stats.q_stats[1].drop_forced,
		       rio_stats.q_stats[1].drop_unforced,
		       rio_stats.q_stats[2].drop_forced,
		       rio_stats.q_stats[2].drop_unforced);
		if (rio_stats.q_stats[0].marked_packets != 0
		    || rio_stats.q_stats[1].marked_packets != 0
		    || rio_stats.q_stats[2].marked_packets != 0)
			printf("marked:\t\t\t%u\t\t%u\t\t\t%u\n",
			       rio_stats.q_stats[0].marked_packets,
			       rio_stats.q_stats[1].marked_packets,
			       rio_stats.q_stats[2].marked_packets);
		printf("throughput:\t\t%sbps\t%sbps\t\t%sbps\n\n",
		       rate2str(calc_rate(rio_stats.q_stats[0].xmit_cnt.bytes,
					  last_bytes[0], sec)),
		       rate2str(calc_rate(rio_stats.q_stats[1].xmit_cnt.bytes,
					  last_bytes[1], sec)),
		       rate2str(calc_rate(rio_stats.q_stats[2].xmit_cnt.bytes,
					  last_bytes[2], sec)));

		last_bytes[0] = rio_stats.q_stats[0].xmit_cnt.bytes;
		last_bytes[1] = rio_stats.q_stats[1].xmit_cnt.bytes;
		last_bytes[2] = rio_stats.q_stats[2].xmit_cnt.bytes;
		last_time = cur_time;
		sleep(interval);
	}
}

int
print_riostats(struct redstats *rp)
{
	int dp;

	for (dp = 0; dp < RIO_NDROPPREC; dp++)
		printf("     RIO[%d] q_avg:%.2f xmit:%llu (forced: %u early:%u marked:%u)\n",
		       dp,
		       ((double)rp[dp].q_avg)/(double)avg_scale,
		       (ull)rp[dp].xmit_cnt.packets, 
		       rp[dp].drop_forced,
		       rp[dp].drop_unforced,
		       rp[dp].marked_packets);
	return 0;
}
