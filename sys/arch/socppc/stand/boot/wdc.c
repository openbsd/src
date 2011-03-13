/*	$OpenBSD: wdc.c,v 1.4 2011/03/13 00:13:53 deraadt Exp $	*/
/*	$NetBSD: wdc.c,v 1.7 2005/12/11 12:17:06 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <machine/param.h>

#include "libsa.h"
#include "wdvar.h"

#define WDCDELAY	100
#define WDCNDELAY_RST	31000 * 10

static int  wdcprobe(struct wdc_channel *chp);
static int  wdc_wait_for_ready(struct wdc_channel *chp);
static int  wdc_read_block(struct wd_softc *sc, struct wdc_command *wd_c);
static int  __wdcwait_reset(struct wdc_channel *chp, int drv_mask);

int (*controller_init)(struct wdc_channel *, u_int) = 0;

/*
 * Reset the controller.
 */
static int
__wdcwait_reset(chp, drv_mask)
	struct wdc_channel *chp;
	int drv_mask;
{
	int timeout;
	u_int8_t st0, st1;

	/* wait for BSY to deassert */
	for (timeout = 0; timeout < WDCNDELAY_RST; timeout++) {
		WDC_WRITE_REG(chp, wd_sdh, WDSD_IBM); /* master */
		delay(10);
		st0 = WDC_READ_REG(chp, wd_status);
		WDC_WRITE_REG(chp, wd_sdh, WDSD_IBM | 0x10); /* slave */
		delay(10);
		st1 = WDC_READ_REG(chp, wd_status);

		if ((drv_mask & 0x01) == 0) {
			/* no master */
			if ((drv_mask & 0x02) != 0 && (st1 & WDCS_BSY) == 0) {
				/* No master, slave is ready, it's done */
				goto end;
			}
		} else if ((drv_mask & 0x02) == 0) {
			/* no slave */
			if ((drv_mask & 0x01) != 0 && (st0 & WDCS_BSY) == 0) {
				/* No slave, master is ready, it's done */
				goto end;
			}
		} else {
			/* Wait for both master and slave to be ready */
			if ((st0 & WDCS_BSY) == 0 && (st1 & WDCS_BSY) == 0) {
				goto end;
			}
		}

		delay(WDCDELAY);
	}

	/* Reset timed out. Maybe it's because drv_mask was not right */
	if (st0 & WDCS_BSY)
		drv_mask &= ~0x01;
	if (st1 & WDCS_BSY)
		drv_mask &= ~0x02;

end:
	return (drv_mask);
}

/* Test to see controller with at last one attached drive is there.
 * Returns a bit for each possible drive found (0x01 for drive 0,
 * 0x02 for drive 1).
 * Logic:
 * - If a status register is at 0xff, assume there is no drive here
 *   (ISA has pull-up resistors).  Similarly if the status register has
 *   the value we last wrote to the bus (for IDE interfaces without pullups).
 *   If no drive at all -> return.
 * - reset the controller, wait for it to complete (may take up to 31s !).
 *   If timeout -> return.
 */
static int
wdcprobe(chp)
	struct wdc_channel *chp;
{
	u_int8_t st0, st1, sc, sn, cl, ch;
	u_int8_t ret_value = 0x03;
	u_int8_t drive;
	int found;

	/*
	 * Sanity check to see if the wdc channel responds at all.
	 */
	WDC_WRITE_REG(chp, wd_sdh, WDSD_IBM);
	delay(10);
	st0 = WDC_READ_REG(chp, wd_status);
	WDC_WRITE_REG(chp, wd_sdh, WDSD_IBM | 0x10);
	delay(10);
	st1 = WDC_READ_REG(chp, wd_status);

	if (st0 == 0xff || st0 == WDSD_IBM)
		ret_value &= ~0x01;
	if (st1 == 0xff || st1 == (WDSD_IBM | 0x10))
		ret_value &= ~0x02;
	if (ret_value == 0)
		return (ENXIO);

