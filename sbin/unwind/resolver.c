/*	$OpenBSD: resolver.c,v 1.4 2019/01/24 17:39:43 florian Exp $	*/

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
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <imsg.h>
#include <pwd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <assert.h>
#include "libunbound/config.h"
#include "libunbound/libunbound/unbound.h"
#include "libunbound/unbound-event.h"
#include "libunbound/sldns/rrdef.h"
#include "libunbound/sldns/pkthdr.h"
#include "libunbound/sldns/sbuffer.h"
#include "libunbound/sldns/wire2str.h"

#include <openssl/crypto.h>

#include "uw_log.h"
#include "unwind.h"
#include "resolver.h"

#define	CHROOT		"/etc/unwind"
#define	DB_DIR		"/trustanchor/"
#define	ROOT_KEY	DB_DIR"root.key"

#define	UB_LOG_VERBOSE	4
#define	UB_LOG_BRIEF	0

struct unwind_resolver {
	struct event			 check_ev;
	struct event			 free_ev;
	struct ub_ctx			*ctx;
	int				 ref_cnt;
	int				 stop;
	enum unwind_resolver_state	 state;
	enum unwind_resolver_type	 type;
	char				*why_bogus;
	int64_t				 histogram[nitems(histogram_limits)];
};

struct check_resolver_data {
	struct unwind_resolver		*res;
	struct unwind_resolver		*check_res;
};

__dead void		 resolver_shutdown(void);
void			 resolver_sig_handler(int sig, short, void *);
void			 resolver_dispatch_frontend(int, short, void *);
void			 resolver_dispatch_main(int, short, void *);
void			 resolve_done(void *, int, void *, int, int,
			     char *, int);
void			 parse_dhcp_forwarders(char *);
void			 new_recursor(void);
void			 new_forwarders(void);
void			 new_static_forwarders(void);
struct unwind_resolver	*create_resolver(enum unwind_resolver_type);
void			 free_resolver(struct unwind_resolver *);
void			 set_forwarders(struct unwind_resolver *,
			     struct unwind_forwarder_head *);
void			 resolver_check_timo(int, short, void *);
void			 resolver_free_timo(int, short, void *);
void			 check_resolver(struct unwind_resolver *);
void			 check_resolver_done(void *, int, void *, int, int,
			     char *, int);
int			 check_forwarders_changed(struct
			     unwind_forwarder_head *,
			     struct unwind_forwarder_head *);
void			 replace_forwarders(struct unwind_forwarder_head *,
			     struct unwind_forwarder_head *);
void			 resolver_ref(struct unwind_resolver *);
void			 resolver_unref(struct unwind_resolver *);
struct unwind_resolver	*best_resolver(void);
int			 resolver_cmp(struct unwind_resolver *,
			     struct unwind_resolver *);
void			 restart_resolvers(void);
void			 show_status(enum unwind_resolver_type, pid_t);
void			 send_resolver_info(struct unwind_resolver *, int,
			     pid_t);
void			 send_detailed_resolver_info(struct unwind_resolver *,
			     pid_t);
void			 send_resolver_histogram_info(struct unwind_resolver *,
			     pid_t pid);

/* for openssl */
void			 init_locks(void);
unsigned long		 id_callback(void);
void			 lock_callback(int, int, const char *, int);

struct unwind_conf	*resolver_conf;
struct imsgev		*iev_frontend;
struct imsgev		*iev_main;
struct unwind_forwarder_head  dhcp_forwarder_list;
struct unwind_resolver	*recursor, *forwarder, *static_forwarder;
struct timeval		 resolver_check_pause = { 30, 0};

struct event_base	*ev_base;

/* for openssl */
pthread_mutex_t		*locks;

