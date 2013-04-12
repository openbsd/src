/*	$OpenBSD: mda.c,v 1.90 2013/04/12 18:22:49 eric Exp $	*/

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

#define MDA_MAXEVP		200000
#define MDA_MAXEVPUSER		15000
#define MDA_MAXSESS		50
#define MDA_MAXSESSUSER		7

struct mda_envelope {
	TAILQ_ENTRY(mda_envelope)	 entry;
	uint64_t			 id;
	time_t				 creation;
	char				*sender;
	char				*dest;
	char				*rcpt;
	enum action_type		 method;
	char				*user;
	char				*buffer;
};

#define FLAG_USER_WAITINFO	0x01
#define FLAG_USER_RUNNABLE	0x02

struct mda_user {
	TAILQ_ENTRY(mda_user)		entry;
	TAILQ_ENTRY(mda_user)		entry_runnable;
	char				name[MAXLOGNAME];
	char				usertable[MAXPATHLEN];
	size_t				evpcount;
	TAILQ_HEAD(, mda_envelope)	envelopes;
	int				flags;
	size_t				running;
	struct userinfo			userinfo;
};

struct mda_session {
	uint64_t		 id;
	struct mda_user		*user;
	struct mda_envelope	*evp;
	struct io		 io;
	struct iobuf		 iobuf;
	FILE			*datafp;
};

static void mda_imsg(struct mproc *, struct imsg *);
static void mda_io(struct io *, int);
static void mda_shutdown(void);
static void mda_sig_handler(int, short, void *);
static int mda_check_loop(FILE *, struct mda_envelope *);
static int mda_getlastline(int, char *, size_t);
static void mda_done(struct mda_session *);
static void mda_fail(struct mda_user *, int, const char *);
static void mda_drain(void);
static void mda_log(const struct mda_envelope *, const char *, const char *);

size_t			evpcount;
static struct tree	sessions;

static TAILQ_HEAD(, mda_user)	users;
static TAILQ_HEAD(, mda_user)	runnable;
size_t				running;

