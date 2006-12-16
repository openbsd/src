/*	$OpenBSD: relayctl.c,v 1.5 2006/12/16 17:52:21 deraadt Exp $	*/

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include "hostated.h"
#include "parser.h"

__dead void	 usage(void);
int		 show_summary_msg(struct imsg *);
int		 show_command_output(struct imsg *);
char		*print_service_status(int);
char		*print_host_status(int, int);
char		*print_table_status(int, int);

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s <command> [arg [...]]\n", __progname);
	exit(1);
}

/* dummy function so that hostatectl does not need libevent */
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

	/* connect to hostated control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, HOSTATED_SOCKET, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", HOSTATED_SOCKET);

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
		imsg_compose(ibuf, IMSG_CTL_SHOW_SUM, 0, 0, NULL, 0);
		printf("type\t%4s\t%-16s\tstatus\n\n", "id", "name");
		break;
	case SERV_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_SERVICE_ENABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case SERV_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_SERVICE_DISABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case TABLE_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_TABLE_ENABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case TABLE_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_TABLE_DISABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case HOST_ENABLE:
		imsg_compose(ibuf, IMSG_CTL_HOST_ENABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case HOST_DISABLE:
		imsg_compose(ibuf, IMSG_CTL_HOST_DISABLE, 0, 0,
		    &res->id, sizeof(res->id));
		break;
	case SHUTDOWN:
		imsg_compose(ibuf, IMSG_CTL_SHUTDOWN, 0, 0, NULL, 0);
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, NULL, 0);
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
				done = show_summary_msg(&imsg);
				break;
			case SERV_DISABLE:
			case SERV_ENABLE:
			case TABLE_DISABLE:
			case TABLE_ENABLE:
			case HOST_DISABLE:
			case HOST_ENABLE:
				done = show_command_output(&imsg);
				break;
			case RELOAD:
			case SHUTDOWN:
			case NONE:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);
	free(ibuf);

	return (0);
}

int
show_summary_msg(struct imsg *imsg)
{
	struct service	*service;
	struct table	*table;
	struct host	*host;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SERVICE:
		service = imsg->data;
		printf("service\t%4u\t%-16s\t%s\n",
		    service->id, service->name,
		    print_service_status(service->flags));
		break;
	case IMSG_CTL_TABLE:
		table = imsg->data;
		printf("table\t%4u\t%-16s\t%s",
		    table->id, table->name,
		    print_table_status(table->up, table->flags));
		printf("\n");
		break;
	case IMSG_CTL_HOST:
		host = imsg->data;
		printf("host\t%4u\t%-16s\t%s\n",
		    host->id, host->name,
		    print_host_status(host->up, host->flags));
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
