/*	$OpenBSD: vx.c,v 1.29 2004/04/24 19:51:48 miod Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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

/* This card lives in D16 space */
#define	__BUS_SPACE_RESTRICT_D16__

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
#include <machine/psl.h>

#include <dev/cons.h>

#include <mvme88k/dev/vme.h>
#include <mvme88k/dev/vxreg.h>

#define splvx()	spltty()

struct vx_info {
	struct   tty *tty;
	u_char   vx_swflags;
	int      vx_linestatus;
	int      open;
	int      waiting;
	u_char   vx_speed;
	u_char   read_pending;
	struct   wring  *wringp;
	struct   rring  *rringp;
};

struct vxsoftc {
	struct device     sc_dev;
	struct evcnt      sc_intrcnt;
	struct vx_info  sc_info[NVXPORTS];
	struct vxreg    *vx_reg;
	vaddr_t		board_addr;
	struct channel    *channel;
	char              channel_number;
	struct packet     sc_bppwait_pkt;
	void              *sc_bppwait_pktp;
	struct intrhand   sc_ih_c;
	struct intrhand   sc_ih_s;
	int               sc_vec;
	struct envelope   *elist_head, *elist_tail;
	struct packet     *plist_head, *plist_tail;
};

int  vxmatch(struct device *, void *, void *);
void vxattach(struct device *, struct device *, void *);

struct cfattach vx_ca = {
	sizeof(struct vxsoftc), vxmatch, vxattach
};

struct cfdriver vx_cd = {
	NULL, "vx", DV_TTY
};

void	bpp_send(struct vxsoftc *, void *, int);
void	ccode(struct vxsoftc *, int, char);
int	create_channels(struct vxsoftc *);
void	create_free_queue(struct vxsoftc *);
short	dtr_ctl(struct vxsoftc *, int, int);
int	env_isvalid(struct envelope *);
struct envelope *find_status_packet(struct vxsoftc *, struct packet *);
short	flush_ctl(struct vxsoftc *, int, int);
struct envelope *get_cmd_tail(struct vxsoftc *);
void	*get_free_envelope(struct vxsoftc *);
void	*get_free_packet(struct vxsoftc *);
struct envelope *get_next_envelope(struct envelope *);
struct packet *get_packet(struct vxsoftc *, struct envelope *);
struct envelope *get_status_head(struct vxsoftc *);
void	put_free_envelope(struct vxsoftc *, void *);
void	put_free_packet(struct vxsoftc *, void *);
void	read_chars(struct vxsoftc *, int);
void	read_wakeup(struct vxsoftc *, int);
short	rts_ctl(struct vxsoftc *, int, int);
void	set_status_head(struct vxsoftc *, void *);
void	vx_break(struct vxsoftc *, int);
int	vx_ccparam(struct vxsoftc *, struct termios *, int);
int	vx_event(struct vxsoftc *, struct packet *);
void	vx_frame(struct vxsoftc *, int);
int	vx_init(struct vxsoftc *);
int	vx_intr(void *);
int	vx_mctl(dev_t, int, int);
void	vx_overflow(struct vxsoftc *, int, long *, u_char *);
int	vx_param(struct tty *, struct termios *);
int	vx_poll(struct vxsoftc *, struct packet *);
void	vxputc(struct vxsoftc *, int, u_char);
int	vx_sintr(struct vxsoftc *);
void	vxstart(struct tty *tp);
u_short	vxtspeed(int);
void	vx_unblock(struct tty *);

/* flags for bpp_send() */
#define	NOWAIT 0
#define	WAIT 1

#define VX_UNIT(x) (minor(x) / NVXPORTS)
#define VX_PORT(x) (minor(x) % NVXPORTS)

struct tty *
vxtty(dev_t dev)
{
	int unit, port;
	struct vxsoftc *sc;

	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *)vx_cd.cd_devs[unit]) == NULL) {
		return (NULL);
	}
	port = VX_PORT(dev);
	return sc->sc_info[port].tty;
}

int
vxmatch(struct device *parent, void *self, void *aux)
{
	struct vxreg *vx_reg;
	struct confargs *ca = aux;
	bus_space_tag_t iot = ca->ca_iot;
	bus_space_handle_t ioh;
	int rc;

	if (bus_space_map(iot, ca->ca_paddr, 0x10000, 0, &ioh) != 0)
		return 0;
	vx_reg = (struct vxreg *)bus_space_vaddr(iot, ioh);
	rc = badvaddr((vaddr_t)&vx_reg->ipc_cr, 1);
	bus_space_unmap(iot, ioh, 0x10000);

	return rc == 0;
}

