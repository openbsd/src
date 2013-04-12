/*	$OpenBSD: mfa_session.c,v 1.16 2013/04/12 18:22:49 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <inttypes.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

enum {
	QT_QUERY,
	QT_EVENT,
};

enum {
	QUERY_READY,
	QUERY_WAITING,
	QUERY_RUNNING,
	QUERY_DONE
};

struct mfa_filter {
	TAILQ_ENTRY(mfa_filter)		 entry;
	struct mproc			 mproc;
	int				 hooks;
	int				 flags;
	int				 ready;
};

struct mfa_filter_chain {
	TAILQ_HEAD(, mfa_filter)	filters;
};

struct mfa_session {
	uint64_t				id;
	int					terminate;
	TAILQ_HEAD(mfa_queries, mfa_query)	queries;
};

struct mfa_query {
	uint64_t		 qid;
	int			 type;
	int			 hook;
	struct mfa_session	*session;
	TAILQ_ENTRY(mfa_query)	 entry;

	int			 state;
	int			 hasrun;
	struct mfa_filter	*current;
	struct tree		 notify;  /* list of filters to notify */

	/* current data */
	union {
		struct {
			struct sockaddr_storage	 local;
			struct sockaddr_storage	 remote;
			char			 hostname[MAXHOSTNAMELEN];
		} connect;
		char			line[SMTPD_MAXLINESIZE];
		struct mailaddr		maddr;
	} u;

	/* current response */
	struct {
		int	 status;
		int	 code;
		char	*response;	
	} smtp;
};

static void mfa_filter_imsg(struct mproc *, struct imsg *);
static struct mfa_query *mfa_query(struct mfa_session *, int, int);
static void mfa_drain_query(struct mfa_query *);
static void mfa_run_query(struct mfa_filter *, struct mfa_query *);
static void mfa_run_data(struct mfa_filter *, uint64_t, const char *);
static struct mfa_filter_chain	chain;

static const char * mfa_query_to_text(struct mfa_query *);
static const char * mfa_filter_to_text(struct mfa_filter *);
static const char * type_to_str(int);
static const char * hook_to_str(int);
static const char * status_to_str(int);
static const char * filterimsg_to_str(int);

struct tree	sessions;
struct tree	queries;

void
mfa_filter_init(void)
{
	static int		 init = 0;
	struct filter		*filter;
	void			*iter;
	struct mfa_filter	*f;
	struct mproc		*p;

	if (init)
		return;
	init = 1;

	tree_init(&sessions);
	tree_init(&queries);

	TAILQ_INIT(&chain.filters);

	iter = NULL;
	while (dict_iter(&env->sc_filters, &iter, NULL, (void **)&filter)) {
		f = xcalloc(1, sizeof *f, "mfa_filter_init");
		p = &f->mproc;
		p->handler = mfa_filter_imsg;
		p->proc = -1;
		p->name = xstrdup(filter->name, "mfa_filter_init");
		p->data = f;
		if (mproc_fork(p, filter->path, filter->name) < 0)
			fatalx("mfa_filter_init");
		m_create(p, IMSG_FILTER_REGISTER, 0, 0, -1, 5);
		m_add_u32(p, FILTER_API_VERSION);
		m_close(p);
		mproc_enable(p);
		TAILQ_INSERT_TAIL(&chain.filters, f, entry);
	}

	if (TAILQ_FIRST(&chain.filters) == NULL)
		mfa_ready();
}

void
mfa_filter_connect(uint64_t id, const struct sockaddr *local,
	const struct sockaddr *remote, const char *host)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	s = xcalloc(1, sizeof(*s), "mfa_query_connect");
	s->id = id;
	TAILQ_INIT(&s->queries);
	tree_xset(&sessions, s->id, s);

	q = mfa_query(s, QT_QUERY, HOOK_CONNECT);

	memmove(&q->u.connect.local, local, local->sa_len);
	memmove(&q->u.connect.remote, remote, remote->sa_len);
	strlcpy(q->u.connect.hostname, host, sizeof(q->u.connect.hostname));

	q->smtp.status = MFA_OK;
	q->smtp.code = 0;
	q->smtp.response = NULL;

	mfa_drain_query(q);
}

void
mfa_filter_event(uint64_t id, int hook)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	/* On disconnect, the session is virtualy dead */
	if (hook == HOOK_DISCONNECT)
		s = tree_xpop(&sessions, id);
	else
		s = tree_xget(&sessions, id);
	q = mfa_query(s, QT_EVENT, hook);

	mfa_drain_query(q);
}

