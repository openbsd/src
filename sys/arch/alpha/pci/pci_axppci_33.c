/*	$NetBSD: pci_axppci_33.c,v 1.1 1995/11/23 02:37:54 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jeffrey Hsu
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
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/lcavar.h>

#include <alpha/pci/pci_axppci_33.h>

#include <alpha/pci/siovar.h>

#include "sio.h"

void    *dec_axppci_33_pci_map_int __P((void *, pci_conftag_t,
	    pci_intr_pin_t, pci_intr_line_t, pci_intrlevel_t,
	    int (*func)(void *), void *));
void    dec_axppci_33_pci_unmap_int __P((void *, void *));

__const struct pci_intr_fns dec_axppci_33_pci_intr_fns = {
        dec_axppci_33_pci_map_int,
        dec_axppci_33_pci_unmap_int,
};

void *
dec_axppci_33_pci_map_int(lcv, tag, pin, line, level, func, arg)
	void *lcv;
        pci_conftag_t tag;
	pci_intr_pin_t pin;
	pci_intr_line_t line;
        pci_intrlevel_t level;
        int (*func) __P((void *));
        void *arg;
{
	struct lca_config *lcp = lcv;
	int bus, device, pirq;
	pci_confreg_t irreg, pirqreg;
	u_int8_t pirqline;

        if (pin == 0) {
                /* No IRQ used. */
                return 0;
        }
        if (pin > 4) {
                printf("dec_axppci_33_map_int: bad interrupt pin %d\n", pin);
                return NULL;
        }

	device = PCI_TAG_DEVICE(tag);

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

	case 8:				/* slot 3 */
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
	default:
		printf("dec_axppci_33_pci_map_int: unknown device %d\n",
			device);
		panic("dec_axppci_33_pci_map_int: bad device number");
	}

#define LCA_SIO_DEVICE 7		/* XXX */

	pirqreg = PCI_CONF_READ(lcp->lc_conffns, lcp->lc_confarg,
	    PCI_MAKE_TAG(0, LCA_SIO_DEVICE, 0), 0x60); /* XXX */
#if 0
	printf("dec_pci_axppci_33_map_int: device %d pin %c: pirq %d, reg = %x\n",
		device, '@' + pin, pirq, pirqreg);
#endif
	pirqline = (pirqreg >> (pirq * 8)) & 0xff;
	if ((pirqline & 0x80) != 0)
		return 0;			/* not routed? */
	pirqline &= 0xf;

#if 0
	printf("dec_pci_axppci_33_map_int: device %d pin %c: mapped to line %d\n",
	    device, '@' + pin, pirqline);
#endif

#if NSIO
	return ISA_INTR_ESTABLISH(&sio_isa_intr_fns, NULL,	/* XXX */
	    pirqline, ISA_IST_LEVEL, pci_intrlevel_to_isa(level),
	    func, arg);
#else
	panic("dec_axppci_33_pci_map_int: no sio!");
#endif
}

void
dec_axppci_33_pci_unmap_int(pifa, cookie)
	void *pifa;
	void *cookie;
{

	panic("dec_axppci_33_pci_unmap_int not implemented");	/* XXX */
}

void
pci_axppci_33_pickintr(pcf, pcfa, ppf, ppfa, pifp, pifap)
        __const struct pci_conf_fns *pcf;
        __const struct pci_pio_fns *ppf;
        void *pcfa, *ppfa;
        __const struct pci_intr_fns **pifp;
        void **pifap;
{
	pci_confreg_t sioclass;
	int sioII;

	/* XXX MAGIC NUMBER */
	sioclass = PCI_CONF_READ(pcf, pcfa, PCI_MAKE_TAG(0, LCA_SIO_DEVICE, 0),
	    PCI_CLASS_REG);
        sioII = (sioclass & 0xff) >= 3;

	if (!sioII)
		printf("WARNING: SIO NOT SIO II... NO BETS...\n");

	*pifp = &dec_axppci_33_pci_intr_fns;
	*pifap = pcfa;			/* XXX assumes apecs_config ptr */
#if NSIO
        sio_intr_setup(ppf, ppfa);
	set_iointr(&sio_iointr);
#else
	panic("pci_axppci_33_pickintr: no I/O interrupt handler (no sio)");
#endif
}
