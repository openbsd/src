/*	$OpenBSD: pciide.c,v 1.1 2006/07/28 17:12:06 kettenis Exp $	*/
/*	$NetBSD: pciide.c,v 1.5 2005/12/11 12:17:06 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/types.h>
#include <lib/libsa/stand.h>

#include "boot.h"
#include "wdvar.h"

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

int
pciide_init(chp, unit)
	struct wdc_channel *chp;
	u_int *unit;
{
	u_int32_t cmdreg, ctlreg;
	int i, compatchan = 0;

	/*
	 * two channels per chip, two drives per channel
	 */
	compatchan = *unit / PCIIDE_CHANNEL_NDEV;
	if (compatchan >= PCIIDE_NUM_CHANNELS)
		return (ENXIO);
	*unit %= PCIIDE_CHANNEL_NDEV;

	DPRINTF(("[pciide] unit: %d, channel: %d\n", *unit, compatchan));

	/*
	 * XXX map?
	 */
	cmdreg = 0x90000200 + compatchan * 0x10;
	ctlreg = 0x90000208 + compatchan * 0x10;

	/* set up cmd regsiters */
	chp->c_cmdbase = (u_int8_t *)cmdreg;
	chp->c_data = (u_int16_t *)(cmdreg + wd_data);
	for (i = 0; i < WDC_NPORTS; i++)
		chp->c_cmdreg[i] = chp->c_cmdbase + i;
	/* set up shadow registers */
	chp->c_cmdreg[wd_status]   = chp->c_cmdreg[wd_command];
	chp->c_cmdreg[wd_features] = chp->c_cmdreg[wd_precomp];
	/* set up ctl registers */
	chp->c_ctlbase = (u_int8_t *)ctlreg;

	return (0);
}
