/*
 * Copyright (C) 1998-2002  Internet Software Consortium.
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

/* $ISC: printmsg.c,v 1.25.2.2 2002/02/08 03:57:23 marka Exp $ */

#include <config.h>

#include <isc/buffer.h>
#include <isc/util.h>

#include <dns/name.h>
#include <dns/rdataset.h>

#include "printmsg.h"

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

static isc_result_t
printsection(dns_message_t *msg, dns_section_t sectionid,
	     const char *section_name)
{
	dns_name_t *name, *print_name;
	dns_rdataset_t *rdataset;
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	dns_name_t empty_name;
	char t[65536];
	isc_boolean_t first;
	isc_boolean_t no_rdata;

	if (sectionid == DNS_SECTION_QUESTION)
		no_rdata = ISC_TRUE;
	else
		no_rdata = ISC_FALSE;

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

		isc_buffer_init(&target, t, sizeof(t));
		first = ISC_TRUE;
		print_name = name;

		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			result = dns_rdataset_totext(rdataset,
						     print_name,
						     ISC_FALSE,
						     no_rdata,
						     &target);
			if (result != ISC_R_SUCCESS)
				return (result);
#ifdef USEINITALWS
			if (first) {
				print_name = &empty_name;
				first = ISC_FALSE;
			}
#endif
		}
		isc_buffer_usedregion(&target, &r);
		printf("%.*s", (int)r.length, (char *)r.base);

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
	   const char *set_name)
{
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char t[65536];

	UNUSED(msg);
	printf(";; %s SECTION:\n", set_name);

	isc_buffer_init(&target, t, sizeof(t));

	result = dns_rdataset_totext(rdataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

isc_result_t
printmessage(dns_message_t *msg) {
	isc_boolean_t did_flag = ISC_FALSE;
	isc_result_t result;
	dns_rdataset_t *opt, *tsig;
	dns_name_t *tsigname;

	result = ISC_R_SUCCESS;

	printf(";; ->>HEADER<<- opcode: %s, status: %s, id: %u\n",
	       opcodetext[msg->opcode], rcodetext[msg->rcode], msg->id);

	printf(";; flags: ");
	if ((msg->flags & DNS_MESSAGEFLAG_QR) != 0) {
		printf("qr");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_AA) != 0) {
		printf("%saa", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
		printf("%stc", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_RD) != 0) {
		printf("%srd", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_RA) != 0) {
		printf("%sra", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_AD) != 0) {
		printf("%sad", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	if ((msg->flags & DNS_MESSAGEFLAG_CD) != 0) {
		printf("%scd", did_flag ? " " : "");
		did_flag = ISC_TRUE;
	}
	printf("; QUERY: %u, ANSWER: %u, AUTHORITY: %u, ADDITIONAL: %u\n",
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
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_QUESTION])) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_QUESTION, "QUESTION");
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ANSWER])) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_ANSWER, "ANSWER");
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_AUTHORITY])) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_AUTHORITY, "AUTHORITY");
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (! ISC_LIST_EMPTY(msg->sections[DNS_SECTION_ADDITIONAL])) {
		printf("\n");
		result = printsection(msg, DNS_SECTION_ADDITIONAL,
				      "ADDITIONAL");
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (tsig != NULL) {
		printf("\n");
		result = printrdata(msg, tsig, tsigname,
				    "PSEUDOSECTION TSIG");
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	printf("\n");

	return (result);
}
