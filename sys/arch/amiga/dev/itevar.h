/*	$NetBSD: itevar.h,v 1.11.2.2 1995/10/20 11:01:11 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
#if ! defined (_ITEVAR_H)
#define _ITEVAR_H

enum ite_arraymaxs {
	MAX_ARGSIZE = 256,
	MAX_TABS = 256,
};

enum ite_attr {
	ATTR_NOR = 0,
	ATTR_INV = 1,
	ATTR_UL = 2,
	ATTR_BOLD = 4,
	ATTR_BLINK = 8,
	ATTR_ALL = 16-1,

	ATTR_KEYPAD = 0x80		/* XXX */
};

struct ite_softc {
	struct	device device;
	struct	tty *tp;
	char	argbuf[MAX_ARGSIZE];
	struct  grf_softc *grf;		/* XXX */
	void	*priv;
	char	*ap;
	u_char	*tabs;
	int	flags;
	int	open_count;
	short	cursorx;
	short	cursory;
	short	rows;
	short	cols;
	u_char	*font;
	u_char	*cursor;
	u_char	font_lo;
	u_char	font_hi;
	short	cpl;			/* XXX? */
	short	ftheight;
	short	ftwidth;
	short	ftbaseline;
	short	ftboldsmear;
	short	planemask;
	short	pos;
	char	imode;
	char	fpd;
	char	hold;
	u_char	escape;
	u_char	cursor_opt;
	u_char	key_repeat;
	char	GL;
	char	GR;
	char	G0;
	char	G1;
	char	G2;
	char	G3;
	char	linefeed_newline;
	char	auto_wrap;
	char	cursor_appmode;
	char	keypad_appmode;
	short	top_margin;
	short	bottom_margin;
	short	inside_margins;
	short 	eightbit_C1;
	short	emul_level;
	enum 	ite_attr attribute;
	enum 	ite_attr save_attribute;
	int	curx;
	int	save_curx;
	int	cury;
	int	save_cury;
};

enum ite_flags {
	ITE_ALIVE  = 0x1,		/* grf layer is configed */
	ITE_ISCONS = 0x2,		/* ite is acting console. */
	ITE_INITED = 0x4,		/* ite has been inited. */
	ITE_ISOPEN = 0x8,		/* ite has been opened */
	ITE_INGRF  = 0x10,		/* ite is in graphics mode */
	ITE_ACTIVE = 0x20,		/* ite is an active terminal */
};

enum ite_replrules {
	RR_CLEAR = 0,
	RR_COPY = 0x3,
	RR_XOR = 0x6,
	RR_COYINVERTED = 0xC
};

enum ite_scrolldir {
	SCROLL_UP = 1,
	SCROLL_DOWN,
	SCROLL_LEFT,
	SCROLL_RIGHT,
};

enum ite_cursact {
	DRAW_CURSOR = 5,
	ERASE_CURSOR,
	MOVE_CURSOR,
	START_CURSOROPT,
	END_CURSOROPT
};

enum ite_special_keycodes {
	KBD_LEFT_SHIFT = 0x60,
	KBD_RIGHT_SHIFT,
	KBD_CAPS_LOCK,
	KBD_CTRL,
	KBD_LEFT_ALT,
	KBD_RIGHT_ALT,
	KBD_LEFT_META,
	KBD_RIGHT_META
};

enum ite_modifiers {
	KBD_MOD_LSHIFT = (1 << (KBD_LEFT_SHIFT - KBD_LEFT_SHIFT)),
	KBD_MOD_RSHIFT = (1 << (KBD_RIGHT_SHIFT- KBD_LEFT_SHIFT)),
	KBD_MOD_CTRL = (1 << (KBD_CTRL - KBD_LEFT_SHIFT)),
	KBD_MOD_LALT = (1 << (KBD_LEFT_ALT - KBD_LEFT_SHIFT)),
	KBD_MOD_RALT = (1 << (KBD_RIGHT_ALT- KBD_LEFT_SHIFT)),
	KBD_MOD_LMETA = (1 << (KBD_LEFT_META - KBD_LEFT_SHIFT)),
	KBD_MOD_RMETA = (1 << (KBD_RIGHT_META- KBD_LEFT_SHIFT)),
	KBD_MOD_CAPS = (1 << (KBD_CAPS_LOCK - KBD_LEFT_SHIFT)),

	KBD_MOD_SHIFT = (KBD_MOD_LSHIFT | KBD_MOD_RSHIFT),
	KBD_MOD_ALT = (KBD_MOD_LALT | KBD_MOD_RALT),
	KBD_MOD_META = (KBD_MOD_LMETA | KBD_MOD_RMETA),
};

enum caller {
	ITEFILT_TTY,
	ITEFILT_CONSOLE,
	ITEFILT_REPEATER
};

enum emul_level {
	EMUL_VT100 = 1,
	EMUL_VT300_8,
	EMUL_VT300_7
};

enum ite_max_getsize { ITEBURST = 64 };

enum tab_size { TABSIZE = 8 };
#define TABEND(u) (ite_tty[u]->t_windsize.ws_col - TABSIZE) /* XXX */

#define set_attr(ip, attr)	((ip)->attribute |= (attr))
#define clr_attr(ip, attr)	((ip)->attribute &= ~(attr))
#define attrloc(ip, y, x) 0
#define attrclr(ip, sy, sx, h, w)
#define attrmov(ip, sy, sx, dy, dx, h, w)
#define attrtest(ip, attr) 0
#define attrset(ip, attr)

struct proc;
struct consdev;
struct termios;

/* console related function */
void	itecnprobe __P((struct consdev *));
void	itecninit __P((struct consdev *));
int	itecngetc __P((dev_t));
void	itecnputc __P((dev_t, int));
void	ite_cnfinish __P((struct ite_softc *));

/* standard ite device entry points. */
void	iteinit __P((dev_t));
int	iteopen __P((dev_t, int, int, struct proc *));
int	iteclose __P((dev_t, int, int, struct proc *));
int	iteread __P((dev_t, struct uio *, int));
int	itewrite __P((dev_t, struct uio *, int));
int	iteioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
void	itestart __P((struct tty *));

/* ite functions */
int	ite_on __P((dev_t, int));
void	ite_off __P((dev_t, int));
void	ite_reinit __P((dev_t));
int	ite_param __P((struct tty *, struct termios *));
void	ite_reset __P((struct ite_softc *));
int	ite_cnfilter __P((u_char, enum caller));
void	ite_filter __P((u_char ,enum caller));

#endif /* _ITEVAR_H */
