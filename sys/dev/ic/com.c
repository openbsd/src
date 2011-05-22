/*	$OpenBSD: com.c,v 1.147 2011/05/22 22:36:53 drahn Exp $	*/
/*	$NetBSD: com.c,v 1.82.4.1 1996/06/02 09:08:00 mrg Exp $	*/

/*
 * Copyright (c) 1997 - 1999, Jason Downs.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
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
 *
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
 */

/*
 * COM driver, based on HP dca driver
 * uses National Semiconductor NS16450/NS16550AF UART
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/vnode.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/bus.h>
#if !defined(__sparc__) || defined(__sparc64__)
#include <machine/intr.h>
#endif

#if !defined(__sparc__) || defined(__sparc64__)
#define	COM_CONSOLE
#include <dev/cons.h>
#endif

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>
#define	com_lcr	com_cfcr

#ifdef COM_PXA2X0
#define com_isr 8
#define ISR_SEND	(ISR_RXPL | ISR_XMODE | ISR_XMITIR)
#define ISR_RECV	(ISR_RXPL | ISR_XMODE | ISR_RCVEIR)
#endif

#ifdef __zaurus__
#include <arch/zaurus/dev/zaurus_scoopvar.h>
#endif

cdev_decl(com);

static u_char tiocm_xxx2mcr(int);

void	compwroff(struct com_softc *);
void	cominit(bus_space_tag_t, bus_space_handle_t, int, int);
int	com_is_console(bus_space_tag_t, bus_addr_t);

struct cfdriver com_cd = {
	NULL, "com", DV_TTY
};

int	comdefaultrate = TTYDEF_SPEED;
#ifdef COM_PXA2X0
bus_addr_t comsiraddr;
#endif
#ifdef COM_CONSOLE
int	comconsfreq;
int	comconsrate = TTYDEF_SPEED;
int	comconsinit;
bus_addr_t comconsaddr = 0;
int	comconsattached;
bus_space_tag_t comconsiot;
bus_space_handle_t comconsioh;
int	comconsunit;
tcflag_t comconscflag = TTYDEF_CFLAG;
#endif

int	commajor;

#ifdef KGDB
#include <sys/kgdb.h>

bus_addr_t com_kgdb_addr;
bus_space_tag_t com_kgdb_iot;
bus_space_handle_t com_kgdb_ioh;

int    com_kgdb_getc(void *);
void   com_kgdb_putc(void *, int);
#endif /* KGDB */

#define	DEVUNIT(x)	(minor(x) & 0x7f)
#define	DEVCUA(x)	(minor(x) & 0x80)

int
comspeed(long freq, long speed)
{
#define	divrnd(n, q)	(((n)*2/(q)+1)/2)	/* divide and round off */

	int x, err;

	if (speed == 0)
		return 0;
	if (speed < 0)
		return -1;
	x = divrnd((freq / 16), speed);
	if (x <= 0)
		return -1;
	err = divrnd((quad_t)freq * 1000 / 16, speed * x) - 1000;
	if (err < 0)
		err = -err;
	if (err > COM_TOLERANCE)
		return -1;
	return x;

#undef	divrnd
}

#ifdef COM_CONSOLE
int
comprobe1(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int i, k;

	/* force access to id reg */
	bus_space_write_1(iot, ioh, com_lcr, 0);
	bus_space_write_1(iot, ioh, com_iir, 0);
	for (i = 0; i < 32; i++) {
		k = bus_space_read_1(iot, ioh, com_iir);
		if (k & 0x38) {
			bus_space_read_1(iot, ioh, com_data); /* cleanup */
		} else
			break;
	}
	if (i >= 32)
		return 0;

	return 1;
}
#endif

int
com_detach(struct device *self, int flags)
{
	struct com_softc *sc = (struct com_softc *)self;
	int maj, mn;

	sc->sc_swflags |= COM_SW_DEAD;

	/* Locate the major number. */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == comopen)
			break;

	/* Nuke the vnodes for any open instances. */
	mn = self->dv_unit;
	vdevgone(maj, mn, mn, VCHR);

	/* XXX a symbolic constant for the cua bit would be nicer. */
	mn |= 0x80;
	vdevgone(maj, mn, mn, VCHR);

	/* Detach and free the tty. */
	if (sc->sc_tty) {
		ttyfree(sc->sc_tty);
	}

	timeout_del(&sc->sc_dtr_tmo);
	timeout_del(&sc->sc_diag_tmo);
	softintr_disestablish(sc->sc_si);

	return (0);
}

int
com_activate(struct device *self, int act)
{
	struct com_softc *sc = (struct com_softc *)self;
	int s, rv = 0;

	s = spltty();
	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
#ifdef KGDB
		if (sc->sc_hwflags & (COM_HW_CONSOLE|COM_HW_KGDB)) {
#else
		if (sc->sc_hwflags & COM_HW_CONSOLE) {
#endif /* KGDB */
			rv = EBUSY;
			break;
		}

		if (sc->disable != NULL && sc->enabled != 0) {
			(*sc->disable)(sc);
			sc->enabled = 0;
		}
		break;
	}
	splx(s);
	return (rv);
}

int
comopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct tty *tp;
	int s;
	int error = 0;

	if (unit >= com_cd.cd_ndevs)
		return ENXIO;
	sc = com_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

#ifdef KGDB
	/*
	 * If this is the kgdb port, no other use is permitted.
	 */
	if (ISSET(sc->sc_hwflags, COM_HW_KGDB))
		return (EBUSY);
