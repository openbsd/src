/*	$OpenBSD: fetch.c,v 1.40 2002/11/08 03:30:17 fgsch Exp $	*/
/*	$NetBSD: fetch.c,v 1.14 1997/08/18 10:20:20 lukem Exp $	*/

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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: fetch.c,v 1.40 2002/11/08 03:30:17 fgsch Exp $";
#endif /* not lint */

/*
 * FTP User Program -- Command line file retrieval
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <arpa/ftp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "ftp_var.h"

static int	url_get(const char *, const char *, const char *);
void		aborthttp(int);
void		abortfile(int);
char    	hextochar(const char *);
char   		*urldecode(const char *);

#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */
#define	FILE_URL	"file:"		/* file URL prefix */
#define FTP_PROXY	"ftp_proxy"	/* env var with ftp proxy location */
#define HTTP_PROXY	"http_proxy"	/* env var with http proxy location */


#define EMPTYSTRING(x)	((x) == NULL || (*(x) == '\0'))

static const char *at_encoding_warning =
   "Extra `@' characters in usernames and passwords should be encoded as %%40";

jmp_buf	httpabort;

/*
 * Retrieve URL, via the proxy in $proxyvar if necessary.
 * Modifies the string argument given.
 * Returns -1 on failure, 0 on success
 */
