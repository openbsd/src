/*	$OpenBSD: wdc_obio.c,v 1.1 2009/09/07 21:16:57 dms Exp $	*/

/*
 * Copyright (c) 2009 Dariusz Swiderski <sfires@sfires.net>
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

#include <sys/types.h>

#include "libsa.h"
#include "wdvar.h"

#define WDC_OBIO_REG_OFFSET	(8 << 17)
#define WDC_OBIO_AUXREG_OFFSET	(6 << 16)

u_int8_t wdc_read_cmdreg(struct wdc_channel *, u_int8_t);
void wdc_write_cmdreg(struct wdc_channel *, u_int8_t, u_int8_t);
u_int8_t wdc_read_ctlreg(struct wdc_channel *, u_int8_t);
void wdc_write_ctlreg(struct wdc_channel *, u_int8_t, u_int8_t);

u_int32_t wdc_base_addr[2];

int
wdc_obio_init(struct wdc_channel *chp, u_int chan)
{
	u_int32_t cmdreg, ctlreg;
	int i;

	/*
	 * two channels per chip, one drive per channel
	 */
	if (chan >= 2 || !wdc_base_addr[chan])
		return (ENXIO);
	chp->ndrives = 1;

	cmdreg = wdc_base_addr[chan] + WDC_OBIO_REG_OFFSET;
	ctlreg = wdc_base_addr[chan] + WDC_OBIO_AUXREG_OFFSET;

	/* set up cmd regsiters */
	chp->c_cmdbase = (u_int8_t *)cmdreg;
	chp->c_data = (u_int16_t *)(cmdreg + wd_data);
	for (i = 0; i < WDC_NPORTS; i++)
		chp->c_cmdreg[i] = chp->c_cmdbase + (i<<16);
	/* set up shadow registers */
	chp->c_cmdreg[wd_status]   = chp->c_cmdreg[wd_command];
	chp->c_cmdreg[wd_features] = chp->c_cmdreg[wd_precomp];
	/* set up ctl registers */
	chp->c_ctlbase = (u_int8_t *)ctlreg;

	chp->read_cmdreg = wdc_read_cmdreg;
	chp->write_cmdreg = wdc_write_cmdreg;
	chp->read_ctlreg = wdc_read_ctlreg;
	chp->write_ctlreg = wdc_write_ctlreg;
                                
	return (0);
}

u_int8_t
wdc_read_cmdreg(struct wdc_channel *chp, u_int8_t reg)
{
	u_int8_t val;
	val = *chp->c_cmdreg[reg];
	if (val == 0xf9 && reg == wd_status)
		val = 0x7f;
	return val;
}

void
wdc_write_cmdreg(struct wdc_channel *chp, u_int8_t reg, u_int8_t val)
{
	*chp->c_cmdreg[reg] = val;
}	

u_int8_t
wdc_read_ctlreg(struct wdc_channel *chp, u_int8_t reg)
{
	u_int8_t val;
	val = chp->c_ctlbase[reg];
	if (val == 0xf9 && reg == wd_aux_altsts)
		val = 0x7f;
	return val;
}

void
wdc_write_ctlreg(struct wdc_channel *chp, u_int8_t reg, u_int8_t val)
{
	chp->c_ctlbase[reg] = val;
}
