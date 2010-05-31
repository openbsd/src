/*	$OpenBSD: queue.c,v 1.82 2010/05/31 23:38:56 jacekm Exp $	*/

/*
 * Copyright (c) 2008-2010 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/stat.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "queue_backend.h"
#include "client.h"

void		 queue_imsg(struct smtpd *, struct imsgev *, struct imsg *);
int		 queue_append(struct incoming *, char *);
void		 queue_destroy(struct incoming *);
int		 queue_control(u_int64_t, int);
__dead void	 queue_shutdown(void);
void		 queue_sig_handler(int, short, void *);

void		 queue_mem_init(struct smtpd *);
void		 queue_mem_content_unref(struct content *);

void		 queue_send(int, short, void *);
void		 queue_expire(struct batch *);
void		 queue_update(int, int, u_int64_t, char *);
void		 queue_done(int, int);
void		 queue_schedule(int, struct batch *);
void		 queue_sleep(int);
time_t		 queue_retry(int, time_t, time_t);

void		 queue_bounce_wait(struct content *);
void		 queue_bounce_schedule(int, short, void *);
void		 queue_bounce_init(int, int);
void		 queue_bounce_event(int, short, void *);

int		 queue_detect_loop(struct incoming *);

struct batch	*batch_it(struct incoming *, char *);
int		 batchsort(const void *, const void *);
int		 action_grow(struct action **, char *);
char		*rcpt_pretty(struct aux *);

/* table of batches in larval state */
void	**incoming;
int	  incoming_sz;

struct queue runqs[3];