static void
mda_imsg(struct mproc *p, struct imsg *imsg)
{
	struct mda_session	*s;
	struct mda_user		*u;
	struct mda_envelope	*e;
	struct envelope		 evp;
	struct userinfo		*userinfo;
	struct deliver		 deliver;
	struct msg		 m;
	const void		*data;
	const char		*error, *parent_error;
	const char		*username, *usertable;
	uint64_t		 reqid;
	size_t			 sz;
	char			 out[256], buf[SMTPD_MAXLINESIZE];
	int			 n, v;
	enum lka_resp_status	status;

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_USERINFO:
			m_msg(&m, imsg);
			m_get_string(&m, &usertable);
			m_get_string(&m, &username);
			m_get_int(&m, (int *)&status);
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
				u->flags &= ~FLAG_USER_WAITINFO;
				u->flags |= FLAG_USER_RUNNABLE;
				TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
				mda_drain();
			}
			return;
		}
	}

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {

		case IMSG_MDA_DELIVER:
			m_msg(&m, imsg);
			m_get_envelope(&m, &evp);
			m_end(&m);

			e = xcalloc(1, sizeof *e, "mda_envelope");
			e->id = evp.id;
			e->creation = evp.creation;
			buf[0] = '\0';
			if (evp.sender.user[0] && evp.sender.domain[0])
				snprintf(buf, sizeof buf, "%s@%s",
				    evp.sender.user, evp.sender.domain);
			e->sender = xstrdup(buf, "mda_envelope:sender");
			snprintf(buf, sizeof buf, "%s@%s",
			    evp.dest.user, evp.dest.domain);
			e->dest = xstrdup(buf, "mda_envelope:dest");
			snprintf(buf, sizeof buf, "%s@%s",
			    evp.rcpt.user, evp.rcpt.domain);
			if (strcmp(buf, e->dest))
				e->rcpt = xstrdup(buf, "mda_envelope:rcpt");
			e->method = evp.agent.mda.method;
			e->buffer = xstrdup(evp.agent.mda.buffer,
			    "mda_envelope:buffer");
			e->user = xstrdup(evp.agent.mda.username,
			    "mda_envelope:user");

			if (evpcount >= MDA_MAXEVP) {
				log_debug("debug: mda: too many envelopes");
				queue_tempfail(e->id,
				    "Too many envelopes in the delivery agent: "
				    "will try again later");
				mda_log(e, "TempFail",
				    "Too many envelopes in the delivery agent: "
				    "will try again later");
				free(e->sender);
				free(e->dest);
				free(e->rcpt);
				free(e->user);
				free(e->buffer);
				free(e);
				return;
			}

			username = evp.agent.mda.username;
			usertable = evp.agent.mda.usertable;
			TAILQ_FOREACH(u, &users, entry)
			    if (!strcmp(username, u->name) &&
				!strcmp(usertable, u->usertable))
					break;

			if (u == NULL) {
				u = xcalloc(1, sizeof *u, "mda_user");
				TAILQ_INSERT_TAIL(&users, u, entry);
				TAILQ_INIT(&u->envelopes);
				strlcpy(u->name, username, sizeof u->name);
				strlcpy(u->usertable, usertable, sizeof u->usertable);
				u->flags |= FLAG_USER_WAITINFO;
				m_create(p_lka, IMSG_LKA_USERINFO, 0, 0, -1,
				    32 + strlen(usertable) + strlen(username));
				m_add_string(p_lka, usertable);
				m_add_string(p_lka, username);
				m_close(p_lka);
				stat_increment("mda.user", 1);
			} else if (u->evpcount >= MDA_MAXEVPUSER) {
				log_debug("debug: mda: too many envelopes for "
				    "\"%s\"", u->name);
				queue_tempfail(e->id,
				    "Too many envelopes for this user in the "
				    "delivery agent: will try again later");
				mda_log(e, "TempFail",
				    "Too many envelopes for this user in the "
				    "delivery agent: will try again later");
				free(e->sender);
				free(e->dest);
				free(e->rcpt);
				free(e->user);
				free(e->buffer);
				free(e);
				return;
			} else if (!(u->flags & FLAG_USER_RUNNABLE) &&
				   !(u->flags & FLAG_USER_WAITINFO)) {
				u->flags |= FLAG_USER_RUNNABLE;
				TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
			}

			stat_increment("mda.envelope", 1);
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
				queue_tempfail(e->id, "Cannot get message fd");
				mda_log(e, "TempFail", "Cannot get message fd");
				mda_done(s);
				return;
			}

			log_debug("debug: mda: got message fd %i "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    imsg->fd, s->id, e->id);

			if ((s->datafp = fdopen(imsg->fd, "r")) == NULL) {
				log_warn("warn: mda: fdopen");
				queue_tempfail(e->id, "fdopen failed");
				mda_log(e, "TempFail", "fdopen failed");
				mda_done(s);
				close(imsg->fd);
				return;
			}

			/* check delivery loop */
			if (mda_check_loop(s->datafp, e)) {
				log_debug("debug: mda: loop detected");
				queue_loop(e->id);
				mda_log(e, "PermFail", "Loop detected");
				mda_done(s);
				return;
			}

			/* start queueing delivery headers */
			if (e->sender[0])
				/* XXX: remove exising Return-Path, if any */
				n = iobuf_fqueue(&s->iobuf,
				    "Return-Path: %s\nDelivered-To: %s\n",
				    e->sender, e->rcpt ? e->rcpt : e->dest);
			else
				n = iobuf_fqueue(&s->iobuf,
				    "Delivered-To: %s\n",
				    e->rcpt ? e->rcpt : e->dest);
			if (n == -1) {
				log_warn("warn: mda: "
				    "fail to write delivery info");
				queue_tempfail(e->id, "Out of memory");
				mda_log(e, "TempFail", "Out of memory");
				mda_done(s);
				return;
			}

			/* request parent to fork a helper process */
			userinfo = &s->user->userinfo;
			switch (e->method) {
			case A_MDA:
				deliver.mode = A_MDA;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				strlcpy(deliver.to, e->buffer,
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
				strlcpy(deliver.from, e->sender,
				    sizeof(deliver.from));
				break;

			case A_MAILDIR:
				deliver.mode = A_MAILDIR;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				strlcpy(deliver.to, e->buffer,
				    sizeof(deliver.to));
				break;

			case A_FILENAME:
				deliver.mode = A_FILENAME;
				deliver.userinfo = *userinfo;
				strlcpy(deliver.user, userinfo->username,
				    sizeof deliver.user);
				strlcpy(deliver.to, e->buffer,
				    sizeof deliver.to);
				break;

			default:
				errx(1, "mda: unknown delivery method: %d",
				    e->method);
			}

			log_debug("debug: mda: querying mda fd "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    s->id, s->evp->id);

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
				log_warn("warn: mda: fail to retrieve mda fd");
				queue_tempfail(e->id, "Cannot get mda fd");
				mda_log(e, "TempFail", "Cannot get mda fd");
				mda_done(s);
				return;
			}

			log_debug("debug: mda: got mda fd %i "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    imsg->fd, s->id, s->evp->id);

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
			e = s->evp;
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
				queue_tempfail(e->id, error);
				snprintf(buf, sizeof buf, "Error (%s)", error);
				mda_log(e, "TempFail", buf);
			}
			else {
				queue_ok(e->id);
				mda_log(e, "Ok", "Delivered");
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
	evpcount = 0;
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
			log_debug("debug: mda: all data sent for session"
			    " %016"PRIx64 " evpid %016"PRIx64,
			    s->id, s->evp->id);
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
#if 0
			log_debug("debug: mda: %zu bytes queued "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    iobuf_queued(&s->iobuf), s->id, s->evp->id);
#endif
		}

		if (ferror(s->datafp)) {
			log_debug("debug: mda: ferror on session %016"PRIx64,
			    s->id);
			m_create(p_parent, IMSG_PARENT_KILL_MDA, 0, 0, -1, 128);
			m_add_id(p_parent, s->id);
			m_add_string(p_parent, "Error reading body");
			m_close(p_parent);
			io_pause(io, IO_PAUSE_OUT);
			return;
		}

		if (feof(s->datafp)) {
			log_debug("debug: mda: end-of-file for session"
			    " %016"PRIx64 " evpid %016"PRIx64,
			    s->id, s->evp->id);
			fclose(s->datafp);
			s->datafp = NULL;
		}
		return;

	case IO_TIMEOUT:
		log_debug("debug: mda: timeout on session %016"PRIx64, s->id);
		io_pause(io, IO_PAUSE_OUT);
		return;

	case IO_ERROR:
		log_debug("debug: mda: io error on session %016"PRIx64": %s",
		    s->id, io->error);
		io_pause(io, IO_PAUSE_OUT);
		return;

	case IO_DISCONNECTED:
		log_debug("debug: mda: io disconnected on session %016"PRIx64,
		    s->id);
		io_pause(io, IO_PAUSE_OUT);
		return;

	default:
		log_debug("debug: mda: unexpected event on session %016"PRIx64,
		    s->id);
		io_pause(io, IO_PAUSE_OUT);
		return;
	}
}

