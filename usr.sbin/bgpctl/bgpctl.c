/*	$OpenBSD: bgpctl.c,v 1.10 2004/01/04 17:55:19 henning Exp $ */

/*
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
#include <sys/un.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

enum actions {
	SHOW,
	SHOW_SUMMARY,
	SHOW_NEIGHBOR
};

struct keywords {
	const char	*keyword;
	int		 value;
};

static const struct keywords keywords_main[] = {
	{ "show",	SHOW}
};

static const struct keywords keywords_show[] = {
	{ "neighbor",	SHOW_NEIGHBOR},
	{ "summary",	SHOW_SUMMARY}
};

int		 main(int, char *[]);
int		 match_keyword(const char *, const struct keywords [], size_t);
void		 show_summary_head(void);
int		 show_summary_msg(struct imsg *);
int		 show_neighbor_msg(struct imsg *);
static char	*fmt_timeframe(time_t t);

struct imsgbuf	ibuf;

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	int			 fd, n, done;
	struct imsg		 imsg;
	enum actions		 action = SHOW_SUMMARY;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		err(1, "control_init: socket");
		exit(1);
	}

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SOCKET_NAME, sizeof(sun.sun_path));
	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		err(1, "connect: %s", SOCKET_NAME);
		exit(1);
	}

	imsg_init(&ibuf, fd);
	done = 0;

	if (argc >= 2)
		action = match_keyword(argv[1], keywords_main,
		    sizeof(keywords_main)/sizeof(keywords_main[0]));

again:
	switch (action) {
	case SHOW:
		if (argc >= 3) {
			action = match_keyword(argv[2], keywords_show,
			    sizeof(keywords_show)/sizeof(keywords_show[0]));
			goto again;
		}
		/* fallthrough */
	case SHOW_SUMMARY:
		if (argc >= 4)
			errx(1, "\"show summary\" does not take arguments");
		imsg_compose(&ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, NULL, 0);
		show_summary_head();
		break;
	case SHOW_NEIGHBOR:
		/* get ip address of neighbor, limit query to that */
		imsg_compose(&ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, NULL, 0);
		break;
	}

	while (!done) {
		if ((n = imsg_read(&ibuf)) == -1)
			break;
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(&ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0) {
				done = 1;
				break;
			}
			switch (action) {
			case SHOW:
			case SHOW_SUMMARY:
				done = show_summary_msg(&imsg);
				break;
			case SHOW_NEIGHBOR:
				done = show_neighbor_msg(&imsg);
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(fd);
}

int
match_keyword(const char *word, const struct keywords table[], size_t cnt)
{
	u_int	match, res, i;

	match = res = 0;

	for (i = 0; i < cnt; i++)
		if (strncmp(word, table[i].keyword,
		    strlen(word)) == 0) {
			match++;
			res = table[i].value;
		}

	if (match > 1)
		errx(1, "ambigous command: %s", word);
	if (match < 1)
		errx(1, "unknown command: %s", word);

	return (res);
}

void
show_summary_head(void)
{
	printf("%-15s %-5s %-10s %-10s %-8s %s\n", "Neighbor", "AS", "MsgRcvd",
	    "MsgSent", "Up/Down", "State");
}

int
show_summary_msg(struct imsg *imsg)
{
	struct peer		*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		printf("%-15s %5u %10llu %10llu %8s %s\n",
		    inet_ntoa(p->conf.remote_addr.sin_addr),
		    p->conf.remote_as, p->stats.msg_rcvd,
		    p->stats.msg_send, fmt_timeframe(p->stats.last_updown),
		    statenames[p->state]);
		break;
	case IMSG_CTL_END:
		return (1);
		break;
	default:
		break;
	}

	return (0);
}

int
show_neighbor_msg(struct imsg *imsg)
{
	struct peer		*p;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_NEIGHBOR:
		p = imsg->data;
		printf("BGP neighbor is %s, remote AS %u\n",
		    inet_ntoa(p->conf.remote_addr.sin_addr),
		    p->conf.remote_as);
		if (p->conf.descr[0])
			printf(" Description: %s\n", p->conf.descr);
		printf("  BGP version 4, remote router-id %s\n",
		    log_ntoa(p->remote_bgpid));
		printf("  BGP state = %s", statenames[p->state]);
		if (p->stats.last_updown != 0)
			printf(", %s for %s",
			    p->state == STATE_ESTABLISHED ? "up" : "down",
			    fmt_timeframe(p->stats.last_updown));
		printf("\n");
		printf("  Last read %s, holdtime %us, keepalive interval %us\n",
		    fmt_timeframe(p->stats.last_read),
		    p->holdtime, p->holdtime/3);
		printf("\n");
		break;
	case IMSG_CTL_END:
		return (1);
		break;
	default:
		break;
	}

	return (0);
}

#define TF_BUFS	8
#define TF_LEN	9

static char *
fmt_timeframe(time_t t)
{
	char		*buf;
	static char	 tfbuf[TF_BUFS][TF_LEN];	/* ring buffer */
	static int	 idx = 0;
	unsigned	 sec, min, hrs, day, week;

	buf = tfbuf[idx++];
	if (idx == TF_BUFS)
		idx = 0;

	if (t == 0) {
		snprintf(buf, TF_LEN, "%s", "Never");
		return (buf);
	}

	week = time(NULL) - t;

	sec = week % 60;
	week /= 60;
	min = week % 60;
	week /= 60;
	hrs = week % 24;
	week /= 24;
	day = week % 7;
	week /= 7;

	if (week > 0)
		snprintf(buf, TF_LEN, "%02uw%01ud%02uh", week, day, hrs);
	else if (day > 0)
		snprintf(buf, TF_LEN, "%01ud%02uh%02um", day, hrs, min);
	else
		snprintf(buf, TF_LEN, "%02u:%02u:%02u", hrs, min, sec);

	return (buf);
}