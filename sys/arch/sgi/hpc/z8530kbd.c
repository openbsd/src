/*	$OpenBSD: z8530kbd.c,v 1.7 2014/12/07 13:10:45 miod Exp $	*/
/*	$NetBSD: zs_kbd.c,v 1.8 2008/03/29 19:15:35 tsutsui Exp $	*/

/*
 * Copyright (c) 2004 Steve Rumble
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * IP20 serial keyboard driver attached to zs channel 0 at 600bps.
 * This layer is the parent of wskbd.
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <machine/autoconf.h>
#include <mips64/archtype.h>

#include <dev/ic/z8530reg.h>
#include <machine/z8530var.h>

#define ZSKBD_BAUD		600
#define ZSKBD_TXQ_LEN		16		/* power of 2 */
#define ZSKBD_RXQ_LEN		64		/* power of 2 */

#define ZSKBD_DIP_SYNC		0x6e		/* 110 */
#define ZSKBD_INTL_KEY		0x6f		/* 111 */
#define ZSKBD_KEY_UP		0x80
#define ZSKBD_KEY_ALL_UP	0xf0

#ifdef ZSKBD_DEBUG
int zskbd_debug = 0;

#define DPRINTF(_x) if (zskbd_debug) printf _x
#else
#define DPRINTF(_x)
#endif

struct zskbd_softc {
	struct device sc_dev;

	struct zskbd_devconfig *sc_dc;

#ifdef WSDISPLAY_COMPAT_RAWKBD
	int		sc_rawkbd;
#endif
};

struct zskbd_devconfig {
	/* transmit tail-chasing fifo */
	uint8_t		txq[ZSKBD_TXQ_LEN];
	u_int		txq_head;
	u_int		txq_tail;

	/* receive tail-chasing fifo */
	uint8_t		rxq[ZSKBD_RXQ_LEN];
	u_int		rxq_head;
	u_int		rxq_tail;

	/* number of non-keystroke bytes expected */
	int		expected;

	/* keyboard configuration */
#define ZSKBD_CTRL_A		0x0
#define ZSKBD_CTRL_A_SBEEP	0x2	/* 200 ms */
#define ZSKBD_CTRL_A_LBEEP	0x4	/* 1000 ms */
#define ZSKBD_CTRL_A_NOCLICK	0x8	/* turn off keyboard click */
#define ZSKBD_CTRL_A_RCB	0x10	/* request config byte */
#define ZSKBD_CTRL_A_NUMLK	0x20	/* num lock led */
#define ZSKBD_CTRL_A_CAPSLK	0x40	/* caps lock led */
#define ZSKBD_CTRL_A_AUTOREP	0x80	/* auto-repeat after 650 ms, 28x/sec */

#define ZSKBD_CTRL_B		0x1
#define ZSKBD_CTRL_B_CMPL_DS1_2	0x2	/* complement of ds1+ds2 (num+capslk) */
#define ZSKBD_CTRL_B_SCRLK	0x4	/* scroll lock light */
#define ZSKBD_CTRL_B_L1		0x8	/* user-configurable lights */
#define ZSKBD_CTRL_B_L2		0x10
#define ZSKBD_CTRL_B_L3		0x20
#define ZSKBD_CTRL_B_L4		0x40
	uint8_t		kbd_conf[2];

	/* dip switch settings */
	uint8_t		dip;

	/* wscons glue */
	struct device  *wskbddev;
	int		enabled;
};

struct zskbd_devconfig zskbd_consdc;

int	zskbd_match(struct device *, void *, void *);
void	zskbd_attach(struct device *, struct device *, void *);

struct cfdriver zskbd_cd = {
	NULL, "zskbd", DV_DULL
};

const struct cfattach zskbd_ca = {
	sizeof(struct zskbd_softc), zskbd_match, zskbd_attach
};

