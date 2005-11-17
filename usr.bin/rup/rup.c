/*	$OpenBSD: rup.c,v 1.21 2005/11/17 19:31:54 moritz Exp $	*/

/*-
 * Copyright (c) 1993, John Brezak
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
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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

#ifndef lint
static char rcsid[] = "$OpenBSD: rup.c,v 1.21 2005/11/17 19:31:54 moritz Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <arpa/inet.h>
#include <err.h>

#undef FSHIFT			/* Use protocol's shift and scale values */
#undef FSCALE
#include <rpcsvc/rstat.h>

#define HOST_WIDTH 27

int printtime;			/* print the remote host(s)'s time */

struct host_list {
	struct host_list *next;
	struct in_addr addr;
} *hosts;

void usage(void);
int print_rup_data(char *, statstime *host_stat);

static int
search_host(struct in_addr addr)
{
	struct host_list *hp;
	
	if (!hosts)
		return(0);

	for (hp = hosts; hp != NULL; hp = hp->next) {
		if (hp->addr.s_addr == addr.s_addr)
			return(1);
	}
	return(0);
}

static void
remember_host(struct in_addr addr)
{
	struct host_list *hp;

	if (!(hp = (struct host_list *)malloc(sizeof(struct host_list)))) {
		err(1, NULL);
		/* NOTREACHED */
	}
	hp->addr.s_addr = addr.s_addr;
	hp->next = hosts;
	hosts = hp;
}


struct rup_data {
	char *host;
	struct statstime statstime;
};
struct rup_data *rup_data;
int rup_data_idx = 0;
int rup_data_max = 0;

enum sort_type {
	SORT_NONE,
	SORT_HOST,
	SORT_LDAV,
	SORT_UPTIME
};
enum sort_type sort_type;

static int
compare(const void *v1, const void *v2)
{
	const struct rup_data *d1 = v1;
	const struct rup_data *d2 = v2;

	switch(sort_type) {
	case SORT_HOST:
		return strcmp(d1->host, d2->host);
	case SORT_LDAV:
		return d1->statstime.avenrun[0]
			- d2->statstime.avenrun[0];
	case SORT_UPTIME:
		return d1->statstime.boottime.tv_sec
			- d2->statstime.boottime.tv_sec;
	default:
		/* something's really wrong here */
		abort();
	}
}

static void
remember_rup_data(char *host, struct statstime *st)
{
	if (rup_data_idx >= rup_data_max) {
		int newsize;
		struct rup_data *newrup;

		newsize = rup_data_max + 16;
		newrup = realloc(rup_data, newsize * sizeof(struct rup_data));
		if (newrup == NULL) {
			err(1, NULL);
			/* NOTREACHED */
		}
		rup_data = newrup;
		rup_data_max = newsize;
	}
	
	if ((rup_data[rup_data_idx].host = strdup(host)) == NULL)
		err(1, NULL);
	rup_data[rup_data_idx].statstime = *st;
	rup_data_idx++;
}


static int
rstat_reply(char *replyp, struct sockaddr_in *raddrp)
{
	struct hostent *hp;
	char *host;
	statstime *host_stat = (statstime *)replyp;

	if (!search_host(raddrp->sin_addr)) {
		hp = gethostbyaddr((char *)&raddrp->sin_addr.s_addr,
		    sizeof(struct in_addr), AF_INET);
		if (hp)
			host = hp->h_name;
		else
			host = inet_ntoa(raddrp->sin_addr);

		remember_host(raddrp->sin_addr);

		if (sort_type != SORT_NONE)
			remember_rup_data(host, host_stat);
		else
			print_rup_data(host, host_stat);
	}

	return (0);
}


