/*	$OpenBSD: ucom.c,v 1.4 2000/07/04 11:44:22 fgsch Exp $ */
/*	$NetBSD: ucom.c,v 1.22 2000/04/14 14:21:55 augustss Exp $	*/

/*
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This code is very heavily based on the 16550 driver, com.c.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/poll.h>

#include <dev/usb/usb.h>

#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ucomvar.h>

#include "ucom.h"

#if NUCOM > 0

#ifdef UCOM_DEBUG
#define DPRINTFN(n, x)	if (ucomdebug > (n)) logprintf x
int ucomdebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define	UCOMUNIT_MASK		0x3ffff
#define	UCOMDIALOUT_MASK	0x80000
#define	UCOMCALLUNIT_MASK	0x40000

#define	UCOMUNIT(x)		(minor(x) & UCOMUNIT_MASK)
#define	UCOMDIALOUT(x)		(minor(x) & UCOMDIALOUT_MASK)
#define	UCOMCALLUNIT(x)		(minor(x) & UCOMCALLUNIT_MASK)

struct ucom_softc {
	USBBASEDEVICE		sc_dev;		/* base device */

	usbd_device_handle	sc_udev;	/* USB device */

	usbd_interface_handle	sc_iface;	/* data interface */

	int			sc_bulkin_no;	/* bulk in endpoint address */
	usbd_pipe_handle	sc_bulkin_pipe;	/* bulk in pipe */
	usbd_xfer_handle	sc_ixfer;	/* read request */
	u_char			*sc_ibuf;	/* read buffer */
	u_int			sc_ibufsize;	/* read buffer size */
	u_int			sc_ibufsizepad;	/* read buffer size padded */

	int			sc_bulkout_no;	/* bulk out endpoint address */
	usbd_pipe_handle	sc_bulkout_pipe;/* bulk out pipe */
	usbd_xfer_handle	sc_oxfer;	/* write request */
	u_char			*sc_obuf;	/* write buffer */
	u_int			sc_obufsize;	/* write buffer size */
	u_int			sc_obufsizepad;	/* write buffer size padded */

	struct ucom_methods     *sc_methods;
	void                    *sc_parent;
	int			sc_portno;

	struct tty		*sc_tty;	/* our tty */
	u_char			sc_lsr;
	u_char			sc_msr;
	u_char			sc_mcr;
	u_char			sc_tx_stopped;
	int			sc_swflags;

	u_char			sc_opening;	/* lock during open */
	int			sc_refcnt;
	u_char			sc_dying;	/* disconnecting */
};

cdev_decl(ucom);

Static void	ucom_cleanup	__P((struct ucom_softc *));
Static void	ucom_hwiflow	__P((struct ucom_softc *));
Static int	ucomparam	__P((struct tty *, struct termios *));
Static void	ucomstart	__P((struct tty *));
Static void	ucom_shutdown	__P((struct ucom_softc *));
Static int	ucom_do_ioctl	__P((struct ucom_softc *, u_long, caddr_t,
				     int, struct proc *));
Static void	ucom_dtr	__P((struct ucom_softc *, int));
Static void	ucom_rts	__P((struct ucom_softc *, int));
Static void	ucom_break	__P((struct ucom_softc *, int));
Static usbd_status ucomstartread __P((struct ucom_softc *));
Static void	ucomreadcb	__P((usbd_xfer_handle, usbd_private_handle, 
				     usbd_status status));
Static void	ucomwritecb	__P((usbd_xfer_handle, usbd_private_handle, 
				     usbd_status status));
Static void	tiocm_to_ucom	__P((struct ucom_softc *, int, int));
Static int	ucom_to_tiocm	__P((struct ucom_softc *));

USB_DECLARE_DRIVER(ucom);

USB_MATCH(ucom)
{
	return (1);
}

