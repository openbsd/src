/*	$OpenBSD: z8530kbd.c,v 1.1 2002/01/15 22:00:12 jason Exp $	*/
/*	$NetBSD: z8530tty.c,v 1.77 2001/05/30 15:24:24 lukem Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996, 1997, 1998, 1999
 *	Charles M. Hannum.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
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
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)zs.c	8.1 (Berkeley) 7/19/93
 */

/*
 * Zilog Z8530 Dual UART driver (tty interface)
 *
 * This is the "slave" driver that will be attached to
 * the "zsc" driver for plain "tty" async. serial lines.
 *
 * Credits, history:
 *
 * The original version of this code was the sparc/dev/zs.c driver
 * as distributed with the Berkeley 4.4 Lite release.  Since then,
 * Gordon Ross reorganized the code into the current parent/child
 * driver scheme, separating the Sun keyboard and mouse support
 * into independent child drivers.
 *
 * RTS/CTS flow-control support was a collaboration of:
 *	Gordon Ross <gwr@netbsd.org>,
 *	Bill Studenmund <wrstuden@loki.stanford.edu>
 *	Ian Dall <Ian.Dall@dsto.defence.gov.au>
 *
 * The driver was massively overhauled in November 1997 by Charles Hannum,
 * fixing *many* bugs, and substantially improving performance.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

#include <sparc64/dev/z8530reg.h>
#include <machine/z8530var.h>

#include <dev/cons.h>

/*
 * How many input characters we can buffer.
 * The port-specific var.h may override this.
 * Note: must be a power of two!
 */
#ifndef	ZSKBD_RING_SIZE
#define	ZSKBD_RING_SIZE	2048
#endif

struct cfdriver zskbd_cd = {
	NULL, "zskbd", DV_TTY
};

/*
 * Make this an option variable one can patch.
 * But be warned:  this must be a power of 2!
 */
u_int zskbd_rbuf_size = ZSKBD_RING_SIZE;

/* Stop input when 3/4 of the ring is full; restart when only 1/4 is full. */
u_int zskbd_rbuf_hiwat = (ZSKBD_RING_SIZE * 1) / 4;
u_int zskbd_rbuf_lowat = (ZSKBD_RING_SIZE * 3) / 4;

struct zskbd_softc {
	struct	device zst_dev;		/* required first: base device */
	struct  tty *zst_tty;
	struct	zs_chanstate *zst_cs;

	struct timeout zst_diag_ch;

	u_int zst_overflows,
	      zst_floods,
	      zst_errors;

	int zst_hwflags,	/* see z8530var.h */
	    zst_swflags;	/* TIOCFLAG_SOFTCAR, ... <ttycom.h> */

	u_int zst_r_hiwat,
	      zst_r_lowat;
	u_char *volatile zst_rbget,
	       *volatile zst_rbput;
	volatile u_int zst_rbavail;
	u_char *zst_rbuf,
	       *zst_ebuf;

	/*
	 * The transmit byte count and address are used for pseudo-DMA
	 * output in the hardware interrupt code.  PDMA can be suspended
	 * to get pending changes done; heldtbc is used for this.  It can
	 * also be stopped for ^S; this sets TS_TTSTOP in tp->t_state.
	 */
	u_char *zst_tba;		/* transmit buffer address */
	u_int zst_tbc,			/* transmit byte count */
	      zst_heldtbc;		/* held tbc while xmission stopped */

	/* Flags to communicate with zskbd_softint() */
	volatile u_char zst_rx_flags,	/* receiver blocked */
#define	RX_TTY_BLOCKED		0x01
#define	RX_TTY_OVERFLOWED	0x02
#define	RX_IBUF_BLOCKED		0x04
#define	RX_IBUF_OVERFLOWED	0x08
#define	RX_ANY_BLOCK		0x0f
			zst_tx_busy,	/* working on an output chunk */
			zst_tx_done,	/* done with one output chunk */
			zst_tx_stopped,	/* H/W level stop (lost CTS) */
			zst_st_check,	/* got a status interrupt */
			zst_rx_ready;

	/* PPS signal on DCD, with or without inkernel clock disciplining */
	u_char  zst_ppsmask;			/* pps signal mask */
	u_char  zst_ppsassert;			/* pps leading edge */
	u_char  zst_ppsclear;			/* pps trailing edge */

	struct device *zst_wskbddev;
	int zst_leds;				/* LED status */
	u_int8_t zst_kbdstate;			/* keyboard state */
	int zst_layout;				/* current layout */
};

/* Macros to clear/set/test flags. */
#define SET(t, f)	(t) |= (f)
#define CLR(t, f)	(t) &= ~(f)
#define ISSET(t, f)	((t) & (f))

/* Definition of the driver for autoconfig. */
static int	zskbd_match(struct device *, void *, void *);
static void	zskbd_attach(struct device *, struct device *, void *);

struct cfattach zskbd_ca = {
	sizeof(struct zskbd_softc), zskbd_match, zskbd_attach
};

struct zsops zsops_kbd;

static void	zsstart __P((struct tty *));
static int	zsparam __P((struct tty *, struct termios *));
static void zs_modem __P((struct zskbd_softc *, int));
static int    zshwiflow __P((struct tty *, int));
static void  zs_hwiflow __P((struct zskbd_softc *));
static void zs_maskintr __P((struct zskbd_softc *));

struct zskbd_softc *zskbd_device_lookup __P((struct cfdriver *, int));

/* Low-level routines. */
static void zskbd_rxint   __P((struct zs_chanstate *));
static void zskbd_stint   __P((struct zs_chanstate *, int));
static void zskbd_txint   __P((struct zs_chanstate *));
static void zskbd_softint __P((struct zs_chanstate *));
static void zskbd_diag __P((void *));

void zskbd_init __P((struct zskbd_softc *));
void zskbd_putc __P((struct zskbd_softc *, u_int8_t));
void zskbd_raw __P((struct zskbd_softc *, u_int8_t));

