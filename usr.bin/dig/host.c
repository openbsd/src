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

/*! \file */
#include <sys/types.h>

#include <err.h>
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

#include <isc/app.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>

#include "dig.h"

static int short_form = 1, listed_server = 0;
static int default_lookups = 1;
static int seen_error = -1;
static int list_addresses = 1;
static dns_rdatatype_t list_type = dns_rdatatype_a;
static int printed_server = 0;
static int ipv4only = 0, ipv6only = 0;

static const char *opcodetext[] = {
	"QUERY",
	"IQUERY",
	"STATUS",
	"RESERVED3",
	"NOTIFY",
	"UPDATE",
	"RESERVED6",
	"RESERVED7",
	"RESERVED8",
	"RESERVED9",
	"RESERVED10",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15"
};

static const char *rcodetext[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"YXDOMAIN",
	"YXRRSET",
	"NXRRSET",
	"NOTAUTH",
	"NOTZONE",
	"RESERVED11",
	"RESERVED12",
	"RESERVED13",
	"RESERVED14",
	"RESERVED15",
	"BADVERS"
};

struct rtype {
	unsigned int type;
	const char *text;
};

static struct rtype rtypes[] = {
	{ 1, 	"has address" },
	{ 2, 	"name server" },
	{ 5, 	"is an alias for" },
	{ 11,	"has well known services" },
	{ 12,	"domain name pointer" },
	{ 13,	"host information" },
	{ 15,	"mail is handled by" },
	{ 16,	"descriptive text" },
	{ 19,	"x25 address" },
	{ 20,	"ISDN address" },
	{ 24,	"has signature" },
	{ 25,	"has key" },
	{ 28,	"has IPv6 address" },
	{ 29,	"location" },
	{ 0, NULL }
};

static char *
rcode_totext(dns_rcode_t rcode)
{
	static char buf[sizeof("?65535")];
	union {
		const char *consttext;
		char *deconsttext;
	} totext;

	if (rcode >= (sizeof(rcodetext)/sizeof(rcodetext[0]))) {
		snprintf(buf, sizeof(buf), "?%u", rcode);
		totext.deconsttext = buf;
	} else
		totext.consttext = rcodetext[rcode];
	return totext.deconsttext;
}

static __dead void
show_usage(void);

static void
show_usage(void) {
	fputs(
"usage: host [-46aCdilrsTVvw] [-c class] [-m flag] [-N ndots] [-R number]\n"
"            [-t type] [-W wait] name [server]\n", stderr);
	exit(1);
}

static void
host_shutdown(void) {
	(void) isc_app_shutdown();
}

static void
received(unsigned int bytes, struct sockaddr_storage *from, dig_query_t *query) {
	struct timespec now;

	if (!short_form) {
		char fromtext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(from, fromtext, sizeof(fromtext));
		clock_gettime(CLOCK_MONOTONIC, &now);
		printf("Received %u bytes from %s in %lld ms\n",
		    bytes, fromtext, uelapsed(&now, &query->time_sent)/1000);
	}
}

static void
trying(char *frm, dig_lookup_t *lookup) {
	UNUSED(lookup);

	if (!short_form)
		printf("Trying \"%s\"\n", frm);
}

