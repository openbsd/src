/* $OpenBSD: pci_550.c,v 1.14 2006/01/29 10:47:35 martin Exp $ */
/* $NetBSD: pci_550.c,v 1.18 2000/06/29 08:58:48 mrg Exp $ */

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Andrew Gallatin.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#include <alpha/pci/pci_550.h>

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

int	dec_550_intr_map(void *, pcitag_t, int, int,
	    pci_intr_handle_t *);
const char *dec_550_intr_string(void *, pci_intr_handle_t);
int	dec_550_intr_line(void *, pci_intr_handle_t);
void	*dec_550_intr_establish(void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *, char *);
void	dec_550_intr_disestablish(void *, void *);

void	*dec_550_pciide_compat_intr_establish(void *, struct device *,
	    struct pci_attach_args *, int, int (*)(void *), void *);
void    dec_550_pciide_compat_intr_disestablish(void *, void *);

#define	DEC_550_PCI_IRQ_BEGIN	8
#define	DEC_550_MAX_IRQ		(64 - DEC_550_PCI_IRQ_BEGIN)

/*
 * The Miata has a Pyxis, which seems to have problems with stray
 * interrupts.  Work around this by just ignoring strays.
 */
#define	PCI_STRAY_MAX		0

/*
 * Some Miata models, notably models with a Cypress PCI-ISA bridge, have
 * a PCI device (the OHCI USB controller) with interrupts tied to ISA IRQ
 * lines.  This IRQ is encoded as: line = FLAG | isa_irq. Usually FLAG
 * is 0xe0, however it can be 0xf0.  We don't allow 0xf0 | irq15.
 */
#define	DEC_550_LINE_IS_ISA(line)	((line) >= 0xe0 && (line) <= 0xfe)
#define	DEC_550_LINE_ISA_IRQ(line)	((line) & 0x0f)

struct alpha_shared_intr *dec_550_pci_intr;

void	dec_550_iointr(void *framep, unsigned long vec);
void	dec_550_intr_enable(int irq);
void	dec_550_intr_disable(int irq);

void
pci_550_pickintr(ccp)
	struct cia_config *ccp;
{
	bus_space_tag_t iot = &ccp->cc_iot;
	pci_chipset_tag_t pc = &ccp->cc_pc;
#if 0
	char *cp;
#endif
	int i;

        pc->pc_intr_v = ccp;
        pc->pc_intr_map = dec_550_intr_map;
        pc->pc_intr_string = dec_550_intr_string;
        pc->pc_intr_line = dec_550_intr_line;
        pc->pc_intr_establish = dec_550_intr_establish;
        pc->pc_intr_disestablish = dec_550_intr_disestablish;

	pc->pc_pciide_compat_intr_establish =
	    dec_550_pciide_compat_intr_establish;
	pc->pc_pciide_compat_intr_disestablish =
	    dec_550_pciide_compat_intr_disestablish;

	/*
	 * DEC 550's interrupts are enabled via the Pyxis interrupt
	 * mask register.  Nothing to map.
	 */

	for (i = 0; i < DEC_550_MAX_IRQ; i++)
		dec_550_intr_disable(i);

	dec_550_pci_intr = alpha_shared_intr_alloc(DEC_550_MAX_IRQ);
	for (i = 0; i < DEC_550_MAX_IRQ; i++) {
		alpha_shared_intr_set_maxstrays(dec_550_pci_intr, i,
		    PCI_STRAY_MAX);
		alpha_shared_intr_set_private(dec_550_pci_intr, i, ccp);
	}

#if NSIO
	sio_intr_setup(pc, iot);
#endif

	set_iointr(dec_550_iointr);
}