void	zskbd_rxint(struct zs_chanstate *);
void	zskbd_stint(struct zs_chanstate *, int);
void	zskbd_txint(struct zs_chanstate *);
void	zskbd_softint(struct zs_chanstate *);
int	zskbd_send(struct zs_chanstate *, uint8_t *, u_int);
int	zskbd_poll(struct zs_chanstate *, uint8_t *);
void	zskbd_ctrl(struct zs_chanstate *, uint8_t, uint8_t, uint8_t, uint8_t);
int	zskbd_process(struct zskbd_devconfig *, uint8_t);

void	zskbd_wskbd_input(struct zs_chanstate *, uint8_t);
int	zskbd_wskbd_enable(void *, int);
void	zskbd_wskbd_set_leds(void *, int);
int	zskbd_wskbd_get_leds(void *);
void	zskbd_wskbd_set_keyclick(void *, int);
int	zskbd_wskbd_get_keyclick(void *);
int	zskbd_wskbd_ioctl(void *, u_long, caddr_t, int, struct proc *);

void	zskbd_cnattach(int, int);
void	zskbd_wskbd_getc(void *, u_int *, int *);
void	zskbd_wskbd_pollc(void *, int);
void	zskbd_wskbd_bell(void *, u_int, u_int, u_int);

extern struct zschan   *zs_get_chan_addr(int, int);
extern int		zs_getc(void *);
extern void		zs_putc(void *, int);

static struct zsops zskbd_zsops = {
	zskbd_rxint,
	zskbd_stint,
	zskbd_txint,
	zskbd_softint
};

extern const struct wscons_keydesc wssgi_keydesctab[];
struct wskbd_mapdata sgikbd_wskbd_keymapdata = {
	wssgi_keydesctab,
	KB_US | KB_DEFAULT
};

const int zskbd_layouts[] = {
	KB_US,
	KB_DE,
	KB_FR,
	KB_IT,
	KB_DK,
	KB_ES,
	KB_NO,
	KB_SV,
	KB_SF,
	KB_UK,
	KB_BE,
	KB_SG,
	KB_NL,
	-1,	/* finnish */
	KB_PT,
	-1	/* greek */
};

const struct wskbd_accessops zskbd_wskbd_accessops = {
	zskbd_wskbd_enable,
	zskbd_wskbd_set_leds,
	zskbd_wskbd_ioctl
};

const struct wskbd_consops zskbd_wskbd_consops = {
	zskbd_wskbd_getc,
	zskbd_wskbd_pollc,
	zskbd_wskbd_bell
};

int			zskbd_is_console = 0;

int
zskbd_match(struct device *parent, void *vcf, void *aux)
{
	if (sys_config.system_type == SGI_IP20) {
		struct zsc_attach_args *args = aux;

		if (args->channel == 0)
			return 1;
	}

	return 0;
}

