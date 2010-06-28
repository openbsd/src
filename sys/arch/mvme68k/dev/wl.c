/*	$OpenBSD: wl.c,v 1.23 2010/06/28 14:13:29 deraadt Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn. All rights reserved.
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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
#include <sys/syslog.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/wlreg.h>
#include <mvme68k/dev/vme.h>

#include "cl.h"

#include "vmes.h"

#define splcl() spltty()

/* min timeout 0xa, what is a good value */
#define CL_TIMEOUT	0x10
#define CL_FIFO_MAX	0x10
#define CL_FIFO_CNT	0xc
#define	CL_RX_TIMEOUT	0x10

#define CL_RXDMAINT	0x82
#define CL_TXDMAINT	0x42
#define CL_TXMASK	0x47
#define CL_RXMASK	0x87 
#define CL_TXINTR	0x02
#define CL_RXINTR	0x02

#define WLRAMLEN	(1 << 16)
struct clboard {
	union {
		struct clreg	clreg;
		volatile char		xx[256];
	} chips[2];
	union {
		volatile u_char		base;
		volatile char		xx[256];
	} sram;
	union {
		volatile u_char		val;
		volatile char		xx[256];
	} ringstatus;
	union {
		volatile u_char		val;
		volatile char		xx[256];
	} ringreset;
	union {
		volatile u_char		val;
		volatile char		xx[256];
	} master;
	union {
		volatile u_char		val;
		volatile char		xx[256];
	} reset;
};


struct cl_info {
	struct tty *tty;
	u_char	cl_swflags;
	u_char	cl_softchar;
	u_char	cl_speed;
	u_char	cl_parstop;	/* parity, stop bits. */
	u_char	cl_rxmode;
	u_char	cl_txmode;
	u_char	cl_clen;
	u_char	cl_parity;
	u_char  transmitting;
	u_long  txcnt;
	u_long  rxcnt;

	volatile void	*rx[2];
	volatile void	*rxp[2];
	volatile void	*tx[2];
	volatile void	*txp[2];
};
#define CLCD_PORTS_PER_CHIP 4
#define CL_BUFSIZE 128

struct wlsoftc {
	struct device	sc_dev;
	char		sc_txintrname[16 + 3];
	char		sc_rxintrname[16 + 3];
	char		sc_mxintrname[16 + 3];

	time_t		sc_rotime;	/* time of last ring overrun */
	time_t		sc_fotime;	/* time of last fifo overrun */

	u_char		sc_memv;
	paddr_t		sc_memvme;
	void		*sc_memp;
	vaddr_t		sc_memkv;

	struct clreg	*cl_reg;
	struct cl_info	sc_cl[CLCD_PORTS_PER_CHIP];
	struct intrhand	sc_ih_e;
	struct intrhand	sc_ih_m;
	struct intrhand	sc_ih_t;
	struct intrhand	sc_ih_r;
	u_char		sc_vec;
	int		sc_flags;
};

struct {
	u_int speed;
	u_char divisor;
	u_char clock;
	u_char rx_timeout;
} cl_clocks[] = {
	/* 30.000 MHz */
	{   64000, 0x3a, 0, 0x01 },
	{   56000, 0x42, 0, 0x01 },
	{   38400, 0x61, 0, 0x01 },
	{   19200, 0xc2, 0, 0x01 },
	{    9600, 0x61, 1, 0x02 },
	{    7200, 0x81, 1, 0x04 },
	{    4800, 0xc2, 1, 0x04 },
	{    3600, 0x40, 2, 0x08 },
	{    2400, 0x61, 2, 0x10 },
	{    1200, 0xc2, 2, 0x20 },
	{     600, 0x61, 3, 0x40 },
	{     300, 0xc2, 3, 0x80 },
	{     150, 0x61, 4, 0xa0 },
	{     110, 0x84, 4, 0xff },
	{      50, 0x00, 5, 0xff },
	{       0, 0x00, 0, 0},
};

/* prototypes */
u_char cl_clkdiv(int speed);
u_char cl_clknum(int speed);
u_char cl_clkrxtimeout(int speed);
void clstart(struct tty *tp);
void cl_unblock(struct tty *tp);
int clccparam(struct wlsoftc *sc, struct termios *par, int channel);

int clparam(struct tty *tp, struct termios *t);
int cl_intr(struct wlsoftc *sc, int);
int cl_mintr(struct wlsoftc *sc);
int cl_txintr(struct wlsoftc *sc);
int cl_rxintr(struct wlsoftc *sc);
void cl_overflow(struct wlsoftc *sc, int channel, long *ptime, u_char *msg);
void cl_parity(struct wlsoftc *sc, int channel);
void cl_frame(struct wlsoftc *sc, int channel);
void cl_break( struct wlsoftc *sc, int channel);
int clmctl(dev_t dev, int bits, int how);
void cl_dumpport(int channel);

int	wlprobe(struct device *parent, void *self, void *aux);
void	wlattach(struct device *parent, struct device *self, void *aux);

static void cl_initchannel(struct wlsoftc *sc, int channel);
static void clputc(struct wlsoftc *sc, int unit, u_char c);
static u_char clgetc(struct wlsoftc *sc, int *channel);
static void cloutput(struct tty *tp);

struct cfattach wl_ca = {
	sizeof(struct wlsoftc), wlprobe, wlattach
};

struct cfdriver wl_cd = {
	NULL, "wl", DV_TTY
};

