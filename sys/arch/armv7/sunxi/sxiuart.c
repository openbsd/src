/*	$OpenBSD: sxiuart.c,v 1.4 2015/04/22 11:39:04 jsg Exp $	*/
/*
 * Copyright (c) 2005 Dale Rahn <drahn@motorola.com>
 * Copyright (c) 2013 Artturi Alm
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/kernel.h>

#include <dev/cons.h>

#ifdef DDB
#include <ddb/db_var.h>
#endif

#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/sunxi/sxiuartreg.h>
#include <armv7/sunxi/sunxireg.h>

#define DEVUNIT(x)      (minor(x) & 0x7f)
#define DEVCUA(x)       (minor(x) & 0x80)

struct sxiuart_softc {
	struct device	sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;
	struct soft_intrhand *sc_si;
	void		*sc_irq;
	struct tty	*sc_tty;
	struct timeout	sc_diag_tmo;
	struct timeout	sc_dtr_tmo;
	int		sc_overflows;
	int		sc_floods;
	int		sc_errors;
	int		sc_halt;
	uint8_t		sc_ier;
	uint8_t		sc_mcr;
	uint8_t		sc_msr;
	uint8_t		sc_dtr;
	uint8_t		sc_lcr;
	int		sc_frequency;
	uint8_t		sc_hwflags;
#define COM_HW_NOIEN    0x01
#define COM_HW_FIFO     0x02
#define COM_HW_SIR      0x20
#define COM_HW_CONSOLE  0x40
#define COM_HW_KGDB     0x80
	uint8_t		sc_swflags;
#define COM_SW_SOFTCAR  0x01
#define COM_SW_CLOCAL   0x02
#define COM_SW_CRTSCTS  0x04
#define COM_SW_MDMBUF   0x08
#define COM_SW_PPS      0x10

	uint8_t		sc_initialize;
	uint8_t		sc_cua;
	uint8_t		*sc_ibuf, *sc_ibufp, *sc_ibufhigh, *sc_ibufend;
#define SXIUART_IBUFSIZE 128
#define SXIUART_IHIGHWATER 100
	uint8_t		sc_ibufs[2][SXIUART_IBUFSIZE];
};


int	sxiuartprobe(struct device *, void *, void *);
void	sxiuartattach(struct device *, struct device *, void *);

void sxiuartcnprobe(struct consdev *);
void sxiuartcninit(struct consdev *);
int sxiuartcnattach(bus_space_tag_t, bus_addr_t, int, long, tcflag_t);
int sxiuartcngetc(dev_t);
void sxiuartcnputc(dev_t, int);
void sxiuartcnpollc(dev_t, int);
int sxiuart_param(struct tty *, struct termios *);
void sxiuart_start(struct tty *);
void sxiuart_pwroff(struct sxiuart_softc *);
void sxiuart_diag(void *);
void sxiuart_raisedtr(void *);
void sxiuart_softint(void *);
int sxiuart_intr(void *);


struct sxiuart_softc *sxiuart_sc(dev_t);

/* XXX - we imitate 'com' serial ports and take over their entry points */
/* XXX: These belong elsewhere */
cdev_decl(sxiuart);

struct cfattach sxiuart_ca = {
	sizeof(struct sxiuart_softc), NULL, sxiuartattach
};

struct cfdriver sxiuart_cd = {
	NULL, "sxiuart", DV_TTY
};

struct consdev sxiuartcons = {
	sxiuartcnprobe, sxiuartcninit,
	sxiuartcngetc, sxiuartcnputc,
	sxiuartcnpollc, NULL,
	NODEV, CN_HIGHPRI
};

bus_space_tag_t	sxiuartconsiot;
bus_space_handle_t sxiuartconsioh;
bus_addr_t	sxiuartconsaddr;
tcflag_t	sxiuartconscflag = TTYDEF_CFLAG;
int		sxiuartdefaultrate = B115200;

struct cdevsw sxiuartdev =
	cdev_tty_init(1/*XXX NIMXUART */ , sxiuart); /* 12: serial port */