/* wskbd glue */
int zskbd_enable __P((void *, int));
void zskbd_set_leds __P((void *, int));
int zskbd_get_leds __P((void *));
int zskbd_ioctl __P((void *, u_long, caddr_t, int, struct proc *));
void zskbd_cngetc __P((void *, u_int *, int *));
void zskbd_cnpollc __P((void *, int));

struct wskbd_accessops zskbd_accessops = {
	zskbd_enable,
	zskbd_set_leds,
	zskbd_ioctl
};

struct wskbd_consops zskbd_consops = {
	zskbd_cngetc,
	zskbd_cnpollc
};

#define	KC(n)	KS_KEYCODE(n)
const keysym_t zskbd_keydesc_us[] = {
    KC(0x02),				KS_Cmd_BrightnessDown,
    KC(0x04),				KS_Cmd_BrightnessUp,
    KC(0x05),				KS_f1,
    KC(0x06),				KS_f2,
    KC(0x07),				KS_f10,
    KC(0x08),				KS_f3,
    KC(0x09),				KS_f11,
    KC(0x0a),				KS_f4,
    KC(0x0b),				KS_f12,
    KC(0x0c),				KS_f5,
    KC(0x0d),				KS_Alt_R,
    KC(0x0e),				KS_f6,
    KC(0x10),				KS_f7,
    KC(0x11),				KS_f8,
    KC(0x12),				KS_f9,
    KC(0x13),				KS_Alt_L,
    KC(0x14),				KS_Up,
    KC(0x15),				KS_Pause,
    KC(0x16),				KS_Print_Screen,
    KC(0x18),				KS_Left,
    KC(0x1b),				KS_Down,
    KC(0x1c),				KS_Right,
    KC(0x1d), KS_Cmd_Debugger,		KS_Escape,
    KC(0x1e),				KS_1,		KS_exclam,
    KC(0x1f),				KS_2,		KS_at,
    KC(0x20),				KS_3,		KS_numbersign,
    KC(0x21),				KS_4,		KS_dollar,
    KC(0x22),				KS_5,		KS_percent,
    KC(0x23),				KS_6,		KS_asciicircum,
    KC(0x24),				KS_7,		KS_ampersand,
    KC(0x25),				KS_8,		KS_asterisk,
    KC(0x26),				KS_9,		KS_parenleft,
    KC(0x27),				KS_0,		KS_parenright,
    KC(0x28),				KS_minus,	KS_underscore,
    KC(0x29),				KS_equal,	KS_plus,
    KC(0x2a),				KS_grave,	KS_asciitilde,
    KC(0x2b),				KS_BackSpace,
    KC(0x2c),				KS_Insert,
    KC(0x2d),				KS_KP_Equal,
    KC(0x2e),				KS_KP_Divide,
    KC(0x2f),				KS_KP_Multiply,
    KC(0x32),				KS_KP_Delete,
    KC(0x34),				KS_Home,
    KC(0x35),				KS_Tab,
    KC(0x36),				KS_q,
    KC(0x37),				KS_w,
    KC(0x38),				KS_e,
    KC(0x39),				KS_r,
    KC(0x3a),				KS_t,
    KC(0x3b),				KS_y,
    KC(0x3c),				KS_u,
    KC(0x3d),				KS_i,
    KC(0x3e),				KS_o,
    KC(0x3f),				KS_p,
    KC(0x40),				KS_bracketleft,	KS_braceleft,
    KC(0x41),				KS_bracketright,KS_braceright,
    KC(0x42),				KS_Delete,
    KC(0x43),				KS_Multi_key,
    KC(0x44),				KS_KP_Home,	KS_KP_7,
    KC(0x45),				KS_KP_Up,	KS_KP_8,
    KC(0x46),				KS_KP_Prior,	KS_KP_9,
    KC(0x47),				KS_KP_Subtract,
    KC(0x4a),				KS_End,
    KC(0x4c),	 KS_Cmd1,		KS_Control_L,
    KC(0x4d),				KS_a,
    KC(0x4e),				KS_s,
    KC(0x4f),				KS_d,
    KC(0x50),				KS_f,
    KC(0x51),				KS_g,
    KC(0x52),				KS_h,
    KC(0x53),				KS_j,
    KC(0x54),				KS_k,
    KC(0x55),				KS_l,
    KC(0x56),				KS_semicolon,	KS_colon,
    KC(0x57),				KS_apostrophe,	KS_quotedbl,
    KC(0x58),				KS_backslash,	KS_bar,
    KC(0x59),				KS_Return,
    KC(0x5a),				KS_KP_Enter,
    KC(0x5b),				KS_KP_Left,	KS_KP_4,
    KC(0x5c),				KS_KP_Begin,	KS_KP_5,
    KC(0x5d),				KS_KP_Right,	KS_KP_6,
    KC(0x5e),				KS_KP_Insert,	KS_KP_0,
    KC(0x5f),				KS_Find,
    KC(0x60),				KS_Prior,
    KC(0x62),				KS_Num_Lock,
    KC(0x63),				KS_Shift_L,
    KC(0x64),				KS_z,
    KC(0x65),				KS_x,
    KC(0x66),				KS_c,
    KC(0x67),				KS_v,
    KC(0x68),				KS_b,
    KC(0x69),				KS_n,
    KC(0x6a),				KS_m,
    KC(0x6b),				KS_comma,	KS_less,
    KC(0x6c),				KS_period,	KS_greater,
    KC(0x6d),				KS_slash,	KS_question,
    KC(0x6e),				KS_Shift_R,
    KC(0x6f),				KS_Linefeed,
    KC(0x70),				KS_KP_End,	KS_KP_1,
    KC(0x71),				KS_KP_Down,	KS_KP_2,
    KC(0x72),				KS_KP_Next,	KS_KP_3,
    KC(0x76),				KS_Help,
    KC(0x77),				KS_Caps_Lock,
    KC(0x78),				KS_Meta_L,
    KC(0x79),				KS_space,
    KC(0x7a),				KS_Meta_R,
    KC(0x7b),				KS_Next,
    KC(0x7d),				KS_KP_Add,
};

