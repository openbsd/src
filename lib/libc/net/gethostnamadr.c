/*	$NetBSD: gethostnamadr.c,v 1.13 1995/05/21 16:21:14 mycroft Exp $	*/

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
#if 0
static char sccsid[] = "@(#)gethostnamadr.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "$Id: gethnamaddr.c,v 4.9.1.1 1993/05/02 22:43:03 vixie Rel ";
#else
static char rcsid[] = "$NetBSD: gethostnamadr.c,v 1.13 1995/05/21 16:21:14 mycroft Exp $";
#endif
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
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#define	MAXALIASES	35
#define	MAXADDRS	35

static char *h_addr_ptrs[MAXADDRS + 1];

#ifdef YP
static char *__ypdomain;
#endif

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[BUFSIZ+1];
static struct in_addr host_addr;
static FILE *hostf = NULL;
static int stayopen = 0;

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

static int qcomp __P((struct in_addr **, struct in_addr **));
static struct hostent *getanswer __P((querybuf *, int, int));

extern int h_errno;

static struct hostent *
getanswer(answer, anslen, iquery)
	querybuf *answer;
	int anslen;
	int iquery;
{
	register HEADER *hp;
	register u_char *cp;
	register int n;
	u_char *eom;
	char *bp, **ap;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, getclass = C_ANY;
	char **hap;

	eom = answer->buf + anslen;
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hostbuf;
	buflen = sizeof(hostbuf);
	cp = answer->buf + sizeof(HEADER);
	if (qdcount) {
		if (iquery) {
			if ((n = dn_expand((u_char *)answer->buf,
			    (u_char *)eom, (u_char *)cp, (u_char *)bp,
			    buflen)) < 0) {
				h_errno = NO_RECOVERY;
				return ((struct hostent *) NULL);
			}
			cp += n + QFIXEDSZ;
			host.h_name = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
		} else
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
		while (--qdcount > 0)
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
	} else if (iquery) {
		if (hp->aa)
			h_errno = HOST_NOT_FOUND;
		else
			h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
	ap = host_aliases;
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
	host.h_addr_list = h_addr_ptrs;
	haveanswer = 0;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand((u_char *)answer->buf, (u_char *)eom,
		    (u_char *)cp, (u_char *)bp, buflen)) < 0)
			break;
		cp += n;
		type = _getshort(cp);
 		cp += sizeof(u_int16_t);
		class = _getshort(cp);
 		cp += sizeof(u_int16_t) + sizeof(u_int32_t);
		n = _getshort(cp);
		cp += sizeof(u_int16_t);
		if (type == T_CNAME) {
			cp += n;
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			*ap++ = bp;
			n = strlen(bp) + 1;
			bp += n;
			buflen -= n;
			continue;
		}
		if (iquery && type == T_PTR) {
			if ((n = dn_expand((u_char *)answer->buf,
			    (u_char *)eom, (u_char *)cp, (u_char *)bp,
			    buflen)) < 0)
				break;
			cp += n;
			host.h_name = bp;
			return(&host);
		}
		if (iquery || type != T_A)  {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
					type, n);
#endif
			cp += n;
			continue;
		}
		if (haveanswer) {
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (class != getclass) {
				cp += n;
				continue;
			}
		} else {
			host.h_length = n;
			getclass = class;
			host.h_addrtype = (class == C_IN) ? AF_INET : AF_UNSPEC;
			if (!iquery) {
				host.h_name = bp;
				bp += strlen(bp) + 1;
			}
		}

		bp += sizeof(align) - ((u_long)bp % sizeof(align));

		if (bp + n >= &hostbuf[sizeof(hostbuf)]) {
#ifdef DEBUG
			if (_res.options & RES_DEBUG)
				printf("size (%d) too big\n", n);
#endif
			break;
		}
		bcopy(cp, *hap++ = bp, n);
		bp +=n;
		cp += n;
		haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
		*hap = NULL;
		if (_res.nsort) {
			qsort(host.h_addr_list, haveanswer,
			    sizeof(struct in_addr),
			    (int (*)__P((const void *, const void *)))qcomp);
		}
		return (&host);
	} else {
		h_errno = TRY_AGAIN;
		return ((struct hostent *) NULL);
	}
}

struct hostent *
gethostbyname(name)
	const char *name;
{
	querybuf buf;
	register const char *cp;
	int n, i;
	extern struct hostent *_gethtbyname(), *_yp_gethtbyname();
	register struct hostent *hp;
	char lookups[MAXDNSLUS];

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
				if (!inet_aton(name, &host_addr)) {
					h_errno = HOST_NOT_FOUND;
					return((struct hostent *) NULL);
				}
				host.h_name = (char *)name;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				host.h_addrtype = AF_INET;
				host.h_length = sizeof(u_int32_t);
				h_addr_ptrs[0] = (char *)&host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (_gethtbyname(name));

	bcopy(_res.lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strncpy(lookups, "bf", sizeof lookups);

	hp = (struct hostent *)NULL;
	for (i = 0; i < MAXDNSLUS && hp == NULL && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			hp = _yp_gethtbyname(name);
			break;
#endif
		case 'b':
			if ((n = res_search(name, C_IN, T_A, buf.buf,
			    sizeof(buf))) < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("res_search failed\n");
#endif
				break;
			}
			hp = getanswer(&buf, n, 0);
			break;
		case 'f':
			hp = _gethtbyname(name);
			break;
		}
	}
	return (hp);
}

