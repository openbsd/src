/*	$OpenBSD: pckbd.c,v 1.1 1998/09/27 03:55:58 rahnds Exp $	*/
/*	$NetBSD: pckbd.c,v 1.14 1996/12/05 01:39:30 cgd Exp $	*/

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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <powerpc/isa/pckbdreg.h>
#include <powerpc/isa/spkrreg.h>
#include <powerpc/isa/timerreg.h>
#include <machine/wsconsio.h>
#include <powerpc/isa/pcppivar.h>

#include <dev/wscons/wsconsvar.h>
#include <dev/wscons/kbd.h>
#include "wscons.h"

#undef KBDATAP
#undef KBOUTP
#undef KBSTATP
#undef KBCMDP
#undef PITAUX_PORT
#define	KBDATAP		0x0	/* kbd data port (I) */
#define	KBOUTP		0x0	/* kbd data port (O) */
#define	KBSTATP		0x4	/* kbd controller status port (I) */
#define	KBCMDP		0x4	/* kbd controller port (O) */
#define	PITAUX_PORT	0x1	/* port B of PPI */

static volatile u_char ack, nak;	/* Don't ask. */
static u_char async, kernel, polling;	/* Really, you don't want to know. */
static u_char lock_state = 0x00,	/* all off */
	      old_lock_state = 0xff,
	      typematic_rate = 0xff,	/* don't update until set by user */
	      old_typematic_rate = 0xff;

bus_space_tag_t pckbd_iot;
isa_chipset_tag_t pckbd_ic;

bus_space_handle_t pckbd_ioh;
bus_space_handle_t pckbd_timer_ioh;
bus_space_handle_t pckbd_delay_ioh;

struct pckbd_softc {
        struct  device sc_dev;
        void    *sc_ih;

	int	sc_bellactive;		/* is the bell active? */
	int	sc_bellpitch;		/* last pitch programmed */
};

#ifdef __BROKEN_INDIRECT_CONFIG
int pckbdprobe __P((struct device *, void *, void *));
#else
int pckbdprobe __P((struct device *, struct cfdata *, void *));
#endif
void pckbdattach __P((struct device *, struct device *, void *));
int pckbdintr __P((void *));

struct cfattach pckbd_ca = {
	sizeof(struct pckbd_softc), pckbdprobe, pckbdattach,
};

struct cfdriver pckbd_cd = {
	NULL, "pckbd", DV_DULL,
};

int	pckbd_cngetc __P((struct device *));
void	pckbd_cnpollc __P((struct device *, int));
void	pckbd_bell __P((struct device *, struct wsconsio_bell_data *));
int	pckbd_ioctl __P((void *, u_long, caddr_t, int,
	    struct proc *));
char	*pckbd_translate __P((struct device *dev, int c));

#if NWSCONS
struct wscons_idev_spec pckbd_wscons_idev = {
	pckbd_cngetc,
	pckbd_cnpollc,
	pckbd_bell,
	pckbd_ioctl,
	pckbd_translate,
	0x7f,			/* key data mask */
	0x80,			/* key-up mask */
};
#endif

void	pckbd_bell_stop __P((void *));
static __inline int kbd_wait_output __P((void));
static __inline int kbd_wait_input __P((void));
static __inline void kbd_flush_input __P((void));
u_char	kbc_get8042cmd __P((void));
int	kbc_put8042cmd __P((u_char));
int	kbd_cmd __P((u_char, u_char));
void	do_async_update __P((void *));
void	async_update __P((void));

int kbd_cmd __P((u_char, u_char));
void do_async_update __P((void *));
void async_update __P((void));

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

#define	KBD_DELAY \
	do { \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
		bus_space_read_1(pckbd_iot, pckbd_delay_ioh, 0); \
	} while(0)

static __inline int
kbd_wait_output()
{
	u_int i;

	for (i = 100000; i; i--)
		if ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_IBF)
		    == 0) {
			KBD_DELAY;
			return 1;
		}
	return 0;
}

static __inline int
kbd_wait_input()
{
	u_int i;

	for (i = 100000; i; i--)
		if ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_DIB)
		    != 0) {
			KBD_DELAY;
			return 1;
		}
	return 0;
}

