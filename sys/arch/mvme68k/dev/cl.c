/*	$NetBSD$	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *   This product includes software developed by Dale Rahn.
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

#include <sys/param.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/device.h>
/* #include <sys/queue.h> */
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>
#include <mvme68k/dev/cd2400reg.h>
#include <sys/syslog.h>
#include "cl.h"

#include "pcctwo.h"

#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#endif

/* min timeout 0xa, what is a good value */
#define CL_TIMEOUT	0x10
#define CL_FIFO_MAX	0x10
#define CL_FIFO_CNT	0xc
#define	CL_RX_TIMEOUT	0x10

#define CL_DMAMODE	0x1
#define CL_INTRMODE	0x0

struct cl_cons {
	u_char *cl_paddr;
	volatile u_char *cl_vaddr;
	volatile struct pcctworeg *pcctwoaddr;
	u_char	channel;
} cl_cons;

struct cl_info {
	struct tty *tty;
	u_char	cl_swflags;
	u_char	cl_softchar;
	u_char	cl_consio;
	u_char	cl_speed;
	u_char	cl_parstop;	/* parity, stop bits. */
	u_char	cl_rxmode;
	u_char	cl_txmode;
	u_char	cl_clen;
	u_char	cl_parity;
	u_char  transmitting;
	u_long  txcnt;
	u_long  rxcnt;
};

#define CLCD_PORTS_PER_CHIP 4
struct clsoftc {
	struct device	sc_dev;
	struct evcnt sc_txintrcnt;
	struct evcnt sc_rxintrcnt;
	struct evcnt sc_mxintrcnt;
	time_t	sc_rotime;	/* time of last ring overrun */
	time_t	sc_fotime;	/* time of last fifo overrun */
	volatile u_char *vbase;
	struct cl_info		sc_cl[CLCD_PORTS_PER_CHIP];
	struct intrhand		sc_ih_e;
	struct intrhand		sc_ih_m;
	struct intrhand		sc_ih_t;
	struct intrhand		sc_ih_r;
	struct pcctworeg	*sc_pcctwo;
	int			sc_flags;
};

struct {
	u_int speed;
	u_char divisor;
	u_char clock;
	u_char rx_timeout;
} cl_clocks[] = {
	{ 64000, 0x26, 0, 0x01},
	{ 56000, 0x2c, 0, 0x01},
	{ 38400, 0x40, 0, 0x01},
	{ 19200, 0x81, 0, 0x02},
	{  9600, 0x40, 1, 0x04},
	{  7200, 0x56, 1, 0x04},
	{  4800, 0x81, 1, 0x08},
	{  3600, 0xad, 1, 0x08},
	{  2400, 0x40, 2, 0x10},
	{  1200, 0x81, 2, 0x20},
	{   600, 0x40, 3, 0x40},
	{   300, 0x81, 3, 0x80},
	{   150, 0x40, 3, 0x80},
	{   110, 0x58, 4, 0xff},
	{    50, 0xC2, 4, 0xff},
	{     0, 0x00, 0, 0},
};

/* prototypes */
int clcnprobe __P((struct consdev *cp));
int clcninit __P((struct consdev *cp));
int clcngetc __P((dev_t dev));
int clcnputc __P((dev_t dev, char c));
u_char cl_clkdiv __P((int speed));
u_char cl_clknum __P((int speed));
u_char cl_clkrxtimeout __P((int speed));
void clstart __P((struct tty *tp));
void cl_unblock __P((struct tty *tp));
int clccparam __P((struct clsoftc *sc, struct termios *par, int channel));

int clparam __P((struct tty *tp, struct termios *t));
int cl_mintr __P((struct clsoftc *sc));
int cl_txintr __P((struct clsoftc *sc));
int cl_rxintr __P((struct clsoftc *sc));
void cl_overflow __P((struct clsoftc *sc, int channel, long *ptime, char *msg));
void cl_parity __P((struct clsoftc *sc, int channel));
void cl_frame __P((struct clsoftc *sc, int channel));
void cl_break __P(( struct clsoftc *sc, int channel));
int clmctl __P((dev_t dev, int bits, int how));
void cl_dumpport __P((int channel));

int	clprobe __P((struct device *parent, void *self, void *aux));
void	clattach __P((struct device *parent, struct device *self, void *aux));

int clopen  __P((dev_t dev, int flag, int mode, struct proc *p));
int clclose __P((dev_t dev, int flag, int mode, struct proc *p));
int clread  __P((dev_t dev, struct uio *uio, int flag));
int clwrite __P((dev_t dev, struct uio *uio, int flag));
int clioctl __P((dev_t dev, int cmd, caddr_t data, int flag, struct proc *p));
int clstop __P((struct tty *tp, int flag));

static void cl_initchannel __P((struct clsoftc *sc, int channel));
static void clputc __P((struct clsoftc *sc, int unit, char c));
static u_char clgetc __P((struct clsoftc *sc, int *channel));
static void cloutput __P((struct tty *tp));

struct cfdriver clcd = {
	NULL, "cl", clprobe, clattach, DV_TTY, sizeof(struct clsoftc), 0
};

