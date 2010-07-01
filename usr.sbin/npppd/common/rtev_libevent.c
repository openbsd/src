/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: rtev_libevent.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/route.h>
#include <ifaddrs.h>
#include <time.h>
#include <event.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>

#define	NO_RTEV_WRAPPER 1
#include "rtev_local.h"
#include "rtev.h"

typedef struct _rtev_libevent rtev_libevent;

static int           rtev_libevent_init0 (rtev_libevent *, int, int, int, int);
static void          rtev_libevent_fini  (rtev_impl *);
static inline time_t get_monosec(void);

static rtev_libevent rtev_libevent_this;

/**
 * Initialize 'rtev' with libevent.
 *
 * @param rt_delay_sec		wait given time before we work after routing
 *				event.
 * @param send_delay_millisec	wait given time each sending.
 * @param send_npackets		send give number of packets at once.
 */
int
rtev_libevent_init(int rt_delay_sec, int send_delay_millisec, int send_npackets,
    int flags)
{
	RTEV_DBG((LOG_DEBUG, "%s(%d,%d,%d)",
	    __func__, rt_delay_sec, send_delay_millisec, send_npackets));
	return rtev_libevent_init0(&rtev_libevent_this, rt_delay_sec,
	    send_delay_millisec, send_npackets, flags);
}

/***********************************************************************
 * private functions
 ***********************************************************************/
static void  rtev_libevent_reset_event (rtev_libevent *);
static void  rtev_libevent_timer_event (int, short, void *);
static void  rtev_libevent_io_event (int, short, void *);

struct _rtev_libevent {
	rtev_impl impl;
	int sock;
	int nsend;
	int p_delay;
	int w_delay;	/* milli sec */
	int write_ready:1, write_wait:1;
	struct event ev, ev_timer;
	time_t last_rtupdate;
};

static void
rtev_libevent_reset_event(rtev_libevent *_this)
{
	int evmask;
	struct timeval tv;

	if (event_initialized(&_this->ev_timer))
		event_del(&_this->ev_timer);
	if (event_initialized(&_this->ev))
		event_del(&_this->ev);

	/*
	 * I/O Event
	 */
	evmask = EV_READ;
	if (_this->write_ready == 0 && rtev_has_write_pending())
		evmask |= EV_WRITE;
	event_set(&_this->ev, _this->sock, evmask, rtev_libevent_io_event,
	    _this);
	event_add(&_this->ev, NULL);
	RTEV_DBG((DEBUG_LEVEL_2, "%s I/O [%s%s] Wait", __func__,
	    ((evmask & EV_READ) != 0)? "R" : "",
	    ((evmask & EV_WRITE) != 0)? "W" : ""));

	/*
	 * Timer event
	 */
	if (_this->write_wait != 0 && _this->w_delay > 0) {
		RTEV_DBG((DEBUG_LEVEL_2, "%s Timer %f sec", __func__,
		    _this->w_delay / 1000.0));
		tv.tv_sec = _this->w_delay / 1000;
		tv.tv_usec = (_this->w_delay % 1000) * 1000L;
		RTEV_ASSERT(tv.tv_usec < 1000000L);
		RTEV_ASSERT(tv.tv_usec >= 0L);
		evtimer_set(&_this->ev_timer, rtev_libevent_timer_event, _this);
		event_add(&_this->ev_timer, &tv);
	} else if (_this->last_rtupdate != 0 && _this->p_delay > 0) {
		time_t currtime;

		currtime = get_monosec();

		tv.tv_sec = _this->last_rtupdate + _this->p_delay - currtime;
		tv.tv_usec = 0;
		if (tv.tv_sec < 0)
			tv.tv_sec = 0;
		RTEV_DBG((DEBUG_LEVEL_2, "%s Timer %ld sec", __func__,
		    (long)tv.tv_sec));

		evtimer_set(&_this->ev_timer, rtev_libevent_timer_event, _this);
		event_add(&_this->ev_timer, &tv);
	}
}

static void
rtev_libevent_timer_event(int sock, short evmask, void *ctx)
{
	time_t currtime;
	rtev_libevent *_this;

	_this = ctx;
	RTEV_DBG((DEBUG_LEVEL_2, "%s", __func__));

	currtime = get_monosec();
	if (_this->last_rtupdate + _this->p_delay <= currtime) {
		_this->impl.base_on_rtevent(&_this->impl);
		_this->last_rtupdate = 0;
	}

	if (_this->write_wait != 0) {
		_this->write_wait = 0;
		if (rtev_has_write_pending() && _this->write_ready != 0) {
			RTEV_DBG((DEBUG_LEVEL_1, "rt_send() by timer"));
			if (_this->impl.base_on_write(&_this->impl,
			    _this->sock, _this->nsend) != 0) {
				log_printf(LOG_INFO,
				    "sending message to routing socket failed"
				    ": %m");
			}
			_this->write_ready = 0;
			_this->write_wait = 1;	/* wait again */
		}
	}

	rtev_libevent_reset_event(_this);
}

