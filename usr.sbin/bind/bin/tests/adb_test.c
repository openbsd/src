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

/* $ISC: adb_test.c,v 1.62 2001/08/08 22:54:28 gson Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/app.h>
#include <isc/buffer.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/socket.h>
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/cache.h>
#include <dns/dispatch.h>
#include <dns/db.h>
#include <dns/log.h>
#include <dns/rootns.h>
#include <dns/result.h>

typedef struct client client_t;
struct client {
	dns_name_t		name;
	const char	       *target;
	ISC_LINK(client_t)	link;
	dns_adbfind_t	       *find;
};

isc_mem_t *mctx;
isc_mempool_t *cmp;
isc_log_t *lctx;
isc_logconfig_t *lcfg;
isc_taskmgr_t *taskmgr;
isc_socketmgr_t *socketmgr;
isc_timermgr_t *timermgr;
dns_dispatchmgr_t *dispatchmgr;
isc_task_t *t1, *t2;
dns_view_t *view;
dns_db_t *rootdb;
ISC_LIST(client_t) clients;
isc_mutex_t client_lock;
isc_stdtime_t now;
dns_adb_t *adb;

static void
check_result(isc_result_t result, const char *format, ...)
     ISC_FORMAT_PRINTF(2, 3);

static void
check_result(isc_result_t result, const char *format, ...) {
	va_list args;

	if (result == ISC_R_SUCCESS)
		return;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, ": %s\n", isc_result_totext(result));
	exit(1);
}

static client_t *
new_client(void) {
	client_t *client;

	client = isc_mempool_get(cmp);
	INSIST(client != NULL);
	dns_name_init(&client->name, NULL);
	ISC_LINK_INIT(client, link);
	client->find = NULL;

	return (client);
}

static void
free_client(client_t **c) {
	client_t *client;

	INSIST(c != NULL);
	client = *c;
	*c = NULL;
	INSIST(client != NULL);
	dns_name_free(&client->name, mctx);
	INSIST(!ISC_LINK_LINKED(client, link));
	INSIST(client->find == NULL);

	isc_mempool_put(cmp, client);
}

static inline void
CLOCK(void) {
	RUNTIME_CHECK(isc_mutex_lock(&client_lock) == ISC_R_SUCCESS);
}

static inline void
CUNLOCK(void) {
	RUNTIME_CHECK(isc_mutex_unlock(&client_lock) == ISC_R_SUCCESS);
}

static void
lookup_callback(isc_task_t *task, isc_event_t *ev) {
	client_t *client;

	client = ev->ev_arg;
	INSIST(client->find == ev->ev_sender);

	printf("NAME %s:\n\tTask %p got event %p type %08x from %p, client %p\n\terr4: %s  err6: %s\n",
	       client->target,
	       task, ev, ev->ev_type, client->find, client,
	       isc_result_totext(client->find->result_v4),
	       isc_result_totext(client->find->result_v6));

	isc_event_free(&ev);
	ev = NULL;

	CLOCK();

	dns_adb_dumpfind(client->find, stderr);
	dns_adb_destroyfind(&client->find);

	ISC_LIST_UNLINK(clients, client, link);
	free_client(&client);

	CUNLOCK();
}

static void
create_managers(void) {
	isc_result_t result;

	taskmgr = NULL;
	result = isc_taskmgr_create(mctx, 5, 0, &taskmgr);
	check_result(result, "isc_taskmgr_create");

	timermgr = NULL;
	result = isc_timermgr_create(mctx, &timermgr);
	check_result(result, "isc_timermgr_create");

	socketmgr = NULL;
	result = isc_socketmgr_create(mctx, &socketmgr);
	check_result(result, "isc_socketmgr_create");

	dispatchmgr = NULL;
	result = dns_dispatchmgr_create(mctx, NULL, &dispatchmgr);
	check_result(result, "dns_dispatchmgr_create");
}

static void
create_view(void) {
	dns_cache_t *cache;
	isc_result_t result;

	/*
	 * View.
	 */
	view = NULL;
	result = dns_view_create(mctx, dns_rdataclass_in, "_default", &view);
	check_result(result, "dns_view_create");

	/*
	 * Cache.
	 */
	cache = NULL;
	result = dns_cache_create(mctx, taskmgr, timermgr, dns_rdataclass_in,
				  "rbt", 0, NULL, &cache);
	check_result(result, "dns_cache_create");
	dns_view_setcache(view, cache);
	dns_cache_detach(&cache);

	{
		unsigned int attrs;
		isc_sockaddr_t any4, any6;
		dns_dispatch_t *disp4 = NULL;
		dns_dispatch_t *disp6 = NULL;

		isc_sockaddr_any(&any4);
		isc_sockaddr_any6(&any6);

		attrs = DNS_DISPATCHATTR_IPV4 | DNS_DISPATCHATTR_UDP;
		RUNTIME_CHECK(dns_dispatch_getudp(dispatchmgr, socketmgr,
						  taskmgr, &any4, 512, 6, 1024,
						  17, 19, attrs, attrs, &disp4)
			      == ISC_R_SUCCESS);
		INSIST(disp4 != NULL);

		attrs = DNS_DISPATCHATTR_IPV6 | DNS_DISPATCHATTR_UDP;
		RUNTIME_CHECK(dns_dispatch_getudp(dispatchmgr, socketmgr,
						  taskmgr, &any6, 512, 6, 1024,
						  17, 19, attrs, attrs, &disp6)
			      == ISC_R_SUCCESS);
		INSIST(disp6 != NULL);

		RUNTIME_CHECK(dns_view_createresolver(view, taskmgr, 10,
						      socketmgr,
						      timermgr, 0,
						      dispatchmgr,
						      disp4, disp6) ==
		      ISC_R_SUCCESS);
	}

	rootdb = NULL;
	result = dns_rootns_create(mctx, dns_rdataclass_in, NULL, &rootdb);
	check_result(result, "dns_rootns_create()");
	dns_view_sethints(view, rootdb);
	dns_db_detach(&rootdb);

	dns_view_freeze(view);
}

