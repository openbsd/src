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
 */

/* getaddrinfo() v1.38 */

/*
   I'd like to thank Matti Aarnio for finding some bugs in this code and
   sending me patches, as well as everyone else who has contributed to this
   code (by reporting bugs, suggesting improvements, and commented on its
   behavior and proposed changes).
*/

/*
   Someone merged an earlier version of this code into the GNU libc and
   added support for getservbyname_r and gethostbyname2_r. The support for
   those functions in this version of the code was written using that work
   as a reference. I may have improved on it, or I may have broken it.
*/

/* To do what POSIX says, even when it's broken, define: */
/* #define BROKEN_LIKE_POSIX 1 */
/* Note: real apps will break if you define this, while nothing other than a
   conformance test suite should have a problem with it undefined. */

/* If your C runtime library provides the POSIX p1003.1g D6.6 bit types
   of the form u?int(16|32)_t, define: */
/* #define HAVE_POSIX1G_TYPES 1 */
/* Note: this implementation tries to guess what the correct values are for
   your compiler+processor combination but might not always get it right. */

/* To enable debugging support (REQUIRES NRL support library), define: */
/* #define DEBUG 1 */

#if FOR_GNULIBC
#define HAVE_POSIX1G_TYPES 1
#define INET6 1
#define LOCAL 1
#define NETDB 1
#undef RESOLVER
#undef HOSTTABLE
#undef DEBUG
#define HAVE_GETSERVBYNAME_R 1
#define HAVE_GETHOSTBYNAME2_R 1
#define getservbyname_r __getservbyname_r
#define gethostbyname2_r __gethostbyname2_r
#endif /* FOR_GNULIBC */

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
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#if LOCAL
#include <stdio.h>
#include <sys/utsname.h>
#include <sys/un.h>
#endif /* LOCAL */
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#if RESOLVER
#include <arpa/nameser.h>
#include <resolv.h>
#endif /* RESOLVER */
#if DEBUG
#include <syslog.h>
#endif /* DEBUG */

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif /* AF_LOCAL */
#ifndef PF_LOCAL
#define PF_LOCAL PF_UNIX
#endif /* PF_LOCAL */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif /* UNIX_PATH_MAX */

#if !HAVE_POSIX1G_TYPES
#if (~0UL) == 0xffffffff
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short
#define int32_t long
#define uint32_t unsigned long
#else /* (~0UL) == 0xffffffff */
#if (~0UL) == 0xffffffffffffffff
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short
#define int32_t int
#define uint32_t unsigned int
#else /* (~0UL) == 0xffffffffffffffff */
#error Neither 32 bit nor 64 bit word size detected.
#error You need to define the bit types manually.
#endif /* (~0UL) == 0xffffffffffffffff */
#endif /* (~0UL) == 0xffffffff */
#endif /* !HAVE_POSIX1G_TYPES */

#if defined(INET6) && !defined(AF_INET6)
#error Without a definition of AF_INET6, this system cannot support IPv6
#error addresses.
#endif /* defined(INET6) && !defined(AF_INET6) */

#if INET6
#ifndef T_AAAA
#define T_AAAA 28
#endif /* T_AAAA */
#endif /* INET6 */

#if DEBUG
#if RESOLVER
#define DEBUG_MESSAGES (_res.options & RES_DEBUG)
#else /* RESOLVER */
int __getaddrinfo_debug = 0;
#define DEBUG_MESSAGES (__getaddrinfo_debug)
#endif /* RESOLVER */
#endif /* DEBUG */

#define GAIH_OKIFUNSPEC 0x0100
#define GAIH_EAI        ~(GAIH_OKIFUNSPEC)

static struct addrinfo nullreq =
{ 0, PF_UNSPEC, 0, 0, 0, NULL, NULL, NULL };

struct gaih_service {
  char *name;
  int num;
};

struct gaih_servtuple {
  struct gaih_servtuple *next;
  int socktype;
  int protocol;
  int port;
};

static struct gaih_servtuple nullserv = {
  NULL, 0, 0, 0
};

