/*-
 * Copyright (c) 2007
 *	Internet Initiative Japan Inc.  All rights reserved.
 */
/* $Id: rt_zebra.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
/*
 * @file This file provides utility functions to help add/delete routing
 * information with GNU Zebra.  This utility uses event(3) and uses a UNIX
 * domain socket at communication with the zserv(Zebra).
 * <p>
 * example:
 * <pre>
    rt_zebra *rtz;

    rtz = rt_zebra_get_instance();
    rt_zebra_init(rtz);
    rt_zebra_start(rtz);

    // add 10.0.0.0/8 to a blackhole
    rt_zebra_add_ipv4_blackhole_rt(rtz, 0x0a000000, 0xff00000);

    // delete 10.0.0.0/8
    rt_zebra_delete_ipv4_blackhole_rt(rtz, 0x0a000000, 0xff00000);

    rt_zebra_stop(rtz);
    rt_zebra_fini(rtz);
 * </pre></p>
 */
/* compile-time options */
#ifndef	RT_ZEBRA_BLACKHOLE_IFNAME
#define	RT_ZEBRA_BLACKHOLE_IFNAME		"lo0"
#endif
#ifndef	DEFAULT_RT_ZEBRA_BLACKHOLE_DISTANCE
#define	DEFAULT_RT_ZEBRA_BLACKHOLE_DISTANCE	16
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>

#include "debugutil.h"
#include "bytebuf.h"
#include "net_utils.h"

#include <zebra.h>
#include <prefix.h>
#include <zclient.h>

#include "rt_zebra.h"
#include "rt_zebra_local.h"

static void  rt_zebra_set_event (rt_zebra *);
static void  rt_zebra_io_event (int, short, void *);
static int   rt_zebra_log (rt_zebra *, int, const char *, ...) __printflike(3,4);
static int   rt_zebra_ipv4_blackhole_rt0(rt_zebra *, uint32_t, uint32_t, int);

static rt_zebra rt_zebra_singleton; 		/* singleton */
static int rt_zebra_blackhole_ifidx = -1;	

/** Returns the only one rt_zebra context. */
rt_zebra *
rt_zebra_get_instance(void)
{
	return &rt_zebra_singleton;
}

/** Initialize the given rt_zebra context. */
int
rt_zebra_init(rt_zebra *_this)
{
	RT_ZEBRA_ASSERT((_this->state == ZEBRA_STATUS_INIT0));

	memset(_this, 0, sizeof(rt_zebra));
	_this->sock = -1;
	if ((_this->buffer = bytebuffer_create(8192)) == NULL)
		return 1;
	_this->state = ZEBRA_STATUS_INIT;

	if (rt_zebra_blackhole_ifidx == -1)
		rt_zebra_blackhole_ifidx = if_nametoindex(
		    RT_ZEBRA_BLACKHOLE_IFNAME);

	return 0;
}

/** Finalialize the given rt_zebra context. */
void
rt_zebra_fini(rt_zebra *_this)
{
	if (_this->state != ZEBRA_STATUS_STOPPED &&
	    _this->state != ZEBRA_STATUS_DISPOSING)
		rt_zebra_stop(_this);

	if (_this->buffer != NULL)
		bytebuffer_destroy(_this->buffer);
	_this->buffer = NULL;
	_this->state = ZEBRA_STATUS_DISPOSING;
}

/**
 * Add the specified IPv4 blackhole routing entry.
 * @param addr	the detination IPv4 address part in host byte-order.
 * @param mask	the detination IPv4 netmask part in host byte-order.
 */
int
rt_zebra_add_ipv4_blackhole_rt(rt_zebra *_this, uint32_t addr,
    uint32_t mask)
{
	return rt_zebra_ipv4_blackhole_rt0(_this, addr, mask, 0);
}

/**
 * Deletes the specified IPv4 blackhole routing entry. 
 * @param addr	the detination IPv4 address part in host byte-order.
 * @param mask	the detination IPv4 netmask part in host byte-order.
 */
int
rt_zebra_delete_ipv4_blackhole_rt(rt_zebra *_this, uint32_t addr,
    uint32_t mask)
{
	return rt_zebra_ipv4_blackhole_rt0(_this, addr, mask, 1);
}

