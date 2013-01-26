/*	$OpenBSD: mda.c,v 1.85 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "smtpd.h"
#include "log.h"

#define MDA_HIWAT		65536

#define MDA_MAXEVP		5000
#define MDA_MAXEVPUSER		500
#define MDA_MAXSESS		50
#define MDA_MAXSESSUSER		7

struct mda_user {
	TAILQ_ENTRY(mda_user)	entry;
	TAILQ_ENTRY(mda_user)	entry_runnable;
	char			name[MAXLOGNAME];
	char			usertable[MAXPATHLEN];
	size_t			evpcount;
	TAILQ_HEAD(, envelope)	envelopes;
	int			runnable;
	size_t			running;
	struct userinfo		userinfo;
};

struct mda_session {
	uint64_t		 id;
	struct mda_user		*user;
	struct envelope		*evp;
	struct io		 io;
	struct iobuf		 iobuf;
	FILE			*datafp;
};

static void mda_imsg(struct mproc *, struct imsg *);
static void mda_io(struct io *, int);
static void mda_shutdown(void);
static void mda_sig_handler(int, short, void *);
static int mda_check_loop(FILE *, struct envelope *);
static int mda_getlastline(int, char *, size_t);
static void mda_done(struct mda_session *);
static void mda_fail(struct mda_user *, int, const char *);
static void mda_drain(void);
static void mda_log(const struct envelope *, const char *, const char *);

size_t			evpcount;
static struct tree	sessions;

static TAILQ_HEAD(, mda_user)	users;
static TAILQ_HEAD(, mda_user)	runnable;
size_t				running;

static void
mda_imsg(struct mproc *p, struct imsg *imsg)
{
	struct delivery_mda	*d_mda;
	struct mda_session	*s;
	struct mda_user		*u;
	struct envelope		*e;
	struct userinfo		*userinfo;
	struct deliver		 deliver;
	struct msg		 m;
	const void		*data;
	const char		*error, *parent_error;
	const char		*username, *usertable;
	uint64_t		 reqid;
	size_t			 sz;
	char			 out[256], stat[MAX_LINE_SIZE];
	int			 n, v, status;

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_USERINFO:
			m_msg(&m, imsg);
			m_get_string(&m, &usertable);
			m_get_string(&m, &username);
			m_get_int(&m, &status);
			if (status == LKA_OK)
				m_get_data(&m, &data, &sz);
			m_end(&m);

			TAILQ_FOREACH(u, &users, entry)
				if (!strcmp(username, u->name) &&
				    !strcmp(usertable, u->usertable))
					break;
			if (u == NULL)
				return;

			if (status == LKA_TEMPFAIL)
				mda_fail(u, 0,
				    "Temporary failure in user lookup");
			else if (status == LKA_PERMFAIL)
				mda_fail(u, 1,
				    "Permanent failure in user lookup");
			else {
				memmove(&u->userinfo, data, sz);
				u->runnable = 1;
				TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
				mda_drain();
			}
			return;
		}
	}

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {

		case IMSG_MDA_DELIVER:
			e = xmalloc(sizeof(*e), "mda:envelope");
			m_msg(&m, imsg);
			m_get_envelope(&m, e);
			m_end(&m);

			if (evpcount >= MDA_MAXEVP) {
				log_debug("debug: mda: too many envelopes");
				queue_tempfail(e->id,
				    "Global envelope limit reached");
				mda_log(e, "TempFail",
				    "Global envelope limit reached");
				free(e);
				return;
			}

			username = e->agent.mda.username;
			usertable = e->agent.mda.usertable;
			TAILQ_FOREACH(u, &users, entry)
			    if (!strcmp(username, u->name) &&
				!strcmp(usertable, u->usertable))
					break;

			if (u && u->evpcount >= MDA_MAXEVPUSER) {
				log_debug("debug: mda: too many envelopes for "
				    "\"%s\"", u->name);
				queue_tempfail(e->id,
				    "User envelope limit reached");
				mda_log(e, "TempFail",
				    "User envelope limit reached");
				free(e);
				return;
			}

			if (u == NULL) {
				u = xcalloc(1, sizeof *u, "mda_user");
				TAILQ_INIT(&u->envelopes);
				strlcpy(u->name, username, sizeof u->name);
				strlcpy(u->usertable, usertable, sizeof u->usertable);
				TAILQ_INSERT_TAIL(&users, u, entry);
				m_create(p_lka, IMSG_LKA_USERINFO, 0, 0, -1,
				    32 + strlen(usertable) + strlen(username));
				m_add_string(p_lka, usertable);
				m_add_string(p_lka, username);
				m_close(p_lka);
			}

			stat_increment("mda.pending", 1);

			evpcount += 1;
			u->evpcount += 1;
			TAILQ_INSERT_TAIL(&u->envelopes, e, entry);
			mda_drain();
			return;

		case IMSG_QUEUE_MESSAGE_FD:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);

			s = tree_xget(&sessions, reqid);
			e = s->evp;

			if (imsg->fd == -1) {
				log_debug("debug: mda: cannot get message fd");
				queue_tempfail(s->evp->id,
				    "Cannot get message fd");
				mda_log(e, "TempFail",
				    "Cannot get messafe fd");
				mda_done(s);
				return;
			}

			if ((s->datafp = fdopen(imsg->fd, "r")) == NULL) {
				log_warn("warn: mda: fdopen");
				queue_tempfail(s->evp->id, "fdopen failed");
				mda_log(e, "TempFail", "fdopen failed");
				mda_done(s);
				return;
			}

			/* check delivery loop */
			if (mda_check_loop(s->datafp, s->evp)) {
				log_debug("debug: mda: loop detected");
				queue_loop(s->evp->id);
				mda_log(e, "PermFail", "Loop detected");
				mda_done(s);
				return;
			}

			/* start queueing delivery headers */
			if (s->evp->sender.user[0] && s->evp->sender.domain[0])
				/* XXX: remove exising Return-Path, if any */
				n = iobuf_fqueue(&s->iobuf,
				    "Return-Path: %s@%s\nDelivered-To: %s@%s\n",
				    s->evp->sender.user, s->evp->sender.domain,
				    s->evp->rcpt.user,
				    s->evp->rcpt.domain);
			else
				n = iobuf_fqueue(&s->iobuf,
				    "Delivered-To: %s@%s\n",
				    s->evp->rcpt.user,
				    s->evp->rcpt.domain);
			if (n == -1) {
				log_warn("warn: mda: "
				    "fail to write delivery info");
				queue_tempfail(s->evp->id, "Out of memory");
				mda_log(s->evp, "TempFail", "Out of memory");
				mda_done(s);
				return;
			}

			/* request parent to fork a helper process */
			d_mda = &s->evp->agent.mda;
			userinfo = &s->user->userinfo;
			switch (d_mda->method) {
			case A_MDA:
				deliver.mode = A_MDA;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				strlcpy(deliver.to, d_mda->buffer,
				    sizeof(deliver.to));
				break;

			case A_MBOX:
				/* MBOX is a special case as we MUST deliver as root,
				 * just override the uid.
				 */
				deliver.mode = A_MBOX;
				deliver.userinfo = *userinfo;
				deliver.userinfo.uid = 0;
				strlcpy(deliver.user, "root",
				    sizeof(deliver.user));
				strlcpy(deliver.to, userinfo->username,
				    sizeof(deliver.to));
				snprintf(deliver.from, sizeof(deliver.from),
				    "%s@%s",
				    s->evp->sender.user,
				    s->evp->sender.domain);
				break;

			case A_MAILDIR:
				deliver.mode = A_MAILDIR;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				strlcpy(deliver.to, d_mda->buffer,
				    sizeof(deliver.to));
				break;

			case A_FILENAME:
				deliver.mode = A_FILENAME;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof deliver.user);
				strlcpy(deliver.to, d_mda->buffer,
				    sizeof deliver.to);
				break;

			default:
				errx(1, "mda: unknown delivery method: %d",
				    d_mda->method);
			}

			m_create(p_parent, IMSG_PARENT_FORK_MDA, 0, 0, -1,
			    32 + sizeof(deliver));
			m_add_id(p_parent, reqid);
			m_add_data(p_parent, &deliver, sizeof(deliver));
			m_close(p_parent);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORK_MDA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);

			s = tree_xget(&sessions, reqid);
			e = s->evp;
			if (imsg->fd == -1) {
				log_warn("warn: mda: fail to retreive mda fd");
				queue_tempfail(s->evp->id, "Cannot get mda fd");
				mda_log(e, "TempFail", "Cannot get mda fd");
				mda_done(s);
				return;
			}

			io_set_blocking(imsg->fd, 0);
			io_init(&s->io, imsg->fd, s, mda_io, &s->iobuf);
			io_set_write(&s->io);
			return;

		case IMSG_MDA_DONE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &parent_error);
			m_end(&m);

			s = tree_xget(&sessions, reqid);
			/*
			 * Grab last line of mda stdout/stderr if available.
			 */
			out[0] = '\0';
			if (imsg->fd != -1)
				mda_getlastline(imsg->fd, out, sizeof(out));
			/*
			 * Choose between parent's description of error and
			 * child's output, the latter having preference over
			 * the former.
			 */
			error = NULL;
			if (strcmp(parent_error, "exited okay") == 0) {
				if (s->datafp || iobuf_queued(&s->iobuf))
					error = "mda exited prematurely";
			} else
				error = out[0] ? out : parent_error;

			/* update queue entry */
			if (error) {
				queue_tempfail(s->evp->id, error);
				snprintf(stat, sizeof stat, "Error (%s)",
				    error);
				mda_log(s->evp, "TempFail", stat);
			}
			else {
				queue_ok(s->evp->id);
				mda_log(s->evp, "Ok", "Delivered");
			}
			mda_done(s);
			return;

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

	errx(1, "mda_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
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

static void
mda_shutdown(void)
{
	log_info("info: mail delivery agent exiting");
	_exit(0);
}

pid_t
mda(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("mda: cannot fork");
	case 0:
		env->sc_pid = getpid();
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(pw->pw_dir) == -1)
		fatal("mda: chroot");
	if (chdir("/") == -1)
		fatal("mda: chdir(\"/\")");

	config_process(PROC_MDA);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mda: cannot drop privileges");

	tree_init(&sessions);
	TAILQ_INIT(&users);
	TAILQ_INIT(&runnable);
	running = 0;

	imsg_callback = mda_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, mda_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, mda_sig_handler, NULL);
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
	mda_shutdown();

	return (0);
}

