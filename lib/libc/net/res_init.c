/*	$OpenBSD: res_init.c,v 1.28 2003/01/28 04:58:00 marc Exp $	*/

/*
 * ++Copyright++ 1985, 1989, 1993
 * -
 * Copyright (c) 1985, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

#ifndef INET6
#define INET6
#endif

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)res_init.c	8.1 (Berkeley) 6/7/93";
static char rcsid[] = "$From: res_init.c,v 8.7 1996/09/28 06:51:07 vixie Exp $";
#else
static char rcsid[] = "$OpenBSD: res_init.c,v 1.28 2003/01/28 04:58:00 marc Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <ctype.h>
#include <resolv.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef INET6
#include <netdb.h>
#endif /* INET6 */

#include "thread_private.h"

/*-------------------------------------- info about "sortlist" --------------
 * Marc Majka		1994/04/16
 * Allan Nathanson	1994/10/29 (BIND 4.9.3.x)
 *
 * NetInfo resolver configuration directory support.
 *
 * Allow a NetInfo directory to be created in the hierarchy which
 * contains the same information as the resolver configuration file.
 *
 * - The local domain name is stored as the value of the "domain" property.
 * - The Internet address(es) of the name server(s) are stored as values
 *   of the "nameserver" property.
 * - The name server addresses are stored as values of the "nameserver"
 *   property.
 * - The search list for host-name lookup is stored as values of the
 *   "search" property.
 * - The sortlist comprised of IP address netmask pairs are stored as
 *   values of the "sortlist" property. The IP address and optional netmask
 *   should be separated by a slash (/) or ampersand (&) character.
 * - Internal resolver variables can be set from the value of the "options"
 *   property.
 */

static void res_setoptions(char *, char *);

#ifdef RESOLVSORT
static const char sort_mask[] = "/&";
#define ISSORTMASK(ch) (strchr(sort_mask, ch) != NULL)
static u_int32_t net_mask(struct in_addr);
#endif

/*
 * Resolver state default settings.
 */
volatile struct _thread_private_key_struct __THREAD_KEY_NAME(_res) = {
	PTHREAD_ONCE_INIT, 0
};

struct __res_state _res
# if defined(__BIND_RES_TEXT)
	= { RES_TIMEOUT, }	/* Motorola, et al. */
# endif
	;
#ifdef INET6
volatile struct _thread_private_key_struct __THREAD_KEY_NAME(_res_ext) = {
	PTHREAD_ONCE_INIT, 0
};

struct __res_state_ext _res_ext;
#endif /* INET6 */

/*
 * Set up default settings.  If the configuration file exist, the values
 * there will have precedence.  Otherwise, the server address is set to
 * INADDR_ANY and the default domain name comes from the gethostname().
 *
 * An interrim version of this code (BIND 4.9, pre-4.4BSD) used 127.0.0.1
 * rather than INADDR_ANY ("0.0.0.0") as the default name server address
 * since it was noted that INADDR_ANY actually meant ``the first interface
 * you "ifconfig"'d at boot time'' and if this was a SLIP or PPP interface,
 * it had to be "up" in order for you to reach your own name server.  It
 * was later decided that since the recommended practice is to always 
 * install local static routes through 127.0.0.1 for all your network
 * interfaces, that we could solve this problem without a code change.
 *
 * The configuration file should always be used, since it is the only way
 * to specify a default domain.  If you are running a server on your local
 * machine, you should say "nameserver 0.0.0.0" or "nameserver 127.0.0.1"
 * in the configuration file.
 *
 * Return 0 if completes successfully, -1 on error
 */