void
vxattach(struct device *parent, struct device *self, void *aux)
{
	struct vxsoftc *sc = (struct vxsoftc *)self;
	struct confargs *ca = aux;
	bus_space_tag_t iot = ca->ca_iot;
	bus_space_handle_t ioh;

	if (ca->ca_vec < 0) {
		printf(": no more interrupts!\n");
		return;
	}
	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_TTY;

	if (bus_space_map(iot, ca->ca_paddr, 0x10000, 0, &ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}

	/* set up dual port memory and registers and init */
	sc->board_addr = (vaddr_t)bus_space_vaddr(iot, ioh);
	sc->vx_reg = (struct vxreg *)sc->board_addr;
	sc->channel = (struct channel *)(sc->board_addr + 0x0100);
	sc->sc_vec = ca->ca_vec;

	printf("\n");

	if (create_channels(sc) != 0) {
		printf("%s: failed to create channel %d\n",
		    sc->sc_dev.dv_xname, sc->channel->channel_number);
		return;
	}
	if (vx_init(sc) != 0) {
		printf("%s: failed to initialize\n", sc->sc_dev.dv_xname);
		return;
	}

	/* enable interrupts */
	sc->sc_ih_c.ih_fn = vx_intr;
	sc->sc_ih_c.ih_arg = sc;
	sc->sc_ih_c.ih_wantframe = 0;
	sc->sc_ih_c.ih_ipl = IPL_TTY;

	vmeintr_establish(ca->ca_vec, &sc->sc_ih_c);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
}

short
dtr_ctl(struct vxsoftc *sc, int port, int on)
{
	struct packet pkt;

	bzero(&pkt, sizeof(struct packet));
	pkt.command = CMD_IOCTL;
	pkt.ioctl_cmd_l = IOCTL_TCXONC;
	pkt.command_pipe_number = sc->channel_number;
	pkt.status_pipe_number = sc->channel_number;
	pkt.device_number = port;
	if (on) {
		pkt.ioctl_arg_l = 6;  /* assert DTR */
	} else {
		pkt.ioctl_arg_l = 7;  /* negate DTR */
	}
	bpp_send(sc, &pkt, NOWAIT);

	return (pkt.error_l);
}

short
rts_ctl(struct vxsoftc *sc, int port, int on)
{
	struct packet pkt;

	bzero(&pkt, sizeof(struct packet));
	pkt.command = CMD_IOCTL;
	pkt.ioctl_cmd_l = IOCTL_TCXONC;
	pkt.command_pipe_number = sc->channel_number;
	pkt.status_pipe_number = sc->channel_number;
	pkt.device_number = port;
	if (on) {
		pkt.ioctl_arg_l = 4;  /* assert RTS */
	} else {
		pkt.ioctl_arg_l = 5;  /* negate RTS */
	}
	bpp_send(sc, &pkt, NOWAIT);

	return (pkt.error_l);
}

#if 0
short
flush_ctl(struct vxsoftc *sc, int port, int which)
{
	struct packet pkt;

	bzero(&pkt, sizeof(struct packet));
	pkt.command = CMD_IOCTL;
	pkt.ioctl_cmd_l = IOCTL_TCFLSH;
	pkt.command_pipe_number = sc->channel_number;
	pkt.status_pipe_number = sc->channel_number;
	pkt.device_number = port;
	pkt.ioctl_arg_l = which; /* 0=input, 1=output, 2=both */
	bpp_send(sc, &pkt, NOWAIT);

	return (pkt.error_l);
}
#endif

int
vx_mctl(dev_t dev, int bits, int how)
{
	int s, unit, port;
	struct vxsoftc *sc;
	struct vx_info *vxt;
	u_char msvr;

	unit = VX_UNIT(dev);
	port = VX_PORT(dev);
	sc = (struct vxsoftc *)vx_cd.cd_devs[unit];
	vxt = &sc->sc_info[port];

	s = splvx();
	switch (how) {
	case DMSET:
		if (bits & TIOCM_RTS) {
			rts_ctl(sc, port, 1);
			vxt->vx_linestatus |= TIOCM_RTS;
		} else {
			rts_ctl(sc, port, 0);
			vxt->vx_linestatus &= ~TIOCM_RTS;
		}
		if (bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 1);
			vxt->vx_linestatus |= TIOCM_DTR;
		} else {
			dtr_ctl(sc, port, 0);
			vxt->vx_linestatus &= ~TIOCM_DTR;
		}
		break;
	case DMBIC:
		if (bits & TIOCM_RTS) {
			rts_ctl(sc, port, 0);
			vxt->vx_linestatus &= ~TIOCM_RTS;
		}
		if (bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 0);
			vxt->vx_linestatus &= ~TIOCM_DTR;
		}
		break;

	case DMBIS:
		if (bits & TIOCM_RTS) {
			rts_ctl(sc, port, 1);
			vxt->vx_linestatus |= TIOCM_RTS;
		}
		if (bits & TIOCM_DTR) {
			dtr_ctl(sc, port, 1);
			vxt->vx_linestatus |= TIOCM_DTR;
		}
		break;

	case DMGET:
		bits = 0;
		msvr = vxt->vx_linestatus;
		if (msvr & TIOCM_DSR) {
			bits |= TIOCM_DSR;
		}
		if (msvr & TIOCM_CD) {
			bits |= TIOCM_CD;
		}
		if (msvr & TIOCM_CTS) {
			bits |= TIOCM_CTS;
		}
		if (msvr & TIOCM_DTR) {
			bits |= TIOCM_DTR;
		}
		if (msvr & TIOCM_RTS) {
			bits |= TIOCM_RTS;
		}
		break;
	}

	splx(s);

#if 0
	bits = 0;
	bits |= TIOCM_DTR;
	bits |= TIOCM_RTS;
	bits |= TIOCM_CTS;
	bits |= TIOCM_CD;
	bits |= TIOCM_DSR;
#endif
	return (bits);
}

int
vxopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int s, unit, port;
	struct vx_info *vxt;
	struct vxsoftc *sc;
	struct tty *tp;
	struct packet opkt;
	u_short code;

	unit = VX_UNIT(dev);
	port = VX_PORT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	vxt = &sc->sc_info[port];