#define CLCDBUF 80

#define CL_UNIT(x) (minor(x) >> 2)
#define CL_CHANNEL(x) (minor(x) & 3)
#define CL_TTY(x) (minor(x))

extern int cputyp;

struct tty *
wltty(dev)
	dev_t dev;
{
	int unit = CL_UNIT(dev);
	int channel;
	struct wlsoftc *sc;

	if (unit >= wl_cd.cd_ndevs || 
	    (sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL)
		return (NULL);
	channel = CL_CHANNEL(dev);
	return sc->sc_cl[channel].tty;
}

int
wlprobe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct wlsoftc *sc = self;
	struct clreg *cl_reg = (struct clreg *)ca->ca_vaddr;

	if (ca->ca_vec & 0x03) {
		printf("%s: bad vector\n", sc->sc_dev.dv_xname);
		return (0);
	}
	return (!badvaddr(&cl_reg->cl_gfrcr, 1));
}

void
wlattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct wlsoftc *sc = (struct wlsoftc *)self;
	struct confargs *ca = aux;
	struct clboard *clb = (struct clboard *)ca->ca_vaddr;
	void *p;
	int i, j, s;

	sc->cl_reg = (struct clreg *)&clb->chips[0].clreg;
	sc->sc_vec = ca->ca_vec;

	sc->sc_memv = 0xa5 + 0;
	sc->sc_memvme = ((0xff00 + sc->sc_memv) << 16);

	clb->reset.val = 0xff;		/* reset card */
	DELAY(1000);
	clb->sram.base = (sc->sc_memvme >> 16) & 0xff;
	DELAY(1000);
	clb->master.val = 0x01;		/* enable sram decoder */
	DELAY(1000);

	printf(":");
	/*printf(" va=%x sc=%x slot 0x%02x vmes 0x%08x", sc->cl_reg, sc,
	    sc->sc_memv, sc->sc_memvme);*/

	while (sc->cl_reg->cl_gfrcr == 0x00)
		;
	sc->cl_reg->cl_ccr = 0x10;	/* reset it */
	while (sc->cl_reg->cl_gfrcr == 0x00)
		;
	if (sc->cl_reg->cl_gfrcr <= 0x10)
		printf(" rev %c", 'A' + sc->cl_reg->cl_gfrcr);
	else
		printf(" rev 0x%02x", sc->cl_reg->cl_gfrcr);
	printf("\n");

	/* set up global registers */
	sc->cl_reg->cl_tpr = CL_TIMEOUT;
	sc->cl_reg->cl_rpilr = (ca->ca_ipl << 1) | 1;
	sc->cl_reg->cl_tpilr = (ca->ca_ipl << 1) | 1;
	sc->cl_reg->cl_mpilr = (ca->ca_ipl << 1) | 1;

	sc->sc_memkv = vmemap(((struct vmessoftc *)parent)->sc_vme,
	    sc->sc_memvme, WLRAMLEN, BUS_VMES);
	sc->sc_memp = (void *)kvtop(sc->sc_memkv);
	if (sc->sc_memkv == 0)
		printf("%s: got no memory", sc->sc_dev.dv_xname);
	else if (badvaddr(sc->sc_memkv, 1))
		printf("%s: cannot tap 0x%08x", sc->sc_dev.dv_xname, sc->sc_memkv);
	else {
		u_char *x = (u_char *)sc->sc_memkv;

		/*printf("%s: pa 0x%08x va 0x%08x", sc->sc_dev.dv_xname,
		    sc->sc_memp, sc->sc_memkv);*/
		x[0] = 0xaa;
		x[1] = 0x55;
		if (x[0] != 0xaa || x[1] != 0x55)
			printf(" 0x%02x 0x%02x", x[0], x[1]);
		x[0] = 0x55;
		x[1] = 0xaa;
		if (x[0] != 0x55 || x[1] != 0xaa)
			printf(" 0x%02x 0x%02x", x[0], x[1]);
		bzero(x, WLRAMLEN);
	}

	/* enable interrupts */
	sc->sc_ih_e.ih_fn = cl_intr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;
	sc->sc_ih_e.ih_wantframe = 0;

	sc->sc_ih_m.ih_fn = cl_intr;
	sc->sc_ih_m.ih_arg = sc;
	sc->sc_ih_m.ih_ipl = ca->ca_ipl;
	sc->sc_ih_m.ih_wantframe = 0;

	sc->sc_ih_t.ih_fn = cl_intr;
	sc->sc_ih_t.ih_arg = sc;
	sc->sc_ih_t.ih_ipl = ca->ca_ipl;
	sc->sc_ih_t.ih_wantframe = 0;

	sc->sc_ih_r.ih_fn = cl_intr;
	sc->sc_ih_r.ih_arg = sc;
	sc->sc_ih_r.ih_ipl = ca->ca_ipl;
	sc->sc_ih_r.ih_wantframe = 0;

	snprintf(sc->sc_txintrname, sizeof sc->sc_txintrname,
	    "%s_tx", self->dv_xname);
	snprintf(sc->sc_rxintrname, sizeof sc->sc_rxintrname,
	    "%s_rx", self->dv_xname);
	snprintf(sc->sc_mxintrname, sizeof sc->sc_mxintrname,
	    "%s_mx", self->dv_xname);

	vmeintr_establish(ca->ca_vec + 0, &sc->sc_ih_e, sc->sc_rxintrname);
	vmeintr_establish(ca->ca_vec + 1, &sc->sc_ih_m, sc->sc_mxintrname);
	vmeintr_establish(ca->ca_vec + 2, &sc->sc_ih_t, sc->sc_txintrname);
	vmeintr_establish(ca->ca_vec + 3, &sc->sc_ih_r, sc->sc_rxintrname);

	p = (void *)sc->sc_memkv;
	s = splhigh();
	for (i = 0; i < CLCD_PORTS_PER_CHIP; i++) {
		for (j = 0; j < 2; j++) {
			sc->sc_cl[i].rx[j] = p;
			sc->sc_cl[i].rxp[j] = (void *)(p - (void *)sc->sc_memkv);
			/*printf("%d:%d rx v %x p %x\n",
			    i, j, sc->sc_cl[i].rx[j], sc->sc_cl[i].rxp[j]);*/
			p += CL_BUFSIZE;
		}
		for (j = 0; j < 2; j++) {
			sc->sc_cl[i].tx[j] = p;
			sc->sc_cl[i].txp[j] = (void *)(p - (void *)sc->sc_memkv);
			/*printf("%d:%d tx v %x p %x\n",
			    i, j, sc->sc_cl[i].tx[j], sc->sc_cl[i].txp[j]);*/
			p += CL_BUFSIZE;
		}
		cl_initchannel(sc, i);
	}
	splx(s);
}

