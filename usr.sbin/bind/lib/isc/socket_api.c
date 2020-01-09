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

/* $Id: socket_api.c,v 1.3 2020/01/09 18:17:19 florian Exp $ */

#include <config.h>

#include <isc/app.h>
#include <isc/magic.h>
#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/socket.h>
#include <isc/util.h>

static isc_mutex_t createlock;
static isc_once_t once = ISC_ONCE_INIT;
static isc_socketmgrcreatefunc_t socketmgr_createfunc = NULL;

static void
initialize(void) {
	RUNTIME_CHECK(isc_mutex_init(&createlock) == ISC_R_SUCCESS);
}

isc_result_t
isc_socket_register(isc_socketmgrcreatefunc_t createfunc) {
	isc_result_t result = ISC_R_SUCCESS;

	RUNTIME_CHECK(isc_once_do(&once, initialize) == ISC_R_SUCCESS);

	LOCK(&createlock);
	if (socketmgr_createfunc == NULL)
		socketmgr_createfunc = createfunc;
	else
		result = ISC_R_EXISTS;
	UNLOCK(&createlock);

	return (result);
}

isc_result_t
isc_socketmgr_createinctx(isc_mem_t *mctx, isc_appctx_t *actx,
			  isc_socketmgr_t **managerp)
{
	isc_result_t result;

	LOCK(&createlock);

	REQUIRE(socketmgr_createfunc != NULL);
	result = (*socketmgr_createfunc)(mctx, managerp);

	UNLOCK(&createlock);

	if (result == ISC_R_SUCCESS)
		isc_appctx_setsocketmgr(actx, *managerp);

	return (result);
}

isc_result_t
isc_socketmgr_create(isc_mem_t *mctx, isc_socketmgr_t **managerp) {
	isc_result_t result;

	if (isc_bind9)
		return (isc__socketmgr_create(mctx, managerp));

	LOCK(&createlock);

	REQUIRE(socketmgr_createfunc != NULL);
	result = (*socketmgr_createfunc)(mctx, managerp);

	UNLOCK(&createlock);

	return (result);
}

void
isc_socketmgr_destroy(isc_socketmgr_t **managerp) {
	REQUIRE(managerp != NULL && ISCAPI_SOCKETMGR_VALID(*managerp));

	if (isc_bind9)
		isc__socketmgr_destroy(managerp);
	else
		(*managerp)->methods->destroy(managerp);

	ENSURE(*managerp == NULL);
}

isc_result_t
isc_socket_create(isc_socketmgr_t *manager, int pf, isc_sockettype_t type,
		  isc_socket_t **socketp)
{
	REQUIRE(ISCAPI_SOCKETMGR_VALID(manager));

	if (isc_bind9)
		return (isc__socket_create(manager, pf, type, socketp));

	return (manager->methods->socketcreate(manager, pf, type, socketp));
}

void
isc_socket_attach(isc_socket_t *sock, isc_socket_t **socketp) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));
	REQUIRE(socketp != NULL && *socketp == NULL);

	if (isc_bind9)
		isc__socket_attach(sock, socketp);
	else
		sock->methods->attach(sock, socketp);

	ENSURE(*socketp == sock);
}

void
isc_socket_detach(isc_socket_t **socketp) {
	REQUIRE(socketp != NULL && ISCAPI_SOCKET_VALID(*socketp));

	if (isc_bind9)
		isc__socket_detach(socketp);
	else
		(*socketp)->methods->detach(socketp);

	ENSURE(*socketp == NULL);
}

isc_result_t
isc_socket_bind(isc_socket_t *sock, isc_sockaddr_t *sockaddr,
		unsigned int options)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_bind(sock, sockaddr, options));

	return (sock->methods->bind(sock, sockaddr, options));
}

isc_result_t
isc_socket_sendto(isc_socket_t *sock, isc_region_t *region, isc_task_t *task,
		  isc_taskaction_t action, void *arg,
		  isc_sockaddr_t *address, struct in6_pktinfo *pktinfo)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_sendto(sock, region, task,
					   action, arg, address, pktinfo));

	return (sock->methods->sendto(sock, region, task, action, arg, address,
				      pktinfo));
}

isc_result_t
isc_socket_connect(isc_socket_t *sock, isc_sockaddr_t *addr, isc_task_t *task,
		   isc_taskaction_t action, void *arg)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_connect(sock, addr, task, action, arg));

	return (sock->methods->connect(sock, addr, task, action, arg));
}

isc_result_t
isc_socket_recv(isc_socket_t *sock, isc_region_t *region, unsigned int minimum,
		isc_task_t *task, isc_taskaction_t action, void *arg)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_recv(sock, region, minimum,
					 task, action, arg));

	return (sock->methods->recv(sock, region, minimum, task, action, arg));
}

void
isc_socket_cancel(isc_socket_t *sock, isc_task_t *task, unsigned int how) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		isc__socket_cancel(sock, task, how);
	else
		sock->methods->cancel(sock, task, how);
}

isc_result_t
isc_socket_getsockname(isc_socket_t *sock, isc_sockaddr_t *addressp) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_getsockname(sock, addressp));

	return (sock->methods->getsockname(sock, addressp));
}

