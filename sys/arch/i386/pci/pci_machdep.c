/*	$NetBSD: pci_machdep.c,v 1.23 1996/04/11 22:15:33 cgd Exp $	*/

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

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pio.h>

#include <i386/isa/icu.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

int pci_mode = -1;

#define	PCI_MODE1_ENABLE	0x80000000UL
#define	PCI_MODE1_ADDRESS_REG	0x0cf8
#define	PCI_MODE1_DATA_REG	0x0cfc

#define	PCI_MODE2_ENABLE_REG	0x0cf8
#define	PCI_MODE2_FORWARD_REG	0x0cfa

void
pci_attach_hook(parent, self, pba)
	struct device *parent, *self;
	struct pcibus_attach_args *pba;
{

	if (pba->pba_bus == 0)
		printf(": configuration mode %d", pci_mode);
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
mode1:
	if (bus >= 256 || device >= 32 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode1 = PCI_MODE1_ENABLE |
		    (bus << 16) | (device << 11) | (function << 8);
	return tag;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
	if (bus >= 256 || device >= 16 || function >= 8)
		panic("pci_make_tag: bad request");

	tag.mode2.port = 0xc000 | (device << 8);
	tag.mode2.enable = 0xf0 | (function << 1);
	tag.mode2.forward = bus;
	return tag;
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
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	data = inl(PCI_MODE1_DATA_REG);
	outl(PCI_MODE1_ADDRESS_REG, 0);
	return data;
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
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
mode1:
	outl(PCI_MODE1_ADDRESS_REG, tag.mode1 | reg);
	outl(PCI_MODE1_DATA_REG, data);
	outl(PCI_MODE1_ADDRESS_REG, 0);
#endif

#if !defined(PCI_CONF_MODE) || (PCI_CONF_MODE == 2)
mode2:
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

	/*
	 * We try to divine which configuration mode the host bridge wants.  We
	 * try mode 2 first, because our probe for mode 1 is likely to succeed
	 * for mode 2 also.
	 *
	 * XXX
	 * This should really be done using the PCI BIOS.
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
pci_intr_map(pc, intrtag, pin, line, ihp)
	pci_chipset_tag_t pc;
	pcitag_t intrtag;
	int pin, line;
	pci_intr_handle_t *ihp;
{

	if (pin == 0) {
		/* No IRQ used. */
		goto bad;
	}

	if (pin > 4) {
		printf("pci_intr_map: bad interrupt pin %d\n", pin);
		goto bad;
	}

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

	*ihp = line;
	return 0;

bad:
	*ihp = -1;
	return 1;
}

const char *
pci_intr_string(pc, ih)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
{
	static char irqstr[8];		/* 4 + 2 + NULL + sanity */

	if (ih == 0 || ih >= ICU_LEN || ih == 2)
		panic("pci_intr_string: bogus handle 0x%x\n", ih);

	sprintf(irqstr, "irq %d", ih);
	return (irqstr);
	
}

void *
pci_intr_establish(pc, ih, level, func, arg, what)
	pci_chipset_tag_t pc;
	pci_intr_handle_t ih;
	int level, (*func) __P((void *));
	void *arg;
	char *what;
{

	if (ih == 0 || ih >= ICU_LEN || ih == 2)
		panic("pci_intr_establish: bogus handle 0x%x\n", ih);

	return isa_intr_establish(NULL, ih, IST_LEVEL, level, func, arg, what);
}

void
pci_intr_disestablish(pc, cookie)
	pci_chipset_tag_t pc;
	void *cookie;
{

	return isa_intr_disestablish(NULL, cookie);
}