int     
dec_550_intr_map(ccv, bustag, buspin, line, ihp)
        void *ccv;
        pcitag_t bustag; 
        int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct cia_config *ccp = ccv;
	pci_chipset_tag_t pc = &ccp->cc_pc;
	int bus, device, function;

	if (buspin == 0) {
		/* No IRQ used. */
		return 1;
	}
	if (buspin > 4) {
		printf("dec_550_intr_map: bad interrupt pin %d\n", buspin);
		return 1;
	}

	alpha_pci_decompose_tag(pc, bustag, &bus, &device, &function);

	/*
	 * There are two main variants of Miata: Miata 1 (Intel SIO)
	 * and Miata {1.5,2} (Cypress).
	 *
	 * The Miata 1 has a CMD PCI IDE wired to compatibility mode at
	 * device 4 of bus 0.  This variant apparently also has the
	 * Pyxis DMA bug.
	 *
	 * On the Miata 1.5 and Miata 2, the Cypress PCI-ISA bridge lives
	 * on device 7 of bus 0.  This device has PCI IDE wired to
	 * compatibility mode on functions 1 and 2.
	 *
	 * There will be no interrupt mapping for these devices, so just
	 * bail out now.
	 */
	if (bus == 0) {
		if ((hwrpb->rpb_variation & SV_ST_MASK) < SV_ST_MIATA_1_5) {
			/* Miata 1 */
			if (device == 7)
				panic("dec_550_intr_map: SIO device");
			else if (device == 4)
				return (1);
		} else {
			/* Miata 1.5 or Miata 2 */
			if (device == 7) {
				if (function == 0)
					panic("dec_550_intr_map: SIO device");
				if (function == 1 || function == 2)
					return (1);
			}
		}
	}

	/*
	 * The console places the interrupt mapping in the "line" value.
	 * A value of (char)-1 indicates there is no mapping.
	 */
	if (line == 0xff) {
		printf("dec_550_intr_map: no mapping for %d/%d/%d\n",
		    bus, device, function);
		return (1);
	}

#if NSIO == 0
	if (DEC_550_LINE_IS_ISA(line)) {
		printf("dec_550_intr_map: ISA IRQ %d for %d/%d/%d\n",
		    DEC_550_LINE_ISA_IRQ(line), bus, device, function);
		return (1);
	}
#endif

	if (DEC_550_LINE_IS_ISA(line) == 0 && line >= DEC_550_MAX_IRQ) {
		printf("dec_550_intr_map: dec 550 irq too large (%d)",
		    line);
		return (1);
	}

	*ihp = line;
	return (0);
}

const char *
dec_550_intr_string(ccv, ih)
	void *ccv;
	pci_intr_handle_t ih;
{
#if 0
	struct cia_config *ccp = ccv;
#endif
	static char irqstr[16];		/* 12 + 2 + NULL + sanity */

#if NSIO
	if (DEC_550_LINE_IS_ISA(ih))
		return (sio_intr_string(NULL /*XXX*/,
		    DEC_550_LINE_ISA_IRQ(ih)));
#endif

	if (ih >= DEC_550_MAX_IRQ)
		panic("dec_550_intr_string: bogus 550 IRQ 0x%lx", ih);
	snprintf(irqstr, sizeof irqstr, "dec 550 irq %ld", ih);
	return (irqstr);
}

int
dec_550_intr_line(ccv, ih)
	void *ccv;
	pci_intr_handle_t ih;
{
#if NSIO
	if (DEC_550_LINE_IS_ISA(ih))
		return (sio_intr_line(NULL /*XXX*/, DEC_550_LINE_ISA_IRQ(ih)));
#endif

	return (ih);
}

