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

#ifndef DIG_H
#define DIG_H

/*! \file */

#include <time.h>

#include <dst/dst.h>

#include <isc/buffer.h>
#include <isc/list.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>

#define MXSERV 20
#define MXNAME (DNS_NAME_MAXTEXT+1)
#define MXRD 32
/*% Buffer Size */
#define BUFSIZE 512
#define COMMSIZE 0xffff
/*% output buffer */
#define OUTPUTBUF 32767
/*% Max RR Limit */
#define MAXRRLIMIT 0xffffffff
#define MAXTIMEOUT 0xffff
/*% Max number of tries */
#define MAXTRIES 0xffffffff
/*% Max number of dots */
#define MAXNDOTS 0xffff
/*% Max number of ports */
#define MAXPORT 0xffff
/*% Max serial number */
#define MAXSERIAL 0xffffffff

/*% Default TCP Timeout */
#define TCP_TIMEOUT 10
/*% Default UDP Timeout */
#define UDP_TIMEOUT 5

#define SERVER_TIMEOUT 1

#define LOOKUP_LIMIT 64
/*%
 * Lookup_limit is just a limiter, keeping too many lookups from being
 * created.  It's job is mainly to prevent the program from running away
 * in a tight loop of constant lookups.  It's value is arbitrary.
 */

/*
 * Defaults for the sigchase suboptions.  Consolidated here because
 * these control the layout of dig_lookup_t (among other things).
 */

typedef struct dig_lookup dig_lookup_t;
typedef struct dig_query dig_query_t;
typedef struct dig_server dig_server_t;
typedef ISC_LIST(dig_server_t) dig_serverlist_t;
typedef struct dig_searchlist dig_searchlist_t;

/*% The dig_lookup structure */
struct dig_lookup {
	int
		pending, /*%< Pending a successful answer */
		waiting_connect,
		doing_xfr,
		ns_search_only, /*%< dig +nssearch, host -C */
		identify, /*%< Append an "on server <foo>" message */
		identify_previous_line, /*% Prepend a "Nameserver <foo>:"
					   message, with newline and tab */
		ignore,
		recurse,
		aaonly,
		adflag,
		cdflag,
		trace, /*% dig +trace */
		trace_root, /*% initial query for either +trace or +nssearch */
		tcp_mode,
		tcp_mode_set,
		ip6_int,
		comments,
		stats,
		section_question,
		section_answer,
		section_authority,
		section_additional,
		servfail_stops,
		new_search,
		need_search,
		done_as_is,
		besteffort,
		dnssec,
		expire,
		sit,
		nsid,   /*% Name Server ID (RFC 5001) */
		ednsneg,
		mapped,
		idnout;

	char textname[MXNAME]; /*% Name we're going to be looking up */
	char cmdline[MXNAME];
	dns_rdatatype_t rdtype;
	dns_rdatatype_t qrdtype;
	dns_rdataclass_t rdclass;
	int rdtypeset;
	int rdclassset;
	char name_space[BUFSIZE];
	char oname_space[BUFSIZE];
	isc_buffer_t namebuf;
	isc_buffer_t onamebuf;
	isc_buffer_t renderbuf;
	char *sendspace;
	dns_name_t *name;
	struct timespec interval;
	dns_message_t *sendmsg;
	dns_name_t *oname;
	ISC_LINK(dig_lookup_t) link;
	ISC_LIST(dig_query_t) q;
	ISC_LIST(dig_query_t) connecting;
	dig_query_t *current_query;
	dig_serverlist_t my_server_list;
	dig_searchlist_t *origin;
	dig_query_t *xfr_q;
	uint32_t retries;
	int nsfound;
	uint16_t udpsize;
	int16_t edns;
	uint32_t ixfr_serial;
	isc_buffer_t rdatabuf;
	char rdatastore[MXNAME];
	dst_context_t *tsigctx;
	isc_buffer_t *querysig;
	uint32_t msgcounter;
	dns_fixedname_t fdomain;
	struct sockaddr_storage *ecs_addr;
	int ecs_plen;
	char *sitvalue;
	dns_ednsopt_t *ednsopts;
	unsigned int ednsoptscnt;
	unsigned int ednsflags;
	dns_opcode_t opcode;
	unsigned int eoferr;
};

/*% The dig_query structure */
struct dig_query {
	dig_lookup_t *lookup;
	int waiting_connect,
		pending_free,
		waiting_senddone,
		first_pass,
		first_soa_rcvd,
		second_rr_rcvd,
		first_repeat_rcvd,
		recv_made,
		warn_id,
		timedout;
	uint32_t first_rr_serial;
	uint32_t second_rr_serial;
	uint32_t msg_count;
	uint32_t rr_count;
	int ixfr_axfr;
	char *servname;
	char *userarg;
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
	ISC_LINK(dig_query_t) clink;
	struct sockaddr_storage sockaddr;
	struct timespec time_sent;
	struct timespec time_recv;
	uint64_t byte_count;
	isc_buffer_t sendbuf;
	isc_timer_t *timer;
};

