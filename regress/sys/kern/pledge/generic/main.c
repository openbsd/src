/*	$OpenBSD: main.c,v 1.5 2015/10/18 13:01:40 semarie Exp $ */
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

#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "manager.h"

void test_request_tty(void);

static void
test_nop()
{
	/* nop */
}

static void
test_inet()
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int saved_errno = errno;
	close(fd);
	errno = saved_errno ? saved_errno : errno;
}

static void
test_kill()
{
	kill(0, SIGINT);
}

static void
test_allowed_syscalls()
{
	clock_getres(CLOCK_MONOTONIC, NULL);
	clock_gettime(CLOCK_MONOTONIC, NULL);
	/* fchdir(); */
	getdtablecount();
	getegid();
	geteuid();
	getgid();
	getgroups(0, NULL);
	getitimer(ITIMER_REAL, NULL);
	getlogin();
	getpgid(0);
	getpgrp();
	getpid();
	getppid();
	/* getresgid(); */
	/* getresuid(); */
	{ struct rlimit rl; getrlimit(RLIMIT_CORE, &rl); }
	getsid(0);
	getthrid();
	{ struct timeval tp; gettimeofday(&tp, NULL); }
	getuid();
	geteuid();
	issetugid();
	/* nanosleep(); */
	/* sigreturn(); */
	umask(0000);
	/* wait4(); */
}


static void
open_close(const char *filename)
{
	int fd;
	int saved_errno;

	errno = 0;
	printf("\n open_close(\"%s\")", filename);
	fd = open(filename, O_RDONLY);
	saved_errno = errno;
	printf(" fd=%d errno=%d", fd, errno);
	if (fd != -1)
		close(fd);
	errno = saved_errno;
}

static void
test_wpaths()
{
	/* absolute file */
	open_close("/etc/passwd");

	/* relative */
	open_close("generic");

	/* relative */
	open_close("../../../../../../../../../../../../../../../etc/passwd");

	/* ENOENT */
	open_close("/nonexistent");

	/* calling exit to flush stdout */
	printf("\n");
	exit(EXIT_SUCCESS);
}

static void
test_pledge()
{
	const char *wpaths[] = { "/sbin", NULL };

	if (pledge("stdio rpath", wpaths) != 0)
		_exit(errno);
}

static void
do_stat(const char *path)
{
	char resolved[PATH_MAX];
	struct stat sb;

	printf("\n stat(\"%s\"):", path);

	/* call realpath(3) */
	errno = 0;
	if (realpath(path, resolved) != NULL)
		printf(" realpath=\"%s\"", resolved);
	else
		printf(" realpath=failed(%d)", errno);

	/* call stat(2) */
	errno = 0;
	if (stat(path, &sb) == 0)
		printf(" uid=%d gid=%d mode=%04o", sb.st_uid, sb.st_gid,
		    sb.st_mode);
	else
		printf(" errno=%d", errno);
}

static void
test_stat()
{
	/* in whitelisted path */
	do_stat("/usr/share/man/man8/afterboot.8");
	do_stat("/usr/share/man/man8/");
	do_stat("/usr/share/man");

	/* parent of whitelisted path */
	do_stat("/usr/share");
	do_stat("/usr");
	do_stat("/");

	/* outside whitelisted path */
	do_stat("/usr/bin/gzip");

	/* calling exit to flush stdout */
	printf("\n");
	exit(EXIT_SUCCESS);
}

static void
test_mmap()
{
	int fd;
	void * data;

	if ((fd = open("/dev/zero", O_RDONLY, 0)) == -1)
		_exit(errno);

	data = mmap(NULL, 4096, PROT_READ|PROT_WRITE|PROT_EXEC,
	    MAP_FILE|MAP_SHARED, fd, 0);

	if (data == MAP_FAILED)
		_exit(errno);

	munmap(data, 4096);
	close(fd);
}

static void
test_rpath()
{
	int fd;
	char data[512];

	if ((fd = open("/dev/zero", O_RDONLY, 0)) == -1)
		_exit(errno);

	if (read(fd, data, sizeof(data)) == -1)
		_exit(errno);

	close(fd);
}

static void
test_wpath()
{
	int fd;
	char data[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };

	if ((fd = open("/dev/null", O_WRONLY, 0)) == -1)
		_exit(errno);

	if (write(fd, data, sizeof(data)) == -1)
		_exit(errno);

	close(fd);
}

