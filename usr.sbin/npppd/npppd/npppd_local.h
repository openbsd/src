/* $OpenBSD: npppd_local.h,v 1.5 2010/07/02 21:20:57 yasuoka Exp $ */

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
#ifndef	_NPPPD_LOCAL_H
#define	_NPPPD_LOCAL_H	1

#ifndef	NPPPD_BUFSZ
/** buffer size */
#define	NPPPD_BUFSZ			BUFSZ
#endif

#include <sys/param.h>
#include <net/if.h>

#include "npppd_defs.h"

#include "slist.h"
#include "hash.h"
#include "properties.h"

#ifdef	USE_NPPPD_RADIUS
#include <radius+.h>
#include "radius_req.h"
#endif

#ifdef	USE_NPPPD_L2TP
#include "debugutil.h"
#include "bytebuf.h"
#include "l2tp.h"
#endif

#ifdef	USE_NPPPD_PPTP
#include "bytebuf.h"
#include "pptp.h"
#endif
#ifdef	USE_NPPPD_PPPOE
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include "bytebuf.h"
#include "pppoe.h"
#endif
#include "npppd_auth.h"
#include "npppd_iface.h"
#include "npppd.h"

#include "privsep.h"

#ifdef	USE_NPPPD_NPPPD_CTL
typedef struct _npppd_ctl {
	/** event context */
	struct event ev_sock;
	/** socket */
	int sock;
	/** enabled or disabled */
	int enabled;
	/** parent of npppd structure */
	void *npppd;
	/** pathname of socket */
	char pathname[MAXPATHLEN];
	/** maximum length of message */
	int max_msgsz;
} npppd_ctl;
#endif

#include "addr_range.h"
#include "npppd_pool.h"

/** structure of pool */
struct _npppd_pool {
	/** base of npppd structure */
	npppd		*npppd;
	/** name of label */
	char		label[NPPPD_GENERIC_NAME_LEN];
	/** name */
	char		name[NPPPD_GENERIC_NAME_LEN];
	/** size of sockaddr_npppd array */
	int		addrs_size;
	/** pointer indicated to sockaddr_npppd array */
	struct sockaddr_npppd *addrs;
	/** list of addresses dynamically allocated */
	slist 		dyna_addrs;
	int		/** whether initialized or not */
			initialized:1,
			/** whether in use or not */
			running:1;
};

/** structure of IPCP configuration */
typedef struct _npppd_ipcp_config {
	/** name */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** label (to associate with npppd structure) */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** pointer indicated to parent npppd structure */
	npppd	*npppd;
	/**
	 * primary DNS server. INADDR_NONE if not inform peer this.
	 * specified in network byte order.
	 */
	struct in_addr	dns_pri;

	/** secondary DNS server. INADDR_NONE if not inform peer this.
	 * specified in network byte order.
	 */
	struct in_addr	dns_sec;

	/**
	 * primary WINS server. INADDR_NONE if not inform peer this.
	 * specified in network byte order.
	 */
	struct in_addr	nbns_pri;

	/**
	 * secondary WINS server. INADDR_NONE if not inform peer this.
	 * specified in network byte order.
	 */
	struct in_addr	nbns_sec;

	/**
	 * bit flag which specifies the way of IP address assignment.
	 * @see	#NPPPD_IP_ASSIGN_FIXED
	 * @see	#NPPPD_IP_ASSIGN_USER_SELECT
	 * @see	#NPPPD_IP_ASSIGN_RADIUS
	 */
	int 		ip_assign_flags;

	int		/** whether use tunnel end point address as DNS server or not */
			dns_use_tunnel_end:1,
			/** whether initialized or not */
			initialized:1,
			reserved:30;
} npppd_ipcp_config;

/** structure which holds an interface of IPCP configuration and references of pool address */
typedef struct _npppd_iface_binding {
	npppd_ipcp_config	*ipcp;
	slist			pools;
} npppd_iface_binding;

/**
 * npppd
 */
struct _npppd {
	/** event handler */
	struct event ev_sigterm, ev_sigint, ev_sighup, ev_timer;

	/** interface which concentrates PPP  */
	npppd_iface		iface[NPPPD_MAX_IFACE];
	/** reference of interface of IPCP configuration and pool address */
	npppd_iface_binding	iface_bind[NPPPD_MAX_IFACE];

	/** address pool */
	npppd_pool		pool[NPPPD_MAX_POOL];

	/** radish pool which uses to manage allocated address */
	struct radish_head *rd;

	/** IPCP configuration */
	npppd_ipcp_config ipcp_config[NPPPD_MAX_IPCP_CONFIG];

	/** map of username to slist of npppd_ppp */
	hash_table *map_user_ppp;

	/** authentication realms */
	slist realms;

	/** interval time(in seconds) which finalizes authentication realms */
	int auth_finalizer_itvl;

	/** name of configuration file */
	char 	config_file[MAXPATHLEN];

	/** name of pid file */
	char 	pidpath[MAXPATHLEN];

	/** process id */
	pid_t	pid;

#ifdef	USE_NPPPD_L2TP
	/** structure of L2TP daemon */
	l2tpd l2tpd;
#endif
#ifdef	USE_NPPPD_PPTP
	/** structure of PPTP daemon */
	pptpd pptpd;
#endif
#ifdef	USE_NPPPD_PPPOE
	/** structure of PPPOE daemon */
	pppoed pppoed;
#endif
	/** configuration file  */
	struct properties * properties;

	/** user properties file */
	struct properties * users_props;

#ifdef	USE_NPPPD_NPPPD_CTL
	npppd_ctl ctl;
#endif
	/** the time in seconds which process was started.*/
	uint32_t	secs;

	/** delay time in seconds reload configuration */
	int16_t		delayed_reload;
	/** counter of reload configuration */
	int16_t		reloading_count;

	/** serial number of routing event which was completed */
	int		rtev_event_serial;

	/** maximum PPP sessions */
	int		max_session;

	int /** whether finalizing or not */
	    finalizing:1,
	    /** whether finalize completed or not */
	    finalized:1;
};

#ifndef	NPPPD_CONFIG_BUFSIZ
#define	NPPPD_CONFIG_BUFSIZ	65536	// 64K
#endif
#ifndef	NPPPD_KEY_BUFSIZ
#define	NPPPD_KEY_BUFSIZ	512
#endif
#define	ppp_iface(ppp)	(&(ppp)->pppd->iface[(ppp)->ifidx])
#define	ppp_ipcp(ppp)	((ppp)->pppd->iface_bind[(ppp)->ifidx].ipcp)
#define	ppp_pools(ppp)	(&(ppp)->pppd->iface_bind[(ppp)->ifidx].pools)

#define	SIN(sa)		((struct sockaddr_in *)(sa))

#define	TIMER_TICK_RUP(interval)			\
	((((interval) % NPPPD_TIMER_TICK_IVAL) == 0)	\
	    ? (interval)				\
	    : (interval) + NPPPD_TIMER_TICK_IVAL	\
		- ((interval) % NPPPD_TIMER_TICK_IVAL))

#ifdef	USE_NPPPD_NPPPD_CTL
void  npppd_ctl_init (npppd_ctl *, npppd *, const char *);
int   npppd_ctl_start (npppd_ctl *);
void  npppd_ctl_stop (npppd_ctl *);
#endif
#define	sin46_port(x)	(((x)->sa_family == AF_INET6)	\
	? ((struct sockaddr_in6 *)(x))->sin6_port		\
	: ((struct sockaddr_in *)(x))->sin_port)


#endif
