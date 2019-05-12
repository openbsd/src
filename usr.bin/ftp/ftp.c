/*	$OpenBSD: ftp.c,v 1.102 2019/05/12 20:58:19 jasper Exp $ */

/*
 * Copyright (c) 2015 Sunil Nimmagadda <sunil@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp.h"
#include "xmalloc.h"

static FILE	*ctrl_fp;
static int	 data_fd;

void
ftp_connect(struct url *url, struct url *proxy, int timeout)
{
	char		*buf = NULL;
	size_t		 n = 0;
	int		 sock;

	if (proxy) {
		http_connect(url, proxy, timeout);
		return;
	}

	if ((sock = tcp_connect(url->host, url->port, timeout)) == -1)
		exit(1);

	if ((ctrl_fp = fdopen(sock, "r+")) == NULL)
		err(1, "%s: fdopen", __func__);

	/* greeting */
	if (ftp_getline(&buf, &n, 0, ctrl_fp) != P_OK) {
		warnx("Can't connect to host `%s'", url->host);
		ftp_command(ctrl_fp, "QUIT");
		exit(1);
	}

	free(buf);
	log_info("Connected to %s\n", url->host);
	if (ftp_auth(ctrl_fp, NULL, NULL) != P_OK) {
		warnx("Can't login to host `%s'", url->host);
		ftp_command(ctrl_fp, "QUIT");
		exit(1);
	}
}

struct url *
ftp_get(struct url *url, struct url *proxy, off_t *offset, off_t *sz)
{
	char	*buf = NULL, *dir, *file;

	if (proxy) {
		url = http_get(url, proxy, offset, sz);
		/* this url should now be treated as HTTP */
		url->scheme = S_HTTP;
		return url;
	}

	log_info("Using binary mode to transfer files.\n");
	if (ftp_command(ctrl_fp, "TYPE I") != P_OK)
		errx(1, "Failed to set mode to binary");

	dir = dirname(url->path);
	if (ftp_command(ctrl_fp, "CWD %s", dir) != P_OK)
		errx(1, "CWD command failed");

	log_info("Retrieving %s\n", url->path);
	file = basename(url->path);
	if (strcmp(url->fname, "-"))
		log_info("local: %s remote: %s\n", url->fname, file);
	else
		log_info("remote: %s\n", file);

	if (ftp_size(ctrl_fp, file, sz, &buf) != P_OK) {
		fprintf(stderr, "%s", buf);
		ftp_command(ctrl_fp, "QUIT");
		exit(1);
	}
	free(buf);

	if (activemode)
		data_fd = ftp_eprt(ctrl_fp);
	else if ((data_fd = ftp_epsv(ctrl_fp)) == -1)
		data_fd = ftp_eprt(ctrl_fp);

	if (data_fd == -1)
		errx(1, "Failed to establish data connection");

	if (*offset && ftp_command(ctrl_fp, "REST %lld", *offset) != P_INTER)
		errx(1, "REST command failed");

	if (ftp_command(ctrl_fp, "RETR %s", file) != P_PRE) {
		ftp_command(ctrl_fp, "QUIT");
		exit(1);
	}

	return url;
}

void
ftp_save(struct url *url, FILE *dst_fp, off_t *offset)
{
	struct sockaddr_storage	 ss;
	FILE			*data_fp;
	socklen_t		 len;
	int			 s;

	if (activemode) {
		len = sizeof(ss);
		if ((s = accept(data_fd, (struct sockaddr *)&ss, &len)) == -1)
			err(1, "%s: accept", __func__);

		close(data_fd);
		data_fd = s;
	}

	if ((data_fp = fdopen(data_fd, "r")) == NULL)
		err(1, "%s: fdopen data_fd", __func__);

	copy_file(dst_fp, data_fp, offset);
	fclose(data_fp);
}

void
ftp_quit(struct url *url)
{
	char	*buf = NULL;
	size_t	 n = 0;

	if (ftp_getline(&buf, &n, 0, ctrl_fp) != P_OK)
		errx(1, "error retrieving file %s", url->fname);

	free(buf);
	ftp_command(ctrl_fp, "QUIT");
	fclose(ctrl_fp);
}

