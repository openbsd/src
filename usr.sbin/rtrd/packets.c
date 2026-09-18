/*	$OpenBSD: packets.c,v 1.4 2026/09/18 04:55:39 deraadt Exp $ */
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
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
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

char *error_code_to_str[] = {
	"CORRUPT_DATA",
	"INTERNAL_ERROR",
	"NO_DATA_AVAILABLE",
	"INVALID_REQUEST",
	"UNSUPPORTED_PROTOCOL_VERSION",
	"UNSUPPORTED_PDU_TYPE",
	"WITHDRAWAL_OF_UNKNOWN_RECORD",
	"DUPLICATE_ANNOUNCEMENT_RECEIVED",
	"UNEXPECTED_PROTOCOL_VERSION",
	"ASPA_PROVIDER_LIST_ERROR",
	"TRANSPORT_ERROR",
	"ORDERING_ERROR",
	"CACHE_RESTART",
	"CACHE_SHUTDOWN"
};

char *pdu_type_to_str[] = {
	"SERIAL_NOTIFY",
	"SERIAL_QUERY",
	"RESET_QUERY",
	"CACHE_RESPONSE",
	"IPV4_PREFIX",
	"RESERVED",
	"IPV6_PREFIX",
	"END_OF_DATA",
	"CACHE_RESET",
	"ROUTER_KEY",
	"ERROR",
	"ASPA_PDU"
};

void
pdu_hton(struct rtr_socket *s, void *pdu)
{
	assert(s);
	assert(pdu);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return;
	if (s->type == RTR_SOCKET_TYPE_UNKNOWN)
		return;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return;

	switch (s->type) {
	case RTR_SOCKET_TYPE_CLIENT:
		pdu_hton_rtr(pdu);
		break;
	case RTR_SOCKET_TYPE_CONTROLLER:
		pdu_hton_controller(pdu);
		break;
	}
}

void
pdu_ntoh(struct rtr_socket *s, void *pdu)
{
	assert(s);
	assert(pdu);

	if (!TestSocketFlag(s, RTR_SOCKET_FLAG_INUSE))
		return;
	if (s->type == RTR_SOCKET_TYPE_UNKNOWN)
		return;
	if (TestSocketFlag(s, RTR_SOCKET_FLAG_CLOSED))
		return;

	switch (s->type) {
	case RTR_SOCKET_TYPE_CLIENT:
		pdu_ntoh_rtr(pdu);
		break;
	case RTR_SOCKET_TYPE_CONTROLLER:
		pdu_ntoh_controller(pdu);
		break;
	}
}

void
pdu_hton_rtr(void *pdu)
{
	struct pdu_header *ph;
	struct pdu_serial_notify *sn;
	struct pdu_serial_query *sq;
	struct pdu_ipv4_prefix *vrp4;
	struct pdu_ipv6_prefix *vrp6;
	struct pdu_end_of_data_v0 *eod0;
	struct pdu_end_of_data *eod;
	struct pdu_router_key *rk;
	struct pdu_error *e;
	uint32_t *text_length;
	struct pdu_aspa *aspa;
	int provider_count;
	int i;

	assert(pdu);
	ph = pdu;

	if ((ph->type != ROUTER_KEY) && (ph->type != ASPA_PDU))
		ph->reserved = htobe16(ph->reserved);
	ph->length = htobe32(ph->length);

	switch (ph->type) {
	case SERIAL_NOTIFY:
		sn = pdu;
		sn->serial_number = htobe32(sn->serial_number);
		break;
	case SERIAL_QUERY:
		sq = pdu;
		sq->serial_number = htobe32(sq->serial_number);
		break;
	/* case RESET_QUERY: */
		/* break; */
	/* case CACHE_RESPONSE: */
		/* break; */
	case IPV4_PREFIX:
		vrp4 = pdu;
		vrp4->asn = htobe32(vrp4->asn);
		break;
	case IPV6_PREFIX:
		vrp6 = pdu;
		vrp6->asn = htobe32(vrp6->asn);
		break;
	case END_OF_DATA:
		if (ph->version == RTR_VERSION_0)
		{
			eod0 = pdu;
			eod0->serial_number = htobe32(eod0->serial_number);
		}
		else
		{
			eod = pdu;
			eod->serial_number    = htobe32(eod->serial_number);
			eod->refresh_interval = htobe32(eod->refresh_interval);
			eod->retry_interval   = htobe32(eod->retry_interval);
			eod->expire_interval  = htobe32(eod->expire_interval);
		}
		break;
	/* case CACHE_RESET: */
		/* break; */
	case ROUTER_KEY:
		if (ph->version < RTR_VERSION_1)
			break;
		rk = pdu;
		rk->asn = htobe32(rk->asn);
		break;
	case ERROR:
		e = pdu;
		text_length = (uint32_t *)(e->pdu + e->pdu_length);
		e->pdu_length = htobe32(e->pdu_length);
		*text_length = htobe32(*text_length);
		break;
	case ASPA_PDU:
		if (ph->version < RTR_VERSION_2)
			break;
		aspa = pdu;
		aspa->customer_asn = htobe32(aspa->customer_asn);
		provider_count =
		    (be32toh(aspa->length) - sizeof(struct pdu_aspa)) /
		    sizeof(uint32_t);
		for (i = 0; i < provider_count; i++) {
			aspa->provider_asns[i] =
			    htobe32(aspa->provider_asns[i]);
		}
		break;
	}
}

