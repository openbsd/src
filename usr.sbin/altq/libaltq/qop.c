/*	$OpenBSD: qop.c,v 1.3 2001/10/26 07:39:52 kjc Exp $	*/
/*	$KAME: qop.c,v 1.10 2001/08/16 10:39:13 kjc Exp $	*/
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
#include <sys/stat.h>
#if defined(__FreeBSD__) && (__FreeBSD_version > 300000)
#include <sys/linker.h>
#endif

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
#include <err.h>
#include <syslog.h>

#include <altq/altq.h>
#include <altq/altq_red.h>
#include <altq/altq_rio.h>
#include <altq/altq_cdnr.h>
#include "altq_qop.h"
#include "qop_cdnr.h"

#define	ALTQ_DEVICE	"/dev/altq/altq"
#define RED_DEVICE	"/dev/altq/red"
#define RIO_DEVICE	"/dev/altq/rio"
#define CDNR_DEVICE	"/dev/altq/cdnr"

#ifndef LIST_HEAD_INITIALIZER
#define LIST_HEAD_INITIALIZER(head)	{ NULL }
#endif

/*
 * token bucket regulator information
 */
struct tbrinfo {
	LIST_ENTRY(tbrinfo) link;
	char	ifname[IFNAMSIZ];	/* if name, e.g. "en0" */
	struct tb_profile tb_prof, otb_prof;
	int installed;
};

/*
 * Static globals
 */
/* a list of configured interfaces */
LIST_HEAD(qop_iflist, ifinfo)	qop_iflist = LIST_HEAD_INITIALIZER(&iflist);
/* a list of configured token bucket regulators */
LIST_HEAD(tbr_list, tbrinfo)	tbr_list = LIST_HEAD_INITIALIZER(&tbr_list);
int	Debug_mode = 0;		/* nosched (dummy mode) */

/*
 * internal functions
 */
static int get_ifmtu(const char *);
static void tbr_install(const char *);
static void tbr_deinstall(const char *);
static int add_filter_rule(struct ifinfo *, struct fltrinfo *,
			   struct fltrinfo **);
static int remove_filter_rule(struct ifinfo *,
			      struct fltrinfo *);
static int filt_check_relation(struct flow_filter *, struct flow_filter *);
static int filt_disjoint(struct flow_filter *, struct flow_filter *);
static int filt_subset(struct flow_filter *, struct flow_filter *);

/*
 * QCMD (Queue Command) API
 */
int
qcmd_init(void)
{
	int error;

	/* read config file and execute commands */
	error = qcmd_config();
	if (error != 0)
		return (error);

	error = qcmd_enableall();
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: qcmd_init failed", qoperror(error));
	return (error);
}

int
qcmd_enable(const char *ifname)
{
	struct ifinfo	*ifinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0)
		error = qop_enable(ifinfo);

	if (error == 0) {
		LOG(LOG_INFO, 0, "%s enabled on interface %s (mtu:%d)",
		    ifinfo->qdisc->qname, ifname, ifinfo->ifmtu);
	} else
		LOG(LOG_ERR, errno, "%s: enable failed!", qoperror(error));
	return (error);
}

int
qcmd_disable(const char *ifname)
{
	struct ifinfo	*ifinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0)
		error = qop_disable(ifinfo);

	if (error != 0)
		LOG(LOG_ERR, errno, "%s: disable failed!", qoperror(error));
	return (error);
}

int
qcmd_enableall()
{
	struct ifinfo	*ifinfo;
	int error;

	LIST_FOREACH(ifinfo, &qop_iflist, next) {
		if ((error = qop_enable(ifinfo)) != 0)
			return (error);
		LOG(LOG_INFO, 0, "%s enabled on interface %s (mtu:%d)",
		    ifinfo->qdisc->qname, ifinfo->ifname, ifinfo->ifmtu);
	}
	return (0);
}

int
qcmd_disableall()
{
	struct ifinfo	*ifinfo;
	int	err, error = 0;
	
	LIST_FOREACH(ifinfo, &qop_iflist, next)
		if ((err = qop_disable(ifinfo)) != 0)
			if (error == 0)
				error = err;
	return (error);
}

int
qcmd_clear(const char *ifname)
{
	struct ifinfo	*ifinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;
		
	if (error == 0)
		error = qop_clear(ifinfo);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: clear failed!", qoperror(error));
	return (error);
}

int
qcmd_destroyall(void)
{
	while (!LIST_EMPTY(&qop_iflist))
		(void)qop_delete_if(LIST_FIRST(&qop_iflist));
	return (0);
}

int
qcmd_restart(void)
{
	qcmd_destroyall();
	return qcmd_init();
}

int
qcmd_delete_class(const char *ifname, const char *clname)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0 &&
	    (clinfo = clname2clinfo(ifinfo, clname)) == NULL)
		error = QOPERR_BADCLASS;

	if (error == 0)
		error = qop_delete_class(clinfo);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: delete_class failed",
		    qoperror(error));
	return (error);
}

int
qcmd_add_filter(const char *ifname, const char *clname, const char *flname,
		 const struct flow_filter *fltr)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0 &&
	    (clinfo = clname2clinfo(ifinfo, clname)) == NULL) {
		/*
		 * there is no matching class.
		 * check if it is for a traffic conditioner
		 */
		if ((ifinfo = input_ifname2ifinfo(ifname)) == NULL ||
		    (clinfo = clname2clinfo(ifinfo, clname)) == NULL)
			error = QOPERR_BADCLASS;
	}

	if (error == 0)
		error = qop_add_filter(NULL, clinfo, flname, fltr, NULL);

	if (error != 0)
		LOG(LOG_ERR, errno, "%s: add filter failed!",
		    qoperror(error));
	else if (IsDebug(DEBUG_ALTQ)) {
		LOG(LOG_DEBUG, 0, "%s: add a filter %s to class %s",
		    ifname, flname ? flname : "(null)",
		    clname ? clname : "(null)");
		print_filter(fltr);
	}
	return (error);
}

