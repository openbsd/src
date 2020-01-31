/*	$OpenBSD: mda_mbox.c,v 1.1 2020/01/31 22:01:20 gilles Exp $	*/

/*
 * Copyright (c) 2018 Gilles Chehade <gilles@poolp.org>
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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"


void
mda_mbox(struct deliver *deliver)
{
	int		ret;
	char		sender[LINE_MAX];
	char		*envp[] = {
		"HOME=/",
		"PATH=" _PATH_DEFPATH,
		"LOGNAME=root",
		"USER=root",
		NULL,
	};

	if (deliver->sender.user[0] == '\0' &&
	    deliver->sender.domain[0] == '\0')
		ret = snprintf(sender, sizeof sender, "MAILER-DAEMON");
	else
		ret = snprintf(sender, sizeof sender, "%s@%s",
			       deliver->sender.user, deliver->sender.domain);
	if (ret < 0 || (size_t)ret >= sizeof sender)
		errx(1, "sender address too long");

	execle(PATH_MAILLOCAL, PATH_MAILLOCAL, "-f",
	       sender, deliver->userinfo.username, (char *)NULL, envp);
	perror("execl");
	_exit(1);
}
