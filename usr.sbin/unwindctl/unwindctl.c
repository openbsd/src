/*	$OpenBSD: unwindctl.c,v 1.10 2019/11/06 14:19:59 florian Exp $	*/

/*
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unwind.h"
#include "captiveportal.h"
#include "frontend.h"
#include "resolver.h"
#include "parser.h"

__dead void	 usage(void);
int		 show_status_msg(struct imsg *);
void		 print_indented_str(char *);
void		 print_histogram(void*, size_t len);

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
	int			 type;
	char			*sockname;

	sockname = UNWIND_SOCKET;
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

	/* Check for root-only actions */
	switch (res->action) {
	case LOG_DEBUG:
	case LOG_VERBOSE:
	case LOG_BRIEF:
	case RELOAD:
		if (geteuid() != 0)
			errx(1, "need root privileges");
		break;
	default:
		break;
	}

	/* Process user request. */
	switch (res->action) {
	case LOG_DEBUG:
		verbose |= OPT_VERBOSE2;
		/* FALLTHROUGH */
	case LOG_VERBOSE:
		verbose |= OPT_VERBOSE;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_LOG_VERBOSE, 0, 0, -1,
		    &verbose, sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		printf("reload request sent.\n");
		done = 1;
		break;
	case PORTAL:
		imsg_compose(ibuf, IMSG_CTL_RECHECK_CAPTIVEPORTAL, 0, 0, -1,
		    NULL, 0);
		printf("recheck request sent.\n");
		done = 1;
		break;
	case STATUS_RECURSOR:
		type = UW_RES_RECURSOR;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
		break;
	case STATUS_DHCP:
		type = UW_RES_DHCP;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
		break;
	case STATUS_STATIC:
		type = UW_RES_FORWARDER;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
		break;
	case STATUS_DOT:
		type = UW_RES_DOT;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
		break;
	case STATUS_STUB:
		type = UW_RES_ASR;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
		break;
	case STATUS:
		type = UW_RES_NONE;
		imsg_compose(ibuf, IMSG_CTL_STATUS, 0, 0, -1, &type,
		    sizeof(type));
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
			case STATUS:
			case STATUS_RECURSOR:
			case STATUS_DHCP:
			case STATUS_STATIC:
			case STATUS_DOT:
			case STATUS_STUB:
				done = show_status_msg(&imsg);
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
show_status_msg(struct imsg *imsg)
{
	static int			 header;
	struct ctl_resolver_info	*cri;
	enum captive_portal_state	 captive_portal_state;

	if (imsg->hdr.type != IMSG_CTL_CAPTIVEPORTAL_INFO && !header++)
		printf("%8s %16s %s\n", "selected", "type", "status");

	switch (imsg->hdr.type) {
	case IMSG_CTL_CAPTIVEPORTAL_INFO:
		memcpy(&captive_portal_state, imsg->data,
		    sizeof(captive_portal_state));
		switch (captive_portal_state) {
		case PORTAL_UNCHECKED:
		case PORTAL_UNKNOWN:
			printf("captive portal is %s\n\n",
			    captive_portal_state_str[captive_portal_state]);
			break;
		case BEHIND:
		case NOT_BEHIND:
			printf("%s captive portal\n\n",
			    captive_portal_state_str[captive_portal_state]);
			break;
		}
		break;
	case IMSG_CTL_RESOLVER_INFO:
		cri = imsg->data;
		printf("%8s %16s %s%s\n", cri->selected ? "*" : " ",
		    uw_resolver_type_str[cri->type],
		    uw_resolver_state_str[cri->state],
		    cri->oppdot ? " (opportunistic DoT)" : "");
		break;
	case IMSG_CTL_RESOLVER_WHY_BOGUS:
		/* make sure this is a string */
		((char *)imsg->data)[imsg->hdr.len - IMSG_HEADER_SIZE -1] =
		    '\0';
		printf("\nReason for not validating:\n");
		print_indented_str(imsg->data);
		break;
	case IMSG_CTL_RESOLVER_HISTOGRAM:
		print_histogram(imsg->data, imsg->hdr.len - IMSG_HEADER_SIZE);
		break;
	case IMSG_CTL_END:
		return (1);
	default:
		break;
	}

	return (0);
}

void
print_indented_str(char * str)
{
	int	 i;
	char	*cur;

	if (strlen(str) <= 72) {
		printf("\t%s\n", str);
		return;
	}
	
	for (i = 71; i >= 0; i--)
		if (str[i] == ' ')
			break;

	if (i < 0)
		cur = strchr(str, ' ');
	else
		cur = &str[i];


	if (cur == NULL)
		printf("\t%s\n", str);
	else {
		*cur = '\0';
		printf("\t%s\n", str);
		print_indented_str(cur + 1);
	}
}

void
print_histogram(void* data, size_t len)
{
	int64_t	 histogram[nitems(histogram_limits)];
	size_t	 i;
	char	 buf[10];

	if (len != sizeof(histogram))
		errx(1, "invalid histogram size");

	printf("\n%40s\n", "histogram[ms]");

	memcpy(histogram, data, len);

	for(i = 1; i < nitems(histogram_limits) - 1; i++) {
		snprintf(buf, sizeof(buf), "<%lld", histogram_limits[i]);
		printf("%6s", buf);
	}
	printf("%6s\n", ">");
	for(i = 1; i < nitems(histogram); i++)
		printf("%6lld", histogram[i]);
	printf("\n");
}