static void
say_message(dns_name_t *name, const char *msg, dns_rdata_t *rdata,
	    dig_query_t *query)
{
	isc_buffer_t *b = NULL;
	char namestr[DNS_NAME_FORMATSIZE];
	isc_region_t r;
	isc_result_t result;
	unsigned int bufsize = BUFSIZ;

	dns_name_format(name, namestr, sizeof(namestr));
 retry:
	result = isc_buffer_allocate(&b, bufsize);
	check_result(result, "isc_buffer_allocate");
	result = dns_rdata_totext(rdata, NULL, b);
	if (result == ISC_R_NOSPACE) {
		isc_buffer_free(&b);
		bufsize *= 2;
		goto retry;
	}
	check_result(result, "dns_rdata_totext");
	isc_buffer_usedregion(b, &r);
	if (query->lookup->identify_previous_line) {
		printf("Nameserver %s:\n\t",
			query->servname);
	}
	printf("%s %s %.*s", namestr,
	       msg, (int)r.length, (char *)r.base);
	if (query->lookup->identify) {
		printf(" on server %s", query->servname);
	}
	printf("\n");
	isc_buffer_free(&b);
}
static isc_result_t
printsection(dns_message_t *msg, dns_section_t sectionid,
	     const char *section_name, int headers,
	     dig_query_t *query)
{
	dns_name_t *name, *print_name;
	dns_rdataset_t *rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t target;
	isc_result_t result, loopresult;
	isc_region_t r;
	dns_name_t empty_name;
	char tbuf[4096];
	int first;
	int no_rdata;

	if (sectionid == DNS_SECTION_QUESTION)
		no_rdata = 1;
	else
		no_rdata = 0;

	if (headers)
		printf(";; %s SECTION:\n", section_name);

	dns_name_init(&empty_name, NULL);

	result = dns_message_firstname(msg, sectionid);
	if (result == ISC_R_NOMORE)
		return (ISC_R_SUCCESS);
	else if (result != ISC_R_SUCCESS)
		return (result);

	for (;;) {
		name = NULL;
		dns_message_currentname(msg, sectionid, &name);

		isc_buffer_init(&target, tbuf, sizeof(tbuf));
		first = 1;
		print_name = name;

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			if (query->lookup->rdtype == dns_rdatatype_axfr &&
			    !((!list_addresses &&
			       (list_type == dns_rdatatype_any ||
				rdataset->type == list_type)) ||
			      (list_addresses &&
			       (rdataset->type == dns_rdatatype_a ||
				rdataset->type == dns_rdatatype_aaaa ||
				rdataset->type == dns_rdatatype_ns ||
				rdataset->type == dns_rdatatype_ptr))))
				continue;
			if (!short_form) {
				result = dns_rdataset_totext(rdataset,
							     print_name,
							     0,
							     no_rdata,
							     &target);
				if (result != ISC_R_SUCCESS)
					return (result);
				UNUSED(first); /* Shut up compiler. */
			} else {
				loopresult = dns_rdataset_first(rdataset);
				while (loopresult == ISC_R_SUCCESS) {
					struct rtype *t;
					const char *rtt;
					char typebuf[DNS_RDATATYPE_FORMATSIZE];
					char typebuf2[DNS_RDATATYPE_FORMATSIZE
						     + 20];
					dns_rdataset_current(rdataset, &rdata);

					for (t = rtypes; t->text != NULL; t++) {
						if (t->type == rdata.type) {
							rtt = t->text;
							goto found;
						}
					}

					dns_rdatatype_format(rdata.type,
							     typebuf,
							     sizeof(typebuf));
					snprintf(typebuf2, sizeof(typebuf2),
						 "has %s record", typebuf);
					rtt = typebuf2;
				found:
					say_message(print_name, rtt,
						    &rdata, query);
					dns_rdata_reset(&rdata);
					loopresult =
						dns_rdataset_next(rdataset);
				}
			}
		}
		if (!short_form) {
			isc_buffer_usedregion(&target, &r);
			if (no_rdata)
				printf(";%.*s", (int)r.length,
				       (char *)r.base);
			else
				printf("%.*s", (int)r.length, (char *)r.base);
		}

		result = dns_message_nextname(msg, sectionid);
		if (result == ISC_R_NOMORE)
			break;
		else if (result != ISC_R_SUCCESS)
			return (result);
	}

	return (ISC_R_SUCCESS);
}

