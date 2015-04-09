/*      $OpenBSD: whois.c,v 1.47 2015/04/09 19:29:53 sthen Exp $   */

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
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
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
#define	AFNICHOST	"whois.afrinic.net"
#define BNICHOST	"whois.registro.br"
#define	PDBHOST		"whois.peeringdb.com"
#define	QNICHOST_TAIL	".whois-servers.net"

#define	WHOIS_PORT	"whois"
#define	WHOIS_SERVER_ID	"Whois Server:"

#define WHOIS_RECURSE		0x01
#define WHOIS_QUICK		0x02

const char *port_whois = WHOIS_PORT;
const char *ip_whois[] = { LNICHOST, RNICHOST, PNICHOST, BNICHOST,
    AFNICHOST, NULL };

__dead void usage(void);
int whois(const char *, const char *, const char *, int);
char *choose_server(const char *, const char *);

int
main(int argc, char *argv[])
{
	int ch, flags, rval;
	char *host, *name, *country;

	country = host = NULL;
	flags = rval = 0;
	while ((ch = getopt(argc, argv, "aAc:dgh:ilmp:PqQrR")) != -1)
		switch (ch) {
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
			port_whois = optarg;
			break;
		case 'P':
			host = PDBHOST;
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
		    port_whois, flags);
	exit(rval);
}

int
whois(const char *query, const char *server, const char *port, int flags)
{
	FILE *fp;
	char *buf, *p, *nhost, *nbuf = NULL;
	size_t len;
	int i, s, error;
	const char *reason = NULL, *fmt;
	struct addrinfo hints, *res, *ai;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(server, port, &hints, &res);
	if (error) {
		if (error == EAI_SERVICE)
			warnx("%s: bad port", port);
		else
			warnx("%s: %s", server, gai_strerror(error));
		return (1);
	}

	for (s = -1, ai = res; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1) {
			error = errno;
			reason = "socket";
			continue;
		}
		if (connect(s, ai->ai_addr, ai->ai_addrlen) == -1) {
			error = errno;
			reason = "connect";
			close(s);
			s = -1;
			continue;
		}
		break;	/*okay*/
	}
	if (s == -1) {
		if (reason) {
			errno = error;
			warn("%s: %s", server, reason);
		} else
			warn("unknown error in connection attempt");
		freeaddrinfo(res);
		return (1);
	}

	if (strcmp(server, "whois.denic.de") == 0 ||
	    strcmp(server, "de" QNICHOST_TAIL) == 0)
		fmt = "-T dn,ace -C ISO-8859-1 %s\r\n";
	else if (strcmp(server, "whois.dk-hostmaster.dk") == 0 ||
	    strcmp(server, "dk" QNICHOST_TAIL) == 0)
		fmt = "--show-handles %s\r\n";
	else
		fmt = "%s\r\n";

	fp = fdopen(s, "r+");
	if (fp == NULL)
		err(1, "fdopen");
	fprintf(fp, fmt, query);
	fflush(fp);
	nhost = NULL;
	while ((buf = fgetln(fp, &len)) != NULL) {
		p = buf + len - 1;
		if (isspace((unsigned char)*p)) {
			do
				*p = '\0';
			while (p > buf && isspace((unsigned char)*--p));
		} else {
			if ((nbuf = malloc(len + 1)) == NULL)
				err(1, "malloc");
			memcpy(nbuf, buf, len);
			nbuf[len] = '\0';
			buf = nbuf;
		}
		puts(buf);

		if (nhost != NULL || !(flags & WHOIS_RECURSE))
			continue;

		if ((p = strstr(buf, WHOIS_SERVER_ID))) {
			p += sizeof(WHOIS_SERVER_ID) - 1;
			while (isblank((unsigned char)*p))
				p++;
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
	fclose(fp);
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
 * (starts with '!') or a CORE handle (COCO-[0-9]+ or COHO-[0-9]+) or an
 * ASN (starts with AS). Fall back to NICHOST for the non-handle case.
 */
char *
choose_server(const char *name, const char *country)
{
	static char *server;
	const char *qhead;
	char *nserver;
	char *ep;
	size_t len;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (country != NULL)
		qhead = country;
	else if ((qhead = strrchr(name, '.')) == NULL) {
		if (*name == '!')
			return (INICHOST);
		else if ((strncasecmp(name, "COCO-", 5) == 0 ||
		    strncasecmp(name, "COHO-", 5) == 0) &&
		    strtol(name + 5, &ep, 10) > 0 && *ep == '\0')
			return (CNICHOST);
		else if ((strncasecmp(name, "AS", 2) == 0) &&
		    strtol(name + 2, &ep, 10) > 0 && *ep == '\0')
			return (MNICHOST);
		else
			return (NICHOST);
	} else if (isdigit(*(++qhead)))
		return (ANICHOST);
	len = strlen(qhead) + sizeof(QNICHOST_TAIL);
	if ((nserver = realloc(server, len)) == NULL)
		err(1, "realloc");
	server = nserver;

	/*
	 * Post-2003 ("new") gTLDs are all supposed to have "whois.nic.domain"
	 * (per registry agreement), some older gTLDs also support this...
	 */
	strlcpy(server, "whois.nic.", len);
	strlcat(server, qhead, len);

	/* most ccTLDs don't do this, but QNICHOST/whois-servers mostly works */
	if ((strlen(qhead) == 2 ||
	    /* and is required for most of the <=2003 TLDs/gTLDs */
	    strncasecmp(qhead, "org", 3) == 0 ||
	    strncasecmp(qhead, "com", 3) == 0 ||
	    strncasecmp(qhead, "net", 3) == 0 ||
	    strncasecmp(qhead, "cat", 3) == 0 ||
	    strncasecmp(qhead, "pro", 3) == 0 ||
	    strncasecmp(qhead, "info", 4) == 0 ||
	    strncasecmp(qhead, "aero", 4) == 0 ||
	    strncasecmp(qhead, "jobs", 4) == 0 ||
	    strncasecmp(qhead, "mobi", 4) == 0 ||
	    strncasecmp(qhead, "museum", 6) == 0 ||
	     /* for others, if whois.nic.TLD doesn't exist, try whois-servers */
	    getaddrinfo(server, NULL, &hints, &res) != 0)) {
		strlcpy(server, qhead, len);
		strlcat(server, QNICHOST_TAIL, len);
	}

	return (server);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-AadgilmPQRr] [-c country-code | -h host] "
		"[-p port] name ...\n", __progname);
	exit(1);
}
