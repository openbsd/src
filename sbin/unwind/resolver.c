/*	$OpenBSD: resolver.c,v 1.39 2019/05/10 14:10:38 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/time.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tls.h>
#include <unistd.h>

#include "libunbound/config.h"
#include "libunbound/libunbound/libworker.h"
#include "libunbound/libunbound/unbound.h"
#include "libunbound/libunbound/unbound-event.h"
#include "libunbound/sldns/sbuffer.h"
#include "libunbound/sldns/rrdef.h"
#include "libunbound/sldns/pkthdr.h"
#include "libunbound/sldns/wire2str.h"
#include "libunbound/util/regional.h"

#include <openssl/crypto.h>

#include "captiveportal.h"
#include "log.h"
#include "frontend.h"
#include "unwind.h"
#include "resolver.h"

#define	UB_LOG_VERBOSE			4
#define	UB_LOG_BRIEF			0

#define	RESOLVER_CHECK_SEC		1
#define	RESOLVER_CHECK_MAXSEC		1024 /* ~17 minutes */

#define	PORTAL_CHECK_SEC		15
#define	PORTAL_CHECK_MAXSEC		600

#define	TRUST_ANCHOR_RETRY_INTERVAL	8640
#define	TRUST_ANCHOR_QUERY_INTERVAL	43200

struct uw_resolver {
	struct event		 check_ev;
	struct event		 free_ev;
	struct ub_ctx		*ctx;
	struct timeval		 check_tv;
	int			 ref_cnt;
	int			 stop;
	enum uw_resolver_state	 state;
	enum uw_resolver_type	 type;
	char			*why_bogus;
	int64_t			 histogram[nitems(histogram_limits)];
};

struct check_resolver_data {
	struct uw_resolver	*res;
	struct uw_resolver	*check_res;
};

__dead void		 resolver_shutdown(void);
void			 resolver_sig_handler(int sig, short, void *);
void			 resolver_dispatch_frontend(int, short, void *);
void			 resolver_dispatch_captiveportal(int, short, void *);
void			 resolver_dispatch_main(int, short, void *);
void			 resolve_done(void *, int, void *, int, int, char *,
			     int);
void			 parse_dhcp_forwarders(char *);
void			 new_recursor(void);
void			 new_forwarders(void);
void			 new_static_forwarders(void);
void			 new_static_dot_forwarders(void);
struct uw_resolver	*create_resolver(enum uw_resolver_type);
void			 free_resolver(struct uw_resolver *);
void			 set_forwarders(struct uw_resolver *,
			     struct uw_forwarder_head *);
void			 resolver_check_timo(int, short, void *);
void			 resolver_free_timo(int, short, void *);
void			 check_resolver(struct uw_resolver *);
void			 check_resolver_done(void *, int, void *, int, int,
			     char *, int);
void			 schedule_recheck_all_resolvers(void);
int			 check_forwarders_changed(struct uw_forwarder_head *,
			     struct uw_forwarder_head *);
void			 replace_forwarders(struct uw_forwarder_head *,
			     struct uw_forwarder_head *);
void			 resolver_ref(struct uw_resolver *);
void			 resolver_unref(struct uw_resolver *);
struct uw_resolver	*best_resolver(void);
int			 resolver_cmp(struct uw_resolver *,
			     struct uw_resolver *);
void			 restart_resolvers(void);
void			 show_status(enum uw_resolver_type, pid_t);
void			 send_resolver_info(struct uw_resolver *, int, pid_t);
void			 send_detailed_resolver_info(struct uw_resolver *,
			     pid_t);
void			 send_resolver_histogram_info(struct uw_resolver *,
			     pid_t);
void			 check_captive_portal(int);
void			 check_captive_portal_timo(int, short, void *);
int			 check_captive_portal_changed(struct uw_conf *,
			     struct uw_conf *);
void			 trust_anchor_resolve(void);
void			 trust_anchor_timo(int, short, void *);
void			 trust_anchor_resolve_done(void *, int, void *, int,
			     int, char *, int);

struct uw_conf			*resolver_conf;
struct imsgev			*iev_frontend;
struct imsgev			*iev_captiveportal;
struct imsgev			*iev_main;
struct uw_forwarder_head	 dhcp_forwarder_list;
struct uw_resolver		*resolvers[UW_RES_NONE];
struct timeval			 captive_portal_check_tv =
				     {PORTAL_CHECK_SEC, 0};
struct event			 captive_portal_check_ev;

struct event			 trust_anchor_timer;

static struct trust_anchor_head	 trust_anchors, new_trust_anchors;

struct event_base		*ev_base;

enum uw_resolver_state		 global_state = DEAD;
enum captive_portal_state	 captive_portal_state = PORTAL_UNCHECKED;

