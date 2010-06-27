/*	$OpenBSD: conn.c,v 1.3 2010/06/27 18:31:13 martinh Exp $ */

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

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "ldapd.h"

int			 conn_dispatch(struct conn *conn);
unsigned long		 ldap_application(struct ber_element *elm);

struct conn_list	 conn_list;

unsigned long
ldap_application(struct ber_element *elm)
{
	return BER_TYPE_OCTETSTRING;
}

void
request_free(struct request *req)
{
	if (req->root != NULL)
		ber_free_elements(req->root);
	free(req);
}

void
conn_close(struct conn *conn)
{
	struct search	*search, *next;

	log_debug("closing connection %d", conn->fd);

	/* Cancel any ongoing searches on this connection. */
	for (search = TAILQ_FIRST(&conn->searches); search; search = next) {
		next = TAILQ_NEXT(search, next);
		search_close(search);
	}

	/* Cancel any queued requests on this connection. */
	namespace_cancel_conn(conn);

	ssl_session_destroy(conn);

	TAILQ_REMOVE(&conn_list, conn, next);
	ber_free(&conn->ber);
	if (conn->bev != NULL)
		bufferevent_free(conn->bev);
	close(conn->fd);
	free(conn->binddn);
	free(conn);

	--stats.conns;
}

/* Marks a connection for disconnect. The connection will be closed when
 * any remaining data has been flushed to the socket.
 */
void
conn_disconnect(struct conn *conn)
{
	conn->disconnect = 1;
	bufferevent_enable(conn->bev, EV_WRITE);
}

void
request_dispatch(struct request *req)
{
	unsigned long		 i;
	struct {
		unsigned long	 type;
		int (*fn)(struct request *);
	} requests[] = {
		{ LDAP_REQ_SEARCH,	ldap_search },
		{ LDAP_REQ_BIND,	ldap_bind },
		{ LDAP_REQ_ADD,		ldap_add },
		{ LDAP_REQ_UNBIND_30,	ldap_unbind },
		{ LDAP_REQ_MODIFY,	ldap_modify },
		{ LDAP_REQ_ABANDON_30,	ldap_abandon },
		{ LDAP_REQ_DELETE_30,	ldap_delete },
		{ LDAP_REQ_EXTENDED,	ldap_extended },
		{ 0,			NULL }
	};

	/* RFC4511, section 4.2.1 says we shouldn't process other requests
	 * while binding. A bind operation can, however, be aborted by sending
	 * another bind operation.
	 */
	if (req->conn->bind_req != NULL && req->type != LDAP_REQ_BIND) {
		log_warnx("got request while bind in progress");
		ldap_respond(req, LDAP_SASL_BIND_IN_PROGRESS);
		return;
	}

	for (i = 0; requests[i].fn != NULL; i++) {
		if (requests[i].type == req->type) {
			requests[i].fn(req);
			break;
		}
	}

	if (requests[i].fn == NULL) {
		log_warnx("unhandled request %d (not implemented)", req->type);
		ldap_respond(req, LDAP_PROTOCOL_ERROR);
	}
}

int
conn_dispatch(struct conn *conn)
{
	int			 class;
	struct request		*req;

	++stats.requests;

	if ((req = calloc(1, sizeof(*req))) == NULL) {
		log_warn("calloc");
		conn_disconnect(conn);
		return -1;
	}

	req->conn = conn;

	if ((req->root = ber_read_elements(&conn->ber, NULL)) == NULL) {
		if (errno != ECANCELED) {
			log_warnx("protocol error");
			conn_disconnect(conn);
		}
		request_free(req);
		return -1;
	}

	/* Read message id and request type.
	 */
	if (ber_scanf_elements(req->root, "{ite",
	    &req->msgid, &class, &req->type, &req->op) != 0) {
		log_warnx("protocol error");
		conn_disconnect(conn);
		request_free(req);
		return -1;
	}

	log_debug("got request type %d, id %lld", req->type, req->msgid);
	request_dispatch(req);
	return 0;
}

