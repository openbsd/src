/*	$OpenBSD: com.c,v 1.39 1998/08/07 16:48:14 pefo Exp $	*/
/*	$NetBSD: com.c,v 1.82.4.1 1996/06/02 09:08:00 mrg Exp $	*/

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
 *	@(#)com.c	7.5 (Berkeley) 5/16/91
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
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/device.h>
#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>	/* XXX */

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#include <dev/ic/ns16550reg.h>
#ifdef COM_HAYESP
#include <dev/ic/hayespreg.h>
#endif
#define	com_lcr	com_cfcr

#include "com.h"

/* XXX: These belong elsewhere */
cdev_decl(com);
bdev_decl(com);

static u_char tiocm_xxx2mcr __P((int));

/*
 * XXX the following two cfattach structs should be different, and possibly
 * XXX elsewhere.
 */
int	comprobe __P((struct device *, void *, void *));
void	comattach __P((struct device *, struct device *, void *));
void	comstart_pending __P((void *));

#if NCOM_ISA
struct cfattach com_isa_ca = {
	sizeof(struct com_softc), comprobe, comattach
};
#endif

#if NCOM_COMMULTI
struct cfattach com_commulti_ca = {
	sizeof(struct com_softc), comprobe, comattach
};
#endif

struct cfdriver com_cd = {
	NULL, "com", DV_TTY
};

#ifndef CONSPEED
#define	CONSPEED B9600
#endif

#ifdef COMCONSOLE
int	comdefaultrate = CONSPEED;		/* XXX why set default? */
#else
int	comdefaultrate = TTYDEF_SPEED;
#endif
int	comconsaddr;
int	comconsinit;
int	comconsattached;
bus_space_tag_t comconsiot;
bus_space_handle_t comconsioh;
tcflag_t comconscflag = TTYDEF_CFLAG;

int	commajor;
int	comsopen = 0;
int	comevents = 0;

#ifdef KGDB
#include <machine/remote-sl.h>
extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	DEVUNIT(x)	(minor(x) & 0x7f)
#define	DEVCUA(x)	(minor(x) & 0x80)

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/*
 * must be called at spltty() or higher.
 */
void
com_absent_notify(sc)
	struct com_softc *sc;
{
	struct tty *tp = sc->sc_tty;

	if (tp) {
		CLR(tp->t_state, TS_CARR_ON|TS_BUSY);
		ttyflush(tp, FREAD|FWRITE);
	}
}

int
comspeed(speed)
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
comprobe1(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
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

#ifdef COM_HAYESP
int
comprobeHAYESP(hayespioh, sc)
	bus_space_handle_t hayespioh;
	struct com_softc *sc;
{
	char	val, dips;
	int	combaselist[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
	bus_space_tag_t iot = sc->sc_iot;

	/*
	 * Hayes ESP cards have two iobases.  One is for compatibility with
	 * 16550 serial chips, and at the same ISA PC base addresses.  The
	 * other is for ESP-specific enhanced features, and lies at a
	 * different addressing range entirely (0x140, 0x180, 0x280, or 0x300).
	 */

	/* Test for ESP signature */
	if ((bus_space_read_1(iot, hayespioh, 0) & 0xf3) == 0)
		return 0;

	/*
	 * ESP is present at ESP enhanced base address; unknown com port
	 */

	/* Get the dip-switch configurations */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETDIPS);
	dips = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1);

	/* Determine which com port this ESP card services: bits 0,1 of  */
	/*  dips is the port # (0-3); combaselist[val] is the com_iobase */
	if (sc->sc_iobase != combaselist[dips & 0x03])
		return 0;

	printf(": ESP");

 	/* Check ESP Self Test bits. */
	/* Check for ESP version 2.0: bits 4,5,6 == 010 */
	bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_GETTEST);
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS1); /* Clear reg 1 */
	val = bus_space_read_1(iot, hayespioh, HAYESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		/* we do not support the necessary features */
		return 0;
	}

	/* Check for ability to emulate 16550: bit 8 == 1 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		/* XXX Does slave really mean no 16550 support?? */
		return 0;
	}

	/*
	 * If we made it this far, we are a full-featured ESP v2.0 (or
	 * better), at the correct com port address.
	 */

	SET(sc->sc_hwflags, COM_HW_HAYESP);
	printf(", 1024 byte fifo\n");
	return 1;
}
#endif

