/*	$OpenBSD: pciide.c,v 1.3 2009/09/07 21:16:57 dms Exp $	*/
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

#include "libsa.h"
#include "wdvar.h"

u_int8_t pciide_read_cmdreg(struct wdc_channel *, u_int8_t);
void pciide_write_cmdreg(struct wdc_channel *, u_int8_t, u_int8_t);
u_int8_t pciide_read_ctlreg(struct wdc_channel *, u_int8_t);
void pciide_write_ctlreg(struct wdc_channel *, u_int8_t, u_int8_t);

u_int32_t pciide_base_addr = 0;

int
pciide_init(struct wdc_channel *chp, u_int chan)
{
	u_int32_t cmdreg, ctlreg;
	int i;

	/*
	 * two channels per chip, one drive per channel
	 */
	if (chan >= PCIIDE_NUM_CHANNELS || pciide_base_addr == 0)
		return (ENXIO);
	chp->ndrives = 1;

	DPRINTF(("[pciide] channel: %d\n", chan));

	/*
	 * XXX map?
	 */
	cmdreg = pciide_base_addr + chan * 0x10;
	ctlreg = pciide_base_addr+0x8 + chan * 0x10;

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

	chp->read_cmdreg = pciide_read_cmdreg;
	chp->write_cmdreg = pciide_write_cmdreg;
	chp->read_ctlreg = pciide_read_ctlreg;
	chp->write_ctlreg = pciide_write_ctlreg;
	return (0);
}

u_int8_t
pciide_read_cmdreg(struct wdc_channel *chp, u_int8_t reg)
{
	return *chp->c_cmdreg[reg];
}

void
pciide_write_cmdreg(struct wdc_channel *chp, u_int8_t reg, u_int8_t val)
{
	*chp->c_cmdreg[reg] = val;
}	

u_int8_t
pciide_read_ctlreg(struct wdc_channel *chp, u_int8_t reg)
{
	return chp->c_ctlbase[reg];
}

void
pciide_write_ctlreg(struct wdc_channel *chp, u_int8_t reg, u_int8_t val)
{
	chp->c_ctlbase[reg] = val;
}