static void
mda_io(struct io *io, int evt)
{
	struct mda_session	*s = io->arg;
	char			*ln;
	size_t			 len;

	log_trace(TRACE_IO, "mda: %p: %s %s", s, io_strevent(evt),
	    io_strio(io));

	switch (evt) {
	case IO_LOWAT:

		/* done */
		if (s->datafp == NULL) {
			io_clear(io);
			return;
		}

		while (iobuf_queued(&s->iobuf) < MDA_HIWAT) {
			if ((ln = fgetln(s->datafp, &len)) == NULL)
				break;
			if (iobuf_queue(&s->iobuf, ln, len) == -1) {
				m_create(p_parent, IMSG_PARENT_KILL_MDA,
				    0, 0, -1, 128);
				m_add_id(p_parent, s->id);
				m_add_string(p_parent, "Out of memory");
				m_close(p_parent);
				io_pause(io, IO_PAUSE_OUT);
				return;
			}
		}

		if (ferror(s->datafp)) {
			log_debug("debug: mda_io: %p: ferror", s);
			m_create(p_parent, IMSG_PARENT_KILL_MDA, 0, 0, -1, 128);
			m_add_id(p_parent, s->id);
			m_add_string(p_parent, "Error reading body");
			m_close(p_parent);
			io_pause(io, IO_PAUSE_OUT);
			return;
		}

		if (feof(s->datafp)) {
			fclose(s->datafp);
			s->datafp = NULL;
		}
		return;

	case IO_TIMEOUT:
		log_debug("debug: mda_io: timeout");
		io_pause(io, IO_PAUSE_OUT);
		return;

	case IO_ERROR:
		log_debug("debug: mda_io: io error: %s", io->error);
		io_pause(io, IO_PAUSE_OUT);
		return;

	case IO_DISCONNECTED:
		log_debug("debug: mda_io: disconnected");
		io_pause(io, IO_PAUSE_OUT);
		return;

	default:
		log_debug("debug: mda_io: unexpected io event: %i", evt);
		io_pause(io, IO_PAUSE_OUT);
		return;
	}
}

