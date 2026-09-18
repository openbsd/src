/*	$OpenBSD: sockets.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"

int listener = -1;
int controller = -1;

char *controller_filename = NULL;

int client_count = 0;
int max_clients = 0;
int max_sockets = 0;
int max_sendq = SENDQ_MAX;

struct rtr_socket *rtr_socket_table;

struct pollfd *poll_table;
int poll_table_count = 0;

uint8_t rtr_max_version = RTR_MAX_VERSION;

ssize_t sendq_block_size = SENDQ_BLOCK - sizeof(struct rtr_sendq_link);

#define rtr_min(a, b)	(((a) > (b)) ? (b) : (a))
#define rtr_max(a, b)	(((a) < (b)) ? (b) : (a))

struct rtr_socket *
fd_to_socket(int fd)
{
	assert(fd >= 0);
	assert(fd < max_sockets);

	return rtr_socket_table + fd;
}

int
is_listener(int fd)
{
	assert(fd >= 0);

	if (fd == listener)
		return 1;
	if (fd == controller)
		return 1;
	return 0;
}

struct pollfd *
poll_find(int fd)
{
	int i;

	assert(fd >= 0);

	for (i = 0; i < poll_table_count; i++) {
		if (poll_table[i].fd == fd)
			return poll_table + i;
	}
	return NULL;
}

int
poll_index(int fd)
{
	int i;

	assert(fd >= 0);

	for (i = 0; i < poll_table_count; i++) {
		if (poll_table[i].fd == fd)
			return i;
	}
	return -1;
}

struct pollfd *
poll_add(int fd, short events)
{
	assert(fd >= 0);

	if (poll_table_count >= max_sockets)
		return NULL;

	if (poll_find(fd))
		return NULL;

	poll_table[poll_table_count].fd = fd;
	poll_table[poll_table_count].events = events;
	poll_table[poll_table_count].revents = 0;

	poll_table_count++;

	return poll_table + (poll_table_count - 1);
}

void
poll_remove(int fd)
{
	int index;
	int last;

	assert(fd >= 0);

	if (poll_table_count < 1)
		return;

	index = poll_index(fd);
	if (index < 0)
		return;

	last = poll_table_count - 1;

	if (index < last)
		poll_table[index] = poll_table[last];

	poll_table_count--;
}

int
poll_isset_events(int fd, short events)
{
	struct pollfd *p;

	assert(fd >= 0);

	p = poll_find(fd);
	if (p == NULL)
		return 0;
	if (p->events & events)
		return 1;
	return 0;
}

int
poll_isset_revents(int fd, short revents)
{
	struct pollfd *p;

	assert(fd >= 0);

	p = poll_find(fd);
	if (p == NULL)
		return 0;
	if (p->revents & revents)
		return 1;
	return 0;
}

void
init_socket(int fd, int type, uint32_t flags, ssize_t read_block_size,
    ssize_t write_block_size, ssize_t sendq_max, uint8_t version)
{
	struct rtr_socket *s;

	if (read_block_size > READ_BLOCK)
		read_block_size = READ_BLOCK;

	if (write_block_size > WRITE_BLOCK)
		write_block_size = WRITE_BLOCK;

	if (read_block_size < 0)
		read_block_size = 0;

	if (write_block_size < 0)
		write_block_size = 0;

	if (sendq_max < 0)
		sendq_max = 0;

	s = fd_to_socket(fd);
	if (s == NULL)
		err(1, NULL);

	s->fd = fd;
	s->name = malloc(RTR_SOCKET_NAME_LENGTH);
	if (s->name == NULL)
		err(1, NULL);
	s->name[0] = '\0';
	s->type = type;
	s->flags = flags;
	memset(&s->client, 0, sizeof(struct sockaddr_in));
	s->read_length = 0;
	s->read_block_size = read_block_size;
	if (read_block_size > 0) {
		s->read_pdu = malloc(PDU_MAX_LENGTH);
		if (s->read_pdu == NULL)
			err(1, NULL);
	} else {
		s->read_pdu = NULL;
	}
	s->write_length = 0;
	s->write_block_size = write_block_size;
	if (write_block_size > 0) {
		s->write_block = malloc(write_block_size);
		if (s->write_block == NULL)
			err(1, NULL);

	} else {
		s->write_block = NULL;
	}

	s->sendq.head = NULL;
	s->sendq.tail = NULL;
	s->sendq.length = 0;
	s->sendq.max = sendq_max;
	s->version = version;
	memset(&s->stats, 0, sizeof(struct socket_stats));
	RB_INIT(&s->vrp4s);
	RB_INIT(&s->vrp6s);
	RB_INIT(&s->brks);
	RB_INIT(&s->vaps);
}

#define MINIMUM_SOCKETS		128
#define CONTROLLER_SOCKETS	32

/* 1 if error */
int
init_socket_table(FILE *fp, char *bind_str, uint16_t port)
{
	int val;
	struct sockaddr_in server;
	struct sockaddr_un local;

	assert (fp);

	max_sockets = getdtablesize();

	if (max_sockets < 0) {
		fprintf(fp, "error: couldnt get max sockets");
		return 1;
	}

	if (max_sockets < MINIMUM_SOCKETS) {
		fprintf(fp, "error: limit too low");
		return 1;
	}

	max_clients = max_sockets - CONTROLLER_SOCKETS;

	rtr_socket_table = calloc(max_sockets,
	    sizeof(struct rtr_socket));

	if (rtr_socket_table == NULL) {
		fprintf(fp, "error: could not allocate socket table");
		return 1;
	}

	poll_table = calloc(max_sockets, sizeof(struct pollfd));

	if (poll_table == NULL) {
		fprintf(fp, "error: could not allocate poll table");
		return 1;
	}
	poll_table_count = 0;

	/* listener */

	listener = socket(AF_INET, SOCK_STREAM, 0);

	if (listener < 0) {
		fprintf(fp, "error: could not create listener socket\n");
		return 1;
	}

	if (listener >= max_sockets) {
		fprintf(fp, "error: listener socket out of bounds");
		return 1;
	}

	val = 1;
	if (setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val))) {
		fprintf(fp, "error: unable to set reuse port on socket\n");
		return 1;
	}

	val = fcntl(listener, F_GETFL, 0);
	fcntl(listener, F_SETFL, val | O_NONBLOCK);

	init_socket(listener, RTR_SOCKET_TYPE_UNKNOWN, 0, 0, 0, 0,
	    RTR_DEFAULT_VERSION);

	memset(&server, 0, sizeof(struct sockaddr_in));

	server.sin_family = AF_INET;
	if (bind_str) {
		server.sin_addr.s_addr = inet_addr(bind_str);
	} else {
		server.sin_addr.s_addr = htobe32(INADDR_ANY);
	}
	server.sin_port = htobe16(port);

	if (bind(listener, (struct sockaddr *) &server, sizeof(server)) < 0) {
		fprintf(fp, "error: could not bind socket to listener port\n");
		return 1;
	}

	if (listen(listener, SOMAXCONN) < 0) {
		fprintf(fp, "error: could not listen on listener port\n");
		return 1;
	}

	poll_add(listener, POLLIN);

	/* controller */

	controller = socket(AF_UNIX, SOCK_STREAM, 0);

	if (controller < 0) {
		fprintf(fp, "error: could not create controller socket\n");
		return 1;
	}

	if (controller >= max_sockets) {
		fprintf(fp, "error: controller socket out of bounds");
		return 1;
	}

	val = fcntl(controller, F_GETFL, 0);
	fcntl(controller, F_SETFL, val | O_NONBLOCK);

	init_socket(controller, RTR_SOCKET_TYPE_UNKNOWN, 0, 0, 0, 0,
	    RTR_DEFAULT_VERSION);

	unlink(controller_filename ? controller_filename : CONTROLLER_FILENAME);

	memset(&local, 0, sizeof(struct sockaddr_un));

	local.sun_family = AF_UNIX;

	strncpy(local.sun_path,
	    controller_filename ? controller_filename : CONTROLLER_FILENAME,
	    sizeof(local.sun_path) - 1);

	umask(S_IXUSR | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);

	if (bind(controller, (struct sockaddr *) &local, sizeof(local)) < 0) {
		fprintf(fp, "error: could not bind controller socket (%s)\n",
		    local.sun_path);
		return 1;
	}

	if (listen(controller, SOMAXCONN) < 0) {
		fprintf(fp, "error: could not listen on controller socket\n");
		return 1;
	}

	poll_add(controller, POLLIN);
	return 0;
}