#if 0
	flush_ctl(sc, port, 2);
#endif

	bzero(&opkt, sizeof(struct packet));
	opkt.link = 0x33333333;	/* eye catcher */
	opkt.command_pipe_number = sc->channel_number;
	opkt.status_pipe_number = sc->channel_number;
	opkt.command = CMD_OPEN;
	opkt.device_number = port;

	bpp_send(sc, &opkt, WAIT);

	if (opkt.error_l) {
#ifdef DEBUG_VXT
		printf("unit %d, port %d, ", unit, port);
		printf("error = %d\n", opkt.error_l);
#endif
		return (ENXIO);
	}

	code = opkt.event_code;

	s = splvx();
	if (vxt->tty) {
		tp = vxt->tty;
	} else {
		tp = vxt->tty = ttymalloc();
	}

	/* set line status */
	tp->t_state |= TS_CARR_ON;
	if (code & E_DCD) {
		tp->t_state |= TS_CARR_ON;
		vxt->vx_linestatus |= TIOCM_CD;
	}
	if (code & E_DSR) {
		vxt->vx_linestatus |= TIOCM_DSR;
	}
	if (code & E_CTS) {
		vxt->vx_linestatus |= TIOCM_CTS;
	}

	tp->t_oproc = vxstart;
	tp->t_param = vx_param;
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
			tp->t_ispeed = tp->t_ospeed = TTYDEF_SPEED;
			tp->t_cflag = TTYDEF_CFLAG;
		}
		/*
		 * do these all the time
		 */
		if (vxt->vx_swflags & TIOCFLAG_CLOCAL)
			tp->t_cflag |= CLOCAL;
		if (vxt->vx_swflags & TIOCFLAG_CRTSCTS)
			tp->t_cflag |= CRTSCTS;
		if (vxt->vx_swflags & TIOCFLAG_MDMBUF)
			tp->t_cflag |= MDMBUF;
		vx_param(tp, &tp->t_termios);
		ttsetwater(tp);

		(void)vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMSET);

		tp->t_state |= TS_CARR_ON;
	} else if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
		splx(s);
		return (EBUSY);
	}

	/*
	 * Reset the tty pointer, as there could have been a dialout
	 * use of the tty with a dialin open waiting.
	 */
	tp->t_dev = dev;
	vxt->open = 1;
	splx(s);
	return (*linesw[tp->t_line].l_open)(dev, tp);
}

int
vx_param(struct tty *tp, struct termios *t)
{
	int unit, port;
	struct vxsoftc *sc;
	dev_t dev;

	dev = tp->t_dev;
	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	port = VX_PORT(dev);
	tp->t_ispeed = t->c_ispeed;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;
	vx_ccparam(sc, t, port);
	vx_unblock(tp);
	return 0;
}

int
vxclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int unit, port;
	struct tty *tp;
	struct vx_info *vxt;
	struct vxsoftc *sc;
	int s;
	struct packet cpkt;

	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	port = VX_PORT(dev);
	vxt = &sc->sc_info[port];
#if 0
	flush_ctl(sc, port, 2);	/* flush both input and output */
#endif

	tp = vxt->tty;
	(*linesw[tp->t_line].l_close)(tp, flag);

	s = splvx();

	if ((tp->t_cflag & HUPCL) != 0) {
		rts_ctl(sc, port, 0);
		dtr_ctl(sc, port, 0);
	}

	bzero(&cpkt, sizeof(struct packet));
	cpkt.link = 0x55555555;	/* eye catcher */
	cpkt.command_pipe_number = sc->channel_number;
	cpkt.status_pipe_number = sc->channel_number;
	cpkt.command = CMD_CLOSE;
	cpkt.device_number = port;
	bpp_send(sc, &cpkt, NOWAIT);

	vxt->open = 0;
	splx(s);
	ttyclose(tp);

	return (0);
}

void
read_wakeup(struct vxsoftc *sc, int port)
{
	struct packet rwp;
	struct vx_info *volatile vxt;

	vxt = &sc->sc_info[port];
	/*
	 * If we already have a read_wakeup paket
	 * for this port, do nothing.
	 */
	if (vxt->read_pending != 0)
		return;
	else
		vxt->read_pending = 1;

	bzero(&rwp, sizeof(struct packet));
	rwp.link = 0x11111111;	/* eye catcher */
	rwp.command_pipe_number = sc->channel_number;
	rwp.status_pipe_number = sc->channel_number;
	rwp.command = CMD_READW;
	rwp.device_number = port;

	/*
	 * Do not wait.  Characters will be transferred
	 * to (*linesw[tp->t_line].l_rint)(c, tp); by
	 * vx_intr()  (IPC will notify via interrupt)
	 */
	bpp_send(sc, &rwp, NOWAIT);
}

