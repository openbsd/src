/*	$OpenBSD: send.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
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
#include <assert.h>
#include <err.h>
#include <errno.h>
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

#define PDU_ERROR_MAX_MESSAGE \
    (PDU_MAX_LENGTH - (sizeof(struct pdu_error) + sizeof(uint32_t)))

ssize_t
sendto_one(struct rtr_socket *s, void *pdu)
{
	struct pdu_header *ph = pdu;
	ssize_t ret;

	assert(s);
	assert(pdu);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type == RTR_SOCKET_TYPE_UNKNOWN)
		return -1;
	if (s->type == RTR_SOCKET_TYPE_CONTROLLER)
		return -1;

	if (ph->type >= PDU_TYPE_MAX) {
		logx(1, "Sending unknown PDU (%u) to %s\n",
		    ph->type, s->name);
	} else {
		logx(1, "Sending %s to %s\n",
		    pdu_type_to_str[ph->type], s->name);
	}

	/* pdu is in host order */

	ph->version = s->version;

	switch (ph->type) {
	case SERIAL_NOTIFY:
	case SERIAL_QUERY:
	case CACHE_RESPONSE:
	case END_OF_DATA:
		ph->reserved = cache[s->version].session_id;
		break;
	}

	pdu_hton(s, ph);
	ret = writeto(s, ph);
	pdu_ntoh(s, ph);
	return ret;
}

void
sendto_allclients(void *pdu)
{
	struct pdu_header *ph = pdu;
	int fd, i;
	struct rtr_socket *s;

	assert(pdu);

	/* pdu is in host order */

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;

		if (is_listener(fd))
			continue;

		s = fd_to_socket(fd);
		if (s == NULL)
			err(1, NULL);

		if ((POLLEVENT(i) & POLLIN) &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) &&
		    !TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED) &&
		    s->type == RTR_SOCKET_TYPE_CLIENT) {

			if (ph->type >= PDU_TYPE_MAX) {
				logx(1, "Sending unknown PDU (%u) to %s\n",
				    ph->type, s->name);
			} else {
				logx(1, "Sending %s to %s\n",
				    pdu_type_to_str[ph->type], s->name);
			}

			ph->version = s->version;

			switch (ph->type) {
			case SERIAL_NOTIFY:
			case SERIAL_QUERY:
			case CACHE_RESPONSE:
			case END_OF_DATA:
				ph->reserved = cache[s->version].session_id;
				break;
			}

			pdu_hton(s, ph);
			writeto(s, ph);
			pdu_ntoh(s, ph);
		}
	}
}

void
sendto_allregisteredclients(void *pdu)
{
	struct pdu_header *ph = pdu;
	struct rtr_socket *s;
	int fd, i;

	assert(pdu);

	/* pdu is in host order */

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;

		if (is_listener(fd))
			continue;

		s = fd_to_socket(fd);
		if (s == NULL)
			err(1, NULL);

		if ((POLLEVENT(i) & POLLIN) &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) &&
		    !TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED) &&
		    s->type == RTR_SOCKET_TYPE_CLIENT &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED)) {

			if (ph->type >= PDU_TYPE_MAX) {
				logx(1, "Sending unknown PDU (%u) to %s\n",
				    ph->type,
				    s->name);
			} else {
				logx(1, "Sending %s to %s\n",
				    pdu_type_to_str[ph->type],
				    s->name);
			}

			ph->version = s->version;

			switch (ph->type) {
			case SERIAL_NOTIFY:
			case SERIAL_QUERY:
			case CACHE_RESPONSE:
			case END_OF_DATA:
				ph->reserved = cache[s->version].session_id;
				break;
			}

			pdu_hton(s, ph);
			writeto(s, ph);
			pdu_ntoh(s, ph);
		}
	}
}

void
sendto_allregisteredclientsversion(void *pdu, uint8_t version)
{
	struct pdu_header *ph = pdu;
	int fd, i;
	struct rtr_socket *s;

	assert(pdu);

	/* pdu is in host order */

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;
		if (is_listener(fd))
			continue;

		s = fd_to_socket(fd);
		if (s == NULL)
			err(1, NULL);

		if ((POLLEVENT(i) & POLLIN) &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE) &&
		    !TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED) &&
		    s->type == RTR_SOCKET_TYPE_CLIENT &&
		    TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED) &&
		    s->version == version) {

			if (ph->type >= PDU_TYPE_MAX) {
				logx(1, "Sending unknown PDU (%u) to %s\n",
				    ph->type,
				    s->name);
			} else {
				logx(1, "Sending %s to %s\n",
				    pdu_type_to_str[ph->type],
				    s->name);
			}

			ph->version = s->version;

			switch (ph->type) {
			case SERIAL_NOTIFY:
			case SERIAL_QUERY:
			case CACHE_RESPONSE:
			case END_OF_DATA:
				ph->reserved = cache[s->version].session_id;
				break;
			}

			pdu_hton(s, ph);
			writeto(s, ph);
			pdu_ntoh(s, ph);
		}

	}
}


