/*	$OpenBSD: cl.c,v 1.49 2005/01/25 12:13:22 miod Exp $ */

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

/* DMA mode still does not work!!! */

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
#include <machine/locore.h>
#include <machine/psl.h>

#include <dev/cons.h>

#include <mvme88k/dev/clreg.h>
#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/pcctwovar.h>

#ifdef	DDB
#include <ddb/db_var.h>
#endif

#define splcl()	spltty()

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
	bus_space_tag_t		cl_iot;
	bus_space_handle_t	cl_ioh;
	volatile u_int8_t	*cl_rxiack;
	u_int8_t		channel;
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
#if 0
	u_char  transmitting;
#endif
	u_long  txcnt;
	u_long  rxcnt;

	void *rx[2];
	void *rxp[2];
	void *tx[2];
	void *txp[2];
};
#define CLCD_PORTS_PER_CHIP 4
#define CL_BUFSIZE 256

#ifndef DO_MALLOC
/* four (4) buffers per port */
char cl_dmabuf[CLCD_PORTS_PER_CHIP * CL_BUFSIZE * 4];
char cl_dmabuf1[CLCD_PORTS_PER_CHIP * CL_BUFSIZE * 4];
#endif

struct clsoftc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	time_t			sc_fotime;	/* time of last fifo overrun */
	struct cl_info		sc_cl[CLCD_PORTS_PER_CHIP];
	struct intrhand		sc_ih_e;
	struct intrhand		sc_ih_m;
	struct intrhand		sc_ih_t;
	struct intrhand		sc_ih_r;
	char			sc_errintrname[16 + 4];
	char			sc_mxintrname[16 + 3];
	char			sc_rxintrname[16 + 3];
	char			sc_txintrname[16 + 3];
	struct pcctwosoftc	*sc_pcctwo;
};

const struct {
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

#define	CL_SAFE_CLOCK	4	/* 9600 entry */

/* prototypes */
cons_decl(cl);
int cl_instat(struct clsoftc *sc);
u_int8_t cl_clkdiv(int speed);
u_int8_t cl_clknum(int speed);
u_int8_t cl_clkrxtimeout(int speed);
void clstart(struct tty *tp);
void cl_unblock(struct tty *tp);
int clccparam(struct clsoftc *sc, struct termios *par, int channel);

int clparam(struct tty *tp, struct termios *t);
int cl_mintr(void *);
int cl_txintr(void *);
int cl_rxintr(void *);
void cl_overflow(struct clsoftc *sc, int channel, long *ptime, char *msg);
void cl_parity(struct clsoftc *sc, int channel);
void cl_frame(struct clsoftc *sc, int channel);
void cl_break( struct clsoftc *sc, int channel);
int clmctl(dev_t dev, int bits, int how);
#ifdef DEBUG
void cl_dumpport(struct clsoftc *, int);
#endif

int	clprobe(struct device *parent, void *self, void *aux);
void	clattach(struct device *parent, struct device *self, void *aux);

void	cl_initchannel(struct clsoftc *sc, int channel);
void	clputc(struct clsoftc *sc, int unit, u_char c);

struct cfattach cl_ca = {
	sizeof(struct clsoftc), clprobe, clattach
};

struct cfdriver cl_cd = {
	NULL, "cl", DV_TTY
};

#if 0
#define CLCDBUF 80
void cloutput(struct tty *tp);
#endif

#define CL_UNIT(x) (minor(x) >> 2)
#define CL_CHANNEL(x) (minor(x) & 3)

struct tty *
cltty(dev)
	dev_t dev;
{
	int unit, channel;
	struct clsoftc *sc;

	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return NULL;
	}
	channel = CL_CHANNEL(dev);
	return sc->sc_cl[channel].tty;
}

int
clprobe(parent, self, aux)
	struct device *parent;
	void *self;
	void *aux;
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rc;

	if (brdtyp == BRD_188)
		return 0;

	/*
	 * We do not accept empty locators here...
	 */
	if (ca->ca_paddr == CD2400_BASE_ADDR ||
	    (ca->ca_paddr == CD2400_SECONDARY_ADDR && brdtyp == BRD_8120)) {
		if (bus_space_map(ca->ca_iot, ca->ca_paddr, CD2400_SIZE,
		    0, &ioh) != 0)
			return 0;
		rc = badvaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 1);
		bus_space_unmap(ca->ca_iot, ca->ca_paddr, CD2400_SIZE);
		return rc == 0;
	}

	return 0;
}