#define CLCDBUF 80
char cltty_ibuffer[CLCDBUF+1];
char cl_obuffer[CLCDBUF+1];

int dopoll = 1;

#define CL_UNIT(x) (minor(x) >> 2)
#define CL_CHANNEL(x) (minor(x) & 3)
#define CL_TTY(x) (minor(x))

extern int cputyp;

struct tty *
cltty(dev)
	dev_t dev;
{
	int unit, channel;
	struct clsoftc *sc;

	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (NULL);
	}
	channel = CL_CHANNEL(dev);
	return (sc->sc_cl[channel].tty);
}

/*
 * probing onboard 166/167/187 CL-cd2400
 * should be previously configured, 
 * we can check the value before resetting the chip
 */
int
clprobe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	volatile u_char *cd_base;
	struct cfdata *cf = self;
	struct confargs *ca = aux;
	int ret;

	if (cputyp != CPU_167 && cputyp != CPU_166)
		return (0);

	cd_base = ca->ca_vaddr;
#if 0
	return (!badvaddr(&cd_base[CD2400_GFRCR], 1));
#endif
	return (ret);
}

void
clattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clsoftc *sc = (struct clsoftc *)self;
	struct confargs *ca = aux;
	int i;
#if 0
	int size = (CD2400_SIZE + PGOFSET) & ~PGOFSET;
#endif

	sc->vbase = ca->ca_vaddr;
	sc->sc_pcctwo = ca->ca_master;

	if (ca->ca_paddr == cl_cons.cl_paddr) {
		/* if this device is configured as console,
		 * line cl_cons.channel is the console */
		sc->sc_cl[cl_cons.channel].cl_consio = 1;
		printf(" console");
	} else {
		/* reset chip only if we are not console device */
		/* wait for GFRCR */
	}
	/* set up global registers */
	sc->vbase[CD2400_TPR] = CL_TIMEOUT;
	sc->vbase[CD2400_RPILR] = 0x03;
	sc->vbase[CD2400_TPILR] = 0x02;
	sc->vbase[CD2400_MPILR] = 0x01;

	for (i = 0; i < CLCD_PORTS_PER_CHIP; i++) {
#if 0
		sc->sc_cl[i].cl_rxmode =
		    !(!((flags >> (i * CL_FLAG_BIT_PCH)) & 0x01));
		sc->sc_cl[i].cl_txmode =
		    !(!((flags >> (i * CL_FLAG_BIT_PCH)) & 0x02));
		sc->sc_cl[i].cl_softchar =
		    !(!((flags >> (i * CL_FLAG_BIT_PCH)) & 0x04));
#endif
		cl_initchannel(sc, i);
	}
	/* enable interrupts */
	sc->sc_ih_e.ih_fn = cl_rxintr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;
	sc->sc_ih_e.ih_wantframe = 0;

	sc->sc_ih_m.ih_fn = cl_mintr;
	sc->sc_ih_m.ih_arg = sc;
	sc->sc_ih_m.ih_ipl = ca->ca_ipl;
	sc->sc_ih_m.ih_wantframe = 0;

	sc->sc_ih_t.ih_fn = cl_txintr;
	sc->sc_ih_t.ih_arg = sc;
	sc->sc_ih_t.ih_ipl = ca->ca_ipl;
	sc->sc_ih_t.ih_wantframe = 0;

	sc->sc_ih_r.ih_fn = cl_rxintr;
	sc->sc_ih_r.ih_arg = sc;
	sc->sc_ih_r.ih_ipl = ca->ca_ipl;
	sc->sc_ih_r.ih_wantframe = 0;
	switch (ca->ca_bustype) {
	case BUS_PCCTWO:
		dopoll = 0;
		pcctwointr_establish(PCC2V_SCC_RXE, &sc->sc_ih_e);
		pcctwointr_establish(PCC2V_SCC_M, &sc->sc_ih_m);
		pcctwointr_establish(PCC2V_SCC_TX, &sc->sc_ih_t);
		pcctwointr_establish(PCC2V_SCC_RX, &sc->sc_ih_r);
		sc->sc_pcctwo = (void *)ca->ca_master;
		sc->sc_pcctwo->pcc2_sccerr = 0x01; /* clear errors */

		/* enable all interrupts at ca_ipl */
		sc->sc_pcctwo->pcc2_sccirq = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
		sc->sc_pcctwo->pcc2_scctx = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
		sc->sc_pcctwo->pcc2_sccrx = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
		break;
	}

	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_txintrcnt);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_rxintrcnt);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_mxintrcnt);
	printf("\n");
}

