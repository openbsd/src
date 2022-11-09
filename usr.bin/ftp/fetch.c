/*	$OpenBSD: fetch.c,v 1.212 2022/11/09 17:41:05 claudio Exp $	*/
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

/*
 * FTP User Program -- Command line file retrieval
 */

#include <sys/types.h>
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
#include <vis.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <resolv.h>
#include <utime.h>

#ifndef NOSSL
#include <tls.h>
#else /* !NOSSL */
struct tls;
#endif /* !NOSSL */

#include "ftp_var.h"
#include "cmds.h"

static int	file_get(const char *, const char *);
static int	url_get(const char *, const char *, const char *, int);
static int	save_chunked(FILE *, struct tls *, int , char *, size_t);
static void	aborthttp(int);
static char	hextochar(const char *);
static char	*urldecode(const char *);
static char	*recode_credentials(const char *_userinfo);
static void	ftp_close(FILE **, struct tls **, int *);
static const char *sockerror(struct tls *);
#ifdef SMALL
#define 	ftp_printf(fp, ...) fprintf(fp, __VA_ARGS__)
#else
static int	ftp_printf(FILE *, const char *, ...);
#endif /* SMALL */
#ifndef NOSSL
static int	proxy_connect(int, char *, char *);
static int	stdio_tls_write_wrapper(void *, const char *, int);
static int	stdio_tls_read_wrapper(void *, char *, int);
#endif /* !NOSSL */

#define	FTP_URL		"ftp://"	/* ftp URL prefix */
#define	HTTP_URL	"http://"	/* http URL prefix */
#define	HTTPS_URL	"https://"	/* https URL prefix */
#define	FILE_URL	"file:"		/* file URL prefix */
#define FTP_PROXY	"ftp_proxy"	/* env var with ftp proxy location */
#define HTTP_PROXY	"http_proxy"	/* env var with http proxy location */

#define EMPTYSTRING(x)	((x) == NULL || (*(x) == '\0'))

static const char at_encoding_warning[] =
    "Extra `@' characters in usernames and passwords should be encoded as %%40";

static jmp_buf	httpabort;

static int	redirect_loop;
static int	retried;

/*
 * Determine whether the character needs encoding, per RFC2396.
 */
static int
to_encode(const char *c0)
{
	/* 2.4.3. Excluded US-ASCII Characters */
	const char *excluded_chars =
	    " "		/* space */
	    "<>#\""	/* delims (modulo "%", see below) */
	    "{}|\\^[]`"	/* unwise */
	    ;
	const unsigned char *c = (const unsigned char *)c0;

	/*
	 * No corresponding graphic US-ASCII.
	 * Control characters and octets not used in US-ASCII.
	 */
	return (iscntrl(*c) || !isascii(*c) ||

	    /*
	     * '%' is also reserved, if is not followed by two
	     * hexadecimal digits.
	     */
	    strchr(excluded_chars, *c) != NULL ||
	    (*c == '%' && (!isxdigit(c[1]) || !isxdigit(c[2]))));
}

/*
 * Encode given URL, per RFC2396.
 * Allocate and return string to the caller.
 */
static char *
url_encode(const char *path)
{
	size_t i, length, new_length;
	char *epath, *epathp;

	length = new_length = strlen(path);

	/*
	 * First pass:
	 * Count characters to encode and determine length of the final URL.
	 */
	for (i = 0; i < length; i++)
		if (to_encode(path + i))
			new_length += 2;

	epath = epathp = malloc(new_length + 1);	/* One more for '\0'. */
	if (epath == NULL)
		err(1, "Can't allocate memory for URL encoding");

	/*
	 * Second pass:
	 * Encode, and copy final URL.
	 */
	for (i = 0; i < length; i++)
		if (to_encode(path + i)) {
			snprintf(epathp, 4, "%%" "%02x",
			    (unsigned char)path[i]);
			epathp += 3;
		} else
			*(epathp++) = path[i];

	*epathp = '\0';
	return (epath);
}

/*
 * Copy a local file (used by the OpenBSD installer).
 * Returns -1 on failure, 0 on success
 */
static int
file_get(const char *path, const char *outfile)
{
	struct stat	 st;
	int		 fd, out = -1, rval = -1, save_errno;
	volatile sig_t	 oldintr, oldinti;
	const char	*savefile;
	char		*buf = NULL, *cp, *pathbuf = NULL;
	const size_t	 buflen = 128 * 1024;
	off_t		 hashbytes;
	ssize_t		 len, wlen;

	direction = "received";

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		warn("Can't open file %s", path);
		return -1;
	}

	if (fstat(fd, &st) == -1)
		filesize = -1;
	else
		filesize = st.st_size;

	if (outfile != NULL)
		savefile = outfile;
	else {
		if (path[strlen(path) - 1] == '/')	/* Consider no file */
			savefile = NULL;		/* after dir invalid. */
		else {
			pathbuf = strdup(path);
			if (pathbuf == NULL)
				errx(1, "Can't allocate memory for filename");
			savefile = basename(pathbuf);
		}
	}

	if (EMPTYSTRING(savefile)) {
		warnx("No filename after directory (use -o): %s", path);
		goto cleanup_copy;
	}

	/* Open the output file.  */
	if (!pipeout) {
		out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
		if (out == -1) {
			warn("Can't open %s", savefile);
			goto cleanup_copy;
		}
	} else
		out = fileno(stdout);

	if ((buf = malloc(buflen)) == NULL)
		errx(1, "Can't allocate memory for transfer buffer");

	/* Trap signals */
	oldintr = NULL;
	oldinti = NULL;
	if (setjmp(httpabort)) {
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		goto cleanup_copy;
	}
	oldintr = signal(SIGINT, aborthttp);

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1, path);

	/* Finally, suck down the file. */
	oldinti = signal(SIGINFO, psummary);
	while ((len = read(fd, buf, buflen)) > 0) {
		bytes += len;
		for (cp = buf; len > 0; len -= wlen, cp += wlen) {
			if ((wlen = write(out, cp, len)) == -1) {
				warn("Writing %s", savefile);
				signal(SIGINT, oldintr);
				signal(SIGINFO, oldinti);
				goto cleanup_copy;
			}
		}
		if (hash && !progress) {
			while (bytes >= hashbytes) {
				(void)putc('#', ttyout);
				hashbytes += mark;
			}
			(void)fflush(ttyout);
		}
	}
	save_errno = errno;
	signal(SIGINT, oldintr);
	signal(SIGINFO, oldinti);
	if (hash && !progress && bytes > 0) {
		if (bytes < mark)
			(void)putc('#', ttyout);
		(void)putc('\n', ttyout);
		(void)fflush(ttyout);
	}
	if (len == -1) {
		warnc(save_errno, "Reading from file");
		goto cleanup_copy;
	}
	progressmeter(1, NULL);
	if (verbose)
		ptransfer(0);

	rval = 0;

