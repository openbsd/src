/*	$OpenBSD: qop_cbq.c,v 1.3 2001/12/03 08:38:48 kjc Exp $	*/
/*	$KAME: qop_cbq.c,v 1.5 2001/08/16 10:39:14 kjc Exp $	*/
/*
 * Copyright (c) Sun Microsystems, Inc. 1993-1998 All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the SMCC Technology
 *      Development Group at Sun Microsystems, Inc.
 *
 * 4. The name of the Sun Microsystems, Inc nor may not be used to endorse or
 *      promote products derived from this software without specific prior
 *      written permission.
 *
 * SUN MICROSYSTEMS DOES NOT CLAIM MERCHANTABILITY OF THIS SOFTWARE OR THE
 * SUITABILITY OF THIS SOFTWARE FOR ANY PARTICULAR PURPOSE.  The software is
 * provided "as is" without express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
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
#include <math.h>

#include <altq/altq.h>
#include <altq/altq_cbq.h>
#include "altq_qop.h"
#include "qop_cbq.h"

static int qcmd_cbq_add_ctl_filters(const char *, const char *);

static int qop_cbq_enable_hook(struct ifinfo *);
static int qop_cbq_delete_class_hook(struct classinfo *);

static int cbq_class_spec(struct ifinfo *, u_long, u_long, u_int, int,
			  u_int, u_int, u_int, u_int, u_int,
			  u_int, cbq_class_spec_t *);

static int cbq_attach(struct ifinfo *);
static int cbq_detach(struct ifinfo *);
static int cbq_clear(struct ifinfo *);
static int cbq_enable(struct ifinfo *);
static int cbq_disable(struct ifinfo *);
static int cbq_add_class(struct classinfo *);
static int cbq_modify_class(struct classinfo *, void *);
static int cbq_delete_class(struct classinfo *);
static int cbq_add_filter(struct fltrinfo *);
static int cbq_delete_filter(struct fltrinfo *);

#define CTL_PBANDWIDTH	2
#define NS_PER_MS	(1000000.0)
#define NS_PER_SEC	(NS_PER_MS*1000.0)
#define RM_FILTER_GAIN	5

#define CBQ_DEVICE	"/dev/altq/cbq"

static int cbq_fd = -1;
static int cbq_refcount = 0;

static struct qdisc_ops cbq_qdisc = {
	ALTQT_CBQ,
	"cbq",
	cbq_attach,
	cbq_detach,
	cbq_clear,
	cbq_enable,
	cbq_disable,
	cbq_add_class,
	cbq_modify_class,
	cbq_delete_class,
	cbq_add_filter,
	cbq_delete_filter,
};

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

/*
 * parser interface
 */
int
cbq_interface_parser(const char *ifname, int argc, char **argv)
{
	u_int  	bandwidth = 100000000;	/* 100Mbps */
	u_int	tbrsize = 0;
	u_int	is_efficient = 0;
	u_int	is_wrr = 1;	/* weighted round-robin is default */

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
		} else if (EQUAL(*argv, "efficient")) {
			is_efficient = 1;
		} else if (EQUAL(*argv, "cbq")) {
			/* just skip */
		} else if (EQUAL(*argv, "cbq-wrr")) {
			is_wrr = 1;
		} else if (EQUAL(*argv, "cbq-prr")) {
			is_wrr = 0;
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	if (qcmd_cbq_add_if(ifname, bandwidth,
			    is_wrr, is_efficient) != 0)
		return (0);
	return (1);
}

