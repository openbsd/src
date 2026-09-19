/*	$OpenBSD: rtrctl.c,v 1.5 2026/09/19 17:23:52 schwarze Exp $ */
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
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtr_config.h"
#include "ometric.h"

static const char * const state_names[] = {
	"Open",
	"Established",
	"Open-Closing",
	"Closing"
};

int rtr_write(int, char *, int);
int rtr_read(int, char *, int);
int connect_socket(char *);
void import_start(int);
void import_end(int);
void read_openbgpd(FILE *, int);
void read_sock_print_ometric(int);

void process_import(char *, char *);
void process_stats(char *);

#define PACKED __attribute__((packed))

#define RTR_VERSION		2

#define PDU_MAX_LENGTH		65535
#define PDU_HEADER_LENGTH	8

enum pdu_type {
	OPEN_CONTROLLER		= 128,
	CLOSE_CONTROLLER	= 129,
	START_OF_IMPORT		= 130,
	IPV4_PREFIX_IMPORT	= 131,
	IPV6_PREFIX_IMPORT	= 132,
	ROUTER_KEY_IMPORT	= 133,
	ASPA_PDU_IMPORT		= 134,
	END_OF_IMPORT		= 135,
	PUSH_IMPORT		= 136,
	QUERY_STATS		= 137,
	START_OF_STATS		= 138,
	GLOBAL_STATS		= 139,
	CLIENT_STATS		= 140,
	CACHE_FRAME_STATS	= 141,
	END_OF_STATS		= 142,
	PDU_RTRX_MAX
};

struct pdu_header {
	uint8_t ver;
	uint8_t type;
	uint16_t reserved;
	uint32_t len;
} PACKED;

struct pdu_open_controller {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	uint32_t controller_version;
	uint32_t controller_flags;
} PACKED;

struct pdu_start_of_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

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
} PACKED;

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
} PACKED;

#define VAP_MAX_PROVIDERS       16378	/* 65532 bytes */
struct pdu_aspa_import {
	uint8_t version;
	uint8_t type;
	uint8_t flags;
	uint8_t zero;
	uint32_t length;
	time_t expire;
	uint32_t customer_asn;
	uint32_t provider_asns[];
} PACKED;

struct pdu_end_of_import {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_query_stats {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pdu_start_of_stats {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

#define NODENAME_SIZE   256
#define DOMAINNAME_SIZE 256
#define RELEASE_SIZE    64

struct pdu_global_stats {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	char nodename[NODENAME_SIZE];
	char domainname[DOMAINNAME_SIZE];
	char release[RELEASE_SIZE];
	time_t start_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t total_client_connects;
	int64_t total_controller_connects;
} PACKED;

#define CLIENT_STATE_REGISTERED 0x01
#define CLIENT_STATE_CLOSED     0x02

struct pdu_client_stats {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
	int64_t fd;
	struct in_addr client_ip;
	uint16_t client_port;
	uint8_t client_version;
	uint8_t client_state;
	time_t connect_time;
	int64_t total_bytes_in;
	int64_t total_bytes_out;
	int64_t sendq_length;
	int64_t sendq_max;
	int64_t vrp4_announcements;
	int64_t vrp4_withdrawals;
	int64_t vrp4_advertised;
	int64_t vrp6_announcements;
	int64_t vrp6_withdrawals;
	int64_t vrp6_advertised;
	int64_t brk_announcements;
	int64_t brk_withdrawals;
	int64_t brk_advertised;
	int64_t vap_announcements;
	int64_t vap_withdrawals;
	int64_t vap_advertised;
	int64_t reset_query_count;
	int64_t serial_query_count;
} PACKED;

struct pdu_cache_frame_stats {
	uint8_t version;
	uint8_t type;
	uint8_t reserved;
	uint8_t flags;
	uint32_t length;
	uint8_t cache_version;
	uint8_t zero;
	uint16_t cache_session_id;
	uint32_t cache_serial;
	int64_t vrp4_count;
	time_t vrp4_creation_time;
	int64_t vrp6_count;
	time_t vrp6_creation_time;
	int64_t brk_count;
	time_t brk_creation_time;
	int64_t vap_count;
	time_t vap_creation_time;
} PACKED;

struct pdu_end_of_stats {
	uint8_t version;
	uint8_t type;
	uint16_t reserved;
	uint32_t length;
} PACKED;

struct pollfd pfd;

int
rtr_write(int sock, char *buf, int size)
{
	int offset = 0, ret;

	if (size > PDU_MAX_LENGTH)
		return  -1;

	while (offset < size) {
		ret = write(sock, buf + offset, size - offset);
		if (ret < 0) {
			printf("write error (%s)\n", strerror(errno));
			close(sock);
			exit(0);
		}
		if (!ret) {
			printf("write eof\n");
			close(sock);
			exit(0);
		}
		offset += ret;
	}
	return size;
}

int
rtr_read(int sock, char *buf, int size)
{
	int offset = 0, ret;

	if (size > PDU_MAX_LENGTH)
		return  -1;

	while (offset < size) {
		ret = read(sock, buf+offset, size-offset);
		if (ret < 0) {
			printf("read error (%s)\n", strerror(errno));
			close(sock);
			exit(0);
		}
		if (!ret) {
			printf("read eof\n");
			close(sock);
			exit(0);
		}
		offset += ret;
	}
	return size;
}

int
connect_socket(char *filename)
{
	struct sockaddr_un  serv_addr;
	int sockfd;

	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0)
		return -1;