void
clattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clsoftc *sc = (struct clsoftc *)self;
	struct confargs *ca = aux;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int i;

	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	iot = sc->sc_iot = ca->ca_iot;
	if (bus_space_map(iot, ca->ca_paddr, CD2400_SIZE, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}
	sc->sc_ioh = ioh;
	sc->sc_pcctwo = (struct pcctwosoftc *)parent;

	if (ca->ca_paddr == CD2400_BASE_ADDR) {
		/*
		 * Although we are still running using the BUG routines,
		 * this device will be elected as the console after
		 * autoconf. Mark it as such.
		 */
		sc->sc_cl[0].cl_consio = 1;
		printf(": console");
	} else {
		/* reset chip only if we are not console device */
		/* wait for GFRCR */
	}
	/* allow chip to settle before continuing */
	delay(800);

	/* set up global registers */
	bus_space_write_1(iot, ioh, CL_TPR, CL_TIMEOUT);
	bus_space_write_1(iot, ioh, CL_RPILR, 0x03);
	bus_space_write_1(iot, ioh, CL_TPILR, 0x02);
	bus_space_write_1(iot, ioh, CL_MPILR, 0x01);

#ifdef DO_MALLOC
	sc->sc_cl[0].rx[0] = (void *)(dvma_malloc(16 * CL_BUFSIZE));
#else
	/* XXX */
	if ((vaddr_t)ca->ca_paddr == CD2400_BASE_ADDR)
		sc->sc_cl[0].rx[0] = (void *)(&cl_dmabuf);
	else
		sc->sc_cl[0].rx[0] = (void *)(&cl_dmabuf1);
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
	for (i = 0; i < CLCD_PORTS_PER_CHIP; i++) {
#if 0
		int j;

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

	/* clear errors */
	bus_space_write_1(sc->sc_pcctwo->sc_iot, sc->sc_pcctwo->sc_ioh,
	    PCCTWO_SCCERR, 0x01);

	/* enable interrupts */
	sc->sc_ih_e.ih_fn = cl_rxintr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_wantframe = 0;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;

	sc->sc_ih_m.ih_fn = cl_mintr;
	sc->sc_ih_m.ih_arg = sc;
	sc->sc_ih_m.ih_wantframe = 0;
	sc->sc_ih_m.ih_ipl = ca->ca_ipl;

	sc->sc_ih_t.ih_fn = cl_txintr;
	sc->sc_ih_t.ih_arg = sc;
	sc->sc_ih_t.ih_wantframe = 0;
	sc->sc_ih_t.ih_ipl = ca->ca_ipl;

	sc->sc_ih_r.ih_fn = cl_rxintr;
	sc->sc_ih_r.ih_arg = sc;
	sc->sc_ih_r.ih_wantframe = 0;
	sc->sc_ih_r.ih_ipl = ca->ca_ipl;

	snprintf(sc->sc_errintrname, sizeof sc->sc_errintrname,
	    "%s_err", self->dv_xname);
	snprintf(sc->sc_mxintrname, sizeof sc->sc_mxintrname,
	    "%s_mx", self->dv_xname);
	snprintf(sc->sc_rxintrname, sizeof sc->sc_rxintrname,
	    "%s_rx", self->dv_xname);
	snprintf(sc->sc_txintrname, sizeof sc->sc_txintrname,
	    "%s_tx", self->dv_xname);

	pcctwointr_establish(PCC2V_SCC_RXE, &sc->sc_ih_e, sc->sc_errintrname);
	pcctwointr_establish(PCC2V_SCC_M, &sc->sc_ih_m, sc->sc_mxintrname);
	pcctwointr_establish(PCC2V_SCC_TX, &sc->sc_ih_t, sc->sc_txintrname);
	pcctwointr_establish(PCC2V_SCC_RX, &sc->sc_ih_r, sc->sc_rxintrname);

	bus_space_write_1(sc->sc_pcctwo->sc_iot, sc->sc_pcctwo->sc_ioh,
	    PCCTWO_SCCICR, PCC2_IRQ_IEN | (ca->ca_ipl & PCC2_IRQ_IPL));
	bus_space_write_1(sc->sc_pcctwo->sc_iot, sc->sc_pcctwo->sc_ioh,
	    PCCTWO_SCCTX, PCC2_IRQ_IEN | (ca->ca_ipl & PCC2_IRQ_IPL));
	bus_space_write_1(sc->sc_pcctwo->sc_iot, sc->sc_pcctwo->sc_ioh,
	    PCCTWO_SCCRX, PCC2_IRQ_IEN | (ca->ca_ipl & PCC2_IRQ_IPL));

	printf("\n");
}

void
cl_initchannel(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	int s;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	/* set up option registers */
	sc->sc_cl[channel].tty = NULL;
	s = splhigh();

	bus_space_write_1(iot, ioh, CL_CAR, channel);
	bus_space_write_1(iot, ioh, CL_LIVR, PCC2_VECT + PCC2V_SCC_RXE);
	bus_space_write_1(iot, ioh, CL_IER, 0);

	if (sc->sc_cl[channel].cl_consio == 0) {
		bus_space_write_1(iot, ioh, CL_CMR, 0x02);
		bus_space_write_1(iot, ioh, CL_COR1, 0x17);
		bus_space_write_1(iot, ioh, CL_COR2, 0x00);
		bus_space_write_1(iot, ioh, CL_COR3, 0x02);
		bus_space_write_1(iot, ioh, CL_COR4, 0xec);
		bus_space_write_1(iot, ioh, CL_COR5, 0xec);
		bus_space_write_1(iot, ioh, CL_COR6, 0x00);
		bus_space_write_1(iot, ioh, CL_COR7, 0x00);
		bus_space_write_1(iot, ioh, CL_SCHR1, 0x00);
		bus_space_write_1(iot, ioh, CL_SCHR2, 0x00);
		bus_space_write_1(iot, ioh, CL_SCHR3, 0x00);
		bus_space_write_1(iot, ioh, CL_SCHR4, 0x00);
		bus_space_write_1(iot, ioh, CL_SCRL, 0x00);
		bus_space_write_1(iot, ioh, CL_SCRH, 0x00);
		bus_space_write_1(iot, ioh, CL_LNXT, 0x00);
		bus_space_write_1(iot, ioh, CL_RBPR, 0x40);	/* 9600 */
		bus_space_write_1(iot, ioh, CL_RCOR, 0x01);
		bus_space_write_1(iot, ioh, CL_TBPR, 0x40);	/* 9600 */
		bus_space_write_1(iot, ioh, CL_TCOR, 0x01 << 5);
		/* console port should be 0x88 already */
		bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x00);
		bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x00);
		bus_space_write_1(iot, ioh, CL_RTPRL, CL_RX_TIMEOUT);
		bus_space_write_1(iot, ioh, CL_RTPRH, 0x00);
	}
	bus_space_write_1(iot, ioh, CL_CCR, 0x20);
	while (bus_space_read_1(iot, ioh, CL_CCR) != 0)
		;

	splx(s);
}


