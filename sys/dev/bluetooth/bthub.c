/*	$OpenBSD: bthub.c,v 1.5 2010/08/29 15:12:28 jasper Exp $	*/

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
#include <sys/fcntl.h>
#include <sys/vnode.h>

#include <netbt/bluetooth.h>

#include <dev/bluetooth/btdev.h>

struct bthub_softc {
	struct device sc_dev;
	int sc_open;
	bdaddr_t sc_laddr;
	LIST_HEAD(, btdev) sc_list;
};

int	bthub_match(struct device *, void *, void *);
void	bthub_attach(struct device *, struct device *, void *);
int	bthub_detach(struct device *, int);
int	bthub_print(void *, const char *);
int	bthub_devioctl(dev_t, u_long, struct btdev_attach_args *);

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
	struct bthub_softc *sc = (struct bthub_softc *)self;
	bdaddr_t *addr = aux;

	sc->sc_open = 0;
	bdaddr_copy(&sc->sc_laddr, addr);

	printf(" %02x:%02x:%02x:%02x:%02x:%02x\n",
	    addr->b[5], addr->b[4], addr->b[3],
	    addr->b[2], addr->b[1], addr->b[0]);
}

int
bthub_detach(struct device *self, int flags)
{
	struct bthub_softc *sc = (struct bthub_softc *)self;
	struct btdev *btdev;
	int maj, mn;
	int err;

	/* Locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == bthubopen)
			break;

	/* Nuke the vnodes for any open instances (calls close) */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	/* Detach all child devices. */
	while (!LIST_EMPTY(&sc->sc_list)) {
		btdev = LIST_FIRST(&sc->sc_list);
		LIST_REMOVE(btdev, sc_next);

		err = config_detach(&btdev->sc_dev, flags);
		if (err && (flags & DETACH_FORCE) == 0) {
			LIST_INSERT_HEAD(&sc->sc_list, btdev, sc_next);
			return err;
		}
	}

	return (0);
}

int
bthub_print(void *aux, const char *parentname)
{
	struct btdev_attach_args *bd = aux;
	bdaddr_t *raddr = &bd->bd_raddr;

	if (parentname != NULL)
		return QUIET;

	printf(" %02x:%02x:%02x:%02x:%02x:%02x",
	    raddr->b[5], raddr->b[4], raddr->b[3], raddr->b[2],
	    raddr->b[1], raddr->b[0]);
	return QUIET;
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
	struct btdev_attach_args *bd;
	int err;

	switch (cmd) {
	case BTDEV_ATTACH:
	case BTDEV_DETACH:
		if ((flag & FWRITE) == 0)
			return (EACCES);
	default:
		break;
	}

	switch (cmd) {
	case BTDEV_ATTACH:
	case BTDEV_DETACH:
		bd = (struct btdev_attach_args *)data;
		err = bthub_devioctl(dev, cmd, bd);
		break;
	default:
		err = ENOTTY;
	}

	return err;
}

int
bthub_devioctl(dev_t dev, u_long cmd, struct btdev_attach_args *bd)
{
	struct device *dv;
	struct bthub_softc *sc;
	struct btdev *btdev;
	int unit;

	/* Locate the relevant bthub. */
	for (unit = 0; unit < bthub_cd.cd_ndevs; unit++) {
		if ((dv = bthub_cd.cd_devs[unit]) == NULL)
			continue;

		sc = (struct bthub_softc *)dv;
		if (bdaddr_same(&sc->sc_laddr, &bd->bd_laddr))
			break;
	}
	if (unit == bthub_cd.cd_ndevs)
		return (ENXIO);

	/* Locate matching child device, if any. */
	LIST_FOREACH(btdev, &sc->sc_list, sc_next) {
		if (!bdaddr_same(&btdev->sc_addr, &bd->bd_raddr))
			continue;
		if (btdev->sc_type != bd->bd_type)
			continue;
		break;
	}

	switch (cmd) {
	case BTDEV_ATTACH:
		if (btdev != NULL)
			return EADDRINUSE;

		dv = config_found(&sc->sc_dev, bd, bthub_print);
		if (dv == NULL)
			return ENXIO;

		btdev = (struct btdev *)dv;
		bdaddr_copy(&btdev->sc_addr, &bd->bd_raddr);
		btdev->sc_type = bd->bd_type;
		LIST_INSERT_HEAD(&sc->sc_list, btdev, sc_next);
		break;

	case BTDEV_DETACH:
		if (btdev == NULL)
			return ENXIO;

		LIST_REMOVE(btdev, sc_next);
		config_detach(&btdev->sc_dev, DETACH_FORCE);
		break;
	}

	return 0;
}

