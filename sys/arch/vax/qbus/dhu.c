/*	$OpenBSD: dhu.c,v 1.6 2002/12/27 19:20:49 hugh Exp $	*/
/*	$NetBSD: dhu.c,v 1.19 2000/06/04 06:17:01 matt Exp $	*/
/*
 * Copyright (c) 1996  Ken C. Wellsch.  All rights reserved.
 * Copyright (c) 1992, 1993
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
#include <arch/vax/qbus/dhureg.h>

/* A DHU-11 has 16 ports while a DHV-11 has only 8. We use 16 by default */

#define	NDHULINE 	16

#define DHU_M2U(c)	((c)>>4)	/* convert minor(dev) to unit # */
#define DHU_LINE(u)	((u)&0xF)	/* extract line # from minor(dev) */

struct	dhu_softc {
	struct	device	sc_dev;		/* Device struct used by config */
	struct	evcnt	sc_rintrcnt;	/* Interrupt statistics */
	struct	evcnt	sc_tintrcnt;	/* Interrupt statistics */
	int		sc_type;	/* controller type, DHU or DHV */
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	bus_dma_tag_t	sc_dmat;
	struct {
		struct	tty *dhu_tty;	/* what we work on */
		bus_dmamap_t dhu_dmah;
		int	dhu_state;	/* to manage TX output status */
		short	dhu_cc;		/* character count on TX */
		short	dhu_modem;	/* modem bits state */
	} sc_dhu[NDHULINE];
};

#define IS_DHU			16	/* Unibus DHU-11 board linecount */
#define IS_DHV			 8	/* Q-bus DHV-11 or DHQ-11 */

#define STATE_IDLE		000	/* no current output in progress */
#define STATE_DMA_RUNNING	001	/* DMA TX in progress */
#define STATE_DMA_STOPPED	002	/* DMA TX was aborted */
#define STATE_TX_ONE_CHAR	004	/* did a single char directly */

/* Flags used to monitor modem bits, make them understood outside driver */

#define DML_DTR		TIOCM_DTR
#define DML_RTS		TIOCM_RTS
#define DML_CTS		TIOCM_CTS
#define DML_DCD		TIOCM_CD
#define DML_RI		TIOCM_RI
#define DML_DSR		TIOCM_DSR
#define DML_BRK		0100000		/* no equivalent, we will mask */

#define DHU_READ_WORD(reg) \
	bus_space_read_2(sc->sc_iot, sc->sc_ioh, reg)
#define DHU_WRITE_WORD(reg, val) \
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, reg, val)
#define DHU_READ_BYTE(reg) \
	bus_space_read_1(sc->sc_iot, sc->sc_ioh, reg)
#define DHU_WRITE_BYTE(reg, val) \
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, reg, val)


/*  On a stock DHV, channel pairs (0/1, 2/3, etc.) must use */
/* a baud rate from the same group.  So limiting to B is likely */
/* best, although clone boards like the ABLE QHV allow all settings. */

static struct speedtab dhuspeedtab[] = {
  {       0,	0		},	/* Groups  */
  {      50,	DHU_LPR_B50	},	/* A	   */
  {      75,	DHU_LPR_B75	},	/* 	 B */
  {     110,	DHU_LPR_B110	},	/* A and B */
  {     134,	DHU_LPR_B134	},	/* A and B */
  {     150,	DHU_LPR_B150	},	/* 	 B */
  {     300,	DHU_LPR_B300	},	/* A and B */
  {     600,	DHU_LPR_B600	},	/* A and B */
  {    1200,	DHU_LPR_B1200	},	/* A and B */
  {    1800,	DHU_LPR_B1800	},	/* 	 B */
  {    2000,	DHU_LPR_B2000	},	/* 	 B */
  {    2400,	DHU_LPR_B2400	},	/* A and B */
  {    4800,	DHU_LPR_B4800	},	/* A and B */
  {    7200,	DHU_LPR_B7200	},	/* A	   */
  {    9600,	DHU_LPR_B9600	},	/* A and B */
  {   19200,	DHU_LPR_B19200	},	/* 	 B */
  {   38400,	DHU_LPR_B38400	},	/* A	   */
  {      -1,	-1		}
};