static void
cl_initchannel(sc, channel)
	struct wlsoftc *sc;
	int channel;
{
	struct clreg *cl_reg = sc->cl_reg;

	/* set up option registers */
	cl_reg->cl_car	= channel;
	cl_reg->cl_livr	= sc->sc_vec;
	cl_reg->cl_ier	= 0x00;
	cl_reg->cl_cmr	= 0x02;
	cl_reg->cl_cor1	= 0x17;
	cl_reg->cl_cor2	= 0x00;
	cl_reg->cl_cor3	= 0x02;
	cl_reg->cl_cor4	= 0xec;
	cl_reg->cl_cor5	= 0xec;
	cl_reg->cl_cor6	= 0x00;
	cl_reg->cl_cor7	= 0x00;
	cl_reg->cl_schr1 = 0x00;
	cl_reg->cl_schr2 = 0x00;
	cl_reg->cl_schr3 = 0x00;
	cl_reg->cl_schr4 = 0x00;
	cl_reg->cl_scrl	= 0x00;
	cl_reg->cl_scrh	= 0x00;
	cl_reg->cl_lnxt	= 0x00;
	cl_reg->cl_rbpr	= 0x40; /* 9600 */
	cl_reg->cl_rcor	= 0x01;
	cl_reg->cl_tbpr	= 0x40; /* 9600 */
	cl_reg->cl_tcor	= 0x01 << 5;
	/* console port should be 0x88 already */
	cl_reg->cl_msvr_rts	= 0x00;
	cl_reg->cl_msvr_dtr	= 0x00;
	cl_reg->cl_rtprl	= CL_RX_TIMEOUT;
	cl_reg->cl_rtprh	= 0x00;
	sc->cl_reg->cl_ccr = 0x20;
	while (sc->cl_reg->cl_ccr != 0)
		;
}


int cldefaultrate = TTYDEF_SPEED;

int clmctl (dev, bits, how)
	dev_t dev;
	int bits;
	int how;
{
	int s;
	struct wlsoftc *sc;
	/* should only be called with valid device */
	sc = (struct wlsoftc *) wl_cd.cd_devs[CL_UNIT(dev)];
	/*
	printf("mctl: dev %x, bits %x, how %x,\n",dev, bits, how);
	*/
	/* settings are currently ignored */
	s = splcl();
	switch (how) {
	case DMSET:
		if( bits & TIOCM_RTS) {
			sc->cl_reg->cl_msvr_rts = 0x01;
		} else {
			sc->cl_reg->cl_msvr_rts = 0x00;
		}
		if( bits & TIOCM_DTR) {
			sc->cl_reg->cl_msvr_dtr = 0x02;
		} else {
			sc->cl_reg->cl_msvr_dtr = 0x00;
		}
		break;

	case DMBIC:
		if( bits & TIOCM_RTS) {
			sc->cl_reg->cl_msvr_rts = 0x00;
		}
		if( bits & TIOCM_DTR) {
			sc->cl_reg->cl_msvr_dtr = 0x00;
		}
		break;

	case DMBIS:
		if( bits & TIOCM_RTS) {
			sc->cl_reg->cl_msvr_rts = 0x01;
		}
		if( bits & TIOCM_DTR) {
			sc->cl_reg->cl_msvr_dtr = 0x02;
		}
		break;

	case DMGET:
		bits = 0;

		{
			u_char msvr;
			msvr = sc->cl_reg->cl_msvr_rts;
			if( msvr & 0x80) {
				bits |= TIOCM_DSR;
			}
			if( msvr & 0x40) {
				bits |= TIOCM_CD;
			}
			if( msvr & 0x20) {
				bits |= TIOCM_CTS;
			}
			if( msvr & 0x10) {
				bits |= TIOCM_DTR;
			}
			if( msvr & 0x02) {
				bits |= TIOCM_DTR;
			}
			if( msvr & 0x01) {
				bits |= TIOCM_RTS;
			}
			
		}
		break;
	}
	splx(s);
#if 0
	bits = 0;
	/* proper defaults? */
	bits |= TIOCM_DTR;
	bits |= TIOCM_RTS;
	bits |= TIOCM_CTS;
	bits |= TIOCM_CD;
	/*	bits |= TIOCM_RI; */
	bits |= TIOCM_DSR;
#endif

	/*
	printf("retbits %x\n", bits);
	*/
	return(bits);
}

