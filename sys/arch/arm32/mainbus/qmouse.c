/* $NetBSD: qmouse.c,v 1.5 1996/03/28 21:56:40 mark Exp $ */

/*
 * Copyright (c) Scott Stevens 1995 All rights reserved
 * Copyright (c) Melvin Tang-Richardson 1995 All rights reserved
 * Copyright (c) Mark Brinicombe 1995 All rights reserved
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

/*
 * Quadrature mouse driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <dev/cons.h>
#include <sys/fcntl.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/time.h>

#include <arm32/mainbus/mainbus.h>
#include <machine/irqhandler.h>
#include <machine/katelib.h>
#include <machine/iomd.h>
#include <machine/mouse.h>

#include "quadmouse.h"

#define TIMER1_COUNT 40000		/* 50Hz */

#define QMOUSE_BSIZE 12*64

struct quadmouse_softc {
	struct device sc_device;
	irqhandler_t sc_ih;
	int sc_iobase;
	struct selinfo sc_rsel;
#define QMOUSE_OPEN 0x01
#define QMOUSE_ASLEEP 0x02
	int sc_state;
	int boundx, boundy, bounda, boundb;	/* Bounding box.  x,y is bottom left */
	int origx, origy;
	int xmult, ymult;	/* Multipliers */
	int lastx, lasty, lastb;
	struct proc *proc;
	struct clist buffer;
};

int	quadmouseprobe	__P((struct device *, void *, void *));
void	quadmouseattach	__P((struct device *, struct device *, void *));
int	quadmouseopen	__P((dev_t, int, int, struct proc *));
int	quadmouseclose	__P((dev_t, int, int, struct proc *));

int        strncmp __P((const char *, const char *, size_t));

int quadmouseintr	(struct quadmouse_softc *);

struct cfattach quadmouse_ca = {
	sizeof(struct quadmouse_softc), quadmouseprobe, quadmouseattach
};

struct cfdriver	quadmouse_cd = {
	NULL, "quadmouse", DV_DULL
};


int
quadmouseprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
/*	struct mainbus_attach_args *mb = aux;*/
	int id;

/* Make sure we have an IOMD we understand */
    
	id = ReadByte(IOMD_ID0) | (ReadByte(IOMD_ID1) << 8);

/* So far I only know about this IOMD */

	switch (id) {
	case RPC600_IOMD_ID:
		return(1);
		break;
	default:
		printf("quadmouse: Unknown IOMD id=%04x", id);
		break;
	}

	return(0);
}


void
quadmouseattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct quadmouse_softc *sc = (void *)self;
	struct mainbus_attach_args *mb = aux;
    
	sc->sc_iobase = mb->mb_iobase;
    
/* Check for a known IOMD chip */

	sc->sc_ih.ih_func = quadmouseintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_TTY;
	sc->sc_ih.ih_name = "T1 quadmouse";

/* Set up origin and multipliers */

	sc->origx = 0;
	sc->origy = 0;
	sc->xmult = 2;
	sc->ymult = 2;

/* Set up bounding box */

	sc->boundx = -4095;
	sc->boundy = -4095;
	sc->bounda = 4096;
	sc->boundb = 4096;

	sc->sc_state = 0;

	WriteWord(IOMD_MOUSEX, sc->origx);
	WriteWord(IOMD_MOUSEY, sc->origy);
    
	printf("\n");
}


int
quadmouseopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct quadmouse_softc *sc;
	int unit = minor(dev);
    
	if (unit >= quadmouse_cd.cd_ndevs)
		return(ENXIO);

	sc = quadmouse_cd.cd_devs[unit];
    
	if (!sc) return(ENXIO);

	if (sc->sc_state & QMOUSE_OPEN) return(EBUSY);

	sc->proc = p;
    
	sc->lastx = -1;
	sc->lasty = -1;
	sc->lastb = -1;

	/* initialise buffer */
	if (clalloc(&sc->buffer, QMOUSE_BSIZE, 0) == -1)
		return(ENOMEM);

	sc->sc_state |= QMOUSE_OPEN;

	WriteByte(IOMD_T1LOW, (TIMER1_COUNT >> 0) & 0xff);
	WriteByte(IOMD_T1HIGH, (TIMER1_COUNT >> 8) & 0xff);
	WriteByte(IOMD_T1GO, 0);
	
	if (irq_claim(IRQ_TIMER1, &sc->sc_ih))
		panic("Cannot claim TIMER1 IRQ for quadmouse%d", sc->sc_device.dv_unit);

	return(0);
}


int
quadmouseclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit = minor (dev);
	struct quadmouse_softc *sc = quadmouse_cd.cd_devs[unit];
    
	if (irq_release(IRQ_TIMER1, &sc->sc_ih) != 0)
		panic("Cannot release IRA");

	sc->proc = NULL;
	sc->sc_state = 0;

	clfree(&sc->buffer);

	return(0);
}

int
quadmouseread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = minor(dev);
	struct quadmouse_softc *sc = quadmouse_cd.cd_devs[unit];
	int error;
	int s;
	int length;
	u_char buffer[128];

	s=spltty();
	while(sc->buffer.c_cc==0) {
		if(flag & IO_NDELAY) {
			(void)splx(s);
			return(EWOULDBLOCK);
		}
		sc->sc_state |= QMOUSE_ASLEEP;
		if((error = tsleep((caddr_t)sc, PZERO | PCATCH, "quadmouseread", 0))) {
			sc->sc_state &= ~QMOUSE_ASLEEP;
			(void)splx(s);
			return(error);
		}
	}
	
	while(sc->buffer.c_cc>0 && uio->uio_resid>0) {
		length=min(sc->buffer.c_cc, uio->uio_resid);
		if(length>sizeof(buffer))
			length=sizeof(buffer);

		(void) q_to_b(&sc->buffer, buffer, length);

		if(error = (uiomove(buffer, length, uio)))
			break;
	}
	(void)splx(s);
	return(error);
}