struct gaih_addrtuple {
  struct gaih_addrtuple *next;
  int family;
  char addr[16];
  char *cname;
};

struct gaih_typeproto {
  int socktype;
  int protocol;
  char *name;
};

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
static int hosttable_lookup_addr(const char *name, const struct addrinfo *req, struct gaih_addrtuple **pat)
{
  FILE *f;
  char buffer[1024];
  char *c, *c2;
  int rval = 1;
  char *prevcname = NULL;
  struct gaih_addrtuple at;

  if (!(f = fopen("/etc/hosts", "r")))
    RETURN_ERROR(-EAI_SYSTEM);

  while(fgets(buffer, sizeof(buffer), f)) {
    if (c = strchr(buffer, '#'))
      *c = 0;

    c = buffer;
    while(*c && !isspace(*c)) c++;
    if (!*c)
      continue;

    *(c++) = 0;

    while(*c && isspace(*c)) c++;
    if (!*c)
      continue;

    if (!(c2 = strstr(c, name)))
      continue;

    if (*(c2 - 1) && !isspace(*(c2 - 1)))
      continue;

    c2 += strlen(name);
    if (*c2 && !isspace(*c2))
      continue;

    c2 = c;
    while(*c2 && !isspace(*c2)) c2++;
    if (!*c2)
      continue;
    *c2 = 0;

    memset(&at, 0, sizeof(struct gaih_addrtuple));

    if (!req->ai_family || (req->ai_family == AF_INET))
      if (inet_pton(AF_INET, buffer, (char *)&at.addr) > 0) {
	at.family = AF_INET;
	goto build;
      };

#if INET6
    if (!req->ai_family || (req->ai_family == AF_INET6))
      if (inet_pton(AF_INET6, buffer, (char *)&at.addr) > 0) {
	at.family = AF_INET6;
	goto build;
      };
#endif /* INET6 */

    continue;

build:
    if (!(*pat = malloc(sizeof(struct gaih_addrtuple))))
      RETURN_ERROR(-EAI_MEMORY);

    memcpy(*pat, &at, sizeof(struct gaih_addrtuple));

    if (req->ai_flags & AI_CANONNAME)
      if (prevcname && !strcmp(prevcname, c))
        (*pat)->cname = prevcname;
      else
        prevcname = (*pat)->cname = strdup(c);

    pat = &((*pat)->next);

    rval = 0;
  };

ret:
  if (f)
    fclose(f);
  return rval;
};
#endif /* HOSTTABLE */

#if NETDB
int netdb_lookup_addr(const char *name, int af, const struct addrinfo *req, struct gaih_addrtuple **pat)
{
  int rval, herrno, i;
  char *prevcname = NULL;
  struct hostent *h;

#if HAVE_GETHOSTBYNAME2_R
  void *buf;
  int buflen = 1024;
  struct hostent th;
  int herrno, j;

  do {
    if (!(buf = malloc(buflen)))
      RETURN_ERROR(-EAI_MEMORY);

    if (!gethostbyname2_r(name, af, &th, buf, buflen, &h, &herrno))
      break;

    free(buf);
    buf = NULL;

    if ((herrno == NETDB_INTERNAL) && (errno == ERANGE)) {
      if (buflen >= 65536)
	RETURN_ERROR(-EAI_MEMORY);

      buflen = buflen << 1;
      continue;
    };
  } while(0);
#else /* HAVE_GETHOSTBYNAME2_R */
  h = gethostbyname2(name, af);
  herrno = h_errno;
#endif /* HAVE_GETHOSTBYNAME2_R */

  if (!h) {
#if DEBUG
    if (DEBUG_MESSAGES)
      fprintf(stderr, "getaddrinfo: gethostbyname2 failed, h_errno=%d\n", herrno);
#endif /* DEBUG */
    switch(herrno) {
      case NETDB_INTERNAL:
	RETURN_ERROR(-EAI_SYSTEM);
      case HOST_NOT_FOUND:
	RETURN_ERROR(1);
      case TRY_AGAIN:
	RETURN_ERROR(-EAI_AGAIN);
      case NO_RECOVERY:
	RETURN_ERROR(-EAI_FAIL);
      case NO_DATA:
	RETURN_ERROR(1);
      default:
	RETURN_ERROR(-EAI_FAIL);
    };
  };

  for (i = 0; h->h_addr_list[i]; i++) {
    while(*pat)
      pat = &((*pat)->next);

    if (!(*pat = malloc(sizeof(struct gaih_addrtuple))))
      RETURN_ERROR(-EAI_MEMORY);

    memset(*pat, 0, sizeof(struct gaih_addrtuple));

    switch((*pat)->family = af) {
      case AF_INET:
	memcpy((*pat)->addr, h->h_addr_list[i], sizeof(struct in_addr));
	break;
#if INET6
      case AF_INET6:
	memcpy((*pat)->addr, h->h_addr_list[i], sizeof(struct in6_addr));
	break;
#endif /* INET6 */
      default:
	RETURN_ERROR(-EAI_FAIL);
    };

    if (req->ai_flags & AI_CANONNAME) {
      if (prevcname && !strcmp(prevcname, h->h_name))
	(*pat)->cname = prevcname;
      else
	prevcname = (*pat)->cname = strdup(h->h_name);
    };
    
    pat = &((*pat)->next);
  }

  rval = 0;

ret:
#if HAVE_GETHOSTBYNAME2_R
  free(buf);
#endif /* HAVE_GETHOSTBYNAME2_R */
  return rval;
};
#endif /* NETDB */

