/*	$OpenBSD: wscons.c,v 1.7 1998/08/15 22:47:53 kstailey Exp $	*/
/*	$NetBSD: wscons.c,v 1.10 1996/12/05 01:39:47 cgd Exp $	*/

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
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
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/termios.h>

#include <dev/cons.h>
#include <dev/wscons/wsconsvar.h>
#include <dev/wscons/wscons_emul.h>
#include <dev/wscons/kbd.h>
#include <machine/wsconsio.h>

cdev_decl(wscons);

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/*
 * Autoconfiguration glue.
 */
struct wscons_softc {
	struct device sc_dev;

	struct wscons_emul_data *sc_emul_data;
	struct tty		*sc_tty;

	void			*sc_fn_cookie;
	wscons_ioctl_t		sc_ioctl;
	wscons_mmap_t		sc_mmap;
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	wsconsmatch __P((struct device *, void *, void *));
#else
int	wsconsmatch __P((struct device *, struct cfdata *, void *));
#endif
void	wsconsattach __P((struct device *, struct device *, void *));

struct cfattach wscons_ca = {
	sizeof (struct wscons_softc), wsconsmatch, wsconsattach,
};

struct cfdriver wscons_cd = {
    NULL, "wscons", DV_TTY,
};

/*
 * Console handing functions and variables.
 */
int	wscons_console_attached;	/* polled console fns attached */
int	wscons_console_unit = -1;
struct wscons_emul_data wscons_console_emul_data;

int	wscons_cngetc __P((dev_t));
void	wscons_cnputc __P((dev_t, int));
void	wscons_cnpollc __P((dev_t, int));

struct consdev wscons_consdev =
    { NULL, NULL, wscons_cngetc, wscons_cnputc, wscons_cnpollc, NODEV, 1 };

/*
 * Input focus handling: where characters from the keyboard are sent.
 */
int	wscons_input_focus = -1;

/*
 * TTY interface helpers.
 */

#define WSCUNIT(dev)	minor(dev)

void	wsconsstart __P((struct tty *));
int	wsconsparam __P((struct tty *, struct termios *));


/*
 * Output device selection and attachment.
 */

int
#ifdef __BROKEN_INDIRECT_CONFIG
wsconsmatch(parent, cfdata, aux)
#else
wsconsmatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *cfdata;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
	struct wscons_attach_args *waa = aux;
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = cfdata;
#endif

	if (waa->waa_isconsole && wscons_console_unit != -1)
		panic("wsconsmatch: multiple consoles?");

	/* If console-ness specified... */
	if (cf->wsconscf_console != -1) {
		/*
		 * If exact match, return match with high priority,
		 * else return don't match.
		 */
		if ((cf->wsconscf_console && waa->waa_isconsole) ||
		    (!cf->wsconscf_console && !waa->waa_isconsole))
			return (10);
		else
			return (0);
	}

	/* Otherwise match with low priority. */
	return (1);
}

void
wsconsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wscons_attach_args *waa = aux;
	struct wscons_softc *sc = (struct wscons_softc *)self;
	int console;

	console = waa->waa_isconsole;
	if (console)
		printf(": console");

	/*
	 * If output has already been set up, record it now.  Otherwise,
	 * do the setup.
	 */
	if (console) {
		sc->sc_emul_data = &wscons_console_emul_data;
		wscons_console_unit = sc->sc_dev.dv_unit;
		wscons_consdev.cn_dev =
		    makedev(25, wscons_console_unit); /* XXX */
	} else {
		sc->sc_emul_data = malloc(sizeof(*sc->sc_emul_data), M_DEVBUF,
		    M_WAITOK);
		wscons_emul_attach(sc->sc_emul_data, &waa->waa_odev_spec);
	}

	/*
	 * Set wscons input focus if this is the console device,
	 * or if we've not yet set the input focus.
	 */
	if (console || wscons_input_focus == -1)
		wscons_input_focus = sc->sc_dev.dv_unit;

	/*
	 * Set up the device's tty structure.
	 */
	sc->sc_tty = ttymalloc();
	tty_attach(sc->sc_tty);

	/*
	 * Record other relevant information: ioctl and mmap functions.
	 */
	sc->sc_fn_cookie = waa->waa_odev_spec.wo_miscfuncs_cookie;
	sc->sc_ioctl = waa->waa_odev_spec.wo_ioctl;
	sc->sc_mmap = waa->waa_odev_spec.wo_mmap;

	printf("\n");
}


/*
 * Keyboard input handling.
 */

