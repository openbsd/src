/*	$OpenBSD: filter_api.c,v 1.16 2015/01/20 17:37:54 deraadt Exp $	*/

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
	int		qtype;
	size_t		datalen;

	struct {
		int		 eom_called;

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
		int  (*connect)(uint64_t, struct filter_connect *);
		int  (*helo)(uint64_t, const char *);
		int  (*mail)(uint64_t, struct mailaddr *);
		int  (*rcpt)(uint64_t, struct mailaddr *);
		int  (*data)(uint64_t);
		void (*dataline)(uint64_t, const char *);
		int  (*eom)(uint64_t, size_t);

		void (*disconnect)(uint64_t);
		void (*reset)(uint64_t);
		void (*commit)(uint64_t);
		void (*rollback)(uint64_t);
	} cb;
} fi;

static void filter_api_init(void);
static void filter_response(struct filter_session *, int, int, const char *);
static void filter_send_response(struct filter_session *);
static void filter_register_query(uint64_t, uint64_t, int);
static void filter_dispatch(struct mproc *, struct imsg *);
static void filter_dispatch_dataline(uint64_t, const char *);
static void filter_dispatch_data(uint64_t);
static void filter_dispatch_eom(uint64_t, size_t);
static void filter_dispatch_connect(uint64_t, struct filter_connect *);
static void filter_dispatch_helo(uint64_t, const char *);
static void filter_dispatch_mail(uint64_t, struct mailaddr *);
static void filter_dispatch_rcpt(uint64_t, struct mailaddr *);
static void filter_dispatch_reset(uint64_t);
static void filter_dispatch_commit(uint64_t);
static void filter_dispatch_rollback(uint64_t);
static void filter_dispatch_disconnect(uint64_t);

static void filter_trigger_eom(struct filter_session *);
static void filter_io_in(struct io *, int);
static void filter_io_out(struct io *, int);
static const char *filterimsg_to_str(int);
static const char *hook_to_str(int);
static const char *query_to_str(int);
static const char *event_to_str(int);


static void
filter_response(struct filter_session *s, int status, int code, const char *line)
{
	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" %s filter_response(%d, %d, %s)",
	    filter_name, s->id, query_to_str(s->qtype), status, code, line);

	s->response.ready = 1;
	s->response.status = status;
	s->response.code = code;
	if (line)
		s->response.line = strdup(line);
	else
		s->response.line = NULL;

	/* eom is special, as the reponse has to be deferred until the pipe is all flushed */
	if (s->qtype == QUERY_EOM)
		filter_trigger_eom(s);
	else
		filter_send_response(s);
}

