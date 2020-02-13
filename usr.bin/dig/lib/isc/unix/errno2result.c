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

/* $Id: errno2result.c,v 1.4 2020/02/13 21:34:06 jung Exp $ */

/*! \file */

#include <errno.h>
#include <string.h>

#include <isc/result.h>
#include <isc/util.h>

#include "errno2result.h"

/*%
 * Convert a POSIX errno value into an isc_result_t.  The
 * list of supported errno values is not complete; new users
 * of this function should add any expected errors that are
 * not already there.
 */
isc_result_t
isc___errno2result(int posixerrno, isc_boolean_t dolog,
		   const char *file, unsigned int line)
{
	switch (posixerrno) {
	case ENOTDIR:
	case ELOOP:
	case EINVAL:		/* XXX sometimes this is not for files */
	case ENAMETOOLONG:
	case EBADF:
		return (ISC_R_INVALIDFILE);
	case ENOENT:
		return (ISC_R_FILENOTFOUND);
	case EACCES:
	case EPERM:
		return (ISC_R_NOPERM);
	case EEXIST:
		return (ISC_R_FILEEXISTS);
	case EIO:
		return (ISC_R_IOERROR);
	case ENOMEM:
		return (ISC_R_NOMEMORY);
	case ENFILE:
	case EMFILE:
		return (ISC_R_TOOMANYOPENFILES);
	case EOVERFLOW:
		return (ISC_R_RANGE);
	case EPIPE:
	case ECONNRESET:
	case ECONNABORTED:
		return (ISC_R_CONNECTIONRESET);
	case ENOTCONN:
		return (ISC_R_NOTCONNECTED);
	case ETIMEDOUT:
		return (ISC_R_TIMEDOUT);
	case ENOBUFS:
		return (ISC_R_NORESOURCES);
	case EAFNOSUPPORT:
		return (ISC_R_FAMILYNOSUPPORT);
	case ENETDOWN:
		return (ISC_R_NETDOWN);
	case EHOSTDOWN:
		return (ISC_R_HOSTDOWN);
	case ENETUNREACH:
		return (ISC_R_NETUNREACH);
	case EHOSTUNREACH:
		return (ISC_R_HOSTUNREACH);
	case EADDRINUSE:
		return (ISC_R_ADDRINUSE);
	case EADDRNOTAVAIL:
		return (ISC_R_ADDRNOTAVAIL);
	case ECONNREFUSED:
		return (ISC_R_CONNREFUSED);
	default:
		if (dolog) {
			UNEXPECTED_ERROR(file, line, "unable to convert errno "
					 "to isc_result: %d: %s",
					 posixerrno, strerror(posixerrno));
		}
		/*
		 * XXXDCL would be nice if perhaps this function could
		 * return the system's error string, so the caller
		 * might have something more descriptive than "unexpected
		 * error" to log with.
		 */
		return (ISC_R_UNEXPECTED);
	}
}
