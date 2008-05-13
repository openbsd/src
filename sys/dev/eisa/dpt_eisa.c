/*	$OpenBSD: dpt_eisa.c,v 1.4 2008/05/13 02:24:08 brad Exp $	*/
/*	$NetBSD: dpt_eisa.c,v 1.2 1999/10/18 21:59:19 ad Exp $	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * EISA front-end for DPT EATA SCSI driver.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#ifdef __NetBSD__
#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#endif /* __OpenBSD__ */

#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/dptreg.h>
#include <dev/ic/dptvar.h>

#define DPT_EISA_SLOT_OFFSET		0x0c00
#define DPT_EISA_IOSIZE			0x0100
#define DPT_EISA_IOCONF			0x90
#define DPT_EISA_EATA_REG_OFFSET	0x88

int	dpt_eisa_irq(bus_space_tag_t, bus_space_handle_t, int *);
#ifdef __NetBSD__
int	dpt_eisa_match(struct device *, struct cfdata *, void *);
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
int	dpt_eisa_match(struct device *, void *, void *);
#endif /* __OpenBSD__ */
void	dpt_eisa_attach(struct device *, struct device *, void *);

struct cfattach dpt_eisa_ca = {
	sizeof(struct dpt_softc), dpt_eisa_match, dpt_eisa_attach
};

const char *dpt_eisa_boards[] = {
	"DPT2402",
	"DPTA401",
	"DPTA402",
	"DPTA410",
	"DPTA411",
	"DPTA412",
	"DPTA420",
	"DPTA501",
	"DPTA502",
	"DPTA701",
	"DPTBC01",
	"NEC8200",	/* OEM */
	"ATT2408",	/* OEM */
	NULL
}; 

int
dpt_eisa_irq(iot, ioh, irq)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int *irq;
{

	switch (bus_space_read_1(iot, ioh, DPT_EISA_IOCONF) & 0x38) {
	case 0x08:
		*irq = 11;
		break;
	case 0x10:
		*irq = 15;
		break;
	case 0x20:
		*irq = 14;
		break;
	default:
		return (-1);
	}

	return (0);
}

int
dpt_eisa_match(parent, match, aux)
	struct device *parent;
#ifdef __NetBSD__
	struct cfdata *match;
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	void *match;
#endif /* __OpenBSD__ */
	void *aux;
{
	struct eisa_attach_args *ea;
	int i;

	ea = aux;

	for (i = 0; dpt_eisa_boards[i] != NULL; i++)
		if (strcmp(ea->ea_idstring, dpt_eisa_boards[i]) == 0)
			break;

	return (dpt_eisa_boards[i] != NULL);
}

void
dpt_eisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct eisa_attach_args *ea;
	bus_space_handle_t ioh;
	eisa_chipset_tag_t ec;
	eisa_intr_handle_t ih;
	struct dpt_softc *sc;
	bus_space_tag_t iot;
	const char *intrstr;
	int irq;
	
	ea = aux;
	sc = (struct dpt_softc *)self;
	iot = ea->ea_iot;
	ec = ea->ea_ec;
	
	printf(": ");

	if (bus_space_map(iot, EISA_SLOT_ADDR(ea->ea_slot) +
	    DPT_EISA_SLOT_OFFSET, DPT_EISA_IOSIZE, 0, &ioh)) {
		printf("can't map i/o space\n");
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ea->ea_dmat;

	/* Map and establish the interrupt. */
	if (dpt_eisa_irq(iot, ioh, &irq)) {
		printf("HBA on invalid IRQ (%d)\n", irq);
		return;
	}

	if (eisa_intr_map(ec, irq, &ih)) {
		printf("can't map interrupt (%d)\n", irq);
		return;
	}
	
	intrstr = eisa_intr_string(ec, ih);
#ifdef __NetBSD__
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    dpt_intr, sc);
#endif /* __NetBSD__ */
#ifdef __OpenBSD__
	sc->sc_ih = eisa_intr_establish(ec, ih, IST_LEVEL, IPL_BIO,
	    dpt_intr, sc, sc->sc_dv.dv_xname);
#endif /* __OpenBSD__ */
	if (sc->sc_ih == NULL) {
		printf("can't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	/* Read the EATA configuration */
	if (dpt_readcfg(sc)) {
		printf("%s: readcfg failed - see dpt(4)\n", 
		    sc->sc_dv.dv_xname);
		return;	
	}

	/* Now attach to the bus-independent code */
	dpt_init(sc, intrstr);
}
