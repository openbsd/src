/*	$OpenBSD: qop_red.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_red.c,v 1.5 2001/08/16 10:39:14 kjc Exp $	*/
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
#include <altq/altq_red.h>
#include "altq_qop.h"
#include "qop_red.h"

static int red_attach(struct ifinfo *);
static int red_detach(struct ifinfo *);
static int red_enable(struct ifinfo *);
static int red_disable(struct ifinfo *);

#define RED_DEVICE	"/dev/altq/red"

static int red_fd = -1;
static int red_refcount = 0;

static struct qdisc_ops red_qdisc = {
	ALTQT_RED,
	"red",
	red_attach,
	red_detach,
	NULL,			/* clear */
	red_enable,
	red_disable,
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
red_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	weight = 0;		/* 0: use default */
	int	inv_pmax = 0;		/* 0: use default */
	int	th_min = 0;		/* 0: use default */
	int	th_max = 0;		/* 0: use default */
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
		} else if (EQUAL(*argv, "weight")) {
			argc--; argv++;
			if (argc > 0)
				weight = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "qlimit")) {
			argc--; argv++;
			if (argc > 0)
				qlimit = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "thmin")) {
			argc--; argv++;
			if (argc > 0)
				th_min = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "thmax")) {
			argc--; argv++;
			if (argc > 0)
				th_max = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "invpmax")) {
			argc--; argv++;
			if (argc > 0)
				inv_pmax = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "red")) {
			/* just skip */
		} else if (EQUAL(*argv, "ecn")) {
			flags |= REDF_ECN;
		} else if (EQUAL(*argv, "flowvalve")) {
			flags |= REDF_FLOWVALVE;
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	pkttime = packet_size * 8 * 1000 / (bandwidth / 1000);
	if (weight != 0) {
		/* check if weight is power of 2 */
		int i, w;
		
		w = weight;
		for (i = 0; w > 1; i++)
			w = w >> 1;
		w = 1 << i;
		if (weight != w) {
			LOG(LOG_ERR, 0, "weight %d: should be power of 2",
			    weight);
			return (0);
		}
	}

	if (qcmd_red_add_if(ifname, bandwidth, weight, inv_pmax,
			    th_min, th_max, qlimit, pkttime, flags) != 0)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_red_add_if(const char *ifname, u_int bandwidth, int weight,
		int inv_pmax, int th_min, int th_max, int qlimit,
		int pkttime, int flags)
{
	int error;
	
	error = qop_red_add_if(NULL, ifname, bandwidth, weight, inv_pmax,
			       th_min, th_max, qlimit, pkttime, flags);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add red on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

/*
 * qop api
 */
int 
qop_red_add_if(struct ifinfo **rp, const char *ifname,
	       u_int bandwidth, int weight, int inv_pmax, int th_min,
	       int th_max, int qlimit, int pkttime, int flags)
{
	struct ifinfo *ifinfo = NULL;
	struct red_ifinfo *red_ifinfo;
	int error;

	if ((red_ifinfo = calloc(1, sizeof(*red_ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	red_ifinfo->weight   = weight;
	red_ifinfo->inv_pmax = inv_pmax;
	red_ifinfo->th_min   = th_min;
	red_ifinfo->th_max   = th_max;
	red_ifinfo->qlimit   = qlimit;
	red_ifinfo->pkttime  = pkttime;
	red_ifinfo->flags    = flags;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &red_qdisc, red_ifinfo);
	if (error != 0) {
		free(red_ifinfo);
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
red_attach(struct ifinfo *ifinfo)
{
	struct red_interface iface;
	struct red_ifinfo *red_ifinfo;
	struct red_conf conf;

	if (red_fd < 0 &&
	    (red_fd = open(RED_DEVICE, O_RDWR)) < 0 &&
	    (red_fd = open_module(RED_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "RED open");
		return (QOPERR_SYSCALL);
	}

	red_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.red_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(red_fd, RED_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	/* set red parameters */
	red_ifinfo = (struct red_ifinfo *)ifinfo->private;
	memset(&conf, 0, sizeof(conf));
	strncpy(conf.iface.red_ifname, ifinfo->ifname, IFNAMSIZ);
	conf.red_weight	  = red_ifinfo->weight;
	conf.red_inv_pmax = red_ifinfo->inv_pmax;
	conf.red_thmin    = red_ifinfo->th_min;
	conf.red_thmax    = red_ifinfo->th_max;
	conf.red_limit    = red_ifinfo->qlimit;
	conf.red_flags    = red_ifinfo->flags;
	if (ioctl(red_fd, RED_CONFIG, &conf) < 0)
		return (QOPERR_SYSCALL);

#if 1
	LOG(LOG_INFO, 0, "red attached to %s", iface.red_ifname);
#endif
	return (0);
}

static int
red_detach(struct ifinfo *ifinfo)
{
	struct red_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.red_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(red_fd, RED_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--red_refcount == 0) {
		close(red_fd);
		red_fd = -1;
	}
	return (0);
}

static int
red_enable(struct ifinfo *ifinfo)
{
	struct red_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.red_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(red_fd, RED_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
red_disable(struct ifinfo *ifinfo)
{
	struct red_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.red_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(red_fd, RED_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}
