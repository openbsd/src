/*	$OpenBSD: in_proto.c,v 1.39 2004/04/25 02:48:04 itojun Exp $	*/
/*	$NetBSD: in_proto.c,v 1.14 1996/02/18 18:58:32 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1993
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
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 *
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>
#include <net/radix.h>
#ifndef SMALL_KERNEL
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#endif

#include <netinet/igmp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

/*
 * TCP/IP protocol family: IP, ICMP, UDP, TCP.
 */

#include "gif.h"
#if NGIF > 0
#include <netinet/in_gif.h>
#endif

#ifdef NSIP
#include <netns/ns_var.h>
#include <netns/idp_var.h>
#endif /* NSIP */

#ifdef IPXIP
#include <netipx/ipx.h>
#include <netipx/ipx_ip.h>
#endif /* NSIP */

#ifdef TPIP
#include <netiso/tp_param.h>
#include <netiso/tp_var.h>
#endif /* TPIP */

#ifdef EON
#include <netiso/eonvar.h>
#endif /* EON */

#ifdef MROUTING
#include <netinet/ip_mroute.h>
#endif /* MROUTING */

#ifdef INET6
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ether.h>
#endif

#include <netinet/ip_ipip.h>

#include "gre.h"
#if NGRE > 0
#include <netinet/ip_gre.h>
#include <net/if_gre.h>
#endif

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include "pfsync.h"
#if NPFSYNC > 0
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#endif

extern	struct domain inetdomain;