int
ftp_getline(char **lineptr, size_t *n, int suppress_output, FILE *fp)
{
	ssize_t		 len;
	char		*bufp, code[4];
	const char	*errstr;
	int		 lookup[] = { P_PRE, P_OK, P_INTER, N_TRANS, N_PERM };


	if ((len = getline(lineptr, n, fp)) == -1)
		err(1, "%s: getline", __func__);

	bufp = *lineptr;
	if (!suppress_output)
		log_info("%s", bufp);

	if (len < 4)
		errx(1, "%s: line too short", __func__);

	(void)strlcpy(code, bufp, sizeof code);
	if (bufp[3] == ' ')
		goto done;

	/* multi-line reply */
	while (!(strncmp(code, bufp, 3) == 0 && bufp[3] == ' ')) {
		if ((len = getline(lineptr, n, fp)) == -1)
			err(1, "%s: getline", __func__);

		bufp = *lineptr;
		if (!suppress_output)
			log_info("%s", bufp);

		if (len < 4)
			continue;
	}

 done:
	(void)strtonum(code, 100, 553, &errstr);
	if (errstr)
		errx(1, "%s: Response code is %s: %s", __func__, errstr, code);

	return lookup[code[0] - '1'];
}

int
ftp_command(FILE *fp, const char *fmt, ...)
{
	va_list	 ap;
	char	*buf = NULL, *cmd;
	size_t	 n = 0;
	int	 r;

	va_start(ap, fmt);
	r = vasprintf(&cmd, fmt, ap);
	va_end(ap);
	if (r < 0)
		errx(1, "%s: vasprintf", __func__);

	if (io_debug)
		fprintf(stderr, ">>> %s\n", cmd);

	if (fprintf(fp, "%s\r\n", cmd) < 0)
		errx(1, "%s: fprintf", __func__);

	(void)fflush(fp);
	free(cmd);
	r = ftp_getline(&buf, &n, 0, fp);
	free(buf);
	return r;

}

int
ftp_auth(FILE *fp, const char *user, const char *pass)
{
	char	*addr = NULL, hn[HOST_NAME_MAX+1], *un;
	int	 code;

	code = ftp_command(fp, "USER %s", user ? user : "anonymous");
	if (code != P_OK && code != P_INTER)
		return code;

	if (pass == NULL) {
		if (gethostname(hn, sizeof hn) == -1)
			err(1, "%s: gethostname", __func__);

		un = getlogin();
		xasprintf(&addr, "%s@%s", un ? un : "anonymous", hn);
	}

	code = ftp_command(fp, "PASS %s", pass ? pass : addr);
	free(addr);
	return code;
}

int
ftp_size(FILE *fp, const char *fn, off_t *sizep, char **buf)
{
	size_t	 n = 0;
	off_t	 file_sz;
	int	 code;

	if (io_debug)
		fprintf(stderr, ">>> SIZE %s\n", fn);

	if (fprintf(fp, "SIZE %s\r\n", fn) < 0)
		errx(1, "%s: fprintf", __func__);

	(void)fflush(fp);
	if ((code = ftp_getline(buf, &n, 1, fp)) != P_OK)
		return code;

	if (sscanf(*buf, "%*u %lld", &file_sz) != 1)
		errx(1, "%s: sscanf size", __func__);

	if (sizep)
		*sizep = file_sz;

	return code;
}

