/*	$OpenBSD: mta.c,v 1.152 2013/02/05 10:53:57 nicm Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define MAXERROR_PER_HOST	4

#define MAXCONN_PER_HOST	10
#define MAXCONN_PER_ROUTE	5
#define MAXCONN_PER_SOURCE	50	/* XXX missing */
#define MAXCONN_PER_CONNECTOR	20
#define MAXCONN_PER_RELAY	100

#define CONNECTOR_DELAY_CONNECT	1
#define CONNECTOR_DELAY_LIMIT	5

static void mta_imsg(struct mproc *, struct imsg *);
static void mta_shutdown(void);
static void mta_sig_handler(int, short, void *);

static void mta_query_mx(struct mta_relay *);
static void mta_query_secret(struct mta_relay *);
static void mta_query_preference(struct mta_relay *);
static void mta_query_source(struct mta_relay *);
static void mta_on_mx(void *, void *, void *);
static void mta_on_source(struct mta_relay *, struct mta_source *);
static void mta_connect(struct mta_connector *);
static void mta_recycle(struct mta_connector *);
static void mta_drain(struct mta_relay *);
static void mta_relay_schedule(struct mta_relay *, unsigned int);
static void mta_relay_timeout(int, short, void *);
static void mta_flush(struct mta_relay *, int, const char *);
static struct mta_route *mta_find_route(struct mta_connector *);
static void mta_log(const struct mta_envelope *, const char *, const char *,
    const char *);

SPLAY_HEAD(mta_relay_tree, mta_relay);
static struct mta_relay *mta_relay(struct envelope *);
static void mta_relay_ref(struct mta_relay *);
static void mta_relay_unref(struct mta_relay *);
static int mta_relay_cmp(const struct mta_relay *, const struct mta_relay *);
SPLAY_PROTOTYPE(mta_relay_tree, mta_relay, entry, mta_relay_cmp);

SPLAY_HEAD(mta_host_tree, mta_host);
static struct mta_host *mta_host(const struct sockaddr *);
static void mta_host_ref(struct mta_host *);
static void mta_host_unref(struct mta_host *);
static int mta_host_cmp(const struct mta_host *, const struct mta_host *);
SPLAY_PROTOTYPE(mta_host_tree, mta_host, entry, mta_host_cmp);

SPLAY_HEAD(mta_domain_tree, mta_domain);
static struct mta_domain *mta_domain(char *, int);
#if 0
static void mta_domain_ref(struct mta_domain *);
#endif
static void mta_domain_unref(struct mta_domain *);
static int mta_domain_cmp(const struct mta_domain *, const struct mta_domain *);
SPLAY_PROTOTYPE(mta_domain_tree, mta_domain, entry, mta_domain_cmp);

SPLAY_HEAD(mta_source_tree, mta_source);
static struct mta_source *mta_source(const struct sockaddr *);
static void mta_source_ref(struct mta_source *);
static void mta_source_unref(struct mta_source *);
static const char *mta_source_to_text(struct mta_source *);
static int mta_source_cmp(const struct mta_source *, const struct mta_source *);
SPLAY_PROTOTYPE(mta_source_tree, mta_source, entry, mta_source_cmp);

static struct mta_connector *mta_connector(struct mta_relay *,
    struct mta_source *);
static void mta_connector_free(struct mta_connector *);
static const char *mta_connector_to_text(struct mta_connector *);

SPLAY_HEAD(mta_route_tree, mta_route);
static struct mta_route *mta_route(struct mta_source *, struct mta_host *);
#if 0
static void mta_route_ref(struct mta_route *);
#endif
static void mta_route_unref(struct mta_route *);
static const char *mta_route_to_text(struct mta_route *);
static int mta_route_cmp(const struct mta_route *, const struct mta_route *);
SPLAY_PROTOTYPE(mta_route_tree, mta_route, entry, mta_route_cmp);

static struct mta_relay_tree		relays;
static struct mta_domain_tree		domains;
static struct mta_host_tree		hosts;
static struct mta_source_tree		sources;
static struct mta_route_tree		routes;

static struct tree batches;

static struct tree wait_mx;
static struct tree wait_preference;
static struct tree wait_secret;
static struct tree wait_source;

