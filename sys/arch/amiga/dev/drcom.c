/*	$OpenBSD: drcom.c,v 1.1 1997/01/16 09:23:55 niklas Exp $	*/
/*	$NetBSD: drcom.c,v 1.2 1996/12/23 09:09:56 veego Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	@(#)drcom.c	7.5 (Berkeley) 5/16/91
 */

/*
 * COM driver, based on HP dca driver
 * uses National Semiconductor NS16450/NS16550AF UART
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>

#include <sys/conf.h>
#include <machine/conf.h>

#include <machine/cpu.h>

#include <amiga/amiga/device.h>
#include <amiga/amiga/drcustom.h>
#include <amiga/amiga/isr.h>

#include <amiga/dev/drisavar.h>
#include <amiga/dev/drcomreg.h>
#include <amiga/dev/drcomvar.h>

#include <dev/ic/ns16550reg.h>

#define	com_lcr	com_cfcr

#include "drcom.h"


#define	COM_IBUFSIZE	(2 * 512)
#define	COM_IHIGHWATER	((3 * COM_IBUFSIZE) / 4)

struct drcom_softc {
	struct device sc_dev;
	void *sc_ih;
	struct tty *sc_tty;

	int sc_overflows;
	int sc_floods;
	int sc_failures;
	int sc_errors;

	int sc_halt;

	int sc_iobase;

	bus_chipset_tag_t sc_bc;
	bus_io_handle_t sc_ioh;

	struct isr sc_isr;

	u_char sc_hwflags;
#define	COM_HW_NOIEN	0x01
#define	COM_HW_FIFO	0x02
#define	COM_HW_HAYESP	0x04
#define	COM_HW_CONSOLE	0x40
	u_char sc_swflags;
#define	COM_SW_SOFTCAR	0x01
#define	COM_SW_CLOCAL	0x02
#define	COM_SW_CRTSCTS	0x04
#define	COM_SW_MDMBUF	0x08
	u_char sc_msr, sc_mcr, sc_lcr, sc_ier;
	u_char sc_dtr;

	u_char *sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
	u_char sc_ibufs[2][COM_IBUFSIZE];

	u_char sc_iir;
};

void	drcomdiag		__P((void *));
int	drcomspeed	__P((long));
int	drcomparam	__P((struct tty *, struct termios *));
void	drcomstart	__P((struct tty *));
void	drcomsoft		__P((void *));
int	drcomintr		__P((void *));

struct consdev;
void	drcomcnprobe	__P((struct consdev *));
void	drcomcninit	__P((struct consdev *));
int	drcomcngetc	__P((dev_t));
void	drcomcnputc	__P((dev_t, int));
void	drcomcnpollc	__P((dev_t, int));

static u_char tiocm_xxx2mcr __P((int));

/*
 * XXX the following two cfattach structs should be different, and possibly
 * XXX elsewhere.
 */
int drcommatch __P((struct device *, struct cfdata *, void *));
void drcomattach __P((struct device *, struct device *, void *));

struct cfattach drcom_ca = {
	sizeof(struct drcom_softc), drcommatch, drcomattach
};

struct cfdriver drcom_cd = {
	NULL, "drcom", DV_TTY
};

void drcominit __P((bus_chipset_tag_t, bus_io_handle_t, int));

#ifdef COMCONSOLE
int	drcomdefaultrate = CONSPEED;		/* XXX why set default? */
#else
int	drcomdefaultrate = 1200 /*TTYDEF_SPEED*/;
#endif
int	drcomconsaddr;
int	drcomconsinit;
int	drcomconsattached;
bus_chipset_tag_t drcomconsbc;
bus_io_handle_t drcomconsioh;
tcflag_t drcomconscflag = TTYDEF_CFLAG;

int	drcommajor;
int	drcomsopen = 0;
int	drcomevents = 0;

#ifdef KGDB
#include <machine/remote-sl.h>
extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	COMUNIT(x)	(minor(x))

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

