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
/* $Id: npppd_iface.c,v 1.2 2010/07/01 03:38:17 yasuoka Exp $ */
/**@file
 * The interface of npppd and kernel.
 * This is an implementation to use tun(4).
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <net/if_tun.h>

#include <fcntl.h>

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include <time.h>
#include <event.h>
#include "radish.h"

#include "npppd_defs.h"
#include "npppd_local.h"
#include "npppd_subr.h"
#include "debugutil.h"
#include "npppd_iface.h"

#ifdef USE_NPPPD_PIPEX
#include <net/if.h>
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include <net/pipex.h>
#endif /* USE_NPPPD_PIPEX */

#ifdef	NPPPD_IFACE_DEBUG
#define	NPPPD_IFACE_DBG(x)	npppd_iface_log x
#define	NPPPD_IFACE_ASSERT(cond)				\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	NPPPD_IFACE_ASSERT(cond)			
#define	NPPPD_IFACE_DBG(x)
#endif

static void  npppd_iface_network_input(npppd_iface *, u_char *, int);
static int   npppd_iface_setup_ip(npppd_iface *);
static void  npppd_iface_io_event_handler (int, short, void *);
static int   npppd_iface_log (npppd_iface *, int, const char *, ...)
	__printflike(3,4);

#ifdef USE_NPPPD_PIPEX
static int npppd_iface_pipex_enable(npppd_iface *_this);
static int npppd_iface_pipex_disable(npppd_iface *_this);
#endif /* USE_NPPPD_PIPEX */


/** initialize npppd_iface */
void
npppd_iface_init(npppd_iface *_this, const char *ifname)
{
	NPPPD_IFACE_ASSERT(_this != NULL);
	memset(_this, 0, sizeof(npppd_iface));

	_this->devf = -1;
	strlcpy(_this->ifname, ifname, sizeof(_this->ifname));
}

static int
npppd_iface_setup_ip(npppd_iface *_this)
{
	int sock, if_flags, changed;
	struct in_addr gw, assigned;
	struct sockaddr_in *sin0;
	struct ifreq ifr;
	struct ifaliasreq ifra;
	npppd_ppp *ppp;

	NPPPD_IFACE_ASSERT(_this != NULL);

	sock = -1;
	changed = 0;
	memset(&ifr, 0, sizeof(ifr));

	/* get address which was assigned to interface */
	assigned.s_addr = INADDR_NONE;
	memset(&ifr, 0, sizeof(ifr));
	memset(&ifra, 0, sizeof(ifra));
	strlcpy(ifr.ifr_name, _this->ifname, sizeof(ifr.ifr_name));
	strlcpy(ifra.ifra_name, _this->ifname, sizeof(ifra.ifra_name));
	sin0 = (struct sockaddr_in *)&ifr.ifr_addr;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		npppd_iface_log(_this, LOG_ERR,
		    "socket() failed in %s(): %m", __func__);
		goto fail;
	}
	if (ioctl(sock, SIOCGIFADDR, &ifr) != 0) {
		if (errno != EADDRNOTAVAIL) {
			npppd_iface_log(_this, LOG_ERR,
			    "get ip address failed: %m");
			goto fail;
		}
		assigned.s_addr = 0;
	} else
		assigned = sin0->sin_addr;

	if (assigned.s_addr != _this->ip4addr.s_addr)
		changed = 1;

	memset(&ifr.ifr_ifru, 0, sizeof(ifr.ifr_ifru));
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		npppd_iface_log(_this, LOG_ERR,
		    "ioctl(,SIOCGIFFLAGS) failed: %m");
		goto fail;
	}
	if_flags = ifr.ifr_flags;

	if (_this->set_ip4addr != 0 && changed) {
		do {
			if (ioctl(sock, SIOCDIFADDR, &ifr) != 0) {
				if (errno == EADDRNOTAVAIL)
					break;
				npppd_iface_log(_this, LOG_ERR,
				    "delete ipaddress %s failed: %m",
				    _this->ifname);
				goto fail;
			}
			if (ioctl(sock, SIOCGIFADDR, &ifr) != 0) {
				if (errno == EADDRNOTAVAIL)
					break;
				npppd_iface_log(_this, LOG_ERR,
				    "cannot get ipaddress %s failed: %m",
				    _this->ifname);
				goto fail;
			}
		} while (1);


		/* ifconfig tun1 down */
		ifr.ifr_flags = if_flags & ~(IFF_UP | IFF_BROADCAST);
		if (ioctl(sock, SIOCSIFFLAGS, &ifr) != 0) {
			npppd_iface_log(_this, LOG_ERR,
			    "disabling %s failed: %m", _this->ifname);
			goto fail;
		}

		sin0 = (struct sockaddr_in *)&ifra.ifra_addr;
		sin0->sin_addr.s_addr = _this->ip4addr.s_addr;
		sin0->sin_family = AF_INET;
		sin0->sin_len = sizeof(struct sockaddr_in);

		sin0 = (struct sockaddr_in *)&ifra.ifra_mask;
		sin0->sin_addr.s_addr = 0xffffffffL;
		sin0->sin_family = AF_INET;
		sin0->sin_len = sizeof(struct sockaddr_in);

		sin0 = (struct sockaddr_in *)&ifra.ifra_broadaddr;
		sin0->sin_addr.s_addr = 0;
		sin0->sin_family = AF_INET;
		sin0->sin_len = sizeof(struct sockaddr_in);

		if (ioctl(sock, SIOCAIFADDR, &ifra) != 0 && errno != EEXIST) {
		/*
		 * alias request, so EEXIST?
		 */
			npppd_iface_log(_this, LOG_ERR,
			    "Cannot assign tun device ip address: %m");
			goto fail;
		}
		assigned.s_addr = _this->ip4addr.s_addr;

	}
	_this->ip4addr.s_addr = assigned.s_addr;
	if (npppd_iface_ip_is_ready(_this)) {
		if (changed) {
			/*
			 * If there is a PPP session which was assigned interface IP address, disconnect it.
			 */
			ppp = npppd_get_ppp_by_ip(_this->npppd, _this->ip4addr);
			if (ppp != NULL) {
				npppd_iface_log(_this, LOG_ERR,
				    "Assigning %s, but ppp=%d is using "
				    "the address. Requested the ppp to stop",
				    inet_ntoa(_this->ip4addr), ppp->id);
				ppp_stop(ppp, "Administrative reason");
			}
		}
		/* ifconfig tun1 up */
		ifr.ifr_flags = if_flags | IFF_UP | IFF_MULTICAST;
		if (ioctl(sock, SIOCSIFFLAGS, &ifr) != 0) {
			npppd_iface_log(_this, LOG_ERR,
			    "enabling %s failed: %m", _this->ifname);
			goto fail;
		}
		/*
		 * Add routing entry to communicate from host itself to _this->ip4addr.
		 */
		gw.s_addr = htonl(INADDR_LOOPBACK);
		in_host_route_add(&_this->ip4addr, &gw, LOOPBACK_IFNAME, 0);
	}
	close(sock); sock = -1;

	return 0;
