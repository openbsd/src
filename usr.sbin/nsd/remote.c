/*
 * remote.c - remote control for the NSD daemon.
 *
 * Copyright (c) 2008, NLnet Labs. All rights reserved.
 *
 * This software is open source.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * Neither the name of the NLNET LABS nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * \file
 *
 * This file contains the remote control functionality for the daemon.
 * The remote control can be performed using either the commandline
 * nsd-control tool, or a TLS capable web browser. 
 * The channel is secured using TLSv1, and certificates.
 * Both the server and the client(control tool) have their own keys.
 */
#include "config.h"
#ifdef HAVE_SSL

#ifdef HAVE_OPENSSL_SSL_H
#include "openssl/ssl.h"
#endif
#ifdef HAVE_OPENSSL_ERR_H
#include <openssl/err.h>
#endif
#ifdef HAVE_OPENSSL_RAND_H
#include <openssl/rand.h>
#endif
#include <ctype.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#ifndef USE_MINI_EVENT
#  ifdef HAVE_EVENT_H
#    include <event.h>
#  else
#    include <event2/event.h>
#    include "event2/event_struct.h"
#    include "event2/event_compat.h"
#  endif
#else
#  include "mini_event.h"
#endif
#include "remote.h"
#include "util.h"
#include "xfrd.h"
#include "xfrd-notify.h"
#include "xfrd-tcp.h"
#include "nsd.h"
#include "options.h"
#include "difffile.h"
#include "ipc.h"

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

/** number of seconds timeout on incoming remote control handshake */
#define REMOTE_CONTROL_TCP_TIMEOUT 120

/** repattern to master or slave */
#define REPAT_SLAVE  1
#define REPAT_MASTER 2

/** if you want zero to be inhibited in stats output.
 * it omits zeroes for types that have no acronym and unused-rcodes */
const int inhibit_zero = 1;

/**
 * a busy control command connection, SSL state
 * Defined here to keep the definition private, and keep SSL out of the .h
 */
struct rc_state {
	/** the next item in list */
	struct rc_state* next, *prev;
	/* if the event was added to the event_base */
	int event_added;
	/** the commpoint */
	struct event c;
	/** timeout for this state */
	struct timeval tval;
	/** in the handshake part */
	enum { rc_none, rc_hs_read, rc_hs_write } shake_state;
	/** the ssl state */
	SSL* ssl;
	/** the rc this is part of */
	struct daemon_remote* rc;
	/** stats list next item */
	struct rc_state* stats_next;
	/** stats list indicator (0 is not part of stats list, 1 is stats,
	 * 2 is stats_noreset. */
	int in_stats_list;
};

/**
 * list of events for accepting connections
 */
struct acceptlist {
	struct acceptlist* next;
	int event_added;
	struct event c;
};

/**
 * The remote control state.
 */
struct daemon_remote {
	/** the master process for this remote control */
	struct xfrd_state* xfrd;
	/** commpoints for accepting remote control connections */
	struct acceptlist* accept_list;
	/** number of active commpoints that are handling remote control */
	int active;
	/** max active commpoints */
	int max_active;
	/** current commpoints busy; double linked, malloced */
	struct rc_state* busy_list;
	/** commpoints waiting for stats to complete (also in busy_list) */
	struct rc_state* stats_list;
	/** last time stats was reported */
	struct timeval stats_time, boot_time;
	/** the SSL context for creating new SSL streams */
	SSL_CTX* ctx;
};

/** 
 * Print fixed line of text over ssl connection in blocking mode
 * @param ssl: print to
 * @param text: the text.
 * @return false on connection failure.
 */
static int ssl_print_text(SSL* ssl, const char* text);

/** 
 * printf style printing to the ssl connection
 * @param ssl: the SSL connection to print to. Blocking.
 * @param format: printf style format string.
 * @return success or false on a network failure.
 */
static int ssl_printf(SSL* ssl, const char* format, ...)
        ATTR_FORMAT(printf, 2, 3);

/**
 * Read until \n is encountered
 * If SSL signals EOF, the string up to then is returned (without \n).
 * @param ssl: the SSL connection to read from. blocking.
 * @param buf: buffer to read to.
 * @param max: size of buffer.
 * @return false on connection failure.
 */
static int ssl_read_line(SSL* ssl, char* buf, size_t max);

/** perform the accept of a new remote control connection */
static void
remote_accept_callback(int fd, short event, void* arg);

/** perform remote control */
static void
remote_control_callback(int fd, short event, void* arg);


/** ---- end of private defines ---- **/


/** log ssl crypto err */
static void
log_crypto_err(const char* str)
{
	/* error:[error code]:[library name]:[function name]:[reason string] */
	char buf[128];
	unsigned long e;
	ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
	log_msg(LOG_ERR, "%s crypto %s", str, buf);
	while( (e=ERR_get_error()) ) {
		ERR_error_string_n(e, buf, sizeof(buf));
		log_msg(LOG_ERR, "and additionally crypto %s", buf);
	}
}

#ifdef BIND8_STATS
/** subtract timers and the values do not overflow or become negative */
static void
timeval_subtract(struct timeval* d, const struct timeval* end, 
	const struct timeval* start)
{
#ifndef S_SPLINT_S
	time_t end_usec = end->tv_usec;
	d->tv_sec = end->tv_sec - start->tv_sec;
	if(end_usec < start->tv_usec) {
		end_usec += 1000000;
		d->tv_sec--;
	}
	d->tv_usec = end_usec - start->tv_usec;
#endif
}
#endif /* BIND8_STATS */

struct daemon_remote*
daemon_remote_create(nsd_options_t* cfg)
{
	char* s_cert;
	char* s_key;
	struct daemon_remote* rc = (struct daemon_remote*)xalloc_zero(
		sizeof(*rc));
	rc->max_active = 10;
	assert(cfg->control_enable);

	/* init SSL library */
#ifdef HAVE_ERR_LOAD_CRYPTO_STRINGS
	ERR_load_crypto_strings();
#endif
	ERR_load_SSL_strings();
#if OPENSSL_VERSION_NUMBER < 0x10100000 || !defined(HAVE_OPENSSL_INIT_CRYPTO)
	OpenSSL_add_all_algorithms();
#else
	OPENSSL_init_crypto(OPENSSL_INIT_ADD_ALL_CIPHERS
		| OPENSSL_INIT_ADD_ALL_DIGESTS
		| OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
#endif
#if OPENSSL_VERSION_NUMBER < 0x10100000 || !defined(HAVE_OPENSSL_INIT_SSL)
	(void)SSL_library_init();
#else
	OPENSSL_init_ssl(0, NULL);
#endif

	if(!RAND_status()) {
		/* try to seed it */
		unsigned char buf[256];
		unsigned int v, seed=(unsigned)time(NULL) ^ (unsigned)getpid();
		size_t i;
		v = seed;
		for(i=0; i<256/sizeof(v); i++) {
			memmove(buf+i*sizeof(v), &v, sizeof(v));
			v = v*seed + (unsigned int)i;
		}
		RAND_seed(buf, 256);
		log_msg(LOG_WARNING, "warning: no entropy, seeding openssl PRNG with time");
	}

	rc->ctx = SSL_CTX_new(SSLv23_server_method());
	if(!rc->ctx) {
		log_crypto_err("could not SSL_CTX_new");
		free(rc);
		return NULL;
	}
	/* no SSLv2, SSLv3 because has defects */
	if((SSL_CTX_set_options(rc->ctx, SSL_OP_NO_SSLv2) & SSL_OP_NO_SSLv2)
		!= SSL_OP_NO_SSLv2){
		log_crypto_err("could not set SSL_OP_NO_SSLv2");
		daemon_remote_delete(rc);
		return NULL;
	}
	if((SSL_CTX_set_options(rc->ctx, SSL_OP_NO_SSLv3) & SSL_OP_NO_SSLv3)
		!= SSL_OP_NO_SSLv3){
		log_crypto_err("could not set SSL_OP_NO_SSLv3");
		daemon_remote_delete(rc);
		return NULL;
	}
	s_cert = cfg->server_cert_file;
	s_key = cfg->server_key_file;
	VERBOSITY(2, (LOG_INFO, "setup SSL certificates"));
	if (!SSL_CTX_use_certificate_file(rc->ctx,s_cert,SSL_FILETYPE_PEM)) {
		log_msg(LOG_ERR, "Error for server-cert-file: %s", s_cert);
		log_crypto_err("Error in SSL_CTX use_certificate_file");
		goto setup_error;
	}
	if(!SSL_CTX_use_PrivateKey_file(rc->ctx,s_key,SSL_FILETYPE_PEM)) {
		log_msg(LOG_ERR, "Error for server-key-file: %s", s_key);
		log_crypto_err("Error in SSL_CTX use_PrivateKey_file");
		goto setup_error;
	}
	if(!SSL_CTX_check_private_key(rc->ctx)) {
		log_msg(LOG_ERR, "Error for server-key-file: %s", s_key);
		log_crypto_err("Error in SSL_CTX check_private_key");
		goto setup_error;
	}
	if(!SSL_CTX_load_verify_locations(rc->ctx, s_cert, NULL)) {
		log_crypto_err("Error setting up SSL_CTX verify locations");
	setup_error:
		daemon_remote_delete(rc);
		return NULL;
	}
	SSL_CTX_set_client_CA_list(rc->ctx, SSL_load_client_CA_file(s_cert));
	SSL_CTX_set_verify(rc->ctx, SSL_VERIFY_PEER, NULL);

	/* and try to open the ports */
	if(!daemon_remote_open_ports(rc, cfg)) {
		log_msg(LOG_ERR, "could not open remote control port");
		goto setup_error;
	}

	if(gettimeofday(&rc->boot_time, NULL) == -1)
		log_msg(LOG_ERR, "gettimeofday: %s", strerror(errno));
	rc->stats_time = rc->boot_time;

	return rc;
}

void daemon_remote_close(struct daemon_remote* rc)
{
	struct rc_state* p, *np;
	struct acceptlist* h, *nh;
	if(!rc) return;

	/* close listen sockets */
	h = rc->accept_list;
	while(h) {
		nh = h->next;
		if(h->event_added)
			event_del(&h->c);
		close(h->c.ev_fd);
		free(h);
		h = nh;
	}
	rc->accept_list = NULL;

	/* close busy connection sockets */
	p = rc->busy_list;
	while(p) {
		np = p->next;
		if(p->event_added)
			event_del(&p->c);
		if(p->ssl)
			SSL_free(p->ssl);
		close(p->c.ev_fd);
		free(p);
		p = np;
	}
	rc->busy_list = NULL;
	rc->active = 0;
}

void daemon_remote_delete(struct daemon_remote* rc)
{
	if(!rc) return;
	daemon_remote_close(rc);
	if(rc->ctx) {
		SSL_CTX_free(rc->ctx);
	}
	free(rc);
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
		return -1;
	}
	/* Listen to it... */
	if (listen(s, TCP_BACKLOG_REMOTE) == -1) {
		log_msg(LOG_ERR, "can't listen: %s", strerror(errno));
		return -1;
	}
	return s;
}

