/*	$OpenBSD: qec.c,v 1.7 1998/10/21 04:12:10 jason Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt and Jason L. Wright.
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <sparc/autoconf.h>
#include <sparc/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/qecreg.h>
#include <sparc/dev/qecvar.h>

int	qecprint	__P((void *, const char *));
void	qecattach	__P((struct device *, struct device *, void *));
void	qec_fix_range	__P((struct qec_softc *, struct sbus_softc *));
void	qec_translate	__P((struct qec_softc *, struct confargs *));

struct cfattach qec_ca = {
	sizeof(struct qec_softc), matchbyname, qecattach
};

struct cfdriver qec_cd = {
	NULL, "qec", DV_DULL
};

int
qecprint(aux, name)
	void *aux;
	const char *name;
{
	register struct confargs *ca = aux;

	if (name)
		printf("%s at %s", ca->ca_ra.ra_name, name);
	printf(" offset 0x%x", ca->ca_offset);
	return (UNCONF);
}

/*
 * Attach all the sub-devices we can find
 */
void
qecattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	struct qec_softc *sc = (void *)self;
	int node;
	struct confargs oca;
	char *name;
	int sbusburst;

	/*
	 * The first IO space is the QEC registers, the second IO
	 * space is the QEC (64K we hope) ram buffer
	 */
	sc->sc_regs = mapiodev(&ca->ca_ra.ra_reg[0], 0,
	    sizeof(struct qecregs));
	sc->sc_buffer = mapiodev(&ca->ca_ra.ra_reg[1], 0,
	    ca->ca_ra.ra_reg[1].rr_len);
	sc->sc_bufsiz = ca->ca_ra.ra_reg[1].rr_len;

	/*
	 * On qec+qe, the qec has the interrupt priority, but we
	 * need to pass that down so that the qe's can handle them.
	 */
	if (ca->ca_ra.ra_nintr == 1)
		sc->sc_pri = ca->ca_ra.ra_intr[0].int_pri;

	node = sc->sc_node = ca->ca_ra.ra_node;

	qec_fix_range(sc, (struct sbus_softc *)parent);

	/*
	 * Get transfer burst size from PROM
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	sc->sc_nchannels = getpropint(ca->ca_ra.ra_node, "#channels", -1);
	if (sc->sc_nchannels == -1) {
		printf(": no channels\n");
		return;
	}
	else if (sc->sc_nchannels < 1 || sc->sc_nchannels > 4) {
		printf(": invalid number of channels: %d\n", sc->sc_nchannels);
		return;
	}

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		/* take SBus burst sizes */
		sc->sc_burst = sbusburst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= sbusburst;

	printf(": %dK memory %d %s",
	    sc->sc_bufsiz / 1024, sc->sc_nchannels,
	    (sc->sc_nchannels == 1) ? "channel" : "channels");

	node = sc->sc_node = ca->ca_ra.ra_node;

	if (ca->ca_bustype == BUS_SBUS)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);

	/* Propagate bootpath */
	if (ca->ca_ra.ra_bp != NULL)
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	printf("\n");

	qec_reset(sc);

	/* search through children */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		qec_translate(sc, &oca);
		oca.ca_bustype = BUS_SBUS;
		(void) config_found(&sc->sc_dev, (void *)&oca, qecprint);
	}
}

void
qec_fix_range(sc, sbp)
	struct qec_softc *sc;
	struct sbus_softc *sbp;
{
	int rlen, i, j;

	rlen = getproplen(sc->sc_node, "ranges");
	sc->sc_range =
		(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_range == NULL) {
		printf("%s: PROM ranges too large: %d\n",
				sc->sc_dev.dv_xname, rlen);
		return;
	}
	sc->sc_nrange = rlen / sizeof(struct rom_range);
	(void)getprop(sc->sc_node, "ranges", sc->sc_range, rlen);

	for (i = 0; i < sc->sc_nrange; i++) {
		for (j = 0; j < sbp->sc_nrange; j++) {
			if (sc->sc_range[i].pspace == sbp->sc_range[j].cspace) {
				sc->sc_range[i].poffset +=
				    sbp->sc_range[j].poffset;
				sc->sc_range[i].pspace =
				    sbp->sc_range[j].pspace;
				break;
			}
		}
	}
}

/*
 * Translate the register addresses of our children 
 */
void
qec_translate(sc, ca)
	struct qec_softc *sc;
	struct confargs *ca;
{
	register int i;

	ca->ca_slot = ca->ca_ra.ra_iospace;
	ca->ca_offset = (int)ca->ca_ra.ra_paddr;

	/* Translate into parent address spaces */
	for (i = 0; i < ca->ca_ra.ra_nreg; i++) {
		int j, cspace = ca->ca_ra.ra_reg[i].rr_iospace;

		for (j = 0; j < sc->sc_nrange; j++) {
			if (sc->sc_range[j].cspace == cspace) {
				(int)ca->ca_ra.ra_reg[i].rr_paddr +=
					sc->sc_range[j].poffset;
				(int)ca->ca_ra.ra_reg[i].rr_iospace =
					sc->sc_range[j].pspace;
				break;
			}
		}
	}
}

/*
 * Reset the QEC
 */
void
qec_reset(sc)
	struct qec_softc *sc;
{
	int i = 200;

	sc->sc_regs->ctrl = QEC_CTRL_RESET;
	while (--i) {
		if ((sc->sc_regs->ctrl & QEC_CTRL_RESET) == 0)
			break;
		DELAY(20);
	}
	if (i == 0) {
		printf("%s: reset failed.\n", sc->sc_dev.dv_xname);
		return;
	}
}
