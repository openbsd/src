/*	$OpenBSD: kbd.c,v 1.8 1998/04/25 00:31:23 deraadt Exp $	*/
/*	$NetBSD: kbd.c,v 1.28 1997/09/13 19:12:18 pk Exp $ */

/*
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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Keyboard driver (/dev/kbd -- note that we do not have minor numbers
 * [yet?]).  Translates incoming bytes to ASCII or to `firm_events' and
 * passes them up to the appropriate reader.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <machine/conf.h>

#include <machine/vuid_event.h>
#include <dev/sun/event_var.h>
#include <machine/kbd.h>
#include <machine/kbio.h>

/*
 * Sun keyboard definitions (from Sprite).
 * These apply to type 2, 3 and 4 keyboards.
 */
#define	KEY_CODE(c)	((c) & KBD_KEYMASK)	/* keyboard code index */
#define	KEY_UP(c)	((c) & KBD_UP)		/* true => key went up */

/*
 * Each KEY_CODE(x) can be translated via the tables below.
 * The result is either a valid ASCII value in [0..0x7f] or is one
 * of the following `magic' values saying something interesting
 * happened.  If LSHIFT or RSHIFT has changed state the next
 * lookup should come from the appropriate table; if ALLUP is
 * sent all keys (including both shifts and the control key) are
 * now up, and the next byte is the keyboard ID code.
 *
 * These tables ignore all function keys (on the theory that if you
 * want these keys, you should use a window system).  Note that
 * `caps lock' is just mapped as `ignore' (so there!). (Only the
 * type 3 and 4 keyboards have a caps lock key anyway.)
 */
#define	KEY_MAGIC	0x80		/* flag => magic value */
#define	KEY_IGNORE	0x80
#define	KEY_L1		KEY_IGNORE
#define	KEY_CAPSLOCK	KEY_IGNORE
#define	KEY_LSHIFT	0x81
#define	KEY_RSHIFT	0x82
#define	KEY_CONTROL	0x83
#define	KEY_ALLUP	0x84		/* all keys are now up; also reset */

/*
 * Decode tables for type 2, 3, and 4 keyboards
 * (stolen from Sprite; see also kbd.h).
 */
static u_char kbd_unshifted[] = {
/*   0 */	KEY_IGNORE,	KEY_L1,		KEY_IGNORE,	KEY_IGNORE,
/*   4 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*   8 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  12 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  16 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  20 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  24 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  28 */	KEY_IGNORE,	'\033',		'1',		'2',
/*  32 */	'3',		'4',		'5',		'6',
/*  36 */	'7',		'8',		'9',		'0',
/*  40 */	'-',		'=',		'`',		'\b',
/*  44 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  48 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  52 */	KEY_IGNORE,	'\t',		'q',		'w',
/*  56 */	'e',		'r',		't',		'y',
/*  60 */	'u',		'i',		'o',		'p',
/*  64 */	'[',		']',		'\177',		KEY_IGNORE,
/*  68 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  72 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  76 */	KEY_CONTROL,	'a',		's',		'd',
/*  80 */	'f',		'g',		'h',		'j',
/*  84 */	'k',		'l',		';',		'\'',
/*  88 */	'\\',		'\r',		KEY_IGNORE,	KEY_IGNORE,
/*  92 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  96 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_LSHIFT,
/* 100 */	'z',		'x',		'c',		'v',
/* 104 */	'b',		'n',		'm',		',',
/* 108 */	'.',		'/',		KEY_RSHIFT,	'\n',
/* 112 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/* 116 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_CAPSLOCK,
/* 120 */	KEY_IGNORE,	' ',		KEY_IGNORE,	KEY_IGNORE,
/* 124 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_ALLUP,
};

static u_char kbd_shifted[] = {
/*   0 */	KEY_IGNORE,	KEY_L1,		KEY_IGNORE,	KEY_IGNORE,
/*   4 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*   8 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  12 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  16 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  20 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  24 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  28 */	KEY_IGNORE,	'\033',		'!',		'@',
/*  32 */	'#',		'$',		'%',		'^',
/*  36 */	'&',		'*',		'(',		')',
/*  40 */	'_',		'+',		'~',		'\b',
/*  44 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  48 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  52 */	KEY_IGNORE,	'\t',		'Q',		'W',
/*  56 */	'E',		'R',		'T',		'Y',
/*  60 */	'U',		'I',		'O',		'P',
/*  64 */	'{',		'}',		'\177',		KEY_IGNORE,
/*  68 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  72 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  76 */	KEY_CONTROL,	'A',		'S',		'D',
/*  80 */	'F',		'G',		'H',		'J',
/*  84 */	'K',		'L',		':',		'"',
/*  88 */	'|',		'\r',		KEY_IGNORE,	KEY_IGNORE,
/*  92 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/*  96 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_LSHIFT,
/* 100 */	'Z',		'X',		'C',		'V',
/* 104 */	'B',		'N',		'M',		'<',
/* 108 */	'>',		'?',		KEY_RSHIFT,	'\n',
/* 112 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,
/* 116 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_CAPSLOCK,
/* 120 */	KEY_IGNORE,	' ',		KEY_IGNORE,	KEY_IGNORE,
/* 124 */	KEY_IGNORE,	KEY_IGNORE,	KEY_IGNORE,	KEY_ALLUP,
};

