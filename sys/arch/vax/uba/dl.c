/*	$OpenBSD: dl.c,v 1.2 1998/05/11 14:59:05 niklas Exp $	*/
/*	$NetBSD: dl.c,v 1.1 1997/02/04 19:13:18 ragge Exp $	*/
/*
 * Copyright (c) 1997  Ben Harris.  All rights reserved.
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
 * Copyright (c) 1995, 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1986, 1990, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 */

/*
 * dl.c -- Device driver for the DL11 and DLV11 serial cards.
 *
 * OS-interface code derived from the dz and dca (hp300) drivers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <machine/pte.h>
#include <machine/trap.h>

#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

#include <vax/uba/dlreg.h>

#define DL_I2C(i) (i)
#define DL_C2I(c) (c)

struct dl_softc {
	struct device	sc_dev;
	dlregs*		sc_addr;
	struct tty*		sc_tty;
};

static	int	dl_match __P((struct device *, void *, void *));
static	void	dl_attach __P((struct device *, struct device *, void *));
static	void	dlrint __P((int));
static	void	dlxint __P((int));
static	void	dlstart __P((struct tty *));
static	int	dlparam __P((struct tty *, struct termios *));
static	void	dlbrk __P((struct dl_softc *, int));
struct	tty *	dltty __P((dev_t));
	int	dlopen __P((dev_t, int, int, struct proc *));
	int	dlclose __P((dev_t, int, int, struct proc *));
	int	dlread __P((dev_t, struct uio *, int));
	int	dlwrite __P((dev_t, struct uio *, int));
	int	dlioctl __P((dev_t, int, caddr_t, int, struct proc *));
	void	dlstop __P((struct tty *, int));

struct cfdriver dl_cd = {
	NULL, "dl", DV_TTY
};

struct cfattach dl_ca = {
	sizeof(struct dl_softc), dl_match, dl_attach
};

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dl_match (parent, match, aux)
	struct device * parent;
	void *match, *aux;
{
	struct uba_attach_args *ua = aux;
	register dlregs *dladdr;

	dladdr = (dlregs*) ua->ua_addr;
#ifdef DL_DEBUG
        printf("Probing for dl at %lo ... ", (long)dladdr);
#endif

	dladdr->dl_xcsr = DL_XCSR_TXIE;
	if (dladdr->dl_xcsr != (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 1; XCSR = %.4b)\n", dladdr->dl_xcsr,
		       DL_XCSR_BITS);
#endif
		return 0;
	}
	
	/*
	 * We have to force an interrupt so the uba driver can work
	 * out where we are.  Unfortunately, the only way to make a
	 * DL11 interrupt is to get it to send or receive a
	 * character.  We'll send a NUL and hope it doesn't hurt
	 * anything.
	 */

	dladdr->u_xbuf.bytes.byte_lo = '\0';
#if 0 /* This test seems to fail 2/3 of the time :-( */
	if (dladdr->dl_xcsr != (DL_XCSR_TXIE)) {
#ifdef DL_DEBUG
	        printf("failed (step 2; XCSR = %.4b)\n", dladdr->dl_xcsr,
		       DL_XCSR_BITS);
#endif
		return 0;
	}
#endif
	DELAY(100000); /* delay 1/10 s for character to transmit */
	if (dladdr->dl_xcsr != (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 3; XCSR = %.4b)\n", dladdr->dl_xcsr,
		       DL_XCSR_BITS);
#endif
		return 0;
	}

	/* Register the TX interrupt handler */
	ua->ua_ivec = dlxint;

        /* What else do I need to do? */

	return 1;

}

static void
dl_attach (parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_softc *uh = (void *)parent;
	struct dl_softc *sc = (void *)self;
	register struct uba_attach_args *ua = aux;
	register dlregs *dladdr;

	dladdr = (dlregs *) ua->ua_addr;
	sc->sc_addr = dladdr;
	
	/* Tidy up the device */

	dladdr->dl_rcsr = DL_RCSR_RXIE;
	dladdr->dl_xcsr = DL_XCSR_TXIE;

	/* Initialize our softc structure. Should be done in open? */
	
	sc->sc_tty = ttymalloc();
	tty_attach(sc->sc_tty);

	/* Now register the RX interrupt handler */
	ubasetvec(self, ua->ua_cvec-1, dlrint);

	printf("\n");
	return;
}

/* Receiver Interrupt Handler */

static void
dlrint(cntlr)
	int cntlr;
{
	struct	dl_softc *sc = dl_cd.cd_devs[cntlr];
	volatile dlregs *dladdr;
	register struct tty *tp;
	register int cc;
	register unsigned c;

	dladdr =  sc->sc_addr;

	if (dladdr->dl_rcsr & DL_RCSR_RX_DONE) {
	        c = dladdr->dl_rbuf;
		cc = c & 0xFF;
		tp = sc->sc_tty;

		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
			return;
		}

		if (c & DL_RBUF_OVERRUN_ERR)
			/*
			 * XXX: This should really be logged somwhere
			 * else where we can afford the time.
			 */
			log(LOG_WARNING, "%s: rx overrun\n",
			    sc->sc_dev.dv_xname);
		if (c & DL_RBUF_FRAMING_ERR)
			cc |= TTY_FE;
		if (c & DL_RBUF_PARITY_ERR)
			cc |= TTY_PE;

		(*linesw[tp->t_line].l_rint)(cc, tp);
	} else
		log(LOG_WARNING, "%s: stray rx interrupt\n",
		    sc->sc_dev.dv_xname);
	return;
}