int
res_init()
{
	struct __res_state *_resp = _THREAD_PRIVATE(_res, _res, &_res);
#ifdef INET6
	struct __res_state_ext *_res_extp = _THREAD_PRIVATE(_res_ext, _res_ext,
							    &_res_ext);
#endif
	register FILE *fp;
	register char *cp, **pp;
	register int n;
	char buf[BUFSIZ];
	int nserv = 0;    /* number of nameserver records read from file */
	int haveenv = 0;
	int havesearch = 0;
	size_t len;
#ifdef RESOLVSORT
	int nsort = 0;
	char *net;
#endif
#ifndef RFC1535
	int dots;
#endif

	/*
	 * These three fields used to be statically initialized.  This made
	 * it hard to use this code in a shared library.  It is necessary,
	 * now that we're doing dynamic initialization here, that we preserve
	 * the old semantics: if an application modifies one of these three
	 * fields of _res before res_init() is called, res_init() will not
	 * alter them.  Of course, if an application is setting them to
	 * _zero_ before calling res_init(), hoping to override what used
	 * to be the static default, we can't detect it and unexpected results
	 * will follow.  Zero for any of these fields would make no sense,
	 * so one can safely assume that the applications were already getting
	 * unexpected results.
	 *
	 * _res.options is tricky since some apps were known to diddle the bits
	 * before res_init() was first called. We can't replicate that semantic
	 * with dynamic initialization (they may have turned bits off that are
	 * set in RES_DEFAULT).  Our solution is to declare such applications
	 * "broken".  They could fool us by setting RES_INIT but none do (yet).
	 */
	if (!_resp->retrans)
		_resp->retrans = RES_TIMEOUT;
	if (!_resp->retry)
		_resp->retry = 4;
	if (!(_resp->options & RES_INIT))
		_resp->options = RES_DEFAULT;

#ifdef USELOOPBACK
	_resp->nsaddr.sin_addr = inet_makeaddr(IN_LOOPBACKNET, 1);
#else
	_resp->nsaddr.sin_addr.s_addr = INADDR_ANY;
#endif
	_resp->nsaddr.sin_family = AF_INET;
	_resp->nsaddr.sin_port = htons(NAMESERVER_PORT);
	_resp->nsaddr.sin_len = sizeof(struct sockaddr_in);
#ifdef INET6
	if (sizeof(_res_extp->nsaddr) >= _resp->nsaddr.sin_len)
		memcpy(&_res_extp->nsaddr, &_resp->nsaddr, _resp->nsaddr.sin_len);
#endif
	_resp->nscount = 1;
	_resp->ndots = 1;
	_resp->pfcode = 0;
	strlcpy(_resp->lookups, "f", sizeof _resp->lookups);

	/* Allow user to override the local domain definition */
	if (issetugid() == 0 && (cp = getenv("LOCALDOMAIN")) != NULL) {
		strlcpy(_resp->defdname, cp, sizeof(_resp->defdname));
		haveenv++;

		/*
		 * Set search list to be blank-separated strings
		 * from rest of env value.  Permits users of LOCALDOMAIN
		 * to still have a search list, and anyone to set the
		 * one that they want to use as an individual (even more
		 * important now that the rfc1535 stuff restricts searches)
		 */
		cp = _resp->defdname;
		pp = _resp->dnsrch;
		*pp++ = cp;
		for (n = 0; *cp && pp < _resp->dnsrch + MAXDNSRCH; cp++) {
			if (*cp == '\n')	/* silly backwards compat */
				break;
			else if (*cp == ' ' || *cp == '\t') {
				*cp = 0;
				n = 1;
			} else if (n) {
				*pp++ = cp;
				n = 0;
				havesearch = 1;
			}
		}
		/* null terminate last domain if there are excess */
		while (*cp != '\0' && *cp != ' ' && *cp != '\t' && *cp != '\n')
			cp++;
		*cp = '\0';
		*pp++ = 0;
	}

#define	MATCH(line, name) \
	(!strncmp(line, name, sizeof(name) - 1) && \
	(line[sizeof(name) - 1] == ' ' || \
	 line[sizeof(name) - 1] == '\t'))

	if ((fp = fopen(_PATH_RESCONF, "r")) != NULL) {
	    strlcpy(_resp->lookups, "bf", sizeof _resp->lookups);

	    /* read the config file */
	    buf[0] = '\0';
	    while ((cp = fgetln(fp, &len)) != NULL) {
		/* skip lines that are too long or zero length */
		if (len >= sizeof(buf) || len == 0)
		    continue;
		(void)memcpy(buf, cp, len);
		buf[len] = '\0';
		/* skip comments */
		if ((cp = strpbrk(buf, ";#")) != NULL)
			*cp = '\0';
		if (buf[0] == '\0')
			continue;
		/* read default domain name */
		if (MATCH(buf, "domain")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("domain") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    strlcpy(_resp->defdname, cp, sizeof(_resp->defdname));
		    if ((cp = strpbrk(_resp->defdname, " \t\n")) != NULL)
			    *cp = '\0';
		    havesearch = 0;
		    continue;
		}
		/* lookup types */
		if (MATCH(buf, "lookup")) {
		    char *sp = NULL;

		    bzero(_resp->lookups, sizeof _resp->lookups);
		    cp = buf + sizeof("lookup") - 1;
		    for (n = 0;; cp++) {
		    	    if (n == MAXDNSLUS)
				    break;
			    if ((*cp == '\0') || (*cp == '\n')) {
				    if (sp) {
					    if (*sp=='y' || *sp=='b' || *sp=='f')
						    _resp->lookups[n++] = *sp;
					    sp = NULL;
				    }
				    break;
			    } else if ((*cp == ' ') || (*cp == '\t') || (*cp == ',')) {
				    if (sp) {
					    if (*sp=='y' || *sp=='b' || *sp=='f')
						    _resp->lookups[n++] = *sp;
					    sp = NULL;
				    }
			    } else if (sp == NULL)
				    sp = cp;
		    }
		    continue;
		}
		/* set search list */
		if (MATCH(buf, "search")) {
		    if (haveenv)	/* skip if have from environ */
			    continue;
		    cp = buf + sizeof("search") - 1;
		    while (*cp == ' ' || *cp == '\t')
			    cp++;
		    if ((*cp == '\0') || (*cp == '\n'))
			    continue;
		    strlcpy(_resp->defdname, cp, sizeof(_resp->defdname));
		    if ((cp = strchr(_resp->defdname, '\n')) != NULL)
			    *cp = '\0';
		    /*
		     * Set search list to be blank-separated strings
		     * on rest of line.
		     */
		    cp = _resp->defdname;
		    pp = _resp->dnsrch;
		    *pp++ = cp;
		    for (n = 0; *cp && pp < _resp->dnsrch + MAXDNSRCH; cp++) {
			    if (*cp == ' ' || *cp == '\t') {
				    *cp = 0;
				    n = 1;
			    } else if (n) {
				    *pp++ = cp;
				    n = 0;
			    }
		    }
		    /* null terminate last domain if there are excess */
		    while (*cp != '\0' && *cp != ' ' && *cp != '\t')
			    cp++;
		    *cp = '\0';
		    *pp++ = 0;
		    havesearch = 1;
		    continue;
		}
		/* read nameservers to query */
		if (MATCH(buf, "nameserver") && nserv < MAXNS) {
#ifdef INET6
		    char *q;
		    struct addrinfo hints, *res;
		    char pbuf[NI_MAXSERV];
#else
		    struct in_addr a;
#endif /* INET6 */

		    cp = buf + sizeof("nameserver") - 1;
		    while (*cp == ' ' || *cp == '\t')
			cp++;
#ifdef INET6
		    if ((*cp == '\0') || (*cp == '\n'))
			continue;
		    for (q = cp; *q; q++) {
			if (isspace(*q)) {
			    *q = '\0';
			    break;
			}
		    }
		    memset(&hints, 0, sizeof(hints));
		    hints.ai_flags = AI_NUMERICHOST;
		    hints.ai_socktype = SOCK_DGRAM;
		    snprintf(pbuf, sizeof(pbuf), "%u", NAMESERVER_PORT);
		    res = NULL;
		    if (getaddrinfo(cp, pbuf, &hints, &res) == 0 &&
			    res->ai_next == NULL) {
			if (res->ai_addrlen <= sizeof(_res_extp->nsaddr_list[nserv])) {
			    memcpy(&_res_extp->nsaddr_list[nserv], res->ai_addr,
				res->ai_addrlen);
			} else {
			    memset(&_res_extp->nsaddr_list[nserv], 0,
				sizeof(_res_extp->nsaddr_list[nserv]));
			}
			if (res->ai_addrlen <= sizeof(_resp->nsaddr_list[nserv])) {
			    memcpy(&_resp->nsaddr_list[nserv], res->ai_addr,
				res->ai_addrlen);
			} else {
			    memset(&_resp->nsaddr_list[nserv], 0,
				sizeof(_resp->nsaddr_list[nserv]));
			}
			nserv++;
		    }
		    if (res)
			freeaddrinfo(res);
#else /* INET6 */
		    if ((*cp != '\0') && (*cp != '\n') && inet_aton(cp, &a)) {
			_resp->nsaddr_list[nserv].sin_addr = a;
			_resp->nsaddr_list[nserv].sin_family = AF_INET;
			_resp->nsaddr_list[nserv].sin_port =
				htons(NAMESERVER_PORT);
			_resp->nsaddr_list[nserv].sin_len =
				sizeof(struct sockaddr_in);
			nserv++;
		    }
#endif /* INET6 */
		    continue;
		}
#ifdef RESOLVSORT
		if (MATCH(buf, "sortlist")) {
		    struct in_addr a;
#ifdef INET6
		    struct in6_addr a6;
		    int m, i;
		    u_char *u;
#endif /* INET6 */

		    cp = buf + sizeof("sortlist") - 1;
		    while (nsort < MAXRESOLVSORT) {
			while (*cp == ' ' || *cp == '\t')
			    cp++;
			if (*cp == '\0' || *cp == '\n' || *cp == ';')
			    break;
			net = cp;
			while (*cp && !ISSORTMASK(*cp) && *cp != ';' &&
			       isascii(*cp) && !isspace(*cp))
				cp++;
			n = *cp;
			*cp = 0;
			if (inet_aton(net, &a)) {
			    _resp->sort_list[nsort].addr = a;
			    if (ISSORTMASK(n)) {
				*cp++ = n;
				net = cp;
				while (*cp && *cp != ';' &&
					isascii(*cp) && !isspace(*cp))
				    cp++;
				n = *cp;
				*cp = 0;
				if (inet_aton(net, &a)) {
				    _resp->sort_list[nsort].mask = a.s_addr;
				} else {
				    _resp->sort_list[nsort].mask = 
					net_mask(_resp->sort_list[nsort].addr);
				}
			    } else {
				_resp->sort_list[nsort].mask = 
				    net_mask(_resp->sort_list[nsort].addr);
			    }
#ifdef INET6
			    _res_extp->sort_list[nsort].af = AF_INET;
			    _res_extp->sort_list[nsort].addr.ina =
				_resp->sort_list[nsort].addr;
			    _res_extp->sort_list[nsort].mask.ina.s_addr =
				_resp->sort_list[nsort].mask;
#endif /* INET6 */
			    nsort++;
			}
#ifdef INET6
			else if (inet_pton(AF_INET6, net, &a6) == 1) {
			    _res_extp->sort_list[nsort].af = AF_INET6;
			    _res_extp->sort_list[nsort].addr.in6a = a6;
			    u = (u_char *)&_res_extp->sort_list[nsort].mask.in6a;
			    *cp++ = n;
			    net = cp;
			    while (*cp && *cp != ';' &&
				    isascii(*cp) && !isspace(*cp))
				cp++;
			    m = n;
			    n = *cp;
			    *cp = 0;
			    switch (m) {
			    case '/':
				m = atoi(net);
				break;
			    case '&':
				if (inet_pton(AF_INET6, net, u) == 1) {
				    m = -1;
				    break;
				}
				/*FALLTHRU*/
			    default:
				m = sizeof(struct in6_addr) * NBBY;
				break;
			    }
			    if (m >= 0) {
				for (i = 0; i < sizeof(struct in6_addr); i++) {
				    if (m <= 0) {
					*u = 0;
				    } else {
					m -= NBBY;
					*u = (u_char)~0;
					if (m < 0)
					    *u <<= -m;
				    }
				    u++;
				}
			    }
			    nsort++;
			}
#endif /* INET6 */
			*cp = n;
		    }
		    continue;
		}
#endif
		if (MATCH(buf, "options")) {
		    res_setoptions(buf + sizeof("options") - 1, "conf");
		    continue;
		}
	    }
	    if (nserv > 1) 
		_resp->nscount = nserv;
#ifdef RESOLVSORT
	    _resp->nsort = nsort;
#endif
	    (void) fclose(fp);
	}
	if (_resp->defdname[0] == 0 &&
	    gethostname(buf, sizeof(_resp->defdname) - 1) == 0 &&
	    (cp = strchr(buf, '.')) != NULL)
	{
		strlcpy(_resp->defdname, cp + 1,
		        sizeof(_resp->defdname));
	}

	/* find components of local domain that might be searched */
	if (havesearch == 0) {
		pp = _resp->dnsrch;
		*pp++ = _resp->defdname;
		*pp = NULL;

#ifndef RFC1535
		dots = 0;
		for (cp = _resp->defdname; *cp; cp++)
			dots += (*cp == '.');

		cp = _resp->defdname;
		while (pp < _resp->dnsrch + MAXDFLSRCH) {
			if (dots < LOCALDOMAINPARTS)
				break;
			cp = strchr(cp, '.') + 1;    /* we know there is one */
			*pp++ = cp;
			dots--;
		}
		*pp = NULL;
#ifdef DEBUG
		if (_resp->options & RES_DEBUG) {
			printf(";; res_init()... default dnsrch list:\n");
			for (pp = _resp->dnsrch; *pp; pp++)
				printf(";;\t%s\n", *pp);
			printf(";;\t..END..\n");
		}
#endif /* DEBUG */
#endif /* !RFC1535 */
	}

	if (issetugid())
		_resp->options |= RES_NOALIASES;
	else if ((cp = getenv("RES_OPTIONS")) != NULL)
		res_setoptions(cp, "env");
	_resp->options |= RES_INIT;
	return (0);
}

