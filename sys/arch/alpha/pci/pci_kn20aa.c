/*	$OpenBSD: pci_kn20aa.c,v 1.6 1996/10/04 03:06:04 deraadt Exp $	*/
/*	$NetBSD: pci_kn20aa.c,v 1.3.4.2 1996/06/13 18:35:31 cgd Exp $	*/

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#include <alpha/pci/pci_kn20aa.h>

#ifndef EVCNT_COUNTERS
#include <machine/intrcnt.h>
#endif

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

int	dec_kn20aa_intr_map __P((void *, pcitag_t, int, int,
	    pci_intr_handle_t *));
const char *dec_kn20aa_intr_string __P((void *, pci_intr_handle_t));
void	*dec_kn20aa_intr_establish __P((void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *, char *));
void	dec_kn20aa_intr_disestablish __P((void *, void *));

#define	KN20AA_PCEB_IRQ	31
#define	KN20AA_MAX_IRQ	32
#define	PCI_STRAY_MAX	5

struct kn20aa_intrhand {
	TAILQ_ENTRY(kn20aa_intrhand) ih_q;
        int     (*ih_fun)();
        void    *ih_arg;
        u_long  ih_count;
        int     ih_level;
};
TAILQ_HEAD(kn20aa_intrchain, kn20aa_intrhand);

struct kn20aa_intrchain kn20aa_pci_intrs[KN20AA_MAX_IRQ];
int	kn20aa_pci_strayintrcnt[KN20AA_MAX_IRQ];
#ifdef EVCNT_COUNTERS
struct evcnt kn20aa_intr_evcnt;
#endif

void	kn20aa_pci_strayintr __P((int irq));
void	kn20aa_iointr __P((void *framep, int vec));
void	kn20aa_enable_intr __P((int irq));
void	kn20aa_disable_intr __P((int irq));
struct kn20aa_intrhand *kn20aa_attach_intr __P((struct kn20aa_intrchain *,
			    int, int (*) (void *), void *));

void
pci_kn20aa_pickintr(ccp)
	struct cia_config *ccp;
{
	int i;
	struct kn20aa_intrhand *nintrhand;
	bus_chipset_tag_t bc = &ccp->cc_bc;
	pci_chipset_tag_t pc = &ccp->cc_pc;

	for (i = 0; i < KN20AA_MAX_IRQ; i++)
		TAILQ_INIT(&kn20aa_pci_intrs[i]);

        pc->pc_intr_v = ccp;
        pc->pc_intr_map = dec_kn20aa_intr_map;
        pc->pc_intr_string = dec_kn20aa_intr_string;
        pc->pc_intr_establish = dec_kn20aa_intr_establish;
        pc->pc_intr_disestablish = dec_kn20aa_intr_disestablish;

#if NSIO
	sio_intr_setup(bc);
#endif

	set_iointr(kn20aa_iointr);

#if NSIO
	kn20aa_enable_intr(KN20AA_PCEB_IRQ);
#if 0 /* XXX init PCEB interrupt handler? */
	kn20aa_attach_intr(&kn20aa_pci_intrs[KN20AA_PCEB_IRQ], ???, ???, ???);
#endif
#endif
}

int     
dec_kn20aa_intr_map(ccv, bustag, buspin, line, ihp)
        void *ccv;
        pcitag_t bustag; 
        int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct cia_config *ccp = ccv;
	pci_chipset_tag_t pc = &ccp->cc_pc;
	int device;
	int kn20aa_irq;
	void *ih;

        if (buspin == 0) {
                /* No IRQ used. */
                return 1;
        }
        if (buspin > 4) {
                printf("pci_map_int: bad interrupt pin %d\n", buspin);
                return 1;
        }

	/*
	 * Slot->interrupt translation.  Appears to work, though it
	 * may not hold up forever.
	 *
	 * The DEC engineers who did this hardware obviously engaged
	 * in random drug testing.
	 */
	pci_decompose_tag(pc, bustag, NULL, &device, NULL);
	switch (device) {
	case 11:
	case 12:
		kn20aa_irq = ((device - 11) + 0) * 4;
		break;

	case 7:
		kn20aa_irq = 8;
		break;

	case 9:
		kn20aa_irq = 12;
		break;

	case 6:					/* 21040 on AlphaStation 500 */
		kn20aa_irq = 13;
		break;

	case 8:
		kn20aa_irq = 16;
		break;

	default:
		panic("pci_kn20aa_map_int: invalid device number %d\n",
		    device);
	}

	kn20aa_irq += buspin - 1;
	if (kn20aa_irq > KN20AA_MAX_IRQ)
		panic("pci_kn20aa_map_int: kn20aa_irq too large (%d)\n",
		    kn20aa_irq);

	*ihp = kn20aa_irq;
	return (0);
}

