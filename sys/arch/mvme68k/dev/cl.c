/*	$OpenBSD: cl.c,v 1.20 2000/03/26 23:31:58 deraadt Exp $ */

/*
 * Copyright (c) 1995 Dale Rahn. All rights reserved.
 *
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
 *	This product includes software developed by Dale Rahn.
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

/* DMA mode still does not work!!! */

#include <sys/param.h>
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
#include <mvme68k/dev/clreg.h>
#include <sys/syslog.h>
#include "cl.h"

#include "pcctwo.h"

#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#endif

#define splcl() spl3()
#define USE_BUFFER

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

struct cl_cons {
	void	*cl_paddr;
	struct clreg *cl_vaddr;
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

	void *rx[2];
	void *rxp[2];
	void *tx[2];
	void *txp[2];

	volatile u_char *pconsum;
	volatile u_char *psupply;
	volatile u_char *buffer; 
	volatile int	nchar;
};
#define CLCD_PORTS_PER_CHIP 4
#define CL_BUFSIZE 256

#ifndef DO_MALLOC
/* four (4) buffers per port */
char cl_dmabuf [CLCD_PORTS_PER_CHIP * CL_BUFSIZE * 4];
#endif

struct clsoftc {
	struct device	sc_dev;
	struct evcnt sc_txintrcnt;
	struct evcnt sc_rxintrcnt;
	struct evcnt sc_mxintrcnt;
	time_t	sc_rotime;	/* time of last ring overrun */
	time_t	sc_fotime;	/* time of last fifo overrun */
	u_char *pbase;
	struct clreg *cl_reg;
	struct cl_info		sc_cl[CLCD_PORTS_PER_CHIP];
	struct intrhand		sc_ih_e;
	struct intrhand		sc_ih_m;
	struct intrhand		sc_ih_t;
	struct intrhand		sc_ih_r;
	struct pcctworeg	*sc_pcctwo;
	int			sc_flags;
	int			ssir;
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
int clcnputc __P((dev_t dev, u_char c));
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
void cl_overflow __P((struct clsoftc *sc, int channel, time_t *ptime, u_char *msg));
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
int clstop  __P((struct tty *tp, int flag));

static void cl_initchannel __P((struct clsoftc *sc, int channel));
static void clputc __P((struct clsoftc *sc, int unit, u_char c));
static u_char clgetc __P((struct clsoftc *sc, int *channel));
static void cloutput __P( (struct tty *tp));
void cl_softint __P((struct clsoftc *sc));
void cl_appendbufn __P((struct clsoftc *sc, u_char channel, u_char *buf, u_short cnt));

struct cfattach cl_ca = {
	sizeof(struct clsoftc), clprobe, clattach
};

struct cfdriver cl_cd = {
	NULL, "cl", DV_TTY, 0
};

#define CLCDBUF 80

int dopoll = 1;

#define CL_UNIT(x) (minor(x) >> 2)
#define CL_CHANNEL(x) (minor(x) & 3)
#define CL_TTY(x) (minor(x))

extern int cputyp;

struct tty * cltty(dev)
	dev_t dev;
{
	int unit, channel;
	struct clsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return (NULL);
	}
	channel = CL_CHANNEL(dev);
	return sc->sc_cl[channel].tty;
}

int	clprobe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	/* probing onboard 166/167/177/187 CL-cd2400
	 * should be previously configured, 
	 * we can check the value before resetting the chip
	 */
	struct clreg *cl_reg;
	struct confargs *ca = aux;
	int ret;
	if (cputyp != CPU_167 && cputyp != CPU_166 && cputyp != CPU_177)
	{
		return 0;
	}
   cl_reg = (struct clreg *)ca->ca_vaddr;

#if 0
	ret = !badvaddr(&cl_reg->cl_gfrcr,1);
#else
	ret = 1;
#endif
	return ret;
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

	sc->cl_reg = (struct clreg *)ca->ca_vaddr;
	sc->sc_pcctwo = ca->ca_master;

	if (ca->ca_paddr == cl_cons.cl_paddr) {
		/* if this device is configured as console,
		 * line cl_cons.channel is the console */
		sc->sc_cl[0].cl_consio = 1;
		printf(": console");
	} else {
		/* reset chip only if we are not console device */
		/* wait for GFRCR */
	}
        /* allow chip to settle before continuing */
        delay(800);

	/* set up global registers */
	sc->cl_reg->cl_tpr = CL_TIMEOUT;
	sc->cl_reg->cl_rpilr = 0x03;
	sc->cl_reg->cl_tpilr = 0x02;
	sc->cl_reg->cl_mpilr = 0x01;

#ifdef DO_MALLOC
	sc->sc_cl[0].rx[0] = (void *)(dvma_malloc(16 * CL_BUFSIZE));
#else
	sc->sc_cl[0].rx[0] = (void *) (&cl_dmabuf);