int
print_rup_data(char *host, statstime *host_stat)
{
	unsigned int ups = 0, upm = 0, uph = 0, upd = 0;
	struct tm *tmp_time, host_time;
	char days_buf[16], hours_buf[16];

	if (printtime)
		printf("%-*.*s", HOST_WIDTH-8, HOST_WIDTH-8, host);
	else
		printf("%-*.*s", HOST_WIDTH, HOST_WIDTH, host);

	tmp_time = localtime((time_t *)&host_stat->curtime.tv_sec);
	host_time = *tmp_time;

	host_stat->curtime.tv_sec -= host_stat->boottime.tv_sec;

	if (host_stat->curtime.tv_sec > 0)
		ups=host_stat->curtime.tv_sec;
	upd=ups/(3600*24);
	ups-=upd*3600*24;
	uph=ups/3600;
	ups-=uph*3600;
	upm=ups/60;

	if (upd != 0)
		snprintf(days_buf, sizeof days_buf, "%3u day%s, ", upd,
		    (upd > 1) ? "s" : "");
	else
		days_buf[0] = '\0';

	if (uph != 0)
		snprintf(hours_buf, sizeof hours_buf, "%2u:%02u, ",
		    uph, upm);
	else
		if (upm != 0)
			snprintf(hours_buf, sizeof hours_buf, "%2u min%s ",
			    upm, (upm == 1) ? ", " : "s,");
		else
			hours_buf[0] = '\0';

	if (printtime)
		printf(" %2d:%02d%cm",
		    (host_time.tm_hour % 12) ? (host_time.tm_hour % 12) : 12,
		    host_time.tm_min,
		    (host_time.tm_hour >= 12) ? 'p' : 'a');

	printf(" up %9.9s%9.9s load average: %.2f %.2f %.2f\n",
	    days_buf, hours_buf,
	    (double)host_stat->avenrun[0]/FSCALE,
	    (double)host_stat->avenrun[1]/FSCALE,
	    (double)host_stat->avenrun[2]/FSCALE);

	return(0);
}


static void
onehost(char *host)
{
	CLIENT *rstat_clnt;
	statstime host_stat;
	static struct timeval timeout = {25, 0};
	extern char *__progname;
	
	rstat_clnt = clnt_create(host, RSTATPROG, RSTATVERS_TIME, "udp");
	if (rstat_clnt == NULL) {
		fprintf(stderr, "%s: %s", __progname,
		    clnt_spcreateerror(host));
		return;
	}

	bzero((char *)&host_stat, sizeof(host_stat));
	if (clnt_call(rstat_clnt, RSTATPROC_STATS, xdr_void, NULL,
	    xdr_statstime, &host_stat, timeout) != RPC_SUCCESS) {
		fprintf(stderr, "%s: %s", __progname,
		    clnt_sperror(rstat_clnt, host));
		clnt_destroy(rstat_clnt);
		return;
	}

	if (sort_type != SORT_NONE)
		remember_rup_data(host, &host_stat);
	else
		print_rup_data(host, &host_stat);

	clnt_destroy(rstat_clnt);
}

static void
allhosts(void)
{
	statstime host_stat;
	enum clnt_stat clnt_stat;
	extern char *__progname;

	if (sort_type != SORT_NONE) {
		printf("collecting responses...\n");
		fflush(stdout);
	}

	clnt_stat = clnt_broadcast(RSTATPROG, RSTATVERS_TIME, RSTATPROC_STATS,
	    xdr_void, NULL, xdr_statstime, (char *)&host_stat, rstat_reply);
	if (clnt_stat != RPC_SUCCESS && clnt_stat != RPC_TIMEDOUT) {
		fprintf(stderr, "%s: %s\n", __progname, clnt_sperrno(clnt_stat));
		exit(1);
	}
}

int
main(int argc, char *argv[])
{
	int ch;
	size_t i;
	extern int optind;

	sort_type = SORT_NONE;
	while ((ch = getopt(argc, argv, "dhlt")) != -1)
		switch (ch) {
		case 'd':
			printtime = 1;
			break;
		case 'h':
			sort_type = SORT_HOST;
			break;
		case 'l':
			sort_type = SORT_LDAV;
			break;
		case 't':
			sort_type = SORT_UPTIME;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	
	setlinebuf(stdout);

	if (argc == optind)
		allhosts();
	else {
		for (; optind < argc; optind++)
			onehost(argv[optind]);
	}

	if (sort_type != SORT_NONE) {
		qsort(rup_data, rup_data_idx, sizeof(struct rup_data),
		    compare);

		for (i = 0; i < rup_data_idx; i++) {
			print_rup_data(rup_data[i].host,
			    &rup_data[i].statstime);
		}
	}

	exit(0);
}


void
usage(void)
{
	fprintf(stderr, "Usage: rup [-dhlt] [hosts ...]\n");
	exit(1);
}
