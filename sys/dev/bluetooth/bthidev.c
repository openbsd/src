/*	$OpenBSD: bthidev.c,v 1.1 2007/09/01 17:06:26 xsa Exp $	*/
/*	$NetBSD: bthidev.c,v 1.1 2006/06/19 15:44:45 gdamore Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/l2cap.h>

#include <dev/usb/hid.h>
#include <dev/bluetooth/btdev.h>
#include <dev/bluetooth/bthid.h>
#include <dev/bluetooth/bthidev.h>

#define MAX_DESCRIPTOR_LEN	1024		/* sanity check */

/* bthidev softc */
struct bthidev_softc {
	struct btdev		sc_btdev;	/* device+ */
	uint16_t		sc_state;
	uint16_t		sc_flags;

	bdaddr_t		sc_laddr;	/* local address */
	bdaddr_t		sc_raddr;	/* remote address */

	void			*sc_desc;	/* HID descriptor */
	int			sc_dlen;	/* descriptor length */

	uint16_t		sc_ctlpsm;	/* control PSM */
	struct l2cap_channel	*sc_ctl;	/* control channel */
	struct l2cap_channel	*sc_ctl_l;	/* control listen */

	uint16_t		sc_intpsm;	/* interrupt PSM */
	struct l2cap_channel	*sc_int;	/* interrupt channel */
	struct l2cap_channel	*sc_int_l;	/* interrupt listen */

	LIST_HEAD(,bthidev)	sc_list;	/* child list */

	struct timeout		sc_reconnect;
	int			sc_attempts;	/* connection attempts */
};

/* device state */
#define BTHID_CLOSED		0
#define BTHID_WAIT_CTL		1
#define BTHID_WAIT_INT		2
#define BTHID_OPEN		3
#define BTHID_DETACHING		4

#define	BTHID_RETRY_INTERVAL	5	/* seconds between connection attempts */

void	bthidev_timeout(void *);
int	bthidev_listen(struct bthidev_softc *);
int	bthidev_connect(struct bthidev_softc *);
int	bthidev_output(struct bthidev *, uint8_t *, int);
void	bthidev_null(struct bthidev *, uint8_t *, int);

int	bthidev_match(struct device *, void *, void *);
void	bthidev_attach(struct device *, struct device *, void *);
int	bthidev_detach(struct device *, int);
int	bthidev_print(void *, const char *);

int	bthidevsubmatch(struct device *parent, void *, void *);

struct cfdriver bthidev_cd = {
	NULL, "bthidev", DV_DULL
};

const struct cfattach bthidev_ca = {
	sizeof(struct bthidev_softc),
	bthidev_match,
	bthidev_attach,
	bthidev_detach,
};

/* bluetooth(9) protocol methods for L2CAP */
void	 bthidev_connecting(void *);
void	 bthidev_ctl_connected(void *);
void	 bthidev_int_connected(void *);
void	 bthidev_ctl_disconnected(void *, int);
void	 bthidev_int_disconnected(void *, int);
void	*bthidev_ctl_newconn(void *, struct sockaddr_bt *,
    struct sockaddr_bt *);
void	*bthidev_int_newconn(void *, struct sockaddr_bt *,
    struct sockaddr_bt *);
void	 bthidev_complete(void *, int);
void	 bthidev_input(void *, struct mbuf *);

const struct btproto bthidev_ctl_proto = {
	bthidev_connecting,
	bthidev_ctl_connected,
	bthidev_ctl_disconnected,
	bthidev_ctl_newconn,
	bthidev_complete,
	NULL,
	bthidev_input,
};

const struct btproto bthidev_int_proto = {
	bthidev_connecting,
	bthidev_int_connected,
	bthidev_int_disconnected,
	bthidev_int_newconn,
	bthidev_complete,
	NULL,
	bthidev_input,
};


int
bthidev_match(struct device *self, void *match, void *aux)
{
	struct btdev_attach_args *bda = (struct btdev_attach_args *)aux;

	if (bda->bd_type != BTDEV_HID
	    || L2CAP_PSM_INVALID(bda->bd_hid.hid_ctl)
	    || L2CAP_PSM_INVALID(bda->bd_hid.hid_int)
	    || bda->bd_hid.hid_desc == NULL
	    || bda->bd_hid.hid_dlen == 0
	    || bda->bd_hid.hid_dlen > MAX_DESCRIPTOR_LEN)
		return 0;

	return 1;
}

