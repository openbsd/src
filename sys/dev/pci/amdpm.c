/*	$OpenBSD: amdpm.c,v 1.3 2002/11/04 17:12:34 fgsch Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/rndvar.h>
#include <dev/pci/amdpmreg.h>

struct amdpm_softc {
	struct device sc_dev;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_tag;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;		/* PMxx space */

	struct timeout sc_rnd_ch;
#ifdef AMDPM_RND_COUNTERS
	struct evcnt sc_rnd_hits;
	struct evcnt sc_rnd_miss;
	struct evcnt sc_rnd_data[256];
#endif
};

int	amdpm_match(struct device *, void *, void *);
void	amdpm_attach(struct device *, struct device *, void *);
void	amdpm_rnd_callout(void *);

struct cfattach amdpm_ca = {
	sizeof(struct amdpm_softc), amdpm_match, amdpm_attach
};

struct cfdriver amdpm_cd = {
	NULL, "amdpm", DV_DULL
};

#ifdef AMDPM_RND_COUNTERS
#define	AMDPM_RNDCNT_INCR(ev)	(ev)->ev_count++
#else
#define	AMDPM_RNDCNT_INCR(ev)	/* nothing */
#endif

int
amdpm_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_PBC768_PMC)
		return (1);
	return (0);
}

void
amdpm_attach(struct device *parent, struct device *self, void *aux)
{
	struct amdpm_softc *sc = (struct amdpm_softc *) self;
	struct pci_attach_args *pa = aux;
	struct timeval tv1, tv2;
	pcireg_t reg;
	int i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_iot = pa->pa_iot;

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG);
	if ((reg & AMDPM_PMIOEN) == 0) {
		printf(": PMxx space isn't enabled\n");
		return;
	}
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_PMPTR);
	if (bus_space_map(sc->sc_iot, AMDPM_PMBASE(reg), AMDPM_PMSIZE,
	    0, &sc->sc_ioh)) {
		printf(": failed to map PMxx space\n");
		return;
	}

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, AMDPM_CONFREG);
	if (reg & AMDPM_RNGEN) {
		/* Check to see if we can read data from the RNG. */
		(void) bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    AMDPM_RNGDATA);
		/* benchmark the RNG */
		microtime(&tv1);
		for (i = 2 * 1024; i--; ) {
			while(!(bus_space_read_1(sc->sc_iot, sc->sc_ioh,
			    AMDPM_RNGSTAT) & AMDPM_RNGDONE))
				;
			(void) bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    AMDPM_RNGDATA);
		}
		microtime(&tv2);

		timersub(&tv2, &tv1, &tv1);
		if (tv1.tv_sec)
			tv1.tv_usec += 1000000 * tv1.tv_sec;
		printf(": rng active, %dKb/sec", 8 * 1000000 / tv1.tv_usec);

#ifdef AMDPM_RND_COUNTERS
			evcnt_attach_dynamic(&sc->sc_rnd_hits, EVCNT_TYPE_MISC,
			    NULL, sc->sc_dev.dv_xname, "rnd hits");
			evcnt_attach_dynamic(&sc->sc_rnd_miss, EVCNT_TYPE_MISC,
			    NULL, sc->sc_dev.dv_xname, "rnd miss");
			for (i = 0; i < 256; i++) {
				evcnt_attach_dynamic(&sc->sc_rnd_data[i],
				    EVCNT_TYPE_MISC, NULL, sc->sc_dev.dv_xname,
				    "rnd data");
			}
#endif
		timeout_set(&sc->sc_rnd_ch, amdpm_rnd_callout, sc);
		amdpm_rnd_callout(sc);
	}
}

void
amdpm_rnd_callout(void *v)
{
	struct amdpm_softc *sc = v;
	u_int32_t reg;
#ifdef AMDPM_RND_COUNTERS
	int i;
#endif

	if ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_RNGSTAT) &
	    AMDPM_RNGDONE) != 0) {
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, AMDPM_RNGDATA);
		add_true_randomness(reg);
#ifdef AMDPM_RND_COUNTERS
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_hits);
		for (i = 0; i < sizeof(reg); i++, reg >>= NBBY)
			AMDPM_RNDCNT_INCR(&sc->sc_rnd_data[reg & 0xff]);
#endif
	} else
		AMDPM_RNDCNT_INCR(&sc->sc_rnd_miss);
	timeout_add(&sc->sc_rnd_ch, 1);
}
