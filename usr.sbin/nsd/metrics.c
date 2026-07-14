/*
 * metrics.c -- prometheus metrics endpoint
 *
 * Copyright (c) 2001-2025, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */

#include "config.h"

#ifdef USE_METRICS

#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <event2/event.h>
#include <event2/http.h>
#include <ctype.h>

#include "nsd.h"
#include "xfrd.h"
#include "options.h"
#include "remote.h"
#include "metrics.h"

/** if you want zero to be inhibited in stats output.
 * it omits zeroes for types that have no acronym and unused-rcodes */
const int metrics_inhibit_zero = 1;

/**
 * list of connection accepting file descriptors
 */
struct metrics_acceptlist {
	struct metrics_acceptlist* next;
	int accept_fd;
	char* ident;
	struct daemon_metrics* metrics;
};

/**
 * The metrics daemon state.
 */
struct daemon_metrics {
	/** the master process for this metrics daemon */
	struct xfrd_state* xfrd;
	/** commpoints for accepting HTTP connections */
	struct metrics_acceptlist* accept_list;
	/** last time stats was reported */
	struct timeval stats_time, boot_time;
	/** libevent http server */
	struct evhttp *http_server;
};

static void
metrics_http_callback(struct evhttp_request *req, void *p);

struct daemon_metrics*
daemon_metrics_create(struct nsd_options* cfg)
{
	struct daemon_metrics* metrics = (struct daemon_metrics*)xalloc_zero(
		sizeof(*metrics));
	assert(cfg->metrics_enable);

	/* and try to open the ports */
	if(!daemon_metrics_open_ports(metrics, cfg)) {
		log_msg(LOG_ERR, "could not open metrics port");
		daemon_metrics_delete(metrics);
		return NULL;
	}

	if(gettimeofday(&metrics->boot_time, NULL) == -1)
		log_msg(LOG_ERR, "gettimeofday: %s", strerror(errno));
	metrics->stats_time = metrics->boot_time;

	return metrics;
}

void daemon_metrics_close(struct daemon_metrics* metrics)
{
	struct metrics_acceptlist *h, *nh;
	if(!metrics) return;

	/* close listen sockets */
	h = metrics->accept_list;
	while(h) {
		nh = h->next;
		close(h->accept_fd);
		free(h->ident);
		free(h);
		h = nh;
	}
	metrics->accept_list = NULL;

	if (metrics->http_server) {
		evhttp_free(metrics->http_server);
		metrics->http_server = NULL;
	}
}

void daemon_metrics_delete(struct daemon_metrics* metrics)
{
	if(!metrics) return;
	daemon_metrics_close(metrics);
	free(metrics);
}

static int
create_tcp_accept_sock(struct addrinfo* addr, int* noproto)
{
#if defined(SO_REUSEADDR) || (defined(INET6) && (defined(IPV6_V6ONLY) || defined(IPV6_USE_MIN_MTU) || defined(IPV6_MTU)))
	int on = 1;
#endif
	int s;
	*noproto = 0;
	if ((s = socket(addr->ai_family, addr->ai_socktype, 0)) == -1) {
#if defined(INET6)
		if (addr->ai_family == AF_INET6 &&
			errno == EAFNOSUPPORT) {
			*noproto = 1;
			log_msg(LOG_WARNING, "fallback to TCP4, no IPv6: not supported");
			return -1;
		}
#endif /* INET6 */
		log_msg(LOG_ERR, "can't create a socket: %s", strerror(errno));
		return -1;
	}
#ifdef  SO_REUSEADDR
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		log_msg(LOG_ERR, "setsockopt(..., SO_REUSEADDR, ...) failed: %s", strerror(errno));
	}
#endif /* SO_REUSEADDR */
#if defined(INET6) && defined(IPV6_V6ONLY)
	if (addr->ai_family == AF_INET6 &&
		setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) < 0)
	{
		log_msg(LOG_ERR, "setsockopt(..., IPV6_V6ONLY, ...) failed: %s", strerror(errno));
		close(s);
		return -1;
	}