int
comprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int iobase, needioh;
	int rv = 1;

#if NCOM_ISA
#define IS_ISA(parent) \
	!strcmp((parent)->dv_cfdata->cf_driver->cd_name, "isa")
#endif
	/*
	 * XXX should be broken out into functions for isa probe and
	 * XXX for commulti probe, with a helper function that contains
	 * XXX most of the interesting stuff.
	 */
#if NCOM_ISA
	if (IS_ISA(parent)) {
		struct isa_attach_args *ia = aux;

		iot = ia->ia_iot;
		iobase = ia->ia_iobase;
		needioh = 1;
	} else
#endif
#if NCOM_COMMULTI
	if (1) {
		struct cfdata *cf = match;
		struct commulti_attach_args *ca = aux;
 
		if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ca->ca_slave)
			return (0);

		iot = ca->ca_iot;
		iobase = ca->ca_iobase;
		ioh = ca->ca_ioh;
		needioh = 0;
	} else
#endif
		return(0);			/* This cannot happen */

	/* if it's in use as console, it's there. */
	if (iobase == comconsaddr && !comconsattached)
		goto out;

	if (needioh && bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh)) {
		rv = 0;
		goto out;
	}
	rv = comprobe1(iot, ioh);
	if (needioh)
		bus_space_unmap(iot, ioh, COM_NPORTS);

out:
#if NCOM_ISA
	if (rv && IS_ISA(parent)) {
		struct isa_attach_args *ia = aux;

		ia->ia_iosize = COM_NPORTS;
		ia->ia_msize = 0;
	}
#endif
	return (rv);
}

void
comattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct com_softc *sc = (void *)self;
	int iobase, irq;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
#ifdef COM_HAYESP
	int	hayesp_ports[] = { 0x140, 0x180, 0x280, 0x300, 0 };
	int	*hayespp;
#endif
	u_int8_t lcr;

	/*
	 * XXX should be broken out into functions for isa attach and
	 * XXX for commulti attach, with a helper function that contains
	 * XXX most of the interesting stuff.
	 */
	if (ISSET(sc->sc_hwflags, COM_HW_REATTACH)) {
		int s;
		s = spltty();
		com_absent_notify(sc);
		splx(s);
	} else
	    sc->sc_hwflags = 0;
	sc->sc_swflags = 0;
#if NCOM_ISA
	if (IS_ISA(parent)) {
		struct isa_attach_args *ia = aux;

		/*
		 * We're living on an isa.
		 */
		iobase = ia->ia_iobase;
		iot = ia->ia_iot;
	        if (iobase != comconsaddr) {
	                if (bus_space_map(iot, iobase, COM_NPORTS, 0, &ioh))
				panic("comattach: io mapping failed");
		} else
	                ioh = comconsioh;
		irq = ia->ia_irq;
	} else
#endif
#if NCOM_COMMULTI
	if (1) {
		struct commulti_attach_args *ca = aux;

		/*
		 * We're living on a commulti.
		 */
		iobase = ca->ca_iobase;
		iot = ca->ca_iot;
		ioh = ca->ca_ioh;
		irq = IRQUNK;

		if (ca->ca_noien)
			SET(sc->sc_hwflags, COM_HW_NOIEN);
	} else
#endif
		panic("comattach: impossible");

	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_iobase = iobase;

	if (iobase == comconsaddr) {
		comconsattached = 1;

		/* 
		 * Need to reset baud rate, etc. of next print so reset
		 * comconsinit.  Also make sure console is always "hardwired".
		 */
		delay(1000);			/* wait for output to finish */
		comconsinit = 0;
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
	}