static void
cl_initchannel(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	int s;
	volatile u_char *cd_base = sc->vbase;

	/* set up option registers */
	sc->sc_cl[channel].tty = NULL;
	s = splhigh();
	cd_base[CD2400_CAR] = (char)channel;
	/* async, do we want to try DMA at some point? */
	cd_base[CD2400_LIVR] = PCC2_VECBASE + 0xc;/* set vector base at 5C */
	cd_base[CD2400_IER] = 0x88;	/* should change XXX */
	cd_base[CD2400_LICR] = 0x00;	/* will change if DMA support XXX */
	/* if the port is not the console */
	if (sc->sc_cl[channel].cl_consio != 1) {
		cd_base[CD2400_CMR] = 0x02; 
		cd_base[CD2400_COR1] = 0x17;
		cd_base[CD2400_COR2] = 0x00;
		cd_base[CD2400_COR3] = 0x02;
		cd_base[CD2400_COR4] = 0xec;
		cd_base[CD2400_COR5] = 0xec;
		cd_base[CD2400_COR6] = 0x00;
		cd_base[CD2400_COR7] = 0x00;
		cd_base[CD2400_SCHR1] = 0x00;
		cd_base[CD2400_SCHR2] = 0x00;
		cd_base[CD2400_SCHR3] = 0x00;
		cd_base[CD2400_SCHR4] = 0x00;
		cd_base[CD2400_SCRl] = 0x00;
		cd_base[CD2400_SCRh] = 0x00;
		cd_base[CD2400_LNXT] = 0x00;
		cd_base[CD2400_RBPR] = 0x40; /* 9600 */
		cd_base[CD2400_RCOR] = 0x01;
		cd_base[CD2400_TBPR] = 0x40; /* 9600 */
		cd_base[CD2400_TCOR] = 0x01 << 5;
		/* console port should be 0x88 already */
		cd_base[CD2400_MSVR_RTS] = 0x00;
		cd_base[CD2400_MSVR_DTR] = 0x00;
		cd_base[CD2400_RTPRl] = CL_RX_TIMEOUT;
		cd_base[CD2400_RTPRh] = 0x00;
	}
	splx(s);
}

int cldefaultrate = TTYDEF_SPEED;

int clmctl(dev, bits, how)
	dev_t dev;
	int bits;
	int how;
{
	struct clsoftc *sc = (struct clsoftc *)clcd.cd_devs[CL_UNIT(dev)];
	int s;

	/* settings are currently ignored */
	s = spltty();
	switch (how) {
	case DMSET:
		if (bits & TIOCM_RTS) {
			sc->vbase[CD2400_MSVR_RTS] = 0x01;
		} else {
			sc->vbase[CD2400_MSVR_RTS] = 0x00;
		}
		if (bits & TIOCM_DTR) {
			sc->vbase[CD2400_MSVR_DTR] = 0x02;
		} else {
			sc->vbase[CD2400_MSVR_DTR] = 0x00;
		}
		break;

	case DMBIC:
		if (bits & TIOCM_RTS) {
			sc->vbase[CD2400_MSVR_RTS] = 0x00;
		}
		if (bits & TIOCM_DTR) {
			sc->vbase[CD2400_MSVR_DTR] = 0x00;
		}
		break;

	case DMBIS:
		if (bits & TIOCM_RTS) {
			sc->vbase[CD2400_MSVR_RTS] = 0x01;
		}
		if (bits & TIOCM_DTR) {
			sc->vbase[CD2400_MSVR_DTR] = 0x02;
		}
		break;

	case DMGET:
		bits = 0;
		{
			u_char msvr = sc->vbase[CD2400_MSVR_RTS];

			if (msvr & 0x80)
				bits |= TIOCM_DSR;
			if (msvr & 0x40)
				bits |= TIOCM_CD;
			if (msvr & 0x20)
				bits |= TIOCM_CTS;
			if (msvr & 0x10)
				bits |= TIOCM_DTR;
			if (msvr & 0x02)
				bits |= TIOCM_DTR;
			if (msvr & 0x01)
				bits |= TIOCM_RTS;
		}
		break;
	}
	(void)splx(s);
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
	return (bits);
}

int
clopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int s, unit, channel;
	struct cl_info *cl;
	struct clsoftc *sc;
	struct tty *tp;
	
	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	s = spltty();
	if (cl->tty) {
		tp = cl->tty;
	} else {
		tp = cl->tty = ttymalloc();
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
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cldefaultrate;
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
#ifdef XXX
		if ((cl->cl_swflags & TIOCFLAG_SOFTCAR) ||
			(clmctl(dev, 0, DMGET) & TIOCM_CD)) {
			tp->t_state |= TS_CARR_ON;
		} else {
			tp->t_state &= ~TS_CARR_ON;
		}
#endif
		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}
#ifdef XXX
	/*
	 * if NONBLOCK requested, ignore carrier
	 */
	if (flag & O_NONBLOCK)
		goto done;
#endif

	splx(s);
	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
#ifdef DEBUG
	cl_dumpport(channel);
#endif
	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
clparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int unit, channel;
	struct clsoftc *sc;
	int s;
	dev_t dev;

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
/*
	t->c_ispeed = tp->t_ispeed;
	t->c_ospeed = tp->t_ospeed;
	t->c_cflag = tp->t_cflag;
*/
	clccparam(sc, t, channel);
	s = spltty();
	cl_unblock(tp);
	splx(s);
	return (0);
}

