/*	$OpenBSD: qdisc_cbq.c,v 1.3 2001/11/07 05:05:00 kjc Exp $	*/
/*	$KAME: qdisc_cbq.c,v 1.4 2001/08/15 12:51:58 kjc Exp $	*/
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
#include <altq/altq_cbq.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>

#include "quip_client.h"
#include "altqstat.h"

#define NCLASSES	64

#ifndef RM_FILTER_GAIN
#define	RM_FILTER_GAIN	5	/* log2 of gain, e.g., 5 => 31/32 */
#endif
#ifndef RM_POWER
#define RM_POWER	(1 << RM_FILTER_GAIN)
#endif

void
cbq_stat_loop(int fd, const char *ifname, int count, int interval)
{
	class_stats_t stats1[NCLASSES], stats2[NCLASSES];
	char clnames[NCLASSES][128];
	u_long clhandles[NCLASSES];
	struct cbq_getstats	get_stats;
	class_stats_t		*sp, *lp, *new, *last, *tmp;
	struct timeval		cur_time, last_time;
	int			i;
	double			flow_bps, sec;
	int cnt = count;

	strlcpy(get_stats.iface.cbq_ifacename, ifname,
		sizeof(get_stats.iface.cbq_ifacename));
	new = &stats1[0];
	last = &stats2[0];

	for (i = 0; i < NCLASSES; i++)
	    clhandles[i] = NULL_CLASS_HANDLE;

	while (count == 0 || cnt-- > 0) {
		get_stats.nclasses = NCLASSES;
		get_stats.stats = new;
		if (ioctl(fd, CBQ_GETSTATS, &get_stats) < 0)
			err(1, "ioctl CBQ_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		for (i=0; i<get_stats.nclasses; i++) {
			sp = &new[i];
			lp = &last[i];

			if (sp->handle != clhandles[i]) {
				quip_chandle2name(ifname, sp->handle,
						  clnames[i], sizeof(clnames[0]));
				clhandles[i] = sp->handle;
				continue;
			}

			switch (sp->handle) {
			case ROOT_CLASS_HANDLE:
				printf("Root Class for Interface %s: %s\n",
				       ifname, clnames[i]);
				break;
			case DEFAULT_CLASS_HANDLE:
				printf("Default Class for Interface %s: %s\n",
				       ifname, clnames[i]);
				break;
			case CTL_CLASS_HANDLE:
				printf("Ctl Class for Interface %s: %s\n",
				       ifname, clnames[i]);
				break;
			default:
				printf("Class %d on Interface %s: %s\n",
				       sp->handle, ifname, clnames[i]);
				break;
			}

			flow_bps = 8.0 / (double)sp->ns_per_byte
			    * 1000*1000*1000;

			printf("\tpriority: %d depth: %d",
			       sp->priority, sp->depth);
			printf(" offtime: %d [us] wrr_allot: %d bytes\n",
			       sp->offtime, sp->wrr_allot);
			printf("\tnsPerByte: %d", sp->ns_per_byte);
			printf("\t(%sbps),", rate2str(flow_bps));
			printf("\tMeasured: %s [bps]\n",
			       rate2str(calc_rate(sp->xmit_cnt.bytes,
						  lp->xmit_cnt.bytes, sec)));
			printf("\tpkts: %llu,\tbytes: %llu\n",
			       (ull)sp->xmit_cnt.packets,
			       (ull)sp->xmit_cnt.bytes);
			printf("\tovers: %u,\toveractions: %u\n",
			       sp->over, sp->overactions);
			printf("\tborrows: %u,\tdelays: %u\n",
			       sp->borrows, sp->delays);
			printf("\tdrops: %llu,\tdrop_bytes: %llu\n",
			       (ull)sp->drop_cnt.packets,
			       (ull)sp->drop_cnt.bytes);
			if (sp->qtype == Q_RED)
				print_redstats(sp->red);
			else if (sp->qtype == Q_RIO)
				print_riostats(sp->red);

			printf("\tQCount: %d,\t(qmax: %d)\n",
			       sp->qcnt, sp->qmax);
			printf("\tAvgIdle: %d [us],\t(maxidle: %d minidle: %d [us])\n",
			       sp->avgidle >> RM_FILTER_GAIN,
			       sp->maxidle >> RM_FILTER_GAIN,
			       sp->minidle / RM_POWER);
		}

		/* swap the buffer pointers */
		tmp = last;
		last = new;
		new = tmp;

		last_time = cur_time;
		sleep(interval);
	}
}