USB_ATTACH(ucom)
{
	struct ucom_softc *sc = (struct ucom_softc *)self;
	struct ucom_attach_args *uca = aux;
	struct tty *tp;

	if (uca->portno != UCOM_UNK_PORTNO)
		printf(": portno %d", uca->portno);
	printf("\n");

	sc->sc_udev = uca->device;
	sc->sc_iface = uca->iface;
	sc->sc_bulkout_no = uca->bulkout;
	sc->sc_bulkin_no = uca->bulkin;
	sc->sc_ibufsize = uca->ibufsize;
	sc->sc_ibufsizepad = uca->ibufsizepad;
	sc->sc_obufsize = uca->obufsize;
	sc->sc_obufsizepad = uca->obufsizepad;
	sc->sc_methods = uca->methods;
	sc->sc_parent = uca->arg;
	sc->sc_portno = uca->portno;

	tp = ttymalloc();
	tp->t_oproc = ucomstart;
	tp->t_param = ucomparam;
	sc->sc_tty = tp;

	DPRINTF(("ucom_attach: tty_attach %p\n", tp));
	tty_attach(tp);

	USB_ATTACH_SUCCESS_RETURN;
}

USB_DETACH(ucom)
{
	struct ucom_softc *sc = (struct ucom_softc *)self;
	int maj, mn;
	int s;

	DPRINTF(("ucom_detach: sc=%p flags=%d tp=%p\n", 
		 sc, flags, sc->sc_tty));

	sc->sc_dying = 1;

#ifdef DIAGNOSTIC
	if (sc->sc_tty == NULL) {
		DPRINTF(("ucom_detach: no tty\n"));
		return (0);
	}
#endif

	s = splusb();
	if (--sc->sc_refcnt >= 0) {
		/* Wake everyone.. how? */
		/* Wait for processes to go away. */
		usb_detach_wait(USBDEV(sc->sc_dev));
	}
	splx(s);

	/* locate the major number */
	for (maj = 0; maj < nchrdev; maj++)
		if (cdevsw[maj].d_open == ucomopen)
			break;

	/* Nuke the vnodes for any open instances. */
	mn = self->dv_unit;
	DPRINTF(("ucom_detach: maj=%d mn=%d\n", maj, mn));
	vdevgone(maj, mn, mn, VCHR);
	vdevgone(maj, mn | UCOMDIALOUT_MASK, mn | UCOMDIALOUT_MASK, VCHR);
	vdevgone(maj, mn | UCOMCALLUNIT_MASK, mn | UCOMCALLUNIT_MASK, VCHR);

	/* Detach and free the tty. */
	tty_detach(sc->sc_tty);
	ttyfree(sc->sc_tty);
	sc->sc_tty = 0;

	return (0);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
ucom_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct ucom_softc *sc = (struct ucom_softc *)self;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		sc->sc_dying = 1;
		break;
	}
	return (0);
}
#endif

void
ucom_shutdown(sc)
	struct ucom_softc *sc;
{
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucom_shutdown\n"));
	/*
	 * Hang up if necessary.  Wait a bit, so the other side has time to
	 * notice even if we immediately open the port again.
	 */
	if (ISSET(tp->t_cflag, HUPCL)) {
		ucom_dtr(sc, 0);
		(void)tsleep(sc, TTIPRI, ttclos, hz);
	}
}

int
ucomopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = UCOMUNIT(dev);
	usbd_status err;
	struct ucom_softc *sc;
	struct tty *tp;
	int s;
	int error;
 
	if (unit >= ucom_cd.cd_ndevs)
		return (ENXIO);
	sc = ucom_cd.cd_devs[unit];
	if (sc == NULL)
		return (ENXIO);

	if (sc->sc_dying)
		return (EIO);

	if (ISSET(sc->sc_dev.dv_flags, DVF_ACTIVE) == 0)
		return (ENXIO);

	tp = sc->sc_tty;

	DPRINTF(("ucomopen: unit=%d, tp=%p\n", unit, tp));

	if (ISSET(tp->t_state, TS_ISOPEN) &&
	    ISSET(tp->t_state, TS_XCLUDE) &&
	    p->p_ucred->cr_uid != 0)
		return (EBUSY);

	s = spltty();

	/*
	 * Do the following iff this is a first open.
	 */
	while (sc->sc_opening)
		tsleep(&sc->sc_opening, PRIBIO, "ucomop", 0);

	if (sc->sc_dying) {
		splx(s);
		return (EIO);
	}
	sc->sc_opening = 1;
	