int
wlopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int s, unit, channel;
	struct cl_info *cl;
	struct wlsoftc *sc;
	struct tty *tp;
	
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	s = splcl();
	if (cl->tty) {
		tp = cl->tty;
	} else {
		tp = cl->tty = ttymalloc(0);
	}
	tp->t_oproc = clstart;
	tp->t_param = clparam;
	tp->t_dev = dev;

	if ((tp->t_state & TS_ISOPEN) == 0) {
		tp->t_state |= TS_WOPEN;
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			/*
			 * only when cleared do we reset to defaults.
			 */
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cldefaultrate;

			tp->t_cflag = TTYDEF_CFLAG;
		}
		/*
		 * do these all the time
		 */
		if (cl->cl_swflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (cl->cl_swflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (cl->cl_swflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
		clparam(tp, &tp->t_termios);
		ttsetwater(tp);

		(void)clmctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);
		tp->t_state |= TS_CARR_ON;
		{
			u_char save = sc->cl_reg->cl_car;
			sc->cl_reg->cl_car = channel;
			sc->cl_reg->cl_ier = IER_MDM | IER_RXD;
			sc->cl_reg->cl_car = save;
		}
	} else if (tp->t_state & TS_XCLUDE && suser(p, 0) != 0) {
		splx(s);
		return(EBUSY);
	}

	splx(s);
	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	return((*linesw[tp->t_line].l_open)(dev, tp, p));
}

int clparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int unit, channel;
	struct wlsoftc *sc;
	int s;
	dev_t dev;

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	clccparam(sc, t, channel);
	s = splcl();
	cl_unblock(tp);
	splx(s);
	return 0;
}

void cloutput(tp)
	struct tty *tp;
{
	int cc, s, unit, cnt;
	u_char *tptr;
	int channel;
	struct wlsoftc *sc;
	dev_t dev;
	u_char cl_obuffer[CLCDBUF+1];

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return;
	}
	channel = CL_CHANNEL(dev);

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = splcl();
	cc = tp->t_outq.c_cc;
	while (cc > 0) {
/*XXX*/
		cnt = min(CLCDBUF,cc);
		cnt = q_to_b(&tp->t_outq, cl_obuffer, cnt);
		if (cnt == 0) {
			break;
		}
		for (tptr = cl_obuffer; tptr < &cl_obuffer[cnt]; tptr++) {
			clputc(sc, channel, *tptr);
		}
		cc -= cnt;
	}
	splx(s);
}

int
wlclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct wlsoftc *sc;
	int s;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	(*linesw[tp->t_line].l_close)(tp, flag, p);

	s = splcl();
	
	sc->cl_reg->cl_car = channel;
	if((tp->t_cflag & HUPCL) != 0) {
		sc->cl_reg->cl_msvr_rts = 0x00;
		sc->cl_reg->cl_msvr_dtr = 0x00;
		sc->cl_reg->cl_ccr = 0x05;
		sc->cl_reg->cl_ier = 0x00;
	}

	splx(s);
	ttyclose(tp);

#if 0
	cl->tty = NULL;
#endif
	return (0);
}

int
wlread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct wlsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return ENXIO;
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
int
wlwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct wlsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return ENXIO;
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
int
wlioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct wlsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
		(sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
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
		/* */
		break;

	case TIOCCBRK:
		/* */
		break;

	case TIOCSDTR:
		(void) clmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) clmctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) clmctl(dev, *(int *) data, DMSET);
		break;

	case TIOCMBIS:
		(void) clmctl(dev, *(int *) data, DMBIS);
		break;

	case TIOCMBIC:
		(void) clmctl(dev, *(int *) data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = clmctl(dev, 0, DMGET);
		break;
	case TIOCGFLAGS:
		*(int *)data = cl->cl_swflags;
		break;
	case TIOCSFLAGS:
		error = suser(p, 0); 
		if (error != 0)
			return(EPERM); 

		cl->cl_swflags = *(int *)data;
		cl->cl_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return(ENOTTY);
	}

	return 0;
}
int
wlstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = splcl();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return 0;
}

static u_char 
clgetc(sc, channel)
	struct wlsoftc *sc;
	int *channel;
{
	struct clreg *cl_reg;
	u_char reoir, licr, isrl, fifo_cnt, data;

	cl_reg = sc->cl_reg;
	/* if no receive interrupt pending wait */
	if ((cl_reg->cl_rir & RIR_REN) == 0) {
		return 0;
	}
	/* XXX do we need to suck the entire FIFO contents? */
	licr = cl_reg->cl_licr;
	*channel = (licr >> 2) & 0x3;
	/* is the interrupt for us (port 0) */
	/* the character is for us yea. */
	isrl = cl_reg->cl_risrl;
#if 0
	if (isrl & 0x01) {
		status = BREAK;
	}
	if (isrl & 0x02) {
		status = FRAME;
	}
	if (isrl & 0x04) {
		status = PARITY;
	}
	if (isrl & 0x08) {
		status = OVERFLOW;
	}
	/* we do not have special characters ;-) */
#endif
	fifo_cnt = cl_reg->cl_rfoc;
	if (fifo_cnt > 0) {
		data = cl_reg->cl_rdr;
		cl_reg->cl_teoir = 0x00;
	} else {
		data = 0;
		cl_reg->cl_teoir = 0x08;
	}
	return data;
}

