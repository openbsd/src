/*	$OpenBSD: qop_priq.h,v 1.1.1.1 2001/06/27 18:23:34 kjc Exp $	*/
/*	$KAME: qop_priq.h,v 1.1 2000/10/18 09:15:19 kjc Exp $	*/
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

#include <altq/altq_priq.h>

/*
 * priq private ifinfo structure
 */
struct priq_ifinfo {
	struct classinfo *default_class;	/* default class */
};

/*
 * priq private classinfo structure
 */
struct priq_classinfo {
	int		pri;
	int		qlimit;
	int		flags;
};

int priq_interface_parser(const char *ifname, int argc, char **argv);
int priq_class_parser(const char *ifname, const char *class_name,
		      const char *parent_name, int argc, char **argv);

int qcmd_priq_add_if(const char *ifname, u_int bandwidth, int flags);
int qcmd_priq_add_class(const char *ifname, const char *class_name,
			int pri, int qlimit, int flags);
int qcmd_priq_modify_class(const char *ifname, const char *class_name,
			int pri, int qlimit, int flags);
int qop_priq_add_if(struct ifinfo **rp, const char *ifname,
		    u_int bandwidth, int flags);
int qop_priq_add_class(struct classinfo **rp, const char *class_name,
		       struct ifinfo *ifinfo,
		       int pri, int qlimit, int flags);
int qop_priq_modify_class(struct classinfo *clinfo, 
			  int pri, int qlimit, int flags);