int
rtr_errno_ignore(int e)
{
	switch (e) {
#if (EWOULDBLOCK != EAGAIN)
	case EAGAIN:
#endif
	case EWOULDBLOCK:
	case EINTR:
	case ENOBUFS:
		return 1;
	}
	return 0;
}

ssize_t
rtr_flush_write(struct rtr_socket *s)
{
	ssize_t ret;
	struct pollfd *pfd;

	assert(s);

	if (s->fd < 0)
		return -1;
	pfd = poll_find(s->fd);
	if (pfd == NULL)
		return -1;
	if (is_listener(s->fd))
		return -1;
	if (!(pfd->events & POLLIN) && !(pfd->events & POLLOUT))
		return -1;
	if (s->write_length <= 0)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;

	ret = write(s->fd, s->write_block, s->write_length);

	if (ret < 0) {
		if (!rtr_errno_ignore(errno)) {
			switch (s->type) {
			case RTR_SOCKET_TYPE_CLIENT:
			case RTR_SOCKET_TYPE_CONTROLLER:
				logx(0, "Write error to %s (%s)\n",
				    s->name,
				    strerror(errno));
				break;
			case RTR_SOCKET_TYPE_UNKNOWN:
			default:
				logx(0, "Write error to unknown type "
				    "%d socket %d (%s)\n",
				    s->type,
				    s->fd,
				    strerror(errno));
				break;
			}

			SetSocketFlag(s, RTR_SOCKET_FLAG_CLOSED);
			return -1;
		}
		ret = 0;
	}
	if (!ret)
		return 0;

	if (ret < s->write_length) {
		memmove(s->write_block, s->write_block + ret,
		    s->write_length - ret);

		if (!(pfd->events & POLLOUT))
			pfd->events |= POLLOUT; /* set */
	} else {
		if (!s->sendq.head) {
			if (pfd->events & POLLOUT)
				pfd->events &= (~POLLOUT); /* clr */
		}
	}

	s->write_length -= ret;

	addstats(global_stats.total_bytes_out, ret);
	addstats(s->stats.total_bytes_out, ret);
	return ret;
}

