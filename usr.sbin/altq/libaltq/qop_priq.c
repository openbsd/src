/*	$OpenBSD: qop_priq.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_priq.c,v 1.3 2001/08/16 10:39:14 kjc Exp $	*/
/*
 * Copyright (C) 2000
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
#include <altq/altq_priq.h>
#include "altq_qop.h"
#include "qop_priq.h"

static int qop_priq_enable_hook(struct ifinfo *);

static int priq_attach(struct ifinfo *);
static int priq_detach(struct ifinfo *);
static int priq_clear(struct ifinfo *);
static int priq_enable(struct ifinfo *);
static int priq_disable(struct ifinfo *);
static int priq_add_class(struct classinfo *);
static int priq_modify_class(struct classinfo *, void *);
static int priq_delete_class(struct classinfo *);
static int priq_add_filter(struct fltrinfo *);
static int priq_delete_filter(struct fltrinfo *);

#define PRIQ_DEVICE	"/dev/altq/priq"

static int priq_fd = -1;
static int priq_refcount = 0;

static struct qdisc_ops priq_qdisc = {
	ALTQT_PRIQ,
	"priq",
	priq_attach,
	priq_detach,
	priq_clear,
	priq_enable,
	priq_disable,
	priq_add_class,
	priq_modify_class,
	priq_delete_class,
	priq_add_filter,
	priq_delete_filter,
};

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

/*
 * parser interface
 */
int
priq_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	int	flags = 0;

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
		} else if (EQUAL(*argv, "priq")) {
			/* just skip */
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	if (qcmd_priq_add_if(ifname, bandwidth, flags) != 0)
		return (0);
	return (1);
}

int
priq_class_parser(const char *ifname, const char *class_name,
		  const char *parent_name, int argc, char **argv)
{
	int	pri = 0, qlimit = 50;
	int	flags = 0, error;

	while (argc > 0) {
		if (EQUAL(*argv, "priority")) {
			argc--; argv++;
			if (argc > 0)
				pri = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "qlimit")) {
			argc--; argv++;
			if (argc > 0)
				qlimit = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "default")) {
			flags |= PRCF_DEFAULTCLASS;
		} else if (EQUAL(*argv, "red")) {
			flags |= PRCF_RED;
		} else if (EQUAL(*argv, "ecn")) {
			flags |= PRCF_ECN;
		} else if (EQUAL(*argv, "rio")) {
			flags |= PRCF_RIO;
		} else if (EQUAL(*argv, "cleardscp")) {
			flags |= PRCF_CLEARDSCP;
		} else {
			LOG(LOG_ERR, 0,
			    "Unknown keyword '%s' in %s, line %d",
			    *argv, altqconfigfile, line_no);
			return (0);
		}

		argc--; argv++;
	}

	if ((flags & PRCF_ECN) && (flags & (PRCF_RED|PRCF_RIO)) == 0)
		flags |= PRCF_RED;

	error = qcmd_priq_add_class(ifname, class_name, pri, qlimit, flags);

	if (error) {
		LOG(LOG_ERR, errno, "priq_class_parser: %s",
		    qoperror(error));
		return (0);
	}
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_priq_add_if(const char *ifname, u_int bandwidth, int flags)
{
	int error;
	
	error = qop_priq_add_if(NULL, ifname, bandwidth, flags);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add priq on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

int
qcmd_priq_add_class(const char *ifname, const char *class_name,
		    int pri, int qlimit, int flags)
{
	struct ifinfo *ifinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0)
		error = qop_priq_add_class(NULL, class_name, ifinfo,
					   pri, qlimit, flags);
	if (error != 0)
		LOG(LOG_ERR, errno,
		    "priq: %s: can't add class '%s' on interface '%s'",
		    qoperror(error), class_name, ifname);
	return (error);
}