int
clccparam(sc, par, channel)
	struct wlsoftc *sc;
	struct termios *par;
	int channel;
{
	u_int divisor, clk, clen;
	int s, imask, ints;

	s = splcl();
	sc->cl_reg->cl_car = channel;
	if (par->c_ospeed == 0) { 
		/* disconnect, drop RTS DTR stop receiver */
		sc->cl_reg->cl_msvr_rts = 0x00;
		sc->cl_reg->cl_msvr_dtr = 0x00;
		sc->cl_reg->cl_ccr = 0x05;
		splx(s);
		return (0xff);
	}

	sc->cl_reg->cl_msvr_rts = 0x03;
	sc->cl_reg->cl_msvr_dtr = 0x03;

	divisor = cl_clkdiv(par->c_ospeed);
	clk	= cl_clknum(par->c_ospeed);
	sc->cl_reg->cl_tbpr = divisor;
	sc->cl_reg->cl_tcor = clk << 5;
	divisor = cl_clkdiv(par->c_ispeed);
	clk	= cl_clknum(par->c_ispeed);
	sc->cl_reg->cl_rbpr = divisor;
	sc->cl_reg->cl_rcor = clk;
	sc->cl_reg->cl_rtprl = cl_clkrxtimeout(par->c_ispeed);
	sc->cl_reg->cl_rtprh = 0x00;

	switch (par->c_cflag & CSIZE) {
	case CS5:
		clen = 4; /* this is the mask for the chip. */
		imask = 0x1F;
		break;
	case CS6:
		clen = 5;
		imask = 0x3F;
		break;
	case CS7:
		clen = 6;
		imask = 0x7F;
		break;
	default:
		clen = 7;
		imask = 0xFF;
	}
	sc->cl_reg->cl_cor3 = par->c_cflag & PARENB ? 4 : 2;

	{
		u_char cor1;
		if (par->c_cflag & PARENB) {
			if (par->c_cflag & PARODD) {
				cor1 = 0xE0 | clen ; /* odd */
			} else {
				cor1 = 0x40 | clen ; /* even */
			}
		} else {
			cor1 = 0x10 | clen; /* ignore parity */
		}
		if (sc->cl_reg->cl_cor1 != cor1) { 
			sc->cl_reg->cl_cor1 = cor1;
			sc->cl_reg->cl_ccr = 0x20;
			while (sc->cl_reg->cl_ccr != 0)
				;
		}
	}

	if ((par->c_cflag & CREAD) == 0) {
		sc->cl_reg->cl_ccr = 0x08;
	} else {
		sc->cl_reg->cl_ccr = 0x0a;
	}
	while (sc->cl_reg->cl_ccr != 0)
		;

	ints = 0;
#define SCC_DSR 0x80
#define SCC_DCD 0x40
#define SCC_CTS 0x20
	if ((par->c_cflag & CLOCAL) == 0) {
		ints |= SCC_DCD;
	}
	if ((par->c_cflag & CCTS_OFLOW) != 0) {
		ints |= SCC_CTS;
	}
	if ((par->c_cflag & CRTSCTS) != 0) {
		ints |= SCC_CTS;
	}
#ifdef DONT_LET_HARDWARE
	if ((par->c_cflag & CCTS_IFLOW) != 0) {
		ints |= SCC_DSR;
	}
#endif
	sc->cl_reg->cl_cor4 = ints | CL_FIFO_CNT;
	sc->cl_reg->cl_cor5 = ints | CL_FIFO_CNT;

	return imask;
}

static int clknum = 0;

u_char 
cl_clkdiv(speed)
	int speed;
{
	int i = 0;

	if (cl_clocks[clknum].speed == speed)
		return cl_clocks[clknum].divisor;

	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].divisor;
		}
	}

	/* return some sane value if unknown speed */
	clknum = 4;
	return cl_clocks[clknum].divisor;
}
u_char 
cl_clknum(speed)
	int speed;
{
	int found = 0;
	int i = 0;
	if (cl_clocks[clknum].speed == speed) {
		return cl_clocks[clknum].clock;
	}
	for  (i = 0; found != 0 && cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[clknum].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].clock;
		}
	}
	/* return some sane value if unknown speed */
	return cl_clocks[4].clock;
}
u_char 
cl_clkrxtimeout(speed)
	int speed;
{
	int i = 0;
	if (cl_clocks[clknum].speed == speed) {
		return cl_clocks[clknum].rx_timeout;
	}
	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].rx_timeout;
		}
	}
	/* return some sane value if unknown speed */
	return cl_clocks[4].rx_timeout;
}
void
cl_unblock(tp)
	struct tty *tp;
{
	tp->t_state &= ~TS_FLUSH;
	if (tp->t_outq.c_cc != 0)
		clstart(tp);
}
void
clstart(tp)
	struct tty *tp;
{
	dev_t dev;
	u_char cbuf;
	struct wlsoftc *sc;
	int channel, unit, s, cnt;

