/*	$OpenBSD: spif.c,v 1.4 1999/02/23 23:47:46 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the SUNW,spif: 8 serial, 1 parallel sbus board
 * based heavily on Iain Hibbert's driver for the MAGMA cards
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/spifreg.h>
#include <sparc/dev/spifvar.h>

#if PIL_TTY == 1
# define IE_MSOFT IE_L1
#elif PIL_TTY == 4
# define IE_MSOFT IE_L4
#elif PIL_TTY == 6
# define IE_MSOFT IE_L6
#else
# error "no suitable software interrupt bit"
#endif

/*
 * useful macros
 */
#define	SET(t, f)	((t) |= (f))
#define	CLR(t, f)	((t) &= ~(f))
#define	ISSET(t, f)	((t) & (f))

int	spifmatch	__P((struct device *, void *, void *));
void	spifattach	__P((struct device *, struct device *, void *));

int	sttymatch	__P((struct device *, void *, void *));
void	sttyattach	__P((struct device *, struct device *, void *));
int	sttyopen	__P((dev_t, int, int, struct proc *));
int	sttyclose	__P((dev_t, int, int, struct proc *));
int	sttyread	__P((dev_t, struct uio *, int));
int	sttywrite	__P((dev_t, struct uio *, int));
int	sttyioctl	__P((dev_t, u_long, caddr_t, int, struct proc *));
int	sttystop	__P((struct tty *, int));
int	spifstcintr	__P((void *));
int	spifsoftintr	__P((void *));
int	stty_param	__P((struct tty *, struct termios *));
struct tty *sttytty	__P((dev_t));
int	stty_modem_control __P((struct stty_port *, int, int));
static __inline	void	stty_write_ccr __P((struct stcregs *, u_int8_t));
int	stty_compute_baud __P((speed_t, int, u_int8_t *, u_int8_t *));
void	stty_start	__P((struct tty *));

int	sbppmatch	__P((struct device *, void *, void *));
void	sbppattach	__P((struct device *, struct device *, void *));
int	sbppopen	__P((dev_t, int, int, struct proc *));
int	sbppclose	__P((dev_t, int, int, struct proc *));
int	sbppread	__P((dev_t, struct uio *, int));
int	sbppwrite	__P((dev_t, struct uio *, int));
int	sbpp_rw	__P((dev_t, struct uio *));
int	spifppcintr	__P((void *));
int	sbppselect	__P((dev_t, int, struct proc *));
int	sbppioctl	__P((dev_t, u_long, caddr_t, int, struct proc *));

struct cfattach spif_ca = {
	sizeof (struct spif_softc), spifmatch, spifattach
};

struct cfdriver spif_cd = {
	NULL, "spif", DV_IFNET
};

struct cfattach stty_ca = {
	sizeof(struct stty_softc), sttymatch, sttyattach
};

struct cfdriver stty_cd = {
	NULL, "stty", DV_TTY
};

struct cfattach sbpp_ca = {
	sizeof(struct sbpp_softc), sbppmatch, sbppattach
};

struct cfdriver sbpp_cd = {
	NULL, "sbpp", DV_DULL
};

int
spifmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("SUNW,spif", ra->ra_name))
		return (0);
	return (1);
}

