/*	$NetBSD: zs.c,v 1.1.1.1 1995/07/25 23:12:07 chuck Exp $	*/

/*
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 * Serial I/O via an SCC,
 */
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <dev/cons.h>
#include <mvme68k/dev/iio.h>
#include <mvme68k/dev/scc.h>
#include <mvme68k/dev/pccreg.h>

#include "zs.h"
#if NZS > 0

/*#define PCLK_FREQ	8333333*/
#undef PCLK_FREQ		/* XXXCDC */
#define PCLK_FREQ	5000000
#define NZSLINE		(NZS*2)

#define RECV_BUF	512
#define ERROR_DET	0xed

#define TS_DRAIN	TS_FLUSH/* waiting for output to drain */

#define splzs()		spl4()

struct zs {
	short   flags;		/* see below */
	char    rr0;		/* holds previous CTS, DCD state */
	unsigned char imask;	/* mask for input chars */
	int     nzs_open;	/* # opens as /dev/zsn */
	int     nkbd_open;	/* # opens as a keyboard */
	int     gsp_unit;	/* unit to send kbd chars to */
	struct tty *tty;	/* link to tty structure */
	struct sccregs scc;	/* SCC shadow registers */
	u_char *rcv_get;
	u_char *rcv_put;
	u_char *rcv_end;
	volatile int rcv_count;
	int     rcv_len;
	char   *send_ptr;
	int     send_count;
	int     sent_count;
	volatile char modem_state;
	volatile char modem_change;
	volatile short hflags;
	char    rcv_buf[RECV_BUF];
};

/* Bits in flags */
#define ZS_SIDEA	1
#define ZS_INITED	2
#define ZS_INTEN	4
#define ZS_RESET	8
#define ZS_CONSOLE	0x20

/* Bits in hflags */
#define ZH_OBLOCK	1	/* output blocked by CTS */
#define ZH_SIRQ		2	/* soft interrupt request */
#define ZH_TXING	4	/* transmitter active */
#define ZH_RXOVF	8	/* receiver buffer overflow */

struct zssoftc {
	struct device dev;
	struct zs zs[2];
};

struct tty *zs_tty[NZSLINE];

struct termios zs_cons_termios;
int     zs_cons_unit = 0;
int     zs_is_console = 0;
struct sccregs *zs_cons_scc;

int zsopen __P((dev_t, int, int, struct proc *));
void zsstart __P((struct tty *));
int zsparam __P((struct tty *, struct termios *));
int zsirq __P((int unit));
void zs_softint __P((void));

unsigned long sir_zs;
void    zs_softint();

#define zsunit(dev)	(minor(dev) >> 1)
#define zsside(dev)	(minor(dev) & 1)

/*
 * Autoconfiguration stuff.
 */
void zsattach __P((struct device *, struct device *, void *));
int zsmatch __P((struct device *, void *, void *));

struct cfdriver zscd = {
	NULL, "zs", zsmatch, zsattach, DV_TTY, sizeof(struct zssoftc), 0
};

int
zsmatch(parent, vcf, args)
	struct device *parent;
	void   *vcf, *args;
{
	struct cfdata *cf = vcf;

	return !badbaddr((caddr_t) IIO_CFLOC_ADDR(cf));
}

