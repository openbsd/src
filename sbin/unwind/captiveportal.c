/*	$OpenBSD: captiveportal.c,v 1.12 2019/05/14 14:51:31 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/syslog.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <net/route.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "log.h"
#include "unwind.h"
#include "captiveportal.h"

enum http_global_state {
	IDLE,
	READING
};

enum http_state {
	INIT,
	SENT_QUERY,
	HEADER_READ
};

struct http_ctx {
	TAILQ_ENTRY(http_ctx)	 entry;
	struct event		 ev;
	int			 fd;
	enum http_state		 state;
	char			*buf;
	size_t			 bufsz;
	int			 status;
	int			 content_length;
};

__dead void	 captiveportal_shutdown(void);
void		 captiveportal_sig_handler(int, short, void *);
void		 captiveportal_startup(void);
void		 http_callback(int, short, void *);
int		 parse_http_header(struct http_ctx *);
void		 check_http_body(struct http_ctx *ctx);
void		 free_http_ctx(struct http_ctx *);
void		 close_other_http_contexts(struct http_ctx *);

struct uw_conf	*captiveportal_conf;
struct imsgev	*iev_main;
struct imsgev	*iev_resolver;
struct imsgev	*iev_frontend;

#define MAX_SERVERS_DNS	 8
enum http_global_state	 http_global_state = IDLE;
TAILQ_HEAD(, http_ctx)	 http_contexts;
int			 http_contexts_count;

struct timeval		 tv = {5, 0};

void
captiveportal_sig_handler(int sig, short event, void *bula)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		captiveportal_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
captiveportal(int debug, int verbose)
{
	struct event	 ev_sigint, ev_sigterm;
	struct passwd	*pw;

	captiveportal_conf = config_new_empty();

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(UNWIND_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	uw_process = PROC_CAPTIVEPORTAL;
	setproctitle("%s", log_procnames[uw_process]);
	log_procinit(log_procnames[uw_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio recvfd", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, captiveportal_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, captiveportal_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the parent process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = captiveportal_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	TAILQ_INIT(&http_contexts);

	event_dispatch();

	captiveportal_shutdown();
}

__dead void
captiveportal_shutdown(void)
{
	/* Close pipes. */
	msgbuf_write(&iev_resolver->ibuf.w);
	msgbuf_clear(&iev_resolver->ibuf.w);
	close(iev_resolver->ibuf.fd);
	msgbuf_write(&iev_frontend->ibuf.w);
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(captiveportal_conf);

	free(iev_resolver);
	free(iev_frontend);
	free(iev_main);

	log_info("captiveportal exiting");
	exit(0);
}

int
captiveportal_imsg_compose_main(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
captiveportal_imsg_compose_resolver(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_resolver, type, 0, pid, -1, data,
	    datalen));
}

int
captiveportal_imsg_compose_frontend(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_frontend, type, 0, pid, -1, data,
	    datalen));
}