#if RESOLVER
#define RRHEADER_SZ 10

int resolver_lookup_addr(const char *name, int type, const struct addrinfo *req, struct gaih_addrtuple **pat)
{
  int rval;
  char answer[PACKETSZ];
  int answerlen;
  char dn[MAXDNAME];
  char *prevcname = NULL;
  void *p, *ep;
  int answers, i, j;
  uint16_t rtype, rclass;

  if ((answerlen = res_search(name, C_IN, type, answer, sizeof(answer))) < 0) {
#if DEBUG
    if (DEBUG_MESSAGES)
      fprintf(stderr, "getaddrinfo: res_search failed, h_errno=%d\n", h_errno);
#endif /* DEBUG */
    switch(h_errno) {
      case NETDB_INTERNAL:
	RETURN_ERROR(-EAI_SYSTEM);
      case HOST_NOT_FOUND:
	RETURN_ERROR(1);
      case TRY_AGAIN:
	RETURN_ERROR(-EAI_AGAIN);
      case NO_RECOVERY:
	RETURN_ERROR(-EAI_FAIL);
      case NO_DATA:
	RETURN_ERROR(1);
      default:
	RETURN_ERROR(-EAI_FAIL);
    };
  };

  p = answer;
  ep = answer + answerlen;
  
  if (answerlen < RRHEADER_SZ)
    RETURN_ERROR(-EAI_FAIL);

  {
    HEADER *h = (HEADER *)p;
    if (!h->qr || (h->opcode != QUERY) || (h->qdcount != htons(1)) ||
	!h->ancount)
      RETURN_ERROR(-EAI_FAIL);
    answers = ntohs(h->ancount);
  };
  p += sizeof(HEADER);

  if ((i = dn_expand(answer, ep, p, dn, sizeof(dn))) < 0)
    RETURN_ERROR(-EAI_FAIL);
  p += i;

  if (p + 2*sizeof(uint16_t) >= ep)
    RETURN_ERROR(-EAI_FAIL);

  GETSHORT(rtype, p);
  GETSHORT(rclass, p);

  if ((rtype != type) || (rclass != C_IN))
    RETURN_ERROR(-EAI_FAIL);

  while(answers--) {
    if ((i = dn_expand(answer, ep, p, dn, sizeof(dn))) < 0)
      RETURN_ERROR(-EAI_FAIL);
    p += i;

    if (p + RRHEADER_SZ >= ep)
      RETURN_ERROR(-EAI_FAIL);

    GETSHORT(rtype, p);
    GETSHORT(rclass, p);
    p += sizeof(uint32_t);
    if (rclass != C_IN)
      RETURN_ERROR(-EAI_FAIL);
    GETSHORT(rclass, p);
    i = rclass;

    if (p + i > ep)
      RETURN_ERROR(-EAI_FAIL);

    if (rtype == type) {
      while(*pat)
	pat = &((*pat)->next);

      if (!(*pat = malloc(sizeof(struct gaih_addrtuple))))
	RETURN_ERROR(-EAI_MEMORY);

      memset(*pat, 0, sizeof(struct gaih_addrtuple));
      
      switch(type) {
        case T_A:
	  if (i != sizeof(struct in_addr))
	    RETURN_ERROR(-EAI_FAIL);
	  (*pat)->family = AF_INET;
	  break;
#if INET6
        case T_AAAA:
	  if (i != sizeof(struct in6_addr))
	    RETURN_ERROR(-EAI_FAIL);
	  (*pat)->family = AF_INET6;
	  break;
#endif /* INET6 */
        default:
	  RETURN_ERROR(-EAI_FAIL);
      };

      memcpy((*pat)->addr, p, i);
    
      if (req->ai_flags & AI_CANONNAME)
	if (prevcname && !strcmp(prevcname, dn))
	  (*pat)->cname = prevcname;
	else
	  prevcname = (*pat)->cname = strdup(dn);
    };
    p += i;
  };

  rval = 0;

ret:
  return rval;
};
#endif /* RESOLVER */

