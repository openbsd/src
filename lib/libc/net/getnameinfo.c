/*	$OpenBSD: getnameinfo.c,v 1.9 2000/02/16 12:53:35 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Issues to be discussed:
 * - Thread safe-ness must be checked
 * - Return values.  There seems to be no standard for return value (RFC2553)
 *   but INRIA implementation returns EAI_xxx defined for getaddrinfo().
 * - RFC2553 says that we should raise error on short buffer.  X/Open says
 *   we need to truncate the result.  We obey RFC2553 (and X/Open should be
 *   modified).
 */

#define INET6

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <string.h>
#include <stddef.h>

#define SUCCESS 0
#define ANY 0
#define YES 1
#define NO  0

static struct afd {
	int a_af;
	int a_addrlen;
	int a_socklen;
	int a_off;
} afdl [] = {
#ifdef INET6
	{PF_INET6, sizeof(struct in6_addr), sizeof(struct sockaddr_in6),
		offsetof(struct sockaddr_in6, sin6_addr)},
#endif
	{PF_INET, sizeof(struct in_addr), sizeof(struct sockaddr_in),
		offsetof(struct sockaddr_in, sin_addr)},
	{0, 0, 0},
};

struct sockinet {
	u_char	si_len;
	u_char	si_family;
	u_short	si_port;
};

#ifdef INET6
static char *ip6_sa2str __P((struct sockaddr_in6 *, char *, int));
#endif 

#define ENI_NOSOCKET 	0
#define ENI_NOSERVNAME	1
#define ENI_NOHOSTNAME	2
#define ENI_MEMORY	3
#define ENI_SYSTEM	4
#define ENI_FAMILY	5
#define ENI_SALEN	6

int
getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)
	const struct sockaddr *sa;
	size_t salen;
	char *host;
	size_t hostlen;
	char *serv;
	size_t servlen;
	int flags;
{
	struct afd *afd;
	struct servent *sp;
	struct hostent *hp;
	u_short port;
	int family, i;
	char *addr, *p;
	u_int32_t v4a;
	int h_error;
	char numserv[512];
	char numaddr[512];

	if (sa == NULL)
		return ENI_NOSOCKET;

	if (sa->sa_len != salen)
		return ENI_SALEN;
	
	family = sa->sa_family;
	for (i = 0; afdl[i].a_af; i++)
		if (afdl[i].a_af == family) {
			afd = &afdl[i];
			goto found;
		}
	return ENI_FAMILY;
	
 found:
	if (salen != afd->a_socklen)
		return ENI_SALEN;
	
	port = ((struct sockinet *)sa)->si_port; /* network byte order */
	addr = (char *)sa + afd->a_off;

	if (serv == NULL || servlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: RFC2553 says that serv == NULL OR servlen == 0
		 * means that the caller does not want the result.
		 */
	} else {
		if (flags & NI_NUMERICSERV)
			sp = NULL;
		else {
			sp = getservbyport(port,
				(flags & NI_DGRAM) ? "udp" : "tcp");
		}
		if (sp) {
			if (strlen(sp->s_name) > servlen)
				return ENI_MEMORY;
			strcpy(serv, sp->s_name);
		} else {
			snprintf(numserv, sizeof(numserv), "%d", ntohs(port));
			if (strlen(numserv) > servlen)
				return ENI_MEMORY;
			strcpy(serv, numserv);
		}
	}

	switch (sa->sa_family) {
	case AF_INET:
		v4a = (u_int32_t)
			ntohl(((struct sockaddr_in *)sa)->sin_addr.s_addr);
		if (IN_MULTICAST(v4a) || IN_EXPERIMENTAL(v4a))
			flags |= NI_NUMERICHOST;
		v4a >>= IN_CLASSA_NSHIFT;
		if (v4a == 0)
			flags |= NI_NUMERICHOST;			
		break;
#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)sa;
		switch (sin6->sin6_addr.s6_addr[0]) {
		case 0x00:
			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
				;
			else if (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))
				;
			else
				flags |= NI_NUMERICHOST;
			break;
		default:
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				flags |= NI_NUMERICHOST;
			}
			else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
				flags |= NI_NUMERICHOST;
			break;
		}
	    }
		break;