ssize_t
sendserialnotifyto_one(struct rtr_socket *s, uint32_t serial_number)
{
	struct pdu_serial_notify sn;
	ssize_t ret;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	sn.version = s->version;
	sn.type = SERIAL_NOTIFY;
	sn.session_id = cache[s->version].session_id;
	sn.length = sizeof(struct pdu_serial_notify);
	sn.serial_number = serial_number;

	logx(1, "Sending SERIAL_NOTIFY %d to %s\n",
	    serial_number,
	    s->name);

	/* pdu is in host order */

	pdu_hton(s, &sn);
	ret = writeto(s, &sn);

	return ret;
}

ssize_t
sendcacheresponseto_one(struct rtr_socket *s)
{
	struct pdu_cache_response cr;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	cr.version = s->version;
	cr.type = CACHE_RESPONSE;
	cr.session_id = cache[s->version].session_id;
	cr.length = sizeof(struct pdu_cache_response);

	logx(1, "Sending CACHE_RESPONSE to %s\n",
	    s->name);

	/* pdu is in host order */

	pdu_hton(s, &cr);
	return writeto(s, &cr);
}


ssize_t
sendvrp4to_one(struct rtr_socket *s, struct vrp4 *vrp4, uint8_t flags)
{
	struct pdu_ipv4_prefix ip4;
	ssize_t ret;

	assert(s);
	assert(vrp4);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	ip4.version = s->version;
	ip4.type = IPV4_PREFIX;
	ip4.reserved = 0;
	ip4.length = sizeof(struct pdu_ipv4_prefix);
	ip4.flags = flags;
	ip4.prefix_length = vrp4->prefix_length;
	ip4.max_prefix_length = vrp4->max_prefix_length;
	ip4.zero = 0;
	ip4.prefix = vrp4->prefix;
	ip4.asn = vrp4->asn;

	/* pdu is in host order */

	pdu_hton(s, &ip4);
	ret = writeto(s, &ip4);

	return ret;
}

ssize_t
sendvrp6to_one(struct rtr_socket *s, struct vrp6 *vrp6, uint8_t flags)
{
	struct pdu_ipv6_prefix ip6;
	ssize_t ret;

	assert(s);
	assert(vrp6);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	ip6.version = s->version;
	ip6.type = IPV6_PREFIX;
	ip6.reserved = 0;
	ip6.length = sizeof(struct pdu_ipv6_prefix);
	ip6.flags = flags;
	ip6.prefix_length = vrp6->prefix_length;
	ip6.max_prefix_length = vrp6->max_prefix_length;
	ip6.zero = 0;
	ip6.prefix = vrp6->prefix;
	ip6.asn = vrp6->asn;

	/* pdu is in host order */

	pdu_hton(s, &ip6);
	ret = writeto(s, &ip6);

	return ret;
}

ssize_t
sendbrkto_one(struct rtr_socket *s, struct brk *brk, uint8_t flags)
{
	unsigned char pdu[PDU_MAX_LENGTH];
	struct pdu_router_key *router_key;
	ssize_t ret;

	assert(s);
	assert(brk);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	router_key = (struct pdu_router_key *)&pdu;

	router_key->version = s->version;
	router_key->type = ROUTER_KEY;
	router_key->flags = flags;
	router_key->zero = 0;
	router_key->length = sizeof(struct pdu_router_key) + brk->spki_length;
	memcpy(router_key->ski, brk->ski, SKI_LENGTH);
	router_key->asn = brk->asn;
	memcpy(router_key->spki, brk->spki, brk->spki_length);

	/* pdu is in host order */

	pdu_hton(s, router_key);
	ret = writeto(s, router_key);

	return ret;
}

