/*	$OpenBSD: control.c,v 1.103 2015/05/28 17:09:18 florian Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

#define CONTROL_BACKLOG 5

struct ctl_conn {
	uint32_t		 id;
	uint8_t			 flags;
#define CTL_CONN_NOTIFY		 0x01
	struct mproc		 mproc;
	uid_t			 euid;
	gid_t			 egid;
};

struct {
	struct event		 ev;
	int			 fd;
} control_state;

static void control_imsg(struct mproc *, struct imsg *);
static void control_shutdown(void);
static void control_listen(void);
static void control_accept(int, short, void *);
static void control_close(struct ctl_conn *);
static void control_sig_handler(int, short, void *);
static void control_dispatch_ext(struct mproc *, struct imsg *);
static void control_digest_update(const char *, size_t, int);
static void control_broadcast_verbose(int, int);

static struct stat_backend *stat_backend = NULL;
extern const char *backend_stat;

static uint32_t			connid = 0;
static struct tree		ctl_conns;
static struct stat_digest	digest;

#define	CONTROL_FD_RESERVE	5

static void
control_imsg(struct mproc *p, struct imsg *imsg)
{
	struct ctl_conn		*c;
	struct stat_value	 val;
	struct msg		 m;
	const char		*key;
	const void		*data;
	size_t			 sz;

	if (p->proc == PROC_PONY) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_SMTP_SESSION:
			c = tree_get(&ctl_conns, imsg->hdr.peerid);
			if (c == NULL)
				return;
			m_compose(&c->mproc, IMSG_CTL_OK, 0, 0, imsg->fd,
			    NULL, 0);
			return;
		}
	}
	if (p->proc == PROC_SCHEDULER) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_OK:
		case IMSG_CTL_FAIL:
		case IMSG_CTL_LIST_MESSAGES:
			c = tree_get(&ctl_conns, imsg->hdr.peerid);
			if (c == NULL)
				return;
			imsg->hdr.peerid = 0;
			m_forward(&c->mproc, imsg);
			return;
		}
	}
	if (p->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_LIST_ENVELOPES:
			c = tree_get(&ctl_conns, imsg->hdr.peerid);
			if (c == NULL)
				return;
			m_forward(&c->mproc, imsg);
			return;
		}
	}
	if (p->proc == PROC_PONY) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_OK:
		case IMSG_CTL_FAIL:
		case IMSG_CTL_MTA_SHOW_HOSTS:
		case IMSG_CTL_MTA_SHOW_RELAYS:
		case IMSG_CTL_MTA_SHOW_ROUTES:
		case IMSG_CTL_MTA_SHOW_HOSTSTATS:
		case IMSG_CTL_MTA_SHOW_BLOCK:
			c = tree_get(&ctl_conns, imsg->hdr.peerid);
			if (c == NULL)
				return;
			imsg->hdr.peerid = 0;
			m_forward(&c->mproc, imsg);
			return;
		}
	}

	switch (imsg->hdr.type) {
	case IMSG_STAT_INCREMENT:
		m_msg(&m, imsg);
		m_get_string(&m, &key);
		m_get_data(&m, &data, &sz);
		m_end(&m);
		memmove(&val, data, sz);
		if (stat_backend)
			stat_backend->increment(key, val.u.counter);
		control_digest_update(key, val.u.counter, 1);
		return;
	case IMSG_STAT_DECREMENT:
		m_msg(&m, imsg);
		m_get_string(&m, &key);
		m_get_data(&m, &data, &sz);
		m_end(&m);
		memmove(&val, data, sz);
		if (stat_backend)
			stat_backend->decrement(key, val.u.counter);
		control_digest_update(key, val.u.counter, 0);
		return;
	case IMSG_STAT_SET:
		m_msg(&m, imsg);
		m_get_string(&m, &key);
		m_get_data(&m, &data, &sz);
		m_end(&m);
		memmove(&val, data, sz);
		if (stat_backend)
			stat_backend->set(key, &val);
		return;
	}

	errx(1, "control_imsg: unexpected %s imsg",
	    imsg_to_str(imsg->hdr.type));
}

static void
control_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		control_shutdown();
		break;
	default:
		fatalx("control_sig_handler: unexpected signal");
	}
}

int
control_create_socket(void)
{
	struct sockaddr_un	sun;
	int			fd;
	mode_t			old_umask;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("control: socket");

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, SMTPD_SOCKET,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatal("control: socket name too long");

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0)
		fatalx("control socket already listening");

	if (unlink(SMTPD_SOCKET) == -1)
		if (errno != ENOENT)
			fatal("control: cannot unlink socket");

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		(void)umask(old_umask);
		fatal("control: bind");
	}
	(void)umask(old_umask);

	if (chmod(SMTPD_SOCKET,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) == -1) {
		(void)unlink(SMTPD_SOCKET);
		fatal("control: chmod");
	}

	session_socket_blockmode(fd, BM_NONBLOCK);
	control_state.fd = fd;

	return fd;
}

pid_t
control(void)
{
	pid_t			 pid;
	struct passwd		*pw;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("control: cannot fork");
	case 0:
		post_fork(PROC_CONTROL);
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	if ((pw = getpwnam(SMTPD_USER)) == NULL)
		fatalx("unknown user " SMTPD_USER);

	stat_backend = env->sc_stat;
	stat_backend->init();

	if (chroot(PATH_CHROOT) == -1)
		fatal("control: chroot");
	if (chdir("/") == -1)
		fatal("control: chdir(\"/\")");

	config_process(PROC_CONTROL);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("control: cannot drop privileges");

	imsg_callback = control_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, control_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, control_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	tree_init(&ctl_conns);

	memset(&digest, 0, sizeof digest);
	digest.startup = time(NULL);

	config_peer(PROC_SCHEDULER);
	config_peer(PROC_QUEUE);
	config_peer(PROC_PARENT);
	config_peer(PROC_LKA);
	config_peer(PROC_PONY);
	config_peer(PROC_CA);
	config_done();

	control_listen();

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	control_shutdown();

	return (0);
}

static void
control_shutdown(void)
{
	log_info("info: control process exiting");
	_exit(0);
}

static void
control_listen(void)
{
	if (listen(control_state.fd, CONTROL_BACKLOG) == -1)
		fatal("control_listen");

	event_set(&control_state.ev, control_state.fd, EV_READ|EV_PERSIST,
	    control_accept, NULL);
	event_add(&control_state.ev, NULL);
}

/* ARGSUSED */
static void
control_accept(int listenfd, short event, void *arg)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*c;

	if (getdtablesize() - getdtablecount() < CONTROL_FD_RESERVE)
		goto pause;

	len = sizeof(sun);
	if ((connfd = accept(listenfd, (struct sockaddr *)&sun, &len)) == -1) {
		if (errno == ENFILE || errno == EMFILE)
			goto pause;
		if (errno == EINTR || errno == EWOULDBLOCK ||
		    errno == ECONNABORTED)
			return;
		fatal("control_accept: accept");
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	c = xcalloc(1, sizeof(*c), "control_accept");
	if (getpeereid(connfd, &c->euid, &c->egid) == -1)
		fatal("getpeereid");
	c->id = ++connid;
	c->mproc.proc = PROC_CLIENT;
	c->mproc.handler = control_dispatch_ext;
	c->mproc.data = c;
	mproc_init(&c->mproc, connfd);
	mproc_enable(&c->mproc);
	tree_xset(&ctl_conns, c->id, c);

	stat_backend->increment("control.session", 1);
	return;

pause:
	log_warnx("warn: ctl client limit hit, disabling new connections");
	event_del(&control_state.ev);
}