int cldefaultrate = TTYDEF_SPEED;

int
clmctl(dev, bits, how)
	dev_t dev;
	int bits;
	int how;
{
	struct clsoftc *sc;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int s;

	/* should only be called with valid device */
	sc = (struct clsoftc *)cl_cd.cd_devs[CL_UNIT(dev)];
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	/* settings are currently ignored */
	s = splcl();
	switch (how) {
	case DMSET:
		if (bits & TIOCM_RTS)
			bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x01);
		else
			bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x00);
		if (bits & TIOCM_DTR)
			bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x02);
		else
			bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x00);
		break;

	case DMBIC:
		if (bits & TIOCM_RTS)
			bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x00);
		if (bits & TIOCM_DTR)
			bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x00);
		break;

	case DMBIS:
		if (bits & TIOCM_RTS)
			bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x01);
		if (bits & TIOCM_DTR)
			bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x02);
		break;

	case DMGET:
		bits = 0;

		{
			u_int8_t msvr;

			msvr = bus_space_read_1(iot, ioh, CL_MSVR_RTS);
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

	return bits;
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
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
	}

	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];

	s = splcl();
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
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = cldefaultrate;

			if (sc->sc_cl[channel].cl_consio != 0) {
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
			u_int8_t save;

			save = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CL_CAR);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_CAR,
			    channel);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_IER, 0x88);
			bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_CAR,
			    save);
		}
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return EBUSY;
	}
	splx(s);

	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
#ifdef DEBUG
	cl_dumpport(sc, channel);
#endif
	return (*linesw[tp->t_line].l_open)(dev, tp);
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
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
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

#if 0
void
cloutput(tp)
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
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
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
#endif

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
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int s;

	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	tp = cl->tty;
	(*linesw[tp->t_line].l_close)(tp, flag);

	s = splcl();
	bus_space_write_1(iot, ioh, CL_CAR, channel);
	if (cl->cl_consio == 0 && (tp->t_cflag & HUPCL) != 0) {
		bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x00);
		bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x00);
		bus_space_write_1(iot, ioh, CL_CCR, 0x05);
	}


	splx(s);
	ttyclose(tp);

#if 0
	cl->tty = NULL;
#endif
#ifdef DEBUG
	cl_dumpport(sc, channel);
#endif

	return 0;
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
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (tp == NULL)
		return ENXIO;
	return (*linesw[tp->t_line].l_read)(tp, uio, flag);
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
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (tp == NULL)
		return ENXIO;
	return (*linesw[tp->t_line].l_write)(tp, uio, flag);
}

int
clioctl(dev, cmd, data, flag, p)
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
	struct clsoftc *sc;
	unit = CL_UNIT(dev);
	if (unit >= cl_cd.cd_ndevs ||
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
		return ENODEV;
	}
	channel = CL_CHANNEL(dev);
	cl = &sc->sc_cl[channel];
	tp = cl->tty;
	if (tp == NULL)
		return ENXIO;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return error;

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
			return EPERM;

		cl->cl_swflags = *(int *)data;
		cl->cl_swflags &= /* only allow valid flags */
			(TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;
	default:
		return ENOTTY;
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

void
clcnprobe(cp)
	struct consdev *cp;
{
	int maj;

	cp->cn_pri = CN_DEAD;

	/* bomb if it'a a MVME188 */
	if (brdtyp == BRD_188 || badaddr(CD2400_BASE_ADDR, 1) != 0)
		return;

	/* do not attach as console if cl has been disabled */
	if (cl_cd.cd_ndevs == 0 || cl_cd.cd_devs[0] == NULL)
		return;

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == clopen)
			break;
	if (maj == nchrdev)
		return;

	cp->cn_dev = makedev(maj, 0);
	cp->cn_pri = CN_NORMAL;
}

void
clcninit(cp)
	struct consdev *cp;
{
	struct clsoftc *sc;

