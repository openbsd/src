/*	$OpenBSD: pci.c,v 1.3 1996/05/07 07:38:49 deraadt Exp $	*/
/*	$NetBSD: pci.c,v 1.19 1996/05/03 17:33:49 christos Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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

/*
 * PCI bus autoconfiguration.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

int pcimatch __P((struct device *, void *, void *));
void pciattach __P((struct device *, struct device *, void *));

struct cfattach pci_ca = {
	sizeof(struct device), pcimatch, pciattach
};

struct cfdriver pci_cd = {
	NULL, "pci", DV_DULL
};

int	pciprint __P((void *, char *));
int	pcisubmatch __P((struct device *, void *, void *));

int
pcimatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pcibus_attach_args *pba = aux;

	if (strcmp(pba->pba_busname, cf->cf_driver->cd_name))
		return (0);

	/* Check the locators */
	if (cf->pcibuscf_bus != PCIBUS_UNK_BUS &&
	    cf->pcibuscf_bus != pba->pba_bus)
		return (0);

	/* sanity */
	if (pba->pba_bus < 0 || pba->pba_bus > 255)
		return (0);

	/*
	 * XXX check other (hardware?) indicators
	 */

	return 1;
}

void
pciattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibus_attach_args *pba = aux;
	bus_chipset_tag_t bc;
	pci_chipset_tag_t pc;
	int bus, device, maxndevs, function, nfunctions;

	pci_attach_hook(parent, self, pba);
	printf("\n");

	bc = pba->pba_bc;
	pc = pba->pba_pc;
	bus = pba->pba_bus;
	maxndevs = pci_bus_maxdevs(pc, bus);

	for (device = 0; device < maxndevs; device++) {
		pcitag_t tag;
		pcireg_t id, class, intr, bhlcr;
		struct pci_attach_args pa;
		int pin;

		tag = pci_make_tag(pc, bus, device, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (id == 0 || id == 0xffffffff)
			continue;

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		nfunctions = PCI_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;

		for (function = 0; function < nfunctions; function++) {
			tag = pci_make_tag(pc, bus, device, function);
			id = pci_conf_read(pc, tag, PCI_ID_REG);
			if (id == 0 || id == 0xffffffff)
				continue;
			class = pci_conf_read(pc, tag, PCI_CLASS_REG);
			intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);

			pa.pa_bc = bc;
			pa.pa_pc = pc;
			pa.pa_device = device;
			pa.pa_function = function;
			pa.pa_tag = tag;
			pa.pa_id = id;
			pa.pa_class = class;

			if (bus == 0) {
				pa.pa_intrswiz = 0;
				pa.pa_intrtag = tag;
			} else {
				pa.pa_intrswiz = pba->pba_intrswiz + device;
				pa.pa_intrtag = pba->pba_intrtag;
			}
			pin = PCI_INTERRUPT_PIN(intr);
			if (pin == PCI_INTERRUPT_PIN_NONE) {
				/* no interrupt */
				pa.pa_intrpin = 0;
			} else {
				/*
				 * swizzle it based on the number of
				 * busses we're behind and our device
				 * number.
				 */
				pa.pa_intrpin =			/* XXX */
				    ((pin + pa.pa_intrswiz - 1) % 4) + 1;
			}
			pa.pa_intrline = PCI_INTERRUPT_LINE(intr);

			config_found_sm(self, &pa, pciprint, pcisubmatch);
		}
	}
}

int
pciprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct pci_attach_args *pa = aux;
	char devinfo[256];

	if (pnp) {
		pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo);
		printf("%s at %s", devinfo, pnp);
	}
	printf(" dev %d function %d", pa->pa_device, pa->pa_function);
	return (UNCONF);
}

int
pcisubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (cf->pcicf_dev != PCI_UNK_DEV &&
	    cf->pcicf_dev != pa->pa_device)
		return 0;
	if (cf->pcicf_function != PCI_UNK_FUNCTION &&
	    cf->pcicf_function != pa->pa_function)
		return 0;
	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

int
pci_io_find(pc, pcitag, reg, iobasep, iosizep)
	pci_chipset_tag_t pc;
	pcitag_t pcitag;
	int reg;
	bus_io_addr_t *iobasep;
	bus_io_size_t *iosizep;
{
	pcireg_t addrdata, sizedata;
	int s;

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("pci_io_find: bad request");

	/* XXX?
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	addrdata = pci_conf_read(pc, pcitag, reg);

	s = splhigh();
	pci_conf_write(pc, pcitag, reg, 0xffffffff);
	sizedata = pci_conf_read(pc, pcitag, reg);
	pci_conf_write(pc, pcitag, reg, addrdata);
	splx(s);

	if (PCI_MAPREG_TYPE(addrdata) != PCI_MAPREG_TYPE_IO)
		panic("pci_io_find: not an I/O region");

	if (iobasep != NULL)
		*iobasep = PCI_MAPREG_IO_ADDR(addrdata);
	if (iosizep != NULL)
		*iosizep = ~PCI_MAPREG_IO_ADDR(sizedata) + 1;

	return (0);
}

int
pci_mem_find(pc, pcitag, reg, membasep, memsizep, cacheablep)
	pci_chipset_tag_t pc;
	pcitag_t pcitag;
	int reg;
	bus_mem_addr_t *membasep;
	bus_mem_size_t *memsizep;
	int *cacheablep;
{
	pcireg_t addrdata, sizedata;
	int s;

	if (reg < PCI_MAPREG_START || reg >= PCI_MAPREG_END || (reg & 3))
		panic("pci_find_mem: bad request");

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	addrdata = pci_conf_read(pc, pcitag, reg);

	s = splhigh();
	pci_conf_write(pc, pcitag, reg, 0xffffffff);
	sizedata = pci_conf_read(pc, pcitag, reg);
	pci_conf_write(pc, pcitag, reg, addrdata);
	splx(s);

	if (PCI_MAPREG_TYPE(addrdata) == PCI_MAPREG_TYPE_IO)
		panic("pci_find_mem: I/O region");

	switch (PCI_MAPREG_MEM_TYPE(addrdata)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
/* XXX */	printf("pci_find_mem: 64-bit region\n");
/* XXX */	return (1);
	default:
		printf("pci_find_mem: reserved region type\n");
		return (1);
	}

	if (membasep != NULL)
		*membasep = PCI_MAPREG_MEM_ADDR(addrdata);	/* PCI addr */
	if (memsizep != NULL)
		*memsizep = ~PCI_MAPREG_MEM_ADDR(sizedata) + 1;
	if (cacheablep != NULL)
		*cacheablep = PCI_MAPREG_MEM_CACHEABLE(addrdata);

	return 0;
}