void
pdu_ntoh_rtr(void *pdu)
{
	struct pdu_header *ph;
	struct pdu_serial_notify *sn;
	struct pdu_serial_query *sq;
	struct pdu_ipv4_prefix *vrp4;
	struct pdu_ipv6_prefix *vrp6;
	struct pdu_end_of_data_v0 *eod0;
	struct pdu_end_of_data *eod;
	struct pdu_router_key *rk;
	struct pdu_error *e;
	uint32_t *text_length;
	struct pdu_aspa *aspa;
	int provider_count;
	int i;

	assert(pdu);
	ph = pdu;

	if ((ph->type != ROUTER_KEY) && (ph->type != ASPA_PDU))
		ph->reserved = be16toh(ph->reserved);
	ph->length = be32toh(ph->length);

	switch (ph->type) {
	case SERIAL_NOTIFY:
		sn = pdu;
		sn->serial_number = be32toh(sn->serial_number);
		break;
	case SERIAL_QUERY:
		sq = pdu;
		sq->serial_number = be32toh(sq->serial_number);
		break;
	/* case RESET_QUERY: */
		/* break; */
	/* case CACHE_RESPONSE: */
		/* break; */
	case IPV4_PREFIX:
		vrp4 = pdu;
		vrp4->asn = be32toh(vrp4->asn);
		break;
	case IPV6_PREFIX:
		vrp6 = pdu;
		vrp6->asn = be32toh(vrp6->asn);
		break;
	case END_OF_DATA:
		if (ph->version == RTR_VERSION_0) {
			eod0 = pdu;
			eod0->serial_number = be32toh(eod0->serial_number);
		} else {
			eod = pdu;
			eod->serial_number    = be32toh(eod->serial_number);
			eod->refresh_interval = be32toh(eod->refresh_interval);
			eod->retry_interval   = be32toh(eod->retry_interval);
			eod->expire_interval  = be32toh(eod->expire_interval);
		}
		break;
	/* case CACHE_RESET: */
		/* break; */
	case ROUTER_KEY:
		if (ph->version < RTR_VERSION_1)
			break;
		rk = pdu;
		rk->asn = be32toh(rk->asn);
		break;
	case ERROR:
		e = pdu;
		e->pdu_length = be32toh(e->pdu_length);
		text_length = (uint32_t *)(e->pdu + e->pdu_length);
		*text_length = be32toh(*text_length);
		break;
	case ASPA_PDU:
		if (ph->version < RTR_VERSION_2)
			break;
		aspa = pdu;
		aspa->customer_asn = be32toh(aspa->customer_asn);
		provider_count = (aspa->length - sizeof(struct pdu_aspa))
		    / sizeof(uint32_t);
		for (i = 0; i < provider_count; i++) {
			aspa->provider_asns[i] =
			    be32toh(aspa->provider_asns[i]);
		}
		break;
	}
}

