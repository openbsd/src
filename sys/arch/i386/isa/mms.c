/*	$OpenBSD: mms.c,v 1.9 1999/01/13 07:26:01 niklas Exp $	*/
/*	$NetBSD: mms.c,v 1.24 1996/05/12 23:12:18 mycroft Exp $	*/

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
#include <machine/intr.h>
#include <machine/pio.h>
#include <machine/mouse.h>
#include <machine/conf.h>

#include <dev/isa/isavar.h>

#define	MMS_ADDR	0	/* offset for register select */
#define	MMS_DATA	1	/* offset for InPort data */
#define	MMS_IDENT	2	/* offset for identification register */
#define	MMS_NPORTS	4

#define	MMS_CHUNK	128	/* chunk size for read */
#define	MMS_BSIZE	1020	/* buffer size */

struct mms_softc {		/* driver status information */
	struct device sc_dev;
	void *sc_ih;

	struct clist sc_q;
	struct selinfo sc_rsel;
	int sc_iobase;		/* I/O port base */
	u_char sc_state;	/* mouse driver state */
#define	MMS_OPEN	0x01	/* device is open */
#define	MMS_ASLP	0x02	/* waiting for mouse data */
	u_char sc_status;	/* mouse button status */
	int sc_x, sc_y;		/* accumulated motion in the X,Y axis */
};

int mmsprobe __P((struct device *, void *, void *));
void mmsattach __P((struct device *, struct device *, void *));
int mmsintr __P((void *));

struct cfattach mms_ca = {
	sizeof(struct mms_softc), mmsprobe, mmsattach
};

struct cfdriver mms_cd = {
	NULL, "mms", DV_TTY
};

#define	MMSUNIT(dev)	(minor(dev))

int
mmsprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;

	/* Read identification register to see if present */
	if (inb(iobase + MMS_IDENT) != 0xde)
		return 0;

	/* Seems it was there; reset. */
	outb(iobase + MMS_ADDR, 0x87);

	ia->ia_iosize = MMS_NPORTS;
	ia->ia_msize = 0;
	return 1;
}

void
mmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mms_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;

	printf("\n");

	/* Other initialization was done by mmsprobe. */
	sc->sc_iobase = iobase;
	sc->sc_state = 0;

	sc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_PULSE,
	    IPL_TTY, mmsintr, sc, sc->sc_dev.dv_xname);
}

int
mmsopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = MMSUNIT(dev);
	struct mms_softc *sc;

	if (unit >= mms_cd.cd_ndevs)
		return ENXIO;
	sc = mms_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_state & MMS_OPEN)
		return EBUSY;

	if (clalloc(&sc->sc_q, MMS_BSIZE, 0) == -1)
		return ENOMEM;

	sc->sc_state |= MMS_OPEN;
	sc->sc_status = 0;
	sc->sc_x = sc->sc_y = 0;

	/* Enable interrupts. */
	outb(sc->sc_iobase + MMS_ADDR, 0x07);
	outb(sc->sc_iobase + MMS_DATA, 0x09);

	return 0;
}

int
mmsclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct mms_softc *sc = mms_cd.cd_devs[MMSUNIT(dev)];

	/* Disable interrupts. */
	outb(sc->sc_iobase + MMS_ADDR, 0x87);

	sc->sc_state &= ~MMS_OPEN;

	clfree(&sc->sc_q);

	return 0;
}

int
mmsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct mms_softc *sc = mms_cd.cd_devs[MMSUNIT(dev)];
	int s;
	int error = 0;
	size_t length;
	u_char buffer[MMS_CHUNK];

	/* Block until mouse activity occured. */

	s = spltty();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return EWOULDBLOCK;
		}
		sc->sc_state |= MMS_ASLP;
		error = tsleep((caddr_t)sc, PZERO | PCATCH, "mmsrea", 0);
		if (error) {
			sc->sc_state &= ~MMS_ASLP;
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
		if ((error = uiomove(buffer, length, uio)) != 0)
			break;
	}

	return error;
}

int
mmsioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct mms_softc *sc = mms_cd.cd_devs[MMSUNIT(dev)];
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
mmsintr(arg)
	void *arg;
{
	struct mms_softc *sc = arg;
	int iobase = sc->sc_iobase;
	u_char buttons, changed, status;
	char dx, dy;
	u_char buffer[5];

	if ((sc->sc_state & MMS_OPEN) == 0)
		/* Interrupts are not expected. */
		return 0;

	/* Freeze InPort registers (disabling interrupts). */
	outb(iobase + MMS_ADDR, 0x07);
	outb(iobase + MMS_DATA, 0x29);

	outb(iobase + MMS_ADDR, 0x00);
	status = inb(iobase + MMS_DATA);

	if (status & 0x40) {
		outb(iobase + MMS_ADDR, 1);
		dx = inb(iobase + MMS_DATA);
		dx = (dx == -128) ? -127 : dx;
		outb(iobase + MMS_ADDR, 2);
		dy = inb(iobase + MMS_DATA);
		dy = (dy == -128) ? 127 : -dy;
	} else
		dx = dy = 0;

	/* Unfreeze InPort registers (reenabling interrupts). */
	outb(iobase + MMS_ADDR, 0x07);
	outb(iobase + MMS_DATA, 0x09);

	buttons = status & BUTSTATMASK;
	changed = status & BUTCHNGMASK;
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

		if (sc->sc_state & MMS_ASLP) {
			sc->sc_state &= ~MMS_ASLP;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->sc_rsel);
	}

	return -1;
}

int
mmsselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct mms_softc *sc = mms_cd.cd_devs[MMSUNIT(dev)];
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