#endif
	/* set it nonblocking */
	/* (StevensUNP p463), if tcp listening socket is blocking, then
	   it may block in accept, even if select() says readable. */
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) {
		log_msg(LOG_ERR, "cannot fcntl tcp: %s", strerror(errno));
	}
	/* Bind it... */
	if (bind(s, (struct sockaddr *)addr->ai_addr, addr->ai_addrlen) != 0) {
		log_msg(LOG_ERR, "can't bind tcp socket: %s", strerror(errno));
		close(s);
		return -1;
	}
	/* Listen to it... */
	if (listen(s, TCP_BACKLOG_METRICS) == -1) {
		log_msg(LOG_ERR, "can't listen: %s", strerror(errno));
		close(s);
		return -1;
	}
	return s;
}

/**
 * Add and open a new metrics port
 * @param metrics: metrics with result list.
 * @param ip: ip str
 * @param nr: port nr
 * @param noproto_is_err: if lack of protocol support is an error.
 * @return false on failure.
 */
static int
metrics_add_open(struct daemon_metrics* metrics, struct nsd_options* cfg, const char* ip,
	int nr, int noproto_is_err)
{
	struct addrinfo hints;
	struct addrinfo* res;
	struct metrics_acceptlist* hl;
	int noproto = 0;
	int fd, r;
	char port[15];
	snprintf(port, sizeof(port), "%d", nr);
	port[sizeof(port)-1]=0;
	memset(&hints, 0, sizeof(hints));
	assert(ip);

	if(ip[0] == '/') {
		/* This looks like a local socket */
		fd = create_local_accept_sock(ip, &noproto);
		/*
		 * Change socket ownership and permissions so users other
		 * than root can access it provided they are in the same
		 * group as the user we run as.
		 */
		if(fd != -1) {
#ifdef HAVE_CHOWN
			if(chmod(ip, (mode_t)(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1) {
				VERBOSITY(3, (LOG_INFO, "cannot chmod metrics socket %s: %s", ip, strerror(errno)));
			}
			if (cfg->username && cfg->username[0] &&
				nsd.uid != (uid_t)-1) {
				if(chown(ip, nsd.uid, nsd.gid) == -1)
					VERBOSITY(2, (LOG_INFO, "cannot chown %u.%u %s: %s",
					  (unsigned)nsd.uid, (unsigned)nsd.gid,
					  ip, strerror(errno)));
			}
#else
			(void)cfg;
#endif
		}
	} else {
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
		/* if we had no interface ip name, "default" is what we
		 * would do getaddrinfo for. */
		if((r = getaddrinfo(ip, port, &hints, &res)) != 0 || !res) {
			log_msg(LOG_ERR, "metrics interface %s:%s getaddrinfo: %s %s",
				ip, port, gai_strerror(r),
#ifdef EAI_SYSTEM
				r==EAI_SYSTEM?(char*)strerror(errno):""
#else
				""
#endif
				);
			return 0;
		}

		/* open fd */
		fd = create_tcp_accept_sock(res, &noproto);
		freeaddrinfo(res);
	}

	if(fd == -1 && noproto) {
		if(!noproto_is_err)
			return 1; /* return success, but do nothing */
		log_msg(LOG_ERR, "cannot open metrics interface %s %d : "
			"protocol not supported", ip, nr);
		return 0;
	}
	if(fd == -1) {
		log_msg(LOG_ERR, "cannot open metrics interface %s %d", ip, nr);
		return 0;
	}

	/* alloc */
	hl = (struct metrics_acceptlist*)xalloc_zero(sizeof(*hl));
	hl->metrics = metrics;
	hl->ident = strdup(ip);
	if(!hl->ident) {
		log_msg(LOG_ERR, "malloc failure");
		close(fd);
		free(hl);
		return 0;
	}
	hl->next = metrics->accept_list;
	metrics->accept_list = hl;

	hl->accept_fd = fd;
	return 1;
}

int
daemon_metrics_open_ports(struct daemon_metrics* metrics, struct nsd_options* cfg)
{
	assert(cfg->metrics_enable && cfg->metrics_port);
	if(cfg->metrics_interface) {
		ip_address_option_type* p;
		for(p = cfg->metrics_interface; p; p = p->next) {
			if(!metrics_add_open(metrics, cfg, p->address, cfg->metrics_port, 1)) {
				return 0;
			}
		}
	} else {
		/* defaults */
		if(cfg->do_ip6 && !metrics_add_open(metrics, cfg, "::1", cfg->metrics_port, 0)) {
			return 0;
		}
		if(cfg->do_ip4 &&
			!metrics_add_open(metrics, cfg, "127.0.0.1", cfg->metrics_port, 1)) {
			return 0;
		}
	}
	return 1;
}

void
daemon_metrics_attach(struct daemon_metrics* metrics, struct xfrd_state* xfrd)
{
	int fd;
	struct metrics_acceptlist* p;
	if(!metrics) return;
	metrics->xfrd = xfrd;

	metrics->http_server = evhttp_new(xfrd->event_base);
	if(!metrics->http_server) {
		log_msg(LOG_ERR, "metrics: out of memory in evhttp_new");
		return;
	}
	for(p = metrics->accept_list; p; p = p->next) {
		fd = p->accept_fd;
		if (evhttp_accept_socket(metrics->http_server, fd)) {
			log_msg(LOG_ERR, "metrics: cannot set http server to accept socket");
		}

		/* only handle requests to metrics_path, anything else returns 404 */
		evhttp_set_cb(metrics->http_server,
                      metrics->xfrd->nsd->options->metrics_path,
                      metrics_http_callback, p);
		/* evhttp_set_gencb(metrics->http_server, metrics_http_callback_generic, p); */
	}
}

/* Callback for handling the active http request to the specific URI */
static void
metrics_http_callback(struct evhttp_request *req, void *p)
{
	struct evbuffer *reply = NULL;
	struct daemon_metrics *metrics = ((struct metrics_acceptlist *)p)->metrics;

	/* currently only GET requests are supported/allowed */
	enum evhttp_cmd_type cmd = evhttp_request_get_command(req);
	if (cmd != EVHTTP_REQ_GET /* && cmd != EVHTTP_REQ_HEAD */) {
		evhttp_send_error(req, HTTP_BADMETHOD, 0);
		return;
	}

	reply = evbuffer_new();

	if (!reply) {
		evhttp_send_error(req, HTTP_INTERNAL, 0);
		log_msg(LOG_ERR, "failed to allocate reply buffer\n");
		return;
	}

	evhttp_add_header(evhttp_request_get_output_headers(req),
	                  "Content-Type", "text/plain; version=0.0.4");
#ifdef BIND8_STATS
	process_stats(NULL, reply, metrics->xfrd, 1);
	evhttp_send_reply(req, HTTP_OK, NULL, reply);
	VERBOSITY(3, (LOG_INFO, "metrics operation completed, response sent"));
#else
	evhttp_send_reply(req, HTTP_NOCONTENT, "No Content - Statistics disabled", reply);
	log_msg(LOG_NOTICE, "metrics requested, but no stats enabled at compile time\n");
	(void)metrics;
#endif /* BIND8_STATS */

	evbuffer_free(reply);
}

#ifdef BIND8_STATS
#define METRIC_MAX_LABELS 2

struct metrics_metric {
	const char *prefix;
	const char *name;
	const char *type;
	size_t label_count;
	const char *label_names[METRIC_MAX_LABELS];
	const char *label_values[METRIC_MAX_LABELS];
};

static void
metric_init_with_prefix(struct metrics_metric *metric, const char *prefix) {
	metric->prefix = prefix;
	metric->name = "";
	metric->label_count = 0;
}

static void
metric_set_name_and_type(struct metrics_metric *metric, const char *name, const char *type) {
	metric->name = name;
	metric->type = type;
}

/* Add a `name="value"` label to the metric.
 * The value must not contain `\`, `\n`, or `"`. */
static void
metric_push_label(struct metrics_metric *metric, const char *name, const char *value) {
	assert(metric->label_count < METRIC_MAX_LABELS);
	metric->label_names[metric->label_count] = name;
	metric->label_values[metric->label_count] = value;
	metric->label_count++;
}

static void
metric_pop_label(struct metrics_metric *metric) {
	assert(metric->label_count > 0);
	metric->label_count--;
}

static void
metric_print(struct metrics_metric *metric, struct evbuffer *buf, uint64_t value) {
	evbuffer_add_printf(buf, "%s%s", metric->prefix, metric->name);
	for (size_t i = 0; i < metric->label_count; i++) {
		evbuffer_add_printf(buf, "%c%s=\"%s\"",
			i == 0 ? '{' : ',',
			metric->label_names[i],
			metric->label_values[i]);
	}
	if (metric->label_count > 0) {
		evbuffer_add_printf(buf, "} %" PRIu64 "\n", value);
	} else {
		evbuffer_add_printf(buf, " %" PRIu64 "\n", value);
	}
}

/** Print a metric value of `integral + decimals_micro * 1e-6`. */
static void
metric_print_micros(struct metrics_metric *metric, struct evbuffer *buf,
	unsigned long integral, unsigned long decimals_micro)
{
	assert(metric->label_count == 0
		&& "metric_print_micros is not implemented for metrics with labels");
	evbuffer_add_printf(buf, "%s%s %lu.%6.6lu\n",
		metric->prefix, metric->name, integral, decimals_micro);
}

static void
metric_print_pop(struct metrics_metric *metric, struct evbuffer *buf, uint64_t value) {
	metric_print(metric, buf, value);
	metric_pop_label(metric);
}

static void
metric_print_help(struct metrics_metric *metric, struct evbuffer *buf, const char *help) {
	evbuffer_add_printf(buf, "# HELP %s%s %s\n# TYPE %s%s %s\n",
		metric->prefix, metric->name, help,
		metric->prefix, metric->name, metric->type);
}

static void
print_stat_block(struct evbuffer *buf, struct nsdst* st, struct metrics_metric *metric) {
	size_t i;

	const char* rcstr[] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN",
		"NOTIMP", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH",
		"NOTZONE", "RCODE11", "RCODE12", "RCODE13", "RCODE14", "RCODE15",
		"BADVERS"
	};

	/* nsd_queries_by_type_total */
	metric_set_name_and_type(metric, "queries_by_type_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received by type.");
	for(i=0; i<= 255; i++) {
		if(metrics_inhibit_zero && st->qtype[i] == 0 &&
			strncmp(rrtype_to_string(i), "TYPE", 4) == 0)
			continue;
		metric_push_label(metric, "type", rrtype_to_string(i));
		metric_print_pop(metric, buf, (uint64_t)st->qtype[i]);
	}

	/* nsd_queries_by_class_total */
	metric_set_name_and_type(metric, "queries_by_class_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received by class.");
	for(i=0; i<4; i++) {
		if(metrics_inhibit_zero && st->qclass[i] == 0 && i != CLASS_IN)
			continue;
		metric_push_label(metric, "class", rrclass_to_string(i));
		metric_print_pop(metric, buf, (uint64_t)st->qclass[i]);
	}

	/* nsd_queries_by_opcode_total */
	metric_set_name_and_type(metric, "queries_by_opcode_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received by opcode.");
	for(i=0; i<6; i++) {
		if(metrics_inhibit_zero && st->opcode[i] == 0 && i != OPCODE_QUERY)
			continue;
		metric_push_label(metric, "opcode", opcode2str(i));
		metric_print_pop(metric, buf, (uint64_t)st->opcode[i]);
	}

	/* nsd_queries_by_rcode_total */
	metric_set_name_and_type(metric, "queries_by_rcode_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received by rcode.");
	for(i=0; i<17; i++) {
		if(metrics_inhibit_zero && st->rcode[i] == 0 &&
			i > RCODE_YXDOMAIN) /*NSD does not use larger*/
			continue;
		metric_push_label(metric, "rcode", rcstr[i]);
		metric_print_pop(metric, buf, (uint64_t)st->rcode[i]);
	}

	/* nsd_queries_by_transport_total */
	metric_set_name_and_type(metric, "queries_by_transport_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received by transport.");
	metric_push_label(metric, "transport", "udp");
	metric_print_pop(metric, buf, (uint64_t)st->qudp);
	metric_push_label(metric, "transport", "udp6");
	metric_print_pop(metric, buf, (uint64_t)st->qudp6);

	/* nsd_queries_with_edns_total */
	metric_set_name_and_type(metric, "queries_with_edns_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received with EDNS OPT.");
	metric_print(metric, buf, (uint64_t)st->edns);

	/* nsd_queries_with_edns_failed_total */
	metric_set_name_and_type(metric, "queries_with_edns_failed_total", "counter");
	metric_print_help(metric, buf, "Total number of queries received with EDNS OPT where EDNS parsing failed.");
	metric_print(metric, buf, (uint64_t)st->ednserr);

	/* nsd_connections_total */
	metric_set_name_and_type(metric, "connections_total", "counter");
	metric_print_help(metric, buf, "Total number of connections.");
	metric_push_label(metric, "transport", "tcp");
	metric_print_pop(metric, buf, (uint64_t)st->ctcp);
	metric_push_label(metric, "transport", "tcp6");
	metric_print_pop(metric, buf, (uint64_t)st->ctcp6);
	metric_push_label(metric, "transport", "tls");
	metric_print_pop(metric, buf, (uint64_t)st->ctls);
	metric_push_label(metric, "transport", "tls6");
	metric_print_pop(metric, buf, (uint64_t)st->ctls6);

	/* nsd_xfr_requests_served_total */
	metric_set_name_and_type(metric, "xfr_requests_served_total", "counter");
	metric_print_help(metric, buf, "Total number of answered zone transfers.");
	metric_push_label(metric, "xfrtype", "AXFR");
	metric_print_pop(metric, buf, (uint64_t)st->raxfr);
	metric_push_label(metric, "xfrtype", "IXFR");
	metric_print_pop(metric, buf, (uint64_t)st->rixfr);

	/* nsd_queries_dropped_total */
	metric_set_name_and_type(metric, "queries_dropped_total", "counter");
	metric_print_help(metric, buf, "Total number of dropped queries.");
	metric_print(metric, buf, (uint64_t)st->dropped);

	/* nsd_queries_rx_failed_total */
	metric_set_name_and_type(metric, "queries_rx_failed_total", "counter");
	metric_print_help(metric, buf, "Total number of queries where receive failed.");
	metric_print(metric, buf, (uint64_t)st->rxerr);

	/* nsd_answers_tx_failed_total */
	metric_set_name_and_type(metric, "answers_tx_failed_total", "counter");
	metric_print_help(metric, buf, "Total number of answers where transmit failed.");
	metric_print(metric, buf, (uint64_t)st->txerr);

	/* nsd_answers_without_aa_total */
	metric_set_name_and_type(metric, "answers_without_aa_total", "counter");
	metric_print_help(metric, buf, "Total number of NOERROR answers without AA flag set.");
	metric_print(metric, buf, (uint64_t)st->nona);

	/* nsd_answers_truncated_total */
	metric_set_name_and_type(metric, "answers_truncated_total", "counter");
	metric_print_help(metric, buf, "Total number of truncated answers.");
	metric_print(metric, buf, (uint64_t)st->truncated);
}

#ifdef USE_ZONE_STATS

void
metrics_zonestat_print_one(struct evbuffer *buf, char *name,
                           struct nsdst *zst)
{
	char* name_valid;
	struct metrics_metric metric;
	metric_init_with_prefix(&metric, "nsd_zonestats_");

	/* The zonestat name can contain characters like '"' that would break the
	 * label value; replace them. */
	name_valid = strdup(name);
	if (!name_valid) {
		log_msg(LOG_ERR, "malloc failure");
		return;
	}
	metrics_make_label_value_valid(name_valid);
	metric_push_label(&metric, "zone", name_valid);

	metric_set_name_and_type(&metric, "queries_total", "counter");
	metric_print_help(&metric, buf, "Total number of queries received.");
	metric_print(&metric, buf,
		(uint64_t)(zst->qudp + zst->qudp6 + zst->ctcp +
			zst->ctcp6 + zst->ctls + zst->ctls6));
	print_stat_block(buf, zst, &metric);
	free(name_valid);
}
#endif /*USE_ZONE_STATS*/

void
metrics_print_stats(struct evbuffer *buf, xfrd_state_type *xfrd,
                    struct timeval *now, int clear, struct nsdst *st,
                    struct nsdst **zonestats, struct timeval *rc_stats_time)
{
	size_t i;
	struct timeval elapsed, uptime;
	struct metrics_metric metric;
	char server_str[16] = {0};

	metric_init_with_prefix(&metric, "nsd_");

	/* nsd_queries_total */
	metric_set_name_and_type(&metric, "queries_total", "counter");
	metric_print_help(&metric, buf, "Total number of queries received.");
	/*per CPU and total*/
	for(i=0; i<xfrd->nsd->child_count; i++) {
		sprintf(server_str, "%d", (int)i);
		metric_push_label(&metric, "server", server_str);
		metric_print_pop(&metric, buf, (uint64_t)xfrd->nsd->children[i].query_count);
	}

	print_stat_block(buf, st, &metric);

	/* uptime (in seconds) */
	timeval_subtract(&uptime, now, &xfrd->nsd->metrics->boot_time);
	metric_set_name_and_type(&metric, "time_up_seconds_total", "counter");
	metric_print_help(&metric, buf, "Uptime since server boot in seconds.");
	metric_print_micros(&metric, buf,
		(unsigned long)uptime.tv_sec, (unsigned long)uptime.tv_usec);

	/* time elapsed since last nsd-control stats reset (in seconds) */
	/* if remote-control is disabled aka rc_stats_time == NULL
	 * use metrics' stats_time */
	if (rc_stats_time) {
		timeval_subtract(&elapsed, now, rc_stats_time);
	} else {
		timeval_subtract(&elapsed, now, &xfrd->nsd->metrics->stats_time);
	}
	metric_set_name_and_type(&metric, "time_elapsed_seconds", "untyped");
	metric_print_help(&metric, buf,
		"Time since last statistics printout and "
	        "reset (by nsd-control stats) in seconds.");
	metric_print_micros(&metric, buf,
		(unsigned long)elapsed.tv_sec, (unsigned long)elapsed.tv_usec);

	/*mem info, database on disksize*/
	metric_set_name_and_type(&metric, "size_db_on_disk_bytes", "gauge");
	metric_print_help(&metric, buf, "Size of DNS database on disk.");
	metric_print(&metric, buf, st->db_disk);

	metric_set_name_and_type(&metric, "size_db_in_mem_bytes", "gauge");
	metric_print_help(&metric, buf, "Size of DNS database in memory.");
	metric_print(&metric, buf, st->db_mem);

	metric_set_name_and_type(&metric, "size_xfrd_in_mem_bytes", "gauge");
	metric_print_help(&metric, buf, "Size of zone transfers and notifies in xfrd process, excluding TSIG data.");
	metric_print(&metric, buf, region_get_mem(xfrd->region));

	metric_set_name_and_type(&metric, "size_config_on_disk_bytes", "gauge");
	metric_print_help(&metric, buf, "Size of zonelist file on disk, excluding nsd.conf.");
	metric_print(&metric, buf, xfrd->nsd->options->zonelist_off);

	metric_set_name_and_type(&metric, "size_config_in_mem_bytes", "gauge");
	metric_print_help(&metric, buf, "Size of config data in memory.");
	metric_print(&metric, buf, region_get_mem(xfrd->nsd->options->region));

	/* number of zones serverd */
	metric_set_name_and_type(&metric, "zones_primary", "gauge");
	metric_print_help(&metric, buf, "Number of primary zones served.");
	metric_print(&metric, buf,
		(uint64_t)(xfrd->notify_zones->count - xfrd->zones->count));

	metric_set_name_and_type(&metric, "zones_secondary", "gauge");
	metric_print_help(&metric, buf, "Number of secondary zones served.");
	metric_print(&metric, buf, (uint64_t)xfrd->zones->count);

#ifdef USE_ZONE_STATS
	zonestat_print(NULL, buf, xfrd, clear, zonestats); /*per-zone statistics*/
#else
	(void)clear; (void)zonestats;
#endif
}

#endif /*BIND8_STATS*/

#endif /* USE_METRICS */
