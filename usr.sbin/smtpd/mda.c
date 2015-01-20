/*	$OpenBSD: mda.c,v 1.109 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <limits.h>
#include <vis.h>

#include "smtpd.h"
#include "log.h"

#define MDA_HIWAT		65536

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

#define USER_WAITINFO	0x01
#define USER_RUNNABLE	0x02
#define USER_ONHOLD	0x04
#define USER_HOLDQ	0x08

struct mda_user {
	uint64_t			id;
	TAILQ_ENTRY(mda_user)		entry;
	TAILQ_ENTRY(mda_user)		entry_runnable;
	char				name[LOGIN_NAME_MAX];
	char				usertable[PATH_MAX];
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

static void mda_io(struct io *, int);
static int mda_check_loop(FILE *, struct mda_envelope *);
static int mda_getlastline(int, char *, size_t);
static void mda_done(struct mda_session *);
static void mda_fail(struct mda_user *, int, const char *, enum enhanced_status_code);
static void mda_drain(void);
static void mda_log(const struct mda_envelope *, const char *, const char *);
static void mda_queue_ok(uint64_t);
static void mda_queue_tempfail(uint64_t, const char *, enum enhanced_status_code);
static void mda_queue_permfail(uint64_t, const char *, enum enhanced_status_code);
static void mda_queue_loop(uint64_t);
static struct mda_user *mda_user(const struct envelope *);
static void mda_user_free(struct mda_user *);
static const char *mda_user_to_text(const struct mda_user *);
static struct mda_envelope *mda_envelope(const struct envelope *);
static void mda_envelope_free(struct mda_envelope *);
static struct mda_session * mda_session(struct mda_user *);

static struct tree	sessions;
static struct tree	users;

static TAILQ_HEAD(, mda_user)	runnable;

void
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
	uint64_t		 reqid;
	time_t			 now;
	size_t			 sz;
	char			 out[256], buf[LINE_MAX];
	int			 n;
	enum lka_resp_status	status;

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_MDA_LOOKUP_USERINFO:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_int(&m, (int *)&status);
			if (status == LKA_OK)
				m_get_data(&m, &data, &sz);
			m_end(&m);

			u = tree_xget(&users, reqid);

			if (status == LKA_TEMPFAIL)
				mda_fail(u, 0,
				    "Temporary failure in user lookup",
				    ESC_OTHER_ADDRESS_STATUS);
			else if (status == LKA_PERMFAIL)
				mda_fail(u, 1,
				    "Permanent failure in user lookup",
				    ESC_DESTINATION_MAILBOX_HAS_MOVED);
			else {
				memmove(&u->userinfo, data, sz);
				u->flags &= ~USER_WAITINFO;
				u->flags |= USER_RUNNABLE;
				TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
				mda_drain();
			}
			return;
		}
	}

	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {

		case IMSG_QUEUE_DELIVER:
			m_msg(&m, imsg);
			m_get_envelope(&m, &evp);
			m_end(&m);

			u = mda_user(&evp);

			if (u->evpcount >= env->sc_mda_task_hiwat) {
				if (!(u->flags & USER_ONHOLD)) {
					log_debug("debug: mda: hiwat reached for "
					    "user \"%s\": holding envelopes",
					    mda_user_to_text(u));
					u->flags |= USER_ONHOLD;
				}
			}

			if (u->flags & USER_ONHOLD) {
				u->flags |= USER_HOLDQ;
				m_create(p_queue, IMSG_MDA_DELIVERY_HOLD, 0, 0, -1);
				m_add_evpid(p_queue, evp.id);
				m_add_id(p_queue, u->id);
				m_close(p_queue);
				return;
			}

			e = mda_envelope(&evp);
			TAILQ_INSERT_TAIL(&u->envelopes, e, entry);
			u->evpcount += 1;
			stat_increment("mda.pending", 1);

			if (!(u->flags & USER_RUNNABLE) &&
			    !(u->flags & USER_WAITINFO)) {
				u->flags |= USER_RUNNABLE;
				TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
			}

			mda_drain();
			return;

		case IMSG_MDA_OPEN_MESSAGE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);

			s = tree_xget(&sessions, reqid);
			e = s->evp;

			if (imsg->fd == -1) {
				log_debug("debug: mda: cannot get message fd");
				mda_queue_tempfail(e->id, "Cannot get message fd",
				    ESC_OTHER_MAIL_SYSTEM_STATUS);
				mda_log(e, "TempFail", "Cannot get message fd");
				mda_done(s);
				return;
			}

			log_debug("debug: mda: got message fd %d "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    imsg->fd, s->id, e->id);

			if ((s->datafp = fdopen(imsg->fd, "r")) == NULL) {
				log_warn("warn: mda: fdopen");
				close(imsg->fd);
				mda_queue_tempfail(e->id, "fdopen failed",
				    ESC_OTHER_MAIL_SYSTEM_STATUS);
				mda_log(e, "TempFail", "fdopen failed");
				mda_done(s);
				return;
			}

			/* check delivery loop */
			if (mda_check_loop(s->datafp, e)) {
				log_debug("debug: mda: loop detected");
				mda_queue_loop(e->id);
				mda_log(e, "PermFail", "Loop detected");
				mda_done(s);
				return;
			}

			n = 0;
			/* prepend "From " separator ... for A_MDA and A_FILENAME backends only */
			if (e->method == A_MDA || e->method == A_FILENAME) {
				time(&now);
				if (e->sender[0])
					n = iobuf_fqueue(&s->iobuf,
					    "From %s %s", e->sender, ctime(&now));
				else
					n = iobuf_fqueue(&s->iobuf,
					    "From MAILER-DAEMON@%s %s", env->sc_hostname, ctime(&now));
			}
			if (n != -1) {
				/* start queueing delivery headers */
				if (e->sender[0])
					/* XXX: remove existing Return-Path, if any */
					n = iobuf_fqueue(&s->iobuf,
					    "Return-Path: %s\nDelivered-To: %s\n",
					    e->sender, e->rcpt ? e->rcpt : e->dest);
				else
					n = iobuf_fqueue(&s->iobuf,
					    "Delivered-To: %s\n",
					    e->rcpt ? e->rcpt : e->dest);
			}
			if (n == -1) {
				log_warn("warn: mda: "
				    "fail to write delivery info");
				mda_queue_tempfail(e->id, "Out of memory",
				    ESC_OTHER_MAIL_SYSTEM_STATUS);
				mda_log(e, "TempFail", "Out of memory");
				mda_done(s);
				return;
			}

			/* request parent to fork a helper process */
			userinfo = &s->user->userinfo;
			memset(&deliver, 0, sizeof deliver);
			switch (e->method) {
			case A_MDA:
				deliver.mode = A_MDA;
				deliver.userinfo = *userinfo;
				(void)strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				(void)strlcpy(deliver.to, e->buffer,
				    sizeof(deliver.to));
				break;

			case A_MBOX:
				/* MBOX is a special case as we MUST deliver as root,
				 * just override the uid.
				 */
				deliver.mode = A_MBOX;
				deliver.userinfo = *userinfo;
				deliver.userinfo.uid = 0;
				(void)strlcpy(deliver.user, "root",
				    sizeof(deliver.user));
				(void)strlcpy(deliver.from, e->sender,
				    sizeof(deliver.from));
				(void)strlcpy(deliver.to, userinfo->username,
				    sizeof(deliver.to));
				break;

			case A_MAILDIR:
				deliver.mode = A_MAILDIR;
				deliver.userinfo = *userinfo;
				(void)strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				(void)strlcpy(deliver.dest, e->dest,
				    sizeof(deliver.dest));
				if (strlcpy(deliver.to, e->buffer,
					sizeof(deliver.to))
				    >= sizeof(deliver.to)) {
					log_warn("warn: mda: "
					    "deliver buffer too large");
					mda_queue_tempfail(e->id,
					    "Maildir path too long",
					    ESC_OTHER_MAIL_SYSTEM_STATUS);
					mda_log(e, "TempFail",
					    "Maildir path too long");
					mda_done(s);
					return;
				}
				break;

			case A_FILENAME:
				deliver.mode = A_FILENAME;
				deliver.userinfo = *userinfo;
				(void)strlcpy(deliver.user, userinfo->username,
				    sizeof deliver.user);
				if (strlcpy(deliver.to, e->buffer,
					sizeof(deliver.to))
				    >= sizeof(deliver.to)) {
					log_warn("warn: mda: "
					    "deliver buffer too large");
					mda_queue_tempfail(e->id,
					    "filename path too long",
					    ESC_OTHER_MAIL_SYSTEM_STATUS);
					mda_log(e, "TempFail",
					    "filename path too long");
					mda_done(s);
					return;
				}
				break;

			case A_LMTP:
				deliver.mode = A_LMTP;
				deliver.userinfo = *userinfo;
				(void)strlcpy(deliver.user, userinfo->username,
				    sizeof(deliver.user));
				(void)strlcpy(deliver.from, e->sender,
				    sizeof(deliver.from));
				if (strlcpy(deliver.to, e->buffer,
					sizeof(deliver.to))
				    >= sizeof(deliver.to)) {
					log_warn("warn: mda: "
					    "deliver buffer too large");
					mda_queue_tempfail(e->id,
					    "socket path too long",
					    ESC_OTHER_MAIL_SYSTEM_STATUS);
					mda_log(e, "TempFail",
					    "socket path too long");
					mda_done(s);
					return;
				}
				break;

			default:
				errx(1, "mda: unknown delivery method: %d",
				    e->method);
			}

			log_debug("debug: mda: querying mda fd "
			    "for session %016"PRIx64 " evpid %016"PRIx64,
			    s->id, s->evp->id);

			m_create(p_parent, IMSG_MDA_FORK, 0, 0, -1);
			m_add_id(p_parent, reqid);
			m_add_data(p_parent, &deliver, sizeof(deliver));
			m_close(p_parent);
			return;
		}
	}

	if (p->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_MDA_FORK:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);

			s = tree_xget(&sessions, reqid);
			e = s->evp;
			if (imsg->fd == -1) {
				log_warn("warn: mda: fail to retrieve mda fd");
				mda_queue_tempfail(e->id, "Cannot get mda fd",
				    ESC_OTHER_MAIL_SYSTEM_STATUS);
				mda_log(e, "TempFail", "Cannot get mda fd");
				mda_done(s);
				return;
			}

			log_debug("debug: mda: got mda fd %d "
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
				mda_queue_tempfail(e->id, error,
				    ESC_OTHER_MAIL_SYSTEM_STATUS);
				(void)snprintf(buf, sizeof buf, "Error (%s)", error);
				mda_log(e, "TempFail", buf);
			}
			else {
				mda_queue_ok(e->id);
				mda_log(e, "Ok", "Delivered");
			}
			mda_done(s);
			return;
		}
	}

	errx(1, "mda_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

void
mda_postfork()
{
}

void
mda_postprivdrop()
{
	tree_init(&sessions);
	tree_init(&users);
	TAILQ_INIT(&runnable);
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
	   done:
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
				m_create(p_parent, IMSG_MDA_KILL,
				    0, 0, -1);
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
			m_create(p_parent, IMSG_MDA_KILL, 0, 0, -1);
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
 			if (iobuf_queued(&s->iobuf) == 0)
				goto done;
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

		if (strchr(buf, ':') == NULL && !isspace((unsigned char)*buf))
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
	char	*ln, buf[LINE_MAX];
	size_t	 len;

	memset(buf, 0, sizeof buf);
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
		(void)strlcpy(dst, "\"", dstsz);
		(void)strnvis(dst + 1, buf, dstsz - 2, VIS_SAFE | VIS_CSTYLE);
		(void)strlcat(dst, "\"", dstsz);
	}

	return (0);
}

