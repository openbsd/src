/*	$OpenBSD: server_file.c,v 1.59 2015/07/20 11:38:19 semarie Exp $	*/

/*
 * Copyright (c) 2006 - 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/time.h>
#include <sys/stat.h>

#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <event.h>

#include "httpd.h"
#include "http.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))
#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))
#define MAX_RANGES	4

struct range {
	off_t	start;
	off_t	end;
};

int		 server_file_access(struct httpd *, struct client *,
		    char *, size_t);
int		 server_file_request(struct httpd *, struct client *,
		    char *, struct stat *);
int		 server_partial_file_request(struct httpd *, struct client *,
		    char *, struct stat *, char *);
int		 server_file_index(struct httpd *, struct client *,
		    struct stat *);
int		 server_file_modified_since(struct http_descriptor *,
		    struct stat *);
int		 server_file_method(struct client *);
int		 parse_range_spec(char *, size_t, struct range *);
struct range	*parse_range(char *, size_t, int *);
int		 buffer_add_range(int, struct evbuffer *, struct range *);

int
server_file_access(struct httpd *env, struct client *clt,
    char *path, size_t len)
{
	struct http_descriptor	*desc = clt->clt_descreq;
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct stat		 st;
	struct kv		*r, key;
	char			*newpath, *encodedpath;
	int			 ret;

	errno = 0;

	if (access(path, R_OK) == -1) {
		goto fail;
	} else if (stat(path, &st) == -1) {
		goto fail;
	} else if (S_ISDIR(st.st_mode)) {
		/* Deny access if directory indexing is disabled */
		if (srv_conf->flags & SRVFLAG_NO_INDEX) {
			errno = EACCES;
			goto fail;
		}

		if (desc->http_path_alias != NULL) {
			/* Recursion - the index "file" is a directory? */
			errno = EINVAL;
			goto fail;
		}

		/* Redirect to path with trailing "/" */
		if (path[strlen(path) - 1] != '/') {
			if ((encodedpath = url_encode(desc->http_path)) == NULL)
				return (500);
			if (asprintf(&newpath, "http%s://%s%s/",
			    srv_conf->flags & SRVFLAG_TLS ? "s" : "",
			    desc->http_host, encodedpath) == -1) {
				free(encodedpath);
				return (500);
			}
			free(encodedpath);

			/* Path alias will be used for the redirection */
			desc->http_path_alias = newpath;

			/* Indicate that the file has been moved */
			return (301);
		}

		/* Append the default index file to the location */
		if (asprintf(&newpath, "%s%s", desc->http_path,
		    srv_conf->index) == -1)
			return (500);
		desc->http_path_alias = newpath;
		if (server_getlocation(clt, newpath) != srv_conf) {
			/* The location has changed */
			return (server_file(env, clt));
		}

		/* Otherwise append the default index file to the path */
		if (strlcat(path, srv_conf->index, len) >= len) {
			errno = EACCES;
			goto fail;
		}

		ret = server_file_access(env, clt, path, len);
		if (ret == 404) {
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

			return (server_file_index(env, clt, &st));
		}
		return (ret);
	} else if (!S_ISREG(st.st_mode)) {
		/* Don't follow symlinks and ignore special files */
		errno = EACCES;
		goto fail;
	}

	key.kv_key = "Range";
	r = kv_find(&desc->http_headers, &key);
	if (r != NULL)
		return (server_partial_file_request(env, clt, path, &st,
		    r->kv_value));
	else
		return (server_file_request(env, clt, path, &st));

 fail:
	switch (errno) {
	case ENOENT:
	case ENOTDIR:
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
	struct http_descriptor	*desc = clt->clt_descreq;
	struct server_config	*srv_conf = clt->clt_srv_conf;
	char			 path[PATH_MAX];
	const char		*stripped, *errstr = NULL;
	int			 ret = 500;

	if (srv_conf->flags & SRVFLAG_FCGI)
		return (server_fcgi(env, clt));

	/* Request path is already canonicalized */
	stripped = server_root_strip(
	    desc->http_path_alias != NULL ?
	    desc->http_path_alias : desc->http_path,
	    srv_conf->strip);
	if ((size_t)snprintf(path, sizeof(path), "%s%s",
	    srv_conf->root, stripped) >= sizeof(path)) {
		errstr = desc->http_path;
		goto abort;
	}

	/* Returns HTTP status code on error */
	if ((ret = server_file_access(env, clt, path, sizeof(path))) > 0) {
		errstr = desc->http_path_alias != NULL ?
		    desc->http_path_alias : desc->http_path;
		goto abort;
	}

	return (ret);

 abort:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, ret, errstr);
	return (-1);
}