static void
filter_send_response(struct filter_session *s)
{
	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" %s filter_send_response() -> %d, %d, %s",
	    filter_name, s->id, query_to_str(s->qtype),
	    s->response.status,
	    s->response.code,
	    s->response.line);

	tree_xpop(&queries, s->qid);

	m_create(&fi.p, IMSG_FILTER_RESPONSE, 0, 0, -1);
	m_add_id(&fi.p, s->qid);
	m_add_int(&fi.p, s->qtype);
	if (s->qtype == QUERY_EOM)
		m_add_u32(&fi.p, s->datalen);
	m_add_int(&fi.p, s->response.status);
	m_add_int(&fi.p, s->response.code);
	if (s->response.line) {
		m_add_string(&fi.p, s->response.line);
		free(s->response.line);
		s->response.line = NULL;
	}
	m_close(&fi.p);

	s->qid = 0;
	s->response.ready = 0;
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
	int			 status, type;
	int			 fds[2], fdin, fdout;

	log_trace(TRACE_FILTERS, "filter-api:%s imsg %s", filter_name,
	    filterimsg_to_str(imsg->hdr.type));

	switch (imsg->hdr.type) {
	case IMSG_FILTER_REGISTER:
		m_msg(&m, imsg);
		m_get_u32(&m, &v);
		m_get_string(&m, &name);
		filter_name = strdup(name);
		m_end(&m);
		if (v != FILTER_API_VERSION) {
			log_warnx("warn: filter-api:%s API mismatch", filter_name);
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
		m_get_int(&m, &type);
		m_end(&m);
		switch (type) {
		case EVENT_CONNECT:
			s = xcalloc(1, sizeof(*s), "filter_dispatch");
			s->id = id;
			s->pipe.iev.sock = -1;
			s->pipe.oev.sock = -1;
			tree_xset(&sessions, id, s);
			break;
		case EVENT_DISCONNECT:
			filter_dispatch_disconnect(id);
			s = tree_xpop(&sessions, id);
			free(s);
			break;
		case EVENT_RESET:
			filter_dispatch_reset(id);
			break;
		case EVENT_COMMIT:
			filter_dispatch_commit(id);
			break;
		case EVENT_ROLLBACK:
			filter_dispatch_rollback(id);
			break;
		default:
			log_warnx("warn: filter-api:%s bad event %d", filter_name, type);
			fatalx("filter-api: exiting");
		}
		break;

	case IMSG_FILTER_QUERY:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_get_id(&m, &qid);
		m_get_int(&m, &type);
		switch(type) {
		case QUERY_CONNECT:
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.local);
			m_get_sockaddr(&m, (struct sockaddr*)&q_connect.remote);
			m_get_string(&m, &q_connect.hostname);
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_connect(id, &q_connect);
			break;
		case QUERY_HELO:
			m_get_string(&m, &line);
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_helo(id, line);
			break;
		case QUERY_MAIL:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_mail(id, &maddr);
			break;
		case QUERY_RCPT:
			m_get_mailaddr(&m, &maddr);
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_rcpt(id, &maddr);
			break;
		case QUERY_DATA:
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_data(id);
			break;
		case QUERY_EOM:
			m_get_u32(&m, &datalen);
			m_end(&m);
			filter_register_query(id, qid, type);
			filter_dispatch_eom(id, datalen);
			break;
		default:
			log_warnx("warn: filter-api:%s bad query %d", filter_name, type);
			fatalx("filter-api: exiting");
		}
		break;

	case IMSG_FILTER_PIPE:
		m_msg(&m, imsg);
		m_get_id(&m, &id);
		m_end(&m);

		fdout = imsg->fd;
		fdin = -1;

		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) == -1) {
			log_warn("warn: filter-api:%s socketpair", filter_name);
			close(fdout);
		}
		else {
			s = tree_xget(&sessions, id);

			s->pipe.eom_called = 0;
			s->pipe.error = 0;
			s->pipe.idatalen = 0;
			s->pipe.odatalen = 0;

			iobuf_init(&s->pipe.obuf, 0, 0);
			io_init(&s->pipe.oev, fdout, s, filter_io_out, &s->pipe.obuf);
			io_set_write(&s->pipe.oev);

			iobuf_init(&s->pipe.ibuf, 0, 0);
			io_init(&s->pipe.iev, fds[0], s, filter_io_in, &s->pipe.ibuf);
			io_set_read(&s->pipe.iev);

			fdin = fds[1];
		}

		log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" tx pipe %d -> %d",
		    filter_name, id, fdin, fdout);

		m_create(&fi.p, IMSG_FILTER_PIPE, 0, 0, fdin);
		m_add_id(&fi.p, id);
		m_close(&fi.p);

		break;
	}
}

static void
filter_register_query(uint64_t id, uint64_t qid, int type)
{
	struct filter_session	*s;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" %s", filter_name, id, query_to_str(type));

	s = tree_xget(&sessions, id);
	if (s->qid) {
		log_warnx("warn: filter-api:%s query already in progess",
		    filter_name);
		fatalx("filter-api: exiting");
	}
	s->qid = qid;
	s->qtype = type;
	s->response.ready = 0;

	tree_xset(&queries, qid, s);
}

static void
filter_dispatch_connect(uint64_t id, struct filter_connect *conn)
{
	if (fi.cb.connect)
		fi.cb.connect(id, conn);
	else
		filter_api_accept(id);
}

static void
filter_dispatch_helo(uint64_t id, const char *helo)
{
	if (fi.cb.helo)
		fi.cb.helo(id, helo);
	else
		filter_api_accept(id);
}

static void
filter_dispatch_mail(uint64_t id, struct mailaddr *mail)
{
	if (fi.cb.mail)
		fi.cb.mail(id, mail);
	else
		filter_api_accept(id);
}

