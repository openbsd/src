/*-
 * Copyright (c) 1995 Jean-Marc Zucconi
 * All rights reserved.
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

#include <errno.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/cpufunc.h>

#include <machine/joystick.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>
#include <i386/isa/timerreg.h>

/* The game port can manage 4 buttons and 4 variable resistors (usually 2
 * joysticks, each with 2 buttons and 2 pots.) via the port at address 0x201.
 * Getting the state of the buttons is done by reading the game port:
 * buttons 1-4 correspond to bits 4-7 and resistors 1-4 (X1, Y1, X2, Y2)
 * to bits 0-3.
 * if button 1 (resp 2, 3, 4) is pressed, the bit 4 (resp 5, 6, 7) is set to 0
 * to get the value of a resistor, write the value 0xff at port and
 * wait until the corresponding bit returns to 0.
 */


/* the formulae below only work if u is  ``not too large''. See also
 * the discussion in microtime.s */
#define usec2ticks(u) 	(((u) * 19549)>>14)
#define ticks2usec(u) 	(((u) * 3433)>>12)


#define joypart(d) minor(d)&1
#define JOYUNIT(d) minor(d)>>1&3

#ifndef JOY_TIMEOUT
#define JOY_TIMEOUT   2000	/* 2 milliseconds */
#endif

struct joy_softc {
	struct device sc_dev;
	int     port;
	int     x_off[2], y_off[2];
	int     timeout[2];
};


int joyprobe __P((struct device *, void *, void *));
void joyattach __P((struct device *, struct device *, void *));
int joyopen __P((dev_t, int, int, struct proc *));
int joyclose __P((dev_t, int, int, struct proc *));
static int get_tick __P((void));

struct cfdriver joycd = {
	NULL, "joy", joyprobe, joyattach, DV_DULL, sizeof(struct joy_softc)
};


int
joyprobe(parent, match, aux)
	struct device *parent;
	void   *match, *aux;

{
#ifdef WANT_JOYSTICK_CONNECTED
	outb(dev->id_iobase, 0xff);
	DELAY(10000);		/* 10 ms delay */
	return (inb(dev->id_iobase) & 0x0f) != 0x0f;
#else
	return 1;
#endif
}

void
joyattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;
{
	struct joy_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	int     unit = sc->sc_dev.dv_unit;

	sc->port = ia->ia_iobase;
	sc->timeout[0] = sc->timeout[1] = 0;
	printf(": joystick\n", unit);
}

int
joyopen(dev, flag, mode, p)
	dev_t   dev;
	int     flag, mode;
	struct proc *p;
{
	int     unit = JOYUNIT(dev);
	int     i = joypart(dev);
	struct joy_softc *sc;

	if (unit >= joycd.cd_ndevs) {
		return (ENXIO);
	}
	sc = joycd.cd_devs[unit];

	if (sc->timeout[i]) {
		return EBUSY;
	}
	sc->x_off[i] = sc->y_off[i] = 0;
	sc->timeout[i] = JOY_TIMEOUT;
	return 0;
}

int
joyclose(dev, flag, mode, p)
	dev_t   dev;
	int     flag, mode;
	struct proc *p;
{
	int     unit = JOYUNIT(dev);
	int     i = joypart(dev);
	struct joy_softc *sc = joycd.cd_devs[unit];

	sc->timeout[i] = 0;
	return 0;
}

int
joyread(dev, uio, flag)
	dev_t   dev;
	struct uio *uio;
	int     flag;
{
	int     unit = JOYUNIT(dev);
	struct joy_softc *sc = joycd.cd_devs[unit];
	int     port = sc->port;
	int     i, t0, t1;
	int     state = 0, x = 0, y = 0;
	struct joystick c;

	disable_intr();
	outb(port, 0xff);
	t0 = get_tick();
	t1 = t0;
	i = usec2ticks(sc->timeout[joypart(dev)]);
	while (t0 - t1 < i) {
		state = inb(port);
		if (joypart(dev) == 1)
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
	enable_intr();
	c.x = x ? sc->x_off[joypart(dev)] + ticks2usec(t0 - x) : 0x80000000;
	c.y = y ? sc->y_off[joypart(dev)] + ticks2usec(t0 - y) : 0x80000000;
	state >>= 4;
	c.b1 = ~state & 1;
	c.b2 = ~(state >> 1) & 1;
	return uiomove((caddr_t) & c, sizeof(struct joystick), uio);
}

int
joyioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	u_long  cmd;
	caddr_t data;
	int     flag;
	struct proc *p;
{
	int     unit = JOYUNIT(dev);
	int     i = joypart(dev);
	struct joy_softc *sc = joycd.cd_devs[unit];
	int     x;

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
	int     low, high;

	outb(TIMER_MODE, TIMER_SEL0);
	low = inb(TIMER_CNTR0);
	high = inb(TIMER_CNTR0);

	return (high << 8) | low;
}