fail:
	if (sock >= 0)
		close(sock);

	return 1;
}

/** set tunnel end address */
int
npppd_iface_reinit(npppd_iface *_this)
{
	int rval;
	struct in_addr backup;
	char buf0[128], buf1[128];

	backup = _this->ip4addr;
	if ((rval = npppd_iface_setup_ip(_this)) != 0)
		return rval;

	if (backup.s_addr != _this->ip4addr.s_addr) {
		npppd_iface_log(_this, LOG_INFO, "Reinited ip4addr %s=>%s",
			(backup.s_addr != INADDR_ANY)
			    ?  inet_ntop(AF_INET, &backup, buf0, sizeof(buf0))
			    : "(not assigned)",
			(_this->ip4addr.s_addr != INADDR_ANY)
			    ?  inet_ntop(AF_INET, &_this->ip4addr, buf1,
				    sizeof(buf1))
			    : "(not assigned)");
	}

	return 0;
}

/** start npppd_iface */
int
npppd_iface_start(npppd_iface *_this)
{
	int x;
	char buf[MAXPATHLEN];

	NPPPD_IFACE_ASSERT(_this != NULL);

	/* open device file */
	snprintf(buf, sizeof(buf), "/dev/%s", _this->ifname);
	if ((_this->devf = open(buf, O_RDWR, 0600)) < 0) {
		npppd_iface_log(_this, LOG_ERR, "open(%s) failed: %m", buf);
		goto fail;
	}

	x = 1;
	if (ioctl(_this->devf, FIONBIO, &x) != 0) {
		npppd_iface_log(_this, LOG_ERR,
		    "ioctl(FIONBIO) failed in %s(): %m", __func__);
		goto fail;
	}

	x = IFF_BROADCAST;
	if (ioctl(_this->devf, TUNSIFMODE, &x) != 0) {
		npppd_iface_log(_this, LOG_ERR,
		    "ioctl(TUNSIFMODE=IFF_BROADCAST) failed in %s(): %m",
			__func__);
		goto fail;
	}

	event_set(&_this->ev, _this->devf, EV_READ | EV_PERSIST,
	    npppd_iface_io_event_handler, _this);
	event_add(&_this->ev, NULL);

	if (npppd_iface_setup_ip(_this) != 0)
		goto fail;

#ifdef USE_NPPPD_PIPEX
	if (npppd_iface_pipex_enable(_this) != 0) {
		log_printf(LOG_WARNING,
		    "npppd_iface_pipex_enable() failed: %m");
	}
#endif /* USE_NPPPD_PIPEX */

	npppd_iface_log(_this, LOG_INFO, "Started ip4addr=%s",
		(npppd_iface_ip_is_ready(_this))?
		    inet_ntop(AF_INET, &_this->ip4addr, buf, sizeof(buf))
		    : "(not assigned)");

	return 0;
fail:
	if (_this->devf >= 0) {
		event_del(&_this->ev);
		close(_this->devf);
	}
	_this->devf = -1;

	return -1;
}

