/*
 * Copyright (c) 2003, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* $arla: uae.h,v 1.4 2003/03/10 01:57:16 lha Exp $ */

/*
 * Unified AFS errnos
 */

#ifndef UAE_HEADER_H
#define UAE_HEADER_H 1

#define UAE_ERROR_base			49733376
#define UAE_ERROR_EPERM			(UAE_ERROR_base + 1)
#define UAE_ERROR_ENOENT		(UAE_ERROR_base + 2)
#define UAE_ERROR_ESRCH			(UAE_ERROR_base + 3)
#define UAE_ERROR_EINTR			(UAE_ERROR_base + 4)
#define UAE_ERROR_EIO			(UAE_ERROR_base + 5)
#define UAE_ERROR_ENXIO			(UAE_ERROR_base + 6)
#define UAE_ERROR_E2BIG			(UAE_ERROR_base + 7)
#define UAE_ERROR_ENOEXEC		(UAE_ERROR_base + 8)
#define UAE_ERROR_EBADF			(UAE_ERROR_base + 9)
#define UAE_ERROR_ECHILD		(UAE_ERROR_base + 10)
#define UAE_ERROR_EAGAIN		(UAE_ERROR_base + 11)
#define UAE_ERROR_ENOMEM		(UAE_ERROR_base + 12)
#define UAE_ERROR_EACCES		(UAE_ERROR_base + 13)
#define UAE_ERROR_EFAULT		(UAE_ERROR_base + 14)
#define UAE_ERROR_ENOTBLK		(UAE_ERROR_base + 15)
#define UAE_ERROR_EBUSY			(UAE_ERROR_base + 16)
#define UAE_ERROR_EEXIST		(UAE_ERROR_base + 17)
#define UAE_ERROR_EXDEV			(UAE_ERROR_base + 18)
#define UAE_ERROR_ENODEV		(UAE_ERROR_base + 19)
#define UAE_ERROR_ENOTDIR		(UAE_ERROR_base + 20)
#define UAE_ERROR_EISDIR		(UAE_ERROR_base + 21)
#define UAE_ERROR_EINVAL		(UAE_ERROR_base + 22)
#define UAE_ERROR_ENFILE		(UAE_ERROR_base + 23)
#define UAE_ERROR_EMFILE		(UAE_ERROR_base + 24)
#define UAE_ERROR_ENOTTY		(UAE_ERROR_base + 25)
#define UAE_ERROR_ETXTBSY		(UAE_ERROR_base + 26)
#define UAE_ERROR_EFBIG			(UAE_ERROR_base + 27)
#define UAE_ERROR_ENOSPC		(UAE_ERROR_base + 28)
#define UAE_ERROR_ESPIPE		(UAE_ERROR_base + 29)
#define UAE_ERROR_EROFS			(UAE_ERROR_base + 30)
#define UAE_ERROR_EMLINK		(UAE_ERROR_base + 31)
#define UAE_ERROR_EPIPE			(UAE_ERROR_base + 32)
#define UAE_ERROR_EDOM			(UAE_ERROR_base + 33)
#define UAE_ERROR_ERANGE		(UAE_ERROR_base + 34)
#define UAE_ERROR_EDEADLK		(UAE_ERROR_base + 35)
#define UAE_ERROR_ENAMETOOLONG		(UAE_ERROR_base + 36)
#define UAE_ERROR_ENOLCK		(UAE_ERROR_base + 37)
#define UAE_ERROR_ENOSYS		(UAE_ERROR_base + 38)
#define UAE_ERROR_ENOTEMPTY		(UAE_ERROR_base + 39)
#define UAE_ERROR_ELOOP			(UAE_ERROR_base + 40)
#define UAE_ERROR_EWOULDBLOCK		(UAE_ERROR_base + 41)
#define UAE_ERROR_ENOMSG		(UAE_ERROR_base + 42)
#define UAE_ERROR_EIDRM			(UAE_ERROR_base + 43)
#define UAE_ERROR_ECHRNG		(UAE_ERROR_base + 44)
#define UAE_ERROR_EL2NSYNC		(UAE_ERROR_base + 45)
#define UAE_ERROR_EL3HLT		(UAE_ERROR_base + 46)
#define UAE_ERROR_EL3RST		(UAE_ERROR_base + 47)
#define UAE_ERROR_ELNRNG		(UAE_ERROR_base + 48)
#define UAE_ERROR_EUNATCH		(UAE_ERROR_base + 49)
#define UAE_ERROR_ENOCSI		(UAE_ERROR_base + 50)
#define UAE_ERROR_EL2HLT		(UAE_ERROR_base + 51)
#define UAE_ERROR_EBADE			(UAE_ERROR_base + 52)
#define UAE_ERROR_EBADR			(UAE_ERROR_base + 53)
#define UAE_ERROR_EXFULL		(UAE_ERROR_base + 54)
#define UAE_ERROR_ENOANO		(UAE_ERROR_base + 55)
#define UAE_ERROR_EBADRQC		(UAE_ERROR_base + 56)
#define UAE_ERROR_EBADSLT		(UAE_ERROR_base + 57)
#define UAE_ERROR_EBFONT		(UAE_ERROR_base + 58)
#define UAE_ERROR_ENOSTR		(UAE_ERROR_base + 59)
#define UAE_ERROR_ENODATA		(UAE_ERROR_base + 60)
#define UAE_ERROR_ETIME			(UAE_ERROR_base + 61)
#define UAE_ERROR_ENOSR			(UAE_ERROR_base + 62)
#define UAE_ERROR_ENONET		(UAE_ERROR_base + 63)
#define UAE_ERROR_ENOPKG		(UAE_ERROR_base + 64)
#define UAE_ERROR_EREMOTE		(UAE_ERROR_base + 65)
#define UAE_ERROR_ENOLINK		(UAE_ERROR_base + 66)
#define UAE_ERROR_EADV			(UAE_ERROR_base + 67)
#define UAE_ERROR_ESRMNT		(UAE_ERROR_base + 68)
#define UAE_ERROR_ECOMM			(UAE_ERROR_base + 69)
#define UAE_ERROR_EPROTO		(UAE_ERROR_base + 70)
#define UAE_ERROR_EMULTIHOP		(UAE_ERROR_base + 71)
#define UAE_ERROR_EDOTDOT		(UAE_ERROR_base + 72)
#define UAE_ERROR_EBADMSG		(UAE_ERROR_base + 73)
#define UAE_ERROR_EOVERFLOW		(UAE_ERROR_base + 74)
#define UAE_ERROR_ENOTUNIQ		(UAE_ERROR_base + 75)
#define UAE_ERROR_EBADFD		(UAE_ERROR_base + 76)
#define UAE_ERROR_EREMCHG		(UAE_ERROR_base + 77)
#define UAE_ERROR_ELIBACC		(UAE_ERROR_base + 78)
#define UAE_ERROR_ELIBBAD		(UAE_ERROR_base + 79)
#define UAE_ERROR_ELIBSCN		(UAE_ERROR_base + 80)
#define UAE_ERROR_ELIBMAX		(UAE_ERROR_base + 81)
#define UAE_ERROR_ELIBEXEC		(UAE_ERROR_base + 82)
#define UAE_ERROR_EILSEQ		(UAE_ERROR_base + 83)
#define UAE_ERROR_ERESTART		(UAE_ERROR_base + 84)
#define UAE_ERROR_ESTRPIPE		(UAE_ERROR_base + 85)
#define UAE_ERROR_EUSERS		(UAE_ERROR_base + 86)
#define UAE_ERROR_ENOTSOCK		(UAE_ERROR_base + 87)
#define UAE_ERROR_EDESTADDRREQ		(UAE_ERROR_base + 88)
#define UAE_ERROR_EMSGSIZE		(UAE_ERROR_base + 89)
#define UAE_ERROR_EPROTOTYPE		(UAE_ERROR_base + 90)
#define UAE_ERROR_ENOPROTOOPT		(UAE_ERROR_base + 91)
#define UAE_ERROR_EPROTONOSUPPORT	(UAE_ERROR_base + 92)
#define UAE_ERROR_ESOCKTNOSUPPORT	(UAE_ERROR_base + 93)
#define UAE_ERROR_EOPNOTSUPP		(UAE_ERROR_base + 94)
#define UAE_ERROR_EPFNOSUPPORT		(UAE_ERROR_base + 95)
#define UAE_ERROR_EAFNOSUPPORT		(UAE_ERROR_base + 96)
#define UAE_ERROR_EADDRINUSE		(UAE_ERROR_base + 97)
#define UAE_ERROR_EADDRNOTAVAIL		(UAE_ERROR_base + 98)
#define UAE_ERROR_ENETDOWN		(UAE_ERROR_base + 99)
#define UAE_ERROR_ENETUNREACH		(UAE_ERROR_base + 100)
#define UAE_ERROR_ENETRESET		(UAE_ERROR_base + 101)
#define UAE_ERROR_ECONNABORTED		(UAE_ERROR_base + 102)
#define UAE_ERROR_ECONNRESET		(UAE_ERROR_base + 103)
#define UAE_ERROR_ENOBUFS		(UAE_ERROR_base + 104)
#define UAE_ERROR_EISCONN		(UAE_ERROR_base + 105)
#define UAE_ERROR_ENOTCONN		(UAE_ERROR_base + 106)
#define UAE_ERROR_ESHUTDOWN		(UAE_ERROR_base + 107)
#define UAE_ERROR_ETOOMANYREFS		(UAE_ERROR_base + 108)
#define UAE_ERROR_ETIMEDOUT		(UAE_ERROR_base + 109)
#define UAE_ERROR_ECONNREFUSED		(UAE_ERROR_base + 110)
#define UAE_ERROR_EHOSTDOWN		(UAE_ERROR_base + 111)
#define UAE_ERROR_EHOSTUNREACH		(UAE_ERROR_base + 112)
#define UAE_ERROR_EALREADY		(UAE_ERROR_base + 113)
#define UAE_ERROR_EINPROGRESS		(UAE_ERROR_base + 114)
#define UAE_ERROR_ESTALE		(UAE_ERROR_base + 115)
#define UAE_ERROR_EUCLEAN		(UAE_ERROR_base + 116)
#define UAE_ERROR_ENOTNAM		(UAE_ERROR_base + 117)
#define UAE_ERROR_ENAVAIL		(UAE_ERROR_base + 118)
#define UAE_ERROR_EISNAM		(UAE_ERROR_base + 119)
#define UAE_ERROR_EREMOTEIO		(UAE_ERROR_base + 120)
#define UAE_ERROR_EDQUOT		(UAE_ERROR_base + 121)
#define UAE_ERROR_ENOMEDIUM		(UAE_ERROR_base + 122)
#define UAE_ERROR_EMEDIUMTYPE		(UAE_ERROR_base + 123)

#define AUF_ERROR_base		49733632

void		uae_init (void);
int32_t		uae_error_to_errno (int32_t);

#endif /* UAE_HEADER_H */
