/* $OpenBSD: pms.c,v 1.24 2011/10/17 17:12:01 mpi Exp $ */
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
#include <sys/malloc.h>

#include <machine/bus.h>

#include <dev/ic/pckbcvar.h>

#include <dev/pckbc/pmsreg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsmousevar.h>

#ifdef DEBUG
#define DPRINTF(x...)	do { printf(x); } while (0);
#else
#define DPRINTF(x...)
#endif

#define DEVNAME(sc)	((sc)->sc_dev.dv_xname)

#define WSMOUSE_BUTTON(x)	(1 << ((x) - 1))

struct pms_softc;

struct pms_protocol {
	int type;
#define PMS_STANDARD	0
#define PMS_INTELLI	1
#define PMS_SYNAPTICS	2
#define PMS_ALPS	3
	u_int packetsize;
	int (*enable)(struct pms_softc *);
	int (*ioctl)(struct pms_softc *, u_long, caddr_t, int, struct proc *);
	int (*sync)(struct pms_softc *, int);
	void (*proc)(struct pms_softc *);
	void (*disable)(struct pms_softc *);
};

struct synaptics_softc {
	int identify;
	int capabilities, ext_capabilities;
	int model, ext_model;
	int resolution, dimension;

	int mode;

	int res_x, res_y;
	int min_x, min_y;
	int max_x, max_y;

	/* Compat mode */
	int wsmode;
	int old_x, old_y;
	u_int old_buttons;
#define SYNAPTICS_SCALE		4
#define SYNAPTICS_PRESSURE	30
};

struct alps_softc {
	int model;
	int mask;
	int version;

	int min_x, min_y;
	int max_x, max_y;
	int old_fin;

	/* Compat mode */
	int wsmode;
	int old_x, old_y;
	u_int old_buttons;
#define ALPS_PRESSURE	40
};

struct pms_softc {		/* driver status information */
	struct device sc_dev;

	pckbc_tag_t sc_kbctag;

	int sc_state;
#define PMS_STATE_DISABLED	0
#define PMS_STATE_ENABLED	1
#define PMS_STATE_SUSPENDED	2

	int sc_dev_enable;
#define PMS_DEV_IGNORE		0x00
#define PMS_DEV_PRIMARY		0x01
#define PMS_DEV_SECONDARY	0x02

	int poll;
	int inputstate;

	const struct pms_protocol *protocol;
	struct synaptics_softc *synaptics;
	struct alps_softc *alps;

	u_char packet[8];

	struct device *sc_wsmousedev;
	struct device *sc_pt_wsmousedev;
};

static const u_int butmap[8] = {
	0,
	WSMOUSE_BUTTON(1),
	WSMOUSE_BUTTON(3),
	WSMOUSE_BUTTON(1) | WSMOUSE_BUTTON(3),
	WSMOUSE_BUTTON(2),
	WSMOUSE_BUTTON(1) | WSMOUSE_BUTTON(2),
	WSMOUSE_BUTTON(2) | WSMOUSE_BUTTON(3),
	WSMOUSE_BUTTON(1) | WSMOUSE_BUTTON(2) | WSMOUSE_BUTTON(3)
};