static int
mda_check_loop(FILE *fp, struct envelope *ep)
{
	char		*buf, *lbuf;
	size_t		 len;
	struct mailaddr	 maddr;
	int		 ret = 0;

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			/* EOF without EOL, copy and add the NUL */
			lbuf = xmalloc(len + 1, "mda_check_loop");
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (strchr(buf, ':') == NULL && !isspace((int)*buf))
			break;

		if (strncasecmp("Delivered-To: ", buf, 14) == 0) {

			bzero(&maddr, sizeof maddr);
			if (! text_to_mailaddr(&maddr, buf + 14))
				continue;
			if (strcasecmp(maddr.user, ep->dest.user) == 0 &&
			    strcasecmp(maddr.domain, ep->dest.domain) == 0) {
				ret = 1;
				break;
			}
		}
		if (lbuf) {
			free(lbuf);
			lbuf = NULL;
		}
	}
	if (lbuf)
		free(lbuf);

	fseek(fp, SEEK_SET, 0);

	return (ret);
}

static int
mda_getlastline(int fd, char *dst, size_t dstsz)
{
	FILE	*fp;
	char	*ln, buf[MAX_LINE_SIZE];
	size_t	 len;

	bzero(buf, sizeof buf);
	if (lseek(fd, 0, SEEK_SET) < 0) {
		log_warn("warn: mda: lseek");
		close(fd);
		return (-1);
	}
	fp = fdopen(fd, "r");
	if (fp == NULL) {
		log_warn("warn: mda: fdopen");
		close(fd);
		return (-1);
	}
	while ((ln = fgetln(fp, &len))) {
		if (ln[len - 1] == '\n')
			len--;
		if (len == 0)
			continue;
		if (len >= sizeof buf)
			len = (sizeof buf) - 1;
		memmove(buf, ln, len);
		buf[len] = '\0';
	}
	fclose(fp);

	if (buf[0]) {
		strlcpy(dst, "\"", dstsz);
		strnvis(dst + 1, buf, dstsz - 2, VIS_SAFE | VIS_CSTYLE);
		strlcat(dst, "\"", dstsz);
	}

	return (0);
}