void
wscons_input(cp)
	char *cp;
{
	struct wscons_softc *sc;
	struct tty *tp;

	if (wscons_input_focus == -1)
		return;

#ifdef DIAGNOSTIC
	if (wscons_input_focus >= wscons_cd.cd_ndevs)
		panic("wscons_input: bogus input focus");
	sc = wscons_cd.cd_devs[wscons_input_focus];
	if (sc == NULL)
		panic("wscons_input: null input device");
	tp = sc->sc_tty;
	if (tp == NULL)
		panic("wscons_input: no tty");
#else
	sc = wscons_cd.cd_devs[wscons_input_focus];
	tp = sc->sc_tty;
#endif

	if (*cp == '\0')	/* deal with kbd generated NULs */
		(*linesw[tp->t_line].l_rint)(*cp, tp);
	else
		while (*cp)
			(*linesw[tp->t_line].l_rint)(*cp++, tp);
}


/*
 * Console (output) tty-handling functions.
 */

int
wsconsopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct wscons_softc *sc;
	int unit = WSCUNIT(dev), newopen, rv;
	struct tty *tp;

	if (unit >= wscons_cd.cd_ndevs)
		return ENXIO;
	sc = wscons_cd.cd_devs[unit];
	if (sc == NULL)
		return ENXIO;

#ifdef DIAGNOSTIC
	if (!sc->sc_tty)
		panic("wscopen: no tty!");
#endif
	tp = sc->sc_tty;

	tp->t_oproc = wsconsstart;
	tp->t_param = wsconsparam;
	tp->t_dev = dev;
	newopen = (tp->t_state & TS_ISOPEN) == 0;
	if (newopen) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		wsconsparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) != 0 && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;

	rv = ((*linesw[tp->t_line].l_open)(dev, tp));
	if (newopen && (rv == 0)) {
		/* set window sizes to be correct */
		tp->t_winsize.ws_row = sc->sc_emul_data->ac_nrow;
		tp->t_winsize.ws_col = sc->sc_emul_data->ac_ncol;
	}
	return (rv);
}

int
wsconsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
	return(0);
}

int
wsconsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
wsconswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
wsconstty(dev)
	dev_t dev;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

int
wsconsioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;

	/* do the line discipline ioctls first */
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	/* then the tty ioctls */
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	/* then the underlying frame buffer device ioctls */
	if (sc->sc_ioctl != NULL)
		error = (*sc->sc_ioctl)(sc->sc_fn_cookie, cmd, data,
		    flag, p);
	if (error >= 0)
		return error;

	/*
	 * then the keyboard ioctls, if we have input focus.
	 * This is done last because it's a special case: it will
	 * return ENOTTY (not -1) if it can't figure out what
	 * to do with the request.
	 */
	if (WSCUNIT(dev) == wscons_input_focus)
		error = kbdioctl(0, cmd, data, flag, p);	/* XXX dev */
	else
		error = ENOTTY;

	return (error);
}

int
wsconsmmap(dev, offset, prot)
	dev_t dev;
	int offset;		/* XXX */
	int prot;
{
	struct wscons_softc *sc = wscons_cd.cd_devs[WSCUNIT(dev)];

	if (sc->sc_mmap != NULL)
		return (*sc->sc_mmap)(sc->sc_dev.dv_parent, offset, prot);
	else
		return -1;
}

void
wsconsstart(tp)
	register struct tty *tp;
{
	struct wscons_softc *sc;
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

	sc = wscons_cd.cd_devs[WSCUNIT(tp->t_dev)];
	wscons_emul_input(sc->sc_emul_data, buf, n);

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

int
wsconsstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	/* XXX ??? */
	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	splx(s);
	return 0;
}

/*
 * Set line parameters.
 */
int
wsconsparam(tp, t)
	struct tty *tp;
	struct termios *t;
{

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	return 0;
}


/*
 * Polled-console handing setup and manipulation.
 */

void
wscons_attach_console(wo)
	const struct wscons_odev_spec *wo;
{

	if (wscons_console_attached)
		panic("wscons_attach_console: multiple times");

	wscons_emul_attach(&wscons_console_emul_data, wo);
	cn_tab = &wscons_consdev;
	wscons_console_attached = 1;
}

void
wscons_cnputc(dev, ic)
	dev_t dev;
	int ic;
{
	char c = ic;

	if (wscons_console_attached)
		wscons_emul_input(&wscons_console_emul_data, &c, 1);
}

int
wscons_cngetc(dev)
	dev_t dev;
{

	return kbd_cngetc(dev);		/* XXX XXX */
}

void
wscons_cnpollc(dev, i)
	dev_t dev;
	int i;
{

	kbd_cnpollc(dev, i);		/* XXX XXX */
}
