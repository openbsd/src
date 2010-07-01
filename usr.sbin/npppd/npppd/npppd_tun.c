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
 * This file provides functions on {@link ::_npppd_tun tunnel device}.
 */
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <event.h>

#include "npppd_local.h"
#include "debugutil.h"
#include "debugmacro.h"

#include "npppd_tun.h"

static void         npppd_tundev_io_event_handler (int, short, void *);

#ifdef	NPPPD_TUN_DEBUG
#define	NPPPD_TUN_ASSERT(cond)	ASSERT(cond)
#else
#define	NPPPD_TUN_ASSERT(cond)
#endif


/**
 * Initialize {@link ::_npppd_tun instance of tunnel device}.
 * @param	minor	device minor number
 */
void
npppd_tundev_init(npppd *_this, int minor)
{
	NPPPD_TUN_ASSERT(_this != NULL);
	NPPPD_TUN_ASSERT(_this->tun_file <= 0);

	_this->tun_file = -1;
	_this->tun_minor = minor;
}

/**
 * Start {@link ::_npppd_tun instance of tunnel device}.
 */
int
npppd_tundev_start(npppd *_this)
{
	int x, sock;
	char buf[MAXPATHLEN];
	struct ifaliasreq ifra;
	struct sockaddr_in *sin0;

	NPPPD_TUN_ASSERT(_this != NULL);

	snprintf(buf, sizeof(buf), "/dev/tun%d", _this->tun_minor);
	if ((_this->tun_file = open(buf, O_RDWR, 0600)) < 0) {
		log_printf(LOG_ERR, "open(%s) failed in %s(): %m",
		    buf, __func__);
		goto fail;
	}

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		log_printf(LOG_ERR, "socket() failed in %s(): %m", __func__);
		goto fail;
	}

	memset(&ifra, 0, sizeof(ifra));
	snprintf(ifra.ifra_name, sizeof(ifra.ifra_name), "tun%d",
	    _this->tun_minor);

	sin0 = (struct sockaddr_in *)&ifra.ifra_addr;
	sin0->sin_addr.s_addr = _this->tun_ip4_addr.s_addr;
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
		log_printf(LOG_ERR, "Cannot assign tun device ip address: %m");
		goto fail;
	}
	close(sock);

	x = 1;
	if (ioctl(_this->tun_file, FIONBIO, &x) != 0) {
		log_printf(LOG_ERR, "ioctl(FIONBIO) failed in %s(): %m",
		    __func__);
		goto fail;
	}
	event_set(&_this->ev_tun, _this->tun_file, EV_READ | EV_PERSIST,
	    npppd_tundev_io_event_handler, _this);
	event_add(&_this->ev_tun, NULL);

	log_printf(LOG_INFO, "Opened /dev/tun%d", _this->tun_minor);

	return 0;
fail:
	if (_this->tun_file >= 0)
		close(_this->tun_file);
	_this->tun_file = -1;

	return -1;
}

/**
 * Stop process on tunnel device.
 */
void
npppd_tundev_stop(npppd *_this)
{
	if (_this->tun_file >= 0) {
		event_del(&_this->ev_tun);
		close(_this->tun_file);
	}
	_this->tun_file = -1;
	log_printf(LOG_NOTICE, "Closed /dev/tun%d", _this->tun_minor);
}

/**
 * Set IP address.
 */
int
npppd_tundev_set_ip_addr(npppd *_this)
{
	return 0;
}

static void
npppd_tundev_io_event_handler(int fd, short evtype, void *data)
{
	int sz;
	npppd *_this;
	uint8_t buffer[8192];

	NPPPD_TUN_ASSERT((evtype & EV_READ) != 0);

	_this = data;
	NPPPD_TUN_ASSERT(_this->tun_file >= 0);

	do {
		sz = read(_this->tun_file, buffer, sizeof(buffer));
		if (sz <= 0) {
			if (sz == 0)
				log_printf(LOG_ERR, "tun%d read failed: %m",
				    _this->tun_minor);
			else if (errno == EAGAIN)
				break;
			else 
				log_printf(LOG_ERR, "tun%d file is closed",
				    _this->tun_minor);
			npppd_tundev_stop(_this);
			return;
		}
		npppd_network_input(_this, buffer, sz);
	} while (1 /* CONSTCOND */);

	return;
}

/**
 * Write to tunnel device.
 */
void
npppd_tundev_write(npppd *_this, uint8_t *pktp, int lpktp)
{
	int err;

	NPPPD_TUN_ASSERT(_this != NULL);
	NPPPD_TUN_ASSERT(_this->tun_file >= 0);

	err = write(_this->tun_file, pktp, lpktp);

	if (err != lpktp)
		log_printf(LOG_ERR, "tun%d write failed in %s(): %m", 
		    _this->tun_minor, __func__);
}
