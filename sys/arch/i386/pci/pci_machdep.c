/*	$OpenBSD: pci_machdep.c,v 1.26 2004/12/23 17:43:18 aaron Exp $	*/
/*	$NetBSD: pci_machdep.c,v 1.28 1997/06/06 23:29:17 thorpej Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Machine-specific functions for PCI autoconfiguration.
 *
 * On PCs, there are two methods of generating PCI configuration cycles.
 * We try to detect the appropriate mechanism for this machine and set
 * up a few function pointers to access the correct method directly.
 *
 * The configuration method can be hard-coded in the config file by
 * using `options PCI_CONF_MODE=N', where `N' is the configuration mode
 * as defined section 3.6.4.1, `Generating Configuration Cycles'.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#define _I386_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/pio.h>
#include <machine/i8259.h>

#include "bios.h"
#if NBIOS > 0
#include <machine/biosvar.h>
extern bios_pciinfo_t *bios_pciinfo;
#endif

#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>
#endif

#include "pcibios.h"
#if NPCIBIOS > 0
#include <i386/pci/pcibiosvar.h>
#endif

int pci_mode = -1;

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

#define	PCI_MODE2_ENABLE_REG	0x0cf8
#define	PCI_MODE2_FORWARD_REG	0x0cfa

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct i386_bus_dma_tag pci_bus_dma_tag = {
	NULL,			/* _cookie */
	_bus_dmamap_create, 
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	NULL,			/* _dmamap_sync */
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

void
pci_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{

#if NBIOS > 0
	if (pba->pba_bus == 0)
		printf(": configuration mode %d (%s)",
			pci_mode, (bios_pciinfo?"bios":"no bios"));
#else
	if (pba->pba_bus == 0)
		printf(": configuration mode %d", pci_mode);
#endif
}

int
pci_bus_maxdevs(pc, busno)
	pci_chipset_tag_t pc;
	int busno;
{

	/*
	 * Bus number is irrelevant.  If Configuration Mechanism 2 is in
	 * use, can only have devices 0-15 on any bus.  If Configuration
	 * Mechanism 1 is in use, can have devices 0-32 (i.e. the `normal'
	 * range).
	 */
	if (pci_mode == 2)
		return (16);
	else
		return (32);
}

pcitag_t
pci_make_tag(pc, bus, device, function)
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	pcitag_t tag;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_make_tag: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
#ifndef PCI_CONF_MODE
mode1:
#endif
	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode1 = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);
	return tag;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
#ifndef PCI_CONF_MODE
mode2:
#endif
	if (bus >= 256 || device >= 16 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode2.port = 0xc000 | (device << 8);
	tag.mode2.enable = 0xf0 | (function << 1);
	tag.mode2.forward = bus;
	return tag;
#endif
}

void
pci_decompose_tag(pc, tag, bp, dp, fp)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int *bp, *dp, *fp;
{

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_decompose_tag: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
#ifndef PCI_CONF_MODE
mode1:
#endif
	if (bp != NULL)
		*bp = (tag.mode1 >> 16) & 0xff;
	if (dp != NULL)
		*dp = (tag.mode1 >> 11) & 0x1f;
	if (fp != NULL)
		*fp = (tag.mode1 >> 8) & 0x7;
	return;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
#ifndef PCI_CONF_MODE
mode2:
#endif
	if (bp != NULL)
		*bp = tag.mode2.forward & 0xff;
	if (dp != NULL)
		*dp = (tag.mode2.port >> 8) & 0xf;
	if (fp != NULL)
		*fp = (tag.mode2.enable >> 1) & 0x7;
#endif
}

pcireg_t
pci_conf_read(pc, tag, reg)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
{
	pcireg_t data;

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_read: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
#ifndef PCI_CONF_MODE
mode1:
#endif
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return data;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
#ifndef PCI_CONF_MODE
mode2:
#endif
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	data = inl(tag.mode2.port | reg);
	outb(PCI_MODE2_ENABLE_REG, 0);
	return data;
#endif
}

