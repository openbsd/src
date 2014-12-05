/*	$OpenBSD: in6_proto.c,v 1.76 2014/12/05 15:50:04 mpi Exp $	*/
/*	$KAME: in6_proto.c,v 1.66 2000/10/10 15:35:47 itojun Exp $	*/

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
 *	@(#)in_proto.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/radix.h>
#ifndef SMALL_KERNEL
#include <net/radix_mpath.h>
#endif
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ipip.h>

#ifdef PIM
#include <netinet6/pim6_var.h>
#endif

#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6protosw.h>

#include "gif.h"
#if NGIF > 0
#include <netinet/ip_ether.h>
#include <netinet6/in6_gif.h>
#endif

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include "pf.h"
#if NPF > 0
#include <netinet6/ip6_divert.h>
#endif

/*
 * TCP/IP protocol family: IP6, ICMP6, UDP, TCP.
 */
u_char ip6_protox[IPPROTO_MAX];

struct ip6protosw inet6sw[] = {
{ 0,		&inet6domain,	IPPROTO_IPV6,	0,
  0,		0,		0,		0,
  0,
  ip6_init,	0,		frag6_slowtimo,	frag6_drain,
  ip6_sysctl,
},
{ SOCK_DGRAM,	&inet6domain,	IPPROTO_UDP,	PR_ATOMIC|PR_ADDR|PR_SPLICE,
  udp6_input,	0,		udp6_ctlinput,	ip6_ctloutput,
  udp_usrreq,	0,
  0,		0,		0,
  udp_sysctl,
},
{ SOCK_STREAM,	&inet6domain,	IPPROTO_TCP,	PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS|PR_SPLICE,
  tcp6_input,	0,		tcp6_ctlinput,	tcp_ctloutput,
  tcp_usrreq,
#ifdef INET	/* don't call initialization and timeout routines twice */
  0,		0,		0,		0,
#else
  tcp_init,	tcp_fasttimo,	tcp_slowtimo,	0,
#endif
  tcp_sysctl,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_RAW,	PR_ATOMIC|PR_ADDR,
  rip6_input,	rip6_output,	rip6_ctlinput,	rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,		rip6_sysctl
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ICMPV6,	PR_ATOMIC|PR_ADDR,
  icmp6_input,	rip6_output,	rip6_ctlinput,	rip6_ctloutput,
  rip6_usrreq,
  icmp6_init,	icmp6_fasttimo,	0,		0,
  icmp6_sysctl,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_DSTOPTS,PR_ATOMIC|PR_ADDR,
  dest6_input,	0,	 	0,		0,
  0,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ROUTING,PR_ATOMIC|PR_ADDR,
  route6_input,	0,	 	0,		0,
  0,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_FRAGMENT,PR_ATOMIC|PR_ADDR,
  frag6_input,	0,	 	0,		0,
  0,
  0,		0,		0,		0,
},
#ifdef IPSEC
{ SOCK_RAW,	&inet6domain,	IPPROTO_AH,	PR_ATOMIC|PR_ADDR,
  ah6_input,	rip6_output, 	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
  ah_sysctl,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp6_input,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
  esp_sysctl,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp6_input, rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
  ipcomp_sysctl,
},
#endif /* IPSEC */
#if NGIF > 0
{ SOCK_RAW,	&inet6domain,	IPPROTO_ETHERIP,PR_ATOMIC|PR_ADDR,
  etherip_input6, rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,		etherip_sysctl
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  in6_gif_input, rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,	/* XXX */
  0,		0,		0,		0,
},
#ifdef INET
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  in6_gif_input, rip6_output, 	0,		rip6_ctloutput,
  rip6_usrreq,	/* XXX */
  0,		0,		0,		0,
},
#endif /* INET */
#else /* NGIF */
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  ip4_input6,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,	/* XXX */
  0,		0,		0,		0,		ipip_sysctl
},
#ifdef INET
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  ip4_input6,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,	/* XXX */
  0,		0,		0,		0,
},
#endif /* INET */
#endif /* GIF */
#ifdef PIM
{ SOCK_RAW,	&inet6domain,	IPPROTO_PIM,	PR_ATOMIC|PR_ADDR,
  pim6_input,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,		pim6_sysctl
},
#endif /* PIM */
#if NCARP > 0
{ SOCK_RAW,	&inet6domain,	IPPROTO_CARP,	PR_ATOMIC|PR_ADDR,
  carp6_proto_input,	rip6_output,	0,	rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,		carp_sysctl
},
#endif /* NCARP */
#if NPF > 0
{ SOCK_RAW,	&inet6domain,	IPPROTO_DIVERT,	PR_ATOMIC|PR_ADDR,
  divert6_input,	0,		0,	rip6_ctloutput,
  divert6_usrreq,
  divert6_init,	0,		0,		0,		divert6_sysctl
},
#endif /* NPF > 0 */
/* raw wildcard */
{ SOCK_RAW,	&inet6domain,	0,		PR_ATOMIC|PR_ADDR,
  rip6_input,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,	rip6_init,
  0,		0,		0,
},
};

struct domain inet6domain =
    { AF_INET6, "internet6", 0, 0, 0,
      (struct protosw *)inet6sw,
      (struct protosw *)&inet6sw[nitems(inet6sw)], 0,
#ifndef SMALL_KERNEL
      rn_mpath_inithead,
#else
      rn_inithead,
#endif
      offsetof(struct sockaddr_in6, sin6_addr) << 3,
      sizeof(struct sockaddr_in6),
      in6_domifattach, in6_domifdetach, };

/*
 * Internet configuration info
 */
int	ip6_forwarding = 0;	/* no forwarding unless sysctl'd to enable */
int	ip6_mforwarding = 0;	/* no multicast forwarding unless ... */
int	ip6_multipath = 0;	/* no using multipath routes unless ... */
int	ip6_sendredirects = 1;
int	ip6_defhlim = IPV6_DEFHLIM;
int	ip6_defmcasthlim = IPV6_DEFAULT_MULTICAST_HOPS;
int	ip6_maxfragpackets = 200;
int	ip6_maxfrags = 200;
int	ip6_log_interval = 5;
int	ip6_hdrnestlimit = 10;	/* appropriate? */
int	ip6_dad_count = 1;	/* DupAddrDetectionTransmits */
int	ip6_dad_pending;	/* number of currently running DADs */
int	ip6_auto_flowlabel = 1;
int	ip6_use_deprecated = 1;	/* allow deprecated addr (RFC2462 5.5.4) */
int	ip6_rr_prune = 5;	/* router renumbering prefix
				 * walk list every 5 sec.    */
int	ip6_mcast_pmtu = 0;	/* enable pMTU discovery for multicast? */
const int ip6_v6only = 1;
int	ip6_neighborgcthresh = 2048; /* Threshold # of NDP entries for GC */
int	ip6_maxifprefixes = 16; /* Max acceptable prefixes via RA per IF */
int	ip6_maxifdefrouters = 16; /* Max acceptable def routers via RA */
int	ip6_maxdynroutes = 4096; /* Max # of routes created via redirect */
time_t	ip6_log_time = (time_t)0L;

/* raw IP6 parameters */
/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPV6SNDQ	8192
#define	RIPV6RCVQ	8192

u_long	rip6_sendspace = RIPV6SNDQ;
u_long	rip6_recvspace = RIPV6RCVQ;

/* ICMPV6 parameters */
int	icmp6_redirtimeout = 10 * 60;	/* 10 minutes */
int	icmp6errppslim = 100;		/* 100pps */
int	ip6_mtudisc_timeout = IPMTUDISCTIMEOUT;
