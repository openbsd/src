/*	$OpenBSD: qdisc_cdnr.c,v 1.1.1.1 2001/06/27 18:23:20 kjc Exp $	*/
/*	$KAME: qdisc_cdnr.c,v 1.3 2000/10/18 09:15:16 kjc Exp $	*/
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
#include <altq/altq_cdnr.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <err.h>

#include "quip_client.h"
#include "altqstat.h"

#define NELEMENTS	64
#define MAX_PROB	(128*1024)

static char *element_names[] = { "none", "top", "element", "tbmeter", "trtcm",
				 "tswtcm" };
static char *tbmprof_names[] = { "in:    ", "out:   " };
static char *tcmprof_names[] = { "green: ", "yellow:", "red:   " };

void
cdnr_stat_loop(int fd, const char *ifname, int count, int interval)
{
	struct tce_stats	stats1[NELEMENTS], stats2[NELEMENTS];
	char			cdnrnames[NELEMENTS][128];
	struct cdnr_get_stats	get_stats;
	struct tce_stats	*sp, *lp, *new, *last, *tmp;
	struct timeval		cur_time, last_time;
	double			sec;
	char			**profile_names, _ifname[32];
	int			i, j, nprofile;
	int cnt = count;

	if (ifname[0] == '_')
		ifname++;
	sprintf(_ifname, "_%s", ifname);

	strcpy(get_stats.iface.cdnr_ifname, ifname);
	new = &stats1[0];
	last = &stats2[0];

	for (i = 0; i < NELEMENTS; i++)
		stats1[i].tce_handle = stats2[i].tce_handle = CDNR_NULL_HANDLE;

	while (count == 0 || cnt-- > 0) {
		get_stats.nskip = 0;
		get_stats.nelements = NELEMENTS;
		get_stats.tce_stats = new;
	
		if (ioctl(fd, CDNR_GETSTATS, &get_stats) < 0)
			err(1, "ioctl CDNR_GETSTATS");

		gettimeofday(&cur_time, NULL);
		sec = calc_interval(&cur_time, &last_time);

		printf("actions:\n");
		printf("  pass:%llu drop:%llu mark:%llu next:%llu return:%llu none:%llu\n",
		       (ull)get_stats.cnts[TCACODE_PASS].packets,
		       (ull)get_stats.cnts[TCACODE_DROP].packets,
		       (ull)get_stats.cnts[TCACODE_MARK].packets,
		       (ull)get_stats.cnts[TCACODE_NEXT].packets,
		       (ull)get_stats.cnts[TCACODE_RETURN].packets,
		       (ull)get_stats.cnts[TCACODE_NONE].packets);

		for (i = 0; i < get_stats.nelements; i++) {
			sp = &new[i];
			lp = &last[i];

			if (sp->tce_handle != lp->tce_handle) {
				quip_chandle2name(_ifname, sp->tce_handle,
						  cdnrnames[i]);
				continue;
			}

			switch (sp->tce_type) {
			case TCETYPE_TBMETER:
				nprofile = 2;
				profile_names = tbmprof_names;
				break;
			case TCETYPE_TRTCM:
			case TCETYPE_TSWTCM:
				nprofile = 3;
				profile_names = tcmprof_names;
				break;
			default:
				profile_names = tbmprof_names; /* silence cc */
				nprofile = 0;
			}

			if (nprofile == 0)
				continue;

			printf("[%s: %s] handle:%#lx\n",
			       element_names[sp->tce_type], cdnrnames[i],
			       sp->tce_handle);
			for (j = 0; j < nprofile; j++) {
				printf("  %s %10llu pkts %16llu bytes (%sbps)\n",
				       profile_names[j], 
				       (ull)sp->tce_cnts[j].packets,
				       (ull)sp->tce_cnts[j].bytes,
				       rate2str(
					       calc_rate(sp->tce_cnts[j].bytes,
							 lp->tce_cnts[j].bytes,
							 sec)));
			}
		}
		printf("\n");

		/* swap the buffer pointers */
		tmp = last;
		last = new;
		new = tmp;

		last_time = cur_time;
		sleep(interval);
	}
}
