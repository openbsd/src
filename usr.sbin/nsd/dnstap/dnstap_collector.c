/*
 * dnstap/dnstap_collector.c -- nsd collector process for dnstap information
 *
 * Copyright (c) 2018, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef USE_MINI_EVENT
#  ifdef HAVE_EVENT_H
#    include <event.h>
#  else
#    include <event2/event.h>
#    include "event2/event_struct.h"
#    include "event2/event_compat.h"
#  endif
#else
#  include "mini_event.h"
#endif
#include "dnstap/dnstap_collector.h"
#include "dnstap/dnstap.h"
#include "util.h"
#include "nsd.h"
#include "region-allocator.h"
#include "buffer.h"
#include "namedb.h"
#include "options.h"

struct dt_collector* dt_collector_create(struct nsd* nsd)
{
	int i, sv[2];
	struct dt_collector* dt_col = (struct dt_collector*)xalloc_zero(
		sizeof(*dt_col));
	dt_col->count = nsd->child_count;
	dt_col->dt_env = NULL;
	dt_col->region = region_create(xalloc, free);
	dt_col->send_buffer = buffer_create(dt_col->region,
		/* msglen + is_response + addrlen + is_tcp + packetlen + packet + zonelen + zone + spare + addr */
		4+1+4+1+4+TCP_MAX_MESSAGE_LEN+4+MAXHOSTNAMELEN + 32 +
#ifdef INET6
		sizeof(struct sockaddr_storage)
#else
		sizeof(struct sockaddr_in)
#endif
		);

	/* open pipes in struct nsd */
	nsd->dt_collector_fd_send = (int*)xalloc_array_zero(dt_col->count,
		sizeof(int));
	nsd->dt_collector_fd_recv = (int*)xalloc_array_zero(dt_col->count,
		sizeof(int));
	for(i=0; i<dt_col->count; i++) {
		int fd[2];
		fd[0] = -1;
		fd[1] = -1;
		if(pipe(fd) < 0) {
			error("dnstap_collector: cannot create pipe: %s",
				strerror(errno));
		}
		if(fcntl(fd[0], F_SETFL, O_NONBLOCK) == -1) {
			log_msg(LOG_ERR, "fcntl failed: %s", strerror(errno));
		}
		if(fcntl(fd[1], F_SETFL, O_NONBLOCK) == -1) {
			log_msg(LOG_ERR, "fcntl failed: %s", strerror(errno));
		}
		nsd->dt_collector_fd_recv[i] = fd[0];
		nsd->dt_collector_fd_send[i] = fd[1];
	}

	/* open socketpair */
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
		error("dnstap_collector: cannot create socketpair: %s",
			strerror(errno));
	}
	if(fcntl(sv[0], F_SETFL, O_NONBLOCK) == -1) {
		log_msg(LOG_ERR, "fcntl failed: %s", strerror(errno));
	}
	if(fcntl(sv[1], F_SETFL, O_NONBLOCK) == -1) {
		log_msg(LOG_ERR, "fcntl failed: %s", strerror(errno));
	}
	dt_col->cmd_socket_dt = sv[0];
	dt_col->cmd_socket_nsd = sv[1];

	return dt_col;
}

void dt_collector_destroy(struct dt_collector* dt_col, struct nsd* nsd)
{
	if(!dt_col) return;
	free(nsd->dt_collector_fd_recv);
	nsd->dt_collector_fd_recv = NULL;
	free(nsd->dt_collector_fd_send);
	nsd->dt_collector_fd_send = NULL;
	region_destroy(dt_col->region);
	free(dt_col);
}

void dt_collector_close(struct dt_collector* dt_col, struct nsd* nsd)
{
	int i;
	if(!dt_col) return;
	if(dt_col->cmd_socket_dt != -1) {
		close(dt_col->cmd_socket_dt);
		dt_col->cmd_socket_dt = -1;
	}
	if(dt_col->cmd_socket_nsd != -1) {
		close(dt_col->cmd_socket_nsd);
		dt_col->cmd_socket_nsd = -1;
	}
	for(i=0; i<dt_col->count; i++) {
		if(nsd->dt_collector_fd_recv[i] != -1) {
			close(nsd->dt_collector_fd_recv[i]);
			nsd->dt_collector_fd_recv[i] = -1;
		}
		if(nsd->dt_collector_fd_send[i] != -1) {
			close(nsd->dt_collector_fd_send[i]);
			nsd->dt_collector_fd_send[i] = -1;
		}
	}
}

/* handle command from nsd to dt collector.
 * mostly, check for fd closed, this means we have to exit */
void
dt_handle_cmd_from_nsd(int ATTR_UNUSED(fd), short event, void* arg)
{
	struct dt_collector* dt_col = (struct dt_collector*)arg;
	if((event&EV_READ) != 0) {
		event_base_loopexit(dt_col->event_base, NULL);
	}
}