static __inline void
kbd_flush_input()
{
	u_int i;

	for (i = 10; i; i--) {
		if ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_DIB)
		    == 0)
			return;
		KBD_DELAY;
		(void) bus_space_read_1(pckbd_iot, pckbd_ioh, KBDATAP);
	}
}

#if 1
/*
 * Get the current command byte.
 */
u_char
kbc_get8042cmd()
{

	if (!kbd_wait_output())
		return -1;
	bus_space_write_1(pckbd_iot, pckbd_ioh, KBCMDP, K_RDCMDBYTE);
	if (!kbd_wait_input())
		return -1;
	return bus_space_read_1(pckbd_iot, pckbd_ioh, KBDATAP);
}
#endif

/*
 * Pass command byte to keyboard controller (8042).
 */
int
kbc_put8042cmd(val)
	u_char val;
{

	if (!kbd_wait_output())
		return 0;
	bus_space_write_1(pckbd_iot, pckbd_ioh, KBCMDP, K_LDCMDBYTE);
	if (!kbd_wait_output())
		return 0;
	bus_space_write_1(pckbd_iot, pckbd_ioh, KBOUTP, val);
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
		bus_space_write_1(pckbd_iot, pckbd_ioh, KBOUTP, val);
		if (polling)
			for (i = 100000; i; i--) {
				if (bus_space_read_1(pckbd_iot,
				    pckbd_ioh, KBSTATP) & KBS_DIB) {
					register u_char c;

					KBD_DELAY;
					c = bus_space_read_1(pckbd_iot,
					    pckbd_ioh, KBDATAP);
					if (c == KBR_ACK || c == KBR_ECHO) {
						ack = 1;
						return 1;
					}
					if (c == KBR_RESEND) {
						nak = 1;
						break;
					}
#ifdef DIAGNOSTIC
					printf("kbd_cmd: input char %x lost\n",
					    c);
#endif
				}
			}
		else
			for (i = 100000; i; i--) {
				(void) bus_space_read_1(pckbd_iot,
				    pckbd_ioh, KBSTATP);
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

/*
 * these are both bad jokes
 */
int
pckbdprobe(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pcppi_attach_args *pa = aux;
	u_int i, rv;

	if (pa->pa_slot != PCPPI_KBD_SLOT)
		return 0;

	rv = 0;

	pckbd_iot = pa->pa_iot;
	pckbd_ic = pa->pa_ic;
	pckbd_ioh = pa->pa_ioh;
	pckbd_timer_ioh = pa->pa_pit_ioh;
	pckbd_delay_ioh = pa->pa_delaybah;

	/* Enable interrupts and keyboard, etc. */
	if (!kbc_put8042cmd(CMDBYTE)) {
		printf("pcprobe: command error\n");
		goto lose;
	}

	rv = 1;			/* from here one out, we let it succeed */
#if 1
	/* Flush any garbage. */
	kbd_flush_input();
	/* Reset the keyboard. */
	if (!kbd_cmd(KBC_RESET, 1)) {
		printf("pcprobe: reset error %d\n", 1);
		goto lose;
	}
	for (i = 600000; i; i--)
		if ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_DIB)
		    != 0) {
			KBD_DELAY;
			break;
		}
	if (i == 0 || bus_space_read_1(pckbd_iot, pckbd_ioh, KBDATAP)
	    != KBR_RSTDONE) {
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

	return rv;
}

void
pckbdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pckbd_softc *sc = (void *)self;
	struct pcppi_attach_args *pa = aux;

	pckbd_iot = pa->pa_iot;
	pckbd_ic = pa->pa_ic;
	pckbd_ioh = pa->pa_ioh;
	pckbd_timer_ioh = pa->pa_pit_ioh;
	pckbd_delay_ioh = pa->pa_delaybah;

	sc->sc_ih = isa_intr_establish(pckbd_ic, 1, IST_EDGE, IPL_TTY,
	    pckbdintr, sc, sc->sc_dev.dv_xname);

	sc->sc_bellactive = sc->sc_bellpitch = 0;

#if NWSCONS
	printf("\n");
	kbdattach(self, &pckbd_wscons_idev);
#else
	printf(": no wscons driver present; no input possible\n");
#endif
}

