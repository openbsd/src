/*	$OpenBSD: sbus.c,v 1.20 2015/03/21 19:55:31 miod Exp $	*/
/*	$NetBSD: sbus.c,v 1.17 1997/06/01 22:10:39 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sbus.c	8.1 (Berkeley) 6/11/93
 */

/*
 * SBus stuff.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>

#include <sparc/dev/sbusreg.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/xboxreg.h>
#include <sparc/dev/xboxvar.h>
#include <sparc/dev/dmareg.h>

int sbus_print(void *, const char *);

/* autoconfiguration driver */
void	sbus_attach(struct device *, struct device *, void *);
int	sbus_match(struct device *, void *, void *);
int	sbus_search(struct device *, void *, void *);

struct cfattach sbus_ca = {
	sizeof(struct sbus_softc), sbus_match, sbus_attach
};

struct cfdriver sbus_cd = {
	NULL, "sbus", DV_DULL
};

/*
 * Print the location of some sbus-attached device (called just
 * before attaching that device).  If `sbus' is not NULL, the
 * device was found but not configured; print the sbus as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
sbus_print(args, sbus)
	void *args;
	const char *sbus;
{
	struct confargs *ca = args;
	char *class;
	static char *sl = "slave-only";

	if (sbus != NULL) {
		printf("\"%s\" at %s", ca->ca_ra.ra_name, sbus);
		class = getpropstring(ca->ca_ra.ra_node, "device_type");
		if (*class != '\0')
			printf(" class %s", class);
	}
	/* Check root node for 'slave-only' property */
	if (getpropint(0, sl, 0) & (1 << ca->ca_slot))
		printf(" %s", sl);
	printf(" slot %d offset 0x%x", ca->ca_slot, ca->ca_offset);

	return ca->ca_bustype < 0 ? UNSUPP : UNCONF;
}

int
sbus_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4)
		return (0);

	if (ca->ca_bustype == BUS_XBOX) {
		struct xbox_softc *xsc = (struct xbox_softc *)parent;

		/* Prevent multiple attachments */
		if (xsc->sc_attached == 0) {
			xsc->sc_attached = 1;
			return (1);
		}

		return (0);
	}

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach an SBus.
 */
void
sbus_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	register struct sbus_softc *sc = (struct sbus_softc *)self;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;
	register int node;
	register char *name;
	struct confargs oca;
	int rlen;

	/*
	 * XXX there is only one SBus, for now -- do not know how to
	 * address children on others
	 */
	if (sc->sc_dev.dv_unit > 0 && ca->ca_bustype != BUS_XBOX) {
		printf(" unsupported\n");
		return;
	}

	/*
	 * Record clock frequency for synchronous SCSI.
	 * IS THIS THE CORRECT DEFAULT??
	 */
	node = ra->ra_node;
	sc->sc_clockfreq = getpropint(node, "clock-frequency", -1);
	if (sc->sc_clockfreq <= 0)
		sc->sc_clockfreq = getpropint(findroot(), "clock-frequency",
		    25 * 1000 * 1000);
	printf(": %s MHz\n", clockfreq(sc->sc_clockfreq));

	/*
	 * Get the SBus burst transfer size if burst transfers are supported
	 */
	sc->sc_burst = getpropint(node, "burst-sizes", 0);
	sc->sc_burst = sc->sc_burst & ~SBUS_BURST_64;

	if (ca->ca_bustype == BUS_XBOX) {
		struct xbox_softc *xsc = (struct xbox_softc *)parent;

		/* Parent has already done the leg work */
		sc->sc_nrange = xsc->sc_nrange;
		sc->sc_range = xsc->sc_range;
		xsc->sc_attached = 2;
	} else {
		if (ra->ra_bp != NULL && strcmp(ra->ra_bp->name, "sbus") == 0)
			oca.ca_ra.ra_bp = ra->ra_bp + 1;
		else
			oca.ca_ra.ra_bp = NULL;

		rlen = getproplen(node, "ranges");
		if (rlen > 0) {
			sc->sc_nrange = rlen / sizeof(struct rom_range);
			sc->sc_range =
				(struct rom_range *)malloc(rlen, M_DEVBUF, M_NOWAIT);
			if (sc->sc_range == 0)
				panic("sbus: PROM ranges too large: %d", rlen);
			(void)getprop(node, "ranges", sc->sc_range, rlen);
		}
	}

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
#ifdef SUN4E
		if (CPU_ISSUN4E && strcmp(name, "vm") == 0)
			continue;
