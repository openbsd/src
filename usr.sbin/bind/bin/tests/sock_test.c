/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1998-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $ISC: sock_test.c,v 1.47.12.4 2004/08/28 06:25:32 marka Exp $ */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <isc/mem.h>
#include <isc/print.h>
#include <isc/task.h>
#include <isc/socket.h>
#include <isc/timer.h>
#include <isc/util.h>

isc_mem_t *mctx;
isc_taskmgr_t *manager;

static void
my_shutdown(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;

	printf("shutdown %s (%p)\n", name, task);
	fflush(stdout);
	isc_event_free(&event);
}

static void
my_send(isc_task_t *task, isc_event_t *event) {
	isc_socket_t *sock;
	isc_socketevent_t *dev;

	sock = event->ev_sender;
	dev = (isc_socketevent_t *)event;

	printf("my_send: %s task %p\n\t(sock %p, base %p, length %d, n %d, "
	       "result %d)\n",
	       (char *)(event->ev_arg), task, sock,
	       dev->region.base, dev->region.length,
	       dev->n, dev->result);

	if (dev->result != ISC_R_SUCCESS) {
		isc_socket_detach(&sock);
		isc_task_shutdown(task);
	}

	isc_mem_put(mctx, dev->region.base, dev->region.length);

	isc_event_free(&event);
}

static void
my_recv(isc_task_t *task, isc_event_t *event) {
	isc_socket_t *sock;
	isc_socketevent_t *dev;
	isc_region_t region;
	char buf[1024];
	char host[256];

	sock = event->ev_sender;
	dev = (isc_socketevent_t *)event;

	printf("Socket %s (sock %p, base %p, length %d, n %d, result %d)\n",
	       (char *)(event->ev_arg), sock,
	       dev->region.base, dev->region.length,
	       dev->n, dev->result);
	if (dev->address.type.sa.sa_family == AF_INET6) {
		inet_ntop(AF_INET6, &dev->address.type.sin6.sin6_addr,
			  host, sizeof(host));
		printf("\tFrom: %s port %d\n", host,
		       ntohs(dev->address.type.sin6.sin6_port));
	} else {
		inet_ntop(AF_INET, &dev->address.type.sin.sin_addr,
			  host, sizeof(host));
		printf("\tFrom: %s port %d\n", host,
		       ntohs(dev->address.type.sin.sin_port));
	}

	if (dev->result != ISC_R_SUCCESS) {
		isc_socket_detach(&sock);

		isc_mem_put(mctx, dev->region.base,
			    dev->region.length);
		isc_event_free(&event);

		isc_task_shutdown(task);
		return;
	}

	/*
	 * Echo the data back.
	 */
	if (strcmp(event->ev_arg, "so2") != 0) {
		size_t len;
		region = dev->region;
		snprintf(buf, sizeof(buf), "\r\nReceived: %.*s\r\n\r\n",
			(int)dev->n, (char *)region.base);
		len = strlen(buf);
		region.base = isc_mem_get(mctx, len + 1);
		region.length = len + 1;
		strlcpy((char *)region.base, buf, region.length);
		isc_socket_send(sock, &region, task, my_send, event->ev_arg);
	} else {
		region = dev->region;
		printf("\r\nReceived: %.*s\r\n\r\n",
		       (int)dev->n, (char *)region.base);
	}

	isc_socket_recv(sock, &dev->region, 1, task, my_recv, event->ev_arg);

	isc_event_free(&event);
}

static void
my_http_get(isc_task_t *task, isc_event_t *event) {
	isc_socket_t *sock;
	isc_socketevent_t *dev;

	sock = event->ev_sender;
	dev = (isc_socketevent_t *)event;

	printf("my_http_get: %s task %p\n\t(sock %p, base %p, length %d, "
	       "n %d, result %d)\n",
	       (char *)(event->ev_arg), task, sock,
	       dev->region.base, dev->region.length,
	       dev->n, dev->result);

	if (dev->result != ISC_R_SUCCESS) {
		isc_socket_detach(&sock);
		isc_task_shutdown(task);
		isc_event_free(&event);
		return;
	}

	isc_socket_recv(sock, &dev->region, 1, task, my_recv, event->ev_arg);

	isc_event_free(&event);
}

