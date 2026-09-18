/*	$OpenBSD: commands.c,v 1.3 2026/09/18 04:55:39 deraadt Exp $ */
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
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"

int
m_serial_notify(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_serial_notify)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Serial notify)");
		return 1;
	}

	logx(1, "Received SERIAL_NOTIFY from %s\n",
	    s->name);

	return 0;
}

int
m_serial_query(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_serial_query *sq;
	struct cache_frame *old_frame;
	struct cache_frame *new_frame;

	struct vrp4 *vrp4;
	struct vrp6 *vrp6;
	struct brk *brk;
	struct vap *vap;
	struct vap *node;

	int initial_command;

	assert(s);
	assert(ph);

	initial_command = 0;

	sq = (struct pdu_serial_query *)ph;

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED)) {
		if (ph->version > rtr_max_version) {
			s->version = rtr_max_version;
			senderrorto_one(s, UNSUPPORTED_PROTOCOL_VERSION, ph,
			    "Protocol version %d is not supported (max %d)",
			    ph->version, rtr_max_version);
			return 1;
		}

		s->version = ph->version;
		SetSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED);
		initial_command = 1;
		logx(1, "Registering %s with version %d via SERIAL_QUERY\n",
		    s->name,
		    s->version);

		remove_sched_type_and_socket(&scheduler,
		    SCHED_TYPE_HANDSHAKE_TIMEOUT, s);
		rtr_gettime();
		schedule_idle_timeout(now.tv_sec, s);

		if (sq->session_id != cache[s->version].session_id) {
			logx(1, "Stale session id %u for %s "
			    "does not match cache session id %u\n",
			    sq->session_id,
			    s->name,
			    cache[s->version].session_id);
			sendcacheresetto_one(s);
			addstats(s->stats.serial_query_count, 1);
			return 0;
		}

	} else {
		remove_sched_type_and_socket(&scheduler,
		    SCHED_TYPE_IDLE_TIMEOUT, s);
		rtr_gettime();
		schedule_idle_timeout(now.tv_sec, s);
	}

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (sq->session_id != cache[s->version].session_id) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Illegal session id change from %u to %u",
		    cache[s->version].session_id,
		    sq->session_id);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_serial_query)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Serial query)");
		return 1;
	}

	logx(1, "Received SERIAL_QUERY with serial %u from %s\n",
	    sq->serial_number,
	    s->name);

	addstats(s->stats.serial_query_count, 1);

	new_frame = get_latest_cache_frame(s->version);

	if (new_frame == NULL) {
		senderrorto_one(s, NO_DATA_AVAILABLE, ph, "No data available");
		return 0;
	}

	if (sq->session_id != cache[s->version].session_id) {
		sendcacheresetto_one(s);
		logx(1, "Session ID mismatch (%u vs %u) from %s\n",
		    sq->session_id, cache[s->version].session_id, s->name);
		return 0;
	}

	if (sq->serial_number == new_frame->serial_number) {
		sendcacheresponseto_one(s);
		sendendofdatato_one(s, new_frame->serial_number);
		logx(1, "Sent keepalive query with serial %u to %s\n",
		    new_frame->serial_number, s->name);
		return 0;
	}

	old_frame = get_serial_cache_frame(s->version, sq->serial_number);

	if (old_frame == NULL) {
		sendcacheresetto_one(s);
		logx(1, "Couldn't find serial %u for %s\n",
		    new_frame->serial_number, s->name);
		return 0;
	}

	if (initial_command) {
		s->stats.vrp4_advertised =
		    old_frame->rtr_vrp4s->entry_count;
		s->stats.vrp6_advertised =
		    old_frame->rtr_vrp6s->entry_count;
		if (ph->version >= RTR_VERSION_1) {
			s->stats.brk_advertised =
			    old_frame->rtr_brks->entry_count;
		} else {
			s->stats.brk_advertised = 0;
		}
		if (ph->version >= RTR_VERSION_2) {
			s->stats.vap_advertised =
			    old_frame->rtr_vaps->entry_count;
		} else {
			s->stats.vap_advertised = 0;
		}
	}

	sendcacheresponseto_one(s);

	/* VRP4 */

	RB_FOREACH(vrp4, vrp4_tree, &new_frame->rtr_vrp4s->vrp4s) {
		if (!RB_FIND(vrp4_tree,
		    &old_frame->rtr_vrp4s->vrp4s, vrp4)) {
			sendvrp4to_one(s, vrp4, RTR_ANNOUNCE);
			addstats(s->stats.vrp4_announcements, 1);
			addstats(s->stats.vrp4_advertised, 1);
		}
	}

	RB_FOREACH_REVERSE(vrp4, vrp4_tree, &old_frame->rtr_vrp4s->vrp4s) {
		if (!RB_FIND(vrp4_tree,
		    &new_frame->rtr_vrp4s->vrp4s, vrp4)) {
			sendvrp4to_one(s, vrp4, RTR_WITHDRAW);
			addstats(s->stats.vrp4_withdrawals, 1);
			substats(s->stats.vrp4_advertised, 1);
		}
	}

	/* VRP6 */

	RB_FOREACH(vrp6, vrp6_tree, &new_frame->rtr_vrp6s->vrp6s) {
		if (!RB_FIND(vrp6_tree,
		    &old_frame->rtr_vrp6s->vrp6s, vrp6)) {
			sendvrp6to_one(s, vrp6, RTR_ANNOUNCE);
			addstats(s->stats.vrp6_announcements, 1);
			addstats(s->stats.vrp6_advertised, 1);
		}
	}

	RB_FOREACH_REVERSE(vrp6, vrp6_tree, &old_frame->rtr_vrp6s->vrp6s) {
		if (!RB_FIND(vrp6_tree,
		    &new_frame->rtr_vrp6s->vrp6s, vrp6)) {
			sendvrp6to_one(s, vrp6, RTR_WITHDRAW);
			addstats(s->stats.vrp6_withdrawals, 1);
			substats(s->stats.vrp6_advertised, 1);
		}
	}

	/* BRK */

	if (ph->version >= RTR_VERSION_1) {
		RB_FOREACH(brk, brk_tree, &new_frame->rtr_brks->brks) {
			if (!RB_FIND(brk_tree,
			    &old_frame->rtr_brks->brks, brk)) {
				sendbrkto_one(s, brk, RTR_ANNOUNCE);
				addstats(s->stats.brk_announcements, 1);
				addstats(s->stats.brk_advertised, 1);
			}
		}

		RB_FOREACH(brk, brk_tree, &old_frame->rtr_brks->brks) {
			if (!RB_FIND(brk_tree,
			    &new_frame->rtr_brks->brks, brk)) {
				sendbrkto_one(s, brk, RTR_WITHDRAW);
				addstats(s->stats.brk_withdrawals, 1);
				substats(s->stats.brk_advertised, 1);
			}
		}
	}

	/* VAP */

	if (ph->version >= RTR_VERSION_2) {
		RB_FOREACH(vap, vap_tree, &new_frame->rtr_vaps->vaps) {
			node = RB_FIND(vap_tree,
			    &old_frame->rtr_vaps->vaps, vap);

			if (node) {
				if (vapfullcmp(vap, node) != 0) {
					sendvapto_one(s, vap, RTR_ANNOUNCE);
					addstats(s->stats.vap_announcements, 1);
					addstats(s->stats.vap_withdrawals, 1);
				}
			} else {
				sendvapto_one(s, vap, RTR_ANNOUNCE);
				addstats(s->stats.vap_announcements, 1);
				addstats(s->stats.vap_advertised, 1);
			}
		}

		RB_FOREACH(vap, vap_tree, &old_frame->rtr_vaps->vaps) {
			if (!RB_FIND(vap_tree,
			    &new_frame->rtr_vaps->vaps, vap)) {
				sendvapto_one(s, vap, RTR_WITHDRAW);
				addstats(s->stats.vap_withdrawals, 1);
				substats(s->stats.vap_advertised, 1);
			}
		}
	}

	sendendofdatato_one(s, new_frame->serial_number);

	logx(0, "Sent delta v%u cache from serial %u to serial %u to %s\n",
	    s->version, old_frame->serial_number, new_frame->serial_number,
	    s->name);

	return 0;
}

