/*	$NetBSD: fetch.c,v 1.2 1997/02/01 10:45:00 lukem Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason Thorpe and Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$NetBSD: fetch.c,v 1.2 1997/02/01 10:45:00 lukem Exp $";
#endif /* not lint */

/*
 * FTP User Program -- Command line file retrieval
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp_var.h"

#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */
#define HTTP_PROXY	"http_proxy"	/* env var with http proxy location */


#define EMPTYSTRING(x)	((x) == NULL || (*(x) == '\0'))

jmp_buf	httpabort;

/*
 * Retrieve an http:// URL, via a proxy if necessary.
 * Modifies the string argument given.
 * Returns -1 on failure, 0 on success
 */
int
http_get(line)
	char *line;
{
	struct sockaddr_in sin;
	int i, out, port, s;
	size_t buflen, len;
	char c, *cp, *cp2, *savefile, *portnum, *path, buf[4096];
	char *proxyenv, *proxy, *host;
	sig_t oldintr;
	off_t hashbytes;

	s = -1;
	proxy = NULL;

	host = line + sizeof(HTTP_URL) - 1;
	path = strchr(host, '/');		/* find path */
	if (EMPTYSTRING(path))
		goto cleanup_http_get;
	*path++ = '\0';
	if (EMPTYSTRING(path))
		goto cleanup_http_get;

	savefile = strrchr(path, '/');			/* find savefile */
	if (savefile != NULL)
		savefile++;
	else
		savefile = path;
	if (EMPTYSTRING(savefile))
		goto cleanup_http_get;

	proxyenv = getenv(HTTP_PROXY);
	if (proxyenv != NULL) {				/* use proxy */
		if (strncmp(proxyenv, HTTP_URL, sizeof(HTTP_URL) - 1) != 0) {
			warnx("Malformed proxy url: %s", proxyenv);
			goto cleanup_http_get;
		}
		proxy = strdup(proxyenv);
		if (proxy == NULL)
			errx(1, "Can't allocate memory for proxy url.");
		host = proxy + sizeof(HTTP_URL) - 1;
		if (EMPTYSTRING(host))
			goto cleanup_http_get;
		*--path = '/';			/* add / back to real path */
		path = strchr(host, '/');	/* remove trailing / on host */
		if (! EMPTYSTRING(path))
			*path++ = '\0';
		path = line;
	}

	portnum = strchr(host, ':');			/* find portnum */
	if (portnum != NULL)
		*portnum++ = '\0';

	if (debug)
		printf("host %s, port %s, path %s, save as %s.\n",
		    host, portnum, path, savefile);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	if (isdigit(host[0])) {
		if (inet_aton(host, &sin.sin_addr) == 0) {
			warnx("invalid IP address: %s", host);
			goto cleanup_http_get;
		}
	} else {
		struct hostent *hp;

		hp = gethostbyname(host);
		if (hp == NULL) {
			warnx("%s: %s", host, hstrerror(h_errno));
			goto cleanup_http_get;
		}
		if (hp->h_addrtype != AF_INET) {
			warnx("%s: not an Internet address?", host);
			goto cleanup_http_get;
		}
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}

	if (! EMPTYSTRING(portnum)) {
		port = atoi(portnum);
		if (port < 1 || (port & 0xffff) != port) {
			warnx("invalid port: %s", portnum);
			goto cleanup_http_get;
		}
		port = htons(port);
	} else
		port = httpport;
	sin.sin_port = port;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1) {
		warnx("Can't create socket");
		goto cleanup_http_get;
	}

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
		warn("Can't connect to %s", host);
		goto cleanup_http_get;
	}

	/*
	 * Construct and send the request.  We're expecting a return
	 * status of "200". Proxy requests don't want leading /.
	 */
	if (!proxy)
		printf("Requesting %s:%d/%s\n", line, ntohs(port), path);
	else
		printf("Requesting %s (via %s)\n", line, proxyenv);
	snprintf(buf, sizeof(buf), "GET %s%s HTTP/1.0\n\n",
	    proxy ? "" : "/", path);
	buflen = strlen(buf);
	if (write(s, buf, buflen) < buflen) {
		warn("write");
		goto cleanup_http_get;
	}
	memset(buf, 0, sizeof(buf));
	for (i = 0, buflen = sizeof(buf), cp = buf; i < buflen; cp++, i++) {
		if (read(s, cp, 1) != 1)
			goto improper;
		if (*cp == '\n')
			break;
	}
	buf[buflen - 1] = '\0';		/* sanity */
	cp = strchr(buf, ' ');
	if (cp == NULL)
		goto improper;
	else
		cp++;
	if (strncmp(cp, "200", 3)) {
		warnx("Error retrieving file: %s", cp);
		goto cleanup_http_get;
	}

	/*
	 * Read the rest of the header.
	 */
	memset(buf, 0, sizeof(buf));
	c = '\0';
	for (i = 0, buflen = sizeof(buf), cp = buf; i < buflen; cp++, i++) {
		if (read(s, cp, 1) != 1)
			goto improper;
		if (*cp == '\n' && c == '\n')
			break;
		c = *cp;
	}
	buf[buflen - 1] = '\0';		/* sanity */

	/*
	 * Look for the "Content-length: " header.
	 */
