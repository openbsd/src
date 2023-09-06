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

/* $Id: dig.c,v 1.20 2023/09/06 04:57:28 jsg Exp $ */

/*! \file */
#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>

#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <isc/app.h>

#include <string.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatatype.h>
#include <dns/rdataclass.h>
#include <dns/result.h>
#include <dns/tsig.h>

#include "dig.h"

#define ADD_STRING(b, s) { 				\
	if (strlen(s) >= isc_buffer_availablelength(b)) \
		return (ISC_R_NOSPACE); 		\
	else 						\
		isc_buffer_putstr(b, s); 		\
}

dig_lookup_t *default_lookup = NULL;

static char *batchname = NULL;
static FILE *batchfp = NULL;
static char *argv0;
static int addresscount = 0;

static char domainopt[DNS_NAME_MAXTEXT];
static char sitvalue[256];

static int short_form = 0, printcmd = 1,
	ip6_int = 0, plusquest = 0, pluscomm = 0,
	multiline = 0, nottl = 0, noclass = 0,
	onesoa = 0, use_usec = 0, nocrypto = 0,
	ipv4only = 0, ipv6only = 0;
static uint32_t splitwidth = 0xffffffff;

/*% rrcomments are neither explicitly enabled nor disabled by default */
static int rrcomments = 0;