	sc = (struct clsoftc *)cl_cd.cd_devs[0];
	cl_cons.cl_iot = sc->sc_iot;
	cl_cons.cl_ioh = sc->sc_ioh;
	cl_cons.cl_rxiack = (void *)(sc->sc_pcctwo->sc_base + PCCTWO_SCCRXIACK);
}

int
cl_instat(sc)
	struct clsoftc *sc;
{
	u_int8_t rir;

	if (sc == NULL)
		rir = bus_space_read_1(cl_cons.cl_iot, cl_cons.cl_ioh, CL_RIR);
	else
		rir = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CL_RIR);

	return (rir & 0x40);
}

int
clcngetc(dev)
	dev_t dev;
{
	u_int8_t val, reoir, licr, data;
	int got_char = 0;
	u_int8_t ier_old;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	iot = cl_cons.cl_iot;
	ioh = cl_cons.cl_ioh;

	bus_space_write_1(iot, ioh, CL_CAR, 0);
	ier_old = bus_space_read_1(iot, ioh, CL_IER);
	if ((ier_old & 0x08) == 0) {
		bus_space_write_1(iot, ioh, CL_IER, 0x08);
	} else
		ier_old = 0xff;

	while (got_char == 0) {
		val = bus_space_read_1(iot, ioh, CL_RIR);
		/* if no receive interrupt pending wait */
		if ((val & 0x80) == 0)
			continue;

		/* XXX do we need to suck the entire FIFO contents? */
		reoir = *cl_cons.cl_rxiack; /* receive PIACK */
		licr = bus_space_read_1(iot, ioh, CL_LICR);
		/* is the interrupt for us? (port 0) */
		if (((licr >> 2) & 0x3) == 0) {
			(void)bus_space_read_1(iot, ioh, CL_RISRL);
			(void)bus_space_read_1(iot, ioh, CL_RFOC);
			data = bus_space_read_1(iot, ioh, CL_RDR);
			if (ier_old != 0xff)
				bus_space_write_1(iot, ioh, CL_IER, ier_old);
			got_char = 1;
		} else {
			/* read and discard the character */
			data = bus_space_read_1(iot, ioh, CL_RDR);
		}
		bus_space_write_1(iot, ioh, CL_TEOIR, 0x00);
	}

	return data;
}

void
clcnputc(dev, c)
	dev_t dev;
	u_char c;
{
	clputc(0, 0, c);
}

void
clcnpollc(dev, on)
	dev_t dev;
	int on;
{
	if (on != 0) {
		/* enable polling */
	} else {
		/* disable polling */
	}
}

void
clputc(sc, unit, c)
	struct clsoftc *sc;
	int unit;
	u_char c;
{
	u_int8_t schar;
	u_int8_t oldchannel;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	int s;

	if (sc == NULL) {
		/* output on console */
		iot = cl_cons.cl_iot;
		ioh = cl_cons.cl_ioh;
	} else {
		iot = sc->sc_iot;
		ioh = sc->sc_ioh;
	}

	s = splhigh();
	oldchannel = bus_space_read_1(iot, ioh, CL_CAR);
	bus_space_write_1(iot, ioh, CL_CAR, unit);
	if (unit == 0) {
		schar = bus_space_read_1(iot, ioh, CL_SCHR3);
		/* send special char, number 3 */
		bus_space_write_1(iot, ioh, CL_SCHR3, c);
		bus_space_write_1(iot, ioh, CL_STCR, 0x08 | 3);
		while (bus_space_read_1(iot, ioh, CL_STCR) != 0) {
			/* wait until cl notices the command
			 * otherwise it may not notice the character
			 * if we send characters too fast.
			 */
		}
		DELAY(5);
		bus_space_write_1(iot, ioh, CL_SCHR3, schar);
	} else {
		if (bus_space_read_1(iot, ioh, CL_TFTC) != 0)
			bus_space_write_1(iot, ioh, CL_TDR, c);
	}
	bus_space_write_1(iot, ioh, CL_CAR, oldchannel);
	splx(s);
}

