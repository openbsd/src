/* $OpenBSD: npppd_ctl.c,v 1.7 2010/09/24 14:50:30 yasuoka Exp $ */

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
/**@file
 * npppd management.
 * This file provides to open UNIX domain socket which located in
 * /var/run/npppd_ctl and accept commmands from the npppdctl command.
 */
/* $Id: npppd_ctl.c,v 1.7 2010/09/24 14:50:30 yasuoka Exp $ */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "npppd_local.h"
#include "debugutil.h"

#include "pathnames.h"
#include "radish.h"
#include "npppd_ctl.h"

#include "net_utils.h"
#include "privsep.h"
#define	sendto(_s, _msg, _len, _flags, _to, _tolen) \
    priv_sendto((_s), (_msg), (_len), (_flags), (_to), (_tolen))

#ifdef USE_NPPPD_PIPEX
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include <netinet/ip_var.h>
#include <sys/ioctl.h>
#include <net/pipex.h>
#endif

#ifndef	NPPPD_CTL_SOCK_FILE_MODE
#define	NPPPD_CTL_SOCK_FILE_MODE 0660
#endif
#define	MSG_SZ_RESERVED	256

#ifdef	NPPPD_CTL_DEBUG
#define	NPPPD_CTL_DBG(x)	npppd_ctl_log x
#define	NPPPD_CTL_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	NPPPD_CTL_DBG(x)
#define	NPPPD_CTL_ASSERT(cond)
#endif
#include "debugutil.h"

#define NPPPD_CTL_WHO_MSGSZ(n)	\
	    (n) * sizeof(struct npppd_who) + sizeof(struct npppd_who_list)

static void  npppd_ctl_command (npppd_ctl *, u_char *, int, struct sockaddr *);
static void  npppd_ctl_io_event (int, short, void *);
static int   npppd_ctl_log (npppd_ctl *, int, const char *, ...) __printflike(3,4);
static void  npppd_who_init(struct npppd_who *, npppd_ctl *, npppd_ppp *);

#ifdef USE_NPPPD_PIPEX
static int npppd_ppp_get_pipex_stat(struct npppd_who *, npppd_ppp *);
#endif

/** initialize npppd management */
void
npppd_ctl_init(npppd_ctl *_this, npppd *_npppd, const char *pathname)
{
	memset(_this, 0, sizeof(npppd_ctl));
	if (pathname != NULL)
		strlcat(_this->pathname, pathname, sizeof(_this->pathname));
	_this->sock = -1;
	_this->npppd = _npppd;
	_this->max_msgsz  = DEFAULT_NPPPD_CTL_MAX_MSGSZ;
}

/** start npppd management */
int
npppd_ctl_start(npppd_ctl *_this)
{
	int flags, dummy, val;
	struct sockaddr_un sun;

	if ((_this->sock = socket(AF_LOCAL, SOCK_DGRAM, 0)) < 0) {
		log_printf(LOG_ERR, "socket() failed in %s(): %m", __func__);
		goto fail;
	}

	val = _this->max_msgsz;
	if (setsockopt(_this->sock, SOL_SOCKET, SO_SNDBUF, &val, sizeof(val))
	    != 0) {
		if (errno == ENOBUFS)
			log_printf(LOG_ERR,
			    "ctl.max_msgbuf may beyonds kernel limit.  "
			    "setsockopt(,SOL_SOCKET, SO_SNDBUF,%d) "
			    "failed in %s(): %m", val, __func__);
			/*
			 * on NetBSD, need to set value which is less than or equal
			 * to kern.sbmax.
			 */
		else
			log_printf(LOG_ERR,
			    "setsockopt(,SOL_SOCKET, SO_SNDBUF,%d) "
			    "failed in %s(): %m", val, __func__);

		goto fail;
	}
	priv_unlink(_this->pathname);
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, _this->pathname, sizeof(sun.sun_path));

	if (priv_bind(_this->sock, (struct sockaddr *)&sun, sizeof(sun))
	    != 0) {
		log_printf(LOG_ERR, "bind() failed in %s(): %m", __func__);
		goto fail;
	}

	dummy = 0;
	if ((flags = fcntl(_this->sock, F_GETFL, &dummy)) < 0) {
		log_printf(LOG_ERR, "fcntl(,F_GETFL) failed in %s(): %m",
		    __func__);
		goto fail;
	} else if (fcntl(_this->sock, F_SETFL, flags | O_NONBLOCK) < 0) {
		log_printf(LOG_ERR, "fcntl(,F_SETFL,O_NONBLOCK) failed in %s()"
		    ": %m", __func__);
		goto fail;
	}
	chown(_this->pathname, -1, NPPPD_GID);
	chmod(_this->pathname, NPPPD_CTL_SOCK_FILE_MODE);

	event_set(&_this->ev_sock, _this->sock, EV_READ | EV_PERSIST,
	    npppd_ctl_io_event, _this);
	event_add(&_this->ev_sock, NULL);

	log_printf(LOG_INFO, "Listening %s (npppd_ctl)", _this->pathname);

	return 0;