void
zsattach(parent, self, args)
	struct device *parent, *self;
	void   *args;
{
	struct zssoftc *dv;
	struct zs *zp, *zc;
	u_char  ir;
	volatile struct scc *scc;
	int     zs_level = IIO_CFLOC_LEVEL(self->dv_cfdata);

	iio_print(self->dv_cfdata);

	/* connect the interrupt */
	dv = (struct zssoftc *) self;
	pccintr_establish(PCCV_ZS, zsirq, zs_level, self->dv_unit);
	/* XXXCDC: needs some work to handle zs1 */

	zp = &dv->zs[0];
	scc = (volatile struct scc *) IIO_CFLOC_ADDR(self->dv_cfdata);

	if (zs_is_console && self->dv_unit == zsunit(zs_cons_unit)) {
		/* SCC is the console - it's already reset */
		zc = zp + zsside(zs_cons_unit);
		zc->scc = *zs_cons_scc;
		zs_cons_scc = &zc->scc;
		zc->flags |= ZS_CONSOLE;
	} else {
		/* reset the SCC */
		scc->cr = 0;
		scc->cr = 9;
		scc->cr = 0xC0;	/* hardware reset of SCC, both sides */
	}

	/* side A */
	zp->scc.s_adr = scc + 1;
	zp->flags |= ZS_SIDEA | ZS_RESET;

	/* side B */
	++zp;
	zp->scc.s_adr = scc;
	zp->flags |= ZS_RESET;

	if (sir_zs == 0)
		sir_zs = allocate_sir(zs_softint, 0);
	printf("\n");

	ir = sys_pcc->zs_int;
	if ((ir & PCC_IMASK) != 0 && (ir & PCC_IMASK) != zs_level)
		panic("zs configured at different IPLs");
	sys_pcc->zs_int = zs_level | PCC_IENABLE | PCC_ZSEXTERN;
}

zs_ttydef(struct zs *zp)
{
	struct tty *tp = zp->tty;

	if ((zp->flags & ZS_CONSOLE) == 0) {
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
	} else
		tp->t_termios = zs_cons_termios;
	ttychars(tp);
	ttsetwater(tp);
	tp->t_oproc = zsstart;
	tp->t_param = zsparam;

	zp->rcv_get = zp->rcv_buf;
	zp->rcv_put = zp->rcv_buf;
	zp->rcv_end = zp->rcv_buf + sizeof(zp->rcv_buf);
	zp->rcv_len = sizeof(zp->rcv_buf) / 2;
}

struct tty *
zstty(dev)
	dev_t   dev;
{

	if (minor(dev) < NZSLINE)
		return (zs_tty[minor(dev)]);

	return (NULL);
}

/* ARGSUSED */
zsopen(dev_t dev, int flag, int mode, struct proc * p)
{
	register struct tty *tp;
	int     error;
	struct zs *zp;
	struct zssoftc *dv;

	if (zsunit(dev) > zscd.cd_ndevs
	    || (dv = (struct zssoftc *) zscd.cd_devs[zsunit(dev)]) == NULL)
		return ENODEV;

	zp = &dv->zs[zsside(dev)];
	if (zp->tty == NULL) {
		zp->tty = ttymalloc();
		zs_ttydef(zp);
		if (minor(dev) < NZSLINE)
			zs_tty[minor(dev)] = zp->tty;
	}
	tp = zp->tty;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		zs_init(zp);
		if ((zp->modem_state & SCC_DCD) != 0)
			tp->t_state |= TS_CARR_ON;
	} else
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
			return (EBUSY);

	error = ((*linesw[tp->t_line].l_open) (dev, tp));

	if (error == 0)
		++zp->nzs_open;
	return error;
}

int
zsclose(dev, flag, mode, p)
	dev_t   dev;
	int     flag, mode;
	struct proc *p;
{
	struct zs *zp;
	struct tty *tp;
	struct zssoftc *dv;
	int     s;

	if (zsunit(dev) > zscd.cd_ndevs
	    || (dv = (struct zssoftc *) zscd.cd_devs[zsunit(dev)]) == NULL)
		return ENODEV;
	zp = &dv->zs[zsside(dev)];
	tp = zp->tty;

	if (zp->nkbd_open == 0) {
		(*linesw[tp->t_line].l_close) (tp, flag);
		s = splzs();
		if ((zp->flags & ZS_CONSOLE) == 0 && (tp->t_cflag & HUPCL) != 0)
			ZBIC(&zp->scc, 5, 0x82);	/* drop DTR, RTS */
		ZBIC(&zp->scc, 3, 1);	/* disable receiver */
		splx(s);
		ttyclose(tp);
	}
	zp->nzs_open = 0;
	return (0);
}

/*ARGSUSED*/
zsread(dev, uio, flag)
	dev_t   dev;
	struct uio *uio;
	int     flag;
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(dev)];
	struct zs *zp = &dv->zs[zsside(dev)];
	struct tty *tp = zp->tty;

	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

