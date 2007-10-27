/*	$OpenBSD: linux_error.c,v 1.4 2007/10/27 22:42:11 miod Exp $	*/
/*	$NetBSD: linux_error.c,v 1.2 1995/04/22 19:48:32 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/errno.h>
#include <compat/linux/linux_errno.h>

int linux_error[1 + ELAST] = {
	0,
	-LINUX_EPERM,
	-LINUX_ENOENT,
	-LINUX_ESRCH,
	-LINUX_EINTR,
	-LINUX_EIO,
	-LINUX_ENXIO,
	-LINUX_E2BIG,
	-LINUX_ENOEXEC,
	-LINUX_EBADF,
	-LINUX_ECHILD,
	-LINUX_EDEADLK,
	-LINUX_ENOMEM,
	-LINUX_EACCES,
	-LINUX_EFAULT,
	-LINUX_ENOTBLK,
	-LINUX_EBUSY,
	-LINUX_EEXIST,
	-LINUX_EXDEV,
	-LINUX_ENODEV,
	-LINUX_ENOTDIR,
	-LINUX_EISDIR,
	-LINUX_EINVAL,
	-LINUX_ENFILE,
	-LINUX_EMFILE,
	-LINUX_ENOTTY,
	-LINUX_ETXTBSY,
	-LINUX_EFBIG,
	-LINUX_ENOSPC,
	-LINUX_ESPIPE,
	-LINUX_EROFS,
	-LINUX_EMLINK,
	-LINUX_EPIPE,
	-LINUX_EDOM,
	-LINUX_ERANGE,
	-LINUX_EAGAIN,
	-LINUX_EINPROGRESS,
	-LINUX_EALREADY,
	-LINUX_ENOTSOCK,
	-LINUX_EDESTADDRREQ,
	-LINUX_EMSGSIZE,
	-LINUX_EPROTOTYPE,
	-LINUX_ENOPROTOOPT,
	-LINUX_EPROTONOSUPPORT,
	-LINUX_ESOCKTNOSUPPORT,
	-LINUX_EOPNOTSUPP,
	-LINUX_EPFNOSUPPORT,
	-LINUX_EAFNOSUPPORT,
	-LINUX_EADDRINUSE,
	-LINUX_EADDRNOTAVAIL,
	-LINUX_ENETDOWN,
	-LINUX_ENETUNREACH,
	-LINUX_ENETRESET,
	-LINUX_ECONNABORTED,
	-LINUX_ECONNRESET,
	-LINUX_ENOBUFS,
	-LINUX_EISCONN,
	-LINUX_ENOTCONN,
	-LINUX_ESHUTDOWN,
	-LINUX_ETOOMANYREFS,
	-LINUX_ETIMEDOUT,
	-LINUX_ECONNREFUSED,
	-LINUX_ELOOP,
	-LINUX_ENAMETOOLONG,
	-LINUX_EHOSTDOWN,
	-LINUX_EHOSTUNREACH,
	-LINUX_ENOTEMPTY,
	-LINUX_ENOSYS,		/* not mapped (EPROCLIM) */
	-LINUX_EUSERS,
	-LINUX_EDQUOT,
	-LINUX_ESTALE,
	-LINUX_EREMOTE,
	-LINUX_ENOSYS,		/* not mapped (EBADRPC) */
	-LINUX_ENOSYS,		/* not mapped (ERPCMISMATCH) */
	-LINUX_ENOSYS,		/* not mapped (EPROGUNAVAIL) */
	-LINUX_ENOSYS,		/* not mapped (EPROGMISMATCH) */
	-LINUX_ENOSYS,		/* not mapped (EPROCUNAVAIL) */
	-LINUX_ENOLCK,
	-LINUX_ENOSYS,
	-LINUX_ENOSYS,		/* not mapped (EFTYPE) */
	-LINUX_ENOSYS,		/* not mapped (EAUTH) */
	-LINUX_ENOSYS,		/* not mapped (ENEEDAUTH) */
	-LINUX_ENOSYS,		/* not mapped (EIPSEC) */
	-LINUX_EOPNOTSUPP,	/* what is ENOATTR? */
	-LINUX_EILSEQ,
	-LINUX_ENOSYS,		/* not mapped (ENOMEDIUM) */
	-LINUX_ENOSYS,		/* not mapped (EMEDIUMTYPE) */
	-LINUX_EOVERFLOW,
	-LINUX_ENOSYS,		/* not mapped (ECANCELED) */
	-LINUX_EIDRM,
	-LINUX_ENOMSG
};