#endif /* KGDB */

	s = spltty();
	if (!sc->sc_tty) {
		tp = sc->sc_tty = ttymalloc(1000000);
	} else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = comstart;
	tp->t_param = comparam;
	tp->t_dev = dev;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
#ifdef COM_CONSOLE
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
			tp->t_cflag = comconscflag;
			tp->t_ispeed = tp->t_ospeed = comconsrate;
		} else
#endif
		{
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_ispeed = tp->t_ospeed = comdefaultrate;
		}
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;

		s = spltty();

		sc->sc_initialize = 1;
		comparam(tp, &tp->t_termios);
		ttsetwater(tp);

		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

		iot = sc->sc_iot;
		ioh = sc->sc_ioh;

		/*
		 * Wake up the sleepy heads.
		 */
		switch (sc->sc_uarttype) {
		case COM_UART_ST16650:
		case COM_UART_ST16650V2:
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
			bus_space_write_1(iot, ioh, com_ier, 0);
			bus_space_write_1(iot, ioh, com_efr, 0);
			bus_space_write_1(iot, ioh, com_lcr, 0);
			break;
		case COM_UART_TI16750:
			bus_space_write_1(iot, ioh, com_ier, 0);
			break;
		case COM_UART_PXA2X0:
			bus_space_write_1(iot, ioh, com_ier, IER_EUART);
			break;
		}

		if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
			u_int8_t fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST;
			u_int8_t lcr;

			if (tp->t_ispeed <= 1200)
				fifo |= FIFO_TRIGGER_1;
			else if (tp->t_ispeed <= 38400)
				fifo |= FIFO_TRIGGER_4;
			else
				fifo |= FIFO_TRIGGER_8;
			if (sc->sc_uarttype == COM_UART_TI16750) {
				fifo |= FIFO_ENABLE_64BYTE;
				lcr = bus_space_read_1(iot, ioh, com_lcr);
				bus_space_write_1(iot, ioh, com_lcr,
				    lcr | LCR_DLAB);
			}

			/*
			 * (Re)enable and drain FIFOs.
			 *
			 * Certain SMC chips cause problems if the FIFOs are
			 * enabled while input is ready. Turn off the FIFO
			 * if necessary to clear the input. Test the input
			 * ready bit after enabling the FIFOs to handle races
			 * between enabling and fresh input.
			 *
			 * Set the FIFO threshold based on the receive speed.
			 */
			for (;;) {
				bus_space_write_1(iot, ioh, com_fifo, 0);
				delay(100);
				(void) bus_space_read_1(iot, ioh, com_data);
				bus_space_write_1(iot, ioh, com_fifo, fifo |
				    FIFO_RCV_RST | FIFO_XMT_RST);
				delay(100);
				if(!ISSET(bus_space_read_1(iot, ioh,
				    com_lsr), LSR_RXRDY))
					break;
			}
			if (sc->sc_uarttype == COM_UART_TI16750)
				bus_space_write_1(iot, ioh, com_lcr, lcr);
		}

		/* Flush any pending I/O. */
		while (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
			(void) bus_space_read_1(iot, ioh, com_data);

		/* You turn me on, baby! */
		sc->sc_mcr = MCR_DTR | MCR_RTS;
		if (!ISSET(sc->sc_hwflags, COM_HW_NOIEN))
			SET(sc->sc_mcr, MCR_IENABLE);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		sc->sc_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
#ifdef COM_PXA2X0
		if (sc->sc_uarttype == COM_UART_PXA2X0)
			sc->sc_ier |= IER_EUART | IER_ERXTOUT;
#endif  
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

		sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);
		if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) || DEVCUA(dev) ||
		    ISSET(sc->sc_msr, MSR_DCD) || ISSET(tp->t_cflag, MDMBUF))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
#ifdef COM_PXA2X0
		if (sc->sc_uarttype == COM_UART_PXA2X0 &&
		    ISSET(sc->sc_hwflags, COM_HW_SIR)) {
			bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
#ifdef __zaurus__
			scoop_set_irled(1);
#endif
		}
#endif
	} else if (ISSET(tp->t_state, TS_XCLUDE) && suser(p, 0) != 0)
		return EBUSY;
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;		/* We go into CUA mode. */
	} else {
		/* tty (not cua) device; wait for carrier if necessary. */
		if (ISSET(flag, O_NONBLOCK)) {
			if (sc->sc_cua) {
				/* Opening TTY non-blocking... but the CUA is busy. */
				splx(s);
				return EBUSY;
			}
		} else {
			while (sc->sc_cua ||
			    (!ISSET(tp->t_cflag, CLOCAL) &&
				!ISSET(tp->t_state, TS_CARR_ON))) {
				SET(tp->t_state, TS_WOPEN);
				error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH, ttopen, 0);
				/*
				 * If TS_WOPEN has been reset, that means the cua device
				 * has been closed.  We don't want to fail in that case,
				 * so just go around again.
				 */
				if (error && ISSET(tp->t_state, TS_WOPEN)) {
					CLR(tp->t_state, TS_WOPEN);
					if (!sc->sc_cua && !ISSET(tp->t_state, TS_ISOPEN))
						compwroff(sc);
					splx(s);
					return error;
				}
			}
		}
	}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp, p);
}

int
comclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;
	int s;

#ifdef COM_CONSOLE
	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;
#endif

	if(sc->sc_swflags & COM_SW_DEAD)
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag, p);
	s = spltty();
	if (ISSET(tp->t_state, TS_WOPEN)) {
		/* tty device is waiting for carrier; drop dtr then re-raise */
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		timeout_add_sec(&sc->sc_dtr_tmo, 2);
	} else {
		/* no one else waiting; turn off the uart */
		compwroff(sc);
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);

