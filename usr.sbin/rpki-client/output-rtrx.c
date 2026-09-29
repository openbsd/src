/*	$OpenBSD: output-rtrx.c,v 1.2 2026/09/29 22:01:58 deraadt Exp $	*/
/*
 * Copyright (c) 2026 Ralph Covelli <rcovelli@he.net>
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

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define RTR_VERSION		2

#define PDU_MAX_LENGTH		65535
#define PDU_HEADER_LENGTH	8

int rtrx_sock = -1;
char *rtrx_filename = "/var/run/rtrd.sock";

enum pdu_type {
	OPEN_CONTROLLER		= 128,
	CLOSE_CONTROLLER	= 129,
	START_OF_IMPORT		= 130,
	IPV4_PREFIX_IMPORT	= 131,
	IPV6_PREFIX_IMPORT	= 132,
	ROUTER_KEY_IMPORT	= 133,
	ASPA_PDU_IMPORT		= 134,
	END_OF_IMPORT		= 135,
	PUSH_IMPORT		= 136
};

struct pdu_header {
	uint8_t ver;
	uint8_t type;
	uint16_t reserved;
	uint32_t len;
} __packed;

struct pdu_open_controller {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	uint32_t controller_version;
	uint32_t controller_flags;
} __packed;

struct pdu_close_controller {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} __packed;

struct pdu_start_of_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} __packed;

struct pdu_ipv4_prefix_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	time_t expire;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in_addr prefix;
	uint32_t asn;
} __packed;

struct pdu_ipv6_prefix_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	time_t expire;
	uint8_t flags;
	uint8_t prefix_length;
	uint8_t max_prefix_length;
	uint8_t zero;
	struct in6_addr prefix;
	uint32_t asn;
} __packed;

#define SKI_LENGTH 20
#define SPKI_LENGTH_P256 91

struct pdu_router_key_import
{
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	time_t expire;
	unsigned char ski[SKI_LENGTH];
	uint32_t asn;
	unsigned char spki[];
} __packed;

#define VAP_MAX_PROVIDERS	16378	/* 65532 bytes */

struct pdu_aspa_import {
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	time_t expire;
	uint32_t customer_asn;
	uint32_t provider_asns[];
} __packed;

struct pdu_end_of_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} __packed;

struct pdu_push_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} __packed;

int rtrx_write(char *, int);
int rtrx_read(char *, int);
int rtrx_open_controller(void);
int rtrx_close_controller(void);
int rtrx_start_of_import(void);
int rtrx_vrp_import(struct vrp *);
int rtrx_brk_import(struct brk *);
int rtrx_vap_import(struct vap *);
int rtrx_end_of_import(void);
int rtrx_push_import(void);

int
rtrx_write(char *buf, int size)
{
	int offset = 0, ret;

	if (rtrx_sock < 0)
		return -1;
	if (size > PDU_MAX_LENGTH)
		return 0;

	while (offset < size) {
		ret = write(rtrx_sock, buf + offset, size - offset);
		if (ret <= 0) {
			close(rtrx_sock);
			rtrx_sock = -1;
			return -1;
		}
		offset += ret;
	}
	return size;
}

int
rtrx_read(char *buf, int size)
{
	int offset = 0, ret;

	if (rtrx_sock < 0)
		return -1;
	if (size > PDU_MAX_LENGTH)
		return 0;

	while (offset < size) {
		ret = read(rtrx_sock, buf + offset, size - offset);
		if (ret <= 0) {
			close(rtrx_sock);
			rtrx_sock = -1;
			return -1;
		}
		offset += ret;
	}
	return size;
}

void
rtrx_connect(void)
{
	struct sockaddr_un  serv_addr;
	int sockfd, val;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		return;

	val = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, val | O_NONBLOCK);

	memset(&serv_addr, 0, sizeof serv_addr);

	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, rtrx_filename,
	    sizeof(serv_addr.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr *) &serv_addr,
	    sizeof serv_addr) < 0) {
		close(sockfd);
		return;
	}

	rtrx_sock = sockfd;

	return;
}

