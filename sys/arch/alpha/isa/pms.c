/*	$NetBSD: pms.c,v 1.1 1996/04/12 01:53:06 cgd Exp $	*/

/*-
 * Copyright (c) 1994 Charles Hannum.
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

/*
 * XXXX
 * This is a hack.  This driver should really be combined with the
 * keyboard driver, since they go through the same buffer and use the
 * same I/O ports.  Frobbing the mouse and keyboard at the same time
 * may result in dropped characters and/or corrupted mouse events.
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
#include <sys/vnode.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <dev/isa/isavar.h>
#include <alpha/wscons/wsconsvar.h>

#define	PMS_DATA	0x60	/* offset for data port, read-write */
#define	PMS_CNTRL	0x64	/* offset for control port, write-only */
#define	PMS_STATUS	0x64	/* offset for status port, read-only */
#define	PMS_NPORTS	8

/* status bits */
#define	PMS_OBUF_FULL	0x01
#define	PMS_IBUF_FULL	0x02

/* controller commands */
#define	PMS_INT_ENABLE	0x47	/* enable controller interrupts */
#define	PMS_INT_DISABLE	0x65	/* disable controller interrupts */
#define	PMS_AUX_ENABLE	0xa8	/* enable auxiliary port */
#define	PMS_AUX_DISABLE	0xa7	/* disable auxiliary port */
#define	PMS_AUX_TEST	0xa9	/* test auxiliary port */

#define	PMS_8042_CMD	0x65

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
#define	PMS_BSIZE	1020	/* buffer size */

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	void *sc_ih;

	u_char sc_state;	/* mouse driver state */
#define	PMS_OPEN	0x01	/* device is open */
#define	PMS_ASLP	0x02	/* waiting for mouse data */
	u_char sc_status;	/* mouse button status */
	int sc_x, sc_y;		/* accumulated motion in the X,Y axis */
};

bus_chipset_tag_t pms_bc;
isa_chipset_tag_t pms_ic;
bus_io_handle_t pms_cntrl_ioh;
#define	pms_status_ioh	pms_cntrl_ioh
bus_io_handle_t pms_data_ioh;

int pmsprobe __P((struct device *, void *, void *));
void pmsattach __P((struct device *, struct device *, void *));
int pmsintr __P((void *));

struct cfattach pms_ca = {
	sizeof(struct pms_softc), pmsprobe, pmsattach,
};

struct cfdriver pms_cd = {
	NULL, "pms", DV_TTY,
};

#define	PMSUNIT(dev)	(minor(dev))

int	pms_enable __P((struct device *));
int	pms_disable __P((struct device *));

struct wscons_mdev_spec pms_mdev_spec = {
	pms_enable,
	pms_disable,
};

static inline void
pms_flush()
{
	u_char c;

	while (c = bus_io_read_1(pms_bc, pms_status_ioh, 0) & 0x03)
		if ((c & PMS_OBUF_FULL) == PMS_OBUF_FULL) {
			/* XXX - delay is needed to prevent some keyboards from
			   wedging when the system boots */
			delay(6);
			(void) bus_io_read_1(pms_bc, pms_data_ioh, 0);
		}
}

static inline void
pms_dev_cmd(value)
	u_char value;
{

	pms_flush();
	bus_io_write_1(pms_bc, pms_cntrl_ioh, 0, 0xd4);
	pms_flush();
	bus_io_write_1(pms_bc, pms_data_ioh, 0, value);
}

static inline void
pms_aux_cmd(value)
	u_char value;
{

	pms_flush();
	bus_io_write_1(pms_bc, pms_cntrl_ioh, 0, value);
}

static inline void
pms_pit_cmd(value)
	u_char value;
{

	pms_flush();
	bus_io_write_1(pms_bc, pms_cntrl_ioh, 0, 0x60);
	pms_flush();
	bus_io_write_1(pms_bc, pms_data_ioh, 0, value);
}

