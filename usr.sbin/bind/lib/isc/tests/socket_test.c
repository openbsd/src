/*
 * Copyright (C) 2011-2015  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <atf-c.h>

#include <unistd.h>
#include <time.h>

#include <isc/platform.h>
#include <isc/socket.h>
#include <isc/print.h>

#include "../task_p.h"
#include "../unix/socket_p.h"
#include "isctest.h"

static isc_boolean_t recv_dscp;
static unsigned int recv_dscp_value;

/*
 * Helper functions
 */

typedef struct {
	isc_boolean_t done;
	isc_result_t result;
	isc_socket_t *socket;
} completion_t;

static void
completion_init(completion_t *completion) {
	completion->done = ISC_FALSE;
	completion->socket = NULL;
}

static void
accept_done(isc_task_t *task, isc_event_t *event) {
	isc_socket_newconnev_t *nevent = (isc_socket_newconnev_t *)event;
	completion_t *completion = event->ev_arg;

	UNUSED(task);

	completion->result = nevent->result;
	completion->done = ISC_TRUE;
	if (completion->result == ISC_R_SUCCESS)
		completion->socket = nevent->newsocket;

	isc_event_free(&event);
}

static void
event_done(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *dev;
	completion_t *completion = event->ev_arg;

	UNUSED(task);

	dev = (isc_socketevent_t *) event;
	completion->result = dev->result;
	completion->done = ISC_TRUE;
	if ((dev->attributes & ISC_SOCKEVENTATTR_DSCP) != 0) {
		recv_dscp = ISC_TRUE;
		recv_dscp_value = dev->dscp;;
	} else
		recv_dscp = ISC_FALSE;
	isc_event_free(&event);
}

static isc_result_t
waitfor(completion_t *completion) {
	int i = 0;
	while (!completion->done && i++ < 5000) {
#ifndef ISC_PLATFORM_USETHREADS
		while (isc__taskmgr_ready(taskmgr))
			isc__taskmgr_dispatch(taskmgr);
#endif
		isc_test_nap(1000);
	}
	if (completion->done)
		return (ISC_R_SUCCESS);
	return (ISC_R_FAILURE);
}

#if 0
static isc_result_t
waitfor(completion_t *completion) {
	int i = 0;
	while (!completion->done && i++ < 5000) {
		waitbody();
	}
	if (completion->done)
		return (ISC_R_SUCCESS);
	return (ISC_R_FAILURE);
}
#endif

static void
waitbody(void) {
#ifndef ISC_PLATFORM_USETHREADS
	struct timeval tv;
	isc_socketwait_t *swait = NULL;

	while (isc__taskmgr_ready(taskmgr))
		isc__taskmgr_dispatch(taskmgr);
	if (socketmgr != NULL) {
		tv.tv_sec = 0;
		tv.tv_usec = 1000 ;
		if (isc__socketmgr_waitevents(socketmgr, &tv, &swait) > 0)
			isc__socketmgr_dispatch(socketmgr, swait);
	} else
#endif
		isc_test_nap(1000);
}

static isc_result_t
waitfor2(completion_t *c1, completion_t *c2) {
	int i = 0;

	while (!(c1->done && c2->done) && i++ < 5000) {
		waitbody();
	}
	if (c1->done && c2->done)
		return (ISC_R_SUCCESS);
	return (ISC_R_FAILURE);
}

/*
 * Individual unit tests
 */