void
bthidev_attach(struct device *parent, struct device *self, void *aux)
{
	struct bthidev_softc *sc = (struct bthidev_softc *)self;
	struct btdev_attach_args *bda = (struct btdev_attach_args *)aux;
	struct bthidev_attach_args bha;
	struct bthidev *dev;
	struct hid_data *d;
	struct hid_item h;
	int maxid, rep, s;

	/*
	 * Init softc
	 */
	LIST_INIT(&sc->sc_list);
	timeout_set(&sc->sc_reconnect, bthidev_timeout, sc);
	sc->sc_state = BTHID_CLOSED;

	/*
	 * copy in our configuration info
	 */
	bdaddr_copy(&sc->sc_laddr, &bda->bd_laddr);
	bdaddr_copy(&sc->sc_raddr, &bda->bd_raddr);

	sc->sc_ctlpsm = bda->bd_hid.hid_ctl;
	sc->sc_intpsm = bda->bd_hid.hid_int;

	sc->sc_flags = bda->bd_hid.hid_flags;
	if (sc->sc_flags & BTHID_INITIATE)
		sc->sc_flags |= BTHID_CONNECT;

	sc->sc_dlen = bda->bd_hid.hid_dlen;
	sc->sc_desc = malloc(bda->bd_hid.hid_dlen, M_BTHIDEV, M_WAITOK);
	if (sc->sc_desc == NULL) {
		printf(" no memory\n");
		return;
	}
	copyin(bda->bd_hid.hid_desc, sc->sc_desc, bda->bd_hid.hid_dlen);

	/*
	 * Parse the descriptor and attach child devices, one per report.
	 */
	maxid = -1;
	h.report_ID = 0;
	d = hid_start_parse(sc->sc_desc, sc->sc_dlen, hid_none);
	while (hid_get_item(d, &h)) {
		if (h.report_ID > maxid)
			maxid = h.report_ID;
	}
	hid_end_parse(d);

	if (maxid < 0) {
		printf(" no reports found\n");
		return;
	}

	printf("\n");

	for (rep = 0 ; rep <= maxid ; rep++) {
		if (hid_report_size(sc->sc_desc, sc->sc_dlen, hid_feature, rep) == 0
		    && hid_report_size(sc->sc_desc, sc->sc_dlen, hid_input, rep) == 0
		    && hid_report_size(sc->sc_desc, sc->sc_dlen, hid_output, rep) == 0)
			continue;

		bha.ba_desc = sc->sc_desc;
		bha.ba_dlen = sc->sc_dlen;
		bha.ba_input = bthidev_null;
		bha.ba_feature = bthidev_null;
		bha.ba_output = bthidev_output;
		bha.ba_id = rep;

		dev = (struct bthidev *)config_found_sm(self, &bha,
		    bthidev_print, bthidevsubmatch);
		if (dev != NULL) {
			dev->sc_parent = &sc->sc_btdev;
			dev->sc_id = rep;
			dev->sc_input = bha.ba_input;
			dev->sc_feature = bha.ba_feature;
			LIST_INSERT_HEAD(&sc->sc_list, dev, sc_next);
		}
	}

	/*
	 * start bluetooth connections
	 */
	s = splsoftnet();
	if ((sc->sc_flags & BTHID_INITIATE) == 0)
		bthidev_listen(sc);

	if (sc->sc_flags & BTHID_CONNECT)
		bthidev_connect(sc);
	splx(s);
}

int
bthidev_detach(struct device *self, int flags)
{
	struct bthidev_softc *sc = (struct bthidev_softc *)self;
	struct bthidev *dev;
	int s;

	s = splsoftnet();
	sc->sc_flags = 0;	/* disable reconnecting */

	/* release interrupt listen */
	if (sc->sc_int_l != NULL) {
		l2cap_detach(&sc->sc_int_l);
		sc->sc_int_l = NULL;
	}

	/* release control listen */
	if (sc->sc_ctl_l != NULL) {
		l2cap_detach(&sc->sc_ctl_l);
		sc->sc_ctl_l = NULL;
	}

	/* close interrupt channel */
	if (sc->sc_int != NULL) {
		l2cap_disconnect(sc->sc_int, 0);
		l2cap_detach(&sc->sc_int);
		sc->sc_int = NULL;
	}

	/* close control channel */
	if (sc->sc_ctl != NULL) {
		l2cap_disconnect(sc->sc_ctl, 0);
		l2cap_detach(&sc->sc_ctl);
		sc->sc_ctl = NULL;
	}

	sc->sc_state = BTHID_DETACHING;
	timeout_del(&sc->sc_reconnect);

	splx(s);

	/* detach children */
	while ((dev = LIST_FIRST(&sc->sc_list)) != NULL) {
		LIST_REMOVE(dev, sc_next);
		config_detach(&dev->sc_dev, flags);
	}

	/* release descriptor */
	if (sc->sc_desc != NULL) {
		free(sc->sc_desc, M_BTHIDEV);
		sc->sc_desc = NULL;
	}
	return 0;
}

