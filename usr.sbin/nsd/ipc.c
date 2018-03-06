/*
 * ipc.c - Interprocess communication routines. Handlers read and write.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "ipc.h"
#include "buffer.h"
#include "xfrd-tcp.h"
#include "nsd.h"
#include "namedb.h"
#include "xfrd.h"
#include "xfrd-notify.h"
#include "difffile.h"
#include "rrl.h"

/* attempt to send NSD_STATS command to child fd */
static void send_stat_to_child(struct main_ipc_handler_data* data, int fd);
/* send reload request over the IPC channel */
static void xfrd_send_reload_req(xfrd_state_type* xfrd);
/* send quit request over the IPC channel */
static void xfrd_send_quit_req(xfrd_state_type* xfrd);
/* perform read part of handle ipc for xfrd */
static void xfrd_handle_ipc_read(struct event* handler, xfrd_state_type* xfrd);

static void
ipc_child_quit(struct nsd* nsd)
{
	/* call shutdown and quit routines */
	nsd->mode = NSD_QUIT;
#ifdef	BIND8_STATS
	bind8_stats(nsd);
#endif /* BIND8_STATS */

#ifdef MEMCLEAN /* OS collects memory pages */
#ifdef RATELIMIT
        rrl_deinit(nsd->this_child->child_num);
#endif
	event_base_free(nsd->event_base);
	region_destroy(nsd->server_region);
#endif
	server_shutdown(nsd);
	exit(0);
}

void
child_handle_parent_command(int fd, short event, void* arg)
{
	sig_atomic_t mode;
	int len;
	struct ipc_handler_conn_data *data =
		(struct ipc_handler_conn_data *) arg;
	if (!(event & EV_READ)) {
		return;
	}

	if ((len = read(fd, &mode, sizeof(mode))) == -1) {
		log_msg(LOG_ERR, "handle_parent_command: read: %s",
			strerror(errno));
		return;
	}
	if (len == 0)
	{
		/* parent closed the connection. Quit */
		ipc_child_quit(data->nsd);
		return;
	}

	switch (mode) {
	case NSD_STATS:
		data->nsd->mode = mode;
		break;
	case NSD_QUIT:
		ipc_child_quit(data->nsd);
		break;
	case NSD_QUIT_CHILD:
		/* close our listening sockets and ack */
		server_close_all_sockets(data->nsd->udp, data->nsd->ifs);
		server_close_all_sockets(data->nsd->tcp, data->nsd->ifs);
		/* mode == NSD_QUIT_CHILD */
		if(write(fd, &mode, sizeof(mode)) == -1) {
			VERBOSITY(3, (LOG_INFO, "quit child write: %s",
				strerror(errno)));
		}
		ipc_child_quit(data->nsd);
		break;
	case NSD_QUIT_WITH_STATS:
#ifdef BIND8_STATS
		DEBUG(DEBUG_IPC, 2, (LOG_INFO, "quit QUIT_WITH_STATS"));
		/* reply with ack and stats and then quit */
		if(!write_socket(fd, &mode, sizeof(mode))) {
			log_msg(LOG_ERR, "cannot write quitwst to parent");
		}
		if(!write_socket(fd, &data->nsd->st, sizeof(data->nsd->st))) {
			log_msg(LOG_ERR, "cannot write stats to parent");
		}
		fsync(fd);
#endif /* BIND8_STATS */
		ipc_child_quit(data->nsd);
		break;
	default:
		log_msg(LOG_ERR, "handle_parent_command: bad mode %d",
			(int) mode);
		break;
	}
}

