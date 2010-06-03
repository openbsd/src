/*	$OpenBSD: ldape.c,v 1.3 2010/06/03 17:29:54 martinh Exp $ */

/*
 * Copyright (c) 2009, 2010 Martin Hedenfalk <martin@bzero.se>
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

#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ldapd.h"

void			 ldape_sig_handler(int fd, short why, void *data);
void			 ldape_dispatch_ldapd(int fd, short event, void *ptr);

int			 ldap_starttls(struct request *req);
void			 send_ldap_extended_response(struct conn *conn,
				int msgid, unsigned long type,
				long long result_code,
				const char *extended_oid);

struct imsgev		*iev_ldapd;
struct control_sock	 csock;

void
ldape_sig_handler(int sig, short why, void *data)
{
	log_debug("ldape: got signal %d", sig);
	if (sig == SIGCHLD) {
		for (;;) {
			pid_t	 pid;
			int	 status;

			pid = waitpid(WAIT_ANY, &status, WNOHANG);
			if (pid <= 0)
				break;
			check_compaction(pid, status);
		}
		return;
	}

	event_loopexit(NULL);
}

void
send_ldap_extended_response(struct conn *conn, int msgid, unsigned long type,
    long long result_code, const char *extended_oid)
{
	int			 rc;
	struct ber_element	*root, *elm;
	void			*buf;

	log_debug("sending response %u with result %d", type, result_code);

	if ((root = ber_add_sequence(NULL)) == NULL)
		goto fail;

	elm = ber_printf_elements(root, "d{tEss",
	    msgid, BER_CLASS_APP, type, result_code, "", "");
	if (elm == NULL)
		goto fail;

	if (extended_oid)
		elm = ber_add_string(elm, extended_oid);

	rc = ber_write_elements(&conn->ber, root);
	ber_free_elements(root);

	if (rc < 0)
		log_warn("failed to create ldap result");
	else {
		ber_get_writebuf(&conn->ber, &buf);
		if (bufferevent_write(conn->bev, buf, rc) != 0)
			log_warn("failed to send ldap result");
	}

	return;
fail:
	if (root)
		ber_free_elements(root);
}

void
send_ldap_result(struct conn *conn, int msgid, unsigned long type,
    long long result_code)
{
	send_ldap_extended_response(conn, msgid, type, result_code, NULL);
}

int
ldap_respond(struct request *req, int code)
{
	if (code >= 0)
		send_ldap_result(req->conn, req->msgid, req->type + 1, code);
	request_free(req);
	return code;
}

int
ldap_abandon(struct request *req)
{
	long long	 msgid;
	struct search	*search;

	if (ber_scanf_elements(req->op, "i", &msgid) != 0) {
		request_free(req);
		return -1;	/* protocol error, but don't respond */
	}

	TAILQ_FOREACH(search, &req->conn->searches, next) {
		if (search->req->msgid == msgid) {
			/* unlinks the search from conn->searches */
			search_close(search);
			break;
		}
	}
	request_free(req);
	return -1;
}

int
ldap_unbind(struct request *req)
{
	log_debug("current bind dn = %s", req->conn->binddn);
	conn_disconnect(req->conn);
	request_free(req);
	return -1;		/* don't send any response */
}

int
ldap_starttls(struct request *req)
{
	req->conn->s_flags |= F_STARTTLS;
	return LDAP_SUCCESS;
}

int
ldap_extended(struct request *req)
{
	int			 i, rc = LDAP_PROTOCOL_ERROR;
	char			*oid = NULL;
	struct ber_element	*ext_val = NULL;
	struct {
		const char	*oid;
		int (*fn)(struct request *);
	} extended_ops[] = {
		{ "1.3.6.1.4.1.1466.20037", ldap_starttls },
		{ NULL }
	};

	if (ber_scanf_elements(req->op, "{se", &oid, &ext_val) != 0)
		goto done;

	log_debug("got extended operation %s", oid);
	req->op = ext_val;

	for (i = 0; extended_ops[i].oid != NULL; i++) {
		if (strcmp(oid, extended_ops[i].oid) == 0) {
			rc = extended_ops[i].fn(req);
			break;
		}
	}

	if (extended_ops[i].fn == NULL)
		log_warnx("unimplemented extended operation %s", oid);

done:
	send_ldap_extended_response(req->conn, req->msgid, LDAP_RES_EXTENDED,
	    rc, oid);

	request_free(req);
	return 0;
}

