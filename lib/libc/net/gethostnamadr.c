/*-
 * Copyright (c) 1985, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: gethostnamadr.c,v 1.41 2000/01/06 08:24:17 d Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"
#endif
#include "thread_private.h"

#define MULTI_PTRS_ARE_ALIASES 1	/* XXX - experimental */

#define	MAXALIASES	35
#define	MAXADDRS	35

static char *h_addr_ptrs[MAXADDRS + 1];

#ifdef YP
static char *__ypdomain;
#endif

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[BUFSIZ+1];
static union {
	struct in_addr _host_in_addr;
	u_char _host_addr[16];		/* IPv4 or IPv6 */
} _host_addr_u;
#define host_addr _host_addr_u._host_addr
static FILE *hostf = NULL;
static int stayopen = 0;

static void map_v4v6_address __P((const char *src, char *dst));
static void map_v4v6_hostent __P((struct hostent *hp, char **bp, int *len));

#ifdef RESOLVSORT
static void addrsort __P((char **, int));
#endif

int _hokchar __P((const char *));

static const char AskedForGot[] =
			  "gethostby*.getanswer: asked for \"%s\", got \"%s\"";

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

typedef union {
	int32_t al;
	char ac;
} align;

static struct hostent *getanswer __P((const querybuf *, int, const char *,
				      int));

extern int h_errno;

int
_hokchar(p)
	const char *p;
{
	char c;

	/*
	 * Many people do not obey RFC 822 and 1035.  The valid
	 * characters are a-z, A-Z, 0-9, '-' and . But the others
	 * tested for below can happen, and we must be more permissive
	 * than the resolver until those idiots clean up their act.
	 * We let '/' through, but not '..'
	 */
	while ((c = *p++)) {
		if (('a' <= c && c <= 'z') ||
		    ('A' <= c && c <= 'Z') ||
		    ('0' <= c && c <= '9'))
			continue;
		if (strchr("-_/", c))
			continue;
		if (c == '.' && *p != '.')
			continue;
		return 0;
	}
	return 1;
}