/*
 * We need to remember the state of the keyboard's shift and control
 * keys, and we need a per-type translation table.
 */
struct kbd_state {
	const u_char *kbd_unshifted;	/* unshifted keys */
	const u_char *kbd_shifted;	/* shifted keys */
	const u_char *kbd_cur;	/* current keys (either of the preceding) */
	union {
		char	c[2];	/* left and right shift keys */
		short	s;	/* true => either shift key */
	} kbd_shift;
#define	kbd_lshift	kbd_shift.c[0]
#define	kbd_rshift	kbd_shift.c[1]
#define	kbd_anyshift	kbd_shift.s
	char	kbd_control;	/* true => ctrl down */
	char	kbd_click;	/* true => keyclick enabled */
	u_char	kbd_pending;	/* Another code from the keyboard is due */
	u_char	kbd_id;		/* a place to store the ID */
	u_char	kbd_layout;	/* a place to store layout */
	char	kbd_leds;	/* LED state */
};

/*
 * Keyboard driver state.  The ascii and kbd links go up and down and
 * we just sit in the middle doing translation.  Note that it is possible
 * to get just one of the two links, in which case /dev/kbd is unavailable.
 * The downlink supplies us with `internal' open and close routines which
 * will enable dataflow across the downlink.  We promise to call open when
 * we are willing to take keystrokes, and to call close when we are not.
 * If /dev/kbd is not the console tty input source, we do this whenever
 * /dev/kbd is in use; otherwise we just leave it open forever.
 */
struct kbd_softc {
	struct	tty *k_cons;		/* uplink for ASCII data to console */
	struct	tty *k_kbd;		/* downlink for output to keyboard */
	void	(*k_open) __P((struct tty *));	/* enable dataflow */
	void	(*k_close) __P((struct tty *));	/* disable dataflow */
	int	k_evmode;		/* set if we should produce events */
	struct	kbd_state k_state;	/* ASCII decode state */
	struct	evvar k_events;		/* event queue state */
	int	k_repeatc;		/* repeated character */
	int	k_repeating;		/* we've called timeout() */
} kbd_softc;

/* Prototypes */
void	kbd_reset __P((struct kbd_state *));
static	int kbd_translate __P((int, struct kbd_state *));
void	kbdattach __P((int));
void	kbd_repeat __P((void *arg));

/* set in kbdattach() */
int kbd_repeat_start;
int kbd_repeat_step;

/*
 * Attach the console keyboard ASCII (up-link) interface.
 * This happens before kbd_serial.
 */
void
kbd_ascii(tp)
	struct tty *tp;
{

	kbd_softc.k_cons = tp;
}

