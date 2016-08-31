/*
 * xfrd.c - XFR (transfer) Daemon source file. Coordinates SOA updates.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "xfrd.h"
#include "xfrd-tcp.h"
#include "xfrd-disk.h"
#include "xfrd-notify.h"
#include "options.h"
#include "util.h"
#include "netio.h"
#include "region-allocator.h"
#include "nsd.h"
#include "packet.h"
#include "rdata.h"
#include "difffile.h"
#include "ipc.h"
#include "remote.h"

#define XFRD_TRANSFER_TIMEOUT_START 10 /* empty zone timeout is between x and 2*x seconds */
#define XFRD_TRANSFER_TIMEOUT_MAX 86400 /* empty zone timeout max expbackoff */
#define XFRD_UDP_TIMEOUT 10 /* seconds, before a udp request times out */
#define XFRD_NO_IXFR_CACHE 172800 /* 48h before retrying ixfr's after notimpl */
#define XFRD_LOWERBOUND_REFRESH 1 /* seconds, smallest refresh timeout */
#define XFRD_LOWERBOUND_RETRY 1 /* seconds, smallest retry timeout */
#define XFRD_MAX_ROUNDS 3 /* max number of rounds along the masters */
#define XFRD_TSIG_MAX_UNSIGNED 103 /* max number of packets without tsig in a tcp stream. */
			/* rfc recommends 100, +3 for offbyone errors/interoperability. */
#define XFRD_CHILD_REAP_TIMEOUT 60 /* seconds to wakeup and reap lost children */
		/* these are reload processes that SIGCHILDed but the signal
		 * was lost, and need waitpid to remove their process entry. */

/* the daemon state */
xfrd_state_t* xfrd = 0;

/* main xfrd loop */
static void xfrd_main(void);
/* shut down xfrd, close sockets. */
static void xfrd_shutdown(void);
/* delete pending task xfr files in tmp */
static void xfrd_clean_pending_tasks(struct nsd* nsd, udb_base* u);
/* create zone rbtree at start */
static void xfrd_init_zones(void);
/* initial handshake with SOAINFO from main and send expire to main */
static void xfrd_receive_soa(int socket, int shortsoa);

/* handle incoming notification message. soa can be NULL. true if transfer needed. */
static int xfrd_handle_incoming_notify(xfrd_zone_t* zone, xfrd_soa_t* soa);

/* call with buffer just after the soa dname. returns 0 on error. */
static int xfrd_parse_soa_info(buffer_type* packet, xfrd_soa_t* soa);
/* set the zone state to a new state (takes care of expiry messages) */
static void xfrd_set_zone_state(xfrd_zone_t* zone, enum xfrd_zone_state new_zone_state);
/* set timer for retry amount (depends on zone_state) */
static void xfrd_set_timer_retry(xfrd_zone_t* zone);
/* set timer for refresh timeout (depends on zone_state) */
static void xfrd_set_timer_refresh(xfrd_zone_t* zone);

/* set reload timeout */
static void xfrd_set_reload_timeout(void);
/* handle reload timeout */
static void xfrd_handle_reload(int fd, short event, void* arg);
/* handle child timeout */
static void xfrd_handle_child_timer(int fd, short event, void* arg);

/* send ixfr request, returns fd of connection to read on */
static int xfrd_send_ixfr_request_udp(xfrd_zone_t* zone);
/* obtain udp socket slot */
static void xfrd_udp_obtain(xfrd_zone_t* zone);

/* read data via udp */
static void xfrd_udp_read(xfrd_zone_t* zone);

/* find master by notify number */
static int find_same_master_notify(xfrd_zone_t* zone, int acl_num_nfy);

/* set the write timer to activate */
static void xfrd_write_timer_set(void);

static void
xfrd_signal_callback(int sig, short event, void* ATTR_UNUSED(arg))
{
	if(!(event & EV_SIGNAL))
		return;
	sig_handler(sig);
}

static void
xfrd_sigsetup(int sig)
{
	/* no need to remember the event ; dealloc on process exit */
	struct event *ev = xalloc_zero(sizeof(*ev));
	signal_set(ev, sig, xfrd_signal_callback, NULL);
	if(event_base_set(xfrd->event_base, ev) != 0) {
		log_msg(LOG_ERR, "xfrd sig handler: event_base_set failed");
	}
	if(signal_add(ev, NULL) != 0) {
		log_msg(LOG_ERR, "xfrd sig handler: signal_add failed");
	}
}

void
xfrd_init(int socket, struct nsd* nsd, int shortsoa, int reload_active,
	pid_t nsd_pid)
{
	region_type* region;

	assert(xfrd == 0);
	/* to setup signalhandling */
	nsd->server_kind = NSD_SERVER_MAIN;

	region = region_create_custom(xalloc, free, DEFAULT_CHUNK_SIZE,
		DEFAULT_LARGE_OBJECT_SIZE, DEFAULT_INITIAL_CLEANUP_SIZE, 1);
	xfrd = (xfrd_state_t*)region_alloc(region, sizeof(xfrd_state_t));
	memset(xfrd, 0, sizeof(xfrd_state_t));
	xfrd->region = region;
	xfrd->xfrd_start_time = time(0);
	xfrd->event_base = nsd_child_event_base();
	if(!xfrd->event_base) {
		log_msg(LOG_ERR, "xfrd: cannot create event base");
		exit(1);
	}
	xfrd->nsd = nsd;
	xfrd->packet = buffer_create(xfrd->region, QIOBUFSZ);
	xfrd->udp_waiting_first = NULL;
	xfrd->udp_waiting_last = NULL;
	xfrd->udp_use_num = 0;
	xfrd->got_time = 0;
	xfrd->xfrfilenumber = 0;
#ifdef USE_ZONE_STATS
	xfrd->zonestat_safe = nsd->zonestatdesired;
#endif
	xfrd->activated_first = NULL;
	xfrd->ipc_pass = buffer_create(xfrd->region, QIOBUFSZ);
	xfrd->last_task = region_alloc(xfrd->region, sizeof(*xfrd->last_task));
	udb_ptr_init(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask]);
	assert(shortsoa || udb_base_get_userdata(xfrd->nsd->task[xfrd->nsd->mytask])->data == 0);

	xfrd->reload_handler.ev_fd = -1;
	xfrd->reload_added = 0;
	xfrd->reload_timeout.tv_sec = 0;
	xfrd->reload_cmd_last_sent = xfrd->xfrd_start_time;
	xfrd->can_send_reload = !reload_active;
	xfrd->reload_pid = nsd_pid;
	xfrd->child_timer_added = 0;

	xfrd->ipc_send_blocked = 0;
	event_set(&xfrd->ipc_handler, socket, EV_PERSIST|EV_READ,
		xfrd_handle_ipc, xfrd);
	if(event_base_set(xfrd->event_base, &xfrd->ipc_handler) != 0)
		log_msg(LOG_ERR, "xfrd ipc handler: event_base_set failed");
	if(event_add(&xfrd->ipc_handler, NULL) != 0)
		log_msg(LOG_ERR, "xfrd ipc handler: event_add failed");
	xfrd->ipc_handler_flags = EV_PERSIST|EV_READ;
	xfrd->ipc_conn = xfrd_tcp_create(xfrd->region, QIOBUFSZ);
	/* not reading using ipc_conn yet */
	xfrd->ipc_conn->is_reading = 0;
	xfrd->ipc_conn->fd = socket;
	xfrd->need_to_send_reload = 0;
	xfrd->need_to_send_shutdown = 0;
	xfrd->need_to_send_stats = 0;

	xfrd->write_zonefile_needed = 0;
	if(nsd->options->zonefiles_write)
		xfrd_write_timer_set();

	xfrd->notify_waiting_first = NULL;
	xfrd->notify_waiting_last = NULL;
	xfrd->notify_udp_num = 0;

#ifdef HAVE_SSL
	daemon_remote_attach(xfrd->nsd->rc, xfrd);
#endif

	xfrd->tcp_set = xfrd_tcp_set_create(xfrd->region);
	xfrd->tcp_set->tcp_timeout = nsd->tcp_timeout;
#ifndef HAVE_ARC4RANDOM
	srandom((unsigned long) getpid() * (unsigned long) time(NULL));
#endif

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd pre-startup"));
	xfrd_init_zones();
	xfrd_receive_soa(socket, shortsoa);
	if(nsd->options->xfrdfile != NULL && nsd->options->xfrdfile[0]!=0)
		xfrd_read_state(xfrd);
	
	/* did we get killed before startup was successful? */
	if(nsd->signal_hint_shutdown) {
		kill(nsd_pid, SIGTERM);
		xfrd_shutdown();
		return;
	}

	/* init libevent signals now, so that in the previous init scripts
	 * the normal sighandler is called, and can set nsd->signal_hint..
	 * these are also looked at in sig_process before we run the main loop*/
	xfrd_sigsetup(SIGHUP);
	xfrd_sigsetup(SIGTERM);
	xfrd_sigsetup(SIGQUIT);
	xfrd_sigsetup(SIGCHLD);
	xfrd_sigsetup(SIGALRM);
	xfrd_sigsetup(SIGILL);
	xfrd_sigsetup(SIGUSR1);
	xfrd_sigsetup(SIGINT);

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd startup"));
	xfrd_main();
}

static void
xfrd_process_activated(void)
{
	xfrd_zone_t* zone;
	while((zone = xfrd->activated_first)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s activation",
			zone->apex_str));
		/* pop zone from activated list */
		xfrd->activated_first = zone->activated_next;
		if(zone->activated_next)
			zone->activated_next->activated_prev = NULL;
		zone->is_activated = 0;
		/* run it : no events, specifically not the TIMEOUT event,
		 * so that running zone transfers are not interrupted */
		xfrd_handle_zone(zone->zone_handler.ev_fd, 0, zone);
	}
}

static void
xfrd_sig_process(void)
{
	int status;
	pid_t child_pid;

	if(xfrd->nsd->signal_hint_quit || xfrd->nsd->signal_hint_shutdown) {
		xfrd->nsd->signal_hint_quit = 0;
		xfrd->nsd->signal_hint_shutdown = 0;
		xfrd->need_to_send_shutdown = 1;
		if(!(xfrd->ipc_handler_flags&EV_WRITE)) {
			ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
		}
	} else if(xfrd->nsd->signal_hint_reload_hup) {
		log_msg(LOG_WARNING, "SIGHUP received, reloading...");
		xfrd->nsd->signal_hint_reload_hup = 0;
		if(xfrd->nsd->options->zonefiles_check) {
			task_new_check_zonefiles(xfrd->nsd->task[
				xfrd->nsd->mytask], xfrd->last_task, NULL);
		}
		xfrd_set_reload_now(xfrd);
	} else if(xfrd->nsd->signal_hint_statsusr) {
		xfrd->nsd->signal_hint_statsusr = 0;
		xfrd->need_to_send_stats = 1;
		if(!(xfrd->ipc_handler_flags&EV_WRITE)) {
			ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
		}
	} 

	/* collect children that exited. */
	xfrd->nsd->signal_hint_child = 0;
	while((child_pid = waitpid(-1, &status, WNOHANG)) != -1 && child_pid != 0) {
		if(status != 0) {
			log_msg(LOG_ERR, "process %d exited with status %d",
				(int)child_pid, status);
		}
	}
	if(!xfrd->child_timer_added) {
		struct timeval tv;
		tv.tv_sec = XFRD_CHILD_REAP_TIMEOUT;
		tv.tv_usec = 0;
		event_set(&xfrd->child_timer, -1, EV_TIMEOUT,
			xfrd_handle_child_timer, xfrd);
		if(event_base_set(xfrd->event_base, &xfrd->child_timer) != 0)
			log_msg(LOG_ERR, "xfrd child timer: event_base_set failed");
		if(event_add(&xfrd->child_timer, &tv) != 0)
			log_msg(LOG_ERR, "xfrd child timer: event_add failed");
		xfrd->child_timer_added = 1;
	}
}