#ifdef COM_HAYESP
	/* Look for a Hayes ESP board. */
	for (hayespp = hayesp_ports; *hayespp != 0; hayespp++) {
		bus_space_handle_t hayespioh;

#define	HAYESP_NPORTS	8			/* XXX XXX XXX ??? ??? ??? */
		if (bus_space_map(iot, *hayespp, HAYESP_NPORTS, 0, &hayespioh))
			continue;
		if (comprobeHAYESP(hayespioh, sc)) {
			sc->sc_hayespbase = *hayespp;
			sc->sc_hayespioh = hayespioh;
			sc->sc_fifolen = 1024;
			break;
		}
		bus_space_unmap(iot, hayespioh, HAYESP_NPORTS);
	}
	/* No ESP; look for other things. */
	if (*hayespp == 0) {
#endif

	/*
	 * Probe for all known forms of UART.
	 */
	lcr = bus_space_read_1(iot, ioh, com_lcr);

	bus_space_write_1(iot, ioh, com_lcr, 0xbf);
	bus_space_write_1(iot, ioh, com_efr, 0);
	bus_space_write_1(iot, ioh, com_lcr, 0);

	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	delay(100);

	switch(bus_space_read_1(iot, ioh, com_iir) >> 6) {
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
			bus_space_write_1(iot, ioh, com_lcr, 0xbf);
			if (bus_space_read_1(iot, ioh, com_efr) == 0)
				sc->sc_uarttype = COM_UART_ST16650V2;
		}
	}

#ifdef notyet
	if (sc->sc_uarttype == COM_UART_16550A) { /* Probe for TI16750s */
		bus_space_write_1(iot, ioh, com_lcr, lcr | LCR_DLAB);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_ENABLE | FIFO_ENABLE_64BYTE);
		if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 7) {
			bus_space_write_1(iot, ioh, com_lcr, 0);
			if ((bus_space_read_1(iot, ioh, com_iir) >> 5) == 6)
				sc->sc_uarttype = COM_UART_TI16750;
		}
		bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE);
	}
#endif

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
	sc->sc_fifolen = 1;	/* default */
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
		printf(": ns16550a, 16 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 16;
		break;
	case COM_UART_ST16650:
		printf(": st16650, no working fifo\n");
		break;
	case COM_UART_ST16650V2:
		printf(": st16650, 32 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 32;
		break;
	case COM_UART_TI16750:
		printf(": ti16750, 64 byte fifo\n");
		SET(sc->sc_hwflags, COM_HW_FIFO);
		sc->sc_fifolen = 64;
		break;
	default:
		panic("comattach: bad fifo type\n");
	}

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, com_fifo, FIFO_RCV_RST | FIFO_XMT_RST);
	(void)bus_space_read_1(iot, ioh, com_data);
	bus_space_write_1(iot, ioh, com_fifo, 0);
#ifdef COM_HAYESP
	}
#endif

	/* disable interrupts */
	bus_space_write_1(iot, ioh, com_ier, 0);
	bus_space_write_1(iot, ioh, com_mcr, 0);

	if (irq != IRQUNK) {
#if NCOM_ISA
		if (IS_ISA(parent)) {
			struct isa_attach_args *ia = aux;

			sc->sc_ih = isa_intr_establish(ia->ia_ic, irq,
			    IST_EDGE, IPL_TTY, comintr, sc,
			    sc->sc_dev.dv_xname);
		} else
#endif
			panic("comattach: IRQ but can't have one");
	}

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			cominit(iot, ioh, kgdb_rate);
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
comopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
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
	if (!sc || ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING))
		return ENXIO;

	s = spltty();
	if (!sc->sc_tty) {
		tp = sc->sc_tty = ttymalloc();
		tty_attach(tp);
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
		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			tp->t_cflag = comconscflag;
		else
			tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = comdefaultrate;

		s = spltty();

		sc->sc_initialize = 1;
		comparam(tp, &tp->t_termios);
		ttsetwater(tp);

		if (comsopen++ == 0)
			timeout(compoll, NULL, 1);

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
			bus_space_write_1(iot, ioh, com_lcr, 0xbf);
			bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
			bus_space_write_1(iot, ioh, com_ier, 0);
			bus_space_write_1(iot, ioh, com_efr, 0);
			bus_space_write_1(iot, ioh, com_lcr, 0);
			break;
		case COM_UART_TI16750:
			bus_space_write_1(iot, ioh, com_ier, 0);
			break;
		}

