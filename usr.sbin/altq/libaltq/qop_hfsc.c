/*	$OpenBSD: qop_hfsc.c,v 1.4 2002/09/08 09:09:54 kjc Exp $	*/
/*	$KAME: qop_hfsc.c,v 1.6 2001/08/16 10:39:14 kjc Exp $	*/
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
#include <math.h>

#include <altq/altq.h>
#include <altq/altq_hfsc.h>
#include "altq_qop.h"
#include "qop_hfsc.h"

static int read_sc(int *, char ***, int *, u_int *, u_int *, u_int *);
static int qop_hfsc_enable_hook(struct ifinfo *);
static int qop_hfsc_delete_class_hook(struct classinfo *);
static int validate_sc(struct service_curve *);

static void gsc_add_sc(struct gen_sc *, struct service_curve *);
static void gsc_sub_sc(struct gen_sc *, struct service_curve *);
static int is_gsc_under_sc(struct gen_sc *, struct service_curve *);
static void gsc_destroy(struct gen_sc *);
static struct segment *gsc_getentry(struct gen_sc *, double);
static int gsc_add_seg(struct gen_sc *, double, double, double, double);
static int gsc_sub_seg(struct gen_sc *, double, double, double, double);
static void gsc_compress(struct gen_sc *);
static double sc_x2y(struct service_curve *, double);

static int hfsc_attach(struct ifinfo *);
static int hfsc_detach(struct ifinfo *);
static int hfsc_clear(struct ifinfo *);
static int hfsc_enable(struct ifinfo *);
static int hfsc_disable(struct ifinfo *);
static int hfsc_add_class(struct classinfo *);
static int hfsc_modify_class(struct classinfo *, void *);
static int hfsc_delete_class(struct classinfo *);
static int hfsc_add_filter(struct fltrinfo *);
static int hfsc_delete_filter(struct fltrinfo *);

#define HFSC_DEVICE	"/dev/altq/hfsc"

static int hfsc_fd = -1;
static int hfsc_refcount = 0;

static struct qdisc_ops hfsc_qdisc = {
	ALTQT_HFSC,
	"hfsc",
	hfsc_attach,
	hfsc_detach,
	hfsc_clear,
	hfsc_enable,
	hfsc_disable,
	hfsc_add_class,
	hfsc_modify_class,
	hfsc_delete_class,
	hfsc_add_filter,
	hfsc_delete_filter,
};

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

/*
 * parser interface
 */
int
hfsc_interface_parser(const char *ifname, int argc, char **argv)
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
		} else if (EQUAL(*argv, "hfsc")) {
			/* just skip */
		} else {
			LOG(LOG_ERR, 0, "Unknown keyword '%s'", *argv);
			return (0);
		}
		argc--; argv++;
	}

	if (qcmd_tbr_register(ifname, bandwidth, tbrsize) != 0)
		return (0);

	if (qcmd_hfsc_add_if(ifname, bandwidth, flags) != 0)
		return (0);
	return (1);
}

