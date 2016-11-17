/*	$OpenBSD: generic3a_machdep.c,v 1.1 2016/11/17 14:41:21 visa Exp $	*/

/*
 * Copyright (c) 2009, 2010, 2012 Miodrag Vallat.
 * Copyright (c) 2016 Visa Hankala.
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
 * Generic Loongson 2Gq and 3A code and configuration data.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <mips64/archtype.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pmon.h>

#include <mips64/loongson3.h>

#include <dev/ic/i8259reg.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <loongson/dev/htbreg.h>
#include <loongson/dev/htbvar.h>
#include <loongson/dev/leiocvar.h>

#define IRQ_CASCADE 2

void	 generic3a_device_register(struct device *, void *);
void	 generic3a_powerdown(void);
void	 generic3a_reset(void);
void	 generic3a_setup(void);

void	 rs780e_setup(void);

void	 rs780e_isa_attach_hook(struct device *, struct device *,
	    struct isabus_attach_args *iba);
void	*rs780e_isa_intr_establish(void *, int, int, int, int (*)(void *),
	    void *, char *);
void	 rs780e_isa_intr_disestablish(void *, void *);

void	 rs780e_eoi(int);
void	 rs780e_set_imask(uint32_t);
void	 rs780e_irq_mask(int);
void	 rs780e_irq_unmask(int);

/* Firmware entry points */
void	(*generic3a_reboot_entry)(void);
void	(*generic3a_poweroff_entry)(void);

struct mips_isa_chipset rs780e_isa_chipset = {
	.ic_v = NULL,
	.ic_attach_hook = rs780e_isa_attach_hook,
	.ic_intr_establish = rs780e_isa_intr_establish,
	.ic_intr_disestablish = rs780e_isa_intr_disestablish
};

const struct legacy_io_range rs780e_legacy_ranges[] = {
	/* isa */
	{ IO_DMAPG + 4,	IO_DMAPG + 4 },
	/* mcclock */
	{ IO_RTC,	IO_RTC + 1 },
#ifdef notyet
	/* pciide */
	{ 0x170,	0x170 + 7 },
	{ 0x1f0,	0x1f0 + 7 },
	{ 0x376,	0x376 },
	{ 0x3f6,	0x3f6 },
#endif
	/* pckbc */
	{ IO_KBD,	IO_KBD },
	{ IO_KBD + 4,	IO_KBD + 4 },

	{ 0, 0 }
};

const struct platform rs780e_platform = {
	.system_type = LOONGSON_3A,
	.vendor = "Loongson",
	.product = "LS3A with RS780E",

	.isa_chipset = &rs780e_isa_chipset,
	.legacy_io_ranges = rs780e_legacy_ranges,

	.setup = rs780e_setup,
	.device_register = generic3a_device_register,

	.powerdown = generic3a_powerdown,
	.reset = generic3a_reset
};

const struct pic rs780e_pic = {
	rs780e_eoi, rs780e_irq_mask, rs780e_irq_unmask
};

uint32_t rs780e_imask;

/*
 * Generic 3A routines
 */

void
generic3a_powerdown(void)
{
	if (generic3a_poweroff_entry != NULL)
		generic3a_poweroff_entry();
}

void
generic3a_reset(void)
{
	if (generic3a_reboot_entry != NULL)
		generic3a_reboot_entry();
}

void
generic3a_setup(void)
{
	const struct pmon_env_reset *resetenv = pmon_get_env_reset();

	if (resetenv != NULL) {
		generic3a_reboot_entry = resetenv->warm_boot;
		generic3a_poweroff_entry = resetenv->poweroff;
	}

	loongson3_intr_init();
}

void
generic3a_device_register(struct device *dev, void *aux)
{
#if notyet
	const char *drvrname = dev->dv_cfdata->cf_driver->cd_name;
	const char *name = dev->dv_xname;

	if (dev->dv_class != bootdev_class)
		return;	

	/* 
	 * The device numbering must match. There's no way
	 * pmon tells us more info. Depending on the usb slot
	 * and hubs used you may be lucky. Also, assume umass/sd for usb
	 * attached devices.
	 */
	switch (bootdev_class) {
	case DV_DISK:
		if (strcmp(drvrname, "wd") == 0 && strcmp(name, bootdev) == 0)
			bootdv = dev;
		else {
			/* XXX this really only works safely for usb0... */
			if ((strcmp(drvrname, "sd") == 0 ||
			    strcmp(drvrname, "cd") == 0) &&
			    strncmp(bootdev, "usb", 3) == 0 &&
			    strcmp(name + 2, bootdev + 3) == 0)
				bootdv = dev;
		}
		break;
	case DV_IFNET:
		/*
		 * This relies on the onboard Ethernet interface being
		 * attached before any other (usb) interface.
		 */
		bootdv = dev;
		break;
	default:
		break;
	}
#endif
}

/*
 * Routines for RS780E-based systems
 */

void
rs780e_setup(void)
{
	generic3a_setup();

	htb_early_setup();

	/*
	 * Set up the PIC in the southbridge.
	 */

	/* master */
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_ICW1) = ICW1_SELECT | ICW1_IC4;
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_ICW2) = ICW2_VECTOR(0);
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_ICW3) = ICW3_CASCADE(IRQ_CASCADE);
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_ICW4) = ICW4_8086;
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_OCW1) = 0xff;

	/* slave */
	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_ICW1) = ICW1_SELECT | ICW1_IC4;
	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_ICW2) = ICW2_VECTOR(8);
	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_ICW3) = ICW3_SIC(IRQ_CASCADE);
	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_ICW4) = ICW4_8086;
	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_OCW1) = 0xff;

	loongson3_register_ht_pic(&rs780e_pic);
}

void
rs780e_isa_attach_hook(struct device *parent, struct device *self,
    struct isabus_attach_args *iba)
{
}

void *
rs780e_isa_intr_establish(void *v, int irq, int type, int level,
    int (*cb)(void *), void *cbarg, char *name)
{
	return loongson3_ht_intr_establish(irq, level, cb, cbarg, name);
}

void
rs780e_isa_intr_disestablish(void *v, void *ih)
{
	loongson3_ht_intr_disestablish(ih);
}

void
rs780e_eoi(int irq)
{
	KASSERT((unsigned int)irq <= 15);

	if (irq & 8) {
		REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_OCW2) =
		    OCW2_SELECT | OCW2_EOI | OCW2_SL | OCW2_ILS(irq);
		irq = IRQ_CASCADE;
	}
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_OCW2) =
	    OCW2_SELECT | OCW2_EOI | OCW2_SL | OCW2_ILS(irq);
}

void
rs780e_set_imask(uint32_t new_imask)
{
	uint8_t imr1, imr2;

	imr1 = 0xff & ~new_imask;
	imr1 &= ~(1u << IRQ_CASCADE);
	imr2 = 0xff & ~(new_imask >> 8);

	REGVAL8(HTB_IO_BASE + IO_ICU2 + PIC_OCW1) = imr2;
	REGVAL8(HTB_IO_BASE + IO_ICU1 + PIC_OCW1) = imr1;

	rs780e_imask = new_imask;
}

void
rs780e_irq_mask(int irq)
{
	rs780e_set_imask(rs780e_imask & ~(1u << irq));
}

void
rs780e_irq_unmask(int irq)
{
	rs780e_set_imask(rs780e_imask | (1u << irq));
}
