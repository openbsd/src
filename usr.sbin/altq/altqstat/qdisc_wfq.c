/*	$KAME: qdisc_wfq.c,v 1.3 2001/08/15 12:51:59 kjc Exp $	*/
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
#include <altq/altq_wfq.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>
#ifndef NO_CURSES
#include <curses.h>
#endif

#include "altqstat.h"

struct wfqinfo {
	int qid;
	queue_stats stats;
	u_quad_t last_bytes;
	double bps;
};

#define NTOP		10
static int ntop = NTOP;

void
wfq_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct wfq_getstats wfq_stats;
	struct timeval cur_time, last_time;
	int i, j, k, nqueues;
	double sec;
	struct wfqinfo *qinfo, **top;
	int cnt = count;

	strlcpy(wfq_stats.iface.wfq_ifacename, ifname,
		sizeof(wfq_stats.iface.wfq_ifacename));

	/*
	 * first, find out how many queues are available
	 */
	for (i = 0; i < MAX_QSIZE; i++) {
		wfq_stats.qid = i;
		if (ioctl(fd, WFQ_GET_STATS, &wfq_stats) < 0)
			break;
	}
	nqueues = i;
	printf("wfq on %s: %d queues are used\n", ifname, nqueues);

	if ((qinfo = malloc(nqueues * sizeof(struct wfqinfo))) == NULL)
		err(1, "malloc failed!");
	if ((top = malloc(ntop * sizeof(struct wfqinfo *))) == NULL)
		err(1, "malloc failed!");

#ifndef NO_CURSES
	sleep(2);  /* wait a bit before clearing the screen */

	initscr();
#endif

	gettimeofday(&last_time, NULL);
	last_time.tv_sec -= interval;

	while (count == 0 || cnt-- > 0) {

		for (j = 0; j < ntop; j++)
			top[j] = NULL;

		for (i = 0; i < nqueues; i++) {
			wfq_stats.qid = i;
			if (ioctl(fd, WFQ_GET_STATS, &wfq_stats) < 0)
				err(1, "ioctl WFQ_GET_STATS");

			qinfo[i].qid = i;
			qinfo[i].stats = wfq_stats.stats;
		}

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		/*
		 * calculate the throughput of each queue
		 */
		for (i = 0; i < nqueues; i++) {
			qinfo[i].bps = calc_rate(qinfo[i].stats.xmit_cnt.bytes,
						 qinfo[i].last_bytes, sec);
			qinfo[i].last_bytes = qinfo[i].stats.xmit_cnt.bytes;

			for (j = 0; j < ntop; j++) {
				if (top[j] == NULL) {
					top[j] = &qinfo[i];
					break;
				}
				if (top[j]->bps < qinfo[i].bps ||
				    (top[j]->bps == qinfo[i].bps &&
				     top[j]->stats.xmit_cnt.packets <
				     qinfo[i].stats.xmit_cnt.packets)) {
					for (k = ntop-1; k > j; k--)
						top[k] = top[k-1];
					top[j] = &qinfo[i];
					break;
				}
			}
		}

		/*
		 * display top
		 */
		printf("[QID] WEIGHT QSIZE(KB) SENT(pkts)     (KB)       DROP(pkts)     (KB)     bps\n\r");

		for (j = 0; j < ntop; j++) {
			if (top[j] != NULL)
				printf("[%4d] %4d %4d %10llu %14llu %10llu %14llu %9s\n\r",
				       top[j]->qid,
				       top[j]->stats.weight,
				       top[j]->stats.bytes / 1024,
				       (ull)top[j]->stats.xmit_cnt.packets, 
				       (ull)top[j]->stats.xmit_cnt.bytes /1024,
				       (ull)top[j]->stats.drop_cnt.packets,
				       (ull)top[j]->stats.drop_cnt.bytes /1024,
				       rate2str(top[j]->bps));
			else
				printf("\n");
		}	
#ifndef NO_CURSES
		refresh();
		mvcur(ntop+1, 0, 0, 0);
#endif

		last_time = cur_time;
		sleep(interval);
	}

}