int
hfsc_class_parser(const char *ifname, const char *class_name,
		  const char *parent_name, int argc, char **argv)
{
	u_int	m1, d, m2, rm1, rd, rm2, fm1, fd, fm2;
	int	qlimit = 50;
	int	flags = 0, admission = 0;
	int	type = 0, error;

	rm1 = rd = rm2 = fm1 = fd = fm2 = 0;
	while (argc > 0) {
		if (*argv[0] == '[') {
			if (read_sc(&argc, &argv, &type, &m1, &d, &m2) != 0) {
				LOG(LOG_ERR, 0,
				    "Bad service curve in %s, line %d",
				    altqconfigfile, line_no);
				return (0);
			}
			if (type & HFSC_REALTIMESC) {
				rm1 = m1; rd = d; rm2 = m2;
			}
			if (type & HFSC_LINKSHARINGSC) {
				fm1 = m1; fd = d; fm2 = m2;
			}
		} else if (EQUAL(*argv, "pshare")) {
			argc--; argv++;
			if (argc > 0) {
				struct ifinfo	*ifinfo;
				u_int pshare;

				pshare = (u_int)strtoul(*argv, NULL, 0);
				if ((ifinfo = ifname2ifinfo(ifname)) != NULL) {
					fm2 = ifinfo->bandwidth / 100 * pshare;
					type |= HFSC_LINKSHARINGSC;
				}
			}
		} else if (EQUAL(*argv, "grate")) {
			argc--; argv++;
			if (argc > 0) {
				rm2 = atobps(*argv);
				type |= HFSC_REALTIMESC;
			}
		} else if (EQUAL(*argv, "qlimit")) {
			argc--; argv++;
			if (argc > 0)
				qlimit = strtoul(*argv, NULL, 0);
		} else if (EQUAL(*argv, "default")) {
			flags |= HFCF_DEFAULTCLASS;
		} else if (EQUAL(*argv, "admission")) {
			argc--; argv++;
			if (argc > 0) {
				if (EQUAL(*argv, "guaranteed")
				    || EQUAL(*argv, "cntlload"))
					admission = 1;
				else if (EQUAL(*argv, "none")) {
					/* nothing */
				} else {
					LOG(LOG_ERR, 0,
					    "unknown admission type - %s, line %d",
					    *argv, line_no);
					return (0);
				}
			}
		} else if (EQUAL(*argv, "red")) {
			flags |= HFCF_RED;
		} else if (EQUAL(*argv, "ecn")) {
			flags |= HFCF_ECN;
		} else if (EQUAL(*argv, "rio")) {
			flags |= HFCF_RIO;
		} else if (EQUAL(*argv, "cleardscp")) {
			flags |= HFCF_CLEARDSCP;
		} else {
			LOG(LOG_ERR, 0,
			    "Unknown keyword '%s' in %s, line %d",
			    *argv, altqconfigfile, line_no);
			return (0);
		}

		argc--; argv++;
	}

	if (type == 0) {
		LOG(LOG_ERR, 0,
		    "hfsc: service curve not specified in %s, line %d",
		    altqconfigfile, line_no);
		return (0);
	}

	if ((flags & HFCF_ECN) && (flags & (HFCF_RED|HFCF_RIO)) == 0)
		flags |= HFCF_RED;

	/*
	 * if the link-sharing service curve is diffrent from
	 * the real-time service curve, we first create a class with the
	 * smaller service curve and then modify the other service curve.
	 */
	if (rm2 <= fm2) {
		m1 = rm1; d = rd; m2 = rm2;
	} else {
		m1 = fm1; d = fd; m2 = fm2;
	}
	error = qcmd_hfsc_add_class(ifname, class_name, parent_name,
				    m1, d, m2, qlimit, flags);

	if (error == 0 && (rm1 != fm1 || rd != fd || rm2 != fm2)) {
		if (rm2 <= fm2) {
			m1 = fm1; d = fd; m2 = fm2; type = HFSC_LINKSHARINGSC;
		} else {
			m1 = rm1; d = rd; m2 = rm2; type = HFSC_REALTIMESC;
		}
		error = qcmd_hfsc_modify_class(ifname, class_name,
					       m1, d, m2, type);
	}

	if (error == 0 && admission) {
		/* this is a special class for rsvp */
		struct ifinfo *ifinfo = ifname2ifinfo(ifname);
		struct classinfo *clinfo = clname2clinfo(ifinfo, class_name);

		if (ifinfo->resv_class != NULL) {
			LOG(LOG_ERR, 0,
			    "more than one admission class specified: %s",
			    class_name);
			return (0);
		}
		ifinfo->resv_class = clinfo;
	}

	if (error) {
		LOG(LOG_ERR, errno, "hfsc_class_parser: %s",
		    qoperror(error));
		return (0);
	}
	return (1);
}

/*
 * read service curve parameters
 * '[' <type> <m1> <d> <m2> ']'
 *  type := "sc", "rt", or "ls"
 */
static int
read_sc(int *argcp, char ***argvp, int *type, u_int *m1, u_int *d, u_int *m2)
{
	int argc = *argcp;
	char **argv = *argvp;
	char *cp;

	cp = *argv;
	if (*cp++ != '[')
		return (-1);
	if (*cp == '\0') {
		cp = *++argv; --argc;
	}
	if (*cp == 's' || *cp == 'S')
		*type = HFSC_DEFAULTSC;
	else if (*cp == 'r' || *cp == 'R')
		*type = HFSC_REALTIMESC;
	else if (*cp == 'l' || *cp == 'L')
		*type = HFSC_LINKSHARINGSC;
	else
		return (-1);
	cp = *++argv; --argc;
	*m1 = atobps(cp);
	cp = *++argv; --argc;
	*d = (u_int)strtoul(cp, NULL, 0);
	cp = *++argv; --argc;
	*m2 = atobps(cp);
	if (strchr(cp, ']') == NULL) {
		cp = *++argv; --argc;
		if (*cp != ']')
			return (-1);
	}
	*argcp = argc;
	*argvp = argv;
	return (0);
}

