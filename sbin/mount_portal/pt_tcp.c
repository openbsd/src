/*	$OpenBSD: pt_tcp.c,v 1.13 2007/12/09 20:54:01 jmc Exp $	*/

/*
 * Copyright (c) 2004 Pedro Martelletto <pedro@ambientworks.net>
 * All rights reserved.
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

#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>

#include "portald.h"

/*
 * Keys should follow the format: tcp/[4||6]/host/port/["priv"]
 */
int
portal_tcp(struct portal_cred *pcr, char *key, char **v, int ks, int *fdp)
{
	char **tp, *tokens[5];
	int priv, s, tc, n;
	struct addrinfo aih, *ai, *ail;

	if (!strlen(key) || key[strlen(key) - 1] == '/')
		return (EINVAL);

	tc = 0;
	for (tp = tokens; tp < &tokens[5] &&
	    (*tp = strsep(&key, "/")) != NULL;)
		if (**tp != '\0') {
			tp++;
			tc++;
		}

	if (tc < 3)
		return (EINVAL);

	memset(&aih, 0x0, sizeof(aih));
	aih.ai_socktype = SOCK_STREAM;
	aih.ai_family = PF_UNSPEC;

	priv = 0;
	tp = tokens;
	if (tc > 3) {
		if (!strcmp(tokens[1], "4"))
			aih.ai_family = PF_INET;
		else if (!strcmp(tokens[1], "6"))
			aih.ai_family = PF_INET6;

		if (aih.ai_family != PF_UNSPEC) {
			tp++;
			tc--;
		}

		if (tc > 4)
			return (EINVAL);

		if (tc > 3) {
			if (!strcmp(tp[tc - 1], "priv")) {
				if (pcr->pcr_uid == 0)
					priv = 1;
				else
					return (EPERM);
			} else
				return (EINVAL);
		}
	}

	n = getaddrinfo(tp[1], tp[2], &aih, &ail);
	if (n) {
		syslog(LOG_ERR, "getaddrinfo: %s", gai_strerror(n));
		return (EINVAL);
	}

	s = -1;

	for (ai = ail; ai != NULL; ai = ai->ai_next) {
		if (priv)
			s = rresvport(NULL);
		else
			s = socket(ai->ai_family, ai->ai_socktype,
			        ai->ai_protocol);
		if (s < 0) {
			syslog(LOG_ERR, "socket: %m");
			continue;
		}

		n = connect(s, ai->ai_addr, ai->ai_addrlen);
		if (!n)
			break;

		syslog(LOG_ERR, "connect: %m");
		close(s);
		s = -1;
	}

	freeaddrinfo(ail);

	if (s == -1)
		return (errno);

	*fdp = s;
	return (0);
}
