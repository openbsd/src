/*	$NetBSD: ite.c,v 1.16 1995/07/17 01:24:34 briggs Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: ite.c 1.28 92/12/20$
 *
 *	@(#)ite.c	8.2 (Berkeley) 1/12/94
 */

/*
 * ite.c
 *
 * The ite module handles the system console; that is, stuff printed
 * by the kernel and by user programs while "desktop" and X aren't
 * running.  Some (very small) parts are based on hp300's 4.4 ite.c,
 * hence the above copyright.
 *
 *   -- Brad and Lawrence, June 26th, 1994
 *
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <dev/cons.h>

#include "../mac68k/via.h"
#include <machine/cpu.h>
#include <machine/frame.h>

#define KEYBOARD_ARRAY
#include <machine/keyboard.h>
#include <machine/adbsys.h>
#include <machine/iteioctl.h>

#include "6x10.h"
#define CHARWIDTH	6
#define CHARHEIGHT	10

#define dprintf if (0) printf

#define ATTR_NONE	0
#define ATTR_BOLD	1
#define ATTR_UNDER	2
#define ATTR_REVERSE	4

enum vt100state_e {
	ESnormal,		/* Nothing yet                             */
	ESesc,			/* Got ESC                                 */
	ESsquare,		/* Got ESC [				   */
	ESgetpars,		/* About to get or getting the parameters  */
	ESgotpars,		/* Finished getting the parameters         */
	ESfunckey,		/* Function key                            */
	EShash,			/* DEC-specific stuff (screen align, etc.) */
	ESsetG0,		/* Specify the G0 character set            */
	ESsetG1,		/* Specify the G1 character set            */
	ESignore		/* Ignore this sequence                    */
} vt100state = ESnormal;

/* Received from MacBSDBoot, stored by Locore: */
long    videoaddr;
long    videorowbytes;
long    videobitdepth;
unsigned long videosize;

/* Calculated in itecninit(): */
static int width, height, scrrows, scrcols;
static void (*putpixel) (int x, int y, int *c, int num);
static void (*reversepixel) (int x, int y, int num);

/* VT100 state: */
#define MAXPARS	16
static int x = 0, y = 0, savex, savey;
static int par[MAXPARS], numpars, hanging_cursor, attr;

/* Our tty: */
struct tty *ite_tty;

/* For polled ADB mode: */
static int polledkey;
extern int adb_polling;

/* Misc */
void    itestart();
static void ite_putchar(char ch);

/* VT100 tab stops & scroll region */
static char tab_stops[255];
static int  scrreg_top, scrreg_bottom;

/*
 * Bitmap handling functions
 */

static inline void 
putpixel1(int xx, int yy, int *c, int num)
{
	unsigned int i, mask;
	unsigned char *sc;

	sc = (unsigned char *) videoaddr;

	i = 7 - (xx & 7);
	mask = ~(1 << i);
	sc += yy * videorowbytes + (xx >> 3);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 1) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel2(int xx, int yy, int *c, int num)
{
	unsigned int i, mask;
	unsigned char *sc;

	sc = (unsigned char *) videoaddr;

	i = 6 - ((xx & 3) << 1);
	mask = ~(3 << i);
	sc += yy * videorowbytes + (xx >> 2);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 3) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel4(int xx, int yy, int *c, int num)
{
	unsigned int i, mask;
	unsigned char *sc;

	sc = (unsigned char *) videoaddr;

	i = 4 - ((xx & 1) << 2);
	mask = ~(15 << i);
	sc += yy * videorowbytes + (xx >> 1);
	while (num--) {
		*sc &= mask;
		*sc |= (*c++ & 15) << i;
		sc += videorowbytes;
	}
}

static void 
putpixel8(int xx, int yy, int *c, int num)
{
	unsigned char *sc;

	sc = (unsigned char *) videoaddr;

	sc += yy * videorowbytes + xx;
	while (num--) {
		*sc = *c++ & 0xff;
		sc += videorowbytes;
	}
}