static const struct alps_model {
	int version;
	int mask;
	int model;
} alps_models[] = {
#if 0
	/* FIXME some clipads are not working yet */
	{ 0x2021, 0xf8, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x2221, 0xf8, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x2222, 0xff, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x3222, 0xf8, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x5212, 0xff, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x6222, 0xcf, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x633b, 0xf8, ALPS_DUALPOINT | ALPS_PASSTHROUGH },
	{ 0x7301, 0xf8, ALPS_DUALPOINT },
#endif
	{ 0x5321, 0xf8, ALPS_GLIDEPOINT },
	{ 0x5322, 0xf8, ALPS_GLIDEPOINT },
	{ 0x603b, 0xf8, ALPS_GLIDEPOINT },
	{ 0x6321, 0xf8, ALPS_GLIDEPOINT },
	{ 0x6322, 0xf8, ALPS_GLIDEPOINT },
	{ 0x6323, 0xf8, ALPS_GLIDEPOINT },
	{ 0x6324, 0x8f, ALPS_GLIDEPOINT },
	{ 0x6325, 0xef, ALPS_GLIDEPOINT },
	{ 0x6326, 0xf8, ALPS_GLIDEPOINT },
	{ 0x7321, 0xf8, ALPS_GLIDEPOINT },
	{ 0x7322, 0xf8, ALPS_GLIDEPOINT },
	{ 0x7325, 0xcf, ALPS_GLIDEPOINT },
#if 0
	{ 0x7326, 0, 0 },	/* XXX Uses unknown v3 protocol */
#endif
};

int	pmsprobe(struct device *, void *, void *);
void	pmsattach(struct device *, struct device *, void *);
int	pmsactivate(struct device *, int);

void	pmsinput(void *, int);

int	pms_change_state(struct pms_softc *, int, int);
int	pms_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	pms_enable(void *);
void	pms_disable(void *);

int	pms_cmd(struct pms_softc *, u_char *, int, u_char *, int);
int	pms_spec_cmd(struct pms_softc *, int);
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

int	pms_enable_synaptics(struct pms_softc *);
int	pms_ioctl_synaptics(struct pms_softc *, u_long, caddr_t, int, struct proc *);
int	pms_sync_synaptics(struct pms_softc *, int);
void	pms_proc_synaptics(struct pms_softc *);
void	pms_disable_synaptics(struct pms_softc *);

int	pms_enable_alps(struct pms_softc *);
int	pms_ioctl_alps(struct pms_softc *, u_long, caddr_t, int, struct proc *);
int	pms_sync_alps(struct pms_softc *, int);
void	pms_proc_alps(struct pms_softc *);

int	synaptics_set_mode(struct pms_softc *, int);
int	synaptics_query(struct pms_softc *, int, int *);
int	synaptics_get_hwinfo(struct pms_softc *);

void	synaptics_pt_proc(struct pms_softc *);

int	synaptics_pt_ioctl(void *, u_long, caddr_t, int, struct proc *);
int	synaptics_pt_enable(void *);
void	synaptics_pt_disable(void *);

int	alps_get_hwinfo(struct pms_softc *);

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

const struct wsmouse_accessops synaptics_pt_accessops = {
	synaptics_pt_enable,
	synaptics_pt_ioctl,
	synaptics_pt_disable,
};

const struct pms_protocol pms_protocols[] = {
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
	},
	/* Synaptics touchpad */
	{
		PMS_SYNAPTICS, 6,
		pms_enable_synaptics,
		pms_ioctl_synaptics,
		pms_sync_synaptics,
		pms_proc_synaptics,
		pms_disable_synaptics
	},
	/* ALPS touchpad */
	{
		PMS_ALPS, 6,
		pms_enable_alps,
		pms_ioctl_alps,
		pms_sync_alps,
		pms_proc_alps,
		NULL
	},
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
pms_spec_cmd(struct pms_softc *sc, int cmd)
{
	if (pms_set_scaling(sc, 1) ||
	    pms_set_resolution(sc, (cmd >> 6) & 0x03) ||
	    pms_set_resolution(sc, (cmd >> 4) & 0x03) ||
	    pms_set_resolution(sc, (cmd >> 2) & 0x03) ||
	    pms_set_resolution(sc, (cmd >> 0) & 0x03))
		return (-1);
	return (0);
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
	int i;

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

	sc->poll = 1;
	sc->sc_dev_enable = 0;

	sc->protocol = &pms_protocols[0];
	for (i = 1; i < nitems(pms_protocols); i++) {
		pms_reset(sc);
		if (pms_protocols[i].enable(sc))
			sc->protocol = &pms_protocols[i];
	}

	DPRINTF("%s: protocol type %d\n", DEVNAME(sc), sc->protocol->type);

	/* no interrupts until enabled */
	pms_change_state(sc, PMS_STATE_DISABLED, PMS_DEV_IGNORE);
}