void    
spifattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct spif_softc *sc = (struct spif_softc *)self;
	struct confargs *ca = aux;
	int stcpri, ppcpri;

	if (ca->ca_ra.ra_nintr != 2) {
		printf(": expected 2 interrupts, got %d\n",
		    ca->ca_ra.ra_nintr);
		return;
	}
	stcpri = ca->ca_ra.ra_intr[SERIAL_INTR].int_pri;
	ppcpri = ca->ca_ra.ra_intr[PARALLEL_INTR].int_pri;

	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected %d registers, got %d\n",
		    1, ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_regs = mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);

	sc->sc_node = ca->ca_ra.ra_node;

	sc->sc_rev = getpropint(sc->sc_node, "revlev", 0);

	sc->sc_osc = getpropint(sc->sc_node, "verosc", 0);
	switch (sc->sc_osc) {
	case SPIF_OSC10:
		sc->sc_osc = 10;
		break;
	case SPIF_OSC9:
	default:
		sc->sc_osc = 9;
		break;
	}

	sc->sc_nser = 8;
	sc->sc_npar = 1;

	sc->sc_rev2 = sc->sc_regs->stc.gfrcr;
	sc->sc_regs->stc.gsvr = 0;

	stty_write_ccr(&sc->sc_regs->stc,
	    CD180_CCR_CMD_RESET | CD180_CCR_RESETALL);
	while (sc->sc_regs->stc.gsvr != 0xff);
	while (sc->sc_regs->stc.gfrcr != sc->sc_rev2);

	sc->sc_regs->stc.gsvr = 0;
	sc->sc_regs->stc.msmr = SPIF_MSMR;
	sc->sc_regs->stc.tsmr = SPIF_TSMR;
	sc->sc_regs->stc.rsmr = SPIF_RSMR;
	sc->sc_regs->stc.pprh = CD180_PPRH;
	sc->sc_regs->stc.pprl = CD180_PPRL;

	printf(": rev %x chiprev %x osc %dMhz stcpri %d ppcpri %d softpri %d\n",
	    sc->sc_rev, sc->sc_rev2, sc->sc_osc, stcpri, ppcpri, PIL_TTY);

	if (sc->sc_osc == 10)
		sc->sc_osc = 10000000;
	else
		sc->sc_osc = 9830400;

	(void)config_found(self, sttymatch, NULL);
	(void)config_found(self, sbppmatch, NULL);

	sc->sc_ppcih.ih_fun = spifppcintr;
	sc->sc_ppcih.ih_arg = sc;
	intr_establish(ppcpri, &sc->sc_ppcih);

	sc->sc_stcih.ih_fun = spifstcintr;
	sc->sc_stcih.ih_arg = sc;
	intr_establish(stcpri, &sc->sc_stcih);

	sc->sc_softih.ih_fun = spifsoftintr;
	sc->sc_softih.ih_arg = sc;
	intr_establish(PIL_TTY, &sc->sc_softih);
}

int
sttymatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct spif_softc *sc = (struct spif_softc *)parent;

	return (aux == sttymatch && sc->sc_ttys == NULL);
}

void
sttyattach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct spif_softc *sc = (struct spif_softc *)parent;
	struct stty_softc *ssc = (struct stty_softc *)dev;
	int port;

	sc->sc_ttys = ssc;

	for (port = 0; port < sc->sc_nser; port++) {
		struct stty_port *sp = &ssc->sc_port[port];
		struct tty *tp;

		sp->sp_dtr = 0;
		sc->sc_regs->dtrlatch[port] = 1;

		tp = ttymalloc();
		if (tp == NULL)
			break;
		tty_attach(tp);

		tp->t_oproc = stty_start;
		tp->t_param = stty_param;

		sp->sp_tty = tp;
		sp->sp_sc = sc;
		sp->sp_channel = port;

		sp->sp_rbuf = malloc(STTY_RBUF_SIZE, M_DEVBUF, M_NOWAIT);
		if(sp->sp_rbuf == NULL)
			break;

		sp->sp_rend = sp->sp_rbuf + STTY_RBUF_SIZE;
	}

	ssc->sc_nports = port;

	printf(": %d tty%s\n", port, port == 1 ? "" : "s");
}