static void
xfrd_main(void)
{
	/* we may have signals from the startup period, process them */
	xfrd_sig_process();
	xfrd->shutdown = 0;
	while(!xfrd->shutdown)
	{
		/* process activated zones before blocking in select again */
		xfrd_process_activated();
		/* dispatch may block for a longer period, so current is gone */
		xfrd->got_time = 0;
		if(event_base_loop(xfrd->event_base, EVLOOP_ONCE) == -1) {
			if (errno != EINTR) {
				log_msg(LOG_ERR,
					"xfrd dispatch failed: %s",
					strerror(errno));
			}
		}
		xfrd_sig_process();
	}
	xfrd_shutdown();
}

static void
xfrd_shutdown()
{
	xfrd_zone_t* zone;

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd shutdown"));
	event_del(&xfrd->ipc_handler);
	close(xfrd->ipc_handler.ev_fd); /* notifies parent we stop */
	if(xfrd->nsd->options->xfrdfile != NULL && xfrd->nsd->options->xfrdfile[0]!=0)
		xfrd_write_state(xfrd);
	if(xfrd->reload_added) {
		event_del(&xfrd->reload_handler);
		xfrd->reload_added = 0;
	}
	if(xfrd->child_timer_added) {
		event_del(&xfrd->child_timer);
		xfrd->child_timer_added = 0;
	}
	if(xfrd->nsd->options->zonefiles_write) {
		event_del(&xfrd->write_timer);
	}
#ifdef HAVE_SSL
	daemon_remote_close(xfrd->nsd->rc); /* close sockets of rc */
#endif
	/* close sockets */
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		if(zone->event_added) {
			event_del(&zone->zone_handler);
			if(zone->zone_handler.ev_fd != -1) {
				close(zone->zone_handler.ev_fd);
				zone->zone_handler.ev_fd = -1;
			}
			zone->event_added = 0;
		}
	}
	close_notify_fds(xfrd->notify_zones);

	/* wait for server parent (if necessary) */
	if(xfrd->reload_pid != -1) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd wait for servermain %d",
			(int)xfrd->reload_pid));
		while(1) {
			if(waitpid(xfrd->reload_pid, NULL, 0) == -1) {
				if(errno == EINTR) continue;
				if(errno == ECHILD) break;
				log_msg(LOG_ERR, "xfrd: waitpid(%d): %s",
					(int)xfrd->reload_pid, strerror(errno));
			}
			break;
		}
	}

	/* if we are killed past this point this is not a problem,
	 * some files left in /tmp are cleaned by the OS, but it is neater
	 * to clean them out */

	/* unlink xfr files for running transfers */
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		if(zone->msg_seq_nr)
			xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
	}
	/* unlink xfr files in not-yet-done task file */
	xfrd_clean_pending_tasks(xfrd->nsd, xfrd->nsd->task[xfrd->nsd->mytask]);
	xfrd_del_tempdir(xfrd->nsd);

	/* process-exit cleans up memory used by xfrd process */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd shutdown complete"));

	exit(0);
}

static void
xfrd_clean_pending_tasks(struct nsd* nsd, udb_base* u)
{
	udb_ptr t;
	udb_ptr_new(&t, u, udb_base_get_userdata(u));
	/* no dealloc of entries, we delete the entire file when done */
	while(!udb_ptr_is_null(&t)) {
		if(TASKLIST(&t)->task_type == task_apply_xfr) {
			xfrd_unlink_xfrfile(nsd, TASKLIST(&t)->yesno);
		}
		udb_ptr_set_rptr(&t, u, &TASKLIST(&t)->next);
	}
	udb_ptr_unlink(&t, u);
}

void
xfrd_init_slave_zone(xfrd_state_t* xfrd, zone_options_t* zone_opt)
{
	xfrd_zone_t *xzone;
	xzone = (xfrd_zone_t*)region_alloc(xfrd->region, sizeof(xfrd_zone_t));
	memset(xzone, 0, sizeof(xfrd_zone_t));
	xzone->apex = zone_opt->node.key;
	xzone->apex_str = zone_opt->name;
	xzone->state = xfrd_zone_refreshing;
	xzone->zone_options = zone_opt;
	/* first retry will use first master */
	xzone->master = 0;
	xzone->master_num = 0;
	xzone->next_master = 0;
	xzone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_START;

	xzone->soa_nsd_acquired = 0;
	xzone->soa_disk_acquired = 0;
	xzone->soa_notified_acquired = 0;
	/* [0]=1, [1]=0; "." domain name */
	xzone->soa_nsd.prim_ns[0] = 1;
	xzone->soa_nsd.email[0] = 1;
	xzone->soa_disk.prim_ns[0]=1;
	xzone->soa_disk.email[0]=1;
	xzone->soa_notified.prim_ns[0]=1;
	xzone->soa_notified.email[0]=1;

	xzone->zone_handler.ev_fd = -1;
	xzone->zone_handler_flags = 0;
	xzone->event_added = 0;

	xzone->tcp_conn = -1;
	xzone->tcp_waiting = 0;
	xzone->udp_waiting = 0;
	xzone->is_activated = 0;

	tsig_create_record_custom(&xzone->tsig, NULL, 0, 0, 4);

	/* set refreshing anyway, if we have data it may be old */
	xfrd_set_refresh_now(xzone);

	xzone->node.key = xzone->apex;
	rbtree_insert(xfrd->zones, (rbnode_t*)xzone);
}

static void
xfrd_init_zones()
{
	zone_options_t *zone_opt;
	assert(xfrd->zones == 0);

	xfrd->zones = rbtree_create(xfrd->region,
		(int (*)(const void *, const void *)) dname_compare);
	xfrd->notify_zones = rbtree_create(xfrd->region,
		(int (*)(const void *, const void *)) dname_compare);

	RBTREE_FOR(zone_opt, zone_options_t*, xfrd->nsd->options->zone_options)
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: adding %s zone",
			zone_opt->name));

		init_notify_send(xfrd->notify_zones, xfrd->region, zone_opt);
		if(!zone_is_slave(zone_opt)) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s, "
				"master zone has no outgoing xfr requests",
				zone_opt->name));
			continue;
		}
		xfrd_init_slave_zone(xfrd, zone_opt);
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: started server %d "
		"secondary zones", (int)xfrd->zones->count));
}

static void
xfrd_process_soa_info_task(struct task_list_d* task)
{
	xfrd_soa_t soa;
	xfrd_soa_t* soa_ptr = &soa;
	xfrd_zone_t* zone;
	DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: process SOAINFO %s",
		dname_to_string(task->zname, 0)));
	zone = (xfrd_zone_t*)rbtree_search(xfrd->zones, task->zname);
	if(task->size <= sizeof(struct task_list_d)+dname_total_size(
		task->zname)+sizeof(uint32_t)*6 + sizeof(uint8_t)*2) {
		/* NSD has zone without any info */
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "SOAINFO for %s lost zone",
			dname_to_string(task->zname,0)));
		soa_ptr = NULL;
	} else {
		uint8_t* p = (uint8_t*)task->zname + dname_total_size(
			task->zname);
		/* read the soa info */
		memset(&soa, 0, sizeof(soa));
		/* left out type, klass, count for speed */
		soa.type = htons(TYPE_SOA);
		soa.klass = htons(CLASS_IN);
		memmove(&soa.ttl, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		soa.rdata_count = htons(7);
		memmove(soa.prim_ns, p, sizeof(uint8_t));
		p += sizeof(uint8_t);
		memmove(soa.prim_ns+1, p, soa.prim_ns[0]);
		p += soa.prim_ns[0];
		memmove(soa.email, p, sizeof(uint8_t));
		p += sizeof(uint8_t);
		memmove(soa.email+1, p, soa.email[0]);
		p += soa.email[0];
		memmove(&soa.serial, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		memmove(&soa.refresh, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		memmove(&soa.retry, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		memmove(&soa.expire, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		memmove(&soa.minimum, p, sizeof(uint32_t));
		p += sizeof(uint32_t);
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "SOAINFO for %s %u",
			dname_to_string(task->zname,0),
			(unsigned)ntohl(soa.serial)));
	}

	if(!zone) {
		DEBUG(DEBUG_IPC,1, (LOG_INFO, "xfrd: zone %s master zone updated",
			dname_to_string(task->zname,0)));
		notify_handle_master_zone_soainfo(xfrd->notify_zones,
			task->zname, soa_ptr);
		return;
	}
	xfrd_handle_incoming_soa(zone, soa_ptr, xfrd_time());
}

static void
xfrd_receive_soa(int socket, int shortsoa)
{
	sig_atomic_t cmd;
	struct udb_base* xtask = xfrd->nsd->task[xfrd->nsd->mytask];
	udb_ptr last_task, t;
	xfrd_zone_t* zone;

	if(!shortsoa) {
		/* put all expired zones into mytask */
		udb_ptr_init(&last_task, xtask);
		RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones) {
			if(zone->state == xfrd_zone_expired) {
				task_new_expire(xtask, &last_task, zone->apex, 1);
			}
		}
		udb_ptr_unlink(&last_task, xtask);
	
		/* send RELOAD to main to give it this tasklist */
		task_process_sync(xtask);
		cmd = NSD_RELOAD;
		if(!write_socket(socket, &cmd,  sizeof(cmd))) {
			log_msg(LOG_ERR, "problems sending reload xfrdtomain: %s",
				strerror(errno));
		}
	}

	/* receive RELOAD_DONE to get SOAINFO tasklist */
	if(block_read(&nsd, socket, &cmd, sizeof(cmd), -1) != sizeof(cmd) ||
		cmd != NSD_RELOAD_DONE) {
		if(nsd.signal_hint_shutdown)
			return;
		log_msg(LOG_ERR, "did not get start signal from main");
		exit(1);
	}
	if(block_read(NULL, socket, &xfrd->reload_pid, sizeof(pid_t), -1)
		!= sizeof(pid_t)) {
		log_msg(LOG_ERR, "xfrd cannot get reload_pid");
	}

	/* process tasklist (SOAINFO data) */
	udb_ptr_unlink(xfrd->last_task, xtask);
	/* if shortsoa: then use my own taskdb that nsdparent filled */
	if(!shortsoa)
		xfrd->nsd->mytask = 1 - xfrd->nsd->mytask;
	xtask = xfrd->nsd->task[xfrd->nsd->mytask];
	task_remap(xtask);
	udb_ptr_new(&t, xtask, udb_base_get_userdata(xtask));
	while(!udb_ptr_is_null(&t)) {
		xfrd_process_soa_info_task(TASKLIST(&t));
	 	udb_ptr_set_rptr(&t, xtask, &TASKLIST(&t)->next);
	}
	udb_ptr_unlink(&t, xtask);
	task_clear(xtask);
	udb_ptr_init(xfrd->last_task, xfrd->nsd->task[xfrd->nsd->mytask]);

	if(!shortsoa) {
		/* receive RELOAD_DONE that signals the other tasklist is
		 * empty, and thus xfrd can operate (can call reload and swap
		 * to the other, empty, tasklist) */
		if(block_read(NULL, socket, &cmd, sizeof(cmd), -1) !=
			sizeof(cmd) ||
			cmd != NSD_RELOAD_DONE) {
			log_msg(LOG_ERR, "did not get start signal 2 from "
				"main");
			exit(1);
		}
	} else {
		/* for shortsoa version, do expire later */
		/* if expire notifications, put in my task and
		 * schedule a reload to make sure they are processed */
		RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones) {
			if(zone->state == xfrd_zone_expired) {
				xfrd_send_expire_notification(zone);
			}
		}
	}
}

