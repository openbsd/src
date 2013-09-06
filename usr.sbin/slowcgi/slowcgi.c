/*	$OpenBSD: slowcgi.c,v 1.8 2013/09/06 07:36:03 blambert Exp $ */
/*
 * Copyright (c) 2013 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2013 Florian Obser <florian@openbsd.org>
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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <err.h>
#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define TIMEOUT_DEFAULT		 120
#define SLOWCGI_USER		 "www"

#define FCGI_CONTENT_SIZE	 65535
#define FCGI_PADDING_SIZE	 255
#define FCGI_RECORD_SIZE	 \
    (sizeof(struct fcgi_record_header) + FCGI_CONTENT_SIZE + FCGI_PADDING_SIZE)

#define STDOUT_DONE		 1
#define STDERR_DONE		 2
#define SCRIPT_DONE		 4

#define FCGI_BEGIN_REQUEST	 1
#define FCGI_ABORT_REQUEST	 2
#define FCGI_END_REQUEST	 3
#define FCGI_PARAMS		 4
#define FCGI_STDIN		 5
#define FCGI_STDOUT		 6
#define FCGI_STDERR		 7
#define FCGI_DATA		 8
#define FCGI_GET_VALUES		 9
#define FCGI_GET_VALUES_RESULT	10
#define FCGI_UNKNOWN_TYPE	11
#define FCGI_MAXTYPE		(FCGI_UNKNOWN_TYPE)

#define FCGI_REQUEST_COMPLETE	0
#define FCGI_CANT_MPX_CONN	1
#define FCGI_OVERLOADED		2
#define FCGI_UNKNOWN_ROLE	3


struct listener {
	struct event	ev, pause;
};

struct env_val {
	SLIST_ENTRY(env_val)	 entry;
	char			*val;
};
SLIST_HEAD(env_head, env_val);

struct fcgi_record_header {
	uint8_t		version;
	uint8_t		type;
	uint16_t	id;
	uint16_t	content_len;
	uint8_t		padding_len;
	uint8_t		reserved;
}__packed;

struct fcgi_response {
	TAILQ_ENTRY(fcgi_response)	entry;
	uint8_t				data[FCGI_RECORD_SIZE];
	size_t				data_pos;
	size_t				data_len;
};
TAILQ_HEAD(fcgi_response_head, fcgi_response);

struct fcgi_stdin {
	TAILQ_ENTRY(fcgi_stdin)	entry;
	uint8_t			data[FCGI_RECORD_SIZE];
	size_t			data_pos;
	size_t			data_len;
};
TAILQ_HEAD(fcgi_stdin_head, fcgi_stdin);

struct client {
	struct event			ev;
	struct event			resp_ev;
	struct event			tmo;
	int				fd;
	uint8_t				buf[FCGI_RECORD_SIZE];
	size_t				buf_pos;
	size_t				buf_len;
	struct fcgi_response_head	response_head;
	struct fcgi_stdin_head		stdin_head;	
	uint16_t			id;
	char				script_name[MAXPATHLEN];
	struct env_head			env;
	int				env_count;
	pid_t				script_pid;
	int				script_status;
	struct event			script_ev;
	struct event			script_err_ev;
	struct event			script_stdin_ev;
	uint8_t				script_flags;
	uint8_t				request_started;
};

struct clients {
	SLIST_ENTRY(clients)	 entry;
	struct client		*client;
};
SLIST_HEAD(clients_head, clients);

struct slowcgi_proc {
	struct clients_head	clients;
	struct event		ev_sigchld;
	struct event		ev_sigpipe;
};

struct fcgi_begin_request_body {
	uint16_t	role;
	uint8_t		flags;
	uint8_t		reserved[5];
}__packed;

struct fcgi_end_request {
	uint32_t	app_status;
	uint8_t		protocol_status;
	uint8_t		reserved[3];
}__packed;
__dead void	usage(void);
void		slowcgi_listen(char *, gid_t);
void		slowcgi_paused(int, short, void*);
void		slowcgi_accept(int, short, void*);
void		slowcgi_request(int, short, void*);
void		slowcgi_response(int, short, void*);
void		slowcgi_timeout(int, short, void*);
void		slowcgi_sig_handler(int, short, void*);
size_t		parse_request(uint8_t* , size_t, struct client*);
void		parse_begin_request(uint8_t*, uint16_t, struct client*,
		    uint16_t);
void		parse_params(uint8_t*, uint16_t, struct client*, uint16_t);
void		parse_stdin(uint8_t*, uint16_t, struct client*, uint16_t);
void		exec_cgi(struct client*);
void		script_in(int, struct event*, struct client*, uint8_t);
void		script_std_in(int, short, void*);
void		script_err_in(int, short, void*);
void		script_out(int, short, void*);
void		create_end_request(struct client*);
void		dump_fcgi_record_header(const char*,
		    struct fcgi_record_header*);
void		cleanup_client(struct client*);
struct loggers {
	void (*err)(int, const char *, ...);
	void (*errx)(int, const char *, ...);
	void (*warn)(const char *, ...);
	void (*warnx)(const char *, ...);
	void (*info)(const char *, ...);
	void (*debug)(const char *, ...);
};

const struct loggers conslogger = {
	err,
	errx,
	warn,
	warnx,
	warnx, /* info */
	warnx /* debug */
};

