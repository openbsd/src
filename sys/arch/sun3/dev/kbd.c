/*	$NetBSD: kbd.c,v 1.11 1995/10/08 23:40:42 gwr Exp $	*/

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
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>

#include <machine/autoconf.h>
#include <machine/kbd.h>
#include <machine/kbio.h>
#include <machine/vuid_event.h>

#include "event_var.h"

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
	char	kbd_takeid;	/* take next byte as ID */
	u_char	kbd_id;		/* a place to store the ID */
	char	kbd_leds;	/* LED state */
	char	_pad;
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
	int	k_isopen;		/* set if open has been done */
	struct	kbd_state k_state;	/* ASCII decode state */
	struct	evvar k_events;		/* event queue state */
	int	k_repeatc;		/* repeated character */
	int	k_repeating;		/* we've called timeout() */
} kbd_softc;

/* Prototypes */
void	kbd_ascii(struct tty *);
void	kbd_serial(struct tty *, void (*)(), void (*)());
int 	kbd_iopen(void);
void	kbd_reset(struct kbd_softc *);
int kbd_translate(int);
void	kbd_rint(int);
int	kbdopen(dev_t, int, int, struct proc *);
int	kbdclose(dev_t, int, int, struct proc *);
int	kbdread(dev_t, struct uio *, int);
int	kbdwrite(dev_t, struct uio *, int);
int	kbdioctl(dev_t, u_long, caddr_t, int, struct proc *);
int	kbdselect(dev_t, int, struct proc *);
int	kbd_docmd(int, int);

/* set in kbdattach() */
int kbd_repeat_start;
int kbd_repeat_step;

/*
 * Initialization done by either kdcninit or kbd_iopen
 */
void
kbd_init_tables()
{
	struct kbd_state *ks;

	ks = &kbd_softc.k_state;
	if (ks->kbd_cur == NULL) {
		ks->kbd_cur = kbd_unshifted;
		ks->kbd_unshifted = kbd_unshifted;
		ks->kbd_shifted = kbd_shifted;
	}
}

/*
 * Attach the console keyboard ASCII (up-link) interface.
 * This is called by the "kd" (keyboard/display) driver to
 * tell this module where to send read-side data.
 */
void
kbd_ascii(struct tty *tp)
{
	kbd_softc.k_cons = tp;
}

/*
 * Attach the console keyboard serial (down-link) interface.
 * This is called by the "zs" driver for the keyboard port
 * to tell this module how to talk to the keyboard.
 */
void
kbd_serial(struct tty *tp, void (*iopen)(), void (*iclose)())
{
	register struct kbd_softc *k;

	k = &kbd_softc;
	k->k_kbd = tp;
	k->k_open = iopen;
	k->k_close = iclose;

	/* Do this before any calls to kbd_rint(). */
	kbd_init_tables();

	/* Now attach the (kd) pseudo-driver. */
	kd_attach(1);	/* This calls kbd_ascii() */
}

/*
 * Initialization to be done at first open.
 * This is called from kbdopen or kdopen (in kd.c)
 */
int
kbd_iopen()
{
	struct kbd_softc *k;
	struct tty *tp;
	int error, s;

	k = &kbd_softc;

	/* Tolerate extra calls. */
	if (k->k_isopen)
		return (0);

	/* Make sure "down" link (to zs1a) is established. */
	tp = k->k_kbd;
	if (tp == NULL)
		return (ENXIO);

	kbd_repeat_start = hz/2;
	kbd_repeat_step = hz/20;

	/* Open the "down" link (never to be closed). */
	tp->t_ispeed = tp->t_ospeed = 1200;
	(*k->k_open)(tp);

	/* Reset the keyboard and find out its type. */
	s = spltty();
	(void) ttyoutput(KBD_CMD_RESET, tp);
	(*tp->t_oproc)(tp);
	/* The wakeup for this sleep is in kbd_reset(). */
	error = tsleep((caddr_t)k, PZERO | PCATCH,
				   devopn, hz);
	if (error == EWOULDBLOCK) { 	/* no response */
		log(LOG_ERR, "keyboard reset failed\n");
		/*
		 * Allow the open anyway (to keep getty happy)
		 * but assume the "least common denominator".
		 */
		k->k_state.kbd_id = KB_SUN2;
		error = 0;
	}

	if (error == 0)
		k->k_isopen = 1;

	splx(s);
	return error;
}

