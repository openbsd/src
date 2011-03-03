/* $OpenBSD: zts.c,v 1.15 2011/03/03 21:48:49 kettenis Exp $ */
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

#include <zaurus/dev/zaurus_sspvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#include <dev/wscons/wsdisplayvar.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_lcd.h>

#ifdef ZTS_DEBUG
#define DPRINTF(x)	do { printf x; } while (0)
#else
#define DPRINTF(x)
#endif

/*
 * ADS784x touch screen controller
 */
#define ADSCTRL_PD0_SH          0       /* PD0 bit */
#define ADSCTRL_PD1_SH          1       /* PD1 bit */
#define ADSCTRL_DFR_SH          2       /* SER/DFR bit */
#define ADSCTRL_MOD_SH          3       /* Mode bit */
#define ADSCTRL_ADR_SH          4       /* Address setting */
#define ADSCTRL_STS_SH          7       /* Start bit */

#define GPIO_TP_INT_C3K		11
#define GPIO_HSYNC_C3K		22

#define POLL_TIMEOUT_RATE0	((hz * 150)/1000)
#define POLL_TIMEOUT_RATE1	(hz / 100) /* XXX every tick */

#define CCNT_HS_400_VGA_C3K 6250	/* 15.024us */

struct tsscale {
	int minx, maxx;
	int miny, maxy;
	int swapxy;
	int resx, resy;
} zts_scale = {
	/* C3000 */
	209, 3620, 312, 3780, 0, 640, 480
};

int	zts_match(struct device *, void *, void *);
void	zts_attach(struct device *, struct device *, void *);
int	zts_activate(struct device *, int);
int	zts_enable(void *);
void	zts_disable(void *);
void	zts_poll(void *);
int	zts_irq(void *);
int	zts_ioctl(void *, u_long, caddr_t, int, struct proc *);

struct zts_softc {
	struct device sc_dev;
	struct timeout sc_ts_poll;
	void *sc_gh;
	int sc_enabled;
	int sc_running;
	int sc_buttons; /* button emulation ? */
	struct device *sc_wsmousedev;
	int sc_oldx;
	int sc_oldy;
	int sc_rawmode;
	
	struct tsscale sc_tsscale;
};

struct cfattach zts_ca = {
	sizeof(struct zts_softc), zts_match, zts_attach, NULL,
	zts_activate
};

struct cfdriver zts_cd = {
	NULL, "zts", DV_DULL
};

int
zts_match(struct device *parent, void *cf, void *aux)
{
	return 1;
}

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

	timeout_set(&sc->sc_ts_poll, zts_poll, sc);

	/* Initialize ADS7846 Difference Reference mode */
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (1<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (3<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (4<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);
	(void)zssp_ic_send(ZSSP_IC_ADS7846,
	    (5<<ADSCTRL_ADR_SH) | (1<<ADSCTRL_STS_SH));
	delay(5000);

	a.accessops = &zts_accessops;
	a.accesscookie = sc;
	printf("\n");

	/* Copy the default scalue values to each softc */
	bcopy(&zts_scale, &sc->sc_tsscale, sizeof(sc->sc_tsscale));
		
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);
}

int
zts_enable(void *v)
{
	struct zts_softc *sc = v;

	if (sc->sc_enabled)
		return EBUSY;

	timeout_del(&sc->sc_ts_poll);

	pxa2x0_gpio_set_function(GPIO_TP_INT_C3K, GPIO_IN);

	/* XXX */
	if (sc->sc_gh == NULL)
		sc->sc_gh = pxa2x0_gpio_intr_establish(GPIO_TP_INT_C3K,
		    IST_EDGE_FALLING, IPL_TTY, zts_irq, sc,
		    sc->sc_dev.dv_xname);
	else
		pxa2x0_gpio_intr_unmask(sc->sc_gh);

	/* enable interrupts */
	sc->sc_enabled = 1;
	sc->sc_running = 1;
	sc->sc_buttons = 0;

	return 0;
}

void
zts_disable(void *v)
{
	struct zts_softc *sc = v;

	timeout_del(&sc->sc_ts_poll);

	if (sc->sc_gh != NULL) {
#if 0
		pxa2x0_gpio_intr_disestablish(sc->sc_gh);
		sc->sc_gh = NULL;
#endif
	}

	/* disable interrupts */
	sc->sc_enabled = 0;
	sc->sc_running = 0;
}