void
mta_imsg(struct mproc *p, struct imsg *imsg)
{
	struct mta_relay	*relay;
	struct mta_task		*task;
	struct mta_source	*source;
	struct mta_domain	*domain;
	struct mta_mx		*mx, *imx;
	struct mta_envelope	*e;
	struct sockaddr_storage	 ss;
	struct tree		*batch;
	struct envelope		 evp;
	struct msg		 m;
	const char		*secret;
	uint64_t		 reqid;
	char			 buf[MAX_LINE_SIZE];
	int			 dnserror, preference, v, status;

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {

		case IMSG_MTA_BATCH:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			batch = xmalloc(sizeof *batch, "mta_batch");
			tree_init(batch);
			tree_xset(&batches, reqid, batch);
			log_trace(TRACE_MTA,
			    "mta: batch:%016" PRIx64 " created", reqid);
			return;

		case IMSG_MTA_BATCH_ADD:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_envelope(&m, &evp);
			m_end(&m);

			relay = mta_relay(&evp);
			batch = tree_xget(&batches, reqid);

			if ((task = tree_get(batch, relay->id)) == NULL) {
				log_trace(TRACE_MTA, "mta: new task for %s",
				    mta_relay_to_text(relay));
				task = xmalloc(sizeof *task, "mta_task");
				TAILQ_INIT(&task->envelopes);
				task->relay = relay;
				tree_xset(batch, relay->id, task);
				task->msgid = evpid_to_msgid(evp.id);
				if (evp.sender.user[0] || evp.sender.domain[0])
					snprintf(buf, sizeof buf, "%s@%s",
					    evp.sender.user, evp.sender.domain);
				else
					buf[0] = '\0';
				task->sender = xstrdup(buf, "mta_task:sender");
			} else
				mta_relay_unref(relay); /* from here */

			/*
			 * Technically, we could handle that by adding a msg
			 * level, but the batch sent by the scheduler should
			 * be valid.
			 */
			if (task->msgid != evpid_to_msgid(evp.id))
				errx(1, "msgid mismatch in batch");

			e = xcalloc(1, sizeof *e, "mta_envelope");
			e->id = evp.id;
			e->creation = evp.creation;
			snprintf(buf, sizeof buf, "%s@%s",
			    evp.dest.user, evp.dest.domain);
			e->dest = xstrdup(buf, "mta_envelope:dest");
			snprintf(buf, sizeof buf, "%s@%s",
			    evp.rcpt.user, evp.rcpt.domain);
			if (strcmp(buf, e->dest))
				e->rcpt = xstrdup(buf, "mta_envelope:rcpt");
			e->task = task;
			/* XXX honour relay->maxrcpt */
			TAILQ_INSERT_TAIL(&task->envelopes, e, entry);
			stat_increment("mta.envelope", 1);
			log_debug("debug: mta: received evp:%016" PRIx64
			    " for <%s>", e->id, e->dest);
			return;

		case IMSG_MTA_BATCH_END:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			batch = tree_xpop(&batches, reqid);
			log_trace(TRACE_MTA, "mta: batch:%016" PRIx64 " closed",
			    reqid);
			/* For all tasks, queue them on its relay */
			while (tree_poproot(batch, &reqid, (void**)&task)) {
				if (reqid != task->relay->id)
					errx(1, "relay id mismatch!");
				relay = task->relay;
				relay->ntask += 1;
				TAILQ_INSERT_TAIL(&relay->tasks, task, entry);
				stat_increment("mta.task", 1);
				mta_drain(relay);
				mta_relay_unref(relay); /* from BATCH_APPEND */
			}
			free(batch);
			return;

		case IMSG_QUEUE_MESSAGE_FD:
			mta_session_imsg(p, imsg);
			return;
		}
	}

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {

		case IMSG_LKA_SECRET:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &secret);
			m_end(&m);
			relay = tree_xpop(&wait_secret, reqid);
			if (secret[0])
				relay->secret = strdup(secret);
			if (relay->secret == NULL) {
				log_warnx("warn: Failed to retrieve secret "
				    "for %s", mta_relay_to_text(relay));
				relay->fail = IMSG_DELIVERY_TEMPFAIL;
				relay->failstr = "Could not retrieve secret";
			}
			relay->status &= ~RELAY_WAIT_SECRET;
			mta_drain(relay);
			mta_relay_unref(relay); /* from mta_query_secret() */
			return;

		case IMSG_LKA_SOURCE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_int(&m, &status);

			relay = tree_xpop(&wait_source, reqid);
			relay->status &= ~RELAY_WAIT_SOURCE;
			if (status == LKA_OK) {
				m_get_sockaddr(&m, (struct sockaddr*)&ss);
				source = mta_source((struct sockaddr *)&ss);
				mta_on_source(relay, source);
				mta_source_unref(source);
			}
			else {
				log_warnx("warn: Failed to get source address"
				    "for %s", mta_relay_to_text(relay));
			}
			m_end(&m);

			mta_drain(relay);
			mta_relay_unref(relay); /* from mta_query_source() */
			return;

		case IMSG_LKA_HELO:
			mta_session_imsg(p, imsg);
			return;

		case IMSG_DNS_HOST:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_sockaddr(&m, (struct sockaddr*)&ss);
			m_get_int(&m, &preference);
			m_end(&m);
			domain = tree_xget(&wait_mx, reqid);
			mx = xcalloc(1, sizeof *mx, "mta: mx");
			mx->host = mta_host((struct sockaddr*)&ss);
			mx->preference = preference;
			TAILQ_FOREACH(imx, &domain->mxs, entry) {
				if (imx->preference >= mx->preference) {
					TAILQ_INSERT_BEFORE(imx, mx, entry);
					return;
				}
			}
			TAILQ_INSERT_TAIL(&domain->mxs, mx, entry);
			return;

		case IMSG_DNS_HOST_END:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_int(&m, &dnserror);
			m_end(&m);
			domain = tree_xpop(&wait_mx, reqid);
			domain->mxstatus = dnserror;
			if (domain->mxstatus == DNS_OK) {
				log_debug("debug: MXs for domain %s:",
				    domain->name);
				TAILQ_FOREACH(mx, &domain->mxs, entry)
					log_debug("	%s preference %i",
					    sa_to_text(mx->host->sa),
					    mx->preference);
			}
			else {
				log_debug("debug: Failed MX query for %s:",
				    domain->name);
			}
			waitq_run(&domain->mxs, domain);
			return;

		case IMSG_DNS_MX_PREFERENCE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_int(&m, &dnserror);
			relay = tree_xpop(&wait_preference, reqid);
			if (dnserror) {
				log_debug("debug: couldn't find backup "
				    "preference for %s",
				    mta_relay_to_text(relay));
				relay->backuppref = INT_MAX;
			} else {
				m_get_int(&m, &relay->backuppref);
				log_debug("debug: found backup preference %i "
				    "for %s",
				    relay->backuppref,
				    mta_relay_to_text(relay));
			}
			m_end(&m);
			relay->status &= ~RELAY_WAIT_PREFERENCE;
			mta_drain(relay);
			mta_relay_unref(relay); /* from mta_query_preference() */
			return;

		case IMSG_DNS_PTR:
			mta_session_imsg(p, imsg);
			return;

		case IMSG_LKA_SSL_INIT:
			mta_session_imsg(p, imsg);
			return;

		case IMSG_LKA_SSL_VERIFY:
			mta_session_imsg(p, imsg);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			log_verbose(v);
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling = v;
			return;
		}
	}

	errx(1, "mta_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
mta_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mta_shutdown();
		break;
	default:
		fatalx("mta_sig_handler: unexpected signal");
	}
}

static void
mta_shutdown(void)
{
	log_info("info: mail transfer agent exiting");
	_exit(0);
}