#if defined(__NetBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#else
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		struct termios t;

		tp->t_dev = dev;

		if (sc->sc_methods->ucom_open != NULL) {
			error = sc->sc_methods->ucom_open(sc->sc_parent,
							  sc->sc_portno);
			if (error) {
				ucom_cleanup(sc);
				return (error);
			}
		}

		ucom_status_change(sc);

		/*
		 * Initialize the termios status to the defaults.  Add in the
		 * sticky bits from TIOCSFLAGS.
		 */
		t.c_ispeed = 0;
		t.c_ospeed = TTYDEF_SPEED;
		t.c_cflag = TTYDEF_CFLAG;
		if (ISSET(sc->sc_swflags, TIOCFLAG_CLOCAL))
			SET(t.c_cflag, CLOCAL);
		if (ISSET(sc->sc_swflags, TIOCFLAG_CRTSCTS))
			SET(t.c_cflag, CRTSCTS);
		if (ISSET(sc->sc_swflags, TIOCFLAG_MDMBUF))
			SET(t.c_cflag, MDMBUF);
		/* Make sure ucomparam() will do something. */
		tp->t_ospeed = 0;
		(void) ucomparam(tp, &t);
		tp->t_iflag = TTYDEF_IFLAG;
		tp->t_oflag = TTYDEF_OFLAG;
		tp->t_lflag = TTYDEF_LFLAG;
		ttychars(tp);
		ttsetwater(tp);

		/*
		 * Turn on DTR.  We must always do this, even if carrier is not
		 * present, because otherwise we'd have to use TIOCSDTR
		 * immediately after setting CLOCAL, which applications do not
		 * expect.  We always assert DTR while the device is open
		 * unless explicitly requested to deassert it.
		 */
		ucom_dtr(sc, 1);

		// XXX CLR(sc->sc_rx_flags, RX_ANY_BLOCK);
		ucom_hwiflow(sc);

		DPRINTF(("ucomopen: open pipes in=%d out=%d\n",
			 sc->sc_bulkin_no, sc->sc_bulkout_no));

		/* Open the bulk pipes */
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkin_no, 0,
				     &sc->sc_bulkin_pipe);
		if (err) {
			DPRINTF(("%s: open bulk out error (addr %d), err=%s\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkin_no, 
				 usbd_errstr(err)));
			return (EIO);
		}
		err = usbd_open_pipe(sc->sc_iface, sc->sc_bulkout_no,
				     USBD_EXCLUSIVE_USE, &sc->sc_bulkout_pipe);
		if (err) {
			DPRINTF(("%s: open bulk in error (addr %d), err=%s\n",
				 USBDEVNAME(sc->sc_dev), sc->sc_bulkout_no,
				 usbd_errstr(err)));
			usbd_close_pipe(sc->sc_bulkin_pipe);
			return (EIO);
		}
		
		/* Allocate a request and an input buffer and start reading. */
		sc->sc_ixfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_ixfer == NULL) {
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}
		sc->sc_ibuf = usbd_alloc_buffer(sc->sc_ixfer,
						sc->sc_ibufsizepad);
		if (sc->sc_ibuf == NULL) {
			usbd_free_xfer(sc->sc_ixfer);
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}

		sc->sc_oxfer = usbd_alloc_xfer(sc->sc_udev);
		if (sc->sc_oxfer == NULL) {
			usbd_free_xfer(sc->sc_ixfer);
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}
		sc->sc_obuf = usbd_alloc_buffer(sc->sc_oxfer,
						sc->sc_obufsizepad);
		if (sc->sc_obuf == NULL) {
			usbd_free_xfer(sc->sc_oxfer);
			usbd_free_xfer(sc->sc_ixfer);
			usbd_close_pipe(sc->sc_bulkin_pipe);
			usbd_close_pipe(sc->sc_bulkout_pipe);
			return (ENOMEM);
		}

		ucomstartread(sc);
	}
	sc->sc_opening = 0;
	wakeup(&sc->sc_opening);
	splx(s);

#if defined(__NetBSD__)
	error = ttyopen(tp, UCOMDIALOUT(dev), ISSET(flag, O_NONBLOCK));
#else
	error = ttyopen(UCOMDIALOUT(dev), tp);
#endif
	if (error)
		goto bad;

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	if (error)
		goto bad;

	return (0);

bad:
#if defined(__NetBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#else
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		/*
		 * We failed to open the device, and nobody else had it opened.
		 * Clean up the state as appropriate.
		 */
		ucom_cleanup(sc);
	}

	return (error);
}

int
ucomclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	DPRINTF(("ucomclose: unit=%d\n", UCOMUNIT(dev)));
	if (!ISSET(tp->t_state, TS_ISOPEN))
		return (0);

	sc->sc_refcnt++;

	(*linesw[tp->t_line].l_close)(tp, flag);
	ttyclose(tp);

