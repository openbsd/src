/*	$OpenBSD: main.c,v 1.1 2015/08/24 09:21:10 semarie Exp $ */
/*
 * Copyright (c) 2015 Sebastien Marie <semarie@openbsd.org>
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

#include <sys/tame.h>

#include <err.h>
#include <stdlib.h>

#include "actions.h"

void start_test(int *ret, int ntest, int flags, const char *paths[], ...);

#define start_test1(ret,ntest,flags,path,...) \
    do { \
	    const char *_paths[] = {path, NULL}; \
	    start_test(ret,ntest,flags,_paths,__VA_ARGS__); \
    } while (0)


int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;

	if (argc != 1)
		errx(1, "usage: %s", argv[0]);

	/* check for env */
	if (getenv("LD_BIND_NOW") == NULL)
		errx(1, "should use LD_BIND_NOW=1 in env");

	/*
	 * testsuite
	 */

	/* _exit is always allowed, and nothing else under flags=0 */
	start_test(&ret, 1, 0, NULL, AC_EXIT);
	start_test(&ret, 2, 0, NULL, AC_INET, AC_EXIT);

	/* test coredump */
	start_test(&ret, 3, TAME_ABORT, NULL, AC_INET, AC_EXIT);

	/* inet under inet is ok */
	start_test(&ret, 4, TAME_INET, NULL, AC_INET, AC_EXIT);

	/* kill under inet is forbidden */
	start_test(&ret, 5, TAME_INET, NULL, AC_KILL, AC_EXIT);

	/* kill under proc is allowed */
	start_test(&ret, 6, TAME_PROC, NULL, AC_KILL, AC_EXIT);

	/* tests several permitted syscalls */
	start_test(&ret, 7, TAME_DNS,  NULL, AC_ALLOWED_SYSCALLS, AC_EXIT);
	start_test(&ret, 8, TAME_INET, NULL, AC_ALLOWED_SYSCALLS, AC_EXIT);

	/* these TAME_* don't have "permitted syscalls" */
	// XXX it is a documentation bug
	start_test(&ret, 9, TAME_PROC, NULL, AC_ALLOWED_SYSCALLS, AC_EXIT);

	/*
	 * test absolute whitelist path
	 */
	/* without wpaths */
	start_test(&ret, 10, TAME_RPATH, NULL,
	    AC_OPENFILE_RDONLY, "/etc/passwd",
	    AC_EXIT);
	/* exact match */
	start_test1(&ret, 11, TAME_RPATH, "/etc/passwd",
	    AC_OPENFILE_RDONLY, "/etc/passwd",
	    AC_EXIT);
	/* subdir match */
	start_test1(&ret, 12, TAME_RPATH, "/etc/",
	    AC_OPENFILE_RDONLY, "/etc/passwd",
	    AC_EXIT);
	/* same without trailing '/' */
	start_test1(&ret, 13, TAME_RPATH, "/etc",
	    AC_OPENFILE_RDONLY, "/etc/passwd",
	    AC_EXIT);
	/* failing one */
	start_test1(&ret, 14, TAME_RPATH, "/bin",
	    AC_OPENFILE_RDONLY, "/etc/passwd",
	    AC_EXIT);

	/*
	 * test relative whitelist path
	 */
	/* without wpaths */
	start_test(&ret, 15, TAME_RPATH, NULL,
	    AC_OPENFILE_RDONLY, "generic",
	    AC_EXIT);
	/* exact match */
	start_test1(&ret, 16, TAME_RPATH, "generic",
	    AC_OPENFILE_RDONLY, "generic",
	    AC_EXIT);
	/* subdir match */
	start_test1(&ret, 17, TAME_RPATH, "./",
	    AC_OPENFILE_RDONLY, "generic",
	    AC_EXIT);
	/* same without trailing '/' */
	start_test1(&ret, 18, TAME_RPATH, ".",
	    AC_OPENFILE_RDONLY, "generic",
	    AC_EXIT);
	/* failing one */
	start_test1(&ret, 19, TAME_RPATH, ".",
	    AC_OPENFILE_RDONLY, "../../../../../../../../../../../../../../../etc/passwd",
	    AC_EXIT);

	/* tame: test reducing flags */
	start_test1(&ret, 20, TAME_RPATH | TAME_WPATH, NULL,
	    AC_TAME, TAME_RPATH,
	    AC_EXIT);

	/* tame: test adding flags */
	start_test1(&ret, 21, TAME_RPATH, NULL,
	    AC_TAME, TAME_RPATH | TAME_WPATH,
	    AC_EXIT);

	/* tame: test replacing flags */
	start_test1(&ret, 22, TAME_RPATH, NULL,
	    AC_TAME, TAME_WPATH,
	    AC_EXIT);

	return (ret);
}
