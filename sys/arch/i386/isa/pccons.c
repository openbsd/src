/*	$NetBSD: pccons.c,v 1.91 1995/12/24 02:30:25 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.  All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)pccons.c	5.11 (Berkeley) 5/21/91
 */

/*
 * code to work keyboard & display for PC-style console
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/pc/display.h>
#include <machine/pccons.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <i386/isa/isa_machdep.h>
#include <i386/isa/kbdreg.h>

#define	XFREE86_BUG_COMPAT

#ifndef BEEP_FREQ
#define BEEP_FREQ 1500
#endif
#ifndef BEEP_TIME
#define BEEP_TIME (hz/5)
#endif

#define PCBURST 128

static u_short *Crtat;			/* pointer to backing store */
static u_short *crtat;			/* pointer to current char */
static volatile u_char ack, nak;	/* Don't ask. */
static u_char async, kernel, polling;	/* Really, you don't want to know. */
static u_char lock_state = 0x00,	/* all off */
	      old_lock_state = 0xff,
	      typematic_rate = 0xff,	/* don't update until set by user */
	      old_typematic_rate = 0xff;
static u_short cursor_shape = 0xffff,	/* don't update until set by user */
	       old_cursor_shape = 0xffff;
int pc_xmode = 0;

#define	PCUNIT(x)	(minor(x))

static struct video_state {
	int 	cx, cy;		/* escape parameters */
	int 	row, col;	/* current cursor position */
	int 	nrow, ncol, nchr;	/* current screen geometry */
	u_char	state;		/* parser state */
#define	VSS_ESCAPE	1
#define	VSS_EBRACE	2
#define	VSS_EPARAM	3
	char	so;		/* in standout mode? */
	char	color;		/* color or mono display */
	char	at;		/* normal attributes */
	char	so_at;		/* standout attributes */
} vs;

struct pc_softc {
	struct	device sc_dev;
	void	*sc_ih;
	struct	tty *sc_tty;
};

int pcprobe __P((struct device *, void *, void *));
void pcattach __P((struct device *, struct device *, void *));
int pcintr __P((void *));

struct cfdriver pccd = {
	NULL, "pc", pcprobe, pcattach, DV_TTY, sizeof(struct pc_softc)
};

#define	COL		80
#define	ROW		25
#define	CHR		2

/*
 * DANGER WIL ROBINSON -- the values of SCROLL, NUM, CAPS, and ALT are
 * important.
 */
#define	SCROLL		0x0001	/* stop output */
#define	NUM		0x0002	/* numeric shift  cursors vs. numeric */
#define	CAPS		0x0004	/* caps shift -- swaps case of letter */
#define	SHIFT		0x0008	/* keyboard shift */
#define	CTL		0x0010	/* control shift  -- allows ctl function */
#define	ASCII		0x0020	/* ascii code for this key */
#define	ALT		0x0080	/* alternate shift -- alternate chars */
#define	FUNC		0x0100	/* function key */
#define	KP		0x0200	/* Keypad keys */
#define	NONE		0x0400	/* no function */

static unsigned int addr_6845 = MONO_BASE;

char *sget __P((void));
void sput __P((u_char *, int));

void	pcstart();
int	pcparam();
char	partab[];

extern pcopen(dev_t, int, int, struct proc *);

#define	KBD_DELAY \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); } \
	{ u_char x = inb(0x84); }

static inline int
kbd_wait_output()
{
	u_int i;

	for (i = 100000; i; i--)
		if ((inb(KBSTATP) & KBS_IBF) == 0) {
			KBD_DELAY;
			return 1;
		}
	return 0;
}

static inline int
kbd_wait_input()
{
	u_int i;

	for (i = 100000; i; i--)
		if ((inb(KBSTATP) & KBS_DIB) != 0) {
			KBD_DELAY;
			return 1;
		}
	return 0;
}

static inline void
kbd_flush_input()
{
	u_int i;

	for (i = 10; i; i--) {
		if ((inb(KBSTATP) & KBS_DIB) == 0)
			return;
		KBD_DELAY;
		(void) inb(KBDATAP);
	}
}

#if 1
/*
 * Get the current command byte.
 */
static u_char
kbc_get8042cmd()
{

	if (!kbd_wait_output())
		return -1;
	outb(KBCMDP, K_RDCMDBYTE);
	if (!kbd_wait_input())
		return -1;
	return inb(KBDATAP);
}
#endif

/*
 * Pass command byte to keyboard controller (8042).
 */
static int
kbc_put8042cmd(val)
	u_char val;
{

	if (!kbd_wait_output())
		return 0;
	outb(KBCMDP, K_LDCMDBYTE);
	if (!kbd_wait_output())
		return 0;
	outb(KBOUTP, val);
	return 1;
}

/*
 * Pass command to keyboard itself
 */
int
kbd_cmd(val, polling)
	u_char val;
	u_char polling;
{
	u_int retries = 3;
	register u_int i;

	do {
		if (!kbd_wait_output())
			return 0;
		ack = nak = 0;
		outb(KBOUTP, val);
		if (polling)
			for (i = 100000; i; i--) {
				if (inb(KBSTATP) & KBS_DIB) {
					register u_char c;

					KBD_DELAY;
					c = inb(KBDATAP);
					if (c == KBR_ACK || c == KBR_ECHO) {
						ack = 1;
						return 1;
					}
					if (c == KBR_RESEND) {
						nak = 1;
						break;
					}
#ifdef DIAGNOSTIC
					printf("kbd_cmd: input char %x lost\n", c);
#endif
				}
			}
		else
			for (i = 100000; i; i--) {
				(void) inb(KBSTATP);
				if (ack)
					return 1;
				if (nak)
					break;
			}
		if (!nak)
			return 0;
	} while (--retries);
	return 0;
}