void
queue_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	struct action		*update;
	struct incoming		*s;
	struct batch		*batch;
	struct message		*m;
	u_int64_t		 content_id;
	u_int			 rq;
	int			 i, fd, error;
	time_t			 now;
	struct iovec		 iov[2];
	char			 aux[2048]; /* XXX */

	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE:
			/*
			 * Create file that will hold mail content.  Its name
			 * uniquely identifies entire mail transaction.  Actions
			 * will refer to the this file as source of mail content.
			 */
			s = calloc(1, sizeof *s);
			if (s == NULL)
				fatal(NULL);
			for (rq = 0; rq < nitems(s->batches); rq++)
				SLIST_INIT(&s->batches[rq]);
			s->content = calloc(1, sizeof *s->content);
			if (s->content == NULL)
				fatal(NULL);
			if (queue_be_content_create(&s->content->id) < 0)
				s->content->id = INVALID_ID;
			i = table_alloc(&incoming, &incoming_sz);
			incoming[i] = s;
			iov[0].iov_base = &s->content->id;
			iov[0].iov_len = sizeof s->content->id;
			iov[1].iov_base = &i;
			iov[1].iov_len = sizeof i;
			imsg_composev(&iev->ibuf, IMSG_QUEUE_CREATE,
			    imsg->hdr.peerid, 0, -1, iov, 2);
			imsg_event_add(iev);
			return;

		case IMSG_QUEUE_DELETE:
			/*
			 * Delete failed transaction's content and actions.
			 */
			memcpy(&i, imsg->data, sizeof i);
			s = table_lookup(incoming, incoming_sz, i);
			if (s == NULL)
				fatalx("queue: bogus delete req");
			incoming[i] = NULL;
			queue_destroy(s);
			return;

		case IMSG_QUEUE_OPEN:
			/*
			 * Open the file that will hold mail content.
			 */
			memcpy(&i, imsg->data, sizeof i);
			s = table_lookup(incoming, incoming_sz, i);
			if (s == NULL)
				fatalx("queue: bogus open req");
			fd = queue_be_content_open(s->content->id, 1);
			if (fd < 0)
				fatal("queue: content open error");
			imsg_compose_event(iev, IMSG_QUEUE_OPEN,
			    imsg->hdr.peerid, 0, fd, NULL, 0);
			return;

		case IMSG_QUEUE_CLOSE:
			/*
			 * Commit mail to queue: we take on responsibility for
			 * performing all requested actions on this content.
			 */
			memcpy(&i, imsg->data, sizeof i);
			s = table_lookup(incoming, incoming_sz, i);
			if (s == NULL)
				fatalx("queue: bogus commit req");
			incoming[i] = NULL;
			if (queue_detect_loop(s) < 0) {
				error = S_MESSAGE_PERMFAILURE;
				imsg_compose_event(iev, IMSG_QUEUE_CLOSE,
				    imsg->hdr.peerid, 0, -1, &error, sizeof error);
				return;
			}
			if (queue_be_commit(s->content->id) < 0) {
				error = S_MESSAGE_TEMPFAILURE;
				imsg_compose_event(iev, IMSG_QUEUE_CLOSE,
				    imsg->hdr.peerid, 0, -1, &error, sizeof error);
				return;
			}
			env->stats->queue.inserts++;
			env->stats->queue.length++;
			time(&now);
			for (rq = 0; rq < nitems(s->batches); rq++) {
				while ((batch = SLIST_FIRST(&s->batches[rq]))) {
					SLIST_REMOVE_HEAD(&s->batches[rq], entry);
					batch = realloc(batch, sizeof *batch);
					if (batch == NULL)
						fatal(NULL);
					batch->retry = now;
					queue_schedule(rq, batch);
				}
			}
			for (i = 0; i < s->nlocal; i++)
				free(s->local[i]);
			free(s->local);
			free(s);
			queue_sleep(Q_LOCAL);
			queue_sleep(Q_RELAY);
			error = 0;
			imsg_compose_event(iev, IMSG_QUEUE_CLOSE,
			    imsg->hdr.peerid, 0, -1, &error, sizeof error);
			return;

		case IMSG_SMTP_ENQUEUE:
			queue_bounce_init(imsg->hdr.peerid, imsg->fd);
			return;
		}
	}

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_APPEND:
			m = imsg->data;
			s = table_lookup(incoming, incoming_sz, m->queue_id);
			if (s == NULL)
				fatalx("queue: bogus append");

			switch (m->recipient.rule.r_action) {
			case A_MBOX:
			case A_MAILDIR:
			case A_EXT:
				/* ?|from|to|user1|user2|path */
				if (m->recipient.rule.r_action == A_MBOX)
					strlcpy(aux, "M|", sizeof aux);
				else if (m->recipient.rule.r_action == A_MAILDIR)
					strlcpy(aux, "D|", sizeof aux);
				else
					strlcpy(aux, "P|", sizeof aux);
				if (m->sender.user[0] && m->sender.domain[0]) {
					strlcat(aux, m->sender.user, sizeof aux);
					strlcat(aux, "@", sizeof aux);
					strlcat(aux, m->sender.domain, sizeof aux);
				}
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->session_rcpt.user, sizeof aux);
				strlcat(aux, "@", sizeof aux);
				strlcat(aux, m->session_rcpt.domain, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->sender.pw_name, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->recipient.pw_name, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->recipient.rule.r_value.path, sizeof aux);
				break;

			case A_FILENAME:
				/* F|from|to|user1|user2|path */
				strlcpy(aux, "F|", sizeof aux);
				if (m->sender.user[0] && m->sender.domain[0]) {
					strlcat(aux, m->sender.user, sizeof aux);
					strlcat(aux, "@", sizeof aux);
					strlcat(aux, m->sender.domain, sizeof aux);
				}
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->session_rcpt.user, sizeof aux);
				strlcat(aux, "@", sizeof aux);
				strlcat(aux, m->session_rcpt.domain, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->sender.pw_name, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, SMTPD_USER, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->recipient.u.filename, sizeof aux);
				break;

			case A_RELAY:
			case A_RELAYVIA:
				/* R|from|to|user|rcpt|via|port|ssl|cert|auth */
				strlcpy(aux, "R|", sizeof aux);
				if (m->sender.user[0] && m->sender.domain[0]) {
					strlcat(aux, m->sender.user, sizeof aux);
					strlcat(aux, "@", sizeof aux);
					strlcat(aux, m->sender.domain, sizeof aux);
				}
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->session_rcpt.user, sizeof aux);
				strlcat(aux, "@", sizeof aux);
				strlcat(aux, m->session_rcpt.domain, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->sender.pw_name, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->recipient.user, sizeof aux);
				strlcat(aux, "@", sizeof aux);
				strlcat(aux, m->recipient.domain, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				if (m->recipient.rule.r_action == A_RELAYVIA)
					strlcat(aux, m->recipient.rule.r_value.relayhost.hostname, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				if (m->recipient.rule.r_value.relayhost.port) {
					char port[10];
					snprintf(port, sizeof port, "%d", ntohs(m->recipient.rule.r_value.relayhost.port));
					strlcat(aux, port, sizeof aux);
				}
				strlcat(aux, "|", sizeof aux);
				switch (m->recipient.rule.r_value.relayhost.flags & F_SSL) {
				case F_SSL:
					strlcat(aux, "ssl", sizeof aux);
					break;
				case F_SMTPS:
					strlcat(aux, "smtps", sizeof aux);
					break;
				case F_STARTTLS:
					strlcat(aux, "starttls", sizeof aux);
					break;
				}
				strlcat(aux, "|", sizeof aux);
				strlcat(aux, m->recipient.rule.r_value.relayhost.cert, sizeof aux);
				strlcat(aux, "|", sizeof aux);
				if (m->recipient.rule.r_value.relayhost.flags & F_AUTH)
					strlcat(aux, "secrets", sizeof aux);
				break;

			default:
				fatalx("queue: bad r_action");
			}
			if (queue_append(s, aux) < 0)
				error = S_MESSAGE_TEMPFAILURE;
			else
				error = 0;
			imsg_compose_event(iev, IMSG_QUEUE_APPEND,
			    imsg->hdr.peerid, 0, -1, &error, sizeof error);
			return;
		}
	}

	if (iev->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_UPDATE:
			update = imsg->data;
			queue_update(Q_LOCAL, imsg->hdr.peerid, update->id,
			    update->arg);
			return;

		case IMSG_BATCH_DONE:
			queue_done(Q_LOCAL, imsg->hdr.peerid);
			return;

		}
	}

	if (iev->proc == PROC_MTA) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_UPDATE:
			update = imsg->data;
			queue_update(Q_RELAY, imsg->hdr.peerid, update->id,
			    update->arg);
			return;

		case IMSG_BATCH_DONE:
			queue_done(Q_RELAY, imsg->hdr.peerid);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_PAUSE_LOCAL:
			runqs[Q_LOCAL].max = 0;
			queue_sleep(Q_LOCAL);
			return;

		case IMSG_QUEUE_PAUSE_RELAY:
			runqs[Q_RELAY].max = 0;
			queue_sleep(Q_RELAY);
			return;

		case IMSG_QUEUE_RESUME_LOCAL:
			runqs[Q_LOCAL].max = env->sc_maxconn;
			queue_sleep(Q_LOCAL);
			return;

		case IMSG_QUEUE_RESUME_RELAY:
			runqs[Q_RELAY].max = env->sc_maxconn;
			queue_sleep(Q_RELAY);
			return;

		case IMSG_QUEUE_SCHEDULE:
			memcpy(&content_id, imsg->data, sizeof content_id);
			error = queue_control(content_id, 1);
			if (error)
				log_warnx("schedule request failed");
			else {
				queue_sleep(Q_LOCAL);
				queue_sleep(Q_RELAY);
				queue_sleep(Q_BOUNCE);
			}
			imsg_compose_event(iev, IMSG_QUEUE_SCHEDULE,
			    imsg->hdr.peerid, 0, -1, &error, sizeof error);
			return;

		case IMSG_QUEUE_REMOVE:
			memcpy(&content_id, imsg->data, sizeof content_id);
			error = queue_control(content_id, 0);
			if (error)
				log_warnx("remove request failed");
			else {
				queue_sleep(Q_LOCAL);
				queue_sleep(Q_RELAY);
				queue_sleep(Q_BOUNCE);
			}
			imsg_compose_event(iev, IMSG_QUEUE_REMOVE,
			    imsg->hdr.peerid, 0, -1, &error, sizeof error);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	fatalx("queue_imsg: unexpected imsg");
}