ssize_t
sendvapto_one(struct rtr_socket *s, struct vap *vap, uint8_t flags)
{
	unsigned char pdu[PDU_MAX_LENGTH];
	struct pdu_aspa *aspa;
	ssize_t ret;
	int32_t p_index;

	assert(s);
	assert(vap);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;
	if (vap->provider_count > VAP_MAX_PROVIDERS)
		return -1;


	aspa = (struct pdu_aspa *)&pdu;

	aspa->version = s->version;
	aspa->type = ASPA_PDU;
	aspa->flags = flags;
	aspa->zero = 0;
	if (flags)
		aspa->length =
		    sizeof(struct pdu_aspa) +
		    (vap->provider_count * sizeof(uint32_t));
	else
		aspa->length = sizeof(struct pdu_aspa);

	aspa->customer_asn = vap->customer_asn;

	if (flags) {
		for (p_index = 0;
		    p_index < vap->provider_count;
		    p_index++) {
			aspa->provider_asns[p_index] =
			    vap->provider_asns[p_index];
		}
	}

	/* pdu is in host order */

	pdu_hton(s, aspa);
	ret = writeto(s, aspa);

	return ret;
}

ssize_t
sendendofdatato_one(struct rtr_socket *s, uint32_t serial_number)
{
	struct pdu_end_of_data_v0 eod0;
	struct pdu_end_of_data eod;
	ssize_t ret;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	logx(1, "Sending END_OF_DATA to %s\n",
	    s->name);

	if (s->version == RTR_VERSION_0) {
		eod0.version = s->version;
		eod0.type = END_OF_DATA;
		eod0.session_id = cache[s->version].session_id;
		eod0.length = sizeof(struct pdu_end_of_data_v0);
		eod0.serial_number = serial_number;

		pdu_hton(s, &eod0);
		ret = writeto(s, &eod0);
	} else {
		eod.version = s->version;
		eod.type = END_OF_DATA;
		eod.session_id = cache[s->version].session_id;
		eod.length = sizeof(struct pdu_end_of_data);
		eod.serial_number = serial_number;
		eod.refresh_interval = cache[s->version].refresh_interval;
		eod.retry_interval = cache[s->version].retry_interval;
		eod.expire_interval = cache[s->version].expire_interval;

		pdu_hton(s, &eod);
		ret = writeto(s, &eod);
	}

	return ret;
}


ssize_t
senderrorto_one(struct rtr_socket *s, uint16_t error_code, void *pdu,
    char *message, ...)
{
	unsigned char error_pdu[PDU_MAX_LENGTH];
	struct pdu_error *pe;
	struct pdu_header *ph;
	char message_buffer[PDU_ERROR_MAX_MESSAGE + 1];	/* has null */
	va_list va;
	uint32_t message_length;
	uint32_t *text_length;
	char *text;

	ssize_t ret;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;

	if (message) {
		va_start(va, message);
		/* has null */
		vsnprintf(message_buffer, PDU_ERROR_MAX_MESSAGE + 1,
		    message, va);
		va_end(va);
		message_length = strlen(message_buffer);
	} else {
		message_length = 0;
	}

	if (message_length > 0) {
		if (check_error_code(error_code, s->version) == 0) {
			logx(0, "Sending ERROR %s (%u) to %s [%s]\n",
			    error_code_to_str[error_code],
			    error_code,
			    s->name,
			    message_buffer);
		} else {
			return 0;
		}
	} else {
		if (check_error_code(error_code, s->version) == 0) {
			logx(0, "Sending ERROR %s (%u) to %s\n",
			    error_code_to_str[error_code],
			    error_code,
			    s->name);
		} else {
			return 0;
		}
	}

	pe = (struct pdu_error *)error_pdu;
	pe->version = s->version;
	pe->type = ERROR;
	pe->error_code = error_code;

	ph = pdu;

	if (ph && (ph->type != ERROR) &&
	    (ph->length <= PDU_MAX_LENGTH - (sizeof(struct pdu_error) +
	    sizeof(uint32_t) + message_length))) {
		pe->pdu_length = ph->length;
		memcpy(pe->pdu, ph, pe->pdu_length);
		pdu_hton(s, pe->pdu);
	} else {
		pe->pdu_length = 0;
	}

	text_length = (uint32_t *)(pe->pdu + pe->pdu_length);

	*text_length = message_length;

	if (message_length > 0) {
		text = (char *)(text_length + 1);
		memcpy(text, message_buffer, message_length);	/* no null */
	}

	pe->length = (sizeof(struct pdu_error) + pe->pdu_length +
	    sizeof(uint32_t) + message_length);

	/* pdu is in host order */

	pdu_hton(s, pe);
	ret = writeto(s, pe);

	return ret;
}