static void
lookup(const char *target) {
	dns_name_t name;
	unsigned char namedata[256];
	client_t *client;
	isc_buffer_t t, namebuf;
	isc_result_t result;
	unsigned int options;

	INSIST(target != NULL);

	client = new_client();
	isc_buffer_init(&t, target, strlen(target));
	isc_buffer_add(&t, strlen(target));
	isc_buffer_init(&namebuf, namedata, sizeof(namedata));
	dns_name_init(&name, NULL);
	result = dns_name_fromtext(&name, &t, dns_rootname, ISC_FALSE,
				   &namebuf);
	check_result(result, "dns_name_fromtext %s", target);

	result = dns_name_dup(&name, mctx, &client->name);
	check_result(result, "dns_name_dup %s", target);

	options = 0;
	options |= DNS_ADBFIND_INET;
	options |= DNS_ADBFIND_INET6;
	options |= DNS_ADBFIND_WANTEVENT;
	options |= DNS_ADBFIND_HINTOK;
	options |= DNS_ADBFIND_GLUEOK;
	result = dns_adb_createfind(adb, t2, lookup_callback, client,
				    &client->name, dns_rootname, options,
				    now, NULL, view->dstport, &client->find);
#if 0
	check_result(result, "dns_adb_createfind()");
#endif
	dns_adb_dumpfind(client->find, stderr);

	if ((client->find->options & DNS_ADBFIND_WANTEVENT) != 0) {
		client->target = target;
		ISC_LIST_APPEND(clients, client, link);
	} else {
		printf("NAME %s:  err4 %s, err6 %s\n",
		       target, isc_result_totext(client->find->result_v4),
		       isc_result_totext(client->find->result_v6));

		dns_adb_destroyfind(&client->find);
		free_client(&client);
	}
}

