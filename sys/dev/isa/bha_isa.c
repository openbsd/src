/*	$OpenBSD: bha_isa.c,v 1.4 2008/06/26 05:42:16 ray Exp $	*/
/*	$NetBSD: bha_isa.c,v 1.14 1998/08/15 10:10:51 mycroft Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/ic/bhareg.h>
#include <dev/ic/bhavar.h>

#include "aha.h"
#if NAHA > 0
int btports[8];	/* cannot be more */
int nbtports;
#endif

#define	BHA_ISA_IOSIZE	4

int	bha_isa_probe(struct device *, void *, void *);
void	bha_isa_attach(struct device *, struct device *, void *);

struct cfattach bha_isa_ca = {
	sizeof(struct bha_softc), bha_isa_probe, bha_isa_attach
};

/*
 * Check the slots looking for a board we recognise
 * If we find one, note its address (slot) and call
 * the actual probe routine to check it out.
 */
int
bha_isa_probe(parent, match, aux)
	struct device *parent;
	void *aux, *match;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct bha_probe_data bpd;
#if NAHA > 0
	struct bha_digit digit; 
#endif
	int rv;

	/* Disallow wildcarded i/o address. */
	if (ia->ia_iobase == IOBASEUNK)
		return (0);

	if (bus_space_map(iot, ia->ia_iobase, BHA_ISA_IOSIZE, 0, &ioh))
		return (0);

	rv = bha_find(iot, ioh, &bpd);

#if NAHA > 0
	if (rv) {
		/* Adaptec 1542 cards do not support this */
		digit.reply.digit = '@';
		digit.cmd.opcode = BHA_INQUIRE_REVISION_3;
		bha_cmd(iot, ioh, NULL, sizeof(digit.cmd), (u_char *)&digit.cmd,
		    sizeof(digit.reply), (u_char *)&digit.reply);
		if (digit.reply.digit == '@')
			return 1;
	}
#endif

	bus_space_unmap(iot, ioh, BHA_ISA_IOSIZE);

	if (rv) {
		if (ia->ia_irq != -1 && ia->ia_irq != bpd.sc_irq)
			return (0);
		if (ia->ia_drq != -1 && ia->ia_drq != bpd.sc_drq)
			return (0);
		ia->ia_irq = bpd.sc_irq;
		ia->ia_drq = bpd.sc_drq;
		ia->ia_msize = 0;
		ia->ia_iosize = BHA_ISA_IOSIZE;
	}
	return (rv);
}

/*
 * Attach all the sub-devices we can find
 */
void
bha_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	struct bha_softc *sc = (void *)self;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh;
	struct bha_probe_data bpd;
	isa_chipset_tag_t ic = ia->ia_ic;
#ifndef __OpenBSD__
	int error;
#endif

	printf("\n");

	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh)) {
		printf("%s: can't map i/o space\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = ia->ia_dmat;
	if (!bha_find(iot, ioh, &bpd)) {
		printf("%s: bha_find failed\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_dmaflags = 0;
	if (bpd.sc_drq != DRQUNK) {
#ifdef __OpenBSD__
		isa_dmacascade(ic, bpd.sc_drq);
#else
		if ((error = isa_dmacascade(ic, bpd.sc_drq)) != 0) {
			printf("%s: unable to cascade DRQ, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
#endif
	} else {
		/*
		 * We have a VLB controller.  If we're at least both
		 * hardware revision E and firmware revision 3.37,
		 * we can do 32-bit DMA (earlier revisions are buggy
		 * in this regard).
		 */
		bha_inquire_setup_information(sc);
		if (strcmp(sc->sc_firmware, "3.37") < 0)
		    printf("%s: buggy VLB controller, disabling 32-bit DMA\n",
		        sc->sc_dev.dv_xname);
		else
			sc->sc_dmaflags = ISABUS_DMA_32BIT;
	}

	sc->sc_ih = isa_intr_establish(ic, bpd.sc_irq, IST_EDGE, IPL_BIO,
	    bha_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	bha_attach(sc, &bpd);

#if NAHA > 0
	/* XXXX To avoid conflicting with the aha1542 probe */
	btports[nbtports++] = ia->ia_iobase;
#endif
}
