/*	$OpenBSD: mainbus.c,v 1.21 2005/11/28 19:59:25 deraadt Exp $	*/
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
#include "mpbios.h"
#include "acpi.h"
#include "ipmi.h"
#include "esm.h"

#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>

#if NBIOS > 0
#include <machine/biosvar.h>
#endif

#if NACPI > 0
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#endif

#if NIPMI > 0
#include <dev/ipmivar.h>
#endif

#if NESM > 0
#include <arch/i386/i386/esmvar.h>
#endif

#if 0
#ifdef SMP /* XXX MULTIPROCESSOR */
#include <machine/mp.h>
#endif
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
	struct cpu_attach_args mba_caa;
	struct apic_attach_args	aaa_caa;
#if NACPI > 0
	struct acpi_attach_args mba_aaa;
#endif
#if NIPMI > 0
	struct ipmi_attach_args mba_iaa;
#endif
#if NESM > 0
	struct esm_attach_args mba_eaa;
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
	extern int cpu_id, cpu_feature;

	printf("\n");

#if NACPI > 0
	{
		memset(&mba.mba_aaa, 0, sizeof(mba.mba_aaa));
		mba.mba_aaa.aaa_name = "acpi";
		mba.mba_aaa.aaa_iot = I386_BUS_SPACE_IO;
		mba.mba_aaa.aaa_memt = I386_BUS_SPACE_MEM;

		config_found(self, &mba.mba_aaa, mainbus_print);
	}
#endif
#if NBIOS > 0
	{
		mba.mba_bios.bios_dev = "bios";
		mba.mba_bios.bios_iot = I386_BUS_SPACE_IO;
		mba.mba_bios.bios_memt = I386_BUS_SPACE_MEM;
		config_found(self, &mba.mba_bios, mainbus_print);
	}
#endif
#if NIPMI > 0
	{
		memset(&mba.mba_iaa, 0, sizeof(mba.mba_iaa));
		mba.mba_iaa.iaa_name = "ipmi";
		mba.mba_iaa.iaa_iot  = I386_BUS_SPACE_IO;
		mba.mba_iaa.iaa_memt = I386_BUS_SPACE_MEM;
		if (ipmi_probe(&mba.mba_iaa))
			config_found(self, &mba.mba_iaa, mainbus_print);
	}
#endif

#if NMPBIOS > 0
	if (mpbios_probe(self))
		mpbios_scan(self);
	else
#endif
	{
		struct cpu_attach_args caa;
		
		memset(&caa, 0, sizeof(caa));
		caa.caa_name = "cpu";
		caa.cpu_number = 0;
		caa.cpu_role = CPU_ROLE_SP;
		caa.cpu_func = 0;
		caa.cpu_signature = cpu_id;
		caa.feature_flags = cpu_feature;

		config_found(self, &caa, mainbus_print);
	}

#if 0
#ifdef SMP
	if (bios_smpinfo != NULL) {
		struct mp_float *mp = bios_smpinfo;

		printf("%s: MP 1.%d configuration %d\n", self->dv_xname,
		    mp->revision, mp->feature1);
	}
#ifdef CPU_DEBUG
	else
		printf ("%s: No MP configuration found.", self->dv_xname);
#endif
#endif
#endif

#if NESM > 0
	{
		memset(&mba.mba_eaa, 0, sizeof(mba.mba_eaa));
		mba.mba_eaa.eaa_name = "esm";
		mba.mba_eaa.eaa_iot  = I386_BUS_SPACE_IO;
		mba.mba_eaa.eaa_memt = I386_BUS_SPACE_MEM;
		if (esm_probe(&mba.mba_eaa))
			config_found(self, &mba.mba_eaa, mainbus_print);
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