#define KBD_MAP(name, base, map) \
    { name, base, sizeof(map)/sizeof(keysym_t), map }

const struct wscons_keydesc zskbd_keydesctab[] = {
	KBD_MAP(KB_US, 0, zskbd_keydesc_us),
	{0, 0, 0, 0},
};

struct wskbd_mapdata zskbd_keymapdata = {
	zskbd_keydesctab
};

#define	ZSKBDUNIT(x)	(minor(x) & 0x7ffff)

struct zskbd_softc *
zskbd_device_lookup(cf, unit)
	struct cfdriver *cf;
	int unit;
{ 
	return (struct zskbd_softc *)device_lookup(cf, unit);
}

/*
 * zskbd_match: how is this zs channel configured?
 */
int 
zskbd_match(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void   *aux;
{
	struct cfdata *cf = vcf;
	struct zsc_attach_args *args = aux;
	int ret;

	/* If we're not looking for a keyboard, just exit */
	if (strcmp(args->type, "keyboard") != 0)
		return (0);

	ret = 10;

	/* Exact match is better than wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == args->channel)
		ret += 2;

	/* This driver accepts wildcard. */
	if (cf->cf_loc[ZSCCF_CHANNEL] == ZSCCF_CHANNEL_DEFAULT)
		ret += 1;

	return (ret);
}

void 
zskbd_attach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct zsc_softc *zsc = (void *) parent;
	struct zskbd_softc *zst = (void *) self;
	struct cfdata *cf = self->dv_cfdata;
	struct zsc_attach_args *args = aux;
	struct wskbddev_attach_args a;
	struct zs_chanstate *cs;
	struct tty *tp;
	int channel, s, tty_unit, console = 0;
	dev_t dev;

	timeout_set(&zst->zst_diag_ch, zskbd_diag, zst);

	tty_unit = zst->zst_dev.dv_unit;
	channel = args->channel;
	cs = zsc->zsc_cs[channel];
	cs->cs_private = zst;
	cs->cs_ops = &zsops_kbd;

	zst->zst_cs = cs;
	zst->zst_swflags = cf->cf_flags;	/* softcar, etc. */
	zst->zst_hwflags = args->hwflags;
	dev = makedev(zs_major, tty_unit);

	if (zst->zst_swflags)
		printf(" flags 0x%x", zst->zst_swflags);

	/*
	 * Check whether we serve as a console device.
	 * XXX - split console input/output channels aren't
	 *	 supported yet on /dev/console
	 */
	if ((zst->zst_hwflags & ZS_HWFLAG_CONSOLE_INPUT) != 0) {
		if ((args->hwflags & ZS_HWFLAG_USE_CONSDEV) != 0) {
			args->consdev->cn_dev = dev;
			cn_tab->cn_pollc = wskbd_cnpollc;
			cn_tab->cn_getc = wskbd_cngetc;
		}
		cn_tab->cn_dev = dev;
		console = 1;
	}

	printf("\n");

	tp = ttymalloc();
	tp->t_dev = dev;
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;
	tp->t_hwiflow = zshwiflow;
	tty_attach(tp);

	zst->zst_tty = tp;
	zst->zst_rbuf = malloc(zskbd_rbuf_size << 1, M_DEVBUF, M_WAITOK);
	zst->zst_ebuf = zst->zst_rbuf + (zskbd_rbuf_size << 1);
	/* Disable the high water mark. */
	zst->zst_r_hiwat = 0;
	zst->zst_r_lowat = 0;
	zst->zst_rbget = zst->zst_rbput = zst->zst_rbuf;
	zst->zst_rbavail = zskbd_rbuf_size;

	/* if there are no enable/disable functions, assume the device
	   is always enabled */
	if (!cs->enable)
		cs->enabled = 1;

	/*
	 * Hardware init
	 */
	if (ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE)) {
		/* Call zsparam similar to open. */
		struct termios t;

		/* Wait a while for previous console output to complete */
		DELAY(10000);

		/* Setup the "new" parameters in t. */
		t.c_ispeed = 0;
		t.c_ospeed = 1200;
		t.c_cflag = CS8 | CLOCAL;

		s = splzs();

		/*
		 * Turn on receiver and status interrupts.
		 * We defer the actual write of the register to zsparam(),
		 * but we must make sure status interrupts are turned on by
		 * the time zsparam() reads the initial rr0 state.
		 */
		SET(cs->cs_preg[1], ZSWR1_RIE | ZSWR1_SIE);

		splx(s);

		/* Make sure zsparam will see changes. */
		tp->t_ospeed = 0;
		(void) zsparam(tp, &t);

		s = splzs();

		/* Make sure DTR is on now. */
		zs_modem(zst, 1);

		splx(s);
	} else if (!ISSET(zst->zst_hwflags, ZS_HWFLAG_NORESET)) {
		/* Not the console; may need reset. */
		int reset;

		reset = (channel == 0) ? ZSWR9_A_RESET : ZSWR9_B_RESET;

		s = splzs();

		zs_write_reg(cs, 9, reset);

		/* Will raise DTR in open. */
		zs_modem(zst, 0);

		splx(s);
	}

	zskbd_init(zst);

	a.console = console;
	a.keymap = &zskbd_keymapdata;
	a.accessops = &zskbd_accessops;
	a.accesscookie = zst;

	if (console)
		wskbd_cnattach(&zskbd_consops, zst, &zskbd_keymapdata);

	zst->zst_wskbddev = config_found(self, &a, wskbddevprint);
}

/* keyboard commands (host->kbd) */
#define	SKBD_CMD_RESET		0x01
#define	SKBD_CMD_BELLON		0x02
#define	SKBD_CMD_BELLOFF	0x03
#define	SKBD_CMD_CLICKON	0x0a
#define	SKBD_CMD_CLICKOFF	0x0b
#define	SKBD_CMD_SETLED		0x0e
#define	SKBD_CMD_LAYOUT		0x0f

