/*	$OpenBSD: zs.c,v 1.9 2000/03/26 23:31:59 deraadt Exp $ */

/*
 * Copyright (c) 2000 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>
#include <mvme68k/dev/scc.h>

#include "pcc.h"
#include "mc.h"

#if NPCC > 0
#include <mvme68k/dev/pccreg.h>
#endif
#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif


#include "zs.h"

#define NZSLINE		(NZS*2)

#define RECV_BUF	512
#define ERROR_DET	0xed

#define TS_DRAIN	TS_FLUSH /* waiting for output to drain */

#define splzs()		spl4()

struct zs {
	short    flags;		/* see below */
	char     rr0;		/* holds previous CTS, DCD state */
	u_char      imask;		/* mask for input chars */
	int      nzs_open;	/* # opens as /dev/zsn */
	struct tty  *tty;		/* link to tty structure */
	struct sccregs scc;		/* SCC shadow registers */
	u_char      *rcv_get;
	u_char      *rcv_put;
	u_char      *rcv_end;
	volatile int   rcv_count;
	int      rcv_len;
	char     *send_ptr;
	int      send_count;
	int      sent_count;
	volatile char  modem_state;
	volatile char  modem_change;
	volatile short hflags;
	char     rcv_buf[RECV_BUF];
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
	struct device  sc_dev;
	struct zs   sc_zs[2];
	struct evcnt   sc_intrcnt;
	struct intrhand   sc_ih;
#if NPCC > 0
	struct pccreg  *sc_pcc;
#endif
#if NMC > 0
	struct mcreg   *sc_mc;
#endif
	int      sc_flags;
};
#define ZSSF_85230	1

struct tty *zs_tty[NZSLINE];

struct   termios zs_cons_termios;
int   zs_cons_unit = 0;
int   zs_is_console = 0;
struct   sccregs *zs_cons_scc;

int   zsopen   __P((dev_t, int, int, struct proc *));
void  zsstart  __P((struct tty *));
int   zsparam  __P((struct tty *, struct termios *));
int   zsirq __P((int unit));
int   zsregs   __P((void *va, int unit, volatile u_char **crp,
						  volatile u_char **drp));
int   zspclk   __P((void));

u_long   sir_zs;
void  zs_softint __P((void));

#define zsunit(dev)	(minor(dev) >> 1)
#define zsside(dev)	(minor(dev) & 1)

/*
 * Autoconfiguration stuff.
 */
void  zsattach __P((struct device *, struct device *, void *));
int   zsmatch __P((struct device *, void *, void *));

struct cfattach zs_ca = {
	sizeof(struct zssoftc), zsmatch, zsattach
};

struct cfdriver zs_cd = {
	NULL, "zs", DV_TTY, 0
};

int
zsmatch(parent, vcf, args)
struct device *parent;
void  *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	unsigned char *zstest = (unsigned char *)ca->ca_vaddr;
	/* 
	 * If zs1 is in the config, we must test to see if it really exists.  
	 * Some 162s only have one scc device, but the memory location for 
	 * the second scc still checks valid and every byte contains 0xFF. So 
	 * this is what we test with for now. XXX - smurph
	 */
	if (!badvaddr(ca->ca_vaddr, 1))
		if (*zstest == 0xFF)
			return (0);
		else
			return (1);
	else
		return (0);
}