#ifdef COM_CONSOLE
#ifdef notyet /* XXXX */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		ttyfree(tp);
		sc->sc_tty = 0;
	}
#endif
#endif
	return 0;
}

void
compwroff(struct com_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;

	CLR(sc->sc_lcr, LCR_SBREAK);
	bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
	bus_space_write_1(iot, ioh, com_ier, 0);
	if (ISSET(tp->t_cflag, HUPCL) &&
	    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
		/* XXX perhaps only clear DTR */
		sc->sc_mcr = 0;
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	}

	/*
	 * Turn FIFO off; enter sleep mode if possible.
	 */
	bus_space_write_1(iot, ioh, com_fifo, 0);
	delay(100);
	if (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		(void) bus_space_read_1(iot, ioh, com_data);
	delay(100);
	bus_space_write_1(iot, ioh, com_fifo,
			  FIFO_RCV_RST | FIFO_XMT_RST);

	switch (sc->sc_uarttype) {
	case COM_UART_ST16650:
	case COM_UART_ST16650V2:
		bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
		bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
		bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
		bus_space_write_1(iot, ioh, com_lcr, 0);
		break;
	case COM_UART_TI16750:
		bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
		break;
#ifdef COM_PXA2X0
	case COM_UART_PXA2X0:
		bus_space_write_1(iot, ioh, com_ier, 0);
#ifdef __zaurus__
		if (ISSET(sc->sc_hwflags, COM_HW_SIR))
			scoop_set_irled(0);
#endif
		break;
#endif
	}
}

void
com_resume(struct com_softc *sc)
{
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed;

	if (!tp || !ISSET(tp->t_state, TS_ISOPEN)) {
#ifdef COM_CONSOLE
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			cominit(iot, ioh, comconsrate, comconsfreq);
#endif
		return;
	}

	/*
	 * Wake up the sleepy heads.
	 */
	switch (sc->sc_uarttype) {
	case COM_UART_ST16650:
	case COM_UART_ST16650V2:
		bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
		bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
		bus_space_write_1(iot, ioh, com_ier, 0);
		bus_space_write_1(iot, ioh, com_efr, 0);
		bus_space_write_1(iot, ioh, com_lcr, 0);
		break;
	case COM_UART_TI16750:
		bus_space_write_1(iot, ioh, com_ier, 0);
		break;
	case COM_UART_PXA2X0:
		bus_space_write_1(iot, ioh, com_ier, IER_EUART);
		break;
	}

	ospeed = comspeed(sc->sc_frequency, tp->t_ospeed);

	if (ospeed != 0) {
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr | LCR_DLAB);
		bus_space_write_1(iot, ioh, com_dlbl, ospeed);
		bus_space_write_1(iot, ioh, com_dlbh, ospeed >> 8);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
	} else {
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
	}

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		u_int8_t fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST;
		u_int8_t lcr;

		if (tp->t_ispeed <= 1200)
			fifo |= FIFO_TRIGGER_1;
		else if (tp->t_ispeed <= 38400)
			fifo |= FIFO_TRIGGER_4;
		else
			fifo |= FIFO_TRIGGER_8;
		if (sc->sc_uarttype == COM_UART_TI16750) {
			fifo |= FIFO_ENABLE_64BYTE;
			lcr = bus_space_read_1(iot, ioh, com_lcr);
			bus_space_write_1(iot, ioh, com_lcr,
			    lcr | LCR_DLAB);
		}

		/*
		 * (Re)enable and drain FIFOs.
		 *
		 * Certain SMC chips cause problems if the FIFOs are
		 * enabled while input is ready. Turn off the FIFO
		 * if necessary to clear the input. Test the input
		 * ready bit after enabling the FIFOs to handle races
		 * between enabling and fresh input.
		 *
		 * Set the FIFO threshold based on the receive speed.
		 */
		for (;;) {
			bus_space_write_1(iot, ioh, com_fifo, 0);
			delay(100);
			(void) bus_space_read_1(iot, ioh, com_data);
			bus_space_write_1(iot, ioh, com_fifo, fifo |
			    FIFO_RCV_RST | FIFO_XMT_RST);
			delay(100);
			if(!ISSET(bus_space_read_1(iot, ioh,
			    com_lsr), LSR_RXRDY))
				break;
		}
		if (sc->sc_uarttype == COM_UART_TI16750)
			bus_space_write_1(iot, ioh, com_lcr, lcr);
	}

	/* Flush any pending I/O. */
	while (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		(void) bus_space_read_1(iot, ioh, com_data);

	/* You turn me on, baby! */
	bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0 &&
	    ISSET(sc->sc_hwflags, COM_HW_SIR)) {
		bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
#ifdef __zaurus__
		scoop_set_irled(1);
#endif
	}
#endif
}

void
com_raisedtr(void *arg)
{
	struct com_softc *sc = arg;

	SET(sc->sc_mcr, MCR_DTR | MCR_RTS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);
}

int
comread(dev_t dev, struct uio *uio, int flag)
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
comwrite(dev_t dev, struct uio *uio, int flag)
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
comtty(dev_t dev)
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

static u_char
tiocm_xxx2mcr(int data)
{
	u_char m = 0;

	if (ISSET(data, TIOCM_DTR))
		SET(m, MCR_DTR);
	if (ISSET(data, TIOCM_RTS))
		SET(m, MCR_RTS);
	return m;
}