#if LOCAL
static int gaih_local(const char *name, const struct gaih_service *service,
		     const struct addrinfo *req, struct addrinfo **pai)
{
  int rval;
  struct utsname utsname;

  if (name || (req->ai_flags & AI_CANONNAME))
    if (uname(&utsname) < 0)
      RETURN_ERROR(-EAI_SYSTEM);
  if (name) {
    if (strcmp(name, "localhost") && strcmp(name, "local") && strcmp(name, "unix") && strcmp(name, utsname.nodename))
      RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_NONAME);
  };

  if (!(*pai = malloc(sizeof(struct addrinfo) + sizeof(struct sockaddr_un) + ((req->ai_flags & AI_CANONNAME) ? (strlen(utsname.nodename) + 1): 0))))
    RETURN_ERROR(-EAI_MEMORY);

  (*pai)->ai_next = NULL;
  (*pai)->ai_flags = req->ai_flags;
  (*pai)->ai_family = AF_LOCAL;
  (*pai)->ai_socktype = req->ai_socktype ? req->ai_socktype : SOCK_STREAM;
  (*pai)->ai_protocol = req->ai_protocol;
  (*pai)->ai_addrlen = sizeof(struct sockaddr_un);
  (*pai)->ai_addr = (void *)(*pai) + sizeof(struct addrinfo);
#if SALEN
  ((struct sockaddr_un *)(*pai)->ai_addr)->sun_len = sizeof(struct sockaddr_un);
#endif /* SALEN */
  ((struct sockaddr_un *)(*pai)->ai_addr)->sun_family = AF_LOCAL;
  memset(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path, 0, UNIX_PATH_MAX);
  if (service) {
    char *c;
    if (c = strchr(service->name, '/')) {
      if (strlen(service->name) >= sizeof(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path))
        RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);
      strcpy(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path, service->name);
    } else {
      if (strlen(P_tmpdir "/") + 1 + strlen(service->name) >= sizeof(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path))
        RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);
      strcpy(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path, P_tmpdir "/");
      strcat(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path, service->name);
    };
  } else {
    char *c;
    if (!(c = tmpnam(NULL)))
      RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SYSTEM);

    strncpy((((struct sockaddr_un *)(*pai)->ai_addr)->sun_path), c, sizeof(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path) - 1);
    c[sizeof(((struct sockaddr_un *)(*pai)->ai_addr)->sun_path) - 1] = 0;
  };
  if (req->ai_flags & AI_CANONNAME) {
    strncpy((*pai)->ai_canonname = (char *)(*pai) + sizeof(struct addrinfo) + sizeof(struct sockaddr_un), utsname.nodename, sizeof(utsname.nodename) - 1);
    (*pai)->ai_canonname[sizeof(utsname.nodename) - 1] = 0;
  } else
    (*pai)->ai_canonname = NULL;

  rval = 0;

