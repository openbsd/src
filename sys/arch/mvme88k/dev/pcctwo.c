/*	$OpenBSD: pcctwo.c,v 1.24 2004/04/24 19:51:48 miod Exp $ */
/*
 * Copyright (c) 1995 Theo de Raadt
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * VME1x7 PCC2 chip
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/cons.h>

#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/pcctwovar.h>

#include "bussw.h"

void	pcctwoattach(struct device *, struct device *, void *);
int	pcctwomatch(struct device *, void *, void *);

struct cfattach pcctwo_ca = {
	sizeof(struct pcctwosoftc), pcctwomatch, pcctwoattach
};

struct cfdriver pcctwo_cd = {
	NULL, "pcctwo", DV_DULL
};

int	pcctwo_print(void *args, const char *bus);
int	pcctwo_scan(struct device *parent, void *child, void *args);

int
pcctwomatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	bus_space_handle_t ioh;
	int rc;
	u_int8_t chipid;

	/* Bomb if wrong cpu */
	switch (brdtyp) {
	case BRD_187:
	case BRD_8120:
	case BRD_197:
		break;
	default:
		return 0;
	}

	if (bus_space_map(ca->ca_iot, ca->ca_paddr + PCC2_BASE, PCC2_SIZE,
	    0, &ioh) != 0)
		return 0;
	rc = badvaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	if (rc == 0) {
		chipid = bus_space_read_1(ca->ca_iot, ioh, PCCTWO_CHIPID);
		if (chipid != PCC2_ID) {
#ifdef DEBUG
			printf("==> pcctwo: wrong chip id %x.\n", chipid);
			rc = -1;
#endif
		}
	}
	bus_space_unmap(ca->ca_iot, ioh, PCC2_SIZE);

	return rc == 0;
}

void
pcctwoattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)self;
	bus_space_handle_t ioh;
	u_int8_t genctl;

	sc->sc_base = ca->ca_paddr + PCC2_BASE;

	if (bus_space_map(ca->ca_iot, sc->sc_base, PCC2_SIZE, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}

	sc->sc_iot = ca->ca_iot;
	sc->sc_ioh = ioh;

	bus_space_write_1(sc->sc_iot, ioh, PCCTWO_VECBASE, PCC2_VECBASE);
	genctl = bus_space_read_1(sc->sc_iot, ioh, PCCTWO_GENCTL);
#if NBUSSW > 0
	if (ca->ca_bustype == BUS_BUSSWITCH) {
                /* Make sure the bus is mc68040 compatible */
		genctl |= PCC2_GENCTL_C040;
	}
#endif
	genctl |= PCC2_GENCTL_IEN;	/* global irq enable */
	bus_space_write_1(sc->sc_iot, ioh, PCCTWO_GENCTL, genctl);

	printf(": rev %d\n",
	    bus_space_read_1(sc->sc_iot, ioh, PCCTWO_CHIPREV));

	config_search(pcctwo_scan, self, args);
}

int
pcctwo_print(args, bus)
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
pcctwo_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct pcctwosoftc *sc = (struct pcctwosoftc *)parent;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = sc->sc_iot;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1) {
		/* offset locator for pcctwo children is relative to segment */
		oca.ca_paddr = sc->sc_base - PCC2_BASE + oca.ca_offset;
	} else {
		oca.ca_paddr = -1;
	}
	oca.ca_bustype = BUS_PCCTWO;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, pcctwo_print);
	return (1);
}

/*
 * PCC2 interrupts land in a PCC2_NVEC sized hole starting at PCC2_VECBASE
 */
int
pcctwointr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
#ifdef DIAGNOSTIC
	if (vec < 0 || vec >= PCC2_NVEC)
		panic("pcctwo_establish: illegal vector 0x%x\n", vec);
#endif

	return (intr_establish(PCC2_VECBASE + vec, ih));
}
