/*	$OpenBSD: bgpctl.c,v 1.1 2004/01/02 02:22:52 henning Exp $ */

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

int	main(int, char *[]);

struct imsgbuf	ibuf;

static const char *statenames[] = {
	"None",
	"Idle",
	"Connect",
	"Active",
	"OpenSent",
	"OpenConfirm",
	"Established"
};

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	int			 fd, n;
	struct imsg		 imsg;
	struct peer		*p;

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
	imsg_compose(&ibuf, IMSG_CTL_SHOW_NEIGHBOR, 0, NULL, 0);
	imsg_read(&ibuf);

	for (;;) {
		if ((n = imsg_get(&ibuf, &imsg)) == -1)
			errx(1, "imsg_get error");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_NEIGHBOR:
			p = imsg.data;
			printf("%s: %s\n",
			    inet_ntoa(p->conf.remote_addr.sin_addr),
			    statenames[p->state]);
			break;
		default:
			break;
		}
	}

	close(fd);
}