/*
 * qcmd api
 */
int
qcmd_hfsc_add_if(const char *ifname, u_int bandwidth, int flags)
{
	int error;
	
	error = qop_hfsc_add_if(NULL, ifname, bandwidth, flags);
	if (error != 0)
		LOG(LOG_ERR, errno, "%s: can't add hfsc on interface '%s'",
		    qoperror(error), ifname);
	return (error);
}

int
qcmd_hfsc_add_class(const char *ifname, const char *class_name,
		    const char *parent_name, u_int m1, u_int d, u_int m2,
		    int qlimit, int flags)
{
	struct ifinfo *ifinfo;
	struct classinfo *parent = NULL;
	struct service_curve sc;
	int error = 0;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		error = QOPERR_BADIF;

	if (error == 0 &&
	    (parent = clname2clinfo(ifinfo, parent_name)) == NULL)
		error = QOPERR_BADCLASS;

	sc.m1 = m1;
	sc.d = d;
	sc.m2 = m2;

	if (error == 0)
		error = qop_hfsc_add_class(NULL, class_name, ifinfo, parent,
					   &sc, qlimit, flags);
	if (error != 0)
		LOG(LOG_ERR, errno,
		    "hfsc: %s: can't add class '%s' on interface '%s'",
		    qoperror(error), class_name, ifname);
	return (error);
}

int
qcmd_hfsc_modify_class(const char *ifname, const char *class_name,
		       u_int m1, u_int d, u_int m2, int sctype)
{
	struct ifinfo *ifinfo;
	struct classinfo *clinfo;
	struct service_curve sc;

	if ((ifinfo = ifname2ifinfo(ifname)) == NULL)
		return (QOPERR_BADIF);

	if ((clinfo = clname2clinfo(ifinfo, class_name)) == NULL)
		return (QOPERR_BADCLASS);

	sc.m1 = m1;
	sc.d = d;
	sc.m2 = m2;
	
	return qop_hfsc_modify_class(clinfo, &sc, sctype);
}

/*
 * qop api
 */
int 
qop_hfsc_add_if(struct ifinfo **rp, const char *ifname,
		u_int bandwidth, int flags)
{
	struct ifinfo *ifinfo = NULL;
	struct hfsc_ifinfo *hfsc_ifinfo = NULL;
	struct service_curve sc;
	int error;

	if ((hfsc_ifinfo = calloc(1, sizeof(*hfsc_ifinfo))) == NULL)
		return (QOPERR_NOMEM);

	error = qop_add_if(&ifinfo, ifname, bandwidth,
			   &hfsc_qdisc, hfsc_ifinfo);
	if (error != 0)
		goto err_ret;

	/* set enable hook */
	ifinfo->enable_hook = qop_hfsc_enable_hook;

	/* create a dummy root class */
	sc.m1 = bandwidth;
	sc.d = 0;
	sc.m2 = bandwidth;
	if ((error = qop_hfsc_add_class(&hfsc_ifinfo->root_class, "root",
					ifinfo, NULL, &sc, 0, 0)) != 0) {
		LOG(LOG_ERR, errno,
		    "hfsc: %s: can't create dummy root class on %s!",
		    qoperror(error), ifname);
		(void)qop_delete_if(ifinfo);
		return (QOPERR_CLASS);
	}

	if (rp != NULL)
		*rp = ifinfo;
	return (0);

 err_ret:
	if (hfsc_ifinfo != NULL) {
		free(hfsc_ifinfo);
		if (ifinfo != NULL)
			ifinfo->private = NULL;
	}
	return (error);
}

#define is_sc_null(sc)	(((sc) == NULL) || ((sc)->m1 == 0 && (sc)->m2 == 0))

