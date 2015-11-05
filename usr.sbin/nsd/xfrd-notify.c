/*
 * xfrd-notify.c - notify sending routines
 *
 * Copyright (c) 2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "xfrd-notify.h"
#include "xfrd.h"
#include "xfrd-tcp.h"
#include "packet.h"

#define XFRD_NOTIFY_RETRY_TIMOUT 15 /* seconds between retries sending NOTIFY */

/* start sending notifies */
static void notify_enable(struct notify_zone_t* zone,
	struct xfrd_soa* new_soa);
/* setup the notify active state */
static void setup_notify_active(struct notify_zone_t* zone);

/* returns if the notify send is done for the notify_current acl */
static int xfrd_handle_notify_reply(struct notify_zone_t* zone, buffer_type* packet);

/* handle zone notify send */
static void xfrd_handle_notify_send(int fd, short event, void* arg);

static void xfrd_notify_next(struct notify_zone_t* zone);

static void xfrd_notify_send_udp(struct notify_zone_t* zone, buffer_type* packet);

static void
notify_send_disable(struct notify_zone_t* zone)
{
	zone->notify_send_enable = 0;
	event_del(&zone->notify_send_handler);
	if(zone->notify_send_handler.ev_fd != -1) {
		close(zone->notify_send_handler.ev_fd);
	}
}

void
notify_disable(struct notify_zone_t* zone)
{
	zone->notify_current = 0;
	/* if added, then remove */
	if(zone->notify_send_enable) {
		notify_send_disable(zone);
	}

	if(xfrd->notify_udp_num == XFRD_MAX_UDP_NOTIFY) {
		/* find next waiting and needy zone */
		while(xfrd->notify_waiting_first) {
			/* snip off */
			struct notify_zone_t* wz = xfrd->notify_waiting_first;
			assert(wz->is_waiting);
			wz->is_waiting = 0;
			xfrd->notify_waiting_first = wz->waiting_next;
			if(wz->waiting_next)
				wz->waiting_next->waiting_prev = NULL;
			if(xfrd->notify_waiting_last == wz)
				xfrd->notify_waiting_last = NULL;
			/* see if this zone needs notify sending */
			if(wz->notify_current) {
				DEBUG(DEBUG_XFRD,1, (LOG_INFO,
					"xfrd: zone %s: notify off waiting list.",
					zone->apex_str)	);
				setup_notify_active(wz);
				return;
			}
		}
	}
	xfrd->notify_udp_num--;
}

void
init_notify_send(rbtree_t* tree, region_type* region, zone_options_t* options)
{
	struct notify_zone_t* not = (struct notify_zone_t*)
		region_alloc(region, sizeof(struct notify_zone_t));
	memset(not, 0, sizeof(struct notify_zone_t));
	not->apex = options->node.key;
	not->apex_str = options->name;
	not->node.key = not->apex;
	not->options = options;

	/* if master zone and have a SOA */
	not->current_soa = (struct xfrd_soa*)region_alloc(region,
		sizeof(struct xfrd_soa));
	memset(not->current_soa, 0, sizeof(struct xfrd_soa));

	not->is_waiting = 0;

	not->notify_send_enable = 0;
	tsig_create_record_custom(&not->notify_tsig, NULL, 0, 0, 4);
	not->notify_current = 0;
	rbtree_insert(tree, (rbnode_t*)not);
}

void
xfrd_del_notify(xfrd_state_t* xfrd, const dname_type* dname)
{
	/* find it */
	struct notify_zone_t* not = (struct notify_zone_t*)rbtree_delete(
		xfrd->notify_zones, dname);
	if(!not)
		return;

	/* waiting list */
	if(not->is_waiting) {
		if(not->waiting_prev)
			not->waiting_prev->waiting_next = not->waiting_next;
		else	xfrd->notify_waiting_first = not->waiting_next;
		if(not->waiting_next)
			not->waiting_next->waiting_prev = not->waiting_prev;
		else	xfrd->notify_waiting_last = not->waiting_prev;
		not->is_waiting = 0;
	}

	/* event */
	if(not->notify_send_enable) {
		notify_disable(not);
	}

	/* del tsig */
	tsig_delete_record(&not->notify_tsig, NULL);

	/* free it */
	region_recycle(xfrd->region, not->current_soa, sizeof(xfrd_soa_t));
	/* the apex is recycled when the zone_options.node.key is removed */
	region_recycle(xfrd->region, not, sizeof(*not));
}