static struct hostent *
getanswer(answer, anslen, qname, qtype)
	const querybuf *answer;
	int anslen;
	const char *qname;
	int qtype;
{
	register const HEADER *hp;
	register const u_char *cp;
	register int n;
	const u_char *eom;
	char *bp, **ap, **hap;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, had_error;
	int toobig = 0;
	char tbuf[MAXDNAME];
	const char *tname;
	int (*name_ok) __P((const char *));

	tname = qname;
	host.h_name = NULL;
	eom = answer->buf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
#ifdef USE_RESOLV_NAME_OK
		name_ok = res_hnok;
		break;
#endif
	case T_PTR:
#ifdef USE_RESOLV_NAME_OK
		name_ok = res_dnok;
#else
		name_ok = _hokchar;
#endif
		break;
	default:
		return (NULL);
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hostbuf;
	buflen = sizeof hostbuf;
	cp = answer->buf + HFIXEDSZ;
	if (qdcount != 1) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	n = dn_expand(answer->buf, eom, cp, bp, buflen);
	if ((n < 0) || !(*name_ok)(bp)) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		host.h_name = bp;
		bp += n;
		buflen -= n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = host.h_name;
	}
	ap = host_aliases;
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
	host.h_addr_list = h_addr_ptrs;
	haveanswer = 0;
	had_error = 0;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, buflen);
		if ((n < 0) || !(*name_ok)(bp)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		if (type == T_SIG) {
			/* XXX - ignore signatures as we don't use them yet */
			cp += n;
			continue;
		}
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !(*name_ok)(tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Store alias. */
			*ap++ = bp;
			n = strlen(bp) + 1;	/* for the \0 */
			bp += n;
			buflen -= n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > buflen) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			host.h_name = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
#ifdef USE_RESOLV_NAME_OK
			if ((n < 0) || !res_hnok(tbuf)) {
#else
			if ((n < 0) || !_hokchar(tbuf)) {
#endif
				had_error++;
				continue;
			}
			cp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > buflen) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			tname = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (type != qtype) {
			syslog(LOG_NOTICE|LOG_AUTH,
	       "gethostby*.getanswer: asked for \"%s %s %s\", got type \"%s\"",
			       qname, p_class(C_IN), p_type(qtype),
			       p_type(type));
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_PTR:
			if (strcasecmp(tname, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, qname, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			n = dn_expand(answer->buf, eom, cp, bp, buflen);
#ifdef USE_RESOLV_NAME_OK
			if ((n < 0) || !res_hnok(bp)) {
#else
			if ((n < 0) || !_hokchar(bp)) {
#endif
				had_error++;
				break;
			}
#if MULTI_PTRS_ARE_ALIASES
			cp += n;
			if (!haveanswer)
				host.h_name = bp;
			else if (ap < &host_aliases[MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
				buflen -= n;
			}
			break;
#else
			host.h_name = bp;
			if (_res.options & RES_USE_INET6) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
				buflen -= n;
				map_v4v6_hostent(&host, &bp, &buflen);
			}
			h_errno = NETDB_SUCCESS;
			return (&host);
#endif
		case T_A:
		case T_AAAA:
			if (strcasecmp(host.h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, host.h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (!haveanswer) {
				register int nn;

				host.h_name = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
				buflen -= nn;
			}

			bp += sizeof(align) - ((u_long)bp % sizeof(align));

			if (bp + n >= &hostbuf[sizeof hostbuf]) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("size (%d) too big\n", n);
#endif
				had_error++;
				continue;
			}
			if (hap >= &h_addr_ptrs[MAXADDRS-1]) {
				if (!toobig++)
#ifdef DEBUG
					if (_res.options & RES_DEBUG)
						printf("Too many addresses (%d)\n", MAXADDRS);
#endif
				cp += n;
				continue;
			}
			bcopy(cp, *hap++ = bp, n);
			bp += n;
			buflen -= n;
			cp += n;
			break;
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
		*hap = NULL;
# if defined(RESOLVSORT)
		/*
		 * Note: we sort even if host can take only one address
		 * in its return structures - should give it the "best"
		 * address in that case, not some random one
		 */
		if (_res.nsort && haveanswer > 1 && qtype == T_A)
			addrsort(h_addr_ptrs, haveanswer);
# endif /*RESOLVSORT*/
		if (!host.h_name) {
			n = strlen(qname) + 1;	/* for the \0 */
			if (n > buflen)
				goto try_again;
			strcpy(bp, qname);
			host.h_name = bp;
			bp += n;
			buflen -= n;
		}
		if (_res.options & RES_USE_INET6)
			map_v4v6_hostent(&host, &bp, &buflen);
		h_errno = NETDB_SUCCESS;
		return (&host);
	}
 try_again:
	h_errno = TRY_AGAIN;
	return (NULL);
}

#ifdef notyet
/*
 * XXX This is an extremely bogus implementation.
 *
 * FreeBSD has this interface:
 *    int gethostbyaddr_r(const char *addr, int len, int type,
 *             struct hostent *result, struct hostent_data *buffer)
 */

struct hostent *
gethostbyname_r(name, hp, buf, buflen, errorp)
	const char * name;
	struct hostent * hp;
	char * buf;
	int buflen;
	int * errorp;
{
	struct hostent *res;

	res = gethostbyname(name);
	*errorp = h_errno;
	if (res == NULL)
		return NULL;
	memcpy(hp, res, sizeof *hp); /* XXX not sufficient */
	return hp;
}

/*
 * XXX This is an extremely bogus implementation.
 */
struct hostent *
gethostbyaddr_r(addr, len, af, he, buf, buflen, errorp)
	const char *addr;	/* XXX should have been def'd as u_char! */
	int len, af;
	struct hostent * he;
	char * buf;
	int buflen;
	int * errorp;
{
	struct hostent * res;

	res = gethostbyaddr(addr, len, af);
	*errorp = h_errno;
	if (res == NULL)
		return NULL;
	memcpy(he, res, sizeof *he); /* XXX not sufficient */
	return he;
}

/* XXX RFC2133 expects a gethostbyname2_r() -- unimplemented */
#endif

_THREAD_PRIVATE_MUTEX(gethostnamadr);

struct hostent *
gethostbyname(name)
	const char *name;
{
	struct hostent *hp;
	extern struct hostent *_gethtbyname2();

	_THREAD_PRIVATE_MUTEX_LOCK(gethostnamadr);
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		hp = _gethtbyname2(name, AF_INET);

	else if (_res.options & RES_USE_INET6) {
		hp = gethostbyname2(name, AF_INET6);
		if (hp == NULL)
			hp = gethostbyname2(name, AF_INET);
	}
	else
		hp = gethostbyname2(name, AF_INET);
	_THREAD_PRIVATE_MUTEX_UNLOCK(gethostnamadr);
	return hp;
}

struct hostent *
gethostbyname2(name, af)
	const char *name;
	int af;
{
	querybuf buf;
	register const char *cp;
	char *bp;
	int n, size, type, len, i;
	extern struct hostent *_gethtbyname2(), *_yp_gethtbyname();
	register struct hostent *hp;
	char lookups[MAXDNSLUS];

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (_gethtbyname2(name, af));

	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		type = T_A;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		type = T_AAAA;
		break;
	default:
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return (NULL);
	}

	host.h_addrtype = af;
	host.h_length = size;

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_query() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = __hostalias(name)))
		name = cp;

	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit(name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (inet_pton(af, name, host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return (NULL);
				}
				strncpy(hostbuf, name, MAXHOSTNAMELEN-1);
				hostbuf[MAXHOSTNAMELEN-1] = '\0';
				bp = hostbuf + MAXHOSTNAMELEN;
				len = sizeof hostbuf - MAXHOSTNAMELEN;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				if (_res.options & RES_USE_INET6)
					map_v4v6_hostent(&host, &bp, &len);
				h_errno = NETDB_SUCCESS;
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}
	if ((isxdigit(name[0]) && strchr(name, ':') != NULL) ||
	    name[0] == ':')
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-IPv6-legal, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (inet_pton(af, name, host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return (NULL);
				}
				strncpy(hostbuf, name, MAXHOSTNAMELEN-1);
				hostbuf[MAXHOSTNAMELEN-1] = '\0';
				bp = hostbuf + MAXHOSTNAMELEN;
				len = sizeof hostbuf - MAXHOSTNAMELEN;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				h_errno = NETDB_SUCCESS;
				return (&host);
			}
			if (!isxdigit(*cp) && *cp != ':' && *cp != '.') 
				break;
		}

	bcopy(_res.lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strncpy(lookups, "bf", sizeof lookups);

	hp = (struct hostent *)NULL;
	for (i = 0; i < MAXDNSLUS && hp == NULL && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			/* YP only supports AF_INET. */
			if (af == AF_INET)
				hp = _yp_gethtbyname(name);
			break;
#endif
		case 'b':
			if ((n = res_search(name, C_IN, type, buf.buf,
			    sizeof(buf))) < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("res_search failed\n");
#endif
				break;
			}
			hp = getanswer(&buf, n, name, type);
			break;
		case 'f':
			hp = _gethtbyname2(name, af);
			break;
		}
	}
	/* XXX h_errno not correct in all cases... */
	return (hp);
}

struct hostent *
gethostbyaddr(addr, len, af)
	const char *addr;	/* XXX should have been def'd as u_char! */
	int len, af;
{
	const u_char *uaddr = (const u_char *)addr;
	int n, size, i;
	querybuf buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME+1], *qp;
	extern struct hostent *_gethtbyaddr(), *_yp_gethtbyaddr();
	char lookups[MAXDNSLUS];
	struct hostent *res;
	
	_THREAD_PRIVATE_MUTEX_LOCK(gethostnamadr);
	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		res = _gethtbyaddr(addr, len, af);
		_THREAD_PRIVATE_MUTEX_UNLOCK(gethostnamadr);
		return (res);
	}

	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)uaddr) ||
	     IN6_IS_ADDR_V4COMPAT((struct in6_addr *)uaddr))) {
		/* Unmap. */
		addr += IN6ADDRSZ - INADDRSZ;
		uaddr += IN6ADDRSZ - INADDRSZ;
		af = AF_INET;
		len = INADDRSZ;
	}
	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		_THREAD_PRIVATE_MUTEX_UNLOCK(gethostnamadr);
		return (NULL);
	}
	if (size != len) {
		errno = EINVAL;
		h_errno = NETDB_INTERNAL;
		_THREAD_PRIVATE_MUTEX_UNLOCK(gethostnamadr);
		return (NULL);
	}
	switch (af) {
	case AF_INET:
		(void) sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
			       (uaddr[3] & 0xff),
			       (uaddr[2] & 0xff),
			       (uaddr[1] & 0xff),
			       (uaddr[0] & 0xff));
		break;
	case AF_INET6:
		qp = qbuf;
		for (n = IN6ADDRSZ - 1; n >= 0; n--) {
			qp += sprintf(qp, "%x.%x.",
				       uaddr[n] & 0xf,
				       (uaddr[n] >> 4) & 0xf);
		}
		strcpy(qp, "ip6.int");
		break;
	}

	bcopy(_res.lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strncpy(lookups, "bf", sizeof lookups);

	hp = (struct hostent *)NULL;
	for (i = 0; i < MAXDNSLUS && hp == NULL && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			/* YP only supports AF_INET. */
			if (af == AF_INET)
				hp = _yp_gethtbyaddr(addr);
			break;
#endif
		case 'b':
			n = res_query(qbuf, C_IN, T_PTR, (u_char *)buf.buf,
			    sizeof buf.buf);
			if (n < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("res_query failed\n");
#endif
				break;
			}
			if (!(hp = getanswer(&buf, n, qbuf, T_PTR)))
				break;
			hp->h_addrtype = af;
			hp->h_length = len;
			bcopy(addr, host_addr, len);
			h_addr_ptrs[0] = (char *)host_addr;
			h_addr_ptrs[1] = NULL;
			if (af == AF_INET && (_res.options & RES_USE_INET6)) {
				map_v4v6_address((char*)host_addr,
				    (char*)host_addr);
				hp->h_addrtype = AF_INET6;
				hp->h_length = IN6ADDRSZ;
			}
			h_errno = NETDB_SUCCESS;
			break;
		case 'f':
			hp = _gethtbyaddr(addr, len, af);
			break;
		}
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(gethostnamadr);
	/* XXX h_errno not correct in all cases... */
	return (hp);
}

