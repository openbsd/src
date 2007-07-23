/*	$OpenBSD: bthub.c,v 1.3 2007/07/23 14:45:38 mk Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/vnode.h>

#include <netbt/bluetooth.h>

struct bthub_softc {
	struct device sc_dev;
	int sc_open;
};

int	bthub_match(struct device *, void *, void *);
void	bthub_attach(struct device *, struct device *, void *);
int	bthub_detach(struct device *, int);

struct cfattach bthub_ca = {
	sizeof(struct bthub_softc), bthub_match, bthub_attach, bthub_detach
};

struct cfdriver bthub_cd = {
	NULL, "bthub", DV_DULL
};

int
bthub_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
bthub_attach(struct device *parent, struct device *self, void *aux)
{
	bdaddr_t *addr = aux;
	struct bthub_softc *sc = (struct bthub_softc *)self;

	sc->sc_open = 0;

	printf(" %02x:%02x:%02x:%02x:%02x:%02x\n",
	    addr->b[5], addr->b[4], addr->b[3],
	    addr->b[2], addr->b[1], addr->b[0]);
}

int
bthub_detach(struct device *self, int flags)
{
	int maj, mn;

	/* Locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == bthubopen)
			break;

	/* Nuke the vnodes for any open instances (calls close) */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	return (0);
}

int
bthubopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct device *dv;
	struct bthub_softc *sc;

	dv = device_lookup(&bthub_cd, minor(dev));
	if (dv == NULL)
		return (ENXIO);

	sc = (struct bthub_softc *)dv;
	if (sc->sc_open) {
		device_unref(dv);
		return (EBUSY);
	}

	sc->sc_open = 1;
	device_unref(dv);

	return (0);
}

int
bthubclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct device *dv;
	struct bthub_softc *sc;

	dv = device_lookup(&bthub_cd, minor(dev));
	sc = (struct bthub_softc *)dv;
	sc->sc_open = 0;
	device_unref(dv);

	return (0);
}

int
bthubioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (ENOTTY);
}

