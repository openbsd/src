/*	$NetBSD: mfc.c,v 1.8.2.1 1995/10/20 11:01:12 chopps Exp $ */

/*
 * Copyright (c) 1994 Michael L. Hitch
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#include <amiga/amiga/cc.h>
#include <amiga/dev/zbusvar.h>

#include <dev/cons.h>

#include "mfcs.h"

#ifndef SEROBUF_SIZE
#define SEROBUF_SIZE	128
#endif
#ifndef SERIBUF_SIZE
#define SERIBUF_SIZE	1024
#endif

#define splser()	spl6()

/*
 * 68581 DUART registers
 */
struct mfc_regs {
	volatile u_char du_mr1a;
#define	du_mr2a		du_mr1a
	u_char pad0;
	volatile u_char du_csra;
#define	du_sra		du_csra
	u_char pad2;
	volatile u_char du_cra;
	u_char pad4;
	volatile u_char du_tba;
#define	du_rba		du_tba
	u_char pad6;
	volatile u_char du_acr;
#define	du_ipcr		du_acr
	u_char pad8;
	volatile u_char du_imr;
#define	du_isr		du_imr
	u_char pad10;
	volatile u_char du_ctur;
#define	du_cmsb		du_ctur
	u_char pad12;
	volatile u_char du_ctlr;
#define	du_clsb		du_ctlr
	u_char pad14;
	volatile u_char du_mr1b;
#define	du_mr2b		du_mr1b
	u_char pad16;
	volatile u_char du_csrb;
#define	du_srb		du_csrb
	u_char pad18;
	volatile u_char du_crb;
	u_char pad20;
	volatile u_char du_tbb;
#define	du_rbb		du_tbb
	u_char pad22;
	volatile u_char du_ivr;
	u_char pad24;
	volatile u_char du_opcr;
#define	du_ip		du_opcr
	u_char pad26;
	volatile u_char du_btst;
#define	du_strc		du_btst
	u_char pad28;
	volatile u_char du_btrst;
#define	du_stpc		du_btrst
	u_char pad30;
};

/*
 * 68681 DUART serial port registers
 */
struct duart_regs {
	volatile u_char ch_mr1;
#define	ch_mr2		ch_mr1
	u_char pad0;
	volatile u_char	ch_csr;
#define	ch_sr		ch_csr
	u_char pad1;
	volatile u_char	ch_cr;
	u_char pad2;
	volatile u_char	ch_tb;
#define	ch_rb		ch_tb
	u_char pad3;
};

struct mfc_softc {
	struct	device sc_dev;
	struct	isr sc_isr;
	struct	mfc_regs *sc_regs;
	u_long	clk_frq;
	u_short	ct_val;
	u_char	ct_usecnt;
	u_char	imask;
	u_char	mfc_iii;
	u_char	last_ip;
};

#if NMFCS > 0
struct mfcs_softc {
	struct	device sc_dev;
	struct	tty *sc_tty;
	struct	duart_regs *sc_duart;
	struct	mfc_regs *sc_regs;
	struct	mfc_softc *sc_mfc;
	int	swflags;
	long	flags;			/* XXX */
#define CT_USED	1			/* CT in use */
	u_short	*rptr, *wptr, incnt, ovfl;
	u_short	inbuf[SERIBUF_SIZE];
	char	*ptr, *end;
	char	outbuf[SEROBUF_SIZE];
	struct vbl_node vbl_node;
};
#endif

#if NMFCP > 0
struct mfcp_softc {
};
#endif

struct mfc_args {
	struct zbus_args zargs;
	char	*subdev;
	char	unit;
};

int mfcprint __P((void *auxp, char *));
void mfcattach __P((struct device *, struct device *, void *));
int mfcmatch __P((struct device *, struct cfdata *, void *));
#if NMFCS > 0
void mfcsattach __P((struct device *, struct device *, void *));
int mfcsmatch __P((struct device *, struct cfdata *, void *));
#endif
#if NMFCP > 0
void mfcpattach __P((struct device *, struct device *, void *));
int mfcpmatch __P((struct device *, struct cfdata *, void *));
#endif
int mfcintr __P((struct mfc_softc *));
void mfcsmint __P((register int unit));

