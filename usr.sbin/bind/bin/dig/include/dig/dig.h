/*
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: dig.h,v 1.71.2.3 2001/11/15 01:24:13 marka Exp $ */

#ifndef DIG_H
#define DIG_H

#include <dns/rdatalist.h>

#include <dst/dst.h>

#include <isc/boolean.h>
#include <isc/buffer.h>
#include <isc/bufferlist.h>
#include <isc/formatcheck.h>
#include <isc/lang.h>
#include <isc/list.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>

#define MXSERV 6
#define MXNAME (DNS_NAME_MAXTEXT+1)
#define MXRD 32
#define BUFSIZE 512
#define COMMSIZE 0xffff
#ifndef RESOLV_CONF
#define RESOLV_CONF "/etc/resolv.conf"
#endif
#define OUTPUTBUF 32767
#define MAXRRLIMIT 0xffffffff
#define MAXTIMEOUT 0xffff
#define MAXTRIES 0xffffffff
#define MAXNDOTS 0xffff
#define MAXPORT 0xffff
#define MAXSERIAL 0xffffffff

/*
 * Default timeout values
 */
#define TCP_TIMEOUT 10
#define UDP_TIMEOUT 5

#define SERVER_TIMEOUT 1

#define LOOKUP_LIMIT 64
/*
 * Lookup_limit is just a limiter, keeping too many lookups from being
 * created.  It's job is mainly to prevent the program from running away
 * in a tight loop of constant lookups.  It's value is arbitrary.
 */

#define ROOTNS 1
/*
 * Set the number of root servers to ask for information when running in
 * trace mode.
 * XXXMWS -- trace mode is currently semi-broken, and this number *MUST*
 * be 1.
 */

ISC_LANG_BEGINDECLS

typedef struct dig_lookup dig_lookup_t;
typedef struct dig_query dig_query_t;
typedef struct dig_server dig_server_t;
typedef ISC_LIST(dig_server_t) dig_serverlist_t;
typedef struct dig_searchlist dig_searchlist_t;

struct dig_lookup {
	isc_boolean_t
	        pending, /* Pending a successful answer */
		waiting_connect,
		doing_xfr,
		ns_search_only, /* dig +nssearch, host -C */
		identify, /* Append an "on server <foo>" message */
		identify_previous_line, /* Prepend a "Nameserver <foo>:"
					   message, with newline and tab */
		ignore,
		recurse,
		aaonly,
		adflag,
		cdflag,
		trace, /* dig +trace */
		trace_root, /* initial query for either +trace or +nssearch */
		tcp_mode,
		nibble,
		comments,
		stats,
		section_question,
		section_answer,
		section_authority,
		section_additional,
		servfail_stops,
		new_search,
		besteffort,
		dnssec;
	char textname[MXNAME]; /* Name we're going to be looking up */
	char cmdline[MXNAME];
	dns_rdatatype_t rdtype;
	dns_rdatatype_t qrdtype;
	dns_rdataclass_t rdclass;
	isc_boolean_t rdtypeset;
	isc_boolean_t rdclassset;
	char namespace[BUFSIZE];
	char onamespace[BUFSIZE];
	isc_buffer_t namebuf;
	isc_buffer_t onamebuf;
	isc_buffer_t sendbuf;
	char *sendspace;
	dns_name_t *name;
	isc_timer_t *timer;
	isc_interval_t interval;
	dns_message_t *sendmsg;
	dns_name_t *oname;
	ISC_LINK(dig_lookup_t) link;
	ISC_LIST(dig_query_t) q;
	dig_query_t *current_query;
	dig_serverlist_t my_server_list;
	dig_searchlist_t *origin;
	dig_query_t *xfr_q;
	isc_uint32_t retries;
	int nsfound;
	isc_uint16_t udpsize;
	isc_uint32_t ixfr_serial;
	isc_buffer_t rdatabuf;
	char rdatastore[MXNAME];
	dst_context_t *tsigctx;
	isc_buffer_t *querysig;
	isc_uint32_t msgcounter;
};

struct dig_query {
	dig_lookup_t *lookup;
	isc_boolean_t waiting_connect,
		first_pass,
		first_soa_rcvd,
		second_rr_rcvd,
		first_repeat_rcvd,
		recv_made;
	isc_uint32_t first_rr_serial;
	isc_uint32_t second_rr_serial;
	isc_uint32_t rr_count;
	char *servname;
	isc_bufferlist_t sendlist,
		recvlist,
		lengthlist;
	isc_buffer_t recvbuf,
		lengthbuf,
		slbuf;
	char *recvspace,
		lengthspace[4],
		slspace[4];
	isc_socket_t *sock;
	ISC_LINK(dig_query_t) link;
	isc_sockaddr_t sockaddr;
	isc_time_t time_sent;
};

struct dig_server {
	char servername[MXNAME];
	ISC_LINK(dig_server_t) link;
};

struct dig_searchlist {
	char origin[MXNAME];
	ISC_LINK(dig_searchlist_t) link;
};

/*
 * Routines in dighost.c.
 */
void
get_address(char *host, in_port_t port, isc_sockaddr_t *sockaddr);

isc_result_t
get_reverse(char *reverse, char *value, isc_boolean_t nibble);

void
fatal(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

void
debug(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

void
check_result(isc_result_t result, const char *msg);

void
setup_lookup(dig_lookup_t *lookup);

void
do_lookup(dig_lookup_t *lookup);

void
start_lookup(void);

void
onrun_callback(isc_task_t *task, isc_event_t *event);

int
dhmain(int argc, char **argv);

void
setup_libs(void);

void
setup_system(void);

dig_lookup_t *
requeue_lookup(dig_lookup_t *lookold, isc_boolean_t servers);

dig_lookup_t *
make_empty_lookup(void);

dig_lookup_t *
clone_lookup(dig_lookup_t *lookold, isc_boolean_t servers);

dig_server_t *
make_server(const char *servname);

void
clone_server_list(dig_serverlist_t src,
		  dig_serverlist_t *dest);

void
cancel_all(void);

void
destroy_libs(void);

void
set_search_domain(char *domain);

/*
 * Routines to be defined in dig.c, host.c, and nslookup.c.
 */

isc_result_t
printmessage(dig_query_t *query, dns_message_t *msg, isc_boolean_t headers);
/*
 * Print the final result of the lookup.
 */

void
received(int bytes, isc_sockaddr_t *from, dig_query_t *query);
/*
 * Print a message about where and when the response
 * was received from, like the final comment in the
 * output of "dig".
 */

void
trying(char *frm, dig_lookup_t *lookup);

void
dighost_shutdown(void);

char *
next_token(char **stringp, const char *delim);

ISC_LANG_ENDDECLS

#endif