int
qcmd_delete_filter(const char *ifname, const char *clname, const char *flname)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	struct fltrinfo		*fltrinfo;
	int error = 0;
	
	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0 &&
	    (clinfo = clname2clinfo(ifinfo, clname)) == NULL) {
		/*
		 * there is no matching class.
		 * check if it is for a traffic conditioner
		 */
		if ((ifinfo = input_ifname2ifinfo(ifname)) == NULL ||
		    (clinfo = clname2clinfo(ifinfo, clname)) == NULL)
		error = QOPERR_BADCLASS;
	}

	if (error == 0 &&
	    (fltrinfo = flname2flinfo(clinfo, flname)) == NULL)
		error = QOPERR_BADFILTER;

	if (error == 0)
		error = qop_delete_filter(fltrinfo);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: delete filter failed!",
		    qoperror(error));
	return (error);
}

int
qcmd_tbr_register(const char *ifname, u_int rate, u_int size)
{
	struct tbrinfo *info;

	if ((info = calloc(1, sizeof(struct tbrinfo))) == NULL)
		return (QOPERR_NOMEM);

	strlcpy(info->ifname, ifname, sizeof(info->ifname));
	info->tb_prof.rate = rate;
	info->tb_prof.depth = size;
	info->installed = 0;
	LIST_INSERT_HEAD(&tbr_list, info, link);
	return (0);
}

/*
 * QOP (Queue Operation) API
 */

int
qop_add_if(struct ifinfo **rp, const char *ifname, u_int bandwidth,
	   struct qdisc_ops *qdisc_ops, void *if_private)
{
	struct ifinfo	*ifinfo;
	int error;

	if (ifname2ifinfo(ifname) != NULL) {
		LOG(LOG_ERR, 0, "qop_add_if: %s already exists!", ifname);
		return (QOPERR_BADIF);
	}

	if ((ifinfo = calloc(1, sizeof(struct ifinfo))) == NULL)
		return (QOPERR_NOMEM);
	ifinfo->ifname = strdup(ifname);
	ifinfo->bandwidth = bandwidth;
	ifinfo->enabled = 0;
	if (ifname[0] == '_')
		/* input interface */
		ifname += 1;
	ifinfo->ifindex = get_ifindex(ifname);
	ifinfo->ifmtu = get_ifmtu(ifname);
	if (qdisc_ops == NULL || Debug_mode)
		ifinfo->qdisc = &nop_qdisc; /* replace syscalls by nops */
	else
		ifinfo->qdisc = qdisc_ops;
	ifinfo->private = if_private;
	LIST_INIT(&ifinfo->cllist);
	LIST_INIT(&ifinfo->fltr_rules);

	/* Link the interface info structure */
	LIST_INSERT_HEAD(&qop_iflist, ifinfo, next);

	/* install token bucket regulator, if necessary */
	tbr_install(ifname);

	/* attach the discipline to the interface */
	if ((error = (*ifinfo->qdisc->attach)(ifinfo)) != 0)
		goto err_ret;

	/* disable and clear the interface */
	if (ifinfo->qdisc->disable != NULL)
		if ((error = (*ifinfo->qdisc->disable)(ifinfo)) != 0)
			goto err_ret;
	if (ifinfo->qdisc->clear != NULL)
		if ((error = (*ifinfo->qdisc->clear)(ifinfo)) != 0)
			goto err_ret;

	if (rp != NULL)
		*rp = ifinfo;
	return (0);

err_ret:
	if (ifinfo != NULL) {
		LIST_REMOVE(ifinfo, next);
		if (ifinfo->ifname != NULL)
			free(ifinfo->ifname);
		free(ifinfo);
	}
	return (error);
}

int
qop_delete_if(struct ifinfo *ifinfo)
{
	(void)qop_disable(ifinfo);
	(void)qop_clear(ifinfo);

	if (ifinfo->delete_hook != NULL)
		(*ifinfo->delete_hook)(ifinfo);

	/* remove this entry from qop_iflist */
	LIST_REMOVE(ifinfo, next);

	(void)(*ifinfo->qdisc->detach)(ifinfo);

	/* deinstall token bucket regulator, if necessary */
	tbr_deinstall(ifinfo->ifname);

	if (ifinfo->private != NULL)
		free(ifinfo->private);
	if (ifinfo->ifname != NULL)
		free(ifinfo->ifname);
	free(ifinfo);
	return (0);
}

int
qop_enable(struct ifinfo *ifinfo)
{
	int error;

	if (ifinfo->enable_hook != NULL)
		if ((error = (*ifinfo->enable_hook)(ifinfo)) != 0)
			return (error);

	if (ifinfo->qdisc->enable != NULL)
		if ((error = (*ifinfo->qdisc->enable)(ifinfo)) != 0)
			return (error);
	ifinfo->enabled = 1;
	return (0);
}

int
qop_disable(struct ifinfo *ifinfo)
{
	int error;

	if (ifinfo->qdisc->disable != NULL)
		if ((error = (*ifinfo->qdisc->disable)(ifinfo)) != 0)
			return (error);
	ifinfo->enabled = 0;
	return (0);
}