static int	dhu_match(struct device *, struct cfdata *, void *);
static void	dhu_attach(struct device *, struct device *, void *);
static	void	dhurint(void *);
static	void	dhuxint(void *);
static	void	dhustart(struct tty *);
static	int	dhuparam(struct tty *, struct termios *);
static	int	dhuiflow(struct tty *, int);
static unsigned	dhumctl(struct dhu_softc *,int, int, int);
	int	dhuopen(dev_t, int, int, struct proc *);
	int	dhuclose(dev_t, int, int, struct proc *);
	int	dhuread(dev_t, struct uio *, int);
	int	dhuwrite(dev_t, struct uio *, int);
	int	dhuioctl(dev_t, u_long, caddr_t, int, struct proc *);
	void	dhustop(struct tty *, int);
struct tty *	dhutty(dev_t);

struct	cfattach dhu_ca = {
	sizeof(struct dhu_softc), (cfmatch_t)dhu_match, dhu_attach
};

struct	cfdriver dhu_cd = {
	NULL, "dhu", DV_TTY
};

/* Autoconfig handles: setup the controller to interrupt, */
/* then complete the housecleaning for full operation */

static int
dhu_match(parent, cf, aux)
        struct device *parent;
	struct cfdata *cf;
        void *aux;
{
	struct uba_attach_args *ua = aux;
	int n;

	/* Reset controller to initialize, enable TX/RX interrupts */
	/* to catch floating vector info elsewhere when completed */

	bus_space_write_2(ua->ua_iot, ua->ua_ioh, DHU_UBA_CSR,
	    DHU_CSR_MASTER_RESET | DHU_CSR_RXIE | DHU_CSR_TXIE);

	/* Now wait up to 3 seconds for self-test to complete. */

	for (n = 0; n < 300; n++) {
		DELAY(10000);
		if ((bus_space_read_2(ua->ua_iot, ua->ua_ioh, DHU_UBA_CSR) &
		    DHU_CSR_MASTER_RESET) == 0)
			break;
	}

	/* If the RESET did not clear after 3 seconds, */
	/* the controller must be broken. */

	if (n >= 300)
		return 0;

	/* Check whether diagnostic run has signalled a failure. */

	if ((bus_space_read_2(ua->ua_iot, ua->ua_ioh, DHU_UBA_CSR) &
	    DHU_CSR_DIAG_FAIL) != 0)
		return 0;

       	return 1;
}

static void
dhu_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct dhu_softc *sc = (void *)self;
	struct uba_attach_args *ua = aux;
	unsigned c;
	int n, i;

	sc->sc_iot = ua->ua_iot;
	sc->sc_ioh = ua->ua_ioh;
	sc->sc_dmat = ua->ua_dmat;
	/* Process the 8 bytes of diagnostic info put into */
	/* the FIFO following the master reset operation. */

	printf("\n%s:", self->dv_xname);
	for (n = 0; n < 8; n++) {
		c = DHU_READ_WORD(DHU_UBA_RBUF);

		if ((c&DHU_DIAG_CODE) == DHU_DIAG_CODE) {
			if ((c&0200) == 0000)
				printf(" rom(%d) version %d",
					((c>>1)&01), ((c>>2)&037));
			else if (((c>>2)&07) != 0)
				printf(" diag-error(proc%d)=%x",
					((c>>1)&01), ((c>>2)&07));
		}
	}

	c = DHU_READ_WORD(DHU_UBA_STAT);

	sc->sc_type = (c & DHU_STAT_DHU)? IS_DHU: IS_DHV;

	if (sc->sc_type == IS_DHU) {
		if (c & DHU_STAT_MDL)
			i = 16;		/* "Modem Low" */
		else
			i = 8;		/* Has modem support */
	} else
		i = 8;

	printf("\n%s: DH%s-11 %d lines\n", self->dv_xname,
	    (sc->sc_type == IS_DHU) ? "U" : "V", i);

	sc->sc_type = i;

	for (i = 0; i < sc->sc_type; i++) {
		struct tty *tp;
		tp = sc->sc_dhu[i].dhu_tty = ttymalloc();
		sc->sc_dhu[i].dhu_state = STATE_IDLE;
		bus_dmamap_create(sc->sc_dmat, tp->t_outq.c_cn, 1, 
		    tp->t_outq.c_cn, 0, BUS_DMA_ALLOCNOW|BUS_DMA_NOWAIT,
		    &sc->sc_dhu[i].dhu_dmah);
		bus_dmamap_load(sc->sc_dmat, sc->sc_dhu[i].dhu_dmah,
		    tp->t_outq.c_cs, tp->t_outq.c_cn, 0, BUS_DMA_NOWAIT);
			
	}

	/* Now establish RX & TX interrupt handlers */

	uba_intr_establish(ua->ua_icookie, ua->ua_cvec,
	    dhurint, sc, &sc->sc_rintrcnt);
	uba_intr_establish(ua->ua_icookie, ua->ua_cvec + 4,
	    dhuxint, sc, &sc->sc_tintrcnt);
	evcnt_attach(&sc->sc_dev, "rintr", &sc->sc_rintrcnt);
	evcnt_attach(&sc->sc_dev, "tintr", &sc->sc_tintrcnt);
}

