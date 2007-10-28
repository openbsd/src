/*	$OpenBSD: envyreg.h,v 1.2 2007/10/28 18:25:21 fgsch Exp $	*/
/*
 * Copyright (c) 2007 Alexandre Ratchov <alex@caoua.org>
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
#ifndef SYS_DEV_PCI_ENVYREG_H
#define SYS_DEV_PCI_ENVYREG_H

/*
 * BARs at PCI config space
 */
#define ENVY_CTL_BAR		0x10
#define ENVY_MT_BAR		0x1c
#define ENVY_CONF		0x60

/*
 * CCS "control" register
 */
#define ENVY_CTL		0x00
#define   ENVY_CTL_RESET	0x80
#define   ENVY_CTL_NATIVE	0x01
#define ENVY_CCS_INTMASK	0x01
#define   ENVY_CCS_INT_MT	0x10
#define   ENVY_CCS_INT_MIDI1	0x80
#define   ENVY_CCS_INT_TMR	0x80
#define   ENVY_CCS_INT_MIDI0	0x80
#define ENVY_CCS_INTSTAT	0x02

/*
 * CCS registers to access indirect registers (CCI)
 */
#define ENVY_CCI_INDEX	0x3
#define ENVY_CCI_DATA	0x4

/*
 * CCS regisers to access iic bus
 */
#define ENVY_I2C_DEV		0x10
#define   ENVY_I2C_DEV_SHIFT	0x01
#define   ENVY_I2C_DEV_WRITE	0x01
#define   ENVY_I2C_DEV_EEPROM	0x50
#define ENVY_I2C_ADDR		0x11
#define ENVY_I2C_DATA		0x12
#define ENVY_I2C_CTL		0x13
#define	  ENVY_I2C_CTL_BUSY	0x1

/*
 * CCI registers to access GPIO pins
 */
#define ENVY_GPIO_DATA		0x20
#define ENVY_GPIO_MASK		0x21
#define ENVY_GPIO_DIR		0x22

/*
 * GPIO pin numbers
 */
#define ENVY_GPIO_CLK		0x2
#define ENVY_GPIO_DOUT		0x8
#define ENVY_GPIO_CSMASK	0x70
#define ENVY_GPIO_CS(dev)	((dev) << 4)

/*
 * EEPROM bytes signification
 */
#define ENVY_EEPROM_CONF	6
#define ENVY_EEPROM_ACLINK	7
#define ENVY_EEPROM_I2S		8
#define ENVY_EEPROM_SPDIF	9
#define ENVY_EEPROM_GPIOMASK	10
#define ENVY_EEPROM_GPIOST	11
#define ENVY_EEPROM_GPIODIR	12

#define ENVY_EEPROM_MAXSZ	32

/*
 * MT registers for play/record params
 */
#define ENVY_MT_INTR		0
#define   ENVY_MT_INTR_PACK	0x01
#define   ENVY_MT_INTR_RACK	0x02
#define   ENVY_MT_INTR_PMASK	0x40
#define   ENVY_MT_INTR_RMASK	0x80
#define ENVY_MT_RATE		1
#define   ENVY_MT_RATEMASK	0x0f
#define ENVY_MT_PADDR		0x10
#define ENVY_MT_PBUFSZ		0x14
#define ENVY_MT_PBLKSZ		0x16
#define ENVY_MT_CTL		0x18
#define   ENVY_MT_CTL_PSTART	0x01
#define   ENVY_MT_CTL_PPAUSE	0x02
#define   ENVY_MT_CTL_RSTART	0x04
#define   ENVY_MT_CTL_RPAUSE	0x08
#define ENVY_MT_RADDR		0x20
#define ENVY_MT_RBUFSZ		0x24
#define ENVY_MT_RBLKSZ		0x26

/*
 * MT registers for monitor gains
 */
#define ENVY_MT_MONDATA		0x38
#define   ENVY_MT_MONVAL_BITS	7
#define   ENVY_MT_MONVAL_MASK	((1 << ENVY_MT_MONVAL_BITS) - 1)
#define ENVY_MT_MONIDX		0x3a

/*
 * MT registers to access the digital mixer
 */
#define ENVY_MT_OUTSRC		0x30
#define   ENVY_MT_OUTSRC_DMA	0x00
#define   ENVY_MT_OUTSRC_MON	0x01
#define   ENVY_MT_OUTSRC_LINE	0x02
#define   ENVY_MT_OUTSRC_SPD	0x03
#define   ENVY_MT_OUTSRC_MASK	0x04
#define ENVY_MT_SPDROUTE	0x32
#define   ENVY_MT_SPDSRC_DMA	0x00
#define   ENVY_MT_SPDSRC_MON	0x01
#define   ENVY_MT_SPDSRC_LINE	0x02
#define   ENVY_MT_SPDSRC_SPD	0x03
#define   ENVY_MT_SPDSRC_MASK	0x04
#define   ENVY_MT_SPDSEL_BITS	0x4
#define   ENVY_MT_SPDSEL_MASK	((1 << ENVY_MT_SPDSEL_BITS) - 1)
#define ENVY_MT_INSEL		0x34
#define   ENVY_MT_INSEL_BITS	0x4
#define   ENVY_MT_INSEL_MASK	((1 << ENVY_MT_INSEL_BITS) - 1)

/*
 * AK4524 control registers
 */
#define AK_PWR			0x00
#define   AK_PWR_DA		0x01
#define   AK_PWR_AD		0x02
#define   AK_PWR_VREF		0x04
#define AK_RST			0x01
#define   AK_RST_DA		0x01
#define   AK_RST_AD		0x02
#define AK_FMT			0x02
#define   AK_FMT_NORM		0
#define   AK_FMT_DBL	       	0x01
#define   AK_FMT_QUAD		0x02
#define   AK_FMT_QAUDFILT	0x04
#define   AK_FMT_256		0
#define   AK_FMT_512		0x04
#define   AK_FMT_1024		0x08
#define   AK_FMT_384		0x10
#define   AK_FMT_768		0x14
#define   AK_FMT_LSB16		0
#define   AK_FMT_LSB20		0x20
#define   AK_FMT_MSB24		0x40
#define   AK_FMT_IIS24		0x60
#define   AK_FMT_LSB24		0x80
#define AK_DEEMVOL		0x03
#define   AK_MUTE		0x80
#define AK_ADC_GAIN0		0x04
#define	AK_ADC_GAIN1		0x05
#define AK_DAC_GAIN0		0x06
#define AK_DAC_GAIN1		0x07

/*
 * default formats
 */
#define ENVY_RFRAME_SIZE	(4 * 12)
#define ENVY_PFRAME_SIZE	(4 * 10)
#define ENVY_RBUF_SIZE		(ENVY_RFRAME_SIZE * 0x1000)
#define ENVY_PBUF_SIZE		(ENVY_PFRAME_SIZE * 0x1000)
#define ENVY_RCHANS		12
#define ENVY_PCHANS		10

#endif /* !defined(SYS_DEV_PCI_ENVYREG_H) */