static void 
reversepixel1(int xx, int yy, int num)
{
	unsigned int mask;
	unsigned char *sc;

	sc = (unsigned char *) videoaddr;
	mask = 0;		/* Get rid of warning from compiler */

	switch (videobitdepth) {
	case 1:
		mask = 1 << (7 - (xx & 7));
		sc += yy * videorowbytes + (xx >> 3);
		break;
	case 2:
		mask = 3 << (6 - ((xx & 3) << 1));
		sc += yy * videorowbytes + (xx >> 2);
		break;
	case 4:
		mask = 15 << (4 - ((xx & 1) << 2));
		sc += yy * videorowbytes + (xx >> 1);
		break;
	case 8:
		mask = 255;
		sc += yy * videorowbytes + xx;
		break;
	default:
		panic("reversepixel(): unsupported bit depth");
	}

	while (num--) {
		*sc ^= mask;
		sc += videorowbytes;
	}
}

static void 
writechar(char ch, int x, int y, int attr)
{
	int     i, j, mask, rev, col[CHARHEIGHT];
	unsigned char *c;

	ch &= 0x7F;
	x *= CHARWIDTH;
	y *= CHARHEIGHT;

	rev = (attr & ATTR_REVERSE) ? 255 : 0;

	c = &Font6x10[ch * CHARHEIGHT];

	switch (videobitdepth) {
	case 1:
		for (j = 0; j < CHARWIDTH; j++) {
			mask = 1 << (CHARWIDTH - 1 - j);
			for (i = 0; i < CHARHEIGHT; i++) {
				col[i] = ((c[i] & mask) ? 255 : 0) ^ rev;
			}
			putpixel1(x + j, y, col, CHARHEIGHT);
		}
		if (attr & ATTR_UNDER) {
			col[0] = 255;
			for (j = 0; j < CHARWIDTH; j++) {
				putpixel1(x + j, y + CHARHEIGHT - 1, col, 1);
			}
		}
		break;
	case 2:
	case 4:
	case 8:
		for (j = 0; j < CHARWIDTH; j++) {
			mask = 1 << (CHARWIDTH - 1 - j);
			for (i = 0; i < CHARHEIGHT; i++) {
				col[i] = ((c[i] & mask) ? 255 : 0) ^ rev;
			}
			putpixel(x + j, y, col, CHARHEIGHT);
		}
		if (attr & ATTR_UNDER) {
			col[0] = 255;
			for (j = 0; j < CHARWIDTH; j++) {
				putpixel(x + j, y + CHARHEIGHT - 1, col, 1);
			}
		}
		break;
	}
}

static void 
drawcursor(void)
{
	int     j, X, Y;

	X = x * CHARWIDTH;
	Y = y * CHARHEIGHT;

	for (j = 0; j < CHARWIDTH; j++) {
		reversepixel(X + j, Y, CHARHEIGHT);
	}
}

static void 
erasecursor(void)
{
	int     j, X, Y;

	X = x * CHARWIDTH;
	Y = y * CHARHEIGHT;

	for (j = 0; j < CHARWIDTH; j++) {
		reversepixel(X + j, Y, CHARHEIGHT);
	}
}

static void 
scrollup(void)
{
	unsigned long *from, *to;
	int     i, linelongs, tocopy, copying;
	linelongs = videorowbytes * CHARHEIGHT / 4;

	to = (unsigned long *) videoaddr + ((scrreg_top-1) * linelongs);
	from = to + linelongs;

	tocopy = (scrreg_bottom - scrreg_top) * linelongs;
	while (tocopy > 0) {
		copying = (tocopy > 16383) ? 16383 : tocopy;
		bcopy(from, to, copying * 4);
		from += copying;
		to += copying;
		tocopy -= copying;
	}
	to = (unsigned long *) videoaddr;
	bzero(to + (scrreg_bottom - 1) * linelongs, linelongs * sizeof(long));
}

