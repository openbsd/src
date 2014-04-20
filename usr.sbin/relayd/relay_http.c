/*	$OpenBSD: relay_http.c,v 1.18 2014/04/20 16:18:32 reyk Exp $	*/

/*
 * Copyright (c) 2006 - 2012 Reyk Floeter <reyk@openbsd.org>
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
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <fnmatch.h>

#include <openssl/ssl.h>

#include "relayd.h"

int		 relay_resolve(struct ctl_relay_event *,
		    struct protonode *, struct protonode *);
int		 relay_handle_http(struct ctl_relay_event *,
		    struct protonode *, struct protonode *,
		    struct protonode *, int);
static int	_relay_lookup_url(struct ctl_relay_event *, char *, char *,
		    char *, enum digest_type);
int		 relay_lookup_url(struct ctl_relay_event *,
		    const char *, enum digest_type);
int		 relay_lookup_query(struct ctl_relay_event *);
int		 relay_lookup_cookie(struct ctl_relay_event *, const char *);
void		 relay_read_httpcontent(struct bufferevent *, void *);
void		 relay_read_httpchunks(struct bufferevent *, void *);
char		*relay_expand_http(struct ctl_relay_event *, char *,
		    char *, size_t);
void		 relay_http_request_close(struct ctl_relay_event *);

void
relay_read_http(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = arg;
	struct rsession		*con = cre->con;
	struct relay		*rlay = con->se_relay;
	struct protocol		*proto = rlay->rl_proto;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	struct protonode	*pn, pk, *proot, *pnv = NULL, pkv;
	char			*line;
	int			 header = 0, ret, pass = 0;
	const char		*errstr;
	size_t			 size;

	getmonotime(&con->se_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: size %lu, to read %lld", __func__, size, cre->toread);
	if (!size) {
		if (cre->dir == RELAY_DIR_RESPONSE)
			return;
		cre->toread = TOREAD_HTTP_HEADER;
		goto done;
	}

	pk.type = NODE_TYPE_HEADER;

	while (!cre->done && (line = evbuffer_readline(src)) != NULL) {
		/*
		 * An empty line indicates the end of the request.
		 * libevent already stripped the \r\n for us.
		 */
		if (!strlen(line)) {
			cre->done = 1;
			free(line);
			break;
		}
		pk.key = line;

		/*
		 * The first line is the GET/POST/PUT/... request,
		 * subsequent lines are HTTP headers.
		 */
		if (++cre->line == 1) {
			pk.value = strchr(pk.key, ' ');
		} else
			pk.value = strchr(pk.key, ':');
		if (pk.value == NULL || strlen(pk.value) < 3) {
			if (cre->line == 1) {
				free(line);
				relay_abort_http(con, 400, "malformed", 0);
				return;
			}

			DPRINTF("%s: request '%s'", __func__, line);
			/* Append line to the output buffer */
			if (relay_bufferevent_print(cre->dst, line) == -1 ||
			    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
				free(line);
				goto fail;
			}
			free(line);
			continue;
		}
		if (*pk.value == ':') {
			*pk.value++ = '\0';
			pk.value += strspn(pk.value, " \t\r\n");
			header = 1;
		} else {
			*pk.value++ = '\0';
			header = 0;
		}

		DPRINTF("%s: header '%s: %s'", __func__, pk.key, pk.value);

		/*
		 * Identify and handle specific HTTP request methods
		 */
		if (cre->line == 1) {
			if (cre->dir == RELAY_DIR_RESPONSE) {
				cre->method = HTTP_METHOD_RESPONSE;
				goto lookup;
			} else if (strcmp("HEAD", pk.key) == 0)
				cre->method = HTTP_METHOD_HEAD;
			else if (strcmp("POST", pk.key) == 0)
				cre->method = HTTP_METHOD_POST;
			else if (strcmp("PUT", pk.key) == 0)
				cre->method = HTTP_METHOD_PUT;
			else if (strcmp("DELETE", pk.key) == 0)
				cre->method = HTTP_METHOD_DELETE;
			else if (strcmp("OPTIONS", pk.key) == 0)
				cre->method = HTTP_METHOD_OPTIONS;
			else if (strcmp("TRACE", pk.key) == 0)
				cre->method = HTTP_METHOD_TRACE;
			else if (strcmp("CONNECT", pk.key) == 0)
				cre->method = HTTP_METHOD_CONNECT;
			else {
				/* Use GET method as the default */
				cre->method = HTTP_METHOD_GET;
			}

			/*
			 * Decode the path and query
			 */
			cre->path = strdup(pk.value);
			if (cre->path == NULL) {
				free(line);
				goto fail;
			}
			cre->version = strchr(cre->path, ' ');
			if (cre->version != NULL)
				*cre->version++ = '\0';
			cre->args = strchr(cre->path, '?');
			if (cre->args != NULL)
				*cre->args++ = '\0';
#ifdef DEBUG
			char	 buf[BUFSIZ];
			if (snprintf(buf, sizeof(buf), " \"%s\"",
			    cre->path) == -1 ||
			    evbuffer_add(con->se_log, buf, strlen(buf)) == -1) {
				free(line);
				goto fail;
			}
#endif

			/*
			 * Lookup protocol handlers in the URL path
			 */
			if ((proto->flags & F_LOOKUP_PATH) == 0)
				goto lookup;

			pkv.key = cre->path;
			pkv.type = NODE_TYPE_PATH;
			pkv.value = cre->args == NULL ? "" : cre->args;

			DPRINTF("%s: lookup path '%s: %s'",
			    __func__, pkv.key, pkv.value);

			if ((proot = RB_FIND(proto_tree,
			    cre->tree, &pkv)) == NULL)
				goto lookup;

			PROTONODE_FOREACH(pnv, proot, entry) {
				ret = relay_handle_http(cre, proot,
				    pnv, &pkv, 0);
				if (ret == PN_FAIL)
					goto abort;
			}
		} else if ((cre->method == HTTP_METHOD_DELETE ||
		    cre->method == HTTP_METHOD_GET ||
		    cre->method == HTTP_METHOD_HEAD ||
		    cre->method == HTTP_METHOD_OPTIONS ||
		    cre->method == HTTP_METHOD_POST ||
		    cre->method == HTTP_METHOD_PUT ||
		    cre->method == HTTP_METHOD_RESPONSE) &&
		    strcasecmp("Content-Length", pk.key) == 0) {
			/*
			 * Need to read data from the client after the
			 * HTTP header.
			 * XXX What about non-standard clients not using
			 * the carriage return? And some browsers seem to
			 * include the line length in the content-length.
			 */
			cre->toread = strtonum(pk.value, 0, LLONG_MAX,
			    &errstr);
			if (errstr) {
				relay_abort_http(con, 500, errstr, 0);
				goto abort;
			}
		} else if ((cre->method == HTTP_METHOD_TRACE) &&
		    strcasecmp("Content-Length", pk.key) == 0) {
			/*
			 * This method should not have a body and thus no
			 * Content-Length header.
			 */
			relay_abort_http(con, 400, "malformed", 0);
			goto abort;
		}
 lookup:
		if (strcasecmp("Transfer-Encoding", pk.key) == 0 &&
		    strcasecmp("chunked", pk.value) == 0)
			cre->chunked = 1;

		/* Match the HTTP header */
		if ((pn = RB_FIND(proto_tree, cre->tree, &pk)) == NULL)
			goto next;

		if (cre->dir == RELAY_DIR_RESPONSE)
			goto handle;

		if (pn->flags & PNFLAG_LOOKUP_URL) {
			/*
			 * Lookup the URL of type example.com/path?args.
			 * Either as a plain string or SHA1/MD5 digest.
			 */
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(0)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_NONE) == PN_FAIL)
				goto abort;
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(DIGEST_SHA1)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_SHA1) == PN_FAIL)
				goto abort;
			if ((pn->flags & PNFLAG_LOOKUP_DIGEST(DIGEST_MD5)) &&
			    relay_lookup_url(cre, pk.value,
			    DIGEST_MD5) == PN_FAIL)
				goto abort;
		} else if (pn->flags & PNFLAG_LOOKUP_QUERY) {
			/* Lookup the HTTP query arguments */
			if (relay_lookup_query(cre) == PN_FAIL)
				goto abort;
		} else if (pn->flags & PNFLAG_LOOKUP_COOKIE) {
			/* Lookup the HTTP cookie */
			if (relay_lookup_cookie(cre, pk.value) == PN_FAIL)
				goto abort;
		}

 handle:
		pass = 0;
		PROTONODE_FOREACH(pnv, pn, entry) {
			ret = relay_handle_http(cre, pn, pnv, &pk, header);
			if (ret == PN_PASS)
				pass = 1;
			else if (ret == PN_FAIL)
				goto abort;
		}

		if (pass) {
 next:
			if (relay_bufferevent_print(cre->dst, pk.key) == -1 ||
			    relay_bufferevent_print(cre->dst,
			    header ? ": " : " ") == -1 ||
			    relay_bufferevent_print(cre->dst, pk.value) == -1 ||
			    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
				free(line);
				goto fail;
			}
		}
		free(line);
	}
	if (cre->done) {
		RB_FOREACH(proot, proto_tree, cre->tree) {
			PROTONODE_FOREACH(pn, proot, entry)
				if (relay_resolve(cre, proot, pn) != 0)
					return;
		}

		switch (cre->method) {
		case HTTP_METHOD_NONE:
			relay_abort_http(con, 406, "no method", 0);
			return;
		case HTTP_METHOD_CONNECT:
			/* Data stream */
			cre->toread = TOREAD_UNLIMITED;
			bev->readcb = relay_read;
			break;
		case HTTP_METHOD_DELETE:
		case HTTP_METHOD_GET:
		case HTTP_METHOD_HEAD:
		case HTTP_METHOD_OPTIONS:
			cre->toread = 0;
			/* FALLTHROUGH */
		case HTTP_METHOD_POST:
		case HTTP_METHOD_PUT:
		case HTTP_METHOD_RESPONSE:
			/* HTTP request payload */
			if (cre->toread > 0)
				bev->readcb = relay_read_httpcontent;

			/* Single-pass HTTP body */
			if (cre->toread < 0) {
				cre->toread = TOREAD_UNLIMITED;
				bev->readcb = relay_read;
			}
			break;
		default:
			/* HTTP handler */
			cre->toread = TOREAD_HTTP_HEADER;
			bev->readcb = relay_read_http;
			break;
		}
		if (cre->chunked) {
			/* Chunked transfer encoding */
			cre->toread = TOREAD_HTTP_CHUNK_LENGTH;
			bev->readcb = relay_read_httpchunks;
		}

		/* Write empty newline and switch to relay mode */
		if (relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;

		relay_http_request_close(cre);

 done:
		if (cre->dir == RELAY_DIR_REQUEST && cre->toread <= 0 &&
		    proto->lateconnect && cre->dst->bev == NULL) {
			if (rlay->rl_conf.fwdmode == FWD_TRANS) {
				relay_bindanyreq(con, 0, IPPROTO_TCP);
				return;
			}
			if (relay_connect(con) == -1)
				relay_abort_http(con, 502, "session failed", 0);
			return;
		}
	}
	if (con->se_done) {
		relay_close(con, "last http read (done)");
		return;
	}
	if (EVBUFFER_LENGTH(src) && bev->readcb != relay_read_http)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	if (relay_splice(cre) == -1)
		relay_close(con, strerror(errno));
	return;
 fail:
	relay_abort_http(con, 500, strerror(errno), 0);
	return;
 abort:
	free(line);
}