static void
control_close(struct ctl_conn *c)
{
	tree_xpop(&ctl_conns, c->id);
	mproc_clear(&c->mproc);
	free(c);

	stat_backend->decrement("control.session", 1);

	if (getdtablesize() - getdtablecount() < CONTROL_FD_RESERVE)
		return;

	if (!event_pending(&control_state.ev, EV_READ, NULL)) {
		log_warnx("warn: re-enabling ctl connections");
		event_add(&control_state.ev, NULL);
	}
}

static void
control_digest_update(const char *key, size_t value, int incr)
{
	size_t	*p;

	p = NULL;

	if (!strcmp(key, "smtp.session")) {
		if (incr)
			p = &digest.clt_connect;
		else
			digest.clt_disconnect += value;
	}
	else if (!strcmp(key, "scheduler.envelope")) {
		if (incr)
			p = &digest.evp_enqueued;
		else
			digest.evp_dequeued += value;
	}
	else if  (!strcmp(key, "scheduler.envelope.expired"))
		p = &digest.evp_expired;
	else if  (!strcmp(key, "scheduler.envelope.removed"))
		p = &digest.evp_removed;
	else if  (!strcmp(key, "scheduler.delivery.ok"))
		p = &digest.dlv_ok;
	else if  (!strcmp(key, "scheduler.delivery.permfail"))
		p = &digest.dlv_permfail;
	else if  (!strcmp(key, "scheduler.delivery.tempfail"))
		p = &digest.dlv_tempfail;
	else if  (!strcmp(key, "scheduler.delivery.loop"))
		p = &digest.dlv_loop;

	else if  (!strcmp(key, "queue.bounce"))
		p = &digest.evp_bounce;

	if (p) {
		if (incr)
			*p = *p + value;
		else
			*p = *p - value;
	}
}