/** stop to use npppd_iface */
void
npppd_iface_stop(npppd_iface *_this)
{
	NPPPD_IFACE_ASSERT(_this != NULL);

	if (_this->devf >= 0) {
#ifdef USE_NPPPD_PIPEX
		if (npppd_iface_pipex_disable(_this) != 0) {
			log_printf(LOG_CRIT,
			    "npppd_iface_pipex_disable() failed: %m");
		}
#endif /* USE_NPPPD_PIPEX */

		event_del(&_this->ev);
		close(_this->devf);
		npppd_iface_log(_this, LOG_INFO, "Stopped");
	}
	_this->devf = -1;
	_this->initialized = 0;
	event_del(&_this->ev);
}

/** finalize npppd_iface */
void
npppd_iface_fini(npppd_iface *_this)
{
	NPPPD_IFACE_ASSERT(_this != NULL);

	npppd_iface_stop(_this);
}


/***********************************************************************
 * PIPEX related functions
 ***********************************************************************/
#ifdef USE_NPPPD_PIPEX

/** enable PIPEX on PPPAC interface */
int
npppd_iface_pipex_enable(npppd_iface *_this)
{
	int enable = 1;

	return ioctl(_this->devf, PIPEXSMODE, &enable);
}

/** disable PIPEX on PPPAC interface */
int
npppd_iface_pipex_disable(npppd_iface *_this)
{
	int disable = 0;

	return ioctl(_this->devf, PIPEXSMODE, &disable);
}

#endif /* USE_NPPPD_PIPEX */


/***********************************************************************
 * I/O related functions
 ***********************************************************************/
/** I/O event handler */
static void
npppd_iface_io_event_handler(int fd, short evtype, void *data)
{
	int sz;
	u_char buffer[8192];
	npppd_iface *_this;

	NPPPD_IFACE_ASSERT((evtype & EV_READ) != 0);

	_this = data;
	NPPPD_IFACE_ASSERT(_this->devf >= 0);
	do {
		sz = read(_this->devf, buffer, sizeof(buffer));
		if (sz <= 0) {
			if (sz == 0)
				npppd_iface_log(_this, LOG_ERR,
				    "file is closed");
			else if (errno == EAGAIN)
				break;
			else 
				npppd_iface_log(_this, LOG_ERR,
				    "read failed: %m");
			npppd_iface_stop(_this);
			return;
		}
		npppd_iface_network_input(_this, buffer, sz);

	} while (1 /* CONSTCOND */);

	return;
}

/** structure of argument of npppd_iface_network_input_delegate */
struct npppd_iface_network_input_arg{
	npppd_iface *_this;
	u_char *pktp;
	int lpktp;
};

/** callback function which works for each PPP session */
static int 
npppd_iface_network_input_delegate(struct radish *radish, void *args0)
{
	npppd_ppp *ppp;
	struct sockaddr_npppd *snp;
	struct npppd_iface_network_input_arg *args;
	
	snp = radish->rd_rtent;

	if (snp->snp_type == SNP_PPP) {
		args = args0;
		ppp = snp->snp_data_ptr;
		if (ppp_iface(ppp) != args->_this)
			return 0;
#ifdef	USE_NPPPD_MPPE
		if (MPPE_READY(ppp)) {
			/* output via MPPE if MPPE started */
			mppe_pkt_output(&ppp->mppe, PPP_PROTO_IP, args->pktp,
			    args->lpktp);
		} else if (MPPE_REQUIRED(ppp)) {
			/* in case MPPE not started but MPPE is mandatory, */
			/* it is not necessary to log because of multicast. */
			return 0;
		}
#endif
		ppp_output(ppp, PPP_PROTO_IP, 0, 0, args->pktp, args->lpktp);
	}

	return 0;
}

