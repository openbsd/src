/*	$OpenBSD: mda.c,v 1.37 2010/03/03 10:52:31 jacekm Exp $	*/

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

#include "smtpd.h"

__dead void	mda_shutdown(void);
void		mda_sig_handler(int, short, void *);
void		mda_dispatch_parent(int, short, void *);
void		mda_dispatch_queue(int, short, void *);
void		mda_dispatch_runner(int, short, void *);
void		mda_setup_events(struct smtpd *);
void		mda_disable_events(struct smtpd *);
void		mda_store(struct batch *);
void		mda_event(int, short, void *);
void		mda_store_done(struct batch *);
void		mda_done(struct smtpd *, struct batch *);

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
		case IMSG_PARENT_MAILBOX_OPEN: {
			struct batch		*b = imsg.data;

			IMSG_SIZE_CHECK(b);

			if ((b = batch_by_id(env, b->id)) == NULL)
				fatalx("mda: internal inconsistency");

			/* parent ensures mboxfd is valid */
			if (imsg.fd == -1)
				fatalx("mda: mboxfd pass failure");

			/* got user's mbox fd */
			if ((b->mboxfp = fdopen(imsg.fd, "w")) == NULL) {
				log_warn("mda: fdopen");
				mda_done(env, b);
				break;
			}

			/* 
			 * From now on, delivery session must be deinited in
			 * the parent process as well as in mda.
			 */
			b->cleanup_parent = 1;

			/* get message content fd */
			imsg_compose_event(env->sc_ievs[PROC_PARENT],
			    IMSG_PARENT_MESSAGE_OPEN, 0, 0, -1, b,
			    sizeof(*b));
			break;
		}

		case IMSG_PARENT_MESSAGE_OPEN: {
			struct batch	*b = imsg.data;

			IMSG_SIZE_CHECK(b);

			if ((b = batch_by_id(env, b->id)) == NULL)
				fatalx("mda: internal inconsistency");

			if (imsg.fd == -1) {
				mda_done(env, b);
				break;
			}

			if ((b->datafp = fdopen(imsg.fd, "r")) == NULL) {
				log_warn("mda: fdopen");
				mda_done(env, b);
				break;
			}

			/* got message content, copy it to mbox */
			mda_store(b);
			break;
		}

		case IMSG_MDA_FINALIZE: {
			struct batch		*b = imsg.data;
			enum message_status	 status;

			IMSG_SIZE_CHECK(b);

			status = b->message.status;
			if ((b = batch_by_id(env, b->id)) == NULL)
				fatalx("mda: internal inconsistency");
			b->message.status = status;

			mda_done(env, b);
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
		case IMSG_BATCH_CREATE: {
			struct batch	*req = imsg.data;
			struct batch	*b;

			IMSG_SIZE_CHECK(req);

			/* runner opens delivery session */
			if ((b = malloc(sizeof(*b))) == NULL)
				fatal(NULL);
			*b = *req;
			msgbuf_init(&b->w);
			b->env = env;
			b->mboxfp = NULL;
			b->datafp = NULL;
			SPLAY_INSERT(batchtree, &env->batch_queue, b);
			break;
		}

		case IMSG_BATCH_APPEND: {
			struct message	*append = imsg.data;
			struct batch	*b;

			IMSG_SIZE_CHECK(append);

			/* runner submits the message to deliver */
			if ((b = batch_by_id(env, append->batch_id)) == NULL)
				fatalx("mda: internal inconsistency");
			if (b->message.message_id[0])
				fatal("mda: runner submitted extra msg");
			b->message = *append;

			/* safe default */
			b->message.status = S_MESSAGE_TEMPFAILURE;
			break;
		}

		case IMSG_BATCH_CLOSE: {
			struct batch	*b = imsg.data;

			IMSG_SIZE_CHECK(b);

			/* runner finished opening delivery session;
			 * request user's mbox fd */
			if ((b = batch_by_id(env, b->id)) == NULL)
				fatalx("mda: internal inconsistency");
			imsg_compose_event(env->sc_ievs[PROC_PARENT],
			    IMSG_PARENT_MAILBOX_OPEN, 0, 0, -1, b,
			    sizeof(*b));
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

	SPLAY_INIT(&env->batch_queue);

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
mda_store(struct batch *b)
{
	char		*p;
	struct buf	*buf;
	int		 ch, len;

	if (b->message.sender.user[0] && b->message.sender.domain[0])
		/* XXX: remove user provided Return-Path, if any */
		len = asprintf(&p, "Return-Path: %s@%s\nDelivered-To: %s@%s\n",
		    b->message.sender.user,
		    b->message.sender.domain,
		    b->message.session_rcpt.user,
		    b->message.session_rcpt.domain);
	else
		len = asprintf(&p, "Delivered-To: %s@%s\n",
		    b->message.session_rcpt.user,
		    b->message.session_rcpt.domain);

	if (len == -1)
		fatal("mda_store: asprintf");

	if (b->message.recipient.rule.r_action == A_MAILDIR) {
		fprintf(b->mboxfp, "%s", p);
		while ((ch = fgetc(b->datafp)) != EOF)
			if (fputc(ch, b->mboxfp) == EOF)
				break;
		if (ferror(b->datafp))
			fatal("mda_store: cannot read message in queue");
		if (fflush(b->mboxfp) || ferror(b->mboxfp))
			fatal("mda_store: cannot write to file");
		mda_store_done(b);
	} else {
		b->w.fd = fileno(b->mboxfp);
		session_socket_blockmode(b->w.fd, BM_NONBLOCK);
		if ((buf = buf_open(len)) == NULL)
			fatal(NULL);
		if (buf_add(buf, p, len) < 0)
			fatal(NULL);
		buf_close(&b->w, buf);
		event_set(&b->ev, b->w.fd, EV_WRITE, mda_event, b);
		event_add(&b->ev, NULL);
	}

	free(p);
}

void
mda_event(int fd, short event, void *p)
{
	char		 tmp[16384];
	struct batch	*b = p;
	struct buf	*buf;
	size_t		 len;

	if (b->w.queued == 0) {
		if ((buf = buf_dynamic(0, sizeof(tmp))) == NULL)
			fatal(NULL);
		len = fread(tmp, 1, sizeof(tmp), b->datafp);
		if (ferror(b->datafp))
			fatal("mda_event: fread failed");
		if (feof(b->datafp) && len == 0) {
			mda_store_done(b);
			return;
		}
		if (buf_add(buf, tmp, len) < 0)
			fatal(NULL);
		buf_close(&b->w, buf);
	}

	if (buf_write(&b->w) < 0) {
		/* XXX: if $? is zero, message is considered delivered despite
		 * write error. */
		log_warn("mda_event: write failed");
		mda_store_done(b);
		return;
	}

	event_set(&b->ev, fd, EV_WRITE, mda_event, b);
	event_add(&b->ev, NULL);
}

void
mda_store_done(struct batch *b)
{
	fclose(b->datafp);
	b->datafp = NULL;

	/* closing mboxfd will trigger EOF in forked mda */
	fsync(fileno(b->mboxfp));
	fclose(b->mboxfp);
	b->mboxfp = NULL;

	/* ... unless it is maildir, in which case we need to
	 * "trigger EOF" differently */
	if (b->message.recipient.rule.r_action == A_MAILDIR)
		imsg_compose_event(b->env->sc_ievs[PROC_PARENT],
		    IMSG_PARENT_MAILDIR_RENAME, 0, 0, -1, b,
		    sizeof(*b));

	/* Waiting for IMSG_MDA_FINALIZE... */
	b->cleanup_parent = 0;
}

void
mda_done(struct smtpd *env, struct batch *b)
{
	if (b->cleanup_parent) {
		/*
		 * Error has occured while both parent and mda maintain some
		 * state for this delivery session.  Need to deinit both.
		 * Deinit parent first.
		 */

		if (b->message.recipient.rule.r_action == A_MAILDIR) {
			/*
			 * In case of maildir, deiniting parent's state consists
			 * of removing the file in tmp.
			 */
			imsg_compose_event(env->sc_ievs[PROC_PARENT],
			    IMSG_PARENT_MAILDIR_FAIL, 0, 0, -1, b,
			    sizeof(*b));
		} else {
			/*
			 * In all other cases, ie. mbox and external, deiniting
			 * parent's state consists of killing its child, and
			 * freeing associated struct child.
			 *
			 * Requesting that parent does this cleanup involves
			 * racing issues.  The race-free way is to simply wait.
			 * Eventually, child timeout in parent will be hit.
			 */
		}

		/*
		 * Either way, parent will eventually send IMSG_MDA_FINALIZE.
		 * Then, the mda deinit will happen.
		 */
		b->cleanup_parent = 0;
	} else {
		/*
		 * Deinit mda.
		 */

		/* update runner (currently: via queue) */
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_QUEUE_MESSAGE_UPDATE, 0, 0, -1,
		    &b->message, sizeof(b->message));

		imsg_compose_event(env->sc_ievs[PROC_RUNNER],
		    IMSG_BATCH_DONE, 0, 0, -1, NULL, 0);

		/* log status */
		if (b->message.recipient.rule.r_action != A_MAILDIR &&
		    b->message.recipient.rule.r_action != A_MBOX) {
			log_info("%s: to=<%s@%s>, delay=%d, stat=%s",
			    b->message.message_id,
			    b->message.session_rcpt.user,
			    b->message.session_rcpt.domain,
			    time(NULL) - b->message.creation,
			    b->message.status & S_MESSAGE_PERMFAILURE ? "MdaPermError" :
			    b->message.status & S_MESSAGE_TEMPFAILURE ? "MdaTempError" :
			    "Sent");
		}
		else {
			log_info("%s: to=<%s@%s>, delay=%d, stat=%s",
			    b->message.message_id,
			    b->message.recipient.user,
			    b->message.recipient.domain,
			    time(NULL) - b->message.creation,
			    b->message.status & S_MESSAGE_PERMFAILURE ? "MdaPermError" :
			    b->message.status & S_MESSAGE_TEMPFAILURE ? "MdaTempError" :
			    "Sent");
		}

		/* deallocate resources */
		SPLAY_REMOVE(batchtree, &env->batch_queue, b);
		if (b->mboxfp)
			fclose(b->mboxfp);
		if (b->datafp)
			fclose(b->datafp);
		msgbuf_clear(&b->w);
		free(b);
	}
}