void
set_cursor_shape()
{
	register int iobase = addr_6845;

	outb(iobase, 10);
	outb(iobase+1, cursor_shape >> 8);
	outb(iobase, 11);
	outb(iobase+1, cursor_shape);
	old_cursor_shape = cursor_shape;
}

void
get_cursor_shape()
{
	register int iobase = addr_6845;

	outb(iobase, 10);
	cursor_shape = inb(iobase+1) << 8;
	outb(iobase, 11);
	cursor_shape |= inb(iobase+1);

	/*
	 * real 6845's, as found on, MDA, Hercules or CGA cards, do
	 * not support reading the cursor shape registers. the 6845
	 * tri-states it's data bus. This is _normally_ read by the
	 * cpu as either 0x00 or 0xff.. in which case we just use
	 * a line cursor.
	 */
	if (cursor_shape == 0x0000 || cursor_shape == 0xffff)
		cursor_shape = 0x0b10;
	else
		cursor_shape &= 0x1f1f;
}

void
do_async_update(poll)
	u_char poll;
{
	int pos;
	static int old_pos = -1;

	async = 0;

	if (lock_state != old_lock_state) {
		old_lock_state = lock_state;
		if (!kbd_cmd(KBC_MODEIND, poll) ||
		    !kbd_cmd(lock_state, poll)) {
			printf("pc: timeout updating leds\n");
			(void) kbd_cmd(KBC_ENABLE, poll);
		}
	}
	if (typematic_rate != old_typematic_rate) {
		old_typematic_rate = typematic_rate;
		if (!kbd_cmd(KBC_TYPEMATIC, poll) ||
		    !kbd_cmd(typematic_rate, poll)) {
			printf("pc: timeout updating typematic rate\n");
			(void) kbd_cmd(KBC_ENABLE, poll);
		}
	}

	if (pc_xmode > 0)
		return;

	pos = crtat - Crtat;
	if (pos != old_pos) {
		register int iobase = addr_6845;
		outb(iobase, 14);
		outb(iobase+1, pos >> 8);
		outb(iobase, 15);
		outb(iobase+1, pos);
		old_pos = pos;
	}
	if (cursor_shape != old_cursor_shape)
		set_cursor_shape();
}

void
async_update()
{

	if (kernel || polling) {
		if (async)
			untimeout(do_async_update, NULL);
		do_async_update(1);
	} else {
		if (async)
			return;
		async = 1;
		timeout(do_async_update, NULL, 1);
	}
}

/*
 * these are both bad jokes
 */
int
pcprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	u_int i;

	/* Enable interrupts and keyboard, etc. */
	if (!kbc_put8042cmd(CMDBYTE)) {
		printf("pcprobe: command error\n");
		return 0;
	}

#if 1
	/* Flush any garbage. */
	kbd_flush_input();
	/* Reset the keyboard. */
	if (!kbd_cmd(KBC_RESET, 1)) {
		printf("pcprobe: reset error %d\n", 1);
		goto lose;
	}
	for (i = 600000; i; i--)
		if ((inb(KBSTATP) & KBS_DIB) != 0) {
			KBD_DELAY;
			break;
		}
	if (i == 0 || inb(KBDATAP) != KBR_RSTDONE) {
		printf("pcprobe: reset error %d\n", 2);
		goto lose;
	}
	/*
	 * Some keyboards seem to leave a second ack byte after the reset.
	 * This is kind of stupid, but we account for them anyway by just
	 * flushing the buffer.
	 */
	kbd_flush_input();
	/* Just to be sure. */
	if (!kbd_cmd(KBC_ENABLE, 1)) {
		printf("pcprobe: reset error %d\n", 3);
		goto lose;
	}

	/*
	 * Some keyboard/8042 combinations do not seem to work if the keyboard
	 * is set to table 1; in fact, it would appear that some keyboards just
	 * ignore the command altogether.  So by default, we use the AT scan
	 * codes and have the 8042 translate them.  Unfortunately, this is
	 * known to not work on some PS/2 machines.  We try desparately to deal
	 * with this by checking the (lack of a) translate bit in the 8042 and
	 * attempting to set the keyboard to XT mode.  If this all fails, well,
	 * tough luck.
	 *
	 * XXX It would perhaps be a better choice to just use AT scan codes
	 * and not bother with this.
	 */
	if (kbc_get8042cmd() & KC8_TRANS) {
		/* The 8042 is translating for us; use AT codes. */
		if (!kbd_cmd(KBC_SETTABLE, 1) || !kbd_cmd(2, 1)) {
			printf("pcprobe: reset error %d\n", 4);
			goto lose;
		}
	} else {
		/* Stupid 8042; set keyboard to XT codes. */
		if (!kbd_cmd(KBC_SETTABLE, 1) || !kbd_cmd(1, 1)) {
			printf("pcprobe: reset error %d\n", 5);
			goto lose;
		}
	}

lose:
	/*
	 * Technically, we should probably fail the probe.  But we'll be nice
	 * and allow keyboard-less machines to boot with the console.
	 */
#endif

	ia->ia_iosize = 16;
	ia->ia_msize = 0;
	return 1;
}

