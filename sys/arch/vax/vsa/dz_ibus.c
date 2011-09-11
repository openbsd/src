/*	$OpenBSD: dz_ibus.c,v 1.29 2011/09/11 19:29:01 miod Exp $	*/
/*	$NetBSD: dz_ibus.c,v 1.15 1999/08/27 17:50:42 ragge Exp $ */
/*
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of 
 *     Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/sid.h>
#include <machine/vsbus.h>
#include <machine/cpu.h>
#include <machine/scb.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

#include <dev/cons.h>

#include "dzkbd.h"
#include "dzms.h"

int     dz_vsbus_match(struct device *, struct cfdata *, void *);
void    dz_vsbus_attach(struct device *, struct device *, void *);

struct  cfattach dz_vsbus_ca = {
	sizeof(struct dz_softc), (cfmatch_t)dz_vsbus_match, dz_vsbus_attach
};

#define	DZ_VSBUS_CSR	0
#define	DZ_VSBUS_RBUF	4
#define	DZ_VSBUS_DTR	9
#define	DZ_VSBUS_BREAK	13
#define	DZ_VSBUS_TBUF	12
#define	DZ_VSBUS_TCR	8
#define	DZ_VSBUS_DCD	13
#define	DZ_VSBUS_RING	13

int
dz_vsbus_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct vsbus_attach_args *va = aux;
	volatile uint16_t *dzP;
	short i;

#if VAX53 || VAX49
	if (vax_boardtype == VAX_BTYP_49 ||
	    vax_boardtype == VAX_BTYP_1303)
		if (cf->cf_loc[0] != DZ_CSR_KA49)
			return 0; /* don't probe unnecessarily */
#endif

	dzP = (volatile uint16_t *)va->va_addr;
	i = dzP[DZ_VSBUS_TCR / 2];
	dzP[DZ_VSBUS_CSR / 2] = DZ_CSR_MSE|DZ_CSR_TXIE;
	dzP[DZ_VSBUS_TCR / 2] = 0;
	DELAY(1000);
	dzP[DZ_VSBUS_TCR / 2] = 1;
	DELAY(100000);
	dzP[DZ_VSBUS_TCR / 2] = i;

	/* If the device doesn't exist, no interrupt has been generated */
	
	return 1;
}

void
dz_vsbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dz_softc *sc = (void *)self;
	struct vsbus_attach_args *va = aux;
#if NDZKBD > 0 || NDZMS > 0
	struct dzkm_attach_args daa;
#endif
	extern vaddr_t dz_console_regs;
	vaddr_t dz_regs;

	printf(": ");

	/*
	 * This assumes that systems where dz@vsbus exist and can be
	 * the console device, can only have one instance of dz@vsbus.
	 * So far, so good.
	 */
	if (dz_console_regs != 0) {
		dz_regs = dz_console_regs;
		printf("console, ");
	} else
		dz_regs = vax_map_physmem(va->va_paddr, 1);

	/* 
	 * XXX - This is evil and ugly, but...
	 * due to the nature of how bus_space_* works on VAX, this will
	 * be perfectly good until everything is converted.
	 */
	sc->sc_ioh = dz_regs;
	sc->sc_dr.dr_csr = DZ_VSBUS_CSR;
	sc->sc_dr.dr_rbuf = DZ_VSBUS_RBUF;
	sc->sc_dr.dr_dtr = DZ_VSBUS_DTR;
	sc->sc_dr.dr_break = DZ_VSBUS_BREAK;
	sc->sc_dr.dr_tbuf = DZ_VSBUS_TBUF;
	sc->sc_dr.dr_tcr = DZ_VSBUS_TCR;
	sc->sc_dr.dr_dcd = DZ_VSBUS_DCD;
	sc->sc_dr.dr_ring = DZ_VSBUS_RING;

	sc->sc_type = DZ_DZV;

	/* no modem control bits except on line 2 */
	sc->sc_dsr = (1 << 0) | (1 << 1) | (1 << 3);

	sc->sc_rcvec = va->va_cvec;
	scb_vecalloc(sc->sc_rcvec, dzxint, sc, SCB_ISTACK, &sc->sc_tintrcnt);
	sc->sc_tcvec = va->va_cvec - 4;
	scb_vecalloc(sc->sc_tcvec, dzrint, sc, SCB_ISTACK, &sc->sc_rintrcnt);
	evcount_attach(&sc->sc_rintrcnt, sc->sc_dev.dv_xname, &sc->sc_rcvec);
	evcount_attach(&sc->sc_tintrcnt, sc->sc_dev.dv_xname, &sc->sc_tcvec);

	printf("4 lines");

	dzattach(sc);

	if (dz_can_have_kbd()) {
#if NDZKBD > 0
		extern struct consdev wsdisplay_cons;

		daa.daa_line = 0;
		DZ_WRITE_WORD(sc, dr_rbuf, DZ_LPR_RX_ENABLE |
		    (DZ_LPR_B4800 << 8) | DZ_LPR_8_BIT_CHAR | daa.daa_line);
		daa.daa_flags =
		    (cn_tab == &wsdisplay_cons ? DZKBD_CONSOLE : 0);
		config_found(self, &daa, dz_print);
#endif
#if NDZMS > 0
		daa.daa_line = 1;
		DZ_WRITE_WORD(sc, dr_rbuf, DZ_LPR_RX_ENABLE |
		    (DZ_LPR_B4800 << 8) | DZ_LPR_8_BIT_CHAR | DZ_LPR_PARENB |
		    DZ_LPR_OPAR | daa.daa_line);
		daa.daa_flags = 0;
		config_found(self, &daa, dz_print);
#endif
	}

#if 0
	s = spltty();
	dzrint(sc);
	dzxint(sc);
	splx(s);
#endif
}
