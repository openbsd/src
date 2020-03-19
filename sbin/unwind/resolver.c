/*	$OpenBSD: resolver.c,v 1.123 2020/03/19 19:27:21 tobhe Exp $	*/

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

#include <net/route.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <netdb.h>
#include <asr.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tls.h>
#include <unistd.h>

#include "libunbound/config.h"
#include "libunbound/libunbound/context.h"
#include "libunbound/libunbound/libworker.h"
#include "libunbound/libunbound/unbound.h"
#include "libunbound/libunbound/unbound-event.h"
#include "libunbound/services/cache/rrset.h"
#include "libunbound/sldns/sbuffer.h"
#include "libunbound/sldns/rrdef.h"
#include "libunbound/sldns/pkthdr.h"
#include "libunbound/sldns/wire2str.h"
#include "libunbound/util/config_file.h"
#include "libunbound/util/module.h"
#include "libunbound/util/regional.h"
#include "libunbound/util/storage/slabhash.h"
#include "libunbound/validator/validator.h"
#include "libunbound/validator/val_kcache.h"
#include "libunbound/validator/val_neg.h"

#include <openssl/crypto.h>

#include "log.h"
#include "frontend.h"
#include "unwind.h"
#include "resolver.h"

#define	TLS_DEFAULT_CA_CERT_FILE	"/etc/ssl/cert.pem"
#define	UB_LOG_VERBOSE			4
#define	UB_LOG_BRIEF			0

/* maximum size of a libunbound forwarder definition: IP@PORT#AUTHNAME */
#define	FWD_MAX				(INET6_ADDRSTRLEN + NI_MAXHOST + 2 + 5)

/*
 * The prefered resolver type can be this many ms slower than the next
 * best and still be picked
 */
#define	PREF_RESOLVER_MEDIAN_SKEW	200		/* 200 ms */
#define	NEXT_RES_MAX			2000		/* 2000 ms */

#define	DOUBT_NXDOMAIN_SEC		(5 * 60)	/* 5 minutes */

#define	RESOLVER_CHECK_SEC		1
#define	RESOLVER_CHECK_MAXSEC		1024 /* ~17 minutes */
#define	DECAY_PERIOD			60
#define	DECAY_NOMINATOR			9
#define	DECAY_DENOMINATOR		10

#define	TRUST_ANCHOR_RETRY_INTERVAL	8640
#define	TRUST_ANCHOR_QUERY_INTERVAL	43200

/* in libworker_event_done_cb() enum sec_status gets mapped to 0, 1 and 2 */
#define	INSECURE	0
#define	BOGUS		1
#define	SECURE		2

struct uw_resolver {
	struct event		 check_ev;
	struct event		 free_ev;
	struct ub_ctx		*ctx;
	void			*asr_ctx;
	struct timeval		 check_tv;
	int			 ref_cnt;
	int			 stop;
	enum uw_resolver_state	 state;
	enum uw_resolver_type	 type;
	int			 check_running;
	int64_t			 median;
	int64_t			 histogram[nitems(histogram_limits)];
	int64_t			 latest_histogram[nitems(histogram_limits)];
};

struct running_query {
	TAILQ_ENTRY(running_query)	 entry;
	struct query_imsg		*query_imsg;
	struct event			 timer_ev;
	struct timespec			 tp;
	struct resolver_preference	 res_pref;
	int				 next_resolver;
	int				 running;
};

TAILQ_HEAD(, running_query)	 running_queries;

typedef void (*resolve_cb_t)(struct uw_resolver *, void *, int, void *, int,
    int, char *);

struct resolver_cb_data {
	resolve_cb_t		 cb;
	void			*data;
	struct uw_resolver	*res;
};

__dead void		 resolver_shutdown(void);
void			 resolver_sig_handler(int sig, short, void *);
void			 resolver_dispatch_frontend(int, short, void *);
void			 resolver_dispatch_main(int, short, void *);
int			 sort_resolver_types(struct resolver_preference *);
void			 setup_query(struct query_imsg *);
struct running_query	*find_running_query(uint64_t);
void			 try_resolver_timo(int, short, void *);
int			 try_next_resolver(struct running_query *);

int			 resolve(struct uw_resolver *, const char*, int, int,
			     void*, resolve_cb_t);
void			 resolve_done(struct uw_resolver *, void *, int, void *,
			     int, int, char *);
void			 ub_resolve_done(void *, int, void *, int, int, char *,
			     int);
void			 asr_resolve_done(struct asr_result *, void *);
void			 new_resolver(enum uw_resolver_type,
			     enum uw_resolver_state);
struct uw_resolver	*create_resolver(enum uw_resolver_type);
void			 setup_unified_caches(void);
void			 set_unified_cache(struct uw_resolver *);
void			 free_resolver(struct uw_resolver *);
void			 set_forwarders(struct uw_resolver *,
			     struct uw_forwarder_head *, int);
void			 resolver_check_timo(int, short, void *);
void			 resolver_free_timo(int, short, void *);
void			 check_resolver(struct uw_resolver *);
void			 check_resolver_done(struct uw_resolver *, void *, int,
			     void *, int, int, char *);
void			 schedule_recheck_all_resolvers(void);
int			 check_forwarders_changed(struct uw_forwarder_head *,
			     struct uw_forwarder_head *);
void			 replace_forwarders(struct uw_forwarder_head *,
			     struct uw_forwarder_head *);
void			 resolver_ref(struct uw_resolver *);
void			 resolver_unref(struct uw_resolver *);
int			 resolver_cmp(const void *, const void *);
void			 restart_ub_resolvers(void);
void			 show_status(pid_t);
void			 show_autoconf(pid_t);
void			 show_mem(pid_t);
void			 send_resolver_info(struct uw_resolver *, pid_t);
void			 send_detailed_resolver_info(struct uw_resolver *,
			     pid_t);
void			 trust_anchor_resolve(void);
void			 trust_anchor_timo(int, short, void *);
void			 trust_anchor_resolve_done(struct uw_resolver *, void *,
			     int, void *, int, int, char *);
void			 replace_autoconf_forwarders(struct
			     imsg_rdns_proposal *);
int			 force_tree_cmp(struct force_tree_entry *,
			     struct force_tree_entry *);
int			 find_force(struct force_tree *, char *,
			     struct uw_resolver **);
int64_t			 histogram_median(int64_t *);
void			 decay_latest_histograms(int, short, void *);
int			 running_query_cnt(void);
int			*resolvers_to_restart(struct uw_conf *,
			     struct uw_conf *);

struct uw_conf			*resolver_conf;
struct imsgev			*iev_frontend;
struct imsgev			*iev_main;
struct uw_forwarder_head	 autoconf_forwarder_list;
struct uw_resolver		*resolvers[UW_RES_NONE];
int				 enabled_resolvers[UW_RES_NONE];
struct timespec			 last_network_change;

struct event			 trust_anchor_timer;
struct event			 decay_timer;

static struct trust_anchor_head	 trust_anchors, new_trust_anchors;

struct event_base		*ev_base;

RB_GENERATE(force_tree, force_tree_entry, entry, force_tree_cmp)

int				 val_id = -1;
struct slabhash			*unified_msg_cache;
struct rrset_cache		*unified_rrset_cache;
struct key_cache		*unified_key_cache;
struct val_neg_cache		*unified_neg_cache;

