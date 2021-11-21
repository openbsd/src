/*	$OpenBSD: select_close.c,v 1.1 2021/11/21 06:21:01 visa Exp $	*/

/*
 * Copyright (c) 2021 Visa Hankala
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

/*
 * Test behaviour when a monitored file descriptor is closed by another thread.
 *
 * Note that this case is not defined by POSIX.
 */

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int	barrier[2];
static int	sock[2];

static int
wait_wchan(const char *wchanname)
{
	struct kinfo_proc kps[3]; /* process + 2 threads */
	struct timespec end, now;
	size_t size;
	unsigned int i;
	int mib[6], ret;

	clock_gettime(CLOCK_MONOTONIC, &now);
	end = now;
	end.tv_sec += 1;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID | KERN_PROC_SHOW_THREADS;
	mib[3] = getpid();
	mib[4] = sizeof(kps[0]);
	mib[5] = sizeof(kps) / sizeof(kps[0]);

	for (;;) {
		memset(kps, 0, sizeof(kps));
		size = sizeof(kps);
		ret = sysctl(mib, 6, kps, &size, NULL, 0);
		if (ret == -1)
			err(1, "sysctl");
		for (i = 0; i < size / sizeof(kps[0]); i++) {
			if (strncmp(kps[i].p_wmesg, wchanname,
			    sizeof(kps[i].p_wmesg)) == 0)
				return 0;
		}

		usleep(1000);
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &end, >=))
			break;
	}

	errx(1, "wchan %s timeout", wchanname);
}

static void *
thread_main(void *arg)
{
	fd_set rfds;
	struct timeval tv;
	int ret;
	char b;

	FD_ZERO(&rfds);
	FD_SET(sock[1], &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	ret = select(sock[1] + 1, &rfds, NULL, NULL, &tv);
	assert(ret == 1);
	assert(FD_ISSET(sock[1], &rfds));

	/* Drain data to prevent subsequent wakeups. */
	read(sock[1], &b, 1);

	/* Sync with parent thread. */
	write(barrier[1], "y", 1);
	read(barrier[1], &b, 1);

	FD_ZERO(&rfds);
	FD_SET(sock[1], &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 100000;
	ret = select(sock[1] + 1, &rfds, NULL, NULL, &tv);
	assert(ret == -1);
	assert(errno == EBADF);

	return NULL;
}

int
main(void)
{
	pthread_t t;
	int ret, saved_fd;
	char b;

	/* Enforce test timeout. */
	alarm(10);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, barrier) == -1)
		err(1, "can't create socket pair");

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock) == -1)
		err(1, "can't create socket pair");

	ret = pthread_create(&t, NULL, thread_main, NULL);
	if (ret != 0) {
		fprintf(stderr, "can't start thread: %s\n", strerror(ret));
		return 1;
	}

	/* Let the thread settle in select(). */
	wait_wchan("kqread");

	/* Awaken poll(). */
	write(sock[0], "x", 1);

	/* Wait until the thread has left select(). */
	read(barrier[0], &b, 1);

	/*
	 * Close and restore the fd that the thread has polled.
	 * This creates a pending badfd knote in the kernel.
	 */
	saved_fd = dup(sock[1]);
	close(sock[1]);
	dup2(saved_fd, sock[1]);
	close(saved_fd);

	/* Let the thread continue. */
	write(barrier[0], "x", 1);

	/* Let the thread settle in select(). */
	wait_wchan("kqread");

	/* Close the fd to awaken select(). */
	close(sock[1]);

	pthread_join(t, NULL);

	close(sock[0]);

	return 0;
}