int
main(int argc, char **argv) {
	isc_result_t result;
	isc_logdestination_t destination;

	UNUSED(argc);
	UNUSED(argv);

	dns_result_register();
	result = isc_app_start();
	check_result(result, "isc_app_start()");

	isc_stdtime_get(&now);

	result = isc_mutex_init(&client_lock);
	check_result(result, "isc_mutex_init(&client_lock)");
	ISC_LIST_INIT(clients);

	/*
	 * EVERYTHING needs a memory context.
	 */
	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	cmp = NULL;
	RUNTIME_CHECK(isc_mempool_create(mctx, sizeof(client_t), &cmp)
		      == ISC_R_SUCCESS);
	isc_mempool_setname(cmp, "adb test clients");

	result = isc_log_create(mctx, &lctx, &lcfg);
	check_result(result, "isc_log_create()");
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
	result = isc_log_createchannel(lcfg, "_default",
				       ISC_LOG_TOFILEDESC,
				       ISC_LOG_DYNAMIC,
				       &destination, ISC_LOG_PRINTTIME);
	check_result(result, "isc_log_createchannel()");
	result = isc_log_usechannel(lcfg, "_default", NULL, NULL);
	check_result(result, "isc_log_usechannel()");

	/*
	 * Set the initial debug level.
	 */
	isc_log_setdebuglevel(lctx, 2);

	create_managers();

	t1 = NULL;
	result = isc_task_create(taskmgr, 0, &t1);
	check_result(result, "isc_task_create t1");
	t2 = NULL;
	result = isc_task_create(taskmgr, 0, &t2);
	check_result(result, "isc_task_create t2");

	printf("task 1 = %p\n", t1);
	printf("task 2 = %p\n", t2);

	create_view();

	adb = view->adb;

	/*
	 * Lock the entire client list here.  This will cause all events
	 * for found names to block as well.
	 */
	CLOCK();
	lookup("f.root-servers.net.");		/* Should be in hints */
	lookup("www.iengines.com");		/* should fetch */
	lookup("www.isc.org");			/* should fetch */
	lookup("www.flame.org");		/* should fetch */
	lookup("kechara.flame.org.");		/* should fetch */
	lookup("moghedien.flame.org.");		/* should fetch */
	lookup("mailrelay.flame.org.");		/* should fetch */
	lookup("ipv4v6.flame.org.");		/* should fetch */
	lookup("nonexistant.flame.org.");	/* should fail to be found */
	lookup("foobar.badns.flame.org.");	/* should fail utterly (NS) */
	lookup("i.root-servers.net.");		/* Should be in hints */
	lookup("www.firstcard.com.");
	lookup("dns04.flame.org.");
	CUNLOCK();

	sleep(10);

	dns_adb_dump(adb, stderr);

	sleep(10);

	CLOCK();
	lookup("f.root-servers.net.");		/* Should be in hints */
	lookup("www.iengines.com");		/* should fetch */
	lookup("www.isc.org");			/* should fetch */
	lookup("www.flame.org");		/* should fetch */
	lookup("kechara.flame.org.");		/* should fetch */
	lookup("moghedien.flame.org.");		/* should fetch */
	lookup("mailrelay.flame.org.");		/* should fetch */
	lookup("ipv4v6.flame.org.");		/* should fetch */
	lookup("nonexistant.flame.org.");	/* should fail to be found */
	lookup("foobar.badns.flame.org.");	/* should fail utterly (NS) */
	lookup("i.root-servers.net.");		/* Should be in hints */
	CUNLOCK();

	sleep(20);

	dns_adb_dump(adb, stderr);

	isc_task_detach(&t1);
	isc_task_detach(&t2);

	isc_mem_stats(mctx, stdout);
	dns_adb_dump(adb, stderr);

	isc_app_run();

	dns_adb_dump(adb, stderr);

	dns_view_detach(&view);
	adb = NULL;

	isc_socketmgr_destroy(&socketmgr);
	isc_timermgr_destroy(&timermgr);

	fprintf(stderr, "Destroying task manager\n");
	isc_taskmgr_destroy(&taskmgr);

	isc_log_destroy(&lctx);

	isc_mempool_destroy(&cmp);
	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	isc_app_finish();

	return (0);
}
