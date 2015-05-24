/*	$OpenBSD: pckbc_hpc.c,v 1.4 2015/05/24 10:57:47 miod Exp $	*/
/* $NetBSD: pckbc_hpc.c,v 1.9 2008/03/15 13:23:24 cube Exp $	 */

/*
 * Copyright (c) 2003 Christopher SEKIYA
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
 *          This product includes software developed for the
 *          NetBSD Project.  See http://www.NetBSD.org/ for
 *          information about NetBSD.
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <sgi/hpc/hpcreg.h>
#include <sgi/hpc/hpcvar.h>
#include <sgi/sgi/ip22.h>

#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>

int      pckbc_hpc_match(struct device *, void *, void *);
void     pckbc_hpc_attach(struct device *, struct device *, void *);

const struct cfattach pckbc_hpc_ca = {
	sizeof(struct pckbc_softc), pckbc_hpc_match, pckbc_hpc_attach
};

int
pckbc_hpc_match(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct hpc_attach_args *ha = aux;

	/* keyboard controller is not wired on Challenge S */
	if (sys_config.system_subtype == IP22_CHALLS)
		return 0;

	if (strcmp(ha->ha_name, cf->cf_driver->cd_name) == 0)
		return 1;

	return 0;
}

void
pckbc_hpc_attach(struct device *parent, struct device * self, void *aux)
{
	struct pckbc_softc *sc = (struct pckbc_softc *)self;
	struct hpc_attach_args *haa = aux;
	struct pckbc_internal *t = NULL;
	bus_space_handle_t ioh_d, ioh_c;
	int console;

	if (hpc_intr_establish(haa->ha_irq, IPL_TTY, pckbcintr, sc,
	    sc->sc_dv.dv_xname) == NULL) {
		printf(": unable to establish interrupt\n");
		return;
	}

	console = pckbc_is_console(haa->ha_st,
	    XKPHYS_TO_PHYS(haa->ha_sh + haa->ha_devoff + 3));

	if (console) {
		/* pckbc_cnattach() has already been called */
		t = &pckbc_consdata;
		pckbc_console_attached = 1;
	} else {
		if (bus_space_subregion(haa->ha_st, haa->ha_sh,
		    haa->ha_devoff + 3 + KBDATAP, 1, &ioh_d) ||
		    bus_space_subregion(haa->ha_st, haa->ha_sh,
		    haa->ha_devoff + 3 + KBCMDP, 1, &ioh_c)) {
			printf(": couldn't map registers\n");
			return;
		}

		t = malloc(sizeof(*t), M_DEVBUF, M_NOWAIT | M_ZERO);
		t->t_iot = haa->ha_st;
		t->t_ioh_c = ioh_c;
		t->t_ioh_d = ioh_d;
	}

	t->t_cmdbyte = KC8_CPU;
	t->t_sc = sc;
	sc->id = t;

	printf("\n");
	pckbc_attach(sc, 0);
}