int
server_file_method(struct client *clt)
{
	struct http_descriptor	*desc = clt->clt_descreq;

	switch (desc->http_method) {
	case HTTP_METHOD_GET:
	case HTTP_METHOD_HEAD:
		return (0);
	default:
		/* Other methods are not allowed */
		errno = EACCES;
		return (405);
	}
	/* NOTREACHED */
}

int
server_file_request(struct httpd *env, struct client *clt, char *path,
    struct stat *st)
{
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct media_type	*media;
	const char		*errstr = NULL;
	int			 fd = -1, ret, code = 500;

	if ((ret = server_file_method(clt)) != 0) {
		code = ret;
		goto abort;
	}

	if ((ret = server_file_modified_since(clt->clt_descreq, st)) != -1)
		return (ret);

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto abort;

	media = media_find_config(env, srv_conf, path);
	ret = server_response_http(clt, 200, media, st->st_size,
	    MINIMUM(time(NULL), st->st_mtim.tv_sec));
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

	/* Adjust read watermark to the socket output buffer size */
	bufferevent_setwatermark(clt->clt_srvbev, EV_READ, 0,
	    clt->clt_sndbufsiz);

	bufferevent_settimeout(clt->clt_srvbev,
	    srv_conf->timeout.tv_sec, srv_conf->timeout.tv_sec);
	bufferevent_enable(clt->clt_srvbev, EV_READ);
	bufferevent_disable(clt->clt_bev, EV_READ);

 done:
	server_reset_http(clt);
	return (0);
 fail:
	bufferevent_disable(clt->clt_bev, EV_READ|EV_WRITE);
	bufferevent_free(clt->clt_bev);
	clt->clt_bev = NULL;
 abort:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, code, errstr);
	return (-1);
}

int
server_partial_file_request(struct httpd *env, struct client *clt, char *path,
    struct stat *st, char *range_str)
{
	struct server_config	*srv_conf = clt->clt_srv_conf;
	struct http_descriptor	*resp = clt->clt_descresp;
	struct http_descriptor	*desc = clt->clt_descreq;
	struct media_type	*media, multipart_media;
	struct range		*range;
	struct evbuffer		*evb = NULL;
	size_t			 content_length;
	int			 code = 500, fd = -1, i, nranges, ret;
	uint32_t		 boundary;
	char			 content_range[64];
	const char		*errstr = NULL;

	/* Ignore range request for methods other than GET */
	if (desc->http_method != HTTP_METHOD_GET)
		return server_file_request(env, clt, path, st);

	if ((range = parse_range(range_str, st->st_size, &nranges)) == NULL) {
		code = 416;
		(void)snprintf(content_range, sizeof(content_range),
		    "bytes */%lld", st->st_size);
		errstr = content_range;
		goto abort;
	}

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto abort;

	media = media_find_config(env, srv_conf, path);
	if ((evb = evbuffer_new()) == NULL) {
		errstr = "failed to allocate file buffer";
		goto abort;
	}

	if (nranges == 1) {
		(void)snprintf(content_range, sizeof(content_range),
		    "bytes %lld-%lld/%lld", range->start, range->end,
		    st->st_size);
		if (kv_add(&resp->http_headers, "Content-Range",
		    content_range) == NULL)
			goto abort;

		content_length = range->end - range->start + 1;
		if (buffer_add_range(fd, evb, range) == 0)
			goto abort;

	} else {
		content_length = 0;
		boundary = arc4random();
		/* Generate a multipart payload of byteranges */
		while (nranges--) {
			if ((i = evbuffer_add_printf(evb, "\r\n--%ud\r\n",
			    boundary)) == -1)
				goto abort;

			content_length += i;
			if ((i = evbuffer_add_printf(evb,
			    "Content-Type: %s/%s\r\n",
			    media->media_type, media->media_subtype)) == -1)
				goto abort;

			content_length += i;
			if ((i = evbuffer_add_printf(evb,
			    "Content-Range: bytes %lld-%lld/%lld\r\n\r\n",
			    range->start, range->end, st->st_size)) == -1)
				goto abort;

			content_length += i;
			if (buffer_add_range(fd, evb, range) == 0)
				goto abort;

			content_length += range->end - range->start + 1;
			range++;
		}

		if ((i = evbuffer_add_printf(evb, "\r\n--%ud--\r\n",
		    boundary)) == -1)
			goto abort;

		content_length += i;

		/* prepare multipart/byteranges media type */
		(void)strlcpy(multipart_media.media_type, "multipart",
		    sizeof(multipart_media.media_type));
		(void)snprintf(multipart_media.media_subtype,
		    sizeof(multipart_media.media_subtype),
		    "byteranges; boundary=%ud", boundary);
		media = &multipart_media;
	}

