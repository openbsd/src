/* $OpenBSD: tcpdrop.c,v 1.1 2004/04/26 19:51:20 markus Exp $ */

/*
 * Copyright (c) 2004 Markus Friedl <markus@openbsd.org>
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <err.h>

extern char *__progname;

/*
 * Drop a tcp connection.
 */
int
main(int argc, char **argv)
{
	struct addrinfo hints, *ail, *aif, *laddr, *faddr;
	struct tcp_ident_mapping tir, dummy;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_DROP };
	int gaierr;
	size_t i = 1;

	if (argc != 5) {
		fprintf(stderr, "usage: %s laddr lport faddr fport\n",
		    __progname);
		exit(1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((gaierr = getaddrinfo(argv[1], argv[2], &hints, &laddr)) != 0)
		errx(1, "%s port %s: %s", argv[1], argv[2],
		    gai_strerror(gaierr));

	if ((gaierr = getaddrinfo(argv[3], argv[4], &hints, &faddr)) != 0) {
		freeaddrinfo(laddr);
		errx(1, "%s port %s: %s", argv[3], argv[4],
		    gai_strerror(gaierr));
	}

	for (ail = laddr; ail; ail = ail->ai_next) {
		for (aif = faddr; aif; aif = aif->ai_next) {
			if (ail->ai_family != aif->ai_family)
				continue;
			memcpy(&tir.faddr, aif->ai_addr, aif->ai_addrlen);
			memcpy(&tir.laddr, ail->ai_addr, ail->ai_addrlen);
			i = sizeof (tir);
			if (sysctl(mib, sizeof (mib) / sizeof (int), &dummy,
			    &i, &tir, i) == -1)
				warn(NULL);
		}
	}
	freeaddrinfo(laddr);
	freeaddrinfo(faddr);

	exit(0);
}
