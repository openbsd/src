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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void usage(void);
void sigalrm(int);
void dopoll(int, int, char *);

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__linux__)
extern char *__progname;
#else
char *__progname;
#endif

/*
 * Test FIFOs and poll(2) both with an emtpy and full FIFO.
 */
int main(int argc, char **argv)
{
	struct sigaction sa;
	ssize_t nread;
	int fd;
	char *fifo, buf[BUFSIZ];

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__linux__)
	__progname = argv[0];
#endif
	if (argc != 2)
		usage();

	fifo = argv[1];
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
	if ((fd = open(fifo, O_RDWR, 0644)) == -1) {
		printf("open %s: %s\n", fifo, strerror(errno));
		exit(1);
	}
	alarm(0);
	(void)unlink(fifo);
	printf("sucessfully opened fifo\n");

	printf("\nTesting empty FIFO:\n");
	dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT");
	dopoll(fd, POLLIN, "POLLIN");
	dopoll(fd, POLLOUT, "POLLOUT");

	if (write(fd, "test", 4) != 4) {
		printf("write error: %s\n", strerror(errno));
		exit(1);
	}

	printf("\nTesting full FIFO:\n");
	dopoll(fd, POLLIN|POLLOUT, "POLLIN|POLLOUT");
	dopoll(fd, POLLIN, "POLLIN");
	dopoll(fd, POLLOUT, "POLLOUT");

	if ((nread = read(fd, buf, sizeof(buf))) <= 0) {
		printf("read error: %s\n", (nread == 0) ? "EOF" : strerror(errno));
		exit(1);
	}
	buf[nread] = '\0';
	printf("\treceived '%s' from FIFO\n", buf);

	exit(0);
}

void
dopoll(int fd, int events, char *str)
{
	struct pollfd pfd;
	int nready;

	pfd.fd = fd;
	pfd.events = events;

	printf("\ttesting %s\n", str);
	pfd.events = events;
	alarm(2);
	nready = poll(&pfd, 1, 0);
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
