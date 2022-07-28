/*	$OpenBSD: logmsg.c,v 1.8 2022/07/28 13:11:48 deraadt Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>

#include "bgpd.h"
#include "session.h"
#include "log.h"

char *
log_fmt_peer(const struct peer_config *peer)
{
	const char	*ip;
	char		*pfmt, *p;

	ip = log_addr(&peer->remote_addr);
	if ((peer->remote_addr.aid == AID_INET && peer->remote_masklen != 32) ||
	    (peer->remote_addr.aid == AID_INET6 &&
	    peer->remote_masklen != 128)) {
		if (asprintf(&p, "%s/%u", ip, peer->remote_masklen) == -1)
			fatal(NULL);
	} else {
		if ((p = strdup(ip)) == NULL)
			fatal(NULL);
	}

	if (peer->descr[0]) {
		if (asprintf(&pfmt, "neighbor %s (%s)", p, peer->descr) ==
		    -1)
			fatal(NULL);
	} else {
		if (asprintf(&pfmt, "neighbor %s", p) == -1)
			fatal(NULL);
	}
	free(p);
	return (pfmt);
}

void
log_peer_info(const struct peer_config *peer, const char *emsg, ...)
{
	char	*p, *nfmt;
	va_list	 ap;

	p = log_fmt_peer(peer);
	if (asprintf(&nfmt, "%s: %s", p, emsg) == -1)
		fatal(NULL);
	va_start(ap, emsg);
	vlog(LOG_INFO, nfmt, ap);
	va_end(ap);
	free(p);
	free(nfmt);
}

void
log_peer_warn(const struct peer_config *peer, const char *emsg, ...)
{
	char	*p, *nfmt;
	va_list	 ap;

	p = log_fmt_peer(peer);
	if (emsg == NULL) {
		if (asprintf(&nfmt, "%s: %s", p, strerror(errno)) == -1)
			fatal(NULL);
	} else {
		if (asprintf(&nfmt, "%s: %s: %s", p, emsg, strerror(errno)) ==
		    -1)
			fatal(NULL);
	}
	va_start(ap, emsg);
	vlog(LOG_ERR, nfmt, ap);
	va_end(ap);
	free(p);
	free(nfmt);
}

void
log_peer_warnx(const struct peer_config *peer, const char *emsg, ...)
{
	char	*p, *nfmt;
	va_list	 ap;

	p = log_fmt_peer(peer);
	if (asprintf(&nfmt, "%s: %s", p, emsg) == -1)
		fatal(NULL);
	va_start(ap, emsg);
	vlog(LOG_ERR, nfmt, ap);
	va_end(ap);
	free(p);
	free(nfmt);
}

void
log_statechange(struct peer *peer, enum session_state nstate,
    enum session_events event)
{
	char	*p;

	/* don't clutter the logs with constant Connect -> Active -> Connect */
	if (nstate == STATE_CONNECT && peer->state == STATE_ACTIVE &&
	    peer->prev_state == STATE_CONNECT)
		return;
	if (nstate == STATE_ACTIVE && peer->state == STATE_CONNECT &&
	    peer->prev_state == STATE_ACTIVE)
		return;

	peer->lasterr = 0;
	p = log_fmt_peer(&peer->conf);
	logit(LOG_INFO, "%s: state change %s -> %s, reason: %s",
	    p, statenames[peer->state], statenames[nstate], eventnames[event]);
	free(p);
}

void
log_notification(const struct peer *peer, uint8_t errcode, uint8_t subcode,
    u_char *data, uint16_t datalen, const char *dir)
{
	char		*p;
	const char	*suberrname = NULL;
	int		 uk = 0;

	p = log_fmt_peer(&peer->conf);
	switch (errcode) {
	case ERR_HEADER:
		if (subcode >= sizeof(suberr_header_names) / sizeof(char *) ||
		    suberr_header_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_header_names[subcode];
		break;
	case ERR_OPEN:
		if (subcode >= sizeof(suberr_open_names) / sizeof(char *) ||
		    suberr_open_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_open_names[subcode];
		break;
	case ERR_UPDATE:
		if (subcode >= sizeof(suberr_update_names) / sizeof(char *) ||
		    suberr_update_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_update_names[subcode];
		break;
	case ERR_CEASE:
		if (subcode >= sizeof(suberr_cease_names) / sizeof(char *) ||
		    suberr_cease_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_cease_names[subcode];
		break;
	case ERR_HOLDTIMEREXPIRED:
		if (subcode != 0)
			uk = 1;
		break;
	case ERR_FSM:
		if (subcode >= sizeof(suberr_fsm_names) / sizeof(char *) ||
		    suberr_fsm_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_fsm_names[subcode];
		break;
	case ERR_RREFRESH:
		if (subcode >= sizeof(suberr_rrefresh_names) / sizeof(char *) ||
		    suberr_rrefresh_names[subcode] == NULL)
			uk = 1;
		else
			suberrname = suberr_rrefresh_names[subcode];
		break;
	default:
		logit(LOG_ERR, "%s: %s notification, unknown errcode "
		    "%u, subcode %u", p, dir, errcode, subcode);
		free(p);
		return;
	}

	if (uk)
		logit(LOG_ERR, "%s: %s notification: %s, unknown subcode %u",
		    p, dir, errnames[errcode], subcode);
	else {
		if (suberrname == NULL)
			logit(LOG_ERR, "%s: %s notification: %s", p,
			    dir, errnames[errcode]);
		else
			logit(LOG_ERR, "%s: %s notification: %s, %s",
			    p, dir, errnames[errcode], suberrname);
	}
	free(p);
}

void
log_conn_attempt(const struct peer *peer, struct sockaddr *sa, socklen_t len)
{
	char		*p;
	const char	*b;

	if (peer == NULL) {	/* connection from non-peer, drop */
		b = log_sockaddr(sa, len);
		logit(LOG_INFO, "connection from non-peer %s refused", b);
	} else {
		/* only log if there is a chance that the session may come up */
		if (peer->conf.down && peer->state == STATE_IDLE)
			return;
		p = log_fmt_peer(&peer->conf);
		logit(LOG_INFO, "Connection attempt from %s while session is "
		    "in state %s", p, statenames[peer->state]);
		free(p);
	}
}