void
cloutput(tp)
	struct tty *tp;
{
	int cc, s, unit, cnt;
	char *tptr;
	int channel;
	struct clsoftc *sc;
	dev_t dev;

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return;
	}
	channel = CL_CHANNEL(dev);

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = spltty();
	cc = tp->t_outq.c_cc;
	while (cc > 0) {
/*XXX*/
		cnt = min(CLCDBUF, cc);
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
clclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;
	int s;
	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	(*linesw[tp->t_line].l_close)(tp, flag);

	s = spltty();
	sc->vbase[CD2400_CAR] = channel;
	if (cl->cl_consio == 0 && (tp->t_cflag & HUPCL) != 0) {
		sc->vbase[CD2400_MSVR_RTS] = 0x00;
		sc->vbase[CD2400_MSVR_DTR] = 0x00;
		sc->vbase[CD2400_CCR] = 0x05;
	}

	splx(s);
	ttyclose(tp);

#if 0
	cl->tty = NULL;
#endif
#if 0
	cl_dumpport(channel);
#endif
	return (0);
}

int
clread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;

	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
clwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;

	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return (ENXIO);
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
clioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error;
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;

	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return (ENXIO);

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

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
		error = suser(p->p_ucred, &p->p_acflag); 
		if (error != 0)
			return (EPERM); 

		cl->cl_swflags = *(int *)data;
		cl->cl_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

int
clstop(tp, flag)
	struct tty *tp;
	int flag;
{
	int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return (0);
}

int
clcnprobe(cp)
	struct consdev *cp;
{
	/* always there ? */
	/* serial major */
	int maj;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == clopen)
			break;
	cp->cn_dev = makedev (maj, 0);
	cp->cn_pri = CN_NORMAL;

	return (1);
}

int
clcninit(cp)
	struct consdev *cp;
{
#ifdef MAP_DOES_WORK
	int size = (0x1ff + PGOFSET) & ~PGOFSET;
	int pcc2_size = (0x3C + PGOFSET) & ~PGOFSET;
#endif
	volatile u_char *cd_base;
	
	cl_cons.cl_paddr = (void *)0xfff45000;
#ifdef MAP_DOES_WORK
	cl_cons.cl_vaddr = mapiodev(cl_cons.cl_paddr, size);
	cd_pcc2_base = mapiodev(0xfff42000, pcc2_size);
#else
	cl_cons.cl_vaddr = cl_cons.cl_paddr;
	cl_cons.pcctwoaddr = (void *)0xfff42000;
#endif
	cd_base = cl_cons.cl_vaddr;
	/* reset the chip? */
#ifdef CLCD_DO_RESET
#endif
#ifdef NEW_CLCD_STRUCT
	/* set up globals */
	cl->tftc = 0x10;
	cl->tpr = CL_TIMEOUT; /* is this correct?? */
	cl->rpilr = 0x03;
	cl->tpilr = 0x02;
	cl->mpilr = 0x01;

	/* set up the tty00 to be 9600 8N1 */
	cl->car = 0x00;
	cl->cor1 = 0x17;	/* No parity, ignore parity, 8 bit char */
	cl->cor2 = 0x00;
	cl->cor3 = 0x02;	/* 1 stop bit */
	cl->cor4 = 0x00;
	cl->cor5 = 0x00;
	cl->cor6 = 0x00;
	cl->cor7 = 0x00;
	cl->schr1 = 0x00;
	cl->schr2 = 0x00;
	cl->schr3 = 0x00;
	cl->schr4 = 0x00;
	cl->scrl = 0x00;
	cl->scrh = 0x00;
	cl->lnxt = 0x00;
	cl->cpsr = 0x00;
#else
	/* set up globals */
#ifdef NOT_ALREADY_SETUP
	cd_base[CD2400_TFTC] = 0x10;
	cd_base[CD2400_TPR] = CL_TIMEOUT; /* is this correct?? */
	cd_base[CD2400_RPILR] = 0x03;
	cd_base[CD2400_TPILR] = 0x02;
	cd_base[CD2400_MPILR] = 0x01;

	/* set up the tty00 to be 9600 8N1 */
	cd_base[CD2400_CAR] = 0x00;
	cd_base[CD2400_COR1] = 0x17;	/* No parity, ignore parity, 8 bit char */
	cd_base[CD2400_COR2] = 0x00;
	cd_base[CD2400_COR3] = 0x02;	/* 1 stop bit */
	cd_base[CD2400_COR4] = 0x00;
	cd_base[CD2400_COR5] = 0x00;
	cd_base[CD2400_COR6] = 0x00;
	cd_base[CD2400_COR7] = 0x00;
	cd_base[CD2400_SCHR1] = 0x00;
	cd_base[CD2400_SCHR2] = 0x00;
	cd_base[CD2400_SCHR3] = 0x00;
	cd_base[CD2400_SCHR4] = 0x00;
	cd_base[CD2400_SCRl] = 0x00;
	cd_base[CD2400_SCRh] = 0x00;
	cd_base[CD2400_LNXT] = 0x00;
	cd_base[CD2400_CPSR] = 0x00;
#endif
#endif
	return (0);
}

int
cl_instat(sc)
	struct clsoftc *sc;
{
	volatile u_char *cd_base;

	if ( NULL == sc) {
		cd_base = cl_cons.cl_vaddr;
	} else {
		cd_base = sc->vbase;
	}
	return (cd_base[CD2400_RIR] & 0x80);
}

