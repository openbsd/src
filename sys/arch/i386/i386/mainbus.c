/*	$OpenBSD: mainbus.c,v 1.15 2002/03/14 01:26:32 millert Exp $	*/
/*	$NetBSD: mainbus.c,v 1.21 1997/06/06 23:14:20 thorpej Exp $	*/

/*
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>		/* for ISA_HOLE_VADDR */
#include <i386/isa/isa_machdep.h>

#include "pci.h"
#include "eisa.h"
#include "isa.h"
#include "apm.h"
#include "bios.h"

#if NBIOS > 0
#include <machine/biosvar.h>
#endif

int	mainbus_match(struct device *, void *, void *);
void	mainbus_attach(struct device *, struct device *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int	mainbus_print(void *, const char *);

union mainbus_attach_args {
	const char *mba_busname;		/* first elem of all */
	struct pcibus_attach_args mba_pba;
	struct eisabus_attach_args mba_eba;
	struct isabus_attach_args mba_iba;
#if NBIOS > 0
	struct bios_attach_args mba_bios;
#endif
};

/*
 * This is set when the ISA bus is attached.  If it's not set by the
 * time it's checked below, then mainbus attempts to attach an ISA.
 */
int     isa_has_been_seen;

/*
 * Probe for the mainbus; always succeeds.
 */
int
mainbus_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	return 1;
}

/*
 * Attach the mainbus.
 */
void
mainbus_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	union mainbus_attach_args mba;

	printf("\n");

#if NBIOS > 0
	{
		mba.mba_bios.bios_dev = "bios";
		mba.mba_bios.bios_iot = I386_BUS_SPACE_IO;
		mba.mba_bios.bios_memt = I386_BUS_SPACE_MEM;
		config_found(self, &mba.mba_bios, mainbus_print);
	}
#endif

	/*
	 * XXX Note also that the presence of a PCI bus should
	 * XXX _always_ be checked, and if present the bus should be
	 * XXX 'found'.  However, because of the structure of the code,
	 * XXX that's not currently possible.
	 */
#if NPCI > 0
	if (pci_mode_detect() != 0) {
		mba.mba_pba.pba_busname = "pci";
		mba.mba_pba.pba_iot = I386_BUS_SPACE_IO;
		mba.mba_pba.pba_memt = I386_BUS_SPACE_MEM;
		mba.mba_pba.pba_dmat = &pci_bus_dma_tag;
		mba.mba_pba.pba_bus = 0;
		config_found(self, &mba.mba_pba, mainbus_print);
	}
#endif

	if (!bcmp(ISA_HOLE_VADDR(EISA_ID_PADDR), EISA_ID, EISA_ID_LEN)) {
		mba.mba_eba.eba_busname = "eisa";
		mba.mba_eba.eba_iot = I386_BUS_SPACE_IO;
		mba.mba_eba.eba_memt = I386_BUS_SPACE_MEM;
#if NEISA > 0
		mba.mba_eba.eba_dmat = &eisa_bus_dma_tag;
#endif
		config_found(self, &mba.mba_eba, mainbus_print);
	}

	if (isa_has_been_seen == 0) {
		mba.mba_iba.iba_busname = "isa";
		mba.mba_iba.iba_iot = I386_BUS_SPACE_IO;
		mba.mba_iba.iba_memt = I386_BUS_SPACE_MEM;
#if NISADMA > 0
		mba.mba_iba.iba_dmat = &isa_bus_dma_tag;
#endif
		config_found(self, &mba.mba_iba, mainbus_print);
	}
}

int
mainbus_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	union mainbus_attach_args *mba = aux;

	if (pnp)
		printf("%s at %s", mba->mba_busname, pnp);
	if (!strcmp(mba->mba_busname, "pci"))
		printf(" bus %d", mba->mba_pba.pba_bus);
	return (UNCONF);
}