int
qop_clear(struct ifinfo *ifinfo)
{
	struct classinfo	*clinfo;

	/* free all classes and filters */
	if (ifinfo->ifname[0] != '_') {
		/* output interface.  delete from leaf classes */
		while (!LIST_EMPTY(&ifinfo->cllist)) {
			LIST_FOREACH(clinfo, &ifinfo->cllist, next) {
				if (clinfo->child != NULL)
					continue;
				qop_delete_class(clinfo);
				/*
				 * the list has been changed,
				 *  restart from the head
				 */
				break;
			}
		}
	} else {
		/* input interface. delete from parents */
		struct classinfo *root = get_rootclass(ifinfo);
		
		while (!LIST_EMPTY(&ifinfo->cllist)) {
			LIST_FOREACH(clinfo, &ifinfo->cllist, next)
				if (clinfo->parent == root) {
					qop_delete_cdnr(clinfo);
					break;
				}
			if (root->child == NULL)
				qop_delete_class(root);
		}
	}
	
	/* clear the interface */
	if (ifinfo->qdisc->clear != NULL)
		return (*ifinfo->qdisc->clear)(ifinfo);
	return (0);
}

int
qop_add_class(struct classinfo **rp, const char *clname,
	      struct ifinfo *ifinfo, struct classinfo *parent, 
	      void *class_private)
{
	struct classinfo	*clinfo;
	int error;

	if ((clinfo = calloc(1, sizeof(*clinfo))) == NULL)
		return (QOPERR_NOMEM);

	if (clname != NULL)
		clinfo->clname = strdup(clname);
	else
		clinfo->clname = strdup("(null)");  /* dummy name */
	clinfo->ifinfo = ifinfo;
	clinfo->private = class_private;
	clinfo->parent = parent;
	clinfo->child = NULL;
	LIST_INIT(&clinfo->fltrlist);

	if ((error = (*ifinfo->qdisc->add_class)(clinfo)) != 0)
		goto err_ret;

	/* link classinfo in lists */
	LIST_INSERT_HEAD(&ifinfo->cllist, clinfo, next);

	if (parent != NULL) {
		clinfo->sibling = parent->child;
		clinfo->parent->child = clinfo;
	}

	if (rp != NULL)
		*rp = clinfo;
	return (0);

err_ret:
	if (clinfo != NULL) {
		if (clinfo->clname != NULL)
			free(clinfo->clname);
		free(clinfo);
	}
	return (error);
}

int
qop_modify_class(struct classinfo *clinfo, void *arg)
{
	return (*clinfo->ifinfo->qdisc->modify_class)(clinfo, arg);
}

int
qop_delete_class(struct classinfo *clinfo)
{
	struct ifinfo		*ifinfo = clinfo->ifinfo;
	struct classinfo	*prev;
	int error;

	/* a class to be removed should not have a child */
	if (clinfo->child != NULL)
		return (QOPERR_CLASS_PERM);

	/* remove filters associated to this class */
	while (!LIST_EMPTY(&clinfo->fltrlist))
		(void)qop_delete_filter(LIST_FIRST(&clinfo->fltrlist));

	if (clinfo->delete_hook != NULL)
		(*clinfo->delete_hook)(clinfo);

	/* remove class info from the interface */
	LIST_REMOVE(clinfo, next);

	/* remove this class from the child list */
	if (clinfo->parent != NULL) {
		if (clinfo->parent->child == clinfo)
			clinfo->parent->child = clinfo->sibling;
		else for (prev = clinfo->parent->child; prev->sibling != NULL;
			  prev = prev->sibling)
			if (prev->sibling == clinfo) {
				prev->sibling = clinfo->sibling;
				break;
			}
	}

	/* delete class from kernel */
	if ((error = (*ifinfo->qdisc->delete_class)(clinfo)) != 0)
		return (error);

	if (clinfo->private != NULL)
		free(clinfo->private);
	if (clinfo->clname != NULL)
		free(clinfo->clname);
	free(clinfo);
	return (0);
}

int
qop_add_filter(struct fltrinfo **rp, struct classinfo *clinfo,
		   const char *flname, const struct flow_filter *fltr,
		   struct fltrinfo **conflict)
{
	struct ifinfo	*ifinfo;
	struct fltrinfo *fltrinfo;
	int error;

	if ((fltrinfo = calloc(1, sizeof(*fltrinfo))) == NULL)
		return (QOPERR_NOMEM);

	fltrinfo->clinfo = clinfo;
	fltrinfo->fltr = *fltr;
#if 1
	/* fix this */
	fltrinfo->line_no = line_no;		/* XXX */
	fltrinfo->dontwarn = filter_dontwarn;	/* XXX */
#endif
	if (flname != NULL)
		fltrinfo->flname = strdup(flname);
	else
		fltrinfo->flname = strdup("(null)");  /* dummy name */

	/* check and save the filter */
	ifinfo = clinfo->ifinfo;
	if ((error = add_filter_rule(ifinfo, fltrinfo, conflict)) != 0)
		goto err_ret;

	/* install the filter to the kernel */
	if ((error = (*ifinfo->qdisc->add_filter)(fltrinfo)) != 0) {
		remove_filter_rule(ifinfo, fltrinfo);
		goto err_ret;
	}

	/* link fltrinfo onto fltrlist of the class */
	LIST_INSERT_HEAD(&clinfo->fltrlist, fltrinfo, next);

	if (rp != NULL)
		*rp = fltrinfo;
	return (0);

err_ret:
	if (fltrinfo != NULL) {
		if (fltrinfo->flname != NULL)
			free(fltrinfo->flname);
		free(fltrinfo);
	}
	return (error);
}

