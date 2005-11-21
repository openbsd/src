/*	$OpenBSD: footbridge_com.c,v 1.6 2005/11/21 18:16:36 millert Exp $	*/
/*	$NetBSD: footbridge_com.c,v 1.13 2003/03/23 14:12:25 chris Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe
 * Copyright (c) 1997 Causality Limited
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * COM driver, using the footbridge UART
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/selinfo.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/termios.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/conf.h>
#include <arm/footbridge/dc21285mem.h>
#include <arm/footbridge/dc21285reg.h>
#include <arm/footbridge/footbridgevar.h>
#include <arm/footbridge/footbridge.h>

#include <dev/cons.h>

#include "fcom.h"

extern u_int dc21285_fclk;


#ifdef DDB
/*
 * Define the keycode recognised as a request to call the debugger
 * A value of 0 disables the feature when DDB is built in
 */
#define DDB_KEYCODE '@'
#ifndef DDB_KEYCODE
#define DDB_KEYCODE	0
#endif	/* DDB_KEYCODE */
#endif	/* DDB */

struct fcom_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void			*sc_ih;
	void			*sc_si;
	int			sc_rx_irq;
	int			sc_tx_irq;
	int			sc_hwflags;
#define HW_FLAG_CONSOLE	0x01
	int			sc_swflags;
	int			sc_l_ubrlcr;
	int			sc_m_ubrlcr;	
	int			sc_h_ubrlcr;	
	char			*sc_rxbuffer[2];
	char			*sc_rxbuf;
	int			sc_rxpos;
	int			sc_rxcur;
	struct tty		*sc_tty;
};

#define RX_BUFFER_SIZE	0x100

static int  fcom_probe   (struct device *, void*, void *);
static void fcom_attach  (struct device *, struct device *, void *);
static void fcom_softintr (void *);

static int fcom_rxintr (void *);
/*static int fcom_txintr (void *);*/

cons_decl(fcom);

struct cfattach fcom_ca = {
	sizeof (struct fcom_softc), fcom_probe, fcom_attach
};

struct cfdriver fcom_cd = {
	NULL, "fcom", DV_DULL
};
#if 0
CFATTACH_DECL(fcom, sizeof(struct fcom_softc),
    fcom_probe, fcom_attach, NULL, NULL);
#endif

extern struct cfdriver fcom_cd;

dev_type_open(fcomopen);
dev_type_close(fcomclose);
dev_type_read(fcomread);
dev_type_write(fcomwrite);
dev_type_ioctl(fcomioctl);
dev_type_tty(fcomtty);
dev_type_poll(fcompoll);

#if 0
const struct cdevsw fcom_cdevsw = {
	fcomopen, fcomclose, fcomread, fcomwrite, fcomioctl,
	nostop, fcomtty, fcompoll, nommap, ttykqfilter, D_TTY
};
#endif

void fcominit	 	(bus_space_tag_t, bus_space_handle_t, int, int);
void fcominitcons 	(bus_space_tag_t, bus_space_handle_t);

bus_space_tag_t fcomconstag;
bus_space_handle_t fcomconsioh;
extern int comcnmode;
extern int comcnspeed;

#define	COMUNIT(x)	(minor(x))
#ifndef CONUNIT
#define CONUNIT	0
#endif

/*
 * The console is set up at init time, well in advance of the reset of the
 * system and thus we have a private bus space tag for the console.
 *
 * The tag is provided by fcom_io.c and fcom_io_asm.S
 */
extern struct bus_space fcomcons_bs_tag;

/*
 * int fcom_probe(struct device *parent, struct cfdata *cf, void *aux)
 *
 * Make sure we are trying to attach a com device and then
 * probe for one.
 */

static int
fcom_probe(parent, cf, aux)
	struct device *parent;
	void *cf;
	void *aux;
{
	union footbridge_attach_args *fba = aux;

	if (strcmp(fba->fba_name, "fcom") == 0)
		return(1);
	return(0);
}

/*
 * void fcom_attach(struct device *parent, struct device *self, void *aux)
 *
 * attach the com device
 */

