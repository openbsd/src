/*
 * Copyright (c) 2017 Helg Bredow <helg@openbsd.org>
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

#include <fuse_opt.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct data {
	int	port;
	char	*fsname;
	char	*x;
	char	*optstring;
	int	debug;
	int	noatime;
	int	ssh_ver;
	int	count;
	int	cache;
};

#define DATA_OPT(o,m,v) {o, offsetof(struct data, m), v}

struct fuse_opt opts[] = {
	FUSE_OPT_KEY("-p ",				1),
	FUSE_OPT_KEY("debug",				3),
	FUSE_OPT_KEY("noatime",				4),

	DATA_OPT("optstring=%s",	optstring,	0),
	DATA_OPT("-f=%s",		fsname, 	0),
	DATA_OPT("-x %s",		x,		0),
	DATA_OPT("--count=%u",		count, 		0),
	DATA_OPT("-1",			ssh_ver,	5),
	/*DATA_OPT("cache=yes",		cache,		1),*/
	DATA_OPT("cache=no",		cache,		0),

	FUSE_OPT_END
};

int
proc(void *data, const char *arg, int key, struct fuse_args *args)
{
	struct data *conf = (struct data *)data;

	if (conf == NULL)
		return (1);

	switch (key)
	{
	case 1:
		conf->port = atoi(&arg[2]);
		return (0);
	case 3:
		conf->debug = 1;
		return (1);
	case 4:
		conf->noatime = 1;
		return (1);
	}

	return (1);
}

#define TEST_DATA_INT(m, v) if (data.m != v) exit(__LINE__)
#define TEST_DATA_STR(m, v) if (data.m == NULL || strcmp(data.m, v) != 0) exit(__LINE__)

/*
 * A NULL 'args' is equivalent to an empty argument vector.
 */
void
test_null_args(void) {
	struct data data;
	struct fuse_args args;

	bzero(&data, sizeof(data));

	if (fuse_opt_parse(NULL, &data, opts, proc) != 0)
		exit(__LINE__);

	TEST_DATA_INT(port, 0);
	TEST_DATA_INT(fsname, 0);
	TEST_DATA_INT(x, 0);
	TEST_DATA_INT(optstring, 0);
	TEST_DATA_INT(debug, 0);
	TEST_DATA_INT(noatime, 0);
	TEST_DATA_INT(ssh_ver, 0);
	TEST_DATA_INT(count, 0);
}

/*
 * A NULL 'opts' is equivalent to an 'opts' array containing a single
 * end marker.
 */
void
test_null_opts(void)
{
	struct data data;
	struct fuse_args args;

	char *argv_null_opts[] = {
		"progname",
		"/mnt"
	};

	args.argc = sizeof(argv_null_opts) / sizeof(argv_null_opts[0]);
	args.argv = argv_null_opts;
	args.allocated = 0;

	bzero(&data, sizeof(data));

	if (fuse_opt_parse(&args, &data, NULL, proc) != 0)
		exit(__LINE__);

	TEST_DATA_INT(port, 0);
	TEST_DATA_INT(fsname, 0);
	TEST_DATA_INT(x, 0);
	TEST_DATA_INT(optstring, 0);
	TEST_DATA_INT(debug, 0);
	TEST_DATA_INT(noatime, 0);
	TEST_DATA_INT(ssh_ver, 0);
	TEST_DATA_INT(count, 0);

	if (args.argc != 2)
		exit(__LINE__);
	if (strcmp(args.argv[0], "progname") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[1], "/mnt") != 0)
		exit(__LINE__);
	if (args.allocated == 0)
		exit(__LINE__);

	fuse_opt_free_args(&args);
}

/*
 * A NULL 'proc' is equivalent to a processing function always returning '1'.
 */
void
test_null_proc(void)
{
	struct data data;
	struct fuse_args args;

        char *argv_null_proc[] = {
                "progname",
                "-odebug,noatime",
                "-d",
                "-p", "22",
                "/mnt",
                "-f=filename",
                "-1",
                "-x", "xanadu",
                "-o", "optstring=",
                "-o", "optstring=optstring",
                "--count=10"
        };

        args.argc = sizeof(argv_null_proc) / sizeof(argv_null_proc[0]);
        args.argv = argv_null_proc;
        args.allocated = 0;

        bzero(&data, sizeof(data));

        if (fuse_opt_parse(&args, &data, opts, NULL) != 0)
                exit(__LINE__);

        TEST_DATA_INT(port, 0);
        TEST_DATA_STR(fsname, "filename");
        TEST_DATA_STR(x, "xanadu");
        TEST_DATA_STR(optstring, "optstring");
        TEST_DATA_INT(debug, 0);
        TEST_DATA_INT(noatime, 0);
        TEST_DATA_INT(ssh_ver, 5);
        TEST_DATA_INT(count, 10);

        if (args.argc != 8)
                exit(__LINE__);
        if (strcmp(args.argv[0], "progname") != 0)
                exit(__LINE__);
        if (strcmp(args.argv[1], "-o") != 0)
                exit(__LINE__);
        if (strcmp(args.argv[2], "debug") != 0)
                exit(__LINE__);
        if (strcmp(args.argv[3], "-o") != 0)
                exit(__LINE__);
        if (strcmp(args.argv[4], "noatime") != 0)
                exit(__LINE__);
	if (strcmp(args.argv[5], "-d") != 0)
		exit(__LINE__);
        if (strcmp(args.argv[6], "-p22") != 0)
                exit(__LINE__);
	if (strcmp(args.argv[7], "/mnt") != 0)
		exit(__LINE__);
	if (args.allocated == 0)
		exit(__LINE__);

	fuse_opt_free_args(&args);
}

/*
 * Test with all args supplied to fuse_opt_parse.
 */
void
test_all_args(void)
{
	struct data data;
	struct fuse_args args;

	char *argv[] = {
		"progname",
		"-odebug,noatime",
		"-d",
		"-p", "22",
		"/mnt",
		"-f=filename",
		"-1",
		"-x", "xanadu",
		"-o", "optstring=optstring,cache=no",
		"--count=10"
	};

	args.argc = sizeof(argv) / sizeof(argv[0]);
	args.argv = argv;
	args.allocated = 0;

	bzero(&data, sizeof(data));

	if (fuse_opt_parse(&args, &data, opts, proc) != 0)
		exit(__LINE__);

	TEST_DATA_INT(port, 22);
	TEST_DATA_STR(fsname, "filename");
	TEST_DATA_STR(x, "xanadu");
	TEST_DATA_STR(optstring, "optstring");
	TEST_DATA_INT(debug, 1);
	TEST_DATA_INT(noatime, 1);
	TEST_DATA_INT(ssh_ver, 5);
	TEST_DATA_INT(count, 10);
	TEST_DATA_INT(cache, 0);

	if (args.argc != 7)
		exit(__LINE__);
	if (strcmp(args.argv[0], "progname") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[1], "-o") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[2], "debug") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[3], "-o") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[4], "noatime") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[5], "-d") != 0)
		exit(__LINE__);
	if (strcmp(args.argv[6], "/mnt") != 0)
		exit(__LINE__);
	if (args.allocated == 0)
		exit(__LINE__);

	fuse_opt_free_args(&args);
}

int
main(void)
{
	test_null_opts();
	test_null_args();
	test_null_proc();
	test_all_args();

	return (0);
}