void	syslog_err(int, const char *, ...);
void	syslog_errx(int, const char *, ...);
void	syslog_warn(const char *, ...);
void	syslog_warnx(const char *, ...);
void	syslog_info(const char *, ...);
void	syslog_debug(const char *, ...);
void	syslog_vstrerror(int, int, const char *, va_list);

const struct loggers syslogger = {
	syslog_err,
	syslog_errx,
	syslog_warn,
	syslog_warnx,
	syslog_info,
	syslog_debug
};

const struct loggers *logger = &conslogger;

#define lerr(_e, _f...) logger->err((_e), _f)
#define lerrx(_e, _f...) logger->errx((_e), _f)
#define lwarn(_f...) logger->warn(_f)
#define lwarnx(_f...) logger->warnx(_f)
#define linfo(_f...) logger->info(_f)
#define ldebug(_f...) logger->debug(_f)

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-d] [-s socket]\n", __progname);
	exit(1);
}

struct timeval		timeout = { TIMEOUT_DEFAULT, 0 };
struct slowcgi_proc	slowcgi_proc;
int			debug = 0;
int			on = 1;
char 			*fcgi_socket = "/var/www/run/slowcgi.sock";

int
main(int argc, char *argv[])
{
	struct passwd	*pw;
	int		 c;

	while ((c = getopt(argc, argv, "ds:")) != -1) {
		switch (c) {
		case 'd':
			debug = 1;
			break;
		case 's':
			fcgi_socket = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (geteuid() != 0)
		errx(1, "need root privileges");

	pw = getpwnam(SLOWCGI_USER);
	if (pw == NULL)
		err(1, "no %s user", SLOWCGI_USER);

	if (!debug && daemon(1, 0) == -1)
		err(1, "daemon");

	event_init();

	slowcgi_listen(fcgi_socket, pw->pw_gid);

	if (chroot(pw->pw_dir) == -1)
		lerr(1, "chroot(%s)", pw->pw_dir);

	if (chdir("/") == -1)
		lerr(1, "chdir(%s)", pw->pw_dir);

	ldebug("chroot: %s", pw->pw_dir);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		lerr(1, "unable to revoke privs");

	SLIST_INIT(&slowcgi_proc.clients);

	signal_set(&slowcgi_proc.ev_sigchld, SIGCHLD, slowcgi_sig_handler,
	    &slowcgi_proc);
	signal_set(&slowcgi_proc.ev_sigpipe, SIGPIPE, slowcgi_sig_handler,
	    &slowcgi_proc);

	signal_add(&slowcgi_proc.ev_sigchld, NULL);
	signal_add(&slowcgi_proc.ev_sigpipe, NULL);

	event_dispatch();
	return (0);
}
void
slowcgi_listen(char *path, gid_t gid)
{
	struct listener		 *l = NULL;
	struct sockaddr_un	 sun;
	mode_t			 old_umask, mode;
	int			 fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		lerr(1, "slowcgi_listen: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (unlink(path) == -1)
		if (errno != ENOENT)
			lerr(1, "slowcgi_listen: unlink %s", path);

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;

	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		lerr(1,"slowcgi_listen: bind: %s", path);

	umask(old_umask);

	if (chmod(path, mode) == -1)
		lerr(1, "slowcgi_listen: chmod: %s", path);

	if (chown(path, 0, gid) == -1)
		lerr(1, "slowcgi_listen: chown: %s", path);

	if (ioctl(fd, FIONBIO, &on) == -1)
		lerr(1, "listener ioctl(FIONBIO)");

	if (listen(fd, 5) == -1)
		lerr(1, "listen");

	l = calloc(1, sizeof(*l));
	if (l == NULL)
		lerr(1, "listener ev alloc");

	event_set(&l->ev, fd, EV_READ | EV_PERSIST, slowcgi_accept, l);
	event_add(&l->ev, NULL);
	evtimer_set(&l->pause, slowcgi_paused, l);

	ldebug("socket: %s", path);
}

void
slowcgi_paused(int fd, short events, void *arg)
{
	struct listener	*l = arg;
	event_add(&l->ev, NULL);
}

void
slowcgi_accept(int fd, short events, void *arg)
{
	struct listener		*l;
	struct sockaddr_storage	 ss;
	struct timeval		 pause;
	struct client		*c;
	struct clients		*clients;
	socklen_t		 len;
	int			 s;

	l = arg;
	pause.tv_sec = 1;
	pause.tv_usec = 0;
	c = NULL;

	len = sizeof(ss);
	s = accept(fd, (struct sockaddr *)&ss, &len);
	if (s == -1) {
		switch (errno) {
		case EINTR:
		case EWOULDBLOCK:
		case ECONNABORTED:
			return;
		case EMFILE:
		case ENFILE:
			event_del(&l->ev);
			evtimer_add(&l->pause, &pause);
			return;
		default:
			lerr(1, "accept");
		}
	}

	if (ioctl(s, FIONBIO, &on) == -1)
		lerr(1, "client ioctl(FIONBIO)");

	c = calloc(1, sizeof(*c));
	if (c == NULL) {
		lwarn("cannot calloc client");
		close(s);
		return;
	}
	clients = calloc(1, sizeof(*clients));
	if (clients == NULL) {
		lwarn("cannot calloc clients");
		close(s);
		free(c);
		return;
	}
	c->fd = s;
	c->buf_pos = 0;
	c->buf_len = 0;
	c->request_started = 0;
	TAILQ_INIT(&c->response_head);
	TAILQ_INIT(&c->stdin_head);

	event_set(&c->ev, s, EV_READ | EV_PERSIST, slowcgi_request, c);
	event_add(&c->ev, NULL);

	event_set(&c->tmo, s, 0, slowcgi_timeout, c);
	event_add(&c->tmo, &timeout);
	clients->client = c;
	SLIST_INSERT_HEAD(&slowcgi_proc.clients, clients, entry);
}

void
slowcgi_timeout(int fd, short events, void *arg)
{
	cleanup_client((struct client*) arg);
}

void
slowcgi_sig_handler(int sig, short event, void *arg)
{
	struct client		*c;
	struct clients		*ncs;
	struct slowcgi_proc 	*p;
	pid_t			 pid;
	int			 status;

	p = arg;
	c = NULL;

	switch (sig) {
	case SIGCHLD:
		pid = wait(&status);
		SLIST_FOREACH(ncs, &p->clients, entry)
			if (ncs->client->script_pid == pid) {
				c = ncs->client;
				break;
			}
		if (c == NULL) {
			lwarnx("caught exit of unknown child %i", pid);
			break;
		}

		if (WIFSIGNALED(status))
			c->script_status = WTERMSIG(status);
		else
			c->script_status = WEXITSTATUS(status);

		if (c->script_flags == (STDOUT_DONE | STDERR_DONE))
			create_end_request(c);
		c->script_flags |= SCRIPT_DONE;

		ldebug("wait: %s", c->script_name);
		break;
	case SIGPIPE:
		/* ignore */
		break;
	default:
		lerr(1, "unexpected signal: %d", sig);

	}
}

void
slowcgi_response(int fd, short events, void *arg)
{
	struct client		 	*c;
	struct fcgi_record_header	*header;
	struct fcgi_response		*resp;
	ssize_t 			 n;

	c = arg;

	while ((resp = TAILQ_FIRST(&c->response_head))) {
		header = (struct fcgi_record_header*) resp->data;
		if (debug)
			dump_fcgi_record_header("resp ", header);

		n = write(fd, resp->data + resp->data_pos, resp->data_len);
		if (n == -1) {
			if (errno == EAGAIN)
				return;
			cleanup_client(c);
			return;
		}
		resp->data_pos += n;
		resp->data_len -= n;
		if (resp->data_len == 0) {
			TAILQ_REMOVE(&c->response_head, resp, entry);
			free(resp);
		}
	}

	if (TAILQ_EMPTY(&c->response_head)) {
		if (c->script_flags == (STDOUT_DONE | STDERR_DONE |
		    SCRIPT_DONE))
			cleanup_client(c);
		else
			event_del(&c->resp_ev);
	}
}

void
slowcgi_request(int fd, short events, void *arg)
{
	struct client	*c;
	size_t 		 n, parsed;

	c = arg;
	parsed = 0;

	n = read(fd, c->buf + c->buf_pos + c->buf_len,
	    FCGI_RECORD_SIZE - c->buf_pos-c->buf_len);
	
	switch (n) {
	case -1:
		switch (errno) {
		case EINTR:
		case EAGAIN:
			return;
		default:
			goto fail;
		}
		break;

	case 0:
		ldebug("closed connection");
		goto fail;
	default:
		break;
	}

	c->buf_len += n;

	do {
		parsed = parse_request(c->buf + c->buf_pos, c->buf_len, c);
		c->buf_pos += parsed;
		c->buf_len -= parsed;
	} while (parsed > 0 && c->buf_len > 0);

	/* Make space for further reads */
	if (c->buf_len > 0) {
		bcopy(c->buf + c->buf_pos, c->buf, c->buf_len);
		c->buf_pos = 0;
	}
	return;
fail:
	cleanup_client(c);
}

void
parse_begin_request(uint8_t *buf, uint16_t n, struct client *c, uint16_t id)
{
	struct fcgi_begin_request_body	*b;
	
	/* XXX -- FCGI_CANT_MPX_CONN */
	if (c->request_started) {
		lwarnx("unexpected FCGI_BEGIN_REQUEST, ignoring");
		return;
	}

	if (n != sizeof(struct fcgi_begin_request_body)) {
		lwarnx("wrong size %d != %d", n,
		    sizeof(struct fcgi_begin_request_body));
		return;
	}

	c->request_started = 1;
	b = (struct fcgi_begin_request_body*) buf;

	c->id = id;
	SLIST_INIT(&c->env);
	c->env_count = 0;
}
void
parse_params(uint8_t *buf, uint16_t n, struct client *c, uint16_t id)
{
	struct env_val			*env_entry;
	uint32_t			 name_len, val_len;

	if (!c->request_started) {
		lwarnx("FCGI_PARAMS without FCGI_BEGIN_REQUEST, ignoring");
		return;
	}

	if (c->id != id) {
		lwarnx("unexpected id, ignoring");
		return;
	}

	name_len = val_len = 0;

	if (n == 0) {
		exec_cgi(c);
		return;
	}
	while (n > 0) {
		if (buf[0] >> 7 == 0) {
			name_len = buf[0];
			n--;
			buf++;
		} else {
			if (n > 3) {
				name_len = ((buf[3] & 0x7f) << 24) +
				    (buf[2] << 16) + (buf[1] << 8) + buf[0];
				n -= 4;
				buf += 4;
			} else
				return;
		}

		if (n > 0) {
			if (buf[0] >> 7 == 0) {
				val_len = buf[0];
				n--;
				buf++;
			} else {
				if (n > 3) {
					val_len = ((buf[3] & 0x7f) << 24) +
					    (buf[2] << 16) + (buf[1] << 8) +
					     buf[0];
					n -= 4;
					buf += 4;
				} else
					return;
			}
		}
		if (n < name_len + val_len)
			return;

		if ((env_entry = malloc(sizeof(struct env_val))) == NULL) {
			lwarnx("cannot allocate env_entry");
			return;
		}

		if ((env_entry->val = calloc(sizeof(char), name_len + val_len +
		    2)) == NULL) {
			lwarnx("cannot allocate env_entry->val");
			free(env_entry);
			return;
		}

		bcopy(buf, env_entry->val, name_len);
		buf += name_len;
		n -= name_len;

		env_entry->val[name_len] = '\0';
		if (val_len < MAXPATHLEN && strcmp(env_entry->val,
		    "SCRIPT_NAME") == 0) {
			bcopy(buf, c->script_name, val_len);
			c->script_name[val_len] = '\0';
		}
		env_entry->val[name_len] = '=';

		bcopy(buf, (env_entry->val) + name_len + 1, val_len);
		buf += val_len;
		n -= val_len;

		SLIST_INSERT_HEAD(&c->env, env_entry, entry);
		c->env_count++;
	}
}

void
parse_stdin(uint8_t *buf, uint16_t n, struct client *c, uint16_t id)
{
	struct fcgi_stdin	*node;

	if (c->id != id) {
		lwarnx("unexpected id, ignoring");
		return;
	}

	if ((node = calloc(1, sizeof(struct fcgi_stdin))) == NULL) {
		lwarnx("cannot calloc stdin node");
		return;
	}

	bcopy(buf, node->data, n);
	node->data_pos = 0;
	node->data_len = n;

	TAILQ_INSERT_TAIL(&c->stdin_head, node, entry);

	event_del(&c->script_stdin_ev);
	event_set(&c->script_stdin_ev, EVENT_FD(&c->script_ev), EV_WRITE |
	    EV_PERSIST, script_out, c);
	event_add(&c->script_stdin_ev, NULL);
}

size_t
parse_request(uint8_t *buf, size_t n, struct client *c)
{
	struct fcgi_record_header 	*h;

	if (n < sizeof(struct fcgi_record_header))
		return (0);

	h = (struct fcgi_record_header*) buf;

	if (debug)
		dump_fcgi_record_header("", h);

	if (n < sizeof(struct fcgi_record_header) + ntohs(h->content_len)
	    + h->padding_len)
		return (0);

	if (h->version != 1)
		lerrx(1, "wrong version");

	switch (h->type) {
	case FCGI_BEGIN_REQUEST:
		parse_begin_request(buf + sizeof(struct fcgi_record_header),
		    ntohs(h->content_len), c, ntohs(h->id));
		break;
	case FCGI_PARAMS:
		parse_params(buf + sizeof(struct fcgi_record_header),
		    ntohs(h->content_len), c, ntohs(h->id));
		break;
	case FCGI_STDIN:
		parse_stdin(buf + sizeof(struct fcgi_record_header),
		    ntohs(h->content_len), c, ntohs(h->id));
		break;
	default:
		lwarnx("unimplemented type %d", h->type);
	}

	return (sizeof(struct fcgi_record_header) + ntohs(h->content_len)
	    + h->padding_len);
}

void
exec_cgi(struct client *c)
{
	struct env_val	*env_entry;
	int		 s[2], s_err[2], i;
	pid_t		 pid;
	char		*argv[2];
	char		**env;

	i = 0;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, s) == -1)
		lerr(1, "socketpair");
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, s_err) == -1)
		lerr(1, "socketpair");
	ldebug("fork: %s", c->script_name);

	switch (pid = fork()) {
	case -1:
		lwarn("fork");
		return;
	case 0:
		/* Child process */
		close(s[0]);
		close(s_err[0]);
		if (dup2(s[1], STDIN_FILENO) == -1)
			_exit(1);
		if (dup2(s[1], STDOUT_FILENO) == -1)
			_exit(1);
		if (dup2(s_err[1], STDERR_FILENO) == -1)
			_exit(1);
		argv[0] = c->script_name;
		argv[1] = NULL;
		if ((env = calloc(c->env_count + 1, sizeof(char*))) == NULL)
			_exit(1);
		SLIST_FOREACH(env_entry, &c->env, entry)
			env[i++] = env_entry->val;
		env[i++] = NULL;
		execve(c->script_name, argv, env);
		_exit(1);

	}
	/* Parent process*/
	close(s[1]);
	close(s_err[1]);
	if (ioctl(s[0], FIONBIO, &on) == -1)
		lerr(1, "script ioctl(FIONBIO)");
	if (ioctl(s_err[0], FIONBIO, &on) == -1)
		lerr(1, "script ioctl(FIONBIO)");

	c->script_pid = pid;
	event_set(&c->script_ev, s[0], EV_READ | EV_PERSIST, script_std_in, c);
	event_add(&c->script_ev, NULL);
	event_set(&c->script_err_ev, s_err[0], EV_READ | EV_PERSIST,
	    script_err_in, c);
	event_add(&c->script_err_ev, NULL);
}