static void
fcom_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	union footbridge_attach_args *fba = aux;
	struct fcom_softc *sc = (struct fcom_softc *)self;

	/* Set up the softc */
	sc->sc_iot = fba->fba_fca.fca_iot;
	sc->sc_ioh = fba->fba_fca.fca_ioh;
	sc->sc_si = softintr_establish(IPL_TTY, fcom_softintr, sc);
	if (sc->sc_si == NULL)
		panic(": can't establish soft interrupt");
	sc->sc_rx_irq = fba->fba_fca.fca_rx_irq;
	sc->sc_tx_irq = fba->fba_fca.fca_tx_irq;
	sc->sc_hwflags = 0;
	sc->sc_swflags = 0;

	/* If we have a console tag then make a note of it */
	if (fcomconstag)
		sc->sc_hwflags |= HW_FLAG_CONSOLE;

	if (sc->sc_hwflags & HW_FLAG_CONSOLE) {
		int major;

		/* locate the major number */
		for (major = 0; major < nchrdev; ++major)
			if (cdevsw[major].d_open == fcomopen)
				break;

		cn_tab->cn_dev = makedev(major, sc->sc_dev.dv_unit);
		printf(": console");
	}
	printf("\n");

	sc->sc_ih = footbridge_intr_claim(sc->sc_rx_irq, IPL_SERIAL,
		"serial_rx", fcom_rxintr, sc);
	if (sc->sc_ih == NULL)
		panic("%s: Cannot install rx interrupt handler",
		    sc->sc_dev.dv_xname);
}

static void fcomstart (struct tty *);
static int fcomparam (struct tty *, struct termios *);

int
fcomopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct fcom_softc *sc;
	int unit = minor(dev);
	struct tty *tp;

	if (unit >= fcom_cd.cd_ndevs)
		return ENXIO;
	sc = fcom_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;
	if (!(tp = sc->sc_tty))
		sc->sc_tty = tp = ttymalloc();
	if (!sc->sc_rxbuffer[0]) {
		sc->sc_rxbuffer[0] = malloc(RX_BUFFER_SIZE, M_DEVBUF, M_WAITOK);
		sc->sc_rxbuffer[1] = malloc(RX_BUFFER_SIZE, M_DEVBUF, M_WAITOK);
		sc->sc_rxpos = 0;
		sc->sc_rxcur = 0;
		sc->sc_rxbuf = sc->sc_rxbuffer[sc->sc_rxcur];
		if (!sc->sc_rxbuf)
			panic("%s: Cannot allocate rx buffer memory",
			    sc->sc_dev.dv_xname);
	}
	tp->t_oproc = fcomstart;
	tp->t_param = fcomparam;
	tp->t_dev = dev;
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);
		ttychars(tp);
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		tp->t_ispeed = 0;
		if (ISSET(sc->sc_hwflags, HW_FLAG_CONSOLE))
			tp->t_ospeed = comcnspeed;
		else
			tp->t_ospeed = TTYDEF_SPEED;

		fcomparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0)
		return EBUSY;
	tp->t_state |= TS_CARR_ON;

	return (*linesw[tp->t_line].l_open)(dev, tp);
}

int
fcomclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;
	/* XXX This is for cons.c. */
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);
#ifdef DIAGNOSTIC
	if (sc->sc_rxbuffer[0] == NULL)
		panic("fcomclose: rx buffers not allocated");
#endif	/* DIAGNOSTIC */
	free(sc->sc_rxbuffer[0], M_DEVBUF);
	free(sc->sc_rxbuffer[1], M_DEVBUF);
	sc->sc_rxbuffer[0] = NULL;
	sc->sc_rxbuffer[1] = NULL;

	return 0;
}

int
fcomread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;

	return (*linesw[tp->t_line].l_read)(tp, uio, flag);
}

int
fcomwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;
	
	return (*linesw[tp->t_line].l_write)(tp, uio, flag);
}

#if 0
int
fcompoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;
 
	return (*linesw[tp->t_line].l_poll)(tp, events, p));
}
#endif

int
fcomioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];
	struct tty *tp = sc->sc_tty;
	int error;
	
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case TIOCGFLAGS:
		*(int *)data = sc->sc_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error)
			return (error); 
		sc->sc_swflags = *(int *)data;
		break;

	default:
		return ENOTTY;
	}

	return 0;
}