static void
my_connect(isc_task_t *task, isc_event_t *event) {
	isc_socket_t *sock;
	isc_socket_connev_t *dev;
	isc_region_t region;
	char buf[1024];
	size_t len;

	sock = event->ev_sender;
	dev = (isc_socket_connev_t *)event;

	printf("%s: Connection result:  %d\n", (char *)(event->ev_arg),
	       dev->result);

	if (dev->result != ISC_R_SUCCESS) {
		isc_socket_detach(&sock);
		isc_event_free(&event);
		isc_task_shutdown(task);
		return;
	}

	/*
	 * Send a GET string, and set up to receive (and just display)
	 * the result.
	 */
	strlcpy(buf, "GET / HTTP/1.1\r\nHost: www.flame.org\r\n"
	       "Connection: Close\r\n\r\n", sizeof(buf));
	len = strlen(buf);
	region.base = isc_mem_get(mctx, len + 1);
	region.length = len + 1;
	strlcpy((char *)region.base, buf, region.length);

	isc_socket_send(sock, &region, task, my_http_get, event->ev_arg);

	isc_event_free(&event);
}

static void
my_listen(isc_task_t *task, isc_event_t *event) {
	char *name = event->ev_arg;
	isc_socket_newconnev_t *dev;
	isc_region_t region;
	isc_socket_t *oldsock;
	isc_task_t *newtask;

	dev = (isc_socket_newconnev_t *)event;

	printf("newcon %s (task %p, oldsock %p, newsock %p, result %d)\n",
	       name, task, event->ev_sender, dev->newsocket, dev->result);
	fflush(stdout);

	if (dev->result == ISC_R_SUCCESS) {
		/*
		 * Queue another listen on this socket.
		 */
		isc_socket_accept(event->ev_sender, task, my_listen,
				  event->ev_arg);

		region.base = isc_mem_get(mctx, 20);
		region.length = 20;

		/*
		 * Create a new task for this socket, and queue up a
		 * recv on it.
		 */
		newtask = NULL;
		RUNTIME_CHECK(isc_task_create(manager, 0, &newtask)
			      == ISC_R_SUCCESS);
		isc_socket_recv(dev->newsocket, &region, 1,
				newtask, my_recv, event->ev_arg);
		isc_task_detach(&newtask);
	} else {
		printf("detaching from socket %p\n", event->ev_sender);
		oldsock = event->ev_sender;

		isc_socket_detach(&oldsock);

		isc_event_free(&event);
		isc_task_shutdown(task);
		return;
	}

	isc_event_free(&event);
}

static void
timeout(isc_task_t *task, isc_event_t *event) {
	isc_socket_t *sock = event->ev_arg;

	printf("Timeout, canceling IO on socket %p (task %p)\n", sock, task);

	isc_socket_cancel(sock, NULL, ISC_SOCKCANCEL_ALL);
	isc_timer_detach((isc_timer_t **)&event->ev_sender);
	isc_event_free(&event);
}

