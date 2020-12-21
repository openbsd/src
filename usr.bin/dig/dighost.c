/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dighost.c,v 1.37 2020/12/21 11:41:08 florian Exp $ */

/*! \file
 *  \note
 * Notice to programmers:  Do not use this code as an example of how to
 * use the ISC library to perform DNS lookups.  Dig and Host both operate
 * on the request level, since they allow fine-tuning of output and are
 * intended as debugging tools.  As a result, they perform many of the
 * functions which could be better handled using the dns_resolver
 * functions in most applications.
 */

#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/tsig.h>

#include <dst/dst.h>
#include <dst/result.h>

#include <isc/base64.h>
#include <isc/hex.h>
#include <isc/log.h>
#include <isc/result.h>
#include <isc/serial.h>
#include <isc/sockaddr.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/types.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>
#include <lwres/lwres.h>

#include "dig.h"

static lwres_conf_t  lwconfdata;
static lwres_conf_t *lwconf = &lwconfdata;

dig_lookuplist_t lookup_list;
dig_serverlist_t server_list;
dig_serverlist_t root_hints_server_list;
dig_searchlistlist_t search_list;

int
	check_ra = 0,
	have_ipv4 = 1,
	have_ipv6 = 1,
	specified_source = 0,
	free_now = 0,
	cancel_now = 0,
	usesearch = 0,
	showsearch = 0,
	qr = 0,
	is_dst_up = 0,
	keep_open = 0;
in_port_t port = 53;
unsigned int timeout = 0;
unsigned int extrabytes;
isc_log_t *lctx = NULL;
isc_taskmgr_t *taskmgr = NULL;
isc_task_t *global_task = NULL;
isc_timermgr_t *timermgr = NULL;
isc_socketmgr_t *socketmgr = NULL;
struct sockaddr_storage bind_address;
struct sockaddr_storage bind_any;
int sendcount = 0;
int recvcount = 0;
int sockcount = 0;
int ndots = -1;
int tries = 3;
int lookup_counter = 0;

static char sitvalue[256];

isc_socket_t *keep = NULL;
struct sockaddr_storage keepaddr;

static const struct {
	const char *ns;
	const int af;
} root_hints[] = {
	{ "198.41.0.4", AF_INET },		/*  a.root-servers.net  */
	{ "2001:503:ba3e::2:30", AF_INET6 },	/*  a.root-servers.net  */
	{ "199.9.14.201", AF_INET },		/*  b.root-servers.net  */
	{ "2001:500:200::b", AF_INET6 },	/*  b.root-servers.net  */
	{ "192.33.4.12", AF_INET },		/*  c.root-servers.net  */
	{ "2001:500:2::c", AF_INET6 },		/*  c.root-servers.net  */
	{ "199.7.91.13", AF_INET },		/*  d.root-servers.net  */
	{ "2001:500:2d::d", AF_INET6 },		/*  d.root-servers.net  */
	{ "192.203.230.10", AF_INET },		/*  e.root-servers.net  */
	{ "2001:500:a8::e", AF_INET6 },		/*  e.root-servers.net  */
	{ "192.5.5.241", AF_INET },		/*  f.root-servers.net  */
	{ "2001:500:2f::f", AF_INET6 },		/*  f.root-servers.net  */
	{ "192.112.36.4", AF_INET },		/*  g.root-servers.net  */
	{ "2001:500:12::d0d", AF_INET6 },	/*  g.root-servers.net  */
	{ "198.97.190.53", AF_INET },		/*  h.root-servers.net  */
	{ "2001:500:1::53", AF_INET6 },		/*  h.root-servers.net */
	{ "192.36.148.17", AF_INET },		/*  i.root-servers.net  */
	{ "2001:7fe::53", AF_INET6 },		/*  i.root-servers.net  */
	{ "192.58.128.30", AF_INET },		/*  j.root-servers.net  */
	{ "2001:503:c27::2:30", AF_INET6 },	/*  j.root-servers.net  */
	{ "193.0.14.129", AF_INET },		/*  k.root-servers.net  */
	{ "2001:7fd::1", AF_INET6 },		/*  k.root-servers.net  */
	{ "199.7.83.42", AF_INET },		/*  l.root-servers.net  */
	{ "2001:500:9f::42", AF_INET6 },	/*  l.root-servers.net  */
	{ "202.12.27.33", AF_INET },		/*  m.root-servers.net  */
	{ "2001:dc3::35", AF_INET6 }		/*  m.root-servers.net  */
};

/*%
 * Exit Codes:
 *
 *\li	0   Everything went well, including things like NXDOMAIN
 *\li	1   Usage error
 *\li	7   Got too many RR's or Names
 *\li	8   Couldn't open batch file
 *\li	9   No reply from server
 *\li	10  Internal error
 */
int exitcode = 0;
int fatalexit = 0;
char keynametext[MXNAME];
char keyfile[MXNAME] = "";
char keysecret[MXNAME] = "";
unsigned char cookie_secret[33];
unsigned char cookie[8];
dns_name_t *hmacname = NULL;
unsigned int digestbits = 0;
isc_buffer_t *namebuf = NULL;
dns_tsigkey_t *tsigkey = NULL;
int validated = 1;
int debugging = 0;
int debugtiming = 0;
char *progname = NULL;
dig_lookup_t *current_lookup = NULL;

#define DIG_MAX_ADDRESSES 20

/* dynamic callbacks */

isc_result_t
(*dighost_printmessage)(dig_query_t *query, dns_message_t *msg,
	int headers);

void
(*dighost_received)(unsigned int bytes, struct sockaddr_storage *from, dig_query_t *query);

void
(*dighost_trying)(char *frm, dig_lookup_t *lookup);

void
(*dighost_shutdown)(void);

/* forward declarations */

static void
cancel_lookup(dig_lookup_t *lookup);

static void
recv_done(isc_task_t *task, isc_event_t *event);

static void
send_udp(dig_query_t *query);

static void
connect_timeout(isc_task_t *task, isc_event_t *event);

static void
launch_next_query(dig_query_t *query, int include_question);

static void
check_next_lookup(dig_lookup_t *lookup);

static int
next_origin(dig_lookup_t *oldlookup);

char *
next_token(char **stringp, const char *delim) {
	char *res;

	do {
		res = strsep(stringp, delim);
		if (res == NULL)
			break;
	} while (*res == '\0');
	return (res);
}

static int
count_dots(char *string) {
	char *s;
	int i = 0;

	s = string;
	while (*s != '\0') {
		if (*s == '.')
			i++;
		s++;
	}
	return (i);
}

static void
hex_dump(isc_buffer_t *b) {
	unsigned int len, i;
	isc_region_t r;

	isc_buffer_usedregion(b, &r);

	printf("%u bytes\n", r.length);
	for (len = 0; len < r.length; len++) {
		printf("%02x ", r.base[len]);
		if (len % 16 == 15) {
			fputs("         ", stdout);
			for (i = len - 15; i <= len; i++) {
				if (r.base[i] >= '!' && r.base[i] <= '}')
					putchar(r.base[i]);
				else
					putchar('.');
			}
			printf("\n");
		}
	}
	if (len % 16 != 0) {
		for (i = len; (i % 16) != 0; i++)
			fputs("   ", stdout);
		fputs("         ", stdout);
		for (i = ((len>>4)<<4); i < len; i++) {
			if (r.base[i] >= '!' && r.base[i] <= '}')
				putchar(r.base[i]);
			else
				putchar('.');
		}
		printf("\n");
	}
}

/*%
 * Append 'len' bytes of 'text' at '*p', failing with
 * ISC_R_NOSPACE if that would advance p past 'end'.
 */
static isc_result_t
append(const char *text, size_t len, char **p, char *end) {
	if (*p + len > end)
		return (ISC_R_NOSPACE);
	memmove(*p, text, len);
	*p += len;
	return (ISC_R_SUCCESS);
}