int
qcmd_priq_modify_class(const char *ifname, const char *class_name,
		       int pri, int qlimit, int flags)
{
	struct ifinfo *ifinfo;
	struct classinfo *clinfo;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if ((clinfo = clname2clinfo(ifinfo, class_name)) == NULL)
		return (QOPERR_BADCLASS);

	return qop_priq_modify_class(clinfo, pri, qlimit, flags);
}

/*
 * qop api
 */
int 
qop_priq_add_if(struct ifinfo **rp, const char *ifname,
		u_int bandwidth, int flags)
{
	struct ifinfo *ifinfo = NULL;
	struct priq_ifinfo *priq_ifinfo = NULL;
	int error;

	if ((priq_ifinfo = calloc(1, sizeof(*priq_ifinfo))) == NULL)
		return (QOPERR_NOMEM);

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &priq_qdisc, priq_ifinfo);
	if (error != 0)
		goto err_ret;

	/* set enable hook */
	ifinfo->enable_hook = qop_priq_enable_hook;

	if (rp != NULL)
		*rp = ifinfo;
	return (0);

 err_ret:
	if (priq_ifinfo != NULL) {
		free(priq_ifinfo);
		if (ifinfo != NULL)
			ifinfo->private = NULL;
	}
	return (error);
}

int 
qop_priq_add_class(struct classinfo **rp, const char *class_name,
		   struct ifinfo *ifinfo, int pri, int qlimit, int flags)
{
	struct classinfo *clinfo;
	struct priq_ifinfo *priq_ifinfo;
	struct priq_classinfo *priq_clinfo = NULL;
	int error;

	priq_ifinfo = ifinfo->private;
	if ((flags & PRCF_DEFAULTCLASS) && priq_ifinfo->default_class != NULL)
		return (QOPERR_CLASS_INVAL);

	if ((priq_clinfo = calloc(1, sizeof(*priq_clinfo))) == NULL) {
		error = QOPERR_NOMEM;
		goto err_ret;
	}

	priq_clinfo->pri = pri;
	priq_clinfo->qlimit = qlimit;
	priq_clinfo->flags = flags;

	if ((error = qop_add_class(&clinfo, class_name, ifinfo, NULL,
				   priq_clinfo)) != 0)
		goto err_ret;

	if (flags & PRCF_DEFAULTCLASS)
		priq_ifinfo->default_class = clinfo;

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (priq_clinfo != NULL) {
		free(priq_clinfo);
		clinfo->private = NULL;
	}
	
	return (error);
}

int
qop_priq_modify_class(struct classinfo *clinfo, 
		      int pri, int qlimit, int flags)
{
	struct priq_classinfo *priq_clinfo, *parent_clinfo;
	int error;

	priq_clinfo = clinfo->private;
	if (clinfo->parent == NULL)
		return (QOPERR_CLASS_INVAL);
	parent_clinfo = clinfo->parent->private;

	priq_clinfo->pri = pri;
	priq_clinfo->qlimit = qlimit;
	priq_clinfo->flags = flags;

	error = qop_modify_class(clinfo, NULL);
	if (error == 0)
		return (0);
	return (error);
}

/*
 * sanity check at enabling priq:
 *  1. there must one default class for an interface
 */
static int
qop_priq_enable_hook(struct ifinfo *ifinfo)
{
	struct priq_ifinfo *priq_ifinfo;
	
	priq_ifinfo = ifinfo->private;
	if (priq_ifinfo->default_class == NULL) {
		LOG(LOG_ERR, 0, "priq: no default class on interface %s!",
		    ifinfo->ifname);
		return (QOPERR_CLASS);
	}
	return (0);
}

/*
 *  system call interfaces for qdisc_ops
 */
