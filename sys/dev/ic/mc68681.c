/*	$OpenBSD: mc68681.c,v 1.2 2013/09/21 20:05:01 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <dev/ic/mc68681reg.h>
#include <dev/ic/mc68681var.h>

#ifdef	DDB
#include <ddb/db_var.h>
#endif

#define	DART_UNIT(dev)	(minor(dev) / N68681PORTS)
#define DART_PORT(dev)	(minor(dev) % N68681PORTS)

struct cfdriver dart_cd = {
	NULL, "dart", DV_TTY
};

#define	MC68681_A_BASE	0x00
#define	MC68681_B_BASE	0x08

cdev_decl(dart);

void	mc68681_set_acr(struct mc68681_softc *);
const struct mc68681_s *mc68681_speed(int);
void	mc68681_start(struct tty *);
int	mc68681_mctl(struct mc68681_softc *, int, int, int);
int	mc68681_param(struct tty *, struct termios *);
void	mc68681_dcdint(struct mc68681_softc *);
void	mc68681_rxint(struct mc68681_softc *, int);
void	mc68681_txint(struct mc68681_softc *, int);

void
mc68681_common_attach(struct mc68681_softc *sc)
{
	sc->sc_line[A_PORT].speed = sc->sc_line[B_PORT].speed =
	    mc68681_speed(B9600);

	sc->sc_sw_reg->mr1[A_PORT] = sc->sc_sw_reg->mr1[B_PORT] =
	    DART_MR1_RX_IRQ_RXRDY | DART_MR1_ERROR_CHAR |
	    DART_MR1_PARITY_NONE | DART_MR1_RX_RTR | DART_MR1_BPC_8;
	sc->sc_sw_reg->mr2[A_PORT] = sc->sc_sw_reg->mr2[B_PORT] =
	    DART_MR2_MODE_NORMAL | /* DART_MR2_TX_CTS | */ DART_MR2_STOP_1;
	sc->sc_sw_reg->cr[A_PORT] = sc->sc_sw_reg->cr[B_PORT] =
	    DART_CR_TX_ENABLE | DART_CR_RX_ENABLE;

	sc->sc_sw_reg->acr &= ~DART_ACR_BRG_SET_MASK;
	sc->sc_sw_reg->acr |= DART_ACR_BRG_SET_2;

	/* Start out with Tx and RX interrupts disabled, but enable input port
	   change interrupt */
	sc->sc_sw_reg->imr |= DART_ISR_IP_CHANGE;

	if (sc->sc_consport >= 0) {
		printf(": console");
		delay(10000);
	}

	mc68681_set_acr(sc);

	(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr);

	printf("\n");
}

/*
 * Update the ACR register. This requires both ports to be disabled. Restart timer if necessary.  */
