/*	$OpenBSD: dz_fwio.c,v 1.2 2008/08/24 14:49:35 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/sid.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>
#include <vax/mbus/fwiovar.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>

#include <vax/dec/dzkbdvar.h>

#include <sys/tty.h>
#include <dev/cons.h>

#include "dzkbd.h"
#include "dzms.h"

int	dz_fwio_match(struct device *, void *, void *);
void	dz_fwio_attach(struct device *, struct device *, void *);

struct cfattach dz_fwio_ca = {
	sizeof(struct dz_softc), dz_fwio_match, dz_fwio_attach
};

extern struct cfdriver dz_cd;

int	dz_fwio_intr(void *);

#define	DZ_FWIO_CSR	0
#define	DZ_FWIO_RBUF	4
#define	DZ_FWIO_DTR	9
#define	DZ_FWIO_BREAK	13
#define	DZ_FWIO_TBUF	12
#define	DZ_FWIO_TCR	8
#define	DZ_FWIO_DCD	13
#define	DZ_FWIO_RING	13

int
dz_fwio_match(struct device *parent, void *vcf, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;

	return strcmp(faa->faa_dev, dz_cd.cd_name) == 0 ? 1 : 0;
}

void
dz_fwio_attach(struct device *parent, struct device *self, void *aux)
{
	struct fwio_attach_args *faa = (struct fwio_attach_args *)aux;
	struct dz_softc *sc = (struct dz_softc *)self;
	paddr_t basepa;
#if NDZKBD > 0 || NDZMS > 0
	struct dzkm_attach_args daa;
	extern struct consdev wsdisplay_cons;
#endif
	extern vaddr_t dz_console_regs;
	vaddr_t dz_regs;
	unsigned int vec;
	int console;
	int serial_console;

	vec = faa->faa_vecbase + FBIC_DEVIRQ2 * 4;
	printf(" vec %d: ", vec);

	/*
	 * Map registers.
	 */

	if (dz_console_regs != 0 && faa->faa_mid == mbus_ioslot) {
		dz_regs = dz_console_regs;
		console = 1;
		serial_console = (vax_confdata & 0x60) == 0;
		if (serial_console)
			printf("console, ");
	} else {
		basepa = faa->faa_base + FWIO_DZ_REG_OFFSET;
		dz_regs = vax_map_physmem(basepa, 1);
		console = 0;
	}

	/* 
	 * XXX - This is evil and ugly, but...
	 * due to the nature of how bus_space_* works on VAX, this will
	 * be perfectly good until everything is converted.
	 */
	sc->sc_ioh = dz_regs;

	sc->sc_dr.dr_csr = DZ_FWIO_CSR;
	sc->sc_dr.dr_rbuf = DZ_FWIO_RBUF;
	sc->sc_dr.dr_tbuf = DZ_FWIO_TBUF;
	sc->sc_dr.dr_tcr = DZ_FWIO_TCR;
	sc->sc_dr.dr_dtr = DZ_FWIO_DTR;
	sc->sc_dr.dr_break = DZ_FWIO_BREAK;
	sc->sc_dr.dr_dcd = DZ_FWIO_DCD;
	sc->sc_dr.dr_ring = DZ_FWIO_RING;

	sc->sc_type = DZ_DZV;

	/* no modem control bits except on line 2 */
	sc->sc_dsr = (1 << 0) | (1 << 1) | (1 << 3);

	printf("4 lines");

	/*
	 * Complete attachment.
	 */

	dzattach(sc);

	/*
	 * Register interrupt handler.
	 */

	if (mbus_intr_establish(vec, IPL_TTY, dz_fwio_intr, sc,
	    self->dv_xname) != 0) {
		printf("\n%s: can't establish interrupt\n", self->dv_xname);
		return;
	}
	
	/*
	 * Attach input devices, if any.
	 */

#if NDZKBD > 0
	daa.daa_line = 0;
	DZ_WRITE_WORD(sc, dr_rbuf, DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) |
	    DZ_LPR_8_BIT_CHAR | daa.daa_line);
	daa.daa_flags =
	    (console && cn_tab == &wsdisplay_cons ? DZKBD_CONSOLE : 0);
	config_found(self, &daa, dz_print);
#endif
#if NDZMS > 0
	daa.daa_line = 1;
	DZ_WRITE_WORD(sc, dr_rbuf, DZ_LPR_RX_ENABLE | (DZ_LPR_B4800 << 8) |
	    DZ_LPR_8_BIT_CHAR | DZ_LPR_PARENB | DZ_LPR_OPAR | daa.daa_line);
	daa.daa_flags = 0;
	config_found(self, &daa, dz_print);
#endif
}

int
dz_fwio_intr(void *v)
{
	struct dz_softc *sc = (struct dz_softc *)v;

	/*
	 * FBIC expects edge interrupts, while the dz does level
	 * interrupts. To avoid missing interrupts while servicing,
	 * we disable further device interrupts while servicing.
	 */
	DZ_WRITE_WORD(sc, dr_csr,
	    DZ_READ_WORD(sc, dr_csr) & ~(DZ_CSR_RXIE | DZ_CSR_TXIE));

	dzrint(sc);
	dzxint(sc);

	DZ_WRITE_WORD(sc, dr_csr,
	    DZ_READ_WORD(sc, dr_csr) | (DZ_CSR_RXIE | DZ_CSR_TXIE));

	return 1;
}