void
zskbd_attach(struct device *parent, struct device *self, void *aux)
{
	struct zskbd_softc *sc = (struct zskbd_softc *)self;
	struct zsc_softc *zsc = (struct zsc_softc *)parent;
	struct zsc_attach_args *args = aux;
	struct zs_chanstate *cs;
	struct zskbd_devconfig *dc;
	struct wskbddev_attach_args wskaa;
	int s, channel, rc;
	uint8_t key;

	printf(": ");

	/* Establish ourself with the MD z8530 driver */
	channel = args->channel;
	cs = zsc->zsc_cs[channel];
	cs->cs_ops = &zskbd_zsops;
	cs->cs_private = sc;

	if (zskbd_is_console)
		dc = &zskbd_consdc;
	else
		dc = malloc(sizeof(struct zskbd_devconfig), M_DEVBUF,
		    M_WAITOK | M_ZERO);
	sc->sc_dc = dc;

	s = splzs();
	zs_write_reg(cs, 9, (channel == 0) ? ZSWR9_A_RESET : ZSWR9_B_RESET);
	cs->cs_preg[1] = ZSWR1_RIE | ZSWR1_TIE;
	cs->cs_preg[4] = (cs->cs_preg[4] & ZSWR4_CLK_MASK) |
			 (ZSWR4_ONESB | ZSWR4_PARENB);	/* 1 stop, odd parity */
	cs->cs_preg[15] &= ~ZSWR15_ENABLE_ENHANCED;
	zs_set_speed(cs, ZSKBD_BAUD);
	zs_loadchannelregs(cs);

	/*
	 * Empty the keyboard input buffer (if the keyboard is the console
	 * input device and the user invoked UKC, the `enter key up' event
	 * will still be pending in the buffer).
	 */
	while ((zs_read_csr(cs) & ZSRR0_RX_READY) != 0)
		(void)zs_read_data(cs);

	if (!zskbd_is_console) {
		/*
		 * Ask the keyboard for its DIP switch settings. This will
		 * also let us know whether the keyboard is connected.
		 */
		dc->expected = 2;
		zskbd_ctrl(cs, ZSKBD_CTRL_A_RCB, 0, 0, 0);
		while (dc->expected != 0) {
			rc = zskbd_poll(cs, &key);
			if (rc != 0) {
				if (rc == ENXIO && dc->expected == 2) {
					printf("no keyboard");
					/*
					 * Attach wskbd nevertheless, in case
					 * the keyboard is plugged late.
					 */
					dc->expected = 0;
					goto dip;
				} else {
					printf("i/o error\n");
					return;
				}
			}

			zskbd_process(dc, key);
		}
	}

	printf("dipsw %02x", dc->dip);
	if (dc->dip < nitems(zskbd_layouts) && zskbd_layouts[dc->dip] != -1)
		sgikbd_wskbd_keymapdata.layout = zskbd_layouts[dc->dip];
dip:

	/*
	 * Disable key click by default. Note that if the keyboard is not
	 * currently connected, the bit will nevertheless stick and will
	 * disable the click as soon as a keyboard led needs to be lit.
	 */
	zskbd_ctrl(cs, ZSKBD_CTRL_A_NOCLICK, 0, 0, 0);

	splx(s);

	printf("\n");

	if (zskbd_is_console)
		dc->enabled = 1;

	/* attach wskbd */
	wskaa.console = zskbd_is_console;
	wskaa.keymap = &sgikbd_wskbd_keymapdata;
	wskaa.accessops = &zskbd_wskbd_accessops;
	wskaa.accesscookie = cs;
	dc->wskbddev = config_found(self, &wskaa, wskbddevprint);
}

void
zskbd_rxint(struct zs_chanstate *cs)
{
	struct zskbd_softc *sc = cs->cs_private;
	struct zskbd_devconfig *dc = sc->sc_dc;
	uint8_t c, r;

	/* clear errors */
	r = zs_read_reg(cs, 1);
	if (r & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE))
		zs_write_csr(cs, ZSWR0_RESET_ERRORS);

	/* read byte and append to our queue */
	c = zs_read_data(cs);

	dc->rxq[dc->rxq_tail] = c;
	dc->rxq_tail = (dc->rxq_tail + 1) & ~ZSKBD_RXQ_LEN;

	cs->cs_softreq = 1;
}

void
zskbd_stint(struct zs_chanstate *cs, int force)
{
	zs_write_csr(cs, ZSWR0_RESET_STATUS);
	cs->cs_softreq = 1;
}

void
zskbd_txint(struct zs_chanstate *cs)
{
	zs_write_reg(cs, 0, ZSWR0_RESET_TXINT);
	cs->cs_softreq = 1;
}