void
mc68681_set_acr(struct mc68681_softc *sc)
{
	uint8_t acr;

	acr = sc->sc_sw_reg->acr;
	/*
	 * If MD attachment configures a timer, ignore this until the timer
	 * limit has been correctly set up. This allows this function to be
	 * invoked at attach time, before cpu_initclocks() gets a chance to
	 * run.
	 */
	if (ISSET(acr, DART_ACR_CT_TIMER_BIT) && *sc->sc_sw_reg->ct == 0)
		acr = (acr & ~DART_ACR_CT_MASK) | DART_ACR_CT_COUNTER_CLK_16;

	/* reset port a */
	(*sc->sc_write)(sc, DART_CRA,
	    DART_CR_RESET_RX | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE);
	(*sc->sc_write)(sc, DART_CRA,
	    DART_CR_RESET_TX /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRA,
	    DART_CR_RESET_ERROR /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRA,
	    DART_CR_RESET_BREAK /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRA,
	    DART_CR_RESET_MR1 /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);

	/* reset port b */
	(*sc->sc_write)(sc, DART_CRB,
	    DART_CR_RESET_RX | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE);
	(*sc->sc_write)(sc, DART_CRB,
	    DART_CR_RESET_TX /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRB,
	    DART_CR_RESET_ERROR /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRB,
	    DART_CR_RESET_BREAK /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);
	(*sc->sc_write)(sc, DART_CRB,
	    DART_CR_RESET_MR1 /* | DART_CR_TX_DISABLE | DART_CR_RX_DISABLE */);

	(*sc->sc_write)(sc, DART_OPRS, sc->sc_sw_reg->oprs);
	(*sc->sc_write)(sc, DART_OPCR, sc->sc_sw_reg->opcr);

	(*sc->sc_write)(sc, DART_ACR, acr);
	/*
	 * Restart timer if necessary.
	 * XXX This loses the current timer cycle.
	 */
	if (ISSET(acr, DART_ACR_CT_TIMER_BIT)) {
		(*sc->sc_write)(sc, DART_CTUR, *sc->sc_sw_reg->ct >> 8);
		(*sc->sc_write)(sc, DART_CTLR, *sc->sc_sw_reg->ct & 0xff);
		(*sc->sc_read)(sc, DART_CTSTART);
	}

	/* reinitialize ports */
	(*sc->sc_write)(sc, DART_MRA, sc->sc_sw_reg->mr1[A_PORT]);
	(*sc->sc_write)(sc, DART_MRA, sc->sc_sw_reg->mr2[A_PORT]);
	(*sc->sc_write)(sc, DART_CSRA, sc->sc_line[A_PORT].speed->csr);
	(*sc->sc_write)(sc, DART_CRA, sc->sc_sw_reg->cr[A_PORT]);

	(*sc->sc_write)(sc, DART_MRB, sc->sc_sw_reg->mr1[B_PORT]);
	(*sc->sc_write)(sc, DART_MRB, sc->sc_sw_reg->mr2[B_PORT]);
	(*sc->sc_write)(sc, DART_CSRB, sc->sc_line[B_PORT].speed->csr);
	(*sc->sc_write)(sc, DART_CRB, sc->sc_sw_reg->cr[B_PORT]);
}

/* speed table */
static const struct mc68681_s mc68681_speeds[] = {
#define	MC68681_SPEED(spd,sets) \
	{ .speed = spd, .brg_sets = sets, \
	  .csr = (DART_CSR_##spd << DART_CSR_RXCLOCK_SHIFT) | \
	         (DART_CSR_##spd << DART_CSR_TXCLOCK_SHIFT) }
	MC68681_SPEED(50, 1),
	MC68681_SPEED(75, 2),
	MC68681_SPEED(110, 1 | 2),
	MC68681_SPEED(134, 1 | 2),
	MC68681_SPEED(150, 2),
	MC68681_SPEED(200, 1),
	MC68681_SPEED(300, 1 | 2),
	MC68681_SPEED(600, 1 | 2),
	MC68681_SPEED(1050, 1),
	MC68681_SPEED(1200, 1 | 2),
	MC68681_SPEED(1800, 2),
	MC68681_SPEED(2000, 2),
	MC68681_SPEED(2400, 1 | 2),
	MC68681_SPEED(4800, 1 | 2),
	MC68681_SPEED(7200, 1),
	MC68681_SPEED(9600, 1 | 2),
	MC68681_SPEED(19200, 2),
	MC68681_SPEED(38400, 1)
#undef MC68681_SPEED
};

const struct mc68681_s *
mc68681_speed(int speed)
{
	const struct mc68681_s *mcs;
	uint n;

	for (n = nitems(mc68681_speeds), mcs = mc68681_speeds; n != 0;
	    n--, mcs++) {
		if (mcs->speed == speed)
			return mcs;
	}

	return NULL;
}

void
mc68681_start(struct tty *tp)
{
	struct mc68681_softc *sc;
	dev_t dev;
	int s;
	int port, tries;
	int c;
	uint ptaddr;
	uint8_t imrbit;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	dev = tp->t_dev;
	sc = (struct mc68681_softc *)dart_cd.cd_devs[DART_UNIT(dev)];
	port = DART_PORT(dev);
	ptaddr = port ? MC68681_B_BASE : MC68681_A_BASE;
	imrbit = port ? DART_ISR_TXB : DART_ISR_TXA;

	s = spltty();

	if (tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP))
		goto bail;

	ttwakeupwr(tp);
	if (tp->t_outq.c_cc == 0)
		goto bail;

	tp->t_state |= TS_BUSY;
	while (tp->t_outq.c_cc != 0) {
		/* load transmitter until it is full */
		for (tries = 10000; tries != 0; tries --)
			if ((*sc->sc_read)(sc, ptaddr + DART_SRA) &
			    DART_SR_TX_READY)
				break;

		if (tries == 0) {
			if (!ISSET(sc->sc_sw_reg->imr, imrbit)) {
				sc->sc_sw_reg->imr |= imrbit;
				(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr);
			}
			goto bail;
		}

		c = getc(&tp->t_outq);
		(*sc->sc_write)(sc, ptaddr + DART_TBA, c & 0xff);
	}
	tp->t_state &= ~TS_BUSY;

bail:
	splx(s);
}

int
dartstop(struct tty *tp, int flag)
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);

	return 0;
}