void
_sethtent(f)
	int f;
{
	if (hostf == NULL)
		hostf = fopen(_PATH_HOSTS, "r" );
	else
		rewind(hostf);
	stayopen = f;
}

void
_endhtent()
{
	if (hostf && !stayopen) {
		(void) fclose(hostf);
		hostf = NULL;
	}
}

struct hostent *
_gethtent()
{
	char *p;
	register char *cp, **q;
	int af;
	size_t len;

	if (!hostf && !(hostf = fopen(_PATH_HOSTS, "r" ))) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
 again:
	if ((p = fgetln(hostf, &len)) == NULL) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	if (p[len-1] == '\n')
		len--;
	if (len >= sizeof(hostbuf) || len == 0)
		goto again;
	p = memcpy(hostbuf, p, len);
	hostbuf[len] = '\0';
	if (*p == '#')
		goto again;
	if ((cp = strchr(p, '#')))
		*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if (inet_pton(AF_INET6, p, host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_pton(AF_INET, p, host_addr) > 0) {
		if (_res.options & RES_USE_INET6) {
			map_v4v6_address((char*)host_addr, (char*)host_addr);
			af = AF_INET6;
			len = IN6ADDRSZ;
		} else {
			af = AF_INET;
			len = INADDRSZ;
		}
	} else {
		goto again;
	}
	/* if this is not something we're looking for, skip it. */
	if (host.h_addrtype != af)
		goto again;
	if (host.h_length != len)
		goto again;
	h_addr_ptrs[0] = (char *)host_addr;
	h_addr_ptrs[1] = NULL;
	host.h_addr_list = h_addr_ptrs;
	host.h_length = len;
	host.h_addrtype = af;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	host.h_name = cp;
	q = host.h_aliases = host_aliases;
	if ((cp = strpbrk(cp, " \t")))
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		if ((cp = strpbrk(cp, " \t")))
			*cp++ = '\0';
	}
	*q = NULL;
	if (_res.options & RES_USE_INET6) {
		char *bp = hostbuf;
		int buflen = sizeof hostbuf;

		map_v4v6_hostent(&host, &bp, &buflen);
	}
	h_errno = NETDB_SUCCESS;
	return (&host);
}

