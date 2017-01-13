/*	$OpenBSD: vmmci.c,v 1.1 2017/01/13 14:37:32 reyk Exp $	*/

/*
 * Copyright (c) 2017 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/timeout.h>
#include <sys/signalvar.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/pci/virtioreg.h>
#include <dev/pci/virtiovar.h>
#include <dev/pv/pvvar.h>
#include <dev/rndvar.h>

enum vmmci_cmd {
	VMMCI_NONE = 0,
	VMMCI_SHUTDOWN,
	VMMCI_REBOOT,
};

struct vmmci_softc {
	struct device		 sc_dev;
	struct virtio_softc	*sc_virtio;
	enum vmmci_cmd		 sc_cmd;
	unsigned int		 sc_interval;
	struct timeout		 sc_tick;
};

int	vmmci_match(struct device *, void *, void *);
void	vmmci_attach(struct device *, struct device *, void *);

int	vmmci_config_change(struct virtio_softc *);

struct cfattach vmmci_ca = {
	sizeof(struct vmmci_softc),
	vmmci_match,
	vmmci_attach,
	NULL
};

struct cfdriver vmmci_cd = {
	NULL, "vmmci", DV_DULL
};

int
vmmci_match(struct device *parent, void *match, void *aux)
{
	struct virtio_softc *va = aux;
	if (va->sc_childdevid == PCI_PRODUCT_VIRTIO_VMMCI)
		return (1);
	return (0);
}

void
vmmci_attach(struct device *parent, struct device *self, void *aux)
{
	struct vmmci_softc *sc = (struct vmmci_softc *)self;
	struct virtio_softc *vsc = (struct virtio_softc *)parent;

	if (vsc->sc_child != NULL)
		panic("already attached to something else");

	vsc->sc_child = self;
	vsc->sc_nvqs = 0;
	vsc->sc_config_change = vmmci_config_change;
	vsc->sc_ipl = IPL_NET;
	sc->sc_virtio = vsc;

	virtio_negotiate_features(vsc, 0, NULL);

	printf("\n");
}

int
vmmci_config_change(struct virtio_softc *vsc)
{
	struct vmmci_softc *sc = (struct vmmci_softc *)vsc->sc_child;
	int cmd = virtio_read_device_config_1(vsc, 0);

	if (cmd == sc->sc_cmd)
		return (0);
	sc->sc_cmd = cmd;

	switch (cmd) {
	case VMMCI_NONE:
		/* no action */
		break;
	case VMMCI_SHUTDOWN:
		pvbus_shutdown(&sc->sc_dev);
		break;
	case VMMCI_REBOOT:
		pvbus_reboot(&sc->sc_dev);
		break;
	default:
		printf("%s: invalid command %d\n", sc->sc_dev.dv_xname, cmd);
		break;
	}

	return (1);
}