#endif
	}
	if (host == NULL || hostlen == 0) {
		/*
		 * do nothing in this case.
		 * in case you are wondering if "&&" is more correct than
		 * "||" here: RFC2553 says that host == NULL OR hostlen == 0
		 * means that the caller does not want the result.
		 */
	} else if (flags & NI_NUMERICHOST) {
		int numaddrlen;

		/* NUMERICHOST and NAMEREQD conflicts with each other */
		if (flags & NI_NAMEREQD)
			return ENI_NOHOSTNAME;
		if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
		    == NULL)
			return ENI_SYSTEM;
		numaddrlen = strlen(numaddr);
		if (numaddrlen + 1 > hostlen) /* don't forget terminator */
			return ENI_MEMORY;
		strcpy(host, numaddr);
#if defined(INET6) && defined(NI_WITHSCOPEID)
		if (afd->a_af == AF_INET6 &&
		    (IN6_IS_ADDR_LINKLOCAL((struct in6_addr *)addr) ||
		     IN6_IS_ADDR_MULTICAST((struct in6_addr *)addr)) &&
		    ((struct sockaddr_in6 *)sa)->sin6_scope_id) {
#ifndef ALWAYS_WITHSCOPE
			if (flags & NI_WITHSCOPEID)
#endif /* !ALWAYS_WITHSCOPE */
			{
				char scopebuf[MAXHOSTNAMELEN], *s;
				int scopelen;

				if ((s = ip6_sa2str((struct sockaddr_in6 *)sa,
						    scopebuf, 0)) == NULL)
					/* XXX what should we do? */
					strcpy(scopebuf, "0");

				scopelen = strlen(scopebuf);
				if (scopelen + 1 + numaddrlen + 1 > hostlen)
					return ENI_MEMORY;

#if 1
				/*
				 * construct <scopeid><delim><numeric-addr>
				 */
				/*
				 * Shift the host string to allocate
				 * space for the scope ID part.
				 */
				memmove(host + scopelen + 1, host,
					numaddrlen);
				/* copy the scope ID and the delimiter */
				memcpy(host, scopebuf, scopelen);
				host[scopelen] = SCOPE_DELIMITER;
				host[scopelen + 1 + numaddrlen] = '\0';
#else
				/*
				 * construct <numeric-addr><delim><scopeid>
				 */
				memcpy(host + numaddrlen + 1, scopebuf,
				    scopelen);
				host[numaddrlen] = SCOPE_DELIMITER;
				host[numaddrlen + 1 + scopelen] = '\0';
#endif
			}
		}
#endif /* INET6 */
	} else {
		hp = gethostbyaddr(addr, afd->a_addrlen, afd->a_af);
		h_error = h_errno;

		if (hp) {
			if (flags & NI_NOFQDN) {
				p = strchr(hp->h_name, '.');
				if (p) *p = '\0';
			}
			if (strlen(hp->h_name) > hostlen) {
				return ENI_MEMORY;
			}
			strcpy(host, hp->h_name);
		} else {
			if (flags & NI_NAMEREQD)
				return ENI_NOHOSTNAME;
			if (inet_ntop(afd->a_af, addr, numaddr, sizeof(numaddr))
			    == NULL)
				return ENI_NOHOSTNAME;
			if (strlen(numaddr) > hostlen)
				return ENI_MEMORY;
			strcpy(host, numaddr);
		}
	}
	return SUCCESS;
}

#ifdef INET6
/* ARGSUSED */
static char *
ip6_sa2str(sa6, buf, flags)
	struct sockaddr_in6 *sa6;
	char *buf;
	int flags;
{
	unsigned int ifindex = (unsigned int)sa6->sin6_scope_id;
	struct in6_addr *a6 = &sa6->sin6_addr;

#ifdef notyet
	if (flags & NI_NUMERICSCOPE) {
		sprintf(buf, "%d", sa6->sin6_scope_id);
		return(buf);
	}
#endif
 
	if (IN6_IS_ADDR_LINKLOCAL(a6) || IN6_IS_ADDR_MC_LINKLOCAL(a6))
		return(if_indextoname(ifindex, buf));

	if (IN6_IS_ADDR_SITELOCAL(a6) || IN6_IS_ADDR_MC_SITELOCAL(a6))
		return(NULL);	/* XXX */
	if (IN6_IS_ADDR_MC_ORGLOCAL(a6))
		return(NULL);	/* XXX */

	return(NULL);		/* XXX */
}
#endif 