int
zts_activate(struct device *self, int act)
{
	struct zts_softc *sc = (struct zts_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		if (sc->sc_enabled == 0)
			break;
		sc->sc_running = 0;
#if 0
		pxa2x0_gpio_intr_disestablish(sc->sc_gh);
#endif
		timeout_del(&sc->sc_ts_poll);

		pxa2x0_gpio_intr_mask(sc->sc_gh);

		/* Turn off reference voltage but leave ADC on. */
		(void)zssp_ic_send(ZSSP_IC_ADS7846, (1 << ADSCTRL_PD1_SH) |
		    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH));

		pxa2x0_gpio_set_function(GPIO_TP_INT_C3K,
		    GPIO_OUT | GPIO_SET);
		break;

	case DVACT_RESUME:
		if (sc->sc_enabled == 0)
			break;
		pxa2x0_gpio_set_function(GPIO_TP_INT_C3K, GPIO_IN);
		pxa2x0_gpio_intr_mask(sc->sc_gh);

		/* Enable automatic low power mode. */
		(void)zssp_ic_send(ZSSP_IC_ADS7846,
		    (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH));

#if 0
		sc->sc_gh = pxa2x0_gpio_intr_establish(GPIO_TP_INT_C3K,
		    IST_EDGE_FALLING, IPL_TTY, zts_irq, sc,
		    sc->sc_dev.dv_xname);
#else
		pxa2x0_gpio_intr_unmask(sc->sc_gh);
#endif
		sc->sc_running = 1;
		break;
	}
	return 0;
}

struct zts_pos {
	int x;
	int y;
	int z;			/* touch pressure */
};

#define NSAMPLES 3
struct zts_pos zts_samples[NSAMPLES];
int	ztsavgloaded = 0;

int	zts_readpos(struct zts_pos *);
void	zts_avgpos(struct zts_pos *);

#define HSYNC()							\
	do {								\
		while (pxa2x0_gpio_get_bit(GPIO_HSYNC_C3K) == 0);	\
		while (pxa2x0_gpio_get_bit(GPIO_HSYNC_C3K) != 0);	\
	} while (0)

int	  pxa2x0_ccnt_enable(int);
u_int32_t pxa2x0_read_ccnt(void);
u_int32_t zts_sync_ads784x(int, int, u_int32_t);
void	  zts_sync_send(u_int32_t);

int
pxa2x0_ccnt_enable(int on)
{
	u_int32_t rv;

	on = on ? 0x1 : 0x0;
	__asm __volatile("mrc p14, 0, %0, c0, c1, 0" : "=r" (rv));
	__asm __volatile("mcr p14, 0, %0, c0, c1, 0" : : "r" (on));
	return ((int)(rv & 0x1));
}

u_int32_t
pxa2x0_read_ccnt(void)
{
	u_int32_t rv;

	__asm __volatile("mrc p14, 0, %0, c1, c1, 0" : "=r" (rv));
	return (rv);
}

/*
 * Communicate synchronously with the ADS784x touch screen controller.
 */
u_int32_t
zts_sync_ads784x(int dorecv/* XXX */, int dosend/* XXX */, u_int32_t cmd)
{
	int	ccen;
	u_int32_t rv;

	/* XXX poll hsync only if LCD is enabled */

	/* start clock counter */
	ccen = pxa2x0_ccnt_enable(1);

	HSYNC();

	if (dorecv)
		/* read SSDR and disable ADS784x */
		rv = zssp_ic_stop(ZSSP_IC_ADS7846);
	else
		rv = 0;

	if (dosend)
		zts_sync_send(cmd);

	/* stop clock counter */
	pxa2x0_ccnt_enable(ccen);

	return (rv);
}

void
zts_sync_send(u_int32_t cmd)
{
	u_int32_t tck;
	u_int32_t a, b;

	/* XXX */
	tck = CCNT_HS_400_VGA_C3K - 151;

	/* send dummy command; discard SSDR */
	(void)zssp_ic_send(ZSSP_IC_ADS7846, cmd);
	
	/* wait for refresh */
	HSYNC();

	/* wait after refresh */
	a = pxa2x0_read_ccnt();
	b = pxa2x0_read_ccnt();
	while ((b - a) < tck)
		b = pxa2x0_read_ccnt();

	/* send the actual command; keep ADS784x enabled */
	zssp_ic_start(ZSSP_IC_ADS7846, cmd);
}