/* Transmitter Interrupt Handler */

static void
dlxint(cntlr)
	int cntlr;
{
	struct	dl_softc *sc = dl_cd.cd_devs[cntlr];
	volatile dlregs *dladdr;
	register struct tty *tp;
	
	dladdr = sc->sc_addr;
	tp = sc->sc_tty;
	tp->t_state &= ~(TS_BUSY | TS_FLUSH);
	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		dlstart(tp);
       
	return;
}

int
dlopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp;
	register int unit;
	struct dl_softc *sc;
	int s, error = 0;

	unit = DL_I2C(minor(dev));

	if (unit >= dl_cd.cd_ndevs || dl_cd.cd_devs[unit] == NULL)
		return ENXIO;
	sc = dl_cd.cd_devs[unit];

	tp = sc->sc_tty;
	if (tp == NULL)
		return ENODEV;
	tp->t_oproc = dlstart;
	tp->t_param = dlparam;
	tp->t_dev = dev;

	if (!(tp->t_state & TS_ISOPEN)) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		/* No modem control, so set CLOCAL. */
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		
		dlparam(tp, &tp->t_termios);
		ttsetwater(tp);
		
	} else if ((tp->t_state & TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
dlclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct dl_softc *sc;
	register struct tty *tp;
	register int unit;

	unit = DL_I2C(minor(dev));
	sc = dl_cd.cd_devs[unit];
      	tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);

	/* Make sure a BREAK state is not left enabled. */
	dlbrk(sc, 0);

	return (ttyclose(tp));
}

int
dlread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;
	struct dl_softc *sc;
	register int unit;

	unit = DL_I2C(minor(dev));
	sc = dl_cd.cd_devs[unit];
	tp = sc->sc_tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dlwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;
	struct dl_softc *sc;
	register int unit;

	unit = DL_I2C(minor(dev));
	sc = dl_cd.cd_devs[unit];
	tp = sc->sc_tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
dlioctl(dev, cmd, data, flag, p)
	dev_t dev;
        int cmd;
        caddr_t data;
        int flag;
        struct proc *p;
{
	struct dl_softc *sc;
        register struct tty *tp;
        register int unit;
        int error;

	unit = DL_I2C(minor(dev));
	sc = dl_cd.cd_devs[unit];
	tp = sc->sc_tty;

        error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
        if (error >= 0)
                return (error);
        error = ttioctl(tp, cmd, data, flag, p);
        if (error >= 0)
                return (error);

	switch (cmd) {
	       
        case TIOCSBRK:
                dlbrk(sc, 1);
                break;

        case TIOCCBRK:
                dlbrk(sc, 0);
                break;

        case TIOCMGET:
		/* No modem control, assume they're all low. */
                *(int *)data = 0;
                break;

        default:
                return (ENOTTY);
        }
        return (0);
}

struct tty *
dltty(dev)
	dev_t dev;
{
	register struct dl_softc* sc;
	
	sc = dl_cd.cd_devs[DL_I2C(minor(dev))];
	return sc->sc_tty;
}

void
dlstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct dl_softc *sc;
	int unit, s;

	unit = DL_I2C(minor(tp->t_dev));
	sc = dl_cd.cd_devs[unit];

	s = spltty();

	if (tp->t_state & TS_BUSY)
                if (!(tp->t_state & TS_TTSTOP))
                        tp->t_state |= TS_FLUSH;
        splx(s);
}

static void
dlstart(tp)
	register struct tty *tp;
{
	register struct dl_softc *sc;
	register dlregs *dladdr;
	register int unit, cc;
	int s;

	unit = DL_I2C(minor(tp->t_dev));
	sc = dl_cd.cd_devs[unit];

	s = spltty();
        if (tp->t_state & (TS_TIMEOUT|TS_BUSY|TS_TTSTOP))
                goto out;
        if (tp->t_outq.c_cc <= tp->t_lowat) {
                if (tp->t_state & TS_ASLEEP) {
                        tp->t_state &= ~TS_ASLEEP;
                        wakeup((caddr_t)&tp->t_outq);
                }
                selwakeup(&tp->t_wsel);
        }
        if (tp->t_outq.c_cc == 0)
                goto out;

	dladdr = sc->sc_addr;

	if (dladdr->dl_xcsr & DL_XCSR_TX_READY) {
		tp->t_state |= TS_BUSY;
		dladdr->u_xbuf.bytes.byte_lo = getc(&tp->t_outq);
	}
out:
	splx(s);
	return;
}

/*ARGSUSED*/
static int
dlparam(tp, t)
        register struct tty *tp;
        register struct termios *t;
{
	/*
	 * All this kind of stuff (speed, character format, whatever)
	 * is set by jumpers on the card.  Changing it is thus rather
	 * tricky for a mere device driver.
	 */
	return 0;
}

static void
dlbrk(sc, state)
	register struct dl_softc *sc;
	int state;
{
	register dlregs *dladdr;
	int s;
	dladdr = sc->sc_addr;
	s = spltty();
	if (state)
		dladdr->dl_xcsr |= DL_XCSR_TX_BREAK;
	else
		dladdr->dl_xcsr &= ~DL_XCSR_TX_BREAK;
	splx(s);
}