cleanup_copy:
	free(buf);
	free(pathbuf);
	if (out >= 0 && out != fileno(stdout))
		close(out);
	close(fd);

	return rval;
}

/*
 * Retrieve URL, via the proxy in $proxyvar if necessary.
 * Returns -1 on failure, 0 on success
 */
static int
url_get(const char *origline, const char *proxyenv, const char *outfile, int lastfile)
{
	char pbuf[NI_MAXSERV], hbuf[NI_MAXHOST], *cp, *portnum, *path, ststr[4];
	char *hosttail, *cause = "unknown", *newline, *host, *port, *buf = NULL;
	char *epath, *redirurl, *loctail, *h, *p, gerror[200];
	int error, isftpurl = 0, isredirect = 0, rval = -1;
	int isunavail = 0, retryafter = -1;
	struct addrinfo hints, *res0, *res;
	const char *savefile;
	char *pathbuf = NULL;
	char *proxyurl = NULL;
	char *credentials = NULL, *proxy_credentials = NULL;
	int fd = -1, out = -1;
	volatile sig_t oldintr, oldinti;
	FILE *fin = NULL;
	off_t hashbytes;
	const char *errstr;
	ssize_t len, wlen;
	size_t bufsize;
	char *proxyhost = NULL;
#ifndef NOSSL
	char *sslpath = NULL, *sslhost = NULL;
	int ishttpsurl = 0;
#endif /* !NOSSL */
#ifndef SMALL
	char *full_host = NULL;
	const char *scheme;
	char *locbase;
	struct addrinfo *ares = NULL;
	char tmbuf[32];
	time_t mtime = 0;
	struct stat stbuf;
	struct tm lmt = { 0 };
	struct timespec ts[2];
#endif /* !SMALL */
	struct tls *tls = NULL;
	int status;
	int save_errno;
	const size_t buflen = 128 * 1024;
	int chunked = 0;

	direction = "received";

	newline = strdup(origline);
	if (newline == NULL)
		errx(1, "Can't allocate memory to parse URL");
	if (strncasecmp(newline, HTTP_URL, sizeof(HTTP_URL) - 1) == 0) {
		host = newline + sizeof(HTTP_URL) - 1;
#ifndef SMALL
		scheme = HTTP_URL;
#endif /* !SMALL */
	} else if (strncasecmp(newline, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
		host = newline + sizeof(FTP_URL) - 1;
		isftpurl = 1;
#ifndef SMALL
		scheme = FTP_URL;
#endif /* !SMALL */
	} else if (strncasecmp(newline, HTTPS_URL, sizeof(HTTPS_URL) - 1) == 0) {
#ifndef NOSSL
		host = newline + sizeof(HTTPS_URL) - 1;
		ishttpsurl = 1;
#else
		errx(1, "%s: No HTTPS support", newline);
#endif /* !NOSSL */
#ifndef SMALL
		scheme = HTTPS_URL;
#endif /* !SMALL */
	} else
		errx(1, "%s: URL not permitted", newline);

	path = strchr(host, '/');		/* Find path */

	/*
	 * Look for auth header in host.
	 * Basic auth from RFC 2617, valid characters for path are in
	 * RFC 3986 section 3.3.
	 */
	if (!isftpurl) {
		p = strchr(host, '@');
		if (p != NULL && (path == NULL || p < path)) {
			*p++ = '\0';
			credentials = recode_credentials(host);

			/* Overwrite userinfo */
			memmove(host, p, strlen(p) + 1);
			path = strchr(host, '/');
		}
	}

	if (EMPTYSTRING(path)) {
		if (outfile) {				/* No slash, but */
			path = strchr(host,'\0');	/* we have outfile. */
			goto noslash;
		}
		if (isftpurl)
			goto noftpautologin;
		warnx("No `/' after host (use -o): %s", origline);
		goto cleanup_url_get;
	}
	*path++ = '\0';
	if (EMPTYSTRING(path) && !outfile) {
		if (isftpurl)
			goto noftpautologin;
		warnx("No filename after host (use -o): %s", origline);
		goto cleanup_url_get;
	}

noslash:
	if (outfile)
		savefile = outfile;
	else {
		if (path[strlen(path) - 1] == '/')	/* Consider no file */
			savefile = NULL;		/* after dir invalid. */
		else {
			pathbuf = strdup(path);
			if (pathbuf == NULL)
				errx(1, "Can't allocate memory for filename");
			savefile = basename(pathbuf);
		}
	}

	if (EMPTYSTRING(savefile)) {
		if (isftpurl)
			goto noftpautologin;
		warnx("No filename after directory (use -o): %s", origline);
		goto cleanup_url_get;
	}

#ifndef SMALL
	if (resume && pipeout) {
		warnx("can't append to stdout");
		goto cleanup_url_get;
	}
#endif /* !SMALL */

	if (proxyenv != NULL) {		/* use proxy */
#ifndef NOSSL
		if (ishttpsurl) {
			sslpath = strdup(path);
			sslhost = strdup(host);
			if (! sslpath || ! sslhost)
				errx(1, "Can't allocate memory for https path/host.");
		}
#endif /* !NOSSL */
		proxyhost = strdup(host);
		if (proxyhost == NULL)
			errx(1, "Can't allocate memory for proxy host.");
		proxyurl = strdup(proxyenv);
		if (proxyurl == NULL)
			errx(1, "Can't allocate memory for proxy URL.");
		if (strncasecmp(proxyurl, HTTP_URL, sizeof(HTTP_URL) - 1) == 0)
			host = proxyurl + sizeof(HTTP_URL) - 1;
		else if (strncasecmp(proxyurl, FTP_URL, sizeof(FTP_URL) - 1) == 0)
			host = proxyurl + sizeof(FTP_URL) - 1;
		else {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		if (EMPTYSTRING(host)) {
			warnx("Malformed proxy URL: %s", proxyenv);
			goto cleanup_url_get;
		}
		if (*--path == '\0')
			*path = '/';		/* add / back to real path */
		path = strchr(host, '/');	/* remove trailing / on host */
		if (!EMPTYSTRING(path))
			*path++ = '\0';		/* i guess this ++ is useless */

		path = strchr(host, '@');	/* look for credentials in proxy */
		if (!EMPTYSTRING(path)) {
			*path = '\0';
			if (strchr(host, ':') == NULL) {
				warnx("Malformed proxy URL: %s", proxyenv);
				goto cleanup_url_get;
			}
			proxy_credentials = recode_credentials(host);
			*path = '@'; /* restore @ in proxyurl */

			/*
			 * This removes the password from proxyurl,
			 * filling with stars
			 */
			for (host = 1 + strchr(proxyurl + 5, ':');  *host != '@';
			    host++)
				*host = '*';

			host = path + 1;
		}

		path = newline;
	}

	if (*host == '[' && (hosttail = strrchr(host, ']')) != NULL &&
	    (hosttail[1] == '\0' || hosttail[1] == ':')) {
		host++;
		*hosttail++ = '\0';
#ifndef SMALL
		if (asprintf(&full_host, "[%s]", host) == -1)
			errx(1, "Cannot allocate memory for hostname");
#endif /* !SMALL */
	} else
		hosttail = host;

	portnum = strrchr(hosttail, ':');		/* find portnum */
	if (portnum != NULL)
		*portnum++ = '\0';
#ifndef NOSSL
	port = portnum ? portnum : (ishttpsurl ? httpsport : httpport);
#else /* !NOSSL */
	port = portnum ? portnum : httpport;
#endif /* !NOSSL */

#ifndef SMALL
	if (full_host == NULL)
		if ((full_host = strdup(host)) == NULL)
			errx(1, "Cannot allocate memory for hostname");
	if (debug)
		fprintf(ttyout, "host %s, port %s, path %s, "
		    "save as %s, auth %s.\n", host, port, path,
		    savefile, credentials ? credentials : "none");
#endif /* !SMALL */

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(host, port, &hints, &res0);
	/*
	 * If the services file is corrupt/missing, fall back
	 * on our hard-coded defines.
	 */
	if (error == EAI_SERVICE && port == httpport) {
		snprintf(pbuf, sizeof(pbuf), "%d", HTTP_PORT);
		error = getaddrinfo(host, pbuf, &hints, &res0);
#ifndef NOSSL
	} else if (error == EAI_SERVICE && port == httpsport) {
		snprintf(pbuf, sizeof(pbuf), "%d", HTTPS_PORT);
		error = getaddrinfo(host, pbuf, &hints, &res0);
#endif /* !NOSSL */
	}
	if (error) {
		warnx("%s: %s", host, gai_strerror(error));
		goto cleanup_url_get;
	}

#ifndef SMALL
	if (srcaddr) {
		hints.ai_flags |= AI_NUMERICHOST;
		error = getaddrinfo(srcaddr, NULL, &hints, &ares);
		if (error) {
			warnx("%s: %s", srcaddr, gai_strerror(error));
			goto cleanup_url_get;
		}
	}
#endif /* !SMALL */

	/* ensure consistent order of the output */
	if (verbose)
		setvbuf(ttyout, NULL, _IOLBF, 0);

	fd = -1;
	for (res = res0; res; res = res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "(unknown)", sizeof(hbuf));
		if (verbose)
			fprintf(ttyout, "Trying %s...\n", hbuf);

		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd == -1) {
			cause = "socket";
			continue;
		}

#ifndef SMALL
		if (srcaddr) {
			if (ares->ai_family != res->ai_family) {
				close(fd);
				fd = -1;
				errno = EINVAL;
				cause = "bind";
				continue;
			}
			if (bind(fd, ares->ai_addr, ares->ai_addrlen) == -1) {
				save_errno = errno;
				close(fd);
				errno = save_errno;
				fd = -1;
				cause = "bind";
				continue;
			}
		}
#endif /* !SMALL */

		error = timed_connect(fd, res->ai_addr, res->ai_addrlen,
		    connect_timeout);
		if (error != 0) {
			save_errno = errno;
			close(fd);
			errno = save_errno;
			fd = -1;
			cause = "connect";
			continue;
		}

		/* get port in numeric */
		if (getnameinfo(res->ai_addr, res->ai_addrlen, NULL, 0,
		    pbuf, sizeof(pbuf), NI_NUMERICSERV) == 0)
			port = pbuf;
		else
			port = NULL;

#ifndef NOSSL
		if (proxyenv && sslhost)
			proxy_connect(fd, sslhost, proxy_credentials);
#endif /* !NOSSL */
		break;
	}
	freeaddrinfo(res0);
#ifndef SMALL
	if (srcaddr)
		freeaddrinfo(ares);
#endif /* !SMALL */
	if (fd < 0) {
		warn("%s", cause);
		goto cleanup_url_get;
	}

#ifndef NOSSL
	if (ishttpsurl) {
		ssize_t ret;
		if (proxyenv && sslpath) {
			ishttpsurl = 0;
			proxyurl = NULL;
			path = sslpath;
		}
		if (sslhost == NULL) {
			sslhost = strdup(host);
			if (sslhost == NULL)
				errx(1, "Can't allocate memory for https host.");
		}
		if ((tls = tls_client()) == NULL) {
			fprintf(ttyout, "failed to create SSL client\n");
			goto cleanup_url_get;
		}
		if (tls_configure(tls, tls_config) != 0) {
			fprintf(ttyout, "TLS configuration failure: %s\n",
			    tls_error(tls));
			goto cleanup_url_get;
		}
		if (tls_connect_socket(tls, fd, sslhost) != 0) {
			fprintf(ttyout, "TLS connect failure: %s\n", tls_error(tls));
			goto cleanup_url_get;
		}
		do {
			ret = tls_handshake(tls);
		} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
		if (ret != 0) {
			fprintf(ttyout, "TLS handshake failure: %s\n", tls_error(tls));
			goto cleanup_url_get;
		}
		fin = funopen(tls, stdio_tls_read_wrapper,
		    stdio_tls_write_wrapper, NULL, NULL);
	} else {
		fin = fdopen(fd, "r+");
		fd = -1;
	}
#else /* !NOSSL */
	fin = fdopen(fd, "r+");
	fd = -1;
#endif /* !NOSSL */

#ifdef SMALL
	if (lastfile) {
		if (pipeout) {
			if (pledge("stdio rpath inet dns tty",  NULL) == -1)
				err(1, "pledge");
		} else {
			if (pledge("stdio rpath wpath cpath inet dns tty", NULL) == -1)
				err(1, "pledge");
		}
	}
#endif

	/*
	 * Construct and send the request. Proxy requests don't want leading /.
	 */
#ifndef NOSSL
	cookie_get(host, path, ishttpsurl, &buf);
#endif /* !NOSSL */

	epath = url_encode(path);
	if (proxyurl) {
		if (verbose) {
			fprintf(ttyout, "Requesting %s (via %s)\n",
			    origline, proxyurl);
		}
		/*
		 * Host: directive must use the destination host address for
		 * the original URI (path).
		 */
		ftp_printf(fin, "GET %s HTTP/1.1\r\n"
		    "Connection: close\r\n"
		    "Host: %s\r\n%s%s\r\n",
		    epath, proxyhost, buf ? buf : "", httpuseragent);
		if (credentials)
			ftp_printf(fin, "Authorization: Basic %s\r\n",
			    credentials);
		if (proxy_credentials)
			ftp_printf(fin, "Proxy-Authorization: Basic %s\r\n",
			    proxy_credentials);
		ftp_printf(fin, "\r\n");
	} else {
		if (verbose)
			fprintf(ttyout, "Requesting %s\n", origline);
#ifndef SMALL
		if (resume || timestamp) {
			if (stat(savefile, &stbuf) == 0) {
				if (resume)
					restart_point = stbuf.st_size;
				if (timestamp)
					mtime = stbuf.st_mtime;
			} else {
				restart_point = 0;
				mtime = 0;
			}
		}
#endif	/* SMALL */
		ftp_printf(fin,
		    "GET /%s HTTP/1.1\r\n"
		    "Connection: close\r\n"
		    "Host: ", epath);
		if (proxyhost) {
			ftp_printf(fin, "%s", proxyhost);
			port = NULL;
		} else if (strchr(host, ':')) {
			/*
			 * strip off scoped address portion, since it's
			 * local to node
			 */
			h = strdup(host);
			if (h == NULL)
				errx(1, "Can't allocate memory.");
			if ((p = strchr(h, '%')) != NULL)
				*p = '\0';
			ftp_printf(fin, "[%s]", h);
			free(h);
		} else
			ftp_printf(fin, "%s", host);

		/*
		 * Send port number only if it's specified and does not equal
		 * 80. Some broken HTTP servers get confused if you explicitly
		 * send them the port number.
		 */
#ifndef NOSSL
		if (port && strcmp(port, (ishttpsurl ? "443" : "80")) != 0)
			ftp_printf(fin, ":%s", port);
		if (restart_point)
			ftp_printf(fin, "\r\nRange: bytes=%lld-",
				(long long)restart_point);
#else /* !NOSSL */
		if (port && strcmp(port, "80") != 0)
			ftp_printf(fin, ":%s", port);
#endif /* !NOSSL */

#ifndef SMALL
		if (mtime && (http_time(mtime, tmbuf, sizeof(tmbuf)) != 0))
			ftp_printf(fin, "\r\nIf-Modified-Since: %s", tmbuf);
#endif /* SMALL */

		ftp_printf(fin, "\r\n%s%s\r\n",
		    buf ? buf : "", httpuseragent);
		if (credentials)
			ftp_printf(fin, "Authorization: Basic %s\r\n",
			    credentials);
		ftp_printf(fin, "\r\n");
	}
	free(epath);

#ifndef NOSSL
	free(buf);
#endif /* !NOSSL */
	buf = NULL;
	bufsize = 0;

	if (fflush(fin) == EOF) {
		warnx("Writing HTTP request: %s", sockerror(tls));
		goto cleanup_url_get;
	}
	if ((len = getline(&buf, &bufsize, fin)) == -1) {
		warnx("Receiving HTTP reply: %s", sockerror(tls));
		goto cleanup_url_get;
	}

	while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
		buf[--len] = '\0';
#ifndef SMALL
	if (debug)
		fprintf(ttyout, "received '%s'\n", buf);
#endif /* !SMALL */

	cp = strchr(buf, ' ');
	if (cp == NULL)
		goto improper;
	else
		cp++;

	strlcpy(ststr, cp, sizeof(ststr));
	status = strtonum(ststr, 200, 503, &errstr);
	if (errstr) {
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", origline, gerror);
		goto cleanup_url_get;
	}

	switch (status) {
	case 200:	/* OK */
#ifndef SMALL
		/*
		 * When we request a partial file, and we receive an HTTP 200
		 * it is a good indication that the server doesn't support
		 * range requests, and is about to send us the entire file.
		 * If the restart_point == 0, then we are not actually
		 * requesting a partial file, and an HTTP 200 is appropriate.
		 */
		if (resume && restart_point != 0) {
			warnx("Server does not support resume.");
			restart_point = resume = 0;
		}
		/* FALLTHROUGH */
	case 206:	/* Partial Content */
#endif /* !SMALL */
		break;
	case 301:	/* Moved Permanently */
	case 302:	/* Found */
	case 303:	/* See Other */
	case 307:	/* Temporary Redirect */
	case 308:	/* Permanent Redirect (RFC 7538) */
		isredirect++;
		if (redirect_loop++ > 10) {
			warnx("Too many redirections requested");
			goto cleanup_url_get;
		}
		break;
#ifndef SMALL
	case 304:	/* Not Modified */
		warnx("File is not modified on the server");
		goto cleanup_url_get;
	case 416:	/* Requested Range Not Satisfiable */
		warnx("File is already fully retrieved.");
		goto cleanup_url_get;
#endif /* !SMALL */
	case 503:
		isunavail = 1;
		break;
	default:
		strnvis(gerror, cp, sizeof gerror, VIS_SAFE);
		warnx("Error retrieving %s: %s", origline, gerror);
		goto cleanup_url_get;
	}

	/*
	 * Read the rest of the header.
	 */
	filesize = -1;

	for (;;) {
		if ((len = getline(&buf, &bufsize, fin)) == -1) {
			warnx("Receiving HTTP reply: %s", sockerror(tls));
			goto cleanup_url_get;
		}

		while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n' ||
		    buf[len-1] == ' ' || buf[len-1] == '\t'))
			buf[--len] = '\0';
		if (len == 0)
			break;
