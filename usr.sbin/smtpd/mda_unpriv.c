/*	$OpenBSD: mda_unpriv.c,v 1.1 2018/06/03 14:04:06 gilles Exp $	*/

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
mda_unpriv(struct dispatcher *dsp, struct deliver *deliver,
    const char *pw_name, const char *pw_dir)
{
	int		idx;
	char	       *mda_environ[10];
	char		mda_exec[LINE_MAX];
	const char     *mda_command;
	const char     *extension;

	if (deliver->mda_exec[0])
		mda_command = deliver->mda_exec;
	else
		mda_command = dsp->u.local.command;

	if (strlcpy(mda_exec, mda_command, sizeof (mda_exec))
	    >= sizeof (mda_exec))
		err(1, "mda command line too long");

	if (! mda_expand_format(mda_exec, sizeof mda_exec, deliver,
		&deliver->userinfo))
		err(1, "mda command line could not be expanded");

	/* setup environment similar to other MTA */
	idx = 0;
	xasprintf(&mda_environ[idx++], "PATH=%s", _PATH_DEFPATH);
	xasprintf(&mda_environ[idx++], "DOMAIN=%s", deliver->rcpt.domain);
	xasprintf(&mda_environ[idx++], "HOME=%s", pw_dir);
	xasprintf(&mda_environ[idx++], "RECIPIENT=%s@%s", deliver->dest.user, deliver->dest.domain);
	xasprintf(&mda_environ[idx++], "SHELL=/bin/sh");
	xasprintf(&mda_environ[idx++], "LOCAL=%s", deliver->rcpt.user);
	xasprintf(&mda_environ[idx++], "LOGNAME=%s", pw_name);
	xasprintf(&mda_environ[idx++], "USER=%s", pw_name);

	if ((extension = strchr(deliver->rcpt.user, *env->sc_subaddressing_delim)) != NULL)
		if (strlen(extension+1))
			xasprintf(&mda_environ[idx++], "EXTENSION=%s", extension+1);

	mda_environ[idx++] = (char *)NULL;

	execle("/bin/sh", "/bin/sh", "-c", mda_exec, (char *)NULL,
	    mda_environ);
	perror("execle");
	_exit(1);
}