void *
dec_550_intr_establish(ccv, ih, level, func, arg, name)
	void *ccv, *arg;
	pci_intr_handle_t ih;
	int level;
	int (*func)(void *);
	char *name;
{
#if 0
	struct cia_config *ccp = ccv;
#endif
	void *cookie;

#if NSIO
	if (DEC_550_LINE_IS_ISA(ih))
		return (sio_intr_establish(NULL /*XXX*/,
		    DEC_550_LINE_ISA_IRQ(ih), IST_LEVEL, level, func, arg,
		    name));
#endif

	if (ih >= DEC_550_MAX_IRQ)
		panic("dec_550_intr_establish: bogus dec 550 IRQ 0x%lx", ih);

	cookie = alpha_shared_intr_establish(dec_550_pci_intr, ih, IST_LEVEL,
	    level, func, arg, name);

	if (cookie != NULL && alpha_shared_intr_isactive(dec_550_pci_intr, ih))
		dec_550_intr_enable(ih);
	return (cookie);
}

void
dec_550_intr_disestablish(ccv, cookie)
        void *ccv, *cookie;
{
	struct cia_config *ccp = ccv;
	struct alpha_shared_intrhand *ih = cookie;
	unsigned int irq = ih->ih_num;
	int s;

#if NSIO
	/*
	 * We have to determine if this is an ISA IRQ or not!  We do this
	 * by checking to see if the intrhand points back to an intrhead
	 * that points to our cia_config.  If not, it's an ISA IRQ.  Pretty
	 * disgusting, eh?
	 */
	if (ih->ih_intrhead->intr_private != ccp) {
		sio_intr_disestablish(NULL /*XXX*/, cookie);
		return;
	}
#endif
 
	s = splhigh();

	alpha_shared_intr_disestablish(dec_550_pci_intr, cookie,
	    "dec 550 irq");
	if (alpha_shared_intr_isactive(dec_550_pci_intr, irq) == 0) {
		dec_550_intr_disable(irq);
		alpha_shared_intr_set_dfltsharetype(dec_550_pci_intr, irq,
		    IST_NONE);
	}
 
	splx(s);
}

void *
dec_550_pciide_compat_intr_establish(v, dev, pa, chan, func, arg)
	void *v;
	struct device *dev;
	struct pci_attach_args *pa;
	int chan;
	int (*func)(void *);
	void *arg;
{
	pci_chipset_tag_t pc = pa->pa_pc;
	void *cookie = NULL;
	int bus, irq;

	alpha_pci_decompose_tag(pc, pa->pa_tag, &bus, NULL, NULL);

	/*
	 * If this isn't PCI bus #0, all bets are off.
	 */
	if (bus != 0)
		return (NULL);

	irq = PCIIDE_COMPAT_IRQ(chan);
#if NSIO
	cookie = sio_intr_establish(NULL /*XXX*/, irq, IST_EDGE, IPL_BIO,
	    func, arg, "dec 550 irq");
#endif
	return (cookie);
}

void
dec_550_pciide_compat_intr_disestablish(v, cookie)
	void *v;
	void *cookie;
{
	sio_intr_disestablish(NULL, cookie);
}

void
dec_550_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq; 

	if (vec >= 0x900) {
		irq = ((vec - 0x900) >> 4);

		if (irq >= DEC_550_MAX_IRQ)
			panic("550_iointr: vec 0x%lx out of range", vec);

		if (!alpha_shared_intr_dispatch(dec_550_pci_intr, irq)) {
			alpha_shared_intr_stray(dec_550_pci_intr, irq,
			    "dec 550 irq");
			if (ALPHA_SHARED_INTR_DISABLE(dec_550_pci_intr, irq))
				dec_550_intr_disable(irq);
		} else
			alpha_shared_intr_reset_strays(dec_550_pci_intr, irq);
		return;
	}
#if NSIO
	if (vec >= 0x800) {
		sio_iointr(framep, vec);
		return;
	}
#endif
	panic("dec_550_iointr: weird vec 0x%lx", vec);
}

void
dec_550_intr_enable(irq)
	int irq;
{

	cia_pyxis_intr_enable(irq + DEC_550_PCI_IRQ_BEGIN, 1);
}

void
dec_550_intr_disable(irq)
	int irq;
{

	cia_pyxis_intr_enable(irq + DEC_550_PCI_IRQ_BEGIN, 0);
}
