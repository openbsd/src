/*	$OpenBSD: qop_cdnr.c,v 1.2 2001/08/16 12:59:43 kjc Exp $	*/
/*	$KAME: qop_cdnr.c,v 1.9 2001/08/16 10:39:14 kjc Exp $	*/
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
#include <altq/altq_cdnr.h>
#include "altq_qop.h"
#include "qop_cdnr.h"
/*
 * diffserve traffic conditioner support
 *
 * we use the existing qop interface to support conditioner.
 */

static struct ifinfo *cdnr_ifname2ifinfo(const char *);
static int cdnr_attach(struct ifinfo *);
static int cdnr_detach(struct ifinfo *);
static int cdnr_enable(struct ifinfo *);
static int cdnr_disable(struct ifinfo *);
static int cdnr_add_class(struct classinfo *);
static int cdnr_modify_class(struct classinfo *, void *);
static int cdnr_delete_class(struct classinfo *);
static int cdnr_add_filter(struct fltrinfo *);
static int cdnr_delete_filter(struct fltrinfo *);
static int verify_tbprofile(struct tb_profile *, const char *);

#define CDNR_DEVICE	"/dev/altq/cdnr"

static int cdnr_fd = -1;
static int cdnr_refcount = 0;

static struct qdisc_ops cdnr_qdisc = {
	ALTQT_CDNR,
	"cdnr",
	cdnr_attach,
	cdnr_detach,
	NULL,			/* clear */
	cdnr_enable,
	cdnr_disable,
	cdnr_add_class,
	cdnr_modify_class,
	cdnr_delete_class,
	cdnr_add_filter,
	cdnr_delete_filter,
};

u_long
cdnr_name2handle(const char *ifname, const char *cdnr_name)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (CDNR_NULL_HANDLE);

	if ((clinfo = clname2clinfo(ifinfo, cdnr_name)) == NULL)
		return (CDNR_NULL_HANDLE);

	return (clinfo->handle);
}

static struct ifinfo *
cdnr_ifname2ifinfo(const char *ifname)
{
	struct ifinfo	*ifinfo;
	char input_ifname[64];

	/*
	 * search for an existing input interface
	 */
	if ((ifinfo = input_ifname2ifinfo(ifname)) != NULL)
		return (ifinfo);

	/*
	 * if there is a corresponding output interface,
	 * create an input interface by prepending "_" to
	 * its name.
	 */
	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (NULL);

	input_ifname[0] = '_';
	strlcpy(input_ifname+1, ifname, sizeof(input_ifname)-1);
	if (qop_add_if(&ifinfo, input_ifname, 0, &cdnr_qdisc, NULL) != 0) {
		LOG(LOG_ERR, errno,
		    "cdnr_ifname2ifinfo: can't add a input interface %s",
		    ifname);
		return (NULL);
	}
	return (ifinfo);
}

int
qcmd_cdnr_add_element(struct tc_action *rp, const char *ifname,
		   const char *cdnr_name, struct tc_action *action)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if ((error = qop_cdnr_add_element(&clinfo, cdnr_name, ifinfo,
					  action)) != 0) {
		LOG(LOG_ERR, errno, "%s: add element failed!",
		    qoperror(error));
		return (error);
	}

	if (rp != NULL) {
		rp->tca_code = TCACODE_HANDLE;
		rp->tca_handle = clinfo->handle;
	}
	return (0);
}

int
qcmd_cdnr_add_tbmeter(struct tc_action *rp, const char *ifname,
		      const char *cdnr_name, 
		      struct tb_profile *profile,
		      struct tc_action *in_action,
		      struct tc_action *out_action)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	verify_tbprofile(profile, cdnr_name);

	if ((error = qop_cdnr_add_tbmeter(&clinfo, cdnr_name, ifinfo,
				  profile, in_action, out_action)) != 0) {
		LOG(LOG_ERR, errno, "%s: add tbmeter failed!",
		    qoperror(error));
		return (error);
	}
	
	if (rp != NULL) {
		rp->tca_code = TCACODE_HANDLE;
		rp->tca_handle = clinfo->handle;
	}
	return (0);
}

