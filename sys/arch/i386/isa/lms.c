/*	$NetBSD: lms.c,v 1.21 1995/12/24 02:30:17 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.
 * Copyright (c) 1992, 1993 Erik Forsberg.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/mouse.h>

#include <dev/isa/isavar.h>

#define	LMS_DATA	0       /* offset for data port, read-only */
#define	LMS_SIGN	1       /* offset for signature port, read-write */
#define	LMS_INTR	2       /* offset for interrupt port, read-only */
#define	LMS_CNTRL	2       /* offset for control port, write-only */
#define	LMS_CONFIG	3	/* for configuration port, read-write */
#define	LMS_NPORTS	4

#define	LMS_CHUNK	128	/* chunk size for read */
#define	LMS_BSIZE	1020	/* buffer size */

struct lms_softc {		/* driver status information */
	struct device sc_dev;
	void *sc_ih;

	struct clist sc_q;
	struct selinfo sc_rsel;
	int sc_iobase;		/* I/O port base */
	u_char sc_state;	/* mouse driver state */
#define	LMS_OPEN	0x01	/* device is open */
#define	LMS_ASLP	0x02	/* waiting for mouse data */
	u_char sc_status;	/* mouse button status */
	int sc_x, sc_y;		/* accumulated motion in the X,Y axis */
};

int lmsprobe __P((struct device *, void *, void *));
void lmsattach __P((struct device *, struct device *, void *));
int lmsintr __P((void *));

struct cfdriver lmscd = {
	NULL, "lms", lmsprobe, lmsattach, DV_TTY, sizeof(struct lms_softc)
};

#define	LMSUNIT(dev)	(minor(dev))

int
lmsprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;

	/* Configure and check for port present. */
	outb(iobase + LMS_CONFIG, 0x91);
	delay(10);
	outb(iobase + LMS_SIGN, 0x0c);
	delay(10);
	if (inb(iobase + LMS_SIGN) != 0x0c)
		return 0;
	outb(iobase + LMS_SIGN, 0x50);
	delay(10);
	if (inb(iobase + LMS_SIGN) != 0x50)
		return 0;

	/* Disable interrupts. */
	outb(iobase + LMS_CNTRL, 0x10);

	ia->ia_iosize = LMS_NPORTS;
	ia->ia_msize = 0;
	return 1;
}

void
lmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lms_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;

	printf("\n");

	/* Other initialization was done by lmsprobe. */
	sc->sc_iobase = iobase;
	sc->sc_state = 0;

	sc->sc_ih = isa_intr_establish(ia->ia_irq, IST_PULSE, IPL_TTY, lmsintr,
	    sc);
}

int
lmsopen(dev, flag)
	dev_t dev;
	int flag;
{
	int unit = LMSUNIT(dev);
	struct lms_softc *sc;

	if (unit >= lmscd.cd_ndevs)
		return ENXIO;
	sc = lmscd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_state & LMS_OPEN)
		return EBUSY;

	if (clalloc(&sc->sc_q, LMS_BSIZE, 0) == -1)
		return ENOMEM;

	sc->sc_state |= LMS_OPEN;
	sc->sc_status = 0;
	sc->sc_x = sc->sc_y = 0;

	/* Enable interrupts. */
	outb(sc->sc_iobase + LMS_CNTRL, 0);

	return 0;
}

int
lmsclose(dev, flag)
	dev_t dev;
	int flag;
{
	struct lms_softc *sc = lmscd.cd_devs[LMSUNIT(dev)];

	/* Disable interrupts. */
	outb(sc->sc_iobase + LMS_CNTRL, 0x10);

	sc->sc_state &= ~LMS_OPEN;

	clfree(&sc->sc_q);

	return 0;
}