static void
mda_fail(struct mda_user *user, int permfail, const char *error, enum enhanced_status_code code)
{
	struct mda_envelope	*e;

	while ((e = TAILQ_FIRST(&user->envelopes))) {
		TAILQ_REMOVE(&user->envelopes, e, entry);
		if (permfail) {
			mda_log(e, "PermFail", error);
			mda_queue_permfail(e->id, error, code);
		}
		else {
			mda_log(e, "TempFail", error);
			mda_queue_tempfail(e->id, error, code);
		}
		mda_envelope_free(e);
	}

	mda_user_free(user);
}

static void
mda_drain(void)
{
	struct mda_user		*u;

	while ((u = (TAILQ_FIRST(&runnable)))) {

		TAILQ_REMOVE(&runnable, u, entry_runnable);

		if (u->evpcount == 0 && u->running == 0) {
			log_debug("debug: mda: all done for user \"%s\"",
			    mda_user_to_text(u));
			mda_user_free(u);
			continue;
		}

		if (u->evpcount == 0) {
			log_debug("debug: mda: no more envelope for \"%s\"",
			    mda_user_to_text(u));
			u->flags &= ~USER_RUNNABLE;
			continue;
		}

		if (u->running >= env->sc_mda_max_user_session) {
			log_debug("debug: mda: "
			    "maximum number of session reached for user \"%s\"",
			    mda_user_to_text(u));
			u->flags &= ~USER_RUNNABLE;
			continue;
		}

		if (tree_count(&sessions) >= env->sc_mda_max_session) {
			log_debug("debug: mda: "
			    "maximum number of session reached");
			TAILQ_INSERT_HEAD(&runnable, u, entry_runnable);
			return;
		}

		mda_session(u);

		if (u->evpcount == env->sc_mda_task_lowat) {
			if (u->flags & USER_ONHOLD) {
				log_debug("debug: mda: down to lowat for user \"%s\": releasing",
				    mda_user_to_text(u));
				u->flags &= ~USER_ONHOLD;
			}
			if (u->flags & USER_HOLDQ) {
				m_create(p_queue, IMSG_MDA_HOLDQ_RELEASE, 0, 0, -1);
				m_add_id(p_queue, u->id);
				m_add_int(p_queue, env->sc_mda_task_release);
				m_close(p_queue);
			}
		}

		/* re-add the user at the tail of the queue */
		TAILQ_INSERT_TAIL(&runnable, u, entry_runnable);
	}
}

