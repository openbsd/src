/* sockerror.c --- convert WinSock error number to string
   Vince Del Vecchio <vdelvecc@spd.analog.com>

   This file is part of GNU CVS.

   GNU CVS is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#include <stdio.h>
#include <winsock.h>

struct err_strs {
    char **strs;
    int first;
    int last;
};

static char *errs1[] = {
    /* EINTR		*/ "Interrupted system call"
};

static char *errs2[] = {
    /* EBADF		*/ "Bad file descriptor"
};

static char *errs3[] = {
    /* EACCES		*/ "Permission denied",
    /* EFAULT		*/ "Bad address"
};

static char *errs4[] = {
    /* EINVAL		*/ "Invalid argument"
};

static char *errs5[] = {
    /* EMFILE		*/ "Too many open files",
};

static char *errs6[] = {
    /* EWOULDBLOCK	*/ "Resource temporarily unavailable",
    /* EINPROGRESS	*/ "Operation now in progress",
    /* EALREADY		*/ "Operation already in progress",
    /* ENOTSOCK		*/ "Socket operation on non-socket",
    /* EDESTADDRREQ	*/ "Destination address required",
    /* EMSGSIZE		*/ "Message too long",
    /* EPROTOTYPE	*/ "Protocol wrong type for socket",
    /* ENOPROTOOPT	*/ "Protocol not available",
    /* EPROTONOSUPPORT	*/ "Protocol not supported",
    /* ESOCKTNOSUPPORT	*/ "Socket type not supported",
    /* EOPNOTSUPP	*/ "Operation not supported on socket",
    /* EPFNOSUPPORT	*/ "Protocol family not supported",
    /* EAFNOSUPPORT	*/ "Address family not supported by protocol",
    /* EADDRINUSE	*/ "Address already in use",
    /* EADDRNOTAVAIL	*/ "Can't assign requested address",
    /* ENETDOWN		*/ "Network is down",
    /* ENETUNREACH	*/ "Network is unreachable",
    /* ENETRESET	*/ "Network connection dropped on reset",
    /* ECONNABORTED	*/ "Software caused connection abort",
    /* ECONNRESET	*/ "Connection reset by peer",
    /* ENOBUFS		*/ "No buffer space available",
    /* EISCONN		*/ "Socket is already connected",
    /* ENOTCONN		*/ "Socket is not connected",
    /* ESHUTDOWN	*/ "Can't send after socket shutdown",
    /* ETOOMANYREFS	*/ "Too many references: can't splice",
    /* ETIMEDOUT	*/ "Connection timed out",
    /* ECONNREFUSED	*/ "Connection refused",
    /* ELOOP		*/ "Too many levels of symbolic links",
    /* ENAMETOOLONG	*/ "File name too long",
    /* EHOSTDOWN	*/ "Host is down",
    /* EHOSTUNREACH	*/ "No route to host",
    /* ENOTEMPTY	*/ "Directory not empty",
    /* EPROCLIM		*/ "Too many processes",
    /* EUSERS		*/ "Too many users",
    /* EDQUOT		*/ "Disc quota exceeded",
    /* ESTALE		*/ "Stale NFS file handle",
    /* EREMOTE		*/ "Object is remote"
};

static char *errs7[] = {
    /* SYSNOTREADY	*/ "Network subsystem unavailable",
    /* VERNOTSUPPORTED	*/ "Requested WinSock version not supported",
    /* NOTINITIALISED	*/ "WinSock was not initialized"
};

#ifdef WSAEDISCON
static char *errs8[] = {
    /* EDISCON		*/ "Graceful shutdown in progress"
};
#endif

static char *errs9[] = {
    /* HOST_NOT_FOUND	*/ "Unknown host",
    /* TRY_AGAIN	*/ "Host name lookup failure",
    /* NO_RECOVERY	*/ "Unknown server error",
    /* NO_DATA		*/ "No address associated with name",
};

/* Some of these errors are defined in the winsock.h header file I have,
   but not in the Winsock 1.1 spec.  I include them some of them anyway,
   where it is not too hard to avoid referencing the symbolic constant. */

static struct err_strs sock_errlist[] = {
    { errs1,	WSAEINTR,	WSAEINTR },
    { errs2,	WSAEBADF,	WSAEBADF },
    { errs3,	WSAEACCES,	WSAEFAULT },
    { errs4,	WSAEINVAL,	WSAEINVAL },
    { errs5,	WSAEMFILE,	WSAEMFILE },
    { errs6,	WSAEWOULDBLOCK, WSAEHOSTUNREACH + 6 },
    { errs7,	WSASYSNOTREADY,	WSANOTINITIALISED },
#ifdef WSAEDISCON
    { errs8,	WSAEDISCON,	WSAEDISCON },
#endif
    { errs9,	WSAHOST_NOT_FOUND, WSANO_DATA }
};

char *
sock_strerror (int errnum)
{
    static char buf[40];
    int i;

    for (i = 0; i < (sizeof sock_errlist / sizeof *sock_errlist); i++)
    {
	if (errnum >= sock_errlist[i].first && errnum <= sock_errlist[i].last)
	    return sock_errlist[i].strs[errnum - sock_errlist[i].first];
    }
    sprintf(buf, "Unknown socket error: %d", errnum);
    return buf;
}
