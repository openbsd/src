/*	$OpenBSD: optionstest.c,v 1.1 2014/12/28 14:01:33 jsing Exp $	*/
/*
 * Copyright (c) 2014 Joel Sing <jsing@openbsd.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/conf.h>

#include "apps.h"

/* Needed to keep apps.c happy... */
BIO *bio_err;
CONF *config;

static int argfunc(struct option *opt, char *arg);

static struct {
	char *arg;
	int flag;
} test_config;

static struct option test_options[] = {
	{
		.name = "arg",
		.type = OPTION_ARG,
		.opt.arg = &test_config.arg,
	},
	{
		.name = "argfunc",
		.type = OPTION_ARG_FUNC,
		.func = argfunc,
	},
	{
		.name = "flag",
		.type = OPTION_FLAG,
		.opt.flag = &test_config.flag,
	},
	{ NULL },
};

char *args1[] = { "opts" };
char *args2[] = { "opts", "-arg", "arg", "-flag" };
char *args3[] = { "opts", "-arg", "arg", "-flag", "unnamed" };
char *args4[] = { "opts", "-arg", "arg", "unnamed", "-flag" };
char *args5[] = { "opts", "unnamed1", "-arg", "arg", "-flag", "unnamed2" };
char *args6[] = { "opts", "-argfunc", "arg", "-flag" };

struct options_test {
	int argc;
	char **argv;
	enum {
		OPTIONS_TEST_NONE,
		OPTIONS_TEST_UNNAMED,
	} type;
	char *unnamed;
	int used;
	int want;
	char *wantarg;
	int wantflag;
};

struct options_test options_tests[] = {
	{
		/* No arguments (only program name). */
		.argc = 1,
		.argv = args1,
		.type = OPTIONS_TEST_NONE,
		.want = 0,
		.wantarg = NULL,
		.wantflag = 0,
	},
	{
		/* Named arguments (unnamed not permitted). */
		.argc = 4,
		.argv = args2,
		.type = OPTIONS_TEST_NONE,
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
	{
		/* Named arguments (unnamed permitted). */
		.argc = 4,
		.argv = args2,
		.type = OPTIONS_TEST_UNNAMED,
		.unnamed = NULL,
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
	{
		/* Named and single unnamed (unnamed not permitted). */
		.argc = 5,
		.argv = args3,
		.type = OPTIONS_TEST_NONE,
		.want = 1,
	},
	{
		/* Named and single unnamed (unnamed permitted). */
		.argc = 5,
		.argv = args3,
		.type = OPTIONS_TEST_UNNAMED,
		.unnamed = "unnamed",
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
	{
		/* Named and single unnamed (different sequence). */
		.argc = 5,
		.argv = args4,
		.type = OPTIONS_TEST_UNNAMED,
		.unnamed = "unnamed",
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
	{
		/* Multiple unnamed arguments (retain last). */
		.argc = 6,
		.argv = args5,
		.type = OPTIONS_TEST_UNNAMED,
		.unnamed = "unnamed2",
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
	{
		/* Function. */
		.argc = 4,
		.argv = args6,
		.type = OPTIONS_TEST_NONE,
		.want = 0,
		.wantarg = "arg",
		.wantflag = 1,
	},
};

#define N_OPTIONS_TESTS \
    (sizeof(options_tests) / sizeof(*options_tests))

int
argfunc(struct option *opt, char *arg)
{
	test_config.arg = arg;
	return (0);
}

static int
do_options_test(int test_no, struct options_test *ot)
{
	char *unnamed = NULL;
	char **arg = NULL;
	int ret;

	if (ot->type == OPTIONS_TEST_UNNAMED)
		arg = &unnamed;

	memset(&test_config, 0, sizeof(test_config));
	ret = options_parse(ot->argc, ot->argv, test_options, arg);
	if (ret != ot->want) {
		fprintf(stderr, "FAIL: test %i options_parse() returned %i, "
		    "want %i\n", test_no, ret, ot->want);
		return (1);
	}
	if (ret != 0)
		return (0);

	if ((test_config.arg != NULL || ot->wantarg != NULL) &&
	    (test_config.arg == NULL || ot->wantarg == NULL ||
	     strcmp(test_config.arg, ot->wantarg) != 0)) {
		fprintf(stderr, "FAIL: test %i got arg '%s', want '%s'\n",
		    test_no, test_config.arg, ot->wantarg);
		return (1);
	}
	if (test_config.flag != ot->wantflag) {
		fprintf(stderr, "FAIL: test %i got flag %i, want %i\n",
		    test_no, test_config.flag, ot->wantflag);
		return (1);
	}
	if (ot->type == OPTIONS_TEST_UNNAMED &&
	    (unnamed != NULL || ot->unnamed != NULL) &&
	    (unnamed == NULL || ot->unnamed == NULL ||
	     strcmp(unnamed, ot->unnamed) != 0)) {
		fprintf(stderr, "FAIL: test %i got unnamed '%s', want '%s'\n",
		    test_no, unnamed, ot->unnamed);
		return (1);
	}

	return (0);
}

int
main(int argc, char **argv)
{
	int failed = 0;
	size_t i;

	for (i = 0; i < N_OPTIONS_TESTS; i++)
		failed += do_options_test(i, &options_tests[i]);

	return (failed);
}
