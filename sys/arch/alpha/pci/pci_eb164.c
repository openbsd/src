/*	$OpenBSD: pci_eb164.c,v 1.1 1997/01/24 19:57:48 niklas Exp $	*/
/*	$NetBSD: pci_eb164.c,v 1.4 1996/11/25 03:47:05 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <vm/vm.h>

#include <machine/autoconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#include <alpha/pci/pci_eb164.h>

#ifndef EVCNT_COUNTERS
#include <machine/intrcnt.h>
#endif

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

int	dec_eb164_intr_map __P((void *, pcitag_t, int, int,
	    pci_intr_handle_t *));
const char *dec_eb164_intr_string __P((void *, pci_intr_handle_t));
void	*dec_eb164_intr_establish __P((void *, pci_intr_handle_t,
	    int, int (*)(void *), void *, char *));
void	dec_eb164_intr_disestablish __P((void *, void *));

#define	EB164_SIO_IRQ	4  
#define	EB164_MAX_IRQ	24
#define	PCI_STRAY_MAX	5

struct alpha_shared_intr *eb164_pci_intr;
#ifdef EVCNT_COUNTERS
struct evcnt eb164_intr_evcnt;
#endif

bus_space_tag_t eb164_intrgate_iot;
bus_space_handle_t eb164_intrgate_ioh;

void	eb164_iointr __P((void *framep, unsigned long vec));
extern void	eb164_intr_enable __P((int irq));	/* pci_eb164_intr.S */
extern void	eb164_intr_disable __P((int irq));	/* pci_eb164_intr.S */

void
pci_eb164_pickintr(ccp)
	struct cia_config *ccp;
{
	bus_space_tag_t iot = ccp->cc_iot;
	pci_chipset_tag_t pc = &ccp->cc_pc;
	int i;

        pc->pc_intr_v = ccp;
        pc->pc_intr_map = dec_eb164_intr_map;
        pc->pc_intr_string = dec_eb164_intr_string;
        pc->pc_intr_establish = dec_eb164_intr_establish;
        pc->pc_intr_disestablish = dec_eb164_intr_disestablish;

	eb164_intrgate_iot = iot;
	if (bus_space_map(eb164_intrgate_iot, 0x804, 3, 0,
	    &eb164_intrgate_ioh) != 0)
		panic("pci_eb164_pickintr: couldn't map interrupt PLD");
	for (i = 0; i < EB164_MAX_IRQ; i++)
		eb164_intr_disable(i);	

	eb164_pci_intr = alpha_shared_intr_alloc(EB164_MAX_IRQ);
	for (i = 0; i < EB164_MAX_IRQ; i++)
		alpha_shared_intr_set_maxstrays(eb164_pci_intr, i,
			PCI_STRAY_MAX);

#if NSIO
	sio_intr_setup(iot);
	eb164_intr_enable(EB164_SIO_IRQ);
#endif

	set_iointr(eb164_iointr);
}

int     
dec_eb164_intr_map(ccv, bustag, buspin, line, ihp)
        void *ccv;
        pcitag_t bustag; 
        int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct cia_config *ccp = ccv;
	pci_chipset_tag_t pc = &ccp->cc_pc;
	int device;
	int eb164_irq, pinbase, pinoff;

        if (buspin == 0) {
                /* No IRQ used. */
                return 1;
        }
        if (buspin > 4) {
                printf("pci_map_int: bad interrupt pin %d\n", buspin);
                return 1;
        }

	pci_decompose_tag(pc, bustag, NULL, &device, NULL);
	switch (device) {
#if 0	/* THIS CODE SHOULD NEVER BE CALLED FOR THE SIO */
	case 8: 					/* SIO */
		eb164_irq = 4;
		break;
#endif

	case 11:
		eb164_irq = 5;				/* IDE */
		break;

	case 5:
	case 6:
	case 7:
	case 9:
		switch (buspin) {
		case 1:
			pinbase = 0;
			break;
		case 2:
		case 3:
		case 4:
			pinbase = (buspin * 4) - 1;
			break;
#ifdef DIAGNOSTIC
		default:
			panic("dec_eb164_intr_map: slot buspin switch");
#endif
		};
		switch (device) {
		case 5:
			pinoff = 2;
			break;

		case 6:
		case 7:
		case 9:
			pinoff = device - 6;
			break;
#ifdef DIAGNOSTIC
		default:
			panic("dec_eb164_intr_map: slot device switch");
#endif
		}
		eb164_irq = pinoff + pinbase;
		break;
	default:
		panic("pci_eb164_map_int: invalid device number %d\n",
		    device);
	}

	if (eb164_irq > EB164_MAX_IRQ)
		panic("pci_eb164_map_int: eb164_irq too large (%d)\n",
		    eb164_irq);

	*ihp = eb164_irq;
	return (0);
}

