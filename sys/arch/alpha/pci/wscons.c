/*	$NetBSD: wscons.c,v 1.3 1995/11/23 02:38:36 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/termios.h>

#include <dev/cons.h>
#include <dev/pseudo/ansicons.h>
#include <alpha/pci/wsconsvar.h>

#define	NWSC	16			/* XXX XXX XXX */

#define WSCUNIT(dev)	minor(dev)

struct wsc_softc {
	int			sc_flags;

	struct device		*sc_odev;
	struct ansicons		*sc_ansicons;

	struct tty		*sc_tty;
};

#define	WSC_INPUT	0x01
#define	WSC_OUTPUT	0x02
#define	WSC_ALIVE	(WSC_INPUT | WSC_OUTPUT)

struct wsc_softc wsc_sc[NWSC];			/* XXX XXX XXX */
int nwsc = NWSC;
int nextiwsc;
int nextowsc;

void	wscstart __P((struct tty *));
int	wscparam __P((struct tty *, struct termios *));

int wsccngetc __P((dev_t));
void wsccnputc __P((dev_t, int));
void wsccnpollc __P((dev_t, int));

struct ansicons *wsc_console_ansicons;
void *wsc_input_arg;
int (*wsc_console_getc) __P((void *)) = NULL;
void (*wsc_console_pollc) __P((void *, int)) = NULL;
struct consdev wsccons = { NULL, NULL, wsccngetc, wsccnputc,
                            wsccnpollc, NODEV, 1 };

int wsc_kbdfocusunit = -1;	/* XXX */

int
wscattach_output(dev, console, acp, acf, acfarg, mrow, mcol, crow, ccol)
	struct device *dev;
	int console;
	struct ansicons *acp;
	struct ansicons_functions *acf;
	void *acfarg;
	int mrow, mcol, crow, ccol;
{

	if (nextowsc >= nwsc)
		return 0;

	wsc_sc[nextowsc].sc_odev = dev;
	wsc_sc[nextowsc].sc_ansicons = acp;

	if (wsc_kbdfocusunit == -1)
		wsc_kbdfocusunit = nextowsc;

	if (!console)
		ansicons_attach(acp, acf, acfarg, mrow, mcol, crow, ccol);
	else {
		wsc_kbdfocusunit = nextowsc;
		wsccons.cn_dev = makedev(25, nextowsc);
	}

	wsc_sc[nextowsc].sc_flags |= WSC_OUTPUT;
	wsc_sc[nextowsc].sc_tty = ttymalloc();

	printf("wsc%d: %s attached as output\n", nextowsc, dev->dv_xname);
	if (console)
		printf("wsc%d: console\n", nextowsc);

	nextowsc++;
	return 1;
}

void
wscattach_input(dev, cookie, getc, pollc)
	struct device *dev;
	void *cookie;
	int (*getc) __P((void *));
	void (*pollc) __P((void *, int));
{

	printf("wsc: %s attached as input device\n", dev->dv_xname);
	
	wsc_input_arg = cookie;
	wsc_console_getc = getc;
	wsc_console_pollc = pollc;
}

int
wscopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct wsc_softc *sc;
	int unit = WSCUNIT(dev);
	struct tty *tp;

	if (unit >= nwsc)
		return ENXIO;
	sc = &wsc_sc[unit];
	if (sc == 0)
		return ENXIO;

	if ((sc->sc_flags & WSC_ALIVE) == 0)
		return ENXIO;

	if (!sc->sc_tty)
		panic("wscopen: no tty!");
	tp = sc->sc_tty;

	tp->t_oproc = wscstart;
	tp->t_param = wscparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		wscparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) != 0 && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
wscclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct wsc_softc *sc = &wsc_sc[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
#ifdef notyet /* XXX */
	ttyfree(tp);
#endif
	return(0);
}

int
wscread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct wsc_softc *sc = &wsc_sc[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
wscwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct wsc_softc *sc = &wsc_sc[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
wsctty(dev)
	dev_t dev;
{
	struct wsc_softc *sc = &wsc_sc[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

int
wscioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wsc_softc *sc = &wsc_sc[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	default:
		return (ENOTTY);
	}

#ifdef DIAGNOSTIC
	panic("wscioctl: impossible");
#endif
}

void
wscstart(tp)
	register struct tty *tp;
{
	register int s, n, i;
	char buf[OBUFSIZ];

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	splx(s);
	n = q_to_b(&tp->t_outq, buf, sizeof(buf));
	for (i = 0; i < n; ++i)
		buf[i] &= 0177;		/* strip parity (argh) */
	ansicons_input(wsc_sc[WSCUNIT(tp->t_dev)].sc_ansicons, buf, n);

	s = spltty();
	tp->t_state &= ~TS_BUSY;
	/* Come back if there's more to do */
	if (tp->t_outq.c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout(ttrstrt, tp, 1);
	}
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (tp->t_state&TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	splx(s);
}

void
wscstop(tp, flag)
	struct tty *tp;
	int flag;
{

}

/*
 * Set line parameters.
 */
int
wscparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}

/*
 * keyboard input!
 */
void
wscons_kbdinput(cp)
	char *cp;
{
	struct wsc_softc *sc;
	struct tty *tp;

	if (wsc_kbdfocusunit == -1)
		return;

	sc = &wsc_sc[wsc_kbdfocusunit];
	tp = sc->sc_tty;

	while (*cp)
		(*linesw[tp->t_line].l_rint)(*cp++, tp);
}

/*
 * XXX CONSOLE HANDLING GUNK.
 */

void
wsc_console(acp, acf, acfarg, mrow, mcol, crow, ccol)
        struct ansicons *acp;
        struct ansicons_functions *acf;
        void *acfarg;
        int mrow, mcol, crow, ccol;
{

	wsc_console_ansicons = acp;
	ansicons_attach(acp, acf, acfarg, mrow, mcol, crow, ccol);

	cn_tab = &wsccons;
}

void
wsccnputc(dev, ic)
	dev_t dev;
	int ic;
{
	char c = ic;

	ansicons_input(wsc_console_ansicons, &c, 1);
}

int
wsccngetc(dev)
	dev_t dev;
{

	if (wsc_console_getc != NULL)
		return (*wsc_console_getc)(wsc_input_arg);
	else
		return '\0';
}

void
wsccnpollc(dev, i)
	dev_t dev;
	int i;
{

	if (wsc_console_pollc != NULL)
		(*wsc_console_pollc)(wsc_input_arg, i);
}
