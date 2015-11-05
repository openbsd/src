/*
 * xfrd-notify.h - notify sending routines.
 *
 * Copyright (c) 2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#ifndef XFRD_NOTIFY_H
#define XFRD_NOTIFY_H

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
#include "tsig.h"
#include "rbtree.h"

struct nsd;
struct region;
struct xfrd_zone;
struct zone_options;
struct zone;
struct xfrd_soa;
struct acl_options;
struct xfrd_state;

/**
 * This struct keeps track of outbound notifies for a zone.
 */
struct notify_zone_t {
	rbnode_t node;
	/* name of the zone */
	const dname_type* apex;
	const char* apex_str;

	tsig_record_type notify_tsig; /* tsig state for notify */
	struct zone_options* options;
	struct xfrd_soa *current_soa; /* current SOA in NSD */

	/* notify sending handler */
	/* Not saved on disk (i.e. kill of daemon stops notifies) */
	int notify_send_enable;
	struct event notify_send_handler;
	struct timeval notify_timeout;
	struct acl_options* notify_current; /* current slave to notify */
	uint8_t notify_restart; /* restart notify after repattern */
	uint8_t notify_retry; /* how manieth retry in sending to current */
	uint16_t notify_query_id;

	/* is this notify waiting for a socket? */
	uint8_t is_waiting;
	/* the double linked waiting list for the udp sockets */
	struct notify_zone_t* waiting_next;
	struct notify_zone_t* waiting_prev;
};

/* initialise outgoing notifies */
void init_notify_send(rbtree_t* tree, region_type* region,
	struct zone_options* options);
/* delete notify zone */
void xfrd_del_notify(struct xfrd_state* xfrd, const dname_type* dname);

/* send notifications to all in the notify list */
void xfrd_send_notify(rbtree_t* tree, const struct dname* apex,
	struct xfrd_soa* new_soa);
/* start notifications, if not started already (does not clobber SOA) */
void xfrd_notify_start(struct notify_zone_t* zone, struct xfrd_state* xfrd);

/* handle soa update notify for a master zone. newsoa can be NULL.
   Makes sure that the soa (serial) has changed. Or drops notify. */
void notify_handle_master_zone_soainfo(rbtree_t* tree,
	const dname_type* apex, struct xfrd_soa* new_soa);

/* close fds in use for notification sending */
void close_notify_fds(rbtree_t* tree);
/* stop send of notify */
void notify_disable(struct notify_zone_t* zone);

#endif /* XFRD_NOTIFY_H */
