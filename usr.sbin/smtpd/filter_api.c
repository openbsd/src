/*	$OpenBSD: filter_api.c,v 1.13 2014/04/09 18:55:19 eric Exp $	*/

/*
 * Copyright (c) 2013 Eric Faurot <eric@openbsd.org>
 * Copyright (c) 2011 Gilles Chehade <gilles@poolp.org>
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
#include <sys/uio.h>

#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define FILTER_HIWAT 65536

static struct tree	queries;
static struct tree	sessions;

struct filter_session {
	uint64_t	id;
	uint64_t	qid;
	int		qhook;

	struct {
		size_t		 datalen;
		int		 error;
		struct io	 iev;
		struct iobuf	 ibuf;
		size_t		 idatalen;
		struct io	 oev;
		struct iobuf	 obuf;
		size_t		 odatalen;
	} pipe;

	struct {
		int		 ready;
		int		 status;
		int		 code;
		int		 notify;
		char		*line;
	} response;
};

static int		 register_done;
static const char	*filter_name;

static struct filter_internals {
	struct mproc	p;

	uint32_t	hooks;
	uint32_t	flags;

	uid_t		uid;
	gid_t		gid;
	const char     *rootpath;

	struct {
		void (*notify)(uint64_t, enum filter_status);
		void (*connect)(uint64_t, struct filter_connect *);
		void (*helo)(uint64_t, const char *);
		void (*mail)(uint64_t, struct mailaddr *);
		void (*rcpt)(uint64_t, struct mailaddr *);
		void (*data)(uint64_t);
		void (*dataline)(uint64_t, const char *);
		void (*eom)(uint64_t);
		void (*event)(uint64_t, enum filter_hook);
	} cb;
} fi;

static void filter_api_init(void);
static void filter_response(struct filter_session *, int, int, const char *line, int);
static void filter_send_response(struct filter_session *);
static void filter_register_query(uint64_t, uint64_t, enum filter_hook);
static void filter_dispatch(struct mproc *, struct imsg *);
static void filter_dispatch_event(uint64_t, enum filter_hook);
static void filter_dispatch_dataline(uint64_t, const char *);
static void filter_dispatch_data(uint64_t, uint64_t);
static void filter_dispatch_eom(uint64_t, uint64_t, size_t);
static void filter_dispatch_notify(uint64_t, enum filter_status);
static void filter_dispatch_connect(uint64_t, uint64_t, struct filter_connect *);
static void filter_dispatch_helo(uint64_t, uint64_t, const char *);
static void filter_dispatch_mail(uint64_t, uint64_t, struct mailaddr *);
static void filter_dispatch_rcpt(uint64_t, uint64_t, struct mailaddr *);
static void filter_trigger_eom(struct filter_session *);
static void filter_io_in(struct io *, int);
static void filter_io_out(struct io *, int);
static const char *filterimsg_to_str(int);
static const char *hook_to_str(int);

void
filter_api_on_notify(void(*cb)(uint64_t, enum filter_status))
{
	filter_api_init();

	fi.cb.notify = cb;
}

void
filter_api_on_connect(void(*cb)(uint64_t, struct filter_connect *))
{
	filter_api_init();

	fi.hooks |= HOOK_CONNECT;
	fi.cb.connect = cb;
}

void
filter_api_on_helo(void(*cb)(uint64_t, const char *))
{
	filter_api_init();

	fi.hooks |= HOOK_HELO;
	fi.cb.helo = cb;
}

void
filter_api_on_mail(void(*cb)(uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_MAIL;
	fi.cb.mail = cb;
}

void
filter_api_on_rcpt(void(*cb)(uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_RCPT;
	fi.cb.rcpt = cb;
}

void
filter_api_on_data(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_DATA;
	fi.cb.data = cb;
}

void
filter_api_on_dataline(void(*cb)(uint64_t, const char *))
{
	filter_api_init();

	fi.hooks |= HOOK_DATALINE | HOOK_EOM;
	fi.cb.dataline = cb;
}

void
filter_api_on_eom(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_EOM;
	fi.cb.eom = cb;
}

void
filter_api_on_event(void(*cb)(uint64_t, enum filter_hook))
{
	filter_api_init();

	fi.hooks |= HOOK_DISCONNECT | HOOK_RESET | HOOK_COMMIT;
	fi.cb.event = cb;
}

void
filter_api_loop(void)
{
	if (register_done) {
		log_warnx("warn: filter-api:%s: filter_api_loop() already called", filter_name);
		fatalx("filter-api: exiting");
	}

	filter_api_init();

	register_done = 1;

	mproc_enable(&fi.p);

	if (fi.rootpath) {
		if (chroot(fi.rootpath) == -1) {
			log_warn("warn: filter-api:%s: chroot", filter_name);
			fatalx("filter-api: exiting");
		}
		if (chdir("/") == -1) {
			log_warn("warn: filter-api:%s: chdir", filter_name);
			fatalx("filter-api: exiting");
		}
	}

	if (setgroups(1, &fi.gid) ||
	    setresgid(fi.gid, fi.gid, fi.gid) ||
	    setresuid(fi.uid, fi.uid, fi.uid)) {
		log_warn("warn: filter-api:%s: cannot drop privileges", filter_name);
		fatalx("filter-api: exiting");
	}

	if (event_dispatch() < 0) {
		log_warn("warn: filter-api:%s: event_dispatch", filter_name);
		fatalx("filter-api: exiting");
	}
}

void
filter_api_accept(uint64_t id)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);
	filter_response(s, FILTER_OK, 0, NULL, 0);
}

void
filter_api_accept_notify(uint64_t id, uint64_t *qid)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);
	*qid = s->qid;
	filter_response(s, FILTER_OK, 0, NULL, 1);
}

void
filter_api_reject(uint64_t id, enum filter_status status)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);

	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(s, status, 0, NULL, 0);
}

void
filter_api_reject_code(uint64_t id, enum filter_status status, uint32_t code,
    const char *line)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);

	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(s, status, code, line, 0);
}

void
filter_api_writeln(uint64_t id, const char *line)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);

	if (s->pipe.oev.sock == -1) {
		log_warnx("warn: filter:%s: cannot write at this point", filter_name);
		fatalx("exiting");
	}

	s->pipe.odatalen += strlen(line) + 1;
	iobuf_fqueue(&s->pipe.obuf, "%s\n", line);
	io_reload(&s->pipe.oev);
}

static void
filter_response(struct filter_session *s, int status, int code, const char *line, int notify)
{
	log_debug("debug: filter-api:%s: got response %s for %016"PRIx64" %d %d %s",
	    filter_name, hook_to_str(s->qhook), s->id,
	    s->response.status,
	    s->response.code,
	    s->response.line);

	s->response.ready = 1;
	s->response.status = status;
	s->response.code = code;
	s->response.notify = notify;
	if (line)
		s->response.line = strdup(line);
	else
		s->response.line = NULL;

	/* For HOOK_EOM, wait until the obuf is drained before sending the  */
	if (s->qhook == HOOK_EOM &&
	    fi.hooks & HOOK_DATALINE &&
	    s->pipe.oev.sock != -1) {
		log_debug("debug: filter-api:%s: got response, waiting for opipe to be closed", filter_name);
		return;
	}

	filter_send_response(s);
}