void
xfrd_reopen_logfile(void)
{
	if (xfrd->nsd->file_rotation_ok)
		log_reopen(xfrd->nsd->log_filename, 0);
}

void
xfrd_deactivate_zone(xfrd_zone_t* z)
{
	if(z->is_activated) {
		/* delete from activated list */
		if(z->activated_prev)
			z->activated_prev->activated_next = z->activated_next;
		else	xfrd->activated_first = z->activated_next;
		if(z->activated_next)
			z->activated_next->activated_prev = z->activated_prev;
		z->is_activated = 0;
	}
}

void
xfrd_del_slave_zone(xfrd_state_t* xfrd, const dname_type* dname)
{
	xfrd_zone_t* z = (xfrd_zone_t*)rbtree_delete(xfrd->zones, dname);
	if(!z) return;
	
	/* io */
	if(z->tcp_waiting) {
		/* delete from tcp waiting list */
		if(z->tcp_waiting_prev)
			z->tcp_waiting_prev->tcp_waiting_next =
				z->tcp_waiting_next;
		else xfrd->tcp_set->tcp_waiting_first = z->tcp_waiting_next;
		if(z->tcp_waiting_next)
			z->tcp_waiting_next->tcp_waiting_prev =
				z->tcp_waiting_prev;
		else xfrd->tcp_set->tcp_waiting_last = z->tcp_waiting_prev;
		z->tcp_waiting = 0;
	}
	if(z->udp_waiting) {
		/* delete from udp waiting list */
		if(z->udp_waiting_prev)
			z->udp_waiting_prev->udp_waiting_next =
				z->udp_waiting_next;
		else	xfrd->udp_waiting_first = z->udp_waiting_next;
		if(z->udp_waiting_next)
			z->udp_waiting_next->udp_waiting_prev =
				z->udp_waiting_prev;
		else	xfrd->udp_waiting_last = z->udp_waiting_prev;
		z->udp_waiting = 0;
	}
	xfrd_deactivate_zone(z);
	if(z->tcp_conn != -1) {
		xfrd_tcp_release(xfrd->tcp_set, z);
	} else if(z->zone_handler.ev_fd != -1 && z->event_added) {
		xfrd_udp_release(z);
	} else if(z->event_added)
		event_del(&z->zone_handler);
	if(z->msg_seq_nr)
		xfrd_unlink_xfrfile(xfrd->nsd, z->xfrfilenumber);

	/* tsig */
	tsig_delete_record(&z->tsig, NULL);

	/* z->dname is recycled when the zone_options is removed */
	region_recycle(xfrd->region, z, sizeof(*z));
}

void
xfrd_free_namedb(struct nsd* nsd)
{
	namedb_close_udb(nsd->db);
	namedb_close(nsd->db);
	nsd->db = 0;
}

static void
xfrd_set_timer_refresh(xfrd_zone_t* zone)
{
	time_t set_refresh;
	time_t set_expire;
	time_t set_min;
	time_t set;
	if(zone->soa_disk_acquired == 0 || zone->state != xfrd_zone_ok) {
		xfrd_set_timer_retry(zone);
		return;
	}
	/* refresh or expire timeout, whichever is earlier */
	set_refresh = ntohl(zone->soa_disk.refresh);
	if (set_refresh > zone->zone_options->pattern->max_refresh_time)
		set_refresh = zone->zone_options->pattern->max_refresh_time;
	else if (set_refresh < zone->zone_options->pattern->min_refresh_time)
		set_refresh = zone->zone_options->pattern->min_refresh_time;
	set_refresh += zone->soa_disk_acquired;
	set_expire = zone->soa_disk_acquired + ntohl(zone->soa_disk.expire);
	if(set_refresh < set_expire)
		set = set_refresh;
	else set = set_expire;
	set_min = zone->soa_disk_acquired + XFRD_LOWERBOUND_REFRESH;
	if(set < set_min)
		set = set_min;
	if(set < xfrd_time())
		set = 0;
	else	set -= xfrd_time();
	xfrd_set_timer(zone, set);
}

static void
xfrd_set_timer_retry(xfrd_zone_t* zone)
{
	time_t set_retry;
	/* set timer for next retry or expire timeout if earlier. */
	if(zone->soa_disk_acquired == 0) {
		/* if no information, use reasonable timeout */
		if(zone->fresh_xfr_timeout == 0)
			zone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_START;
#ifdef HAVE_ARC4RANDOM_UNIFORM
		xfrd_set_timer(zone, zone->fresh_xfr_timeout
			+ arc4random_uniform(zone->fresh_xfr_timeout));
#elif HAVE_ARC4RANDOM
		xfrd_set_timer(zone, zone->fresh_xfr_timeout
                        + arc4random() % zone->fresh_xfr_timeout);
#else
		xfrd_set_timer(zone, zone->fresh_xfr_timeout
			+ random()%zone->fresh_xfr_timeout);
#endif
		/* exponential backoff - some master data in zones is paid-for
		   but non-working, and will not get fixed. */
		zone->fresh_xfr_timeout *= 2;
		if(zone->fresh_xfr_timeout > XFRD_TRANSFER_TIMEOUT_MAX)
			zone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_MAX;
	} else if(zone->state == xfrd_zone_expired ||
		xfrd_time() + (time_t)ntohl(zone->soa_disk.retry) <
		zone->soa_disk_acquired + (time_t)ntohl(zone->soa_disk.expire))
	{
		set_retry = ntohl(zone->soa_disk.retry);
		if(set_retry > zone->zone_options->pattern->max_retry_time)
			set_retry = zone->zone_options->pattern->max_retry_time;
		else if(set_retry < zone->zone_options->pattern->min_retry_time)
			set_retry = zone->zone_options->pattern->min_retry_time;
		if(set_retry < XFRD_LOWERBOUND_RETRY)
			set_retry =  XFRD_LOWERBOUND_RETRY;
		xfrd_set_timer(zone, set_retry);
	} else {
		if(ntohl(zone->soa_disk.expire) < XFRD_LOWERBOUND_RETRY)
			xfrd_set_timer(zone, XFRD_LOWERBOUND_RETRY);
		else {
			if(zone->soa_disk_acquired + (time_t)ntohl(zone->soa_disk.expire) < xfrd_time())
				xfrd_set_timer(zone, XFRD_LOWERBOUND_RETRY);
			else xfrd_set_timer(zone, zone->soa_disk_acquired +
				ntohl(zone->soa_disk.expire) - xfrd_time());
		}
	}
}

void
xfrd_handle_zone(int ATTR_UNUSED(fd), short event, void* arg)
{
	xfrd_zone_t* zone = (xfrd_zone_t*)arg;

	if(zone->tcp_conn != -1) {
		if(event == 0) /* activated, but already in TCP, nothing to do*/
			return;
		/* busy in tcp transaction: an internal error */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event tcp", zone->apex_str));
		xfrd_tcp_release(xfrd->tcp_set, zone);
		/* continue to retry; as if a timeout happened */
		event = EV_TIMEOUT;
	}

	if((event & EV_READ)) {
		/* busy in udp transaction */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s event udp read", zone->apex_str));
		xfrd_udp_read(zone);
		return;
	}

	/* timeout */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s timeout", zone->apex_str));
	if(zone->zone_handler.ev_fd != -1 && zone->event_added &&
		(event & EV_TIMEOUT)) {
		assert(zone->tcp_conn == -1);
		xfrd_udp_release(zone);
	}

	if(zone->tcp_waiting) {
		DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s skips retry, TCP connections full",
			zone->apex_str));
		xfrd_unset_timer(zone);
		return;
	}
	if(zone->udp_waiting) {
		DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s skips retry, UDP connections full",
			zone->apex_str));
		xfrd_unset_timer(zone);
		return;
	}

	if(zone->soa_disk_acquired)
	{
		if (zone->state != xfrd_zone_expired &&
			xfrd_time() >= zone->soa_disk_acquired + (time_t)ntohl(zone->soa_disk.expire)) {
			/* zone expired */
			log_msg(LOG_ERR, "xfrd: zone %s has expired", zone->apex_str);
			xfrd_set_zone_state(zone, xfrd_zone_expired);
		}
		else if(zone->state == xfrd_zone_ok &&
			xfrd_time() >= zone->soa_disk_acquired + (time_t)ntohl(zone->soa_disk.refresh)) {
			/* zone goes to refreshing state. */
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s is refreshing", zone->apex_str));
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
		}
	}

	/* only make a new request if no request is running (UDPorTCP) */
	if(zone->zone_handler.ev_fd == -1 && zone->tcp_conn == -1) {
		/* make a new request */
		xfrd_make_request(zone);
	}
}

void
xfrd_make_request(xfrd_zone_t* zone)
{
	if(zone->next_master != -1) {
		/* we are told to use this next master */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd zone %s use master %i",
			zone->apex_str, zone->next_master));
		zone->master_num = zone->next_master;
		zone->master = acl_find_num(zone->zone_options->pattern->
			request_xfr, zone->master_num);
		/* if there is no next master, fallback to use the first one */
		if(!zone->master) {
			zone->master = zone->zone_options->pattern->request_xfr;
			zone->master_num = 0;
		}
		/* fallback to cycle master */
		zone->next_master = -1;
		zone->round_num = 0; /* fresh set of retries after notify */
	} else {
		/* cycle master */

		if(zone->round_num != -1 && zone->master && zone->master->next)
		{
			/* try the next master */
			zone->master = zone->master->next;
			zone->master_num++;
		} else {
			/* start a new round */
			zone->master = zone->zone_options->pattern->request_xfr;
			zone->master_num = 0;
			zone->round_num++;
		}
		if(zone->round_num >= XFRD_MAX_ROUNDS) {
			/* tried all servers that many times, wait */
			zone->round_num = -1;
			xfrd_set_timer_retry(zone);
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd zone %s makereq wait_retry, rd %d mr %d nx %d",
				zone->apex_str, zone->round_num, zone->master_num, zone->next_master));
			return;
		}
	}

	/* cache ixfr_disabled only for XFRD_NO_IXFR_CACHE time */
	if (zone->master->ixfr_disabled &&
	   (zone->master->ixfr_disabled + XFRD_NO_IXFR_CACHE) <= time(NULL)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "clear negative caching ixfr "
						"disabled for master %s num "
						"%d ",
			zone->master->ip_address_spec, zone->master_num));
		zone->master->ixfr_disabled = 0;
	}

	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s make request round %d mr %d nx %d",
		zone->apex_str, zone->round_num, zone->master_num, zone->next_master));
	/* perform xfr request */
	if (!zone->master->use_axfr_only && zone->soa_disk_acquired > 0 &&
		!zone->master->ixfr_disabled) {

		if (zone->master->allow_udp) {
			xfrd_set_timer(zone, XFRD_UDP_TIMEOUT);
			xfrd_udp_obtain(zone);
		}
		else { /* doing 3 rounds of IXFR/TCP might not be useful */
			xfrd_set_timer(zone, xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
		}
	}
	else if (zone->master->use_axfr_only || zone->soa_disk_acquired <= 0) {
		xfrd_set_timer(zone, xfrd->tcp_set->tcp_timeout);
		xfrd_tcp_obtain(xfrd->tcp_set, zone);
	}
	else if (zone->master->ixfr_disabled) {
		if (zone->zone_options->pattern->allow_axfr_fallback) {
			xfrd_set_timer(zone, xfrd->tcp_set->tcp_timeout);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
		} else {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s axfr "
				"fallback not allowed, skipping master %s.",
				zone->apex_str, zone->master->ip_address_spec));
		}
	}
}