int
bthidev_print(void *aux, const char *pnp)
{
	struct bthidev_attach_args *ba = aux;

	if (pnp != NULL)
		printf("%s:", pnp);

	if (ba->ba_id > 0)
		printf(" reportid %d", ba->ba_id);

	return UNCONF;
}

int
bthidevsubmatch(struct device *parent, void *match, void *aux)
{
	struct bthidev_attach_args *ba = aux;
	struct cfdata *cf = match;

	if (cf->bthidevcf_reportid != BTHIDEV_UNK_REPORTID &&
	    cf->bthidevcf_reportid != ba->ba_id)
		return (0);

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}


/*****************************************************************************
 *
 *	bluetooth(4) HID attach/detach routines
 */

/*
 * callouts are scheduled to initiate outgoing connections
 * after the connection has been lost.
 */
void
bthidev_timeout(void *arg)
{
	struct bthidev_softc *sc = arg;
	int s, err;

	s = splsoftnet();

	switch (sc->sc_state) {
	case BTHID_CLOSED:
		sc->sc_flags |= BTHID_CONNECT;
		err = bthidev_connect(sc);
		if (err)
			printf("%s: connect failed (%d)\n",
			    sc->sc_btdev.sc_dev.dv_xname, err);
		break;

	case BTHID_WAIT_CTL:
		break;

	case BTHID_WAIT_INT:
		break;

	case BTHID_OPEN:
		break;

	case BTHID_DETACHING:
		wakeup(sc);
		break;

	default:
		break;
	}
	splx(s);
}

/*
 * listen for our device
 */
int
bthidev_listen(struct bthidev_softc *sc)
{
	struct sockaddr_bt sa;
	int err;

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);

	/*
	 * Listen on control PSM
	 */
	err = l2cap_attach(&sc->sc_ctl_l, &bthidev_ctl_proto, sc);
	if (err)
		return err;

	sa.bt_psm = sc->sc_ctlpsm;
	err = l2cap_bind(sc->sc_ctl_l, &sa);
	if (err)
		return err;

	err = l2cap_listen(sc->sc_ctl_l);
	if (err)
		return err;

	/*
	 * Listen on interrupt PSM
	 */
	err = l2cap_attach(&sc->sc_int_l, &bthidev_int_proto, sc);
	if (err)
		return err;

	sa.bt_psm = sc->sc_intpsm;
	err = l2cap_bind(sc->sc_int_l, &sa);
	if (err)
		return err;

	err = l2cap_listen(sc->sc_int_l);
	if (err)
		return err;

	sc->sc_state = BTHID_WAIT_CTL;
	return 0;
}

/*
 * start connecting to our device
 */
int
bthidev_connect(struct bthidev_softc *sc)
{
	struct sockaddr_bt sa;
	int err;

	if (sc->sc_attempts++ > 0)
		printf("%s: connect (#%d)\n",
		    sc->sc_btdev.sc_dev.dv_xname, sc->sc_attempts);

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;

	err = l2cap_attach(&sc->sc_ctl, &bthidev_ctl_proto, sc);
	if (err) {
		printf("%s: l2cap_attach failed (%d)\n",
		    sc->sc_btdev.sc_dev.dv_xname, err);
		return err;
	}

	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);
	err = l2cap_bind(sc->sc_ctl, &sa);
	if (err) {
		printf("%s: l2cap_bind failed (%d)\n",
		    sc->sc_btdev.sc_dev.dv_xname, err);
		return err;
	}

	sa.bt_psm = sc->sc_ctlpsm;
	bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
	err = l2cap_connect(sc->sc_ctl, &sa);
	if (err) {
		printf("%s: l2cap_connect failed (%d)\n",
		    sc->sc_btdev.sc_dev.dv_xname, err);
		return err;
	}

	sc->sc_state = BTHID_WAIT_CTL;
	return 0;
}