struct cfdriver mfccd = {
	NULL, "mfc", (cfmatch_t) mfcmatch, mfcattach,
	DV_DULL, sizeof(struct mfc_softc), NULL, 0 };

#if NMFCS > 0
struct cfdriver mfcscd = {
	NULL, "mfcs", (cfmatch_t) mfcsmatch, mfcsattach,
	DV_TTY, sizeof(struct mfcs_softc), NULL, 0 };
#endif

#if NMFCP > 0
struct cfdriver mfcpcd = {
	NULL, "mfcp", (cfmatch_t) mfcpmatch, mfcpattach,
	DV_DULL, sizeof(struct mfcp_softc), NULL, 0 };
#endif

int	mfcsstart(), mfcsparam(), mfcshwiflow();
int	mfcs_active;
int	mfcsdefaultrate = 38400 /*TTYDEF_SPEED*/;
#define SWFLAGS(dev) (sc->swflags | (((dev) & 0x80) == 0 ? TIOCFLAG_SOFTCAR : 0))

#ifdef notyet
/*
 * MultiFaceCard III, II+ (not supported yet), and
 * SerialMaster 500+ (not supported yet)
 * baud rate tables for BRG set 1 [not used yet]
 */

struct speedtab mfcs3speedtab1[] = {
	0,	0,
	100,	0x00,
	220,	0x11,
	600,	0x44,
	1200,	0x55,
	2400,	0x66,
	4800,	0x88,
	9600,	0x99,
	19200,	0xbb,
	115200,	0xcc,
	-1,	-1
};

/*
 * MultiFaceCard II, I, and SerialMaster 500
 * baud rate tables for BRG set 1 [not used yet]
 */

struct speedtab mfcs2speedtab1[] = {
	0,	0,
	50,	0x00,
	110,	0x11,
	300,	0x44,
	600,	0x55,
	1200,	0x66,
	2400,	0x88,
 	4800,	0x99,
	9600,	0xbb,
	38400,	0xcc,
	-1,	-1
};
#endif

/*
 * MultiFaceCard III, II+ (not supported yet), and
 * SerialMaster 500+ (not supported yet)
 * baud rate tables for BRG set 2
 */

struct speedtab mfcs3speedtab2[] = {
	0,	0,
	150,	0x00,
	200,	0x11,
	300,	0x33,
	600,	0x44,
	1200,	0x55,
	2400,	0x66,
	4800,	0x88,
	9600,	0x99,
	19200,	0xbb,
	38400,	0xcc,
	-1,	-1
};

/*
 * MultiFaceCard II, I, and SerialMaster 500
 * baud rate tables for BRG set 2
 */

struct speedtab mfcs2speedtab2[] = {
	0,	0,
	75,	0x00,
	100,	0x11,
	150,	0x33,
	300,	0x44,
	600,	0x55,
	1200,	0x66,
	2400,	0x88,
 	4800,	0x99,
	9600,	0xbb,
	19200,	0xcc,
	-1,	-1
};

/*
 * if we are an bsc/Alf Data MultFaceCard (I, II, and III)
 */
int
mfcmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct zbus_args *zap;

	zap = auxp;
	if (zap->manid == 2092 &&
	    (zap->prodid == 16 || zap->prodid == 17 || zap->prodid == 18))

		return(1);
	return(0);
}