void
isc_socket_ipv6only(isc_socket_t *sock, isc_boolean_t yes) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		isc__socket_ipv6only(sock, yes);
	else
		sock->methods->ipv6only(sock, yes);
}

void
isc_socket_dscp(isc_socket_t *sock, isc_dscp_t dscp) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	sock->methods->dscp(sock, dscp);
}

isc_sockettype_t
isc_socket_gettype(isc_socket_t *sock) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_gettype(sock));

	return (sock->methods->gettype(sock));
}

void
isc_socket_setname(isc_socket_t *sock, const char *name, void *tag) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	UNUSED(sock);		/* in case REQUIRE() is empty */
	UNUSED(name);
	UNUSED(tag);
}

isc_result_t
isc_socket_fdwatchcreate(isc_socketmgr_t *manager, int fd, int flags,
			 isc_sockfdwatch_t callback, void *cbarg,
			 isc_task_t *task, isc_socket_t **socketp)
{
	REQUIRE(ISCAPI_SOCKETMGR_VALID(manager));

	if (isc_bind9)
		return (isc__socket_fdwatchcreate(manager, fd, flags,
						  callback, cbarg,
						  task, socketp));

	return (manager->methods->fdwatchcreate(manager, fd, flags,
						callback, cbarg, task,
						socketp));
}

isc_result_t
isc_socket_fdwatchpoke(isc_socket_t *sock, int flags)
{
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_fdwatchpoke(sock, flags));

	return (sock->methods->fdwatchpoke(sock, flags));
}

isc_result_t
isc_socket_dup(isc_socket_t *sock, isc_socket_t **socketp) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));
	REQUIRE(socketp != NULL && *socketp == NULL);

	if (isc_bind9)
		return (isc__socket_dup(sock, socketp));

	return (sock->methods->dup(sock, socketp));
}

int
isc_socket_getfd(isc_socket_t *sock) {
	REQUIRE(ISCAPI_SOCKET_VALID(sock));

	if (isc_bind9)
		return (isc__socket_getfd(sock));

	return (sock->methods->getfd(sock));
}

isc_result_t
isc_socket_open(isc_socket_t *sock) {
	return (isc__socket_open(sock));
}

isc_result_t
isc_socket_close(isc_socket_t *sock) {
	return (isc__socket_close(sock));
}

isc_result_t
isc_socketmgr_create2(isc_mem_t *mctx, isc_socketmgr_t **managerp,
		       unsigned int maxsocks)
{
	return (isc__socketmgr_create2(mctx, managerp, maxsocks));
}

isc_result_t
isc_socket_recvv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		 unsigned int minimum, isc_task_t *task,
		 isc_taskaction_t action, void *arg)
{
	return (isc__socket_recvv(sock, buflist, minimum, task, action, arg));
}

isc_result_t
isc_socket_recv2(isc_socket_t *sock, isc_region_t *region,
		  unsigned int minimum, isc_task_t *task,
		  isc_socketevent_t *event, unsigned int flags)
{
	return (isc__socket_recv2(sock, region, minimum, task, event, flags));
}

isc_result_t
isc_socket_send(isc_socket_t *sock, isc_region_t *region,
		 isc_task_t *task, isc_taskaction_t action, void *arg)
{
	return (isc__socket_send(sock, region, task, action, arg));
}

isc_result_t
isc_socket_sendv(isc_socket_t *sock, isc_bufferlist_t *buflist,
		  isc_task_t *task, isc_taskaction_t action, void *arg)
{
	return (isc__socket_sendv(sock, buflist, task, action, arg));
}

isc_result_t
isc_socket_sendtov(isc_socket_t *sock, isc_bufferlist_t *buflist,
		    isc_task_t *task, isc_taskaction_t action, void *arg,
		    isc_sockaddr_t *address, struct in6_pktinfo *pktinfo)
{
	return (isc__socket_sendtov(sock, buflist, task, action, arg,
				    address, pktinfo));
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

isc_result_t
isc_socket_sendto2(isc_socket_t *sock, isc_region_t *region,
		    isc_task_t *task,
		    isc_sockaddr_t *address, struct in6_pktinfo *pktinfo,
		    isc_socketevent_t *event, unsigned int flags)
{
	return (isc__socket_sendto2(sock, region, task, address, pktinfo,
				    event, flags));
}

void
isc_socket_cleanunix(isc_sockaddr_t *sockaddr, isc_boolean_t active) {
	isc__socket_cleanunix(sockaddr, active);
}

isc_result_t
isc_socket_permunix(isc_sockaddr_t *sockaddr, uint32_t perm,
		     uint32_t owner, uint32_t group)
{
	return (isc__socket_permunix(sockaddr, perm, owner, group));
}

isc_result_t
isc_socket_filter(isc_socket_t *sock, const char *filter) {
	return (isc__socket_filter(sock, filter));
}

isc_result_t
isc_socket_listen(isc_socket_t *sock, unsigned int backlog) {
	return (isc__socket_listen(sock, backlog));
}

isc_result_t
isc_socket_accept(isc_socket_t *sock, isc_task_t *task,
		   isc_taskaction_t action, void *arg)
{
	return (isc__socket_accept(sock, task, action, arg));
}

isc_result_t
isc_socket_getpeername(isc_socket_t *sock, isc_sockaddr_t *addressp) {
	return (isc__socket_getpeername(sock, addressp));
}