static void
rtev_libevent_io_event(int sock, short evmask, void *ctx)
{
	char buf[8192];
	struct rt_msghdr *rt_msg;
	int sz, rt_updated;
	rtev_libevent *_this;

	_this = ctx;

	RTEV_DBG((DEBUG_LEVEL_2, "%s I/O [%s%s] Ready", __func__,
	    ((evmask & EV_READ) != 0)? "R" : "",
	    ((evmask & EV_WRITE) != 0)? "W" : ""));

	if ((evmask & EV_WRITE) != 0) {
		if (_this->write_wait != 0 && _this->w_delay > 0) {
			_this->write_ready = 1;
		} else {
			RTEV_DBG((DEBUG_LEVEL_1, "rt_send() by event"));
			if (_this->impl.base_on_write(&_this->impl, sock,
			    _this->nsend) != 0) {
				log_printf(LOG_INFO,
				    "sending message to routing socket failed"
				    ": %m");
			}
			_this->write_ready = 0;
			_this->write_wait = 1;
		}
	}
	if ((evmask & EV_READ) != 0) {
		rt_updated = 0;
		while ((sz = recv(sock, buf, sizeof(buf), 0)) > 0) {
			rt_msg = (struct rt_msghdr *)buf;
			if (rt_msg->rtm_version != RTM_VERSION)
				continue;
			switch (rt_msg->rtm_type) {
			case RTM_ADD:
			case RTM_CHANGE:
			case RTM_NEWADDR:
			case RTM_DELADDR:
			case RTM_DELETE:
			case RTM_IFINFO:
				rt_updated++;
				break;
			}
		}
		if (sz < 0)
			RTEV_ASSERT(errno == EAGAIN);
		if (rt_updated) {
			if (_this->p_delay <= 0)
				rtev_libevent_timer_event(sock, evmask, ctx);
			else if (_this->last_rtupdate == 0)
				_this->last_rtupdate = get_monosec();
		}
	}
	rtev_libevent_reset_event(_this);
}

static void
rtev_libevent_on_write(rtev_impl *impl)
{
	rtev_libevent *_this = impl->impl;

	rtev_libevent_reset_event(_this);
}

static int
rtev_libevent_init0(rtev_libevent *_this, int rt_delay, int send_delay,
    int nsend, int flags)
{
	int sock, fflags, dummy;

	sock = -1;
	memset(_this, 0, sizeof(rtev_libevent));

	_this->impl.impl = _this;
	_this->impl.impl_fini = rtev_libevent_fini;
	_this->impl.impl_on_write = rtev_libevent_on_write;

	if (rtev_base_init(&_this->impl, flags) != 0)
		goto fail;

	if ((sock = socket(PF_ROUTE, SOCK_RAW, AF_UNSPEC)) < 0)
		goto fail;

	dummy = 0;
	if ((fflags = fcntl(sock, F_GETFL, dummy)) < 0)
		goto fail;

	if (fcntl(sock, F_SETFL, fflags | O_NONBLOCK) < 0)
		goto fail;

	_this->sock = sock;

	_this->w_delay = send_delay;
	_this->nsend = nsend;
	_this->p_delay = rt_delay;
	if (rt_delay <= 0)
		rtev_libevent_timer_event(0, EV_TIMEOUT, _this);
	rtev_libevent_reset_event(_this);

	return 0;
fail:
	if (sock >= 0)
		close(sock);

	if (event_initialized(&_this->ev))
		event_del(&_this->ev);

	_this->impl.impl = NULL;

	return -1;
}

static void
rtev_libevent_fini(rtev_impl *impl)
{
	rtev_libevent *_this = impl->impl;

	if (_this->sock >= 0)
		close(_this->sock);

	if (event_initialized(&_this->ev))
		event_del(&_this->ev);
	if (event_initialized(&_this->ev_timer))
		event_del(&_this->ev_timer);
	_this->sock = -1;
}

static inline time_t
get_monosec(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		abort();
	return ts.tv_sec;
}
