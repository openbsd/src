/*	$OpenBSD: qdisc_blue.c,v 1.1.1.1 2001/06/27 18:23:19 kjc Exp $	*/
/*	$KAME: qdisc_blue.c,v 1.2 2000/10/18 09:15:16 kjc Exp $	*/
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
#include <altq/altq_blue.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>

#include "altqstat.h"

void
blue_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct blue_stats blue_stats;
	struct timeval cur_time, last_time;
	u_int64_t last_bytes;
	double sec;
	int cnt = count;

	strcpy(blue_stats.iface.blue_ifname, ifname);

	gettimeofday(&last_time, NULL);
	last_time.tv_sec -= interval;
	last_bytes = 0;

	while (count == 0 || cnt-- > 0) {
	
		if (ioctl(fd, BLUE_GETSTATS, &blue_stats) < 0)
			err(1, "ioctl BLUE_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		printf(" q_len:%d , q_limit:%d, q_pmark: %d\n",
		       blue_stats.q_len,  blue_stats.q_limit,
		       blue_stats.q_pmark);
		printf(" xmit: %llu pkts, drop: %llu pkts (forced: %llu, early: %llu)\n",
		       (ull)blue_stats.xmit_packets,
		       (ull)blue_stats.drop_packets,
		       (ull)blue_stats.drop_forced,
		       (ull)blue_stats.drop_unforced);
		if (blue_stats.marked_packets != 0)
			printf(" marked: %llu\n",
			       (ull)blue_stats.marked_packets);
		printf(" throughput: %sbps\n",
		       rate2str(calc_rate(blue_stats.xmit_bytes,
					  last_bytes, sec)));

		last_bytes = blue_stats.xmit_bytes;
		last_time = cur_time;
		sleep(interval);
	}
}