int
vxread(dev_t dev, struct uio *uio, int flag)
{
	int unit, port;
	struct tty *tp;
	struct vx_info *volatile vxt;
	struct vxsoftc *volatile sc;

	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	port = VX_PORT(dev);
	vxt = &sc->sc_info[port];
	tp = vxt->tty;
	if (!tp)
		return ENXIO;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

int
vxwrite(dev_t dev, struct uio *uio, int flag)
{
	int unit, port;
	struct tty *tp;
	struct vx_info *vxt;
	struct vxsoftc *sc;
	struct wring *wp;
	struct packet wwp;
	u_short get, put;

	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	port = VX_PORT(dev);
	vxt = &sc->sc_info[port];
	tp = vxt->tty;
	if (!tp)
		return ENXIO;

	wp = sc->sc_info[port].wringp;
	get = wp->get;
	put = wp->put;
	if ((put + 1) == get) {
		bzero(&wwp, sizeof(struct packet));
		wwp.link = 0x22222222;	/* eye catcher */
		wwp.command_pipe_number = sc->channel_number;
		wwp.status_pipe_number = sc->channel_number;
		wwp.command = CMD_WRITEW;
		wwp.device_number = port;
		bpp_send(sc, &wwp, WAIT);

		if (wwp.error_l != 0)
			return (ENXIO);
	}

	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

int
vxioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error;
	int unit, port;
	struct tty *tp;
	struct vx_info *vxt;
	struct vxsoftc *sc;

	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return (ENODEV);
	}
	port = VX_PORT(dev);
	vxt = &sc->sc_info[port];
	tp = vxt->tty;
	if (!tp)
		return ENXIO;

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
		vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIS);
		break;

	case TIOCCDTR:
		vx_mctl(dev, TIOCM_DTR | TIOCM_RTS, DMBIC);
		break;

	case TIOCMSET:
		vx_mctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		vx_mctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		vx_mctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = vx_mctl(dev, 0, DMGET);
		break;

	case TIOCGFLAGS:
		*(int *)data = vxt->vx_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p, 0);
		if (error != 0)
			return (EPERM);

		vxt->vx_swflags = *(int *)data;
		vxt->vx_swflags &= /* only allow valid flags */
		    (TIOCFLAG_SOFTCAR | TIOCFLAG_CLOCAL | TIOCFLAG_CRTSCTS);
		break;

	default:
		return (ENOTTY);
	}

	return 0;
}

int
vxstop(struct tty *tp, int flag)
{
	int s;

	s = splvx();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state & TS_TTSTOP) == 0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
	return 0;
}

void
vxputc(struct vxsoftc *sc, int port, u_char c)
{
	struct wring *wp;

	wp = sc->sc_info[port].wringp;
	wp->data[wp->put++ & (WRING_BUF_SIZE - 1)] = c;
	wp->put &= (WRING_BUF_SIZE - 1);
}

u_short
vxtspeed(int speed)
{
	switch (speed) {
	case B0:
		return VB0;
	case B50:
		return VB50;
	case B75:
		return VB75;
	case B110:
		return VB110;
	case B134:
		return VB134;
	case B150:
		return VB150;
	case B200:
		return VB200;
	case B300:
		return VB300;
	case B600:
		return VB600;
	case B1200:
		return VB1200;
	case B1800:
		return VB1800;
	case B2400:
		return VB2400;
	case B4800:
		return VB4800;
	case B9600:
		return VB9600;
	case B19200:
		return VB19200;
	case B38400:
		return VB38400;
	default:
		return VB9600;
	}
}

int
vx_ccparam(struct vxsoftc *sc, struct termios *par, int port)
{
	int imask = 0, s;
	int cflag;
	struct packet pkt;

	if (par->c_ospeed == 0) {
		s = splvx();
		/* disconnect, drop RTS DTR stop receiver */
		rts_ctl(sc, port, 0);
		dtr_ctl(sc, port, 0);
		splx(s);
		return (0xff);
	}

	bzero(&pkt, sizeof(struct packet));
	pkt.command = CMD_IOCTL;
	pkt.ioctl_cmd_l = IOCTL_TCGETA;
	pkt.command_pipe_number = sc->channel_number;
	pkt.status_pipe_number = sc->channel_number;
	pkt.device_number = port;
	bpp_send(sc, &pkt, WAIT);

	cflag = pkt.pb.tio.c_cflag;
	cflag |= vxtspeed(par->c_ospeed);

	switch (par->c_cflag & CSIZE) {
	case CS5:
		cflag |= VCS5;
		imask = 0x1F;
		break;
	case CS6:
		cflag |= VCS6;
		imask = 0x3F;
		break;
	case CS7:
		cflag |= VCS7;
		imask = 0x7F;
		break;
	default:
		cflag |= VCS8;
		imask = 0xFF;
	}

	if (par->c_cflag & PARENB) cflag |= VPARENB;
	else cflag &= ~VPARENB;
	if (par->c_cflag & PARODD) cflag |= VPARODD;
	else cflag &= ~VPARODD;
	if (par->c_cflag & CREAD) cflag |= VCREAD;
	else cflag &= ~VCREAD;
	if (par->c_cflag & CLOCAL) cflag |= VCLOCAL;
	else cflag &= ~VCLOCAL;
	if (par->c_cflag & HUPCL) cflag |= VHUPCL;
	else cflag &= ~VHUPCL;

	pkt.command = CMD_IOCTL;
	pkt.ioctl_cmd_l = IOCTL_TCSETA;
	pkt.command_pipe_number = sc->channel_number;
	pkt.status_pipe_number = sc->channel_number;
	pkt.device_number = port;
	pkt.pb.tio.c_cflag = cflag;
	bpp_send(sc, &pkt, WAIT);

	return imask;
}

void
vx_unblock(struct tty *tp)
{
	tp->t_state &= ~TS_FLUSH;
	if (tp->t_outq.c_cc != 0)
		vxstart(tp);
}

