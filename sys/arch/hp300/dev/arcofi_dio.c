/*	$OpenBSD: arcofi_dio.c,v 1.1 2011/12/21 23:12:03 miod Exp $	*/

/*
 * Copyright (c) 2011 Miodrag Vallat.
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

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/ic/arcofivar.h>

#include <hp300/dev/dioreg.h>
#include <hp300/dev/diovar.h>

#include <hp300/dev/diodevs.h>

void	arcofi_dio_attach(struct device *, struct device *, void *);
int	arcofi_dio_match(struct device *, void *, void *);

struct arcofi_dio_softc {
	struct arcofi_softc	sc_arcofi;

	struct isr		sc_isr;
};

const struct cfattach arcofi_dio_ca = {
	sizeof(struct arcofi_dio_softc),
	arcofi_dio_match,
	arcofi_dio_attach
};

extern struct hp300_bus_space_tag hp300_mem_tag; /* XXX */

int
arcofi_dio_match(struct device *parent, void *match, void *vaa)
{
	struct dio_attach_args *da = vaa;

	if (da->da_id != DIO_DEVICE_ID_AUDIO)
		return 0;

	return 1;
}

void
arcofi_dio_attach(struct device *parent, struct device *self, void *vaa)
{
	struct arcofi_dio_softc *adsc = (struct arcofi_dio_softc *)self;
	struct arcofi_softc *sc = &adsc->sc_arcofi;
	struct dio_attach_args *da = vaa;
	bus_addr_t base;
	unsigned int u;
	int ipl;

	for (u = 0; u < ARCOFI_NREGS; u++)
		sc->sc_reg[u] = (u << 1) | 0x01;

	base = (bus_addr_t)dio_scodetopa(da->da_scode);
	sc->sc_iot = &hp300_mem_tag;
	/*
	 * XXX We request BUS_SPACE_MAP_LINEAR only to be able to use DIO_IPL
	 * XXX below; this ought to be provided in the attach_args
	 */
	if (bus_space_map(sc->sc_iot, base, DIOII_SIZEOFF, BUS_SPACE_MAP_LINEAR,
	    &sc->sc_ioh) != 0) {
		printf(": can't map registers\n");
		return;
	}
	ipl = DIO_IPL(bus_space_vaddr(sc->sc_iot, sc->sc_ioh));

	sc->sc_sih = softintr_establish(IPL_SOFT, &arcofi_swintr, sc);
	if (sc->sc_sih == NULL) {
		printf(": can't register soft interrupt\n");
		return;
	}
	adsc->sc_isr.isr_func = arcofi_hwintr;
	adsc->sc_isr.isr_arg = sc;
	adsc->sc_isr.isr_ipl = ipl;
	adsc->sc_isr.isr_priority = IPL_AUDIO;
	dio_intr_establish(&adsc->sc_isr, self->dv_xname);

	printf(" ipl %d\n", ipl);

	arcofi_attach(sc, "dio");
}
