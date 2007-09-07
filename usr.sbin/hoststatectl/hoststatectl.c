/*	$OpenBSD: hoststatectl.c,v 1.21 2007/09/07 08:31:36 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include <openssl/ssl.h>

#include "hoststated.h"
#include "parser.h"

__dead void	 usage(void);
int		 show_summary_msg(struct imsg *, int);
int		 show_command_output(struct imsg *);
char		*print_service_status(int);
char		*print_host_status(int, int);
char		*print_table_status(int, int);
char		*print_relay_status(int);

struct imsgname {
	int type;
	char *name;
	void (*func)(struct imsg *);
};

struct imsgname *monitor_lookup(u_int8_t);
void		 monitor_host_status(struct imsg *);
void		 monitor_id(struct imsg *);
int		 monitor(struct imsg *);

struct imsgname imsgs[] = {
	{ IMSG_HOST_STATUS,		"host_status",		monitor_host_status },
	{ IMSG_CTL_SERVICE_DISABLE,	"ctl_disable_service",	monitor_id },
	{ IMSG_CTL_SERVICE_ENABLE,	"ctl_service_enable",	monitor_id },
	{ IMSG_CTL_TABLE_DISABLE,	"ctl_table_disable",	monitor_id },
	{ IMSG_CTL_TABLE_ENABLE,	"ctl_table_enable",	monitor_id },
	{ IMSG_CTL_HOST_DISABLE,	"ctl_host_disable",	monitor_id },
	{ IMSG_CTL_HOST_ENABLE,		"ctl_host_enable",	monitor_id },
	{ IMSG_CTL_TABLE_CHANGED,	"ctl_table_changed",	monitor_id },
	{ IMSG_CTL_PULL_RULESET,	"ctl_pull_ruleset",	monitor_id },
	{ IMSG_CTL_PUSH_RULESET,	"ctl_push_ruleset",	monitor_id },
	{ IMSG_SYNC,			"sync",			NULL },
	{ 0,				NULL,			NULL }
};
struct imsgname imsgunknown = {
	-1,				"<unknown>",		NULL
};

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s <command> [arg [...]]\n", __progname);
	exit(1);
}

/* dummy function so that hoststatectl does not need libevent */
void
imsg_event_add(struct imsgbuf *i)
{
	/* nothing */
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	int			 ctl_sock;
	int			 done = 0;
	int			 n;

	/* parse options */
	if ((res = parse(argc - 1, argv + 1)) == NULL)
		exit(1);

	/* connect to hoststated control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, HOSTSTATED_SOCKET, sizeof(sun.sun_path));
 reconnect:
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		/* Keep retrying if running in monitor mode */
		if (res->action == MONITOR &&
		    (errno == ENOENT || errno == ECONNREFUSED)) {
			usleep(100);
			goto reconnect;
		}
		err(1, "connect: %s", HOSTSTATED_SOCKET);
	}

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, ctl_sock, NULL);
	done = 0;

	/* process user request */
	switch (res->action) {
	case NONE:
		usage();
		/* not reached */
	case SHOW_SUM:
	case SHOW_HOSTS:
	case SHOW_RELAYS:
		imsg_compose(ibuf, IMSG_CTL_SHOW_SUM, 0, 0, -1, NULL, 0);
		printf("Type\t%4s\t%-24s\t%-7s\tStatus\n",
		    "Id", "Name", "Avlblty");
		break;
	case SERV_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_SERVICE_ENABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case SERV_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_SERVICE_DISABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case TABLE_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_TABLE_ENABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case TABLE_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_TABLE_DISABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case HOST_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_HOST_ENABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case HOST_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_HOST_DISABLE, 0, 0, -1,
		    &res->id, sizeof(res->id));
		break;
	case SHUTDOWN:
		imsg_compose(ibuf, IMSG_CTL_SHUTDOWN, 0, 0, -1, NULL, 0);
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		break;
	case MONITOR:
		imsg_compose(ibuf, IMSG_CTL_NOTIFY, 0, 0, -1, NULL, 0);
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (res->action) {
			case SHOW_SUM:
			case SHOW_HOSTS:
			case SHOW_RELAYS:
				done = show_summary_msg(&imsg, res->action);
				break;
			case SERV_DISABLE:
			case SERV_ENABLE:
			case TABLE_DISABLE:
			case TABLE_ENABLE:
			case HOST_DISABLE:
			case HOST_ENABLE:
			case RELOAD:
			case SHUTDOWN:
				done = show_command_output(&imsg);
				break;
			case NONE:
				break;
			case MONITOR:
				done = monitor(&imsg);
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);
	free(ibuf);

	return (0);
}

struct imsgname *
monitor_lookup(u_int8_t type)
{
	int i;

	for (i = 0; imsgs[i].name != NULL; i++)
		if (imsgs[i].type == type)
			return (&imsgs[i]);
	return (&imsgunknown);
}

void
monitor_host_status(struct imsg *imsg)
{
	struct ctl_status	 cs;

	memcpy(&cs, imsg->data, sizeof(cs));
	printf("\tid: %u\n", cs.id);
	printf("\tstate: ");
	switch (cs.up) {
	case HOST_UP:
		printf("up\n");
		break;
	case HOST_DOWN:
		printf("down\n");
		break;
	default:
		printf("unknown\n");
		break;
	}
}

