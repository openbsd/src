/* $OpenBSD: fuse_opt.c,v 1.3 2013/06/12 22:42:52 tedu Exp $ */
/*
 * Copyright (c) 2013 Sylvestre Gallon <ccna.syl@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "fuse_opt.h"
#include "fuse_private.h"

int
fuse_opt_add_arg(struct fuse_args *args, const char *name)
{
	char **av;
	int i;

	/* copy argv, we cannot reallocate the orignal argv */
	if (args->allocated == 0) {
		av = malloc((args->argc + 1) * sizeof(*av));
		if (av == NULL)
			return (-1);

		for (i = 0 ; i < args->argc ; i++) {
			av[i] = strdup(args->argv[i]);
			if (av[i] == NULL)
				return (-1);
		}

		av[args->argc] = strdup(name);
		if (av[args->argc] == NULL)
			return (-1);

		args->argc++;
		args->argv = av;
		args->allocated = 1;
	} else {
		av = realloc(args->argv, (args->argc + 1) * sizeof(*av));
		if (av == NULL)
			return (-1);

		args->argv = av;
		args->argc++;
		args->argv[args->argc - 1] = strdup(name);
		if (args->argv[args->argc - 1] == NULL)
			return (-1);
	}

	return (0);
}

int
fuse_opt_parse(struct fuse_args *args, void *data, struct fuse_opt *opt,
    fuse_opt_proc_t f)
{
	const char *arg;
	struct fuse_opt *good;
	int ret;
	int i, j;

	for (i = 0 ; i < args->argc ; i++) {
		arg = args->argv[i];

		/* not - and not -- */
		if (arg[0] != '-') {
			ret = (f) ? f(data, arg, FUSE_OPT_KEY_NONOPT, 0) : 0;

			if (ret == -1)
				return (ret);
		} else {
			switch (arg[1]) {
			case 'o':
				DPRINTF("%s: -o X,Y not supported yet.\n",
				    __func__);
				break ;
			case '-':
				DPRINTF("%s: long option not supported yet.",
				    __func__);
				break ;
			default:
				good = NULL;
				for (j = 0 ; opt[j].templ != NULL ; j++)
					if (strcmp(arg, opt[j].templ) == 0) {
						good = &opt[j];
						break ;
					}

				if (!good)
					break ;

				if (good->val == -1 && f) {
					ret = f(data, arg, good->val, 0);

					if (ret == -1)
						return (ret);
				}
				break;
			}
		}
	}
	return (0);
}

int
fuse_opt_insert_arg(struct fuse_args *args, int p, const char *arg)
{
	char **av;
	int i;

	if (p > args->argc + 1)
		return -1;

	if (args->allocated) {
		av = realloc(args->argv, (args->argc + 1) * sizeof(char *));
		if (av == NULL)
			return (-1);

		args->argv = av;
		args->argc++;
		for (i = args->argc - 1 ; i > p ; i++)
			args->argv[i] = args->argv[i - 1];

		args->argv[p] = strdup(arg);
		if (args->argv[p] == NULL)
			return (-1);
	}
	return (0);
}

int
fuse_opt_free_args(struct fuse_args *args)
{
	int i;

	if (args->allocated)
		for (i = 0 ; i < args->argc ; i++)
			free(args->argv[i]);
	free(args->argv);
	args->allocated = 0;
	args->argc = 0;

	return (0);
}