void
vxstart(struct tty *tp)
{
	dev_t dev;
	struct vxsoftc *sc;
	struct wring *wp;
	int cc, port, unit, s, cnt, i;
	u_short get, put;
	char buffer[WRING_BUF_SIZE];

	dev = tp->t_dev;
	port = VX_PORT(dev);
	unit = VX_UNIT(dev);
	if (unit >= vx_cd.cd_ndevs ||
	    (sc = (struct vxsoftc *) vx_cd.cd_devs[unit]) == NULL) {
		return;
	}

	if ((tp->t_state & TS_ISOPEN) == 0)
		return;

	s = splvx();
	if ((tp->t_state & (TS_TIMEOUT | TS_BUSY | TS_TTSTOP | TS_FLUSH)) == 0) {
		tp->t_state |= TS_BUSY;
		wp = sc->sc_info[port].wringp;
		get = wp->get;
		put = wp->put;
		cc = tp->t_outq.c_cc;
		while (cc > 0) {
			cnt = min(WRING_BUF_SIZE, cc);
			cnt = q_to_b(&tp->t_outq, buffer, cnt);
			buffer[cnt] = 0;
			for (i = 0; i < cnt; i++) {
				vxputc(sc, port, buffer[i]);
			}
			cc -= cnt;
		}
		tp->t_state &= ~TS_BUSY;
	}
	splx(s);
}

void
read_chars(struct vxsoftc *sc, int port)
{
	/*
	 * This routine is called by vx_intr() when there are
	 * characters in the read ring.  It will process one
	 * cooked line, put the chars in the line disipline ring,
	 * and then return.  The characters may then
	 * be read by vxread.
	 */
	struct vx_info *vxt;
	struct rring *rp;
	struct tty *tp;
	u_short get, put;
	int frame_count, i, open;
	char c;

	vxt = &sc->sc_info[port];
	tp = vxt->tty;
	rp = vxt->rringp;
	open = vxt->open;
	get = rp->get;
	put = rp->put;
#ifdef DEBUG_VXT
	printf("read_chars() get=%d, put=%d ", get, put);
	printf("open = %d ring at 0x%x\n", open, rp);
#endif
	while (get != put) {
		frame_count = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
		rp->get &= (RRING_BUF_SIZE - 1);
		for (i = 0; i < frame_count; i++) {
			c = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
			rp->get &= (RRING_BUF_SIZE - 1);
			if (open)
				(*linesw[tp->t_line].l_rint)(c, tp);
		}
		c = rp->data[rp->get++ & (RRING_BUF_SIZE - 1)];
		rp->get &= (RRING_BUF_SIZE - 1);
		if (!(c & DELIMITER)) {
			vx_frame(sc, port);
			break;
		} else {
			break;
		}
		get = rp->get;
		put = rp->put;
	}
	vxt->read_pending = 0;
	read_wakeup(sc, port);
}

void
ccode(struct vxsoftc *sc, int port, char c)
{
	struct vx_info *vxt;
	struct tty *tp;

	tp = vxt->tty;
	(*linesw[tp->t_line].l_rint)(c, tp);
}

int
vx_intr(void *arg)
{
	struct vxsoftc *sc = arg;
	struct envelope *envp, *next_envp;
	struct packet *pktp, pkt;
	int valid;
	short cmd;
	u_char port;

	sc->sc_intrcnt.ev_count++;

	while (env_isvalid(get_status_head(sc))) {
		pktp = get_packet(sc, get_status_head(sc));
		valid = env_isvalid(get_status_head(sc));
		cmd = pktp->command;
		port = pktp->device_number;
		/*
		 * If we are waiting on this packet, store the info
		 * so bpp_send can process the packet
		 */
		if (sc->sc_bppwait_pktp == pktp)
			d16_bcopy(pktp, &sc->sc_bppwait_pkt, sizeof(struct packet));

		d16_bcopy(pktp, &pkt, sizeof(struct packet));
		next_envp = get_next_envelope(get_status_head(sc));
		envp = get_status_head(sc);
		/* return envelope and packet to the free queues */
		put_free_envelope(sc, envp);
		put_free_packet(sc, pktp);
		/* mark new status pipe head pointer */
		set_status_head(sc, next_envp);
		/* if it was valid, process packet */
		switch (cmd) {
		case CMD_READW:
#ifdef DEBUG_VXT
			printf("READW Packet\n");
#endif
			read_chars(sc, port);
			break;
		case CMD_WRITEW:
#ifdef DEBUG_VXT
			printf("WRITEW Packet\n");  /* Still don't know XXXsmurph */
#endif
			break;
		case CMD_EVENT:
#ifdef DEBUG_VXT
			printf("EVENT Packet\n");
#endif
			vx_event(sc, &pkt);
			break;
		case CMD_PROCESSED:
#ifdef DEBUG_VXT
			printf("CMD_PROCESSED Packet\n");
#endif
			break;
		default:
#ifdef DEBUG_VXT
			printf("Other packet 0x%x\n", cmd);
#endif
			break;
		}
	}
	return 1;
}