void
pci_conf_write(pc, tag, reg, data)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg;
	pcireg_t data;
{

#ifndef PCI_CONF_MODE
	switch (pci_mode) {
	case 1:
		goto mode1;
	case 2:
		goto mode2;
	default:
		panic("pci_conf_write: mode not configured");
	}
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 1)
#ifndef PCI_CONF_MODE
mode1:
#endif
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	outl(PCI_MODE1_DATA_REG, data);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
#ifndef PCI_CONF_MODE
mode2:
#endif
	outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
	outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
	outl(tag.mode2.port | reg, data);
	outb(PCI_MODE2_ENABLE_REG, 0);
#endif
}

int
pci_mode_detect()
{

#ifdef PCI_CONF_MODE
#if (PCI_CONF_MODE == 1) || (PCI_CONF_MODE == 2)
	return (pci_mode = PCI_CONF_MODE);
#else
#error Invalid PCI configuration mode.
#endif
#else
	if (pci_mode != -1)
		return pci_mode;

#if NBIOS > 0
	/*
	 * If we have PCI info passed from the BIOS, use the mode given there
	 * for all of this code.  If not, pass on through to the previous tests
	 * to try and devine the correct mode.
	 */
	if (bios_pciinfo != NULL) {
		if (bios_pciinfo->pci_chars & 0x2)
			return (pci_mode = 2);

		if (bios_pciinfo->pci_chars & 0x1)
			return (pci_mode = 1);

		/* We should never get here, but if we do, fall through... */
	}
#endif

	/*
	 * We try to divine which configuration mode the host bridge wants.  We
	 * try mode 2 first, because our probe for mode 1 is likely to succeed
	 * for mode 2 also.
	 *
	 * This should really be done using the PCI BIOS.  If we get here, the
	 * PCI BIOS does not exist, or the boot blocks did not provide the
	 * information.
	 */
	outb(PCI_MODE2_ENABLE_REG, 0);
	outb(PCI_MODE2_FORWARD_REG, 0);
	if (inb(PCI_MODE2_ENABLE_REG) != 0 ||
	    inb(PCI_MODE2_FORWARD_REG) != 0)
		goto not2;
	return (pci_mode = 2);

not2:
	outl(PCI_MODE1_ADDRESS_REG, PCI_MODE1_ENABLE);
	if (inl(PCI_MODE1_ADDRESS_REG) != PCI_MODE1_ENABLE)
		goto not1;
	outl(PCI_MODE1_ADDRESS_REG, 0);
	if (inl(PCI_MODE1_ADDRESS_REG) != 0)
		goto not1;
	return (pci_mode = 1);

not1:
	return (pci_mode = 0);
#endif
}