/* Receiver Interrupt */

static void
dhurint(arg)
	void *arg;
{
	struct	dhu_softc *sc = arg;
	struct tty *tp;
	int cc, line;
	unsigned c, delta;
	int overrun = 0;

	while ((c = DHU_READ_WORD(DHU_UBA_RBUF)) & DHU_RBUF_DATA_VALID) {

		/* Ignore diagnostic FIFO entries. */

		if ((c & DHU_DIAG_CODE) == DHU_DIAG_CODE)
			continue;

		cc = c & 0xFF;
		line = DHU_LINE(c>>8);
		tp = sc->sc_dhu[line].dhu_tty;

		/* LINK.TYPE is set so we get modem control FIFO entries */

		if ((c & DHU_DIAG_CODE) == DHU_MODEM_CODE) {
			c = (c << 8);
			/* Do MDMBUF flow control, wakeup sleeping opens */
			if (c & DHU_STAT_DCD) {
				if (!(tp->t_state & TS_CARR_ON))
				    (void)(*linesw[tp->t_line].l_modem)(tp, 1);
			}
			else if ((tp->t_state & TS_CARR_ON) &&
				(*linesw[tp->t_line].l_modem)(tp, 0) == 0)
					(void) dhumctl(sc, line, 0, DMSET);

			/* Do CRTSCTS flow control */
			delta = c ^ sc->sc_dhu[line].dhu_modem;
			sc->sc_dhu[line].dhu_modem = c;
			if ((delta & DHU_STAT_CTS) &&
			    (tp->t_state & TS_ISOPEN) &&
			    (tp->t_cflag & CRTSCTS)) {
				if (c & DHU_STAT_CTS) {
					tp->t_state &= ~TS_TTSTOP;
					ttstart(tp);
				} else {
					tp->t_state |= TS_TTSTOP;
					dhustop(tp, 0);
				}
			}
			continue;
		}

		if (!(tp->t_state & TS_ISOPEN)) {
			wakeup((caddr_t)&tp->t_rawq);
			continue;
		}

		if ((c & DHU_RBUF_OVERRUN_ERR) && overrun == 0) {
			log(LOG_WARNING, "%s: silo overflow, line %d\n",
				sc->sc_dev.dv_xname, line);
			overrun = 1;
		}
		/* A BREAK key will appear as a NULL with a framing error */
		if (c & DHU_RBUF_FRAMING_ERR)
			cc |= TTY_FE;
		if (c & DHU_RBUF_PARITY_ERR)
			cc |= TTY_PE;

		(*linesw[tp->t_line].l_rint)(cc, tp);
	}
}

/* Transmitter Interrupt */

static void
dhuxint(arg)
	void *arg;
{
	struct	dhu_softc *sc = arg;
	struct tty *tp;
	int line;

	line = DHU_LINE(DHU_READ_BYTE(DHU_UBA_CSR_HI));

	tp = sc->sc_dhu[line].dhu_tty;

	tp->t_state &= ~TS_BUSY;
	if (tp->t_state & TS_FLUSH)
		tp->t_state &= ~TS_FLUSH;
	else {
		if (sc->sc_dhu[line].dhu_state == STATE_DMA_STOPPED)
			sc->sc_dhu[line].dhu_cc -= 
			DHU_READ_WORD(DHU_UBA_TBUFCNT);
		ndflush(&tp->t_outq, sc->sc_dhu[line].dhu_cc);
		sc->sc_dhu[line].dhu_cc = 0;
	}

	sc->sc_dhu[line].dhu_state = STATE_IDLE;

	if (tp->t_line)
		(*linesw[tp->t_line].l_start)(tp);
	else
		dhustart(tp);
}