void
captiveportal_dispatch_main(int fd, short event, void *bula)
{
	static struct uw_conf	*nconf;
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct http_ctx		*ctx;
	int			 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC_RESOLVER:
			/*
			 * Setup pipe and event handler to the resolver
			 * process.
			 */
			if (iev_resolver) {
				fatalx("%s: received unexpected imsg fd "
				    "to captiveportal", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				fatalx("%s: expected to receive imsg fd to "
				   "captiveportal but didn't receive any",
				   __func__);
				break;
			}

			iev_resolver = malloc(sizeof(struct imsgev));
			if (iev_resolver == NULL)
				fatal(NULL);

			imsg_init(&iev_resolver->ibuf, fd);
			iev_resolver->handler = captiveportal_dispatch_resolver;
			iev_resolver->events = EV_READ;

			event_set(&iev_resolver->ev, iev_resolver->ibuf.fd,
			    iev_resolver->events, iev_resolver->handler,
			    iev_resolver);
			event_add(&iev_resolver->ev, NULL);
			break;
		case IMSG_SOCKET_IPC_FRONTEND:
			/*
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend) {
				fatalx("%s: received unexpected imsg fd "
				    "to frontend", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				fatalx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);
				break;
			}

			iev_frontend = malloc(sizeof(struct imsgev));
			if (iev_frontend == NULL)
				fatal(NULL);

			imsg_init(&iev_frontend->ibuf, fd);
			iev_frontend->handler = captiveportal_dispatch_frontend;
			iev_frontend->events = EV_READ;

			event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
			    iev_frontend->events, iev_frontend->handler,
			    iev_frontend);
			event_add(&iev_frontend->ev, NULL);
			break;
		case IMSG_RECONF_CONF:
		case IMSG_RECONF_CAPTIVE_PORTAL_HOST:
		case IMSG_RECONF_CAPTIVE_PORTAL_PATH:
		case IMSG_RECONF_CAPTIVE_PORTAL_EXPECTED_RESPONSE:
		case IMSG_RECONF_BLOCKLIST_FILE:
		case IMSG_RECONF_FORWARDER:
		case IMSG_RECONF_DOT_FORWARDER:
			imsg_receive_config(&imsg, &nconf);
			break;
		case IMSG_RECONF_END:
			if (nconf == NULL)
				fatalx("%s: IMSG_RECONF_END without "
				    "IMSG_RECONF_CONF", __func__);
			merge_config(captiveportal_conf, nconf);
			nconf = NULL;
			break;
		case IMSG_HTTPSOCK:
			if ((fd = imsg.fd) == -1) {
				fatalx("%s: expected to receive imsg fd to "
				   "captiveportal but didn't receive any",
				   __func__);
				break;
			}

			if (http_global_state == READING ||
			    http_contexts_count >= MAX_SERVERS_DNS) {
				/* don't try more servers */
				close(fd);
				break;
			}

			if ((ctx = malloc(sizeof(*ctx))) == NULL) {
				close(fd);
				break;
			}

			ctx->state = INIT;
			ctx->fd = fd;
			ctx->bufsz = 0;
			ctx->buf = NULL;
			ctx->status = -1;
			ctx->content_length = -1;

			event_set(&ctx->ev, fd, EV_READ | EV_WRITE |
			    EV_PERSIST, http_callback, ctx);
			event_add(&ctx->ev, &tv);

			TAILQ_INSERT_TAIL(&http_contexts, ctx, entry);

			http_contexts_count++;

			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
captiveportal_dispatch_resolver(int fd, short event, void *bula)
{
	struct imsgev	*iev = bula;
	struct imsgbuf	*ibuf = &iev->ibuf;
	struct imsg	 imsg;
	int		 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
captiveportal_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev	*iev = bula;
	struct imsgbuf	*ibuf = &iev->ibuf;
	struct imsg	 imsg;
	int		 n, verbose, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_LOG_VERBOSE:
			if (IMSG_DATA_SIZE(imsg) != sizeof(verbose))
				fatalx("%s: IMSG_CTL_LOG_VERBOSE wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
http_callback(int fd, short events, void *arg)
{
	struct http_ctx	*ctx;
	ssize_t		 n;
	char		*query, buf[512], *vis_str, *p, *ep;

	ctx = (struct http_ctx *)arg;

	if (events & EV_TIMEOUT) {
		log_debug("%s: TIMEOUT", __func__);
		goto err;
	}

	if (events & EV_READ) {
		if ((n = read(fd, buf, sizeof(buf))) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				return;
			else {
				log_warn("%s: read", __func__);
				if (http_global_state == READING)
					http_global_state = IDLE;
				goto err;
			}
		}

		if (http_contexts_count > 1)
			close_other_http_contexts(ctx);
		http_global_state = READING;

		if (n == 0) {
			check_http_body(ctx);
			return;
		}
		p = recallocarray(ctx->buf, ctx->bufsz, ctx->bufsz + n, 1);
		if (p == NULL) {
			log_warn("%s", __func__);
			goto err;
		}
		ctx->buf = p;
		memcpy(ctx->buf + ctx->bufsz, buf, n);
		ctx->bufsz += n;

		if (ctx->state == HEADER_READ && ctx->content_length != -1 &&
		    ctx->bufsz >= (size_t)ctx->content_length) {
			check_http_body(ctx);
			return;
		}

		if (ctx->state == SENT_QUERY) {
			ep = memmem(ctx->buf, ctx->bufsz, "\r\n\r\n", 4);
			if (ep != NULL) {
				ctx->state = HEADER_READ;
				*ep = '\0';
				if (strlen(ctx->buf) != (uintptr_t)
				    (ep - ctx->buf)) {
					log_warnx("binary data in header");
					goto err;
				}
				stravis(&vis_str, ctx->buf,
				    VIS_NL | VIS_CSTYLE);
				log_debug("header\n%s", vis_str);
				free(vis_str);

				if (parse_http_header(ctx) != 0)
					goto err;

				p = ctx->buf;
				ep += 4;
				ctx->bufsz = (ctx->buf + ctx->bufsz) - ep;
				ctx->buf = malloc(ctx->bufsz);
				memcpy(ctx->buf, ep, ctx->bufsz);
				free(p);
			}
		}
	}

	if (events & EV_WRITE) {
		if (ctx->state == INIT) {
			n = asprintf(&query,
			    "GET %s HTTP/1.1\r\nHost: %s\r\n"
			    "Connection: close\r\n\r\n",
			    captiveportal_conf->captive_portal_path,
			    captiveportal_conf->captive_portal_host);
			write(fd, query, n);
			free(query);
			event_del(&ctx->ev);
			event_set(&ctx->ev, fd, EV_READ | EV_PERSIST,
			    http_callback, ctx);
			event_add(&ctx->ev, &tv);
			ctx->state = SENT_QUERY;
		} else {
			log_warnx("invalid state: %d", ctx->state);
			goto err;
		}
	}
	return;
err:
	free_http_ctx(ctx);
}

int
parse_http_header(struct http_ctx *ctx)
{
	char		*p, *ep;
	const char	*errstr;

	/* scan past HTTP/1.x */
	p = strchr(ctx->buf, ' ');
	if (p == NULL)
		return (1);
	while (isspace((int)*p))
		p++;
	ep = strchr(p, ' ');
	if (ep == NULL)
		return (1);
	*ep = '\0';
	ctx->status = strtonum(p, 100, 599, &errstr);
	if (errstr != NULL) {
		log_warnx("%s: status is %s: %s", __func__, errstr, p);
		return (1);
	}

	log_debug("%s: status: %d", __func__, ctx->status);

	/* ignore parse errors from here on out, we got the status */

	p = strcasestr(ep + 1, "Content-Length:");
	if (p == NULL)
		return (0);

	p += sizeof("Content-Length:") - 1;
	while (isspace((int)*p))
		p++;

	ep = strchr(p, '\r');
	if (ep == NULL)
		return (0);

	*ep = '\0';
	ctx->content_length = strtonum(p, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		log_warnx("%s: Content-Lenght is %s: %s", __func__, errstr, p);
		ctx->content_length = -1;
		return (0);
	}
	log_debug("content-length: %d", ctx->content_length);
	return (0);
}

void
check_http_body(struct http_ctx *ctx)
{
	enum captive_portal_state	 state;
	char				*p, *vis_str;

	p = recallocarray(ctx->buf, ctx->bufsz, ctx->bufsz + 1, 1);
	if (p == NULL) {
		log_warn("%s", __func__);
		free_http_ctx(ctx);
		return;
	}
	ctx->buf = p;
	*(ctx->buf + ctx->bufsz) = '\0';
	ctx->bufsz++;
	stravis(&vis_str, ctx->buf, VIS_NL | VIS_CSTYLE);
	log_debug("body[%ld]\n%s", ctx->bufsz, vis_str);

	if (ctx->status == captiveportal_conf->captive_portal_expected_status &&
	    strcmp(vis_str,
	    captiveportal_conf->captive_portal_expected_response) == 0) {
		log_debug("%s: not behind captive portal", __func__);
		state = NOT_BEHIND;
	} else {
		log_debug("%s: behind captive portal", __func__);
		state = BEHIND;
	}
	captiveportal_imsg_compose_resolver(IMSG_CAPTIVEPORTAL_STATE, 0,
	    &state, sizeof(state));
	free_http_ctx(ctx);
	http_global_state = IDLE;
}

void
free_http_ctx(struct http_ctx *ctx)
{
	if (ctx == NULL)
		return;

	event_del(&ctx->ev);
	close(ctx->fd);
	TAILQ_REMOVE(&http_contexts, ctx, entry);
	free(ctx->buf);
	free(ctx);
	http_contexts_count--;
}

void
close_other_http_contexts(struct http_ctx *octx)
{
	struct http_ctx	*ctx, *t;

	log_debug("%s", __func__);
	TAILQ_FOREACH_SAFE(ctx, &http_contexts, entry, t)
		if(ctx != octx)
			free_http_ctx(ctx);
}
