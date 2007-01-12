/*	$OpenBSD: check_send_expect.c,v 1.5 2007/01/12 16:43:01 pyr Exp $ */
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
#include <limits.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <errno.h>

#include "hoststated.h"

int
check_send_expect(struct ctl_tcp_event *cte)
{
	u_char	*b;

	/*
	 * ensure string is nul-terminated.
	 */
	b = buf_reserve(cte->buf, 1);
	if (b == NULL)
		fatal("out of memory");
	*b = '\0';
	if (fnmatch(cte->table->exbuf, cte->buf->buf, 0) == 0) {
		cte->host->up = HOST_UP;
		return (0);
	}
	cte->host->up = HOST_UNKNOWN;

	/*
	 * go back to original position.
	 */
	cte->buf->wpos--;
	return (1);
}