static const char * const	 as112_zones[] = {
	/* RFC1918 */
	"10.in-addr.arpa. transparent",
	"16.172.in-addr.arpa. transparent",
	"31.172.in-addr.arpa. transparent",
	"168.192.in-addr.arpa. transparent",

	/* RFC3330 */
	"0.in-addr.arpa. transparent",
	"254.169.in-addr.arpa. transparent",
	"2.0.192.in-addr.arpa. transparent",
	"100.51.198.in-addr.arpa. transparent",
	"113.0.203.in-addr.arpa. transparent",
	"255.255.255.255.in-addr.arpa. transparent",

	/* RFC6598 */
	"64.100.in-addr.arpa. transparent",
	"65.100.in-addr.arpa. transparent",
	"66.100.in-addr.arpa. transparent",
	"67.100.in-addr.arpa. transparent",
	"68.100.in-addr.arpa. transparent",
	"69.100.in-addr.arpa. transparent",
	"70.100.in-addr.arpa. transparent",
	"71.100.in-addr.arpa. transparent",
	"72.100.in-addr.arpa. transparent",
	"73.100.in-addr.arpa. transparent",
	"74.100.in-addr.arpa. transparent",
	"75.100.in-addr.arpa. transparent",
	"76.100.in-addr.arpa. transparent",
	"77.100.in-addr.arpa. transparent",
	"78.100.in-addr.arpa. transparent",
	"79.100.in-addr.arpa. transparent",
	"80.100.in-addr.arpa. transparent",
	"81.100.in-addr.arpa. transparent",
	"82.100.in-addr.arpa. transparent",
	"83.100.in-addr.arpa. transparent",
	"84.100.in-addr.arpa. transparent",
	"85.100.in-addr.arpa. transparent",
	"86.100.in-addr.arpa. transparent",
	"87.100.in-addr.arpa. transparent",
	"88.100.in-addr.arpa. transparent",
	"89.100.in-addr.arpa. transparent",
	"90.100.in-addr.arpa. transparent",
	"91.100.in-addr.arpa. transparent",
	"92.100.in-addr.arpa. transparent",
	"93.100.in-addr.arpa. transparent",
	"94.100.in-addr.arpa. transparent",
	"95.100.in-addr.arpa. transparent",
	"96.100.in-addr.arpa. transparent",
	"97.100.in-addr.arpa. transparent",
	"98.100.in-addr.arpa. transparent",
	"99.100.in-addr.arpa. transparent",
	"100.100.in-addr.arpa. transparent",
	"101.100.in-addr.arpa. transparent",
	"102.100.in-addr.arpa. transparent",
	"103.100.in-addr.arpa. transparent",
	"104.100.in-addr.arpa. transparent",
	"105.100.in-addr.arpa. transparent",
	"106.100.in-addr.arpa. transparent",
	"107.100.in-addr.arpa. transparent",
	"108.100.in-addr.arpa. transparent",
	"109.100.in-addr.arpa. transparent",
	"110.100.in-addr.arpa. transparent",
	"111.100.in-addr.arpa. transparent",
	"112.100.in-addr.arpa. transparent",
	"113.100.in-addr.arpa. transparent",
	"114.100.in-addr.arpa. transparent",
	"115.100.in-addr.arpa. transparent",
	"116.100.in-addr.arpa. transparent",
	"117.100.in-addr.arpa. transparent",
	"118.100.in-addr.arpa. transparent",
	"119.100.in-addr.arpa. transparent",
	"120.100.in-addr.arpa. transparent",
	"121.100.in-addr.arpa. transparent",
	"122.100.in-addr.arpa. transparent",
	"123.100.in-addr.arpa. transparent",
	"124.100.in-addr.arpa. transparent",
	"125.100.in-addr.arpa. transparent",
	"126.100.in-addr.arpa. transparent",
	"127.100.in-addr.arpa. transparent",

	/* RFC4291 */
	"0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0."
	"ip6.arpa. transparent",

	/* RFC4193 */
	"D.F.ip6.arpa. transparent",

	/* RFC4291 */
	"8.E.F.ip6.arpa. transparent",
	"9.E.F.ip6.arpa. transparent",
	"A.E.F.ip6.arpa. transparent",
	"B.E.F.ip6.arpa. transparent",

	/* RFC3849 */
	"8.B.D.0.1.0.0.2.ip6.arpa. transparent"
};

const char	 bogus_past[]	= "validation failure <. NS IN>: signature "
				  "expired";
const char	 bogus_future[]	= "validation failure <. NS IN>: signature "
				  "before inception date";

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
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;
	struct timeval		 tv = {DECAY_PERIOD, 0};
	struct alloc_cache	 cache_alloc_test;

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

	if (unveil(TLS_DEFAULT_CA_CERT_FILE, "r") == -1)
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

	evtimer_set(&trust_anchor_timer, trust_anchor_timo, NULL);
	evtimer_set(&decay_timer, decay_latest_histograms, NULL);
	evtimer_add(&decay_timer, &tv);

	clock_gettime(CLOCK_MONOTONIC, &last_network_change);

	alloc_init(&cache_alloc_test, NULL, 0);
	if (cache_alloc_test.max_reg_blocks != 10)
		fatalx("local libunbound/util/alloc.c diff lost");
	alloc_clear(&cache_alloc_test);

	setup_unified_caches();

	TAILQ_INIT(&autoconf_forwarder_list);
	TAILQ_INIT(&trust_anchors);
	TAILQ_INIT(&new_trust_anchors);
	TAILQ_INIT(&running_queries);

	event_dispatch();

	resolver_shutdown();
}