/* Test UDP sendto/recv (IPv4) */
ATF_TC(udp_sendto);
ATF_TC_HEAD(udp_sendto, tc) {
	atf_tc_set_md_var(tc, "descr", "UDP sendto/recv");
}
ATF_TC_BODY(udp_sendto, tc) {
	isc_result_t result;
	isc_sockaddr_t addr1, addr2;
	struct in_addr in;
	isc_socket_t *s1 = NULL, *s2 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion;
	isc_region_t r;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in.s_addr = inet_addr("127.0.0.1");
	isc_sockaddr_fromin(&addr1, &in, 0);
	isc_sockaddr_fromin(&addr2, &in, 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_bind(s1, &addr1, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_bind(s2, &addr2, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s2, &addr2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr2) != 0);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	completion_init(&completion);
	result = isc_socket_sendto(s1, &r, task, event_done, &completion,
				   &addr2, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s2, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");

	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);

	isc_test_end();
}

/* Test UDP sendto/recv with duplicated socket */
ATF_TC(udp_dup);
ATF_TC_HEAD(udp_dup, tc) {
	atf_tc_set_md_var(tc, "descr", "duplicated socket sendto/recv");
}
ATF_TC_BODY(udp_dup, tc) {
	isc_result_t result;
	isc_sockaddr_t addr1, addr2;
	struct in_addr in;
	isc_socket_t *s1 = NULL, *s2 = NULL, *s3 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion;
	isc_region_t r;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in.s_addr = inet_addr("127.0.0.1");
	isc_sockaddr_fromin(&addr1, &in, 0);
	isc_sockaddr_fromin(&addr2, &in, 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_bind(s1, &addr1, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_bind(s2, &addr2, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s2, &addr2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr2) != 0);

	result = isc_socket_dup(s2, &s3);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	completion_init(&completion);
	result = isc_socket_sendto(s1, &r, task, event_done, &completion,
				   &addr2, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	strcpy(sendbuf, "World");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	completion_init(&completion);
	result = isc_socket_sendto(s1, &r, task, event_done, &completion,
				   &addr2, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s2, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s3, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "World");

	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);
	isc_socket_detach(&s3);

	isc_test_end();
}

/* Test TCP sendto/recv (IPv4) */
ATF_TC(udp_dscp_v4);
ATF_TC_HEAD(udp_dscp_v4, tc) {
	atf_tc_set_md_var(tc, "descr", "UDP DSCP IPV4");
}
ATF_TC_BODY(udp_dscp_v4, tc) {
	isc_result_t result;
	isc_sockaddr_t addr1, addr2;
	struct in_addr in;
	isc_socket_t *s1 = NULL, *s2 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion;
	isc_region_t r;
	isc_socketevent_t *socketevent;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in.s_addr = inet_addr("127.0.0.1");
	isc_sockaddr_fromin(&addr1, &in, 0);
	isc_sockaddr_fromin(&addr2, &in, 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));
	result = isc_socket_bind(s1, &addr1, ISC_SOCKET_REUSEADDRESS);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_udp, &s2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));
	result = isc_socket_bind(s2, &addr2, ISC_SOCKET_REUSEADDRESS);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));
	result = isc_socket_getsockname(s2, &addr2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr2) != 0);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	completion_init(&completion);

	socketevent = isc_socket_socketevent(mctx, s1, ISC_SOCKEVENT_SENDDONE,
					     event_done, &completion);
	ATF_REQUIRE(socketevent != NULL);

	if ((isc_net_probedscp() & ISC_NET_DSCPPKTV4) != 0) {
		socketevent->dscp = 056; /* EF */
		socketevent->attributes |= ISC_SOCKEVENTATTR_DSCP;
	} else if ((isc_net_probedscp() & ISC_NET_DSCPSETV4) != 0) {
		isc_socket_dscp(s1, 056);  /* EF */
		socketevent->dscp = 0;
		socketevent->attributes &= ~ISC_SOCKEVENTATTR_DSCP;
	}

	recv_dscp = ISC_FALSE;
	recv_dscp_value = 0;

	result = isc_socket_sendto2(s1, &r, task, &addr2, NULL, socketevent, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			   isc_result_totext(result));
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s2, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");

	if ((isc_net_probedscp() & ISC_NET_DSCPRECVV4) != 0) {
		ATF_CHECK(recv_dscp);
		ATF_CHECK_EQ(recv_dscp_value, 056);
	} else
		ATF_CHECK(!recv_dscp);
	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);

	isc_test_end();
}

/* Test TCP sendto/recv (IPv4) */
ATF_TC(udp_dscp_v6);
ATF_TC_HEAD(udp_dscp_v6, tc) {
	atf_tc_set_md_var(tc, "descr", "udp dscp ipv6");
}
ATF_TC_BODY(udp_dscp_v6, tc) {
#if defined(ISC_PLATFORM_HAVEIPV6) && defined(WANT_IPV6)
	isc_result_t result;
	isc_sockaddr_t addr1, addr2;
	struct in6_addr in6;
	isc_socket_t *s1 = NULL, *s2 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion;
	isc_region_t r;
	isc_socketevent_t *socketevent;
	int n;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	n = inet_pton(AF_INET6, "::1", &in6.s6_addr);
	ATF_REQUIRE(n == 1);
	isc_sockaddr_fromin6(&addr1, &in6, 0);
	isc_sockaddr_fromin6(&addr2, &in6, 0);

	result = isc_socket_create(socketmgr, PF_INET6, isc_sockettype_udp,
				   &s1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	result = isc_socket_bind(s1, &addr1, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_create(socketmgr, PF_INET6, isc_sockettype_udp,
				   &s2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	result = isc_socket_bind(s2, &addr2, 0);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	result = isc_socket_getsockname(s2, &addr2);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr2) != 0);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	completion_init(&completion);

	socketevent = isc_socket_socketevent(mctx, s1, ISC_SOCKEVENT_SENDDONE,
					     event_done, &completion);
	ATF_REQUIRE(socketevent != NULL);

	if ((isc_net_probedscp() & ISC_NET_DSCPPKTV6) != 0) {
		socketevent->dscp = 056; /* EF */
		socketevent->attributes = ISC_SOCKEVENTATTR_DSCP;
	} else if ((isc_net_probedscp() & ISC_NET_DSCPSETV6) != 0)
		isc_socket_dscp(s1, 056);  /* EF */

	recv_dscp = ISC_FALSE;
	recv_dscp_value = 0;

	result = isc_socket_sendto2(s1, &r, task, &addr2, NULL, socketevent, 0);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s2, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");
	if ((isc_net_probedscp() & ISC_NET_DSCPRECVV6) != 0) {
		ATF_CHECK(recv_dscp);
		ATF_CHECK_EQ(recv_dscp_value, 056);
	} else
		ATF_CHECK(!recv_dscp);

	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);

	isc_test_end();
#else
	UNUSED(tc);
	atf_tc_skip("IPv6 not available");
#endif
}