enum unwind_resolver_state	 global_state = DEAD;

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

	if (chroot(CHROOT) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	unwind_process = PROC_RESOLVER;
	setproctitle("%s", log_procnames[unwind_process]);
	log_procinit(log_procnames[unwind_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	unveil(DB_DIR, "rwc");

	if (pledge("stdio inet dns rpath wpath cpath recvfd", NULL) == -1)
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

	init_locks();
	CRYPTO_set_id_callback(id_callback);
	CRYPTO_set_locking_callback(lock_callback);

	new_recursor();

	SIMPLEQ_INIT(&dhcp_forwarder_list);

	event_dispatch();

	resolver_shutdown();
}

__dead void
resolver_shutdown(void)
{
	log_debug("%s", __func__);
	/* XXX we might have many more ctx lying aroung */
	if (recursor != NULL) {
		recursor->ref_cnt = 0; /* not coming back to deref ctx*/
		free_resolver(recursor);
	}
	if (forwarder != NULL) {
		forwarder->ref_cnt = 0; /* not coming back to deref ctx */
		free_resolver(forwarder);
	}
	if (static_forwarder != NULL) {
		static_forwarder->ref_cnt = 0;
		free_resolver(static_forwarder);
	}

	/* Close pipes. */
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(resolver_conf);

	free(iev_frontend);
	free(iev_main);

	log_info("resolver exiting");
	exit(0);
}

int
resolver_imsg_compose_frontend(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_frontend, type, 0, pid, -1,
	    data, datalen));
}

