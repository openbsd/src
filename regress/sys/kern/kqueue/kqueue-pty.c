/*	$OpenBSD: kqueue-pty.c,v 1.6 2015/08/13 10:11:38 uebayasi Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public Domain	*/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/event.h>
#include <stdio.h>
#include <unistd.h>
#include <util.h>
#include <termios.h>
#include <fcntl.h>
#include <err.h>
#include <string.h>

static int
pty_check(int kq, struct kevent *ev, int n, int rm, int rs, int wm, int ws)
{
	struct timespec ts;
	int i;

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	if ((n = kevent(kq, NULL, 0, ev, n, &ts)) < 0)
		err(1, "slave: kevent");

	if (n == 0)
		return (1);

	for (i = 0; i < n; i++, ev++) {
		if (ev->filter == EVFILT_READ) {
			if (rm < 0 && ev->ident == -rm)
				return (1);
			if (rs < 0 && ev->ident == -rs)
				return (1);
		} else if (ev->filter == EVFILT_WRITE) {
			if (wm < 0 && ev->ident == -wm)
				return (1);
			if (ws < 0 && ev->ident == -ws)
				return (1);
		} else
			errx(1, "unknown event");
	}

	return (0);
}

int do_pty(void);

int
do_pty(void)
{
	struct kevent ev[4];
	struct termios tt;
	int kq, massa, slave;
	char buf[1024];

	tcgetattr(STDIN_FILENO, &tt);
	cfmakeraw(&tt);
	tt.c_lflag &= ~ECHO;
	if (openpty(&massa, &slave, NULL, &tt, NULL) < 0)
		err(1, "openpty");
	if (fcntl(massa, F_SETFL, O_NONBLOCK) < 0)
		err(1, "massa: fcntl");
	if (fcntl(slave, F_SETFL, O_NONBLOCK) < 0)
		err(1, "massa: fcntl");
	if ((kq = kqueue()) == -1)
		err(1, "kqueue");

	/* test the read from the slave works */
	EV_SET(&ev[0], massa, EVFILT_READ,  EV_ADD|EV_ENABLE, 0, 0, NULL);
	EV_SET(&ev[1], massa, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, NULL);
	EV_SET(&ev[2], slave, EVFILT_READ,  EV_ADD|EV_ENABLE, 0, 0, NULL);
	EV_SET(&ev[3], slave, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, NULL);
	if (kevent(kq, ev, 4, NULL, 0, NULL) < 0)
		err(1, "slave: kevent add");

	memset(buf, 0, sizeof buf);

	if (write(massa, " ", 1) != 1)
		err(1, "massa: write");

	if (pty_check(kq, ev, 4, -massa, slave, massa, slave))
		return (1);

	read(slave, buf, sizeof(buf));

	if (pty_check(kq, ev, 4, -massa, -slave, massa, slave))
		return (1);

	while (write(massa, buf, sizeof(buf)) > 0)
		continue;

	if (pty_check(kq, ev, 4, -massa, slave, -massa, slave))
		return (1);

	read(slave, buf, 1);

	if (pty_check(kq, ev, 4, -massa, slave, massa, slave))
		return (1);

	while (read(slave, buf, sizeof(buf)) > 0)
		continue;

	if (pty_check(kq, ev, 4, -massa, -slave, massa, slave))
		return (1);

	return (0);
}
