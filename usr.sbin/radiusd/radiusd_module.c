/*	$OpenBSD: radiusd_module.c,v 1.2 2015/07/27 08:58:09 yasuoka Exp $	*/

/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
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

/* radiusd_module.c -- helper functions for radiusd modules */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <pwd.h>

#include "radiusd.h"
#include "radiusd_module.h"
#include "imsg_subr.h"

static void	(*module_config_set) (void *, const char *, int,
		    char * const *) = NULL;
static void	(*module_start_module) (void *) = NULL;
static void	(*module_stop_module) (void *) = NULL;
static void	(*module_userpass) (void *, u_int, const char *, const char *)
		    = NULL;
static void	(*module_access_request) (void *, u_int, const u_char *,
		    size_t) = NULL;

struct module_base {
	void			*ctx;
	struct imsgbuf		 ibuf;
	bool			 priv_dropped;

	/* Buffer for receiving the RADIUS packet */
	u_char			*radpkt;
	int			 radpktsiz;
	int			 radpktoff;

#ifdef USE_LIBEVENT
	struct module_imsgbuf	*module_imsgbuf;
	bool			 writeready;
	bool			 ev_onhandler;
	struct event		 ev;
#endif
};

static int	 module_common_radpkt(struct module_base *, uint32_t, u_int,
		    int, const u_char *, size_t);
static int	 module_recv_imsg(struct module_base *);
static int	 module_imsg_handler(struct module_base *, struct imsg *);
#ifdef USE_LIBEVENT
static void	 module_on_event(int, short, void *);
#endif
static void	 module_reset_event(struct module_base *);

struct module_base *
module_create(int sock, void *ctx, struct module_handlers *handler)
{
	struct module_base	*base;

	if ((base = calloc(1, sizeof(struct module_base))) == NULL)
		return (NULL);

	imsg_init(&base->ibuf, sock);
	base->ctx = ctx;

	module_userpass = handler->userpass;
	module_access_request = handler->access_request;
	module_config_set = handler->config_set;
	module_start_module = handler->start;
	module_stop_module = handler->stop;

	return (base);
}

void
module_start(struct module_base *base)
{
#ifdef USE_LIBEVENT
	int	 on;

	on = 1;
	if (fcntl(base->ibuf.fd, O_NONBLOCK, &on) == -1)
		err(1, "Failed to setup NONBLOCK");
	module_reset_event(base);
#endif
}

int
module_run(struct module_base *base)
{
	int	 ret;

	ret = module_recv_imsg(base);
	if (ret == 0)
		imsg_flush(&base->ibuf);

	return (ret);
}

void
module_destroy(struct module_base *base)
{
	imsg_clear(&base->ibuf);
	free(base);
}

void
module_load(struct module_base *base)
{
	struct radiusd_module_load_arg	 load;

	memset(&load, 0, sizeof(load));
	if (module_userpass != NULL)
		load.cap |= RADIUSD_MODULE_CAP_USERPASS;
	if (module_access_request != NULL)
		load.cap |= RADIUSD_MODULE_CAP_ACCSREQ;
	imsg_compose(&base->ibuf, IMSG_RADIUSD_MODULE_LOAD, 0, 0, -1, &load,
	    sizeof(load));
	imsg_flush(&base->ibuf);
}

void
module_drop_privilege(struct module_base *base)
{
	struct passwd	*pw;

	/* Drop the privilege */
	if ((pw = getpwnam(RADIUSD_USER)) == NULL)
		goto on_fail;
	if (chroot(pw->pw_dir) == -1)
		goto on_fail;
	if (chdir("/") == -1)
		goto on_fail;
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		goto on_fail;
	base->priv_dropped = true;

on_fail:
	return;
}

int
module_notify_secret(struct module_base *base, const char *secret)
{
	int		 ret;

	ret = imsg_compose(&base->ibuf, IMSG_RADIUSD_MODULE_NOTIFY_SECRET,
	    0, 0, -1, secret, strlen(secret) + 1);
	module_reset_event(base);

	return (ret);
}

int
module_send_message(struct module_base *base, uint32_t cmd, const char *fmt,
    ...)
{
	char	*msg;
	va_list	 ap;
	int	 ret;

	if (fmt == NULL)
		ret = imsg_compose(&base->ibuf, cmd, 0, 0, -1, NULL, 0);
	else {
		va_start(ap, fmt);
		vasprintf(&msg, fmt, ap);
		va_end(ap);
		if (msg == NULL)
			return (-1);
		ret = imsg_compose(&base->ibuf, cmd, 0, 0, -1, msg,
		    strlen(msg) + 1);
		free(msg);
	}
	module_reset_event(base);

	return (ret);
}

