/* $OpenBSD: in6_proto.c,v 1.13 2000/01/13 06:01:22 angelos Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <net/radix.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet6/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/icmp6.h>

#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/ip_ipsp.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ip4.h>

#include <netinet6/pim6_var.h>

#include <netinet6/nd6.h>

#include <netinet6/ip6protosw.h>

#include "gif.h"
#if NGIF > 0
#include <netinet6/in6_gif.h>
#endif

#include <net/net_osdep.h>

#define	offsetof(type, member)	((size_t)(&((type *)0)->member))

/*
 * TCP/IP protocol family: IP6, ICMP6, UDP, TCP.
 */

extern	struct domain inet6domain;

struct ip6protosw inet6sw[] = {
{ 0,		&inet6domain,	IPPROTO_IPV6,	0,
  0,		0,		0,		0,
  0,
  ip6_init,	0,		frag6_slowtimo,	frag6_drain,
  ip6_sysctl,
},
{ SOCK_DGRAM,	&inet6domain,	IPPROTO_UDP,	PR_ATOMIC | PR_ADDR,
  udp6_input,	0,		udp6_ctlinput,	ip6_ctloutput,
 udp6_usrreq,	0,
  0,		0,		0,
  udp_sysctl,
},
#ifdef TCP6
{ SOCK_STREAM,	&inet6domain,	IPPROTO_TCP,	PR_CONNREQUIRED | PR_WANTRCVD,
  tcp6_input,	0,		tcp6_ctlinput,	tcp6_ctloutput,
  tcp6_usrreq,
  tcp6_init,	tcp6_fasttimo,	tcp6_slowtimo,	tcp6_drain,
  tcp6_sysctl,
},
#else
{ SOCK_STREAM,	&inet6domain,	IPPROTO_TCP,	PR_CONNREQUIRED | PR_WANTRCVD,
  tcp6_input,	0,		tcp6_ctlinput,	tcp_ctloutput,
  tcp6_usrreq,
#ifdef INET	/* don't call timeout routines twice */
  tcp_init,	0,		0,		tcp_drain,
#else
  tcp_init,	tcp_fasttimo,	tcp_slowtimo,	tcp_drain,
#endif
  tcp_sysctl,
},
#endif /*TCP6*/
{ SOCK_RAW,	&inet6domain,	IPPROTO_RAW,	PR_ATOMIC | PR_ADDR,
  rip6_input,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq,
  0,		0,		0,		0,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ICMPV6,	PR_ATOMIC | PR_ADDR,
  icmp6_input,	rip6_output,	0,		rip6_ctloutput,
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
  ah6_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  ah_sysctl,
},
{ SOCK_RAW,	&inet6domain,	IPPROTO_ESP,	PR_ATOMIC|PR_ADDR,
  esp6_input,	0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  esp_sysctl,
},
#if 0
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPCOMP,	PR_ATOMIC|PR_ADDR,
  ipcomp6_input, 0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
  ipsec6_sysctl,
},
#endif /* 0 */
#endif /* IPSEC */
#if NGIF > 0
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV4,	PR_ATOMIC|PR_ADDR,
  in6_gif_input,0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
},
#ifdef INET6
{ SOCK_RAW,	&inet6domain,	IPPROTO_IPV6,	PR_ATOMIC|PR_ADDR,
  in6_gif_input,0,	 	0,		0,
  0,	  
  0,		0,		0,		0,
},
#endif /* INET6 */
#else /* NFIG */
{ SOCK_RAW,     &inet6domain,    IPPROTO_IPV4,  PR_ATOMIC|PR_ADDR,
  ip4_input6,   rip6_output,     0,              rip6_ctloutput,
  rip6_usrreq,   /* XXX */
  0,            0,              0,              0,              ip4_sysctl
},
#ifdef INET6
{ SOCK_RAW,     &inet6domain,    IPPROTO_IPV6,  PR_ATOMIC|PR_ADDR,
  ip4_input6,   rip6_output,     0,              rip6_ctloutput,
  0,   
  0,            0,              0,              0,
},
#endif /* INET6 */
#endif /* GIF */
{ SOCK_RAW,     &inet6domain,	IPPROTO_PIM,	PR_ATOMIC|PR_ADDR,
  pim6_input,    rip6_output,	0,              rip6_ctloutput, 
  rip6_usrreq,
  0,            0,              0,              0,
},
/* raw wildcard */
{ SOCK_RAW,	&inet6domain,	0,		PR_ATOMIC | PR_ADDR,
  rip6_input,	rip6_output,	0,		rip6_ctloutput,
  rip6_usrreq, rip6_init,
  0,		0,		0,
},
};