void
parent_handle_xfrd_command(netio_type *ATTR_UNUSED(netio),
		      netio_handler_type *handler,
		      netio_event_types_type event_types)
{
	sig_atomic_t mode;
	int len;
	struct ipc_handler_conn_data *data =
		(struct ipc_handler_conn_data *) handler->user_data;
	if (!(event_types & NETIO_EVENT_READ)) {
		return;
	}

	if ((len = read(handler->fd, &mode, sizeof(mode))) == -1) {
		log_msg(LOG_ERR, "handle_xfrd_command: read: %s",
			strerror(errno));
		return;
	}
	if (len == 0)
	{
		/* xfrd closed, we must quit */
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "handle_xfrd_command: xfrd closed channel."));
		close(handler->fd);
		handler->fd = -1;
		data->nsd->mode = NSD_SHUTDOWN;
		return;
	}

	switch (mode) {
	case NSD_RELOAD:
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "parent handle xfrd command RELOAD"));
		data->nsd->signal_hint_reload = 1;
		break;
	case NSD_QUIT:
	case NSD_SHUTDOWN:
		data->nsd->mode = mode;
		break;
	case NSD_STATS:
		data->nsd->signal_hint_stats = 1;
		break;
	case NSD_REAP_CHILDREN:
		data->nsd->signal_hint_child = 1;
		break;
	default:
		log_msg(LOG_ERR, "handle_xfrd_command: bad mode %d",
			(int) mode);
		break;
	}
}

static void
send_stat_to_child(struct main_ipc_handler_data* data, int fd)
{
	sig_atomic_t cmd = NSD_STATS;
	if(write(fd, &cmd, sizeof(cmd)) == -1) {
		if(errno == EAGAIN || errno == EINTR)
			return; /* try again later */
		log_msg(LOG_ERR, "svrmain: problems sending stats to child %d command: %s",
			(int)data->child->pid, strerror(errno));
		return;
	}
	data->child->need_to_send_STATS = 0;
}

#ifndef NDEBUG
int packet_read_query_section(buffer_type *packet, uint8_t* dest, uint16_t* qtype, uint16_t* qclass);
static void
debug_print_fwd_name(int ATTR_UNUSED(len), buffer_type* packet, int acl_num)
{
	uint8_t qnamebuf[MAXDOMAINLEN];
	uint16_t qtype, qclass;
	const dname_type* dname;
	region_type* tempregion = region_create(xalloc, free);

	size_t bufpos = buffer_position(packet);
	buffer_rewind(packet);
	buffer_skip(packet, 12);
	if(packet_read_query_section(packet, qnamebuf, &qtype, &qclass)) {
		dname = dname_make(tempregion, qnamebuf, 1);
		log_msg(LOG_INFO, "main: fwd packet for %s, acl %d",
			dname_to_string(dname,0), acl_num);
	} else {
		log_msg(LOG_INFO, "main: fwd packet badqname, acl %d", acl_num);
	}
	buffer_set_position(packet, bufpos);
	region_destroy(tempregion);
}
#endif

static void
send_quit_to_child(struct main_ipc_handler_data* data, int fd)
{
#ifdef BIND8_STATS
	sig_atomic_t cmd = NSD_QUIT_WITH_STATS;
#else
	sig_atomic_t cmd = NSD_QUIT;
#endif
	if(write(fd, &cmd, sizeof(cmd)) == -1) {
		if(errno == EAGAIN || errno == EINTR)
			return; /* try again later */
		log_msg(LOG_ERR, "svrmain: problems sending quit to child %d command: %s",
			(int)data->child->pid, strerror(errno));
		return;
	}
	data->child->need_to_send_QUIT = 0;
	DEBUG(DEBUG_IPC,2, (LOG_INFO, "main: sent quit to child %d",
		(int)data->child->pid));
}

/** the child is done, mark it as exited */
static void
child_is_done(struct nsd* nsd, int fd)
{
	size_t i;
	if(fd != -1) close(fd);
	for(i=0; i<nsd->child_count; ++i)
		if(nsd->children[i].child_fd == fd) {
			nsd->children[i].child_fd = -1;
			nsd->children[i].handler->fd = -1;
			if(nsd->children[i].need_to_exit) {
				DEBUG(DEBUG_IPC,1, (LOG_INFO, "server %d is done",
					(int)nsd->children[i].pid));
				nsd->children[i].has_exited = 1;
			} else {
				log_msg(LOG_WARNING,
				       "server %d died unexpectedly, restarting",
				       (int)nsd->children[i].pid);
				/* this child is now going to be re-forked as
				 * a subprocess of this server-main, and if a
				 * reload is in progress the other children
				 * are subprocesses of reload.  Until the
				 * reload is done and they are all reforked. */
				nsd->children[i].pid = -1;
				nsd->restart_children = 1;
			}
		}
	parent_check_all_children_exited(nsd);
}

