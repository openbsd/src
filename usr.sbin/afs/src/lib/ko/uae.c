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

/*
 * Unified AFS errnos
 *
 * The idea is that we use up the et space 'uae' and 'uaf' (512
 * entries) and use those for AFS errno's that the server can use
 * then the client supports the TellMeAboutYourself callbackmanager
 * interface.
 *
 * This implemetation builds a array at start time that we can use for
 * fast lookup of the uae error to a local errno. For those that we
 * don't support, we map them one to one (just like before uae).
 */

#include "ko_locl.h"
#ifdef RCSID
RCSID("$arla: uae.c,v 1.5 2004/07/06 13:13:51 tol Exp $") ;
#endif

static int32_t 	*uae_array;
static int	 uae_len;

static void
uae_set(int32_t errno_error, int32_t uae_error)
{
    int index = uae_error - UAE_ERROR_base;
    assert(index >= 0);
    assert(index < uae_len);
    uae_array[index] = errno_error;
}

/*
 *
 */

int32_t
uae_error_to_errno(int32_t error)
{
    if (error < UAE_ERROR_base || error - UAE_ERROR_base > uae_len)
	return error;
    return uae_array[error - UAE_ERROR_base];
}

/* 
 * Initialize UAE
 */

void
uae_init (void)
{
    int i;

    /* arla_warnx (ADEBCONN, "inituae"); */

    uae_len = 256 * 2;
    uae_array = malloc(uae_len * sizeof(uae_array[0]));
    if (uae_array == NULL)
        errx(-1, "uae_init: malloc failed\n");
    
    /* guess we have a one to one mapping */
    for (i = 0; i < uae_len; i++)
	uae_array[i] = i;

    /* 
       perl -n -e \
       'if (/UAE_ERROR_(\w+)/) { 
          print "#ifdef $1\n    uae_set($1,UAE_ERROR_$1);\n#endif\n";
	  }' \
       uae.h
    */
#ifdef base
    uae_set(base,UAE_ERROR_base);
#endif
#ifdef EPERM
    uae_set(EPERM,UAE_ERROR_EPERM);
#endif
#ifdef ENOENT
    uae_set(ENOENT,UAE_ERROR_ENOENT);
#endif
#ifdef ESRCH
    uae_set(ESRCH,UAE_ERROR_ESRCH);
#endif
#ifdef EINTR
    uae_set(EINTR,UAE_ERROR_EINTR);
#endif
#ifdef EIO
    uae_set(EIO,UAE_ERROR_EIO);
#endif
#ifdef ENXIO
    uae_set(ENXIO,UAE_ERROR_ENXIO);
#endif
#ifdef E2BIG
    uae_set(E2BIG,UAE_ERROR_E2BIG);
#endif
#ifdef ENOEXEC
    uae_set(ENOEXEC,UAE_ERROR_ENOEXEC);
#endif
#ifdef EBADF
    uae_set(EBADF,UAE_ERROR_EBADF);
#endif
#ifdef ECHILD
    uae_set(ECHILD,UAE_ERROR_ECHILD);
#endif
#ifdef EAGAIN
    uae_set(EAGAIN,UAE_ERROR_EAGAIN);
#endif
#ifdef ENOMEM
    uae_set(ENOMEM,UAE_ERROR_ENOMEM);
#endif
#ifdef EACCES
    uae_set(EACCES,UAE_ERROR_EACCES);
#endif
#ifdef EFAULT
    uae_set(EFAULT,UAE_ERROR_EFAULT);
#endif
#ifdef ENOTBLK
    uae_set(ENOTBLK,UAE_ERROR_ENOTBLK);
#endif
#ifdef EBUSY
    uae_set(EBUSY,UAE_ERROR_EBUSY);
#endif
#ifdef EEXIST
    uae_set(EEXIST,UAE_ERROR_EEXIST);
#endif
#ifdef EXDEV
    uae_set(EXDEV,UAE_ERROR_EXDEV);
#endif
#ifdef ENODEV
    uae_set(ENODEV,UAE_ERROR_ENODEV);
#endif
#ifdef ENOTDIR
    uae_set(ENOTDIR,UAE_ERROR_ENOTDIR);
#endif
#ifdef EISDIR
    uae_set(EISDIR,UAE_ERROR_EISDIR);
#endif
#ifdef EINVAL
    uae_set(EINVAL,UAE_ERROR_EINVAL);
#endif
#ifdef ENFILE
    uae_set(ENFILE,UAE_ERROR_ENFILE);
#endif
#ifdef EMFILE
    uae_set(EMFILE,UAE_ERROR_EMFILE);
#endif
#ifdef ENOTTY
    uae_set(ENOTTY,UAE_ERROR_ENOTTY);
#endif
#ifdef ETXTBSY
    uae_set(ETXTBSY,UAE_ERROR_ETXTBSY);
#endif
#ifdef EFBIG
    uae_set(EFBIG,UAE_ERROR_EFBIG);
#endif
#ifdef ENOSPC
    uae_set(ENOSPC,UAE_ERROR_ENOSPC);
#endif
#ifdef ESPIPE
    uae_set(ESPIPE,UAE_ERROR_ESPIPE);
#endif
#ifdef EROFS
    uae_set(EROFS,UAE_ERROR_EROFS);
#endif
#ifdef EMLINK
    uae_set(EMLINK,UAE_ERROR_EMLINK);
#endif
#ifdef EPIPE
    uae_set(EPIPE,UAE_ERROR_EPIPE);
#endif
#ifdef EDOM
    uae_set(EDOM,UAE_ERROR_EDOM);
#endif
#ifdef ERANGE
    uae_set(ERANGE,UAE_ERROR_ERANGE);
#endif
#ifdef EDEADLK
    uae_set(EDEADLK,UAE_ERROR_EDEADLK);
#endif
#ifdef ENAMETOOLONG
    uae_set(ENAMETOOLONG,UAE_ERROR_ENAMETOOLONG);
#endif
#ifdef ENOLCK
    uae_set(ENOLCK,UAE_ERROR_ENOLCK);
#endif
#ifdef ENOSYS
    uae_set(ENOSYS,UAE_ERROR_ENOSYS);
#endif
#ifdef ENOTEMPTY
    uae_set(ENOTEMPTY,UAE_ERROR_ENOTEMPTY);
#endif
#ifdef ELOOP
    uae_set(ELOOP,UAE_ERROR_ELOOP);
#endif
#ifdef EWOULDBLOCK
    uae_set(EWOULDBLOCK,UAE_ERROR_EWOULDBLOCK);
#endif
#ifdef ENOMSG
    uae_set(ENOMSG,UAE_ERROR_ENOMSG);
#endif
#ifdef EIDRM
    uae_set(EIDRM,UAE_ERROR_EIDRM);
#endif
#ifdef ECHRNG
    uae_set(ECHRNG,UAE_ERROR_ECHRNG);
#endif
#ifdef EL2NSYNC
    uae_set(EL2NSYNC,UAE_ERROR_EL2NSYNC);
#endif
#ifdef EL3HLT
    uae_set(EL3HLT,UAE_ERROR_EL3HLT);
#endif
#ifdef EL3RST
    uae_set(EL3RST,UAE_ERROR_EL3RST);
#endif
#ifdef ELNRNG
    uae_set(ELNRNG,UAE_ERROR_ELNRNG);
#endif
#ifdef EUNATCH
    uae_set(EUNATCH,UAE_ERROR_EUNATCH);
#endif
#ifdef ENOCSI
    uae_set(ENOCSI,UAE_ERROR_ENOCSI);
#endif
#ifdef EL2HLT
    uae_set(EL2HLT,UAE_ERROR_EL2HLT);
#endif
#ifdef EBADE
    uae_set(EBADE,UAE_ERROR_EBADE);
#endif
#ifdef EBADR
    uae_set(EBADR,UAE_ERROR_EBADR);
#endif
#ifdef EXFULL
    uae_set(EXFULL,UAE_ERROR_EXFULL);
#endif
#ifdef ENOANO
    uae_set(ENOANO,UAE_ERROR_ENOANO);
#endif
#ifdef EBADRQC
    uae_set(EBADRQC,UAE_ERROR_EBADRQC);
#endif
#ifdef EBADSLT
    uae_set(EBADSLT,UAE_ERROR_EBADSLT);
#endif
#ifdef EBFONT
    uae_set(EBFONT,UAE_ERROR_EBFONT);
#endif
#ifdef ENOSTR
    uae_set(ENOSTR,UAE_ERROR_ENOSTR);
#endif
#ifdef ENODATA
    uae_set(ENODATA,UAE_ERROR_ENODATA);
#endif
#ifdef ETIME
    uae_set(ETIME,UAE_ERROR_ETIME);
#endif
#ifdef ENOSR
    uae_set(ENOSR,UAE_ERROR_ENOSR);
#endif
#ifdef ENONET
    uae_set(ENONET,UAE_ERROR_ENONET);
#endif
#ifdef ENOPKG
    uae_set(ENOPKG,UAE_ERROR_ENOPKG);
#endif
#ifdef EREMOTE
    uae_set(EREMOTE,UAE_ERROR_EREMOTE);
#endif
#ifdef ENOLINK
    uae_set(ENOLINK,UAE_ERROR_ENOLINK);
#endif
#ifdef EADV
    uae_set(EADV,UAE_ERROR_EADV);
#endif
#ifdef ESRMNT
    uae_set(ESRMNT,UAE_ERROR_ESRMNT);
#endif
#ifdef ECOMM
    uae_set(ECOMM,UAE_ERROR_ECOMM);
#endif
#ifdef EPROTO
    uae_set(EPROTO,UAE_ERROR_EPROTO);
#endif
#ifdef EMULTIHOP
    uae_set(EMULTIHOP,UAE_ERROR_EMULTIHOP);
#endif
#ifdef EDOTDOT
    uae_set(EDOTDOT,UAE_ERROR_EDOTDOT);
#endif
#ifdef EBADMSG
    uae_set(EBADMSG,UAE_ERROR_EBADMSG);
#endif
#ifdef EOVERFLOW
    uae_set(EOVERFLOW,UAE_ERROR_EOVERFLOW);
#endif
#ifdef ENOTUNIQ
    uae_set(ENOTUNIQ,UAE_ERROR_ENOTUNIQ);
#endif
#ifdef EBADFD
    uae_set(EBADFD,UAE_ERROR_EBADFD);
#endif
#ifdef EREMCHG
    uae_set(EREMCHG,UAE_ERROR_EREMCHG);
#endif
#ifdef ELIBACC
    uae_set(ELIBACC,UAE_ERROR_ELIBACC);
#endif
#ifdef ELIBBAD
    uae_set(ELIBBAD,UAE_ERROR_ELIBBAD);
#endif
#ifdef ELIBSCN
    uae_set(ELIBSCN,UAE_ERROR_ELIBSCN);
#endif
#ifdef ELIBMAX
    uae_set(ELIBMAX,UAE_ERROR_ELIBMAX);
#endif
#ifdef ELIBEXEC
    uae_set(ELIBEXEC,UAE_ERROR_ELIBEXEC);
#endif
#ifdef EILSEQ
    uae_set(EILSEQ,UAE_ERROR_EILSEQ);
#endif
#ifdef ERESTART
    uae_set(ERESTART,UAE_ERROR_ERESTART);
#endif
#ifdef ESTRPIPE
    uae_set(ESTRPIPE,UAE_ERROR_ESTRPIPE);
#endif
#ifdef EUSERS
    uae_set(EUSERS,UAE_ERROR_EUSERS);
#endif
#ifdef ENOTSOCK
    uae_set(ENOTSOCK,UAE_ERROR_ENOTSOCK);
#endif
#ifdef EDESTADDRREQ
    uae_set(EDESTADDRREQ,UAE_ERROR_EDESTADDRREQ);
#endif
#ifdef EMSGSIZE
    uae_set(EMSGSIZE,UAE_ERROR_EMSGSIZE);
#endif
#ifdef EPROTOTYPE
    uae_set(EPROTOTYPE,UAE_ERROR_EPROTOTYPE);
#endif
#ifdef ENOPROTOOPT
    uae_set(ENOPROTOOPT,UAE_ERROR_ENOPROTOOPT);
#endif
#ifdef EPROTONOSUPPORT
    uae_set(EPROTONOSUPPORT,UAE_ERROR_EPROTONOSUPPORT);
#endif
#ifdef ESOCKTNOSUPPORT
    uae_set(ESOCKTNOSUPPORT,UAE_ERROR_ESOCKTNOSUPPORT);
#endif
#ifdef EOPNOTSUPP
    uae_set(EOPNOTSUPP,UAE_ERROR_EOPNOTSUPP);
#endif
#ifdef EPFNOSUPPORT
    uae_set(EPFNOSUPPORT,UAE_ERROR_EPFNOSUPPORT);
#endif
#ifdef EAFNOSUPPORT
    uae_set(EAFNOSUPPORT,UAE_ERROR_EAFNOSUPPORT);
#endif
#ifdef EADDRINUSE
    uae_set(EADDRINUSE,UAE_ERROR_EADDRINUSE);
#endif
#ifdef EADDRNOTAVAIL
    uae_set(EADDRNOTAVAIL,UAE_ERROR_EADDRNOTAVAIL);
#endif
#ifdef ENETDOWN
    uae_set(ENETDOWN,UAE_ERROR_ENETDOWN);
#endif
#ifdef ENETUNREACH
    uae_set(ENETUNREACH,UAE_ERROR_ENETUNREACH);
#endif
#ifdef ENETRESET
    uae_set(ENETRESET,UAE_ERROR_ENETRESET);
#endif
#ifdef ECONNABORTED
    uae_set(ECONNABORTED,UAE_ERROR_ECONNABORTED);
#endif
#ifdef ECONNRESET
    uae_set(ECONNRESET,UAE_ERROR_ECONNRESET);
#endif
#ifdef ENOBUFS
    uae_set(ENOBUFS,UAE_ERROR_ENOBUFS);
#endif
#ifdef EISCONN
    uae_set(EISCONN,UAE_ERROR_EISCONN);
#endif
#ifdef ENOTCONN
    uae_set(ENOTCONN,UAE_ERROR_ENOTCONN);
#endif
#ifdef ESHUTDOWN
    uae_set(ESHUTDOWN,UAE_ERROR_ESHUTDOWN);
#endif
#ifdef ETOOMANYREFS
    uae_set(ETOOMANYREFS,UAE_ERROR_ETOOMANYREFS);
#endif
#ifdef ETIMEDOUT
    uae_set(ETIMEDOUT,UAE_ERROR_ETIMEDOUT);
#endif
#ifdef ECONNREFUSED
    uae_set(ECONNREFUSED,UAE_ERROR_ECONNREFUSED);
#endif
#ifdef EHOSTDOWN
    uae_set(EHOSTDOWN,UAE_ERROR_EHOSTDOWN);
#endif
#ifdef EHOSTUNREACH
    uae_set(EHOSTUNREACH,UAE_ERROR_EHOSTUNREACH);
#endif
#ifdef EALREADY
    uae_set(EALREADY,UAE_ERROR_EALREADY);
#endif
#ifdef EINPROGRESS
    uae_set(EINPROGRESS,UAE_ERROR_EINPROGRESS);
#endif
#ifdef ESTALE
    uae_set(ESTALE,UAE_ERROR_ESTALE);
#endif
#ifdef EUCLEAN
    uae_set(EUCLEAN,UAE_ERROR_EUCLEAN);
#endif
#ifdef ENOTNAM
    uae_set(ENOTNAM,UAE_ERROR_ENOTNAM);
#endif
#ifdef ENAVAIL
    uae_set(ENAVAIL,UAE_ERROR_ENAVAIL);
#endif
#ifdef EISNAM
    uae_set(EISNAM,UAE_ERROR_EISNAM);
#endif
#ifdef EREMOTEIO
    uae_set(EREMOTEIO,UAE_ERROR_EREMOTEIO);
#endif
#ifdef EDQUOT
    uae_set(EDQUOT,UAE_ERROR_EDQUOT);
#endif
#ifdef ENOMEDIUM
    uae_set(ENOMEDIUM,UAE_ERROR_ENOMEDIUM);
#endif
#ifdef EMEDIUMTYPE
    uae_set(EMEDIUMTYPE,UAE_ERROR_EMEDIUMTYPE);
#endif
}