/*
 * Attach the console keyboard serial (down-link) interface.
 * We pick up the initial keyboard click state here as well.
 */
void
kbd_serial(tp, iopen, iclose)
	struct tty *tp;
	void (*iopen) __P((struct tty *));
	void (*iclose) __P((struct tty *));
{
	register struct kbd_softc *k;
	register char *cp;

	k = &kbd_softc;
	k->k_kbd = tp;
	k->k_open = iopen;
	k->k_close = iclose;

	if (!CPU_ISSUN4) {
		cp = getpropstring(optionsnode, "keyboard-click?");
		if (cp && strcmp(cp, "true") == 0)
			k->k_state.kbd_click = 1;
	}
}

/*
 * Called from main() during pseudo-device setup.  If this keyboard is
 * the console, this is our chance to open the underlying serial port and
 * send a RESET, so that we can find out what kind of keyboard it is.
 */
void
kbdattach(kbd)
	int kbd;
{
	register struct kbd_softc *k;
	register struct tty *tp;

	kbd_repeat_start = hz/5;
	kbd_repeat_step = hz/20;

	if (kbd_softc.k_cons != NULL) {
		k = &kbd_softc;
		tp = k->k_kbd;
		(*k->k_open)(tp);	/* never to be closed */
		if (ttyoutput(KBD_CMD_RESET, tp) >= 0)
			panic("kbdattach");
		(*tp->t_oproc)(tp);	/* get it going */

		/*
		 * Wait here for the keyboard initialization to complete
		 * since subsequent kernel console access (ie. cnget())
		 * may cause the PROM to interfere with the device.
		 */
		if (tsleep((caddr_t)&kbd_softc.k_state,
			   PZERO | PCATCH, devopn, hz) != 0) {
			/* no response */
			printf("kbd: reset failed\n");
			kbd_reset(&kbd_softc.k_state);
		}
		printf("kbd: type = %d, layout = %d\n",
		    kbd_softc.k_state.kbd_id, kbd_softc.k_state.kbd_layout);
	}
}

void
kbd_reset(ks)
	register struct kbd_state *ks;
{
	/*
	 * On first identification, set up the table pointers.
	 */
	if (ks->kbd_unshifted == NULL) {
		ks->kbd_unshifted = kbd_unshifted;
		ks->kbd_shifted = kbd_shifted;
		ks->kbd_cur = ks->kbd_unshifted;
	}

	/* Restore keyclick, if necessary */
	switch (ks->kbd_id) {

	case KB_SUN2:
		/* Type 2 keyboards don't support keyclick */
		break;

	case KB_SUN3:
		/* Type 3 keyboards come up with keyclick on */
		if (!ks->kbd_click)
			(void) kbd_docmd(KBD_CMD_NOCLICK, 0);
		break;

	case KB_SUN4:
		/* Type 4 keyboards come up with keyclick off */
		if (ks->kbd_click)
			(void) kbd_docmd(KBD_CMD_CLICK, 0);
		break;
	default:
		printf("Unknown keyboard type %d\n", ks->kbd_id);
	}

	ks->kbd_leds = 0;
}

/*
 * Turn keyboard up/down codes into ASCII.
 */
static int
kbd_translate(c, ks)
	register int c;
	register struct kbd_state *ks;
{
	register int down;

	if (ks->kbd_cur == NULL) {
		/*
		 * Do not know how to translate yet.
		 * We will find out when a RESET comes along.
		 */
		return (-1);
	}
	down = !KEY_UP(c);
	c = ks->kbd_cur[KEY_CODE(c)];
	if (c & KEY_MAGIC) {
		switch (c) {

		case KEY_LSHIFT:
			ks->kbd_lshift = down;
			break;

		case KEY_RSHIFT:
			ks->kbd_rshift = down;
			break;

		case KEY_ALLUP:
			ks->kbd_anyshift = 0;
			ks->kbd_control = 0;
			break;

		case KEY_CONTROL:
			ks->kbd_control = down;
			/* FALLTHROUGH */

		case KEY_IGNORE:
			return (-1);

		default:
			panic("kbd_translate");
		}
		if (ks->kbd_anyshift)
			ks->kbd_cur = ks->kbd_shifted;
		else
			ks->kbd_cur = ks->kbd_unshifted;
		return (-1);
	}
	if (!down)
		return (-1);
	if (ks->kbd_control) {
		/* control space and unshifted control atsign return null */
		if (c == ' ' || c == '2')
			return (0);
		/* unshifted control hat */
		if (c == '6')
			return ('^' & 0x1f);
		/* standard controls */
		if (c >= '@' && c < 0x7f)
			return (c & 0x1f);
	}
	return (c);
}


