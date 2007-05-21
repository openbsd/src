/*	$OpenBSD: pci_machdep.c,v 1.12 2007/05/21 22:10:45 kettenis Exp $	*/
/*	$NetBSD: pci_machdep.c,v 1.3 2003/05/07 21:33:58 fvdl Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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
#include <sys/lock.h>

#include <uvm/uvm_extern.h>

#define _X86_BUS_DMA_PRIVATE
#include <machine/bus.h>

#include <machine/pio.h>
#include <machine/intr.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include "ioapic.h"

#if NIOAPIC > 0
#include <machine/i82093var.h>
#include <machine/mpbiosvar.h>
#endif

int pci_mode = -1;

#if defined(MULTIPROCESSOR) && 0
struct simplelock pci_conf_slock = SIMPLELOCK_INITIALIZER;
#else
struct simplelock pci_conf_slock = { 0 };
#endif

#define	PCI_CONF_LOCK(s)						\
do {									\
	(s) = splhigh();						\
	simple_lock(&pci_conf_slock);					\
} while (0)

#define	PCI_CONF_UNLOCK(s)						\
do {									\
	simple_unlock(&pci_conf_slock);					\
	splx((s));							\
} while (0)

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

#define	PCI_MODE2_ENABLE_REG	0x0cf8
#define	PCI_MODE2_FORWARD_REG	0x0cfa

#define _m1tag(b, d, f) \
	(PCI_MODE1_ENABLE | ((b) << 16) | ((d) << 11) | ((f) << 8))
#define _qe(bus, dev, fcn, vend, prod) \
	{_m1tag(bus, dev, fcn), PCI_ID_CODE(vend, prod)}
struct {
	u_int32_t tag;
	pcireg_t id;
} pcim1_quirk_tbl[] = {
	_qe(0, 0, 0, PCI_VENDOR_COMPAQ, PCI_PRODUCT_COMPAQ_TRIFLEX1),
	/* XXX Triflex2 not tested */
	_qe(0, 0, 0, PCI_VENDOR_COMPAQ, PCI_PRODUCT_COMPAQ_TRIFLEX2),
	_qe(0, 0, 0, PCI_VENDOR_COMPAQ, PCI_PRODUCT_COMPAQ_TRIFLEX4),
	/* Triton needed for Connectix Virtual PC */
	_qe(0, 0, 0, PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82437FX),
	/* Connectix Virtual PC 5 has a 440BX */
	_qe(0, 0, 0, PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82443BX_NOAGP),
	{0, 0xffffffff} /* patchable */
};
#undef _m1tag
#undef _qe

/*
 * PCI doesn't have any special needs; just use the generic versions
 * of these functions.
 */
struct x86_bus_dma_tag pci_bus_dma_tag = {
	NULL,			/* _may_bounce */
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	NULL,
	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap,
};

extern void amdgart_probe(struct pcibus_attach_args *);

void
pci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
	if (pba->pba_bus == 0) {
		printf(": configuration mode %d", pci_mode);
		amdgart_probe(pba);
	}
}

int
pci_bus_maxdevs(pci_chipset_tag_t pc, int busno)
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
pci_make_tag(pci_chipset_tag_t pc, int bus, int device, int function)
{
	pcitag_t tag;

	switch (pci_mode) {
	case 1:
		if (bus >= 256 || device >= 32 || function >= 8)
			panic("pci_make_tag: bad request");

		tag.mode1 = PCI_MODE1_ENABLE |
		    	(bus << 16) | (device << 11) | (function << 8);
		break;
	case 2:
		if (bus >= 256 || device >= 16 || function >= 8)
			panic("pci_make_tag: bad request");

		tag.mode2.port = 0xc000 | (device << 8);
		tag.mode2.enable = 0xf0 | (function << 1);
		tag.mode2.forward = bus;
		break;
	default:
		panic("pci_make_tag: mode not configured");
	}

	return tag;
}

void
pci_decompose_tag(pci_chipset_tag_t pc, pcitag_t tag, int *bp, int *dp, int *fp)
{

	switch (pci_mode) {
	case 1:
		if (bp != NULL)
			*bp = (tag.mode1 >> 16) & 0xff;
		if (dp != NULL)
			*dp = (tag.mode1 >> 11) & 0x1f;
		if (fp != NULL)
			*fp = (tag.mode1 >> 8) & 0x7;
		break;
	case 2:
		if (bp != NULL)
			*bp = tag.mode2.forward & 0xff;
		if (dp != NULL)
			*dp = (tag.mode2.port >> 8) & 0xf;
		if (fp != NULL)
			*fp = (tag.mode2.enable >> 1) & 0x7;
		break;
	default:
		panic("pci_decompose_tag: mode not configured");
	}
}