void
resolver_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf;
	struct imsg			 imsg;
	struct query_imsg		*query_imsg;
	struct unwind_resolver		*res;
	enum unwind_resolver_type	 type;
	ssize_t				 n;
	int				 shut = 0, verbose, err;
	int				 update_resolvers;

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
			/* Already checked by frontend. */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			update_resolvers = (log_getverbose() & OPT_VERBOSE2)
			    != (verbose & OPT_VERBOSE2);
			log_setverbose(verbose);
			if (update_resolvers)
				restart_resolvers();
			break;
		case IMSG_SHUTDOWN:
			resolver_imsg_compose_frontend(IMSG_SHUTDOWN, 0, NULL,
			    0);
			break;
		case IMSG_QUERY:
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
			    unwind_resolver_type_str[res->type]);

			query_imsg->resolver = res;
			resolver_ref(res);

			clock_gettime(CLOCK_MONOTONIC, &query_imsg->tp);

			if ((err = ub_resolve_event(res->ctx,
			    query_imsg->qname, query_imsg->t, query_imsg->c,
			    (void *)query_imsg, resolve_done,
			    &query_imsg->async_id)) != 0)
				log_warn("%s: ub_resolve_async: err: %d, %s",
				    __func__, err, ub_strerror(err));
			break;
		case IMSG_FORWARDER:
			/* make sure this is a string */
			((char *)imsg.data)[imsg.hdr.len - IMSG_HEADER_SIZE -1]
			    = '\0';
			parse_dhcp_forwarders(imsg.data);
			break;
		case IMSG_CTL_STATUS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(type))
				break;
			memcpy(&type, imsg.data, sizeof(type));
			show_status(type, imsg.hdr.pid);
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
	static struct unwind_conf	*nconf;
	struct unwind_forwarder		*unwind_forwarder;
	struct imsg			 imsg;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf;
	ssize_t				 n;
	int				 shut = 0, forwarders_changed;

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
		case IMSG_SOCKET_IPC:
			/*
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend) {
				log_warnx("%s: received unexpected imsg fd "
				    "to resolver", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				log_warnx("%s: expected to receive imsg fd to "
				   "resolver but didn't receive any", __func__);
				break;
			}

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
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct unwind_conf))) == NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct unwind_conf));
			SIMPLEQ_INIT(&nconf->unwind_forwarder_list);
			break;
		case IMSG_RECONF_FORWARDER:
			if ((unwind_forwarder = malloc(sizeof(struct
			    unwind_forwarder))) == NULL)
				fatal(NULL);
			memcpy(unwind_forwarder, imsg.data, sizeof(struct
			    unwind_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->unwind_forwarder_list,
			    unwind_forwarder, entry);
			break;
		case IMSG_RECONF_END:
			forwarders_changed = check_forwarders_changed(
			    &resolver_conf->unwind_forwarder_list,
			    &nconf->unwind_forwarder_list);
			merge_config(resolver_conf, nconf);
			nconf = NULL;
			if (forwarders_changed) {
				log_debug("static forwarders changed");
				new_static_forwarders();
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
	struct unwind_resolver	*res;
	struct timespec		 tp, elapsed;
	int64_t			 ms;
	size_t			 i;
	char			*str;

	clock_gettime(CLOCK_MONOTONIC, &tp);

	query_imsg = (struct query_imsg *)arg;
	res = (struct unwind_resolver *)query_imsg->resolver;

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
	resolver_imsg_compose_frontend(IMSG_ANSWER_HEADER, 0,
	    query_imsg, sizeof(*query_imsg));
	free(query_imsg);
	resolver_unref(res);
}

void
parse_dhcp_forwarders(char *resolvers)
{
	struct unwind_forwarder_head	 new_forwarder_list;
	struct unwind_forwarder		*unwind_forwarder;
	char				*ns;

	SIMPLEQ_INIT(&new_forwarder_list);

	if (resolvers != NULL) {
		while((ns = strsep(&resolvers, ",")) != NULL) {
			log_debug("%s: %s", __func__, ns);
			if ((unwind_forwarder = malloc(sizeof(struct
			    unwind_forwarder))) == NULL)
				fatal(NULL);
			if (strlcpy(unwind_forwarder->name, ns,
			    sizeof(unwind_forwarder->name)) >=
			    sizeof(unwind_forwarder->name))
				fatalx("strlcpy");
			SIMPLEQ_INSERT_TAIL(&new_forwarder_list,
			    unwind_forwarder, entry);
		}
	}

	if (check_forwarders_changed(&new_forwarder_list,
	    &dhcp_forwarder_list)) {
		replace_forwarders(&new_forwarder_list, &dhcp_forwarder_list);
		new_forwarders();
	} else
		log_debug("%s: forwarders didn't change", __func__);
}

void
new_recursor(void)
{
	free_resolver(recursor);
	recursor = create_resolver(RECURSOR);
	check_resolver(recursor);
}

void
new_forwarders(void)
{
	free_resolver(forwarder);

	if (SIMPLEQ_EMPTY(&dhcp_forwarder_list)) {
		forwarder = NULL;
		return;
	}

	log_debug("%s: create_resolver", __func__);
	forwarder = create_resolver(FORWARDER);
	set_forwarders(forwarder, &dhcp_forwarder_list);

	check_resolver(forwarder);
}

void
new_static_forwarders(void)
{
	free_resolver(static_forwarder);

	if (SIMPLEQ_EMPTY(&resolver_conf->unwind_forwarder_list)) {
		static_forwarder = NULL;
		return;
	}

	log_debug("%s: create_resolver", __func__);
	static_forwarder = create_resolver(STATIC_FORWARDER);
	set_forwarders(static_forwarder, &resolver_conf->unwind_forwarder_list);

	check_resolver(static_forwarder);
}

struct unwind_resolver *
create_resolver(enum unwind_resolver_type type)
{
	struct unwind_resolver	*res;
	int err;

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

	ub_ctx_debuglevel(res->ctx, log_getverbose() & OPT_VERBOSE2 ?
	    UB_LOG_VERBOSE : UB_LOG_BRIEF);

	if ((err = ub_ctx_add_ta_autr(res->ctx, ROOT_KEY)) != 0) {
		ub_ctx_delete(res->ctx);
		free(res);
		log_warnx("error adding trust anchor: %s",
		    ub_strerror(err));
		return (NULL);
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
			log_warnx("error setting aggressive-nsec: yes: %s",
			    ub_strerror(err));
			return (NULL);
		}
	}

	evtimer_set(&res->check_ev, resolver_check_timo, res);

	return (res);
}

void
free_resolver(struct unwind_resolver *res)
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
set_forwarders(struct unwind_resolver *res, struct unwind_forwarder_head
    *unwind_forwarder_list)
{
	struct unwind_forwarder	*unwind_forwarder;

	SIMPLEQ_FOREACH(unwind_forwarder, unwind_forwarder_list, entry)
		ub_ctx_set_fwd(res->ctx, unwind_forwarder->name);
}

void
resolver_check_timo(int fd, short events, void *arg)
{
	check_resolver((struct unwind_resolver *)arg);
}

void
resolver_free_timo(int fd, short events, void *arg)
{
	free_resolver((struct unwind_resolver *)arg);
}

void
check_resolver(struct unwind_resolver *res)
{
	struct unwind_resolver		*check_res;
	struct check_resolver_data	*data;
	int				 err;

	log_debug("%s: create_resolver", __func__);
	if ((check_res = create_resolver(res->type)) == NULL)
		fatal("%s", __func__);
	if ((data = malloc(sizeof(*data))) == NULL)
		fatal("%s", __func__);

	switch(check_res->type) {
	case RECURSOR:
		break;
	case FORWARDER:
		set_forwarders(check_res, &dhcp_forwarder_list);
		break;
	case STATIC_FORWARDER:
		set_forwarders(check_res,
		    &resolver_conf->unwind_forwarder_list);
		break;
	case RESOLVER_NONE:
		fatalx("type NONE");
		break;
	}

	resolver_ref(check_res);
	resolver_ref(res);
	data->check_res = check_res;
	data->res = res;

	if ((err = ub_resolve_event(check_res->ctx, ".",  LDNS_RR_TYPE_NS,
	    LDNS_RR_CLASS_IN, data,
	    check_resolver_done, NULL)) != 0) {
		log_warn("%s: ub_resolve_event: err: %d, %s",
		    __func__, err, ub_strerror(err));
		resolver_unref(check_res);
		resolver_unref(res);
		evtimer_add(&res->check_ev, &resolver_check_pause);
	}
}

void
check_resolver_done(void *arg, int rcode, void *answer_packet, int answer_len,
    int sec, char *why_bogus, int was_ratelimited)
{
	struct check_resolver_data	*data;
	struct unwind_resolver		*best;
	char				*str;

	data = (struct check_resolver_data *)arg;

	log_debug("%s: rcode: %d", __func__, rcode);

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

	if (sec == 2)
		data->res->state = VALIDATING;
	else if (rcode == 0) {
		log_debug("%s: why bogus: %s", __func__, why_bogus);
		data->res->state = RESOLVING;
		/* best effort */
		data->res->why_bogus = strdup(why_bogus);
	} else
		data->res->state = DEAD; /* we know the root exists */

