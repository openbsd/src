/*	$OpenBSD: whois.c,v 1.4 2009/09/08 15:40:25 claudio Exp $ */

/*
 * Copyright (c) 2007 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "irrfilter.h"

#define WHOIS_STDOPTS	"-r -a"

char *qtype_opts[] = {
	"",
	"-T aut-num",
	"-K -T as-set",
	"-K -T route -i origin",
	"-K -T route6 -i origin"
};

char	*server = "whois.radb.net";
char	*port = "whois";

int
whois(const char *query, enum qtype qtype)
{
	FILE		*sfw, *sfr;
	int		 s, r = -1, error = 0, attempt, ret;
	struct addrinfo	 hints, *res, *ai;
	const char	*reason = NULL;
	char		*fmt;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(server, port, &hints, &res);
	if (error) {
		if (error == EAI_SERVICE)
			warnx("%s: bad port", port);
		else
			warnx("%s: %s", server, gai_strerror(error));
		return (1);
	}

	for (s = -1, ai = res; ai != NULL; ai = ai->ai_next) {
		attempt = 0;
		do {
			attempt++;
			if (s != -1)
				close(s);
			s = socket(ai->ai_family, ai->ai_socktype,
			    ai->ai_protocol);
			if (s == -1) {
				error = errno;
				reason = "socket";
			} else
				r = connect(s, ai->ai_addr, ai->ai_addrlen);
		} while (r == -1 && errno == ETIMEDOUT && attempt <= 3);

		if (r == -1) {
			error = errno;
			reason = "connect";
			close(s);
			s = -1;
			continue;
		}
		if (s != -1)
			break;	/*okay*/
	}
	freeaddrinfo(res);

	if (s == -1) {
		if (reason) {
			errno = error;
			warn("%s: %s", server, reason);
		} else
			warn("unknown error in connection attempt");
		return (1);
	}

	sfr = fdopen(s, "r");
	sfw = fdopen(s, "w");
	if (sfr == NULL || sfw == NULL)
		err(1, "fdopen");
	fmt = "%s %s %s\r\n";
	fprintf(sfw, fmt, WHOIS_STDOPTS, qtype_opts[qtype], query);
	fflush(sfw);

	if ((ret = parse_response(sfr, qtype)) == -1)
		warnx("parse error, query=\"%s %s\"", qtype_opts[qtype], query);

	fclose(sfw);
	fclose(sfr);
	close(s);
	return (ret);
}
