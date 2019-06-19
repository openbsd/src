/*	$OpenBSD: switchctl.c,v 1.9 2018/10/24 18:06:21 akoshibe Exp $	*/

/*
 * Copyright (c) 2007-2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/tree.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <event.h>
#include <pwd.h>

#include "switchd.h"
#include "parser.h"

__dead void	 usage(void);

struct imsgname {
	int type;
	char *name;
	void (*func)(struct imsg *);
};

int		 show_summary_msg(struct imsg *, int);

struct imsgname *monitor_lookup(uint8_t);
void		 monitor_id(struct imsg *);
int		 monitor(struct imsg *);

int		 ca_opt(struct parse_result *);

struct imsgname imsgs[] = {
	{ IMSG_CTL_OK,			"ok",			NULL },
	{ IMSG_CTL_FAIL,		"fail",			NULL },
	{ IMSG_CTL_VERBOSE,		"verbose",		NULL },
	{ IMSG_CTL_RELOAD,		"reload",		NULL },
	{ IMSG_CTL_RESET,		"reset",		NULL },
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

	fprintf(stderr, "usage: %s [-q] [-s socket] command [arg ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	struct switch_client	 swc;
	struct switch_address	*to;
	struct passwd		*pw;
	int			 ctl_sock;
	int			 done = 1;
	int			 n;
	int			 ch;
	int			 v = 0;
	int			 quiet = 0;
	int			 verbose = 0;
	const char		*sock = SWITCHD_SOCKET;

	while ((ch = getopt(argc, argv, "qs:v")) != -1) {
		switch (ch) {
		case 'q':
			quiet = 1;
			break;
		case 's':
			sock = optarg;
			break;
		case 'v':
			verbose = 2;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if ((pw = getpwnam(SWITCHD_USER)) == NULL)
		fatal("switchctl: getpwnam");

	/*
	 * pledge in switchctl:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reading from the /dev/switch device.
	 * wpath - for accessing the /dev/switch device.
	 * inet - for handling tcp connections with OpenFlow peers.
	 * unix - for opening the control socket.
	 * dns - for parsehostport() in the device spec.
	 */
	if (pledge("stdio rpath wpath inet unix dns", NULL) == -1)
		err(1, "pledge");

	log_init(quiet ? 0 : 2, LOG_USER);

	/* parse options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	res->quiet = quiet;
	res->verbose = verbose;

	if (res->quiet && res->verbose)
		fatal("conflicting -v and -q options");

	switch (res->action) {
	case NONE:
		usage();
		break;
	case DUMP_DESC:
	case DUMP_FEATURES:
	case DUMP_FLOWS:
	case DUMP_TABLES:
	case FLOW_ADD:
	case FLOW_DELETE:
	case FLOW_MODIFY:
		ofpclient(res, pw);
		break;
	default:
		goto connect;
	}

	return (0);

 connect:
	/* connect to sdnflowd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sock, sizeof(sun.sun_path));
 reconnect:
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		/* Keep retrying if running in monitor mode */
		if (res->action == MONITOR &&
		    (errno == ENOENT || errno == ECONNREFUSED)) {
			usleep(100);
			goto reconnect;
		}
		err(1, "connect: %s", sock);
	}

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (res->ibuf != NULL)
		ibuf = res->ibuf;
	else
		if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
			err(1, "malloc");
	imsg_init(ibuf, ctl_sock);

	/* process user request */
	switch (res->action) {
	case RESETALL:
		v = RESET_ALL;
		break;
	case LOG_VERBOSE:
		v = 2;
		break;
	case LOG_BRIEF:
	default:
		v = 0;
		break;
	}

	switch (res->action) {
	case NONE:
		usage();
		/* NOTREACHED */
		break;
	case SHOW_SUM:
	case SHOW_SWITCHES:
	case SHOW_MACS:
		imsg_compose(ibuf, IMSG_CTL_SHOW_SUM, 0, 0, -1, NULL, 0);
		printf("%-4s\t%-4s\t%-8s\t%-24s\t%s\n",
		    "Switch", "Port", "Type", "Name", "Info");
		done = 0;
		break;
	case CONNECT:
	case DISCONNECT:
		memset(&swc, 0, sizeof(swc));
		if (res->addr.ss_family == AF_UNSPEC)
			errx(1, "invalid address");

		memcpy(&swc.swc_addr.swa_addr, &res->addr, sizeof(res->addr));
		if (res->action == DISCONNECT) {
			imsg_compose(ibuf, IMSG_CTL_DISCONNECT, 0, 0, -1,
			    &swc, sizeof(swc));
			break;
		}

		to = &swc.swc_target;
		memcpy(to, &res->uri, sizeof(*to));

		imsg_compose(ibuf, IMSG_CTL_CONNECT, 0, 0, -1,
		    &swc, sizeof(swc));
		break;
	case RESETALL:
		imsg_compose(ibuf, IMSG_CTL_RESET, 0, 0, -1, &v, sizeof(v));
		printf("reset request sent.\n");
		break;
	case LOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1,
		    res->path, strlen(res->path));
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		break;
	case MONITOR:
		imsg_compose(ibuf, IMSG_CTL_NOTIFY, 0, 0, -1, NULL, 0);
		done = 0;
		break;
	case LOG_VERBOSE:
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_VERBOSE, 0, 0, -1, &v, sizeof(v));
		printf("logging request sent.\n");
		break;
	default:
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
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
			case SHOW_SWITCHES:
			case SHOW_MACS:
				done = show_summary_msg(&imsg, res->action);
				break;
			case MONITOR:
				done = monitor(&imsg);
				break;
			default:
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
show_summary_msg(struct imsg *imsg, int type)
{
	struct switch_control	*sw;
	struct macaddr		*mac;
	struct timeval		 tv;
	static unsigned int	 sw_id = 0;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SWITCH:
		IMSG_SIZE_CHECK(imsg, sw);
		sw = imsg->data;
		sw_id = sw->sw_id;

		if (!(type == SHOW_SUM || type == SHOW_SWITCHES))
			break;
		printf("%-4u\t%-4s\t%-8s\t%-24s\n",
		    sw->sw_id, "", "switch",
		    print_host(&sw->sw_addr, NULL, 0));
		break;
	case IMSG_CTL_MAC:
		IMSG_SIZE_CHECK(imsg, mac);
		mac = imsg->data;

		if (!(type == SHOW_SUM || type == SHOW_MACS))
			break;

		getmonotime(&tv);
		printf("%-4u\t%-4u\t%-8s\t%-24s\tage %llds\n",
		    sw_id, mac->mac_port, "mac",
		    print_ether(mac->mac_addr),
		    (long long)tv.tv_sec - mac->mac_age);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		errx(1, "wrong message in summary: %u", imsg->hdr.type);
		break;
	}
	return (0);
}

struct imsgname *
monitor_lookup(uint8_t type)
{
	int i;

	for (i = 0; imsgs[i].name != NULL; i++)
		if (imsgs[i].type == type)
			return (&imsgs[i]);
	return (&imsgunknown);
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
	printf("\ttimestamp: %lld, %s", (long long)now, ctime(&now));
	if (imn->type == -1)
		done = 1;
	if (imn->func != NULL)
		(*imn->func)(imsg);

	return (done);
}