int
ftp_eprt(FILE *fp)
{
	struct sockaddr_storage	 ss;
	char			 addr[NI_MAXHOST], port[NI_MAXSERV], *eprt;
	socklen_t		 len;
	int			 e, on, ret, sock;

	len = sizeof(ss);
	memset(&ss, 0, len);
	if (getsockname(fileno(fp), (struct sockaddr *)&ss, &len) == -1) {
		warn("%s: getsockname", __func__);
		return -1;
	}

	/* pick a free port */
	switch (ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&ss)->sin_port = 0;
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&ss)->sin6_port = 0;
		break;
	default:
		errx(1, "%s: Invalid socket family", __func__);
	}

	if ((sock = socket(ss.ss_family, SOCK_STREAM, 0)) == -1) {
		warn("%s: socket", __func__);
		return -1;
	}

	switch (ss.ss_family) {
	case AF_INET:
		on = IP_PORTRANGE_HIGH;
		if (setsockopt(sock, IPPROTO_IP, IP_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			warn("setsockopt IP_PORTRANGE (ignored)");
		break;
	case AF_INET6:
		on = IPV6_PORTRANGE_HIGH;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_PORTRANGE,
		    (char *)&on, sizeof(on)) < 0)
			warn("setsockopt IPV6_PORTRANGE (ignored)");
		break;
	}

	if (bind(sock, (struct sockaddr *)&ss, len) == -1) {
		close(sock);
		warn("%s: bind", __func__);
		return -1;
	}

	if (listen(sock, 1) == -1) {
		close(sock);
		warn("%s: listen", __func__);
		return -1;
	}

	/* Find out the ephermal port chosen */
	len = sizeof(ss);
	memset(&ss, 0, len);
	if (getsockname(sock, (struct sockaddr *)&ss, &len) == -1) {
		close(sock);
		warn("%s: getsockname", __func__);
		return -1;
	}

	if ((e = getnameinfo((struct sockaddr *)&ss, len,
	    addr, sizeof(addr), port, sizeof(port),
	    NI_NUMERICHOST | NI_NUMERICSERV)) != 0) {
		close(sock);
		warn("%s: getnameinfo: %s", __func__, gai_strerror(e));
		return -1;
	}

	xasprintf(&eprt, "EPRT |%d|%s|%s|",
	    ss.ss_family == AF_INET ? 1 : 2, addr, port);

	ret = ftp_command(fp, "%s", eprt);
	free(eprt);
	if (ret != P_OK) {
		close(sock);
		return -1;
	}

	return sock;
}

int
ftp_epsv(FILE *fp)
{
	struct sockaddr_storage	 ss;
	char			*buf = NULL, delim[4], *s, *e;
	size_t			 n = 0;
	socklen_t		 len;
	int			 error, port, sock;

	if (io_debug)
		fprintf(stderr, ">>> EPSV\n");

	if (fprintf(fp, "EPSV\r\n") < 0)
		errx(1, "%s: fprintf", __func__);

	(void)fflush(fp);
	if (ftp_getline(&buf, &n, 1, fp) != P_OK) {
		free(buf);
		return -1;
	}

	if ((s = strchr(buf, '(')) == NULL || (e = strchr(s, ')')) == NULL) {
		warnx("Malformed EPSV reply");
		free(buf);
		return -1;
	}

	s++;
	*e = '\0';
	if (sscanf(s, "%c%c%c%d%c", &delim[0], &delim[1], &delim[2],
	    &port, &delim[3]) != 5) {
		warnx("EPSV parse error");
		free(buf);
		return -1;
	}
	free(buf);

	if (delim[0] != delim[1] || delim[0] != delim[2]
	    || delim[0] != delim[3]) {
		warnx("EPSV parse error");
		return -1;
	}

	len = sizeof(ss);
	memset(&ss, 0, len);
	if (getpeername(fileno(fp), (struct sockaddr *)&ss, &len) == -1) {
		warn("%s: getpeername", __func__);
		return -1;
	}

	switch (ss.ss_family) {
	case AF_INET:
		((struct sockaddr_in *)&ss)->sin_port = htons(port);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)&ss)->sin6_port = htons(port);
		break;
	default:
		errx(1, "%s: Invalid socket family", __func__);
	}

	if ((sock = socket(ss.ss_family, SOCK_STREAM, 0)) == -1) {
		warn("%s: socket", __func__);
		return -1;
	}

	for (error = connect(sock, (struct sockaddr *)&ss, len);
	     error != 0 && errno == EINTR; error = connect_wait(sock))
		continue;

	if (error != 0) {
		warn("%s: connect", __func__);
		return -1;
	}

	return sock;
}