struct tty *
fcomtty(dev)
	dev_t dev;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(dev)];

	return sc->sc_tty;
}

int
fcomstop(struct tty *tp, int flag)
{
	return 0;
}

static void
fcomstart(tp)
	struct tty *tp;
{
	struct clist *cl;
	int s, len;
	u_char buf[64];
	int loop;
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int timo;

	s = spltty();
	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP)) {
		(void)splx(s);
		return;
	}
	tp->t_state |= TS_BUSY;
	(void)splx(s);

/*	s = splserial();*/
	/* wait for any pending transmission to finish */
	timo = 100000;
	while ((bus_space_read_4(iot, ioh, UART_FLAGS) & UART_TX_BUSY) && --timo)
		;

	s = splserial();
	if (bus_space_read_4(iot, ioh, UART_FLAGS) & UART_TX_BUSY) {
		tp->t_state |= TS_TIMEOUT;
		timeout_add(&tp->t_rstrt_to, 1);
		(void)splx(s);
		return;
	}

	(void)splx(s);
	
	cl = &tp->t_outq;
	len = q_to_b(cl, buf, 64);
	for (loop = 0; loop < len; ++loop) {
/*		s = splserial();*/

		bus_space_write_4(iot, ioh, UART_DATA, buf[loop]);

		/* wait for this transmission to complete */
		timo = 100000;
		while ((bus_space_read_4(iot, ioh, UART_FLAGS) & UART_TX_BUSY) && --timo)
			;
/*		(void)splx(s);*/
	}
	s = spltty();
	tp->t_state &= ~TS_BUSY;
	if (cl->c_cc) {
		tp->t_state |= TS_TIMEOUT;
		timeout_add(&tp->t_rstrt_to, 1);
	}
	if (cl->c_cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup(cl);
		}
		selwakeup(&tp->t_wsel);
	}
	(void)splx(s);
}

static int
fcomparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct fcom_softc *sc = fcom_cd.cd_devs[minor(tp->t_dev)];
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	int baudrate;
	int h_ubrlcr;
	int m_ubrlcr;
	int l_ubrlcr;
	int s;

	/* check requested parameters */
	if (t->c_ospeed < 0)
		return (EINVAL);
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	switch (t->c_ospeed) {
	case B1200:
	case B2400:
	case B4800:
	case B9600:
	case B19200:
	case B38400:
		baudrate = UART_BRD(dc21285_fclk, t->c_ospeed);
		break;
	default:
		baudrate = UART_BRD(dc21285_fclk, 9600);
		break;
	}

	l_ubrlcr = baudrate & 0xff;
	m_ubrlcr = (baudrate >> 8) & 0xf;
	h_ubrlcr = 0;

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		h_ubrlcr |= UART_DATA_BITS_5;
		break;
	case CS6:
		h_ubrlcr |= UART_DATA_BITS_6;
		break;
	case CS7:
		h_ubrlcr |= UART_DATA_BITS_7;
		break;
	case CS8:
		h_ubrlcr |= UART_DATA_BITS_8;
		break;
	}

	if (ISSET(t->c_cflag, PARENB)) {
		h_ubrlcr |= UART_PARITY_ENABLE;
		if (ISSET(t->c_cflag, PARODD))
			h_ubrlcr |= UART_ODD_PARITY;
		else
			h_ubrlcr |= UART_EVEN_PARITY;
	}

	if (ISSET(t->c_cflag, CSTOPB))
		h_ubrlcr |= UART_STOP_BITS_2;

	bus_space_write_4(iot, ioh, UART_L_UBRLCR, l_ubrlcr);
	bus_space_write_4(iot, ioh, UART_M_UBRLCR, m_ubrlcr);
	bus_space_write_4(iot, ioh, UART_H_UBRLCR, h_ubrlcr);

	s = splserial();

	sc->sc_l_ubrlcr = l_ubrlcr;
	sc->sc_m_ubrlcr = m_ubrlcr;
	sc->sc_h_ubrlcr = h_ubrlcr;

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(sc->sc_swflags, TIOCFLAG_SOFTCAR) ||
	    ISSET(sc->sc_hwflags, HW_FLAG_CONSOLE)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/* and copy to tty */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	bus_space_write_4(iot, ioh, UART_L_UBRLCR, l_ubrlcr);
	bus_space_write_4(iot, ioh, UART_M_UBRLCR, m_ubrlcr);
	bus_space_write_4(iot, ioh, UART_H_UBRLCR, h_ubrlcr);

	(void)splx(s);

	return (0);
}