int
clcngetc(dev)
	dev_t dev;
{
	u_char val, reoir, licr, isrl, data, status, fifo_cnt;
	int got_char = 0;
	volatile u_char *cd_base = cl_cons.cl_vaddr;
	volatile struct pcctworeg *pcc2_base = cl_cons.pcctwoaddr;

	while (got_char == 0) {
		val = cd_base[CD2400_RIR];
		/* if no receive interrupt pending wait */
		if (!(val & 0x80)) {
			continue;
		}
		/* XXX do we need to suck the entire FIFO contents? */
		reoir = pcc2_base->pcc2_sccrxiack; /* receive PIACK */
		licr = cd_base[CD2400_LICR];
		if (((licr >> 2) & 0x3) == 0) {
			/* is the interrupt for us (port 0) */
			/* the character is for us. */
			isrl = cd_base[CD2400_RISRl];
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
			fifo_cnt = cd_base[CD2400_RFOC];
			data = cd_base[CD2400_RDR];
			got_char = 1;
			cd_base[CD2400_TEOIR] = 0x00;
		} else {
			data = cd_base[CD2400_RDR];
			cd_base[CD2400_TEOIR] = 0x00;
		}

	}
	return (data);
}

int
clcnputc(dev, c)
	dev_t dev;
	char c;
{
	/* is this the correct location for the cr -> cr/lf tranlation? */
	if (c == '\n')
		clputc(0, 0, '\r');

	clputc(0, 0, c);
	return (0);
}

clcnpollc(dev, on)
	dev_t dev;
	int on;
{
	if (1 == on) {
		/* enable polling */
	} else {
		/* disable polling */
	}
}

static void
clputc(sc, unit, c)
	struct clsoftc *sc;
	int unit;
	char c;
{
	int s;
	u_char schar;
	u_char oldchannel;
	volatile u_char *cd_base;
	if (0 == sc) {
		/* output on console */
		cd_base = cl_cons.cl_vaddr;
	} else {
		cd_base = sc->vbase;
	}
#ifdef NEW_CLCD_STRUCT
	/* should we disable, flush and all that goo? */
	cl->car = unit;
	schar = cl->schr3;
	cl->schr3 = c;
	cl->stcr = 0x08 | 0x03; /* send special char, char 3 */
	while (0 != cl->stcr) {
		/* wait until cl notices the command
		 * otherwise it may not notice the character
		 * if we send characters too fast.
		 */
	}
	cl->schr3 = schar;
#else
if (unit == 0) {
	s = splhigh();
	oldchannel = cd_base[CD2400_CAR];
	cd_base[CD2400_CAR] = unit;
	schar = cd_base[CD2400_SCHR3];
	cd_base[CD2400_SCHR3] = c;
	cd_base[CD2400_STCR] = 0x08 | 0x03; /* send special char, char 3 */
	while (0 != cd_base[CD2400_STCR]) {
		/* wait until cl notices the command
		 * otherwise it may not notice the character
		 * if we send characters too fast.
		 */
	}
	DELAY(5);
	cd_base[CD2400_SCHR3] = schar;
	cd_base[CD2400_CAR] = oldchannel;
	splx(s);
} else {
	s = splhigh();
	oldchannel = cd_base[CD2400_CAR];
	cd_base[CD2400_CAR] = unit;
	if (cd_base[CD2400_TFTC] > 0) {
		cd_base[CD2400_TDR] = c;
	}
	cd_base[CD2400_CAR] = oldchannel;
	splx(s);
}
#endif
}

