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

/* $ISC: sig0_test.c,v 1.9 2001/03/28 02:43:44 bwelling Exp $ */

#include <config.h>

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/boolean.h>
#include <isc/assertions.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/error.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/net.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/socket.h>
#include <isc/util.h>

#include <dns/dnssec.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdataset.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/types.h>

#include <dst/result.h>
#include <dst/dst.h>

#define CHECK(str, x) { \
	if ((x) != ISC_R_SUCCESS) { \
		printf("%s: %s\n", (str), isc_result_totext(x)); \
		exit(-1); \
	} \
}

isc_mutex_t lock;
dst_key_t *key;
isc_mem_t *mctx;
unsigned char qdata[1024], rdata[1024];
isc_buffer_t qbuffer, rbuffer;
isc_taskmgr_t *taskmgr;
isc_entropy_t *ent = NULL;
isc_task_t *task1;
isc_log_t *log = NULL;
isc_logconfig_t *logconfig = NULL;
isc_socket_t *s;
isc_sockaddr_t address;
char output[10 * 1024];
isc_buffer_t outbuf;
static const dns_master_style_t *style = &dns_master_style_debug;

static void
senddone(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent = (isc_socketevent_t *)event;

	REQUIRE(sevent != NULL);
	REQUIRE(sevent->ev_type == ISC_SOCKEVENT_SENDDONE);
	REQUIRE(task == task1);

	printf("senddone\n");

	isc_event_free(&event);
}

static void
recvdone(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent = (isc_socketevent_t *)event;
	isc_buffer_t source;
	isc_result_t result;
	dns_message_t *response;

	REQUIRE(sevent != NULL);
	REQUIRE(sevent->ev_type == ISC_SOCKEVENT_RECVDONE);
	REQUIRE(task == task1);

	printf("recvdone\n");
	if (sevent->result != ISC_R_SUCCESS) {
		printf("failed\n");
		exit(-1);
	}

	isc_buffer_init(&source, sevent->region.base, sevent->region.length);
	isc_buffer_add(&source, sevent->n);

	response = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &response);
	CHECK("dns_message_create", result);
	result = dns_message_parse(response, &source, 0);
	CHECK("dns_message_parse", result);

	isc_buffer_init(&outbuf, output, sizeof(output));
	result = dns_message_totext(response, style, 0, &outbuf);
	CHECK("dns_message_totext", result);
	printf("%.*s\n", (int)isc_buffer_usedlength(&outbuf),
	       (char *)isc_buffer_base(&outbuf));

	dns_message_destroy(&response);
	isc_event_free(&event);

	isc_app_shutdown();
}

static void
buildquery(void) {
	isc_result_t result;
	dns_rdataset_t *question = NULL;
	dns_name_t *qname = NULL;
	isc_region_t r, inr;
	dns_message_t *query;
	char nametext[] = "host.example";
	isc_buffer_t namesrc, namedst;
	unsigned char namedata[256];
	isc_sockaddr_t sa;
	dns_compress_t cctx;

	query = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &query);
	CHECK("dns_message_create", result);
	result = dns_message_setsig0key(query, key);
	CHECK("dns_message_setsig0key", result);

	result = dns_message_gettemprdataset(query, &question);
	CHECK("dns_message_gettemprdataset", result);
	dns_rdataset_init(question);
	dns_rdataset_makequestion(question, dns_rdataclass_in,
				  dns_rdatatype_a);
	result = dns_message_gettempname(query, &qname);
	CHECK("dns_message_gettempname", result);
	isc_buffer_init(&namesrc, nametext, strlen(nametext));
	isc_buffer_add(&namesrc, strlen(nametext));
	isc_buffer_init(&namedst, namedata, sizeof namedata);
	dns_name_init(qname, NULL);
	result = dns_name_fromtext(qname, &namesrc, dns_rootname, ISC_FALSE,
				   &namedst);
	CHECK("dns_name_fromtext", result);
	ISC_LIST_APPEND(qname->list, question, link);
	dns_message_addname(query, qname, DNS_SECTION_QUESTION);

	isc_buffer_init(&qbuffer, qdata, sizeof(qdata));

	result = dns_compress_init(&cctx, -1, mctx);
	CHECK("dns_compress_init", result);
	result = dns_message_renderbegin(query, &cctx, &qbuffer);
	CHECK("dns_message_renderbegin", result);
	result = dns_message_rendersection(query, DNS_SECTION_QUESTION, 0);
	CHECK("dns_message_rendersection(question)", result);
	result = dns_message_rendersection(query, DNS_SECTION_ANSWER, 0);
	CHECK("dns_message_rendersection(answer)", result);
	result = dns_message_rendersection(query, DNS_SECTION_AUTHORITY, 0);
	CHECK("dns_message_rendersection(auth)", result);
	result = dns_message_rendersection(query, DNS_SECTION_ADDITIONAL, 0);
	CHECK("dns_message_rendersection(add)", result);
	result = dns_message_renderend(query);
	CHECK("dns_message_renderend", result);
	dns_compress_invalidate(&cctx);

	isc_buffer_init(&outbuf, output, sizeof(output));
	result = dns_message_totext(query, style, 0, &outbuf);
	CHECK("dns_message_totext", result);
	printf("%.*s\n", (int)isc_buffer_usedlength(&outbuf),
	       (char *)isc_buffer_base(&outbuf));

	isc_buffer_usedregion(&qbuffer, &r);
	isc_sockaddr_any(&sa);
	result = isc_socket_bind(s, &sa);
	CHECK("isc_socket_bind", result);
	result = isc_socket_sendto(s, &r, task1, senddone, NULL, &address,
				   NULL);
	CHECK("isc_socket_sendto", result);

	inr.base = rdata;
	inr.length = sizeof(rdata);
	result = isc_socket_recv(s, &inr, 1, task1, recvdone, NULL);
	CHECK("isc_socket_recv", result);
	dns_message_destroy(&query);
}

