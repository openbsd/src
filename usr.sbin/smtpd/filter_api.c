/*	$OpenBSD: filter_api.c,v 1.6 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
#include <sys/uio.h>

#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

static struct tree		queries;

struct query {
	uint64_t		qid;
	enum filter_hook	hook;
};
static int			register_done;

static struct filter_internals {
	struct mproc	p;

	uint32_t	hooks;
	uint32_t	flags;

	struct {
		void (*notify)(uint64_t, enum filter_status);
		void (*connect)(uint64_t, uint64_t, struct filter_connect *);
		void (*helo)(uint64_t, uint64_t, const char *);
		void (*mail)(uint64_t, uint64_t, struct mailaddr *);
		void (*rcpt)(uint64_t, uint64_t, struct mailaddr *);
		void (*data)(uint64_t, uint64_t);
		void (*dataline)(uint64_t, const char *);
		void (*eom)(uint64_t, uint64_t);
		void (*event)(uint64_t, enum filter_hook);
	} cb;

} fi;

static void filter_api_init(void);
static void filter_response(uint64_t, int, int, const char *line, int);
static void filter_dispatch(struct mproc *, struct imsg *);
static void filter_dispatch_event(uint64_t, enum filter_hook);
static void filter_dispatch_dataline(uint64_t, const char *);
static void filter_dispatch_data(uint64_t, uint64_t);
static void filter_dispatch_eom(uint64_t, uint64_t);
static void filter_dispatch_notify(uint64_t, enum filter_status);
static void filter_dispatch_connect(uint64_t, uint64_t, struct filter_connect *);
static void filter_dispatch_helo(uint64_t, uint64_t, const char *);
static void filter_dispatch_mail(uint64_t, uint64_t, struct mailaddr *);
static void filter_dispatch_rcpt(uint64_t, uint64_t, struct mailaddr *);

const char *
proc_to_str(int proc)
{
	return "PEER";
}

void
filter_api_on_notify(void(*cb)(uint64_t, enum filter_status))
{
	filter_api_init();

	fi.cb.notify = cb;
}

void
filter_api_on_connect(void(*cb)(uint64_t, uint64_t, struct filter_connect *))
{
	filter_api_init();

	fi.hooks |= HOOK_CONNECT;
	fi.cb.connect = cb;
}

void
filter_api_on_helo(void(*cb)(uint64_t, uint64_t, const char *))
{
	filter_api_init();

	fi.hooks |= HOOK_HELO;
	fi.cb.helo = cb;
}

void
filter_api_on_mail(void(*cb)(uint64_t, uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_MAIL;
	fi.cb.mail = cb;
}

void
filter_api_on_rcpt(void(*cb)(uint64_t, uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_RCPT;
	fi.cb.rcpt = cb;
}

void
filter_api_on_data(void(*cb)(uint64_t, uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_DATA;
	fi.cb.data = cb;
}

void
filter_api_on_dataline(void(*cb)(uint64_t, const char *), int flags)
{
	filter_api_init();

	fi.hooks |= HOOK_DATALINE;
	fi.flags |= flags & FILTER_ALTERDATA;
	fi.cb.dataline = cb;
}

void
filter_api_on_eom(void(*cb)(uint64_t, uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_EOM;
	fi.cb.eom = cb;
}

void
filter_api_on_event(void(*cb)(uint64_t, enum filter_hook))
{
	filter_api_init();

	fi.hooks |= HOOK_DISCONNECT | HOOK_RESET | HOOK_COMMIT;
	fi.cb.event = cb;
}

void
filter_api_loop(void)
{
	if (register_done) {
		errx(1, "filter_api_loop already called");
		return;
	}

	filter_api_init();

	register_done = 1;

	if (event_dispatch() < 0)
		errx(1, "event_dispatch");
}

void
filter_api_accept(uint64_t id)
{
	filter_response(id, FILTER_OK, 0, NULL, 0);
}

void
filter_api_accept_notify(uint64_t id)
{
	filter_response(id, FILTER_OK, 0, NULL, 1);
}

void
filter_api_reject(uint64_t id, enum filter_status status)
{
	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(id, status, 0, NULL, 0);
}

void
filter_api_reject_code(uint64_t id, enum filter_status status, uint32_t code,
    const char *line)
{
	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(id, status, code, line, 0);
}

void
filter_api_data(uint64_t id, const char *line)
{
	m_create(&fi.p, IMSG_FILTER_DATA, 0, 0, -1, 1024);
	m_add_id(&fi.p, id);
	m_add_string(&fi.p, line);
	m_close(&fi.p);
}

static void
filter_response(uint64_t qid, int status, int code, const char *line, int notify)
{
	struct filter_query	*q;

	q = tree_xpop(&queries, qid);
	free(q);

	m_create(&fi.p, IMSG_FILTER_RESPONSE, 0, 0, -1, 64);
	m_add_id(&fi.p, qid);
	m_add_int(&fi.p, status);
	m_add_int(&fi.p, code);
	m_add_int(&fi.p, notify);
	if (line)
		m_add_string(&fi.p, line);
	m_close(&fi.p);
}

static void
filter_api_init(void)
{
	static int	init = 0;

	if (init)
		return;

	init = 1;

	bzero(&fi, sizeof(fi));
	tree_init(&queries);
	event_init();
	mproc_init(&fi.p, 0);
}

static void
filter_dispatch(struct mproc *p, struct imsg *imsg)
{
	struct filter_connect	 q_connect;
	struct mailaddr		 maddr;
	struct msg		 m;
	const char		*line;
	uint32_t		 v;
	uint64_t		 id, qid;
	int			 status, event, hook;

	switch (imsg->hdr.type) {
	case IMSG_FILTER_REGISTER:
		m_msg(&m, imsg);
		m_get_u32(&m, &v);
		m_end(&m);
		if (v != FILTER_API_VERSION)
			errx(1, "API version mismatch");
		m_create(p, IMSG_FILTER_REGISTER, 0, 0, -1, 18);
		m_add_int(p, fi.hooks);
		m_add_int(p, fi.flags);
		m_close(p);
		break;

	case IMSG_FILTER_EVENT:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_int(&m, &event);
		m_end(&m);
		filter_dispatch_event(id, event);
		break;

	case IMSG_FILTER_QUERY:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_id(&m, &qid);
		m_get_int(&m, &hook);
		tree_xset(&queries, qid, NULL);
		switch(hook) {
		case HOOK_CONNECT:
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.local);
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.remote);
			m_get_string(&m, &q_connect.hostname);
			m_end(&m);
			filter_dispatch_connect(id, qid, &q_connect);
			break;
		case HOOK_HELO:
			m_get_string(&m, &line);
			m_end(&m);
			filter_dispatch_helo(id, qid, line);
			break;
		case HOOK_MAIL:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_dispatch_mail(id, qid, &maddr);
			break;
		case HOOK_RCPT:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_dispatch_rcpt(id, qid, &maddr);
			break;
		case HOOK_DATA:
			m_end(&m);
			filter_dispatch_data(id, qid);
			break;
		case HOOK_EOM:
			m_end(&m);
			filter_dispatch_eom(id, qid);
			break;
		default:
			errx(1, "bad query hook", hook);
		}
		break;

	case IMSG_FILTER_NOTIFY:
		m_msg(&m, imsg);
		m_get_id(&m, &qid);
		m_get_int(&m, &status);
		m_end(&m);
		filter_dispatch_notify(qid, status);
		break;

	case IMSG_FILTER_DATA:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_string(&m, &line);
		m_end(&m);
		filter_dispatch_dataline(id, line);
		break;
	}
}

static void
filter_dispatch_event(uint64_t id,  enum filter_hook event)
{
	fi.cb.event(id, event);
}

static void
filter_dispatch_notify(uint64_t qid, enum filter_status status)
{
	fi.cb.notify(qid, status);
}

static void
filter_dispatch_connect(uint64_t id, uint64_t qid, struct filter_connect *conn)
{
	fi.cb.connect(id, qid, conn);
}

static void
filter_dispatch_helo(uint64_t id, uint64_t qid, const char *helo)
{
	fi.cb.helo(id, qid, helo);
}

static void
filter_dispatch_mail(uint64_t id, uint64_t qid, struct mailaddr *mail)
{
	fi.cb.mail(id, qid, mail);
}

static void
filter_dispatch_rcpt(uint64_t id, uint64_t qid, struct mailaddr *rcpt)
{
	fi.cb.rcpt(id, qid, rcpt);
}

static void
filter_dispatch_data(uint64_t id, uint64_t qid)
{
	fi.cb.data(id, qid);
}

static void
filter_dispatch_dataline(uint64_t id, const char *data)
{
	fi.cb.dataline(id, data);
}

static void
filter_dispatch_eom(uint64_t id, uint64_t qid)
{
	fi.cb.eom(id, qid);
}