ret:
  return rval;
};
#endif /* LOCAL */

static struct gaih_typeproto gaih_inet_typeproto[] = {
  { 0, 0, NULL },
  { SOCK_STREAM, IPPROTO_TCP, "tcp" },
  { SOCK_DGRAM, IPPROTO_UDP, "udp" },
  { 0, 0, NULL }
};

static int gaih_inet_serv(char *servicename, struct gaih_typeproto *tp, struct gaih_servtuple **st)
{
  int rval;
  struct servent *s;
#if HAVE_GETSERVBYNAME_R
  int i;
  void *buf;
  int buflen = 1024;
  struct servent ts;

  do {
    if (!(buf = malloc(buflen)))
      RETURN_ERROR(-EAI_MEMORY);

    if (!getservbyname_r(servicename, tp->name, &ts, buf, buflen, &s))
      break;

    free(buf);
    buf = NULL;

    if (errno != ERANGE)
      RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);

    if (buflen >= 65536)
      RETURN_ERROR(-EAI_MEMORY);

    buflen = buflen << 1;
  } while(1);
#else /* HAVE_GETSERVBYNAME_R */
  if (!(s = getservbyname(servicename, tp->name)))
    RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);
#endif /* HAVE_GETSERVBYNAME_R */

  if (!(*st = malloc(sizeof(struct gaih_servtuple))))
    RETURN_ERROR(-EAI_MEMORY);

  (*st)->next = NULL;
  (*st)->socktype = tp->socktype;
  (*st)->protocol = tp->protocol;
  (*st)->port = s->s_port;

  rval = 0;

ret:
#if HAVE_GETSERVBYNAME_R
  if (buf)
    free(buf);
#endif /* HAVE_GETSERVBYNAME_R */
  return rval;
}

static int gaih_inet(const char *name, const struct gaih_service *service,
		     const struct addrinfo *req, struct addrinfo **pai)
{
  int rval;
  struct hostent *h = NULL;
  struct gaih_typeproto *tp = gaih_inet_typeproto;
  struct gaih_servtuple *st = &nullserv;
  struct gaih_addrtuple *at = NULL;
  int i;

  if (req->ai_protocol || req->ai_socktype) {
    for (tp++; tp->name &&
	  ((req->ai_socktype != tp->socktype) || !req->ai_socktype) && 
	  ((req->ai_protocol != tp->protocol) || !req->ai_protocol); tp++);
    if (!tp->name)
      if (req->ai_socktype)
	RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SOCKTYPE);
      else
	RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);
  }

  if (service && (service->num < 0)) {
    if (tp->name) {
      if (rval = gaih_inet_serv(service->name, tp, &st))
	goto ret;
    } else {
      struct gaih_servtuple **pst = &st;
      for (tp++; tp->name; tp++) {
	if (rval = gaih_inet_serv(service->name, tp, pst)) {
	  if (rval & GAIH_OKIFUNSPEC)
	    continue;
	  goto ret;
	};
	pst = &((*pst)->next);
      };
      if (st == &nullserv)
	RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);
    };
  } else {
    if (!(st = malloc(sizeof(struct gaih_servtuple))))
      RETURN_ERROR(-EAI_MEMORY);

    st->next = NULL;
    st->socktype = tp->socktype;
    st->protocol = tp->protocol;
    if (service)
      st->port = htons(service->num);
    else
      st->port = 0;
  };

  if (!name) {
    if (!(at = malloc(sizeof(struct gaih_addrtuple))))
      RETURN_ERROR(-EAI_MEMORY);

    memset(at, 0, sizeof(struct gaih_addrtuple));

#if INET6
    if (req->ai_family)
      at->family = req->ai_family;
    else {
      if (!(at->next = malloc(sizeof(struct gaih_addrtuple))))
	RETURN_ERROR(-EAI_MEMORY);

      at->family = AF_INET6;

      memset(at->next, 0, sizeof(struct gaih_addrtuple));
      at->next->family = AF_INET;
    };
#else /* INET6 */
    at->family = AF_INET;
#endif /* INET6 */

    goto build;
  };

  if (!req->ai_family || (req->ai_family == AF_INET)) {
    struct in_addr in_addr;
    if (inet_pton(AF_INET, name, &in_addr) > 0) {
      if (!(at = malloc(sizeof(struct gaih_addrtuple))))
	RETURN_ERROR(-EAI_MEMORY);
      
      memset(at, 0, sizeof(struct gaih_addrtuple));
      
      at->family = AF_INET;
      memcpy(at->addr, &in_addr, sizeof(struct in_addr));
      goto build;
    };
  };

