/*	$OpenBSD: silireg.h,v 1.4 2007/03/24 02:28:06 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

#define SILI_PCI_BAR_GLOBAL	0x10
#define SILI_PCI_BAR_PORT	0x14
#define SILI_PCI_BAR_INDIRECT	0x18

#define SILI_REG_PORT0_STATUS	0x00 /* Port 0 Slot Status */
#define SILI_REG_PORT1_STATUS	0x04 /* Port 1 Slot Status */
#define SILI_REG_PORT2_STATUS	0x08 /* Port 2 Slot Status */
#define SILI_REG_PORT3_STATUS	0x0c /* Port 3 Slot Status */
#define SILI_REG_GC		0x40 /* Global Control */
#define SILI_REG_GIS		0x40 /* Global Interrupt Status */
#define SILI_REG_PHYCONF	0x48 /* PHY Configuration */
#define SILI_REG_BISTCTL	0x50 /* BIST Control */
#define SILI_REG_BISTPATTERN	0x54 /* BIST Pattern */
#define SILI_REG_BISTSTAT	0x58 /* BIST Status */
#define SILI_REG_FLASHADDR	0x70 /* Flash Address */
#define SILI_REG_FLASHDATA	0x74 /* Flash Memory Data / GPIO Control */
#define SILI_REG_GPIOCTL	SILI_REG_FLASHDATA
#define SILI_REG_IICADDR	0x78 /* I2C Address */
#define SILI_REG_IIC		0x7c /* I2C Data / Control */