int
cbq_class_parser(const char *ifname, const char *class_name,
		 const char *parent_name, int argc, char **argv)
{
	const char	*borrow = NULL;
	u_int   pri = 1;
	u_int	pbandwidth = 0;
	u_int	bandwidth = 0;
	u_int	maxdelay = 0;		/* 0 means default */
	u_int	maxburst = 0;		/* 0 means default */
	u_int	minburst = 0;		/* 0 means default */
	u_int	av_pkt_size = 0;  /* 0 means use if mtu as default */
	u_int	max_pkt_size = 0; /* 0 means use if mtu as default */
	int	flags = 0;
	cbq_tos_t	admission_type = CBQ_QOS_NONE;
	int	error;

	if (parent_name == NULL)
		flags |= CBQCLF_ROOTCLASS;

	while (argc > 0) {
		if (EQUAL(*argv, "priority")) {
			argc--; argv++;
			if (argc > 0)
				pri = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "default")) {
			flags |= CBQCLF_DEFCLASS;
		} else if (EQUAL(*argv, "control")) {
			flags |= CBQCLF_CTLCLASS;
		} else if (EQUAL(*argv, "admission")) {
			argc--; argv++;
			if (argc > 0) {
				if (EQUAL(*argv, "guaranteed"))
					admission_type = CBQ_QOS_GUARANTEED;
				else if (EQUAL(*argv, "predictive"))
					admission_type = CBQ_QOS_PREDICTIVE;
				else if (EQUAL(*argv, "cntlload"))
					admission_type = CBQ_QOS_CNTR_LOAD;
				else if (EQUAL(*argv, "cntldelay"))
					admission_type = CBQ_QOS_CNTR_DELAY;
				else if (EQUAL(*argv, "none"))
					admission_type = CBQ_QOS_NONE;
				else {
					LOG(LOG_ERR, 0,
					    "unknown admission type - %s, line %d",
					    *argv, line_no);
					return (0);
				}
			}
		} else if (EQUAL(*argv, "maxdelay")) {
			argc--; argv++;
			if (argc > 0)
				maxdelay = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "borrow")) {
			borrow = parent_name;
#if 1
			/* support old style "borrow [parent]" */
			if (argc > 1 &&
			    EQUAL(*(argv + 1), parent_name)) {
				/* old style, skip borrow_name */
				argc--; argv++;
			}
#endif
		} else if (EQUAL(*argv, "pbandwidth")) {
			argc--; argv++;
			if (argc > 0)
				pbandwidth = strtoul(*argv, NULL, 0);
			if (pbandwidth > 100) {
				LOG(LOG_ERR, 0,
				    "bad pbandwidth %d for %s!",
				    pbandwidth, class_name);
				return (0);
			}
		} else if (EQUAL(*argv, "exactbandwidth")) {
			argc--; argv++;
			if (argc > 0)
				bandwidth = atobps(*argv);
		} else if (EQUAL(*argv, "maxburst")) {
			argc--; argv++;
			if (argc > 0)
				maxburst = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "minburst")) {
			argc--; argv++;
			if (argc > 0)
				minburst = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "packetsize")) {
			argc--; argv++;
			if (argc > 0)
				av_pkt_size = atobytes(*argv);
		} else if (EQUAL(*argv, "maxpacketsize")) {
			argc--; argv++;
			if (argc > 0)
				max_pkt_size = atobytes(*argv);
		} else if (EQUAL(*argv, "red")) {
			flags |= CBQCLF_RED;
		} else if (EQUAL(*argv, "ecn")) {
			flags |= CBQCLF_ECN;
		} else if (EQUAL(*argv, "flowvalve")) {
			flags |= CBQCLF_FLOWVALVE;
		} else if (EQUAL(*argv, "rio")) {
			flags |= CBQCLF_RIO;
		} else if (EQUAL(*argv, "cleardscp")) {
			flags |= CBQCLF_CLEARDSCP;
		} else {
			LOG(LOG_ERR, 0,
			    "Unknown keyword '%s' in %s, line %d",
			    *argv, altqconfigfile, line_no);
			return (0);
		}

		argc--; argv++;
	}

	if ((flags & (CBQCLF_RED|CBQCLF_RIO)) == (CBQCLF_RED|CBQCLF_RIO)) {
		LOG(LOG_ERR, 0,
		    "both red and rio defined on interface '%s'",
		    ifname);
		return (0);
	}
	if ((flags & (CBQCLF_ECN|CBQCLF_FLOWVALVE))
	    && (flags & (CBQCLF_RED|CBQCLF_RIO)) == 0)
		flags |= CBQCLF_RED;

	if (strcmp("ctl_class", class_name) == 0)
		flags |= CBQCLF_CTLCLASS;

	if (bandwidth == 0 && pbandwidth != 0) {
		struct ifinfo	*ifinfo;

		if ((ifinfo = ifname2ifinfo(ifname)) != NULL)
			bandwidth = ifinfo->bandwidth / 100 * pbandwidth;
	}

	error = qcmd_cbq_add_class(ifname, class_name, parent_name, borrow,
				   pri, bandwidth,
				   maxdelay, maxburst, minburst,
				   av_pkt_size, max_pkt_size,
				   admission_type, flags);
	if (error)
		return (0);
	return (1);
}

/*
 * qcmd api
 */