	/* assert SRST, wait for reset to complete */
	WDC_WRITE_REG(chp, wd_sdh, WDSD_IBM);
	delay(10);
	WDC_WRITE_CTLREG(chp, wd_aux_ctlr, WDCTL_RST | WDCTL_IDS);
	delay(1000);
	WDC_WRITE_CTLREG(chp, wd_aux_ctlr, WDCTL_IDS);
	delay(1000);
	(void) WDC_READ_REG(chp, wd_error);
	WDC_WRITE_CTLREG(chp, wd_aux_ctlr, WDCTL_4BIT);
	delay(10);

	ret_value = __wdcwait_reset(chp, ret_value);

	/* if reset failed, there's nothing here */
	if (ret_value == 0)
		return (ENXIO);

	/*
	 * Test presence of drives. First test register signatures looking for
	 * ATAPI devices. If it's not an ATAPI and reset said there may be
	 * something here assume it's ATA or OLD. Ghost will be killed later in
	 * attach routine.
	 */
	found = 0;
	for (drive = 0; drive < 2; drive++) {
		if ((ret_value & (0x01 << drive)) == 0)
			continue;
		return (0);
	}
	return (ENXIO);	
}

/*
 * Initialize the device.
 */
int
wdc_init(sc, unit)
	struct wd_softc *sc;
	u_int unit;
{
	if (!controller_init)
		return (ENXIO);
	if ((*controller_init)(&sc->sc_channel, unit) != 0)
		return (ENXIO);
	if (wdcprobe(&sc->sc_channel) != 0)
		return (ENXIO);
	return (0);
}

/*
 * Wait until the device is ready.
 */
int
wdc_wait_for_ready(chp)
	struct wdc_channel *chp;
{
	u_int timeout;
	for (timeout = WDC_TIMEOUT; timeout > 0; --timeout) {
		if ((WDC_READ_REG(chp, wd_status) & (WDCS_BSY | WDCS_DRDY))
				== WDCS_DRDY)
			return (0);
	}
	return (ENXIO);
}

/*
 * Read one block off the device.
 */
int
wdc_read_block(sc, wd_c)
	struct wd_softc *sc;
	struct wdc_command *wd_c;
{
	int i;
	struct wdc_channel *chp = &sc->sc_channel;
	u_int16_t *ptr = (u_int16_t*)wd_c->data;

	if (ptr == NULL)
		return (0);

	for (i = wd_c->bcount; i > 0; i -= sizeof(u_int16_t))
		*ptr++ = WDC_READ_DATA(chp);

	return (0);
}

/*
 * Send a command to the device (CHS and LBA addressing).
 */
int
wdccommand(wd, wd_c)
	struct wd_softc *wd;
	struct wdc_command *wd_c;
{
	u_int8_t err;
	struct wdc_channel *chp = &wd->sc_channel;

#if 0
	DPRINTF(("wdccommand(%d, %d, %d, %d, %d, %d, %d)\n",
	    wd_c->drive, wd_c->r_command, wd_c->r_cyl,
	    wd_c->r_head, wd_c->r_sector, wd_c->bcount,
	    wd_c->r_precomp));
#endif

	WDC_WRITE_REG(chp, wd_precomp, wd_c->r_precomp);
	WDC_WRITE_REG(chp, wd_seccnt, wd_c->r_count);
	WDC_WRITE_REG(chp, wd_sector, wd_c->r_sector);
	WDC_WRITE_REG(chp, wd_cyl_lo, wd_c->r_cyl);
	WDC_WRITE_REG(chp, wd_cyl_hi, wd_c->r_cyl >> 8);
	WDC_WRITE_REG(chp, wd_sdh,
	    WDSD_IBM | (wd_c->drive << 4) | wd_c->r_head);
	WDC_WRITE_REG(chp, wd_command, wd_c->r_command);

	if (wdc_wait_for_ready(chp) != 0)
		return (ENXIO);

	if (WDC_READ_REG(chp, wd_status) & WDCS_ERR) {
		DPRINTF(("wd%d: error %x\n", wd->sc_unit,
		    WDC_READ_REG(chp, wd_error)));
		return (ENXIO);
	}

	return (0);
}

/*
 * Send a command to the device (LBA48 addressing).
 */