/*% opcode text */
static const char * const opcodetext[] = {
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

/*% return code text */
static const char * const rcodetext[] = {
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

/*% safe rcodetext[] */
static const char *
rcode_totext(dns_rcode_t rcode)
{
	static char buf[sizeof("?65535")];

	if (rcode == dns_rcode_badcookie)
		return ("BADCOOKIE");
	if (rcode >= (sizeof(rcodetext)/sizeof(rcodetext[0]))) {
		snprintf(buf, sizeof(buf), "?%u", rcode);
		return (buf);
	}
	return (rcodetext[rcode]);
}

/*% print usage */
static void
print_usage(FILE *fp) {
	fputs(
"usage: dig [@server] [-46hiuv] [-b sourceaddr[#port]] [-c class] [-f file]\n"
"           [-k keyfile] [-p port] [-q name] [-t type] [-x addr]\n"
"           [-y [hmac:]name:key] [name] [type] [class]\n"
"           +[no]aaonly +[no]additional +[no]adflag +[no]all +[no]answer\n"
"           +[no]authority +[no]besteffort +bufsize=# +[no]cdflag +[no]class\n"
"           +[no]cmd +[no]comments +[no]cookie[=value] +[no]crypto +[no]dnssec\n"
"           +domain=name +[no]edns[=#] +ednsflags[=#] +[no]ednsnegotiation\n"
"           +[no]ednsopt[=code[:value]] +[no]expire +[no]fail +[no]identify\n"
"           +[no]ignore +[no]keepopen +[no]multiline +ndots=# +[no]nsid\n"
"           +[no]nssearch +[no]onesoa +[no]opcode=# +[no]qr +[no]question\n"
"           +[no]recurse +retry=# +[no]rrcomments +[no]search +[no]short\n"
"           +[no]showsearch +[no]split=# +[no]stats +[no]subnet=addr[/prefix]\n"
"           +[no]tcp +timeout=# +[no]trace +tries=# +[no]ttlid +[no]vc\n", fp);
}

static __dead void
usage(void);

static void
usage(void) {
	print_usage(stderr);
	exit(1);
}

/*% version */
static void
version(void) {
	fputs("dig " VERSION "\n", stderr);
}

/*% help */
static void
help(void) {
	print_usage(stdout);
}

/*%
 * Callback from dighost.c to print the received message.
 */
static void
received(unsigned int bytes, struct sockaddr_storage *from, dig_query_t *query) {
	time_t tnow;
	struct tm tmnow;
	char time_str[100];
	char fromtext[ISC_SOCKADDR_FORMATSIZE];

	isc_sockaddr_format(from, fromtext, sizeof(fromtext));

	if (query->lookup->stats && !short_form) {
		if (use_usec)
			printf(";; Query time: %lld usec\n",
			    uelapsed(&query->time_recv, &query->time_sent));
		else
			printf(";; Query time: %lld msec\n",
			    uelapsed(&query->time_recv, &query->time_sent) /
			    1000);
		printf(";; SERVER: %s(%s)\n", fromtext, query->servname);
		time(&tnow);
		tmnow  = *localtime(&tnow);

		if (strftime(time_str, sizeof(time_str),
			     "%a %b %d %H:%M:%S %Z %Y", &tmnow) > 0U)
			printf(";; WHEN: %s\n", time_str);
		if (query->lookup->doing_xfr) {
			printf(";; XFR size: %u records (messages %u, "
			       "bytes %llu)\n",
			       query->rr_count, query->msg_count,
			       query->byte_count);
		} else {
			printf(";; MSG SIZE  rcvd: %u\n", bytes);
		}
		if (tsigkey != NULL) {
			if (!validated)
				puts(";; WARNING -- Some TSIG could not "
				     "be validated");
		}
		if ((tsigkey == NULL) && (keysecret[0] != 0)) {
			puts(";; WARNING -- TSIG key was not used.");
		}
		puts("");
	} else if (query->lookup->identify && !short_form) {
		if (use_usec)
			printf(";; Received %llu bytes "
			    "from %s(%s) in %lld us\n\n",
			    query->lookup->doing_xfr
			    ? query->byte_count
			    : (uint64_t)bytes,
			    fromtext, query->userarg,
			    uelapsed(&query->time_recv, &query->time_sent));
		else
			printf(";; Received %llu bytes "
			    "from %s(%s) in %lld ms\n\n",
			    query->lookup->doing_xfr
			    ?  query->byte_count
			    : (uint64_t)bytes,
			    fromtext, query->userarg,
			    uelapsed(&query->time_recv, &query->time_sent) /
			    1000);
	}
}

/*
 * Callback from dighost.c to print that it is trying a server.
 * Not used in dig.
 * XXX print_trying
 */
static void
trying(char *frm, dig_lookup_t *lookup) {
	UNUSED(frm);
	UNUSED(lookup);
}

/*%
 * Internal print routine used to print short form replies.
 */
static isc_result_t
say_message(dns_rdata_t *rdata, dig_query_t *query, isc_buffer_t *buf) {
	isc_result_t result;
	char store[sizeof(" in 18446744073709551616 us.")];
	unsigned int styleflags = 0;

	if (query->lookup->trace || query->lookup->ns_search_only) {
		result = dns_rdatatype_totext(rdata->type, buf);
		if (result != ISC_R_SUCCESS)
			return (result);
		ADD_STRING(buf, " ");
	}

	/* Turn on rrcomments if explicitly enabled */
	if (rrcomments > 0)
		styleflags |= DNS_STYLEFLAG_RRCOMMENT;
	if (nocrypto)
		styleflags |= DNS_STYLEFLAG_NOCRYPTO;
	result = dns_rdata_tofmttext(rdata, NULL, styleflags, 0,
				     splitwidth, " ", buf);
	if (result == ISC_R_NOSPACE)
		return (result);
	check_result(result, "dns_rdata_totext");
	if (query->lookup->identify) {
		ADD_STRING(buf, " from server ");
		ADD_STRING(buf, query->servname);
		if (use_usec)
			snprintf(store, sizeof(store), " in %lld us.",
			    uelapsed(&query->time_recv, &query->time_sent));
		else
			snprintf(store, sizeof(store), " in %lld ms.",
			    uelapsed(&query->time_recv, &query->time_sent) /
			    1000);
		ADD_STRING(buf, store);
	}
	ADD_STRING(buf, "\n");
	return (ISC_R_SUCCESS);
}

/*%
 * short_form message print handler.  Calls above say_message()
 */
static isc_result_t
short_answer(dns_message_t *msg, dns_messagetextflag_t flags,
	     isc_buffer_t *buf, dig_query_t *query)
{
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	isc_result_t result, loopresult;
	dns_name_t empty_name;
	dns_rdata_t rdata = DNS_RDATA_INIT;

	UNUSED(flags);

	dns_name_init(&empty_name, NULL);
	result = dns_message_firstname(msg, DNS_SECTION_ANSWER);
	if (result == ISC_R_NOMORE)
		return (ISC_R_SUCCESS);
	else if (result != ISC_R_SUCCESS)
		return (result);

	for (;;) {
		name = NULL;
		dns_message_currentname(msg, DNS_SECTION_ANSWER, &name);

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			loopresult = dns_rdataset_first(rdataset);
			while (loopresult == ISC_R_SUCCESS) {
				dns_rdataset_current(rdataset, &rdata);
				result = say_message(&rdata, query,
						     buf);
				if (result == ISC_R_NOSPACE)
					return (result);
				check_result(result, "say_message");
				loopresult = dns_rdataset_next(rdataset);
				dns_rdata_reset(&rdata);
			}
		}
		result = dns_message_nextname(msg, DNS_SECTION_ANSWER);
		if (result == ISC_R_NOMORE)
			break;
		else if (result != ISC_R_SUCCESS)
			return (result);
	}

	return (ISC_R_SUCCESS);
}

static int
isdotlocal(dns_message_t *msg) {
	isc_result_t result;
	static unsigned char local_ndata[] = { "\005local\0" };
	static unsigned char local_offsets[] = { 0, 6 };
	static dns_name_t local =
		DNS_NAME_INITABSOLUTE(local_ndata, local_offsets);

	for (result = dns_message_firstname(msg, DNS_SECTION_QUESTION);
	     result == ISC_R_SUCCESS;
	     result = dns_message_nextname(msg, DNS_SECTION_QUESTION))
	{
		dns_name_t *name = NULL;
		dns_message_currentname(msg, DNS_SECTION_QUESTION, &name);
		if (dns_name_issubdomain(name, &local))
			return (1);
	}
	return (0);
}

/*
 * Callback from dighost.c to print the reply from a server
 */
static isc_result_t
printmessage(dig_query_t *query, dns_message_t *msg, int headers) {
	isc_result_t result;
	dns_messagetextflag_t flags;
	isc_buffer_t *buf = NULL;
	unsigned int len = OUTPUTBUF;
	dns_master_style_t *style = NULL;
	unsigned int styleflags = 0;

	styleflags |= DNS_STYLEFLAG_REL_OWNER;
	if (query->lookup->comments)
		styleflags |= DNS_STYLEFLAG_COMMENT;
	/* Turn on rrcomments if explicitly enabled */
	if (rrcomments > 0)
		styleflags |= DNS_STYLEFLAG_RRCOMMENT;
	if (nottl)
		styleflags |= DNS_STYLEFLAG_NO_TTL;
	if (noclass)
		styleflags |= DNS_STYLEFLAG_NO_CLASS;
	if (nocrypto)
		styleflags |= DNS_STYLEFLAG_NOCRYPTO;
	if (multiline) {
		styleflags |= DNS_STYLEFLAG_OMIT_OWNER;
		styleflags |= DNS_STYLEFLAG_OMIT_CLASS;
		styleflags |= DNS_STYLEFLAG_REL_DATA;
		styleflags |= DNS_STYLEFLAG_OMIT_TTL;
		styleflags |= DNS_STYLEFLAG_TTL;
		styleflags |= DNS_STYLEFLAG_MULTILINE;
		/* Turn on rrcomments unless explicitly disabled */
		if (rrcomments >= 0)
			styleflags |= DNS_STYLEFLAG_RRCOMMENT;
	}
	if (multiline || (nottl && noclass))
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 24, 24, 32, 80, 8,
						 splitwidth);
	else if (nottl || noclass)
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 24, 32, 40, 80, 8,
						 splitwidth);
	else
		result = dns_master_stylecreate2(&style, styleflags,
						 24, 32, 40, 48, 80, 8,
						 splitwidth);
	check_result(result, "dns_master_stylecreate");

	if (query->lookup->cmdline[0] != 0) {
		if (!short_form)
			fputs(query->lookup->cmdline, stdout);
		query->lookup->cmdline[0]=0;
	}
	debug("printmessage(%s %s %s)", headers ? "headers" : "noheaders",
	      query->lookup->comments ? "comments" : "nocomments",
	      short_form ? "short_form" : "long_form");

	flags = 0;
	if (!headers) {
		flags |= DNS_MESSAGETEXTFLAG_NOHEADERS;
		flags |= DNS_MESSAGETEXTFLAG_NOCOMMENTS;
	}
	if (onesoa && query->lookup->rdtype == dns_rdatatype_axfr)
		flags |= (query->msg_count == 0) ? DNS_MESSAGETEXTFLAG_ONESOA :
						   DNS_MESSAGETEXTFLAG_OMITSOA;
	if (!query->lookup->comments)
		flags |= DNS_MESSAGETEXTFLAG_NOCOMMENTS;

	result = isc_buffer_allocate(&buf, len);
	check_result(result, "isc_buffer_allocate");

	if (query->lookup->comments && !short_form) {
		if (query->lookup->cmdline[0] != 0)
			printf("; %s\n", query->lookup->cmdline);
		if (msg == query->lookup->sendmsg)
			printf(";; Sending:\n");
		else
			printf(";; Got answer:\n");

		if (headers) {
			if (isdotlocal(msg)) {
				printf(";; WARNING: .local is reserved for "
				       "Multicast DNS\n;; You are currently "
				       "testing what happens when an mDNS "
				       "query is leaked to DNS\n");
			}
			printf(";; ->>HEADER<<- opcode: %s, status: %s, "
			       "id: %u\n",
			       opcodetext[msg->opcode],
			       rcode_totext(msg->rcode),
			       msg->id);
			printf(";; flags:");
			if ((msg->flags & DNS_MESSAGEFLAG_QR) != 0)
				printf(" qr");
			if ((msg->flags & DNS_MESSAGEFLAG_AA) != 0)
				printf(" aa");
			if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0)
				printf(" tc");
			if ((msg->flags & DNS_MESSAGEFLAG_RD) != 0)
				printf(" rd");
			if ((msg->flags & DNS_MESSAGEFLAG_RA) != 0)
				printf(" ra");
			if ((msg->flags & DNS_MESSAGEFLAG_AD) != 0)
				printf(" ad");
			if ((msg->flags & DNS_MESSAGEFLAG_CD) != 0)
				printf(" cd");
			if ((msg->flags & 0x0040U) != 0)
				printf("; MBZ: 0x4");

			printf("; QUERY: %u, ANSWER: %u, "
			       "AUTHORITY: %u, ADDITIONAL: %u\n",
			       msg->counts[DNS_SECTION_QUESTION],
			       msg->counts[DNS_SECTION_ANSWER],
			       msg->counts[DNS_SECTION_AUTHORITY],
			       msg->counts[DNS_SECTION_ADDITIONAL]);

			if (msg != query->lookup->sendmsg &&
			    (msg->flags & DNS_MESSAGEFLAG_RD) != 0 &&
			    (msg->flags & DNS_MESSAGEFLAG_RA) == 0)
				printf(";; WARNING: recursion requested "
				       "but not available\n");
		}
		if (msg != query->lookup->sendmsg &&
		    query->lookup->edns != -1 && msg->opt == NULL &&
		    (msg->rcode == dns_rcode_formerr ||
		     msg->rcode == dns_rcode_notimp))
			printf("\n;; WARNING: EDNS query returned status "
			       "%s - retry with '%s+noedns'\n",
			       rcode_totext(msg->rcode),
			       query->lookup->dnssec ? "+nodnssec ": "");
		if (msg != query->lookup->sendmsg && extrabytes != 0U)
			printf(";; WARNING: Message has %u extra byte%s at "
			       "end\n", extrabytes, extrabytes != 0 ? "s" : "");
	}