void
relay_read_httpcontent(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	size_t			 size;

	getmonotime(&con->se_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: dir %d, size %lu, to read %lld",
	    __func__, cre->dir, size, cre->toread);
	if (!size)
		return;
	if (relay_spliceadjust(cre) == -1)
		goto fail;

	if (cre->toread > 0) {
		/* Read content data */
		if ((off_t)size > cre->toread) {
			size = cre->toread;
			if (relay_bufferevent_write_chunk(cre->dst, src, size)
			    == -1)
				goto fail;
			cre->toread = 0;
		} else {
			if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
				goto fail;
			cre->toread -= size;
		}
		DPRINTF("%s: done, size %lu, to read %lld", __func__,
		    size, cre->toread);
	}
	if (cre->toread == 0) {
		cre->toread = TOREAD_HTTP_HEADER;
		bev->readcb = relay_read_http;
	}
	if (con->se_done)
		goto done;
	if (bev->readcb != relay_read_httpcontent)
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;
 done:
	relay_close(con, "last http content read");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_read_httpchunks(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);
	char			*line;
	long long		 llval;
	size_t			 size;

	getmonotime(&con->se_tv_last);

	size = EVBUFFER_LENGTH(src);
	DPRINTF("%s: dir %d, size %lu, to read %lld",
	    __func__, cre->dir, size, cre->toread);
	if (!size)
		return;
	if (relay_spliceadjust(cre) == -1)
		goto fail;

	if (cre->toread > 0) {
		/* Read chunk data */
		if ((off_t)size > cre->toread) {
			size = cre->toread;
			if (relay_bufferevent_write_chunk(cre->dst, src, size)
			    == -1)
				goto fail;
			cre->toread = 0;
		} else {
			if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
				goto fail;
			cre->toread -= size;
		}
		DPRINTF("%s: done, size %lu, to read %lld", __func__,
		    size, cre->toread);
	}
	switch (cre->toread) {
	case TOREAD_HTTP_CHUNK_LENGTH:
		line = evbuffer_readline(src);
		if (line == NULL) {
			/* Ignore empty line, continue */
			bufferevent_enable(bev, EV_READ);
			return;
		}
		if (strlen(line) == 0) {
			free(line);
			goto next;
		}

		/*
		 * Read prepended chunk size in hex, ignore the trailer.
		 * The returned signed value must not be negative.
		 */
		if (sscanf(line, "%llx", &llval) != 1 || llval < 0) {
			free(line);
			relay_close(con, "invalid chunk size");
			return;
		}

		if (relay_bufferevent_print(cre->dst, line) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		free(line);

		/* Last chunk is 0 bytes followed by optional trailer */
		if ((cre->toread = llval) == 0) {
			DPRINTF("%s: last chunk", __func__);
			cre->toread = TOREAD_HTTP_CHUNK_TRAILER;
		}
		break;
	case TOREAD_HTTP_CHUNK_TRAILER:
		/* Last chunk is 0 bytes followed by trailer and empty line */
		line = evbuffer_readline(src);
		if (line == NULL) {
			/* Ignore empty line, continue */
			bufferevent_enable(bev, EV_READ);
			return;
		}
		if (relay_bufferevent_print(cre->dst, line) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			free(line);
			goto fail;
		}
		if (strlen(line) == 0) {
			/* Switch to HTTP header mode */
			cre->toread = TOREAD_HTTP_HEADER;
			bev->readcb = relay_read_http;
		}
		free(line);
		break;
	case 0:
		/* Chunk is terminated by an empty newline */
		line = evbuffer_readline(src);
		if (line != NULL)
			free(line);
		if (relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;
		cre->toread = TOREAD_HTTP_CHUNK_LENGTH;
		break;
	}

 next:
	if (con->se_done)
		goto done;
	if (EVBUFFER_LENGTH(src))
		bev->readcb(bev, arg);
	bufferevent_enable(bev, EV_READ);
	return;

 done:
	relay_close(con, "last http chunk read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_http_request_close(struct ctl_relay_event *cre)
{
	if (cre->path != NULL) {
		free(cre->path);
		cre->path = NULL;
	}

	cre->args = NULL;
	cre->version = NULL;

	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}

	cre->line = 0;
	cre->method = 0;
	cre->done = 0;
	cre->chunked = 0;
}

static int
_relay_lookup_url(struct ctl_relay_event *cre, char *host, char *path,
    char *query, enum digest_type type)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *md = NULL;
	int			 ret = PN_FAIL;

	if (asprintf(&val, "%s%s%s%s",
	    host, path,
	    query == NULL ? "" : "?",
	    query == NULL ? "" : query) == -1) {
		relay_abort_http(con, 500, "failed to allocate URL", 0);
		return (PN_FAIL);
	}

	DPRINTF("%s: %s", __func__, val);

	switch (type) {
	case DIGEST_SHA1:
	case DIGEST_MD5:
		if ((md = digeststr(type, val, strlen(val), NULL)) == NULL) {
			relay_abort_http(con, 500,
			    "failed to allocate digest", 0);
			goto fail;
		}
		pkv.key = md;
		break;
	case DIGEST_NONE:
		pkv.key = val;
		break;
	}
	pkv.type = NODE_TYPE_URL;
	pkv.value = "";

	if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
		goto done;

	PROTONODE_FOREACH(pnv, proot, entry) {
		ret = relay_handle_http(cre, proot, pnv, &pkv, 0);
		if (ret == PN_FAIL)
			goto fail;
	}

 done:
	ret = PN_PASS;
 fail:
	if (md != NULL)
		free(md);
	free(val);
	return (ret);
}

int
relay_lookup_url(struct ctl_relay_event *cre, const char *str,
    enum digest_type type)
{
	struct rsession	*con = cre->con;
	int		 i, j, dots;
	char		*hi[RELAY_MAXLOOKUPLEVELS], *p, *pp, *c, ch;
	char		 ph[MAXHOSTNAMELEN];
	int		 ret;

	if (cre->path == NULL)
		return (PN_PASS);

	/*
	 * This is an URL lookup algorithm inspired by
	 * http://code.google.com/apis/safebrowsing/
	 *     developers_guide.html#PerformingLookups
	 */

	DPRINTF("%s: host: '%s', path: '%s', query: '%s'", __func__,
	    str, cre->path, cre->args == NULL ? "" : cre->args);

	if (canonicalize_host(str, ph, sizeof(ph)) == NULL) {
		relay_abort_http(con, 400, "invalid host name", 0);
		return (PN_FAIL);
	}

	bzero(hi, sizeof(hi));
	for (dots = -1, i = strlen(ph) - 1; i > 0; i--) {
		if (ph[i] == '.' && ++dots)
			hi[dots - 1] = &ph[i + 1];
		if (dots > (RELAY_MAXLOOKUPLEVELS - 2))
			break;
	}
	if (dots == -1)
		dots = 0;
	hi[dots] = ph;

	if ((pp = strdup(cre->path)) == NULL) {
		relay_abort_http(con, 500, "failed to allocate path", 0);
		return (PN_FAIL);
	}
	for (i = (RELAY_MAXLOOKUPLEVELS - 1); i >= 0; i--) {
		if (hi[i] == NULL)
			continue;

		/* 1. complete path with query */
		if (cre->args != NULL)
			if ((ret = _relay_lookup_url(cre, hi[i],
			    pp, cre->args, type)) != PN_PASS)
				goto done;

		/* 2. complete path without query */
		if ((ret = _relay_lookup_url(cre, hi[i],
		    pp, NULL, type)) != PN_PASS)
			goto done;

		/* 3. traverse path */
		for (j = 0, p = strchr(pp, '/');
		    p != NULL; p = strchr(p, '/'), j++) {
			if (j > (RELAY_MAXLOOKUPLEVELS - 2) || ++p == '\0')
				break;
			c = &pp[p - pp];
			ch = *c;
			*c = '\0';
			if ((ret = _relay_lookup_url(cre, hi[i],
			    pp, NULL, type)) != PN_PASS)
				goto done;
			*c = ch;
		}
	}

	ret = PN_PASS;
 done:
	free(pp);
	return (ret);
}

int
relay_lookup_query(struct ctl_relay_event *cre)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *ptr;
	int			 ret;

	if (cre->path == NULL || cre->args == NULL || strlen(cre->args) < 2)
		return (PN_PASS);
	if ((val = strdup(cre->args)) == NULL) {
		relay_abort_http(con, 500, "failed to allocate query", 0);
		return (PN_FAIL);
	}

	ptr = val;
	while (ptr != NULL && strlen(ptr)) {
		pkv.key = ptr;
		pkv.type = NODE_TYPE_QUERY;
		if ((ptr = strchr(ptr, '&')) != NULL)
			*ptr++ = '\0';
		if ((pkv.value =
		    strchr(pkv.key, '=')) == NULL ||
		    strlen(pkv.value) < 1)
			continue;
		*pkv.value++ = '\0';

		if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
			continue;
		PROTONODE_FOREACH(pnv, proot, entry) {
			ret = relay_handle_http(cre, proot,
			    pnv, &pkv, 0);
			if (ret == PN_FAIL)
				goto done;
		}
	}

	ret = PN_PASS;
 done:
	free(val);
	return (ret);
}

int
relay_lookup_cookie(struct ctl_relay_event *cre, const char *str)
{
	struct rsession		*con = cre->con;
	struct protonode	*proot, *pnv, pkv;
	char			*val, *ptr;
	int			 ret;

	if ((val = strdup(str)) == NULL) {
		relay_abort_http(con, 500, "failed to allocate cookie", 0);
		return (PN_FAIL);
	}

	for (ptr = val; ptr != NULL && strlen(ptr);) {
		if (*ptr == ' ')
			*ptr++ = '\0';
		pkv.key = ptr;
		pkv.type = NODE_TYPE_COOKIE;
		if ((ptr = strchr(ptr, ';')) != NULL)
			*ptr++ = '\0';
		/*
		 * XXX We do not handle attributes
		 * ($Path, $Domain, or $Port)
		 */
		if (*pkv.key == '$')
			continue;

		if ((pkv.value =
		    strchr(pkv.key, '=')) == NULL ||
		    strlen(pkv.value) < 1)
			continue;
		*pkv.value++ = '\0';
		if (*pkv.value == '"')
			*pkv.value++ = '\0';
		if (pkv.value[strlen(pkv.value) - 1] == '"')
			pkv.value[strlen(pkv.value) - 1] = '\0';
		if ((proot = RB_FIND(proto_tree, cre->tree, &pkv)) == NULL)
			continue;
		PROTONODE_FOREACH(pnv, proot, entry) {
			ret = relay_handle_http(cre, proot, pnv, &pkv, 0);
			if (ret == PN_FAIL)
				goto done;
		}
	}

	ret = PN_PASS;
 done:
	free(val);
	return (ret);
}

void
relay_abort_http(struct rsession *con, u_int code, const char *msg,
    u_int16_t labelid)
{
	struct relay		*rlay = con->se_relay;
	struct bufferevent	*bev = con->se_in.bev;
	const char		*httperr = print_httperror(code), *text = "";
	char			*httpmsg;
	time_t			 t;
	struct tm		*lt;
	char			 tmbuf[32], hbuf[128];
	const char		*style, *label = NULL;

	/* In some cases this function may be called from generic places */
	if (rlay->rl_proto->type != RELAY_PROTO_HTTP ||
	    (rlay->rl_proto->flags & F_RETURN) == 0) {
		relay_close(con, msg);
		return;
	}

	if (bev == NULL)
		goto done;

	/* Some system information */
	if (print_host(&rlay->rl_conf.ss, hbuf, sizeof(hbuf)) == NULL)
		goto done;

	/* RFC 2616 "tolerates" asctime() */
	time(&t);
	lt = localtime(&t);
	tmbuf[0] = '\0';
	if (asctime_r(lt, tmbuf) != NULL)
		tmbuf[strlen(tmbuf) - 1] = '\0';	/* skip final '\n' */

	/* Do not send details of the Internal Server Error */
	if (code != 500)
		text = msg;
	if (labelid != 0)
		label = pn_id2name(labelid);

	/* A CSS stylesheet allows minimal customization by the user */
	if ((style = rlay->rl_proto->style) == NULL)
		style = "body { background-color: #a00000; color: white; }";

	/* Generate simple HTTP+HTML error document */
	if (asprintf(&httpmsg,
	    "HTTP/1.0 %03d %s\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Connection: close\r\n"
	    "Content-Type: text/html\r\n"
	    "\r\n"
	    "<!DOCTYPE HTML PUBLIC "
	    "\"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
	    "<html>\n"
	    "<head>\n"
	    "<title>%03d %s</title>\n"
	    "<style type=\"text/css\"><!--\n%s\n--></style>\n"
	    "</head>\n"
	    "<body>\n"
	    "<h1>%s</h1>\n"
	    "<div id='m'>%s</div>\n"
	    "<div id='l'>%s</div>\n"
	    "<hr><address>%s at %s port %d</address>\n"
	    "</body>\n"
	    "</html>\n",
	    code, httperr, tmbuf, RELAYD_SERVERNAME,
	    code, httperr, style, httperr, text,
	    label == NULL ? "" : label,
	    RELAYD_SERVERNAME, hbuf, ntohs(rlay->rl_conf.port)) == -1)
		goto done;

	/* Dump the message without checking for success */
	relay_dump(&con->se_in, httpmsg, strlen(httpmsg));
	free(httpmsg);

 done:
	if (asprintf(&httpmsg, "%s (%03d %s)", msg, code, httperr) == -1)
		relay_close(con, msg);
	else {
		relay_close(con, httpmsg);
		free(httpmsg);
	}
}

char *
relay_expand_http(struct ctl_relay_event *cre, char *val, char *buf, size_t len)
{
	struct rsession	*con = cre->con;
	struct relay	*rlay = con->se_relay;
	char		 ibuf[128];

	if (strlcpy(buf, val, len) >= len)
		return (NULL);

	if (strstr(val, "$REMOTE_") != NULL) {
		if (strstr(val, "$REMOTE_ADDR") != NULL) {
			if (print_host(&cre->ss, ibuf, sizeof(ibuf)) == NULL)
				return (NULL);
			if (expand_string(buf, len,
			    "$REMOTE_ADDR", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$REMOTE_PORT") != NULL) {
			snprintf(ibuf, sizeof(ibuf), "%u", ntohs(cre->port));
			if (expand_string(buf, len,
			    "$REMOTE_PORT", ibuf) != 0)
				return (NULL);
		}
	}
	if (strstr(val, "$SERVER_") != NULL) {
		if (strstr(val, "$SERVER_ADDR") != NULL) {
			if (print_host(&rlay->rl_conf.ss,
			    ibuf, sizeof(ibuf)) == NULL)
				return (NULL);
			if (expand_string(buf, len,
			    "$SERVER_ADDR", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$SERVER_PORT") != NULL) {
			snprintf(ibuf, sizeof(ibuf), "%u",
			    ntohs(rlay->rl_conf.port));
			if (expand_string(buf, len,
			    "$SERVER_PORT", ibuf) != 0)
				return (NULL);
		}
		if (strstr(val, "$SERVER_NAME") != NULL) {
			if (expand_string(buf, len,
			    "$SERVER_NAME", RELAYD_SERVERNAME) != 0)
				return (NULL);
		}
	}
	if (strstr(val, "$TIMEOUT") != NULL) {
		snprintf(ibuf, sizeof(ibuf), "%lld",
		    (long long)rlay->rl_conf.timeout.tv_sec);
		if (expand_string(buf, len, "$TIMEOUT", ibuf) != 0)
			return (NULL);
	}

	return (buf);
}

int
relay_resolve(struct ctl_relay_event *cre,
    struct protonode *proot, struct protonode *pn)
{
	struct rsession		*con = cre->con;
	char			 buf[IBUF_READ_SIZE], *ptr;
	int			 id;

	if (pn->mark && (pn->mark != con->se_mark))
		return (0);

	switch (pn->action) {
	case NODE_ACTION_FILTER:
		id = cre->nodes[proot->id];
		if (SIMPLEQ_NEXT(pn, entry) == NULL)
			cre->nodes[proot->id] = 0;
		if (id <= 1)
			return (0);
		break;
	case NODE_ACTION_EXPECT:
		id = cre->nodes[proot->id];
		if (SIMPLEQ_NEXT(pn, entry) == NULL)
			cre->nodes[proot->id] = 0;
		if (id > 1)
			return (0);
		break;
	default:
		if (cre->nodes[pn->id]) {
			cre->nodes[pn->id] = 0;
			return (0);
		}
		break;
	}
	switch (pn->action) {
	case NODE_ACTION_APPEND:
	case NODE_ACTION_CHANGE:
		ptr = pn->value;
		if ((pn->flags & PNFLAG_MACRO) &&
		    (ptr = relay_expand_http(cre, pn->value,
		    buf, sizeof(buf))) == NULL)
			break;
		if (relay_bufferevent_print(cre->dst, pn->key) == -1 ||
		    relay_bufferevent_print(cre->dst, ": ") == -1 ||
		    relay_bufferevent_print(cre->dst, ptr) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1) {
			relay_abort_http(con, 500,
			    "failed to modify header", 0);
			return (-1);
		}
		DPRINTF("%s: add '%s: %s'", __func__, pn->key, ptr);
		break;
	case NODE_ACTION_EXPECT:
		DPRINTF("%s: missing '%s: %s'", __func__, pn->key, pn->value);
		relay_abort_http(con, 403, "incomplete request", pn->label);
		return (-1);
	case NODE_ACTION_FILTER:
		DPRINTF("%s: filtered '%s: %s'", __func__, pn->key, pn->value);
		relay_abort_http(con, 403, "rejecting request", pn->label);
		return (-1);
	default:
		break;
	}
	return (0);
}

int
relay_handle_http(struct ctl_relay_event *cre, struct protonode *proot,
    struct protonode *pn, struct protonode *pk, int header)
{
	struct rsession		*con = cre->con;
	char			 buf[IBUF_READ_SIZE], *ptr;
	int			 ret = PN_DROP, mark = 0;
	struct protonode	*next;

	/* Check if this action depends on a marked session */
	if (pn->mark != 0)
		mark = pn->mark == con->se_mark ? 1 : -1;

	switch (pn->action) {
	case NODE_ACTION_EXPECT:
	case NODE_ACTION_FILTER:
	case NODE_ACTION_MARK:
		break;
	default:
		if (mark == -1)
			return (PN_PASS);
		break;
	}

	switch (pn->action) {
	case NODE_ACTION_APPEND:
		if (!header)
			return (PN_PASS);
		ptr = pn->value;
		if ((pn->flags & PNFLAG_MACRO) &&
		    (ptr = relay_expand_http(cre, pn->value,
		    buf, sizeof(buf))) == NULL)
			break;
		if (relay_bufferevent_print(cre->dst, pn->key) == -1 ||
		    relay_bufferevent_print(cre->dst, ": ") == -1 ||
		    relay_bufferevent_print(cre->dst, pk->value) == -1 ||
		    relay_bufferevent_print(cre->dst, ", ") == -1 ||
		    relay_bufferevent_print(cre->dst, ptr) == -1 ||
		    relay_bufferevent_print(cre->dst, "\r\n") == -1)
			goto fail;
		cre->nodes[pn->id] = 1;
		DPRINTF("%s: append '%s: %s, %s'", __func__,
		    pk->key, pk->value, ptr);
		break;
	case NODE_ACTION_CHANGE:
	case NODE_ACTION_REMOVE:
		if (!header)
			return (PN_PASS);
		DPRINTF("%s: change/remove '%s: %s'", __func__,
		    pk->key, pk->value);
		break;
	case NODE_ACTION_EXPECT:
		/*
		 * A client may specify the header line for multiple times
		 * trying to circumvent the filter.
		 */
		if (cre->nodes[proot->id] > 1) {
			relay_abort_http(con, 400, "repeated header line", 0);
			return (PN_FAIL);
		}
		/* FALLTHROUGH */
	case NODE_ACTION_FILTER:
		DPRINTF("%s: %s '%s: %s'", __func__,
		    (pn->action == NODE_ACTION_EXPECT) ? "expect" : "filter",
		    pn->key, pn->value);

		/* Do not drop the entity */
		ret = PN_PASS;

		if (mark != -1 &&
		    fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0) {
			cre->nodes[proot->id] = 1;

			/* Fail instantly */
			if (pn->action == NODE_ACTION_FILTER) {
				(void)relay_lognode(con, pn, pk,
				    buf, sizeof(buf));
				relay_abort_http(con, 403,
				    "rejecting request", pn->label);
				return (PN_FAIL);
			}
		}
		next = SIMPLEQ_NEXT(pn, entry);
		if (next == NULL || next->action != pn->action)
			cre->nodes[proot->id]++;
		break;
	case NODE_ACTION_HASH:
		DPRINTF("%s: hash '%s: %s'", __func__,
		    pn->key, pk->value);
		if (!con->se_hashkeyset)
			con->se_hashkey = HASHINIT;
		con->se_hashkey = hash32_str(pk->value, con->se_hashkey);
		con->se_hashkeyset = 1;
		log_debug("%s: hash 0x%04x", __func__, con->se_hashkey);
		ret = PN_PASS;
		break;
	case NODE_ACTION_LOG:
		log_debug("%s: log '%s: %s'", __func__, pn->key, pk->value);
		ret = PN_PASS;
		break;
	case NODE_ACTION_MARK:
		DPRINTF("%s: mark '%s: %s'", __func__,
		    pn->key, pk->value);
		if (fnmatch(pn->value, pk->value, FNM_CASEFOLD) == 0)
			con->se_mark = pn->mark;
		ret = PN_PASS;
		break;
	case NODE_ACTION_NONE:
		return (PN_PASS);
	}
	if (mark != -1 && relay_lognode(con, pn, pk, buf, sizeof(buf)) == -1)
		goto fail;

	return (ret);
 fail:
	relay_abort_http(con, 500, strerror(errno), 0);
	return (PN_FAIL);
}