/*
 * To be called at spltty - tty already locked.
 * Returns status of carrier.
 */

int
mc68681_mctl(struct mc68681_softc *sc, int port, int flags, int how)
{
	int op, newflags = 0;
	struct mc68681_line *line;
	int s;

	line = &sc->sc_line[port];

	s = spltty();

	if (flags & TIOCM_DTR) {
		op = sc->sc_hw[port].dtr_op;
		if (op >= 0) {
			newflags |= 1 << op;
			flags &= ~TIOCM_DTR;
		}
	}
	if (flags & TIOCM_RTS) {
		op = sc->sc_hw[port].rts_op;
		if (op >= 0) {
			newflags |= 1 << op;
			flags &= ~TIOCM_RTS;
		}
	}

	switch (how) {
	case DMSET:
		(*sc->sc_write)(sc, DART_OPRS, newflags);
		(*sc->sc_write)(sc, DART_OPRR, ~newflags);
		break;
	case DMBIS:
		(*sc->sc_write)(sc, DART_OPRS, newflags);
		break;
	case DMBIC:
		(*sc->sc_write)(sc, DART_OPRR, newflags);
		break;
	case DMGET:
		flags = 0;	/* XXX not supported */
		break;
	}

	splx(s);
	return flags;
}

int
dartioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error;
	int unit, port;
	struct tty *tp;
	struct mc68681_line *line;
	struct mc68681_softc *sc;

	unit = DART_UNIT(dev);
	if (unit >= dart_cd.cd_ndevs)
		return ENXIO;
	sc = (struct mc68681_softc *)dart_cd.cd_devs[unit];
	if (sc == NULL)
		return ENXIO;
	port = DART_PORT(dev);
	line = &sc->sc_line[port];
	tp = line->tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	switch (cmd) {
	case TIOCSBRK:
	case TIOCCBRK:
		break;
	case TIOCSDTR:
		(void)mc68681_mctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;
	case TIOCCDTR:
		(void)mc68681_mctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;
	case TIOCMSET:
		(void)mc68681_mctl(sc, port, *(int *)data, DMSET);
		break;
	case TIOCMBIS:
		(void)mc68681_mctl(sc, port, *(int *)data, DMBIS);
		break;
	case TIOCMBIC:
		(void)mc68681_mctl(sc, port, *(int *)data, DMBIC);
		break;
	case TIOCMGET:
		*(int *)data = mc68681_mctl(sc, port, 0, DMGET);
		break;
	case TIOCGFLAGS:
		if (sc->sc_consport == port)
			line->swflags |= TIOCFLAG_SOFTCAR;
		*(int *)data = line->swflags;
		break;
	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return EPERM;

		line->swflags = *(int *)data;
		if (sc->sc_consport == port)
			line->swflags |= TIOCFLAG_SOFTCAR;
		line->swflags &= /* only allow valid flags */
		    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return ENOTTY;
	}

	return 0;
}