int
vx_event(struct vxsoftc *sc, struct packet *evntp)
{
	u_short code = evntp->event_code;
	struct packet evnt;
	struct vx_info *vxt;

	vxt = &sc->sc_info[evntp->device_number];

	if (code & E_INTR) {
		ccode(sc, evntp->device_number, CINTR);
	}
	if (code & E_QUIT) {
		ccode(sc, evntp->device_number, CQUIT);
	}
	if (code & E_HUP) {
		rts_ctl(sc, evntp->device_number, 0);
		dtr_ctl(sc, evntp->device_number, 0);
	}
	if (code & E_DCD) {
		vxt->vx_linestatus |= TIOCM_CD;
	}
	if (code & E_DSR) {
		vxt->vx_linestatus |= TIOCM_DSR;
	}
	if (code & E_CTS) {
		vxt->vx_linestatus |= TIOCM_CTS;
	}
	if (code & E_LOST_DCD) {
		vxt->vx_linestatus &= ~TIOCM_CD;
	}
	if (code & E_LOST_DSR) {
		vxt->vx_linestatus &= ~TIOCM_DSR;
	}
	if (code & E_LOST_CTS) {
		vxt->vx_linestatus &= ~TIOCM_CTS;
	}
	if (code & E_PR_FAULT) {
		/* do something... */
	}
	if (code & E_PR_POUT) {
		/* do something... */
	}
	if (code & E_PR_SELECT) {
		/* do something... */
	}
	if (code & E_SWITCH) {
		/* do something... */
	}
	if (code & E_BREAK) {
		vx_break(sc, evntp->device_number);
	}

	/* send an event packet back to the device */
	bzero(&evnt, sizeof(struct packet));
	evnt.command = CMD_EVENT;
	evnt.device_number = evntp->device_number;
	evnt.command_pipe_number = sc->channel_number;
	/* return status on same channel */
	evnt.status_pipe_number = sc->channel_number;
	/* send packet to the firmware */
	bpp_send(sc, &evnt, NOWAIT);

	return 1;
}

void
vx_overflow(struct vxsoftc *sc, int port, long *ptime, u_char *msg)
{
	log(LOG_WARNING, "%s port %d: overrun\n",
	    sc->sc_dev.dv_xname, port);
}

void
vx_frame(struct vxsoftc *sc, int port)
{
	log(LOG_WARNING, "%s port %d: frame error\n",
	    sc->sc_dev.dv_xname, port);
}

void
vx_break(struct vxsoftc *sc, int port)
{
	/*
	 * No need to check for a ddb break, as the console can never be on
	 * this hardware.
	 */
	log(LOG_WARNING, "%s port %d: break detected\n",
	    sc->sc_dev.dv_xname, port);
}

/*
 *	Initialization and Buffered Pipe Protocol (BPP) code
 */

void
create_free_queue(struct vxsoftc *sc)
{
	int i;
	struct envelope *envp, env;
	struct packet *pktp, pkt;

	envp = (struct envelope *)ENVELOPE_AREA;
	sc->elist_head = envp;
	for (i = 0; i < NENVELOPES; i++) {
		bzero(&env, sizeof(struct envelope));
		if (i == NENVELOPES - 1)
			env.link = NULL;
		else
			env.link = (u_long)envp + sizeof(struct envelope);
		env.packet_ptr = NULL;
		env.valid_flag = 0;
		d16_bcopy(&env, envp, sizeof(struct envelope));
		envp++;
	}
	sc->elist_tail = --envp;

	pktp = (struct packet *)PACKET_AREA;
	sc->plist_head = pktp;
	for (i = 0; i < NPACKETS; i++) {
		bzero(&pkt, sizeof(struct packet));
		if (i == NPACKETS - 1)
			pkt.link = NULL;
		else
			pkt.link = (u_long)pktp + sizeof(struct packet);
		d16_bcopy(&pkt, pktp, sizeof(struct packet));
		pktp++;
	}
	sc->plist_tail = --pktp;
}

void *
get_free_envelope(struct vxsoftc *sc)
{
	void *envp;
	u_long link;

	envp = sc->elist_head;
	/* pick envelope next pointer from the envelope itself */
	d16_bcopy((const void *)&sc->elist_head->link, &link, sizeof link);
	sc->elist_head = (struct envelope *)link;
	d16_bzero(envp, sizeof(struct envelope));

	return envp;
}

void
put_free_envelope(struct vxsoftc *sc, void *ep)
{
	struct envelope *envp = (struct envelope *)ep;
	u_long link;

#if 0
	d16_bzero(envp, sizeof(struct envelope));
#endif
	/* put envelope next pointer in the envelope itself */
	link = (u_long)envp;
	d16_bcopy(&link, (void *)&sc->elist_tail->link, sizeof link);
	d16_bzero((void *)&envp->link, sizeof envp->link);

	sc->elist_tail = envp;
}

void *
get_free_packet(struct vxsoftc *sc)
{
	struct packet *pktp;
	u_long link;

	pktp = sc->plist_head;
	/* pick packet next pointer from the packet itself */
	d16_bcopy((const void *)&sc->plist_head->link, &link, sizeof link);
	sc->plist_head = (struct packet *)link;
	d16_bzero(pktp, sizeof(struct packet));

	return pktp;
}

void
put_free_packet(struct vxsoftc *sc, void *pp)
{
	struct packet *pktp = (struct packet *)pp;
	u_long link;

#if 0
	d16_bzero(pktp, sizeof(struct packet));
#endif
	pktp->command = CMD_PROCESSED;
	/* put packet next pointer in the packet itself */
	link = (u_long)pktp;
	d16_bcopy(&link, (void *)&sc->plist_tail->link, sizeof link);
	d16_bzero((void *)&pktp->link, sizeof pktp->link);

	sc->plist_tail = pktp;
}

