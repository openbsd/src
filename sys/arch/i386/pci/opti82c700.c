/*	$OpenBSD: opti82c700.c,v 1.3 2000/03/28 03:37:59 mickey Exp $	*/
/*	$NetBSD: opti82c700.c,v 1.1 1999/11/17 01:21:20 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/*
 * Support for the Opti 82c700 PCI-ISA bridge interrupt controller.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <i386/pci/pci_intr_fixup.h>
#include <i386/pci/opti82c700reg.h>

int	opti82c700_getclink __P((pciintr_icu_handle_t, int, int *));
int	opti82c700_get_intr __P((pciintr_icu_handle_t, int, int *));
int	opti82c700_set_intr __P((pciintr_icu_handle_t, int, int));
int	opti82c700_get_trigger __P((pciintr_icu_handle_t, int, int *));
int	opti82c700_set_trigger __P((pciintr_icu_handle_t, int, int));

const struct pciintr_icu opti82c700_pci_icu = {
	opti82c700_getclink,
	opti82c700_get_intr,
	opti82c700_set_intr,
	opti82c700_get_trigger,
	opti82c700_set_trigger,
};

struct opti82c700_handle {
	pci_chipset_tag_t ph_pc;
	pcitag_t ph_tag;
};

int	opti82c700_addr __P((int, int *, int *));

int
opti82c700_init(pc, iot, tag, ptagp, phandp)
	pci_chipset_tag_t pc;
	bus_space_tag_t iot;
	pcitag_t tag;
	pciintr_icu_tag_t *ptagp;
	pciintr_icu_handle_t *phandp;
{
	struct opti82c700_handle *ph;

	ph = malloc(sizeof(*ph), M_DEVBUF, M_NOWAIT);
	if (ph == NULL)
		return (1);

	ph->ph_pc = pc;
	ph->ph_tag = tag;

	*ptagp = &opti82c700_pci_icu;
	*phandp = ph;
	return (0);
}

int
opti82c700_addr(link, addrofs, ofs)
	int link, *addrofs, *ofs;
{
	int regofs, src;

	regofs = FIRESTAR_PIR_REGOFS(link);
	src = FIRESTAR_PIR_SELECTSRC(link);

	switch (src) {
	case FIRESTAR_PIR_SELECT_NONE:
		return (1);

	case FIRESTAR_PIR_SELECT_IRQ:
		if (regofs < 0 || regofs > 7)
			return (1);
		*addrofs = FIRESTAR_CFG_INTR_IRQ + (regofs >> 2);
		*ofs = (regofs & 3) << 3;
		break;

	case FIRESTAR_PIR_SELECT_PIRQ:
	case FIRESTAR_PIR_SELECT_BRIDGE:
		if (regofs < 0 || regofs > 3)
			return (1);
		*addrofs = FIRESTAR_CFG_INTR_PIRQ;
		*ofs = regofs << 2;
		break;

	default:
		return (1);
	}

	return (0);
}

int
opti82c700_getclink(v, link, clinkp)
	pciintr_icu_handle_t v;
	int link, *clinkp;
{

	if (FIRESTAR_LEGAL_LINK(link)) {
		*clinkp = link;
		return (0);
	}

	return (1);
}

int
opti82c700_get_intr(v, clink, irqp)
	pciintr_icu_handle_t v;
	int clink, *irqp;
{
	struct opti82c700_handle *ph = v;
	pcireg_t reg;
	int val, addrofs, ofs;

	if (FIRESTAR_LEGAL_LINK(clink) == 0)
		return (1);

	if (opti82c700_addr(clink, &addrofs, &ofs))
		return (1);

	reg = pci_conf_read(ph->ph_pc, ph->ph_tag, addrofs);
	val = (reg >> ofs) & FIRESTAR_CFG_PIRQ_MASK;
	*irqp = (val == FIRESTAR_PIRQ_NONE) ? 0xff : val;

	return (0);
}

int
opti82c700_set_intr(v, clink, irq)
	pciintr_icu_handle_t v;
	int clink, irq;
{
	struct opti82c700_handle *ph = v;
	int addrofs, ofs;
	pcireg_t reg;

	if (FIRESTAR_LEGAL_LINK(clink) == 0 || FIRESTAR_LEGAL_IRQ(irq) == 0)
		return (1);

	if (opti82c700_addr(clink, &addrofs, &ofs))
		return (1);

	reg = pci_conf_read(ph->ph_pc, ph->ph_tag, addrofs);
	reg &= ~(FIRESTAR_CFG_PIRQ_MASK << ofs);
	reg |= (irq << ofs);
	pci_conf_write(ph->ph_pc, ph->ph_tag, addrofs, reg);

	return (0);
}

int
opti82c700_get_trigger(v, irq, triggerp)
	pciintr_icu_handle_t v;
	int irq, *triggerp;
{
	struct opti82c700_handle *ph = v;
	int i, val, addrofs, ofs;
	pcireg_t reg;

	if (FIRESTAR_LEGAL_IRQ(irq) == 0) {
		/* ISA IRQ? */
		*triggerp = IST_EDGE;
		return (0);
	}

	/*
	 * Search PCIDV1 registers.
	 */
	for (i = 0; i < 8; i++) {
		opti82c700_addr(FIRESTAR_PIR_MAKELINK(FIRESTAR_PIR_SELECT_IRQ,
		    i), &addrofs, &ofs);
		reg = pci_conf_read(ph->ph_pc, ph->ph_tag, addrofs);
		val = (reg >> ofs) & FIRESTAR_CFG_PIRQ_MASK;
		if (val != irq)
			continue;
		val = ((reg >> ofs) >> FIRESTAR_TRIGGER_SHIFT) &
		    FIRESTAR_TRIGGER_MASK;
		*triggerp = val ? IST_LEVEL : IST_EDGE;
		return (0);
	}

	return (1);
}

int
opti82c700_set_trigger(v, irq, trigger)
	pciintr_icu_handle_t v;
	int irq, trigger;
{
	struct opti82c700_handle *ph = v;
	int i, val, addrofs, ofs;
	pcireg_t reg;

	if (FIRESTAR_LEGAL_IRQ(irq) == 0) {
		/* ISA IRQ? */
		return ((trigger != IST_LEVEL) ? 0 : 1);
	}

	/*
	 * Search PCIDV1 registers.
	 */
	for (i = 0; i < 8; i++) {
		opti82c700_addr(FIRESTAR_PIR_MAKELINK(FIRESTAR_PIR_SELECT_IRQ,
		    i), &addrofs, &ofs);
		reg = pci_conf_read(ph->ph_pc, ph->ph_tag, addrofs);
		val = (reg >> ofs) & FIRESTAR_CFG_PIRQ_MASK;
		if (val != irq)
			continue;
		if (trigger == IST_LEVEL)
			reg |= (FIRESTAR_TRIGGER_MASK <<
			    (FIRESTAR_TRIGGER_SHIFT + ofs));
		else
			reg &= ~(FIRESTAR_TRIGGER_MASK <<
			    (FIRESTAR_TRIGGER_SHIFT + ofs));
		pci_conf_write(ph->ph_pc, ph->ph_tag, addrofs, reg);
		return (0);
	}

	return (1);
}