int
qop_delete_filter(struct fltrinfo *fltrinfo)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error;

	/* remove filter info from the class */
	clinfo = fltrinfo->clinfo;
	ifinfo = clinfo->ifinfo;


	/* remove the entry from fltrlist of the class */
	LIST_REMOVE(fltrinfo, next);

	remove_filter_rule(ifinfo, fltrinfo);

	/* delete filter from kernel */
	if ((error = (*ifinfo->qdisc->delete_filter)(fltrinfo)) != 0)
		return (error);

	if (fltrinfo->flname)
		free(fltrinfo->flname);
	free(fltrinfo);
	return (0);
}

const char *
qoperror(int qoperrno)
{
	static char buf[64];
	
	if (qoperrno <= QOPERR_MAX)
		return (qop_errlist[qoperrno]);
	snprintf(buf, sizeof(buf), "unknown error %d", qoperrno);
	return (buf);
}

/*
 * misc functions
 */
struct ifinfo *
ifname2ifinfo(const char *ifname)
{
	struct ifinfo	*ifinfo;

	LIST_FOREACH(ifinfo, &qop_iflist, next)
		if (ifinfo->ifname != NULL &&
		    strcmp(ifinfo->ifname, ifname) == 0)
			return (ifinfo);
	return (NULL);
}

struct ifinfo *
input_ifname2ifinfo(const char *ifname)
{
	struct ifinfo	*ifinfo;

	LIST_FOREACH(ifinfo, &qop_iflist, next)
		if (ifinfo->ifname[0] == '_' &&
		    strcmp(ifinfo->ifname+1, ifname) == 0)
			return (ifinfo);
	return (NULL);
}

struct classinfo *
clname2clinfo(const struct ifinfo *ifinfo, const char *clname)
{
	struct classinfo	*clinfo;
	
	LIST_FOREACH(clinfo, &ifinfo->cllist, next)
		if (clinfo->clname != NULL &&
		    strcmp(clinfo->clname, clname) == 0)
			return (clinfo);
	return (NULL);
}

struct classinfo *
clhandle2clinfo(struct ifinfo *ifinfo, u_long handle)
{
	struct classinfo *clinfo;

	LIST_FOREACH(clinfo, &ifinfo->cllist, next)
		if (clinfo->handle == handle)
			return (clinfo);
	return (NULL);
}

struct fltrinfo *
flname2flinfo(const struct classinfo *clinfo, const char *flname)
{
	struct fltrinfo	*fltrinfo;
	
	LIST_FOREACH(fltrinfo, &clinfo->fltrlist, next)
		if (fltrinfo->flname != NULL &&
		    strcmp(fltrinfo->flname, flname) == 0)
			return (fltrinfo);
	return (NULL);
}

struct fltrinfo *
flhandle2fltrinfo(struct ifinfo *ifinfo, u_long handle)
{
	struct fltrinfo *fltrinfo;

	LIST_FOREACH(fltrinfo, &ifinfo->fltr_rules, nextrule)
		if (fltrinfo->handle == handle)
			return (fltrinfo);
	return (NULL);
}

int
is_q_enabled(const char *ifname)
{
	struct ifinfo	*ifinfo;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (0);
	return (ifinfo->enabled);
}

/*
 * functions to walk through a class tree:
 *
 *   for (clinfo = get_rootclass(ifinfo);
 *	clinfo != NULL; clinfo = get_nextclass(clinfo)) {
 *	  do_something;
 *   }
 */
struct classinfo *get_rootclass(struct ifinfo *ifinfo)
{
	struct classinfo *clinfo;

	/* find a class without parent */
	LIST_FOREACH(clinfo, &ifinfo->cllist, next)
		if (clinfo->parent == NULL)
			return (clinfo);
	return (NULL);
}

/* return next class in the tree */
struct classinfo *get_nextclass(struct classinfo *clinfo)
{
	struct classinfo *next;

	if (clinfo->child != NULL)
		next = clinfo->child;
	else if (clinfo->sibling != NULL)
		next = clinfo->sibling;
	else {
		next = clinfo;
		while ((next = next->parent) != NULL)
			if (next->sibling) {
				next = next->sibling;
				break;
			}
	}
	return (next);
}

u_long
atobps(const char *s)
{
	double bandwidth;
	char *cp;
			
	bandwidth = strtod(s, &cp);
	if (cp != NULL) {
		if (*cp == 'K' || *cp == 'k')
			bandwidth *= 1000;
		else if (*cp == 'M' || *cp == 'm')
			bandwidth *= 1000000;
		else if (*cp == 'G' || *cp == 'g')
			bandwidth *= 1000000000;
	}
	if (bandwidth < 0)
		bandwidth = 0;
	return ((u_long)bandwidth);
}

u_long
atobytes(const char *s)
{
	double bytes;
	char *cp;
			
	bytes = strtod(s, &cp);
	if (cp != NULL) {
		if (*cp == 'K' || *cp == 'k')
			bytes *= 1024;
		else if (*cp == 'M' || *cp == 'm')
			bytes *= 1024 * 1024;
		else if (*cp == 'G' || *cp == 'g')
			bytes *= 1024 * 1024 * 1024;
	}
	if (bytes < 0)
		bytes = 0;
	return ((u_long)bytes);
}

static int 
get_ifmtu(const char *ifname)
{
	int s, mtu;
	struct ifreq ifr;
#ifdef __OpenBSD__
	struct if_data ifdata;
#endif

	mtu = 512; /* default MTU */

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return (mtu);
	strncpy(ifr.ifr_name, ifname, sizeof ifr.ifr_name);
#ifdef __OpenBSD__
	ifr.ifr_data = (caddr_t)&ifdata;
	if (ioctl(s, SIOCGIFDATA, (caddr_t)&ifr) == 0)
		mtu = ifdata.ifi_mtu;
#else
	if (ioctl(s, SIOCGIFMTU, (caddr_t)&ifr) == 0)
		mtu = ifr.ifr_mtu;
#endif
	close(s);
	return (mtu);
}