void
create_end_request(struct client *c)
{
	struct fcgi_response		*resp;
	struct fcgi_record_header	*header;
	struct fcgi_end_request		*end_request;

	if ((resp = malloc(sizeof(struct fcgi_response))) == NULL) {
		lwarnx("cannot malloc fcgi_response");
		return;
	}
	header = (struct fcgi_record_header*) resp->data;
	header->version = 1;
	header->type = FCGI_END_REQUEST;
	header->id = htons(c->id);
	header->content_len = htons(sizeof(struct
	    fcgi_end_request));
	header->padding_len = 0;
	header->reserved = 0;
	end_request = (struct fcgi_end_request *) resp->data +
	    sizeof(struct fcgi_record_header);
	end_request->app_status = htonl(c->script_status);
	end_request->protocol_status = FCGI_REQUEST_COMPLETE;
	resp->data_pos = 0;
	resp->data_len = sizeof(struct fcgi_end_request) +
	    sizeof(struct fcgi_record_header);
	TAILQ_INSERT_TAIL(&c->response_head, resp, entry);	
}

void
script_in(int fd, struct event *ev, struct client *c, uint8_t type)
{
	struct fcgi_response		*resp;
	struct fcgi_record_header	*header;
	ssize_t 			 n;

	if ((resp = malloc(sizeof(struct fcgi_response))) == NULL) {
		lwarnx("cannot malloc fcgi_response");
		return;
	}
	header = (struct fcgi_record_header*) resp->data;
	header->version = 1;
	header->type = type;
	header->id = htons(c->id);
	header->padding_len = 0;
	header->reserved = 0;

	n = read(fd, resp->data + sizeof(struct fcgi_record_header),
	     FCGI_CONTENT_SIZE);
	
	if (n == -1) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
			free(resp);
			return;
		default:
			n = 0; /* fake empty FCGI_STD{OUT,ERR} response */
		}
	}
	header->content_len = htons(n);
	resp->data_pos = 0;
	resp->data_len = n + sizeof(struct fcgi_record_header);
	TAILQ_INSERT_TAIL(&c->response_head, resp, entry);

	event_del(&c->resp_ev);
	event_set(&c->resp_ev, EVENT_FD(&c->ev), EV_WRITE | EV_PERSIST,
	    slowcgi_response, c);
	event_add(&c->resp_ev, NULL);

	if (n == 0) {
		if (type == FCGI_STDOUT)
			c->script_flags |= STDOUT_DONE;
		else
			c->script_flags |= STDERR_DONE;

		if (c->script_flags == (STDOUT_DONE | STDERR_DONE |
		    SCRIPT_DONE)) {
			create_end_request(c);
		}
		event_del(ev);
		close(fd);
	}
}

