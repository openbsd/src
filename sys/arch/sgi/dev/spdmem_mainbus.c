/*	$OpenBSD: spdmem_mainbus.c,v 1.1 2010/03/22 21:22:08 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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
 * Display SPD memory information obtained from an IP35 brick L1 controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <dev/spdmemvar.h>

#include <mips64/archtype.h>

#include <machine/autoconf.h>

#include <sgi/sgi/l1.h>

struct spdmem_mainbus_softc {
	struct spdmem_softc	 sc_base;
	uint8_t			*sc_spd;
	size_t			 sc_spdlen;
};

int	spdmem_mainbus_match(struct device *, void *, void *);
void	spdmem_mainbus_attach(struct device *, struct device *, void *);
uint8_t	spdmem_mainbus_read(struct spdmem_softc *, uint8_t);

struct cfattach spdmem_mainbus_ca = {
	sizeof(struct spdmem_mainbus_softc),
	spdmem_mainbus_match, spdmem_mainbus_attach
};

int
spdmem_mainbus_match(struct device *parent, void *vcf, void *aux)
{
	struct spdmem_attach_args *saa = (struct spdmem_attach_args *)aux;
	extern struct cfdriver spdmem_cd;
	int rc;
	uint8_t *spd;
	size_t spdlen;

	if (sys_config.system_type != SGI_IP35)
		return 0;

	if (strcmp(saa->maa.maa_name, spdmem_cd.cd_name) != 0)
		return 0;

	rc = l1_get_brick_spd_record(saa->maa.maa_nasid, saa->dimm,
	    &spd, &spdlen);
	if (rc == 0) {
		free(spd, M_DEVBUF);
		return 1;
	} else
		return 0;
}

void
spdmem_mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct spdmem_mainbus_softc *sc = (struct spdmem_mainbus_softc *)self;
	struct spdmem_attach_args *saa = (struct spdmem_attach_args *)aux;
	int rc;

	printf(" dimm %d:", saa->dimm);

	rc = l1_get_brick_spd_record(saa->maa.maa_nasid, saa->dimm,
	    &sc->sc_spd, &sc->sc_spdlen);
	if (rc != 0) {
		printf(" can't get SPD record from L1, error %d\n", rc);
		return;
	}

	sc->sc_base.sc_read = spdmem_mainbus_read;
	spdmem_attach_common(&sc->sc_base);
	/* free record, as it won't be accessed anymore */
	sc->sc_spdlen = 0;
	free(sc->sc_spd, M_DEVBUF);
}

uint8_t
spdmem_mainbus_read(struct spdmem_softc *v, uint8_t reg)
{
	struct spdmem_mainbus_softc *sc = (struct spdmem_mainbus_softc *)v;

	if (reg < sc->sc_spdlen)
		return sc->sc_spd[reg];
	else
		return 0;
}