int 
qop_hfsc_add_class(struct classinfo **rp, const char *class_name,
		   struct ifinfo *ifinfo, struct classinfo *parent, 
		   struct service_curve *sc, int qlimit, int flags)
{
	struct classinfo *clinfo;
	struct hfsc_ifinfo *hfsc_ifinfo;
	struct hfsc_classinfo *hfsc_clinfo = NULL, *parent_clinfo = NULL;
	int error;

	hfsc_ifinfo = ifinfo->private;
	if ((flags & HFCF_DEFAULTCLASS) && hfsc_ifinfo->default_class != NULL)
		return (QOPERR_CLASS_INVAL);

	if (validate_sc(sc) != 0)
		return (QOPERR_INVAL);

	/* admission control */
	if (parent != NULL && !is_sc_null(sc)) {
		parent_clinfo = parent->private;
		gsc_add_sc(&parent_clinfo->gen_rsc, sc);
		gsc_add_sc(&parent_clinfo->gen_fsc, sc);
		if (!is_gsc_under_sc(&parent_clinfo->gen_rsc,
				     &parent_clinfo->rsc) ||
		    !is_gsc_under_sc(&parent_clinfo->gen_fsc,
				     &parent_clinfo->fsc)) {
			/* admission control failure */
			error = QOPERR_ADMISSION_NOBW;
			goto err_ret;
		}
	}
		
	if ((hfsc_clinfo = calloc(1, sizeof(*hfsc_clinfo))) == NULL) {
		error = QOPERR_NOMEM;
		goto err_ret;
	}

	hfsc_clinfo->rsc = *sc;
	hfsc_clinfo->fsc = *sc;
	LIST_INIT(&hfsc_clinfo->gen_rsc);
	LIST_INIT(&hfsc_clinfo->gen_fsc);
	hfsc_clinfo->qlimit = qlimit;
	hfsc_clinfo->flags = flags;

	if ((error = qop_add_class(&clinfo, class_name, ifinfo, parent,
				   hfsc_clinfo)) != 0)
		goto err_ret;

	/* set delete hook */
	clinfo->delete_hook = qop_hfsc_delete_class_hook;
	
	if (flags & HFCF_DEFAULTCLASS)
		hfsc_ifinfo->default_class = clinfo;

	if (parent == NULL) {
		/*
		 * if this is a root class, reserve 20% of the real-time
		 * bandwidth for safety.
		 * many network cards are not able to saturate the wire,
		 * and if we allocate real-time traffic more than the
		 * maximum sending rate of the card, hfsc is no longer
		 * able to meet the delay bound requirements.
		 */
		hfsc_clinfo->rsc.m1 = hfsc_clinfo->rsc.m1 / 10 * 8;
		hfsc_clinfo->rsc.m2 = hfsc_clinfo->rsc.m2 / 10 * 8;
	}

	if (rp != NULL)
		*rp = clinfo;
	return (0);

 err_ret:
	/* cancel admission control */
	if (parent != NULL && !is_sc_null(sc)) {
		gsc_sub_sc(&parent_clinfo->gen_rsc, sc);
		gsc_sub_sc(&parent_clinfo->gen_fsc, sc);
	}

	if (hfsc_clinfo != NULL) {
		free(hfsc_clinfo);
		clinfo->private = NULL;
	}
	
	return (error);
}

/*
 * this is called from qop_delete_class() before a class is destroyed
 * for discipline specific cleanup.
 */
static int 
qop_hfsc_delete_class_hook(struct classinfo *clinfo)
{
	struct hfsc_classinfo *hfsc_clinfo, *parent_clinfo;

	hfsc_clinfo = clinfo->private;
	
	/* cancel admission control */
	if (clinfo->parent != NULL) {
		parent_clinfo = clinfo->parent->private;

		gsc_sub_sc(&parent_clinfo->gen_rsc, &hfsc_clinfo->rsc);
		gsc_sub_sc(&parent_clinfo->gen_fsc, &hfsc_clinfo->fsc);
	}

	gsc_destroy(&hfsc_clinfo->gen_rsc);
	gsc_destroy(&hfsc_clinfo->gen_fsc);
	return (0);
}

int
qop_hfsc_modify_class(struct classinfo *clinfo, 
		      struct service_curve *sc, int sctype)
{
	struct hfsc_classinfo *hfsc_clinfo, *parent_clinfo;
	struct service_curve rsc, fsc;
	int error;