struct protosw inetsw[] = {
{ 0,		&inetdomain,	0,		0,
  0,		ip_output,	0,		0,
  0,
  ip_init,	0,		ip_slowtimo,	ip_drain,	ip_sysctl
},
{ SOCK_DGRAM,	&inetdomain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR,
  udp_input,	0,		udp_ctlinput,	ip_ctloutput,
  udp_usrreq,
  udp_init,	0,		0,		0,		udp_sysctl
},
{ SOCK_STREAM,	&inetdomain,	IPPROTO_TCP,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS,
  tcp_input,	0,		tcp_ctlinput,	tcp_ctloutput,
  tcp_usrreq,
  tcp_init,	0,		tcp_slowtimo,	tcp_drain,	tcp_sysctl
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inetdomain,	IPPROTO_ICMP,	PR_ATOMIC|PR_ADDR,
  icmp_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  icmp_init,	0,		0,		0,		icmp_sysctl
},
#if NGIF > 0
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  in_gif_input,	rip_output, 	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,		ipip_sysctl
},
#ifdef INET6
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  in_gif_input,	rip_output,	 0,		0,
  rip_usrreq,	/*XXX*/
  0,		0,		0,		0,
},
#endif /* INET6 */
#else /* NGIF */
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPIP,	PR_ATOMIC|PR_ADDR,
  ip4_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,		ipip_sysctl
},
#ifdef INET6
{ SOCK_RAW,	&inetdomain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  ip4_input,	rip_output, 	0,		rip_ctloutput,
  rip_usrreq,	/*XXX*/
  0,		0,		0,		0,
},
#endif /* INET6 */
#endif /*NGIF*/
{ SOCK_RAW,	&inetdomain,	IPPROTO_IGMP,	PR_ATOMIC|PR_ADDR,
  igmp_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  igmp_init,	igmp_fasttimo,	igmp_slowtimo,	0,
},
#ifdef TPIP
{ SOCK_SEQPACKET,&inetdomain,	IPPROTO_TP,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS,
  tpip_input,	0,		tpip_ctlinput,	tp_ctloutput,
  tp_usrreq,
  tp_init,	0,		tp_slowtimo,	tp_drain,
},
#endif /* TPIP */
/* EON (ISO CLNL over IP) */
#ifdef EON
{ SOCK_RAW,	&inetdomain,	IPPROTO_EON,	0,
  eoninput,	0,		eonctlinput,		0,
  0,
  eonprotoinit,	0,		0,		0,
},
#endif /* EON */
#ifdef IPXIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR,
  ipxip_input,	rip_output,	ipxip_ctlinput,	0,
  rip_usrreq,
  ipxipprotoinit,0,		0,		0,
},
#endif /* NSIP */
#ifdef NSIP
{ SOCK_RAW,	&inetdomain,	IPPROTO_IDP,	PR_ATOMIC|PR_ADDR,
  idpip_input,	rip_output,	nsip_ctlinput,	0,
  rip_usrreq,
  0,		0,		0,		0,
},
#endif /* NSIP */
#ifdef IPSEC
{ SOCK_RAW,   &inetdomain,    IPPROTO_AH,     PR_ATOMIC|PR_ADDR,
  ah4_input,   rip_output,    ah4_ctlinput,   rip_ctloutput,
  rip_usrreq,
  0,          0,              0,              0,		ah_sysctl
},
{ SOCK_RAW,   &inetdomain,    IPPROTO_ESP,    PR_ATOMIC|PR_ADDR,
  esp4_input,  rip_output,    esp4_ctlinput,  rip_ctloutput,
  rip_usrreq,
  0,          0,              0,              0,		esp_sysctl
},
{ SOCK_RAW,   &inetdomain,    IPPROTO_ETHERIP, PR_ATOMIC|PR_ADDR,
  etherip_input,  rip_output, 0,              rip_ctloutput,
  rip_usrreq,
  0,          0,              0,              0,		etherip_sysctl
},
{ SOCK_RAW,   &inetdomain,    IPPROTO_IPCOMP, PR_ATOMIC|PR_ADDR,
  ipcomp4_input,  rip_output, 0,              rip_ctloutput,
  rip_usrreq,
  0,          0,              0,              0,                ipcomp_sysctl
},
#endif /* IPSEC */
#if NGRE > 0
{ SOCK_RAW,     &inetdomain,    IPPROTO_GRE,    PR_ATOMIC|PR_ADDR,
  gre_input,    rip_output,     0,              rip_ctloutput,
  rip_usrreq,
  0,            0,              0,             0,		gre_sysctl
},
{ SOCK_RAW,     &inetdomain,    IPPROTO_MOBILE, PR_ATOMIC|PR_ADDR,
  gre_mobile_input,     rip_output,     0,              rip_ctloutput,
  rip_usrreq,
  0,            0,              0,              0,		ipmobile_sysctl
},
#endif /* NGRE > 0 */
#if NCARP > 0
{ SOCK_RAW,	&inetdomain,	IPPROTO_CARP,	PR_ATOMIC|PR_ADDR,
  carp_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,		carp_sysctl
},
#endif /* NCARP > 0 */
#if NPFSYNC > 0
{ SOCK_RAW,	&inetdomain,	IPPROTO_PFSYNC,	PR_ATOMIC|PR_ADDR,
  pfsync_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  0,		0,		0,		0,
},
#endif /* NPFSYNC > 0 */
/* raw wildcard */
{ SOCK_RAW,	&inetdomain,	0,		PR_ATOMIC|PR_ADDR,
  rip_input,	rip_output,	0,		rip_ctloutput,
  rip_usrreq,
  rip_init,	0,		0,		0,
},
};

struct domain inetdomain =
    { AF_INET, "internet", 0, 0, 0,
      inetsw, &inetsw[sizeof(inetsw)/sizeof(inetsw[0])], 0,
#ifndef SMALL_KERNEL
      rn_mpath_inithead,
#else
      rn_inithead,
#endif
      32, sizeof(struct sockaddr_in) };

#ifdef notyet /* XXXX */
#include "hy.h"
#if NHY > 0
/*
 * HYPERchannel protocol family: raw interface.
 */
int	rhy_output();
extern	struct domain hydomain;

struct protosw hysw[] = {
{ SOCK_RAW,	&hydomain,	0,		PR_ATOMIC|PR_ADDR,
  0,		rhy_output,	0,		0,
  rip_usrreq,
  0,		0,		0,		0,
},
};

struct domain hydomain =
    { AF_HYLINK, "hy", 0, 0, 0, hysw, &hysw[sizeof (hysw)/sizeof(hysw[0])] };
#endif
#endif
