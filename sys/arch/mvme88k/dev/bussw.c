/*	$OpenBSD: bussw.c,v 1.20 2007/12/13 18:50:10 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme88k/dev/busswreg.h>

struct bussw_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

void    bussw_attach(struct device *, struct device *, void *);
int     bussw_match(struct device *, void *, void *);

struct cfattach bussw_ca = {
	sizeof(struct bussw_softc), bussw_match, bussw_attach
};

struct cfdriver bussw_cd = {
	NULL, "bussw", DV_DULL
};

int	bussw_print(void *, const char *);
int	bussw_scan(struct device *, void *, void *);

int
bussw_match(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	bus_space_handle_t ioh;
	int rc;
	u_int8_t chipid;

	/* Don't match if wrong cpu */
	if (brdtyp != BRD_197)
		return 0;

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, BS_SIZE, 0, &ioh) != 0)
		return 0;
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	if (rc == 0) {
		chipid = bus_space_read_1(ca->ca_iot, ioh, BS_CHIPID);
		if (chipid != BUSSWITCH_ID) {
#ifdef DEBUG
			printf("==> busswitch: wrong chip id %x\n", chipid);
#endif
			rc = -1;
		}
	}
	bus_space_unmap(ca->ca_iot, ioh, BS_SIZE);

	return rc == 0;
}

void
bussw_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct bussw_softc *sc = (struct bussw_softc *)self;
	bus_space_handle_t ioh;
	int i;

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, BS_SIZE, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}

	sc->sc_iot = ca->ca_iot;
	sc->sc_ioh = ioh;

	bus_space_write_1(sc->sc_iot, ioh, BS_VBASE,
	    bus_space_read_1(sc->sc_iot, ioh, BS_VBASE) | BS_VECBASE);

	/* enable external interrupts */
	bus_space_write_2(sc->sc_iot, ioh, BS_GCSR,
	    bus_space_read_2(sc->sc_iot, ioh, BS_GCSR) | BS_GCSR_XIPL);

	/* disable write posting */
	for (i = 0; i < 4; i++)
		bus_space_write_1(sc->sc_iot, ioh, BS_PAR + i,
		    bus_space_read_1(sc->sc_iot, ioh, BS_PAR + i) & ~BS_PAR_WPEN);

	/* enable abort switch */
	bus_space_write_1(sc->sc_iot, ioh, BS_ABORT,
	    bus_space_read_1(sc->sc_iot, ioh, BS_ABORT) | BS_ABORT_IEN);

	printf(": rev %x\n",
	    bus_space_read_1(sc->sc_iot, ioh, BS_CHIPREV));

	config_search(bussw_scan, self, args);
}

int
bussw_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
bussw_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct confargs oca, *ca = args;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1) {
		oca.ca_paddr = ca->ca_paddr + oca.ca_offset;
	} else {
		oca.ca_paddr = -1;
	}
	oca.ca_bustype = BUS_BUSSWITCH;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, bussw_print);
	return (1);
}
