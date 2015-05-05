/*
 * Copyright (c) 2004, 2014 Todd C. Miller <Todd.Miller@courtesan.com>
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
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
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
void eoftest(char *, int, int);

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
	struct sigaction sa;
#if !defined(__OpenBSD__) && !defined(__FreeBSD__) && !defined(__NetBSD__) && \
    !defined(__linux__)
	__progname = argv[0];
#endif
	if (argc != 2)
		usage();

	/* Just warn EINTR from SIGALRM */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sigalrm;
	sigaction(SIGALRM, &sa, NULL);

	runtest(argv[1], 0, 0);
	runtest(argv[1], 0, INFTIM);
	runtest(argv[1], O_NONBLOCK, 0);
	runtest(argv[1], O_NONBLOCK, INFTIM);
	eoftest(argv[1], O_NONBLOCK, INFTIM);

	exit(0);
}

void
runtest(char *fifo, int flags, int timeout)
{
	ssize_t nread;
	int fd;
	char buf[BUFSIZ];

	(void)unlink(fifo);
	if (mkfifo(fifo, 0644) != 0) {
		printf("mkfifo %s: %s\n", fifo, strerror(errno));
		exit(1);
	}

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
	dopoll(fd, 0, "(none)", timeout);
	doselect(fd, fd, timeout);
	doselect(fd, -1, timeout);
	doselect(-1, fd, timeout);
	doselect(-1, -1, timeout);

	if (write(fd, "test", 4) != 4) {
		printf("write error: %s\n", strerror(errno));
		exit(1);
	}

	printf("\nTesting full FIFO:\n");
	dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT", timeout);
	dopoll(fd, POLLIN, "POLLIN", timeout);
	dopoll(fd, POLLOUT, "POLLOUT", timeout);
	dopoll(fd, 0, "(none)", timeout);
	doselect(fd, fd, timeout);
	doselect(fd, -1, timeout);
	doselect(-1, fd, timeout);
	doselect(-1, -1, timeout);

	if ((nread = read(fd, buf, sizeof(buf))) <= 0) {
		printf("read error: %s\n", (nread == 0) ? "EOF" : strerror(errno));
		exit(1);
	}
	buf[nread] = '\0';
	printf("\treceived '%s' from FIFO\n", buf);
}

int
eof_writer(const char *fifo, int flags)
{
	int fd;

	switch (fork()) {
	case -1:
		printf("fork: %s\n", strerror(errno));
		return -1;
	case 0:
		/* child */
		break;
	default:
		/* parent */
		return 0;
	}

	/* Wait for the reader to connect. */
	for (;;) {
		alarm(2);
		fd = open(fifo, O_WRONLY | flags, 0644);
		alarm(0);
		if (fd != -1)
			break;
		if (errno != ENOENT && errno != ENXIO) {
			printf("open %s O_WRONLY: %s\n", fifo, strerror(errno));
			return -1;
		}
		usleep(10000);
	}

	/*
	 * We need to give the reader time to call select() before
	 * we close the fd.  This is racey...
	 */
	usleep(100000);
	close(fd);
	_exit(0);
}

void
eoftest(char *fifo, int flags, int timeout)
{
	ssize_t nread;
	int fd = -1, rval, pass, status;
	char buf[BUFSIZ];

	/*
	 * Test all combinations of select and poll.
	 */
	for (pass = 0; pass < 16; pass++) {
		/*
		 * We run each test twice, once with a fresh fifo,
		 * and once with a reused one.
		 */
		if ((pass & 1) == 0) {
			if (fd != -1)
				close(fd);
			(void)unlink(fifo);
			if (mkfifo(fifo, 0644) != 0) {
				printf("mkfifo %s: %s\n", fifo, strerror(errno));
				exit(1);
			}

			/* XXX - also verify that we get alarm for O_RDWR */
			alarm(2);
			if ((fd = open(fifo, O_RDONLY | flags, 0644)) == -1) {
				printf("open %s: %s\n", fifo, strerror(errno));
				exit(1);
			}
			alarm(0);

			printf("\nOpened fifo for reading %s%s\n", fifo,
			    (flags & O_NONBLOCK) ? " (nonblocking)" : "");
		}

		printf("\nTesting EOF FIFO behavior (pass %d):\n", pass);

		/*
		 * The writer will sleep for a bit to give the reader time
		 * to call select() before anything has been written.
		 */
		rval = eof_writer(fifo, flags);
		if (rval == -1)
			exit(1);

		switch (pass) {
		case 0:
		case 1:
		    dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT", timeout);
		    break;
		case 2:
		case 3:
		    dopoll(fd, POLLIN, "POLLIN", timeout);
		    break;
		case 4:
		case 5:
		    dopoll(fd, POLLOUT, "POLLOUT", timeout);
		    break;
		case 6:
		case 7:
		    dopoll(fd, 0, "(none)", timeout);
		    break;
		case 8:
		case 9:
		    doselect(fd, fd, timeout);
		    break;
		case 10:
		case 11:
		    doselect(fd, -1, timeout);
		    break;
		case 12:
		case 13:
		    doselect(-1, fd, timeout);
		    break;
		case 14:
		case 15:
		    doselect(-1, -1, timeout);
		    break;
		}
		wait(&status);
		if ((nread = read(fd, buf, sizeof(buf))) < 0) {
			printf("read error: %s\n", strerror(errno));
			exit(1);
		}
		buf[nread] = '\0';
		printf("\treceived %s%s%s from FIFO\n", nread ? "'" : "",
		    nread ? buf : "EOF", nread ? "'" : "");
	}
	close(fd);
	(void)unlink(fifo);
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

	printf("\tselect%s%s, timeout=%d\n", rfds ? " read" : "",
	    wfds ? " write" : rfds ? "" : " (none)", timeout);

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
