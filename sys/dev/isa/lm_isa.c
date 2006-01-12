/*	$OpenBSD: lm_isa.c,v 1.4 2006/01/12 22:34:51 kettenis Exp $	*/
/*	$NetBSD: lm_isa.c,v 1.9 2002/11/15 14:55:44 ad Exp $ */

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Squier.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/sensors.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/ic/nslm7xvar.h>

#if defined(LMDEBUG)
#define DPRINTF(x)		do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

int  lm_isa_match(struct device *, void *, void *);
void lm_isa_attach(struct device *, struct device *, void *);
u_int8_t lm_isa_readreg(struct lm_softc *, int);
void lm_isa_writereg(struct lm_softc *, int, int);

struct cfattach lm_isa_ca = {
	sizeof(struct lm_softc),
	lm_isa_match,
	lm_isa_attach
};

int
lm_isa_match(struct device *parent, void *match, void *aux)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct isa_attach_args *ia = aux;
	int iobase, banksel, vendid, addr;

	iot = ia->ia_iot;
	iobase = ia->ipa_io[0].base;

	if (bus_space_map(iot, iobase, 8, 0, &ioh)) {
		DPRINTF(("%s: can't map i/o space\n", __func__));
		return (0);
	}

	/* Probe for Winbond chips. */
	bus_space_write_1(iot, ioh, LMC_ADDR, WB_BANKSEL);
	banksel = bus_space_read_1(iot, ioh, LMC_DATA);
	bus_space_write_1(iot, ioh, LMC_ADDR, WB_VENDID);
	vendid = bus_space_read_1(iot, ioh, LMC_DATA);
	if (((banksel & 0x80) && vendid == (WB_VENDID_WINBOND >> 8)) ||
	    (!(banksel & 0x80) && vendid == (WB_VENDID_WINBOND & 0xff)))
		goto found;

	/*
	 * Probe for LM78/79/81.
	 *
	 * XXX Assumes the address has not been changed from the
	 * power up default.
	 */ 
	bus_space_write_1(iot, ioh, LMC_ADDR, LMD_SBUSADDR);
	addr = bus_space_read_1(iot, ioh, LMC_DATA);
	if ((addr & 0xfc) == 0x2c)
		goto found;

	bus_space_unmap(iot, ioh, 8);

	return (0);

 found:
	bus_space_unmap(iot, ioh, 8);

	ia->ipa_nio = 1;
	ia->ipa_io[0].length = 8;

	ia->ipa_nmem = 0;
	ia->ipa_nirq = 0;
	ia->ipa_ndrq = 0;

	return (1);
}

void
lm_isa_attach(struct device *parent, struct device *self, void *aux)
{
	struct lm_softc *sc = (struct lm_softc *)self;
	int iobase;
	bus_space_tag_t iot;
	struct isa_attach_args *ia = aux;

        iobase = ia->ipa_io[0].base;
	iot = sc->lm_iot = ia->ia_iot;

	if (bus_space_map(iot, iobase, 8, 0, &sc->lm_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	/* Bus-independant attachment */
	sc->lm_writereg = lm_isa_writereg;
	sc->lm_readreg = lm_isa_readreg;
	lm_attach(sc);
}

u_int8_t
lm_isa_readreg(struct lm_softc *sc, int reg)
{
	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_ADDR, reg);
	return (bus_space_read_1(sc->lm_iot, sc->lm_ioh, LMC_DATA));
}

void
lm_isa_writereg(struct lm_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_ADDR, reg);
	bus_space_write_1(sc->lm_iot, sc->lm_ioh, LMC_DATA, val);
}