#if defined(__NetBSD__)
	if (!ISSET(tp->t_state, TS_ISOPEN) && tp->t_wopen == 0) {
#else
	if (!ISSET(tp->t_state, TS_ISOPEN)) {
#endif
		/*
		 * Although we got a last close, the device may still be in
		 * use; e.g. if this was the dialout node, and there are still
		 * processes waiting for carrier on the non-dialout node.
		 */
		ucom_cleanup(sc);
	}

	if (sc->sc_methods->ucom_close != NULL)
		sc->sc_methods->ucom_close(sc->sc_parent, sc->sc_portno);

	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));

	return (0);
}
 
int
ucomread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;

	if (sc->sc_dying)
		return (EIO);
 
	sc->sc_refcnt++;
	error = ((*linesw[tp->t_line].l_read)(tp, uio, flag));
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}
 
int
ucomwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(dev)];
	struct tty *tp = sc->sc_tty;
	int error;

	if (sc->sc_dying)
		return (EIO);
 
	sc->sc_refcnt++;
	error = ((*linesw[tp->t_line].l_write)(tp, uio, flag));
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

struct tty *
ucomtty(dev)
	dev_t dev;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(dev)];
	struct tty *tp = sc->sc_tty;

	return (tp);
}

int
ucomioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(dev)];
	int error;

	sc->sc_refcnt++;
	error = ucom_do_ioctl(sc, cmd, data, flag, p);
	if (--sc->sc_refcnt < 0)
		usb_detach_wakeup(USBDEV(sc->sc_dev));
	return (error);
}

Static int
ucom_do_ioctl(sc, cmd, data, flag, p)
	struct ucom_softc *sc;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct tty *tp = sc->sc_tty;
	int error;
	int s;

	if (sc->sc_dying)
		return (EIO);
 
	DPRINTF(("ucomioctl: cmd=0x%08lx\n", cmd));

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	error = ttioctl(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);

	if (sc->sc_methods->ucom_ioctl != NULL) {
		error = sc->sc_methods->ucom_ioctl(sc->sc_parent,
			    sc->sc_portno, cmd, data, flag, p);
		if (error >= 0)
			return (error);
	}

	error = 0;

	DPRINTF(("ucomioctl: our cmd=0x%08lx\n", cmd));
	s = spltty();

	switch (cmd) {
	case TIOCSBRK:
		ucom_break(sc, 1);
		break;

	case TIOCCBRK:
		ucom_break(sc, 0);
		break;

	case TIOCSDTR:
		ucom_dtr(sc, 1);
		break;

	case TIOCCDTR:
		ucom_dtr(sc, 0);
		break;

	case TIOCGFLAGS:
		*(int *)data = sc->sc_swflags;
		break;

	case TIOCSFLAGS:
		error = suser(p->p_ucred, &p->p_acflag); 
		if (error)
			break;
		sc->sc_swflags = *(int *)data;
		break;

	case TIOCMSET:
	case TIOCMBIS:
	case TIOCMBIC:
		tiocm_to_ucom(sc, cmd, *(int *)data);
		break;

	case TIOCMGET:
		*(int *)data = ucom_to_tiocm(sc);
		break;

	default:
		error = ENOTTY;
		break;
	}

	splx(s);

	return (error);
}

Static void
tiocm_to_ucom(sc, how, ttybits)
	struct ucom_softc *sc;
	int how, ttybits;
{
	u_char combits;

	combits = 0;
	if (ISSET(ttybits, TIOCM_DTR))
		SET(combits, UMCR_DTR);
	if (ISSET(ttybits, TIOCM_RTS))
		SET(combits, UMCR_RTS);
 
	switch (how) {
	case TIOCMBIC:
		CLR(sc->sc_mcr, combits);
		break;

	case TIOCMBIS:
		SET(sc->sc_mcr, combits);
		break;

	case TIOCMSET:
		CLR(sc->sc_mcr, UMCR_DTR | UMCR_RTS);
		SET(sc->sc_mcr, combits);
		break;
	}

	ucom_dtr(sc, (sc->sc_mcr & UMCR_DTR) != 0);
	ucom_rts(sc, (sc->sc_mcr & UMCR_RTS) != 0);
}