int
mc68681_param(struct tty *tp, struct termios *t)
{
	int unit, port, s, acrupdate;
	const struct mc68681_s *spd;
	uint8_t acr, mr1, mr2;
	struct mc68681_line *line;
	struct mc68681_softc *sc;
	dev_t dev;
	uint ptaddr;

	dev = tp->t_dev;
	unit = DART_UNIT(dev);
	sc = (struct mc68681_softc *)dart_cd.cd_devs[unit];
	port = DART_PORT(dev);
	line = &sc->sc_line[port];
	ptaddr = port ? MC68681_B_BASE : MC68681_A_BASE;

	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	/* Reset to make global changes */
	if (sc->sc_consport != port) {
		/* disable Tx and Rx */
		if (port == A_PORT)
			sc->sc_sw_reg->imr &= ~(DART_ISR_TXA | DART_ISR_RXA);
		else
			sc->sc_sw_reg->imr &= ~(DART_ISR_TXB | DART_ISR_RXB);
		(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr);

		acrupdate = 0;

		/*
		 * Try to set baudrate. If the rate being asked for
		 * uses a different BRG than the other port, bail
		 * out with EAGAIN.
		 * Note that, upon close, we will reset to 9600
		 * bps which is compatible with both BRG.
		 */
		spd = mc68681_speed(tp->t_ispeed);
		if (spd == NULL)
			return EINVAL;
		if ((spd->brg_sets &
		    sc->sc_line[port ^ 1].speed->brg_sets) == 0)
			return EAGAIN;
		line->speed = spd;
		if (spd->brg_sets != (1 | 2)) {
			acr = sc->sc_sw_reg->acr;
			switch (acr & DART_ACR_BRG_SET_MASK) {
			case DART_ACR_BRG_SET_2:
				if (spd->brg_sets == 1) {
					acr &= ~DART_ACR_BRG_SET_MASK;
					acr |= DART_ACR_BRG_SET_1;
				}
				break;
			case DART_ACR_BRG_SET_1:
				if (spd->brg_sets == 2) {
					acr &= ~DART_ACR_BRG_SET_MASK;
					acr |= DART_ACR_BRG_SET_2;
				}
				break;
			}
			if (acr != sc->sc_sw_reg->acr) {
				sc->sc_sw_reg->acr = acr;
				acrupdate = 1;
			}
		}
		if (acrupdate == 0)
			(*sc->sc_write)(sc, ptaddr + DART_CSRA, spd->csr);

		/* get saved mode registers and clear set up parameters */
		mr1 = sc->sc_sw_reg->mr1[port];
		mr1 &= ~(DART_MR1_BPC_MASK | DART_MR1_PARITY_MASK);
		mr2 = sc->sc_sw_reg->mr2[port];
		mr2 &= ~DART_MR2_STOP_MASK;

		/* set up character size */
		switch (t->c_cflag & CSIZE) {
		case CS8:
			mr1 |= DART_MR1_BPC_8;
			break;
		case CS7:
			mr1 |= DART_MR1_BPC_7;
			break;
		case CS6:
			mr1 |= DART_MR1_BPC_6;
			break;
		case CS5:
			mr1 |= DART_MR1_BPC_5;
			break;
		}

		/* set up stop bits */
		if (t->c_cflag & CSTOPB)
			mr2 |= DART_MR2_STOP_2;
		else {
			/*
			 * When running in 5 bpc mode, low stop bit length
			 * values are actually off .5 stop bits.
			 */
			if ((t->c_cflag & CSIZE) == CS5)
				mr2 |= DART_MR2_STOP_1_CL5;
			else
				mr2 |= DART_MR2_STOP_1;
		}

		/* set up parity */
		if (t->c_cflag & PARENB) {
			mr1 |= DART_MR1_PARITY_ENABLE;
			if (t->c_cflag & PARODD)
				mr1 |= DART_MR1_PARITY_ENABLE_ODD;
			else
				mr1 |= DART_MR1_PARITY_ENABLE_EVEN;
		} else
			mr1 |= DART_MR1_PARITY_NONE;

		if (sc->sc_sw_reg->mr1[port] != mr1 ||
		    sc->sc_sw_reg->mr2[port] != mr2) {
			if (acrupdate == 0) {
				/* write mode registers to duart */
				(*sc->sc_write)(sc, ptaddr + DART_CRA,
				    DART_CR_RESET_MR1);
				(*sc->sc_write)(sc, ptaddr + DART_MRA, mr1);
				(*sc->sc_write)(sc, ptaddr + DART_MRA, mr2);
			}

			/* save changed mode registers */
			sc->sc_sw_reg->mr1[port] = mr1;
			sc->sc_sw_reg->mr2[port] = mr2;
		}

		if (acrupdate != 0) {
			s = spltty();
			mc68681_set_acr(sc);
			splx(s);
		}
	}

	/* enable transmitter? */
	if (tp->t_state & TS_BUSY) {
		if (port == A_PORT)
			sc->sc_sw_reg->imr |= DART_ISR_TXA;
		else
			sc->sc_sw_reg->imr |= DART_ISR_TXB;
		/* will be done below
		(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr); */
	}

	/* re-enable the receiver */
	if (port == A_PORT)
		sc->sc_sw_reg->imr |= DART_ISR_RXA;
	else
		sc->sc_sw_reg->imr |= DART_ISR_RXB;
	(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr);

	return 0;
}