int
queue_append(struct incoming *s, char *auxraw)
{
	struct batch	*batch;
	struct action	*action;
	char		*copy;
	struct aux	 aux;

	log_debug("aux %s", auxraw);

	copy = strdup(auxraw);
	if (copy == NULL)
		fatal(NULL);
	auxsplit(&aux, copy);

	/* remember local recipients for delivered-to: loop detection */
	if (aux.mode[0] != 'R') {
		if (s->nlocal == s->local_sz) {
			s->local_sz *= 2;
			s->local = realloc(s->local, ++s->local_sz *
			    sizeof s->local[0]);
			if (s->local == NULL)
				fatal(NULL);
		}
		/*
		 * XXX: using rcpt_to is wrong because it's unexpanded address
		 * as seen in RCPT TO; must use expanded address in the form
		 * <user>@<domain>, but since lka expands local addresses to
		 * just <user> this is currently undoable.
		 */
		s->local[s->nlocal] = strdup(aux.rcpt_to);
		if (s->local[s->nlocal] == NULL)
			fatal(NULL);
		s->nlocal++;
	}

	/* assign batch */
	if (aux.mode[0] != 'R')
		batch = batch_it(s, "");
	else if (aux.relay_via[0])
		batch = batch_it(s, aux.relay_via);
	else
		batch = batch_it(s, strchr(aux.rcpt, '@'));
	if (batch == NULL)
		fatal(NULL);
	free(copy);

	action = malloc(sizeof *action);
	if (action == NULL)
		fatal(NULL);
	SLIST_INSERT_HEAD(&batch->actions, action, entry);
	if (queue_be_action_new(s->content->id, &action->id, auxraw) < 0)
		return -1;

	s->content->ref++;

	return 0;
}

struct batch *
batch_it(struct incoming *s, char *sortkey)
{
	struct batch	*batch;
	size_t		 batch_sz;
	u_int		 rq;

	if (*sortkey) {
		rq = Q_RELAY;
		SLIST_FOREACH(batch, &s->batches[rq], entry)
			if (strcmp(batch->sortkey, sortkey) == 0)
				break;
	} else {
		rq = Q_LOCAL;
		batch = NULL;
	}

	if (batch == NULL) {
		batch_sz = sizeof *batch + strlen(sortkey) + 1;
		batch = malloc(batch_sz);
		if (batch == NULL)
			return NULL;
		SLIST_INIT(&batch->actions);
		batch->retry = 0;
		batch->content = s->content;
		strlcpy(batch->sortkey, sortkey, batch_sz - sizeof *batch);
		SLIST_INSERT_HEAD(&s->batches[rq], batch, entry);
	}

	return batch;
}

void
queue_destroy(struct incoming *s)
{
	struct batch	*batch;
	struct action	*action;
	u_int		 rq;
	int		 i;

	for (rq = 0; rq < nitems(s->batches); rq++) {
		while ((batch = SLIST_FIRST(&s->batches[rq]))) {
			SLIST_REMOVE_HEAD(&s->batches[rq], entry);
			while ((action = SLIST_FIRST(&batch->actions))) {
				SLIST_REMOVE_HEAD(&batch->actions, entry);
				queue_be_action_delete(s->content->id,
				    action->id);
				free(action);
			}
			free(batch);
		}
	}
	queue_be_content_delete(s->content->id);
	free(s->content);
	for (i = 0; i < s->nlocal; i++)
		free(s->local[i]);
	free(s->local);
	free(s);
}