/* keyboard responses (kbd->host) */
#define	SKBD_RSP_RESET_OK	0x04	/* normal reset status */
#define	SKBD_RSP_IDLE		0x7f	/* no keys down */
#define	SKBD_RSP_LAYOUT		0xfe	/* layout follows */
#define	SKBD_RSP_RESET		0xff	/* reset status follows */

#define	SKBD_STATE_RESET	0
#define	SKBD_STATE_LAYOUT	1
#define	SKBD_STATE_GETKEY	2

void
zskbd_init(zst)
	struct zskbd_softc *zst;
{
	struct zs_chanstate *cs = zst->zst_cs;
	int s, tries;
	u_int8_t v3, v4, v5, rr0;

	/* setup for 1200n81 */
	if (zs_set_speed(cs, 1200)) {			/* set 1200bps */
		printf(": failed to set baudrate\n");
		return;
	}
	if (zs_set_modes(cs, CS8 | CLOCAL)) {
		printf(": failed to set modes\n");
		return;
	}

	s = splzs();

	zs_maskintr(zst);

	v3 = cs->cs_preg[3];				/* set 8 bit chars */
	v5 = cs->cs_preg[5];
	CLR(v3, ZSWR3_RXSIZE);
	CLR(v5, ZSWR5_TXSIZE);
	SET(v3, ZSWR3_RX_8);
	SET(v5, ZSWR5_TX_8);
	cs->cs_preg[3] = v3;
	cs->cs_preg[5] = v5;

	v4 = cs->cs_preg[4];				/* no parity 1 stop */
	CLR(v4, ZSWR4_SBMASK | ZSWR4_PARMASK);
	SET(v4, ZSWR4_ONESB | ZSWR4_EVENP);
	cs->cs_preg[4] = v4;