struct hostent *
_gethtbyname(name)
	const char *name;
{
	extern struct hostent *_gethtbyname2();
	struct hostent *hp;

	if (_res.options & RES_USE_INET6) {
		hp = _gethtbyname2(name, AF_INET6);
		if (hp)
			return (hp);
	}
	return (_gethtbyname2(name, AF_INET));
}

struct hostent *
_gethtbyname2(name, af)
	const char *name;
	int af;
{
	register struct hostent *p;
	register char **cp;
	
	_sethtent(0);
	while ((p = _gethtent())) {
		if (p->h_addrtype != af)
			continue;
		if (strcasecmp(p->h_name, name) == 0)
			break;
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
 found:
	_endhtent();
	return (p);
}

struct hostent *
_gethtbyaddr(addr, len, af)
	const char *addr;
	int len, af;
{
	register struct hostent *p;

	host.h_length = len;
	host.h_addrtype = af;

	_sethtent(0);
	while ((p = _gethtent()))
		if (p->h_addrtype == af && !bcmp(p->h_addr, addr, len))
			break;
	_endhtent();
	return (p);
}

#ifdef YP
struct hostent *
_yphostent(line)
	char *line;
{
	static struct in_addr host_addrs[MAXADDRS];
	char *p = line;
	char *cp, **q;
	char **hap;
	struct in_addr *buf;
	int more;

	host.h_name = NULL;
	host.h_addr_list = h_addr_ptrs;
	host.h_length = INADDRSZ;
	host.h_addrtype = AF_INET;
	hap = h_addr_ptrs;
	buf = host_addrs;
	q = host.h_aliases = host_aliases;

nextline:
	more = 0;
	cp = strpbrk(p, " \t");
	if (cp == NULL) {
		if (host.h_name == NULL)
			return (NULL);
		else
			goto done;
	}
	*cp++ = '\0';

	*hap++ = (char *)buf;
	(void) inet_aton(p, buf++);

	while (*cp == ' ' || *cp == '\t')
		cp++;
	p = cp;
	cp = strpbrk(p, " \t\n");
	if (cp != NULL) {
		if (*cp == '\n')
			more = 1;
		*cp++ = '\0';
	}
	if (!host.h_name)
		host.h_name = p;
	else if (strcmp(host.h_name, p)==0)
		;
	else if (q < &host_aliases[MAXALIASES - 1])
		*q++ = p;
	p = cp;
	if (more)
		goto nextline;

	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (*cp == '\n') {
			cp++;
			goto nextline;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
done:
	*q = NULL;
	*hap = NULL;
	return (&host);
}

struct hostent *
_yp_gethtbyaddr(addr)
	const char *addr;
{
	struct hostent *hp = (struct hostent *)NULL;
	static char *__ypcurrent;
	int __ypcurrentlen, r;
	char name[sizeof("xxx.xxx.xxx.xxx")];
	
	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return (hp);
	}
	sprintf(name, "%u.%u.%u.%u",
		((unsigned)addr[0] & 0xff),
		((unsigned)addr[1] & 0xff),
		((unsigned)addr[2] & 0xff),
		((unsigned)addr[3] & 0xff));
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	r = yp_match(__ypdomain, "hosts.byaddr", name,
		strlen(name), &__ypcurrent, &__ypcurrentlen);
	if (r==0)
		hp = _yphostent(__ypcurrent);
	if (hp==NULL)
		h_errno = HOST_NOT_FOUND;
	return (hp);
}

struct hostent *
_yp_gethtbyname(name)
	const char *name;
{
	struct hostent *hp = (struct hostent *)NULL;
	static char *__ypcurrent;
	int __ypcurrentlen, r;

	if (strlen(name) >= MAXHOSTNAMELEN)
		return (NULL);
	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return (hp);
	}
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	r = yp_match(__ypdomain, "hosts.byname", name,
		strlen(name), &__ypcurrent, &__ypcurrentlen);
	if (r == 0)
		hp = _yphostent(__ypcurrent);
	if (hp == NULL)
		h_errno = HOST_NOT_FOUND;
	return (hp);
}
#endif