#define CONTENTLEN "Content-Length: "
	for (cp = buf; *cp != '\0'; cp++) {
		if (tolower(*cp) == 'c' &&
		    strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0)
			break;
	}
	if (*cp == '\0')
		goto improper;
	cp += sizeof(CONTENTLEN) - 1;
	cp2 = strchr(cp, '\n');
	if (cp2 == NULL)
		goto improper;
	else
		*cp2 = '\0';
	filesize = atoi(cp);
	if (filesize < 1)
		goto improper;

	/* Open the output file. */
	out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (out < 0) {
		warn("Can't open %s", savefile);
		goto cleanup_http_get;
	}

	/* Trap signals */
	oldintr = NULL;
	if (setjmp(httpabort)) {
		if (oldintr)
			(void) signal(SIGINT, oldintr);
		goto cleanup_http_get;
	}
	oldintr = signal(SIGINT, aborthttp);

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1);

	/* Finally, suck down the file. */
	i = 0;
	while ((len = read(s, buf, sizeof(buf))) > 0) {
		bytes += len;
		for (cp = buf; len > 0; len -= i, cp += i) {
			if ((i = write(out, cp, len)) == -1) {
				warn("Writing %s", savefile);
				goto cleanup_http_get;
			}
			else if (i == 0)
				break;
		}
		if (hash && !progress) {
			while (bytes >= hashbytes) {
				(void) putchar('#');
				hashbytes += mark;
			}
			(void) fflush(stdout);
		}
	}
	if (hash && !progress && bytes > 0) {
		if (bytes < mark)
			(void) putchar('#');
		(void) putchar('\n');
		(void) fflush(stdout);
	}
	if (len != 0) {
		warn("Reading from socket");
		goto cleanup_http_get;
	}
	progressmeter(1);
	if (verbose)
		printf("Successfully retrieved file.\n");
	(void) signal(SIGINT, oldintr);

	close(s);
	close(out);
	if (proxy)
		free(proxy);
	return(0);

improper:
	warnx("improper response from %s", host);
cleanup_http_get:
	if (s != -1)
		close(s);
	if (proxy)
		free(proxy);
	return(-1);
}

/*
 * Abort a http retrieval
 */
void
aborthttp()
{

	alarmtimer(0);
	printf("\nhttp fetch aborted\n");
	(void) fflush(stdout);
	longjmp(httpabort, 1);
}

/*
 * Retrieve multiple files from the command line, transferring
 * files of the form "host:path", "ftp://host/path" using the
 * ftp protocol, and files of the form "http://host/path" using
 * the http protocol.
 * If path has a trailing "/", then return(-1);
 * the path will be cd-ed into and the connection remains open,
 * and the function will return -1 (to indicate the connection
 * is alive).
 * If an error occurs the return value will be the offset+1 in
 * argv[] of the file that caused a problem (i.e, argv[x]
 * returns x+1)
 * Otherwise, 0 is returned if all files retrieved successfully.
 */
