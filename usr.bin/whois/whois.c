/*	$OpenBSD: whois.c,v 1.17 2002/12/17 20:17:49 millert Exp $	*/

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
static const char rcsid[] = "$OpenBSD: whois.c,v 1.17 2002/12/17 20:17:49 millert Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define	NICHOST		"whois.crsnic.net"
#define	INICHOST	"whois.internic.net"
#define	DNICHOST	"whois.nic.mil"
#define	GNICHOST	"whois.nic.gov"
#define	ANICHOST	"whois.arin.net"
#define	RNICHOST	"whois.ripe.net"
#define	PNICHOST	"whois.apnic.net"
#define	RUNICHOST	"whois.ripn.net"
#define	MNICHOST	"whois.ra.net"
#define LNICHOST	"whois.lacnic.net"
#define	QNICHOST_TAIL	".whois-servers.net"
#define	WHOIS_PORT	43

#define WHOIS_RECURSE		0x01
#define WHOIS_INIC_FALLBACK	0x02
#define WHOIS_QUICK		0x04

static __dead void usage(void);
static int whois(char *, struct addrinfo *, int);

int
main(int argc, char **argv)
{
	int ch, i, j, error, rval;
	int use_qnichost, flags;
	size_t len;
	char *host, *qnichost, *name;
	struct addrinfo hints, *res;

#ifdef SOCKS
	SOCKSinit(argv[0]);
#endif

	host = NULL;
	qnichost = NULL;
	flags = 0;
	use_qnichost = 0;
	rval = 0;
	while ((ch = getopt(argc, argv, "adgh:ilmpqQrR")) != -1)
		switch((char)ch) {
		case 'a':
			host = ANICHOST;
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
			host = PNICHOST;
			break;
		case 'q':
			/* default */
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
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	/*
	 * If no nic host is specified, use whois-servers.net
	 * if there is a '.' in the name, else fall back to NICHOST.
	 */
	if (host == NULL) {
		use_qnichost = 1;
		host = NICHOST;
		if (!(flags & WHOIS_QUICK))
			flags |= WHOIS_INIC_FALLBACK | WHOIS_RECURSE;
	}
	for (name = *argv; (name = *argv) != NULL; argv++) {
		if (use_qnichost) {
			if (qnichost) {
				free(qnichost);
				qnichost = NULL;
			}
			for (i = j = 0; name[i]; i++)
				if (name[i] == '.')
					j = i;
			if (j != 0) {
				len = i - j + 1 + strlen(QNICHOST_TAIL);
				qnichost = (char *)calloc(len, sizeof(char));
				if (!qnichost)
					err(1, "calloc");
				strlcpy(qnichost, name + j + 1, len);
				strlcat(qnichost, QNICHOST_TAIL, len);
				memset(&hints, 0, sizeof(hints));
				hints.ai_flags = 0;
				hints.ai_family = AF_UNSPEC;
				hints.ai_socktype = SOCK_STREAM;
				error = getaddrinfo(qnichost, "whois",
				    &hints, &res);
				if (error) {
					warnx("%s: %s", qnichost,
					    gai_strerror(error));
					rval++;
					continue;
				}
			}
		}
		if (!qnichost) {
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = 0;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			error = getaddrinfo(host, "whois", &hints, &res);
			if (error) {
				warnx("%s: %s", host, gai_strerror(error));
				rval++;
				continue;
			}
		}

		whois(name, res, flags);
		freeaddrinfo(res);
	}
	exit(rval);
}

static int
whois(char *name, struct addrinfo *res, int flags)
{
	FILE *sfi, *sfo;
	char *buf, *p, *nhost, *nbuf = NULL;
	size_t len;
	int s, nomatch, rval;
	const char *reason = NULL;

	s = -1;
	rval = 0;
	for (/*nothing*/; res; res = res->ai_next) {
		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s < 0) {
			reason = "socket";
			continue;
		}
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
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
		return (rval + 1);
	}

	sfi = fdopen(s, "r");
	sfo = fdopen(s, "w");
	if (sfi == NULL || sfo == NULL)
		err(EX_OSERR, "fdopen");
	(void)fprintf(sfo, "%s\r\n", name);
	(void)fflush(sfo);
	nhost = NULL;
	nomatch = 0;
	while ((buf = fgetln(sfi, &len))) {
		if (buf[len - 2] == '\r')
			buf[len - 2] = '\0';
		else if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((nbuf = malloc(len + 1)) == NULL)
				err(1, "malloc");
			memcpy(nbuf, buf, len);
			nbuf[len] = '\0';
			buf = nbuf;
		}

		if ((flags & WHOIS_RECURSE) && !nhost &&
		    (p = strstr(buf, "Whois Server: "))) {
			p += sizeof("Whois Server: ") - 1;
			if ((len = strcspn(p, " \t\n\r"))) {
				if ((nhost = malloc(len + 1)) == NULL)
					err(1, "malloc");
				memcpy(nhost, p, len);
				nhost[len] = '\0';
			}
		}
		if ((flags & WHOIS_INIC_FALLBACK) && !nhost && !nomatch &&
		    (p = strstr(buf, "No match for \""))) {
			p += sizeof("No match for \"") - 1;
			if ((len = strcspn(p, "\"")) && len == strlen(name) &&
			    strncasecmp(name, p, len) == 0)
				nomatch = 1;
		}
		(void)puts(buf);
	}
	if (nbuf)
		free(nbuf);

	/* Do second lookup as needed */
	if (nomatch && !nhost) {
		(void)printf("Looking up %s at %s.\n\n", name, INICHOST);
		nhost = INICHOST;
	}
	if (nhost) {
		struct addrinfo hints, *res2;
		int error;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = 0;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		error = getaddrinfo(nhost, "whois", &hints, &res2);
		if (error) {
			warnx("%s: %s", nhost, gai_strerror(error));
			return (rval + 1);
		}
		if (!nomatch)
			free(nhost);
		rval += whois(name, res2, 0);
		freeaddrinfo(res2);
	}
	return (rval);
}

static __dead void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: whois [-adgilmpqQrR] [-h hostname] name ...\n");
	exit(EX_USAGE);
}