pid_t
mta(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("mta: cannot fork");
	case 0:
		env->sc_pid = getpid();
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;
	if (chroot(pw->pw_dir) == -1)
		fatal("mta: chroot");
	if (chdir("/") == -1)
		fatal("mta: chdir(\"/\")");

	config_process(PROC_MTA);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");

	SPLAY_INIT(&relays);
	SPLAY_INIT(&domains);
	SPLAY_INIT(&hosts);
	SPLAY_INIT(&sources);
	SPLAY_INIT(&routes);

	tree_init(&batches);
	tree_init(&wait_secret);
	tree_init(&wait_mx);
	tree_init(&wait_preference);
	tree_init(&wait_source);

	imsg_callback = mta_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_PARENT);
	config_peer(PROC_QUEUE);
	config_peer(PROC_LKA);
	config_peer(PROC_CONTROL);
	config_done();

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mta_shutdown();

	return (0);
}

/*
 * Local error on the given source.
 */
void
mta_source_error(struct mta_relay *relay, struct mta_route *route, const char *e)
{
	struct mta_connector	*c;
	/*
	 * Remember the source as broken for this connector.
	 */
	c = mta_connector(relay, route->src);
	if (!(c->flags & CONNECTOR_SOURCE_ERROR))
		log_info("smtp-out: Error on connector %s: %s",
		    mta_connector_to_text(c), e);
	c->flags |= CONNECTOR_SOURCE_ERROR;
}

/*
 * TODO:
 * Currently all errors are reported on the host itself.  Technically,
 * it should depend on the error, and it would be probably better to report
 * it at the connector level.  But we would need to have persistent routes
 * for that.  Hosts are "naturally" persisted, as they are referenced from
 * the MX list on the domain.
 * Also, we need a timeout on that.
 */
void
mta_route_error(struct mta_relay *relay, struct mta_route *route)
{
	route->dst->nerror++;

	if (route->dst->flags & HOST_IGNORE)
		return;

	if (route->dst->nerror > MAXERROR_PER_HOST) {
		log_info("smtp-out: Too many errors on host %s: ignoring this MX",
		    mta_host_to_text(route->dst));
		route->dst->flags |= HOST_IGNORE;
	}
}

void
mta_route_ok(struct mta_relay *relay, struct mta_route *route)
{
	struct mta_connector	*c;

	log_debug("debug: mta: route ok %s", mta_route_to_text(route));

	route->dst->nerror = 0;

	c = mta_connector(relay, route->src);
	c->flags &= ~CONNECTOR_SOURCE_ERROR;
}

void
mta_route_collect(struct mta_relay *relay, struct mta_route *route)
{
	struct mta_connector	*c;

	log_debug("debug: mta: route collect %s", mta_route_to_text(route));

	relay->nconn -= 1;
	route->nconn -= 1;
	route->src->nconn -= 1;
	route->dst->nconn -= 1;

	c = mta_connector(relay, route->src);
	mta_route_unref(route); /* from mta_find_route() */

	c->nconn -= 1;

	if (c->flags & CONNECTOR_LIMIT) {
		log_debug("debug: mta; resetting limit flags on connector %s",
		    mta_connector_to_text(c));
		c->flags &= ~CONNECTOR_LIMIT;
	}

	mta_recycle(c);
	mta_drain(relay);
	mta_relay_unref(relay); /* from mta_connect() */
}

struct mta_task *
mta_route_next_task(struct mta_relay *relay, struct mta_route *route)
{
	struct mta_task	*task;

	if ((task = TAILQ_FIRST(&relay->tasks))) {
		TAILQ_REMOVE(&relay->tasks, task, entry);
		relay->ntask -= 1;
		task->relay = NULL;
	}

	return (task);
}

void
mta_delivery(struct mta_envelope *e, const char *relay, int delivery,
    const char *status)
{
	if (delivery == IMSG_DELIVERY_OK) {
		mta_log(e, "Ok", relay, status);
		queue_ok(e->id);
	}
	else if (delivery == IMSG_DELIVERY_TEMPFAIL) {
		mta_log(e, "TempFail", relay, status);
		queue_tempfail(e->id, status);
	}
	else if (delivery == IMSG_DELIVERY_PERMFAIL) {
		mta_log(e, "PermFail", relay, status);
		queue_permfail(e->id, status);
	}
	else if (delivery == IMSG_DELIVERY_LOOP) {
		mta_log(e, "PermFail", relay, "Loop detected");
		queue_loop(e->id);
	}
	else
		errx(1, "bad delivery");
}

static void
mta_query_mx(struct mta_relay *relay)
{
	uint64_t	id;

	if (relay->status & RELAY_WAIT_MX)
		return;

	log_debug("debug: mta_query_mx(%s)", relay->domain->name);

	if (waitq_wait(&relay->domain->mxs, mta_on_mx, relay)) {
		id = generate_uid();
		tree_xset(&wait_mx, id, relay->domain);
		if (relay->domain->flags)
			dns_query_host(id, relay->domain->name);
		else
			dns_query_mx(id, relay->domain->name);
		relay->domain->lastmxquery = time(NULL);
	}
	relay->status |= RELAY_WAIT_MX;
	mta_relay_ref(relay);
}

static void
mta_query_secret(struct mta_relay *relay)
{
	if (relay->status & RELAY_WAIT_SECRET)
		return;

	log_debug("debug: mta_query_secret(%s)", mta_relay_to_text(relay));

	tree_xset(&wait_secret, relay->id, relay);
	relay->status |= RELAY_WAIT_SECRET;

	m_create(p_lka, IMSG_LKA_SECRET, 0, 0, -1, 128);
	m_add_id(p_lka, relay->id);
	m_add_string(p_lka, relay->authtable);
	m_add_string(p_lka, relay->authlabel);
	m_close(p_lka);

	mta_relay_ref(relay);
}

static void
mta_query_preference(struct mta_relay *relay)
{
	if (relay->status & RELAY_WAIT_PREFERENCE)
		return;

	log_debug("debug: mta_query_preference(%s)", mta_relay_to_text(relay));

	tree_xset(&wait_preference, relay->id, relay);
	relay->status |= RELAY_WAIT_PREFERENCE;
	dns_query_mx_preference(relay->id, relay->domain->name,
		relay->backupname);
	mta_relay_ref(relay);
}

