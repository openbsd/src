/*	$NetBSD: pci_kn20aa.c,v 1.1 1995/11/23 02:38:00 cgd Exp $	*/

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

void	*kn20aa_pci_map_int __P((void *, pci_conftag_t, pci_intr_pin_t,
	    pci_intr_line_t, pci_intrlevel_t, int (*func)(void *), void *));
void	kn20aa_pci_unmap_int __P((void *, void *));

__const struct pci_intr_fns kn20aa_pci_intr_fns = {
	kn20aa_pci_map_int,
	kn20aa_pci_unmap_int,
};

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
struct kn20aa_intrhand *kn20aa_attach_intr __P((struct kn20aa_intrchain *,
			    pci_intrlevel_t, int (*) (void *), void *));

void
pci_kn20aa_pickintr(pcf, pcfa, ppf, ppfa, pifp, pifap)
	__const struct pci_conf_fns *pcf;
	__const struct pci_pio_fns *ppf;
	void *pcfa, *ppfa;
	__const struct pci_intr_fns **pifp;
	void **pifap;
{
	int i;
	struct kn20aa_intrhand *nintrhand;


	for (i = 0; i < KN20AA_MAX_IRQ; i++)
		TAILQ_INIT(&kn20aa_pci_intrs[i]);

	*pifp = &kn20aa_pci_intr_fns;
	*pifap = NULL;					/* XXX ? */

#if NSIO
	sio_intr_setup(ppf, ppfa);
#endif

	set_iointr(kn20aa_iointr);

#if NSIO
	kn20aa_enable_intr(KN20AA_PCEB_IRQ);
#if 0 /* XXX init PCEB interrupt handler? */
	kn20aa_attach_intr(&kn20aa_pci_intrs[KN20AA_PCEB_IRQ], ???, ???, ???);
#endif
#endif
}

void *
kn20aa_pci_map_int(ccv, tag, pin, line, level, func, arg)
	void *ccv;
        pci_conftag_t tag;
	pci_intr_pin_t pin;
	pci_intr_line_t line;
        pci_intrlevel_t level;
        int (*func) __P((void *));
        void *arg;
{
	int device;
	int kn20aa_slot, kn20aa_irq;
	void *ih;

        if (pin == 0) {
                /* No IRQ used. */
                return 0;
        }
        if (pin > 4) {
                printf("pci_map_int: bad interrupt pin %d\n", pin);
                return NULL;
        }

	/*
	 * Slot->interrupt translation.  Appears to work, though it
	 * may not hold up forever.
	 *
	 * The DEC engineers who did this hardware obviously engaged
	 * in random drug testing.
	 */
	switch (device = PCI_TAG_DEVICE(tag)) {
	case 11:
	case 12:
		kn20aa_slot = (device - 11) + 0;
		break;

	case 7:
		kn20aa_slot = 2;
		break;

	case 8:
		kn20aa_slot = 4;
		break;

	case 9:
		kn20aa_slot = 3;
		break;

	default:
		panic("pci_kn20aa_map_int: invalid device number %d\n",
		    device);
	}

	kn20aa_irq = (kn20aa_slot * 4) + pin - 1;
	if (kn20aa_irq > KN20AA_MAX_IRQ)
		panic("pci_kn20aa_map_int: kn20aa_irq too large (%d)\n",
		    kn20aa_irq);

#if 0
	printf("kn20aa_attach_intr: func 0x%lx, arg 0x%lx, level %d, irq %d\n",
	    func, arg, level, kn20aa_irq);
#endif

	ih = kn20aa_attach_intr(&kn20aa_pci_intrs[kn20aa_irq], level,
	    func, arg);
	kn20aa_enable_intr(kn20aa_irq);
	return (ih);
}

void
kn20aa_pci_unmap_int(pifa, cookie)
	void *pifa;
	void *cookie;
{

	panic("kn20aa_pci_unmap_int not implemented");	/* XXX */
}

/*
 * caught a stray interrupt; notify if not too many seen already.
 */
void
kn20aa_pci_strayintr(irq)
	int irq;
{

	if (++kn20aa_pci_strayintrcnt[irq] <= PCI_STRAY_MAX)
		log(LOG_ERR, "stray PCI interrupt %d%s\n", irq,
		    kn20aa_pci_strayintrcnt[irq] >= PCI_STRAY_MAX ?
		    "; stopped logging" : "");
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
	 * From disassembling the OSF/1 source code:
	 * the following appears to enable a given interrupt request.
	 * "blech."  I'd give valuable body parts for better docs or
	 * for a good decompiler.
	 */
	wbflush();
	REGVAL(0x8780000000L + 0x40L) |= (1 << irq);	/* XXX */
	wbflush();
}

struct kn20aa_intrhand *
kn20aa_attach_intr(chain, level, func, arg)
	struct kn20aa_intrchain *chain;
	pci_intrlevel_t level;
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