int
sttyopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct spif_softc *csc;
	struct stty_softc *sc;
	struct stty_port *sp;
	struct tty *tp;
	int card = SPIF_CARD(dev);
	int port = SPIF_PORT(dev);
	int s;

	if (card >= stty_cd.cd_ndevs || card >= spif_cd.cd_ndevs)
		return (ENXIO);

	sc = stty_cd.cd_devs[card];
	csc = spif_cd.cd_devs[card];
	if (sc == NULL)
		return (ENXIO);

	if (port >= sc->sc_nports)
		return (ENXIO);

	sp = &sc->sc_port[port];
	tp = sp->sp_tty;
	tp->t_dev = dev;

	if (!ISSET(tp->t_state, TS_ISOPEN)) {
		SET(tp->t_state, TS_WOPEN);

		ttychars(tp);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_cflag = TTYDEF_CFLAG;
		if (ISSET(sp->sp_openflags, TIOCFLAG_CLOCAL))
			SET(tp->t_cflag, CLOCAL);
		if (ISSET(sp->sp_openflags, TIOCFLAG_CRTSCTS))
			SET(tp->t_cflag, CRTSCTS);
		if (ISSET(sp->sp_openflags, TIOCFLAG_MDMBUF))
			SET(tp->t_cflag, MDMBUF);
		tp->t_lflag = TTYDEF_LFLAG;
		tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;

		sp->sp_rput = sp->sp_rget = sp->sp_rbuf;

		s = spltty();

		csc->sc_regs->stc.car = sp->sp_channel;
		stty_write_ccr(&csc->sc_regs->stc,
		    CD180_CCR_CMD_RESET | CD180_CCR_RESETCHAN);

		stty_param(tp, &tp->t_termios);

		ttsetwater(tp);

		csc->sc_regs->stc.srer = CD180_SRER_CD | CD180_SRER_RXD;

		if (ISSET(sp->sp_openflags, TIOCFLAG_SOFTCAR) || sp->sp_carrier)
			SET(tp->t_state, TS_CARR_ON);
		else
			CLR(tp->t_state, TS_CARR_ON);
	}
	else if (ISSET(tp->t_state, TS_XCLUDE) && p->p_ucred->cr_uid != 0) {
		return (EBUSY);
	} else {
		s = spltty();
	}

	if (!ISSET(flags, O_NONBLOCK)) {
		while (!ISSET(tp->t_cflag, CLOCAL) &&
		    !ISSET(tp->t_state, TS_CARR_ON)) {
			int error;

			SET(tp->t_state, TS_WOPEN);
			error = ttysleep(tp, &tp->t_rawq, TTIPRI | PCATCH,
			    "sttycd", 0);
			if (error != 0) {
				splx(s);
				CLR(tp->t_state, TS_WOPEN);
				return (error);
			}
		}
	}

	splx(s);

	return ((*linesw[tp->t_line].l_open)(dev, tp));
}

int
sttyclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct stty_softc *sc = stty_cd.cd_devs[SPIF_CARD(dev)];
	struct stty_port *sp = &sc->sc_port[SPIF_PORT(dev)];
	struct spif_softc *csc = sp->sp_sc;
	struct tty *tp = sp->sp_tty;
	int port = SPIF_PORT(dev);
	int s;

	(*linesw[tp->t_line].l_close)(tp, flags);
	s = spltty();

	if (ISSET(tp->t_cflag, HUPCL) || !ISSET(tp->t_state, TS_ISOPEN)) {
		stty_modem_control(sp, 0, DMSET);
		csc->sc_regs->stc.car = port;
		csc->sc_regs->stc.ccr =
		    CD180_CCR_CMD_RESET | CD180_CCR_RESETCHAN;
	}

	splx(s);
	ttyclose(tp);
	return (0);
}

