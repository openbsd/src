/*	$OpenBSD: getnetnamadr.c,v 1.26 2005/08/06 20:30:03 espie Exp $	*/

/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Copyright (c) 1993 Carlos Leandro and Rui Salgueiro
 *	Dep. Matematica Universidade de Coimbra, Portugal, Europe
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "thread_private.h"

extern int h_errno;

struct netent *_getnetbyaddr(in_addr_t net, int type);
struct netent *_getnetbyname(const char *name);

int _hokchar(const char *);

#define BYADDR 0
#define BYNAME 1
#define	MAXALIASES	35

#define	MAXPACKET	(64*1024)

typedef union {
	HEADER	hdr;
	u_char	buf[MAXPACKET];
} querybuf;

typedef union {
	long	al;
	char	ac;
} align;

static struct netent *
getnetanswer(querybuf *answer, int anslen, int net_i)
{

	HEADER *hp;
	u_char *cp;
	int n;
	u_char *eom;
	int type, class, ancount, qdcount, haveanswer, i, nchar;
	char aux1[MAXHOSTNAMELEN], aux2[MAXHOSTNAMELEN], ans[MAXHOSTNAMELEN];
	char *in, *st, *pauxt, *bp, **ap, *ep;
	char *paux1 = &aux1[0], *paux2 = &aux2[0];
	static	struct netent net_entry;
	static	char *net_aliases[MAXALIASES], netbuf[BUFSIZ+1];

	/*
	 * find first satisfactory answer
	 *
	 *      answer --> +------------+  ( MESSAGE )
	 *		   |   Header   |
	 *		   +------------+
	 *		   |  Question  | the question for the name server
	 *		   +------------+
	 *		   |   Answer   | RRs answering the question
	 *		   +------------+
	 *		   | Authority  | RRs pointing toward an authority
	 *		   | Additional | RRs holding additional information
	 *		   +------------+
	 */
	eom = answer->buf + anslen;
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount); /* #/records in the answer section */
	qdcount = ntohs(hp->qdcount); /* #/entries in the question section */
	bp = netbuf;
	ep = netbuf + sizeof(netbuf);
	cp = answer->buf + HFIXEDSZ;
	if (!qdcount) {
		if (hp->aa)
			h_errno = HOST_NOT_FOUND;
		else
			h_errno = TRY_AGAIN;
		return (NULL);
	}
	while (qdcount-- > 0) {
		n = __dn_skipname(cp, eom);
		if (n < 0 || (cp + n + QFIXEDSZ) > eom) {
			h_errno = NO_RECOVERY;
			return(NULL);
		}
		cp += n + QFIXEDSZ;
	}
	ap = net_aliases;
	*ap = NULL;
	net_entry.n_aliases = net_aliases;
	haveanswer = 0;
	while (--ancount >= 0 && cp < eom) {
		n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
#ifdef USE_RESOLV_NAME_OK
		if ((n < 0) || !res_dnok(bp))
#else
		if ((n < 0) || !_hokchar(bp))
#endif
			break;
		cp += n;
		ans[0] = '\0';
		strlcpy(&ans[0], bp, sizeof ans);
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		cp += INT32SZ;		/* TTL */
		GETSHORT(n, cp);
		if (class == C_IN && type == T_PTR) {
			n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
#ifdef USE_RESOLV_NAME_OK
			if ((n < 0) || !res_hnok(bp))
#else
			if ((n < 0) || !_hokchar(bp))
#endif
			{
				cp += n;
				return (NULL);
			}
			cp += n; 
			if ((ap + 2) < &net_aliases[MAXALIASES]) {
				*ap++ = bp;
				bp += strlen(bp) + 1;
				net_entry.n_addrtype =
					(class == C_IN) ? AF_INET : AF_UNSPEC;
				haveanswer++;
			}
		}
	}
	if (haveanswer) {
		*ap = NULL;
		switch (net_i) {
		case BYADDR:
			net_entry.n_name = *net_entry.n_aliases;
			net_entry.n_net = 0L;
			break;
		case BYNAME:
			ap = net_entry.n_aliases;
		next_alias:
			in = *ap++;
			if (in == NULL) {
				h_errno = HOST_NOT_FOUND;
				return (NULL);
			}
			net_entry.n_name = ans;
			aux2[0] = '\0';
			for (i = 0; i < 4; i++) {
				for (st = in, nchar = 0;
				     isdigit((unsigned char)*st);
				     st++, nchar++)
					;
				if (*st != '.' || nchar == 0 || nchar > 3)
					goto next_alias;
				if (i != 0)
					nchar++;
				strlcpy(paux1, in, nchar+1);
				strlcat(paux1, paux2, MAXHOSTNAMELEN);
				pauxt = paux2;
				paux2 = paux1;
				paux1 = pauxt;
				in = ++st;
			}		  
			if (strcasecmp(in, "IN-ADDR.ARPA") != 0)
				goto next_alias;
			net_entry.n_net = inet_network(paux2);
			break;
		}
		net_entry.n_aliases++;
		return (&net_entry);
	}
	h_errno = TRY_AGAIN;
	return (NULL);
}

