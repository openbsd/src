/*	$OpenBSD: sti_sgc.c,v 1.18 2003/10/30 19:25:12 mickey Exp $	*/

/*
 * Copyright (c) 2000-2003 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * These cards has to be known to work so far:
 *	- HPA1991AGrayscale rev 0.02	(705/35) (byte-wide)
 *	- HPA1991AC19       rev 0.02	(715/33) (byte-wide)
 *	- HPA208LC1280      rev 8.04	(712/80) just works
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <uvm/uvm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wsconsio.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

#include <hppa/dev/cpudevs.h>

#define	STI_MEMSIZE	0x2000000
#define	STI_ROMSIZE	0x8000
#define	STI_ID_FDDI	0x280b31af	/* Medusa FDDI ROM id */

/* gecko optional graphics */
#define	STI_GOPT1_REV	0x17
#define	STI_GOPT2_REV	0x70

/* internal EG */
#define	STI_INEG_REV	0x60
#define	STI_INEG_PROM	0xf0011000

int sti_sgc_probe(struct device *, void *, void *);
void sti_sgc_attach(struct device *, struct device *, void *);

struct cfattach sti_sgc_ca = {
	sizeof(struct sti_softc), sti_sgc_probe, sti_sgc_attach
};

struct cfattach sti_phantom_ca = {
	sizeof(struct sti_softc), sti_sgc_probe, sti_sgc_attach
};

/*
 * Locate STI ROM.
 * On some machines it may not be part of the HPA space.
 */
paddr_t
sti_sgc_getrom(int unit, struct confargs *ca)
{
	paddr_t rom = PAGE0->pd_resv2[1];

	if (unit) {
		if (ca->ca_type.iodc_sv_model == HPPA_FIO_GSGC &&
		    (ca->ca_type.iodc_revision == STI_GOPT1_REV ||
		     ca->ca_type.iodc_revision == STI_GOPT2_REV))
			/* these two share the onboard's prom */ ;
		else
			rom = 0;
	}

	if (rom < HPPA_IOBEGIN) {
		if (unit == 0 &&
		    ca->ca_type.iodc_sv_model == HPPA_FIO_GSGC &&
		    ca->ca_type.iodc_revision == STI_INEG_REV)
			rom = STI_INEG_PROM;
		else
			rom = ca->ca_hpa;
	}

	return (rom);
}

int
sti_sgc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;
	bus_space_handle_t romh;
	paddr_t rom;
	u_int32_t id;
	u_char devtype;
	int rv = 0, romunmapped = 0;

	if (ca->ca_type.iodc_type != HPPA_TYPE_FIO)
		return (0);

	/* these can only be graphics anyway */
	if (ca->ca_type.iodc_sv_model == HPPA_FIO_GSGC)
		return (1);

	/* these need futher checking for the graphics id */
	if (ca->ca_type.iodc_sv_model != HPPA_FIO_SGC)
		return 0;

	rom = sti_sgc_getrom(cf->cf_unit, ca);
#ifdef STIDEBUG
	printf ("sti: hpa=%x, rom=%x\n", ca->ca_hpa, rom);
#endif

	/* if it does not map, probably part of the lasi space */
	if ((rv = bus_space_map(ca->ca_iot, rom, STI_ROMSIZE, 0, &romh))) {
#ifdef STIDEBUG
		printf ("sti: cannot map rom space (%d)\n", rv);
#endif
		if ((rom & HPPA_IOBEGIN) == HPPA_IOBEGIN) {
			romh = rom;
			romunmapped++;
		} else {
			/* in this case nobody has no freaking idea */
			return 0;
		}
	}

#ifdef STIDEBUG
	printf("sti: romh=%x\n", romh);
#endif

	devtype = bus_space_read_1(ca->ca_iot, romh, 3);

#ifdef STIDEBUG
	printf("sti: devtype=%d\n", devtype);
#endif
	rv = 1;
	switch (devtype) {
	case STI_DEVTYPE4:
		id = bus_space_read_4(ca->ca_iot, romh, 0x8);
		break;
	case STI_DEVTYPE1:
		id = (bus_space_read_1(ca->ca_iot, romh, 0x10 +  3) << 24) |
		     (bus_space_read_1(ca->ca_iot, romh, 0x10 +  7) << 16) |
		     (bus_space_read_1(ca->ca_iot, romh, 0x10 + 11) <<  8) |
		     (bus_space_read_1(ca->ca_iot, romh, 0x10 + 15));

		break;
	default:
#ifdef STIDEBUG
		printf("sti: unknown type (%x)\n", devtype);
#endif
		rv = 0;
	}

	if (rv &&
	    ca->ca_type.iodc_sv_model == HPPA_FIO_SGC && id == STI_ID_FDDI) {
#ifdef STIDEBUG
		printf("sti: not a graphics device\n");
#endif
		rv = 0;
	}

	if (!romunmapped)
		bus_space_unmap(ca->ca_iot, romh, STI_ROMSIZE);
	return (rv);
}

void
sti_sgc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sti_softc *sc = (void *)self;
	struct confargs *ca = aux;
	paddr_t rom;
	int rv;

	rom = sti_sgc_getrom(sc->sc_dev.dv_cfdata->cf_unit, ca);

#ifdef STIDEBUG
	printf("sti: hpa=%x, rom=%x\n", ca->ca_hpa, rom);
#endif
	sc->memt = sc->iot = ca->ca_iot;

	if ((rv = bus_space_map(ca->ca_iot, ca->ca_hpa, STI_MEMSIZE, 0,
	    &sc->ioh))) {
#ifdef STIDEBUG
		printf(": cannot map io space (%d)\n", rv);
#endif
		return;
	}

	/* if it does not map, probably part of the lasi space */
	if (rom == ca->ca_hpa)
		sc->romh = sc->ioh;
	else if ((rv = bus_space_map(ca->ca_iot, rom, STI_ROMSIZE, 0, &sc->romh))) {
		if ((rom & HPPA_IOBEGIN) == HPPA_IOBEGIN)
			sc->romh = rom;
		else {
#ifdef STIDEBUG
			printf (": cannot map rom space (%d)\n", rv);
#endif
			/* in this case i have no freaking idea */
			bus_space_unmap(ca->ca_iot, sc->ioh,  STI_MEMSIZE);
			return;
		}
	}

	/* PCXL2: enale accel i/o for this space */
	if (cpu_type == hpcxl2)
		eaio_l2(0x8 >> (((ca->ca_hpa >> 25) & 3) - 2));

#ifdef STIDEBUG
	printf("sti: ioh=%x, romh=%x\n", sc->ioh, sc->romh);
#endif
	sc->sc_devtype = bus_space_read_1(sc->iot, sc->romh, 3);
	if (ca->ca_hpa == (hppa_hpa_t)PAGE0->mem_cons.pz_hpa)
		sc->sc_flags |= STI_CONSOLE;
	sti_attach_common(sc);
}