void
zskbd_softint(struct zs_chanstate *cs)
{
	struct zskbd_softc *sc = cs->cs_private;
	struct zskbd_devconfig *dc = sc->sc_dc;
	int rr0;
	uint8_t key;

	/* handle pending transmissions */
	if (dc->txq_head != dc->txq_tail) {
		int s;

		s = splzs();
		while (dc->txq_head != dc->txq_tail) {
			rr0 = zs_read_csr(cs);
			if ((rr0 & ZSRR0_TX_READY) == 0)
				break;
			zs_write_data(cs, dc->txq[dc->txq_head]);
			dc->txq_head = (dc->txq_head + 1) & ~ZSKBD_TXQ_LEN;
		}
		splx(s);
	}

	/* handle incoming keystrokes/config */
	while (dc->rxq_head != dc->rxq_tail) {
		key = dc->rxq[dc->rxq_head];
		dc->rxq_head = (dc->rxq_head + 1) & ~ZSKBD_RXQ_LEN;
		if (zskbd_process(dc, key) != 0) {
			/*
			 * The `international' key (only found in non-us
			 * layouts) is supposed to be keycode 111, but is
			 * apparently 110 (same as the status byte prefix)
			 * on some (all?) models.
			 */
			if (key == ZSKBD_DIP_SYNC)
				key = ZSKBD_INTL_KEY;

			/* toss wskbd a bone */
			if (dc->enabled)
				zskbd_wskbd_input(cs, key);
		}
	}
}

int
zskbd_process(struct zskbd_devconfig *dc, uint8_t key)
{
	switch (dc->expected) {
	case 2:
		if (key != ZSKBD_DIP_SYNC) {
			/* only during attach, thus no device name prefix */
			printf("unexpected configuration byte header"
			    " (%02x), ", key);
			/* transition state anyway */
		}
		dc->expected--;
		return 0;
	case 1:
		dc->dip = key;
		dc->expected--;
		return 0;
	default:
	case 0:
		return 1;
	}
}

/* expects to be in splzs() */
int
zskbd_send(struct zs_chanstate *cs, uint8_t *c, u_int len)
{
	struct zskbd_softc *sc = cs->cs_private;
	struct zskbd_devconfig *dc = sc->sc_dc;
	u_int i;
	int rr0;

	while (len != 0) {
		rr0 = zs_read_csr(cs);
		if ((rr0 & ZSRR0_TX_READY) == 0) {
			/*
			 * poll until whole transmission complete during
			 * autoconf
			 */
			if (cold) {
				for (i = 1000; i != 0; i--) {
					if ((rr0 & ZSRR0_TX_READY) != 0)
						break;
					delay(100);
				}
				if (i == 0)
					return EIO;
			} else
				break;
		}
		zs_write_data(cs, *c++);
		len--;
	}

	/*
	 * Enqueue any remaining bytes.
	 */
	while (len != 0) {
		dc->txq[dc->txq_tail] = *c++;
		dc->txq_tail = (dc->txq_tail + 1) & ~ZSKBD_TXQ_LEN;
		len--;
		cs->cs_softreq = 1;
	}

	return 0;
}

/* expects to be in splzs() */
int
zskbd_poll(struct zs_chanstate *cs, uint8_t *key)
{
	u_int i;
	int rr0, rr1, c;

	for (i = 1000; i != 0; i--) {
		rr0 = zs_read_csr(cs);
		if ((rr0 & ZSRR0_RX_READY) != 0)
			break;
		delay(100);
	}
	if (i == 0)
		return ENXIO;

	rr1 = zs_read_reg(cs, 1);
	c = zs_read_data(cs);

	if (rr1 & (ZSRR1_FE | ZSRR1_DO | ZSRR1_PE))
		return EIO;

	*key = (uint8_t)c;
	return 0;
}

