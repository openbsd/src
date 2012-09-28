/*	$OpenBSD: mta.c,v 1.144 2012/09/28 14:03:00 chl Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#define MTA_MAXCONN	5	/* connections per route */
#define MTA_MAXMAIL	100	/* mails per session     */
#define MTA_MAXRCPT	1000	/* rcpt per mail         */

struct mta_batch2 {
	uint64_t	id;
	struct tree	tasks;		/* map route to task */
};

SPLAY_HEAD(mta_route_tree, mta_route);

static void mta_imsg(struct imsgev *, struct imsg *);
static void mta_shutdown(void);
static void mta_sig_handler(int, short, void *);

static struct mta_route *mta_route_for(struct envelope *);
static void mta_route_drain(struct mta_route *);
static void mta_route_free(struct mta_route *);
static void mta_envelope_done(struct mta_task *, struct envelope *, const char *);
static int mta_route_cmp(struct mta_route *, struct mta_route *);

SPLAY_PROTOTYPE(mta_route_tree, mta_route, entry, mta_route_cmp);

static struct mta_route_tree routes = SPLAY_INITIALIZER(&routes);
static struct tree batches = SPLAY_INITIALIZER(&batches);

void
mta_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct mta_route	*route;
	struct mta_batch2	*batch;
	struct mta_task		*task;
	struct envelope		*e;
	struct ssl		*ssl;
	uint64_t		 id;

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {

		case IMSG_BATCH_CREATE:
			id = *(uint64_t*)(imsg->data);
			batch = xmalloc(sizeof *batch, "mta_batch");
			batch->id = id;
			tree_init(&batch->tasks);
			tree_xset(&batches, batch->id, batch);
			log_trace(TRACE_MTA,
			    "mta: batch:%016" PRIx64 " created", batch->id);
			return;

		case IMSG_BATCH_APPEND:
			e = xmemdup(imsg->data, sizeof *e, "mta:envelope");
			route = mta_route_for(e);
			batch = tree_xget(&batches, e->batch_id);

			if ((task = tree_get(&batch->tasks, route->id)) == NULL) {
				log_trace(TRACE_MTA, "mta: new task for %s",
				    mta_route_to_text(route));
				task = xmalloc(sizeof *task, "mta_task");
				TAILQ_INIT(&task->envelopes);
				task->route = route;
				tree_xset(&batch->tasks, route->id, task);
				task->msgid = evpid_to_msgid(e->id);
				task->sender = e->sender;
				route->refcount += 1;
			}

			/* Technically, we could handle that by adding a msg
			 * level, but the batch sent by the scheduler should
			 * be valid.
			 */
			if (task->msgid != evpid_to_msgid(e->id))
				errx(1, "msgid mismatch in batch");

			/* XXX honour route->maxrcpt */
			TAILQ_INSERT_TAIL(&task->envelopes, e, entry);
			stat_increment("mta.envelope", 1);
			log_debug("mta: received evp:%016" PRIx64 " for <%s@%s>",
			    e->id, e->dest.user, e->dest.domain);
			return;

		case IMSG_BATCH_CLOSE:
			id = *(uint64_t*)(imsg->data);
			batch = tree_xpop(&batches, id);
			log_trace(TRACE_MTA, "mta: batch:%016" PRIx64 " closed",
			    batch->id);
			/* for all tasks, queue them on there route */
			while (tree_poproot(&batch->tasks, &id, (void**)&task)) {
				if (id != task->route->id)
					errx(1, "route id mismatch!");
				task->route->refcount -= 1;
				task->route->ntask += 1;
				TAILQ_INSERT_TAIL(&task->route->tasks, task, entry);
				stat_increment("mta.task", 1);
				mta_route_drain(task->route);
			}
			free(batch);
			return;

		case IMSG_QUEUE_MESSAGE_FD:
			mta_session_imsg(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET:
		case IMSG_DNS_HOST:
		case IMSG_DNS_HOST_END:
		case IMSG_DNS_PTR:
			mta_session_imsg(iev, imsg);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_ssl = xcalloc(1, sizeof *env->sc_ssl, "mta:sc_ssl");
			return;

		case IMSG_CONF_SSL:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			ssl = xmemdup(imsg->data, sizeof *ssl, "mta:ssl");
			ssl->ssl_cert = xstrdup((char*)imsg->data + sizeof *ssl,
			    "mta:ssl_cert");
			ssl->ssl_key = xstrdup((char*)imsg->data +
			    sizeof *ssl + ssl->ssl_cert_len, "mta:ssl_key");
			SPLAY_INSERT(ssltree, env->sc_ssl, ssl);
			return;

		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			env->sc_flags &= ~SMTPD_CONFIGURING;
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
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
	log_info("mail transfer agent exiting");
	_exit(0);
}

pid_t
mta(void)
{
	pid_t		 pid;

	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mta: cannot fork");
	case 0:
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

	smtpd_process = PROC_MTA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mta: cannot drop privileges");

	imsg_callback = mta_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mta_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mta_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mta_shutdown();

	return (0);
}

const char *
mta_response_status(const char *r)
{
	switch (r[0]) {
	case '2':
		return "Sent";
	case '4':
	case '5':
		return "RemoteError";
	default:
		return "LocalError";
	}
}

int
mta_response_delivery(const char *r)
{
	switch (r[0]) {
	case '2':
		return IMSG_QUEUE_DELIVERY_OK;
	case '5':
	case '6':
		if (r[1] == '4' && r[2] == '6')
			return IMSG_QUEUE_DELIVERY_LOOP;
		return IMSG_QUEUE_DELIVERY_PERMFAIL;
	default:
		return IMSG_QUEUE_DELIVERY_TEMPFAIL;
	}
}

const char *
mta_response_text(const char *r)
{
	return (r + 4);
}

void
mta_route_error(struct mta_route *route, const char *error)
{
	route->nfail += 1;
	strlcpy(route->errorline, error, sizeof route->errorline);
	log_warnx("mta: %s error: %s", mta_route_to_text(route), error);
}

void
mta_route_ok(struct mta_route *route)
{
	log_debug("mta: %s ready", mta_route_to_text(route));
	route->nfail = 0;
}

void
mta_route_collect(struct mta_route *route)
{
	route->nsession -= 1;

	mta_route_drain(route);
}

const char *
mta_route_to_text(struct mta_route *route)
{
	static char	 buf[1024];
	const char	*sep = "";

	buf[0] = '\0';

	snprintf(buf, sizeof buf, "route:%s[", route->hostname);

	if (route->flags & ROUTE_STARTTLS) {
		sep = ",";
		strlcat(buf, "starttls", sizeof buf);
	}

	if (route->flags & ROUTE_SMTPS) {
		strlcat(buf, sep, sizeof buf);
		sep = ",";
		strlcat(buf, "smtps", sizeof buf);
	}

	if (route->flags & ROUTE_AUTH) {
		strlcat(buf, sep, sizeof buf);
		sep = ",";
		strlcat(buf, "auth=", sizeof buf);
		strlcat(buf, route->auth, sizeof buf);
	}

	if (route->cert) {
		strlcat(buf, sep, sizeof buf);
		sep = ",";
		strlcat(buf, "cert=", sizeof buf);
		strlcat(buf, route->cert, sizeof buf);
	}

	if (route->flags & ROUTE_MX) {
		strlcat(buf, sep, sizeof buf);
		sep = ",";
		strlcat(buf, "mx", sizeof buf);
	}

	if (route->flags & ROUTE_BACKUP) {
		strlcat(buf, sep, sizeof buf);
		sep = ",";
		strlcat(buf, "backup=", sizeof buf);
		strlcat(buf, route->backupname, sizeof buf);
	}

	strlcat(buf, "]", sizeof buf);

	return (buf);
}

static struct mta_route *
mta_route_for(struct envelope *e)
{
	struct ssl		ssl;
	struct mta_route	key, *route;

	bzero(&key, sizeof key);

	key.flags = e->agent.mta.relay.flags;
	if (e->agent.mta.relay.flags & ROUTE_BACKUP) {
		key.hostname = e->dest.domain;
		key.backupname = e->agent.mta.relay.hostname;
	} else if (e->agent.mta.relay.hostname[0]) {
		key.hostname = e->agent.mta.relay.hostname;
		key.flags |= ROUTE_MX;
	} else
		key.hostname = e->dest.domain;
	key.port = e->agent.mta.relay.port;
	key.cert = e->agent.mta.relay.cert;
	if (!key.cert[0])
		key.cert = NULL;
	key.auth = e->agent.mta.relay.authmap;
	if (!key.auth[0])
		key.auth = NULL;

	if ((route = SPLAY_FIND(mta_route_tree, &routes, &key)) == NULL) {
		route = xcalloc(1, sizeof *route, "mta_route");
		TAILQ_INIT(&route->tasks);
		route->id = generate_uid();
		route->flags = key.flags;
		route->hostname = xstrdup(key.hostname, "mta: hostname");
		route->backupname = key.backupname ?
		    xstrdup(key.backupname, "mta: backupname") : NULL;
		route->port = key.port;
		route->cert = key.cert ? xstrdup(key.cert, "mta: cert") : NULL;
		route->auth = key.auth ? xstrdup(key.auth, "mta: auth") : NULL;
		if (route->cert) {
			strlcpy(ssl.ssl_name, route->cert, sizeof(ssl.ssl_name));
			route->ssl = SPLAY_FIND(ssltree, env->sc_ssl, &ssl);
		}
		SPLAY_INSERT(mta_route_tree, &routes, route);

		route->maxconn = MTA_MAXCONN;
		route->maxmail = MTA_MAXMAIL;
		route->maxrcpt = MTA_MAXRCPT;

		log_trace(TRACE_MTA, "mta: new %s", mta_route_to_text(route));
		stat_increment("mta.route", 1);
	} else {
		log_trace(TRACE_MTA, "mta: reusing %s", mta_route_to_text(route));
	}

	return (route);
}

static void
mta_route_free(struct mta_route *route)
{
	log_trace(TRACE_MTA, "mta: freeing %s", mta_route_to_text(route));
	SPLAY_REMOVE(mta_route_tree, &routes, route);
	free(route->hostname);
	if (route->cert)
		free(route->cert);
	if (route->auth)
		free(route->auth);
	free(route);
}

static void
mta_route_drain(struct mta_route *route)
{
	struct mta_task		*task;
	struct envelope		*e;

	log_debug("mta: draining %s (tasks=%i, refs=%i, sessions=%i)",
	    mta_route_to_text(route),
	    route->ntask, route->refcount, route->nsession);

	if (route->ntask == 0 && route->refcount == 0 && route->nsession == 0) {
		mta_route_free(route);
		stat_decrement("mta.route", 1);
		return;
	}

	if (route->ntask == 0) {
		log_debug("mta: no task for %s", mta_route_to_text(route));
		return;
	}

	if (route->nfail > 3) {
		/* Three connection errors in a row: consider that the route
		 * has a problem.
		 */
		log_debug("mta: too many failures on %s",
		    mta_route_to_text(route));

		while ((task = TAILQ_FIRST(&route->tasks))) {
			TAILQ_REMOVE(&route->tasks, task, entry);
			route->ntask -= 1;
			while((e = TAILQ_FIRST(&task->envelopes)))
				mta_envelope_done(task, e, route->errorline);
			free(task);
			stat_decrement("mta.task", 1);
		}
		route->nfail = 0;
		/* XXX maybe close the route for while */
		return;
	}

	/* make sure there are one session for each task */
	while (route->nsession < route->ntask) {
		/* if we have reached the max number of session, wait */
		if (route->nsession >= route->maxconn) {
			log_debug("mta: max conn reached for %s",
			    mta_route_to_text(route));
			return;
		}
		route->nsession += 1;
		mta_session(route);
	}
}

static void
mta_envelope_done(struct mta_task *task, struct envelope *e, const char *status)
{
	char	relay[MAX_LINE_SIZE];

	envelope_set_errormsg(e, "%s", status);

	snprintf(relay, sizeof relay, "relay=%s, ", task->route->hostname);
	log_envelope(e, relay, e->errorline);

	imsg_compose_event(env->sc_ievs[PROC_QUEUE],
	    mta_response_delivery(e->errorline), 0, 0, -1, e, sizeof(*e));
	TAILQ_REMOVE(&task->envelopes, e, entry);
	free(e);
	stat_decrement("mta.envelope", 1);
}

static int
mta_route_cmp(struct mta_route *a, struct mta_route *b)
{
	int	r;

	if (a->flags < b->flags)
		return (-1);
	if (a->flags > b->flags)
		return (1);

	if (a->port < b->port)
		return (-1);
	if (a->port > b->port)
		return (1);

	if (a->auth == NULL && b->auth)
		return (-1);
	if (a->auth && b->auth == NULL)
		return (1);
	if (a->auth && ((r = strcmp(a->auth, b->auth))))
		return (r);

	if (a->cert == NULL && b->cert)
		return (-1);
	if (a->cert && b->cert == NULL)
		return (1);
	if (a->cert && ((r = strcmp(a->cert, b->cert))))
		return (r);

	if (a->backupname && ((r = strcmp(a->backupname, b->backupname))))
		return (r);

	if ((r = strcmp(a->hostname, b->hostname)))
		return (r);

	return (0);
}

SPLAY_GENERATE(mta_route_tree, mta_route, entry, mta_route_cmp);