	memset(&serv_addr, 0, sizeof serv_addr);

	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, filename, sizeof(serv_addr.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr *) &serv_addr,
	    sizeof serv_addr) < 0) {
		close(sockfd);
		return -1;
	}

	pfd.fd = sockfd;
	pfd.events |= POLLIN;
	return sockfd;
}

#define check_token(v,s) do {                                  \
	if ((v) == NULL) {                                     \
		fprintf(stderr, "unable to parse %s\n", (s));  \
		exit(1);                                       \
	}                                                      \
} while (0)

#define MAXLINE 2097152

void
import_start(int sock)
{
	struct pdu_open_controller oc;
	struct pdu_start_of_import soi;
	uint32_t length;

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
	rtr_write(sock, (char *)&oc, length);

	soi.version = RTR_VERSION;
	soi.type = START_OF_IMPORT;
	soi.reserved = 0;
	length = sizeof(struct pdu_start_of_import);
	soi.length = length;
	soi.reserved = htobe16(soi.reserved);
	soi.length = htobe32(soi.length);
	rtr_write(sock, (char *)&soi, length);
}

void
import_end(int sock)
{
	struct pdu_end_of_import eoi;
	uint32_t length;

	eoi.version = RTR_VERSION;
	eoi.type = END_OF_IMPORT;
	eoi.reserved = 0;
	length = sizeof(struct pdu_end_of_import);
	eoi.length = length;
	eoi.reserved = htobe16(eoi.reserved);
	eoi.length = htobe32(eoi.length);
	rtr_write(sock, (char *)&eoi, length);
}