int
clccparam(sc, par, channel)
	struct clsoftc *sc;
	struct termios *par;
	int channel;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int divisor, clk, clen;
	int s, imask, ints;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	s = splcl();
	bus_space_write_1(iot, ioh, CL_CAR, channel);
	if (par->c_ospeed == 0) {
		/* dont kill the console */
		if (sc->sc_cl[channel].cl_consio == 0) {
			/* disconnect, drop RTS DTR stop receiver */
			bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x00);
			bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x00);
			bus_space_write_1(iot, ioh, CL_CCR, 0x05);
		}
		splx(s);
		return 0xff;
	}

	bus_space_write_1(iot, ioh, CL_MSVR_RTS, 0x03);
	bus_space_write_1(iot, ioh, CL_MSVR_DTR, 0x03);

	divisor = cl_clkdiv(par->c_ospeed);
	clk	= cl_clknum(par->c_ospeed);
	bus_space_write_1(iot, ioh, CL_TBPR, divisor);
	bus_space_write_1(iot, ioh, CL_TCOR, clk << 5);
	divisor = cl_clkdiv(par->c_ispeed);
	clk	= cl_clknum(par->c_ispeed);
	bus_space_write_1(iot, ioh, CL_RBPR, divisor);
	bus_space_write_1(iot, ioh, CL_RCOR, clk);
	bus_space_write_1(iot, ioh, CL_RTPRL, cl_clkrxtimeout(par->c_ispeed));
	bus_space_write_1(iot, ioh, CL_RTPRH, 0x00);

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

	bus_space_write_1(iot, ioh, CL_COR3, par->c_cflag & PARENB ? 4 : 2);

	{
		u_int8_t cor1;
		if (par->c_cflag & PARENB) {
			if (par->c_cflag & PARODD) {
				cor1 = 0xE0 | clen ; /* odd */
			} else {
				cor1 = 0x40 | clen ; /* even */
			}
		} else {
			cor1 = 0x10 | clen; /* ignore parity */
		}

		if (bus_space_read_1(iot, ioh, CL_COR1) != cor1) {
			bus_space_write_1(iot, ioh, CL_COR1, cor1);
			bus_space_write_1(iot, ioh, CL_CCR, 0x20);
			while (bus_space_read_1(iot, ioh, CL_CCR) != 0)
				;
		}
	}

	if (sc->sc_cl[channel].cl_consio == 0 && (par->c_cflag & CREAD) == 0)
		bus_space_write_1(iot, ioh, CL_CCR, 0x08);
	else
		bus_space_write_1(iot, ioh, CL_CCR, 0x0a);

	while (bus_space_read_1(iot, ioh, CL_CCR) != 0)
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
	bus_space_write_1(iot, ioh, CL_COR4, ints | CL_FIFO_CNT);
	bus_space_write_1(iot, ioh, CL_COR5, ints | CL_FIFO_CNT);

	splx(s);

	return imask;
}

static int clknum = 0;

u_int8_t
cl_clkdiv(speed)
	int speed;
{
	int i;

	if (cl_clocks[clknum].speed == speed)
		return cl_clocks[clknum].divisor;

	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].divisor;
		}
	}

	/* return some sane value if unknown speed */
	return cl_clocks[CL_SAFE_CLOCK].divisor;
}

u_int8_t
cl_clknum(speed)
	int speed;
{
	int i;

	if (cl_clocks[clknum].speed == speed)
		return cl_clocks[clknum].clock;

	for (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[clknum].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].clock;
		}
	}

	/* return some sane value if unknown speed */
	return cl_clocks[CL_SAFE_CLOCK].clock;
}

u_int8_t
cl_clkrxtimeout(speed)
	int speed;
{
	int i;

	if (cl_clocks[clknum].speed == speed)
		return cl_clocks[clknum].rx_timeout;

	for  (i = 0; cl_clocks[i].speed != 0; i++) {
		if (cl_clocks[i].speed == speed) {
			clknum = i;
			return cl_clocks[clknum].rx_timeout;
		}
	}

	/* return some sane value if unknown speed */
	return cl_clocks[CL_SAFE_CLOCK].rx_timeout;
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
	struct clsoftc *sc;
	int channel, unit, s;
#if 0
	int cnt;
	u_int8_t cbuf;
#endif

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
	    (sc = (struct clsoftc *)cl_cd.cd_devs[unit]) == NULL) {
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
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_CAR, channel);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_TDR, cbuf);
	} else {
		sc->sc_cl[channel].transmitting = 0;
	}
#else
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0)
	{
		tp->t_state |= TS_BUSY;
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_CAR, channel);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, CL_IER,
		    bus_space_read_1(sc->sc_iot, sc->sc_ioh, CL_IER) | 0x03);
	}
#endif
	splx(s);
}

int
cl_mintr(arg)
	void *arg;
{
	struct clsoftc *sc = arg;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t mir, misr, msvr;
	int channel;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	mir = bus_space_read_1(iot, ioh, CL_MIR);
	if ((mir & 0x40) == 0) {
		return 0;
	}

	channel = mir & 0x03;
	misr = bus_space_read_1(iot, ioh, CL_MISR);
	msvr = bus_space_read_1(iot, ioh, CL_MSVR_RTS);
	if (misr & 0x01) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 1 unexpected\n",channel);
	}
	if (misr & 0x02) {
		/* timers are not currently used?? */
		log(LOG_WARNING, "cl_mintr: channel %x timer 2 unexpected\n",channel);
	}
	if (misr & 0x20) {
#ifdef DEBUG
		log(LOG_WARNING, "cl_mintr: channel %x cts %x\n",channel,
		    ((msvr & 0x20) != 0x0)
		);
#endif
	}
	if (misr & 0x40) {
		struct tty *tp = sc->sc_cl[channel].tty;
#ifdef DEBUG
		log(LOG_WARNING, "cl_mintr: channel %x cd %x\n",channel,
		    ((msvr & 0x40) != 0x0)
		);
#endif
		ttymodem(tp, ((msvr & 0x40) != 0x0) );
	}
	if (misr & 0x80) {
#ifdef DEBUG
		log(LOG_WARNING, "cl_mintr: channel %x dsr %x\n",channel,
		((msvr & 0x80) != 0x0)
		);
#endif
	}
	bus_space_write_1(iot, ioh, CL_MEOIR, 0);
	return 1;
}