pcireg_t
pci_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{
	pcireg_t data;
	int s;

	PCI_CONF_LOCK(s);
	switch (pci_mode) {
	case 1:
		outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
		data = inl(PCI_MODE1_DATA_REG);
		outl(PCI_MODE1_ADDRESS_REG, 0);
		break;
	case 2:
		outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
		outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
		data = inl(tag.mode2.port | reg);
		outb(PCI_MODE2_ENABLE_REG, 0);
		break;
	default:
		panic("pci_conf_read: mode not configured");
	}
	PCI_CONF_UNLOCK(s);

	return data;
}

void
pci_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	int s;

	PCI_CONF_LOCK(s);
	switch (pci_mode) {
	case 1:
		outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
		outl(PCI_MODE1_DATA_REG, data);
		outl(PCI_MODE1_ADDRESS_REG, 0);
		break;
	case 2:
		outb(PCI_MODE2_ENABLE_REG, tag.mode2.enable);
		outb(PCI_MODE2_FORWARD_REG, tag.mode2.forward);
		outl(tag.mode2.port | reg, data);
		outb(PCI_MODE2_ENABLE_REG, 0);
		break;
	default:
		panic("pci_conf_write: mode not configured");
	}
	PCI_CONF_UNLOCK(s);
}

int
pci_mode_detect(void)
{

#ifdef PCI_CONF_MODE
#if (PCI_CONF_MODE == 1) || (PCI_CONF_MODE == 2)
	return (pci_mode = PCI_CONF_MODE);
#else
#error Invalid PCI configuration mode.
#endif
#else
	u_int32_t sav, val;
	int i;
	pcireg_t idreg;

	if (pci_mode != -1)
		return pci_mode;

	/*
	 * We try to divine which configuration mode the host bridge wants.
	 */

	sav = inl(PCI_MODE1_ADDRESS_REG);

	pci_mode = 1; /* assume this for now */
	/*
	 * catch some known buggy implementations of mode 1
	 */
	for (i = 0; i < sizeof(pcim1_quirk_tbl) / sizeof(pcim1_quirk_tbl[0]);
	     i++) {
		pcitag_t t;

		if (!pcim1_quirk_tbl[i].tag)
			break;
		t.mode1 = pcim1_quirk_tbl[i].tag;
		idreg = pci_conf_read(0, t, PCI_ID_REG); /* needs "pci_mode" */
		if (idreg == pcim1_quirk_tbl[i].id) {
#ifdef DEBUG
			printf("known mode 1 PCI chipset (%08x)\n",
			       idreg);
#endif
			return (pci_mode);
		}
	}

	/*
	 * Strong check for standard compliant mode 1:
	 * 1. bit 31 ("enable") can be set
	 * 2. byte/word access does not affect register
	 */
	outl(PCI_MODE1_ADDRESS_REG, PCI_MODE1_ENABLE);
	outb(PCI_MODE1_ADDRESS_REG + 3, 0);
	outw(PCI_MODE1_ADDRESS_REG + 2, 0);
	val = inl(PCI_MODE1_ADDRESS_REG);
	if ((val & 0x80fffffc) != PCI_MODE1_ENABLE) {
#ifdef DEBUG
		printf("pci_mode_detect: mode 1 enable failed (%x)\n",
		       val);
#endif
		goto not1;
	}
	outl(PCI_MODE1_ADDRESS_REG, 0);
	val = inl(PCI_MODE1_ADDRESS_REG);
	if ((val & 0x80fffffc) != 0)
		goto not1;
	return (pci_mode);
not1:
	outl(PCI_MODE1_ADDRESS_REG, sav);

	/*
	 * This mode 2 check is quite weak (and known to give false
	 * positives on some Compaq machines).
	 * However, this doesn't matter, because this is the
	 * last test, and simply no PCI devices will be found if
	 * this happens.
	 */
	outb(PCI_MODE2_ENABLE_REG, 0);
	outb(PCI_MODE2_FORWARD_REG, 0);
	if (inb(PCI_MODE2_ENABLE_REG) != 0 ||
	    inb(PCI_MODE2_FORWARD_REG) != 0)
		goto not2;
	return (pci_mode = 2);
not2:

	return (pci_mode = 0);
#endif
}

int
pci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	int pin = pa->pa_intrpin;
	int line = pa->pa_intrline;
#if NIOAPIC > 0
	int rawpin = pa->pa_rawintrpin;
	pci_chipset_tag_t pc = pa->pa_pc;
	int bus, dev, func;
	int mppin;
#endif

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	if (pin > PCI_INTERRUPT_PIN_MAX) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

