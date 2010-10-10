/*	$OpenBSD: wdc_obio.c,v 1.1 2010/10/10 16:38:55 syuu Exp $	*/
/*	$NetBSD: wdc_obio.c,v 1.1 2006/09/01 21:26:18 uwe Exp $	*/

/*-
 * Copyright (c) 1998, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>
#include <dev/ic/wdcreg.h>

#include <octeon/dev/obiovar.h>

struct wdc_obio_softc {
	struct	wdc_softc sc_wdcdev;
	struct	channel_softc *sc_chanptr;
	struct	channel_softc sc_channel;
	void	*sc_ih;
};

int	wdc_obio_match(struct device *, void *, void *);
void	wdc_obio_attach(struct device *, struct device *, void *);
void	wdc_obio_poll(void *);

struct cfattach wdc_obio_ca = {
	sizeof(struct wdc_obio_softc), wdc_obio_match, wdc_obio_attach
};

#define	WDC_OBIO_REG_NPORTS	WDC_NREG
#define	WDC_OBIO_REG_SIZE	(WDC_OBIO_REG_NPORTS)

u_int8_t wdc_obio_read_reg(struct channel_softc *chp,  enum wdc_regs reg);
void wdc_obio_write_reg(struct channel_softc *chp,  enum wdc_regs reg,
    u_int8_t val);

struct channel_softc_vtbl wdc_obio_vtbl = {
	wdc_obio_read_reg,
	wdc_obio_write_reg,
	wdc_default_lba48_write_reg,
	wdc_default_read_raw_multi_2,
	wdc_default_write_raw_multi_2,
	wdc_default_read_raw_multi_4,
	wdc_default_write_raw_multi_4
};

struct timeout wdc_obio_timeout;

int
wdc_obio_match(struct device *parent, void *vcf, void *aux)
{
	return (1);
}

void
wdc_obio_poll(void *arg)
{
	int s;
	struct channel_softc *chp = (struct channel_softc *)arg;
	if ((chp->ch_flags & WDCF_IRQ_WAIT)) {
		s = splbio();
		wdcintr(chp);
		splx(s);
	}
	timeout_add(&wdc_obio_timeout, 5);
}

void
wdc_obio_attach(struct device *parent, struct device *self, void *aux)
{
	struct wdc_obio_softc *sc = (void *)self;
	struct obio_attach_args *oba = aux;
	struct channel_softc *chp = &sc->sc_channel;

	printf("\n");

	chp->cmd_iot = chp->ctl_iot = oba->oba_memt;
	chp->_vtbl = &wdc_obio_vtbl;

	if (bus_space_map(chp->cmd_iot, oba->oba_baseaddr,
	    WDC_OBIO_REG_SIZE, BUS_SPACE_MAP_KSEG0, &chp->cmd_ioh)) {
		printf(": couldn't map registers\n");
		return;
	}

	sc->sc_ih = NULL;
	timeout_set(&wdc_obio_timeout, wdc_obio_poll, (void *)chp);
	timeout_add(&wdc_obio_timeout, 5);

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_PREATA;
	sc->sc_wdcdev.PIO_cap = 0;
	sc->sc_chanptr = chp;
	sc->sc_wdcdev.channels = &sc->sc_chanptr;
	sc->sc_wdcdev.nchannels = 1;
	chp->channel = 0;
	chp->wdc = &sc->sc_wdcdev;
	chp->ch_flags = WDCF_VERBOSE_PROBE;

	chp->ch_queue = malloc(sizeof(struct channel_queue), M_DEVBUF,
	    M_NOWAIT);
	if (chp->ch_queue == NULL) {
		printf("%s: can't allocate memory for command queue\n",
		    self->dv_xname);
		return;
	}
	
	wdcattach(chp);
	wdc_print_current_modes(chp);
}

u_int8_t
wdc_obio_read_reg(struct channel_softc *chp,  enum wdc_regs reg)
{
	if (reg & 0x1)
		reg &= (_WDC_REGMASK - 1);
	else
		reg = 0x1 | (reg & _WDC_REGMASK);

	if (reg & _WDC_AUX) {
		panic("ctrl register not supported\n");
		return 0;
	}else
		return (bus_space_read_1(chp->cmd_iot, chp->cmd_ioh,
		    reg & _WDC_REGMASK));
}

void
wdc_obio_write_reg(struct channel_softc *chp,  enum wdc_regs reg, u_int8_t val)
{
	if (reg & 0x1)
		reg &= (_WDC_REGMASK - 1);
	else
		reg = 0x1 | (reg & _WDC_REGMASK);

	if (reg & _WDC_AUX)
		panic("ctrl register not supported\n");
	else
		bus_space_write_1(chp->cmd_iot, chp->cmd_ioh,
		    reg & _WDC_REGMASK, val);
}