static void
tbr_install(const char *ifname)
{
	struct tbrinfo *info;
	struct tbrreq req;
	int fd;

	LIST_FOREACH(info, &tbr_list, link)
	    if (strcmp(info->ifname, ifname) == 0)
		    break;
	if (info == NULL)
		return;
	if (info->tb_prof.rate == 0 || info->installed)
		return;

	/* get the current token bucket regulator */
	if ((fd = open(ALTQ_DEVICE, O_RDWR)) < 0)
		err(1, "can't open altq device");
	strncpy(req.ifname, ifname, IFNAMSIZ-1);
	if (ioctl(fd, ALTQTBRGET, &req) < 0)
		err(1, "ALTQTBRGET for interface %s", req.ifname);

	/* save the current values */
	info->otb_prof.rate = req.tb_prof.rate;
	info->otb_prof.depth = req.tb_prof.depth;
	
	/*
	 * if tbr is not specified in the config file and tbr is already
	 * configured, do not change.
	 */
	if (req.tb_prof.rate != 0) {
		LOG(LOG_INFO, 0,
		    "tbr is already installed on %s,\n"
		    "  using the current setting (rate:%.2fM  size:%.2fK).",
		    info->ifname,
		    (double)req.tb_prof.rate/1000000.0,
		    (double)req.tb_prof.depth/1024.0);
		close (fd);
		return;
	}

	/* if the new size is not specified, use heuristics */
	if (info->tb_prof.depth == 0) {
		u_int rate, size;
		
		rate = info->tb_prof.rate;
		if (rate <= 1*1000*1000)
			size = 1;
		else if (rate <= 10*1000*1000)
			size = 4;
		else if (rate <= 200*1000*1000)
			size = 8;
		else
			size = 24;
		size = size * 1500;  /* assume the default mtu is 1500 */
		info->tb_prof.depth = size;
	}

	/* install the new tbr */
	strncpy(req.ifname, ifname, IFNAMSIZ-1);
	req.tb_prof.rate = info->tb_prof.rate;
	req.tb_prof.depth = info->tb_prof.depth;
	if (ioctl(fd, ALTQTBRSET, &req) < 0)
		err(1, "ALTQTBRSET for interface %s", req.ifname);
	LOG(LOG_INFO, 0,
	    "tbr installed on %s (rate:%.2fM  size:%.2fK)",
	    info->ifname,
	    (double)info->tb_prof.rate/1000000.0,
	    (double)info->tb_prof.depth/1024.0);
	close(fd);
	info->installed = 1;
}

static void
tbr_deinstall(const char *ifname)
{
	struct tbrinfo *info;
	struct tbrreq req;
	int fd;

	LIST_FOREACH(info, &tbr_list, link)
	    if (strcmp(info->ifname, ifname) == 0)
		    break;
	if (info == NULL)
		return;

	/* if we installed tbr, restore the old values */
	if (info->installed != 0) {
		strncpy(req.ifname, ifname, IFNAMSIZ-1);
		req.tb_prof.rate = info->otb_prof.rate;
		req.tb_prof.depth = info->otb_prof.depth;
		if ((fd = open(ALTQ_DEVICE, O_RDWR)) < 0)
			err(1, "can't open altq device");
		if (ioctl(fd, ALTQTBRSET, &req) < 0)
			err(1, "ALTQTBRSET for interface %s", req.ifname);
		close(fd);
	}
	LIST_REMOVE(info, link);
	free(info);
}

void
print_filter(const struct flow_filter *filt)
{
	if (filt->ff_flow.fi_family == AF_INET) {
		struct in_addr in_addr;

		in_addr.s_addr = filt->ff_flow.fi_dst.s_addr;
		LOG(LOG_DEBUG, 0,
		    " Filter Dest Addr: %s (mask %#x) Port: %d",
		    inet_ntoa(in_addr), ntoh32(filt->ff_mask.mask_dst.s_addr),
		    ntoh16(filt->ff_flow.fi_dport));
		in_addr.s_addr = filt->ff_flow.fi_src.s_addr;
		LOG(LOG_DEBUG, 0,
		    "        Src Addr: %s (mask %#x) Port: %d",
		    inet_ntoa(in_addr), ntoh32(filt->ff_mask.mask_src.s_addr),
		    ntoh16(filt->ff_flow.fi_sport));
		LOG(LOG_DEBUG, 0, "        Protocol: %d TOS %#x (mask %#x)",
		    filt->ff_flow.fi_proto, filt->ff_flow.fi_tos,
		    filt->ff_mask.mask_tos);
	}
#ifdef INET6
	else if (filt->ff_flow.fi_family == AF_INET6) {
		char str1[INET6_ADDRSTRLEN], str2[INET6_ADDRSTRLEN];
		const struct flow_filter6 *sfilt6;

		sfilt6 = (const struct flow_filter6 *)filt; 
		LOG(LOG_DEBUG, 0, "Filter6 Dest Addr: %s (mask %s) Port: %d",
		    inet_ntop(AF_INET6, &sfilt6->ff_flow6.fi6_dst,
			      str1, sizeof(str1)),
		    inet_ntop(AF_INET6, &sfilt6->ff_mask6.mask6_dst,
			      str2, sizeof(str2)),
		    ntoh16(sfilt6->ff_flow6.fi6_dport));
		LOG(LOG_DEBUG, 0, "        Src Addr: %s (mask %s) Port: %d",
		    inet_ntop(AF_INET6, &sfilt6->ff_flow6.fi6_src,
			      str1, sizeof(str1)),
		    inet_ntop(AF_INET6, &sfilt6->ff_mask6.mask6_src,
			      str2, sizeof(str2)),
		    ntoh16(sfilt6->ff_flow6.fi6_sport));
		LOG(LOG_DEBUG, 0, "        Protocol: %d TCLASS %#x (mask %#x)",
		    sfilt6->ff_flow6.fi6_proto, sfilt6->ff_flow6.fi6_tclass,
		    sfilt6->ff_mask6.mask6_tclass);
	}
#endif /* INET6 */
}