struct netent *
getnetbyaddr(in_addr_t net, int net_type)
{
	struct __res_state *_resp = _THREAD_PRIVATE(_res, _res, &_res);
	unsigned int netbr[4];
	int nn, anslen;
	querybuf *buf;
	char qbuf[MAXDNAME];
	in_addr_t net2;
	struct netent *net_entry = NULL;
	char lookups[MAXDNSLUS];
	int i;

	if (_res_init(0) == -1)
		return(_getnetbyaddr(net, net_type));

	bcopy(_resp->lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strlcpy(lookups, "bf", sizeof lookups);

	for (i = 0; i < MAXDNSLUS && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			/* There is no YP support. */
			break;
#endif	/* YP */
		case 'b':
			if (net_type != AF_INET)
				break;	/* DNS only supports AF_INET? */

			for (nn = 4, net2 = net; net2; net2 >>= 8)
				netbr[--nn] = net2 & 0xff;
			switch (nn) {
			case 3: 	/* Class A */
				snprintf(qbuf, sizeof(qbuf),
				    "0.0.0.%u.in-addr.arpa", netbr[3]);
				break;
			case 2: 	/* Class B */
				snprintf(qbuf, sizeof(qbuf),
				    "0.0.%u.%u.in-addr.arpa",
		    		    netbr[3], netbr[2]);
				break;
			case 1: 	/* Class C */
				snprintf(qbuf, sizeof(qbuf),
				    "0.%u.%u.%u.in-addr.arpa",
		    		    netbr[3], netbr[2], netbr[1]);
				break;
			case 0: 	/* Class D - E */
				snprintf(qbuf, sizeof(qbuf),
				    "%u.%u.%u.%u.in-addr.arpa",
				    netbr[3], netbr[2], netbr[1], netbr[0]);
				break;
			}
			buf = malloc(sizeof(*buf));
			if (buf == NULL)
				break;
			anslen = res_query(qbuf, C_IN, T_PTR, buf->buf,
			    sizeof(buf->buf));
			if (anslen < 0) {
				free(buf);
#ifdef DEBUG
				if (_resp->options & RES_DEBUG)
					printf("res_query failed\n");
#endif
				break;
			}
			net_entry = getnetanswer(buf, anslen, BYADDR);
			free(buf);
			if (net_entry != NULL) {
				unsigned u_net = net;	/* maybe net should be unsigned ? */

				/* Strip trailing zeros */
				while ((u_net & 0xff) == 0 && u_net != 0)
					u_net >>= 8;
				net_entry->n_net = u_net;
				return (net_entry);
			}
			break;
		case 'f':
			net_entry = _getnetbyaddr(net, net_type);
			if (net_entry != NULL)
				return (net_entry);
		}
	}

	/* Nothing matched. */
	return (NULL);
}

struct netent *
getnetbyname(const char *net)
{
	struct __res_state *_resp = _THREAD_PRIVATE(_res, _res, &_res);
	int anslen;
	querybuf *buf;
	char qbuf[MAXDNAME];
	struct netent *net_entry = NULL;
	char lookups[MAXDNSLUS];
	int i;

	if (_res_init(0) == -1)
		return (_getnetbyname(net));

	bcopy(_resp->lookups, lookups, sizeof lookups);
	if (lookups[0] == '\0')
		strlcpy(lookups, "bf", sizeof lookups);

	for (i = 0; i < MAXDNSLUS && lookups[i]; i++) {
		switch (lookups[i]) {
#ifdef YP
		case 'y':
			/* There is no YP support. */
			break;
#endif	/* YP */
		case 'b':
			strlcpy(qbuf, net, sizeof qbuf);
			buf = malloc(sizeof(*buf));
			if (buf == NULL)
				break;
			anslen = res_search(qbuf, C_IN, T_PTR, buf->buf,
			    sizeof(buf->buf));
			if (anslen < 0) {
				free(buf);
#ifdef DEBUG
				if (_resp->options & RES_DEBUG)
					printf("res_query failed\n");
#endif
				break;
			}
			net_entry = getnetanswer(buf, anslen, BYNAME);
			free(buf);
			if (net_entry != NULL)
				return (net_entry);
			break;
		case 'f':
			net_entry = _getnetbyname(net);
			if (net_entry != NULL)
				return (net_entry);
			break;
		}
	}

	/* Nothing matched. */
	return (NULL);
}
