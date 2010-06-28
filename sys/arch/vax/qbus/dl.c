/*	$OpenBSD: dl.c,v 1.11 2010/06/28 14:13:31 deraadt Exp $	*/
/*	$NetBSD: dl.c,v 1.11 2000/01/24 02:40:29 matt Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997  Ben Harris.  All rights reserved.
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/scb.h>

#include <arch/vax/qbus/ubavar.h>
#include <arch/vax/qbus/dlreg.h>

struct dl_softc {
	struct device	sc_dev;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	struct tty	*sc_tty;
};

static	int	dl_match(struct device *, struct cfdata *, void *);
static	void	dl_attach(struct device *, struct device *, void *);
static	void	dlrint(void *);
static	void	dlxint(void *);
static	void	dlstart(struct tty *);
static	int	dlparam(struct tty *, struct termios *);
static	void	dlbrk(struct dl_softc *, int);
struct	tty *	dltty(dev_t);
	int	dlopen(dev_t, int, int, struct proc *);
	int	dlclose(dev_t, int, int, struct proc *);
	int	dlread(dev_t, struct uio *, int);
	int	dlwrite(dev_t, struct uio *, int);
	int	dlioctl(dev_t, int, caddr_t, int, struct proc *);
	void	dlstop(struct tty *, int);

struct cfattach dl_ca = {
	sizeof(struct dl_softc), (cfmatch_t)dl_match, dl_attach
};

struct	cfdriver dl_cd = {
	NULL, "dl", DV_TTY
};

#define	DL_READ_WORD(reg) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, reg)
#define	DL_WRITE_WORD(reg, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, reg, val)
#define	DL_WRITE_BYTE(reg, val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val)

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dl_match (parent, cf, aux)
	struct device * parent;
	struct cfdata *cf;
	void *aux;
{
	struct uba_attach_args *ua = aux;

#ifdef DL_DEBUG
	printf("Probing for dl at %lo ... ", (long)ua->ua_iaddr);
#endif

	bus_space_write_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR, DL_XCSR_TXIE);
	if (bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR) !=
	    (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 1; XCSR = %.4b)\n", 
		    bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR), 
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

	bus_space_write_1(ua->ua_iot, ua->ua_ioh, DL_UBA_XBUFL, '\0');
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
	if (bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR) !=
	    (DL_XCSR_TXIE | DL_XCSR_TX_READY)) {
#ifdef DL_DEBUG
	        printf("failed (step 3; XCSR = %.4b)\n", 
		    bus_space_read_2(ua->ua_iot, ua->ua_ioh, DL_UBA_XCSR),
		    DL_XCSR_BITS);
#endif
		return 0;
	}


        /* What else do I need to do? */

	return 1;

}

static void
dl_attach (parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct dl_softc *sc = (void *)self;
	register struct uba_attach_args *ua = aux;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	
	/* Tidy up the device */

	DL_WRITE_WORD(DL_UBA_RCSR, DL_RCSR_RXIE);
	DL_WRITE_WORD(DL_UBA_XCSR, DL_XCSR_TXIE);

	/* Initialize our softc structure. Should be done in open? */
	
	sc->sc_tty = ttymalloc(0);

	/* Now register the TX & RX interrupt handlers */
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec    , dlxint, sc);
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec - 4, dlrint, sc);

	printf("\n");
}

/* Receiver Interrupt Handler */

static void
dlrint(arg)
	void *arg;
{
	struct	dl_softc *sc = arg;
	register struct tty *tp;
	register int cc;
	register unsigned c;

	if (DL_READ_WORD(DL_UBA_RCSR) & DL_RCSR_RX_DONE) {
	        c = DL_READ_WORD(DL_UBA_RBUF);
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
dlxint(arg)
	void *arg;
{
	struct dl_softc *sc = arg;
	register struct tty *tp;
	
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

	unit = minor(dev);

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
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		/* No modem control, so set CLOCAL. */
		tp->t_cflag = TTYDEF_CFLAG | CLOCAL;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		
		dlparam(tp, &tp->t_termios);
		ttsetwater(tp);
		
	} else if ((tp->t_state & TS_XCLUDE) && suser(p, 0) != 0)
		return EBUSY;

	return ((*linesw[tp->t_line].l_open)(dev, tp, p));
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

	unit = minor(dev);
	sc = dl_cd.cd_devs[unit];
      	tp = sc->sc_tty;

	(*linesw[tp->t_line].l_close)(tp, flag, p);

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

	unit = minor(dev);
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

	unit = minor(dev);
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

	unit = minor(dev);
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
	
	sc = dl_cd.cd_devs[minor(dev)];
	return sc->sc_tty;
}

void
dlstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register struct dl_softc *sc;
	int unit, s;

	unit = minor(tp->t_dev);
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
	register int unit;
	int s;

	unit = minor(tp->t_dev);
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


	if (DL_READ_WORD(DL_UBA_XCSR) & DL_XCSR_TX_READY) {
		tp->t_state |= TS_BUSY;
		DL_WRITE_BYTE(DL_UBA_XBUFL, getc(&tp->t_outq));
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
	int s = spltty();

	if (state) {
		DL_WRITE_WORD(DL_UBA_XCSR, DL_READ_WORD(DL_UBA_XCSR) |
		    DL_XCSR_TX_BREAK);
	} else {
		DL_WRITE_WORD(DL_UBA_XCSR, DL_READ_WORD(DL_UBA_XCSR) &
		    ~DL_XCSR_TX_BREAK);
	}
	splx(s);
}