int
qcmd_cbq_add_if(const char *ifname, u_int bandwidth, int is_wrr, int efficient)
{
	int error;
	
	error = qop_cbq_add_if(NULL, ifname, bandwidth, is_wrr, efficient);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add cbq on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

int
qcmd_cbq_add_class(const char *ifname, const char *class_name,
		   const char *parent_name, const char *borrow_name,
		   u_int pri, u_int bandwidth,
		   u_int maxdelay, u_int maxburst, u_int minburst,
		   u_int av_pkt_size, u_int max_pkt_size,
		   int admission_type, int flags)
{
	struct ifinfo *ifinfo;
	struct cbq_ifinfo *cbq_ifinfo;
	struct classinfo *parent = NULL, *borrow = NULL;
	u_int	ctl_bandwidth = 0;
	int	error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;
	cbq_ifinfo = ifinfo->private;

	if (error == 0 && parent_name != NULL &&
	    (parent = clname2clinfo(ifinfo, parent_name)) == NULL)
		error = QOPERR_BADCLASS;

	if (error == 0 && borrow_name != NULL &&
	    (borrow = clname2clinfo(ifinfo, borrow_name)) == NULL)
		error = QOPERR_BADCLASS;

	if (flags & CBQCLF_DEFCLASS) {
		/*
		 * if this is a default class and no ctl_class is defined,
		 * we will create a ctl_class.
		 */
		if (cbq_ifinfo->ctl_class == NULL) {
			/* reserve bandwidth for ctl_class */
			ctl_bandwidth =
				ifinfo->bandwidth / 100 * CTL_PBANDWIDTH;
			bandwidth -= ctl_bandwidth;
		}
	}

	if (error == 0)
		error = qop_cbq_add_class(NULL, class_name, ifinfo, parent,
					  borrow, pri, bandwidth,
					  maxdelay, maxburst, minburst,
					  av_pkt_size, max_pkt_size,
					  admission_type, flags);
	if (error != 0)
		LOG(LOG_ERR, errno,
		    "cbq: %s: can't add class '%s' on interface '%s'",
		    qoperror(error), class_name, ifname);

	if (ctl_bandwidth != 0) {
		/*
		 * If were adding the default traffic class and
		 * no ctl_class is defined, also add the ctl traffic class.
		 * This is for RSVP and IGMP packets.
		 */
		if (qcmd_cbq_add_class(ifname, "ctl_class", parent_name,
		      borrow_name, 6, ctl_bandwidth,
		      maxdelay, maxburst, minburst, av_pkt_size,
		      max_pkt_size, admission_type, CBQCLF_CTLCLASS) != 0) {
			LOG(LOG_ERR, errno, "can't create ctl_class!");
			return (QOPERR_CLASS);
		}
	}

	/*
	 * if this is a ctl class, add the default filters for backward
	 * compatibility
	 */
	if (flags & CBQCLF_CTLCLASS)
		qcmd_cbq_add_ctl_filters(ifname, class_name);

	return (error);
}

int
qcmd_cbq_modify_class(const char *ifname, const char *class_name,
		      u_int pri, u_int bandwidth,
		      u_int maxdelay, u_int maxburst, u_int minburst,
		      u_int av_pkt_size, u_int max_pkt_size, int flags)
{
	struct ifinfo *ifinfo;
	struct classinfo *clinfo;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if ((clinfo = clname2clinfo(ifinfo, class_name)) == NULL)
		return (QOPERR_BADCLASS);

	return qop_cbq_modify_class(clinfo, pri, bandwidth,
				    maxdelay, maxburst, minburst,
				    av_pkt_size, max_pkt_size, flags);
}

/*
 * add the default filters for ctl_class (for backward compatibility).
 */
#ifndef IPPROTO_RSVP
#define IPPROTO_RSVP		46
#endif

static int 
qcmd_cbq_add_ctl_filters(const char *ifname, const char *clname)
{
	struct flow_filter	sfilt;
	u_int8_t ctl_protos[3] = {IPPROTO_ICMP, IPPROTO_IGMP, IPPROTO_RSVP};
#ifdef INET6
	struct flow_filter6	sfilt6;
	u_int8_t ctl6_protos[3] = {IPPROTO_ICMPV6, IPPROTO_IGMP, IPPROTO_RSVP};
#endif
	int error, i;

	for (i = 0; i < (int)sizeof(ctl_protos); i++) {
		memset(&sfilt, 0, sizeof(sfilt));
		sfilt.ff_flow.fi_family = AF_INET;
		sfilt.ff_flow.fi_proto = ctl_protos[i];

		filter_dontwarn = 1;		/* XXX */
		error = qcmd_add_filter(ifname, clname, NULL, &sfilt);
		filter_dontwarn = 0;		/* XXX */
		if (error) {
			LOG(LOG_ERR, 0,
			    "can't add ctl class filter on interface '%s'",
			    ifname);
			return (error);
		}
	}

#ifdef INET6
	for (i = 0; i < sizeof(ctl6_protos); i++) {
		memset(&sfilt6, 0, sizeof(sfilt6));
		sfilt6.ff_flow6.fi6_family = AF_INET6;
		sfilt6.ff_flow6.fi6_proto = ctl6_protos[i];

		error = qcmd_add_filter(ifname, clname, NULL,
					(struct flow_filter *)&sfilt6);
		if (error) {
			LOG(LOG_WARNING, 0,
			    "can't add ctl class IPv6 filter on interface '%s'",
			    ifname);
			return (error);
		}
	}
#endif
	return (0);
}

/*
 * qop api
 */
int 
qop_cbq_add_if(struct ifinfo **rp, const char *ifname,
	       u_int bandwidth, int is_wrr, int efficient)
{
	struct ifinfo *ifinfo = NULL;
	struct cbq_ifinfo *cbq_ifinfo = NULL;
	int error;

	if ((cbq_ifinfo = calloc(1, sizeof(*cbq_ifinfo))) == NULL)
		return (QOPERR_NOMEM);

	cbq_ifinfo->nsPerByte =
		(1.0 / (double)bandwidth) * NS_PER_SEC * 8;
	cbq_ifinfo->is_wrr = is_wrr;
	cbq_ifinfo->is_efficient = efficient;

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &cbq_qdisc, cbq_ifinfo);
	if (error != 0)
		goto err_ret;

	/* set enable hook */
	ifinfo->enable_hook = qop_cbq_enable_hook;

	if (rp != NULL)
		*rp = ifinfo;
	return (0);

 err_ret:
	if (cbq_ifinfo != NULL) {
		free(cbq_ifinfo);
		if (ifinfo != NULL)
			ifinfo->private = NULL;
	}
	return (error);
}