/*
#ifdef CLCD_DO_POLLED_INPUT
*/
#if 1
void
cl_chkinput()
{
	struct tty *tp;
	int unit;
	struct clsoftc *sc;
	int channel;

	if (dopoll == 0)
		return;
	for (unit = 0; unit < clcd.cd_ndevs; unit++) {
		if (unit >= clcd.cd_ndevs || 
			(sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
			continue;
		}
		if (cl_instat(sc)) {
			while (cl_instat(sc)){
				int ch;
				u_char c;
				/*
				*(pinchar++) = clcngetc();
				*/
				ch = clgetc(sc, &channel) & 0xff;
				c = ch;

				tp = sc->sc_cl[channel].tty;
				if (NULL != tp) {
					(*linesw[tp->t_line].l_rint)(c, tp);
				}
			}
			/*
			wakeup(tp);
			*/
		}
	}
}
#endif

static u_char 
clgetc(sc, channel)
	struct clsoftc *sc;
	int *channel;
{
	volatile u_char *cd_base;
	volatile struct pcctworeg *pcc2_base;
	u_char val, reoir, licr, isrl, fifo_cnt, data;

	if (0 == sc) {
		cd_base = cl_cons.cl_vaddr;
		pcc2_base = cl_cons.pcctwoaddr;
	} else {
		cd_base = sc->vbase;
		pcc2_base = sc->sc_pcctwo;
	}
	val = cd_base[CD2400_RIR];
	/* if no receive interrupt pending wait */
	if (!(val & 0x80)) {
		return (0);
	}
	/* XXX do we need to suck the entire FIFO contents? */
	reoir = pcc2_base->pcc2_sccrxiack; /* receive PIACK */
	licr = cd_base[CD2400_LICR];
	*channel = (licr >> 2) & 0x3;
	/* is the interrupt for us (port 0) */
	/* the character is for us yea. */
	isrl = cd_base[CD2400_RISRl];
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
	fifo_cnt = cd_base[CD2400_RFOC];
	if (fifo_cnt > 0) {
		data = cd_base[CD2400_RDR];
		cd_base[CD2400_TEOIR] = 0x00;
	} else {
		data = 0;
		cd_base[CD2400_TEOIR] = 0x08;
	}
	return (data);
}

int
clccparam(sc, par, channel)
	struct clsoftc *sc;
	struct termios *par;
	int channel;
{
	u_int divisor, clk, clen;
	int s, imask, ints;

	s = spltty();
	sc->vbase[CD2400_CAR] = channel;
	if (par->c_ospeed == 0) { 
		/* dont kill the console */
		if (sc->sc_cl[channel].cl_consio == 0) {
			/* disconnect, drop RTS DTR stop reciever */
			sc->vbase[CD2400_MSVR_RTS] = 0x00;
			sc->vbase[CD2400_MSVR_DTR] = 0x00;
			sc->vbase[CD2400_CCR] = 0x05;
		}
		splx(s);
		return (0xff);
	}

	sc->vbase[CD2400_MSVR_RTS] = 0x03;
	sc->vbase[CD2400_MSVR_DTR] = 0x03;

	divisor = cl_clkdiv(par->c_ospeed);
	clk = cl_clknum(par->c_ospeed);
	sc->vbase[CD2400_TBPR] = divisor;
	sc->vbase[CD2400_TCOR] = clk << 5;
	divisor = cl_clkdiv(par->c_ispeed);
	clk = cl_clknum(par->c_ispeed);
	sc->vbase[CD2400_RBPR] = divisor;
	sc->vbase[CD2400_RCOR] = clk;
	sc->vbase[CD2400_RTPRl] = cl_clkrxtimeout(par->c_ispeed);
	sc->vbase[CD2400_RTPRh] = 0x00;

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
	sc->vbase[CD2400_COR3] = par->c_cflag & PARENB ? 4 : 2;

	if (par->c_cflag & PARENB) {
		if (par->c_cflag & PARODD) {
			sc->vbase[CD2400_COR1] = 0xE0 | clen ; /* odd */
		} else {
			sc->vbase[CD2400_COR1] = 0x40 | clen ; /* even */
		}
	} else {
		sc->vbase[CD2400_COR1] = 0x10 | clen; /* ignore parity */
	}

	if (sc->sc_cl[channel].cl_consio == 0 &&
	    (par->c_cflag & CREAD) == 0 ) {
/*
		sc->vbase[CD2400_CSR] = 0x08;
*/
		sc->vbase[CD2400_CCR] = 0x08;
	} else {
		sc->vbase[CD2400_CCR] = 0x0a;
	}
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
	sc->vbase[CD2400_COR4] = ints | CL_FIFO_CNT;
	sc->vbase[CD2400_COR5] = ints | CL_FIFO_CNT;
	return (imask);
}

static int clknum = 0;

u_char
cl_clkdiv(speed)
	int speed;
{
	int i = 0;

	if (cl_clocks[clknum].speed == speed) {
		return (cl_clocks[clknum].divisor);
	}
	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return (cl_clocks[clknum].divisor);
		}
	}
	/* return some sane value if unknown speed */
	return (cl_clocks[4].divisor);
}

u_char 
cl_clknum(speed)
	int speed;
{
	int found = 0;
	int i = 0;

	if (cl_clocks[clknum].speed == speed) {
		return (cl_clocks[clknum].clock);
	}
	for  (i = 0; found != 0 && cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[clknum].speed == speed) {
			clknum = i;
			return (cl_clocks[clknum].clock);
		}
	}
	/* return some sane value if unknown speed */
	return (cl_clocks[4].clock);
}

u_char 
cl_clkrxtimeout(speed)
	int speed;
{
	int i = 0;

	if (cl_clocks[clknum].speed == speed) {
		return (cl_clocks[clknum].rx_timeout);
	}
	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return (cl_clocks[clknum].rx_timeout);
		}
	}
	/* return some sane value if unknown speed */
	return (cl_clocks[4].rx_timeout);
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
	struct clsoftc *sc;
	int channel, unit, s, cnt;

	dev = tp->t_dev;
	channel = CL_CHANNEL(dev);
/* hack to test output on non console only */
#if 0
	if (channel == 0) {
		cloutput(tp);
		return;
	}
#endif
	unit = CL_UNIT(dev);
	if (unit >= clcd.cd_ndevs || 
	    (sc = (struct clsoftc *) clcd.cd_devs[unit]) == NULL) {
		return;
	}

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = spltty();
#if 0
	if (sc->sc_cl[channel].transmitting == 1) {
		/* i'm busy, go away, I will get to it later. */
		splx(s);
		return;
	}
	cnt = q_to_b(&tp->t_outq, &cbuf, 1);
	if (cnt != 0) {
		sc->sc_cl[channel].transmitting = 1;
		sc->vbase[CD2400_CAR] = channel;
		sc->vbase[CD2400_TDR] = cbuf;
	} else {
		sc->sc_cl[channel].transmitting = 0;
	}