static void
mda_done(struct mda_session *s)
{
	log_debug("debug: mda: session %016" PRIx64 " done", s->id);

	tree_xpop(&sessions, s->id);

	mda_envelope_free(s->evp);

	s->user->running--;
	if (!(s->user->flags & USER_RUNNABLE)) {
		log_debug("debug: mda: user \"%s\" becomes runnable",
		    s->user->name);
		TAILQ_INSERT_TAIL(&runnable, s->user, entry_runnable);
		s->user->flags |= USER_RUNNABLE;
	}

	if (s->datafp)
		fclose(s->datafp);
	io_clear(&s->io);
	iobuf_clear(&s->iobuf);

	free(s);

	stat_decrement("mda.running", 1);

	mda_drain();
}

static void
mda_log(const struct mda_envelope *evp, const char *prefix, const char *status)
{
	char rcpt[LINE_MAX];
	const char *method;

	rcpt[0] = '\0';
	if (evp->rcpt)
		(void)snprintf(rcpt, sizeof rcpt, "rcpt=<%s>, ", evp->rcpt);

	if (evp->method == A_MAILDIR)
		method = "maildir";
	else if (evp->method == A_MBOX)
		method = "mbox";
	else if (evp->method == A_FILENAME)
		method = "file";
	else if (evp->method == A_MDA)
		method = "mda";
	else if (evp->method == A_LMTP)
		method = "lmtp";
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

static void
mda_queue_ok(uint64_t evpid)
{
	m_create(p_queue, IMSG_MDA_DELIVERY_OK, 0, 0, -1);
	m_add_evpid(p_queue, evpid);
	m_close(p_queue);
}

static void
mda_queue_tempfail(uint64_t evpid, const char *reason, enum enhanced_status_code code)
{
	m_create(p_queue, IMSG_MDA_DELIVERY_TEMPFAIL, 0, 0, -1);
	m_add_evpid(p_queue, evpid);
	m_add_string(p_queue, reason);
	m_add_int(p_queue, (int)code);
	m_close(p_queue);
}

static void
mda_queue_permfail(uint64_t evpid, const char *reason, enum enhanced_status_code code)
{
	m_create(p_queue, IMSG_MDA_DELIVERY_PERMFAIL, 0, 0, -1);
	m_add_evpid(p_queue, evpid);
	m_add_string(p_queue, reason);
	m_add_int(p_queue, (int)code);
	m_close(p_queue);
}

static void
mda_queue_loop(uint64_t evpid)
{
	m_create(p_queue, IMSG_MDA_DELIVERY_LOOP, 0, 0, -1);
	m_add_evpid(p_queue, evpid);
	m_close(p_queue);
}

static struct mda_user *
mda_user(const struct envelope *evp)
{
	struct mda_user	*u;
	void		*i;

	i = NULL;
	while (tree_iter(&users, &i, NULL, (void**)(&u))) {
		if (!strcmp(evp->agent.mda.username, u->name) &&
		    !strcmp(evp->agent.mda.usertable, u->usertable))
			return (u);
	}

	u = xcalloc(1, sizeof *u, "mda_user");
	u->id = generate_uid();
	TAILQ_INIT(&u->envelopes);
	(void)strlcpy(u->name, evp->agent.mda.username, sizeof(u->name));
	(void)strlcpy(u->usertable, evp->agent.mda.usertable,
	    sizeof(u->usertable));

	tree_xset(&users, u->id, u);

	m_create(p_lka, IMSG_MDA_LOOKUP_USERINFO, 0, 0, -1);
	m_add_id(p_lka, u->id);
	m_add_string(p_lka, evp->agent.mda.usertable);
	m_add_string(p_lka, evp->agent.mda.username);
	m_close(p_lka);
	u->flags |= USER_WAITINFO;

	stat_increment("mda.user", 1);

	log_debug("mda: new user %llx for \"%s\"", u->id, mda_user_to_text(u));

	return (u);
}

static void
mda_user_free(struct mda_user *u)
{
	tree_xpop(&users, u->id);

	if (u->flags & USER_HOLDQ) {
		m_create(p_queue, IMSG_MDA_HOLDQ_RELEASE, 0, 0, -1);
		m_add_id(p_queue, u->id);
		m_add_int(p_queue, 0);
		m_close(p_queue);
	}

	free(u);
	stat_decrement("mda.user", 1);
}

static const char *
mda_user_to_text(const struct mda_user *u)
{
	static char buf[1024];

	(void)snprintf(buf, sizeof(buf), "%s:%s", u->usertable, u->name);

	return (buf);
}

static struct mda_envelope *
mda_envelope(const struct envelope *evp)
{
	struct mda_envelope	*e;
	char			 buf[LINE_MAX];

	e = xcalloc(1, sizeof *e, "mda_envelope");
	e->id = evp->id;
	e->creation = evp->creation;
	buf[0] = '\0';
	if (evp->sender.user[0] && evp->sender.domain[0])
		(void)snprintf(buf, sizeof buf, "%s@%s",
		    evp->sender.user, evp->sender.domain);
	e->sender = xstrdup(buf, "mda_envelope:sender");
	(void)snprintf(buf, sizeof buf, "%s@%s", evp->dest.user, evp->dest.domain);
	e->dest = xstrdup(buf, "mda_envelope:dest");
	(void)snprintf(buf, sizeof buf, "%s@%s", evp->rcpt.user, evp->rcpt.domain);
	if (strcmp(buf, e->dest))
		e->rcpt = xstrdup(buf, "mda_envelope:rcpt");
	e->method = evp->agent.mda.method;
	e->buffer = xstrdup(evp->agent.mda.buffer, "mda_envelope:buffer");
	e->user = xstrdup(evp->agent.mda.username, "mda_envelope:user");

	stat_increment("mda.envelope", 1);

	return (e);
}

static void
mda_envelope_free(struct mda_envelope *e)
{
	free(e->sender);
	free(e->dest);
	free(e->rcpt);
	free(e->user);
	free(e->buffer);
	free(e);

	stat_decrement("mda.envelope", 1);
}

static struct mda_session *
mda_session(struct mda_user * u)
{
	struct mda_session *s;

	s = xcalloc(1, sizeof *s, "mda_session");
	s->id = generate_uid();
	s->user = u;
	s->io.sock = -1;
	if (iobuf_init(&s->iobuf, 0, 0) == -1)
		fatal("mda_session");

	tree_xset(&sessions, s->id, s);

	s->evp = TAILQ_FIRST(&u->envelopes);
	TAILQ_REMOVE(&u->envelopes, s->evp, entry);
	u->evpcount--;
	u->running++;

	stat_decrement("mda.pending", 1);
	stat_increment("mda.running", 1);

	log_debug("debug: mda: new session %016" PRIx64
	    " for user \"%s\" evpid %016" PRIx64, s->id,
	    mda_user_to_text(u), s->evp->id);

	m_create(p_queue, IMSG_MDA_OPEN_MESSAGE, 0, 0, -1);
	m_add_id(p_queue, s->id);
	m_add_msgid(p_queue, evpid_to_msgid(s->evp->id));
	m_close(p_queue);

	return (s);
}
