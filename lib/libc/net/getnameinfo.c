/*
 * %%% copyright-cmetz-96-bsd
 * Copyright (c) 1996-1999, Craig Metz, All rights reserved.
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
 *      This product includes software developed by Craig Metz and
 *      by other contributors.
 * 4. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* getnameinfo() v1.38 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <resolv.h>

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif /* AF_LOCAL */

#ifndef min 
#define min(x,y) (((x) > (y)) ? (y) : (x))
#endif /* min */

#define RETURN_ERROR(x) do { \
    rval = (x); \
    goto ret; \
    } while(0)

static int netdb_lookup_name(int family, void *addr, int addrlen, char *name,
	int namelen, int flags)
{
  struct hostent *hostent;
  char *c, *c2;
  int rval, i;

  if (!(hostent = gethostbyaddr(addr, addrlen, family))) {
    switch(h_errno) {
      case NETDB_INTERNAL:
        RETURN_ERROR(EAI_SYSTEM);
      case HOST_NOT_FOUND:
        RETURN_ERROR(1);
      case TRY_AGAIN:
        RETURN_ERROR(EAI_AGAIN);
      case NO_RECOVERY:
        RETURN_ERROR(EAI_FAIL);
      case NO_DATA:
        RETURN_ERROR(1);
      default:
        RETURN_ERROR(EAI_FAIL);
    };
  };

  endhostent();

  c = hostent->h_name;
  if ((flags & NI_NOFQDN) && (_res.options & RES_INIT) && _res.defdname[0] &&
  	(c2 = strstr(c + 1, _res.defdname)) && (*(--c2) == '.')) {
    *c2 = 0;
    i = min(c2 - c, namelen) - 1;
    strncpy(name, c, i);
  } else
    strncpy(name, c, namelen - 1);

  rval = 0;

ret:
  return rval;
}

int getnameinfo(const struct sockaddr *sa, size_t addrlen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
  int rval;
  int serrno = errno;

  if (!sa || (addrlen != SA_LEN(sa)))
    RETURN_ERROR(EAI_FAIL);

  if (host && (hostlen > 0))
    switch(sa->sa_family) {
      case AF_INET6:
        if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)sa)->sin6_addr)) {
	  if (flags & NI_NUMERICHOST)
	    goto inet6_noname;
	  else
            strncpy(host, "*", hostlen - 1);
          break;
        };

	if (IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)sa)->sin6_addr)) {
	  struct sockaddr_in sin;
	  memset(&sin, 0, sizeof(struct sockaddr_in));
	  sin.sin_len = sizeof(struct sockaddr_in);
	  sin.sin_family = AF_INET;
	  sin.sin_port = ((struct sockaddr_in6 *)sa)->sin6_port;
	  sin.sin_addr.s_addr = ((u_int32_t *)&((struct sockaddr_in6 *)sa)->sin6_addr)[3];
	  if (!(rval = getnameinfo((struct sockaddr *)&sin, sizeof(struct sockaddr_in), host, hostlen, serv, servlen, flags | NI_NAMEREQD)))
	    goto ret;
	  if (rval != EAI_NONAME)
	    goto ret;
	  goto inet6_noname;
	};

	if (flags & NI_NUMERICHOST)
	  goto inet6_noname;

	if ((rval = netdb_lookup_name(AF_INET6,
	    &((struct sockaddr_in6 *)sa)->sin6_addr, sizeof(struct in6_addr),
	    host, hostlen, flags)) < 0)
	  goto ret;
					      
	if (!rval)
	  break;

inet6_noname:
	if (flags & NI_NAMEREQD)
	  RETURN_ERROR(EAI_NONAME);
	
	if (!inet_ntop(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, host, hostlen))
	  RETURN_ERROR(EAI_NONAME);

	break;
      case AF_INET:
	if (flags & NI_NUMERICHOST)
	  goto inet_noname;

        if (!((struct sockaddr_in *)sa)->sin_addr.s_addr) {
          strncpy(host, "*", hostlen - 1);
          break;
        };

	if ((rval = netdb_lookup_name(AF_INET,
	    &((struct sockaddr_in *)sa)->sin_addr, sizeof(struct in_addr),
	    host, hostlen, flags)) < 0)
	  goto ret;

	if (!rval)
	  break;
inet_noname:
	if (flags & NI_NAMEREQD)
	  RETURN_ERROR(EAI_NONAME);
	
	if (!inet_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, host, hostlen))
	  RETURN_ERROR(EAI_NONAME);

	break;
      case AF_LOCAL:
	if (!(flags & NI_NUMERICHOST)) {
	  struct utsname utsname;
	  
	  if (!uname(&utsname)) {
	    strncpy(host, utsname.nodename, hostlen - 1);
	    break;
	  };
	};
	
	if (flags & NI_NAMEREQD)
	  RETURN_ERROR(EAI_NONAME);
	
	strncpy(host, "localhost", hostlen - 1);
	break;
      default:
        RETURN_ERROR(EAI_FAMILY);
    };

  if (serv && (servlen > 0))
    switch(sa->sa_family) {
      case AF_INET:
      case AF_INET6:
	if (!(flags & NI_NUMERICSERV)) {
	  struct servent *s;
	  if (s = getservbyport(((struct sockaddr_in *)sa)->sin_port, (flags & NI_DGRAM) ? "udp" : "tcp")) {
	    strncpy(serv, s->s_name, servlen - 1);
	    break;
	  };
          if (!((struct sockaddr_in *)sa)->sin_port) {
            strncpy(serv, "*", servlen - 1);
            break;
          };
	};
	snprintf(serv, servlen - 1, "%d", ntohs(((struct sockaddr_in *)sa)->sin_port));
	break;
      case AF_LOCAL:
	strncpy(serv, ((struct sockaddr_un *)sa)->sun_path, servlen - 1);
	break;
    };

  if (host && (hostlen > 0))
    host[hostlen-1] = 0;
  if (serv && (servlen > 0))
    serv[servlen-1] = 0;
  rval = 0;

ret:
  if (rval == 1)
    rval = EAI_FAIL;

  errno = serrno;

  return rval;
};