#ifdef BIND8_STATS
/** add stats to total */
void
stats_add(struct nsdst* total, struct nsdst* s)
{
	unsigned i;
	for(i=0; i<sizeof(total->qtype)/sizeof(stc_type); i++)
		total->qtype[i] += s->qtype[i];
	for(i=0; i<sizeof(total->qclass)/sizeof(stc_type); i++)
		total->qclass[i] += s->qclass[i];
	total->qudp += s->qudp;
	total->qudp6 += s->qudp6;
	total->ctcp += s->ctcp;
	total->ctcp6 += s->ctcp6;
	for(i=0; i<sizeof(total->rcode)/sizeof(stc_type); i++)
		total->rcode[i] += s->rcode[i];
	for(i=0; i<sizeof(total->opcode)/sizeof(stc_type); i++)
		total->opcode[i] += s->opcode[i];
	total->dropped += s->dropped;
	total->truncated += s->truncated;
	total->wrongzone += s->wrongzone;
	total->txerr += s->txerr;
	total->rxerr += s->rxerr;
	total->edns += s->edns;
	total->ednserr += s->ednserr;
	total->raxfr += s->raxfr;
	total->nona += s->nona;

	total->db_disk = s->db_disk;
	total->db_mem = s->db_mem;
}

/** subtract stats from total */
void
stats_subtract(struct nsdst* total, struct nsdst* s)
{
	unsigned i;
	for(i=0; i<sizeof(total->qtype)/sizeof(stc_type); i++)
		total->qtype[i] -= s->qtype[i];
	for(i=0; i<sizeof(total->qclass)/sizeof(stc_type); i++)
		total->qclass[i] -= s->qclass[i];
	total->qudp -= s->qudp;
	total->qudp6 -= s->qudp6;
	total->ctcp -= s->ctcp;
	total->ctcp6 -= s->ctcp6;
	for(i=0; i<sizeof(total->rcode)/sizeof(stc_type); i++)
		total->rcode[i] -= s->rcode[i];
	for(i=0; i<sizeof(total->opcode)/sizeof(stc_type); i++)
		total->opcode[i] -= s->opcode[i];
	total->dropped -= s->dropped;
	total->truncated -= s->truncated;
	total->wrongzone -= s->wrongzone;
	total->txerr -= s->txerr;
	total->rxerr -= s->rxerr;
	total->edns -= s->edns;
	total->ednserr -= s->ednserr;
	total->raxfr -= s->raxfr;
	total->nona -= s->nona;
}

#define FINAL_STATS_TIMEOUT 10 /* seconds */
static void
read_child_stats(struct nsd* nsd, struct nsd_child* child, int fd)
{
	struct nsdst s;
	errno=0;
	if(block_read(nsd, fd, &s, sizeof(s), FINAL_STATS_TIMEOUT)!=sizeof(s)) {
		log_msg(LOG_ERR, "problems reading finalstats from server "
			"%d: %s", (int)child->pid, strerror(errno));
	} else {
		stats_add(&nsd->st, &s);
		child->query_count = s.qudp + s.qudp6 + s.ctcp + s.ctcp6;
		/* we know that the child is going to close the connection
		 * now (this is an ACK of the QUIT_W_STATS so we know the
		 * child is done, no longer sending e.g. NOTIFY contents) */
		child_is_done(nsd, fd);
	}
}
#endif /* BIND8_STATS */

