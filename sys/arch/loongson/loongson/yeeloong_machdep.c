/*	$OpenBSD: yeeloong_machdep.c,v 1.4 2010/02/12 19:37:31 miod Exp $	*/

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Lemote Fuloong 2F and Yeeloong specific code and configuration data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <mips64/archtype.h>
#include <machine/autoconf.h>

#include <dev/isa/isareg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <loongson/dev/bonitoreg.h>
#include <loongson/dev/bonitovar.h>
#include <loongson/dev/bonito_irq.h>
#include <loongson/dev/glxreg.h>
#include <loongson/dev/glxvar.h>

void	yeeloong_attach_hook(pci_chipset_tag_t);
int	yeeloong_intr_map(int, int, int);

void	fuloong_powerdown(void);
void	yeeloong_powerdown(void);
void	yeeloong_reset(void);

const struct bonito_config yeeloong_bonito = {
	.bc_adbase = 11,

	.bc_gpioIE = LOONGSON_INTRMASK_GPIO,
	.bc_intEdge = LOONGSON_INTRMASK_PCI_SYSERR |
	    LOONGSON_INTRMASK_PCI_PARERR,
	.bc_intSteer = 0,
	.bc_intPol = LOONGSON_INTRMASK_DRAM_PARERR |
	    LOONGSON_INTRMASK_PCI_SYSERR | LOONGSON_INTRMASK_PCI_PARERR |
	    LOONGSON_INTRMASK_INT0 | LOONGSON_INTRMASK_INT1,

	.bc_legacy_pic = 1,

	.bc_attach_hook = yeeloong_attach_hook,
	.bc_intr_map = yeeloong_intr_map
};

const struct legacy_io_range fuloong_legacy_ranges[] = {
	/* isa */
	{ IO_DMAPG + 4,	IO_DMAPG + 4 },
	/* mcclock */
	{ IO_RTC,	IO_RTC + 1 },
	/* pciide */
	{ 0x170,	0x170 + 7 },
	{ 0x1f0,	0x1f0 + 7 },
	{ 0x376,	0x376 },
	{ 0x3f6,	0x3f6 },
	/* com */
	{ IO_COM2,	IO_COM2 + 8 },

	{ 0 }
};

const struct legacy_io_range yeeloong_legacy_ranges[] = {
	/* isa */
	{ IO_DMAPG + 4,	IO_DMAPG + 4 },
	/* pckbc */
	{ IO_KBD,	IO_KBD },
	{ IO_KBD + 4,	IO_KBD + 4 },
	/* mcclock */
	{ IO_RTC,	IO_RTC + 1 },
	/* pciide */
	{ 0x170,	0x170 + 7 },
	{ 0x1f0,	0x1f0 + 7 },
	{ 0x376,	0x376 },
	{ 0x3f6,	0x3f6 },

	{ 0 }
};

const struct platform fuloong_platform = {
	.system_type = LOONGSON_FULOONG,
	.vendor = "Lemote",
	.product = "Fuloong",

	.bonito_config = &yeeloong_bonito,
	.legacy_io_ranges = fuloong_legacy_ranges,

	.setup = NULL,
	.powerdown = fuloong_powerdown,
	.reset = yeeloong_reset
};

const struct platform yeeloong_platform = {
	.system_type = LOONGSON_YEELOONG,
	.vendor = "Lemote",
	.product = "Yeeloong",

	.bonito_config = &yeeloong_bonito,
	.legacy_io_ranges = yeeloong_legacy_ranges,

	.setup = NULL,
	.powerdown = yeeloong_powerdown,
	.reset = yeeloong_reset
};

void
yeeloong_attach_hook(pci_chipset_tag_t pc)
{
	pcireg_t id;
	pcitag_t tag;
	int dev;

	/*
	 * Check for an AMD CS5536 chip; if one is found, register
	 * the proper PCI configuration space hooks.
	 */

	for (dev = pci_bus_maxdevs(pc, 0); dev >= 0; dev--) {
		tag = pci_make_tag(pc, 0, dev, 0);
		id = pci_conf_read(pc, tag, PCI_ID_REG);
		if (id == PCI_ID_CODE(PCI_VENDOR_AMD,
		    PCI_PRODUCT_AMD_CS5536_PCISB)) {
			glx_init(pc, tag, dev);
			break;
		}
	}
}

int
yeeloong_intr_map(int dev, int fn, int pin)
{
	switch (dev) {
	/* onboard devices, only pin A is wired */
	case 6:
	case 7:
	case 8:
	case 9:
		if (pin == PCI_INTERRUPT_PIN_A)
			return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA +
			    (dev - 6));
		break;
	/* PCI slot */
	case 10:
		return BONITO_DIRECT_IRQ(LOONGSON_INTR_PCIA +
		    (pin - PCI_INTERRUPT_PIN_A));
	/* Geode chip */
	case 14:
		switch (fn) {
		case 1:	/* Flash */
			return BONITO_ISA_IRQ(6);
		case 3:	/* AC97 */
			return BONITO_ISA_IRQ(9);
		case 4:	/* OHCI */
		case 5:	/* EHCI */
			return BONITO_ISA_IRQ(11);
		}
		break;
	default:
		break;
	}

	return -1;
}

void
fuloong_powerdown()
{
	vaddr_t gpiobase;

	gpiobase = BONITO_PCIIO_BASE + (rdmsr(DIVIL_LBAR_GPIO) & 0xff00);
	/* enable GPIO 13 */
	REGVAL(gpiobase + GPIOL_OUT_EN) = GPIO_ATOMIC_VALUE(13, 1);
	/* set GPIO13 value to zero */
	REGVAL(gpiobase + GPIOL_OUT_VAL) = GPIO_ATOMIC_VALUE(13, 0);
}

void
yeeloong_powerdown()
{
	REGVAL(BONITO_GPIODATA) &= ~0x00000001;
	REGVAL(BONITO_GPIOIE) &= ~0x00000001;
}

void
yeeloong_reset()
{
	wrmsr(GLCP_SYS_RST, rdmsr(GLCP_SYS_RST) | 1);
}
