/*	$OpenBSD: lpt_lbus.c,v 1.4 2005/01/31 21:35:50 grange Exp $	*/

/*
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
 * All rights reserved.
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
 *	This software is a component of "386BSD" developed by
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Device Driver for AT parallel printer port
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/lptreg.h>
#include <dev/ic/lptvar.h>

int lpt_localbus_probe(struct device *, void *, void *);
void lpt_localbus_attach(struct device *, struct device *, void *);

struct cfattach lpt_pica_ca = {
	sizeof(struct lpt_softc), lpt_localbus_probe, lpt_localbus_attach
};

struct cfattach lpt_algor_ca = {
	sizeof(struct lpt_softc), lpt_localbus_probe, lpt_localbus_attach
};

int
lpt_localbus_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct confargs *ca = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t base;
	u_int8_t mask, data;
	int i;

#ifdef DEBUG
#define	ABORT								     \
	do {								     \
		printf("lpt_localbus_probe: mask %x data %x failed\n", mask, \
		    data);						     \
		return 0;						     \
	} while (0)
#else
#define	ABORT	return 0
#endif

	if (!BUS_MATCHNAME(ca, "lpt"))
		 return(0);

/*XXX need to check where to pick up iotag when porting this */
	iot = sys_config.localbus_iot;
	base = (bus_addr_t)BUS_CVTADDR(ca);
	if (bus_space_map(iot, base, LPT_NPORTS, 0, &ioh)) {
		return 0;
        }

	mask = 0xff;

	data = 0x55;				/* Alternating zeros */
	if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
		ABORT;

	data = 0xaa;				/* Alternating ones */
	if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
		ABORT;

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking zero */
		data = ~(1 << i);
		if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	for (i = 0; i < CHAR_BIT; i++) {	/* Walking one */
		data = (1 << i);
		if (!lpt_port_test(iot, ioh, base, lpt_data, data, mask))
			ABORT;
	}

	bus_space_write_1(iot, ioh, lpt_data, 0);
	bus_space_write_1(iot, ioh, lpt_control, 0);

	return 1;
}

void
lpt_localbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lpt_softc *sc = (void *)self;
	struct confargs *ca = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t base;

	printf("\n");

	sc->sc_state = 0;
	iot = sc->sc_iot = &pmonmips_bus_io;
	base = (bus_space_handle_t)BUS_CVTADDR(ca);
	if (bus_space_map(iot, base, LPT_NPORTS, 0, &ioh)) {
		panic("unexpected bus_space_map error");
        }
	sc->sc_ioh = ioh;

	bus_space_write_1(iot, ioh, lpt_control, LPC_NINIT);

	BUS_INTR_ESTABLISH(ca, lptintr, sc);
}
