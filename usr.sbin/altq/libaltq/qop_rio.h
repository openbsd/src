/*	$OpenBSD: qop_rio.h,v 1.1.1.1 2001/06/27 18:23:36 kjc Exp $	*/
/*	$KAME: qop_rio.h,v 1.2 2000/10/18 09:15:20 kjc Exp $	*/
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

/*
 * rio private ifinfo structure
 */
struct rio_ifinfo {
	struct redparams red_params[RIO_NDROPPREC];
	int weight;		/* weight for EWMA */
	int qlimit;		/* max queue length */
	int pkttime;		/* average packet time in usec */
	int flags;		/* see below */
};

int rio_interface_parser(const char *ifname, int argc, char **argv);
int qcmd_rio_add_if(const char *ifname, u_int bandwidth, int weight,
		    int lo_inv_pmax, int lo_th_min, int lo_th_max,
		    int mid_inv_pmax, int mid_th_min, int mid_th_max,
		    int hi_inv_pmax, int hi_th_min, int hi_th_max,
		    int qlimit, int pkttime, int flags);
int qop_rio_add_if(struct ifinfo **rp, const char *ifname,
		   u_int bandwidth, int weight, struct redparams *red_params,
		   int qlimit, int pkttime, int flags);