int
comioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
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
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCCBRK:
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		break;
	case TIOCSDTR:
		SET(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCCDTR:
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMSET:
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		SET(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		break;
	case TIOCMBIC:
		CLR(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
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
		if (bus_space_read_1(iot, ioh, com_ier))
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
		if (ISSET(driverbits, COM_SW_PPS))
			SET(userbits, TIOCFLAG_PPS);

		*(int *)data = userbits;
		break;
	}
	case TIOCSFLAGS: {
		int userbits, driverbits = 0;

		error = suser(p, 0);
		if (error != 0)
			return(EPERM);

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
		if (ISSET(userbits, TIOCFLAG_PPS))
			SET(driverbits, COM_SW_PPS);

		sc->sc_swflags = driverbits;
		break;
	}
	default:
		return ENOTTY;
	}

	return 0;
}

/* already called at spltty */
int
comparam(struct tty *tp, struct termios *t)
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed = comspeed(sc->sc_frequency, t->c_ospeed);
	u_char lcr;
	tcflag_t oldcflag;

	/* Check requested parameters. */
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

	if (ospeed == 0) {
		CLR(sc->sc_mcr, MCR_DTR);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	}

	/*
	 * Set the FIFO threshold based on the receive speed, if we are
	 * changing it.
	 */
	if (sc->sc_initialize || (tp->t_ispeed != t->c_ispeed)) {
		sc->sc_initialize = 0;

		if (ospeed != 0) {
			/*
			 * Make sure the transmit FIFO is empty before
			 * proceeding.  If we don't do this, some revisions
			 * of the UART will hang.  Interestingly enough,
			 * even if we do this while the last character is
			 * still being pushed out, they don't hang.  This
			 * seems good enough.
			 */
			while (ISSET(tp->t_state, TS_BUSY)) {
				int error;

				++sc->sc_halt;
				error = ttysleep(tp, &tp->t_outq,
				    TTOPRI | PCATCH, "comprm", 0);
				--sc->sc_halt;
				if (error) {
					comstart(tp);
					return (error);
				}
			}

			bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
			bus_space_write_1(iot, ioh, com_dlbl, ospeed);
			bus_space_write_1(iot, ioh, com_dlbh, ospeed >> 8);
			bus_space_write_1(iot, ioh, com_lcr, lcr);
			SET(sc->sc_mcr, MCR_DTR);
			bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		} else
			bus_space_write_1(iot, ioh, com_lcr, lcr);

		if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
			if (sc->sc_uarttype == COM_UART_TI16750) {
				bus_space_write_1(iot, ioh, com_lcr,
				    lcr | LCR_DLAB);
				bus_space_write_1(iot, ioh, com_fifo,
				    FIFO_ENABLE | FIFO_ENABLE_64BYTE |
				    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
				bus_space_write_1(iot, ioh, com_lcr, lcr);
			} else
				bus_space_write_1(iot, ioh, com_fifo,
				    FIFO_ENABLE |
				    (t->c_ispeed <= 1200 ? FIFO_TRIGGER_1 : FIFO_TRIGGER_8));
		}
	} else
		bus_space_write_1(iot, ioh, com_lcr, lcr);

	/* When not using CRTSCTS, RTS follows DTR. */
	if (!ISSET(t->c_cflag, CRTSCTS)) {
		if (ISSET(sc->sc_mcr, MCR_DTR)) {
			if (!ISSET(sc->sc_mcr, MCR_RTS)) {
				SET(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
			}
		} else {
			if (ISSET(sc->sc_mcr, MCR_RTS)) {
				CLR(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
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
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
	}

	/* Just to be sure... */
	comstart(tp);
	return 0;
}

void
comstart(struct tty *tp)
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		goto out;
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) || sc->sc_halt > 0)
		goto stopped;
	if (ISSET(tp->t_cflag, CRTSCTS) && !ISSET(sc->sc_msr, MSR_CTS))
		goto stopped;
	ttwakeupwr(tp);
	if (tp->t_outq.c_cc == 0)
		goto stopped;
	SET(tp->t_state, TS_BUSY);

#ifdef COM_PXA2X0
	/* Enable transmitter slow infrared mode. */
	if (sc->sc_uarttype == COM_UART_PXA2X0 &&
	    ISSET(sc->sc_hwflags, COM_HW_SIR))
		bus_space_write_1(iot, ioh, com_isr, ISR_SEND);
#endif

	/* Enable transmit completion interrupts. */
	if (!ISSET(sc->sc_ier, IER_ETXRDY)) {
		SET(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
	}

	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
		u_char buffer[128];	/* largest fifo */
		int i, n;

		n = q_to_b(&tp->t_outq, buffer,
		    min(sc->sc_fifolen, sizeof buffer));
		for (i = 0; i < n; i++) {
			bus_space_write_1(iot, ioh, com_data, buffer[i]);
		}
		bzero(buffer, n);
	} else if (tp->t_outq.c_cc != 0)
		bus_space_write_1(iot, ioh, com_data, getc(&tp->t_outq));
out:
	splx(s);
	return;
stopped:
	if (ISSET(sc->sc_ier, IER_ETXRDY)) {
		CLR(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
#ifdef COM_PXA2X0
		if (sc->sc_uarttype == COM_UART_PXA2X0 &&
		    ISSET(sc->sc_hwflags, COM_HW_SIR)) {
			int timo;

			/* Wait for empty transmit shift register. */
			timo = 20000;
			while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
			    LSR_TSRE) && --timo)
				delay(1);

			/* Enable receiver slow infrared mode. */
			bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
		}
#endif
	}
	splx(s);
}

/*
 * Stop output on a line.
 */
int
comstop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY))
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	splx(s);
	return 0;
}

