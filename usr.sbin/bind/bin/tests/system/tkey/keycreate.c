/*
 * Copyright (C) 2001  Internet Software Consortium.
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

/* $ISC: keycreate.c,v 1.7 2001/04/16 17:23:34 gson Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/entropy.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/sockaddr.h>
#include <isc/socket.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/dispatch.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/request.h>
#include <dns/result.h>
#include <dns/tkey.h>
#include <dns/tsig.h>
#include <dns/view.h>

#include <dst/result.h>

#define CHECK(str, x) { \
	if ((x) != ISC_R_SUCCESS) { \
		fprintf(stderr, "I:%s: %s\n", (str), isc_result_totext(x)); \
		exit(-1); \
	} \
}

#define RUNCHECK(x) RUNTIME_CHECK((x) == ISC_R_SUCCESS)

#define PORT 5300
#define TIMEOUT 30

static dst_key_t *ourkey;
static isc_mem_t *mctx;
static dns_tsigkey_t *tsigkey, *initialkey;
static dns_tsig_keyring_t *ring;
static unsigned char noncedata[16];
static isc_buffer_t nonce;
static dns_requestmgr_t *requestmgr;

static void
recvquery(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *reqev = (dns_requestevent_t *)event;
	isc_result_t result;
	dns_message_t *query, *response;
	char keyname[256];
	isc_buffer_t keynamebuf;

	UNUSED(task);

	REQUIRE(reqev != NULL);

	if (reqev->result != ISC_R_SUCCESS) {
		fprintf(stderr, "I:request event result: %s\n",
			isc_result_totext(reqev->result));
		exit(-1);
	}

	query = reqev->ev_arg;

	response = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE, &response);
	CHECK("dns_message_create", result);

	result = dns_request_getresponse(reqev->request, response,
					 DNS_MESSAGEPARSE_PRESERVEORDER);
	CHECK("dns_request_getresponse", result);

	if (response->rcode != dns_rcode_noerror) {
		result = ISC_RESULTCLASS_DNSRCODE + response->rcode;
		fprintf(stderr, "I:response rcode: %s\n",
			isc_result_totext(result));
			exit(-1);
	}

	result = dns_tkey_processdhresponse(query, response, ourkey, &nonce,
					    &tsigkey, ring);
	CHECK("dns_tkey_processdhresponse", result);

	/*
	 * Yes, this is a hack.
	 */
	isc_buffer_init(&keynamebuf, keyname, sizeof(keyname));
	result = dst_key_buildfilename(tsigkey->key, 0, "", &keynamebuf);
	CHECK("dst_key_buildfilename", result);
	printf("%.*s\n", (int)isc_buffer_usedlength(&keynamebuf),
	       (char *)isc_buffer_base(&keynamebuf));
	result = dst_key_tofile(tsigkey->key,
				DST_TYPE_PRIVATE | DST_TYPE_PUBLIC, "");
	CHECK("dst_key_tofile", result);

	dns_message_destroy(&query);
	dns_message_destroy(&response);
	dns_request_destroy(&reqev->request);
	isc_event_free(&event);
	isc_app_shutdown();
	return;
}

static void
sendquery(isc_task_t *task, isc_event_t *event) {
	struct in_addr inaddr;
	isc_sockaddr_t address;
	isc_region_t r;
	isc_result_t result;
	dns_fixedname_t keyname;
	isc_buffer_t namestr, keybuf;
	unsigned char keydata[9];
	dns_message_t *query;
	dns_request_t *request;
	static char keystr[] = "0123456789ab";

	isc_event_free(&event);

	inet_pton(AF_INET, "10.53.0.1", &inaddr);
	isc_sockaddr_fromin(&address, &inaddr, PORT);

	dns_fixedname_init(&keyname);
	isc_buffer_init(&namestr, "tkeytest.", 9);
	isc_buffer_add(&namestr, 9);
	result = dns_name_fromtext(dns_fixedname_name(&keyname), &namestr,
				   NULL, ISC_FALSE, NULL);
	CHECK("dns_name_fromtext", result);

	isc_buffer_init(&keybuf, keydata, 9);
	result = isc_base64_decodestring(keystr, &keybuf);
	CHECK("isc_base64_decodestring", result);

	isc_buffer_usedregion(&keybuf, &r);

	initialkey = NULL;
	result = dns_tsigkey_create(dns_fixedname_name(&keyname),
				    DNS_TSIG_HMACMD5_NAME,
				    isc_buffer_base(&keybuf),
				    isc_buffer_usedlength(&keybuf),
				    ISC_FALSE, NULL, 0, 0, mctx, ring,
				    &initialkey);
	CHECK("dns_tsigkey_create", result);

	query = NULL;
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &query);
	CHECK("dns_message_create", result);

	result = dns_tkey_builddhquery(query, ourkey, dns_rootname,
				       DNS_TSIG_HMACMD5_NAME, &nonce, 3600);
	CHECK("dns_tkey_builddhquery", result);

	request = NULL;
	result = dns_request_create(requestmgr, query, &address,
				    0, initialkey, TIMEOUT, task,
				    recvquery, query, &request);
	CHECK("dns_request_create", result);
}

