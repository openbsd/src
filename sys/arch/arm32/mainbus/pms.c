/* $NetBSD: pms.c,v 1.1 1996/03/28 21:50:19 mark Exp $ */

/*-
 * Copyright (c) 1996 D.C. Tsen
 * Copyright (c) 1994 Charles Hannum.
 * Copyright (c) 1992, 1993 Erik Forsberg.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from:pms.c,v 1.24 1995/12/24 02:30:28 mycroft Exp
 */

/*
 * Ported from 386 version of PS/2 mouse driver.
 * D.C. Tsen
 */

#include "pms.h"
#if NPMS > 1
#error Only one PS/2 style mouse may be configured into your system.
#endif

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
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/katelib.h>
#include <machine/irqhandler.h>
#include <machine/iomd.h>
#include <machine/mouse.h>
#include <arm32/mainbus/mainbus.h>

/* mouse commands */
#define	PMS_SET_SCALE11	0xe6	/* set scaling 1:1 */
#define	PMS_SET_SCALE21 0xe7	/* set scaling 2:1 */
#define	PMS_SET_RES	0xe8	/* set resolution */
#define	PMS_GET_SCALE	0xe9	/* get scaling factor */
#define	PMS_SET_STREAM	0xea	/* set streaming mode */
#define	PMS_SET_SAMPLE	0xf3	/* set sampling rate */
#define	PMS_DEV_ENABLE	0xf4	/* mouse on */
#define	PMS_DEV_DISABLE	0xf5	/* mouse off */
#define	PMS_RESET	0xff	/* reset */

#define	PMS_CHUNK	128	/* chunk size for read */
#define	PMS_BSIZE	(20*64)	/* buffer size */

struct pms_softc {		/* driver status information */
	struct device sc_dev;
	irqhandler_t sc_ih;

	struct proc *proc;
	struct clist sc_q;
	struct selinfo sc_rsel;
	u_int sc_state;	/* mouse driver state */
#define	PMS_OPEN	0x01	/* device is open */
#define	PMS_ASLP	0x02	/* waiting for mouse data */
	u_int sc_status;	/* mouse button status */
	int sc_x, sc_y;		/* accumulated motion in the X,Y axis */
	int boundx, boundy, bounda, boundb;	/* Bounding box.  x,y is bottom left */
	int origx, origy;
	int lastx, lasty, lastb;
};

int pmsprobe __P((struct device *, void *, void *));
void pmsattach __P((struct device *, struct device *, void *));
int pmsintr __P((void *));

struct cfattach pms_ca = {
	sizeof(struct pms_softc), pmsprobe, pmsattach
};

struct cfdriver	pms_cd = {
	NULL, "pms", DV_DULL
};

#define	PMSUNIT(dev)	(minor(dev))

static inline void
pms_flush()
{
	while (inb(IOMD_MSCR) & 0x20) {
		delay(6);
		(void) inb(IOMD_MSDATA);
		delay(6);
		(void) inb(IOMD_MSDATA);
		delay(6);
		(void) inb(IOMD_MSDATA);
	}
}

static void
cmd_mouse(unsigned char cmd)
{
	unsigned char c;
	int i = 0;
	int retry = 10;

	for (i = 0; i < 1000; i++) {
		if (inb(IOMD_MSCR) & 0x80)
			break;
		delay(2);
	}
	if (i == 1000)
		printf("Mouse transmit not ready\n");

resend:
	outb(IOMD_MSDATA, cmd);
	delay(2);
	c = inb(IOMD_MSCR) & (unsigned char) 0xff;
	while (!(c & (unsigned char) 0x20)) {
		delay(1);
		c = inb(IOMD_MSCR);
	}

	delay(10000);

	c = inb(IOMD_MSDATA) & 0xff;
	if ((c == 0xFA) || (c == 0xEE))
		return;

	if (--retry) {
		pms_flush();
		goto resend;
	}

	printf("Mouse cmd failed, cmd = %x, status = %x\n", cmd, c);
	return;
}

int
pmsprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	/*struct mainbus_attach_args *mb = aux;*/
	int i, j;
	int mid;
	int id;

/* Make sure we have an IOMD we understand */
    
	id = ReadByte(IOMD_ID0) | (ReadByte(IOMD_ID1) << 8);

/* So far I only know about this IOMD */

	switch (id) {
	case RPC600_IOMD_ID:
		return(0);
		break;
	case RC7500_IOC_ID:
		break;
	default:
		printf("pms: Unknown IOMD id=%04x", id);
		return(0);
		break;
	}

	outb(IOMD_MSCR, 0x08);	/* enable the mouse */

	i = 0;
	while ((inb(IOMD_MSCR) & 0x03) != 0x03) {
		if (i++ > 10) {
			printf("Mouse not found, status = <%x>.\n", inb(IOMD_MSCR));
			return(0);
		}
		pms_flush();
		delay(2);
		outb(IOMD_MSCR, 0x08);
	}

	pms_flush();

	/*
	 * Disable, reset and enable the mouse.
	 */
	cmd_mouse(PMS_DEV_DISABLE);
	cmd_mouse(PMS_RESET);
	delay(300000);
	j = 10;
	i = 0;
	while ((mid = inb(IOMD_MSDATA)) != 0xAA) {
		if (++i > 500) {
			if (--j < 0) {
				printf("Mouse Reset failed, status = <%x>.\n", mid);
				return(0);
			}
			pms_flush();
			cmd_mouse(PMS_RESET);
			i = 0;
		}
		delay(100000);
	}
	mid = inb(IOMD_MSDATA);