fail:
	if (_this->sock >= 0)
		close(_this->sock);
	_this->sock = -1;

	return -1;
}

/** stop npppd management */
void
npppd_ctl_stop(npppd_ctl *_this)
{
	if (_this->sock >= 0) {
		event_del(&_this->ev_sock);
		close(_this->sock);
		_this->sock = -1;
		log_printf(LOG_INFO, "Shutdown %s (npppd_ctl)",
		    _this->pathname);
	}
}

/** execute management procedure on each command */
static void
npppd_ctl_command(npppd_ctl *_this, u_char *pkt, int pktlen,
    struct sockaddr *peer)
{
	u_char respbuf[BUFSIZ];
	int command;

	if (pktlen < sizeof(int)) {
		npppd_ctl_log(_this, LOG_ERR, "Packet too small.");
		return;
	}
	command = *(int *)pkt;
	switch (command) {
	case NPPPD_CTL_CMD_WHO: {
		int i, c, idx, msgsz;
		npppd *_npppd;
		struct npppd_who_list *l;
		slist users;

		l = NULL;
		_npppd = _this->npppd;
		slist_init(&users);
		if (npppd_get_all_users(_npppd, &users) != 0) {
			npppd_ctl_log(_this, LOG_ERR,
			    "npppd_get_all_users() failed in %s: %m", __func__);
			goto cmd_who_out;
		}
#ifdef NPPPD_CTL_DEBUG
#if 0
		/* for debug, copy the first user 3000 times. */
		if (slist_length(&users) > 0) {
			for (i = 0; i < 32000; i++)
				slist_add(&users, slist_get(&users, 0));
		}
#endif
#endif
		NPPPD_CTL_ASSERT(_this->max_msgsz > 0);
		if ((l = malloc(_this->max_msgsz)) == NULL) {
			npppd_ctl_log(_this, LOG_ERR,
			    "malloc() failed in %s: %m", __func__);
			goto cmd_who_out;
		}

		/* number of entry per chunk */
		c = _this->max_msgsz - sizeof(struct npppd_who_list);
		c /= sizeof(struct npppd_who);

		l->count = slist_length(&users);
		slist_itr_first(&users);
		for (i = 0, idx = 0; slist_itr_has_next(&users); i++) {
			npppd_who_init(&l->entry[idx++], _this,
			    slist_itr_next(&users));
			idx %= c;
			if (idx == 0) {
				/* the last entry this chunk */
				msgsz = offsetof(struct npppd_who_list,
				    entry[c]);
				if (sendto(_this->sock, l, msgsz, 0, peer,
				    peer->sa_len) < 0)
					goto cmd_who_send_error;
			}
		}
		if (i == 0 || idx != 0) {
			msgsz = offsetof(struct npppd_who_list, entry[(i % c)]);
			if (sendto(_this->sock, l, msgsz, 0, peer,
			    peer->sa_len) < 0)
				goto cmd_who_send_error;
		}
cmd_who_out:
		slist_fini(&users);
		if (l != NULL)
			free(l);
		break;
cmd_who_send_error:
	/*
	 * FIXME: we should wait until the buffer is available.
	 */
		NPPPD_CTL_DBG((_this, LOG_DEBUG, "sendto() failed in %s: %m",
		    __func__));
		if (errno == ENOBUFS || errno == EMSGSIZE || errno == EINVAL) {
			npppd_ctl_log(_this, LOG_INFO,
			    "'who' is requested, but "
			    "the buffer is not enough.");
		} else {
			npppd_ctl_log(_this, LOG_ERR,
			    "sendto() failed in %s: %m",
			    __func__);
		}
		slist_fini(&users);
		if (l != NULL)
			free(l);
		break;
	    }
	case NPPPD_CTL_CMD_DISCONNECT_USER: {
		int i, stopped;
		npppd *_npppd;
		struct npppd_disconnect_user_req *req;
		npppd_ppp *ppp;
		slist *ppplist;

		stopped = 0;
		_npppd = _this->npppd;
		req = (struct npppd_disconnect_user_req *)pkt;

		if (sizeof(struct npppd_disconnect_user_req) > pktlen) {
			npppd_ctl_log(_this, LOG_ERR,
			    "'disconnect by user' is requested, "
			    " but the request has invalid data length"
			    "(%d:%d)", pktlen, (int)sizeof(req->username));
			break;
		}
		for (i = 0; i < sizeof(req->username); i++) {
			if (req->username[i] == '\0')
				break;
		}
		if (i >= sizeof(req->username)) {
			npppd_ctl_log(_this, LOG_ERR,
			    "'disconnect by user' is requested, "
			    " but the request has invalid user name");
			break;
		}

		if ((ppplist = npppd_get_ppp_by_user(_npppd, req->username))
		    == NULL) {
			npppd_ctl_log(_this, LOG_INFO,
			    "npppd_get_ppp_by_user() could't find user \"%s\" in %s: %m",
			    req->username, __func__);
			goto user_end;
			break;
		}
		slist_itr_first(ppplist);
		while (slist_itr_has_next(ppplist)) {
			ppp = slist_itr_next(ppplist);
			if (ppp == NULL)
				continue;

			ppp_stop(ppp, NULL);
			stopped++;
		}
user_end:

		npppd_ctl_log(_this, LOG_NOTICE,
		    "'disconnect by user' is requested, "
		    "stopped %d connections.", stopped);
		snprintf(respbuf, sizeof(respbuf),
		    "Disconnected %d ppp connections", stopped);

		if (sendto(_this->sock, respbuf, strlen(respbuf), 0, peer,
		    peer->sa_len) < 0) {
			npppd_ctl_log(_this, LOG_ERR,
			    "sendto() failed in %s: %m", __func__);
		}
		break;
	    }
	case NPPPD_CTL_CMD_RESET_ROUTING_TABLE:
	    {
		if (npppd_reset_routing_table(_this->npppd, 0) == 0)
			strlcpy(respbuf, "Reset the routing table successfully.",
			    sizeof(respbuf));
		else
			snprintf(respbuf, sizeof(respbuf),
			    "Failed to reset the routing table.:%s",
			    strerror(errno));

		if (sendto(_this->sock, respbuf, strlen(respbuf), 0, peer,
		    peer->sa_len) < 0) {
			npppd_ctl_log(_this, LOG_ERR,
			    "sendto() failed in %s: %m", __func__);

		}
		break;
	    }
	default:
	    npppd_ctl_log(_this, LOG_ERR,
		"Received unknown command %04x", command);
	}
fail:
	return;
}