int
rtrx_open_controller(void)
{
	struct pdu_open_controller oc;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	oc.version = RTR_VERSION;
	oc.type = OPEN_CONTROLLER;
	oc.reserved = 0;
	length = sizeof(struct pdu_open_controller);
	oc.length = length;
	oc.controller_version = 1;
	oc.controller_flags = 1;
	oc.reserved = htobe16(oc.reserved);
	oc.length = htobe32(oc.length);
	oc.controller_version = htobe32(oc.controller_version);
	oc.controller_flags = htobe32(oc.controller_flags);

	if (rtrx_write((char *)&oc, length) < 0)
		return -1;

	return 0;
}

int
rtrx_close_controller(void)
{
	struct pdu_close_controller cc;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	cc.version = RTR_VERSION;
	cc.type = CLOSE_CONTROLLER;
	cc.reserved = 0;
	length = sizeof(struct pdu_close_controller);
	cc.length = length;
	cc.reserved = htobe16(cc.reserved);
	cc.length = htobe32(cc.length);

	if (rtrx_write((char *)&cc, length) < 0)
		return -1;

	return 0;
}

int
rtrx_start_of_import(void)
{
	struct pdu_start_of_import soi;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	soi.version = RTR_VERSION;
	soi.type = START_OF_IMPORT;
	soi.reserved = 0;
	length = sizeof(struct pdu_start_of_import);
	soi.length = length;
	soi.reserved = htobe16(soi.reserved);
	soi.length = htobe32(soi.length);

	if (rtrx_write((char *)&soi, length) < 0)
		return -1;

	return 0;
}

int
rtrx_vrp_import(struct vrp *rpki_vrp)
{
	struct pdu_ipv4_prefix_import ip4;
	struct pdu_ipv6_prefix_import ip6;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	if (rpki_vrp->afi == AFI_IPV4) {
		ip4.version = RTR_VERSION;
		ip4.type = IPV4_PREFIX_IMPORT;
		ip4.reserved = 0;
		length = sizeof(struct pdu_ipv4_prefix_import);
		ip4.length = length;
		ip4.expire = rpki_vrp->expires;
		ip4.flags = 1;
		ip4.prefix_length = rpki_vrp->addr.prefixlen;
		ip4.max_prefix_length = rpki_vrp->maxlength;
		ip4.zero = 0;
		ip4.prefix = *(struct in_addr *)rpki_vrp->addr.addr;
		ip4.asn = rpki_vrp->asid;
		ip4.reserved = htobe16(ip4.reserved);
		ip4.length   = htobe32(ip4.length);
		ip4.expire   = htobe64(ip4.expire);
		ip4.asn      = htobe32(ip4.asn);

		if (rtrx_write((char *)&ip4, length) < 0)
			return -1;
	}
	if (rpki_vrp->afi == AFI_IPV6) {
		ip6.version = RTR_VERSION;
		ip6.type = IPV6_PREFIX_IMPORT;
		ip6.reserved = 0;
		length = sizeof(struct pdu_ipv6_prefix_import);
		ip6.length = length;
		ip6.expire = rpki_vrp->expires;
		ip6.flags = 1;
		ip6.prefix_length = rpki_vrp->addr.prefixlen;
		ip6.max_prefix_length = rpki_vrp->maxlength;
		ip6.zero = 0;
		ip6.prefix = *(struct in6_addr *)rpki_vrp->addr.addr;
		ip6.asn = rpki_vrp->asid;
		ip6.reserved = htobe16(ip6.reserved);
		ip6.length   = htobe32(ip6.length);
		ip6.expire   = htobe64(ip6.expire);
		ip6.asn      = htobe32(ip6.asn);

		if (rtrx_write((char *)&ip6, length) < 0)
			return -1;
	}

	return 0;
}

int
rtrx_brk_import(struct brk *rpki_brk)
{
	struct pdu_router_key_import *rkey;
	uint32_t length;
	unsigned char buf[PDU_MAX_LENGTH];
	unsigned char *pk_der = NULL;
	size_t pk_len;

	if (rtrx_sock < 0)
		return -1;

	rkey = (struct pdu_router_key_import *)&buf;

	rkey->version = RTR_VERSION;
	rkey->type = ROUTER_KEY_IMPORT;
	rkey->flags = 1;
	rkey->zero = 0;
	length = sizeof(struct pdu_router_key_import);
	rkey->expire = rpki_brk->expires;
	if (hex_decode(rpki_brk->ski, rkey->ski, SKI_LENGTH) == -1)
		return 0;
	rkey->asn = rpki_brk->asid;
	if (base64_decode(rpki_brk->pubkey, strlen(rpki_brk->pubkey),
	    &pk_der, &pk_len) == -1)
		return 0;
	if (pk_len >
	    (PDU_MAX_LENGTH - sizeof(struct pdu_router_key_import))) {
		free(pk_der);
		return 0;
	}
	memcpy(rkey->spki, pk_der, pk_len);
	length += pk_len;
	rkey->length = length;
	free(pk_der);
	rkey->length = htobe32(rkey->length);
	rkey->expire = htobe64(rkey->expire);
	rkey->asn = htobe32(rkey->asn);

	if (rtrx_write((char *)rkey, length) < 0)
		return -1;

	return 0;
}