static void
xfrd_udp_obtain(xfrd_zone_t* zone)
{
	assert(zone->udp_waiting == 0);
	if(zone->tcp_conn != -1) {
		/* no tcp and udp at the same time */
		xfrd_tcp_release(xfrd->tcp_set, zone);
	}
	if(xfrd->udp_use_num < XFRD_MAX_UDP) {
		int fd;
		xfrd->udp_use_num++;
		fd = xfrd_send_ixfr_request_udp(zone);
		if(fd == -1)
			xfrd->udp_use_num--;
		else {
			if(zone->event_added)
				event_del(&zone->zone_handler);
			event_set(&zone->zone_handler, fd,
				EV_PERSIST|EV_READ|EV_TIMEOUT,
				xfrd_handle_zone, zone);
			if(event_base_set(xfrd->event_base, &zone->zone_handler) != 0)
				log_msg(LOG_ERR, "xfrd udp: event_base_set failed");
			if(event_add(&zone->zone_handler, &zone->timeout) != 0)
				log_msg(LOG_ERR, "xfrd udp: event_add failed");
			zone->zone_handler_flags=EV_PERSIST|EV_READ|EV_TIMEOUT;
			zone->event_added = 1;
		}
		return;
	}
	/* queue the zone as last */
	zone->udp_waiting = 1;
	zone->udp_waiting_next = NULL;
	zone->udp_waiting_prev = xfrd->udp_waiting_last;
	if(!xfrd->udp_waiting_first)
		xfrd->udp_waiting_first = zone;
	if(xfrd->udp_waiting_last)
		xfrd->udp_waiting_last->udp_waiting_next = zone;
	xfrd->udp_waiting_last = zone;
	xfrd_unset_timer(zone);
}

time_t
xfrd_time()
{
	if(!xfrd->got_time) {
		xfrd->current_time = time(0);
		xfrd->got_time = 1;
	}
	return xfrd->current_time;
}

void
xfrd_copy_soa(xfrd_soa_t* soa, rr_type* rr)
{
	const uint8_t* rr_ns_wire = dname_name(domain_dname(rdata_atom_domain(rr->rdatas[0])));
	uint8_t rr_ns_len = domain_dname(rdata_atom_domain(rr->rdatas[0]))->name_size;
	const uint8_t* rr_em_wire = dname_name(domain_dname(rdata_atom_domain(rr->rdatas[1])));
	uint8_t rr_em_len = domain_dname(rdata_atom_domain(rr->rdatas[1]))->name_size;

	if(rr->type != TYPE_SOA || rr->rdata_count != 7) {
		log_msg(LOG_ERR, "xfrd: copy_soa called with bad rr, type %d rrs %u.",
			rr->type, rr->rdata_count);
		return;
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: copy_soa rr, type %d rrs %u, ttl %u.",
			(int)rr->type, (unsigned)rr->rdata_count, (unsigned)rr->ttl));
	soa->type = htons(rr->type);
	soa->klass = htons(rr->klass);
	soa->ttl = htonl(rr->ttl);
	soa->rdata_count = htons(rr->rdata_count);

	/* copy dnames */
	soa->prim_ns[0] = rr_ns_len;
	memcpy(soa->prim_ns+1, rr_ns_wire, rr_ns_len);
	soa->email[0] = rr_em_len;
	memcpy(soa->email+1, rr_em_wire, rr_em_len);

	/* already in network format */
	memcpy(&soa->serial, rdata_atom_data(rr->rdatas[2]), sizeof(uint32_t));
	memcpy(&soa->refresh, rdata_atom_data(rr->rdatas[3]), sizeof(uint32_t));
	memcpy(&soa->retry, rdata_atom_data(rr->rdatas[4]), sizeof(uint32_t));
	memcpy(&soa->expire, rdata_atom_data(rr->rdatas[5]), sizeof(uint32_t));
	memcpy(&soa->minimum, rdata_atom_data(rr->rdatas[6]), sizeof(uint32_t));
	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd: copy_soa rr, serial %u refresh %u retry %u expire %u",
		(unsigned)ntohl(soa->serial), (unsigned)ntohl(soa->refresh),
		(unsigned)ntohl(soa->retry), (unsigned)ntohl(soa->expire)));
}

static void
xfrd_set_zone_state(xfrd_zone_t* zone, enum xfrd_zone_state s)
{
	if(s != zone->state) {
		enum xfrd_zone_state old = zone->state;
		zone->state = s;
		if((s == xfrd_zone_expired || old == xfrd_zone_expired)
			&& s!=old) {
			xfrd_send_expire_notification(zone);
		}
	}
}

void
xfrd_set_refresh_now(xfrd_zone_t* zone)
{
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd zone %s is activated, state %d",
		zone->apex_str, zone->state));
	if(!zone->is_activated) {
		/* push onto list */
		zone->activated_prev = 0;
		zone->activated_next = xfrd->activated_first;
		if(xfrd->activated_first)
			xfrd->activated_first->activated_prev = zone;
		xfrd->activated_first = zone;
		zone->is_activated = 1;
	}
}

void
xfrd_unset_timer(xfrd_zone_t* zone)
{
	assert(zone->zone_handler.ev_fd == -1);
	if(zone->event_added)
		event_del(&zone->zone_handler);
	zone->zone_handler_flags = 0;
	zone->event_added = 0;
}

void
xfrd_set_timer(xfrd_zone_t* zone, time_t t)
{
	int fd = zone->zone_handler.ev_fd;
	int fl = ((fd == -1)?EV_TIMEOUT:zone->zone_handler_flags);
	/* randomize the time, within 90%-100% of original */
	/* not later so zones cannot expire too late */
	/* only for times far in the future */
	if(t > 10) {
		time_t base = t*9/10;
#ifdef HAVE_ARC4RANDOM_UNIFORM
		t = base + arc4random_uniform(t-base);
#elif HAVE_ARC4RANDOM
		t = base + arc4random() % (t-base);
#else
		t = base + random()%(t-base);
#endif
	}

	/* keep existing flags and fd, but re-add with timeout */
	if(zone->event_added)
		event_del(&zone->zone_handler);
	else	fd = -1;
	zone->timeout.tv_sec = t;
	zone->timeout.tv_usec = 0;
	event_set(&zone->zone_handler, fd, fl, xfrd_handle_zone, zone);
	if(event_base_set(xfrd->event_base, &zone->zone_handler) != 0)
		log_msg(LOG_ERR, "xfrd timer: event_base_set failed");
	if(event_add(&zone->zone_handler, &zone->timeout) != 0)
		log_msg(LOG_ERR, "xfrd timer: event_add failed");
	zone->zone_handler_flags = fl;
	zone->event_added = 1;
}

void
xfrd_handle_incoming_soa(xfrd_zone_t* zone,
	xfrd_soa_t* soa, time_t acquired)
{
	if(soa == NULL) {
		/* nsd no longer has a zone in memory */
		zone->soa_nsd_acquired = 0;
		xfrd_set_zone_state(zone, xfrd_zone_refreshing);
		xfrd_set_refresh_now(zone);
		return;
	}
	if(zone->soa_nsd_acquired && soa->serial == zone->soa_nsd.serial)
		return;

	if(zone->soa_disk_acquired && soa->serial == zone->soa_disk.serial)
	{
		/* soa in disk has been loaded in memory */
		log_msg(LOG_INFO, "zone %s serial %u is updated to %u.",
			zone->apex_str, (unsigned)ntohl(zone->soa_nsd.serial),
			(unsigned)ntohl(soa->serial));
		zone->soa_nsd = zone->soa_disk;
		zone->soa_nsd_acquired = zone->soa_disk_acquired;
		xfrd->write_zonefile_needed = 1;
		if(xfrd_time() - zone->soa_disk_acquired
			< (time_t)ntohl(zone->soa_disk.refresh))
		{
			/* zone ok, wait for refresh time */
			xfrd_set_zone_state(zone, xfrd_zone_ok);
			zone->round_num = -1;
			xfrd_set_timer_refresh(zone);
		} else if(xfrd_time() - zone->soa_disk_acquired
			< (time_t)ntohl(zone->soa_disk.expire))
		{
			/* zone refreshing */
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
			xfrd_set_refresh_now(zone);
		}
		if(xfrd_time() - zone->soa_disk_acquired
			>= (time_t)ntohl(zone->soa_disk.expire)) {
			/* zone expired */
			xfrd_set_zone_state(zone, xfrd_zone_expired);
			xfrd_set_refresh_now(zone);
		}

		if(zone->soa_notified_acquired != 0 &&
			(zone->soa_notified.serial == 0 ||
		   	compare_serial(ntohl(zone->soa_disk.serial),
				ntohl(zone->soa_notified.serial)) >= 0))
		{	/* read was in response to this notification */
			zone->soa_notified_acquired = 0;
		}
		if(zone->soa_notified_acquired && zone->state == xfrd_zone_ok)
		{
			/* refresh because of notification */
			xfrd_set_zone_state(zone, xfrd_zone_refreshing);
			xfrd_set_refresh_now(zone);
		}
		xfrd_send_notify(xfrd->notify_zones, zone->apex, &zone->soa_nsd);
		return;
	}

	/* user must have manually provided zone data */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd: zone %s serial %u from zonefile. refreshing",
		zone->apex_str, (unsigned)ntohl(soa->serial)));
	zone->soa_nsd = *soa;
	zone->soa_disk = *soa;
	zone->soa_nsd_acquired = acquired;
	zone->soa_disk_acquired = acquired;
	if(zone->soa_notified_acquired != 0 &&
		(zone->soa_notified.serial == 0 ||
	   	compare_serial(ntohl(zone->soa_disk.serial),
			ntohl(zone->soa_notified.serial)) >= 0))
	{	/* user provided in response to this notification */
		zone->soa_notified_acquired = 0;
	}
	xfrd_set_zone_state(zone, xfrd_zone_refreshing);
	xfrd_set_refresh_now(zone);
	xfrd_send_notify(xfrd->notify_zones, zone->apex, &zone->soa_nsd);
}

void
xfrd_send_expire_notification(xfrd_zone_t* zone)
{
	task_new_expire(xfrd->nsd->task[xfrd->nsd->mytask], xfrd->last_task,
		zone->apex, zone->state == xfrd_zone_expired);
	xfrd_set_reload_timeout();
}

int
xfrd_udp_read_packet(buffer_type* packet, int fd)
{
	ssize_t received;

	/* read the data */
	buffer_clear(packet);
	received = recvfrom(fd, buffer_begin(packet), buffer_remaining(packet),
		0, NULL, NULL);
	if(received == -1) {
		log_msg(LOG_ERR, "xfrd: recvfrom failed: %s",
			strerror(errno));
		return 0;
	}
	buffer_set_limit(packet, received);
	return 1;
}