static void
mta_query_source(struct mta_relay *relay)
{
	log_debug("debug: mta_query_source(%s)", mta_relay_to_text(relay));

	m_create(p_lka, IMSG_LKA_SOURCE, 0, 0, -1, 64);
	m_add_id(p_lka, relay->id);
	m_add_string(p_lka, relay->sourcetable);
	m_close(p_lka);

	tree_xset(&wait_source, relay->id, relay);
	relay->status |= RELAY_WAIT_SOURCE;
	mta_relay_ref(relay);
}

static void
mta_on_mx(void *tag, void *arg, void *data)
{
	struct mta_domain	*domain = data;
	struct mta_relay	*relay = arg;

	log_debug("debug: mta_on_mx(%p, %s, %s)",
	    tag, domain->name, mta_relay_to_text(relay));

	switch (domain->mxstatus) {
	case DNS_OK:
		break;
	case DNS_RETRY:
		relay->fail = IMSG_DELIVERY_TEMPFAIL;
		relay->failstr = "Temporary failure in MX lookup";
		break;
	case DNS_EINVAL:
		relay->fail = IMSG_DELIVERY_PERMFAIL;
		relay->failstr = "Invalid domain name";
		break;
	case DNS_ENONAME:
		relay->fail = IMSG_DELIVERY_PERMFAIL;
		relay->failstr = "Domain does not exist";
		break;
	case DNS_ENOTFOUND:
		relay->fail = IMSG_DELIVERY_TEMPFAIL;
		relay->failstr = "No MX found for domain";
		break;
	default:
		fatalx("bad DNS lookup error code");
		break;
	}

	if (domain->mxstatus)
		log_info("smtp-out: Failed to resolve MX for %s: %s",
		    mta_relay_to_text(relay), relay->failstr);

	relay->status &= ~RELAY_WAIT_MX;
	mta_drain(relay);
	mta_relay_unref(relay); /* from mta_drain() */
}

static void
mta_on_source(struct mta_relay *relay, struct mta_source *source)
{
	mta_connector(relay, source);
}

static void
mta_connect(struct mta_connector *c)
{
	struct mta_route	*route;

	log_debug("debug: mta_connect() for %s", mta_connector_to_text(c));

	route = mta_find_route(c);
	if (route == NULL) {
		mta_recycle(c);
		if (c->queue == &c->relay->c_limit)
			c->clearlimit = time(NULL) + CONNECTOR_DELAY_LIMIT;
		if (c->queue == &c->relay->c_ready)
			fatalx("connector with no route ended up in ready list");
		return;
	}

	c->nconn += 1;
	c->lastconn = time(NULL);
	c->nextconn = c->lastconn + CONNECTOR_DELAY_CONNECT;

	c->relay->nconn += 1;
	c->relay->lastconn = c->lastconn;
	route->nconn += 1;
	route->lastconn = c->lastconn;
	route->src->nconn += 1;
	route->src->lastconn = c->lastconn;
	route->dst->nconn += 1;
	route->dst->lastconn = c->lastconn;

	mta_recycle(c);

	mta_relay_ref(c->relay);
	mta_session(c->relay, route);	/* this never fails synchronously */
}

static void
mta_recycle(struct mta_connector *c)
{
	TAILQ_REMOVE(c->queue, c, lst_entry);

	if (c->flags & CONNECTOR_ERROR) {
		log_debug("debug: mta: putting %s on error queue",
		    mta_connector_to_text(c));
		c->queue = &c->relay->c_error;
	}
	else if (c->flags & CONNECTOR_LIMIT) {
		log_debug("debug: mta: putting %s on limit queue",
		    mta_connector_to_text(c));
		c->queue = &c->relay->c_limit;
	}
	else if (c->nextconn > time(NULL)) {
		log_debug("debug: mta: putting %s on delay queue",
		    mta_connector_to_text(c));
		c->queue = &c->relay->c_delay;
	}
	else {
		log_debug("debug: mta: putting %s on ready queue",
		    mta_connector_to_text(c));
		c->queue = &c->relay->c_ready;
	}

	TAILQ_INSERT_TAIL(c->queue, c, lst_entry);
}

static void
mta_relay_timeout(int fd, short ev, void *arg)
{
	struct mta_relay	*r = arg;
	struct mta_connector	*c;
	time_t			 t;

	log_debug("debug: mta: timeout for %s", mta_relay_to_text(r));

	t = time(NULL);

	/*
	 * Clear the limit flags on all connectors.
	 */
	while ((c = TAILQ_FIRST(&r->c_limit))) {
		/* This requires that the list is always sorted */
		if (c->clearlimit > t)
			break;
		log_debug("debug: mta: clearing limits on %s",
		    mta_connector_to_text(c));
		c->flags &= ~CONNECTOR_LIMIT;
		mta_recycle(c);
	}

	while ((c = TAILQ_FIRST(&r->c_delay))) {
		/* This requires that the list is always sorted */
		if (c->nextconn > t)
			break;
		log_debug("debug: mta: delay expired for %s",
		    mta_connector_to_text(c));
		mta_recycle(c);
	}

	mta_drain(r);
	mta_relay_unref(r); /* from mta_relay_schedule() */
}

static void
mta_relay_schedule(struct mta_relay *r, unsigned int delay)
{
	struct timeval	tv;

	if (evtimer_pending(&r->ev, &tv))
		return;

	log_debug("debug: mta: adding relay timeout: %u", delay);

	tv.tv_sec = delay;
	tv.tv_usec = 0;
	evtimer_add(&r->ev, &tv);
	mta_relay_ref(r);
}

