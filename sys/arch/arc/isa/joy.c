/*	$NetBSD: joy.c,v 1.3 1996/05/05 19:46:15 christos Exp $	*/

/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
 *
 * Ported to NetBSD by Matthieu Herrb <matthieu@laas.fr>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/errno.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/joystick.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <arc/isa/timerreg.h>

/*
 * The game port can manage 4 buttons and 4 variable resistors (usually 2
 * joysticks, each with 2 buttons and 2 pots.) via the port at address 0x201.
 * Getting the state of the buttons is done by reading the game port;
 * buttons 1-4 correspond to bits 4-7 and resistors 1-4 (X1, Y1, X2, Y2)
 * to bits 0-3.  If button 1 (resp 2, 3, 4) is pressed, the bit 4 (resp 5,
 * 6, 7) is set to 0 to get the value of a resistor, write the value 0xff
 * at port and wait until the corresponding bit returns to 0.
 */

/*
 * The formulae below only work if u is ``not too large''.  See also
 * the discussion in microtime.s
 */
#define USEC2TICKS(u) 	(((u) * 19549) >> 14)
#define TICKS2USEC(u) 	(((u) * 3433) >> 12)


#define JOYPART(d) (minor(d) & 1)
#define JOYUNIT(d) minor(d) >> 1 & 3

#ifndef JOY_TIMEOUT
#define JOY_TIMEOUT   2000	/* 2 milliseconds */
#endif

#define JOY_NPORTS    1

struct joy_softc {
	struct	device sc_dev;
	int	port;
	int	x_off[2], y_off[2];
	int	timeout[2];
};

int		joyprobe __P((struct device *, void *, void *));
int		joyread __P((dev_t dev, struct uio *uio, int flag));
int		joyioctl __P((dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p));
void		joyattach __P((struct device *, struct device *, void *));
int		joyopen __P((dev_t, int, int, struct proc *));
int		joyclose __P((dev_t, int, int, struct proc *));
static int	get_tick __P((void));

struct cfattach joy_ca = {
	sizeof(struct joy_softc), joyprobe, joyattach
};

struct cfdriver joy_cd = {
	NULL, "joy", DV_DULL
};


int
joyprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
#ifdef WANT_JOYSTICK_CONNECTED
	int iobase = ia->ia_iobase;

	isa_outb(iobase, 0xff);
	DELAY(10000);		/* 10 ms delay */
	return (isa_inb(iobase) & 0x0f) != 0x0f;
#else
	ia->ia_iosize = JOY_NPORTS;
	ia->ia_msize = 0;
	return 1;
#endif
}

void
joyattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct joy_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	int iobase = ia->ia_iobase;

	sc->port = iobase;
	sc->timeout[0] = sc->timeout[1] = 0;
	isa_outb(iobase, 0xff);
	DELAY(10000);		/* 10 ms delay */
	printf(": joystick%sconnected\n",
	    (isa_inb(iobase) & 0x0f) == 0x0f ? " not " : " ");
}

int
joyopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = JOYUNIT(dev);
	int i = JOYPART(dev);
	struct joy_softc *sc;

	if (unit >= joy_cd.cd_ndevs)
		return (ENXIO);

	sc = joy_cd.cd_devs[unit];

	if (sc->timeout[i])
		return EBUSY;

	sc->x_off[i] = sc->y_off[i] = 0;
	sc->timeout[i] = JOY_TIMEOUT;
	return 0;
}

int
joyclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = JOYUNIT(dev);
	int i = JOYPART(dev);
	struct joy_softc *sc = joy_cd.cd_devs[unit];

	sc->timeout[i] = 0;
	return 0;
}

int
joyread(dev_t dev, struct uio *uio, int flag)
{
	int unit = JOYUNIT(dev);
	struct joy_softc *sc = joy_cd.cd_devs[unit];
	struct joystick c;
	int port = sc->port;
	int i, t0, t1;
	int state = 0, x = 0, y = 0;

	/* disable_intr(); */ /* XXX BAD */
	isa_outb(port, 0xff);
	t0 = get_tick();
	t1 = t0;
	i = USEC2TICKS(sc->timeout[JOYPART(dev)]);
	while (t0 - t1 < i) {
		state = isa_inb(port);
		if (JOYPART(dev) == 1)
			state >>= 2;
		t1 = get_tick();
		if (t1 > t0)
			t1 -= TIMER_FREQ / hz;
		if (!x && !(state & 0x01))
			x = t1;
		if (!y && !(state & 0x02))
			y = t1;
		if (x && y)
			break;
	}
	/* enable_intr(); */ /* XXX BAD */
	c.x = x ? sc->x_off[JOYPART(dev)] + TICKS2USEC(t0 - x) : 0x80000000;
	c.y = y ? sc->y_off[JOYPART(dev)] + TICKS2USEC(t0 - y) : 0x80000000;
	state >>= 4;
	c.b1 = ~state & 1;
	c.b2 = ~(state >> 1) & 1;
	return uiomove((caddr_t) & c, sizeof(struct joystick), uio);
}

int
joyioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = JOYUNIT(dev);
	struct joy_softc *sc = joy_cd.cd_devs[unit];
	int i = JOYPART(dev);
	int x;

	switch (cmd) {
	case JOY_SETTIMEOUT:
		x = *(int *) data;
		if (x < 1 || x > 10000)	/* 10ms maximum! */
			return EINVAL;
		sc->timeout[i] = x;
		break;
	case JOY_GETTIMEOUT:
		*(int *) data = sc->timeout[i];
		break;
	case JOY_SET_X_OFFSET:
		sc->x_off[i] = *(int *) data;
		break;
	case JOY_SET_Y_OFFSET:
		sc->y_off[i] = *(int *) data;
		break;
	case JOY_GET_X_OFFSET:
		*(int *) data = sc->x_off[i];
		break;
	case JOY_GET_Y_OFFSET:
		*(int *) data = sc->y_off[i];
		break;
	default:
		return ENXIO;
	}
	return 0;
}

static int
get_tick()
{
	int low, high;

	isa_outb(TIMER_MODE, TIMER_SEL0);
	low = isa_inb(TIMER_CNTR0);
	high = isa_inb(TIMER_CNTR0);

	return (high << 8) | low;
}