void
xfrd_udp_release(xfrd_zone_t* zone)
{
	assert(zone->udp_waiting == 0);
	if(zone->event_added)
		event_del(&zone->zone_handler);
	if(zone->zone_handler.ev_fd != -1) {
		close(zone->zone_handler.ev_fd);
	}
	zone->zone_handler.ev_fd = -1;
	zone->zone_handler_flags = 0;
	zone->event_added = 0;
	/* see if there are waiting zones */
	if(xfrd->udp_use_num == XFRD_MAX_UDP)
	{
		while(xfrd->udp_waiting_first) {
			/* snip off waiting list */
			xfrd_zone_t* wz = xfrd->udp_waiting_first;
			assert(wz->udp_waiting);
			wz->udp_waiting = 0;
			xfrd->udp_waiting_first = wz->udp_waiting_next;
			if(wz->udp_waiting_next)
				wz->udp_waiting_next->udp_waiting_prev = NULL;
			if(xfrd->udp_waiting_last == wz)
				xfrd->udp_waiting_last = NULL;
			/* see if this zone needs udp connection */
			if(wz->tcp_conn == -1) {
				int fd = xfrd_send_ixfr_request_udp(wz);
				if(fd != -1) {
					if(wz->event_added)
						event_del(&wz->zone_handler);
					event_set(&wz->zone_handler, fd,
						EV_READ|EV_TIMEOUT|EV_PERSIST,
						xfrd_handle_zone, wz);
					if(event_base_set(xfrd->event_base,
						&wz->zone_handler) != 0)
						log_msg(LOG_ERR, "cannot set event_base for ixfr");
					if(event_add(&wz->zone_handler, &wz->timeout) != 0)
						log_msg(LOG_ERR, "cannot add event for ixfr");
					wz->zone_handler_flags = EV_READ|EV_TIMEOUT|EV_PERSIST;
					wz->event_added = 1;
					return;
				} else {
					/* make this zone do something with
					 * this failure to act */
					xfrd_set_refresh_now(wz);
				}
			}
		}
	}
	/* no waiting zones */
	if(xfrd->udp_use_num > 0)
		xfrd->udp_use_num--;
}

static void
xfrd_udp_read(xfrd_zone_t* zone)
{
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s read udp data", zone->apex_str));
	if(!xfrd_udp_read_packet(xfrd->packet, zone->zone_handler.ev_fd)) {
		zone->master->bad_xfr_count++;
		if (zone->master->bad_xfr_count > 2) {
			zone->master->ixfr_disabled = time(NULL);
			zone->master->bad_xfr_count = 0;
		}
		/* drop packet */
		xfrd_udp_release(zone);
		/* query next server */
		xfrd_make_request(zone);
		return;
	}
	switch(xfrd_handle_received_xfr_packet(zone, xfrd->packet)) {
		case xfrd_packet_tcp:
			xfrd_set_timer(zone, xfrd->tcp_set->tcp_timeout);
			xfrd_udp_release(zone);
			xfrd_tcp_obtain(xfrd->tcp_set, zone);
			break;
		case xfrd_packet_transfer:
		case xfrd_packet_newlease:
			/* nothing more to do */
			assert(zone->round_num == -1);
			xfrd_udp_release(zone);
			break;
		case xfrd_packet_notimpl:
			zone->master->ixfr_disabled = time(NULL);
			/* drop packet */
			xfrd_udp_release(zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
		case xfrd_packet_more:
		case xfrd_packet_drop:
			/* drop packet */
			xfrd_udp_release(zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
		case xfrd_packet_bad:
		default:
			zone->master->bad_xfr_count++;
			if (zone->master->bad_xfr_count > 2) {
				zone->master->ixfr_disabled = time(NULL);
				zone->master->bad_xfr_count = 0;
			}
			/* drop packet */
			xfrd_udp_release(zone);
			/* query next server */
			xfrd_make_request(zone);
			break;
	}
}

int
xfrd_send_udp(acl_options_t* acl, buffer_type* packet, acl_options_t* ifc)
{
#ifdef INET6
	struct sockaddr_storage to;
#else
	struct sockaddr_in to;
#endif /* INET6 */
	int fd, family;

	/* this will set the remote port to acl->port or TCP_PORT */
	socklen_t to_len = xfrd_acl_sockaddr_to(acl, &to);

	/* get the address family of the remote host */
	if(acl->is_ipv6) {
#ifdef INET6
		family = PF_INET6;
#else
		return -1;
#endif /* INET6 */
	} else {
		family = PF_INET;
	}

	fd = socket(family, SOCK_DGRAM, IPPROTO_UDP);
	if(fd == -1) {
		log_msg(LOG_ERR, "xfrd: cannot create udp socket to %s: %s",
			acl->ip_address_spec, strerror(errno));
		return -1;
	}

	/* bind it */
	if (!xfrd_bind_local_interface(fd, ifc, acl, 0)) {
		log_msg(LOG_ERR, "xfrd: cannot bind outgoing interface '%s' to "
				 "udp socket: No matching ip addresses found",
			ifc->ip_address_spec);
		close(fd);
		return -1;
	}

	/* send it (udp) */
	if(sendto(fd,
		buffer_current(packet),
		buffer_remaining(packet), 0,
		(struct sockaddr*)&to, to_len) == -1)
	{
		log_msg(LOG_ERR, "xfrd: sendto %s failed %s",
			acl->ip_address_spec, strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

int
xfrd_bind_local_interface(int sockd, acl_options_t* ifc, acl_options_t* acl,
	int tcp)
{
#ifdef SO_LINGER
	struct linger linger = {1, 0};
#endif
	socklen_t frm_len;
#ifdef INET6
	struct sockaddr_storage frm;
#else
	struct sockaddr_in frm;
#endif /* INET6 */
	int ret = 1;

	if (!ifc) /* no outgoing interface set */
		return 1;

	while (ifc) {
		if (ifc->is_ipv6 != acl->is_ipv6) {
			/* check if we have a matching address family */
			ifc = ifc->next;
			continue;
		}

		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: bind() %s to %s socket",
			ifc->ip_address_spec, tcp? "tcp":"udp"));
		ret = 0;
		frm_len = xfrd_acl_sockaddr_frm(ifc, &frm);

		if (tcp) {
#ifdef SO_REUSEADDR
			if (setsockopt(sockd, SOL_SOCKET, SO_REUSEADDR, &frm,
				frm_len) < 0) {
				VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt "
			     "SO_REUSEADDR failed: %s", strerror(errno)));
			}
#else
			VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt SO_REUSEADDR "
			     "failed: SO_REUSEADDR not defined"));
#endif /* SO_REUSEADDR */

			if (ifc->port != 0) {
#ifdef SO_LINGER
				if (setsockopt(sockd, SOL_SOCKET, SO_LINGER,
					&linger, sizeof(linger)) < 0) {
					VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt "
				     "SO_LINGER failed: %s", strerror(errno)));
				}
#else
				VERBOSITY(2, (LOG_WARNING, "xfrd: setsockopt SO_LINGER "
					"failed: SO_LINGER not defined"));
#endif /* SO_LINGER */
			}
		}

		/* found one */
		if(bind(sockd, (struct sockaddr*)&frm, frm_len) >= 0) {
			DEBUG(DEBUG_XFRD,2, (LOG_INFO, "xfrd: bind() %s to %s "
						       "socket was successful",
			ifc->ip_address_spec, tcp? "tcp":"udp"));
			return 1;
		}

		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "xfrd: bind() %s to %s socket"
					       "failed: %s",
			ifc->ip_address_spec, tcp? "tcp":"udp",
			strerror(errno)));

		log_msg(LOG_WARNING, "xfrd: could not bind source address:port to "
		     "socket: %s", strerror(errno));
		/* try another */
		ifc = ifc->next;
	}
	return ret;
}

void
xfrd_tsig_sign_request(buffer_type* packet, tsig_record_type* tsig,
	acl_options_t* acl)
{
	tsig_algorithm_type* algo;
	assert(acl->key_options && acl->key_options->tsig_key);
	algo = tsig_get_algorithm_by_name(acl->key_options->algorithm);
	if(!algo) {
		log_msg(LOG_ERR, "tsig unknown algorithm %s",
			acl->key_options->algorithm);
		return;
	}
	assert(algo);
	tsig_init_record(tsig, algo, acl->key_options->tsig_key);
	tsig_init_query(tsig, ID(packet));
	tsig_prepare(tsig);
	tsig_update(tsig, packet, buffer_position(packet));
	tsig_sign(tsig);
	tsig_append_rr(tsig, packet);
	ARCOUNT_SET(packet, ARCOUNT(packet) + 1);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "appending tsig to packet"));
	/* prepare for validating tsigs */
	tsig_prepare(tsig);
}

static int
xfrd_send_ixfr_request_udp(xfrd_zone_t* zone)
{
	int fd;

	/* make sure we have a master to query the ixfr request to */
	assert(zone->master);

	if(zone->tcp_conn != -1) {
		/* tcp is using the zone_handler.fd */
		log_msg(LOG_ERR, "xfrd: %s tried to send udp whilst tcp engaged",
			zone->apex_str);
		return -1;
	}
	xfrd_setup_packet(xfrd->packet, TYPE_IXFR, CLASS_IN, zone->apex,
		qid_generate());
	zone->query_id = ID(xfrd->packet);
	/* delete old xfr file? */
	if(zone->msg_seq_nr)
		xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
	zone->msg_seq_nr = 0;
	zone->msg_rr_count = 0;
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "sent query with ID %d", zone->query_id));
        NSCOUNT_SET(xfrd->packet, 1);
	xfrd_write_soa_buffer(xfrd->packet, zone->apex, &zone->soa_disk);
	/* if we have tsig keys, sign the ixfr query */
	if(zone->master->key_options && zone->master->key_options->tsig_key) {
		xfrd_tsig_sign_request(xfrd->packet, &zone->tsig, zone->master);
	}
	buffer_flip(xfrd->packet);
	xfrd_set_timer(zone, XFRD_UDP_TIMEOUT);

	if((fd = xfrd_send_udp(zone->master, xfrd->packet,
		zone->zone_options->pattern->outgoing_interface)) == -1)
		return -1;

	DEBUG(DEBUG_XFRD,1, (LOG_INFO,
		"xfrd sent udp request for ixfr=%u for zone %s to %s",
		(unsigned)ntohl(zone->soa_disk.serial),
		zone->apex_str, zone->master->ip_address_spec));
	return fd;
}

static int xfrd_parse_soa_info(buffer_type* packet, xfrd_soa_t* soa)
{
	if(!buffer_available(packet, 10))
		return 0;
	soa->type = htons(buffer_read_u16(packet));
	soa->klass = htons(buffer_read_u16(packet));
	soa->ttl = htonl(buffer_read_u32(packet));
	if(ntohs(soa->type) != TYPE_SOA || ntohs(soa->klass) != CLASS_IN)
	{
		return 0;
	}

	if(!buffer_available(packet, buffer_read_u16(packet)) /* rdata length */ ||
		!(soa->prim_ns[0] = dname_make_wire_from_packet(soa->prim_ns+1, packet, 1)) ||
		!(soa->email[0] = dname_make_wire_from_packet(soa->email+1, packet, 1)))
	{
		return 0;
	}
	soa->rdata_count = 7; /* rdata in SOA */
	soa->serial = htonl(buffer_read_u32(packet));
	soa->refresh = htonl(buffer_read_u32(packet));
	soa->retry = htonl(buffer_read_u32(packet));
	soa->expire = htonl(buffer_read_u32(packet));
	soa->minimum = htonl(buffer_read_u32(packet));

	return 1;
}