int
pmsactivate(struct device *self, int act)
{
	struct pms_softc *sc = (struct pms_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		if (sc->sc_state == PMS_STATE_ENABLED)
			pms_change_state(sc, PMS_STATE_SUSPENDED,
			    PMS_DEV_IGNORE);
		break;
	case DVACT_RESUME:
		if (sc->sc_state == PMS_STATE_SUSPENDED)
			pms_change_state(sc, PMS_STATE_ENABLED,
			    PMS_DEV_IGNORE);
		break;
	}
	return (0);
}

int
pms_change_state(struct pms_softc *sc, int newstate, int dev)
{
	int i;

	if (dev != PMS_DEV_IGNORE) {
		switch (newstate) {
		case PMS_STATE_ENABLED:
			if (sc->sc_dev_enable & dev)
				return (EBUSY);

			sc->sc_dev_enable |= dev;

			if (sc->sc_state == PMS_STATE_ENABLED)
				return (0);

			break;
		case PMS_STATE_DISABLED:
			sc->sc_dev_enable &= ~dev;

			if (sc->sc_dev_enable)
				return (0);

			break;
		}
	}

	switch (newstate) {
	case PMS_STATE_ENABLED:
		sc->inputstate = 0;

		pckbc_slot_enable(sc->sc_kbctag, PCKBC_AUX_SLOT, 1);

		if (sc->poll)
			pckbc_flush(sc->sc_kbctag, PCKBC_AUX_SLOT);

		pms_reset(sc);

		if (sc->protocol->type != PMS_STANDARD &&
		    sc->protocol->enable(sc) == 0)
			sc->protocol = &pms_protocols[0];

		if (sc->protocol->type == PMS_STANDARD)
			for (i = 1; i < nitems(pms_protocols); i++) {
				pms_reset(sc);
				if (pms_protocols[i].enable(sc))
					sc->protocol = &pms_protocols[i];
			}

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

	return pms_change_state(sc, PMS_STATE_ENABLED, PMS_DEV_PRIMARY);
}

void
pms_disable(void *v)
{
	struct pms_softc *sc = v;

	pms_change_state(sc, PMS_STATE_DISABLED, PMS_DEV_PRIMARY);
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
#ifdef DIAGNOSTIC
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

int
synaptics_set_mode(struct pms_softc *sc, int mode)
{
	struct synaptics_softc *syn = sc->synaptics;

	if (pms_spec_cmd(sc, mode) ||
	    pms_set_rate(sc, SYNAPTICS_CMD_SET_MODE))
		return (-1);

	syn->mode = mode;

	return (0);
}

int
synaptics_query(struct pms_softc *sc, int query, int *val)
{
	u_char resp[3];

	if (pms_spec_cmd(sc, query) ||
	    pms_get_status(sc, resp))
		return (-1);

	if (val)
		*val = (resp[0] << 16) | (resp[1] << 8) | resp[2];

	return (0);
}

int
synaptics_get_hwinfo(struct pms_softc *sc)
{
	struct synaptics_softc *syn = sc->synaptics;

	if (synaptics_query(sc, SYNAPTICS_QUE_IDENTIFY, &syn->identify))
		return (-1);
	if (synaptics_query(sc, SYNAPTICS_QUE_CAPABILITIES,
	    &syn->capabilities))
		return (-1);
	if (synaptics_query(sc, SYNAPTICS_QUE_MODEL, &syn->model))
		return (-1);
	if ((SYNAPTICS_CAP_EXTENDED_QUERIES(syn->capabilities) >= 1) &&
	    synaptics_query(sc, SYNAPTICS_QUE_EXT_MODEL, &syn->ext_model))
		return (-1);
	if ((SYNAPTICS_CAP_EXTENDED_QUERIES(syn->capabilities) >= 4) &&
	    synaptics_query(sc, SYNAPTICS_QUE_EXT_CAPABILITIES,
		&syn->ext_capabilities))
		return (-1);
	if ((SYNAPTICS_ID_MAJOR(syn->identify) >= 4) &&
	    synaptics_query(sc, SYNAPTICS_QUE_RESOLUTION, &syn->resolution))
		return (-1);
	if ((SYNAPTICS_CAP_EXTENDED_QUERIES(syn->capabilities) >= 5) &&
	    (syn->ext_capabilities & SYNAPTICS_EXT_CAP_MAX_DIMENSIONS) &&
	    synaptics_query(sc, SYNAPTICS_QUE_EXT_DIMENSIONS, &syn->dimension))
		return (-1);

	syn->res_x = SYNAPTICS_RESOLUTION_X(syn->resolution);
	syn->res_y = SYNAPTICS_RESOLUTION_Y(syn->resolution);
	syn->min_x = SYNAPTICS_XMIN_BEZEL;
	syn->min_y = SYNAPTICS_YMIN_BEZEL;
	syn->max_x = (syn->dimension) ?
	    SYNAPTICS_DIM_X(syn->dimension) : SYNAPTICS_XMAX_BEZEL;
	syn->max_y = (syn->dimension) ?
	    SYNAPTICS_DIM_Y(syn->dimension) : SYNAPTICS_YMAX_BEZEL;

	if (SYNAPTICS_EXT_MODEL_BUTTONS(syn->ext_model) > 8)
		syn->ext_model &= ~0xf000;

	return (0);
}

void
synaptics_pt_proc(struct pms_softc *sc)
{
	u_int buttons;
	int dx, dy;

	if ((sc->sc_dev_enable & PMS_DEV_SECONDARY) == 0)
		return;

	buttons = butmap[sc->packet[1] & PMS_PS2_BUTTONSMASK];
	dx = (sc->packet[1] & PMS_PS2_XNEG) ?
	    (int)sc->packet[4] - 256 : sc->packet[4];
	dy = (sc->packet[1] & PMS_PS2_YNEG) ?
	    (int)sc->packet[5] - 256 : sc->packet[5];

	wsmouse_input(sc->sc_pt_wsmousedev,
	    buttons, dx, dy, 0, 0, WSMOUSE_INPUT_DELTA);
}

int
synaptics_pt_enable(void *v)
{
	struct pms_softc *sc = v;

	return (pms_change_state(sc, PMS_STATE_ENABLED, PMS_DEV_SECONDARY));
}

void
synaptics_pt_disable(void *v)
{
	struct pms_softc *sc = v;

	pms_change_state(sc, PMS_STATE_DISABLED, PMS_DEV_SECONDARY);
}

int
synaptics_pt_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_PS2;
		break;
	default:
		return (-1);
	}
	return (0);
}

