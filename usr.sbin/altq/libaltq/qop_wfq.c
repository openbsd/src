/*	$OpenBSD: qop_wfq.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_wfq.c,v 1.5 2001/08/16 10:39:15 kjc Exp $	*/
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
#include <altq/altq_wfq.h>
#include "altq_qop.h"
#include "qop_wfq.h"

static int wfq_attach(struct ifinfo *);
static int wfq_detach(struct ifinfo *);
static int wfq_enable(struct ifinfo *);
static int wfq_disable(struct ifinfo *);

#define WFQ_DEVICE	"/dev/altq/wfq"

static int wfq_fd = -1;
static int wfq_refcount = 0;

static struct qdisc_ops wfq_qdisc = {
	ALTQT_WFQ,
	"wfq",
	wfq_attach,
	wfq_detach,
	NULL,			/* clear */
	wfq_enable,
	wfq_disable,
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
wfq_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	hash_policy = 0;	/* 0: use default */
	int	nqueues = 0;		/* 0: use default */
	int	qsize = 0;		/* 0: use default */

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
		} else if (EQUAL(*argv, "nqueues")) {
			argc--; argv++;
			if (argc > 0)
				nqueues = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "qsize")) {
			argc--; argv++;
			if (argc > 0)
				qsize = atobytes(*argv);
		} else if (EQUAL(*argv, "hash")) {
			argc--; argv++;
			if (argc > 0) {
				if (EQUAL(*argv, "dstaddr"))
					hash_policy = WFQ_HASH_DSTADDR;
				else if (EQUAL(*argv, "full"))
					hash_policy = WFQ_HASH_FULL;
				else if (EQUAL(*argv, "srcport"))
					hash_policy = WFQ_HASH_SRCPORT;
				else {
					LOG(LOG_ERR, 0,
					    "Unknown hash policy '%s'",
					    argv);
					return (0);
				}
			}
		} else if (EQUAL(*argv, "wfq")) {
			/* just skip */
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}
	
	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	if (qsize != 0 && qsize < 1500) {
		LOG(LOG_ERR, 0, "qsize too small: %d bytes", qsize);
		return (0);
	}

	if (qcmd_wfq_add_if(ifname, bandwidth,
			    hash_policy, nqueues, qsize) != 0)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_wfq_add_if(const char *ifname, u_int bandwidth, int hash_policy,
		int nqueues, int qsize)
{
	int error;
	
	error = qop_wfq_add_if(NULL, ifname, bandwidth,
			       hash_policy, nqueues, qsize);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add wfq on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

/*
 * qop api
 */
int 
qop_wfq_add_if(struct ifinfo **rp, const char *ifname, u_int bandwidth,
	       int hash_policy, int nqueues, int qsize)
{
	struct ifinfo *ifinfo = NULL;
	struct wfq_ifinfo *wfq_ifinfo;
	int error;

	if ((wfq_ifinfo = calloc(1, sizeof(*wfq_ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	wfq_ifinfo->hash_policy = hash_policy;
	wfq_ifinfo->nqueues     = nqueues;
	wfq_ifinfo->qsize       = qsize;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &wfq_qdisc, wfq_ifinfo);
	if (error != 0) {
		free(wfq_ifinfo);
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
wfq_attach(struct ifinfo *ifinfo)
{
	struct wfq_interface iface;
	struct wfq_ifinfo *wfq_ifinfo;
	struct wfq_conf conf;

	if (wfq_fd < 0 &&
	    (wfq_fd = open(WFQ_DEVICE, O_RDWR)) < 0 &&
	    (wfq_fd = open_module(WFQ_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "WFQ open");
		return (QOPERR_SYSCALL);
	}

	wfq_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.wfq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(wfq_fd, WFQ_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	/* set wfq parameters */
	wfq_ifinfo = (struct wfq_ifinfo *)ifinfo->private;
	if (wfq_ifinfo->hash_policy != 0 || wfq_ifinfo->nqueues != 0 ||
	    wfq_ifinfo->qsize != 0) {
		memset(&conf, 0, sizeof(conf));
		strncpy(conf.iface.wfq_ifacename, ifinfo->ifname, IFNAMSIZ);
		conf.hash_policy = wfq_ifinfo->hash_policy;
		conf.nqueues     = wfq_ifinfo->nqueues;
		conf.qlimit      = wfq_ifinfo->qsize;
		if (ioctl(wfq_fd, WFQ_CONFIG, &conf) < 0) {
			LOG(LOG_ERR, errno, "WFQ_CONFIG");
			return (QOPERR_SYSCALL);
		}
	}
#if 1
	LOG(LOG_INFO, 0, "wfq attached to %s", iface.wfq_ifacename);
#endif
	return (0);
}

static int
wfq_detach(struct ifinfo *ifinfo)
{
	struct wfq_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.wfq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(wfq_fd, WFQ_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--wfq_refcount == 0) {
		close(wfq_fd);
		wfq_fd = -1;
	}
	return (0);
}

static int
wfq_enable(struct ifinfo *ifinfo)
{
	struct wfq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.wfq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(wfq_fd, WFQ_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
wfq_disable(struct ifinfo *ifinfo)
{
	struct wfq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.wfq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(wfq_fd, WFQ_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}
