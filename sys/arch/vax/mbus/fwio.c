/*	$OpenBSD: fwio.c,v 1.1 2008/08/18 23:19:25 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
 * Firefox Workstation I/O Module
 *
 * This M-bus board sports:
 * - a System Support Chip implementing the (off cpu) clocks.
 * - a SII controller, in SCSI mode, with 128KB static memory.
 * - a LANCE Ethernet controller, with 128KB static memory.
 * - a DZQ11-compatible DC7085 4 lines serial controller.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/nexus.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>
#include <vax/mbus/fwiovar.h>

struct fwio_softc {
	struct device	sc_dev;
	int		sc_loc[1];	/* locators override */
};

void	fwio_attach(struct device *, struct device *, void *);
int	fwio_match(struct device *, void *, void *);

struct cfdriver fwio_cd = {
	NULL, "fwio", DV_DULL
};

const struct cfattach fwio_ca = {
	sizeof(struct fwio_softc), fwio_match, fwio_attach
};

int	fwio_print(void *, const char *);

int
fwio_match(struct device *parent, void *vcf, void *aux)
{
	struct mbus_attach_args *maa = (struct mbus_attach_args *)aux;

	if (maa->maa_class == CLASS_IO && maa->maa_interface == INTERFACE_FBIC)
		return 1;

	return 0;
}

void
fwio_attach(struct device *parent, struct device *self, void *aux)
{
	struct mbus_attach_args *maa = (struct mbus_attach_args *)aux;
	struct fwio_softc *sc = (struct fwio_softc *)self;
	struct fwio_attach_args faa;

	printf("\n");

	/*
	 * Save our mid in locators.  booted_sd() in autoconf.c depends
	 * on this to find the correct boot device.
	 */
	sc->sc_loc[0] = maa->maa_mid;
	self->dv_cfdata->cf_loc = sc->sc_loc;

	faa.faa_mid = maa->maa_mid;
	faa.faa_base = MBUS_SLOT_BASE(maa->maa_mid);
	faa.faa_vecbase = maa->maa_vecbase;

	faa.faa_dev = "dz";
	(void)config_found(self, &faa, fwio_print);

	faa.faa_dev = "le";
	(void)config_found(self, &faa, fwio_print);

	faa.faa_dev = "sii";
	(void)config_found(self, &faa, fwio_print);
}

int
fwio_print(void *aux, const char *pnp)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;

	if (pnp != NULL)
		printf("%s at %s", faa->faa_dev, pnp);

	return (UNCONF);
}
