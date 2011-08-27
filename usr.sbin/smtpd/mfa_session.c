/*	$OpenBSD: mfa_session.c,v 1.1 2011/08/27 22:32:41 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

void mfa_session(struct submit_status *, enum session_state);
struct mfa_session *mfa_session_init(struct submit_status *, enum session_state);
struct mfa_session *mfa_session_find(u_int64_t);
struct mfa_session *mfa_session_xfind(u_int64_t);
int mfa_session_proceed(struct mfa_session *);
void mfa_session_pickup(struct mfa_session *);
void mfa_session_fail(struct mfa_session *);
void mfa_session_destroy(struct mfa_session *);
void mfa_session_done(struct mfa_session *);

void mfa_session_imsg(int, short, void *);

void
mfa_session(struct submit_status *ss, enum session_state state)
{
	struct mfa_session *ms;

	ms = mfa_session_init(ss, state);
	if (ms->filter == NULL) {
		mfa_session_done(ms);
		return;
	}
	if (! mfa_session_proceed(ms))
		mfa_session_fail(ms);
}

struct mfa_session *
mfa_session_init(struct submit_status *ss, enum session_state state)
{
	struct mfa_session *ms;

	ms = calloc(1, sizeof(*ms));
	if (ms == NULL)
		fatal("mfa_session_init: calloc");

	ms->id = generate_uid();
	ms->ss = *ss;
	ms->ss.code = 250;
	ms->state = state;
	ms->filter = TAILQ_FIRST(env->sc_filters);

	SPLAY_INSERT(mfatree, &env->mfa_sessions, ms);

	return ms;
}


int
mfa_session_proceed(struct mfa_session *ms)
{
	struct filter_msg      	 fm;

	fm.id = ms->id;
	fm.cl_id = ms->ss.id;
	fm.version = FILTER_API_VERSION;

	switch (ms->state) {
	case S_HELO:
		fm.type = FILTER_HELO;
		if (strlcpy(fm.u.helo.buffer, ms->ss.envelope.delivery.helo,
			sizeof(fm.u.helo.buffer)) >= sizeof(fm.u.helo.buffer))
			fatalx("mfa_session_proceed: HELO: truncation");
		break;

	case S_MAIL_MFA:
		fm.type = FILTER_MAIL;
		if (strlcpy(fm.u.mail.user, ms->ss.u.maddr.user,
			sizeof(ms->ss.u.maddr.user)) >= sizeof(ms->ss.u.maddr.user))
			fatalx("mfa_session_proceed: MAIL: user truncation");
		if (strlcpy(fm.u.mail.domain, ms->ss.u.maddr.domain,
			sizeof(ms->ss.u.maddr.domain)) >= sizeof(ms->ss.u.maddr.domain))
			fatalx("mfa_session_proceed: MAIL: domain truncation");
		break;

	case S_RCPT_MFA:
		fm.type = FILTER_RCPT;
		if (strlcpy(fm.u.mail.user, ms->ss.u.maddr.user,
			sizeof(ms->ss.u.maddr.user)) >= sizeof(ms->ss.u.maddr.user))
			fatalx("mfa_session_proceed: RCPT: user truncation");
		if (strlcpy(fm.u.mail.domain, ms->ss.u.maddr.domain,
			sizeof(ms->ss.u.maddr.domain)) >= sizeof(ms->ss.u.maddr.domain))
			fatalx("mfa_session_proceed: RCPT: domain truncation");
		break;

	default:
		fatalx("mfa_session_proceed: no such state");
	}

	imsg_compose(ms->filter->ibuf, fm.type, 0, 0, -1,
	    &fm, sizeof(fm));
	event_set(&ms->filter->ev, ms->filter->ibuf->fd, EV_READ|EV_WRITE, mfa_session_imsg, ms->filter);
	event_add(&ms->filter->ev, NULL);
	return 1;
}

void
mfa_session_pickup(struct mfa_session *ms)
{
	if (ms->fm.code == -1) {
		mfa_session_fail(ms);
		return;
	}

	ms->filter = TAILQ_NEXT(ms->filter, f_entry);
	if (ms->filter == NULL)
		mfa_session_done(ms);
	else
		mfa_session_proceed(ms);
}

void
mfa_session_done(struct mfa_session *ms)
{
	enum imsg_type imsg_type;

	switch (ms->state) {
	case S_HELO:
		imsg_type = IMSG_MFA_HELO;
		break;
	case S_MAIL_MFA:
		if (ms->ss.code != 530) {
			imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_MAIL, 0,
			    0, -1, &ms->ss, sizeof(ms->ss));
			mfa_session_destroy(ms);
			return;
		}
		imsg_type = IMSG_MFA_MAIL;
		break;
	case S_RCPT_MFA:
		if (ms->ss.code != 530) {
			imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_LKA_RULEMATCH,
			    0, 0, -1, &ms->ss, sizeof(ms->ss));
			mfa_session_destroy(ms);
			return;
		}
		imsg_type = IMSG_MFA_RCPT;
		break;
	default:
		fatalx("mfa_session_done: unsupported state");
	}

	imsg_compose_event(env->sc_ievs[PROC_SMTP], imsg_type, 0, 0,
	    -1, &ms->ss, sizeof(struct submit_status));
	mfa_session_destroy(ms);
}

struct mfa_session *
mfa_session_find(u_int64_t id)
{
	struct mfa_session key;

	key.id = id;
	return SPLAY_FIND(mfatree, &env->mfa_sessions, &key);
}

struct mfa_session *
mfa_session_xfind(u_int64_t id)
{
	struct mfa_session *ms;

	ms = mfa_session_find(id);
	if (ms == NULL)
		fatalx("mfa_session_xfind: mfa session missing");

	return ms;
}

void
mfa_session_fail(struct mfa_session *ms)
{
	ms->ss.code = 530;
	mfa_session_done(ms);
}

void
mfa_session_destroy(struct mfa_session *ms)
{
	SPLAY_REMOVE(mfatree, &env->mfa_sessions, ms);
	free(ms);
}

void
mfa_session_imsg(int fd, short event, void *p)
{
	struct filter	       *filter = p;
	struct mfa_session     *ms;
	struct imsg		imsg;
	ssize_t			n;
	struct filter_msg	fm;
	short			evflags = EV_READ;

	if (event & EV_READ) {
		n = imsg_read(filter->ibuf);
		if (n == -1)
			fatal("imsg_read");
		if (n == 0) {
			event_del(&filter->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&filter->ibuf->w) == -1)
			fatal("msgbuf_write");
		if (filter->ibuf->w.queued)
			evflags |= EV_WRITE;
	}

	for (;;) {
		n = imsg_get(filter->ibuf, &imsg);
		if (n == -1)
			fatalx("imsg_get");
		if (n == 0)
			break;

		if ((imsg.hdr.len - IMSG_HEADER_SIZE)
		    != sizeof(fm))
			fatalx("corrupted imsg");

		memcpy(&fm, imsg.data, sizeof (fm));
		if (fm.version != FILTER_API_VERSION)
			fatalx("API version mismatch");

		switch (imsg.hdr.type) {
		case FILTER_HELO:
		case FILTER_EHLO:
		case FILTER_MAIL:
		case FILTER_RCPT:
		case FILTER_DATA:
			ms = mfa_session_xfind(fm.id);

			/* overwrite filter code */
			ms->fm.code = fm.code;

			/* success, overwrite */
			if (fm.code == 1)
				ms->fm = fm;

			mfa_session_pickup(ms);
			break;
		default:
			fatalx("unsupported imsg");
		}
		imsg_free(&imsg);
	}
	event_set(&filter->ev, filter->ibuf->fd, evflags,
	    mfa_session_imsg, filter);
	event_add(&filter->ev, NULL);
}

int
mfa_session_cmp(struct mfa_session *s1, struct mfa_session *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return -1;

	if (s1->id > s2->id)
		return 1;

	return 0;
}

SPLAY_GENERATE(mfatree, mfa_session, nodes, mfa_session_cmp);
