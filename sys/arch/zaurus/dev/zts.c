/* $OpenBSD: zts.c,v 1.2 2005/01/28 23:26:54 drahn Exp $ */
/*
 * Copyright (c) 2005 Dale Rahn <drahn@openbsd.org>
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/kernel.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0_gpio.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

u_int32_t pxa2x0_ssp_read_val(u_int32_t);

int zts_match(struct device *, void *, void *);
void zts_attach(struct device *, struct device *, void *);
int zts_irq(void *v);
void zts_poll(void *v);

int      zts_enable(void *);
void     zts_disable(void *);
int      zts_ioctl(void *, u_long, caddr_t, int, struct proc *);

#define DO_RELATIVE

struct zts_softc {
	struct device sc_dev;

	struct timeout sc_ts_poll;

	int sc_enabled;
	int sc_buttons; /* button emulation ? */
	struct device *sc_wsmousedev;
#ifdef DO_RELATIVE
	int sc_oldx;
	int sc_oldy;
#endif
};

#define ADSCTRL_PD0_SH          0       // PD0 bit
#define ADSCTRL_PD1_SH          1       // PD1 bit
#define ADSCTRL_DFR_SH          2       // SER/DFR bit
#define ADSCTRL_MOD_SH          3       // Mode bit
#define ADSCTRL_ADR_SH          4       // Address setting
#define ADSCTRL_STS_SH          7       // Start bit


struct cfattach zts_ca = {
	sizeof(struct zts_softc), zts_match, zts_attach
};

struct cfdriver zts_cd = {
	NULL, "zts", DV_DULL
};

int
zts_match(struct device *parent, void *cf, void *aux)
{
	return 1;
}

#define IRQ_GPIO_TP_INT_C3K 11
#define POLL_TIMEOUT_RATE ((hz * 150)/1000)
/*
#define POLL_TIMEOUT_RATE ((hz * 500)/1000)
*/

const struct wsmouse_accessops zts_accessops = {
        zts_enable,
	zts_ioctl,
	zts_disable
};


void
zts_attach(struct device *parent, struct device *self, void *aux)
{
	struct zts_softc *sc = (struct zts_softc *)self;
	struct wsmousedev_attach_args a;  


	timeout_set(&(sc->sc_ts_poll), zts_poll, sc);
	timeout_add(&sc->sc_ts_poll, POLL_TIMEOUT_RATE);

/*
	pxa2x0_gpio_set_function(IRQ_GPIO_TP_INT_C3K, GPIO_IN);

	pxa2x0_gpio_intr_establish(IRQ_GPIO_TP_INT_C3K, IST_EDGE_RISING,
	    IPL_TTY, zts_irq, sc, sc->sc_dev.dv_xname);
*/

	printf ("\n");

	a.accessops = &zts_accessops;
	a.accesscookie = sc;
		
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

}

void
zts_poll(void *v) 
{
	struct zts_softc *sc = v;

	timeout_add(&sc->sc_ts_poll, POLL_TIMEOUT_RATE);

	zts_irq(v);
}

int
zts_irq(void *v)
{

	struct zts_softc *sc = v;
	u_int32_t cmd;
	u_int32_t t0, t1;
	u_int32_t x;
	u_int32_t y;
	int down;

	/* check that pen is down */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t0 = pxa2x0_ssp_read_val(cmd);


	/* X */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (5 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		/* XXX - read multiple times so it is stable? */
	x = pxa2x0_ssp_read_val(cmd);


	/* Y */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		/* XXX - read multiple times so it is stable? */
	y = pxa2x0_ssp_read_val(cmd);


	/* check that pen is still down */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t1 = pxa2x0_ssp_read_val(cmd);
	
	down = pxa2x0_gpio_get_bit(IRQ_GPIO_TP_INT_C3K);

	if (t0 != 0 && t1 != 0) {
		/*
		printf("zts: t0 %x t1 %x, x %x y %x int %d\n", t0, t1, x, y,
		    down);
		*/
	}

#ifdef DO_RELATIVE
	/*
	 * relative mode here is really just a hack until abs mode
	 * really works in X.
	 */
	if (t0 != 0 && t1 != 0) {
		int dx, dy;
		int skip = 0;

		if ( sc->sc_oldx == -1) {
			skip = 1;
		}

		dx = x - sc->sc_oldx; /* temp */
		dy = y - sc->sc_oldy;

		/* scale down */
		dx /= 10;
		dy /= 10;

		/* y is inverted */
		dy = - dy;

		sc->sc_oldx = x;
		sc->sc_oldy = y;
		if (!skip)
			wsmouse_input(sc->sc_wsmousedev, 0/* XXX buttons */,
			    dx, dy, 0 /* XXX*/, WSMOUSE_INPUT_DELTA);
	} else {
		sc->sc_oldx = -1;
		sc->sc_oldy = -1;
	}
#else
	
	if (t0 != 0 && t1 != 0)
		wsmouse_input(sc->sc_wsmousedev, 0/* XXX buttons */, x, y,
		    0 /* XXX*/,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z );
#endif


	/*
	pxa2x0_gpio_clear_intr(IRQ_GPIO_TP_INT_C3K);
	*/

	return 1;
}

int
zts_enable(void *v)
{
	struct zts_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	sc->sc_enabled = 1;
	sc->sc_buttons = 0;

	/* enable interrupt, or polling */
	timeout_add(&sc->sc_ts_poll, POLL_TIMEOUT_RATE);

	return 0;
}

void
zts_disable(void *v)
{
	struct zts_softc *sc = v;

	timeout_del(&sc->sc_ts_poll);

	/* disable interrupts/polling */
	sc->sc_enabled = 0;
}

int
zts_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		return (0);
	}

	return (-1);
}