Static int
ucom_to_tiocm(sc)
	struct ucom_softc *sc;
{
	u_char combits;
	int ttybits = 0;

	combits = sc->sc_mcr;
	if (ISSET(combits, UMCR_DTR))
		SET(ttybits, TIOCM_DTR);
	if (ISSET(combits, UMCR_RTS))
		SET(ttybits, TIOCM_RTS);

	combits = sc->sc_msr;
	if (ISSET(combits, UMSR_DCD))
		SET(ttybits, TIOCM_CD);
	if (ISSET(combits, UMSR_CTS))
		SET(ttybits, TIOCM_CTS);
	if (ISSET(combits, UMSR_DSR))
		SET(ttybits, TIOCM_DSR);
	if (ISSET(combits, UMSR_RI | UMSR_TERI))
		SET(ttybits, TIOCM_RI);

#if 0
XXX;
	if (sc->sc_ier != 0)
		SET(ttybits, TIOCM_LE);
#endif

	return (ttybits);
}

Static void
ucom_break(sc, onoff)
	struct ucom_softc *sc;
	int onoff;
{
	DPRINTF(("ucom_break: onoff=%d\n", onoff));

	if (sc->sc_methods->ucom_set != NULL)
		sc->sc_methods->ucom_set(sc->sc_parent, sc->sc_portno,
		    UCOM_SET_BREAK, onoff);
}

Static void
ucom_dtr(sc, onoff)
	struct ucom_softc *sc;
	int onoff;
{
	DPRINTF(("ucom_dtr: onoff=%d\n", onoff));

	if (sc->sc_methods->ucom_set != NULL)
		sc->sc_methods->ucom_set(sc->sc_parent, sc->sc_portno, 
		    UCOM_SET_DTR, onoff);
}

Static void
ucom_rts(sc, onoff)
	struct ucom_softc *sc;
	int onoff;
{
	DPRINTF(("ucom_rts: onoff=%d\n", onoff));

	if (sc->sc_methods->ucom_set != NULL)
		sc->sc_methods->ucom_set(sc->sc_parent, sc->sc_portno, 
		    UCOM_SET_RTS, onoff);
}

void
ucom_status_change(sc)
	struct ucom_softc *sc;
{
	if (sc->sc_methods->ucom_get_status != NULL) {
		sc->sc_methods->ucom_get_status(sc->sc_parent, sc->sc_portno,
		    &sc->sc_lsr, &sc->sc_msr);
	} else {
		sc->sc_lsr = 0;
		sc->sc_msr = 0;
	}
}

Static int
ucomparam(tp, t)
	struct tty *tp;
	struct termios *t;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(tp->t_dev)];
	int error;

	if (sc->sc_dying)
		return (EIO);

	/* Check requested parameters. */
	if (t->c_ispeed && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/*
	 * For the console, always force CLOCAL and !HUPCL, so that the port
	 * is always active.
	 */
	if (ISSET(sc->sc_swflags, TIOCFLAG_SOFTCAR)) {
		SET(t->c_cflag, CLOCAL);
		CLR(t->c_cflag, HUPCL);
	}

	/*
	 * If there were no changes, don't do anything.  This avoids dropping
	 * input and improves performance when all we did was frob things like
	 * VMIN and VTIME.
	 */
	if (tp->t_ospeed == t->c_ospeed &&
	    tp->t_cflag == t->c_cflag)
		return (0);

	//XXX lcr = ISSET(sc->sc_lcr, LCR_SBREAK) | cflag2lcr(t->c_cflag);

	/* And copy to tty. */
	tp->t_ispeed = 0;
	tp->t_ospeed = t->c_ospeed;
	tp->t_cflag = t->c_cflag;

	if (sc->sc_methods->ucom_param != NULL) {
		error = sc->sc_methods->ucom_param(sc->sc_parent, sc->sc_portno,
			    t);
		if (error)
			return (error);
	}

	// XXX worry about CHWFLOW

	/*
	 * Update the tty layer's idea of the carrier bit, in case we changed
	 * CLOCAL or MDMBUF.  We don't hang up here; we only do that by
	 * explicit request.
	 */
	DPRINTF(("ucomparam: l_modem\n"));
	(void) (*linesw[tp->t_line].l_modem)(tp, 1 /* XXX carrier */ );

#if 0
XXX what if the hardware is not open
	if (!ISSET(t->c_cflag, CHWFLOW)) {
		if (sc->sc_tx_stopped) {
			sc->sc_tx_stopped = 0;
			ucomstart(tp);
		}
	}
#endif

	return (0);
}