void
kbd_repeat(arg)
	void *arg;
{
	struct kbd_softc *k = (struct kbd_softc *)arg;
	int s = spltty();

	if (k->k_repeating && k->k_repeatc >= 0 && k->k_cons != NULL) {
		ttyinput(k->k_repeatc, k->k_cons);
		timeout(kbd_repeat, k, kbd_repeat_step);
	}
	splx(s);
}

void
kbd_rint(c)
	register int c;
{
	register struct kbd_softc *k = &kbd_softc;
	register struct firm_event *fe;
	register int put;

	if (k->k_repeating) {
		k->k_repeating = 0;
		untimeout(kbd_repeat, k);
	}

	/*
	 * Reset keyboard after serial port overrun, so we can resynch.
	 * The printf below should be shortened and/or replaced with a
	 * call to log() after this is tested (and how will we test it?!).
	 */
	if (c & (TTY_FE|TTY_PE)) {
		printf("keyboard input parity or framing error (0x%x)\n", c);
		(void) ttyoutput(KBD_CMD_RESET, k->k_kbd);
		(*k->k_kbd->t_oproc)(k->k_kbd);
		return;
	}

	/* Read the keyboard id if we read a KBD_RESET last time */
	if (k->k_state.kbd_pending == KBD_RESET) {
		k->k_state.kbd_pending = 0;
		k->k_state.kbd_id = c;
		kbd_reset(&k->k_state);
		if (c == KB_SUN4) {
			/* Arrange to get keyboard layout as well */
			(void)ttyoutput(KBD_CMD_GLAYOUT, k->k_kbd);
			(*k->k_kbd->t_oproc)(k->k_kbd);
		} else
			wakeup((caddr_t)&k->k_state);
		return;
	}

	/* Read the keyboard layout if we read a KBD_LAYOUT last time */
	if (k->k_state.kbd_pending == KBD_LAYOUT) {
		k->k_state.kbd_pending = 0;
		k->k_state.kbd_layout = c;
		/*
		 * Wake up anyone waiting for type.
		 */
		wakeup((caddr_t)&k->k_state);
		return;
	}

	/*
	 * If reset or layout in progress, setup to grab the accompanying
	 * keyboard response next time (id on reset, dip switch on layout).
	 */
	if (c == KBD_RESET || c == KBD_LAYOUT) {
		k->k_state.kbd_pending = c;
		return;
	}

	/*
	 * If /dev/kbd is not connected in event mode, but we are sending
	 * data to /dev/console, translate and send upstream.  Note that
	 * we will get this while opening /dev/kbd if it is not already
	 * open and we do not know its type.
	 */
	if (!k->k_evmode) {
		c = kbd_translate(c, &k->k_state);
		if (c >= 0 && k->k_cons != NULL) {
			ttyinput(c, k->k_cons);
			k->k_repeating = 1;
			k->k_repeatc = c;
			timeout(kbd_repeat, k, kbd_repeat_start);
		}
		return;
	}

	/*
	 * IDLEs confuse the MIT X11R4 server badly, so we must drop them.
	 * This is bad as it means the server will not automatically resync
	 * on all-up IDLEs, but I did not drop them before, and the server
	 * goes crazy when it comes time to blank the screen....
	 */
	if (c == KBD_IDLE)
		return;

	/*
	 * Keyboard is generating events.  Turn this keystroke into an
	 * event and put it in the queue.  If the queue is full, the
	 * keystroke is lost (sorry!).
	 */
	put = k->k_events.ev_put;
	fe = &k->k_events.ev_q[put];
	put = (put + 1) % EV_QSIZE;
	if (put == k->k_events.ev_get) {
		log(LOG_WARNING, "keyboard event queue overflow\n"); /* ??? */
		return;
	}
	fe->id = KEY_CODE(c);
	fe->value = KEY_UP(c) ? VKEY_UP : VKEY_DOWN;
	fe->time = time;
	k->k_events.ev_put = put;
	EV_WAKEUP(&k->k_events);
}

