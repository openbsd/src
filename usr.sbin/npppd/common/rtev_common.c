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
/* $Id: rtev_common.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
/*
 * PF_ROUTE related utility functions.
 * <p>
 * When use with libevent, call rtev_libevent_init() to initialize this
 * library.</p>
 * usage:
 * <pre>
 *	    #include <sys/types.h>
 *	    #include <time.h>
 *	    #include <event.h>
 *	    #include <ifaddrs.h>
 *	    #include <rtev.h>
 *	
 *	    int main()
 *	    {
 *	    	event_init();
 *	    	rtev_libevent_init(5, 100, 16);	// init after event_init()
 *
 *	    	event_loop();
 *
 *	    	rtev_fini();			// fini before exit()
 *	    	exit(0);
 *	    }
 *
 *	    void hogehoge()
 *	    {
 *		struct ifaddrs *ifa = NULL;
 *
 *		getifaddrs(&ifa);	// rtev.h replaces getifaddrs(3)
 *		    :
 *		freeifaddrs(&ifa);
 *	    }
 * </pre>
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <ifaddrs.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#define	NO_RTEV_WRAPPER 1
#include "bytebuf.h"
#include "rtev.h"
#include "rtev_local.h"

#ifdef RTEV_DEBUG
#include "debugutil.h"
#endif

#ifndef	RTEV_BUFSIZ
#define	RTEV_BUFSIZ		131072	/* 128K */
#endif

static int   rtev_base_on_rtevent(rtev_impl *);
static int   rtev_base_on_write(rtev_impl *, int, int);

static struct ifaddrs *rtev_cached_ifa = NULL;
static int rtev_event_serial = 0;
static int rtev_send_serial = 0;
static int rtev_ifa_cache_serial = 0;
static bytebuffer *rtev_sndbuf = NULL;
static u_char rtev_buffer_space[RTEV_BUFSIZ];
static rtev_impl *singleton_impl = NULL;

static inline int             rtev_update_ifa_cache(rtev_impl *);
static int                    rtev_update_ifa_cache_do(rtev_impl *);
static void                   ifa_rbentry_init (void);
static void                   ifa_rbentry_fini (void);
static inline int             ifa_rb_ifacenam_insert (struct ifaddrs *);
static inline int             ifa_rb_sockaddr_insert (struct ifaddrs *);
static inline struct ifaddrs  *ifa_rb_ifacenam_find (const char *);
static inline struct ifaddrs  *ifa_rb_sockaddr_find (struct sockaddr const *);

/**
 * Write a routing message.
 * @return not zero indicates error.  See errno.
 */
int
rtev_write(void *rtm_msg)
{
	int rval;
	struct rt_msghdr *rtm;

	rtm = rtm_msg;
	rtm->rtm_seq = rtev_send_serial++;
	if (bytebuffer_put(rtev_sndbuf, rtm, rtm->rtm_msglen) == NULL)
		rval = -1;
	else
		rval = rtm->rtm_msglen;

	if (singleton_impl->impl_on_write != NULL)
		singleton_impl->impl_on_write(singleton_impl);

	return rval;
}

/**
 * same as getifaddrs(3) but returned obeject is cached.
 * The cached object may be freed by the event handler of this library,
 * so you cannot use it after the event handler returns the event loop.
 */
int
rtev_getifaddrs(struct ifaddrs **ifa)
{
	if (rtev_update_ifa_cache(singleton_impl) != 0)
		return 1;

	*ifa = rtev_cached_ifa;

	return 0;
}

/**
 * checks whether given address is the primary address of the interface.
 * @return not zero if the address is the primary.
 */
int
rtev_ifa_is_primary(const char *ifname, struct sockaddr *sa)
{
	int count;
	struct ifaddrs *ifa;

	for (count = 0, ifa = rtev_getifaddrs_by_ifname(ifname); ifa != NULL;
	    ifa = ifa->ifa_next) {
		
		if (strcmp(ifa->ifa_name, ifname) != 0)
			break;
		if (ifa->ifa_addr->sa_family != sa->sa_family)
			continue;
		switch (sa->sa_family) {
		case AF_INET:
			if (((struct sockaddr_in *)sa)->sin_addr.s_addr ==
			    ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr
			    .s_addr) {
				if (count == 0)
					return 1;
				return 0;
			}
			count++;
			break;
		case AF_INET6:
			if (IN6_ARE_ADDR_EQUAL(
			    &((struct sockaddr_in6 *)sa)->sin6_addr,
			    &((struct sockaddr_in6 *)ifa->ifa_addr)
			    ->sin6_addr)){
				if (count == 0)
					return 1;
				return 0;
			}
			count++;
			break;
		}
	}
	return 0;
}

/** returns the routing event serial number */
int
rtev_get_event_serial()
{
	return rtev_event_serial;
}

/** same as ifaddrs(3), but returned object is the first entry of 'ifname' */
struct ifaddrs *
rtev_getifaddrs_by_ifname(const char *ifname)
{
	if (rtev_update_ifa_cache(singleton_impl) != 0)
		return NULL;

	return ifa_rb_ifacenam_find(ifname);
}