int
pci_intr_map(pa, ihp)
	struct pci_attach_args *pa;
	pci_intr_handle_t *ihp;
{
#if NIOAPIC > 0
	struct mp_intr_map *mip;
	int bus, dev, func;
#endif

#if (NPCIBIOS > 0) || (NIOAPIC > 0)
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t intrtag = pa->pa_intrtag;
#endif
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	if (pin > 4) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

	ihp->line = line;
	ihp->pin = pin;
#if NPCIBIOS > 0
	pci_intr_header_fixup(pc, intrtag, ihp);
	line = ihp->line;
#endif

	/*
	 * Section 6.2.4, `Miscellaneous Functions', says that 255 means
	 * `unknown' or `no connection' on a PC.  We assume that a device with
	 * `no connection' either doesn't have an interrupt (in which case the
	 * pin number should be 0, and would have been noticed above), or
	 * wasn't configured by the BIOS (in which case we punt, since there's
	 * no real way we can know how the interrupt lines are mapped in the
	 * hardware).
	 *
	 * XXX
	 * Since IRQ 0 is only used by the clock, and we can't actually be sure
	 * that the BIOS did its job, we also recognize that as meaning that
	 * the BIOS has not configured the device.
	 */
	if (line == 0 || line == 255) {
		printf("pci_intr_map: no mapping for pin %c\n", '@' + pin);
		goto bad;
	} else {
		if (line >= ICU_LEN) {
			printf("pci_intr_map: bad interrupt line %d\n", line);
			goto bad;
		}
		if (line == 2) {
			printf("pci_intr_map: changed line 2 to line 9\n");
			line = 9;
		}
	}
#if NIOAPIC > 0
	pci_decompose_tag (pc, intrtag, &bus, &dev, &func);

	if (mp_busses != NULL) {
		/*
		 * Assumes 1:1 mapping between PCI bus numbers and
		 * the numbers given by the MP bios.
		 * XXX Is this a valid assumption?
		 */
		int mpspec_pin = (dev<<2)|(pin-1);

		for (mip = mp_busses[bus].mb_intrs; mip != NULL; mip=mip->next) {
			if (mip->bus_pin == mpspec_pin) {
				ihp->line = mip->ioapic_ih | line;
				return 0;
			}
		}
		if (mip == NULL && mp_isa_bus != -1) {
			for (mip = mp_busses[mp_isa_bus].mb_intrs; mip != NULL;
			    mip=mip->next) {
				if (mip->bus_pin == line) {
					ihp->line = mip->ioapic_ih | line;
					return 0;
				}
			}
		}
		if (mip == NULL && mp_eisa_bus != -1) {
			for (mip = mp_busses[mp_eisa_bus].mb_intrs;
			    mip != NULL; mip=mip->next) {
				if (mip->bus_pin == line) {
					ihp->line = mip->ioapic_ih | line;
					return 0;
				}
			}
		}
		if (mip == NULL) {
			printf("pci_intr_map: "
			    "bus %d dev %d func %d pin %d; line %d\n",
			    bus, dev, func, pin, line);
			printf("pci_intr_map: no MP mapping found\n");
		}
	}
#endif

	return 0;

bad:
	ihp->line = -1;
	return 1;
}

const char *
pci_intr_string(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{
	static char irqstr[64];

	if (ih.line == 0 || (ih.line  & 0xff) >= ICU_LEN || ih.line == 2)
		panic("pci_intr_string: bogus handle 0x%x", ih.line);

#if NIOAPIC > 0
	if (ih.line & APIC_INT_VIA_APIC) {
		snprintf(irqstr, sizeof irqstr, "apic %d int %d (irq %d)",
		     APIC_IRQ_APIC(ih.line), APIC_IRQ_PIN(ih.line),
		     ih.line & 0xff);
		return (irqstr);
	}
#endif

	snprintf(irqstr, sizeof irqstr, "irq %d", ih.line);
	return (irqstr);
}

void *
pci_intr_establish(pc, ih, level, func, arg, what)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	int level, (*func)(void *);
	void *arg;
	char *what;
{
	void *ret;

#if NIOAPIC > 0
	if (ih.line != -1 && ih.line & APIC_INT_VIA_APIC)
		return (apic_intr_establish(ih.line, IST_LEVEL, level, func, 
		    arg, what));
#endif
	if (ih.line == 0 || ih.line >= ICU_LEN || ih.line == 2)
		panic("pci_intr_establish: bogus handle 0x%x", ih.line);

	ret = isa_intr_establish(NULL, ih.line, IST_LEVEL, level, func, arg,
	    what);
#if NPCIBIOS > 0
	if (ret)
		pci_intr_route_link(pc, &ih);
#endif
	return (ret);
}

void
pci_intr_disestablish(pc, cookie)
	pci_chipset_tag_t pc;
	void *cookie;
{
	/* XXX oh, unroute the pci int link? */
	return (isa_intr_disestablish(NULL, cookie));
}
