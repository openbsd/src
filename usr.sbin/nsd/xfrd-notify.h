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

#include <config.h>
#include "tsig.h"
#include "netio.h"
#include "rbtree.h"

struct nsd;
struct region;
struct xfrd_zone;
struct zone_options;
struct zone;
struct xfrd_soa;
struct acl_options;

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
	netio_handler_type notify_send_handler;
	struct timespec notify_timeout;
	struct acl_options* notify_current; /* current slave to notify */
	uint8_t notify_retry; /* how manieth retry in sending to current */
	uint16_t notify_query_id;

	/* is this notify waiting for a socket? */
	uint8_t is_waiting;
	/* next in the waiting list for the udp sockets */
	struct notify_zone_t* waiting_next;
};

/* initialise outgoing notifies */
void init_notify_send(rbtree_t* tree, netio_type* netio, region_type* region,
        const dname_type* apex, struct zone_options* options,
	struct zone* dbzone);

/* send notifications to all in the notify list */
void xfrd_send_notify(rbtree_t* tree, const struct dname* apex,
	struct xfrd_soa* new_soa);

/* handle soa update notify for a master zone. newsoa can be NULL.
   Makes sure that the soa (serial) has changed. Or drops notify. */
void notify_handle_master_zone_soainfo(rbtree_t* tree,
	const dname_type* apex, struct xfrd_soa* new_soa);

/* close fds in use for notification sending */
void close_notify_fds(rbtree_t* tree);

#endif /* XFRD_NOTIFY_H */