/* ARGSUSED */
static void
control_dispatch_ext(struct mproc *p, struct imsg *imsg)
{
	struct sockaddr_storage	 ss;
	struct ctl_conn		*c;
	int			 v;
	struct stat_kv		*kvp;
	char			*key;
	struct stat_value	 val;
	size_t			 len;

	c = p->data;

	if (imsg == NULL) {
		control_close(c);
		return;
	}

	if (imsg->hdr.peerid != IMSG_VERSION) {
		m_compose(p, IMSG_CTL_FAIL, IMSG_VERSION, 0, -1, NULL, 0);
		return;
	}

	switch (imsg->hdr.type) {
	case IMSG_CTL_SMTP_SESSION:
		if (env->sc_flags & (SMTPD_SMTP_PAUSED | SMTPD_EXITING)) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		m_compose(p_pony, IMSG_CTL_SMTP_SESSION, c->id, 0, -1,
		    &c->euid, sizeof(c->euid));
		return;

	case IMSG_CTL_GET_DIGEST:
		if (c->euid)
			goto badcred;
		digest.timestamp = time(NULL);
		m_compose(p, IMSG_CTL_GET_DIGEST, 0, 0, -1, &digest, sizeof digest);
		return;

	case IMSG_CTL_GET_STATS:
		if (c->euid)
			goto badcred;
		kvp = imsg->data;
		if (! stat_backend->iter(&kvp->iter, &key, &val))
			kvp->iter = NULL;
		else {
			(void)strlcpy(kvp->key, key, sizeof kvp->key);
			kvp->val = val;
		}
		m_compose(p, IMSG_CTL_GET_STATS, 0, 0, -1, kvp, sizeof *kvp);
		return;

	case IMSG_CTL_SHUTDOWN:
		/* NEEDS_FIX */
		log_debug("debug: received shutdown request");

		if (c->euid)
			goto badcred;

		if (env->sc_flags & SMTPD_EXITING) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		env->sc_flags |= SMTPD_EXITING;
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		m_compose(p_parent, IMSG_CTL_SHUTDOWN, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_VERBOSE:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
			goto badcred;

		memcpy(&v, imsg->data, sizeof(v));
		verbose = v;
		log_verbose(verbose);

		control_broadcast_verbose(IMSG_CTL_VERBOSE, verbose);

		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_TRACE_ENABLE:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
			goto badcred;

		memcpy(&v, imsg->data, sizeof(v));
		verbose |= v;
		log_verbose(verbose);

		control_broadcast_verbose(IMSG_CTL_VERBOSE, verbose);

		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_TRACE_DISABLE:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
			goto badcred;

		memcpy(&v, imsg->data, sizeof(v));
		verbose &= ~v;
		log_verbose(verbose);

		control_broadcast_verbose(IMSG_CTL_VERBOSE, verbose);

		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_PROFILE_ENABLE:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
			goto badcred;

		memcpy(&v, imsg->data, sizeof(v));
		profiling |= v;

		control_broadcast_verbose(IMSG_CTL_PROFILE, profiling);

		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_PROFILE_DISABLE:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
			goto badcred;

		memcpy(&v, imsg->data, sizeof(v));
		profiling &= ~v;

		control_broadcast_verbose(IMSG_CTL_PROFILE, profiling);

		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_PAUSE_EVP:
		if (c->euid)
			goto badcred;

		imsg->hdr.peerid = c->id;
		m_forward(p_scheduler, imsg);
		return;

	case IMSG_CTL_PAUSE_MDA:
		if (c->euid)
			goto badcred;

		if (env->sc_flags & SMTPD_MDA_PAUSED) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: mda paused");
		env->sc_flags |= SMTPD_MDA_PAUSED;
		m_compose(p_queue, IMSG_CTL_PAUSE_MDA, 0, 0, -1, NULL, 0);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_PAUSE_MTA:
		if (c->euid)
			goto badcred;

		if (env->sc_flags & SMTPD_MTA_PAUSED) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: mta paused");
		env->sc_flags |= SMTPD_MTA_PAUSED;
		m_compose(p_queue, IMSG_CTL_PAUSE_MTA, 0, 0, -1, NULL, 0);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_PAUSE_SMTP:
		if (c->euid)
			goto badcred;

		if (env->sc_flags & SMTPD_SMTP_PAUSED) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: smtp paused");
		env->sc_flags |= SMTPD_SMTP_PAUSED;
		m_compose(p_pony, IMSG_CTL_PAUSE_SMTP, 0, 0, -1, NULL, 0);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_RESUME_EVP:
		if (c->euid)
			goto badcred;

		imsg->hdr.peerid = c->id;
		m_forward(p_scheduler, imsg);
		return;

	case IMSG_CTL_RESUME_MDA:
		if (c->euid)
			goto badcred;

		if (! (env->sc_flags & SMTPD_MDA_PAUSED)) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: mda resumed");
		env->sc_flags &= ~SMTPD_MDA_PAUSED;
		m_compose(p_queue, IMSG_CTL_RESUME_MDA, 0, 0, -1, NULL, 0);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_RESUME_MTA:
		if (c->euid)
			goto badcred;

		if (!(env->sc_flags & SMTPD_MTA_PAUSED)) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: mta resumed");
		env->sc_flags &= ~SMTPD_MTA_PAUSED;
		m_compose(p_queue, IMSG_CTL_RESUME_MTA, 0, 0, -1, NULL, 0);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_RESUME_SMTP:
		if (c->euid)
			goto badcred;

		if (!(env->sc_flags & SMTPD_SMTP_PAUSED)) {
			m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			return;
		}
		log_info("info: smtp resumed");
		env->sc_flags &= ~SMTPD_SMTP_PAUSED;
		m_forward(p_pony, imsg);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_RESUME_ROUTE:
		if (c->euid)
			goto badcred;

		m_forward(p_pony, imsg);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	case IMSG_CTL_LIST_MESSAGES:
		if (c->euid)
			goto badcred;
		m_compose(p_scheduler, IMSG_CTL_LIST_MESSAGES, c->id, 0, -1,
		    imsg->data, imsg->hdr.len - sizeof(imsg->hdr));
		return;

	case IMSG_CTL_LIST_ENVELOPES:
		if (c->euid)
			goto badcred;
		m_compose(p_scheduler, IMSG_CTL_LIST_ENVELOPES, c->id, 0, -1,
		    imsg->data, imsg->hdr.len - sizeof(imsg->hdr));
		return;

	case IMSG_CTL_MTA_SHOW_HOSTS:
	case IMSG_CTL_MTA_SHOW_RELAYS:
	case IMSG_CTL_MTA_SHOW_ROUTES:
	case IMSG_CTL_MTA_SHOW_HOSTSTATS:
	case IMSG_CTL_MTA_SHOW_BLOCK:
		if (c->euid)
			goto badcred;

		imsg->hdr.peerid = c->id;
		m_forward(p_pony, imsg);
		return;

	case IMSG_CTL_SHOW_STATUS:
		if (c->euid)
			goto badcred;

		m_compose(p, IMSG_CTL_SHOW_STATUS, 0, 0, -1, &env->sc_flags,
		    sizeof(env->sc_flags));
		return;

	case IMSG_CTL_MTA_BLOCK:
	case IMSG_CTL_MTA_UNBLOCK:
		if (c->euid)
			goto badcred;

		if (imsg->hdr.len - IMSG_HEADER_SIZE <= sizeof(ss))
			goto invalid;
		memmove(&ss, imsg->data, sizeof(ss));
		m_create(p_pony, imsg->hdr.type, c->id, 0, -1);
		m_add_sockaddr(p_pony, (struct sockaddr *)&ss);
		m_add_string(p_pony, (char *)imsg->data + sizeof(ss));
		m_close(p_pony);
		return;

	case IMSG_CTL_SCHEDULE:
		if (c->euid)
			goto badcred;

		imsg->hdr.peerid = c->id;
		m_forward(p_scheduler, imsg);
		return;

	case IMSG_CTL_REMOVE:
		if (c->euid)
			goto badcred;

		imsg->hdr.peerid = c->id;
		m_forward(p_scheduler, imsg);
		return;

	case IMSG_CTL_UPDATE_TABLE:
		if (c->euid)
			goto badcred;

		/* table name too long */
		len = strlen(imsg->data);
		if (len >= LINE_MAX)
			goto invalid;

		m_forward(p_lka, imsg);
		m_compose(p, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
		return;

	default:
		log_debug("debug: control_dispatch_ext: "
		    "error handling %s imsg",
		    imsg_to_str(imsg->hdr.type));
		return;
	}
badcred:
invalid:
	m_compose(p, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
}

static void
control_broadcast_verbose(int msg, int v)
{
	m_create(p_lka, msg, 0, 0, -1);
	m_add_int(p_lka, v);
	m_close(p_lka);

	m_create(p_pony, msg, 0, 0, -1);
	m_add_int(p_pony, v);
	m_close(p_pony);

	m_create(p_queue, msg, 0, 0, -1);
	m_add_int(p_queue, v);
	m_close(p_queue);

	m_create(p_ca, msg, 0, 0, -1);
	m_add_int(p_ca, v);
	m_close(p_ca);

	m_create(p_scheduler, msg, 0, 0, -1);
	m_add_int(p_scheduler, v);
	m_close(p_scheduler);

	m_create(p_parent, msg, 0, 0, -1);
	m_add_int(p_parent, v);
	m_close(p_parent);
}
