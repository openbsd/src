/* $OpenBSD: pms.c,v 1.18 2011/01/03 19:46:34 shadchin Exp $ */
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

struct pms_softc;

struct pms_protocol {
	int type;
#define PMS_STANDARD	0
#define PMS_INTELLI	1
	u_int packetsize;
	int (*enable)(struct pms_softc *);
	int (*ioctl)(struct pms_softc *, u_long, caddr_t, int, struct proc *);
	int (*sync)(struct pms_softc *, int);
	void (*proc)(struct pms_softc *);
	void (*disable)(struct pms_softc *);
};

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	pckbc_tag_t sc_kbctag;

	int sc_state;
#define PMS_STATE_DISABLED	0
#define PMS_STATE_ENABLED	1
#define PMS_STATE_SUSPENDED	2

	int poll;
	int inputstate;

	const struct pms_protocol *protocol;

	u_char packet[8];

	struct device *sc_wsmousedev;
};

#define PMS_BUTTON1DOWN		0x0001	/* left */
#define PMS_BUTTON2DOWN		0x0002	/* middle */
#define PMS_BUTTON3DOWN		0x0004	/* right */

static const u_int butmap[8] = {
	0,
	PMS_BUTTON1DOWN,
	PMS_BUTTON3DOWN,
	PMS_BUTTON1DOWN | PMS_BUTTON3DOWN,
	PMS_BUTTON2DOWN,
	PMS_BUTTON1DOWN | PMS_BUTTON2DOWN,
	PMS_BUTTON2DOWN | PMS_BUTTON3DOWN,
	PMS_BUTTON1DOWN | PMS_BUTTON2DOWN | PMS_BUTTON3DOWN
};

/* PS/2 mouse data packet */
#define PMS_PS2_BUTTONSMASK	0x07
#define PMS_PS2_BUTTON1		0x01	/* left */
#define PMS_PS2_BUTTON2		0x04	/* middle */
#define PMS_PS2_BUTTON3		0x02	/* right */
#define PMS_PS2_XNEG		0x10
#define PMS_PS2_YNEG		0x20

#define PMS_INTELLI_MAGIC1	200
#define PMS_INTELLI_MAGIC2	100
#define PMS_INTELLI_MAGIC3	80
#define PMS_INTELLI_ID		0x03

int	pmsprobe(struct device *, void *, void *);
void	pmsattach(struct device *, struct device *, void *);
int	pmsactivate(struct device *, int);

void	pmsinput(void *, int);

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

int	pms_enable_intelli(struct pms_softc *);

int	pms_ioctl_mouse(struct pms_softc *, u_long, caddr_t, int, struct proc *);
int	pms_sync_mouse(struct pms_softc *, int);
void	pms_proc_mouse(struct pms_softc *);

struct cfattach pms_ca = {
	sizeof(struct pms_softc), pmsprobe, pmsattach, NULL,
	pmsactivate
};

struct cfdriver pms_cd = {
	NULL, "pms", DV_DULL
};

const struct wsmouse_accessops pms_accessops = {
	pms_enable,
	pms_ioctl,
	pms_disable,
};

const struct pms_protocol pms_mouse[] = {
	/* Generic PS/2 mouse */
	{
		PMS_STANDARD, 3,
		NULL,
		pms_ioctl_mouse,
		pms_sync_mouse,
		pms_proc_mouse,
		NULL
	},
	/* Microsoft IntelliMouse */
	{
		PMS_INTELLI, 4,
		pms_enable_intelli,
		pms_ioctl_mouse,
		pms_sync_mouse,
		pms_proc_mouse,
		NULL
	}
};

