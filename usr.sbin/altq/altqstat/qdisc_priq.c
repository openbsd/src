/*	$OpenBSD: qdisc_priq.c,v 1.2 2001/08/16 12:59:43 kjc Exp $	*/
/*	$KAME: qdisc_priq.c,v 1.2 2001/08/15 12:51:59 kjc Exp $	*/
/*
 * Copyright (C) 2000
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
#include <altq/altq_priq.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>

#include "quip_client.h"
#include "altqstat.h"

void
priq_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct class_stats stats1[PRIQ_MAXPRI], stats2[PRIQ_MAXPRI];
	char clnames[PRIQ_MAXPRI][128];
	struct priq_class_stats	get_stats;
	struct class_stats	*sp, *lp, *new, *last, *tmp;
	struct timeval		cur_time, last_time;
	int			i;
	double			sec;
	int			cnt = count;
	
	strlcpy(get_stats.iface.ifname, ifname,
		sizeof(get_stats.iface.ifname));
	new = &stats1[0];
	last = &stats2[0];

	/* invalidate class handles */
	for (i=0; i<PRIQ_MAXPRI; i++)
		last[i].class_handle = PRIQ_NULLCLASS_HANDLE;

	while (count == 0 || cnt-- > 0) {
		get_stats.stats = new;
		get_stats.maxpri = PRIQ_MAXPRI;
		if (ioctl(fd, PRIQ_GETSTATS, &get_stats) < 0)
			err(1, "ioctl PRIQ_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		printf("\n%s:\n", ifname);

		for (i = get_stats.maxpri; i >= 0; i--) {
			sp = &new[i];
			lp = &last[i];

			if (sp->class_handle == PRIQ_NULLCLASS_HANDLE)
				continue;

			if (sp->class_handle != lp->class_handle) {
				quip_chandle2name(ifname, sp->class_handle,
						  clnames[i], sizeof(clnames[0]));
				continue;
			}

			printf("[%s] handle:%#lx pri:%d\n",
			       clnames[i], sp->class_handle, i);
			printf("  measured: %sbps qlen:%2d period:%u\n",
			       rate2str(calc_rate(sp->xmitcnt.bytes,
						  lp->xmitcnt.bytes, sec)),
			       sp->qlength, sp->period);
			printf("     packets:%llu (%llu bytes) drops:%llu\n",
			       (ull)sp->xmitcnt.packets,
			       (ull)sp->xmitcnt.bytes,
			       (ull)sp->dropcnt.packets);
			if (sp->qtype == Q_RED)
				print_redstats(sp->red);
			else if (sp->qtype == Q_RIO)
				print_riostats(sp->red);
		}

		/* swap the buffer pointers */
		tmp = last;
		last = new;
		new = tmp;

		last_time = cur_time;
		sleep(interval);
	}
}