repopulate_buffer:

	if (query->lookup->comments && headers && !short_form) {
		result = dns_message_pseudosectiontotext(msg,
			 DNS_PSEUDOSECTION_OPT,
			 style, flags, buf);
		if (result == ISC_R_NOSPACE) {
buftoosmall:
			len += OUTPUTBUF;
			isc_buffer_free(&buf);
			result = isc_buffer_allocate(&buf, len);
			if (result == ISC_R_SUCCESS)
				goto repopulate_buffer;
			else
				goto cleanup;
		}
		check_result(result,
		     "dns_message_pseudosectiontotext");
	}

	if (query->lookup->section_question && headers) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_QUESTION,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		}
	}
	if (query->lookup->section_answer) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_ANSWER,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		} else {
			result = short_answer(msg, flags, buf, query);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "short_answer");
		}
	}
	if (query->lookup->section_authority) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						       DNS_SECTION_AUTHORITY,
						       style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
		}
	}
	if (query->lookup->section_additional) {
		if (!short_form) {
			result = dns_message_sectiontotext(msg,
						      DNS_SECTION_ADDITIONAL,
						      style, flags, buf);
			if (result == ISC_R_NOSPACE)
				goto buftoosmall;
			check_result(result, "dns_message_sectiontotext");
			/*
			 * Only print the signature on the first record.
			 */
			if (headers) {
				result = dns_message_pseudosectiontotext(
						   msg,
						   DNS_PSEUDOSECTION_TSIG,
						   style, flags, buf);
				if (result == ISC_R_NOSPACE)
					goto buftoosmall;
				check_result(result,
					  "dns_message_pseudosectiontotext");
				result = dns_message_pseudosectiontotext(
						   msg,
						   DNS_PSEUDOSECTION_SIG0,
						   style, flags, buf);
				if (result == ISC_R_NOSPACE)
					goto buftoosmall;
				check_result(result,
					   "dns_message_pseudosectiontotext");
			}
		}
	}

	if (headers && query->lookup->comments && !short_form)
		printf("\n");

	printf("%.*s", (int)isc_buffer_usedlength(buf),
	       (char *)isc_buffer_base(buf));
	isc_buffer_free(&buf);

cleanup:
	if (style != NULL)
		dns_master_styledestroy(&style);
	return (result);
}

/*%
 * print the greeting message when the program first starts up.
 */
static void
printgreeting(int argc, char **argv, dig_lookup_t *lookup) {
	int i;
	static int first = 1;
	char append[MXNAME];

	if (printcmd) {
		snprintf(lookup->cmdline, sizeof(lookup->cmdline),
			 "%s; <<>> dig " VERSION " <<>>",
			 first?"\n":"");
		i = 1;
		while (i < argc) {
			snprintf(append, sizeof(append), " %s", argv[i++]);
			strlcat(lookup->cmdline, append,
				sizeof(lookup->cmdline));
		}
		strlcat(lookup->cmdline, "\n", sizeof(lookup->cmdline));
		if (first && addresscount != 0) {
			snprintf(append, sizeof(append),
				 "; (%d server%s found)\n",
				 addresscount,
				 addresscount > 1 ? "s" : "");
			strlcat(lookup->cmdline, append,
				sizeof(lookup->cmdline));
		}
		if (first) {
			snprintf(append, sizeof(append),
				 ";; global options:%s%s\n",
				 short_form ? " +short" : "",
				 printcmd ? " +cmd" : "");
			first = 0;
			strlcat(lookup->cmdline, append,
				sizeof(lookup->cmdline));
		}
	}
}