pid_t
ldape(struct passwd *pw, char *csockpath, int pipe_parent2ldap[2])
{
	int			 on = 1;
	pid_t			 pid;
	struct namespace	*ns;
	struct listener		*l;
	struct sockaddr_un	*sun = NULL;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct event		 ev_sigchld;
	struct event		 ev_sighup;
	char			 host[128];

	TAILQ_INIT(&conn_list);

	pid = fork();
	if (pid < 0)
		fatal("ldape: fork");
	if (pid > 0)
		return pid;

	setproctitle("ldap server");
	event_init();

	signal_set(&ev_sigint, SIGINT, ldape_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, ldape_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, ldape_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, ldape_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	close(pipe_parent2ldap[0]);

	/* Initialize parent imsg events. */
	if ((iev_ldapd = calloc(1, sizeof(struct imsgev))) == NULL)
		fatal("calloc");
	imsg_init(&iev_ldapd->ibuf, pipe_parent2ldap[1]);
	iev_ldapd->handler = ldape_dispatch_ldapd;
	imsg_event_add(iev_ldapd);

	/* Initialize control socket. */
	bzero(&csock, sizeof(csock));
	csock.cs_name = csockpath;
	control_init(&csock);
	control_listen(&csock);
	TAILQ_INIT(&ctl_conns);

	/* Initialize LDAP listeners.
	 */
	TAILQ_FOREACH(l, &conf->listeners, entry) {
		l->fd = socket(l->ss.ss_family, SOCK_STREAM, 0);
		if (l->fd < 0)
			fatal("ldape: socket");

		setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

		if (l->ss.ss_family == AF_UNIX) {
			sun = (struct sockaddr_un *)&l->ss;
			log_info("listening on %s", sun->sun_path);
			if (unlink(sun->sun_path) == -1 && errno != ENOENT)
				fatal("ldape: unlink");
		} else {
			print_host(&l->ss, host, sizeof(host));
			log_info("listening on %s:%d", host, ntohs(l->port));
		}

		if (bind(l->fd, (struct sockaddr *)&l->ss, l->ss.ss_len) != 0)
			fatal("ldape: bind");
		if (listen(l->fd, 20) != 0)
			fatal("ldape: listen");

		if (l->ss.ss_family == AF_UNIX) {
			mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
			if (chmod(sun->sun_path, mode) == -1) {
				unlink(sun->sun_path);
				fatal("ldape: chmod");
			}
		}

		fd_nonblock(l->fd);

		event_set(&l->ev, l->fd, EV_READ|EV_PERSIST, conn_accept, l);
		event_add(&l->ev, NULL);

		ssl_setup(conf, l);
	}

	TAILQ_FOREACH(ns, &conf->namespaces, next) {
		if (namespace_open(ns) != 0)
			fatal(ns->suffix);
	}

	if (pw != NULL) {
		if (chroot(pw->pw_dir) == -1)
			fatal("chroot");
		if (chdir("/") == -1)
			fatal("chdir(\"/\")");

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			fatal("cannot drop privileges");
	}

	log_debug("ldape: entering event loop");
	event_dispatch();

	while ((ns = TAILQ_FIRST(&conf->namespaces)) != NULL)
		namespace_remove(ns);

	control_cleanup(&csock);

	log_info("ldape: exiting");
	_exit(0);
}

void
ldape_dispatch_ldapd(int fd, short event, void *ptr)
{
	struct imsgev		*iev = ptr;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	if (imsg_event_handle(iev, event) != 0)
		return;

	ibuf = &iev->ibuf;
	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("ldape_dispatch_ldapd: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LDAPD_AUTH_RESULT: {
			struct conn		*conn;
			struct auth_res		*ares;

			ares = imsg.data;
			log_debug("authentication on conn %i/%lld = %d",
			    ares->fd, ares->msgid, ares->ok);
			conn = conn_by_fd(ares->fd);
			if (conn->bind_req &&
			    conn->bind_req->msgid == ares->msgid)
				ldap_bind_continue(conn, ares->ok);
			else
				log_warnx("spurious auth result");
			break;
		}
		default:
			log_debug("ldape_dispatch_ldapd: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