	if (!cs->cs_heldchange) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 1;
		} else
			zs_loadchannelregs(cs);
	}

	/*
	 * Hardware flow control is disabled, turn off the buffer water
	 * marks and unblock any soft flow control state.  Otherwise, enable
	 * the water marks.
	 */
	zst->zst_r_hiwat = 0;
	zst->zst_r_lowat = 0;
	if (ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
		CLR(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
		zst->zst_rx_ready = 1;
		cs->cs_softreq = 1;
	}
	if (ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
		CLR(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
		zs_hwiflow(zst);
	}

	/*
	 * Force a recheck of the hardware carrier and flow control status,
	 * since we may have changed which bits we're looking at.
	 */
	zskbd_stint(cs, 1);

	splx(s);

	/*
	 * Hardware flow control is disabled, unblock any hard flow control
	 * state.
	 */
	if (zst->zst_tx_stopped) {
		zst->zst_tx_stopped = 0;
		zsstart(zst->zst_tty);
	}

	zskbd_softint(cs);

	/* Ok, start the reset sequence... */

	s = splhigh();
	zst->zst_leds = 0;
	zst->zst_layout = -1;

	for (tries = 5; tries != 0; tries--) {
		int ltries;

		zskbd_putc(zst, SKBD_CMD_RESET);

		ltries = 1000;
		while (--ltries > 0) {
			rr0 = *cs->cs_reg_csr;
			if (rr0 & ZSRR0_RX_READY)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		zskbd_raw(zst, *cs->cs_reg_data);
		if (zst->zst_kbdstate != SKBD_STATE_RESET)
			continue;

		ltries = 1000;
		while (--ltries > 0) {
			rr0 = *cs->cs_reg_csr;
			if (rr0 & ZSRR0_RX_READY)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		zskbd_raw(zst, *cs->cs_reg_data);
		if (zst->zst_kbdstate != SKBD_STATE_GETKEY)
			continue;

		zskbd_putc(zst, SKBD_CMD_LAYOUT);

		ltries = 1000;
		while (--ltries > 0) {
			rr0 = *cs->cs_reg_csr;
			if (rr0 & ZSRR0_RX_READY)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		zskbd_raw(zst, *cs->cs_reg_data);
		if (zst->zst_kbdstate != SKBD_STATE_LAYOUT)
			continue;
		ltries = 1000;
		while (--ltries > 0) {
			rr0 = *cs->cs_reg_csr;
			if (rr0 & ZSRR0_RX_READY)
				break;
			DELAY(1000);
		}
		if (ltries == 0)
			continue;
		zskbd_raw(zst, *cs->cs_reg_data);
		if (zst->zst_kbdstate == SKBD_STATE_GETKEY)
			break;
	}
	if (tries == 0)
		printf(":reset timeout\n");
	else
		printf("reset ok, layout %d\n", zst->zst_layout);
	splx(s);
}

void
zskbd_raw(zst, c)
	struct zskbd_softc *zst;
	u_int8_t c;
{
	int claimed = 0;

	printf("raw(state %d, code %x)\n", zst->zst_kbdstate, c);
	switch (c) {
	case SKBD_RSP_RESET:
		zst->zst_kbdstate = SKBD_STATE_RESET;
		claimed = 1;
		break;
	case SKBD_RSP_LAYOUT:
		zst->zst_kbdstate = SKBD_STATE_LAYOUT;
		claimed = 1;
		break;
	case SKBD_RSP_IDLE:
		zst->zst_kbdstate = SKBD_STATE_GETKEY;
		claimed = 1;
	}

	if (claimed) {
		printf("out state: %d\n", zst->zst_kbdstate);
		return;
	}

	switch (zst->zst_kbdstate) {
	case SKBD_STATE_RESET:
		zst->zst_kbdstate = SKBD_STATE_GETKEY;
		if (c != SKBD_RSP_RESET_OK)
			printf("%s: reset1 invalid code 0x%02x\n",
			    zst->zst_dev.dv_xname, c);
		break;
	case SKBD_STATE_LAYOUT:
		zst->zst_kbdstate = SKBD_STATE_GETKEY;
		printf("layout: %02x\n", c);
		zst->zst_layout = c;
		break;
	case SKBD_STATE_GETKEY:
		printf("KEY(%02x)\n", c);
		break;
	}
	printf("out state: %d\n", zst->zst_kbdstate);
}

void
zskbd_putc(zst, c)
	struct zskbd_softc *zst;
	u_int8_t c;
{
	u_int8_t rr0;
	int s;

	s = splhigh();
	do {
		rr0 = *zst->zst_cs->cs_reg_csr;
	} while ((rr0 & ZSRR0_TX_READY) == 0);
	*zst->zst_cs->cs_reg_data = c;
	delay(2);
	splx(s);
}

/*
 * Start or restart transmission.
 */
static void
zsstart(tp)
	struct tty *tp;
{
	struct zskbd_softc *zst = zskbd_device_lookup(&zskbd_cd, ZSKBDUNIT(tp->t_dev));
	struct zs_chanstate *cs = zst->zst_cs;
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (zst->zst_tx_stopped)
		goto out;

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}

	/* Grab the first contiguous region of buffer space. */
	{
		u_char *tba;
		int tbc;

		tba = tp->t_outq.c_cf;
		tbc = ndqb(&tp->t_outq, 0);
	
		(void) splzs();

		zst->zst_tba = tba;
		zst->zst_tbc = tbc;
	}

	SET(tp->t_state, TS_BUSY);
	zst->zst_tx_busy = 1;

	/* Enable transmit completion interrupts if necessary. */
	if (!ISSET(cs->cs_preg[1], ZSWR1_TIE)) {
		SET(cs->cs_preg[1], ZSWR1_TIE);
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
	}

	/* Output the first character of the contiguous buffer. */
	{
		zs_write_data(cs, *zst->zst_tba);
		zst->zst_tbc--;
		zst->zst_tba++;
	}
out:
	splx(s);
	return;
}

/*
 * Set ZS tty parameters from termios.
 * XXX - Should just copy the whole termios after
 * making sure all the changes could be done.
 */
static int
zsparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct zskbd_softc *zst = zskbd_device_lookup(&zskbd_cd, ZSKBDUNIT(tp->t_dev));
	struct zs_chanstate *cs = zst->zst_cs;
	int ospeed, cflag;
	u_char tmp3, tmp4, tmp5;
	int s, error;

	ospeed = t->c_ospeed;
	cflag = t->c_cflag;

	/* Check requested parameters. */
	if (ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != ospeed)
		return (EINVAL);

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(zst->zst_swflags, TIOCFLAG_SOFTCAR) ||
	    ISSET(zst->zst_hwflags, ZS_HWFLAG_CONSOLE)) {
		SET(cflag, CLOCAL);
		CLR(cflag, HUPCL);
	}

	/*
	 * Only whack the UART when params change.
	 * Some callers need to clear tp->t_ospeed
	 * to make sure initialization gets done.
	 */
	if (tp->t_ospeed == ospeed &&
	    tp->t_cflag == cflag)
		return (0);

	/*
	 * Call MD functions to deal with changed
	 * clock modes or H/W flow control modes.
	 * The BRG divisor is set now. (reg 12,13)
	 */
	error = zs_set_speed(cs, ospeed);
	if (error)
		return (error);
	error = zs_set_modes(cs, cflag);
	if (error)
		return (error);

	/*
	 * Block interrupts so that state will not
	 * be altered until we are done setting it up.
	 *
	 * Initial values in cs_preg are set before
	 * our attach routine is called.  The master
	 * interrupt enable is handled by zsc.c
	 *
	 */
	s = splzs();

	/*
	 * Recalculate which status ints to enable.
	 */
	zs_maskintr(zst);

	/* Recompute character size bits. */
	tmp3 = cs->cs_preg[3];
	tmp5 = cs->cs_preg[5];
	CLR(tmp3, ZSWR3_RXSIZE);
	CLR(tmp5, ZSWR5_TXSIZE);
	switch (ISSET(cflag, CSIZE)) {
	case CS5:
		SET(tmp3, ZSWR3_RX_5);
		SET(tmp5, ZSWR5_TX_5);
		break;
	case CS6:
		SET(tmp3, ZSWR3_RX_6);
		SET(tmp5, ZSWR5_TX_6);
		break;
	case CS7:
		SET(tmp3, ZSWR3_RX_7);
		SET(tmp5, ZSWR5_TX_7);
		break;
	case CS8:
		SET(tmp3, ZSWR3_RX_8);
		SET(tmp5, ZSWR5_TX_8);
		break;
	}
	cs->cs_preg[3] = tmp3;
	cs->cs_preg[5] = tmp5;

	/*
	 * Recompute the stop bits and parity bits.  Note that
	 * zs_set_speed() may have set clock selection bits etc.
	 * in wr4, so those must preserved.
	 */
	tmp4 = cs->cs_preg[4];
	CLR(tmp4, ZSWR4_SBMASK | ZSWR4_PARMASK);
	if (ISSET(cflag, CSTOPB))
		SET(tmp4, ZSWR4_TWOSB);
	else
		SET(tmp4, ZSWR4_ONESB);
	if (!ISSET(cflag, PARODD))
		SET(tmp4, ZSWR4_EVENP);
	if (ISSET(cflag, PARENB))
		SET(tmp4, ZSWR4_PARENB);
	cs->cs_preg[4] = tmp4;

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = ospeed;
	tp->t_cflag = cflag;

	/*
	 * If nothing is being transmitted, set up new current values,
	 * else mark them as pending.
	 */
	if (!cs->cs_heldchange) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 1;
		} else
			zs_loadchannelregs(cs);
	}

	/*
	 * If hardware flow control is disabled, turn off the buffer water
	 * marks and unblock any soft flow control state.  Otherwise, enable
	 * the water marks.
	 */
	if (!ISSET(cflag, CHWFLOW)) {
		zst->zst_r_hiwat = 0;
		zst->zst_r_lowat = 0;
		if (ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			zst->zst_rx_ready = 1;
			cs->cs_softreq = 1;
		}
		if (ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED)) {
			CLR(zst->zst_rx_flags, RX_TTY_BLOCKED|RX_IBUF_BLOCKED);
			zs_hwiflow(zst);
		}
	} else {
		zst->zst_r_hiwat = zskbd_rbuf_hiwat;
		zst->zst_r_lowat = zskbd_rbuf_lowat;
	}

	/*
	 * Force a recheck of the hardware carrier and flow control status,
	 * since we may have changed which bits we're looking at.
	 */
	zskbd_stint(cs, 1);

	splx(s);

	/*
	 * If hardware flow control is disabled, unblock any hard flow control
	 * state.
	 */
	if (!ISSET(cflag, CHWFLOW)) {
		if (zst->zst_tx_stopped) {
			zst->zst_tx_stopped = 0;
			zsstart(tp);
		}
	}

	zskbd_softint(cs);

	return (0);
}