/* ARGSUSED */
static void
res_setoptions(options, source)
	char *options, *source;
{
	struct __res_state *_resp = _THREAD_PRIVATE(_res, _res, &_res);
	char *cp = options;
	char *endp;
	long l;

#ifdef DEBUG
	if (_resp->options & RES_DEBUG)
		printf(";; res_setoptions(\"%s\", \"%s\")...\n",
		       options, source);
#endif
	while (*cp) {
		/* skip leading and inner runs of spaces */
		while (*cp == ' ' || *cp == '\t')
			cp++;
		/* search for and process individual options */
		if (!strncmp(cp, "ndots:", sizeof("ndots:") - 1)) {
			char *p = cp + sizeof("ndots:") - 1;
			l = strtol(p, &endp, 10);
			if (l >= 0 && endp != p &&
			    (*endp = '\0' || isspace(*endp))) {
				if (l <= RES_MAXNDOTS)
					_resp->ndots = l;
				else
					_resp->ndots = RES_MAXNDOTS;
#ifdef DEBUG
				if (_resp->options & RES_DEBUG)
					printf(";;\tndots=%u\n", _resp->ndots);
#endif
			}
		} else if (!strncmp(cp, "debug", sizeof("debug") - 1)) {
#ifdef DEBUG
			if (!(_resp->options & RES_DEBUG)) {
				printf(";; res_setoptions(\"%s\", \"%s\")..\n",
				       options, source);
				_resp->options |= RES_DEBUG;
			}
			printf(";;\tdebug\n");
#endif
		} else if (!strncmp(cp, "inet6", sizeof("inet6") - 1)) {
			_resp->options |= RES_USE_INET6;
		} else if (!strncmp(cp, "insecure1", sizeof("insecure1") - 1)) {
			_resp->options |= RES_INSECURE1;
		} else if (!strncmp(cp, "insecure2", sizeof("insecure2") - 1)) {
			_resp->options |= RES_INSECURE2;
		} else if (!strncmp(cp, "edns0", sizeof("edns0") - 1)) {
			_resp->options |= RES_USE_EDNS0;
		} else {
			/* XXX - print a warning here? */
		}
		/* skip to next run of spaces */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
	}
}

#ifdef RESOLVSORT
/* XXX - should really support CIDR which means explicit masks always. */
static u_int32_t
net_mask(in)		/* XXX - should really use system's version of this */
	struct in_addr in;
{
	register u_int32_t i = ntohl(in.s_addr);

	if (IN_CLASSA(i))
		return (htonl(IN_CLASSA_NET));
	else if (IN_CLASSB(i))
		return (htonl(IN_CLASSB_NET));
	return (htonl(IN_CLASSC_NET));
}
#endif