static void
filter_send_response(struct filter_session *s)
{
	log_debug("debug: filter-api:%s: sending response %s for %016"PRIx64" %d %d %s",
	    filter_name, hook_to_str(s->qhook), s->id,
	    s->response.status,
	    s->response.code,
	    s->response.line);

	tree_xpop(&queries, s->qid);

	m_create(&fi.p, IMSG_FILTER_RESPONSE, 0, 0, -1);
	m_add_id(&fi.p, s->qid);
	m_add_int(&fi.p, s->qhook);
	if (s->qhook == HOOK_EOM)
		m_add_u32(&fi.p, (s->qhook & HOOK_DATALINE) ?
		    s->pipe.odatalen : s->pipe.datalen);
	m_add_int(&fi.p, s->response.status);
	m_add_int(&fi.p, s->response.code);
	m_add_int(&fi.p, s->response.notify);
	if (s->response.line) {
		m_add_string(&fi.p, s->response.line);
		free(s->response.line);
		s->response.line = NULL;
	}
	m_close(&fi.p);

	s->qid = 0;
	s->response.ready = 0;
}

void
filter_api_setugid(uid_t uid, gid_t gid)
{
	filter_api_init();

	if (! uid) {
		log_warn("warn: filter-api:%s: can't set uid 0", filter_name);
		fatalx("filter-api: exiting");
	}
	if (! gid) {
		log_warn("warn: filter-api:%s: can't set gid 0", filter_name);
		fatalx("filter-api: exiting");
	}
	fi.uid = uid;
	fi.gid = gid;
}