ssize_t
sendcacheresetto_one(struct rtr_socket *s)
{
	struct pdu_cache_reset cr;
	ssize_t ret;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CLIENT)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	cr.version = s->version;
	cr.type = CACHE_RESET;
	cr.reserved = 0;
	cr.length = sizeof(struct pdu_cache_reset);

	logx(1, "Sending CACHE_RESET to %s\n",
	    s->name);

	/* pdu is in host order */

	pdu_hton(s, &cr);
	ret = writeto(s, &cr);

	return ret;
}

ssize_t
sendstartofstatsto_one(struct rtr_socket *s)
{
	struct pdu_start_of_stats sos;
	ssize_t ret;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	sos.version = s->version;
	sos.type = START_OF_STATS;
	sos.reserved = 0;
	sos.length = sizeof(struct pdu_start_of_stats);

	logx(1, "Sending START_OF_STATS to %s\n",
	    s->name);

	/* pdu is in host order */

	pdu_hton(s, &sos);
	ret = writeto(s, &sos);

	return ret;
}

ssize_t
sendglobalstatsto_one(struct rtr_socket *s)
{
	struct pdu_global_stats gs;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	gs.version = s->version;
	gs.type = GLOBAL_STATS;
	gs.reserved = 0;
	gs.length = sizeof(struct pdu_global_stats);

	memset(gs.nodename, 0, NODENAME_SIZE);
	strncpy(gs.nodename, global_stats.nodename, NODENAME_SIZE-1);
	gs.nodename[NODENAME_SIZE-1]= '\0';

	memset(gs.domainname, 0, DOMAINNAME_SIZE);
	strncpy(gs.domainname, global_stats.domainname, DOMAINNAME_SIZE-1);
	gs.domainname[DOMAINNAME_SIZE-1]= '\0';

	memset(gs.release, 0, RELEASE_SIZE);
	strncpy(gs.release, global_stats.release, RELEASE_SIZE-1);
	gs.release[RELEASE_SIZE-1]= '\0';

	gs.start_time = global_stats.start_time;

	gs.total_bytes_in = global_stats.total_bytes_in;
	gs.total_bytes_out = global_stats.total_bytes_out;
	gs.total_client_connects = global_stats.total_client_connects;
	gs.total_controller_connects = global_stats.total_controller_connects;

	logx(1, "Sending GLOBAL_STATS to %s\n",
	    s->name);

	/* pdu is in host order */

	pdu_hton(s, &gs);
	return writeto(s, &gs);
}

void
sendclientstatsto_one(struct rtr_socket *s)
{
	struct pdu_client_stats cs;
	int fd, i;
	struct rtr_socket *client;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return;

	cs.version = s->version;
	cs.type = CLIENT_STATS;
	cs.reserved = 0;
	cs.length = sizeof(struct pdu_client_stats);

	for (i = 0; i < poll_table_count; i++) {
		fd = poll_table[i].fd;
		if (is_listener(fd))
			continue;

		client = fd_to_socket(fd);
		if (client == NULL)
			err(1, NULL);

		if ((POLLEVENT(i) & POLLIN) &&
		    TestSocketFlag(
			client, RTR_SOCKET_FLAG_INUSE) &&
		    client->type == RTR_SOCKET_TYPE_CLIENT) {
			cs.fd = fd;
			cs.client_ip = client->client.sin_addr;
			cs.client_port = client->client.sin_port;
			cs.client_version = client->version;
			cs.client_state = 0;
			if (TestSocketFlag(client, RTR_SOCKET_FLAG_REGISTERED))
				cs.client_state |= CLIENT_STATE_REGISTERED;
			if (TestSocketFlag(client, RTR_SOCKET_FLAG_CLOSED))
				cs.client_state |= CLIENT_STATE_CLOSED;
			cs.connect_time = client->stats.connect_time;
			cs.total_bytes_in = client->stats.total_bytes_in;
			cs.total_bytes_out = client->stats.total_bytes_out;
			cs.sendq_length = client->sendq.length;
			cs.sendq_max = client->sendq.max;
			cs.vrp4_announcements = client->stats.vrp4_announcements;
			cs.vrp4_withdrawals = client->stats.vrp4_withdrawals;
			cs.vrp4_advertised = client->stats.vrp4_advertised;
			cs.vrp6_announcements = client->stats.vrp6_announcements;
			cs.vrp6_withdrawals = client->stats.vrp6_withdrawals;
			cs.vrp6_advertised = client->stats.vrp6_advertised;
			cs.brk_announcements = client->stats.brk_announcements;
			cs.brk_withdrawals = client->stats.brk_withdrawals;
			cs.brk_advertised = client->stats.brk_advertised;
			cs.vap_announcements = client->stats.vap_announcements;
			cs.vap_withdrawals = client->stats.vap_withdrawals;
			cs.vap_advertised = client->stats.vap_advertised;
			cs.reset_query_count = client->stats.reset_query_count;
			cs.serial_query_count = client->stats.serial_query_count;
			pdu_hton(s, &cs);
			writeto(s, &cs);
			pdu_ntoh(s, &cs);
		}
	}
	logx(1, "Sending CLIENT_STATS to %s\n", s->name);
}