static int
priq_attach(struct ifinfo *ifinfo)
{
	struct priq_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);

	if (priq_fd < 0 &&
	    (priq_fd = open(PRIQ_DEVICE, O_RDWR)) < 0 &&
	    (priq_fd = open_module(PRIQ_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "PRIQ open");
		return (QOPERR_SYSCALL);
	}

	priq_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);
	iface.arg = ifinfo->bandwidth;

	if (ioctl(priq_fd, PRIQ_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_detach(struct ifinfo *ifinfo)
{
	struct priq_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(priq_fd, PRIQ_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--priq_refcount == 0) {
		close(priq_fd);
		priq_fd = -1;
	}
	return (0);
}

static int
priq_clear(struct ifinfo *ifinfo)
{
	struct priq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(priq_fd, PRIQ_CLEAR, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_enable(struct ifinfo *ifinfo)
{
	struct priq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(priq_fd, PRIQ_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_disable(struct ifinfo *ifinfo)
{
	struct priq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(priq_fd, PRIQ_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_add_class(struct classinfo *clinfo)
{
	struct priq_add_class class_add;
	struct priq_classinfo *priq_clinfo;
	struct priq_ifinfo *priq_ifinfo;

	priq_ifinfo = clinfo->ifinfo->private;
	priq_clinfo = clinfo->private;
	
	memset(&class_add, 0, sizeof(class_add));
	strncpy(class_add.iface.ifname, clinfo->ifinfo->ifname, IFNAMSIZ);

	class_add.pri = priq_clinfo->pri;
	class_add.qlimit = priq_clinfo->qlimit;
	class_add.flags = priq_clinfo->flags;
	if (ioctl(priq_fd, PRIQ_ADD_CLASS, &class_add) < 0) {
		clinfo->handle = PRIQ_NULLCLASS_HANDLE;
		return (QOPERR_SYSCALL);
	}
	clinfo->handle = class_add.class_handle;
	return (0);
}

static int
priq_modify_class(struct classinfo *clinfo, void *arg)
{
	struct priq_modify_class class_mod;
	struct priq_classinfo *priq_clinfo;

	priq_clinfo = clinfo->private;

	memset(&class_mod, 0, sizeof(class_mod));
	strncpy(class_mod.iface.ifname, clinfo->ifinfo->ifname, IFNAMSIZ);
	class_mod.class_handle = clinfo->handle;

	class_mod.pri = priq_clinfo->pri;
	class_mod.qlimit = priq_clinfo->qlimit;
	class_mod.flags = priq_clinfo->flags;

	if (ioctl(priq_fd, PRIQ_MOD_CLASS, &class_mod) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_delete_class(struct classinfo *clinfo)
{
	struct priq_delete_class class_delete;

	if (clinfo->handle == PRIQ_NULLCLASS_HANDLE)
		return (0);

	memset(&class_delete, 0, sizeof(class_delete));
	strncpy(class_delete.iface.ifname, clinfo->ifinfo->ifname,
		IFNAMSIZ);
	class_delete.class_handle = clinfo->handle;

	if (ioctl(priq_fd, PRIQ_DEL_CLASS, &class_delete) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
priq_add_filter(struct fltrinfo *fltrinfo)
{
	struct priq_add_filter fltr_add;
	
	memset(&fltr_add, 0, sizeof(fltr_add));
	strncpy(fltr_add.iface.ifname, fltrinfo->clinfo->ifinfo->ifname,
		IFNAMSIZ);
	fltr_add.class_handle = fltrinfo->clinfo->handle;
	fltr_add.filter = fltrinfo->fltr;

	if (ioctl(priq_fd, PRIQ_ADD_FILTER, &fltr_add) < 0)
		return (QOPERR_SYSCALL);
	fltrinfo->handle = fltr_add.filter_handle;
	return (0);
}

static int
priq_delete_filter(struct fltrinfo *fltrinfo)
{
	struct priq_delete_filter fltr_del;

	memset(&fltr_del, 0, sizeof(fltr_del));
	strncpy(fltr_del.iface.ifname, fltrinfo->clinfo->ifinfo->ifname,
		IFNAMSIZ);
	fltr_del.filter_handle = fltrinfo->handle;

	if (ioctl(priq_fd, PRIQ_DEL_FILTER, &fltr_del) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}