/*
 * Compute interupt enable bits and set in the pending bits. Called both
 * in zsparam() and when PPS (pulse per second timing) state changes.
 * Must be called at splzs().
 */
static void
zs_maskintr(zst)
	struct zskbd_softc *zst;
{
	struct zs_chanstate *cs = zst->zst_cs;
	int tmp15;

	cs->cs_rr0_mask = cs->cs_rr0_cts | cs->cs_rr0_dcd;
	if (zst->zst_ppsmask != 0)
		cs->cs_rr0_mask |= cs->cs_rr0_pps;
	tmp15 = cs->cs_preg[15];
	if (ISSET(cs->cs_rr0_mask, ZSRR0_DCD))
		SET(tmp15, ZSWR15_DCD_IE);
	else
		CLR(tmp15, ZSWR15_DCD_IE);
	if (ISSET(cs->cs_rr0_mask, ZSRR0_CTS))
		SET(tmp15, ZSWR15_CTS_IE);
	else
		CLR(tmp15, ZSWR15_CTS_IE);
	cs->cs_preg[15] = tmp15;
}


/*
 * Raise or lower modem control (DTR/RTS) signals.  If a character is
 * in transmission, the change is deferred.
 */
static void
zs_modem(zst, onoff)
	struct zskbd_softc *zst;
	int onoff;
{
	struct zs_chanstate *cs = zst->zst_cs;

	if (cs->cs_wr5_dtr == 0)
		return;

	if (onoff)
		SET(cs->cs_preg[5], cs->cs_wr5_dtr);
	else
		CLR(cs->cs_preg[5], cs->cs_wr5_dtr);

	if (!cs->cs_heldchange) {
		if (zst->zst_tx_busy) {
			zst->zst_heldtbc = zst->zst_tbc;
			zst->zst_tbc = 0;
			cs->cs_heldchange = 1;
		} else
			zs_loadchannelregs(cs);
	}
}

/*
 * Try to block or unblock input using hardware flow-control.
 * This is called by kern/tty.c if MDMBUF|CRTSCTS is set, and
 * if this function returns non-zero, the TS_TBLOCK flag will
 * be set or cleared according to the "block" arg passed.
 */
