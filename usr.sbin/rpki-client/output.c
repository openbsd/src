/*	$OpenBSD: output.c,v 1.15 2020/05/03 20:24:02 deraadt Exp $ */
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

#include <openssl/x509v3.h>

#include "extern.h"

char		*outputdir;
int		 outformats;

static char	 output_tmpname[PATH_MAX];
static char	 output_name[PATH_MAX];

static const struct outputs {
	int	 format;
	char	*name;
	int	(*fn)(FILE *, struct vrp_tree *, struct stats *);
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
outputfiles(struct vrp_tree *v, struct stats *st)
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
		if ((*outputs[i].fn)(fout, v, st) != 0) {
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

	r = snprintf(output_name, sizeof output_name,
	    "%s/%s", outputdir, name);
	if (r < 0 || r > (int)sizeof(output_name))
		err(1, "path too long");
	r = snprintf(output_tmpname, sizeof output_tmpname,
	    "%s.XXXXXXXXXXX", output_name);
	if (r < 0 || r > (int)sizeof(output_tmpname))
		err(1, "path too long");
	fd = mkostemp(output_tmpname, O_CLOEXEC);
	if (fd == -1)
		err(1, "mkostemp");
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

	time(&t);
	setenv("TZ", "UTC", 1);
	tp = localtime(&t);
	strftime(tbuf, sizeof tbuf, "%a %b %e %H:%M:%S %Z %Y", tp);

	gethostname(hn, sizeof hn);

	if (fprintf(out,
	    "# Generated on host %s at %s\n"
	    "# Processing time %lld seconds (%lld seconds user, %lld seconds system)\n"
	    "# Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)\n"
	    "# Certificates: %zu (%zu failed parse, %zu invalid)\n"
	    "# Trust Anchor Locators: %zu (%s)\n"
	    "# Manifests: %zu (%zu failed parse, %zu stale)\n"
	    "# Certificate revocation lists: %zu\n"
	    "# Repositories: %zu\n"
	    "# VRP Entries: %zu (%zu unique)\n",
	    hn, tbuf, (long long)st->elapsed_time.tv_sec,
	    (long long)st->user_time.tv_sec, (long long)st->system_time.tv_sec,
            st->roas, st->roas_fail, st->roas_invalid,
            st->certs, st->certs_fail, st->certs_invalid,
	    st->tals, st->talnames,
	    st->mfts, st->mfts_fail, st->mfts_stale,
	    st->crls,
	    st->repos,
	    st->vrps, st->uniqs) < 0)
		return -1;
	return 0;
}
