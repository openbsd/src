/*	$OpenBSD: mainbus.c,v 1.3 2005/10/19 01:41:44 marco Exp $	*/
/*	$NetBSD: mainbus.c,v 1.1 2003/04/26 18:39:29 fvdl Exp $	*/

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
#include <dev/pci/pcivar.h>

#include <dev/isa/isareg.h>

#include "pci.h"
#include "isa.h"
#include "acpi.h"
#include "ipmi.h"

#include <machine/cpuvar.h>
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>

#if NACPI > 0
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#endif

#if NIPMI > 0
#include <dev/ipmivar.h>
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
	struct isabus_attach_args mba_iba;
	struct cpu_attach_args mba_caa;
	struct apic_attach_args aaa_caa;
#if NACPI > 0
	struct acpi_attach_args mba_aaa;
#endif	
#if NIPMI > 0
	struct ipmi_attach_args mba_iaa;
#endif
};

/*
 * This is set when the ISA bus is attached.  If it's not set by the
 * time it's checked below, then mainbus attempts to attach an ISA.
 */
int	isa_has_been_seen;
#if NISA > 0
struct isabus_attach_args mba_iba = {
	"isa",
	X86_BUS_SPACE_IO, X86_BUS_SPACE_MEM,
	NULL,
};
#endif

#if defined(MPBIOS) || defined(MPACPI)
struct mp_bus *mp_busses;
int mp_nbus;
struct mp_intr_map *mp_intrs;
int mp_nintr;
 
int mp_isa_bus = -1;
int mp_eisa_bus = -1;

#ifdef MPVERBOSE
int mp_verbose = 1;
#else
int mp_verbose = 0;
#endif
#endif


/*
 * Probe for the mainbus; always succeeds.
 */
int
mainbus_match(struct device *parent, void *match, void *aux)
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
#if NPCI > 0
	union mainbus_attach_args mba;
#endif
#ifdef MPBIOS
	int mpbios_present = 0;
#endif

	printf("\n");

#if NACPI > 0
	{
		memset(&mba.mba_aaa, 0, sizeof(mba.mba_aaa));
		mba.mba_aaa.aaa_name = "acpi";
		mba.mba_aaa.aaa_iot = X86_BUS_SPACE_IO;
		mba.mba_aaa.aaa_memt = X86_BUS_SPACE_MEM;

		config_found(self, &mba.mba_aaa, mainbus_print);
	}
#endif

#if NIPMI > 0
	{
		memset(&mba.mba_iaa, 0, sizeof(mba.mba_iaa));
		mba.mba_iaa.iaa_name = "ipmi";
		mba.mba_iaa.iaa_iot  = X86_BUS_SPACE_IO;
		mba.mba_iaa.iaa_memt = X86_BUS_SPACE_MEM;
		if (ipmi_probe(&mba.mba_iaa))
			config_found(self, &mba.mba_iaa, mainbus_print);
	}
#endif

#ifdef MPBIOS
	mpbios_present = mpbios_probe(self);
#endif

#if NPCI > 0
	pci_mode = pci_mode_detect();
#endif

#ifdef MPBIOS
	if (mpbios_present)
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
                        
		config_found(self, &caa, mainbus_print);
	}

#if NPCI > 0
	if (pci_mode != 0) {
		mba.mba_pba.pba_busname = "pci";
		mba.mba_pba.pba_iot = X86_BUS_SPACE_IO;
		mba.mba_pba.pba_memt = X86_BUS_SPACE_MEM;
		mba.mba_pba.pba_dmat = &pci_bus_dma_tag;
		mba.mba_pba.pba_bus = 0;
		mba.mba_pba.pba_pc = NULL;
		config_found(self, &mba.mba_pba, mainbus_print);
	}
#endif

#if NISA > 0
	if (isa_has_been_seen == 0)
		config_found(self, &mba_iba, mainbus_print);
#endif

}

int
mainbus_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	union mainbus_attach_args *mba = aux;

	if (pnp)
		printf("%s at %s", mba->mba_busname, pnp);
	if (strcmp(mba->mba_busname, "pci") == 0)
		printf(" bus %d", mba->mba_pba.pba_bus);
	return (UNCONF);
}
