/*	$Id: fargs.c,v 1.17 2019/05/08 20:00:25 benno Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

#define	RSYNC_PATH	"rsync"

char **
fargs_cmdline(struct sess *sess, const struct fargs *f, size_t *skip)
{
	arglist		 args;
	size_t		 j;
	char		*rsync_path, *ap, *arg;

	memset(&args, 0, sizeof args);

	assert(f != NULL);
	assert(f->sourcesz > 0);

	if ((rsync_path = sess->opts->rsync_path) == NULL)
		rsync_path = RSYNC_PATH;

	if (f->host != NULL) {
		/*
		 * Splice arguments from -e "foo bar baz" into array
		 * elements required for execve(2).
		 * This doesn't do anything fancy: it splits along
		 * whitespace into the array.
		 */

		if (sess->opts->ssh_prog) {
			ap = strdup(sess->opts->ssh_prog);
			if (ap == NULL)
				goto out;

			while ((arg = strsep(&ap, " \t")) != NULL) {
				if (arg[0] == '\0') {
					ap++;	/* skip seperators */
					continue;
				}

				addargs(&args, "%s", arg);
			}
		} else
			addargs(&args, "ssh");

		addargs(&args, "%s", f->host);
		addargs(&args, "%s", rsync_path);
		if (skip)
			*skip = args.num;
		addargs(&args, "--server");
		if (f->mode == FARGS_RECEIVER)
			addargs(&args, "--sender");
	} else {
		addargs(&args, "%s", rsync_path);
		addargs(&args, "--server");
	}

	/* Shared arguments. */

	if (sess->opts->del)
		addargs(&args, "--delete");
	if (sess->opts->numeric_ids)
		addargs(&args, "--numeric-ids");
	if (sess->opts->preserve_gids)
		addargs(&args, "-g");
	if (sess->opts->preserve_links)
		addargs(&args, "-l");
	if (sess->opts->dry_run)
		addargs(&args, "-n");
	if (sess->opts->preserve_uids)
		addargs(&args, "-o");
	if (sess->opts->preserve_perms)
		addargs(&args, "-p");
	if (sess->opts->devices)
		addargs(&args, "-D");
	if (sess->opts->recursive)
		addargs(&args, "-r");
	if (sess->opts->preserve_times)
		addargs(&args, "-t");
	if (verbose > 3)
		addargs(&args, "-v");
	if (verbose > 2)
		addargs(&args, "-v");
	if (verbose > 1)
		addargs(&args, "-v");
	if (verbose > 0)
		addargs(&args, "-v");
	if (sess->opts->one_file_system > 1)
		addargs(&args, "-x");
	if (sess->opts->one_file_system > 0)
		addargs(&args, "-x");
	if (sess->opts->specials && !sess->opts->devices)
		addargs(&args, "--specials");
	if (!sess->opts->specials && sess->opts->devices)
		/* --devices is sent as -D --no-specials */
		addargs(&args, "--no-specials");

	/* Terminate with a full-stop for reasons unknown. */

	addargs(&args, ".");

	if (f->mode == FARGS_RECEIVER) {
		for (j = 0; j < f->sourcesz; j++)
			addargs(&args, "%s", f->sources[j]);
	} else
		addargs(&args, "%s", f->sink);

	return args.list;
out:
	freeargs(&args);
	ERR("calloc");
	return NULL;
}