void
script_std_in(int fd, short events, void *arg)
{
	struct client *c = arg;
	script_in(fd, &c->script_ev, c, FCGI_STDOUT);
}

void
script_err_in(int fd, short events, void *arg)
{
	struct client *c = arg;
	script_in(fd, &c->script_err_ev, c, FCGI_STDERR);
}

void
script_out(int fd, short events, void *arg)
{
	struct client		*c;
	struct fcgi_stdin	*node;
	ssize_t 		 n;

	c = arg;

	while ((node = TAILQ_FIRST(&c->stdin_head))) {
		if (node->data_len == 0) { /* end of stdin marker */
			shutdown(fd, SHUT_WR);
			break;
		}
		n = write(fd, node->data + node->data_pos, node->data_len);
		if (n == -1) {
			if (errno == EAGAIN)
				return;
			event_del(&c->script_stdin_ev);
			return;
		}
		node->data_pos += n;
		node->data_len -= n;
		if (node->data_len == 0) {
			TAILQ_REMOVE(&c->stdin_head, node, entry);
			free(node);
		}
	}
}

void
cleanup_client(struct client *c)
{
	struct fcgi_response	*resp;
	struct fcgi_stdin	*stdin_node;
	struct env_val		*env_entry;
	struct clients		*ncs, *tcs;

	evtimer_del(&c->tmo);
	if (event_initialized(&c->ev))
		event_del(&c->ev);
	if (event_initialized(&c->resp_ev))
		event_del(&c->resp_ev);
	if (event_initialized(&c->script_ev)) {
		close(EVENT_FD(&c->script_ev));
		event_del(&c->script_ev);
	}
	if (event_initialized(&c->script_err_ev)) {
		close(EVENT_FD(&c->script_err_ev));
		event_del(&c->script_err_ev);
	}
	if (event_initialized(&c->script_stdin_ev)) {
		close(EVENT_FD(&c->script_stdin_ev));
		event_del(&c->script_stdin_ev);
	}
	close(c->fd);
	while (!SLIST_EMPTY(&c->env)) {
		env_entry = SLIST_FIRST(&c->env);
		SLIST_REMOVE_HEAD(&c->env, entry);
		free(env_entry->val);
		free(env_entry);
	}

	while ((resp = TAILQ_FIRST(&c->response_head))) {
		TAILQ_REMOVE(&c->response_head, resp, entry);
		free(resp);
	}
	while ((stdin_node = TAILQ_FIRST(&c->stdin_head))) {
		TAILQ_REMOVE(&c->stdin_head, stdin_node, entry);
		free(stdin_node);
	}
	SLIST_FOREACH_SAFE(ncs, &slowcgi_proc.clients, entry, tcs) {
		if (ncs->client == c) {
			SLIST_REMOVE(&slowcgi_proc.clients, ncs, clients,
			    entry);
			free(ncs);
			break;
		}
	}
	free(c);
}