void
pcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pc_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	printf(": %s\n", vs.color ? "color" : "mono");
	do_async_update(1);

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_EDGE, IPL_TTY, pcintr,
	    sc);
}

int
pcopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct pc_softc *sc;
	int unit = PCUNIT(dev);
	struct tty *tp;

	if (unit >= pccd.cd_ndevs)
		return ENXIO;
	sc = pccd.cd_devs[unit];
	if (sc == 0)
		return ENXIO;

	if (!sc->sc_tty)
		tp = sc->sc_tty = ttymalloc();
	else
		tp = sc->sc_tty;

	tp->t_oproc = pcstart;
	tp->t_param = pcparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		pcparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
pcclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
#ifdef notyet /* XXX */
	ttyfree(tp);
#endif
	return(0);
}

int
pcread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
pcwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
pctty(dev)
	dev_t dev;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 * Catch the character, and see who it goes to.
 */
int
pcintr(arg)
	void *arg;
{
	struct pc_softc *sc = arg;
	register struct tty *tp = sc->sc_tty;
	u_char *cp;

	if ((inb(KBSTATP) & KBS_DIB) == 0)
		return 0;
	if (polling)
		return 1;
	do {
		cp = sget();
		if (!tp || (tp->t_state & TS_ISOPEN) == 0)
			return 1;
		if (cp)
			do
				(*linesw[tp->t_line].l_rint)(*cp++, tp);
			while (*cp);
	} while (inb(KBSTATP) & KBS_DIB);
	return 1;
}

int
pcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case CONSOLE_X_MODE_ON:
		pc_xmode_on();
		return 0;
	case CONSOLE_X_MODE_OFF:
		pc_xmode_off();
		return 0;
	case CONSOLE_X_BELL:
		/*
		 * If set, data is a pointer to a length 2 array of
		 * integers.  data[0] is the pitch in Hz and data[1]
		 * is the duration in msec.
		 */
		if (data)
			sysbeep(((int*)data)[0],
				(((int*)data)[1] * hz) / 1000);
		else
			sysbeep(BEEP_FREQ, BEEP_TIME);
		return 0;
	case CONSOLE_SET_TYPEMATIC_RATE: {
 		u_char	rate;

 		if (!data)
			return EINVAL;
		rate = *((u_char *)data);
		/*
		 * Check that it isn't too big (which would cause it to be
		 * confused with a command).
		 */
		if (rate & 0x80)
			return EINVAL;
		typematic_rate = rate;
		async_update();
		return 0;
 	}
	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("pcioctl: impossible");
#endif
}

void
pcstart(tp)
	struct tty *tp;
{
	struct clist *cl;
	int s, len, n;
	u_char buf[PCBURST];

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto out;
	tp->t_state |= TS_BUSY;
	splx(s);
	/*
	 * We need to do this outside spl since it could be fairly
	 * expensive and we don't want our serial ports to overflow.
	 */
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, PCBURST);
	sput(buf, len);
	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (cl->c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout(ttrstrt, tp, 1);
	}
	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(cl);
		}
		selwakeup(&tp->t_wsel);
	}
out:
	splx(s);
}

void
pcstop(tp, flag)
	struct tty *tp;
	int flag;
{

}

void
pccnprobe(cp)
	struct consdev *cp;
{
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == pcopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;
}

/* ARGSUSED */
void
pccninit(cp)
	struct consdev *cp;
{

	/*
	 * For now, don't screw with it.
	 */
	/* crtat = 0; */
}

/* ARGSUSED */
void
pccnputc(dev, c)
	dev_t dev;
	char c;
{
	u_char oldkernel = kernel;

	kernel = 1;
	if (c == '\n')
		sput("\r\n", 2);
	else
		sput(&c, 1);
	kernel = oldkernel;
}

/* ARGSUSED */
pccngetc(dev)
	dev_t dev;
{
	register char *cp;

	if (pc_xmode > 0)
		return 0;

	do {
		/* wait for byte */
		while ((inb(KBSTATP) & KBS_DIB) == 0);
		/* see if it's worthwhile */
		cp = sget();
	} while (!cp);
	if (*cp == '\r')
		return '\n';
	return *cp;
}

void
pccnpollc(dev, on)
	dev_t dev;
	int on;
{

	polling = on;
	if (!on) {
		int unit;
		struct pc_softc *sc;
		int s;

		/*
		 * If disabling polling on a device that's been configured,
		 * make sure there are no bytes left in the FIFO, holding up
		 * the interrupt line.  Otherwise we won't get any further
		 * interrupts.
		 */
		unit = PCUNIT(dev);
		if (pccd.cd_ndevs > unit) {
			sc = pccd.cd_devs[unit];
			if (sc != 0) {
				s = spltty();
				pcintr(sc);
				splx(s);
			}
		}
	}
}	

/*
 * Set line parameters.
 */
int
pcparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

#define	wrtchar(c, at) do {\
	char *cp = (char *)crtat; *cp++ = (c); *cp = (at); crtat++; vs.col++; \
} while (0)

/* translate ANSI color codes to standard pc ones */
static char fgansitopc[] = {
	FG_BLACK, FG_RED, FG_GREEN, FG_BROWN, FG_BLUE,
	FG_MAGENTA, FG_CYAN, FG_LIGHTGREY
};

static char bgansitopc[] = {
	BG_BLACK, BG_RED, BG_GREEN, BG_BROWN, BG_BLUE,
	BG_MAGENTA, BG_CYAN, BG_LIGHTGREY
};