	dev = tp->t_dev;
	channel = CL_CHANNEL(dev);
	unit = CL_UNIT(dev);
	if (unit >= wl_cd.cd_ndevs || 
	    (sc = (struct wlsoftc *) wl_cd.cd_devs[unit]) == NULL) {
		return;
	}

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = splcl();
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0) {
		tp->t_state |= TS_BUSY;
		sc->cl_reg->cl_car = channel;
		sc->cl_reg->cl_ier = sc->cl_reg->cl_ier | IER_TXMPTY | IER_TXD;
#if 0
		zscnputc(0, 'S');
#endif
	}
	splx(s);
}

int
cl_intr(sc, vec)
	struct wlsoftc *sc;
	int vec;
{
	u_char livr = sc->cl_reg->cl_livr;
	u_char stk = sc->cl_reg->cl_stk;
	int i = 0;

	stk = ((stk & 0x80) >> 6) | (stk & 0x01);
#if 1
	zscnputc(0, '[');
	zscnputc(0, '0' + vec - 0x70);
	zscnputc(0, '0' + livr - 0x70);
	zscnputc(0, '0' + stk);
	zscnputc(0, ']');
#endif

	switch (stk & 3) {
	case 0:
#if 0
		i += cl_rxintr(sc);
		i += cl_mintr(sc);
		i += cl_txintr(sc);
#else
		i += cl_rxintr(sc);
#endif
		break;
	case 1:
		i += cl_mintr(sc);
		break;
	case 2:
		i += cl_txintr(sc);
		break;
	case 3:
		i += cl_rxintr(sc);
		break;
	}
	return (i);
}

int
cl_mintr(sc)
	struct wlsoftc *sc;
{
	u_char mir, misr, msvr;
	int channel;
	struct tty *tp;

	mir = sc->cl_reg->cl_mir;
	if((mir & MIR_MACT) == 0)
		return 0;
	zscnputc(0, 'M');

	misr = sc->cl_reg->cl_misr;
	msvr = sc->cl_reg->cl_msvr_rts;
	channel = mir & MIR_MCM_M;

	printf("stk 0x%x mir 0x%x chan 0x%x\n",
	    sc->cl_reg->cl_stk, mir, channel);

	if (misr & MISR_TIMER1) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 1 unexpected\n",channel);
	}
	if (misr & MISR_TIMER2) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 2 unexpected\n",channel);
	}
	if (misr & MISR_CTSCHG) {
		log(LOG_WARNING, "cl_mintr: channel %x cts %x\n",channel, 
		    ((msvr & 0x20) != 0x0));
	}
	if (misr & MISR_CDCHG) {
		struct tty *tp = sc->sc_cl[channel].tty;
		log(LOG_WARNING, "cl_mintr: channel %x cd %x\n",channel,
		    ((msvr & 0x40) != 0x0));
		ttymodem(tp, ((msvr & 0x40) != 0x0) );
	}
	if (misr & MISR_DSRCHG) {
		log(LOG_WARNING, "cl_mintr: channel %x dsr %x\n",channel,
		    ((msvr & 0x80) != 0x0));
	}
	sc->cl_reg->cl_meoir = 0x00;
	return 1;
}

int
cl_txintr(sc)
	struct wlsoftc *sc;
{
	static int empty = 0;
	u_char tir, cmr, teoir, tisr, tftc;
	int chan;
	struct tty *tp;
	int cnt;
	u_char buffer[CL_FIFO_MAX +1];
	u_char *tptr;
	u_char dmabsts;
	int nbuf, busy, resid;
	void *pbuffer;

	tir = sc->cl_reg->cl_tir;
	if((tir & (TIR_TEN|TIR_TACT)) == 0)
		return 0;
#if 0
	zscnputc(0, 'T');
#endif

	cmr = sc->cl_reg->cl_cmr;
	chan = tir & TIR_TCN_M;
	tisr = sc->cl_reg->cl_tisr;
	tftc = sc->cl_reg->cl_tftc;
	
	printf("stk 0x%x tir 0x%x chan 0x%x cmr 0x%x tisr 0x%x tftc 0x%x\n",
	    sc->cl_reg->cl_stk, tir, chan, cmr, tisr, tftc);

	sc->sc_cl[chan].txcnt++;
	tp = sc->sc_cl[chan].tty;

	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0) {
		sc->cl_reg->cl_ier = sc->cl_reg->cl_ier & ~(IER_TXMPTY|IER_TXD);
		sc->cl_reg->cl_teoir = TEOIR_NOTRANSF;
		return 1;
	}

	switch (cmr & CL_TXMASK) {
	case CL_TXINTR:
		cnt = min((int)tftc, tp->t_outq.c_cc);
		if (cnt != 0) {
			/*printf("s%d ", cnt);*/
			cnt = q_to_b(&tp->t_outq, buffer, cnt);
			/*printf("%d:", cnt);*/
			empty = 0;
			for (tptr = buffer; tptr < &buffer[cnt]; tptr++) {
				/*printf("%c", *tptr);*/
				sc->cl_reg->cl_tdr = *tptr;
			}
			/*printf("\n", cnt);*/
			teoir = 0;
		} else {
			if (empty > 5 && ((empty % 20000 )== 0))
				log(LOG_WARNING, "cl_txintr empty intr %d chan %d\n",
				    empty, chan);
			empty++;
			teoir = TEOIR_NOTRANSF;
			if (tp->t_state & TS_BUSY) {
				tp->t_state &= ~(TS_BUSY | TS_FLUSH);
				if (tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup((caddr_t) &tp->t_outq);
				}
				selwakeup(&tp->t_wsel);
			}
			sc->cl_reg->cl_ier = sc->cl_reg->cl_ier & ~(IER_TXMPTY|IER_TXD);
		}
		break;
	default:
		log(LOG_WARNING, "cl_txintr unknown mode %x\n", cmr);
		/* we probably will go to hell quickly now */
		teoir = TEOIR_NOTRANSF;
		break;
	}
	sc->cl_reg->cl_teoir = teoir;
	return (1);
}