int
sttyioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct stty_softc *stc = stty_cd.cd_devs[SPIF_CARD(dev)];
	struct stty_port *sp = &stc->sc_port[SPIF_PORT(dev)];
	struct spif_softc *sc = sp->sp_sc;
	struct tty *tp = sp->sp_tty;
	int error;

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flags, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flags, p);
	if (error >= 0)
		return (error);

	error = 0;

	switch (cmd) {
	case TIOCSBRK:
		SET(sp->sp_flags, STTYF_SET_BREAK);
		sc->sc_regs->stc.car = sp->sp_channel;
		sc->sc_regs->stc.srer |= CD180_SRER_TXD;
		break;
	case TIOCCBRK:
		SET(sp->sp_flags, STTYF_CLR_BREAK);
		sc->sc_regs->stc.car = sp->sp_channel;
		sc->sc_regs->stc.srer |= CD180_SRER_TXD;
		break;
	case TIOCSDTR:
		stty_modem_control(sp, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		stty_modem_control(sp, TIOCM_DTR, DMBIC);
		break;
	case TIOCMBIS:
		stty_modem_control(sp, *((int *)data), DMBIS);
		break;
	case TIOCMBIC:
		stty_modem_control(sp, *((int *)data), DMBIC);
		break;
	case TIOCMGET:
		*((int *)data) = stty_modem_control(sp, 0, DMGET);
		break;
	case TIOCGFLAGS:
		*((int *)data) = sp->sp_openflags;
		break;
	case TIOCSFLAGS:
		if (suser(p->p_ucred, &p->p_acflag))
			error = EPERM;
		else
			sp->sp_openflags = *((int *)data) &
			    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL |
			     TIOCFLAG_CRTSCTS | TIOCFLAG_MDMBUF);
		break;
	default:
		error = ENOTTY;
	}

	return (error);
}

int
stty_modem_control(sp, bits, how)
	struct stty_port *sp;
	int bits, how;
{
	struct spif_softc *csc = sp->sp_sc;
	struct tty *tp = sp->sp_tty;
	int s, msvr;

	s = spltty();
	csc->sc_regs->stc.car = sp->sp_channel;

	switch (how) {
	case DMGET:
		bits = TIOCM_LE;
		if (sp->sp_dtr)
			bits |= TIOCM_DTR;
		msvr = csc->sc_regs->stc.msvr;
		if (ISSET(msvr, CD180_MSVR_DSR))
			bits |= TIOCM_DSR;
		if (ISSET(msvr, CD180_MSVR_CD))
			bits |= TIOCM_CD;
		if (ISSET(msvr, CD180_MSVR_CTS))
			bits |= TIOCM_CTS;
		if (ISSET(msvr, CD180_MSVR_RTS))
			bits |= TIOCM_RTS;
		break;
	case DMSET:
		if (ISSET(bits, TIOCM_DTR)) {
			sp->sp_dtr = 1;
			csc->sc_regs->dtrlatch[sp->sp_channel] = 0;
		}
		else {
			sp->sp_dtr = 0;
			csc->sc_regs->dtrlatch[sp->sp_channel] = 1;
		}
		if (ISSET(bits, TIOCM_RTS))
			csc->sc_regs->stc.msvr &= ~CD180_MSVR_RTS;
		else
			csc->sc_regs->stc.msvr |= CD180_MSVR_RTS;
		break;
	case DMBIS:
		if (ISSET(bits, TIOCM_DTR)) {
			sp->sp_dtr = 1;
			csc->sc_regs->dtrlatch[sp->sp_channel] = 0;
		}
		if (ISSET(bits, TIOCM_RTS) && !ISSET(tp->t_cflag, CRTSCTS))
			csc->sc_regs->stc.msvr &= ~CD180_MSVR_RTS;
		break;
	case DMBIC:
		if (ISSET(bits, TIOCM_DTR)) {
			sp->sp_dtr = 0;
			csc->sc_regs->dtrlatch[sp->sp_channel] = 1;
		}
		if (ISSET(bits, TIOCM_RTS))
			csc->sc_regs->stc.msvr |= CD180_MSVR_RTS;
		break;
	}

	splx(s);
	return (bits);
}

