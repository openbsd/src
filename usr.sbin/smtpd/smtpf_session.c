/*	$OpenBSD: smtpf_session.c,v 1.1 2017/05/22 13:40:54 gilles Exp $	*/

/*
 * Copyright (c) 2017 Gilles Chehade <gilles@poolp.org>
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
#include <sys/tree.h>
#include <sys/socket.h>

#include <errno.h>
#include <imsg.h>
#include <limits.h>
#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"

static void smtpf_session_init(void);

static void
smtpf_session_init(void)
{
	static int	init = 0;

	if (!init)
		init = 1;
}

int
smtpf_session(struct listener *listener, int sock,
    const struct sockaddr_storage *ss, const char *hostname)
{
	log_debug("debug: smtpf: new client on listener: %p", listener);

	smtpf_session_init();

	errno = EOPNOTSUPP;
	return (-1);
}

void
smtpf_session_imsg(struct mproc *p, struct imsg *imsg)
{
}
