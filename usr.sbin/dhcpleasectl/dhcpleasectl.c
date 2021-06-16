/*	$OpenBSD: dhcpleasectl.c,v 1.4 2021/06/16 14:06:18 florian Exp $	*/

/*
 * Copyright (c) 2021 Florian Obser <florian@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet6/nd6.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpleased.h"
#include "frontend.h"
#include "parser.h"

__dead void	 usage(void);
int		 show_interface_msg(struct imsg *);

struct imsgbuf	*ibuf;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	int			 ctl_sock;
	int			 done = 0;
	int			 n, verbose = 0;
	int			 ch;
	char			*sockname;

	sockname = _PATH_DHCPLEASED_SOCKET;
	while ((ch = getopt(argc, argv, "s:")) != -1) {
		switch (ch) {
		case 's':
			sockname = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (pledge("stdio unix", NULL) == -1)
		err(1, "pledge");

	/* Parse command line. */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	/* Connect to control socket. */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path));

	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, ctl_sock);
	done = 0;

	/* Process user request. */
	switch (res->action) {
	case LOG_VERBOSE:
		verbose = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_LOG_VERBOSE, 0, 0, -1,
		    &verbose, sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	case SHOW_INTERFACE:
		imsg_compose(ibuf, IMSG_CTL_SHOW_INTERFACE_INFO, 0, 0, -1,
		    &res->if_index, sizeof(res->if_index));
		break;
	case SEND_REQUEST:
		imsg_compose(ibuf, IMSG_CTL_SEND_REQUEST, 0, 0, -1,
		    &res->if_index, sizeof(res->if_index));
		done = 1;
		break;
	default:
		usage();
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) <= 0 && errno != EAGAIN)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;

			switch (res->action) {
			case SHOW_INTERFACE:
				done = show_interface_msg(&imsg);
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
show_interface_msg(struct imsg *imsg)
{
	static int		 if_count = 0;
	struct ctl_engine_info	*cei;
	struct timespec		 now, diff;
	time_t			 y, d, h, m, s;
	int			 i;
	char			 buf[IF_NAMESIZE], *bufp;
	char			 ipbuf[INET_ADDRSTRLEN];
	char			 maskbuf[INET_ADDRSTRLEN];
	char			 gwbuf[INET_ADDRSTRLEN];

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE_INFO:
		cei = imsg->data;

		if (if_count++ > 0)
			printf("\n");

		bufp = if_indextoname(cei->if_index, buf);
		printf("%s [%s]:\n", bufp != NULL ? bufp : "unknown",
		    cei->state);
		memset(ipbuf, 0, sizeof(ipbuf));
		if (cei->server_identifier.s_addr != INADDR_ANY) {
			if (inet_ntop(AF_INET, &cei->server_identifier, ipbuf,
			    sizeof(ipbuf)) == NULL)
				ipbuf[0] = '\0';
		} else if (cei->dhcp_server.s_addr != INADDR_ANY) {
			if (inet_ntop(AF_INET, &cei->dhcp_server, ipbuf,
			    sizeof(ipbuf)) == NULL)
				ipbuf[0] = '\0';
		}
		if (ipbuf[0] != '\0')
			printf("\tserver: %s\n", ipbuf);
		if (cei->requested_ip.s_addr != INADDR_ANY) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			timespecsub(&now, &cei->request_time, &diff);
			memset(ipbuf, 0, sizeof(ipbuf));
			memset(maskbuf, 0, sizeof(maskbuf));
			memset(gwbuf, 0, sizeof(gwbuf));
			if (inet_ntop(AF_INET, &cei->requested_ip, ipbuf,
			    sizeof(ipbuf)) == NULL)
				ipbuf[0] = '\0';
			if (inet_ntop(AF_INET, &cei->mask, maskbuf,
			    sizeof(maskbuf)) == NULL)
				maskbuf[0] = '\0';
			printf("\t    IP: %s/%s\n", ipbuf, maskbuf);
			for (i = 0; i < cei->routes_len; i++) {
				if (inet_ntop(AF_INET, &cei->routes[i].dst,
				    ipbuf, sizeof(ipbuf)) == NULL)
					ipbuf[0] = '\0';
				if (inet_ntop(AF_INET, &cei->routes[i].mask,
				    maskbuf, sizeof(maskbuf)) == NULL)
					maskbuf[0] = '\0';
				if (inet_ntop(AF_INET, &cei->routes[i].gw,
				    gwbuf, sizeof(gwbuf)) == NULL)
					gwbuf[0] = '\0';

				printf("\t%s\t%s/%s - %s\n", i == 0 ? "routes:"
				    : "", ipbuf, maskbuf, gwbuf);
			}
			if (cei->nameservers[0].s_addr != INADDR_ANY) {
				printf("\t   DNS:");
				for (i = 0; i < MAX_RDNS_COUNT &&
				    cei->nameservers[i].s_addr != INADDR_ANY;
				    i++) {
					if (inet_ntop(AF_INET,
					    &cei->nameservers[i], ipbuf,
					    sizeof(ipbuf)) == NULL)
						continue;
					printf(" %s", ipbuf);
				}
				printf("\n");
			}
			s = cei->lease_time - diff.tv_sec;
			if (s < 0)
				s = 0;
			y = s / 31556926; s -= y * 31556926;
			d = s / 86400; s -= d * 86400;
			h = s / 3600; s -= h * 3600;
			m = s / 60; s -= m * 60;

			printf("\t lease: ");
			if (y > 0)
				printf("%lldy ", y);
			if (d > 0)
				printf("%lldd ", d);
			if (h > 0)
				printf("%lldh ", h);
			if (m > 0)
				printf("%lldm ", m);
			if (s > 0)
				printf("%llds ", s);
			printf("\n");
		}
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}