int
stty_param(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct stty_softc *st = stty_cd.cd_devs[SPIF_CARD(tp->t_dev)];
	struct stty_port *sp = &st->sc_port[SPIF_PORT(tp->t_dev)];
	struct spif_softc *sc = sp->sp_sc;
	u_int8_t rbprl, rbprh, tbprl, tbprh;
	int s, opt;

	if (t->c_ospeed &&
	    stty_compute_baud(t->c_ospeed, sc->sc_osc, &tbprl, &tbprh))
		return (EINVAL);

	if (t->c_ispeed &&
	    stty_compute_baud(t->c_ispeed, sc->sc_osc, &rbprl, &rbprh))
		return (EINVAL);

	s = spltty();

	/* hang up line if ospeed is zero, otherwise raise DTR */
	stty_modem_control(sp, TIOCM_DTR,
	    (t->c_ospeed == 0 ? DMBIC : DMBIS));

	sc->sc_regs->stc.car = sp->sp_channel;

	opt = 0;
	if (ISSET(t->c_cflag, PARENB)) {
		opt |= CD180_COR1_PARMODE_NORMAL;
		opt |= (ISSET(t->c_cflag, PARODD) ?
				CD180_COR1_ODDPAR :
				CD180_COR1_EVENPAR);
	}
	else
		opt |= CD180_COR1_PARMODE_NO;

	if (!ISSET(t->c_iflag, INPCK))
		opt |= CD180_COR1_IGNPAR;

	if (ISSET(t->c_cflag, CSTOPB))
		opt |= CD180_COR1_STOP2;

	switch (t->c_cflag & CSIZE) {
	case CS5:
		opt |= CD180_COR1_CS5;
		break;
	case CS6:
		opt |= CD180_COR1_CS6;
		break;
	case CS7:
		opt |= CD180_COR1_CS7;
		break;
	default:
		opt |= CD180_COR1_CS8;
		break;
	}
	sc->sc_regs->stc.cor1 = opt;

	opt = CD180_COR2_ETC;
	if (ISSET(t->c_cflag, CRTSCTS))
		opt |= CD180_COR2_CTSAE;
	sc->sc_regs->stc.cor2 = opt;

	sc->sc_regs->stc.cor3 = STTY_RX_FIFO_THRESHOLD;

	stty_write_ccr(&sc->sc_regs->stc, CD180_CCR_CMD_COR |
	    CD180_CCR_CORCHG1 | CD180_CCR_CORCHG2 | CD180_CCR_CORCHG3);

	sc->sc_regs->stc.schr1 = 0x11;
	sc->sc_regs->stc.schr2 = 0x13;
	sc->sc_regs->stc.schr3 = 0x11;
	sc->sc_regs->stc.schr4 = 0x13;
	sc->sc_regs->stc.rtpr = 0x28;

	sc->sc_regs->stc.mcor1 = CD180_MCOR1_CDZD | STTY_RX_DTR_THRESHOLD;
	sc->sc_regs->stc.mcor2 = CD180_MCOR2_CDOD;

	if (t->c_ospeed) {
		sc->sc_regs->stc.tbprh = tbprh;
		sc->sc_regs->stc.tbprl = tbprl;
	}

	if (t->c_ispeed) {
		sc->sc_regs->stc.rbprh = rbprh;
		sc->sc_regs->stc.rbprl = rbprl;
	}

	stty_write_ccr(&sc->sc_regs->stc, CD180_CCR_CMD_CHAN |
	    CD180_CCR_CHAN_TXEN | CD180_CCR_CHAN_RXEN);

	sp->sp_carrier = sc->sc_regs->stc.msvr & CD180_MSVR_CD;

	splx(s);
	return (0);
}

int
sttyread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct stty_softc *sc = stty_cd.cd_devs[SPIF_CARD(dev)];
	struct stty_port *sp = &sc->sc_port[SPIF_PORT(dev)];
	struct tty *tp = sp->sp_tty;

	return ((*linesw[tp->t_line].l_read)(tp, uio, flags));
}