int
zts_readpos(struct zts_pos *pos)
{
	int cmd;
	int t0, t1;
	int down;

	/* XXX */
	pxa2x0_gpio_set_function(GPIO_HSYNC_C3K, GPIO_IN);

	/* check that pen is down */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t0 = zssp_ic_send(ZSSP_IC_ADS7846, cmd);
	down = !(t0 < 10);
	if (down == 0)
		goto out;

	/* Y */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	(void)zts_sync_ads784x(0, 1, cmd);

	/* Y */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (1 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	(void)zts_sync_ads784x(1, 1, cmd);

	/* X */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (5 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	pos->y = zts_sync_ads784x(1, 1, cmd);

	/* T0 */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (3 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	pos->x = zts_sync_ads784x(1, 1, cmd);

	/* T1 */
	cmd = (1 << ADSCTRL_PD0_SH) | (1 << ADSCTRL_PD1_SH) |
	    (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);

	t0 = zts_sync_ads784x(1, 1, cmd);
	t1 = zts_sync_ads784x(1, 0, cmd);

	/* check that pen is still down */
	/* XXX pressure sensitivity varies with X or what? */
	if (t0 == 0 || (pos->x * (t1 - t0) / t0) >= 15000)
		down = 0;
	pos->z = down;

out:
	/* Enable automatic low power mode. */
        cmd = (4 << ADSCTRL_ADR_SH) | (1 << ADSCTRL_STS_SH);
	(void)zssp_ic_send(ZSSP_IC_ADS7846, cmd);
	
	return (down);
}

#define NAVGSAMPLES (NSAMPLES < 3 ? NSAMPLES : 3)
void
zts_avgpos(struct zts_pos *pos)
{
	struct zts_pos *tpp = zts_samples;
	int diff[NAVGSAMPLES];
	int mindiff, mindiffv;
	int n;
	int i;
	static int tail;

	if (ztsavgloaded < NAVGSAMPLES) {
		tpp[(tail + ztsavgloaded) % NSAMPLES] = *pos;
		ztsavgloaded++;
		return;
	}

	tpp[tail] = *pos;
	tail = (tail+1) % NSAMPLES;

	/* X */
	i = tail;
	for (n = 0 ; n < NAVGSAMPLES; n++) {
		int alt;
		alt = (i+1) % NSAMPLES;
		diff[n] = tpp[i].x - tpp[alt].x;
		if (diff[n] < 0)
			diff[n] = - diff[n]; /* ABS */
		i = alt;
	}
	mindiffv = diff[0];
	mindiff = 0;
	for (n = 1; n < NAVGSAMPLES; n++) {
		if (diff[n] < mindiffv) {
			mindiffv = diff[n];
			mindiff = n;
		}
	}
	pos->x = (tpp[(tail + mindiff) % NSAMPLES].x +
	    tpp[(tail + mindiff + 1) % NSAMPLES].x) / 2;

	/* Y */
	i = tail;
	for (n = 0 ; n < NAVGSAMPLES; n++) {
		int alt;
		alt = (i+1) % NSAMPLES;
		diff[n] = tpp[i].y - tpp[alt].y;
		if (diff[n] < 0)
			diff[n] = - diff[n]; /* ABS */
		i = alt;
	}
	mindiffv = diff[0];
	mindiff = 0;
	for (n = 1; n < NAVGSAMPLES; n++) {
		if (diff[n] < mindiffv) {
			mindiffv = diff[n];
			mindiff = n;
		}
	}
	pos->y = (tpp[(tail + mindiff) % NSAMPLES].y +
	    tpp[(tail + mindiff + 1) % NSAMPLES].y) / 2;
}

void
zts_poll(void *v)
{
	int s;

	s = spltty();
	(void)zts_irq(v);
	splx(s);
}

#define TS_STABLE 8
int
zts_irq(void *v)
{
	struct zts_softc *sc = v;
	struct zts_pos tp;
	int s;
	int pindown;
	int down;
	extern int zkbd_modstate;

	if (!sc->sc_running)
		return 0;

	s = splhigh();
	pindown = pxa2x0_gpio_get_bit(GPIO_TP_INT_C3K) ? 0 : 1;
	if (pindown) {
		pxa2x0_gpio_intr_mask(sc->sc_gh);
		timeout_add(&sc->sc_ts_poll, POLL_TIMEOUT_RATE1);
	}

	down = zts_readpos(&tp);

	if (!pindown) {
		pxa2x0_gpio_intr_unmask(sc->sc_gh);
		timeout_add(&sc->sc_ts_poll, POLL_TIMEOUT_RATE0);
		ztsavgloaded = 0;
	}
	pxa2x0_gpio_clear_intr(GPIO_TP_INT_C3K);
	splx(s);
	
	if (down) {
		zts_avgpos(&tp);
		if (!sc->sc_rawmode &&
		    (sc->sc_tsscale.maxx - sc->sc_tsscale.minx) != 0 &&
		    (sc->sc_tsscale.maxy - sc->sc_tsscale.miny) != 0) {
			/* Scale down to the screen resolution. */
			tp.x = ((tp.x - sc->sc_tsscale.minx) *
			    sc->sc_tsscale.resx) /
			    (sc->sc_tsscale.maxx - sc->sc_tsscale.minx);
			tp.y = ((tp.y - sc->sc_tsscale.miny) *
			    sc->sc_tsscale.resy) /
			    (sc->sc_tsscale.maxy - sc->sc_tsscale.miny);
		}
	}

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
		tp.x = sc->sc_oldx;
		tp.y = sc->sc_oldy;
	}

	if (down || sc->sc_buttons != down) {
		DPRINTF(("%s: tp.z = %d, tp.x = %d, tp.y = %d\n",
		    sc->sc_dev.dv_xname, tp.z, tp.x, tp.y));

		wsmouse_input(sc->sc_wsmousedev, down, tp.x, tp.y,
		    0 /* z */, 0 /* w */,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z);
		sc->sc_buttons = down;
		sc->sc_oldx = tp.x;
		sc->sc_oldy = tp.y;
	}

	return 1;
}

int
zts_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error = 0;
	struct zts_softc *sc = v;
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;

	DPRINTF(("zts_ioctl(%d, '%c', %d)\n",
	    IOCPARM_LEN(cmd), IOCGROUP(cmd), cmd & 0xff));

	switch (cmd) {
	case WSMOUSEIO_SCALIBCOORDS:
		if (!(wsmc->minx >= 0 && wsmc->maxx >= 0 &&
		    wsmc->miny >= 0 && wsmc->maxy >= 0 &&
		    wsmc->resx >= 0 && wsmc->resy >= 0 &&
		    wsmc->minx < 32768 && wsmc->maxx < 32768 &&
		    wsmc->miny < 32768 && wsmc->maxy < 32768 &&
		    (wsmc->maxx - wsmc->minx) != 0 &&
		    (wsmc->maxy - wsmc->miny) != 0 &&
		    wsmc->resx < 32768 && wsmc->resy < 32768 &&
		    wsmc->swapxy >= 0 && wsmc->swapxy <= 1 &&
		    wsmc->samplelen >= 0 && wsmc->samplelen <= 1))
			return (EINVAL);

		sc->sc_tsscale.minx = wsmc->minx;
		sc->sc_tsscale.maxx = wsmc->maxx;
		sc->sc_tsscale.miny = wsmc->miny;
		sc->sc_tsscale.maxy = wsmc->maxy;
		sc->sc_tsscale.swapxy = wsmc->swapxy;
		sc->sc_tsscale.resx = wsmc->resx;
		sc->sc_tsscale.resy = wsmc->resy;
		sc->sc_rawmode = wsmc->samplelen;
		break;
	case WSMOUSEIO_GCALIBCOORDS:
		wsmc->minx = sc->sc_tsscale.minx;
		wsmc->maxx = sc->sc_tsscale.maxx;
		wsmc->miny = sc->sc_tsscale.miny;
		wsmc->maxy = sc->sc_tsscale.maxy;
		wsmc->swapxy = sc->sc_tsscale.swapxy;
		wsmc->resx = sc->sc_tsscale.resx;
		wsmc->resy = sc->sc_tsscale.resy;
		wsmc->samplelen = sc->sc_rawmode;
		break;
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_TPANEL;
		break;
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}
