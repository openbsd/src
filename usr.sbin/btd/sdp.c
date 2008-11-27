/*	$OpenBSD: sdp.c,v 1.5 2008/11/27 00:51:17 uwe Exp $	*/

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <netbt/l2cap.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btd.h"
#include "sdp.h"

struct sdp_state {
	TAILQ_HEAD(, sdp_session) sessions;
};

struct sdp_session *sdp_new_session(struct sdp_state *, const bdaddr_t *);
struct sdp_session *sdp_find_session(struct sdp_state *, const bdaddr_t *);
int sdp_open_session(struct sdp_session *, const bdaddr_t *);
void sdp_close_session(struct sdp_session *);
void sdp_delete_session(struct sdp_session *);
void sdp_eventcb(int, short, void *);

void
sdp_init(struct btd *env)
{
	struct sdp_state *sdp;

	sdp = env->sdp = calloc(1, sizeof(*env->sdp));

	TAILQ_INIT(&sdp->sessions);
}

/* return 0 on success, 2 if the SDP query is still in progress, 1 if
 * the device was not found, -1 on error */
int
sdp_get_devinfo(struct bt_interface *iface, struct bt_device *btdev)
{
	struct sdp_state *sdp = iface->env->sdp;
	struct sdp_session *sess;
	const char *service;

	switch (btdev->type) {
	case BTDEV_NONE:
		return 1;
	case BTDEV_HID:
		service = "HID";
		break;
	case BTDEV_HSET:
		service = "HSET";
		break;
	case BTDEV_HF:
		service = "HF";
		break;
	default:
		log_warnx("sdp_get_devinfo: invalid device type %#x",
		    btdev->type);
		return -1;
	}

	if ((sess = sdp_find_session(sdp, &btdev->addr)) == NULL &&
	    (sess = sdp_new_session(sdp, &btdev->addr)) == NULL)
		return -1;

	if (sdp_open_session(sess, &iface->addr) < 0)
		return -1;

	if (sdp_query(sess, &btdev->info.baa, &iface->addr, &btdev->addr,
	    service) < 0) {
		sdp_close_session(sess);
		return -1;
	}

	sdp_close_session(sess);
	return 0;
}

struct sdp_session *
sdp_new_session(struct sdp_state *sdp, const bdaddr_t *raddr)
{
	struct sdp_session *sess;

	if ((sess = calloc(1, sizeof(*sess))) == NULL) {
		log_warn("sdp_new_session");
		return NULL;
	}

	sess->sdp = sdp;
	sess->state = SDP_SESSION_CLOSED;
	bdaddr_copy(&sess->raddr, raddr);

	TAILQ_INSERT_TAIL(&sdp->sessions, sess, entry);

	return sess;
}

struct sdp_session *
sdp_find_session(struct sdp_state *sdp, const bdaddr_t *raddr)
{
	struct sdp_session *sess;

	TAILQ_FOREACH(sess, &sdp->sessions, entry)
		if (bdaddr_same(&sess->raddr, raddr))
			return sess;

	return NULL;
}

int
sdp_open_session(struct sdp_session *sess, const bdaddr_t *laddr)
{
	struct sockaddr_bt sa;
	socklen_t size;

	if (sess->state != SDP_SESSION_CLOSED) {
		if (bdaddr_same(&sess->laddr, laddr))
			return 0;

		sdp_close_session(sess);
	}

	bdaddr_copy(&sess->laddr, laddr);

	sess->fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (sess->fd == -1) {
		log_warn("sdp_open_session: socket");
		goto fail;
	}

	memset(&sa, 0, sizeof(sa));
	sa.bt_len = sizeof(sa);
	sa.bt_family = AF_BLUETOOTH;
	bdaddr_copy(&sa.bt_bdaddr, &sess->laddr);

	if (bind(sess->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		log_warn("sdp_open_session: bind");
		goto fail;
	}

	sa.bt_psm = L2CAP_PSM_SDP;
	bdaddr_copy(&sa.bt_bdaddr, &sess->raddr);

	if (connect(sess->fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		log_warn("sdp_open_session: connect");
		goto fail;
	}

	size = sizeof(sess->omtu);
	if (getsockopt(sess->fd, BTPROTO_L2CAP, SO_L2CAP_OMTU, &sess->omtu,
	    &size) < 0) {
		log_warn("sdp_open_session: getsockopt");
		goto fail;
	}

	size = sizeof(sess->imtu);
	if (getsockopt(sess->fd, BTPROTO_L2CAP, SO_L2CAP_IMTU, &sess->imtu,
	    &size) < 0) {
		log_warn("sdp_open_session: getsockopt");
		goto fail;
	}

	if ((sess->req = malloc((size_t)sess->omtu)) == NULL) {
		log_warn("sdp_open_session: malloc req");
		goto fail;
	}
	sess->req_e = sess->req + sess->omtu;

	if ((sess->rsp = malloc((size_t)sess->imtu)) == NULL) {
		log_warn("sdp_open_session: malloc rsp");
		goto fail;
	}
	sess->rsp_e = sess->rsp + sess->imtu;

	event_set(&sess->ev, sess->fd, EV_READ | EV_PERSIST,
	    sdp_eventcb, sess);
	if (event_add(&sess->ev, NULL)) {
		log_warnx("event_add failed");
		goto fail;
	}

	sess->state = SDP_SESSION_OPEN;
	return 0;

fail:
	if (sess->fd != -1) {
		close(sess->fd);
		sess->fd = -1;
	}

	if (sess->req != NULL) {
		free(sess->req);
		sess->req = NULL;
	}

	if (sess->rsp != NULL) {
		free(sess->rsp);
		sess->rsp = NULL;
	}

	return -1;
}

void
sdp_close_session(struct sdp_session *sess)
{
	if (sess->state == SDP_SESSION_CLOSED)
		return;

	if (sess->req != NULL) {
		free(sess->req);
		sess->req = NULL;
	}

	if (sess->rsp != NULL) {
		free(sess->rsp);
		sess->rsp = NULL;
	}

	close(sess->fd);
	sess->fd = -1;
	sess->state = SDP_SESSION_CLOSED;
}

void
sdp_delete_session(struct sdp_session *sess)
{
	struct sdp_state *sdp = sess->sdp;

	sdp_close_session(sess);

	TAILQ_REMOVE(&sdp->sessions, sess, entry);

	free(sess);
}

void
sdp_eventcb(int fd, short evflags, void *arg)
{
}