int
sttywrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct stty_softc *sc = stty_cd.cd_devs[SPIF_CARD(dev)];
	struct stty_port *sp = &sc->sc_port[SPIF_PORT(dev)];
	struct tty *tp = sp->sp_tty;

	return ((*linesw[tp->t_line].l_write)(tp, uio, flags));
}

struct tty *
sttytty(dev)
	dev_t dev;
{
	struct stty_softc *sc = stty_cd.cd_devs[SPIF_CARD(dev)];
	struct stty_port *sp = &sc->sc_port[SPIF_PORT(dev)];

	return (sp->sp_tty);
}

int
sttystop(tp, flags)
	struct tty *tp;
	int flags;
{
	struct stty_softc *sc = stty_cd.cd_devs[SPIF_CARD(tp->t_dev)];
	struct stty_port *sp = &sc->sc_port[SPIF_PORT(tp->t_dev)];
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
		SET(sp->sp_flags, STTYF_STOP);
	}
	splx(s);
	return (0);
}

void
stty_start(tp)
	struct tty *tp;
{
	struct stty_softc *stc = stty_cd.cd_devs[SPIF_CARD(tp->t_dev)];
	struct stty_port *sp = &stc->sc_port[SPIF_PORT(tp->t_dev)];
	struct spif_softc *sc = sp->sp_sc;
	int s;

	s = spltty();

	if (!ISSET(tp->t_state, TS_TTSTOP | TS_TIMEOUT | TS_BUSY)) {
		if (tp->t_outq.c_cc <= tp->t_lowat) {
			if (ISSET(tp->t_state, TS_ASLEEP)) {
				CLR(tp->t_state, TS_ASLEEP);
				wakeup(&tp->t_outq);
			}
			selwakeup(&tp->t_wsel);
		}
		if (tp->t_outq.c_cc) {
			sp->sp_txc = ndqb(&tp->t_outq, 0);
			sp->sp_txp = tp->t_outq.c_cf;
			SET(tp->t_state, TS_BUSY);
			sc->sc_regs->stc.car = sp->sp_channel;
			sc->sc_regs->stc.srer |= CD180_SRER_TXD;
		}
	}

	splx(s);
}