void
rtr_flushall_write(void)
{

	int fd;
	int i;

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;
		if (!is_listener(fd) &&
		    ((POLLEVENT(i) & POLLIN) || (POLLEVENT(i) & POLLOUT)) &&
		    TestSocketFlag(rtr_socket_table+fd, RTR_SOCKET_FLAG_INUSE)
		    && !TestSocketFlag(rtr_socket_table+fd,
		    RTR_SOCKET_FLAG_CLOSED)) {
			rtr_flush_write(rtr_socket_table+fd);
		}
	}
}

/* 1 on fail */
int
rtr_sendq_add(struct rtr_socket *s, void *data, int length)
{

	ssize_t sendq_move_length;
	ssize_t sendq_read_index;

	struct rtr_sendq_link *new_sendq_link;

	assert(s);
	assert(data);

	if (length < 0)
		return 1;
	if (length == 0)
		return 0;

	sendq_read_index = 0;

	/* sendq overflow! */
	if (s->sendq.length + length > s->sendq.max)
		return 1;

	if (!s->sendq.head) {
		/* no sendq exists! */

		new_sendq_link = malloc(SENDQ_BLOCK);
		if (new_sendq_link == NULL)
			err(1, NULL);
;
		new_sendq_link->length = 0;
		new_sendq_link->offset = 0;
		new_sendq_link->next = NULL;

		s->sendq.head = new_sendq_link;
		s->sendq.tail = new_sendq_link;
		s->sendq.length = 0;
	}

	while (sendq_read_index < length) {
		/* check length of input remaining versus */
		/* length left in this sendq block */
		sendq_move_length = rtr_min(length - sendq_read_index,
		sendq_block_size - s->sendq.tail->length);

		if (sendq_move_length > 0) {
			memcpy(s->sendq.tail->sendq_block +
			    s->sendq.tail->length, (unsigned char *)data +
			    sendq_read_index, sendq_move_length);

			sendq_read_index += sendq_move_length;
			s->sendq.tail->length += sendq_move_length;
			s->sendq.length += sendq_move_length;
		}

		if (sendq_read_index < length) {
			/* we still have more to add in the */
			/* sendq...we need a new block */
			new_sendq_link = malloc(SENDQ_BLOCK);

			if (new_sendq_link == NULL)
				err(1, NULL);

			new_sendq_link->length = 0;
			new_sendq_link->offset = 0;
			new_sendq_link->next = NULL;

			s->sendq.tail->next = new_sendq_link;
			s->sendq.tail = new_sendq_link;
		}
	}
	return 0;
}