static void
filter_dispatch_rcpt(uint64_t id, struct mailaddr *rcpt)
{
	if (fi.cb.rcpt)
		fi.cb.rcpt(id, rcpt);
	else
		filter_api_accept(id);
}

static void
filter_dispatch_data(uint64_t id)
{
	if (fi.cb.data)
		fi.cb.data(id);
	else
		filter_api_accept(id);
}

static void
filter_dispatch_reset(uint64_t id)
{
	if (fi.cb.reset)
		fi.cb.reset(id);
}

static void
filter_dispatch_commit(uint64_t id)
{
	if (fi.cb.commit)
		fi.cb.commit(id);
}

static void
filter_dispatch_rollback(uint64_t id)
{
	if (fi.cb.rollback)
		fi.cb.rollback(id);
}

static void
filter_dispatch_disconnect(uint64_t id)
{
	if (fi.cb.disconnect)
		fi.cb.disconnect(id);
}

static void
filter_dispatch_dataline(uint64_t id, const char *data)
{
	if (fi.cb.dataline)
		fi.cb.dataline(id, data);
	else
		filter_api_writeln(id, data);
}

static void
filter_dispatch_eom(uint64_t id, size_t datalen)
{
	struct filter_session	*s;

	s = tree_xget(&sessions, id);
	s->datalen = datalen;

	filter_trigger_eom(s);
}

static void
filter_trigger_eom(struct filter_session *s)
{
	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_trigger_eom(%d, %d, %zu, %zu, %zu)",
	    filter_name, s->id, s->pipe.iev.sock, s->pipe.oev.sock,
	    s->datalen, s->pipe.idatalen, s->pipe.odatalen);

	/* This is called when 
	 * - EOM query is first received
	 * - input data is closed
	 * - output has been written
	 */

	/* input not done yet, or EOM query not received */
	if (s->pipe.iev.sock != -1 || s->qid == 0)
		return;

	if (s->pipe.error)
		goto fail;

	/* if size don't match, error out */
	if (s->pipe.idatalen != s->datalen) {
		log_trace(TRACE_FILTERS, "filter-api:%s tx datalen mismatch: %zu/%zu",
		    filter_name, s->pipe.idatalen, s->datalen);
		s->pipe.error = 1;
		goto fail;
	}

	/* if we didn't send the eom to the user do it now */
	if (!s->pipe.eom_called) {
		s->pipe.eom_called = 1;
		if (fi.cb.eom)
			fi.cb.eom(s->id, s->datalen);
		else
			filter_api_accept(s->id);
		return;
	}

	if (s->pipe.error)
		goto fail;

	/* wait for the output socket to be closed */
	if (s->pipe.oev.sock != -1)
		return;

	s->datalen = s->pipe.odatalen;
	filter_send_response(s);

    fail:
	/* XXX */
	return;
}

static void
filter_io_in(struct io *io, int evt)
{
	struct filter_session	*s = io->arg;
	char			*line;
	size_t			 len;

	log_trace(TRACE_FILTERS, "filter-api:%s filter_io_in(%p, %s)",
	    filter_name, s, io_strevent(evt));

	switch (evt) {
	case IO_DATAIN:
	    nextline:
		line = iobuf_getline(&s->pipe.ibuf, &len);
		if ((line == NULL && iobuf_len(&s->pipe.ibuf) >= LINE_MAX) ||
		    (line && len >= LINE_MAX)) {
			s->pipe.error = 1;
			break;
		}
		/* No complete line received */
		if (line == NULL) {
			iobuf_normalize(&s->pipe.ibuf);
			/* flow control */
			if (iobuf_queued(&s->pipe.obuf) >= FILTER_HIWAT)
				io_pause(&s->pipe.iev, IO_PAUSE_IN);
			return;
		}

		s->pipe.idatalen += len + 1;
		/* XXX warning: do not clear io from this call! */
		filter_dispatch_dataline(s->id, line);
		goto nextline;

	case IO_DISCONNECTED:
		if (iobuf_len(&s->pipe.ibuf)) {
			log_warn("warn: filter-api:%s %016"PRIx64" incomplete input",
			    filter_name, s->id);
		}
		log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" input done (%zu bytes)",
		    filter_name, s->id, s->pipe.idatalen);
		break;

	default:
		log_warn("warn: filter-api:%s %016"PRIx64": unexpected io event %d on data pipe",
		    filter_name, s->id, evt);
		s->pipe.error = 1;

	}
	if (s->pipe.error) {
		io_clear(&s->pipe.oev);
		iobuf_clear(&s->pipe.obuf);
	}
	io_clear(&s->pipe.iev);
	iobuf_clear(&s->pipe.ibuf);
	filter_trigger_eom(s);
}

