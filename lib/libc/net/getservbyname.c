/*
 * Copyright (c) 1983, 1993
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: getservbyname.c,v 1.5 2000/01/06 08:24:17 d Exp $";
#endif /* LIBC_SCCS and not lint */

#include <netdb.h>
#include <string.h>
#include "thread_private.h"

extern int _serv_stayopen;

_THREAD_PRIVATE_MUTEX(getservbyname_r);

struct servent *
getservbyname_r(name, proto, se, buf, buflen)
	const char *name, *proto;
	struct servent *se;
	char *buf;
	int buflen;
{
	register struct servent *p;
	register char **cp;

	_THREAD_PRIVATE_MUTEX_LOCK(getservbyname_r);
	setservent(_serv_stayopen);
	while ((p = getservent())) {
		if (strcmp(name, p->s_name) == 0)
			goto gotname;
		for (cp = p->s_aliases; *cp; cp++)
			if (strcmp(name, *cp) == 0)
				goto gotname;
		continue;
gotname:
		if (proto == 0 || strcmp(p->s_proto, proto) == 0)
			break;
	}
	if (!_serv_stayopen)
		endservent();
	_THREAD_PRIVATE_MUTEX_UNLOCK(getservbyname_r);
	return (p);
}

struct servent *getservbyname(name, proto)
	const char *name, *proto;
{
	_THREAD_PRIVATE_KEY(getservbyname);
	static char buf[4096];
	char *bufp = (char*)_THREAD_PRIVATE(getservbyname, buf, NULL);

	if (bufp == NULL)
		return (NULL);
	return getservbyname_r(name, proto, (struct servent*) bufp, 
		bufp + sizeof(struct servent), 
		sizeof buf - sizeof(struct servent) );
}
