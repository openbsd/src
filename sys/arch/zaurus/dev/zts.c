/* $OpenBSD: zts.c,v 1.5 2005/02/16 22:25:18 drahn Exp $ */
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

struct zts_softc {
	struct device sc_dev;

	struct timeout sc_ts_poll;

	int sc_enabled;
	int sc_buttons; /* button emulation ? */
	struct device *sc_wsmousedev;
	int sc_oldx;
	int sc_oldy;
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

	sc->sc_enabled = 0;
	sc->sc_buttons = 0;

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

#define TS_STABLE 8
#define NSAMPLES 3
int
zts_irq(void *v)
{
	struct zts_softc *sc = v;
	u_int32_t cmd;
	u_int32_t t0, t1, xv, x[NSAMPLES], yv, y[NSAMPLES];
	int i, diff[NSAMPLES];
	int down;
	int mindiff, mindiffv;
	extern int zkbd_modstate;

	for (i = 0; i < NSAMPLES; i++) {
		/* check that pen is down */
		cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
		    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		t0 = pxa2x0_ssp_read_val(cmd);

		/* X */
		cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
		    (5 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		x[i] = pxa2x0_ssp_read_val(cmd);

		/* Y */
		cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
		    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		y[i] = pxa2x0_ssp_read_val(cmd);

		/* check that pen is still down */
		cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
		    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

		t1 = pxa2x0_ssp_read_val(cmd);
	}

	/* X */
	for (i = 0 ; i < NSAMPLES; i++) {
		int alt;
		alt = i+1;
		if (alt == NSAMPLES)
			alt = 0;

		diff[i] = x[i]-x[alt];
		if (diff[i] < 0)
			diff[i] = -diff[i]; /* ABS */
	}
	mindiffv = diff[0];
	mindiff = 0;
	if (diff[1] < mindiffv) {
		mindiffv = diff[1];
		mindiff = 1;
	}
	if (diff[2] < mindiffv) {
		mindiff = 2;
	}
	switch (mindiff) {
	case 0:
		xv = (x[0] + x[1]) / 2;
		break;
	case 1:
		xv = (x[1] + x[2]) / 2;
		break;
	case 2:
		xv = (x[2] + x[0]) / 2;
		break;
	}

	/* Y */
	for (i = 0 ; i < NSAMPLES; i++) {
		int alt;
		alt = i+1;
		if (alt == NSAMPLES)
			alt = 0;

		diff[i] = y[i]-y[alt];
		if (diff[i] < 0)
			diff[i] = -diff[i]; /* ABS */
	}
	mindiffv = diff[0];
	mindiff = 0;
	if (diff[1] < mindiffv) {
		mindiffv = diff[1];
		mindiff = 1;
	}
	if (diff[2] < mindiffv) {
		mindiff = 2;
	}
	switch (mindiff) {
	case 0:
		yv = (y[0] + y[1]) / 2;
		break;
	case 1:
		yv = (y[1] + y[2]) / 2;
		break;
	case 2:
		yv = (y[2] + y[0]) / 2;
		break;
	}
	
	down = (t0 > 10 && t1 > 10);
	if (zkbd_modstate != 0 && down) {
		if(zkbd_modstate & (1 << 1)) {
			/* Fn */
			down = 2;
		}
		if(zkbd_modstate & (1 << 2)) {
			/* 'Alt' */
			down = 4;
		}
	}
	if (!down) {
		/* x/y values are not reliable when pen is up */
		xv = sc->sc_oldx;
		yv = sc->sc_oldy;
	}
	if (down || sc->sc_buttons != down) {
		wsmouse_input(sc->sc_wsmousedev, down, xv, yv, 0 /* z */,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z);
		sc->sc_buttons = down;
		sc->sc_oldx = xv;
		sc->sc_oldy = yv;
	}

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