static void
mda_fail(struct mda_user *user, int permfail, const char *error)
{
	struct envelope	*e;

	while ((e = TAILQ_FIRST(&user->envelopes))) {
		TAILQ_REMOVE(&user->envelopes, e, entry);
		if (permfail) {
			mda_log(e, "PermFail", error);
			queue_permfail(e->id, error);
		}
		else {
			mda_log(e, "TempFail", error);
			queue_tempfail(e->id, error);
		}
		free(e);
	}

	TAILQ_REMOVE(&users, user, entry);
	free(user);
}

static void
mda_drain(void)
{
	struct mda_session	*s;
	struct mda_user		*user;

	while ((user = (TAILQ_FIRST(&runnable)))) {

		if (running >= MDA_MAXSESS) {
			log_debug("debug: mda: "
			    "maximum number of session reached");
			return;
		}

		log_debug("debug: mda: new session for user \"%s\"",
		    user->name);

		s = xcalloc(1, sizeof *s, "mda_drain");
		s->user = user;
		s->evp = TAILQ_FIRST(&user->envelopes);
		TAILQ_REMOVE(&user->envelopes, s->evp, entry);
		s->id = generate_uid();
		if (iobuf_init(&s->iobuf, 0, 0) == -1)
			fatal("mda_drain");
		s->io.sock = -1;
		tree_xset(&sessions, s->id, s);

		m_create(p_queue, IMSG_QUEUE_MESSAGE_FD, 0, 0, -1, 18);
		m_add_id(p_queue, s->id);
		m_add_msgid(p_queue, evpid_to_msgid(s->evp->id));
		m_close(p_queue);

		stat_decrement("mda.pending", 1);

		user->evpcount--;
		evpcount--;

		stat_increment("mda.running", 1);

		user->running++;
		running++;

		/*
		 * The user is still runnable if there are pending envelopes
		 * and the session limit is not reached. We put it at the tail
		 * so that everyone gets a fair share.
		 */
		TAILQ_REMOVE(&runnable, user, entry_runnable);
		user->runnable = 0;
		if (TAILQ_FIRST(&user->envelopes) &&
		    user->running < MDA_MAXSESSUSER) {
			TAILQ_INSERT_TAIL(&runnable, user, entry_runnable);
			user->runnable = 1;
			log_debug("debug: mda: user \"%s\" still runnable",
			    user->name);
		}
	}
}

