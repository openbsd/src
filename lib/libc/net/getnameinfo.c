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
 */

/* getnameinfo() v1.38 */

/* To enable debugging support (REQUIRES NRL support library), define: */
/* #define DEBUG 1 */

#ifdef __OpenBSD__
#define HAVE_POSIX1G_TYPES 1
#define INET6 1
#define LOCAL 1
#define NETDB 1
#define SALEN 1
#undef RESOLVER
#undef HOSTTABLE
#undef DEBUG
#undef HAVE_GETSERVBYNAME_R
#undef HAVE_GETHOSTBYNAME2_R
#endif /* __OpenBSD__ */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#if LOCAL
#include <sys/un.h>
#include <sys/utsname.h>
#endif /* LOCAL */
#include <netdb.h>
#include <errno.h>
#include <string.h>
#if RESOLVER
#include <arpa/nameser.h>
#include <resolv.h>
#endif /* RESOLVER */

#include "support.h"

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif /* AF_LOCAL */

#ifndef min 
#define min(x,y) (((x) > (y)) ? (y) : (x))
#endif /* min */

#if DEBUG
#if RESOLVER
#define DEBUG_MESSAGES (_res.options & RES_DEBUG)
#else /* RESOLVER */
int __getnameinfo_debug = 0;
#define DEBUG_MESSAGES (__getnameinfo_debug)
#endif /* RESOLVER */
#endif /* DEBUG */

#if DEBUG
#define RETURN_ERROR(x) do { \
    if (DEBUG_MESSAGES) \
      fprintf(stderr, "%s:%d: returning %s\n", __FILE__, __LINE__, #x); \
    rval = (x); \
    goto ret; \
    } while(0)
#else /* DEBUG */
#define RETURN_ERROR(x) do { \
    rval = (x); \
    goto ret; \
    } while(0)
#endif /* DEBUG */

#if HOSTTABLE
static int hosttable_lookup_name(int family, void *addr, char *name, int namelen, int flags)
{
  int rval;
  FILE *f;
  char buffer[1024];
  char addrbuf[16];
  char *c, *c2;
  int i;
  char *prevcname = NULL;

  if (!(f = fopen("/etc/hosts", "r")))
    RETURN_ERROR(EAI_SYSTEM);

  while(fgets(buffer, sizeof(buffer), f)) {
    if (c = strchr(buffer, '#'))
      *c = 0;

    c = buffer;
    while(*c && !isspace(*c)) c++;
    if (!*c)
      continue;

    *(c++) = 0;

    if (family == AF_INET)
      if (inet_pton(AF_INET, buffer, addrbuf) > 0)
	if (!memcmp(addrbuf, addr, sizeof(struct in_addr)))
	  goto build;

#if INET6
    if (family == AF_INET6)
      if (inet_pton(AF_INET6, buffer, addrbuf) > 0)
	if (!memcmp(addrbuf, addr, sizeof(struct in6_addr)))
	  goto build;
#endif /* INET6 */

    continue;

build:
    while(*c && isspace(*c)) c++;
    if (!*c)
      continue;

    c2 = c;
    while(*c2 && !isspace(*c2)) c2++;
    if (!*c2)
      continue;
    *c2 = 0;

    if ((flags & NI_NOFQDN) && (_res.options & RES_INIT) && _res.defdname[0] && (c2 = strstr(c + 1, _res.defdname)) && (*(--c2) == '.')) {
      *c2 = 0;
      i = min(c2 - c, namelen) - 1;
      strncpy(name, c, i);
    } else
      strncpy(name, c, namelen - 1);

    rval = 0;
    goto ret;
  };

  RETURN_ERROR(1);

ret:
  fclose(f);
  return rval;
};
#endif /* HOSTTABLE */