#ifndef SMALL
		if (debug)
			fprintf(ttyout, "received '%s'\n", buf);
#endif /* !SMALL */

		/* Look for some headers */
		cp = buf;
#define CONTENTLEN "Content-Length:"
		if (strncasecmp(cp, CONTENTLEN, sizeof(CONTENTLEN) - 1) == 0) {
			cp += sizeof(CONTENTLEN) - 1;
			cp += strspn(cp, " \t");
			cp[strcspn(cp, " \t")] = '\0';
			filesize = strtonum(cp, 0, LLONG_MAX, &errstr);
			if (errstr != NULL)
				goto improper;
#ifndef SMALL
			if (restart_point)
				filesize += restart_point;
#endif /* !SMALL */
#define LOCATION "Location:"
		} else if (isredirect &&
		    strncasecmp(cp, LOCATION, sizeof(LOCATION) - 1) == 0) {
			cp += sizeof(LOCATION) - 1;
			cp += strspn(cp, " \t");
			/*
			 * If there is a colon before the first slash, this URI
			 * is not relative. RFC 3986 4.2
			 */
			if (cp[strcspn(cp, ":/")] != ':') {
#ifdef SMALL
				errx(1, "Relative redirect not supported");
#else /* SMALL */
				/* XXX doesn't handle protocol-relative URIs */
				if (*cp == '/') {
					locbase = NULL;
					cp++;
				} else {
					locbase = strdup(path);
					if (locbase == NULL)
						errx(1, "Can't allocate memory"
						    " for location base");
					loctail = strchr(locbase, '#');
					if (loctail != NULL)
						*loctail = '\0';
					loctail = strchr(locbase, '?');
					if (loctail != NULL)
						*loctail = '\0';
					loctail = strrchr(locbase, '/');
					if (loctail == NULL) {
						free(locbase);
						locbase = NULL;
					} else
						loctail[1] = '\0';
				}
				/* Contruct URL from relative redirect */
				if (asprintf(&redirurl, "%s%s%s%s/%s%s",
				    scheme, full_host,
				    portnum ? ":" : "",
				    portnum ? portnum : "",
				    locbase ? locbase : "",
				    cp) == -1)
					errx(1, "Cannot build "
					    "redirect URL");
				free(locbase);
#endif /* SMALL */
			} else if ((redirurl = strdup(cp)) == NULL)
				errx(1, "Cannot allocate memory for URL");
			loctail = strchr(redirurl, '#');
			if (loctail != NULL)
				*loctail = '\0';
			if (verbose) {
				char *visbuf;
				if (stravis(&visbuf, redirurl, VIS_SAFE) == -1)
					err(1, "Cannot vis redirect URL");
				fprintf(ttyout, "Redirected to %s\n", visbuf);
				free(visbuf);
			}
			ftp_close(&fin, &tls, &fd);
			rval = url_get(redirurl, proxyenv, savefile, lastfile);
			free(redirurl);
			goto cleanup_url_get;
#define RETRYAFTER "Retry-After:"
		} else if (isunavail &&
		    strncasecmp(cp, RETRYAFTER, sizeof(RETRYAFTER) - 1) == 0) {
			size_t s;
			cp += sizeof(RETRYAFTER) - 1;
			cp += strspn(cp, " \t");
			cp[strcspn(cp, " \t")] = '\0';
			retryafter = strtonum(cp, 0, 0, &errstr);
			if (errstr != NULL)
				retryafter = -1;
#define TRANSFER_ENCODING "Transfer-Encoding:"
		} else if (strncasecmp(cp, TRANSFER_ENCODING,
			    sizeof(TRANSFER_ENCODING) - 1) == 0) {
			cp += sizeof(TRANSFER_ENCODING) - 1;
			cp += strspn(cp, " \t");
			cp[strcspn(cp, " \t")] = '\0';
			if (strcasecmp(cp, "chunked") == 0)
				chunked = 1;
#ifndef SMALL
#define LAST_MODIFIED "Last-Modified:"
		} else if (strncasecmp(cp, LAST_MODIFIED,
			    sizeof(LAST_MODIFIED) - 1) == 0) {
			cp += sizeof(LAST_MODIFIED) - 1;
			cp[strcspn(cp, "\t")] = '\0';
			if (strptime(cp, "%a, %d %h %Y %T %Z", &lmt) == NULL)
				server_timestamps = 0;
#endif /* !SMALL */
		}
	}
	free(buf);
	buf = NULL;

	/* Content-Length should be ignored for Transfer-Encoding: chunked */
	if (chunked)
		filesize = -1;

	if (isunavail) {
		if (retried || retryafter != 0)
			warnx("Error retrieving %s: 503 Service Unavailable",
			    origline);
		else {
			if (verbose)
				fprintf(ttyout, "Retrying %s\n", origline);
			retried = 1;
			ftp_close(&fin, &tls, &fd);
			rval = url_get(origline, proxyenv, savefile, lastfile);
		}
		goto cleanup_url_get;
	}

	/* Open the output file.  */
	if (!pipeout) {
#ifndef SMALL
		if (resume)
			out = open(savefile, O_CREAT | O_WRONLY | O_APPEND,
				0666);
		else
#endif /* !SMALL */
			out = open(savefile, O_CREAT | O_WRONLY | O_TRUNC,
				0666);
		if (out == -1) {
			warn("Can't open %s", savefile);
			goto cleanup_url_get;
		}
	} else {
		out = fileno(stdout);
#ifdef SMALL
		if (lastfile) {
			if (pledge("stdio tty", NULL) == -1)
				err(1, "pledge");
		}
#endif
	}

	if ((buf = malloc(buflen)) == NULL)
		errx(1, "Can't allocate memory for transfer buffer");

	/* Trap signals */
	oldintr = NULL;
	oldinti = NULL;
	if (setjmp(httpabort)) {
		if (oldintr)
			(void)signal(SIGINT, oldintr);
		if (oldinti)
			(void)signal(SIGINFO, oldinti);
		goto cleanup_url_get;
	}
	oldintr = signal(SIGINT, aborthttp);

	bytes = 0;
	hashbytes = mark;
	progressmeter(-1, path);

	/* Finally, suck down the file. */
	oldinti = signal(SIGINFO, psummary);
	if (chunked) {
		error = save_chunked(fin, tls, out, buf, buflen);
		signal(SIGINT, oldintr);
		signal(SIGINFO, oldinti);
		if (error == -1)
			goto cleanup_url_get;
	} else {
		while ((len = fread(buf, 1, buflen, fin)) > 0) {
			bytes += len;
			for (cp = buf; len > 0; len -= wlen, cp += wlen) {
				if ((wlen = write(out, cp, len)) == -1) {
					warn("Writing %s", savefile);
					signal(SIGINT, oldintr);
					signal(SIGINFO, oldinti);
					goto cleanup_url_get;
				}
			}
			if (hash && !progress) {
				while (bytes >= hashbytes) {
					(void)putc('#', ttyout);
					hashbytes += mark;
				}
				(void)fflush(ttyout);
			}
		}
		save_errno = errno;
		signal(SIGINT, oldintr);
		signal(SIGINFO, oldinti);
		if (hash && !progress && bytes > 0) {
			if (bytes < mark)
				(void)putc('#', ttyout);
			(void)putc('\n', ttyout);
			(void)fflush(ttyout);
		}
		if (len == 0 && ferror(fin)) {
			errno = save_errno;
			warnx("Reading from socket: %s", sockerror(tls));
			goto cleanup_url_get;
		}
	}
	progressmeter(1, NULL);
	if (
#ifndef SMALL
		!resume &&
#endif /* !SMALL */
		filesize != -1 && len == 0 && bytes != filesize) {
		if (verbose)
			fputs("Read short file.\n", ttyout);
		goto cleanup_url_get;
	}

	if (verbose)
		ptransfer(0);

	rval = 0;
	goto cleanup_url_get;