/** Start processing */
int
rt_zebra_start(rt_zebra *_this)
{
	int ival, sock;
	struct sockaddr_un sun;

	RT_ZEBRA_ASSERT(_this->state == ZEBRA_STATUS_INIT ||
	    _this->state == ZEBRA_STATUS_STOPPED);

	sun.sun_len = sizeof(sun);
	sun.sun_family = AF_LOCAL;
	strlcpy(sun.sun_path, ZEBRA_SERV_PATH, sizeof(sun.sun_path));

	sock = -1;

	if ((sock = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
		log_printf(LOG_ERR,
		    "Creating a socket to the zserv failed: %m");
		return 1;
	}
	if ((ival = fcntl(sock, F_GETFL, 0)) < 0) {
		log_printf(LOG_ERR, "fcntl(,F_GETFL) failed: %m");
		goto fail;
	} else if (fcntl(sock, F_SETFL, ival | O_NONBLOCK) < 0) {
		log_printf(LOG_ERR, "fcntl(,F_SETFL, +O_NONBLOCK) failed: %m");
		goto fail;
	}

	_this->state = ZEBRA_STATUS_CONNECTING;
	_this->sock = sock;
	sock = -1;
	if (connect(_this->sock, (struct sockaddr *)&sun, sizeof(sun)) == 0) {
		/*
		 * don't change the state here, but change it on the next
		 * write ready event.
		 */
	} else {
		switch (errno) {
		case EINPROGRESS:
			break;
		default:
			log_printf(LOG_ERR,
			    "Connection to the zserv failed: %m");		
			goto fail;
		}
	}
	event_set(&_this->ev_sock, _this->sock, EV_READ|EV_WRITE,
	    rt_zebra_io_event, _this);
	event_add(&_this->ev_sock, NULL);

	return 0;
fail:
	if (sock >= 0)
		close(sock);
	rt_zebra_stop(_this);

	return 1;
}

/** Stop processing */
void
rt_zebra_stop(rt_zebra *_this)
{
	if (_this->sock >= 0) {
		event_del(&_this->ev_sock);
		close(_this->sock);
	}
	_this->sock = -1;
	_this->state = ZEBRA_STATUS_STOPPED;
}

/** Is processing */
int
rt_zebra_is_running(rt_zebra *_this)
{
	return (_this->state == ZEBRA_STATUS_INIT ||
	    _this->state == ZEBRA_STATUS_STOPPED ||
	    _this->state == ZEBRA_STATUS_DISPOSING)? 0 : 1;
}

static int
rt_zebra_ipv4_blackhole_rt0(rt_zebra *_this, uint32_t addr0,
    uint32_t mask0, int delete)
{
	int i, prefix, msg_flags, len, flags0, distance;
	u_char buf[1024], *cp;
	uint32_t addr, mask;

	RT_ZEBRA_DBG((_this, LOG_DEBUG,
	    "%s %s(%08x,%08x)", __func__, (delete)? "delete" : "add",
	    addr0, mask0));

	if (_this->state == ZEBRA_STATUS_INIT ||
	    _this->state == ZEBRA_STATUS_DISPOSING)
		return 1;

	distance = DEFAULT_RT_ZEBRA_BLACKHOLE_DISTANCE;
	addr = ntohl(addr0);
	mask = ntohl(mask0);

	/*
	 * Create a zebra protocol message.
	 */
	cp = buf;
	msg_flags = ZAPI_MESSAGE_NEXTHOP | ZAPI_MESSAGE_DISTANCE;
	flags0 = ZEBRA_FLAG_STATIC | ZEBRA_FLAG_BLACKHOLE;

	/* zebra protocol header */
	PUTSHORT(0, cp);			/* length place holder */
	if (delete) {
		PUTCHAR(ZEBRA_IPV4_ROUTE_DELETE, cp);	/* command */
	} else {
		PUTCHAR(ZEBRA_IPV4_ROUTE_ADD, cp);	/* command */
	}

	PUTCHAR(ZEBRA_ROUTE_STATIC, cp);	/* route type */
	PUTCHAR(flags0, cp);			/* flags */
	PUTCHAR(msg_flags, cp);			/* message */

	/* destination address/netmask */
	prefix = netmask2prefixlen(mask);
	PUTCHAR(prefix, cp);			/* prefix */
	for (i = 0; i < (prefix + 7) / 8; i++)
		PUTCHAR(*(((u_char *)&addr0) + i), cp);

	/* nexthop */
	PUTCHAR(1, cp);				/* number of message */
	PUTCHAR(ZEBRA_NEXTHOP_IFINDEX, cp);
	PUTLONG(rt_zebra_blackhole_ifidx , cp);

	if ((msg_flags & ZAPI_MESSAGE_DISTANCE) != 0)
		PUTCHAR(distance, cp);		/* distance */

	len = cp - buf;				/* save length */
	cp = buf;				/* rewind the position */
	PUTSHORT(len, cp);			/* length */

	if (bytebuffer_put(_this->buffer, buf, len) == NULL)
		return 1;

	if (_this->state == ZEBRA_STATUS_CONNECTED) {
		if (_this->write_ready)
			rt_zebra_io_event(_this->sock, 0, _this);
	}
	if (_this->state == ZEBRA_STATUS_STOPPED)
		rt_zebra_start(_this);

	return 0;
}

static void
rt_zebra_io_event(int fd, short ev, void *ctx)
{
	int sz;
	u_char buf[BUFSIZ];
	rt_zebra *_this;

	_this = ctx;

	RT_ZEBRA_DBG((_this, LOG_DEBUG, "%s [%s%s%s%s]", __func__,
	    (ev & EV_READ)? "R" : "-", (ev & EV_WRITE)? "W" : "-",
	    (_this->write_ready)? "w" : "-",
	    (bytebuffer_position(_this->buffer) != 0)? "P" : "-"));

	if ((ev & EV_WRITE) != 0) {
		if (_this->state == ZEBRA_STATUS_CONNECTING) {
			rt_zebra_log(_this, LOG_NOTICE,
			    "Established a new connection to the zserv.");
			_this->state = ZEBRA_STATUS_CONNECTED;
		}
		_this->write_ready = 1;
	}
	if ((ev & EV_READ) != 0) {
		if ((sz = read(_this->sock, buf, sizeof(buf))) <= 0) {
			if (sz == 0 || errno == ECONNRESET) {
				/* connection closed or reseted by the peer. */

				rt_zebra_log(_this, LOG_INFO,
				    "Connection closed by the zserv");
				rt_zebra_stop(_this);
				return;
			}
			if (errno != EAGAIN) {
				rt_zebra_log(_this, LOG_INFO,
				    "read() failed from the zserv: %m");
				rt_zebra_stop(_this);
				return;
			}
		} else {
			/* assumes no responce from a zebra. */

			RT_ZEBRA_ASSERT("NOTREACHED" == NULL);
			RT_ZEBRA_DBG((_this, LOG_DEBUG,
			    "Received unexpected %d bytes message.", sz));
		}
	}
	if (_this->write_ready != 0) {
		bytebuffer_flip(_this->buffer);
		while (bytebuffer_has_remaining(_this->buffer)) {
			if ((sz = write(_this->sock,
			    bytebuffer_pointer(_this->buffer),
			    bytebuffer_remaining(_this->buffer))) < 0) {
				if (errno == EAGAIN)
					break;
				bytebuffer_compact(_this->buffer);

				rt_zebra_log(_this, LOG_ERR,
				    "write() failed to the zserv.:%m");
				_this->state = ZEBRA_STATUS_CONNECTED;
				rt_zebra_stop(_this);
				return;
			}
			_this->write_ready = 0;
			bytebuffer_get(_this->buffer, BYTEBUFFER_GET_DIRECT,
			    sz);
		}
		bytebuffer_compact(_this->buffer);
	}
	rt_zebra_set_event(_this);

	return;
}

static int
rt_zebra_log(rt_zebra *_this, int logprio, const char *fmt, ...)
{
	int rval;
	char buf[BUFSIZ];
	va_list ap;

	strlcpy(buf, "rt_zebra ", sizeof(buf));
	strlcat(buf, fmt, sizeof(buf));

	va_start(ap, fmt);
	rval = vlog_printf(logprio, buf, ap);
	va_end(ap);

	return rval;
}

static void
rt_zebra_set_event(rt_zebra *_this)
{
	int evmask;

	RT_ZEBRA_ASSERT(_this->sock >= 0);
	evmask = EV_READ;
	if (_this->write_ready == 0)
		evmask |= EV_WRITE;

	event_del(&_this->ev_sock);
	event_set(&_this->ev_sock, _this->sock, evmask, rt_zebra_io_event,
	    _this);
	event_add(&_this->ev_sock, NULL);
}
