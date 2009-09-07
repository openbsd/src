/*	$OpenBSD: wdvar.h,v 1.2 2009/09/07 21:16:57 dms Exp $	*/
/*	$NetBSD: wdvar.h,v 1.6 2005/12/11 12:17:06 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * Copyright (c) 2001 Dynarc AB, Sweden. All rights reserved.
 *
 * This code is derived from software written by Anders Magnusson,
 * ragge@ludd.luth.se
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef _STAND_WDVAR_H
#define _STAND_WDVAR_H

#include <sys/disklabel.h>

#include <dev/ic/wdcreg.h>
#include <dev/ata/atareg.h>
#include <dev/pci/pciidereg.h>

/*
 * WD1003 / ATA Disk Controller register definitions.
 */

/* offsets of registers in the 'regular' register region */
#define wd_data                 0       /* data register (R/W - 16 bits) */
#define wd_error                1       /* error register (R) */
#define wd_precomp              1       /* write precompensation (W) */
#define wd_seccnt               2       /* sector count (R/W) */
#define wd_ireason              2       /* interrupt reason (R/W) (for atapi) */
#define wd_sector               3       /* first sector number (R/W) */
#define wd_cyl_lo               4       /* cylinder address, low byte (R/W) */
#define wd_cyl_hi               5       /* cylinder address, high byte (R/W) */
#define wd_sdh                  6       /* sector size/drive/head (R/W) */
#define wd_command              7       /* command register (W) */
#define wd_lba_lo               3       /* lba address, low byte (RW) */
#define wd_lba_mi               4       /* lba address, middle byte (RW) */
#define wd_lba_hi               5       /* lba address, high byte (RW) */

/* "shadow" registers; these may or may not overlap regular registers */
#define wd_status               8       /* immediate status (R) */
#define wd_features             9       /* features (W) */

/* offsets of registers in the auxiliary register region */
#define wd_aux_altsts           0       /* alternate fixed disk status (R) */
#define wd_aux_ctlr             0       /* fixed disk controller control (W) */
#define  WDCTL_4BIT              0x08   /* use four head bits (wd1003) */
#define  WDCTL_RST               0x04   /* reset the controller */
#define  WDCTL_IDS               0x02   /* disable controller interrupts */

#define WDC_TIMEOUT		2000000
#define PCIIDE_CHANNEL_NDEV	2
#define NUNITS			(PCIIDE_CHANNEL_NDEV * PCIIDE_NUM_CHANNELS)
#define WDC_NPORTS		8	/* XXX */
#define WDC_NSHADOWREG		2	/* XXX */

struct wdc_channel {
	volatile u_int8_t *c_cmdbase;
	volatile u_int8_t *c_ctlbase;
	volatile u_int8_t *c_cmdreg[WDC_NPORTS + WDC_NSHADOWREG];
	volatile u_int16_t *c_data;

	u_int8_t ndrives;

	u_int8_t (*read_cmdreg)(struct wdc_channel *chp, u_int8_t reg);
	void (*write_cmdreg)(struct wdc_channel *chp, u_int8_t reg, u_int8_t val);
	u_int8_t (*read_ctlreg)(struct wdc_channel *chp, u_int8_t reg);
	void (*write_ctlreg)(struct wdc_channel *chp, u_int8_t reg, u_int8_t val);
};

#define WDC_READ_REG(chp, reg)		(chp)->read_cmdreg(chp, reg)
#define WDC_WRITE_REG(chp, reg, val)	(chp)->write_cmdreg(chp, reg, val)
#define WDC_READ_CTLREG(chp, reg)	(chp)->read_ctlreg(chp, reg)
#define WDC_WRITE_CTLREG(chp, reg, val)	(chp)->write_ctlreg(chp, reg, val)
#define WDC_READ_DATA(chp)		*(chp)->c_data

struct wd_softc {
#define WDF_LBA		0x0001
#define WDF_LBA48	0x0002
	u_int16_t sc_flags;

	u_int sc_part;
	u_int sc_unit;

	u_int64_t sc_capacity;

	struct ataparams sc_params;
	struct disklabel sc_label;
	struct wdc_channel sc_channel;
	u_int sc_drive;
};

struct wdc_command {
	u_int8_t drive;		/* drive id */

	u_int8_t r_command;	/* Parameters to upload to registers */
	u_int8_t r_head;
	u_int16_t r_cyl;
	u_int8_t r_sector;
	u_int8_t r_count;
	u_int8_t r_precomp;

	u_int16_t bcount;
	void *data;

	u_int64_t r_blkno;
};

int	wdc_init		(struct wd_softc*, u_int);
int	wdccommand		(struct wd_softc*, struct wdc_command*);
int	wdccommandext		(struct wd_softc*, struct wdc_command*);
int	wdc_exec_read		(struct wd_softc*, u_int8_t, daddr_t, void*);
int	wdc_exec_identify	(struct wd_softc*, void*);


#endif /* _STAND_WDVAR_H */