static void 
scrolldown(void)
{
	unsigned long *from, *to;
	int     i, linelongs;
	linelongs = videorowbytes * CHARHEIGHT / 4;

	to = (unsigned long *) videoaddr + linelongs * (scrreg_bottom);
	from = to - linelongs;

	for (i = (scrreg_bottom - scrreg_top) * linelongs; i > 0; i--) {
		*--to = *--from;
	}
	for (i = linelongs; i > 0; i--) {
		*--to = 0;
	}
}

static void 
clear_screen(int which)
{
	unsigned long *p;
	int     i, linelongs;

	p = (unsigned long *) videoaddr;
	linelongs = videorowbytes * CHARHEIGHT / 4;

	switch (which) {
	case 0:		/* To end of screen	 */
		p += y * linelongs;
		i = (scrrows - y) * linelongs;
		break;
	case 1:		/* To start of screen	 */
		i = y * linelongs;
		break;
	case 2:		/* Whole screen		 */
		i = scrrows * linelongs;
		break;
	}

	bzero(p, i * sizeof(long));
}

static void 
clear_line(int which)
{
	int     start, end, i;

	/*
	 * This routine runs extremely slowly.  I don't think it's
	 * used all that often, except for To end of line.  I'll go
	 * back and speed this up when I speed up the whole ite
	 * module. --LK
	 */

	switch (which) {
	case 0:		/* To end of line	 */
		start = x;
		end = scrcols;
		break;
	case 1:		/* To start of line	 */
		start = 0;
		end = x;
		break;
	case 2:		/* Whole line		 */
		start = 0;
		end = scrcols;
		break;
	}

	for (i = start; i < end; i++) {
		writechar(' ', i, y, ATTR_NONE);
	}
}
static void
reset_tabs(void)
{
	int i;

	for (i = 0; i<= scrcols; i++) {
		tab_stops[i] = ((i % 8) == 0);
	}
}

static void
vt100_reset(void)
{
	reset_tabs;
	scrreg_top    = 1;
	scrreg_bottom = scrrows;
	attr = ATTR_NONE;
}

static void 
putc_normal(char ch)
{
	switch (ch) {
	case '\a':		/* Beep			 */
		asc_ringbell();
		break;
	case 127:		/* Delete		 */
	case '\b':		/* Backspace		 */
		if (hanging_cursor) {
			hanging_cursor = 0;
		} else
			if (x > 0) {
				x--;
			}
		break;
	case '\t':		/* Tab			 */
		do {
			ite_putchar(' ');
		} while (tab_stops[x] = 0);
		break;
	case '\n':		/* Line feed		 */
		if (y == scrreg_bottom - 1) {
			scrollup();
		} else {
			y++;
		}
		break;
	case '\r':		/* Carriage return	 */
		x = 0;
		hanging_cursor = 0;
		break;
	case '\e':		/* Escape		 */
		vt100state = ESesc;
		hanging_cursor = 0;
		break;
	default:
		if (ch >= ' ') {
			if (hanging_cursor) {
				x = 0;
				if (y == scrreg_bottom - 1) {
					scrollup();
				} else {
					y++;
				}
				hanging_cursor = 0;
			}
			writechar(ch, x, y, attr);
			if (x == scrcols - 1) {
				hanging_cursor = 1;
			} else {
				x++;
			}
			if (x >= scrcols) {	/* can we ever get here? */
				x = 0;
				y++;
			}
		}
		break;
	}
}

static void 
putc_esc(char ch)
{
	vt100state = ESnormal;

	switch (ch) {
	case '[':
		vt100state = ESsquare;
		break;
	case 'D':		/* Line feed		 */
		if (y == scrreg_bottom - 1) {
			scrollup();
		} else {
			y++;
		}
		break;
	case 'H':		/* Set tab stop		 */
		tab_stops[x] = 1;
		break;
	case 'M':		/* Cursor up		 */
		if (y == scrreg_top - 1) {
			scrolldown();
		} else {
			y--;
		}
		break;
	case '>':
		vt100_reset();
		break;
	case '7':		/* Save cursor		 */
		savex = x;
		savey = y;
		break;
	case '8':		/* Restore cursor	 */
		x = savex;
		y = savey;
		break;
	default:
		/* Rest not supported */
		break;
	}
}