/*
 * Got a console receive interrupt -
 * the console processor wants to give us a character.
 * Catch the character, and see who it goes to.
 */
int
pckbdintr(arg)
	void *arg;
{
	u_char data;
	static u_char last;

	if ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_DIB) == 0)
		return 0;
	if (polling)
		return 1;
	do {
		KBD_DELAY;
		data = bus_space_read_1(pckbd_iot, pckbd_ioh, KBDATAP);

		switch (data) {
		case KBR_ACK:
			ack = 1;
			break;
		case KBR_RESEND:
			nak = 1;
			break;
		default:
			/* Always ignore typematic keys */
			if (data == last)
				break;
			last = data;
#if NWSCONS
			kbd_input(data);
#endif
			break;
		}
	} while (bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP) & KBS_DIB);
	return 1;
}

void
do_async_update(v)
	void *v;
{
	u_long poll = (u_long)v;

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
}

void
async_update()
{
	if (kernel || polling) {
		if (async)
			untimeout(do_async_update, NULL);
		do_async_update((void *)1);
	} else {
		if (async)
			return;
		async = 1;
		timeout(do_async_update, NULL, 1);
	}
}

int
pckbd_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{

	switch (cmd) {
	case WSCONSIO_KBD_GTYPE:
		*(int *)data = KBD_TYPE_PC;
		return 0;
	}
	return ENOTTY;
}

#if 0
int
pcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct pc_softc *sc = pccd.cd_devs[PCUNIT(dev)];
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
#endif