void
rtr_sendq_pop(struct rtr_socket *s, int length)
{
	int sendq_pop_index = 0;
	int sendq_pop_willing;
	int sendq_pop_move;
	struct rtr_sendq_link *sendq_next;

	assert(s);

	/* trunc length to sendq length */
	sendq_pop_willing = rtr_min(length, s->sendq.length);

	while (sendq_pop_index < sendq_pop_willing) {
		/* length of remaining head sendq block versus */
		/* how much remains to remove */
		sendq_pop_move = rtr_min(s->sendq.head->length -
		    s->sendq.head->offset,
		    sendq_pop_willing - sendq_pop_index);

		if (sendq_pop_move > 0) {
			s->sendq.head->offset += sendq_pop_move;
			sendq_pop_index += sendq_pop_move;
			s->sendq.length -= sendq_pop_move;
		}

		if (s->sendq.head->length == s->sendq.head->offset) {
			/* we pulled everything out of this block, remove it. */
			sendq_next = s->sendq.head->next;
			free(s->sendq.head);
			s->sendq.head = sendq_next;
			if (!s->sendq.head)
				s->sendq.tail = NULL;
		}

	}
}

void
rtr_sendq_popall(struct rtr_socket *s)
{
	assert(s);
	rtr_sendq_pop(s, s->sendq.length);
}

/* 0 sendq is now empty */
/* 1 on fail (theres more in the sendq) */
int
rtr_sendq_flush(struct rtr_socket *s)
{

	int sendq_flush_move;
	int sendq_flush_index = 0;
	int sendq_flush_willing;
	struct rtr_sendq_link *sendq_next;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!s->sendq.head)
		return 0;

	/* trunc write block remaining length to sendq length */
	sendq_flush_willing = rtr_min((s->write_block_size > WRITE_BLOCK ?
	    WRITE_BLOCK : s->write_block_size) - s->write_length,
	    s->sendq.length);

	while (sendq_flush_index < sendq_flush_willing) {
		/* length of remaining head sendq block versus */
		/* how much remains to remove */

		sendq_flush_move = rtr_min(s->sendq.head->length -
		    s->sendq.head->offset,
		    sendq_flush_willing - sendq_flush_index);

		if (sendq_flush_move > 0) {
			memcpy(s->write_block + s->write_length,
			    s->sendq.head->sendq_block + s->sendq.head->offset,
			    sendq_flush_move);

			s->sendq.head->offset += sendq_flush_move;
			s->write_length += sendq_flush_move;
			sendq_flush_index += sendq_flush_move;
			s->sendq.length -= sendq_flush_move;
		}

		if (s->sendq.head->length == s->sendq.head->offset) {
			/* we pulled everything out of this block, remove it. */

			sendq_next = s->sendq.head->next;
			free(s->sendq.head);
			s->sendq.head = sendq_next;
			if (!s->sendq.head)
				s->sendq.tail = NULL;
		}
	}

	if (s->sendq.length > 0)
		return 1;
	return 0;
}