	ret = server_response_http(clt, 206, media, content_length,
	    MINIMUM(time(NULL), st->st_mtim.tv_sec));
	switch (ret) {
	case -1:
		goto fail;
	case 0:
		/* Connection is already finished */
		close(fd);
		evbuffer_free(evb);
		evb = NULL;
		goto done;
	default:
		break;
	}

	if (server_bufferevent_write_buffer(clt, evb) == -1)
		goto fail;

	evbuffer_free(evb);
	evb = NULL;

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
	bufferevent_disable(clt->clt_bev, EV_READ|EV_WRITE);
	bufferevent_free(clt->clt_bev);
	clt->clt_bev = NULL;
 abort:
	if (errstr == NULL)
		errstr = strerror(errno);
	server_abort_http(clt, code, errstr);
	return (-1);
}

int
server_file_index(struct httpd *env, struct client *clt, struct stat *st)
{
	char			  path[PATH_MAX];
	char			  tmstr[21];
	struct http_descriptor	 *desc = clt->clt_descreq;
	struct server_config	 *srv_conf = clt->clt_srv_conf;
	struct dirent		**namelist, *dp;
	int			  namesize, i, ret, fd = -1, namewidth, skip;
	int			  code = 500;
	struct evbuffer		 *evb = NULL;
	struct media_type	 *media;
	const char		 *stripped, *style;
	char			 *escapeduri, *escapedhtml, *escapedpath;
	struct tm		  tm;
	time_t			  t, dir_mtime;

	if ((ret = server_file_method(clt)) != 0) {
		code = ret;
		goto abort;
	}

	/* Request path is already canonicalized */
	stripped = server_root_strip(desc->http_path, srv_conf->strip);
	if ((size_t)snprintf(path, sizeof(path), "%s%s",
	    srv_conf->root, stripped) >= sizeof(path))
		goto abort;

	/* Now open the file, should be readable or we have another problem */
	if ((fd = open(path, O_RDONLY)) == -1)
		goto abort;

	/* Save last modification time */
	dir_mtime = MINIMUM(time(NULL), st->st_mtim.tv_sec);

	if ((evb = evbuffer_new()) == NULL)
		goto abort;

	if ((namesize = scandir(path, &namelist, NULL, alphasort)) == -1)
		goto abort;

	/* Indicate failure but continue going through the list */
	skip = 0;

	if ((escapedpath = escape_html(desc->http_path)) == NULL)
		goto fail;

	/* A CSS stylesheet allows minimal customization by the user */
	style = "body { background-color: white; color: black; font-family: "
	    "sans-serif; }\nhr { border: 0; border-bottom: 1px dashed; }\n";
	/* Generate simple HTML index document */
	if (evbuffer_add_printf(evb,
	    "<!DOCTYPE html>\n"
	    "<html>\n"
	    "<head>\n"
	    "<title>Index of %s</title>\n"
	    "<style type=\"text/css\"><!--\n%s\n--></style>\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>Index of %s</h1>\n"
	    "<hr>\n<pre>\n",
	    escapedpath, style, escapedpath) == -1)
		skip = 1;

	free(escapedpath);

	for (i = 0; i < namesize; i++) {
		dp = namelist[i];

		if (skip ||
		    fstatat(fd, dp->d_name, st, 0) == -1) {
			free(dp);
			continue;
		}

		t = st->st_mtime;
		localtime_r(&t, &tm);
		strftime(tmstr, sizeof(tmstr), "%d-%h-%Y %R", &tm);
		namewidth = 51 - strlen(dp->d_name);

		if ((escapeduri = url_encode(dp->d_name)) == NULL)
			goto fail;
		if ((escapedhtml = escape_html(dp->d_name)) == NULL)
			goto fail;

		if (dp->d_name[0] == '.' &&
		    !(dp->d_name[1] == '.' && dp->d_name[2] == '\0')) {
			/* ignore hidden files starting with a dot */
		} else if (S_ISDIR(st->st_mode)) {
			namewidth -= 1; /* trailing slash */
			if (evbuffer_add_printf(evb,
			    "<a href=\"%s%s/\">%s/</a>%*s%s%20s\n",
			    strchr(escapeduri, ':') != NULL ? "./" : "",
			    escapeduri, escapedhtml,
			    MAXIMUM(namewidth, 0), " ", tmstr, "-") == -1)
				skip = 1;
		} else if (S_ISREG(st->st_mode)) {
			if (evbuffer_add_printf(evb,
			    "<a href=\"%s%s\">%s</a>%*s%s%20llu\n",
			    strchr(escapeduri, ':') != NULL ? "./" : "",
			    escapeduri, escapedhtml,
			    MAXIMUM(namewidth, 0), " ",
			    tmstr, st->st_size) == -1)
				skip = 1;
		}
		free(escapeduri);
		free(escapedhtml);
		free(dp);
	}
	free(namelist);

	if (skip ||
	    evbuffer_add_printf(evb,
	    "</pre>\n<hr>\n</body>\n</html>\n") == -1)
		goto abort;

	close(fd);
	fd = -1;

	media = media_find_config(env, srv_conf, "index.html");
	ret = server_response_http(clt, 200, media, EVBUFFER_LENGTH(evb),
	    dir_mtime);
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
	evb = NULL;

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
	bufferevent_disable(clt->clt_bev, EV_READ|EV_WRITE);
	bufferevent_free(clt->clt_bev);
	clt->clt_bev = NULL;
 abort:
	if (fd != -1)
		close(fd);
	if (evb != NULL)
		evbuffer_free(evb);
	server_abort_http(clt, code, desc->http_path);
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
	if (error & EVBUFFER_ERROR) {
		if (errno == EFBIG) {
			bufferevent_enable(bev, EV_READ);
			return;
		}
		server_close(clt, "buffer event error");
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		clt->clt_done = 1;

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

		dst = EVBUFFER_OUTPUT(clt->clt_bev);
		if (EVBUFFER_LENGTH(dst)) {
			/* Finish writing all data first */
			bufferevent_enable(clt->clt_bev, EV_WRITE);
			return;
		}

		server_close(clt, "done");
		return;
	}
	server_close(clt, "unknown event error");
	return;
}