void
comdiag(void *arg)
{
	struct com_softc *sc = arg;
	int overflows, floods;
	int s;

	s = spltty();
	sc->sc_errors = 0;
	overflows = sc->sc_overflows;
	sc->sc_overflows = 0;
	floods = sc->sc_floods;
	sc->sc_floods = 0;
	splx(s);
	log(LOG_WARNING, "%s: %d silo overflow%s, %d ibuf overflow%s\n",
	    sc->sc_dev.dv_xname,
	    overflows, overflows == 1 ? "" : "s",
	    floods, floods == 1 ? "" : "s");
}

void
comsoft(void *arg)
{
	struct com_softc *sc = (struct com_softc *)arg;
	struct tty *tp;
	u_char *ibufp;
	u_char *ibufend;
	int c;
	int s;
	static int lsrmap[8] = {
		0,      TTY_PE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE,
		TTY_FE, TTY_PE|TTY_FE
	};

	if (sc == NULL || sc->sc_ibufp == sc->sc_ibuf)
		return;

	tp = sc->sc_tty;

	s = spltty();

	ibufp = sc->sc_ibuf;
	ibufend = sc->sc_ibufp;

	if (ibufp == ibufend) {
		splx(s);
		return;
	}

	sc->sc_ibufp = sc->sc_ibuf = (ibufp == sc->sc_ibufs[0]) ?
				     sc->sc_ibufs[1] : sc->sc_ibufs[0];
	sc->sc_ibufhigh = sc->sc_ibuf + COM_IHIGHWATER;
	sc->sc_ibufend = sc->sc_ibuf + COM_IBUFSIZE;

	if (tp == NULL || !ISSET(tp->t_state, TS_ISOPEN)) {
		splx(s);
		return;
	}

	if (ISSET(tp->t_cflag, CRTSCTS) &&
	    !ISSET(sc->sc_mcr, MCR_RTS)) {
		/* XXX */
		SET(sc->sc_mcr, MCR_RTS);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr,
		    sc->sc_mcr);
	}

	splx(s);

	while (ibufp < ibufend) {
		c = *ibufp++;
		if (ISSET(*ibufp, LSR_OE)) {
			sc->sc_overflows++;
			if (sc->sc_errors++ == 0)
				timeout_add_sec(&sc->sc_diag_tmo, 60);
		}
		/* This is ugly, but fast. */
		c |= lsrmap[(*ibufp++ & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
		(*linesw[tp->t_line].l_rint)(c, tp);
	}
}

#ifdef KGDB

/*
 * If a line break is set, or data matches one of the characters
 * gdb uses to signal a connection, then start up kgdb. Just gobble
 * any other data. Done in a stand alone function because comintr
 * does tty stuff and we don't have one.
 */

int
kgdbintr(void *arg)
{
	struct com_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_char lsr, data, msr, delta;

	if (!ISSET(sc->sc_hwflags, COM_HW_KGDB))
		return(0);

	for (;;) {
		lsr = bus_space_read_1(iot, ioh, com_lsr);
		if (ISSET(lsr, LSR_RXRDY)) {
			do {
				data = bus_space_read_1(iot, ioh, com_data);
				if (data == 3 || data == '$' || data == '+' ||
				    ISSET(lsr, LSR_BI)) {
					kgdb_connect(1);
					data = 0;
				}
				lsr = bus_space_read_1(iot, ioh, com_lsr);
			} while (ISSET(lsr, LSR_RXRDY));

		}
		if (ISSET(lsr, LSR_BI|LSR_FE|LSR_PE|LSR_OE))
			printf("weird lsr %02x\n", lsr);

		msr = bus_space_read_1(iot, ioh, com_msr);

		if (msr != sc->sc_msr) {
			delta = msr ^ sc->sc_msr;
			sc->sc_msr = msr;
			if (ISSET(delta, MSR_DCD)) {
				if (!ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
					CLR(sc->sc_mcr, sc->sc_dtr);
					bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
				}
			}
		}
		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return (1);
	}
}
#endif /* KGDB */

int
comintr(void *arg)
{
	struct com_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp;
	u_char lsr, data, msr, delta;

	if (!sc->sc_tty)
		return (0);		/* Can't do squat. */

	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
		return (0);

	tp = sc->sc_tty;

	for (;;) {
		lsr = bus_space_read_1(iot, ioh, com_lsr);

		if (ISSET(lsr, LSR_RXRDY)) {
			u_char *p = sc->sc_ibufp;

			softintr_schedule(sc->sc_si);
			do {
				data = bus_space_read_1(iot, ioh, com_data);
				if (ISSET(lsr, LSR_BI)) {
#if defined(COM_CONSOLE) && defined(DDB)
					if (ISSET(sc->sc_hwflags,
					    COM_HW_CONSOLE)) {
						if (db_console)
							Debugger();
						goto next;
					}
#endif
					data = 0;
				}
				if (p >= sc->sc_ibufend) {
					sc->sc_floods++;
					if (sc->sc_errors++ == 0)
						timeout_add_sec(&sc->sc_diag_tmo, 60);
				} else {
					*p++ = data;
					*p++ = lsr;
					if (p == sc->sc_ibufhigh &&
					    ISSET(tp->t_cflag, CRTSCTS)) {
						/* XXX */
						CLR(sc->sc_mcr, MCR_RTS);
						bus_space_write_1(iot, ioh, com_mcr,
						    sc->sc_mcr);
					}
				}
#if defined(COM_CONSOLE) && defined(DDB)
			next:
#endif
				lsr = bus_space_read_1(iot, ioh, com_lsr);
			} while (ISSET(lsr, LSR_RXRDY));

			sc->sc_ibufp = p;
		}
		msr = bus_space_read_1(iot, ioh, com_msr);

		if (msr != sc->sc_msr) {
			delta = msr ^ sc->sc_msr;

			ttytstamp(tp, sc->sc_msr & MSR_CTS, msr & MSR_CTS,
			    sc->sc_msr & MSR_DCD, msr & MSR_DCD);

			sc->sc_msr = msr;
			if (ISSET(delta, MSR_DCD)) {
				if (!ISSET(sc->sc_swflags, COM_SW_SOFTCAR) &&
				    (*linesw[tp->t_line].l_modem)(tp, ISSET(msr, MSR_DCD)) == 0) {
					CLR(sc->sc_mcr, sc->sc_dtr);
					bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
				}
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

#ifdef COM_PXA2X0
		if (sc->sc_uarttype == COM_UART_PXA2X0 &&
		    ISSET(sc->sc_hwflags, COM_HW_SIR) &&
		    ISSET(lsr, LSR_TXRDY) && ISSET(lsr, LSR_TSRE))
			bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
#endif

		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return (1);
	}
}

/*
 * The following functions are polled getc and putc routines, shared
 * by the console and kgdb glue.
 */

int
com_common_getc(bus_space_tag_t iot, bus_space_handle_t ioh)
{
	int s = splhigh();
	u_char stat, c;

#ifdef COM_PXA2X0
	if (com_is_console(iot, comsiraddr))
		bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
#endif

	/* Block until a character becomes available. */
	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		continue;

	c = bus_space_read_1(iot, ioh, com_data);

	/* Clear any interrupts generated by this transmission. */
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
	return (c);
}

void
com_common_putc(bus_space_tag_t iot, bus_space_handle_t ioh, int c)
{
	int s = spltty();
	int timo;

	/* Wait for any pending transmission to finish. */
	timo = 2000;
	while (!ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		delay(1);

#ifdef COM_PXA2X0
	if (com_is_console(iot, comsiraddr))
		bus_space_write_1(iot, ioh, com_isr, ISR_SEND);
#endif
	bus_space_write_1(iot, ioh, com_data, (u_int8_t)(c & 0xff));
	bus_space_barrier(iot, ioh, 0, COM_NPORTS,
	    (BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE));

	/* Wait for this transmission to complete. */
	timo = 2000;
	while (!ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		delay(1);

#ifdef COM_PXA2X0
	if (com_is_console(iot, comsiraddr)) {
		/* Wait for transmit shift register to become empty. */
		timo = 20000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_TSRE)
		    && --timo)
			delay(1);

		bus_space_write_1(iot, ioh, com_isr, ISR_RECV);
	}
#endif

	splx(s);
}

void
cominit(bus_space_tag_t iot, bus_space_handle_t ioh, int rate, int frequency)
{
	int s = splhigh();
	u_char stat;

	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	rate = comspeed(frequency, rate); /* XXX not comdefaultrate? */
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS);
	bus_space_write_1(iot, ioh, com_mcr, MCR_DTR | MCR_RTS);
#ifdef COM_PXA2X0
	/* XXX */
	bus_space_write_1(iot, ioh, com_ier, IER_EUART);  /* Make sure they are off */
#else
	bus_space_write_1(iot, ioh, com_ier, 0);  /* Make sure they are off */
#endif
	bus_space_write_1(iot, ioh, com_fifo,
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1);
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

#ifdef COM_CONSOLE
void  
comcnprobe(struct consdev *cp)
{
	bus_space_handle_t ioh;
	int found;

	if (comconsaddr == 0)
		return;

	if (bus_space_map(comconsiot, comconsaddr, COM_NPORTS, 0, &ioh))
		return;
	found = comprobe1(comconsiot, ioh);
	bus_space_unmap(comconsiot, ioh, COM_NPORTS);
	if (!found)
		return;

	/* Locate the major number. */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	/* Initialize required fields. */
	cp->cn_dev = makedev(commajor, comconsunit);
#if defined(COMCONSOLE) || !defined(__amd64__)
	cp->cn_pri = CN_HIGHPRI;
#else
	cp->cn_pri = CN_LOWPRI;
#endif
}

void
comcninit(struct consdev *cp)
{
	if (bus_space_map(comconsiot, comconsaddr, COM_NPORTS, 0, &comconsioh))
		panic("comcninit: mapping failed");

	if (comconsfreq == 0)
		comconsfreq = COM_FREQ;

	cominit(comconsiot, comconsioh, comconsrate, comconsfreq);
	comconsinit = 0;
}

int
comcnattach(bus_space_tag_t iot, bus_addr_t iobase, int rate, int frequency, tcflag_t cflag)
{
	static struct consdev comcons = {
		NULL, NULL, comcngetc, comcnputc, comcnpollc, NULL,
		NODEV, CN_LOWPRI
	};

#ifndef __sparc64__
	if (bus_space_map(iot, iobase, COM_NPORTS, 0, &comconsioh))
		return ENOMEM;
#endif

	cominit(iot, comconsioh, rate, frequency);

	cn_tab = &comcons;

	comconsiot = iot;
	comconsaddr = iobase;
	comconscflag = cflag;
	comconsfreq = frequency;
	comconsrate = rate;

	return (0);
}

int
comcngetc(dev_t dev)
{
	return (com_common_getc(comconsiot, comconsioh));
}

/*
 * Console kernel output character routine.
 */
void
comcnputc(dev_t dev, int c)
{
	com_common_putc(comconsiot, comconsioh, c);
}

void
comcnpollc(dev_t dev, int on)
{

}
#endif	/* COM_CONSOLE */

#ifdef KGDB
int
com_kgdb_attach(bus_space_tag_t iot, bus_addr_t iobase, int rate,
    int frequency, tcflag_t cflag)
{
#ifdef COM_CONSOLE
	if (iot == comconsiot && iobase == comconsaddr) {
		return (EBUSY); /* cannot share with console */
	}
#endif

	com_kgdb_iot = iot;
	com_kgdb_addr = iobase;

	if (bus_space_map(com_kgdb_iot, com_kgdb_addr, COM_NPORTS, 0,
	    &com_kgdb_ioh))
		panic("com_kgdb_attach: mapping failed");

	/* XXX We currently don't respect KGDBMODE? */
	cominit(com_kgdb_iot, com_kgdb_ioh, rate, frequency);

	kgdb_attach(com_kgdb_getc, com_kgdb_putc, NULL);
	kgdb_dev = 123; /* unneeded, only to satisfy some tests */

	return (0);
}

/* ARGSUSED */
int
com_kgdb_getc(void *arg)
{

	return (com_common_getc(com_kgdb_iot, com_kgdb_ioh));
}

/* ARGSUSED */
void
com_kgdb_putc(void *arg, int c)
{

	return (com_common_putc(com_kgdb_iot, com_kgdb_ioh, c));
}
#endif /* KGDB */

#ifdef COM_PXA2X0
int
com_is_console(bus_space_tag_t iot, bus_addr_t iobase)
{

	if (comconsiot == iot && comconsaddr == iobase)
		return (1);
#ifdef KGDB
	else if (com_kgdb_iot == iot && com_kgdb_addr == iobase)
		return (1);
#endif
	return (0);
}
#endif /* COM_PXA2X0 */

void	com_enable_debugport(struct com_softc *);
void	com_fifo_probe(struct com_softc *);

#if defined(COM_CONSOLE) || defined(KGDB)
void
com_enable_debugport(struct com_softc *sc)
{
	int s;

	/* Turn on line break interrupt, set carrier. */
	s = splhigh();
#ifdef KGDB
	SET(sc->sc_ier, IER_ERXRDY);
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		sc->sc_ier |= IER_EUART | IER_ERXTOUT;
#endif
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_ier, sc->sc_ier);
#endif
	SET(sc->sc_mcr, MCR_DTR | MCR_RTS | MCR_IENABLE);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr, sc->sc_mcr);

	splx(s);
}
#endif	/* COM_CONSOLE || KGDB */

void
com_attach_subr(struct com_softc *sc)
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	u_int8_t lcr;

	sc->sc_ier = 0;
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		sc->sc_ier |= IER_EUART;
#endif
	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

#ifdef COM_CONSOLE
	if (sc->sc_iot == comconsiot && sc->sc_iobase == comconsaddr) {
		comconsattached = 1;
		delay(10000);			/* wait for output to finish */
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}
#endif

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);
	bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	/*
	 * Skip specific probes if attachment code knows it already.
	 */
	if (sc->sc_uarttype == COM_UART_UNKNOWN)
		switch (bus_space_read_1(iot, ioh, com_iir) >> 6) {
		case 0:
			sc->sc_uarttype = COM_UART_16450;
			break;
		case 2:
			sc->sc_uarttype = COM_UART_16550;
			break;
		case 3:
			sc->sc_uarttype = COM_UART_16550A;
			break;
		default:
			sc->sc_uarttype = COM_UART_UNKNOWN;
			break;
		}

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for ST16650s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		if (bus_space_read_1(iot, ioh, com_efr) == 0) {
			sc->sc_uarttype = COM_UART_ST16650;
		} else {
			bus_space_write_1(iot, ioh, com_lcr, LCR_EFR);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}