/*
 * Check the RRs in an IXFR/AXFR reply.
 * returns 0 on error, 1 on correct parseable packet.
 * done = 1 if the last SOA in an IXFR/AXFR has been seen.
 * soa then contains that soa info.
 * (soa contents is modified by the routine)
 */
static int
xfrd_xfr_check_rrs(xfrd_zone_t* zone, buffer_type* packet, size_t count,
	int *done, xfrd_soa_t* soa, region_type* temp)
{
	/* first RR has already been checked */
	uint32_t tmp_serial = 0;
	uint16_t type, rrlen;
	size_t i, soapos, mempos;
	const dname_type* dname;
	domain_table_type* owners;
	rdata_atom_type* rdatas;

	for(i=0; i<count; ++i,++zone->msg_rr_count)
	{
		if (*done) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr has "
				"trailing garbage", zone->apex_str));
			return 0;
		}
		region_free_all(temp);
		owners = domain_table_create(temp);
		/* check the dname for errors */
		dname = dname_make_from_packet(temp, packet, 1, 1);
		if(!dname) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr unable "
				"to parse owner name", zone->apex_str));
			return 0;
		}
		if(!buffer_available(packet, 10)) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr hdr "
				"too small", zone->apex_str));
			return 0;
		}
		soapos = buffer_position(packet);
		type = buffer_read_u16(packet);
		(void)buffer_read_u16(packet); /* class */
		(void)buffer_read_u32(packet); /* ttl */
		rrlen = buffer_read_u16(packet);
		if(!buffer_available(packet, rrlen)) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr pkt "
				"too small", zone->apex_str));
			return 0;
		}
		mempos = buffer_position(packet);
		if(rdata_wireformat_to_rdata_atoms(temp, owners, type, rrlen,
			packet, &rdatas) == -1) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr unable "
				"to parse rdata", zone->apex_str));
			return 0;
		}
		if(type == TYPE_SOA) {
			/* check the SOAs */
			buffer_set_position(packet, soapos);
			if(!xfrd_parse_soa_info(packet, soa)) {
				DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr "
					"unable to parse soainfo", zone->apex_str));
				return 0;
			}
			if(zone->msg_rr_count == 1 &&
				ntohl(soa->serial) != zone->msg_new_serial) {
				/* 2nd RR is SOA with lower serial, this is an IXFR */
				zone->msg_is_ixfr = 1;
				if(!zone->soa_disk_acquired) {
					DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr "
						"got ixfr but need axfr", zone->apex_str));
					return 0; /* got IXFR but need AXFR */
				}
				if(ntohl(soa->serial) != ntohl(zone->soa_disk.serial)) {
					DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr "
						"bad start serial", zone->apex_str));
					return 0; /* bad start serial in IXFR */
				}
				zone->msg_old_serial = ntohl(soa->serial);
				tmp_serial = ntohl(soa->serial);
			}
			else if(ntohl(soa->serial) == zone->msg_new_serial) {
				/* saw another SOA of new serial. */
				if(zone->msg_is_ixfr == 1) {
					zone->msg_is_ixfr = 2; /* seen middle SOA in ixfr */
				} else {
					/* 2nd SOA for AXFR or 3rd newSOA for IXFR */
					*done = 1;
				}
			}
			else if (zone->msg_is_ixfr) {
				/* some additional checks */
				if(ntohl(soa->serial) > zone->msg_new_serial) {
					DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr "
						"bad middle serial", zone->apex_str));
					return 0; /* bad middle serial in IXFR */
				}
				if(ntohl(soa->serial) < tmp_serial) {
					DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s xfr "
						"serial decreasing not allowed", zone->apex_str));
					return 0; /* middle serial decreases in IXFR */
				}
				/* serial ok, update tmp serial */
				tmp_serial = ntohl(soa->serial);
			}
		}
		buffer_set_position(packet, mempos);
		buffer_skip(packet, rrlen);
	}
	/* packet seems to have a valid DNS RR structure */
	return 1;
}

static int
xfrd_xfr_process_tsig(xfrd_zone_t* zone, buffer_type* packet)
{
	int have_tsig = 0;
	assert(zone && zone->master && zone->master->key_options
		&& zone->master->key_options->tsig_key && packet);
	if(!tsig_find_rr(&zone->tsig, packet)) {
		log_msg(LOG_ERR, "xfrd: zone %s, from %s: malformed tsig RR",
			zone->apex_str, zone->master->ip_address_spec);
		return 0;
	}
	if(zone->tsig.status == TSIG_OK) {
		have_tsig = 1;
		if (zone->tsig.error_code != TSIG_ERROR_NOERROR) {
			log_msg(LOG_ERR, "xfrd: zone %s, from %s: tsig error "
				"(%s)", zone->apex_str,
				zone->master->ip_address_spec,
				tsig_error(zone->tsig.error_code));
		}
	}
	if(have_tsig) {
		/* strip the TSIG resource record off... */
		buffer_set_limit(packet, zone->tsig.position);
		ARCOUNT_SET(packet, ARCOUNT(packet) - 1);
	}

	/* keep running the TSIG hash */
	tsig_update(&zone->tsig, packet, buffer_limit(packet));
	if(have_tsig) {
		if (!tsig_verify(&zone->tsig)) {
			log_msg(LOG_ERR, "xfrd: zone %s, from %s: bad tsig signature",
				zone->apex_str, zone->master->ip_address_spec);
			return 0;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s, from %s: good tsig signature",
			zone->apex_str, zone->master->ip_address_spec));
		/* prepare for next tsigs */
		tsig_prepare(&zone->tsig);
	}
	else if(zone->tsig.updates_since_last_prepare > XFRD_TSIG_MAX_UNSIGNED) {
		/* we allow a number of non-tsig signed packets */
		log_msg(LOG_INFO, "xfrd: zone %s, from %s: too many consecutive "
			"packets without TSIG", zone->apex_str,
			zone->master->ip_address_spec);
		return 0;
	}

	if(!have_tsig && zone->msg_seq_nr == 0) {
		log_msg(LOG_ERR, "xfrd: zone %s, from %s: no tsig in first packet of reply",
			zone->apex_str, zone->master->ip_address_spec);
		return 0;
	}
	return 1;
}

/* parse the received packet. returns xfrd packet result code. */
static enum xfrd_packet_result
xfrd_parse_received_xfr_packet(xfrd_zone_t* zone, buffer_type* packet,
	xfrd_soa_t* soa)
{
	size_t rr_count;
	size_t qdcount = QDCOUNT(packet);
	size_t ancount = ANCOUNT(packet), ancount_todo;
	size_t nscount = NSCOUNT(packet);
	int done = 0;
	region_type* tempregion = NULL;

	/* has to be axfr / ixfr reply */
	if(!buffer_available(packet, QHEADERSZ)) {
		log_msg(LOG_INFO, "packet too small");
		return xfrd_packet_bad;
	}

	/* only check ID in first response message. Could also check that
	 * AA bit and QR bit are set, but not needed.
	 */
	DEBUG(DEBUG_XFRD,2, (LOG_INFO,
		"got query with ID %d and %d needed", ID(packet), zone->query_id));
	if(ID(packet) != zone->query_id) {
		log_msg(LOG_ERR, "xfrd: zone %s received bad query id from %s, "
				 "dropped",
			zone->apex_str, zone->master->ip_address_spec);
		return xfrd_packet_bad;
	}
	/* check RCODE in all response messages */
	if(RCODE(packet) != RCODE_OK) {
		log_msg(LOG_ERR, "xfrd: zone %s received error code %s from "
				 "%s",
			zone->apex_str, rcode2str(RCODE(packet)),
			zone->master->ip_address_spec);
		if (RCODE(packet) == RCODE_IMPL ||
			RCODE(packet) == RCODE_FORMAT) {
			return xfrd_packet_notimpl;
		}
		if (RCODE(packet) != RCODE_NOTAUTH) {
			/* RFC 2845: If NOTAUTH, client should do TSIG checking */
			return xfrd_packet_drop;
		}
	}
	/* check TSIG */
	if(zone->master->key_options) {
		if(!xfrd_xfr_process_tsig(zone, packet)) {
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "dropping xfr reply due "
				"to bad TSIG"));
			return xfrd_packet_bad;
		}
	}
	if (RCODE(packet) == RCODE_NOTAUTH) {
		return xfrd_packet_drop;
	}

	buffer_skip(packet, QHEADERSZ);

	/* skip question section */
	for(rr_count = 0; rr_count < qdcount; ++rr_count) {
		if (!packet_skip_rr(packet, 1)) {
			log_msg(LOG_ERR, "xfrd: zone %s, from %s: bad RR in "
					 		 "question section",
				zone->apex_str, zone->master->ip_address_spec);
			return xfrd_packet_bad;
		}
	}
	if(zone->msg_rr_count == 0 && ancount == 0) {
		if(zone->tcp_conn == -1 && TC(packet)) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: TC flagged"));
			return xfrd_packet_tcp;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: too short xfr packet: no "
					       			   "answer"));
		/* if IXFR is unknown, fallback to AXFR (if allowed) */
		if (nscount == 1) {
			if(!packet_skip_dname(packet) || !xfrd_parse_soa_info(packet, soa)) {
				DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s, from %s: "
					"no SOA begins authority section",
					zone->apex_str, zone->master->ip_address_spec));
				return xfrd_packet_bad;
			}
			return xfrd_packet_notimpl;
		}
		return xfrd_packet_bad;
	}
	ancount_todo = ancount;

	tempregion = region_create(xalloc, free);
	if(zone->msg_rr_count == 0) {
		const dname_type* soaname = dname_make_from_packet(tempregion,
			packet, 1, 1);
		if(!soaname) { /* parse failure */
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s, from %s: "
				"parse error in SOA record",
				zone->apex_str, zone->master->ip_address_spec));
			region_destroy(tempregion);
			return xfrd_packet_bad;
		}
		if(dname_compare(soaname, zone->apex) != 0) { /* wrong name */
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s, from %s: "
				"wrong SOA record",
				zone->apex_str, zone->master->ip_address_spec));
			region_destroy(tempregion);
			return xfrd_packet_bad;
		}

		/* parse the first RR, see if it is a SOA */
		if(!xfrd_parse_soa_info(packet, soa))
		{
			DEBUG(DEBUG_XFRD,1, (LOG_ERR, "xfrd: zone %s, from %s: "
						      "bad SOA rdata",
				zone->apex_str, zone->master->ip_address_spec));
			region_destroy(tempregion);
			return xfrd_packet_bad;
		}
		if(zone->soa_disk_acquired != 0 &&
			zone->state != xfrd_zone_expired /* if expired - accept anything */ &&
			compare_serial(ntohl(soa->serial), ntohl(zone->soa_disk.serial)) < 0) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd: zone %s ignoring old serial from %s",
				zone->apex_str, zone->master->ip_address_spec));
			VERBOSITY(1, (LOG_INFO,
				"xfrd: zone %s ignoring old serial from %s",
				zone->apex_str, zone->master->ip_address_spec));
			region_destroy(tempregion);
			return xfrd_packet_bad;
		}
		if(zone->soa_disk_acquired != 0 && zone->soa_disk.serial == soa->serial) {
			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s got "
						       "update indicating "
						       "current serial",
				zone->apex_str));
			/* (even if notified) the lease on the current soa is renewed */
			zone->soa_disk_acquired = xfrd_time();
			if(zone->soa_nsd.serial == soa->serial)
				zone->soa_nsd_acquired = xfrd_time();
			xfrd_set_zone_state(zone, xfrd_zone_ok);
 			DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s is ok",
				zone->apex_str));
			if(zone->soa_notified_acquired == 0) {
				/* not notified or anything, so stop asking around */
				zone->round_num = -1; /* next try start a new round */
				xfrd_set_timer_refresh(zone);
				region_destroy(tempregion);
				return xfrd_packet_newlease;
			}
			/* try next master */
			region_destroy(tempregion);
			return xfrd_packet_drop;
		}
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "IXFR reply has ok serial (have \
%u, reply %u).", (unsigned)ntohl(zone->soa_disk.serial), (unsigned)ntohl(soa->serial)));
		/* serial is newer than soa_disk */
		if(ancount == 1) {
			/* single record means it is like a notify */
			(void)xfrd_handle_incoming_notify(zone, soa);
		}
		else if(zone->soa_notified_acquired && zone->soa_notified.serial &&
			compare_serial(ntohl(zone->soa_notified.serial), ntohl(soa->serial)) < 0) {
			/* this AXFR/IXFR notifies me that an even newer serial exists */
			zone->soa_notified.serial = soa->serial;
		}
		zone->msg_new_serial = ntohl(soa->serial);
		zone->msg_rr_count = 1;
		zone->msg_is_ixfr = 0;
		if(zone->soa_disk_acquired)
			zone->msg_old_serial = ntohl(zone->soa_disk.serial);
		else zone->msg_old_serial = 0;
		ancount_todo = ancount - 1;
	}

	if(zone->tcp_conn == -1 && TC(packet)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s received TC from %s. retry tcp.",
			zone->apex_str, zone->master->ip_address_spec));
		region_destroy(tempregion);
		return xfrd_packet_tcp;
	}

	if(zone->tcp_conn == -1 && ancount < 2) {
		/* too short to be a real ixfr/axfr data transfer: need at */
		/* least two RRs in the answer section. */
		/* The serial is newer, so try tcp to this master. */
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: udp reply is short. Try "
					       			   "tcp anyway."));
		region_destroy(tempregion);
		return xfrd_packet_tcp;
	}

	if(!xfrd_xfr_check_rrs(zone, packet, ancount_todo, &done, soa,
		tempregion))
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s sent bad xfr "
					       			   "reply.", zone->apex_str));
		region_destroy(tempregion);
		return xfrd_packet_bad;
	}
	region_destroy(tempregion);
	if(zone->tcp_conn == -1 && done == 0) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: udp reply incomplete"));
		return xfrd_packet_bad;
	}
	if(done == 0)
		return xfrd_packet_more;
	if(zone->master->key_options) {
		if(zone->tsig.updates_since_last_prepare != 0) {
			log_msg(LOG_INFO, "xfrd: last packet of reply has no "
					 		  "TSIG");
			return xfrd_packet_bad;
		}
	}
	return xfrd_packet_transfer;
}