/*
 * (un)block input via hw flowcontrol
 */
Static void
ucom_hwiflow(sc)
	struct ucom_softc *sc;
{
	DPRINTF(("ucom_hwiflow:\n"));
#if 0
XXX
	bus_space_tag_t iot = sc->sc_iot;
	bus_space_handle_t ioh = sc->sc_ioh;

	if (sc->sc_mcr_rts == 0)
		return;

	if (ISSET(sc->sc_rx_flags, RX_ANY_BLOCK)) {
		CLR(sc->sc_mcr, sc->sc_mcr_rts);
		CLR(sc->sc_mcr_active, sc->sc_mcr_rts);
	} else {
		SET(sc->sc_mcr, sc->sc_mcr_rts);
		SET(sc->sc_mcr_active, sc->sc_mcr_rts);
	}
	bus_space_write_1(iot, ioh, com_mcr, sc->sc_mcr_active);
#endif
}

Static void
ucomstart(tp)
	struct tty *tp;
{
	struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(tp->t_dev)];
	usbd_status err;
	int s;
	u_char *data;
	int cnt;

	if (sc->sc_dying)
		return;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY | TS_TIMEOUT | TS_TTSTOP)) {
		DPRINTFN(4,("ucomstart: no go, state=0x%x\n", tp->t_state));
		goto out;
	}
	if (sc->sc_tx_stopped)
		goto out;

	if (tp->t_outq.c_cc <= tp->t_lowat) {
		if (ISSET(tp->t_state, TS_ASLEEP)) {
			CLR(tp->t_state, TS_ASLEEP);
			wakeup(&tp->t_outq);
		}
		selwakeup(&tp->t_wsel);
		if (tp->t_outq.c_cc == 0)
			goto out;
	}

	/* Grab the first contiguous region of buffer space. */
	data = tp->t_outq.c_cf;
	cnt = ndqb(&tp->t_outq, 0);

	if (cnt == 0) {
		DPRINTF(("ucomstart: cnt==0\n"));
		goto out;
	}

	SET(tp->t_state, TS_BUSY);

	if (cnt > sc->sc_obufsize) {
		DPRINTF(("ucomstart: big buffer %d chars\n", cnt));
		cnt = sc->sc_obufsize;
	}
	if (sc->sc_methods->ucom_write != NULL)
		sc->sc_methods->ucom_write(sc->sc_parent, sc->sc_portno,
					   sc->sc_obuf, data, &cnt);
	else
		memcpy(sc->sc_obuf, data, cnt);

	DPRINTFN(4,("ucomstart: %d chars\n", cnt));
	usbd_setup_xfer(sc->sc_oxfer, sc->sc_bulkout_pipe, 
			(usbd_private_handle)sc, sc->sc_obuf, cnt,
			USBD_NO_COPY, USBD_NO_TIMEOUT, ucomwritecb);
	/* What can we do on error? */
	err = usbd_transfer(sc->sc_oxfer);
#ifdef DIAGNOSTIC
	if (err != USBD_IN_PROGRESS)
		printf("ucomstart: err=%s\n", usbd_errstr(err));
#endif

out:
	splx(s);
}

#if defined(__NetBSD__)
void
#else
int
#endif
ucomstop(tp, flag)
	struct tty *tp;
	int flag;
{
	DPRINTF(("ucomstop: flag=%d\n", flag));
#if 0
	/*struct ucom_softc *sc = ucom_cd.cd_devs[UCOMUNIT(tp->t_dev)];*/
	int s;

	s = spltty();
	if (ISSET(tp->t_state, TS_BUSY)) {
		DPRINTF(("ucomstop: XXX\n"));
		/* sc->sc_tx_stopped = 1; */
		if (!ISSET(tp->t_state, TS_TTSTOP))
			SET(tp->t_state, TS_FLUSH);
	}
	splx(s);
#endif
#if !defined(__NetBSD__)
	return 0;
#endif
}

