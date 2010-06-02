/*	$OpenBSD: mda.c,v 1.48 2010/06/02 19:16:53 chl Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2009-2010 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

#include <errno.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "smtpd.h"
#include "queue_backend.h"

void			 mda_imsg(struct smtpd *, struct imsgev *, struct imsg *);
__dead void		 mda_shutdown(void);
void			 mda_sig_handler(int, short, void *);
void			 mda_setup_events(struct smtpd *);
void			 mda_disable_events(struct smtpd *);
void			 mda_store(struct mda_session *);
void			 mda_store_event(int, short, void *);
struct mda_session	*mda_lookup(struct smtpd *, u_int32_t);

void
mda_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	char			 output[128], *error, *parent_error;
	struct deliver		 deliver;
	struct mda_session	*s;
	struct action		*action;

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_BATCH_CREATE:
			s = malloc(sizeof *s);
			if (s == NULL)
				fatal(NULL);
			msgbuf_init(&s->w);
			bzero(&s->ev, sizeof s->ev);
			s->id = imsg->hdr.peerid;
			s->content_id = *(u_int64_t *)imsg->data;
			s->datafp = fdopen(imsg->fd, "r");
			if (s->datafp == NULL)
				fatalx("mda: fdopen");
			LIST_INSERT_HEAD(&env->mda_sessions, s, entry);
			return;

		case IMSG_BATCH_APPEND:
			LIST_FOREACH(s, &env->mda_sessions, entry)
				if (s->id == imsg->hdr.peerid)
					break;
			if (s == NULL)
				fatalx("mda: bogus append");
			action = imsg->data;
			s->action_id = action->id;
			s->auxraw = strdup(action->data);
			if (s->auxraw == NULL)
				fatal(NULL);
			auxsplit(&s->aux, s->auxraw);
			return;

		case IMSG_BATCH_CLOSE:
			LIST_FOREACH(s, &env->mda_sessions, entry)
				if (s->id == imsg->hdr.peerid)
					break;
			if (s == NULL)
				fatalx("mda: bogus close");
			memcpy(&s->birth, imsg->data, sizeof s->birth);

			/* request helper process from parent */
			if (s->aux.mode[0] == 'M') {
				deliver.mode = 'P';
				strlcpy(deliver.user, "root", sizeof deliver.user);
				snprintf(deliver.to, sizeof deliver.to,
				    "exec /usr/libexec/mail.local %s",
				    s->aux.user_to);
			} else {
				deliver.mode = s->aux.mode[0];
				strlcpy(deliver.user, s->aux.user_to, sizeof deliver.user);
				strlcpy(deliver.to, s->aux.path, sizeof deliver.to);
			}
			imsg_compose_event(env->sc_ievs[PROC_PARENT],
			    IMSG_PARENT_FORK_MDA, s->id, 0, -1, &deliver,
			    sizeof deliver);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORK_MDA:
			s = mda_lookup(env, imsg->hdr.peerid);

			if (imsg->fd < 0)
				fatalx("mda: fd pass fail");
			s->w.fd = imsg->fd;

			mda_store(s);
			return;

		case IMSG_MDA_DONE:
			s = mda_lookup(env, imsg->hdr.peerid);

			/* all parent errors are temporary */
			if (asprintf(&parent_error, "100 %s", (char *)imsg->data) < 0)
				fatal("mda: asprintf");

			/*
			 * Grab last line of mda stdout/stderr if available.
			 */
			output[0] = '\0';
			if (imsg->fd != -1) {
				char *ln, *buf;
				FILE *fp;
				size_t len;

				buf = NULL;
				if (lseek(imsg->fd, 0, SEEK_SET) < 0)
					fatalx("lseek");
				fp = fdopen(imsg->fd, "r");
				if (fp == NULL)
					fatal("mda: fdopen");
				while ((ln = fgetln(fp, &len))) {
					if (ln[len - 1] == '\n')
						ln[len - 1] = '\0';
					else {
						buf = malloc(len + 1);
						if (buf == NULL)
							fatal(NULL);
						memcpy(buf, ln, len);
						buf[len] = '\0';
						ln = buf;
					}
					strlcpy(output, "100 \"", sizeof output);
					strnvis(output + 5, ln,
					    sizeof(output) - 6,
					    VIS_SAFE | VIS_CSTYLE);
					strlcat(output, "\"", sizeof output);
				}
				free(buf);
				fclose(fp);
			}

			/*
			 * Choose between parent's description of error and
			 * child's output, the latter having preference over
			 * the former.
			 */
			if (strcmp(parent_error + 4, "exited okay") == 0) {
				if (!feof(s->datafp) || s->w.queued)
					error = "100 mda exited prematurely";
				else
					error = "200 ok";
			} else {
				if (output[0])
					error = output;
				else
					error = parent_error;
			}

			/* update queue entry */
			action = malloc(sizeof *action + strlen(error));
			if (action == NULL)
				fatal(NULL);
			action->id = s->action_id;
			strlcpy(action->data, error, strlen(error) + 1);
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_BATCH_UPDATE, s->id, 0, -1, action,
			    sizeof *action + strlen(error));
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_BATCH_DONE, s->id, 0, -1, NULL, 0);

			/* log status */
			log_info("%s: to=%s, delay=%d, stat=%s%s%s",
			    queue_be_decode(s->content_id), rcpt_pretty(&s->aux),
			    time(NULL) - s->birth,
			    *error == '2' ? "Sent" : "Error (",
			    *error == '2' ? "" : error + 4,
			    *error == '2' ? "" : ")");

			/* destroy session */
			LIST_REMOVE(s, entry);
			if (s->w.fd != -1)
				close(s->w.fd);
			if (s->datafp)
				fclose(s->datafp);
			msgbuf_clear(&s->w);
			event_del(&s->ev);
			free(s->auxraw);
			free(s);
			free(parent_error);
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	fatalx("mda_imsg: unexpected imsg");
}

