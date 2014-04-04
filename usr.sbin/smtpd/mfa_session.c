/*	$OpenBSD: mfa_session.c,v 1.21 2014/04/04 16:10:42 eric Exp $	*/

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


struct mfa_filterproc {
	TAILQ_ENTRY(mfa_filterproc)	 entry;
	struct mproc			 mproc;
	int				 hooks;
	int				 flags;
	int				 ready;
};

struct mfa_filter {
	TAILQ_ENTRY(mfa_filter)		 entry;
	struct mfa_filterproc		*proc;
};
TAILQ_HEAD(mfa_filters, mfa_filter);

struct mfa_session {
	uint64_t				 id;
	int					 terminate;
	TAILQ_HEAD(mfa_queries, mfa_query)	 queries;
	struct mfa_filters			*filters;
	struct mfa_filter			*fcurr;
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
			char			 hostname[SMTPD_MAXHOSTNAMELEN];
		} connect;
		char			line[SMTPD_MAXLINESIZE];
		struct mailaddr		maddr;
		size_t			datalen;
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
static void mfa_set_fdout(struct mfa_session *, int);

static TAILQ_HEAD(, mfa_filterproc)	procs;
struct dict				chains;

static const char * mfa_query_to_text(struct mfa_query *);
static const char * mfa_filter_to_text(struct mfa_filter *);
static const char * mfa_filterproc_to_text(struct mfa_filterproc *);
static const char * type_to_str(int);
static const char * hook_to_str(int);
static const char * status_to_str(int);
static const char * filterimsg_to_str(int);

struct tree	sessions;
struct tree	queries;


static void
mfa_extend_chain(struct mfa_filters *chain, const char *name)
{
	struct mfa_filter	*n;
	struct mfa_filters	*fchain;
	struct filter		*fconf;
	int			 i;

	fconf = dict_xget(&env->sc_filters, name);
	if (fconf->chain) {
		log_debug("mfa:     extending with \"%s\"", name);
		for (i = 0; i < MAX_FILTER_PER_CHAIN; i++) {
			if (!fconf->filters[i][0])
				break;
			mfa_extend_chain(chain, fconf->filters[i]);
		}
	}
	else {
		log_debug("mfa:     adding filter \"%s\"", name);
		n = xcalloc(1, sizeof(*n), "mfa_extend_chain");
		fchain = dict_get(&chains, name);
		n->proc = TAILQ_FIRST(fchain)->proc;
		TAILQ_INSERT_TAIL(chain, n, entry);
	}
}

