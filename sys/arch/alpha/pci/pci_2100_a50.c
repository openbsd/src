/*	$NetBSD: pci_2100_a50.c,v 1.2 1995/08/03 01:17:10 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <vm/vm.h>

#include <dev/isa/isavar.h>
#include <alpha/isa/isa_intr.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <alpha/pci/pci_chipset.h>

void	pci_2100_a50_attach __P((struct device *, struct device *, void *));
void	*pci_2100_a50_map_int __P((pcitag_t, pci_intrlevel, int (*) (void *),
	    void *, int));

struct pci_cfg_fcns pci_2100_a50_sio1_cfg_fcns = {	/* XXX diff? */
	pci_2100_a50_attach, pci_2100_a50_map_int,
};

struct pci_cfg_fcns pci_2100_a50_sio2_cfg_fcns = {
	pci_2100_a50_attach, pci_2100_a50_map_int,
};

void
pci_2100_a50_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	int bus, device;

#if 0
	for (bus = 0; bus <= 255; bus++)
#else
	/*
	 * XXX
	 * Some current chipsets do wacky things with bus numbers > 0.
	 * This seems like a violation of protocol, but the PCI BIOS does
	 * allow one to query the maximum bus number, and eventually we
	 * should do so.
	 */
	for (bus = 0; bus <= 0; bus++)
#endif
		for (device = 0; device <= 31; device++)
			pci_attach_subdev(self, bus, device);
}

void *
pci_2100_a50_map_int(tag, level, func, arg, pin)
        pcitag_t tag;
        pci_intrlevel level;
        int (*func) __P((void *));
        void *arg;
	int pin;
{
	int bus, device, pirq;
	pcireg_t pirqreg;
	u_int8_t line;

	bus = (tag >> 21) & 0xff;		/* XXX */
	device = (tag >> 16) & 0x1f;

	if (bus != 0)				/* XXX */
		return NULL;

	switch (device) {
	case 6:					/* NCR SCSI */
		pirq = 3;
		break;

	case 11:				/* slot 1 */
		switch (pin) {
		case PCI_INTERRUPT_PIN_A:
		case PCI_INTERRUPT_PIN_D:
			pirq = 0;
			break;
		case PCI_INTERRUPT_PIN_B:
			pirq = 2;
			break;
		case PCI_INTERRUPT_PIN_C:
			pirq = 1;
			break;
		};
		break;

	case 12:				/* slot 2 */
		switch (pin) {
		case PCI_INTERRUPT_PIN_A:
		case PCI_INTERRUPT_PIN_D:
			pirq = 1;
			break;
		case PCI_INTERRUPT_PIN_B:
			pirq = 0;
			break;
		case PCI_INTERRUPT_PIN_C:
			pirq = 2;
			break;
		};
		break;

	case 13:				/* slot 3 */
		switch (pin) {
		case PCI_INTERRUPT_PIN_A:
		case PCI_INTERRUPT_PIN_D:
			pirq = 2;
			break;
		case PCI_INTERRUPT_PIN_B:
			pirq = 1;
			break;
		case PCI_INTERRUPT_PIN_C:
			pirq = 0;
			break;
		};
		break;
	}

	pirqreg = pci_conf_read(pci_make_tag(0, 7, 0), 0x60);	/* XXX */
#if 0
	printf("pci_2100_a50_map_int: device %d pin %c: pirq %d, reg = %x\n",
		device, '@' + pin, pirq, pirqreg);
#endif
	line = (pirqreg >> (pirq * 8)) & 0xff;
	if ((line & 0x80) != 0)
		return 0;			/* not routed? */
	line &= 0xf;

#if 0
	printf("pci_2100_a50_map_int: device %d pin %c: mapped to line %d\n",
	    device, '@' + pin, line);
#endif

	return isa_intr_establish(line, ISA_IST_LEVEL, pcilevel_to_isa(level),
	    func, arg);
}

void
pci_2100_a50_pickintr()
{
	pcireg_t sioclass;
	int sioII;

	/* XXX MAGIC NUMBER */
	sioclass = pci_conf_read(pci_make_tag(0, 7, 0), PCI_CLASS_REG);
        sioII = (sioclass & 0xff) >= 3;
	if (!sioII)
		printf("WARNING: SIO NOT SIO II... NO BETS...\n");

	if (!sioII)
		pci_cfg_fcns = &pci_2100_a50_sio1_cfg_fcns;
	else
		pci_cfg_fcns = &pci_2100_a50_sio2_cfg_fcns;

	isa_intr_fcns = &sio_intr_fcns;
	(*isa_intr_fcns->isa_intr_setup)();
	set_iointr(isa_intr_fcns->isa_iointr);
}