	if (validate_sc(sc) != 0)
		return (QOPERR_INVAL);
		
	hfsc_clinfo = clinfo->private;
	if (clinfo->parent == NULL)
		return (QOPERR_CLASS_INVAL);
	parent_clinfo = clinfo->parent->private;

	/* save old service curves */
	rsc = hfsc_clinfo->rsc;
	fsc = hfsc_clinfo->fsc;

	/* admission control */
	if (sctype & HFSC_REALTIMESC) {
		if (!is_gsc_under_sc(&hfsc_clinfo->gen_rsc, sc)) {
			/* admission control failure */
			return (QOPERR_ADMISSION);
		}
		
		gsc_sub_sc(&parent_clinfo->gen_rsc, &hfsc_clinfo->rsc);
		gsc_add_sc(&parent_clinfo->gen_rsc, sc);
		if (!is_gsc_under_sc(&parent_clinfo->gen_rsc,
				     &parent_clinfo->rsc)) {
			/* admission control failure */
			gsc_sub_sc(&parent_clinfo->gen_rsc, sc);
			gsc_add_sc(&parent_clinfo->gen_rsc, &hfsc_clinfo->rsc);
			return (QOPERR_ADMISSION_NOBW);
		}
		hfsc_clinfo->rsc = *sc;
	}
	if (sctype & HFSC_LINKSHARINGSC) {
		if (!is_gsc_under_sc(&hfsc_clinfo->gen_fsc, sc)) {
			/* admission control failure */
			return (QOPERR_ADMISSION);
		}
		
		gsc_sub_sc(&parent_clinfo->gen_fsc, &hfsc_clinfo->fsc);
		gsc_add_sc(&parent_clinfo->gen_fsc, sc);
		if (!is_gsc_under_sc(&parent_clinfo->gen_fsc,
				     &parent_clinfo->fsc)) {
			/* admission control failure */
			gsc_sub_sc(&parent_clinfo->gen_fsc, sc);
			gsc_add_sc(&parent_clinfo->gen_fsc, &hfsc_clinfo->fsc);
			return (QOPERR_ADMISSION_NOBW);
		}
		hfsc_clinfo->fsc = *sc;
	}

	error = qop_modify_class(clinfo, (void *)((long)sctype));
	if (error == 0)
		return (0);

	/* modify failed!, restore the old service curves */
	if (sctype & HFSC_REALTIMESC) {
		gsc_sub_sc(&parent_clinfo->gen_rsc, sc);
		gsc_add_sc(&parent_clinfo->gen_rsc, &rsc);
		hfsc_clinfo->rsc = rsc;
	}
	if (sctype & HFSC_LINKSHARINGSC) {
		gsc_sub_sc(&parent_clinfo->gen_fsc, sc);
		gsc_add_sc(&parent_clinfo->gen_fsc, &fsc);
		hfsc_clinfo->fsc = fsc;
	}
	return (error);
}

/*
 * sanity check at enabling hfsc:
 *  1. there must one default class for an interface
 *  2. the default class must be a leaf class
 *  3. an internal class should not have filters
 * (rule 2 and 3 are due to the fact that the hfsc link-sharing algorithm
 *  do not schedule internal classes.)
 */
static int
qop_hfsc_enable_hook(struct ifinfo *ifinfo)
{
	struct hfsc_ifinfo *hfsc_ifinfo;
	struct classinfo *clinfo;
	
	hfsc_ifinfo = ifinfo->private;
	if (hfsc_ifinfo->default_class == NULL) {
		LOG(LOG_ERR, 0, "hfsc: no default class on interface %s!",
		    ifinfo->ifname);
		return (QOPERR_CLASS);
	} else if (hfsc_ifinfo->default_class->child != NULL) {
		LOG(LOG_ERR, 0, "hfsc: default class on %s must be a leaf!",
		    ifinfo->ifname);
		return (QOPERR_CLASS);
	}

	LIST_FOREACH(clinfo, &ifinfo->cllist, next) {
		if (clinfo->child != NULL && !LIST_EMPTY(&clinfo->fltrlist)) {
			LOG(LOG_ERR, 0,
			    "hfsc: internal class \"%s\" should not have a filter!",
			    clinfo->clname);
			return (QOPERR_CLASS);
		}
	}

	return (0);
}