const char*
xfrd_pretty_time(time_t v)
{
	struct tm* tm = localtime(&v);
	static char buf[64];
	if(!strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tm))
		snprintf(buf, sizeof(buf), "strftime-err-%u", (unsigned)v);
	return buf;
}

enum xfrd_packet_result
xfrd_handle_received_xfr_packet(xfrd_zone_t* zone, buffer_type* packet)
{
	xfrd_soa_t soa;
	enum xfrd_packet_result res;
        uint64_t xfrfile_size;

	/* parse and check the packet - see if it ends the xfr */
	switch((res=xfrd_parse_received_xfr_packet(zone, packet, &soa)))
	{
		case xfrd_packet_more:
		case xfrd_packet_transfer:
			/* continue with commit */
			break;
		case xfrd_packet_newlease:
			return xfrd_packet_newlease;
		case xfrd_packet_tcp:
			return xfrd_packet_tcp;
		case xfrd_packet_notimpl:
		case xfrd_packet_bad:
		case xfrd_packet_drop:
		default:
		{
			/* rollback */
			if(zone->msg_seq_nr > 0) {
				/* do not process xfr - if only one part simply ignore it. */
				/* delete file with previous parts of commit */
				xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
				VERBOSITY(1, (LOG_INFO, "xfrd: zone %s "
					"reverted transfer %u from %s",
					zone->apex_str, zone->msg_rr_count?
					(int)zone->msg_new_serial:0,
					zone->master->ip_address_spec));
				zone->msg_seq_nr = 0;
			} else if (res == xfrd_packet_bad) {
				VERBOSITY(1, (LOG_INFO, "xfrd: zone %s "
					"bad transfer %u from %s",
					zone->apex_str, zone->msg_rr_count?
					(int)zone->msg_new_serial:0,
					zone->master->ip_address_spec));
			}
			if (res == xfrd_packet_notimpl)
				return res;
			else
				return xfrd_packet_bad;
		}
	}

	/* dump reply on disk to diff file */
	/* if first part, get new filenumber.  Numbers can wrap around, 64bit
	 * is enough so we do not collide with older-transfers-in-progress */
	if(zone->msg_seq_nr == 0)
		zone->xfrfilenumber = xfrd->xfrfilenumber++;
	diff_write_packet(dname_to_string(zone->apex,0),
		zone->zone_options->pattern->pname,
		zone->msg_old_serial, zone->msg_new_serial, zone->msg_seq_nr,
		buffer_begin(packet), buffer_limit(packet), xfrd->nsd,
		zone->xfrfilenumber);
	VERBOSITY(3, (LOG_INFO,
		"xfrd: zone %s written received XFR packet from %s with serial %u to "
		"disk", zone->apex_str, zone->master->ip_address_spec,
		(int)zone->msg_new_serial));
	zone->msg_seq_nr++;

        xfrfile_size = xfrd_get_xfrfile_size(xfrd->nsd, zone->xfrfilenumber);
	if( zone->zone_options->pattern->size_limit_xfr != 0 &&
	    xfrfile_size > zone->zone_options->pattern->size_limit_xfr ) {
            /*	    xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
                    xfrd_set_reload_timeout(); */
            log_msg(LOG_INFO, "xfrd : transfered zone data was too large %llu", (long long unsigned)xfrfile_size);
	    return xfrd_packet_bad;
	}
	if(res == xfrd_packet_more) {
		/* wait for more */
		return xfrd_packet_more;
	}

	/* done. we are completely sure of this */
	buffer_clear(packet);
	buffer_printf(packet, "received update to serial %u at %s from %s",
		(unsigned)zone->msg_new_serial, xfrd_pretty_time(xfrd_time()),
		zone->master->ip_address_spec);
	if(zone->master->key_options) {
		buffer_printf(packet, " TSIG verified with key %s",
			zone->master->key_options->name);
	}
	buffer_flip(packet);
	diff_write_commit(zone->apex_str, zone->msg_old_serial,
		zone->msg_new_serial, zone->msg_seq_nr, 1,
		(char*)buffer_begin(packet), xfrd->nsd, zone->xfrfilenumber);
	VERBOSITY(1, (LOG_INFO, "xfrd: zone %s committed \"%s\"",
		zone->apex_str, (char*)buffer_begin(packet)));
	/* reset msg seq nr, so if that is nonnull we know xfr file exists */
	zone->msg_seq_nr = 0;
	/* now put apply_xfr task on the tasklist */
	if(!task_new_apply_xfr(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, zone->apex, zone->msg_old_serial,
		zone->msg_new_serial, zone->xfrfilenumber)) {
		/* delete the file and pretend transfer was bad to continue */
		xfrd_unlink_xfrfile(xfrd->nsd, zone->xfrfilenumber);
		xfrd_set_reload_timeout();
		return xfrd_packet_bad;
	}
	/* update the disk serial no. */
	zone->soa_disk_acquired = xfrd_time();
	zone->soa_disk = soa;
	if(zone->soa_notified_acquired && (
		zone->soa_notified.serial == 0 ||
		compare_serial(htonl(zone->soa_disk.serial),
		htonl(zone->soa_notified.serial)) >= 0))
	{
		zone->soa_notified_acquired = 0;
	}
	if(!zone->soa_notified_acquired) {
		/* do not set expired zone to ok:
		 * it would cause nsd to start answering
		 * bad data, since the zone is not loaded yet.
		 * if nsd does not reload < retry time, more
		 * queries (for even newer versions) are made.
		 * For expired zone after reload it is set ok (SOAINFO ipc). */
		if(zone->state != xfrd_zone_expired)
			xfrd_set_zone_state(zone, xfrd_zone_ok);
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s is waiting for reload",
			zone->apex_str));
		zone->round_num = -1; /* next try start anew */
		xfrd_set_timer_refresh(zone);
		xfrd_set_reload_timeout();
		return xfrd_packet_transfer;
	} else {
		/* try to get an even newer serial */
		/* pretend it was bad to continue queries */
		xfrd_set_reload_timeout();
		return xfrd_packet_bad;
	}
}

static void
xfrd_set_reload_timeout()
{
	if(xfrd->nsd->options->xfrd_reload_timeout == -1)
		return; /* automatic reload disabled. */
	if(xfrd->reload_timeout.tv_sec == 0 ||
		xfrd_time() >= (time_t)xfrd->reload_timeout.tv_sec ) {
		/* no reload wait period (or it passed), do it right away */
		xfrd_set_reload_now(xfrd);
		/* start reload wait period */
		xfrd->reload_timeout.tv_sec = xfrd_time() +
			xfrd->nsd->options->xfrd_reload_timeout;
		xfrd->reload_timeout.tv_usec = 0;
		return;
	}
	/* cannot reload now, set that after the timeout a reload has to happen */
	if(xfrd->reload_added == 0) {
		struct timeval tv;
		tv.tv_sec = xfrd->reload_timeout.tv_sec - xfrd_time();
		tv.tv_usec = 0;
		if(tv.tv_sec > xfrd->nsd->options->xfrd_reload_timeout)
			tv.tv_sec = xfrd->nsd->options->xfrd_reload_timeout;
		event_set(&xfrd->reload_handler, -1, EV_TIMEOUT,
			xfrd_handle_reload, xfrd);
		if(event_base_set(xfrd->event_base, &xfrd->reload_handler) != 0)
			log_msg(LOG_ERR, "cannot set reload event base");
		if(event_add(&xfrd->reload_handler, &tv) != 0)
			log_msg(LOG_ERR, "cannot add reload event");
		xfrd->reload_added = 1;
	}
}

static void
xfrd_handle_reload(int ATTR_UNUSED(fd), short event, void* ATTR_UNUSED(arg))
{
	/* reload timeout */
	assert(event & EV_TIMEOUT);
	(void)event;
	/* timeout wait period after this request is sent */
	xfrd->reload_added = 0;
	xfrd->reload_timeout.tv_sec = xfrd_time() +
		xfrd->nsd->options->xfrd_reload_timeout;
	xfrd_set_reload_now(xfrd);
}

void
xfrd_handle_notify_and_start_xfr(xfrd_zone_t* zone, xfrd_soa_t* soa)
{
	if(xfrd_handle_incoming_notify(zone, soa)) {
		if(zone->zone_handler.ev_fd == -1 && zone->tcp_conn == -1 &&
			!zone->tcp_waiting && !zone->udp_waiting) {
			xfrd_set_refresh_now(zone);
		}
		/* zones with no content start expbackoff again; this is also
		 * for nsd-control started transfer commands, and also when
		 * the master apparently sends notifies (is back up) */
		if(zone->soa_disk_acquired == 0)
			zone->fresh_xfr_timeout = XFRD_TRANSFER_TIMEOUT_START;
	}
}