int
pms_enable_synaptics(struct pms_softc *sc)
{
	struct synaptics_softc *syn = sc->synaptics;
	struct wsmousedev_attach_args a;
	u_char resp[3];
	int mode;

	if (pms_set_resolution(sc, 0) ||
	    pms_set_resolution(sc, 0) ||
	    pms_set_resolution(sc, 0) ||
	    pms_set_resolution(sc, 0) ||
	    pms_get_status(sc, resp) ||
	    resp[1] != SYNAPTICS_ID_MAGIC)
		return (0);

	if (sc->synaptics == NULL) {
		sc->synaptics = syn = malloc(sizeof(struct synaptics_softc),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (syn == NULL) {
			printf("%s: synaptics: not enough memory\n",
			    DEVNAME(sc));
			return (0);
		}

		if (synaptics_get_hwinfo(sc))
			return (0);

		if ((syn->model & SYNAPTICS_MODEL_NEWABS) == 0) {
			printf("%s: don't support Synaptics OLDABS\n",
			    DEVNAME(sc));
			return (0);
		}

		/* enable pass-through PS/2 port if supported */
		if (syn->capabilities & SYNAPTICS_CAP_PASSTHROUGH) {
			a.accessops = &synaptics_pt_accessops;
			a.accesscookie = sc;
			sc->sc_pt_wsmousedev = config_found((void *)sc, &a,
			    wsmousedevprint);
		}

		syn->wsmode = WSMOUSE_COMPAT;

		printf("%s: Synaptics %s, firmware %d.%d\n", DEVNAME(sc),
		    (syn->ext_capabilities & SYNAPTICS_EXT_CAP_CLICKPAD ?
			"clickpad" : "touchpad"),
		    SYNAPTICS_ID_MAJOR(syn->identify),
		    SYNAPTICS_ID_MINOR(syn->identify));
	}

	mode = SYNAPTICS_ABSOLUTE_MODE | SYNAPTICS_HIGH_RATE;
	if (SYNAPTICS_ID_MAJOR(syn->identify) >= 4)
		mode |= SYNAPTICS_DISABLE_GESTURE;
	if (syn->capabilities & SYNAPTICS_CAP_EXTENDED)
		mode |= SYNAPTICS_W_MODE;
	if (synaptics_set_mode(sc, mode))
		return (0);

	/* enable advanced gesture mode if supported */
	if ((syn->ext_capabilities & SYNAPTICS_EXT_CAP_ADV_GESTURE) &&
	    (pms_spec_cmd(sc, SYNAPTICS_QUE_MODEL) ||
	     pms_set_rate(sc, SYNAPTICS_CMD_SET_ADV_GESTURE_MODE)))
		return (0);

	return (1);
}

int
pms_ioctl_synaptics(struct pms_softc *sc, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct synaptics_softc *syn = sc->synaptics;
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;
	int wsmode;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_SYNAPTICS;
		break;
	case WSMOUSEIO_GCALIBCOORDS:
		wsmc->minx = syn->min_x;
		wsmc->maxx = syn->max_x;
		wsmc->miny = syn->min_y;
		wsmc->maxy = syn->max_y;
		wsmc->swapxy = 0;
		wsmc->resx = syn->res_x;
		wsmc->resy = syn->res_y;
		break;
	case WSMOUSEIO_SETMODE:
		wsmode = *(u_int *)data;
		if (wsmode != WSMOUSE_COMPAT && wsmode != WSMOUSE_NATIVE)
			return (EINVAL);
		syn->wsmode = wsmode;
		break;
	default:
		return (-1);
	}
	return (0);
}