/*
 * functions to check the filter-rules.
 * when a new filter is added, we check the relation to the existing filters
 * and if some inconsistency is found, produce an error or a warning message.
 *
 * filter matching is performed from the head of the list.
 * let
 *    S: a set of packets that filter s matches
 *    T: a set of packets that filter t matches
 * filter relations are:
 *   disjoint: S ^ T = empty
 *   subset:   S <= T
 *   intersect: S ^ T = not empty
 *
 * a new filter is disjoint or subset of the existing filters --> ok
 * a new filter is superset of an existing filter --> order problem
 * a new filter intersect an existing filter --> warning
 *
 * port-intersect: a special case we don't make warning
 *      - intersection is only port numbers
 *	- one specifies src port and the other specifies dst port
 * there must be no packet with well-known port numbers in
 * both src and dst ports.  so this is ok.
 */

#define FILT_DISJOINT		1
#define FILT_SUBSET		2
#define FILT_SUPERSET		3
#define FILT_INTERSECT		4
#define FILT_PORTINTERSECT	5

static int 
add_filter_rule(struct ifinfo *ifinfo, struct fltrinfo *fltrinfo,
		struct fltrinfo **conflict)
{
	struct fltrinfo *fp, *front, *back, *prev = NULL;
	int relation;

	LIST_FOREACH(fp, &ifinfo->fltr_rules, nextrule) {
		if (fp->fltr.ff_ruleno > fltrinfo->fltr.ff_ruleno) {
			front = fp;
			back = fltrinfo;
			prev = fp;
		} else {
			front = fltrinfo;
			back = fp;
		}

		relation = filt_check_relation(&front->fltr, &back->fltr);

		switch (relation) {
		case FILT_SUBSET:
		case FILT_DISJOINT:
			/* OK */
			break;
		case FILT_SUPERSET:
			if (front->dontwarn == 0 && back->dontwarn == 0)
				LOG(LOG_ERR, 0,
				    "filters for \"%s\" at line %d and for \"%s\" at line %d has an order problem!", 
				    front->clinfo->clname, front->line_no,
				    back->clinfo->clname, back->line_no);

			if (conflict != NULL)
				*conflict = fp;
			return (QOPERR_FILTER_SHADOW);
		case FILT_PORTINTERSECT:
			break;
		case FILT_INTERSECT:
			/*
			 * if the intersecting two filters beloging to the
			 * same class, it's ok.
			 */
			if (front->clinfo == back->clinfo)
				break;
			if (front->dontwarn == 0 && back->dontwarn == 0)
				LOG(LOG_WARNING, 0,
				    "warning: filter for \"%s\" at line %d could override filter for \"%s\" at line %d", 
				    front->clinfo->clname, front->line_no,
				    back->clinfo->clname, back->line_no);
			break;
		}
	}

	if (prev == NULL)
		LIST_INSERT_HEAD(&ifinfo->fltr_rules, fltrinfo, nextrule);
	else
		LIST_INSERT_AFTER(prev, fltrinfo, nextrule);
	return (0);
}

static int 
remove_filter_rule(struct ifinfo *ifinfo, struct fltrinfo *fltrinfo)
{
	LIST_REMOVE(fltrinfo, nextrule);
	return (0);
}

static int
filt_check_relation(struct flow_filter *front, struct flow_filter *back)
{
	int rval;
	
	if (front->ff_flow.fi_family != back->ff_flow.fi_family)
		return (FILT_DISJOINT);

	if (filt_disjoint(front, back))
		return (FILT_DISJOINT);

	if ((rval = filt_subset(front, back)) == 1)
		return (FILT_SUBSET);

	if (filt_subset(back, front) == 1)
		return (FILT_SUPERSET);

	if (rval == 2)
		return (FILT_PORTINTERSECT);
	
	return (FILT_INTERSECT);
}

