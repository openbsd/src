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

/* $ISC: byname_test.c,v 1.25 2001/01/09 21:40:55 bwelling Exp $ */

/*
 * Principal Author: Bob Halley
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/netaddr.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/cache.h>
#include <dns/dispatch.h>
#include <dns/events.h>
#include <dns/forward.h>
#include <dns/log.h>
#include <dns/resolver.h>
#include <dns/result.h>

static isc_mem_t *mctx = NULL;
static isc_taskmgr_t *taskmgr;
static dns_view_t *view = NULL;
static dns_adbfind_t *find = NULL;
static isc_task_t *task = NULL;
static dns_fixedname_t name;
static dns_fixedname_t target;
static isc_log_t *lctx;
static isc_logconfig_t *lcfg;
static unsigned int level = 0;

static void adb_callback(isc_task_t *task, isc_event_t *event);

static void
log_init(void) {
	isc_logdestination_t destination;
	unsigned int flags;

	/*
	 * Setup a logging context.
	 */
	RUNTIME_CHECK(isc_log_create(mctx, &lctx, &lcfg) == ISC_R_SUCCESS);
	isc_log_setcontext(lctx);
	dns_log_init(lctx);
	dns_log_setcontext(lctx);

	/*
	 * Create and install the default channel.
	 */
	destination.file.stream = stderr;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	flags = ISC_LOG_PRINTTIME;
	RUNTIME_CHECK(isc_log_createchannel(lcfg, "_default",
					    ISC_LOG_TOFILEDESC,
					    ISC_LOG_DYNAMIC,
					    &destination, flags) ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_log_usechannel(lcfg, "_default", NULL, NULL) ==
		      ISC_R_SUCCESS);
	isc_log_setdebuglevel(lctx, level);
}

static void
print_addresses(dns_adbfind_t *find) {
	dns_adbaddrinfo_t *address;

	for (address = ISC_LIST_HEAD(find->list);
	     address != NULL;
	     address = ISC_LIST_NEXT(address, publink)) {
		isc_netaddr_t netaddr;
		char text[ISC_NETADDR_FORMATSIZE];
		isc_netaddr_fromsockaddr(&netaddr, &address->sockaddr);
		isc_netaddr_format(&netaddr, text, sizeof(text));
		printf("%s\n", text);
	}
}

static void
print_name(dns_name_t *name) {
	char text[DNS_NAME_FORMATSIZE];

	dns_name_format(name, text, sizeof(text));
	printf("%s\n", text);
}

static void
do_find(isc_boolean_t want_event) {
	isc_result_t result;
	isc_boolean_t done = ISC_FALSE;
	unsigned int options;

	options = DNS_ADBFIND_INET | DNS_ADBFIND_INET6;
	if (want_event)
		options |= DNS_ADBFIND_WANTEVENT | DNS_ADBFIND_EMPTYEVENT;
	dns_fixedname_init(&target);
	result = dns_adb_createfind(view->adb, task, adb_callback, NULL,
				    dns_fixedname_name(&name),
				    dns_rootname, options, 0,
				    dns_fixedname_name(&target), 0,
				    &find);
	if (result == ISC_R_SUCCESS) {
		if (!ISC_LIST_EMPTY(find->list)) {
			/*
			 * We have at least some of the addresses for the
			 * name.
			 */
			INSIST((find->options & DNS_ADBFIND_WANTEVENT) == 0);
			print_addresses(find);
			done = ISC_TRUE;
		} else {
			/*
			 * We don't know any of the addresses for this
			 * name.
			 */
			if ((find->options & DNS_ADBFIND_WANTEVENT) == 0) {
				/*
				 * And ADB isn't going to send us any events
				 * either.  This query loses.
				 */
				done = ISC_TRUE;
			}
			/*
			 * If the DNS_ADBFIND_WANTEVENT flag was set, we'll
			 * get an event when something happens.
			 */
		}
	} else if (result == DNS_R_ALIAS) {
		print_name(dns_fixedname_name(&target));
		done = ISC_TRUE;
	} else {
		printf("dns_adb_createfind() returned %s\n",
		       isc_result_totext(result));
		done = ISC_TRUE;
	}

	if (done) {
		if (find != NULL)
			dns_adb_destroyfind(&find);
		isc_app_shutdown();
	}
}

static void
adb_callback(isc_task_t *etask, isc_event_t *event) {
	unsigned int type = event->ev_type;

	REQUIRE(etask == task);

	isc_event_free(&event);
	dns_adb_destroyfind(&find);

	if (type == DNS_EVENT_ADBMOREADDRESSES)
		do_find(ISC_FALSE);
	else if (type == DNS_EVENT_ADBNOMOREADDRESSES) {
		printf("no more addresses\n");
		isc_app_shutdown();
	} else {
		printf("unexpected ADB event type %u\n", type);
		isc_app_shutdown();
	}
}

static void
run(isc_task_t *task, isc_event_t *event) {
	UNUSED(task);
	do_find(ISC_TRUE);
	isc_event_free(&event);
}

