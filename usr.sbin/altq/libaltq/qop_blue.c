/*	$OpenBSD: qop_blue.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_blue.c,v 1.5 2001/08/16 10:39:13 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>

#include <altq/altq.h>
#include <altq/altq_blue.h>
#include "altq_qop.h"
#include "qop_blue.h"

static int blue_attach(struct ifinfo *);
static int blue_detach(struct ifinfo *);
static int blue_enable(struct ifinfo *);
static int blue_disable(struct ifinfo *);

#define BLUE_DEVICE	"/dev/altq/blue"

static int blue_fd = -1;
static int blue_refcount = 0;

static struct qdisc_ops blue_qdisc = {
	ALTQT_BLUE,
	"blue",
	blue_attach,
	blue_detach,
	NULL,			/* clear */
	blue_enable,
	blue_disable,
	NULL,			/* add class */
	NULL,			/* modify class */
	NULL,			/* delete class */
	NULL,			/* add filter */
	NULL			/* delete filter */
};

/*
 * parser interface
 */
#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

int
blue_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	max_pmark = 4000;
	int	hold_time = 1000;
	int	qlimit = 60;
	int	pkttime = 0;
	int	flags = 0;
	int	packet_size = 1000;

	/*
	 * process options
	 */
	while (argc > 0) {
		if (EQUAL(*argv, "bandwidth")) {
			argc--; argv++;
			if (argc > 0)
				bandwidth = atobps(*argv);
		} else if (EQUAL(*argv, "tbrsize")) {
			argc--; argv++;
			if (argc > 0)
				tbrsize = atobytes(*argv);
		} else if (EQUAL(*argv, "packetsize")) {
			argc--; argv++;
			if (argc > 0)
				packet_size = atobytes(*argv);
		} else if (EQUAL(*argv, "qlimit")) {
			argc--; argv++;
			if (argc > 0)
				qlimit = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "maxpmark")) {
			argc--; argv++;
			if (argc > 0)
				max_pmark = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "holdtime")) {
			argc--; argv++;
			if (argc > 0)
				hold_time = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "blue")) {
			/* just skip */
		} else if (EQUAL(*argv, "ecn")) {
			flags |= BLUEF_ECN;
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	pkttime = packet_size * 8 * 1000 / (bandwidth / 1000);

	if (qcmd_blue_add_if(ifname, bandwidth, max_pmark, hold_time,
			     qlimit, pkttime, flags) != 0)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_blue_add_if(const char *ifname, u_int bandwidth, int max_pmark,
		 int hold_time, int qlimit, int pkttime, int flags)
{
	int error;
	
	error = qop_blue_add_if(NULL, ifname, bandwidth, max_pmark, hold_time,
				qlimit, pkttime, flags);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add blue on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

/*
 * qop api
 */
int 
qop_blue_add_if(struct ifinfo **rp, const char *ifname, u_int bandwidth,
		int max_pmark, int hold_time, int qlimit,
		int pkttime, int flags)
{
	struct ifinfo *ifinfo = NULL;
	struct blue_ifinfo *blue_ifinfo;
	int error;

	if ((blue_ifinfo = calloc(1, sizeof(*blue_ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	blue_ifinfo->max_pmark = max_pmark;
	blue_ifinfo->hold_time = hold_time;
	blue_ifinfo->qlimit    = qlimit;
	blue_ifinfo->pkttime   = pkttime;
	blue_ifinfo->flags     = flags;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &blue_qdisc, blue_ifinfo);
	if (error != 0) {
		free(blue_ifinfo);
		return (error);
	}

	if (rp != NULL)
		*rp = ifinfo;
	return (0);
}

/*
 *  system call interfaces for qdisc_ops
 */
static int
blue_attach(struct ifinfo *ifinfo)
{
	struct blue_interface iface;
	struct blue_ifinfo *blue_ifinfo;
	struct blue_conf conf;

	if (blue_fd < 0 &&
	    (blue_fd = open(BLUE_DEVICE, O_RDWR)) < 0 &&
	    (blue_fd = open_module(BLUE_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "BLUE open");
		return (QOPERR_SYSCALL);
	}

	blue_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.blue_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(blue_fd, BLUE_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	/* set blue parameters */
	blue_ifinfo = (struct blue_ifinfo *)ifinfo->private;
	memset(&conf, 0, sizeof(conf));
	strncpy(conf.iface.blue_ifname, ifinfo->ifname, IFNAMSIZ);
	conf.blue_max_pmark = blue_ifinfo->max_pmark;
	conf.blue_hold_time = blue_ifinfo->hold_time;
	conf.blue_limit     = blue_ifinfo->qlimit;
	conf.blue_pkttime   = blue_ifinfo->pkttime;
	conf.blue_flags     = blue_ifinfo->flags;
	if (ioctl(blue_fd, BLUE_CONFIG, &conf) < 0)
		return (QOPERR_SYSCALL);

#if 1
	LOG(LOG_INFO, 0, "blue attached to %s", iface.blue_ifname);
#endif
	return (0);
}

static int
blue_detach(struct ifinfo *ifinfo)
{
	struct blue_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.blue_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(blue_fd, BLUE_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--blue_refcount == 0) {
		close(blue_fd);
		blue_fd = -1;
	}
	return (0);
}

static int
blue_enable(struct ifinfo *ifinfo)
{
	struct blue_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.blue_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(blue_fd, BLUE_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
blue_disable(struct ifinfo *ifinfo)
{
	struct blue_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.blue_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(blue_fd, BLUE_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}
