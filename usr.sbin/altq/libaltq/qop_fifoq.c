/*	$OpenBSD: qop_fifoq.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_fifoq.c,v 1.5 2001/08/16 10:39:14 kjc Exp $	*/
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
#include <altq/altq_fifoq.h>
#include "altq_qop.h"
#include "qop_fifoq.h"

static int fifoq_attach(struct ifinfo *);
static int fifoq_detach(struct ifinfo *);
static int fifoq_enable(struct ifinfo *);
static int fifoq_disable(struct ifinfo *);

#define FIFOQ_DEVICE	"/dev/altq/fifoq"

static int fifoq_fd = -1;
static int fifoq_refcount = 0;

static struct qdisc_ops fifoq_qdisc = {
	ALTQT_FIFOQ,
	"fifoq",
	fifoq_attach,
	fifoq_detach,
	NULL,			/* clear */
	fifoq_enable,
	fifoq_disable,
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
fifoq_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	qlimit = 50;

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
		} else if (EQUAL(*argv, "qlimit")) {
			argc--; argv++;
			if (argc > 0)
				qlimit = (int)strtol(*argv, NULL, 0);
		} else if (EQUAL(*argv, "fifoq")) {
			/* just skip */
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	if (qcmd_fifoq_add_if(ifname, bandwidth, qlimit) != 0)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_fifoq_add_if(const char *ifname, u_int bandwidth, int qlimit)
{
	int error;
	
	error = qop_fifoq_add_if(NULL, ifname, bandwidth, qlimit);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add fifoq on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

/*
 * qop api
 */
int 
qop_fifoq_add_if(struct ifinfo **rp, const char *ifname,
		 u_int bandwidth, int qlimit)
{
	struct ifinfo *ifinfo = NULL;
	struct fifoq_ifinfo *fifoq_ifinfo;
	int error;

	if ((fifoq_ifinfo = calloc(1, sizeof(*fifoq_ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	fifoq_ifinfo->qlimit   = qlimit;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &fifoq_qdisc, fifoq_ifinfo);
	if (error != 0) {
		free(fifoq_ifinfo);
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
fifoq_attach(struct ifinfo *ifinfo)
{
	struct fifoq_interface iface;
	struct fifoq_ifinfo *fifoq_ifinfo;
	struct fifoq_conf conf;

	if (fifoq_fd < 0 &&
	    (fifoq_fd = open(FIFOQ_DEVICE, O_RDWR)) < 0 &&
	    (fifoq_fd = open_module(FIFOQ_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "FIFOQ open");
		return (QOPERR_SYSCALL);
	}

	fifoq_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.fifoq_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(fifoq_fd, FIFOQ_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	/* set fifoq parameters */
	fifoq_ifinfo = (struct fifoq_ifinfo *)ifinfo->private;
	if (fifoq_ifinfo->qlimit > 0) {
		memset(&conf, 0, sizeof(conf));
		strncpy(conf.iface.fifoq_ifname, ifinfo->ifname, IFNAMSIZ);
		conf.fifoq_limit = fifoq_ifinfo->qlimit;
		if (ioctl(fifoq_fd, FIFOQ_CONFIG, &conf) < 0)
			return (QOPERR_SYSCALL);
	}
#if 1
	LOG(LOG_INFO, 0, "fifoq attached to %s", iface.fifoq_ifname);
#endif
	return (0);
}

static int
fifoq_detach(struct ifinfo *ifinfo)
{
	struct fifoq_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.fifoq_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(fifoq_fd, FIFOQ_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--fifoq_refcount == 0) {
		close(fifoq_fd);
		fifoq_fd = -1;
	}
	return (0);
}

static int
fifoq_enable(struct ifinfo *ifinfo)
{
	struct fifoq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.fifoq_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(fifoq_fd, FIFOQ_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
fifoq_disable(struct ifinfo *ifinfo)
{
	struct fifoq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.fifoq_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(fifoq_fd, FIFOQ_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}