#ifdef COM_HAYESP
		/* Setup the ESP board */
		if (ISSET(sc->sc_hwflags, COM_HW_HAYESP)) {
			bus_space_handle_t hayespioh = sc->sc_hayespioh;

			bus_space_write_1(iot, ioh, com_fifo,
			     FIFO_DMA_MODE|FIFO_ENABLE|
			     FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_8);

			/* Set 16550 compatibility mode */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETMODE);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, 
			     HAYESP_MODE_FIFO|HAYESP_MODE_RTS|
			     HAYESP_MODE_SCALE);

			/* Set RTS/CTS flow control */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETFLOWTYPE);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_RTS);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, HAYESP_FLOW_CTS);

			/* Set flow control levels */
			bus_space_write_1(iot, hayespioh, HAYESP_CMD1, HAYESP_SETRXFLOW);
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2, 
			     HAYESP_HIBYTE(HAYESP_RXHIWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXHIWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_HIBYTE(HAYESP_RXLOWMARK));
			bus_space_write_1(iot, hayespioh, HAYESP_CMD2,
			     HAYESP_LOBYTE(HAYESP_RXLOWMARK));
		} else
#endif
		if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
			u_int8_t fifo = FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST;
			u_int8_t lcr;

			if (tp->t_ispeed <= 1200)
				fifo |= FIFO_TRIGGER_1;
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

		/* flush any pending I/O */
		while (ISSET(bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
			(void) bus_space_read_1(iot, ioh, com_data);
		/* you turn me on, baby */
		sc->sc_mcr = MCR_DTR | MCR_RTS;
		if (!ISSET(sc->sc_hwflags, COM_HW_NOIEN))
			SET(sc->sc_mcr, MCR_IENABLE);
		bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr);
		sc->sc_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);

		sc->sc_msr = bus_space_read_1(iot, ioh, com_msr);
		if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) || DEVCUA(dev) ||
		    ISSET(sc->sc_msr, MSR_DCD) || ISSET(tp->t_cflag, MDMBUF))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
	} else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			/* Ah, but someone already is dialed in... */
			splx(s);
			return EBUSY;
		}
		sc->sc_cua = 1;		/* We go into CUA mode */
	}

	/* wait for carrier if necessary */
	if (ISSET(flag, O_NONBLOCK)) {
		if (!DEVCUA(dev) && sc->sc_cua) {
			/* Opening TTY non-blocking... but the CUA is busy */
			splx(s);
			return EBUSY;
		}
	} else {
		while (sc->sc_cua ||
		    (!ISSET(tp->t_cflag, CLOCAL) &&
		    !ISSET(tp->t_state, TS_CARR_ON))) {
			SET(tp->t_state, TS_WOPEN);
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    ttopen, 0);
			if (!DEVCUA(dev) && sc->sc_cua && error == EINTR)
				continue;
			if (error) {
				/* XXX should turn off chip if we're the
				   only waiter */
				if (DEVCUA(dev))
					sc->sc_cua = 0;
				CLR(tp->t_state, TS_WOPEN);
				splx(s);
				return error;
			}
			if (!DEVCUA(dev) && sc->sc_cua)
				continue;
		}
	}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp);
}
 