int
module_userpass_ok(struct module_base *base, u_int q_id, const char *msg)
{
	int		 ret;
	struct iovec	 iov[2];

	iov[0].iov_base = &q_id;
	iov[0].iov_len = sizeof(q_id);
	iov[1].iov_base = (char *)msg;
	iov[1].iov_len = strlen(msg) + 1;
	ret = imsg_composev(&base->ibuf, IMSG_RADIUSD_MODULE_USERPASS_OK,
	    0, 0, -1, iov, 2);
	module_reset_event(base);

	return (ret);
}

int
module_userpass_fail(struct module_base *base, u_int q_id, const char *msg)
{
	int		 ret;
	struct iovec	 iov[2];

	iov[0].iov_base = &q_id;
	iov[0].iov_len = sizeof(q_id);
	iov[1].iov_base = (char *)msg;
	iov[1].iov_len = strlen(msg) + 1;
	ret = imsg_composev(&base->ibuf, IMSG_RADIUSD_MODULE_USERPASS_FAIL,
	    0, 0, -1, iov, 2);
	module_reset_event(base);

	return (ret);
}

int
module_accsreq_answer(struct module_base *base, u_int q_id, int modified,
    const u_char *pkt, size_t pktlen)
{
	return (module_common_radpkt(base, IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER,
	    q_id, modified, pkt, pktlen));
}

int
module_accsreq_aborted(struct module_base *base, u_int q_id)
{
	int	 ret;

	ret = imsg_compose(&base->ibuf, IMSG_RADIUSD_MODULE_ACCSREQ_ABORTED,
	    0, 0, -1, &q_id, sizeof(u_int));
	module_reset_event(base);

	return (ret);
}

static int
module_common_radpkt(struct module_base *base, uint32_t imsg_type, u_int q_id,
    int modified, const u_char *pkt, size_t pktlen)
{
	int		 ret = 0, off = 0, len, siz;
	struct iovec	 iov[2];
	struct radiusd_module_radpkt_arg	 ans;

	len = pktlen;
	ans.q_id = q_id;
	ans.modified = modified;
	while (off < len) {
		siz = MAX_IMSGSIZE - sizeof(ans);
		if (len - off > siz) {
			ans.final = true;
			ans.datalen = siz;
		} else {
			ans.final = true;
			ans.datalen = len - off;
		}
		iov[0].iov_base = &ans;
		iov[0].iov_len = sizeof(ans);
		iov[1].iov_base = (u_char *)pkt + off;
		iov[1].iov_len = ans.datalen;
		ret = imsg_composev(&base->ibuf, imsg_type, 0, 0, -1, iov, 2);
		if (ret == -1)
			break;
		off += ans.datalen;
	}
	module_reset_event(base);

	return (ret);
}

static int
module_recv_imsg(struct module_base *base)
{
	ssize_t		 n;
	struct imsg	 imsg;

	if ((n = imsg_read(&base->ibuf)) == -1 || n == 0) {
		/* XXX */
		return (-1);
	}

	for (;;) {
		if ((n = imsg_get(&base->ibuf, &imsg)) == -1)
			/* XXX */
			return (-1);
		if (n == 0)
			break;
		module_imsg_handler(base, &imsg);
		imsg_free(&imsg);
	}
	module_reset_event(base);

	return (0);
}

