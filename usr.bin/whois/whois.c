/*	$OpenBSD: whois.c,v 1.23 2003/01/15 23:16:29 millert Exp $	*/

/*
 * Copyright (c) 1980, 1993
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)whois.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: whois.c,v 1.23 2003/01/15 23:16:29 millert Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	NICHOST		"whois.crsnic.net"
#define	INICHOST	"whois.networksolutions.com"
#define	CNICHOST	"whois.corenic.net"
#define	DNICHOST	"whois.nic.mil"
#define	GNICHOST	"whois.nic.gov"
#define	ANICHOST	"whois.arin.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	RUNICHOST	"whois.ripn.net"
#define	MNICHOST	"whois.ra.net"
#define LNICHOST	"whois.lacnic.net"
#define SNICHOST	"whois.6bone.net"
#define BNICHOST	"whois.registro.br"
#define	QNICHOST_TAIL	".whois-servers.net"

#define	WHOIS_PORT	"whois"
#define	WHOIS_SERVER_ID	"Whois Server: "

#define WHOIS_RECURSE		0x01
#define WHOIS_QUICK		0x02

const char *port = WHOIS_PORT;
const char *ip_whois[] = { LNICHOST, RNICHOST, PNICHOST, BNICHOST, NULL };

static __dead void usage(void);
static int whois(const char *, const char *, const char *, int);
static char *choose_server(const char *, const char *);

int
main(int argc, char **argv)
{
	int ch, flags, rval;
	char *host, *name, *country, *server;

#ifdef SOCKS
	SOCKSinit(argv[0]);
#endif
	country = host = server = NULL;
	flags = rval = 0;
	while ((ch = getopt(argc, argv, "aAc:dgh:ilmp:qQrR6")) != -1)
		switch(ch) {
		case 'a':
			host = ANICHOST;
			break;
		case 'A':
			host = PNICHOST;
			break;
		case 'c':
			country = optarg;
			break;
		case 'd':
			host = DNICHOST;
			break;
		case 'g':
			host = GNICHOST;
			break;
		case 'h':
			host = optarg;
			break;
		case 'i':
			host = INICHOST;
			break;
		case 'l':
			host = LNICHOST;
			break;
		case 'm':
			host = MNICHOST;
			break;
		case 'p':
			port = optarg;
			break;
		case 'q':
			/* deprecated, now the default */
			break;
		case 'Q':
			flags |= WHOIS_QUICK;
			break;
		case 'r':
			host = RNICHOST;
			break;
		case 'R':
			host = RUNICHOST;
			break;
		case '6':
			host = SNICHOST;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc || (country != NULL && host != NULL))
		usage();

	if (host == NULL && country == NULL && !(flags & WHOIS_QUICK))
		flags |= WHOIS_RECURSE;
	for (name = *argv; (name = *argv) != NULL; argv++)
		rval += whois(name, host ? host : choose_server(name, country),
		    port, flags);
	exit(rval);
}

static int
whois(const char *query, const char *server, const char *port, int flags)
{
	FILE *sfi, *sfo;
	char *buf, *p, *nhost, *nbuf = NULL;
	size_t len;
	int i, s, error;
	const char *reason = NULL;
	struct addrinfo hints, *res, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(server, port, &hints, &res);
	if (error) {
		warnx("%s: %s", server, gai_strerror(error));
		return (1);
	}

	for (s = -1, ai = res; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) {
			reason = "socket";
			continue;
		}
		if (connect(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			reason = "connect";
			close(s);
			s = -1;
			continue;
		}
		break;	/*okay*/
	}
	if (s < 0) {
		if (reason)
			warn("%s", reason);
		else
			warn("unknown error in connection attempt");
		freeaddrinfo(res);
		return (1);
	}

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(1, "fdopen");
	(void)fprintf(sfo, "%s\r\n", query);
	(void)fflush(sfo);
	nhost = NULL;
	while ((buf = fgetln(sfi, &len)) != NULL) {
		p = buf + len - 1;
		if (isspace((unsigned char)*p)) {
			do
				*p-- = '\0';
			while (p != buf && isspace((unsigned char)*p));
		} else {
			if ((nbuf = malloc(len + 1)) == NULL)
				err(1, "malloc");
			memcpy(nbuf, buf, len);
			nbuf[len] = '\0';
			buf = nbuf;
		}
		(void)puts(buf);

		if (nhost != NULL || !(flags & WHOIS_RECURSE))
			continue;

		if ((p = strstr(buf, WHOIS_SERVER_ID))) {
			p += sizeof(WHOIS_SERVER_ID) - 1;
			if ((len = strcspn(p, " \t\n\r"))) {
				if ((nhost = malloc(len + 1)) == NULL)
					err(1, "malloc");
				memcpy(nhost, p, len);
				nhost[len] = '\0';
			}
		} else if (strcmp(server, ANICHOST) == 0) {
			for (p = buf; *p != '\0'; p++)
				*p = tolower((unsigned char)*p);
			for (i = 0; ip_whois[i] != NULL; i++) {
				if (strstr(buf, ip_whois[i]) != NULL) {
					nhost = strdup(ip_whois[i]);
					if (nhost == NULL)
						err(1, "strdup");
					break;
				}
			}
		}
	}
	if (nbuf != NULL)
		free(nbuf);

	if (nhost != NULL) {
		error = whois(query, nhost, port, 0);
		free(nhost);
	}
	freeaddrinfo(res);
	return (error);
}

/*
 * If no country is specified determine the top level domain from the query.
 * If the TLD is a number, query ARIN, otherwise, use TLD.whois-server.net.
 * If the domain does not contain '.', check to see if it is an NSI handle
 * (starts with '!') or a CORE handle (COCO-[0-9]+ or COHO-[0-9]+).
 * Fall back to NICHOST for the non-handle case.
 */
static char *
choose_server(const char *name, const char *country)
{
	static char *server;
	const char *qhead;
	char *ep;
	size_t len;

	if (country != NULL)
		qhead = country;
	else if ((qhead = strrchr(name, '.')) == NULL) {
		if (*name == '!')
			return (INICHOST);
		else if ((strncasecmp(name, "COCO-", 5) == 0 ||
		    strncasecmp(name, "COHO-", 5) == 0) &&
		    strtol(name + 5, &ep, 10) > 0 && *ep == '\0')
			return (CNICHOST);  
		else
			return (NICHOST);
	} else if (isdigit(*(++qhead)))
		return (ANICHOST);
	len = strlen(qhead) + sizeof(QNICHOST_TAIL);
	if ((server = realloc(server, len)) == NULL)
		err(1, "realloc");
	strlcpy(server, qhead, len);
	strlcat(server, QNICHOST_TAIL, len);
	return (server);
}

static __dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-aAdgilmQrR6] [-c country-code | -h hostname] "
		"[-p port] name ...\n", __progname);
	exit(1);
}
