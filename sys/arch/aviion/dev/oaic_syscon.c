/*	$OpenBSD: oaic_syscon.c,v 1.1 2013/10/15 01:41:44 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
#include <sys/device.h>
#include <sys/buf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/bus.h>
#include <machine/av400.h>

#include <aviion/dev/sysconvar.h>

#include <dev/ic/aic6250reg.h>
#include <dev/ic/aic6250var.h>

int	oaic_syscon_match(struct device *, void *, void *);
void	oaic_syscon_attach(struct device *, struct device *, void *);

struct oaic_syscon_softc {
	struct aic6250_softc	sc_base;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct intrhand		sc_ih;
};

const struct cfattach oaic_syscon_ca = {
	sizeof(struct oaic_syscon_softc),
	oaic_syscon_match, oaic_syscon_attach
};

uint8_t	oaic_read(struct aic6250_softc *, uint);
void	oaic_write(struct aic6250_softc *, uint, uint8_t);

int
oaic_syscon_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;

	switch (cpuid) {
#ifdef AV400
	case AVIION_300_310:
	case AVIION_400_4000:
	case AVIION_410_4100:
	case AVIION_300C_310C:
	case AVIION_300CD_310CD:
	case AVIION_300D_310D:
	case AVIION_4300_25:
	case AVIION_4300_20:
	case AVIION_4300_16:
		switch (ca->ca_paddr) {
		case AV400_SCSI:
			break;
		default:
			return 0;
		}
		break;
#endif
	default:
		return 0;
	}

	return 1;
}

void
oaic_syscon_attach(struct device *parent, struct device *self, void *aux)
{
	struct oaic_syscon_softc *ssc = (struct oaic_syscon_softc *)self;
	struct aic6250_softc *sc = (struct aic6250_softc *)self;
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int intsrc;

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, AIC_NREG << 2, 0,
	    &ioh) != 0) {
		printf(": can't map registers\n");
		return;
	}
	ssc->sc_iot = ca->ca_iot;
	ssc->sc_ioh = ioh;

	/*
	 * Do NOT ask any question about this.
	 */
	*(volatile uint32_t *)0xfff840c0 = 0x6e;

	/*
	 * According to the hardware manual (chapter 10 ``Programming the
	 * Small Computer System Interface'', page 10-2), the ``Clock
	 * Frequency Mode'' bit in control register #1 must be clear. This
	 * hints the AIC6250 runs at 10MHz.
	 */
	sc->sc_freq = 10;
	sc->sc_initiator = 7;

	/* port A is an output port, single-ended mode */
	sc->sc_cr0 = AIC_CR0_EN_PORT_A;
	/* port B used as the upper 8 bits of the 16-bit DMA path */
	sc->sc_cr1 = AIC_CR1_ENABLE_16BIT_MEM_BUS;

	sc->sc_read = oaic_read;
	sc->sc_write = oaic_write;

	aic6250_attach(sc);

	ssc->sc_ih.ih_fn = (int(*)(void *))aic6250_intr;
	ssc->sc_ih.ih_arg = sc;
	ssc->sc_ih.ih_flags = 0;
	ssc->sc_ih.ih_ipl = IPL_BIO;
	intsrc = INTSRC_SCSI1;
	sysconintr_establish(intsrc, &ssc->sc_ih, self->dv_xname);
}

uint8_t
oaic_read(struct aic6250_softc *sc, uint reg)
{
	struct oaic_syscon_softc *ssc = (struct oaic_syscon_softc *)sc;
	uint32_t rc;

	rc = bus_space_read_4(ssc->sc_iot, ssc->sc_ioh, reg << 2);
	return rc & 0xff;
}

void
oaic_write(struct aic6250_softc *sc, uint reg, uint8_t val)
{
	struct oaic_syscon_softc *ssc = (struct oaic_syscon_softc *)sc;

	bus_space_write_4(ssc->sc_iot, ssc->sc_ioh, reg << 2, val);
}