int
m_reset_query(struct rtr_socket *s, struct pdu_header *ph)
{
	struct cache_frame *frame;

	struct vrp4 *vrp4;
	struct vrp6 *vrp6;
	struct brk *brk;
	struct vap *vap;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED)) {
		if (ph->version > rtr_max_version) {
			s->version = rtr_max_version;
			senderrorto_one(s, UNSUPPORTED_PROTOCOL_VERSION, ph,
			    "Protocol version %d is not supported (max %d)",
			    ph->version, rtr_max_version);
			return 1;
		}

		s->version = ph->version;
		SetSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED);
		logx(1, "Registering %s with version %d via RESET_QUERY\n",
		    s->name,
		    s->version);

		remove_sched_type_and_socket(&scheduler,
		    SCHED_TYPE_HANDSHAKE_TIMEOUT, s);
		rtr_gettime();
		schedule_idle_timeout(now.tv_sec, s);
	} else {
		remove_sched_type_and_socket(&scheduler,
		    SCHED_TYPE_IDLE_TIMEOUT, s);
		rtr_gettime();
		schedule_idle_timeout(now.tv_sec, s);
	}

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_reset_query)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Reset query)");
		return 1;
	}

	logx(1, "Received RESET_QUERY from %s\n",
	    s->name);

	addstats(s->stats.reset_query_count, 1);

	s->stats.vrp4_advertised = 0;
	s->stats.vrp6_advertised = 0;
	s->stats.brk_advertised = 0;
	s->stats.vap_advertised = 0;

	frame = get_latest_cache_frame(s->version);

	if (frame == NULL) {
		senderrorto_one(s, NO_DATA_AVAILABLE, ph, "No data available");
		return 0;
	}

	sendcacheresponseto_one(s);

	RB_FOREACH(vrp4, vrp4_tree, &frame->rtr_vrp4s->vrp4s) {
		sendvrp4to_one(s, vrp4, RTR_ANNOUNCE);
		addstats(s->stats.vrp4_announcements, 1);
		addstats(s->stats.vrp4_advertised, 1);
	}

	RB_FOREACH(vrp6, vrp6_tree, &frame->rtr_vrp6s->vrp6s) {
		sendvrp6to_one(s, vrp6, RTR_ANNOUNCE);
		addstats(s->stats.vrp6_announcements, 1);
		addstats(s->stats.vrp6_advertised, 1);
	}

	if (ph->version >= RTR_VERSION_1) {
		RB_FOREACH(brk, brk_tree, &frame->rtr_brks->brks) {
			sendbrkto_one(s, brk, RTR_ANNOUNCE);
			addstats(s->stats.brk_announcements, 1);
			addstats(s->stats.brk_advertised, 1);
		}
	}

	if (ph->version >= RTR_VERSION_2) {
		RB_FOREACH(vap, vap_tree, &frame->rtr_vaps->vaps) {
			sendvapto_one(s, vap, RTR_ANNOUNCE);
			addstats(s->stats.vap_announcements, 1);
			addstats(s->stats.vap_advertised, 1);
		}
	}

	sendendofdatato_one(s, frame->serial_number);

	logx(0, "Sent full v%u cache serial %u to %s\n",
	    s->version, frame->serial_number,
	    s->name);

	return 0;
}

