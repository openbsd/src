/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $ISC: notify.c,v 1.24 2001/03/31 01:03:26 bwelling Exp $ */

#include <config.h>

#include <isc/log.h>

#include <dns/message.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <named/log.h>
#include <named/notify.h>

/*
 * This module implements notify as in RFC 1996.
 */

static void
notify_log(int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	isc_log_vwrite(ns_g_lctx, DNS_LOGCATEGORY_NOTIFY, NS_LOGMODULE_NOTIFY,
		       level, fmt, ap);
	va_end(ap);
}

static void
respond(ns_client_t *client, isc_result_t result) {
	dns_rcode_t rcode;
	dns_message_t *message;
	isc_result_t msg_result;

	message = client->message;
	rcode = dns_result_torcode(result);

	msg_result = dns_message_reply(message, ISC_TRUE);
	if (msg_result != ISC_R_SUCCESS)
		msg_result = dns_message_reply(message, ISC_FALSE);
	if (msg_result != ISC_R_SUCCESS) {
		ns_client_next(client, msg_result);
		return;
	}
	message->rcode = rcode;
	if (rcode == dns_rcode_noerror)
		message->flags |= DNS_MESSAGEFLAG_AA;
	else
		message->flags &= ~DNS_MESSAGEFLAG_AA;
	ns_client_send(client);
}

void
ns_notify_start(ns_client_t *client) {
	dns_message_t *request = client->message;
	isc_result_t result;
	dns_name_t *zonename;
	dns_rdataset_t *zone_rdataset;
	dns_zone_t *zone = NULL;
	char str[DNS_NAME_FORMATSIZE];

	/*
	 * Interpret the question section.
	 */
	result = dns_message_firstname(request, DNS_SECTION_QUESTION);
	if (result != ISC_R_SUCCESS) {
		notify_log(ISC_LOG_INFO, "notify question section empty");
		goto failure;
	}

	/*
	 * The question section must contain exactly one question.
	 */
	zonename = NULL;
	dns_message_currentname(request, DNS_SECTION_QUESTION, &zonename);
	zone_rdataset = ISC_LIST_HEAD(zonename->list);
	if (ISC_LIST_NEXT(zone_rdataset, link) != NULL) {
		notify_log(ISC_LOG_INFO,
			   "notify question section contains multiple RRs");
		goto failure;
	}

	/* The zone section must have exactly one name. */
	result = dns_message_nextname(request, DNS_SECTION_ZONE);
	if (result != ISC_R_NOMORE) {
		notify_log(ISC_LOG_INFO,
			   "notify question section contains multiple RRs");
		goto failure;
	}

	/* The one rdataset must be an SOA. */
	if (zone_rdataset->type != dns_rdatatype_soa) {
		notify_log(ISC_LOG_INFO,
			   "notify question section contains no SOA");
		goto failure;
	}

	result = dns_zt_find(client->view->zonetable, zonename, 0, NULL,
			     &zone);
	if (result != ISC_R_SUCCESS) {
		dns_name_format(zonename, str, sizeof(str));
		notify_log(ISC_LOG_INFO,
			   "received notify for zone '%s': not authoritative",
			   str);
		goto failure;
	}

	switch(dns_zone_gettype(zone)) {
	case dns_zone_master:
	case dns_zone_slave:
	case dns_zone_stub:	/* Allow dialup passive to work. */
		respond(client, dns_zone_notifyreceive(zone,
			ns_client_getsockaddr(client), request));
		break;
	default:
		dns_name_format(zonename, str, sizeof(str));
		notify_log(ISC_LOG_INFO,
			   "received notify for zone '%s': not authoritative",
			   str);
		goto failure;
	}
	dns_zone_detach(&zone);
	return;

 failure:
	if (zone != NULL)
		dns_zone_detach(&zone);
	respond(client, result);
}