int
spifstcintr(vsc)
	void *vsc;
{
	struct spif_softc *sc = (struct spif_softc *)vsc;
	struct stty_port *sp;
	u_int8_t channel, ar, *ptr;
	int needsoft = 0, r = 0, i;

	/*
	 * Receive data service request
	 * (also Receive error service request)
	 */
	ar = sc->sc_regs->istc.rrar & CD180_GSVR_IMASK;

	switch (ar) {
	case CD180_GSVR_RXGOOD:
		r = 1;
		channel = CD180_GSCR_CHANNEL(sc->sc_regs->stc.gscr1);
		sp = &sc->sc_ttys->sc_port[channel];
		ptr = sp->sp_rput;
		for (i = sc->sc_regs->stc.rdcr; i > 0; i--) {
			*ptr++ = 0;
			*ptr++ = sc->sc_regs->stc.rdr;
			if (ptr == sp->sp_rend)
				ptr = sp->sp_rbuf;
			if (ptr == sp->sp_rget) {
				if (ptr == sp->sp_rbuf)
					ptr = sp->sp_rend;
				ptr -= 2;
				SET(sp->sp_flags, STTYF_RING_OVERFLOW);
				break;
			}
		}
		sp->sp_rput = ptr;
		needsoft = 1;
		break;
	case CD180_GSVR_RXEXCEPTION:
		r = 1;
		channel = CD180_GSCR_CHANNEL(sc->sc_regs->stc.gscr1);
		sp = &sc->sc_ttys->sc_port[channel];
		ptr = sp->sp_rput;
		*ptr++ = sc->sc_regs->stc.rcsr;
		*ptr++ = sc->sc_regs->stc.rdr;
		if (ptr == sp->sp_rend)
			ptr = sp->sp_rbuf;
		if (ptr == sp->sp_rget) {
			if (ptr == sp->sp_rbuf)
				ptr = sp->sp_rend;
			ptr -= 2;
			SET(sp->sp_flags, STTYF_RING_OVERFLOW);
			break;
		}
		sp->sp_rput = ptr;
		needsoft = 1;
		break;
	}
	sc->sc_regs->stc.eosrr = 0;

	/*
	 * Transmit service request
	 */
	ar = sc->sc_regs->istc.trar & CD180_GSVR_IMASK;
	if (ar == CD180_GSVR_TXDATA) {
		int cnt = 0;

		r = 1;
		channel = CD180_GSCR_CHANNEL(sc->sc_regs->stc.gscr1);
		sp = &sc->sc_ttys->sc_port[channel];

		if (!ISSET(sp->sp_flags, STTYF_STOP)) {
			if (ISSET(sp->sp_flags, STTYF_SET_BREAK)) {
				sc->sc_regs->stc.tdr = 0;
				sc->sc_regs->stc.tdr = 0x81;
				CLR(sp->sp_flags, STTYF_SET_BREAK);
				cnt += 2;
			}
			if (ISSET(sp->sp_flags, STTYF_CLR_BREAK)) {
				sc->sc_regs->stc.tdr = 0;
				sc->sc_regs->stc.tdr = 0x83;
				CLR(sp->sp_flags, STTYF_CLR_BREAK);
				cnt += 2;
			}

			while (sp->sp_txc > 0 && cnt < (CD180_TX_FIFO_SIZE-1)) {
				u_int8_t ch;

				ch = *sp->sp_txp;
				sp->sp_txc--;
				sp->sp_txp++;

				if (ch == 0) {
					sc->sc_regs->stc.tdr = ch;
					cnt++;
				}
				sc->sc_regs->stc.tdr = ch;
				cnt++;
			}

			if (sp->sp_txc == 0 ||
			    ISSET(sp->sp_flags, STTYF_STOP)) {
				sc->sc_regs->stc.srer &= ~CD180_SRER_TXD;
				CLR(sp->sp_flags, STTYF_STOP);
				SET(sp->sp_flags, STTYF_DONE);
				needsoft = 1;
			}
		}
	}
	sc->sc_regs->stc.eosrr = 0;

	/*
	 * Modem signal service request
	 */
	ar = sc->sc_regs->istc.mrar & CD180_GSVR_IMASK;
	if (ar == CD180_GSVR_STATCHG) {
		r = 1;
		channel = CD180_GSCR_CHANNEL(sc->sc_regs->stc.gscr1);
		sp = &sc->sc_ttys->sc_port[channel];
		ar = sc->sc_regs->stc.mcr;
		if (ar & CD180_MCR_CD) {
			SET(sp->sp_flags, STTYF_CDCHG);
			needsoft = 1;
		}

		sc->sc_regs->stc.mcr = 0;
	}
	sc->sc_regs->stc.eosrr = 0;

	if (needsoft) {
#if defined(SUN4M)
		if (CPU_ISSUN4M)
			raise(0, PIL_TTY);
		else
#endif
			ienab_bis(IE_MSOFT);
	}
	return (r);
}

