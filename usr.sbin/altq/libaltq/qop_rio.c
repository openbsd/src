/*	$OpenBSD: qop_rio.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_rio.c,v 1.5 2001/08/16 10:39:15 kjc Exp $	*/
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
#include <altq/altq_rio.h>
#include "altq_qop.h"
#include "qop_rio.h"

static int rio_attach(struct ifinfo *);
static int rio_detach(struct ifinfo *);
static int rio_enable(struct ifinfo *);
static int rio_disable(struct ifinfo *);

#define RIO_DEVICE	"/dev/altq/rio"

static int rio_fd = -1;
static int rio_refcount = 0;

static struct qdisc_ops rio_qdisc = {
	ALTQT_RIO,
	"rio",
	rio_attach,
	rio_detach,
	NULL,			/* clear */
	rio_enable,
	rio_disable,
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
rio_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	weight = 0;		/* 0: use default */
	int	lo_inv_pmax = 0;	/* 0: use default */
	int	lo_th_min = 0;		/* 0: use default */
	int	lo_th_max = 0;		/* 0: use default */
	int	med_inv_pmax = 0;	/* 0: use default */
	int	med_th_min = 0;		/* 0: use default */
	int	med_th_max = 0;		/* 0: use default */
	int	hi_inv_pmax = 0;	/* 0: use default */
	int	hi_th_min = 0;		/* 0: use default */
	int	hi_th_max = 0;		/* 0: use default */
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
		} else if (EQUAL(*argv, "lo_thmin")) {
			argc--; argv++;
			if (argc > 0)
				lo_th_min = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "lo_thmax")) {
			argc--; argv++;
			if (argc > 0)
				lo_th_max = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "lo_invpmax")) {
			argc--; argv++;
			if (argc > 0)
				lo_inv_pmax = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "med_thmin")) {
			argc--; argv++;
			if (argc > 0)
				med_th_min = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "med_thmax")) {
			argc--; argv++;
			if (argc > 0)
				med_th_max = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "med_invpmax")) {
			argc--; argv++;
			if (argc > 0)
				med_inv_pmax = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "hi_thmin")) {
			argc--; argv++;
			if (argc > 0)
				hi_th_min = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "hi_thmax")) {
			argc--; argv++;
			if (argc > 0)
				hi_th_max = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "hi_invpmax")) {
			argc--; argv++;
			if (argc > 0)
				hi_inv_pmax = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "rio")) {
			/* just skip */
		} else if (EQUAL(*argv, "ecn")) {
			flags |= RIOF_ECN;
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

	if (qcmd_rio_add_if(ifname, bandwidth, weight,
			    lo_inv_pmax, lo_th_min, lo_th_max,
			    med_inv_pmax, med_th_min, med_th_max,
			    hi_inv_pmax, hi_th_min, hi_th_max,
			    qlimit, pkttime, flags) != 0)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_rio_add_if(const char *ifname, u_int bandwidth, int weight,
		int lo_inv_pmax, int lo_th_min, int lo_th_max,
		int med_inv_pmax, int med_th_min, int med_th_max,
		int hi_inv_pmax, int hi_th_min, int hi_th_max,
		int qlimit, int pkttime, int flags)
{
	struct redparams red_params[RIO_NDROPPREC];
	int error;

	red_params[0].inv_pmax = lo_inv_pmax;
	red_params[0].th_min   = lo_th_min;
	red_params[0].th_max   = lo_th_max;
	red_params[1].inv_pmax = med_inv_pmax;
	red_params[1].th_min   = med_th_min;
	red_params[1].th_max   = med_th_max;
	red_params[2].inv_pmax = hi_inv_pmax;
	red_params[2].th_min   = hi_th_min;
	red_params[2].th_max   = hi_th_max;
	
	error = qop_rio_add_if(NULL, ifname, bandwidth, weight, red_params,
			       qlimit, pkttime, flags);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add rio on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

/*
 * qop api
 */
int 
qop_rio_add_if(struct ifinfo **rp, const char *ifname,
	       u_int bandwidth, int weight, struct redparams *red_params,
	       int qlimit, int pkttime, int flags)
{
	struct ifinfo *ifinfo = NULL;
	struct rio_ifinfo *rio_ifinfo;
	int i, error;

	if ((rio_ifinfo = calloc(1, sizeof(*rio_ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	for (i = 0; i < RIO_NDROPPREC; i++)
		rio_ifinfo->red_params[i] = red_params[i];
	rio_ifinfo->weight   = weight;
	rio_ifinfo->qlimit   = qlimit;
	rio_ifinfo->pkttime  = pkttime;
	rio_ifinfo->flags    = flags;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &rio_qdisc, rio_ifinfo);
	if (error != 0) {
		free(rio_ifinfo);
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
rio_attach(struct ifinfo *ifinfo)
{
	struct rio_interface iface;
	struct rio_ifinfo *rio_ifinfo;
	struct rio_conf conf;
	int i;

	if (rio_fd < 0 &&
	    (rio_fd = open(RIO_DEVICE, O_RDWR)) < 0 &&
	    (rio_fd = open_module(RIO_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "RIO open");
		return (QOPERR_SYSCALL);
	}

	rio_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.rio_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(rio_fd, RIO_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	/* set rio parameters */
	rio_ifinfo = (struct rio_ifinfo *)ifinfo->private;
	memset(&conf, 0, sizeof(conf));
	strncpy(conf.iface.rio_ifname, ifinfo->ifname, IFNAMSIZ);
	for (i = 0; i < RIO_NDROPPREC; i++)
		conf.q_params[i] = rio_ifinfo->red_params[i];
	conf.rio_weight	  = rio_ifinfo->weight;
	conf.rio_limit    = rio_ifinfo->qlimit;
	conf.rio_flags    = rio_ifinfo->flags;
	if (ioctl(rio_fd, RIO_CONFIG, &conf) < 0)
		return (QOPERR_SYSCALL);

#if 1
	LOG(LOG_INFO, 0, "rio attached to %s", iface.rio_ifname);
#endif
	return (0);
}

static int
rio_detach(struct ifinfo *ifinfo)
{
	struct rio_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.rio_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(rio_fd, RIO_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--rio_refcount == 0) {
		close(rio_fd);
		rio_fd = -1;
	}
	return (0);
}

static int
rio_enable(struct ifinfo *ifinfo)
{
	struct rio_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.rio_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(rio_fd, RIO_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
rio_disable(struct ifinfo *ifinfo)
{
	struct rio_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.rio_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(rio_fd, RIO_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}