static void
mta_drain(struct mta_relay *r)
{
	struct mta_connector	*c;
	struct mta_source	*s;
	char			 buf[64];

	log_debug("debug: mta: draining %s "
	    "refcount=%i, ntask=%zu, nconnector=%zu, nconn=%zu", 
	    mta_relay_to_text(r),
	    r->refcount, r->ntask, r->nconnector, r->nconn);

	/*
	 * All done.
	 */
	if (r->ntask == 0) {
		log_debug("debug: mta: all done for %s", mta_relay_to_text(r));
		return;
	}

	/*
	 * If we know that this relay is failing flush the tasks.
	 */
	if (r->fail) {
		mta_flush(r, r->fail, r->failstr);
		return;
	}

	/* Query secret if needed. */
	if (r->flags & RELAY_AUTH && r->secret == NULL)
		mta_query_secret(r);

	/* Query our preference if needed. */
	if (r->backupname && r->backuppref == -1)
		mta_query_preference(r);

	/* Query the domain MXs if needed. */
	if (r->domain->lastmxquery == 0)
		mta_query_mx(r);

	/* Wait until we are ready to proceed. */
	if (r->status & RELAY_WAITMASK) {
		buf[0] = '\0';
		if (r->status & RELAY_WAIT_MX)
			strlcat(buf, "MX ", sizeof buf);
		if (r->status & RELAY_WAIT_PREFERENCE)
			strlcat(buf, "preference ", sizeof buf);
		if (r->status & RELAY_WAIT_SECRET)
			strlcat(buf, "secret ", sizeof buf);
		if (r->status & RELAY_WAIT_SOURCE)
			strlcat(buf, "source ", sizeof buf);
		if (r->status & RELAY_WAIT_HELO)
			strlcat(buf, "helo ", sizeof buf);
		log_debug("debug: mta: %s waiting for %s",
		    mta_relay_to_text(r), buf);
		return;
	}

	/*
	 * Start new connections if possible.
	 * XXX find a better heuristic for the good number of connections
	 * depending on the number of tasks and other factors.  We might
	 * want to try more than the number of task, to have a chance to
	 * hit a mx faster if the first ones timeout.
	 */
	while (r->nconn < r->ntask) {
		log_debug("debug: mta: trying to create new connection: "
		    "refcount=%i, ntask=%zu, nconnector=%zu, nconn=%zu", 
		    r->refcount, r->ntask, r->nconnector, r->nconn);

		/* Check the per-relay connection limit */
		if (r->nconn >= MAXCONN_PER_RELAY) {
			log_debug("debug: mta: hit connection limit on %s",
			    mta_relay_to_text(r));
			return;
		}

		/* Use the first connector if ready */
		c = TAILQ_FIRST(&r->c_ready);
		if (c) {
			log_debug("debug: mta: using connector %s",
			    mta_connector_to_text(c));
			r->sourceloop = 0;
			mta_connect(c);
			continue;
		}

		/* No new connectors */
		if (r->sourceloop > r->nconnector) {
			log_debug("debug: mta: no new connector available");

			if (TAILQ_FIRST(&r->c_delay)) {
				mta_relay_schedule(r, 1);
				log_debug(
				    "debug: mta: waiting for relay timeout");
				return;
			}

			if (TAILQ_FIRST(&r->c_limit)) {
				mta_relay_schedule(r, 5);
				log_debug(
				    "debug: mta: waiting for relay timeout");
				return;
			}

			log_debug("debug: mta: failing...");
			/*
			 * All sources have been tried and no connectors can
			 * be used.
			 */
			if (r->nconnector == 0) {
				r->fail = IMSG_DELIVERY_TEMPFAIL;
				r->failstr = "No source address";
			}
			else {
				r->fail = IMSG_DELIVERY_TEMPFAIL;
				r->failstr = "No MX could be reached";
			}
			mta_flush(r, r->fail, r->failstr);
			return;
		}

		r->sourceloop++;
		log_debug("debug: mta: need new connector (attempt %zu)",
			r->sourceloop);
		if (r->sourcetable) {
			log_debug("debug: mta: querying source %s",
			    r->sourcetable);
			mta_query_source(r);
			return;
		}
		log_debug("debug: mta: using default source");
		s = mta_source(NULL);
		mta_on_source(r, s);
		mta_source_unref(s);
	}
}

static void
mta_flush(struct mta_relay *relay, int fail, const char *error)
{
	struct mta_envelope	*e;
	struct mta_task		*task;
	const char		*pfx;
	size_t			 n;

	log_debug("debug: mta_flush(%s, %i, \"%s\")",
	    mta_relay_to_text(relay), fail, error);

	if (fail == IMSG_DELIVERY_TEMPFAIL)
		pfx = "TempFail";
	else if (fail == IMSG_DELIVERY_PERMFAIL)
		pfx = "PermFail";
	else
		errx(1, "unexpected delivery status %i", fail);

	n = 0;
	while ((task = TAILQ_FIRST(&relay->tasks))) {
		TAILQ_REMOVE(&relay->tasks, task, entry);
		while ((e = TAILQ_FIRST(&task->envelopes))) {
			TAILQ_REMOVE(&task->envelopes, e, entry);
			mta_delivery(e, relay->domain->name, fail, error);
			free(e->dest);
			free(e->rcpt);
			free(e);
			n++;
		}
		free(task->sender);
		free(task);
	}

	stat_decrement("mta.task", relay->ntask);
	stat_decrement("mta.envelope", n);
	relay->ntask = 0;
}

/*
 * Find a route to use for this connector
 */