void
resolver_sig_handler(int sig, short event, void *arg)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		resolver_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
resolver(int debug, int verbose)
{
	struct event	 ev_sigint, ev_sigterm;
	struct passwd	*pw;

	resolver_conf = config_new_empty();

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(UNWIND_USER)) == NULL)
		fatal("getpwnam");

	uw_process = PROC_RESOLVER;
	setproctitle("%s", log_procnames[uw_process]);
	log_procinit(log_procnames[uw_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (unveil(tls_default_ca_cert_file(), "r") == -1)
		fatal("unveil");

	if (pledge("stdio inet dns rpath recvfd", NULL) == -1)
		fatal("pledge");

	ev_base = event_init();

	/* Setup signal handler(s). */
	signal_set(&ev_sigint, SIGINT, resolver_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, resolver_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the main process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);

	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = resolver_dispatch_main;

	/* Setup event handlers. */
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	evtimer_set(&captive_portal_check_ev, check_captive_portal_timo, NULL);
	evtimer_set(&trust_anchor_timer, trust_anchor_timo, NULL);

	new_recursor();

	SIMPLEQ_INIT(&dhcp_forwarder_list);
	TAILQ_INIT(&trust_anchors);
	TAILQ_INIT(&new_trust_anchors);

	event_dispatch();

	resolver_shutdown();
}

__dead void
resolver_shutdown(void)
{
	log_debug("%s", __func__);

	/* Close pipes. */
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_clear(&iev_captiveportal->ibuf.w);
	close(iev_captiveportal->ibuf.fd);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(resolver_conf);

	free(iev_frontend);
	free(iev_captiveportal);
	free(iev_main);

	log_info("resolver exiting");
	exit(0);
}

int
resolver_imsg_compose_main(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data, datalen));
}

int
resolver_imsg_compose_frontend(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_frontend, type, 0, pid, -1,
	    data, datalen));
}

int
resolver_imsg_compose_captiveportal(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_captiveportal, type, 0, pid, -1,
	    data, datalen));
}

