/*	$OpenBSD: sbus.c,v 1.6 1998/11/11 00:26:00 jason Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * Sbus stuff.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <vm/vm.h>

#include <machine/autoconf.h>

#include <sparc/dev/sbusreg.h>
#include <sparc/dev/sbusvar.h>

int sbus_print __P((void *, const char *));
void sbusreset __P((int));

/* autoconfiguration driver */
void	sbus_attach __P((struct device *, struct device *, void *));
int	sbus_match __P((struct device *, void *, void *));

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
	register struct confargs *ca = args;
	static char *sl = "slave-only";

	if (sbus)
		printf("%s at %s", ca->ca_ra.ra_name, sbus);
	/* Check root node for 'slave-only' property */
	if (getpropint(0, sl, 0) & (1 << ca->ca_slot))
		printf(" %s", sl);
	printf(" slot %d offset 0x%x", ca->ca_slot, ca->ca_offset);
	return (UNCONF);
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

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

/*
 * Attach an Sbus.
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
	 * XXX there is only one Sbus, for now -- do not know how to
	 * address children on others
	 */
	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	/*
	 * Record clock frequency for synchronous SCSI.
	 * IS THIS THE CORRECT DEFAULT??
	 */
	node = ra->ra_node;
	sc->sc_clockfreq = getpropint(node, "clock-frequency", 25*1000*1000);
	printf(": clock = %s MHz\n", clockfreq(sc->sc_clockfreq));

	/*
	 * Get the SBus burst transfer size if burst transfers are supported
	 */
	sc->sc_burst = getpropint(node, "burst-sizes", 0);

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

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		sbus_translate(self, &oca);
		oca.ca_bustype = BUS_SBUS;
		(void) config_found(&sc->sc_dev, (void *)&oca, sbus_print);
	}
}

void
sbus_translate(dev, ca)
	struct device *dev;
	struct confargs *ca;
{
	struct sbus_softc *sc = (struct sbus_softc *)dev;
	register int base, slot;
	register int i;

	if (sc->sc_nrange == 0) {
		/* Old-style Sbus configuration */
		base = (int)ca->ca_ra.ra_paddr;
		if (SBUS_ABS(base)) {
			ca->ca_slot = SBUS_ABS_TO_SLOT(base);
			ca->ca_offset = SBUS_ABS_TO_OFFSET(base);
		} else {
			if (!CPU_ISSUN4C)
				panic("relative sbus addressing not supported");
			ca->ca_slot = slot = ca->ca_ra.ra_iospace;
			ca->ca_offset = base;
			ca->ca_ra.ra_paddr = (void *)SBUS_ADDR(slot, base);
			ca->ca_ra.ra_iospace = PMAP_OBIO;

			/* Fix any remaining register banks */
			for (i = 1; i < ca->ca_ra.ra_nreg; i++) {
				base = (int)ca->ca_ra.ra_reg[i].rr_paddr;
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
					(int)ca->ca_ra.ra_reg[i].rr_paddr +=
						sc->sc_range[j].poffset;
					(int)ca->ca_ra.ra_reg[i].rr_iospace =
						sc->sc_range[j].pspace;
					break;
				}
			}
		}
	}
}

/*
 * Each attached device calls sbus_establish after it initializes
 * its sbusdev portion.
 */
void
sbus_establish(sd, dev)
	register struct sbusdev *sd;
	register struct device *dev;
{
	register struct sbus_softc *sc;
	register struct device *curdev;

	/*
	 * We have to look for the sbus by name, since it is not necessarily
	 * our immediate parent (i.e. sun4m /iommu/sbus/espdma/esp)
	 * We don't just use the device structure of the above-attached
	 * sbus, since we might (in the future) support multiple sbus's.
	 */
	for (curdev = dev->dv_parent; ; curdev = curdev->dv_parent) {
		if (!curdev || !curdev->dv_xname)
			panic("sbus_establish: can't find sbus parent for %s",
			      sd->sd_dev->dv_xname
					? sd->sd_dev->dv_xname
					: "<unknown>" );

		if (strncmp(curdev->dv_xname, "sbus", 4) == 0)
			break;
	}
	sc = (struct sbus_softc *) curdev;

	sd->sd_dev = dev;
	sd->sd_bchain = sc->sc_sbdev;
	sc->sc_sbdev = sd;
}

/*
 * Reset the given sbus. (???)
 */
void
sbusreset(sbus)
	int sbus;
{
	register struct sbusdev *sd;
	struct sbus_softc *sc = sbus_cd.cd_devs[sbus];
	struct device *dev;

	printf("reset %s:", sc->sc_dev.dv_xname);
	for (sd = sc->sc_sbdev; sd != NULL; sd = sd->sd_bchain) {
		if (sd->sd_reset) {
			dev = sd->sd_dev;
			(*sd->sd_reset)(dev);
			printf(" %s", dev->dv_xname);
		}
	}
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
	 * XXX how to handle more then one sbus?
	 */

	if (getpropint(0, "slave-only", 0) & (1 << ca->ca_slot)) {
		printf("%s: dma card found in non-dma sbus slot %d"
			": not supported\n", ra->ra_name, ca->ca_slot);
		return (0);
	}

	return (1);
}