static int
validate_sc(struct service_curve *sc)
{
	/* the 1st segment of a concave curve must be zero */
	if (sc->m1 < sc->m2 && sc->m1 != 0) {
		LOG(LOG_ERR, 0, "m1 must be 0 for convex!");
		return (-1);
	}
	return (0);
}

/*
 * admission control using generalized service curve
 */
#define	INFINITY	HUGE_VAL  /* positive infinity defined in <math.h> */

/* add a new service curve to a generilized service curve */
static void
gsc_add_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	if (is_sc_null(sc))
		return;
	if (sc->d != 0)
		gsc_add_seg(gsc, 0, 0, (double)sc->d, (double)sc->m1);
	gsc_add_seg(gsc, (double)sc->d, 0, INFINITY, (double)sc->m2);
}

/* subtract a service curve from a generilized service curve */
static void
gsc_sub_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	if (is_sc_null(sc))
		return;
	if (sc->d != 0)
		gsc_sub_seg(gsc, 0, 0, (double)sc->d, (double)sc->m1);
	gsc_sub_seg(gsc, (double)sc->d, 0, INFINITY, (double)sc->m2);
}

/*
 * check whether all points of a generalized service curve have
 * their y-coordinates no larger than a given two-piece linear
 * service curve.
 */
static int
is_gsc_under_sc(struct gen_sc *gsc, struct service_curve *sc)
{
	struct segment *s, *last, *end;
	double y;

	if (is_sc_null(sc)) {
		if (LIST_EMPTY(gsc))
			return (1);
		LIST_FOREACH(s, gsc, _next) {
			if (s->m != 0)
				return (0);
		}
		return (1);
	}
	/*
	 * gsc has a dummy entry at the end with x = INFINITY.
	 * loop through up to this dummy entry.
	 */
	end = gsc_getentry(gsc, INFINITY);
	if (end == NULL)
		return (1);
	last = NULL;
	for (s = LIST_FIRST(gsc); s != end; s = LIST_NEXT(s, _next)) {
		if (s->y > sc_x2y(sc, s->x))
			return (0);
		last = s;
	}
	/* last now holds the real last segment */
	if (last == NULL)
		return (1);
	if (last->m > sc->m2)
		return (0);
	if (last->x < sc->d && last->m > sc->m1) {
		y = last->y + (sc->d - last->x) * last->m;
		if (y > sc_x2y(sc, sc->d))
			return (0);
	}
	return (1);
}

static void
gsc_destroy(struct gen_sc *gsc)
{
	struct segment *s;

	while ((s = LIST_FIRST(gsc)) != NULL) {
		LIST_REMOVE(s, _next);
		free(s);
	}
}

/*
 * return a segment entry starting at x.
 * if gsc has no entry starting at x, a new entry is created at x.
 */
static struct segment *
gsc_getentry(struct gen_sc *gsc, double x)
{
	struct segment *new, *prev, *s;

	prev = NULL;
	LIST_FOREACH(s, gsc, _next) {
		if (s->x == x)
			return (s);	/* matching entry found */
		else if (s->x < x)
			prev = s;
		else
			break;
	}

	/* we have to create a new entry */
	if ((new = calloc(1, sizeof(struct segment))) == NULL)
		return (NULL);

	new->x = x;
	if (x == INFINITY || s == NULL)
		new->d = 0;
	else if (s->x == INFINITY)
		new->d = INFINITY;
	else
		new->d = s->x - x;
	if (prev == NULL) {
		/* insert the new entry at the head of the list */
		new->y = 0;
		new->m = 0;
		LIST_INSERT_HEAD(gsc, new, _next);
	} else {
		/*
		 * the start point intersects with the segment pointed by
		 * prev.  divide prev into 2 segments
		 */
		if (x == INFINITY) {
			prev->d = INFINITY;
			if (prev->m == 0)
				new->y = prev->y;
			else
				new->y = INFINITY;
		} else {
			prev->d = x - prev->x;
			new->y = prev->d * prev->m + prev->y;
		}
		new->m = prev->m;
		LIST_INSERT_AFTER(prev, new, _next);
	}
	return (new);
}

