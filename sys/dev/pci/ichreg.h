/*	$OpenBSD: ichreg.h,v 1.5 2005/11/18 11:44:00 grange Exp $	*/
/*
 * Copyright (c) 2004, 2005 Alexander Yurchenko <grange@openbsd.org>
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

#ifndef _DEV_PCI_ICHREG_H_
#define _DEV_PCI_ICHREG_H_

/*
 * Intel ICH registers definitions
 */

/*
 * LPC interface bridge registers
 */

/*
 * PCI configuration registers
 */
#define ICH_PMBASE	0x40		/* ACPI base address */
#define ICH_ACPI_CNTL	0x44		/* ACPI control */
#define ICH_ACPI_CNTL_ACPI_EN	(1 << 4)	/* ACPI enable */
#define ICH_GEN_PMCON1	0xa0		/* general PM configuration */
/* ICHx-M only */
#define ICH_GEN_PMCON1_SS_EN	0x08		/* enable SpeedStep */

#define ICH_PMSIZE	128		/* ACPI space size */

/*
 * Power management I/O registers
 */
#define ICH_PM_TMR	0x08		/* PM timer */
/* ICHx-M only */
#define ICH_PM_CNTL	0x20		/* power management control */
#define ICH_PM_ARB_DIS		0x01		/* disable arbiter */
#define ICH_PM_SS_CNTL	0x50		/* SpeedStep control */
#define ICH_PM_SS_STATE_LOW	0x01		/* low power state */

/*
 * 6300ESB watchdog timer registers
 */
#define ICH_WDT_BASE	0x10		/* memory space base address */
#define ICH_WDT_CONF	0x60		/* configuration register */
#define ICH_WDT_CONF_MASK	0xffff		/* 16-bit register */
#define ICH_WDT_CONF_INT_MASK	0x3		/* interrupt type */
#define ICH_WDT_CONF_INT_IRQ	0x0		/* IRQ (APIC 1, INT 10) */
#define ICH_WDT_CONF_INT_SMI	0x2		/* SMI */
#define ICH_WDT_CONF_INT_DIS	0x3		/* disabled */
#define ICH_WDT_CONF_PRE	(1 << 2)	/* 2^5 clock divisor */
#define ICH_WDT_CONF_OUTDIS	(1 << 5)	/* WDT_TOUT# output disabled */
#define ICH_WDT_LOCK	0x68		/* lock register */
#define ICH_WDT_LOCK_LOCKED	(1 << 0)	/* register locked */
#define ICH_WDT_LOCK_ENABLED	(1 << 1)	/* WDT enabled */
#define ICH_WDT_LOCK_FREERUN	(1 << 2)	/* free running mode */

#define ICH_WDT_PRE1	0x00		/* preload value 1 */
#define ICH_WDT_PRE2	0x04		/* preload value 2 */
#define ICH_WDT_GIS	0x08		/* general interrupt status */
#define ICH_WDT_GIS_ACTIVE	(1 << 0)	/* interrupt active */
#define ICH_WDT_RELOAD	0x0c		/* reload register */
#define ICH_WDT_RELOAD_RLD	(1 << 8)	/* safe reload */
#define ICH_WDT_RELOAD_TIMEOUT	(1 << 9)	/* timeout occured */

#endif	/* !_DEV_PCI_ICHREG_H_ */
