/*	$OpenBSD: qop_hfsc.h,v 1.1.1.1 2001/06/27 18:23:33 kjc Exp $	*/
/*	$KAME: qop_hfsc.h,v 1.2 2000/10/18 09:15:19 kjc Exp $	*/
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

#include <altq/altq_hfsc.h>

/*
 * generalized service curve used for admission control
 */
struct segment {
	LIST_ENTRY(segment)	_next;
	double	x, y, d, m;
};

typedef LIST_HEAD(gen_sc, segment) gsc_head_t;

/*
 * hfsc private ifinfo structure
 */
struct hfsc_ifinfo {
	struct classinfo *root_class;		/* root class */
	struct classinfo *default_class;	/* default class */
};

/*
 * hfsc private classinfo structure
 */
struct hfsc_classinfo {
	struct service_curve rsc;	/* real-time service curve */
	struct service_curve fsc;	/* fair service curve */
	gsc_head_t	gen_rsc;	/* generalized real-time sc */
	gsc_head_t	gen_fsc;	/* generalized fsc */
	int		qlimit;
	int		flags;
};

int hfsc_interface_parser(const char *ifname, int argc, char **argv);
int hfsc_class_parser(const char *ifname, const char *class_name,
		      const char *parent_name, int argc, char **argv);

int qcmd_hfsc_add_if(const char *ifname, u_int bandwidth, int flags);
int qcmd_hfsc_add_class(const char *ifname, const char *class_name,
			const char *parent_name, u_int m1, u_int d, u_int m2,
			int qlimit, int flags);
int qcmd_hfsc_modify_class(const char *ifname, const char *class_name,
			   u_int m1, u_int d, u_int m2, int sctype);

int qop_hfsc_add_if(struct ifinfo **rp, const char *ifname,
		    u_int bandwidth, int flags);
int qop_hfsc_add_class(struct classinfo **rp, const char *class_name,
		       struct ifinfo *ifinfo, struct classinfo *parent, 
		       struct service_curve *sc, int qlimit, int flags);
int qop_hfsc_modify_class(struct classinfo *clinfo, 
			  struct service_curve *sc, int sctype);