void
mfa_filter_prepare(void)
{
	static int		 prepare = 0;
	struct filter		*filter;
	void			*iter;
	struct mfa_filterproc	*proc;
	struct mfa_filters	*fchain;
	struct mfa_filter	*f;
	struct mproc		*p;
	int			 done, i;

	if (prepare)
		return;
	prepare = 1;

	TAILQ_INIT(&procs);
	dict_init(&chains);

	log_debug("mfa: building simple chains...");

	/* create all filter proc and associated chains */
	iter = NULL;
	while (dict_iter(&env->sc_filters, &iter, NULL, (void **)&filter)) {
		if (filter->chain)
			continue;

		log_debug("mfa: building simple chain \"%s\"", filter->name);

		proc = xcalloc(1, sizeof(*proc), "mfa_filter_init");
		p = &proc->mproc;
		p->handler = mfa_filter_imsg;
		p->proc = PROC_FILTER;
		p->name = xstrdup(filter->name, "mfa_filter_init");
		p->data = proc;
		if (mproc_fork(p, filter->path, filter->name) < 0)
			fatalx("mfa_filter_init");

		log_debug("mfa: registering proc \"%s\"", filter->name);

		f = xcalloc(1, sizeof(*f), "mfa_filter_init");
		f->proc = proc;

		TAILQ_INSERT_TAIL(&procs, proc, entry);
		fchain = xcalloc(1, sizeof(*fchain), "mfa_filter_prepare");
		TAILQ_INIT(fchain);
		TAILQ_INSERT_TAIL(fchain, f, entry);
		dict_xset(&chains, filter->name, fchain);
		filter->done = 1;
	}

	log_debug("mfa: building complex chains...");

	/* resolve all chains */
	done = 0;
	while (!done) {
		done = 1;
		iter = NULL;
		while (dict_iter(&env->sc_filters, &iter, NULL, (void **)&filter)) {
			if (filter->done)
				continue;
			done = 0;
			filter->done = 1;
			for (i = 0; i < MAX_FILTER_PER_CHAIN; i++) {
				if (!filter->filters[i][0])
					break;
				if (!dict_get(&chains, filter->filters[i])) {
					filter->done = 0;
					break;
				}
			}
			if (filter->done == 0)
				continue;
			fchain = xcalloc(1, sizeof(*fchain), "mfa_filter_prepare");
			TAILQ_INIT(fchain);
			log_debug("mfa: building chain \"%s\"...", filter->name);
			for (i = 0; i < MAX_FILTER_PER_CHAIN; i++) {
				if (!filter->filters[i][0])
					break;
				mfa_extend_chain(fchain, filter->filters[i]);
			}
			log_debug("mfa: done building chain \"%s\"", filter->name);
			dict_xset(&chains, filter->name, fchain);
		}
	}
	log_debug("mfa: done building complex chains");

	if (dict_get(&chains, "default") == NULL) {
		log_debug("mfa: done building default chain");
		fchain = xcalloc(1, sizeof(*fchain), "mfa_filter_prepare");
		TAILQ_INIT(fchain);
		dict_xset(&chains, "default", fchain);
	}
}

void
mfa_filter_init(void)
{
	static int		 init = 0;
	struct mfa_filterproc	*p;

	if (init)
		return;
	init = 1;

	tree_init(&sessions);
	tree_init(&queries);

	TAILQ_FOREACH(p, &procs, entry) {
		m_create(&p->mproc, IMSG_FILTER_REGISTER, 0, 0, -1);
		m_add_u32(&p->mproc, FILTER_API_VERSION);
		m_add_string(&p->mproc, p->mproc.name);
		m_close(&p->mproc);
		mproc_enable(&p->mproc);
	}

	if (TAILQ_FIRST(&procs) == NULL)
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
	s->filters = dict_xget(&chains, "default");
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
mfa_filter_eom(uint64_t id, int hook, size_t datalen)
{
	struct mfa_session	*s;
	struct mfa_query	*q;

	s = tree_xget(&sessions, id);
	q = mfa_query(s, QT_QUERY, hook);
	q->u.datalen = datalen;

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

static void
mfa_set_fdout(struct mfa_session *s, int fdout)
{
	struct mproc	*p;

	while(s->fcurr) {
		if (s->fcurr->proc->hooks & HOOK_DATALINE) {
			log_trace(TRACE_MFA, "mfa: sending fd %d to %s", fdout, mfa_filter_to_text(s->fcurr));
			p = &s->fcurr->proc->mproc;
			m_create(p, IMSG_FILTER_PIPE_SETUP, 0, 0, fdout);
			m_add_id(p, s->id);
			m_close(p);
			return;
		}
		s->fcurr = TAILQ_PREV(s->fcurr, mfa_filters, entry);
	}

	log_trace(TRACE_MFA, "mfa: chain input is %d", fdout);

	m_create(p_pony, IMSG_SMTP_MESSAGE_OPEN, 0, 0, fdout); /* XXX bogus */
	m_add_id(p_pony, s->id);
	m_add_int(p_pony, 1);
	m_close(p_pony);
	return;
}

void
mfa_build_fd_chain(uint64_t id, int fdout)
{
	struct mfa_session	*s;

	s = tree_xget(&sessions, id);
	s->fcurr = TAILQ_LAST(s->filters, mfa_filters);
	mfa_set_fdout(s, fdout);
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
	q->current = TAILQ_FIRST(s->filters);
	q->hasrun = 0;

	log_trace(TRACE_MFA, "filter: new query %s %s", type_to_str(type),
	    hook_to_str(hook));

	return (q);
}

static void
mfa_drain_query(struct mfa_query *q)
{
	struct mfa_filterproc	*proc;
	struct mfa_query	*prev;

	log_trace(TRACE_MFA, "filter: draining query %s", mfa_query_to_text(q));

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
				    "filter: waiting for running query %s",
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
				    "filter: query blocked by previoius query %s",
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
		    "filter: query %016"PRIx64" done: "
		    "status=%s code=%d response=\"%s\"",
		    q->qid,
		    status_to_str(q->smtp.status),
		    q->smtp.code,
		    q->smtp.response);

		/* Done, notify all listeners and return smtp response */
		while (tree_poproot(&q->notify, NULL, (void**)&proc)) {
			m_create(&proc->mproc, IMSG_FILTER_NOTIFY, 0, 0, -1);
			m_add_id(&proc->mproc, q->qid);
			m_add_int(&proc->mproc, q->smtp.status);
			m_close(&proc->mproc);
		}

		m_create(p_pony, IMSG_MFA_SMTP_RESPONSE, 0, 0, -1);
		m_add_id(p_pony, q->session->id);
		m_add_int(p_pony, q->smtp.status);
		m_add_u32(p_pony, q->smtp.code);
		if (q->smtp.response)
			m_add_string(p_pony, q->smtp.response);
		m_close(p_pony);

		free(q->smtp.response);
	}

	TAILQ_REMOVE(&q->session->queries, q, entry);
	/* If the query was a disconnect event, the session can be freed */
	if (q->hook == HOOK_DISCONNECT) {
		/* XXX assert prev == NULL */
		log_trace(TRACE_MFA, "filter: freeing session %016" PRIx64, q->session->id);
		free(q->session);
	}

	log_trace(TRACE_MFA, "filter: freeing query %016" PRIx64, q->qid);
	free(q);
}