static struct mta_route *
mta_find_route(struct mta_connector *c)
{
	struct mta_route	*route, *best;
	struct mta_mx		*mx;
	int			 level, limit_host, limit_route;
	int			 family_mismatch, seen;

	limit_host = 0;
	limit_route = 0;
	family_mismatch = 0;
	level = -1;
	best = NULL;
	seen = 0;

	if (c->nconn >= MAXCONN_PER_CONNECTOR) {
		log_debug("debug: mta: hit limit on connector %s",
		    mta_connector_to_text(c));
		c->flags |= CONNECTOR_LIMIT_SOURCE;
		return (NULL);
	}

	TAILQ_FOREACH(mx, &c->relay->domain->mxs, entry) {
		/*
		 * New preference level
		 */		
		if (mx->preference > level) {
#ifndef IGNORE_MX_PREFERENCE
			/*
			 * Use the current best MX if found.
			 */
			if (best)
				break;

			/*
			 * No candidate found.  There are valid MXs at this
			 * preference level but they reached their limit.
			 */
			if (limit_host || limit_route)
				break;

			/*
			 *  If we are a backup MX, do not relay to MXs with
			 *  a greater preference value.
			 */
			if (c->relay->backuppref >= 0 &&
			    mx->preference >= c->relay->backuppref)
				break;

			/*
			 * Start looking at MXs on this preference level.
			 */ 
#endif
			level = mx->preference;
		}

		if (mx->host->flags & HOST_IGNORE)
			continue;

		/* Found a possibly valid mx */
		seen++;

		if (c->source->sa &&
		    c->source->sa->sa_family != mx->host->sa->sa_family) {
			family_mismatch = 1;
			continue;
		}

		if (mx->host->nconn >= MAXCONN_PER_HOST) {
			limit_host = 1;
			continue;
		}

		route = mta_route(c->source, mx->host);

		if (route->nconn >= MAXCONN_PER_ROUTE) {
			limit_route = 1;
			mta_route_unref(route); /* from here */
			continue;
		}

		/* Use the route with the lowest number of connections. */
		if (best && route->nconn >= best->nconn) {
			mta_route_unref(route); /* from here */
			continue;
		}

		if (best)
			mta_route_unref(best); /* from here */
		best = route;
	}

	if (best)
		return (best);

	if (seen == 0) {
		log_info("smtp-out: No reachable MX for connector %s",
		    mta_connector_to_text(c));
		c->flags |= CONNECTOR_MX_ERROR;
	}
	else if (family_mismatch) {
		log_info("smtp-out: Address family mismatch on connector %s",
		    mta_connector_to_text(c));
		c->flags |= CONNECTOR_FAMILY_ERROR;
	}
	else if (limit_route) {
		log_debug("debug: mta: hit route limit on connector %s",
		    mta_connector_to_text(c));
		c->flags |= CONNECTOR_LIMIT_ROUTE;
	}
	else if (limit_host) {
		log_debug("debug: mta: hit host limit on connector %s",
		    mta_connector_to_text(c));
		c->flags |= CONNECTOR_LIMIT_HOST;
	}

	return (NULL);
}

static void
mta_log(const struct mta_envelope *evp, const char *prefix, const char *relay,
    const char *status)
{
	char rcpt[MAX_LINE_SIZE];

	rcpt[0] = '\0';
	if (evp->rcpt)
		snprintf(rcpt, sizeof rcpt, "rcpt=<%s>, ", evp->rcpt);

	log_info("relay: %s for %016" PRIx64 ": from=<%s>, to=<%s>, "
	    "%srelay=%s delay=%s, stat=%s",
	    prefix,
	    evp->id,
	    evp->task->sender,
	    evp->dest,
	    rcpt,
	    relay,
	    duration_to_text(time(NULL) - evp->creation),
	    status);
}

static struct mta_relay *
mta_relay(struct envelope *e)
{
	struct mta_relay	 key, *r;

	bzero(&key, sizeof key);

	if (e->agent.mta.relay.flags & RELAY_BACKUP) {
		key.domain = mta_domain(e->dest.domain, 0);
		key.backupname = e->agent.mta.relay.hostname;
	} else if (e->agent.mta.relay.hostname[0]) {
		key.domain = mta_domain(e->agent.mta.relay.hostname, 1);
		key.flags |= RELAY_MX;
	} else {
		key.domain = mta_domain(e->dest.domain, 0);
	}

	key.flags = e->agent.mta.relay.flags;
	key.port = e->agent.mta.relay.port;
	key.cert = e->agent.mta.relay.cert;
	if (!key.cert[0])
		key.cert = NULL;
	key.authtable = e->agent.mta.relay.authtable;
	if (!key.authtable[0])
		key.authtable = NULL;
	key.authlabel = e->agent.mta.relay.authlabel;
	if (!key.authlabel[0])
		key.authlabel = NULL;
	key.sourcetable = e->agent.mta.relay.sourcetable;
	if (!key.sourcetable[0])
		key.sourcetable = NULL;
	key.helotable = e->agent.mta.relay.helotable;
	if (!key.helotable[0])
		key.helotable = NULL;

	if ((r = SPLAY_FIND(mta_relay_tree, &relays, &key)) == NULL) {
		r = xcalloc(1, sizeof *r, "mta_relay");
		TAILQ_INIT(&r->tasks);
		TAILQ_INIT(&r->c_ready);
		TAILQ_INIT(&r->c_delay);
		TAILQ_INIT(&r->c_limit);
		TAILQ_INIT(&r->c_error);
		r->id = generate_uid();
		r->flags = key.flags;
		r->domain = key.domain;
		r->backupname = key.backupname ?
		    xstrdup(key.backupname, "mta: backupname") : NULL;
		r->backuppref = -1;
		r->port = key.port;
		r->cert = key.cert ? xstrdup(key.cert, "mta: cert") : NULL;
		if (key.authtable)
			r->authtable = xstrdup(key.authtable, "mta: authtable");
		if (key.authlabel)
			r->authlabel = xstrdup(key.authlabel, "mta: authlabel");
		if (key.sourcetable)
			r->sourcetable = xstrdup(key.sourcetable,
			    "mta: sourcetable");
		if (key.helotable)
			r->helotable = xstrdup(key.helotable,
			    "mta: helotable");
		SPLAY_INSERT(mta_relay_tree, &relays, r);
		evtimer_set(&r->ev, mta_relay_timeout, r);
		log_trace(TRACE_MTA, "mta: new %s", mta_relay_to_text(r));
		stat_increment("mta.relay", 1);
	} else {
		mta_domain_unref(key.domain); /* from here */
		log_trace(TRACE_MTA, "mta: reusing %s", mta_relay_to_text(r));
	}

	r->refcount++;
	return (r);
}