/**
 * Add and open a new control port
 * @param rc: rc with result list.
 * @param ip: ip str
 * @param nr: port nr
 * @param noproto_is_err: if lack of protocol support is an error.
 * @return false on failure.
 */
static int
add_open(struct daemon_remote* rc, const char* ip, int nr, int noproto_is_err)
{
	struct addrinfo hints;
	struct addrinfo* res;
	struct acceptlist* hl;
	int noproto;
	int fd, r;
	char port[15];
	snprintf(port, sizeof(port), "%d", nr);
	port[sizeof(port)-1]=0;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;
	if((r = getaddrinfo(ip, port, &hints, &res)) != 0 || !res) {
                log_msg(LOG_ERR, "control interface %s:%s getaddrinfo: %s %s",
			ip?ip:"default", port, gai_strerror(r),
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
	if(fd == -1 && noproto) {
		if(!noproto_is_err)
			return 1; /* return success, but do nothing */
		log_msg(LOG_ERR, "cannot open control interface %s %d : "
			"protocol not supported", ip, nr);
		return 0;
	}
	if(fd == -1) {
		log_msg(LOG_ERR, "cannot open control interface %s %d", ip, nr);
		return 0;
	}

	/* alloc */
	hl = (struct acceptlist*)xalloc_zero(sizeof(*hl));
	hl->next = rc->accept_list;
	rc->accept_list = hl;

	hl->c.ev_fd = fd;
	hl->event_added = 0;
	return 1;
}

int
daemon_remote_open_ports(struct daemon_remote* rc, nsd_options_t* cfg)
{
	assert(cfg->control_enable && cfg->control_port);
	if(cfg->control_interface) {
		ip_address_option_t* p;
		for(p = cfg->control_interface; p; p = p->next) {
			if(!add_open(rc, p->address, cfg->control_port, 1)) {
				return 0;
			}
		}
	} else {
		/* defaults */
		if(cfg->do_ip6 && !add_open(rc, "::1", cfg->control_port, 0)) {
			return 0;
		}
		if(cfg->do_ip4 &&
			!add_open(rc, "127.0.0.1", cfg->control_port, 1)) {
			return 0;
		}
	}
	return 1;
}

void
daemon_remote_attach(struct daemon_remote* rc, struct xfrd_state* xfrd)
{
	int fd;
	struct acceptlist* p;
	if(!rc) return;
	rc->xfrd = xfrd;
	for(p = rc->accept_list; p; p = p->next) {
		/* add event */
		fd = p->c.ev_fd;
		event_set(&p->c, fd, EV_PERSIST|EV_READ, remote_accept_callback,
			rc);
		if(event_base_set(xfrd->event_base, &p->c) != 0)
			log_msg(LOG_ERR, "remote: cannot set event_base");
		if(event_add(&p->c, NULL) != 0)
			log_msg(LOG_ERR, "remote: cannot add event");
		p->event_added = 1;
	}
}

static void
remote_accept_callback(int fd, short event, void* arg)
{
	struct daemon_remote *rc = (struct daemon_remote*)arg;
#ifdef INET6
	struct sockaddr_storage addr;
#else
	struct sockaddr_in addr;
#endif
	socklen_t addrlen;
	int newfd;
	struct rc_state* n;

	if (!(event & EV_READ)) {
		return;
	}

	/* perform the accept */
	addrlen = sizeof(addr);
	newfd = accept(fd, (struct sockaddr*)&addr, &addrlen);
	if(newfd == -1) {
		if (    errno != EINTR
			&& errno != EWOULDBLOCK
#ifdef ECONNABORTED
			&& errno != ECONNABORTED
#endif /* ECONNABORTED */
#ifdef EPROTO
			&& errno != EPROTO
#endif /* EPROTO */
			) {
			log_msg(LOG_ERR, "accept failed: %s", strerror(errno));
		}
		return;
	}

	/* create new commpoint unless we are servicing already */
	if(rc->active >= rc->max_active) {
		log_msg(LOG_WARNING, "drop incoming remote control: "
			"too many connections");
	close_exit:
		close(newfd);
		return;
	}
	if (fcntl(newfd, F_SETFL, O_NONBLOCK) == -1) {
		log_msg(LOG_ERR, "fcntl failed: %s", strerror(errno));
		goto close_exit;
	}

	/* setup state to service the remote control command */
	n = (struct rc_state*)calloc(1, sizeof(*n));
	if(!n) {
		log_msg(LOG_ERR, "out of memory");
		goto close_exit;
	}

	n->tval.tv_sec = REMOTE_CONTROL_TCP_TIMEOUT; 
	n->tval.tv_usec = 0L;

	event_set(&n->c, newfd, EV_PERSIST|EV_TIMEOUT|EV_READ,
		remote_control_callback, n);
	if(event_base_set(xfrd->event_base, &n->c) != 0) {
		log_msg(LOG_ERR, "remote_accept: cannot set event_base");
		free(n);
		goto close_exit;
	}
	if(event_add(&n->c, &n->tval) != 0) {
		log_msg(LOG_ERR, "remote_accept: cannot add event");
		free(n);
		goto close_exit;
	}
	n->event_added = 1;

	if(2 <= verbosity) {
		char s[128];
		addr2str(&addr, s, sizeof(s));
		VERBOSITY(2, (LOG_INFO, "new control connection from %s", s));
	}

	n->shake_state = rc_hs_read;
	n->ssl = SSL_new(rc->ctx);
	if(!n->ssl) {
		log_crypto_err("could not SSL_new");
		event_del(&n->c);
		free(n);
		goto close_exit;
	}
	SSL_set_accept_state(n->ssl);
        (void)SSL_set_mode(n->ssl, SSL_MODE_AUTO_RETRY);
	if(!SSL_set_fd(n->ssl, newfd)) {
		log_crypto_err("could not SSL_set_fd");
		event_del(&n->c);
		SSL_free(n->ssl);
		free(n);
		goto close_exit;
	}

	n->rc = rc;
	n->stats_next = NULL;
	n->in_stats_list = 0;
	n->prev = NULL;
	n->next = rc->busy_list;
	if(n->next) n->next->prev = n;
	rc->busy_list = n;
	rc->active ++;

	/* perform the first nonblocking read already, for windows, 
	 * so it can return wouldblock. could be faster too. */
	remote_control_callback(newfd, EV_READ, n);
}

/** delete from list */
static void
state_list_remove_elem(struct rc_state** list, struct rc_state* todel)
{
	if(todel->prev) todel->prev->next = todel->next;
	else	*list = todel->next;
	if(todel->next) todel->next->prev = todel->prev;
}

/** delete from stats list */
static void
stats_list_remove_elem(struct rc_state** list, struct rc_state* todel)
{
	while(*list) {
		if( (*list) == todel) {
			*list = (*list)->stats_next;
			return;
		}
		list = &(*list)->stats_next;
	}
}

/** decrease active count and remove commpoint from busy list */
static void
clean_point(struct daemon_remote* rc, struct rc_state* s)
{
	if(s->in_stats_list)
		stats_list_remove_elem(&rc->stats_list, s);
	state_list_remove_elem(&rc->busy_list, s);
	rc->active --;
	if(s->event_added)
		event_del(&s->c);
	if(s->ssl) {
		SSL_shutdown(s->ssl);
		SSL_free(s->ssl);
	}
	close(s->c.ev_fd);
	free(s);
}

static int
ssl_print_text(SSL* ssl, const char* text)
{
	int r;
	if(!ssl) 
		return 0;
	ERR_clear_error();
	if((r=SSL_write(ssl, text, (int)strlen(text))) <= 0) {
		if(SSL_get_error(ssl, r) == SSL_ERROR_ZERO_RETURN) {
			VERBOSITY(2, (LOG_WARNING, "in SSL_write, peer "
				"closed connection"));
			return 0;
		}
		log_crypto_err("could not SSL_write");
		return 0;
	}
	return 1;
}

/** print text over the ssl connection */
static int
ssl_print_vmsg(SSL* ssl, const char* format, va_list args)
{
	char msg[1024];
	vsnprintf(msg, sizeof(msg), format, args);
	return ssl_print_text(ssl, msg);
}

/** printf style printing to the ssl connection */
static int
ssl_printf(SSL* ssl, const char* format, ...)
{
	va_list args;
	int ret;
	va_start(args, format);
	ret = ssl_print_vmsg(ssl, format, args);
	va_end(args);
	return ret;
}

static int
ssl_read_line(SSL* ssl, char* buf, size_t max)
{
	int r;
	size_t len = 0;
	if(!ssl)
		return 0;
	while(len < max) {
		ERR_clear_error();
		if((r=SSL_read(ssl, buf+len, 1)) <= 0) {
			if(SSL_get_error(ssl, r) == SSL_ERROR_ZERO_RETURN) {
				buf[len] = 0;
				return 1;
			}
			log_crypto_err("could not SSL_read");
			return 0;
		}
		if(buf[len] == '\n') {
			/* return string without \n */
			buf[len] = 0;
			return 1;
		}
		len++;
	}
	buf[max-1] = 0;
	log_msg(LOG_ERR, "control line too long (%d): %s", (int)max, buf);
	return 0;
}

/** skip whitespace, return new pointer into string */
static char*
skipwhite(char* str)
{
	/* EOS \0 is not a space */
	while( isspace((unsigned char)*str) ) 
		str++;
	return str;
}

/** send the OK to the control client */
static void
send_ok(SSL* ssl)
{
	(void)ssl_printf(ssl, "ok\n");
}

/** get zone argument (if any) or NULL, false on error */
static int
get_zone_arg(SSL* ssl, xfrd_state_t* xfrd, char* arg,
	zone_options_t** zo)
{
	const dname_type* dname;
	if(!arg[0]) {
		/* no argument present, return NULL */
		*zo = NULL;
		return 1;
	}
	dname = dname_parse(xfrd->region, arg);
	if(!dname) {
		ssl_printf(ssl, "error cannot parse zone name '%s'\n", arg);
		*zo = NULL;
		return 0;
	}
	*zo = zone_options_find(xfrd->nsd->options, dname);
	region_recycle(xfrd->region, (void*)dname, dname_total_size(dname));
	if(!*zo) {
		ssl_printf(ssl, "error zone %s not configured\n", arg);
		return 0;
	}
	return 1;
}

/** do the stop command */
static void
do_stop(SSL* ssl, xfrd_state_t* xfrd)
{
	xfrd->need_to_send_shutdown = 1;

	if(!(xfrd->ipc_handler_flags&EV_WRITE)) {
		ipc_xfrd_set_listening(xfrd, EV_PERSIST|EV_READ|EV_WRITE);
	}

	send_ok(ssl);
}

/** do the log_reopen command, it only needs reload_now */
static void
do_log_reopen(SSL* ssl, xfrd_state_t* xfrd)
{
	xfrd_set_reload_now(xfrd);
	send_ok(ssl);
}

/** do the reload command */
static void
do_reload(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	task_new_check_zonefiles(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, zo?(const dname_type*)zo->node.key:NULL);
	xfrd_set_reload_now(xfrd);
	send_ok(ssl);
}

/** do the write command */
static void
do_write(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	task_new_write_zonefiles(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, zo?(const dname_type*)zo->node.key:NULL);
	xfrd_set_reload_now(xfrd);
	send_ok(ssl);
}

/** do the notify command */
static void
do_notify(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	if(zo) {
		struct notify_zone_t* n = (struct notify_zone_t*)rbtree_search(
			xfrd->notify_zones, (const dname_type*)zo->node.key);
		if(n) {
			xfrd_notify_start(n, xfrd);
			send_ok(ssl);
		} else {
			ssl_printf(ssl, "error zone does not have notify\n");
		}
	} else {
		struct notify_zone_t* n;
		RBTREE_FOR(n, struct notify_zone_t*, xfrd->notify_zones) {
			xfrd_notify_start(n, xfrd);
		}
		send_ok(ssl);
	}
}

/** do the transfer command */
static void
do_transfer(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	xfrd_zone_t* zone;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	if(zo) {
		zone = (xfrd_zone_t*)rbtree_search(xfrd->zones, (const
			dname_type*)zo->node.key);
		if(zone) {
			xfrd_handle_notify_and_start_xfr(zone, NULL);
			send_ok(ssl);
		} else {
			ssl_printf(ssl, "error zone not slave\n");
		}
	} else {
		RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones) {
			xfrd_handle_notify_and_start_xfr(zone, NULL);
		}
		ssl_printf(ssl, "ok, %u zones\n", (unsigned)xfrd->zones->count);
	}
}

/** force transfer a zone */
static void
force_transfer_zone(xfrd_zone_t* zone)
{
	/* if in TCP transaction, stop it immediately. */
	if(zone->tcp_conn != -1)
		xfrd_tcp_release(xfrd->tcp_set, zone);
	else if(zone->zone_handler.ev_fd != -1)
		xfrd_udp_release(zone);
	/* pretend we not longer have it and force any
	 * zone to be downloaded (even same serial, w AXFR) */
	zone->soa_disk_acquired = 0;
	zone->soa_nsd_acquired = 0;
	xfrd_handle_notify_and_start_xfr(zone, NULL);
}

/** do the force transfer command */
static void
do_force_transfer(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	xfrd_zone_t* zone;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	if(zo) {
		zone = (xfrd_zone_t*)rbtree_search(xfrd->zones, (const
			dname_type*)zo->node.key);
		if(zone) {
			force_transfer_zone(zone);
			send_ok(ssl);
		} else {
			ssl_printf(ssl, "error zone not slave\n");
		}
	} else {
		RBTREE_FOR(zone, xfrd_zone_t*, xfrd->zones) {
			force_transfer_zone(zone);
		}
		ssl_printf(ssl, "ok, %u zones\n", (unsigned)xfrd->zones->count);
	}
}

static int
print_soa_status(SSL* ssl, const char* str, xfrd_soa_t* soa, time_t acq)
{
	if(acq) {
		if(!ssl_printf(ssl, "	%s: \"%u since %s\"\n", str,
			(unsigned)ntohl(soa->serial), xfrd_pretty_time(acq)))
			return 0;
	} else {
		if(!ssl_printf(ssl, "	%s: none\n", str))
			return 0;
	}
	return 1;
}

/** print zonestatus for one domain */
static int
print_zonestatus(SSL* ssl, xfrd_state_t* xfrd, zone_options_t* zo)
{
	xfrd_zone_t* xz = (xfrd_zone_t*)rbtree_search(xfrd->zones,
		(const dname_type*)zo->node.key);
	struct notify_zone_t* nz = (struct notify_zone_t*)rbtree_search(
		xfrd->notify_zones, (const dname_type*)zo->node.key);
	if(!ssl_printf(ssl, "zone:	%s\n", zo->name))
		return 0;
	if(!zo->part_of_config) {
		if(!ssl_printf(ssl, "	pattern: %s\n", zo->pattern->pname))
			return 0;
	}
	if(nz) {
		if(nz->is_waiting) {
			if(!ssl_printf(ssl, "	notify: \"waiting-for-fd\"\n"))
				return 0;
		} else if(nz->notify_send_enable) {
			if(!ssl_printf(ssl, "	notify: \"sent try %d "
				"to %s with serial %u\"\n", nz->notify_retry,
				nz->notify_current->ip_address_spec,
				(unsigned)ntohl(nz->current_soa->serial)))
				return 0;
		}
	}
	if(!xz) {
		if(!ssl_printf(ssl, "	state: master\n"))
			return 0;
		return 1;
	}
	if(!ssl_printf(ssl, "	state: %s\n",
		(xz->state == xfrd_zone_ok)?"ok":(
		(xz->state == xfrd_zone_expired)?"expired":"refreshing")))
		return 0;
	if(!print_soa_status(ssl, "served-serial", &xz->soa_nsd,
		xz->soa_nsd_acquired))
		return 0;
	if(!print_soa_status(ssl, "commit-serial", &xz->soa_disk,
		xz->soa_disk_acquired))
		return 0;
	if(xz->round_num != -1) {
		if(!print_soa_status(ssl, "notified-serial", &xz->soa_notified,
			xz->soa_notified_acquired))
			return 0;
	}

	/* UDP */
	if(xz->udp_waiting) {
		if(!ssl_printf(ssl, "	transfer: \"waiting-for-UDP-fd\"\n"))
			return 0;
	} else if(xz->zone_handler.ev_fd != -1 && xz->tcp_conn == -1) {
		if(!ssl_printf(ssl, "	transfer: \"sent UDP to %s\"\n",
			xz->master->ip_address_spec))
			return 0;
	}

	/* TCP */
	if(xz->tcp_waiting) {
		if(!ssl_printf(ssl, "	transfer: \"waiting-for-TCP-fd\"\n"))
			return 0;
	} else if(xz->tcp_conn != -1) {
		if(!ssl_printf(ssl, "	transfer: \"TCP connected to %s\"\n",
			xz->master->ip_address_spec))
			return 0;
	}

	return 1;
}

/** do the zonestatus command */
static void
do_zonestatus(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	zone_options_t* zo;
	if(!get_zone_arg(ssl, xfrd, arg, &zo))
		return;
	if(zo) (void)print_zonestatus(ssl, xfrd, zo);
	else {
		RBTREE_FOR(zo, zone_options_t*,
			xfrd->nsd->options->zone_options) {
			if(!print_zonestatus(ssl, xfrd, zo))
				return;
		}
	}
}

/** do the verbosity command */
static void
do_verbosity(SSL* ssl, char* str)
{
	int val = atoi(str);
	if(strcmp(str, "") == 0) {
		ssl_printf(ssl, "verbosity %d\n", verbosity);
		return;
	}
	if(val == 0 && strcmp(str, "0") != 0) {
		ssl_printf(ssl, "error in verbosity number syntax: %s\n", str);
		return;
	}
	verbosity = val;
	task_new_set_verbosity(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, val);
	xfrd_set_reload_now(xfrd);
	send_ok(ssl);
}

/** find second argument, modifies string */
static int
find_arg2(SSL* ssl, char* arg, char** arg2)
{
	char* as = strrchr(arg, ' ');
	if(as) {
		as[0]=0;
		*arg2 = as+1;
		while(isspace((unsigned char)*as) && as > arg)
			as--;
		as[0]=0;
		return 1;
	}
	ssl_printf(ssl, "error could not find next argument "
		"after %s\n", arg);
	return 0;
}

/** do the status command */
static void
do_status(SSL* ssl, xfrd_state_t* xfrd)
{
	if(!ssl_printf(ssl, "version: %s\n", PACKAGE_VERSION))
		return;
	if(!ssl_printf(ssl, "verbosity: %d\n", verbosity))
		return;
#ifdef RATELIMIT
	if(!ssl_printf(ssl, "ratelimit: %d\n",
		(int)xfrd->nsd->options->rrl_ratelimit))
		return;
#else
	(void)xfrd;
#endif
}

/** do the stats command */
static void
do_stats(struct daemon_remote* rc, int peek, struct rc_state* rs)
{
#ifdef BIND8_STATS
	/* queue up to get stats after a reload is done (to gather statistics
	 * from the servers) */
	assert(!rs->in_stats_list);
	if(peek) rs->in_stats_list = 2;
	else	rs->in_stats_list = 1;
	rs->stats_next = rc->stats_list;
	rc->stats_list = rs;
	/* block the tcp waiting for the reload */
	event_del(&rs->c);
	rs->event_added = 0;
	/* force a reload */
	xfrd_set_reload_now(xfrd);
#else
	(void)rc; (void)peek;
	(void)ssl_printf(rs->ssl, "error no stats enabled at compile time\n");
#endif /* BIND8_STATS */
}

/** see if we have more zonestatistics entries and it has to be incremented */
static void
zonestat_inc_ifneeded(xfrd_state_t* xfrd)
{
#ifdef USE_ZONE_STATS
	if(xfrd->nsd->options->zonestatnames->count != xfrd->zonestat_safe)
		task_new_zonestat_inc(xfrd->nsd->task[xfrd->nsd->mytask],
			xfrd->last_task, 
			xfrd->nsd->options->zonestatnames->count);
#else
	(void)xfrd;
#endif /* USE_ZONE_STATS */
}

/** perform the addzone command for one zone */
static int
perform_addzone(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	const dname_type* dname;
	zone_options_t* zopt;
	char* arg2 = NULL;
	if(!find_arg2(ssl, arg, &arg2))
		return 0;

	/* if we add it to the xfrd now, then xfrd could download AXFR and
	 * store it and the NSD-reload would see it in the difffile before
	 * it sees the add-config task.
	 */
	/* thus: AXFRs and IXFRs must store the pattern name in the
	 * difffile, so that it can be added when the AXFR or IXFR is seen.
	 */

	/* check that the pattern exists */
	if(!rbtree_search(xfrd->nsd->options->patterns, arg2)) {
		(void)ssl_printf(ssl, "error pattern %s does not exist\n",
			arg2);
		return 0;
	}

	dname = dname_parse(xfrd->region, arg);
	if(!dname) {
		(void)ssl_printf(ssl, "error cannot parse zone name\n");
		return 0;
	}

	/* see if zone is a duplicate */
	if( (zopt=zone_options_find(xfrd->nsd->options, dname)) ) {
		region_recycle(xfrd->region, (void*)dname,
			dname_total_size(dname));
		(void)ssl_printf(ssl, "zone %s already exists\n", arg);
		return 1;
	}
	region_recycle(xfrd->region, (void*)dname, dname_total_size(dname));
	dname = NULL;

	/* add to zonelist and adds to config in memory */
	zopt = zone_list_add(xfrd->nsd->options, arg, arg2);
	if(!zopt) {
		/* also dname parse error here */
		(void)ssl_printf(ssl, "error could not add zonelist entry\n");
		return 0;
	}
	/* make addzone task and schedule reload */
	task_new_add_zone(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, arg, arg2,
		getzonestatid(xfrd->nsd->options, zopt));
	zonestat_inc_ifneeded(xfrd);
	xfrd_set_reload_now(xfrd);
	/* add to xfrd - notify (for master and slaves) */
	init_notify_send(xfrd->notify_zones, xfrd->region, zopt);
	/* add to xfrd - slave */
	if(zone_is_slave(zopt)) {
		xfrd_init_slave_zone(xfrd, zopt);
	}
	return 1;
}

/** perform the delzone command for one zone */
static int
perform_delzone(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	const dname_type* dname;
	zone_options_t* zopt;

	dname = dname_parse(xfrd->region, arg);
	if(!dname) {
		(void)ssl_printf(ssl, "error cannot parse zone name\n");
		return 0;
	}

	/* see if we have the zone in question */
	zopt = zone_options_find(xfrd->nsd->options, dname);
	if(!zopt) {
		region_recycle(xfrd->region, (void*)dname,
			dname_total_size(dname));
		/* nothing to do */
		if(!ssl_printf(ssl, "warning zone %s not present\n", arg))
			return 0;
		return 1;
	}

	/* see if it can be deleted */
	if(zopt->part_of_config) {
		region_recycle(xfrd->region, (void*)dname,
			dname_total_size(dname));
		(void)ssl_printf(ssl, "error zone defined in nsd.conf, "
			"cannot delete it in this manner: remove it from "
			"nsd.conf yourself and repattern\n");
		return 0;
	}

	/* create deletion task */
	task_new_del_zone(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, dname);
	xfrd_set_reload_now(xfrd);
	/* delete it in xfrd */
	if(zone_is_slave(zopt)) {
		xfrd_del_slave_zone(xfrd, dname);
	}
	xfrd_del_notify(xfrd, dname);
	/* delete from config */
	zone_list_del(xfrd->nsd->options, zopt);

	region_recycle(xfrd->region, (void*)dname, dname_total_size(dname));
	return 1;
}

/** do the addzone command */
static void
do_addzone(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	if(!perform_addzone(ssl, xfrd, arg))
		return;
	send_ok(ssl);
}

/** do the delzone command */
static void
do_delzone(SSL* ssl, xfrd_state_t* xfrd, char* arg)
{
	if(!perform_delzone(ssl, xfrd, arg))
		return;
	send_ok(ssl);
}

/** do the addzones command */
static void
do_addzones(SSL* ssl, xfrd_state_t* xfrd)
{
	char buf[2048];
	int num = 0;
	while(ssl_read_line(ssl, buf, sizeof(buf))) {
		if(buf[0] == 0x04 && buf[1] == 0)
			break; /* end of transmission */
		if(!perform_addzone(ssl, xfrd, buf)) {
			if(!ssl_printf(ssl, "error for input line '%s'\n", 
				buf))
				return;
		} else {
			if(!ssl_printf(ssl, "added: %s\n", buf))
				return;
			num++;
		}
	}
	(void)ssl_printf(ssl, "added %d zones\n", num);
}

/** do the delzones command */
static void
do_delzones(SSL* ssl, xfrd_state_t* xfrd)
{
	char buf[2048];
	int num = 0;
	while(ssl_read_line(ssl, buf, sizeof(buf))) {
		if(buf[0] == 0x04 && buf[1] == 0)
			break; /* end of transmission */
		if(!perform_delzone(ssl, xfrd, buf)) {
			if(!ssl_printf(ssl, "error for input line '%s'\n", 
				buf))
				return;
		} else {
			if(!ssl_printf(ssl, "removed: %s\n", buf))
				return;
			num++;
		}
	}
	(void)ssl_printf(ssl, "deleted %d zones\n", num);
}


/** remove TSIG key from config and add task so that reload does too */
static void remove_key(xfrd_state_t* xfrd, const char* kname)
{
	/* add task before deletion because the name string could be deleted */
	task_new_del_key(xfrd->nsd->task[xfrd->nsd->mytask], xfrd->last_task,
		kname);
	key_options_remove(xfrd->nsd->options, kname);
	xfrd_set_reload_now(xfrd); /* this is executed when the current control
		command ends, thus the entire config changes are bunched up */
}

/** add TSIG key to config and add task so that reload does too */
static void add_key(xfrd_state_t* xfrd, key_options_t* k)
{
	key_options_add_modify(xfrd->nsd->options, k);
	task_new_add_key(xfrd->nsd->task[xfrd->nsd->mytask], xfrd->last_task,
		k);
	xfrd_set_reload_now(xfrd);
}

/** check if keys have changed */
static void repat_keys(xfrd_state_t* xfrd, nsd_options_t* newopt)
{
	nsd_options_t* oldopt = xfrd->nsd->options;
	key_options_t* k;
	/* find deleted keys */
	k = (key_options_t*)rbtree_first(oldopt->keys);
	while((rbnode_t*)k != RBTREE_NULL) {
		key_options_t* next = (key_options_t*)rbtree_next(
			(rbnode_t*)k);
		if(!key_options_find(newopt, k->name))
			remove_key(xfrd, k->name);
		k = next;
	}
	/* find added or changed keys */
	RBTREE_FOR(k, key_options_t*, newopt->keys) {
		key_options_t* origk = key_options_find(oldopt, k->name);
		if(!origk)
			add_key(xfrd, k);
		else if(!key_options_equal(k, origk))
			add_key(xfrd, k);
	}
}

/** find zone given the implicit pattern */
static const dname_type*
parse_implicit_name(xfrd_state_t* xfrd,const char* pname)
{
	if(strncmp(pname, PATTERN_IMPLICIT_MARKER,
		strlen(PATTERN_IMPLICIT_MARKER)) != 0)
		return NULL;
	return dname_parse(xfrd->region, pname +
		strlen(PATTERN_IMPLICIT_MARKER));
}

/** remove cfgzone and add task so that reload does too */
static void
remove_cfgzone(xfrd_state_t* xfrd, const char* pname)
{
	/* dname and find the zone for the implicit pattern */
	zone_options_t* zopt = NULL;
	const dname_type* dname = parse_implicit_name(xfrd, pname);
	if(!dname) {
		/* should have a parseable name, but it did not */
		return;
	}

	/* find the zone entry for the implicit pattern */
	zopt = zone_options_find(xfrd->nsd->options, dname);
	if(!zopt) {
		/* this should not happen; implicit pattern has zone entry */
		region_recycle(xfrd->region, (void*)dname,
			dname_total_size(dname));
		return;
	}

	/* create deletion task */
	task_new_del_zone(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, dname);
	xfrd_set_reload_now(xfrd);
	/* delete it in xfrd */
	if(zone_is_slave(zopt)) {
		xfrd_del_slave_zone(xfrd, dname);
	}
	xfrd_del_notify(xfrd, dname);

	/* delete from zoneoptions */
	zone_options_delete(xfrd->nsd->options, zopt);

	/* recycle parsed dname */
	region_recycle(xfrd->region, (void*)dname, dname_total_size(dname));
}

/** add cfgzone and add task so that reload does too */
static void
add_cfgzone(xfrd_state_t* xfrd, const char* pname)
{
	/* add to our zonelist */
	zone_options_t* zopt = zone_options_create(xfrd->nsd->options->region);
	if(!zopt)
		return;
	zopt->part_of_config = 1;
	zopt->name = region_strdup(xfrd->nsd->options->region, 
		pname + strlen(PATTERN_IMPLICIT_MARKER));
	zopt->pattern = pattern_options_find(xfrd->nsd->options, pname);
	if(!zopt->name || !zopt->pattern)
		return;
	if(!nsd_options_insert_zone(xfrd->nsd->options, zopt)) {
		log_msg(LOG_ERR, "bad domain name or duplicate zone '%s' "
			"pattern %s", zopt->name, pname);
	}

	/* make addzone task and schedule reload */
	task_new_add_zone(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, zopt->name, pname,
		getzonestatid(xfrd->nsd->options, zopt));
	/* zonestat_inc is done after the entire config file has been done */
	xfrd_set_reload_now(xfrd);
	/* add to xfrd - notify (for master and slaves) */
	init_notify_send(xfrd->notify_zones, xfrd->region, zopt);
	/* add to xfrd - slave */
	if(zone_is_slave(zopt)) {
		xfrd_init_slave_zone(xfrd, zopt);
	}
}

/** remove pattern and add task so that reload does too */
static void
remove_pat(xfrd_state_t* xfrd, const char* name)
{
	/* add task before deletion, because name-string could be deleted */
	task_new_del_pattern(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, name);
	pattern_options_remove(xfrd->nsd->options, name);
	xfrd_set_reload_now(xfrd);
}

/** add pattern and add task so that reload does too */
static void
add_pat(xfrd_state_t* xfrd, pattern_options_t* p)
{
	pattern_options_add_modify(xfrd->nsd->options, p);
	task_new_add_pattern(xfrd->nsd->task[xfrd->nsd->mytask],
		xfrd->last_task, p);
	xfrd_set_reload_now(xfrd);
}

/** interrupt zones that are using changed or removed patterns */
static void
repat_interrupt_zones(xfrd_state_t* xfrd, nsd_options_t* newopt)
{
	/* if masterlist changed:
	 *   interrupt slave zone (UDP or TCP) transfers.
	 *   slave zones reset master to start of list.
	 */
	xfrd_zone_t* xz;
	struct notify_zone_t* nz;
	RBTREE_FOR(xz, xfrd_zone_t*, xfrd->zones) {
		pattern_options_t* oldp = xz->zone_options->pattern;
		pattern_options_t* newp = pattern_options_find(newopt,
			oldp->pname);
		if(!newp || !acl_list_equal(oldp->request_xfr,
			newp->request_xfr)) {
			/* interrupt transfer */
			if(xz->tcp_conn != -1) {
				xfrd_tcp_release(xfrd->tcp_set, xz);
				xfrd_set_refresh_now(xz);
			} else if(xz->zone_handler.ev_fd != -1) {
				xfrd_udp_release(xz);
				xfrd_set_refresh_now(xz);
			}
			xz->master = 0;
			xz->master_num = 0;
			xz->next_master = -1;
			xz->round_num = 0; /* fresh set of retries */
		}
	}
	/* if notify list changed:
	 *   interrupt notify that is busy.
	 *   reset notify to start of list.  (clear all other reset_notify)
	 */
	RBTREE_FOR(nz, struct notify_zone_t*, xfrd->notify_zones) {
		pattern_options_t* oldp = nz->options->pattern;
		pattern_options_t* newp = pattern_options_find(newopt,
			oldp->pname);
		if(!newp || !acl_list_equal(oldp->notify, newp->notify)) {
			/* interrupt notify */
			if(nz->notify_send_enable) {
				notify_disable(nz);
				/* set to restart the notify after the
				 * pattern has been changed. */
				nz->notify_restart = 2;
			} else {
				nz->notify_restart = 1;
			}
		} else {
			nz->notify_restart = 0;
		}
	}
}

/** for notify, after the pattern changes, restart the affected notifies */
static void
repat_interrupt_notify_start(xfrd_state_t* xfrd)
{
	struct notify_zone_t* nz;
	RBTREE_FOR(nz, struct notify_zone_t*, xfrd->notify_zones) {
		if(nz->notify_restart) {
			if(nz->notify_current)
				nz->notify_current = nz->options->pattern->notify;
			if(nz->notify_restart == 2) {
				if(nz->notify_restart)
					xfrd_notify_start(nz, xfrd);
			}
		}
	}
}

/** check if patterns have changed */
static void
repat_patterns(xfrd_state_t* xfrd, nsd_options_t* newopt)
{
	/* zones that use changed patterns must have:
	 * - their AXFR/IXFR interrupted: try again, acl may have changed.
	 *   if the old master/key still exists, OK, fix master-numptrs and
	 *   keep going.  Otherwise, stop xfer and reset TSIG.
	 * - send NOTIFY reset to start of NOTIFY list (and TSIG reset).
	 */
	nsd_options_t* oldopt = xfrd->nsd->options;
	pattern_options_t* p;
	int search_zones = 0;

	repat_interrupt_zones(xfrd, newopt);
	/* find deleted patterns */
	p = (pattern_options_t*)rbtree_first(oldopt->patterns);
	while((rbnode_t*)p != RBTREE_NULL) {
		pattern_options_t* next = (pattern_options_t*)rbtree_next(
			(rbnode_t*)p);
		if(!pattern_options_find(newopt, p->pname)) {
			if(p->implicit) {
				/* first remove its zone */
				VERBOSITY(1, (LOG_INFO, "zone removed from config: %s", p->pname + strlen(PATTERN_IMPLICIT_MARKER)));
				remove_cfgzone(xfrd, p->pname);
			}
			remove_pat(xfrd, p->pname);
		}
		p = next;
	}
	/* find added or changed patterns */
	RBTREE_FOR(p, pattern_options_t*, newopt->patterns) {
		pattern_options_t* origp = pattern_options_find(oldopt,
			p->pname);
		if(!origp) {
			/* no zones can use it, no zone_interrupt needed */
			add_pat(xfrd, p);
			if(p->implicit) {
				VERBOSITY(1, (LOG_INFO, "zone added to config: %s", p->pname + strlen(PATTERN_IMPLICIT_MARKER)));
				add_cfgzone(xfrd, p->pname);
			}
		} else if(!pattern_options_equal(p, origp)) {
			uint8_t newstate = 0;
			if (p->request_xfr && !origp->request_xfr) {
				newstate = REPAT_SLAVE;
			} else if (!p->request_xfr && origp->request_xfr) {
				newstate = REPAT_MASTER;
			}
			add_pat(xfrd, p);
			if (p->implicit && newstate) {
				const dname_type* dname =
					parse_implicit_name(xfrd, p->pname);
				if (dname) {
					if (newstate == REPAT_SLAVE) {
						zone_options_t* zopt =
							zone_options_find(
							oldopt, dname);
						if (zopt) {
							xfrd_init_slave_zone(
								xfrd, zopt);
						}
					} else if (newstate == REPAT_MASTER) {
						xfrd_del_slave_zone(xfrd,
							dname);
					}
					region_recycle(xfrd->region,
						(void*)dname,
						dname_total_size(dname));
				}
			} else if(!p->implicit && newstate) {
				/* search all zones with this pattern */
				search_zones = 1;
				origp->xfrd_flags = newstate;
			}
		}
	}
	if (search_zones) {
		zone_options_t* zone_opt;
		/* search in oldopt because 1) it contains zonelist zones,
		 * and 2) you need oldopt(existing) to call xfrd_init */
		RBTREE_FOR(zone_opt, zone_options_t*, oldopt->zone_options) {
			pattern_options_t* oldp = zone_opt->pattern;
			if (!oldp->implicit) {
				if (oldp->xfrd_flags == REPAT_SLAVE) {
					/* xfrd needs stable reference so get
					 * it from the oldopt(modified) tree */
					xfrd_init_slave_zone(xfrd, zone_opt);
				} else if (oldp->xfrd_flags == REPAT_MASTER) {
					xfrd_del_slave_zone(xfrd,
						(const dname_type*)
						zone_opt->node.key);
				}
				oldp->xfrd_flags = 0;
			}
		}
	}
	repat_interrupt_notify_start(xfrd);
}

/** true if options are different that can be set via repat. */
static int
repat_options_changed(xfrd_state_t* xfrd, nsd_options_t* newopt)
{
#ifdef RATELIMIT
	if(xfrd->nsd->options->rrl_ratelimit != newopt->rrl_ratelimit)
		return 1;
	if(xfrd->nsd->options->rrl_whitelist_ratelimit != newopt->rrl_whitelist_ratelimit)
		return 1;
	if(xfrd->nsd->options->rrl_slip != newopt->rrl_slip)
		return 1;
#else
	(void)xfrd; (void)newopt;
#endif
	return 0;
}

/** check if global options have changed */
static void
repat_options(xfrd_state_t* xfrd, nsd_options_t* newopt)
{
	if(repat_options_changed(xfrd, newopt)) {
		/* update our options */
#ifdef RATELIMIT
		xfrd->nsd->options->rrl_ratelimit = newopt->rrl_ratelimit;
		xfrd->nsd->options->rrl_whitelist_ratelimit = newopt->rrl_whitelist_ratelimit;
		xfrd->nsd->options->rrl_slip = newopt->rrl_slip;
#endif
		task_new_opt_change(xfrd->nsd->task[xfrd->nsd->mytask],
			xfrd->last_task, newopt);
		xfrd_set_reload_now(xfrd);
	}
}

/** print errors over ssl, gets pointer-to-pointer to ssl, so it can set
 * the pointer to NULL on failure and stop printing */
static void
print_ssl_cfg_err(void* arg, const char* str)
{
	SSL** ssl = (SSL**)arg;
	if(!*ssl) return;
	if(!ssl_printf(*ssl, "%s", str))
		*ssl = NULL; /* failed, stop printing */
}

/** do the repattern command: reread config file and apply keys, patterns */
static void
do_repattern(SSL* ssl, xfrd_state_t* xfrd)
{
	region_type* region = region_create(xalloc, free);
	nsd_options_t* opt;
	const char* cfgfile = xfrd->nsd->options->configfile;

	/* check chroot and configfile, if possible to reread */
	if(xfrd->nsd->chrootdir) {
		size_t l = strlen(xfrd->nsd->chrootdir);
		while(l>0 && xfrd->nsd->chrootdir[l-1] == '/')
			--l;
		if(strncmp(xfrd->nsd->chrootdir, cfgfile, l) != 0) {
			ssl_printf(ssl, "error %s is not relative to %s: "
				"chroot prevents reread of config\n",
				cfgfile, xfrd->nsd->chrootdir);
			region_destroy(region);
			return;
		}
		cfgfile += l;
	}

	ssl_printf(ssl, "reconfig start, read %s\n", cfgfile);
	opt = nsd_options_create(region);
	if(!parse_options_file(opt, cfgfile, &print_ssl_cfg_err, &ssl)) {
		/* error already printed */
		region_destroy(region);
		return;
	}
	/* check for differences in TSIG keys and patterns, and apply,
	 * first the keys, so that pattern->keyptr can be set right. */
	repat_keys(xfrd, opt);
	repat_patterns(xfrd, opt);
	repat_options(xfrd, opt);
	zonestat_inc_ifneeded(xfrd);
	send_ok(ssl);
	region_destroy(region);
}

/** do the serverpid command: printout pid of server process */
static void
do_serverpid(SSL* ssl, xfrd_state_t* xfrd)
{
	(void)ssl_printf(ssl, "%u\n", (unsigned)xfrd->reload_pid);
}

/** check for name with end-of-string, space or tab after it */
static int
cmdcmp(char* p, const char* cmd, size_t len)
{
	return strncmp(p,cmd,len)==0 && (p[len]==0||p[len]==' '||p[len]=='\t');
}

/** execute a remote control command */
static void
execute_cmd(struct daemon_remote* rc, SSL* ssl, char* cmd, struct rc_state* rs)
{
	char* p = skipwhite(cmd);
	/* compare command */
	if(cmdcmp(p, "stop", 4)) {
		do_stop(ssl, rc->xfrd);
	} else if(cmdcmp(p, "reload", 6)) {
		do_reload(ssl, rc->xfrd, skipwhite(p+6));
	} else if(cmdcmp(p, "write", 5)) {
		do_write(ssl, rc->xfrd, skipwhite(p+5));
	} else if(cmdcmp(p, "status", 6)) {
		do_status(ssl, rc->xfrd);
	} else if(cmdcmp(p, "stats_noreset", 13)) {
		do_stats(rc, 1, rs);
	} else if(cmdcmp(p, "stats", 5)) {
		do_stats(rc, 0, rs);
	} else if(cmdcmp(p, "log_reopen", 10)) {
		do_log_reopen(ssl, rc->xfrd);
	} else if(cmdcmp(p, "addzone", 7)) {
		do_addzone(ssl, rc->xfrd, skipwhite(p+7));
	} else if(cmdcmp(p, "delzone", 7)) {
		do_delzone(ssl, rc->xfrd, skipwhite(p+7));
	} else if(cmdcmp(p, "addzones", 8)) {
		do_addzones(ssl, rc->xfrd);
	} else if(cmdcmp(p, "delzones", 8)) {
		do_delzones(ssl, rc->xfrd);
	} else if(cmdcmp(p, "notify", 6)) {
		do_notify(ssl, rc->xfrd, skipwhite(p+6));
	} else if(cmdcmp(p, "transfer", 8)) {
		do_transfer(ssl, rc->xfrd, skipwhite(p+8));
	} else if(cmdcmp(p, "force_transfer", 14)) {
		do_force_transfer(ssl, rc->xfrd, skipwhite(p+14));
	} else if(cmdcmp(p, "zonestatus", 10)) {
		do_zonestatus(ssl, rc->xfrd, skipwhite(p+10));
	} else if(cmdcmp(p, "verbosity", 9)) {
		do_verbosity(ssl, skipwhite(p+9));
	} else if(cmdcmp(p, "repattern", 9)) {
		do_repattern(ssl, rc->xfrd);
	} else if(cmdcmp(p, "reconfig", 8)) {
		do_repattern(ssl, rc->xfrd);
	} else if(cmdcmp(p, "serverpid", 9)) {
		do_serverpid(ssl, rc->xfrd);
	} else {
		(void)ssl_printf(ssl, "error unknown command '%s'\n", p);
	}
}

/** handle remote control request */
static void
handle_req(struct daemon_remote* rc, struct rc_state* s, SSL* ssl)
{
	int r;
	char pre[10];
	char magic[8];
	char buf[1024];
	if (fcntl(s->c.ev_fd, F_SETFL, 0) == -1) { /* set blocking */
		log_msg(LOG_ERR, "cannot fcntl rc: %s", strerror(errno));
	}

	/* try to read magic UBCT[version]_space_ string */
	ERR_clear_error();
	if((r=SSL_read(ssl, magic, (int)sizeof(magic)-1)) <= 0) {
		if(SSL_get_error(ssl, r) == SSL_ERROR_ZERO_RETURN)
			return;
		log_crypto_err("could not SSL_read");
		return;
	}
	magic[7] = 0;
	if( r != 7 || strncmp(magic, "NSDCT", 5) != 0) {
		VERBOSITY(2, (LOG_INFO, "control connection has bad header"));
		/* probably wrong tool connected, ignore it completely */
		return;
	}

	/* read the command line */
	if(!ssl_read_line(ssl, buf, sizeof(buf))) {
		return;
	}
	snprintf(pre, sizeof(pre), "NSDCT%d ", NSD_CONTROL_VERSION);
	if(strcmp(magic, pre) != 0) {
		VERBOSITY(2, (LOG_INFO, "control connection had bad "
			"version %s, cmd: %s", magic, buf));
		ssl_printf(ssl, "error version mismatch\n");
		return;
	}
	VERBOSITY(2, (LOG_INFO, "control cmd: %s", buf));

	/* figure out what to do */
	execute_cmd(rc, ssl, buf, s);
}

static void
remote_control_callback(int fd, short event, void* arg)
{
	struct rc_state* s = (struct rc_state*)arg;
	struct daemon_remote* rc = s->rc;
	int r;
	if( (event&EV_TIMEOUT) ) {
		log_msg(LOG_ERR, "remote control timed out");
		clean_point(rc, s);
		return;
	}
	/* (continue to) setup the SSL connection */
	ERR_clear_error();
	r = SSL_do_handshake(s->ssl);
	if(r != 1) {
		int r2 = SSL_get_error(s->ssl, r);
		if(r2 == SSL_ERROR_WANT_READ) {
			if(s->shake_state == rc_hs_read) {
				/* try again later */
				return;
			}
			s->shake_state = rc_hs_read;
			event_del(&s->c);
			event_set(&s->c, fd, EV_PERSIST|EV_TIMEOUT|EV_READ,
				remote_control_callback, s);
			if(event_base_set(xfrd->event_base, &s->c) != 0)
				log_msg(LOG_ERR, "remote_accept: cannot set event_base");
			if(event_add(&s->c, &s->tval) != 0)
				log_msg(LOG_ERR, "remote_accept: cannot add event");
			return;
		} else if(r2 == SSL_ERROR_WANT_WRITE) {
			if(s->shake_state == rc_hs_write) {
				/* try again later */
				return;
			}
			s->shake_state = rc_hs_write;
			event_del(&s->c);
			event_set(&s->c, fd, EV_PERSIST|EV_TIMEOUT|EV_WRITE,
				remote_control_callback, s);
			if(event_base_set(xfrd->event_base, &s->c) != 0)
				log_msg(LOG_ERR, "remote_accept: cannot set event_base");
			if(event_add(&s->c, &s->tval) != 0)
				log_msg(LOG_ERR, "remote_accept: cannot add event");
			return;
		} else {
			if(r == 0)
				log_msg(LOG_ERR, "remote control connection closed prematurely");
			log_crypto_err("remote control failed ssl");
			clean_point(rc, s);
			return;
		}
	}
	s->shake_state = rc_none;

	/* once handshake has completed, check authentication */
	if(SSL_get_verify_result(s->ssl) == X509_V_OK) {
		X509* x = SSL_get_peer_certificate(s->ssl);
		if(!x) {
			VERBOSITY(2, (LOG_INFO, "remote control connection "
				"provided no client certificate"));
			clean_point(rc, s);
			return;
		}
		VERBOSITY(3, (LOG_INFO, "remote control connection authenticated"));
		X509_free(x);
	} else {
		VERBOSITY(2, (LOG_INFO, "remote control connection failed to "
			"authenticate with client certificate"));
		clean_point(rc, s);
		return;
	}

	/* if OK start to actually handle the request */
	handle_req(rc, s, s->ssl);

	if(!s->in_stats_list) {
		VERBOSITY(3, (LOG_INFO, "remote control operation completed"));
		clean_point(rc, s);
	}
}

#ifdef BIND8_STATS
static const char*
opcode2str(int o)
{
	switch(o) {
		case OPCODE_QUERY: return "QUERY";
		case OPCODE_IQUERY: return "IQUERY";
		case OPCODE_STATUS: return "STATUS";
		case OPCODE_NOTIFY: return "NOTIFY";
		case OPCODE_UPDATE: return "UPDATE";
		default: return "OTHER";
	}
}

/** print long number */
static int
print_longnum(SSL* ssl, char* desc, uint64_t x)
{
	if(x > (uint64_t)1024*1024*1024) {
		/* more than a Gb */
		size_t front = (size_t)(x / (uint64_t)1000000);
		size_t back = (size_t)(x % (uint64_t)1000000);
		return ssl_printf(ssl, "%s%u%6.6u\n", desc, 
			(unsigned)front, (unsigned)back);
	} else {
		return ssl_printf(ssl, "%s%u\n", desc, (unsigned)x);
	}
}

/* print one block of statistics.  n is name and d is delimiter */
static void
print_stat_block(SSL* ssl, char* n, char* d, struct nsdst* st)
{
	const char* rcstr[] = {"NOERROR", "FORMERR", "SERVFAIL", "NXDOMAIN",
	    "NOTIMP", "REFUSED", "YXDOMAIN", "YXRRSET", "NXRRSET", "NOTAUTH",
	    "NOTZONE", "RCODE11", "RCODE12", "RCODE13", "RCODE14", "RCODE15",
	    "BADVERS"
	};
	size_t i;
	for(i=0; i<= 255; i++) {
		if(inhibit_zero && st->qtype[i] == 0 &&
			strncmp(rrtype_to_string(i), "TYPE", 4) == 0)
			continue;
		if(!ssl_printf(ssl, "%s%snum.type.%s=%u\n", n, d,
			rrtype_to_string(i), (unsigned)st->qtype[i]))
			return;
	}

	/* opcode */
	for(i=0; i<6; i++) {
		if(inhibit_zero && st->opcode[i] == 0 && i != OPCODE_QUERY)
			continue;
		if(!ssl_printf(ssl, "%s%snum.opcode.%s=%u\n", n, d,
			opcode2str(i), (unsigned)st->opcode[i]))
			return;
	}

	/* qclass */
	for(i=0; i<4; i++) {
		if(inhibit_zero && st->qclass[i] == 0 && i != CLASS_IN)
			continue;
		if(!ssl_printf(ssl, "%s%snum.class.%s=%u\n", n, d,
			rrclass_to_string(i), (unsigned)st->qclass[i]))
			return;
	}

	/* rcode */
	for(i=0; i<17; i++) {
		if(inhibit_zero && st->rcode[i] == 0 &&
			i > RCODE_YXDOMAIN) /* NSD does not use larger */
			continue;
		if(!ssl_printf(ssl, "%s%snum.rcode.%s=%u\n", n, d, rcstr[i],
			(unsigned)st->rcode[i]))
			return;
	}

	/* edns */
	if(!ssl_printf(ssl, "%s%snum.edns=%u\n", n, d, (unsigned)st->edns))
		return;

	/* ednserr */
	if(!ssl_printf(ssl, "%s%snum.ednserr=%u\n", n, d,
		(unsigned)st->ednserr))
		return;

	/* qudp */
	if(!ssl_printf(ssl, "%s%snum.udp=%u\n", n, d, (unsigned)st->qudp))
		return;
	/* qudp6 */
	if(!ssl_printf(ssl, "%s%snum.udp6=%u\n", n, d, (unsigned)st->qudp6))
		return;
	/* ctcp */
	if(!ssl_printf(ssl, "%s%snum.tcp=%u\n", n, d, (unsigned)st->ctcp))
		return;
	/* ctcp6 */
	if(!ssl_printf(ssl, "%s%snum.tcp6=%u\n", n, d, (unsigned)st->ctcp6))
		return;

	/* nona */
	if(!ssl_printf(ssl, "%s%snum.answer_wo_aa=%u\n", n, d,
		(unsigned)st->nona))
		return;

	/* rxerr */
	if(!ssl_printf(ssl, "%s%snum.rxerr=%u\n", n, d, (unsigned)st->rxerr))
		return;

	/* txerr */
	if(!ssl_printf(ssl, "%s%snum.txerr=%u\n", n, d, (unsigned)st->txerr))
		return;

	/* number of requested-axfr, number of times axfr served to clients */
	if(!ssl_printf(ssl, "%s%snum.raxfr=%u\n", n, d, (unsigned)st->raxfr))
		return;

	/* truncated */
	if(!ssl_printf(ssl, "%s%snum.truncated=%u\n", n, d,
		(unsigned)st->truncated))
		return;

	/* dropped */
	if(!ssl_printf(ssl, "%s%snum.dropped=%u\n", n, d,
		(unsigned)st->dropped))
		return;
}

#ifdef USE_ZONE_STATS
static void
resize_zonestat(xfrd_state_t* xfrd, size_t num)
{
	struct nsdst** a = xalloc_array_zero(num, sizeof(struct nsdst*));
	if(xfrd->zonestat_clear_num != 0)
		memcpy(a, xfrd->zonestat_clear, xfrd->zonestat_clear_num
			* sizeof(struct nsdst*));
	free(xfrd->zonestat_clear);
	xfrd->zonestat_clear = a;
	xfrd->zonestat_clear_num = num;
}

static void
zonestat_print(SSL* ssl, xfrd_state_t* xfrd, int clear)
{
	struct zonestatname* n;
	struct nsdst stat0, stat1;
	RBTREE_FOR(n, struct zonestatname*, xfrd->nsd->options->zonestatnames){
		char* name = (char*)n->node.key;
		if(n->id >= xfrd->zonestat_safe)
			continue; /* newly allocated and reload has not yet
				done and replied with new size */
		if(name == NULL || name[0]==0)
			continue; /* empty name, do not output */
		/* the statistics are stored in two blocks, during reload
		 * the newly forked processes get the other block to use,
		 * these blocks are mmapped and are currently in use to
		 * add statistics to */
		memcpy(&stat0, &xfrd->nsd->zonestat[0][n->id], sizeof(stat0));
		memcpy(&stat1, &xfrd->nsd->zonestat[1][n->id], sizeof(stat1));
		stats_add(&stat0, &stat1);
		
		/* save a copy of current (cumulative) stats in stat1 */
		memcpy(&stat1, &stat0, sizeof(stat1));
		/* subtract last total of stats that was 'cleared' */
		if(n->id < xfrd->zonestat_clear_num &&
			xfrd->zonestat_clear[n->id])
			stats_subtract(&stat0, xfrd->zonestat_clear[n->id]);
		if(clear) {
			/* extend storage array if needed */
			if(n->id >= xfrd->zonestat_clear_num) {
				if(n->id+1 < xfrd->nsd->options->zonestatnames->count)
					resize_zonestat(xfrd, xfrd->nsd->options->zonestatnames->count);
				else
					resize_zonestat(xfrd, n->id+1);
			}
			if(!xfrd->zonestat_clear[n->id])
				xfrd->zonestat_clear[n->id] = xalloc(
					sizeof(struct nsdst));
			/* store last total of stats */
			memcpy(xfrd->zonestat_clear[n->id], &stat1,
				sizeof(struct nsdst));
		}

		/* stat0 contains the details that we want to print */
		if(!ssl_printf(ssl, "%s%snum.queries=%u\n", name, ".",
			(unsigned)(stat0.qudp + stat0.qudp6 + stat0.ctcp +
				stat0.ctcp6)))
			return;
		print_stat_block(ssl, name, ".", &stat0);
	}
}
#endif /* USE_ZONE_STATS */

static void
print_stats(SSL* ssl, xfrd_state_t* xfrd, struct timeval* now, int clear)
{
	size_t i;
	stc_t total = 0;
	struct timeval elapsed, uptime;

	/* per CPU and total */
	for(i=0; i<xfrd->nsd->child_count; i++) {
		if(!ssl_printf(ssl, "server%d.queries=%u\n", (int)i,
			(unsigned)xfrd->nsd->children[i].query_count))
			return;
		total += xfrd->nsd->children[i].query_count;
	}
	if(!ssl_printf(ssl, "num.queries=%u\n", (unsigned)total))
		return;

	/* time elapsed and uptime (in seconds) */
	timeval_subtract(&uptime, now, &xfrd->nsd->rc->boot_time);
	timeval_subtract(&elapsed, now, &xfrd->nsd->rc->stats_time);
	if(!ssl_printf(ssl, "time.boot=%u.%6.6u\n",
		(unsigned)uptime.tv_sec, (unsigned)uptime.tv_usec))
		return;
	if(!ssl_printf(ssl, "time.elapsed=%u.%6.6u\n",
		(unsigned)elapsed.tv_sec, (unsigned)elapsed.tv_usec))
		return;

	/* mem info, database on disksize */
	if(!print_longnum(ssl, "size.db.disk=", xfrd->nsd->st.db_disk))
		return;
	if(!print_longnum(ssl, "size.db.mem=", xfrd->nsd->st.db_mem))
		return;
	if(!print_longnum(ssl, "size.xfrd.mem=", region_get_mem(xfrd->region)))
		return;
	if(!print_longnum(ssl, "size.config.disk=", 
		xfrd->nsd->options->zonelist_off))
		return;
	if(!print_longnum(ssl, "size.config.mem=", region_get_mem(
		xfrd->nsd->options->region)))
		return;
	print_stat_block(ssl, "", "", &xfrd->nsd->st);

	/* zone statistics */
	if(!ssl_printf(ssl, "zone.master=%u\n",
		(unsigned)(xfrd->notify_zones->count - xfrd->zones->count)))
		return;
	if(!ssl_printf(ssl, "zone.slave=%u\n", (unsigned)xfrd->zones->count))
		return;
#ifdef USE_ZONE_STATS
	zonestat_print(ssl, xfrd, clear); /* per-zone statistics */
#else
	(void)clear;
#endif
}

static void
clear_stats(xfrd_state_t* xfrd)
{
	size_t i;
	uint64_t dbd = xfrd->nsd->st.db_disk;
	uint64_t dbm = xfrd->nsd->st.db_mem;
	for(i=0; i<xfrd->nsd->child_count; i++) {
		xfrd->nsd->children[i].query_count = 0;
	}
	memset(&xfrd->nsd->st, 0, sizeof(struct nsdst));
	/* zonestats are cleared by storing the cumulative value that
	 * was last printed in the zonestat_clear array, and subtracting
	 * that before the next stats printout */
	xfrd->nsd->st.db_disk = dbd;
	xfrd->nsd->st.db_mem = dbm;
}

void
daemon_remote_process_stats(struct daemon_remote* rc)
{
	struct rc_state* s;
	struct timeval now;
	if(!rc) return;
	if(gettimeofday(&now, NULL) == -1)
		log_msg(LOG_ERR, "gettimeofday: %s", strerror(errno));
	/* pop one and give it stats */
	while((s = rc->stats_list)) {
		assert(s->in_stats_list);
		print_stats(s->ssl, rc->xfrd, &now, (s->in_stats_list == 1));
		if(s->in_stats_list == 1) {
			clear_stats(rc->xfrd);
			rc->stats_time = now;
		}
		VERBOSITY(3, (LOG_INFO, "remote control stats printed"));
		rc->stats_list = s->next;
		s->in_stats_list = 0;
		clean_point(rc, s);
	}
}
#endif /* BIND8_STATS */

#endif /* HAVE_SSL */