#define FMT_START	int x = ReadWord(IOMD_MOUSEX)&0xffff;		\
			int y = ReadWord(IOMD_MOUSEY)&0xffff;		\
			int b = ReadByte(IO_MOUSE_BUTTONS)&0x70;	\
			if(x&0x8000) x|=0xffff0000;			\
			if(y&0x8000) y|=0xffff0000;			\
			x = (x - sc->origx);				\
			y = (y - sc->origy);				\
			if (x<(sc->boundx)) x = sc->boundx;		\
				WriteWord(IOMD_MOUSEX, x + sc->origx);	\
			if (x>(sc->bounda)) x = sc->bounda;		\
				WriteWord(IOMD_MOUSEX, x + sc->origx);	\
			if (y<(sc->boundy)) y = sc->boundy;		\
				WriteWord(IOMD_MOUSEY, y + sc->origy);	\
			if (y>(sc->boundb)) y = sc->boundb;		\
				WriteWord(IOMD_MOUSEY, y + sc->origy);	\
			x=x*sc->xmult;					\
			y=y*sc->ymult;

#define	FMT_END


int
quadmouseioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct quadmouse_softc *sc = quadmouse_cd.cd_devs[minor(dev)];

	switch (cmd) {
	case MOUSEIOC_WRITEX:
		WriteWord(IOMD_MOUSEX, *(int *)data+sc->origx);
		return 0;
	case MOUSEIOC_WRITEY:
		WriteWord(IOMD_MOUSEY, *(int *)data+sc->origy);
		return 0;
	case MOUSEIOC_SETSTATE:
	{
		register struct mouse_state *co = (void *)data;
		WriteWord(IOMD_MOUSEX, co->x);
		WriteWord(IOMD_MOUSEY, co->y);

		/* Silly, but here for completeness, just incase */
		/* the hardware supports it *giggle*  		 */

/* This is not writable -- mark -- technically this should fault */

/*		WriteWord ( IO_MOUSE_BUTTONS, co->buttons );*/
		return 0;
	}
	case MOUSEIOC_SETBOUNDS:
	{
		register struct mouse_boundingbox *bo = (void *)data;
		sc->boundx = bo->x; sc->boundy = bo->y;
		sc->bounda = bo->a; sc->boundb = bo->b;
		return 0;
	}
	case MOUSEIOC_SETORIGIN:
	{
		register struct mouse_origin *oo = (void *)data;
/*		int oldx, oldy;*/
		/* Need to fix up! */
		sc->origx = oo->x;
		sc->origy = oo->y;
		return 0;
	}
	case MOUSEIOC_GETSTATE:
	{
		register struct mouse_state *co = (void *)data;
		FMT_START
		co->x = x;
		co->y = y;
		co->buttons = b ^ 0x70;
		FMT_END
		return 0;
	}
	case MOUSEIOC_GETBOUNDS:
	{
		register struct mouse_boundingbox *bo = (void *)data;
		bo->x = sc->boundx; bo->y = sc->boundy;
		bo->a = sc->bounda; bo->b = sc->boundb;
		return 0;
	}
	case MOUSEIOC_GETORIGIN:
	{
		register struct mouse_origin *oo = (void *)data;
		oo->x = sc->origx;
		oo->y = sc->origy;
		return 0;
	}
	}   

	return (EINVAL);
}


int
quadmouseintr(sc)
	struct quadmouse_softc *sc;
{
	int s;
	struct mousebufrec buffer;
	int dosignal=0;

	FMT_START

	b &= 0x70;
	b >>= 4;
        if (x != sc->lastx || y != sc->lasty || b != sc->lastb) {
		/* Mouse state changed */
		buffer.status = b | ( b ^ sc->lastb) << 3 | (((x==sc->lastx) && (y==sc->lasty))?0:0x40);
		buffer.x = x;
		buffer.y = y;
		microtime(&buffer.event_time);

		if(sc->buffer.c_cc==0)
			dosignal=1;

		s=spltty();
		(void) b_to_q((char *)&buffer, sizeof(buffer), &sc->buffer);
		(void)splx(s);
		selwakeup(&sc->sc_rsel);

		if(sc->sc_state & QMOUSE_ASLEEP) {
			sc->sc_state &= ~QMOUSE_ASLEEP;
			wakeup((caddr_t)sc);
		}

		if(dosignal)
			psignal(sc->proc, SIGIO);
		
		sc->lastx = x;
		sc->lasty = y;
		sc->lastb = b;
	}

	FMT_END
	return(0);
}

int
quadmouseselect(dev, rw, p)
	dev_t dev;
	int rw;
	struct proc *p;
{
	int unit = minor(dev);
	struct quadmouse_softc *sc = quadmouse_cd.cd_devs[unit];
	int s;

	if(rw == FWRITE)
		return 0;

	s=spltty();
	if(sc->buffer.c_cc > 0) {
		selrecord(p, &sc->sc_rsel);
		(void)splx(s);
		return 0;
	}
	(void)splx(s);
	return 1;
}

/* End of quadmouse.c */