#endif
	sc->sc_cl[0].rx[1] = (void *)(((int)sc->sc_cl[0].rx[0]) + CL_BUFSIZE);
	sc->sc_cl[1].rx[0] = (void *)(((int)sc->sc_cl[0].rx[1]) + CL_BUFSIZE);
	sc->sc_cl[1].rx[1] = (void *)(((int)sc->sc_cl[1].rx[0]) + CL_BUFSIZE);

	sc->sc_cl[2].rx[0] = (void *)(((int)sc->sc_cl[1].rx[1]) + CL_BUFSIZE);
	sc->sc_cl[2].rx[1] = (void *)(((int)sc->sc_cl[2].rx[0]) + CL_BUFSIZE);
	sc->sc_cl[3].rx[0] = (void *)(((int)sc->sc_cl[2].rx[1]) + CL_BUFSIZE);
	sc->sc_cl[3].rx[1] = (void *)(((int)sc->sc_cl[3].rx[0]) + CL_BUFSIZE);

	sc->sc_cl[0].tx[0] = (void *)(((int)sc->sc_cl[3].rx[1]) + CL_BUFSIZE);
	sc->sc_cl[0].tx[1] = (void *)(((int)sc->sc_cl[0].tx[0]) + CL_BUFSIZE);
	sc->sc_cl[1].tx[0] = (void *)(((int)sc->sc_cl[0].tx[1]) + CL_BUFSIZE);
	sc->sc_cl[1].tx[1] = (void *)(((int)sc->sc_cl[1].tx[0]) + CL_BUFSIZE);

	sc->sc_cl[2].tx[0] = (void *)(((int)sc->sc_cl[1].tx[1]) + CL_BUFSIZE);
	sc->sc_cl[2].tx[1] = (void *)(((int)sc->sc_cl[2].tx[0]) + CL_BUFSIZE);
	sc->sc_cl[3].tx[0] = (void *)(((int)sc->sc_cl[2].tx[1]) + CL_BUFSIZE);
	sc->sc_cl[3].tx[1] = (void *)(((int)sc->sc_cl[3].tx[0]) + CL_BUFSIZE);
#ifdef USE_BUFFER
	/* receive buffer and dma buffer are "shared" */
	for (i = 0; i < CLCD_PORTS_PER_CHIP; i++) {
		sc->sc_cl[i].buffer = sc->sc_cl[i].rx[0];
		sc->sc_cl[i].pconsum = sc->sc_cl[i].buffer;
		sc->sc_cl[i].psupply = sc->sc_cl[i].buffer;
		sc->sc_cl[i].nchar = 0;
	}
	sc->ssir = allocate_sir(cl_softint, (void *)sc);
#endif
	for (i = 0; i < CLCD_PORTS_PER_CHIP; i++) {
		int j;
#if 0
		for (j = 0; j < 2 ; j++) {
			sc->sc_cl[i].rxp[j] = (void *)kvtop(sc->sc_cl[i].rx[j]);
			printf("cl[%d].rxbuf[%d] %x p %x\n",
			    i, j, sc->sc_cl[i].rx[j], sc->sc_cl[i].rxp[j]);
			sc->sc_cl[i].txp[j] = (void *)kvtop(sc->sc_cl[i].tx[j]);
			printf("cl[%d].txbuf[%d] %x p %x\n",
			    i, j, sc->sc_cl[i].tx[j], sc->sc_cl[i].txp[j]);
		}
#endif
		
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
        /* allow chip to settle before continuing */
        delay(50);

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
		pcctwointr_establish(PCC2V_SCC_RXE,&sc->sc_ih_e);
		pcctwointr_establish(PCC2V_SCC_M,&sc->sc_ih_m);
		pcctwointr_establish(PCC2V_SCC_TX,&sc->sc_ih_t);
		pcctwointr_establish(PCC2V_SCC_RX,&sc->sc_ih_r);
		sc->sc_pcctwo = (void *)ca->ca_master;
		sc->sc_pcctwo->pcc2_sccerr = 0x01; /* clear errors */

		/* enable all interrupts at ca_ipl */
		sc->sc_pcctwo->pcc2_sccirq = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
		sc->sc_pcctwo->pcc2_scctx  = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
		sc->sc_pcctwo->pcc2_sccrx  = PCC2_IRQ_IEN | (ca->ca_ipl & 0x7);
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
	struct clreg *cl_reg = sc->cl_reg;
	/* set up option registers */
	sc->sc_cl[channel].tty = NULL;
	s = splhigh();
	cl_reg->cl_car	= (u_char) channel;
	cl_reg->cl_livr	= PCC2_VECBASE + 0xc;/* set vector base at 5C */
	/* if the port is not the console, should be init for all ports??*/
	cl_reg->cl_ier	= 0x00;
	if (sc->sc_cl[channel].cl_consio != 1) {
		cl_reg->cl_cmr	= 0x02; 
		cl_reg->cl_cor1	= 0x17;
		cl_reg->cl_cor2	= 0x00;
		cl_reg->cl_cor3	= 0x02;
		cl_reg->cl_cor4	= 0xec;
		cl_reg->cl_cor5	= 0xec;
		cl_reg->cl_cor6	= 0x00;
		cl_reg->cl_cor7	= 0x00;
		cl_reg->cl_schr1	= 0x00;
		cl_reg->cl_schr2	= 0x00;
		cl_reg->cl_schr3	= 0x00;
		cl_reg->cl_schr4	= 0x00;
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
	}
	sc->cl_reg->cl_ccr = 0x20;
	while (sc->cl_reg->cl_ccr != 0) {
	}

	splx(s);
}