/* expects to be in splzs() */
void
zskbd_ctrl(struct zs_chanstate *cs, uint8_t a_on, uint8_t a_off, uint8_t b_on,
    uint8_t b_off)
{
	struct zskbd_softc *sc = cs->cs_private;
	struct zskbd_devconfig *dc = sc->sc_dc;

	dc->kbd_conf[ZSKBD_CTRL_A] |=   a_on;
	dc->kbd_conf[ZSKBD_CTRL_A] &= ~(a_off | ZSKBD_CTRL_B);
	dc->kbd_conf[ZSKBD_CTRL_B] &=  ~b_off;
	dc->kbd_conf[ZSKBD_CTRL_B] |=  (b_on | ZSKBD_CTRL_B);

	zskbd_send(cs, dc->kbd_conf, 2);

	/* make sure we don't resend these each time */
	dc->kbd_conf[ZSKBD_CTRL_A] &= ~(ZSKBD_CTRL_A_RCB | ZSKBD_CTRL_A_SBEEP |
	    ZSKBD_CTRL_A_LBEEP);
}

/******************************************************************************
 * wskbd glue
 ******************************************************************************/

void
zskbd_wskbd_input(struct zs_chanstate *cs, uint8_t key)
{
	struct zskbd_softc *sc = cs->cs_private;
	u_int type;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	int s;
#endif

	if (sc->sc_dc->wskbddev == NULL)
		return;	/* why bother */

	if (key & ZSKBD_KEY_UP) {
		if ((key & ZSKBD_KEY_ALL_UP) == ZSKBD_KEY_ALL_UP)
			type = WSCONS_EVENT_ALL_KEYS_UP;
		else
			type = WSCONS_EVENT_KEY_UP;
	} else
		type = WSCONS_EVENT_KEY_DOWN;

	wskbd_input(sc->sc_dc->wskbddev, type, (key & ~ZSKBD_KEY_UP));

	DPRINTF(("zskbd_wskbd_input: inputted key 0x%x\n", key));

#ifdef WSDISPLAY_COMPAT_RAWKBD
	if (sc->sc_rawkbd &&
	    type != WSCONS_EVENT_ALL_KEYS_UP) {
		s = spltty();
		wskbd_rawinput(sc->sc_dc->wskbddev, &key, 1);
		splx(s);
	}
#endif
}

int
zskbd_wskbd_enable(void *cookie, int on)
{
	struct zs_chanstate *cs = cookie;
	struct zskbd_softc *sc = cs->cs_private;

	if (on) {
		if (sc->sc_dc->enabled)
			return (EBUSY);
		else
			sc->sc_dc->enabled = 1;
	} else
		sc->sc_dc->enabled = 0;

	DPRINTF(("zskbd_wskbd_enable: %s\n", on ? "enabled" : "disabled"));

	return (0);
}

void
zskbd_wskbd_set_leds(void *cookie, int leds)
{
	struct zs_chanstate *cs = cookie;
	int 	s;
	uint8_t	a_on, a_off, b_on, b_off;

	a_on = a_off = b_on = b_off = 0;

	if (leds & WSKBD_LED_CAPS)
		a_on |=  ZSKBD_CTRL_A_CAPSLK;
	else
		a_off |= ZSKBD_CTRL_A_CAPSLK;

	if (leds & WSKBD_LED_NUM)
		a_on |=  ZSKBD_CTRL_A_NUMLK;
	else
		a_off |= ZSKBD_CTRL_A_NUMLK;

	if (leds & WSKBD_LED_SCROLL)
		b_on |=  ZSKBD_CTRL_B_SCRLK;
	else
		b_off |= ZSKBD_CTRL_B_SCRLK;

	s = splzs();
	zskbd_ctrl(cs, a_on, a_off, b_on, b_off);
	splx(s);
}

int
zskbd_wskbd_get_leds(void *cookie)
{
	struct zs_chanstate *cs = cookie;
	struct zskbd_softc *sc = cs->cs_private;
	int leds;

	leds = 0;

	if (sc->sc_dc->kbd_conf[ZSKBD_CTRL_A] & ZSKBD_CTRL_A_NUMLK)
		leds |= WSKBD_LED_NUM;

	if (sc->sc_dc->kbd_conf[ZSKBD_CTRL_A] & ZSKBD_CTRL_A_CAPSLK)
		leds |= WSKBD_LED_CAPS;

	if (sc->sc_dc->kbd_conf[ZSKBD_CTRL_B] & ZSKBD_CTRL_B_SCRLK)
		leds |= WSKBD_LED_SCROLL;

	return (leds);
}