int
main(int argc, char *argv[]) {
	isc_task_t *t1, *t2;
	isc_timermgr_t *timgr;
	isc_time_t expires;
	isc_interval_t interval;
	isc_timer_t *ti1;
	unsigned int workers;
	isc_socketmgr_t *socketmgr;
	isc_socket_t *so1, *so2;
	isc_sockaddr_t sockaddr;
	struct in_addr ina;
	struct in6_addr in6a;
	isc_result_t result;
	int pf;

	if (argc > 1)
		workers = atoi(argv[1]);
	else
		workers = 2;
	printf("%d workers\n", workers);

	if (isc_net_probeipv6() == ISC_R_SUCCESS)
		pf = PF_INET6;
	else
		pf = PF_INET;

	/*
	 * EVERYTHING needs a memory context.
	 */
	mctx = NULL;
	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);

	/*
	 * The task manager is independent (other than memory context)
	 */
	manager = NULL;
	RUNTIME_CHECK(isc_taskmgr_create(mctx, workers, 0, &manager) ==
		      ISC_R_SUCCESS);

	/*
	 * Timer manager depends only on the memory context as well.
	 */
	timgr = NULL;
	RUNTIME_CHECK(isc_timermgr_create(mctx, &timgr) == ISC_R_SUCCESS);

	t1 = NULL;
	RUNTIME_CHECK(isc_task_create(manager, 0, &t1) == ISC_R_SUCCESS);
	t2 = NULL;
	RUNTIME_CHECK(isc_task_create(manager, 0, &t2) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t1, my_shutdown, "1") ==
		      ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_task_onshutdown(t2, my_shutdown, "2") ==
		      ISC_R_SUCCESS);

	printf("task 1 = %p\n", t1);
	printf("task 2 = %p\n", t2);

	socketmgr = NULL;
	RUNTIME_CHECK(isc_socketmgr_create(mctx, &socketmgr) == ISC_R_SUCCESS);

	/*
	 * Open up a listener socket.
	 */
	so1 = NULL;

	if (pf == PF_INET6) {
		in6a = in6addr_any;
		isc_sockaddr_fromin6(&sockaddr, &in6a, 5544);
	} else {
		ina.s_addr = INADDR_ANY;
		isc_sockaddr_fromin(&sockaddr, &ina, 5544);
	}
	RUNTIME_CHECK(isc_socket_create(socketmgr, pf, isc_sockettype_tcp,
					&so1) == ISC_R_SUCCESS);
	result = isc_socket_bind(so1, &sockaddr);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_socket_listen(so1, 0) == ISC_R_SUCCESS);

	/*
	 * Queue up the first accept event.
	 */
	RUNTIME_CHECK(isc_socket_accept(so1, t1, my_listen, "so1")
		      == ISC_R_SUCCESS);
	isc_time_settoepoch(&expires);
	isc_interval_set(&interval, 10, 0);
	ti1 = NULL;
	RUNTIME_CHECK(isc_timer_create(timgr, isc_timertype_once, &expires,
				       &interval, t1, timeout, so1, &ti1) ==
		      ISC_R_SUCCESS);

	/*
	 * Open up a socket that will connect to www.flame.org, port 80.
	 * Why not.  :)
	 */
	so2 = NULL;
	ina.s_addr = inet_addr("204.152.184.97");
	if (0 && pf == PF_INET6)
		isc_sockaddr_v6fromin(&sockaddr, &ina, 80);
	else
		isc_sockaddr_fromin(&sockaddr, &ina, 80);
	RUNTIME_CHECK(isc_socket_create(socketmgr, isc_sockaddr_pf(&sockaddr),
					isc_sockettype_tcp,
					&so2) == ISC_R_SUCCESS);

	RUNTIME_CHECK(isc_socket_connect(so2, &sockaddr, t2,
					 my_connect, "so2") == ISC_R_SUCCESS);

	/*
	 * Detaching these is safe, since the socket will attach to the
	 * task for any outstanding requests.
	 */
	isc_task_detach(&t1);
	isc_task_detach(&t2);

	/*
	 * Wait a short while.
	 */
	sleep(10);

	fprintf(stderr, "Destroying socket manager\n");
	isc_socketmgr_destroy(&socketmgr);

	fprintf(stderr, "Destroying timer manager\n");
	isc_timermgr_destroy(&timgr);

	fprintf(stderr, "Destroying task manager\n");
	isc_taskmgr_destroy(&manager);

	isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	return (0);
}
