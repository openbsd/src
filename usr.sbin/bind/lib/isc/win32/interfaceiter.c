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

/* $ISC: interfaceiter.c,v 1.4 2001/07/17 20:29:27 gson Exp $ */

/*
 * Note that this code will need to be revisited to support IPv6 Interfaces.
 * For now we just iterate through IPv4 interfaces.
 */

#include <config.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <isc/interfaceiter.h>
#include <isc/mem.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/types.h>
#include <isc/util.h>
#include "errno2result.h"

/* Common utility functions */

/*
 * Extract the network address part from a "struct sockaddr".
 *
 * The address family is given explicity
 * instead of using src->sa_family, because the latter does not work
 * for copying a network mask obtained by SIOCGIFNETMASK (it does
 * not have a valid address family).
 */


#define IFITER_MAGIC		0x49464954U	/* IFIT. */
#define VALID_IFITER(t)		((t) != NULL && (t)->magic == IFITER_MAGIC)

struct isc_interfaceiter {
	unsigned int		magic;		/* Magic number. */
	isc_mem_t		*mctx;
	int			socket;
	INTERFACE_INFO		IFData;		/* Current Interface Info */
	int			numIF;		/* Current Interface count */
	int			totalIF;	/* Total Number
						   of Interfaces */
	INTERFACE_INFO		*buf;		/* Buffer for WSAIoctl data. */
	unsigned int		bufsize;	/* Bytes allocated. */
	INTERFACE_INFO		*pos;		/* Current offset in IF List */
	isc_interface_t		current;	/* Current interface data. */
	isc_result_t		result;		/* Last result code. */
};


/*
 * Size of buffer for SIO_GET_INTERFACE_LIST, in number of interfaces.
 * We assume no sane system will have more than than 1K of IP addresses on
 * all of its adapters.
 */
#define IFCONF_SIZE_INITIAL	  16
#define IFCONF_SIZE_INCREMENT	  64
#define IFCONF_SIZE_MAX		1040

static void
get_addr(unsigned int family, isc_netaddr_t *dst, struct sockaddr *src) {
	dst->family = family;
	switch (family) {
	case AF_INET:
		memcpy(&dst->type.in,
		       &((struct sockaddr_in *) src)->sin_addr,
		       sizeof(struct in_addr));
		break;
	case	AF_INET6:
		memcpy(&dst->type.in6,
		       &((struct sockaddr_in6 *) src)->sin6_addr,
		       sizeof(struct in6_addr));
		break;
	default:
		INSIST(0);
		break;
	}
}

isc_result_t
isc_interfaceiter_create(isc_mem_t *mctx, isc_interfaceiter_t **iterp) {
	isc_interfaceiter_t *iter;
	isc_result_t result;
	int error;
	unsigned long bytesReturned = 0;

	REQUIRE(mctx != NULL);
	REQUIRE(iterp != NULL);
	REQUIRE(*iterp == NULL);

	iter = isc_mem_get(mctx, sizeof(*iter));
	if (iter == NULL)
		return (ISC_R_NOMEMORY);

	iter->mctx = mctx;
	iter->buf = NULL;

	/*
	 * Create an unbound datagram socket to do the
	 * SIO_GET_INTERFACE_LIST WSAIoctl on.
	 */
	if ((iter->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "making interface scan socket: %s",
				 strerror(errno));
		result = ISC_R_UNEXPECTED;
		goto socket_failure;
	}

	/*
	 * Get the interface configuration, allocating more memory if
	 * necessary.
	 */
	iter->bufsize = IFCONF_SIZE_INITIAL*sizeof(INTERFACE_INFO);

	for (;;) {
		iter->buf = isc_mem_get(mctx, iter->bufsize);
		if (iter->buf == NULL) {
			result = ISC_R_NOMEMORY;
			goto alloc_failure;
		}

		if (WSAIoctl(iter->socket, SIO_GET_INTERFACE_LIST,
			     0, 0, iter->buf, iter->bufsize,
			     &bytesReturned, 0, 0)
		    == SOCKET_ERROR)
		{
			error = WSAGetLastError();
			if (error != WSAEFAULT && error != WSAENOBUFS) {
				errno = error;
				UNEXPECTED_ERROR(__FILE__, __LINE__,
					     "get interface configuration: %s",
						 NTstrerror(error));
				result = ISC_R_UNEXPECTED;
				goto ioctl_failure;
			}
			/*
			 * EINVAL.  Retry with a bigger buffer.
			 */
		} else {
			/*
			 * The WSAIoctl succeeded.
			 * If the number of the returned bytes is the same
			 * as the buffer size, we will grow it just in
			 * case and retry.
			 */
			if (bytesReturned > 0 &&
			    (bytesReturned < iter->bufsize))
				break;
		}
		if (iter->bufsize >= IFCONF_SIZE_MAX*sizeof(INTERFACE_INFO)) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "get interface configuration: "
					 "maximum buffer size exceeded");
			result = ISC_R_UNEXPECTED;
			goto ioctl_failure;
		}
		isc_mem_put(mctx, iter->buf, iter->bufsize);

		iter->bufsize += IFCONF_SIZE_INCREMENT *
			sizeof(INTERFACE_INFO);
	}

	/*
	 * A newly created iterator has an undefined position
	 * until isc_interfaceiter_first() is called.
	 */
	iter->pos = NULL;
	iter->result = ISC_R_FAILURE;
	iter->numIF = 0;
	iter->totalIF = bytesReturned/sizeof(INTERFACE_INFO);


	iter->magic = IFITER_MAGIC;
	*iterp = iter;
	/* We don't need the socket any more, so close it */
	closesocket(iter->socket);
	return (ISC_R_SUCCESS);

 ioctl_failure:
	isc_mem_put(mctx, iter->buf, iter->bufsize);

 alloc_failure:
	(void) closesocket(iter->socket);

 socket_failure:
	isc_mem_put(mctx, iter, sizeof *iter);
	return (result);
}