#if NIOAPIC > 0
	pci_decompose_tag(pc, pa->pa_tag, &bus, &dev, &func);
	if (mp_busses != NULL) {
		mppin = (dev << 2)|(rawpin - 1);
		if (intr_find_mpmapping(bus, mppin, ihp) == 0) {
			*ihp |= line;
			return 0;
		}
		if (pa->pa_bridgetag) {
			int pin = PPB_INTERRUPT_SWIZZLE(rawpin, dev);
			if (pa->pa_bridgeih[pin - 1] != -1) {
				*ihp = pa->pa_bridgeih[pin - 1];
				*ihp |= line;
				return 0;
			}
		}
		/*
		 * No explicit PCI mapping found. This is not fatal,
		 * we'll try the ISA (or possibly EISA) mappings next.
		 */
	}
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
	if (line == 0 || line == X86_PCI_INTERRUPT_LINE_NO_CONNECTION)
		goto bad;

	if (line >= NUM_LEGACY_IRQS) {
		printf("pci_intr_map: bad interrupt line %d\n", line);
		goto bad;
	}
	if (line == 2) {
		printf("pci_intr_map: changed line 2 to line 9\n");
		line = 9;
	}

#if NIOAPIC > 0
	if (mp_busses != NULL) {
		if (intr_find_mpmapping(mp_isa_bus->mb_idx, line, ihp) == 0) {
			*ihp |= line;
			return 0;
		}
#if NEISA > 0
		if (intr_find_mpmapping(mp_eisa_bus->mb_idx, line, ihp) == 0) {
			*ihp |= line;
			return 0;
		}
#endif
		printf("pci_intr_map: bus %d dev %d func %d pin %d; line %d\n",
		    bus, dev, func, pin, line);
		printf("pci_intr_map: no MP mapping found\n");
	}
#endif

	*ihp = line;
	return 0;

bad:
	*ihp = -1;
	return 1;
}

const char *
pci_intr_string(pci_chipset_tag_t pc, pci_intr_handle_t ih)
{
	static char irqstr[64];

	if (ih == 0)
		panic("pci_intr_string: bogus handle 0x%x", ih);


#if NIOAPIC > 0
	if (ih & APIC_INT_VIA_APIC)
		snprintf(irqstr, sizeof(irqstr), "apic %d int %d (irq %d)",
		    APIC_IRQ_APIC(ih),
		    APIC_IRQ_PIN(ih),
		    ih&0xff);
	else
		snprintf(irqstr, sizeof(irqstr), "irq %d", ih&0xff);
#else

	snprintf(irqstr, sizeof(irqstr), "irq %d", ih&0xff);
#endif
	return (irqstr);

}

void *
pci_intr_establish(pci_chipset_tag_t pc, pci_intr_handle_t ih, int level,
    int (*func)(void *), void *arg, char *what)
{
	int pin, irq;
	struct pic *pic;

	pic = &i8259_pic;
	pin = irq = ih;

#if NIOAPIC > 0
	if (ih & APIC_INT_VIA_APIC) {
		pic = (struct pic *)ioapic_find(APIC_IRQ_APIC(ih));
		if (pic == NULL) {
			printf("pci_intr_establish: bad ioapic %d\n",
			    APIC_IRQ_APIC(ih));
			return NULL;
		}
		pin = APIC_IRQ_PIN(ih);
		irq = APIC_IRQ_LEGACY_IRQ(ih);
		if (irq < 0 || irq >= NUM_LEGACY_IRQS)
			irq = -1;
	}
#endif

	return intr_establish(irq, pic, pin, IST_LEVEL, level, func, arg, what);
}

void
pci_intr_disestablish(pci_chipset_tag_t pc, void *cookie)
{
	intr_disestablish(cookie);
}

/*
 * Determine which flags should be passed to the primary PCI bus's
 * autoconfiguration node.  We use this to detect broken chipsets
 * which cannot safely use memory-mapped device access.
 */
int
pci_bus_flags(void)
{
	int rval = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED;
	int device, maxndevs;
	pcitag_t tag;
	pcireg_t id;

	maxndevs = pci_bus_maxdevs(NULL, 0);

	for (device = 0; device < maxndevs; device++) {
		tag = pci_make_tag(NULL, 0, device, 0);
		id = pci_conf_read(NULL, tag, PCI_ID_REG);

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;
		/* XXX Not invalid, but we've done this ~forever. */
		if (PCI_VENDOR(id) == 0)
			continue;

		switch (PCI_VENDOR(id)) {
		case PCI_VENDOR_SIS:
			switch (PCI_PRODUCT(id)) {
			case PCI_PRODUCT_SIS_85C496:
				goto disable_mem;
			}
			break;
		}
	}

	return (rval);

 disable_mem:
	printf("Warning: broken PCI-Host bridge detected; "
	    "disabling memory-mapped access\n");
	rval &= ~(PCI_FLAGS_MEM_ENABLED);
	return (rval);
}