void
resolver_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	struct query_imsg	*query_imsg;
	struct uw_resolver	*res;
	enum uw_resolver_type	 type;
	ssize_t			 n;
	int			 shut = 0, verbose, err;
	int			 update_resolvers;
	char			*ta;

	ibuf = &iev->ibuf;

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
				    "%lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&verbose, imsg.data, sizeof(verbose));
			update_resolvers = (log_getverbose() & OPT_VERBOSE2)
			    != (verbose & OPT_VERBOSE2);
			log_setverbose(verbose);
			if (update_resolvers)
				restart_resolvers();
			break;
		case IMSG_QUERY:
			if (IMSG_DATA_SIZE(imsg) != sizeof(*query_imsg))
				fatalx("%s: IMSG_QUERY wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			query_imsg = malloc(sizeof(*query_imsg)); /* XXX */
			memcpy(query_imsg, imsg.data, sizeof(*query_imsg));

			log_debug("%s: IMSG_QUERY[%llu], qname: %s, t: %d, "
			    "c: %d", __func__, query_imsg->id,
			    query_imsg->qname, query_imsg->t, query_imsg->c);

			res = best_resolver();

			if (res == NULL) {
				log_warnx("can't find working resolver");
				break;
			}

			log_debug("%s: choosing %s", __func__,
			    uw_resolver_type_str[res->type]);

			query_imsg->resolver = res;
			resolver_ref(res);

			clock_gettime(CLOCK_MONOTONIC, &query_imsg->tp);

			if ((err = ub_resolve_event(res->ctx,
			    query_imsg->qname, query_imsg->t, query_imsg->c,
			    query_imsg, resolve_done,
			    &query_imsg->async_id)) != 0) {
				log_warn("%s: ub_resolve_async: err: %d, %s",
				    __func__, err, ub_strerror(err));
				resolver_unref(res);
			}
			break;
		case IMSG_FORWARDER:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			parse_dhcp_forwarders(imsg.data);
			break;
		case IMSG_CTL_STATUS:
			if (IMSG_DATA_SIZE(imsg) != sizeof(type))
				fatalx("%s: IMSG_CTL_STATUS wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&type, imsg.data, sizeof(type));
			show_status(type, imsg.hdr.pid);
			break;
		case IMSG_CTL_RECHECK_CAPTIVEPORTAL:
			check_captive_portal(1);
			break;
		case IMSG_NEW_TA:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			ta = imsg.data;
			add_new_ta(&new_trust_anchors, ta);
			break;
		case IMSG_NEW_TAS_ABORT:
			log_debug("%s: IMSG_NEW_TAS_ABORT", __func__);
			free_tas(&new_trust_anchors);
			break;
		case IMSG_NEW_TAS_DONE:
			log_debug("%s: IMSG_NEW_TAS_DONE", __func__);
			if (merge_tas(&new_trust_anchors, &trust_anchors)) {
				new_recursor();
				new_forwarders();
				new_static_forwarders();
				new_static_dot_forwarders();
			}
			break;
		case IMSG_RECHECK_RESOLVERS:
			schedule_recheck_all_resolvers();
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
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
resolver_dispatch_captiveportal(int fd, short event, void *bula)
{
	struct imsgev	*iev = bula;
	struct imsgbuf	*ibuf;
	struct imsg	 imsg;
	ssize_t		 n;
	int		 shut = 0;


	ibuf = &iev->ibuf;

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
		case IMSG_CAPTIVEPORTAL_STATE:
			if (IMSG_DATA_SIZE(imsg) !=
			    sizeof(captive_portal_state))
				fatalx("%s: IMSG_CAPTIVEPORTAL_STATE wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&captive_portal_state, imsg.data,
			    sizeof(captive_portal_state));
			log_debug("%s: IMSG_CAPTIVEPORTAL_STATE: %s", __func__,
			    captive_portal_state_str[captive_portal_state]);

			if (captive_portal_state == NOT_BEHIND) {
				evtimer_del(&captive_portal_check_ev);
				schedule_recheck_all_resolvers();
			}

			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
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
resolver_dispatch_main(int fd, short event, void *bula)
{
	static struct uw_conf	*nconf;
	struct uw_forwarder	*uw_forwarder;
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	ssize_t			 n;
	int			 shut = 0, forwarders_changed;
	int			 dot_forwarders_changed;
	int			 captive_portal_changed;

	ibuf = &iev->ibuf;

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
		case IMSG_SOCKET_IPC_FRONTEND:
			/*
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend)
				fatalx("%s: received unexpected imsg fd "
				    "to resolver", __func__);

			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg fd to "
				   "resolver but didn't receive any", __func__);

			iev_frontend = malloc(sizeof(struct imsgev));
			if (iev_frontend == NULL)
				fatal(NULL);

			imsg_init(&iev_frontend->ibuf, fd);
			iev_frontend->handler = resolver_dispatch_frontend;
			iev_frontend->events = EV_READ;

			event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
			iev_frontend->events, iev_frontend->handler,
			    iev_frontend);
			event_add(&iev_frontend->ev, NULL);
			break;
		case IMSG_SOCKET_IPC_CAPTIVEPORTAL:
			/*
			 * Setup pipe and event handler to the captiveportal
			 * process.
			 */
			if (iev_captiveportal)
				fatalx("%s: received unexpected imsg fd "
				    "to resolver", __func__);

			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg fd to "
				   "resolver but didn't receive any", __func__);

			iev_captiveportal = malloc(sizeof(struct imsgev));
			if (iev_captiveportal == NULL)
				fatal(NULL);

			imsg_init(&iev_captiveportal->ibuf, fd);
			iev_captiveportal->handler =
			    resolver_dispatch_captiveportal;
			iev_captiveportal->events = EV_READ;

			event_set(&iev_captiveportal->ev,
			    iev_captiveportal->ibuf.fd,
			iev_captiveportal->events, iev_captiveportal->handler,
			    iev_captiveportal);
			event_add(&iev_captiveportal->ev, NULL);
			break;
		case IMSG_STARTUP:
			if (pledge("stdio inet dns rpath", NULL) == -1)
				fatal("pledge");
			break;
		case IMSG_RECONF_CONF:
			if (nconf != NULL)
				fatalx("%s: IMSG_RECONF_CONF already in "
				    "progress", __func__);
			if (IMSG_DATA_SIZE(imsg) != sizeof(struct uw_conf))
				fatalx("%s: IMSG_RECONF_CONF wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			if ((nconf = malloc(sizeof(struct uw_conf))) == NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct uw_conf));
			nconf->captive_portal_host = NULL;
			nconf->captive_portal_path = NULL;
			nconf->captive_portal_expected_response = NULL;
			SIMPLEQ_INIT(&nconf->uw_forwarder_list);
			SIMPLEQ_INIT(&nconf->uw_dot_forwarder_list);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_HOST:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			if ((nconf->captive_portal_host = strdup(imsg.data)) ==
			    NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_PATH:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			if ((nconf->captive_portal_path = strdup(imsg.data)) ==
			    NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_CAPTIVE_PORTAL_EXPECTED_RESPONSE:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			if ((nconf->captive_portal_expected_response =
			    strdup(imsg.data)) == NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_BLOCKLIST_FILE:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			if ((nconf->blocklist_file = strdup(imsg.data)) ==
			    NULL)
				fatal("%s: strdup", __func__);
			break;
		case IMSG_RECONF_FORWARDER:
			if (IMSG_DATA_SIZE(imsg) != sizeof(struct uw_forwarder))
				fatalx("%s: IMSG_RECONF_FORWARDER wrong length:"
				    " %lu", __func__, IMSG_DATA_SIZE(imsg));
			if ((uw_forwarder = malloc(sizeof(struct
			    uw_forwarder))) == NULL)
				fatal(NULL);
			memcpy(uw_forwarder, imsg.data, sizeof(struct
			    uw_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->uw_forwarder_list,
			    uw_forwarder, entry);
			break;
		case IMSG_RECONF_DOT_FORWARDER:
			if (IMSG_DATA_SIZE(imsg) != sizeof(struct uw_forwarder))
				fatalx("%s: IMSG_RECONF_DOT_FORWARDER wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			if ((uw_forwarder = malloc(sizeof(struct
			    uw_forwarder))) == NULL)
				fatal(NULL);
			memcpy(uw_forwarder, imsg.data, sizeof(struct
			    uw_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->uw_dot_forwarder_list,
			    uw_forwarder, entry);
			break;
		case IMSG_RECONF_END:
			if (nconf == NULL)
				fatalx("%s: IMSG_RECONF_END without "
				    "IMSG_RECONF_CONF", __func__);
			forwarders_changed = check_forwarders_changed(
			    &resolver_conf->uw_forwarder_list,
			    &nconf->uw_forwarder_list);
			dot_forwarders_changed = check_forwarders_changed(
			    &resolver_conf->uw_dot_forwarder_list,
			    &nconf->uw_dot_forwarder_list);
			captive_portal_changed = check_captive_portal_changed(
			    resolver_conf, nconf);
			merge_config(resolver_conf, nconf);
			nconf = NULL;
			if (forwarders_changed) {
				log_debug("static forwarders changed");
				new_static_forwarders();
			}
			if (dot_forwarders_changed) {
				log_debug("static DoT forwarders changed");
				new_static_dot_forwarders();
			}
			if (resolver_conf->captive_portal_host == NULL)
				captive_portal_state = PORTAL_UNCHECKED;
			else if (captive_portal_state == PORTAL_UNCHECKED ||
			    captive_portal_changed) {
				if (resolver_conf->captive_portal_auto)
					check_captive_portal(1);
			}
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
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
resolve_done(void *arg, int rcode, void *answer_packet, int answer_len,
    int sec, char *why_bogus, int was_ratelimited)
{
	struct query_imsg	*query_imsg;
	struct uw_resolver	*res;
	struct timespec		 tp, elapsed;
	int64_t			 ms;
	size_t			 i;
	char			*str;

	clock_gettime(CLOCK_MONOTONIC, &tp);

	query_imsg = (struct query_imsg *)arg;
	res = (struct uw_resolver *)query_imsg->resolver;

	timespecsub(&tp, &query_imsg->tp, &elapsed);

	log_debug("elapsed: %lld.%ld", elapsed.tv_sec, elapsed.tv_nsec);

	ms = elapsed.tv_sec * 1000 + elapsed.tv_nsec / 1000000;

	for (i = 1; i < nitems(histogram_limits); i++) {
		if (ms > histogram_limits[i - 1] && ms < histogram_limits[i])
			break;
	}

	res->histogram[i]++;

	log_debug("%s: async_id: %d, ref_cnt: %d, elapsed: %lldms, "
	    "histogram: %lld - %lld", __func__, query_imsg->async_id,
	    res->ref_cnt, ms, histogram_limits[i], res->histogram[i]);

	log_debug("%s: rcode: %d", __func__, rcode);

	if (answer_len < LDNS_HEADER_SIZE) {
		log_warnx("bad packet: too short");
		goto servfail;
	}

	if (rcode == LDNS_RCODE_SERVFAIL) {
		if (res->stop != 1)
			check_resolver(res);
		goto servfail;
	}

	if ((str = sldns_wire2str_pkt(answer_packet, answer_len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

	query_imsg->err = 0;

	if (res->state == VALIDATING)
		query_imsg->bogus = sec == 1;
	else
		query_imsg->bogus = 0;
	resolver_imsg_compose_frontend(IMSG_ANSWER_HEADER, 0, query_imsg,
	    sizeof(*query_imsg));

	/* XXX imsg overflow */
	resolver_imsg_compose_frontend(IMSG_ANSWER, 0,
	    answer_packet, answer_len);

	free(query_imsg);
	resolver_unref(res);
	return;

servfail:
	query_imsg->err = -4; /* UB_SERVFAIL */
	resolver_imsg_compose_frontend(IMSG_ANSWER_HEADER, 0, query_imsg,
	    sizeof(*query_imsg));
	free(query_imsg);
	resolver_unref(res);
}

void
parse_dhcp_forwarders(char *forwarders)
{
	struct uw_forwarder_head	 new_forwarder_list;
	struct uw_forwarder		*uw_forwarder;
	char				*ns;

	SIMPLEQ_INIT(&new_forwarder_list);

	if (forwarders != NULL) {
		while((ns = strsep(&forwarders, ",")) != NULL) {
			log_debug("%s: %s", __func__, ns);
			if ((uw_forwarder = malloc(sizeof(struct
			    uw_forwarder))) == NULL)
				fatal(NULL);
			if (strlcpy(uw_forwarder->name, ns,
			    sizeof(uw_forwarder->name)) >=
			    sizeof(uw_forwarder->name))
				fatalx("strlcpy");
			SIMPLEQ_INSERT_TAIL(&new_forwarder_list, uw_forwarder,
			    entry);
		}
	}

	if (check_forwarders_changed(&new_forwarder_list,
	    &dhcp_forwarder_list)) {
		replace_forwarders(&new_forwarder_list, &dhcp_forwarder_list);
		new_forwarders();
		if (resolver_conf->captive_portal_auto)
			check_captive_portal(1);
	} else
		log_debug("%s: forwarders didn't change", __func__);
}

void
new_recursor(void)
{
	free_resolver(resolvers[UW_RES_RECURSOR]);
	resolvers[UW_RES_RECURSOR] = NULL;

	if (TAILQ_EMPTY(&trust_anchors))
		return;

	resolvers[UW_RES_RECURSOR] = create_resolver(UW_RES_RECURSOR);
	check_resolver(resolvers[UW_RES_RECURSOR]);
}

void
new_forwarders(void)
{
	free_resolver(resolvers[UW_RES_DHCP]);
	resolvers[UW_RES_DHCP] = NULL;

	if (SIMPLEQ_EMPTY(&dhcp_forwarder_list))
		return;

	if (TAILQ_EMPTY(&trust_anchors))
		return;

	log_debug("%s: create_resolver", __func__);
	resolvers[UW_RES_DHCP] = create_resolver(UW_RES_DHCP);

	check_resolver(resolvers[UW_RES_DHCP]);
}

void
new_static_forwarders(void)
{
	free_resolver(resolvers[UW_RES_FORWARDER]);
	resolvers[UW_RES_FORWARDER] = NULL;

	if (SIMPLEQ_EMPTY(&resolver_conf->uw_forwarder_list))
		return;

	if (TAILQ_EMPTY(&trust_anchors))
		return;

	log_debug("%s: create_resolver", __func__);
	resolvers[UW_RES_FORWARDER] = create_resolver(UW_RES_FORWARDER);

	check_resolver(resolvers[UW_RES_FORWARDER]);
}

void
new_static_dot_forwarders(void)
{
	free_resolver(resolvers[UW_RES_DOT]);
	resolvers[UW_RES_DOT] = NULL;

	if (SIMPLEQ_EMPTY(&resolver_conf->uw_dot_forwarder_list))
		return;

	if (TAILQ_EMPTY(&trust_anchors))
		return;

	log_debug("%s: create_resolver", __func__);
	resolvers[UW_RES_DOT] = create_resolver(UW_RES_DOT);

	check_resolver(resolvers[UW_RES_DOT]);
}

struct uw_resolver *
create_resolver(enum uw_resolver_type type)
{
	struct uw_resolver	*res;
	struct trust_anchor	*ta;
	int			 err;

	if ((res = calloc(1, sizeof(*res))) == NULL) {
		log_warn("%s", __func__);
		return (NULL);
	}

	res->type = type;

	log_debug("%s: %p", __func__, res);

	if ((res->ctx = ub_ctx_create_event(ev_base)) == NULL) {
		free(res);
		log_warnx("could not create unbound context");
		return (NULL);
	}

	res->state = UNKNOWN;
	res->check_tv.tv_sec = RESOLVER_CHECK_SEC;
	res->check_tv.tv_usec = arc4random() % 1000000; /* modulo bias is ok */

	ub_ctx_debuglevel(res->ctx, log_getverbose() & OPT_VERBOSE2 ?
	    UB_LOG_VERBOSE : UB_LOG_BRIEF);

	TAILQ_FOREACH(ta, &trust_anchors, entry) {
		if ((err = ub_ctx_add_ta(res->ctx, ta->ta)) != 0) {
			ub_ctx_delete(res->ctx);
			free(res);
			log_warnx("error adding trust anchor: %s",
			    ub_strerror(err));
			return (NULL);
		}
	}

	if((err = ub_ctx_set_option(res->ctx, "aggressive-nsec:", "yes"))
	    != 0) {
		ub_ctx_delete(res->ctx);
		free(res);
		log_warnx("error setting aggressive-nsec: yes: %s",
		    ub_strerror(err));
		return (NULL);
	}

	if (!log_getdebug()) {
		if((err = ub_ctx_set_option(res->ctx, "use-syslog:", "yes"))
		    != 0) {
			ub_ctx_delete(res->ctx);
			free(res);
			log_warnx("error setting use-syslog: yes: %s",
			    ub_strerror(err));
			return (NULL);
		}
	}

	evtimer_set(&res->check_ev, resolver_check_timo, res);

	switch(res->type) {
	case UW_RES_RECURSOR:
		break;
	case UW_RES_DHCP:
		set_forwarders(res, &dhcp_forwarder_list);
		break;
	case UW_RES_FORWARDER:
		set_forwarders(res, &resolver_conf->uw_forwarder_list);
		break;
	case UW_RES_DOT:
		set_forwarders(res, &resolver_conf->uw_dot_forwarder_list);
		ub_ctx_set_option(res->ctx, "tls-cert-bundle:",
		    tls_default_ca_cert_file());
		ub_ctx_set_tls(res->ctx, 1);
		break;
	default:
		fatalx("unknown resolver type %d", type);
		break;
	}

	return (res);
}

void
free_resolver(struct uw_resolver *res)
{
	if (res == NULL)
		return;

	log_debug("%s: [%p] ref_cnt: %d", __func__, res, res->ref_cnt);
	
	if (res->ref_cnt > 0)
		res->stop = 1;
	else {
		evtimer_del(&res->check_ev);
		ub_ctx_delete(res->ctx);
		free(res->why_bogus);
		free(res);
	}
}

void
set_forwarders(struct uw_resolver *res, struct uw_forwarder_head
    *uw_forwarder_list)
{
	struct uw_forwarder	*uw_forwarder;

	SIMPLEQ_FOREACH(uw_forwarder, uw_forwarder_list, entry)
		ub_ctx_set_fwd(res->ctx, uw_forwarder->name);
}

void
resolver_check_timo(int fd, short events, void *arg)
{
	check_resolver((struct uw_resolver *)arg);
}

void
resolver_free_timo(int fd, short events, void *arg)
{
	free_resolver((struct uw_resolver *)arg);
}

void
check_resolver(struct uw_resolver *res)
{
	struct uw_resolver		*check_res;
	struct check_resolver_data	*data;
	int				 err;

	log_debug("%s: create_resolver", __func__);
	if ((check_res = create_resolver(res->type)) == NULL)
		fatal("%s", __func__);
	if ((data = malloc(sizeof(*data))) == NULL)
		fatal("%s", __func__);

	resolver_ref(check_res);
	resolver_ref(res);
	data->check_res = check_res;
	data->res = res;

	if ((err = ub_resolve_event(check_res->ctx, ".",  LDNS_RR_TYPE_NS,
	    LDNS_RR_CLASS_IN, data,
	    check_resolver_done, NULL)) != 0) {
		log_warn("%s: ub_resolve_event: err: %d, %s", __func__, err,
		    ub_strerror(err));
		res->state = UNKNOWN;
		resolver_unref(check_res);
		resolver_unref(res);
		res->check_tv.tv_sec = RESOLVER_CHECK_SEC;
		evtimer_add(&res->check_ev, &res->check_tv);

		log_debug("%s: evtimer_add: %lld - %s: %s", __func__,
		    data->res->check_tv.tv_sec,
		    uw_resolver_type_str[data->res->type],
		    uw_resolver_state_str[data->res->state]);
	}
}

void
check_resolver_done(void *arg, int rcode, void *answer_packet, int answer_len,
    int sec, char *why_bogus, int was_ratelimited)
{
	struct check_resolver_data	*data;
	struct uw_resolver		*best;
	struct timeval			 tv = {0, 1};
	enum uw_resolver_state		 prev_state;
	char				*str;

	data = (struct check_resolver_data *)arg;

	log_debug("%s: rcode: %d", __func__, rcode);

	prev_state = data->res->state;

	if (answer_len < LDNS_HEADER_SIZE) {
		data->res->state = DEAD;
		log_warnx("bad packet: too short");
		goto out;
	}

	if (rcode == LDNS_RCODE_SERVFAIL) {
		data->res->state = DEAD;
		goto out;
	}

	if ((str = sldns_wire2str_pkt(answer_packet, answer_len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

	if (sec == 2) {
		data->res->state = VALIDATING;
		if (!(evtimer_pending(&trust_anchor_timer, NULL)))
			evtimer_add(&trust_anchor_timer, &tv);
	 } else if (rcode == LDNS_RCODE_NOERROR &&
	    LDNS_RCODE_WIRE((uint8_t*)answer_packet) == LDNS_RCODE_NOERROR) {
		log_debug("%s: why bogus: %s", __func__, why_bogus);
		data->res->state = RESOLVING;
		/* best effort */
		data->res->why_bogus = strdup(why_bogus);
	} else
		data->res->state = DEAD; /* we know the root exists */

out:
	if (!data->res->stop && data->res->state == DEAD) {
		if (prev_state == DEAD)
			data->res->check_tv.tv_sec *= 2;
		else
			data->res->check_tv.tv_sec = RESOLVER_CHECK_SEC;

		if (data->res->check_tv.tv_sec > RESOLVER_CHECK_MAXSEC)
			data->res->check_tv.tv_sec = RESOLVER_CHECK_MAXSEC;

		evtimer_add(&data->res->check_ev, &data->res->check_tv);

		log_debug("%s: evtimer_add: %lld - %s: %s", __func__,
		    data->res->check_tv.tv_sec,
		    uw_resolver_type_str[data->res->type],
		    uw_resolver_state_str[data->res->state]);
	}

	log_debug("%s: %s: %s", __func__,
	    uw_resolver_type_str[data->res->type],
	    uw_resolver_state_str[data->res->state]);

	log_debug("%s: %p - %p", __func__, data->res, data->res->ctx);

	resolver_unref(data->res);
	data->check_res->stop = 1; /* do not free in callback */
	resolver_unref(data->check_res);

	free(data);

	best = best_resolver();

	if (best->state != global_state) {
		if (best->state < RESOLVING && global_state > UNKNOWN)
			resolver_imsg_compose_frontend(IMSG_RESOLVER_DOWN, 0,
			    NULL, 0);
		else if (best->state > UNKNOWN && global_state < RESOLVING)
			resolver_imsg_compose_frontend(IMSG_RESOLVER_UP, 0,
			    NULL, 0);
		global_state = best->state;
	}
}

void
schedule_recheck_all_resolvers(void)
{
	struct timeval	 tv;
	int		 i;

	tv.tv_sec = 0;

	log_debug("%s", __func__);

	for (i = 0; i < UW_RES_NONE; i++) {
		if (resolvers[i] == NULL)
			continue;
		tv.tv_usec = arc4random() % 1000000; /* modulo bias is ok */
		evtimer_add(&resolvers[i]->check_ev, &tv);
	}
}

int
check_forwarders_changed(struct uw_forwarder_head *list_a,
    struct uw_forwarder_head *list_b)
{
	struct uw_forwarder	*a, *b;

	a = SIMPLEQ_FIRST(list_a);
	b = SIMPLEQ_FIRST(list_b);

	while(a != NULL && b != NULL) {
		if (strcmp(a->name, b->name) != 0)
			return 1;
		a = SIMPLEQ_NEXT(a, entry);
		b = SIMPLEQ_NEXT(b, entry);
	}

	if (a != NULL || b != NULL)
		return 1;
	return 0;
}

void
resolver_ref(struct uw_resolver *res)
{
	if (res->ref_cnt == INT_MAX)
		fatalx("%s: INT_MAX references", __func__);
	res->ref_cnt++;
}

void
resolver_unref(struct uw_resolver *res)
{
	struct timeval	 tv = { 0, 1};

	if (res->ref_cnt == 0)
		fatalx("%s: unreferenced resolver", __func__);

	res->ref_cnt--;

	/*
	 * Decouple from libunbound event callback.
	 * If we free the ctx inside of resolve_done or check_resovler_done
	 * we are cutting of the branch we are sitting on and hit a
	 * user-after-free
	 */
	if (res->stop && res->ref_cnt == 0) {
		evtimer_set(&res->free_ev, resolver_free_timo, res);
		evtimer_add(&res->free_ev, &tv);
	}
}

void
replace_forwarders(struct uw_forwarder_head *new_list, struct
    uw_forwarder_head *old_list)
{
	struct uw_forwarder	*uw_forwarder;

	while ((uw_forwarder =
	    SIMPLEQ_FIRST(old_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(old_list, entry);
		free(uw_forwarder);
	}

	while ((uw_forwarder = SIMPLEQ_FIRST(new_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(new_list, entry);
		SIMPLEQ_INSERT_TAIL(old_list, uw_forwarder, entry);
	}
}

struct uw_resolver*
best_resolver(void)
{
	struct uw_resolver	*res = NULL;
	int			 i;

	log_debug("%s: %s: %s, %s: %s, %s: %s, %s: %s, captive_portal: %s",
	    __func__,
	    uw_resolver_type_str[UW_RES_RECURSOR], resolvers[UW_RES_RECURSOR]
	    != NULL ? uw_resolver_state_str[resolvers[UW_RES_RECURSOR]->state]
	    : "NA",
	    uw_resolver_type_str[UW_RES_DHCP], resolvers[UW_RES_DHCP] != NULL ?
	    uw_resolver_state_str[resolvers[UW_RES_DHCP]->state] : "NA",
	    uw_resolver_type_str[UW_RES_FORWARDER],
	    resolvers[UW_RES_FORWARDER] != NULL ?
	    uw_resolver_state_str[resolvers[UW_RES_FORWARDER]->state] : "NA",
	    uw_resolver_type_str[UW_RES_DOT],
	    resolvers[UW_RES_DOT] != NULL ?
	    uw_resolver_state_str[resolvers[UW_RES_DOT]->state] :
	    "NA", captive_portal_state_str[captive_portal_state]);

	if (captive_portal_state == UNKNOWN || captive_portal_state == BEHIND) {
		if (resolvers[UW_RES_DHCP] != NULL) {
			res = resolvers[UW_RES_DHCP];
			goto out;
		}
	}

	res = resolvers[resolver_conf->res_pref[0]];

	for (i = 1; i < resolver_conf->res_pref_len; i++)
		if (resolver_cmp(res,
		    resolvers[resolver_conf->res_pref[i]]) < 0)
			res = resolvers[resolver_conf->res_pref[i]];
out:
	log_debug("%s: %s state: %s", __func__, uw_resolver_type_str[res->type],
	    uw_resolver_state_str[res->state]);
	return (res);
}

int
resolver_cmp(struct uw_resolver *a, struct uw_resolver *b)
{
	if (a == NULL && b == NULL)
		return 0;

	if (b == NULL)
		return 1;

	if (a == NULL)
		return -1;

	return (a->state < b->state ? -1 : a->state > b->state ? 1 : 0);
}

void
restart_resolvers(void)
{
	int	 verbose;

	verbose = log_getverbose() & OPT_VERBOSE2 ? UB_LOG_VERBOSE :
	    UB_LOG_BRIEF;
	log_debug("%s: %d", __func__, verbose);

	new_recursor();
	new_static_forwarders();
	new_static_dot_forwarders();
	new_forwarders();
}

void
show_status(enum uw_resolver_type type, pid_t pid)
{
	struct uw_resolver	*best;
	int			 i;

	best = best_resolver();

	switch(type) {
	case UW_RES_NONE:
		resolver_imsg_compose_frontend(IMSG_CTL_CAPTIVEPORTAL_INFO,
		    pid, &captive_portal_state, sizeof(captive_portal_state));
		for (i = 0; i < resolver_conf->res_pref_len; i++)
			send_resolver_info(
			    resolvers[resolver_conf->res_pref[i]],
			    resolvers[resolver_conf->res_pref[i]] ==
			    best, pid);
		break;
	case UW_RES_RECURSOR:
	case UW_RES_DHCP:
	case UW_RES_FORWARDER:
	case UW_RES_DOT:
		send_resolver_info(resolvers[type], resolvers[type] == best,
		    pid);
		send_detailed_resolver_info(resolvers[type], pid);
		break;
	default:
		fatalx("unknown resolver type %d", type);
		break;
	}
	resolver_imsg_compose_frontend(IMSG_CTL_END, pid, NULL, 0);
}

void
send_resolver_info(struct uw_resolver *res, int selected, pid_t pid)
{
	struct ctl_resolver_info	 cri;

	if (res == NULL)
		return;

	cri.state = res->state;
	cri.type = res->type;
	cri.selected = selected;
	resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_INFO, pid, &cri,
	    sizeof(cri));
}

void
send_detailed_resolver_info(struct uw_resolver *res, pid_t pid)
{
	char	 buf[1024];

	if (res == NULL)
		return;

	if (res->state == RESOLVING) {
		(void)strlcpy(buf, res->why_bogus, sizeof(buf));
		resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_WHY_BOGUS,
		    pid, buf, sizeof(buf));
	}
	send_resolver_histogram_info(res, pid);
}

void
send_resolver_histogram_info(struct uw_resolver *res, pid_t pid)
{
	int64_t	 histogram[nitems(histogram_limits)];

	memcpy(histogram, res->histogram, sizeof(histogram));

	resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_HISTOGRAM,
		    pid, histogram, sizeof(histogram));
}

void
check_captive_portal_timo(int fd, short events, void *arg)
{
	captive_portal_check_tv.tv_sec *= 2;
	if (captive_portal_check_tv.tv_sec > PORTAL_CHECK_MAXSEC)
		captive_portal_check_tv.tv_sec = PORTAL_CHECK_MAXSEC;
	check_captive_portal(0);
}

void
check_captive_portal(int timer_reset)
{
	log_debug("%s", __func__);

	if (resolver_conf->captive_portal_host == NULL) {
		log_debug("%s: no captive portal url configured", __func__);
		return;
	}

	if (resolvers[UW_RES_DHCP] == NULL) {
		log_debug("%s no DHCP nameservers known", __func__);
		return;
	}

	if (timer_reset)
		captive_portal_check_tv.tv_sec = PORTAL_CHECK_SEC;

	evtimer_add(&captive_portal_check_ev, &captive_portal_check_tv);

	captive_portal_state = PORTAL_UNKNOWN;

	resolver_imsg_compose_main(IMSG_RESOLVE_CAPTIVE_PORTAL, 0, NULL, 0);
}

int
check_captive_portal_changed(struct uw_conf *a, struct uw_conf *b)
{

	if (a->captive_portal_expected_status !=
	    b->captive_portal_expected_status)
		return (1);

	if (a->captive_portal_host == NULL && b->captive_portal_host != NULL)
		return (1);
	if (a->captive_portal_host != NULL && b->captive_portal_host == NULL)
		return (1);
	if (a->captive_portal_host != NULL && b->captive_portal_host != NULL &&
	    strcmp(a->captive_portal_host, b->captive_portal_host) != 0)
		return (1);

	if (a->captive_portal_path == NULL && b->captive_portal_path != NULL)
		return (1);
	if (a->captive_portal_path != NULL && b->captive_portal_path == NULL)
		return (1);
	if (a->captive_portal_path != NULL && b->captive_portal_path != NULL &&
	    strcmp(a->captive_portal_path, b->captive_portal_path) != 0)
		return (1);

	if (a->captive_portal_expected_response == NULL &&
	    b->captive_portal_expected_response != NULL)
		return (1);
	if (a->captive_portal_expected_response != NULL &&
	    b->captive_portal_expected_response == NULL)
		return (1);
	if (a->captive_portal_expected_response != NULL &&
	    b->captive_portal_expected_response != NULL &&
	    strcmp(a->captive_portal_expected_response,
	    b->captive_portal_expected_response) != 0)
		return (1);

	return (0);
}

void
trust_anchor_resolve(void)
{
	struct uw_resolver	*res;
	struct timeval		 tv = {TRUST_ANCHOR_RETRY_INTERVAL, 0};
	int			 err;

	log_debug("%s", __func__);

	res = best_resolver();

	if (res == NULL || res->state < VALIDATING) {
		evtimer_add(&trust_anchor_timer, &tv);
		return;
	}

	resolver_ref(res);

	if ((err = ub_resolve_event(res->ctx, ".",  LDNS_RR_TYPE_DNSKEY,
	    LDNS_RR_CLASS_IN, res, trust_anchor_resolve_done,
	    NULL)) != 0) {
		log_warn("%s: ub_resolve_async: err: %d, %s",
		    __func__, err, ub_strerror(err));
		resolver_unref(res);
		evtimer_add(&trust_anchor_timer, &tv);
	}
}

void
trust_anchor_timo(int fd, short events, void *arg)
{
	trust_anchor_resolve();
}

void
trust_anchor_resolve_done(void *arg, int rcode, void *answer_packet,
    int answer_len, int sec, char *why_bogus, int was_ratelimited)
{
	struct uw_resolver	*res;
	struct ub_result	*result;
	sldns_buffer		*buf;
	struct regional		*region;
	struct timeval		 tv = {TRUST_ANCHOR_RETRY_INTERVAL, 0};
	int			 i, tas, n;
	uint16_t		 dnskey_flags;
	char			*str, rdata_buf[1024], *ta;

	res = (struct uw_resolver *)arg;

	if ((result = calloc(1, sizeof(*result))) == NULL)
		goto out;

	log_debug("%s: rcode: %d", __func__, rcode);

	if (!sec) {
		log_debug("%s: sec: %d", __func__, sec);
		goto out;
	}

	if ((str = sldns_wire2str_pkt(answer_packet, answer_len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

	buf = sldns_buffer_new(answer_len);
	region = regional_create();
	result->rcode = LDNS_RCODE_SERVFAIL;
	if(region && buf) {
		sldns_buffer_clear(buf);
		sldns_buffer_write(buf, answer_packet, answer_len);
		sldns_buffer_flip(buf);
		libworker_enter_result(result, buf, region, sec);
		result->answer_packet = NULL;
		result->answer_len = 0;
		sldns_buffer_free(buf);
		regional_destroy(region);

		if (result->rcode != LDNS_RCODE_NOERROR) {
			log_debug("%s: result->rcode: %d", __func__,
			    result->rcode);
			goto out;
		}

		i = 0;
		tas = 0;
		while(result->data[i] != NULL) {
			if (result->len[i] < 2) {
				if (tas > 0)
					resolver_imsg_compose_frontend(
					    IMSG_NEW_TAS_ABORT, 0, NULL, 0);
				goto out;
			}
			n = sldns_wire2str_rdata_buf(result->data[i],
			    result->len[i], rdata_buf, sizeof(rdata_buf),
			    LDNS_RR_TYPE_DNSKEY);

			if (n < 0 || (size_t)n >= sizeof(rdata_buf)) {
				log_warnx("trust anchor buffer to small");
				resolver_imsg_compose_frontend(
				    IMSG_NEW_TAS_ABORT, 0, NULL, 0);
				goto out;
			}

			memcpy(&dnskey_flags, result->data[i], 2);
			dnskey_flags = ntohs(dnskey_flags);
			if ((dnskey_flags & LDNS_KEY_SEP_KEY) &&
			    !(dnskey_flags & LDNS_KEY_REVOKE_KEY)) {
				asprintf(&ta, ".\t%d\tIN\tDNSKEY\t%s",
				    ROOT_DNSKEY_TTL, rdata_buf);
				log_debug("%s: ta: %s", __func__, ta);
				resolver_imsg_compose_frontend(IMSG_NEW_TA, 0,
				    ta, strlen(ta) + 1);
				tas++;
				free(ta);
			}
			i++;
		}
		if (tas > 0) {
			resolver_imsg_compose_frontend(IMSG_NEW_TAS_DONE, 0,
			    NULL, 0);
			tv.tv_sec = TRUST_ANCHOR_QUERY_INTERVAL;
		}
	}
out:
	ub_resolve_free(result);
	resolver_unref(res);
	evtimer_add(&trust_anchor_timer, &tv);
}
