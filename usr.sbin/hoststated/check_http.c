/*	$OpenBSD: check_http.c,v 1.10 2007/01/12 16:43:01 pyr Exp $	*/
/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <sys/param.h>

#include <net/if.h>
#include <sha1.h>
#include <limits.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "hoststated.h"

int
check_http_code(struct ctl_tcp_event *cte)
{
	char		*head;
	char		 scode[4];
	const char	*estr;
	int		 code;

	head = cte->buf->buf;
	if (strncmp(head, "HTTP/1.1 ", strlen("HTTP/1.1 ")) &&
	    strncmp(head, "HTTP/1.0 ", strlen("HTTP/1.0 "))) {
		log_debug("check_http_code: cannot parse HTTP version");
		cte->host->up = HOST_DOWN;
		return (1);
	}
	head += strlen("HTTP/1.1 ");
	if (strlen(head) < 5) /* code + \r\n */ {
		cte->host->up = HOST_DOWN;
		return (1);
	}
	strlcpy(scode, head, sizeof(scode));
	code = strtonum(scode, 100, 999, &estr);
	if (estr != NULL) {
		log_debug("check_http_code: cannot parse HTTP code");
		cte->host->up = HOST_DOWN;
		return (1);
	}
	if (code != cte->table->retcode) {
		log_debug("check_http_code: invalid HTTP code returned");
		cte->host->up = HOST_DOWN;
	} else
		cte->host->up = HOST_UP;
	return (!(cte->host->up == HOST_UP));
}

int
check_http_digest(struct ctl_tcp_event *cte)
{
	char	*head;
	char	 digest[(SHA1_DIGEST_LENGTH*2)+1];

	head = cte->buf->buf;
	if ((head = strstr(head, "\r\n\r\n")) == NULL) {
		log_debug("check_http_digest: host %u no end of headers",
		    cte->host->id);
		cte->host->up = HOST_DOWN;
		return (1);
	}
	head += strlen("\r\n\r\n");
	SHA1Data(head, strlen(head), digest);

	if (strcmp(cte->table->digest, digest)) {
		log_warnx("check_http_digest: wrong digest for host %u",
		    cte->host->id);
		cte->host->up = HOST_DOWN;
	} else
		cte->host->up = HOST_UP;
	return (!(cte->host->up == HOST_UP));
}