static void
map_v4v6_address(src, dst)
	const char *src;
	char *dst;
{
	u_char *p = (u_char *)dst;
	char tmp[INADDRSZ];
	int i;

	/* Stash a temporary copy so our caller can update in place. */
	bcopy(src, tmp, INADDRSZ);
	/* Mark this ipv6 addr as a mapped ipv4. */
	for (i = 0; i < 10; i++)
		*p++ = 0x00;
	*p++ = 0xff;
	*p++ = 0xff;
	/* Retrieve the saved copy and we're done. */
	bcopy(tmp, (void*)p, INADDRSZ);
}

static void
map_v4v6_hostent(hp, bpp, lenp)
	struct hostent *hp;
	char **bpp;
	int *lenp;
{
	char **ap;

	if (hp->h_addrtype != AF_INET || hp->h_length != INADDRSZ)
		return;
	hp->h_addrtype = AF_INET6;
	hp->h_length = IN6ADDRSZ;
	for (ap = hp->h_addr_list; *ap; ap++) {
		int i = sizeof(align) - ((u_long)*bpp % sizeof(align));

		if (*lenp < (i + IN6ADDRSZ)) {
			/* Out of memory.  Truncate address list here.  XXX */
			*ap = NULL;
			return;
		}
		*bpp += i;
		*lenp -= i;
		map_v4v6_address(*ap, *bpp);
		*ap = *bpp;
		*bpp += IN6ADDRSZ;
		*lenp -= IN6ADDRSZ;
	}
}

struct hostent *
gethostent()
{
	return (_gethtent());
}

#ifdef RESOLVSORT
static void
addrsort(ap, num)
	char **ap;
	int num;
{
	int i, j;
	char **p;
	short aval[MAXADDRS];
	int needsort = 0;

	p = ap;
	for (i = 0; i < num; i++, p++) {
	    for (j = 0 ; (unsigned)j < _res.nsort; j++)
		if (_res.sort_list[j].addr.s_addr == 
		    (((struct in_addr *)(*p))->s_addr & _res.sort_list[j].mask))
			break;
	    aval[i] = j;
	    if (needsort == 0 && i > 0 && j < aval[i-1])
		needsort = i;
	}
	if (!needsort)
	    return;

	while (needsort < num) {
	    for (j = needsort - 1; j >= 0; j--) {
		if (aval[j] > aval[j+1]) {
		    char *hp;

		    i = aval[j];
		    aval[j] = aval[j+1];
		    aval[j+1] = i;

		    hp = ap[j];
		    ap[j] = ap[j+1];
		    ap[j+1] = hp;

		} else
		    break;
	    }
	    needsort++;
	}
}
#endif