int
pms_sync_synaptics(struct pms_softc *sc, int data)
{
	switch (sc->inputstate) {
	case 0:
		if ((data & 0xc8) != 0x80)
			return (-1);
		break;
	case 3:
		if ((data & 0xc8) != 0xc0)
			return (-1);
		break;
	}

	return (0);
}

void
pms_proc_synaptics(struct pms_softc *sc)
{
	struct synaptics_softc *syn = sc->synaptics;
	u_int buttons;
	int x, y, z, w, dx, dy;

	w = ((sc->packet[0] & 0x30) >> 2) | ((sc->packet[0] & 0x04) >> 1) |
	    ((sc->packet[3] & 0x04) >> 2);

	if ((syn->capabilities & SYNAPTICS_CAP_PASSTHROUGH) && w == 3) {
		synaptics_pt_proc(sc);
		return;
	}

	if ((sc->sc_dev_enable & PMS_DEV_PRIMARY) == 0)
		return;

	/* XXX ignore advanced gesture packet, not yet supported */
	if ((syn->ext_capabilities & SYNAPTICS_EXT_CAP_ADV_GESTURE) && w == 2)
		return;

	x = ((sc->packet[3] & 0x10) << 8) | ((sc->packet[1] & 0x0f) << 8) |
	    sc->packet[4];
	y = ((sc->packet[3] & 0x20) << 7) | ((sc->packet[1] & 0xf0) << 4) |
	    sc->packet[5];
	z = sc->packet[2];

	buttons = ((sc->packet[0] & sc->packet[3]) & 0x01) ?
	    WSMOUSE_BUTTON(1) : 0;
	buttons |= ((sc->packet[0] & sc->packet[3]) & 0x02) ?
	    WSMOUSE_BUTTON(3) : 0;

	if (syn->ext_capabilities & SYNAPTICS_EXT_CAP_CLICKPAD) {
		buttons |= ((sc->packet[0] ^ sc->packet[3]) & 0x01) ?
		    WSMOUSE_BUTTON(1) : 0;
	} else if (syn->capabilities & SYNAPTICS_CAP_MIDDLE_BUTTON) {
		buttons |= ((sc->packet[0] ^ sc->packet[3]) & 0x01) ?
		    WSMOUSE_BUTTON(2) : 0;
	}

	if (syn->capabilities & SYNAPTICS_CAP_FOUR_BUTTON) {
		buttons |= ((sc->packet[0] ^ sc->packet[3]) & 0x01) ?
		    WSMOUSE_BUTTON(4) : 0;
		buttons |= ((sc->packet[0] ^ sc->packet[3]) & 0x02) ?
		    WSMOUSE_BUTTON(5) : 0;
	} else if (SYNAPTICS_EXT_MODEL_BUTTONS(syn->ext_model) &&
	    ((sc->packet[0] ^ sc->packet[3]) & 0x02)) {
		buttons |= (sc->packet[4] & 0x01) ? WSMOUSE_BUTTON(6) : 0;
		buttons |= (sc->packet[5] & 0x01) ? WSMOUSE_BUTTON(7) : 0;
		buttons |= (sc->packet[4] & 0x02) ? WSMOUSE_BUTTON(8) : 0;
		buttons |= (sc->packet[5] & 0x02) ? WSMOUSE_BUTTON(9) : 0;
		buttons |= (sc->packet[4] & 0x04) ? WSMOUSE_BUTTON(10) : 0;
		buttons |= (sc->packet[5] & 0x04) ? WSMOUSE_BUTTON(11) : 0;
		buttons |= (sc->packet[4] & 0x08) ? WSMOUSE_BUTTON(12) : 0;
		buttons |= (sc->packet[5] & 0x08) ? WSMOUSE_BUTTON(13) : 0;
		x &= ~0x0f;
		y &= ~0x0f;
	}

	/* ignore final events that happen when removing all fingers */
	if (x <= 1 || y <= 1) {
		x = syn->old_x;
		y = syn->old_y;
	}

	if (syn->wsmode == WSMOUSE_NATIVE) {
		wsmouse_input(sc->sc_wsmousedev, buttons, x, y, z, w,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z | WSMOUSE_INPUT_ABSOLUTE_W);
	} else {
		dx = dy = 0;
		if (z > SYNAPTICS_PRESSURE) {
			dx = x - syn->old_x;
			dy = y - syn->old_y;
			dx /= SYNAPTICS_SCALE;
			dy /= SYNAPTICS_SCALE;
		}
		if (dx || dy || buttons != syn->old_buttons)
			wsmouse_input(sc->sc_wsmousedev, buttons, dx, dy, 0, 0,
			    WSMOUSE_INPUT_DELTA);
		syn->old_buttons = buttons;
	}

	syn->old_x = x;
	syn->old_y = y;
}