int
lmsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct lms_softc *sc = lmscd.cd_devs[LMSUNIT(dev)];
	int s;
	int error;
	size_t length;
	u_char buffer[LMS_CHUNK];

	/* Block until mouse activity occured. */

	s = spltty();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return EWOULDBLOCK;
		}
		sc->sc_state |= LMS_ASLP;
		if (error = tsleep((caddr_t)sc, PZERO | PCATCH, "lmsrea", 0)) {
			sc->sc_state &= ~LMS_ASLP;
			splx(s);
			return error;
		}
	}
	splx(s);

	/* Transfer as many chunks as possible. */

	while (sc->sc_q.c_cc > 0 && uio->uio_resid > 0) {
		length = min(sc->sc_q.c_cc, uio->uio_resid);
		if (length > sizeof(buffer))
			length = sizeof(buffer);

		/* Remove a small chunk from the input queue. */
		(void) q_to_b(&sc->sc_q, buffer, length);

		/* Copy the data to the user process. */
		if (error = uiomove(buffer, length, uio))
			break;
	}

	return error;
}

int
lmsioctl(dev, cmd, addr, flag)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
{
	struct lms_softc *sc = lmscd.cd_devs[LMSUNIT(dev)];
	struct mouseinfo info;
	int s;
	int error;

	switch (cmd) {
	case MOUSEIOCREAD:
		s = spltty();

		info.status = sc->sc_status;
		if (sc->sc_x || sc->sc_y)
			info.status |= MOVEMENT;

		if (sc->sc_x > 127)
			info.xmotion = 127;
		else if (sc->sc_x < -127)
			/* Bounding at -127 avoids a bug in XFree86. */
			info.xmotion = -127;
		else
			info.xmotion = sc->sc_x;

		if (sc->sc_y > 127)
			info.ymotion = 127;
		else if (sc->sc_y < -127)
			info.ymotion = -127;
		else
			info.ymotion = sc->sc_y;

		/* Reset historical information. */
		sc->sc_x = sc->sc_y = 0;
		sc->sc_status &= ~BUTCHNGMASK;
		ndflush(&sc->sc_q, sc->sc_q.c_cc);

		splx(s);
		error = copyout(&info, addr, sizeof(struct mouseinfo));
		break;

	default:
		error = EINVAL;
		break;
	}

	return error;
}

int
lmsintr(arg)
	void *arg;
{
	struct lms_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char hi, lo, buttons, changed;
	char dx, dy;
	u_char buffer[5];

	if ((sc->sc_state & LMS_OPEN) == 0)
		/* Interrupts are not expected. */
		return 0;

	outb(iobase + LMS_CNTRL, 0xab);
	hi = inb(iobase + LMS_DATA);
	outb(iobase + LMS_CNTRL, 0x90);
	lo = inb(iobase + LMS_DATA);
	dx = ((hi & 0x0f) << 4) | (lo & 0x0f);
	/* Bounding at -127 avoids a bug in XFree86. */
	dx = (dx == -128) ? -127 : dx;

	outb(iobase + LMS_CNTRL, 0xf0);
	hi = inb(iobase + LMS_DATA);
	outb(iobase + LMS_CNTRL, 0xd0);
	lo = inb(iobase + LMS_DATA);
	dy = ((hi & 0x0f) << 4) | (lo & 0x0f);
	dy = (dy == -128) ? 127 : -dy;

	outb(iobase + LMS_CNTRL, 0);

	buttons = (~hi >> 5) & 0x07;
	changed = ((buttons ^ sc->sc_status) & 0x07) << 3;
	sc->sc_status = buttons | (sc->sc_status & ~BUTSTATMASK) | changed;

	if (dx || dy || changed) {
		/* Update accumulated movements. */
		sc->sc_x += dx;
		sc->sc_y += dy;

		/* Add this event to the queue. */
		buffer[0] = 0x80 | (buttons ^ BUTSTATMASK);
		buffer[1] = dx;
		buffer[2] = dy;
		buffer[3] = buffer[4] = 0;
		(void) b_to_q(buffer, sizeof buffer, &sc->sc_q);

		if (sc->sc_state & LMS_ASLP) {
			sc->sc_state &= ~LMS_ASLP;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->sc_rsel);
	}

	return -1;
}

int
lmsselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct lms_softc *sc = lmscd.cd_devs[LMSUNIT(dev)];
	int s;
	int ret;

	if (rw == FWRITE)
		return 0;

	s = spltty();
	if (!sc->sc_q.c_cc) {
		selrecord(p, &sc->sc_rsel);
		ret = 0;
	} else
		ret = 1;
	splx(s);

	return ret;
}