#if INET6
  if (!req->ai_family || (req->ai_family == AF_INET6)) {
    struct in6_addr in6_addr;
    if (inet_pton(AF_INET6, name, &in6_addr) > 0) {
      if (!(at = malloc(sizeof(struct gaih_addrtuple))))
	RETURN_ERROR(-EAI_MEMORY);
      
      memset(at, 0, sizeof(struct gaih_addrtuple));
      
      at->family = AF_INET6;
      memcpy(at->addr, &in6_addr, sizeof(struct in6_addr));
      goto build;
    };
  };
#endif /* INET6 */

  if (!(req->ai_flags & AI_NUMERICHOST)) {
#if NETDB
#if INET6
    if (!req->ai_family || (req->ai_family == AF_INET6))
      if ((rval = netdb_lookup_addr(name, AF_INET6, req, &at)) < 0)
	goto ret;
#endif /* INET6 */
    if (!req->ai_family || (req->ai_family == AF_INET))
      if ((rval = netdb_lookup_addr(name, AF_INET, req, &at)) < 0)
	goto ret;

    if (!rval)
      goto build;
#else /* NETDB */
#if HOSTTABLE
    if ((rval = hosttable_lookup_addr(name, req, &at)) < 0)
      goto ret;

    if (!rval)
      goto build;
#endif /* HOSTTABLE */

#if RESOLVER
#if INET6
    {
    int rval2;

    if (!req->ai_family || (req->ai_family == AF_INET6))
      if ((rval2 = resolver_lookup_addr(name, T_AAAA, req, &at)) < 0) {
	rval = rval2;
	goto ret;
      };
#endif /* INET6 */

    if (!req->ai_family || (req->ai_family == AF_INET))
      if ((rval = resolver_lookup_addr(name, T_A, req, &at)) < 0)
	goto ret;

#if INET6
    if (!rval || !rval2)
      goto build;
    };
#else /* INET6 */
    if (!rval)
      goto build;
#endif /* INET6 */
#endif /* RESOLVER */
#endif /* NETDB */
  };

  if (!at)
    RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_NONAME);

build:
  {
    char *prevcname = NULL;
    struct gaih_servtuple *st2;
    struct gaih_addrtuple *at2 = at;
    int j;

    while(at2) {
      if (req->ai_flags & AI_CANONNAME) {
	if (at2->cname)
	  j = strlen(at2->cname) + 1;
	else
	  if (name)
	    j = strlen(name) + 1;
	  else
	    j = 2;
      } else
	j = 0;

#if INET6
      if (at2->family == AF_INET6)
	i = sizeof(struct sockaddr_in6);
      else
#endif /* INET6 */
	i = sizeof(struct sockaddr_in);

      st2 = st;
      while(st2) {
	if (!(*pai = malloc(sizeof(struct addrinfo) + i + j)))
	  RETURN_ERROR(-EAI_MEMORY);

	memset(*pai, 0, sizeof(struct addrinfo) + i + j);

	(*pai)->ai_flags = req->ai_flags;
	(*pai)->ai_family = at2->family;
	(*pai)->ai_socktype = st2->socktype;
	(*pai)->ai_protocol = st2->protocol;
	(*pai)->ai_addrlen = i;
	(*pai)->ai_addr = (void *)(*pai) + sizeof(struct addrinfo);
#if SALEN
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_len = i;
#endif /* SALEN */
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_family = at2->family;
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_port = st2->port;

#if INET6
	if (at2->family == AF_INET6)
	  memcpy(&((struct sockaddr_in6 *)(*pai)->ai_addr)->sin6_addr, at2->addr, sizeof(struct in6_addr));
	else
#endif /* INET6 */
	  memcpy(&((struct sockaddr_in *)(*pai)->ai_addr)->sin_addr, at2->addr, sizeof(struct in_addr));

	if (j) {
	  (*pai)->ai_canonname = (void *)(*pai) + sizeof(struct addrinfo) + i;
	  if (at2->cname) {
	    strcpy((*pai)->ai_canonname, at2->cname);
	    if (prevcname != at2->cname) {
	      if (prevcname)
		free(prevcname);
	      prevcname = at2->cname;
	    };
	  } else
	    strcpy((*pai)->ai_canonname, name ? name : "*");
	};

	pai = &((*pai)->ai_next);

	st2 = st2->next;
      };
      at2 = at2->next;
    };
  };

  rval = 0;