struct hostent *
gethostbyaddr(addr, len, type)
	const char *addr;
	int len, type;
{
	int n, i;
	querybuf buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME];
	extern struct hostent *_gethtbyaddr(), *_yp_gethtbyaddr();
	char lookups[MAXDNSLUS];
	
	if (type != AF_INET)
		return ((struct hostent *) NULL);
	(void)sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
		((unsigned)addr[3] & 0xff),
		((unsigned)addr[2] & 0xff),
		((unsigned)addr[1] & 0xff),
		((unsigned)addr[0] & 0xff));

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (_gethtbyaddr(addr, len, type));

	bcopy(_res.lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strncpy(lookups, "bf", sizeof lookups);

	hp = (struct hostent *)NULL;
	for (i = 0; i < MAXDNSLUS && hp == NULL && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			hp = _yp_gethtbyaddr(addr, len, type);
			break;
#endif
		case 'b':
			n = res_query(qbuf, C_IN, T_PTR, (char *)&buf, sizeof(buf));
			if (n < 0) {
#ifdef DEBUG
				if (_res.options & RES_DEBUG)
					printf("res_query failed\n");
#endif
				break;
			}
			hp = getanswer(&buf, n, 1);
			if (hp == NULL)
				break;
			hp->h_addrtype = type;
			hp->h_length = len;
			h_addr_ptrs[0] = (char *)&host_addr;
			h_addr_ptrs[1] = (char *)0;
			host_addr = *(struct in_addr *)addr;
			break;
		case 'f':
			hp = _gethtbyaddr(addr, len, type);
			break;
		}
	}
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

	if (hostf == NULL && (hostf = fopen(_PATH_HOSTS, "r" )) == NULL)
		return (NULL);
again:
	if ((p = fgets(hostbuf, BUFSIZ, hostf)) == NULL)
		return (NULL);
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp == NULL)
		goto again;
	*cp = '\0';
	cp = strpbrk(p, " \t");
	if (cp == NULL)
		goto again;
	*cp++ = '\0';
	/* THIS STUFF IS INTERNET SPECIFIC */
	h_addr_ptrs[0] = (char *)&host_addr;
	h_addr_ptrs[1] = NULL;
	(void) inet_aton(p, &host_addr);
	host.h_addr_list = h_addr_ptrs;
	host.h_length = sizeof(u_int32_t);
	host.h_addrtype = AF_INET;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	host.h_name = cp;
	q = host.h_aliases = host_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL) 
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&host);
}

struct hostent *
_gethtbyname(name)
	char *name;
{
	register struct hostent *p;
	register char **cp;
	
	_sethtent(0);
	while (p = _gethtent()) {
		if (strcasecmp(p->h_name, name) == 0)
			break;
		for (cp = p->h_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
found:
	_endhtent();
	if (p==NULL)
		h_errno = HOST_NOT_FOUND;
	return (p);
}

struct hostent *
_gethtbyaddr(addr, len, type)
	const char *addr;
	int len, type;
{
	register struct hostent *p;

	_sethtent(0);
	while (p = _gethtent())
		if (p->h_addrtype == type && !bcmp(p->h_addr, addr, len))
			break;
	_endhtent();
	if (p==NULL)
		h_errno = HOST_NOT_FOUND;
	return (p);
}

static int
qcomp(a1, a2)
	struct in_addr **a1, **a2;
{
	int pos1, pos2;

	for (pos1 = 0; pos1 < _res.nsort; pos1++)
		if (_res.sort_list[pos1].addr.s_addr ==
		    ((*a1)->s_addr & _res.sort_list[pos1].mask))
			break;
	for (pos2 = 0; pos2 < _res.nsort; pos2++)
		if (_res.sort_list[pos2].addr.s_addr ==
		    ((*a2)->s_addr & _res.sort_list[pos2].mask))
			break;
	return pos1 - pos2;
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
	host.h_length = sizeof(u_int32_t);
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
_yp_gethtbyaddr(addr, len, type)
	const char *addr;
	int len, type;
{
	struct hostent *hp = (struct hostent *)NULL;
	static char *__ypcurrent;
	int __ypcurrentlen, r;
	char name[sizeof("xxx.xxx.xxx.xxx") + 1];
	
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

	if (!__ypdomain) {
		if (_yp_check(&__ypdomain) == 0)
			return (hp);
	}
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	r = yp_match(__ypdomain, "hosts.byname", name,
		strlen(name), &__ypcurrent, &__ypcurrentlen);
	if (r==0)
		hp = _yphostent(__ypcurrent);
	if (hp==NULL)
		h_errno = HOST_NOT_FOUND;
	return (hp);
}
#endif