static isc_result_t
reverse_octets(const char *in, char **p, char *end) {
	const char *dot = strchr(in, '.');
	size_t len;
	if (dot != NULL) {
		isc_result_t result;
		result = reverse_octets(dot + 1, p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = append(".", 1, p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		len = (int) (dot - in);
	} else {
		len = (int) strlen(in);
	}
	return (append(in, len, p, end));
}

isc_result_t
get_reverse(char *reverse, size_t len, char *value, int ip6_int,
	    int strict)
{
	int r;
	struct in_addr in;
	struct in6_addr in6;
	isc_result_t result;

	r = inet_pton(AF_INET6, value, &in6);
	if (r > 0) {
		/* This is a valid IPv6 address. */
		static char hex_digits[] = {
			'0', '1', '2', '3', '4', '5', '6', '7',
			'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
		};
		int i;
		unsigned char *bytes = (unsigned char *)&in6;
		char* cp;

		if (len <= 15 * 4 + sizeof("ip6.int"))
			return (ISC_R_NOMEMORY);

		cp = reverse;
		for (i = 15; i >= 0; i--) {
			*cp++ = hex_digits[bytes[i] & 0x0f];
			*cp++ = '.';
			*cp++ = hex_digits[(bytes[i] >> 4) & 0x0f];
			*cp++ = '.';
		}
		*cp = '\0';
		if (strlcat(reverse, ip6_int ? "ip6.int" : "ip6.arpa", len)
		    >= len)
			return (ISC_R_NOSPACE);
		return (ISC_R_SUCCESS);
	} else {
		/*
		 * Not a valid IPv6 address.  Assume IPv4.
		 * If 'strict' is not set, construct the
		 * in-addr.arpa name by blindly reversing
		 * octets whether or not they look like integers,
		 * so that this can be used for RFC2317 names
		 * and such.
		 */
		char *p = reverse;
		char *end = reverse + len;
		if (strict && inet_pton(AF_INET, value, &in) != 1)
			return (DNS_R_BADDOTTEDQUAD);
		result = reverse_octets(value, &p, end);
		if (result != ISC_R_SUCCESS)
			return (result);
		/* Append .in-addr.arpa. and a terminating NUL. */
		result = append(".in-addr.arpa.", 15, &p, end);
		return (result);
	}
}

void
fatal(const char *format, ...) {
	va_list args;

	fflush(stdout);
	fprintf(stderr, "%s: ", progname);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (exitcode < 10)
		exitcode = 10;
	if (fatalexit != 0)
		exitcode = fatalexit;
	exit(exitcode);
}

void
debug(const char *format, ...) {
	va_list args;
	struct timespec t;

	if (debugging) {
		fflush(stdout);
		if (debugtiming) {
			clock_gettime(CLOCK_MONOTONIC, &t);
			fprintf(stderr, "%lld.%06ld: ", t.tv_sec, t.tv_nsec /
			    1000);
		}
		va_start(args, format);
		vfprintf(stderr, format, args);
		va_end(args);
		fprintf(stderr, "\n");
	}
}

void
check_result(isc_result_t result, const char *msg) {
	if (result != ISC_R_SUCCESS) {
		fatal("%s: %s", msg, isc_result_totext(result));
	}
}

/*%
 * Create a server structure, which is part of the lookup structure.
 * This is little more than a linked list of servers to query in hopes
 * of finding the answer the user is looking for
 */
dig_server_t *
make_server(const char *servname, const char *userarg) {
	dig_server_t *srv;

	REQUIRE(servname != NULL);

	debug("make_server(%s)", servname);
	srv = malloc(sizeof(struct dig_server));
	if (srv == NULL)
		fatal("memory allocation failure in %s:%d",
		      __FILE__, __LINE__);
	strlcpy(srv->servername, servname, MXNAME);
	strlcpy(srv->userarg, userarg, MXNAME);
	ISC_LINK_INIT(srv, link);
	return (srv);
}

static int
addr2af(int lwresaddrtype)
{
	int af = 0;

	switch (lwresaddrtype) {
	case LWRES_ADDRTYPE_V4:
		af = AF_INET;
		break;

	case LWRES_ADDRTYPE_V6:
		af = AF_INET6;
		break;
	}

	return (af);
}

/*%
 * Create a copy of the server list from the lwres configuration structure.
 * The dest list must have already had ISC_LIST_INIT applied.
 */
static void
copy_server_list(lwres_conf_t *confdata, dig_serverlist_t *dest) {
	dig_server_t *newsrv;
	char tmp[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") +
		 sizeof("%4000000000")];
	int af;
	int i;

	debug("copy_server_list()");
	for (i = 0; i < confdata->nsnext; i++) {
		af = addr2af(confdata->nameservers[i].family);

		if (af == AF_INET && !have_ipv4)
			continue;
		if (af == AF_INET6 && !have_ipv6)
			continue;

		inet_ntop(af, confdata->nameservers[i].address,
				   tmp, sizeof(tmp));
		if (af == AF_INET6 && confdata->nameservers[i].zone != 0) {
			char buf[sizeof("%4000000000")];
			snprintf(buf, sizeof(buf), "%%%u",
				 confdata->nameservers[i].zone);
			strlcat(tmp, buf, sizeof(tmp));
		}
		newsrv = make_server(tmp, tmp);
		ISC_LINK_INIT(newsrv, link);
		ISC_LIST_ENQUEUE(*dest, newsrv, link);
	}
}

void
flush_server_list(void) {
	dig_server_t *s, *ps;

	debug("flush_server_list()");
	s = ISC_LIST_HEAD(server_list);
	while (s != NULL) {
		ps = s;
		s = ISC_LIST_NEXT(s, link);
		ISC_LIST_DEQUEUE(server_list, ps, link);
		free(ps);
	}
}

/* this used to be bind9_getaddresses from lib/bind9 */
static isc_result_t
get_addresses(const char *hostname, in_port_t dstport,
		   struct sockaddr_storage *addrs, int addrsize, int *addrcount)
{
	struct addrinfo *ai = NULL, *tmpai, hints;
	int result, i;
	char dport[sizeof("65535")];

	REQUIRE(hostname != NULL);
	REQUIRE(addrs != NULL);
	REQUIRE(addrcount != NULL);
	REQUIRE(addrsize > 0);

	memset(&hints, 0, sizeof(hints));
	if (!have_ipv6)
		hints.ai_family = PF_INET;
	else if (!have_ipv4)
		hints.ai_family = PF_INET6;
	else {
		hints.ai_family = PF_UNSPEC;
		hints.ai_flags = AI_ADDRCONFIG;
	}
	hints.ai_socktype = SOCK_DGRAM;

	snprintf(dport, sizeof(dport), "%d", dstport);
	result = getaddrinfo(hostname, dport, &hints, &ai);
	switch (result) {
	case 0:
		break;
	case EAI_NONAME:
	case EAI_NODATA:
		return (ISC_R_NOTFOUND);
	default:
		return (ISC_R_FAILURE);
	}
	for (tmpai = ai, i = 0;
	     tmpai != NULL && i < addrsize;
	     tmpai = tmpai->ai_next)
	{
		if (tmpai->ai_family != AF_INET &&
		    tmpai->ai_family != AF_INET6)
			continue;
		if (tmpai->ai_addrlen > sizeof(addrs[i]))
			continue;
		memset(&addrs[i], 0, sizeof(addrs[i]));
		memcpy(&addrs[i], tmpai->ai_addr, tmpai->ai_addrlen);
		i++;

	}
	freeaddrinfo(ai);
	*addrcount = i;
	if (*addrcount == 0)
		return (ISC_R_NOTFOUND);
	else
		return (ISC_R_SUCCESS);
}

isc_result_t
set_nameserver(char *opt) {
	isc_result_t result;
	struct sockaddr_storage sockaddrs[DIG_MAX_ADDRESSES];
	int count, i;
	dig_server_t *srv;
	char tmp[NI_MAXHOST];

	if (opt == NULL)
		return ISC_R_NOTFOUND;

	result = get_addresses(opt, 0, sockaddrs,
				    DIG_MAX_ADDRESSES, &count);
	if (result != ISC_R_SUCCESS)
		return (result);

	flush_server_list();

	for (i = 0; i < count; i++) {
		int error;
		error = getnameinfo((struct sockaddr *)&sockaddrs[i],
		    sockaddrs[i].ss_len, tmp, sizeof(tmp), NULL, 0,
		    NI_NUMERICHOST | NI_NUMERICSERV);
		if (error)
			fatal("%s", gai_strerror(error));
		srv = make_server(tmp, opt);
		if (srv == NULL)
			fatal("memory allocation failure");
		ISC_LIST_APPEND(server_list, srv, link);
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
add_nameserver(lwres_conf_t *confdata, const char *addr, int af) {

	int i = confdata->nsnext;

	if (confdata->nsnext >= LWRES_CONFMAXNAMESERVERS)
		return (ISC_R_FAILURE);

	switch (af) {
	case AF_INET:
		confdata->nameservers[i].family = LWRES_ADDRTYPE_V4;
		confdata->nameservers[i].length = sizeof(struct in_addr);
		break;
	case AF_INET6:
		confdata->nameservers[i].family = LWRES_ADDRTYPE_V6;
		confdata->nameservers[i].length = sizeof(struct in6_addr);
		break;
	default:
		return (ISC_R_FAILURE);
	}

	if (inet_pton(af, addr, &confdata->nameservers[i].address) == 1) {
		confdata->nsnext++;
		return (ISC_R_SUCCESS);
	}
	return (ISC_R_FAILURE);
}

/*%
 * Produce a cloned server list.  The dest list must have already had
 * ISC_LIST_INIT applied.
 */
void
clone_server_list(dig_serverlist_t src, dig_serverlist_t *dest) {
	dig_server_t *srv, *newsrv;

	debug("clone_server_list()");
	srv = ISC_LIST_HEAD(src);
	while (srv != NULL) {
		newsrv = make_server(srv->servername, srv->userarg);
		ISC_LINK_INIT(newsrv, link);
		ISC_LIST_ENQUEUE(*dest, newsrv, link);
		srv = ISC_LIST_NEXT(srv, link);
	}
}

/*%
 * Create an empty lookup structure, which holds all the information needed
 * to get an answer to a user's question.  This structure contains two
 * linked lists: the server list (servers to query) and the query list
 * (outstanding queries which have been made to the listed servers).
 */
dig_lookup_t *
make_empty_lookup(void) {
	dig_lookup_t *looknew;

	debug("make_empty_lookup()");

	INSIST(!free_now);

	looknew = malloc(sizeof(struct dig_lookup));
	if (looknew == NULL)
		fatal("memory allocation failure in %s:%d",
		       __FILE__, __LINE__);
	looknew->pending = 1;
	looknew->textname[0] = 0;
	looknew->cmdline[0] = 0;
	looknew->rdtype = dns_rdatatype_a;
	looknew->qrdtype = dns_rdatatype_a;
	looknew->rdclass = dns_rdataclass_in;
	looknew->rdtypeset = 0;
	looknew->rdclassset = 0;
	looknew->sendspace = NULL;
	looknew->sendmsg = NULL;
	looknew->name = NULL;
	looknew->oname = NULL;
	looknew->xfr_q = NULL;
	looknew->current_query = NULL;
	looknew->doing_xfr = 0;
	looknew->ixfr_serial = 0;
	looknew->trace = 0;
	looknew->trace_root = 0;
	looknew->identify = 0;
	looknew->identify_previous_line = 0;
	looknew->ignore = 0;
	looknew->servfail_stops = 1;
	looknew->besteffort = 1;
	looknew->dnssec = 0;
	looknew->ednsflags = 0;
	looknew->opcode = dns_opcode_query;
	looknew->expire = 0;
	looknew->nsid = 0;
	looknew->idnout = 0;
	looknew->sit = 0;
	looknew->udpsize = 0;
	looknew->edns = -1;
	looknew->recurse = 1;
	looknew->aaonly = 0;
	looknew->adflag = 0;
	looknew->cdflag = 0;
	looknew->ns_search_only = 0;
	looknew->origin = NULL;
	looknew->tsigctx = NULL;
	looknew->querysig = NULL;
	looknew->retries = tries;
	looknew->nsfound = 0;
	looknew->tcp_mode = 0;
	looknew->tcp_mode_set = 0;
	looknew->ip6_int = 0;
	looknew->comments = 1;
	looknew->stats = 1;
	looknew->section_question = 1;
	looknew->section_answer = 1;
	looknew->section_authority = 1;
	looknew->section_additional = 1;
	looknew->new_search = 0;
	looknew->done_as_is = 0;
	looknew->need_search = 0;
	looknew->ecs_addr = NULL;
	looknew->ecs_plen = 0;
	looknew->sitvalue = NULL;
	looknew->ednsopts = NULL;
	looknew->ednsoptscnt = 0;
	looknew->ednsneg = 0;
	looknew->eoferr = 0;
	dns_fixedname_init(&looknew->fdomain);
	ISC_LINK_INIT(looknew, link);
	ISC_LIST_INIT(looknew->q);
	ISC_LIST_INIT(looknew->connecting);
	ISC_LIST_INIT(looknew->my_server_list);
	return (looknew);
}

#define EDNSOPT_OPTIONS 100U

static void
cloneopts(dig_lookup_t *looknew, dig_lookup_t *lookold) {
	size_t len = sizeof(looknew->ednsopts[0]) * EDNSOPT_OPTIONS;
	size_t i;
	looknew->ednsopts = malloc(len);
	if (looknew->ednsopts == NULL)
		fatal("out of memory");
	for (i = 0; i < EDNSOPT_OPTIONS; i++) {
		looknew->ednsopts[i].code = 0;
		looknew->ednsopts[i].length = 0;
		looknew->ednsopts[i].value = NULL;
	}
	looknew->ednsoptscnt = 0;
	if (lookold == NULL || lookold->ednsopts == NULL)
		return;

	for (i = 0; i < lookold->ednsoptscnt; i++) {
		len = lookold->ednsopts[i].length;
		if (len != 0) {
			INSIST(lookold->ednsopts[i].value != NULL);
			looknew->ednsopts[i].value =
				 malloc(len);
			if (looknew->ednsopts[i].value == NULL)
				fatal("out of memory");
			memmove(looknew->ednsopts[i].value,
				lookold->ednsopts[i].value, len);
		}
		looknew->ednsopts[i].code = lookold->ednsopts[i].code;
		looknew->ednsopts[i].length = len;
	}
	looknew->ednsoptscnt = lookold->ednsoptscnt;
}

/*%
 * Clone a lookup, perhaps copying the server list.  This does not clone
 * the query list, since it will be regenerated by the setup_lookup()
 * function, nor does it queue up the new lookup for processing.
 * Caution: If you don't clone the servers, you MUST clone the server
 * list separately from somewhere else, or construct it by hand.
 */
dig_lookup_t *
clone_lookup(dig_lookup_t *lookold, int servers) {
	dig_lookup_t *looknew;

	debug("clone_lookup()");

	INSIST(!free_now);

	looknew = make_empty_lookup();
	INSIST(looknew != NULL);
	strlcpy(looknew->textname, lookold->textname, MXNAME);
	strlcpy(looknew->cmdline, lookold->cmdline, MXNAME);
	looknew->textname[MXNAME-1] = 0;
	looknew->rdtype = lookold->rdtype;
	looknew->qrdtype = lookold->qrdtype;
	looknew->rdclass = lookold->rdclass;
	looknew->rdtypeset = lookold->rdtypeset;
	looknew->rdclassset = lookold->rdclassset;
	looknew->doing_xfr = lookold->doing_xfr;
	looknew->ixfr_serial = lookold->ixfr_serial;
	looknew->trace = lookold->trace;
	looknew->trace_root = lookold->trace_root;
	looknew->identify = lookold->identify;
	looknew->identify_previous_line = lookold->identify_previous_line;
	looknew->ignore = lookold->ignore;
	looknew->servfail_stops = lookold->servfail_stops;
	looknew->besteffort = lookold->besteffort;
	looknew->dnssec = lookold->dnssec;
	looknew->ednsflags = lookold->ednsflags;
	looknew->opcode = lookold->opcode;
	looknew->expire = lookold->expire;
	looknew->nsid = lookold->nsid;
	looknew->sit = lookold->sit;
	looknew->sitvalue = lookold->sitvalue;
	if (lookold->ednsopts != NULL) {
		cloneopts(looknew, lookold);
	} else {
		looknew->ednsopts = NULL;
		looknew->ednsoptscnt = 0;
	}
	looknew->ednsneg = lookold->ednsneg;
	looknew->idnout = lookold->idnout;
	looknew->udpsize = lookold->udpsize;
	looknew->edns = lookold->edns;
	looknew->recurse = lookold->recurse;
	looknew->aaonly = lookold->aaonly;
	looknew->adflag = lookold->adflag;
	looknew->cdflag = lookold->cdflag;
	looknew->ns_search_only = lookold->ns_search_only;
	looknew->tcp_mode = lookold->tcp_mode;
	looknew->tcp_mode_set = lookold->tcp_mode_set;
	looknew->comments = lookold->comments;
	looknew->stats = lookold->stats;
	looknew->section_question = lookold->section_question;
	looknew->section_answer = lookold->section_answer;
	looknew->section_authority = lookold->section_authority;
	looknew->section_additional = lookold->section_additional;
	looknew->origin = lookold->origin;
	looknew->retries = lookold->retries;
	looknew->tsigctx = NULL;
	looknew->need_search = lookold->need_search;
	looknew->done_as_is = lookold->done_as_is;
	looknew->eoferr = lookold->eoferr;

	if (lookold->ecs_addr != NULL) {
		size_t len = sizeof(struct sockaddr_storage);
		looknew->ecs_addr = malloc(len);
		if (looknew->ecs_addr == NULL)
			fatal("out of memory");
		memmove(looknew->ecs_addr, lookold->ecs_addr, len);
		looknew->ecs_plen = lookold->ecs_plen;
	}

	dns_name_copy(dns_fixedname_name(&lookold->fdomain),
		      dns_fixedname_name(&looknew->fdomain), NULL);

	if (servers)
		clone_server_list(lookold->my_server_list,
				  &looknew->my_server_list);
	return (looknew);
}

/*%
 * Requeue a lookup for further processing, perhaps copying the server
 * list.  The new lookup structure is returned to the caller, and is
 * queued for processing.  If servers are not cloned in the requeue, they
 * must be added before allowing the current event to complete, since the
 * completion of the event may result in the next entry on the lookup
 * queue getting run.
 */
dig_lookup_t *
requeue_lookup(dig_lookup_t *lookold, int servers) {
	dig_lookup_t *looknew;

	debug("requeue_lookup()");

	lookup_counter++;
	if (lookup_counter > LOOKUP_LIMIT)
		fatal("too many lookups");

	looknew = clone_lookup(lookold, servers);
	INSIST(looknew != NULL);

	debug("before insertion, init@%p -> %p, new@%p -> %p",
	      lookold, lookold->link.next, looknew, looknew->link.next);
	ISC_LIST_PREPEND(lookup_list, looknew, link);
	debug("after insertion, init -> %p, new = %p, new -> %p",
	      lookold, looknew, looknew->link.next);
	return (looknew);
}

void
setup_text_key(void) {
	isc_result_t result;
	dns_name_t keyname;
	isc_buffer_t secretbuf;
	unsigned int secretsize;
	unsigned char *secretstore;

	debug("setup_text_key()");
	result = isc_buffer_allocate(&namebuf, MXNAME);
	check_result(result, "isc_buffer_allocate");
	dns_name_init(&keyname, NULL);
	check_result(result, "dns_name_init");
	isc_buffer_putstr(namebuf, keynametext);
	secretsize = (unsigned int) strlen(keysecret) * 3 / 4;
	secretstore = malloc(secretsize);
	if (secretstore == NULL)
		fatal("memory allocation failure in %s:%d",
		      __FILE__, __LINE__);
	isc_buffer_init(&secretbuf, secretstore, secretsize);
	result = isc_base64_decodestring(keysecret, &secretbuf);
	if (result != ISC_R_SUCCESS)
		goto failure;

	secretsize = isc_buffer_usedlength(&secretbuf);

	if (hmacname == NULL) {
		result = DST_R_UNSUPPORTEDALG;
		goto failure;
	}

	result = dns_name_fromtext(&keyname, namebuf, dns_rootname, 0, namebuf);
	if (result != ISC_R_SUCCESS)
		goto failure;

	result = dns_tsigkey_create(&keyname, hmacname, secretstore,
				    (int)secretsize, 0, NULL, 0, 0,
				    &tsigkey);
 failure:
	if (result != ISC_R_SUCCESS)
		printf(";; Couldn't create key %s: %s\n",
		       keynametext, isc_result_totext(result));
	else
		dst_key_setbits(tsigkey->key, digestbits);

	free(secretstore);
	dns_name_invalidate(&keyname);
	isc_buffer_free(&namebuf);
}

static uint32_t
parse_bits(char *arg, uint32_t max) {
	uint32_t tmp;
	const char *errstr;

	tmp = strtonum(arg, 0, max, &errstr);
	if (errstr != NULL)
		fatal("digest bits is %s: '%s'", errstr, arg);
	tmp = (tmp + 7) & ~0x7U;
	return (tmp);
}

isc_result_t
parse_netprefix(struct sockaddr_storage **sap, int *plen, const char *value) {
	struct sockaddr_storage *sa = NULL;
	struct in_addr *in4;
	struct in6_addr *in6;
	int prefix_length;

	REQUIRE(sap != NULL && *sap == NULL);

	sa = calloc(1, sizeof(*sa));
	if (sa == NULL)
		fatal("out of memory");

	in4 = &((struct sockaddr_in *)sa)->sin_addr;
	in6 = &((struct sockaddr_in6 *)sa)->sin6_addr;

	if (strcmp(value, "0") == 0) {
		sa->ss_family = AF_UNSPEC;
		prefix_length = 0;
		goto done;
	}

	if ((prefix_length = inet_net_pton(AF_INET6, value, in6, sizeof(*in6)))
	    != -1) {
		sa->ss_len = sizeof(struct sockaddr_in6);
		sa->ss_family = AF_INET6;
	} else if ((prefix_length = inet_net_pton(AF_INET, value, in4,
	    sizeof(*in4))) != -1) {
		sa->ss_len = sizeof(struct sockaddr_in);
		sa->ss_family = AF_INET;
	} else
		fatal("invalid address '%s'", value);

done:
	*plen = prefix_length;
	*sap = sa;

	return (ISC_R_SUCCESS);
}

/*
 * Parse HMAC algorithm specification
 */
void
parse_hmac(const char *hmac) {
	char buf[20];
	size_t len;

	REQUIRE(hmac != NULL);

	len = strlen(hmac);
	if (len >= sizeof(buf))
		fatal("unknown key type '%.*s'", (int)len, hmac);
	strlcpy(buf, hmac, sizeof(buf));

	digestbits = 0;

	if (strcasecmp(buf, "hmac-sha1") == 0) {
		hmacname = DNS_TSIG_HMACSHA1_NAME;
		digestbits = 0;
	} else if (strncasecmp(buf, "hmac-sha1-", 10) == 0) {
		hmacname = DNS_TSIG_HMACSHA1_NAME;
		digestbits = parse_bits(&buf[10], 160);
	} else if (strcasecmp(buf, "hmac-sha224") == 0) {
		hmacname = DNS_TSIG_HMACSHA224_NAME;
	} else if (strncasecmp(buf, "hmac-sha224-", 12) == 0) {
		hmacname = DNS_TSIG_HMACSHA224_NAME;
		digestbits = parse_bits(&buf[12], 224);
	} else if (strcasecmp(buf, "hmac-sha256") == 0) {
		hmacname = DNS_TSIG_HMACSHA256_NAME;
	} else if (strncasecmp(buf, "hmac-sha256-", 12) == 0) {
		hmacname = DNS_TSIG_HMACSHA256_NAME;
		digestbits = parse_bits(&buf[12], 256);
	} else if (strcasecmp(buf, "hmac-sha384") == 0) {
		hmacname = DNS_TSIG_HMACSHA384_NAME;
	} else if (strncasecmp(buf, "hmac-sha384-", 12) == 0) {
		hmacname = DNS_TSIG_HMACSHA384_NAME;
		digestbits = parse_bits(&buf[12], 384);
	} else if (strcasecmp(buf, "hmac-sha512") == 0) {
		hmacname = DNS_TSIG_HMACSHA512_NAME;
	} else if (strncasecmp(buf, "hmac-sha512-", 12) == 0) {
		hmacname = DNS_TSIG_HMACSHA512_NAME;
		digestbits = parse_bits(&buf[12], 512);
	} else {
		fprintf(stderr, ";; Warning, ignoring "
			"invalid TSIG algorithm %s\n", buf);
	}
}

/*
 * Get a key from a named.conf format keyfile
 */
static isc_result_t
read_confkey(void) {
	cfg_parser_t *pctx = NULL;
	cfg_obj_t *file = NULL;
	const cfg_obj_t *keyobj = NULL;
	const cfg_obj_t *secretobj = NULL;
	const cfg_obj_t *algorithmobj = NULL;
	const char *keyname;
	const char *secretstr;
	const char *algorithm;
	isc_result_t result;

	result = cfg_parser_create(NULL, &pctx);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = cfg_parse_file(pctx, keyfile, &cfg_type_sessionkey,
				&file);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = cfg_map_get(file, "key", &keyobj);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	(void) cfg_map_get(keyobj, "secret", &secretobj);
	(void) cfg_map_get(keyobj, "algorithm", &algorithmobj);
	if (secretobj == NULL || algorithmobj == NULL)
		fatal("key must have algorithm and secret");

	keyname = cfg_obj_asstring(cfg_map_getname(keyobj));
	secretstr = cfg_obj_asstring(secretobj);
	algorithm = cfg_obj_asstring(algorithmobj);

	strlcpy(keynametext, keyname, sizeof(keynametext));
	strlcpy(keysecret, secretstr, sizeof(keysecret));
	parse_hmac(algorithm);
	setup_text_key();

 cleanup:
	if (pctx != NULL) {
		if (file != NULL)
			cfg_obj_destroy(pctx, &file);
		cfg_parser_destroy(&pctx);
	}

	return (result);
}

void
setup_file_key(void) {
	isc_result_t result;

	debug("setup_file_key()");

	/* Try reading the key as a session.key keyfile */
	result = read_confkey();

	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "Couldn't read key from %s: %s\n",
			keyfile, isc_result_totext(result));
}

static dig_searchlist_t *
make_searchlist_entry(char *domain) {
	dig_searchlist_t *search;
	search = malloc(sizeof(*search));
	if (search == NULL)
		fatal("memory allocation failure in %s:%d",
		      __FILE__, __LINE__);
	strlcpy(search->origin, domain, MXNAME);
	search->origin[MXNAME-1] = 0;
	ISC_LINK_INIT(search, link);
	return (search);
}

static void
clear_searchlist(void) {
	dig_searchlist_t *search;
	while ((search = ISC_LIST_HEAD(search_list)) != NULL) {
		ISC_LIST_UNLINK(search_list, search, link);
		free(search);
	}
}

static void
create_search_list(lwres_conf_t *confdata) {
	int i;
	dig_searchlist_t *search;

	debug("create_search_list()");
	clear_searchlist();

	for (i = 0; i < confdata->searchnxt; i++) {
		search = make_searchlist_entry(confdata->search[i]);
		ISC_LIST_APPEND(search_list, search, link);
	}
}

/*%
 * Setup the system as a whole, reading key information and resolv.conf
 * settings.
 */
void
setup_system(int ipv4only, int ipv6only) {
	dig_searchlist_t *domain = NULL;
	lwres_result_t lwresult;
	int lwresflags = 0;

	debug("setup_system()");

	if (ipv4only) {
		if (have_ipv4)
			have_ipv6 = 0;
		else
			fatal("can't find IPv4 networking");
	}

	if (ipv6only) {
		if (have_ipv6)
			have_ipv4 = 0;
		else
			fatal("can't find IPv6 networking");
	}

	if (have_ipv4)
		lwresflags |= LWRES_USEIPV4;
	if (have_ipv6)
		lwresflags |= LWRES_USEIPV6;
	lwres_conf_init(lwconf, lwresflags);

	lwresult = lwres_conf_parse(lwconf, _PATH_RESCONF);
	if (lwresult != LWRES_R_SUCCESS && lwresult != LWRES_R_NOTFOUND)
		fatal("parse of %s failed", _PATH_RESCONF);

	/* Make the search list */
	if (lwconf->searchnxt > 0)
		create_search_list(lwconf);
	else { /* No search list. Use the domain name if any */
		if (lwconf->domainname != NULL) {
			domain = make_searchlist_entry(lwconf->domainname);
			ISC_LIST_APPEND(search_list, domain, link);
			domain  = NULL;
		}
	}

	if (ndots == -1) {
		ndots = lwconf->ndots;
		debug("ndots is %d.", ndots);
	}

	/* If user doesn't specify server use nameservers from resolv.conf. */
	if (ISC_LIST_EMPTY(server_list))
		copy_server_list(lwconf, &server_list);

	/* If we don't find a nameserver fall back to localhost */
	if (ISC_LIST_EMPTY(server_list)) {
		if (have_ipv4) {
			lwresult = add_nameserver(lwconf, "127.0.0.1", AF_INET);
			if (lwresult != ISC_R_SUCCESS)
				fatal("add_nameserver failed");
		}
		if (have_ipv6) {
			lwresult = add_nameserver(lwconf, "::1", AF_INET6);
			if (lwresult != ISC_R_SUCCESS)
				fatal("add_nameserver failed");
		}

		copy_server_list(lwconf, &server_list);
	}

	if (keyfile[0] != 0)
		setup_file_key();
	else if (keysecret[0] != 0)
		setup_text_key();
	arc4random_buf(cookie_secret, sizeof(cookie_secret));
}

/*%
 * Override the search list derived from resolv.conf by 'domain'.
 */
void
set_search_domain(char *domain) {
	dig_searchlist_t *search;

	clear_searchlist();
	search = make_searchlist_entry(domain);
	ISC_LIST_APPEND(search_list, search, link);
}

/*%
 * Setup the ISC and DNS libraries for use by the system.
 */
void
setup_libs(void) {
	isc_result_t result;
	isc_logconfig_t *logconfig = NULL;

	debug("setup_libs()");

	dns_result_register();

	result = isc_log_create(&lctx, &logconfig);
	check_result(result, "isc_log_create");

	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	result = isc_log_usechannel(logconfig, "default_debug", NULL, NULL);
	check_result(result, "isc_log_usechannel");

	isc_log_setdebuglevel(lctx, 0);

	result = isc_taskmgr_create(1, 0, &taskmgr);
	check_result(result, "isc_taskmgr_create");

	result = isc_task_create(taskmgr, 0, &global_task);
	check_result(result, "isc_task_create");
	isc_task_setname(global_task, "dig", NULL);

	result = isc_timermgr_create(&timermgr);
	check_result(result, "isc_timermgr_create");

	result = isc_socketmgr_create(&socketmgr);
	check_result(result, "isc_socketmgr_create");

	check_result(result, "isc_entropy_create");

	result = dst_lib_init();
	check_result(result, "dst_lib_init");
	is_dst_up = 1;
}

typedef struct dig_ednsoptname {
	uint32_t code;
	const char  *name;
} dig_ednsoptname_t;

dig_ednsoptname_t optnames[] = {
	{ 3, "NSID" },		/* RFC 5001 */
	{ 5, "DAU" },		/* RFC 6975 */
	{ 6, "DHU" },		/* RFC 6975 */
	{ 7, "N3U" },		/* RFC 6975 */
	{ 8, "ECS" },		/* RFC 7871 */
	{ 9, "EXPIRE" },	/* RFC 7314 */
	{ 10, "COOKIE" },	/* RFC 7873 */
	{ 11, "KEEPALIVE" },	/* RFC 7828 */
	{ 12, "PADDING" },	/* RFC 7830 */
	{ 12, "PAD" },		/* shorthand */
	{ 13, "CHAIN" },	/* RFC 7901 */
	{ 14, "KEY-TAG" },	/* RFC 8145 */
	{ 26946, "DEVICEID" },	/* Brian Hartvigsen */
};

#define N_EDNS_OPTNAMES  (sizeof(optnames) / sizeof(optnames[0]))

void
save_opt(dig_lookup_t *lookup, char *code, char *value) {
	isc_result_t result;
	uint32_t num = 0;
	isc_buffer_t b;
	int found = 0;
	unsigned int i;
	const char *errstr;

	if (lookup->ednsoptscnt >= EDNSOPT_OPTIONS)
		fatal("too many ednsopts");

	for (i = 0; i < N_EDNS_OPTNAMES; i++) {
		if (strcasecmp(code, optnames[i].name) == 0) {
			num = optnames[i].code;
			found = 1;
			break;
		}
	}

	if (!found) {
		num = strtonum(code, 0, 65535, &errstr);
		if (errstr != NULL)
			fatal("edns code point is %s: '%s'", errstr, code);
	}

	if (lookup->ednsopts == NULL) {
		cloneopts(lookup, NULL);
	}

	if (lookup->ednsopts[lookup->ednsoptscnt].value != NULL)
		free(lookup->ednsopts[lookup->ednsoptscnt].value);

	lookup->ednsopts[lookup->ednsoptscnt].code = num;
	lookup->ednsopts[lookup->ednsoptscnt].length = 0;
	lookup->ednsopts[lookup->ednsoptscnt].value = NULL;

	if (value != NULL) {
		char *buf;
		buf = malloc(strlen(value)/2 + 1);
		if (buf == NULL)
			fatal("out of memory");
		isc_buffer_init(&b, buf, (unsigned int) strlen(value)/2 + 1);
		result = isc_hex_decodestring(value, &b);
		check_result(result, "isc_hex_decodestring");
		lookup->ednsopts[lookup->ednsoptscnt].value =
						 isc_buffer_base(&b);
		lookup->ednsopts[lookup->ednsoptscnt].length =
						 isc_buffer_usedlength(&b);
	}

	lookup->ednsoptscnt++;
}

/*%
 * Add EDNS0 option record to a message.  Currently, the only supported
 * options are UDP buffer size, the DO bit, and EDNS options
 * (e.g., NSID, SIT, client-subnet)
 */
static void
add_opt(dns_message_t *msg, uint16_t udpsize, uint16_t edns,
	unsigned int flags, dns_ednsopt_t *opts, size_t count)
{
	dns_rdataset_t *rdataset = NULL;
	isc_result_t result;

	debug("add_opt()");
	result = dns_message_buildopt(msg, &rdataset, edns, udpsize, flags,
				      opts, count);
	check_result(result, "dns_message_buildopt");
	result = dns_message_setopt(msg, rdataset);
	check_result(result, "dns_message_setopt");
}

/*%
 * Add a question section to a message, asking for the specified name,
 * type, and class.
 */
static void
add_question(dns_message_t *message, dns_name_t *name,
	     dns_rdataclass_t rdclass, dns_rdatatype_t rdtype)
{
	dns_rdataset_t *rdataset;
	isc_result_t result;

	debug("add_question()");
	rdataset = NULL;
	result = dns_message_gettemprdataset(message, &rdataset);
	check_result(result, "dns_message_gettemprdataset()");
	dns_rdataset_makequestion(rdataset, rdclass, rdtype);
	ISC_LIST_APPEND(name->list, rdataset, link);
}

/*%
 * Check if we're done with all the queued lookups, which is true iff
 * all sockets, sends, and recvs are accounted for (counters == 0),
 * and the lookup list is empty.
 * If we are done, pass control back out to dighost_shutdown() (which is
 * part of dig.c, host.c, or nslookup.c) to either shutdown the system as
 * a whole or reseed the lookup list.
 */
static void
check_if_done(void) {
	debug("check_if_done()");
	debug("list %s", ISC_LIST_EMPTY(lookup_list) ? "empty" : "full");
	if (ISC_LIST_EMPTY(lookup_list) && current_lookup == NULL &&
	    sendcount == 0) {
		INSIST(sockcount == 0);
		INSIST(recvcount == 0);
		debug("shutting down");
		dighost_shutdown();
	}
}

/*%
 * Clear out a query when we're done with it.  WARNING: This routine
 * WILL invalidate the query pointer.
 */
static void
clear_query(dig_query_t *query) {
	dig_lookup_t *lookup;

	REQUIRE(query != NULL);

	debug("clear_query(%p)", query);

	if (query->timer != NULL)
		isc_timer_detach(&query->timer);
	lookup = query->lookup;

	if (lookup->current_query == query)
		lookup->current_query = NULL;

	if (ISC_LINK_LINKED(query, link))
		ISC_LIST_UNLINK(lookup->q, query, link);
	if (ISC_LINK_LINKED(query, clink))
		ISC_LIST_UNLINK(lookup->connecting, query, clink);
	if (ISC_LINK_LINKED(&query->recvbuf, link))
		ISC_LIST_DEQUEUE(query->recvlist, &query->recvbuf,
				 link);
	if (ISC_LINK_LINKED(&query->lengthbuf, link))
		ISC_LIST_DEQUEUE(query->lengthlist, &query->lengthbuf,
				 link);
	INSIST(query->recvspace != NULL);

	if (query->sock != NULL) {
		isc_socket_detach(&query->sock);
		sockcount--;
		debug("sockcount=%d", sockcount);
	}
	free(query->recvspace);
	isc_buffer_invalidate(&query->recvbuf);
	isc_buffer_invalidate(&query->lengthbuf);
	if (query->waiting_senddone)
		query->pending_free = 1;
	else
		free(query);
}

/*%
 * Try and clear out a lookup if we're done with it.  Return 1 if
 * the lookup was successfully cleared.  If 1 is returned, the
 * lookup pointer has been invalidated.
 */
static int
try_clear_lookup(dig_lookup_t *lookup) {
	dig_query_t *q;

	REQUIRE(lookup != NULL);

	debug("try_clear_lookup(%p)", lookup);

	if (ISC_LIST_HEAD(lookup->q) != NULL ||
	    ISC_LIST_HEAD(lookup->connecting) != NULL)
	{
		if (debugging) {
			q = ISC_LIST_HEAD(lookup->q);
			while (q != NULL) {
				debug("query to %s still pending", q->servname);
				q = ISC_LIST_NEXT(q, link);
			}

			q = ISC_LIST_HEAD(lookup->connecting);
			while (q != NULL) {
				debug("query to %s still connecting",
				      q->servname);
				q = ISC_LIST_NEXT(q, clink);
			}
		}
		return (0);
	}

	/*
	 * At this point, we know there are no queries on the lookup,
	 * so can make it go away also.
	 */
	destroy_lookup(lookup);
	return (1);
}

void
destroy_lookup(dig_lookup_t *lookup) {
	dig_server_t *s;
	void *ptr;

	debug("destroy");
	s = ISC_LIST_HEAD(lookup->my_server_list);
	while (s != NULL) {
		debug("freeing server %p belonging to %p", s, lookup);
		ptr = s;
		s = ISC_LIST_NEXT(s, link);
		ISC_LIST_DEQUEUE(lookup->my_server_list,
				 (dig_server_t *)ptr, link);
		free(ptr);
	}
	if (lookup->sendmsg != NULL)
		dns_message_destroy(&lookup->sendmsg);
	if (lookup->querysig != NULL) {
		debug("freeing buffer %p", lookup->querysig);
		isc_buffer_free(&lookup->querysig);
	}
	if (lookup->sendspace != NULL)
		free(lookup->sendspace);

	if (lookup->tsigctx != NULL)
		dst_context_destroy(&lookup->tsigctx);

	if (lookup->ecs_addr != NULL)
		free(lookup->ecs_addr);

	if (lookup->ednsopts != NULL) {
		size_t i;
		for (i = 0; i < EDNSOPT_OPTIONS; i++) {
			if (lookup->ednsopts[i].value != NULL)
				free(lookup->ednsopts[i].value);
		}
		free(lookup->ednsopts);
	}

	free(lookup);
}

/*%
 * If we can, start the next lookup in the queue running.
 * This assumes that the lookup on the head of the queue hasn't been
 * started yet.  It also removes the lookup from the head of the queue,
 * setting the current_lookup pointer pointing to it.
 */
void
start_lookup(void) {
	debug("start_lookup()");
	if (cancel_now)
		return;

	/*
	 * If there's a current lookup running, we really shouldn't get
	 * here.
	 */
	INSIST(current_lookup == NULL);

	current_lookup = ISC_LIST_HEAD(lookup_list);
	/*
	 * Put the current lookup somewhere so cancel_all can find it
	 */
	if (current_lookup != NULL) {
		ISC_LIST_DEQUEUE(lookup_list, current_lookup, link);
		if (setup_lookup(current_lookup))
			do_lookup(current_lookup);
		else if (next_origin(current_lookup))
			check_next_lookup(current_lookup);
	} else {
		check_if_done();
	}
}

/*%
 * If we can, clear the current lookup and start the next one running.
 * This calls try_clear_lookup, so may invalidate the lookup pointer.
 */
static void
check_next_lookup(dig_lookup_t *lookup) {

	INSIST(!free_now);

	debug("check_next_lookup(%p)", lookup);

	if (ISC_LIST_HEAD(lookup->q) != NULL) {
		debug("still have a worker");
		return;
	}
	if (try_clear_lookup(lookup)) {
		current_lookup = NULL;
		start_lookup();
	}
}

/*%
 * Create and queue a new lookup as a followup to the current lookup,
 * based on the supplied message and section.  This is used in trace and
 * name server search modes to start a new lookup using servers from
 * NS records in a reply. Returns the number of followup lookups made.
 */
static int
followup_lookup(dns_message_t *msg, dig_query_t *query, dns_section_t section)
{
	dig_lookup_t *lookup = NULL;
	dig_server_t *srv = NULL;
	dns_rdataset_t *rdataset = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_name_t *name = NULL;
	isc_result_t result;
	int success = 0;
	int numLookups = 0;
	int num;
	isc_result_t lresult, addresses_result;
	char bad_namestr[DNS_NAME_FORMATSIZE];
	dns_name_t *domain;
	int horizontal = 0, bad = 0;

	INSIST(!free_now);

	debug("following up %s", query->lookup->textname);

	addresses_result = ISC_R_SUCCESS;
	bad_namestr[0] = '\0';
	for (result = dns_message_firstname(msg, section);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(msg, section)) {
		name = NULL;
		dns_message_currentname(msg, section, &name);

		if (section == DNS_SECTION_AUTHORITY) {
			rdataset = NULL;
			result = dns_message_findtype(name, dns_rdatatype_soa,
						      0, &rdataset);
			if (result == ISC_R_SUCCESS)
				return (0);
		}
		rdataset = NULL;
		result = dns_message_findtype(name, dns_rdatatype_ns, 0,
					      &rdataset);
		if (result != ISC_R_SUCCESS)
			continue;

		debug("found NS set");

		if (query->lookup->trace && !query->lookup->trace_root) {
			dns_namereln_t namereln;
			unsigned int nlabels;
			int order;

			domain = dns_fixedname_name(&query->lookup->fdomain);
			namereln = dns_name_fullcompare(name, domain,
							&order, &nlabels);
			if (namereln == dns_namereln_equal) {
				if (!horizontal)
					printf(";; BAD (HORIZONTAL) REFERRAL\n");
				horizontal = 1;
			} else if (namereln != dns_namereln_subdomain) {
				if (!bad)
					printf(";; BAD REFERRAL\n");
				bad = 1;
				continue;
			}
		}

		for (result = dns_rdataset_first(rdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(rdataset)) {
			char namestr[DNS_NAME_FORMATSIZE];
			dns_rdata_ns_t ns;

			if (query->lookup->trace_root &&
			    query->lookup->nsfound >= MXSERV)
				break;

			dns_rdataset_current(rdataset, &rdata);

			query->lookup->nsfound++;
			result = dns_rdata_tostruct_ns(&rdata, &ns);
			check_result(result, "dns_rdata_tostruct_ns");
			dns_name_format(&ns.name, namestr, sizeof(namestr));
			dns_rdata_freestruct_ns(&ns);

			/* Initialize lookup if we've not yet */
			debug("found NS %s", namestr);
			if (!success) {
				success = 1;
				lookup_counter++;
				lookup = requeue_lookup(query->lookup,
							0);
				cancel_lookup(query->lookup);
				lookup->doing_xfr = 0;
				if (!lookup->trace_root &&
				    section == DNS_SECTION_ANSWER)
					lookup->trace = 0;
				else
					lookup->trace = query->lookup->trace;
				lookup->ns_search_only =
					query->lookup->ns_search_only;
				lookup->trace_root = 0;
				if (lookup->ns_search_only)
					lookup->recurse = 0;
				domain = dns_fixedname_name(&lookup->fdomain);
				dns_name_copy(name, domain, NULL);
			}
			debug("adding server %s", namestr);
			num = getaddresses(lookup, namestr, &lresult);
			if (lresult != ISC_R_SUCCESS) {
				printf("couldn't get address for '%s': %s\n",
				       namestr, isc_result_totext(lresult));
				if (addresses_result == ISC_R_SUCCESS) {
					addresses_result = lresult;
					strlcpy(bad_namestr, namestr,
						sizeof(bad_namestr));
				}
			}
			numLookups += num;
			dns_rdata_reset(&rdata);
		}
	}
	if (numLookups == 0 && addresses_result != ISC_R_SUCCESS) {
		fatal("couldn't get address for '%s': %s",
		      bad_namestr, isc_result_totext(result));
	}

	if (lookup == NULL &&
	    section == DNS_SECTION_ANSWER &&
	    (query->lookup->trace || query->lookup->ns_search_only))
		return (followup_lookup(msg, query, DNS_SECTION_AUTHORITY));

	/*
	 * Randomize the order the nameserver will be tried.
	 */
	if (numLookups > 1) {
		uint32_t i, j;
		dig_serverlist_t my_server_list;
		dig_server_t *next;

		ISC_LIST_INIT(my_server_list);

		i = numLookups;
		for (srv = ISC_LIST_HEAD(lookup->my_server_list);
		     srv != NULL;
		     srv = ISC_LIST_HEAD(lookup->my_server_list)) {
			INSIST(i > 0);
			j = arc4random_uniform(i);
			next = ISC_LIST_NEXT(srv, link);
			while (j-- > 0 && next != NULL) {
				srv = next;
				next = ISC_LIST_NEXT(srv, link);
			}
			ISC_LIST_DEQUEUE(lookup->my_server_list, srv, link);
			ISC_LIST_APPEND(my_server_list, srv, link);
			i--;
		}
		ISC_LIST_APPENDLIST(lookup->my_server_list,
				    my_server_list, link);
	}

	return (numLookups);
}

/*%
 * Create and queue a new lookup using the next origin from the search
 * list, read in setup_system().
 *
 * Return 1 iff there was another searchlist entry.
 */
static int
next_origin(dig_lookup_t *oldlookup) {
	dig_lookup_t *newlookup;
	dig_searchlist_t *search;
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_result_t result;

	INSIST(!free_now);

	debug("next_origin()");
	debug("following up %s", oldlookup->textname);

	if (!usesearch)
		/*
		 * We're not using a search list, so don't even think
		 * about finding the next entry.
		 */
		return (0);

	/*
	 * Check for a absolute name or ndots being met.
	 */
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_name_fromstring2(name, oldlookup->textname, NULL, 0);
	if (result == ISC_R_SUCCESS &&
	    (dns_name_isabsolute(name) ||
	     (int)dns_name_countlabels(name) > ndots))
		return (0);

	if (oldlookup->origin == NULL && !oldlookup->need_search)
		/*
		 * Then we just did rootorg; there's nothing left.
		 */
		return (0);
	if (oldlookup->origin == NULL && oldlookup->need_search) {
		newlookup = requeue_lookup(oldlookup, 1);
		newlookup->origin = ISC_LIST_HEAD(search_list);
		newlookup->need_search = 0;
	} else {
		search = ISC_LIST_NEXT(oldlookup->origin, link);
		if (search == NULL && oldlookup->done_as_is)
			return (0);
		newlookup = requeue_lookup(oldlookup, 1);
		newlookup->origin = search;
	}
	cancel_lookup(oldlookup);
	return (1);
}

/*%
 * Insert an SOA record into the sendmessage in a lookup.  Used for
 * creating IXFR queries.
 */
static void
insert_soa(dig_lookup_t *lookup) {
	isc_result_t result;
	dns_rdata_soa_t soa;
	dns_rdata_t *rdata = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataset_t *rdataset = NULL;
	dns_name_t *soaname = NULL;

	debug("insert_soa()");
	soa.serial = lookup->ixfr_serial;
	soa.refresh = 0;
	soa.retry = 0;
	soa.expire = 0;
	soa.minimum = 0;
	soa.common.rdclass = lookup->rdclass;
	soa.common.rdtype = dns_rdatatype_soa;

	dns_name_init(&soa.origin, NULL);
	dns_name_init(&soa.contact, NULL);

	dns_name_clone(dns_rootname, &soa.origin);
	dns_name_clone(dns_rootname, &soa.contact);

	isc_buffer_init(&lookup->rdatabuf, lookup->rdatastore,
			sizeof(lookup->rdatastore));

	result = dns_message_gettemprdata(lookup->sendmsg, &rdata);
	check_result(result, "dns_message_gettemprdata");

	result = dns_rdata_fromstruct_soa(rdata, lookup->rdclass,
				      dns_rdatatype_soa, &soa,
				      &lookup->rdatabuf);
	check_result(result, "isc_rdata_fromstruct_soa");

	result = dns_message_gettemprdatalist(lookup->sendmsg, &rdatalist);
	check_result(result, "dns_message_gettemprdatalist");

	result = dns_message_gettemprdataset(lookup->sendmsg, &rdataset);
	check_result(result, "dns_message_gettemprdataset");

	dns_rdatalist_init(rdatalist);
	rdatalist->type = dns_rdatatype_soa;
	rdatalist->rdclass = lookup->rdclass;
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);

	dns_rdatalist_tordataset(rdatalist, rdataset);

	result = dns_message_gettempname(lookup->sendmsg, &soaname);
	check_result(result, "dns_message_gettempname");
	dns_name_init(soaname, NULL);
	dns_name_clone(lookup->name, soaname);
	ISC_LIST_INIT(soaname->list);
	ISC_LIST_APPEND(soaname->list, rdataset, link);
	dns_message_addname(lookup->sendmsg, soaname, DNS_SECTION_AUTHORITY);
}

static void
compute_cookie(unsigned char *clientcookie, size_t len) {
	/* XXXMPA need to fix, should be per server. */
	INSIST(len >= 8U);
	memmove(clientcookie, cookie_secret, 8);
}

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
static void
populate_root_hints(void)
{
	dig_server_t *newsrv;
	size_t i;

	if (!ISC_LIST_EMPTY(root_hints_server_list))
		return;

	for (i = 0; i < nitems(root_hints); i++) {
		if (!have_ipv4 && root_hints[i].af == AF_INET)
			continue;
		if (!have_ipv6 && root_hints[i].af == AF_INET6)
			continue;
		newsrv = make_server(root_hints[i].ns, root_hints[i].ns);
		ISC_LINK_INIT(newsrv, link);
		ISC_LIST_ENQUEUE(root_hints_server_list, newsrv, link);
	}
}
#undef nitems

/*%
 * Setup the supplied lookup structure, making it ready to start sending
 * queries to servers.  Create and initialize the message to be sent as
 * well as the query structures and buffer space for the replies.  If the
 * server list is empty, clone it from the system default list.
 */
int
setup_lookup(dig_lookup_t *lookup) {
	isc_result_t result;
	uint32_t id;
	unsigned int len;
	dig_server_t *serv;
	dig_query_t *query;
	isc_buffer_t b;
	dns_compress_t cctx;
	char store[MXNAME];
	char ecsbuf[20];
	char sitbuf[256];

	REQUIRE(lookup != NULL);
	INSIST(!free_now);

	debug("setup_lookup(%p)", lookup);

	result = dns_message_create(DNS_MESSAGE_INTENTRENDER,
				    &lookup->sendmsg);
	check_result(result, "dns_message_create");

	if (lookup->new_search) {
		debug("resetting lookup counter.");
		lookup_counter = 0;
	}

	if (ISC_LIST_EMPTY(lookup->my_server_list)) {
		if (lookup->trace && lookup->trace_root) {
			populate_root_hints();
			clone_server_list(root_hints_server_list,
			    &lookup->my_server_list);
		} else {
			debug("cloning server list");
			clone_server_list(server_list,
			    &lookup->my_server_list);
		}
	}
	result = dns_message_gettempname(lookup->sendmsg, &lookup->name);
	check_result(result, "dns_message_gettempname");
	dns_name_init(lookup->name, NULL);

	isc_buffer_init(&lookup->namebuf, lookup->name_space,
			sizeof(lookup->name_space));
	isc_buffer_init(&lookup->onamebuf, lookup->oname_space,
			sizeof(lookup->oname_space));

	/*
	 * If the name has too many dots, force the origin to be NULL
	 * (which produces an absolute lookup).  Otherwise, take the origin
	 * we have if there's one in the struct already.  If it's NULL,
	 * take the first entry in the searchlist iff either usesearch
	 * is TRUE or we got a domain line in the resolv.conf file.
	 */
	if (lookup->new_search) {
		if ((count_dots(lookup->textname) >= ndots) || !usesearch) {
			lookup->origin = NULL; /* Force abs lookup */
			lookup->done_as_is = 1;
			lookup->need_search = usesearch;
		} else if (lookup->origin == NULL && usesearch) {
			lookup->origin = ISC_LIST_HEAD(search_list);
			lookup->need_search = 0;
		}
	}

	if (lookup->origin != NULL) {
		debug("trying origin %s", lookup->origin->origin);
		result = dns_message_gettempname(lookup->sendmsg,
						 &lookup->oname);
		check_result(result, "dns_message_gettempname");
		dns_name_init(lookup->oname, NULL);
		/* XXX Helper funct to conv char* to name? */
		len = (unsigned int) strlen(lookup->origin->origin);
		isc_buffer_init(&b, lookup->origin->origin, len);
		isc_buffer_add(&b, len);
		result = dns_name_fromtext(lookup->oname, &b, dns_rootname,
					   0, &lookup->onamebuf);
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(lookup->sendmsg,
						&lookup->name);
			dns_message_puttempname(lookup->sendmsg,
						&lookup->oname);
			fatal("'%s' is not in legal name syntax (%s)",
			      lookup->origin->origin,
			      isc_result_totext(result));
		}
		if (lookup->trace && lookup->trace_root) {
			dns_name_clone(dns_rootname, lookup->name);
		} else {
			dns_fixedname_t fixed;
			dns_name_t *name;

			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			len = (unsigned int) strlen(lookup->textname);
			isc_buffer_init(&b, lookup->textname, len);
			isc_buffer_add(&b, len);
			result = dns_name_fromtext(name, &b, NULL, 0, NULL);
			if (result == ISC_R_SUCCESS &&
			    !dns_name_isabsolute(name))
				result = dns_name_concatenate(name,
							      lookup->oname,
							      lookup->name,
							      &lookup->namebuf);
			else if (result == ISC_R_SUCCESS)
				result = dns_name_copy(name, lookup->name,
						       &lookup->namebuf);
			if (result != ISC_R_SUCCESS) {
				dns_message_puttempname(lookup->sendmsg,
							&lookup->name);
				dns_message_puttempname(lookup->sendmsg,
							&lookup->oname);
				if (result == DNS_R_NAMETOOLONG)
					return (0);
				fatal("'%s' is not in legal name syntax (%s)",
				      lookup->textname,
				      isc_result_totext(result));
			}
		}
		dns_message_puttempname(lookup->sendmsg, &lookup->oname);
	} else
	{
		debug("using root origin");
		if (lookup->trace && lookup->trace_root)
			dns_name_clone(dns_rootname, lookup->name);
		else {
			len = (unsigned int) strlen(lookup->textname);
			isc_buffer_init(&b, lookup->textname, len);
			isc_buffer_add(&b, len);
			result = dns_name_fromtext(lookup->name, &b,
						   dns_rootname, 0,
						   &lookup->namebuf);
		}
		if (result != ISC_R_SUCCESS) {
			dns_message_puttempname(lookup->sendmsg,
						&lookup->name);
			fatal("'%s' is not a legal name "
			      "(%s)", lookup->textname,
			      isc_result_totext(result));
		}
	}
	dns_name_format(lookup->name, store, sizeof(store));
	dighost_trying(store, lookup);
	INSIST(dns_name_isabsolute(lookup->name));

	id = arc4random();
	lookup->sendmsg->id = (unsigned short)id & 0xFFFF;
	lookup->sendmsg->opcode = lookup->opcode;
	lookup->msgcounter = 0;
	/*
	 * If this is a trace request, completely disallow recursion, since
	 * it's meaningless for traces.
	 */
	if (lookup->trace || (lookup->ns_search_only && !lookup->trace_root))
		lookup->recurse = 0;

	if (lookup->recurse &&
	    lookup->rdtype != dns_rdatatype_axfr &&
	    lookup->rdtype != dns_rdatatype_ixfr) {
		debug("recursive query");
		lookup->sendmsg->flags |= DNS_MESSAGEFLAG_RD;
	}

	/* XXX aaflag */
	if (lookup->aaonly) {
		debug("AA query");
		lookup->sendmsg->flags |= DNS_MESSAGEFLAG_AA;
	}

	if (lookup->adflag) {
		debug("AD query");
		lookup->sendmsg->flags |= DNS_MESSAGEFLAG_AD;
	}

	if (lookup->cdflag) {
		debug("CD query");
		lookup->sendmsg->flags |= DNS_MESSAGEFLAG_CD;
	}

	dns_message_addname(lookup->sendmsg, lookup->name,
			    DNS_SECTION_QUESTION);

	if (lookup->trace && lookup->trace_root) {
		lookup->qrdtype = lookup->rdtype;
		lookup->rdtype = dns_rdatatype_ns;
	}

	if ((lookup->rdtype == dns_rdatatype_axfr) ||
	    (lookup->rdtype == dns_rdatatype_ixfr)) {
		/*
		 * Force TCP mode if we're doing an axfr.
		 */
		if (lookup->rdtype == dns_rdatatype_axfr) {
			lookup->doing_xfr = 1;
			lookup->tcp_mode = 1;
		} else if (lookup->tcp_mode) {
			lookup->doing_xfr = 1;
		}
	}

	add_question(lookup->sendmsg, lookup->name, lookup->rdclass,
		     lookup->rdtype);

	/* add_soa */
	if (lookup->rdtype == dns_rdatatype_ixfr)
		insert_soa(lookup);

	/* XXX Insist this? */
	lookup->tsigctx = NULL;
	lookup->querysig = NULL;
	if (tsigkey != NULL) {
		debug("initializing keys");
		result = dns_message_settsigkey(lookup->sendmsg, tsigkey);
		check_result(result, "dns_message_settsigkey");
	}

	lookup->sendspace = malloc(COMMSIZE);
	if (lookup->sendspace == NULL)
		fatal("memory allocation failure");

	result = dns_compress_init(&cctx, -1);
	check_result(result, "dns_compress_init");

	debug("starting to render the message");
	isc_buffer_init(&lookup->renderbuf, lookup->sendspace, COMMSIZE);
	result = dns_message_renderbegin(lookup->sendmsg, &cctx,
					 &lookup->renderbuf);
	check_result(result, "dns_message_renderbegin");
	if (lookup->udpsize > 0 || lookup->dnssec ||
	    lookup->edns > -1 || lookup->ecs_addr != NULL)
	{
#define MAXOPTS (EDNSOPT_OPTIONS + DNS_EDNSOPTIONS)
		dns_ednsopt_t opts[MAXOPTS];
		unsigned int flags;
		unsigned int i = 0;

		if (lookup->udpsize == 0)
			lookup->udpsize = 4096;
		if (lookup->edns < 0)
			lookup->edns = 0;

		if (lookup->nsid) {
			INSIST(i < MAXOPTS);
			opts[i].code = DNS_OPT_NSID;
			opts[i].length = 0;
			opts[i].value = NULL;
			i++;
		}

		if (lookup->ecs_addr != NULL) {
			uint8_t addr[16];
			uint16_t family;
			uint32_t plen;
			struct sockaddr *sa;
			struct sockaddr_in *sin;
			struct sockaddr_in6 *sin6;
			size_t addrl;

			sa = (struct sockaddr *)lookup->ecs_addr;
			plen = lookup->ecs_plen;

			/* Round up prefix len to a multiple of 8 */
			addrl = (plen + 7) / 8;

			INSIST(i < MAXOPTS);
			opts[i].code = DNS_OPT_CLIENT_SUBNET;
			opts[i].length = (uint16_t) addrl + 4;
			check_result(result, "isc_buffer_allocate");

			/*
			 * XXXMUKS: According to RFC7871, "If there is
			 * no ADDRESS set, i.e., SOURCE PREFIX-LENGTH is
			 * set to 0, then FAMILY SHOULD be set to the
			 * transport over which the query is sent."
			 *
			 * However, at this point we don't know what
			 * transport(s) we'll be using, so we can't
			 * set the value now. For now, we're using
			 * IPv4 as the default the +subnet option
			 * used an IPv4 prefix, or for +subnet=0,
			 * and IPv6 if the +subnet option used an
			 * IPv6 prefix.
			 *
			 * (For future work: preserve the offset into
			 * the buffer where the family field is;
			 * that way we can update it in send_udp()
			 * or send_tcp_connect() once we know
			 * what it outght to be.)
			 */
			switch (sa->sa_family) {
			case AF_UNSPEC:
				INSIST(plen == 0);
				family = 1;
				break;
			case AF_INET:
				INSIST(plen <= 32);
				family = 1;
				sin = (struct sockaddr_in *) sa;
				memmove(addr, &sin->sin_addr, addrl);
				break;
			case AF_INET6:
				INSIST(plen <= 128);
				family = 2;
				sin6 = (struct sockaddr_in6 *) sa;
				memmove(addr, &sin6->sin6_addr, addrl);
				break;
			default:
				INSIST(0);
			}

			isc_buffer_init(&b, ecsbuf, sizeof(ecsbuf));
			/* family */
			isc_buffer_putuint16(&b, family);
			/* source prefix-length */
			isc_buffer_putuint8(&b, plen);
			/* scope prefix-length */
			isc_buffer_putuint8(&b, 0);

			/* address */
			if (addrl > 0) {
				/* Mask off last address byte */
				if ((plen % 8) != 0)
					addr[addrl - 1] &=
						~0U << (8 - (plen % 8));
				isc_buffer_putmem(&b, addr,
						  (unsigned)addrl);
			}

			opts[i].value = (uint8_t *) ecsbuf;
			i++;
		}

		if (lookup->sit) {
			INSIST(i < MAXOPTS);
			opts[i].code = DNS_OPT_COOKIE;
			if (lookup->sitvalue != NULL) {
				isc_buffer_init(&b, sitbuf, sizeof(sitbuf));
				result = isc_hex_decodestring(lookup->sitvalue,
							      &b);
				check_result(result, "isc_hex_decodestring");
				opts[i].value = isc_buffer_base(&b);
				opts[i].length = isc_buffer_usedlength(&b);
			} else {
				compute_cookie(cookie, sizeof(cookie));
				opts[i].length = 8;
				opts[i].value = cookie;
			}
			i++;
		}

		if (lookup->expire) {
			INSIST(i < MAXOPTS);
			opts[i].code = DNS_OPT_EXPIRE;
			opts[i].length = 0;
			opts[i].value = NULL;
			i++;
		}

		if (lookup->ednsoptscnt != 0) {
			INSIST(i + lookup->ednsoptscnt <= MAXOPTS);
			memmove(&opts[i], lookup->ednsopts,
				sizeof(dns_ednsopt_t) * lookup->ednsoptscnt);
			i += lookup->ednsoptscnt;
		}

		flags = lookup->ednsflags;
		flags &= ~DNS_MESSAGEEXTFLAG_DO;
		if (lookup->dnssec)
			flags |= DNS_MESSAGEEXTFLAG_DO;
		add_opt(lookup->sendmsg, lookup->udpsize,
			lookup->edns, flags, opts, i);
	}

	result = dns_message_rendersection(lookup->sendmsg,
					   DNS_SECTION_QUESTION);
	check_result(result, "dns_message_rendersection");
	result = dns_message_rendersection(lookup->sendmsg,
					   DNS_SECTION_AUTHORITY);
	check_result(result, "dns_message_rendersection");
	result = dns_message_renderend(lookup->sendmsg);
	check_result(result, "dns_message_renderend");
	debug("done rendering");

	dns_compress_invalidate(&cctx);

	/*
	 * Force TCP mode if the request is larger than 512 bytes.
	 */
	if (isc_buffer_usedlength(&lookup->renderbuf) > 512)
		lookup->tcp_mode = 1;

	lookup->pending = 0;

	for (serv = ISC_LIST_HEAD(lookup->my_server_list);
	     serv != NULL;
	     serv = ISC_LIST_NEXT(serv, link)) {
		query = malloc(sizeof(dig_query_t));
		if (query == NULL)
			fatal("memory allocation failure in %s:%d",
			      __FILE__, __LINE__);
		debug("create query %p linked to lookup %p",
		       query, lookup);
		query->lookup = lookup;
		query->timer = NULL;
		query->waiting_connect = 0;
		query->waiting_senddone = 0;
		query->pending_free = 0;
		query->recv_made = 0;
		query->first_pass = 1;
		query->first_soa_rcvd = 0;
		query->second_rr_rcvd = 0;
		query->first_repeat_rcvd = 0;
		query->warn_id = 1;
		query->timedout = 0;
		query->first_rr_serial = 0;
		query->second_rr_serial = 0;
		query->servname = serv->servername;
		query->userarg = serv->userarg;
		query->rr_count = 0;
		query->msg_count = 0;
		query->byte_count = 0;
		query->ixfr_axfr = 0;
		ISC_LIST_INIT(query->recvlist);
		ISC_LIST_INIT(query->lengthlist);
		query->sock = NULL;
		query->recvspace = malloc(COMMSIZE);
		if (query->recvspace == NULL)
			fatal("memory allocation failure");

		isc_buffer_init(&query->recvbuf, query->recvspace, COMMSIZE);
		isc_buffer_init(&query->lengthbuf, query->lengthspace, 2);
		isc_buffer_init(&query->slbuf, query->slspace, 2);
		query->sendbuf = lookup->renderbuf;

		ISC_LINK_INIT(query, clink);
		ISC_LINK_INIT(query, link);
		ISC_LIST_ENQUEUE(lookup->q, query, link);
	}

	/* XXX qrflag, print_query, etc... */
	if (!ISC_LIST_EMPTY(lookup->q) && qr) {
		extrabytes = 0;
		dighost_printmessage(ISC_LIST_HEAD(lookup->q), lookup->sendmsg,
			     1);
	}
	return (1);
}

/*%
 * Event handler for send completion.  Track send counter, and clear out
 * the query if the send was canceled.
 */
static void
send_done(isc_task_t *_task, isc_event_t *event) {
	isc_socketevent_t *sevent = (isc_socketevent_t *)event;
	isc_buffer_t *b = NULL;
	dig_query_t *query, *next;
	dig_lookup_t *l;

	REQUIRE(event->ev_type == ISC_SOCKEVENT_SENDDONE);

	UNUSED(_task);

	debug("send_done()");
	sendcount--;
	debug("sendcount=%d", sendcount);
	INSIST(sendcount >= 0);

	for  (b = ISC_LIST_HEAD(sevent->bufferlist);
	      b != NULL;
	      b = ISC_LIST_HEAD(sevent->bufferlist)) {
		ISC_LIST_DEQUEUE(sevent->bufferlist, b, link);
		free(b);
	}

	query = event->ev_arg;
	query->waiting_senddone = 0;
	l = query->lookup;

	if (l->ns_search_only && !l->trace_root && !l->tcp_mode) {
		debug("sending next, since searching");
		next = ISC_LIST_NEXT(query, link);
		if (next != NULL)
			send_udp(next);
	}

	isc_event_free(&event);

	if (query->pending_free)
		free(query);

	check_if_done();
}

/*%
 * Cancel a lookup, sending isc_socket_cancel() requests to all outstanding
 * IO sockets.  The cancel handlers should take care of cleaning up the
 * query and lookup structures
 */
static void
cancel_lookup(dig_lookup_t *lookup) {
	dig_query_t *query, *next;

	debug("cancel_lookup()");
	query = ISC_LIST_HEAD(lookup->q);
	while (query != NULL) {
		next = ISC_LIST_NEXT(query, link);
		if (query->sock != NULL) {
			isc_socket_cancel(query->sock, global_task,
					  ISC_SOCKCANCEL_ALL);
			check_if_done();
		} else {
			clear_query(query);
		}
		query = next;
	}
	lookup->pending = 0;
	lookup->retries = 0;
}

static void
bringup_timer(dig_query_t *query, unsigned int default_timeout) {
	dig_lookup_t *l;
	unsigned int local_timeout;
	isc_result_t result;

	debug("bringup_timer()");
	/*
	 * If the timer already exists, that means we're calling this
	 * a second time (for a retry).  Don't need to recreate it,
	 * just reset it.
	 */
	l = query->lookup;
	if (ISC_LINK_LINKED(query, link) && ISC_LIST_NEXT(query, link) != NULL)
		local_timeout = SERVER_TIMEOUT;
	else {
		if (timeout == 0)
			local_timeout = default_timeout;
		else
			local_timeout = timeout;
	}
	debug("have local timeout of %d", local_timeout);
	l->interval.tv_sec = local_timeout;
	l->interval.tv_nsec = 0;
	if (query->timer != NULL)
		isc_timer_detach(&query->timer);
	result = isc_timer_create(timermgr,
				  &l->interval, global_task, connect_timeout,
				  query, &query->timer);
	check_result(result, "isc_timer_create");
}

static void
force_timeout(dig_query_t *query) {
	isc_event_t *event;

	debug("force_timeout ()");
	event = isc_event_allocate(query, ISC_TIMEREVENT_IDLE,
				   connect_timeout, query,
				   sizeof(isc_event_t));
	if (event == NULL) {
		fatal("isc_event_allocate: %s",
		      isc_result_totext(ISC_R_NOMEMORY));
	}
	isc_task_send(global_task, &event);

	/*
	 * The timer may have expired if, for example, get_address() takes
	 * long time and the timer was running on a different thread.
	 * We need to cancel the possible timeout event not to confuse
	 * ourselves due to the duplicate events.
	 */
	if (query->timer != NULL)
		isc_timer_detach(&query->timer);
}

static void
connect_done(isc_task_t *task, isc_event_t *event);

/*%
 * Unlike send_udp, this can't be called multiple times with the same
 * query.  When we retry TCP, we requeue the whole lookup, which should
 * start anew.
 */
static void
send_tcp_connect(dig_query_t *query) {
	isc_result_t result;
	dig_query_t *next;
	dig_lookup_t *l;

	debug("send_tcp_connect(%p)", query);

	l = query->lookup;
	query->waiting_connect = 1;
	query->lookup->current_query = query;
	result = get_address(query->servname, port, &query->sockaddr);
	if (result != ISC_R_SUCCESS) {
		/*
		 * This servname doesn't have an address.  Try the next server
		 * by triggering an immediate 'timeout' (we lie, but the effect
		 * is the same).
		 */
		force_timeout(query);
		return;
	}

	if (specified_source &&
	    (isc_sockaddr_pf(&query->sockaddr) !=
	     isc_sockaddr_pf(&bind_address))) {
		printf(";; Skipping server %s, incompatible "
		       "address family\n", query->servname);
		query->waiting_connect = 0;
		if (ISC_LINK_LINKED(query, link))
			next = ISC_LIST_NEXT(query, link);
		else
			next = NULL;
		l = query->lookup;
		clear_query(query);
		if (next == NULL) {
			printf(";; No acceptable nameservers\n");
			check_next_lookup(l);
			return;
		}
		send_tcp_connect(next);
		return;
	}

	INSIST(query->sock == NULL);

	if (keep != NULL && isc_sockaddr_equal(&keepaddr, &query->sockaddr)) {
		sockcount++;
		isc_socket_attach(keep, &query->sock);
		query->waiting_connect = 0;
		launch_next_query(query, 1);
		goto search;
	}

	result = isc_socket_create(socketmgr,
				   isc_sockaddr_pf(&query->sockaddr),
				   isc_sockettype_tcp, &query->sock);
	check_result(result, "isc_socket_create");
	sockcount++;
	debug("sockcount=%d", sockcount);
	if (specified_source)
		result = isc_socket_bind(query->sock, &bind_address,
					 ISC_SOCKET_REUSEADDRESS);
	else {
		if ((isc_sockaddr_pf(&query->sockaddr) == AF_INET) &&
		    have_ipv4)
			isc_sockaddr_any(&bind_any);
		else
			isc_sockaddr_any6(&bind_any);
		result = isc_socket_bind(query->sock, &bind_any, 0);
	}
	check_result(result, "isc_socket_bind");
	bringup_timer(query, TCP_TIMEOUT);
	result = isc_socket_connect(query->sock, &query->sockaddr,
				    global_task, connect_done, query);
	check_result(result, "isc_socket_connect");
 search:
	/*
	 * If we're at the endgame of a nameserver search, we need to
	 * immediately bring up all the queries.  Do it here.
	 */
	if (l->ns_search_only && !l->trace_root) {
		debug("sending next, since searching");
		if (ISC_LINK_LINKED(query, link)) {
			next = ISC_LIST_NEXT(query, link);
			ISC_LIST_DEQUEUE(l->q, query, link);
		} else
			next = NULL;
		ISC_LIST_ENQUEUE(l->connecting, query, clink);
		if (next != NULL)
			send_tcp_connect(next);
	}
}

static isc_buffer_t *
clone_buffer(isc_buffer_t *source) {
	isc_buffer_t *buffer;
	buffer = malloc(sizeof(*buffer));
	if (buffer == NULL)
		fatal("memory allocation failure in %s:%d",
		      __FILE__, __LINE__);
	*buffer = *source;
	return (buffer);
}

/*%
 * Send a UDP packet to the remote nameserver, possible starting the
 * recv action as well.  Also make sure that the timer is running and
 * is properly reset.
 */
static void
send_udp(dig_query_t *query) {
	dig_lookup_t *l = NULL;
	isc_result_t result;
	isc_buffer_t *sendbuf;

	debug("send_udp(%p)", query);

	l = query->lookup;
	bringup_timer(query, UDP_TIMEOUT);
	l->current_query = query;
	debug("working on lookup %p, query %p", query->lookup, query);
	if (!query->recv_made) {
		/* XXX Check the sense of this, need assertion? */
		query->waiting_connect = 0;
		result = get_address(query->servname, port, &query->sockaddr);
		if (result != ISC_R_SUCCESS) {
			/* This servname doesn't have an address. */
			force_timeout(query);
			return;
		}

		result = isc_socket_create(socketmgr,
					   isc_sockaddr_pf(&query->sockaddr),
					   isc_sockettype_udp, &query->sock);
		check_result(result, "isc_socket_create");
		sockcount++;
		debug("sockcount=%d", sockcount);
		if (specified_source) {
			result = isc_socket_bind(query->sock, &bind_address,
						 ISC_SOCKET_REUSEADDRESS);
		} else {
			isc_sockaddr_anyofpf(&bind_any,
					isc_sockaddr_pf(&query->sockaddr));
			result = isc_socket_bind(query->sock, &bind_any, 0);
		}
		check_result(result, "isc_socket_bind");

		query->recv_made = 1;
		ISC_LINK_INIT(&query->recvbuf, link);
		ISC_LIST_ENQUEUE(query->recvlist, &query->recvbuf,
				 link);
		debug("recving with lookup=%p, query=%p, sock=%p",
		      query->lookup, query, query->sock);
		result = isc_socket_recvv(query->sock, &query->recvlist, 1,
					  global_task, recv_done, query);
		check_result(result, "isc_socket_recvv");
		recvcount++;
		debug("recvcount=%d", recvcount);
	}
	ISC_LIST_INIT(query->sendlist);
	sendbuf = clone_buffer(&query->sendbuf);
	ISC_LIST_ENQUEUE(query->sendlist, sendbuf, link);
	debug("sending a request");
	clock_gettime(CLOCK_MONOTONIC, &query->time_sent);
	INSIST(query->sock != NULL);
	query->waiting_senddone = 1;
	result = isc_socket_sendtov2(query->sock, &query->sendlist,
				     global_task, send_done, query,
				     &query->sockaddr, NULL,
				     ISC_SOCKFLAG_NORETRY);
	check_result(result, "isc_socket_sendtov");
	sendcount++;
}

/*%
 * IO timeout handler, used for both connect and recv timeouts.  If
 * retries are still allowed, either resend the UDP packet or queue a
 * new TCP lookup.  Otherwise, cancel the lookup.
 */
static void
connect_timeout(isc_task_t *task, isc_event_t *event) {
	dig_lookup_t *l = NULL;
	dig_query_t *query = NULL, *cq;

	UNUSED(task);
	REQUIRE(event->ev_type == ISC_TIMEREVENT_IDLE);

	debug("connect_timeout()");

	query = event->ev_arg;
	l = query->lookup;
	isc_event_free(&event);

	INSIST(!free_now);

	if ((query != NULL) && (query->lookup->current_query != NULL) &&
	    ISC_LINK_LINKED(query->lookup->current_query, link) &&
	    (ISC_LIST_NEXT(query->lookup->current_query, link) != NULL)) {
		debug("trying next server...");
		cq = query->lookup->current_query;
		if (!l->tcp_mode)
			send_udp(ISC_LIST_NEXT(cq, link));
		else {
			if (query->sock != NULL)
				isc_socket_cancel(query->sock, NULL,
						  ISC_SOCKCANCEL_ALL);
			send_tcp_connect(ISC_LIST_NEXT(cq, link));
		}
		return;
	}

	if (l->tcp_mode && query->sock != NULL) {
		query->timedout = 1;
		isc_socket_cancel(query->sock, NULL, ISC_SOCKCANCEL_ALL);
	}

	if (l->retries > 1) {
		if (!l->tcp_mode) {
			l->retries--;
			debug("resending UDP request to first server");
			send_udp(ISC_LIST_HEAD(l->q));
		} else {
			debug("making new TCP request, %d tries left",
			      l->retries);
			l->retries--;
			requeue_lookup(l, 1);
			cancel_lookup(l);
			check_next_lookup(l);
		}
	} else {
		if (!l->ns_search_only) {
			fputs(l->cmdline, stdout);
			printf(";; connection timed out; no servers could be "
			       "reached\n");
		}
		cancel_lookup(l);
		check_next_lookup(l);
		if (exitcode < 9)
			exitcode = 9;
	}
}

/*%
 * Event handler for the TCP recv which gets the length header of TCP
 * packets.  Start the next recv of length bytes.
 */
static void
tcp_length_done(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent;
	isc_buffer_t *b = NULL;
	isc_result_t result;
	dig_query_t *query = NULL;
	dig_lookup_t *l, *n;
	uint16_t length;

	REQUIRE(event->ev_type == ISC_SOCKEVENT_RECVDONE);
	INSIST(!free_now);

	UNUSED(task);

	debug("tcp_length_done()");

	sevent = (isc_socketevent_t *)event;
	query = event->ev_arg;

	recvcount--;
	INSIST(recvcount >= 0);

	b = ISC_LIST_HEAD(sevent->bufferlist);
	INSIST(b ==  &query->lengthbuf);
	ISC_LIST_DEQUEUE(sevent->bufferlist, b, link);

	if (sevent->result == ISC_R_CANCELED) {
		isc_event_free(&event);
		l = query->lookup;
		clear_query(query);
		check_next_lookup(l);
		return;
	}
	if (sevent->result != ISC_R_SUCCESS) {
		char sockstr[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(&query->sockaddr, sockstr,
				    sizeof(sockstr));
		printf(";; communications error to %s: %s\n",
		       sockstr, isc_result_totext(sevent->result));
		if (keep != NULL)
			isc_socket_detach(&keep);
		l = query->lookup;
		isc_socket_detach(&query->sock);
		sockcount--;
		debug("sockcount=%d", sockcount);
		INSIST(sockcount >= 0);
		if (sevent->result == ISC_R_EOF && l->eoferr == 0U) {
			n = requeue_lookup(l, 1);
			n->eoferr++;
		}
		isc_event_free(&event);
		clear_query(query);
		cancel_lookup(l);
		check_next_lookup(l);
		return;
	}
	length = isc_buffer_getuint16(b);
	if (length == 0) {
		isc_event_free(&event);
		launch_next_query(query, 0);
		return;
	}

	/*
	 * Even though the buffer was already init'ed, we need
	 * to redo it now, to force the length we want.
	 */
	isc_buffer_invalidate(&query->recvbuf);
	isc_buffer_init(&query->recvbuf, query->recvspace, length);
	ENSURE(ISC_LIST_EMPTY(query->recvlist));
	ISC_LINK_INIT(&query->recvbuf, link);
	ISC_LIST_ENQUEUE(query->recvlist, &query->recvbuf, link);
	debug("recving with lookup=%p, query=%p", query->lookup, query);
	result = isc_socket_recvv(query->sock, &query->recvlist, length, task,
				  recv_done, query);
	check_result(result, "isc_socket_recvv");
	recvcount++;
	debug("resubmitted recv request with length %d, recvcount=%d",
	      length, recvcount);
	isc_event_free(&event);
}

/*%
 * For transfers that involve multiple recvs (XFR's in particular),
 * launch the next recv.
 */
static void
launch_next_query(dig_query_t *query, int include_question) {
	isc_result_t result;
	dig_lookup_t *l;
	isc_buffer_t *buffer;

	INSIST(!free_now);

	debug("launch_next_query()");

	if (!query->lookup->pending) {
		debug("ignoring launch_next_query because !pending");
		isc_socket_detach(&query->sock);
		sockcount--;
		debug("sockcount=%d", sockcount);
		INSIST(sockcount >= 0);
		query->waiting_connect = 0;
		l = query->lookup;
		clear_query(query);
		check_next_lookup(l);
		return;
	}

	isc_buffer_clear(&query->slbuf);
	isc_buffer_clear(&query->lengthbuf);
	isc_buffer_putuint16(&query->slbuf, (uint16_t) query->sendbuf.used);
	ISC_LIST_INIT(query->sendlist);
	ISC_LINK_INIT(&query->slbuf, link);
	if (!query->first_soa_rcvd) {
		buffer = clone_buffer(&query->slbuf);
		ISC_LIST_ENQUEUE(query->sendlist, buffer, link);
		if (include_question) {
			buffer = clone_buffer(&query->sendbuf);
			ISC_LIST_ENQUEUE(query->sendlist, buffer, link);
		}
	}

	ISC_LINK_INIT(&query->lengthbuf, link);
	ISC_LIST_ENQUEUE(query->lengthlist, &query->lengthbuf, link);

	result = isc_socket_recvv(query->sock, &query->lengthlist, 0,
				  global_task, tcp_length_done, query);
	check_result(result, "isc_socket_recvv");
	recvcount++;
	debug("recvcount=%d", recvcount);
	if (!query->first_soa_rcvd) {
		debug("sending a request in launch_next_query");
		clock_gettime(CLOCK_MONOTONIC, &query->time_sent);
		query->waiting_senddone = 1;
		result = isc_socket_sendv(query->sock, &query->sendlist,
					  global_task, send_done, query);
		check_result(result, "isc_socket_sendv");
		sendcount++;
		debug("sendcount=%d", sendcount);
	}
	query->waiting_connect = 0;
	return;
}

/*%
 * Event handler for TCP connect complete.  Make sure the connection was
 * successful, then pass into launch_next_query to actually send the
 * question.
 */
static void
connect_done(isc_task_t *task, isc_event_t *event) {
	char sockstr[ISC_SOCKADDR_FORMATSIZE];
	isc_socketevent_t *sevent = NULL;
	dig_query_t *query = NULL, *next;
	dig_lookup_t *l;

	UNUSED(task);

	REQUIRE(event->ev_type == ISC_SOCKEVENT_CONNECT);
	INSIST(!free_now);

	debug("connect_done()");

	sevent = (isc_socketevent_t *)event;
	query = sevent->ev_arg;

	INSIST(query->waiting_connect);

	query->waiting_connect = 0;

	if (sevent->result == ISC_R_CANCELED) {
		debug("in cancel handler");
		isc_sockaddr_format(&query->sockaddr, sockstr, sizeof(sockstr));
		if (query->timedout)
			printf(";; Connection to %s(%s) for %s failed: %s.\n",
			       sockstr, query->servname,
			       query->lookup->textname,
			       isc_result_totext(ISC_R_TIMEDOUT));
		isc_socket_detach(&query->sock);
		INSIST(sockcount > 0);
		sockcount--;
		debug("sockcount=%d", sockcount);
		query->waiting_connect = 0;
		isc_event_free(&event);
		l = query->lookup;
		clear_query(query);
		check_next_lookup(l);
		return;
	}
	if (sevent->result != ISC_R_SUCCESS) {

		debug("unsuccessful connection: %s",
		      isc_result_totext(sevent->result));
		isc_sockaddr_format(&query->sockaddr, sockstr, sizeof(sockstr));
		if (sevent->result != ISC_R_CANCELED)
			printf(";; Connection to %s(%s) for %s failed: "
			       "%s.\n", sockstr,
			       query->servname, query->lookup->textname,
			       isc_result_totext(sevent->result));
		isc_socket_detach(&query->sock);
		INSIST(sockcount > 0);
		sockcount--;
		/* XXX Clean up exitcodes */
		if (exitcode < 9)
			exitcode = 9;
		debug("sockcount=%d", sockcount);
		query->waiting_connect = 0;
		isc_event_free(&event);
		l = query->lookup;
		if ((l->current_query != NULL) &&
		    (ISC_LINK_LINKED(l->current_query, link)))
			next = ISC_LIST_NEXT(l->current_query, link);
		else
			next = NULL;
		clear_query(query);
		if (next != NULL) {
			bringup_timer(next, TCP_TIMEOUT);
			send_tcp_connect(next);
		} else
			check_next_lookup(l);
		return;
	}
	if (keep_open) {
		if (keep != NULL)
			isc_socket_detach(&keep);
		isc_socket_attach(query->sock, &keep);
		keepaddr = query->sockaddr;
	}
	launch_next_query(query, 1);
	isc_event_free(&event);
}

/*%
 * Check if the ongoing XFR needs more data before it's complete, using
 * the semantics of IXFR and AXFR protocols.  Much of the complexity of
 * this routine comes from determining when an IXFR is complete.
 * 0 means more data is on the way, and the recv has been issued.
 */
static int
check_for_more_data(dig_query_t *query, dns_message_t *msg,
		    isc_socketevent_t *sevent)
{
	dns_rdataset_t *rdataset = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_soa_t soa;
	uint32_t ixfr_serial = query->lookup->ixfr_serial, serial;
	isc_result_t result;
	int ixfr = query->lookup->rdtype == dns_rdatatype_ixfr;
	int axfr = query->lookup->rdtype == dns_rdatatype_axfr;

	if (ixfr)
		axfr = query->ixfr_axfr;

	debug("check_for_more_data()");

	/*
	 * By the time we're in this routine, we know we're doing
	 * either an AXFR or IXFR.  If there's no second_rr_type,
	 * then we don't yet know which kind of answer we got back
	 * from the server.  Here, we're going to walk through the
	 * rr's in the message, acting as necessary whenever we hit
	 * an SOA rr.
	 */

	query->msg_count++;
	query->byte_count += sevent->n;
	result = dns_message_firstname(msg, DNS_SECTION_ANSWER);
	if (result != ISC_R_SUCCESS) {
		puts("; Transfer failed.");
		return (1);
	}
	do {
		dns_name_t *name;
		name = NULL;
		dns_message_currentname(msg, DNS_SECTION_ANSWER,
					&name);
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			result = dns_rdataset_first(rdataset);
			if (result != ISC_R_SUCCESS)
				continue;
			do {
				query->rr_count++;
				dns_rdata_reset(&rdata);
				dns_rdataset_current(rdataset, &rdata);
				/*
				 * If this is the first rr, make sure
				 * it's an SOA
				 */
				if ((!query->first_soa_rcvd) &&
				    (rdata.type != dns_rdatatype_soa)) {
					puts("; Transfer failed.  "
					     "Didn't start with SOA answer.");
					return (1);
				}
				if ((!query->second_rr_rcvd) &&
				    (rdata.type != dns_rdatatype_soa)) {
					query->second_rr_rcvd = 1;
					query->second_rr_serial = 0;
					debug("got the second rr as nonsoa");
					axfr = query->ixfr_axfr = 1;
					goto next_rdata;
				}

				/*
				 * If the record is anything except an SOA
				 * now, just continue on...
				 */
				if (rdata.type != dns_rdatatype_soa)
					goto next_rdata;

				/* Now we have an SOA.  Work with it. */
				debug("got an SOA");
				result = dns_rdata_tostruct_soa(&rdata, &soa);
				check_result(result, "dns_rdata_tostruct_soa");
				serial = soa.serial;
				dns_rdata_freestruct_soa(&soa);
				if (!query->first_soa_rcvd) {
					query->first_soa_rcvd = 1;
					query->first_rr_serial = serial;
					debug("this is the first serial %u",
					      serial);
					if (ixfr && isc_serial_ge(ixfr_serial,
								  serial)) {
						debug("got up to date "
						      "response");
						goto doexit;
					}
					goto next_rdata;
				}
				if (axfr) {
					debug("doing axfr, got second SOA");
					goto doexit;
				}
				if (!query->second_rr_rcvd) {
					if (query->first_rr_serial == serial) {
						debug("doing ixfr, got "
						      "empty zone");
						goto doexit;
					}
					debug("this is the second serial %u",
					      serial);
					query->second_rr_rcvd = 1;
					query->second_rr_serial = serial;
					goto next_rdata;
				}
				/*
				 * If we get to this point, we're doing an
				 * IXFR and have to start really looking
				 * at serial numbers.
				 */
				if (query->first_rr_serial == serial) {
					debug("got a match for ixfr");
					if (!query->first_repeat_rcvd) {
						query->first_repeat_rcvd =
							1;
						goto next_rdata;
					}
					debug("done with ixfr");
					goto doexit;
				}
				debug("meaningless soa %u", serial);
			next_rdata:
				result = dns_rdataset_next(rdataset);
			} while (result == ISC_R_SUCCESS);
		}
		result = dns_message_nextname(msg, DNS_SECTION_ANSWER);
	} while (result == ISC_R_SUCCESS);
	launch_next_query(query, 0);
	return (0);
 doexit:
	dighost_received(sevent->n, &sevent->address, query);
	return (1);
}

static void
process_sit(dig_lookup_t *l, dns_message_t *msg,
	    isc_buffer_t *optbuf, size_t optlen)
{
	char bb[256];
	isc_buffer_t hexbuf;
	size_t len;
	const unsigned char *sit;
	int copysit;
	isc_result_t result;

	if (l->sitvalue != NULL) {
		isc_buffer_init(&hexbuf, bb, sizeof(bb));
		result = isc_hex_decodestring(l->sitvalue, &hexbuf);
		check_result(result, "isc_hex_decodestring");
		sit = isc_buffer_base(&hexbuf);
		len = isc_buffer_usedlength(&hexbuf);
		copysit = 0;
	} else {
		sit = cookie;
		len = sizeof(cookie);
		copysit = 1;
	}

	INSIST(msg->sitok == 0 && msg->sitbad == 0);
	if (optlen >= len && optlen >= 8U) {
		if (timingsafe_bcmp(isc_buffer_current(optbuf), sit, 8) == 0) {
			msg->sitok = 1;
		} else {
			printf(";; Warning: SIT client cookie mismatch\n");
			msg->sitbad = 1;
			copysit = 0;
		}
	} else {
		printf(";; Warning: SIT bad token (too short)\n");
		msg->sitbad = 1;
		copysit = 0;
	}
	if (copysit) {
		isc_region_t r;

		r.base = isc_buffer_current(optbuf);
		r.length = (unsigned int)optlen;
		isc_buffer_init(&hexbuf, sitvalue, sizeof(sitvalue));
		result = isc_hex_totext(&r, 2, "", &hexbuf);
		check_result(result, "isc_hex_totext");
		if (isc_buffer_availablelength(&hexbuf) > 0) {
			isc_buffer_putuint8(&hexbuf, 0);
			l->sitvalue = sitvalue;
		}
	}
	isc_buffer_forward(optbuf, (unsigned int)optlen);
}

static void
process_opt(dig_lookup_t *l, dns_message_t *msg) {
	dns_rdata_t rdata;
	isc_result_t result;
	isc_buffer_t optbuf;
	uint16_t optcode, optlen;
	dns_rdataset_t *opt = msg->opt;
	int seen_cookie = 0;

	result = dns_rdataset_first(opt);
	if (result == ISC_R_SUCCESS) {
		dns_rdata_init(&rdata);
		dns_rdataset_current(opt, &rdata);
		isc_buffer_init(&optbuf, rdata.data, rdata.length);
		isc_buffer_add(&optbuf, rdata.length);
		while (isc_buffer_remaininglength(&optbuf) >= 4) {
			optcode = isc_buffer_getuint16(&optbuf);
			optlen = isc_buffer_getuint16(&optbuf);
			switch (optcode) {
			case DNS_OPT_COOKIE:
				/*
				 * Only process the first cookie option.
				 */
				if (seen_cookie) {
					isc_buffer_forward(&optbuf, optlen);
					break;
				}
				process_sit(l, msg, &optbuf, optlen);
				seen_cookie = 1;
				break;
			default:
				isc_buffer_forward(&optbuf, optlen);
				break;
			}
		}
	}
}

static int
ednsvers(dns_rdataset_t *opt) {
	return ((opt->ttl >> 16) & 0xff);
}

/*%
 * Event handler for recv complete.  Perform whatever actions are necessary,
 * based on the specifics of the user's request.
 */
static void
recv_done(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent = NULL;
	dig_query_t *query = NULL;
	isc_buffer_t *b = NULL;
	dns_message_t *msg = NULL;
	isc_result_t result;
	dig_lookup_t *n, *l;
	int docancel = 0;
	int match = 1;
	unsigned int parseflags;
	dns_messageid_t id;
	unsigned int msgflags;
	int newedns;

	UNUSED(task);
	INSIST(!free_now);

	debug("recv_done()");

	recvcount--;
	debug("recvcount=%d", recvcount);
	INSIST(recvcount >= 0);

	query = event->ev_arg;
	clock_gettime(CLOCK_MONOTONIC, &query->time_recv);
	debug("lookup=%p, query=%p", query->lookup, query);

	l = query->lookup;

	REQUIRE(event->ev_type == ISC_SOCKEVENT_RECVDONE);
	sevent = (isc_socketevent_t *)event;

	b = ISC_LIST_HEAD(sevent->bufferlist);
	INSIST(b == &query->recvbuf);
	ISC_LIST_DEQUEUE(sevent->bufferlist, &query->recvbuf, link);

	if ((l->tcp_mode) && (query->timer != NULL))
		isc_timer_touch(query->timer);
	if ((!l->pending && !l->ns_search_only) || cancel_now) {
		debug("no longer pending.  Got %s",
			isc_result_totext(sevent->result));
		query->waiting_connect = 0;

		isc_event_free(&event);
		clear_query(query);
		check_next_lookup(l);
		return;
	}

	if (sevent->result != ISC_R_SUCCESS) {
		if (sevent->result == ISC_R_CANCELED) {
			debug("in recv cancel handler");
			query->waiting_connect = 0;
		} else {
			printf(";; communications error: %s\n",
			       isc_result_totext(sevent->result));
			if (keep != NULL)
				isc_socket_detach(&keep);
			isc_socket_detach(&query->sock);
			sockcount--;
			debug("sockcount=%d", sockcount);
			INSIST(sockcount >= 0);
		}
		if (sevent->result == ISC_R_EOF && l->eoferr == 0U) {
			n = requeue_lookup(l, 1);
			n->eoferr++;
		}
		isc_event_free(&event);
		clear_query(query);
		cancel_lookup(l);
		check_next_lookup(l);
		return;
	}

	if (!l->tcp_mode &&
	    !isc_sockaddr_compare(&sevent->address, &query->sockaddr,
				  ISC_SOCKADDR_CMPADDR|
				  ISC_SOCKADDR_CMPPORT|
				  ISC_SOCKADDR_CMPSCOPE|
				  ISC_SOCKADDR_CMPSCOPEZERO)) {
		char buf1[ISC_SOCKADDR_FORMATSIZE];
		char buf2[ISC_SOCKADDR_FORMATSIZE];
		struct sockaddr_storage any;

		if (isc_sockaddr_pf(&query->sockaddr) == AF_INET)
			isc_sockaddr_any(&any);
		else
			isc_sockaddr_any6(&any);

		/*
		* We don't expect a match when the packet is
		* sent to 0.0.0.0, :: or to a multicast addresses.
		* XXXMPA broadcast needs to be handled here as well.
		*/
		if ((!isc_sockaddr_eqaddr(&query->sockaddr, &any) &&
		     !isc_sockaddr_ismulticast(&query->sockaddr)) ||
		    isc_sockaddr_getport(&query->sockaddr) !=
		    isc_sockaddr_getport(&sevent->address)) {
			isc_sockaddr_format(&sevent->address, buf1,
			sizeof(buf1));
			isc_sockaddr_format(&query->sockaddr, buf2,
			sizeof(buf2));
			printf(";; reply from unexpected source: %s,"
			" expected %s\n", buf1, buf2);
			match = 0;
		}
	}

	result = dns_message_peekheader(b, &id, &msgflags);
	if (result != ISC_R_SUCCESS || l->sendmsg->id != id) {
		match = 0;
		if (l->tcp_mode) {
			int fail = 1;
			if (result == ISC_R_SUCCESS) {
				if (!query->first_soa_rcvd ||
				     query->warn_id)
					printf(";; %s: ID mismatch: "
					       "expected ID %u, got %u\n",
					       query->first_soa_rcvd ?
					       "WARNING" : "ERROR",
					       l->sendmsg->id, id);
				if (query->first_soa_rcvd)
					fail = 0;
				query->warn_id = 0;
			} else
				printf(";; ERROR: short "
				       "(< header size) message\n");
			if (fail) {
				isc_event_free(&event);
				clear_query(query);
				cancel_lookup(l);
				check_next_lookup(l);
				return;
			}
			match = 1;
		} else if (result == ISC_R_SUCCESS)
			printf(";; Warning: ID mismatch: "
			       "expected ID %u, got %u\n", l->sendmsg->id, id);
		else
			printf(";; Warning: short "
			       "(< header size) message received\n");
	}

	if (result == ISC_R_SUCCESS && (msgflags & DNS_MESSAGEFLAG_QR) == 0)
		printf(";; Warning: query response not set\n");

	if (!match)
		goto udp_mismatch;

	result = dns_message_create(DNS_MESSAGE_INTENTPARSE, &msg);
	check_result(result, "dns_message_create");

	if (tsigkey != NULL) {
		if (l->querysig == NULL) {
			debug("getting initial querysig");
			result = dns_message_getquerytsig(l->sendmsg,
							  &l->querysig);
			check_result(result, "dns_message_getquerytsig");
		}
		result = dns_message_setquerytsig(msg, l->querysig);
		check_result(result, "dns_message_setquerytsig");
		result = dns_message_settsigkey(msg, tsigkey);
		check_result(result, "dns_message_settsigkey");
		msg->tsigctx = l->tsigctx;
		l->tsigctx = NULL;
		if (l->msgcounter != 0)
			msg->tcp_continuation = 1;
		l->msgcounter++;
	}

	debug("before parse starts");
	parseflags = 0;
	if (l->besteffort) {
		parseflags |= DNS_MESSAGEPARSE_BESTEFFORT;
		parseflags |= DNS_MESSAGEPARSE_IGNORETRUNCATION;
	}
	result = dns_message_parse(msg, b, parseflags);
	if (result == DNS_R_RECOVERABLE) {
		printf(";; Warning: Message parser reports malformed "
		       "message packet.\n");
		result = ISC_R_SUCCESS;
	}
	if (result != ISC_R_SUCCESS) {
		printf(";; Got bad packet: %s\n", isc_result_totext(result));
		hex_dump(b);
		query->waiting_connect = 0;
		dns_message_destroy(&msg);
		isc_event_free(&event);
		clear_query(query);
		cancel_lookup(l);
		check_next_lookup(l);
		return;
	}
	if (msg->counts[DNS_SECTION_QUESTION] != 0) {
		match = 1;
		for (result = dns_message_firstname(msg, DNS_SECTION_QUESTION);
		     result == ISC_R_SUCCESS && match;
		     result = dns_message_nextname(msg, DNS_SECTION_QUESTION)) {
			dns_name_t *name = NULL;
			dns_rdataset_t *rdataset;

			dns_message_currentname(msg, DNS_SECTION_QUESTION,
						&name);
			for (rdataset = ISC_LIST_HEAD(name->list);
			     rdataset != NULL;
			     rdataset = ISC_LIST_NEXT(rdataset, link)) {
				if (l->rdtype != rdataset->type ||
				    l->rdclass != rdataset->rdclass ||
				    !dns_name_equal(l->name, name)) {
					char namestr[DNS_NAME_FORMATSIZE];
					char typebuf[DNS_RDATATYPE_FORMATSIZE];
					char classbuf[DNS_RDATACLASS_FORMATSIZE];
					dns_name_format(name, namestr,
							sizeof(namestr));
					dns_rdatatype_format(rdataset->type,
							     typebuf,
							     sizeof(typebuf));
					dns_rdataclass_format(rdataset->rdclass,
							      classbuf,
							      sizeof(classbuf));
					printf(";; Question section mismatch: "
					       "got %s/%s/%s\n",
					       namestr, typebuf, classbuf);
					match = 0;
				}
			}
		}
		if (!match) {
			dns_message_destroy(&msg);
			if (l->tcp_mode) {
				isc_event_free(&event);
				clear_query(query);
				cancel_lookup(l);
				check_next_lookup(l);
				return;
			} else
				goto udp_mismatch;
		}
	}
	if (msg->rcode == dns_rcode_badvers && msg->opt != NULL &&
	    (newedns = ednsvers(msg->opt)) < l->edns && l->ednsneg) {
		/*
		 * Add minimum EDNS version required checks here if needed.
		 */
		if (l->comments)
			printf(";; BADVERS, retrying with EDNS version %u.\n",
			       (unsigned int)newedns);
		l->edns = newedns;
		n = requeue_lookup(l, 1);
		if (l->trace && l->trace_root)
			n->rdtype = l->qrdtype;
		dns_message_destroy(&msg);
		isc_event_free(&event);
		clear_query(query);
		cancel_lookup(l);
		check_next_lookup(l);
		return;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0 &&
	    !l->ignore && !l->tcp_mode) {
		if (l->sitvalue == NULL && l->sit && msg->opt != NULL)
			process_opt(l, msg);
		if (l->comments)
			printf(";; Truncated, retrying in TCP mode.\n");
		n = requeue_lookup(l, 1);
		n->tcp_mode = 1;
		if (l->trace && l->trace_root)
			n->rdtype = l->qrdtype;
		dns_message_destroy(&msg);
		isc_event_free(&event);
		clear_query(query);
		cancel_lookup(l);
		check_next_lookup(l);
		return;
	}
	if ((msg->rcode == dns_rcode_servfail && !l->servfail_stops) ||
	    (check_ra && (msg->flags & DNS_MESSAGEFLAG_RA) == 0 && l->recurse))
	{
		dig_query_t *next = ISC_LIST_NEXT(query, link);
		if (l->current_query == query)
			l->current_query = NULL;
		if (next != NULL) {
			debug("sending query %p\n", next);
			if (l->tcp_mode)
				send_tcp_connect(next);
			else
				send_udp(next);
		}
		/*
		 * If our query is at the head of the list and there
		 * is no next, we're the only one left, so fall
		 * through to print the message.
		 */
		if ((ISC_LIST_HEAD(l->q) != query) ||
		    (ISC_LIST_NEXT(query, link) != NULL)) {
			if (l->comments)
				printf(";; Got %s from %s, "
				       "trying next server\n",
				       msg->rcode == dns_rcode_servfail ?
				       "SERVFAIL reply" :
				       "recursion not available",
				       query->servname);
			clear_query(query);
			check_next_lookup(l);
			dns_message_destroy(&msg);
			isc_event_free(&event);
			return;
		}
	}

	if (tsigkey != NULL) {
		result = dns_tsig_verify(&query->recvbuf, msg);
		if (result != ISC_R_SUCCESS) {
			printf(";; Couldn't verify signature: %s\n",
			       isc_result_totext(result));
			validated = 0;
		}
		l->tsigctx = msg->tsigctx;
		msg->tsigctx = NULL;
		if (l->querysig != NULL) {
			debug("freeing querysig buffer %p", l->querysig);
			isc_buffer_free(&l->querysig);
		}
		result = dns_message_getquerytsig(msg, &l->querysig);
		check_result(result,"dns_message_getquerytsig");
	}

	extrabytes = isc_buffer_remaininglength(b);

	debug("after parse");
	if (l->doing_xfr && l->xfr_q == NULL) {
		l->xfr_q = query;
		/*
		 * Once we are in the XFR message, increase
		 * the timeout to much longer, so brief network
		 * outages won't cause the XFR to abort
		 */
		if (timeout != INT_MAX && query->timer != NULL) {
			unsigned int local_timeout;

			if (timeout == 0) {
				if (l->tcp_mode)
					local_timeout = TCP_TIMEOUT * 4;
				else
					local_timeout = UDP_TIMEOUT * 4;
			} else {
				if (timeout < (INT_MAX / 4))
					local_timeout = timeout * 4;
				else
					local_timeout = INT_MAX;
			}
			debug("have local timeout of %d", local_timeout);
			l->interval.tv_sec = local_timeout;
			l->interval.tv_nsec = 0;
			result = isc_timer_reset(query->timer,
						 &l->interval,
						 0);
			check_result(result, "isc_timer_reset");
		}
	}

	if (l->sitvalue != NULL) {
		if (msg->opt == NULL)
			printf(";; expected opt record in response\n");
		else
			process_opt(l, msg);
	} else if (l->sit && msg->opt != NULL)
		process_opt(l, msg);

	if (!l->doing_xfr || l->xfr_q == query) {
		if (msg->rcode == dns_rcode_nxdomain &&
		    (l->origin != NULL || l->need_search)) {
			if (!next_origin(query->lookup) || showsearch) {
				dighost_printmessage(query, msg, 1);
				dighost_received(b->used, &sevent->address, query);
			}
		} else if (!l->trace && !l->ns_search_only) {
				dighost_printmessage(query, msg, 1);
		} else if (l->trace) {
			int nl = 0;
			int count = msg->counts[DNS_SECTION_ANSWER];

			debug("in TRACE code");
			if (!l->ns_search_only)
				dighost_printmessage(query, msg, 1);

			l->rdtype = l->qrdtype;
			if (l->trace_root || (l->ns_search_only && count > 0)) {
				if (!l->trace_root)
					l->rdtype = dns_rdatatype_soa;
				nl = followup_lookup(msg, query,
						     DNS_SECTION_ANSWER);
				l->trace_root = 0;
			} else if (count == 0)
				nl = followup_lookup(msg, query,
						     DNS_SECTION_AUTHORITY);
			if (nl == 0)
				docancel = 1;
		} else {
			debug("in NSSEARCH code");

			if (l->trace_root) {
				/*
				 * This is the initial NS query.
				 */
				int nl;

				l->rdtype = dns_rdatatype_soa;
				nl = followup_lookup(msg, query,
						     DNS_SECTION_ANSWER);
				if (nl == 0)
					docancel = 1;
				l->trace_root = 0;
				usesearch = 0;
			} else
				dighost_printmessage(query, msg, 1);
		}
	}

	if (l->pending)
		debug("still pending.");
	if (l->doing_xfr) {
		if (query != l->xfr_q) {
			dns_message_destroy(&msg);
			isc_event_free(&event);
			query->waiting_connect = 0;
			return;
		}
		if (!docancel)
			docancel = check_for_more_data(query, msg, sevent);
		if (docancel) {
			dns_message_destroy(&msg);
			clear_query(query);
			cancel_lookup(l);
			check_next_lookup(l);
		}
	} else {

		if (msg->rcode == dns_rcode_noerror || l->origin == NULL) {

				dighost_received(b->used, &sevent->address, query);
		}

		if (!query->lookup->ns_search_only)
			query->lookup->pending = 0;
		if (!query->lookup->ns_search_only ||
		    query->lookup->trace_root || docancel) {
				dns_message_destroy(&msg);

			cancel_lookup(l);
		}
		clear_query(query);
		check_next_lookup(l);
	}
	if (msg != NULL) {
			dns_message_destroy(&msg);
	}
	isc_event_free(&event);
	return;

 udp_mismatch:
	isc_buffer_invalidate(&query->recvbuf);
	isc_buffer_init(&query->recvbuf, query->recvspace, COMMSIZE);
	ISC_LIST_ENQUEUE(query->recvlist, &query->recvbuf, link);
	result = isc_socket_recvv(query->sock, &query->recvlist, 1,
				  global_task, recv_done, query);
	check_result(result, "isc_socket_recvv");
	recvcount++;
	isc_event_free(&event);
	return;
}

/*%
 * Turn a name into an address, using system-supplied routines.  This is
 * used in looking up server names, etc... and needs to use system-supplied
 * routines, since they may be using a non-DNS system for these lookups.
 */
isc_result_t
get_address(char *host, in_port_t myport, struct sockaddr_storage *sockaddr) {
	int count;
	isc_result_t result;

	result = get_addresses(host, myport, sockaddr, 1, &count);
	if (result != ISC_R_SUCCESS)
		return (result);

	INSIST(count == 1);

	return (ISC_R_SUCCESS);
}

int
getaddresses(dig_lookup_t *lookup, const char *host, isc_result_t *resultp) {
	isc_result_t result;
	struct sockaddr_storage sockaddrs[DIG_MAX_ADDRESSES];
	int count, i;
	dig_server_t *srv;
	char tmp[NI_MAXHOST];

	result = get_addresses(host, 0, sockaddrs,
				    DIG_MAX_ADDRESSES, &count);
	if (resultp != NULL)
		*resultp = result;
	if (result != ISC_R_SUCCESS) {
		if (resultp == NULL)
			fatal("couldn't get address for '%s': %s",
			      host, isc_result_totext(result));
		return (0);
	}

	for (i = 0; i < count; i++) {
		int error;
		error = getnameinfo((struct sockaddr *)&sockaddrs[i],
		    sockaddrs[i].ss_len, tmp, sizeof(tmp), NULL, 0,
		    NI_NUMERICHOST | NI_NUMERICSERV);
		if (error)
			fatal("%s", gai_strerror(error));

		srv = make_server(tmp, host);
		ISC_LIST_APPEND(lookup->my_server_list, srv, link);
	}

	return (count);
}

/*%
 * Initiate either a TCP or UDP lookup
 */
void
do_lookup(dig_lookup_t *lookup) {
	dig_query_t *query;

	REQUIRE(lookup != NULL);

	debug("do_lookup()");
	lookup->pending = 1;
	query = ISC_LIST_HEAD(lookup->q);
	if (query != NULL) {
		if (lookup->tcp_mode)
			send_tcp_connect(query);
		else
			send_udp(query);
	}
}

/*%
 * Start everything in action upon task startup.
 */
void
onrun_callback(isc_task_t *task, isc_event_t *event) {
	UNUSED(task);

	isc_event_free(&event);
	start_lookup();
}

/*%
 * Make everything on the lookup queue go away.  Mainly used by the
 * SIGINT handler.
 */
void
cancel_all(void) {
	dig_lookup_t *l, *n;
	dig_query_t *q, *nq;

	debug("cancel_all()");

	if (free_now) {
		return;
	}
	cancel_now = 1;
	if (current_lookup != NULL) {
		for (q = ISC_LIST_HEAD(current_lookup->q);
		     q != NULL;
		     q = nq)
		{
			nq = ISC_LIST_NEXT(q, link);
			debug("canceling pending query %p, belonging to %p",
			      q, current_lookup);
			if (q->sock != NULL)
				isc_socket_cancel(q->sock, NULL,
						  ISC_SOCKCANCEL_ALL);
			else
				clear_query(q);
		}
		for (q = ISC_LIST_HEAD(current_lookup->connecting);
		     q != NULL;
		     q = nq)
		{
			nq = ISC_LIST_NEXT(q, clink);
			debug("canceling connecting query %p, belonging to %p",
			      q, current_lookup);
			if (q->sock != NULL)
				isc_socket_cancel(q->sock, NULL,
						  ISC_SOCKCANCEL_ALL);
			else
				clear_query(q);
		}
	}
	l = ISC_LIST_HEAD(lookup_list);
	while (l != NULL) {
		n = ISC_LIST_NEXT(l, link);
		ISC_LIST_DEQUEUE(lookup_list, l, link);
		try_clear_lookup(l);
		l = n;
	}
}

/*%
 * Destroy all of the libs we are using, and get everything ready for a
 * clean shutdown.
 */
void
destroy_libs(void) {

	if (keep != NULL)
		isc_socket_detach(&keep);
	debug("destroy_libs()");
	if (global_task != NULL) {
		debug("freeing task");
		isc_task_detach(&global_task);
	}
	/*
	 * The taskmgr_destroy() call blocks until all events are cleared
	 * from the task.
	 */
	if (taskmgr != NULL) {
		debug("freeing taskmgr");
		isc_taskmgr_destroy(&taskmgr);
	}
	REQUIRE(sockcount == 0);
	REQUIRE(recvcount == 0);
	REQUIRE(sendcount == 0);

	INSIST(ISC_LIST_HEAD(lookup_list) == NULL);
	INSIST(current_lookup == NULL);
	INSIST(!free_now);

	free_now = 1;

	lwres_conf_clear(lwconf);

	flush_server_list();

	clear_searchlist();

	if (socketmgr != NULL) {
		debug("freeing socketmgr");
		isc_socketmgr_destroy(&socketmgr);
	}
	if (timermgr != NULL) {
		debug("freeing timermgr");
		isc_timermgr_destroy(&timermgr);
	}
	if (tsigkey != NULL) {
		debug("freeing key %p", tsigkey);
		dns_tsigkey_detach(&tsigkey);
	}
	if (namebuf != NULL)
		isc_buffer_free(&namebuf);

	if (is_dst_up) {
		debug("destroy DST lib");
		dst_lib_destroy();
		is_dst_up = 0;
	}

	debug("Removing log context");
	isc_log_destroy(&lctx);

}

int64_t
uelapsed(const struct timespec *t1, const struct timespec *t2)
{
	struct timespec	 diff, zero = {0, 0};
	struct timeval	 tv;

	timespecsub(t1, t2, &diff);

	if (timespeccmp(&diff, &zero, <=))
		return 0;

	TIMESPEC_TO_TIMEVAL(&tv, &diff);

	return (tv.tv_sec * 1000000 + tv.tv_usec);
}