#if RESOLVER
#if INET6
static char hextab[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                         '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
#endif /* INET6 */

struct rrheader {
  int16_t type;
  int16_t class;
  u_int32_t ttl;
  int16_t size;
};
#define RRHEADER_SZ 10

int resolver_lookup_name(const char *ptrname, char *name, int namelen, int flags)
{
  int rval;
  char answer[PACKETSZ];
  int answerlen;
  char dn[MAXDNAME];
  char *prevcname = NULL;
  void *p, *ep;
  int answers, i;
  uint16_t rtype, rclass;

  if ((answerlen = res_search(ptrname, C_IN, T_PTR, answer, sizeof(answer))) < 0) {
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

  p = answer;
  ep = answer + answerlen;
  
  if (answerlen < sizeof(HEADER))
    RETURN_ERROR(EAI_FAIL);

  {
    HEADER *h = (HEADER *)p;
    if (!h->qr || (h->opcode != QUERY) || (h->qdcount != htons(1)) || !h->ancount)
      RETURN_ERROR(EAI_FAIL);

    answers = ntohs(h->ancount);
  };
  p += sizeof(HEADER);

  if ((i = dn_expand(answer, ep, p, dn, sizeof(dn))) < 0)
    RETURN_ERROR(EAI_FAIL);

  p += i;

  if (p + 2*sizeof(u_int16_t) >= ep)
    RETURN_ERROR(EAI_FAIL);

  GETSHORT(rtype, p);
  GETSHORT(rclass, p);

  if ((rtype != T_PTR) || (rclass != C_IN))
    RETURN_ERROR(EAI_FAIL);

  while(answers--) {
    if ((i = dn_expand(answer, ep, p, dn, sizeof(dn))) < 0)
      RETURN_ERROR(EAI_FAIL);

    p += i;
  
    if (p + RRHEADER_SZ >= ep)
      RETURN_ERROR(EAI_FAIL);

    GETSHORT(rtype, p);
    GETSHORT(rclass, p);
    p += sizeof(uint32_t);
    if (rclass != C_IN)
      RETURN_ERROR(EAI_FAIL);
    GETSHORT(rclass, p);
    i = rclass;

    if (p + i > ep)
      RETURN_ERROR(EAI_FAIL);

    if (rtype == T_PTR) {
      if (dn_expand(answer, ep, p, dn, sizeof(dn)) != i)
        RETURN_ERROR(EAI_FAIL);

      {
      char *c2;

      if ((flags & NI_NOFQDN) && (_res.options & RES_INIT) && _res.defdname[0] && (c2 = strstr(dn + 1, _res.defdname)) && (*(--c2) == '.')) {
        *c2 = 0;
        strncpy(name, dn, min(c2 - dn, namelen) - 1);
      } else
        strncpy(name, dn, namelen - 1);
      };
    };
    p += i;
  };

  rval = 0;

ret:
  return rval;
};
#endif /* RESOLVER */

#if NETDB
static int netdb_lookup_name(int family, void *addr, int addrlen, char *name,
	int namelen)
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
#endif /* NETDB */

int getnameinfo(const struct sockaddr *sa, size_t addrlen, char *host, size_t hostlen, char *serv, size_t servlen, int flags)
{
  int rval;
  int serrno = errno;

  if (!sa || (addrlen != SA_LEN(sa)))
    RETURN_ERROR(EAI_FAIL);

  if (host && (hostlen > 0))
    switch(sa->sa_family) {
#if INET6
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
#if SALEN
	  sin.sin_len = sizeof(struct sockaddr_in);
#endif /* SALEN */
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

#if HOSTTABLE
	if ((rval = hosttable_lookup_name(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, host, hostlen, flags)) < 0)
	  goto ret;
	
	if (!rval)
	  break;
#endif /* HOSTTABLE */
#if RESOLVER
	{
	  char ptrname[sizeof("0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.0.1.2.3.4.5.6.7.8.9.a.b.c.d.e.f.ip6.int.")];
	  {
	    int i;
	    char *c = ptrname;
	    u_int8_t *p = (u_int8_t *)&((struct sockaddr_in6 *)sa)->sin6_addr + sizeof(struct in6_addr) - 1;
	    
	    for (i = sizeof(struct in6_addr) / sizeof(u_int8_t); i > 0; i--, p--) {
	      *(c++) = hextab[*p & 0x0f];
	      *(c++) = '.';
	      *(c++) = hextab[(*p & 0xf0) >> 4];
	      *(c++) = '.';
	    };
	    strcpy(c, "ip6.int.");
	  };
	  
	  if ((rval = resolver_lookup_name(ptrname, host, hostlen, flags)) < 0)
	    goto ret;
	  
	  if (!rval)
	    break;
	};
#endif /* RESOLVER */

inet6_noname:
	if (flags & NI_NAMEREQD)
	  RETURN_ERROR(EAI_NONAME);
	
	if (!inet_ntop(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr, host, hostlen))
	  RETURN_ERROR(EAI_NONAME);

	break;
#endif /* INET6 */
      case AF_INET:
	if (flags & NI_NUMERICHOST)
	  goto inet_noname;

        if (!((struct sockaddr_in *)sa)->sin_addr.s_addr) {
          strncpy(host, "*", hostlen - 1);
          break;
        };

#if HOSTTABLE
	if ((rval = hosttable_lookup_name(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, host, hostlen, flags)) < 0)
	  goto ret;

	if (!rval)
	  break;
#endif /* HOSTTABLE */
#if RESOLVER
	{
	  char ptrname[30];
	  u_int8_t *p = (u_int8_t *)&((struct sockaddr_in *)sa)->sin_addr;
	  sprintf(ptrname, "%d.%d.%d.%d.in-addr.arpa.", p[3], p[2], p[1], p[0]);
	  
	  if ((rval = resolver_lookup_name(ptrname, host, hostlen, flags)) < 0)
	    goto ret;

	  if (!rval)
	    break;
	};
#endif /* RESOLVER */

inet_noname:
	if (flags & NI_NAMEREQD)
	  RETURN_ERROR(EAI_NONAME);
	
	if (!inet_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr, host, hostlen))
	  RETURN_ERROR(EAI_NONAME);

	break;
#if LOCAL
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
#endif /* LOCAL */
      default:
        RETURN_ERROR(EAI_FAMILY);
    };

  if (serv && (servlen > 0))
    switch(sa->sa_family) {
      case AF_INET:
#if INET6
      case AF_INET6:
#endif /* INET6 */
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
#if LOCAL
      case AF_LOCAL:
	strncpy(serv, ((struct sockaddr_un *)sa)->sun_path, servlen - 1);
	break;
#endif /* LOCAL */
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