int
main(int argc, char *argv[]) {
	isc_boolean_t verbose = ISC_FALSE;
	unsigned int workers = 2;
	isc_timermgr_t *timermgr;
	int ch;
	isc_socketmgr_t *socketmgr;
	dns_dispatchmgr_t *dispatchmgr;
	dns_cache_t *cache;
	isc_buffer_t b;

	RUNTIME_CHECK(isc_app_start() == ISC_R_SUCCESS);

	dns_result_register();

	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	while ((ch = isc_commandline_parse(argc, argv, "d:vw:")) != -1) {
		switch (ch) {
		case 'd':
			level = (unsigned int)atoi(isc_commandline_argument);
			break;
		case 'v':
			verbose = ISC_TRUE;
			break;
		case 'w':
			workers = (unsigned int)atoi(isc_commandline_argument);
			break;
		}
	}

	log_init();

	if (verbose) {
		printf("%u workers\n", workers);
		printf("IPv4: %s\n", isc_result_totext(isc_net_probeipv4()));
		printf("IPv6: %s\n", isc_result_totext(isc_net_probeipv6()));
	}

	taskmgr = NULL;
	RUNTIME_CHECK(isc_taskmgr_create(mctx, workers, 0, &taskmgr) ==
		      ISC_R_SUCCESS);
	task = NULL;
	RUNTIME_CHECK(isc_task_create(taskmgr, 0, &task) ==
		      ISC_R_SUCCESS);
	isc_task_setname(task, "byname", NULL);

	dispatchmgr = NULL;
	RUNTIME_CHECK(dns_dispatchmgr_create(mctx, NULL, &dispatchmgr)
		      == ISC_R_SUCCESS);

	timermgr = NULL;
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timermgr) == ISC_R_SUCCESS);
	socketmgr = NULL;
	RUNTIME_CHECK(isc_socketmgr_create(mctx, &socketmgr) == ISC_R_SUCCESS);

	cache = NULL;
	RUNTIME_CHECK(dns_cache_create(mctx, taskmgr, timermgr,
				       dns_rdataclass_in, "rbt", 0, NULL,
				       &cache) == ISC_R_SUCCESS);

	view = NULL;
	RUNTIME_CHECK(dns_view_create(mctx, dns_rdataclass_in, "default",
				      &view) == ISC_R_SUCCESS);

	{
		unsigned int attrs;
		dns_dispatch_t *disp4 = NULL;
		dns_dispatch_t *disp6 = NULL;

		if (isc_net_probeipv4() == ISC_R_SUCCESS) {
			isc_sockaddr_t any4;
			isc_sockaddr_any(&any4);

			attrs = DNS_DISPATCHATTR_IPV4 | DNS_DISPATCHATTR_UDP;
			RUNTIME_CHECK(dns_dispatch_getudp(dispatchmgr,
							  socketmgr,
							  taskmgr, &any4,
							  512, 6, 1024,
							  17, 19, attrs,
							  attrs, &disp4)
				      == ISC_R_SUCCESS);
			INSIST(disp4 != NULL);
		}

		if (isc_net_probeipv6() == ISC_R_SUCCESS) {
			isc_sockaddr_t any6;

			isc_sockaddr_any6(&any6);

			attrs = DNS_DISPATCHATTR_IPV6 | DNS_DISPATCHATTR_UDP;
			RUNTIME_CHECK(dns_dispatch_getudp(dispatchmgr,
							  socketmgr,
							  taskmgr, &any6,
							  512, 6, 1024,
							  17, 19, attrs,
							  attrs, &disp6)
				      == ISC_R_SUCCESS);
			INSIST(disp6 != NULL);
		}

		RUNTIME_CHECK(dns_view_createresolver(view, taskmgr, 10,
						      socketmgr,
						      timermgr, 0,
						      dispatchmgr,
						      disp4, disp6) ==
		      ISC_R_SUCCESS);

		if (disp4 != NULL)
			dns_dispatch_detach(&disp4);
		if (disp6 != NULL)
			dns_dispatch_detach(&disp6);
	}

	{
		struct in_addr ina;
		isc_sockaddr_t sa;
		isc_sockaddrlist_t sal;

		ISC_LIST_INIT(sal);
		ina.s_addr = inet_addr("127.0.0.1");
		isc_sockaddr_fromin(&sa, &ina, 53);
		ISC_LIST_APPEND(sal, &sa, link);

		RUNTIME_CHECK(dns_fwdtable_add(view->fwdtable, dns_rootname,
					       &sal, dns_fwdpolicy_only)
			      == ISC_R_SUCCESS);
	}

	dns_view_setcache(view, cache);
	dns_view_freeze(view);

	dns_cache_detach(&cache);

	printf("name = %s\n", argv[isc_commandline_index]);
	isc_buffer_init(&b, argv[isc_commandline_index],
			strlen(argv[isc_commandline_index]));
	isc_buffer_add(&b, strlen(argv[isc_commandline_index]));
	dns_fixedname_init(&name);
	dns_fixedname_init(&target);
	RUNTIME_CHECK(dns_name_fromtext(dns_fixedname_name(&name), &b,
					dns_rootname, ISC_FALSE, NULL) ==
		      ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_app_onrun(mctx, task, run, NULL) == ISC_R_SUCCESS);

	(void)isc_app_run();

	dns_view_detach(&view);
	isc_task_shutdown(task);
	isc_task_detach(&task);

	dns_dispatchmgr_destroy(&dispatchmgr);

	isc_taskmgr_destroy(&taskmgr);

	isc_socketmgr_destroy(&socketmgr);
	isc_timermgr_destroy(&timermgr);

	isc_log_destroy(&lctx);

	if (verbose)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	isc_app_finish();

	return (0);
}