static int
url_get(origline, proxyenv, outfile)
	const char *origline;
	const char *proxyenv;
	const char *outfile;
{
	struct addrinfo hints, *res0, *res;
	int error;
	int i, isftpurl, isfileurl, isredirect;
	volatile int s, out;
	size_t len;
	char *cp, *ep, *portnum, *path;
	char pbuf[NI_MAXSERV];
	const char * volatile savefile;
	char *line, *host, *port, *buf;
	char * volatile proxy;
	char *hosttail;
	volatile sig_t oldintr;
	off_t hashbytes;
	char *cause = "unknown";
	FILE *fin;
	int rval;

	s = -1;
	proxy = NULL;
	fin = NULL;
	buf = NULL;
	isftpurl = 0;
	isfileurl = 0;
	isredirect = 0;
	rval = -1;

	line = strdup(origline);
	if (line == NULL)
		errx(1, "Can't allocate memory to parse URL");
	if (strncasecmp(line, HTTP_URL, sizeof(HTTP_URL) - 1) == 0)
		host = line + sizeof(HTTP_URL) - 1;
	else if (strncasecmp(line, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
		host = line + sizeof(FTP_URL) - 1;
		isftpurl = 1;
	} else if (strncasecmp(line, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
		host = line + sizeof(FILE_URL) - 1;
		isfileurl = 1;
	} else
		errx(1, "url_get: Invalid URL '%s'", line);

	if (isfileurl) {
		path = host;
	} else {
		path = strchr(host, '/');		/* find path */
		if (EMPTYSTRING(path)) {
			if (isftpurl)
				goto noftpautologin;
			warnx("Invalid URL (no `/' after host): %s", origline);
			goto cleanup_url_get;
		}
		*path++ = '\0';
		if (EMPTYSTRING(path)) {
			if (isftpurl)
				goto noftpautologin;
			warnx("Invalid URL (no file after host): %s", origline);
			goto cleanup_url_get;
		}
	}

	if (outfile)
		savefile = outfile;
	else
		savefile = basename(path);

	if (EMPTYSTRING(savefile)) {
		if (isftpurl)
			goto noftpautologin;
		warnx("Invalid URL (no file after directory): %s", origline);
		goto cleanup_url_get;
	}

	if (proxyenv != NULL) {				/* use proxy */
		proxy = strdup(proxyenv);
		if (proxy == NULL)
			errx(1, "Can't allocate memory for proxy URL.");
		if (strncasecmp(proxy, HTTP_URL, sizeof(HTTP_URL) - 1) == 0)
			host = proxy + sizeof(HTTP_URL) - 1;
		else if (strncasecmp(proxy, FTP_URL, sizeof(FTP_URL) - 1) == 0)
			host = proxy + sizeof(FTP_URL) - 1;
		else {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		if (EMPTYSTRING(host)) {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		*--path = '/';			/* add / back to real path */
		path = strchr(host, '/');	/* remove trailing / on host */
		if (! EMPTYSTRING(path))
			*path++ = '\0';
		path = line;
	}

	if (isfileurl) {
		struct stat st;

		s = open(path, O_RDONLY);
		if (s == -1) {
			warn("Can't open file %s", path);
			goto cleanup_url_get;
		}

		if (fstat(s, &st) == -1)
			filesize = -1;
		else
			filesize = st.st_size;

		/* Open the output file.  */
		if (strcmp(savefile, "-") != 0) {
			out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
			if (out < 0) {
				warn("Can't open %s", savefile);
				goto cleanup_url_get;
			}
		} else
			out = fileno(stdout);

		/* Trap signals */
		oldintr = NULL;
		if (setjmp(httpabort)) {
			if (oldintr)
				(void)signal(SIGINT, oldintr);
			goto cleanup_url_get;
		}
		oldintr = signal(SIGINT, abortfile);
	
		bytes = 0;
		hashbytes = mark;
		progressmeter(-1);

		if ((buf = malloc(4096)) == NULL)
			errx(1, "Can't allocate memory for transfer buffer\n");
	
		/* Finally, suck down the file. */
		i = 0;
		while ((len = read(s, buf, 4096)) > 0) {
			bytes += len;
			for (cp = buf; len > 0; len -= i, cp += i) {
				if ((i = write(out, cp, len)) == -1) {
					warn("Writing %s", savefile);
					goto cleanup_url_get;
				}
				else if (i == 0)
					break;
			}
			if (hash && !progress) {
				while (bytes >= hashbytes) {
					(void)putc('#', ttyout);
					hashbytes += mark;
				}
				(void)fflush(ttyout);
			}
		}
		if (hash && !progress && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (len != 0) {
			warn("Reading from file");
			goto cleanup_url_get;
		}
		progressmeter(1);
		if (verbose)
			fputs("Successfully retrieved file.\n", ttyout);
		(void)signal(SIGINT, oldintr);
	
		rval = 0;
		goto cleanup_url_get;
	}

	if (*host == '[' && (hosttail = strrchr(host, ']')) != NULL &&
	    (hosttail[1] == '\0' || hosttail[1] == ':')) {
		host++;
		*hosttail++ = '\0';
	} else
		hosttail = host;

	portnum = strrchr(hosttail, ':');		/* find portnum */
	if (portnum != NULL)
		*portnum++ = '\0';

	if (debug)
		fprintf(ttyout, "host %s, port %s, path %s, save as %s.\n",
		    host, portnum, path, savefile);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	port = portnum ? portnum : httpport;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error == EAI_SERVICE && port == httpport) {
		/*
		 * If the services file is corrupt/missing, fall back
		 * on our hard-coded defines.
		 */
		snprintf(pbuf, sizeof(pbuf), "%d", HTTP_PORT);
		error = getaddrinfo(host, pbuf, &hints, &res0);
	}
	if (error) {
		warnx("%s: %s", gai_strerror(error), host);
		goto cleanup_url_get;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		getnameinfo(res->ai_addr, res->ai_addrlen, pbuf, sizeof(pbuf),
			NULL, 0, NI_NUMERICHOST);
		fprintf(ttyout, "Trying %s...\n", pbuf);

		s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (s == -1) {
			cause = "socket";
			continue;
		}

again:
		if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
			if (errno == EINTR)
				goto again;
			close(s);
			s = -1;
			cause = "connect";
			continue;
		}

		/* get port in numeric */
		if (getnameinfo(res->ai_addr, res->ai_addrlen, NULL, 0,
		    pbuf, sizeof(pbuf), NI_NUMERICSERV) == 0)
			port = pbuf;
		else
			port = NULL;

		break;
	}
	freeaddrinfo(res0);
	if (s < 0) {
		warn("%s", cause);
		goto cleanup_url_get;
	}

	fin = fdopen(s, "r+");

	/*
	 * Construct and send the request. Proxy requests don't want leading /.
	 */
	if (proxy) {
		/*
		 * Host: directive must use the destination host address for
		 * the original URI (path).  We do not attach it at this moment.
		 */
		if (verbose)
			fprintf(ttyout, "Requesting %s (via %s)\n",
			    origline, proxyenv);
		fprintf(fin, "GET %s HTTP/1.0\r\n%s\r\n\r\n", path, HTTP_USER_AGENT);
	} else {
		if (verbose)
			fprintf(ttyout, "Requesting %s\n", origline);
		if (strchr(host, ':')) {
			char *h, *p;

			/* strip off scoped address portion, since it's local to node */
			h = strdup(host);
			if (h == NULL)
				errx(1, "Can't allocate memory.");
			if ((p = strchr(h, '%')) != NULL)
				*p = '\0';
			/*
			 * Send port number only if it's specified and does not equal
			 * 80. Some broken HTTP servers get confused if you explicitly
			 * send them the port number.
			 */
			if (port && strcmp(port, "80") != 0)
				fprintf(fin, "GET /%s HTTP/1.0\r\nHost: [%s]:%s\r\n%s\r\n\r\n", 
				    path, h, port, HTTP_USER_AGENT);
			else
				fprintf(fin, "GET /%s HTTP/1.0\r\nHost: [%s]\r\n%s\r\n\r\n", 
				    path, h, HTTP_USER_AGENT);
			free(h);
		} else {
			if (port && strcmp(port, "80") != 0)
				fprintf(fin, "GET /%s HTTP/1.0\r\nHost: %s:%s\r\n%s\r\n\r\n", 
				    path, host, port, HTTP_USER_AGENT);
			else
				fprintf(fin, "GET /%s HTTP/1.0\r\nHost: %s\r\n%s\r\n\r\n", 
				    path, host, HTTP_USER_AGENT);
		}
	}
	if (fflush(fin) == EOF) {
		warn("Writing HTTP request");
		goto cleanup_url_get;
	}

	if ((buf = fparseln(fin, &len, NULL, "\0\0\0", 0)) == NULL) {
		warn("Receiving HTTP reply");
		goto cleanup_url_get;
	}

	while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
		buf[--len] = '\0';
	if (debug)
		fprintf(ttyout, "received '%s'\n", buf);

	cp = strchr(buf, ' ');
	if (cp == NULL)
		goto improper;
	else
		cp++;
	if (strncmp(cp, "301", 3) == 0 || strncmp(cp, "302", 3) == 0) {
		isredirect++;
	} else if (strncmp(cp, "200", 3)) {
		warnx("Error retrieving file: %s", cp);
		goto cleanup_url_get;
	}

	/*
	 * Read the rest of the header.
	 */
	free(buf);
	filesize = -1;

	while (1) {
		if ((buf = fparseln(fin, &len, NULL, "\0\0\0", 0)) == NULL) {
			warn("Receiving HTTP reply");
			goto cleanup_url_get;
		}
		while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
			buf[--len] = '\0';
		if (len == 0)
			break;
		if (debug)
			fprintf(ttyout, "received '%s'\n", buf);

		/* Look for some headers */
		cp = buf;
#define CONTENTLEN "Content-Length: "
		if (strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0) {
			cp += sizeof(CONTENTLEN) - 1;
			filesize = strtol(cp, &ep, 10);
			if (filesize < 1 || *ep != '\0')
				goto improper;
#define LOCATION "Location: "
		} else if (isredirect &&
		    strncasecmp(cp, LOCATION, sizeof(LOCATION) - 1) == 0) {
			cp += sizeof(LOCATION) - 1;
			if (verbose)
				fprintf(ttyout, "Redirected to %s\n", cp);
			if (fin != NULL)
				fclose(fin);
			else if (s != -1)
				close(s);
			if (proxy)
				free(proxy);
			free(line);
			rval = url_get(cp, proxyenv, outfile);
			if (buf)
				free(buf);
			return (rval);
		}
	}
	free(buf);

	/* Open the output file.  */
	if (strcmp(savefile, "-") != 0) {
		out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if (out < 0) {
			warn("Can't open %s", savefile);
			goto cleanup_url_get;
		}
	} else
		out = fileno(stdout);

	/* Trap signals */
	oldintr = NULL;
	if (setjmp(httpabort)) {
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		goto cleanup_url_get;
	}
	oldintr = signal(SIGINT, aborthttp);

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1);

	/* Finally, suck down the file. */
	if ((buf = malloc(4096)) == NULL)
		errx(1, "Can't allocate memory for transfer buffer\n");
	i = 0;
	while ((len = fread(buf, sizeof(char), 4096, fin)) > 0) {
		bytes += len;
		for (cp = buf; len > 0; len -= i, cp += i) {
			if ((i = write(out, cp, len)) == -1) {
				warn("Writing %s", savefile);
				goto cleanup_url_get;
			}
			else if (i == 0)
				break;
		}
		if (hash && !progress) {
			while (bytes >= hashbytes) {
				(void)putc('#', ttyout);
				hashbytes += mark;
			}
			(void)fflush(ttyout);
		}
	}
	if (hash && !progress && bytes > 0) {
		if (bytes < mark)
			(void)putc('#', ttyout);
		(void)putc('\n', ttyout);
		(void)fflush(ttyout);
	}
	if (len != 0) {
		warn("Reading from socket");
		goto cleanup_url_get;
	}
	progressmeter(1);
	if (filesize != -1 && len == 0 && bytes != filesize) {
		if (verbose)
			fputs("Read short file.\n", ttyout);
		goto cleanup_url_get;
	}

	if (verbose)
		fputs("Successfully retrieved file.\n", ttyout);
	(void)signal(SIGINT, oldintr);

	rval = 0;
	goto cleanup_url_get;

noftpautologin:
	warnx(
	    "Auto-login using ftp URLs isn't supported when using $ftp_proxy");
	goto cleanup_url_get;

improper:
	warnx("Improper response from %s", host);

cleanup_url_get:
	if (fin != NULL)
		fclose(fin);
	else if (s != -1)
		close(s);
	if (buf)
		free(buf);
	if (proxy)
		free(proxy);
	free(line);
	return (rval);
}

/*
 * Abort a http retrieval
 */
void
aborthttp(notused)
	int notused;
{

	alarmtimer(0);
	fputs("\nhttp fetch aborted.\n", ttyout);
	(void)fflush(ttyout);
	longjmp(httpabort, 1);
}

/*
 * Abort a http retrieval
 */
void
abortfile(notused)
	int notused;
{

	alarmtimer(0);
	fputs("\nfile fetch aborted.\n", ttyout);
	(void)fflush(ttyout);
	longjmp(httpabort, 1);
}

/*
 * Retrieve multiple files from the command line, transferring
 * files of the form "host:path", "ftp://host/path" using the
 * ftp protocol, and files of the form "http://host/path" using
 * the http protocol.
 * If path has a trailing "/", then return (-1);
 * the path will be cd-ed into and the connection remains open,
 * and the function will return -1 (to indicate the connection
 * is alive).
 * If an error occurs the return value will be the offset+1 in
 * argv[] of the file that caused a problem (i.e, argv[x]
 * returns x+1)
 * Otherwise, 0 is returned if all files retrieved successfully.
 */
int
auto_fetch(argc, argv, outfile)
	int argc;
	char *argv[];
	char *outfile;
{
	static char lasthost[MAXHOSTNAMELEN];
	char *xargv[5];
	char *cp, *line, *host, *dir, *file, *portnum;
	char *user, *pass;
	char *ftpproxy, *httpproxy;
	int rval, xargc;
	volatile int argpos;
	int dirhasglob, filehasglob;
	char rempath[MAXPATHLEN];

	argpos = 0;

	if (setjmp(toplevel)) {
		if (connected)
			disconnect(0, NULL);
		return (argpos + 1);
	}
	(void)signal(SIGINT, (sig_t)intr);
	(void)signal(SIGPIPE, (sig_t)lostpeer);

	ftpproxy = getenv(FTP_PROXY);
	httpproxy = getenv(HTTP_PROXY);

	/*
	 * Loop through as long as there's files to fetch.
	 */
	for (rval = 0; (rval == 0) && (argpos < argc); free(line), argpos++) {
		if (strchr(argv[argpos], ':') == NULL)
			break;
		host = dir = file = portnum = user = pass = NULL;

		/*
		 * We muck with the string, so we make a copy.
		 */
		line = strdup(argv[argpos]);
		if (line == NULL)
			errx(1, "Can't allocate memory for auto-fetch.");

		/*
		 * Try HTTP URL-style arguments first.
		 */
		if (strncasecmp(line, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
		    strncasecmp(line, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
			if (url_get(line, httpproxy, outfile) == -1)
				rval = argpos + 1;
			continue;
		}

		/*
		 * Try FTP URL-style arguments next. If ftpproxy is
		 * set, use url_get() instead of standard ftp.
		 * Finally, try host:file.
		 */
		host = line;
		if (strncasecmp(line, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
			char *passend, *passagain, *userend;

			if (ftpproxy) {
				if (url_get(line, ftpproxy, outfile) == -1)
					rval = argpos + 1;
				continue;
			}
			host += sizeof(FTP_URL) - 1;
			dir = strchr(host, '/');

			/* Look for [user:pass@]host[:port] */

			/* check if we have "user:pass@" */
			userend = strchr(host, ':');
			passend = strchr(host, '@');
			if (passend && userend && userend < passend &&
			    (!dir || passend < dir)) {
				user = host;
				pass = userend + 1;
				host = passend + 1;
				*userend = *passend = '\0';
				passagain = strchr(host, '@');
				if (strchr(pass, '@') != NULL || 
				    (passagain != NULL && passagain < dir)) {
					warnx(at_encoding_warning);
					goto bad_ftp_url;
				}	

				if (EMPTYSTRING(user) || EMPTYSTRING(pass)) {
bad_ftp_url:
					warnx("Invalid URL: %s", argv[argpos]);
					rval = argpos + 1;
					continue;
				}
				user = urldecode(user);
				pass = urldecode(pass);
			}

#ifdef INET6
			/* check [host]:port, or [host] */
			if (host[0] == '[') {
				cp = strchr(host, ']');
				if (cp && (!dir || cp < dir)) {
					if (cp + 1 == dir || cp[1] == ':') {
						host++;
						*cp++ = '\0';
					} else
						cp = NULL;
				} else
					cp = host;
			} else
				cp = host;
#else
			cp = host;
#endif

			/* split off host[:port] if there is */
			if (cp) {
				portnum = strchr(cp, ':');
				if (!portnum)
					;
				else {
					if (!dir)
						;
					else if (portnum + 1 < dir) {
						*portnum++ = '\0';
						/*
						 * XXX should check if portnum
						 * is decimal number
						 */
					} else {
						/* empty portnum */
						goto bad_ftp_url;
					}
				}
			} else
				portnum = NULL;
		} else {			/* classic style `host:file' */
			dir = strchr(host, ':');
		}
		if (EMPTYSTRING(host)) {
			rval = argpos + 1;
			continue;
		}

		/*
		 * If dir is NULL, the file wasn't specified
		 * (URL looked something like ftp://host)
		 */
		if (dir != NULL)
			*dir++ = '\0';

		/*
		 * Extract the file and (if present) directory name.
		 */
		if (! EMPTYSTRING(dir)) {
			cp = strrchr(dir, '/');
			if (cp != NULL) {
				*cp++ = '\0';
				file = cp;
			} else {
				file = dir;
				dir = NULL;
			}
		}
		if (debug)
			fprintf(ttyout, "user %s:%s host %s port %s dir %s file %s\n",
			    user, pass, host, portnum, dir, file);

		/*
		 * Set up the connection if we don't have one.
		 */
		if (strcmp(host, lasthost) != 0) {
			int oautologin;

			(void)strcpy(lasthost, host);
			if (connected)
				disconnect(0, NULL);
			xargv[0] = __progname;
			xargv[1] = host;
			xargv[2] = NULL;
			xargc = 2;
			if (! EMPTYSTRING(portnum)) {
				xargv[2] = portnum;
				xargv[3] = NULL;
				xargc = 3;
			}
			oautologin = autologin;
			if (user != NULL)
				autologin = 0;
			setpeer(xargc, xargv);
			autologin = oautologin;
			if ((connected == 0) ||
			    ((connected == 1) && !ftp_login(host, user, pass))) {
				warnx("Can't connect or login to host `%s'",
				    host);
				rval = argpos + 1;
				continue;
			}

			/* Always use binary transfers. */
			setbinary(0, NULL);
		}
		/* cd back to '/' */
		xargv[0] = "cd";
		xargv[1] = "/";
		xargv[2] = NULL;
		cd(2, xargv);
		if (! dirchange) {
			rval = argpos + 1;
			continue;
		}

		dirhasglob = filehasglob = 0;
		if (doglob) {
			if (! EMPTYSTRING(dir) &&
			    strpbrk(dir, "*?[]{}") != NULL)
				dirhasglob = 1;
			if (! EMPTYSTRING(file) &&
			    strpbrk(file, "*?[]{}") != NULL)
				filehasglob = 1;
		}

		/* Change directories, if necessary. */
		if (! EMPTYSTRING(dir) && !dirhasglob) {
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

		if (verbose)
			fprintf(ttyout, "Retrieving %s/%s\n", dir ? dir : "", file);

		if (dirhasglob) {
			snprintf(rempath, sizeof(rempath), "%s/%s", dir, file);
			file = rempath;
		}

		/* Fetch the file(s). */
		xargc = 2;
		xargv[0] = "get";
		xargv[1] = file;
		xargv[2] = NULL;
		if (dirhasglob || filehasglob) {
			int ointeractive;

			ointeractive = interactive;
			interactive = 0;
			xargv[0] = "mget";
			mget(xargc, xargv);
			interactive = ointeractive;
		} else {
			if (outfile != NULL) {
				xargv[2] = outfile;
				xargv[3] = NULL;
				xargc++;
			}
			get(xargc, xargv);
		}

		if ((code / 100) != COMPLETE)
			rval = argpos + 1;
	}
	if (connected && rval != -1)
		disconnect(0, NULL);
	return (rval);
}

char *
urldecode(str) 
        const char *str;
{
        char *ret;
        char c;
        int i, reallen;

        if (str == NULL) 
                return NULL;
        if ((ret = malloc(strlen(str)+1)) == NULL)
                err(1, "Can't allocate memory for URL decoding");
        for (i = 0, reallen = 0; str[i] != '\0'; i++, reallen++, ret++) {
                c = str[i];
                if (c == '+') {
                        *ret = ' ';
                        continue;
                }
                /* Can't use strtol here because next char after %xx may be
                 * a digit. */
                if (c == '%' && isxdigit(str[i+1]) && isxdigit(str[i+2])) {
                        *ret = hextochar(&str[i+1]);
                        i+=2;
                        continue;
                }
                *ret = c;
        }
        *ret = '\0';

        return ret-reallen;
}

char
hextochar(str)
        const char *str;
{
        char c, ret;

        c = str[0];
        ret = c;
        if (isalpha(c))
                ret -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
                ret -= '0';
        ret *= 16;

        c = str[1];
        ret += c;
        if (isalpha(c))
                ret -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
                ret -= '0';
        return ret;
}

int
isurl(p)
	const char *p;
{

	if (strncasecmp(p, FTP_URL, sizeof(FTP_URL) - 1) == 0 ||
	    strncasecmp(p, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
	    strncasecmp(p, FILE_URL, sizeof(FILE_URL) - 1) == 0 ||
	    strstr(p, ":/"))
		return (1);
	return (0);
}