/*ARGSUSED*/
zswrite(dev, uio, flag)
	dev_t   dev;
	struct uio *uio;
	int     flag;
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(dev)];
	struct zs *zp = &dv->zs[zsside(dev)];
	struct tty *tp = zp->tty;

	return ((*linesw[tp->t_line].l_write) (tp, uio, flag));
}

zsioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	caddr_t data;
	int     cmd, flag;
	struct proc *p;
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(dev)];
	struct zs *zp = &dv->zs[zsside(dev)];
	struct tty *tp = zp->tty;
	register struct sccregs *scc = &zp->scc;
	register int error, s;

	error = (*linesw[tp->t_line].l_ioctl) (tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	error = 0;
	s = splzs();
	switch (cmd) {
	case TIOCSDTR:
		ZBIS(scc, 5, 0x80);
		break;
	case TIOCCDTR:
		ZBIC(scc, 5, 0x80);
		break;
	case TIOCSBRK:
		splx(s);
		zs_drain(zp);
		s = splzs();
		ZBIS(scc, 5, 0x10);
		spltty();
		zs_unblock(tp);
		break;
	case TIOCCBRK:
		ZBIC(scc, 5, 0x10);
		break;
	case TIOCMGET:
		*(int *) data = zscc_mget(scc);
		break;
	case TIOCMSET:
		zscc_mset(scc, *(int *) data);
		zscc_mclr(scc, ~*(int *) data);
		break;
	case TIOCMBIS:
		zscc_mset(scc, *(int *) data);
		break;
	case TIOCMBIC:
		zscc_mclr(scc, *(int *) data);
		break;
	default:
		error = ENOTTY;
	}
	splx(s);
	return error;
}

zsparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &dv->zs[zsside(tp->t_dev)];
	register int s;

	zs_drain(zp);
	s = splzs();
	zp->imask = zscc_params(&zp->scc, t);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	if ((tp->t_cflag & CCTS_OFLOW) == 0)
		zp->hflags &= ~ZH_OBLOCK;
	else
		if ((zp->modem_state & 0x20) == 0)
			zp->hflags |= ZH_OBLOCK;
	spltty();
	zs_unblock(tp);
	splx(s);
	return 0;
}

void
zsstart(tp)
	struct tty *tp;
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &dv->zs[zsside(tp->t_dev)];
	register int s, n;

	s = spltty();
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_DRAIN)) == 0) {
		n = ndqb(&tp->t_outq, 0);
		if (n > 0) {
			tp->t_state |= TS_BUSY;
			splzs();
			zp->hflags |= ZH_TXING;
			zp->send_ptr = tp->t_outq.c_cf;
			zp->send_count = n;
			zp->sent_count = 0;
			zs_txint(zp);
			spltty();
		}
	}
	splx(s);
}