int
m_cache_response(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_cache_response)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Cache response)");
		return 1;
	}

	logx(1, "Received CACHE_RESPONSE from %s\n",
	    s->name);

	return 0;
}

int
m_ipv4_prefix(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_ipv4_prefix)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (IPv4 prefix)");
		return 1;
	}

	logx(1, "Received IPV4_PREFIX from %s\n",
	    s->name);

	return 0;
}

int
m_reserved(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	return 0;
}

int
m_ipv6_prefix(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_ipv6_prefix)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (IPv6 prefix)");
		return 1;
	}

	logx(1, "Received IPV6_PREFIX from %s\n",
	    s->name);

	return 0;
}

int
m_end_of_data(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->version == 0) {
		if (ph->length != sizeof(struct pdu_end_of_data_v0)) {
			senderrorto_one(s, CORRUPT_DATA, ph,
			    "Corrupt length (End of data version 0)");
			return 1;
		}
	} else {
		if (ph->length != sizeof(struct pdu_end_of_data)) {
			senderrorto_one(s, CORRUPT_DATA, ph,
			    "Corrupt length (End of data)");
			return 1;
		}
	}

	logx(1, "Received END_OF_DATA from %s\n",
	    s->name);

	return 0;
}

int
m_cache_reset(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_cache_reset)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Cache reset)");
		return 1;
	}

	logx(1, "Received CACHE_RESET from %s\n",
	    s->name);

	return 0;
}