static void
plus_option(const char *option, int is_batchfile,
	    dig_lookup_t *lookup)
{
	isc_result_t result;
	char option_store[256];
	char *cmd, *value, *ptr, *code, *ep;
	const char *errstr;
	long lval;
	uint32_t num;
	int state = 1;
	size_t n;

	strlcpy(option_store, option, sizeof(option_store));
	ptr = option_store;
	cmd = next_token(&ptr, "=");
	if (cmd == NULL) {
		printf(";; Invalid option %s\n", option_store);
		return;
	}
	value = ptr;
	if (strncasecmp(cmd, "no", 2)==0) {
		cmd += 2;
		state = 0;
	}

#define FULLCHECK(A) \
	do { \
		size_t _l = strlen(cmd); \
		if (_l >= sizeof(A) || strncasecmp(cmd, A, _l) != 0) \
			goto invalid_option; \
	} while (0)
#define FULLCHECK2(A, B) \
	do { \
		size_t _l = strlen(cmd); \
		if ((_l >= sizeof(A) || strncasecmp(cmd, A, _l) != 0) && \
		    (_l >= sizeof(B) || strncasecmp(cmd, B, _l) != 0)) \
			goto invalid_option; \
	} while (0)

	switch (cmd[0]) {
	case 'a':
		switch (cmd[1]) {
		case 'a': /* aaonly / aaflag */
			FULLCHECK2("aaonly", "aaflag");
			lookup->aaonly = state;
			break;
		case 'd':
			switch (cmd[2]) {
			case 'd': /* additional */
				FULLCHECK("additional");
				lookup->section_additional = state;
				break;
			case 'f': /* adflag */
			case '\0': /* +ad is a synonym for +adflag */
				FULLCHECK("adflag");
				lookup->adflag = state;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'l': /* all */
			FULLCHECK("all");
			lookup->section_question = state;
			lookup->section_authority = state;
			lookup->section_answer = state;
			lookup->section_additional = state;
			lookup->comments = state;
			lookup->stats = state;
			printcmd = state;
			break;
		case 'n': /* answer */
			FULLCHECK("answer");
			lookup->section_answer = state;
			break;
		case 'u': /* authority */
			FULLCHECK("authority");
			lookup->section_authority = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'b':
		switch (cmd[1]) {
		case 'e':/* besteffort */
			FULLCHECK("besteffort");
			lookup->besteffort = state;
			break;
		case 'u':/* bufsize */
			FULLCHECK("bufsize");
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			num = strtonum(value, 0, COMMSIZE, &errstr);
			if (errstr != NULL)
				fatal("buffer size is %s: '%s'", errstr, value);
			lookup->udpsize = num;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'c':
		switch (cmd[1]) {
		case 'd':/* cdflag */
			switch (cmd[2]) {
			case 'f': /* cdflag */
			case '\0': /* +cd is a synonym for +cdflag */
				FULLCHECK("cdflag");
				lookup->cdflag = state;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'l': /* class */
			/* keep +cl for backwards compatibility */
			FULLCHECK2("cl", "class");
			noclass = !state;
			break;
		case 'm': /* cmd */
			FULLCHECK("cmd");
			printcmd = state;
			break;
		case 'o': /* comments */
			switch (cmd[2]) {
			case 'o':
				FULLCHECK("cookie");
				goto sit;
			case 'm':
				FULLCHECK("comments");
				lookup->comments = state;
				if (lookup == default_lookup)
					pluscomm = state;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'r':
			FULLCHECK("crypto");
			nocrypto = !state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'd':
		switch (cmd[1]) {
		case 'e': /* defname */
			FULLCHECK("defname");
			if (!lookup->trace) {
				usesearch = state;
			}
			break;
		case 'n': /* dnssec */
			FULLCHECK("dnssec");
			if (state && lookup->edns == -1)
				lookup->edns = 0;
			lookup->dnssec = state;
			break;
		case 'o': /* domain */
			FULLCHECK("domain");
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			strlcpy(domainopt, value, sizeof(domainopt));
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'e':
		switch (cmd[1]) {
		case 'd':
			switch(cmd[2]) {
			case 'n':
				switch (cmd[3]) {
				case 's':
					switch (cmd[4]) {
					case 0:
						FULLCHECK("edns");
						if (!state) {
							lookup->edns = -1;
							break;
						}
						if (value == NULL) {
							lookup->edns = 0;
							break;
						}
						num = strtonum(value, 0, 255,
						    &errstr);
						if (errstr != NULL)
							fatal("edns is %s: "
							    "'%s'", errstr,
							    value);
						lookup->edns = num;
						break;
					case 'f':
						FULLCHECK("ednsflags");
						if (!state) {
							lookup->ednsflags = 0;
							break;
						}
						if (value == NULL) {
							lookup->ednsflags = 0;
							break;
						}
						errno = 0;
						lval = strtol(value, &ep, 0);
						if (value[0] == '\0' || *ep !=
						    '\0' || lval < 0 || lval >
						    0xffff || errno != 0)
							fatal("Couldn't parse "
							      "ednsflags");
						lookup->ednsflags = lval;
						break;
					case 'n':
						FULLCHECK("ednsnegotiation");
						lookup->ednsneg = state;
						break;
					case 'o':
						FULLCHECK("ednsopt");
						if (!state) {
							lookup->ednsoptscnt = 0;
							break;
						}
						if (value == NULL)
							fatal("ednsopt no "
							      "code point "
							      "specified");
						code = next_token(&value, ":");
						save_opt(lookup, code, value);
						break;
					default:
						goto invalid_option;
					}
					break;
				default:
					goto invalid_option;
				}
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'x':
			FULLCHECK("expire");
			lookup->expire = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'f': /* fail */
		FULLCHECK("fail");
		lookup->servfail_stops = state;
		break;
	case 'i':
		switch (cmd[1]) {
		case 'd': /* identify */
			switch (cmd[2]) {
			case 'e':
				FULLCHECK("identify");
				lookup->identify = state;
				break;
			case 'n':
				FULLCHECK("idnout");
				fprintf(stderr, ";; IDN support not enabled\n");
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'g': /* ignore */
		default: /*
			  * Inherits default for compatibility (+[no]i*).
			  */
			FULLCHECK("ignore");
			lookup->ignore = state;
		}
		break;
	case 'k':
		FULLCHECK("keepopen");
		keep_open = state;
		break;
	case 'm': /* multiline */
		FULLCHECK("multiline");
		multiline = state;
		break;
	case 'n':
		switch (cmd[1]) {
		case 'd': /* ndots */
			FULLCHECK("ndots");
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			num = strtonum(value, 0, MAXNDOTS, &errstr);
			if (errstr != NULL)
				fatal("ndots is %s: '%s'", errstr, value);
			ndots = num;
			break;
		case 's':
			switch (cmd[2]) {
			case 'i': /* nsid */
				FULLCHECK("nsid");
				if (state && lookup->edns == -1)
					lookup->edns = 0;
				lookup->nsid = state;
				break;
			case 's': /* nssearch */
				FULLCHECK("nssearch");
				lookup->ns_search_only = state;
				if (state) {
					lookup->trace_root = 1;
					lookup->recurse = 1;
					lookup->identify = 1;
					lookup->stats = 0;
					lookup->comments = 0;
					lookup->section_additional = 0;
					lookup->section_authority = 0;
					lookup->section_question = 0;
					lookup->rdtype = dns_rdatatype_ns;
					lookup->rdtypeset = 1;
					short_form = 1;
					rrcomments = 0;
				}
				break;
			default:
				goto invalid_option;
			}
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'o':
		switch (cmd[1]) {
		case 'n':
			FULLCHECK("onesoa");
			onesoa = state;
			break;
		case 'p':
			FULLCHECK("opcode");
			if (!state) {
				lookup->opcode = 0;	/* default - query */
				break;
			}
			if (value == NULL)
				goto need_value;
			for (num = 0;
			     num < sizeof(opcodetext)/sizeof(opcodetext[0]);
			     num++) {
				if (strcasecmp(opcodetext[num], value) == 0)
					break;
			}
			if (num < 16) {
				lookup->opcode = (dns_opcode_t)num;
				break;
			}
			num = strtonum(value, 0, 15, &errstr);
			if (errstr != NULL)
				fatal("opcode is %s: '%s'", errstr, value);
			lookup->opcode = (dns_opcode_t)num;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'q':
		switch (cmd[1]) {
		case 'r': /* qr */
			FULLCHECK("qr");
			qr = state;
			break;
		case 'u': /* question */
			FULLCHECK("question");
			lookup->section_question = state;
			if (lookup == default_lookup)
				plusquest = state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'r':
		switch (cmd[1]) {
		case 'd': /* rdflag */
			FULLCHECK("rdflag");
			lookup->recurse = state;
			break;
		case 'e':
			switch (cmd[2]) {
			case 'c': /* recurse */
				FULLCHECK("recurse");
				lookup->recurse = state;
				break;
			case 't': /* retry / retries */
				FULLCHECK2("retry", "retries");
				if (value == NULL)
					goto need_value;
				if (!state)
					goto invalid_option;
				lookup->retries = strtonum(value, 0,
				    MAXTRIES - 1, &errstr);
				if (errstr != NULL)
					fatal("retries is %s: '%s'", errstr,
					    value);
				lookup->retries++;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'r': /* rrcomments */
			FULLCHECK("rrcomments");
			rrcomments = state ? 1 : -1;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 's':
		switch (cmd[1]) {
		case 'e': /* search */
			FULLCHECK("search");
			if (!lookup->trace) {
				usesearch = state;
			}
			break;
		case 'h':
			if (cmd[2] != 'o')
				goto invalid_option;
			switch (cmd[3]) {
			case 'r': /* short */
				FULLCHECK("short");
				short_form = state;
				if (state) {
					printcmd = 0;
					lookup->section_additional = 0;
					lookup->section_answer = 1;
					lookup->section_authority = 0;
					lookup->section_question = 0;
					lookup->comments = 0;
					lookup->stats = 0;
					rrcomments = -1;
				}
				break;
			case 'w': /* showsearch */
				FULLCHECK("showsearch");
				if (!lookup->trace) {
					showsearch = state;
					usesearch = state;
				}
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'i':
			switch (cmd[2]) {
			case 't': /* sit */
				FULLCHECK("sit");
 sit:
				if (state && lookup->edns == -1)
					lookup->edns = 0;
				lookup->sit = state;
				if (value != NULL) {
					n = strlcpy(sitvalue, value,
						    sizeof(sitvalue));
					if (n >= sizeof(sitvalue))
						fatal("SIT data too large");
					lookup->sitvalue = sitvalue;
				} else
					lookup->sitvalue = NULL;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 'p': /* split */
			FULLCHECK("split");
			if (value != NULL && !state)
				goto invalid_option;
			if (!state) {
				splitwidth = 0;
				break;
			} else if (value == NULL)
				break;

			splitwidth = strtonum(value, 0, 1023, &errstr);
			if (errstr != NULL)
				fatal("split is %s: '%s'", errstr, value);
			if ((splitwidth % 4) != 0U) {
				splitwidth = ((splitwidth + 3) / 4) * 4;
				fprintf(stderr, ";; Warning, split must be "
						"a multiple of 4; adjusting "
						"to %u\n", splitwidth);
			}
			/*
			 * There is an adjustment done in the
			 * totext_<rrtype>() functions which causes
			 * splitwidth to shrink.  This is okay when we're
			 * using the default width but incorrect in this
			 * case, so we correct for it
			 */
			if (splitwidth)
				splitwidth += 3;
			break;
		case 't': /* stats */
			FULLCHECK("stats");
			lookup->stats = state;
			break;
		case 'u': /* subnet */
			FULLCHECK("subnet");
			if (state && value == NULL)
				goto need_value;
			if (!state) {
				if (lookup->ecs_addr != NULL) {
					free(lookup->ecs_addr);
					lookup->ecs_addr = NULL;
				}
				break;
			}
			if (lookup->edns == -1)
				lookup->edns = 0;
			if (lookup->ecs_addr != NULL) {
				free(lookup->ecs_addr);
				lookup->ecs_addr = NULL;
			}
			result = parse_netprefix(&lookup->ecs_addr,
			    &lookup->ecs_plen, value);
			if (result != ISC_R_SUCCESS)
				fatal("Couldn't parse client");
			break;
		default:
			goto invalid_option;
		}
		break;
	case 't':
		switch (cmd[1]) {
		case 'c': /* tcp */
			FULLCHECK("tcp");
			if (!is_batchfile) {
				lookup->tcp_mode = state;
				lookup->tcp_mode_set = 1;
			}
			break;
		case 'i': /* timeout */
			FULLCHECK("timeout");
			if (value == NULL)
				goto need_value;
			if (!state)
				goto invalid_option;
			timeout = strtonum(value, 0, MAXTIMEOUT, &errstr);
			if (errstr != NULL)
				fatal("timeout is %s: '%s'", errstr, value);
			if (timeout == 0)
				timeout = 1;
			break;
		case 'r':
			switch (cmd[2]) {
			case 'a': /* trace */
				FULLCHECK("trace");
				lookup->trace = state;
				lookup->trace_root = state;
				if (state) {
					lookup->recurse = 0;
					lookup->identify = 1;
					lookup->comments = 0;
					rrcomments = 0;
					lookup->stats = 0;
					lookup->section_additional = 0;
					lookup->section_authority = 1;
					lookup->section_question = 0;
					lookup->dnssec = 1;
					usesearch = 0;
				}
				break;
			case 'i': /* tries */
				FULLCHECK("tries");
				if (value == NULL)
					goto need_value;
				if (!state)
					goto invalid_option;
				lookup->retries = strtonum(value, 0, MAXTRIES,
				    &errstr);
				if (errstr != NULL)
					fatal("tries is %s: '%s'", errstr,
					    value);
				if (lookup->retries == 0)
					lookup->retries = 1;
				break;
			default:
				goto invalid_option;
			}
			break;
		case 't': /* ttlid */
			FULLCHECK("ttlid");
			nottl = !state;
			break;
		default:
			goto invalid_option;
		}
		break;
	case 'v':
		FULLCHECK("vc");
		if (!is_batchfile) {
			lookup->tcp_mode = state;
			lookup->tcp_mode_set = 1;
		}
		break;
	default:
	invalid_option:
	need_value:
		fprintf(stderr, "Invalid option: +%s\n",
			option);
		usage();
	}
	return;
}

/*%
 * #1 returned if value was used
 */
static const char *single_dash_opts = "46dhinuv";
static const char *dash_opts = "46bcdfhikmnptvyx";
static int
dash_option(char *option, char *next, dig_lookup_t **lookup,
	    int *open_type_class, int *need_clone,
	    int config_only, int argc, char **argv,
	    int *firstarg)
{
	char opt, *value, *ptr, *ptr2, *ptr3;
	isc_result_t result;
	int value_from_next;
	isc_textregion_t tr;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	char textname[MXNAME];
	char *cmd;
	uint32_t num;
	const char *errstr;

	while (strpbrk(option, single_dash_opts) == &option[0]) {
		/*
		 * Since the -[46dhinuv] options do not take an argument,
		 * account for them (in any number and/or combination)
		 * if they appear as the first character(s) of a q-opt.
		 */
		opt = option[0];
		switch (opt) {
		case '4':
			if (have_ipv4)
				have_ipv6 = 0;
			else
				fatal("can't find IPv4 networking");
			break;
		case '6':
			if (have_ipv6)
				have_ipv4 = 0;
			else
				fatal("can't find IPv6 networking");
			break;
		case 'd':
			ptr = strpbrk(&option[1], dash_opts);
			if (ptr != &option[1]) {
				cmd = option;
				FULLCHECK("debug");
				debugging = 1;
				return (0);
			} else
				debugging = 1;
			break;
		case 'h':
			help();
			exit(0);
			break;
		case 'i':
			ip6_int = 1;
			break;
		case 'n':
			/* deprecated */
			break;
		case 'u':
			use_usec = 1;
			break;
		case 'v':
			version();
			exit(0);
			break;
		}
		if (strlen(option) > 1U)
			option = &option[1];
		else
			return (0);
	}
	opt = option[0];
	if (strlen(option) > 1U) {
		value_from_next = 0;
		value = &option[1];
	} else {
		value_from_next = 1;
		value = next;
	}
	if (value == NULL)
		goto invalid_option;
	switch (opt) {
	case 'b': {
		struct addrinfo *ai = NULL, hints;
		int error;
		char *hash;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_socktype = SOCK_DGRAM;

		hash = strchr(value, '#');
		if (hash != NULL) {
			*hash = '\0';
			error = getaddrinfo(value, hash + 1, &hints, &ai);
			*hash = '#';
		} else
			error = getaddrinfo(value, NULL, &hints, &ai);

		if (error)
			fatal("invalid address %s: %s", value,
			    gai_strerror(error));
		if (ai == NULL || ai->ai_addrlen > sizeof(bind_address))
			fatal("invalid address %s", value);
		if (!have_ipv4 && ai->ai_family == AF_INET)
			fatal("%s: wrong address family", value);
		if (!have_ipv6 && ai->ai_family == AF_INET6)
			fatal("%s: wrong address family", value);

		memset(&bind_address, 0, sizeof(bind_address));
		memcpy(&bind_address, ai->ai_addr, ai->ai_addrlen);

		specified_source = 1;
		return (value_from_next);
	}
	case 'c':
		if ((*lookup)->rdclassset) {
			fprintf(stderr, ";; Warning, extra class option\n");
		}
		*open_type_class = 0;
		tr.base = value;
		tr.length = (unsigned int) strlen(value);
		result = dns_rdataclass_fromtext(&rdclass,
						 (isc_textregion_t *)&tr);
		if (result == ISC_R_SUCCESS) {
			(*lookup)->rdclass = rdclass;
			(*lookup)->rdclassset = 1;
		} else
			fprintf(stderr, ";; Warning, ignoring "
				"invalid class %s\n",
				value);
		return (value_from_next);
	case 'f':
		batchname = value;
		return (value_from_next);
	case 'k':
		strlcpy(keyfile, value, sizeof(keyfile));
		return (value_from_next);
	case 'p':
		num = strtonum(value, 0, MAXPORT, &errstr);
		if (errstr != NULL)
			fatal("port number is %s: '%s'", errstr, value);
		port = num;
		return (value_from_next);
	case 'q':
		if (!config_only) {
			if (*need_clone)
				(*lookup) = clone_lookup(default_lookup,
							 1);
			*need_clone = 1;
			strlcpy((*lookup)->textname, value,
				sizeof((*lookup)->textname));
			(*lookup)->trace_root = (*lookup)->trace  ||
						     (*lookup)->ns_search_only;
			(*lookup)->new_search = 1;
			if (*firstarg) {
				printgreeting(argc, argv, *lookup);
				*firstarg = 0;
			}
			ISC_LIST_APPEND(lookup_list, (*lookup), link);
			debug("looking up %s", (*lookup)->textname);
		}
		return (value_from_next);
	case 't':
		*open_type_class = 0;
		if (strncasecmp(value, "ixfr=", 5) == 0) {
			rdtype = dns_rdatatype_ixfr;
			result = ISC_R_SUCCESS;
		} else {
			tr.base = value;
			tr.length = (unsigned int) strlen(value);
			result = dns_rdatatype_fromtext(&rdtype,
						(isc_textregion_t *)&tr);
			if (result == ISC_R_SUCCESS &&
			    rdtype == dns_rdatatype_ixfr) {
				result = DNS_R_UNKNOWN;
			}
		}
		if (result == ISC_R_SUCCESS) {
			if ((*lookup)->rdtypeset) {
				fprintf(stderr, ";; Warning, "
						"extra type option\n");
			}
			if (rdtype == dns_rdatatype_ixfr) {
				uint32_t serial;
				(*lookup)->rdtype = dns_rdatatype_ixfr;
				(*lookup)->rdtypeset = 1;
				serial = strtonum(&value[5], 0, MAXSERIAL,
				    &errstr);
				if (errstr != NULL)
					fatal("serial number is %s: '%s'",
					    errstr, &value[5]);
				(*lookup)->ixfr_serial = serial;
				(*lookup)->section_question = plusquest;
				(*lookup)->comments = pluscomm;
				if (!(*lookup)->tcp_mode_set)
					(*lookup)->tcp_mode = 1;
			} else {
				(*lookup)->rdtype = rdtype;
				if (!config_only)
					(*lookup)->rdtypeset = 1;
				if (rdtype == dns_rdatatype_axfr) {
					(*lookup)->section_question = plusquest;
					(*lookup)->comments = pluscomm;
				}
				(*lookup)->ixfr_serial = 0;
			}
		} else
			fprintf(stderr, ";; Warning, ignoring "
				 "invalid type %s\n",
				 value);
		return (value_from_next);
	case 'y':
		ptr = next_token(&value, ":");	/* hmac type or name */
		if (ptr == NULL) {
			usage();
		}
		ptr2 = next_token(&value, ":");	/* name or secret */
		if (ptr2 == NULL)
			usage();
		ptr3 = next_token(&value, ":"); /* secret or NULL */
		if (ptr3 != NULL) {
			parse_hmac(ptr);
			ptr = ptr2;
			ptr2 = ptr3;
		} else  {
			hmacname = DNS_TSIG_HMACSHA256_NAME;
			digestbits = 0;
		}
		strlcpy(keynametext, ptr, sizeof(keynametext));
		strlcpy(keysecret, ptr2, sizeof(keysecret));
		return (value_from_next);
	case 'x':
		if (*need_clone)
			*lookup = clone_lookup(default_lookup, 1);
		*need_clone = 1;
		if (get_reverse(textname, sizeof(textname), value,
				ip6_int, 0) == ISC_R_SUCCESS) {
			strlcpy((*lookup)->textname, textname,
				sizeof((*lookup)->textname));
			debug("looking up %s", (*lookup)->textname);
			(*lookup)->trace_root = (*lookup)->trace  ||
						(*lookup)->ns_search_only;
			(*lookup)->ip6_int = ip6_int;
			if (!(*lookup)->rdtypeset)
				(*lookup)->rdtype = dns_rdatatype_ptr;
			if (!(*lookup)->rdclassset)
				(*lookup)->rdclass = dns_rdataclass_in;
			(*lookup)->new_search = 1;
			if (*firstarg) {
				printgreeting(argc, argv, *lookup);
				*firstarg = 0;
			}
			ISC_LIST_APPEND(lookup_list, *lookup, link);
		} else {
			fprintf(stderr, "Invalid IP address %s\n", value);
			exit(1);
		}
		return (value_from_next);
	invalid_option:
	default:
		fprintf(stderr, "Invalid option: -%s\n", option);
		usage();
	}
	/* NOTREACHED */
	return (0);
}

/*%
 * Because we may be trying to do memory allocation recording, we're going
 * to need to parse the arguments for the -m *before* we start the main
 * argument parsing routine.
 *
 * I'd prefer not to have to do this, but I am not quite sure how else to
 * fix the problem.  Argument parsing in dig involves memory allocation
 * by its nature, so it can't be done in the main argument parser.
 */
static void
preparse_args(int argc, char **argv) {
	int rc;
	char **rv;
	char *option;

	rc = argc;
	rv = argv;
	for (rc--, rv++; rc > 0; rc--, rv++) {
		if (rv[0][0] != '-')
			continue;
		option = &rv[0][1];
		while (strpbrk(option, single_dash_opts) == &option[0]) {
			switch (option[0]) {
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
			}
			option = &option[1];
		}
	}
}

static void
parse_args(int is_batchfile, int config_only,
	   int argc, char **argv)
{
	isc_result_t result;
	isc_textregion_t tr;
	int firstarg = 1;
	dig_lookup_t *lookup = NULL;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	int open_type_class = 1;
	char batchline[MXNAME];
	int bargc;
	char *bargv[64];
	int rc;
	char **rv;
	char *input;
	int i;
	int need_clone = 1;
	const char *errstr;

	/*
	 * The semantics for parsing the args is a bit complex; if
	 * we don't have a host yet, make the arg apply globally,
	 * otherwise make it apply to the latest host.  This is
	 * a bit different than the previous versions, but should
	 * form a consistent user interface.
	 *
	 * First, create a "default lookup" which won't actually be used
	 * anywhere, except for cloning into new lookups
	 */

	debug("parse_args()");
	if (!is_batchfile) {
		debug("making new lookup");
		default_lookup = make_empty_lookup();
		default_lookup->adflag = 1;
		default_lookup->edns = 0;
	}

	if (is_batchfile && !config_only) {
		/* Processing '-f batchfile'. */
		lookup = clone_lookup(default_lookup, 1);
		need_clone = 0;
	} else
		lookup = default_lookup;

	rc = argc;
	rv = argv;
	for (rc--, rv++; rc > 0; rc--, rv++) {
		debug("main parsing %s", rv[0]);
		if (strncmp(rv[0], "%", 1) == 0)
			break;
		if (rv[0][0] == '@') {

			if (is_batchfile && !config_only) {
				addresscount = getaddresses(lookup, &rv[0][1],
							     &result);
				if (result != ISC_R_SUCCESS) {
					fprintf(stderr, "couldn't get address "
						"for '%s': %s: skipping "
						"lookup\n", &rv[0][1],
						isc_result_totext(result));
					if (ISC_LINK_LINKED(lookup, link))
						ISC_LIST_DEQUEUE(lookup_list,
								 lookup, link);
					destroy_lookup(lookup);
					return;
				}
			} else
				addresscount = getaddresses(lookup, &rv[0][1],
							    NULL);
		} else if (rv[0][0] == '+') {
			plus_option(&rv[0][1], is_batchfile,
				    lookup);
		} else if (rv[0][0] == '-') {
			if (rc <= 1) {
				if (dash_option(&rv[0][1], NULL,
						&lookup, &open_type_class,
						&need_clone, config_only,
						argc, argv, &firstarg)) {
					rc--;
					rv++;
				}
			} else {
				if (dash_option(&rv[0][1], rv[1],
						&lookup, &open_type_class,
						&need_clone, config_only,
						argc, argv, &firstarg)) {
					rc--;
					rv++;
				}
			}
		} else {
			/*
			 * Anything which isn't an option
			 */
			if (open_type_class) {
				if (strncasecmp(rv[0], "ixfr=", 5) == 0) {
					rdtype = dns_rdatatype_ixfr;
					result = ISC_R_SUCCESS;
				} else {
					tr.base = rv[0];
					tr.length =
						(unsigned int) strlen(rv[0]);
					result = dns_rdatatype_fromtext(&rdtype,
						(isc_textregion_t *)&tr);
					if (result == ISC_R_SUCCESS &&
					    rdtype == dns_rdatatype_ixfr) {
						fprintf(stderr, ";; Warning, "
							"ixfr requires a "
							"serial number\n");
						continue;
					}
				}
				if (result == ISC_R_SUCCESS) {
					if (lookup->rdtypeset) {
						fprintf(stderr, ";; Warning, "
							"extra type option\n");
					}
					if (rdtype == dns_rdatatype_ixfr) {
						uint32_t serial;
						lookup->rdtype =
							dns_rdatatype_ixfr;
						lookup->rdtypeset = 1;
						serial = strtonum(&rv[0][5], 0,
						    MAXSERIAL, &errstr);
						if (errstr != NULL)
							fatal("serial number "
							    "is %s: '%s'",
							    errstr, &rv[0][5]);
						lookup->ixfr_serial = serial;
						lookup->section_question =
							plusquest;
						lookup->comments = pluscomm;
						if (!lookup->tcp_mode_set)
							lookup->tcp_mode = 1;
					} else {
						lookup->rdtype = rdtype;
						lookup->rdtypeset = 1;
						if (rdtype ==
						    dns_rdatatype_axfr) {
						    lookup->section_question =
								plusquest;
						    lookup->comments = pluscomm;
						}
						lookup->ixfr_serial = 0;
					}
					continue;
				}
				result = dns_rdataclass_fromtext(&rdclass,
						     (isc_textregion_t *)&tr);
				if (result == ISC_R_SUCCESS) {
					if (lookup->rdclassset) {
						fprintf(stderr, ";; Warning, "
							"extra class option\n");
					}
					lookup->rdclass = rdclass;
					lookup->rdclassset = 1;
					continue;
				}
			}

			if (!config_only) {
				if (need_clone)
					lookup = clone_lookup(default_lookup,
								      1);
				need_clone = 1;
				strlcpy(lookup->textname, rv[0],
					sizeof(lookup->textname));
				lookup->trace_root = lookup->trace  ||
						     lookup->ns_search_only;
				lookup->new_search = 1;
				if (firstarg) {
					printgreeting(argc, argv, lookup);
					firstarg = 0;
				}
				ISC_LIST_APPEND(lookup_list, lookup, link);
				debug("looking up %s", lookup->textname);
			}
			/* XXX Error message */
		}
	}

	/*
	 * If we have a batchfile, seed the lookup list with the
	 * first entry, then trust the callback in dighost_shutdown
	 * to get the rest
	 */
	if ((batchname != NULL) && !(is_batchfile)) {
		if (strcmp(batchname, "-") == 0)
			batchfp = stdin;
		else
			batchfp = fopen(batchname, "r");
		if (batchfp == NULL) {
			perror(batchname);
			if (exitcode < 8)
				exitcode = 8;
			fatal("couldn't open specified batch file");
		}
		/* XXX Remove code dup from shutdown code */
	next_line:
		if (fgets(batchline, sizeof(batchline), batchfp) != NULL) {
			bargc = 1;
			debug("batch line %s", batchline);
			if (batchline[0] == '\r' || batchline[0] == '\n'
			    || batchline[0] == '#' || batchline[0] == ';')
				goto next_line;
			input = batchline;
			bargv[bargc] = next_token(&input, " \t\r\n");
			while ((bargc < 14) && (bargv[bargc] != NULL)) {
				bargc++;
				bargv[bargc] = next_token(&input, " \t\r\n");
			}

			bargv[0] = argv[0];
			argv0 = argv[0];

			for(i = 0; i < bargc; i++)
				debug("batch argv %d: %s", i, bargv[i]);
			parse_args(1, 0, bargc, (char **)bargv);
			return;
		}
		return;
	}
	/*
	 * If no lookup specified, search for root
	 */
	if ((lookup_list.head == NULL) && !config_only) {
		if (need_clone)
			lookup = clone_lookup(default_lookup, 1);
		need_clone = 1;
		lookup->trace_root = lookup->trace || lookup->ns_search_only;
		lookup->new_search = 1;
		strlcpy(lookup->textname, ".", sizeof(lookup->textname));
		lookup->rdtype = dns_rdatatype_ns;
		lookup->rdtypeset = 1;
		if (firstarg) {
			printgreeting(argc, argv, lookup);
			firstarg = 0;
		}
		ISC_LIST_APPEND(lookup_list, lookup, link);
	}
	if (!need_clone)
		destroy_lookup(lookup);
}

/*
 * Callback from dighost.c to allow program-specific shutdown code.
 * Here, we're possibly reading from a batch file, then shutting down
 * for real if there's nothing in the batch file to read.
 */
static void
query_finished(void) {
	char batchline[MXNAME];
	int bargc;
	char *bargv[16];
	char *input;
	int i;

	if (batchname == NULL) {
		isc_app_shutdown();
		return;
	}

	fflush(stdout);
	if (feof(batchfp)) {
		batchname = NULL;
		isc_app_shutdown();
		if (batchfp != stdin)
			fclose(batchfp);
		return;
	}

	if (fgets(batchline, sizeof(batchline), batchfp) != NULL) {
		debug("batch line %s", batchline);
		bargc = 1;
		input = batchline;
		bargv[bargc] = next_token(&input, " \t\r\n");
		while ((bargc < 14) && (bargv[bargc] != NULL)) {
			bargc++;
			bargv[bargc] = next_token(&input, " \t\r\n");
		}

		bargv[0] = argv0;

		for(i = 0; i < bargc; i++)
			debug("batch argv %d: %s", i, bargv[i]);
		parse_args(1, 0, bargc, (char **)bargv);
		start_lookup();
	} else {
		batchname = NULL;
		if (batchfp != stdin)
			fclose(batchfp);
		isc_app_shutdown();
		return;
	}
}

void dig_setup(int argc, char **argv)
{
	isc_result_t result;

	ISC_LIST_INIT(lookup_list);
	ISC_LIST_INIT(server_list);
	ISC_LIST_INIT(root_hints_server_list);
	ISC_LIST_INIT(search_list);

	if (pledge("stdio rpath inet dns", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	debug("dig_setup()");

	/* setup dighost callbacks */
	dighost_printmessage = printmessage;
	dighost_received = received;
	dighost_trying = trying;
	dighost_shutdown = query_finished;

	progname = argv[0];
	preparse_args(argc, argv);

	result = isc_app_start();
	check_result(result, "isc_app_start");

	setup_libs();
	setup_system(ipv4only, ipv6only);
}

void dig_query_setup(int is_batchfile, int config_only,
		int argc, char **argv)
{
	debug("dig_query_setup");

	parse_args(is_batchfile, config_only, argc, argv);
	if (keyfile[0] != 0)
		setup_file_key();
	else if (keysecret[0] != 0)
		setup_text_key();

	if (pledge("stdio inet dns", NULL) == -1) {
		perror("pledge");
		exit(1);
	}

	if (domainopt[0] != '\0') {
		set_search_domain(domainopt);
		usesearch = 1;
	}
}

void dig_startup(void) {
	isc_result_t result;

	debug("dig_startup()");

	result = isc_app_onrun(global_task, onrun_callback, NULL);
	check_result(result, "isc_app_onrun");
	isc_app_run();
}

void
dig_shutdown(void) {
	destroy_lookup(default_lookup);
	if (batchname != NULL) {
		if (batchfp != stdin)
			fclose(batchfp);
		batchname = NULL;
	}

	cancel_all();
	destroy_libs();
}

/*% Main processing routine for dig */
int
main(int argc, char **argv) {
	extern char *__progname;

	if (strcmp("host", __progname) == 0)
		return host_main(argc, argv);
	if (strcmp("nslookup", __progname) == 0)
		return nslookup_main(argc, argv);

	dig_setup(argc, argv);
	dig_query_setup(0, 0, argc, argv);
	dig_startup();
	dig_shutdown();

	return (exitcode);
}