static void 
putc_gotpars(char ch)
{
	int     i;

	vt100state = ESnormal;
	switch (ch) {
	case 'A':		/* Up			 */
		i = par[0];
		do {
			if (y == scrreg_top - 1) {
				scrolldown();
			} else {
				y--;
			};
			i--;
		} while (i > 0);
		break;
	case 'B':		/* Down			 */
		i = par[0];
		do {
			if (y == scrreg_bottom - 1) {
				scrollup();
			} else {
				y++;
			};
			i--;
		} while (i > 0);
		break;
	case 'C':		/* Right		 */
		x+= par[0] ? par[0] : 1;
		break;
	case 'D':		/* Left			 */
		x-= par[0] ? par[0] : 1;
		break;
	case 'H':		/* Set cursor position	 */
		x = par[1] - 1;
		y = par[0] - 1;
		hanging_cursor = 0;
		break;
	case 'J':		/* Clear part of screen	 */
		clear_screen(par[0]);
		break;
	case 'K':		/* Clear part of line	 */
		clear_line(par[0]);
		break;
	case 'g':		/* Clear tab stops	 */
		if (numpars >= 1 && par[0] == 3) {
			reset_tabs();
		}
		break;
	case 'm':		/* Set attribute	 */
		for (i = 0; i < numpars; i++) {
			switch (par[i]) {
			case 0:
				attr = ATTR_NONE;
				break;
			case 1:
				attr |= ATTR_BOLD;
				break;
			case 4:
				attr |= ATTR_UNDER;
				break;
			case 7:
				attr |= ATTR_REVERSE;
				break;
			}
		}
		break;
	case 'r':		/* Set scroll region	 */
		/* ensure top < bottom, and both within limits */
		if ((numpars > 0) && (par[0] < scrrows)) {
			scrreg_top = par[0];
		} else {
			scrreg_top = 1;
		}
		if ((numpars > 1) && (par[1] <= scrrows) && (par[1] > par[0])) {
			scrreg_bottom = par[1];
		} else {
			scrreg_bottom = scrrows;
		}
		break;
	}
}

static void 
putc_getpars(char ch)
{
	if (ch == '?') {
		/* Not supported */
		return;
	}
	if (ch == '[') {
		vt100state = ESnormal;
		/* Not supported */
		return;
	}
	if (ch == ';' && numpars < MAXPARS - 1) {
		numpars++;
	} else
		if (ch >= '0' && ch <= '9') {
			par[numpars] *= 10;
			par[numpars] += ch - '0';
		} else {
			numpars++;
			vt100state = ESgotpars;
			putc_gotpars(ch);
		}
}

static void 
putc_square(char ch)
{
	int     i;

	for (i = 0; i < MAXPARS; i++) {
		par[i] = 0;
	}

	numpars = 0;
	vt100state = ESgetpars;

	putc_getpars(ch);
}

static void 
ite_putchar(char ch)
{
	switch (vt100state) {
		default:vt100state = ESnormal;	/* FALLTHROUGH */
	case ESnormal:
		putc_normal(ch);
		break;
	case ESesc:
		putc_esc(ch);
		break;
	case ESsquare:
		putc_square(ch);
		break;
	case ESgetpars:
		putc_getpars(ch);
		break;
	case ESgotpars:
		putc_gotpars(ch);
		break;
	}

	if (x >= scrcols) {
		x = scrcols - 1;
	}
	if (x < 0) {
		x = 0;
	}
	if (y >= scrrows) {
		y = scrrows - 1;
	}
	if (y < 0) {
		y = 0;
	}
}
/*
 * Keyboard support functions
 */

static int 
ite_dopollkey(int key)
{
	polledkey = key;

	return 0;
}