ssize_t
writeto(struct rtr_socket *s, void *pdu)
{
	struct pdu_header *ph;
	ssize_t pdu_length;
	int writeto_move;
	struct pollfd *pfd;

	assert(s);
	assert(pdu);

	ph = pdu;

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->fd < 0)
		return -1;
	pfd = poll_find(s->fd);
	if (pfd == NULL)
		return -1;

	/* pdu is in network order */

	pdu_length = be32toh(ph->length);

	if (s->sendq.head) {
		if (rtr_sendq_add(s, (unsigned char *)ph, pdu_length) == 0) {
			if (!(pfd->events & POLLOUT))
				pfd->events |= POLLOUT; /* set */
		} else {
			goto sendq_overflow;
		}
		return pdu_length;
	}

	writeto_move = rtr_min(pdu_length, (s->write_block_size > WRITE_BLOCK ?
	    WRITE_BLOCK : s->write_block_size) - s->write_length);

	if (writeto_move > 0) {
		memcpy(s->write_block + s->write_length, ph, writeto_move);
		s->write_length += writeto_move;
	}

	if (writeto_move < pdu_length) {
		if (rtr_sendq_add(s, (unsigned char *)ph + writeto_move,
		    pdu_length - writeto_move) == 0) {
			if (!(pfd->events & POLLOUT))
				pfd->events |= POLLOUT; /* set */
		} else {
			goto sendq_overflow;
		}
	}

	return pdu_length;

	sendq_overflow:

	SetSocketFlag(s, RTR_SOCKET_FLAG_CLOSED);
	switch (s->type) {
	case RTR_SOCKET_TYPE_CLIENT:
	case RTR_SOCKET_TYPE_CONTROLLER:
		logx(0, "SendQ exceeded for %s\n",
		    s->name);
		break;
	case RTR_SOCKET_TYPE_UNKNOWN:
	default:
		logx(0, "SendQ exceeded for unknown type "
		    "%d connection on socket %d\n",
		    s->type, s->fd);
		break;
	}

	return -1;
}

void
rtr_close(struct rtr_socket *s)
{
	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return;

	if (s->fd < 0)
		return;

	if (is_listener(s->fd))
		return;

	switch (s->type) {
	case RTR_SOCKET_TYPE_CLIENT:
	case RTR_SOCKET_TYPE_CONTROLLER:
		logx(0, "Closing connection to %s\n",
		    s->name);
		break;
	case RTR_SOCKET_TYPE_UNKNOWN:
	default:
		logx(0, "Closing unknown type %d connection on socket %d\n",
		    s->type, s->fd);
		break;
	}

	remove_sched_socket(&scheduler, s);

	rtr_flush_write(s);

	if (s->type == RTR_SOCKET_TYPE_CLIENT)
		client_count--;

	poll_remove(s->fd);

	rtr_sendq_popall(s);

	free(s->name);
	free(s->read_pdu);
	free(s->write_block);

	free_vrp4_tree(&s->vrp4s);
	free_vrp6_tree(&s->vrp6s);
	free_brk_tree(&s->brks);
	free_vap_tree(&s->vaps);

	init_socket(s->fd, RTR_SOCKET_TYPE_UNKNOWN, 0, 0, 0, 0,
	    RTR_DEFAULT_VERSION);

	close(s->fd);
}

void
rtr_flushall_closed(void)
{
	int fd, i;
	struct rtr_socket *s;

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;
		s = fd_to_socket(fd);
		if (s == NULL)
			err(1, NULL);

		if ((POLLEVENT(i) & POLLIN) &&
		    !is_listener(fd) &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE)
		    && TestSocketFlag(s,
		    RTR_SOCKET_FLAG_CLOSED) &&
		    s->type != RTR_SOCKET_TYPE_UNKNOWN) {
			rtr_close(s);
		}
	}
}

void
rtr_shutdown(int restart)
{
	int fd, i;
	struct rtr_socket *s;
	char *reason;
	uint16_t error_code;

	if (restart)
		reason = "Restarting server";
	else
		reason = "Server shutting down";

	logx(0, "%s\n", reason);

	logx(0, "Client count is %d\n", client_count);

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;
		s = fd_to_socket(fd);
		if (s == NULL)
			err(1, NULL);

		if (!(POLLEVENT(i) & POLLIN) ||
		    is_listener(fd) ||
		    !TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) ||
		    s->type != RTR_SOCKET_TYPE_CLIENT ||
		    !TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
			continue;

		if (s->version >= RTR_VERSION_2)
			error_code = restart ? CACHE_RESTART : CACHE_SHUTDOWN;
		else
			error_code = INTERNAL_ERROR;

		senderrorto_one(s, error_code, NULL, reason);

		rtr_flush_write(s);
	}

	exit(0);
}