zsstop(struct tty * tp, int flag)
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &dv->zs[zsside(tp->t_dev)];
	int     s, n;

	s = splzs();
	zp->send_count = 0;
	n = zp->sent_count;
	zp->sent_count = 0;
	if ((tp->t_state & TS_BUSY) != 0 && (flag & FWRITE) == 0) {
		tp->t_state &= ~TS_BUSY;
		spltty();
		ndflush(&tp->t_outq, n);
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (tp->t_state & TS_ASLEEP) {
				tp->t_state &= ~TS_ASLEEP;
				wakeup((caddr_t) & tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
	}
	splx(s);
}

zs_init(zp)
	struct zs *zp;
{
	register int s;

	s = splzs();
	zscc_init(zp, &zp->tty->t_termios);
	zp->rr0 = zp->modem_state = ZREAD0(&zp->scc);
	ZBIS(&zp->scc, 1, 0x13);/* ints on tx, rx and ext/status */
	ZBIS(&zp->scc, 9, 8);	/* enable ints */
	zp->flags |= ZS_INTEN;
	splx(s);
}

zscc_init(zp, par)
	struct zs *zp;
	struct termios *par;
{
	struct sccregs *scc;

	scc = &zp->scc;
	ZWRITE(scc, 2, 0);
	ZWRITE(scc, 10, 0);
	ZWRITE(scc, 11, 0x50);	/* rx & tx clock = brgen */
	ZWRITE(scc, 14, 3);	/* brgen enabled, from pclk */
	zp->imask = zscc_params(scc, par);
	ZBIS(scc, 5, 0x82);	/* set DTR and RTS */
	zp->flags |= ZS_INITED;
}

int
zscc_params(scc, par)
	struct sccregs *scc;
	struct termios *par;
{
	unsigned divisor, speed;
	int     spd, imask, ints;

	speed = par->c_ospeed;
	if (speed == 0) {
		/* disconnect - drop DTR & RTS, disable receiver */
		ZBIC(scc, 5, 0x82);
		ZBIC(scc, 3, 1);
		return 0xFF;
	}
	if ((par->c_cflag & CREAD) == 0)
		ZBIC(scc, 3, 1);/* disable receiver */
	divisor = (PCLK_FREQ / 32 + (speed >> 1)) / speed - 2;
	ZWRITE(scc, 12, divisor);
	ZWRITE(scc, 13, divisor >> 8);
	switch (par->c_cflag & CSIZE) {
	case CS5:
		spd = 0;
		imask = 0x1F;
		break;
	case CS6:
		spd = 0x40;
		imask = 0x3F;
		break;
	case CS7:
		spd = 0x20;
		imask = 0x7F;
		break;
	default:
		spd = 0x60;
		imask = 0xFF;
	}
	ZWRITE(scc, 5, (scc->s_val[5] & ~0x60) | spd);
	ZWRITE(scc, 3, (scc->s_val[3] & ~0xC0) | (spd << 1));
	spd = par->c_cflag & CSTOPB ? 8 : 0;
	spd |= par->c_cflag & PARENB ? par->c_cflag & PARODD ? 1 : 3 : 0;
	ZWRITE(scc, 4, 0x44 | spd);
	ZBIS(scc, 5, 8);	/* enable transmitter */
	if ((par->c_cflag & CREAD) != 0)
		ZBIS(scc, 3, 1);/* enable receiver */
	ints = 0;
	if ((par->c_cflag & CLOCAL) == 0)
		ints |= SCC_DCD;
	if ((par->c_cflag & CCTS_OFLOW) != 0)
		ints |= SCC_CTS;
	ZWRITE(scc, 15, ints);
	return imask;
}

zscc_mget(register struct sccregs * scc)
{
	int     bits = 0, rr0;

	if ((scc->s_val[3] & SCC_RCVEN) != 0)
		bits |= TIOCM_LE;
	if ((scc->s_val[5] & SCC_DTR) != 0)
		bits |= TIOCM_DTR;
	if ((scc->s_val[5] & SCC_RTS) != 0)
		bits |= TIOCM_RTS;
	rr0 = ZREAD0(scc);
	if ((rr0 & SCC_CTS) != 0)
		bits |= TIOCM_CTS;
	if ((rr0 & SCC_DCD) != 0)
		bits |= TIOCM_CAR;
	return bits;
}

zscc_mset(register struct sccregs * scc, int bits)
{
	if ((bits & TIOCM_LE) != 0)
		ZBIS(scc, 3, SCC_RCVEN);
	if ((bits & TIOCM_DTR) != 0)
		ZBIS(scc, 5, SCC_DTR);
	if ((bits & TIOCM_RTS) != 0)
		ZBIS(scc, 5, SCC_RTS);
}

zscc_mclr(register struct sccregs * scc, int bits)
{
	if ((bits & TIOCM_LE) != 0)
		ZBIC(scc, 3, SCC_RCVEN);
	if ((bits & TIOCM_DTR) != 0)
		ZBIC(scc, 5, TIOCM_DTR);
	if ((bits & TIOCM_RTS) != 0)
		ZBIC(scc, 5, SCC_RTS);
}

zs_drain(register struct zs * zp)
{
	register int s;

	zp->tty->t_state |= TS_DRAIN;
	/* wait for Tx buffer empty and All sent bits to be set */
	s = splzs();
	while ((ZREAD0(&zp->scc) & SCC_TXRDY) == 0
	    || (ZREAD(&zp->scc, 1) & 1) == 0) {
		splx(s);
		DELAY(100);
		s = splzs();
	}
	splx(s);
}

zs_unblock(register struct tty * tp)
{
	tp->t_state &= ~TS_DRAIN;
	if (tp->t_outq.c_cc != 0)
		zsstart(tp);
}

/*
 * Hardware interrupt from an SCC.
 */
int
zsirq(int unit)
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[unit];
	register struct zs *zp = &dv->zs[0];
	register int ipend, x;
	register volatile struct scc *scc;

	x = splzs();
	scc = zp->scc.s_adr;
	scc->cr = 3;		/* read int pending from A side */
	DELAY(5);
	ipend = scc->cr;
	if ((ipend & 0x20) != 0)
		zs_rxint(zp);
	if ((ipend & 0x10) != 0)
		zs_txint(zp);
	if ((ipend & 0x8) != 0)
		zs_extint(zp);
	++zp;			/* now look for B side ints */
	if ((ipend & 0x4) != 0)
		zs_rxint(zp);
	if ((ipend & 0x2) != 0)
		zs_txint(zp);
	if ((ipend & 0x1) != 0)
		zs_extint(zp);
	splx(x);
	return ipend != 0;
}