void
filter_api_no_chroot(void)
{
	filter_api_init();

	fi.rootpath = NULL;
}

void
filter_api_set_chroot(const char *rootpath)
{
	filter_api_init();

	fi.rootpath = rootpath;
}

static void
filter_api_init(void)
{
	extern const char *__progname;
	struct passwd  *pw;
	static int	init = 0;

	if (init)
		return;

	init = 1;

	log_init(-1);
	log_verbose(1);

	pw = getpwnam(SMTPD_USER);
	if (pw == NULL) {
		log_warn("warn: filter-api:%s: getpwnam", filter_name);
		fatalx("filter-api: exiting");
	}

	smtpd_process = PROC_FILTER;
	filter_name = __progname;

	tree_init(&queries);
	tree_init(&sessions);
	event_init();

	memset(&fi, 0, sizeof(fi));
	fi.p.proc = PROC_PONY;
	fi.p.name = "filter";
	fi.p.handler = filter_dispatch;
	fi.uid = pw->pw_uid;
	fi.gid = pw->pw_gid;
	fi.rootpath = PATH_CHROOT;
	
	mproc_init(&fi.p, 0);
}

static void
filter_dispatch(struct mproc *p, struct imsg *imsg)
{
	struct filter_session	*s;
	struct filter_connect	 q_connect;
	struct mailaddr		 maddr;
	struct msg		 m;
	const char		*line, *name;
	uint32_t		 v, datalen;
	uint64_t		 id, qid;
	int			 status, event, hook;
	int			 fds[2], fdin, fdout;

	log_debug("debug: filter-api:%s: imsg %s", filter_name,
	    filterimsg_to_str(imsg->hdr.type));

	switch (imsg->hdr.type) {
	case IMSG_FILTER_REGISTER:
		m_msg(&m, imsg);
		m_get_u32(&m, &v);
		m_get_string(&m, &name);
		filter_name = strdup(name);
		m_end(&m);
		if (v != FILTER_API_VERSION) {
			log_warnx("warn: filter-api:%s: API mismatch", filter_name);
			fatalx("filter-api: exiting");
		}
		m_create(p, IMSG_FILTER_REGISTER, 0, 0, -1);
		m_add_int(p, fi.hooks);
		m_add_int(p, fi.flags);
		m_close(p);
		break;

	case IMSG_FILTER_EVENT:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_int(&m, &event);
		m_end(&m);
		filter_dispatch_event(id, event);
		if (event == HOOK_DISCONNECT) {
			s = tree_xpop(&sessions, id);
			free(s);
		}
		break;

	case IMSG_FILTER_QUERY:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_id(&m, &qid);
		m_get_int(&m, &hook);
		switch(hook) {
		case HOOK_CONNECT:
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.local);
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.remote);
			m_get_string(&m, &q_connect.hostname);
			m_end(&m);
			s = xcalloc(1, sizeof(*s), "filter_dispatch");
			s->id = id;
			s->pipe.iev.sock = -1;
			s->pipe.oev.sock = -1;
			tree_xset(&sessions, id, s);
			filter_register_query(id, qid, hook);
			filter_dispatch_connect(id, qid, &q_connect);
			break;
		case HOOK_HELO:
			m_get_string(&m, &line);
			m_end(&m);
			filter_register_query(id, qid, hook);
			filter_dispatch_helo(id, qid, line);
			break;
		case HOOK_MAIL:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_register_query(id, qid, hook);
			filter_dispatch_mail(id, qid, &maddr);
			break;
		case HOOK_RCPT:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_register_query(id, qid, hook);
			filter_dispatch_rcpt(id, qid, &maddr);
			break;
		case HOOK_DATA:
			m_end(&m);
			filter_register_query(id, qid, hook);
			filter_dispatch_data(id, qid);
			break;
		case HOOK_EOM:
			m_get_u32(&m, &datalen);
			m_end(&m);
			filter_register_query(id, qid, hook);
			filter_dispatch_eom(id, qid, datalen);
			break;
		default:
			log_warnx("warn: filter-api:%s: bad hook %d", filter_name, hook);
			fatalx("filter-api: exiting");
		}
		break;

	case IMSG_FILTER_PIPE_SETUP:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_end(&m);

		fdout = imsg->fd;
		fdin = -1;

		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) == -1) {
			log_warn("warn: filter-api:%s: socketpair", filter_name);
			close(fdout);
		}
		else {
			s = tree_xget(&sessions, id);
			iobuf_init(&s->pipe.obuf, 0, 0);
			io_init(&s->pipe.oev, fdout, s, filter_io_out, &s->pipe.obuf);
			io_set_write(&s->pipe.oev);

			iobuf_init(&s->pipe.ibuf, 0, 0);
			io_init(&s->pipe.iev, fds[0], s, filter_io_in, &s->pipe.ibuf);
			io_set_read(&s->pipe.iev);

			fdin = fds[1];
			/* XXX notify? */
		}
		log_debug("debug: filter-api:%s: tx pipe %d -> %d for %016"PRIx64, filter_name, fdin, fdout, id);
		m_create(&fi.p, IMSG_FILTER_PIPE_SETUP, 0, 0, fdin);
		m_add_id(&fi.p, id);
		m_close(&fi.p);
		break;

	case IMSG_FILTER_PIPE_ABORT:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_end(&m);
		s = tree_xget(&sessions, id);
		if (s->pipe.iev.sock != -1) {
			io_clear(&s->pipe.iev);
			iobuf_clear(&s->pipe.ibuf);
		}
		if (s->pipe.oev.sock != -1) {
			io_clear(&s->pipe.oev);
			iobuf_clear(&s->pipe.obuf);
		}
		/* XXX notify? */
		break;

	case IMSG_FILTER_NOTIFY:
		m_msg(&m, imsg);
		m_get_id(&m, &qid);
		m_get_int(&m, &status);
		m_end(&m);
		filter_dispatch_notify(qid, status);
		break;

	}
}