noftpautologin:
	warnx(
	    "Auto-login using ftp URLs isn't supported when using $ftp_proxy");
	goto cleanup_url_get;

improper:
	warnx("Improper response from %s", host);

cleanup_url_get:
#ifndef SMALL
	free(full_host);
#endif /* !SMALL */
#ifndef NOSSL
	free(sslhost);
#endif /* !NOSSL */
	ftp_close(&fin, &tls, &fd);
	if (out >= 0 && out != fileno(stdout)) {
#ifndef SMALL
		if (server_timestamps && lmt.tm_zone != 0 &&
		    fstat(out, &stbuf) == 0 && S_ISREG(stbuf.st_mode) != 0) {
			ts[0].tv_nsec = UTIME_NOW;
			ts[1].tv_nsec = 0;
			setenv("TZ", lmt.tm_zone, 1);
			if (((ts[1].tv_sec = mktime(&lmt)) != -1) &&
			    (futimens(out, ts) == -1))
				warnx("Unable to set file modification time");
		}
#endif /* !SMALL */
		close(out);
	}
	free(buf);
	free(pathbuf);
	free(proxyhost);
	free(proxyurl);
	free(newline);
	free(credentials);
	free(proxy_credentials);
	return (rval);
}

static int
save_chunked(FILE *fin, struct tls *tls, int out, char *buf, size_t buflen)
{

	char			*header = NULL, *end, *cp;
	unsigned long		chunksize;
	size_t			hsize = 0, rlen, wlen;
	ssize_t			written;
	char			cr, lf;

	for (;;) {
		if (getline(&header, &hsize, fin) == -1)
			break;
		/* strip CRLF and any optional chunk extension */
		header[strcspn(header, "; \t\r\n")] = '\0';
		errno = 0;
		chunksize = strtoul(header, &end, 16);
		if (errno || header[0] == '\0' || *end != '\0' ||
		    chunksize > INT_MAX) {
			warnx("Invalid chunk size '%s'", header);
			free(header);
			return -1;
		}

		if (chunksize == 0) {
			/* We're done.  Ignore optional trailer. */
			free(header);
			return 0;
		}

		for (written = 0; chunksize != 0; chunksize -= rlen) {
			rlen = (chunksize < buflen) ? chunksize : buflen;
			rlen = fread(buf, 1, rlen, fin);
			if (rlen == 0)
				break;
			bytes += rlen;
			for (cp = buf, wlen = rlen; wlen > 0;
			    wlen -= written, cp += written) {
				if ((written = write(out, cp, wlen)) == -1) {
					warn("Writing output file");
					free(header);
					return -1;
				}
			}
		}

		if (rlen == 0 ||
		    fread(&cr, 1, 1, fin) != 1 ||
		    fread(&lf, 1, 1, fin) != 1)
			break;

		if (cr != '\r' || lf != '\n') {
			warnx("Invalid chunked encoding");
			free(header);
			return -1;
		}
	}
	free(header);

	if (ferror(fin))
		warnx("Error while reading from socket: %s", sockerror(tls));
	else
		warnx("Invalid chunked encoding: short read");

	return -1;
}