int
spifsoftintr(vsc)
	void *vsc;
{
	struct spif_softc *sc = (struct spif_softc *)vsc;
	struct stty_softc *stc = sc->sc_ttys;
	int r = 0, i, data, s, flags;
	u_int8_t stat, msvr;
	struct stty_port *sp;
	struct tty *tp;

	if (stc != NULL) {
		for (i = 0; i < stc->sc_nports; i++) {
			sp = &stc->sc_port[i];
			tp = sp->sp_tty;

			if (!ISSET(tp->t_state, TS_ISOPEN))
				continue;

			while (sp->sp_rget != sp->sp_rput) {
				stat = sp->sp_rget[0];
				data = sp->sp_rget[1];
				if ((sp->sp_rget + 2) == sp->sp_rend)
					sp->sp_rget = sp->sp_rbuf;
				else
					sp->sp_rget = sp->sp_rget + 2;

				if (stat & (CD180_RCSR_BE | CD180_RCSR_FE))
					data |= TTY_FE;

				if (stat & CD180_RCSR_PE)
					data |= TTY_PE;

				if (stat & CD180_RCSR_OE)
					log(LOG_WARNING,
					    "%s-%x: fifo overflow\n",
					    stc->sc_dev.dv_xname, i);
				(*linesw[tp->t_line].l_rint)(data, tp);
				r = 1;
			}

			s = splhigh();
			flags = sp->sp_flags;
			CLR(sp->sp_flags, STTYF_DONE | STTYF_CDCHG |
			    STTYF_RING_OVERFLOW);
			splx(s);

			if (ISSET(flags, STTYF_CDCHG)) {
				s = spltty();
				sc->sc_regs->stc.car = i;
				msvr = sc->sc_regs->stc.msvr;
				splx(s);

				sp->sp_carrier = msvr & CD180_MSVR_CD;
				(*linesw[tp->t_line].l_modem)(tp,
				    sp->sp_carrier);
				r = 1;
			}

			if (ISSET(flags, STTYF_RING_OVERFLOW)) {
				log(LOG_WARNING, "%s-%x: ring overflow\n",
					stc->sc_dev.dv_xname, i);
				r = 1;
			}

			if (ISSET(flags, STTYF_DONE)) {
				ndflush(&tp->t_outq,
				    sp->sp_txp - tp->t_outq.c_cf);
				CLR(tp->t_state, TS_BUSY);
				(*linesw[tp->t_line].l_start)(tp);
				r = 1;
			}
		}
	}

	return (r);
}

static __inline	void
stty_write_ccr(stc, val)
	struct stcregs *stc;
	u_int8_t val;
{
	int tries = 100000;

	while (stc->ccr && tries--);
	if (tries == 0)
		printf("CCR: timeout\n");
	stc->ccr = val;
}

int
stty_compute_baud(speed, clock, bprlp, bprhp)
	speed_t speed;
	int clock;
	u_int8_t *bprlp, *bprhp;
{
	u_int32_t rate;

	rate = (2 * clock) / (16 * speed);
	if (rate & 1)
		rate = (rate >> 1) + 1;
	else
		rate = rate >> 1;

	if (rate > 0xffff || rate == 0)
		return (1);

	*bprlp = rate & 0xff;
	*bprhp = (rate >> 8) & 0xff;
	return (0);
}

int
sbppmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct spif_softc *sc = (struct spif_softc *)parent;

	return (aux == sbppmatch && sc->sc_bpps == NULL);
}

void
sbppattach(parent, dev, aux)
	struct device *parent, *dev;
	void *aux;
{
	struct spif_softc *sc = (struct spif_softc *)parent;
	struct sbpp_softc *psc = (struct sbpp_softc *)dev;
	int port;

	sc->sc_bpps = psc;

	for (port = 0; port < sc->sc_npar; port++) {
	}

	psc->sc_nports = port;
	printf(": %d port%s\n", port, port == 1 ? "" : "s");
}

int
sbppopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	return (ENXIO);
}

int
sbppclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	return (ENXIO);
}

int
spifppcintr(v)
	void *v;
{
	return (0);
}

int
sbppread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (sbpp_rw(dev, uio));
}

int
sbppwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (sbpp_rw(dev, uio));
}

int
sbpp_rw(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	return (ENXIO);
}

int
sbppselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	return (ENODEV);
}

int
sbppioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	int error;

	error = ENOTTY;

	return (error);
}
