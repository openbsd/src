/*	$OpenBSD: qdisc_red.c,v 1.1.1.1 2001/06/27 18:23:21 kjc Exp $	*/
/*	$KAME: qdisc_red.c,v 1.2 2000/10/18 09:15:17 kjc Exp $	*/
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
red_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct red_stats red_stats;
	struct timeval cur_time, last_time;
	u_int64_t last_bytes;
	double sec;
	int cnt = count;
	
	strcpy(red_stats.iface.red_ifname, ifname);

	gettimeofday(&last_time, NULL);
	last_time.tv_sec -= interval;
	last_bytes = 0;

	while (count == 0 || cnt-- > 0) {
	
		if (ioctl(fd, RED_GETSTATS, &red_stats) < 0)
			err(1, "ioctl RED_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		printf(" weight:%d inv_pmax:%d qthresh:(%d,%d)\n",
		       red_stats.weight,  red_stats.inv_pmax,
		       red_stats.th_min,  red_stats.th_max);
		printf(" q_len:%d (avg: %.2f), q_limit:%d\n",
		       red_stats.q_len,
		       ((double)red_stats.q_avg)/(double)avg_scale,
		       red_stats.q_limit);
		printf(" xmit:%llu pkts, drop:%llu pkts (forced: %u, early: %u)\n",
		       (ull)red_stats.xmit_cnt.packets,
		       (ull)red_stats.drop_cnt.packets,
		       red_stats.drop_forced, red_stats.drop_unforced);
		if (red_stats.marked_packets != 0)
			printf(" marked: %u\n", red_stats.marked_packets);
		printf(" throughput: %sbps\n",
		       rate2str(calc_rate(red_stats.xmit_cnt.bytes,
					  last_bytes, sec)));
		if (red_stats.fv_alloc > 0) {
			printf(" flowvalve: alloc:%u flows:%u\n",
			       red_stats.fv_alloc, red_stats.fv_flows);
			printf("  predrop:%u pass:%u escape:%u\n",
			       red_stats.fv_predrop, red_stats.fv_pass,
			       red_stats.fv_escape);
		}
		printf("\n");

		last_bytes = red_stats.xmit_cnt.bytes;
		last_time = cur_time;
		sleep(interval);
	}
}

int
print_redstats(struct redstats *rp)
{
	printf("     RED q_avg:%.2f xmit:%llu (forced:%u early:%u marked:%u)\n",
	       ((double)rp->q_avg)/(double)avg_scale,
	       (ull)rp->xmit_cnt.packets, 
	       rp->drop_forced,
	       rp->drop_unforced,
	       rp->marked_packets);
	return 0;
}