/*
 * Walk all runqueues to schedule or remove requested content.
 */
int
queue_control(u_int64_t content_id, int schedule)
{
	struct batch	*b, *next;
	struct action	*action;
	struct action_be a;
	struct aux	 aux;
	u_int		 rq, n;

	n = 0;
	for (rq = 0; rq < nitems(runqs); rq++) {
		for (b = SLIST_FIRST(&runqs[rq].head); b; b = next) {
			next = SLIST_NEXT(b, entry);
			if (content_id && b->content->id != content_id)
				continue;
			n++;
			SLIST_REMOVE(&runqs[rq].head, b, batch, entry);
			if (schedule) {
				time(&b->retry);
				queue_schedule(rq, b);
				continue;
			}
			while ((action = SLIST_FIRST(&b->actions))) {
				SLIST_REMOVE_HEAD(&b->actions, entry);
				if (queue_be_action_read(&a, b->content->id,
				    action->id) < 0)
					fatal("queue: action read error");
				auxsplit(&aux, a.aux);
				log_info("%s: to=%s, delay=%d, stat=Removed",
				    queue_be_decode(b->content->id),
				    rcpt_pretty(&aux), time(NULL) - a.birth);
				queue_be_action_delete(b->content->id,
				    action->id);
				queue_mem_content_unref(b->content);
				free(action);
			}
			free(b);
		}
	}

	return (n > 0 ? 0 : -1);
}

void
queue_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		queue_shutdown();
		break;
	default:
		fatalx("queue_sig_handler: unexpected signal");
	}
}

void
queue_shutdown(void)
{
	log_info("queue exiting");
	_exit(0);
}

pid_t
queue(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;
	u_int		 rq;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch },
		{ PROC_SMTP,	imsg_dispatch },
		{ PROC_MDA,	imsg_dispatch },
		{ PROC_MTA,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");

	smtpd_process = PROC_QUEUE;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");

	/*
	 * Queue opens fds for four purposes: smtp, mta, mda, and bounces.
	 * Therefore, use all available fd space and set the maxconn (=max
	 * session count for each of these tasks) to a quarter of this value.
	 */
	fdlimit(1.0);
	if ((env->sc_maxconn = availdesc() / 4) < 1)
		fatalx("queue: fd starvation");

	imsg_callback = queue_imsg;
	event_init();

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	for (rq = 0; rq < nitems(runqs); rq++) {
		SLIST_INIT(&runqs[rq].head);
		runqs[rq].env = env;
		runqs[rq].max = env->sc_maxconn;
	}
	runqs[Q_LOCAL].name = "Q_LOCAL";
	runqs[Q_RELAY].name = "Q_RELAY";
	runqs[Q_BOUNCE].name = "Q_BOUNCE";

	/* bouncing costs 2 fds: file and socket */
	runqs[Q_BOUNCE].max /= 2;

	queue_mem_init(env);
	queue_sleep(Q_LOCAL);
	queue_sleep(Q_RELAY);
	queue_sleep(Q_BOUNCE);

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	event_dispatch();
	queue_shutdown();

	return (0);
}