#define is_sc_null(sc)	(((sc) == NULL) || ((sc)->m1 == 0 && (sc)->m2 == 0))

int 
qop_cbq_add_class(struct classinfo **rp, const char *class_name,
		  struct ifinfo *ifinfo, struct classinfo *parent, 
		  struct classinfo *borrow, u_int pri, u_int bandwidth,
		  u_int maxdelay, u_int maxburst, u_int minburst,
		  u_int av_pkt_size, u_int max_pkt_size,
		  int admission_type, int flags)
{
	struct classinfo *clinfo;
	struct cbq_ifinfo *cbq_ifinfo;
	struct cbq_classinfo *cbq_clinfo, *parent_clinfo;
	u_int parent_handle, borrow_handle;
	int error;

	cbq_ifinfo = ifinfo->private;

	if (parent == NULL) {
		if (cbq_ifinfo->root_class != NULL)
			return (QOPERR_CLASS_INVAL);
		flags |= CBQCLF_ROOTCLASS;
	}
	if ((flags & CBQCLF_DEFCLASS) && cbq_ifinfo->default_class != NULL)
		return (QOPERR_CLASS_INVAL);
	if ((flags & CBQCLF_CTLCLASS) && cbq_ifinfo->ctl_class != NULL)
		return (QOPERR_CLASS_INVAL);

	/* admission control */
	if (parent != NULL) {
		parent_clinfo = parent->private;
		if (bandwidth >
		    parent_clinfo->bandwidth - parent_clinfo->allocated) {
#ifdef ALLOW_OVERCOMMIT
			LOG(LOG_WARNING, 0,
			    "bandwidth overcommitted %uK requested but only %dK available (%uK already allocated)",
			    bandwidth / 1000,
			    ((int)parent_clinfo->bandwidth -
			     parent_clinfo->allocated) / 1000,
			    parent_clinfo->allocated / 1000);
#else /* !ALLOW_OVERCOMMIT */
			LOG(LOG_ERR, 0,
			    "cbq admission failed! %uK requested but only %uK available (%uK already allocated)",
			    bandwidth / 1000,
			    (parent_clinfo->bandwidth -
			     parent_clinfo->allocated) / 1000,
			    parent_clinfo->allocated / 1000);
			return (QOPERR_ADMISSION_NOBW);
#endif /* !ALLOW_OVERCOMMIT */
		}
	}
		
	if ((cbq_clinfo = calloc(1, sizeof(*cbq_clinfo))) == NULL)
		return (QOPERR_NOMEM);

	cbq_clinfo->bandwidth = bandwidth;
	cbq_clinfo->allocated = 0;

	/* if average paket size isn't specified, set if mtu. */
	if (av_pkt_size == 0) {	/* use default */
		av_pkt_size = ifinfo->ifmtu;
		if (av_pkt_size > MCLBYTES)	/* do what TCP does */
			av_pkt_size &= ~MCLBYTES;
	} else if (av_pkt_size > ifinfo->ifmtu)
		av_pkt_size = ifinfo->ifmtu;

	if (max_pkt_size == 0)	/* use default */
		max_pkt_size = ifinfo->ifmtu;
	else if (max_pkt_size > ifinfo->ifmtu)
		max_pkt_size = ifinfo->ifmtu;

	cbq_clinfo->maxdelay = maxdelay;
	cbq_clinfo->maxburst = maxburst;
	cbq_clinfo->minburst = minburst;
	cbq_clinfo->av_pkt_size = av_pkt_size;
	cbq_clinfo->max_pkt_size = max_pkt_size;

	parent_handle = parent != NULL ? parent->handle : NULL_CLASS_HANDLE;
	borrow_handle = borrow != NULL ? borrow->handle : NULL_CLASS_HANDLE;

	if (cbq_class_spec(ifinfo, parent_handle, borrow_handle, pri, flags,
			   bandwidth, maxdelay, maxburst, minburst,
			   av_pkt_size, max_pkt_size,
			   &cbq_clinfo->class_spec) < 0) {
		error = QOPERR_INVAL;
		goto err_ret;
	}

	clinfo = NULL;
	error = qop_add_class(&clinfo, class_name, ifinfo, parent, cbq_clinfo);
	if (error != 0)
		goto err_ret;

	/* set delete hook */
	clinfo->delete_hook = qop_cbq_delete_class_hook;
	
	if (parent == NULL)
		cbq_ifinfo->root_class = clinfo;
	else {
		parent_clinfo = parent->private;
		parent_clinfo->allocated += bandwidth;
	}
	if (flags & CBQCLF_DEFCLASS)
		cbq_ifinfo->default_class = clinfo;
	if (flags & CBQCLF_CTLCLASS)
		cbq_ifinfo->ctl_class = clinfo;

	switch (admission_type) {
	case CBQ_QOS_CNTR_LOAD:
	case CBQ_QOS_GUARANTEED:
	case CBQ_QOS_PREDICTIVE:
	case CBQ_QOS_CNTR_DELAY:
		if (ifinfo->resv_class != NULL) {
			LOG(LOG_ERR, 0,
			    "%s: duplicate resv meta class", class_name);
			return (QOPERR_CLASS);
		}
		ifinfo->resv_class = clinfo;
	}

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	if (cbq_clinfo != NULL) {
		free(cbq_clinfo);
		if (clinfo != NULL)
		    clinfo->private = NULL;
	}
	return (error);
}

