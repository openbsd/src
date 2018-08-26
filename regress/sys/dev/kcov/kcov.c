/*	$OpenBSD: kcov.c,v 1.1 2018/08/26 08:12:09 anton Exp $	*/

/*
 * Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
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

#include <sys/ioctl.h>
#include <sys/kcov.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int test_close(int);
static int test_coverage(int);
static int test_exec(int);
static int test_fork(int);
static int test_mode(int);
static int test_open(int);

static void do_syscall(void);
static void dump(const unsigned long *);
static void kcov_disable(int);
static void kcov_enable(int);
static int kcov_open(void);
static __dead void usage(void);

static const char *self;
static unsigned long bufsize = 256 << 10;

int
main(int argc, char *argv[])
{
	struct {
		const char *name;
		int (*fn)(int);
		int coverage;		/* test must produce coverage */
	} tests[] = {
		{ "coverage",	test_coverage,	1 },
		{ "fork",	test_fork,	1 },
		{ "exec",	test_exec,	1 },
		{ "mode",	test_mode,	1 },
		{ "open",	test_open,	0 },
		{ "close",	test_close,	0 },
		{ NULL,		NULL,		0 },
	};
	unsigned long *cover;
	int c, fd, i;
	int nfail = 0;
	int prereq = 0;
	int reexec = 0;
	int verbose = 0;

	self = argv[0];

	while ((c = getopt(argc, argv, "Epv")) != -1)
		switch (c) {
		case 'E':
			reexec = 1;
			break;
		case 'p':
			prereq = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (argc > 0)
		usage();

	if (prereq) {
		fd = kcov_open();
		close(fd);
		return 0;
	}

	if (reexec) {
		do_syscall();
		return 0;
	}

	fd = kcov_open();
	if (ioctl(fd, KIOSETBUFSIZE, &bufsize) == -1)
		err(1, "ioctl: KIOSETBUFSIZE");
	cover = mmap(NULL, bufsize * sizeof(unsigned long),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cover == MAP_FAILED)
		err(1, "mmap");

	for (i = 0; tests[i].name != NULL; i++) {
		printf("===> %s\n", tests[i].name);
		*cover = 0;
		nfail += tests[i].fn(fd);
		if (verbose)
			dump(cover);
		if (tests[i].coverage && *cover == 0) {
			warnx("coverage empty (count=%lu, fd=%d)\n",
			    *cover, fd);
			nfail++;
		} else if (!tests[i].coverage && *cover != 0) {
			warnx("coverage is not empty (count=%lu, fd=%d)\n",
			    *cover, fd);
			nfail++;
		}
	}

	if (munmap(cover, bufsize * sizeof(unsigned long)) == -1)
		err(1, "munmap");
	close(fd);

	if (nfail > 0)
		return 1;
	return 0;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: kcov [-Epv]\n");
	exit(1);
}

static void
do_syscall(void)
{
	getpid();
}

static void
dump(const unsigned long *cover)
{
	unsigned long i;

	for (i = 0; i < cover[0]; i++)
		printf("%lu/%lu: %p\n", i + 1, cover[0], (void *)cover[i + 1]);
}

static int
kcov_open(void)
{
	int fd;

	fd = open("/dev/kcov", O_RDWR);
	if (fd == -1)
		err(1, "open: /dev/kcov");
	return fd;
}

static void
kcov_enable(int fd)
{
	if (ioctl(fd, KIOENABLE) == -1)
		err(1, "ioctl: KIOENABLE");
}

static void
kcov_disable(int fd)
{
	if (ioctl(fd, KIODISABLE) == -1)
		err(1, "ioctl: KIODISABLE");
}

/*
 * Close before mmap.
 */
static int
test_close(int oldfd)
{
	int fd;

	fd = kcov_open();
	close(fd);
	return 0;
}

/*
 * Coverage of current thread.
 */
static int
test_coverage(int fd)
{
	kcov_enable(fd);
	do_syscall();
	kcov_disable(fd);
	return 0;
}

/*
 * Coverage of thread after exec.
 */
static int
test_exec(int fd)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		kcov_enable(fd);
		execlp(self, self, "-E", NULL);
		_exit(1);
	}

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	if (WIFSIGNALED(status)) {
		warnx("terminated by signal (%d)", WTERMSIG(status));
		return 1;
	} else if (WEXITSTATUS(status) != 0) {
		warnx("non-zero exit (%d)", WEXITSTATUS(status));
		return 1;
	}

	/* Upon exit, the kcov descriptor must be reusable again. */
	kcov_enable(fd);
	kcov_disable(fd);

	return 0;
}

/*
 * Coverage of thread after fork.
 */
static int
test_fork(int fd)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		kcov_enable(fd);
		do_syscall();
		_exit(0);
	}

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	if (WIFSIGNALED(status)) {
		warnx("terminated by signal (%d)", WTERMSIG(status));
		return 1;
	} else if (WEXITSTATUS(status) != 0) {
		warnx("non-zero exit (%d)", WEXITSTATUS(status));
		return 1;
	}

	/* Upon exit, the kcov descriptor must be reusable again. */
	kcov_enable(fd);
	kcov_disable(fd);

	return 0;
}

/*
 * Mode transitions.
 */
static int
test_mode(int fd)
{
	if (ioctl(fd, KIOENABLE) == -1) {
		warnx("KIOSETBUFSIZE -> KIOENABLE");
		return 1;
	}
	if (ioctl(fd, KIODISABLE) == -1) {
		warnx("KIOENABLE -> KIODISABLE");
		return 1;
	}
	if (ioctl(fd, KIOSETBUFSIZE, 0) != -1) {
		warnx("KIOSETBUFSIZE -> KIOSETBUFSIZE");
		return 1;
	}
	if (ioctl(fd, KIODISABLE) != -1) {
		warnx("KIOSETBUFSIZE -> KIODISABLE");
		return 1;
	}

	kcov_enable(fd);
	if (ioctl(fd, KIOENABLE) != -1) {
		warnx("KIOENABLE -> KIOENABLE");
		return 1;
	}
	if (ioctl(fd, KIOSETBUFSIZE, 0) != -1) {
		warnx("KIOENABLE -> KIOSETBUFSIZE");
		return 1;
	}
	kcov_disable(fd);

	return 0;
}

/*
 * Open /dev/kcov more than once.
 */
static int
test_open(int oldfd)
{
	unsigned long *cover;
	int fd;
	int ret = 0;

	fd = kcov_open();
	if (ioctl(fd, KIOSETBUFSIZE, &bufsize) == -1)
		err(1, "ioctl: KIOSETBUFSIZE");
	cover = mmap(NULL, bufsize * sizeof(unsigned long),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cover == MAP_FAILED)
		err(1, "mmap");

	kcov_enable(fd);
	do_syscall();
	kcov_disable(fd);

	if (*cover == 0) {
		warnx("coverage empty (count=0, fd=%d)\n", fd);
		ret = 1;
	}
	if (munmap(cover, bufsize * sizeof(unsigned long)))
		err(1, "munmap");
	close(fd);
	return ret;
}