static void
filter_io_out(struct io *io, int evt)
{
	struct filter_session    *s = io->arg;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_io_out(%s)",
	    filter_name, s->id, io_strevent(evt));

	switch (evt) {
	case IO_TIMEOUT:
	case IO_DISCONNECTED:
	case IO_ERROR:
		log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" io error on output pipe",
		    filter_name, s->id);
		s->pipe.error = 1;
		break;

	case IO_LOWAT:
		/* flow control */
		if (s->pipe.iev.sock != -1 && s->pipe.iev.flags & IO_PAUSE_IN) {
			io_resume(&s->pipe.iev, IO_PAUSE_IN);
			return;
		}

		/* if the input is done and there is a response we are done */
		if (s->pipe.iev.sock == -1 && s->response.ready)
			break;

		/* just wait for more data to send */
		return;

	default:
		fatalx("filter_io_out()");
	}

	io_clear(&s->pipe.oev);
	iobuf_clear(&s->pipe.obuf);
	if (s->pipe.error) {
		io_clear(&s->pipe.iev);
		iobuf_clear(&s->pipe.ibuf);
	}
	filter_trigger_eom(s);
}

#define CASE(x) case x : return #x

static const char *
filterimsg_to_str(int imsg)
{
	switch (imsg) {
	CASE(IMSG_FILTER_REGISTER);
	CASE(IMSG_FILTER_EVENT);
	CASE(IMSG_FILTER_QUERY);
	CASE(IMSG_FILTER_PIPE);
	CASE(IMSG_FILTER_RESPONSE);
	default:
		return ("IMSG_FILTER_???");
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
		return ("HOOK_???");
	}
}

static const char *
query_to_str(int query)
{
	switch (query) {
	CASE(QUERY_CONNECT);
	CASE(QUERY_HELO);
	CASE(QUERY_MAIL);
	CASE(QUERY_RCPT);
	CASE(QUERY_DATA);
	CASE(QUERY_EOM);
	CASE(QUERY_DATALINE);
	default:
		return ("QUERY_???");
	}
}

