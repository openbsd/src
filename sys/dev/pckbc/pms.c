/* $OpenBSD: pms.c,v 1.13 2010/11/15 13:51:20 krw Exp $ */
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

#define DEVNAME(sc)	((sc)->sc_dev.dv_xname)

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	pckbc_tag_t sc_kbctag;
	int sc_kbcslot;

	int sc_state;
#define PMS_STATE_DISABLED	0
#define PMS_STATE_ENABLED	1
#define PMS_STATE_SUSPENDED	2

	int poll;
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

int	pms_cmd(struct pms_softc *, u_char *, int, u_char *, int);
int	pms_get_devid(struct pms_softc *, u_char *);
int	pms_get_status(struct pms_softc *, u_char *);
int	pms_set_rate(struct pms_softc *, int);
int	pms_set_resolution(struct pms_softc *, int);
int	pms_set_scaling(struct pms_softc *, int);
int	pms_reset(struct pms_softc *);
int	pms_dev_enable(struct pms_softc *);
int	pms_dev_disable(struct pms_softc *);

int	pms_setintellimode(struct pms_softc *sc);

const struct wsmouse_accessops pms_accessops = {
	pms_enable,
	pms_ioctl,
	pms_disable,
};

int
pms_cmd(struct pms_softc *sc, u_char *cmd, int len, u_char *resp, int resplen)
{
	if (sc->poll) {
		return pckbc_poll_cmd(sc->sc_kbctag, sc->sc_kbcslot,
		    cmd, len, resplen, resp, 1);
	} else {
		return pckbc_enqueue_cmd(sc->sc_kbctag, sc->sc_kbcslot,
		    cmd, len, resplen, 1, resp);
	}
}

int
pms_get_devid(struct pms_softc *sc, u_char *resp)
{
	u_char cmd[1];

	cmd[0] = PMS_SEND_DEV_ID;
	return (pms_cmd(sc, cmd, 1, resp, 1));
}

int
pms_get_status(struct pms_softc *sc, u_char *resp)
{
	u_char cmd[1];

	cmd[0] = PMS_SEND_DEV_STATUS;
	return (pms_cmd(sc, cmd, 1, resp, 3));
}

int
pms_set_rate(struct pms_softc *sc, int value)
{
	u_char cmd[2];

	cmd[0] = PMS_SET_SAMPLE;
	cmd[1] = value;
	return (pms_cmd(sc, cmd, 2, NULL, 0));
}

int
pms_set_resolution(struct pms_softc *sc, int value)
{
	u_char cmd[2];

	cmd[0] = PMS_SET_RES;
	cmd[1] = value;
	return (pms_cmd(sc, cmd, 2, NULL, 0));
}

int
pms_set_scaling(struct pms_softc *sc, int scale)
{
	u_char cmd[1];

	switch (scale) {
	case 1:
	default:
		cmd[0] = PMS_SET_SCALE11;
		break;
	case 2:
		cmd[0] = PMS_SET_SCALE21;
		break;
	}
	return (pms_cmd(sc, cmd, 1, NULL, 0));
}

int
pms_reset(struct pms_softc *sc)
{
	u_char cmd[1], resp[2];
	int res;

	cmd[0] = PMS_RESET;
	res = pms_cmd(sc, cmd, 1, resp, 2);
#ifdef DEBUG
	if (res || resp[0] != PMS_RSTDONE || resp[1] != 0)
		printf("%s: reset error %d (response 0x%02x, type 0x%02x)\n",
		    DEVNAME(sc), res, resp[0], resp[1]);
#endif
	return (res);
}

int
pms_dev_enable(struct pms_softc *sc)
{
	u_char cmd[1];
	int res;

	cmd[0] = PMS_DEV_ENABLE;
	res = pms_cmd(sc, cmd, 1, NULL, 0);
	if (res)
		printf("%s: enable error\n", DEVNAME(sc));
	return (res);
}

int
pms_dev_disable(struct pms_softc *sc)
{
	u_char cmd[1];
	int res;

	cmd[0] = PMS_DEV_DISABLE;
	res = pms_cmd(sc, cmd, 1, NULL, 0);
	if (res)
		printf("%s: disable error\n", DEVNAME(sc));
	return (res);
}

int
pms_setintellimode(struct pms_softc *sc)
{
	static const int rates[] = {200, 100, 80};
	u_char resp;

	if (pms_set_rate(sc, rates[0]) ||
	    pms_set_rate(sc, rates[1]) ||
	    pms_set_rate(sc, rates[2]) ||
	    pms_get_devid(sc, &resp) ||
	    resp != 0x03)
		return (0);

	return (1);
}

int
pmsprobe(struct device *parent, void *match, void *aux)
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
	if (res || resp[0] != PMS_RSTDONE || resp[1] != 0) {
#ifdef DEBUG
		printf("pms: reset error %d (response 0x%02x, type 0x%02x)\n",
		    res, resp[0], resp[1]);
#endif
		return (0);
	}

	return (1);
}

void
pmsattach(struct device *parent, struct device *self, void *aux)
{
	struct pms_softc *sc = (void *)self;
	struct pckbc_attach_args *pa = aux;
	struct wsmousedev_attach_args a;

	sc->sc_kbctag = pa->pa_tag;
	sc->sc_kbcslot = pa->pa_slot;

	printf("\n");

	pckbc_set_inputhandler(sc->sc_kbctag, sc->sc_kbcslot,
	    pmsinput, sc, DEVNAME(sc));

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
	sc->poll = 1;
	pms_change_state(sc, PMS_STATE_DISABLED);
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
	switch (newstate) {
	case PMS_STATE_ENABLED:
		if (sc->sc_state == PMS_STATE_ENABLED)
			return (EBUSY);

		sc->inputstate = 0;
		sc->oldbuttons = 0;

		pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 1);

		if (sc->poll)
			pckbc_flush(sc->sc_kbctag, sc->sc_kbcslot);

		pms_reset(sc);

		sc->intelli = pms_setintellimode(sc);

		pms_dev_enable(sc);
		break;
	case PMS_STATE_DISABLED:
	case PMS_STATE_SUSPENDED:
		pms_dev_disable(sc);
		pckbc_slot_enable(sc->sc_kbctag, sc->sc_kbcslot, 0);
		break;
	}

	sc->sc_state = newstate;
	sc->poll = (newstate == PMS_STATE_SUSPENDED) ? 1 : 0;

	return (0);
}

int
pms_enable(void *v)
{
	struct pms_softc *sc = v;

	return pms_change_state(sc, PMS_STATE_ENABLED);
}

void
pms_disable(void *v)
{
	struct pms_softc *sc = v;

	pms_change_state(sc, PMS_STATE_DISABLED);
}

int
pms_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
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

void
pmsinput(void *vsc, int data)
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