void
zsattach(parent, self, args)
struct device *parent, *self;
void  *args;
{
	struct zssoftc *sc;
	struct zs *zp, *zc;
	u_char  ir;
	volatile struct scc *scc;
	volatile u_char *scc_cr, *scc_dr;
	struct confargs *ca = args;
	int     zs_level = ca->ca_ipl;
	int   size;
	static int initirq = 0;

	/* connect the interrupt */
	sc = (struct zssoftc *) self;

	sc->sc_ih.ih_fn = zsirq;
	sc->sc_ih.ih_arg = (void *)self->dv_unit;
	sc->sc_ih.ih_ipl = zs_level;
	switch (ca->ca_bustype) {
#if NPCC > 0
		case BUS_PCC:
			pccintr_establish(PCCV_ZS, &sc->sc_ih);
			sc->sc_pcc = (struct pccreg *)ca->ca_master;
			break;
#endif
#if NMC > 0
		case BUS_MC:
			if (sys_mc->mc_chiprev == 0x01)
				/* 
				 * MC rev 0x01 has a bug and can not access scc regs directly. 
				 * Macros will do the right thing based on the value of 
				 * mc_rev1_bug - XXX smurph 
				 */
				mc_rev1_bug = 1; /* defined in scc.h */
			mcintr_establish(MCV_ZS, &sc->sc_ih);
			sc->sc_mc = (struct mcreg *)ca->ca_master;
			break;
#endif
	}

	zp = &sc->sc_zs[0];
	scc = (volatile struct scc *)ca->ca_vaddr;

	/*
	 * the locations of the control and data register move around
	 * on different MVME models, so we generate independent pointers
	 * to them.
	 */
	size = zsregs(ca->ca_vaddr, 0, &scc_cr, &scc_dr);

	if (zs_is_console && self->dv_unit == zsunit(zs_cons_unit)) {
		/* SCC is the console - it's already reset */
		zc = zp + zsside(zs_cons_unit);
		zc->scc = *zs_cons_scc;
		zs_cons_scc = &zc->scc;
		zc->flags |= ZS_CONSOLE;
	} else {
		/* reset the SCC */
		*(scc_cr + size) = 0;
		*(scc_cr + size) = 9;
		*(scc_cr + size) = 0xC0;	/* hardware reset of SCC, both sides */
	}

	/* side A */
	zp->scc.s_cr = scc_cr + size;
	zp->scc.s_dr = scc_dr + size;
	zp->flags |= ZS_SIDEA | ZS_RESET;

	/* side B */
	++zp;
	zp->scc.s_cr = scc_cr;
	zp->scc.s_dr = scc_dr;
	zp->flags |= ZS_RESET;

	if (sir_zs == 0)
		sir_zs = allocate_sir(zs_softint, 0);

	printf("\n");

	/*
	 * XXX we end up doing this twice... once for
	 * each ZS chip. We should really not turn interrupts on until
	 * we have initialized the last of the two chips. But then we
	 * will need to search the config system to see if we will be
	 * called for the 2nd chip... otherwise, a config without a zs1
	 * would never enable interrupts!
	 */
	switch (ca->ca_bustype) {
#if NPCC > 0
		case BUS_PCC:
			ir = sc->sc_pcc->pcc_zsirq;
			if ((ir & PCC_IRQ_IPL) != 0 && (ir & PCC_IRQ_IPL) != zs_level)
				panic("zs configured at different IPLs");
			if (initirq)
				break;
			sc->sc_pcc->pcc_zsirq = zs_level | PCC_IRQ_IEN | PCC_ZS_PCCVEC;
			break;
#endif
#if NMC > 0
		case BUS_MC:
			ir = sc->sc_mc->mc_zsirq;
			if ((ir & MC_IRQ_IPL) != 0 && (ir & MC_IRQ_IPL) != zs_level)
				panic("zs configured at different IPLs");
			if (initirq)
				break;
			sc->sc_mc->mc_zsirq = zs_level | MC_IRQ_IEN;
			break;
#endif
	}
	initirq = 1;

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
}

void
zs_ttydef(zp)
struct zs *zp;
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
int
zsopen(dev, flag, mode, p)
dev_t dev;
int flag, mode;
struct proc *p;
{
	register struct tty *tp;
	int     error;
	struct zs *zp;
	struct zssoftc *sc;

