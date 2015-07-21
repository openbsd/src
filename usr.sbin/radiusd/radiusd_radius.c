/*	$OpenBSD: radiusd_radius.c,v 1.1 2015/07/21 04:06:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2013 Internet Initiative Japan Inc.
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
#include <sys/socket.h>
#include <netinet/in.h>

#include <err.h>
#include <event.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <radius.h>

#include "radiusd.h"
#include "radiusd_module.h"
#include "util.h"
#include "log.h"

struct radius_server {
	struct module_radius		*module;
	int				 sock;
	union {
		struct sockaddr_in6	 sin6;
		struct sockaddr_in	 sin4;
	}				 addr;
	union {
		struct sockaddr_in6	 sin6;
		struct sockaddr_in	 sin4;
	}				 local;
	struct event			 ev;
	u_char				 req_id_seq;
};

struct module_radius {
	struct module_base		*base;
	struct radius_server		 server[4];
	char				 secret[RADIUSD_SECRET_MAX];
	u_int				 nserver;
	u_int				 curr_server;
	u_int				 req_timeout;
	u_int				 max_tries;
	u_int				 max_failovers;
	u_int				 nfailover;
	TAILQ_HEAD(,module_radius_req)	 req;
};

struct module_radius_req {
	struct module_radius		*module;
	struct radius_server		*server;
	u_char				 q_id;
	RADIUS_PACKET			*q_pkt;
	u_int				 ntry;
	u_int				 nfailover;
	u_char				 req_id;
	struct event			 ev;
	TAILQ_ENTRY(module_radius_req)	 next;
};

static void	 module_radius_init(struct module_radius *);
static void	 module_radius_config_set(void *, const char *, int,
		    char * const *);
static void	 module_radius_start(void *);
static void	 module_radius_stop(void *);
static void	 module_radius_access_request(void *, u_int, const u_char *,
		    size_t);
static int	 radius_server_start(struct radius_server *);
static void	 radius_server_stop(struct radius_server *);
static void	 radius_server_on_event(int, short, void *);
static void	 radius_server_on_fail(struct radius_server *, const char *);
static void	 module_radius_req_send(struct module_radius_req *);
static int	 module_radius_req_reset_event(struct module_radius_req *);
static void	 module_radius_req_on_timeout(int, short, void *);
static void	 module_radius_req_on_success(struct module_radius_req *,
		    const u_char *, size_t);
static void	 module_radius_req_on_failure(struct module_radius_req *);

static void	 module_radius_req_free(struct module_radius_req *);
static void	 module_radius_req_select_server(struct module_radius_req *);

static void	 module_radius_req_reset_msgauth(struct module_radius_req *);
static void	 module_radius_log(struct module_radius *, int, const char *, ...);

static struct module_handlers module_radius_handlers = {
	.config_set = module_radius_config_set,
	.start = module_radius_start,
	.stop = module_radius_stop,
	.access_request = module_radius_access_request
};

#ifndef nitems
#define nitems(_x)    (sizeof((_x)) / sizeof((_x)[0]))
#endif

int
main(int argc, char *argv[])
{
	static struct module_radius module_radius;

	module_radius_init(&module_radius);
	openlog(NULL, LOG_PID, LOG_DAEMON);

	// XXX drop privilledge
	// XXX change root

	if ((module_radius.base = module_create(
	    STDIN_FILENO, &module_radius, &module_radius_handlers)) == NULL)
		err(1, "Could not create a module instance");

	module_load(module_radius.base);
	log_init(1);
	event_init();
	module_start(module_radius.base);
	event_loop(0);

	exit(EXIT_SUCCESS);
}

static void
module_radius_init(struct module_radius *_this)
{
	memset(_this, 0, sizeof(struct module_radius));
	TAILQ_INIT(&_this->req);
}

static void
module_radius_config_set(void *ctx, const char *paramname, int paramvalc,
    char * const * paramvalv)
{
	const char		*errmsg = NULL;
	struct addrinfo		*res;
	struct module_radius	*_this = ctx;

	if (strcmp(paramname, "server") == 0) {
		SYNTAX_ASSERT(paramvalc == 1,
		    "`server' must have just one argument");
		SYNTAX_ASSERT(_this->nserver < (int)nitems(_this->server),
		    "number of server reached limit");

		if (addrport_parse(paramvalv[0], IPPROTO_UDP, &res) != 0)
			SYNTAX_ASSERT(0, "could not parse address and port");
		memcpy(&_this->server[_this->nserver].addr, res->ai_addr,
		    res->ai_addrlen);

		if (ntohs(_this->server[_this->nserver].addr.sin4.sin_port)
		    == 0)
			_this->server[_this->nserver].addr.sin4.sin_port
			    = htons(RADIUS_DEFAULT_PORT);

		_this->server[_this->nserver].sock = -1;
		_this->nserver++;
		freeaddrinfo(res);
	} else if (strcmp(paramname, "request-timeout") == 0) {
		SYNTAX_ASSERT(paramvalc == 1,
		    "`request-timeout' must have just one argument");
		_this->req_timeout = (int)strtonum(paramvalv[0], 0,
		    UINT16_MAX, &errmsg);
		if (_this->req_timeout == 0 && errmsg != NULL) {
			module_send_message(_this->base, IMSG_NG,
			    "`request-timeout must be 0-%d", UINT16_MAX);
			return;
		}
	} else if (strcmp(paramname, "max-tries") == 0) {
		SYNTAX_ASSERT(paramvalc == 1,
		    "`max-tries' must have just one argument");
		_this->max_tries = (int)strtonum(paramvalv[0], 0,
		    UINT16_MAX, &errmsg);
		if (_this->max_tries == 0 && errmsg != NULL) {
			module_send_message(_this->base, IMSG_NG,
			    "`max-tries must be 0-%d", UINT16_MAX);
			return;
		}

	} else if (strcmp(paramname, "max-failovers") == 0) {
		SYNTAX_ASSERT(paramvalc == 1,
		    "`max-failovers' must have just one argument");
		_this->max_failovers = (int)strtonum(paramvalv[0], 0,
		    UINT16_MAX, &errmsg);
		if (_this->max_failovers == 0 && errmsg != NULL) {
			module_send_message(_this->base, IMSG_NG,
			    "`max-failovers' must be 0-%d", UINT16_MAX);
			return;
		}
	} else if (strcmp(paramname, "secret") == 0) {
		SYNTAX_ASSERT(paramvalc == 1,
		    "`secret' must have just one argument");
		if (strlcpy(_this->secret, paramvalv[0], sizeof(_this->secret))
		    >= sizeof(_this->secret)) {
			module_send_message(_this->base, IMSG_NG,
			    "`secret' length must be 0-%lu",
			    (u_long) sizeof(_this->secret) - 1);
			return;
		}
	} else {
		module_send_message(_this->base, IMSG_NG,
		    "Unknown config parameter name `%s'", paramname);
		return;
	}
	module_send_message(_this->base, IMSG_OK, NULL);

	return;
syntax_error:
	module_send_message(_this->base, IMSG_NG, "%s", errmsg);
}

static void
module_radius_start(void *ctx)
{
	u_int			 i;
	struct module_radius	*_this = ctx;

	if (_this->nserver <= 0) {
		module_send_message(_this->base, IMSG_NG,
			"module `radius' needs one `server' at least");
		return;
	}

	for (i = 0; i < _this->nserver; i++) {
		_this->server[i].module = _this;
		if (radius_server_start(&_this->server[i]) != 0) {
			module_send_message(_this->base, IMSG_NG,
				"module `radius' failed to start one of "
				"the servers");
			return;
		}
	}
	module_send_message(_this->base, IMSG_OK, NULL);

	if (_this->secret[0] != '\0')
		module_notify_secret(_this->base, _this->secret);
}

static void
module_radius_stop(void *ctx)
{
	u_int				 i;
	struct module_radius_req	*req, *treq;
	struct module_radius		*_this = ctx;

	TAILQ_FOREACH_SAFE(req, &_this->req, next, treq) {
		module_radius_req_on_failure(req);
		TAILQ_REMOVE(&_this->req, req, next);
	}

	for (i = 0; i < _this->nserver; i++)
		radius_server_stop(&_this->server[i]);
}

static void
module_radius_access_request(void *ctx, u_int q_id, const u_char *pkt,
    size_t pktlen)
{
	struct module_radius		*_this = ctx;
	struct module_radius_req	*req;
	u_char				 attrbuf[256];
	ssize_t				 attrlen;

	req = malloc(sizeof(struct module_radius_req));
	if (req == NULL) {
		module_radius_log(_this, LOG_WARNING,
		    "%s: Out of memory: %m", __func__);
		goto on_fail;
	}

	req->ntry = 0;
	req->module = _this;
	req->q_id = q_id;
	if ((req->q_pkt = radius_convert_packet(pkt, pktlen)) == NULL) {
		module_radius_log(_this, LOG_WARNING,
		    "%s: radius_convert_packet() failed: %m", __func__);
		goto on_fail;
	}
	evtimer_set(&req->ev, module_radius_req_on_timeout, req);
	TAILQ_INSERT_TAIL(&req->module->req, req, next);

	/*
	 * radiusd decrypt User-Password attribute.  crypt it again with our
	 * secret.
	 */
	attrlen = sizeof(attrbuf);
	if (_this->secret[0] != '\0' &&
	    radius_get_raw_attr(req->q_pkt, RADIUS_TYPE_USER_PASSWORD,
		    attrbuf, &attrlen) == 0) {
		attrbuf[attrlen] = '\0';
		radius_del_attr_all(req->q_pkt, RADIUS_TYPE_USER_PASSWORD);
		radius_put_user_password_attr(req->q_pkt, attrbuf,
		    _this->secret);
	}

	/* select current server */
	module_radius_req_select_server(req);

	module_radius_req_send(req);

	return;

