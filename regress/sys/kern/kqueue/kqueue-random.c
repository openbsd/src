/*	$OpenBSD: kqueue-random.c,v 1.9 2015/08/13 10:12:04 uebayasi Exp $	*/
/*	Written by Michael Shalayeff, 2002, Public Domain	*/

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <string.h>

#include <sys/param.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <sys/fcntl.h>

#include <dev/rndvar.h>

int do_random(void);

int
do_random(void)
{
	int n, fd, kq;
	struct timespec ts;
	struct kevent ev;
	u_int32_t buf[BUFSIZ];

	if ((fd = open("/dev/random", O_RDONLY)) < 0) {
		warn("open: /dev/random");
		return (1);
	}
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		warn("fcntl");
		close(fd);
		return (1);
	}

	if ((kq = kqueue()) < 0) {
		warn("kqueue");
		close(fd);
		return (1);
	}

	memset(&ev, 0, sizeof(ev));
	ev.ident = fd;
	ev.filter = EVFILT_READ;
	ev.flags = EV_ADD | EV_ENABLE;
	n = kevent(kq, &ev, 1, NULL, 0, NULL);
	if (n == -1) {
		warn("kevent");
		close(kq);
		close(fd);
		return (1);
	}

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	if (kevent(kq, NULL, 0, &ev, 1, &ts) < 0) {
		warn("kevent2");
		return (1);
	}

	n = MIN((ev.data + 7) / 8, sizeof(buf));
	if (read(fd, buf, n) < 1) {
		warnx("read %d", n);
		return (1);
	}

	close(kq);
	close(fd);

	return (0);
}