void
mfcattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct mfc_softc *scc;
	struct zbus_args *zap;
	struct mfc_args ma;
	int unit;
	struct mfc_regs *rp;

	zap = auxp;

	printf ("\n");

	scc = (struct mfc_softc *)dp;
	unit = scc->sc_dev.dv_unit;
	scc->sc_regs = rp = zap->va;
	if (zap->prodid == 18)
		scc->mfc_iii = 3;
	scc->clk_frq = scc->mfc_iii ? 230400 : 115200;

	rp->du_opcr = 0x00;		/* configure output port? */
	rp->du_btrst = 0x0f;		/* clear modem lines */
	rp->du_ivr = 0;			/* IVR */
	rp->du_imr = 0;			/* IMR */
	rp->du_acr = 0xe0;		/* baud rate generate set 2 */
	rp->du_ctur = 0;
	rp->du_ctlr = 4;
	rp->du_csra = 0xcc;		/* clock select = 38400 */
	rp->du_cra = 0x10;		/* reset mode register ptr */
	rp->du_cra = 0x20;
	rp->du_cra = 0x30;
	rp->du_cra = 0x40;
	rp->du_mr1a = 0x93;		/* MRA1 */
	rp->du_mr2a = 0x17;		/* MRA2 */
	rp->du_csrb = 0xcc;		/* clock select = 38400 */
	rp->du_crb = 0x10;		/* reset mode register ptr */
	rp->du_crb = 0x20;
	rp->du_crb = 0x30;
	rp->du_crb = 0x40;
	rp->du_mr1b = 0x93;		/* MRB1 */
	rp->du_mr2b = 0x17;		/* MRB2 */
	rp->du_cra = 0x05;		/* enable A Rx & Tx */
	rp->du_crb = 0x05;		/* enable B Rx & Tx */

	scc->sc_isr.isr_intr = mfcintr;
	scc->sc_isr.isr_arg = scc;
	scc->sc_isr.isr_ipl = 6;
	add_isr(&scc->sc_isr);

	/* configure ports */
	bcopy(zap, &ma.zargs, sizeof(struct zbus_args));
	ma.subdev = "mfcs";
	ma.unit = unit * 2;
	config_found(dp, &ma, mfcprint);
	ma.unit = unit * 2 + 1;
	config_found(dp, &ma, mfcprint);
	ma.subdev = "mfcp";
	ma.unit = unit;
	config_found(dp, &ma, mfcprint);
}

/*
 *
 */
int
mfcsmatch(pdp, cdp, auxp)
	struct device *pdp;
	struct cfdata *cdp;
	void *auxp;
{
	struct mfc_args *ma;

	ma = auxp;
	if (strcmp(ma->subdev, "mfcs") == 0)
		return (1);
	return (0);
}

void
mfcsattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	int unit;
	struct mfcs_softc *sc;
	struct mfc_softc *scc;
	struct mfc_args *ma;
	struct mfc_regs *rp;

	sc = (struct mfcs_softc *) dp;
	scc = (struct mfc_softc *) pdp;
	ma = auxp;

	if (dp) {
		printf (": input fifo %d output fifo %d\n", SERIBUF_SIZE,
		    SEROBUF_SIZE);
		alloc_sicallback();
	}

	unit = ma->unit;
	mfcs_active |= 1 << unit;
	sc->rptr = sc->wptr = sc->inbuf;
	sc->sc_mfc = scc;
	sc->sc_regs = rp = scc->sc_regs;
	sc->sc_duart = (struct duart_regs *) ((unit & 1) ? &rp->du_mr1b :
	    &rp->du_mr1a);
	/*
	 * should have only one vbl routine to handle all ports?
	 */
	sc->vbl_node.function = (void (*) (void *)) mfcsmint;
	sc->vbl_node.data = (void *) unit;
	add_vbl_function(&sc->vbl_node, 1, (void *) unit);
}

/*
 * print diag if pnp is NULL else just extra
 */
int
mfcprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(UNCONF);
	return(QUIET);
}

int
mfcsopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	struct mfcs_softc *sc;
	int unit, error, s;

	error = 0;
	unit = dev & 0x1f;

	if (unit >= mfcscd.cd_ndevs || (mfcs_active & (1 << unit)) == 0)
		return (ENXIO);
	sc = mfcscd.cd_devs[unit];

	s = spltty();

	if (sc->sc_tty)
		tp = sc->sc_tty;
	else
		tp = sc->sc_tty = ttymalloc();

	tp->t_oproc = (void (*) (struct tty *)) mfcsstart;
	tp->t_param = mfcsparam;
	tp->t_dev = dev;
	tp->t_hwiflow = mfcshwiflow;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			/*
			 * only when cleared do we reset to defaults.
			 */
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = mfcsdefaultrate;
		}
		/*
		 * do these all the time
		 */
		if (sc->swflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (sc->swflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (sc->swflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
		mfcsparam(tp, &tp->t_termios);
		ttsetwater(tp);

		(void)mfcsmctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);
		if ((SWFLAGS(dev) & TIOCFLAG_SOFTCAR) ||
		    (mfcsmctl(dev, 0, DMGET) & TIOCM_CD))
			tp->t_state |= TS_CARR_ON;
		else
			tp->t_state &= ~TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return(EBUSY);
	}

	/*
	 * if NONBLOCK requested, ignore carrier
	 */
	if (flag & O_NONBLOCK)
		goto done;

	/*
	 * block waiting for carrier
	 */
	while ((tp->t_state & TS_CARR_ON) == 0 && (tp->t_cflag & CLOCAL) == 0) {
		tp->t_state |= TS_WOPEN;
		error = ttysleep(tp, (caddr_t)&tp->t_rawq,
		    TTIPRI | PCATCH, ttopen, 0);
		if (error) {
			splx(s);
			return(error);
		}
	}
done:
	/* This is a way to handle lost XON characters */
	if ((flag & O_TRUNC) && (tp->t_state & TS_TTSTOP)) {
		tp->t_state &= ~TS_TTSTOP;
	        ttstart (tp);
	}

	splx(s);
	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	return((*linesw[tp->t_line].l_open)(dev, tp));
}

