/*	$OpenBSD: ipx_proto.c,v 1.6 2003/06/02 23:28:16 millert Exp $	*/

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
 *	@(#)ipx_proto.c
 *
 * from FreeBSD Id: ipx_proto.c,v 1.4 1996/01/05 20:47:05 wollman Exp
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>

#include <net/radix.h>

#include <netipx/ipx.h>
#include <netipx/spx.h>

/*
 * IPX protocol family: IPX, ERR, PXP, SPX, ROUTE.
 */

struct protosw ipxsw[] = {
{ 0,		&ipxdomain,	0,		0,
  0,		ipx_output,	0,		0,
  0,
  ipx_init,	0,		0,		0,	ipx_sysctl
},
{ SOCK_DGRAM,	&ipxdomain,	0,		PR_ATOMIC|PR_ADDR,
  0,		0,		ipx_ctlinput,	ipx_ctloutput,
  ipx_usrreq,
  0,		0,		0,		0,	ipx_sysctl
},
{ SOCK_STREAM,	&ipxdomain,	IPXPROTO_SPX,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS,
  spx_input,	0,		spx_ctlinput,	spx_ctloutput,
  spx_usrreq,
  spx_init,	spx_fasttimo,	spx_slowtimo,	0,	spx_sysctl
},
{ SOCK_SEQPACKET,&ipxdomain,	IPXPROTO_SPX,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ATOMIC|PR_ABRTACPTDIS,
  spx_input,	0,		spx_ctlinput,	spx_ctloutput,
  spx_usrreq_sp,
  0,		0,		0,		0,	spx_sysctl
},
{ SOCK_RAW,	&ipxdomain,	IPXPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  ipx_input,	ipx_output,	0,		ipx_ctloutput,
  ipx_raw_usrreq,
  0,		0,		0,		0,	ipx_sysctl
},
#ifdef IPTUNNEL
{ SOCK_RAW,	&ipxdomain,	IPPROTO_IPX,	PR_ATOMIC|PR_ADDR,
  iptun_input,	rip_output,	iptun_ctlinput,	0,
  rip_usrreq,
  0,		0,		0,		0,	ipx_sysctl,
},
#endif
};

struct domain ipxdomain =
    { AF_IPX, "Internetwork Packet Exchange", 0, 0, 0, 
      ipxsw, &ipxsw[sizeof(ipxsw)/sizeof(ipxsw[0])], 0,
      rn_inithead, 16, sizeof(struct sockaddr_ipx)};

