/*	$OpenBSD: server_file.c,v 1.22 2014/07/31 17:55:09 reyk Exp $	*/

/*
 * Copyright (c) 2006 - 2014 Reyk Floeter <reyk@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <err.h>
#include <event.h>

#include <openssl/ssl.h>

#include "httpd.h"
#include "http.h"

int	 server_file_access(struct client *, char *, size_t,
	    struct stat *);
int	 server_file_index(struct httpd *, struct client *);

int
server_file_access(struct client *clt, char *path, size_t len,
    struct stat *st)
{
	struct http_descriptor	*desc = clt->clt_desc;
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct stat		 stb;
	char			*newpath;

	errno = 0;

	if (access(path, R_OK) == -1) {
		goto fail;
	} else if (stat(path, st) == -1) {
		goto fail;
	} else if (S_ISDIR(st->st_mode)) {
		/* Deny access if directory indexing is disabled */
		if (srv_conf->flags & SRVFLAG_NO_INDEX) {
			errno = EACCES;
			goto fail;
		}

		if (!len) {
			/* Recursion - the index "file" is a directory? */
			errno = EINVAL;
			goto fail;
		}

		/* Redirect to path with trailing "/" */
		if (path[strlen(path) - 1] != '/') {
			if (asprintf(&newpath, "http://%s%s/",
			    desc->http_host, desc->http_path) == -1)
				return (500);
			free(desc->http_path);
			desc->http_path = newpath;

			/* Indicate that the file has been moved */
			return (301);
		}

		/* Otherwise append the default index file */
		if (strlcat(path, srv_conf->index, len) >= len) {
			errno = EACCES;
			goto fail;
		}

		/* Check again but set len to 0 to avoid recursion */
		if (server_file_access(clt, path, 0, &stb) == 404) {
			/*
			 * Index file not found; fail if auto-indexing is
			 * not enabled, otherwise return success but
			 * indicate directory with S_ISDIR of the previous
			 * stat.
			 */
			if ((srv_conf->flags & SRVFLAG_AUTO_INDEX) == 0) {
				errno = EACCES;
				goto fail;
			}
		} else {
			/* return updated stat from index file */
			memcpy(st, &stb, sizeof(*st));
		}
	} else if (!S_ISREG(st->st_mode)) {
		/* Don't follow symlinks and ignore special files */
		errno = EACCES;
		goto fail;
	}

	return (0);

 fail:
	switch (errno) {
	case ENOENT:
		return (404);
	case EACCES:
		return (403);
	default:
		return (500);
	}

	/* NOTREACHED */
}

int
server_file(struct httpd *env, struct client *clt)
{
	struct http_descriptor	*desc = clt->clt_desc;
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct media_type	*media;
	const char		*errstr = NULL;
	int			 fd = -1, ret;
	char			 path[MAXPATHLEN];
	struct stat		 st;

	/* Request path is already canonicalized */
	if ((size_t)snprintf(path, sizeof(path), "%s%s",
	    srv_conf->path, desc->http_path) >= sizeof(path)) {
		/* Do not echo the uncanonicalized path */
		server_abort_http(clt, 500, desc->http_path);
		return (-1);
	}

	/* Returns HTTP status code on error */
	if ((ret = server_file_access(clt, path, sizeof(path), &st)) != 0) {
		server_abort_http(clt, ret, desc->http_path);
		return (-1);
	}

	if (S_ISDIR(st.st_mode)) {
		/* List directory index */
		return (server_file_index(env, clt));
	}

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto fail;

	media = media_find(env->sc_mediatypes, path);
	ret = server_response_http(clt, 200, media, st.st_size);
	switch (ret) {
	case -1:
		goto fail;
	case 0:
		/* Connection is already finished */
		close(fd);
		goto done;
	default:
		break;
	}

	clt->clt_fd = fd;
	if (clt->clt_srvbev != NULL)
		bufferevent_free(clt->clt_srvbev);

	clt->clt_srvbev = bufferevent_new(clt->clt_fd, server_read,
	    server_write, server_file_error, clt);
	if (clt->clt_srvbev == NULL) {
		errstr = "failed to allocate file buffer event";
		goto fail;
	}

	bufferevent_settimeout(clt->clt_srvbev,
	    srv_conf->timeout.tv_sec, srv_conf->timeout.tv_sec);
	bufferevent_enable(clt->clt_srvbev, EV_READ);
	bufferevent_disable(clt->clt_bev, EV_READ);

 done:
	server_reset_http(clt);
	return (0);
 fail:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, 500, errstr);
	return (-1);
}