#if 0
void
zskbd_wskbd_set_keyclick(void *cookie, int on)
{
	struct zs_chanstate *cs = cookie;
	int s;

	if (on) {
		if (!zskbd_wskbd_get_keyclick(cookie)) {
			s = splzs();
			zskbd_ctrl(cs, 0, ZSKBD_CTRL_A_NOCLICK, 0, 0);
			splx(s);
		}
	} else {
		if (zskbd_wskbd_get_keyclick(cookie)) {
			s = splzs();
			zskbd_ctrl(cs, ZSKBD_CTRL_A_NOCLICK, 0, 0, 0);
			splx(s);
		}
	}
}

int
zskbd_wskbd_get_keyclick(void *cookie)
{
	struct zs_chanstate *cs = cookie;
	struct zskbd_softc *sc = cs->cs_private;

	if (sc->sc_dc->kbd_conf[ZSKBD_CTRL_A] & ZSKBD_CTRL_A_NOCLICK)
		return (0);
	else
		return (1);
}
#endif

int
zskbd_wskbd_ioctl(void *cookie, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct zs_chanstate *cs = cookie;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct zskbd_softc *sc = cs->cs_private;
#endif

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_SGI;
		break;
	case WSKBDIO_SETLEDS:
		zskbd_wskbd_set_leds(cs, *(int *)data);
		break;
	case WSKBDIO_GETLEDS:
		*(int *)data = zskbd_wskbd_get_leds(cs);
		break;
#if 0
	case WSKBDIO_SETKEYCLICK:
		zskbd_wskbd_set_keyclick(cs, *(int *)data);
		break;
	case WSKBDIO_GETKEYCLICK:
		*(int *)data = zskbd_wskbd_get_keyclick(cs);
		break;
#endif
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		break;
#endif
	default:
		return -1;
	}

	return 0;
}

/*
 * console routines
 */
void
zskbd_cnattach(int zsunit, int zschan)
{
	struct zschan *zs;
	struct zskbd_devconfig *dc;

	zs = zs_get_chan_addr(zsunit, zschan);
	dc = &zskbd_consdc;

	/*
	 * Try and figure out our dip switches early, in order to pick
	 * the right keyboard layout.
	 */
	dc->expected = 2;
	zs_putc(zs, ZSKBD_CTRL_A_RCB);
	zs_putc(zs, ZSKBD_CTRL_B);	/* unnecessary? */

	while (dc->expected != 0) {
		zskbd_process(dc, zs_getc(zs));
	}

	if (dc->dip < nitems(zskbd_layouts) && zskbd_layouts[dc->dip] != -1)
		sgikbd_wskbd_keymapdata.layout = zskbd_layouts[dc->dip];

	wskbd_cnattach(&zskbd_wskbd_consops, zs, &sgikbd_wskbd_keymapdata);
	zskbd_is_console = 1;
}

void
zskbd_wskbd_getc(void *cookie, u_int *type, int *data)
{
	int key;

	key = zs_getc(cookie);

	if (key & ZSKBD_KEY_UP)
		*type = WSCONS_EVENT_KEY_UP;
	else
		*type = WSCONS_EVENT_KEY_DOWN;

	*data = key & ~ZSKBD_KEY_UP;
}

void
zskbd_wskbd_pollc(void *cookie, int on)
{
}

void
zskbd_wskbd_bell(void *cookie, u_int pitch, u_int period, u_int volume)
{
	/*
	 * Since we don't have any state, this'll nuke our lights,
	 * key click, and other bits in ZSKBD_CTRL_A.
	 */
	if (period >= 1000)
		zs_putc(cookie, ZSKBD_CTRL_A_LBEEP);
	else
		zs_putc(cookie, ZSKBD_CTRL_A_SBEEP);
}