/* read data from fd into buffer, true when message is complete */
static int read_into_buffer(int fd, struct buffer* buf)
{
	size_t msglen;
	ssize_t r;
	if(buffer_position(buf) < 4) {
		/* read the length of the message */
		r = read(fd, buffer_current(buf), 4 - buffer_position(buf));
		if(r == -1) {
			if(errno == EAGAIN || errno == EINTR) {
				/* continue to read later */
				return 0;
			}
			log_msg(LOG_ERR, "dnstap collector: read failed: %s",
				strerror(errno));
			return 0;
		}
		buffer_skip(buf, r);
		if(buffer_position(buf) < 4)
			return 0; /* continue to read more msglen later */
	}

	/* msglen complete */
	msglen = buffer_read_u32_at(buf, 0);
	/* assert we have enough space, if we don't and we wanted to continue,
	 * we would have to skip the message somehow, but that should never
	 * happen because send_buffer and receive_buffer have the same size */
	assert(buffer_capacity(buf) >= msglen + 4);
	r = read(fd, buffer_current(buf), msglen - (buffer_position(buf) - 4));
	if(r == -1) {
		if(errno == EAGAIN || errno == EINTR) {
			/* continue to read later */
			return 0;
		}
		log_msg(LOG_ERR, "dnstap collector: read failed: %s",
			strerror(errno));
		return 0;
	}
	buffer_skip(buf, r);
	if(buffer_position(buf) < 4 + msglen)
		return 0; /* read more msg later */

	/* msg complete */
	buffer_flip(buf);
	return 1;
}

/* submit the content of the buffer received to dnstap */
static void
dt_submit_content(struct dt_env* dt_env, struct buffer* buf)
{
	uint8_t is_response, is_tcp;
#ifdef INET6
	struct sockaddr_storage addr;
#else
	struct sockaddr_in addr;
#endif
	socklen_t addrlen;
	size_t pktlen;
	uint8_t* data;
	size_t zonelen;
	uint8_t* zone;

	/* parse content from buffer */
	if(!buffer_available(buf, 4+1+4)) return;
	buffer_skip(buf, 4); /* skip msglen */
	is_response = buffer_read_u8(buf);
	addrlen = buffer_read_u32(buf);
	if(addrlen > sizeof(addr)) return;
	if(!buffer_available(buf, addrlen)) return;
	buffer_read(buf, &addr, addrlen);
	if(!buffer_available(buf, 1+4)) return;
	is_tcp = buffer_read_u8(buf);
	pktlen = buffer_read_u32(buf);
	if(!buffer_available(buf, pktlen)) return;
	data = buffer_current(buf);
	buffer_skip(buf, pktlen);
	if(!buffer_available(buf, 4)) return;
	zonelen = buffer_read_u32(buf);
	if(zonelen == 0) {
		zone = NULL;
	} else {
		if(zonelen > MAXDOMAINLEN) return;
		if(!buffer_available(buf, zonelen)) return;
		zone = buffer_current(buf);
		buffer_skip(buf, zonelen);
	}

	/* submit it */
	if(is_response) {
		dt_msg_send_auth_response(dt_env, &addr, is_tcp, zone,
			zonelen, data, pktlen);
	} else {
		dt_msg_send_auth_query(dt_env, &addr, is_tcp, zone,
			zonelen, data, pktlen);
	}
}

/* handle input from worker for dnstap */
void
dt_handle_input(int fd, short event, void* arg)
{
	struct dt_collector_input* dt_input = (struct dt_collector_input*)arg;
	if((event&EV_READ) != 0) {
		/* read */
		if(!read_into_buffer(fd, dt_input->buffer))
			return;

		/* once data is complete, write it to dnstap */
		VERBOSITY(4, (LOG_INFO, "dnstap collector: received msg len %d",
			(int)buffer_remaining(dt_input->buffer)));
		if(dt_input->dt_collector->dt_env) {
			dt_submit_content(dt_input->dt_collector->dt_env,
				dt_input->buffer);
		}
		
		/* clear buffer for next message */
		buffer_clear(dt_input->buffer);
	}
}

/* init dnstap */
static void dt_init_dnstap(struct dt_collector* dt_col, struct nsd* nsd)
{
	int num_workers = 1;
#ifdef HAVE_CHROOT
	if(nsd->chrootdir && nsd->chrootdir[0]) {
		int l = strlen(nsd->chrootdir)-1; /* ends in trailing slash */
		if (nsd->options->dnstap_socket_path &&
			nsd->options->dnstap_socket_path[0] == '/' &&
			strncmp(nsd->options->dnstap_socket_path,
				nsd->chrootdir, l) == 0)
			nsd->options->dnstap_socket_path += l;
	}
#endif
	dt_col->dt_env = dt_create(nsd->options->dnstap_socket_path, num_workers);
	if(!dt_col->dt_env) {
		log_msg(LOG_ERR, "could not create dnstap env");
		return;
	}
	dt_apply_cfg(dt_col->dt_env, nsd->options);
	dt_init(dt_col->dt_env);
}

