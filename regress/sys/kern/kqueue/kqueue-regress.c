/*	$OpenBSD: kqueue-regress.c,v 1.1 2018/06/17 08:30:20 anton Exp $	*/
/*
 *	Written by Anton Lindqvist <anton@openbsd.org> 2018 Public Domain
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <err.h>
#include <signal.h>
#include <stdlib.h>

#include "main.h"

int
do_regress(void)
{
	struct kevent kev[2];
	int kq;

	ASS((kq = kqueue()) >= 0,
	    warn("kqueue"));

	EV_SET(&kev[0], kq, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	ASS(kevent(kq, kev, 2, NULL, 0, NULL) == 0,
	    warn("can't register events on kqueue"));

	/* kq intentionally left open */

	return 0;
}