void
kbd_reset(k)
	struct kbd_softc *k;
{
	struct kbd_state *ks;

	ks = &k->k_state;

	/*
	 * On first identification, wake up anyone waiting for type
	 * and set up the table pointers.
	 */
	if (k->k_isopen == 0)
		wakeup((caddr_t)k);

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
	}

	/* LEDs are off after reset. */
	ks->kbd_leds = 0;
}

/*
 * Turn keyboard up/down codes into ASCII.
 */
int
kbd_translate(register int c)
{
	register struct kbd_state *ks;
	register int down;

	ks = &kbd_softc.k_state;
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
kbd_repeat(void *arg)
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
kbd_rint(register int c)
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
	 */
	if (c & (TTY_FE|TTY_PE)) {
		log(LOG_ERR, "keyboard input error (0x%x)\n", c);
		(void) ttyoutput(KBD_CMD_RESET, k->k_kbd);
		(*k->k_kbd->t_oproc)(k->k_kbd);
		return;
	}

	/* Read the keyboard id if we read a KBD_RESET last time */
	if (k->k_state.kbd_takeid) {
		k->k_state.kbd_takeid = 0;
		k->k_state.kbd_id = c;
		kbd_reset(k);
		return;
	}

	/* If we have been reset, setup to grab the keyboard id next time */
	if (c == KBD_RESET) {
		k->k_state.kbd_takeid = 1;
		return;
	}

	/*
	 * If /dev/kbd is not connected in event mode, but we are sending
	 * data to /dev/console, translate and send upstream.  Note that
	 * we will get this while opening /dev/kbd if it is not already
	 * open and we do not know its type.
	 */
	if (!k->k_evmode) {
		c = kbd_translate(c);
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
kbdopen(dev_t dev, int flags, int mode, struct proc *p)
{
	int error;

#if 1	/* XXX - temporary hack */
	/* XXX - Should make login chown devices in /etc/fbtab */
	/* Require root or same UID as the kd session leader. */
	if (p->p_ucred->cr_uid) {
		struct tty *kd_tp;
		struct proc *kd_p;
		extern struct tty *kdtty();

		/* Make sure kd is attached and open. */
		kd_tp = kdtty(0);
		if ((kd_tp == NULL) || (kd_tp->t_session == NULL))
			return (EPERM);
		kd_p = kd_tp->t_session->s_leader;
		if (p->p_ucred->cr_uid != kd_p->p_ucred->cr_uid)
			return (EACCES);
	}
#endif

	/* Exclusive open required for /dev/kbd */
	if (kbd_softc.k_events.ev_io)
		return (EBUSY);
	kbd_softc.k_events.ev_io = p;

	if ((error = kbd_iopen()) != 0) {
		kbd_softc.k_events.ev_io = NULL;
		return (error);
	}
	ev_init(&kbd_softc.k_events);
	return (0);
}

int
kbdclose(dev_t dev, int flags, int mode, struct proc *p)
{

	/*
	 * Turn off event mode, dump the queue, and close the keyboard
	 * unless it is supplying console input.
	 */
	kbd_softc.k_evmode = 0;
	ev_fini(&kbd_softc.k_events);
	kbd_softc.k_events.ev_io = NULL;
	return (0);
}

int
kbdread(dev_t dev, struct uio *uio, int flags)
{

	return (ev_read(&kbd_softc.k_events, uio, flags));
}

/* this routine should not exist, but is convenient to write here for now */
int
kbdwrite(dev_t dev, struct uio *uio, int flags)
{

	return (EOPNOTSUPP);
}

int
kbdioctl(dev_t dev, u_long cmd, register caddr_t data,
	int flag, struct proc *p)
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
			 * This is X11 asking if a type 3 keyboard is
			 * really a type 3 keyboard.  Say yes.
			 */
			((struct okiockey *)data)->kio_entry = HOLE;
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
		*data = 0;
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
kbdselect(dev_t dev, int rw, struct proc *p)
{

	return (ev_select(&kbd_softc.k_events, rw, p));
}

/*
 * Execute a keyboard command; return 0 on success.
 * If `isuser', force a small delay before output if output queue
 * is flooding.  (The keyboard runs at 1200 baud, or 120 cps.)
 */
int
kbd_docmd(int cmd, int isuser)
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