void
conn_read(struct bufferevent *bev, void *data)
{
	size_t			 nused = 0;
	struct conn		*conn = data;
	struct evbuffer		*input;

	input = EVBUFFER_INPUT(bev);
	ber_set_readbuf(&conn->ber,
	    EVBUFFER_DATA(input), EVBUFFER_LENGTH(input));

	while (conn->ber.br_rend - conn->ber.br_rptr > 0) {
		if (conn_dispatch(conn) == 0)
			nused += conn->ber.br_rptr - conn->ber.br_rbuf;
		else
			break;
	}

	evbuffer_drain(input, nused);
}

void
conn_write(struct bufferevent *bev, void *data)
{
	struct search	*search, *next;
	struct conn	*conn = data;

	/* Continue any ongoing searches.
	 * Note that the search may be unlinked and freed by conn_search.
	 */
	for (search = TAILQ_FIRST(&conn->searches); search; search = next) {
		next = TAILQ_NEXT(search, next);
		conn_search(search);
	}

	if (conn->disconnect)
		conn_close(conn);
	else if (conn->s_flags & F_STARTTLS) {
		conn->s_flags &= ~F_STARTTLS;
		bufferevent_free(conn->bev);
		conn->bev = NULL;
		ssl_session_init(conn);
	}
}

void
conn_err(struct bufferevent *bev, short why, void *data)
{
	struct conn	*conn = data;

	if ((why & EVBUFFER_EOF) == EVBUFFER_EOF)
		log_debug("end-of-file on connection %i", conn->fd);
	else if ((why & EVBUFFER_TIMEOUT) == EVBUFFER_TIMEOUT)
		log_debug("timeout on connection %i", conn->fd);
	else
		log_warnx("error 0x%02X on connection %i", why, conn->fd);

	conn_close(conn);
}

void
conn_accept(int fd, short why, void *data)
{
	int			 afd;
	socklen_t		 addrlen;
	struct conn		*conn;
	struct listener		*l = data;
	struct sockaddr_storage	 remote_addr;
	char			 host[128];

	addrlen = sizeof(remote_addr);
	afd = accept(fd, (struct sockaddr *)&remote_addr, &addrlen);
	if (afd == -1) {
		log_warn("accept");
		return;
	}

	if (l->ss.ss_family == AF_UNIX) {
		uid_t		 euid;
		gid_t		 egid;

		if (getpeereid(afd, &euid, &egid) == -1)
			log_warnx("conn_accept: getpeereid");
		else
			log_debug("accepted local connection by uid %d", euid);
	} else {
		print_host(&remote_addr, host, sizeof(host));
		log_debug("accepted connection from %s on fd %d", host, afd);
	}

	fd_nonblock(afd);

	if ((conn = calloc(1, sizeof(*conn))) == NULL) {
		log_warn("malloc");
		close(afd);
		return;
	}
	conn->ber.fd = -1;
	conn->s_l = l;
	ber_set_application(&conn->ber, ldap_application);
	conn->fd = afd;

	if (l->flags & F_LDAPS) {
		ssl_session_init(conn);
	} else {
		conn->bev = bufferevent_new(afd, conn_read, conn_write,
		    conn_err, conn);
		if (conn->bev == NULL) {
			log_warn("conn_accept: bufferevent_new");
			close(afd);
			free(conn);
			return;
		}
		bufferevent_enable(conn->bev, EV_READ);
		bufferevent_settimeout(conn->bev, 0, 60);
	}

	TAILQ_INIT(&conn->searches);
	TAILQ_INSERT_HEAD(&conn_list, conn, next);

	if (l->flags & F_SECURE)
		conn->s_flags |= F_SECURE;

	++stats.conns;
}

struct conn *
conn_by_fd(int fd)
{
	struct conn		*conn;

	TAILQ_FOREACH(conn, &conn_list, next) {
		if (conn->fd == fd)
			return conn;
	}
	return NULL;
}