int
cl_txintr(arg)
	void *arg;
{
	static int empty;
	struct clsoftc *sc = arg;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t tir, cmr, teoir;
	u_int8_t max;
	int channel;
	struct tty *tp;
	int cnt;
	u_char buffer[CL_FIFO_MAX +1];

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;
	tir = bus_space_read_1(iot, ioh, CL_TIR);
	if ((tir & 0x40) == 0) {
		return 0;
	}

	channel = tir & 0x03;
	sc->sc_cl[channel].txcnt ++;

	cmr = bus_space_read_1(iot, ioh, CL_CMR);

	tp = sc->sc_cl[channel].tty;
	if (tp == NULL || (tp->t_state & TS_ISOPEN) == 0) {
		bus_space_write_1(iot, ioh, CL_IER,
		    bus_space_read_1(iot, ioh, CL_IER) & ~0x03);
		bus_space_write_1(iot, ioh, CL_TEOIR, 0x08);
		return 1;
	}

	switch (cmr & CL_TXMASK) {
	case CL_TXDMAINT:
	{
		u_int8_t dmabsts;
		int nbuf, busy, resid;
		void *pbuffer;

		dmabsts = bus_space_read_1(iot, ioh, CL_DMABSTS);
		nbuf = ((dmabsts & 0x8) >> 3) & 0x1;
		busy = ((dmabsts & 0x4) >> 2) & 0x1;

		do {
			pbuffer = sc->sc_cl[channel].tx[nbuf];
			resid = tp->t_outq.c_cc;
			cnt = min (CL_BUFSIZE,resid);
			log(LOG_WARNING, "cl_txintr: resid %x cnt %x pbuf %p\n",
			    resid, cnt, pbuffer);
			if (cnt != 0) {
				cnt = q_to_b(&tp->t_outq, pbuffer, cnt);
				resid -= cnt;
				if (nbuf == 0) {
					bus_space_write_2(iot, ioh, CL_ATBADRU,
					    ((u_long)sc->sc_cl[channel].txp[nbuf]) >> 16);
					bus_space_write_2(iot, ioh, CL_ATBADRL,
					    ((u_long) sc->sc_cl[channel].txp[nbuf]) & 0xffff);
					bus_space_write_2(iot, ioh, CL_ATBCNT,
					    cnt);
					bus_space_write_1(iot, ioh, CL_ATBSTS,
					    0x43);
				} else {
					bus_space_write_2(iot, ioh, CL_BTBADRU,
					    ((u_long)sc->sc_cl[channel].txp[nbuf]) >> 16);
					bus_space_write_2(iot, ioh, CL_BTBADRL,
					    ((u_long) sc->sc_cl[channel].txp[nbuf]) & 0xffff);
					bus_space_write_2(iot, ioh, CL_BTBCNT,
					    cnt);
					bus_space_write_1(iot, ioh, CL_BTBSTS,
					    0x43);
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
				bus_space_write_1(iot, ioh, CL_IER,
				    bus_space_read_1(iot, ioh, CL_IER) & ~0x03);
			}
			nbuf = ~nbuf & 0x1;
			busy--;
		} while (resid != 0 && busy != -1);/* if not busy do other buffer */
	}
		break;
	case CL_TXINTR:
		max = bus_space_read_1(iot, ioh, CL_TFTC);
		cnt = min((int)max,tp->t_outq.c_cc);
		if (cnt != 0) {
			cnt = q_to_b(&tp->t_outq, buffer, cnt);
			empty = 0;
			bus_space_write_multi_1(iot, ioh, CL_TDR, buffer, cnt);
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
			bus_space_write_1(iot, ioh, CL_IER,
			    bus_space_read_1(iot, ioh, CL_IER) & ~0x03);
		}
		break;
	default:
		log(LOG_WARNING, "cl_txintr unknown mode %x\n", cmr);
		/* we probably will go to hell quickly now */
		teoir = 0x08;
	}
	bus_space_write_1(iot, ioh, CL_TEOIR, teoir);
	return 1;
}

int
cl_rxintr(arg)
	void *arg;
{
	struct clsoftc *sc = arg;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t rir, channel, cmr, risrl;
	u_int8_t fifocnt;
	struct tty *tp;
	int i;
	u_int8_t reoir;
	u_char buffer[CL_FIFO_MAX +1];
#ifdef DDB
	int wantddb = 0;
#endif

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	rir = bus_space_read_1(iot, ioh, CL_RIR);
	if ((rir & 0x40) == 0x0) {
		return 0;
	}

	channel = rir & 0x3;
	cmr = bus_space_read_1(iot, ioh, CL_CMR);

	sc->sc_cl[channel].rxcnt ++;
	risrl = bus_space_read_1(iot, ioh, CL_RISRL);
	if (risrl & 0x80) {
		/* timeout, no characters */
	} else
	/* We don't need no stinkin special characters */
	if (risrl & 0x08) {
		cl_overflow(sc, channel, (long *)&sc->sc_fotime, "fifo");
	} else
	if (risrl & 0x04) {
		cl_parity(sc, channel);
	} else
	if (risrl & 0x02) {
		cl_frame(sc, channel);
	} else
	if (risrl & 0x01) {
#ifdef DDB
		if (sc->sc_cl[channel].cl_consio)
			wantddb = db_console;
#endif
		cl_break(sc, channel);
	}
	reoir = 0x08;

	switch (cmr & CL_RXMASK) {
	case CL_RXDMAINT:
	{
		int nbuf;
		u_int16_t cnt;
		int bufcomplete;
		u_int8_t status, dmabsts;
		u_int8_t risrh;

		risrh = bus_space_read_1(iot, ioh, CL_RISRH);
		dmabsts = bus_space_read_1(iot, ioh, CL_DMABSTS);
		nbuf = (risrh & 0x08) ? 1 : 0;
		bufcomplete = (risrh & 0x20) ? 1 : 0;
		if (nbuf == 0) {
			cnt = bus_space_read_2(iot, ioh, CL_ARBCNT);
			status = bus_space_read_1(iot, ioh, CL_ARBSTS);
		} else {
			cnt = bus_space_read_2(iot, ioh, CL_BRBCNT);
			status = bus_space_read_1(iot, ioh, CL_BRBSTS);
		}
#if USE_BUFFER
		cl_appendbufn(sc, channel, sc->rx[nbuf], cnt);
#else
		{
			int i;
			u_char *pbuf;

			tp = sc->sc_cl[channel].tty;
			pbuf = sc->sc_cl[channel].rx[nbuf];
			/* this should be done at off level */
			{
				u_int16_t rcbadru, rcbadrl;
				u_int8_t arbsts, brbsts;
				u_char *pbufs, *pbufe;

				rcbadru = bus_space_read_2(iot, ioh,
				    CL_RCBADRU);
				rcbadrl = bus_space_read_2(iot, ioh,
				    CL_RCBADRL);
				arbsts = bus_space_read_1(iot, ioh, CL_ARBSTS);
				brbsts = bus_space_read_1(iot, ioh, CL_BRBSTS);
				pbufs = sc->sc_cl[channel].rxp[nbuf];
				pbufe = (u_char *)(((u_long)rcbadru << 16) | (u_long)rcbadrl);
				cnt = pbufe - pbufs;
			}
			reoir = 0x0 | (bufcomplete) ? 0 : 0xd0;
			bus_space_write_1(iot, ioh, CL_REOIR, reoir);

			DELAY(10); /* give the chip a moment */

			for (i = 0; i < cnt; i++) {
				u_char c;
				c = pbuf[i];
				(*linesw[tp->t_line].l_rint)(c,tp);
			}
			/* this should be done at off level */
			if (nbuf == 0) {
				bus_space_write_2(iot, ioh, CL_ARBCNT,
				    CL_BUFSIZE);
				bus_space_write_2(iot, ioh, CL_ARBSTS, 0x01);
			} else {
				bus_space_write_2(iot, ioh, CL_BRBCNT,
				    CL_BUFSIZE);
				bus_space_write_2(iot, ioh, CL_BRBSTS, 0x01);
			}
		}
#endif
	}
		bus_space_write_1(iot, ioh, CL_REOIR, reoir);
		break;
	case CL_RXINTR:
		fifocnt = bus_space_read_1(iot, ioh, CL_RFOC);
		tp = sc->sc_cl[channel].tty;
		bus_space_read_multi_1(iot, ioh, CL_RDR, buffer, fifocnt);
		if (tp == NULL) {
			/* if the channel is not configured,
			 * dont send characters upstream.
			 * also fix problem with NULL dereference
			 */
			reoir = 0x00;
			break;
		}

		bus_space_write_1(iot, ioh, CL_REOIR, reoir);
		for (i = 0; i < fifocnt; i++) {
			u_char c;
			c = buffer[i];
#if USE_BUFFER
			cl_appendbuf(sc, channel, c);
#else
			/* does any restricitions exist on spl
			 * for this call
			 */
			(*linesw[tp->t_line].l_rint)(c,tp);
#endif
		}
		break;
	default:
		log(LOG_WARNING, "cl_rxintr unknown mode %x\n", cmr);
		/* we probably will go to hell quickly now */
		bus_space_write_1(iot, ioh, CL_REOIR, 0x08);
	}
#ifdef DDB
	if (wantddb != 0)
		Debugger();
#endif
	return 1;
}

void
cl_overflow(sc, channel, ptime, msg)
	struct clsoftc *sc;
	int channel;
	long *ptime;
	char *msg;
{
	log(LOG_WARNING, "%s[%d]: %s overrun\n", sc->sc_dev.dv_xname,
	    channel, msg);
}

void
cl_parity(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s[%d]: parity error\n", sc->sc_dev.dv_xname,
	    channel);
}