int
zshwiflow(tp, block)
	struct tty *tp;
	int block;
{
	struct zskbd_softc *zst = zskbd_device_lookup(&zskbd_cd, ZSKBDUNIT(tp->t_dev));
	struct zs_chanstate *cs = zst->zst_cs;
	int s;

	if (cs->cs_wr5_rts == 0)
		return (0);

	s = splzs();
	if (block) {
		if (!ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
			SET(zst->zst_rx_flags, RX_TTY_BLOCKED);
			zs_hwiflow(zst);
		}
	} else {
		if (ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
			CLR(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			zst->zst_rx_ready = 1;
			cs->cs_softreq = 1;
		}
		if (ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
			CLR(zst->zst_rx_flags, RX_TTY_BLOCKED);
			zs_hwiflow(zst);
		}
	}
	splx(s);
	return (1);
}

/*
 * Internal version of zshwiflow
 * called at splzs
 */
static void
zs_hwiflow(zst)
	struct zskbd_softc *zst;
{
	struct zs_chanstate *cs = zst->zst_cs;

	if (cs->cs_wr5_rts == 0)
		return;

	if (ISSET(zst->zst_rx_flags, RX_ANY_BLOCK)) {
		CLR(cs->cs_preg[5], cs->cs_wr5_rts);
		CLR(cs->cs_creg[5], cs->cs_wr5_rts);
	} else {
		SET(cs->cs_preg[5], cs->cs_wr5_rts);
		SET(cs->cs_creg[5], cs->cs_wr5_rts);
	}
	zs_write_reg(cs, 5, cs->cs_creg[5]);
}


/****************************************************************
 * Interface to the lower layer (zscc)
 ****************************************************************/

#define	integrate
integrate void zskbd_rxsoft __P((struct zskbd_softc *, struct tty *));
integrate void zskbd_txsoft __P((struct zskbd_softc *, struct tty *));
integrate void zskbd_stsoft __P((struct zskbd_softc *, struct tty *));
/*
 * receiver ready interrupt.
 * called at splzs
 */
static void
zskbd_rxint(cs)
	struct zs_chanstate *cs;
{
	struct zskbd_softc *zst = cs->cs_private;
	u_char *put, *end;
	u_int cc;
	u_char rr0, rr1, c;

	end = zst->zst_ebuf;
	put = zst->zst_rbput;
	cc = zst->zst_rbavail;

	while (cc > 0) {
		/*
		 * First read the status, because reading the received char
		 * destroys the status of this char.
		 */
		rr1 = zs_read_reg(cs, 1);
		c = zs_read_data(cs);

		if (ISSET(rr1, ZSRR1_FE | ZSRR1_DO | ZSRR1_PE)) {
			/* Clear the receive error. */
			zs_write_csr(cs, ZSWR0_RESET_ERRORS);
		}

		put[0] = c;
		put[1] = rr1;
		put += 2;
		if (put >= end)
			put = zst->zst_rbuf;
		cc--;

		rr0 = zs_read_csr(cs);
		if (!ISSET(rr0, ZSRR0_RX_READY))
			break;
	}

	/*
	 * Current string of incoming characters ended because
	 * no more data was available or we ran out of space.
	 * Schedule a receive event if any data was received.
	 * If we're out of space, turn off receive interrupts.
	 */
	zst->zst_rbput = put;
	zst->zst_rbavail = cc;
	if (!ISSET(zst->zst_rx_flags, RX_TTY_OVERFLOWED)) {
		zst->zst_rx_ready = 1;
		cs->cs_softreq = 1;
	}

	/*
	 * See if we are in danger of overflowing a buffer. If
	 * so, use hardware flow control to ease the pressure.
	 */
	if (!ISSET(zst->zst_rx_flags, RX_IBUF_BLOCKED) &&
	    cc < zst->zst_r_hiwat) {
		SET(zst->zst_rx_flags, RX_IBUF_BLOCKED);
		zs_hwiflow(zst);
	}

	/*
	 * If we're out of space, disable receive interrupts
	 * until the queue has drained a bit.
	 */
	if (!cc) {
		SET(zst->zst_rx_flags, RX_IBUF_OVERFLOWED);
		CLR(cs->cs_preg[1], ZSWR1_RIE);
		cs->cs_creg[1] = cs->cs_preg[1];
		zs_write_reg(cs, 1, cs->cs_creg[1]);
	}
}

/*
 * transmitter ready interrupt.  (splzs)
 */
static void
zskbd_txint(cs)
	struct zs_chanstate *cs;
{
	struct zskbd_softc *zst = cs->cs_private;

	/*
	 * If we've delayed a parameter change, do it now, and restart
	 * output.
	 */
	if (cs->cs_heldchange) {
		zs_loadchannelregs(cs);
		cs->cs_heldchange = 0;
		zst->zst_tbc = zst->zst_heldtbc;
		zst->zst_heldtbc = 0;
	}

	/* Output the next character in the buffer, if any. */
	if (zst->zst_tbc > 0) {
		zs_write_data(cs, *zst->zst_tba);
		zst->zst_tbc--;
		zst->zst_tba++;
	} else {
		/* Disable transmit completion interrupts if necessary. */
		if (ISSET(cs->cs_preg[1], ZSWR1_TIE)) {
			CLR(cs->cs_preg[1], ZSWR1_TIE);
			cs->cs_creg[1] = cs->cs_preg[1];
			zs_write_reg(cs, 1, cs->cs_creg[1]);
		}
		if (zst->zst_tx_busy) {
			zst->zst_tx_busy = 0;
			zst->zst_tx_done = 1;
			cs->cs_softreq = 1;
		}
	}
}

/*
 * status change interrupt.  (splzs)
 */
static void
zskbd_stint(cs, force)
	struct zs_chanstate *cs;
	int force;
{
	struct zskbd_softc *zst = cs->cs_private;
	u_char rr0, delta;

	rr0 = zs_read_csr(cs);
	zs_write_csr(cs, ZSWR0_RESET_STATUS);

	/*
	 * Check here for console break, so that we can abort
	 * even when interrupts are locking up the machine.
	 */
	if (!force)
		delta = rr0 ^ cs->cs_rr0;
	else
		delta = cs->cs_rr0_mask;
	cs->cs_rr0 = rr0;

	if (ISSET(delta, cs->cs_rr0_mask)) {
		SET(cs->cs_rr0_delta, delta);

		/*
		 * Stop output immediately if we lose the output
		 * flow control signal or carrier detect.
		 */
		if (ISSET(~rr0, cs->cs_rr0_mask)) {
			zst->zst_tbc = 0;
			zst->zst_heldtbc = 0;
		}

		zst->zst_st_check = 1;
		cs->cs_softreq = 1;
	}
}

void
zskbd_diag(arg)
	void *arg;
{
	struct zskbd_softc *zst = arg;
	int overflows, floods;
	int s;

	s = splzs();
	overflows = zst->zst_overflows;
	zst->zst_overflows = 0;
	floods = zst->zst_floods;
	zst->zst_floods = 0;
	zst->zst_errors = 0;
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf flood%s\n",
	    zst->zst_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

integrate void
zskbd_rxsoft(zst, tp)
	struct zskbd_softc *zst;
	struct tty *tp;
{
	struct zs_chanstate *cs = zst->zst_cs;
	int (*rint) __P((int c, struct tty *tp)) = linesw[tp->t_line].l_rint;
	u_char *get, *end;
	u_int cc, scc;
	u_char rr1;
	int code;
	int s;

	end = zst->zst_ebuf;
	get = zst->zst_rbget;
	scc = cc = zskbd_rbuf_size - zst->zst_rbavail;

	if (cc == zskbd_rbuf_size) {
		zst->zst_floods++;
		if (zst->zst_errors++ == 0)
			timeout_add(&zst->zst_diag_ch, 60 * hz);
	}

	/* If not yet open, drop the entire buffer content here */
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		get += cc << 1;
		if (get >= end)
			get -= zskbd_rbuf_size << 1;
		cc = 0;
	}
	while (cc) {
		code = get[0];
		rr1 = get[1];
		if (ISSET(rr1, ZSRR1_DO | ZSRR1_FE | ZSRR1_PE)) {
			if (ISSET(rr1, ZSRR1_DO)) {
				zst->zst_overflows++;
				if (zst->zst_errors++ == 0)
					timeout_add(&zst->zst_diag_ch, 60 * hz);
			}
			if (ISSET(rr1, ZSRR1_FE))
				SET(code, TTY_FE);
			if (ISSET(rr1, ZSRR1_PE))
				SET(code, TTY_PE);
		}
		if ((*rint)(code, tp) == -1) {
			/*
			 * The line discipline's buffer is out of space.
			 */
			if (!ISSET(zst->zst_rx_flags, RX_TTY_BLOCKED)) {
				/*
				 * We're either not using flow control, or the
				 * line discipline didn't tell us to block for
				 * some reason.  Either way, we have no way to
				 * know when there's more space available, so
				 * just drop the rest of the data.
				 */
				get += cc << 1;
				if (get >= end)
					get -= zskbd_rbuf_size << 1;
				cc = 0;
			} else {
				/*
				 * Don't schedule any more receive processing
				 * until the line discipline tells us there's
				 * space available (through comhwiflow()).
				 * Leave the rest of the data in the input
				 * buffer.
				 */
				SET(zst->zst_rx_flags, RX_TTY_OVERFLOWED);
			}
			break;
		}
		get += 2;
		if (get >= end)
			get = zst->zst_rbuf;
		cc--;
	}

	if (cc != scc) {
		zst->zst_rbget = get;
		s = splzs();
		cc = zst->zst_rbavail += scc - cc;
		/* Buffers should be ok again, release possible block. */
		if (cc >= zst->zst_r_lowat) {
			if (ISSET(zst->zst_rx_flags, RX_IBUF_OVERFLOWED)) {
				CLR(zst->zst_rx_flags, RX_IBUF_OVERFLOWED);
				SET(cs->cs_preg[1], ZSWR1_RIE);
				cs->cs_creg[1] = cs->cs_preg[1];
				zs_write_reg(cs, 1, cs->cs_creg[1]);
			}
			if (ISSET(zst->zst_rx_flags, RX_IBUF_BLOCKED)) {
				CLR(zst->zst_rx_flags, RX_IBUF_BLOCKED);
				zs_hwiflow(zst);
			}
		}
		splx(s);
	}
}

integrate void
zskbd_txsoft(zst, tp)
	struct zskbd_softc *zst;
	struct tty *tp;
{

	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, (int)(zst->zst_tba - tp->t_outq.c_cf));
	(*linesw[tp->t_line].l_start)(tp);
}