#define	CODE_SIZE	4		/* Use a max of 4 for now... */
typedef struct {
	u_short	type;
	char unshift[CODE_SIZE];
	char shift[CODE_SIZE];
	char ctl[CODE_SIZE];
} Scan_def;
static Scan_def	scan_codes[] = {
    {	NONE,	"",		"",		"",	 },	/* 0 unused */
    {	ASCII,	"\033",		"\033",		"\033",	 },	/* 1 ESCape */
    {	ASCII,	"1",		"!",		"!",	 },	/* 2 1 */
    {	ASCII,	"2",		"@",		"\000",	 },	/* 3 2 */
    {	ASCII,	"3",		"#",		"#",	 },	/* 4 3 */
    {	ASCII,	"4",		"$",		"$",	 },	/* 5 4 */
    {	ASCII,	"5",		"%",		"%",	 },	/* 6 5 */
    {	ASCII,	"6",		"^",		"\036",	 },	/* 7 6 */
    {	ASCII,	"7",		"&",		"&",	 },	/* 8 7 */
    {	ASCII,	"8",		"*",		"\010",	 },	/* 9 8 */
    {	ASCII,	"9",		"(",		"(",	 },	/* 10 9 */
    {	ASCII,	"0",		")",		")",	 },	/* 11 0 */
    {	ASCII,	"-",		"_",		"\037",	 },	/* 12 - */
    {	ASCII,	"=",		"+",		"+",	 },	/* 13 = */
    {	ASCII,	"\177",		"\177",		"\010",	 },	/* 14 backspace */
    {	ASCII,	"\t",		"\177\t",	"\t",	 },	/* 15 tab */
    {	ASCII,	"q",		"Q",		"\021",	 },	/* 16 q */
    {	ASCII,	"w",		"W",		"\027",	 },	/* 17 w */
    {	ASCII,	"e",		"E",		"\005",	 },	/* 18 e */
    {	ASCII,	"r",		"R",		"\022",	 },	/* 19 r */
    {	ASCII,	"t",		"T",		"\024",	 },	/* 20 t */
    {	ASCII,	"y",		"Y",		"\031",	 },	/* 21 y */
    {	ASCII,	"u",		"U",		"\025",	 },	/* 22 u */
    {	ASCII,	"i",		"I",		"\011",	 },	/* 23 i */
    {	ASCII,	"o",		"O",		"\017",	 },	/* 24 o */
    {	ASCII,	"p",		"P",		"\020",	 },	/* 25 p */
    {	ASCII,	"[",		"{",		"\033",	 },	/* 26 [ */
    {	ASCII,	"]",		"}",		"\035",	 },	/* 27 ] */
    {	ASCII,	"\r",		"\r",		"\n",	 },	/* 28 return */
    {	CTL,	"",		"",		"",	 },	/* 29 control */
    {	ASCII,	"a",		"A",		"\001",	 },	/* 30 a */
    {	ASCII,	"s",		"S",		"\023",	 },	/* 31 s */
    {	ASCII,	"d",		"D",		"\004",	 },	/* 32 d */
    {	ASCII,	"f",		"F",		"\006",	 },	/* 33 f */
    {	ASCII,	"g",		"G",		"\007",	 },	/* 34 g */
    {	ASCII,	"h",		"H",		"\010",	 },	/* 35 h */
    {	ASCII,	"j",		"J",		"\n",	 },	/* 36 j */
    {	ASCII,	"k",		"K",		"\013",	 },	/* 37 k */
    {	ASCII,	"l",		"L",		"\014",	 },	/* 38 l */
    {	ASCII,	";",		":",		";",	 },	/* 39 ; */
    {	ASCII,	"'",		"\"",		"'",	 },	/* 40 ' */
    {	ASCII,	"`",		"~",		"`",	 },	/* 41 ` */
    {	SHIFT,	"",		"",		"",	 },	/* 42 shift */
    {	ASCII,	"\\",		"|",		"\034",	 },	/* 43 \ */
    {	ASCII,	"z",		"Z",		"\032",	 },	/* 44 z */
    {	ASCII,	"x",		"X",		"\030",	 },	/* 45 x */
    {	ASCII,	"c",		"C",		"\003",	 },	/* 46 c */
    {	ASCII,	"v",		"V",		"\026",	 },	/* 47 v */
    {	ASCII,	"b",		"B",		"\002",	 },	/* 48 b */
    {	ASCII,	"n",		"N",		"\016",	 },	/* 49 n */
    {	ASCII,	"m",		"M",		"\r",	 },	/* 50 m */
    {	ASCII,	",",		"<",		"<",	 },	/* 51 , */
    {	ASCII,	".",		">",		">",	 },	/* 52 . */
    {	ASCII,	"/",		"?",		"\037",	 },	/* 53 / */
    {	SHIFT,	"",		"",		"",	 },	/* 54 shift */
    {	KP,	"*",		"*",		"*",	 },	/* 55 kp * */
    {	ALT,	"",		"",		"",	 },	/* 56 alt */
    {	ASCII,	" ",		" ",		"\000",	 },	/* 57 space */
    {	CAPS,	"",		"",		"",	 },	/* 58 caps */
    {	FUNC,	"\033[M",	"\033[Y",	"\033[k", },	/* 59 f1 */
    {	FUNC,	"\033[N",	"\033[Z",	"\033[l", },	/* 60 f2 */
    {	FUNC,	"\033[O",	"\033[a",	"\033[m", },	/* 61 f3 */
    {	FUNC,	"\033[P",	"\033[b",	"\033[n", },	/* 62 f4 */
    {	FUNC,	"\033[Q",	"\033[c",	"\033[o", },	/* 63 f5 */
    {	FUNC,	"\033[R",	"\033[d",	"\033[p", },	/* 64 f6 */
    {	FUNC,	"\033[S",	"\033[e",	"\033[q", },	/* 65 f7 */
    {	FUNC,	"\033[T",	"\033[f",	"\033[r", },	/* 66 f8 */
    {	FUNC,	"\033[U",	"\033[g",	"\033[s", },	/* 67 f9 */
    {	FUNC,	"\033[V",	"\033[h",	"\033[t", },	/* 68 f10 */
    {	NUM,	"",		"",		"",	 },	/* 69 num lock */
    {	SCROLL,	"",		"",		"",	 },	/* 70 scroll lock */
    {	KP,	"7",		"\033[H",	"7",	 },	/* 71 kp 7 */
    {	KP,	"8",		"\033[A",	"8",	 },	/* 72 kp 8 */
    {	KP,	"9",		"\033[I",	"9",	 },	/* 73 kp 9 */
    {	KP,	"-",		"-",		"-",	 },	/* 74 kp - */
    {	KP,	"4",		"\033[D",	"4",	 },	/* 75 kp 4 */
    {	KP,	"5",		"\033[E",	"5",	 },	/* 76 kp 5 */
    {	KP,	"6",		"\033[C",	"6",	 },	/* 77 kp 6 */
    {	KP,	"+",		"+",		"+",	 },	/* 78 kp + */
    {	KP,	"1",		"\033[F",	"1",	 },	/* 79 kp 1 */
    {	KP,	"2",		"\033[B",	"2",	 },	/* 80 kp 2 */
    {	KP,	"3",		"\033[G",	"3",	 },	/* 81 kp 3 */
    {	KP,	"0",		"\033[L",	"0",	 },	/* 82 kp 0 */
    {	KP,	".",		"\177",		".",	 },	/* 83 kp . */
    {	NONE,	"",		"",		"",	 },	/* 84 0 */
    {	NONE,	"100",		"",		"",	 },	/* 85 0 */
    {	NONE,	"101",		"",		"",	 },	/* 86 0 */
    {	FUNC,	"\033[W",	"\033[i",	"\033[u", },	/* 87 f11 */
    {	FUNC,	"\033[X",	"\033[j",	"\033[v", },	/* 88 f12 */
    {	NONE,	"102",		"",		"",	 },	/* 89 0 */
    {	NONE,	"103",		"",		"",	 },	/* 90 0 */
    {	NONE,	"",		"",		"",	 },	/* 91 0 */
    {	NONE,	"",		"",		"",	 },	/* 92 0 */
    {	NONE,	"",		"",		"",	 },	/* 93 0 */
    {	NONE,	"",		"",		"",	 },	/* 94 0 */
    {	NONE,	"",		"",		"",	 },	/* 95 0 */
    {	NONE,	"",		"",		"",	 },	/* 96 0 */
    {	NONE,	"",		"",		"",	 },	/* 97 0 */
    {	NONE,	"",		"",		"",	 },	/* 98 0 */
    {	NONE,	"",		"",		"",	 },	/* 99 0 */
    {	NONE,	"",		"",		"",	 },	/* 100 */
    {	NONE,	"",		"",		"",	 },	/* 101 */
    {	NONE,	"",		"",		"",	 },	/* 102 */
    {	NONE,	"",		"",		"",	 },	/* 103 */
    {	NONE,	"",		"",		"",	 },	/* 104 */
    {	NONE,	"",		"",		"",	 },	/* 105 */
    {	NONE,	"",		"",		"",	 },	/* 106 */
    {	NONE,	"",		"",		"",	 },	/* 107 */
    {	NONE,	"",		"",		"",	 },	/* 108 */
    {	NONE,	"",		"",		"",	 },	/* 109 */
    {	NONE,	"",		"",		"",	 },	/* 110 */
    {	NONE,	"",		"",		"",	 },	/* 111 */
    {	NONE,	"",		"",		"",	 },	/* 112 */
    {	NONE,	"",		"",		"",	 },	/* 113 */
    {	NONE,	"",		"",		"",	 },	/* 114 */
    {	NONE,	"",		"",		"",	 },	/* 115 */
    {	NONE,	"",		"",		"",	 },	/* 116 */
    {	NONE,	"",		"",		"",	 },	/* 117 */
    {	NONE,	"",		"",		"",	 },	/* 118 */
    {	NONE,	"",		"",		"",	 },	/* 119 */
    {	NONE,	"",		"",		"",	 },	/* 120 */
    {	NONE,	"",		"",		"",	 },	/* 121 */
    {	NONE,	"",		"",		"",	 },	/* 122 */
    {	NONE,	"",		"",		"",	 },	/* 123 */
    {	NONE,	"",		"",		"",	 },	/* 124 */
    {	NONE,	"",		"",		"",	 },	/* 125 */
    {	NONE,	"",		"",		"",	 },	/* 126 */
    {	NONE,	"",		"",		"",	 },	/* 127 */
};
/*
 * Get characters from the keyboard.  If none are present, return NULL.
 */