struct ifaddrs *
rtev_getifaddrs_by_sockaddr(struct sockaddr const *sa)
{
	if (rtev_update_ifa_cache(singleton_impl) != 0)
		return NULL;

	return ifa_rb_sockaddr_find(sa);
}

/** same as if_nametoindex(3), but fast and no memory allocation. */
unsigned int
rtev_if_nametoindex(const char *ifname)
{
	unsigned int ni;
	struct ifaddrs *ifa;

	/* adapted from lib/libc/net/if_nametoindex.c */
	ni = 0;

	for (ifa = rtev_getifaddrs_by_ifname(ifname); ifa != NULL;
	    ifa = ifa->ifa_next) {
		if (ifa->ifa_addr &&
		    ifa->ifa_addr->sa_family == AF_LINK) {
			if (strcmp(ifa->ifa_name, ifname) == 0) {
				ni = ((struct sockaddr_dl*)ifa->ifa_addr)
				    ->sdl_index;
				break;
			} else
				break;
		}
	}

	if (!ni)
		errno = ENXIO;

	return ni;
}

/** API have write pending packet internaly */
int
rtev_has_write_pending(void)
{
	if (bytebuffer_position(rtev_sndbuf) > 0)
		return 1;
	return 0;
}

/** finalize this library. */
void
rtev_fini(void)
{
	if (singleton_impl != NULL)
		singleton_impl->impl_fini(singleton_impl->impl);
	singleton_impl = NULL;

	if (rtev_cached_ifa != NULL)
		freeifaddrs(rtev_cached_ifa);
	rtev_cached_ifa = NULL;
	ifa_rbentry_fini();

	if (rtev_sndbuf != NULL) {
		bytebuffer_unwrap(rtev_sndbuf);
		bytebuffer_destroy(rtev_sndbuf);
	}
	rtev_sndbuf = NULL;
}

/* protected virtual */
int
rtev_base_init(rtev_impl *impl, int flags)
{
	ifa_rbentry_init();

	if (rtev_sndbuf == NULL) {
		if ((rtev_sndbuf = bytebuffer_wrap(rtev_buffer_space,
		    sizeof(rtev_buffer_space))) == NULL)
			goto fail;
		bytebuffer_clear(rtev_sndbuf);
	}
	impl->base_on_rtevent = rtev_base_on_rtevent;
	impl->base_on_write = rtev_base_on_write;
	impl->base_flags = flags;
	singleton_impl = impl;

	return 0;
fail:
	rtev_fini();

	return 1;
}

static int
rtev_base_on_rtevent(rtev_impl *impl)
{

	RTEV_DBG((LOG_DEBUG, "%s", __func__));

	rtev_event_serial++;

	if ((impl->base_flags & RTEV_UPDATE_IFA_ON_DEMAND) == 0)
		return rtev_update_ifa_cache_do(impl);

	return 0;
}

static inline int
rtev_update_ifa_cache(rtev_impl *impl)
{
	if (rtev_event_serial == rtev_ifa_cache_serial && 
	    rtev_cached_ifa != NULL)
		return 0;

	return rtev_update_ifa_cache_do(impl);
}

static int
rtev_update_ifa_cache_do(rtev_impl *impl)
{
	const char *ifname;
	struct ifaddrs *ifa;
	struct ifaddrs *ifa0;

	RTEV_DBG((LOG_DEBUG, "%s", __func__));

	ifa0 = NULL;
	rtev_ifa_cache_serial = rtev_event_serial;
	if (getifaddrs(&ifa0) != 0)
		return 1;
	if (rtev_cached_ifa != NULL) {
		ifa_rbentry_fini();
		freeifaddrs(rtev_cached_ifa);
		rtev_cached_ifa = NULL;
	}

	for (ifa = ifa0, ifname = NULL; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifname == NULL || strcmp(ifa->ifa_name, ifname)) {
			ifname = ifa->ifa_name;
			if (ifa_rb_ifacenam_find(ifname) == NULL) {
				if (ifa_rb_ifacenam_insert(ifa) != 0)
					goto on_error;
			}
		}
		if (ifa->ifa_addr != NULL &&
		    ifa_rb_sockaddr_find(ifa->ifa_addr) == NULL) {
			if (ifa_rb_sockaddr_insert(ifa) != 0)
				goto on_error;
		}
	}
	rtev_cached_ifa = ifa0;

	return 0;

on_error:
	if (ifa0)
		freeifaddrs(ifa0);
	rtev_cached_ifa = NULL;
	ifa_rbentry_fini();

	return 1;
}