zs_txint(register struct zs * zp)
{
	struct tty *tp = zp->tty;
	struct sccregs *scc;
	int     c;
	u_char *get;

	scc = &zp->scc;
	ZWRITE0(scc, 0x28);	/* reset Tx interrupt */
	if ((zp->hflags & ZH_OBLOCK) == 0) {
		get = zp->send_ptr;
		while ((ZREAD0(scc) & SCC_TXRDY) != 0 && zp->send_count > 0) {
			c = *get++;
			ZWRITED(scc, c);
			--zp->send_count;
			++zp->sent_count;
		}
		zp->send_ptr = get;
		if (zp->send_count == 0 && (zp->hflags & ZH_TXING) != 0) {
			zp->hflags &= ~ZH_TXING;
			zp->hflags |= ZH_SIRQ;
			setsoftint(sir_zs);
		}
	}
}

zs_rxint(register struct zs * zp)
{
	register int stat, c, n, extra;
	u_char *put;

	put = zp->rcv_put;
	n = zp->rcv_count;
	for (;;) {
		if ((ZREAD0(&zp->scc) & SCC_RXFULL) == 0)	/* check Rx full */
			break;
		stat = ZREAD(&zp->scc, 1) & 0x70;
		c = ZREADD(&zp->scc) & zp->imask;
		/* stat encodes parity, overrun, framing errors */
		if (stat != 0)
			ZWRITE0(&zp->scc, 0x30);	/* reset error */
		if ((zp->hflags & ZH_RXOVF) != 0) {
			zp->hflags &= ~ZH_RXOVF;
			stat |= 0x20;
		}
		extra = (stat != 0 || c == ERROR_DET) ? 2 : 0;
		if (n + extra + 1 < zp->rcv_len) {
			if (extra != 0) {
				*put++ = ERROR_DET;
				if (put >= zp->rcv_end)
					put = zp->rcv_buf;
				*put++ = stat;
				if (put >= zp->rcv_end)
					put = zp->rcv_buf;
				n += 2;
			}
			*put++ = c;
			if (put >= zp->rcv_end)
				put = zp->rcv_buf;
			++n;
		} else
			zp->hflags |= ZH_RXOVF;
	}
	if (n > zp->rcv_count) {
		zp->rcv_put = put;
		zp->rcv_count = n;
		zp->hflags |= ZH_SIRQ;
		setsoftint(sir_zs);
	}
}