#if 0	/* until com works with large FIFOs */
	if (sc->sc_uarttype == COM_UART_ST16650V2) { /* Probe for XR16850s */
		u_int8_t dlbl, dlbh;

		/* Enable latch access and get the current values. */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		dlbl = bus_space_read_1(iot, ioh, com_dlbl);
		dlbh = bus_space_read_1(iot, ioh, com_dlbh);

		/* Zero out the latch divisors */
		bus_space_write_1(iot, ioh, com_dlbl, 0);
		bus_space_write_1(iot, ioh, com_dlbh, 0);

		if (bus_space_read_1(iot, ioh, com_dlbh) == 0x10) {
			sc->sc_uarttype = COM_UART_XR16850;
			sc->sc_uartrev = bus_space_read_1(iot, ioh, com_dlbl);
		}

		/* Reset to original. */
		bus_space_write_1(iot, ioh, com_dlbl, dlbl);
		bus_space_write_1(iot, ioh, com_dlbh, dlbh);
	}
#endif

	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for TI16750s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_ENABLE | FIFO_ENABLE_64BYTE);
		if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 7) {
#if 0
			bus_space_write_1(iot, ioh, com_lcr, 0);
			if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 6)
#endif
				sc->sc_uarttype = COM_UART_TI16750;
		}
		bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	}

	/* Reset the LCR (latch access is probably enabled). */
	bus_space_write_1(iot, ioh, com_lcr, lcr);
	if (sc->sc_uarttype == COM_UART_16450) { /* Probe for 8250 */
		u_int8_t scr0, scr1, scr2;

		scr0 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0xa5);
		scr1 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, 0x5a);
		scr2 = bus_space_read_1(iot, ioh, com_scratch);
		bus_space_write_1(iot, ioh, com_scratch, scr0);

		if ((scr1 != 0xa5) || (scr2 != 0x5a))
			sc->sc_uarttype = COM_UART_8250;
	}

	/*
	 * Print UART type and initialize ourself.
	 */
	switch (sc->sc_uarttype) {
	case COM_UART_UNKNOWN:
		printf(": unknown uart\n");
		break;
	case COM_UART_8250:
		printf(": ns8250, no fifo\n");
		break;
	case COM_UART_16450:
		printf(": ns16450, no fifo\n");
		break;
	case COM_UART_16550:
		printf(": ns16550, no working fifo\n");
		break;
	case COM_UART_16550A:
		if (sc->sc_fifolen == 0)
			sc->sc_fifolen = 16;
		printf(": ns16550a, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
#ifdef COM_PXA2X0
	case COM_UART_PXA2X0:
		printf(": pxa2x0, 32 byte fifo");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		if (sc->sc_iobase == comsiraddr) {
			SET(sc->sc_hwflags, COM_HW_SIR);
			printf(" (SIR)");
		}
		printf("\n");
		break;
#endif
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		if (sc->sc_fifolen == 0)
			sc->sc_fifolen = 32;
		printf(": st16650, %d byte fifo\n", sc->sc_fifolen);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		break;
	case COM_UART_ST16C654:
		printf(": st16c654, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
	case COM_UART_TI16750:
		printf(": ti16750, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
#if 0
	case COM_UART_XR16850:
		printf(": xr16850 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
#ifdef COM_UART_OX16C950
	case COM_UART_OX16C950:
		printf(": ox16c950 (rev %d), 128 byte fifo\n", sc->sc_uartrev);
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 128;
		break;
#endif
#endif
	default:
		panic("comattach: bad fifo type");
	}

#ifdef KGDB
	/*
	 * Allow kgdb to "take over" this port.  If this is
	 * the kgdb device, it has exclusive use.
	 */

	if (iot == com_kgdb_iot && sc->sc_iobase == com_kgdb_addr &&
	    !ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		printf("%s: kgdb\n", sc->sc_dev.dv_xname);
		SET(sc->sc_hwflags, COM_HW_KGDB);
	}
#endif /* KGDB */

#if defined(COM_CONSOLE) || defined(KGDB)
	if (!ISSET(sc->sc_hwflags, COM_HW_CONSOLE|COM_HW_KGDB))
#endif
		com_fifo_probe(sc);

	if (sc->sc_fifolen == 0) {
		CLR(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 1;
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	if (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);

	sc->sc_mcr = 0;
	bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);

#ifdef COM_CONSOLE
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		int maj;

		/* locate the major number */
		for (maj = 0; maj < nchrdev; maj++)
			if (cdevsw[maj].d_open == comopen)
				break;

		if (maj < nchrdev && cn_tab->cn_dev == NODEV)
			cn_tab->cn_dev = makedev(maj, sc->sc_dev.dv_unit);

		printf("%s: console\n", sc->sc_dev.dv_xname);
	}
#endif

	timeout_set(&sc->sc_diag_tmo, comdiag, sc);
	timeout_set(&sc->sc_dtr_tmo, com_raisedtr, sc);
	sc->sc_si = softintr_establish(IPL_TTY, comsoft, sc);
	if (sc->sc_si == NULL)
		panic("%s: can't establish soft interrupt",
		    sc->sc_dev.dv_xname);

	/*
	 * If there are no enable/disable functions, assume the device
	 * is always enabled.
	 */
	if (!sc->enable)
		sc->enabled = 1;

#if defined(COM_CONSOLE) || defined(KGDB)
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE|COM_HW_KGDB))
		com_enable_debugport(sc);
#endif
}

void
com_fifo_probe(struct com_softc *sc)
{
	bus_space_handle_t ioh = sc->sc_ioh;
	bus_space_tag_t iot = sc->sc_iot;
	u_int8_t fifo, ier;
	int timo, len;

	if (!ISSET(sc->sc_hwflags, COM_HW_FIFO))
		return;

	ier = 0;
#ifdef COM_PXA2X0
	if (sc->sc_uarttype == COM_UART_PXA2X0)
		ier |= IER_EUART;
#endif
	bus_space_write_1(iot, ioh, com_ier, ier);
	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	bus_space_write_1(iot, ioh, com_dlbl, 3);
	bus_space_write_1(iot, ioh, com_dlbh, 0);
	bus_space_write_1(iot, ioh, com_lcr, LCR_PNONE | LCR_8BITS);
	bus_space_write_1(iot, ioh, com_mcr, MCR_LOOPBACK);

	fifo = FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST;
	if (sc->sc_uarttype == COM_UART_TI16750)
		fifo |= FIFO_ENABLE_64BYTE;

	bus_space_write_1(iot, ioh, com_fifo, fifo);

	for (len = 0; len < 256; len++) {
		bus_space_write_1(iot, ioh, com_data, (len + 1));
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
		    LSR_TXRDY) && --timo)
			delay(1);
		if (!timo)
			break;
	}

	delay(100);

	for (len = 0; len < 256; len++) {
		timo = 2000;
		while (!ISSET(bus_space_read_1(iot, ioh, com_lsr),
		    LSR_RXRDY) && --timo)
			delay(1);
		if (!timo || bus_space_read_1(iot, ioh, com_data) != (len + 1))
			break;
	}

	/* For safety, always use the smaller value. */
	if (sc->sc_fifolen > len) {
		printf("%s: probed fifo depth: %d bytes\n",
		    sc->sc_dev.dv_xname, len);
		sc->sc_fifolen = len;
	}
}