int
kbdopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	int s, error;
	struct tty *tp;

	if (kbd_softc.k_events.ev_io)
		return (EBUSY);
	kbd_softc.k_events.ev_io = p;
	/*
	 * If no console keyboard, tell the device to open up, maybe for
	 * the first time.  Then make sure we know what kind of keyboard
	 * it is.
	 */
	tp = kbd_softc.k_kbd;
	if (kbd_softc.k_cons == NULL)
		(*kbd_softc.k_open)(tp);
	error = 0;
	s = spltty();
	if (kbd_softc.k_state.kbd_cur == NULL) {
		(void) ttyoutput(KBD_CMD_RESET, tp);
		(*tp->t_oproc)(tp);
		error = tsleep((caddr_t)&kbd_softc.k_state, PZERO | PCATCH,
		    devopn, hz);
		if (error == EWOULDBLOCK)	/* no response */
			error = ENXIO;
	}
	splx(s);
	if (error) {
		kbd_softc.k_events.ev_io = NULL;
		return (error);
	}
	ev_init(&kbd_softc.k_events);
	return (0);
}

int
kbdclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{

	/*
	 * Turn off event mode, dump the queue, and close the keyboard
	 * unless it is supplying console input.
	 */
	kbd_softc.k_evmode = 0;
	ev_fini(&kbd_softc.k_events);
	if (kbd_softc.k_cons == NULL)
		(*kbd_softc.k_close)(kbd_softc.k_kbd);
	kbd_softc.k_events.ev_io = NULL;
	return (0);
}

int
kbdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (ev_read(&kbd_softc.k_events, uio, flags));
}

/* this routine should not exist, but is convenient to write here for now */
int
kbdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	return (EOPNOTSUPP);
}

int
kbdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flag;
	struct proc *p;
{
	register struct kbd_softc *k = &kbd_softc;
	register struct kiockey *kmp;
	register u_char *tp;

