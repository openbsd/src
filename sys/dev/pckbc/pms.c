/* $OpenBSD: pms.c,v 1.6 2010/09/29 19:39:18 deraadt Exp $ */
/* $NetBSD: psm.c,v 1.11 2000/06/05 22:20:57 sommerfeld Exp $ */

/*-
 * Copyright (c) 1994 Charles M. Hannum.
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/ioctl.h>

#include <machine/bus.h>

#include <dev/ic/pckbcvar.h>

#include <dev/pckbc/pmsreg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	pckbc_tag_t sc_kbctag;
	int sc_kbcslot;

	int sc_state;
#define PMS_STATE_DISABLED	0
#define PMS_STATE_ENABLED	1
#define PMS_STATE_SUSPENDED	2

	int intelli;
	int inputstate;
	u_int buttons, oldbuttons;	/* mouse button status */
	signed char dx, dy;

	struct device *sc_wsmousedev;
};

int pmsprobe(struct device *, void *, void *);
void pmsattach(struct device *, struct device *, void *);
int pmsactivate(struct device *, int);
void pmsinput(void *, int);

struct cfattach pms_ca = {
	sizeof(struct pms_softc), pmsprobe, pmsattach, NULL,
	pmsactivate
};

int	pms_change_state(struct pms_softc *, int);
int	pms_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	pms_enable(void *);
void	pms_disable(void *);

int	pms_setintellimode(pckbc_tag_t, pckbc_slot_t);

const struct wsmouse_accessops pms_accessops = {
	pms_enable,
	pms_ioctl,
	pms_disable,
};

int
pms_setintellimode(pckbc_tag_t tag, pckbc_slot_t slot)
{
	u_char cmd[2], resp[1];
	int i, res;
	static const u_char rates[] = {200, 100, 80};

	cmd[0] = PMS_SET_SAMPLE;
	for (i = 0; i < 3; i++) {
		cmd[1] = rates[i];
		res = pckbc_enqueue_cmd(tag, slot, cmd, 2, 0, 0, NULL);
		if (res)
			return (0);
	}

	cmd[0] = PMS_SEND_DEV_ID;
	res = pckbc_enqueue_cmd(tag, slot, cmd, 1, 1, 1, resp);
	if (res || resp[0] != 3)
		return (0);

	return (1);
}

int
pmsprobe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pckbc_attach_args *pa = aux;
	u_char cmd[1], resp[2];
	int res;

	if (pa->pa_slot != PCKBC_AUX_SLOT)
		return (0);

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* reset the device */
	cmd[0] = PMS_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 2, resp, 1);
	if (res) {
#ifdef DEBUG
		printf("pmsprobe: reset error %d\n", res);
#endif
		return (0);
	}
	if (resp[0] != PMS_RSTDONE) {
		printf("pmsprobe: reset response 0x%x\n", resp[0]);
		return (0);
	}

	/* get type number (0 = mouse) */
	if (resp[1] != 0) {
#ifdef DEBUG
		printf("pmsprobe: type 0x%x\n", resp[1]);
#endif
		return (0);
	}

	return (1);
}

void
pmsattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pms_softc *sc = (void *)self;
	struct pckbc_attach_args *pa = aux;
	struct wsmousedev_attach_args a;
	u_char cmd[1], resp[2];
	int res;

	sc->sc_kbctag = pa->pa_tag;
	sc->sc_kbcslot = pa->pa_slot;

	printf("\n");

	/* Flush any garbage. */
	pckbc_flush(pa->pa_tag, pa->pa_slot);

	/* reset the device */
	cmd[0] = PMS_RESET;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 2, resp, 1);
#ifdef DEBUG
	if (res || resp[0] != PMS_RSTDONE || resp[1] != 0) {
		printf("pmsattach: reset error\n");
		return;
	}
#endif

	sc->inputstate = 0;
	sc->oldbuttons = 0;

	pckbc_set_inputhandler(sc->sc_kbctag, sc->sc_kbcslot,
			       pmsinput, sc, sc->sc_dev.dv_xname);

	a.accessops = &pms_accessops;
	a.accesscookie = sc;

	/*
	 * Attach the wsmouse, saving a handle to it.
	 * Note that we don't need to check this pointer against NULL
	 * here or in pmsintr, because if this fails pms_enable() will
	 * never be called, so pmsinput() will never be called.
	 */
	sc->sc_wsmousedev = config_found(self, &a, wsmousedevprint);

	/* no interrupts until enabled */
	cmd[0] = PMS_DEV_DISABLE;
	res = pckbc_poll_cmd(pa->pa_tag, pa->pa_slot, cmd, 1, 0, NULL, 0);
	if (res)
		printf("pmsattach: disable error\n");
	pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 0);
}

int
pmsactivate(struct device *self, int act)
{
	struct pms_softc *sc = (struct pms_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		if (sc->sc_state == PMS_STATE_ENABLED)
			pms_change_state(sc, PMS_STATE_SUSPENDED);
		break;
	case DVACT_RESUME:
		if (sc->sc_state == PMS_STATE_SUSPENDED)
			pms_change_state(sc, PMS_STATE_ENABLED);
		break;
	}
	return (0);
}