/*
 * Get information about the current interface to iter->current.
 * If successful, return ISC_R_SUCCESS.
 * If the interface has an unsupported address family, or if
 * some operation on it fails, return ISC_R_IGNORE to make
 * the higher-level iterator code ignore it.
 */

static isc_result_t
internal_current(isc_interfaceiter_t *iter, int family) {
	BOOL ifNamed = FALSE;
	unsigned long flags;

	REQUIRE(VALID_IFITER(iter));
	REQUIRE(iter->numIF >= 0);

	memset(&iter->current, 0, sizeof(iter->current));
	iter->current.af = family;

	get_addr(family, &iter->current.address,
		 (struct sockaddr *)&(iter->IFData.iiAddress));

	/*
	 * Get interface flags.
	 */

	iter->current.flags = 0;
	flags = iter->IFData.iiFlags;

	if ((flags & IFF_UP) != 0)
		iter->current.flags |= INTERFACE_F_UP;

	if ((flags & IFF_POINTTOPOINT) != 0) {
		iter->current.flags |= INTERFACE_F_POINTTOPOINT;
		sprintf(iter->current.name, "PPP Interface %d", iter->numIF);
		ifNamed = TRUE;
	}

	if ((flags & IFF_LOOPBACK) != 0) {
		iter->current.flags |= INTERFACE_F_LOOPBACK;
		sprintf(iter->current.name, "Loopback Interface %d",
			iter->numIF);
		ifNamed = TRUE;
	}

	/*
	 * If the interface is point-to-point, get the destination address.
	 */
	if ((iter->current.flags & INTERFACE_F_POINTTOPOINT) != 0) {
		get_addr(family, &iter->current.dstaddress,
		(struct sockaddr *)&(iter->IFData.iiBroadcastAddress));
	}

	if (ifNamed == FALSE)
		sprintf(iter->current.name,
			"TCP/IP Interface %d", iter->numIF);

	/*
	 * Get the network mask.
	 */
	switch (family) {
	case AF_INET:
		get_addr(family, &iter->current.netmask,
			 (struct sockaddr *)&(iter->IFData.iiNetmask));
		break;
	case AF_INET6:
		break;
	}

	return (ISC_R_SUCCESS);
}

/*
 * Step the iterator to the next interface.  Unlike
 * isc_interfaceiter_next(), this may leave the iterator
 * positioned on an interface that will ultimately
 * be ignored.  Return ISC_R_NOMORE if there are no more
 * interfaces, otherwise ISC_R_SUCCESS.
 */
static isc_result_t
internal_next(isc_interfaceiter_t *iter) {
	if (iter->numIF >= iter->totalIF)
		return (ISC_R_NOMORE);

	/*
	 * The first one needs to be set up to point to the last
	 * Element of the array.  Go to the end and back up
	 * Microsoft's implementation is peculiar for returning
	 * the list in reverse order
	 */
	 
	if (iter->numIF == 0)
		iter->pos = (INTERFACE_INFO *)(iter->buf + (iter->totalIF));

	iter->pos--;
	if (&(iter->pos) < &(iter->buf))
		return (ISC_R_NOMORE);

	memset(&(iter->IFData), 0, sizeof(INTERFACE_INFO));
	memcpy(&(iter->IFData), iter->pos, sizeof(INTERFACE_INFO));
	iter->numIF++;

	return (ISC_R_SUCCESS);
}

isc_result_t
isc_interfaceiter_current(isc_interfaceiter_t *iter,
			  isc_interface_t *ifdata) {
	REQUIRE(iter->result == ISC_R_SUCCESS);
	memcpy(ifdata, &iter->current, sizeof(*ifdata));
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_interfaceiter_first(isc_interfaceiter_t *iter) {
	isc_result_t result;

	REQUIRE(VALID_IFITER(iter));

	iter->numIF = 0;
	for (;;) {
		result = internal_next(iter);
		if (result != ISC_R_SUCCESS)
			break;
		result = internal_current(iter, AF_INET);
		if (result != ISC_R_IGNORE)
			break;
	}
	iter->result = result;
	return (result);
}

isc_result_t
isc_interfaceiter_next(isc_interfaceiter_t *iter) {
	isc_result_t result;

	REQUIRE(VALID_IFITER(iter));
	REQUIRE(iter->result == ISC_R_SUCCESS);

	for (;;) {
		result = internal_next(iter);
		if (result != ISC_R_SUCCESS)
			break;
		result = internal_current(iter,AF_INET);
		if (result != ISC_R_IGNORE)
			break;
	}
	iter->result = result;
	return (result);
}

void
isc_interfaceiter_destroy(isc_interfaceiter_t **iterp) {
	isc_interfaceiter_t *iter;
	REQUIRE(iterp != NULL);
	iter = *iterp;
	REQUIRE(VALID_IFITER(iter));

	isc_mem_put(iter->mctx, iter->buf, iter->bufsize);

	iter->magic = 0;
	isc_mem_put(iter->mctx, iter, sizeof *iter);
	*iterp = NULL;
}