	if (zsunit(dev) >= zs_cd.cd_ndevs ||
		 (sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(dev)]) == NULL)
		return (ENODEV);

	zp = &sc->sc_zs[zsside(dev)];
	if (zp->tty == NULL) {
		zp->tty = ttymalloc();
		tty_attach(zp->tty);
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
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);

	error = ((*linesw[tp->t_line].l_open) (dev, tp));

	if (error == 0)
		++zp->nzs_open;
	return (error);
}

int
zsclose(dev, flag, mode, p)
dev_t   dev;
int     flag, mode;
struct proc *p;
{
	struct zs *zp;
	struct tty *tp;
	struct zssoftc *sc;
	int s;

	if (zsunit(dev) > zs_cd.cd_ndevs ||
		 (sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(dev)]) == NULL)
		return (ENODEV);
	zp = &sc->sc_zs[zsside(dev)];
	tp = zp->tty;

	(*linesw[tp->t_line].l_close) (tp, flag);
	s = splzs();
	if ((zp->flags & ZS_CONSOLE) == 0 && (tp->t_cflag & HUPCL) != 0)
		ZBIC(&zp->scc, 5, 0x82);		  /* drop DTR, RTS */
	ZBIC(&zp->scc, 3, 1);	/* disable receiver */
	splx(s);
	ttyclose(tp);
	zp->nzs_open = 0;
	return (0);
}

/*ARGSUSED*/
int
zsread(dev, uio, flag)
dev_t dev;
struct uio *uio;
int   flag;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(dev)];
	struct zs *zp = &sc->sc_zs[zsside(dev)];
	struct tty *tp = zp->tty;

	return ((*linesw[tp->t_line].l_read) (tp, uio, flag));
}

/*ARGSUSED*/
int
zswrite(dev, uio, flag)
dev_t   dev;
struct uio *uio;
int     flag;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(dev)];
	struct zs *zp = &sc->sc_zs[zsside(dev)];
	struct tty *tp = zp->tty;

	return ((*linesw[tp->t_line].l_write) (tp, uio, flag));
}

int
zsioctl(dev, cmd, data, flag, p)
dev_t   dev;
caddr_t data;
int     cmd, flag;
struct proc *p;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(dev)];
	struct zs *zp = &sc->sc_zs[zsside(dev)];
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
	return (error);
}

int
zsparam(tp, t)
struct tty *tp;
struct termios *t;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &sc->sc_zs[zsside(tp->t_dev)];
	register int s;

	zs_drain(zp);
	s = splzs();
	zp->imask = zscc_params(&zp->scc, t);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	if ((tp->t_cflag & CCTS_OFLOW) == 0)
		zp->hflags &= ~ZH_OBLOCK;
	else if ((zp->modem_state & 0x20) == 0)
		zp->hflags |= ZH_OBLOCK;
	spltty();
	zs_unblock(tp);
	splx(s);
	return (0);
}

void
zsstart(tp)
struct tty *tp;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &sc->sc_zs[zsside(tp->t_dev)];
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

zsstop(tp, flag)
struct tty *tp;
int flag;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[zsunit(tp->t_dev)];
	struct zs *zp = &sc->sc_zs[zsside(tp->t_dev)];
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
#ifdef MVME162
	if (cputyp == CPU_162)
		ZWRITE(scc, 2, MC_VECBASE+MCV_ZS);
#endif
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
		return (0xFF);
	}
	if ((par->c_cflag & CREAD) == 0)
		ZBIC(scc, 3, 1);/* disable receiver */
	divisor = (zspclk() / 32 + (speed >> 1)) / speed - 2;
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
#if 0
	if (cputyp == CPU_162) {
		ZWRITE(scc, 15, ints | 1);
		/*
		 * now.. register 7 has become register 7': disable all
		 * 82530-only features for now
		 */
		ZWRITE(scc, 7, 0x20);
	}
#endif
	ZWRITE(scc, 15, ints);
	return (imask);
}

zscc_mget(scc)
register struct sccregs *scc;
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
	return (bits);
}