/*
 * `pc3' termcap emulation.
 */
void
sput(cp, n)
	u_char *cp;
	int n;
{
	u_char c, scroll = 0;

	if (pc_xmode > 0)
		return;

	if (crtat == 0) {
		u_short volatile *cp;
		u_short was;
		unsigned cursorat;

		cp = ISA_HOLE_VADDR(CGA_BUF);
		was = *cp;
		*cp = (u_short) 0xA55A;
		if (*cp != 0xA55A) {
			cp = ISA_HOLE_VADDR(MONO_BUF);
			addr_6845 = MONO_BASE;
			vs.color = 0;
		} else {
			*cp = was;
			addr_6845 = CGA_BASE;
			vs.color = 1;
		}

		/* Extract cursor location */
		outb(addr_6845, 14);
		cursorat = inb(addr_6845+1) << 8;
		outb(addr_6845, 15);
		cursorat |= inb(addr_6845+1);

#ifdef FAT_CURSOR
		cursor_shape = 0x0012;
#endif

		Crtat = (u_short *)cp;
		crtat = (u_short *)(cp + cursorat);

		vs.ncol = COL;
		vs.nrow = ROW;
		vs.nchr = COL * ROW;
		vs.at = FG_LIGHTGREY | BG_BLACK;

		if (vs.color == 0)
			vs.so_at = FG_BLACK | BG_LIGHTGREY;
		else
			vs.so_at = FG_YELLOW | BG_BLACK;

		fillw((vs.at << 8) | ' ', crtat, vs.nchr - cursorat);
	}

	while (n--) {
		if (!(c = *cp++))
			continue;

		switch (c) {
		case 0x1B:
			if (vs.state >= VSS_ESCAPE) {
				wrtchar(c, vs.so_at); 
				vs.state = 0;
				goto maybe_scroll;
			} else
				vs.state = VSS_ESCAPE;
			break;

		case '\t': {
			int inccol = 8 - (vs.col & 7);
			crtat += inccol;
			vs.col += inccol;
		}
		maybe_scroll:
			if (vs.col >= COL) {
				vs.col -= COL;
				scroll = 1;
			}
			break;

		case '\010':
			if (crtat <= Crtat)
				break;
			--crtat;
			if (--vs.col < 0)
				vs.col += COL;	/* non-destructive backspace */
			break;

		case '\r':
			crtat -= vs.col;
			vs.col = 0;
			break;

		case '\n':
			crtat += vs.ncol;
			scroll = 1;
			break;

		default:
		bypass:
			switch (vs.state) {
			case 0:
				if (c == '\a')
					sysbeep(BEEP_FREQ, BEEP_TIME);
				else {
					/*
					 * If we're outputting multiple printed
					 * characters, just blast them to the
					 * screen until we reach the end of the
					 * buffer or a control character.  This
					 * saves time by short-circuiting the
					 * switch.
					 * If we reach the end of the line, we
					 * break to do a scroll check.
					 */
					for (;;) {
						if (vs.so)
							wrtchar(c, vs.so_at);
						else
							wrtchar(c, vs.at);
						if (vs.col >= vs.ncol) {
							vs.col = 0;
							scroll = 1;
							break;
						}
						if (!n || (c = *cp) < ' ')
							break;
						n--, cp++;
					}
				}
				break;
			case VSS_ESCAPE:
				if (c == '[') {	/* Start ESC [ sequence */
					vs.cx = vs.cy = 0;
					vs.state = VSS_EBRACE;
				} else if (c == 'c') { /* Clear screen & home */
					fillw((vs.at << 8) | ' ', Crtat,
					    vs.nchr);
					crtat = Crtat;
					vs.col = 0;
					vs.state = 0;
				} else { /* Invalid, clear state */
					wrtchar(c, vs.so_at); 
					vs.state = 0;
					goto maybe_scroll;
				}
				break;
			default: /* VSS_EBRACE or VSS_EPARAM */
				switch (c) {
					int pos;
				case 'm':
					if (!vs.cx)
						vs.so = 0;
					else
						vs.so = 1;
					vs.state = 0;
					break;
				case 'A': { /* back cx rows */
					int cx = vs.cx;
					if (cx <= 0)
						cx = 1;
					else
						cx %= vs.nrow;
					pos = crtat - Crtat;
					pos -= vs.ncol * cx;
					if (pos < 0)
						pos += vs.nchr;
					crtat = Crtat + pos;
					vs.state = 0;
					break;
				}
				case 'B': { /* down cx rows */
					int cx = vs.cx;
					if (cx <= 0)
						cx = 1;
					else
						cx %= vs.nrow;
					pos = crtat - Crtat;
					pos += vs.ncol * cx;
					if (pos >= vs.nchr) 
						pos -= vs.nchr;
					crtat = Crtat + pos;
					vs.state = 0;
					break;
				}
				case 'C': { /* right cursor */
					int cx = vs.cx,
					    col = vs.col;
					if (cx <= 0)
						cx = 1;
					else
						cx %= vs.ncol;
					pos = crtat - Crtat;
					pos += cx;
					col += cx;
					if (col >= vs.ncol) {
						pos -= vs.ncol;
						col -= vs.ncol;
					}
					vs.col = col;
					crtat = Crtat + pos;
					vs.state = 0;
					break;
				}
				case 'D': { /* left cursor */
					int cx = vs.cx,
					    col = vs.col;
					if (cx <= 0)
						cx = 1;
					else
						cx %= vs.ncol;
					pos = crtat - Crtat;
					pos -= cx;
					col -= cx;
					if (col < 0) {
						pos += vs.ncol;
						col += vs.ncol;
					}
					vs.col = col;
					crtat = Crtat + pos;
					vs.state = 0;
					break;
				}
				case 'J': /* Clear ... */
					switch (vs.cx) {
					case 0:
						/* ... to end of display */
						fillw((vs.at << 8) | ' ', crtat,
						    Crtat + vs.nchr - crtat);
						break;
					case 1:
						/* ... to next location */
						fillw((vs.at << 8) | ' ', Crtat,
						    crtat - Crtat + 1);
						break;
					case 2:
						/* ... whole display */
						fillw((vs.at << 8) | ' ', Crtat,
						    vs.nchr);
						break;
					}
					vs.state = 0;
					break;
				case 'K': /* Clear line ... */
					switch (vs.cx) {
					case 0:
						/* ... current to EOL */
						fillw((vs.at << 8) | ' ', crtat,
						    vs.ncol - vs.col);
						break;
					case 1:
						/* ... beginning to next */
						fillw((vs.at << 8) | ' ',
						    crtat - vs.col,
						    vs.col + 1);
						break;
					case 2:
						/* ... entire line */
						fillw((vs.at << 8) | ' ',
						    crtat - vs.col, vs.ncol);
						break;
					}
					vs.state = 0;
					break;
				case 'f': /* in system V consoles */
				case 'H': { /* Cursor move */
					int cx = vs.cx,
					    cy = vs.cy;
					if (!cx || !cy) {
						crtat = Crtat;
						vs.col = 0;
					} else {
						if (cx > vs.nrow)
							cx = vs.nrow;
						if (cy > vs.ncol)
							cy = vs.ncol;
						crtat = Crtat +
						    (cx - 1) * vs.ncol + cy - 1;
						vs.col = cy - 1;
					}
					vs.state = 0;
					break;
				}
				case 'M': { /* delete cx rows */
					u_short *crtAt = crtat - vs.col;
					int cx = vs.cx,
					    row = (crtAt - Crtat) / vs.ncol,
					    nrow = vs.nrow - row;
					if (cx <= 0)
						cx = 1;
					else if (cx > nrow)
						cx = nrow;
					if (cx < nrow)
						bcopy(crtAt + vs.ncol * cx,
						    crtAt, vs.ncol * (nrow -
						    cx) * CHR);
					fillw((vs.at << 8) | ' ',
					    crtAt + vs.ncol * (nrow - cx),
					    vs.ncol * cx);
					vs.state = 0;
					break;
				}
				case 'S': { /* scroll up cx lines */
					int cx = vs.cx;
					if (cx <= 0)
						cx = 1;
					else if (cx > vs.nrow)
						cx = vs.nrow;
					if (cx < vs.nrow)
						bcopy(Crtat + vs.ncol * cx,
						    Crtat, vs.ncol * (vs.nrow -
						    cx) * CHR);
					fillw((vs.at << 8) | ' ',
					    Crtat + vs.ncol * (vs.nrow - cx),
					    vs.ncol * cx);
					/* crtat -= vs.ncol * cx; /* XXX */
					vs.state = 0;
					break;
				}
				case 'L': { /* insert cx rows */
					u_short *crtAt = crtat - vs.col;
					int cx = vs.cx,
					    row = (crtAt - Crtat) / vs.ncol,
					    nrow = vs.nrow - row;
					if (cx <= 0)
						cx = 1;
					else if (cx > nrow)
						cx = nrow;
					if (cx < nrow)
						bcopy(crtAt,
						    crtAt + vs.ncol * cx,
						    vs.ncol * (nrow - cx) *
						    CHR);
					fillw((vs.at << 8) | ' ', crtAt,
					    vs.ncol * cx);
					vs.state = 0;
					break;
				}
				case 'T': { /* scroll down cx lines */
					int cx = vs.cx;
					if (cx <= 0)
						cx = 1;
					else if (cx > vs.nrow)
						cx = vs.nrow;
					if (cx < vs.nrow)
						bcopy(Crtat,
						    Crtat + vs.ncol * cx,
						    vs.ncol * (vs.nrow - cx) *
						    CHR);
					fillw((vs.at << 8) | ' ', Crtat,
					    vs.ncol * cx);
					/* crtat += vs.ncol * cx; /* XXX */
					vs.state = 0;
					break;
				}
				case ';': /* Switch params in cursor def */
					vs.state = VSS_EPARAM;
					break;
				case 'r':
					vs.so_at = (vs.cx & FG_MASK) |
					    ((vs.cy << 4) & BG_MASK);
					vs.state = 0;
					break;
				case 'x': /* set attributes */
					switch (vs.cx) {
					case 0:
						vs.at = FG_LIGHTGREY | BG_BLACK;
						break;
					case 1:
						/* ansi background */
						if (!vs.color)
							break;
						vs.at &= FG_MASK;
						vs.at |= bgansitopc[vs.cy & 7];
						break;
					case 2:
						/* ansi foreground */
						if (!vs.color)
							break;
						vs.at &= BG_MASK;
						vs.at |= fgansitopc[vs.cy & 7];
						break;
					case 3:
						/* pc text attribute */
						if (vs.state >= VSS_EPARAM)
							vs.at = vs.cy;
						break;
					}
					vs.state = 0;
					break;
					
				default: /* Only numbers valid here */
					if ((c >= '0') && (c <= '9')) {
						if (vs.state >= VSS_EPARAM) {
							vs.cy *= 10;
							vs.cy += c - '0';
						} else {
							vs.cx *= 10;
							vs.cx += c - '0';
						}
					} else
						vs.state = 0;
					break;
				}
				break;
			}
		}
		if (scroll) {
			scroll = 0;
			/* scroll check */
			if (crtat >= Crtat + vs.nchr) {
				if (!kernel) {
					int s = spltty();
					if (lock_state & SCROLL)
						tsleep((caddr_t)&lock_state,
						    PUSER, "pcputc", 0);
					splx(s);
				}
				bcopy(Crtat + vs.ncol, Crtat,
				    (vs.nchr - vs.ncol) * CHR);
				fillw((vs.at << 8) | ' ',
				    Crtat + vs.nchr - vs.ncol, vs.ncol);
				crtat -= vs.ncol;
			}
		}
	}
	async_update();
}