int
m_router_key(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length < sizeof(struct pdu_router_key)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (Router key)");
		return 1;
	}

	logx(1, "Received ROUTER_KEY from %s\n",
	    s->name);

	return 0;
}

int
m_error(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_error *pe;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change "
		    "from %d to %d from client %s:%u\n",
		    s->version, ph->version,
		    inet_ntoa(s->client.sin_addr), s->client.sin_port);
		return 1;
	}

	if (ph->length < (sizeof(struct pdu_error) + sizeof(uint32_t))) {
		logx(0, "Corrupt length from client %s:%u\n",
		    inet_ntoa(s->client.sin_addr), s->client.sin_port);
		return 1;
	}

	pe = (struct pdu_error *)ph;

	if (pe->error_code < PDU_ERROR_MAX) {
		logx(0, "Received RTR error %u (%s) from client %s:%u\n",
		    pe->error_code,
		    error_code_to_str[pe->error_code],
		    inet_ntoa(s->client.sin_addr),
		    s->client.sin_port);
	} else {
		logx(0, "Received unknown RTR error %u from client %s:%u\n",
		    pe->error_code,
		    inet_ntoa(s->client.sin_addr),
		    s->client.sin_port);
	}

	if (pe->error_code == NO_DATA_AVAILABLE)
		return 0;

	return 1;
}

int
m_aspa_pdu(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		senderrorto_one(s, UNEXPECTED_PROTOCOL_VERSION, ph,
		    "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length < sizeof(struct pdu_aspa)) {
		senderrorto_one(s, CORRUPT_DATA, ph,
		    "Corrupt length (ASPA)");
		return 1;
	}

	logx(1, "Received ASPA_PDU from %s\n",
	    s->name);

	return 0;
}

/* Controller PDUs */

int
m_open_controller(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_open_controller *oc;

	assert(s);
	assert(ph);

	oc = (struct pdu_open_controller *)ph;

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_open_controller)) {
		logx(0, "Corrupt length (Open controller)");
		return 1;
	}

	logx(1, "Received OPEN_CONTROLLER from %s\n",
	    s->name);

	if (oc->controller_version != 1) {
		logx(0, "Protocol mismatch (Open controller)");
		return 1;
	}

	SetSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED);

	return 0;
}

int
m_close_controller(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_close_controller)) {
		logx(0, "Corrupt length (Close controller)");
		return 1;
	}

	logx(1, "Received CLOSE_CONTROLLER from %s\n",
	    s->name);

	return 1;
}

int
m_start_of_import(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_start_of_import)) {
		logx(0, "Corrupt length (Start of import)");
		return 1;
	}

	logx(0, "Initiating table import from %s\n",
	    s->name);
	logx(1, "Received START_OF_IMPORT from %s\n",
	    s->name);

	free_vrp4_tree(&s->vrp4s);
	free_vrp6_tree(&s->vrp6s);
	free_brk_tree(&s->brks);
	free_vap_tree(&s->vaps);

	return 0;
}