zscc_mset(scc, bits)
register struct sccregs *scc;
int bits;
{
	if ((bits & TIOCM_LE) != 0)
		ZBIS(scc, 3, SCC_RCVEN);
	if ((bits & TIOCM_DTR) != 0)
		ZBIS(scc, 5, SCC_DTR);
	if ((bits & TIOCM_RTS) != 0)
		ZBIS(scc, 5, SCC_RTS);
}

zscc_mclr(scc, bits)
register struct sccregs *scc;
int bits;
{
	if ((bits & TIOCM_LE) != 0)
		ZBIC(scc, 3, SCC_RCVEN);
	if ((bits & TIOCM_DTR) != 0)
		ZBIC(scc, 5, TIOCM_DTR);
	if ((bits & TIOCM_RTS) != 0)
		ZBIC(scc, 5, SCC_RTS);
}

zs_drain(zp)
register struct zs *zp;
{
	register int s;

	zp->tty->t_state |= TS_DRAIN;
	/* wait for Tx buffer empty and All sent bits to be set */
	s = splzs();
	while ((ZREAD0(&zp->scc) & SCC_TXRDY) == 0 ||
			 (ZREAD(&zp->scc, 1) & 1) == 0) {
		splx(s);
		DELAY(100);
		s = splzs();
	}
	splx(s);
}

zs_unblock(tp)
register struct tty *tp;
{
	tp->t_state &= ~TS_DRAIN;
	if (tp->t_outq.c_cc != 0)
		zsstart(tp);
}

/*
 * Hardware interrupt from an SCC.
 */
int
zsirq(unit)
int unit;
{
	struct zssoftc *sc = (struct zssoftc *) zs_cd.cd_devs[unit];
	register struct zs *zp = &sc->sc_zs[0];
	register int ipend, x;

	ipend = ZREAD(&zp->scc, 3);	/* read int pending from A side */
	if (ipend == 0)
		return (0);
	if ((ipend & 0x20) != 0)
		zs_rxint(zp);
	if ((ipend & 0x10) != 0)
		zs_txint(zp);
	if ((ipend & 0x8) != 0)
		zs_extint(zp);
	++zp;				/* now look for B side ints */
	if ((ipend & 0x4) != 0)
		zs_rxint(zp);
	if ((ipend & 0x2) != 0)
		zs_txint(zp);
	if ((ipend & 0x1) != 0)
		zs_extint(zp);
	ZWRITE0(&zp->scc, 0x38);	/* reset highest IUS */

	sc->sc_intrcnt.ev_count++;
	return (1);
}

zs_txint(zp)
register struct zs *zp;
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

zs_rxint(zp)
register struct zs *zp;
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
zs_extint(zp)
register struct zs *zp;
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
	for (unit = 0; unit < zs_cd.cd_ndevs; ++unit) {
		if (zs_cd.cd_devs[unit] == NULL)
			continue;
		zp = &((struct zssoftc *) zs_cd.cd_devs[unit])->sc_zs[0];
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
 * Routines for using side A of the first SCC as a console.
 */

/* probe for the SCC; should check hardware */
zscnprobe(cp)
struct consdev *cp;
{
	int maj;

	switch (cputyp) {
		case CPU_147:
		case CPU_162:
			break;
		default:
			cp->cn_pri = CN_DEAD;
			return (0);
	}

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == zsopen)
			break;

		/* initialize required fields */
	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_INTERNAL;	/* better than PROM console */

	return (1);
}

/* initialize the keyboard for use as the console */
struct termios zscn_termios = {
	TTYDEF_IFLAG,
	TTYDEF_OFLAG,
	(CREAD | CS8 | HUPCL),
	TTYDEF_LFLAG,
	{0},
	TTYDEF_SPEED,
	TTYDEF_SPEED
};

struct sccregs zs_cons_sccregs;
int     zs_cons_imask;

zscninit()
{
	zs_cnsetup(0, &zscn_termios);
}

