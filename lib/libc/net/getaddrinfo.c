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

/* getaddrinfo() v1.38 */

/*
   I'd like to thank Matti Aarnio for finding some bugs in this code and
   sending me patches, as well as everyone else who has contributed to this
   code (by reporting bugs, suggesting improvements, and commented on its
   behavior and proposed changes).
*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif /* AF_LOCAL */
#ifndef PF_LOCAL
#define PF_LOCAL PF_UNIX
#endif /* PF_LOCAL */
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif /* UNIX_PATH_MAX */

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

#define RETURN_ERROR(x) do { \
    rval = (x); \
    goto ret; \
    } while(0)

static int netdb_lookup_addr(const char *name, int af, const struct addrinfo *req, struct gaih_addrtuple **pat)
{
  int rval, herrno, i;
  char *prevcname = NULL;
  struct hostent *h;

  h = gethostbyname2(name, af);
  herrno = h_errno;

  if (!h) {
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
      case AF_INET6:
	memcpy((*pat)->addr, h->h_addr_list[i], sizeof(struct in6_addr));
	break;
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
  return rval;
};

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
  ((struct sockaddr_un *)(*pai)->ai_addr)->sun_len = sizeof(struct sockaddr_un);
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

  if (!(s = getservbyname(servicename, tp->name)))
    RETURN_ERROR(GAIH_OKIFUNSPEC | -EAI_SERVICE);

  if (!(*st = malloc(sizeof(struct gaih_servtuple))))
    RETURN_ERROR(-EAI_MEMORY);

  (*st)->next = NULL;
  (*st)->socktype = tp->socktype;
  (*st)->protocol = tp->protocol;
  (*st)->port = s->s_port;

  rval = 0;

ret:
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

    if (req->ai_family)
      at->family = req->ai_family;
    else {
      if (!(at->next = malloc(sizeof(struct gaih_addrtuple))))
	RETURN_ERROR(-EAI_MEMORY);

      at->family = AF_INET6;

      memset(at->next, 0, sizeof(struct gaih_addrtuple));
      at->next->family = AF_INET;
    };

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

  if (!(req->ai_flags & AI_NUMERICHOST)) {
    if (!req->ai_family || (req->ai_family == AF_INET6))
      if ((rval = netdb_lookup_addr(name, AF_INET6, req, &at)) < 0)
	goto ret;
    if (!req->ai_family || (req->ai_family == AF_INET))
      if ((rval = netdb_lookup_addr(name, AF_INET, req, &at)) < 0)
	goto ret;

    if (!rval)
      goto build;
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

      if (at2->family == AF_INET6)
	i = sizeof(struct sockaddr_in6);
      else
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
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_len = i;
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_family = at2->family;
	((struct sockaddr_in *)(*pai)->ai_addr)->sin_port = st2->port;

	if (at2->family == AF_INET6)
	  memcpy(&((struct sockaddr_in6 *)(*pai)->ai_addr)->sin6_addr, at2->addr, sizeof(struct in6_addr));
	else
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
  { PF_INET6, "inet6", gaih_inet },
  { PF_INET, "inet", gaih_inet },
  { PF_LOCAL, "local", gaih_local },
  { -1, NULL, NULL }
};

int getaddrinfo(const char *name, const char *service,
		const struct addrinfo *req, struct addrinfo **pai)
{
  int rval = EAI_SYSTEM; /* XXX */
  int i, j = 0;
  struct addrinfo *p = NULL, **end;
  struct gaih *g = gaih, *pg = NULL;
  struct gaih_service gaih_service, *pservice;

  if (name && (name[0] == '*') && !name[1])
    name = NULL;

  if (service && (service[0] == '*') && !service[1])
    service = NULL;

  if (!req)
    req = &nullreq;

  if (req->ai_flags & ~(AI_CANONNAME | AI_PASSIVE | AI_NUMERICHOST | AI_EXT))
    RETURN_ERROR(EAI_BADFLAGS);

  if (service && *service) {
    char *c;
    gaih_service.num = strtoul(gaih_service.name = (void *)service, &c, 10);
    if (*c) {
      gaih_service.num = -1;
    }

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
    rval = 0;
    goto ret;
  }

  if (!pai && !rval) {
    rval = 0;
    goto ret;
  };

  RETURN_ERROR(EAI_NONAME);

ret:
  return rval;
}