static int
filt_disjoint(struct flow_filter *front, struct flow_filter *back)
{
	u_int32_t mask;
	u_int8_t tosmask;
	
	if (front->ff_flow.fi_family == AF_INET) {
		if (front->ff_flow.fi_proto != 0 && back->ff_flow.fi_proto != 0
		    && front->ff_flow.fi_proto != back->ff_flow.fi_proto)
			return (1);
		if (front->ff_flow.fi_sport != 0 && back->ff_flow.fi_sport != 0
		    && front->ff_flow.fi_sport != back->ff_flow.fi_sport)
			return (1);
		if (front->ff_flow.fi_dport != 0 && back->ff_flow.fi_dport != 0
		    && front->ff_flow.fi_dport != back->ff_flow.fi_dport)
			return (1);
		if (front->ff_flow.fi_gpi != 0 && back->ff_flow.fi_gpi != 0
		    && front->ff_flow.fi_gpi != back->ff_flow.fi_gpi)
			return (1);
		if (front->ff_flow.fi_src.s_addr != 0 &&
		    back->ff_flow.fi_src.s_addr != 0) {
			mask = front->ff_mask.mask_src.s_addr &
				back->ff_mask.mask_src.s_addr;
			if ((front->ff_flow.fi_src.s_addr & mask) !=
			    (back->ff_flow.fi_src.s_addr & mask))
				return (1);
		}
		if (front->ff_flow.fi_dst.s_addr != 0 &&
		    back->ff_flow.fi_dst.s_addr != 0) {
			mask = front->ff_mask.mask_dst.s_addr &
				back->ff_mask.mask_dst.s_addr;
			if ((front->ff_flow.fi_dst.s_addr & mask) !=
			    (back->ff_flow.fi_dst.s_addr & mask))
				return (1);
		}
		if (front->ff_flow.fi_tos != 0 && back->ff_flow.fi_tos != 0) {
			tosmask = front->ff_mask.mask_tos &
				back->ff_mask.mask_tos;
			if ((front->ff_flow.fi_tos & tosmask) !=
			    (back->ff_flow.fi_tos & tosmask))
				return (1);
		}
		return (0);
	}
#ifdef INET6
	else if (front->ff_flow.fi_family == AF_INET6) {
		struct flow_filter6 *front6, *back6;
		int i;

		front6 = (struct flow_filter6 *)front;
		back6 = (struct flow_filter6 *)back;

		if (front6->ff_flow6.fi6_proto != 0 &&
		    back6->ff_flow6.fi6_proto != 0 &&
		    front6->ff_flow6.fi6_proto != back6->ff_flow6.fi6_proto)
			return (1);
		if (front6->ff_flow6.fi6_flowlabel != 0 &&
		    back6->ff_flow6.fi6_flowlabel != 0 &&
		    front6->ff_flow6.fi6_flowlabel !=
		    back6->ff_flow6.fi6_flowlabel)
			return (1);
		if (front6->ff_flow6.fi6_sport != 0 &&
		    back6->ff_flow6.fi6_sport != 0 &&
		    front6->ff_flow6.fi6_sport != back6->ff_flow6.fi6_sport)
			return (1);
		if (front6->ff_flow6.fi6_dport != 0 &&
		    back6->ff_flow6.fi6_dport != 0 &&
		    front6->ff_flow6.fi6_dport != back6->ff_flow6.fi6_dport)
			return (1);
		if (front6->ff_flow6.fi6_gpi != 0 &&
		    back6->ff_flow6.fi6_gpi != 0 &&
		    front6->ff_flow6.fi6_gpi != back6->ff_flow6.fi6_gpi)
			return (1);
		if (!IN6_IS_ADDR_UNSPECIFIED(&front6->ff_flow6.fi6_src) &&
		    !IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_src)) {
			for (i=0; i<4; i++) {
				mask = IN6ADDR32(&front6->ff_mask6.mask6_src, i)
					& IN6ADDR32(&back6->ff_mask6.mask6_src, i);
				if ((IN6ADDR32(&front6->ff_flow6.fi6_src, i) & mask) !=
				    (IN6ADDR32(&back6->ff_flow6.fi6_src, i) & mask))
					return (1);
			}
		}
		if (!IN6_IS_ADDR_UNSPECIFIED(&front6->ff_flow6.fi6_dst) &&
		    !IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_dst)) {
			for (i=0; i<4; i++) {
				mask = IN6ADDR32(&front6->ff_mask6.mask6_dst, i)
					& IN6ADDR32(&back6->ff_mask6.mask6_dst, i);
				if ((IN6ADDR32(&front6->ff_flow6.fi6_dst, i) & mask) !=
				    (IN6ADDR32(&back6->ff_flow6.fi6_dst, i) & mask))
				return (1);
			}
		}
		if (front6->ff_flow6.fi6_tclass != 0 &&
		    back6->ff_flow6.fi6_tclass != 0) {
			tosmask = front6->ff_mask6.mask6_tclass &
				back6->ff_mask6.mask6_tclass;
			if ((front6->ff_flow6.fi6_tclass & tosmask) !=
			    (back6->ff_flow6.fi6_tclass & tosmask))
				return (1);
		}
		return (0);
	}
#endif /* INET6 */
	return (0);
}

/*
 * check if "front" is a subset of "back".  assumes they are not disjoint
 * return value 0: not a subset
 *              1: subset
 *              2: subset except src & dst ports
 *		   (possible port-intersect)
 */