/*
 * Abort a http retrieval
 */
/* ARGSUSED */
static void
aborthttp(int signo)
{
	const char errmsg[] = "\nfetch aborted.\n";

	write(fileno(ttyout), errmsg, sizeof(errmsg) - 1);
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
auto_fetch(int argc, char *argv[], char *outfile)
{
	char *xargv[5];
	char *cp, *url, *host, *dir, *file, *portnum;
	char *username, *pass, *pathstart;
	char *ftpproxy, *httpproxy;
	int rval, xargc, lastfile;
	volatile int argpos;
	int dirhasglob, filehasglob, oautologin;
	char rempath[PATH_MAX];

	argpos = 0;

	if (setjmp(toplevel)) {
		if (connected)
			disconnect(0, NULL);
		return (argpos + 1);
	}
	(void)signal(SIGINT, (sig_t)intr);
	(void)signal(SIGPIPE, (sig_t)lostpeer);

	if ((ftpproxy = getenv(FTP_PROXY)) != NULL && *ftpproxy == '\0')
		ftpproxy = NULL;
	if ((httpproxy = getenv(HTTP_PROXY)) != NULL && *httpproxy == '\0')
		httpproxy = NULL;

	/*
	 * Loop through as long as there's files to fetch.
	 */
	username = pass = NULL;
	for (rval = 0; (rval == 0) && (argpos < argc); free(url), argpos++) {
		if (strchr(argv[argpos], ':') == NULL)
			break;

		free(username);
		free(pass);
		host = dir = file = portnum = username = pass = NULL;

		lastfile = (argv[argpos+1] == NULL);

		/*
		 * We muck with the string, so we make a copy.
		 */
		url = strdup(argv[argpos]);
		if (url == NULL)
			errx(1, "Can't allocate memory for auto-fetch.");

		if (strncasecmp(url, FILE_URL, sizeof(FILE_URL) - 1) == 0) {
			if (file_get(url + sizeof(FILE_URL) - 1, outfile) == -1)
				rval = argpos + 1;
			continue;
		}

		/*
		 * Try HTTP URL-style arguments next.
		 */
		if (strncasecmp(url, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
		    strncasecmp(url, HTTPS_URL, sizeof(HTTPS_URL) -1) == 0) {
			redirect_loop = 0;
			retried = 0;
			if (url_get(url, httpproxy, outfile, lastfile) == -1)
				rval = argpos + 1;
			continue;
		}

		/*
		 * Try FTP URL-style arguments next. If ftpproxy is
		 * set, use url_get() instead of standard ftp.
		 * Finally, try host:file.
		 */
		host = url;
		if (strncasecmp(url, FTP_URL, sizeof(FTP_URL) - 1) == 0) {
			char *passend, *passagain, *userend;

			if (ftpproxy) {
				if (url_get(url, ftpproxy, outfile, lastfile) == -1)
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
				username = host;
				pass = userend + 1;
				host = passend + 1;
				*userend = *passend = '\0';
				passagain = strchr(host, '@');
				if (strchr(pass, '@') != NULL ||
				    (passagain != NULL && passagain < dir)) {
					warnx(at_encoding_warning);
					username = pass = NULL;
					goto bad_ftp_url;
				}

				if (EMPTYSTRING(username)) {
bad_ftp_url:
					warnx("Invalid URL: %s", argv[argpos]);
					rval = argpos + 1;
					username = pass = NULL;
					continue;
				}
				username = urldecode(username);
				pass = urldecode(pass);
			}

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

			/* split off host[:port] if there is */
			if (cp) {
				portnum = strchr(cp, ':');
				pathstart = strchr(cp, '/');
				/* : in path is not a port # indicator */
				if (portnum && pathstart &&
				    pathstart < portnum)
					portnum = NULL;

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
		if (!EMPTYSTRING(dir)) {
			cp = strrchr(dir, '/');
			if (cp != NULL) {
				*cp++ = '\0';
				file = cp;
			} else {
				file = dir;
				dir = NULL;
			}
		}
#ifndef SMALL
		if (debug)
			fprintf(ttyout,
			    "user %s:%s host %s port %s dir %s file %s\n",
			    username, pass ? "XXXX" : NULL, host, portnum,
			    dir, file);
#endif /* !SMALL */

		/*
		 * Set up the connection.
		 */
		if (connected)
			disconnect(0, NULL);
		xargv[0] = __progname;
		xargv[1] = host;
		xargv[2] = NULL;
		xargc = 2;
		if (!EMPTYSTRING(portnum)) {
			xargv[2] = portnum;
			xargv[3] = NULL;
			xargc = 3;
		}
		oautologin = autologin;
		if (username == NULL)
			anonftp = 1;
		else {
			anonftp = 0;
			autologin = 0;
		}
		setpeer(xargc, xargv);
		autologin = oautologin;
		if (connected == 0 ||
		    (connected == 1 && autologin && (username == NULL ||
		    !ftp_login(host, username, pass)))) {
			warnx("Can't connect or login to host `%s'", host);
			rval = argpos + 1;
			continue;
		}

		/* Always use binary transfers. */
		setbinary(0, NULL);

		dirhasglob = filehasglob = 0;
		if (doglob) {
			if (!EMPTYSTRING(dir) &&
			    strpbrk(dir, "*?[]{}") != NULL)
				dirhasglob = 1;
			if (!EMPTYSTRING(file) &&
			    strpbrk(file, "*?[]{}") != NULL)
				filehasglob = 1;
		}

		/* Change directories, if necessary. */
		if (!EMPTYSTRING(dir) && !dirhasglob) {
			xargv[0] = "cd";
			xargv[1] = dir;
			xargv[2] = NULL;
			cd(2, xargv);
			if (!dirchange) {
				rval = argpos + 1;
				continue;
			}
		}

		if (EMPTYSTRING(file)) {
#ifndef SMALL
			rval = -1;
#else /* !SMALL */
			recvrequest("NLST", "-", NULL, "w", 0, 0);
			rval = 0;
#endif /* !SMALL */
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
#ifndef SMALL
			if (resume) {
				xargc = 3;
				xargv[1] = "-c";
				xargv[2] = file;
				xargv[3] = NULL;
			}
#endif /* !SMALL */
			mget(xargc, xargv);
			interactive = ointeractive;
		} else {
			if (outfile != NULL) {
				xargv[2] = outfile;
				xargv[3] = NULL;
				xargc++;
			}
#ifndef SMALL
			if (resume)
				reget(xargc, xargv);
			else
#endif /* !SMALL */
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
urldecode(const char *str)
{
	char *ret, c;
	int i, reallen;

	if (str == NULL)
		return NULL;
	if ((ret = malloc(strlen(str) + 1)) == NULL)
		err(1, "Can't allocate memory for URL decoding");
	for (i = 0, reallen = 0; str[i] != '\0'; i++, reallen++, ret++) {
		c = str[i];
		if (c == '+') {
			*ret = ' ';
			continue;
		}

		/* Cannot use strtol here because next char
		 * after %xx may be a digit.
		 */
		if (c == '%' && isxdigit((unsigned char)str[i + 1]) &&
		    isxdigit((unsigned char)str[i + 2])) {
			*ret = hextochar(&str[i + 1]);
			i += 2;
			continue;
		}
		*ret = c;
	}
	*ret = '\0';

	return ret - reallen;
}

static char *
recode_credentials(const char *userinfo)
{
	char *ui, *creds;
	size_t ulen, credsize;

	/* url-decode the user and pass */
	ui = urldecode(userinfo);

	ulen = strlen(ui);
	credsize = (ulen + 2) / 3 * 4 + 1;
	creds = malloc(credsize);
	if (creds == NULL)
		errx(1, "out of memory");
	if (b64_ntop(ui, ulen, creds, credsize) == -1)
		errx(1, "error in base64 encoding");
	free(ui);
	return (creds);
}

static char
hextochar(const char *str)
{
	unsigned char c, ret;

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
isurl(const char *p)
{

	if (strncasecmp(p, FTP_URL, sizeof(FTP_URL) - 1) == 0 ||
	    strncasecmp(p, HTTP_URL, sizeof(HTTP_URL) - 1) == 0 ||
#ifndef NOSSL
	    strncasecmp(p, HTTPS_URL, sizeof(HTTPS_URL) - 1) == 0 ||
#endif /* !NOSSL */
	    strncasecmp(p, FILE_URL, sizeof(FILE_URL) - 1) == 0 ||
	    strstr(p, ":/"))
		return (1);
	return (0);
}

#ifndef SMALL
static int
ftp_printf(FILE *fp, const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);
	ret = vfprintf(fp, fmt, ap);
	va_end(ap);

	if (debug) {
		va_start(ap, fmt);
		vfprintf(ttyout, fmt, ap);
		va_end(ap);
	}

	return ret;
}
#endif /* !SMALL */

static void
ftp_close(FILE **fin, struct tls **tls, int *fd)
{
#ifndef NOSSL
	int	ret;

	if (*tls != NULL) {
		if (tls_session_fd != -1)
			dprintf(STDERR_FILENO, "tls session resumed: %s\n",
			    tls_conn_session_resumed(*tls) ? "yes" : "no");
		do {
			ret = tls_close(*tls);
		} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);
		tls_free(*tls);
		*tls = NULL;
	}
	if (*fd != -1) {
		close(*fd);
		*fd = -1;
	}
#endif
	if (*fin != NULL) {
		fclose(*fin);
		*fin = NULL;
	}
}

static const char *
sockerror(struct tls *tls)
{
	int	save_errno = errno;
#ifndef NOSSL
	if (tls != NULL) {
		const char *tlserr = tls_error(tls);
		if (tlserr != NULL)
			return tlserr;
	}
#endif
	return strerror(save_errno);
}

#ifndef NOSSL
static int
proxy_connect(int socket, char *host, char *cookie)
{
	int l;
	char buf[1024];
	char *connstr, *hosttail, *port;

	if (*host == '[' && (hosttail = strrchr(host, ']')) != NULL &&
		(hosttail[1] == '\0' || hosttail[1] == ':')) {
		host++;
		*hosttail++ = '\0';
	} else
		hosttail = host;

	port = strrchr(hosttail, ':');		/* find portnum */
	if (port != NULL)
		*port++ = '\0';
	if (!port)
		port = "443";

	if (cookie) {
		l = asprintf(&connstr, "CONNECT %s:%s HTTP/1.1\r\n"
			"Proxy-Authorization: Basic %s\r\n%s\r\n\r\n",
			host, port, cookie, HTTP_USER_AGENT);
	} else {
		l = asprintf(&connstr, "CONNECT %s:%s HTTP/1.1\r\n%s\r\n\r\n",
			host, port, HTTP_USER_AGENT);
	}

	if (l == -1)
		errx(1, "Could not allocate memory to assemble connect string!");
#ifndef SMALL
	if (debug)
		printf("%s", connstr);
#endif /* !SMALL */
	if (write(socket, connstr, l) != l)
		err(1, "Could not send connect string");
	read(socket, &buf, sizeof(buf)); /* only proxy header XXX: error handling? */
	free(connstr);
	return(200);
}

static int
stdio_tls_write_wrapper(void *arg, const char *buf, int len)
{
	struct tls *tls = arg;
	ssize_t ret;

	do {
		ret = tls_write(tls, buf, len);
	} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);

	return ret;
}

static int
stdio_tls_read_wrapper(void *arg, char *buf, int len)
{
	struct tls *tls = arg;
	ssize_t ret;

	do {
		ret = tls_read(tls, buf, len);
	} while (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT);

	return ret;
}
#endif /* !NOSSL */