int
pmsprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct isa_attach_args *ia = aux;
	u_char x;

	pms_bc = ia->ia_bc;

	if (ia->ia_iobase != 0x60)
		return 0;

	if (bus_io_map(pms_bc, PMS_DATA, 1, &pms_data_ioh) ||
	    bus_io_map(pms_bc, PMS_CNTRL, 1, &pms_cntrl_ioh))
		return 0;

	pms_dev_cmd(PMS_RESET);
	pms_aux_cmd(PMS_AUX_TEST);
	delay(1000);
	x = bus_io_read_1(pms_bc, pms_data_ioh, 0);
	pms_pit_cmd(PMS_INT_DISABLE);
	if (x & 0x04)
		return 0;

	ia->ia_iosize = PMS_NPORTS;
	ia->ia_msize = 0;
	return 1;
}

void
pmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pms_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;

	pms_bc = ia->ia_bc;
	pms_ic = ia->ia_ic;

	if (bus_io_map(pms_bc, PMS_DATA, 1, &pms_data_ioh) ||
	    bus_io_map(pms_bc, PMS_CNTRL, 1, &pms_cntrl_ioh)) {
		printf(": can't map I/O ports!\n");
		return;
	}

	msattach(self, &pms_mdev_spec);

	printf("\n");

	/* Other initialization was done by pmsprobe. */
	sc->sc_state = 0;

	sc->sc_ih = isa_intr_establish(pms_ic, ia->ia_irq, IST_EDGE, IPL_TTY,
	    pmsintr, sc);
}

int
pms_enable(dev)
	struct device *dev;
{
	struct pms_softc *sc = (struct pms_softc *)dev;

	if (sc->sc_state & PMS_OPEN)
		return EBUSY;

	sc->sc_state |= PMS_OPEN;
	sc->sc_status = 0;
	sc->sc_x = sc->sc_y = 0;

	/* Enable interrupts. */
	pms_dev_cmd(PMS_DEV_ENABLE);
	pms_aux_cmd(PMS_AUX_ENABLE);
#if 0
	pms_dev_cmd(PMS_SET_RES);
	pms_dev_cmd(3);		/* 8 counts/mm */
	pms_dev_cmd(PMS_SET_SCALE21);
	pms_dev_cmd(PMS_SET_SAMPLE);
	pms_dev_cmd(100);	/* 100 samples/sec */
	pms_dev_cmd(PMS_SET_STREAM);
#endif
	pms_pit_cmd(PMS_INT_ENABLE);

	return 0;
}

int
pms_disable(dev)
	struct device *dev;
{
	struct pms_softc *sc = (struct pms_softc *)dev;

	/* Disable interrupts. */
	pms_dev_cmd(PMS_DEV_DISABLE);
	pms_pit_cmd(PMS_INT_DISABLE);
	pms_aux_cmd(PMS_AUX_DISABLE);

	sc->sc_state &= ~PMS_OPEN;

	return 0;
}

/* Masks for the first byte of a packet */
#define PS2LBUTMASK 0x01
#define PS2RBUTMASK 0x02
#define PS2MBUTMASK 0x04

int
pmsintr(arg)
	void *arg;
{
	struct pms_softc *sc = arg;
	static int state = 0;
	static u_char buttons;
	u_char changed;
	static char dx, dy;
	u_char buffer[5];

	if ((sc->sc_state & PMS_OPEN) == 0) {
		/* Interrupts are not expected.  Discard the byte. */
		pms_flush();
		return 0;
	}

	switch (state) {

	case 0:
		buttons = bus_io_read_1(pms_bc, pms_data_ioh, 0);
		if ((buttons & 0xc0) == 0)
			++state;
		break;

	case 1:
		dx = bus_io_read_1(pms_bc, pms_data_ioh, 0);
		/* Bounding at -127 avoids a bug in XFree86. */
		dx = (dx == -128) ? -127 : dx;
		++state;
		break;

	case 2:
		dy = bus_io_read_1(pms_bc, pms_data_ioh, 0);
		dy = (dy == -128) ? -127 : dy;
		state = 0;

		buttons = ((buttons & PS2LBUTMASK) << 2) |
			  ((buttons & (PS2RBUTMASK | PS2MBUTMASK)) >> 1);
		changed = (buttons ^ sc->sc_status);
		sc->sc_status = buttons;

		if (dx || dy || changed)
			ms_event(buttons, dx, dy);
		break;
	}

	return -1;
}