int
pms_change_state(struct pms_softc *sc, int newstate)
{
	u_char cmd[1];
	int res;

	switch (newstate) {
	case PMS_STATE_ENABLED:
		if (sc->sc_state == PMS_STATE_ENABLED)
			return EBUSY;
		sc->inputstate = 0;
		sc->oldbuttons = 0;

		pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 1);

		pckbc_flush(sc->sc_kbctag, sc->sc_kbcslot);
		sc->intelli = pms_setintellimode(sc->sc_kbctag, sc->sc_kbcslot);

		cmd[0] = PMS_DEV_ENABLE;
		res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot,
		    cmd, 1, 0, 1, 0);
		if (res)
			printf("pms_enable: command error\n");
#if 0
		{
			u_char scmd[2];

			scmd[0] = PMS_SET_RES;
			scmd[1] = 3; /* 8 counts/mm */
			res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
						2, 0, 1, 0);
			if (res)
				printf("pms_enable: setup error1 (%d)\n", res);

			scmd[0] = PMS_SET_SCALE21;
			res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
						1, 0, 1, 0);
			if (res)
				printf("pms_enable: setup error2 (%d)\n", res);

			scmd[0] = PMS_SET_SAMPLE;
			scmd[1] = 100; /* 100 samples/sec */
			res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, scmd,
						2, 0, 1, 0);
			if (res)
				printf("pms_enable: setup error3 (%d)\n", res);
		}
#endif
		sc->sc_state = newstate;
		break;
	case PMS_STATE_DISABLED:

		/* FALLTHROUGH */
	case PMS_STATE_SUSPENDED:
		cmd[0] = PMS_DEV_DISABLE;
		res = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot,
		    cmd, 1, 0, 1, 0);
		if (res)
			printf("pms_disable: command error\n");
		pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 0);
		sc->sc_state = newstate;
		break;
	}
	return 0;
}

int
pms_enable(v)
	void *v;
{
	struct pms_softc *sc = v;

	return pms_change_state(sc, PMS_STATE_ENABLED);
}

void
pms_disable(v)
	void *v;
{
	struct pms_softc *sc = v;

	pms_change_state(sc, PMS_STATE_DISABLED);
}

int
pms_ioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct pms_softc *sc = v;
	u_char kbcmd[2];
	int i;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		break;
		
	case WSMOUSEIO_SRES:
		i = ((int) *(u_int *)data - 12) / 25;		
		/* valid values are {0,1,2,3} */
		if (i < 0)
			i = 0;
		if (i > 3)
			i = 3;
		
		kbcmd[0] = PMS_SET_RES;
		kbcmd[1] = (unsigned char) i;			
		i = pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot, kbcmd, 
		    2, 0, 1, 0);
		
		if (i)
			printf("pms_ioctl: SET_RES command error\n");
		break;
		
	default:
		return (-1);
	}
	return (0);
}

/* Masks for the first byte of a packet */
#define PS2LBUTMASK 0x01
#define PS2RBUTMASK 0x02
#define PS2MBUTMASK 0x04

void pmsinput(vsc, data)
void *vsc;
int data;
{
	struct pms_softc *sc = vsc;
	signed char dz = 0;
	u_int changed;

	if (sc->sc_state != PMS_STATE_ENABLED) {
		/* Interrupts are not expected.  Discard the byte. */
		return;
	}

	switch (sc->inputstate) {

	case 0:
		if ((data & 0xc0) == 0) { /* no ovfl, bit 3 == 1 too? */
			sc->buttons = ((data & PS2LBUTMASK) ? 0x1 : 0) |
			    ((data & PS2MBUTMASK) ? 0x2 : 0) |
			    ((data & PS2RBUTMASK) ? 0x4 : 0);
			++sc->inputstate;
		}
		break;

	case 1:
		sc->dx = data;
		/* Bounding at -127 avoids a bug in XFree86. */
		sc->dx = (sc->dx == -128) ? -127 : sc->dx;
		++sc->inputstate;
		break;

	case 2:
		sc->dy = data;
		sc->dy = (sc->dy == -128) ? -127 : sc->dy;
		++sc->inputstate;
		break;

	case 3:
		dz = data;
		dz = (dz == -128) ? -127 : dz;
		++sc->inputstate;
		break;
	}

	if ((sc->inputstate == 3 && sc->intelli == 0) || sc->inputstate == 4) {
		sc->inputstate = 0;

		changed = (sc->buttons ^ sc->oldbuttons);
		sc->oldbuttons = sc->buttons;

		if (sc->dx || sc->dy || dz || changed)
			wsmouse_input(sc->sc_wsmousedev,
				      sc->buttons, sc->dx, sc->dy, dz, 0,
				      WSMOUSE_INPUT_DELTA);
	}

	return;
}

struct cfdriver pms_cd = {
	NULL, "pms", DV_DULL
};
