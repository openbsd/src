/*	$OpenBSD: mda_unpriv.c,v 1.10 2026/09/11 14:43:29 gilles Exp $	*/

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

#include <err.h>
#include <paths.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

static void
mda_escape(char *dst, size_t dstsz, const char *src)
{
	size_t	i;

	if (strlcpy(dst, src, dstsz) >= dstsz)
		errx(1, "mda environment value too long");
	for (i = 0; dst[i] != '\0'; i++)
		if (strchr(MAILADDR_RAW_ESCAPE, dst[i]))
			dst[i] = ':';
}

void
mda_unpriv(struct dispatcher *dsp, struct deliver *deliver,
    const char *pw_name, const char *pw_dir)
{
	int		idx;
	char		s_user[SMTPD_MAXLOCALPARTSIZE];
	char		s_domain[SMTPD_MAXDOMAINPARTSIZE];
	char		d_user[SMTPD_MAXLOCALPARTSIZE];
	char		d_domain[SMTPD_MAXDOMAINPARTSIZE];
	char		r_user[SMTPD_MAXLOCALPARTSIZE];
	char		r_domain[SMTPD_MAXDOMAINPARTSIZE];
	char		subaddr[SMTPD_SUBADDRESS_SIZE];
	char	       *mda_environ[12];
	char		mda_exec[LINE_MAX];
	char		mda_wrapper[LINE_MAX];
	const char     *mda_command;
	const char     *mda_command_wrap;

	if (deliver->mda_exec[0])
		mda_command = deliver->mda_exec;
	else
		mda_command = dsp->u.local.command;

	if (strlcpy(mda_exec, mda_command, sizeof (mda_exec))
	    >= sizeof (mda_exec))
		errx(1, "mda command line too long");

	if (mda_expand_format(mda_exec, sizeof mda_exec, deliver,
		&deliver->userinfo, NULL) == -1)
		errx(1, "mda command line could not be expanded");

	mda_command = mda_exec;

	mda_escape(r_user, sizeof r_user, deliver->rcpt.user);
	mda_escape(r_domain, sizeof r_domain, deliver->rcpt.domain);
	mda_escape(d_user, sizeof d_user, deliver->dest.user);
	mda_escape(d_domain, sizeof d_domain, deliver->dest.domain);
	mda_escape(s_user, sizeof s_user, deliver->sender.user);
	mda_escape(s_domain, sizeof s_domain, deliver->sender.domain);
	mda_escape(subaddr, sizeof subaddr, deliver->mda_subaddress);

	/* setup environment similar to other MTA */
	idx = 0;
	xasprintf(&mda_environ[idx++], "PATH=%s", _PATH_DEFPATH);
	xasprintf(&mda_environ[idx++], "DOMAIN=%s", r_domain);
	xasprintf(&mda_environ[idx++], "HOME=%s", pw_dir);
	xasprintf(&mda_environ[idx++], "ORIGINAL_RECIPIENT=%s@%s", r_user, r_domain);
	xasprintf(&mda_environ[idx++], "RECIPIENT=%s@%s", d_user, d_domain);
	xasprintf(&mda_environ[idx++], "SHELL=/bin/sh");
	xasprintf(&mda_environ[idx++], "LOCAL=%s", r_user);
	xasprintf(&mda_environ[idx++], "LOGNAME=%s", deliver->userinfo.username);
	xasprintf(&mda_environ[idx++], "USER=%s", deliver->userinfo.username);

	if (deliver->sender.user[0])
		xasprintf(&mda_environ[idx++], "SENDER=%s@%s",
		    s_user, s_domain);
	else
		xasprintf(&mda_environ[idx++], "SENDER=");

	if (deliver->mda_subaddress[0])
		xasprintf(&mda_environ[idx++], "EXTENSION=%s", subaddr);

	mda_environ[idx++] = (char *)NULL;

	if (dsp->u.local.mda_wrapper) {
		mda_command_wrap = dict_get(env->sc_mda_wrappers,
		    dsp->u.local.mda_wrapper);
		if (mda_command_wrap == NULL)
			errx(1, "could not find wrapper %s",
			    dsp->u.local.mda_wrapper);

		if (strlcpy(mda_wrapper, mda_command_wrap, sizeof (mda_wrapper))
		    >= sizeof (mda_wrapper))
			errx(1, "mda command line too long");

		if (mda_expand_format(mda_wrapper, sizeof mda_wrapper, deliver,
			&deliver->userinfo, mda_command) == -1)
			errx(1, "mda command line could not be expanded");
		mda_command = mda_wrapper;
	}
	execle("/bin/sh", "/bin/sh", "-c", mda_command, (char *)NULL,
            mda_environ);

	perror("execle");
	_exit(1);
}