/*ARGSUSED*/
int
mfcsclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct tty *tp;
	int unit;
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];
	struct mfc_softc *scc= sc->sc_mfc;

	unit = dev & 31;

	tp = sc->sc_tty;
	(*linesw[tp->t_line].l_close)(tp, flag);
	sc->sc_duart->ch_cr = 0x70;			/* stop break */

	scc->imask &= ~(0x7 << ((unit & 1) * 4));
	scc->sc_regs->du_imr = scc->imask;
	if (sc->flags & CT_USED) {
		--scc->ct_usecnt;
		sc->flags &= ~CT_USED;
	}

	/*
	 * If the device is closed, it's close, no matter whether we deal with
	 * modem control signals nor not.
	 */
#if 0
	if (tp->t_cflag & HUPCL || tp->t_state & TS_WOPEN ||
	    (tp->t_state & TS_ISOPEN) == 0)
#endif
		(void) mfcsmctl(dev, 0, DMSET);
	ttyclose(tp);
#if not_yet
	if (tp != &mfcs_cons) {
		remove_vbl_function(&sc->vbl_node);
		ttyfree(tp);
		sc->sc_tty = (struct tty *) NULL;
	}
#endif
	return (0);
}

int
mfcsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];
	struct tty *tp = sc->sc_tty;
	if (tp == NULL)
		return(ENXIO);
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
mfcswrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];
	struct tty *tp = sc->sc_tty;

	if (tp == NULL)
		return(ENXIO);
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

struct tty *
mfcstty(dev)
	dev_t dev;
{
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];

	return (sc->sc_tty);
}

int
mfcsioctl(dev, cmd, data, flag, p)
	dev_t	dev;
	caddr_t data;
	struct proc *p;
{
	register struct tty *tp;
	register int unit = dev & 31;
	register int error;
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];

	tp = sc->sc_tty;
	if (!tp)
		return ENXIO;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return(error);

	switch (cmd) {
	case TIOCSBRK:
		sc->sc_duart->ch_cr = 0x60;		/* start break */
		break;

	case TIOCCBRK:
		sc->sc_duart->ch_cr = 0x70;		/* stop break */
		break;

	case TIOCSDTR:
		(void) mfcsmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) mfcsmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) mfcsmctl(dev, *(int *) data, DMSET);
		break;

	case TIOCMBIS:
		(void) mfcsmctl(dev, *(int *) data, DMBIS);
		break;

	case TIOCMBIC:
		(void) mfcsmctl(dev, *(int *) data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = mfcsmctl(dev, 0, DMGET);
		break;
	case TIOCGFLAGS:
		*(int *)data = SWFLAGS(dev);
		break;
	case TIOCSFLAGS:
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0)
			return(EPERM);

		sc->swflags = *(int *)data;
                sc->swflags &= /* only allow valid flags */
                  (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		/* XXXX need to change duart parameters? */
		break;
	default:
		return(ENOTTY);
	}

	return(0);
}