ret:
  if (st != &nullserv) {
    struct gaih_servtuple *st2 = st;
    while(st) {
      st2 = st->next;
      free(st);
      st = st2;
    }
  }
  if (at) {
    struct gaih_addrtuple *at2 = at;
    while(at) {
      at2 = at->next;
      free(at);
      at = at2;
    }
  }

  return rval;
}

struct gaih {
  int family;
  char *name;
  int (*gaih)(const char *name, const struct gaih_service *service,
	      const struct addrinfo *req, struct addrinfo **pai);
};

static struct gaih gaih[] = {
#if INET6
  { PF_INET6, "inet6", gaih_inet },
#endif /* INET6 */
  { PF_INET, "inet", gaih_inet },
#if LOCAL
  { PF_LOCAL, "local", gaih_local },
#endif /* LOCAL */
  { -1, NULL, NULL }
};

#if DEBUG
static void dump_addrinfo(const struct addrinfo *ai, int follownext)
{
  char *c;

loop:
  fprintf(stderr, "addrinfo at ");
  if (!ai) {
    fprintf(stderr, "NULL\n");
    return;
  };
  fprintf(stderr, "%08x:\n", (unsigned int)ai);
  fprintf(stderr, "  flags=%x(", ai->ai_flags);
  c = "";
  if (ai->ai_flags & AI_PASSIVE) {
    fprintf(stderr, "passive");
    c = " ";
  };
  if (ai->ai_flags & AI_CANONNAME) {
    fprintf(stderr, "%scanonname", c);
    c = " ";
  };
  if (ai->ai_flags & AI_NUMERICHOST) {
    fprintf(stderr, "%snumerichost", c);
    c = " ";
  };
  if (ai->ai_flags & AI_EXT) {
    fprintf(stderr, "%sext", c);
  };
  fprintf(stderr, ")\n");
  fprintf(stderr, "  family=%x(%s)\n", ai->ai_family, nrl_afnumtoname(ai->ai_family));
  fprintf(stderr, "  socktype=%x(%s)\n", ai->ai_socktype, nrl_socktypenumtoname(ai->ai_socktype));
  fprintf(stderr, "  protocol=%x\n", ai->ai_protocol);
  fprintf(stderr, "  addrlen=%x\n", ai->ai_addrlen);
  fprintf(stderr, "  addr=%08x", (unsigned int)ai->ai_addr);
  if (ai->ai_addr) {
    fprintf(stderr, ":\n");
#if SALEN
    fprintf(stderr, "    len=%x\n", ai->ai_addr->sa_len);
#endif /* SALEN */
    fprintf(stderr, "    family=%x(%s)\n", ai->ai_addr->sa_family, nrl_afnumtoname(ai->ai_addr->sa_family));
    fprintf(stderr, "    data=");

#if SALEN
    if (ai->ai_addrlen != ai->ai_addr->sa_len) {
      fprintf(stderr, "  (addrlen != len, skipping)");
    } else
#endif /* SALEN */
    {
      uint8_t *p;
      int i;

      p = (uint8_t *)ai->ai_addr->sa_data;
      i = ai->ai_addrlen - ((void *)ai->ai_addr->sa_data - (void *)ai->ai_addr);
      while (i-- > 0)
        fprintf(stderr, "%02x", *(p++));
    };
  };
  fprintf(stderr, "\n  canonname=%08x", (unsigned int)ai->ai_canonname);
  if (ai->ai_canonname)
    fprintf(stderr, "(%s)", ai->ai_canonname);
  fprintf(stderr, "\n  next=%08x\n", (unsigned int)ai->ai_next);

  if (follownext && ai->ai_next) {
    ai = ai->ai_next;
    goto loop;
  };
};
#endif /* DEBUG */