Static void
ucomwritecb(xfer, p, status)
	usbd_xfer_handle xfer;
	usbd_private_handle p;
	usbd_status status;
{
	struct ucom_softc *sc = (struct ucom_softc *)p;
	struct tty *tp = sc->sc_tty;
	u_int32_t cc;
	int s;

	DPRINTFN(5,("ucomwritecb: status=%d\n", status));

	if (status == USBD_CANCELLED)
		return;

	if (status) {
		DPRINTF(("ucomwritecb: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, NULL, NULL, &cc, NULL);
	DPRINTFN(5,("ucomwritecb: cc=%d\n", cc));

	s = spltty();
	CLR(tp->t_state, TS_BUSY);
	if (ISSET(tp->t_state, TS_FLUSH))
		CLR(tp->t_state, TS_FLUSH);
	else
		ndflush(&tp->t_outq, cc);
	(*linesw[tp->t_line].l_start)(tp);
	splx(s);
}

Static usbd_status
ucomstartread(sc)
	struct ucom_softc *sc;
{
	usbd_status err;

	DPRINTFN(5,("ucomstartread: start\n"));
	usbd_setup_xfer(sc->sc_ixfer, sc->sc_bulkin_pipe, 
			(usbd_private_handle)sc, 
			sc->sc_ibuf, sc->sc_ibufsize,
			USBD_SHORT_XFER_OK | USBD_NO_COPY,
			USBD_NO_TIMEOUT, ucomreadcb);
	err = usbd_transfer(sc->sc_ixfer);
	if (err != USBD_IN_PROGRESS) {
		DPRINTF(("ucomstartread: err=%s\n", usbd_errstr(err)));
		return (err);
	}
	return (USBD_NORMAL_COMPLETION);
}
 
Static void
ucomreadcb(xfer, p, status)
	usbd_xfer_handle xfer;
	usbd_private_handle p;
	usbd_status status;
{
	struct ucom_softc *sc = (struct ucom_softc *)p;
	struct tty *tp = sc->sc_tty;
	int (*rint) __P((int c, struct tty *tp)) = linesw[tp->t_line].l_rint;
	usbd_status err;
	u_int32_t cc;
	u_char *cp;
	int s;

	if (status == USBD_CANCELLED)
		return;

	if (status) {
		DPRINTF(("ucomreadcb: status=%d\n", status));
		usbd_clear_endpoint_stall_async(sc->sc_bulkin_pipe);
		/* XXX we should restart after some delay. */
		return;
	}

	usbd_get_xfer_status(xfer, NULL, (void **)&cp, &cc, NULL);
	DPRINTFN(5,("ucomreadcb: got %d chars, tp=%p\n", cc, tp));
	if (sc->sc_methods->ucom_read != NULL)
		sc->sc_methods->ucom_read(sc->sc_parent, sc->sc_portno,
					  &cp, &cc);

	s = spltty();
	/* Give characters to tty layer. */
	while (cc-- > 0) {
		DPRINTFN(7,("ucomreadcb: char=0x%02x\n", *cp));
		if ((*rint)(*cp++, tp) == -1) {
			/* XXX what should we do? */
			printf("%s: lost %d chars\n", USBDEVNAME(sc->sc_dev),
			       cc);
			break;
		}
	}
	splx(s);

	err = ucomstartread(sc);
	if (err) {
		printf("%s: read start failed\n", USBDEVNAME(sc->sc_dev));
		/* XXX what should we dow now? */
	}
}

Static void
ucom_cleanup(sc)
	struct ucom_softc *sc;
{
	DPRINTF(("ucom_cleanup: closing pipes\n"));

	ucom_shutdown(sc);
	usbd_abort_pipe(sc->sc_bulkin_pipe);
	usbd_close_pipe(sc->sc_bulkin_pipe);
	usbd_abort_pipe(sc->sc_bulkout_pipe);
	usbd_close_pipe(sc->sc_bulkout_pipe);
	usbd_free_xfer(sc->sc_ixfer);
	usbd_free_xfer(sc->sc_oxfer);
}

#endif /* NUCOM > 0 */

int
ucomprint(aux, pnp)
	void *aux;
	const char *pnp;
{

	if (pnp)
		printf("ucom at %s\n", pnp);
	return (UNCONF);
}

int
#if defined(__OpenBSD__)
ucomsubmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct cfdata *cf = match;
#else
ucomsubmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
#endif
	struct ucom_attach_args *uca = aux;

	if (uca->portno != UCOM_UNK_PORTNO &&
	    cf->ucomcf_portno != UCOM_UNK_PORTNO &&
	    cf->ucomcf_portno != uca->portno)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}