int
mfcsparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int cfcr, cflag, unit, ospeed;
	struct mfcs_softc *sc = mfcscd.cd_devs[tp->t_dev & 31];
	struct mfc_softc *scc= sc->sc_mfc;

	cflag = t->c_cflag;
	unit = tp->t_dev & 31;
	if (sc->flags & CT_USED) {
		--scc->ct_usecnt;
		sc->flags &= ~CT_USED;
	}
	ospeed = ttspeedtab(t->c_ospeed, scc->mfc_iii ? mfcs3speedtab2 :
	    mfcs2speedtab2);

	/*
	 * If Baud Rate Generator can't generate requested speed,
	 * try to use the counter/timer.
	 */
	if (ospeed < 0 && (scc->clk_frq % t->c_ospeed) == 0) {
		ospeed = scc->clk_frq / t->c_ospeed;	/* divisor */
		if (scc->ct_usecnt > 0 && scc->ct_val != ospeed)
			ospeed = -1;
		else {
			scc->sc_regs->du_ctur = ospeed >> 8;
			scc->sc_regs->du_ctlr = ospeed;
			scc->ct_val = ospeed;
			++scc->ct_usecnt;
			sc->flags |= CT_USED;
			ospeed = 0xdd;
		}
	}
	/* XXXX 68681 duart could handle split speeds */
	if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
		return(EINVAL);

	/* XXXX handle parity, character size, stop bits, flow control */

	/*
	 * copy to tty
	 */
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = cflag;

	/*
	 * enable interrupts
	 */
	scc->imask |= (0x2 << ((unit & 1) * 4)) | 0x80;
	scc->sc_regs->du_imr = scc->imask;
#if defined(DEBUG) && 0
	printf("mfcsparam: speed %d => %x ct %d imask %x cflag %x\n",
	    t->c_ospeed, ospeed, scc->ct_val, scc->imask, cflag);
#endif
	if (ospeed == 0)
		(void)mfcsmctl(tp->t_dev, 0, DMSET);	/* hang up line */
	else {
		/*
		 * (re)enable DTR
		 * and set baud rate. (8 bit mode)
		 */
		(void)mfcsmctl(tp->t_dev, TIOCM_DTR | TIOCM_RTS, DMSET);
		sc->sc_duart->ch_csr = ospeed;
	}
	return(0);
}

int mfcshwiflow(tp, flag)
        struct tty *tp;
        int flag;
{
	struct mfcs_softc *sc = mfcscd.cd_devs[tp->t_dev & 31];
	int unit = tp->t_dev & 1;

        if (flag)
		sc->sc_regs->du_btrst = 1 << unit;
	else
		sc->sc_regs->du_btst = 1 << unit;
        return 1;
}

int
mfcsstart(tp)
	struct tty *tp;
{
	int cc, s, unit;
	struct mfcs_softc *sc = mfcscd.cd_devs[tp->t_dev & 31];
	struct mfc_softc *scc= sc->sc_mfc;

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	unit = tp->t_dev & 1;

	s = splser();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))
		goto out;

	cc = tp->t_outq.c_cc;
	if (cc <= tp->t_lowat) {
		if (tp->t_state & TS_ASLEEP) {
			tp->t_state &= ~TS_ASLEEP;
			wakeup((caddr_t) & tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
	}
	if (cc == 0 || (tp->t_state & TS_BUSY))
		goto out;

	/*
	 * We only do bulk transfers if using CTSRTS flow control, not for
	 * (probably sloooow) ixon/ixoff devices.
	 */
	if ((tp->t_cflag & CRTSCTS) == 0)
		cc = 1;

	/*
	 * Limit the amount of output we do in one burst
	 * to prevent hogging the CPU.
	 */
	if (cc > SEROBUF_SIZE)
		cc = SEROBUF_SIZE;
	cc = q_to_b(&tp->t_outq, sc->outbuf, cc);
	if (cc > 0) {
		tp->t_state |= TS_BUSY;

		sc->ptr = sc->outbuf;
		sc->end = sc->outbuf + cc;

		/*
		 * Get first character out, then have TBE-interrupts blow out
		 * further characters, until buffer is empty, and TS_BUSY gets
		 * cleared.
		 */
		sc->sc_duart->ch_tb = *sc->ptr++;
		scc->imask |= 1 << (unit * 4);
		sc->sc_regs->du_imr = scc->imask;
	}
out:
	splx(s);
}

/*
 * Stop output on a line.
 */
/*ARGSUSED*/
int
mfcsstop(tp, flag)
	struct tty *tp;
{
	int s;

	s = splser();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}

int
mfcsmctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	int unit, s;
	u_char ub;
	struct mfcs_softc *sc = mfcscd.cd_devs[dev & 31];

	unit = dev & 1;

	/*
	 * convert TIOCM* mask into CIA mask
	 * which is active low
	 */
	if (how != DMGET) {
		ub = 0;
		/*
		 * need to save current state of DTR & RTS ?
		 */
		if (bits & TIOCM_DTR)
			ub |= 0x04 << unit;
		if (bits & TIOCM_RTS)
			ub |= 0x01 << unit;
	}
	s = splser();
	switch (how) {
	case DMSET:
		sc->sc_regs->du_btst = ub;
		sc->sc_regs->du_btrst = ub ^ (0x05 << unit);
		break;

	case DMBIC:
		sc->sc_regs->du_btrst = ub;
		ub = ~sc->sc_regs->du_ip;
		break;

	case DMBIS:
		sc->sc_regs->du_btst = ub;
		ub = ~sc->sc_regs->du_ip;
		break;

	case DMGET:
		ub = ~sc->sc_regs->du_ip;
		break;
	}
	(void)splx(s);

	/* XXXX should keep DTR & RTS states in softc? */
	bits = TIOCM_DTR | TIOCM_RTS;
	if (ub & (1 << unit))
		bits |= TIOCM_CTS;
	if (ub & (4 << unit))
		bits |= TIOCM_DSR;
	if (ub & (0x10 << unit))
		bits |= TIOCM_CD;
	/* XXXX RI is not supported on all boards */
	if (sc->sc_regs->pad26 & (1 << unit))
		bits |= TIOCM_RI;

	return(bits);
}

