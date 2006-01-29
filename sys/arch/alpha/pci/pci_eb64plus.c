/* $OpenBSD: pci_eb64plus.c,v 1.7 2006/01/29 10:47:35 martin Exp $ */
/* $NetBSD: pci_eb64plus.c,v 1.10 2001/07/27 00:25:20 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

#include <alpha/pci/pci_eb64plus.h>

#include "sio.h"
#if NSIO
#include <alpha/pci/siovar.h>
#endif

int	dec_eb64plus_intr_map(void *, pcitag_t, int, int,
	    pci_intr_handle_t *);
const char *dec_eb64plus_intr_string(void *, pci_intr_handle_t);
void	*dec_eb64plus_intr_establish(void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *, char *);
void	dec_eb64plus_intr_disestablish(void *, void *);

#define	EB64PLUS_MAX_IRQ	32
#define	PCI_STRAY_MAX		5

struct alpha_shared_intr *eb64plus_pci_intr;

bus_space_tag_t eb64plus_intrgate_iot;
bus_space_handle_t eb64plus_intrgate_ioh;

void	eb64plus_iointr(void *arg, unsigned long vec);
extern void	eb64plus_intr_enable(int irq);  /* pci_eb64plus_intr.S */
extern void	eb64plus_intr_disable(int irq); /* pci_eb64plus_intr.S */

void
pci_eb64plus_pickintr(acp)
	struct apecs_config *acp;
{
	bus_space_tag_t iot = &acp->ac_iot;
	pci_chipset_tag_t pc = &acp->ac_pc;
	int i;

        pc->pc_intr_v = acp;
        pc->pc_intr_map = dec_eb64plus_intr_map;
        pc->pc_intr_string = dec_eb64plus_intr_string;
        pc->pc_intr_establish = dec_eb64plus_intr_establish;
        pc->pc_intr_disestablish = dec_eb64plus_intr_disestablish;

	/* Not supported on the EB64+. */
	pc->pc_pciide_compat_intr_establish = NULL;

	eb64plus_intrgate_iot = iot;
	if (bus_space_map(eb64plus_intrgate_iot, 0x804, 3, 0,
	    &eb64plus_intrgate_ioh) != 0)
		panic("pci_eb64plus_pickintr: couldn't map interrupt PLD");
	for (i = 0; i < EB64PLUS_MAX_IRQ; i++)
		eb64plus_intr_disable(i);	

	eb64plus_pci_intr = alpha_shared_intr_alloc(EB64PLUS_MAX_IRQ);
	for (i = 0; i < EB64PLUS_MAX_IRQ; i++) {
		alpha_shared_intr_set_maxstrays(eb64plus_pci_intr, i,
			PCI_STRAY_MAX);
	}

#if NSIO
	sio_intr_setup(pc, iot);
#endif
	set_iointr(eb64plus_iointr);
}

int     
dec_eb64plus_intr_map(acv, bustag, buspin, line, ihp)
	void *acv;
	pcitag_t bustag;
	int buspin, line;
        pci_intr_handle_t *ihp;
{
	struct apecs_config *acp = acv;
	pci_chipset_tag_t pc = &acp->ac_pc;
	int bus, device, function;

	if (buspin == 0) {
		/* No IRQ used. */
		return 1;
	}
	if (buspin > 4) {
		printf("dec_eb64plus_intr_map: bad interrupt pin %d\n", buspin);
		return 1;
	}

	alpha_pci_decompose_tag(pc, bustag, &bus, &device, &function);

	/*
	 * The console places the interrupt mapping in the "line" value.
	 * A value of (char)-1 indicates there is no mapping.
	 */
	if (line == 0xff) {
		printf("dec_eb64plus_intr_map: no mapping for %d/%d/%d\n",
		    bus, device, function);
		return (1);
	}

	if (line >= EB64PLUS_MAX_IRQ)
		panic("dec_eb64plus_intr_map: eb64+ irq too large (%d)",
		    line);

	*ihp = line;
	return (0);
}

