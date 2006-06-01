/*	$OpenBSD: iq80321_pci.c,v 1.4 2006/06/01 03:44:53 drahn Exp $	*/
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

struct irq_map {
	uint8_t	dev;
	uint8_t	intrpin;
	uint8_t	irq;
};


struct pci_id_list {
	uint8_t bus;
	uint8_t dev;
	pci_vendor_id_t	vend;
	pci_product_id_t prod;
};

struct board_id {
	char *name;
	struct irq_map *irq_map;
	struct pci_id_list list[]; 
};

struct irq_map *iq80321_irq_map;

struct irq_map iq80321_thecus_irq_map[] = {
	{ 1, 1, ICU_INT_XINT(2) }, /* thecus re0 29 ??? */
	{ 2, 1, ICU_INT_XINT(2) }, /* thecus re1 29 ??? */
	{ 3, 1, ICU_INT_XINT(2) }, /* thecus sata 29 */
	{ 4, 1, ICU_INT_XINT(2) }, /* thecus uhci0 29 ??? */
	{ 4, 2, ICU_INT_XINT(2) }, /* thecus uhci1 29 ??? */
	{ 4, 3, ICU_INT_XINT(2) }, /* thecus ehci0 29 */
	{ 5, 1, ICU_INT_XINT(2) }, /* thecus minipci slot */

	{ 0, 0, 255}
};

struct irq_map iq80321_hdlg_irq_map[] = {
	{ 1, 1, ICU_INT_XINT(0) }, /* em0 27 ??? */
	{ 2, 1, ICU_INT_XINT(1) }, /* wdc0 28 ??? */
	{ 3, 1, ICU_INT_XINT(2) }, /* ochi0 29 */
	{ 3, 2, ICU_INT_XINT(2) }, /* ochi0 29 */
	{ 3, 3, ICU_INT_XINT(2) }, /* echi0 29 */

	{ 0, 0, 255}
};
struct board_id thecus = {
	"Thecus Nx100",
	iq80321_thecus_irq_map,
	{
	{ 0, 	1,	PCI_VENDOR_REALTEK,	PCI_PRODUCT_REALTEK_RT8169 },
	/* fill in the rest of the devices */
	{ 0, 	0,	0,	0 }
	}
};
struct board_id iodata = {
	"I/O Data HDL-G",
	iq80321_hdlg_irq_map,
	{
	{ 0, 	1,	PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82541GI },
	/* fill in the rest of the devices */
	{ 0, 	0,	0,	0 }
	}
};

struct board_id *systems[] = {
	&thecus,
	&iodata,
	NULL
};

void
iq80321_pci_init(pci_chipset_tag_t pc, void *cookie)
{
	pc->pc_intr_v = cookie;		/* the i80321 softc */
	pc->pc_intr_map = iq80321_pci_intr_map;
	pc->pc_intr_string = iq80321_pci_intr_string;
	pc->pc_intr_establish = iq80321_pci_intr_establish;
	pc->pc_intr_disestablish = iq80321_pci_intr_disestablish;

}
void
iq80321_pci_init2(pci_chipset_tag_t pc, void *cookie)
{
	pcitag_t tag;
	int i, j;
	struct board_id *sys;

	tag = pci_make_tag(pc, 0, 1, 0);

	for (i = 0; systems[i] != NULL; i++) {
		sys = systems[i];
		for (j = 0; sys->list[j].vend != 0; j++) {
			if ((sys->list[j].vend | sys->list[j].prod << 16)  !=
			     pci_conf_read(pc, tag, 0 /* ID */)) {
				sys = NULL;
				break;
			}
		}
		if (sys != NULL)
			break;
	}
	if (sys == NULL)
		printf("board id failed\n");
	else
		printf(": %s", sys->name);
	iq80321_irq_map = sys->irq_map;

}

int
iq80321_pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct i80321_softc *sc = pa->pa_pc->pc_intr_v;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_intrtag;
	int i;

	int b, d, f;
	uint32_t busno;
	uint32_t intr;

	busno = bus_space_read_4(sc->sc_st, sc->sc_atu_sh, ATU_PCIXSR);
	busno = PCIXSR_BUSNO(busno);
	if (busno == 0xff)
		busno = 0;

	pci_decompose_tag(pc, tag, &b, &d, &f);

	/* No mappings for devices not on our bus. */
	if (b != busno)
		goto no_mapping;

	for (i = 0; iq80321_irq_map[i].irq != 255; i++) {
		if (d == iq80321_irq_map[i].dev &&
		    pa->pa_intrpin == iq80321_irq_map[i].intrpin) {
			*ihp = iq80321_irq_map[i].irq;
			intr = pci_conf_read(pa->pa_pc, pa->pa_intrtag,
			    PCI_INTERRUPT_REG);
			intr = (intr & ~0xff) | iq80321_irq_map[i].irq;
			pci_conf_write(pa->pa_pc, pa->pa_intrtag,
			    PCI_INTERRUPT_REG, intr);
			return (0);
		}
	}

 no_mapping:
	intr = pci_conf_read(pa->pa_pc, pa->pa_intrtag,
	    PCI_INTERRUPT_REG);

	printf("iq80321_pci_intr_map: no mapping for %d/%d/%d (%d, %d, %d)\n",
	    pa->pa_bus, pa->pa_device, pa->pa_function, d, pa->pa_intrpin, intr);
	return (1);
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