int
qcmd_cdnr_add_trtcm(struct tc_action *rp, const char *ifname,
		    const char *cdnr_name, 
		    struct tb_profile *cmtd_profile,
		    struct tb_profile *peak_profile,
		    struct tc_action *green_action,
		    struct tc_action *yellow_action,
		    struct tc_action *red_action, int coloraware)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	verify_tbprofile(cmtd_profile, cdnr_name);
	verify_tbprofile(peak_profile, cdnr_name);

	if ((error = qop_cdnr_add_trtcm(&clinfo, cdnr_name, ifinfo,
			  cmtd_profile, peak_profile,
			  green_action, yellow_action, red_action,
	     		  coloraware)) != 0) {
		LOG(LOG_ERR, errno, "%s: add trtcm failed!",
		    qoperror(error));
		return (error);
	}
	
	if (rp != NULL) {
		rp->tca_code = TCACODE_HANDLE;
		rp->tca_handle = clinfo->handle;
	}
	return (0);
}

int
qcmd_cdnr_add_tswtcm(struct tc_action *rp, const char *ifname,
		     const char *cdnr_name, const u_int32_t cmtd_rate,
		     const u_int32_t peak_rate, const u_int32_t avg_interval,
		     struct tc_action *green_action,
		     struct tc_action *yellow_action,
		     struct tc_action *red_action)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;
	int error;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if (cmtd_rate > peak_rate) {
		LOG(LOG_ERR, 0,
		    "add tswtcm: cmtd_rate larger than peak_rate!");
		return (QOPERR_INVAL);
	}

	if ((error = qop_cdnr_add_tswtcm(&clinfo, cdnr_name, ifinfo,
					cmtd_rate, peak_rate, avg_interval,
					green_action, yellow_action,
					red_action)) != 0) {
		LOG(LOG_ERR, errno, "%s: add tswtcm failed!",
		    qoperror(error));
		return (error);
	}
	
	if (rp != NULL) {
		rp->tca_code = TCACODE_HANDLE;
		rp->tca_handle = clinfo->handle;
	}
	return (0);
}