void
parent_handle_child_command(netio_type *ATTR_UNUSED(netio),
		      netio_handler_type *handler,
		      netio_event_types_type event_types)
{
	sig_atomic_t mode;
	int len;
	struct main_ipc_handler_data *data =
		(struct main_ipc_handler_data*)handler->user_data;

	/* do a nonblocking write to the child if it is ready. */
	if (event_types & NETIO_EVENT_WRITE) {
		if(data->child->need_to_send_STATS &&
			!data->child->need_to_exit) {
			send_stat_to_child(data, handler->fd);
		} else if(data->child->need_to_send_QUIT) {
			send_quit_to_child(data, handler->fd);
			if(!data->child->need_to_send_QUIT)
				handler->event_types = NETIO_EVENT_READ;
		} else {
			handler->event_types = NETIO_EVENT_READ;
		}
	}

	if (!(event_types & NETIO_EVENT_READ)) {
		return;
	}

	if (data->forward_mode) {
		int got_acl;
		/* forward the data to xfrd */
		DEBUG(DEBUG_IPC,2, (LOG_INFO,
			"main passed packet readup %d", (int)data->got_bytes));
		if(data->got_bytes < sizeof(data->total_bytes))
		{
			if ((len = read(handler->fd,
				(char*)&data->total_bytes+data->got_bytes,
				sizeof(data->total_bytes)-data->got_bytes)) == -1) {
				log_msg(LOG_ERR, "handle_child_command: read: %s",
					strerror(errno));
				return;
			}
			if(len == 0) {
				/* EOF */
				data->forward_mode = 0;
				return;
			}
			data->got_bytes += len;
			if(data->got_bytes < sizeof(data->total_bytes))
				return;
			data->total_bytes = ntohs(data->total_bytes);
			buffer_clear(data->packet);
			if(data->total_bytes > buffer_capacity(data->packet)) {
				log_msg(LOG_ERR, "internal error: ipc too large");
				exit(1);
			}
			return;
		}
		/* read the packet */
		if(data->got_bytes-sizeof(data->total_bytes) < data->total_bytes) {
			if((len = read(handler->fd, buffer_current(data->packet),
				data->total_bytes - (data->got_bytes-sizeof(data->total_bytes))
				)) == -1 ) {
				log_msg(LOG_ERR, "handle_child_command: read: %s",
					strerror(errno));
				return;
			}
			if(len == 0) {
				/* EOF */
				data->forward_mode = 0;
				return;
			}
			data->got_bytes += len;
			buffer_skip(data->packet, len);
			/* read rest later */
			return;
		}
		/* read the acl numbers */
		got_acl = data->got_bytes - sizeof(data->total_bytes) - data->total_bytes;
		if((len = read(handler->fd, (char*)&data->acl_num+got_acl,
			sizeof(data->acl_num)+sizeof(data->acl_xfr)-got_acl)) == -1 ) {
			log_msg(LOG_ERR, "handle_child_command: read: %s",
				strerror(errno));
			return;
		}
		if(len == 0) {
			/* EOF */
			data->forward_mode = 0;
			return;
		}
		got_acl += len;
		data->got_bytes += len;
		if(got_acl >= (int)(sizeof(data->acl_num)+sizeof(data->acl_xfr))) {
			uint16_t len = htons(data->total_bytes);
			DEBUG(DEBUG_IPC,2, (LOG_INFO,
				"main fwd passed packet write %d", (int)data->got_bytes));
#ifndef NDEBUG
			if(nsd_debug_level >= 2)
				debug_print_fwd_name(len, data->packet, data->acl_num);
#endif
			data->forward_mode = 0;
			mode = NSD_PASS_TO_XFRD;
			if(!write_socket(*data->xfrd_sock, &mode, sizeof(mode)) ||
			   !write_socket(*data->xfrd_sock, &len, sizeof(len)) ||
			   !write_socket(*data->xfrd_sock, buffer_begin(data->packet),
				data->total_bytes) ||
			   !write_socket(*data->xfrd_sock, &data->acl_num,
			   	sizeof(data->acl_num)) ||
			   !write_socket(*data->xfrd_sock, &data->acl_xfr,
			   	sizeof(data->acl_xfr))) {
				log_msg(LOG_ERR, "error in ipc fwd main2xfrd: %s",
					strerror(errno));
			}
		}
		return;
	}

	/* read command from ipc */
	if ((len = read(handler->fd, &mode, sizeof(mode))) == -1) {
		log_msg(LOG_ERR, "handle_child_command: read: %s",
			strerror(errno));
		return;
	}
	if (len == 0)
	{
		child_is_done(data->nsd, handler->fd);
		return;
	}

	switch (mode) {
	case NSD_QUIT:
		data->nsd->mode = mode;
		break;
#ifdef BIND8_STATS
	case NSD_QUIT_WITH_STATS:
		read_child_stats(data->nsd, data->child, handler->fd);
		break;
#endif /* BIND8_STATS */
	case NSD_STATS:
		data->nsd->signal_hint_stats = 1;
		break;
	case NSD_REAP_CHILDREN:
		data->nsd->signal_hint_child = 1;
		break;
	case NSD_PASS_TO_XFRD:
		/* set mode for handle_child_command; echo to xfrd. */
		data->forward_mode = 1;
		data->got_bytes = 0;
		data->total_bytes = 0;
		break;
	default:
		log_msg(LOG_ERR, "handle_child_command: bad mode %d",
			(int) mode);
		break;
	}
}