int cldefaultrate = TTYDEF_SPEED;

int clmctl (dev, bits, how)
	dev_t dev;
	int bits;
	int how;
{
	int s;
	struct clsoftc *sc;
	/* should only be called with valid device */
	sc = (struct clsoftc *) cl_cd.cd_devs[CL_UNIT(dev)];
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
	return(bits);
}

int clopen (dev, flag, mode, p)
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
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	s = splcl();
	if (cl->tty) {
		tp = cl->tty;
	} else {
		tp = cl->tty = ttymalloc();
		tty_attach(tp);
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

			if(sc->sc_cl[channel].cl_consio == 1) {
				/* console is 8N1 */
				tp->t_cflag = (CREAD | CS8 | HUPCL);
			} else {
				tp->t_cflag = TTYDEF_CFLAG;
			}
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
		{
			u_char save = sc->cl_reg->cl_car;
			sc->cl_reg->cl_car = channel;
			sc->cl_reg->cl_ier	= 0x88;
#ifdef CL_DMA_WORKS
			{
			sc->cl_reg->cl_cmr	=
				/* CL_TXDMAINT | */ CL_RXDMAINT; 
			sc->cl_reg->cl_ier	= 0xa8;
			sc->cl_reg->cl_licr	= 0x00;
			}
			sc->cl_reg->cl_arbadrl	=
				((u_long)sc->sc_cl[channel].rxp[0]) & 0xffff;
			sc->cl_reg->cl_arbadru	=
				((u_long)sc->sc_cl[channel].rxp[0]) >> 16;
			sc->cl_reg->cl_brbadrl	=
				((u_long)sc->sc_cl[channel].rxp[1]) & 0xffff;
			sc->cl_reg->cl_brbadru	=
				((u_long)sc->sc_cl[channel].rxp[1]) >> 16;
			sc->cl_reg->cl_atbadrl	=
				((u_long)sc->sc_cl[channel].txp[0]) & 0xffff;
			sc->cl_reg->cl_atbadru	=
				((u_long)sc->sc_cl[channel].txp[0]) >> 16;
			sc->cl_reg->cl_btbadrl	=
				((u_long)sc->sc_cl[channel].txp[1]) & 0xffff;
			sc->cl_reg->cl_btbadru	=
				((u_long)sc->sc_cl[channel].txp[1]) >> 16;
			sc->cl_reg->cl_arbcnt	= CL_BUFSIZE;
			sc->cl_reg->cl_brbcnt	= CL_BUFSIZE;
			sc->cl_reg->cl_arbsts	= 0x01;
			sc->cl_reg->cl_brbsts	= 0x01;
if (channel == 2) { /* test one channel now */
			/* shift for tx DMA */
			/* no shift for rx DMA */
#if 0
			/* tx only */
			sc->cl_reg->cl_licr	= (CL_DMAMODE << 4);
			sc->cl_reg->cl_cmr	= 0x42; 
#endif
		/* rx only */
			sc->cl_reg->cl_licr	= 0x00;
			sc->cl_reg->cl_cmr	= 0x82; 
}
			sc->cl_reg->cl_ccr = 0x20;
			while (sc->cl_reg->cl_ccr != 0) {
			}
#endif /* CL_DMA_WORKS */
			sc->cl_reg->cl_car = save;
		}
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return(EBUSY);
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
	return((*linesw[tp->t_line].l_open)(dev, tp));
}
int clparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	int unit, channel;
	struct clsoftc *sc;
	int s;
	dev_t dev;

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
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
	struct clsoftc *sc;
	dev_t dev;
	u_char cl_obuffer[CLCDBUF+1];

	dev = tp->t_dev;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return;
	}
	channel = CL_CHANNEL(dev);

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = splcl();
	cc = tp->t_outq.c_cc;
	while (cc > 0) {
/*XXX*/
		cnt = min (CLCDBUF,cc);
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

int clclose (dev, flag, mode, p)
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
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	(*linesw[tp->t_line].l_close)(tp, flag);

	s = splcl();
	
	sc->cl_reg->cl_car = channel;
	if(cl->cl_consio == 0 && (tp->t_cflag & HUPCL) != 0) {
		sc->cl_reg->cl_msvr_rts = 0x00;
		sc->cl_reg->cl_msvr_dtr = 0x00;
		sc->cl_reg->cl_ccr = 0x05;
	}

	splx(s);
	ttyclose(tp);

#if 0
	cl->tty = NULL;
#endif
#ifdef DEBUG
	cl_dumpport(channel);
#endif

	return 0;
}
int clread (dev, uio, flag)
	dev_t dev;
	struct uio *uio;