int
auto_fetch(argc, argv)
	int argc;
	char *argv[];
{
	static char lasthost[MAXHOSTNAMELEN];
	char *xargv[5];
	char *cp, *line, *host, *dir, *file, *portnum;
	int rval, xargc, argpos;

	argpos = 0;

	if (setjmp(toplevel)) {
		if (connected)
			disconnect(0, NULL);
		return(argpos + 1);
	}
	(void) signal(SIGINT, intr);
	(void) signal(SIGPIPE, lostpeer);

	/*
	 * Loop through as long as there's files to fetch.
	 */
	for (rval = 0; (rval == 0) && (argpos < argc); free(line), argpos++) {
		if (strchr(argv[argpos], ':') == NULL)
			break;
		host = dir = file = portnum = NULL;

		/*
		 * We muck with the string, so we make a copy.
		 */
		line = strdup(argv[argpos]);
		if (line == NULL)
			errx(1, "Can't allocate memory for auto-fetch.");

		/*
		 * Try HTTP URL-style arguments first.
		 */
		if (strncmp(line, HTTP_URL, sizeof(HTTP_URL) - 1) == 0) {
			if (http_get(line) == -1)
				rval = argpos + 1;
			continue;
		}

		/*
		 * Try FTP URL-style arguments next, then host:file.
		 */
		host = line;
		if (strncmp(line, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
			host += sizeof(FTP_URL) - 1;
			cp = strchr(host, '/');

			/* Look for a port number after the host name. */
			portnum = strchr(host, ':');
			if (portnum != NULL)
				*portnum++ = '\0';
		} else				/* classic style `host:file' */
			cp = strchr(host, ':');
		if (EMPTYSTRING(host)) {
			rval = argpos + 1;
			continue;
		}

		/*
		 * If cp is NULL, the file wasn't specified
		 * (URL looked something like ftp://host)
		 */
		if (cp != NULL)
			*cp++ = '\0';

		/*
		 * Extract the file and (if present) directory name.
		 */
		dir = cp;
		if (! EMPTYSTRING(dir)) {
			cp = strrchr(cp, '/');
			if (cp != NULL) {
				*cp++ = '\0';
				file = cp;
			} else {
				file = dir;
				dir = NULL;
			}
		}
		if (debug)
			printf("host '%s', dir '%s', file '%s'\n",
			    host, dir, file);

		/*
		 * Set up the connection if we don't have one.
		 */
		if (strcmp(host, lasthost) != 0) {
			strcpy(lasthost, host);
			if (connected)
				disconnect(0, NULL);
			xargv[0] = __progname;
			xargv[1] = host;
			xargv[2] = NULL;
			xargc = 2;
			if (portnum != NULL) {
				xargv[2] = portnum;
				xargv[3] = NULL;
				xargc = 3;
			}
			setpeer(xargc, xargv);
			if (connected == 0) {
				warnx("Can't connect to host `%s'", host);
				rval = argpos + 1;
				continue;
			}

			/* Always use binary transfers. */
			setbinary(0, NULL);
		}
		else	/* already have connection, cd back to '/' */
		{
			xargv[0] = "cd";
			xargv[1] = "/";
			xargv[2] = NULL;
			cd(2, xargv);
			if (! dirchange) {
				rval = argpos + 1;
				continue;
			}
		}

		/* Change directories, if necessary. */
		if (! EMPTYSTRING(dir)) {
			xargv[0] = "cd";
			xargv[1] = dir;
			xargv[2] = NULL;
			cd(2, xargv);
			if (! dirchange) {
				rval = argpos + 1;
				continue;
			}
		}

		if (EMPTYSTRING(file)) {
			rval = -1;
			continue;
		}

		if (!verbose)
			printf("Retrieving %s/%s\n", dir ? dir : "", file);

		/* Fetch the file. */
		xargv[0] = "get";
		xargv[1] = file;
		xargv[2] = NULL;
		get(2, xargv);

		if ((code / 100) != COMPLETE)	/* XXX: is this valid? */
			rval = argpos + 1;
	}
	if (connected && rval != -1)
		disconnect(0, NULL);
	return (rval);
}