void
read_openbgpd(FILE *fp, int sock)
{
	struct pdu_ipv4_prefix_import ip4;
	struct pdu_ipv6_prefix_import ip6;
	struct pdu_aspa_import *aspa;
	uint32_t length;
	char *s_asn;
	char *s_cidr;
	char *s_prefix;
	char *s_len;
	char *s_maxlen;
	char *s_expire;
	char *type;
	char *s_customer_asn;
	char *s_provider_asn;
	unsigned char buf[PDU_MAX_LENGTH];
	char line[MAXLINE];
	uint32_t p_count;

	while (fgets(line, MAXLINE, fp)) {
		if (line[0] == '#')
			continue;
		if (strcmp(line, "roa-set {\n") == 0)
			break;
	}

	while (fgets(line, MAXLINE, fp)) {
		if (line[0] == '#')
			continue;
		if (strcmp(line, "}\n") == 0)
			break;

		s_cidr = strtok(line, " \t\n");
		check_token(s_cidr, "cidr");

		type = strtok(NULL, " \t\n");
		check_token(type, "type");

		if (strcmp(type, "maxlen") == 0) {
			s_maxlen = strtok(NULL, " \t\n");
			check_token(s_maxlen, "maxlen");

			strtok(NULL, " \t\n");

			s_asn = strtok(NULL, " \t\n");
			check_token(s_asn, "asn");

			strtok(NULL, " \t\n");

			s_expire = strtok(NULL, " \t\n");
			check_token(s_expire, "expire");

			s_prefix = strtok(s_cidr, "/");
			check_token(s_prefix, "prefix");

			s_len = strtok(NULL, "/");
			check_token(s_len, "len");
		} else {
			s_asn = strtok(NULL, " \t\n");
			check_token(s_asn, "asn");

			strtok(NULL, " \t\n");

			s_expire = strtok(NULL, " \t\n");
			check_token(s_expire, "expire");

			s_prefix = strtok(s_cidr, "/");
			check_token(s_prefix, "prefix");

			s_len = strtok(NULL, "/");
			check_token(s_len, "len");

			s_maxlen = s_len;
		}

		if (strchr(s_prefix, '.')) {
			if (inet_pton(AF_INET, s_prefix, &ip4.prefix) == 1) {
				ip4.version = RTR_VERSION;
				ip4.type = IPV4_PREFIX_IMPORT;
				ip4.reserved = 0;
				length = sizeof(struct pdu_ipv4_prefix_import);
				ip4.length = length;
				ip4.expire = atoll(s_expire);
				ip4.flags = 1;
				ip4.prefix_length = atoi(s_len);
				ip4.max_prefix_length = atoi(s_maxlen);
				ip4.zero = 0;
				ip4.asn = atol(s_asn);

				ip4.reserved = htobe16(ip4.reserved);
				ip4.length   = htobe32(ip4.length);
				ip4.expire   = htobe64(ip4.expire);
				ip4.asn      = htobe32(ip4.asn);

				rtr_write(sock, (char *)&ip4, length);
			}
		}

		if (strchr(s_prefix, ':')) {
			if (inet_pton(AF_INET6, s_prefix, &ip6.prefix) == 1) {
				ip6.version = RTR_VERSION;
				ip6.type = IPV6_PREFIX_IMPORT;
				ip6.reserved = 0;
				length = sizeof(struct pdu_ipv6_prefix_import);
				ip6.length = length;
				ip6.expire = atoll(s_expire);
				ip6.flags = 1;
				ip6.prefix_length = atoi(s_len);
				ip6.max_prefix_length = atoi(s_maxlen);
				ip6.zero = 0;
				ip6.asn = atol(s_asn);

				ip6.reserved = htobe16(ip6.reserved);
				ip6.length   = htobe32(ip6.length);
				ip6.expire   = htobe64(ip6.expire);
				ip6.asn      = htobe32(ip6.asn);

				rtr_write(sock, (char *)&ip6, length);
			}
		}
	}

	aspa = (struct pdu_aspa_import *)&buf;

	while (fgets(line, MAXLINE, fp)) {
		if (line[0] == '#')
			continue;

		if (strcmp(line, "aspa-set {\n") == 0)
			break;

	}

	while (fgets(line, MAXLINE, fp)) {
		if (line[0] == '#')
			continue;

		if (strcmp(line, "}\n") == 0)
			break;

		strtok(line, " \t\n");

		s_customer_asn = strtok(NULL, " \t\n");
		check_token(s_customer_asn, "customer_asn");

		strtok(NULL, " \t\n");

		s_expire = strtok(NULL, " \t\n");
		check_token(s_expire, "expire");

		strtok(NULL, " \t\n");
		strtok(NULL, " \t\n");

		aspa->version = RTR_VERSION;
		aspa->type = ASPA_PDU_IMPORT;
		aspa->flags = 1;
		aspa->zero = 0;
		length = sizeof(struct pdu_aspa_import);
		aspa->expire = atoll(s_expire);
		aspa->customer_asn = atol(s_customer_asn);

		aspa->expire       = htobe64(aspa->expire);
		aspa->customer_asn = htobe32(aspa->customer_asn);

		p_count = 0;
		while ((s_provider_asn = strtok(NULL, ", \t\n")) != NULL) {
			if (strcmp(s_provider_asn, "}") == 0) {
				if (p_count > 0) {
					aspa->length = length;

					aspa->length = htobe32(aspa->length);

					rtr_write(sock, (char *)aspa, length);
				}
				break;
			}
			if (p_count < VAP_MAX_PROVIDERS) {
				aspa->provider_asns[p_count] =
				    atol(s_provider_asn);

				aspa->provider_asns[p_count] =
				    htobe32(aspa->provider_asns[p_count]);

				length += sizeof(uint32_t);
				p_count++;
			}
		}

	}

	while (fgets(line, MAXLINE, fp)) {
		if (line[0] == '#')
			continue;
	}

}