struct dig_server {
	char servername[MXNAME];
	char userarg[MXNAME];
	ISC_LINK(dig_server_t) link;
};

struct dig_searchlist {
	char origin[MXNAME];
	ISC_LINK(dig_searchlist_t) link;
};

typedef ISC_LIST(dig_searchlist_t) dig_searchlistlist_t;
typedef ISC_LIST(dig_lookup_t) dig_lookuplist_t;

/*
 * Externals from dighost.c
 */

extern dig_lookuplist_t lookup_list;
extern dig_serverlist_t server_list;
extern dig_serverlist_t root_hints_server_list;
extern dig_searchlistlist_t search_list;
extern unsigned int extrabytes;

extern int check_ra, have_ipv4, have_ipv6, specified_source,
	usesearch, showsearch, qr;
extern in_port_t port;
extern unsigned int timeout;
extern int sendcount;
extern int ndots;
extern int lookup_counter;
extern int exitcode;
extern struct sockaddr_storage bind_address;
extern char keynametext[MXNAME];
extern char keyfile[MXNAME];
extern char keysecret[MXNAME];
extern dns_name_t *hmacname;
extern unsigned int digestbits;
extern dns_tsigkey_t *tsigkey;
extern int validated;
extern isc_taskmgr_t *taskmgr;
extern isc_task_t *global_task;
extern int free_now;
extern int debugging, debugtiming;
extern int keep_open;

extern char *progname;
extern int tries;
extern int fatalexit;

int host_main(int, char **);
int nslookup_main(int, char **);

/*
 * Routines in dighost.c.
 */
isc_result_t
get_address(char *host, in_port_t port, struct sockaddr_storage *sockaddr);

int
getaddresses(dig_lookup_t *lookup, const char *host, isc_result_t *resultp);

isc_result_t
get_reverse(char *reverse, size_t len, char *value, int ip6_int,
	    int strict);

__dead void
fatal(const char *format, ...)
__attribute__((__format__(__printf__, 1, 2)));

void
debug(const char *format, ...) __attribute__((__format__(__printf__, 1, 2)));

void
check_result(isc_result_t result, const char *msg);

int
setup_lookup(dig_lookup_t *lookup);

void
destroy_lookup(dig_lookup_t *lookup);

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
setup_system(int ipv4only, int ipv6only);

isc_result_t
parse_netprefix(struct sockaddr_storage **sap, int *plen, const char *value);

void
parse_hmac(const char *hmacstr);

dig_lookup_t *
requeue_lookup(dig_lookup_t *lookold, int servers);

dig_lookup_t *
make_empty_lookup(void);

dig_lookup_t *
clone_lookup(dig_lookup_t *lookold, int servers);

dig_server_t *
make_server(const char *servname, const char *userarg);

void
flush_server_list(void);

isc_result_t
set_nameserver(char *opt);

void
clone_server_list(dig_serverlist_t src,
		  dig_serverlist_t *dest);

void
cancel_all(void);

void
destroy_libs(void);

void
set_search_domain(char *domain);

char *
next_token(char **stringp, const char *delim);

int64_t
uelapsed(const struct timespec *t1, const struct timespec *t2);

/*
 * Routines to be defined in dig.c, host.c, and nslookup.c. and
 * then assigned to the appropriate function pointer
 */

extern isc_result_t
(*dighost_printmessage)(dig_query_t *query, dns_message_t *msg, int headers);
/*%<
 * Print the final result of the lookup.
 */

extern void
(*dighost_received)(unsigned int bytes, struct sockaddr_storage *from, dig_query_t *query);
/*%<
 * Print a message about where and when the response
 * was received from, like the final comment in the
 * output of "dig".
 */

extern void
(*dighost_trying)(char *frm, dig_lookup_t *lookup);

extern void
(*dighost_shutdown)(void);

void save_opt(dig_lookup_t *lookup, char *code, char *value);

void setup_file_key(void);
void setup_text_key(void);

/*
 * Routines exported from dig.c for use by dig for iOS
 */

/*%<
 * Call once only to set up libraries, parse global
 * parameters and initial command line query parameters
 */
void
dig_setup(int argc, char **argv);

/*%<
 * Call to supply new parameters for the next lookup
 */
void
dig_query_setup(int, int, int argc, char **argv);

/*%<
 * set the main application event cycle running
 */
void
dig_startup(void);

/*%<
 * Cleans up the application
 */
void
dig_shutdown(void);

#endif
