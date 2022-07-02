/*	$OpenBSD: hotplug.c,v 1.22 2022/07/02 08:50:41 visa Exp $	*/
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
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/hotplug.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>

#define HOTPLUG_MAXEVENTS	64

static int opened;
static struct hotplug_event evqueue[HOTPLUG_MAXEVENTS];
static int evqueue_head, evqueue_tail, evqueue_count;
static struct selinfo hotplug_sel;

void filt_hotplugrdetach(struct knote *);
int  filt_hotplugread(struct knote *, long);

const struct filterops hotplugread_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_hotplugrdetach,
	.f_event	= filt_hotplugread,
};

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
		continue;
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

	error = tsleep_nsec(&evqueue, PRIBIO | PCATCH, "htplev", INFSLP);
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
	klist_insert_locked(klist, kn);
	splx(s);
	return (0);
}

void
filt_hotplugrdetach(struct knote *kn)
{
	int s;

	s = splbio();
	klist_remove_locked(&hotplug_sel.si_note, kn);
	splx(s);
}

int
filt_hotplugread(struct knote *kn, long hint)
{
	kn->kn_data = evqueue_count;

	return (evqueue_count > 0);
}