int
server_file_index(struct httpd *env, struct client *clt)
{
	char			  path[MAXPATHLEN];
	char			  tmstr[21];
	struct http_descriptor	 *desc = clt->clt_desc;
	struct server_config	 *srv_conf = clt->clt_srv_conf;
	struct dirent		**namelist, *dp;
	int			  namesize, i, ret, fd = -1, namewidth, skip;
	struct evbuffer		 *evb = NULL;
	struct media_type	 *media;
	const char		 *style;
	struct stat		  st;
	struct tm		  tm;
	time_t			  t;

	/* Request path is already canonicalized */
	if ((size_t)snprintf(path, sizeof(path), "%s%s",
	    srv_conf->path, desc->http_path) >= sizeof(path))
		goto fail;

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto fail;

	if ((evb = evbuffer_new()) == NULL)
		goto fail;

	if ((namesize = scandir(path, &namelist, NULL, alphasort)) == -1)
		goto fail;

	/* Indicate failure but continue going through the list */
	skip = 0;

	/* A CSS stylesheet allows minimal customization by the user */
	style = "body { background-color: white; color: black; font-family: "
	    "sans-serif; }";
	/* Generate simple HTML index document */
	if (evbuffer_add_printf(evb,
	    "<!DOCTYPE HTML PUBLIC "
	    "\"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
	    "<html>\n"
	    "<head>\n"
	    "<title>Index of %s</title>\n"
	    "<style type=\"text/css\"><!--\n%s\n--></style>\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>Index of %s</h1>\n"
	    "<hr>\n<pre>\n",
	    desc->http_path, style, desc->http_path) == -1)
		skip = 1;

	for (i = 0; i < namesize; i++) {
		dp = namelist[i];

		if (skip ||
		    fstatat(fd, dp->d_name, &st, 0) == -1) {
			free(dp);
			continue;
		}

		t = st.st_mtime;
		localtime_r(&t, &tm);			
		strftime(tmstr, sizeof(tmstr), "%d-%h-%Y %R", &tm);
		namewidth = 51 - strlen(dp->d_name);

		if (dp->d_name[0] == '.' &&
		    !(dp->d_name[1] == '.' && dp->d_name[2] == '\0')) {
			/* ignore hidden files starting with a dot */
		} else if (dp->d_type == DT_DIR) {
			namewidth -= 1; /* trailing slash */
			if (evbuffer_add_printf(evb,
			    "<a href=\"%s\">%s/</a>%*s%s%20s\n",
			    dp->d_name, dp->d_name,
			    MAX(namewidth, 0), " ", tmstr, "-") == -1)
				skip = 1;
		} else if (dp->d_type == DT_REG) {
			if (evbuffer_add_printf(evb,
			    "<a href=\"%s\">%s</a>%*s%s%20llu\n",
			    dp->d_name, dp->d_name,
			    MAX(namewidth, 0), " ", tmstr, st.st_size) == -1)
				skip = 1;
		}
		free(dp);
	}
	free(namelist);

	if (skip ||
	    evbuffer_add_printf(evb,
	    "</pre>\n<hr>\n</body>\n</html>\n") == -1)
		goto fail;

	close(fd);

	media = media_find(env->sc_mediatypes, "index.html");
	ret = server_response_http(clt, 200, media, EVBUFFER_LENGTH(evb));
	switch (ret) {
	case -1:
		goto fail;
	case 0:
		/* Connection is already finished */
		evbuffer_free(evb);
		goto done;
	default:
		break;
	}

	if (server_bufferevent_write_buffer(clt, evb) == -1)
		goto fail;
	evbuffer_free(evb);

	bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
	if (clt->clt_persist)
		clt->clt_toread = TOREAD_HTTP_HEADER;
	else
		clt->clt_toread = TOREAD_HTTP_NONE;
	clt->clt_done = 0;

 done:
	server_reset_http(clt);
	return (0);

 fail:
	if (fd != -1)
		close(fd);
	if (evb != NULL)
		evbuffer_free(evb);
	server_abort_http(clt, 500, desc->http_path);
	return (-1);
}

void
server_file_error(struct bufferevent *bev, short error, void *arg)
{
	struct client		*clt = arg;
	struct evbuffer		*dst;

	if (error & EVBUFFER_TIMEOUT) {
		server_close(clt, "buffer event timeout");
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ);

		clt->clt_done = 1;

		dst = EVBUFFER_OUTPUT(clt->clt_bev);
		if (EVBUFFER_LENGTH(dst)) {
			/* Finish writing all data first */
			bufferevent_enable(clt->clt_bev, EV_WRITE);
			return;
		}

		if (clt->clt_persist) {
			/* Close input file and wait for next HTTP request */
			if (clt->clt_fd != -1)
				close(clt->clt_fd);
			clt->clt_fd = -1;
			clt->clt_toread = TOREAD_HTTP_HEADER;
			server_reset_http(clt);
			bufferevent_enable(clt->clt_bev, EV_READ|EV_WRITE);
			return;
		}
		server_close(clt, "done");
		return;
	}
	if (error & EVBUFFER_ERROR && errno == EFBIG) {
		bufferevent_enable(bev, EV_READ);
		return;
	}
	server_close(clt, "buffer event error");
	return;
}
