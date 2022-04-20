/*	$OpenBSD: output.c,v 1.26 2022/04/20 15:29:24 tb Exp $ */
/*
 * Copyright (c) 2019 Theo de Raadt <deraadt@openbsd.org>
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

/*-
 * Copyright (C) 2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2012 Oleg Moskalenko <mom040267@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

#include "extern.h"

int		 outformats;

static char	 output_tmpname[PATH_MAX];
static char	 output_name[PATH_MAX];

static const struct outputs {
	int	 format;
	char	*name;
	int	(*fn)(FILE *, struct vrp_tree *, struct brk_tree *,
		    struct stats *);
} outputs[] = {
	{ FORMAT_OPENBGPD, "openbgpd", output_bgpd },
	{ FORMAT_BIRD, "bird1v4", output_bird1v4 },
	{ FORMAT_BIRD, "bird1v6", output_bird1v6 },
	{ FORMAT_BIRD, "bird", output_bird2 },
	{ FORMAT_CSV, "csv", output_csv },
	{ FORMAT_JSON, "json", output_json },
	{ 0, NULL }
};

static FILE	*output_createtmp(char *);
static void	 output_cleantmp(void);
static int	 output_finish(FILE *);
static void	 sig_handler(int);
static void	 set_signal_handler(void);

int
outputfiles(struct vrp_tree *v, struct brk_tree *b, struct stats *st)
{
	int i, rc = 0;

	atexit(output_cleantmp);
	set_signal_handler();

	for (i = 0; outputs[i].name; i++) {
		FILE *fout;

		if (!(outformats & outputs[i].format))
			continue;

		fout = output_createtmp(outputs[i].name);
		if (fout == NULL) {
			warn("cannot create %s", outputs[i].name);
			rc = 1;
			continue;
		}
		if ((*outputs[i].fn)(fout, v, b, st) != 0) {
			warn("output for %s format failed", outputs[i].name);
			fclose(fout);
			output_cleantmp();
			rc = 1;
			continue;
		}
		if (output_finish(fout) != 0) {
			warn("finish for %s format failed", outputs[i].name);
			output_cleantmp();
			rc = 1;
			continue;
		}
	}

	return rc;
}

static FILE *
output_createtmp(char *name)
{
	FILE *f;
	int fd, r;

	if (strlcpy(output_name, name, sizeof output_name) >=
	    sizeof output_name)
		err(1, "path too long");
	r = snprintf(output_tmpname, sizeof output_tmpname,
	    "%s.XXXXXXXXXXX", output_name);
	if (r < 0 || r > (int)sizeof(output_tmpname))
		err(1, "path too long");
	fd = mkostemp(output_tmpname, O_CLOEXEC);
	if (fd == -1)
		err(1, "mkostemp: %s", output_tmpname);
	(void) fchmod(fd, 0644);
	f = fdopen(fd, "w");
	if (f == NULL)
		err(1, "fdopen");
	return f;
}

static int
output_finish(FILE *out)
{
	if (fclose(out) != 0)
		return -1;
	if (rename(output_tmpname, output_name) == -1)
		return -1;
	output_tmpname[0] = '\0';
	return 0;
}

static void
output_cleantmp(void)
{
	if (*output_tmpname)
		unlink(output_tmpname);
	output_tmpname[0] = '\0';
}

/*
 * Signal handler that clears the temporary files.
 */
static void
sig_handler(int sig)
{
	output_cleantmp();
	_exit(2);
}

/*
 * Set signal handler on panic signals.
 */
static void
set_signal_handler(void)
{
	struct sigaction sa;
	int i, signals[] = {SIGTERM, SIGHUP, SIGINT, SIGUSR1, SIGUSR2,
	    SIGPIPE, SIGXCPU, SIGXFSZ, 0};

	memset(&sa, 0, sizeof(sa));
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_handler;

	for (i = 0; signals[i] != 0; i++) {
		if (sigaction(signals[i], &sa, NULL) == -1) {
			warn("sigaction(%s)", strsignal(signals[i]));
			continue;
		}
	}
}

int
outputheader(FILE *out, struct stats *st)
{
	char		hn[NI_MAXHOST], tbuf[80];
	struct tm	*tp;
	time_t		t;
	int		i;

	time(&t);
	tp = gmtime(&t);
	strftime(tbuf, sizeof tbuf, "%a %b %e %H:%M:%S UTC %Y", tp);

	gethostname(hn, sizeof hn);

	if (fprintf(out,
	    "# Generated on host %s at %s\n"
	    "# Processing time %lld seconds (%llds user, %llds system)\n"
	    "# Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)\n"
	    "# BGPsec Router Certificates: %zu\n"
	    "# Certificates: %zu (%zu invalid)\n",
	    hn, tbuf, (long long)st->elapsed_time.tv_sec,
	    (long long)st->user_time.tv_sec, (long long)st->system_time.tv_sec,
	    st->roas, st->roas_fail, st->roas_invalid,
	    st->brks, st->certs, st->certs_fail) < 0)
		return -1;

	if (fprintf(out,
	    "# Trust Anchor Locators: %zu (%zu invalid) [", st->tals,
	    talsz - st->tals) < 0)
		return -1;
	for (i = 0; i < talsz; i++)
		if (fprintf(out, " %s", tals[i]) < 0)
			return -1;

	if (fprintf(out,
	    " ]\n"
	    "# Manifests: %zu (%zu failed parse, %zu stale)\n"
	    "# Certificate revocation lists: %zu\n"
	    "# Ghostbuster records: %zu\n"
	    "# Repositories: %zu\n"
	    "# VRP Entries: %zu (%zu unique)\n",
	    st->mfts, st->mfts_fail, st->mfts_stale,
	    st->crls,
	    st->gbrs,
	    st->repos,
	    st->vrps, st->uniqs) < 0)
		return -1;
	return 0;
}