#define BUF_SIZE 32

void
read_sock_print_ometric(int sock)
{
	struct pdu_header *h;

	struct pdu_global_stats *gs;
	struct pdu_client_stats *cs;
	struct pdu_cache_frame_stats *cfs;

	char packet[PDU_MAX_LENGTH];

	struct pdu_open_controller oc;

	struct pdu_header pduout;

	struct timespec ts;

	struct ometric *rtrd_info;
	struct ometric *rtrd_start_time;
	struct ometric *rtrd_bytes_in;
	struct ometric *rtrd_bytes_out;
	struct ometric *rtrd_client_connects;
	struct ometric *rtrd_controller_connects;

	struct ometric *rtrd_client;
	struct ometric *rtrd_client_state;
	struct ometric *rtrd_client_connect_time;
	struct ometric *rtrd_client_version;
	struct ometric *rtrd_client_bytes_in;
	struct ometric *rtrd_client_bytes_out;
	struct ometric *rtrd_client_sendq_length;
	struct ometric *rtrd_client_sendq_max;
	struct ometric *rtrd_client_vrp4_announcements;
	struct ometric *rtrd_client_vrp4_withdrawals;
	struct ometric *rtrd_client_vrp4_advertised;
	struct ometric *rtrd_client_vrp6_announcements;
	struct ometric *rtrd_client_vrp6_withdrawals;
	struct ometric *rtrd_client_vrp6_advertised;
	struct ometric *rtrd_client_brk_announcements;
	struct ometric *rtrd_client_brk_withdrawals;
	struct ometric *rtrd_client_brk_advertised;
	struct ometric *rtrd_client_vap_announcements;
	struct ometric *rtrd_client_vap_withdrawals;
	struct ometric *rtrd_client_vap_advertised;
	struct ometric *rtrd_client_reset_queries;
	struct ometric *rtrd_client_serial_queries;

	struct ometric *rtrd_cache;
	struct ometric *rtrd_cache_serial;
	struct ometric *rtrd_cache_vrp4;
	struct ometric *rtrd_cache_vrp6;
	struct ometric *rtrd_cache_brk;
	struct ometric *rtrd_cache_vap;

	struct olabels *ol;
	const char *gkeys[4] = { "nodename", "domainname", "release", NULL };
	const char *gvalues[4];

	const char *ckeys[3] = { "version", "session_id", NULL };
	const char *cvalues[3];

	const char *clikeys[3] = { "remote_ip", "remote_port", NULL };
	const char *clivalues[3];

	char version[BUF_SIZE];
	char session_id[BUF_SIZE];

	char ip[BUF_SIZE];
	char port[BUF_SIZE];

	int done;

	done = 0;

	oc.version = RTR_VERSION;
	oc.type = OPEN_CONTROLLER;
	oc.reserved = 0;
	oc.length = sizeof(struct pdu_open_controller);
	oc.controller_version = 1;
	oc.controller_flags = 2;

	oc.reserved            = htobe16(oc.reserved);
	oc.length              = htobe32(oc.length);
	oc.controller_version  = htobe32(oc.controller_version);
	oc.controller_flags    = htobe32(oc.controller_flags);

	rtr_write(sock, (char *)&oc, sizeof(struct pdu_open_controller));

	pduout.ver = RTR_VERSION;
	pduout.type = QUERY_STATS;
	pduout.reserved = 0;
	pduout.len = htobe32(PDU_HEADER_LENGTH);

	rtr_write(sock, (char *)&pduout, PDU_HEADER_LENGTH);

	rtrd_info = ometric_new(OMT_INFO, "rtrd", "rtrd information");
	rtrd_start_time = ometric_new(OMT_GAUGE, "rtrd_start_time",
	    "start of this server instance as epoch timestamp");
	rtrd_bytes_in = ometric_new(OMT_COUNTER,
	    "rtrd_bytes_in",
	    "per byte type count of received messages");
	rtrd_bytes_out = ometric_new(OMT_COUNTER,
	    "rtrd_bytes_out",
	    "per byte type count of transmitted messages");
	rtrd_client_connects = ometric_new(OMT_COUNTER,
	    "rtrd_client_connects",
	    "per session type count of client connections");
	rtrd_controller_connects = ometric_new(OMT_COUNTER,
	    "rtrd_controller_connects",
	    "per session type count of controller connections");

	rtrd_client = ometric_new(OMT_INFO, "rtrd_client", "rtrd client");
	rtrd_client_state = ometric_new_state(state_names,
	    (sizeof(state_names) / sizeof(char *)),
	    "rtrd_client_state", "client session state");
	rtrd_client_connect_time = ometric_new(OMT_GAUGE,
	    "rtrd_client_connect_time",
	    "connect time of this client instance as epoch timestamp");
	rtrd_client_version = ometric_new(OMT_GAUGE, "rtrd_client_version",
	    "version of this client instance");
	rtrd_client_bytes_in = ometric_new(OMT_COUNTER,
	    "rtrd_client_bytes_in",
	    "per byte type count of received messages from client");
	rtrd_client_bytes_out = ometric_new(OMT_COUNTER,
	    "rtrd_client_bytes_out",
	    "per byte type count of transmitted messages to client");
	rtrd_client_sendq_length = ometric_new(OMT_GAUGE,
	    "rtrd_client_sendq_length",
	    "count in bytes of the sendq of this client");
	rtrd_client_sendq_max = ometric_new(OMT_GAUGE,
	    "rtrd_client_sendq_max",
	    "count in bytes of the sendq max of this client");
	rtrd_client_vrp4_announcements = ometric_new(OMT_COUNTER,
	    "rtrd_client_vrp4_announcements",
	    "per PDU type count of VRP4 announcements to client");
	rtrd_client_vrp4_withdrawals = ometric_new(OMT_COUNTER,
	    "rtrd_client_vrp4_withdrawals",
	    "per PDU type count of VRP4 withdrawals to client");
	rtrd_client_vrp4_advertised = ometric_new(OMT_GAUGE,
	    "rtrd_client_vrp4_advertised",
	    "per PDU type count of current VRP4 advertisements to client");
	rtrd_client_vrp6_announcements = ometric_new(OMT_COUNTER,
	    "rtrd_client_vrp6_announcements",
	    "per PDU type count of VRP6 announcements to client");
	rtrd_client_vrp6_withdrawals = ometric_new(OMT_COUNTER,
	    "rtrd_client_vrp6_withdrawals",
	    "per PDU type count of VRP6 withdrawals to client");
	rtrd_client_vrp6_advertised = ometric_new(OMT_GAUGE,
	    "rtrd_client_vrp6_advertised",
	    "per PDU type count of current VRP6 advertisements to client");
	rtrd_client_brk_announcements = ometric_new(OMT_COUNTER,
	    "rtrd_client_brk_announcements",
	    "per PDU type count of BRK announcements to client");
	rtrd_client_brk_withdrawals = ometric_new(OMT_COUNTER,
	    "rtrd_client_brk_withdrawals",
	    "per PDU type count of BRK withdrawals to client");
	rtrd_client_brk_advertised = ometric_new(OMT_GAUGE,
	    "rtrd_client_brk_advertised",
	    "per PDU type count of current BRK advertisements to client");
	rtrd_client_vap_announcements = ometric_new(OMT_COUNTER,
	    "rtrd_client_vap_announcements",
	    "per PDU type count of VAP announcements to client");
	rtrd_client_vap_withdrawals = ometric_new(OMT_COUNTER,
	    "rtrd_client_vap_withdrawals",
	    "per PDU type count of VAP withdrawals to client");
	rtrd_client_vap_advertised = ometric_new(OMT_GAUGE,
	    "rtrd_client_vap_advertised",
	    "per PDU type count of current VAP advertisements to client");
	rtrd_client_reset_queries = ometric_new(OMT_COUNTER,
	    "rtrd_client_reset_queries",
	    "per PDU type count of Reset Queries from client");
	rtrd_client_serial_queries = ometric_new(OMT_COUNTER,
	    "rtrd_client_serial_queries",
	    "per PDU type count of Serial Queries from client");

	rtrd_cache = ometric_new(OMT_INFO, "rtrd_cache", "rtrd cache");
	rtrd_cache_serial = ometric_new(OMT_GAUGE, "rtrd_cache_serial",
	    "serial number of this cache frame");
	rtrd_cache_vrp4 = ometric_new(OMT_GAUGE, "rtrd_cache_vrp4",
	    "per object type count of VRP4 entries");
	rtrd_cache_vrp6 = ometric_new(OMT_GAUGE, "rtrd_cache_vrp6",
	    "per object type count of VRP6 entries");
	rtrd_cache_brk = ometric_new(OMT_GAUGE, "rtrd_cache_brk",
	    "per object type count of BRK entries");
	rtrd_cache_vap = ometric_new(OMT_GAUGE, "rtrd_cache_vap",
	    "per object type count of VAP entries");

	while (!done) {
		poll(&pfd, 1, -1);

		if ((pfd.fd != sock) || !(pfd.revents & POLLIN))
			continue;

		rtr_read(sock, packet, PDU_HEADER_LENGTH);
		h=(struct pdu_header *)packet;
		h->len = be32toh(h->len);

		if (h->len < PDU_HEADER_LENGTH) {
			fprintf(stderr, "packet too small\n");
			exit(0);
		}
		if (h->len > PDU_MAX_LENGTH) {
			fprintf(stderr, "packet too large\n");
			exit(0);
		}

		if (h->len-PDU_HEADER_LENGTH > 0)
			rtr_read(sock, packet+PDU_HEADER_LENGTH,
			    h->len-PDU_HEADER_LENGTH);

		switch (h->type) {
		case GLOBAL_STATS:
			gs=(struct pdu_global_stats *)packet;
			gs->start_time = be64toh(gs->start_time);
			gs->total_bytes_in = be64toh(gs->total_bytes_in);
			gs->total_bytes_out = be64toh(gs->total_bytes_out);
			gs->total_client_connects =
			    be64toh(gs->total_client_connects);
			gs->total_controller_connects =
			    be64toh(gs->total_controller_connects);

			gvalues[0] = gs->nodename;
			gvalues[1] = gs->domainname;
			gvalues[2] = gs->release;
			gvalues[3] = NULL;

			ol = olabels_new(gkeys, gvalues);
			ometric_set_info(rtrd_info, NULL, NULL, ol);
			olabels_free(ol);

			ts.tv_sec = gs->start_time;
			ts.tv_nsec = 0;
			ometric_set_timespec(rtrd_start_time, &ts, NULL);

			ometric_set_int(rtrd_bytes_in,
			    gs->total_bytes_in, NULL);
			ometric_set_int(rtrd_bytes_out,
			    gs->total_bytes_out, NULL);

			ometric_set_int(rtrd_client_connects,
			    gs->total_client_connects, NULL);
			ometric_set_int(rtrd_controller_connects,
			    gs->total_controller_connects, NULL);
			break;
		case CLIENT_STATS:
			cs=(struct pdu_client_stats *)packet;
			cs->fd =
			    be64toh(cs->fd);
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

			snprintf(ip, BUF_SIZE, "%s",
			    inet_ntoa(cs->client_ip));
			snprintf(port, BUF_SIZE, "%u",
			    cs->client_port);

			clivalues[0] = ip;
			clivalues[1] = port;
			clivalues[2] = NULL;

			ol = olabels_new(clikeys, clivalues);
			ometric_set_info(rtrd_client, NULL, NULL, ol);
			ometric_set_state(rtrd_client_state,
			    state_names[(cs->client_state) & 0x03], ol);

			ts.tv_sec = cs->connect_time;
			ts.tv_nsec = 0;
			ometric_set_timespec(rtrd_client_connect_time,
			    &ts, ol);

			ometric_set_int(rtrd_client_version,
			    cs->client_version, ol);

			ometric_set_int(rtrd_client_bytes_in,
			    cs->total_bytes_in, ol);
			ometric_set_int(rtrd_client_bytes_out,
			    cs->total_bytes_out, ol);

			ometric_set_int(rtrd_client_sendq_length,
			    cs->sendq_length, ol);
			ometric_set_int(rtrd_client_sendq_max,
			    cs->sendq_max, ol);

			ometric_set_int(rtrd_client_vrp4_announcements,
			    cs->vrp4_announcements, ol);
			ometric_set_int(rtrd_client_vrp4_withdrawals,
			    cs->vrp4_withdrawals, ol);
			ometric_set_int(rtrd_client_vrp4_advertised,
			    cs->vrp4_advertised, ol);

			ometric_set_int(rtrd_client_vrp6_announcements,
			    cs->vrp6_announcements, ol);
			ometric_set_int(rtrd_client_vrp6_withdrawals,
			    cs->vrp6_withdrawals, ol);
			ometric_set_int(rtrd_client_vrp6_advertised,
			    cs->vrp6_advertised, ol);

			ometric_set_int(rtrd_client_brk_announcements,
			    cs->brk_announcements, ol);
			ometric_set_int(rtrd_client_brk_withdrawals,
			    cs->brk_withdrawals, ol);
			ometric_set_int(rtrd_client_brk_advertised,
			    cs->brk_advertised, ol);

			ometric_set_int(rtrd_client_vap_announcements,
			    cs->vap_announcements, ol);
			ometric_set_int(rtrd_client_vap_withdrawals,
			    cs->vap_withdrawals, ol);
			ometric_set_int(rtrd_client_vap_advertised,
			    cs->vap_advertised, ol);

			ometric_set_int(rtrd_client_reset_queries,
			    cs->reset_query_count, ol);
			ometric_set_int(rtrd_client_serial_queries,
			    cs->serial_query_count, ol);

			olabels_free(ol);
			break;
		case CACHE_FRAME_STATS:
			cfs=(struct pdu_cache_frame_stats *)packet;
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

			if ((cfs->flags & 1) == 0)
				break;

			snprintf(version, BUF_SIZE, "%u",
			    cfs->cache_version);
			snprintf(session_id, BUF_SIZE, "%u",
			    cfs->cache_session_id);

			cvalues[0] = version;
			cvalues[1] = session_id;
			cvalues[2] = NULL;

			ol = olabels_new(ckeys, cvalues);
			ometric_set_info(rtrd_cache, NULL, NULL, ol);

			ometric_set_int(rtrd_cache_serial,
			    cfs->cache_serial, ol);

			ometric_set_int(rtrd_cache_vrp4,
			    cfs->vrp4_count, ol);
			ometric_set_int(rtrd_cache_vrp6,
			    cfs->vrp6_count, ol);
			ometric_set_int(rtrd_cache_brk,
			    cfs->brk_count, ol);
			ometric_set_int(rtrd_cache_vap,
			    cfs->vap_count, ol);

			olabels_free(ol);
			break;
		case END_OF_STATS:
			pduout.ver = RTR_VERSION;
			pduout.type = CLOSE_CONTROLLER;
			pduout.reserved = 0;
			pduout.len = htobe32(PDU_HEADER_LENGTH);

			rtr_write(sock, (char *)&pduout, PDU_HEADER_LENGTH);
			done = 1;
			break;
		default:
			break;
		}
	}

	ometric_output_all(stdout);
	ometric_free_all();
}

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