void
pdu_hton_controller(void *pdu)
{
	struct pdu_header *ph;
	struct pdu_ipv4_prefix_import *vrp4;
	struct pdu_ipv6_prefix_import *vrp6;
	struct pdu_router_key_import *rk;
	struct pdu_aspa_import *aspa;
	struct pdu_global_stats *gs;
	struct pdu_client_stats *cs;
	struct pdu_cache_frame_stats *cfs;
	struct pdu_open_controller *oc;
	int provider_count;
	int i;

	assert(pdu);
	ph = pdu;

	ph->length = htobe32(ph->length);

	switch (ph->type) {
	case OPEN_CONTROLLER:
		oc = pdu;
		oc->controller_version = htobe32(oc->controller_version);
		oc->controller_flags   = htobe32(oc->controller_flags);
		break;
	/* case CLOSE_CONTROLLER: */
		/* break; */
	/* case START_OF_IMPORT: */
		/* break; */
	case IPV4_PREFIX_IMPORT:
		vrp4 = pdu;
		vrp4->expire = htobe64(vrp4->expire);
		vrp4->asn    = htobe32(vrp4->asn);
		break;
	case IPV6_PREFIX_IMPORT:
		vrp6 = pdu;
		vrp6->expire = htobe64(vrp6->expire);
		vrp6->asn    = htobe32(vrp6->asn);
		break;
	case ROUTER_KEY_IMPORT:
		rk = pdu;
		rk->expire = htobe64(rk->expire);
		rk->asn    = htobe32(rk->asn);
		break;
	case ASPA_PDU_IMPORT:
		aspa = pdu;
		aspa->expire = htobe64(aspa->expire);
		aspa->customer_asn = htobe32(aspa->customer_asn);
		provider_count =
		    (be32toh(aspa->length) - sizeof(struct pdu_aspa)) /
		    sizeof(uint32_t);
		for (i = 0; i < provider_count; i++) {
			aspa->provider_asns[i] =
			    htobe32(aspa->provider_asns[i]);
		}
		break;
	/* case END_OF_IMPORT: */
		/* break; */
	/* case QUERY_STATS: */
		/* break; */
	/* case START_OF_STATS: */
		/* break; */
	case GLOBAL_STATS:
		gs = pdu;
		gs->start_time =
		    htobe64(gs->start_time);
		gs->total_bytes_in =
		    htobe64(gs->total_bytes_in);
		gs->total_bytes_out =
		    htobe64(gs->total_bytes_out);
		gs->total_client_connects =
		    htobe64(gs->total_client_connects);
		gs->total_controller_connects =
		    htobe64(gs->total_controller_connects);
		break;
	case CLIENT_STATS:
		cs = pdu;
		cs->fd = htobe64(cs->fd);
		cs->client_port =
		    htobe16(cs->client_port);
		cs->connect_time =
		    htobe64(cs->connect_time);
		cs->total_bytes_in =
		    htobe64(cs->total_bytes_in);
		cs->total_bytes_out =
		    htobe64(cs->total_bytes_out);
		cs->sendq_length =
		    htobe64(cs->sendq_length);
		cs->sendq_max =
		    htobe64(cs->sendq_max);
		cs->vrp4_announcements =
		    htobe64(cs->vrp4_announcements);
		cs->vrp4_withdrawals =
		    htobe64(cs->vrp4_withdrawals);
		cs->vrp4_advertised =
		    htobe64(cs->vrp4_advertised);
		cs->vrp6_announcements =
		    htobe64(cs->vrp6_announcements);
		cs->vrp6_withdrawals =
		    htobe64(cs->vrp6_withdrawals);
		cs->vrp6_advertised =
		    htobe64(cs->vrp6_advertised);
		cs->brk_announcements =
		    htobe64(cs->brk_announcements);
		cs->brk_withdrawals =
		    htobe64(cs->brk_withdrawals);
		cs->brk_advertised =
		    htobe64(cs->brk_advertised);
		cs->vap_announcements =
		    htobe64(cs->vap_announcements);
		cs->vap_withdrawals =
		    htobe64(cs->vap_withdrawals);
		cs->vap_advertised =
		    htobe64(cs->vap_advertised);
		cs->reset_query_count =
		    htobe64(cs->reset_query_count);
		cs->serial_query_count =
		    htobe64(cs->serial_query_count);
		break;
	case CACHE_FRAME_STATS:
		cfs = pdu;
		cfs->cache_session_id =
		    htobe16(cfs->cache_session_id);
		cfs->cache_serial =
		    htobe32(cfs->cache_serial);
		cfs->vrp4_count =
		    htobe64(cfs->vrp4_count);
		cfs->vrp4_creation_time =
		    htobe64(cfs->vrp4_creation_time);
		cfs->vrp6_count =
		    htobe64(cfs->vrp6_count);
		cfs->vrp6_creation_time =
		    htobe64(cfs->vrp6_creation_time);
		cfs->brk_count =
		    htobe64(cfs->brk_count);
		cfs->brk_creation_time =
		    htobe64(cfs->brk_creation_time);
		cfs->vap_count =
		    htobe64(cfs->vap_count);
		cfs->vap_creation_time =
		    htobe64(cfs->vap_creation_time);
		break;
	/* case END_OF_STATS: */
		/* break; */
	}
}