static int
mda_check_loop(FILE *fp, struct mda_envelope *e)
{
	char		*buf, *lbuf;
	size_t		 len;
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
			if (strcasecmp(buf + 14, e->dest) == 0) {
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
	char	*ln, buf[SMTPD_MAXLINESIZE];
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
	struct mda_envelope	*e;

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
		free(e->sender);
		free(e->dest);
		free(e->rcpt);
		free(e->user);
		free(e->buffer);
		free(e);
		stat_decrement("mda.envelope", 1);
	}

	TAILQ_REMOVE(&users, user, entry);
	free(user);
	stat_decrement("mda.user", 1);
}

static void
mda_drain(void)
{
	struct mda_session	*s;
	struct mda_user		*u;

	while ((u = (TAILQ_FIRST(&runnable)))) {
		TAILQ_REMOVE(&runnable, u, entry_runnable);

		if (u->evpcount == 0 && u->running == 0) {
			log_debug("debug: mda: all done for user \"%s\"",
			    u->name);
			TAILQ_REMOVE(&users, u, entry);
			free(u);
			stat_decrement("mda.user", 1);
			continue;
		}

		if (u->evpcount == 0) {
			log_debug("debug: mda: no more envelope for \"%s\"",
			    u->name);
			u->flags &= ~FLAG_USER_RUNNABLE;
			continue;
		}

		if (u->running >= MDA_MAXSESSUSER) {
			log_debug("debug: mda: "
			    "maximum number of session reached for user \"%s\"",
			    u->name);
			u->flags &= ~FLAG_USER_RUNNABLE;
			continue;
		}

		if (running >= MDA_MAXSESS) {
			log_debug("debug: mda: "
			    "maximum number of session reached");
			TAILQ_INSERT_HEAD(&runnable, u, entry_runnable);
			return;
		}

		s = xcalloc(1, sizeof *s, "mda_drain");
		s->user = u;
		s->evp = TAILQ_FIRST(&u->envelopes);
		TAILQ_REMOVE(&u->envelopes, s->evp, entry);
		s->id = generate_uid();
		if (iobuf_init(&s->iobuf, 0, 0) == -1)
			fatal("mda_drain");
		s->io.sock = -1;
		tree_xset(&sessions, s->id, s);

		log_debug("debug: mda: new session %016" PRIx64
		    " for user \"%s\" evpid %016" PRIx64, s->id, u->name,
		    s->evp->id);

		m_create(p_queue, IMSG_QUEUE_MESSAGE_FD, 0, 0, -1, 18);
		m_add_id(p_queue, s->id);
		m_add_msgid(p_queue, evpid_to_msgid(s->evp->id));
		m_close(p_queue);

		evpcount--;
		u->evpcount--;
		stat_decrement("mda.pending", 1);

		running++;
		u->running++;
		stat_increment("mda.running", 1);

		/* Re-add the user at the tail of the queue */
		TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
	}
}