int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return ENXIO;
	return((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
int clwrite (dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit, channel;
	struct tty *tp;
	struct cl_info *cl;
	struct clsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (!tp)
		return ENXIO;
	return((*linesw[tp->t_line].l_write)(tp, uio, flag));
}
int clioctl (dev, cmd, data, flag, p)
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
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
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
		error = suser(p->p_ucred, &p->p_acflag); 
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
clstop(tp, flag)
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

	return 1;
}

int
clcninit(cp)
struct consdev *cp;
{
#ifdef MAP_DOES_WORK
	int size = (0x1ff + PGOFSET) & ~PGOFSET;
	int pcc2_size = (0x3C + PGOFSET) & ~PGOFSET;
#endif
	struct clreg *cl_reg;
	
	cl_cons.cl_paddr = (void *)0xfff45000;
#ifdef MAP_DOES_WORK
	cl_cons.cl_vaddr = mapiodev(cl_cons.cl_paddr,size);
	cd_pcc2_base = mapiodev(0xfff42000,pcc2_size);
#else
	cl_cons.cl_vaddr   = (struct clreg *)IIOV(cl_cons.cl_paddr);
	cl_cons.pcctwoaddr = (void *)IIOV(0xfff42000);
#endif
	cl_reg = cl_cons.cl_vaddr;
	/* reset the chip? */
#ifdef CLCD_DO_RESET
#endif
	/* set up globals */
#ifdef NOT_ALREADY_SETUP
	cl_reg->cl_tftc  = 0x10;
	cl_reg->cl_tpr   = CL_TIMEOUT; /* is this correct?? */
	cl_reg->cl_rpilr = 0x03;
	cl_reg->cl_tpilr = 0x02;
	cl_reg->cl_mpilr = 0x01;

	/* set up the tty00 to be 9600 8N1 */
	cl_reg->cl_car   = 0x00;
	cl_reg->cl_cor1  = 0x17;	/* No parity, ignore parity, 8 bit char */
	cl_reg->cl_cor2  = 0x00;
	cl_reg->cl_cor3  = 0x02;	/* 1 stop bit */
	cl_reg->cl_cor4  = 0x00;
	cl_reg->cl_cor5  = 0x00;
	cl_reg->cl_cor6  = 0x00;
	cl_reg->cl_cor7  = 0x00;
	cl_reg->cl_schr1 = 0x00;
	cl_reg->cl_schr2 = 0x00;
	cl_reg->cl_schr3 = 0x00;
	cl_reg->cl_schr4 = 0x00;
	cl_reg->cl_scrl  = 0x00;
	cl_reg->cl_scrh  = 0x00;
	cl_reg->cl_lnxt  = 0x00;
	cl_reg->cl_cpsr  = 0x00;
#endif
	return 0;
}

int
cl_instat(sc)
	struct clsoftc *sc;
{
	struct clreg *cl_reg;
	if ( NULL == sc) {
		cl_reg = cl_cons.cl_vaddr;
	} else {
		cl_reg = sc->cl_reg;
	}
	return (cl_reg->cl_rir & 0x40);
}
int
clcngetc(dev)
	dev_t dev;
{
	u_char val, reoir, licr, isrl, data, status, fifo_cnt;
	int got_char = 0;
	u_char ier_old = 0xff;
	struct clreg *cl_reg = cl_cons.cl_vaddr;
	volatile struct pcctworeg *pcc2_base = cl_cons.pcctwoaddr;
	cl_reg->cl_car = 0;
	if (!(cl_reg->cl_ier & 0x08)) {
		ier_old = cl_reg->cl_ier;
		cl_reg->cl_ier	= 0x08;
	}
	while (got_char == 0) {
		val = cl_reg->cl_rir;
		/* if no receive interrupt pending wait */
		if (!(val & 0x80)) {
			continue;
		}
		/* XXX do we need to suck the entire FIFO contents? */
		reoir = pcc2_base->pcc2_sccrxiack; /* receive PIACK */
		licr = cl_reg->cl_licr;
		if (((licr >> 2) & 0x3) == 0) {
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
			data = cl_reg->cl_rdr;
			if (ier_old != 0xff) {
				cl_reg->cl_ier  = ier_old;
			}
			got_char = 1;
			cl_reg->cl_teoir = 0x00;
		} else {
			data = cl_reg->cl_rdr;
			cl_reg->cl_teoir = 0x00;
		}

	}
	
	return data;
}

int
clcnputc(dev, c)
	dev_t dev;
	u_char c;
{
	/* is this the correct location for the cr -> cr/lf tranlation? */
	if (c == '\n')
		clputc(0, 0, '\r');

	clputc(0, 0, c);
	return 0;
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
	return;
}
static void
clputc(sc, unit, c)
	struct clsoftc *sc;
	int unit;
	u_char c;
{
	int s;
	u_char schar;
	u_char oldchannel;
	struct clreg *cl_reg;
	if (0 == sc) {
		/* output on console */
		cl_reg = cl_cons.cl_vaddr;
	} else {
		cl_reg = sc->cl_reg;
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
	oldchannel = cl_reg->cl_car;
	cl_reg->cl_car = unit;
	schar = cl_reg->cl_schr3;
	cl_reg->cl_schr3 = c;
	cl_reg->cl_stcr = 0x08 | 0x03; /* send special char, char 3 */
	while (0 != cl_reg->cl_stcr) {
		/* wait until cl notices the command
		 * otherwise it may not notice the character
		 * if we send characters too fast.
		 */
	}
	DELAY(5);
	cl_reg->cl_schr3 = schar;
	cl_reg->cl_car = oldchannel;
	splx(s);
} else {
	s = splhigh();
	oldchannel = cl_reg->cl_car;
	cl_reg->cl_car = unit;
	if (cl_reg->cl_tftc > 0) {
		cl_reg->cl_tdr = c;
	}
	cl_reg->cl_car = oldchannel;
	splx(s);
}
#endif
	return;
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
	for (unit = 0; unit < cl_cd.cd_ndevs; unit++) {
		if (unit >= cl_cd.cd_ndevs || 
			(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
			continue;
		}
		if (cl_instat(sc)) {
			while (cl_instat(sc)){
				int ch;
				u_char c;
				/*
				*(pinchar++) = clcngetc();
				*/
				ch = clgetc(sc,&channel) & 0xff;
				c = ch;

				tp = sc->sc_cl[channel].tty;
				if (NULL != tp) {
					(*linesw[tp->t_line].l_rint)(c,tp);
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
	struct clreg *cl_reg;
	volatile struct pcctworeg *pcc2_base;
	u_char val, reoir, licr, isrl, fifo_cnt, data;
	if (0 == sc) {
		cl_reg = cl_cons.cl_vaddr;
		pcc2_base = cl_cons.pcctwoaddr;
	} else {
		cl_reg = sc->cl_reg;
		pcc2_base = sc->sc_pcctwo;
	}
	val = cl_reg->cl_rir;
	/* if no receive interrupt pending wait */
	if (!(val & 0x80)) {
		return 0;
	}
	/* XXX do we need to suck the entire FIFO contents? */
	reoir = pcc2_base->pcc2_sccrxiack; /* receive PIACK */
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
	struct clsoftc *sc;
	struct termios *par;
	int channel;
{
	u_int divisor, clk, clen;
	int s, imask, ints;

	s = splcl();
	sc->cl_reg->cl_car = channel;
	if (par->c_ospeed == 0) { 
		/* dont kill the console */
		if(sc->sc_cl[channel].cl_consio == 0) {
			/* disconnect, drop RTS DTR stop reciever */
			sc->cl_reg->cl_msvr_rts = 0x00;
			sc->cl_reg->cl_msvr_dtr = 0x00;
			sc->cl_reg->cl_ccr = 0x05;
		}
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
			while (sc->cl_reg->cl_ccr != 0) {
			}
		}
	}

	if (sc->sc_cl[channel].cl_consio == 0
		&& (par->c_cflag & CREAD) == 0 )
	{
		sc->cl_reg->cl_ccr = 0x08;
	} else {
		sc->cl_reg->cl_ccr = 0x0a;
	}
	while (sc->cl_reg->cl_ccr != 0) {
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
	if (cl_clocks[clknum].speed == speed) {
		return cl_clocks[clknum].divisor;
	}
	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].divisor;
		}
	}
	/* return some sane value if unknown speed */
	return cl_clocks[4].divisor;
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
	if (unit >= cl_cd.cd_ndevs || 
		(sc = (struct clsoftc *) cl_cd.cd_devs[unit]) == NULL) {
		return;
	}

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = splcl();
#if 0
	if (sc->sc_cl[channel].transmitting == 1) {
		/* i'm busy, go away, I will get to it later. */
		splx(s);
		return;
	}
	cnt = q_to_b(&tp->t_outq, &cbuf, 1);
	if ( cnt != 0 ) {
		sc->sc_cl[channel].transmitting = 1;
		sc->cl_reg->cl_car = channel;
		sc->cl_reg->cl_tdr = cbuf;
	} else {
		sc->sc_cl[channel].transmitting = 0;
	}
#else
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0)
	{
		tp->t_state |= TS_BUSY;
		sc->cl_reg->cl_car = channel;
		sc->cl_reg->cl_ier = sc->cl_reg->cl_ier | 0x3;
	}
#endif
	splx(s);
	return;
}
int
cl_mintr(sc)
	struct clsoftc *sc;
{
	u_char mir, misr, msvr;
	int channel;
	if(((mir = sc->cl_reg->cl_mir) & 0x40) == 0x0) {
		/* only if intr is not shared? */
		log(LOG_WARNING, "cl_mintr extra intr\n");
		return 0;
	}
	sc->sc_mxintrcnt.ev_count++;

	channel = mir & 0x03;
	misr = sc->cl_reg->cl_misr;
	msvr = sc->cl_reg->cl_msvr_rts;
	if (misr & 0x01) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 1 unexpected\n",channel);
	}
	if (misr & 0x02) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 2 unexpected\n",channel);
	}
	if (misr & 0x20) {
		struct tty *tp = sc->sc_cl[channel].tty;
#ifdef VERBOSE_LOG_MESSAGES
		log(LOG_WARNING, "cl_mintr: channel %x cts %x\n",channel, 
		((msvr & 0x20) != 0x0)
		);
#endif
		if (msvr & 0x20) {
			cl_unblock(tp);
		}
	}
	if (misr & 0x40) {
		struct tty *tp = sc->sc_cl[channel].tty;
#ifdef VERBOSE_LOG_MESSAGES
		log(LOG_WARNING, "cl_mintr: channel %x cd %x\n",channel,
		((msvr & 0x40) != 0x0)
		);
#endif
		ttymodem(tp, ((msvr & 0x40) != 0x0) );
	}
	if (misr & 0x80) {
#ifdef VERBOSE_LOG_MESSAGES
		log(LOG_WARNING, "cl_mintr: channel %x dsr %x\n",channel,
		((msvr & 0x80) != 0x0)
		);
#endif
	}
	sc->cl_reg->cl_meoir = 0x00;
	return 1;
}

int
cl_txintr(sc)
	struct clsoftc *sc;
{
	static empty = 0;
	u_char tir, cmr, teoir;
	u_char max;
	int channel;
	struct tty *tp;
	int cnt;
	u_char buffer[CL_FIFO_MAX +1];
	u_char *tptr;
	if(((tir = sc->cl_reg->cl_tir) & 0x40) == 0x0) {
		/* only if intr is not shared ??? */
		log(LOG_WARNING, "cl_txintr extra intr\n");
		return 0;
	}
	sc->sc_txintrcnt.ev_count++;

	channel	= tir & 0x03;
	cmr	= sc->cl_reg->cl_cmr;
	
	sc->sc_cl[channel].txcnt ++;

	tp = sc->sc_cl[channel].tty;
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0) {
		sc->cl_reg->cl_ier = sc->cl_reg->cl_ier & ~0x3;
		sc->cl_reg->cl_teoir = 0x08;
		return 1;
	}
	switch (cmr & CL_TXMASK) {
	case CL_TXDMAINT:
		{
			u_char dmabsts;
			int nbuf, busy, resid;
			void *pbuffer;
			dmabsts = sc->cl_reg->cl_dmabsts;
		log(LOG_WARNING, "cl_txintr: DMAMODE channel %x dmabsts %x\n",
			channel, dmabsts);
			nbuf = ((dmabsts & 0x8) >> 3) & 0x1;
			busy = ((dmabsts & 0x4) >> 2) & 0x1;

			do {
				pbuffer = sc->sc_cl[channel].tx[nbuf];
				resid = tp->t_outq.c_cc;
				cnt = min (CL_BUFSIZE,resid);
		log(LOG_WARNING, "cl_txintr: resid %x cnt %x pbuf %x\n",
			resid, cnt, pbuffer);
				if (cnt != 0) {
					cnt = q_to_b(&tp->t_outq, pbuffer, cnt);
					resid -= cnt;
					if (nbuf == 0) {
						sc->cl_reg->cl_atbadru =
							((u_long) sc->sc_cl[channel].txp[nbuf]) >> 16;
						sc->cl_reg->cl_atbadrl =
							((u_long) sc->sc_cl[channel].txp[nbuf]) & 0xffff;
						sc->cl_reg->cl_atbcnt = cnt;
						sc->cl_reg->cl_atbsts = 0x43;
					} else {
						sc->cl_reg->cl_btbadru =
							((u_long) sc->sc_cl[channel].txp[nbuf]) >> 16;
						sc->cl_reg->cl_btbadrl =
							((u_long) sc->sc_cl[channel].txp[nbuf]) & 0xffff;
						sc->cl_reg->cl_btbcnt = cnt;
						sc->cl_reg->cl_btbsts = 0x43;
					}
				teoir = 0x08;
				} else {
					teoir = 0x08;
					if (tp->t_state & TS_BUSY) {
						tp->t_state &= ~(TS_BUSY | TS_FLUSH);
						if (tp->t_state & TS_ASLEEP) {
							tp->t_state &= ~TS_ASLEEP;
							wakeup((caddr_t) &tp->t_outq);
						}
						selwakeup(&tp->t_wsel);
					}
					sc->cl_reg->cl_ier = sc->cl_reg->cl_ier & ~0x3;
				}
				nbuf = ~nbuf & 0x1;
				busy--;
			} while (resid != 0 && busy != -1);/* if not busy do other buffer */
		log(LOG_WARNING, "cl_txintr: done\n");
		}
		break;
	case CL_TXINTR:
		max = sc->cl_reg->cl_tftc;
		cnt = min ((int)max,tp->t_outq.c_cc);
		if (cnt != 0) {
			cnt = q_to_b(&tp->t_outq, buffer, cnt);
			empty = 0;
			for (tptr = buffer; tptr < &buffer[cnt]; tptr++) {
				sc->cl_reg->cl_tdr = *tptr;
			}
			teoir = 0x00;
		} else {
			if (empty > 5 && ((empty % 20000 )== 0)) {
			log(LOG_WARNING, "cl_txintr to many empty intr %d channel %d\n",
				empty, channel);
			}
			empty++;
			teoir = 0x08;
			if (tp->t_state & TS_BUSY) {
				tp->t_state &= ~(TS_BUSY | TS_FLUSH);
				if (tp->t_state & TS_ASLEEP) {
					tp->t_state &= ~TS_ASLEEP;
					wakeup((caddr_t) &tp->t_outq);
				}
				selwakeup(&tp->t_wsel);
			}
			sc->cl_reg->cl_ier = sc->cl_reg->cl_ier & ~0x3;
		}
		break;
	default:
		log(LOG_WARNING, "cl_txintr unknown mode %x\n", cmr);
		/* we probably will go to hell quickly now */
		teoir = 0x08;
	}
	sc->cl_reg->cl_teoir = teoir;
	return 1;
}