void
sendcacheframestatsto_one(struct rtr_socket *s)
{
	struct pdu_cache_frame_stats cfs;
	int i;
	uint8_t v;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return;

	cfs.version = s->version;
	cfs.type = CACHE_FRAME_STATS;
	cfs.reserved = 0;
	cfs.flags = 0;
	cfs.length = sizeof(struct pdu_cache_frame_stats);

	for (v = RTR_VERSION_0; v <= RTR_MAX_VERSION; v++) {
		if (cache[v].head == -1 && cache[v].tail == -1)
			continue;

		for (i = cache[v].tail; i != cache[v].head;
		    i = (i + 1) % cache[v].frame_count) {
			cfs.flags = 0;
			cfs.cache_version = v;
			cfs.zero = 0;
			cfs.cache_session_id = cache[v].session_id;
			cfs.cache_serial = CFRAME(v,i).serial_number;
			cfs.vrp4_count = CFRAME(v,i).rtr_vrp4s->entry_count;
			cfs.vrp4_creation_time = CFRAME(v,i).rtr_vrp4s->creation_time;
			cfs.vrp6_count = CFRAME(v,i).rtr_vrp6s->entry_count;
			cfs.vrp6_creation_time = CFRAME(v,i).rtr_vrp6s->creation_time;

			if (v >= RTR_VERSION_1) {
				cfs.brk_count = CFRAME(v,i).rtr_brks->entry_count;
				cfs.brk_creation_time =
				    CFRAME(v,i).rtr_brks->creation_time;
			} else {
				cfs.brk_count = 0;
				cfs.brk_creation_time = 0;
			}
			if (v >= RTR_VERSION_2) {
				cfs.vap_count = CFRAME(v,i).rtr_vaps->entry_count;
				cfs.vap_creation_time =
				    CFRAME(v,i).rtr_vaps->creation_time;
			} else {
				cfs.vap_count = 0;
				cfs.vap_creation_time = 0;
			}
			pdu_hton(s, &cfs);
			writeto(s, &cfs);
			pdu_ntoh(s, &cfs);
		}

		cfs.flags = 1;
		cfs.cache_version = v;
		cfs.zero = 0;
		cfs.cache_session_id = cache[v].session_id;
		cfs.cache_serial = CHEAD(v).serial_number;
		cfs.vrp4_count = CHEAD(v).rtr_vrp4s->entry_count;
		cfs.vrp4_creation_time = CHEAD(v).rtr_vrp4s->creation_time;
		cfs.vrp6_count = CHEAD(v).rtr_vrp6s->entry_count;
		cfs.vrp6_creation_time = CHEAD(v).rtr_vrp6s->creation_time;
		if (v >= RTR_VERSION_1) {
			cfs.brk_count = CHEAD(v).rtr_brks->entry_count;
			cfs.brk_creation_time =
			    CHEAD(v).rtr_brks->creation_time;
		} else {
			cfs.brk_count = 0;
			cfs.brk_creation_time = 0;
		}
		if (v >= RTR_VERSION_2) {
			cfs.vap_count = CHEAD(v).rtr_vaps->entry_count;
			cfs.vap_creation_time =
			    CHEAD(v).rtr_vaps->creation_time;
		} else {
			cfs.vap_count = 0;
			cfs.vap_creation_time = 0;
		}
		pdu_hton(s, &cfs);
		writeto(s, &cfs);
		pdu_ntoh(s, &cfs);
	}
	logx(1, "Sending CACHE_FRAME_STATS to %s\n", s->name);
}

ssize_t
sendendofstatsto_one(struct rtr_socket *s)
{
	struct pdu_end_of_stats eos;

	assert(s);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return -1;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return -1;
	if (s->type != RTR_SOCKET_TYPE_CONTROLLER)
		return -1;
	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_REGISTERED))
		return -1;

	eos.version = s->version;
	eos.type = END_OF_STATS;
	eos.reserved = 0;
	eos.length = sizeof(struct pdu_end_of_stats);

	logx(1, "Sending END_OF_STATS to %s\n", s->name);

	/* pdu is in host order */

	pdu_hton(s, &eos);
	return writeto(s, &eos);
}