static void
test_cpath()
{
	const char filename[] = "/tmp/generic-test-cpath";

	if (mkdir(filename, S_IRWXU) == -1)
		_exit(errno);

	if (rmdir(filename) == -1)
		_exit(errno);
}

int
main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;

	if (argc != 1)
		errx(1, "usage: %s", argv[0]);

	/*
	 * testsuite
	 */

	/* _exit is always allowed, and nothing else under flags=0 */
	start_test(&ret, "", NULL, test_nop);
	start_test(&ret, "", NULL, test_inet);

	/* test coredump */
	start_test(&ret, "abort", NULL, test_inet);

	/* inet under inet is ok */
	start_test(&ret, "inet", NULL, test_inet);

	/* kill under fattr is forbidden (don't have PLEDGE_SELF) */
	start_test(&ret, "fattr", NULL, test_kill);

	/* kill under proc is allowed */
	start_test(&ret, "proc", NULL, test_kill);

	/* tests PLEDGE_SELF for permitted syscalls */
	start_test(&ret, "malloc",  NULL, test_allowed_syscalls);
	start_test(&ret, "rw",      NULL, test_allowed_syscalls);
	start_test(&ret, "stdio",   NULL, test_allowed_syscalls);
	start_test(&ret, "rpath",   NULL, test_allowed_syscalls);
	start_test(&ret, "wpath",   NULL, test_allowed_syscalls);
	start_test(&ret, "tmppath", NULL, test_allowed_syscalls);
	start_test(&ret, "inet",    NULL, test_allowed_syscalls);
	start_test(&ret, "unix",    NULL, test_allowed_syscalls);
	start_test(&ret, "dns",     NULL, test_allowed_syscalls);
	start_test(&ret, "getpw",   NULL, test_allowed_syscalls);

	/* tests req without PLEDGE_SELF for "permitted syscalls" */
	// XXX it is a documentation bug
	start_test(&ret, "ioctl", NULL, test_allowed_syscalls);
	start_test(&ret, "proc",  NULL, test_allowed_syscalls);
	start_test(&ret, "cpath", NULL, test_allowed_syscalls);
	start_test(&ret, "abort", NULL, test_allowed_syscalls);
	start_test(&ret, "fattr", NULL, test_allowed_syscalls);

	start_test(&ret, "rpath", NULL, test_rpath);
	start_test(&ret, "wpath", NULL, test_wpath);
	start_test(&ret, "rpath cpath", NULL, test_cpath);

	/*
	 * test whitelist path
	 */
	start_test(&ret, "stdio rpath", NULL, test_wpaths);
	start_test1(&ret, "stdio rpath", NULL, test_wpaths);
	start_test1(&ret, "stdio rpath", "/", test_wpaths);
	start_test1(&ret, "stdio rpath", "/etc", test_wpaths);
	start_test1(&ret, "stdio rpath", "/etc/", test_wpaths);
	start_test1(&ret, "stdio rpath", "/etc/passwd", test_wpaths);
	// XXX start_test1(&ret, "stdio rpath", "/etc/passwd/", test_wpaths);
	start_test1(&ret, "stdio rpath", "/bin", test_wpaths);
	start_test1(&ret, "stdio rpath", "generic", test_wpaths);
	start_test1(&ret, "stdio rpath", "", test_wpaths);
	start_test1(&ret, "stdio rpath", ".", test_wpaths);

	/*
	 * test pledge(2) arguments
	 */
	/* same request */
	start_test(&ret, "stdio rpath", NULL, test_pledge);
	/* same request (stdio = malloc rw) */
	start_test(&ret, "malloc rw rpath", NULL, test_pledge);
	/* reduce request */
	start_test(&ret, "stdio rpath wpath", NULL, test_pledge);
	/* reduce request (with same/other wpaths) */
	start_test1(&ret, "stdio rpath wpath", "/sbin", test_pledge);
	start_test1(&ret, "stdio rpath wpath", "/", test_pledge);
	/* add request */
	start_test(&ret, "stdio", NULL, test_pledge);
	/* change request */
	start_test(&ret, "unix", NULL, test_pledge);

	/* test stat(2) */
	start_test1(&ret, "stdio rpath", "/usr/share/man", test_stat);

	/* mmap */
	start_test1(&ret, "rpath malloc prot_exec", "/dev/zero", test_mmap);
	start_test1(&ret, "rpath malloc", "/dev/zero", test_mmap);

	/* tty */
	start_test(&ret, NULL, NULL, test_request_tty);

	return (ret);
}