int
cl_rxintr(sc)
	struct clsoftc *sc;
{
	u_char rir, channel, cmr, risrl;
	u_char c;
	u_char fifocnt;
	struct tty *tp;
	int i;
	u_char reoir;
	u_char buffer[CL_FIFO_MAX +1];
#ifdef CONSOLEBREAKDDB
	int wantddb = 0;
#endif
	
	rir = sc->cl_reg->cl_rir;
	if((rir & 0x40) == 0x0) {
		/* only if intr is not shared ??? */
		log(LOG_WARNING, "cl_rxintr extra intr\n");
		return 0;
	}
	sc->sc_rxintrcnt.ev_count++;
	channel = rir & 0x3;
	cmr = sc->cl_reg->cl_cmr;
	reoir = 0x08;

	sc->sc_cl[channel].rxcnt ++;
	risrl = sc->cl_reg->cl_risrl;
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
#ifdef CONSOLEBREAKDDB
		if (sc->sc_cl[channel].cl_consio)
			wantddb = 1;
#endif
		cl_break(sc, channel);
		reoir = 0x08;
	}

	switch (cmr & CL_RXMASK) {
	case CL_RXDMAINT:
		{
			int nbuf;
			u_short cnt;
			int bufcomplete;
			u_char status, dmabsts;
			u_char risrh = sc->cl_reg->cl_risrh;
			dmabsts = sc->cl_reg->cl_dmabsts;
#ifdef DMA_DEBUG
log(LOG_WARNING, "cl_txintr: DMAMODE channel %x dmabsts %x risrl %x risrh %x\n",
	channel, dmabsts, risrl, risrh);
#endif
			nbuf = (risrh & 0x08) ? 1 : 0;
			bufcomplete = (risrh & 0x20) ? 1 : 0;
			if (nbuf == 0) {
				cnt  = sc->cl_reg->cl_arbcnt;
				status =  sc->cl_reg->cl_arbsts;
			} else {
				cnt  = sc->cl_reg->cl_brbcnt;
				status =  sc->cl_reg->cl_brbsts;
			}
#ifdef DMA_DEBUG
log(LOG_WARNING, "cl_rxintr: 1channel %x buf %x cnt %x status %x\n",
channel, nbuf, cnt, status);
#endif
#ifdef USE_BUFFER
			cl_appendbufn(sc, channel,
				sc->sc_cl[channel].rx[nbuf], cnt);
#else 
			{
				int i;
				u_char *pbuf;
				tp = sc->sc_cl[channel].tty;
				pbuf = sc->sc_cl[channel].rx[nbuf];
			/* this should be done at off level */
{
	u_short rcbadru, rcbadrl;
	u_char arbsts, brbsts;
	u_char *pbufs, *pbufe;
	rcbadru = sc->cl_reg->cl_rcbadru;
	rcbadrl = sc->cl_reg->cl_rcbadrl;
	arbsts =  sc->cl_reg->cl_arbsts;
	brbsts =  sc->cl_reg->cl_brbsts;
	pbufs = sc->sc_cl[channel].rxp[nbuf];
	pbufe = (u_char *)(((u_long)rcbadru << 16) | (u_long)rcbadrl);
	cnt = pbufe - pbufs;
#ifdef DMA_DEBUG
	log(LOG_WARNING, "cl_rxintr: rcbadru %x rcbadrl %x arbsts %x brbsts %x cnt %x\n",
	rcbadru, rcbadrl, arbsts, brbsts, cnt);
#endif
#ifdef DMA_DEBUG1
	log(LOG_WARNING, "cl_rxintr: buf %x cnt %x\n",
	nbuf, cnt);
#endif
}
				reoir = 0x0 | (bufcomplete) ? 0 : 0xd0;
				sc->cl_reg->cl_reoir = reoir;
#ifdef DMA_DEBUG
log(LOG_WARNING, "cl_rxintr: reoir %x\n", reoir);
#endif
				delay(10); /* give the chip a moment */
#ifdef DMA_DEBUG
log(LOG_WARNING, "cl_rxintr: 2channel %x buf %x cnt %x status %x\n",
channel, nbuf, cnt, status);
#endif
				for (i = 0; i < cnt; i++) {
					u_char c;
					c = pbuf[i];
					(*linesw[tp->t_line].l_rint)(c,tp);
				}
			/* this should be done at off level */
				if (nbuf == 0) {
					sc->cl_reg->cl_arbcnt = CL_BUFSIZE;
					sc->cl_reg->cl_arbsts = 0x01;
				} else {
					sc->cl_reg->cl_brbcnt = CL_BUFSIZE;
					sc->cl_reg->cl_brbsts = 0x01;
				}
			}
#endif
		}
		sc->cl_reg->cl_reoir = reoir;
		break;
	case CL_RXINTR:
		fifocnt = sc->cl_reg->cl_rfoc;
		tp = sc->sc_cl[channel].tty;
		for (i = 0; i < fifocnt; i++) {
			buffer[i] = sc->cl_reg->cl_rdr;
		}
		if (NULL == tp) {
			/* if the channel is not configured,
			 * dont send characters upstream.
			 * also fix problem with NULL dereference
			 */
			reoir = 0x00;
			break;
		}

		sc->cl_reg->cl_reoir = reoir;
#ifdef USE_BUFFER
		cl_appendbufn(sc, channel, buffer, fifocnt);
#else
		for (i = 0; i < fifocnt; i++) {
			u_char c;
			c = buffer[i];
			/* does any restricitions exist on spl
			 * for this call
			 */
			(*linesw[tp->t_line].l_rint)(c,tp);
			reoir = 0x00;
		}
#endif
		break;
	default:
		log(LOG_WARNING, "cl_rxintr unknown mode %x\n", cmr);
		/* we probably will go to hell quickly now */
		reoir = 0x08;
		sc->cl_reg->cl_reoir = reoir;
	}