on_fail:
	free(req);
	module_accsreq_aborted(_this->base, q_id);
}

/*
 * radius_server
 */
static int
radius_server_start(struct radius_server *_this)
{
	socklen_t	 locallen;
	char		 buf0[NI_MAXHOST + NI_MAXSERV + 32];
	char		 buf1[NI_MAXHOST + NI_MAXSERV + 32];

	if ((_this->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		log_warn("%s: socket() failed", __func__);
		goto on_error;
	}
	if (connect(_this->sock, (struct sockaddr *)&_this->addr,
		_this->addr.sin4.sin_len) != 0) {
		log_warn("%s: connect to %s failed", __func__,
		    addrport_tostring((struct sockaddr *)&_this->addr,
			_this->addr.sin4.sin_len, buf1, sizeof(buf1)));
		goto on_error;
	}
	locallen = sizeof(_this->local);
	if (getsockname(_this->sock, (struct sockaddr *)&_this->local,
	    &locallen) != 0) {
		log_warn("%s: getsockanme() failed", __func__);
		goto on_error;
	}
	module_radius_log(_this->module, LOG_INFO,
	    "Use %s to send requests for %s",
	    addrport_tostring((struct sockaddr *)&_this->local,
		    locallen, buf0, sizeof(buf0)),
	    addrport_tostring((struct sockaddr *)&_this->addr,
		    _this->addr.sin4.sin_len, buf1, sizeof(buf1)));

	event_set(&_this->ev, _this->sock, EV_READ | EV_PERSIST,
	    radius_server_on_event, _this);
	if (event_add(&_this->ev, NULL)) {
		log_warn("%s: event_add() failed", __func__);
		goto on_error;
	}

	return (0);
on_error:
	if (_this->sock >= 0)
		close(_this->sock);
	_this->sock = -1;
	return (-1);
}

static void
radius_server_stop(struct radius_server *_this)
{
	event_del(&_this->ev);
	if (_this->sock >= 0)
		close(_this->sock);
	_this->sock = -1;
}

static void
radius_server_on_event(int fd, short evmask, void *ctx)
{
	int				 sz, res_id;
	u_char				 pkt[65535];
	char				 buf[NI_MAXHOST + NI_MAXSERV + 32];
	struct radius_server		*_this = ctx;
	RADIUS_PACKET			*radpkt = NULL;
	struct module_radius_req	*req;
	struct sockaddr			*peer;

	peer = (struct sockaddr *)&_this->addr;
	if ((sz = recv(_this->sock, pkt, sizeof(pkt), 0)) <= 0) {
		module_radius_log(_this->module, LOG_WARNING,
		    "server=%s recv() failed: %m",
		    addrport_tostring(peer, peer->sa_len, buf, sizeof(buf)));
		return;
	}
	if ((radpkt = radius_convert_packet(pkt, sz)) == NULL) {
		module_radius_log(_this->module, LOG_WARNING,
		    "server=%s could not convert the received message to a "
		    "RADIUS packet object: %m",
		    addrport_tostring(peer, peer->sa_len, buf, sizeof(buf)));
		return;
	}
	res_id = radius_get_id(radpkt);
	TAILQ_FOREACH(req, &_this->module->req, next) {
		if (req->server == _this && req->req_id == res_id)
			break;
	}
	if (req == NULL) {
		module_radius_log(_this->module, LOG_WARNING,
		    "server=%s Received radius message has unknown id=%d",
		    addrport_tostring(peer, peer->sa_len, buf, sizeof(buf)),
		    res_id);
		goto out;
	}
	radius_set_request_packet(radpkt, req->q_pkt);

	if (_this->module->secret[0] != '\0') {
		if (radius_check_response_authenticator(radpkt,
		    _this->module->secret) != 0) {
			module_radius_log(_this->module, LOG_WARNING,
			    "server=%s Received radius message(id=%d) has bad "
			    "authenticator",
			    addrport_tostring(peer, peer->sa_len, buf,
			    sizeof(buf)), res_id);
			goto out;
		}
		if (radius_has_attr(radpkt,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR) &&
		    radius_check_message_authenticator(radpkt,
			    _this->module->secret) != 0) {
			module_radius_log(_this->module, LOG_WARNING,
			    "server=%s Received radius message(id=%d) has bad "
			    "message authenticator",
			    addrport_tostring(peer, peer->sa_len, buf,
			    sizeof(buf)), res_id);
			goto out;
		}
	}

	module_radius_log(_this->module, LOG_INFO,
	    "q=%u received a response from server %s", req->q_id,
	    addrport_tostring(peer, peer->sa_len, buf, sizeof(buf)));

	module_radius_req_on_success(req, radius_get_data(radpkt),
	    radius_get_length(radpkt));
out:
	if (radpkt != NULL)
		radius_delete_packet(radpkt);
}

static void
radius_server_on_fail(struct radius_server *_this, const char *failmsg)
{
	char		 buf0[NI_MAXHOST + NI_MAXSERV + 32];
	char		 buf1[NI_MAXHOST + NI_MAXSERV + 32];
	struct sockaddr	*caddr, *naddr;

	caddr = (struct sockaddr *)&_this->addr;
	if (_this->module->nserver <= 1) {
		module_radius_log(_this->module, LOG_WARNING,
		    "Server %s failed: %s",
		    addrport_tostring(caddr, caddr->sa_len, buf0, sizeof(buf0)),
		    failmsg);
		return;
	}
	_this->module->curr_server++;
	_this->module->curr_server %= _this->module->nserver;
	naddr = (struct sockaddr *)
	    &_this->module->server[_this->module->curr_server].addr;

	module_radius_log(_this->module, LOG_WARNING,
	    "Server %s failed: %s  Fail over to %s",
	    addrport_tostring(caddr, caddr->sa_len, buf0, sizeof(buf0)),
	    failmsg,
	    addrport_tostring(naddr, naddr->sa_len, buf1, sizeof(buf1)));
}

/* module_radius_req */

static void
module_radius_req_send(struct module_radius_req *req)
{
	int		 sz;
	struct sockaddr	*peer;
	char		 msg[BUFSIZ];

	peer = (struct sockaddr *)&req->server->addr;
	if ((sz = send(req->server->sock, radius_get_data(req->q_pkt),
	    radius_get_length(req->q_pkt), 0)) < 0) {
		module_radius_log(req->module, LOG_WARNING,
		    "Sending RADIUS query q=%u to %s failed: %m",
		    req->q_id,
		    addrport_tostring(peer, peer->sa_len, msg, sizeof(msg)));
		/* retry anyway */
	}
	module_radius_log(req->module, LOG_INFO,
	    "Send RADIUS query q=%u id=%d to %s successfully",
	    req->q_id, req->req_id,
	    addrport_tostring(peer, peer->sa_len, msg, sizeof(msg)));
	if (module_radius_req_reset_event(req) != -1)
		req->ntry++;
}

static int
module_radius_req_reset_event(struct module_radius_req *req)
{
	struct timeval	 tv;
	static int	 timeouts[] = { 2, 4, 8 };

	tv.tv_usec = 0;
	if (req->module->req_timeout != 0)
		tv.tv_sec = req->module->req_timeout;
	else {
		if (req->ntry < nitems(timeouts))
			tv.tv_sec = timeouts[req->ntry];
		else
			tv.tv_sec = timeouts[nitems(timeouts) - 1];
	}
	if (evtimer_add(&req->ev, &tv) != 0) {
		module_radius_log(req->module, LOG_WARNING,
		    "Cannot proccess the request for q=%u: "
		    "evtimer_add() failed: %m", req->q_id);
		module_radius_req_on_failure(req);
		return(-1);
	}
	return (0);
}

static void
module_radius_req_on_timeout(int fd, short evmask, void *ctx)
{
	struct module_radius_req	*req = ctx;
	char				 msg[BUFSIZ];


	if (req->module->max_tries <= req->ntry) {
		snprintf(msg, sizeof(msg), "q=%u didn't response RADIUS query "
		    "%d time%s", req->q_id, req->ntry,
		    (req->ntry > 0)? "s" : "");
		radius_server_on_fail(req->server, msg);
		if (++req->nfailover >= req->module->max_failovers) {
			module_radius_log(req->module,
			    LOG_WARNING, "RADIUS query q=%u time out",
			    req->q_id);
			module_radius_req_on_failure(req);
			return;
		}
		/* select the next server */
		module_radius_req_select_server(req);
	}
	module_radius_req_send(req);
}

static void
module_radius_req_on_success(struct module_radius_req *req,
    const u_char *pkt, size_t pktlen)
{
	module_accsreq_answer(req->module->base, req->q_id, 1, pkt, pktlen);
	module_radius_req_free(req);
}

static void
module_radius_req_on_failure(struct module_radius_req *req)
{
	module_accsreq_aborted(req->module->base, req->q_id);
	module_radius_req_free(req);
}


static void
module_radius_req_free(struct module_radius_req *req)
{
	evtimer_del(&req->ev);
	TAILQ_REMOVE(&req->module->req, req, next);
	if (req->q_pkt != NULL)
		radius_delete_packet(req->q_pkt);
	free(req);
}

static void
module_radius_req_select_server(struct module_radius_req *req)
{
	req->server = &req->module->server[req->module->curr_server];
	req->ntry = 0;
	req->req_id = req->server->req_id_seq++;
	radius_set_id(req->q_pkt, req->req_id);
	module_radius_req_reset_msgauth(req);
}

static void
module_radius_req_reset_msgauth(struct module_radius_req *req)
{
	if (radius_has_attr(req->q_pkt, RADIUS_TYPE_MESSAGE_AUTHENTICATOR))
		radius_del_attr_all(req->q_pkt,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR);
	if (req->module->secret[0] != '\0')
		radius_put_message_authenticator(req->q_pkt,
		    req->module->secret);
}

static void
module_radius_log(struct module_radius *_this, int pri, const char *fmt, ...)
{
	char		fmt0[BUFSIZ];
	va_list		va;

	snprintf(fmt0, sizeof(fmt0), "radius: %s", fmt);
	va_start(va, fmt);
	vlog(pri, fmt0, va);
	va_end(va);
}