void
core_loop(void)
{
	int fd, i, val, delay;
	struct sockaddr_in client;
	socklen_t client_len;
	unsigned char read_block[READ_BLOCK];
	ssize_t read_length, read_move_length, read_index;
	struct rtr_socket *s;
	struct pdu_header *ph;
	uint32_t pdu_length;

	int (*command_function)(struct rtr_socket *, struct pdu_header *);

	for (;;) {
		rtr_gettime();
		delay = time_until_next_event(now);

		poll(poll_table, poll_table_count, delay);

		if (sigflags != 0)
			signal_processor();

		rtr_gettime();
		process_scheduler_events(now.tv_sec);

		if (listener >= 0 && poll_isset_revents(listener, POLLIN)) {
			/* we have a new client! */

			client_len = sizeof(struct sockaddr_in);

			fd = accept(listener, (struct sockaddr *)&client,
			    &client_len);
			if (fd < 0)
				continue;

			client.sin_port = be16toh(client.sin_port);

			val = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, val | O_NONBLOCK);

			logx(0, "Opening client connection from %s:%u on "
			    "socket %d\n", inet_ntoa(client.sin_addr),
			    client.sin_port, fd);

			if (fd >= max_sockets) {
				logx(0, "Rejecting client connection %s:%u "
				    "(too many sockets)\n",
				    inet_ntoa(client.sin_addr),
				    client.sin_port);
				close(fd);
				continue;
			}

			if (client_count >= max_clients) {
				logx(0, "Rejecting client connection %s:%u "
				    "(too many clients)\n",
				    inet_ntoa(client.sin_addr),
				    client.sin_port);
				close(fd);
				continue;
			}

			val = 1;
			if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE,
			    &val, sizeof(val)) == -1) {
				logx(0, "Rejecting client connection %s:%u "
				    "(could not set keepalive)\n",
				    inet_ntoa(client.sin_addr),
				    client.sin_port);
				close(fd);
				continue;
			}

			client_count++;

			poll_add(fd, POLLIN);

			init_socket(fd, RTR_SOCKET_TYPE_CLIENT,
			    RTR_SOCKET_FLAG_INUSE, READ_BLOCK,
			    WRITE_BLOCK, max_sendq,
			    RTR_DEFAULT_VERSION);

			s = fd_to_socket(fd);
			if (s == NULL)
				err(1, NULL);

			s->client = client;

			rtr_gettime();
			s->stats.connect_time = now.tv_sec;

			snprintf(s->name, RTR_SOCKET_NAME_LENGTH,
			    "client %s:%d (%d)",
			    inet_ntoa(client.sin_addr),
			    client.sin_port, fd);

			addstats(global_stats.total_client_connects, 1);

			rtr_gettime();
			schedule_handshake_timeout(now.tv_sec, s);
		}

		if (controller >= 0 && poll_isset_revents(controller, POLLIN)) {
			/* we have a new controller */

			fd = accept(controller, NULL, NULL);
			if (fd < 0)
				continue;

			val = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, val | O_NONBLOCK);

			logx(0, "Opening controller connection on socket %d\n",
			    fd);

			if (fd >= max_sockets) {
				logx(0, "Rejecting controller connection "
				    "on socket %d (too many sockets)\n", fd);
				close(fd);
				continue;
			}

			poll_add(fd, POLLIN);

			init_socket(fd, RTR_SOCKET_TYPE_CONTROLLER,
			    RTR_SOCKET_FLAG_INUSE, UNIX_READ_BLOCK,
			    UNIX_WRITE_BLOCK, max_sendq,
			    RTR_MAX_VERSION);

			s = fd_to_socket(fd);
			if (s == NULL)
				err(1, NULL);

			rtr_gettime();
			s->stats.connect_time = now.tv_sec;

			snprintf(s->name, RTR_SOCKET_NAME_LENGTH,
			    "controller socket %d",
			    fd);
			addstats(global_stats.total_controller_connects, 1);
		}

		for (i = 0; i < poll_table_count; i++) {
			fd = poll_table[i].fd;
			/* take care of our existing clients */

			if (is_listener(fd))
				continue;

			s = fd_to_socket(fd);
			if (s == NULL)
				err(1, NULL);

			if ((poll_table[i].revents & POLLOUT) &&
			    (s->type != RTR_SOCKET_TYPE_UNKNOWN) &&
			    TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) &&
			    !TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED)) {
				if (!rtr_sendq_flush(s))
					POLLEVENT(i) &= (~POLLOUT); /* clr */
			}


			if (!(poll_table[i].revents & POLLIN) ||
			    (s->type == RTR_SOCKET_TYPE_UNKNOWN) ||
			    !TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) ||
			    TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
				continue;

			read_length = read(fd, read_block,
			    s->read_block_size > READ_BLOCK ?
			    READ_BLOCK : s->read_block_size);

			if (read_length < 0) {
				/* possible error */
				if (rtr_errno_ignore(errno))
					continue;

				switch (s->type) {
				case RTR_SOCKET_TYPE_CLIENT:
				case RTR_SOCKET_TYPE_CONTROLLER:
					logx(0, "Read error from %s (%s)\n",
					    s->name,
					    strerror(errno));
					break;
				case RTR_SOCKET_TYPE_UNKNOWN:
				default:
					logx(0, "Read error from unknown type "
					    "%d socket %d (%s)\n",
					    s->type,
					    fd,
					    strerror(errno));
					break;
				}

				rtr_close(s);
				continue;
			}

			if (!read_length) {
				/* eof */
				switch (s->type) {
				case RTR_SOCKET_TYPE_CLIENT:
				case RTR_SOCKET_TYPE_CONTROLLER:
					logx(0, "Read EOF from %s\n",
					    s->name);
					break;
				case RTR_SOCKET_TYPE_UNKNOWN:
				default:
					logx(0, "Read EOF from unknown type "
					    "%d socket %d\n",
					    s->type,
					    fd);
					break;
				}

				rtr_close(s);
				continue;
			}

			if (read_length > 0) {
				addstats(global_stats.total_bytes_in,
				    read_length);
				addstats(s->stats.total_bytes_in,
				    read_length);
			}

			read_index = 0;

			while (read_index < read_length) {
				ph = (struct pdu_header *)s->read_pdu;

				if (s->read_length <
				    (ssize_t)sizeof(struct pdu_header)) {
					read_move_length =
					    rtr_min(
					    (ssize_t)sizeof(struct pdu_header)
					    - s->read_length,
					    read_length - read_index);

					if (read_move_length > 0) {
						memcpy(s->read_pdu +
						    s->read_length,
						    read_block +
						    read_index,
						    read_move_length);
					}

					read_index += read_move_length;
					s->read_length += read_move_length;

					if (s->read_length <
					    (ssize_t)sizeof(struct pdu_header))
						break;
				}

				/* we have a complete header */

				pdu_length = be32toh(ph->length);

				if (pdu_length < sizeof(struct pdu_header)) {
					/* the PDU is too small */

					senderrorto_one(s, CORRUPT_DATA, NULL,
					    "The PDU is too small (%d bytes)",
					    pdu_length);

					rtr_close(s);
					break;
				}

				if (pdu_length > PDU_MAX_LENGTH) {
					/* the PDU is too large */

					senderrorto_one(s, CORRUPT_DATA, NULL,
					    "The PDU is too large (%d bytes)",
					    pdu_length);

					rtr_close(s);
					break;
				}

				read_move_length = rtr_min(
					pdu_length - s->read_length,
					read_length - read_index);

				if (read_move_length > 0) {
					memcpy(s->read_pdu + s->read_length,
					    read_block + read_index,
					    read_move_length);
				}

				read_index += read_move_length;
				s->read_length += read_move_length;

				if (s->read_length < pdu_length)
					break;

				/* we have a complete PDU */

				pdu_ntoh(s, ph);

				/* lookup and execute PDU function */
				command_function = command_lookup(
				    ph->version,
				    ph->type,
				    s->type);

				if (command_function) {
					if ((*command_function)(s, ph) != 0) {
						rtr_close(s);
						break;
					}
				} else {
					senderrorto_one(s,
					    UNSUPPORTED_PDU_TYPE,
					    ph,
					    "Unsupported PDU type "
					    "%d for version %d",
					    ph->type, ph->version);
					rtr_close(s);
					break;
				}
				/* this PDU has been processed. */
				/* clear read buffer. */

				s->read_length=0;

			} /* while (read_index < read_length) */

		} /* for (i = 0; i < poll_table_count; i++) */

		rtr_flushall_closed();
		rtr_flushall_write();

	} /* end of for (;;) */
}