void
sxiuartattach(struct device *parent, struct device *self, void *args)
{
	struct armv7_attach_args *aa = args;
	struct sxiuart_softc *sc = (struct sxiuart_softc *) self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int s;

	sc->sc_iot = iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_dev->mem[0].addr,
	    aa->aa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("sxiuartattach: bus_space_map failed!");
	ioh = sc->sc_ioh;

	if (aa->aa_dev->mem[0].addr == sxiuartconsaddr) {
		cn_tab->cn_dev = makedev(12 /* XXX */, 0);
		cdevsw[12] = sxiuartdev;		/* KLUDGE */

		printf(": console");
		/* XXX compare uses of COM_HW_CONSOLE against com.c */
		SET(sc->sc_hwflags, COM_HW_CONSOLE);
		SET(sc->sc_swflags, COM_SW_SOFTCAR);
		sxiuartconsiot = iot;
		sxiuartconsioh = ioh;
	}

	timeout_set(&sc->sc_diag_tmo, sxiuart_diag, sc);
	timeout_set(&sc->sc_dtr_tmo, sxiuart_raisedtr, sc);
	sc->sc_si = softintr_establish(IPL_TTY, sxiuart_softint, sc);
	if(sc->sc_si == NULL)
		panic("%s: can't establish soft interrupt.",
		    sc->sc_dev.dv_xname);

	sc->sc_frequency = 24000000; /* XXX */

	if ((bus_space_read_1(sc->sc_iot, sc->sc_ioh, SXIUART_IIR) &
	  IIR_BUSY) == IIR_BUSY)
		(void)bus_space_read_4(sc->sc_iot, sc->sc_ioh, SXIUART_USR);
	sc->sc_ier = 0;
	/* disable interrupts */
	bus_space_write_1(iot, ioh, SXIUART_IER, sc->sc_ier);

	/* clear and disable fifo */
	bus_space_write_1(iot, ioh, SXIUART_FCR, 0 | RFIFOR | XFIFOR);

	s = splhigh();
	SET(sc->sc_mcr, MCR_DTR | MCR_RTS | MCR_IENABLE);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SXIUART_MCR, sc->sc_mcr);
	splx(s);

	arm_intr_establish(aa->aa_dev->irq[0], IPL_TTY,
	    sxiuart_intr, sc, sc->sc_dev.dv_xname);

	printf("\n");
}