void
monitor_id(struct imsg *imsg)
{
	struct ctl_id		 id;

	memcpy(&id, imsg->data, sizeof(id));
	printf("\tid: %u\n", id.id);
	if (strlen(id.name))
		printf("\tname: %s\n", id.name);
}

int
monitor(struct imsg *imsg)
{
	time_t			 now;
	int			 done = 0;
	struct imsgname		*imn;

	now = time(NULL);

	imn = monitor_lookup(imsg->hdr.type);
	printf("%s: imsg type %u len %u peerid %u pid %d\n", imn->name,
	    imsg->hdr.type, imsg->hdr.len, imsg->hdr.peerid, imsg->hdr.pid);
	printf("\ttimestamp: %u, %s", now, ctime(&now));
	if (imn->type == -1)
		done = 1;
	if (imn->func != NULL)
		(*imn->func)(imsg);

	return (done);
}

int
show_summary_msg(struct imsg *imsg, int type)
{
	struct service		*service;
	struct table		*table;
	struct host		*host;
	struct relay		*rlay;
	struct ctl_stats	 stats[RELAY_MAXPROC], crs;
	int			 i;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SERVICE:
		if (type == SHOW_RELAYS)
			break;
		service = imsg->data;
		printf("service\t%4u\t%-24s\t%-7s\t%s\n",
		    service->conf.id, service->conf.name, "",
		    print_service_status(service->conf.flags));
		break;
	case IMSG_CTL_TABLE:
		if (type == SHOW_RELAYS)
			break;
		table = imsg->data;
		printf("table\t%4u\t%-24s\t%-7s\t%s\n",
		    table->conf.id, table->conf.name, "",
		    print_table_status(table->up, table->conf.flags));
		break;
	case IMSG_CTL_HOST:
		if (type == SHOW_RELAYS)
			break;
		host = imsg->data;
		printf("host\t%4u\t%-24s\t%-7s\t%s\n",
		    host->conf.id, host->conf.name,
		    print_availability(host->check_cnt, host->up_cnt),
		    print_host_status(host->up, host->flags));
		if (type == SHOW_HOSTS && host->check_cnt) {
			printf("\t%4s\ttotal: %lu/%lu checks",
			    "", host->up_cnt, host->check_cnt);
			if (host->retry_cnt)
				printf(", %d retries", host->retry_cnt);
			printf("\n");
		}
		break;
	case IMSG_CTL_RELAY:
		if (type == SHOW_HOSTS)
			break;
		rlay = imsg->data;
		printf("relay\t%4u\t%-24s\t%-7s\t%s\n",
		    rlay->conf.id, rlay->conf.name, "",
		    print_relay_status(rlay->conf.flags));
		break;
	case IMSG_CTL_STATISTICS:
		if (type != SHOW_RELAYS)
			break;
		bcopy(imsg->data, &stats, sizeof(stats));
		bzero(&crs, sizeof(crs));
		crs.interval = stats[0].interval;
		for (i = 0; stats[i].id != EMPTY_ID; i++) {
			crs.cnt += stats[i].cnt;
			crs.last += stats[i].last;
			crs.avg += stats[i].avg;
			crs.last_hour += stats[i].last_hour;
			crs.avg_hour += stats[i].avg_hour;
			crs.last_day += stats[i].last_day;
			crs.avg_day += stats[i].avg_day;
		}
		if (crs.cnt == 0)
			break;
		printf("\t%4s\ttotal: %lu sessions\n"
		    "\t%4s\tlast: %lu/%us %lu/h %lu/d sessions\n"
		    "\t%4s\taverage: %lu/%us %lu/h %lu/d sessions\n",
		    "", crs.cnt,
		    "", crs.last, crs.interval,
		    crs.last_hour, crs.last_day,
		    "", crs.avg, crs.interval,
		    crs.avg_hour, crs.avg_day);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		errx(1, "wrong message in summary: %u", imsg->hdr.type);
		break;
	}
	return (0);
}

int
show_command_output(struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CTL_OK:
		printf("command succeeded\n");
		break;
	case IMSG_CTL_FAIL:
		printf("command failed\n");
		break;
	default:
		errx(1, "wrong message in summary: %u", imsg->hdr.type);
	}
	return (1);
}

char *
print_service_status(int flags)
{
	if (flags & F_DISABLE) {
		return ("disabled");
	} else if (flags & F_DOWN) {
		return ("down");
	} else if (flags & F_BACKUP) {
		return ("active (using backup table)");
	} else
		return ("active");
}

char *
print_table_status(int up, int fl)
{
	static char buf[1024];

	bzero(buf, sizeof(buf));

	if (fl & F_DISABLE) {
		snprintf(buf, sizeof(buf) - 1, "disabled");
	} else if (!up) {
		snprintf(buf, sizeof(buf) - 1, "empty");
	} else
		snprintf(buf, sizeof(buf) - 1, "active (%d hosts up)", up);
	return (buf);
}

char *
print_host_status(int status, int fl)
{
	if (fl & F_DISABLE)
		return ("disabled");

	switch (status) {
	case HOST_DOWN:
		return ("down");
	case HOST_UNKNOWN:
		return ("unknown");
	case HOST_UP:
		return ("up");
	default:
		errx(1, "invalid status: %d", status);
	}
}

char *
print_relay_status(int flags)
{
	if (flags & F_DISABLE) {
		return ("disabled");
	} else
		return ("active");
}