void
queue_mem_init(struct smtpd *env)
{
	SLIST_HEAD(,batch)	  lookup[4096];
	void			**batch;
	struct content		 *content;
	struct action		 *action;
	struct batch		 *b;
	char			 *sortkey;
	struct action_be	  a;
	struct aux		  aux;
	int			  batch_sz, batches, rq, sz, i;

	for (i = 0; i < 4096; i++)
		SLIST_INIT(&lookup[i]);
	batch = NULL;
	batch_sz = 0;
	batches = 0;

	/*
	 * Sort actions into batches.
	 */
	for (;;) {
		if (queue_be_getnext(&a) < 0)
			fatal("queue: backend error");
		if (a.action_id == 0)
			break;
		auxsplit(&aux, a.aux);

		/*
		 * Assignment to batch is based on the sortkey:
		 * B=<content_id>	for bounced mail
		 * R=<domain>		for relayed mail
		 * L=<action_id>	for local mail
		 */
		if (a.status[0] == '5' || a.status[0] == '6')
			asprintf(&sortkey, "B=%s", queue_be_decode(a.content_id));
		else if (aux.mode[0] == 'R') {
			if (aux.relay_via[0])
				asprintf(&sortkey, "R=%s", aux.relay_via);
			else
				asprintf(&sortkey, "R=%s", strchr(aux.rcpt, '@'));
		} else
			asprintf(&sortkey, "L=%s", queue_be_decode(a.action_id));

		content = NULL;
		SLIST_FOREACH(b, &lookup[a.content_id & 4095], entry) {
			if (b->content->id == a.content_id) {
				content = b->content;
				if (strcmp(b->sortkey, sortkey) == 0)
					break;
			}
		}

		if (b == NULL) {
			sz = sizeof *b + strlen(sortkey) + 1;
			b = malloc(sz);
			if (b == NULL)
				fatal("queue_mem_init");
			SLIST_INIT(&b->actions);
			strlcpy(b->sortkey, sortkey, sz - sizeof *b);

			if (*sortkey == 'B')
				rq = Q_BOUNCE;
			else if (*sortkey == 'R')
				rq = Q_RELAY;
			else
				rq = Q_LOCAL;

			b->retry = queue_retry(rq, a.birth, a.birth + 1);
			while (b->retry < time(NULL))
				b->retry = queue_retry(rq, a.birth, b->retry);

			if (b->retry > a.birth + SMTPD_EXPIRE)
				b->retry = NO_RETRY_EXPIRED;

			if (content)
				b->content = content;
			else {
				b->content = calloc(1, sizeof *b->content);
				if (b->content == NULL)
					fatal("queue_mem_init");
				b->content->id = a.content_id;
				b->content->ref = 0;
				env->stats->queue.length++;
			}

			SLIST_INSERT_HEAD(&lookup[a.content_id & 4095], b, entry);
			if (batches == batch_sz) {
				batch_sz *= 2;
				batch = realloc(batch, ++batch_sz * sizeof *batch);
				if (batch == NULL)
					fatal("queue_mem_init");
			}
			batch[batches] = b;
			batches++;
		}

		action = malloc(sizeof *action);
		if (action == NULL)
			fatal("queue_mem_init");
		action->id = a.action_id;
		SLIST_INSERT_HEAD(&b->actions, action, entry);
		b->content->ref++;
		free(sortkey);
	}

	/*
	 * Add batches to schedule.
	 */
	qsort(batch, batches, sizeof *batch, batchsort);
	for (i = 0; i < batches; i++) {
		b = batch[i];
		if (b->sortkey[0] == 'B')
			rq = Q_BOUNCE;
		else if (b->sortkey[0] == 'R')
			rq = Q_RELAY;
		else
			rq = Q_LOCAL;
		b = realloc(b, sizeof *b);
		if (b == NULL)
			fatal("queue_mem_init");
		queue_schedule(rq, b);
	}
	free(batch);
}

int
batchsort(const void *x, const void *y)
{
	const struct batch *b1 = x, *b2 = y;
	return (b1->retry < b2->retry ? -1 : b1->retry > b2->retry);
}

void
queue_mem_content_unref(struct content *content)
{
	content->ref--;
	if (content->ref < 0)
		fatalx("queue: bad refcount");
	else if (content->ref == 0) {
		queue_be_content_delete(content->id);
		runqs[Q_LOCAL].env->stats->queue.length--;
	}
}

void
queue_send(int fd, short event, void *p)
{
	struct smtpd		*env;
	struct batch		*batch;
	struct action		*action;
	struct action_be	 a;
	int			 rq, i, to, size;
	time_t			 now;

	rq = (struct queue *)p - runqs;
	env = runqs[rq].env;
	time(&now);
	i = -1;

	while ((batch = SLIST_FIRST(&runqs[rq].head))) {
		if (batch->retry > now || runqs[rq].sessions >= runqs[rq].max)
			break;

		SLIST_REMOVE_HEAD(&runqs[rq].head, entry);
		i = table_alloc(&runqs[rq].session, &runqs[rq].session_sz);
		runqs[rq].session[i] = batch;
		runqs[rq].sessions++;

		log_debug("%s: %d: start %s", runqs[rq].name, i,
		    queue_be_decode(batch->content->id));

		if (batch->retry == NO_RETRY_EXPIRED) {
			log_debug("%s: %d: expire", runqs[rq].name, i);
			queue_expire(batch);
			queue_done(rq, i);
			continue;
		}

		if (rq == Q_BOUNCE) {
			log_debug("%s: %d: socket request", runqs[rq].name, i);
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_SMTP_ENQUEUE, i, 0, -1, NULL, 0);
			continue;
		}

		log_debug("%s: %d: send", runqs[rq].name, i);

		fd = queue_be_content_open(batch->content->id, 0);
		if (fd < 0)
			fatal("queue: content open error");

		if (rq == Q_LOCAL)
			to = PROC_MDA;
		else
			to = PROC_MTA;

		imsg_compose_event(env->sc_ievs[to], IMSG_BATCH_CREATE, i, 0,
		    fd, &batch->content->id, sizeof batch->content->id);

		while ((action = SLIST_FIRST(&batch->actions))) {
			SLIST_REMOVE_HEAD(&batch->actions, entry);
			if (queue_be_action_read(&a, batch->content->id,
			    action->id) < 0)
				fatal("queue: action read error");
			size = action_grow(&action, a.aux);
			imsg_compose_event(env->sc_ievs[to], IMSG_BATCH_APPEND,
			    i, 0, -1, action, size);
			free(action);
		}

		imsg_compose_event(env->sc_ievs[to], IMSG_BATCH_CLOSE, i, 0, -1,
		    &a.birth, sizeof a.birth);
	}

	/* Sanity check: were we called for no good reason? */
	if (i == -1)
		fatalx("queue_send: empty run");

	queue_sleep(rq);
}

