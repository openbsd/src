/*	$OpenBSD: pfour.c,v 1.7 2001/11/06 19:53:16 miod Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/malloc.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/dev/pfourreg.h>

struct pfour_softc {
	struct	device sc_dev;		/* base device */
	volatile u_long *sc_vaddr;	/* pfour register */
	int	nothing;
};

static int	pfourmatch __P((struct device *, void *, void *));
static void	pfourattach __P((struct device *, struct device *, void *));
struct cfdriver pfourcd = { NULL, "pfour", pfourmatch, pfourattach,
	DV_DULL, sizeof(struct pfour_softc)
};

int
pfourmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

void
pfourattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct pfour_softc *sc = (struct pfour_softc *)self;
	extern struct cfdata cfdata[];
	register struct confargs *ca = args;
	struct confargs oca;
	register short *p;
	struct cfdata *cf;
	u_long val;

	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}

	sc->sc_vaddr = (u_long *)mapiodev(ca->ca_ra.ra_reg, PFOUR_REG,
	    NBPG, ca->ca_bustype);
	if (sc->sc_vaddr == NULL) {
		printf("\n");
		return;
	}
	val = probeget(sc->sc_vaddr, 4);

	if (val == -1) {
		printf(": empty\n");
		return;
	}

	printf(": cardtype 0x%02x\n", PFOUR_FBTYPE(val));

	*sc->sc_vaddr = PFOUR_REG_VIDEO | PFOUR_REG_RESET;
	*sc->sc_vaddr = PFOUR_REG_VIDEO;

	for (cf = cfdata; cf->cf_driver; cf++) {
		if (cf->cf_fstate == FSTATE_FOUND)
			continue;
		for (p = cf->cf_parents; *p >= 0; p++)
			if (self->dv_cfdata == &cfdata[*p]) {
				oca.ca_ra.ra_iospace = -1;
				oca.ca_ra.ra_len = 0;
				oca.ca_ra.ra_nreg = 1;
				oca.ca_ra.ra_pfour = val;
				oca.ca_ra.ra_intr[0].int_vec = -1;
				oca.ca_ra.ra_nintr = 0;
				oca.ca_ra.ra_name = cf->cf_driver->cd_name;
				oca.ca_ra.ra_paddr = ca->ca_ra.ra_paddr;
				oca.ca_bustype = BUS_PFOUR;

				if ((*cf->cf_driver->cd_match)(self, cf, &oca) == 0)
					continue;
				config_attach(self, cf, &oca, NULL);
			}
	}
}

void
pfour_reset()
{
	struct pfour_softc *sc = pfourcd.cd_devs[0];

	*sc->sc_vaddr = PFOUR_REG_VIDEO | PFOUR_REG_RESET;
	delay(1);
	*sc->sc_vaddr = PFOUR_REG_VIDEO;
}

int
pfour_videosize(reg, xp, yp)
	int reg;
	int *xp, *yp;
{
	if (PFOUR_ID(reg) == PFOUR_ID_COLOR24) {
		*xp = 1152;
		*yp = 900;
		return 0;
	}

	switch (PFOUR_SIZE(reg)) {
	case PFOUR_SIZE_1152X900:
		*xp = 1152;
		*yp = 900;
		break;
	case PFOUR_SIZE_1024X1024:
		*xp = 1024;
		*yp = 1024;
		break;
	case PFOUR_SIZE_1280X1024:
		*xp = 1280;
		*yp = 1024;
		break;
	case PFOUR_SIZE_1600X1280:
		*xp = 1600;
		*yp = 1280;
		break;
	case PFOUR_SIZE_1440X1440:
		*xp = 1440;
		*yp = 1440;
		break;
	case PFOUR_SIZE_640X480:
		*xp = 640;
		*yp = 480;
		break;
	default:
		*xp = 1152;		/* assume, but indicate error */
		*yp = 900;
		return (-1);
	}
	return (0);
}

void
pfourenable(on)
	int on;
{
	struct pfour_softc *sc = pfourcd.cd_devs[0];

	if (on)
		*sc->sc_vaddr |= PFOUR_REG_VIDEO;
	else
		*sc->sc_vaddr &= ~PFOUR_REG_VIDEO;
}

int
pfourstatus()
{
	struct pfour_softc *sc = pfourcd.cd_devs[0];

	return (*sc->sc_vaddr & PFOUR_REG_VIDEO);
}