void
process_import(char *openbgpd, char *controller_filename)
{
	FILE *fp;
	int sock;

	assert(openbgpd);
	assert(controller_filename);

	fp = fopen(openbgpd, "r");

	if (fp == NULL) {
		fprintf(stderr, "could not open %s\n", openbgpd);
		exit(1);
	}

	if ((sock = connect_socket(controller_filename)) < 0) {
		fprintf(stderr, "could not open socket\n");
		exit(1);
	}

	import_start(sock);
	read_openbgpd(fp, sock);
	import_end(sock);

	fclose(fp);
	close(sock);
}

void
process_stats(char *controller_filename)
{
	int sock;

	assert(controller_filename);

	sock = connect_socket(controller_filename);

	if (sock < 0) {
		fprintf(stderr, "could not open socket\n");
		exit(1);
	}

	read_sock_print_ometric(sock);
	close(sock);
}

#define COMMAND_MAXLENGTH	80

#define COMMAND_UNKNOWN		0
#define COMMAND_RELOAD		1
#define COMMAND_STATS		2

int
main(int argc, char **argv)
{
	int command, c;
	char *controller_filename = CONTROLLER_FILENAME;
	char *import_filename = IMPORT_FILENAME;

	if (pledge("stdio rpath unix", NULL) == -1) {
		fprintf(stderr, "pledge error\n");
		exit(1);
	}

	setvbuf(stdout, NULL, _IOLBF, 0);

	while ((c = getopt(argc, argv, "hs:")) != -1) {
		switch (c) {
		case 's':
			controller_filename = optarg;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT,SIG_IGN);
	signal(SIGALRM,SIG_IGN);

	if (argc < 1)
		usage();

	command = COMMAND_UNKNOWN;

	if (strncmp(argv[0], "reload", COMMAND_MAXLENGTH) == 0)
		command = COMMAND_RELOAD;

	if (strncmp(argv[0], "stats", COMMAND_MAXLENGTH) == 0)
		command = COMMAND_STATS;

	argv++;
	argc--;

	switch (command) {
	case COMMAND_RELOAD:
		if (argc >= 1)
			import_filename = argv[0];
		printf("reload request sent.\n");
		process_import(import_filename, controller_filename);
		printf("request processed\n");
		break;
	case COMMAND_STATS:
		process_stats(controller_filename);
		break;
	case COMMAND_UNKNOWN:
	default:
		usage();
	}

	return 0;
}
