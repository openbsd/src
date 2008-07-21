/*	$OpenBSD: dump.c,v 1.10 2008/07/21 19:14:15 rainer Exp $	*/
/*	$KAME: dump.c,v 1.27 2002/05/29 14:23:55 itojun Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <netinet/in.h>

/* XXX: the following two are non-standard include files */
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "rtadvd.h"
#include "timer.h"
#include "if.h"
#include "log.h"
#include "dump.h"

extern struct ralist ralist;

static char *ether_str(struct sockaddr_dl *);

static char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};

static char *
ether_str(sdl)
	struct sockaddr_dl *sdl;
{
	static char hbuf[NI_MAXHOST];
	u_char *cp;

	if (sdl->sdl_alen) {
		cp = (u_char *)LLADDR(sdl);
		snprintf(hbuf, sizeof(hbuf), "%x:%x:%x:%x:%x:%x",
			cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
	} else
		snprintf(hbuf, sizeof(hbuf), "NONE");

	return(hbuf);
}

char*
lifetime(int lt)
{
	char *str;

	if (lt == ND6_INFINITE_LIFETIME)
		asprintf(&str, "infinity");
	else
		asprintf(&str, "%ld", (long)lt);
	return str;
}

void
rtadvd_dump()
{
	struct rainfo *rai;
	struct prefix *pfx;
	char prefixbuf[INET6_ADDRSTRLEN];
	int first;
	struct timeval now;
	char *origin, *vltime, *pltime, *flags;
	char *vltimexpire=NULL, *pltimexpire=NULL;

	gettimeofday(&now, NULL); /* XXX: unused in most cases */
	SLIST_FOREACH(rai, &ralist, entry) {
		log_info("%s:", rai->ifname);

		log_info("  Status: %s",
		    (iflist[rai->ifindex]->ifm_flags & IFF_UP) ? "UP" : "DOWN");

		/* control information */
		if (rai->lastsent.tv_sec) {
			/* note that ctime() appends CR by itself */
			log_info("  Last RA sent: %s",
			    ctime((time_t *)&rai->lastsent.tv_sec));

		}
		if (rai->timer) {
			log_info("  Next RA will be sent: %s",
			    ctime((time_t *)&rai->timer->tm.tv_sec));

		}
		else
			log_info("  RA timer is stopped");
		log_info("  waits: %d, initcount: %d",

			rai->waiting, rai->initcounter);

		/* statistics */
		log_info("  statistics: RA(out/in/inconsistent): "
		    "%llu/%llu/%llu, RS(input): %llu",
		    (unsigned long long)rai->raoutput,
		    (unsigned long long)rai->rainput,
		    (unsigned long long)rai->rainconsistent,
		    (unsigned long long)rai->rsinput);

		/* interface information */
		if (rai->advlinkopt)
			log_info("  Link-layer address: %s",
			    ether_str(rai->sdl));
		log_info("  MTU: %d", rai->phymtu);

		/* Router configuration variables */
		log_info("  DefaultLifetime: %d, MaxAdvInterval: %d, "
		    "MinAdvInterval: %d, "
		    "Flags: %s%s, Preference: %s, MTU: %d",
		    rai->lifetime, rai->maxinterval, rai->mininterval,
		    rai->managedflg ? "M" : "-", rai->otherflg ? "O" : "-",
		    rtpref_str[(rai->rtpref >> 3) & 0xff], rai->linkmtu);
		log_info("  ReachableTime: %d, RetransTimer: %d, "
		    "CurHopLimit: %d", rai->reachabletime,
		    rai->retranstimer, rai->hoplimit);
		if (rai->clockskew)
			log_info("  Clock skew: %ldsec",
			    rai->clockskew);
		first = 1;
		TAILQ_FOREACH(pfx, &rai->prefixes, entry) {
			if (first) {
				log_info("  Prefixes:");
				first = 0;
			}
			switch (pfx->origin) {
			case PREFIX_FROM_KERNEL:
				origin = "KERNEL";
				break;
			case PREFIX_FROM_CONFIG:
				origin = "CONFIG";
				break;
			case PREFIX_FROM_DYNAMIC:
				origin = "DYNAMIC";
				break;
			default:
				origin = "";
			}
			if (pfx->vltimeexpire != 0)
				asprintf(&vltimexpire, "(decr,expire %ld)", (long)
				    pfx->vltimeexpire > now.tv_sec ?
				    pfx->vltimeexpire - now.tv_sec : 0);
			if (pfx->pltimeexpire != 0)
				asprintf(&pltimexpire, "(decr,expire %ld)", (long)
				    pfx->pltimeexpire > now.tv_sec ?
				    pfx->pltimeexpire - now.tv_sec : 0);

			vltime = lifetime(pfx->validlifetime);
			pltime = lifetime(pfx->preflifetime);
			asprintf(&flags, "%s%s",
			    pfx->onlinkflg ? "L" : "-",
			    pfx->autoconfflg ? "A" : "-");
			log_info("    %s/%d(%s, vltime: %s%s, "
			    "pltime: %s%s, flags: %s)",
			    inet_ntop(AF_INET6, &pfx->prefix, prefixbuf,
			    sizeof(prefixbuf)), pfx->prefixlen, origin,
			    vltime, (vltimexpire)? vltimexpire : "",
			    pltime, (pltimexpire)? pltimexpire : "", flags);

			if (vltimexpire) {
				free(vltimexpire);
				vltimexpire = NULL;
			}
			if (pltimexpire) {
				free(pltimexpire);
				pltimexpire = NULL;
			}
			free(vltime);
			free(pltime);
			free(flags);
		}
	}
}