static int
rtev_base_on_write(rtev_impl *impl, int rtsock, int npackets)
{
	int i, rval;
	struct rt_msghdr *rtm;

	rval = 0;
	bytebuffer_flip(rtev_sndbuf);
	for (i = 0; i < npackets && bytebuffer_remaining(rtev_sndbuf) > 0; i++){
		rtm = bytebuffer_pointer(rtev_sndbuf);
		if (send(rtsock, rtm, rtm->rtm_msglen, 0) <= 0 && 
		    !(rtm->rtm_type == RTM_DELETE && errno == ESRCH) &&
		    !(rtm->rtm_type == RTM_ADD    && errno == EEXIST)) {
			rval = 1;
		}
		bytebuffer_get(rtev_sndbuf, BYTEBUFFER_GET_DIRECT,
		    rtm->rtm_msglen);
	}
	bytebuffer_compact(rtev_sndbuf);

	return rval;
}

/*
 * Red-black trees for interface name and interface address lookups.
 */
#include <sys/tree.h>	/* BSD sys/tree.h */

struct ifa_rbentry {
	struct ifaddrs *ifa;
	RB_ENTRY(ifa_rbentry) rbe;
};

static inline int ifacenam_compar(struct ifa_rbentry *, struct ifa_rbentry *);
static RB_HEAD(ifa_rb_ifacenam, ifa_rbentry) ifa_rb_ifacenam;
RB_PROTOTYPE(ifa_rb_ifacenam, ifa_rbentry, rbe, ifacenam_compar);
RB_GENERATE(ifa_rb_ifacenam, ifa_rbentry, rbe, ifacenam_compar);

static inline int sockaddr_compar(struct ifa_rbentry *, struct ifa_rbentry *);
static RB_HEAD(ifa_rb_sockaddr, ifa_rbentry) ifa_rb_sockaddr;
RB_PROTOTYPE(ifa_rb_sockaddr, ifa_rbentry, rbe, sockaddr_compar);
RB_GENERATE(ifa_rb_sockaddr, ifa_rbentry, rbe, sockaddr_compar);

static void
ifa_rbentry_init(void)
{
	RB_INIT(&ifa_rb_ifacenam);
	RB_INIT(&ifa_rb_sockaddr);
}

static void
ifa_rbentry_fini(void)
{
	struct ifa_rbentry *e, *n;
	
	for (e = RB_MIN(ifa_rb_ifacenam, &ifa_rb_ifacenam); e; e = n) {
		n = RB_NEXT(ifa_rb_ifacenam, &ifa_rb_ifacenam, e);
		RB_REMOVE(ifa_rb_ifacenam, &ifa_rb_ifacenam, e);
		free(e);
	}
	for (e = RB_MIN(ifa_rb_sockaddr, &ifa_rb_sockaddr); e; e = n) {
		n = RB_NEXT(ifa_rb_sockaddr, &ifa_rb_sockaddr, e);
		RB_REMOVE(ifa_rb_sockaddr, &ifa_rb_sockaddr, e);
		free(e);
	}
}

static inline int
ifa_rb_ifacenam_insert(struct ifaddrs *ifa)
{
	struct ifa_rbentry *e;

	if ((e = malloc(sizeof(struct ifa_rbentry))) == NULL)
		return -1;

	e->ifa = ifa;
	RB_INSERT(ifa_rb_ifacenam, &ifa_rb_ifacenam, e);

	return 0;
}

static inline int
ifa_rb_sockaddr_insert(struct ifaddrs *ifa)
{
	struct ifa_rbentry *e;

	if ((e = malloc(sizeof(struct ifa_rbentry))) == NULL)
		return -1;

	e->ifa = ifa;
	RB_INSERT(ifa_rb_sockaddr, &ifa_rb_sockaddr, e);

	return 0;
}

static inline struct ifaddrs *
ifa_rb_ifacenam_find(const char *ifname)
{
	struct ifa_rbentry *e, e0;
	struct ifaddrs ifa;

	e = &e0;
	e->ifa = &ifa;
	e->ifa->ifa_name = (char *)ifname;

	e = RB_FIND(ifa_rb_ifacenam, &ifa_rb_ifacenam, e);
	if (e == NULL)
		return NULL;

	return e->ifa;
}

static inline struct ifaddrs *
ifa_rb_sockaddr_find(struct sockaddr const *sa)
{
	struct ifa_rbentry *e, e0;
	struct ifaddrs ifa;

	e = &e0;
	e->ifa = &ifa;
	e->ifa->ifa_addr = (struct sockaddr *)sa;

	e = RB_FIND(ifa_rb_sockaddr, &ifa_rb_sockaddr, e);
	if (e == NULL)
		return NULL;

	return e->ifa;
}

static inline int
ifacenam_compar(struct ifa_rbentry *a, struct ifa_rbentry *b)
{
	return strcmp(a->ifa->ifa_name, b->ifa->ifa_name);
}

static inline int
sockaddr_compar(struct ifa_rbentry *a, struct ifa_rbentry *b)
{
	int cmp;

	cmp = b->ifa->ifa_addr->sa_family - a->ifa->ifa_addr->sa_family;
	if (cmp != 0)
		return cmp;
	return memcmp(a->ifa->ifa_addr->sa_data, b->ifa->ifa_addr->sa_data,
	    MIN(a->ifa->ifa_addr->sa_len, b->ifa->ifa_addr->sa_len) -
		offsetof(struct sockaddr, sa_data));
}
