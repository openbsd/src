/*	$OpenBSD: iq80321_pci.c,v 1.2 2006/05/29 17:30:26 drahn Exp $	*/
/*	$NetBSD: iq80321_pci.c,v 1.5 2005/12/11 12:17:09 christos Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IQ80321 PCI interrupt support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <armish/dev/iq80321reg.h>
#include <armish/dev/iq80321var.h>

#include <arm/xscale/i80321reg.h>
#include <arm/xscale/i80321var.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

int	iq80321_pci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
const char *iq80321_pci_intr_string(void *, pci_intr_handle_t);
const struct evcnt *iq80321_pci_intr_evcnt(void *, pci_intr_handle_t);
void	*iq80321_pci_intr_establish(void *, pci_intr_handle_t,
	    int, int (*func)(void *), void *, char *);
void	iq80321_pci_intr_disestablish(void *, void *);

void
iq80321_pci_init(pci_chipset_tag_t pc, void *cookie)
{

	pc->pc_intr_v = cookie;		/* the i80321 softc */
	pc->pc_intr_map = iq80321_pci_intr_map;
	pc->pc_intr_string = iq80321_pci_intr_string;
	pc->pc_intr_establish = iq80321_pci_intr_establish;
	pc->pc_intr_disestablish = iq80321_pci_intr_disestablish;
}

int
iq80321_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct i80321_softc *sc = pa->pa_pc->pc_intr_v;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_intrtag;

	int b, d, f;
	uint32_t busno;
	uint32_t intr;

	/*
	 * The IQ80321's interrupts are routed like so:
	 *
	 *	XINT0	i82544 Gig-E
	 *
	 *	XINT1	UART
	 *
	 *	XINT2	INTA# from S-PCI-X slot
	 *
	 *	XINT3	INTB# from S-PCI-X slot
	 */

	busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	busno = PCIXSR_BUSNO(busno);
	if (busno == 0xff)
		busno = 0;

	pci_decompose_tag(pc, tag, &b, &d, &f);

	/* No mappings for devices not on our bus. */
	if (b != busno)
		goto no_mapping;

	switch (d) {
#if 1
	case 1:			/* theucs re(4) 0 */
		if (pa->pa_intrpin == 1) {
			*ihp = ICU_INT_XINT(2); /* 29 */
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);

			return (0);
		}
		goto no_mapping;
	case 2:			/* theucs re(4) 1 */
		if (pa->pa_intrpin == 1) {
			*ihp = ICU_INT_XINT(2); /* 30 */
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		goto no_mapping;
#endif
	case 3:			/* thecus sata */
		if (pa->pa_intrpin == 1) {
			*ihp = ICU_INT_XINT(2);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		goto no_mapping;
	case 4:			/* thecus */
#if 0
		if (pa->pa_intrpin == 1) {	/* thecus uhci1 */
			*ihp = ICU_INT_XINT(2);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
#endif
#if 0
		if (pa->pa_intrpin == 2) {	/* thecus uhci1 */
			*ihp = ICU_INT_XINT(2);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
#endif
		if (pa->pa_intrpin == 3) {	/* thecus ehci1 */
			*ihp = ICU_INT_XINT(2);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		goto no_mapping;
	case 5:			/* thecus minipci slot */
		if (pa->pa_intrpin == 1) {
			*ihp = ICU_INT_XINT(3);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		goto no_mapping;
	case 6:			/* S-PCI-X slot */
		if (pa->pa_intrpin == 1) {
			*ihp = ICU_INT_XINT(2);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		if (pa->pa_intrpin == 2) {
			*ihp = ICU_INT_XINT(3);
			pci_conf_write(pc, tag, PCI_INTERRUPT_REG, *ihp);
			return (0);
		}
		goto no_mapping;

	default:
 no_mapping:
		intr = pci_conf_read(pa->pa_pc, pa->pa_intrtag,
		    PCI_INTERRUPT_REG);

		printf("iq80321_pci_intr_map: no mapping for %d/%d/%d (%d, %d, %d)\n",
		    pa->pa_bus, pa->pa_device, pa->pa_function, d, pa->pa_intrpin, intr);
		return (1);
	}

	return (0);
}

const char *
iq80321_pci_intr_string(void *v, pci_intr_handle_t ih)
{

	return (i80321_irqnames[ih]);
}

const struct evcnt *
iq80321_pci_intr_evcnt(void *v, pci_intr_handle_t ih)
{

	/* XXX For now. */
	return (NULL);
}

void *
iq80321_pci_intr_establish(void *v, pci_intr_handle_t ih, int ipl,
    int (*func)(void *), void *arg, char *name)
{

	return (i80321_intr_establish(ih, ipl, func, arg, name));
}

void
iq80321_pci_intr_disestablish(void *v, void *cookie)
{

	i80321_intr_disestablish(cookie);
}