#ifdef CONSOLEBREAKDDB
	if (wantddb)
		Debugger();
#endif
	return 1;
}

void
cl_overflow (sc, channel, ptime, msg)
struct clsoftc *sc;
int channel;
time_t *ptime;
u_char *msg;
{
/*
	if (*ptime != time.tv_sec) {
*/
	{
/*
		*ptime = time.tv_sec;
*/
		log(LOG_WARNING, "%s%d[%d]: %s overrun\n", cl_cd.cd_name,
			0 /* fix */, channel, msg);
	}
	return;
}
void
cl_parity (sc, channel)
	struct clsoftc *sc;
	int channel;
{
#ifdef VERBOSE_LOG_MESSAGES
	log(LOG_WARNING, "%s%d[%d]: parity error\n", cl_cd.cd_name, 0, channel);
#endif
	return;
}
void
cl_frame (sc, channel)
	struct clsoftc *sc;
	int channel;
{
#ifdef VERBOSE_LOG_MESSAGES
	log(LOG_WARNING, "%s%d[%d]: frame error\n", cl_cd.cd_name, 0, channel);
#endif
	return;
}
void
cl_break (sc, channel)
	struct clsoftc *sc;
	int channel;
{
#ifdef VERBOSE_LOG_MESSAGES
	log(LOG_WARNING, "%s%d[%d]: break detected\n", cl_cd.cd_name, 0, channel);
#endif
	return;
}