void
mfa_filter_mailaddr(uint64_t id, int hook, const struct mailaddr *maddr)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	s = tree_xget(&sessions, id);
	q = mfa_query(s, QT_QUERY, hook);

	strlcpy(q->u.maddr.user, maddr->user, sizeof(q->u.maddr.user));
	strlcpy(q->u.maddr.domain, maddr->domain, sizeof(q->u.maddr.domain));

	mfa_drain_query(q);
}

void
mfa_filter_line(uint64_t id, int hook, const char *line)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	s = tree_xget(&sessions, id);
	q = mfa_query(s, QT_QUERY, hook);

	strlcpy(q->u.line, line, sizeof(q->u.line));

	mfa_drain_query(q);
}

void
mfa_filter(uint64_t id, int hook)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	s = tree_xget(&sessions, id);
	q = mfa_query(s, QT_QUERY, hook);

	mfa_drain_query(q);
}

void
mfa_filter_data(uint64_t id, const char *line)
{
	mfa_run_data(TAILQ_FIRST(&chain.filters), id, line);
}

static void
mfa_run_data(struct mfa_filter *f, uint64_t id, const char *line)
{
	struct mproc	*p;
	size_t		 len;

	log_trace(TRACE_MFA,
	    "mfa: running data for %016"PRIx64" on filter %p: %s", id, f, line);

	len = 16 + strlen(line);

	/* Send the dataline to the filters that want to see it. */
	while (f) {
		if (f->hooks & HOOK_DATALINE) {
			p = &f->mproc;
			m_create(p, IMSG_FILTER_DATA, 0, 0, -1, len);
			m_add_id(p, id);
			m_add_string(p, line);
			m_close(p);

			/*
			 * If this filter wants to alter data, we stop
			 * iterating here, and the filter becomes responsible
			 * for sending datalines back.
			 */
			if (f->flags & FILTER_ALTERDATA) {
				log_trace(TRACE_MFA,
	 			   "mfa: expect datalines from filter %s",
				   mfa_filter_to_text(f));
				return;
			}
		}
		f = TAILQ_NEXT(f, entry);
	}

	/* When all filters are done, send the line back to the smtp process. */
	log_trace(TRACE_MFA,
	    "mfa: sending final data to smtp for %016"PRIx64" on filter %p: %s", id, f, line);

	m_create(p_smtp, IMSG_MFA_SMTP_DATA, 0, 0, -1, len);
	m_add_id(p_smtp, id);
	m_add_string(p_smtp, line);
	m_close(p_smtp);
}

static struct mfa_query *
mfa_query(struct mfa_session *s, int type, int hook)
{
	struct mfa_query	*q;

	q = xcalloc(1, sizeof *q, "mfa_query");
	q->qid = generate_uid();
	q->session = s;
	q->type = type;
	q->hook = hook;
	tree_init(&q->notify);
	TAILQ_INSERT_TAIL(&s->queries, q, entry);

	q->state = QUERY_READY;
	q->current = TAILQ_FIRST(&chain.filters);
	q->hasrun = 0;

	log_trace(TRACE_MFA, "mfa: new query %s %s", type_to_str(type),
	    hook_to_str(hook));

	return (q);
}

