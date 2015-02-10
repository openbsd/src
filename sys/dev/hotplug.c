/*	$OpenBSD: hotplug.c,v 1.14 2015/02/10 21:58:16 miod Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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
 * Device attachment and detachment notifications.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/hotplug.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/vnode.h>

#define HOTPLUG_MAXEVENTS	64

static int opened;
static struct hotplug_event evqueue[HOTPLUG_MAXEVENTS];
static int evqueue_head, evqueue_tail, evqueue_count;
static struct selinfo hotplug_sel;

void filt_hotplugrdetach(struct knote *);
int  filt_hotplugread(struct knote *, long);

struct filterops hotplugread_filtops =
	{ 1, NULL, filt_hotplugrdetach, filt_hotplugread};

#define EVQUEUE_NEXT(p) (p == HOTPLUG_MAXEVENTS - 1 ? 0 : p + 1)


int hotplug_put_event(struct hotplug_event *);
int hotplug_get_event(struct hotplug_event *);

void hotplugattach(int);

void
hotplugattach(int count)
{
	opened = 0;
	evqueue_head = 0;
	evqueue_tail = 0;
	evqueue_count = 0;
}

void
hotplug_device_attach(enum devclass class, char *name)
{
	struct hotplug_event he;

	he.he_type = HOTPLUG_DEVAT;
	he.he_devclass = class;
	strlcpy(he.he_devname, name, sizeof(he.he_devname));
	hotplug_put_event(&he);
}

void
hotplug_device_detach(enum devclass class, char *name)
{
	struct hotplug_event he;

	he.he_type = HOTPLUG_DEVDT;
	he.he_devclass = class;
	strlcpy(he.he_devname, name, sizeof(he.he_devname));
	hotplug_put_event(&he);
}

int
hotplug_put_event(struct hotplug_event *he)
{
	if (evqueue_count == HOTPLUG_MAXEVENTS && opened) {
		printf("hotplug: event lost, queue full\n");
		return (1);
	}

	evqueue[evqueue_head] = *he;
	evqueue_head = EVQUEUE_NEXT(evqueue_head);
	if (evqueue_count == HOTPLUG_MAXEVENTS)
		evqueue_tail = EVQUEUE_NEXT(evqueue_tail);
	else 
		evqueue_count++;
	wakeup(&evqueue);
	selwakeup(&hotplug_sel);
	return (0);
}

int
hotplug_get_event(struct hotplug_event *he)
{
	int s;

	if (evqueue_count == 0)
		return (1);

	s = splbio();
	*he = evqueue[evqueue_tail];
	evqueue_tail = EVQUEUE_NEXT(evqueue_tail);
	evqueue_count--;
	splx(s);
	return (0);
}

int
hotplugopen(dev_t dev, int flag, int mode, struct proc *p)
{
	if (minor(dev) != 0)
		return (ENXIO);
	if ((flag & FWRITE))
		return (EPERM);
	if (opened)
		return (EBUSY);
	opened = 1;
	return (0);
}

int
hotplugclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct hotplug_event he;

	while (hotplug_get_event(&he) == 0)
		;
	opened = 0;
	return (0);
}

int
hotplugread(dev_t dev, struct uio *uio, int flags)
{
	struct hotplug_event he;
	int error;

	if (uio->uio_resid != sizeof(he))
		return (EINVAL);

again:
	if (hotplug_get_event(&he) == 0)
		return (uiomove(&he, sizeof(he), uio));
	if (flags & IO_NDELAY)
		return (EAGAIN);

	error = tsleep(&evqueue, PRIBIO | PCATCH, "htplev", 0);
	if (error)
		return (error);
	goto again;
}

int
hotplugioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	case FIOASYNC:
		/* ignore */
	case FIONBIO:
		/* handled in the upper fs layer */
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
hotplugpoll(dev_t dev, int events, struct proc *p)
{
	int revents = 0;

	if (events & (POLLIN | POLLRDNORM)) {
		if (evqueue_count > 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else
			selrecord(p, &hotplug_sel);
	}

	return (revents);
}

int
hotplugkqfilter(dev_t dev, struct knote *kn)
{
	struct klist *klist;
	int s;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		klist = &hotplug_sel.si_note;
		kn->kn_fop = &hotplugread_filtops;
		break;
	default:
		return (EINVAL);
	}

	s = splbio();
	SLIST_INSERT_HEAD(klist, kn, kn_selnext);
	splx(s);
	return (0);
}

void
filt_hotplugrdetach(struct knote *kn)
{
	int s;

	s = splbio();
	SLIST_REMOVE(&hotplug_sel.si_note, kn, knote, kn_selnext);
	splx(s);
}

int
filt_hotplugread(struct knote *kn, long hint)
{
	kn->kn_data = evqueue_count;

	return (evqueue_count > 0);
}