void
cl_frame(sc, channel)
	struct clsoftc *sc;
	int channel;
{
	log(LOG_WARNING, "%s[%d]: frame error\n", sc->sc_dev.dv_xname,
	    channel);
}

void
cl_break(sc, channel)
	struct clsoftc *sc;
	int channel;
{
#ifdef DEBUG
	log(LOG_WARNING, "%s[%d]: break detected\n", sc->sc_dev.dv_xname,
	    channel);
#endif
}

#ifdef DEBUG
void
cl_dumpport(struct clsoftc *sc, int channel)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	u_int8_t livr, cmr, cor1, cor2, cor3, cor4, cor5, cor6, cor7,
	    schr1, schr2, schr3, schr4, scrl, scrh, lnxt,
	    rbpr, rcor, tbpr, tcor, rpilr, rir, tpr, ier, ccr,
	    dmabsts, arbsts, brbsts, atbsts, btbsts,
	    csr, rts, dtr, rtprl, rtprh;
	u_int16_t rcbadru, rcbadrl, arbadru, arbadrl, arbcnt,
	    brbadru, brbadrl, brbcnt;
	u_int16_t tcbadru, tcbadrl, atbadru, atbadrl, atbcnt,
	    btbadru, btbadrl, btbcnt;
	int s;

	iot = sc->sc_iot;
	ioh = sc->sc_ioh;

	s = splcl();
	bus_space_write_1(iot, ioh, CL_CAR, channel);
	livr = bus_space_read_1(iot, ioh, CL_LIVR);
	cmr = bus_space_read_1(iot, ioh, CL_CMR);
	cor1 = bus_space_read_1(iot, ioh, CL_COR1);
	cor2 = bus_space_read_1(iot, ioh, CL_COR2);
	cor3 = bus_space_read_1(iot, ioh, CL_COR3);
	cor4 = bus_space_read_1(iot, ioh, CL_COR4);
	cor5 = bus_space_read_1(iot, ioh, CL_COR5);
	cor6 = bus_space_read_1(iot, ioh, CL_COR6);
	cor7 = bus_space_read_1(iot, ioh, CL_COR7);
	schr1 = bus_space_read_1(iot, ioh, CL_SCHR1);
	schr2 = bus_space_read_1(iot, ioh, CL_SCHR2);
	schr3 = bus_space_read_1(iot, ioh, CL_SCHR3);
	schr4 = bus_space_read_1(iot, ioh, CL_SCHR4);
	scrl = bus_space_read_1(iot, ioh, CL_SCRL);
	scrh = bus_space_read_1(iot, ioh, CL_SCRH);
	lnxt = bus_space_read_1(iot, ioh, CL_LNXT);
	rbpr = bus_space_read_1(iot, ioh, CL_RBPR);
	rcor = bus_space_read_1(iot, ioh, CL_RCOR);
	tbpr = bus_space_read_1(iot, ioh, CL_TBPR);
	rpilr = bus_space_read_1(iot, ioh, CL_RPILR);
	rir = bus_space_read_1(iot, ioh, CL_RIR);
	ier = bus_space_read_1(iot, ioh, CL_IER);
	ccr = bus_space_read_1(iot, ioh, CL_CCR);
	tcor = bus_space_read_1(iot, ioh, CL_TCOR);
	csr = bus_space_read_1(iot, ioh, CL_CSR);
	tpr = bus_space_read_1(iot, ioh, CL_TPR);
	rts = bus_space_read_1(iot, ioh, CL_MSVR_RTS);
	dtr = bus_space_read_1(iot, ioh, CL_MSVR_DTR);
	rtprl = bus_space_read_1(iot, ioh, CL_RTPRL);
	rtprh = bus_space_read_1(iot, ioh, CL_RTPRH);
	dmabsts = bus_space_read_1(iot, ioh, CL_DMABSTS);
	tcbadru = bus_space_read_2(iot, ioh, CL_TCBADRU);
	tcbadrl = bus_space_read_2(iot, ioh, CL_TCBADRL);
	rcbadru = bus_space_read_2(iot, ioh, CL_RCBADRU);
	rcbadrl = bus_space_read_2(iot, ioh, CL_RCBADRL);
	arbadru = bus_space_read_2(iot, ioh, CL_ARBADRU);
	arbadrl = bus_space_read_2(iot, ioh, CL_ARBADRL);
	arbcnt  = bus_space_read_2(iot, ioh, CL_ARBCNT);
	arbsts  = bus_space_read_1(iot, ioh, CL_ARBSTS);
	brbadru = bus_space_read_2(iot, ioh, CL_BRBADRU);
	brbadrl = bus_space_read_2(iot, ioh, CL_BRBADRL);
	brbcnt  = bus_space_read_2(iot, ioh, CL_BRBCNT);
	brbsts  = bus_space_read_1(iot, ioh, CL_BRBSTS);
	atbadru = bus_space_read_2(iot, ioh, CL_ATBADRU);
	atbadrl = bus_space_read_2(iot, ioh, CL_ATBADRL);
	atbcnt  = bus_space_read_2(iot, ioh, CL_ATBCNT);
	atbsts  = bus_space_read_1(iot, ioh, CL_ATBSTS);
	btbadru = bus_space_read_2(iot, ioh, CL_BTBADRU);
	btbadrl = bus_space_read_2(iot, ioh, CL_BTBADRL);
	btbcnt  = bus_space_read_2(iot, ioh, CL_BTBCNT);
	btbsts  = bus_space_read_1(iot, ioh, CL_BTBSTS);
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
#endif