#else
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0) {
		tp->t_state |= TS_BUSY;
		sc->vbase[CD2400_CAR] = channel;
		sc->vbase[CD2400_IER] = 0xb;
	}
#endif
	splx(s);
}

int
cl_mintr(sc)
	struct clsoftc *sc;
{
	u_char mir, misr, msvr;
	int channel;
	struct tty *tp;

	mir = sc->vbase[CD2400_MIR];
	if ((mir & 0x40) == 0x0) {
		/* only if intr is not shared? */
		printf("cl_mintr extra intr\n");
		return (0);
	}
	sc->sc_mxintrcnt.ev_count++;

	channel = mir & 0x03;
	misr = sc->vbase[CD2400_MISR];
	msvr = sc->vbase[CD2400_MSVR_RTS];
	if (misr & 0x01) {
		/* timers are not currently used?? */
		printf("cl_mintr: channel %x timer 1 unexpected\n", channel);
	}
	if (misr & 0x02) {
		/* timers are not currently used?? */
		printf("cl_mintr: channel %x timer 2 unexpected\n", channel);
	}
	if (misr & 0x20) {
		printf("cl_mintr: channel %x cts %x\n", channel, 
		    (msvr & 0x20) != 0x0);
	}
	if (misr & 0x40) {
		struct tty *tp = sc->sc_cl[channel].tty;

		printf("cl_mintr: channel %x cd %x\n", channel,
		    (msvr & 0x40) != 0x0);
		ttymodem(tp, (msvr & 0x40) != 0x0);
	}
	if (misr & 0x80) {
		printf("cl_mintr: channel %x dsr %x\n", channel,
		    (msvr & 0x80) != 0x0);
	}
	sc->vbase[CD2400_MEOIR] = 0x00;
	return (1);
}

int
cl_txintr(sc)
	struct clsoftc *sc;
{
	static empty = 0;
	u_char tir, licr, teoir;
	u_char max;
	int channel;
	struct tty *tp;
	int cnt;
	u_char buffer[CL_FIFO_MAX +1];
	u_char *tptr;

	tir = sc->vbase[CD2400_TIR];
	if ((tir & 0x40) == 0x0) {
		/* only if intr is not shared ??? */
		printf("cl_txintr extra intr\n");
		return (0);
	}
	sc->sc_txintrcnt.ev_count++;

	channel = tir & 0x03;
	licr = sc->vbase[CD2400_LICR];
	
	sc->sc_cl[channel].txcnt ++;

	tp = sc->sc_cl[channel].tty;
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0) {
		sc->vbase[CD2400_TEOIR] = 0x08;
		return (1);
	}
	switch ((licr >> 4) & 0x3) {
	case CL_DMAMODE:
		teoir = 0x08;
		break;
	case CL_INTRMODE:
		max = sc->vbase[CD2400_TFTC];
		cnt = min((int)max, tp->t_outq.c_cc);
		if (cnt != 0) {
			cnt = q_to_b(&tp->t_outq, buffer, cnt);
			empty = 0;
			for (tptr = buffer; tptr < &buffer[cnt]; tptr++) {
				sc->vbase[CD2400_TDR]= *tptr;
			}
			teoir = 0x00;
		} else {
			if (empty > 5 && (empty % 20000 ) == 0) {
				printf("cl_txintr: too many empty intr %d chan %d\n",
				    empty, channel);
			}
			empty++;
			teoir = 0x08;
			if (tp->t_state & TS_BUSY) {
				tp->t_state &= ~(TS_BUSY | TS_FLUSH);
				if (tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup((caddr_t)&tp->t_outq);
				}
				selwakeup(&tp->t_wsel);
			}
			sc->vbase[CD2400_IER] = sc->vbase[CD2400_IER] & ~0x3;
		}
		break;
	default:
		printf("cl_txintr unknown mode %x\n", (licr >> 4) & 0x3);
		/* we probably will go to hell quickly now */
		teoir = 0x08;
	}
	sc->vbase[CD2400_TEOIR] = teoir;
	return (1);
}

int
cl_rxintr(sc)
	struct clsoftc *sc;
{
	u_char rir, channel, licr, risrl;
	u_char c;
	u_char fifocnt;
	struct tty *tp;
	int i;
	u_char reoir;
	
	rir = sc->vbase[CD2400_RIR];
	if ((rir & 0x40) == 0x0) {
		/* only if intr is not shared ??? */
		printf("cl_rxintr extra intr\n");
		return (0);
	}
	sc->sc_rxintrcnt.ev_count++;
	channel = rir & 0x3;
	licr = sc->vbase[CD2400_LICR];
	reoir = 0x08;

	sc->sc_cl[channel].rxcnt ++;