int
cl_rxintr(sc)
	struct wlsoftc *sc;
{
	u_char rir, chan, cmr, risrl;
	u_char buffer[CL_FIFO_MAX +1];
	u_char reoir = REOIR_NOTRANSF, fifocnt, c;
	struct tty *tp;
	int i;
	
	rir = sc->cl_reg->cl_rir;
	if((rir & RIR_RACT) == 0)
		return 0;
#if 0
	zscnputc(0, 'R');
#endif

	cmr = sc->cl_reg->cl_cmr;
	risrl = sc->cl_reg->cl_risrl;
	chan = rir & RIR_RCN_M;

	/*printf("stk 0x%x rir 0x%x chan 0x%x cmr 0x%x risrl 0x%x\n",
	    sc->cl_reg->cl_stk, rir, chan, cmr, risrl);*/

	sc->sc_cl[chan].rxcnt++;

	if (risrl & RISRL_TIMEOUT) {
		reoir = REOIR_NOTRANSF;
	} else if (risrl & RISRL_OE) {
		cl_overflow(sc, chan, &sc->sc_fotime, "fifo");
		reoir = REOIR_NOTRANSF;
	} else if (risrl & RISRL_PE) {
		cl_parity(sc, chan);
		reoir = REOIR_NOTRANSF;
	} else if (risrl & RISRL_FE) {
		cl_frame(sc, chan);
		reoir = REOIR_NOTRANSF;
	} else if (risrl & RISRL_BREAK) {
		cl_break(sc, chan);
		reoir = REOIR_NOTRANSF;
	} else {
		switch (cmr & CL_RXMASK) {
		case CL_RXINTR:
			fifocnt = sc->cl_reg->cl_rfoc;
			tp = sc->sc_cl[chan].tty;
			for (i = 0; i < fifocnt; i++)
				buffer[i] = sc->cl_reg->cl_rdr;
			if (NULL == tp) {
				/*
				 * if the channel is not configured,
				 * dont send characters upstream.
				 * also fix problem with NULL dereference
				 */
				reoir = 0;
				break;
			}
	
			sc->cl_reg->cl_reoir = reoir;
			for (i = 0; i < fifocnt; i++) {
				u_char c = buffer[i];
	
				/* does any restricitions exist on spl for this call */
				(*linesw[tp->t_line].l_rint)(c, tp);
				reoir = 0;
			}
			break;
		default:
			log(LOG_WARNING, "cl_rxintr unknown mode %x\n", cmr);
			/* we probably will go to hell quickly now */
			reoir = REOIR_NOTRANSF;
			break;
		}
	}
	sc->cl_reg->cl_reoir = reoir;
	return (1);
}

void
cl_overflow (sc, channel, ptime, msg)
struct wlsoftc *sc;
int channel;
long *ptime;
u_char *msg;
{
/*
	if (*ptime != time.tv_sec) {
*/
	{
/*
		*ptime = time.tv_sec;
*/
		log(LOG_WARNING, "%s%d[%d]: %s overrun\n", wl_cd.cd_name,
			0 /* fix */, channel, msg);
	}
}

void
cl_parity (sc, channel)
	struct wlsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: parity error\n", wl_cd.cd_name, 0, channel);
}

void
cl_frame (sc, channel)
	struct wlsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: frame error\n", wl_cd.cd_name, 0, channel);
}

void
cl_break (sc, channel)
	struct wlsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: break detected\n", wl_cd.cd_name, 0, channel);
}

void
cl_dumpport0()
{
	cl_dumpport(0);
}

void
cl_dumpport1()
{
	cl_dumpport(1);
}

void
cl_dumpport2()
{
	cl_dumpport(2);
}

void
cl_dumpport3()
{
	cl_dumpport(3);
}