#define	CODE_SIZE	4		/* Use a max of 4 for now... */
typedef struct {
	u_short	type;
	char unshift[CODE_SIZE];
	char shift[CODE_SIZE];
	char ctl[CODE_SIZE];
} Scan_def;

static Scan_def	scan_codes[] = {
	NONE,	"",		"",		"",		/* 0 unused */
	ASCII,	"\033",		"\033",		"\033",		/* 1 ESCape */
	ASCII,	"1",		"!",		"!",		/* 2 1 */
	ASCII,	"2",		"@",		"\000",		/* 3 2 */
	ASCII,	"3",		"#",		"#",		/* 4 3 */
	ASCII,	"4",		"$",		"$",		/* 5 4 */
	ASCII,	"5",		"%",		"%",		/* 6 5 */
	ASCII,	"6",		"^",		"\036",		/* 7 6 */
	ASCII,	"7",		"&",		"&",		/* 8 7 */
	ASCII,	"8",		"*",		"\010",		/* 9 8 */
	ASCII,	"9",		"(",		"(",		/* 10 9 */
	ASCII,	"0",		")",		")",		/* 11 0 */
	ASCII,	"-",		"_",		"\037",		/* 12 - */
	ASCII,	"=",		"+",		"+",		/* 13 = */
	ASCII,	"\177",		"\177",		"\010",		/* 14 backspace */
	ASCII,	"\t",		"\177\t",	"\t",		/* 15 tab */
	ASCII,	"q",		"Q",		"\021",		/* 16 q */
	ASCII,	"w",		"W",		"\027",		/* 17 w */
	ASCII,	"e",		"E",		"\005",		/* 18 e */
	ASCII,	"r",		"R",		"\022",		/* 19 r */
	ASCII,	"t",		"T",		"\024",		/* 20 t */
	ASCII,	"y",		"Y",		"\031",		/* 21 y */
	ASCII,	"u",		"U",		"\025",		/* 22 u */
	ASCII,	"i",		"I",		"\011",		/* 23 i */
	ASCII,	"o",		"O",		"\017",		/* 24 o */
	ASCII,	"p",		"P",		"\020",		/* 25 p */
	ASCII,	"[",		"{",		"\033",		/* 26 [ */
	ASCII,	"]",		"}",		"\035",		/* 27 ] */
	ASCII,	"\r",		"\r",		"\n",		/* 28 return */
	CTL,	"",		"",		"",		/* 29 control */
	ASCII,	"a",		"A",		"\001",		/* 30 a */
	ASCII,	"s",		"S",		"\023",		/* 31 s */
	ASCII,	"d",		"D",		"\004",		/* 32 d */
	ASCII,	"f",		"F",		"\006",		/* 33 f */
	ASCII,	"g",		"G",		"\007",		/* 34 g */
	ASCII,	"h",		"H",		"\010",		/* 35 h */
	ASCII,	"j",		"J",		"\n",		/* 36 j */
	ASCII,	"k",		"K",		"\013",		/* 37 k */
	ASCII,	"l",		"L",		"\014",		/* 38 l */
	ASCII,	";",		":",		";",		/* 39 ; */
	ASCII,	"'",		"\"",		"'",		/* 40 ' */
	ASCII,	"`",		"~",		"`",		/* 41 ` */
	SHIFT,	"",		"",		"",		/* 42 shift */
	ASCII,	"\\",		"|",		"\034",		/* 43 \ */
	ASCII,	"z",		"Z",		"\032",		/* 44 z */
	ASCII,	"x",		"X",		"\030",		/* 45 x */
	ASCII,	"c",		"C",		"\003",		/* 46 c */
	ASCII,	"v",		"V",		"\026",		/* 47 v */
	ASCII,	"b",		"B",		"\002",		/* 48 b */
	ASCII,	"n",		"N",		"\016",		/* 49 n */
	ASCII,	"m",		"M",		"\r",		/* 50 m */
	ASCII,	",",		"<",		"<",		/* 51 , */
	ASCII,	".",		">",		">",		/* 52 . */
	ASCII,	"/",		"?",		"\037",		/* 53 / */
	SHIFT,	"",		"",		"",		/* 54 shift */
	KP,	"*",		"*",		"*",		/* 55 kp * */
	ALT,	"",		"",		"",		/* 56 alt */
	ASCII,	" ",		" ",		"\000",		/* 57 space */
	CAPS,	"",		"",		"",		/* 58 caps */
	FUNC,	"\033[M",	"\033[Y",	"\033[k",	/* 59 f1 */
	FUNC,	"\033[N",	"\033[Z",	"\033[l",	/* 60 f2 */
	FUNC,	"\033[O",	"\033[a",	"\033[m",	/* 61 f3 */
	FUNC,	"\033[P",	"\033[b",	"\033[n",	/* 62 f4 */
	FUNC,	"\033[Q",	"\033[c",	"\033[o",	/* 63 f5 */
	FUNC,	"\033[R",	"\033[d",	"\033[p",	/* 64 f6 */
	FUNC,	"\033[S",	"\033[e",	"\033[q",	/* 65 f7 */
	FUNC,	"\033[T",	"\033[f",	"\033[r",	/* 66 f8 */
	FUNC,	"\033[U",	"\033[g",	"\033[s",	/* 67 f9 */
	FUNC,	"\033[V",	"\033[h",	"\033[t",	/* 68 f10 */
	NUM,	"",		"",		"",		/* 69 num lock */
	SCROLL,	"",		"",		"",		/* 70 scroll lock */
	KP,	"7",		"\033[H",	"7",		/* 71 kp 7 */
	KP,	"8",		"\033[A",	"8",		/* 72 kp 8 */
	KP,	"9",		"\033[I",	"9",		/* 73 kp 9 */
	KP,	"-",		"-",		"-",		/* 74 kp - */
	KP,	"4",		"\033[D",	"4",		/* 75 kp 4 */
	KP,	"5",		"\033[E",	"5",		/* 76 kp 5 */
	KP,	"6",		"\033[C",	"6",		/* 77 kp 6 */
	KP,	"+",		"+",		"+",		/* 78 kp + */
	KP,	"1",		"\033[F",	"1",		/* 79 kp 1 */
	KP,	"2",		"\033[B",	"2",		/* 80 kp 2 */
	KP,	"3",		"\033[G",	"3",		/* 81 kp 3 */
	KP,	"0",		"\033[L",	"0",		/* 82 kp 0 */
	KP,	".",		"\177",		".",		/* 83 kp . */
	NONE,	"",		"",		"",		/* 84 0 */
	NONE,	"100",		"",		"",		/* 85 0 */
	NONE,	"101",		"",		"",		/* 86 0 */
	FUNC,	"\033[W",	"\033[i",	"\033[u",	/* 87 f11 */
	FUNC,	"\033[X",	"\033[j",	"\033[v",	/* 88 f12 */
	NONE,	"102",		"",		"",		/* 89 0 */
	NONE,	"103",		"",		"",		/* 90 0 */
	NONE,	"",		"",		"",		/* 91 0 */
	NONE,	"",		"",		"",		/* 92 0 */
	NONE,	"",		"",		"",		/* 93 0 */
	NONE,	"",		"",		"",		/* 94 0 */
	NONE,	"",		"",		"",		/* 95 0 */
	NONE,	"",		"",		"",		/* 96 0 */
	NONE,	"",		"",		"",		/* 97 0 */
	NONE,	"",		"",		"",		/* 98 0 */
	NONE,	"",		"",		"",		/* 99 0 */
	NONE,	"",		"",		"",		/* 100 */
	NONE,	"",		"",		"",		/* 101 */
	NONE,	"",		"",		"",		/* 102 */
	NONE,	"",		"",		"",		/* 103 */
	NONE,	"",		"",		"",		/* 104 */
	NONE,	"",		"",		"",		/* 105 */
	NONE,	"",		"",		"",		/* 106 */
	NONE,	"",		"",		"",		/* 107 */
	NONE,	"",		"",		"",		/* 108 */
	NONE,	"",		"",		"",		/* 109 */
	NONE,	"",		"",		"",		/* 110 */
	NONE,	"",		"",		"",		/* 111 */
	NONE,	"",		"",		"",		/* 112 */
	NONE,	"",		"",		"",		/* 113 */
	NONE,	"",		"",		"",		/* 114 */
	NONE,	"",		"",		"",		/* 115 */
	NONE,	"",		"",		"",		/* 116 */
	NONE,	"",		"",		"",		/* 117 */
	NONE,	"",		"",		"",		/* 118 */
	NONE,	"",		"",		"",		/* 119 */
	NONE,	"",		"",		"",		/* 120 */
	NONE,	"",		"",		"",		/* 121 */
	NONE,	"",		"",		"",		/* 122 */
	NONE,	"",		"",		"",		/* 123 */
	NONE,	"",		"",		"",		/* 124 */
	NONE,	"",		"",		"",		/* 125 */
	NONE,	"",		"",		"",		/* 126 */
	NONE,	"",		"",		"",		/* 127 */
};

