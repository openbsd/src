/*	$OpenBSD: systrace-error.c,v 1.2 2002/12/05 19:39:27 fgsch Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BU
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TOR
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "intercept.h"
#include "systrace.h"
#include "systrace-errno.h"

struct systrace_error {
	char *name;
	int errno;
} systrace_errors[] = {
	{ "EPERM", SYSTRACE_EPERM },
	{ "ENOENT", SYSTRACE_ENOENT },
	{ "ESRCH", SYSTRACE_ESRCH },
	{ "EINTR", SYSTRACE_EINTR },
	{ "EIO", SYSTRACE_EIO },
	{ "ENXIO", SYSTRACE_ENXIO },
	{ "E2BIG", SYSTRACE_E2BIG },
	{ "ENOEXEC", SYSTRACE_ENOEXEC },
	{ "EBADF", SYSTRACE_EBADF },
	{ "ECHILD", SYSTRACE_ECHILD },
	{ "EDEADLK", SYSTRACE_EDEADLK },
	{ "ENOMEM", SYSTRACE_ENOMEM },
	{ "EACCES", SYSTRACE_EACCES },
	{ "EFAULT", SYSTRACE_EFAULT },
	{ "ENOTBLK", SYSTRACE_ENOTBLK },
	{ "EBUSY", SYSTRACE_EBUSY },
	{ "EEXIST", SYSTRACE_EEXIST },
	{ "EXDEV", SYSTRACE_EXDEV },
	{ "ENODEV", SYSTRACE_ENODEV },
	{ "ENOTDIR", SYSTRACE_ENOTDIR },
	{ "EISDIR", SYSTRACE_EISDIR },
	{ "EINVAL", SYSTRACE_EINVAL },
	{ "ENFILE", SYSTRACE_ENFILE },
	{ "EMFILE", SYSTRACE_EMFILE },
	{ "ENOTTY", SYSTRACE_ENOTTY },
	{ "ETXTBSY", SYSTRACE_ETXTBSY },
	{ "EFBIG", SYSTRACE_EFBIG },
	{ "ENOSPC", SYSTRACE_ENOSPC },
	{ "ESPIPE", SYSTRACE_ESPIPE },
	{ "EROFS", SYSTRACE_EROFS },
	{ "EMLINK", SYSTRACE_EMLINK },
	{ "EPIPE", SYSTRACE_EPIPE },
	{ "EDOM", SYSTRACE_EDOM },
	{ "ERANGE", SYSTRACE_ERANGE },
	{ "EAGAIN", SYSTRACE_EAGAIN },
	{ "EWOULDBLOCK", SYSTRACE_EWOULDBLOCK },
	{ "EINPROGRESS", SYSTRACE_EINPROGRESS },
	{ "EALREADY", SYSTRACE_EALREADY },
	{ "ENOTSOCK", SYSTRACE_ENOTSOCK },
	{ "EDESTADDRREQ", SYSTRACE_EDESTADDRREQ },
	{ "EMSGSIZE", SYSTRACE_EMSGSIZE },
	{ "EPROTOTYPE", SYSTRACE_EPROTOTYPE },
	{ "ENOPROTOOPT", SYSTRACE_ENOPROTOOPT },
	{ "EPROTONOSUPPORT", SYSTRACE_EPROTONOSUPPORT },
	{ "ESOCKTNOSUPPORT", SYSTRACE_ESOCKTNOSUPPORT },
	{ "EOPNOTSUPP", SYSTRACE_EOPNOTSUPP },
	{ "EPFNOSUPPORT", SYSTRACE_EPFNOSUPPORT },
	{ "EAFNOSUPPORT", SYSTRACE_EAFNOSUPPORT },
	{ "EADDRINUSE", SYSTRACE_EADDRINUSE },
	{ "EADDRNOTAVAIL", SYSTRACE_EADDRNOTAVAIL },
	{ "ENETDOWN", SYSTRACE_ENETDOWN },
	{ "ENETUNREACH", SYSTRACE_ENETUNREACH },
	{ "ENETRESET", SYSTRACE_ENETRESET },
	{ "ECONNABORTED", SYSTRACE_ECONNABORTED },
	{ "ECONNRESET", SYSTRACE_ECONNRESET },
	{ "ENOBUFS", SYSTRACE_ENOBUFS },
	{ "EISCONN", SYSTRACE_EISCONN },
	{ "ENOTCONN", SYSTRACE_ENOTCONN },
	{ "ESHUTDOWN", SYSTRACE_ESHUTDOWN },
	{ "ETOOMANYREFS", SYSTRACE_ETOOMANYREFS },
	{ "ETIMEDOUT", SYSTRACE_ETIMEDOUT },
	{ "ECONNREFUSED", SYSTRACE_ECONNREFUSED },
	{ "ELOOP", SYSTRACE_ELOOP },
	{ "ENAMETOOLONG", SYSTRACE_ENAMETOOLONG },
	{ "EHOSTDOWN", SYSTRACE_EHOSTDOWN },
	{ "EHOSTUNREACH", SYSTRACE_EHOSTUNREACH },
	{ "ENOTEMPTY", SYSTRACE_ENOTEMPTY },
	{ "EPROCLIM", SYSTRACE_EPROCLIM },
	{ "EUSERS", SYSTRACE_EUSERS },
	{ "EDQUOT", SYSTRACE_EDQUOT },
	{ "ESTALE", SYSTRACE_ESTALE },
	{ "EREMOTE", SYSTRACE_EREMOTE },
	{ "EBADRPC", SYSTRACE_EBADRPC },
	{ "ERPCMISMATCH", SYSTRACE_ERPCMISMATCH },
	{ "EPROGUNAVAIL", SYSTRACE_EPROGUNAVAIL },
	{ "EPROGMISMATCH", SYSTRACE_EPROGMISMATCH },
	{ "EPROCUNAVAIL", SYSTRACE_EPROCUNAVAIL },
	{ "ENOLCK", SYSTRACE_ENOLCK },
	{ "ENOSYS", SYSTRACE_ENOSYS },
	{ "EFTYPE", SYSTRACE_EFTYPE },
	{ "EAUTH", SYSTRACE_EAUTH },
	{ "ENEEDAUTH", SYSTRACE_ENEEDAUTH },
	{ "EIPSEC", SYSTRACE_EIPSEC },
	{ "ELAST", SYSTRACE_ELAST },
	{ NULL, 0}
};

int
systrace_error_translate(char *name)
{
	struct systrace_error *error = systrace_errors;

	while (error->name != NULL) {
		if (!strcasecmp(error->name, name))
			return (error->errno);
		error++;
	}

	return (-1);
}