/*
 * this is called from qop_delete_class() before a class is destroyed
 * for discipline specific cleanup.
 */
static int 
qop_cbq_delete_class_hook(struct classinfo *clinfo)
{
	struct cbq_classinfo *cbq_clinfo, *parent_clinfo;

	/* cancel admission control */
	if (clinfo->parent != NULL) {
		cbq_clinfo = clinfo->private;
		parent_clinfo = clinfo->parent->private;
		
		parent_clinfo->allocated -= cbq_clinfo->bandwidth;
	}
	return (0);
}

int
qop_cbq_modify_class(struct classinfo *clinfo, u_int pri, u_int bandwidth,
		     u_int maxdelay, u_int maxburst, u_int minburst,
		     u_int av_pkt_size, u_int max_pkt_size, int flags)
{
	struct ifinfo *ifinfo;
	struct cbq_classinfo *cbq_clinfo, *parent_clinfo;
	u_int	parent_handle, borrow_handle;
	u_int	old_bandwidth;
	int	error;

	ifinfo = clinfo->ifinfo;
	cbq_clinfo = clinfo->private;

	/* admission control */
	old_bandwidth = cbq_clinfo->bandwidth;
	if (clinfo->parent != NULL) {
		parent_clinfo = clinfo->parent->private;
		if (bandwidth > old_bandwidth) {
			/* increase bandwidth */
			if (bandwidth - old_bandwidth >
			    parent_clinfo->bandwidth
			    - parent_clinfo->allocated)
				return (QOPERR_ADMISSION_NOBW);
		} else if (bandwidth < old_bandwidth) {
			/* decrease bandwidth */
			if (bandwidth < cbq_clinfo->allocated)
				return (QOPERR_ADMISSION);
		}
	}

	/* if average paket size isn't specified, set if mtu. */
	if (av_pkt_size == 0) {	/* use default */
		av_pkt_size = ifinfo->ifmtu;
		if (av_pkt_size > MCLBYTES)	/* do what TCP does */
			av_pkt_size &= ~MCLBYTES;
	} else if (av_pkt_size > ifinfo->ifmtu)
		av_pkt_size = ifinfo->ifmtu;

	if (max_pkt_size == 0)	/* use default */
		max_pkt_size = ifinfo->ifmtu;
	else if (max_pkt_size > ifinfo->ifmtu)
		max_pkt_size = ifinfo->ifmtu;

	cbq_clinfo->maxdelay = maxdelay;
	cbq_clinfo->maxburst = maxburst;
	cbq_clinfo->minburst = minburst;
	cbq_clinfo->av_pkt_size = av_pkt_size;
	cbq_clinfo->max_pkt_size = max_pkt_size;

	parent_handle = cbq_clinfo->class_spec.parent_class_handle;
	borrow_handle = cbq_clinfo->class_spec.borrow_class_handle;

	if (cbq_class_spec(ifinfo, parent_handle, borrow_handle, pri, flags,
			   bandwidth, maxdelay, maxburst, minburst,
			   av_pkt_size, max_pkt_size,
			   &cbq_clinfo->class_spec) < 0) {
		return (QOPERR_INVAL);
	}

	error = qop_modify_class(clinfo, NULL);

	if (error == 0) {
		if (clinfo->parent != NULL) {
			parent_clinfo = clinfo->parent->private;
			parent_clinfo->allocated -= old_bandwidth;
			parent_clinfo->allocated += bandwidth;
		}
		cbq_clinfo->bandwidth = bandwidth;
	}
	return (error);
}