/* cleanup dt collector process for exit */
static void dt_collector_cleanup(struct dt_collector* dt_col, struct nsd* nsd)
{
	int i;
	dt_delete(dt_col->dt_env);
	event_del(dt_col->cmd_event);
	for(i=0; i<dt_col->count; i++) {
		event_del(dt_col->inputs[i].event);
	}
	dt_collector_close(dt_col, nsd);
	event_base_free(dt_col->event_base);
#ifdef MEMCLEAN
	free(dt_col->cmd_event);
	if(dt_col->inputs) {
		for(i=0; i<dt_col->count; i++) {
			free(dt_col->inputs[i].event);
		}
		free(dt_col->inputs);
	}
	dt_collector_destroy(dt_col, nsd);
#endif
}

/* attach events to the event base to listen to the workers and cmd channel */
static void dt_attach_events(struct dt_collector* dt_col, struct nsd* nsd)
{
	int i;
	/* create event base */
	dt_col->event_base = nsd_child_event_base();
	if(!dt_col->event_base) {
		error("dnstap collector: event_base create failed");
	}

	/* add command handler */
	dt_col->cmd_event = (struct event*)xalloc_zero(
		sizeof(*dt_col->cmd_event));
	event_set(dt_col->cmd_event, dt_col->cmd_socket_dt,
		EV_PERSIST|EV_READ, dt_handle_cmd_from_nsd, dt_col);
	if(event_base_set(dt_col->event_base, dt_col->cmd_event) != 0)
		log_msg(LOG_ERR, "dnstap collector: event_base_set failed");
	if(event_add(dt_col->cmd_event, NULL) != 0)
		log_msg(LOG_ERR, "dnstap collector: event_add failed");
	
	/* add worker input handlers */
	dt_col->inputs = xalloc_array_zero(dt_col->count,
		sizeof(*dt_col->inputs));
	for(i=0; i<dt_col->count; i++) {
		dt_col->inputs[i].dt_collector = dt_col;
		dt_col->inputs[i].event = (struct event*)xalloc_zero(
			sizeof(struct event));
		event_set(dt_col->inputs[i].event,
			nsd->dt_collector_fd_recv[i], EV_PERSIST|EV_READ,
			dt_handle_input, &dt_col->inputs[i]);
		if(event_base_set(dt_col->event_base,
			dt_col->inputs[i].event) != 0)
			log_msg(LOG_ERR, "dnstap collector: event_base_set failed");
		if(event_add(dt_col->inputs[i].event, NULL) != 0)
			log_msg(LOG_ERR, "dnstap collector: event_add failed");
		
		dt_col->inputs[i].buffer = buffer_create(dt_col->region,
			/* msglen + is_response + addrlen + is_tcp + packetlen + packet + zonelen + zone + spare + addr */
			4+1+4+1+4+TCP_MAX_MESSAGE_LEN+4+MAXHOSTNAMELEN + 32 +
#ifdef INET6
			sizeof(struct sockaddr_storage)
#else
			sizeof(struct sockaddr_in)
#endif
		);
		assert(buffer_capacity(dt_col->inputs[i].buffer) ==
			buffer_capacity(dt_col->send_buffer));
	}
}

/* the dnstap collector process main routine */
static void dt_collector_run(struct dt_collector* dt_col, struct nsd* nsd)
{
	/* init dnstap */
	VERBOSITY(1, (LOG_INFO, "dnstap collector started"));
	dt_init_dnstap(dt_col, nsd);
	dt_attach_events(dt_col, nsd);

	/* run */
	if(event_base_loop(dt_col->event_base, 0) == -1) {
		error("dnstap collector: event_base_loop failed");
	}

	/* cleanup and done */
	VERBOSITY(1, (LOG_INFO, "dnstap collector stopped"));
	dt_collector_cleanup(dt_col, nsd);
	exit(0);
}

void dt_collector_start(struct dt_collector* dt_col, struct nsd* nsd)
{
	/* fork */
	dt_col->dt_pid = fork();
	if(dt_col->dt_pid == -1) {
		error("dnstap_collector: fork failed: %s", strerror(errno));
	}
	if(dt_col->dt_pid == 0) {
		/* the dt collector process is this */
		/* close the nsd side of the command channel */
		close(dt_col->cmd_socket_nsd);
		dt_col->cmd_socket_nsd = -1;
		dt_collector_run(dt_col, nsd);
		/* NOTREACH */
		exit(0);
	} else {
		/* the parent continues on, with starting NSD */
		/* close the dt side of the command channel */
		close(dt_col->cmd_socket_dt);
		dt_col->cmd_socket_dt = -1;
	}
}