void
cl_dumpport(channel)
	int channel;
{
	u_char	livr, cmr, cor1, cor2, cor3, cor4, cor5, cor6, cor7,
		schr1, schr2, schr3, schr4, scrl, scrh, lnxt,
		rbpr, rcor, tbpr, tcor, rpilr, rir, tpr, ier, ccr,
		dmabsts, arbsts, brbsts, atbsts, btbsts,
		csr, rts, dtr, rtprl, rtprh;
	volatile void * parbadru, *parbadrl,  *parbsts, *parbcnt;
	u_short rcbadru, rcbadrl, arbadru, arbadrl, arbcnt,
		brbadru, brbadrl, brbcnt;
	u_short tcbadru, tcbadrl, atbadru, atbadrl, atbcnt,
		btbadru, btbadrl, btbcnt;
	struct wlsoftc *sc;

	struct clreg *cl_reg;
	int s;

	sc = (struct wlsoftc *) wl_cd.cd_devs[0];
	cl_reg = sc->cl_reg;

	s = splcl();
	cl_reg->cl_car	= (u_char) channel;
	livr = cl_reg->cl_livr;
	cmr = cl_reg->cl_cmr;
	cor1 = cl_reg->cl_cor1;
	cor2 = cl_reg->cl_cor2;
	cor3 = cl_reg->cl_cor3;
	cor4 = cl_reg->cl_cor4;
	cor5 = cl_reg->cl_cor5;
	cor6 = cl_reg->cl_cor6;
	cor7 = cl_reg->cl_cor7;
	schr1 = cl_reg->cl_schr1;
	schr2 = cl_reg->cl_schr2;
	schr3 = cl_reg->cl_schr3;
	schr4 = cl_reg->cl_schr4;
	scrl = cl_reg->cl_scrl;
	scrh = cl_reg->cl_scrh;
	lnxt = cl_reg->cl_lnxt;
	rbpr = cl_reg->cl_rbpr;
	rcor = cl_reg->cl_rcor;
	tbpr = cl_reg->cl_tbpr;
	rpilr = cl_reg->cl_rpilr;
	ier = cl_reg->cl_ier;
	ccr = cl_reg->cl_ccr;
	tcor = cl_reg->cl_tcor;
	csr = cl_reg->cl_csr;
	tpr = cl_reg->cl_tpr;
	rts = cl_reg->cl_msvr_rts;
	dtr = cl_reg->cl_msvr_dtr;
	rtprl = cl_reg->cl_rtprl;
	rtprh = cl_reg->cl_rtprh;
	dmabsts = cl_reg->cl_dmabsts;
	tcbadru = cl_reg->cl_tcbadru;
	tcbadrl = cl_reg->cl_tcbadrl;
	rcbadru = cl_reg->cl_rcbadru;
	rcbadrl = cl_reg->cl_rcbadrl;

	parbadru = &(cl_reg->cl_arbadru);
	parbadrl = &(cl_reg->cl_arbadrl);
	parbcnt  = &(cl_reg->cl_arbcnt);
	parbsts  = &(cl_reg->cl_arbsts);

	arbadru = cl_reg->cl_arbadru;
	arbadrl = cl_reg->cl_arbadrl;
	arbcnt  = cl_reg->cl_arbcnt;
	arbsts  = cl_reg->cl_arbsts;

	brbadru = cl_reg->cl_brbadru;
	brbadrl = cl_reg->cl_brbadrl;
	brbcnt  = cl_reg->cl_brbcnt;
	brbsts  = cl_reg->cl_brbsts;

	atbadru = cl_reg->cl_atbadru;
	atbadrl = cl_reg->cl_atbadrl;
	atbcnt  = cl_reg->cl_atbcnt;
	atbsts  = cl_reg->cl_atbsts;

	btbadru = cl_reg->cl_btbadru;
	btbadrl = cl_reg->cl_btbadrl;
	btbcnt  = cl_reg->cl_btbcnt;
	btbsts  = cl_reg->cl_btbsts;

	splx(s);

	printf("{ port %x livr %x cmr %x\n",
		  channel,livr,   cmr);
	printf("cor1 %x cor2 %x cor3 %x cor4 %x cor5 %x cor6 %x cor7 %x\n",
		cor1,   cor2,   cor3,   cor4,   cor5,   cor6,   cor7);
	printf("schr1 %x schr2 %x schr3 %x schr4 %x\n",
		schr1,   schr2,   schr3,   schr4);
	printf("scrl %x scrh %x lnxt %x\n",
		scrl,   scrh,   lnxt);
	printf("rbpr %x rcor %x tbpr %x tcor %x\n",
		rbpr,   rcor,   tbpr,   tcor);
	printf("rpilr %x rir %x ier %x ccr %x\n",
		rpilr,   rir,   ier,   ccr);
	printf("tpr %x csr %x rts %x dtr %x\n",
		tpr,   csr,   rts,   dtr);
	printf("rtprl %x rtprh %x\n",
		rtprl,   rtprh);
	printf("rxcnt %x txcnt %x\n",
		sc->sc_cl[channel].rxcnt, sc->sc_cl[channel].txcnt);
	printf("dmabsts %x, tcbadru %x, tcbadrl %x, rcbadru %x, rcbadrl %x,\n",
		dmabsts,    tcbadru,    tcbadrl,    rcbadru,    rcbadrl );
	printf("parbadru %x, parbadrl %x, parbcnt %x, parbsts %x\n",
		parbadru,    parbadrl,    parbcnt,    parbsts);
	printf("arbadru %x, arbadrl %x, arbcnt %x, arbsts %x\n",
		arbadru,    arbadrl,    arbcnt,    arbsts);
	printf("brbadru %x, brbadrl %x, brbcnt %x, brbsts %x\n",
		brbadru,    brbadrl,    brbcnt,    brbsts);
	printf("atbadru %x, atbadrl %x, atbcnt %x, atbsts %x\n",
		atbadru,    atbadrl,    atbcnt,    atbsts);
	printf("btbadru %x, btbadrl %x, btbcnt %x, btbsts %x\n",
		btbadru,    btbadrl,    btbcnt,    btbsts);
	printf("}\n");
}

static void
clputc(sc, unit, c)
	struct wlsoftc *sc;
	int unit;
	u_char c;
{
	int s;
	u_char schar;
	u_char oldchannel;
	struct clreg *cl_reg;
	cl_reg = sc->cl_reg;

	s = splhigh();
	oldchannel = cl_reg->cl_car;
	cl_reg->cl_car = unit;
	if (cl_reg->cl_tftc > 0) {
		cl_reg->cl_tdr = c;
	}
	cl_reg->cl_car = oldchannel;
	splx(s);
}