static void
mfa_run_query(struct mfa_filter *f, struct mfa_query *q)
{
	if ((f->proc->hooks & q->hook) == 0) {
		log_trace(TRACE_MFA, "filter: skipping filter %s for query %s",
		    mfa_filter_to_text(f), mfa_query_to_text(q));
		return;
	}

	log_trace(TRACE_MFA, "filter: running filter %s for query %s",
	    mfa_filter_to_text(f), mfa_query_to_text(q));

	if (q->type == QT_QUERY) {
		m_create(&f->proc->mproc, IMSG_FILTER_QUERY, 0, 0, -1);
		m_add_id(&f->proc->mproc, q->session->id);
		m_add_id(&f->proc->mproc, q->qid);
		m_add_int(&f->proc->mproc, q->hook);

		switch (q->hook) {
		case HOOK_CONNECT:
			m_add_sockaddr(&f->proc->mproc,
			    (struct sockaddr *)&q->u.connect.local);
			m_add_sockaddr(&f->proc->mproc,
			    (struct sockaddr *)&q->u.connect.remote);
			m_add_string(&f->proc->mproc, q->u.connect.hostname);
			break;
		case HOOK_HELO:
			m_add_string(&f->proc->mproc, q->u.line);
			break;
		case HOOK_MAIL:
		case HOOK_RCPT:
			m_add_mailaddr(&f->proc->mproc, &q->u.maddr);
			break;
		case HOOK_EOM:
			m_add_u32(&f->proc->mproc, q->u.datalen);
			break;
		default:
			break;
		}
		m_close(&f->proc->mproc);

		tree_xset(&queries, q->qid, q);
		q->state = QUERY_RUNNING;
	}
	else {
		m_create(&f->proc->mproc, IMSG_FILTER_EVENT, 0, 0, -1);
		m_add_id(&f->proc->mproc, q->session->id);
		m_add_int(&f->proc->mproc, q->hook);
		m_close(&f->proc->mproc);
 	}
}