/*****************************************************************************
 *
 *	bluetooth(9) callback methods for L2CAP
 *
 *	All these are called from Bluetooth Protocol code, in a soft
 *	interrupt context at IPL_SOFTNET.
 */

void
bthidev_connecting(void *arg)
{

	/* dont care */
}

void
bthidev_ctl_connected(void *arg)
{
	struct sockaddr_bt sa;
	struct bthidev_softc *sc = arg;
	int err;

	if (sc->sc_state != BTHID_WAIT_CTL)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int == NULL);

	if (sc->sc_flags & BTHID_CONNECT) {
		/* initiate connect on interrupt PSM */
		err = l2cap_attach(&sc->sc_int, &bthidev_int_proto, sc);
		if (err)
			goto fail;

		memset(&sa, 0, sizeof(sa));
		sa.bt_len = sizeof(sa);
		sa.bt_family = AF_BLUETOOTH;
		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_laddr);

		err = l2cap_bind(sc->sc_int, &sa);
		if (err)
			goto fail;

		sa.bt_psm = sc->sc_intpsm;
		bdaddr_copy(&sa.bt_bdaddr, &sc->sc_raddr);
		err = l2cap_connect(sc->sc_int, &sa);
		if (err)
			goto fail;
	}

	sc->sc_state = BTHID_WAIT_INT;
	return;

fail:
	l2cap_detach(&sc->sc_ctl);
	sc->sc_ctl = NULL;
	printf("%s: connect failed (%d)\n",
	    sc->sc_btdev.sc_dev.dv_xname, err);
}

void
bthidev_int_connected(void *arg)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_state != BTHID_WAIT_INT)
		return;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int != NULL);

	sc->sc_attempts = 0;
	sc->sc_flags &= ~BTHID_CONNECT;
	sc->sc_state = BTHID_OPEN;

	printf("%s: connected\n", sc->sc_btdev.sc_dev.dv_xname);
}

/*
 * Disconnected
 *
 * Depending on our state, this could mean several things, but essentially
 * we are lost.
 */
void
bthidev_ctl_disconnected(void *arg, int err)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_ctl != NULL) {
		l2cap_detach(&sc->sc_ctl);
		sc->sc_ctl = NULL;
	}

	sc->sc_state = BTHID_CLOSED;

	if (sc->sc_int == NULL) {
		printf("%s: disconnected\n", sc->sc_btdev.sc_dev.dv_xname);

		if (sc->sc_flags & BTHID_INITIATE)
			timeout_add(&sc->sc_reconnect,
			    BTHID_RETRY_INTERVAL * hz);
		else
			sc->sc_state = BTHID_WAIT_CTL;
	}
}

void
bthidev_int_disconnected(void *arg, int err)
{
	struct bthidev_softc *sc = arg;

	if (sc->sc_int != NULL) {
		l2cap_detach(&sc->sc_int);
		sc->sc_int = NULL;
	}

	sc->sc_state = BTHID_CLOSED;

	if (sc->sc_ctl == NULL) {
		printf("%s: disconnected\n", sc->sc_btdev.sc_dev.dv_xname);

		if (sc->sc_flags & BTHID_INITIATE)
			timeout_add(&sc->sc_reconnect,
			    BTHID_RETRY_INTERVAL * hz);
		else
			sc->sc_state = BTHID_WAIT_CTL;
	}
}

/*
 * New Connections
 *
 * We give a new L2CAP handle back if this matches the BDADDR we are
 * listening for and we are in the right state. bthidev_connected will
 * be called when the connection is open, so nothing else to do here
 */
void *
bthidev_ctl_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct bthidev_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0
	    || (sc->sc_flags & BTHID_INITIATE)
	    || sc->sc_state != BTHID_WAIT_CTL
	    || sc->sc_ctl != NULL
	    || sc->sc_int != NULL)
		return NULL;

	l2cap_attach(&sc->sc_ctl, &bthidev_ctl_proto, sc);
	return sc->sc_ctl;
}