int
comclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return 0;

	(*linesw[tp->t_line].l_close)(tp, flag);
	s = spltty();
	if (!ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		/* can't do any of this stuff .... */
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, com_lcr, sc->sc_lcr);
		bus_space_write_1(iot, ioh, com_ier, 0);
		if (ISSET(tp->t_cflag, HUPCL) &&
		    !ISSET(sc->sc_swflags, COM_SW_SOFTCAR)) {
			/* XXX perhaps only clear DTR */
			bus_space_write_1(iot, ioh, com_mcr, 0);
		}

		/*
		 * Turn FIFO off; enter sleep mode if possible.
		 */
		bus_space_write_1(iot, ioh, com_fifo, 0);
		delay(100);
		(void) bus_space_read_1(iot, ioh, com_data);
		delay(100);
		bus_space_write_1(iot, ioh, com_fifo,
		    FIFO_RCV_RST | FIFO_XMT_RST);

		switch (sc->sc_uarttype) {
		case COM_UART_ST16650:
		case COM_UART_ST16650V2:
			bus_space_write_1(iot, ioh, com_lcr, 0xbf);
			bus_space_write_1(iot, ioh, com_efr, EFR_ECB);
			bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
			bus_space_write_1(iot, ioh, com_lcr, 0);
			break;
		case COM_UART_TI16750:
			bus_space_write_1(iot, ioh, com_ier, IER_SLEEP);
			break;
		}
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);
	if (--comsopen == 0)
		untimeout(compoll, NULL);
	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);
#ifdef COM_DEBUG
	/* mark it ready for more use if reattached earlier */
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT_PENDING)) {
	    printf("comclose pending cleared\n");
	}
#endif
	CLR(sc->sc_hwflags, COM_HW_ABSENT_PENDING);

#ifdef notyet /* XXXX */
	if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE)) {
		ttyfree(tp);
		sc->sc_tty = 0;
	}
#endif
	return 0;
}
 
int
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
	struct tty *tp = sc->sc_tty;
 
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
comtty(dev)
	dev_t dev;
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(dev)];
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
comioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = DEVUNIT(dev);
	struct com_softc *sc = com_cd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int error;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

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

		error = suser(p->p_ucred, &p->p_acflag); 
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

int
comparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed = comspeed(t->c_ospeed);
	u_char lcr;
	tcflag_t oldcflag;
	int s;

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		int s = spltty();
		com_absent_notify(sc);
		splx(s);
		return EIO;
	}

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
					splx(s);
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

		if (!ISSET(sc->sc_hwflags, COM_HW_HAYESP) &&
		    ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
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
	splx(s);
	comstart(tp);
	return 0;
}

void
comstart_pending(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	int s;

	s = spltty();
	com_absent_notify(sc);
	splx(s);
}

void
comstart(tp)
	struct tty *tp;
{
	struct com_softc *sc = com_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int s;

	s = spltty();
	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT|COM_HW_ABSENT_PENDING)) {
		/*
		 * not quite good enough: if caller is ttywait() it will
		 * go to sleep immediately, so hang out a bit and then
		 * prod caller again.
		 */
		com_absent_notify(sc);
		timeout(comstart_pending, sc, 1);
		goto out;
	}
	if (ISSET(tp->t_state, TS_BUSY))
		goto out;
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) || sc->sc_halt > 0)
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
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
	}
	if (ISSET(sc->sc_hwflags, COM_HW_FIFO)) {
#ifdef COM_HAYESP
		u_char buffer[1024];	/* XXX: largest fifo */
#else
		u_char buffer[64];	/* XXX: largest fifo */
#endif
		u_char *cp = buffer;
		int n = q_to_b(&tp->t_outq, cp, sc->sc_fifolen);
		do {
			bus_space_write_1(iot, ioh, com_data, *cp++);
		} while (--n);
	} else
		bus_space_write_1(iot, ioh, com_data, getc(&tp->t_outq));
out:
	splx(s);
	return;
stopped:
	if (ISSET(sc->sc_ier, IER_ETXRDY)) {
		CLR(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, com_ier, sc->sc_ier);
	}
	splx(s);
}