static void
fcom_softintr(arg)
	void *arg;
{
	struct fcom_softc *sc = arg;
	struct tty *tp = sc->sc_tty;
	int s;
	int loop;
	int len;
	char *ptr;

	s = spltty();
	ptr = sc->sc_rxbuf;
	len = sc->sc_rxpos;
	sc->sc_rxcur ^= 1;
	sc->sc_rxbuf = sc->sc_rxbuffer[sc->sc_rxcur];
	sc->sc_rxpos = 0;
	(void)splx(s);

	for (loop = 0; loop < len; ++loop)
		(*linesw[tp->t_line].l_rint)(ptr[loop], tp);
}

#if 0
static int
fcom_txintr(arg)
	void *arg;
{
/*	struct fcom_softc *sc = arg;*/

	printf("fcom_txintr()\n");	
	return(0);
}
#endif

static int
fcom_rxintr(arg)
	void *arg;
{
	struct fcom_softc *sc = arg;
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;
	struct tty *tp = sc->sc_tty;
	int status;
	int byte;
	int softint_scheduled = 0;

	do {
		status = bus_space_read_4(iot, ioh, UART_FLAGS);
		if ((status & UART_RX_FULL))
			break;
		byte = bus_space_read_4(iot, ioh, UART_DATA);
		status = bus_space_read_4(iot, ioh, UART_RX_STAT);
#if defined(DDB) && DDB_KEYCODE > 0
		/*
		 * Temporary hack so that I can force the kernel into
		 * the debugger via the serial port
		 */
		if (byte == DDB_KEYCODE) Debugger();
#endif
		if (tp && (tp->t_state & TS_ISOPEN))
			if (sc->sc_rxpos < RX_BUFFER_SIZE) {
				sc->sc_rxbuf[sc->sc_rxpos++] = byte;
				if (!softint_scheduled) {
					softint_scheduled = 1;
					softintr_schedule(sc->sc_si);
				}
			}
	} while (1);
	return(1);
}

#if 0
void
fcom_iflush(sc)
	struct fcom_softc *sc;
{
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	/* flush any pending I/O */
	while (!ISSET(bus_space_read_4(iot, ioh, UART_FLAGS), UART_RX_FULL))
		(void) bus_space_read_4(iot, ioh, UART_DATA);
}
#endif

/*
 * Following are all routines needed for COM to act as console
 */

#if 0
void
fcomcnprobe(cp)
	struct consdev *cp;
{
	int major;

	/* Serial console is always present so no probe */

	/* locate the major number */
	major = cdevsw_lookup_major(&fcom_cdevsw);

	/* initialize required fields */
	cp->cn_dev = makedev(major, CONUNIT);
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
}

void
fcomcninit(cp)
	struct consdev *cp;
{
	fcomconstag = &fcomcons_bs_tag;

	if (bus_space_map(fcomconstag, DC21285_ARMCSR_BASE, DC21285_ARMCSR_SIZE, 0, &fcomconsioh))
		panic("fcomcninit: mapping failed");

	fcominitcons(fcomconstag, fcomconsioh);
}
#endif

struct consdev fcomcons = {
	NULL, NULL, fcomcngetc, fcomcnputc, fcomcnpollc, NULL,
	    NODEV, CN_NORMAL
};

int
fcomcnattach(iobase, rate, cflag)
	u_int iobase;
	int rate;
	tcflag_t cflag;
{
#if 0
	static struct consdev fcomcons = {
		NULL, NULL, fcomcngetc, fcomcnputc, fcomcnpollc, NULL,
		    NULL, NULL, NODEV, CN_NORMAL
	};
#endif

	fcomconstag = &fcomcons_bs_tag;

	if (bus_space_map(fcomconstag, iobase, DC21285_ARMCSR_SIZE,
	    0, &fcomconsioh))
		panic("fcomcninit: mapping failed");

	fcominit(fcomconstag, fcomconsioh, rate, cflag);

	cn_tab = &fcomcons;

/*	comcnspeed = rate;
	comcnmode = cflag;*/
	return (0);
}