__dead void
resolver_shutdown(void)
{
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

void
resolver_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf;
	struct imsg			 imsg;
	struct query_imsg		*query_imsg;
	ssize_t				 n;
	int				 shut = 0, verbose, i;
	char				*ta;

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
			if ((log_getverbose() & OPT_VERBOSE3)
			    != (verbose & OPT_VERBOSE3))
				restart_ub_resolvers();
			log_setverbose(verbose);
			break;
		case IMSG_QUERY:
			if (IMSG_DATA_SIZE(imsg) != sizeof(*query_imsg))
				fatalx("%s: IMSG_QUERY wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			if ((query_imsg = malloc(sizeof(*query_imsg))) ==
			    NULL) {
				log_warn("cannot allocate query");
				break;
			}
			memcpy(query_imsg, imsg.data, sizeof(*query_imsg));
			setup_query(query_imsg);
			break;
		case IMSG_CTL_STATUS:
			if (IMSG_DATA_SIZE(imsg) != 0)
				fatalx("%s: IMSG_CTL_STATUS wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			show_status(imsg.hdr.pid);
			break;
		case IMSG_CTL_AUTOCONF:
			if (IMSG_DATA_SIZE(imsg) != 0)
				fatalx("%s: IMSG_CTL_AUTOCONF wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			show_autoconf(imsg.hdr.pid);
			break;
		case IMSG_CTL_MEM:
			if (IMSG_DATA_SIZE(imsg) != 0)
				fatalx("%s: IMSG_CTL_AUTOCONF wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			show_mem(imsg.hdr.pid);
			break;
		case IMSG_NEW_TA:
			/* make sure this is a string */
			((char *)imsg.data)[IMSG_DATA_SIZE(imsg) - 1] = '\0';
			ta = imsg.data;
			add_new_ta(&new_trust_anchors, ta);
			break;
		case IMSG_NEW_TAS_ABORT:
			free_tas(&new_trust_anchors);
			break;
		case IMSG_NEW_TAS_DONE:
			if (merge_tas(&new_trust_anchors, &trust_anchors))
				restart_ub_resolvers();
			break;
		case IMSG_NETWORK_CHANGED:
			clock_gettime(CLOCK_MONOTONIC, &last_network_change);
			schedule_recheck_all_resolvers();
			for (i = 0; i < UW_RES_NONE; i++) {
				if (resolvers[i] == NULL)
					continue;
				memset(resolvers[i]->latest_histogram, 0,
				    sizeof(resolvers[i]->latest_histogram));
				resolvers[i]->median = histogram_median(
				    resolvers[i]->latest_histogram);
			}

			break;
		case IMSG_REPLACE_DNS:
			if (IMSG_DATA_SIZE(imsg) !=
			    sizeof(struct imsg_rdns_proposal))
				fatalx("%s: IMSG_ADD_DNS wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			replace_autoconf_forwarders((struct
			    imsg_rdns_proposal *)imsg.data);
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
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	ssize_t			 n;
	int			 shut = 0, i, *restart;

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

		case IMSG_STARTUP:
			if (pledge("stdio inet dns rpath", NULL) == -1)
				fatal("pledge");
			break;
		case IMSG_RECONF_CONF:
		case IMSG_RECONF_BLOCKLIST_FILE:
		case IMSG_RECONF_FORWARDER:
		case IMSG_RECONF_DOT_FORWARDER:
		case IMSG_RECONF_FORCE:
			imsg_receive_config(&imsg, &nconf);
			break;
		case IMSG_RECONF_END:
			if (nconf == NULL)
				fatalx("%s: IMSG_RECONF_END without "
				    "IMSG_RECONF_CONF", __func__);
			restart = resolvers_to_restart(resolver_conf, nconf);
			merge_config(resolver_conf, nconf);
			memset(enabled_resolvers, 0, sizeof(enabled_resolvers));
			for (i = 0; i < resolver_conf->res_pref.len; i++)
				enabled_resolvers[
				    resolver_conf->res_pref.types[i]] = 1;
			nconf = NULL;
			for (i = 0; i < UW_RES_NONE; i++)
				if (restart[i])
					new_resolver(i, UNKNOWN);
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

int
sort_resolver_types(struct resolver_preference *dst)
{
	memcpy(dst, &resolver_conf->res_pref, sizeof(*dst));

	/*
	 * Sort by resolver quality, validating > resolving etc.
	 * mergesort is stable and keeps the configured preference order
	 */
	return mergesort(dst->types, dst->len, sizeof(dst->types[0]),
	    resolver_cmp);
}

void
setup_query(struct query_imsg *query_imsg)
{
	struct running_query	*rq;
	struct uw_resolver	*res;

	if (find_running_query(query_imsg->id) != NULL) {
		free(query_imsg);
		return;
	}

	if ((rq = calloc(1, sizeof(*rq))) == NULL) {
		log_warnx(NULL);
		free(query_imsg);
		return;
	}

	clock_gettime(CLOCK_MONOTONIC, &rq->tp);
	rq->query_imsg = query_imsg;
	rq->next_resolver = 0;

	find_force(&resolver_conf->force, query_imsg->qname, &res);

	if (res != NULL && res->state != DEAD && res->state != UNKNOWN) {
		rq->res_pref.len = 1;
		rq->res_pref.types[0] = res->type;
	} else if (sort_resolver_types(&rq->res_pref) == -1) {
		log_warn("mergesort");
		free(rq->query_imsg);
		free(rq);
		return;
	}

	evtimer_set(&rq->timer_ev, try_resolver_timo, rq);

	TAILQ_INSERT_TAIL(&running_queries, rq, entry);
	try_next_resolver(rq);
}

struct running_query *
find_running_query(uint64_t id)
{
	struct running_query	*rq;

	TAILQ_FOREACH(rq, &running_queries, entry) {
		if (rq->query_imsg->id == id)
			return rq;
	}
	return NULL;
}

void
try_resolver_timo(int fd, short events, void *arg)
{
	struct running_query	*rq = arg;

	try_next_resolver(rq);
}

int
try_next_resolver(struct running_query *rq)
{
	struct uw_resolver	*res = NULL;
	struct query_imsg	*query_imsg = NULL;
	struct timespec		 tp, elapsed;
	struct timeval		 tv = {0, 0};
	int64_t			 ms;
	char			 qclass_buf[16];
	char			 qtype_buf[16];

	while(rq->next_resolver < rq->res_pref.len &&
	    ((res = resolvers[rq->res_pref.types[rq->next_resolver]]) == NULL ||
	    res->state == DEAD || res->state == UNKNOWN))
		rq->next_resolver++;

	if (res == NULL) {
		evtimer_del(&rq->timer_ev); /* we are not going to find one */
		log_debug("%s: could not find (any more) working resolvers",
		    __func__);
		goto err;
	}

	rq->next_resolver++;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	timespecsub(&tp, &rq->tp, &elapsed);
	ms = elapsed.tv_sec * 1000 + elapsed.tv_nsec / 1000000;

	sldns_wire2str_class_buf(rq->query_imsg->c, qclass_buf,
	    sizeof(qclass_buf));
	sldns_wire2str_type_buf(rq->query_imsg->t, qtype_buf,
	    sizeof(qtype_buf));
	log_debug("%s[+%lldms]: %s[%s] %s %s %s", __func__, ms,
	    uw_resolver_type_str[res->type], uw_resolver_state_str[res->state],
	    rq->query_imsg->qname, qclass_buf, qtype_buf);

	if ((query_imsg = malloc(sizeof(*query_imsg))) == NULL) {
		log_warnx("%s", __func__);
		goto err;
	}
	memcpy(query_imsg, rq->query_imsg, sizeof(*query_imsg));
	clock_gettime(CLOCK_MONOTONIC, &query_imsg->tp);

	ms = res->median;
	if (ms > NEXT_RES_MAX)
		ms = NEXT_RES_MAX;
	if (res->type == resolver_conf->res_pref.types[0])
		tv.tv_usec = 1000 * (PREF_RESOLVER_MEDIAN_SKEW + ms);
	else
		tv.tv_usec = 1000 * ms;

	while (tv.tv_usec >= 1000000) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000;
	}
	evtimer_add(&rq->timer_ev, &tv);

	rq->running++;
	if (resolve(res, query_imsg->qname, query_imsg->t,
	    query_imsg->c, query_imsg, resolve_done) != 0) {
		rq->running--;
		goto err;
	}

	return 0;

 err:
	free(query_imsg);
	if (rq->running == 0) {
		TAILQ_REMOVE(&running_queries, rq, entry);
		evtimer_del(&rq->timer_ev);
		free(rq->query_imsg);
		free(rq);
	}
	return 1;
}

int
resolve(struct uw_resolver *res, const char* name, int rrtype, int rrclass,
    void *mydata, resolve_cb_t cb)
{
	struct resolver_cb_data	*cb_data = NULL;
	struct asr_query	*aq = NULL;
	int			 err;

	resolver_ref(res);

	if ((cb_data = malloc(sizeof(*cb_data))) == NULL)
		goto err;
	cb_data->cb = cb;
	cb_data->data = mydata;
	cb_data->res = res;

	switch(res->type) {
	case UW_RES_ASR:
		if ((aq = res_query_async(name, rrclass, rrtype, res->asr_ctx))
		    == NULL) {
			log_warn("%s: res_query_async", __func__);
			goto err;
		}
		if (event_asr_run(aq, asr_resolve_done, cb_data) == NULL) {
			log_warn("%s: res_query_async", __func__);
			goto err;
		}
		break;
	case UW_RES_RECURSOR:
	case UW_RES_DHCP:
	case UW_RES_ODOT_DHCP:
	case UW_RES_FORWARDER:
	case UW_RES_ODOT_FORWARDER:
	case UW_RES_DOT:
		if ((err = ub_resolve_event(res->ctx, name,  rrtype, rrclass,
		    cb_data, ub_resolve_done, NULL)) != 0) {
			log_warn("%s: ub_resolve_event: err: %d, %s", __func__,
			    err, ub_strerror(err));
			goto err;
		}
		break;
	default:
		fatalx("unknown resolver type %d", res->type);
		break;
	}

	return 0;
 err:
	free(cb_data);
	free(aq);
	resolver_unref(res);
	return 1;
}

void
resolve_done(struct uw_resolver *res, void *arg, int rcode,
    void *answer_packet, int answer_len, int sec, char *why_bogus)
{
	struct uw_resolver	*tmp_res;
	struct ub_result	*result = NULL;
	sldns_buffer		*buf = NULL;
	struct regional		*region = NULL;
	struct query_imsg	*query_imsg;
	struct running_query	*rq;
	struct timespec		 tp, elapsed;
	int64_t			 ms;
	size_t			 i;
	int			 running_res, asr_pref_pos, force_acceptbogus;
	char			*str;
	char			 rcode_buf[16];
	char			 qclass_buf[16];
	char			 qtype_buf[16];

	clock_gettime(CLOCK_MONOTONIC, &tp);

	query_imsg = (struct query_imsg *)arg;

	timespecsub(&tp, &query_imsg->tp, &elapsed);

	ms = elapsed.tv_sec * 1000 + elapsed.tv_nsec / 1000000;

	for (i = 0; i < nitems(histogram_limits); i++) {
		if (ms < histogram_limits[i])
			break;
	}
	if (i == nitems(histogram_limits))
		log_debug("histogram bucket error");
	else {
		res->histogram[i]++;
		/* latest_histogram is in units of 1000 to avoid rounding
		   down when decaying */
		res->latest_histogram[i] += 1000;
		res->median = histogram_median(res->latest_histogram);
	}

	if ((rq = find_running_query(query_imsg->id)) == NULL)
		goto out;

	running_res = --rq->running;

	if (answer_len < LDNS_HEADER_SIZE) {
		log_warnx("bad packet: too short");
		goto servfail;
	}

	if (rcode == LDNS_RCODE_SERVFAIL) {
		if (res->stop != 1)
			check_resolver(res);
		goto servfail;
	}

	if ((result = calloc(1, sizeof(*result))) == NULL)
		goto servfail;
	if ((buf = sldns_buffer_new(answer_len)) == NULL)
		goto servfail;
	if ((region = regional_create()) == NULL)
		goto servfail;

	result->rcode = LDNS_RCODE_SERVFAIL;

	sldns_buffer_clear(buf);
	sldns_buffer_write(buf, answer_packet, answer_len);
	sldns_buffer_flip(buf);
	libworker_enter_result(result, buf, region, sec);
	result->answer_packet = NULL;
	result->answer_len = 0;

	sldns_wire2str_rcode_buf(result->rcode, rcode_buf, sizeof(rcode_buf));
	sldns_wire2str_class_buf(query_imsg->c, qclass_buf, sizeof(qclass_buf));
	sldns_wire2str_type_buf(query_imsg->t, qtype_buf, sizeof(qtype_buf));
	log_debug("%s[%s]: %s %s %s rcode: %s[%d], elapsed: %lldms, running: %d",
	    __func__, uw_resolver_type_str[res->type], query_imsg->qname,
	    qclass_buf, qtype_buf, rcode_buf, result->rcode, ms,
	    running_query_cnt());

	force_acceptbogus = find_force(&resolver_conf->force, query_imsg->qname,
	    &tmp_res);
	if (tmp_res != NULL && tmp_res->type != res->type)
		force_acceptbogus = 0;

	timespecsub(&tp, &last_network_change, &elapsed);
	if ((result->rcode == LDNS_RCODE_NXDOMAIN || sec == BOGUS) &&
	    !force_acceptbogus && res->type != UW_RES_ASR && elapsed.tv_sec <
	    DOUBT_NXDOMAIN_SEC) {
		/*
		 * Doubt NXDOMAIN or BOGUS if we just switched networks, we
		 * might be behind a captive portal.
		 */
		log_debug("%s: doubt NXDOMAIN or BOGUS from %s, network change"
		    " %llds ago", __func__, uw_resolver_type_str[res->type],
		    elapsed.tv_sec);

		/* search for ASR */
		asr_pref_pos = -1;
		for (i = 0; i < (size_t)rq->res_pref.len; i++)
			if (rq->res_pref.types[i] == UW_RES_ASR) {
				asr_pref_pos = i;
				break;
			}

		if (asr_pref_pos != -1 && resolvers[UW_RES_ASR] != NULL) {
			/* go to ASR if not yet scheduled */
			if (asr_pref_pos >= rq->next_resolver) {
				rq->next_resolver = asr_pref_pos;
				try_next_resolver(rq);
			}
			goto out;
		}
		log_debug("%s: using NXDOMAIN or BOGUS, couldn't find working "
		    "ASR", __func__);
	}

	if (log_getverbose() & OPT_VERBOSE2 && (str =
	    sldns_wire2str_pkt(answer_packet, answer_len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

	if (result->rcode == LDNS_RCODE_SERVFAIL)
		goto servfail;

	query_imsg->err = 0;

	if (sec == SECURE)
		res->state = VALIDATING;

	if (res->state == VALIDATING && sec == BOGUS) {
		query_imsg->bogus = !force_acceptbogus;
		if (query_imsg->bogus && why_bogus != NULL)
			log_warnx("%s", why_bogus);
	} else
		query_imsg->bogus = 0;

	resolver_imsg_compose_frontend(IMSG_ANSWER_HEADER, 0, query_imsg,
	    sizeof(*query_imsg));

	/* XXX imsg overflow */
	resolver_imsg_compose_frontend(IMSG_ANSWER, 0, answer_packet,
	    answer_len);

	TAILQ_REMOVE(&running_queries, rq, entry);
	evtimer_del(&rq->timer_ev);
	free(rq->query_imsg);
	free(rq);
	goto out;

 servfail:
	/* try_next_resolver() might free rq */
	if (try_next_resolver(rq) != 0 && running_res == 0) {
		/* we are the last one, send SERVFAIL */
		query_imsg->err = -4; /* UB_SERVFAIL */
		resolver_imsg_compose_frontend(IMSG_ANSWER_HEADER, 0,
		    query_imsg, sizeof(*query_imsg));
	}
 out:
	free(query_imsg);
	sldns_buffer_free(buf);
	regional_destroy(region);
	ub_resolve_free(result);
}

void
new_resolver(enum uw_resolver_type type, enum uw_resolver_state state)
{
	free_resolver(resolvers[type]);
	resolvers[type] = NULL;

	if (!enabled_resolvers[type])
		return;

	switch (type) {
	case UW_RES_ASR:
	case UW_RES_DHCP:
	case UW_RES_ODOT_DHCP:
		if (TAILQ_EMPTY(&autoconf_forwarder_list))
			return;
		break;
	case UW_RES_RECURSOR:
		break;
	case UW_RES_FORWARDER:
	case UW_RES_ODOT_FORWARDER:
		if (TAILQ_EMPTY(&resolver_conf->uw_forwarder_list))
			return;
		break;
	case UW_RES_DOT:
		if (TAILQ_EMPTY(&resolver_conf->uw_dot_forwarder_list))
			return;
		break;
	case UW_RES_NONE:
		fatalx("cannot create UW_RES_NONE resolver");
	}

	switch (type) {
	case UW_RES_RECURSOR:
	case UW_RES_DHCP:
	case UW_RES_ODOT_DHCP:
	case UW_RES_FORWARDER:
	case UW_RES_ODOT_FORWARDER:
	case UW_RES_DOT:
		if (TAILQ_EMPTY(&trust_anchors))
			return;
		break;
	case UW_RES_ASR:
		break;
	case UW_RES_NONE:
		fatalx("cannot create UW_RES_NONE resolver");
	}

	if ((resolvers[type] = create_resolver(type)) == NULL)
		return;

	switch (state) {
	case UNKNOWN:
		check_resolver(resolvers[type]);
		break;
	case VALIDATING:
		set_unified_cache(resolvers[type]);
		/* FALLTHROUGH */
	case RESOLVING:
		resolvers[type]->state = state;
		break;
	default:
		fatalx("%s: invalid resolver state: %s", __func__,
		    uw_resolver_state_str[state]);
	}
}

void
set_unified_cache(struct uw_resolver *res)
{
	if (res == NULL || res->ctx == NULL)
		return;

	if (res->ctx->env->msg_cache != NULL) {
		/* XXX we are currently not using this */
		if (res->ctx->env->msg_cache != unified_msg_cache ||
		    res->ctx->env->rrset_cache != unified_rrset_cache ||
		    res->ctx->env->key_cache != unified_key_cache ||
		    res->ctx->env->neg_cache != unified_neg_cache)
			fatalx("wrong unified cache set on resolver");
		else
			/* we are upgrading from UNKNOWN back to VALIDATING */
			return;
	}

	res->ctx->env->msg_cache = unified_msg_cache;
	res->ctx->env->rrset_cache = unified_rrset_cache;
	res->ctx->env->key_cache = unified_key_cache;
	res->ctx->env->neg_cache = unified_neg_cache;

	context_finalize(res->ctx);

	if (res->ctx->env->msg_cache != unified_msg_cache ||
	    res->ctx->env->rrset_cache != unified_rrset_cache ||
	    res->ctx->env->key_cache != unified_key_cache ||
	    res->ctx->env->neg_cache != unified_neg_cache)
		fatalx("failed to set unified caches, libunbound/validator/"
		    "validator.c diff lost");
}

static const struct {
	const char *name;
	const char *value;
} options[] = {
	{ "aggressive-nsec:", "yes" },
	{ "fast-server-permil:", "950" },
	{ "edns-buffer-size:", "1232" },
	{ "target-fetch-policy:", "0 0 0 0 0" },
	{ "outgoing-range:", "64" }
};

struct uw_resolver *
create_resolver(enum uw_resolver_type type)
{
	struct uw_resolver	*res;
	struct trust_anchor	*ta;
	struct uw_forwarder	*uw_forwarder;
	size_t			 i;
	int			 err;
	char			*resolv_conf = NULL, *tmp = NULL;

	if ((res = calloc(1, sizeof(*res))) == NULL) {
		log_warn("%s", __func__);
		return (NULL);
	}

	res->type = type;
	res->state = UNKNOWN;
	res->check_tv.tv_sec = RESOLVER_CHECK_SEC;
	res->check_tv.tv_usec = arc4random() % 1000000; /* modulo bias is ok */

	switch (type) {
	case UW_RES_ASR:
		if (TAILQ_EMPTY(&autoconf_forwarder_list)) {
			free(res);
			return (NULL);
		}
		TAILQ_FOREACH(uw_forwarder, &autoconf_forwarder_list, entry) {
			tmp = resolv_conf;
			if (asprintf(&resolv_conf, "%snameserver %s\n", tmp ==
			    NULL ? "" : tmp, uw_forwarder->ip) == -1) {
				free(tmp);
				free(res);
				log_warnx("could not create asr context");
				return (NULL);
			}
			free(tmp);
		}

		if ((res->asr_ctx = asr_resolver_from_string(resolv_conf)) ==
		    NULL) {
			free(res);
			free(resolv_conf);
			log_warnx("could not create asr context");
			return (NULL);
		}
		free(resolv_conf);
		break;
	case UW_RES_RECURSOR:
	case UW_RES_DHCP:
	case UW_RES_ODOT_DHCP:
	case UW_RES_FORWARDER:
	case UW_RES_ODOT_FORWARDER:
	case UW_RES_DOT:
		if ((res->ctx = ub_ctx_create_event(ev_base)) == NULL) {
			free(res);
			log_warnx("could not create unbound context");
			return (NULL);
		}

		ub_ctx_debuglevel(res->ctx, log_getverbose() & OPT_VERBOSE3 ?
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

		for (i = 0; i < nitems(options); i++) {
			if ((err = ub_ctx_set_option(res->ctx, options[i].name,
			    options[i].value)) != 0) {
				ub_ctx_delete(res->ctx);
				free(res);
				log_warnx("error setting %s: %s: %s",
				    options[i].name, options[i].value,
				    ub_strerror(err));
				return (NULL);
			}
		}

		if (!log_getdebug()) {
			if((err = ub_ctx_set_option(res->ctx, "use-syslog:",
			    "yes")) != 0) {
				ub_ctx_delete(res->ctx);
				free(res);
				log_warnx("error setting use-syslog: yes: %s",
				    ub_strerror(err));
				return (NULL);
			}
		}

		break;
	default:
		fatalx("unknown resolver type %d", type);
		break;
	}

	evtimer_set(&res->check_ev, resolver_check_timo, res);

	switch(res->type) {
	case UW_RES_ASR:
		break;
	case UW_RES_RECURSOR:
		break;
	case UW_RES_DHCP:
		set_forwarders(res, &autoconf_forwarder_list, 0);
		break;
	case UW_RES_ODOT_DHCP:
		set_forwarders(res, &autoconf_forwarder_list, 853);
		ub_ctx_set_option(res->ctx, "tls-cert-bundle:",
		    TLS_DEFAULT_CA_CERT_FILE);
		ub_ctx_set_tls(res->ctx, 1);
		break;
	case UW_RES_FORWARDER:
		set_forwarders(res, &resolver_conf->uw_forwarder_list, 0);
		break;
	case UW_RES_ODOT_FORWARDER:
		set_forwarders(res, &resolver_conf->uw_forwarder_list, 853);
		ub_ctx_set_option(res->ctx, "tls-cert-bundle:",
		    TLS_DEFAULT_CA_CERT_FILE);
		ub_ctx_set_tls(res->ctx, 1);
		break;
	case UW_RES_DOT:
		set_forwarders(res, &resolver_conf->uw_dot_forwarder_list, 0);
		ub_ctx_set_option(res->ctx, "tls-cert-bundle:",
		    TLS_DEFAULT_CA_CERT_FILE);
		ub_ctx_set_tls(res->ctx, 1);
		break;
	default:
		fatalx("unknown resolver type %d", type);
		break;
	}

	/* for the forwarder cases allow AS112 zones */
	switch(res->type) {
	case UW_RES_DHCP:
	case UW_RES_ODOT_DHCP:
	case UW_RES_FORWARDER:
	case UW_RES_ODOT_FORWARDER:
	case UW_RES_DOT:
		for (i = 0; i < nitems(as112_zones); i++) {
			if((err = ub_ctx_set_option(res->ctx, "local-zone:",
			    as112_zones[i])) != 0) {
				ub_ctx_delete(res->ctx);
				free(res);
				log_warnx("error setting local-zone: %s: %s",
				    as112_zones[i], ub_strerror(err));
				return (NULL);
			}
		}
		break;
	default:
		break;
	}

	return (res);
}

void
free_resolver(struct uw_resolver *res)
{
	struct val_env	*val_env;

	if (res == NULL)
		return;

	if (res->ref_cnt > 0)
		res->stop = 1;
	else {
		evtimer_del(&res->check_ev);
		if (res->ctx != NULL) {
			if (res->ctx->env->msg_cache == unified_msg_cache) {
				val_env = (struct val_env*)
				    res->ctx->env->modinfo[val_id];
				res->ctx->env->msg_cache = NULL;
				res->ctx->env->rrset_cache = NULL;
				val_env->kcache = NULL;
				res->ctx->env->key_cache = NULL;
				val_env->neg_cache = NULL;
				res->ctx->env->neg_cache = NULL;
			}
		}
		ub_ctx_delete(res->ctx);
		asr_resolver_free(res->asr_ctx);
		free(res);
	}
}

void
setup_unified_caches(void)
{
	struct ub_ctx	*ctx;
	struct val_env	*val_env;
	size_t		 i;
	int		 err, j;

	if ((ctx = ub_ctx_create_event(ev_base)) == NULL)
		fatalx("could not create unbound context");

	for (i = 0; i < nitems(options); i++) {
		if ((err = ub_ctx_set_option(ctx, options[i].name,
		    options[i].value)) != 0) {
			fatalx("error setting %s: %s: %s", options[i].name,
			    options[i].value, ub_strerror(err));
		}
	}

	context_finalize(ctx);

	if (ctx->env->msg_cache == NULL || ctx->env->rrset_cache == NULL ||
	    ctx->env->key_cache == NULL || ctx->env->neg_cache == NULL)
		fatalx("could not setup unified caches");

	unified_msg_cache = ctx->env->msg_cache;
	unified_rrset_cache = ctx->env->rrset_cache;
	unified_key_cache = ctx->env->key_cache;
	unified_neg_cache = ctx->env->neg_cache;

	if (val_id == -1) {
		for (j = 0; j < ctx->mods.num; j++) {
			if (strcmp(ctx->mods.mod[j]->name, "validator") == 0) {
				val_id = j;
				break;
			}
		}
		if (val_id == -1)
			fatalx("cannot find validator module");
	}

	val_env = (struct val_env*)ctx->env->modinfo[val_id];
	ctx->env->msg_cache = NULL;
	ctx->env->rrset_cache = NULL;
	ctx->env->key_cache = NULL;
	val_env->kcache = NULL;
	ctx->env->neg_cache = NULL;
	val_env->neg_cache = NULL;
	ub_ctx_delete(ctx);
}

void
set_forwarders(struct uw_resolver *res, struct uw_forwarder_head
    *uw_forwarder_list, int port_override)
{
	struct uw_forwarder	*uw_forwarder;
	int			 ret;
	char			 fwd[FWD_MAX];

	TAILQ_FOREACH(uw_forwarder, uw_forwarder_list, entry) {
		if (uw_forwarder->auth_name[0] != '\0')
			ret = snprintf(fwd, sizeof(fwd), "%s@%d#%s",
			    uw_forwarder->ip, port_override ? port_override :
			    uw_forwarder->port, uw_forwarder->auth_name);
		else
			ret = snprintf(fwd, sizeof(fwd), "%s@%d",
			    uw_forwarder->ip, port_override ? port_override :
			    uw_forwarder->port);

		if (ret < 0 || (size_t)ret >= sizeof(fwd)) {
			log_warnx("forwarder too long");
			continue;
		}

		ub_ctx_set_fwd(res->ctx, fwd);
	}
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
check_resolver(struct uw_resolver *resolver_to_check)
{
	struct uw_resolver		*res;

	if (resolver_to_check == NULL)
		return;

	if (resolver_to_check->check_running)
		return;

	if ((res = create_resolver(resolver_to_check->type)) == NULL)
		return;

	resolver_ref(resolver_to_check);

	resolver_to_check->check_running++;
	if (resolve(res, ".", LDNS_RR_TYPE_NS, LDNS_RR_CLASS_IN,
	    resolver_to_check, check_resolver_done) != 0) {
		resolver_to_check->check_running--;
		resolver_to_check->state = UNKNOWN;
		resolver_unref(resolver_to_check);
		resolver_to_check->check_tv.tv_sec = RESOLVER_CHECK_SEC;
		evtimer_add(&resolver_to_check->check_ev,
		    &resolver_to_check->check_tv);
	}


}

void
check_resolver_done(struct uw_resolver *res, void *arg, int rcode,
    void *answer_packet, int answer_len, int sec, char *why_bogus)
{
	struct uw_resolver	*checked_resolver = arg;
	struct timeval		 tv = {0, 1};
	enum uw_resolver_state	 prev_state;
	int			 bogus_time = 0;
	char			*str;

	checked_resolver->check_running--;

	prev_state = checked_resolver->state;

	if (answer_len < LDNS_HEADER_SIZE) {
		checked_resolver->state = DEAD;
		log_warnx("%s: bad packet: too short", __func__);
		goto out;
	}

	if (rcode == LDNS_RCODE_SERVFAIL) {
		log_debug("%s: %s rcode: SERVFAIL", __func__,
		    uw_resolver_type_str[checked_resolver->type]);

		checked_resolver->state = DEAD;
		goto out;
	}

	if (sec == SECURE) {
		if (prev_state != VALIDATING)
			new_resolver(checked_resolver->type, VALIDATING);
		if (!(evtimer_pending(&trust_anchor_timer, NULL)))
			evtimer_add(&trust_anchor_timer, &tv);
	 } else if (rcode == LDNS_RCODE_NOERROR &&
	    LDNS_RCODE_WIRE((uint8_t*)answer_packet) == LDNS_RCODE_NOERROR) {
		if (why_bogus) {
			bogus_time = strncmp(why_bogus, bogus_past,
			    sizeof(bogus_past) - 1) == 0 || strncmp(why_bogus,
			    bogus_future, sizeof(bogus_future) - 1) == 0;

			log_warnx("%s: %s", uw_resolver_type_str[
			    checked_resolver->type], why_bogus);
		}
		if (prev_state != RESOLVING)
			new_resolver(checked_resolver->type, RESOLVING);
	} else
		checked_resolver->state = DEAD; /* we know the root exists */

	log_debug("%s: %s: %s", __func__,
	    uw_resolver_type_str[checked_resolver->type],
	    uw_resolver_state_str[checked_resolver->state]);

	if (log_getverbose() & OPT_VERBOSE2 && (str =
	    sldns_wire2str_pkt(answer_packet, answer_len)) != NULL) {
		log_debug("%s", str);
		free(str);
	}

out:
	if (!checked_resolver->stop && (checked_resolver->state == DEAD ||
	    bogus_time)) {
		if (prev_state == DEAD || bogus_time)
			checked_resolver->check_tv.tv_sec *= 2;
		else
			checked_resolver->check_tv.tv_sec = RESOLVER_CHECK_SEC;

		if (checked_resolver->check_tv.tv_sec > RESOLVER_CHECK_MAXSEC)
			checked_resolver->check_tv.tv_sec =
			    RESOLVER_CHECK_MAXSEC;

		evtimer_add(&checked_resolver->check_ev,
		    &checked_resolver->check_tv);
	}

	resolver_unref(checked_resolver);
	res->stop = 1; /* do not free in callback */
}

void
asr_resolve_done(struct asr_result *ar, void *arg)
{
	struct resolver_cb_data	*cb_data = arg;
	cb_data->cb(cb_data->res, cb_data->data, ar->ar_rcode, ar->ar_data,
	    ar->ar_datalen, 0, NULL);
	free(ar->ar_data);
	resolver_unref(cb_data->res);
	free(cb_data);
}

void
ub_resolve_done(void *arg, int rcode, void *answer_packet, int answer_len,
    int sec, char *why_bogus, int was_ratelimited)
{
	struct resolver_cb_data	*cb_data = arg;
	cb_data->cb(cb_data->res, cb_data->data, rcode, answer_packet,
	    answer_len, sec, why_bogus);
	resolver_unref(cb_data->res);
	free(cb_data);
}

void
schedule_recheck_all_resolvers(void)
{
	struct timeval	 tv;
	int		 i;

	tv.tv_sec = 0;

	for (i = 0; i < UW_RES_NONE; i++) {
		if (resolvers[i] == NULL)
			continue;
		tv.tv_usec = arc4random() % 1000000; /* modulo bias is ok */
		resolvers[i]->state = UNKNOWN;
		evtimer_add(&resolvers[i]->check_ev, &tv);
	}
}

int
check_forwarders_changed(struct uw_forwarder_head *list_a,
    struct uw_forwarder_head *list_b)
{
	struct uw_forwarder	*a, *b;

	a = TAILQ_FIRST(list_a);
	b = TAILQ_FIRST(list_b);

	while(a != NULL && b != NULL) {
		if (strcmp(a->ip, b->ip) != 0)
			return 1;
		if (a->port != b->port)
			return 1;
		if (strcmp(a->auth_name, b->auth_name) != 0)
			return 1;
		a = TAILQ_NEXT(a, entry);
		b = TAILQ_NEXT(b, entry);
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
	    TAILQ_FIRST(old_list)) != NULL) {
		TAILQ_REMOVE(old_list, uw_forwarder, entry);
		free(uw_forwarder);
	}

	TAILQ_CONCAT(old_list, new_list, entry);
}

int
resolver_cmp(const void *_a, const void *_b)
{
	const enum uw_resolver_type	 a = *(const enum uw_resolver_type *)_a;
	const enum uw_resolver_type	 b = *(const enum uw_resolver_type *)_b;
	int64_t				 a_median, b_median;

	if (resolvers[a] == NULL && resolvers[b] == NULL)
		return 0;

	if (resolvers[b] == NULL)
		return -1;

	if (resolvers[a] == NULL)
		return 1;

	if (resolvers[a]->state < resolvers[b]->state)
		return 1;
	else if (resolvers[a]->state > resolvers[b]->state)
		return -1;
	else {
		a_median = resolvers[a]->median;
		b_median = resolvers[b]->median;
		if (resolvers[a]->type == resolver_conf->res_pref.types[0])
			a_median -= PREF_RESOLVER_MEDIAN_SKEW;
		else if (resolvers[b]->type == resolver_conf->res_pref.types[0])
			b_median -= PREF_RESOLVER_MEDIAN_SKEW;
		if (a_median < b_median)
			return -1;
		else if (a_median > b_median)
			return 1;
		else
			return 0;
	}
}

void
restart_ub_resolvers(void)
{
	int	 i;

	for (i = 0; i < UW_RES_NONE; i++)
		if (i != UW_RES_ASR)
			new_resolver(i, resolvers[i] != NULL ?
			    resolvers[i]->state : UNKNOWN);
}

void
show_status(pid_t pid)
{
	struct resolver_preference	 res_pref;
	int				 i;

	if (sort_resolver_types(&res_pref) == -1)
		log_warn("mergesort");

	for (i = 0; i < resolver_conf->res_pref.len; i++)
		send_resolver_info(resolvers[res_pref.types[i]], pid);

	resolver_imsg_compose_frontend(IMSG_CTL_END, pid, NULL, 0);
}

void
show_autoconf(pid_t pid)
{
	struct uw_forwarder		*uw_forwarder;
	struct ctl_forwarder_info	 cfi;

	TAILQ_FOREACH(uw_forwarder, &autoconf_forwarder_list, entry) {
		memset(&cfi, 0, sizeof(cfi));
		cfi.if_index = uw_forwarder->if_index;
		cfi.src = uw_forwarder->src;
		/* no truncation, structs are in sync */
		memcpy(cfi.ip, uw_forwarder->ip, sizeof(cfi.ip));
		resolver_imsg_compose_frontend(
		    IMSG_CTL_AUTOCONF_RESOLVER_INFO,
		    pid, &cfi, sizeof(cfi));
	}

	resolver_imsg_compose_frontend(IMSG_CTL_END, pid, NULL, 0);
}

void
show_mem(pid_t pid)
{
	struct ctl_mem_info	 cmi;

	memset(&cmi, 0, sizeof(cmi));
	cmi.msg_cache_used = slabhash_get_mem(unified_msg_cache);
	cmi.msg_cache_max = slabhash_get_size(unified_msg_cache);
	cmi.rrset_cache_used = slabhash_get_mem(&unified_rrset_cache->table);
	cmi.rrset_cache_max = slabhash_get_size(&unified_rrset_cache->table);
	cmi.key_cache_used = slabhash_get_mem(unified_key_cache->slab);
	cmi.key_cache_max = slabhash_get_size(unified_key_cache->slab);
	cmi.neg_cache_used = unified_neg_cache->use;
	cmi.neg_cache_max = unified_neg_cache->max;
	resolver_imsg_compose_frontend(IMSG_CTL_MEM_INFO, pid, &cmi,
	    sizeof(cmi));

}

void
send_resolver_info(struct uw_resolver *res, pid_t pid)
{
	struct ctl_resolver_info	 cri;
	size_t				 i;

	if (res == NULL)
		return;

	cri.state = res->state;
	cri.type = res->type;
	cri.median = res->median;

	memcpy(cri.histogram, res->histogram, sizeof(cri.histogram));
	memcpy(cri.latest_histogram, res->latest_histogram,
	    sizeof(cri.latest_histogram));
	for (i = 0; i < nitems(histogram_limits); i++)
		cri.latest_histogram[i] =
		    (cri.latest_histogram[i] + 500) / 1000;

	resolver_imsg_compose_frontend(IMSG_CTL_RESOLVER_INFO, pid, &cri,
	    sizeof(cri));
}

void
trust_anchor_resolve(void)
{
	struct resolver_preference	 res_pref;
	struct uw_resolver		*res;
	struct timeval			 tv = {TRUST_ANCHOR_RETRY_INTERVAL, 0};

	if (sort_resolver_types(&res_pref) == -1)
		log_warn("mergesort");

	res = resolvers[res_pref.types[0]];

	if (res == NULL || res->state < VALIDATING)
		goto err;

	if (resolve(res, ".",  LDNS_RR_TYPE_DNSKEY, LDNS_RR_CLASS_IN, NULL,
	    trust_anchor_resolve_done) != 0)
		goto err;

	return;
 err:
	evtimer_add(&trust_anchor_timer, &tv);
}

void
trust_anchor_timo(int fd, short events, void *arg)
{
	trust_anchor_resolve();
}

void
trust_anchor_resolve_done(struct uw_resolver *res, void *arg, int rcode,
    void *answer_packet, int answer_len, int sec, char *why_bogus)
{
	struct ub_result	*result = NULL;
	sldns_buffer		*buf = NULL;
	struct regional		*region = NULL;
	struct timeval		 tv = {TRUST_ANCHOR_RETRY_INTERVAL, 0};
	int			 i, tas, n;
	uint16_t		 dnskey_flags;
	char			 rdata_buf[1024], *ta;

	if (answer_len < LDNS_HEADER_SIZE) {
		log_warnx("bad packet: too short");
		goto out;
	}

	if ((result = calloc(1, sizeof(*result))) == NULL)
		goto out;

	if (sec != SECURE)
		goto out;

	if ((buf = sldns_buffer_new(answer_len)) == NULL)
		goto out;
	if ((region = regional_create()) == NULL)
		goto out;
	result->rcode = LDNS_RCODE_SERVFAIL;

	sldns_buffer_clear(buf);
	sldns_buffer_write(buf, answer_packet, answer_len);
	sldns_buffer_flip(buf);
	libworker_enter_result(result, buf, region, sec);
	result->answer_packet = NULL;
	result->answer_len = 0;

	if (result->rcode != LDNS_RCODE_NOERROR)
		goto out;

	i = 0;
	tas = 0;
	while(result->data[i] != NULL) {
		if (result->len[i] < 2) {
			if (tas > 0)
				resolver_imsg_compose_frontend(
				    IMSG_NEW_TAS_ABORT, 0, NULL, 0);
			goto out;
		}
		n = sldns_wire2str_rdata_buf(result->data[i], result->len[i],
		    rdata_buf, sizeof(rdata_buf), LDNS_RR_TYPE_DNSKEY);

		if (n < 0 || (size_t)n >= sizeof(rdata_buf)) {
			log_warnx("trust anchor buffer to small");
			resolver_imsg_compose_frontend(IMSG_NEW_TAS_ABORT, 0,
			    NULL, 0);
			goto out;
		}

		memcpy(&dnskey_flags, result->data[i], 2);
		dnskey_flags = ntohs(dnskey_flags);
		if ((dnskey_flags & LDNS_KEY_SEP_KEY) && !(dnskey_flags &
		    LDNS_KEY_REVOKE_KEY)) {
			asprintf(&ta, ".\t%d\tIN\tDNSKEY\t%s", ROOT_DNSKEY_TTL,
			    rdata_buf);
			resolver_imsg_compose_frontend(IMSG_NEW_TA, 0, ta,
			    strlen(ta) + 1);
			tas++;
			free(ta);
		}
		i++;
	}
	if (tas > 0) {
		resolver_imsg_compose_frontend(IMSG_NEW_TAS_DONE, 0, NULL, 0);
		tv.tv_sec = TRUST_ANCHOR_QUERY_INTERVAL;
	}
out:
	sldns_buffer_free(buf);
	regional_destroy(region);
	ub_resolve_free(result);
	evtimer_add(&trust_anchor_timer, &tv);
}

void
replace_autoconf_forwarders(struct imsg_rdns_proposal *rdns_proposal)
{
	struct uw_forwarder_head	 new_forwarder_list;
	struct uw_forwarder		*uw_forwarder, *tmp;
	int				 i, rdns_count, af, changed = 0;
	char				 ntopbuf[INET6_ADDRSTRLEN], *src;
	const char			*ns;

	TAILQ_INIT(&new_forwarder_list);
	af = rdns_proposal->rtdns.sr_family;
	src = rdns_proposal->rtdns.sr_dns;

	switch (af) {
	case AF_INET:
		rdns_count = (rdns_proposal->rtdns.sr_len -
		    offsetof(struct sockaddr_rtdns, sr_dns)) /
		    sizeof(struct in_addr);
		break;
	case AF_INET6:
		rdns_count = (rdns_proposal->rtdns.sr_len -
		    offsetof(struct sockaddr_rtdns, sr_dns)) /
		    sizeof(struct in6_addr);
		break;
	default:
		log_warnx("%s: unsupported address family: %d", __func__, af);
		return;
	}

	for (i = 0; i < rdns_count; i++) {
		switch (af) {
		case AF_INET:
			if (((struct in_addr *)src)->s_addr == INADDR_LOOPBACK)
				continue;
			ns = inet_ntop(af, (struct in_addr *)src, ntopbuf,
			    INET6_ADDRSTRLEN);
			src += sizeof(struct in_addr);
			break;
		case AF_INET6:
			if (IN6_IS_ADDR_LOOPBACK((struct in6_addr *)src))
				continue;
			ns = inet_ntop(af, (struct in6_addr *)src, ntopbuf,
			    INET6_ADDRSTRLEN);
			src += sizeof(struct in6_addr);
		}

		if ((uw_forwarder = calloc(1, sizeof(struct uw_forwarder))) ==
		    NULL)
			fatal(NULL);
		if (strlcpy(uw_forwarder->ip, ns, sizeof(uw_forwarder->ip))
		    >= sizeof(uw_forwarder->ip))
			fatalx("strlcpy");
		uw_forwarder->port = 53;
		uw_forwarder->if_index = rdns_proposal->if_index;
		uw_forwarder->src = rdns_proposal->src;
		TAILQ_INSERT_TAIL(&new_forwarder_list, uw_forwarder, entry);
	}

	TAILQ_FOREACH(tmp, &autoconf_forwarder_list, entry) {
		/* if_index of zero signals to clear all proposals */
		if (rdns_proposal->src == tmp->src &&
		    (rdns_proposal->if_index == 0 || rdns_proposal->if_index ==
		    tmp->if_index))
			continue;
		if ((uw_forwarder = calloc(1, sizeof(struct uw_forwarder))) ==
		    NULL)
			fatal(NULL);
		if (strlcpy(uw_forwarder->ip, tmp->ip,
		    sizeof(uw_forwarder->ip)) >= sizeof(uw_forwarder->ip))
			fatalx("strlcpy");
		uw_forwarder->port = tmp->port;
		uw_forwarder->src = tmp->src;
		uw_forwarder->if_index = tmp->if_index;
		TAILQ_INSERT_TAIL(&new_forwarder_list, uw_forwarder, entry);
	}

	changed = check_forwarders_changed(&new_forwarder_list,
	    &autoconf_forwarder_list);

	if (changed) {
		replace_forwarders(&new_forwarder_list,
		    &autoconf_forwarder_list);
		new_resolver(UW_RES_ASR, UNKNOWN);
		new_resolver(UW_RES_DHCP, UNKNOWN);
		new_resolver(UW_RES_ODOT_DHCP, UNKNOWN);
	} else {
		while ((tmp = TAILQ_FIRST(&new_forwarder_list)) != NULL) {
			TAILQ_REMOVE(&new_forwarder_list, tmp, entry);
			free(tmp);
		}
	}
}

int
force_tree_cmp(struct force_tree_entry *a, struct force_tree_entry *b)
{
	return strcasecmp(a->domain, b->domain);
}

int
find_force(struct force_tree *tree, char *qname, struct uw_resolver **res)
{
	struct force_tree_entry	*n, e;
	char 			*p;

	if (res)
		*res = NULL;
	if (RB_EMPTY(tree))
		return 0;

	p = qname;
	do {
		if (strlcpy(e.domain, p, sizeof(e.domain)) >= sizeof(e.domain))
			fatal("qname too large");
		n = RB_FIND(force_tree, tree, &e);
		if (n != NULL) {
			log_debug("%s: %s -> %s[%s]", __func__, qname, p,
			    uw_resolver_type_str[n->type]);
			if (res)
				*res = resolvers[n->type];
			return n->acceptbogus;
		}
		if (*p == '.')
			p++;
		p = strchr(p, '.');
		if (p != NULL && p[1] != '\0')
			p++;
	} while (p != NULL);
	return 0;

}

int64_t
histogram_median(int64_t *histogram)
{
	size_t	 i;
	int64_t	 sample_count = 0, running_count = 0;

	/* skip first bucket, it contains cache hits */
	for (i = 1; i < nitems(histogram_limits); i++)
		sample_count += histogram[i];

	if (sample_count == 0)
		return 0;

	for (i = 1; i < nitems(histogram_limits); i++) {
		running_count += histogram[i];
		if (running_count >= sample_count / 2)
			break;
	}

	if (i >= nitems(histogram_limits) - 1)
		return INT64_MAX;
	return (histogram_limits[i - 1] + histogram_limits[i]) / 2;
}

void
decay_latest_histograms(int fd, short events, void *arg)
{
	enum uw_resolver_type	 i;
	size_t			 j;
	struct uw_resolver	*res;
	struct timeval		 tv = {DECAY_PERIOD, 0};

	for (i = 0; i < UW_RES_NONE; i++) {
		res = resolvers[i];
		if (res == NULL)
			continue;
		for (j = 0; j < nitems(res->latest_histogram); j++)
			/* multiply then divide, avoiding truncating to 0 */
			res->latest_histogram[j] = res->latest_histogram[j] *
			    DECAY_NOMINATOR / DECAY_DENOMINATOR;
		res->median = histogram_median(res->latest_histogram);
	}
	evtimer_add(&decay_timer, &tv);
}

int
running_query_cnt(void)
{
	struct running_query	*e;
	int			 cnt = 0;

	TAILQ_FOREACH(e, &running_queries, entry)
		cnt++;
	return cnt;
}

int *
resolvers_to_restart(struct uw_conf *oconf, struct uw_conf *nconf)
{
	static int	 restart[UW_RES_NONE];
	int		 o_enabled[UW_RES_NONE];
	int		 n_enabled[UW_RES_NONE];
	int		 i;

	memset(&restart, 0, sizeof(restart));
	if (check_forwarders_changed(&oconf->uw_forwarder_list,
	    &nconf->uw_forwarder_list)) {
		restart[UW_RES_FORWARDER] = 1;
		restart[UW_RES_ODOT_FORWARDER] = 1;
	}
	if (check_forwarders_changed(&oconf->uw_dot_forwarder_list,
	    &nconf->uw_dot_forwarder_list)) {
		restart[UW_RES_DOT] = 1;
	}
	memset(o_enabled, 0, sizeof(o_enabled));
	memset(n_enabled, 0, sizeof(n_enabled));
	for (i = 0; i < oconf->res_pref.len; i++)
		o_enabled[oconf->res_pref.types[i]] = 1;

	for (i = 0; i < nconf->res_pref.len; i++)
		n_enabled[nconf->res_pref.types[i]] = 1;

	for (i = 0; i < UW_RES_NONE; i++) {
		if (n_enabled[i] != o_enabled[i])
			restart[i] = 1;
	}
	return restart;
}