#if 0
	cmd_mouse(PMS_SET_RES);
	cmd_mouse(3);		/* 8 counts/mm */
	cmd_mouse(PMS_SET_SCALE21);
	cmd_mouse(PMS_SET_SAMPLE);
	cmd_mouse(100);	/* 100 samples/sec */
	cmd_mouse(PMS_SET_STREAM);
#endif
	cmd_mouse(PMS_DEV_ENABLE);
	return 1;
}

void
pmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pms_softc *sc = (void *)self;
	struct mainbus_attach_args *mb = aux;

	printf("\n");

	/* Other initialization was done by pmsprobe. */
	sc->sc_state = 0;
	sc->origx = 0;
	sc->origy = 0;
	sc->boundx = -4095;
	sc->boundy = -4095;
	sc->bounda = 4096;
	sc->boundb = 4096;

	sc->sc_ih.ih_func = pmsintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_TTY;
	sc->sc_ih.ih_name = "pms";
	if (mb->mb_irq != IRQUNK)
		sc->sc_ih.ih_num = mb->mb_irq;
	else
#ifdef RC7500
		sc->sc_ih.ih_num = IRQ_MSDRX;
#else
		panic("pms: No IRQ specified for pms interrupt handler");
#endif
}

int
pmsopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = PMSUNIT(dev);
	struct pms_softc *sc;

	if (unit >= pms_cd.cd_ndevs)
		return ENXIO;
	sc = pms_cd.cd_devs[unit];
	if (!sc)
		return ENXIO;

	if (sc->sc_state & PMS_OPEN)
		return EBUSY;

	if (clalloc(&sc->sc_q, PMS_BSIZE, 0) == -1)
		return ENOMEM;

	sc->proc = p;
	sc->sc_state |= PMS_OPEN;
	sc->sc_status = 0;
	sc->sc_x = sc->sc_y = 0;
	sc->lastx = -1;
	sc->lasty = -1;
	sc->lastb = -1;

	if (irq_claim(IRQ_INSTRUCT, &sc->sc_ih) == -1)
		panic("Cannot claim MOUSE IRQ");

	return 0;
}

int
pmsclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct pms_softc *sc = pms_cd.cd_devs[PMSUNIT(dev)];

	if (irq_release(IRQ_INSTRUCT, &sc->sc_ih) != 0)
		panic("Cannot release MOUSE IRQ");
 
	sc->proc = NULL;
	sc->sc_state &= ~PMS_OPEN;
	sc->sc_x = sc->sc_y = 0;

	clfree(&sc->sc_q);

	return 0;
}

int
pmsread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct pms_softc *sc = pms_cd.cd_devs[PMSUNIT(dev)];
	int s;
	int error = 0;
	size_t length;
	u_char buffer[PMS_CHUNK];

	/* Block until mouse activity occured. */

	s = spltty();
	while (sc->sc_q.c_cc == 0) {
		if (flag & IO_NDELAY) {
			splx(s);
			return EWOULDBLOCK;
		}
		sc->sc_state |= PMS_ASLP;
		if (error = tsleep((caddr_t)sc, (PZERO | PCATCH), "pmsread", 0)) {
			sc->sc_state &= ~PMS_ASLP;
			splx(s);
			return error;
		}
	}

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
	splx(s);

	return error;
}