/*
 * This is the nitty gritty.  All the rest if this code
 * was hell to come by.  Getting this right from the
 * Moto manual took *time*!
 */
int
create_channels(struct vxsoftc *sc)
{
	struct envelope *envp;
	u_short status;
	u_short tas;
	struct vxreg *ipc_csr;

	ipc_csr = sc->vx_reg;
	/* wait for busy bit to clear */
	while ((ipc_csr->ipc_cr & IPC_CR_BUSY)) ;

	create_free_queue(sc);
	/* set up channel header. we only want one */
	tas = ipc_csr->ipc_tas;
	while (!(tas & IPC_TAS_VALID_STATUS)) {
		envp = get_free_envelope(sc);
		sc->channel->command_pipe_head_ptr_h = HI(envp);
		sc->channel->command_pipe_head_ptr_l = LO(envp);
		sc->channel->command_pipe_tail_ptr_h =
		    sc->channel->command_pipe_head_ptr_h;
		sc->channel->command_pipe_tail_ptr_l =
		    sc->channel->command_pipe_head_ptr_l;
		envp = get_free_envelope(sc);
		sc->channel->status_pipe_head_ptr_h = HI(envp);
		sc->channel->status_pipe_head_ptr_l = LO(envp);
		sc->channel->status_pipe_tail_ptr_h =
		    sc->channel->status_pipe_head_ptr_h;
		sc->channel->status_pipe_tail_ptr_l =
		    sc->channel->status_pipe_head_ptr_l;
		sc->channel->interrupt_level = IPL_TTY;
		sc->channel->interrupt_vec = sc->sc_vec;
		sc->channel->channel_priority = 0;
		sc->channel->channel_number = 0;
		sc->channel->valid = 1;
		sc->channel->address_modifier = 0x8d; /* A32/D16 supervisor data access */
		sc->channel->datasize = 0; /* 32 bit data mode */

		/* loop until TAS bit is zero */
		while ((ipc_csr->ipc_tas & IPC_TAS_TAS)) ;
		ipc_csr->ipc_tas |= IPC_TAS_TAS;
		/* load address of channel header */
		ipc_csr->ipc_addrh = HI(sc->channel);
		ipc_csr->ipc_addrl = LO(sc->channel);
		/* load address modifier reg (supervisor data access) */
		ipc_csr->ipc_amr = 0x8d;
		/* load tas with create channel command */
		ipc_csr->ipc_tas |= IPC_CSR_CREATE;
		/* set vaild command bit */
		ipc_csr->ipc_tas |= IPC_TAS_VALID_CMD;
		/* notify IPC of the CSR command */
		ipc_csr->ipc_cr |= IPC_CR_ATTEN;

		/* loop until IPC sets valid status bit */
		delay(5000);
		tas = ipc_csr->ipc_tas;
	}

	/* save the status */
	status = ipc_csr->ipc_sr;
	/* set COMMAND COMPLETE bit */
	ipc_csr->ipc_tas |= IPC_TAS_COMPLETE;
	/* notify IPC that we are through */
	ipc_csr->ipc_cr |= IPC_CR_ATTEN;
	/* check and see if the channel was created */
	if (!status && sc->channel->valid) {
		sc->channel_number = sc->channel->channel_number;
		printf("%s: created channel %d\n", sc->sc_dev.dv_xname,
		       sc->channel->channel_number);
		return 0;
	} else {
		switch (status) {
		case 0x0000:
			printf("%s: channel not valid\n",
			       sc->sc_dev.dv_xname);
			break;
		case 0xFFFF:
			printf("%s: invalid CSR command\n",
			       sc->sc_dev.dv_xname);
			break;
		case 0xC000:
			printf("%s: could not read channel structure\n",
			       sc->sc_dev.dv_xname);
			break;
		case 0x8000:
			printf("%s: could not write channel structure\n",
			       sc->sc_dev.dv_xname);
			break;
		default:
			printf("%s: unknown IPC CSR command error 0x%x\n",
			       sc->sc_dev.dv_xname, status);
			break;
		}
		return 1;
	}
}

struct envelope *
get_next_envelope(struct envelope *thisenv)
{
	u_long ptr;

	d16_bcopy((const void*)&thisenv->link, &ptr, sizeof ptr);

	return ((struct envelope *)ptr);
}

int
env_isvalid(struct envelope *thisenv)
{
	return (int)thisenv->valid_flag;
}

struct envelope *
get_cmd_tail(struct vxsoftc *sc)
{
	unsigned long retaddr;

	retaddr = (unsigned long)sc->vx_reg;
	retaddr += sc->channel->command_pipe_tail_ptr_l;
	return ((struct envelope *)retaddr);
}

struct envelope *
get_status_head(struct vxsoftc *sc)
{
	unsigned long retaddr;

	retaddr = (unsigned long)sc->vx_reg;
	retaddr += sc->channel->status_pipe_head_ptr_l;
	return ((struct envelope *)retaddr);
}

void
set_status_head(struct vxsoftc *sc, void *envp)
{
	sc->channel->status_pipe_head_ptr_h = HI(envp);
	sc->channel->status_pipe_head_ptr_l = LO(envp);
}