/*
 * Level 6 interrupt processing for the MultiFaceCard 68681 DUART
 */

int
mfcintr (scc)
	struct mfc_softc *scc;
{
	struct mfcs_softc *sc;
	struct mfc_regs *regs;
	struct tty *tp;
	int istat, unit;
	u_short c;

	regs = scc->sc_regs;
	istat = regs->du_isr & scc->imask;
	if (istat == 0)
		return (0);
	unit = scc->sc_dev.dv_unit * 2;
	if (istat & 0x02) {		/* channel A receive interrupt */
		sc = mfcscd.cd_devs[unit];
		while (1) {
			c = regs->du_sra << 8;
			if ((c & 0x0100) == 0)
				break;
			c |= regs->du_rba;
			if (sc->incnt == SERIBUF_SIZE)
				++sc->ovfl;
			else {
				*sc->wptr++ = c;
				if (sc->wptr == sc->inbuf + SERIBUF_SIZE)
					sc->wptr = sc->inbuf;
				++sc->incnt;
				if (sc->incnt > SERIBUF_SIZE - 16)
					regs->du_btrst = 1;
			}
			if (c & 0x1000)
				regs->du_cra = 0x40;
		}
	}
	if (istat & 0x20) {		/* channel B receive interrupt */
		sc = mfcscd.cd_devs[unit + 1];
		while (1) {
			c = regs->du_srb << 8;
			if ((c & 0x0100) == 0)
				break;
			c |= regs->du_rbb;
			if (sc->incnt == SERIBUF_SIZE)
				++sc->ovfl;
			else {
				*sc->wptr++ = c;
				if (sc->wptr == sc->inbuf + SERIBUF_SIZE)
					sc->wptr = sc->inbuf;
				++sc->incnt;
				if (sc->incnt > SERIBUF_SIZE - 16)
					regs->du_btrst = 2;
			}
			if (c & 0x1000)
				regs->du_crb = 0x40;
		}
	}
	if (istat & 0x01) {		/* channel A transmit interrupt */
		sc = mfcscd.cd_devs[unit];
		tp = sc->sc_tty;
		if (sc->ptr == sc->end) {
			tp->t_state &= ~(TS_BUSY | TS_FLUSH);
			scc->imask &= ~0x01;
			regs->du_imr = scc->imask;
			add_sicallback (tp->t_line ?
			    (sifunc_t)linesw[tp->t_line].l_start
			    : (sifunc_t)mfcsstart, tp, NULL);

		}
		else
			regs->du_tba = *sc->ptr++;
	}
	if (istat & 0x10) {		/* channel B transmit interrupt */
		sc = mfcscd.cd_devs[unit + 1];
		tp = sc->sc_tty;
		if (sc->ptr == sc->end) {
			tp->t_state &= ~(TS_BUSY | TS_FLUSH);
			scc->imask &= ~0x10;
			regs->du_imr = scc->imask;
			add_sicallback (tp->t_line ?
			    (sifunc_t)linesw[tp->t_line].l_start
			    : (sifunc_t)mfcsstart, tp, NULL);
		}
		else
			regs->du_tbb = *sc->ptr++;
	}
	if (istat & 0x80) {		/* input port change interrupt */
		c = regs->du_ipcr;
		printf ("%s: ipcr %02x", scc->sc_dev.dv_xname, c);
	}
	return(1);
}

