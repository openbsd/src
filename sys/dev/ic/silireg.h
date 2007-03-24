/*	$OpenBSD: silireg.h,v 1.5 2007/03/24 02:42:21 dlg Exp $ */

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

/* PCI Registers */
#define SILI_PCI_BAR_GLOBAL	0x10 /* Global Registers address */
#define SILI_PCI_BAR_PORT	0x14 /* Port Registers address */
#define SILI_PCI_BAR_INDIRECT	0x18 /* Indirect IO Registers address */

/* Global Registers */
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

/* Port Registers */
#define SILI_PREG_LRAM		0x0000 /* Port LRAM */
	/* XXX 31 slots and port multiplier stuff sits in here */
#define SILI_PREG_PCS		0x1000 /* Port Control Set / Status */
#define SILI_PREG_PCC		0x1004 /* Port Control Clear */
#define SILI_PREG_IS		0x1008 /* Interrupt Status */
#define SILI_PREG_IES		0x1008 /* Interrupt Enable Set */
#define SILI_PREG_IEC		0x1008 /* Interrupt Enable Clear */
#define SILI_PREG_AUA		0x101c /* Activation Upper Address */
#define SILI_PREG_FIFO		0x1020 /* Command Execution FIFO */
#define SILI_PREG_CE		0x1024 /* Command Error */
#define SILI_PREG_FC		0x1028 /* FIS Configuration */
#define SILI_PREG_RFT		0x102c /* Request FIFO Threshold */
#define SILI_PREG_DEC		0x1040 /* 8b/10b Decode Error Counter */
#define SILI_PREG_CEC		0x1044 /* CRC Error Counter */
#define SILI_PREG_HEC		0x1048 /* Handshake Error Counter */
#define SILI_PREG_PHYCONF	0x1050 /* Port PHY Configuration */
#define SILI_PREG_PSS		0x1800 /* Port Slot Status */
#define SILI_PREG_CAR		0x1c00 /* Command Activation Registers */
	/* XXX up to 0x1cf7 is more of these */
#define SILI_PREG_CONTEXT	0x1e0f /* Port Context Register */
#define SILI_PREG_SCTL		0x1f00 /* SControl */
#define SILI_PREG_SSTS		0x1f04 /* SStatus */
#define SILI_PREG_SERR		0x1f08 /* SError */
#define SILI_PREG_SACT		0x1f0c /* SActive */