const char *
dec_kn20aa_intr_string(ccv, ih)
	void *ccv;
	pci_intr_handle_t ih;
{
	struct cia_config *ccp = ccv;
        static char irqstr[15];          /* 11 + 2 + NULL + sanity */

        if (ih > KN20AA_MAX_IRQ)
                panic("dec_kn20aa_a50_intr_string: bogus kn20aa IRQ 0x%x\n",
		    ih);

        sprintf(irqstr, "kn20aa irq %d", ih);
        return (irqstr);
}

void *
dec_kn20aa_intr_establish(ccv, ih, level, func, arg, name)
        void *ccv, *arg;
        pci_intr_handle_t ih;
        int level;
        int (*func) __P((void *));
	char *name;
{           
        struct cia_config *ccp = ccv;
	void *cookie;

        if (ih > KN20AA_MAX_IRQ)
                panic("dec_kn20aa_intr_establish: bogus kn20aa IRQ 0x%x\n",
		    ih);

	cookie = kn20aa_attach_intr(&kn20aa_pci_intrs[ih], level, func, arg);
	kn20aa_enable_intr(ih);
	return (cookie);
}

void    
dec_kn20aa_intr_disestablish(ccv, cookie)
        void *ccv, *cookie;
{
	struct cia_config *ccp = ccv;

	panic("dec_kn20aa_intr_disestablish not implemented"); /* XXX */
}

/*
 * caught a stray interrupt; notify if not too many seen already.
 */
void
kn20aa_pci_strayintr(irq)
	int irq;
{

	kn20aa_pci_strayintrcnt[irq]++;
	if (kn20aa_pci_strayintrcnt[irq] == PCI_STRAY_MAX)
		kn20aa_disable_intr(irq);

	log(LOG_ERR, "stray kn20aa irq %d\n", irq);
	if (kn20aa_pci_strayintrcnt[irq] == PCI_STRAY_MAX)
		log(LOG_ERR, "disabling interrupts on kn20aa irq %d\n", irq);
}

void
kn20aa_iointr(framep, vec)
	void *framep;
	int vec;
{
	struct kn20aa_intrhand *ih;
	int irq, handled;

	if (vec >= 0x900) {
		if (vec >= 0x900 + (KN20AA_MAX_IRQ << 4))
			panic("kn20aa_iointr: vec 0x%x out of range\n", vec);
		irq = (vec - 0x900) >> 4;

#ifdef EVCNT_COUNTERS
		kn20aa_intr_evcnt.ev_count++;
#else
		if (KN20AA_MAX_IRQ != INTRCNT_KN20AA_IRQ_LEN)
			panic("kn20aa interrupt counter sizes inconsistent");
		intrcnt[INTRCNT_KN20AA_IRQ + irq]++;
#endif

		for (ih = kn20aa_pci_intrs[irq].tqh_first, handled = 0;
		    ih != NULL; ih = ih->ih_q.tqe_next) {
			int rv;

			rv = (*ih->ih_fun)(ih->ih_arg);

			ih->ih_count++;
			handled = handled || (rv != 0);
		}
		if (!handled)
			kn20aa_pci_strayintr(irq);
		return;
	}
	if (vec >= 0x800) {
#if NSIO
		sio_iointr(framep, vec);
#endif
		return;
	} 
	panic("kn20aa_iointr: weird vec 0x%x\n", vec);
}

void
kn20aa_enable_intr(irq)
	int irq;
{

	/*
	 * From disassembling small bits of the OSF/1 kernel:
	 * the following appears to enable a given interrupt request.
	 * "blech."  I'd give valuable body parts for better docs or
	 * for a good decompiler.
	 */
	wbflush();
	REGVAL(0x8780000000L + 0x40L) |= (1 << irq);	/* XXX */
	wbflush();
}

void
kn20aa_disable_intr(irq)
	int irq;
{

	wbflush();
	REGVAL(0x8780000000L + 0x40L) &= ~(1 << irq);	/* XXX */
	wbflush();
}

struct kn20aa_intrhand *
kn20aa_attach_intr(chain, level, func, arg)
	struct kn20aa_intrchain *chain;
	int level;
	int (*func) __P((void *));
	void *arg;
{
	struct kn20aa_intrhand *nintrhand;

	nintrhand = (struct kn20aa_intrhand *)
	    malloc(sizeof *nintrhand, M_DEVBUF, M_WAITOK);

        nintrhand->ih_fun = func;
        nintrhand->ih_arg = arg;
        nintrhand->ih_count = 0;
        nintrhand->ih_level = level;
	TAILQ_INSERT_TAIL(chain, nintrhand, ih_q);
	
	return (nintrhand);
}