int
mfcsxintr(unit)
	int unit;
{
	int s1, s2, ovfl;
	struct mfcs_softc *sc = mfcscd.cd_devs[unit];
	struct tty *tp = sc->sc_tty;

	/*
	 * Make sure we're not interrupted by another
	 * vbl, but allow level6 ints
	 */
	s1 = spltty();

	/*
	 * pass along any acumulated information
	 * while input is not blocked
	 */
	while (sc->incnt && (tp->t_state & TS_TBLOCK) == 0) {
		/*
		 * no collision with ser_fastint()
		 */
		mfcseint(unit, *sc->rptr++);

		ovfl = 0;
		/* lock against mfcs_fastint() */
		s2 = splser();
		--sc->incnt;
		if (sc->rptr == sc->inbuf + SERIBUF_SIZE)
			sc->rptr = sc->inbuf;
		if (sc->ovfl != 0) {
			ovfl = sc->ovfl;
			sc->ovfl = 0;
		}
		splx(s2);
		if (ovfl != 0)
			log(LOG_WARNING, "%s: %d buffer overflow!\n",
			    sc->sc_dev.dv_xname, ovfl);
	}
	if (sc->incnt == 0 && (tp->t_state & TS_TBLOCK) == 0) {
		sc->sc_regs->du_btst = 1 << unit;	/* XXXX */
	}
	splx(s1);
}

int
mfcseint(unit, stat)
	int unit, stat;
{
	struct mfcs_softc *sc = mfcscd.cd_devs[unit];
	struct tty *tp;
	u_char ch;
	int c;

	tp = sc->sc_tty;
	ch = stat & 0xff;
	c = ch;

	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		/* we don't care about parity errors */
		if (kgdb_dev == makedev(sermajor, unit) && c == FRAME_END)
			kgdb_connect(0);	/* trap into kgdb */
#endif
		return;
	}

	/*
	 * Check for break and (if enabled) parity error.
	 */
	if (stat & 0xc000)
		c |= TTY_FE;
	else if (stat & 0x2000)
			c |= TTY_PE;

	if (stat & 0x1000)
		log(LOG_WARNING, "%s: fifo overflow\n",
		    ((struct mfcs_softc *)mfcscd.cd_devs[unit])->sc_dev.dv_xname);

	(*linesw[tp->t_line].l_rint)(c, tp);
}

/*
 * This interrupt is periodically invoked in the vertical blank
 * interrupt.  It's used to keep track of the modem control lines
 * and (new with the fast_int code) to move accumulated data
 * up into the tty layer.
 */
void
mfcsmint(unit)
	int unit;
{
	struct tty *tp;
	struct mfcs_softc *sc = mfcscd.cd_devs[unit];
	u_char stat, last, istat;

	tp = sc->sc_tty;
	if (!tp)
		return;

	if ((tp->t_state & (TS_ISOPEN | TS_WOPEN)) == 0) {
		sc->rptr = sc->wptr = sc->inbuf;
		sc->incnt = 0;
		return;
	}
	/*
	 * empty buffer
	 */
	mfcsxintr(unit);

	stat = ~sc->sc_regs->du_ip;
	last = sc->sc_mfc->last_ip;
	sc->sc_mfc->last_ip = stat;

	/*
	 * check whether any interesting signal changed state
	 */
	istat = stat ^ last;

	if ((istat & (0x10 << (unit & 1))) && 		/* CD changed */
	    (SWFLAGS(tp->t_dev) & TIOCFLAG_SOFTCAR) == 0) {
		if (stat & (0x10 << (unit & 1)))
			(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0) {
			sc->sc_regs->du_btrst = 0x0a << (unit & 1);
		}
	}
}