void
parent_check_all_children_exited(struct nsd* nsd)
{
	size_t i;
	for(i=0; i < nsd->child_count; i++) {
		if(!nsd->children[i].need_to_exit)
		      return;
		if(!nsd->children[i].has_exited)
		      return;
	}
	nsd->mode = NSD_QUIT_SYNC;
	DEBUG(DEBUG_IPC,2, (LOG_INFO, "main: all children exited. quit sync."));
}

void
parent_handle_reload_command(netio_type *ATTR_UNUSED(netio),
		      netio_handler_type *handler,
		      netio_event_types_type event_types)
{
	sig_atomic_t mode;
	int len;
	size_t i;
	struct nsd *nsd = (struct nsd*) handler->user_data;
	if (!(event_types & NETIO_EVENT_READ)) {
		return;
	}
	/* read command from ipc */
	if ((len = read(handler->fd, &mode, sizeof(mode))) == -1) {
		log_msg(LOG_ERR, "handle_reload_command: read: %s",
			strerror(errno));
		return;
	}
	if (len == 0)
	{
		if(handler->fd != -1) {
			close(handler->fd);
			handler->fd = -1;
		}
		log_msg(LOG_ERR, "handle_reload_cmd: reload closed cmd channel");
		nsd->reload_failed = 1;
		return;
	}
	switch (mode) {
	case NSD_QUIT_SYNC:
		/* set all children to exit, only then notify xfrd. */
		/* so that buffered packets to pass to xfrd can arrive. */
		for(i=0; i < nsd->child_count; i++) {
			nsd->children[i].need_to_exit = 1;
			if(nsd->children[i].pid > 0 &&
			   nsd->children[i].child_fd != -1) {
				nsd->children[i].need_to_send_QUIT = 1;
				nsd->children[i].handler->event_types
					|= NETIO_EVENT_WRITE;
			} else {
				if(nsd->children[i].child_fd == -1)
					nsd->children[i].has_exited = 1;
			}
		}
		parent_check_all_children_exited(nsd);
		break;
	default:
		log_msg(LOG_ERR, "handle_reload_command: bad mode %d",
			(int) mode);
		break;
	}
}

static void
xfrd_send_reload_req(xfrd_state_type* xfrd)
{
	sig_atomic_t req = NSD_RELOAD;
	uint64_t p = xfrd->last_task->data;
	udb_ptr_unlink(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask]);
	task_process_sync(xfrd->nsd->task[xfrd->nsd->mytask]);
	/* ask server_main for a reload */
	if(write(xfrd->ipc_handler.ev_fd, &req, sizeof(req)) == -1) {
		udb_ptr_init(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask]);
		udb_ptr_set(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask], p);
		if(errno == EAGAIN || errno == EINTR)
			return; /* try again later */
		log_msg(LOG_ERR, "xfrd: problems sending reload command: %s",
			strerror(errno));
		return;
	}
	DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: asked nsd to reload new updates"));
	/* swapped task to other side, start to use other task udb. */
	xfrd->nsd->mytask = 1 - xfrd->nsd->mytask;
	task_remap(xfrd->nsd->task[xfrd->nsd->mytask]);
	udb_ptr_init(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask]);
	assert(udb_base_get_userdata(xfrd->nsd->task[xfrd->nsd->mytask])->data == 0);

	xfrd_prepare_zones_for_reload();
	xfrd->reload_cmd_last_sent = xfrd_time();
	xfrd->need_to_send_reload = 0;
	xfrd->can_send_reload = 0;
}