int
main(int argc, char *argv[]) {
	isc_boolean_t verbose = ISC_FALSE;
	isc_socketmgr_t *socketmgr;
	isc_timermgr_t *timermgr;
	struct in_addr inaddr;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_buffer_t b;
	int ch;
	isc_result_t result;
	in_port_t port = 53;

	RUNTIME_CHECK(isc_app_start() == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_mutex_init(&lock) == ISC_R_SUCCESS);

	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	while ((ch = isc_commandline_parse(argc, argv, "vp:")) != -1) {
		switch (ch) {
		case 'v':
			verbose = ISC_TRUE;
			break;
		case 'p':
			port = (unsigned int)atoi(isc_commandline_argument);
			break;
		}
	}

	RUNTIME_CHECK(isc_entropy_create(mctx, &ent) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dst_lib_init(mctx, ent, 0) == ISC_R_SUCCESS);

	dns_result_register();
	dst_result_register();

	taskmgr = NULL;
	RUNTIME_CHECK(isc_taskmgr_create(mctx, 2, 0, &taskmgr) ==
		      ISC_R_SUCCESS);
	task1 = NULL;
	RUNTIME_CHECK(isc_task_create(taskmgr, 0, &task1) == ISC_R_SUCCESS);

	timermgr = NULL;
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timermgr) == ISC_R_SUCCESS);
	socketmgr = NULL;
	RUNTIME_CHECK(isc_socketmgr_create(mctx, &socketmgr) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_log_create(mctx, &log, &logconfig) == ISC_R_SUCCESS);

	s = NULL;
	RUNTIME_CHECK(isc_socket_create(socketmgr, PF_INET,
					isc_sockettype_udp, &s) ==
		      ISC_R_SUCCESS);

	inaddr.s_addr = htonl(INADDR_LOOPBACK);
	isc_sockaddr_fromin(&address, &inaddr, port);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	isc_buffer_init(&b, "child.example.", strlen("child.example."));
	isc_buffer_add(&b, strlen("child.example."));
	result = dns_name_fromtext(name, &b, dns_rootname, ISC_FALSE, NULL);
	CHECK("dns_name_fromtext", result);

	key = NULL;
	result = dst_key_fromfile(name, 4017, DNS_KEYALG_DSA,
				  DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				  NULL, mctx, &key);
	CHECK("dst_key_fromfile", result);

	buildquery();

	(void)isc_app_run();

	isc_task_shutdown(task1);
	isc_task_detach(&task1);
	isc_taskmgr_destroy(&taskmgr);

	isc_socket_detach(&s);
	isc_socketmgr_destroy(&socketmgr);
	isc_timermgr_destroy(&timermgr);

	dst_key_free(&key);

	dst_lib_destroy();

	isc_entropy_detach(&ent);

	isc_log_destroy(&log);

	if (verbose)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	DESTROYLOCK(&lock);

	isc_app_finish();

	return (0);
}