int
server_file_modified_since(struct http_descriptor *desc, struct stat *st)
{
	struct kv	 key, *since;
	struct tm	 tm;

	key.kv_key = "If-Modified-Since";
	if ((since = kv_find(&desc->http_headers, &key)) != NULL &&
	    since->kv_value != NULL) {
		memset(&tm, 0, sizeof(struct tm));

		/*
		 * Return "Not modified" if the file hasn't changed since
		 * the requested time.
		 */
		if (strptime(since->kv_value,
		    "%a, %d %h %Y %T %Z", &tm) != NULL &&
		    timegm(&tm) >= st->st_mtim.tv_sec)
			return (304);
	}

	return (-1);
}

struct range *
parse_range(char *str, size_t file_sz, int *nranges)
{
	static struct range	 ranges[MAX_RANGES];
	int			 i = 0;
	char			*p, *q;

	/* Extract range unit */
	if ((p = strchr(str, '=')) == NULL)
		return (NULL);

	*p++ = '\0';
	/* Check if it's a bytes range spec */
	if (strcmp(str, "bytes") != 0)
		return (NULL);

	while ((q = strchr(p, ',')) != NULL) {
		*q++ = '\0';

		/* Extract start and end positions */
		if (parse_range_spec(p, file_sz, &ranges[i]) == 0)
			continue;

		i++;
		if (i == MAX_RANGES)
			return (NULL);

		p = q;
	}

	if (parse_range_spec(p, file_sz, &ranges[i]) != 0)
		i++;

	*nranges = i;
	return (i ? ranges : NULL);
}

int
parse_range_spec(char *str, size_t size, struct range *r)
{
	size_t		 start_str_len, end_str_len;
	char		*p, *start_str, *end_str;
	const char	*errstr;

	if ((p = strchr(str, '-')) == NULL)
		return (0);

	*p++ = '\0';
	start_str = str;
	end_str = p;
	start_str_len = strlen(start_str);
	end_str_len = strlen(end_str);

	/* Either 'start' or 'end' is optional but not both */
	if ((start_str_len == 0) && (end_str_len == 0))
		return (0);

	if (end_str_len) {
		r->end = strtonum(end_str, 0, LLONG_MAX, &errstr);
		if (errstr)
			return (0);

		if ((size_t)r->end >= size)
			r->end = size - 1;
	} else
		r->end = size - 1;

	if (start_str_len) {
		r->start = strtonum(start_str, 0, LLONG_MAX, &errstr);
		if (errstr)
			return (0);

		if ((size_t)r->start >= size)
			return (0);
	} else {
		r->start = size - r->end;
		r->end = size - 1;
	}

	if (r->end < r->start)
		return (0);

	return (1);
}

int
buffer_add_range(int fd, struct evbuffer *evb, struct range *range)
{
	char	buf[BUFSIZ];
	size_t	n, range_sz;
	ssize_t	nread;

	if (lseek(fd, range->start, SEEK_SET) == -1)
		return (0);

	range_sz = range->end - range->start + 1;
	while (range_sz) {
		n = MINIMUM(range_sz, sizeof(buf));
		if ((nread = read(fd, buf, n)) == -1)
			return (0);

		evbuffer_add(evb, buf, nread);
		range_sz -= nread;
	}

	return (1);
}
