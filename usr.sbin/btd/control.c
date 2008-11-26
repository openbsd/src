/*	$OpenBSD: control.c,v 1.3 2008/11/26 06:51:43 uwe Exp $	*/

/*
 * Copyright (c) 2008 Uwe Stuehler <uwe@openbsd.org>
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
#include <sys/stat.h>
#include <sys/un.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btd.h"
#include "btctl.h"

#define CONTROL_BACKLOG 5

void socket_blockmode(int, int);
void control_acceptcb(int, short, void *);
void control_readcb(struct bufferevent *, void *);
void control_errorcb(struct bufferevent *, short, void *);
int control_interface_stmt(struct btd *, btctl_interface_stmt *);
int control_attach_stmt(struct btd *, btctl_attach_stmt *);
int control_commit(struct btd *, const struct btd *);

struct control_connection {
	TAILQ_ENTRY(control_connection) entry;
	struct bufferevent *ev;
	struct btd *env;
	struct btd *new_env;
	int fd;
};

static struct event ev_control;
static TAILQ_HEAD(, control_connection) connections =
    TAILQ_HEAD_INITIALIZER(connections);

void
control_init(struct btd *env)
{
	struct sockaddr_un sun;
	mode_t old_umask;
	int fd;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("control_init: socket");

	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, BTD_SOCKET,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatalx("control_init: socket name too long");

	if (unlink(BTD_SOCKET) == -1)
		if (errno != ENOENT)
			fatal("control_init: unlink");

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		fatal("control_init: bind");
	(void)umask(old_umask);

	if (chmod(BTD_SOCKET, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) == -1) {
		log_warn("control_init: chmod");
		close(fd);
		(void)unlink(BTD_SOCKET);
		exit(1);
	}
	
	if (listen(fd, CONTROL_BACKLOG) == -1)
		fatal("control_init: listen");

	socket_blockmode(fd, 0);

	event_set(&ev_control, fd, EV_READ | EV_PERSIST,
	    control_acceptcb, env);
	event_add(&ev_control, NULL);
}

/* called from priv process */
void
control_cleanup(void)
{
	if (unlink(BTD_SOCKET) == -1)
		fatal("control_init: unlink");
}

void
socket_blockmode(int fd, int block)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (block)
		flags &= ~O_NONBLOCK;
	else
		flags |= O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

void
control_acceptcb(int fd, short evflags, void *arg)
{
	struct btd *env = arg;
	struct control_connection *conn;
	struct sockaddr_storage sa;
	socklen_t salen;
	int new_fd;

	salen = sizeof(sa);
	if ((new_fd = accept(fd, (struct sockaddr *)&sa, &salen)) == -1) {
		log_warn("control_eventcb: accept");
		return;
	}

	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		fatal("control_eventcb: calloc");

	conn->env = env;
	conn->fd = new_fd;

	if ((conn->ev = bufferevent_new(new_fd, control_readcb, NULL,
	    control_errorcb, conn)) == NULL)
		fatal("control_errorcb: bufferevent_new");

	TAILQ_INSERT_TAIL(&connections, conn, entry);
	bufferevent_enable(conn->ev, EV_READ);
}

void
control_readcb(struct bufferevent *ev, void *arg)
{
	btctl_interface_stmt interface_stmt;
	btctl_attach_stmt attach_stmt;
	struct control_connection *conn = arg;
	enum btctl_stmt_type stmt_type;
	size_t stmt_size = 0;
	int err;

	if (EVBUFFER_LENGTH(EVBUFFER_INPUT(ev)) < sizeof(stmt_type))
		return;

	memcpy(&stmt_type, EVBUFFER_DATA(EVBUFFER_INPUT(ev)),
	    sizeof(stmt_type));

	switch (stmt_type) {
	case BTCTL_CONFIG:
	case BTCTL_COMMIT:
	case BTCTL_ROLLBACK:
		break;
	case BTCTL_INTERFACE_STMT:
		stmt_size += sizeof(btctl_interface_stmt);
		break;
	case BTCTL_ATTACH_STMT:
		stmt_size += sizeof(btctl_attach_stmt);
		break;
	}

	if (EVBUFFER_LENGTH(EVBUFFER_INPUT(ev)) <= stmt_size)
		return;

	bufferevent_read(ev, &stmt_type, sizeof(stmt_type));
	/*log_debug("stmt %#x size %lu", stmt_type, stmt_size);*/

	switch (stmt_type) {
	case BTCTL_CONFIG:
		if (conn->new_env == NULL) {
			conn->new_env = conf_new();
			err = conn->new_env == NULL ? ENOMEM : 0;
		} else
			err = EALREADY;
		break;

	case BTCTL_COMMIT:
		if (conn->new_env != NULL) {
			err = control_commit(conn->env, conn->new_env);
			if (err == 0) {
				conf_delete(conn->new_env);
				conn->new_env = NULL;
			}
		} else
			err = 0;
		break;

	case BTCTL_ROLLBACK:
		if (conn->new_env != NULL) {
			conf_delete(conn->new_env);
			conn->new_env = NULL;
		}
		err = 0;
		break;

	case BTCTL_INTERFACE_STMT:
		if (conn->new_env == NULL) {
			err = EOPNOTSUPP;
			break;
		}
		bufferevent_read(ev, &interface_stmt, stmt_size);
		err = control_interface_stmt(conn->new_env, &interface_stmt);
		break;

	case BTCTL_ATTACH_STMT:
		if (conn->new_env == NULL) {
			err = EOPNOTSUPP;
			break;
		}
		bufferevent_read(ev, &attach_stmt, stmt_size);
		err = control_attach_stmt(conn->new_env, &attach_stmt);
		break;

	default:
		log_warnx("Invalid control packet of type %#x", stmt_type);
		close(conn->fd);
		return;
	}

	bufferevent_write(ev, &err, sizeof(err));
}

void
control_errorcb(struct bufferevent *ev, short what, void *arg)
{
	struct control_connection *conn = arg;

	TAILQ_REMOVE(&connections, conn, entry);

	if (conn->new_env != NULL)
		conf_delete(conn->new_env);

	bufferevent_free(conn->ev);
	close(conn->fd);
	free(conn);
}

int
control_interface_stmt(struct btd *conf, btctl_interface_stmt *stmt)
{
	struct bt_interface *iface;

	if ((iface = conf_add_interface(conf, &stmt->addr)) == NULL)
		return errno;

	strlcpy(iface->name, stmt->name,  sizeof(iface->name));

	if (stmt->flags & BTCTL_INTERFACE_DISABLED)
		iface->disabled = 1;

	return 0;
}

int
control_attach_stmt(struct btd *conf, btctl_attach_stmt *stmt)
{
	struct bt_device *btdev;

	if ((btdev = conf_add_device(conf, &stmt->addr)) == NULL)
		return errno;

	btdev->type = stmt->type;

	strlcpy(btdev->pin, stmt->pin,  sizeof(btdev->pin));
	btdev->pin_size = stmt->pin_size;

	btdev->flags |= BTDF_ATTACH;

	return 0;
}

int
control_commit(struct btd *env, const struct btd *conf)
{
	return hci_reinit(env, conf);
}
