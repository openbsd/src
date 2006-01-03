/* $OpenBSD: tcpdrop.c,v 1.5 2006/01/03 01:46:27 stevesk Exp $ */

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

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>

extern char *__progname;

/*
 * Drop a tcp connection.
 */
int
main(int argc, char **argv)
{
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_DROP };
	struct addrinfo hints, *ail, *aif, *laddr, *faddr;
	char fhbuf[NI_MAXHOST], fsbuf[NI_MAXSERV];
	char lhbuf[NI_MAXHOST], lsbuf[NI_MAXSERV];
	struct tcp_ident_mapping tir;
	int gaierr, rval = 0;

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

			if ((gaierr = getnameinfo(aif->ai_addr, aif->ai_addrlen,
			    fhbuf, sizeof(fhbuf),
			    fsbuf, sizeof(fsbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
				errx(1, "getnameinfo: %s", gai_strerror(gaierr));
			if ((gaierr = getnameinfo(ail->ai_addr, ail->ai_addrlen,
			    lhbuf, sizeof(lhbuf),
			    lsbuf, sizeof(lsbuf),
			    NI_NUMERICHOST | NI_NUMERICSERV)) != 0)
				errx(1, "getnameinfo: %s", gai_strerror(gaierr));

			if (sysctl(mib, sizeof (mib) / sizeof (int), NULL,
			    NULL, &tir, sizeof(tir)) == -1) {
				rval = 1;
				warn("%s %s %s %s", lhbuf, lsbuf, fhbuf, fsbuf);
			} else
				printf("%s %s %s %s: dropped\n",
				    lhbuf, lsbuf, fhbuf, fsbuf);

		}
	}
	freeaddrinfo(laddr);
	freeaddrinfo(faddr);
	exit(rval);
}