void
queue_expire(struct batch *batch)
{
	struct action	*action, *fail;
	struct action_be a;
	struct aux	 aux;
	time_t		 birth;
	int		 error;

	action = SLIST_FIRST(&batch->actions);
	if (queue_be_action_read(&a, batch->content->id, action->id) < 0)
		fatal("queue: action read error");

	auxsplit(&aux, a.aux);
	birth = a.birth;

	if (a.status[0] == '5' || a.status[0] == '6') {
		log_warnx("%s: to=%s, delay=%d, stat=Expired (no bounce due "
		    "to: larval bounce)",
		    queue_be_decode(batch->content->id), aux.mail_from,
		    time(NULL) - birth);
		while ((action = SLIST_FIRST(&batch->actions))) {
			SLIST_REMOVE_HEAD(&batch->actions, entry);
			queue_be_action_delete(batch->content->id, action->id);
			queue_mem_content_unref(batch->content);
			free(action);
		}
		return;
	}

	if (aux.mail_from[0] == '\0') {
		while ((action = SLIST_FIRST(&batch->actions))) {
			SLIST_REMOVE_HEAD(&batch->actions, entry);

			if (queue_be_action_read(&a, batch->content->id,
			    action->id) < 0)
				fatal("queue: action read error");
			auxsplit(&aux, a.aux);
			log_warnx("%s: to=%s, delay=%d, stat=Expired (no bounce "
			    "due to: double bounce)",
			    queue_be_decode(batch->content->id),
			    rcpt_pretty(&aux), time(NULL) - birth);

			queue_be_action_delete(batch->content->id, action->id);
			queue_mem_content_unref(batch->content);
			free(action);
		}
		return;
	}

	SLIST_FOREACH(action, &batch->actions, entry)
		if (queue_be_action_status(batch->content->id, action->id,
		    "600 Message expired after too many delivery attempts") < 0)
			break;

	if (action) {
		fail = action;
		error = errno;
	} else {
		fail = NULL;
		error = 0;
	}

	while ((action = SLIST_FIRST(&batch->actions))) {
		if (action == fail)
			break;
		SLIST_REMOVE_HEAD(&batch->actions, entry);

		if (queue_be_action_read(&a, batch->content->id,
		    action->id) < 0)
			fatal("queue: action read error");
		auxsplit(&aux, a.aux);
		log_info("%s: to=%s, delay=%d, stat=Expired",
		    queue_be_decode(batch->content->id), rcpt_pretty(&aux),
		    time(NULL) - birth);

		SLIST_INSERT_HEAD(&batch->content->actions, action, entry);

		queue_bounce_wait(batch->content);
	}

	while ((action = SLIST_FIRST(&batch->actions))) {
		SLIST_REMOVE_HEAD(&batch->actions, entry);

		if (queue_be_action_read(&a, batch->content->id,
		    action->id) < 0)
			fatal("queue: action read error");
		auxsplit(&aux, a.aux);
		log_warnx("%s: to=%s, delay=%d, stat=Expired (no bounce due "
		    "to: %s)",
		    queue_be_decode(batch->content->id), rcpt_pretty(&aux),
		    time(NULL) - birth, strerror(error));

		queue_be_action_delete(batch->content->id, action->id);
		queue_mem_content_unref(batch->content);
		free(action);
	}
}

/*
 * Grow action to append auxillary info needed by mta and mda.  To conserve
 * memory, queue calls this routine only for active delivery sessions so that
 * pending actions, potentially many, remain tiny.
 */
int
action_grow(struct action **action, char *aux)
{
	struct action *p;
	int size;

	size = sizeof *p + strlen(aux) + 1;
	p = realloc(*action, size);
	if (p == NULL)
		fatal(NULL);
	strlcpy(p->arg, aux, size - sizeof *p);
	*action = p;

	return size;
}

void
queue_update(int rq, int i, u_int64_t action_id, char *new_status)
{
	struct batch	*batch;
	struct action	*action;
	struct action_be a;
	struct aux	 aux;

	batch = table_lookup(runqs[rq].session, runqs[rq].session_sz, i);
	if (batch == NULL)
		fatalx("queue: bogus update");

	if (*new_status == '2') {
		queue_be_action_delete(batch->content->id, action_id);
		queue_mem_content_unref(batch->content);
		return;
	}

	action = malloc(sizeof *action);
	if (action == NULL)
		fatal(NULL);
	action->id = action_id;

	if (*new_status == '5' || *new_status == '6') {
		if (queue_be_action_read(&a, batch->content->id, action_id) < 0)
			fatal("queue: queue read error");

		auxsplit(&aux, a.aux);

		if (aux.mail_from[0] == '\0') {
			log_warnx("%s: bounce recipient %s not contactable, "
			    "bounce dropped",
			    queue_be_decode(batch->content->id), aux.rcpt_to);
			queue_be_action_delete(batch->content->id, action_id);
			queue_mem_content_unref(batch->content);
			free(action);
			return;
		}

		if (queue_be_action_status(batch->content->id, action_id,
		    new_status) < 0) {
			log_warn("%s: recipient %s not contactable, bounce not "
			    "created due to queue error",
			    queue_be_decode(batch->content->id), aux.rcpt_to);
			queue_be_action_delete(batch->content->id, action_id);
			queue_mem_content_unref(batch->content);
			free(action);
			return;
		}

		SLIST_INSERT_HEAD(&batch->content->actions, action, entry);

		queue_bounce_wait(batch->content);
	} else {
		queue_be_action_status(batch->content->id, action_id, new_status);
		SLIST_INSERT_HEAD(&batch->actions, action, entry);
	}
}