int
drcomspeed(speed)
	long speed;
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0)
		return 0;
	if (speed < 0)
		return -1;
	x = divrnd((COM_FREQ / 16), speed);
	if (x <= 0)
		return -1;
	err = divrnd((COM_FREQ / 16) * 1000, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
	return x;

#undef	divrnd(n, q)
}

int
drcommatch(parent, cfp, auxp)
	struct device *parent;
	struct cfdata *cfp;
	void *auxp;
{

	/* Exactly two of us live on the DraCo */

	if (is_draco() && matchname(auxp, "drcom") &&
	    (cfp->cf_unit >= 0) && (cfp->cf_unit < 2))
		return 1;

	return 0;
}

void
drcomattach(parent, self, auxp)
	struct device *parent, *self;
	void *auxp;
{
	struct drcom_softc *sc = (void *)self;
	int iobase;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;

	/*
	 * XXX should be broken out into functions for isa attach and
	 * XXX for drcommulti attach, with a helper function that contains
	 * XXX most of the interesting stuff.
	 */
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	bc = 0;
	iobase = self->dv_cfdata->cf_unit ? 0x2f8 : 0x3f8;

	if (iobase != drcomconsaddr) {
		(void)bus_io_map(bc, iobase, COM_NPORTS, &ioh);
	} else {
		ioh = drcomconsioh;
	}

	sc->sc_bc = bc;
	sc->sc_ioh = ioh;
	sc->sc_iobase = iobase;

	if (iobase == drcomconsaddr) {
		drcomconsattached = 1;

		/* 
		 * Need to reset baud rate, etc. of next print so reset
		 * drcomconsinit.  Also make sure console is always "hardwired".
		 */
		delay(1000);			/* wait for output to finish */
		drcomconsinit = 0;
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}


	/* look for a NS 16550AF UART with FIFOs */
	bus_io_write_1(bc, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_14);
	delay(100);
	if (ISSET(bus_io_read_1(bc, ioh, com_iir), IIR_FIFO_MASK) == IIR_FIFO_MASK)
		if (ISSET(bus_io_read_1(bc, ioh, com_fifo), FIFO_TRIGGER_14) == FIFO_TRIGGER_14) {
			SET(sc->sc_hwflags, COM_HW_FIFO);
			printf(": ns16550a, working fifo\n");
		} else
			printf(": ns16550, broken fifo\n");
	else
		printf(": ns8250 or ns16450, no fifo\n");
	bus_io_write_1(bc, ioh, com_fifo, 0);
#ifdef COM_HAYESP
	}
#endif

	if (amiga_ttyspl < (PSL_S|PSL_IPL5)) {
		printf("%s: raising amiga_ttyspl from 0x%x to 0x%x\n",
		    sc->sc_dev.dv_xname, amiga_ttyspl, PSL_S|PSL_IPL5);
		amiga_ttyspl = PSL_S|PSL_IPL5;
	}

	/* disable interrupts */
	(void)bus_io_read_1(bc, ioh, com_iir);
	bus_io_write_1(bc, ioh, com_ier, 0);
	bus_io_write_1(bc, ioh, com_mcr, 0);

	sc->sc_isr.isr_intr = drcomintr;
	sc->sc_isr.isr_arg = sc;
	sc->sc_isr.isr_ipl = 5;
	add_isr(&sc->sc_isr);

#ifdef KGDB
	if (kgdb_dev == makedev(drcommajor, unit)) {
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			drcominit(bc, ioh, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("%s: ", sc->sc_dev.dv_xname);
				kgdb_connect(1);
			} else
				printf("%s: kgdb enabled\n",
				    sc->sc_dev.dv_xname);
		}
	}
#endif

	/* XXX maybe move up some? */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
		printf("%s: console\n", sc->sc_dev.dv_xname);
}