int
m_ipv4_prefix_import(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_ipv4_prefix_import *pdu_vrp4_import;
	struct vrp4 vrp4;
	uint8_t flags;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_ipv4_prefix_import)) {
		logx(0, "Corrupt length (IPv4 prefix import)");
		return 1;
	}

	pdu_vrp4_import = (struct pdu_ipv4_prefix_import *)ph;

	if (check_pdu_ipv4_prefix_controller(pdu_vrp4_import) != 0) {
		logx(0, "Corrupt data (IPv4 prefix import)");
		return 1;
	}

	vrp4.expire = pdu_vrp4_import->expire;
	vrp4.prefix_length = pdu_vrp4_import->prefix_length;
	vrp4.max_prefix_length = pdu_vrp4_import->max_prefix_length;
	vrp4.zero = 0;
	vrp4.prefix = pdu_vrp4_import->prefix;
	vrp4.asn = pdu_vrp4_import->asn;

	flags = pdu_vrp4_import->flags;

	if ((flags & 1) == RTR_ANNOUNCE) {
		rtr_gettime();
		if (insert_vrp4(&s->vrp4s, &vrp4, now.tv_sec) != 0) {
			logx(0, "Duplicate announcement received "
			    "(IPv4 prefix import)");
			return 1;
		}
	} else {
		if (remove_vrp4(&s->vrp4s, &vrp4) != 0) {
			logx(0, "Withdrawal of unknown record "
			    "(IPv4 prefix import)");
			return 1;
		}
	}

	return 0;
}

int
m_ipv6_prefix_import(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_ipv6_prefix_import *pdu_vrp6_import;
	struct vrp6 vrp6;
	uint8_t flags;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_ipv6_prefix_import)) {
		logx(0, "Corrupt length (IPv6 prefix import)");
		return 1;
	}

	pdu_vrp6_import = (struct pdu_ipv6_prefix_import *)ph;

	if (check_pdu_ipv6_prefix_controller(pdu_vrp6_import) != 0) {
		logx(0, "Corrupt data (IPv6 prefix import)");
		return 1;
	}

	vrp6.expire = pdu_vrp6_import->expire;
	vrp6.prefix_length = pdu_vrp6_import->prefix_length;
	vrp6.max_prefix_length = pdu_vrp6_import->max_prefix_length;
	vrp6.zero = 0;
	vrp6.prefix = pdu_vrp6_import->prefix;
	vrp6.asn = pdu_vrp6_import->asn;

	flags = pdu_vrp6_import->flags;

	if ((flags & 1) == RTR_ANNOUNCE) {
		rtr_gettime();
		if (insert_vrp6(&s->vrp6s, &vrp6, now.tv_sec) != 0) {
			logx(0, "Duplicate announcement received "
			    "(IPv6 prefix)");
			return 1;
		}
	} else {
		if (remove_vrp6(&s->vrp6s, &vrp6) != 0) {
			logx(0, "Withdrawal of unknown record "
			    "(IPv6 prefix)");
			return 1;
		}
	}

	return 0;
}

int
m_router_key_import(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_router_key_import *router_key_import;

	uint32_t spki_length;

	unsigned char buffer[PDU_MAX_LENGTH];
	struct brk *brk;
	uint8_t flags;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length < sizeof(struct pdu_router_key_import)) {
		logx(0, "Corrupt length (Router key import)");
		return 1;
	}

	brk = (struct brk *)buffer;

	router_key_import = (struct pdu_router_key_import *)ph;

	if (check_pdu_router_key_controller(router_key_import) != 0) {
		logx(0, "Corrupt data (Router key import)");
		return 1;
	}

	spki_length =
	    router_key_import->length -
	    sizeof(struct pdu_router_key_import);

	if (spki_length > (PDU_MAX_LENGTH - sizeof(struct brk)))
		return 0;

	brk->expire = router_key_import->expire;
	memcpy(brk->ski, router_key_import->ski, SKI_LENGTH);
	brk->asn = router_key_import->asn;
	brk->spki_length = spki_length;
	if (spki_length > 0)
		memcpy(brk->spki, router_key_import->spki, spki_length);

	flags = router_key_import->flags;

	if ((flags & 1) == RTR_ANNOUNCE) {
		rtr_gettime();
		if (insert_brk(&s->brks, brk, now.tv_sec) != 0) {
			logx(0, "Duplicate announcement received "
			    "(Router key import)");
			return 1;
		}
	} else {
		if (remove_brk(&s->brks, brk) != 0) {
			logx(0, "Withdrawal of unknown record "
			    "(Router key import)");
			return 1;
		}
	}

	return 0;
}