/* Test TCP sendto/recv (IPv4) */
ATF_TC(tcp_dscp_v4);
ATF_TC_HEAD(tcp_dscp_v4, tc) {
	atf_tc_set_md_var(tc, "descr", "tcp dscp ipv4");
}
ATF_TC_BODY(tcp_dscp_v4, tc) {
	isc_result_t result;
	isc_sockaddr_t addr1;
	struct in_addr in;
	isc_socket_t *s1 = NULL, *s2 = NULL, *s3 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion, completion2;
	isc_region_t r;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	in.s_addr = inet_addr("127.0.0.1");
	isc_sockaddr_fromin(&addr1, &in, 0);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_tcp, &s1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_socket_bind(s1, &addr1, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_listen(s1, 3);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_socket_create(socketmgr, PF_INET, isc_sockettype_tcp, &s2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	completion_init(&completion2);
	result = isc_socket_accept(s1, task, accept_done, &completion2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	completion_init(&completion);
	result = isc_socket_connect(s2, &addr1, task, event_done, &completion);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	waitfor2(&completion, &completion2);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK(completion2.done);
	ATF_CHECK_EQ(completion2.result, ISC_R_SUCCESS);
	s3 = completion2.socket;

	isc_socket_dscp(s2, 056);  /* EF */

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	recv_dscp = ISC_FALSE;
	recv_dscp_value = 0;

	completion_init(&completion);
	result = isc_socket_sendto(s2, &r, task, event_done, &completion,
				   NULL, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s3, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");

	if ((isc_net_probedscp() & ISC_NET_DSCPRECVV4) != 0) {
		if (recv_dscp)
			ATF_CHECK_EQ(recv_dscp_value, 056);
	} else
		ATF_CHECK(!recv_dscp);

	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);
	isc_socket_detach(&s3);

	isc_test_end();
}

/* Test TCP sendto/recv (IPv6) */
ATF_TC(tcp_dscp_v6);
ATF_TC_HEAD(tcp_dscp_v6, tc) {
	atf_tc_set_md_var(tc, "descr", "tcp dscp ipv6");
}
ATF_TC_BODY(tcp_dscp_v6, tc) {
#ifdef ISC_PLATFORM_HAVEIPV6
	isc_result_t result;
	isc_sockaddr_t addr1;
	struct in6_addr in6;
	isc_socket_t *s1 = NULL, *s2 = NULL, *s3 = NULL;
	isc_task_t *task = NULL;
	char sendbuf[BUFSIZ], recvbuf[BUFSIZ];
	completion_t completion, completion2;
	isc_region_t r;
	int n;

	UNUSED(tc);

	result = isc_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	n = inet_pton(AF_INET6, "::1", &in6.s6_addr);
	ATF_REQUIRE(n == 1);
	isc_sockaddr_fromin6(&addr1, &in6, 0);

	result = isc_socket_create(socketmgr, PF_INET6, isc_sockettype_tcp,
				   &s1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_socket_bind(s1, &addr1, 0);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	result = isc_socket_getsockname(s1, &addr1);
	ATF_CHECK_EQ_MSG(result, ISC_R_SUCCESS, "%s",
			 isc_result_totext(result));
	ATF_REQUIRE(isc_sockaddr_getport(&addr1) != 0);

	result = isc_socket_listen(s1, 3);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_socket_create(socketmgr, PF_INET6, isc_sockettype_tcp,
				   &s2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_task_create(taskmgr, 0, &task);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	completion_init(&completion2);
	result = isc_socket_accept(s1, task, accept_done, &completion2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	completion_init(&completion);
	result = isc_socket_connect(s2, &addr1, task, event_done, &completion);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	waitfor2(&completion, &completion2);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK(completion2.done);
	ATF_CHECK_EQ(completion2.result, ISC_R_SUCCESS);
	s3 = completion2.socket;

	isc_socket_dscp(s2, 056);  /* EF */

	strcpy(sendbuf, "Hello");
	r.base = (void *) sendbuf;
	r.length = strlen(sendbuf) + 1;

	recv_dscp = ISC_FALSE;
	recv_dscp_value = 0;

	completion_init(&completion);
	result = isc_socket_sendto(s2, &r, task, event_done, &completion,
				   NULL, NULL);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);

	r.base = (void *) recvbuf;
	r.length = BUFSIZ;
	completion_init(&completion);
	result = isc_socket_recv(s3, &r, 1, task, event_done, &completion);
	ATF_CHECK_EQ(result, ISC_R_SUCCESS);
	waitfor(&completion);
	ATF_CHECK(completion.done);
	ATF_CHECK_EQ(completion.result, ISC_R_SUCCESS);
	ATF_CHECK_STREQ(recvbuf, "Hello");

	if ((isc_net_probedscp() & ISC_NET_DSCPRECVV6) != 0) {
		/*
		 * IPV6_RECVTCLASS is undefined for TCP however
		 * if we do get it it should be the value we set.
		 */
		if (recv_dscp)
			ATF_CHECK_EQ(recv_dscp_value, 056);
	} else
		ATF_CHECK(!recv_dscp);

	isc_task_detach(&task);

	isc_socket_detach(&s1);
	isc_socket_detach(&s2);
	isc_socket_detach(&s3);

	isc_test_end();
#else
	UNUSED(tc);
	atf_tc_skip("IPv6 not available");
#endif
}

ATF_TC(net_probedscp);
ATF_TC_HEAD(net_probedscp, tc) {
	atf_tc_set_md_var(tc, "descr", "probe dscp capabilities");
}
ATF_TC_BODY(net_probedscp, tc) {
	unsigned int n;

	UNUSED(tc);

	n = isc_net_probedscp();
	ATF_CHECK((n & ~ISC_NET_DSCPALL) == 0);

	/* ISC_NET_DSCPSETV4 MUST be set if any is set. */
	if (n & (ISC_NET_DSCPSETV4|ISC_NET_DSCPPKTV4|ISC_NET_DSCPRECVV4))
		ATF_CHECK_MSG((n & ISC_NET_DSCPSETV4) != 0,
			      "IPv4:%s%s%s\n",
			      (n & ISC_NET_DSCPSETV4) ? " set" : " none",
			      (n & ISC_NET_DSCPPKTV4) ? " packet" : "",
			      (n & ISC_NET_DSCPRECVV4) ? " receive" : "");

	/* ISC_NET_DSCPSETV6 MUST be set if any is set. */
	if (n & (ISC_NET_DSCPSETV6|ISC_NET_DSCPPKTV6|ISC_NET_DSCPRECVV6))
		ATF_CHECK_MSG((n & ISC_NET_DSCPSETV6) != 0,
			      "IPv6:%s%s%s\n",
			      (n & ISC_NET_DSCPSETV6) ? " set" : " none",
			      (n & ISC_NET_DSCPPKTV6) ? " packet" : "",
			      (n & ISC_NET_DSCPRECVV6) ? " receive" : "");

#if 0
	fprintf(stdout, "IPv4:%s%s%s\n",
		(n & ISC_NET_DSCPSETV4) ? " set" : "none",
		(n & ISC_NET_DSCPPKTV4) ? " packet" : "",
		(n & ISC_NET_DSCPRECVV4) ? " receive" : "");

	fprintf(stdout, "IPv6:%s%s%s\n",
		(n & ISC_NET_DSCPSETV6) ? " set" : "none",
		(n & ISC_NET_DSCPPKTV6) ? " packet" : "",
		(n & ISC_NET_DSCPRECVV6) ? " receive" : "");
#endif
}

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, udp_sendto);
	ATF_TP_ADD_TC(tp, udp_dup);
	ATF_TP_ADD_TC(tp, tcp_dscp_v4);
	ATF_TP_ADD_TC(tp, tcp_dscp_v6);
	ATF_TP_ADD_TC(tp, udp_dscp_v4);
	ATF_TP_ADD_TC(tp, udp_dscp_v6);
	ATF_TP_ADD_TC(tp, net_probedscp);

	return (atf_no_error());
}