void
mda_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mda_shutdown();
		break;
	default:
		fatalx("mda_sig_handler: unexpected signal");
	}
}

void
mda_shutdown(void)
{
	log_info("mail delivery agent exiting");
	_exit(0);
}

void
mda_setup_events(struct smtpd *env)
{
}

void
mda_disable_events(struct smtpd *env)
{
}

pid_t
mda(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mda: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(pw->pw_dir) == -1)
		fatal("mda: chroot");
	if (chdir("/") == -1)
		fatal("mda: chdir(\"/\")");

	smtpd_process = PROC_MDA;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mda: cannot drop privileges");

	LIST_INIT(&env->mda_sessions);

	imsg_callback = mda_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mda_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mda_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	mda_setup_events(env);
	if (event_dispatch() < 0)
		fatal("event_dispatch");
	mda_shutdown();

	return (0);
}

void
mda_store(struct mda_session *s)
{
	char		*p;
	struct ibuf	*buf;
	int		 len;

	/* XXX: remove user provided Return-Path, if any */
	if (s->aux.mail_from[0])
		len = asprintf(&p, "Return-Path: %s\nDelivered-To: %s\n",
		    s->aux.mail_from, s->aux.rcpt_to);
	else
		len = asprintf(&p, "Delivered-To: %s\n", s->aux.rcpt_to);

	if (len == -1)
		fatal("mda_store: asprintf");

	session_socket_blockmode(s->w.fd, BM_NONBLOCK);
	if ((buf = ibuf_open(len)) == NULL)
		fatal(NULL);
	if (ibuf_add(buf, p, len) < 0)
		fatal(NULL);
	ibuf_close(&s->w, buf);
	event_set(&s->ev, s->w.fd, EV_WRITE, mda_store_event, s);
	event_add(&s->ev, NULL);
	free(p);
}

void
mda_store_event(int fd, short event, void *p)
{
	char			 tmp[16384];
	struct mda_session	*s = p;
	struct ibuf		*buf;
	size_t			 len;

	if (s->w.queued == 0) {
		if ((buf = ibuf_dynamic(0, sizeof tmp)) == NULL)
			fatal(NULL);
		len = fread(tmp, 1, sizeof tmp, s->datafp);
		if (ferror(s->datafp))
			fatal("mda_store_event: fread failed");
		if (feof(s->datafp) && len == 0) {
			close(s->w.fd);
			s->w.fd = -1;
			return;
		}
		if (ibuf_add(buf, tmp, len) < 0)
			fatal(NULL);
		ibuf_close(&s->w, buf);
	}

	if (ibuf_write(&s->w) < 0) {
		close(s->w.fd);
		s->w.fd = -1;
		return;
	}

	event_set(&s->ev, fd, EV_WRITE, mda_store_event, s);
	event_add(&s->ev, NULL);
}

struct mda_session *
mda_lookup(struct smtpd *env, u_int32_t id)
{
	struct mda_session *s;

	LIST_FOREACH(s, &env->mda_sessions, entry)
		if (s->id == id)
			break;

	if (s == NULL)
		fatalx("mda: bogus session id");

	return s;
}