void
ipc_xfrd_set_listening(struct xfrd_state* xfrd, short mode)
{
	int fd = xfrd->ipc_handler.ev_fd;
	struct event_base* base = xfrd->event_base;
	event_del(&xfrd->ipc_handler);
	event_set(&xfrd->ipc_handler, fd, mode, xfrd_handle_ipc, xfrd);
	if(event_base_set(base, &xfrd->ipc_handler) != 0)
		log_msg(LOG_ERR, "ipc: cannot set event_base");
	/* no timeout for IPC events */
	if(event_add(&xfrd->ipc_handler, NULL) != 0)
		log_msg(LOG_ERR, "ipc: cannot add event");
	xfrd->ipc_handler_flags = mode;
}

static void
xfrd_send_shutdown_req(xfrd_state_type* xfrd)
{
	sig_atomic_t cmd = NSD_SHUTDOWN;
	xfrd->ipc_send_blocked = 1;
	ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ);
	DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc send shutdown"));
	if(!write_socket(xfrd->ipc_handler.ev_fd, &cmd, sizeof(cmd))) {
		log_msg(LOG_ERR, "xfrd: error writing shutdown to main: %s",
			strerror(errno));
	}
	xfrd->need_to_send_shutdown = 0;
}

static void
xfrd_send_quit_req(xfrd_state_type* xfrd)
{
	sig_atomic_t cmd = NSD_QUIT;
	xfrd->ipc_send_blocked = 1;
	ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ);
	DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc send ackreload(quit)"));
	if(!write_socket(xfrd->ipc_handler.ev_fd, &cmd, sizeof(cmd))) {
		log_msg(LOG_ERR, "xfrd: error writing ack to main: %s",
			strerror(errno));
	}
	xfrd->need_to_send_quit = 0;
}

static void
xfrd_send_stats(xfrd_state_type* xfrd)
{
	sig_atomic_t cmd = NSD_STATS;
	DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc send stats"));
	if(!write_socket(xfrd->ipc_handler.ev_fd, &cmd, sizeof(cmd))) {
		log_msg(LOG_ERR, "xfrd: error writing stats to main: %s",
			strerror(errno));
	}
	xfrd->need_to_send_stats = 0;
}

void
xfrd_handle_ipc(int ATTR_UNUSED(fd), short event, void* arg)
{
	xfrd_state_type* xfrd = (xfrd_state_type*)arg;
        if ((event & EV_READ))
	{
		/* first attempt to read as a signal from main
		 * could block further send operations */
		xfrd_handle_ipc_read(&xfrd->ipc_handler, xfrd);
	}
        if ((event & EV_WRITE))
	{
		if(xfrd->ipc_send_blocked) { /* wait for RELOAD_DONE */
			ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ);
			return;
		}
		if(xfrd->need_to_send_shutdown) {
			xfrd_send_shutdown_req(xfrd);
		} else if(xfrd->need_to_send_quit) {
			xfrd_send_quit_req(xfrd);
		} else if(xfrd->can_send_reload && xfrd->need_to_send_reload) {
			xfrd_send_reload_req(xfrd);
		} else if(xfrd->need_to_send_stats) {
			xfrd_send_stats(xfrd);
		}
		if(!(xfrd->can_send_reload && xfrd->need_to_send_reload) &&
			!xfrd->need_to_send_shutdown &&
			!xfrd->need_to_send_quit &&
			!xfrd->need_to_send_stats) {
			/* disable writing for now */
			ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ);
		}
	}

}