int
pms_cmd(struct pms_softc *sc, u_char *cmd, int len, u_char *resp, int resplen)
{
	if (sc->poll) {
		return pckbc_poll_cmd(sc->sc_kbctag, PCKBC_AUX_SLOT,
		    cmd, len, resplen, resp, 1);
	} else {
		return pckbc_enqueue_cmd(sc->sc_kbctag, PCKBC_AUX_SLOT,
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
pms_enable_intelli(struct pms_softc *sc)
{
	u_char resp;

	/* the special sequence to enable the third button and the roller */
	if (pms_set_rate(sc, PMS_INTELLI_MAGIC1) ||
	    pms_set_rate(sc, PMS_INTELLI_MAGIC2) ||
	    pms_set_rate(sc, PMS_INTELLI_MAGIC3) ||
	    pms_get_devid(sc, &resp) ||
	    resp != PMS_INTELLI_ID)
		return (0);

	return (1);
}

int
pms_ioctl_mouse(struct pms_softc *sc, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
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

		if (pms_set_resolution(sc, i))
			printf("%s: SET_RES command error\n", DEVNAME(sc));
		break;
	default:
		return (-1);
	}
	return (0);
}

int
pms_sync_mouse(struct pms_softc *sc, int data)
{
	if (sc->inputstate != 0)
		return (0);

	switch (sc->protocol->type) {
	case PMS_STANDARD:
		if ((data & 0xc0) != 0)
			return (-1);
		break;
	case PMS_INTELLI:
		if ((data & 0x08) != 0x08)
			return (-1);
		break;
	}

	return (0);
}

void
pms_proc_mouse(struct pms_softc *sc)
{
	u_int buttons;
	int  dx, dy, dz;

	buttons = butmap[sc->packet[0] & PMS_PS2_BUTTONSMASK];
	dx = (sc->packet[0] & PMS_PS2_XNEG) ?
	    (int)sc->packet[1] - 256 : sc->packet[1];
	dy = (sc->packet[0] & PMS_PS2_YNEG) ?
	    (int)sc->packet[2] - 256 : sc->packet[2];

	switch (sc->protocol->type) {
	case PMS_STANDARD:
		dz = 0;
		break;
	case PMS_INTELLI:
		dz = (signed char)sc->packet[3];
		break;
	}

	wsmouse_input(sc->sc_wsmousedev,
	    buttons, dx, dy, dz, 0, WSMOUSE_INPUT_DELTA);
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

	printf("\n");

	pckbc_set_inputhandler(sc->sc_kbctag, PCKBC_AUX_SLOT,
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
	pms_change_state(sc, PMS_STATE_ENABLED);
	sc->poll = 1; /* XXX */
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
	int i;

	switch (newstate) {
	case PMS_STATE_ENABLED:
		if (sc->sc_state == PMS_STATE_ENABLED)
			return (EBUSY);

		sc->inputstate = 0;

		pckbc_slot_enable(sc->sc_kbctag, PCKBC_AUX_SLOT, 1);

		if (sc->poll)
			pckbc_flush(sc->sc_kbctag, PCKBC_AUX_SLOT);

		pms_reset(sc);

		sc->protocol = &pms_mouse[0];
		for (i = 1; i < nitems(pms_mouse); i++)
			if (pms_mouse[i].enable(sc))
				sc->protocol = &pms_mouse[i];

#ifdef DEBUG
		printf("%s: protocol type %d\n", DEVNAME(sc), sc->protocol->type);
#endif

		pms_dev_enable(sc);
		break;
	case PMS_STATE_DISABLED:
	case PMS_STATE_SUSPENDED:
		pms_dev_disable(sc);

		if (sc->protocol && sc->protocol->disable)
			sc->protocol->disable(sc);

		pckbc_slot_enable(sc->sc_kbctag, PCKBC_AUX_SLOT, 0);
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

	if (sc->protocol && sc->protocol->ioctl)
		return (sc->protocol->ioctl(sc, cmd, data, flag, p));
	else
		return (-1);
}

void
pmsinput(void *vsc, int data)
{
	struct pms_softc *sc = vsc;

	if (sc->sc_state != PMS_STATE_ENABLED) {
		/* Interrupts are not expected.  Discard the byte. */
		return;
	}

	if (sc->protocol->sync(sc, data)) {
#ifdef DEBUG
		printf("%s: not in sync yet, discard input\n", DEVNAME(sc));
#endif
		sc->inputstate = 0;
		return;
	}

	sc->packet[sc->inputstate++] = data;
	if (sc->inputstate != sc->protocol->packetsize)
		return;

	sc->protocol->proc(sc);
	sc->inputstate = 0;
}