/*
 * Stop output on a line.
 */
int
comstop(tp, flag)
	struct tty *tp;
	int flag;
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
comdiag(arg)
	void *arg;
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
compoll(arg)
	void *arg;
{
	int unit;
	struct com_softc *sc;
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
	if (comevents == 0) {
		splx(s);
		goto out;
	}
	comevents = 0;
	splx(s);

	for (unit = 0; unit < com_cd.cd_ndevs; unit++) {
		sc = com_cd.cd_devs[unit];
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
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, com_mcr,
			    sc->sc_mcr);
		}

		splx(s);

		while (ibufp < ibufend) {
			c = *ibufp++;
			if (*ibufp & LSR_OE) {
				sc->sc_overflows++;
				if (sc->sc_errors++ == 0)
					timeout(comdiag, sc, 60 * hz);
			}
			/* This is ugly, but fast. */
			c |= lsrmap[(*ibufp++ & (LSR_BI|LSR_FE|LSR_PE)) >> 2];
			(*linesw[tp->t_line].l_rint)(c, tp);
		}
	}

out:
	timeout(compoll, NULL, 1);
}

int
comintr(arg)
	void *arg;
{
	struct com_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp;
	u_char lsr, data, msr, delta;
#ifdef PPS_SYNC
	struct timeval tv;
	long usec;
#endif /* PPS_SYNC */
#ifdef COM_DEBUG
	int n;
	struct {
		u_char iir, lsr, msr;
	} iter[32];
#endif

	if (ISSET(sc->sc_hwflags, COM_HW_ABSENT) || !sc->sc_tty)
		return (0);		/* can't do squat. */

#ifdef COM_DEBUG
	n = 0;
	if (ISSET(iter[n].iir = bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
		return (0);
#else
	if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
		return (0);
#endif

	tp = sc->sc_tty;

	for (;;) {
#ifdef COM_DEBUG
		iter[n].lsr =
#endif
		lsr = bus_space_read_1(iot, ioh, com_lsr);

		if (ISSET(lsr, LSR_RXRDY)) {
			register u_char *p = sc->sc_ibufp;

			comevents = 1;
			do {
				data = bus_space_read_1(iot, ioh, com_data);
				if (ISSET(lsr, LSR_BI)) {
#ifdef notdef
					printf("break %02x %02x %02x %02x\n",
					    sc->sc_msr, sc->sc_mcr, sc->sc_lcr,
					    sc->sc_dtr);
#endif
#ifdef DDB
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
						timeout(comdiag, sc, 60 * hz);
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
#ifdef DDB
			next:
#endif
#ifdef COM_DEBUG
				if (++n >= 32)
					goto ohfudge;
				iter[n].lsr =
#endif
				lsr = bus_space_read_1(iot, ioh, com_lsr);
			} while (ISSET(lsr, LSR_RXRDY));

			sc->sc_ibufp = p;
		}
#ifdef COM_DEBUG
		else if (ISSET(lsr, LSR_BI|LSR_FE|LSR_PE|LSR_OE))
			printf("weird lsr %02x\n", lsr);
#endif

#ifdef COM_DEBUG
		iter[n].msr =
#endif
		msr = bus_space_read_1(iot, ioh, com_msr);

		if (msr != sc->sc_msr) {
			delta = msr ^ sc->sc_msr;
			sc->sc_msr = msr;
			if (ISSET(delta, MSR_DCD)) {
#ifdef PPS_SYNC
				if (ISSET(sc->sc_swflags, COM_SW_PPS)) {
					if (ISSET(msr, MSR_DCD)) {
						usec = time.tv_usec;
						microtime(&tv);
						usec = tv.tv_usec - usec;
						if (usec < 0)
							usec += 1000000;
						hardpps(&tv, usec);
					}
				}
				else
#endif /* PPS_SYNC */
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

#ifdef COM_DEBUG
		if (++n >= 32)
			goto ohfudge;
		if (ISSET(iter[n].iir = bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return (1);
#else
		if (ISSET(bus_space_read_1(iot, ioh, com_iir), IIR_NOPEND))
			return (1);
#endif
	}
#ifdef COM_DEBUG
ohfudge:
	printf("comintr: too many iterations");
	for (n = 0; n < 32; n++) {
		if ((n % 4) == 0)
			printf("\ncomintr: iter[%02d]", n);
		printf("  %02x %02x %02x", iter[n].iir, iter[n].lsr, iter[n].msr);
	}
	printf("\n");
	printf("comintr: msr %02x mcr %02x lcr %02x ier %02x\n",
	    sc->sc_msr, sc->sc_mcr, sc->sc_lcr, sc->sc_ier);
	printf("comintr: state %08x cc %d\n", sc->sc_tty->t_state,
	    sc->sc_tty->t_outq.c_cc);
#endif
}

/*
 * Following are all routines needed for COM to act as console
 */
#include <dev/cons.h>

#ifdef arc
#undef CONADDR
	extern int CONADDR;
#endif

void
comcnprobe(cp)
	struct consdev *cp;
{
	/* XXX NEEDS TO BE FIXED XXX */
#ifdef arc
	bus_space_tag_t iot = &arc_bus_io;
#else
#ifdef power4e
	bus_space_tag_t iot = &p4e_isa_io;
#else
        bus_space_tag_t iot = 0;
#endif   
#endif
	bus_space_handle_t ioh;
	int found;

	if(CONADDR == 0) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	comconsiot = iot;
	if (bus_space_map(iot, CONADDR, COM_NPORTS, 0, &ioh)) {
		cp->cn_pri = CN_DEAD;
		return;
	}
	found = comprobe1(iot, ioh);
	bus_space_unmap(iot, ioh, COM_NPORTS);
	if (!found) {
		cp->cn_pri = CN_DEAD;
		return;
	}

	/* locate the major number */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(commajor, CONUNIT);
#ifdef	COMCONSOLE
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
comcninit(cp)
	struct consdev *cp;
{

	if (bus_space_map(comconsiot, CONADDR, COM_NPORTS, 0, &comconsioh))
		panic("comcninit: mapping failed");

	cominit(comconsiot, comconsioh, comdefaultrate);
	comconsaddr = CONADDR;
	comconsinit = 0;
}

void
cominit(iot, ioh, rate)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rate;
{
	int s = splhigh();
	u_char stat;

	bus_space_write_1(iot, ioh, com_lcr, LCR_DLAB);
	rate = comspeed(comdefaultrate);
	bus_space_write_1(iot, ioh, com_dlbl, rate);
	bus_space_write_1(iot, ioh, com_dlbh, rate >> 8);
	bus_space_write_1(iot, ioh, com_lcr, LCR_8BITS);
	bus_space_write_1(iot, ioh, com_ier, IER_ERXRDY | IER_ETXRDY);
	bus_space_write_1(iot, ioh, com_fifo, FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_4);
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

int
comcngetc(dev)
	dev_t dev;
{
	int s = splhigh();
	bus_space_tag_t iot = comconsiot;
	bus_space_handle_t ioh = comconsioh;
	u_char stat, c;

	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_RXRDY))
		;
	c = bus_space_read_1(iot, ioh, com_data);
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
	return c;
}

/*
 * Console kernel output character routine.
 */
void
comcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s = splhigh();
	bus_space_tag_t iot = comconsiot;
	bus_space_handle_t ioh = comconsioh;
	u_char stat;
	register int timo;

#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (comconsinit == 0) {
		cominit(iot, ioh, comdefaultrate);
		comconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	bus_space_write_1(iot, ioh, com_data, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (!ISSET(stat = bus_space_read_1(iot, ioh, com_lsr), LSR_TXRDY) && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = bus_space_read_1(iot, ioh, com_iir);
	splx(s);
}

void
comcnpollc(dev, on)
	dev_t dev;
	int on;
{

}