static void
npppd_who_init(struct npppd_who *_this, npppd_ctl *ctl, npppd_ppp *ppp)
{
	struct timespec curr_time;
	npppd_auth_base *realm = ppp->realm;
	npppd_iface *iface = ppp_iface(ppp);

	strlcpy(_this->name, ppp->username, sizeof(_this->name));
	_this->time = ppp->start_time;
	if (clock_gettime(CLOCK_MONOTONIC, &curr_time) < 0) {
		NPPPD_CTL_ASSERT(0);
	}
	_this->duration_sec = curr_time.tv_sec - ppp->start_monotime;
	strlcpy(_this->phy_label, ppp->phy_label, sizeof(_this->phy_label));
	if (((struct sockaddr *)&ppp->phy_info)->sa_len > 0) {
		memcpy(&_this->phy_info, &ppp->phy_info,
		    ((struct sockaddr *)&ppp->phy_info)->sa_len);
	}
	strlcpy(_this->ifname, iface->ifname, sizeof(_this->ifname));
	strlcpy(_this->rlmname, npppd_auth_get_name(realm),
	    sizeof(_this->rlmname));

	_this->assign_ip4 = ppp->ppp_framed_ip_address;
	_this->ipackets = ppp->ipackets;
	_this->opackets = ppp->opackets;
	_this->ierrors = ppp->ierrors;
	_this->oerrors = ppp->oerrors;
	_this->ibytes = ppp->ibytes;
	_this->obytes = ppp->obytes;
	_this->id = ppp->id;

#ifdef USE_NPPPD_PIPEX
	if (ppp->pipex_enabled != 0) {
		if (npppd_ppp_get_pipex_stat(_this, ppp) != 0) {
			npppd_ctl_log(ctl, LOG_NOTICE,
			    "npppd_ppp_get_pipex_stat() failed in %s: %m",
			    __func__);
		}
	}
#endif
}