const char *
dec_eb164_intr_string(ccv, ih)
	void *ccv;
	pci_intr_handle_t ih;
{
#if 0
	struct cia_config *ccp = ccv;
#endif
        static char irqstr[15];          /* 11 + 2 + NULL + sanity */

        if (ih > EB164_MAX_IRQ)
                panic("dec_eb164_intr_string: bogus eb164 IRQ 0x%x\n", ih);
        sprintf(irqstr, "eb164 irq %d", ih);
        return (irqstr);
}

void *
dec_eb164_intr_establish(ccv, ih, level, func, arg, name)
        void *ccv, *arg;
        pci_intr_handle_t ih;
        int level;
        int (*func) __P((void *));
	char *name;
{
	void *cookie;

	if (ih > EB164_MAX_IRQ)
		panic("dec_eb164_intr_establish: bogus eb164 IRQ 0x%x\n", ih);

	cookie = alpha_shared_intr_establish(eb164_pci_intr, ih, IST_LEVEL,
	    level, func, arg, name);

	if (cookie != NULL && alpha_shared_intr_isactive(eb164_pci_intr, ih))
		eb164_intr_enable(ih);
	return (cookie);
}

void
dec_eb164_intr_disestablish(ccv, cookie)
        void *ccv, *cookie;
{
#if 0
	struct cia_config *ccp = ccv;
#endif

	panic("dec_eb164_intr_disestablish not implemented"); /* XXX */
}

void
eb164_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq; 

	if (vec >= 0x900) {
		if (vec >= 0x900 + (EB164_MAX_IRQ << 4))
			panic("eb164_iointr: vec 0x%x out of range\n", vec);
		irq = (vec - 0x900) >> 4;

#ifdef EVCNT_COUNTERS
		eb164_intr_evcnt.ev_count++;
#else
		if (EB164_MAX_IRQ != INTRCNT_EB164_IRQ_LEN)
			panic("eb164 interrupt counter sizes inconsistent");
		intrcnt[INTRCNT_EB164_IRQ + irq]++;
#endif

		if (!alpha_shared_intr_dispatch(eb164_pci_intr, irq)) {
			alpha_shared_intr_stray(eb164_pci_intr, irq,
			    "eb164 irq");
			if (eb164_pci_intr[irq].intr_nstrays ==
			    eb164_pci_intr[irq].intr_maxstrays)
				eb164_intr_disable(irq);
		}
		return;
	}
#if NSIO
	if (vec >= 0x800) {
		sio_iointr(framep, vec);
		return;
	}
#endif
	panic("eb164_iointr: weird vec 0x%x\n", vec);
}

#if 0		/* THIS DOES NOT WORK!  see pci_eb164_intr.S. */
u_int8_t eb164_intr_mask[3] = { 0xff, 0xff, 0xff };

void
eb164_intr_enable(irq)
	int irq;
{
	int byte = (irq / 8), bit = (irq % 8);

#if 1
	printf("eb164_intr_enable: enabling %d (%d:%d)\n", irq, byte, bit);
#endif
	eb164_intr_mask[byte] &= ~(1 << bit);

	bus_space_write_1(eb164_intrgate_iot, eb164_intrgate_ioh, byte,
	    eb164_intr_mask[byte]);
}

void
eb164_intr_disable(irq)
	int irq;
{
	int byte = (irq / 8), bit = (irq % 8);

#if 1
	printf("eb164_intr_disable: disabling %d (%d:%d)\n", irq, byte, bit);
#endif
	eb164_intr_mask[byte] |= (1 << bit);

	bus_space_write_1(eb164_intrgate_iot, eb164_intrgate_ioh, byte,
	    eb164_intr_mask[byte]);
}
#endif