/*
 * Get characters from the keyboard.  If none are present, return NULL.
 */
char *
sget()
{
	u_char dt;
	static u_char extended = 0, shift_state = 0;
	static u_char capchar[2];

top:
	KBD_DELAY;
	dt = inb(KBDATAP);

	switch (dt) {
	case KBR_ACK:
		ack = 1;
		goto loop;
	case KBR_RESEND:
		nak = 1;
		goto loop;
	}

	if (pc_xmode > 0) {
#if defined(DDB) && defined(XSERVER_DDB)
		/* F12 enters the debugger while in X mode */
		if (dt == 88)
			Debugger();
#endif
		capchar[0] = dt;
		capchar[1] = 0;
		/*
		 * Check for locking keys.
		 *
		 * XXX Setting the LEDs this way is a bit bogus.  What if the
		 * keyboard has been remapped in X?
		 */
		switch (scan_codes[dt & 0x7f].type) {
		case NUM:
			if (dt & 0x80) {
				shift_state &= ~NUM;
				break;
			}
			if (shift_state & NUM)
				break;
			shift_state |= NUM;
			lock_state ^= NUM;
			async_update();
			break;
		case CAPS:
			if (dt & 0x80) {
				shift_state &= ~CAPS;
				break;
			}
			if (shift_state & CAPS)
				break;
			shift_state |= CAPS;
			lock_state ^= CAPS;
			async_update();
			break;
		case SCROLL:
			if (dt & 0x80) {
				shift_state &= ~SCROLL;
				break;
			}
			if (shift_state & SCROLL)
				break;
			shift_state |= SCROLL;
			lock_state ^= SCROLL;
			if ((lock_state & SCROLL) == 0)
				wakeup((caddr_t)&lock_state);
			async_update();
			break;
		}
		return capchar;
	}

	switch (dt) {
	case KBR_EXTENDED:
		extended = 1;
		goto loop;
	}

#ifdef DDB
	/*
	 * Check for cntl-alt-esc.
	 */
	if ((dt == 1) && (shift_state & (CTL | ALT)) == (CTL | ALT)) {
		Debugger();
		dt |= 0x80;	/* discard esc (ddb discarded ctl-alt) */
	}
#endif

	/*
	 * Check for make/break.
	 */
	if (dt & 0x80) {
		/*
		 * break
		 */
		dt &= 0x7f;
		switch (scan_codes[dt].type) {
		case NUM:
			shift_state &= ~NUM;
			break;
		case CAPS:
			shift_state &= ~CAPS;
			break;
		case SCROLL:
			shift_state &= ~SCROLL;
			break;
		case SHIFT:
			shift_state &= ~SHIFT;
			break;
		case ALT:
			shift_state &= ~ALT;
			break;
		case CTL:
			shift_state &= ~CTL;
			break;
		}
	} else {
		/*
		 * make
		 */
		switch (scan_codes[dt].type) {
		/*
		 * locking keys
		 */
		case NUM:
			if (shift_state & NUM)
				break;
			shift_state |= NUM;
			lock_state ^= NUM;
			async_update();
			break;
		case CAPS:
			if (shift_state & CAPS)
				break;
			shift_state |= CAPS;
			lock_state ^= CAPS;
			async_update();
			break;
		case SCROLL:
			if (shift_state & SCROLL)
				break;
			shift_state |= SCROLL;
			lock_state ^= SCROLL;
			if ((lock_state & SCROLL) == 0)
				wakeup((caddr_t)&lock_state);
			async_update();
			break;
		/*
		 * non-locking keys
		 */
		case SHIFT:
			shift_state |= SHIFT;
			break;
		case ALT:
			shift_state |= ALT;
			break;
		case CTL:
			shift_state |= CTL;
			break;
		case ASCII:
			/* control has highest priority */
			if (shift_state & CTL)
				capchar[0] = scan_codes[dt].ctl[0];
			else if (shift_state & SHIFT)
				capchar[0] = scan_codes[dt].shift[0];
			else
				capchar[0] = scan_codes[dt].unshift[0];
			if ((lock_state & CAPS) && capchar[0] >= 'a' &&
			    capchar[0] <= 'z') {
				capchar[0] -= ('a' - 'A');
			}
			capchar[0] |= (shift_state & ALT);
			extended = 0;
			return capchar;
		case NONE:
			break;
		case FUNC: {
			char *more_chars;
			if (shift_state & SHIFT)
				more_chars = scan_codes[dt].shift;
			else if (shift_state & CTL)
				more_chars = scan_codes[dt].ctl;
			else
				more_chars = scan_codes[dt].unshift;
			extended = 0;
			return more_chars;
		}
		case KP: {
			char *more_chars;
			if (shift_state & (SHIFT | CTL) ||
			    (lock_state & NUM) == 0 || extended)
				more_chars = scan_codes[dt].shift;
			else
				more_chars = scan_codes[dt].unshift;
			extended = 0;
			return more_chars;
		}
		}
	}

	extended = 0;
loop:
	if ((inb(KBSTATP) & KBS_DIB) == 0)
		return 0;
	goto top;
}

int
pcmmap(dev, offset, nprot)
	dev_t dev;
	int offset;
	int nprot;
{

	if (offset > 0x20000)
		return -1;
	return i386_btop(0xa0000 + offset);
}

pc_xmode_on()
{
	struct trapframe *fp;

	if (pc_xmode)
		return;
	pc_xmode = 1;

#ifdef XFREE86_BUG_COMPAT
	/* If still unchanged, get current shape. */
	if (cursor_shape == 0xffff)
		get_cursor_shape();
#endif

#ifdef COMPAT_10
	/* This is done by i386_iopl(3) now. */
	fp = curproc->p_md.md_regs;
	fp->tf_eflags |= PSL_IOPL;
#endif
}

pc_xmode_off()
{
	struct trapframe *fp;

	if (pc_xmode == 0)
		return;
	pc_xmode = 0;

#ifdef XFREE86_BUG_COMPAT
	/* XXX It would be hard to justify why the X server doesn't do this. */
	set_cursor_shape();
#endif
	async_update();

	fp = curproc->p_md.md_regs;
	fp->tf_eflags &= ~PSL_IOPL;
}