void
dump_fcgi_record_header(const char* p, struct fcgi_record_header *h)
{
	ldebug("%sversion:         %d", p, h->version);
	ldebug("%stype:            %d", p, h->type);
	ldebug("%srequestId:       %d", p, ntohs(h->id));
	ldebug("%scontentLength:   %d", p, ntohs(h->content_len));
	ldebug("%spaddingLength:   %d", p, h->padding_len);
	ldebug("%sreserved:        %d", p, h->reserved);
}

void
syslog_vstrerror(int e, int priority, const char *fmt, va_list ap)
{
	char *s;

	if (vasprintf(&s, fmt, ap) == -1) {
		syslog(LOG_EMERG, "unable to alloc in syslog_vstrerror");
		exit(1);
	}
	syslog(priority, "%s: %s", s, strerror(e));
	free(s);
}

void
syslog_err(int ecode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_EMERG, fmt, ap);
	va_end(ap);
	exit(ecode);
}

void
syslog_errx(int ecode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);
	exit(ecode);
}

void
syslog_warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	syslog_vstrerror(errno, LOG_WARNING, fmt, ap);
	va_end(ap);
}

void
syslog_warnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);
}

void
syslog_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_INFO, fmt, ap);
	va_end(ap);
}

void
syslog_debug(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	vsyslog(LOG_DEBUG, fmt, ap);
	va_end(ap);
}
