/*	$OpenBSD: output.c,v 1.5 2019/12/19 16:32:44 claudio Exp $ */
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
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#include <openssl/x509v3.h>

#include "extern.h"

char		*outputdir;
char		 output_tmpname[PATH_MAX];
char		 output_name[PATH_MAX];

int		 outformats;

struct outputs {
	int	 format;
	char	*name;
	int	(*fn)(FILE *, struct vrp_tree *);
} outputs[] = {
	{ FORMAT_OPENBGPD, "openbgpd", output_bgpd },
	{ FORMAT_BIRD, "bird", output_bird },
	{ FORMAT_CSV, "csv", output_csv },
	{ FORMAT_JSON, "json", output_json },
	{ 0, NULL }
};

void		 sig_handler(int);
void		 set_signal_handler(void);

int
outputfiles(struct vrp_tree *v)
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
			logx("cannot create %s", outputs[i].name);
			rc = 1;
			continue;
		}
		if ((*outputs[i].fn)(fout, v) != 0) {
			logx("output for %s format failed", outputs[i].name);
			fclose(fout);
			output_cleantmp();
			rc = 1;
			continue;
		}
		output_finish(fout);
	}

	return rc;
}

FILE *
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

void
output_finish(FILE *out)
{
	fclose(out);
	out = NULL;

	rename(output_tmpname, output_name);
	output_tmpname[0] = '\0';
}

void
output_cleantmp(void)
{
	if (*output_tmpname)
		unlink(output_tmpname);
	output_tmpname[0] = '\0';
}

/*
 * Signal handler that clears the temporary files.
 */
void
sig_handler(int sig __unused)
{
	output_cleantmp();
	_exit(2);
}

/*
 * Set signal handler on panic signals.
 */
void
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