int
pmsioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct pms_softc *sc = pms_cd.cd_devs[PMSUNIT(dev)];
	struct mouseinfo info;
	int s;
	int error = 0;

	s = spltty();

	switch (cmd) {
	case MOUSEIOC_SETSTATE:
		printf("MOUSEIOC_SETSTATE called\n");
		break;
	case MOUSEIOC_WRITEX:
		sc->sc_x = *(int *) addr;
		break;
	case MOUSEIOC_WRITEY:
		sc->sc_y = *(int *) addr;
		break;
	case MOUSEIOC_SETBOUNDS:
	{
		register struct mouse_boundingbox *bo = (void *) addr;
		sc->boundx = bo->x; sc->boundy = bo->y;
		sc->bounda = bo->a; sc->boundb = bo->b;
		break;
	}
	case MOUSEIOC_SETORIGIN:
	{
		register struct mouse_origin *oo = (void *) addr;
		sc->origx = oo->x;
		sc->origy = oo->y;
		break;
	}
	case MOUSEIOC_GETBOUNDS:
	{
		register struct mouse_boundingbox *bo = (void *) addr;
		bo->x = sc->boundx; bo->y = sc->boundy;
		bo->a = sc->bounda; bo->b = sc->boundb;
		break;
	}
	case MOUSEIOC_GETORIGIN:
	{
		register struct mouse_origin *oo = (void *) addr;
		oo->x = sc->origx;
		oo->y = sc->origy;
		break;
	}
	case MOUSEIOC_GETSTATE:
		printf("MOUSEIOC_GETSTATE called\n");
		/*
		 * Fall through.
		 */
/*	case MOUSEIOCREAD:*/

		info.status = sc->sc_status;
		if (sc->sc_x || sc->sc_y)
			info.status |= MOVEMENT;

#if 1
		info.xmotion = sc->sc_x;
		info.ymotion = sc->sc_y;
#else
		if (sc->sc_x > sc->bounda)
			info.xmotion = sc->bounda;
		else if (sc->sc_x < sc->boundx)
			info.xmotion = sc->boundx;
		else
			info.xmotion = sc->sc_x;

		if (sc->sc_y > sc->boundb)
			info.ymotion = sc->boundb;
		else if (sc->sc_y < sc->boundy)
			info.ymotion = sc->boundy;
		else
			info.ymotion = sc->sc_y;
#endif

		/* Reset historical information. */
		sc->sc_x = sc->sc_y = 0;
		sc->sc_status &= ~BUTCHNGMASK;
		ndflush(&sc->sc_q, sc->sc_q.c_cc);

		error = copyout(&info, addr, sizeof(struct mouseinfo));
		break;

	default:
		error = EINVAL;
		break;
	}
	splx(s);

	return error;
}

/* Masks for the first byte of a packet */
#define PS2LBUTMASK 0x01
#define PS2RBUTMASK 0x02
#define PS2MBUTMASK 0x04

#define XNEG_MASK	0x10
#define YNEG_MASK	0x20

int
pmsintr(arg)
	void *arg;
{
	struct pms_softc *sc = arg;
	static int state = 0;
	static u_char buttons;
	u_char changed;
	static int dx, dy;
	int dosignal = 0;
	int s;
	u_char b;
	struct mousebufrec mbuffer;

	if ((sc->sc_state & PMS_OPEN) == 0) {
		/* Interrupts are not expected.  Discard the byte. */
		pms_flush();
		return(-1);
	}

	switch (state) {

	case 0:
		buttons = inb(IOMD_MSDATA);
		if ((buttons & 0xc0) == 0)
			++state;
		break;

	case 1:
		dx = inb(IOMD_MSDATA);
		dx = (buttons & XNEG_MASK) ? -dx : dx;
		++state;
		break;

	case 2:
		dy = inb(IOMD_MSDATA);
		dy = (buttons & YNEG_MASK) ? -dy : dy;
		state = 0;

		buttons = ((buttons & PS2LBUTMASK) << 2) |
			  ((buttons & (PS2RBUTMASK | PS2MBUTMASK)) >> 1);
		/*
		 * Ahhh, the Xarm server interrupt button bits reversed.
		 * If bit is set to 1, it means button is not pressed which is
		 * not what PS/2 mouse button bits said.  Since we are simulating
		 * the quadmouse, that's not too picky about it.
		 */
		buttons ^= BUTSTATMASK;
		changed = ((buttons ^ sc->sc_status) & BUTSTATMASK) << 3;
		sc->sc_status = buttons | (sc->sc_status & ~BUTSTATMASK) | changed;

		if (dx || dy || changed) {
			if (dx < 0)
				dx = -(256 + dx);
			if (dy < 0)
				dy = -(256 + dy);

			/* Update accumulated movements. */
			sc->sc_x += dx;
			sc->sc_y += dy;

			if (sc->sc_x > sc->bounda)
				sc->sc_x = sc->bounda;
			else if (sc->sc_x < sc->boundx)
				sc->sc_x = sc->boundx;

			if (sc->sc_y > sc->boundb)
				sc->sc_y = sc->boundb;
			else if (sc->sc_y < sc->boundy)
				sc->sc_y = sc->boundy;

			if (sc->sc_q.c_cc == 0) {
				dosignal = 1;
			}

			/* Add this event to the queue. */
			b = buttons & BUTSTATMASK;
			mbuffer.status = b | (b ^ sc->lastb) << 3
				| (((sc->sc_x==sc->lastx) && (sc->sc_y==sc->lasty))?0:0x40);
			mbuffer.x = sc->sc_x * 2;
			mbuffer.y = sc->sc_y * 2;
			microtime(&mbuffer.event_time);
			s = spltty();
			(void) b_to_q((char *)&mbuffer, sizeof(mbuffer), &sc->sc_q);
			(void) splx(s);
			sc->lastx = sc->sc_x;
			sc->lasty = sc->sc_y;
			sc->lastb = b;

			selwakeup(&sc->sc_rsel);
			if (sc->sc_state & PMS_ASLP) {
				sc->sc_state &= ~PMS_ASLP;
				wakeup((caddr_t)sc);
			}
			if (dosignal)
				psignal(sc->proc, SIGIO);
		}

		break;
	}

	return(0);
}

int
pmsselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	struct pms_softc *sc = pms_cd.cd_devs[PMSUNIT(dev)];
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