static void
xfrd_handle_ipc_read(struct event* handler, xfrd_state_type* xfrd)
{
        sig_atomic_t cmd;
        int len;

	if(xfrd->ipc_conn->is_reading==2) {
		buffer_type* tmp = xfrd->ipc_pass;
		uint32_t acl_num;
		int32_t acl_xfr;
		/* read acl_num */
		int ret = conn_read(xfrd->ipc_conn);
		if(ret == -1) {
			log_msg(LOG_ERR, "xfrd: error in read ipc: %s", strerror(errno));
			xfrd->ipc_conn->is_reading = 0;
			return;
		}
		if(ret == 0)
			return;
		buffer_flip(xfrd->ipc_conn->packet);
		xfrd->ipc_pass = xfrd->ipc_conn->packet;
		xfrd->ipc_conn->packet = tmp;
		xfrd->ipc_conn->is_reading = 0;
		acl_num = buffer_read_u32(xfrd->ipc_pass);
		acl_xfr = (int32_t)buffer_read_u32(xfrd->ipc_pass);
		xfrd_handle_passed_packet(xfrd->ipc_conn->packet, acl_num, acl_xfr);
		return;
	}
	if(xfrd->ipc_conn->is_reading) {
		/* reading an IPC message */
		buffer_type* tmp;
		int ret = conn_read(xfrd->ipc_conn);
		if(ret == -1) {
			log_msg(LOG_ERR, "xfrd: error in read ipc: %s", strerror(errno));
			xfrd->ipc_conn->is_reading = 0;
			return;
		}
		if(ret == 0)
			return;
		buffer_flip(xfrd->ipc_conn->packet);
		/* use ipc_conn to read remaining data as well */
		tmp = xfrd->ipc_pass;
		xfrd->ipc_conn->is_reading=2;
		xfrd->ipc_pass = xfrd->ipc_conn->packet;
		xfrd->ipc_conn->packet = tmp;
		xfrd->ipc_conn->total_bytes = sizeof(xfrd->ipc_conn->msglen);
		xfrd->ipc_conn->msglen = 2*sizeof(uint32_t);
		buffer_clear(xfrd->ipc_conn->packet);
		buffer_set_limit(xfrd->ipc_conn->packet, xfrd->ipc_conn->msglen);
		return;
	}

        if((len = read(handler->ev_fd, &cmd, sizeof(cmd))) == -1) {
		if(errno != EINTR && errno != EAGAIN)
                	log_msg(LOG_ERR, "xfrd_handle_ipc: read: %s",
                        	strerror(errno));
                return;
        }
        if(len == 0)
        {
		/* parent closed the connection. Quit */
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: main closed connection."));
		xfrd->shutdown = 1;
		return;
        }

        switch(cmd) {
        case NSD_QUIT:
        case NSD_SHUTDOWN:
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: main sent shutdown cmd."));
                xfrd->shutdown = 1;
                break;
	case NSD_RELOAD_DONE:
		/* reload has finished */
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc recv RELOAD_DONE"));
		if(block_read(NULL, handler->ev_fd, &xfrd->reload_pid,
			sizeof(pid_t), -1) != sizeof(pid_t)) {
			log_msg(LOG_ERR, "xfrd cannot get reload_pid");
		}
		/* read the not-mytask for the results and soainfo */
		xfrd_process_task_result(xfrd,
			xfrd->nsd->task[1-xfrd->nsd->mytask]);
		/* reset the IPC, (and the nonblocking ipc write;
		   the new parent does not want half a packet) */
		xfrd->can_send_reload = 1;
		xfrd->ipc_send_blocked = 0;
		ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
		xfrd_reopen_logfile();
		xfrd_check_failed_updates();
		break;
	case NSD_PASS_TO_XFRD:
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc recv PASS_TO_XFRD"));
		xfrd->ipc_conn->is_reading = 1;
		break;
	case NSD_RELOAD_REQ:
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc recv RELOAD_REQ"));
		/* make reload happen, right away, and schedule file check */
		task_new_check_zonefiles(xfrd->nsd->task[xfrd->nsd->mytask],
			xfrd->last_task, NULL);
		xfrd_set_reload_now(xfrd);
		break;
	case NSD_RELOAD:
		/* main tells us that reload is done, stop ipc send to main */
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: ipc recv RELOAD"));
		ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
		xfrd->need_to_send_quit = 1;
		break;
        default:
                log_msg(LOG_ERR, "xfrd_handle_ipc: bad mode %d (%d)", (int)cmd,
			(int)ntohl(cmd));
                break;
        }

	if(xfrd->ipc_conn->is_reading) {
		/* setup read of info */
		xfrd->ipc_conn->total_bytes = 0;
		xfrd->ipc_conn->msglen = 0;
		buffer_clear(xfrd->ipc_conn->packet);
	}
}