static int
xfrd_handle_notify_reply(struct notify_zone_t* zone, buffer_type* packet)
{
	if((OPCODE(packet) != OPCODE_NOTIFY) ||
		(QR(packet) == 0)) {
		log_msg(LOG_ERR, "xfrd: zone %s: received bad notify reply opcode/flags",
			zone->apex_str);
		return 0;
	}
	/* we know it is OPCODE NOTIFY, QUERY_REPLY and for this zone */
	if(ID(packet) != zone->notify_query_id) {
		log_msg(LOG_ERR, "xfrd: zone %s: received notify-ack with bad ID",
			zone->apex_str);
		return 0;
	}
	/* could check tsig, but why. The reply does not cause processing. */
	if(RCODE(packet) != RCODE_OK) {
		log_msg(LOG_ERR, "xfrd: zone %s: received notify response error %s from %s",
			zone->apex_str, rcode2str(RCODE(packet)),
			zone->notify_current->ip_address_spec);
		if(RCODE(packet) == RCODE_IMPL)
			return 1; /* rfc1996: notimpl notify reply: consider retries done */
		return 0;
	}
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s: host %s acknowledges notify",
		zone->apex_str, zone->notify_current->ip_address_spec));
	return 1;
}

static void
xfrd_notify_next(struct notify_zone_t* zone)
{
	/* advance to next in acl */
	zone->notify_current = zone->notify_current->next;
	zone->notify_retry = 0;
	if(zone->notify_current == 0) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s: no more notify-send acls. stop notify.",
			zone->apex_str));
		notify_disable(zone);
		return;
	}
}

static void
xfrd_notify_send_udp(struct notify_zone_t* zone, buffer_type* packet)
{
	int fd;
	if(zone->notify_send_enable) {
		notify_send_disable(zone);
	}
	/* Set timeout for next reply */
	zone->notify_timeout.tv_sec = XFRD_NOTIFY_RETRY_TIMOUT;
	/* send NOTIFY to secondary. */
	xfrd_setup_packet(packet, TYPE_SOA, CLASS_IN, zone->apex,
		qid_generate());
	zone->notify_query_id = ID(packet);
	OPCODE_SET(packet, OPCODE_NOTIFY);
	AA_SET(packet);
	if(zone->current_soa->serial != 0) {
		/* add current SOA to answer section */
		ANCOUNT_SET(packet, 1);
		xfrd_write_soa_buffer(packet, zone->apex, zone->current_soa);
	}
	if(zone->notify_current->key_options) {
		xfrd_tsig_sign_request(packet, &zone->notify_tsig, zone->notify_current);
	}
	buffer_flip(packet);
	fd = xfrd_send_udp(zone->notify_current, packet,
		zone->options->pattern->outgoing_interface);
	if(fd == -1) {
		log_msg(LOG_ERR, "xfrd: zone %s: could not send notify #%d to %s",
			zone->apex_str, zone->notify_retry,
			zone->notify_current->ip_address_spec);
		event_set(&zone->notify_send_handler, -1, EV_TIMEOUT,
			xfrd_handle_notify_send, zone);
		if(event_base_set(xfrd->event_base, &zone->notify_send_handler) != 0)
			log_msg(LOG_ERR, "notify_send: event_base_set failed");
		if(evtimer_add(&zone->notify_send_handler, &zone->notify_timeout) != 0)
			log_msg(LOG_ERR, "notify_send: evtimer_add failed");
		zone->notify_send_enable = 1;
		return;
	}
	event_set(&zone->notify_send_handler, fd, EV_READ | EV_TIMEOUT,
		xfrd_handle_notify_send, zone);
	if(event_base_set(xfrd->event_base, &zone->notify_send_handler) != 0)
		log_msg(LOG_ERR, "notify_send: event_base_set failed");
	if(event_add(&zone->notify_send_handler, &zone->notify_timeout) != 0)
		log_msg(LOG_ERR, "notify_send: evtimer_add failed");
	zone->notify_send_enable = 1;
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s: sent notify #%d to %s",
		zone->apex_str, zone->notify_retry,
		zone->notify_current->ip_address_spec));
}

