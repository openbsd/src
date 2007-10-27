/* $OpenBSD: osf1_cvt_errno.c,v 1.2 2007/10/27 22:42:11 miod Exp $ */
/* $NetBSD: osf1_cvt_errno.c,v 1.4 1999/05/01 02:16:01 cgd Exp $ */

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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
#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_cvt.h>

/*
 * This table is used to translate NetBSD errnos to OSF/1 errnos
 * when returning from a system call.
 *
 * It is up to date as of Digital UNIX V4.0 and NetBSD 1.4.
 */

const int osf1_errno_rxlist[1 + ELAST] = {
    0,
    OSF1_EPERM,			/* EPERM (1) -> 1 */
    OSF1_ENOENT,		/* ENOENT (2) -> 2 */
    OSF1_ESRCH,			/* ESRCH (3) -> 3 */
    OSF1_EINTR,			/* EINTR (4) -> 4 */
    OSF1_EIO,			/* EIO (5) -> 5 */
    OSF1_ENXIO,			/* ENXIO (6) -> 6 */
    OSF1_E2BIG,			/* E2BIG (7) -> 7 */
    OSF1_ENOEXEC,		/* ENOEXEC (8) -> 8 */
    OSF1_EBADF,			/* EBADF (9) -> 9 */
    OSF1_ECHILD,		/* ECHILD (10) -> 10 */
    OSF1_EDEADLK,		/* EDEADLK (11) -> 11 */
    OSF1_ENOMEM,		/* ENOMEM (12) -> 12 */
    OSF1_EACCES,		/* EACCES (13) -> 13 */
    OSF1_EFAULT,		/* EFAULT (14) -> 14 */
    OSF1_ENOTBLK,		/* ENOTBLK (15) -> 15 */
    OSF1_EBUSY,			/* EBUSY (16) -> 16 */
    OSF1_EEXIST,		/* EEXIST (17) -> 17 */
    OSF1_EXDEV,			/* EXDEV (18) -> 18 */
    OSF1_ENODEV,		/* ENODEV (19) -> 19 */
    OSF1_ENOTDIR,		/* ENOTDIR (20) -> 20 */
    OSF1_EISDIR,		/* EISDIR (21) -> 21 */
    OSF1_EINVAL,		/* EINVAL (22) -> 22 */
    OSF1_ENFILE,		/* ENFILE (23) -> 23 */
    OSF1_EMFILE,		/* EMFILE (24) -> 24 */
    OSF1_ENOTTY,		/* ENOTTY (25) -> 25 */
    OSF1_ETXTBSY,		/* ETXTBSY (26) -> 26 */
    OSF1_EFBIG,			/* EFBIG (27) -> 27 */
    OSF1_ENOSPC,		/* ENOSPC (28) -> 28 */
    OSF1_ESPIPE,		/* ESPIPE (29) -> 29 */
    OSF1_EROFS,			/* EROFS (30) -> 30 */
    OSF1_EMLINK,		/* EMLINK (31) -> 31 */
    OSF1_EPIPE,			/* EPIPE (32) -> 32 */
    OSF1_EDOM,			/* EDOM (33) -> 33 */
    OSF1_ERANGE,		/* ERANGE (34) -> 34 */
    OSF1_EWOULDBLOCK,		/* EAGAIN (35) -> OSF1_EWOULDBLOCK (35) */
    OSF1_EINPROGRESS,		/* EINPROGRESS (36) -> 36 */
    OSF1_EALREADY,		/* EALREADY (37) -> 37 */
    OSF1_ENOTSOCK,		/* ENOTSOCK (38) -> 38 */
    OSF1_EDESTADDRREQ,		/* EDESTADDRREQ (39) -> 39 */
    OSF1_EMSGSIZE,		/* EMSGSIZE (40) -> 40 */
    OSF1_EPROTOTYPE,		/* EPROTOTYPE (41) -> 41 */
    OSF1_ENOPROTOOPT,		/* ENOPROTOOPT (42) -> 42 */
    OSF1_EPROTONOSUPPORT,	/* EPROTONOSUPPORT (43) -> 43 */
    OSF1_ESOCKTNOSUPPORT,	/* ESOCKTNOSUPPORT (44) -> 44 */
    OSF1_EOPNOTSUPP,		/* EOPNOTSUPP (45) -> 45 */
    OSF1_EPFNOSUPPORT,		/* EPFNOSUPPORT (46) -> 46 */
    OSF1_EAFNOSUPPORT,		/* EAFNOSUPPORT (47) -> 47 */
    OSF1_EADDRINUSE,		/* EADDRINUSE (48) -> 48 */
    OSF1_EADDRNOTAVAIL,		/* EADDRNOTAVAIL (49) -> 49 */
    OSF1_ENETDOWN,		/* ENETDOWN (50) -> 50 */
    OSF1_ENETUNREACH,		/* ENETUNREACH (51) -> 51 */
    OSF1_ENETRESET,		/* ENETRESET (52) -> 52 */
    OSF1_ECONNABORTED,		/* ECONNABORTED (53) -> 53 */
    OSF1_ECONNRESET,		/* ECONNRESET (54) -> 54 */
    OSF1_ENOBUFS,		/* ENOBUFS (55) -> 55 */
    OSF1_EISCONN,		/* EISCONN (56) -> 56 */
    OSF1_ENOTCONN,		/* ENOTCONN (57) -> 57 */
    OSF1_ESHUTDOWN,		/* ESHUTDOWN (58) -> 58 */
    OSF1_ETOOMANYREFS,		/* ETOOMANYREFS (59) -> 59 */
    OSF1_ETIMEDOUT,		/* ETIMEDOUT (60) -> 60 */
    OSF1_ECONNREFUSED,		/* ECONNREFUSED (61) -> 61 */
    OSF1_ELOOP,			/* ELOOP (62) -> 62 */
    OSF1_ENAMETOOLONG,		/* ENAMETOOLONG (63) -> 63 */
    OSF1_EHOSTDOWN,		/* EHOSTDOWN (64) -> 64 */
    OSF1_EHOSTUNREACH,		/* EHOSTUNREACH (65) -> 65 */
    OSF1_ENOTEMPTY,		/* ENOTEMPTY (66) -> 66 */
    OSF1_EPROCLIM,		/* EPROCLIM (67) -> 67 */
    OSF1_EUSERS,		/* EUSERS (68) -> 68 */
    OSF1_EDQUOT,		/* EDQUOT (69) -> 69 */
    OSF1_ESTALE,		/* ESTALE (70) -> 70 */
    OSF1_EREMOTE,		/* EREMOTE (71) -> 71 */
    OSF1_EBADRPC,		/* EBADRPC (72) -> 72 */
    OSF1_ERPCMISMATCH,		/* ERPCMISMATCH (73) -> 73 */
    OSF1_EPROGUNAVAIL,		/* EPROGUNAVAIL (74) -> 74 */
    OSF1_EPROGMISMATCH,		/* EPROGMISMATCH (75) -> 75 */
    OSF1_EPROCUNAVAIL,		/* EPROCUNAVAIL (76) -> 76 */
    OSF1_ENOLCK,		/* ENOLCK (77) -> 77 */
    OSF1_ENOSYS,		/* ENOSYS (78) -> 78 */
    OSF1_EFTYPE,		/* EFTYPE (79) -> 79 */
    OSF1_ENOSYS,		/* EAUTH (80) has no equivalent */
    OSF1_ENOSYS,		/* ENEEDAUTH (81) has no equivalent */
    OSF1_ENOSYS,		/* EIPSEC (82) has no equivalent */
    OSF1_ENOSYS,		/* ENOATTR (83) has no equivalent */
    OSF1_EILSEQ,		/* EILSEQ (84) -> 116 */
    OSF1_ENOSYS,		/* ENOMEDIUM (85) has no equivalent */
    OSF1_ENOSYS,		/* EMEDIUMTYPE (86) has no equivalent */
    OSF1_EOVERFLOW,		/* EOVERFLOW (87) -> 103 */
    OSF1_ECANCELED,		/* ECANCELED (88) -> 94 */
    OSF1_EIDRM,			/* EIDRM (89) -> 81 */
    OSF1_ENOMSG			/* ENOMSG (90) -> 80 */
};