static void
mfa_filter_imsg(struct mproc *p, struct imsg *imsg)
{
	struct mfa_filterproc	*proc = p->data;
	struct mfa_session	*s;
	struct mfa_query	*q, *next;
	struct msg		 m;
	const char		*line;
	uint64_t		 qid;
	uint32_t		 datalen;
	int			 qhook, status, code, notify;

	if (imsg == NULL) {
		log_warnx("warn: filter \"%s\" closed unexpectedly", p->name);
		fatalx("exiting");
	}

	log_trace(TRACE_MFA, "filter: imsg %s from procfilter %s",
	    filterimsg_to_str(imsg->hdr.type),
	    mfa_filterproc_to_text(proc));

	switch (imsg->hdr.type) {

	case IMSG_FILTER_REGISTER:
		if (proc->ready) {
			log_warnx("warn: filter \"%s\" already registered",
			    proc->mproc.name);
			exit(1);
		}
		
		m_msg(&m, imsg);
		m_get_int(&m, &proc->hooks);
		m_get_int(&m, &proc->flags);
		m_end(&m);
		proc->ready = 1;

		log_debug("debug: filter \"%s\": hooks 0x%08x flags 0x%04x",
		    proc->mproc.name, proc->hooks, proc->flags);

		TAILQ_FOREACH(proc, &procs, entry)
			if (!proc->ready)
				return;
		mfa_ready();
		break;

	case IMSG_FILTER_RESPONSE:
		m_msg(&m, imsg);
		m_get_id(&m, &qid);
		m_get_int(&m, &qhook);
		if (qhook == HOOK_EOM)
			m_get_u32(&m, &datalen);
		m_get_int(&m, &status);
		m_get_int(&m, &code);
		m_get_int(&m, &notify);
		if (m_is_eom(&m))
			line = NULL;
		else
			m_get_string(&m, &line);
		m_end(&m);

		q = tree_xpop(&queries, qid);
		if (q->hook != qhook) {
			log_warnx("warn: mfa: hook mismatch %d != %d", q->hook, qhook);
			fatalx("exiting");
		}
		q->smtp.status = status;
		if (code)
			q->smtp.code = code;
		if (line) {
			free(q->smtp.response);
			q->smtp.response = xstrdup(line, "mfa_filter_imsg");
		}
		q->state = (status == FILTER_OK) ? QUERY_READY : QUERY_DONE;
		if (notify)
			tree_xset(&q->notify, (uintptr_t)(proc), proc);
		if (qhook == HOOK_EOM)
			q->u.datalen = datalen;

		next = TAILQ_NEXT(q, entry);
		mfa_drain_query(q);

		/*
		 * If there is another query after this one which is waiting,
		 * make it move forward.
		 */
		if (next && next->state == QUERY_WAITING)
			mfa_drain_query(next);
		break;

	case IMSG_FILTER_PIPE_SETUP:
		m_msg(&m, imsg);
		m_get_id(&m, &qid);
		m_end(&m);

		s = tree_xget(&sessions, qid);
		s->fcurr = TAILQ_PREV(s->fcurr, mfa_filters, entry);
		mfa_set_fdout(s, imsg->fd);
		break;

	default:
		log_warnx("warn: bad imsg from filter %s", p->name);
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

	snprintf(buf, sizeof buf, "filter:%s", mfa_filterproc_to_text(f->proc));

	return (buf);
}

static const char *
mfa_filterproc_to_text(struct mfa_filterproc *proc)
{
	static char buf[1024];

	snprintf(buf, sizeof buf, "%s[hooks=0x%08x,flags=0x%04x]",
	    proc->mproc.name, proc->hooks, proc->flags);

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
	CASE(IMSG_FILTER_PIPE_SETUP);
	CASE(IMSG_FILTER_PIPE_ABORT);
	CASE(IMSG_FILTER_NOTIFY);
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