#endif
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		if (sbus_translate(self, &oca) == 0)
			oca.ca_bustype = BUS_SBUS;
		else
			oca.ca_bustype = -1;	/* force attach to fail */

		config_found_sm(&sc->sc_dev, (void *)&oca, sbus_print,
		    sbus_search);
	}
}

int
sbus_search(struct device *parent, void *vcf, void *args)
{
	struct cfdata *cf = vcf;
	struct confargs *oca = args;

	if (oca->ca_bustype < 0)
		return 0;

	return (cf->cf_attach->ca_match)(parent, cf, oca);
}

int
sbus_translate(dev, ca)
	struct device *dev;
	struct confargs *ca;
{
	struct sbus_softc *sc = (struct sbus_softc *)dev;
	int base, slot;
	int i;

	if (sc->sc_nrange == 0) {
		/* Old-style SBus configuration */
		base = (int)ca->ca_ra.ra_paddr;
		if (SBUS_ABS(base)) {
			ca->ca_slot = SBUS_ABS_TO_SLOT(base);
			ca->ca_offset = SBUS_ABS_TO_OFFSET(base);
		} else {
			if (!CPU_ISSUN4C && !CPU_ISSUN4E) {
				printf("%s: relative sbus addressing not supported\n",
				    dev->dv_xname);
				return ENXIO;
			}
			ca->ca_slot = slot = ca->ca_ra.ra_iospace;
			ca->ca_offset = base;

			/* Fix all register banks */
			for (i = 0; i < ca->ca_ra.ra_nreg; i++) {
				base = (int)ca->ca_ra.ra_reg[i].rr_paddr;
				if ((base & ~SBUS_PAGE_MASK) != 0)
					return ENXIO;
				ca->ca_ra.ra_reg[i].rr_paddr =
					(void *)SBUS_ADDR(slot, base);
				ca->ca_ra.ra_reg[i].rr_iospace = PMAP_OBIO;
			}
		}
	} else {
		ca->ca_slot = ca->ca_ra.ra_iospace;
		ca->ca_offset = (int)ca->ca_ra.ra_paddr;

		/* Translate into parent address spaces */
		for (i = 0; i < ca->ca_ra.ra_nreg; i++) {
			int j, cspace = ca->ca_ra.ra_reg[i].rr_iospace;

			for (j = 0; j < sc->sc_nrange; j++) {
				if (sc->sc_range[j].cspace == cspace) {
					ca->ca_ra.ra_reg[i].rr_paddr +=
						sc->sc_range[j].poffset;
					ca->ca_ra.ra_reg[i].rr_iospace =
						sc->sc_range[j].pspace;
					break;
				}
			}
		}
	}

	return 0;
}

/*
 * Returns true if this sbus slot is capable of dma
 */
int
sbus_testdma(sc, ca)
	struct sbus_softc *sc;
	struct confargs *ca;
{
        struct romaux *ra = &ca->ca_ra;

	/*
	 * XXX how to handle more than one sbus?
	 */

	if (getpropint(0, "slave-only", 0) & (1 << ca->ca_slot)) {
		printf("%s: dma card found in non-dma sbus slot %d"
			": not supported\n", ra->ra_name, ca->ca_slot);
		return (0);
	}

	return (1);
}