static void
xfrd_handle_notify_send(int fd, short event, void* arg)
{
	struct notify_zone_t* zone = (struct notify_zone_t*)arg;
	buffer_type* packet = xfrd_get_temp_buffer();
	assert(zone->notify_current);
	if(zone->is_waiting) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: notify waiting, skipped, %s", zone->apex_str));
		return;
	}
	if((event & EV_READ)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO,
			"xfrd: zone %s: read notify ACK", zone->apex_str));
		assert(fd != -1);
		if(xfrd_udp_read_packet(packet, fd)) {
			if(xfrd_handle_notify_reply(zone, packet))
				xfrd_notify_next(zone);
		}
	} else if((event & EV_TIMEOUT)) {
		DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s: notify timeout",
			zone->apex_str));
		/* timeout, try again */
	}
	/* see if notify is still enabled */
	if(zone->notify_current) {
		zone->notify_retry++;
		if(zone->notify_retry > zone->options->pattern->notify_retry) {
			log_msg(LOG_ERR, "xfrd: zone %s: max notify send count reached, %s unreachable",
				zone->apex_str, zone->notify_current->ip_address_spec);
			xfrd_notify_next(zone);
		}
	}
	if(zone->notify_current) {
		/* try again */
		xfrd_notify_send_udp(zone, packet);
	}
}

static void
setup_notify_active(struct notify_zone_t* zone)
{
	zone->notify_retry = 0;
	zone->notify_current = zone->options->pattern->notify;
	zone->notify_timeout.tv_sec = 0;
	zone->notify_timeout.tv_usec = 0;

	if(zone->notify_send_enable)
		notify_send_disable(zone);
	event_set(&zone->notify_send_handler, -1, EV_TIMEOUT,
		xfrd_handle_notify_send, zone);
	if(event_base_set(xfrd->event_base, &zone->notify_send_handler) != 0)
		log_msg(LOG_ERR, "notifysend: event_base_set failed");
	if(evtimer_add(&zone->notify_send_handler, &zone->notify_timeout) != 0)
		log_msg(LOG_ERR, "notifysend: evtimer_add failed");
	zone->notify_send_enable = 1;
}

static void
notify_enable(struct notify_zone_t* zone, struct xfrd_soa* new_soa)
{
	if(!zone->options->pattern->notify) {
		return; /* no notify acl, nothing to do */
	}

	if(new_soa == NULL)
		memset(zone->current_soa, 0, sizeof(xfrd_soa_t));
	else
		memcpy(zone->current_soa, new_soa, sizeof(xfrd_soa_t));
	if(zone->is_waiting)
		return;

	if(xfrd->notify_udp_num < XFRD_MAX_UDP_NOTIFY) {
		setup_notify_active(zone);
		xfrd->notify_udp_num++;
		return;
	}
	/* put it in waiting list */
	zone->notify_current = zone->options->pattern->notify;
	zone->is_waiting = 1;
	zone->waiting_next = NULL;
	zone->waiting_prev = xfrd->notify_waiting_last;
	if(xfrd->notify_waiting_last) {
		xfrd->notify_waiting_last->waiting_next = zone;
	} else {
		xfrd->notify_waiting_first = zone;
	}
	xfrd->notify_waiting_last = zone;
	DEBUG(DEBUG_XFRD,1, (LOG_INFO, "xfrd: zone %s: notify on waiting list.",
		zone->apex_str));
}

void
xfrd_notify_start(struct notify_zone_t* zone, struct xfrd_state* xfrd)
{
	xfrd_zone_t* xz;
	if(zone->is_waiting || zone->notify_send_enable)
		return;
	xz = (xfrd_zone_t*)rbtree_search(xfrd->zones, zone->apex);
	if(xz && xz->soa_nsd_acquired)
		notify_enable(zone, &xz->soa_nsd);
	else	notify_enable(zone, NULL);
}

void
xfrd_send_notify(rbtree_t* tree, const dname_type* apex, struct xfrd_soa* new_soa)
{
	/* lookup the zone */
	struct notify_zone_t* zone = (struct notify_zone_t*)
		rbtree_search(tree, apex);
	assert(zone);
	if(zone->notify_send_enable)
		notify_disable(zone);

	notify_enable(zone, new_soa);
}

void
notify_handle_master_zone_soainfo(rbtree_t* tree,
	const dname_type* apex, struct xfrd_soa* new_soa)
{
	/* lookup the zone */
	struct notify_zone_t* zone = (struct notify_zone_t*)
		rbtree_search(tree, apex);
	if(!zone) return; /* got SOAINFO but zone was deleted meanwhile */

	/* check if SOA changed */
	if( (new_soa == NULL && zone->current_soa->serial == 0) ||
		(new_soa && new_soa->serial == zone->current_soa->serial))
		return;
	if(zone->notify_send_enable)
		notify_disable(zone);
	notify_enable(zone, new_soa);
}

void
close_notify_fds(rbtree_t* tree)
{
	struct notify_zone_t* zone;
	RBTREE_FOR(zone, struct notify_zone_t*, tree)
	{
		if(zone->notify_send_enable)
			notify_send_disable(zone);
	}
}
