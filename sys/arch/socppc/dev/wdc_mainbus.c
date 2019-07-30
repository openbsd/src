/*	$OpenBSD: wdc_mainbus.c,v 1.2 2011/05/09 22:33:54 matthew Exp $	*/

/*
 * Copyright (c) 2009 Mark Kettenis
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

/*
 * Driver for Compact Flash interface on the local bus of a PowerQUICC
 * MPC8343E processor as found on the RouterBOARD 600.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <dev/ofw/openfirm.h>

#include <dev/ata/atavar.h>
#include <dev/ic/wdcvar.h>

struct wdc_mainbus_softc {
	struct wdc_softc	sc_wdcdev;
	struct channel_softc	*sc_chanptr;
	struct channel_softc	sc_channel;

	void	*sc_ih;
};

int	wdc_mainbus_match(struct device *, void *, void *);
void	wdc_mainbus_attach(struct device *, struct device *, void *);

struct cfattach wdc_mainbus_ca = {
	sizeof(struct wdc_mainbus_softc), wdc_mainbus_match, wdc_mainbus_attach
};

/*
 * The CF slots on the RouterBOARD 600 are wired up in a completely
 * retarded way.  Instead of using the obvious address lines it uses
 * LAD[11:15], which means we need to waste more than 0.5 MB of
 * address space to map 10 8-bit registers.
 */
#define	WDC_MAINBUS_REG_NPORTS		WDC_NREG
#define	WDC_MAINBUS_REG_SIZE		(WDC_MAINBUS_REG_NPORTS << 16)
#define WDC_MAINBUS_REG_OFFSET		(8 << 17)
#define	WDC_MAINBUS_AUXREG_NPORTS	2
#define	WDC_MAINBUS_AUXREG_SIZE		(WDC_MAINBUS_AUXREG_NPORTS << 16)
#define	WDC_MAINBUS_AUXREG_OFFSET	(6 << 16)

u_int8_t wdc_mainbus_read_reg(struct channel_softc *chp,  enum wdc_regs reg);
void wdc_mainbus_write_reg(struct channel_softc *chp,  enum wdc_regs reg,
    u_int8_t val);

struct channel_softc_vtbl wdc_mainbus_vtbl = {
	wdc_mainbus_read_reg,
	wdc_mainbus_write_reg,
	wdc_default_lba48_write_reg,
	wdc_default_read_raw_multi_2,
	wdc_default_write_raw_multi_2,
	wdc_default_read_raw_multi_4,
	wdc_default_write_raw_multi_4
};

int
wdc_mainbus_match(struct device *parent, void *cfdata, void *aux)
{
	struct mainbus_attach_args *ma = aux;
	char buf[32];

	if (OF_getprop(ma->ma_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "rb,cf") != 0)
		return (0);

	return (1);
}

void
wdc_mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct wdc_mainbus_softc *sc = (void *)self;
	struct mainbus_attach_args *ma = aux;
	struct channel_softc *chp = &sc->sc_channel;
	int reg[2];
	int ivec;

	if (OF_getprop(ma->ma_node, "reg", &reg, sizeof(reg)) < sizeof(reg)) {
		printf(": missing registers\n");
		return;
	}

	if (OF_getprop(ma->ma_node, "interrupts", &ivec, sizeof(ivec)) <= 0) {
		printf(": missing interrupts\n");
		return;
	}

	chp->cmd_iot = chp->ctl_iot = ma->ma_iot;
	chp->_vtbl = &wdc_mainbus_vtbl;

	if (bus_space_map(chp->cmd_iot, reg[0] + WDC_MAINBUS_REG_OFFSET,
	    WDC_MAINBUS_REG_SIZE, 0, &chp->cmd_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	if (bus_space_map(chp->ctl_iot, reg[0] + WDC_MAINBUS_AUXREG_OFFSET,
	    WDC_MAINBUS_AUXREG_SIZE, 0, &chp->ctl_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_ih = intr_establish(ivec, IST_LEVEL, IPL_BIO, wdcintr,
	    chp, self->dv_xname);

	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16 | WDC_CAPABILITY_PREATA;
	sc->sc_wdcdev.PIO_cap = 0;
	sc->sc_chanptr = chp;
	sc->sc_wdcdev.channels = &sc->sc_chanptr;
	sc->sc_wdcdev.nchannels = 1;
	chp->channel = 0;
	chp->wdc = &sc->sc_wdcdev;

	chp->ch_queue = wdc_alloc_queue();
	if (chp->ch_queue == NULL) {
		printf("%s: cannot allocate channel queue\n",
		    self->dv_xname);
		/* XXX disestablish interrupt */
		return;
	}

	printf("\n");
	
	wdcattach(chp);
	wdc_print_current_modes(chp);
}

u_int8_t
wdc_mainbus_read_reg(struct channel_softc *chp, enum wdc_regs reg)
{
	uint8_t val;

	if (reg & _WDC_AUX) {
		val = bus_space_read_1(chp->ctl_iot, chp->ctl_ioh,
		    (reg & _WDC_REGMASK) << 16);
		if (val == 0xf9 && reg == wdr_altsts)
			val = 0x7f;
	} else {
		val = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh,
		    (reg & _WDC_REGMASK) << 16);
		if (val == 0xf9 && reg == wdr_status)
			val = 0x7f;
	}
	return val;
}

void
wdc_mainbus_write_reg(struct channel_softc *chp, enum wdc_regs reg,
    u_int8_t val)
{
	if (reg & _WDC_AUX) 
		bus_space_write_1(chp->ctl_iot, chp->ctl_ioh,
		    (reg & _WDC_REGMASK) << 16, val);
	else
		bus_space_write_1(chp->cmd_iot, chp->cmd_ioh,
		    (reg & _WDC_REGMASK) << 16, val);
}