void *
bthidev_int_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct bthidev_softc *sc = arg;

	if (bdaddr_same(&raddr->bt_bdaddr, &sc->sc_raddr) == 0
	    || (sc->sc_flags & BTHID_INITIATE)
	    || sc->sc_state != BTHID_WAIT_INT
	    || sc->sc_ctl == NULL
	    || sc->sc_int != NULL)
		return NULL;

	l2cap_attach(&sc->sc_int, &bthidev_int_proto, sc);
	return sc->sc_int;
}

void
bthidev_complete(void *arg, int count)
{

	/* dont care */
}

/*
 * Receive reports from the protocol stack.
 */
void
bthidev_input(void *arg, struct mbuf *m)
{
	struct bthidev_softc *sc = arg;
	struct bthidev *dev;
	uint8_t *data;
	int len;

	if (sc->sc_state != BTHID_OPEN)
		goto release;

	if (m->m_pkthdr.len > m->m_len)
		printf("%s: truncating HID report\n",
		    sc->sc_btdev.sc_dev.dv_xname);

	len = m->m_len;
	data = mtod(m, uint8_t *);

	if (BTHID_TYPE(data[0]) == BTHID_DATA) {
		/*
		 * data[0] == type / parameter
		 * data[1] == id
		 * data[2..len] == report
		 */
		if (len < 3)
			goto release;

		LIST_FOREACH(dev, &sc->sc_list, sc_next) {
			if (data[1] == dev->sc_id) {
				switch (BTHID_DATA_PARAM(data[0])) {
				case BTHID_DATA_INPUT:
					(*dev->sc_input)(dev, data + 2, len - 2);
					break;

				case BTHID_DATA_FEATURE:
					(*dev->sc_feature)(dev, data + 2, len - 2);
					break;

				default:
					break;
				}

				goto release;
			}
		}
		printf("%s: report id %d, len = %d ignored\n",
		    sc->sc_btdev.sc_dev.dv_xname, data[1], len - 2);

		goto release;
	}

	if (BTHID_TYPE(data[0]) == BTHID_CONTROL) {
		if (len < 1)
			goto release;

		if (BTHID_DATA_PARAM(data[0]) == BTHID_CONTROL_UNPLUG) {
			printf("%s: unplugged\n",
			    sc->sc_btdev.sc_dev.dv_xname);

			/* close interrupt channel */
			if (sc->sc_int != NULL) {
				l2cap_disconnect(sc->sc_int, 0);
				l2cap_detach(&sc->sc_int);
				sc->sc_int = NULL;
			}

			/* close control channel */
			if (sc->sc_ctl != NULL) {
				l2cap_disconnect(sc->sc_ctl, 0);
				l2cap_detach(&sc->sc_ctl);
				sc->sc_ctl = NULL;
			}
		}

		goto release;
	}

release:
	m_freem(m);
}

/*****************************************************************************
 *
 *	IO routines
 */

void
bthidev_null(struct bthidev *dev, uint8_t *report, int len)
{

	/*
	 * empty routine just in case the device
	 * provided no method to handle this report
	 */
}

int
bthidev_output(struct bthidev *dev, uint8_t *report, int rlen)
{
	struct bthidev_softc *sc = (struct bthidev_softc *)dev->sc_parent;
	struct mbuf *m;
	int s, err;

	if (sc == NULL || sc->sc_state != BTHID_OPEN)
		return ENOTCONN;

	KASSERT(sc->sc_ctl != NULL);
	KASSERT(sc->sc_int != NULL);

	if (rlen == 0 || report == NULL)
		return 0;

	if (rlen > MHLEN - 2) {
		printf("%s: output report too long (%d)!\n",
		    sc->sc_btdev.sc_dev.dv_xname, rlen);

		return EMSGSIZE;
	}

	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOMEM;

	/*
	 * data[0] = type / parameter
	 * data[1] = id
	 * data[2..N] = report
	 */
	mtod(m, uint8_t *)[0] = (uint8_t)((BTHID_DATA << 4) | BTHID_DATA_OUTPUT);
	mtod(m, uint8_t *)[1] = dev->sc_id;
	memcpy(mtod(m, uint8_t *) + 2, report, rlen);
	m->m_pkthdr.len = m->m_len = rlen + 2;

	s = splsoftnet();
	err = l2cap_send(sc->sc_int, m);
	splx(s);

	return err;
}