int
qcmd_cdnr_delete(const char *ifname, const char *cdnr_name)
{
	struct ifinfo		*ifinfo;
	struct classinfo	*clinfo;

	if ((ifinfo = cdnr_ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if ((clinfo = clname2clinfo(ifinfo, cdnr_name)) == NULL)
		return (QOPERR_BADCLASS);

	return qop_delete_cdnr(clinfo);
}

/*
 * class operations:
 *	class structure is used to hold conditioners.
 *	XXX
 *	conditioners has dependencies in the reverse order; parent nodes
 *	refere to child nodes, and thus, a child is created first and
 *	parents should be removed first.
 *	qop_add_cdnr() and qop_delete_cdnr() are wrapper functions
 *	of qop_add_class() and qop_delete_class(), and takes care
 *	of dependencies.
 *	1. when adding a conditioner, it is created as a child of a
 *	   dummy root class.  then, the child conditioners are made
 *	   as its children.
 *	2. when deleting a conditioner, its child conditioners are made
 *	   as children of the dummy root class.  then, the conditioner
 *	   is deleted.
 */

int
qop_add_cdnr(struct classinfo **rp, const char *cdnr_name,
	     struct ifinfo *ifinfo, struct classinfo **childlist,
	     void *cdnr_private)
{
	struct classinfo	*clinfo, *root, *cl, *prev;
	int error;

	/*
	 * if there is no root cdnr, create one.
	 */
	if ((root = get_rootclass(ifinfo)) == NULL) {
		if ((error = qop_add_class(&root, "cdnr_root",
					   ifinfo, NULL, NULL)) != 0) {
			LOG(LOG_ERR, errno,
			    "cdnr: %s: can't create dummy root cdnr on %s!",
			    qoperror(error), ifinfo->ifname);
			return (QOPERR_CLASS);
		}
	}

	/*
	 * create a class as a child of a root class.
	 */
	if ((error = qop_add_class(&clinfo, cdnr_name,
				   ifinfo, root, cdnr_private)) != 0)
		return (error);
	/*
	 * move child nodes
	 */
	for (cl = *childlist; cl != NULL; cl = *++childlist) {
		if (cl->parent != root) {
			/*
			 * this conditioner already has a non-root parent.
			 * we can't track down a multi-parent node by a
			 * tree structure; leave it as it is.
			 * (we need a mechanism similar to a symbolic link
			 * in a file system)
			 */
			continue;
		}
		/* remove this child from the root */
		if (root->child == cl)
			root->child = cl->sibling;
		else for (prev = root->child;
			  prev->sibling != NULL; prev = prev->sibling)
			if (prev->sibling == cl) {
				prev->sibling = cl->sibling;
				break;
			}

		/* add as a child */
		cl->sibling = clinfo->child;
		clinfo->child = cl;
		cl->parent = clinfo;
	}
		
	if (rp != NULL)
		*rp = clinfo;
	return (0);
}

int
qop_delete_cdnr(struct classinfo *clinfo)
{
	struct classinfo *cl, *root;
	int error;

	if ((root = get_rootclass(clinfo->ifinfo)) == NULL) {
		LOG(LOG_ERR, 0, "qop_delete_cdnr: no root cdnr!");
		return (QOPERR_CLASS);
	}

	if (clinfo->parent != root)
		return (QOPERR_CLASS_PERM);

	if ((cl = clinfo->child) != NULL) {
		/* change child's parent to root, find the last child */
		while (cl->sibling != NULL) {
			cl->parent = root;
			cl = cl->sibling;
		}
		cl->parent = root;

		/* move children to siblings */
		cl->sibling = clinfo->sibling;
		clinfo->sibling = cl;
		clinfo->child = NULL;
	}

	error = qop_delete_class(clinfo);

	if (error) {
		/* ick! restore the class tree */
		if (cl != NULL) {
			clinfo->child = clinfo->sibling;
			clinfo->sibling = cl->sibling;
			cl->sibling = NULL;
			/* restore parent field */
			for (cl = clinfo->child; cl != NULL; cl = cl->sibling)
				cl->parent = clinfo;
		}
	}
	return (error);
}

int 
qop_cdnr_add_element(struct classinfo **rp, const char *cdnr_name,
		     struct ifinfo *ifinfo, struct tc_action *action)
{
	struct classinfo *clinfo, *clist[2];
	struct cdnrinfo *cdnrinfo = NULL;
	int error;

	if (action->tca_code == TCACODE_HANDLE) {
		clinfo = clhandle2clinfo(ifinfo, action->tca_handle);
		if (clinfo == NULL)
			return (QOPERR_BADCLASS);
		clist[0] = clinfo;
		clist[1] = NULL;
#if 1
		/*
		 * if the conditioner referred to doesn't have a name,
		 * this is called just to add a name to it.
		 * we can simply add the name to the existing conditioner
		 * and return it.
		 */
		if (cdnr_name != NULL &&
		    strcmp(clinfo->clname, "(null)") == 0) {
			free(clinfo->clname);
			clinfo->clname = strdup(cdnr_name);
			if (rp != NULL)
				*rp = clinfo;
			return (0);
		}
#endif
	} else
		clist[0] = NULL;

	if ((cdnrinfo = calloc(1, sizeof(*cdnrinfo))) == NULL)
		return (QOPERR_NOMEM);

	cdnrinfo->tce_type = TCETYPE_ELEMENT;
	cdnrinfo->tce_un.element.action = *action;

	if ((error = qop_add_cdnr(&clinfo, cdnr_name, ifinfo, clist,
				  cdnrinfo)) != 0)
		goto err_ret;

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (cdnrinfo != NULL)
		free(cdnrinfo);
	return (error);
}

int 
qop_cdnr_add_tbmeter(struct classinfo **rp, const char *cdnr_name,
		     struct ifinfo *ifinfo,
		     struct tb_profile *profile,
		     struct tc_action *in_action,
		     struct tc_action *out_action)
{
	struct classinfo *clinfo, *clist[3];
	struct cdnrinfo *cdnrinfo = NULL;
	int n, error;

	n = 0;
	if (in_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, in_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	if (out_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, out_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	clist[n] = NULL;

	if ((cdnrinfo = calloc(1, sizeof(*cdnrinfo))) == NULL)
		return (QOPERR_NOMEM);

	cdnrinfo->tce_type = TCETYPE_TBMETER;
	cdnrinfo->tce_un.tbmeter.profile = *profile;
	cdnrinfo->tce_un.tbmeter.in_action = *in_action;
	cdnrinfo->tce_un.tbmeter.out_action = *out_action;

	if ((error = qop_add_cdnr(&clinfo, cdnr_name, ifinfo, clist,
				   cdnrinfo)) != 0)
		goto err_ret;

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (cdnrinfo != NULL)
		free(cdnrinfo);
	return (error);
}

int 
qop_cdnr_modify_tbmeter(struct classinfo *clinfo, struct tb_profile *profile)
{
	struct cdnrinfo *cdnrinfo = clinfo->private;

	if (cdnrinfo->tce_type != TCETYPE_TBMETER)
		return (QOPERR_CLASS_INVAL);
	cdnrinfo->tce_un.tbmeter.profile = *profile;

	return qop_modify_class(clinfo, NULL);
}

int 
qop_cdnr_add_trtcm(struct classinfo **rp, const char *cdnr_name,
		   struct ifinfo *ifinfo,
		   struct tb_profile *cmtd_profile,
		   struct tb_profile *peak_profile,
		   struct tc_action *green_action,
		   struct tc_action *yellow_action,
		   struct tc_action *red_action, int coloraware)
{
	struct classinfo *clinfo, *clist[4];
	struct cdnrinfo *cdnrinfo = NULL;
	int n, error;

	n = 0;
	if (green_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, green_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	if (yellow_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, yellow_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	if (red_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, yellow_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	clist[n] = NULL;

	if ((cdnrinfo = calloc(1, sizeof(*cdnrinfo))) == NULL)
		return (QOPERR_NOMEM);

	cdnrinfo->tce_type = TCETYPE_TRTCM;
	cdnrinfo->tce_un.trtcm.cmtd_profile = *cmtd_profile;
	cdnrinfo->tce_un.trtcm.peak_profile = *peak_profile;
	cdnrinfo->tce_un.trtcm.green_action = *green_action;
	cdnrinfo->tce_un.trtcm.yellow_action = *yellow_action;
	cdnrinfo->tce_un.trtcm.red_action = *red_action;
	cdnrinfo->tce_un.trtcm.coloraware = coloraware;

	if ((error = qop_add_cdnr(&clinfo, cdnr_name, ifinfo, clist,
				  cdnrinfo)) != 0)
		goto err_ret;

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (cdnrinfo != NULL)
		free(cdnrinfo);
	return (error);
}

int 
qop_cdnr_modify_trtcm(struct classinfo *clinfo,
		      struct tb_profile *cmtd_profile,
		      struct tb_profile *peak_profile, int coloraware)
{
	struct cdnrinfo *cdnrinfo = clinfo->private;

	if (cdnrinfo->tce_type != TCETYPE_TRTCM)
		return (QOPERR_CLASS_INVAL);
	cdnrinfo->tce_un.trtcm.cmtd_profile = *cmtd_profile;
	cdnrinfo->tce_un.trtcm.peak_profile = *peak_profile;
	cdnrinfo->tce_un.trtcm.coloraware = coloraware;

	return qop_modify_class(clinfo, NULL);
}

int 
qop_cdnr_add_tswtcm(struct classinfo **rp, const char *cdnr_name,
		    struct ifinfo *ifinfo, const u_int32_t cmtd_rate,
		    const u_int32_t peak_rate, const u_int32_t avg_interval,
		    struct tc_action *green_action,
		    struct tc_action *yellow_action,
		    struct tc_action *red_action)
{
	struct classinfo *clinfo, *clist[4];
	struct cdnrinfo *cdnrinfo = NULL;
	int n, error;

	n = 0;
	if (green_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, green_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	if (yellow_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, yellow_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	if (red_action->tca_code == TCACODE_HANDLE) {
		clist[n] = clhandle2clinfo(ifinfo, yellow_action->tca_handle);
		if (clist[n] == NULL)
			return (QOPERR_BADCLASS);
		n++;
	}
	clist[n] = NULL;

	if ((cdnrinfo = calloc(1, sizeof(*cdnrinfo))) == NULL)
		return (QOPERR_NOMEM);

	cdnrinfo->tce_type = TCETYPE_TSWTCM;
	cdnrinfo->tce_un.tswtcm.cmtd_rate = cmtd_rate;
	cdnrinfo->tce_un.tswtcm.peak_rate = peak_rate;
	cdnrinfo->tce_un.tswtcm.avg_interval = avg_interval;
	cdnrinfo->tce_un.tswtcm.green_action = *green_action;
	cdnrinfo->tce_un.tswtcm.yellow_action = *yellow_action;
	cdnrinfo->tce_un.tswtcm.red_action = *red_action;

	if ((error = qop_add_cdnr(&clinfo, cdnr_name, ifinfo, clist,
				  cdnrinfo)) != 0)
		goto err_ret;

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (cdnrinfo != NULL)
		free(cdnrinfo);
	return (error);
}

int 
qop_cdnr_modify_tswtcm(struct classinfo *clinfo, const u_int32_t cmtd_rate,
		       const u_int32_t peak_rate, const u_int32_t avg_interval)
{
	struct cdnrinfo *cdnrinfo = clinfo->private;

	if (cdnrinfo->tce_type != TCETYPE_TSWTCM)
		return (QOPERR_CLASS_INVAL);
	cdnrinfo->tce_un.tswtcm.cmtd_rate = cmtd_rate;
	cdnrinfo->tce_un.tswtcm.peak_rate = peak_rate;
	cdnrinfo->tce_un.tswtcm.avg_interval = avg_interval;
	
	return qop_modify_class(clinfo, NULL);
}

/*
 *  system call interfaces for qdisc_ops
 */
static int
cdnr_attach(struct ifinfo *ifinfo)
{
	struct cdnr_interface iface;

	if (cdnr_fd < 0 &&
	    (cdnr_fd = open(CDNR_DEVICE, O_RDWR)) < 0 &&
	    (cdnr_fd = open_module(CDNR_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "CDNR open");
		return (QOPERR_SYSCALL);
	}

	cdnr_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cdnr_ifname, ifinfo->ifname+1, IFNAMSIZ);

	if (ioctl(cdnr_fd, CDNR_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);
#if 1
	LOG(LOG_INFO, 0, "conditioner attached to %s", iface.cdnr_ifname);
#endif
	return (0);
}

static int
cdnr_detach(struct ifinfo *ifinfo)
{
	struct cdnr_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cdnr_ifname, ifinfo->ifname+1, IFNAMSIZ);

	if (ioctl(cdnr_fd, CDNR_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--cdnr_refcount == 0) {
		close(cdnr_fd);
		cdnr_fd = -1;
	}
	return (0);
}

static int
cdnr_enable(struct ifinfo *ifinfo)
{
	struct cdnr_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cdnr_ifname, ifinfo->ifname+1, IFNAMSIZ);

	if (ioctl(cdnr_fd, CDNR_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cdnr_disable(struct ifinfo *ifinfo)
{
	struct cdnr_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cdnr_ifname, ifinfo->ifname+1, IFNAMSIZ);

	if (ioctl(cdnr_fd, CDNR_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cdnr_add_class(struct classinfo *clinfo)
{
	struct cdnr_add_element element_add;
	struct cdnr_add_tbmeter tbmeter_add;
	struct cdnr_add_trtcm   trtcm_add;
	struct cdnr_add_tswtcm  tswtcm_add;
	struct cdnrinfo *cdnrinfo;
	
	cdnrinfo = clinfo->private;

	/* root class is a dummy class */
	if (clinfo->parent == NULL) {
		clinfo->handle = 0;
		return (0);
	}

	switch (cdnrinfo->tce_type) {
	case TCETYPE_ELEMENT:
		memset(&element_add, 0, sizeof(element_add));
		strncpy(element_add.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		element_add.action = cdnrinfo->tce_un.element.action;
		if (ioctl(cdnr_fd, CDNR_ADD_ELEM, &element_add) < 0) {
			clinfo->handle = CDNR_NULL_HANDLE;
			return (QOPERR_SYSCALL);
		}
		clinfo->handle = element_add.cdnr_handle;
		break;

	case TCETYPE_TBMETER:
		memset(&tbmeter_add, 0, sizeof(tbmeter_add));
		strncpy(tbmeter_add.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		tbmeter_add.profile = cdnrinfo->tce_un.tbmeter.profile;
		tbmeter_add.in_action = cdnrinfo->tce_un.tbmeter.in_action;
		tbmeter_add.out_action = cdnrinfo->tce_un.tbmeter.out_action;
		if (ioctl(cdnr_fd, CDNR_ADD_TBM, &tbmeter_add) < 0) {
			clinfo->handle = CDNR_NULL_HANDLE;
			return (QOPERR_SYSCALL);
		}
		clinfo->handle = tbmeter_add.cdnr_handle;
		break;

	case TCETYPE_TRTCM:
		memset(&trtcm_add, 0, sizeof(trtcm_add));
		strncpy(trtcm_add.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		trtcm_add.cmtd_profile = cdnrinfo->tce_un.trtcm.cmtd_profile;
		trtcm_add.peak_profile = cdnrinfo->tce_un.trtcm.peak_profile;
		trtcm_add.green_action = cdnrinfo->tce_un.trtcm.green_action;
		trtcm_add.yellow_action = cdnrinfo->tce_un.trtcm.yellow_action;
		trtcm_add.red_action = cdnrinfo->tce_un.trtcm.red_action;
		trtcm_add.coloraware = cdnrinfo->tce_un.trtcm.coloraware;
		if (ioctl(cdnr_fd, CDNR_ADD_TCM, &trtcm_add) < 0) {
			clinfo->handle = CDNR_NULL_HANDLE;
			return (QOPERR_SYSCALL);
		}
		clinfo->handle = trtcm_add.cdnr_handle;
		break;

	case TCETYPE_TSWTCM:
		memset(&tswtcm_add, 0, sizeof(tswtcm_add));
		strncpy(tswtcm_add.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		tswtcm_add.cmtd_rate = cdnrinfo->tce_un.tswtcm.cmtd_rate;
		tswtcm_add.peak_rate = cdnrinfo->tce_un.tswtcm.peak_rate;
		tswtcm_add.avg_interval = cdnrinfo->tce_un.tswtcm.avg_interval;
		tswtcm_add.green_action = cdnrinfo->tce_un.tswtcm.green_action;
		tswtcm_add.yellow_action = cdnrinfo->tce_un.tswtcm.yellow_action;
		tswtcm_add.red_action = cdnrinfo->tce_un.tswtcm.red_action;
		if (ioctl(cdnr_fd, CDNR_ADD_TSW, &tswtcm_add) < 0) {
			clinfo->handle = CDNR_NULL_HANDLE;
			return (QOPERR_SYSCALL);
		}
		clinfo->handle = tswtcm_add.cdnr_handle;
		break;

	default:
		return (QOPERR_CLASS_INVAL);
	}
	return (0);
}

static int
cdnr_modify_class(struct classinfo *clinfo, void *arg)
{
	struct cdnr_modify_tbmeter tbmeter_modify;
	struct cdnr_modify_trtcm   trtcm_modify;
	struct cdnr_modify_tswtcm  tswtcm_modify;
	struct cdnrinfo *cdnrinfo;

	cdnrinfo = clinfo->private;

	switch (cdnrinfo->tce_type) {
	case TCETYPE_TBMETER:
		memset(&tbmeter_modify, 0, sizeof(tbmeter_modify));
		strncpy(tbmeter_modify.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		tbmeter_modify.cdnr_handle = clinfo->handle;
		tbmeter_modify.profile = cdnrinfo->tce_un.tbmeter.profile;
		if (ioctl(cdnr_fd, CDNR_MOD_TBM, &tbmeter_modify) < 0)
			return (QOPERR_SYSCALL);
		break;

	case TCETYPE_TRTCM:
		memset(&trtcm_modify, 0, sizeof(trtcm_modify));
		strncpy(trtcm_modify.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		trtcm_modify.cdnr_handle = clinfo->handle;
		trtcm_modify.cmtd_profile =
			cdnrinfo->tce_un.trtcm.cmtd_profile;
		trtcm_modify.peak_profile =
			cdnrinfo->tce_un.trtcm.peak_profile;
		trtcm_modify.coloraware = cdnrinfo->tce_un.trtcm.coloraware;
		if (ioctl(cdnr_fd, CDNR_MOD_TCM, &trtcm_modify) < 0)
			return (QOPERR_SYSCALL);
		break;

	case TCETYPE_TSWTCM:
		memset(&tswtcm_modify, 0, sizeof(tswtcm_modify));
		strncpy(tswtcm_modify.iface.cdnr_ifname,
			clinfo->ifinfo->ifname+1, IFNAMSIZ);
		tswtcm_modify.cdnr_handle = clinfo->handle;
		tswtcm_modify.cmtd_rate = cdnrinfo->tce_un.tswtcm.cmtd_rate;
		tswtcm_modify.peak_rate = cdnrinfo->tce_un.tswtcm.peak_rate;
		tswtcm_modify.avg_interval = cdnrinfo->tce_un.tswtcm.avg_interval;
		if (ioctl(cdnr_fd, CDNR_MOD_TSW, &tswtcm_modify) < 0)
			return (QOPERR_SYSCALL);
		break;

	default:
		return (QOPERR_CLASS_INVAL);
	}
	return (0);
}

static int
cdnr_delete_class(struct classinfo *clinfo)
{
	struct cdnr_delete_element element_delete;

	if (clinfo->handle == CDNR_NULL_HANDLE)
		return (0);

	memset(&element_delete, 0, sizeof(element_delete));
	strncpy(element_delete.iface.cdnr_ifname, clinfo->ifinfo->ifname+1,
		IFNAMSIZ);
	element_delete.cdnr_handle = clinfo->handle;

	if (ioctl(cdnr_fd, CDNR_DEL_ELEM, &element_delete) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cdnr_add_filter(struct fltrinfo *fltrinfo)
{
	struct cdnr_add_filter fltr_add;
	
	memset(&fltr_add, 0, sizeof(fltr_add));
	strncpy(fltr_add.iface.cdnr_ifname,
		fltrinfo->clinfo->ifinfo->ifname+1, IFNAMSIZ);
	fltr_add.cdnr_handle = fltrinfo->clinfo->handle;
	fltr_add.filter = fltrinfo->fltr;

	if (ioctl(cdnr_fd, CDNR_ADD_FILTER, &fltr_add) < 0)
		return (QOPERR_SYSCALL);
	fltrinfo->handle = fltr_add.filter_handle;
	return (0);
}

static int
cdnr_delete_filter(struct fltrinfo *fltrinfo)
{
	struct cdnr_delete_filter fltr_del;

	memset(&fltr_del, 0, sizeof(fltr_del));
	strncpy(fltr_del.iface.cdnr_ifname,
		fltrinfo->clinfo->ifinfo->ifname+1, IFNAMSIZ);
	fltr_del.filter_handle = fltrinfo->handle;

	if (ioctl(cdnr_fd, CDNR_DEL_FILTER, &fltr_del) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}


static int 
verify_tbprofile(struct tb_profile *profile, const char *cdnr_name)
{
	if (profile->depth < 1500) {
		LOG(LOG_WARNING, 0,
		    "warning: token bucket depth for %s is too small (%d)",
		    cdnr_name, profile->depth);
		return (-1);
	}
	return (0);
}