static void
mfa_drain_query(struct mfa_query *q)
{
	struct mfa_filter	*f;
	struct mfa_query	*prev;
	size_t			 len;

	log_trace(TRACE_MFA, "mfa: draining query %s", mfa_query_to_text(q));

	/*
	 * The query must be passed through all filters that registered
	 * a hook, until one rejects it.  
	 */
	while (q->state != QUERY_DONE) {

		/* Walk over all filters */
		while (q->current) {

			/* Trigger the current filter if not done yet. */
			if (!q->hasrun) {
				mfa_run_query(q->current, q);
				q->hasrun = 1;
			}
			if (q->state == QUERY_RUNNING) {
				log_trace(TRACE_MFA,
				    "mfa: waiting for running query %s",
				    mfa_query_to_text(q));
				return;
			}

			/*
			 * Do not move forward if the query ahead of us is
			 * waiting on this filter.
			 */
			prev = TAILQ_PREV(q, mfa_queries, entry);
			if (prev && prev->current == q->current) {
				q->state = QUERY_WAITING;
				log_trace(TRACE_MFA,
				    "mfa: query blocked by previoius query %s",
				    mfa_query_to_text(prev));
				return;
			}

			q->current = TAILQ_NEXT(q->current, entry);
			q->hasrun = 0;
		}
		q->state = QUERY_DONE;
	}

	if (q->type == QT_QUERY) {

		log_trace(TRACE_MFA,
		    "mfa: query 0x%016"PRIx64" done: "
		    "status=%s code=%i response=\"%s\"",
		    q->qid,
		    status_to_str(q->smtp.status),
		    q->smtp.code,
		    q->smtp.response);

		/* Done, notify all listeners and return smtp response */
		while (tree_poproot(&q->notify, NULL, (void**)&f)) {
			m_create(&f->mproc, IMSG_FILTER_NOTIFY, 0, 0, -1, 16);
			m_add_id(&f->mproc, q->qid);
			m_add_int(&f->mproc, q->smtp.status);
			m_close(&f->mproc);
		}

		len = 48;
		if (q->smtp.response)
			len += strlen(q->smtp.response);
		m_create(p_smtp, IMSG_MFA_SMTP_RESPONSE, 0, 0, -1, len);
		m_add_id(p_smtp, q->session->id);
		m_add_int(p_smtp, q->smtp.status);
		m_add_u32(p_smtp, q->smtp.code);
		if (q->smtp.response)
			m_add_string(p_smtp, q->smtp.response);
		m_close(p_smtp);

		free(q->smtp.response);
	}

	TAILQ_REMOVE(&q->session->queries, q, entry);
	/* If the query was a disconnect event, the session can be freed */
	if (q->type == HOOK_DISCONNECT) {
		/* XXX assert prev == NULL */
		free(q->session);
	}

	log_trace(TRACE_MFA, "mfa: freeing query %016" PRIx64, q->qid);
	free(q);
}

static void
mfa_run_query(struct mfa_filter *f, struct mfa_query *q)
{
	if ((f->hooks & q->hook) == 0) {
		log_trace(TRACE_MFA, "mfa: skipping filter %s for query %s",
		    mfa_filter_to_text(f), mfa_query_to_text(q));
		return;
	}

	log_trace(TRACE_MFA, "mfa: running filter %s for query %s",
	    mfa_filter_to_text(f), mfa_query_to_text(q));

	if (q->type == QT_QUERY) {
		m_create(&f->mproc, IMSG_FILTER_QUERY, 0, 0, -1, 1024);
		m_add_id(&f->mproc, q->session->id);
		m_add_id(&f->mproc, q->qid);
		m_add_int(&f->mproc, q->hook);

		switch (q->hook) {
		case HOOK_CONNECT:
			m_add_sockaddr(&f->mproc,
			    (struct sockaddr *)&q->u.connect.local);
			m_add_sockaddr(&f->mproc,
			    (struct sockaddr *)&q->u.connect.remote);
			m_add_string(&f->mproc, q->u.connect.hostname);
			break;
		case HOOK_HELO:
			m_add_string(&f->mproc, q->u.line);
			break;
		case HOOK_MAIL:
		case HOOK_RCPT:
			m_add_mailaddr(&f->mproc, &q->u.maddr);
			break;
		default:
			break;
		}

		m_close(&f->mproc);

		tree_xset(&queries, q->qid, q);
		q->state = QUERY_RUNNING;
	}
	else {
		m_create(&f->mproc, IMSG_FILTER_EVENT, 0, 0, -1, 16);
		m_add_id(&f->mproc, q->session->id);
		m_add_int(&f->mproc, q->hook);
		m_close(&f->mproc);
 	}
}

