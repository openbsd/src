/* $OpenBSD: osf1_errno.h,v 1.1 2000/08/04 15:47:54 ericj Exp $ */
/* $NetBSD: osf1_errno.h,v 1.2 1999/04/23 18:00:34 cgd Exp $ */

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

/*
 * OSF/1 error number definitions, as described by the Digital UNIX V4.0
 * <sys/errno.h>.
 */

#ifndef _COMPAT_OSF1_OSF1_ERRNO_H_
#define _COMPAT_OSF1_OSF1_ERRNO_H_

#define OSF1_ESUCCESS		0
#define OSF1_EPERM		1
#define OSF1_ENOENT		2
#define OSF1_ESRCH		3
#define OSF1_EINTR		4
#define OSF1_EIO		5
#define OSF1_ENXIO		6
#define OSF1_E2BIG		7
#define OSF1_ENOEXEC		8
#define OSF1_EBADF		9
#define OSF1_ECHILD		10
#define OSF1_EDEADLK		11
#define OSF1_ENOMEM		12
#define OSF1_EACCES		13
#define OSF1_EFAULT		14
#define OSF1_ENOTBLK		15
#define OSF1_EBUSY		16
#define OSF1_EEXIST		17
#define OSF1_EXDEV		18
#define OSF1_ENODEV		19
#define OSF1_ENOTDIR		20
#define OSF1_EISDIR		21
#define OSF1_EINVAL		22
#define OSF1_ENFILE		23
#define OSF1_EMFILE		24
#define OSF1_ENOTTY		25
#define OSF1_ETXTBSY		26
#define OSF1_EFBIG		27
#define OSF1_ENOSPC		28
#define OSF1_ESPIPE		29
#define OSF1_EROFS		30
#define OSF1_EMLINK		31
#define OSF1_EPIPE		32
#define OSF1_EDOM		33
#define OSF1_ERANGE		34
#define OSF1_EWOULDBLOCK	35
#define OSF1_EINPROGRESS	36
#define OSF1_EALREADY		37
#define OSF1_ENOTSOCK		38
#define OSF1_EDESTADDRREQ	39
#define OSF1_EMSGSIZE		40
#define OSF1_EPROTOTYPE		41
#define OSF1_ENOPROTOOPT	42
#define OSF1_EPROTONOSUPPORT	43
#define OSF1_ESOCKTNOSUPPORT	44
#define OSF1_EOPNOTSUPP		45
#define OSF1_EPFNOSUPPORT	46
#define OSF1_EAFNOSUPPORT	47
#define OSF1_EADDRINUSE		48
#define OSF1_EADDRNOTAVAIL	49
#define OSF1_ENETDOWN		50
#define OSF1_ENETUNREACH	51
#define OSF1_ENETRESET		52
#define OSF1_ECONNABORTED	53
#define OSF1_ECONNRESET		54
#define OSF1_ENOBUFS		55
#define OSF1_EISCONN		56
#define OSF1_ENOTCONN		57
#define OSF1_ESHUTDOWN		58
#define OSF1_ETOOMANYREFS	59
#define OSF1_ETIMEDOUT		60
#define OSF1_ECONNREFUSED	61
#define OSF1_ELOOP		62
#define OSF1_ENAMETOOLONG	63
#define OSF1_EHOSTDOWN		64
#define OSF1_EHOSTUNREACH	65
#define OSF1_ENOTEMPTY		66
#define OSF1_EPROCLIM		67
#define OSF1_EUSERS		68
#define OSF1_EDQUOT		69
#define OSF1_ESTALE		70
#define OSF1_EREMOTE		71
#define OSF1_EBADRPC		72
#define OSF1_ERPCMISMATCH	73
#define OSF1_EPROGUNAVAIL	74
#define OSF1_EPROGMISMATCH	75
#define OSF1_EPROCUNAVAIL	76
#define OSF1_ENOLCK		77
#define OSF1_ENOSYS		78
#define OSF1_EFTYPE		79
#define OSF1_ENOMSG		80
#define OSF1_EIDRM		81
#define OSF1_ENOSR		82
#define OSF1_ETIME		83
#define OSF1_EBADMSG		84
#define OSF1_EPROTO		85
#define OSF1_ENODATA		86
#define OSF1_ENOSTR		87
#define OSF1_ECLONEME		88
#define OSF1_EDIRTY		89
#define OSF1_EDUPPKG		90
#define OSF1_EVERSION		91
#define OSF1_ENOPKG		92
#define OSF1_ENOSYM		93
#define OSF1_ECANCELED		94
#define OSF1_EFAIL		95
#define OSF1_EINPROG		97
#define OSF1_EMTIMERS		98
#define OSF1_ENOTSUP		99
#define OSF1_EAIO		100
#define OSF1_EMULTIHOP		101
#define OSF1_ENOLINK		102
#define OSF1_EOVERFLOW		103
#define OSF1_EILSEQ		116
#define OSF1_ESOFT		123
#define OSF1_EMEDIA		124

#endif /* _COMPAT_OSF1_OSF1_ERRNO_H_ */