static isc_result_t
printrdata(dns_message_t *msg, dns_rdataset_t *rdataset, dns_name_t *owner,
	   const char *set_name, int headers)
{
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char tbuf[4096];

	UNUSED(msg);
	if (headers)
		printf(";; %s SECTION:\n", set_name);

	isc_buffer_init(&target, tbuf, sizeof(tbuf));

	result = dns_rdataset_totext(rdataset, owner, 0, 0,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

static void
chase_cnamechain(dns_message_t *msg, dns_name_t *qname) {
	isc_result_t result;
	dns_rdataset_t *rdataset;
	dns_rdata_cname_t cname;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned int i = msg->counts[DNS_SECTION_ANSWER];

	while (i-- > 0) {
		rdataset = NULL;
		result = dns_message_findname(msg, DNS_SECTION_ANSWER, qname,
					      dns_rdatatype_cname, 0, NULL,
					      &rdataset);
		if (result != ISC_R_SUCCESS)
			return;
		result = dns_rdataset_first(rdataset);
		check_result(result, "dns_rdataset_first");
		dns_rdata_reset(&rdata);
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct_cname(&rdata, &cname);
		check_result(result, "dns_rdata_tostruct_cname");
		dns_name_copy(&cname.cname, qname, NULL);
		dns_rdata_freestruct_cname(&cname);
	}
}

static isc_result_t
printmessage(dig_query_t *query, dns_message_t *msg, int headers) {
	int did_flag = 0;
	dns_rdataset_t *opt, *tsig = NULL;
	dns_name_t *tsigname;
	isc_result_t result = ISC_R_SUCCESS;
	int force_error;

	UNUSED(headers);

	/*
	 * We get called multiple times.
	 * Preserve any existing error status.
	 */
	force_error = (seen_error == 1) ? 1 : 0;
	seen_error = 1;
	if (listed_server && !printed_server) {
		char sockstr[ISC_SOCKADDR_FORMATSIZE];

		printf("Using domain server:\n");
		printf("Name: %s\n", query->userarg);
		isc_sockaddr_format(&query->sockaddr, sockstr,
				    sizeof(sockstr));
		printf("Address: %s\n", sockstr);
		printf("Aliases: \n\n");
		printed_server = 1;
	}

	if (msg->rcode != 0) {
		char namestr[DNS_NAME_FORMATSIZE];
		dns_name_format(query->lookup->name, namestr, sizeof(namestr));

		if (query->lookup->identify_previous_line)
			printf("Nameserver %s:\n\t%s not found: %d(%s)\n",
			       query->servname,
			       (msg->rcode != dns_rcode_nxdomain) ? namestr :
			       query->lookup->textname, msg->rcode,
			       rcode_totext(msg->rcode));
		else
			printf("Host %s not found: %d(%s)\n",
			       (msg->rcode != dns_rcode_nxdomain) ? namestr :
			       query->lookup->textname, msg->rcode,
			       rcode_totext(msg->rcode));
		return (ISC_R_SUCCESS);
	}

	if (default_lookups && query->lookup->rdtype == dns_rdatatype_a) {
		char namestr[DNS_NAME_FORMATSIZE];
		dig_lookup_t *lookup;
		dns_fixedname_t fixed;
		dns_name_t *name;

		/* Add AAAA and MX lookups. */
		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		dns_name_copy(query->lookup->name, name, NULL);
		chase_cnamechain(msg, name);
		dns_name_format(name, namestr, sizeof(namestr));
		lookup = clone_lookup(query->lookup, 0);
		if (lookup != NULL) {
			strlcpy(lookup->textname, namestr,
				sizeof(lookup->textname));
			lookup->rdtype = dns_rdatatype_aaaa;
			lookup->rdtypeset = 1;
			lookup->origin = NULL;
			lookup->retries = tries;
			ISC_LIST_APPEND(lookup_list, lookup, link);
		}
		lookup = clone_lookup(query->lookup, 0);
		if (lookup != NULL) {
			strlcpy(lookup->textname, namestr,
				sizeof(lookup->textname));
			lookup->rdtype = dns_rdatatype_mx;
			lookup->rdtypeset = 1;
			lookup->origin = NULL;
			lookup->retries = tries;
			ISC_LIST_APPEND(lookup_list, lookup, link);
		}
	}

	if (!short_form) {
		printf(";; ->>HEADER<<- opcode: %s, status: %s, id: %u\n",
		       opcodetext[msg->opcode], rcode_totext(msg->rcode),
		       msg->id);
		printf(";; flags: ");
		if ((msg->flags & DNS_MESSAGEFLAG_QR) != 0) {
			printf("qr");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_AA) != 0) {
			printf("%saa", did_flag ? " " : "");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
			printf("%stc", did_flag ? " " : "");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_RD) != 0) {
			printf("%srd", did_flag ? " " : "");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_RA) != 0) {
			printf("%sra", did_flag ? " " : "");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_AD) != 0) {
			printf("%sad", did_flag ? " " : "");
			did_flag = 1;
		}
		if ((msg->flags & DNS_MESSAGEFLAG_CD) != 0) {
			printf("%scd", did_flag ? " " : "");
			did_flag = 1;
			POST(did_flag);
		}
		printf("; QUERY: %u, ANSWER: %u, "
		       "AUTHORITY: %u, ADDITIONAL: %u\n",
		       msg->counts[DNS_SECTION_QUESTION],
		       msg->counts[DNS_SECTION_ANSWER],
		       msg->counts[DNS_SECTION_AUTHORITY],
		       msg->counts[DNS_SECTION_ADDITIONAL]);
		opt = dns_message_getopt(msg);
		if (opt != NULL)
			printf(";; EDNS: version: %u, udp=%u\n",
			       (unsigned int)((opt->ttl & 0x00ff0000) >> 16),
			       (unsigned int)opt->rdclass);
		tsigname = NULL;
		tsig = dns_message_gettsig(msg, &tsigname);
		if (tsig != NULL)
			printf(";; PSEUDOSECTIONS: TSIG\n");
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_QUESTION]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_QUESTION, "QUESTION",
				      1, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ANSWER])) {
		if (!short_form)
			printf("\n");
		result = printsection(msg, DNS_SECTION_ANSWER, "ANSWER",
				      !short_form, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_AUTHORITY]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_AUTHORITY, "AUTHORITY",
				      1, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ADDITIONAL]) &&
	    !short_form) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_ADDITIONAL,
				      "ADDITIONAL", 1, query);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if ((tsig != NULL) && !short_form) {
		printf("\n");
		result = printrdata(msg, tsig, tsigname,
				    "PSEUDOSECTION TSIG", 1);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (!short_form)
		printf("\n");

	if (short_form && !default_lookups &&
	    ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ANSWER])) {
		char namestr[DNS_NAME_FORMATSIZE];
		char typestr[DNS_RDATATYPE_FORMATSIZE];
		dns_name_format(query->lookup->name, namestr, sizeof(namestr));
		dns_rdatatype_format(query->lookup->rdtype, typestr,
				     sizeof(typestr));
		printf("%s has no %s record\n", namestr, typestr);
	}
	seen_error = force_error;
	return (result);
}