/* add a segment to a generalized service curve */
static int
gsc_add_seg(struct gen_sc *gsc, double x, double y, double d, double m)
{
	struct segment *start, *end, *s;
	double x2;

	if (d == INFINITY)
		x2 = INFINITY;
	else
		x2 = x + d;
	start = gsc_getentry(gsc, x);
	end   = gsc_getentry(gsc, x2);
	if (start == NULL || end == NULL)
		return (-1);

	for (s = start; s != end; s = LIST_NEXT(s, _next)) {
		s->m += m;
		s->y += y + (s->x - x) * m;
	}

	end = gsc_getentry(gsc, INFINITY);
	for (; s != end; s = LIST_NEXT(s, _next)) {
		s->y += m * d;
	}

	return (0);
}

/* subtract a segment from a generalized service curve */
static int
gsc_sub_seg(struct gen_sc *gsc, double x, double y, double d, double m)
{
	if (gsc_add_seg(gsc, x, y, d, -m) < 0)
		return (-1);
	gsc_compress(gsc);
	return (0);
}

/*
 * collapse adjacent segments with the same slope
 */
static void
gsc_compress(struct gen_sc *gsc)
{
	struct segment *s, *next;

 again:
	LIST_FOREACH(s, gsc, _next) {

		if ((next = LIST_NEXT(s, _next)) == NULL) {
			if (LIST_FIRST(gsc) == s && s->m == 0) {
				/*
				 * if this is the only entry and its
				 * slope is 0, it's a remaining dummy
				 * entry. we can discard it.
				 */
				LIST_REMOVE(s, _next);
				free(s);
			}
			break;
		}

		if (s->x == next->x) {
			/* discard this entry */
			LIST_REMOVE(s, _next);
			free(s);
			goto again;
		} else if (s->m == next->m) {
			/* join the two entries */
			if (s->d != INFINITY && next->d != INFINITY)
				s->d += next->d;
			LIST_REMOVE(next, _next);
			free(next);
			goto again;
		}
	}
}

/* get y-projection of a service curve */
static double
sc_x2y(struct service_curve *sc, double x)
{
	double y;

	if (x <= (double)sc->d)
		/* y belongs to the 1st segment */
		y = x * (double)sc->m1;
	else
		/* y belongs to the 2nd segment */
		y = (double)sc->d * (double)sc->m1
			+ (x - (double)sc->d) * (double)sc->m2;
	return (y);
}

/*
 *  system call interfaces for qdisc_ops
 */