int
wdccommandext(wd, wd_c)
	struct wd_softc *wd;
	struct wdc_command *wd_c;
{
	u_int8_t err;
	struct wdc_channel *chp = &wd->sc_channel;

	/* Select drive, head, and addressing mode. */
	WDC_WRITE_REG(chp, wd_sdh, (wd_c->drive << 4) | WDSD_LBA);

	/* previous */
	WDC_WRITE_REG(chp, wd_features, 0);
	WDC_WRITE_REG(chp, wd_seccnt, wd_c->r_count >> 8);
	WDC_WRITE_REG(chp, wd_lba_hi, wd_c->r_blkno >> 40);
	WDC_WRITE_REG(chp, wd_lba_mi, wd_c->r_blkno >> 32);
	WDC_WRITE_REG(chp, wd_lba_lo, wd_c->r_blkno >> 24);

	/* current */
	WDC_WRITE_REG(chp, wd_features, 0);
	WDC_WRITE_REG(chp, wd_seccnt, wd_c->r_count);
	WDC_WRITE_REG(chp, wd_lba_hi, wd_c->r_blkno >> 16);
	WDC_WRITE_REG(chp, wd_lba_mi, wd_c->r_blkno >> 8);
	WDC_WRITE_REG(chp, wd_lba_lo, wd_c->r_blkno);

	/* Send command. */
	WDC_WRITE_REG(chp, wd_command, wd_c->r_command);

	if (wdc_wait_for_ready(chp) != 0)
		return (ENXIO);

	if (WDC_READ_REG(chp, wd_status) & WDCS_ERR) {
		DPRINTF(("wd%d: error %x\n", wd->sc_unit,
		    WDC_READ_REG(chp, wd_error)));
		return (ENXIO);
	}

	return (0);
}

/*
 * Issue 'device identify' command.
 */
int
wdc_exec_identify(wd, data)
	struct wd_softc *wd;
	void *data;
{
	int error;
	struct wdc_command wd_c;

	memset(&wd_c, 0, sizeof(wd_c));

	wd_c.drive = wd->sc_drive;
	wd_c.r_command = WDCC_IDENTIFY;
	wd_c.bcount = DEV_BSIZE;
	wd_c.data = data;

	if ((error = wdccommand(wd, &wd_c)) != 0)
		return (error);

	return wdc_read_block(wd, &wd_c);
}

/*
 * Issue 'read' command.
 */
int
wdc_exec_read(wd, cmd, blkno, data)
	struct wd_softc *wd;
	u_int8_t cmd;
	daddr32_t blkno;
	void *data;
{
	int error;
	struct wdc_command wd_c;

	memset(&wd_c, 0, sizeof(wd_c));

	if (wd->sc_flags & WDF_LBA48) {
		/* LBA48 */
		wd_c.r_blkno = blkno;
	} else if (wd->sc_flags & WDF_LBA) {
		/* LBA */
		wd_c.r_sector = (blkno >> 0) & 0xff;
		wd_c.r_cyl = (blkno >> 8) & 0xffff;
		wd_c.r_head = (blkno >> 24) & 0x0f;
		wd_c.r_head |= WDSD_LBA;
	} else {
		/* LHS */
		wd_c.r_sector = blkno % wd->sc_label.d_nsectors;
		wd_c.r_sector++;    /* Sectors begin with 1, not 0. */
		blkno /= wd->sc_label.d_nsectors;
		wd_c.r_head = blkno % wd->sc_label.d_ntracks;
		blkno /= wd->sc_label.d_ntracks;
		wd_c.r_cyl = blkno;
		wd_c.r_head |= WDSD_CHS;
	}

	wd_c.data = data;
	wd_c.r_count = 1;
	wd_c.drive = wd->sc_drive;
	wd_c.r_command = cmd;
	wd_c.bcount = wd->sc_label.d_secsize;

	if (wd->sc_flags & WDF_LBA48)
		error = wdccommandext(wd, &wd_c);
	else
		error = wdccommand(wd, &wd_c);

	if (error != 0)
		return (error);

	return wdc_read_block(wd, &wd_c);
}