static void
mta_relay_ref(struct mta_relay *r)
{
	r->refcount++;
}

static void
mta_relay_unref(struct mta_relay *relay)
{
	struct mta_connector	*c;

	if (--relay->refcount)
		return;

	log_debug("debug: mta: freeing %s", mta_relay_to_text(relay));
	SPLAY_REMOVE(mta_relay_tree, &relays, relay);
	if (relay->cert)
		free(relay->cert);
	if (relay->authtable)
		free(relay->authtable);
	if (relay->authlabel)
		free(relay->authlabel);

	while ((tree_poproot(&relay->connectors, NULL, (void**)&c)))
		mta_connector_free(c);

	if (evtimer_pending(&relay->ev, NULL))
		evtimer_del(&relay->ev);

	mta_domain_unref(relay->domain); /* from constructor */
	free(relay);
	stat_decrement("mta.relay", 1);
}

const char *
mta_relay_to_text(struct mta_relay *relay)
{
	static char	 buf[1024];
	char		 tmp[32];
	const char	*sep = ",";

	snprintf(buf, sizeof buf, "[relay:%s", relay->domain->name);

	if (relay->port) {
		strlcat(buf, sep, sizeof buf);
		snprintf(tmp, sizeof tmp, "port=%i", (int)relay->port);
		strlcat(buf, tmp, sizeof buf);
	}

	if (relay->flags & RELAY_STARTTLS) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "starttls", sizeof buf);
	}

	if (relay->flags & RELAY_SMTPS) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "smtps", sizeof buf);
	}

	if (relay->flags & RELAY_AUTH) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "auth=", sizeof buf);
		strlcat(buf, relay->authtable, sizeof buf);
		strlcat(buf, ":", sizeof buf);
		strlcat(buf, relay->authlabel, sizeof buf);
	}

	if (relay->cert) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "cert=", sizeof buf);
		strlcat(buf, relay->cert, sizeof buf);
	}

	if (relay->flags & RELAY_MX) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "mx", sizeof buf);
	}

	if (relay->flags & RELAY_BACKUP) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "backup=", sizeof buf);
		strlcat(buf, relay->backupname, sizeof buf);
	}

	if (relay->sourcetable) {
		strlcat(buf, sep, sizeof buf);
		strlcat(buf, "sourcetable=", sizeof buf);
		strlcat(buf, relay->sourcetable, sizeof buf);
	}

	strlcat(buf, "]", sizeof buf);

	return (buf);
}

static int
mta_relay_cmp(const struct mta_relay *a, const struct mta_relay *b)
{
	int	r;

	if (a->domain < b->domain)
		return (-1);
	if (a->domain > b->domain)
		return (1);

	if (a->flags < b->flags)
		return (-1);
	if (a->flags > b->flags)
		return (1);

	if (a->port < b->port)
		return (-1);
	if (a->port > b->port)
		return (1);

	if (a->authtable == NULL && b->authtable)
		return (-1);
	if (a->authtable && b->authtable == NULL)
		return (1);
	if (a->authtable && ((r = strcmp(a->authtable, b->authtable))))
		return (r);
	if (a->authlabel && ((r = strcmp(a->authlabel, b->authlabel))))
		return (r);
	if (a->sourcetable == NULL && b->sourcetable)
		return (-1);
	if (a->sourcetable && b->sourcetable == NULL)
		return (1);
	if (a->sourcetable && ((r = strcmp(a->sourcetable, b->sourcetable))))
		return (r);

	if (a->cert == NULL && b->cert)
		return (-1);
	if (a->cert && b->cert == NULL)
		return (1);
	if (a->cert && ((r = strcmp(a->cert, b->cert))))
		return (r);

	if (a->backupname && ((r = strcmp(a->backupname, b->backupname))))
		return (r);

	return (0);
}

SPLAY_GENERATE(mta_relay_tree, mta_relay, entry, mta_relay_cmp);

static struct mta_host *
mta_host(const struct sockaddr *sa)
{
	struct mta_host		key, *h;
	struct sockaddr_storage	ss;

	memmove(&ss, sa, sa->sa_len);
	key.sa = (struct sockaddr*)&ss;
	h = SPLAY_FIND(mta_host_tree, &hosts, &key);

	if (h == NULL) {
		h = xcalloc(1, sizeof(*h), "mta_host");
		h->sa = xmemdup(sa, sa->sa_len, "mta_host");
		SPLAY_INSERT(mta_host_tree, &hosts, h);
		stat_increment("mta.host", 1);
	}

	h->refcount++;
	return (h);
}

static void
mta_host_ref(struct mta_host *h)
{
	h->refcount++;
}

static void
mta_host_unref(struct mta_host *h)
{
	if (--h->refcount)
		return;

	SPLAY_REMOVE(mta_host_tree, &hosts, h);
	free(h->sa);
	free(h->ptrname);
	stat_decrement("mta.host", 1);
}

const char *
mta_host_to_text(struct mta_host *h)
{
	static char buf[1024];

	if (h->ptrname)
		snprintf(buf, sizeof buf, "%s (%s)",
		    sa_to_text(h->sa), h->ptrname);
	else
		snprintf(buf, sizeof buf, "%s", sa_to_text(h->sa));

	return (buf);
}

static int
mta_host_cmp(const struct mta_host *a, const struct mta_host *b)
{
	if (a->sa->sa_len < b->sa->sa_len)
		return (-1);
	if (a->sa->sa_len > b->sa->sa_len)
		return (1);
	return (memcmp(a->sa, b->sa, a->sa->sa_len));
}

SPLAY_GENERATE(mta_host_tree, mta_host, entry, mta_host_cmp);

static struct mta_domain *
mta_domain(char *name, int flags)
{
	struct mta_domain	key, *d;

	key.name = name;
	key.flags = flags;
	d = SPLAY_FIND(mta_domain_tree, &domains, &key);

	if (d == NULL) {
		d = xcalloc(1, sizeof(*d), "mta_domain");
		d->name = xstrdup(name, "mta_domain");
		d->flags = flags;
		TAILQ_INIT(&d->mxs);
		SPLAY_INSERT(mta_domain_tree, &domains, d);
		stat_increment("mta.domain", 1);
	}

	d->refcount++;
	return (d);
}