static const char *
event_to_str(int event)
{
	switch (event) {
	CASE(EVENT_CONNECT);
	CASE(EVENT_RESET);
	CASE(EVENT_DISCONNECT);
	CASE(EVENT_COMMIT);
	CASE(EVENT_ROLLBACK);
	default:
		return ("EVENT_???");
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
		return (filter_name);
	return ("filter");
}

const char *
imsg_to_str(int imsg)
{
	static char buf[32];

	snprintf(buf, sizeof(buf), "%d", imsg);

	return (buf);
}


/*
 * These functions are callable by filters
 */

void
filter_api_setugid(uid_t uid, gid_t gid)
{
	filter_api_init();

	if (! uid) {
		log_warn("warn: filter-api:%s can't set uid 0", filter_name);
		fatalx("filter-api: exiting");
	}
	if (! gid) {
		log_warn("warn: filter-api:%s can't set gid 0", filter_name);
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
		log_warn("warn: filter-api:%s getpwnam", filter_name);
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

	/* XXX just for now */
	fi.hooks = ~0;

	mproc_init(&fi.p, 0);
}

void
filter_api_on_connect(int(*cb)(uint64_t, struct filter_connect *))
{
	filter_api_init();

	fi.hooks |= HOOK_CONNECT;
	fi.cb.connect = cb;
}

void
filter_api_on_helo(int(*cb)(uint64_t, const char *))
{
	filter_api_init();

	fi.hooks |= HOOK_HELO;
	fi.cb.helo = cb;
}

void
filter_api_on_mail(int(*cb)(uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_MAIL;
	fi.cb.mail = cb;
}

void
filter_api_on_rcpt(int(*cb)(uint64_t, struct mailaddr *))
{
	filter_api_init();

	fi.hooks |= HOOK_RCPT;
	fi.cb.rcpt = cb;
}

void
filter_api_on_data(int(*cb)(uint64_t))
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
filter_api_on_eom(int(*cb)(uint64_t, size_t))
{
	filter_api_init();

	fi.hooks |= HOOK_EOM;
	fi.cb.eom = cb;
}

void
filter_api_on_reset(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_RESET;
	fi.cb.reset = cb;
}

void
filter_api_on_disconnect(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_DISCONNECT;
	fi.cb.disconnect = cb;
}

void
filter_api_on_commit(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_COMMIT;
	fi.cb.commit = cb;
}

void
filter_api_on_rollback(void(*cb)(uint64_t))
{
	filter_api_init();

	fi.hooks |= HOOK_ROLLBACK;
	fi.cb.rollback = cb;
}

void
filter_api_loop(void)
{
	if (register_done) {
		log_warnx("warn: filter-api:%s filter_api_loop() already called", filter_name);
		fatalx("filter-api: exiting");
	}

	filter_api_init();

	register_done = 1;

	mproc_enable(&fi.p);

	if (fi.rootpath) {
		if (chroot(fi.rootpath) == -1) {
			log_warn("warn: filter-api:%s chroot", filter_name);
			fatalx("filter-api: exiting");
		}
		if (chdir("/") == -1) {
			log_warn("warn: filter-api:%s chdir", filter_name);
			fatalx("filter-api: exiting");
		}
	}

	if (setgroups(1, &fi.gid) ||
	    setresgid(fi.gid, fi.gid, fi.gid) ||
	    setresuid(fi.uid, fi.uid, fi.uid)) {
		log_warn("warn: filter-api:%s cannot drop privileges", filter_name);
		fatalx("filter-api: exiting");
	}

	if (event_dispatch() < 0) {
		log_warn("warn: filter-api:%s event_dispatch", filter_name);
		fatalx("filter-api: exiting");
	}
}

int
filter_api_accept(uint64_t id)
{
	struct filter_session	*s;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_api_accept()", filter_name, id);

	s = tree_xget(&sessions, id);
	filter_response(s, FILTER_OK, 0, NULL);

	return (1);
}

int
filter_api_reject(uint64_t id, enum filter_status status)
{
	struct filter_session	*s;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_api_reject(%d)",
	    filter_name, id, status);

	s = tree_xget(&sessions, id);

	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(s, status, 0, NULL);

	return (1);
}

int
filter_api_reject_code(uint64_t id, enum filter_status status, uint32_t code,
    const char *line)
{
	struct filter_session	*s;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_api_reject_code(%d, %u, %s)",
	    filter_name, id, status, code, line);

	s = tree_xget(&sessions, id);

	/* This is NOT an acceptable status for a failure */
	if (status == FILTER_OK)
		status = FILTER_FAIL;

	filter_response(s, status, code, line);

	return (1);
}

void
filter_api_writeln(uint64_t id, const char *line)
{
	struct filter_session	*s;

	log_trace(TRACE_FILTERS, "filter-api:%s %016"PRIx64" filter_api_writeln(%s)", filter_name, id, line);

	s = tree_xget(&sessions, id);

	if (s->pipe.oev.sock == -1) {
		log_warnx("warn: filter:%s: cannot write at this point", filter_name);
		fatalx("exiting");
	}

	s->pipe.odatalen += strlen(line) + 1;
	iobuf_fqueue(&s->pipe.obuf, "%s\n", line);
	io_reload(&s->pipe.oev);
}

const char *
filter_api_sockaddr_to_text(const struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0,
		NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

const char *
filter_api_mailaddr_to_text(const struct mailaddr *maddr)
{
	static char  buffer[LINE_MAX];

	strlcpy(buffer, maddr->user, sizeof buffer);
	strlcat(buffer, "@", sizeof buffer);
	if (strlcat(buffer, maddr->domain, sizeof buffer) >= sizeof buffer)
		return (NULL);

	return (buffer);
}