/* Ext/status interrupt */
zs_extint(register struct zs * zp)
{
	int     rr0;
	struct tty *tp = zp->tty;

	rr0 = ZREAD0(&zp->scc);
	ZWRITE0(&zp->scc, 0x10);/* reset ext/status int */
	if ((tp->t_cflag & CCTS_OFLOW) != 0) {
		if ((rr0 & 0x20) == 0)
			zp->hflags |= ZH_OBLOCK;
		else {
			zp->hflags &= ~ZH_OBLOCK;
			if ((rr0 & SCC_TXRDY) != 0)
				zs_txint(zp);
		}
	}
	zp->modem_change |= rr0 ^ zp->modem_state;
	zp->modem_state = rr0;
	zp->hflags |= ZH_SIRQ;
	setsoftint(sir_zs);
}

void
zs_softint()
{
	int     s, n, n0, c, stat, rr0;
	struct zs *zp;
	struct tty *tp;
	u_char *get;
	int     unit, side;

	s = splzs();
	for (unit = 0; unit < zscd.cd_ndevs; ++unit) {
		if (zscd.cd_devs[unit] == NULL)
			continue;
		zp = &((struct zssoftc *) zscd.cd_devs[unit])->zs[0];
		for (side = 0; side < 2; ++side, ++zp) {
			if ((zp->hflags & ZH_SIRQ) == 0)
				continue;
			zp->hflags &= ~ZH_SIRQ;
			tp = zp->tty;

			/* check for tx done */
			spltty();
			if (tp != NULL && zp->send_count == 0
			    && (tp->t_state & TS_BUSY) != 0) {
				tp->t_state &= ~(TS_BUSY | TS_FLUSH);
				ndflush(&tp->t_outq, zp->sent_count);
				if (tp->t_outq.c_cc <= tp->t_lowat) {
					if (tp->t_state & TS_ASLEEP) {
						tp->t_state &= ~TS_ASLEEP;
						wakeup((caddr_t) & tp->t_outq);
					}
					selwakeup(&tp->t_wsel);
				}
				if (tp->t_line != 0)
					(*linesw[tp->t_line].l_start) (tp);
				else
					zsstart(tp);
			}
			splzs();

			/* check for received characters */
			get = zp->rcv_get;
			while (zp->rcv_count > 0) {
				c = *get++;
				if (get >= zp->rcv_end)
					get = zp->rcv_buf;
				if (c == ERROR_DET) {
					stat = *get++;
					if (get >= zp->rcv_end)
						get = zp->rcv_buf;
					c = *get++;
					if (get >= zp->rcv_end)
						get = zp->rcv_buf;
					zp->rcv_count -= 3;
				} else {
					stat = 0;
					--zp->rcv_count;
				}
				spltty();
				if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0)
					continue;
				if (zp->nzs_open == 0) {
#ifdef notdef
					if (stat == 0)
						kbd_newchar(zp->gsp_unit, c);
#endif
				} else {
					if ((stat & 0x10) != 0)
						c |= TTY_PE;
					if ((stat & 0x20) != 0) {
						log(LOG_WARNING, "zs: fifo overflow\n");
						c |= TTY_FE;	/* need some error for
								 * slip stuff */
					}
					if ((stat & 0x40) != 0)
						c |= TTY_FE;
					(*linesw[tp->t_line].l_rint) (c, tp);
				}
				splzs();
			}
			zp->rcv_get = get;

			/* check for modem lines changing */
			while (zp->modem_change != 0 || zp->modem_state != zp->rr0) {
				rr0 = zp->rr0 ^ zp->modem_change;
				zp->modem_change = rr0 ^ zp->modem_state;

				/* Check if DCD (carrier detect) has changed */
				if (tp != NULL && (rr0 & 8) != (zp->rr0 & 8)) {
					spltty();
					ttymodem(tp, rr0 & 8);
					/* XXX possibly should disable line if
					 * return value is 0 */
					splzs();
				}
				zp->rr0 = rr0;
			}
		}
	}
	splx(s);
}

/*
 * Routines to divert an SCC channel to the input side of /dev/gsp
 * for the keyboard.
 */