#ifdef USE_NPPPD_PIPEX
static int
npppd_ppp_get_pipex_stat(struct npppd_who *_this, npppd_ppp *ppp)
{
	npppd_iface *iface = ppp_iface(ppp);
	struct pipex_session_stat_req req;
#ifdef USE_NPPPD_PPPOE
	pppoe_session *pppoe;
#endif
#ifdef USE_NPPPD_PPTP
	pptp_call *call;
#endif
#ifdef USE_NPPPD_L2TP
	l2tp_call *l2tp;
#endif

	if (ppp->pipex_enabled == 0)
		return 0;

	memset(&req, 0, sizeof(req));
	switch(ppp->tunnel_type) {
#ifdef	USE_NPPPD_PPPOE
	case PPP_TUNNEL_PPPOE:
		pppoe = (pppoe_session *)ppp->phy_context;

		/* PPPOE specific information */
		req.psr_protocol = PIPEX_PROTO_PPPOE;
		req.psr_session_id = pppoe->session_id;
		break;
#endif
#ifdef	USE_NPPPD_PPTP
	case PPP_TUNNEL_PPTP:
		call = (pptp_call *)ppp->phy_context;

		/* PPTP specific information */
		req.psr_session_id = call->id;
		req.psr_protocol = PIPEX_PROTO_PPTP;
		break;
#endif
#ifdef USE_NPPPD_L2TP
	case PPP_TUNNEL_L2TP:
		l2tp = (l2tp_call *)ppp->phy_context;

		/* L2TP specific information */
		req.psr_session_id = l2tp->session_id;
		req.psr_protocol = PIPEX_PROTO_L2TP;
		break;
#endif
	default:
		NPPPD_CTL_ASSERT(0);
		errno = EINVAL;
		return 1;
	}

	/* update statistics in kernel */
	if (ioctl(iface->devf, PIPEXGSTAT, &req) != 0)
		return 1;

	_this->ipackets += req.psr_stat.ipackets;
	_this->opackets += req.psr_stat.opackets;
	_this->ierrors += req.psr_stat.ierrors;
	_this->oerrors += req.psr_stat.oerrors;
	_this->ibytes += req.psr_stat.ibytes;
	_this->obytes += req.psr_stat.obytes;

	return 0;
}
#endif

/** IO event handler */
static void
npppd_ctl_io_event(int fd, short evmask, void *ctx)
{
	int sz;
	u_char buf[BUFSIZ];
	npppd_ctl *_this;
	struct sockaddr_storage ss;
	socklen_t sslen;

	_this = ctx;
	if ((evmask & EV_READ) != 0) {
		sslen = sizeof(ss);
		if ((sz = recvfrom(_this->sock, buf, sizeof(buf), 0,
		    (struct sockaddr *)&ss, &sslen)) < 0) {
			npppd_ctl_log(_this, LOG_ERR,
			    "recvfrom() failed in %s(): %m", __func__);
			npppd_ctl_stop(_this);

			return;
		}
		npppd_ctl_command(_this, buf, sz, (struct sockaddr *)&ss);
	}
	return;
}

/** Record log that begins the label based this instance. */
static int
npppd_ctl_log(npppd_ctl *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	NPPPD_CTL_ASSERT(_this != NULL);

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "npppdctl: %s", fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}
