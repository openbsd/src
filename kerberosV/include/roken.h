/* This is an OS dependent, generated file */


#ifndef __ROKEN_H__
#define __ROKEN_H__

/* -*- C -*- */
/*
 * Copyright (c) 1995-2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#define ROKEN_LIB_FUNCTION
#define ROKEN_LIB_CALL
#define ROKEN_LIB_VARIABLE


typedef int rk_socket_t;

#define rk_closesocket(x) close(x)
#define rk_SOCK_IOCTL(s,c,a) ioctl((s),(c),(a))
#define rk_IS_BAD_SOCKET(s) ((s) < 0)
#define rk_IS_SOCKET_ERROR(rv) ((rv) < 0)
#define rk_SOCK_ERRNO errno
#define rk_INVALID_SOCKET (-1)

#define rk_SOCK_INIT() 0
#define rk_SOCK_EXIT() do { } while(0)


#define IN_LOOPBACKNET 127


#define UNREACHABLE(x)
#define UNUSED_ARGUMENT(x)


#include <sys/param.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <grp.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include <paths.h>

#include <dirent.h>



#include <roken-common.h>

ROKEN_CPP_START

#define rk_UNCONST(x) ((void *)(uintptr_t)(const void *)(x))












#define asnprintf rk_asnprintf
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    rk_asnprintf (char **, size_t, const char *, ...)
     __attribute__ ((format (printf, 3, 4)));

#define vasnprintf rk_vasnprintf
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    vasnprintf (char **, size_t, const char *, va_list)
     __attribute__((format (printf, 3, 0)));



#define strlwr rk_strlwr
ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL strlwr(char *);



#define strsep_copy rk_strsep_copy
ROKEN_LIB_FUNCTION ssize_t ROKEN_LIB_CALL strsep_copy(const char**, const char*, char*, size_t);




#define strupr rk_strupr
ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL strupr(char *);





#define rk_strerror_r strerror_r







#include <pwd.h>
ROKEN_LIB_FUNCTION struct passwd * ROKEN_LIB_CALL k_getpwnam (const char *);
ROKEN_LIB_FUNCTION struct passwd * ROKEN_LIB_CALL k_getpwuid (uid_t);

ROKEN_LIB_FUNCTION const char * ROKEN_LIB_CALL get_default_username (void);








#define rk_rename(__rk_rn_from,__rk_rn_to) rename(__rk_rn_from,__rk_rn_to)

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL daemon(int, int);









#define bswap32 rk_bswap32
ROKEN_LIB_FUNCTION unsigned int ROKEN_LIB_CALL bswap32(unsigned int);

#define bswap16 rk_bswap16
ROKEN_LIB_FUNCTION unsigned short ROKEN_LIB_CALL bswap16(unsigned short);



ROKEN_LIB_FUNCTION time_t ROKEN_LIB_CALL tm2time (struct tm, int);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL unix_verify_user(char *, char *);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL roken_concat (char *, size_t, ...);

ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL roken_mconcat (char **, size_t, ...);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL roken_vconcat (char *, size_t, va_list);

ROKEN_LIB_FUNCTION size_t ROKEN_LIB_CALL
    roken_vmconcat (char **, size_t, va_list);

ROKEN_LIB_FUNCTION ssize_t ROKEN_LIB_CALL
    net_write (rk_socket_t, const void *, size_t);

ROKEN_LIB_FUNCTION ssize_t ROKEN_LIB_CALL
    net_read (rk_socket_t, void *, size_t);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
    issuid(void);


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL get_window_size(int fd, int *, int *);




#define getipnodebyname rk_getipnodebyname
ROKEN_LIB_FUNCTION struct hostent * ROKEN_LIB_CALL
getipnodebyname (const char *, int, int, int *);

#define getipnodebyaddr rk_getipnodebyaddr
ROKEN_LIB_FUNCTION struct hostent * ROKEN_LIB_CALL
getipnodebyaddr (const void *, size_t, int, int *);

#define freehostent rk_freehostent
ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
freehostent (struct hostent *);

#define copyhostent rk_copyhostent
ROKEN_LIB_FUNCTION struct hostent * ROKEN_LIB_CALL
copyhostent (const struct hostent *);









ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
getnameinfo_verified(const struct sockaddr *, socklen_t,
		     char *, size_t,
		     char *, size_t,
		     int);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
roken_getaddrinfo_hostspec(const char *, int, struct addrinfo **); 
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
roken_getaddrinfo_hostspec2(const char *, int, int, struct addrinfo **);




#define emalloc rk_emalloc
ROKEN_LIB_FUNCTION void * ROKEN_LIB_CALL emalloc (size_t);
#define ecalloc rk_ecalloc
ROKEN_LIB_FUNCTION void * ROKEN_LIB_CALL ecalloc(size_t, size_t);
#define erealloc rk_erealloc
ROKEN_LIB_FUNCTION void * ROKEN_LIB_CALL erealloc (void *, size_t);
#define estrdup rk_estrdup
ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL estrdup (const char *);

/*
 * kludges and such
 */

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
roken_gethostby_setup(const char*, const char*);
ROKEN_LIB_FUNCTION struct hostent* ROKEN_LIB_CALL
roken_gethostbyname(const char*);
ROKEN_LIB_FUNCTION struct hostent* ROKEN_LIB_CALL 
roken_gethostbyaddr(const void*, size_t, int);

#define roken_getservbyname(x,y) getservbyname(x,y)

#define roken_openlog(a,b,c) openlog(a,b,c)

#define roken_getsockname(a,b,c) getsockname(a,b,c)




ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
mini_inetd_addrinfo (struct addrinfo*, rk_socket_t *);

ROKEN_LIB_FUNCTION void ROKEN_LIB_CALL
mini_inetd (int, rk_socket_t *);


#define strsvis rk_strsvis
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
strsvis(char *, const char *, int, const char *);

#define strsvisx rk_strsvisx
ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
strsvisx(char *, const char *, size_t, int, const char *);


ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
strvis(char *, const char *, int);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
strvisx(char *, const char *, size_t, int);

#define svis rk_svis
ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL
svis(char *, int, int, int, const char *);

ROKEN_LIB_FUNCTION int ROKEN_LIB_CALL
unvis(char *, int, int *, int);

#define vis rk_vis
ROKEN_LIB_FUNCTION char * ROKEN_LIB_CALL
vis(char *, int, int, int);




#define rk_random() arc4random()




ROKEN_CPP_END

#endif /* __ROKEN_H__ */
