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
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <resolv.h>

#ifndef min
#define min(x,y) (((x) > (y)) ? (y) : (x))
#endif				/* min */

static int 
netdb_lookup_name(int family, void *addr, int addrlen, char *name,
    int namelen, int flags)
{
	struct hostent *hostent;
	char   *c, *c2;
	int     i;

	if (!(hostent = gethostbyaddr(addr, addrlen, family))) {
		switch (h_errno) {
		case NETDB_INTERNAL:
			return(EAI_SYSTEM);
		case HOST_NOT_FOUND:
			return(1);
		case TRY_AGAIN:
			return(EAI_AGAIN);
		case NO_RECOVERY:
			return(EAI_FAIL);
		case NO_DATA:
			return(1);
		default:
			return(EAI_FAIL);
		}
	}

	endhostent();

	c = hostent->h_name;
	if ((flags & NI_NOFQDN) && (_res.options & RES_INIT) && _res.defdname[0] &&
	    (c2 = strstr(c + 1, _res.defdname)) && (*(--c2) == '.')) {
		*c2 = 0;
		i = min(c2 - c, namelen);
		strlcpy(name, c, i);
	} else
		strlcpy(name, c, namelen);
	return 0;
}

int 
getnameinfo(const struct sockaddr *sa, size_t addrlen, char *host,
    size_t hostlen, char *serv, size_t servlen, int flags)
{
	int     rval;
	int     saved_errno;

	if (sa == NULL || addrlen != sa->sa_len)
		return EAI_FAIL;
	saved_errno = errno;

	if (host && hostlen > 0) {
		switch (sa->sa_family) {
		case AF_INET6:
		    {
			struct sockaddr_in6 *sin6 = (void *)sa;

			if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
				if (flags & NI_NUMERICHOST)
					goto inet6_noname;
				strlcpy(host, "*", hostlen);
				break;
			}

			if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
				struct sockaddr_in sin;

				memset(&sin, 0, sizeof(struct sockaddr_in));
				sin.sin_len = sizeof(struct sockaddr_in);
				sin.sin_family = AF_INET;
				sin.sin_port = sin6->sin6_port;
				sin.sin_addr.s_addr =
				    ((u_int32_t *)&sin6->sin6_addr)[3];
				if (!(rval = getnameinfo((struct sockaddr *)&sin,
				    sizeof(struct sockaddr_in), host, hostlen,
				    serv, servlen, flags | NI_NAMEREQD)))
					goto ret;
				if (rval != EAI_NONAME)
					goto ret;
				goto inet6_noname;
			}

			if (flags & NI_NUMERICHOST)
				goto inet6_noname;
			if ((rval = netdb_lookup_name(AF_INET6,
			    &sin6->sin6_addr, sizeof(struct in6_addr),
			    host, hostlen, flags)) < 0)
				goto ret;

			if (!rval)
				break;
	inet6_noname:
			if (flags & NI_NAMEREQD) {
				rval = EAI_NONAME;
				goto ret;
			}
			if (!inet_ntop(AF_INET6, &sin6->sin6_addr, host, hostlen)) {
				rval = EAI_NONAME;
				goto ret;
			}
			break;
		    }
		case AF_INET:
		    {
			struct sockaddr_in *sin = (void *)sa;

			if (flags & NI_NUMERICHOST)
				goto inet_noname;

			if (sin->sin_addr.s_addr == 0) {
				strlcpy(host, "*", hostlen);
				break;
			}

			if ((rval = netdb_lookup_name(AF_INET,
			    &sin->sin_addr, sizeof(struct in_addr),
			    host, hostlen, flags)) < 0)
				goto ret;

			if (!rval)
				break;
	inet_noname:
			if (flags & NI_NAMEREQD) {
				rval = EAI_NONAME;
				goto ret;
			}
			if (!inet_ntop(AF_INET, &sin->sin_addr, host, hostlen)) {
				rval = EAI_NONAME;
				goto ret;
			}
			break;
		    }
		case AF_LOCAL:
			if (!(flags & NI_NUMERICHOST)) {
				struct utsname utsname;

				if (!uname(&utsname)) {
					strlcpy(host, utsname.nodename, hostlen);
					break;
				}
			}

			if (flags & NI_NAMEREQD) {
				rval = EAI_NONAME;
				goto ret;
			}

			strlcpy(host, "localhost", hostlen);
			break;
		default:
			rval = EAI_FAMILY;
			goto ret;
		}
	}

	if (serv && servlen > 0) {
		switch (sa->sa_family) {
		case AF_INET:
		    {
			struct sockaddr_in *sin = (void *)sa;
			struct servent *s;

			if ((flags & NI_NUMERICSERV) == 0) {
				s = getservbyport(sin->sin_port,
				    (flags & NI_DGRAM) ? "udp" : "tcp");
				if (s) {
					strlcpy(serv, s->s_name, servlen);
					break;
				}
				if (sin->sin_port == 0) {
					strlcpy(serv, "*", servlen);
					break;
				}
			}
			snprintf(serv, servlen, "%d", ntohs(sin->sin_port));
			break;
		    }
		case AF_INET6:
		    {
			struct sockaddr_in6 *sin6 = (void *)sa;
			struct servent *s;

			if ((flags & NI_NUMERICSERV) == 0) {

				s = getservbyport(sin6->sin6_port,
				    (flags & NI_DGRAM) ? "udp" : "tcp");
				if (s) {
					strlcpy(serv, s->s_name, servlen);
					break;
				}
				if (sin6->sin6_port == 0) {
					strlcpy(serv, "*", servlen);
					break;
				}
			}
			snprintf(serv, servlen, "%d", ntohs(sin6->sin6_port));
			break;
		    }
		case AF_LOCAL:
		    {
			struct sockaddr_un *sun = (void *)sa;

			strlcpy(serv, sun->sun_path, servlen);
			break;
		    }
		}
	}
	rval = 0;

ret:
	if (rval == 1)
		rval = EAI_FAIL;
	errno = saved_errno;
	return (rval);
}
