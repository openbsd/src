/*	$OpenBSD: pchb.c,v 1.6 2007/05/19 19:32:03 tedu Exp $	*/
/*	$NetBSD: pchb.c,v 1.1 2003/04/26 18:39:50 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <dev/pci/pcidevs.h>

#include <arch/amd64/pci/pchbvar.h>

#define PCISET_BRIDGETYPE_MASK	0x3
#define PCISET_TYPE_COMPAT	0x1
#define PCISET_TYPE_AUX		0x2

#define PCISET_BUSCONFIG_REG	0x48
#define PCISET_BRIDGE_NUMBER(reg)	(((reg) >> 8) & 0xff)
#define PCISET_PCI_BUS_NUMBER(reg)	(((reg) >> 16) & 0xff)

/* XXX should be in dev/ic/i82443reg.h */
#define	I82443BX_SDRAMC_REG	0x76

/* XXX should be in dev/ic/i82424{reg.var}.h */
#define I82424_CPU_BCTL_REG		0x53
#define I82424_PCI_BCTL_REG		0x54

#define I82424_BCTL_CPUMEM_POSTEN	0x01
#define I82424_BCTL_CPUPCI_POSTEN	0x02
#define I82424_BCTL_PCIMEM_BURSTEN	0x01
#define I82424_BCTL_PCI_BURSTEN		0x02

/* XXX should be in dev/ic/amd64htreg.h */
#define AMD64HT_LDT0_BUS	0x94
#define AMD64HT_LDT0_TYPE	0x98
#define AMD64HT_LDT1_BUS	0xb4
#define AMD64HT_LDT1_TYPE	0xb8
#define AMD64HT_LDT2_BUS	0xd4
#define AMD64HT_LDT2_TYPE	0xd8

#define AMD64HT_NUM_LDT		3

#define AMD64HT_LDT_TYPE_MASK		0x0000001f
#define  AMD64HT_LDT_INIT_COMPLETE	0x00000002
#define  AMD64HT_LDT_NC			0x00000004

#define AMD64HT_LDT_SEC_BUS_NUM(reg)	(((reg) >> 8) & 0xff)

int	pchbmatch(struct device *, void *, void *);
void	pchbattach(struct device *, struct device *, void *);

int	pchb_print(void *, const char *);
void	pchb_amd64ht_attach (struct device *, struct pci_attach_args *, int);

struct cfattach pchb_ca = {
	sizeof(struct pchb_softc), pchbmatch, pchbattach,
};

struct cfdriver pchb_cd = {
	NULL, "pchb", DV_DULL
};

int
pchbmatch(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_HOST) {
		return (1);
	}

	return (0);
}

void
pchbattach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	int i;

	printf("\n");

	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_AMD:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_AMD_AMD64_HT:
			for (i = 0; i < AMD64HT_NUM_LDT; i++)
				pchb_amd64ht_attach(self, pa, i);
			break;
		}
		break;
	case PCI_VENDOR_INTEL:
#ifdef PCIAGP
		pciagp_set_pchb(pa);
#endif
		break;
	}
}

int
pchb_print(void *aux, const char *pnp)
{
	struct pcibus_attach_args *pba = aux;

	if (pnp)
		printf("%s at %s", pba->pba_busname, pnp);
	printf(" bus %d", pba->pba_bus);
	return (UNCONF);
}

void
pchb_amd64ht_attach (struct device *self, struct pci_attach_args *pa, int i)
{
	struct pcibus_attach_args pba;
	pcireg_t type, bus;
	int reg;

	reg = AMD64HT_LDT0_TYPE + i * 0x20;
	type = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);
	if ((type & AMD64HT_LDT_INIT_COMPLETE) == 0 ||
	    (type & AMD64HT_LDT_NC) == 0)
		return;

	reg = AMD64HT_LDT0_BUS + i * 0x20;
	bus = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);
	if (AMD64HT_LDT_SEC_BUS_NUM(bus) > 0) {
		pba.pba_busname = "pci";
		pba.pba_iot = pa->pa_iot;
		pba.pba_memt = pa->pa_memt;
		pba.pba_dmat = pa->pa_dmat;
		pba.pba_bus = AMD64HT_LDT_SEC_BUS_NUM(bus);
		pba.pba_bridgetag = NULL;
		pba.pba_pc = pa->pa_pc;
		config_found(self, &pba, pchb_print);
	}
}