int
sxiuart_intr(void *arg)
{
	struct sxiuart_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp;
	uint32_t cnt;
	uint8_t c, iir, lsr, msr, delta;
	uint8_t *p;

	iir = bus_space_read_1(iot, ioh, SXIUART_IIR);

	if ((iir & IIR_IMASK) == IIR_BUSY) {
		(void)bus_space_read_1(iot, ioh, SXIUART_USR);
		return (0);
	}
	if (ISSET(iir, IIR_NOPEND))
		return (0);

	if (sc->sc_tty == NULL)
		return (0);

	tp = sc->sc_tty;
	cnt = 0;
loop:
	lsr = bus_space_read_1(iot, ioh, SXIUART_LSR);
	if (ISSET(lsr, LSR_RXRDY)) {
		if (cnt == 0) {
			p = sc->sc_ibufp;
			softintr_schedule(sc->sc_si);
		}
		cnt++;

		c = bus_space_read_1(iot, ioh, SXIUART_RBR);
		if (ISSET(lsr, LSR_BI)) {
#if defined(DDB)
			if (ISSET(sc->sc_hwflags,
			    COM_HW_CONSOLE)) {
				if (db_console)
					Debugger();
				goto loop;
			}
#endif
			c = 0;
		}
		if (p >= sc->sc_ibufend) {
			sc->sc_floods++;
			if (sc->sc_errors++ == 0)
				timeout_add_sec(&sc->sc_diag_tmo, 60);
		} else {
			*p++ = c;
			*p++ = lsr;
			if (p == sc->sc_ibufhigh &&
			    ISSET(tp->t_cflag, CRTSCTS)) {
				/* XXX */
				CLR(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
			}
		}
		goto loop;
	} else if (cnt > 0)
		sc->sc_ibufp = p;

	msr = bus_space_read_1(iot, ioh, SXIUART_MSR);
	if (msr != sc->sc_msr) {
		delta = msr ^ sc->sc_msr;

		ttytstamp(tp, sc->sc_msr & MSR_CTS,
		    msr & MSR_CTS, sc->sc_msr & MSR_DCD,
		    msr & MSR_DCD);

		sc->sc_msr = msr;
		if (ISSET(delta, MSR_DCD)) {
			if (!ISSET(sc->sc_swflags, COM_SW_SOFTCAR) &&
			    (*linesw[tp->t_line].l_modem)(tp,
			    ISSET(msr, MSR_DCD)) == 0) {
				CLR(sc->sc_mcr, sc->sc_dtr);
				bus_space_write_1(iot, ioh, SXIUART_MCR,
				    sc->sc_mcr);
			}
		}
		if (ISSET(delta & msr, MSR_CTS) &&
		    ISSET(tp->t_cflag, CRTSCTS))
			(*linesw[tp->t_line].l_start)(tp);
	}

	if (ISSET(tp->t_state, TS_BUSY) && ISSET(lsr, LSR_TXRDY)) {
		CLR(tp->t_state, TS_BUSY | TS_FLUSH);
		if (sc->sc_halt > 0)
			wakeup(&tp->t_outq);
		(*linesw[tp->t_line].l_start)(tp);
	}

	iir = bus_space_read_1(iot, ioh, SXIUART_IIR);

	if (ISSET(iir, IIR_NOPEND))
		goto done;

	cnt = 0;
	goto loop;
done:
	return (1);
}

int
sxiuart_param(struct tty *tp, struct termios *t)
{
	struct sxiuart_softc *sc = sxiuart_cd.cd_devs[DEVUNIT(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int ospeed = t->c_ospeed;
	int error;
	tcflag_t oldcflag;
	uint16_t ratediv;
	uint8_t lcr = 0;

	if (t->c_ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return (EINVAL);

	/* XXX get prev state of SXIUART_LCR_SBREAK bit */

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
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
	}

	if (ospeed != 0) { /* XXX sc_initialize? */
		while (ISSET(tp->t_state, TS_BUSY)) {
			++sc->sc_halt;
			error = ttysleep(tp, &tp->t_outq,
			    TTOPRI | PCATCH, "sxiuartprm", 0);
			--sc->sc_halt;
			if (error) {
				sxiuart_start(tp);
				return (error);
			}
		}
		bus_space_write_1(iot, ioh, SXIUART_LCR, lcr | LCR_DLAB);
		ratediv = 13;
		bus_space_write_1(iot, ioh, SXIUART_DLL, ratediv);
		bus_space_write_1(iot, ioh, SXIUART_DLH, ratediv >> 8);
		bus_space_write_1(iot, ioh, SXIUART_LCR, lcr);
		SET(sc->sc_mcr, MCR_DTR);
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
	} else
		bus_space_write_1(iot, ioh, SXIUART_LCR, lcr);

	/* setup fifo */
	bus_space_write_1(iot, ioh, SXIUART_FCR, FIFOE | FIFO_RXT0);

	/* When not using CRTSCTS, RTS follows DTR. */
	if (!ISSET(t->c_cflag, CRTSCTS)) {
		if (ISSET(sc->sc_mcr, MCR_DTR)) {
			if (!ISSET(sc->sc_mcr, MCR_RTS)) {
				SET(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
			}
		} else {
			if (ISSET(sc->sc_mcr, MCR_RTS)) {
				CLR(sc->sc_mcr, MCR_RTS);
				bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
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
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
	}


	sxiuart_start(tp);

	return (0);
}

void
sxiuart_start(struct tty *tp)
{
        struct sxiuart_softc *sc = sxiuart_sc(tp->t_dev);
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int i, n, s;
	uint8_t buf[64];

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		splx(s);
		return;
	}
	if (ISSET(tp->t_state, TS_TIMEOUT | TS_TTSTOP) || sc->sc_halt > 0)
		goto stopped;
	if (ISSET(tp->t_cflag, CRTSCTS) && !ISSET(sc->sc_msr, MSR_CTS))
		goto stopped;
#if 0
	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		if (tp->t_outq.c_cc == 0)
			goto stopped;
		selwakeup(&tp->t_wsel);
	}
#else
	ttwakeupwr(tp);
	if (tp->t_outq.c_cc == 0)
		goto stopped;
#endif
	SET(tp->t_state, TS_BUSY);

	if (!ISSET(sc->sc_ier, IER_ETXRDY)) {
		SET(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, SXIUART_IER, sc->sc_ier);
	}

	n = q_to_b(&tp->t_outq, buf, sizeof(buf));
	for (i = 0; i < n; i++)
		bus_space_write_1(iot, ioh, SXIUART_THR, buf[i]);
	bzero(buf, n);
	splx(s);
	return;
stopped:
	if (ISSET(sc->sc_ier, IER_ETXRDY)) {
		CLR(sc->sc_ier, IER_ETXRDY);
		bus_space_write_1(iot, ioh, SXIUART_IER, sc->sc_ier);
	}
	splx(s);
}

void
sxiuart_pwroff(struct sxiuart_softc *sc)
{
}

void
sxiuart_diag(void *arg)
{
	struct sxiuart_softc *sc = arg;
	int overflows, floods;
	int s = spltty();
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
sxiuart_raisedtr(void *arg)
{
	struct sxiuart_softc *sc = arg;

	SET(sc->sc_mcr, MCR_DTR | MCR_RTS);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, SXIUART_MCR, sc->sc_mcr);
}

void
sxiuart_softint(void *arg)
{
	struct sxiuart_softc *sc = arg;
	struct tty *tp;
	uint8_t *ibufp;
	uint8_t *ibufend;
	int c, s;
	static int lsrmap[8] = {
		0,	TTY_PE,
		TTY_FE,	TTY_PE|TTY_FE,
		TTY_FE,	TTY_PE|TTY_FE,
		TTY_FE,	TTY_PE|TTY_FE
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
	sc->sc_ibufhigh = sc->sc_ibuf + SXIUART_IHIGHWATER;
	sc->sc_ibufend = sc->sc_ibuf + SXIUART_IBUFSIZE;

	if (tp == NULL || !ISSET(tp->t_state, TS_ISOPEN)) {
		splx(s);
		return;
	}

	if (ISSET(tp->t_cflag, CRTSCTS) &&
	    !ISSET(sc->sc_mcr, MCR_RTS)) {
		/* XXX */
		SET(sc->sc_mcr, MCR_RTS);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, SXIUART_MCR,
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
#define ASD (LSR_BI | LSR_FE | LSR_PE)
		c |= lsrmap[(*ibufp++ & ASD) >> 2];
#undef ASD
		(*linesw[tp->t_line].l_rint)(c, tp);
	}
}

int
sxiuartopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct sxiuart_softc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct tty *tp;
	int s;
	int error = 0;

	sc = sxiuart_sc(dev);
	if (sc == NULL)
		return (ENXIO);

	s = spltty();
	if (sc->sc_tty == NULL)
		tp = sc->sc_tty = ttymalloc(1000000);
	else
		tp = sc->sc_tty;
	splx(s);

	tp->t_oproc = sxiuart_start;
	tp->t_param = sxiuart_param;
	tp->t_dev = dev;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;

		if (ISSET(sc->sc_hwflags, COM_HW_CONSOLE))
			tp->t_cflag = sxiuartconscflag;
		else
			tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, COM_SW_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, COM_SW_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, COM_SW_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = sxiuartdefaultrate;

		s = spltty();

		sc->sc_initialize = 1;
		sxiuart_param(tp, &tp->t_termios);
		ttsetwater(tp);

		sc->sc_ibufp = sc->sc_ibuf = sc->sc_ibufs[0];
		sc->sc_ibufhigh = sc->sc_ibuf + SXIUART_IHIGHWATER;
		sc->sc_ibufend = sc->sc_ibuf + SXIUART_IBUFSIZE;

		iot = sc->sc_iot;
		ioh = sc->sc_ioh;

		bus_space_write_1(iot, ioh, SXIUART_FCR, FIFOE | FIFO_RXT2);
		delay(100);
		while (ISSET(bus_space_read_1(iot, ioh, SXIUART_LSR),
		    LSR_RXRDY))
			(void)bus_space_read_1(iot, ioh, SXIUART_RBR);

		sc->sc_mcr = MCR_DTR | MCR_RTS | MCR_IENABLE;
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);

		sc->sc_ier = IER_ERXRDY | IER_ERLS | IER_EMSC;
		bus_space_write_1(iot, ioh, SXIUART_IER, sc->sc_ier);

		sc->sc_msr = bus_space_read_1(iot, ioh, SXIUART_MSR);
		if (ISSET(sc->sc_swflags, COM_SW_SOFTCAR) || DEVCUA(dev) ||
		    ISSET(sc->sc_msr, MSR_DCD) ||
		    ISSET(tp->t_cflag, MDMBUF))
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);


	} else if (ISSET(tp->t_state, TS_XCLUDE) && suser(p, 0) != 0)
		return (EBUSY);
	else
		s = spltty();

	if (DEVCUA(dev)) {
		if (ISSET(tp->t_state, TS_ISOPEN)) {
			splx(s);
			return (EBUSY);
		}
		sc->sc_cua = 1;
	} else {
		/* tty (not cua) device; wait for carrier if necessary */
		if (ISSET(flag, O_NONBLOCK)) {
			if (sc->sc_cua) {
				/* Opening TTY non-blocking... but the CUA is busy */
				splx(s);
				return (EBUSY);
			}
		} else {
			while (sc->sc_cua ||
			    (!ISSET(tp->t_cflag, CLOCAL) &&
				!ISSET(tp->t_state, TS_CARR_ON))) {
				SET(tp->t_state, TS_WOPEN);
				error = ttysleep(tp, &tp->t_rawq,
				    TTIPRI | PCATCH, ttopen, 0);
				/*
				 * If TS_WOPEN has been reset, that means the
				 * cua device has been closed.  We don't want
				 * to fail in that case,
				 * so just go around again.
				 */
				if (error && ISSET(tp->t_state, TS_WOPEN)) {
					CLR(tp->t_state, TS_WOPEN);
					if (!sc->sc_cua && !ISSET(tp->t_state,
					    TS_ISOPEN))
						sxiuart_pwroff(sc);
					splx(s);
					return (error);
				}
			}
		}
	}
	splx(s);

	return (*linesw[tp->t_line].l_open)(dev, tp, p);
}

int
sxiuartclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit = DEVUNIT(dev);
	struct sxiuart_softc *sc = sxiuart_cd.cd_devs[unit];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;
	int s;

	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	(*linesw[tp->t_line].l_close)(tp, flag, p);
	s = spltty();
	if (ISSET(tp->t_state, TS_WOPEN)) {
		/* tty device is waiting for carrier; drop dtr then re-raise */
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
		timeout_add(&sc->sc_dtr_tmo, hz * 2);
	} else {
		/* no one else waiting; turn off the uart */
		sxiuart_pwroff(sc);
	}
	CLR(tp->t_state, TS_BUSY | TS_FLUSH);

	sc->sc_cua = 0;
	splx(s);
	ttyclose(tp);

	return (0);
}