const char *
dec_eb64plus_intr_string(acv, ih)
	void *acv;
	pci_intr_handle_t ih;
{
        static char irqstr[15];          /* 11 + 2 + NULL + sanity */

        if (ih > EB64PLUS_MAX_IRQ)
                panic("dec_eb64plus_intr_string: bogus eb64+ IRQ 0x%lx", ih);
        snprintf(irqstr, sizeof irqstr, "eb64+ irq %ld", ih);
        return (irqstr);
}

void *
dec_eb64plus_intr_establish(acv, ih, level, func, arg, name)
        void *acv;
        pci_intr_handle_t ih;
        int level;
        int (*func)(void *);
	void *arg;
	char *name;
{
	void *cookie;

	if (ih > EB64PLUS_MAX_IRQ)
		panic("dec_eb64plus_intr_establish: bogus eb64+ IRQ 0x%lx",
		    ih);

	cookie = alpha_shared_intr_establish(eb64plus_pci_intr, ih, IST_LEVEL,
	    level, func, arg, name);

	if (cookie != NULL && alpha_shared_intr_isactive(eb64plus_pci_intr, ih))
		eb64plus_intr_enable(ih);

	return (cookie);
}

void
dec_eb64plus_intr_disestablish(acv, cookie)
        void *acv, *cookie;
{
	struct alpha_shared_intrhand *ih = cookie;
	unsigned int irq = ih->ih_num;
	int s;
 
	s = splhigh();

	alpha_shared_intr_disestablish(eb64plus_pci_intr, cookie,
	    "eb64+ irq");
	if (alpha_shared_intr_isactive(eb64plus_pci_intr, irq) == 0) {
		eb64plus_intr_disable(irq);
		alpha_shared_intr_set_dfltsharetype(eb64plus_pci_intr, irq,
		    IST_NONE);
	}
 
	splx(s);
}

void
eb64plus_iointr(framep, vec)
	void *framep;
	unsigned long vec;
{
	int irq; 

	if (vec >= 0x900) {
		if (vec >= 0x900 + (EB64PLUS_MAX_IRQ << 4))
			panic("eb64plus_iointr: vec 0x%lx out of range", vec);
		irq = (vec - 0x900) >> 4;

		if (!alpha_shared_intr_dispatch(eb64plus_pci_intr, irq)) {
		    alpha_shared_intr_stray(eb64plus_pci_intr, irq,
			"eb64+ irq");
			if (ALPHA_SHARED_INTR_DISABLE(eb64plus_pci_intr, irq))
				eb64plus_intr_disable(irq);
		} else
			alpha_shared_intr_reset_strays(eb64plus_pci_intr, irq);
		return;
	}
#if NSIO
	if (vec >= 0x800) {
		sio_iointr(framep, vec);
		return;
	}
#endif
	panic("eb64plus_iointr: weird vec 0x%lx", vec);
}

#if 0		/* THIS DOES NOT WORK!  see pci_eb64plus_intr.S. */
u_int8_t eb64plus_intr_mask[3] = { 0xff, 0xff, 0xff };

void
eb64plus_intr_enable(irq)
	int irq;
{
	int byte = (irq / 8), bit = (irq % 8);

#if 1
	printf("eb64plus_intr_enable: enabling %d (%d:%d)\n", irq, byte, bit);
#endif
	eb64plus_intr_mask[byte] &= ~(1 << bit);

	bus_space_write_1(eb64plus_intrgate_iot, eb64plus_intrgate_ioh, byte,
	    eb64plus_intr_mask[byte]);
}

void
eb64plus_intr_disable(irq)
	int irq;
{
	int byte = (irq / 8), bit = (irq % 8);

#if 1
	printf("eb64plus_intr_disable: disabling %d (%d:%d)\n", irq, byte, bit);
#endif
	eb64plus_intr_mask[byte] |= (1 << bit);

	bus_space_write_1(eb64plus_intrgate_iot, eb64plus_intrgate_ioh, byte,
	    eb64plus_intr_mask[byte]);
}
#endif