void
xfrd_handle_passed_packet(buffer_type* packet,
	int acl_num, int acl_num_xfr)
{
	uint8_t qnamebuf[MAXDOMAINLEN];
	uint16_t qtype, qclass;
	const dname_type* dname;
	region_type* tempregion = region_create(xalloc, free);
	xfrd_zone_t* zone;

	buffer_skip(packet, QHEADERSZ);
	if(!packet_read_query_section(packet, qnamebuf, &qtype, &qclass)) {
		region_destroy(tempregion);
		return; /* drop bad packet */
	}

	dname = dname_make(tempregion, qnamebuf, 1);
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: got passed packet for %s, acl "
		   "%d", dname_to_string(dname,0), acl_num));

	/* find the zone */
	zone = (xfrd_zone_t*)rbtree_search(xfrd->zones, dname);
	if(!zone) {
		/* this could be because the zone has been deleted meanwhile */
		DEBUG(DEBUG_XFRD, 1, (LOG_INFO, "xfrd: incoming packet for "
			"unknown zone %s", dname_to_string(dname,0)));
		region_destroy(tempregion);
		return; /* drop packet for unknown zone */
	}
	region_destroy(tempregion);

	/* handle */
	if(OPCODE(packet) == OPCODE_NOTIFY) {
		xfrd_soa_t soa;
		int have_soa = 0;
		int next;
		/* get serial from a SOA */
		if(ANCOUNT(packet) == 1 && packet_skip_dname(packet) &&
			xfrd_parse_soa_info(packet, &soa)) {
				have_soa = 1;
		}
		xfrd_handle_notify_and_start_xfr(zone, have_soa?&soa:NULL);
		/* First, see if our notifier has a match in provide-xfr */
		if (acl_find_num(zone->zone_options->pattern->request_xfr,
				acl_num_xfr))
			next = acl_num_xfr;
		else /* If not, find master that matches notifiers ACL entry */
			next = find_same_master_notify(zone, acl_num);
		if(next != -1) {
			zone->next_master = next;
			DEBUG(DEBUG_XFRD,1, (LOG_INFO,
				"xfrd: notify set next master to query %d",
				next));
		}
	}
	else {
		/* ignore other types of messages */
	}
}

static int
xfrd_handle_incoming_notify(xfrd_zone_t* zone, xfrd_soa_t* soa)
{
	if(soa && zone->soa_disk_acquired && zone->state != xfrd_zone_expired &&
	   compare_serial(ntohl(soa->serial),ntohl(zone->soa_disk.serial)) <= 0)
	{
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: ignored notify %s %u old serial, zone valid "
			"(soa disk serial %u)", zone->apex_str,
			(unsigned)ntohl(soa->serial),
			(unsigned)ntohl(zone->soa_disk.serial)));
		return 0; /* ignore notify with old serial, we have a valid zone */
	}
	if(soa == 0) {
		zone->soa_notified.serial = 0;
	}
	else if (zone->soa_notified_acquired == 0 ||
		 zone->soa_notified.serial == 0 ||
		 compare_serial(ntohl(soa->serial),
			ntohl(zone->soa_notified.serial)) > 0)
	{
		zone->soa_notified = *soa;
	}
	zone->soa_notified_acquired = xfrd_time();
	if(zone->state == xfrd_zone_ok) {
		xfrd_set_zone_state(zone, xfrd_zone_refreshing);
	}
	/* transfer right away */
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "Handle incoming notify for zone %s",
		zone->apex_str));
	return 1;
}

static int
find_same_master_notify(xfrd_zone_t* zone, int acl_num_nfy)
{
	acl_options_t* nfy_acl = acl_find_num(zone->zone_options->pattern->
		allow_notify, acl_num_nfy);
	int num = 0;
	acl_options_t* master = zone->zone_options->pattern->request_xfr;
	if(!nfy_acl)
		return -1;
	while(master)
	{
		if(acl_addr_matches_host(nfy_acl, master))
			return num;
		master = master->next;
		num++;
	}
	return -1;
}

void
xfrd_check_failed_updates()
{
	/* see if updates have not come through */
	xfrd_zone_t* zone;
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		/* zone has a disk soa, and no nsd soa or a different nsd soa */
		if(zone->soa_disk_acquired != 0 &&
			(zone->soa_nsd_acquired == 0 ||
			zone->soa_disk.serial != zone->soa_nsd.serial))
		{
			if(zone->soa_disk_acquired <
				xfrd->reload_cmd_last_sent)
			{
				/* this zone should have been loaded, since its disk
				   soa time is before the time of the reload cmd. */
				xfrd_soa_t dumped_soa = zone->soa_disk;
				log_msg(LOG_ERR, "xfrd: zone %s: soa serial %u "
						 		 "update failed, restarting "
						 		 "transfer (notified zone)",
					zone->apex_str, (unsigned)ntohl(zone->soa_disk.serial));
				/* revert the soa; it has not been acquired properly */
				zone->soa_disk_acquired = zone->soa_nsd_acquired;
				zone->soa_disk = zone->soa_nsd;
				/* pretend we are notified with disk soa.
				   This will cause a refetch of the data, and reload. */
				xfrd_handle_incoming_notify(zone, &dumped_soa);
				xfrd_set_timer_refresh(zone);
			} else if(zone->soa_disk_acquired >= xfrd->reload_cmd_last_sent) {
				/* this zone still has to be loaded,
				   make sure reload is set to be sent. */
				if(xfrd->need_to_send_reload == 0 &&
					xfrd->reload_added == 0) {
					log_msg(LOG_ERR, "xfrd: zone %s: needs "
									 "to be loaded. reload lost? "
									 "try again", zone->apex_str);
					xfrd_set_reload_timeout();
				}
			}
		}
	}
}

void
xfrd_prepare_zones_for_reload()
{
	xfrd_zone_t* zone;
	RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones)
	{
		/* zone has a disk soa, and no nsd soa or a different nsd soa */
		if(zone->soa_disk_acquired != 0 &&
			(zone->soa_nsd_acquired == 0 ||
			zone->soa_disk.serial != zone->soa_nsd.serial))
		{
			if(zone->soa_disk_acquired == xfrd_time()) {
				/* antedate by one second.
				 * this makes sure that the zone time is before
				 * reload, so that check_failed_zones() is
				 * certain of the result.
				 */
				zone->soa_disk_acquired--;
			}
		}
	}
}

struct buffer*
xfrd_get_temp_buffer()
{
	return xfrd->packet;
}

#ifdef BIND8_STATS
/** process stat info task */
static void
xfrd_process_stat_info_task(xfrd_state_t* xfrd, struct task_list_d* task)
{
	size_t i;
	stc_t* p = (void*)task->zname + sizeof(struct nsdst);
	stats_add(&xfrd->nsd->st, (struct nsdst*)task->zname);
	for(i=0; i<xfrd->nsd->child_count; i++) {
		xfrd->nsd->children[i].query_count += *p++;
	}
	/* got total, now see if users are interested in these statistics */
#ifdef HAVE_SSL
	daemon_remote_process_stats(xfrd->nsd->rc);
#endif
}
#endif /* BIND8_STATS */

#ifdef USE_ZONE_STATS
/** process zonestat inc task */
static void
xfrd_process_zonestat_inc_task(xfrd_state_t* xfrd, struct task_list_d* task)
{
	xfrd->zonestat_safe = (unsigned)task->oldserial;
	zonestat_remap(xfrd->nsd, 0, xfrd->zonestat_safe*sizeof(struct nsdst));
	xfrd->nsd->zonestatsize[0] = xfrd->zonestat_safe;
	zonestat_remap(xfrd->nsd, 1, xfrd->zonestat_safe*sizeof(struct nsdst));
	xfrd->nsd->zonestatsize[1] = xfrd->zonestat_safe;
}
#endif /* USE_ZONE_STATS */

static void
xfrd_handle_taskresult(xfrd_state_t* xfrd, struct task_list_d* task)
{
#ifndef BIND8_STATS
	(void)xfrd;
#endif
	switch(task->task_type) {
	case task_soa_info:
		xfrd_process_soa_info_task(task);
		break;
#ifdef BIND8_STATS
	case task_stat_info:
		xfrd_process_stat_info_task(xfrd, task);
		break;
#endif /* BIND8_STATS */
#ifdef USE_ZONE_STATS
	case task_zonestat_inc:
		xfrd_process_zonestat_inc_task(xfrd, task);
		break;
#endif
	default:
		log_msg(LOG_WARNING, "unhandled task result in xfrd from "
			"reload type %d", (int)task->task_type);
	}
}

void xfrd_process_task_result(xfrd_state_t* xfrd, struct udb_base* taskudb)
{
	udb_ptr t;
	/* remap it for usage */
	task_remap(taskudb);
	/* process the task-results in the taskudb */
	udb_ptr_new(&t, taskudb, udb_base_get_userdata(taskudb));
	while(!udb_ptr_is_null(&t)) {
		xfrd_handle_taskresult(xfrd, TASKLIST(&t));
		udb_ptr_set_rptr(&t, taskudb, &TASKLIST(&t)->next);
	}
	udb_ptr_unlink(&t, taskudb);
	/* clear the udb so it can be used by xfrd to make new tasks for
	 * reload, this happens when the reload signal is sent, and thus
	 * the taskudbs are swapped */
	task_clear(taskudb);
}

void xfrd_set_reload_now(xfrd_state_t* xfrd)
{
	xfrd->need_to_send_reload = 1;
	if(!(xfrd->ipc_handler_flags&EV_WRITE)) {
		ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
	}
}

static void
xfrd_handle_write_timer(int ATTR_UNUSED(fd), short event, void* ATTR_UNUSED(arg))
{
	/* timeout for write events */
	assert(event & EV_TIMEOUT);
	(void)event;
	if(xfrd->nsd->options->zonefiles_write == 0)
		return;
	/* call reload to write changed zonefiles */
	if(!xfrd->write_zonefile_needed) {
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "zonefiles write timer (nothing)"));
		xfrd_write_timer_set();
		return;
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "zonefiles write timer"));
	task_new_write_zonefiles(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, NULL);
	xfrd_set_reload_now(xfrd);
	xfrd->write_zonefile_needed = 0;
	xfrd_write_timer_set();
}

static void xfrd_write_timer_set()
{
	struct timeval tv;
	if(xfrd->nsd->options->zonefiles_write == 0)
		return;
	tv.tv_sec = xfrd->nsd->options->zonefiles_write;
	tv.tv_usec = 0;
	event_set(&xfrd->write_timer, -1, EV_TIMEOUT,
		xfrd_handle_write_timer, xfrd);
	if(event_base_set(xfrd->event_base, &xfrd->write_timer) != 0)
		log_msg(LOG_ERR, "xfrd write timer: event_base_set failed");
	if(event_add(&xfrd->write_timer, &tv) != 0)
		log_msg(LOG_ERR, "xfrd write timer: event_add failed");
}

static void xfrd_handle_child_timer(int ATTR_UNUSED(fd), short event,
	void* ATTR_UNUSED(arg))
{
	assert(event & EV_TIMEOUT);
	(void)event;
	/* only used to wakeup the process to reap children, note the
	 * event is no longer registered */
	xfrd->child_timer_added = 0;
}