out:
	if (!data->res->stop)
		evtimer_add(&data->res->check_ev, &resolver_check_pause);

	log_debug("%s: %s: %s", __func__,
	    unwind_resolver_type_str[data->res->type],
	    unwind_resolver_state_str[data->res->state]);

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

/* for openssl */
void
init_locks(void)
{
	int	 i;

	if ((locks = calloc(CRYPTO_num_locks(), sizeof(pthread_mutex_t))) ==
	    NULL)
		fatal("%s", __func__);

	for (i = 0; i < CRYPTO_num_locks(); i++)
		pthread_mutex_init(&locks[i], NULL);

}

unsigned long
id_callback(void) {
	return ((unsigned long)pthread_self());
}

void
lock_callback(int mode, int type, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
		pthread_mutex_lock(&locks[type]);
	else
		pthread_mutex_unlock(&locks[type]);
}

int
check_forwarders_changed(struct unwind_forwarder_head *list_a,
    struct unwind_forwarder_head *list_b)
{
	struct unwind_forwarder	*a, *b;

	a = SIMPLEQ_FIRST(list_a);
	b = SIMPLEQ_FIRST(list_b);

	while(a != NULL && b != NULL) {
		log_debug("a: %s, b: %s", a->name, b->name);
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
resolver_ref(struct unwind_resolver *res)
{
	if (res->ref_cnt == INT_MAX)
		fatalx("%s: INT_MAX references", __func__);
	res->ref_cnt++;
}

void
resolver_unref(struct unwind_resolver *res)
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
replace_forwarders(struct unwind_forwarder_head *new_list, struct
    unwind_forwarder_head *old_list)
{
	struct unwind_forwarder	*unwind_forwarder;

	while ((unwind_forwarder =
	    SIMPLEQ_FIRST(old_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(old_list, entry);
		free(unwind_forwarder);
	}

	while ((unwind_forwarder = SIMPLEQ_FIRST(new_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(new_list, entry);
		SIMPLEQ_INSERT_TAIL(old_list, unwind_forwarder, entry);
	}
}

struct unwind_resolver*
best_resolver(void)
{
	struct unwind_resolver	*res = NULL;

	if (recursor != NULL)
		log_debug("%s: %s state: %s", __func__,
		    unwind_resolver_type_str[recursor->type],
		    unwind_resolver_state_str[recursor->state]);

	if (static_forwarder != NULL)
		log_debug("%s: %s state: %s", __func__,
		    unwind_resolver_type_str[static_forwarder->type],
		    unwind_resolver_state_str[static_forwarder->state]);

	if (forwarder != NULL)
		log_debug("%s: %s state: %s", __func__,
		    unwind_resolver_type_str[forwarder->type],
		    unwind_resolver_state_str[forwarder->state]);

	res = recursor;

	if (resolver_cmp(res, static_forwarder) < 0)
		res = static_forwarder;

	if (resolver_cmp(res, forwarder) < 0)
		res = forwarder;

	return (res);
}

int
resolver_cmp(struct unwind_resolver *a, struct unwind_resolver *b)
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
	new_forwarders();
}

void
show_status(enum unwind_resolver_type type, pid_t pid)
{
	struct unwind_resolver		*best;

	best = best_resolver();

	switch(type) {
	case RESOLVER_NONE:
		send_resolver_info(recursor, recursor == best, pid);
		send_resolver_info(forwarder, forwarder == best, pid);
		send_resolver_info(static_forwarder, static_forwarder == best,
		    pid);
		break;
	case RECURSOR:
		send_resolver_info(recursor, recursor == best, pid);
		send_detailed_resolver_info(recursor, pid);
		break;
	case FORWARDER:
		send_resolver_info(forwarder, forwarder == best, pid);
		send_detailed_resolver_info(forwarder, pid);
		break;
	case STATIC_FORWARDER:
		send_resolver_info(static_forwarder, static_forwarder == best,
		    pid);
		send_detailed_resolver_info(static_forwarder, pid);
		break;
	}
	resolver_imsg_compose_frontend(IMSG_CTL_END, pid, NULL, 0);
}

void
send_resolver_info(struct unwind_resolver *res, int selected, pid_t pid)
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
send_detailed_resolver_info(struct unwind_resolver *res, pid_t pid)
{
	char	 buf[1024];

	if (res->type == RESOLVING) {
		(void)strlcpy(buf, res->why_bogus, sizeof(buf));
		resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_WHY_BOGUS,
		    pid, buf, sizeof(buf));
	}
	send_resolver_histogram_info(res, pid);
}

void
send_resolver_histogram_info(struct unwind_resolver *res, pid_t pid)
{
	int64_t	 histogram[nitems(histogram_limits)];

	memcpy(histogram, res->histogram, sizeof(histogram));

	resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_HISTOGRAM,
		    pid, histogram, sizeof(histogram));
}