static void
mfa_filter_imsg(struct mproc *p, struct imsg *imsg)
{
	struct mfa_filter	*f;
	struct mfa_query	*q, *next;
	struct msg		 m;
	const char		*line;
	uint64_t		 id, qid;
	int			 status, code, notify;

	f = p->data;
	log_trace(TRACE_MFA, "mfa: imsg %s from filter %s",
	    filterimsg_to_str(imsg->hdr.type),
	    mfa_filter_to_text(f));

	switch (imsg->hdr.type) {

	case IMSG_FILTER_REGISTER:
		if (f->ready) {
			log_warnx("warn: filter \"%s\" already registered",
			    f->mproc.name);
			exit(1);
		}
		
		m_msg(&m, imsg);
		m_get_int(&m, &f->hooks);
		m_get_int(&m, &f->flags);
		m_end(&m);
		f->ready = 1;

		log_debug("debug: filter \"%s\": hooks 0x%08x flags 0x%04x",
		    f->mproc.name, f->hooks, f->flags);

		TAILQ_FOREACH(f, &chain.filters, entry)
			if (!f->ready)
				return;
		mfa_ready();
		break;

	case IMSG_FILTER_DATA:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_string(&m, &line);
		m_end(&m);
		mfa_run_data(TAILQ_NEXT(f, entry), id, line);
		break;

	case IMSG_FILTER_RESPONSE:
		m_msg(&m, imsg);
		m_get_id(&m, &qid);
		m_get_int(&m, &status);
		m_get_int(&m, &code);
		m_get_int(&m, &notify);
		if (m_is_eom(&m))
			line = NULL;
		else
			m_get_string(&m, &line);
		
		m_end(&m);

		q = tree_xpop(&queries, qid);
		q->smtp.status = status;
		if (code)
			q->smtp.code = code;
		if (line) {
			free(q->smtp.response);
			q->smtp.response = xstrdup(line, "mfa_filter_imsg");
		}
		q->state = (status == FILTER_OK) ? QUERY_READY : QUERY_DONE;
		if (notify)
			tree_xset(&q->notify, (uintptr_t)(f), f);

		next = TAILQ_NEXT(q, entry);
		mfa_drain_query(q);

		/*
		 * If there is another query after this one which is waiting,
		 * make it move forward.
		 */
		if (next && next->state == QUERY_WAITING)
			mfa_drain_query(next);
		break;

	default:
		log_warnx("bad imsg from filter %s", p->name);
		exit(1);
	}
}


static const char *
mfa_query_to_text(struct mfa_query *q)
{
	static char buf[1024];
	char tmp[1024];

	tmp[0] = '\0';

	switch(q->hook) {

	case HOOK_CONNECT:
		strlcat(tmp, "=", sizeof tmp);
		strlcat(tmp, ss_to_text(&q->u.connect.local), sizeof tmp);
		strlcat(tmp, " <-> ", sizeof tmp);
		strlcat(tmp, ss_to_text(&q->u.connect.remote), sizeof tmp);
		strlcat(tmp, "(", sizeof tmp);
		strlcat(tmp, q->u.connect.hostname, sizeof tmp);
		strlcat(tmp, ")", sizeof tmp);
		break;

	case HOOK_MAIL:
	case HOOK_RCPT:
		snprintf(tmp, sizeof tmp, "=%s@%s",
		    q->u.maddr.user, q->u.maddr.domain);
		break;

	case HOOK_HELO:
		snprintf(tmp, sizeof tmp, "=%s", q->u.line);
		break;

	default:
		break;
	}

	snprintf(buf, sizeof buf, "%016"PRIx64"[%s,%s%s]",
	    q->qid, type_to_str(q->type), hook_to_str(q->hook), tmp);

	return (buf);
}

static const char *
mfa_filter_to_text(struct mfa_filter *f)
{
	static char buf[1024];

	snprintf(buf, sizeof buf, "%s[hooks=0x%04x,flags=0x%x]",
	    f->mproc.name, f->hooks, f->flags);

	return (buf);
}


#define CASE(x) case x : return #x

static const char *
filterimsg_to_str(int imsg)
{
	switch (imsg) {
	CASE(IMSG_FILTER_REGISTER);
	CASE(IMSG_FILTER_EVENT);
	CASE(IMSG_FILTER_QUERY);
	CASE(IMSG_FILTER_NOTIFY);
	CASE(IMSG_FILTER_DATA);
	CASE(IMSG_FILTER_RESPONSE);
	default:
		return "IMSG_FILTER_???";
	}
}

static const char *
hook_to_str(int hook)
{
	switch (hook) {
	CASE(HOOK_CONNECT);
	CASE(HOOK_HELO);
	CASE(HOOK_MAIL);
	CASE(HOOK_RCPT);
	CASE(HOOK_DATA);
	CASE(HOOK_EOM);
	CASE(HOOK_RESET);
	CASE(HOOK_DISCONNECT);
	CASE(HOOK_COMMIT);
	CASE(HOOK_ROLLBACK);
	CASE(HOOK_DATALINE);
	default:
		return "HOOK_???";
	}
}

static const char *
type_to_str(int type)
{
	switch (type) {
	CASE(QT_QUERY);
	CASE(QT_EVENT);
	default:
		return "QT_???";
	}
}

static const char *
status_to_str(int status)
{
	switch (status) {
	CASE(MFA_OK);
	CASE(MFA_FAIL);
	CASE(MFA_CLOSE);
	default:
		return "MFA_???";
	}
}