int
m_aspa_pdu_import(struct rtr_socket *s, struct pdu_header *ph)
{
	struct pdu_aspa_import *aspa_import;
	int provider_count;
	struct vap_buffer vap_buf;
	int i;
	uint8_t flags;

	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length < sizeof(struct pdu_aspa_import)) {
		logx(0, "Corrupt length (ASPA)");
		return 1;
	}

	aspa_import = (struct pdu_aspa_import *)ph;

	if (check_pdu_aspa_controller(aspa_import) != 0) {
		logx(0, "Corrupt data (ASPA import)");
		return 1;
	}

	provider_count = (aspa_import->length - sizeof(*aspa_import))
	    / sizeof(uint32_t);

	if (provider_count > VAP_MAX_PROVIDERS) {
		logx(0, "Too many ASPA providers for AS%d: %d "
		    "(truncated to %d)",
		    aspa_import->customer_asn,
		    provider_count,
		    VAP_MAX_PROVIDERS);
		provider_count = VAP_MAX_PROVIDERS;
	}

	vap_buf.expire = aspa_import->expire;
	vap_buf.customer_asn = aspa_import->customer_asn;
	vap_buf.provider_count = provider_count;
	for (i = 0; i < provider_count; i++) {
		vap_buf.provider_asns[i] = aspa_import->provider_asns[i];
	}

	flags = aspa_import->flags;

	if ((flags & 1) == RTR_ANNOUNCE) {
		if (provider_count == 0) {
			logx(0, "ASPA provider list error "
			    "(No providers announced)");
			return 1;
		}
		rtr_gettime();
		if (insert_vap(&s->vaps,
		    (struct vap *)&vap_buf, now.tv_sec) != 0) {
			logx(0, "Duplicate announcement received (ASPA)");
			return 1;
		}
	} else {
		if (provider_count > 0) {
			logx(0, "ASPA provider list error "
			    "(Providers listed in withdrawal)");
			return 1;
		}
		if (remove_vap(&s->vaps, (struct vap *)&vap_buf) != 0) {
			logx(0, "Withdrawal of unknown record (ASPA)");
			return 1;
		}
	}

	return 0;
}

int
m_end_of_import(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_end_of_import)) {
		logx(0, "Corrupt length (End of import)");
		return 1;
	}

	logx(1, "Received END_OF_IMPORT from %s\n",
	    s->name);

	update_cache(&s->vrp4s, &s->vrp6s, &s->brks, &s->vaps);

	remove_sched_type(&scheduler, SCHED_TYPE_CLEANUP);
	rtr_gettime();
	schedule_cleanup(now.tv_sec);

	logx(0, "Completed table import from %s\n",
	    s->name);

	return 1;
}

int
m_push_import(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_push_import)) {
		logx(0, "Corrupt length (Push import)");
		return 1;
	}

	logx(1, "Received PUSH_IMPORT from %s\n",
	    s->name);

	update_cache(&s->vrp4s, &s->vrp6s, &s->brks, &s->vaps);

	remove_sched_type(&scheduler, SCHED_TYPE_CLEANUP);
	rtr_gettime();
	schedule_cleanup(now.tv_sec);

	logx(0, "Completed table push import from %s\n",
	    s->name);

	return 0;
}

int
m_query_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_query_stats)) {
		logx(0, "Corrupt length (Query stats)");
		return 1;
	}

	logx(1, "Received QUERY_STATS from %s\n",
	    s->name);

	sendstartofstatsto_one(s);

	sendglobalstatsto_one(s);
	sendclientstatsto_one(s);
	sendcacheframestatsto_one(s);

	sendendofstatsto_one(s);

	logx(0, "Sent statistics to %s\n",
	    s->name);

	return 0;
}