void
pdu_ntoh_controller(void *pdu)
{
	struct pdu_header *ph;
	struct pdu_ipv4_prefix_import *vrp4;
	struct pdu_ipv6_prefix_import *vrp6;
	struct pdu_router_key_import *rk;
	struct pdu_aspa_import *aspa;
	struct pdu_global_stats *gs;
	struct pdu_client_stats *cs;
	struct pdu_cache_frame_stats *cfs;
	struct pdu_open_controller *oc;
	int provider_count;
	int i;

	assert(pdu);
	ph = pdu;

	ph->length = be32toh(ph->length);

	switch (ph->type) {
	case OPEN_CONTROLLER:
		oc = pdu;
		oc->controller_version = be32toh(oc->controller_version);
		oc->controller_flags   = be32toh(oc->controller_flags);
		break;
	/* case CLOSE_CONTROLLER: */
		/* break; */
	/* case START_OF_IMPORT: */
		/* break; */
	case IPV4_PREFIX_IMPORT:
		vrp4 = pdu;
		vrp4->expire = be64toh(vrp4->expire);
		vrp4->asn    = be32toh(vrp4->asn);
		break;
	case IPV6_PREFIX_IMPORT:
		vrp6 = pdu;
		vrp6->expire = be64toh(vrp6->expire);
		vrp6->asn = be32toh(vrp6->asn);
		break;
	case ROUTER_KEY_IMPORT:
		rk = pdu;
		rk->expire = be64toh(rk->expire);
		rk->asn    = be32toh(rk->asn);
		break;
	case ASPA_PDU_IMPORT:
		aspa = pdu;
		aspa->expire = be64toh(aspa->expire);
		aspa->customer_asn = be32toh(aspa->customer_asn);
		provider_count =
		    (aspa->length - sizeof(struct pdu_aspa)) /
		    sizeof(uint32_t);
		for (i = 0; i < provider_count; i++) {
			aspa->provider_asns[i] =
			    be32toh(aspa->provider_asns[i]);
		}
		break;
	/* case END_OF_IMPORT: */
		/* break; */
	/* case QUERY_STATS: */
		/* break; */
	/* case START_OF_STATS: */
		/* break; */
	case GLOBAL_STATS:
		gs = pdu;
		gs->start_time =
		    be64toh(gs->start_time);
		gs->total_bytes_in =
		    be64toh(gs->total_bytes_in);
		gs->total_bytes_out =
		    be64toh(gs->total_bytes_out);
		gs->total_client_connects =
		    be64toh(gs->total_client_connects);
		gs->total_controller_connects =
		    be64toh(gs->total_controller_connects);
		break;
	case CLIENT_STATS:
		cs = pdu;
		cs->fd = be64toh(cs->fd);
		cs->client_port =
		    be16toh(cs->client_port);
		cs->connect_time =
		    be64toh(cs->connect_time);
		cs->total_bytes_in =
		    be64toh(cs->total_bytes_in);
		cs->total_bytes_out =
		    be64toh(cs->total_bytes_out);
		cs->sendq_length =
		    be64toh(cs->sendq_length);
		cs->sendq_max =
		    be64toh(cs->sendq_max);
		cs->vrp4_announcements =
		    be64toh(cs->vrp4_announcements);
		cs->vrp4_withdrawals =
		    be64toh(cs->vrp4_withdrawals);
		cs->vrp4_advertised =
		    be64toh(cs->vrp4_advertised);
		cs->vrp6_announcements =
		    be64toh(cs->vrp6_announcements);
		cs->vrp6_withdrawals =
		    be64toh(cs->vrp6_withdrawals);
		cs->vrp6_advertised =
		    be64toh(cs->vrp6_advertised);
		cs->brk_announcements =
		    be64toh(cs->brk_announcements);
		cs->brk_withdrawals =
		    be64toh(cs->brk_withdrawals);
		cs->brk_advertised =
		    be64toh(cs->brk_advertised);
		cs->vap_announcements =
		    be64toh(cs->vap_announcements);
		cs->vap_withdrawals =
		    be64toh(cs->vap_withdrawals);
		cs->vap_advertised =
		    be64toh(cs->vap_advertised);
		cs->reset_query_count =
		    be64toh(cs->reset_query_count);
		cs->serial_query_count =
		    be64toh(cs->serial_query_count);
		break;
	case CACHE_FRAME_STATS:
		cfs = pdu;
		cfs->cache_session_id =
		    be16toh(cfs->cache_session_id);
		cfs->cache_serial =
		    be32toh(cfs->cache_serial);
		cfs->vrp4_count =
		    be64toh(cfs->vrp4_count);
		cfs->vrp4_creation_time =
		    be64toh(cfs->vrp4_creation_time);
		cfs->vrp6_count =
		    be64toh(cfs->vrp6_count);
		cfs->vrp6_creation_time =
		    be64toh(cfs->vrp6_creation_time);
		cfs->brk_count =
		    be64toh(cfs->brk_count);
		cfs->brk_creation_time =
		    be64toh(cfs->brk_creation_time);
		cfs->vap_count =
		    be64toh(cfs->vap_count);
		cfs->vap_creation_time =
		    be64toh(cfs->vap_creation_time);
		break;
	/* case END_OF_STATS: */
		/* break; */
	}
}