static void
mda_done(struct mda_session *s)
{
	struct mda_user	*u;

	tree_xpop(&sessions, s->id);

	log_debug("debug: mda: session %016" PRIx64 " done", s->id);

	u = s->user;

	running--;
	u->running--;
	stat_decrement("mda.running", 1);

	if (s->datafp)
		fclose(s->datafp);
	io_clear(&s->io);
	iobuf_clear(&s->iobuf);

	free(s->evp->sender);
	free(s->evp->dest);
	free(s->evp->rcpt);
	free(s->evp->user);
	free(s->evp->buffer);
	free(s->evp);
	free(s);
	stat_decrement("mda.envelope", 1);

	if (!(u->flags & FLAG_USER_RUNNABLE)) {
		log_debug("debug: mda: user \"%s\" becomes runnable", u->name);
		TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
		u->flags |= FLAG_USER_RUNNABLE;
	}

	mda_drain();
}

static void
mda_log(const struct mda_envelope *evp, const char *prefix, const char *status)
{
	char rcpt[SMTPD_MAXLINESIZE];
	const char *method;

	rcpt[0] = '\0';
	if (evp->rcpt)
		snprintf(rcpt, sizeof rcpt, "rcpt=<%s>, ", evp->rcpt);

	if (evp->method == A_MAILDIR)
		method = "maildir";
	else if (evp->method == A_MBOX)
		method = "mbox";
	else if (evp->method == A_FILENAME)
		method = "file";
	else if (evp->method == A_MDA)
		method = "mda";
	else
		method = "???";

	log_info("delivery: %s for %016" PRIx64 ": from=<%s>, to=<%s>, "
	    "%suser=%s, method=%s, delay=%s, stat=%s",
	    prefix,
	    evp->id,
	    evp->sender ? evp->sender : "",
	    evp->dest,
	    rcpt,
	    evp->user,
	    method,
	    duration_to_text(time(NULL) - evp->creation),
	    status);
}