int
drcomopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct drcom_softc *sc;
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	struct tty *tp;
	int s;
	int error = 0;
 
	if (unit >= drcom_cd.cd_ndevs)
		return ENXIO;
	sc = drcom_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (!sc->sc_tty) {
		tp = sc->sc_tty = ttymalloc();
		tty_attach(tp);
	} else
		tp = sc->sc_tty;

	tp->t_oproc = drcomstart;
	tp->t_param = drcomparam;
	tp->t_dev = dev;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			tp->t_cflag = drcomconscflag;
		else
			tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = drcomdefaultrate;

		s = spltty();

		drcomparam(tp, &tp->t_termios);
		ttsetwater(tp);

		if (drcomsopen++ == 0)
			timeout(drcomsoft, NULL, 1);

		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

		bc = sc->sc_bc;
		ioh = sc->sc_ioh;
#ifdef COM_HAYESP
		/* Setup the ESP board */
		if (ISSET(sc->sc_hwflags, COM_HW_HAYESP)) {
			bus_io_handle_t hayespioh = sc->sc_hayespioh;

			bus_io_write_1(bc, ioh, com_fifo,
			     FIFO_DMA_MODE|FIFO_ENABLE|
			     FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_8);

			/* Set 16550 drcompatibility mode */
			bus_io_write_1(bc, hayespioh, HAYESP_CMD1, HAYESP_SETMODE);
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2, 
			     HAYESP_MODE_FIFO|HAYESP_MODE_RTS|
			     HAYESP_MODE_SCALE);

			/* Set RTS/CTS flow control */
			bus_io_write_1(bc, hayespioh, HAYESP_CMD1, HAYESP_SETFLOWTYPE);
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2, HAYESP_FLOW_RTS);
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2, HAYESP_FLOW_CTS);

			/* Set flow control levels */
			bus_io_write_1(bc, hayespioh, HAYESP_CMD1, HAYESP_SETRXFLOW);
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2, 
			     HAYESP_HIBYTE(HAYESP_RXHIWMARK));
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXHIWMARK));
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2,
			     HAYESP_HIBYTE(HAYESP_RXLOWMARK));
			bus_io_write_1(bc, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXLOWMARK));
		} else
#endif
		if (ISSET(sc->sc_hwflags, COM_HW_FIFO))
			/* Set the FIFO threshold based on the receive speed. */
			bus_io_write_1(bc, ioh, com_fifo,
			    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST |
			    (tp->t_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
		/* flush any pending I/O */
		while (ISSET(bus_io_read_1(bc, ioh, com_lsr), LSR_RXRDY))
			(void) bus_io_read_1(bc, ioh, com_data);
		/* you turn me on, baby */
		sc->sc_mcr = MCR_DTR | MCR_RTS;
		if (!ISSET(sc->sc_hwflags, COM_HW_NOIEN))
			SET(sc->sc_mcr, MCR_IENABLE);
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		sc->sc_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
		bus_io_write_1(bc, ioh, com_ier, sc->sc_ier);

		sc->sc_msr = bus_io_read_1(bc, ioh, com_msr);
		if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) ||
		    ISSET(sc->sc_msr, MSR_DCD) || ISSET(tp->t_cflag, MDMBUF))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
	} else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;
	else
		s = spltty();

	/* wait for carrier if necessary */
	if (!ISSET(flag, O_NONBLOCK))
		while (!ISSET(tp->t_cflag, CLOCAL) &&
		    !ISSET(tp->t_state, TS_CARR_ON)) {
			SET(tp->t_state, TS_WOPEN);
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    ttopen, 0);
			if (error) {
				/* XXX should turn off chip if we're the
				   only waiter */
				splx(s);
				return error;
			}
		}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp);
}
 
int
drcomclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct drcom_softc *sc = drcom_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag);
	s = spltty();
	CLR(sc->sc_lcr, LCR_SBREAK);
	bus_io_write_1(bc, ioh, com_lcr, sc->sc_lcr);
	bus_io_write_1(bc, ioh, com_ier, 0);
	if (ISSET(tp->t_cflag, HUPCL) &&
	    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
		/* XXX perhaps only clear DTR */
		bus_io_write_1(bc, ioh, com_mcr, 0);
		bus_io_write_1(bc, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
	if (--drcomsopen == 0)
		untimeout(drcomsoft, NULL);
	splx(s);
	ttyclose(tp);
#ifdef notyet /* XXXX */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		ttyfree(tp);
		sc->sc_tty = 0;
	}
#endif
	return 0;
}
 
