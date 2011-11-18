/*	$Id: apropos.c,v 1.7 2011/11/18 01:10:03 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "apropos_db.h"
#include "man_conf.h"
#include "mandoc.h"

static	int	 cmp(const void *, const void *);
static	void	 list(struct res *, size_t, void *);
static	void	 usage(void);

static	char	*progname;

int
apropos(int argc, char *argv[])
{
	struct man_conf	 dirs;
	int		 ch, use_man_conf;
	size_t		 terms;
	struct opts	 opts;
	struct expr	*e;
	extern int	 optind;
	extern char	*optarg;

	memset(&dirs, 0, sizeof(struct man_conf));
	memset(&opts, 0, sizeof(struct opts));
	use_man_conf = 1;

	progname = strrchr(argv[0], '/');
	if (progname == NULL)
		progname = argv[0];
	else
		++progname;

	while (-1 != (ch = getopt(argc, argv, "M:m:S:s:"))) 
		switch (ch) {
		case ('M'):
			use_man_conf = 0;
			/* FALLTHROUGH */
		case ('m'):
			manpath_parse(&dirs, optarg);
			break;
		case ('S'):
			opts.arch = optarg;
			break;
		case ('s'):
			opts.cat = optarg;
			break;
		default:
			usage();
			return(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if (0 == argc) 
		return(EXIT_SUCCESS);

	if (NULL == (e = exprcomp(argc, argv, &terms))) {
		fprintf(stderr, "Bad expression\n");
		return(EXIT_FAILURE);
	}

	/*
	 * Configure databases.
	 * The keyword database is a btree that allows for duplicate
	 * entries.
	 * The index database is a recno.
	 */

	if (use_man_conf)
		man_conf_parse(&dirs);
	ch = apropos_search(dirs.argc, dirs.argv, &opts,
			e, terms, NULL, list);

	man_conf_free(&dirs);
	exprfree(e);
	if (0 == ch)
		fprintf(stderr, "%s: Database error\n", progname);
	return(ch ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* ARGSUSED */
static void
list(struct res *res, size_t sz, void *arg)
{
	int		 i;

	qsort(res, sz, sizeof(struct res), cmp);

	for (i = 0; i < (int)sz; i++)
		printf("%s(%s%s%s) - %s\n", res[i].title, 
				res[i].cat, 
				*res[i].arch ? "/" : "",
				*res[i].arch ? res[i].arch : "",
				res[i].desc);
}

static int
cmp(const void *p1, const void *p2)
{

	return(strcmp(((const struct res *)p1)->title,
				((const struct res *)p2)->title));
}

static void
usage(void)
{

	fprintf(stderr, "usage: %s "
			"[-M path] "
			"[-m path] "
			"[-S arch] "
			"[-s section] "
			"expression...\n", 
			progname);
}