int
dhuopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	int unit, line;
	struct dhu_softc *sc;
	int s, error = 0;

	unit = DHU_M2U(minor(dev));
	line = DHU_LINE(minor(dev));

	if (unit >= dhu_cd.cd_ndevs || dhu_cd.cd_devs[unit] == NULL)
		return (ENXIO);

	sc = dhu_cd.cd_devs[unit];

	if (line >= sc->sc_type)
		return ENXIO;

	s = spltty();
	DHU_WRITE_BYTE(DHU_UBA_CSR, DHU_CSR_RXIE | line);
	sc->sc_dhu[line].dhu_modem = DHU_READ_WORD(DHU_UBA_STAT);
	splx(s);

	tp = sc->sc_dhu[line].dhu_tty;

	tp->t_oproc   = dhustart;
	tp->t_param   = dhuparam;
	tp->t_hwiflow = dhuiflow;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN; /* XXX */
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
		}
		(void) dhuparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if ((tp->t_state & TS_XCLUDE) && curproc->p_ucred->cr_uid != 0)
		return (EBUSY);
	/* Use DMBIS and *not* DMSET or else we clobber incoming bits */
	if (dhumctl(sc, line, DML_DTR|DML_RTS, DMBIS) & DML_DCD)
		tp->t_state |= TS_CARR_ON;
	s = spltty();
	while (!(flag & O_NONBLOCK) && !(tp->t_cflag & CLOCAL) &&
	       !(tp->t_state & TS_CARR_ON)) {
		tp->t_state |= TS_WOPEN; /* XXX */
		error = ttysleep(tp, (caddr_t)&tp->t_rawq,
				TTIPRI | PCATCH, ttopen, 0);
		if (error)
			break;
	}
	splx(s);
	if (error)
		return (error);
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
dhuclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	int unit, line;
	struct dhu_softc *sc;

	unit = DHU_M2U(minor(dev));
	line = DHU_LINE(minor(dev));

	sc = dhu_cd.cd_devs[unit];

	tp = sc->sc_dhu[line].dhu_tty;

	(*linesw[tp->t_line].l_close)(tp, flag);

	/* Make sure a BREAK state is not left enabled. */

	(void) dhumctl(sc, line, DML_BRK, DMBIC);

	/* Do a hangup if so required. */

	if ((tp->t_cflag & HUPCL) || (tp->t_state & TS_WOPEN) || /* XXX */
	    !(tp->t_state & TS_ISOPEN))
		(void) dhumctl(sc, line, 0, DMSET);

	return (ttyclose(tp));
}

int
dhuread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	struct dhu_softc *sc;
	struct tty *tp;

	sc = dhu_cd.cd_devs[DHU_M2U(minor(dev))];

	tp = sc->sc_dhu[DHU_LINE(minor(dev))].dhu_tty;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
dhuwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
{
	struct dhu_softc *sc;
	struct tty *tp;

	sc = dhu_cd.cd_devs[DHU_M2U(minor(dev))];

	tp = sc->sc_dhu[DHU_LINE(minor(dev))].dhu_tty;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

/*ARGSUSED*/
int
dhuioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct dhu_softc *sc;
	struct tty *tp;
	int unit, line;
	int error;

	unit = DHU_M2U(minor(dev));
	line = DHU_LINE(minor(dev));
	sc = dhu_cd.cd_devs[unit];
	tp = sc->sc_dhu[line].dhu_tty;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	switch (cmd) {

	case TIOCSBRK:
		(void) dhumctl(sc, line, DML_BRK, DMBIS);
		break;

	case TIOCCBRK:
		(void) dhumctl(sc, line, DML_BRK, DMBIC);
		break;

	case TIOCSDTR:
		(void) dhumctl(sc, line, DML_DTR|DML_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) dhumctl(sc, line, DML_DTR|DML_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) dhumctl(sc, line, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) dhumctl(sc, line, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) dhumctl(sc, line, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = (dhumctl(sc, line, 0, DMGET) & ~DML_BRK);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

struct tty *
dhutty(dev)
        dev_t dev;
{
	struct dhu_softc *sc = dhu_cd.cd_devs[DHU_M2U(minor(dev))];
	struct tty *tp = sc->sc_dhu[DHU_LINE(minor(dev))].dhu_tty;
        return (tp);
}

/*ARGSUSED*/
void
dhustop(tp, flag)
	struct tty *tp;
{
	struct dhu_softc *sc;
	int line;
	int s;

	s = spltty();

	if (tp->t_state & TS_BUSY) {

		sc = dhu_cd.cd_devs[DHU_M2U(minor(tp->t_dev))];
		line = DHU_LINE(minor(tp->t_dev));

		if (sc->sc_dhu[line].dhu_state == STATE_DMA_RUNNING) {

			sc->sc_dhu[line].dhu_state = STATE_DMA_STOPPED;

			DHU_WRITE_BYTE(DHU_UBA_CSR, DHU_CSR_RXIE | line);
			DHU_WRITE_WORD(DHU_UBA_LNCTRL, 
			    DHU_READ_WORD(DHU_UBA_LNCTRL) | 
			    DHU_LNCTRL_DMA_ABORT);
		}

		if (!(tp->t_state & TS_TTSTOP))
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

static void
dhustart(tp)
	struct tty *tp;
{
	struct dhu_softc *sc;
	int line, cc;
	int addr;
	int s;

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
	cc = ndqb(&tp->t_outq, 0);
	if (cc == 0) 
		goto out;

	tp->t_state |= TS_BUSY;

	sc = dhu_cd.cd_devs[DHU_M2U(minor(tp->t_dev))];

	line = DHU_LINE(minor(tp->t_dev));

	DHU_WRITE_BYTE(DHU_UBA_CSR, DHU_CSR_RXIE | line);

	sc->sc_dhu[line].dhu_cc = cc;

	if (cc == 1) {

		sc->sc_dhu[line].dhu_state = STATE_TX_ONE_CHAR;
		
		DHU_WRITE_WORD(DHU_UBA_TXCHAR, 
		    DHU_TXCHAR_DATA_VALID | *tp->t_outq.c_cf);

	} else {

		sc->sc_dhu[line].dhu_state = STATE_DMA_RUNNING;

		addr = sc->sc_dhu[line].dhu_dmah->dm_segs[0].ds_addr +
			(tp->t_outq.c_cf - tp->t_outq.c_cs);

		DHU_WRITE_WORD(DHU_UBA_TBUFCNT, cc);
		DHU_WRITE_WORD(DHU_UBA_TBUFAD1, addr & 0xFFFF);
		DHU_WRITE_WORD(DHU_UBA_TBUFAD2, ((addr>>16) & 0x3F) |
		    DHU_TBUFAD2_TX_ENABLE);
		DHU_WRITE_WORD(DHU_UBA_LNCTRL, 
		    DHU_READ_WORD(DHU_UBA_LNCTRL) & ~DHU_LNCTRL_DMA_ABORT);
		DHU_WRITE_WORD(DHU_UBA_TBUFAD2,
		    DHU_READ_WORD(DHU_UBA_TBUFAD2) | DHU_TBUFAD2_DMA_START);
	}
out:
	splx(s);
	return;
}

static int
dhuparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct dhu_softc *sc;
	int cflag = t->c_cflag;
	int ispeed = ttspeedtab(t->c_ispeed, dhuspeedtab);
	int ospeed = ttspeedtab(t->c_ospeed, dhuspeedtab);
	unsigned lpr, lnctrl;
	int unit, line;
	int s;

	unit = DHU_M2U(minor(tp->t_dev));
	line = DHU_LINE(minor(tp->t_dev));

	sc = dhu_cd.cd_devs[unit];

	/* check requested parameters */
        if (ospeed < 0 || ispeed < 0)
                return (EINVAL);

        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	if (ospeed == 0) {
		(void) dhumctl(sc, line, 0, DMSET);	/* hang up line */
		return (0);
	}

	s = spltty();
	DHU_WRITE_BYTE(DHU_UBA_CSR, DHU_CSR_RXIE | line);

	lpr = ((ispeed&017)<<8) | ((ospeed&017)<<12) ;

	switch (cflag & CSIZE) {

	case CS5:
		lpr |= DHU_LPR_5_BIT_CHAR;
		break;

	case CS6:
		lpr |= DHU_LPR_6_BIT_CHAR;
		break;

	case CS7:
		lpr |= DHU_LPR_7_BIT_CHAR;
		break;

	default:
		lpr |= DHU_LPR_8_BIT_CHAR;
		break;
	}

	if (cflag & PARENB)
		lpr |= DHU_LPR_PARENB;
	if (!(cflag & PARODD))
		lpr |= DHU_LPR_EPAR;
	if (cflag & CSTOPB)
		lpr |= DHU_LPR_2_STOP;

	DHU_WRITE_WORD(DHU_UBA_LPR, lpr);

	DHU_WRITE_WORD(DHU_UBA_TBUFAD2, 
	    DHU_READ_WORD(DHU_UBA_TBUFAD2) | DHU_TBUFAD2_TX_ENABLE);

	lnctrl = DHU_READ_WORD(DHU_UBA_LNCTRL);

	/* Setting LINK.TYPE enables modem signal change interrupts. */

	lnctrl |= (DHU_LNCTRL_RX_ENABLE | DHU_LNCTRL_LINK_TYPE);

	/* Enable the auto XON/XOFF feature on the controller */

	if (t->c_iflag & IXON)
		lnctrl |= DHU_LNCTRL_OAUTO;
	else
		lnctrl &= ~DHU_LNCTRL_OAUTO;

	if (t->c_iflag & IXOFF)
		lnctrl |= DHU_LNCTRL_IAUTO;
	else
		lnctrl &= ~DHU_LNCTRL_IAUTO;

	DHU_WRITE_WORD(DHU_UBA_LNCTRL, lnctrl);

	splx(s);
	return (0);
}

static int
dhuiflow(tp, flag)
	struct tty *tp;
	int flag;
{
	struct dhu_softc *sc;
	int line = DHU_LINE(minor(tp->t_dev));

	if (tp->t_cflag & CRTSCTS) {
		sc = dhu_cd.cd_devs[DHU_M2U(minor(tp->t_dev))];
		(void) dhumctl(sc, line, DML_RTS, ((flag)? DMBIC: DMBIS));
		return (1);
	}
	return (0);
}

static unsigned
dhumctl(sc, line, bits, how)
	struct dhu_softc *sc;
	int line, bits, how;
{
	unsigned status;
	unsigned lnctrl;
	unsigned mbits;
	int s;

	s = spltty();

	DHU_WRITE_BYTE(DHU_UBA_CSR, DHU_CSR_RXIE | line);

	mbits = 0;

	/* external signals as seen from the port */

	status = DHU_READ_WORD(DHU_UBA_STAT);

	if (status & DHU_STAT_CTS)
		mbits |= DML_CTS;

	if (status & DHU_STAT_DCD)
		mbits |= DML_DCD;

	if (status & DHU_STAT_DSR)
		mbits |= DML_DSR;

	if (status & DHU_STAT_RI)
		mbits |= DML_RI;

	/* internal signals/state delivered to port */

	lnctrl = DHU_READ_WORD(DHU_UBA_LNCTRL);

	if (lnctrl & DHU_LNCTRL_RTS)
		mbits |= DML_RTS;

	if (lnctrl & DHU_LNCTRL_DTR)
		mbits |= DML_DTR;

	if (lnctrl & DHU_LNCTRL_BREAK)
		mbits |= DML_BRK;

	switch (how) {

	case DMSET:
		mbits = bits;
		break;

	case DMBIS:
		mbits |= bits;
		break;

	case DMBIC:
		mbits &= ~bits;
		break;

	case DMGET:
		splx(s);
		return (mbits);
	}

	if (mbits & DML_RTS)
		lnctrl |= DHU_LNCTRL_RTS;
	else
		lnctrl &= ~DHU_LNCTRL_RTS;

	if (mbits & DML_DTR)
		lnctrl |= DHU_LNCTRL_DTR;
	else
		lnctrl &= ~DHU_LNCTRL_DTR;

	if (mbits & DML_BRK)
		lnctrl |= DHU_LNCTRL_BREAK;
	else
		lnctrl &= ~DHU_LNCTRL_BREAK;

	DHU_WRITE_WORD(DHU_UBA_LNCTRL, lnctrl);

	splx(s);
	return (mbits);
}