integrate void
zskbd_stsoft(zst, tp)
	struct zskbd_softc *zst;
	struct tty *tp;
{
	struct zs_chanstate *cs = zst->zst_cs;
	u_char rr0, delta;
	int s;

	s = splzs();
	rr0 = cs->cs_rr0;
	delta = cs->cs_rr0_delta;
	cs->cs_rr0_delta = 0;
	splx(s);

	if (ISSET(delta, cs->cs_rr0_dcd)) {
		/*
		 * Inform the tty layer that carrier detect changed.
		 */
		(void) (*linesw[tp->t_line].l_modem)(tp, ISSET(rr0, ZSRR0_DCD));
	}

	if (ISSET(delta, cs->cs_rr0_cts)) {
		/* Block or unblock output according to flow control. */
		if (ISSET(rr0, cs->cs_rr0_cts)) {
			zst->zst_tx_stopped = 0;
			(*linesw[tp->t_line].l_start)(tp);
		} else {
			zst->zst_tx_stopped = 1;
		}
	}
}

/*
 * Software interrupt.  Called at zssoft
 *
 * The main job to be done here is to empty the input ring
 * by passing its contents up to the tty layer.  The ring is
 * always emptied during this operation, therefore the ring
 * must not be larger than the space after "high water" in
 * the tty layer, or the tty layer might drop our input.
 *
 * Note: an "input blockage" condition is assumed to exist if
 * EITHER the TS_TBLOCK flag or zst_rx_blocked flag is set.
 */
static void
zskbd_softint(cs)
	struct zs_chanstate *cs;
{
	struct zskbd_softc *zst = cs->cs_private;
	struct tty *tp = zst->zst_tty;
	int s;

	s = spltty();

	if (zst->zst_rx_ready) {
		zst->zst_rx_ready = 0;
		zskbd_rxsoft(zst, tp);
	}

	if (zst->zst_st_check) {
		zst->zst_st_check = 0;
		zskbd_stsoft(zst, tp);
	}

	if (zst->zst_tx_done) {
		zst->zst_tx_done = 0;
		zskbd_txsoft(zst, tp);
	}

	splx(s);
}

struct zsops zsops_kbd = {
	zskbd_rxint,	/* receive char available */
	zskbd_stint,	/* external/status */
	zskbd_txint,	/* xmit buffer empty */
	zskbd_softint,	/* process software interrupt */
};

int
zskbd_enable(v, on)
	void *v;
	int on;
{
	struct zskbd_softc *zst = v;

	printf("zskbd_enable: %s\n", zst->zst_dev.dv_xname);
	return (0);
}

void
zskbd_set_leds(v, on)
	void *v;
	int on;
{
	struct zskbd_softc *zst = v;

	printf("zskbd_set_leds: %s\n", zst->zst_dev.dv_xname);
	zst->zst_leds = on;
}

int
zskbd_get_leds(v)
	void *v;
{
	struct zskbd_softc *zst = v;

	printf("zskbd_get_leds: %s\n", zst->zst_dev.dv_xname);
	return (zst->zst_leds);
}

int
zskbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct zskbd_softc *zst = v;

	printf("zskbd_ioctl: %s\n", zst->zst_dev.dv_xname);

	switch (cmd) {
	case WSKBDIO_GTYPE:
#if 0		/* XXX need to allocate a type... */
		*(int *)data = WSKBD_TYPE_XXX;
#endif
		return (0);
	case WSKBDIO_SETLEDS:
		zskbd_set_leds(v, *(int *)data);
		return (0);
	case WSKBDIO_GETLEDS:
		*(int *)data = zskbd_get_leds(v);
		return (0);
	}
	return (-1);
}

void
zskbd_cnpollc(v, on)
	void *v;
	int on;
{
	struct zskbd_softc *zst = v;
	extern int swallow_zsintrs;

	printf("%s: cnpollc...", zst->zst_dev.dv_xname);

	if (on)
		swallow_zsintrs++;
	else
		swallow_zsintrs--;
}

void
zskbd_cngetc(v, type, data)
	void *v;
	u_int *type;
	int *data;
{
	struct zskbd_softc *zst = v;
	int s;
	u_int8_t c, rr0;

	printf("%s: cngetc...", zst->zst_dev.dv_xname);

	s = splhigh();
	do {
		rr0 = *zst->zst_cs->cs_reg_csr;
	} while ((rr0 & ZSRR0_RX_READY) == 0);

	c = *zst->zst_cs->cs_reg_data;
	splx(s);

	printf("%02x\n", c);

	*type = (c & 0x80) ? WSCONS_EVENT_KEY_UP : WSCONS_EVENT_KEY_DOWN;
	*data = c & 0x7f;
}
