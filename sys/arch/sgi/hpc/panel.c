/*	$OpenBSD: panel.c,v 1.2 2013/11/04 11:57:26 mpi Exp $	*/

/*
 * Copyright (c) 2012 Miodrag Vallat.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/timeout.h>

#include <machine/autoconf.h>
#include <mips64/archtype.h>
#include <machine/bus.h>

#include <sgi/hpc/iocreg.h>
#include <sgi/hpc/hpcreg.h>
#include <sgi/hpc/hpcvar.h>

#include <sgi/sgi/ip22.h>

#include "audio.h"
#include "wskbd.h"
extern int wskbd_set_mixervolume(long, long);

struct panel_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	int			 sc_irq;		/* irq number */
	void			*sc_ih;			/* irq handler cookie */
	struct timeout		 sc_repeat_tmo;		/* polling timeout */
};

/* Repeat delays for volume buttons. */
#define	PANEL_REPEAT_FIRST	400
#define	PANEL_REPEAT_NEXT	100

int	panel_match(struct device *, void *, void *);
void	panel_attach(struct device *, struct device *, void *);

struct cfdriver panel_cd = {
	NULL, "panel", DV_DULL
};

const struct cfattach panel_ca = {
	sizeof(struct panel_softc), panel_match, panel_attach
};

int	panel_intr(void *);
void	panel_repeat(void *);
#if NAUDIO > 0 && NWSKBD > 0
void	panel_volume_adjust(struct panel_softc *, uint8_t);
#endif

int
panel_match(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct hpc_attach_args *ha = aux;

	if (strcmp(ha->ha_name, cf->cf_driver->cd_name) != 0)
		return 0;

	if (sys_config.system_type == SGI_IP20)
		return 0;

	return 1;
}

void
panel_attach(struct device *parent, struct device *self, void *aux)
{
	struct panel_softc *sc = (struct panel_softc *)self;
	struct hpc_attach_args *haa = aux;

	sc->sc_iot = haa->ha_st;
	if (bus_space_subregion(haa->ha_st, haa->ha_sh, haa->ha_devoff + 3, 1,
	    &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_irq = haa->ha_irq;
	sc->sc_ih = hpc_intr_establish(sc->sc_irq, IPL_BIO, panel_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		return;
	}

	if (sys_config.system_subtype == IP22_INDY)
		printf(": power and volume buttons\n");
	else
		printf(": power button\n");

	timeout_set(&sc->sc_repeat_tmo, panel_repeat, sc);
}

int
panel_intr(void *v)
{
	struct panel_softc *sc = (struct panel_softc *)v;
	uint8_t reg, ack;
	extern int allowpowerdown;

	reg = bus_space_read_1(sc->sc_iot, sc->sc_ioh, 0);

	ack = IOC_PANEL_POWER_STATE;
	if (sys_config.system_subtype == IP22_INDIGO2 ||
	    (reg & IOC_PANEL_POWER_IRQ) == 0)
		ack |= IOC_PANEL_POWER_IRQ;
	if (sys_config.system_subtype == IP22_INDY) {
		if ((reg & IOC_PANEL_VDOWN_IRQ) == 0)
			ack |= IOC_PANEL_VDOWN_IRQ;
		if ((reg & IOC_PANEL_VUP_IRQ) == 0)
			ack |= IOC_PANEL_VUP_IRQ;
	}
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, 0, ack);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, 1,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	/*
	 * Panel interrupts are latched for about 300 msec after being
	 * acked, and not until all buttons are depressed.
	 * We need to temporary disable the interrupt and switch to polling,
	 * until the interrupt can be enabled again.
	 */
	hpc_intr_disable(sc->sc_ih);

	/*
	 * If the power button is down, try and issue a shutdown immediately
	 * (if allowed).
	 */
	if (sys_config.system_subtype == IP22_INDIGO2 ||
	    (reg & IOC_PANEL_POWER_IRQ) == 0) {
		if (allowpowerdown == 1) {
			allowpowerdown = 0;
			psignal(initproc, SIGUSR2);
		}
	}

#if NAUDIO > 0 && NWSKBD > 0
	/*
	 * If any of the volume buttons is down, update volume.
	 */
	if (sys_config.system_subtype == IP22_INDY)
		panel_volume_adjust(sc, reg);
#endif

	timeout_add_msec(&sc->sc_repeat_tmo, PANEL_REPEAT_FIRST);

	return 1;
}

void
panel_repeat(void *v)
{
	struct panel_softc *sc = (struct panel_softc *)v;
	uint8_t reg;

	reg = bus_space_read_1(sc->sc_iot, sc->sc_ioh, 0);

#if NAUDIO > 0 && NWSKBD > 0
	/*
	 * Volume button autorepeat.
	 */
	if (sys_config.system_subtype == IP22_INDY)
		panel_volume_adjust(sc, reg);
#endif

	if (hpc_is_intr_pending(sc->sc_irq)) {
		/*
		 * Keep acking everything to get the interrupt finally
		 * unlatched.
		 */
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, 0,
		    IOC_PANEL_POWER_STATE | IOC_PANEL_POWER_IRQ |
		    IOC_PANEL_VDOWN_IRQ | IOC_PANEL_VDOWN_HOLD |
		    IOC_PANEL_VUP_IRQ | IOC_PANEL_VUP_HOLD);
		bus_space_barrier(sc->sc_iot, sc->sc_ioh, 0, 1,
		    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

		timeout_add_msec(&sc->sc_repeat_tmo, PANEL_REPEAT_NEXT);
	} else {
		hpc_intr_enable(sc->sc_ih);
	}
}

#if NAUDIO > 0 && NWSKBD > 0
void
panel_volume_adjust(struct panel_softc *sc, uint8_t reg)
{
	long adjust;

	switch (reg & (IOC_PANEL_VDOWN_IRQ | IOC_PANEL_VUP_IRQ)) {
	case 0:				/* both buttons pressed: mute */
		adjust = 0;
		break;
	case IOC_PANEL_VDOWN_IRQ:	/* up button pressed */
		adjust = 1;
		break;
	case IOC_PANEL_VDUP_IRQ:	/* down button pressed */
		adjust = -1;
		break;
	case IOC_PANEL_VDOWN_IRQ | IOC_PANEL_VDUP_IRQ:
		return;
	}

	wskbd_set_mixervolume(adjust, 1);
}
#endif
