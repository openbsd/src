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

/* $Id: socket_api.c,v 1.1 2020/02/07 09:58:54 florian Exp $ */



#include <isc/app.h>
#include <isc/magic.h>
#include <isc/socket.h>
#include <isc/util.h>

isc_result_t
isc_socketmgr_create(isc_socketmgr_t **managerp) {
	return (isc__socketmgr_create(managerp));
}

void
isc_socketmgr_destroy(isc_socketmgr_t **managerp) {
	REQUIRE(managerp != NULL && ISCAPI_SOCKETMGR_VALID(*managerp));

	isc__socketmgr_destroy(managerp);

	ENSURE(*managerp == NULL);
}

isc_result_t
isc_socket_create(isc_socketmgr_t *manager, int pf, isc_sockettype_t type,
		  isc_socket_t **socketp)
{
	REQUIRE(ISCAPI_SOCKETMGR_VALID(manager));

	return (isc__socket_create(manager, pf, type, socketp));
}

void
isc_socket_attach(isc_socket_t *sock, isc_socket_t **socketp) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));
	REQUIRE(socketp != NULL && *socketp == NULL);

	isc__socket_attach(sock, socketp);

	ENSURE(*socketp == sock);
}

void
isc_socket_detach(isc_socket_t **socketp) {
	REQUIRE(socketp != NULL && ISCAPI_SOCKET_VALID(*socketp));

	isc__socket_detach(socketp);

	ENSURE(*socketp == NULL);
}

isc_result_t
isc_socket_bind(isc_socket_t *sock, isc_sockaddr_t *sockaddr,
		unsigned int options)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	return (isc__socket_bind(sock, sockaddr, options));
}

isc_result_t
isc_socket_connect(isc_socket_t *sock, isc_sockaddr_t *addr, isc_task_t *task,
		   isc_taskaction_t action, void *arg)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	return (isc__socket_connect(sock, addr, task, action, arg));
}

void
isc_socket_cancel(isc_socket_t *sock, isc_task_t *task, unsigned int how) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	isc__socket_cancel(sock, task, how);
}

isc_result_t
isc_socket_recvv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		 unsigned int minimum, isc_task_t *task,
		 isc_taskaction_t action, void *arg)
{
	return (isc__socket_recvv(sock, buflist, minimum, task, action, arg));
}

isc_result_t
isc_socket_sendv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		  isc_task_t *task, isc_taskaction_t action, void *arg)
{
	return (isc__socket_sendv(sock, buflist, task, action, arg));
}

isc_result_t
isc_socket_sendtov2(isc_socket_t *sock, isc_bufferlist_t *buflist,
		    isc_task_t *task, isc_taskaction_t action, void *arg,
		    isc_sockaddr_t *address, struct in6_pktinfo *pktinfo,
		    unsigned int flags)
{
	return (isc__socket_sendtov2(sock, buflist, task, action, arg,
				     address, pktinfo, flags));
}