static void
filter_register_query(uint64_t id, uint64_t qid, enum filter_hook hook)
{
	struct filter_session	*s;

	log_debug("debug: filter-api:%s: query %s for %016"PRIx64,
		filter_name, hook_to_str(hook), id);

	s = tree_xget(&sessions, id);
	if (s->qid) {
		log_warn("warn: filter-api:%s: query already in progess",
		    filter_name);
		fatalx("filter-api: exiting");
	}
	s->qid = qid;
	s->qhook = hook;
	s->response.ready = 0;

	tree_xset(&queries, qid, s);
}

static void
filter_dispatch_event(uint64_t id, enum filter_hook event)
{
	fi.cb.event(id, event);
}

static void
filter_dispatch_notify(uint64_t qid, enum filter_status status)
{
	fi.cb.notify(qid, status);
}

static void
filter_dispatch_connect(uint64_t id, uint64_t qid, struct filter_connect *conn)
{
	fi.cb.connect(id, conn);
}

static void
filter_dispatch_helo(uint64_t id, uint64_t qid, const char *helo)
{
	fi.cb.helo(id, helo);
}

static void
filter_dispatch_mail(uint64_t id, uint64_t qid, struct mailaddr *mail)
{
	fi.cb.mail(id, mail);
}

static void
filter_dispatch_rcpt(uint64_t id, uint64_t qid, struct mailaddr *rcpt)
{
	fi.cb.rcpt(id, rcpt);
}

static void
filter_dispatch_data(uint64_t id, uint64_t qid)
{
	fi.cb.data(id);
}

static void
filter_dispatch_eom(uint64_t id, uint64_t qid, size_t datalen)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);
	s->pipe.datalen = datalen;

	if (fi.hooks & HOOK_DATALINE) {
		/* wait for the io to be done  */
		if (s->pipe.iev.sock != -1) {
			log_debug("debug: filter-api:%s: eom received for %016"PRIx64", waiting for io to end",
			    filter_name, id);
			return;
		}
		filter_trigger_eom(s);
		return;
	}

	fi.cb.eom(s->id);
}

static void
filter_dispatch_dataline(uint64_t id, const char *data)
{
	fi.cb.dataline(id, data);
}