static int
module_imsg_handler(struct module_base *base, struct imsg *imsg)
{
	ssize_t	 datalen;

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	switch (imsg->hdr.type) {
	case IMSG_RADIUSD_MODULE_SET_CONFIG:
	    {
		struct radiusd_module_set_arg	 *arg;
		struct radiusd_module_object	 *val;
		u_int				  i;
		size_t				  off;
		char				**argv;

		arg = (struct radiusd_module_set_arg *)imsg->data;
		off = sizeof(struct radiusd_module_set_arg);

		if ((argv = calloc(sizeof(const char *), arg->nparamval))
		    == NULL) {
			module_send_message(base, IMSG_NG,
			    "Out of memory: %s", strerror(errno));
			break;
		}
		for (i = 0; i < arg->nparamval; i++) {
			if (datalen - off <
			    sizeof(struct radiusd_module_object))
				break;
			val = (struct radiusd_module_object *)
			    ((caddr_t)imsg->data + off);
			if (datalen - off < val->size)
				break;
			argv[i] = (char *)(val + 1);
			off += val->size;
		}
		if (i >= arg->nparamval)
			module_config_set(base->ctx, arg->paramname,
			    arg->nparamval, argv);
		else
			module_send_message(base, IMSG_NG,
			    "Internal protocol error");
		free(argv);

		break;
	    }
	case IMSG_RADIUSD_MODULE_START:
		if (module_start_module != NULL) {
			module_start_module(base->ctx);
			if (!base->priv_dropped) {
				syslog(LOG_ERR, "Module tried to start with "
				    "root priviledge");
				abort();
			}
		} else {
			if (!base->priv_dropped) {
				syslog(LOG_ERR, "Module tried to start with "
				    "root priviledge");
				abort();
			}
			module_send_message(base, IMSG_OK, NULL);
		}
		break;
	case IMSG_RADIUSD_MODULE_STOP:
		if (module_stop_module != NULL)
			module_stop_module(base->ctx);
		break;
	case IMSG_RADIUSD_MODULE_USERPASS:
	    {
		struct radiusd_module_userpass_arg *userpass;

		if (module_userpass == NULL) {
			syslog(LOG_ERR, "Received USERPASS message, but "
			    "module doesn't support");
			break;
		}
		if (datalen <
		    (ssize_t)sizeof(struct radiusd_module_userpass_arg)) {
			syslog(LOG_ERR, "Received USERPASS message, but "
			    "length is wrong");
			break;
		}
		userpass = (struct radiusd_module_userpass_arg *)imsg->data;
		module_userpass(base->ctx, userpass->q_id, userpass->user,
		    (userpass->has_pass)? userpass->pass : NULL);
		explicit_bzero(userpass,
		    sizeof(struct radiusd_module_userpass_arg));
		break;
	    }
	case IMSG_RADIUSD_MODULE_ACCSREQ:
	    {
		struct radiusd_module_radpkt_arg	*accessreq;
		int					 chunklen;

		if (module_access_request == NULL) {
			syslog(LOG_ERR, "Received ACCSREQ message, but "
			    "module doesn't support");
			break;
		}
		if (datalen <
		    (ssize_t)sizeof(struct radiusd_module_radpkt_arg)) {
			syslog(LOG_ERR, "Received ACCSREQ message, but "
			    "length is wrong");
			break;
		}
		accessreq = (struct radiusd_module_radpkt_arg *)imsg->data;
		if (base->radpktsiz < accessreq->datalen) {
			u_char *nradpkt;
			if ((nradpkt = realloc(base->radpkt,
			    accessreq->datalen)) == NULL) {
				syslog(LOG_ERR, "Could not handle received "
				    "ACCSREQ message: %m");
				base->radpktoff = 0;
				goto accsreq_out;
			}
			base->radpkt = nradpkt;
			base->radpktsiz = accessreq->datalen;
		}
		chunklen = datalen - sizeof(struct radiusd_module_radpkt_arg);
		if (chunklen > base->radpktsiz - base->radpktoff){
			syslog(LOG_ERR,
			    "Could not handle received ACCSREQ message: "
			    "received length is too big");
			base->radpktoff = 0;
			goto accsreq_out;
		}
		memcpy(base->radpkt + base->radpktoff,
		    (caddr_t)(accessreq + 1), chunklen);
		base->radpktoff += chunklen;
		if (!accessreq->final)
			goto accsreq_out;
		if (base->radpktoff != base->radpktsiz) {
			syslog(LOG_ERR,
			    "Could not handle received ACCSREQ "
			    "message: length is mismatch");
			base->radpktoff = 0;
			goto accsreq_out;
		}
		module_access_request(base->ctx, accessreq->q_id,
		    base->radpkt, base->radpktoff);
		base->radpktoff = 0;
accsreq_out:
		break;
	    }
	}

	return (0);
}

#ifdef USE_LIBEVENT
static void
module_on_event(int fd, short evmask, void *ctx)
{
	struct module_base	*base = ctx;
	int			 ret;

	base->ev_onhandler = true;
	if (evmask & EV_WRITE)
		base->writeready = true;
	if (evmask & EV_READ) {
		ret = module_recv_imsg(base);
		if (ret < 0)
			goto on_error;
	}
	while (base->writeready && base->ibuf.w.queued) {
		ret = msgbuf_write(&base->ibuf.w);
		if (ret > 0)
			continue;
		base->writeready = false;
		if (ret == 0 && errno == EAGAIN)
			break;
		syslog(LOG_ERR, "Write fail");
		goto on_error;
	}
	base->ev_onhandler = false;
	module_reset_event(base);
	return;

on_error:
	if (event_initialized(&base->ev))
		event_del(&base->ev);
	close(base->ibuf.fd);
}
#endif

static void
module_reset_event(struct module_base *base)
{
#ifdef USE_LIBEVENT
	short		 evmask = 0;
	struct timeval	*tvp = NULL, tv = { 0, 0 };

	if (base->ev_onhandler)
		return;
	if (event_initialized(&base->ev))
		event_del(&base->ev);

	evmask |= EV_READ;
	if (base->ibuf.w.queued) {
		if (!base->writeready)
			evmask |= EV_WRITE;
		else
			tvp = &tv;	/* fire immediately */
	}
	event_set(&base->ev, base->ibuf.fd, evmask, module_on_event, base);
	if (event_add(&base->ev, tvp) == -1)
		syslog(LOG_ERR, "event_add() failed in %s()", __func__);
#endif
}