#if 0
static void
mta_domain_ref(struct mta_domain *d)
{
	d->refcount++;
}
#endif

static void
mta_domain_unref(struct mta_domain *d)
{
	struct mta_mx	*mx;

	if (--d->refcount)
		return;

	while ((mx = TAILQ_FIRST(&d->mxs))) {
		TAILQ_REMOVE(&d->mxs, mx, entry);
		mta_host_unref(mx->host); /* from IMSG_DNS_HOST */
		free(mx);
	}

	SPLAY_REMOVE(mta_domain_tree, &domains, d);
	free(d->name);
	stat_decrement("mta.domain", 1);
}

static int
mta_domain_cmp(const struct mta_domain *a, const struct mta_domain *b)
{
	if (a->flags < b->flags)
		return (-1);
	if (a->flags > b->flags)
		return (1);
	return (strcasecmp(a->name, b->name));
}

SPLAY_GENERATE(mta_domain_tree, mta_domain, entry, mta_domain_cmp);

static struct mta_source *
mta_source(const struct sockaddr *sa)
{
	struct mta_source	key, *s;
	struct sockaddr_storage	ss;

	if (sa) {
		memmove(&ss, sa, sa->sa_len);
		key.sa = (struct sockaddr*)&ss;
	} else
		key.sa = NULL;
	s = SPLAY_FIND(mta_source_tree, &sources, &key);

	if (s == NULL) {
		s = xcalloc(1, sizeof(*s), "mta_source");
		if (sa)
			s->sa = xmemdup(sa, sa->sa_len, "mta_source");
		SPLAY_INSERT(mta_source_tree, &sources, s);
		stat_increment("mta.source", 1);
	}

	s->refcount++;
	return (s);
}

static void
mta_source_ref(struct mta_source *s)
{
	s->refcount++;
}

static void
mta_source_unref(struct mta_source *s)
{
	if (--s->refcount)
		return;

	SPLAY_REMOVE(mta_source_tree, &sources, s);
	free(s->sa);
	stat_decrement("mta.source", 1);
}

static const char *
mta_source_to_text(struct mta_source *s)
{
	static char buf[1024];

	if (s->sa == NULL)
		return "[]";
	snprintf(buf, sizeof buf, "%s", sa_to_text(s->sa));
	return (buf);
}

static int
mta_source_cmp(const struct mta_source *a, const struct mta_source *b)
{
	if (a->sa == NULL)
		return ((b->sa == NULL) ? 0 : -1);
	if (b->sa == NULL)
		return (1);
	if (a->sa->sa_len < b->sa->sa_len)
		return (-1);
	if (a->sa->sa_len > b->sa->sa_len)
		return (1);
	return (memcmp(a->sa, b->sa, a->sa->sa_len));
}

SPLAY_GENERATE(mta_source_tree, mta_source, entry, mta_source_cmp);

static struct mta_connector *
mta_connector(struct mta_relay *relay, struct mta_source *source)
{
	struct mta_connector	*c;

	c = tree_get(&relay->connectors, (uintptr_t)(source));
	if (c == NULL) {
		c = xcalloc(1, sizeof(*c), "mta_connector");
		c->relay = relay;
		c->source = source;
		mta_source_ref(source);
		c->queue = &relay->c_ready;
		TAILQ_INSERT_HEAD(c->queue, c, lst_entry);
		tree_xset(&relay->connectors, (uintptr_t)(source), c);
		relay->nconnector++;
		stat_increment("mta.connector", 1);
		log_debug("debug: mta: new connector %s",
		    mta_connector_to_text(c));
	}

	return (c);
}

static void
mta_connector_free(struct mta_connector *c)
{
	c->relay->nconnector--;
	TAILQ_REMOVE(c->queue, c, lst_entry);
	mta_source_unref(c->source);
	stat_decrement("mta.connector", 1);
}

static const char *
mta_connector_to_text(struct mta_connector *c)
{
	static char buf[1024];

	snprintf(buf, sizeof buf, "%s->%s",
	    mta_source_to_text(c->source),
	    mta_relay_to_text(c->relay));
	return (buf);
}

static struct mta_route *
mta_route(struct mta_source *src, struct mta_host *dst)
{
	struct mta_route	key, *r;

	key.src = src;
	key.dst = dst;
	r = SPLAY_FIND(mta_route_tree, &routes, &key);

	if (r == NULL) {
		r = xcalloc(1, sizeof(*r), "mta_route");
		r->src = src;
		r->dst = dst;
		SPLAY_INSERT(mta_route_tree, &routes, r);
		mta_source_ref(src);
		mta_host_ref(dst);
		stat_increment("mta.route", 1);
	}

	r->refcount++;
	return (r);
}

#if 0
static void
mta_route_ref(struct mta_route *r)
{
	r->refcount++;
}
#endif

static void
mta_route_unref(struct mta_route *r)
{
	if (--r->refcount)
		return;

	SPLAY_REMOVE(mta_route_tree, &routes, r);
	mta_source_unref(r->src); /* from constructor */
	mta_host_unref(r->dst); /* from constructor */
	stat_decrement("mta.route", 1);
}

static const char *
mta_route_to_text(struct mta_route *r)
{
	static char	buf[1024];

	snprintf(buf, sizeof buf, "%s <-> %s",
	    mta_source_to_text(r->src),
	    mta_host_to_text(r->dst));

	return (buf);
}

static int
mta_route_cmp(const struct mta_route *a, const struct mta_route *b)
{
	if (a->src < b->src)
		return (-1);
	if (a->src > b->src)
		return (1);

	if (a->dst < b->dst)
		return (-1);
	if (a->dst > b->dst)
		return (1);

	return (0);
}

SPLAY_GENERATE(mta_route_tree, mta_route, entry, mta_route_cmp);