static void
npppd_iface_network_input_ipv4(npppd_iface *_this, u_char *pktp, int lpktp)
{
	struct ip *iphdr;
	npppd *_npppd;
	npppd_ppp *ppp;
	struct npppd_iface_network_input_arg input_arg;

	NPPPD_IFACE_ASSERT(_this != NULL);
	NPPPD_IFACE_ASSERT(pktp != NULL);

	iphdr = (struct ip *)pktp;
	_npppd = _this->npppd;

	if (lpktp < sizeof(iphdr)) {
		npppd_iface_log(_this, LOG_ERR, "Received short packet.");
		return;
	}
	if (IN_MULTICAST(ntohl(iphdr->ip_dst.s_addr))) {
		NPPPD_IFACE_ASSERT(((npppd *)(_this->npppd))->rd != NULL);
		input_arg._this = _this;
		input_arg.pktp = pktp;
		input_arg.lpktp = lpktp;
		/* delegate */
		rd_walktree(((npppd *)(_this->npppd))->rd,
		    npppd_iface_network_input_delegate, &input_arg);
		return;
	}
	ppp = npppd_get_ppp_by_ip(_npppd, iphdr->ip_dst);
	if (ppp == NULL) {
#ifdef NPPPD_DEBUG
		log_printf(LOG_INFO, "%s received a packet to unknown "
		    "%s.", _this->ifname, inet_ntoa(iphdr->ip_dst));
		show_hd(debug_get_debugfp(), pktp, lpktp);
#endif
		return;
	}
#ifndef NO_ADJUST_MSS
	if (ppp->adjust_mss) {
		adjust_tcp_mss(pktp, lpktp, MRU_IPMTU(ppp->peer_mru));
	}
#endif
	if (ppp->timeout_sec > 0 && !ip_is_idle_packet(iphdr, lpktp))
		ppp_reset_idle_timeout(ppp);

#ifdef	USE_NPPPD_MPPE
	if (MPPE_READY(ppp)) {
		/* output via MPPE if MPPE started */
		mppe_pkt_output(&ppp->mppe, PPP_PROTO_IP, pktp, lpktp);
		return;
	} else if (MPPE_REQUIRED(ppp)) {
		/* in case MPPE not started but MPPE is mandatory */
		ppp_log(ppp, LOG_WARNING, "A packet received from network, "
		    "but MPPE is not started.");
		return;
	}
#endif
	ppp_output(ppp, PPP_PROTO_IP, 0, 0, pktp, lpktp);
}

/**
 * This function is called when an input packet come from network(tun).
 * Currently, it assumes that it input IPv4 packet.
 */
static void
npppd_iface_network_input(npppd_iface *_this, u_char *pktp, int lpktp)
{
	uint32_t af;

	if (lpktp < sizeof(uint32_t)) {
		npppd_iface_log(_this, LOG_ERR, "Received short packet.");
		return;
	}
	GETLONG(af, pktp);
	lpktp -= sizeof(uint32_t);

	switch (af) {
	case AF_INET:
		npppd_iface_network_input_ipv4(_this, pktp, lpktp);
		break;

	default:
		NPPPD_IFACE_ASSERT(0);
		break;

	}
}

/** write to tunnel device */
void
npppd_iface_write(npppd_iface *_this, int proto, u_char *pktp, int lpktp)
{
	int err;
	uint32_t th;
	struct iovec iov[2];

	NPPPD_IFACE_ASSERT(_this != NULL);
	NPPPD_IFACE_ASSERT(_this->devf >= 0);

	th = htonl(proto);

	iov[0].iov_base = &th;
	iov[0].iov_len = sizeof(th);
	iov[1].iov_base = pktp;
	iov[1].iov_len = lpktp;

	err = writev(_this->devf, iov, countof(iov));

	if (err != lpktp + sizeof(th))
		npppd_iface_log(_this, LOG_ERR, "write failed: %m");
}

/***********************************************************************
 * misc functions
 ***********************************************************************/
/** Log it which starts the label based on this instance. */
static int
npppd_iface_log(npppd_iface *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	NPPPD_IFACE_ASSERT(_this != NULL);

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "%s %s", _this->ifname, fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}