static int 
ite_pollforchar(void)
{
	int     s;
	register int intbits;

	s = splhigh();

	polledkey = -1;
	adb_polling = 1;

	/* pretend we're VIA interrupt dispatcher */
	while (polledkey == -1) {
		intbits = via_reg(VIA1, vIFR);

		if (intbits & V1IF_ADBRDY) {
			mrg_adbintr();
			via_reg(VIA1, vIFR) = V1IF_ADBRDY;
		}
		if (intbits & 0x10) {
			mrg_pmintr();
			via_reg(VIA1, vIFR) = 0x10;
		}
	}

	adb_polling = 0;

	splx(s);

	return polledkey;
}
/*
 * Tty handling functions
 */

static void 
iteattach(struct device * parent, struct device * dev, void *aux)
{
	printf(" (minimal console)\n");
}

extern int matchbyname();

struct cfdriver itecd = {
	NULL, "ite", matchbyname, iteattach, DV_TTY, sizeof(struct device)
};

int
iteopen(dev_t dev, int mode, int devtype, struct proc * p)
{
	register struct tty *tp;
	register int error;
	int     first = 0;

	dprintf("iteopen(): enter(0x%x)\n", (int) dev);
	if (ite_tty == NULL)
		tp = ite_tty = ttymalloc();
	else
		tp = ite_tty;
	if ((tp->t_state & (TS_ISOPEN | TS_XCLUDE)) == (TS_ISOPEN | TS_XCLUDE)
	    && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	tp->t_oproc = itestart;
	tp->t_param = NULL;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = CS8 | CREAD;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		tp->t_state = TS_ISOPEN | TS_CARR_ON;
		ttsetwater(tp);
	}
	error = (*linesw[tp->t_line].l_open) (dev, tp);
	tp->t_winsize.ws_row = scrrows;
	tp->t_winsize.ws_col = scrcols;
	dprintf("iteopen(): exit(%d)\n", error);
	return (error);
}

int
iteclose(dev_t dev, int flag, int mode, struct proc * p)
{
	dprintf("iteclose: enter (%d)\n", (int) dev);
	(*linesw[ite_tty->t_line].l_close) (ite_tty, flag);
	ttyclose(ite_tty);
#if 0
	ttyfree(ite_tty);
	ite_tty = (struct tty *) 0;
#endif
	dprintf("iteclose: exit\n");
	return 0;
}

int
iteread(dev_t dev, struct uio * uio, int flag)
{
	dprintf("iteread: enter\n");
	return (*linesw[ite_tty->t_line].l_read) (ite_tty, uio, flag);
	dprintf("iteread: exit\n");
}

int
itewrite(dev_t dev, struct uio * uio, int flag)
{
	dprintf("itewrite: enter\n");
	return (*linesw[ite_tty->t_line].l_write) (ite_tty, uio, flag);
	dprintf("itewrite: exit\n");
}

struct tty *
itetty(dev)
	dev_t   dev;
{
	return (ite_tty);
}

int
iteioctl(dev_t dev, int cmd, caddr_t addr, int flag, struct proc * p)
{
	register struct tty *tp = ite_tty;
	int     error;

	dprintf("iteioctl: enter(%d, 0x%x)\n", cmd, cmd);
	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, addr, flag, p);
	if (error >= 0) {
		dprintf("iteioctl: exit(%d)\n", error);
		return (error);
	}
	error = ttioctl(tp, cmd, addr, flag, p);
	if (error >= 0) {
		dprintf("iteioctl: exit(%d)\n", error);
		return (error);
	}
	switch (cmd) {
	case ITEIOC_RINGBELL:{
			asc_ringbell();
			return (0);
		}
	case ITEIOC_SETBELL:{
			struct bellparams *bp = (void *) addr;

			asc_setbellparams(bp->freq, bp->len, bp->vol);
			return (0);
		}
	case ITEIOC_GETBELL:{
			struct bellparams *bp = (void *) addr;

			asc_getbellparams(&bp->freq, &bp->len, &bp->vol);
			return (0);
		}
	}
	dprintf("iteioctl: exit(ENOTTY)\n");
	return (ENOTTY);
}

