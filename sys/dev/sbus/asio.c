/*	$OpenBSD: asio.c,v 1.1 2002/03/06 16:09:46 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for Aurora 210SJ serial ports.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>
#include <sys/tty.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/sbus/sbusvar.h>
#include <dev/sbus/asioreg.h>
#include <dev/ic/comvar.h>

#define BAUD_BASE       (1843200)

struct asio_port {
	u_int8_t		ap_inten;
	bus_space_handle_t	ap_bh;
	struct device		*ap_dev;
};

struct asio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_bt;
	bus_space_handle_t	sc_csr_h;
	void			*sc_ih;
	struct asio_port	sc_ports[2];
	int			sc_nports;
};

struct asio_attach_args {
	char *aaa_name;
	int aaa_port;
	bus_space_tag_t aaa_iot;
	bus_space_handle_t aaa_ioh;
};

int	asio_match __P((struct device *, void *, void *));
void	asio_attach __P((struct device *, struct device *, void *));
int	asio_print __P((void *, const char *));
int	asio_intr __P((void *));

struct cfattach asio_ca = {
	sizeof(struct asio_softc), asio_match, asio_attach
};

struct cfdriver asio_cd = {
	NULL, "asio", DV_DULL
};

int	com_asio_match __P((struct device *, void *, void *));
void	com_asio_attach __P((struct device *, struct device *, void *));

struct cfattach com_asio_ca = {
	sizeof(struct com_softc), com_asio_match, com_asio_attach
};

int
asio_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct sbus_attach_args *sa = aux;

	if (strcmp(sa->sa_name, "sio2") == 0)
		return (1);
	return (0);
}

void
asio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct asio_softc *sc = (void *)self;
	struct sbus_attach_args *sa = aux;
	struct asio_attach_args aaa;
	int i, map;
	u_int8_t csr;

	sc->sc_bt = sa->sa_bustag;

	if (sa->sa_nreg < 3) {
		printf(": %d registers expected, got %d\n",
		    3, sa->sa_nreg);
		return;
	}

	if (sbus_bus_map(sa->sa_bustag,
	    sa->sa_reg[0].sbr_slot,
	    sa->sa_reg[0].sbr_offset,
	    sa->sa_reg[0].sbr_size,
	    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_csr_h)) {
		printf(": couldn't map csr\n");
		return;
	}

	for (i = 0; i < 2; i++) {
		if (sbus_bus_map(sa->sa_bustag,
		    sa->sa_reg[i + 1].sbr_slot,
		    sa->sa_reg[i + 1].sbr_offset,
		    sa->sa_reg[i + 1].sbr_size,
		    BUS_SPACE_MAP_LINEAR, 0, &sc->sc_ports[i].ap_bh)) {
			printf(": couldn't map uart%d\n", i);
			return;
		}
	}

	/* XXX: revision specific*/
	sc->sc_ports[0].ap_inten = ASIO_CSR_SJ_UART0_INTEN;
	sc->sc_ports[1].ap_inten = ASIO_CSR_UART1_INTEN;

	printf("\n");

	sc->sc_nports = 2;

	for (i = 0; i < sc->sc_nports; i++) {
		aaa.aaa_name = "com";
		aaa.aaa_port = i;
		aaa.aaa_iot = sc->sc_bt;
		aaa.aaa_ioh = sc->sc_ports[i].ap_bh;
		sc->sc_ports[i].ap_dev = config_found(self, &aaa, asio_print);
	}

	/* XXX won't work if pio comes along later and zots this.. */
	csr = ASIO_CSR_SBUS_INT5;
	for (map = 0, i = 0; i < sc->sc_nports; i++) {
		if (sc->sc_ports[i].ap_dev != NULL) {
			map = 1;
			csr |= sc->sc_ports[i].ap_inten;
		}
	}
	csr |= 2;
	bus_space_write_1(sc->sc_bt, sc->sc_csr_h, ASIO_CSR, csr);

	if (map == 0)
		return;

	sc->sc_ih = bus_intr_establish(sa->sa_bustag, sa->sa_pri,
	    IPL_TTY, 0, asio_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: failed to map interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}
}

int
asio_print(aux, name)
	void *aux;
	const char *name;
{
	struct asio_attach_args *aaa;

	if (name != NULL)
		printf("%s at %s", aaa->aaa_name, name);
	printf(" port %d", aaa->aaa_port);
	return (UNCONF);
}

int
asio_intr(vsc)
	void *vsc;
{
	struct asio_softc *sc = vsc;
	int i, r = 0;

	for (i = 0; i < sc->sc_nports; i++)
		r += comintr(sc->sc_ports[i].ap_dev);
	return (r);
}

int
com_asio_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}

void
com_asio_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (struct com_softc *)self;
	struct asio_attach_args *aaa = aux;

	sc->sc_iot = aaa->aaa_iot;
	sc->sc_ioh = aaa->aaa_ioh;
	sc->sc_iobase = sc->sc_ioh;
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
	sc->sc_frequency = BAUD_BASE;
	com_attach_subr(sc);
}