void
mc68681_dcdint(struct mc68681_softc *sc)
{
	unsigned int dcdstate;
	uint8_t ipcr, ip, ip_bit;
	struct tty *tp;
	int port;
	struct mc68681_line *line;

	ipcr = (*sc->sc_read)(sc, DART_IPCR);
	ip = (*sc->sc_read)(sc, DART_IP);

	for (port = A_PORT; port <= B_PORT; port++) {
		ip_bit = 0;
		switch (sc->sc_hw[port].dcd_ip) {
		case 3:
			if (ipcr & DART_IPCR_IP3_CHANGED)
				ip_bit = DART_IP_IP3;
			break;
		case 2:
			if (ipcr & DART_IPCR_IP2_CHANGED)
				ip_bit = DART_IP_IP2;
			break;
		case 1:
			if (ipcr & DART_IPCR_IP1_CHANGED)
				ip_bit = DART_IP_IP1;
			break;
		case 0:
			if (ipcr & DART_IPCR_IP0_CHANGED)
				ip_bit = DART_IP_IP0;
			break;
		}

		if (ip_bit != 0) {
			dcdstate = ip & ip_bit;
			if (sc->sc_hw[port].dcd_active_low)
				dcdstate = !dcdstate;

			line = &sc->sc_line[port];
			tp = line->tty;
			if (tp != NULL)
				ttymodem(tp, dcdstate);
		}
	}
}

struct tty *
darttty(dev_t dev)
{
	int unit, port;
	struct mc68681_softc *sc;

	unit = DART_UNIT(dev);
	sc = (struct mc68681_softc *)dart_cd.cd_devs[unit];
	port = DART_PORT(dev);
	return sc->sc_line[port].tty;
}

int
dartopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int s, unit, port;
	struct mc68681_line *line;
	struct mc68681_softc *sc;
	struct tty *tp;

	unit = DART_UNIT(dev);
	if (unit >= dart_cd.cd_ndevs)
		return ENXIO;
	sc = (struct mc68681_softc *)dart_cd.cd_devs[unit];
	if (sc == NULL)
		return ENXIO;
	port = DART_PORT(dev);
	line = &sc->sc_line[port];

	s = spltty();
	if (line->tty != NULL)
		tp = line->tty;
	else
		tp = line->tty = ttymalloc(0);

	tp->t_oproc = mc68681_start;
	tp->t_param = mc68681_param;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = B9600;
		mc68681_param(tp, &tp->t_termios);
		if (sc->sc_consport == port) {
			/* console is 8N1 */
			tp->t_cflag = CREAD | CS8 | HUPCL;
		} else {
			tp->t_cflag = TTYDEF_CFLAG;
		}
		ttsetwater(tp);
		(void)mc68681_mctl(sc, port, TIOCM_DTR | TIOCM_RTS, DMSET);
		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && suser(p, 0) != 0) {
		splx(s);
		return EBUSY;
	}

	splx(s);
	return (*linesw[tp->t_line].l_open)(dev, tp, p);
}

int
dartclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct tty *tp;
	struct mc68681_line *line;
	struct mc68681_softc *sc;

	sc = (struct mc68681_softc *)dart_cd.cd_devs[DART_UNIT(dev)];
	line = &sc->sc_line[DART_PORT(dev)];
	tp = line->tty;

	(*linesw[tp->t_line].l_close)(tp, flag, p);
	ttyclose(tp);

	return 0;
}

int
dartread(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp;
	struct mc68681_softc *sc;

	sc = (struct mc68681_softc *)dart_cd.cd_devs[DART_UNIT(dev)];
	tp = sc->sc_line[DART_PORT(dev)].tty;

	return (*linesw[tp->t_line].l_read)(tp, uio, flag);
}