/*
 * sanity check at enabling cbq:
 *  there must one root class and one default class for an interface
 */
static int
qop_cbq_enable_hook(struct ifinfo *ifinfo)
{
	struct cbq_ifinfo *cbq_ifinfo;
	
	cbq_ifinfo = ifinfo->private;
	if (cbq_ifinfo->root_class == NULL) {
		LOG(LOG_ERR, 0, "cbq: no root class on interface %s!",
		    ifinfo->ifname);
		return (QOPERR_CLASS);
	}
	if (cbq_ifinfo->default_class == NULL) {
		LOG(LOG_ERR, 0, "cbq: no default class on interface %s!",
		    ifinfo->ifname);
		return (QOPERR_CLASS);
	}
	return (0);
}

static int
cbq_class_spec(struct ifinfo *ifinfo, u_long parent_class,
	       u_long borrow_class, u_int pri, int flags,
	       u_int bandwidth, u_int maxdelay, u_int maxburst,
	       u_int minburst, u_int av_pkt_size, u_int max_pkt_size,
	       cbq_class_spec_t *cl_spec)
{
	struct cbq_ifinfo *cbq_ifinfo = ifinfo->private;
	double          maxq, maxidle_s, maxidle, minidle,
			offtime, nsPerByte, ptime, cptime;
	double		z = (double)(1 << RM_FILTER_GAIN);
	double          g = (1.0 - 1.0 / z);
	double          f;
 	double		gton;
 	double		gtom;
	double		maxrate;

 	/* Compute other class parameters */
	if (bandwidth == 0)
		f = 0.0001;	/* small enough? */
	else
		f = ((double) bandwidth / (double) ifinfo->bandwidth);

	if (av_pkt_size == 0) {	/* use default */
		av_pkt_size = ifinfo->ifmtu;
		if (av_pkt_size > MCLBYTES)	/* do what TCP does */
			av_pkt_size &= ~MCLBYTES;
	} else if (av_pkt_size > ifinfo->ifmtu)
		av_pkt_size = ifinfo->ifmtu;
	if (max_pkt_size == 0)	/* use default */
		max_pkt_size = ifinfo->ifmtu;
	else if (max_pkt_size > ifinfo->ifmtu)
		max_pkt_size = ifinfo->ifmtu;

        nsPerByte = cbq_ifinfo->nsPerByte / f;
	ptime = (double) av_pkt_size * (double)cbq_ifinfo->nsPerByte;
	maxrate = f * ((double)ifinfo->bandwidth / 8.0);
	cptime = ptime * (1.0 - f) / f;
#if 1 /* ALTQ */
	if (nsPerByte * (double)max_pkt_size > (double)INT_MAX) {
		/*
		 * this causes integer overflow in kernel!
		 * (bandwidth < 6Kbps when max_pkt_size=1500)
		 */
		if (bandwidth != 0)
			LOG(LOG_WARNING, 0, "warning: class is too slow!!");
		nsPerByte = (double)(INT_MAX / max_pkt_size);
	}
#endif
	if (maxburst == 0) {  /* use default */
		if (cptime > 10.0 * NS_PER_MS)
			maxburst = 4;
		else
			maxburst = 16;
	}
	if (minburst == 0)  /* use default */
		minburst = 2;
	if (minburst > maxburst)
		minburst = maxburst;

	if (IsDebug(DEBUG_ALTQ)) {
		int packet_time;
		LOG(LOG_DEBUG, 0,
		    "cbq_flowspec: maxburst=%d,minburst=%d,pkt_size=%d",
		    maxburst, minburst, av_pkt_size);
		LOG(LOG_DEBUG, 0,
		    "  nsPerByte=%.2f ns, link's nsPerByte=%.2f, f=%.3f",
		    nsPerByte, cbq_ifinfo->nsPerByte, f);
		packet_time = av_pkt_size * (int)nsPerByte / 1000;
		LOG(LOG_DEBUG, 0,
		    "  packet time=%d [us]\n", packet_time);
		if (maxburst * packet_time < 20000) {
			LOG(LOG_WARNING, 0,
			  "warning: maxburst smaller than timer granularity!");
			LOG(LOG_WARNING, 0,
			    "         maxburst=%d, packet_time=%d [us]",
			    maxburst, packet_time);
		}
	}
 	gton = pow(g, (double)maxburst);
 	gtom = pow(g, (double)(minburst-1));
	maxidle = ((1.0 / f - 1.0) * ((1.0 - gton) / gton));
	maxidle_s = (1.0 - g);
	if (maxidle > maxidle_s)
		maxidle = ptime * maxidle;
	else
		maxidle = ptime * maxidle_s;
	if (IsDebug(DEBUG_ALTQ))
		LOG(LOG_DEBUG, 0, "  maxidle=%.2f us", maxidle/1000.0);
	if (minburst)
		offtime = cptime * (1.0 + 1.0/(1.0 - g) * (1.0 - gtom) / gtom);
	else
		offtime = cptime;
	minidle = -((double)max_pkt_size * (double)nsPerByte);
	if (IsDebug(DEBUG_ALTQ))
		LOG(LOG_DEBUG, 0, "  offtime=%.2f us minidle=%.2f us",
		    offtime/1000.0, minidle/1000.0);

	maxidle = ((maxidle * 8.0) / nsPerByte) * pow(2, RM_FILTER_GAIN);
#if 1 /* ALTQ */
	/* also scale offtime and minidle */
 	offtime = (offtime * 8.0) / nsPerByte * pow(2, RM_FILTER_GAIN);
	minidle = ((minidle * 8.0) / nsPerByte) * pow(2, RM_FILTER_GAIN);
#endif
	maxidle = maxidle / 1000.0;
	offtime = offtime / 1000.0;
	minidle = minidle / 1000.0;
	/* adjust queue size when maxdelay is specified.
	   queue size should be relative to its share */
	if (maxdelay == 0) {
		if (flags & (CBQCLF_RED|CBQCLF_RIO))
			maxq = 60.0;
		else
			maxq = 30.0;
	} else {
		maxq = ((double) maxdelay * NS_PER_MS) / (nsPerByte * av_pkt_size);
		if (maxq < 4) {
			LOG(LOG_WARNING, 0,
			    "warning: maxq (%d) is too small. set to %d",
			    (int)maxq, 4);
			maxq = 4;
		}
	}
	if (bandwidth == 0 && borrow_class == NULL_CLASS_HANDLE)
		/* filter out this class by setting queue size to zero */
		maxq = 0;
	if (IsDebug(DEBUG_ALTQ)) {
		if ((u_int)maxq < maxburst)
			LOG(LOG_WARNING, 0,
			   "warning: maxq (%d) is smaller than maxburst(%d)",
			    (int)maxq, maxburst);
		else if (maxq > 100.0)
			LOG(LOG_WARNING, 0,
			    "warning: maxq %d too large\n", (int)maxq);
		LOG(LOG_DEBUG, 0, "  maxq=%d", (int)maxq);
	}

	if (parent_class == NULL_CLASS_HANDLE) {
		if ((flags & CBQCLF_ROOTCLASS) == 0)
			flags |= CBQCLF_ROOTCLASS;
		if (cbq_ifinfo->is_wrr)
			flags |= CBQCLF_WRR;
		if (cbq_ifinfo->is_efficient)
			flags |= CBQCLF_EFFICIENT;
	}

	memset((void *)cl_spec, 0, sizeof(cbq_class_spec_t));
	cl_spec->priority = pri;
	cl_spec->nano_sec_per_byte = (u_int) nsPerByte;
	cl_spec->maxq = (u_int) maxq;
	cl_spec->maxidle = (u_int) fabs(maxidle);
	cl_spec->minidle = (int)minidle;
	cl_spec->offtime = (u_int) fabs(offtime);

	cl_spec->parent_class_handle = parent_class;
	cl_spec->borrow_class_handle = borrow_class;

	cl_spec->pktsize = av_pkt_size;
	cl_spec->flags = flags;

	return (0);
}