static void
filter_trigger_eom(struct filter_session *s)
{
	log_debug("debug: filter-api:%s: tx eom (%zu) for %016"PRIx64, filter_name, s->pipe.datalen, s->id);

	if (!s->pipe.error && s->pipe.idatalen != s->pipe.datalen) {
		log_debug("debug: filter-api:%s: tx datalen mismatch: %zu/%zu",
		    filter_name, s->pipe.idatalen, s->pipe.datalen);
		s->pipe.error = 1;
	}
	if (s->pipe.error) {
		log_debug("debug: filter-api:%s: tx pipe.error", filter_name);
		/* XXX error? */
	}

	/* if the filter has no eom callback, we accept the message */
	if (fi.cb.eom) {
		log_debug("debug: filter-api:%s: calling eom callback", filter_name);
		fi.cb.eom(s->id);
	} else {
		log_debug("debug: filter-api:%s: accepting by default", filter_name);
		filter_api_accept(s->id);
	}

	/* if the output is done and the response is ready, send it */
	if ((s->pipe.oev.sock == -1 || iobuf_queued(&s->pipe.obuf) == 0) &&
	    s->response.ready) {
		log_debug("debug: filter-api:%s: sending response", filter_name);
		if (s->pipe.oev.sock != -1) {
			io_clear(&s->pipe.oev);
			iobuf_clear(&s->pipe.obuf);
		}
		filter_send_response(s);
	}
	else {
		log_debug("debug: filter-api:%s: waiting for obuf to drain", filter_name);
	}
}

static void
filter_io_in(struct io *io, int evt)
{
	struct filter_session	*s = io->arg;
	char			*line;
	size_t			 len;

	log_debug("debug: filter-api:%s: filter_io_in(%p, %s)",
	    filter_name, s, io_strevent(evt));

	switch (evt) {
	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&s->pipe.ibuf, &len);
		if ((line == NULL && iobuf_len(&s->pipe.ibuf) >= SMTPD_MAXLINESIZE) ||
		    (line && len >= SMTPD_MAXLINESIZE)) {
			s->pipe.error = 1;
			io_clear(&s->pipe.oev);
			iobuf_clear(&s->pipe.obuf);
			break;
		}
		/* No complete line received */
		if (line == NULL) {
			iobuf_normalize(&s->pipe.ibuf);
			/* flow control */
			if (iobuf_queued(&s->pipe.obuf) >= FILTER_HIWAT)
				io_pause(&s->pipe.oev, IO_PAUSE_IN);
			return;
		}
		s->pipe.idatalen += len + 1;
		filter_dispatch_dataline(s->id, line);
		goto nextline;

	case IO_DISCONNECTED:
		if (s->qhook == HOOK_EOM)
			filter_trigger_eom(s);
		else {
			log_debug("debug: filter-api:%s: datain closed, for %016"PRIx64", waiting for eom",
		    filter_name, s->id);
		}
		break;
	default:
		s->pipe.error = 1;
		io_clear(&s->pipe.oev);
		iobuf_clear(&s->pipe.obuf);
	}
	io_clear(&s->pipe.iev);
	iobuf_clear(&s->pipe.ibuf);
}

static void
filter_io_out(struct io *io, int evt)
{
	struct filter_session    *s = io->arg;

	log_debug("debug: filter-api:%s: filter_io_out(%p, %s)",
	    filter_name, s, io_strevent(evt));

	switch (evt) {
	case IO_TIMEOUT:
	case IO_DISCONNECTED:
	case IO_ERROR:
		log_debug("debug: filter-api:%s: io error on output pipe",
		    filter_name);
		s->pipe.error = 1;
		io_clear(&s->pipe.oev);
		iobuf_clear(&s->pipe.obuf);
		if (s->pipe.iev.sock != -1) {
			io_clear(&s->pipe.iev);
			iobuf_clear(&s->pipe.ibuf);
		}
		break;

	case IO_LOWAT:
		/* flow control */
		if (s->pipe.iev.sock != -1 && s->pipe.iev.flags & IO_PAUSE_IN)
			io_resume(&s->pipe.iev, IO_PAUSE_IN);

		/* if the input is done and there is a response, send it */
		if (s->pipe.iev.sock == -1 && s->response.ready) {
			io_clear(&s->pipe.oev);
			iobuf_clear(&s->pipe.obuf);
			filter_send_response(s);
		}
		break;
	default:
		fatalx("filter_io_out()");
	}
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

/*
 * These functions are called from mproc.c
 */

enum smtp_proc_type smtpd_process;

const char *
proc_name(enum smtp_proc_type proc)
{
	if (proc == PROC_FILTER)
		return filter_name;
	return "filter";
}

const char *
imsg_to_str(int imsg)
{
	static char buf[32];

	snprintf(buf, sizeof(buf), "%d", imsg);

	return (buf);
}