int
main(int argc, char *argv[]) {
	char *ourkeyname;
	isc_taskmgr_t *taskmgr;
	isc_timermgr_t *timermgr;
	isc_socketmgr_t *socketmgr;
	isc_socket_t *sock;
	unsigned int attrs, attrmask;
	isc_sockaddr_t bind_any;
	dns_dispatchmgr_t *dispatchmgr;
	dns_dispatch_t *dispatchv4;
	dns_view_t *view;
	isc_entropy_t *ectx;
	dns_tkeyctx_t *tctx;
	isc_log_t *log;
	isc_logconfig_t *logconfig;
	isc_task_t *task;
	isc_result_t result;

	RUNCHECK(isc_app_start());

	if (argc < 2) {
		fprintf(stderr, "I:no DH key provided\n");
		exit(-1);
	}
	ourkeyname = argv[1];

	dns_result_register();

	mctx = NULL;
	RUNCHECK(isc_mem_create(0, 0, &mctx));

	ectx = NULL;
	RUNCHECK(isc_entropy_create(mctx, &ectx));
	RUNCHECK(isc_entropy_createfilesource(ectx, "random.data"));

	log = NULL;
	logconfig = NULL;
	RUNCHECK(isc_log_create(mctx, &log, &logconfig));

	RUNCHECK(dst_lib_init(mctx, ectx, ISC_ENTROPY_GOODONLY));

	taskmgr = NULL;
	RUNCHECK(isc_taskmgr_create(mctx, 1, 0, &taskmgr));
	task = NULL;
	RUNCHECK(isc_task_create(taskmgr, 0, &task));
	timermgr = NULL;
	RUNCHECK(isc_timermgr_create(mctx, &timermgr));
	socketmgr = NULL;
	RUNCHECK(isc_socketmgr_create(mctx, &socketmgr));
	dispatchmgr = NULL;
	RUNCHECK(dns_dispatchmgr_create(mctx, NULL, &dispatchmgr));
	isc_sockaddr_any(&bind_any);
	attrs = DNS_DISPATCHATTR_UDP |
		DNS_DISPATCHATTR_MAKEQUERY |
		DNS_DISPATCHATTR_IPV4;
	attrmask = DNS_DISPATCHATTR_UDP |
		   DNS_DISPATCHATTR_TCP |
		   DNS_DISPATCHATTR_IPV4 |
		   DNS_DISPATCHATTR_IPV6;
	dispatchv4 = NULL;
	RUNCHECK(dns_dispatch_getudp(dispatchmgr, socketmgr, taskmgr,
					  &bind_any, 4096, 4, 2, 3, 5,
					  attrs, attrmask, &dispatchv4));
	requestmgr = NULL;
	RUNCHECK(dns_requestmgr_create(mctx, timermgr, socketmgr, taskmgr,
					    dispatchmgr, dispatchv4, NULL,
					    &requestmgr));

	ring = NULL;
	RUNCHECK(dns_tsigkeyring_create(mctx, &ring));
	tctx = NULL;
	RUNCHECK(dns_tkeyctx_create(mctx, ectx, &tctx));

	view = NULL;
	RUNCHECK(dns_view_create(mctx, 0, "_test", &view));
	dns_view_setkeyring(view, ring);

	sock = NULL;
	RUNCHECK(isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp,
				   &sock));

	RUNCHECK(isc_app_onrun(mctx, task, sendquery, NULL));

	ourkey = NULL;
	result = dst_key_fromnamedfile(ourkeyname, 
				       DST_TYPE_PUBLIC | DST_TYPE_PRIVATE,
				       mctx, &ourkey);
	CHECK("dst_key_fromnamedfile", result);

	isc_buffer_init(&nonce, noncedata, sizeof(noncedata));
	result = isc_entropy_getdata(ectx, noncedata, sizeof(noncedata),
				     NULL, ISC_ENTROPY_BLOCKING);
	CHECK("isc_entropy_getdata", result);
	isc_buffer_add(&nonce, sizeof(noncedata));

	(void)isc_app_run();

	dns_requestmgr_shutdown(requestmgr);
	dns_requestmgr_detach(&requestmgr);
	dns_dispatch_detach(&dispatchv4);
	dns_dispatchmgr_destroy(&dispatchmgr);
	isc_task_shutdown(task);
	isc_task_detach(&task);
	isc_taskmgr_destroy(&taskmgr);
	isc_socket_detach(&sock);
	isc_socketmgr_destroy(&socketmgr);
	isc_timermgr_destroy(&timermgr);

	dst_key_free(&ourkey);
	dns_tsigkey_detach(&initialkey);
	dns_tsigkey_detach(&tsigkey);

	dns_tkeyctx_destroy(&tctx);

	dns_view_detach(&view);

	isc_log_destroy(&log);

	dst_lib_destroy();
	isc_entropy_detach(&ectx);

	isc_mem_destroy(&mctx);

	isc_app_finish();

	return (0);
}
