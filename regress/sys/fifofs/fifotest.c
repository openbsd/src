/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef INFTIM
#define	INFTIM	-1
#endif

void usage(void);
void sigalrm(int);
void dopoll(int, int, char *, int);
void doselect(int, int, int);
void runtest(char *, int, int);

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__linux__)
extern char *__progname;
#else
char *__progname;
#endif

/*
 * Test FIFOs and poll(2) both with an emtpy and full FIFO.
 */
int
main(int argc, char **argv)
{
#if !defined(__OpenBSD__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && \
    !defined(__linux__)
	__progname = argv[0];
#endif
	if (argc != 2)
		usage();

	runtest(argv[1], 0, 0);
	runtest(argv[1], 0, INFTIM);
	runtest(argv[1], O_NONBLOCK, 0);
	runtest(argv[1], O_NONBLOCK, INFTIM);

	exit(0);
}

void
runtest(char *fifo, int flags, int timeout)
{
	struct sigaction sa;
	ssize_t nread;
	int fd;
	char buf[BUFSIZ];

	(void)unlink(fifo);
	if (mkfifo(fifo, 0644) != 0) {
		printf("mkfifo %s: %s\n", fifo, strerror(errno));
		exit(1);
	}

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigalrm;
	sigaction(SIGALRM, &sa, NULL);

	alarm(2);
	if ((fd = open(fifo, O_RDWR | flags, 0644)) == -1) {
		printf("open %s: %s\n", fifo, strerror(errno));
		exit(1);
	}
	alarm(0);
	(void)unlink(fifo);
	printf("\nOpened fifo %s%s\n", fifo,
	    (flags & O_NONBLOCK) ? " (nonblocking)" : "");

	printf("\nTesting empty FIFO:\n");
	dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT", timeout);
	dopoll(fd, POLLIN, "POLLIN", timeout);
	dopoll(fd, POLLOUT, "POLLOUT", timeout);
	doselect(fd, fd, timeout);
	doselect(fd, -1, timeout);
	doselect(-1, fd, timeout);

	if (write(fd, "test", 4) != 4) {
		printf("write error: %s\n", strerror(errno));
		exit(1);
	}

	printf("\nTesting full FIFO:\n");
	dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT", timeout);
	dopoll(fd, POLLIN, "POLLIN", timeout);
	dopoll(fd, POLLOUT, "POLLOUT", timeout);
	doselect(fd, fd, timeout);
	doselect(fd, -1, timeout);
	doselect(-1, fd, timeout);

	if ((nread = read(fd, buf, sizeof(buf))) <= 0) {
		printf("read error: %s\n", (nread == 0) ? "EOF" : strerror(errno));
		exit(1);
	}
	buf[nread] = '\0';
	printf("\treceived '%s' from FIFO\n", buf);
}

void
dopoll(int fd, int events, char *str, int timeout)
{
	struct pollfd pfd;
	int nready;

	pfd.fd = fd;
	pfd.events = events;

	printf("\tpoll %s, timeout=%d\n", str, timeout);
	pfd.events = events;
	alarm(2);
	nready = poll(&pfd, 1, timeout);
	alarm(0);
	if (nready < 0) {
		printf("poll: %s\n", strerror(errno));
		return;
	}
	printf("\t\t%d fd(s) ready%s", nready, nready ? ", revents ==" : "");
	if (pfd.revents & POLLIN)
		printf(" POLLIN");
	if (pfd.revents & POLLOUT)
		printf(" POLLOUT");
	if (pfd.revents & POLLERR)
		printf(" POLLERR");
	if (pfd.revents & POLLHUP)
		printf(" POLLHUP");
	if (pfd.revents & POLLNVAL)
		printf(" POLLNVAL");
	printf("\n");
}

void
doselect(int rfd, int wfd, int timeout)
{
	struct timeval tv, *tvp;
	fd_set *rfds = NULL, *wfds = NULL;
	int nready, maxfd;

	if (timeout == INFTIM)
		tvp = NULL;
	else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		tvp = &tv;
	}
	tv.tv_sec = tv.tv_usec = 0;
	maxfd = rfd > wfd ? rfd : wfd;
	if (rfd != -1) {
		rfds = calloc(howmany(maxfd + 1, NFDBITS), sizeof(fd_mask));
		if (rfds == NULL) {
			printf("unable to allocate memory\n");
			exit(1);
		}
		FD_SET(rfd, rfds);
	}
	if (wfd != -1) {
		wfds = calloc(howmany(maxfd + 1, NFDBITS), sizeof(fd_mask));
		if (wfds == NULL) {
			printf("unable to allocate memory\n");
			exit(1);
		}
		FD_SET(wfd, wfds);
	}

	printf("\tselect%s%s\n", rfds ? " read" : "",
	    wfds ? " write" : "");

	alarm(2);
	nready = select(maxfd + 1, rfds, wfds, NULL, &tv);
	alarm(0);
	if (nready < 0) {
		printf("select: %s\n", strerror(errno));
		goto cleanup;
	}
	printf("\t\t%d fd(s) ready", nready);
	if (rfds != NULL && FD_ISSET(rfd, rfds))
		printf(", readable");
	if (wfds != NULL && FD_ISSET(wfd, wfds))
		printf(", writeable");
	printf("\n");
cleanup:
	free(rfds);
	free(wfds);
}

void
sigalrm(int dummy)
{
	/* Just cause EINTR */
	return;
}

void
usage(void)
{
	fprintf(stderr, "usage: %s fifoname\n", __progname);
	exit(1);
}