int
zs_kbdopen(int unit, int gsp_unit, struct termios * tiop, struct proc * p)
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(unit)];
	struct zs *zp = &dv->zs[zsside(unit)];
	int     error;

	error = zsopen(unit, 0, 0, p);
	if (error != 0)
		return error;
	++zp->nkbd_open;
	--zp->nzs_open;
	zsparam(zp->tty, tiop);
	zp->gsp_unit = gsp_unit;
	return 0;
}

void
zs_kbdclose(int unit)
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(unit)];
	struct zs *zp = &dv->zs[zsside(unit)];

	zp->nkbd_open = 0;
	if (zp->nzs_open == 0)
		zsclose(unit, 0, 0, 0);
}

void
zs_kbdput(int unit, int c)
{
	struct zssoftc *dv = (struct zssoftc *) zscd.cd_devs[zsunit(unit)];
	struct zs *zp = &dv->zs[zsside(unit)];
	struct tty *tp = zp->tty;

	putc(c, &tp->t_outq);
	zsstart(tp);
}

/*
 * Routines for using side A of the first SCC as a console.
 */

/* probe for the SCC; should check hardware */
zscnprobe(cp)
	struct consdev *cp;
{
	int     maj;
	char   *prom_cons;
	extern char *prom_getvar();

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == zsopen)
			break;

	/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_NORMAL;

	return 1;
}

/* initialize the keyboard for use as the console */
struct termios zscn_termios = {
	TTYDEF_IFLAG,
	TTYDEF_OFLAG,
	TTYDEF_CFLAG,
	TTYDEF_LFLAG,
	{0},
	TTYDEF_SPEED,
	TTYDEF_SPEED
};

struct sccregs zs_cons_sccregs;
int     zs_cons_imask;

unsigned zs_cons_addrs[] = {ZS0_PHYS, ZS1_PHYS};


zscninit()
{
	zs_cnsetup(0, &zscn_termios);
}

/* Polling routine for console input from a serial port. */
int
zscngetc(dev_t dev)
{
	register struct sccregs *scc = zs_cons_scc;
	int     c, s, stat;

	s = splzs();
	for (;;) {
		while ((ZREAD0(scc) & SCC_RXFULL) == 0)	/* wait for Rx full */
			;
		stat = ZREAD(scc, 1) & 0x70;
		c = ZREADD(scc) & zs_cons_imask;
		/* stat encodes parity, overrun, framing errors */
		if (stat == 0)
			break;
		ZWRITE0(scc, 0x30);	/* reset error */
	}
	splx(s);
	return c;
}

zscnputc(dev_t dev, int c)
{
	register struct sccregs *scc = zs_cons_scc;
	int     s;

	s = splzs();
	while ((ZREAD0(scc) & SCC_TXRDY) == 0);
	ZWRITED(scc, c);
	splx(s);
}

zs_cnsetup(int unit, struct termios * tiop)
{
	register volatile struct scc *scc_adr;
	register struct sccregs *scc;

	zs_cons_unit = unit;
	zs_is_console = 1;
	zs_cons_scc = scc = &zs_cons_sccregs;

	scc_adr = (volatile struct scc *) IIOV(zs_cons_addrs[zsunit(unit)]);

	scc_adr[1].cr = 0;
	scc_adr[1].cr = 9;
	scc_adr[1].cr = 0xC0;	/* hardware reset of SCC, both sides */
	if (!zsside(unit))
		++scc_adr;

	scc->s_adr = scc_adr;
	ZWRITE(scc, 2, 0);
	ZWRITE(scc, 10, 0);
	ZWRITE(scc, 11, 0x50);	/* rx & tx clock = brgen */
	ZWRITE(scc, 14, 3);	/* brgen enabled, from pclk */
	zs_cons_imask = zscc_params(scc, tiop);
	ZBIS(scc, 5, 0x82);	/* set DTR and RTS */

	zs_cons_termios = *tiop;/* save for later */
}

/*
 * Routines for using the keyboard SCC as the input side of
 * the 'gsp' console device.
 */

/* probe for the keyboard; should check hardware */
zs_kbdcnprobe(cp, unit)
	struct consdev *cp;
	int     unit;
{
	return (unsigned) unit < NZSLINE;
}
#endif				/* NZS */