void
queue_done(int rq, int i)
{
	struct action_be a;
	struct batch	*batch;
	struct action	*action;

	/* Take batch off the session table. */
	batch = table_lookup(runqs[rq].session, runqs[rq].session_sz, i);
	if (batch == NULL)
		fatalx("queue: bogus batch");
	runqs[rq].session[i] = NULL;
	runqs[rq].sessions--;

	log_debug("%s: %d: done", runqs[rq].name, i);

	/* All actions sent? */
	if (SLIST_EMPTY(&batch->actions)) {
		if (batch->content->ref == 0) {
			free(batch->content->ev);
			free(batch->content);
		}
		free(batch);
	} else {
		/* Batch has actions with temporary errors. */
		action = SLIST_FIRST(&batch->actions);
		if (queue_be_action_read(&a, batch->content->id,
		    action->id) < 0)
			fatal("queue: action read error");
		batch->retry = queue_retry(rq, a.birth, batch->retry);
		if (batch->retry > a.birth + SMTPD_EXPIRE)
			batch->retry = NO_RETRY_EXPIRED;
		queue_schedule(rq, batch);
	}

	queue_sleep(rq);
}

/*
 * Insert batch into runqueue in retry time order.
 */
void
queue_schedule(int rq, struct batch *batch)
{
	struct batch *b, *prev;

	prev = NULL;

	SLIST_FOREACH(b, &runqs[rq].head, entry) {
		if (b->retry >= batch->retry) {
			if (prev)
				SLIST_INSERT_AFTER(prev, batch, entry);
			else
				SLIST_INSERT_HEAD(&runqs[rq].head, batch,
				    entry);
			break;
		}
		prev = b;
	}

	if (b == NULL) {
		if (prev)
			SLIST_INSERT_AFTER(prev, batch, entry);
		else
			SLIST_INSERT_HEAD(&runqs[rq].head, batch, entry);
	}
}

void
queue_sleep(int rq)
{
	struct timeval	 tv;
	struct batch	*next;
	time_t		 now;

	evtimer_del(&runqs[rq].ev);

	if (runqs[rq].sessions >= runqs[rq].max)
		return;

	next = SLIST_FIRST(&runqs[rq].head);
	if (next == NULL)
		return;
	
	time(&now);
	if (next->retry < now)
		tv.tv_sec = 0;
	else
		tv.tv_sec = next->retry - now;
	tv.tv_usec = 0;

	log_debug("%s: sleep %lus", runqs[rq].name, tv.tv_sec);

	evtimer_set(&runqs[rq].ev, queue_send, &runqs[rq]);
	evtimer_add(&runqs[rq].ev, &tv);
}

/*
 * Qmail-like retry schedule.
 *
 * Local deliveries are tried more often than remote.
 */
time_t
queue_retry(int rq, time_t birth, time_t last)
{
	int n;

	if (last - birth < 0)
		n = 0;
	else if (rq == Q_RELAY)
		n = sqrt(last - birth) + 20;
	else
		n = sqrt(last - birth) + 10;

	return birth + n * n;
}

/*
 * Wait for permanent failures against this content for few more seconds.
 * If none arrive, combine them into single batch and put on Q_BOUNCE
 * runqueue.  If one does arrive, append it, and restart the timer.
 */
void
queue_bounce_wait(struct content *content)
{
	struct timeval tv;

	if (content->ev == NULL) {
		content->ev = calloc(1, sizeof *content->ev);
		if (content->ev == NULL)
			fatal(NULL);
	}
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_del(content->ev);
	evtimer_set(content->ev, queue_bounce_schedule, content);
	evtimer_add(content->ev, &tv);
}

void
queue_bounce_schedule(int fd, short event, void *p)
{
	struct content	*content = p;
	struct batch	*batch;
	struct action	*action;

	free(content->ev);
	content->ev = NULL;

	batch = malloc(sizeof *batch);
	if (batch == NULL)
		fatal(NULL);
	SLIST_INIT(&batch->actions);
	batch->content = content;
	while ((action = SLIST_FIRST(&content->actions))) {
		SLIST_REMOVE_HEAD(&content->actions, entry);
		SLIST_INSERT_HEAD(&batch->actions, action, entry);
	}
	time(&batch->retry);
	queue_schedule(Q_BOUNCE, batch);
	queue_sleep(Q_BOUNCE);
}