int
drcomread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct drcom_softc *sc = drcom_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
drcomwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct drcom_softc *sc = drcom_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
drcomtty(dev)
	dev_t dev;
{
	struct drcom_softc *sc = drcom_cd.cd_devs[COMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}
 
static u_char
tiocm_xxx2mcr(data)
	int data;
{
	u_char m = 0;

	if (ISSET(data, TIOCM_DTR))
		SET(m, MCR_DTR);
	if (ISSET(data, TIOCM_RTS))
		SET(m, MCR_RTS);
	return m;
}

int
drcomioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = COMUNIT(dev);
	struct drcom_softc *sc = drcom_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case TIOCSBRK:
		SET(sc->sc_lcr, LCR_SBREAK);
		bus_io_write_1(bc, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCCBRK:
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_io_write_1(bc, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCSDTR:
		SET(sc->sc_mcr, sc->sc_dtr);
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCCDTR:
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMSET:
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		SET(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMBIC:
		CLR(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMGET: {
		u_char m;
		int bits = 0;

		m = sc->sc_mcr;
		if (ISSET(m, MCR_DTR))
			SET(bits, TIOCM_DTR);
		if (ISSET(m, MCR_RTS))
			SET(bits, TIOCM_RTS);
		m = sc->sc_msr;
		if (ISSET(m, MSR_DCD))
			SET(bits, TIOCM_CD);
		if (ISSET(m, MSR_CTS))
			SET(bits, TIOCM_CTS);
		if (ISSET(m, MSR_DSR))
			SET(bits, TIOCM_DSR);
		if (ISSET(m, MSR_RI | MSR_TERI))
			SET(bits, TIOCM_RI);
		if (bus_io_read_1(bc, ioh, com_ier))
			SET(bits, TIOCM_LE);
		*(int *)data = bits;
		break;
	}
	case TIOCGFLAGS: {
		int driverbits, userbits = 0;

		driverbits = sc->sc_swflags;
		if (ISSET(driverbits, COM_SW_SOFTCAR))
			SET(userbits, TIOCFLAG_SOFTCAR);
		if (ISSET(driverbits, COM_SW_CLOCAL))
			SET(userbits, TIOCFLAG_CLOCAL);
		if (ISSET(driverbits, COM_SW_CRTSCTS))
			SET(userbits, TIOCFLAG_CRTSCTS);
		if (ISSET(driverbits, COM_SW_MDMBUF))
			SET(userbits, TIOCFLAG_MDMBUF);

		*(int *)data = userbits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0) {
			return(EPERM); 
		}

		userbits = *(int *)data;
		if (ISSET(userbits, TIOCFLAG_SOFTCAR) ||
		    ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			SET(driverbits, COM_SW_SOFTCAR);
		if (ISSET(userbits, TIOCFLAG_CLOCAL))
			SET(driverbits, COM_SW_CLOCAL);
		if (ISSET(userbits, TIOCFLAG_CRTSCTS))
			SET(driverbits, COM_SW_CRTSCTS);
		if (ISSET(userbits, TIOCFLAG_MDMBUF))
			SET(driverbits, COM_SW_MDMBUF);

		sc->sc_swflags = driverbits;
		break;
	}
	default:
		return ENOTTY;
	}

	return 0;
}

int
drcomparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct drcom_softc *sc = drcom_cd.cd_devs[COMUNIT(tp->t_dev)];
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int ospeed = drcomspeed(t->c_ospeed);
	u_char lcr;
	tcflag_t oldcflag;
	int s;

	/* check requested parameters */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return EINVAL;

	lcr = ISSET(sc->sc_lcr, LCR_SBREAK);

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		SET(lcr, LCR_5BITS);
		break;
	case CS6:
		SET(lcr, LCR_6BITS);
		break;
	case CS7:
		SET(lcr, LCR_7BITS);
		break;
	case CS8:
		SET(lcr, LCR_8BITS);
		break;
	}
	if (ISSET(t->c_cflag, PARENB)) {
		SET(lcr, LCR_PENAB);
		if (!ISSET(t->c_cflag, PARODD))
			SET(lcr, LCR_PEVEN);
	}
	if (ISSET(t->c_cflag, CSTOPB))
		SET(lcr, LCR_STOPB);

	sc->sc_lcr = lcr;

	s = spltty();

	if (ospeed == 0) {
		CLR(sc->sc_mcr, MCR_DTR);
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
	}

	/*
	 * Set the FIFO threshold based on the receive speed, if we are
	 * changing it.
	 */
#if 1
	if (tp->t_ispeed != t->c_ispeed) {
#else
	if (1) {
#endif
		if (ospeed != 0) {
			/*
			 * Make sure the transmit FIFO is empty before
			 * proceeding.  If we don't do this, some revisions
			 * of the UART will hang.  Interestingly enough,
			 * even if we do this will the last character is
			 * still being pushed out, they don't hang.  This
			 * seems good enough.
			 */
			while (ISSET(tp->t_state, TS_BUSY)) {
				int error;

				++sc->sc_halt;
				error = ttysleep(tp, &tp->t_outq,
				    TTOPRI | PCATCH, "drcomprm", 0);
				--sc->sc_halt;
				if (error) {
					splx(s);
					drcomstart(tp);
					return (error);
				}
			}

			bus_io_write_1(bc, ioh, com_lcr, lcr | LCR_DLAB);
			bus_io_write_1(bc, ioh, com_dlbl, ospeed);
			bus_io_write_1(bc, ioh, com_dlbh, ospeed >> 8);
			bus_io_write_1(bc, ioh, com_lcr, lcr);
			SET(sc->sc_mcr, MCR_DTR);
			bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
		} else
			bus_io_write_1(bc, ioh, com_lcr, lcr);

		if (!ISSET(sc->sc_hwflags, COM_HW_HAYESP) &&
		    ISSET(sc->sc_hwflags, COM_HW_FIFO))
			bus_io_write_1(bc, ioh, com_fifo,
			    FIFO_ENABLE |
			    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
	} else
		bus_io_write_1(bc, ioh, com_lcr, lcr);

	/* When not using CRTSCTS, RTS follows DTR. */
	if (!ISSET(t->c_cflag, CRTSCTS)) {
		if (ISSET(sc->sc_mcr, MCR_DTR)) {
			if (!ISSET(sc->sc_mcr, MCR_RTS)) {
				SET(sc->sc_mcr, MCR_RTS);
				bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
			}
		} else {
			if (ISSET(sc->sc_mcr, MCR_RTS)) {
				CLR(sc->sc_mcr, MCR_RTS);
				bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
			}
		}
		sc->sc_dtr = MCR_DTR | MCR_RTS;
	} else
		sc->sc_dtr = MCR_DTR;

	/* and copy to tty */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	oldcflag = tp->t_cflag;
	tp->t_cflag = t->c_cflag;

	/*
	 * If DCD is off and MDMBUF is changed, ask the tty layer if we should
	 * stop the device.
	 */
	if (!ISSET(sc->sc_msr, MSR_DCD) &&
	    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR) &&
	    ISSET(oldcflag, MDMBUF) != ISSET(tp->t_cflag, MDMBUF) &&
	    (*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
	}

	/* Just to be sure... */
	splx(s);
	drcomstart(tp);
	return 0;
}

void
drcomstart(tp)
	struct tty *tp;
{
	struct drcom_softc *sc = drcom_cd.cd_devs[COMUNIT(tp->t_dev)];
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		goto out;
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) ||
	    sc->sc_halt > 0)
		goto stopped;
	if (ISSET(tp->t_cflag, CRTSCTS) && !ISSET(sc->sc_msr, MSR_CTS))
		goto stopped;
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		if (tp->t_outq.c_cc == 0)
			goto stopped;
		selwakeup(&tp->t_wsel);
	}
	SET(tp->t_state, TS_BUSY);

	if (!ISSET(sc->sc_ier, IER_ETXRDY)) {
		SET(sc->sc_ier, IER_ETXRDY);
		bus_io_write_1(bc, ioh, com_ier, sc->sc_ier);
	}
#ifdef COM_HAYESP
	if (ISSET(sc->sc_hwflags, COM_HW_HAYESP)) {
		u_char buffer[1024], *cp = buffer;
		int n = q_to_b(&tp->t_outq, cp, sizeof buffer);
		do
			bus_io_write_1(bc, ioh, com_data, *cp++);
		while (--n);
	}
	else
#endif
	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		u_char buffer[16], *cp = buffer;
		int n = q_to_b(&tp->t_outq, cp, sizeof buffer);
		do {
			bus_io_write_1(bc, ioh, com_data, *cp++);
		} while (--n);
	} else
		bus_io_write_1(bc, ioh, com_data, getc(&tp->t_outq));