/*
 *  system call interfaces for qdisc_ops
 */
static int
cbq_attach(struct ifinfo *ifinfo)
{
	struct cbq_interface iface;

	if (cbq_fd < 0 &&
	    (cbq_fd = open(CBQ_DEVICE, O_RDWR)) < 0 &&
	    (cbq_fd = open_module(CBQ_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "CBQ open");
		return (QOPERR_SYSCALL);
	}

	cbq_refcount++;
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cbq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(cbq_fd, CBQ_IF_ATTACH, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_detach(struct ifinfo *ifinfo)
{
	struct cbq_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cbq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(cbq_fd, CBQ_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--cbq_refcount == 0) {
		close(cbq_fd);
		cbq_fd = -1;
	}
	return (0);
}

static int
cbq_clear(struct ifinfo *ifinfo)
{
	struct cbq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cbq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(cbq_fd, CBQ_CLEAR_HIERARCHY, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_enable(struct ifinfo *ifinfo)
{
	struct cbq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cbq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(cbq_fd, CBQ_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_disable(struct ifinfo *ifinfo)
{
	struct cbq_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.cbq_ifacename, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(cbq_fd, CBQ_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_add_class(struct classinfo *clinfo)
{
	struct cbq_add_class class_add;
	struct cbq_classinfo *cbq_clinfo;
	struct cbq_ifinfo *cbq_ifinfo;

	cbq_ifinfo = clinfo->ifinfo->private;
	cbq_clinfo = clinfo->private;
	
	memset(&class_add, 0, sizeof(class_add));
	strncpy(class_add.cbq_iface.cbq_ifacename,
		clinfo->ifinfo->ifname, IFNAMSIZ);

	class_add.cbq_class = cbq_clinfo->class_spec;

	if (ioctl(cbq_fd, CBQ_ADD_CLASS, &class_add) < 0)
		return (QOPERR_SYSCALL);

	clinfo->handle = class_add.cbq_class_handle;
	return (0);
}

static int
cbq_modify_class(struct classinfo *clinfo, void *arg)
{
	struct cbq_modify_class class_mod;
	struct cbq_classinfo *cbq_clinfo;

	cbq_clinfo = clinfo->private;

	memset(&class_mod, 0, sizeof(class_mod));
	strncpy(class_mod.cbq_iface.cbq_ifacename,
		clinfo->ifinfo->ifname, IFNAMSIZ);
	class_mod.cbq_class_handle = clinfo->handle;
	class_mod.cbq_class = cbq_clinfo->class_spec;

	if (ioctl(cbq_fd, CBQ_MODIFY_CLASS, &class_mod) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_delete_class(struct classinfo *clinfo)
{
	struct cbq_delete_class class_delete;

	memset(&class_delete, 0, sizeof(class_delete));
	strncpy(class_delete.cbq_iface.cbq_ifacename,
		clinfo->ifinfo->ifname, IFNAMSIZ);
	class_delete.cbq_class_handle = clinfo->handle;

	if (ioctl(cbq_fd, CBQ_DEL_CLASS, &class_delete) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
cbq_add_filter(struct fltrinfo *fltrinfo)
{
	struct cbq_add_filter fltr_add;
	
	memset(&fltr_add, 0, sizeof(fltr_add));
	strncpy(fltr_add.cbq_iface.cbq_ifacename,
		fltrinfo->clinfo->ifinfo->ifname, IFNAMSIZ);
	fltr_add.cbq_class_handle = fltrinfo->clinfo->handle;
	fltr_add.cbq_filter = fltrinfo->fltr;

	if (ioctl(cbq_fd, CBQ_ADD_FILTER, &fltr_add) < 0)
		return (QOPERR_SYSCALL);
	fltrinfo->handle = fltr_add.cbq_filter_handle;
	return (0);
}

static int
cbq_delete_filter(struct fltrinfo *fltrinfo)
{
	struct cbq_delete_filter fltr_del;

	memset(&fltr_del, 0, sizeof(fltr_del));
	strncpy(fltr_del.cbq_iface.cbq_ifacename,
		fltrinfo->clinfo->ifinfo->ifname, IFNAMSIZ);
	fltr_del.cbq_filter_handle = fltrinfo->handle;

	if (ioctl(cbq_fd, CBQ_DEL_FILTER, &fltr_del) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}