int
m_start_of_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_start_of_stats)) {
		logx(0, "Corrupt length (Start of stats)");
		return 1;
	}

	logx(1, "Received START_OF_STATS from %s\n",
	    s->name);

	return 0;
}

int
m_global_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_global_stats)) {
		logx(0, "Corrupt length (Global stats)");
		return 1;
	}

	logx(1, "Received GLOBAL_STATS from %s\n",
	    s->name);

	return 0;
}

int
m_client_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_client_stats)) {
		logx(0, "Corrupt length (Client stats)");
		return 1;
	}

	logx(1, "Received CLIENT_STATS from %s\n",
	    s->name);

	return 0;
}

int
m_cache_frame_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_cache_frame_stats)) {
		logx(0, "Corrupt length (Cache frame stats)");
		return 1;
	}

	logx(1, "Received CACHE_FRAME_STATS from %s\n",
	    s->name);

	return 0;
}

int
m_end_of_stats(struct rtr_socket *s, struct pdu_header *ph)
{
	assert(s);
	assert(ph);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return 0;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return 0;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return 0;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return 0;

	if (ph->version != s->version) {
		logx(0, "Illegal protocol version change from %d to %d",
		    s->version, ph->version);
		return 1;
	}

	if (ph->length != sizeof(struct pdu_end_of_stats)) {
		logx(0, "Corrupt length (End of stats)");
		return 1;
	}

	logx(1, "Received END_OF_STATS from %s\n",
	    s->name);

	return 0;
}

int (*command_table[])(struct rtr_socket *, struct pdu_header *) = {
	m_serial_notify,	/*   0 */
	m_serial_query,		/*   1 */
	m_reset_query,		/*   2 */
	m_cache_response,	/*   3 */
	m_ipv4_prefix,		/*   4 */
	m_reserved,		/*   5 */
	m_ipv6_prefix,		/*   6 */
	m_end_of_data,		/*   7 */
	m_cache_reset,		/*   8 */
	m_router_key,		/*   9 */
	m_error,		/*  10 */
	m_aspa_pdu		/*  11 */
};

int (*rtrx_command_table[])(struct rtr_socket *, struct pdu_header *) = {
	m_open_controller,	/* 128 */
	m_close_controller,	/* 129 */
	m_start_of_import,	/* 130 */
	m_ipv4_prefix_import,	/* 131 */
	m_ipv6_prefix_import,	/* 132 */
	m_router_key_import,	/* 133 */
	m_aspa_pdu_import,	/* 134 */
	m_end_of_import,	/* 135 */
	m_push_import,		/* 136 */
	m_query_stats,		/* 137 */
	m_start_of_stats,	/* 138 */
	m_global_stats,		/* 139 */
	m_client_stats,		/* 140 */
	m_cache_frame_stats,	/* 141 */
	m_end_of_stats		/* 142 */
};

int (*command_lookup(uint8_t version, uint8_t type, int sock_type))
    (struct rtr_socket *, struct pdu_header *)
{

	/* check for stuff thats not there */

	if ((type >= PDU_TYPE_MAX) && (type < PDU_SPLIT))
		return NULL;

	if (type >= PDU_RTRX_MAX)
		return NULL;

	if (type == RESERVED)
		return NULL;

	/* check for wrong versions */

	if ((type == ROUTER_KEY) && (version < RTR_VERSION_1))
		return NULL;

	if ((type == ASPA_PDU) && (version < RTR_VERSION_2))
		return NULL;

	if ((type >= PDU_SPLIT) && (version < RTR_VERSION_2))
		return NULL;

	/* check for wrong types */

	if ((type < PDU_SPLIT) && (sock_type != RTR_SOCKET_TYPE_CLIENT))
		return NULL;

	if ((type >= PDU_SPLIT) && (sock_type != RTR_SOCKET_TYPE_CONTROLLER))
		return NULL;

	if (type < PDU_SPLIT)
		return command_table[type];
	else
		return rtrx_command_table[type-PDU_SPLIT];
}