struct packet *
get_packet(struct vxsoftc *sc, struct envelope *thisenv)
{
	u_long baseaddr;

	if (thisenv == NULL)
		return NULL;

	/*
	 * packet ptr returned on status pipe is only last two bytes
	 * so we must supply the full address based on the board address.
	 * This also works for all envelopes because every address is an
	 * offset to the board address.
	 */
	d16_bcopy((const void *)&thisenv->packet_ptr, &baseaddr, sizeof baseaddr);
	baseaddr |= (u_long)sc->vx_reg;

	return ((struct packet *)baseaddr);
}

/*
 *	Send a command via BPP
 */
void
bpp_send(struct vxsoftc *sc, void *pkt, int wait_flag)
{
	struct envelope *envp;
	struct packet *pktp;
	u_long ptr;

	/* load up packet in dual port mem */
	pktp = get_free_packet(sc);
	d16_bcopy(pkt, pktp, sizeof(struct packet));

	envp = get_cmd_tail(sc);
	ptr = (unsigned long)get_free_envelope(sc); /* put a NULL env on the tail */
	d16_bcopy(&ptr, (void *)&envp->link, sizeof envp->link);
	sc->channel->command_pipe_tail_ptr_h = HI(ptr);
	sc->channel->command_pipe_tail_ptr_l = LO(ptr);
	ptr = (u_long)pktp;
	d16_bcopy(&ptr, (void *)&envp->packet_ptr, sizeof envp->packet_ptr);
	envp->valid_flag = 1;

	sc->vx_reg->ipc_cr |= IPC_CR_ATTEN;

	/* wait for a packet to return */
	if (wait_flag != NOWAIT) {
		while (pktp->command != CMD_PROCESSED) {
#ifdef DEBUG_VXT
			printf("Polling for packet 0x%x in envelope 0x%x...\n", pktp, envp);
#endif
			vx_intr(sc);
			delay(5000);
		}
		d16_bcopy(pktp, pkt, sizeof(struct packet));
	}
}

/*
 *	BPP commands
 */

int
vx_init(struct vxsoftc *sc)
{
	int i;
	struct init_info *infp, inf;
	struct wring *wringp;
	struct rring *rringp;
	struct termio def_termio;
	struct packet init;
	struct packet evnt;

	bzero(&def_termio, sizeof(struct termio));
	/* init wait queue */
	d16_bzero(&sc->sc_bppwait_pkt, sizeof(struct packet));
	sc->sc_bppwait_pktp = NULL;
	/* set up init_info array */
	wringp = (struct wring *)WRING_AREA;
	rringp = (struct rring *)RRING_AREA;
	infp = (struct init_info *)INIT_INFO_AREA;
	for (i = 0; i < NVXPORTS; i++) {
		bzero(&inf, sizeof(struct init_info));
		inf.write_ring_ptr_h = HI(wringp);
		inf.write_ring_ptr_l = LO(wringp);
		sc->sc_info[i].wringp = wringp;
		inf.read_ring_ptr_h = HI(rringp);
		inf.read_ring_ptr_l = LO(rringp);
		sc->sc_info[i].rringp = rringp;
#ifdef DEBUG_VXT
		printf("write at 0x%8x, read at 0x%8x\n", wringp, rringp);
#endif
		inf.write_ring_size = WRING_DATA_SIZE;
		inf.read_ring_size = RRING_DATA_SIZE;
		inf.def_termio.c_iflag = VBRKINT;
		inf.def_termio.c_oflag = 0;
		inf.def_termio.c_cflag = (VB9600 | VCS8);

		inf.def_termio.c_lflag = VISIG; /* enable signal processing */
		inf.def_termio.c_line = 1; /* raw line discipline */
		inf.def_termio.c_cc[0] = CINTR;
		inf.def_termio.c_cc[1] = CQUIT;
		inf.def_termio.c_cc[2] = CERASE;
		inf.def_termio.c_cc[3] = CKILL;
		inf.def_termio.c_cc[4] = 20;
		inf.def_termio.c_cc[5] = 2;
		inf.reserved1 = 0;  /* Must Be Zero */
		inf.reserved2 = 0;
		inf.reserved3 = 0;
		inf.reserved4 = 0;
		d16_bcopy(&inf, infp, sizeof(struct init_info));
		wringp++; rringp++; infp++;
	}
	/* set up init_packet */
	bzero(&init, sizeof(struct packet));
	init.link = 0x12345678;	/* eye catcher */
	init.command = CMD_INIT;
	init.command_pipe_number = sc->channel_number;
	/* return status on the same channel */
	init.status_pipe_number = sc->channel_number;
	init.interrupt_level = IPL_TTY;
	init.interrupt_vec = sc->sc_vec;
	init.init_info_ptr_h = HI(INIT_INFO_AREA);
	init.init_info_ptr_l = LO(INIT_INFO_AREA);

	/* send packet to the firmware and wait for completion */
	bpp_send(sc, &init, WAIT);
	if (init.error_l != 0)
		return init.error_l;

	/* send one event packet to each device */
	for (i = 0; i < NVXPORTS; i++) {
		bzero(&evnt, sizeof(struct packet));
		evnt.command = CMD_EVENT;
		evnt.device_number = i;
		evnt.command_pipe_number = sc->channel_number;
		/* return status on same channel */
		evnt.status_pipe_number = sc->channel_number;
		/* send packet to the firmware */
		bpp_send(sc, &evnt, NOWAIT);
	}
	return 0;
}
