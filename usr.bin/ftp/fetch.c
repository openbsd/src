/*	$OpenBSD: fetch.c,v 1.26 2000/01/04 17:15:12 deraadt Exp $	*/
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
static char rcsid[] = "$OpenBSD: fetch.c,v 1.26 2000/01/04 17:15:12 deraadt Exp $";
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

#include "ftp_var.h"

static int	url_get __P((const char *, const char *, const char *));
void		aborthttp __P((int));
void		abortfile __P((int));


#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */
#define	FILE_URL	"file:"		/* file URL prefix */
#define FTP_PROXY	"ftp_proxy"	/* env var with ftp proxy location */
#define HTTP_PROXY	"http_proxy"	/* env var with http proxy location */


#define EMPTYSTRING(x)	((x) == NULL || (*(x) == '\0'))

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
	int i, out, isftpurl, isfileurl;
	volatile int s;
	size_t len;
	char c, *cp, *ep, *portnum, *path, buf[4096];
	const char *savefile;
	char *line, *proxy, *host, *port;
	volatile sig_t oldintr;
	off_t hashbytes;
	char *cause = "unknown";

	s = -1;
	proxy = NULL;
	isftpurl = 0;
	isfileurl = 0;

#ifdef __GNUC__				/* XXX: to shut up gcc warnings */
	(void)&out;
	(void)&proxy;
	(void)&savefile;
#endif

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
	
		/* Finally, suck down the file. */
		i = 0;
		while ((len = read(s, buf, sizeof(buf))) > 0) {
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
	
		close(s);
		if (out != fileno(stdout))
			close(out);
		if (proxy)
			free(proxy);
		free(line);
		return (0);
	}

	portnum = strchr(host, ':');			/* find portnum */
	if (portnum != NULL)
		*portnum++ = '\0';

	if (debug)
		fprintf(ttyout, "host %s, port %s, path %s, save as %s.\n",
		    host, portnum, path, savefile);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	port = portnum ? portnum : httpport;
	error = getaddrinfo(host, port, &hints, &res0);
	if (error) {
		warnx("%s: %s", gai_strerror(error), host);
		goto cleanup_url_get;
	}

	s = -1;
	for (res = res0; res; res = res->ai_next) {
		getnameinfo(res->ai_addr, res->ai_addrlen, buf, sizeof(buf),
			NULL, 0, NI_NUMERICHOST);
		fprintf(ttyout, "Trying %s...\n", buf);

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

		break;
	}
	freeaddrinfo(res0);
	if (s < 0) {
		warn(cause);
		goto cleanup_url_get;
	}

	/*
	 * Construct and send the request.  We're expecting a return
	 * status of "200". Proxy requests don't want leading /.
	 */
	if (verbose) {
		if (!proxy)
			fprintf(ttyout, "Requesting %s\n", origline);
		else
			fprintf(ttyout, "Requesting %s (via %s)\n",
			    origline, proxyenv);
	}
	snprintf(buf, sizeof(buf), "GET %s%s HTTP/1.0\r\nHost: %s\r\n\r\n",
	    proxy ? "" : "/", path, host);
	len = strlen(buf);
	if (write(s, buf, len) < len) {
		warn("Writing HTTP request");
		goto cleanup_url_get;
	}
	memset(buf, 0, sizeof(buf));
	for (cp = buf; cp < buf + sizeof(buf); ) {
		if (read(s, cp, 1) != 1)
			goto improper;
		if (*cp == '\r')
			continue;
		if (*cp == '\n')
			break;
		cp++;
	}
	buf[sizeof(buf) - 1] = '\0';		/* sanity */
	cp = strchr(buf, ' ');
	if (cp == NULL)
		goto improper;
	else
		cp++;
	if (strncmp(cp, "200", 3)) {
		warnx("Error retrieving file: %s", cp);
		goto cleanup_url_get;
	}

	/*
	 * Read the rest of the header.
	 */
	memset(buf, 0, sizeof(buf));
	c = '\0';
	for (cp = buf; cp < buf + sizeof(buf); ) {
		if (read(s, cp, 1) != 1)
			goto improper;
		if (*cp == '\r')
			continue;
		if (*cp == '\n' && c == '\n')
			break;
		c = *cp;
		cp++;
	}
	buf[sizeof(buf) - 1] = '\0';		/* sanity */

	/* Look for the "Content-length: " header.  */
#define CONTENTLEN "Content-Length: "
	for (cp = buf; *cp != '\0'; cp++) {
		if (tolower(*cp) == 'c' &&
		    strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0)
			break;
	}
	if (*cp != '\0') {
		cp += sizeof(CONTENTLEN) - 1;
		ep = strchr(cp, '\n');
		if (ep == NULL)
			goto improper;
		else
			*ep = '\0';
		filesize = strtol(cp, &ep, 10);
		if (filesize < 1 || *ep != '\0')
			goto improper;
	} else
		filesize = -1;

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
	i = 0;
	while ((len = read(s, buf, sizeof(buf))) > 0) {
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

	close(s);
	if (out != fileno(stdout))
		close(out);
	if (proxy)
		free(proxy);
	free(line);
	return (0);

noftpautologin:
	warnx(
	    "Auto-login using ftp URLs isn't supported when using $ftp_proxy");
	goto cleanup_url_get;

improper:
	warnx("Improper response from %s", host);

cleanup_url_get:
	if (s != -1)
		close(s);
	if (proxy)
		free(proxy);
	free(line);
	return (-1);
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
			if (ftpproxy) {
				if (url_get(line, ftpproxy, outfile) == -1)
					rval = argpos + 1;
				continue;
			}
			host += sizeof(FTP_URL) - 1;
			dir = strchr(host, '/');

			/* Look for [user:pass@]host[:port] */
			pass = strpbrk(host, ":@/");
			if (pass == NULL || *pass == '/') {
				pass = NULL;
				goto parsed_url;
			}
			if (pass == host || *pass == '@') {
bad_ftp_url:
				warnx("Invalid URL: %s", argv[argpos]);
				rval = argpos + 1;
				continue;
			}
			*pass++ = '\0';
			/* XXX - assumes no '@' in pathname */
			if ((cp = strrchr(pass, '@')) == NULL)
				cp = strpbrk(pass, ":@/");
			if (cp == NULL || *cp == '/') {
				portnum = pass;
				pass = NULL;
				goto parsed_url;
			}
			if (EMPTYSTRING(cp) || *cp == ':')
				goto bad_ftp_url;
			*cp++ = '\0';
			user = host;
			if (EMPTYSTRING(user))
				goto bad_ftp_url;
			host = cp;

#if 0
			/* look for IPv6 address URL */
			if (host == '[' && (cp = strrchr(host, ']'))) {
				host++;
				*cp++ = '\0';
			} else
				cp = host;
#endif
		
			portnum = strrchr(cp, ':');
			if (portnum != NULL)
				*portnum++ = '\0';
		} else {			/* classic style `host:file' */
			dir = strchr(host, ':');
		}
parsed_url:
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
			    ((connected == 1) && !login(host, user, pass))) {
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

int
isurl(p)
	const char *p;
{
	if (strncasecmp(p, FTP_URL, sizeof(FTP_URL) - 1) == 0 ||
	    strncasecmp(p, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
	    strncasecmp(p, FILE_URL, sizeof(FILE_URL) - 1) == 0)
		return 1;
	return 0;
}