void 
itestart(register struct tty * tp)
{
	register int cc, s;
	int     hiwat = 0, hadcursor = 0;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;

	cc = tp->t_outq.c_cc;
	splx(s);
	erasecursor();
	while (cc-- > 0) {
		ite_putchar(getc(&tp->t_outq));
	}
	drawcursor();

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	splx(s);
}

void 
itestop(struct tty * tp, int flag)
{
	int     s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0) {
			tp->t_state |= TS_FLUSH;
		}
	}
	splx(s);
}

int
ite_intr(adb_event_t * event)
{
	static int shift = 0, control = 0;
	int     key, press, val, state;
	char    str[10], *s;

	key = event->u.k.key;
	press = ADBK_PRESS(key);
	val = ADBK_KEYVAL(key);

	if (val == ADBK_SHIFT) {
		shift = press;
	} else
		if (val == ADBK_CONTROL) {
			control = press;
		} else
			if (press) {
				switch (val) {
				case ADBK_UP:
					str[0] = '\e';
					str[1] = 'O';
					str[2] = 'A';
					str[3] = '\0';
					break;
				case ADBK_DOWN:
					str[0] = '\e';
					str[1] = 'O';
					str[2] = 'B';
					str[3] = '\0';
					break;
				case ADBK_RIGHT:
					str[0] = '\e';
					str[1] = 'O';
					str[2] = 'C';
					str[3] = '\0';
					break;
				case ADBK_LEFT:
					str[0] = '\e';
					str[1] = 'O';
					str[2] = 'D';
					str[3] = '\0';
					break;
				default:
					state = 0;
					if (shift) {
						state = 1;
					}
					if (control) {
						state = 2;
					}
					str[0] = keyboard[val][state];
					str[1] = '\0';
					break;
				}
				if (adb_polling) {
					polledkey = str[0];
				} else {
					for (s = str; *s; s++) {
						(*linesw[ite_tty->t_line].l_rint) (*s, ite_tty);
					}
				}
			}
}
/*
 * Console functions
 */

int
itecnprobe(struct consdev * cp)
{
	int     maj, unit;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++) {
		if (cdevsw[maj].d_open == iteopen) {
			break;
		}
	}

	if (maj == nchrdev) {
		panic("itecnprobe(): did not find iteopen().");
	}
	unit = 0;		/* hardcode first device as console. */

	/* initialize required fields */
	cp->cn_dev = makedev(maj, unit);
	cp->cn_pri = CN_INTERNAL;
}

int
itecninit(struct consdev * cp)
{
	width = videosize & 0xffff;
	height = (videosize >> 16) & 0xffff;
	scrrows = height / CHARHEIGHT;
	scrcols = width / CHARWIDTH;

	vt100_reset();

	switch (videobitdepth) {
	default:
	case 1:
		putpixel = putpixel2;
		reversepixel = reversepixel1;
		break;
	case 2:
		putpixel = putpixel2;
		reversepixel = reversepixel1;
		break;
	case 4:
		putpixel = putpixel4;
		reversepixel = reversepixel1;
		break;
	case 8:
		putpixel = putpixel8;
		reversepixel = reversepixel1;
		break;
	}

	iteon(cp->cn_dev, 0);
}

int
iteon(dev_t dev, int flags)
{
	erasecursor();
	clear_screen(2);
	drawcursor();
}

int
iteoff(dev_t dev, int flags)
{
	erasecursor();
	clear_screen(2);
}

int
itecngetc(dev_t dev)
{
	/* Oh, man... */

	return ite_pollforchar();
}

int
itecnputc(dev_t dev, int c)
{
	extern dev_t mac68k_serdev;
	int     s;

/* 	s = splhigh (); */

	erasecursor();
	ite_putchar(c);
	drawcursor();
	if (mac68k_machine.serial_boot_echo)
		sercnputc(mac68k_serdev, c);

/* 	splx (s); */
}