int
dartwrite(dev_t dev, struct uio *uio, int flag)
{
	struct tty *tp;
	struct mc68681_softc *sc;

	sc = (struct mc68681_softc *)dart_cd.cd_devs[DART_UNIT(dev)];
	tp = sc->sc_line[DART_PORT(dev)].tty;

	return (*linesw[tp->t_line].l_write)(tp, uio, flag);
}

void
mc68681_rxint(struct mc68681_softc *sc, int port)
{
	struct tty *tp;
	uint8_t data, sr;
	uint ptaddr;

	tp = sc->sc_line[port].tty;
	ptaddr = port ? MC68681_B_BASE : MC68681_A_BASE;

	/* read status reg */
	while ((sr = (*sc->sc_read)(sc, ptaddr + DART_SRA)) &
	    DART_SR_RX_READY) {
		/* read data and reset receiver */
		data = (*sc->sc_read)(sc, ptaddr + DART_RBA);

		if ((tp->t_state & (TS_ISOPEN|TS_WOPEN)) == 0 &&
		    sc->sc_consport != port) {
			return;
		}

		if (sr & DART_SR_BREAK) {
			/* clear break state */
			(*sc->sc_write)(sc, ptaddr + DART_CRA,
			    DART_CR_RESET_BREAK);
			(*sc->sc_write)(sc, ptaddr + DART_CRA,
			    DART_CR_RESET_ERROR);

#if defined(DDB)
			if (db_console != 0 && sc->sc_consport == port)
				Debugger();
#endif
		} else if (sr & (DART_SR_FRAME | DART_SR_PARITY |
		    DART_SR_OVERRUN)) { /* errors */
			if (sr & DART_SR_OVERRUN)
				printf("%s: receiver overrun port %c\n",
				    sc->sc_dev.dv_xname, 'A' + port);
			if (sr & DART_SR_FRAME)
				printf("%s: framing error port %c\n",
				    sc->sc_dev.dv_xname, 'A' + port);
			if (sr & DART_SR_PARITY)
				printf("%s: parity error port %c\n",
				    sc->sc_dev.dv_xname, 'A' + port);
			/* clear error state */
			(*sc->sc_write)(sc, ptaddr + DART_CRA,
			    DART_CR_RESET_ERROR);
		} else {
			/* no errors */
			if (ISSET(tp->t_state, TS_ISOPEN))
				(*linesw[tp->t_line].l_rint)(data,tp);
		}
	}
}

void
mc68681_txint(struct mc68681_softc *sc, int port)
{
	struct tty *tp;

	tp = sc->sc_line[port].tty;

	if ((tp->t_state & (TS_ISOPEN|TS_WOPEN))==0)
		goto out;

	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;

	if (tp->t_state & TS_BUSY) {
		tp->t_state &= ~TS_BUSY;
		(*linesw[tp->t_line].l_start)(tp);
		if (tp->t_state & TS_BUSY) {
			return;
		}
	}
out:

	/* disable transmitter */
	if (port == A_PORT)
		sc->sc_sw_reg->imr &= ~DART_ISR_TXA;
	else
		sc->sc_sw_reg->imr &= ~DART_ISR_TXB;

	(*sc->sc_write)(sc, DART_IMR, sc->sc_sw_reg->imr);
}

void
mc68681_intr(struct mc68681_softc *sc, uint8_t isr)
{
	if (isr & DART_ISR_IP_CHANGE)
		mc68681_dcdint(sc);

	if (isr & DART_ISR_RXA)
		mc68681_rxint(sc, A_PORT);
	if (isr & DART_ISR_RXB)
		mc68681_rxint(sc, B_PORT);

	if (isr & DART_ISR_TXA)
		mc68681_txint(sc, A_PORT);
	if (isr & DART_ISR_TXB)
		mc68681_txint(sc, B_PORT);

#if 0 /* not enabled in imr */
	if (isr & DART_ISR_DELTA_BREAK_A)
		(*sc->sc_write)(sc, DART_CRA, DART_CR_RESET_BREAK);
	if (isr & DART_ISR_DELTA_BREAK_B)
		(*sc->sc_write)(sc, DART_CRB, DART_CR_RESET_BREAK);
#endif
}