static int
filt_subset(struct flow_filter *front, struct flow_filter *back)
{
	u_int16_t srcport, dstport;
	
	if (front->ff_flow.fi_family == AF_INET) {
		if (front->ff_flow.fi_proto == 0 &&
		    back->ff_flow.fi_proto != 0)
			return (0);
		if (front->ff_flow.fi_gpi == 0 && back->ff_flow.fi_gpi != 0)
			return (0);
		if (front->ff_flow.fi_src.s_addr == 0) {
			if (back->ff_flow.fi_src.s_addr != 0)
				return (0);
		} else if (back->ff_flow.fi_src.s_addr != 0 &&
			 (~front->ff_mask.mask_src.s_addr &
			  back->ff_mask.mask_src.s_addr))
			return (0);
		if (front->ff_flow.fi_dst.s_addr == 0) {
			if (back->ff_flow.fi_dst.s_addr != 0)
				return (0);
		} else if (back->ff_flow.fi_dst.s_addr != 0 &&
			 (~front->ff_mask.mask_dst.s_addr &
			  back->ff_mask.mask_dst.s_addr))
			return (0);
		if (~front->ff_mask.mask_tos & back->ff_mask.mask_tos)
			return (0);

		if (front->ff_flow.fi_sport == 0 &&
		    back->ff_flow.fi_sport != 0) {
			srcport = ntohs(back->ff_flow.fi_sport);
			dstport = ntohs(front->ff_flow.fi_dport);
			if (dstport > 0 /* && dstport < 1024 */ &&
			    srcport > 0 /* && srcport < 1024 */)
				return (2);
			return (0);
		}
		if (front->ff_flow.fi_dport == 0 &&
		    back->ff_flow.fi_dport != 0) {
			dstport = ntohs(back->ff_flow.fi_dport);
			srcport = ntohs(front->ff_flow.fi_sport);
			if (srcport > 0 /* && srcport < 1024 */ &&
			    dstport > 0 /* && dstport < 1024 */)
				return (2);
			return (0);
		}
			
		return (1);
	}
#ifdef INET6
	else if (front->ff_flow.fi_family == AF_INET6) {
		struct flow_filter6 *front6, *back6;
		int i;
		
		front6 = (struct flow_filter6 *)front;
		back6 = (struct flow_filter6 *)back;

		if (front6->ff_flow6.fi6_proto == 0 &&
		    back6->ff_flow6.fi6_proto != 0)
			return (0);
		if (front6->ff_flow6.fi6_flowlabel == 0 &&
		    back6->ff_flow6.fi6_flowlabel != 0)
			return (0);
		if (front6->ff_flow6.fi6_gpi == 0 &&
		    back6->ff_flow6.fi6_gpi != 0)
			return (0);
		
		if (IN6_IS_ADDR_UNSPECIFIED(&front6->ff_flow6.fi6_src)) {
			if (!IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_src))
				return (0);
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_src))
			for (i=0; i<4; i++)
				if (~IN6ADDR32(&front6->ff_mask6.mask6_src, i) &
				    IN6ADDR32(&back6->ff_mask6.mask6_src, i))
					return (0);
		if (IN6_IS_ADDR_UNSPECIFIED(&front6->ff_flow6.fi6_dst)) {
			if (!IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_dst))
				return (0);
		} else if (!IN6_IS_ADDR_UNSPECIFIED(&back6->ff_flow6.fi6_dst))
			for (i=0; i<4; i++)
				if (~IN6ADDR32(&front6->ff_mask6.mask6_dst, i) &
				    IN6ADDR32(&back6->ff_mask6.mask6_dst, i))
					return (0);

		if (~front6->ff_mask6.mask6_tclass &
		    back6->ff_mask6.mask6_tclass)
			return (0);

		if (front6->ff_flow6.fi6_sport == 0 &&
		    back6->ff_flow6.fi6_sport != 0) {
			srcport = ntohs(back6->ff_flow6.fi6_sport);
			dstport = ntohs(front6->ff_flow6.fi6_dport);
			if (dstport > 0 /* && dstport < 1024 */ &&
			    srcport > 0 /* && srcport < 1024 */)
				return (2);
			return (0);
		}
		if (front6->ff_flow6.fi6_dport == 0 &&
		    back6->ff_flow6.fi6_dport != 0) {
			dstport = ntohs(back6->ff_flow6.fi6_dport);
			srcport = ntohs(front6->ff_flow6.fi6_sport);
			if (srcport > 0 /* && srcport < 1024 */ &&
			    dstport > 0 /* && dstport < 1024 */)
				return (2);
			return (0);
		}
	}
#endif /* INET6 */
	return (1);
}


/*
 * setting RED or RIO default parameters
 */
int
qop_red_set_defaults(int th_min, int th_max, int inv_pmax)
{
	struct redparams params;
	int fd;

	if ((fd = open(RED_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "RED open");
		return (QOPERR_SYSCALL);
	}

	params.th_min = th_min;
	params.th_max = th_max;
	params.inv_pmax = inv_pmax;

	if (ioctl(fd, RED_SETDEFAULTS, &params) < 0) {
		LOG(LOG_ERR, errno, "RED_SETDEFAULTS");
		return (QOPERR_SYSCALL);
	}

	(void)close(fd);
	return (0);
}

int
qop_rio_set_defaults(struct redparams *params)
{
	int i, fd;

	/* sanity check */
	for (i = 1; i < RIO_NDROPPREC; i++) {
		if (params[i].th_max > params[i-1].th_min)
			LOG(LOG_WARNING, 0,
			    "warning: overlap found in RIO thresholds");
	}

	if ((fd = open(RIO_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "RIO open");
		return (QOPERR_SYSCALL);
	}

	if (ioctl(fd, RIO_SETDEFAULTS, params) < 0) {
		LOG(LOG_ERR, errno, "RIO_SETDEFAULTS");
		return (QOPERR_SYSCALL);
	}

	(void)close(fd);
	return (0);
}

/*
 * try to load and open KLD module
 * (also check the altq device file)
 */
int
open_module(const char *devname, int flags)
{
#if defined(__FreeBSD__) && (__FreeBSD_version > 300000)
	char modname[64], filename[MAXPATHLEN], *cp;
	int fd;
#endif
	struct stat sbuf;

	/* check if the altq device exists */
	if (stat(devname, &sbuf) < 0) {
		LOG(LOG_ERR, errno, "can't access %s!", devname);
		return (-1);
	}

#if defined(__FreeBSD__) && (__FreeBSD_version > 300000)
	/* turn discipline name into module name */
	strlcpy(modname, "altq_", sizeof(modname));
	if ((cp = strrchr(devname, '/')) == NULL)
		return (-1);
	strlcat(modname, cp + 1, sizeof(modname));

	/* check if the kld module exists */
	snprintf(filename, sizeof(filename), "/modules/%s.ko", modname);
	if (stat(filename, &sbuf) < 0) {
		/* module file doesn't exist */
		return (-1);
	}

	if (kldload(modname) < 0) {
		LOG(LOG_ERR, errno, "kldload %s failed!", modname);
		return (-1);
	}

	/* successfully loaded, open the device */
	LOG(LOG_INFO, 0, "kld module %s loaded", modname);
	fd = open(devname, flags);
	return (fd);
#else
	return (-1);
#endif
}
