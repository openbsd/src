/*	$OpenBSD: xbox.c,v 1.1 1999/04/18 03:24:26 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 * Driver for the Sun SBus Expansion Subsystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>	/* for SBUS_BURST_* */

#include <sparc/dev/xboxreg.h>
#include <sparc/dev/xboxvar.h>

int	xboxmatch	__P((struct device *, void *, void *));
void	xboxattach	__P((struct device *, struct device *, void *));
int	xboxprint	__P((void *, const char *));
void	xbox_fix_range	__P((struct xbox_softc *sc, struct sbus_softc *sbp));

struct cfattach xbox_ca = {
	sizeof (struct xbox_softc), xboxmatch, xboxattach
};

struct cfdriver xbox_cd = {
	NULL, "xbox", DV_IFNET
};

int
xboxmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp("SUNW,xbox", ra->ra_name))
		return (0);

	if (!sbus_testdma((struct sbus_softc *)parent, ca))
		return(0);
	return (1);
}

void    
xboxattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct xbox_softc *sc = (struct xbox_softc *)self;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	int node = ca->ca_ra.ra_node;
	struct confargs oca;
	char *s;

	s = getpropstring(node, "model");
	printf(": model %s", s);

	s = getpropstring(node, "child-present");
	if (strcmp(s, "false") == 0) {
		printf(": no devices\n");
		return;
	}

	sc->sc_regs.xa_write0 = mapiodev(&ca->ca_ra.ra_reg[0], 0,
	    sizeof(*sc->sc_regs.xa_write0));
	sc->sc_regs.xa_errs = mapiodev(&ca->ca_ra.ra_reg[1], 0,
	    sizeof(*sc->sc_regs.xa_errs));
	sc->sc_regs.xa_ctl0 = mapiodev(&ca->ca_ra.ra_reg[2], 0,
	    sizeof(*sc->sc_regs.xa_ctl0));
	sc->sc_regs.xa_ctl1 = mapiodev(&ca->ca_ra.ra_reg[3], 0,
	    sizeof(*sc->sc_regs.xa_ctl1));
	sc->sc_regs.xa_elua = mapiodev(&ca->ca_ra.ra_reg[4], 0,
	    sizeof(*sc->sc_regs.xa_elua));
	sc->sc_regs.xa_ella = mapiodev(&ca->ca_ra.ra_reg[5], 0,
	    sizeof(*sc->sc_regs.xa_ella));
	sc->sc_regs.xa_rsrv = mapiodev(&ca->ca_ra.ra_reg[6], 0,
	    sizeof(*sc->sc_regs.xa_rsrv));
	sc->sc_regs.xb_errs = mapiodev(&ca->ca_ra.ra_reg[7], 0,
	    sizeof(*sc->sc_regs.xb_errs));
	sc->sc_regs.xb_ctl0 = mapiodev(&ca->ca_ra.ra_reg[8], 0,
	    sizeof(*sc->sc_regs.xb_ctl0));
	sc->sc_regs.xb_ctl1 = mapiodev(&ca->ca_ra.ra_reg[9], 0,
	    sizeof(*sc->sc_regs.xb_ctl1));
	sc->sc_regs.xb_elua = mapiodev(&ca->ca_ra.ra_reg[10], 0,
	    sizeof(*sc->sc_regs.xb_elua));
	sc->sc_regs.xb_ella = mapiodev(&ca->ca_ra.ra_reg[11], 0,
	    sizeof(*sc->sc_regs.xb_ella));
	sc->sc_regs.xb_rsrv = mapiodev(&ca->ca_ra.ra_reg[12], 0,
	    sizeof(*sc->sc_regs.xb_rsrv));

	if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "SUNW,xbox") == 0)
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	sc->sc_key = getpropint(node, "write0-key", -1);
	sc->sc_node = node;

	*sc->sc_regs.xa_write0 = (sc->sc_key << 24) | XAC_CTL1_OFFSET |
	    XBOX_CTL1_CSIE | XBOX_CTL1_TRANSPARENT;
	*sc->sc_regs.xa_write0 = (sc->sc_key << 24) | XBC_CTL1_OFFSET |
	    XBOX_CTL1_XSIE | XBOX_CTL1_XSBRE | XBOX_CTL1_XSSE;
	DELAY(100);

	xbox_fix_range(sc, (struct sbus_softc *)parent);

	sbus_establish(&sc->sc_sd, &sc->sc_dv);

	printf("\n");

	oca = (*ca);
	oca.ca_bustype = BUS_XBOX;
	if (ca->ca_ra.ra_bp != NULL)
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	(void)config_found(&sc->sc_dv, (void *)&oca, xboxprint);
}

/*
 * Fix up our address ranges based on parent address spaces.
 */
void
xbox_fix_range(sc, sbp)
	struct xbox_softc *sc;
	struct sbus_softc *sbp;
{
	int rlen, i, j;

	rlen = getproplen(sc->sc_node, "ranges");
	sc->sc_range =
		(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
	if (sc->sc_range == NULL) {
		printf("%s: PROM ranges too large: %d\n",
				sc->sc_dv.dv_xname, rlen);
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

int
xboxprint(args, sbus)
	void *args;
	const char *sbus;
{
	struct confargs *ca = args;

	if (sbus)
		printf("%s at %s", ca->ca_ra.ra_name, sbus);
	return (UNCONF);
}