struct domain inet6domain =
    { AF_INET6, "internet6", 0, 0, 0, 
      (struct protosw *)inet6sw,
      (struct protosw *)&inet6sw[sizeof(inet6sw)/sizeof(inet6sw[0])], 0,
      rn_inithead,
      offsetof(struct sockaddr_in6, sin6_addr) << 3,
      sizeof(struct sockaddr_in6) };

/*
 * Internet configuration info
 */
#ifndef	IPV6FORWARDING
#ifdef GATEWAY6
#define	IPV6FORWARDING	1	/* forward IP6 packets not for us */
#else
#define	IPV6FORWARDING	0	/* don't forward IP6 packets not for us */
#endif /* GATEWAY6 */
#endif /* !IPV6FORWARDING */

#ifndef	IPV6_SENDREDIRECTS
#define	IPV6_SENDREDIRECTS	1
#endif

int	ip6_forwarding = IPV6FORWARDING;	/* act as router? */
int	ip6_sendredirects = IPV6_SENDREDIRECTS;
int	ip6_defhlim = IPV6_DEFHLIM;
int	ip6_defmcasthlim = IPV6_DEFAULT_MULTICAST_HOPS;
int	ip6_accept_rtadv = 0;	/* "IPV6FORWARDING ? 0 : 1" is dangerous */
int	ip6_maxfragpackets = 200;
int	ip6_log_interval = 5;
int	ip6_hdrnestlimit = 50;	/* appropriate? */
int	ip6_dad_count = 1;	/* DupAddrDetectionTransmits */
u_int32_t ip6_flow_seq;
int	ip6_auto_flowlabel = 1;
#if NGIF > 0
int	ip6_gif_hlim = GIF_HLIM;
#else
int	ip6_gif_hlim = 0;
#endif
int	ip6_use_deprecated = 1;	/* allow deprecated addr (RFC2462 5.5.4) */
int	ip6_rr_prune = 5;	/* router renumbering prefix
				 * walk list every 5 sec.    */
#ifdef MAPPED_ADDR_ENABLED
int	ip6_mapped_addr_on = 1;
#endif /* MAPPED_ADDR_ENABLED */

u_int32_t ip6_id = 0UL;
int	ip6_keepfaith = 0;
time_t	ip6_log_time = (time_t)0L;

/* icmp6 */
/*
 * BSDI4 defines these variables in in_proto.c...
 * XXX: what if we don't define INET? Should we define pmtu6_expire
 * or so? (jinmei@kame.net 19990310)
 */
int pmtu_expire = 60*10;
int pmtu_probe = 60*2;

/* raw IP6 parameters */
/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPV6SNDQ	8192
#define	RIPV6RCVQ	8192

u_long	rip6_sendspace = RIPV6SNDQ;
u_long	rip6_recvspace = RIPV6RCVQ;

/* ICMPV6 parameters */
int	icmp6_rediraccept = 1;		/* accept and process redirects */
int	icmp6_redirtimeout = 10 * 60;	/* 10 minutes */
u_int	icmp6errratelim = 1000;		/* 1000usec = 1msec */
int	icmp6_nodeinfo = 1;		/* enable/disable NI response */

#ifdef TCP6
/* TCP on IP6 parameters */
int	tcp6_sendspace = 1024 * 8;
int	tcp6_recvspace = 1024 * 8;
int 	tcp6_mssdflt = TCP6_MSS;
int 	tcp6_rttdflt = TCP6TV_SRTTDFLT / PR_SLOWHZ;
int	tcp6_do_rfc1323 = 1;
int	tcp6_conntimeo = TCP6TV_KEEP_INIT;	/* initial connection timeout */
int	tcp6_43maxseg = 0;
int	tcp6_pmtu = 0;