void
queue_bounce_init(int i, int sock)
{
	struct smtpd	*env = runqs[Q_BOUNCE].env;
	struct batch	*batch;
	struct bounce	*s;
	struct action	*action;
	struct action_be a;
	struct aux	 aux;
	int		 fd, header;

	log_debug("%s: %d: init", runqs[Q_BOUNCE].name, i);

	batch = table_lookup(runqs[Q_BOUNCE].session,
	    runqs[Q_BOUNCE].session_sz, i);
	if (batch == NULL)
		fatalx("queue: bogus bounce batch");

	if (sock < 0) {
		queue_done(Q_BOUNCE, i);
		return;
	}

	fd = queue_be_content_open(batch->content->id, 0);
	if (fd < 0)
		fatal("queue: content open error");

	s = calloc(1, sizeof *s);
	if (s == NULL)
		fatal(NULL);
	s->batch = batch;
	s->pcb = client_init(sock, fd, env->sc_hostname, 1);
	s->id = i;
	client_sender(s->pcb, "");
	client_ssl_optional(s->pcb);

	header = 0;
	SLIST_FOREACH(action, &batch->actions, entry) {
		if (queue_be_action_read(&a, batch->content->id,
		    action->id) < 0)
			fatal("queue: backend read error");
		auxsplit(&aux, a.aux);
		if (header == 0) {
			client_rcpt(s->pcb, "%s", aux.mail_from);
			client_printf(s->pcb,
			    "From: Mailer Daemon <MAILER-DAEMON@%s>\n"
			    "To: %s\n"
			    "Subject: Delivery status notification\n"
			    "Date: %s\n"
			    "\n"
			    "This is automated mail delivery notification, please DO NOT REPLY.\n"
			    "An error has occurred while attempting to deliver your mail to the\n"
			    "following recipients:\n"
			    "\n",
			    env->sc_hostname, aux.mail_from,
			    time_to_text(time(NULL)));
			header = 1;
		}
		if (strlen(a.status) > 4 && (a.status[0] == '1' || a.status[0] == '6'))
			a.status += 4;
		client_printf(s->pcb, "%s: %s\n\n", aux.rcpt_to, a.status);
	}
	client_printf(s->pcb, "Below is a copy of your mail:\n\n");

	session_socket_blockmode(sock, BM_NONBLOCK);
	event_set(&s->ev, sock, EV_READ|EV_WRITE, queue_bounce_event, s);
	event_add(&s->ev, &s->pcb->timeout);
}

void
queue_bounce_event(int fd, short event, void *p)
{
	struct action	*action;
	struct bounce	*s = p;
	char		*status = NULL;

	if (event & EV_TIMEOUT) {
		status = "100 timeout";
		goto out;
	}

	switch (client_talk(s->pcb, event & EV_WRITE)) {
	case CLIENT_STOP_WRITE:
		goto ro;
	case CLIENT_WANT_WRITE:
		goto rw;
	case CLIENT_RCPT_FAIL:
		status = s->pcb->reply;
		break;
	case CLIENT_DONE:
		status = s->pcb->status;
		break;
	default:
		fatalx("queue: bad client_talk");
	}

out:
	log_debug("%s: %d: last event", runqs[Q_BOUNCE].name, s->id);

	if (*status == '5' || *status == '6')
		fatalx("queue: smtp refused bounce");
	if (*status == '2') {
		while ((action = SLIST_FIRST(&s->batch->actions))) {
			SLIST_REMOVE_HEAD(&s->batch->actions, entry);
			queue_be_action_delete(s->batch->content->id,
			    action->id);
			queue_mem_content_unref(s->batch->content);
			free(action);
		}
	}
	queue_done(Q_BOUNCE, s->id);
	client_close(s->pcb);
	free(s);
	return;

ro:
	event_set(&s->ev, fd, EV_READ, queue_bounce_event, s);
	event_add(&s->ev, &s->pcb->timeout);
	return;

rw:
	event_set(&s->ev, fd, EV_READ|EV_WRITE, queue_bounce_event, s);
	event_add(&s->ev, &s->pcb->timeout);
}

int
queue_detect_loop(struct incoming *s)
{
	FILE	*fp;
	char	*buf, *lbuf;
	size_t	 len, received;
	int	 fd, i;

	fd = queue_be_content_open(s->content->id, 0);
	if (fd < 0)
		fatal("queue_detect_loop: content open error");
	fp = fdopen(fd, "r");
	if (fp == NULL)
		fatal("queue_detect_loop: fdopen");

	received = 0;
	lbuf = NULL;

	while ((buf = fgetln(fp, &len))) {
		free(lbuf);
		lbuf = NULL;

		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			len--;
		} else {
			/* EOF without EOL, copy and add the NUL */
			if ((lbuf = malloc(len + 1)) == NULL)
				fatal(NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (*buf == '\0') {
			buf = NULL;
			break;
		}

		if (strncasecmp(buf, "Received:", 9) == 0) {
			received++;
			if (received >= MAX_HOPS_COUNT) 
				break;
		} else if (strncasecmp(buf, "Delivered-To:", 13) == 0) {
			buf += 13;
			while (isspace(*buf))
				buf++;
			buf[strcspn(buf, " \t")] = '\0';
			for (i = 0; i < s->nlocal; i++)
				if (strcmp(s->local[i], buf) == 0)
					break;
			if (i < s->nlocal)
				break;
		}
	}
	free(lbuf);
	fclose(fp);

	return (buf == NULL ? 0 : -1);
}