	switch (licr & 0x03) {
	case CL_DMAMODE:
		reoir = 0x08;
		break;
	case CL_INTRMODE:
		risrl = sc->vbase[CD2400_RISRl];
		if (risrl & 0x80) {
			/* timeout, no characters */
			reoir = 0x08;
		} else
		/* We don't need no sinkin special characters */
		if (risrl & 0x08) {
			cl_overflow (sc, channel, &sc->sc_fotime, "fifo");
			reoir = 0x08;
		} else
		if (risrl & 0x04) {
			cl_parity(sc, channel);
			reoir = 0x08;
		} else
		if (risrl & 0x02) {
			cl_frame(sc, channel);
			reoir = 0x08;
		} else
		if (risrl & 0x01) {
			cl_break(sc, channel);
			reoir = 0x08;
		} else {
			fifocnt = sc->vbase[CD2400_RFOC];
			tp = sc->sc_cl[channel].tty;
			for (i = 0; i < fifocnt; i++) {
				c = sc->vbase[CD2400_RDR];
#if USE_BUFFER
				cl_appendbuf(sc, channel, c);
#else
				/* does any restricitions exist on spl
				 * for this call
				 */
				(*linesw[tp->t_line].l_rint)(c, tp);
				reoir = 0x00;
#endif
			}
		}
		break;
	default:
		printf("cl_rxintr unknown mode %x\n", licr & 0x03);
		/* we probably will go to hell quickly now */
		reoir = 0x08;
	}
	sc->vbase[CD2400_REOIR] = reoir;
	return (1);
}

void
cl_overflow(sc, channel, ptime, msg)
	struct clsoftc *sc;
	int channel;
	long *ptime;
	char *msg;
{
/*
	if (*ptime != time.tv_sec) {
*/
	{
/*
		*ptime = time.tv_sec);
*/
		log(LOG_WARNING, "%s%d[%d]: %s overrun", clcd.cd_name,
			0 /* fix */, channel, msg);
	}
}

void
cl_parity(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: parity error", clcd.cd_name, 0, channel);
}

void
cl_frame(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: frame error", clcd.cd_name, 0, channel);
}

void
cl_break(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s%d[%d]: break detected", clcd.cd_name, 0, channel);
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
	u_char livr, cmr, cor1, cor2, cor3, cor4, cor5, cor6, cor7;
	u_char schr1, schr2, schr3, schr4, scrl, scrh, lnxt;
	u_char rbpr, rcor, tbpr, tcor, rpilr, rir, tpr, ier, ccr;
	u_char csr, rts, dtr, rtprl, rtprh;
	struct clsoftc *sc = (struct clsoftc *) clcd.cd_devs[0];
	volatile u_char *cd_base = cl_cons.cl_vaddr;
	int s;

	s = spltty();
	cd_base[CD2400_CAR] = (char) channel;
	livr = cd_base[CD2400_LIVR];
	cmr = cd_base[CD2400_CMR];
	cor1 = cd_base[CD2400_COR1];
	cor2 = cd_base[CD2400_COR2];
	cor3 = cd_base[CD2400_COR3];
	cor4 = cd_base[CD2400_COR4];
	cor5 = cd_base[CD2400_COR5];
	cor6 = cd_base[CD2400_COR6];
	cor7 = cd_base[CD2400_COR7];
	schr1 = cd_base[CD2400_SCHR1];
	schr2 = cd_base[CD2400_SCHR2];
	schr3 = cd_base[CD2400_SCHR3];
	schr4 = cd_base[CD2400_SCHR4];
	scrl = cd_base[CD2400_SCRl];
	scrh = cd_base[CD2400_SCRh];
	lnxt = cd_base[CD2400_LNXT];
	rbpr = cd_base[CD2400_RBPR];
	rcor = cd_base[CD2400_RCOR];
	tbpr = cd_base[CD2400_TBPR];
	rpilr = cd_base[CD2400_RPILR];
	ier = cd_base[CD2400_IER];
	ccr = cd_base[CD2400_CCR];
	tcor = cd_base[CD2400_TCOR];
	csr = cd_base[CD2400_CSR];
	tpr = cd_base[CD2400_TPR];
	rts = cd_base[CD2400_MSVR_RTS];
	dtr = cd_base[CD2400_MSVR_DTR];
	rtprl = cd_base[CD2400_RTPRl];
	rtprh = cd_base[CD2400_RTPRh];
	splx(s);

	printf("{ port %x livr %x cmr %x\n", channel, livr, cmr);
	printf("cor1 %x cor2 %x cor3 %x cor4 %x cor5 %x cor6 %x cor7 %x\n",
	    cor1, cor2, cor3, cor4, cor5, cor6, cor7);
	printf("schr1 %x schr2 %x schr3 %x schr4 %x\n", schr1, schr2, schr3,
	    schr4);
	printf("scrl %x scrh %x lnxt %x\n", scrl, scrh, lnxt);
	printf("rbpr %x rcor %x tbpr %x tcor %x\n", rbpr, rcor, tbpr, tcor);
	printf("rpilr %x rir %x ier %x ccr %x\n", rpilr, rir, ier, ccr);
	printf("tpr %x csr %x rts %x dtr %x\n", tpr, csr, rts, dtr);
	printf("rtprl %x rtprh %x\n", rtprl, rtprh);
	printf("rxcnt %x txcnt %x\n", sc->sc_cl[channel].rxcnt,
	    sc->sc_cl[channel].txcnt);
	printf("}\n");
}