int
sxiuartread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tty;

	tty = sxiuarttty(dev);
	if (tty == NULL)
		return (ENODEV);

	return ((*linesw[tty->t_line].l_read)(tty, uio, flag));
}

int
sxiuartwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tty;

	tty = sxiuarttty(dev);
	if (tty == NULL)
		return (ENODEV);

	return ((*linesw[tty->t_line].l_write)(tty, uio, flag));
}

static u_char tiocm_xxx2mcr(int);

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
sxiuartioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct sxiuart_softc *sc;
	struct tty *tp;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int error;

	sc = sxiuart_sc(dev);
	if (sc == NULL)
		return (ENODEV);

	tp = sc->sc_tty;
	if (tp == NULL)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	switch (cmd) {
	case TIOCSBRK:
		SET(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, SXIUART_LCR, sc->sc_lcr);
		break;
	case TIOCCBRK:
		CLR(sc->sc_lcr, LCR_SBREAK);
		bus_space_write_1(iot, ioh, SXIUART_LCR, sc->sc_lcr);
		break;
	case TIOCSDTR:
		SET(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
		break;
	case TIOCCDTR:
		CLR(sc->sc_mcr, sc->sc_dtr);
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
		break;
	case TIOCMSET:
		CLR(sc->sc_mcr, MCR_DTR | MCR_RTS);
	case TIOCMBIS:
		SET(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
		break;
	case TIOCMBIC:
		CLR(sc->sc_mcr, tiocm_xxx2mcr(*(int *)data));
		bus_space_write_1(iot, ioh, SXIUART_MCR, sc->sc_mcr);
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
		if (bus_space_read_1(iot, ioh, SXIUART_IER))
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
		return (ENOTTY);
	}

	return (0);
}


int
sxiuartstop(struct tty *tp, int flag)
{
	return (0);
}

struct tty *
sxiuarttty(dev_t dev)
{
	int unit;
	struct sxiuart_softc *sc;
	unit = DEVUNIT(dev);
	if (unit >= sxiuart_cd.cd_ndevs)
		return (NULL);
	sc = (struct sxiuart_softc *)sxiuart_cd.cd_devs[unit];
	if (sc == NULL)
		return (NULL);
	return (sc->sc_tty);
}

struct sxiuart_softc *
sxiuart_sc(dev_t dev)
{
	int unit;
	struct sxiuart_softc *sc;
	unit = DEVUNIT(dev);
	if (unit >= sxiuart_cd.cd_ndevs)
		return (NULL);
	sc = (struct sxiuart_softc *)sxiuart_cd.cd_devs[unit];
	return (sc);
}

/* serial console */
void
sxiuartcnprobe(struct consdev *cp)
{
	cp->cn_dev = makedev(12 /* XXX */, 0);
	cp->cn_pri = CN_HIGHPRI;
}

void
sxiuartcninit(struct consdev *cp)
{
}

int
sxiuartcnattach(bus_space_tag_t iot, bus_addr_t iobase, int rate, long freq,
	tcflag_t cflag)
{
	int s;
	uint16_t ratediv;

	if (bus_space_map(iot, iobase, UARTx_SIZE, 0, &sxiuartconsioh))
		return (ENOMEM);

	sxiuartconsiot = iot;
	sxiuartconsaddr = iobase;
	sxiuartconscflag = cflag;

	s = splhigh();
	bus_space_write_1(iot, sxiuartconsioh, SXIUART_LCR, LCR_DLAB);

	ratediv = 13; /* for 115200baud with 24000000hz freq */
	bus_space_write_1(iot, sxiuartconsioh, SXIUART_DLL, ratediv);
	bus_space_write_1(iot, sxiuartconsioh, SXIUART_DLH, ratediv >> 8);
	bus_space_write_1(iot, sxiuartconsioh, SXIUART_LCR, LCR_8BITS);

	bus_space_write_1(iot, sxiuartconsioh, SXIUART_MCR, MCR_DTR | MCR_RTS);
	bus_space_write_1(iot, sxiuartconsioh, SXIUART_IER, 0);

	bus_space_write_1(iot, sxiuartconsioh, SXIUART_FCR, FIFOE | FIFO_RXT0);

	(void)bus_space_read_1(iot, sxiuartconsioh, SXIUART_IIR);
	splx(s);

	cn_tab = &sxiuartcons;

	return (0);
}

int
sxiuartcngetc(dev_t dev)
{
	int s;
	uint8_t c;

	s = splhigh();

	while (!ISSET(bus_space_read_1(sxiuartconsiot, sxiuartconsioh,
	    SXIUART_LSR), LSR_RXRDY))
		continue;
	c = bus_space_read_1(sxiuartconsiot, sxiuartconsioh, SXIUART_RBR);

	/* clear any pending interrupts */
	(void)bus_space_read_1(sxiuartconsiot, sxiuartconsioh, SXIUART_IIR);

	splx(s);
	return (c);
}

void
sxiuartcnputc(dev_t dev, int c)
{
	int s = spltty();
	int timo = 500;

	while (!ISSET(bus_space_read_1(sxiuartconsiot, sxiuartconsioh,
	    SXIUART_LSR), LSR_THRE) && --timo)
		continue;

	bus_space_write_1(sxiuartconsiot, sxiuartconsioh, SXIUART_THR,
	    (uint8_t)c);
	bus_space_barrier(sxiuartconsiot, sxiuartconsioh, 0, 1,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);

	splx(s);
}

void
sxiuartcnpollc(dev_t dev, int on)
{
}