/* put data for sending to the collector process into the buffer */
static int
prep_send_data(struct buffer* buf, uint8_t is_response,
#ifdef INET6
	struct sockaddr_storage* addr,
#else
	struct sockaddr_in* addr,
#endif
	socklen_t addrlen, int is_tcp, struct buffer* packet,
	struct zone* zone)
{
	buffer_clear(buf);
	if(!buffer_available(buf, 4+1+4+addrlen+1+4+buffer_remaining(packet)))
		return 0; /* does not fit in send_buffer, log is dropped */
	buffer_skip(buf, 4); /* the length of the message goes here */
	buffer_write_u8(buf, is_response);
	buffer_write_u32(buf, addrlen);
	buffer_write(buf, addr, (size_t)addrlen);
	buffer_write_u8(buf, (is_tcp?1:0));
	buffer_write_u32(buf, buffer_remaining(packet));
	buffer_write(buf, buffer_begin(packet), buffer_remaining(packet));
	if(zone && zone->apex && domain_dname(zone->apex)) {
		if(!buffer_available(buf, 4 + domain_dname(zone->apex)->name_size))
			return 0;
		buffer_write_u32(buf, domain_dname(zone->apex)->name_size);
		buffer_write(buf, dname_name(domain_dname(zone->apex)),
			domain_dname(zone->apex)->name_size);
	} else {
		if(!buffer_available(buf, 4))
			return 0;
		buffer_write_u32(buf, 0);
	}

	buffer_flip(buf);
	/* write length of message */
	buffer_write_u32_at(buf, 0, buffer_remaining(buf)-4);
	return 1;
}

/* attempt to write buffer to socket, if it blocks do not write it. */
static void attempt_to_write(int s, uint8_t* data, size_t len)
{
	size_t total = 0;
	ssize_t r;
	while(total < len) {
		r = write(s, data+total, len-total);
		if(r == -1) {
			if(errno == EAGAIN && total == 0) {
				/* on first write part, check if pipe is full,
				 * if the nonblocking fd blocks, then drop
				 * the message */
				return;
			}
			if(errno != EAGAIN && errno != EINTR) {
				/* some sort of error, print it and drop it */
				log_msg(LOG_ERR,
					"dnstap collector: write failed: %s",
					strerror(errno));
				return;
			}
			/* continue and write this again */
			/* for EINTR, we have to do this,
			 * for EAGAIN, if the first part succeeded, we have
			 * to continue to write the remainder of the message,
			 * because otherwise partial messages confuse the
			 * receiver. */
			continue;
		}
		total += r;
	}
}

void dt_collector_submit_auth_query(struct nsd* nsd,
#ifdef INET6
	struct sockaddr_storage* addr,
#else
	struct sockaddr_in* addr,
#endif
	socklen_t addrlen, int is_tcp, struct buffer* packet)
{
	if(!nsd->dt_collector) return;
	if(!nsd->options->dnstap_log_auth_query_messages) return;
	VERBOSITY(4, (LOG_INFO, "dnstap submit auth query"));

	/* marshal data into send buffer */
	if(!prep_send_data(nsd->dt_collector->send_buffer, 0, addr, addrlen,
		is_tcp, packet, NULL))
		return; /* probably did not fit in buffer */

	/* attempt to send data; do not block */
	attempt_to_write(nsd->dt_collector_fd_send[nsd->this_child->child_num],
		buffer_begin(nsd->dt_collector->send_buffer),
		buffer_remaining(nsd->dt_collector->send_buffer));
}

void dt_collector_submit_auth_response(struct nsd* nsd,
#ifdef INET6
	struct sockaddr_storage* addr,
#else
	struct sockaddr_in* addr,
#endif
	socklen_t addrlen, int is_tcp, struct buffer* packet,
	struct zone* zone)
{
	if(!nsd->dt_collector) return;
	if(!nsd->options->dnstap_log_auth_response_messages) return;
	VERBOSITY(4, (LOG_INFO, "dnstap submit auth response"));

	/* marshal data into send buffer */
	if(!prep_send_data(nsd->dt_collector->send_buffer, 1, addr, addrlen,
		is_tcp, packet, zone))
		return; /* probably did not fit in buffer */

	/* attempt to send data; do not block */
	attempt_to_write(nsd->dt_collector_fd_send[nsd->this_child->child_num],
		buffer_begin(nsd->dt_collector->send_buffer),
		buffer_remaining(nsd->dt_collector->send_buffer));
}
