/*	$OpenBSD: sigio_common.c,v 1.1 2018/11/12 16:50:28 visa Exp $	*/

/*
 * Copyright (c) 2018 Visa Hankala
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
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include "common.h"

static char buf[1024];

int
test_common_badpgid(int fd)
{
	/* ID of non-existent process */
	assert(fcntl(fd, F_SETOWN, 1000000) == -1);
	assert(errno == ESRCH);

	/* ID of non-existent process group */
	assert(fcntl(fd, F_SETOWN, -1000000) == -1);
	assert(errno == ESRCH);

	return 0;
}

int
test_common_badsession(int fd)
{
	int sfd;
	pid_t pid, ppid;

	/* Ensure this process has its own process group. */
	assert(setpgid(0, 0) == 0);

	ppid = getpid();
	if (test_fork(&pid, &sfd) == PARENT) {
		test_barrier(sfd);
	} else {
		assert(setsid() != -1);
		assert(fcntl(fd, F_SETOWN, ppid) == -1);
		assert(errno == EPERM);
		assert(fcntl(fd, F_SETOWN, -ppid) == -1);
		assert(errno == EPERM);
		test_barrier(sfd);
	}
	return test_wait(pid, sfd);
}

/*
 * Test that signal is not delivered if there is a privilege mismatch.
 */
int
test_common_cansigio(int *fds)
{
	struct passwd *pw;
	int flags, sfd;
	pid_t pid, ppid;

	assert((pw = getpwnam(SIGIO_REGRESS_USER)) != NULL);
	assert(pw->pw_uid != getuid());

	flags = fcntl(fds[0], F_GETFL);
	assert(fcntl(fds[0], F_SETFL, flags | O_ASYNC) == 0);

	ppid = getpid();
	if (test_fork(&pid, &sfd) == PARENT) {
		/* Privilege mismatch prevents signal sending. */
		reject_signal(SIGIO);
		test_barrier(sfd);
		test_barrier(sfd);
		reject_signal(SIGIO);
		assert(read(fds[0], buf, 1) == 1);

		test_barrier(sfd);
		assert(setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == 0);

		/* Privileges allow signal sending. */
		reject_signal(SIGIO);
		test_barrier(sfd);
		test_barrier(sfd);
		expect_signal(SIGIO);
		assert(read(fds[0], buf, 1) == 1);
	} else {
		assert(setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == 0);
		assert(fcntl(fds[0], F_SETOWN, ppid) == 0);

		test_barrier(sfd);
		assert(write(fds[1], buf, 1) == 1);
		test_barrier(sfd);

		test_barrier(sfd);

		test_barrier(sfd);
		assert(write(fds[1], buf, 1) == 1);
		test_barrier(sfd);
	}
	return test_wait(pid, sfd);
}

/*
 * Test that SIGIO gets triggered when data becomes available for reading.
 */
int
test_common_read(int *fds)
{
	int flags, sfd;
	pid_t pid;

	flags = fcntl(fds[0], F_GETFL);
	assert(fcntl(fds[0], F_SETFL, flags | O_ASYNC) == 0);

	assert(fcntl(fds[0], F_SETOWN, getpid()) == 0);

	if (test_fork(&pid, &sfd) == PARENT) {
		reject_signal(SIGIO);
		test_barrier(sfd);
		test_barrier(sfd);
		expect_signal(SIGIO);
		assert(read(fds[0], buf, 1) == 1);
	} else {
		test_barrier(sfd);
		assert(write(fds[1], buf, 1) == 1);
		test_barrier(sfd);
	}
	return test_wait(pid, sfd);
}

/*
 * Test that SIGIO gets triggered when buffer space becomes available
 * for writing.
 */
int
test_common_write(int *fds)
{
	ssize_t n;
	int flags, sfd;
	pid_t pid;

	flags = fcntl(fds[0], F_GETFL);
	assert(fcntl(fds[0], F_SETFL, flags | O_ASYNC | O_NONBLOCK) == 0);
	flags = fcntl(fds[1], F_GETFL);
	assert(fcntl(fds[1], F_SETFL, flags | O_NONBLOCK) == 0);

	assert(fcntl(fds[0], F_SETOWN, getpid()) == 0);

	if (test_fork(&pid, &sfd) == PARENT) {
		while ((n = write(fds[0], buf, sizeof(buf))) > 0)
			continue;
		assert(n == -1);
		assert(errno == EWOULDBLOCK);
		reject_signal(SIGIO);

		test_barrier(sfd);
		test_barrier(sfd);
		expect_signal(SIGIO);
		assert(write(fds[0], buf, 1) == 1);
	} else {
		test_barrier(sfd);
		while ((n = read(fds[1], buf, sizeof(buf))) > 0)
			continue;
		assert(n == -1);
		assert(errno == EWOULDBLOCK);
		test_barrier(sfd);
	}
	return test_wait(pid, sfd);
}