void
pms_disable_synaptics(struct pms_softc *sc)
{
	struct synaptics_softc *syn = sc->synaptics;

	if (syn->capabilities & SYNAPTICS_CAP_SLEEP)
		synaptics_set_mode(sc, SYNAPTICS_SLEEP_MODE |
		    SYNAPTICS_DISABLE_GESTURE);
}

int
alps_get_hwinfo(struct pms_softc *sc)
{
	struct alps_softc *alps = sc->alps;
	u_char resp[3];
	int i;

	if (pms_set_resolution(sc, 0) ||
	    pms_set_scaling(sc, 2) ||
	    pms_set_scaling(sc, 2) ||
	    pms_set_scaling(sc, 2) ||
	    pms_get_status(sc, resp)) {
		DPRINTF("%s: alps: model query error\n", DEVNAME(sc));
		return (-1);
	}

	alps->version = (resp[0] << 8) | (resp[1] << 4) | (resp[2] / 20 + 1);

	for (i = 0; i < nitems(alps_models); i++)
		if (alps->version == alps_models[i].version) {
			alps->model = alps_models[i].model;
			alps->mask = alps_models[i].mask;
			return (0);
		}

	return (-1);

}

int
pms_enable_alps(struct pms_softc *sc)
{
	struct alps_softc *alps = sc->alps;
	u_char resp[3];

	if (pms_set_resolution(sc, 0) ||
	    pms_set_scaling(sc, 1) ||
	    pms_set_scaling(sc, 1) ||
	    pms_set_scaling(sc, 1) ||
	    pms_get_status(sc, resp) ||
	    resp[0] != PMS_ALPS_MAGIC1 ||
	    resp[1] != PMS_ALPS_MAGIC2 ||
	    (resp[2] != PMS_ALPS_MAGIC3_1 && resp[2] != PMS_ALPS_MAGIC3_2))
		return (0);

	if (sc->alps == NULL) {
		sc->alps = alps = malloc(sizeof(struct alps_softc),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		if (alps == NULL) {
			printf("%s: alps: not enough memory\n", DEVNAME(sc));
			goto err;
		}

		if (alps_get_hwinfo(sc))
			goto err;

		printf("%s: ALPS %s, version 0x%04x\n", DEVNAME(sc),
		    (alps->model & ALPS_DUALPOINT ? "Dualpoint" : "Glidepoint"),
		    alps->version);

		alps->min_x = ALPS_XMIN_BEZEL;
		alps->min_y = ALPS_YMIN_BEZEL;
		alps->max_x = ALPS_XMAX_BEZEL;
		alps->max_y = ALPS_YMAX_BEZEL;

		alps->wsmode = WSMOUSE_COMPAT;
	}

	if (alps->model == 0)
		goto err;

	if ((alps->model & ALPS_PASSTHROUGH) &&
	   (pms_set_scaling(sc, 2) ||
	    pms_set_scaling(sc, 2) ||
	    pms_set_scaling(sc, 2) ||
	    pms_dev_disable(sc))) {
		DPRINTF("%s: alps: passthrough on error\n", DEVNAME(sc));
		goto err;
	}

	if (pms_dev_disable(sc) ||
	    pms_dev_disable(sc) ||
	    pms_set_rate(sc, 0x0a)) {
		DPRINTF("%s: alps: tapping error\n", DEVNAME(sc));
		goto err;
	}

	if (pms_dev_disable(sc) ||
	    pms_dev_disable(sc) ||
	    pms_dev_disable(sc) ||
	    pms_dev_disable(sc) ||
	    pms_dev_enable(sc)) {
		DPRINTF("%s: alps: absolute mode error\n", DEVNAME(sc));
		goto err;
	}

	if ((alps->model & ALPS_PASSTHROUGH) &&
	   (pms_set_scaling(sc, 1) ||
	    pms_set_scaling(sc, 1) ||
	    pms_set_scaling(sc, 1) ||
	    pms_dev_disable(sc))) {
		DPRINTF("%s: alps: passthrough off error\n", DEVNAME(sc));
		goto err;
	}

	return (1);

err:
	pms_reset(sc);

	return (0);
}

int
pms_ioctl_alps(struct pms_softc *sc, u_long cmd, caddr_t data, int flag,
    struct proc *p)
{
	struct alps_softc *alps = sc->alps;
	struct wsmouse_calibcoords *wsmc = (struct wsmouse_calibcoords *)data;
	int wsmode;

	switch (cmd) {
	case WSMOUSEIO_GTYPE:
		*(u_int *)data = WSMOUSE_TYPE_ALPS;
		break;
	case WSMOUSEIO_GCALIBCOORDS:
		wsmc->minx = alps->min_x;
		wsmc->maxx = alps->max_x;
		wsmc->miny = alps->min_y;
		wsmc->maxy = alps->max_y;
		wsmc->swapxy = 0;
		break;
	case WSMOUSEIO_SETMODE:
		wsmode = *(u_int *)data;
		if (wsmode != WSMOUSE_COMPAT && wsmode != WSMOUSE_NATIVE)
			return (EINVAL);
		alps->wsmode = wsmode;
		break;
	default:
		return (-1);
	}
	return (0);
}

int
pms_sync_alps(struct pms_softc *sc, int data)
{
	struct alps_softc *alps = sc->alps;

	switch (sc->inputstate) {
	case 0:
		if ((data & alps->mask) != alps->mask)
			return (-1);
		break;
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		if ((data & 0x80) != 0)
			return (-1);
		break;
	}

	return (0);
}

void
pms_proc_alps(struct pms_softc *sc)
{
	struct alps_softc *alps = sc->alps;
	int x, y, z, dx, dy;
	u_int buttons;
	int fin, ges;

	x = sc->packet[1] | ((sc->packet[2] & 0x78) << 4);
	y = sc->packet[4] | ((sc->packet[3] & 0x70) << 3);
	z = sc->packet[5];

	/*
	 * XXX The Y-axis is in the oposit direction compared to
	 * Synaptics touchpads and PS/2 mouses.
	 * It's why we need to translate the y value here for both
	 * NATIVE and COMPAT modes.
	 */
	y = ALPS_YMAX_BEZEL - y + ALPS_YMIN_BEZEL;

	buttons = ((sc->packet[3] & 1) ? WSMOUSE_BUTTON(1) : 0) |
	    ((sc->packet[3] & 2) ? WSMOUSE_BUTTON(3) : 0) |
	    ((sc->packet[3] & 4) ? WSMOUSE_BUTTON(2) : 0);

	if (alps->wsmode == WSMOUSE_NATIVE) {
		if (z == 127) {
			/* DualPoint touchpads are not absolute. */
			wsmouse_input(sc->sc_wsmousedev, buttons, x, y, 0, 0,
			    WSMOUSE_INPUT_DELTA);
			return;
		}

		ges = sc->packet[2] & 0x01;
		fin = sc->packet[2] & 0x02;

		/* Simulate click (tap) */
		if (ges && !fin)
			z = 35;

		/* Generate a null pressure event (needed for tap & drag) */
		if (ges && fin && !alps->old_fin)
			z = 0;

		wsmouse_input(sc->sc_wsmousedev, buttons, x, y, z, 0,
		    WSMOUSE_INPUT_ABSOLUTE_X | WSMOUSE_INPUT_ABSOLUTE_Y |
		    WSMOUSE_INPUT_ABSOLUTE_Z);

		alps->old_fin = fin;
	} else {
		dx = dy = 0;
		if (z > ALPS_PRESSURE) {
			dx = x - alps->old_x;
			dy = y - alps->old_y;

			/* Prevent jump */
			dx = abs(dx) > 50 ? 0 : dx;
			dy = abs(dy) > 50 ? 0 : dy;
		}

		if (dx || dy || buttons != alps->old_buttons)
			wsmouse_input(sc->sc_wsmousedev, buttons, dx, dy, 0, 0,
			    WSMOUSE_INPUT_DELTA);

		alps->old_x = x;
		alps->old_y = y;
		alps->old_buttons = buttons;
	}
}
