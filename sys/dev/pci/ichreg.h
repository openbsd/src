/*	$OpenBSD: ichreg.h,v 1.2 2004/09/25 13:58:44 grange Exp $	*/
/*
 * Copyright (c) 2004 Alexander Yurchenko <grange@openbsd.org>
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

#endif	/* !_DEV_PCI_ICHREG_H_ */
