/*	$OpenBSD: mda.c,v 1.39 2010/04/19 20:10:36 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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

__dead void		 mda_shutdown(void);
void			 mda_sig_handler(int, short, void *);
void			 mda_dispatch_parent(int, short, void *);
void			 mda_dispatch_queue(int, short, void *);
void			 mda_dispatch_runner(int, short, void *);
void			 mda_setup_events(struct smtpd *);
void			 mda_disable_events(struct smtpd *);
void			 mda_store(struct mda_session *);
void			 mda_store_event(int, short, void *);
struct mda_session	*mda_lookup(struct smtpd *, u_int32_t);

int mda_id;

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
mda_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_PARENT];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mda_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_FORK_MDA: {
			struct mda_session	*s;

			s = mda_lookup(env, imsg.hdr.peerid);

			if (imsg.fd < 0)
				fatalx("mda: fd pass fail");
			s->w.fd = imsg.fd;

			/* send message content to the helper process */
			mda_store(s);
			break;
		}

		case IMSG_MDA_DONE: {
			char			 output[128];
			struct mda_session	*s;
			struct path		*path;
			char			*error, *parent_error;

			s = mda_lookup(env, imsg.hdr.peerid);

			/*
			 * Grab last line of mda stdout/stderr if available.
			 */
			output[0] = '\0';
			if (imsg.fd != -1) {
				char *ln, *buf;
				FILE *fp;
				size_t len;

				buf = NULL;
				if (lseek(imsg.fd, 0, SEEK_SET) < 0)
					fatalx("lseek");
				fp = fdopen(imsg.fd, "r");
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
					strlcpy(output, "\"", sizeof output);
					strnvis(output + 1, ln,
					    sizeof(output) - 2,
					    VIS_SAFE | VIS_CSTYLE);
					strlcat(output, "\"", sizeof output);
					log_debug("mda_out: %s", output);
				}
				free(buf);
				fclose(fp);
			}

			/*
			 * Choose between parent's description of error and
			 * child's output, the latter having preference over
			 * the former.
			 */
			error = NULL;
			parent_error = imsg.data;
			if (strcmp(parent_error, "exited okay") == 0) {
				if (!feof(s->datafp) || s->w.queued)
					error = "mda exited prematurely";
			} else {
				if (output[0])
					error = output;
				else
					error = parent_error;
			}

			/* update queue entry */
			if (error == NULL)
				s->msg.status = S_MESSAGE_ACCEPTED;
			else
				strlcpy(s->msg.session_errorline, error,
				    sizeof s->msg.session_errorline);
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1, &s->msg,
			    sizeof s->msg);

			/*
			 * XXX: which struct path gets used for logging depends
			 * on whether lka did aliases or .forward processing;
			 * lka may need to be changed to present data in more
			 * unified way.
			 */
			if (s->msg.recipient.rule.r_action == A_MAILDIR ||
			    s->msg.recipient.rule.r_action == A_MBOX)
				path = &s->msg.recipient;
			else
				path = &s->msg.session_rcpt;

			/* log status */
			if (error && asprintf(&error, "Error (%s)", error) < 0)
				fatal("mda: asprintf");
			log_info("%s: to=<%s@%s>, delay=%d, stat=%s",
			    s->msg.message_id, path->user, path->domain,
			    time(NULL) - s->msg.creation,
			    error ? error : "Sent");
			free(error);

			/* destroy session */
			LIST_REMOVE(s, entry);
			if (s->w.fd != -1)
				close(s->w.fd);
			if (s->datafp)
				fclose(s->datafp);
			msgbuf_clear(&s->w);
			event_del(&s->ev);
			free(s);

			/* update runner's session count */
			imsg_compose_event(env->sc_ievs[PROC_RUNNER],
			    IMSG_MDA_SESS_NEW, 0, 0, -1, NULL, 0);
			break;
		}

		case IMSG_CTL_VERBOSE: {
			int verbose;

			IMSG_SIZE_CHECK(&verbose);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		}

		default:
			log_warnx("mda_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mda_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mda_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_QUEUE];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mda_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("mda_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("mda_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
mda_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_RUNNER];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mda_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MDA_SESS_NEW: {
			struct deliver		 deliver;
			struct mda_session	*s;
			struct path		*path;

			/* make new session based on provided args */
			s = calloc(1, sizeof *s);
			if (s == NULL)
				fatal(NULL);
			msgbuf_init(&s->w);
			s->msg = *(struct message *)imsg.data;
			s->msg.status = S_MESSAGE_TEMPFAILURE;
			s->id = mda_id++;
			s->datafp = fdopen(imsg.fd, "r");
			if (s->datafp == NULL)
				fatalx("mda: fdopen");
			LIST_INSERT_HEAD(&env->mda_sessions, s, entry);

			/* request parent to fork a helper process */
			path = &s->msg.recipient;
			switch (path->rule.r_action) {
			case A_EXT:
				deliver.mode = A_EXT;
				strlcpy(deliver.user, path->pw_name,
				    sizeof deliver.user);
				strlcpy(deliver.to, path->rule.r_value.path,
				    sizeof deliver.to);
				break;

			case A_MBOX:
				deliver.mode = A_EXT;
				strlcpy(deliver.user, "root",
				    sizeof deliver.user);
				snprintf(deliver.to, sizeof deliver.to,
				    "%s -f %s@%s %s", PATH_MAILLOCAL,
				    s->msg.sender.user, s->msg.sender.domain,
				    path->pw_name);
				break;

			case A_MAILDIR:
				deliver.mode = A_MAILDIR;
				strlcpy(deliver.user, path->pw_name,
				    sizeof deliver.user);
				strlcpy(deliver.to, path->rule.r_value.path,
				    sizeof deliver.to);
				break;

			case A_FILENAME:
				deliver.mode = A_FILENAME;
				/* XXX: unconditional SMTPD_USER is wrong. */
				strlcpy(deliver.user, SMTPD_USER,
				    sizeof deliver.user);
				strlcpy(deliver.to, path->u.filename,
				    sizeof deliver.to);
				break;

			default:
				fatalx("unknown rule action");
			}

			imsg_compose_event(env->sc_ievs[PROC_PARENT],
			    IMSG_PARENT_FORK_MDA, s->id, 0, -1, &deliver,
			    sizeof deliver);
			break;
		}
		default:
			log_warnx("mda_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("mda_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
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
		{ PROC_PARENT,	mda_dispatch_parent },
		{ PROC_QUEUE,	mda_dispatch_queue },
		{ PROC_RUNNER,	mda_dispatch_runner }
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

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mda: chroot");
	if (chdir("/") == -1)
		fatal("mda: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_MDA;
	setproctitle("%s", env->sc_title[smtpd_process]);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mda: cannot drop privileges");
#endif

	LIST_INIT(&env->mda_sessions);

	event_init();

	signal_set(&ev_sigint, SIGINT, mda_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mda_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	mda_setup_events(env);
	event_dispatch();
	mda_shutdown();

	return (0);
}

void
mda_store(struct mda_session *s)
{
	char		*p;
	struct buf	*buf;
	int		 len;

	if (s->msg.sender.user[0] && s->msg.sender.domain[0])
		/* XXX: remove user provided Return-Path, if any */
		len = asprintf(&p, "Return-Path: %s@%s\nDelivered-To: %s@%s\n",
		    s->msg.sender.user, s->msg.sender.domain,
		    s->msg.session_rcpt.user, s->msg.session_rcpt.domain);
	else
		len = asprintf(&p, "Delivered-To: %s@%s\n",
		    s->msg.session_rcpt.user, s->msg.session_rcpt.domain);

	if (len == -1)
		fatal("mda_store: asprintf");

	session_socket_blockmode(s->w.fd, BM_NONBLOCK);
	if ((buf = buf_open(len)) == NULL)
		fatal(NULL);
	if (buf_add(buf, p, len) < 0)
		fatal(NULL);
	buf_close(&s->w, buf);
	event_set(&s->ev, s->w.fd, EV_WRITE, mda_store_event, s);
	event_add(&s->ev, NULL);
	free(p);
}

void
mda_store_event(int fd, short event, void *p)
{
	char			 tmp[16384];
	struct mda_session	*s = p;
	struct buf		*buf;
	size_t			 len;

	if (s->w.queued == 0) {
		if ((buf = buf_dynamic(0, sizeof tmp)) == NULL)
			fatal(NULL);
		len = fread(tmp, 1, sizeof tmp, s->datafp);
		if (ferror(s->datafp))
			fatal("mda_store_event: fread failed");
		if (feof(s->datafp) && len == 0) {
			close(s->w.fd);
			s->w.fd = -1;
			return;
		}
		if (buf_add(buf, tmp, len) < 0)
			fatal(NULL);
		buf_close(&s->w, buf);
	}

	if (buf_write(&s->w) < 0) {
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