int
fcomcndetach(void)
{
	bus_space_unmap(fcomconstag, fcomconsioh, DC21285_ARMCSR_SIZE);

	cn_tab = NULL;
	return (0);
}

/*
 * Initialize UART to known state.
 */
void
fcominit(iot, ioh, rate, mode)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int rate;
	int mode;
{
	int baudrate;
	int h_ubrlcr;
	int m_ubrlcr;
	int l_ubrlcr;

	switch (rate) {
	case B1200:
	case B2400:
	case B4800:
	case B9600:
	case B19200:
	case B38400:
		baudrate = UART_BRD(dc21285_fclk, rate);
		break;
	default:
		baudrate = UART_BRD(dc21285_fclk, 9600);
		break;
	}

	h_ubrlcr = 0;
	switch (mode & CSIZE) {
	case CS5:
		h_ubrlcr |= UART_DATA_BITS_5;
		break;
	case CS6:
		h_ubrlcr |= UART_DATA_BITS_6;
		break;
	case CS7:
		h_ubrlcr |= UART_DATA_BITS_7;
		break;
	case CS8:
		h_ubrlcr |= UART_DATA_BITS_8;
		break;
	}

	if (mode & PARENB)
		h_ubrlcr |= UART_PARITY_ENABLE;
	if (mode & PARODD)
		h_ubrlcr |= UART_ODD_PARITY;
	else
		h_ubrlcr |= UART_EVEN_PARITY;

	if (mode & CSTOPB)
		h_ubrlcr |= UART_STOP_BITS_2;

	m_ubrlcr = (baudrate >> 8) & 0xf;
	l_ubrlcr = baudrate & 0xff;

	bus_space_write_4(iot, ioh, UART_L_UBRLCR, l_ubrlcr);
	bus_space_write_4(iot, ioh, UART_M_UBRLCR, m_ubrlcr);
	bus_space_write_4(iot, ioh, UART_H_UBRLCR, h_ubrlcr);
}
#if 0
/*
 * Set UART for console use. Do normal init, then enable interrupts.
 */
void
fcominitcons(iot, ioh)
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
{
	int s = splserial();

	fcominit(iot, ioh, comcnspeed, comcnmode);

	delay(10000);

	(void)splx(s);
}
#endif

int
fcomcngetc(dev)
	dev_t dev;
{
	int s = splserial();
	bus_space_tag_t iot = fcomconstag;
	bus_space_handle_t ioh = fcomconsioh;
	u_char stat, c;

	while ((bus_space_read_4(iot, ioh, UART_FLAGS) & UART_RX_FULL) != 0)
		;
	c = bus_space_read_4(iot, ioh, UART_DATA);
	stat = bus_space_read_4(iot, ioh, UART_RX_STAT);
	(void)splx(s);
#if defined(DDB) && DDB_KEYCODE > 0
		/*
		 * Temporary hack so that I can force the kernel into
		 * the debugger via the serial port
		 */
		if (c == DDB_KEYCODE) Debugger();
#endif

	return (c);
}

/*
 * Console kernel output character routine.
 */
void
fcomcnputc(dev, c)
	dev_t dev;
	int c;
{
	int s = splserial();
	bus_space_tag_t iot = fcomconstag;
	bus_space_handle_t ioh = fcomconsioh;
	int timo;

	/* wait for any pending transmission to finish */
	timo = 50000;
	while ((bus_space_read_4(iot, ioh, UART_FLAGS) & UART_TX_BUSY) && --timo)
		;
	bus_space_write_4(iot, ioh, UART_DATA, c);

	/* wait for this transmission to complete */
	timo = 1500000;
	while ((bus_space_read_4(iot, ioh, UART_FLAGS) & UART_TX_BUSY) && --timo)
		;
	/* Clear interrupt status here */
	(void)splx(s);
}

void
fcomcnpollc(dev, on)
	dev_t dev;
	int on;
{
}