	switch (cmd) {

	case KIOCTRANS:
		if (*(int *)data == TR_UNTRANS_EVENT)
			return (0);
		break;

	case KIOCGTRANS:
		/*
		 * Get translation mode
		 */
		*(int *)data = TR_UNTRANS_EVENT;
		return (0);

	case KIOCGETKEY:
		if (((struct okiockey *)data)->kio_station == 118) {
			/*
			 * This is X11 asking (in an inappropriate fashion)
			 * if a type 3 keyboard is really a type 3 keyboard.
			 * Say yes (inappropriately).
			 */
			((struct okiockey *)data)->kio_entry = (u_char)HOLE;
			return (0);
		}
		break;

	case KIOCSKEY:
		kmp = (struct kiockey *)data;

		switch (kmp->kio_tablemask) {
		case KIOC_NOMASK:
			tp = kbd_unshifted;
			break;
		case KIOC_SHIFTMASK:
			tp = kbd_shifted;
			break;
		default:
			/* Silently ignore unsupported masks */
			return (0);
		}
		if (kmp->kio_entry & 0xff80)
			/* Silently ignore funny entries */
			return (0);

		tp[kmp->kio_station] = kmp->kio_entry;
		return (0);

	case KIOCGKEY:
		kmp = (struct kiockey *)data;

		switch (kmp->kio_tablemask) {
		case KIOC_NOMASK:
			tp = kbd_unshifted;
			break;
		case KIOC_SHIFTMASK:
			tp = kbd_shifted;
			break;
		default:
			return (0);
		}
		kmp->kio_entry = tp[kmp->kio_station] & ~KEY_MAGIC;
		return (0);

	case KIOCCMD:
		/*
		 * ``unimplemented commands are ignored'' (blech)
		 * so cannot check return value from kbd_docmd
		 */
#ifdef notyet
		while (kbd_docmd(*(int *)data, 1) == ENOSPC) /*ERESTART?*/
			(void) sleep((caddr_t)&lbolt, TTOPRI);
#else
		(void) kbd_docmd(*(int *)data, 1);
#endif
		return (0);

	case KIOCTYPE:
		*(int *)data = k->k_state.kbd_id;
		return (0);

	case KIOCSDIRECT:
		k->k_evmode = *(int *)data;
		return (0);

	case KIOCLAYOUT:
		*(unsigned int *)data = k->k_state.kbd_layout;
		return (0);

	case KIOCSLED:
		if (k->k_state.kbd_id != KB_SUN4) {
			/* xxx NYI */
			k->k_state.kbd_leds = *(char*)data;
		} else {
			int s;
			char leds = *(char *)data;
			struct tty *tp = kbd_softc.k_kbd;
			s = spltty();
			if (tp->t_outq.c_cc > 120)
				(void) tsleep((caddr_t)&lbolt, TTIPRI,
					      ttyout, 0);
			splx(s);
			if (ttyoutput(KBD_CMD_SETLED, tp) >= 0)
				return (ENOSPC);	/* ERESTART? */
			k->k_state.kbd_leds = leds;
			if (ttyoutput(leds, tp) >= 0)
				return (ENOSPC);	/* ERESTART? */
			(*tp->t_oproc)(tp);
		}
		return (0);

	case KIOCGLED:
		*(char *)data = k->k_state.kbd_leds;
		return (0);


	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		k->k_events.ev_async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != k->k_events.ev_io->p_pgid)
			return (EPERM);
		return (0);

	default:
		return (ENOTTY);
	}

	/*
	 * We identified the ioctl, but we do not handle it.
	 */
	return (EOPNOTSUPP);		/* misuse, but what the heck */
}

int
kbdselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{

	return (ev_select(&kbd_softc.k_events, rw, p));
}

/*
 * Execute a keyboard command; return 0 on success.
 * If `isuser', force a small delay before output if output queue
 * is flooding.  (The keyboard runs at 1200 baud, or 120 cps.)
 */
int
kbd_docmd(cmd, isuser)
	int cmd;
	int isuser;
{
	register struct tty *tp = kbd_softc.k_kbd;
	register struct kbd_softc *k = &kbd_softc;
	int s;

	if (tp == NULL)
		return (ENXIO);		/* ??? */
	switch (cmd) {

	case KBD_CMD_BELL:
	case KBD_CMD_NOBELL:
		/* Supported by type 2, 3, and 4 keyboards */
		break;

	case KBD_CMD_CLICK:
		/* Unsupported by type 2 keyboards */
		if (k->k_state.kbd_id != KB_SUN2) {
			k->k_state.kbd_click = 1;
			break;
		}
		return (EINVAL);

	case KBD_CMD_NOCLICK:
		/* Unsupported by type 2 keyboards */
		if (k->k_state.kbd_id != KB_SUN2) {
			k->k_state.kbd_click = 0;
			break;
		}
		return (EINVAL);

	default:
		return (EINVAL);	/* ENOTTY? EOPNOTSUPP? */
	}

	if (isuser) {
		s = spltty();
		if (tp->t_outq.c_cc > 120)
			(void) tsleep((caddr_t)&lbolt, TTIPRI,
			    ttyout, 0);
		splx(s);
	}
	if (ttyoutput(cmd, tp) >= 0)
		return (ENOSPC);	/* ERESTART? */
	(*tp->t_oproc)(tp);
	return (0);
}