static const char * optstring = "46ac:dilnrst:vVwCDN:R:TW:";

/*% version */
static void
version(void) {
	fputs("host " VERSION "\n", stderr);
}

static void
pre_parse_args(int argc, char **argv) {
	int c;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case '4':
			if (ipv6only)
				fatal("only one of -4 and -6 allowed");
			ipv4only = 1;
			break;
		case '6':
			if (ipv4only)
				fatal("only one of -4 and -6 allowed");
			ipv6only = 1;
			break;
		case 'a': break;
		case 'c': break;
		case 'd': break;
		case 'i': break;
		case 'l': break;
		case 'n': break;
		case 'r': break;
		case 's': break;
		case 't': break;
		case 'v': break;
		case 'V':
			  version();
			  exit(0);
			  break;
		case 'w': break;
		case 'C': break;
		case 'D':
			if (debugging)
				debugtiming = 1;
			debugging = 1;
			break;
		case 'N': break;
		case 'R': break;
		case 'T': break;
		case 'W': break;
		default:
			show_usage();
		}
	}
	optind = 1;
	optreset = 1;
}

static void
parse_args(int argc, char **argv) {
	char hostname[MXNAME];
	dig_lookup_t *lookup;
	int c;
	char store[MXNAME];
	isc_textregion_t tr;
	isc_result_t result = ISC_R_SUCCESS;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	uint32_t serial = 0;
	const char *errstr;

	lookup = make_empty_lookup();

	lookup->servfail_stops = 0;
	lookup->comments = 0;

	while ((c = getopt(argc, argv, optstring)) != -1) {
		switch (c) {
		case 'l':
			lookup->tcp_mode = 1;
			lookup->rdtype = dns_rdatatype_axfr;
			lookup->rdtypeset = 1;
			fatalexit = 3;
			break;
		case 'v':
		case 'd':
			short_form = 0;
			break;
		case 'r':
			lookup->recurse = 0;
			break;
		case 't':
			if (strncasecmp(optarg,	"ixfr=", 5) == 0) {
				rdtype = dns_rdatatype_ixfr;
				/* XXXMPA add error checking */
				serial = strtoul(optarg + 5,
						 NULL, 10);
				result = ISC_R_SUCCESS;
			} else {
				tr.base = optarg;
				tr.length = strlen(optarg);
				result = dns_rdatatype_fromtext(&rdtype,
						   (isc_textregion_t *)&tr);
			}

			if (result != ISC_R_SUCCESS) {
				fatalexit = 2;
				fatal("invalid type: %s\n", optarg);
			}
			if (!lookup->rdtypeset ||
			    lookup->rdtype != dns_rdatatype_axfr)
				lookup->rdtype = rdtype;
			lookup->rdtypeset = 1;
			if (rdtype == dns_rdatatype_axfr) {
				/* -l -t any -v */
				list_type = dns_rdatatype_any;
				short_form = 0;
				lookup->tcp_mode = 1;
			} else if (rdtype == dns_rdatatype_ixfr) {
				lookup->ixfr_serial = serial;
				lookup->tcp_mode = 1;
				list_type = rdtype;
			} else
				list_type = rdtype;
			list_addresses = 0;
			default_lookups = 0;
			break;
		case 'c':
			tr.base = optarg;
			tr.length = strlen(optarg);
			result = dns_rdataclass_fromtext(&rdclass,
						   (isc_textregion_t *)&tr);

			if (result != ISC_R_SUCCESS) {
				fatalexit = 2;
				fatal("invalid class: %s\n", optarg);
			} else {
				lookup->rdclass = rdclass;
				lookup->rdclassset = 1;
			}
			default_lookups = 0;
			break;
		case 'a':
			if (!lookup->rdtypeset ||
			    lookup->rdtype != dns_rdatatype_axfr)
				lookup->rdtype = dns_rdatatype_any;
			list_type = dns_rdatatype_any;
			list_addresses = 0;
			lookup->rdtypeset = 1;
			short_form = 0;
			default_lookups = 0;
			break;
		case 'i':
			lookup->ip6_int = 1;
			break;
		case 'n':
			/* deprecated */
			break;
		case 'm':
			/* Handled by pre_parse_args(). */
			break;
		case 'w':
			/*
			 * The timer routines are coded such that
			 * timeout==MAXINT doesn't enable the timer
			 */
			timeout = INT_MAX;
			break;
		case 'W':
			timeout = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "timeout is %s: %s", errstr, optarg);
			if (timeout < 1)
				timeout = 1;
			break;
		case 'R':
			tries = strtonum(optarg, INT_MIN, INT_MAX - 1, &errstr);
			if (errstr != NULL)
				errx(1, "retries is %s: %s", errstr, optarg);
			tries++;
			if (tries < 2)
				tries = 2;
			break;
		case 'T':
			lookup->tcp_mode = 1;
			break;
		case 'C':
			debug("showing all SOAs");
			lookup->rdtype = dns_rdatatype_ns;
			lookup->rdtypeset = 1;
			lookup->rdclass = dns_rdataclass_in;
			lookup->rdclassset = 1;
			lookup->ns_search_only = 1;
			lookup->trace_root = 1;
			lookup->identify_previous_line = 1;
			default_lookups = 0;
			break;
		case 'N':
			debug("setting NDOTS to %s", optarg);
			ndots = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "ndots is %s: %s", errstr, optarg);
			break;
		case 'D':
			/* Handled by pre_parse_args(). */
			break;
		case '4':
			/* Handled by pre_parse_args(). */
			break;
		case '6':
			/* Handled by pre_parse_args(). */
			break;
		case 's':
			lookup->servfail_stops = 1;
			break;
		default:
			show_usage();
		}
	}

	lookup->retries = tries;

	argc -= optind;
	argv += optind;

	if (argc == 0)
		show_usage();

	strlcpy(hostname, argv[0], sizeof(hostname));

	if (argc >= 2) {
		isc_result_t res;

		if ((res = set_nameserver(argv[1])))
			fatal("couldn't get address for '%s': %s",
			    argv[1], isc_result_totext(res));
		debug("server is %s", *argv + 1);
		listed_server = 1;
	} else
		check_ra = 1;

	lookup->pending = 0;
	if (get_reverse(store, sizeof(store), hostname,
			lookup->ip6_int, 1) == ISC_R_SUCCESS) {
		strlcpy(lookup->textname, store, sizeof(lookup->textname));
		lookup->rdtype = dns_rdatatype_ptr;
		lookup->rdtypeset = 1;
		default_lookups = 0;
	} else {
		strlcpy(lookup->textname, hostname, sizeof(lookup->textname));
		usesearch = 1;
	}
	lookup->new_search = 1;
	ISC_LIST_APPEND(lookup_list, lookup, link);
}

int
host_main(int argc, char **argv) {
	isc_result_t result;

	tries = 2;

	ISC_LIST_INIT(lookup_list);
	ISC_LIST_INIT(server_list);
	ISC_LIST_INIT(root_hints_server_list);
	ISC_LIST_INIT(search_list);

	fatalexit = 1;

	/* setup dighost callbacks */
	dighost_printmessage = printmessage;
	dighost_received = received;
	dighost_trying = trying;
	dighost_shutdown = host_shutdown;

	debug("main()");
	progname = argv[0];
	pre_parse_args(argc, argv);
	result = isc_app_start();
	check_result(result, "isc_app_start");

	if (pledge("stdio rpath inet dns", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	setup_libs();

	if (pledge("stdio inet dns", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	parse_args(argc, argv);
	setup_system(ipv4only, ipv6only);
	result = isc_app_onrun(global_task, onrun_callback, NULL);
	check_result(result, "isc_app_onrun");
	isc_app_run();
	cancel_all();
	destroy_libs();
	return ((seen_error == 0) ? 0 : 1);
}
