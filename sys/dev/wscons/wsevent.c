/* $OpenBSD: wsevent.c,v 1.12 2015/02/10 21:56:09 miod Exp $ */
/* $NetBSD: wsevent.c,v 1.16 2003/08/07 16:31:29 agc Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)event.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Internal "wscons_event" queue interface for the keyboard and mouse drivers.
 */

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/selinfo.h>
#include <sys/poll.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wseventvar.h>

/*
 * Initialize a wscons_event queue.
 */
void
wsevent_init(struct wseventvar *ev)
{

	if (ev->q != NULL) {
#ifdef DIAGNOSTIC
		printf("wsevent_init: already initialized\n");
#endif
		return;
	}
	ev->get = ev->put = 0;
	ev->q = malloc(WSEVENT_QSIZE * sizeof(struct wscons_event),
	    M_DEVBUF, M_WAITOK | M_ZERO);
}

/*
 * Tear down a wscons_event queue.
 */
void
wsevent_fini(struct wseventvar *ev)
{
	if (ev->q == NULL) {
#ifdef DIAGNOSTIC
		printf("wsevent_fini: already invoked\n");
#endif
		return;
	}
	free(ev->q, M_DEVBUF, 0);
	ev->q = NULL;
}

/*
 * User-level interface: read, poll.
 * (User cannot write an event queue.)
 */
int
wsevent_read(struct wseventvar *ev, struct uio *uio, int flags)
{
	int s, n, cnt, error;

	/*
	 * Make sure we can return at least 1.
	 */
	if (uio->uio_resid < sizeof(struct wscons_event))
		return (EMSGSIZE);	/* ??? */
	s = splwsevent();
	while (ev->get == ev->put) {
		if (flags & IO_NDELAY) {
			splx(s);
			return (EWOULDBLOCK);
		}
		ev->wanted = 1;
		error = tsleep(ev, PWSEVENT | PCATCH,
		    "wsevent_read", 0);
		if (error) {
			splx(s);
			return (error);
		}
	}
	/*
	 * Move wscons_event from tail end of queue (there is at least one
	 * there).
	 */
	if (ev->put < ev->get)
		cnt = WSEVENT_QSIZE - ev->get;	/* events in [get..QSIZE) */
	else
		cnt = ev->put - ev->get;	/* events in [get..put) */
	splx(s);
	n = howmany(uio->uio_resid, sizeof(struct wscons_event));
	if (cnt > n)
		cnt = n;
	error = uiomovei((caddr_t)&ev->q[ev->get],
	    cnt * sizeof(struct wscons_event), uio);
	n -= cnt;
	/*
	 * If we do not wrap to 0, used up all our space, or had an error,
	 * stop.  Otherwise move from front of queue to put index, if there
	 * is anything there to move.
	 */
	if ((ev->get = (ev->get + cnt) % WSEVENT_QSIZE) != 0 ||
	    n == 0 || error || (cnt = ev->put) == 0)
		return (error);
	if (cnt > n)
		cnt = n;
	error = uiomovei((caddr_t)&ev->q[0],
	    cnt * sizeof(struct wscons_event), uio);
	ev->get = cnt;
	return (error);
}

int
wsevent_poll(struct wseventvar *ev, int events, struct proc *p)
{
	int revents = 0;
	int s = splwsevent();

	if (events & (POLLIN | POLLRDNORM)) {
		if (ev->get != ev->put)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &ev->sel);
	}

	splx(s);
	return (revents);
}