void
cl_dumpport0()
{
	cl_dumpport(0);
	return;
}
void
cl_dumpport1()
{
	cl_dumpport(1);
	return;
}
void
cl_dumpport2()
{
	cl_dumpport(2);
	return;
}
void
cl_dumpport3()
{
	cl_dumpport(3);
	return;
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
	struct clsoftc *sc;

	struct clreg *cl_reg;
	int s;

	cl_reg = cl_cons.cl_vaddr;

	sc = (struct clsoftc *) cl_cd.cd_devs[0];

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
	return;
}
#ifdef USE_BUFFER
void
cl_appendbuf(sc, channel, c)
	struct clsoftc *sc;
	u_char channel;
	u_char c;
{
	int s;
	/* s = splcl(); */
	if (1 + sc->sc_cl[channel].nchar >= CL_BUFSIZE ) {
		cl_overflow (sc, channel, &sc->sc_fotime, "rbuf");
		/* just toss the character */
		return;
	}
	*(sc->sc_cl[channel].psupply++) = c;
	if (&(sc->sc_cl[channel].buffer[CL_BUFSIZE])
	   == sc->sc_cl[channel].psupply) {
		sc->sc_cl[channel].psupply = sc->sc_cl[channel].buffer;
	}
	sc->sc_cl[channel].nchar ++;
	setsoftint(sc->ssir);
	/* splx (s); */
	
}
void
cl_appendbufn(sc, channel, buf, cnt)
	struct clsoftc *sc;
	u_char channel;
	u_char *buf;
	u_short cnt;
{
	int s;
	int i;
	/* s = splcl(); */ /* should be called at splcl(). */
	if (cnt + sc->sc_cl[channel].nchar >= CL_BUFSIZE ) {
		cl_overflow (sc, channel, &sc->sc_fotime, "rbuf");
		/* just toss the character(s)
		 *  It could be argued that not all of the charactes
		 *  should be tossed, just the ones that actually
		 *  overflow the buffer. eh, O well.
		 */
		return;
	}
	for (i = 0; i < cnt; i++) {
		*(sc->sc_cl[channel].psupply++) = buf[i];
		if (&(sc->sc_cl[channel].buffer[CL_BUFSIZE]) == sc->sc_cl[channel].psupply)
		{
			sc->sc_cl[channel].psupply = sc->sc_cl[channel].buffer;
		}
		sc->sc_cl[channel].nchar ++;
	}
	setsoftint(sc->ssir);
	/* splx (s); */
}

void
cl_softint(sc)
	struct clsoftc *sc;
{
	int i;
	int s;
	u_char c;
	struct tty *tp;

	for (i = 0 ; i < CLCD_PORTS_PER_CHIP; i ++) {
/* printf("channel %x sc %x\n", i, sc); */ 
		tp = sc->sc_cl[i].tty;
/* printf("channel %x pconsum %x\n", i, sc->sc_cl[i].pconsum); */ 
		while (sc->sc_cl[i].nchar > 0) {
			s = splcl();
			c = *(sc->sc_cl[i].pconsum++);
			if (&(sc->sc_cl[i].buffer[CL_BUFSIZE]) == sc->sc_cl[i].pconsum)
			{
				sc->sc_cl[i].pconsum = sc->sc_cl[i].buffer;
			}
			sc->sc_cl[i].nchar--;
			splx(s);
			(*linesw[tp->t_line].l_rint)(c,tp);
		}
	}
}
#endif