/* Polling routine for console input from a serial port. */
int
zscngetc(dev)
dev_t dev;
{
	register struct sccregs *scc = zs_cons_scc;
	int     c, s, stat;

	s = splhigh();	/* XXX was splzs() */
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
	return (c);
}

zscnputc(dev, c)
dev_t dev;
int c;
{
	register struct sccregs *scc = zs_cons_scc;
	int     s;

	s = splhigh();	/* XXX was splzs() */
	while ((ZREAD0(scc) & SCC_TXRDY) == 0)
		;
	ZWRITED(scc, c);
	splx(s);
}

zs_cnsetup(unit, tiop)
int unit;
struct termios *tiop;
{
	volatile u_char *scc_cr, *scc_dr;
	struct sccregs *scc;
	int size;

	zs_cons_unit = unit;
	zs_is_console = 1;
	zs_cons_scc = scc = &zs_cons_sccregs;

	/*
	 * the locations of the control and data register move around
	 * on different MVME models, so we generate independent pointers
	 * to them.
	 */
	size = zsregs(NULL, unit, &scc_cr, &scc_dr);

	*(scc_cr + size) = 0;
	*(scc_cr + size) = 9;
	*(scc_cr + size) = 0xC0;	/* hardware reset of SCC, both sides */
	if (!zsside(unit)) {
		scc_cr += size;
		scc_dr += size;
	}

	scc->s_cr = scc_cr;
	scc->s_dr = scc_dr;
#ifdef MVME162
	if (cputyp == CPU_162)
		ZWRITE(scc, 2, MC_VECBASE+MCV_ZS);
#endif
	ZWRITE(scc, 10, 0);
	ZWRITE(scc, 11, 0x50);	/* rx & tx clock = brgen */
	ZWRITE(scc, 14, 3);	/* brgen enabled, from pclk */
	zs_cons_imask = zscc_params(scc, tiop);
	ZBIS(scc, 5, 0x82);	/* set DTR and RTS */

	zs_cons_termios = *tiop;/* save for later */
}

#ifdef MVME147
u_long zs_cons_addrs_147[] = { ZS0_PHYS_147, ZS1_PHYS_147};
#endif
#ifdef MVME162
u_long zs_cons_addrs_162[] = { ZS0_PHYS_162, ZS1_PHYS_162};
#endif

/*
 * fills in pointers to the registers and returns how far apart
 * the two halves of the chip are.
 * 
 * it vaddr == NULL, it tries to determine the hardware address in
 * an intelligent fashion from the unit number.
 */
int
zsregs(va, unit, crp, drp)
void *va;
int unit;
volatile u_char **crp, **drp;
{
#ifdef MVME147
	volatile struct scc_147 *scc_adr_147;
#endif
#ifdef MVME162
	volatile struct scc_162 *scc_adr_162;
#endif
	volatile u_char *scc_cr, *scc_dr;
	int size;

	switch (cputyp) {
#ifdef MVME147
		case CPU_147:
			if (!va)
				va = (void *)IIOV(zs_cons_addrs_147[zsunit(unit)]);
			scc_adr_147 = (volatile struct scc_147 *)va;
			scc_cr = &scc_adr_147->cr;
			scc_dr = &scc_adr_147->dr;
			size = sizeof(struct scc_147);
			break;
#endif
#ifdef MVME162
		case CPU_162:
			if (!va)
				va = (void *)IIOV(zs_cons_addrs_162[zsunit(unit)]);
			scc_adr_162 = (volatile struct scc_162 *)va;
			scc_cr = &scc_adr_162->cr;
			scc_dr = &scc_adr_162->dr;
			size = sizeof(struct scc_162);
			break;
#endif
	}

	*crp = scc_cr;
	*drp = scc_dr;
	return (size);
}

int
zspclk()
{
	switch (cputyp) {
#ifdef MVME147
		case CPU_147:
			return (PCLK_FREQ_147);
#endif
#ifdef MVME162
		case CPU_162:
			return (PCLK_FREQ_162);
#endif
	}
}
