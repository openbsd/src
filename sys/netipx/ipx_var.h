/*	$OpenBSD: ipx_var.h,v 1.6 2003/06/02 23:28:16 millert Exp $	*/

/*-
 *
 * Copyright (c) 1996 Michael Shalayeff
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ipx_var.h
 *
 * from FreeBSD Id: ipx_var.h,v 1.3 1995/11/04 09:03:27 julian Exp
 */

#ifndef _NETIPX_IPX_VAR_H_
#define _NETIPX_IPX_VAR_H_

/*
 * IPX Kernel Structures and Variables
 */
struct	ipxstat {
	u_long  ipxs_total;             /* total packets received */
	u_long  ipxs_badsum;            /* checksum bad */
	u_long  ipxs_tooshort;          /* packet too short */
	u_long  ipxs_toosmall;          /* not enough data */
	u_long  ipxs_forward;           /* packets forwarded */
	u_long  ipxs_cantforward;       /* packets rcvd for unreachable dest */
	u_long  ipxs_delivered;         /* datagrams delivered to upper level*/
	u_long  ipxs_localout;          /* total ipx packets generated here */
	u_long  ipxs_odropped;          /* lost packets due to nobufs, etc. */
	u_long  ipxs_noroute;           /* packets discarded due to no route */
	u_long  ipxs_mtutoosmall;       /* the interface mtu is too small */
};

/*
 * Names for IPX sysctl objects.
 */

#define	IPXCTL_CHECKSUM		1
#define IPXCTL_FORWARDING	2
#define IPXCTL_NETBIOS		3
#define IPXCTL_RECVSPACE	4
#define IPXCTL_SENDSPACE	5
#define	IPXCTL_MAXID		6

#define IPXCTL_NAMES { \
	{ 0, 0}, \
	{ "checksum", CTLTYPE_INT }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "netbios", CTLTYPE_INT }, \
	{ "recvspace", CTLTYPE_INT }, \
	{ "sendspace", CTLTYPE_INT }, \
}

#ifdef _KERNEL
extern struct ipxstat ipxstat;
#endif

#endif