int
rtrx_vap_import(struct vap *rpki_vap)
{
	struct pdu_aspa_import *aspa;
	uint32_t length;
	unsigned char buf[PDU_MAX_LENGTH];
	uint32_t p_count;
	size_t i;

	if (rtrx_sock < 0)
		return -1;

	if (rpki_vap->overflowed)
		return 0;

	aspa = (struct pdu_aspa_import *)&buf;

	aspa->version = RTR_VERSION;
	aspa->type = ASPA_PDU_IMPORT;
	aspa->flags = 1;
	aspa->zero = 0;
	length = sizeof(struct pdu_aspa_import);
	aspa->expire = rpki_vap->expires;
	aspa->customer_asn = rpki_vap->custasid;
	aspa->expire = htobe64(aspa->expire);
	aspa->customer_asn = htobe32(aspa->customer_asn);
	p_count = rpki_vap->num_providers > VAP_MAX_PROVIDERS ?
	    VAP_MAX_PROVIDERS : rpki_vap->num_providers;
	for (i = 0; i < p_count; i++) {
		aspa->provider_asns[i] = rpki_vap->providers[i];
		aspa->provider_asns[i] = htobe32(aspa->provider_asns[i]);
	}
	length += (p_count * sizeof(uint32_t));
	aspa->length = length;
	aspa->length = htobe32(aspa->length);

	if (rtrx_write((char *)aspa, length) < 0)
		return -1;

	return 0;
}

int
rtrx_end_of_import(void)
{
	struct pdu_end_of_import eoi;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	eoi.version = RTR_VERSION;
	eoi.type = END_OF_IMPORT;
	eoi.reserved = 0;
	length = sizeof(struct pdu_end_of_import);
	eoi.length = length;
	eoi.reserved = htobe16(eoi.reserved);
	eoi.length = htobe32(eoi.length);

	if (rtrx_write((char *)&eoi, length) < 0)
		return -1;

	return 0;
}

int
rtrx_push_import(void)
{
	struct pdu_push_import pi;
	uint32_t length;

	if (rtrx_sock < 0)
		return -1;

	pi.version = RTR_VERSION;
	pi.type = PUSH_IMPORT;
	pi.reserved = 0;
	length = sizeof(struct pdu_push_import);
	pi.length = length;
	pi.reserved = htobe16(pi.reserved);
	pi.length = htobe32(pi.length);

	if (rtrx_write((char *)&pi, length) < 0)
		return -1;

	return 0;
}

int
output_rtrx(FILE *out, struct validation_data *vd, struct stats *st)
{
	struct vrp	*rpki_vrp;
	struct brk	*rpki_brk;
	struct vap	*rpki_vap;
	int val;

	if (rtrx_sock < 0)
		return -1;

	val = fcntl(rtrx_sock, F_GETFL, 0);
	fcntl(rtrx_sock, F_SETFL, val & ~O_NONBLOCK);

	if (rtrx_open_controller() < 0)
		return -1;

	if (rtrx_start_of_import() < 0)
		return -1;

	RB_FOREACH(rpki_vrp, vrp_tree, &vd->vrps) {
		if (rtrx_vrp_import(rpki_vrp) < 0)
			return -1;
	}

	RB_FOREACH(rpki_brk, brk_tree, &vd->brks) {
		if (rtrx_brk_import(rpki_brk) < 0)
			return -1;
	}

	if (!excludeaspa) {
		RB_FOREACH(rpki_vap, vap_tree, &vd->vaps) {
			if (rtrx_vap_import(rpki_vap) < 0)
				return -1;
		}
	}

	if (rtrx_end_of_import() < 0)
		return -1;

	close(rtrx_sock);
	rtrx_sock = -1;

	return 0;
}