#define IPV4_PREFIX_BITS	(sizeof(struct in_addr) * 8)

/* 1 on fail */

int
check_pdu_ipv4_prefix_controller(struct pdu_ipv4_prefix_import *pdu)
{
	struct in_addr network;

	assert(pdu);

	if (pdu->prefix_length > IPV4_PREFIX_BITS)
		return 1;

	if (pdu->max_prefix_length > IPV4_PREFIX_BITS)
		return 1;

	if (pdu->prefix_length > pdu->max_prefix_length)
		return 1;

	network = address_to_network4(pdu->prefix, pdu->prefix_length);

	if (memcmp(&pdu->prefix, &network, sizeof(struct in_addr)) != 0)
		return 1;

	return 0;
}

#define IPV6_PREFIX_BITS	(sizeof(struct in6_addr) * 8)

/* 1 on fail */

int
check_pdu_ipv6_prefix_controller(struct pdu_ipv6_prefix_import *pdu)
{
	struct in6_addr network;

	assert(pdu);

	if (pdu->prefix_length > IPV6_PREFIX_BITS)
		return 1;

	if (pdu->max_prefix_length > IPV6_PREFIX_BITS)
		return 1;

	if (pdu->prefix_length > pdu->max_prefix_length)
		return 1;

	network = address_to_network6(pdu->prefix, pdu->prefix_length);

	if (memcmp(&pdu->prefix, &network, sizeof(struct in6_addr)) != 0)
		return 1;

	return 0;
}

/* 1 on fail */

int
check_pdu_router_key_controller(struct pdu_router_key_import *pdu)
{
	int spki_length;

	assert(pdu);

	if (pdu->asn == 0)
		return 1;

	spki_length = pdu->length - sizeof(struct pdu_router_key_import);

	if (spki_length != SPKI_LENGTH_P256)
		return 1;

	return 0;
}

/* 1 on fail */

int
check_pdu_aspa_controller(struct pdu_aspa_import *pdu)
{
	int i;
	int provider_count;
	int provider_length;

	assert(pdu);

	if (pdu->customer_asn == 0)
		return 1;

	provider_length = pdu->length - sizeof(*pdu);

	if ((provider_length % sizeof(uint32_t)) != 0)
		return 1;

	provider_count = provider_length / sizeof(uint32_t);

	for (i = 0; i < provider_count; i++) {
		if (pdu->customer_asn == pdu->provider_asns[i])
			return 1;
	}

	return 0;
}

/* 1 on fail */

int
check_error_code(uint16_t error_code, uint8_t version)
{
	switch (version) {
	case RTR_VERSION_0:
		if (error_code <= DUPLICATE_ANNOUNCEMENT_RECEIVED)
			return 0;
		break;
	case RTR_VERSION_1:
		if (error_code <= UNEXPECTED_PROTOCOL_VERSION)
			return 0;
		break;
	case RTR_VERSION_2:
		if (error_code <= CACHE_SHUTDOWN)
			return 0;
		break;
	}

	return 1;
}