static void
mda_done(struct mda_session *s)
{
	tree_xpop(&sessions, s->id);

	running--;
	s->user->running--;

	stat_decrement("mda.running", 1);

	if (TAILQ_FIRST(&s->user->envelopes) == NULL && s->user->running == 0) {
		log_debug("debug: mda: "
		    "all done for user \"%s\"", s->user->name);
		TAILQ_REMOVE(&users, s->user, entry);
		free(s->user);
	} else if (s->user->runnable == 0 &&
	    TAILQ_FIRST(&s->user->envelopes) &&
	    s->user->running < MDA_MAXSESSUSER) {
		log_debug("debug: mda: user \"%s\" becomes runnable",
		    s->user->name);
		TAILQ_INSERT_TAIL(&runnable, s->user, entry_runnable);
		s->user->runnable = 1;
	}

	if (s->datafp)
		fclose(s->datafp);
	io_clear(&s->io);
	iobuf_clear(&s->iobuf);
	free(s->evp);
	free(s);

	mda_drain();
}

static void
mda_log(const struct envelope *evp, const char *prefix, const char *status)
{
	char rcpt[MAX_LINE_SIZE];
	const char *method;

	rcpt[0] = '\0';
	if (strcmp(evp->rcpt.user, evp->dest.user) ||
	    strcmp(evp->rcpt.domain, evp->dest.domain))
		snprintf(rcpt, sizeof rcpt, "rcpt=<%s@%s>, ",
		    evp->rcpt.user, evp->rcpt.domain);

	if (evp->agent.mda.method == A_MAILDIR)
		method = "maildir";
	else if (evp->agent.mda.method == A_MBOX)
		method = "mbox";
	else if (evp->agent.mda.method == A_FILENAME)
		method = "file";
	else if (evp->agent.mda.method == A_MDA)
		method = "mda";
	else
		method = "???";

	log_info("delivery: %s for %016" PRIx64 ": from=<%s@%s>, to=<%s@%s>, "
	    "%suser=%s, method=%s, delay=%s, stat=%s",
	    prefix,
	    evp->id, evp->sender.user, evp->sender.domain,
	    evp->dest.user, evp->dest.domain,
	    rcpt,
	    evp->agent.mda.username, method,
	    duration_to_text(time(NULL) - evp->creation),
	    status);
}
