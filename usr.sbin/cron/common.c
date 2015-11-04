/*	$OpenBSD: common.c,v 1.2 2015/11/04 20:28:17 millert Exp $	*/

/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <bitstring.h>		/* for structs.h */
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "pathnames.h"
#include "macros.h"
#include "structs.h"
#include "funcs.h"
#include "globals.h"

void
set_cron_cwd(void)
{
	struct stat sb;
	struct group *grp = NULL;

	grp = getgrnam(CRON_GROUP);
	/* first check for CRONDIR ("/var/cron" or some such)
	 */
	if (stat(CRONDIR, &sb) < 0 && errno == ENOENT) {
		perror(CRONDIR);
		if (0 == mkdir(CRONDIR, 0710)) {
			fprintf(stderr, "%s: created\n", CRONDIR);
			stat(CRONDIR, &sb);
		} else {
			fprintf(stderr, "%s: ", CRONDIR);
			perror("mkdir");
			exit(EXIT_FAILURE);
		}
	}
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			CRONDIR);
		exit(EXIT_FAILURE);
	}
	if (chdir(CRONDIR) < 0) {
		fprintf(stderr, "cannot chdir(%s), bailing out.\n", CRONDIR);
		perror(CRONDIR);
		exit(EXIT_FAILURE);
	}

	/* CRONDIR okay (now==CWD), now look at SPOOL_DIR ("tabs" or some such)
	 */
	if (stat(SPOOL_DIR, &sb) < 0 && errno == ENOENT) {
		perror(SPOOL_DIR);
		if (0 == mkdir(SPOOL_DIR, 0700)) {
			fprintf(stderr, "%s: created\n", SPOOL_DIR);
			stat(SPOOL_DIR, &sb);
		} else {
			fprintf(stderr, "%s: ", SPOOL_DIR);
			perror("mkdir");
			exit(EXIT_FAILURE);
		}
	}
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			SPOOL_DIR);
		exit(EXIT_FAILURE);
	}
	if (grp != NULL) {
		if (sb.st_gid != grp->gr_gid)
			chown(SPOOL_DIR, -1, grp->gr_gid);
		if (sb.st_mode != 01730)
			chmod(SPOOL_DIR, 01730);
	}

	/* finally, look at AT_DIR ("atjobs" or some such)
	 */
	if (stat(AT_DIR, &sb) < 0 && errno == ENOENT) {
		perror(AT_DIR);
		if (0 == mkdir(AT_DIR, 0700)) {
			fprintf(stderr, "%s: created\n", AT_DIR);
			stat(AT_DIR, &sb);
		} else {
			fprintf(stderr, "%s: ", AT_DIR);
			perror("mkdir");
			exit(EXIT_FAILURE);
		}
	}
	if (!S_ISDIR(sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory, bailing out.\n",
			AT_DIR);
		exit(EXIT_FAILURE);
	}
	if (grp != NULL) {
		if (sb.st_gid != grp->gr_gid)
			chown(AT_DIR, -1, grp->gr_gid);
		if (sb.st_mode != 01770)
			chmod(AT_DIR, 01770);
	}
}

int
strtot(const char *nptr, char **endptr, time_t *tp)
{
	long long ll;

	errno = 0;
	ll = strtoll(nptr, endptr, 10);
	if (*endptr == nptr)
		return (-1);
	if (ll < 0 || (errno == ERANGE && ll == LLONG_MAX) || (time_t)ll != ll)
		return (-1);
	*tp = (time_t)ll;
	return (0);
}