static int
hfsc_attach(struct ifinfo *ifinfo)
{
	struct hfsc_attach attach;

	if (hfsc_fd < 0 &&
	    (hfsc_fd = open(HFSC_DEVICE, O_RDWR)) < 0 &&
	    (hfsc_fd = open_module(HFSC_DEVICE, O_RDWR)) < 0) {
		LOG(LOG_ERR, errno, "HFSC open");
		return (QOPERR_SYSCALL);
	}

	hfsc_refcount++;
	memset(&attach, 0, sizeof(attach));
	strncpy(attach.iface.hfsc_ifname, ifinfo->ifname, IFNAMSIZ);
	attach.bandwidth = ifinfo->bandwidth;

	if (ioctl(hfsc_fd, HFSC_IF_ATTACH, &attach) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_detach(struct ifinfo *ifinfo)
{
	struct hfsc_interface iface;
	
	memset(&iface, 0, sizeof(iface));
	strncpy(iface.hfsc_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(hfsc_fd, HFSC_IF_DETACH, &iface) < 0)
		return (QOPERR_SYSCALL);

	if (--hfsc_refcount == 0) {
		close(hfsc_fd);
		hfsc_fd = -1;
	}
	return (0);
}

static int
hfsc_clear(struct ifinfo *ifinfo)
{
	struct hfsc_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.hfsc_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(hfsc_fd, HFSC_CLEAR_HIERARCHY, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_enable(struct ifinfo *ifinfo)
{
	struct hfsc_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.hfsc_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(hfsc_fd, HFSC_ENABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_disable(struct ifinfo *ifinfo)
{
	struct hfsc_interface iface;

	memset(&iface, 0, sizeof(iface));
	strncpy(iface.hfsc_ifname, ifinfo->ifname, IFNAMSIZ);

	if (ioctl(hfsc_fd, HFSC_DISABLE, &iface) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_add_class(struct classinfo *clinfo)
{
	struct hfsc_add_class class_add;
	struct hfsc_classinfo *hfsc_clinfo;
	struct hfsc_ifinfo *hfsc_ifinfo;

	/* root class is a dummy class */
	if (clinfo->parent == NULL) {
		clinfo->handle = HFSC_ROOTCLASS_HANDLE;
		return (0);
	}
	
	hfsc_ifinfo = clinfo->ifinfo->private;
	hfsc_clinfo = clinfo->private;
	
	memset(&class_add, 0, sizeof(class_add));
	strncpy(class_add.iface.hfsc_ifname, clinfo->ifinfo->ifname, IFNAMSIZ);
	if (clinfo->parent == hfsc_ifinfo->root_class)
		class_add.parent_handle = HFSC_ROOTCLASS_HANDLE;
	else
		class_add.parent_handle = clinfo->parent->handle;
	class_add.service_curve = hfsc_clinfo->rsc;
	class_add.qlimit = hfsc_clinfo->qlimit;
	class_add.flags = hfsc_clinfo->flags;
	if (ioctl(hfsc_fd, HFSC_ADD_CLASS, &class_add) < 0) {
		clinfo->handle = HFSC_NULLCLASS_HANDLE;
		return (QOPERR_SYSCALL);
	}
	clinfo->handle = class_add.class_handle;
	return (0);
}

static int
hfsc_modify_class(struct classinfo *clinfo, void *arg)
{
	struct hfsc_modify_class class_mod;
	struct hfsc_classinfo *hfsc_clinfo;
	long sctype;

	sctype = (long)arg;
	hfsc_clinfo = clinfo->private;

	memset(&class_mod, 0, sizeof(class_mod));
	strncpy(class_mod.iface.hfsc_ifname, clinfo->ifinfo->ifname, IFNAMSIZ);
	class_mod.class_handle = clinfo->handle;
	if (sctype & HFSC_REALTIMESC)
		class_mod.service_curve = hfsc_clinfo->rsc;
	else if (sctype & HFSC_LINKSHARINGSC)
		class_mod.service_curve = hfsc_clinfo->fsc;
	else
		return (QOPERR_INVAL);
	class_mod.sctype = sctype;

	if (ioctl(hfsc_fd, HFSC_MOD_CLASS, &class_mod) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_delete_class(struct classinfo *clinfo)
{
	struct hfsc_delete_class class_delete;

	if (clinfo->handle == HFSC_NULLCLASS_HANDLE ||
	    clinfo->handle == HFSC_ROOTCLASS_HANDLE)
		return (0);

	memset(&class_delete, 0, sizeof(class_delete));
	strncpy(class_delete.iface.hfsc_ifname, clinfo->ifinfo->ifname,
		IFNAMSIZ);
	class_delete.class_handle = clinfo->handle;

	if (ioctl(hfsc_fd, HFSC_DEL_CLASS, &class_delete) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}

static int
hfsc_add_filter(struct fltrinfo *fltrinfo)
{
	struct hfsc_add_filter fltr_add;
	
	memset(&fltr_add, 0, sizeof(fltr_add));
	strncpy(fltr_add.iface.hfsc_ifname, fltrinfo->clinfo->ifinfo->ifname,
		IFNAMSIZ);
	fltr_add.class_handle = fltrinfo->clinfo->handle;
	fltr_add.filter = fltrinfo->fltr;

	if (ioctl(hfsc_fd, HFSC_ADD_FILTER, &fltr_add) < 0)
		return (QOPERR_SYSCALL);
	fltrinfo->handle = fltr_add.filter_handle;
	return (0);
}

static int
hfsc_delete_filter(struct fltrinfo *fltrinfo)
{
	struct hfsc_delete_filter fltr_del;

	memset(&fltr_del, 0, sizeof(fltr_del));
	strncpy(fltr_del.iface.hfsc_ifname, fltrinfo->clinfo->ifinfo->ifname,
		IFNAMSIZ);
	fltr_del.filter_handle = fltrinfo->handle;

	if (ioctl(hfsc_fd, HFSC_DEL_FILTER, &fltr_del) < 0)
		return (QOPERR_SYSCALL);
	return (0);
}