out:
	splx(s);
	return;
stopped:
	if (ISSET(sc->sc_ier, IER_ETXRDY)) {
		CLR(sc->sc_ier, IER_ETXRDY);
		bus_io_write_1(bc, ioh, com_ier, sc->sc_ier);
	}
	splx(s);
}

/*
 * Stop output on a line.
 */
void
drcomstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	splx(s);
}

void
drcomdiag(arg)
	void *arg;
{
	struct drcom_softc *sc = arg;
	int overflows, floods, failures;
	int s;

	s = spltty();
	sc->sc_errors = 0;
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	failures = sc->sc_failures;
	sc->sc_failures = 0;
	splx(s);

	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf overflow%s, %d uart failure%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s",
	    failures, failures == 1 ? "" : "s");
}

void
drcomsoft(arg)
	void *arg;
{
	int unit;
	struct drcom_softc *sc;
	struct tty *tp;
	register u_char *ibufp;
	u_char *ibufend;
	register int c;
	int s;
	static int lsrmap[8] = {
		0,      TTY_PE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE
	};

	s = spltty();
	if (drcomevents == 0) {
		splx(s);
		goto out;
	}
	drcomevents = 0;
	splx(s);

	for (unit = 0; unit < drcom_cd.cd_ndevs; unit++) {
		sc = drcom_cd.cd_devs[unit];
		if (sc == 0 || sc->sc_ibufp == sc->sc_ibuf)
			continue;

		tp = sc->sc_tty;

		s = spltty();

		ibufp = sc->sc_ibuf;
		ibufend = sc->sc_ibufp;

		if (ibufp == ibufend) {
			splx(s);
			continue;
		}

		sc->sc_ibufp = sc->sc_ibuf = (ibufp == sc->sc_ibufs[0]) ?
					     sc->sc_ibufs[1] : sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

		if (tp == 0 || !ISSET(tp->t_state, TS_ISOPEN)) {
			splx(s);
			continue;
		}

		if (ISSET(tp->t_cflag, CRTSCTS) &&
		    !ISSET(sc->sc_mcr, MCR_RTS)) {
			/* XXX */
			SET(sc->sc_mcr, MCR_RTS);
			bus_io_write_1(sc->sc_bc, sc->sc_ioh, com_mcr,
			    sc->sc_mcr);
		}

		splx(s);

		while (ibufp < ibufend) {
			c = *ibufp++;
			if (*ibufp & LSR_OE) {
				sc->sc_overflows++;
				if (sc->sc_errors++ == 0)
					timeout(drcomdiag, sc, 60 * hz);
			}
			/* This is ugly, but fast. */
			c |= lsrmap[(*ibufp++ & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
	}

out:
	timeout(drcomsoft, NULL, 1);
}

int
drcomintr(arg)
	void *arg;
{
	struct drcom_softc *sc = arg;
	bus_chipset_tag_t bc = sc->sc_bc;
	bus_io_handle_t ioh = sc->sc_ioh;
	struct tty *tp;
	u_char iir, lsr, data, msr, delta;
#ifdef COM_DEBUG
	int n;
	struct {
		u_char iir, lsr, msr;
	} iter[32];
#endif

#ifdef COM_DEBUG
	n = 0;
	iter[n].iir =
#endif
	iir = bus_io_read_1(bc, ioh, com_iir);
	if (ISSET(iir, IIR_NOPEND))
		return (0);

	tp = sc->sc_tty;

	for (;;) {
#ifdef COM_DEBUG
		iter[n].lsr =
#endif
		lsr = bus_io_read_1(bc, ioh, com_lsr);

		if (ISSET(lsr, LSR_RXRDY)) {
			register u_char *p = sc->sc_ibufp;

			drcomevents = 1;
			do {
				data = bus_io_read_1(bc, ioh, com_data);
				if (ISSET(lsr, LSR_BI)) {
#ifdef notdef
					printf("break %02x %02x %02x %02x\n",
					    sc->sc_msr, sc->sc_mcr, sc->sc_lcr,
					    sc->sc_dtr);
#endif
#ifdef DDB
					if (ISSET(sc->sc_hwflags,
					    COM_HW_CONSOLE)) {
						Debugger();
						goto next;
					}
#endif
				}
				if (p >= sc->sc_ibufend) {
					sc->sc_floods++;
					if (sc->sc_errors++ == 0)
						timeout(drcomdiag, sc, 60 * hz);
				} else {
					*p++ = data;
					*p++ = lsr;
					if (p == sc->sc_ibufhigh &&
					    ISSET(tp->t_cflag, CRTSCTS)) {
						/* XXX */
						CLR(sc->sc_mcr, MCR_RTS);
						bus_io_write_1(bc, ioh,
						    com_mcr, sc->sc_mcr);
					}
				}
#ifdef DDB
			next:
#endif
#ifdef COM_DEBUG
				if (++n >= 32)
					goto ohfudge;
				iter[n].lsr =
#endif
				lsr = bus_io_read_1(bc, ioh, com_lsr);
			} while (ISSET(lsr, LSR_RXRDY));

			sc->sc_ibufp = p;
		} else {
#ifdef COM_DEBUG
			if (ISSET(lsr, LSR_BI|LSR_FE|LSR_PE|LSR_OE))
				printf("weird lsr %02x\n", lsr);
#endif
			if ((iir & IIR_IMASK) == IIR_RXRDY) {
				sc->sc_failures++;
				if (sc->sc_errors++ == 0)
					timeout(drcomdiag, sc, 60 * hz);
				bus_io_write_1(bc, ioh, com_ier, 0);
				delay(10);
				bus_io_write_1(bc, ioh, com_ier, sc->sc_ier);
				iir = IIR_NOPEND;
				continue;
			}
		}

#ifdef COM_DEBUG
		iter[n].msr =
#endif
		msr = bus_io_read_1(bc, ioh, com_msr);

		if (msr != sc->sc_msr) {
			delta = msr ^ sc->sc_msr;
			sc->sc_msr = msr;
			if (ISSET(delta, MSR_DCD) &&
			    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR) &&
			    (*linesw[tp->t_line].l_modem)(tp, ISSET(msr, MSR_DCD)) == 0) {
				CLR(sc->sc_mcr, sc->sc_dtr);
				bus_io_write_1(bc, ioh, com_mcr, sc->sc_mcr);
			}
			if (ISSET(delta & msr, MSR_CTS) &&
			    ISSET(tp->t_cflag, CRTSCTS)) {
				/* the line is up and we want to do rts/cts flow control */
				(*linesw[tp->t_line].l_start)(tp);
			}
		}

		if (ISSET(lsr, LSR_TXRDY) && ISSET(tp->t_state, TS_BUSY)) {
			CLR(tp->t_state, TS_BUSY | TS_FLUSH);
			if (sc->sc_halt > 0)
				wakeup(&tp->t_outq);
			(*linesw[tp->t_line].l_start)(tp);
		}

#ifdef COM_DEBUG
		if (++n >= 32)
			goto ohfudge;
		iter[n].iir =
#endif
		iir = bus_io_read_1(bc, ioh, com_iir);
		if (ISSET(iir, IIR_NOPEND))
			return (1);
	}

#ifdef COM_DEBUG
ohfudge:
	printf("drcomintr: too many iterations");
	for (n = 0; n < 32; n++) {
		if ((n % 4) == 0)
			printf("\ndrcomintr: iter[%02d]", n);
		printf("  %02x %02x %02x", iter[n].iir, iter[n].lsr, iter[n].msr);
	}
	printf("\n");
	printf("drcomintr: msr %02x mcr %02x lcr %02x ier %02x\n",
	    sc->sc_msr, sc->sc_mcr, sc->sc_lcr, sc->sc_ier);
	printf("drcomintr: state %08x cc %d\n", sc->sc_tty->t_state,
	    sc->sc_tty->t_outq.c_cc);
	return (1);
#endif
}

/*
 * Following are all routines needed for COM to act as console
 */
#include <dev/cons.h>

void
drcomcnprobe(cp)
	struct consdev *cp;
{
	/* XXX NEEDS TO BE FIXED XXX */
	bus_chipset_tag_t bc = 0;
	bus_io_handle_t ioh;
	int found;

	if (bus_io_map(bc, CONADDR, COM_NPORTS, &ioh)) {
		cp->cn_pri = CN_DEAD;
		return;
	}
	found = 1/*drcomprobe1(bc, ioh, CONADDR)*/;
	bus_io_unmap(bc, ioh, COM_NPORTS);
	if (!found) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* locate the major number */
	for (drcommajor = 0; drcommajor < nchrdev; drcommajor++)
		if (cdevsw[drcommajor].d_open == drcomopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(drcommajor, CONUNIT);
#ifdef	COMCONSOLE
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
drcomcninit(cp)
	struct consdev *cp;
{

#if 0
	XXX NEEDS TO BE FIXED XXX
	drcomconsbc = ???;
#endif
	if (bus_io_map(drcomconsbc, CONADDR, COM_NPORTS, &drcomconsioh))
		panic("drcomcninit: mapping failed");

	drcominit(drcomconsbc, drcomconsioh, drcomdefaultrate);
	drcomconsaddr = CONADDR;
	drcomconsinit = 0;
}

void
drcominit(bc, ioh, rate)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
	int rate;
{
	int s = splhigh();
	u_char stat;

	bus_io_write_1(bc, ioh, com_lcr, LCR_DLAB);
	rate = drcomspeed(drcomdefaultrate);
	bus_io_write_1(bc, ioh, com_dlbl, rate);
	bus_io_write_1(bc, ioh, com_dlbh, rate >> 8);
	bus_io_write_1(bc, ioh, com_lcr, LCR_8BITS);
	bus_io_write_1(bc, ioh, com_ier, IER_ERXRDY | IER_ETXRDY);
	bus_io_write_1(bc, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1);
	bus_io_write_1(bc, ioh, com_mcr, MCR_DTR | MCR_RTS);
	DELAY(100);
	stat = bus_io_read_1(bc, ioh, com_iir);
	splx(s);
}

int
drcomcngetc(dev)
	dev_t dev;
{
	int s = splhigh();
	bus_chipset_tag_t bc = drcomconsbc;
	bus_io_handle_t ioh = drcomconsioh;
	u_char stat, c;

	while (!ISSET(stat = bus_io_read_1(bc, ioh, com_lsr), LSR_RXRDY))
		;
	c = bus_io_read_1(bc, ioh, com_data);
	stat = bus_io_read_1(bc, ioh, com_iir);
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
void
drcomcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s = splhigh();
	bus_chipset_tag_t bc = drcomconsbc;
	bus_io_handle_t ioh = drcomconsioh;
	u_char stat;
	register int timo;

#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (drcomconsinit == 0) {
		drcominit(bc, ioh, drcomdefaultrate);
		drcomconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (!ISSET(stat = bus_io_read_1(bc, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	bus_io_write_1(bc, ioh, com_data, c);
	/* wait for this transmission to drcomplete */
	timo = 1500000;
	while (!ISSET(stat = bus_io_read_1(bc, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = bus_io_read_1(bc, ioh, com_iir);
	splx(s);
}

void
drcomcnpollc(dev, on)
	dev_t dev;
	int on;
{

}