/*
 * Parameters for keepalive option.
 * Connections for which SO_KEEPALIVE is set will be probed
 * after being idle for a time of tcp6_keepidle (in units of PR_SLOWHZ).
 * Starting at that time, the connection is probed at intervals
 * of tcp6_keepintvl (same units) until a response is received
 * or until tcp6_keepcnt probes have been made, at which time
 * the connection is dropped.  Note that a tcp6_keepidle value
 * under 2 hours is nonconformant with RFC-1122, Internet Host Requirements.
 */
int	tcp6_keepidle = TCP6TV_KEEP_IDLE;	/* time before probing idle */
int	tcp6_keepintvl = TCP6TV_KEEPINTVL;	/* interval betwn idle probes */
int	tcp6_keepcnt = TCP6TV_KEEPCNT;		/* max idle probes */
int	tcp6_maxpersistidle = TCP6TV_KEEP_IDLE;	/* max idle time in persist */

#ifndef INET_SERVER
#define	TCP6_LISTEN_HASH_SIZE	17
#define	TCP6_CONN_HASH_SIZE	97
#define	TCP6_SYN_HASH_SIZE	293
#define	TCP6_SYN_BUCKET_SIZE	35
#else
#define	TCP6_LISTEN_HASH_SIZE	97
#define	TCP6_CONN_HASH_SIZE	9973
#define	TCP6_SYN_HASH_SIZE	997
#define	TCP6_SYN_BUCKET_SIZE	35
#endif
int	tcp6_listen_hash_size = TCP6_LISTEN_HASH_SIZE;
int	tcp6_conn_hash_size = TCP6_CONN_HASH_SIZE;
struct	tcp6_hash_list tcp6_listen_hash[TCP6_LISTEN_HASH_SIZE],
	tcp6_conn_hash[TCP6_CONN_HASH_SIZE];

int	tcp6_syn_cache_size = TCP6_SYN_HASH_SIZE;
int	tcp6_syn_cache_limit = TCP6_SYN_HASH_SIZE*TCP6_SYN_BUCKET_SIZE;
int	tcp6_syn_bucket_limit = 3*TCP6_SYN_BUCKET_SIZE;
struct	syn_cache_head6 tcp6_syn_cache[TCP6_SYN_HASH_SIZE];
struct	syn_cache_head6 *tcp6_syn_cache_first;
int	tcp6_syn_cache_interval = 8;	/* runs timer every 4 seconds */
int	tcp6_syn_cache_timeo = TCP6TV_KEEP_INIT;

/*
 * Parameters for computing a desirable data segment size
 * given an upper bound (either interface MTU, or peer's MSS option)_.
 * As applications tend to use a buffer size that is a multiple
 * of kilobytes, try for something that divides evenly. However,
 * do not round down too much.
 *
 * Round segment size down to a multiple of TCP6_ROUNDSIZE if this
 * does not result in lowering by more than (size/TCP6_ROUNDFRAC).
 * For example, round 536 to 512.  Older versions of the system
 * effectively used MCLBYTES (1K or 2K) as TCP6_ROUNDSIZE, with
 * a value of 1 for TCP6_ROUNDFRAC (eliminating its effect).
 * We round to a multiple of 256 for SLIP.
 */
#ifndef	TCP6_ROUNDSIZE
#define	TCP6_ROUNDSIZE	256	/* round to multiple of 256 */
#endif
#ifndef	TCP6_ROUNDFRAC
#define	TCP6_ROUNDFRAC	10	/* round down at most N/10, or 10% */
#endif

int	tcp6_roundsize = TCP6_ROUNDSIZE;
int	tcp6_roundfrac = TCP6_ROUNDFRAC;
#endif /*TCP6*/

/* UDP on IP6 parameters */
int	udp6_sendspace = 9216;		/* really max datagram size */
int	udp6_recvspace = 40 * (1024 + sizeof(struct sockaddr_in6));
					/* 40 1K datagrams */