char *
pckbd_translate(dev, c)
	struct device *dev;
	int c;
{
	u_char dt = c;
	static u_char extended = 0, shift_state = 0;
	static u_char capchar[2];

	if (dt == KBR_EXTENDED) {
		extended = 1;
		return (NULL);
	}

#ifdef DDB
	/*
	 * Check for cntl-alt-esc.
	 */
	if ((dt == 1) && (shift_state & (CTL | ALT)) == (CTL | ALT)) {
		if (db_console)
			Debugger();
		return (NULL);
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
	return (NULL);
}


/* ARGSUSED */
int
pckbd_cngetc(dev)
	struct device *dev;
{
        register char *cp = NULL;
	u_char data;
	static u_char last;

        do {
		/* wait for byte */
                while ((bus_space_read_1(pckbd_iot, pckbd_ioh, KBSTATP)
		    & KBS_DIB) == 0)
			KBD_DELAY;
		KBD_DELAY;

                data = bus_space_read_1(pckbd_iot, pckbd_ioh, KBDATAP);

                if (data == KBR_ACK) {
                        ack = 1;
                        continue;
		}
                if (data ==  KBR_RESEND) {
                        nak = 1;
                        continue;
		}

		/* Ignore typematic keys */
		if (data == last)
			continue;
		last = data;

		cp = pckbd_translate(NULL, data);
        } while (!cp);
        if (*cp == '\r')
                return '\n';
        return *cp;
}

void
pckbd_cnpollc(dev, on)
	struct device *dev;
        int on;
{
	struct pckbd_softc *sc = (struct pckbd_softc *)dev;

        polling = on;
        if (!on) {
                int s;

                /*
                 * If disabling polling on a device that's been configured,
                 * make sure there are no bytes left in the FIFO, holding up
                 * the interrupt line.  Otherwise we won't get any further
                 * interrupts.
                 */
		if (sc != 0) {
			s = spltty();
			pckbdintr(sc);
			splx(s);
		}
        }
}

void
pckbd_bell(dev, wbd)
	struct device *dev;
	struct wsconsio_bell_data *wbd;
{
	struct pckbd_softc *sc = (struct pckbd_softc *)dev;
	int pitch, period;
	int s;

	pitch = wbd->wbd_pitch;
	period = (wbd->wbd_period * hz) / 1000;
	/* XXX volume ignored */

	s = splhigh();
	if (sc->sc_bellactive)
		untimeout(pckbd_bell_stop, sc);
	splx(s);
	if (pitch == 0 || period == 0) {
		pckbd_bell_stop(sc);
		sc->sc_bellpitch = 0;
		return;
	}
	if (!sc->sc_bellactive || sc->sc_bellpitch != pitch) {
		s = splhigh();
		bus_space_write_1(pckbd_iot, pckbd_timer_ioh, TIMER_MODE,
		    TIMER_SEL2 | TIMER_16BIT | TIMER_SQWAVE);
		bus_space_write_1(pckbd_iot, pckbd_timer_ioh, TIMER_CNTR2,
		    TIMER_DIV(pitch) % 256);
		bus_space_write_1(pckbd_iot, pckbd_timer_ioh, TIMER_CNTR2,
		    TIMER_DIV(pitch) / 256);
		/* enable speaker */
		bus_space_write_1(pckbd_iot, pckbd_ioh, PITAUX_PORT,
	    	    bus_space_read_1(pckbd_iot, pckbd_ioh, PITAUX_PORT) |
		      PIT_SPKR);
		splx(s);
	}
	sc->sc_bellpitch = pitch;
	sc->sc_bellactive = 1;
	timeout(pckbd_bell_stop, sc, period);
}

void
pckbd_bell_stop(arg)
	void *arg;
{
	struct pckbd_softc *sc = arg;
	int s;

	/* disable bell */
	s = splhigh();
	bus_space_write_1(pckbd_iot, pckbd_ioh, PITAUX_PORT,
	    bus_space_read_1(pckbd_iot, pckbd_ioh, PITAUX_PORT) & ~PIT_SPKR);
	sc->sc_bellactive = 0;
	splx(s);
}