int getaddrinfo(const char *name, const char *service,
		const struct addrinfo *req, struct addrinfo **pai)
{
  int rval = EAI_SYSTEM; /* XXX */
  int i, j = 0;
  struct addrinfo *p = NULL, **end;
  struct gaih *g = gaih, *pg = NULL;
  struct gaih_service gaih_service, *pservice;

#if DEBUG
  if (DEBUG_MESSAGES) {
    fprintf(stderr, "getaddrinfo(name=%s, service=%s, req=%p, pai=%p)\n  req: ", name ? name : "NULL", service ? service : "NULL", req, pai);

    dump_addrinfo(req, 0);
  };
#endif /* DEBUG */

  if (name && (name[0] == '*') && !name[1])
    name = NULL;

  if (service && (service[0] == '*') && !service[1])
    service = NULL;

#if BROKEN_LIKE_POSIX
  if  (!name && !service && !(req.ai_flags & AI_EXT))
    RETURN_ERROR(EAI_NONAME);
#endif /* BROKEN_LIKE_POSIX */

  if (!req)
    req = &nullreq;

  if (req->ai_flags & ~(AI_CANONNAME | AI_PASSIVE | AI_NUMERICHOST | AI_EXT))
    RETURN_ERROR(EAI_BADFLAGS);

#if BROKEN_LIKE_POSIX
  if ((req->ai_flags & AI_CANONNAME) && !name && !(req.ai_flags & AI_EXT))
    RETURN_ERROR(EAI_BADFLAGS);
#endif /* BROKEN_LIKE_POSIX */

  if (service && *service) {
    char *c;
    gaih_service.num = strtoul(gaih_service.name = (void *)service, &c, 10);
    if (*c) {
      gaih_service.num = -1;
    }
#if BROKEN_LIKE_POSIX
      else
        if (!req->ai_socktype && !(req.ai_flags & AI_EXT))
	  RETURN_ERROR(EAI_SERVICE);
#endif /* BROKEN_LIKE_POSIX */

    pservice = &gaih_service;
  } else
    pservice = NULL;

  if (pai)
    end = &p;
  else
    end = NULL;

  while(g->gaih) {
    if ((req->ai_family == g->family) || !req->ai_family) {
      j++;
      if (!((pg && (pg->gaih == g->gaih)))) {
	pg = g;
	if (rval = g->gaih(name, pservice, req, end)) {
	  if (!req->ai_family && (rval & GAIH_OKIFUNSPEC))
	    continue;

	  if (p)
	    freeaddrinfo(p);

	  rval = -(rval & GAIH_EAI);
	  goto ret;
	}
	if (end)
          while(*end) end = &((*end)->ai_next);
      }
    }
    g++;
  }

  if (!j)
    RETURN_ERROR(EAI_FAMILY);

  if (p) {
    *pai = p;
#if DEBUG
    if (DEBUG_MESSAGES) {
      fprintf(stderr, "getaddrinfo: Success. *pai:\n");
      dump_addrinfo(p, 1);
    };
#endif /* DEBUG */
    rval = 0;
    goto ret;
  }

  if (!pai && !rval) {
    rval = 0;
    goto ret;
  };

  RETURN_ERROR(EAI_NONAME);

ret:
#if DEBUG
  if (DEBUG_MESSAGES)
    fprintf(stderr, "getaddrinfo=%d\n", rval);
#endif /* DEBUG */
  return rval;
}

