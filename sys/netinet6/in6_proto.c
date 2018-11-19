/*	$OpenBSD: in6_proto.c,v 1.103 2018/11/19 10:15:04 claudio Exp $	*/
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
#include <net/route.h>
#include <net/rtable.h>

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

#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/ip6protosw.h>

#include "gif.h"
#if NGIF > 0
#include <net/if_gif.h>
#endif

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#include "pf.h"
#if NPF > 0
#include <netinet6/ip6_divert.h>
#endif

#include "etherip.h"
#if NETHERIP > 0
#include <net/if_etherip.h>
#endif

#include "gre.h"
#if NGRE > 0
#include <net/if_gre.h>
#endif

/*
 * TCP/IP protocol family: IP6, ICMP6, UDP, TCP.
 */
u_char ip6_protox[IPPROTO_MAX];

const struct protosw inet6sw[] = {
{
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_IPV6,
  .pr_init	= ip6_init,
  .pr_slowtimo	= frag6_slowtimo,
  .pr_sysctl	= ip6_sysctl
},
{
  .pr_type	= SOCK_DGRAM,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_UDP,
  .pr_flags	= PR_ATOMIC|PR_ADDR|PR_SPLICE,
  .pr_input	= udp_input,
  .pr_ctlinput	= udp6_ctlinput,
  .pr_ctloutput	= ip6_ctloutput,
  .pr_usrreq	= udp_usrreq,
  .pr_attach	= udp_attach,
  .pr_detach	= udp_detach,
  .pr_sysctl	= udp_sysctl
},
{
  .pr_type	= SOCK_STREAM,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_TCP,
  .pr_flags	= PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS|PR_SPLICE,
  .pr_input	= tcp_input,
  .pr_ctlinput	= tcp6_ctlinput,
  .pr_ctloutput	= tcp_ctloutput,
  .pr_usrreq	= tcp_usrreq,
  .pr_attach	= tcp_attach,
  .pr_detach	= tcp_detach,
  .pr_sysctl	= tcp_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_RAW,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= rip6_input,
  .pr_ctlinput	= rip6_ctlinput,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_sysctl	= rip6_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_ICMPV6,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= icmp6_input,
  .pr_ctlinput	= rip6_ctlinput,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_init	= icmp6_init,
  .pr_fasttimo	= icmp6_fasttimo,
  .pr_sysctl	= icmp6_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_DSTOPTS,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= dest6_input
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_ROUTING,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= route6_input
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_FRAGMENT,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= frag6_input
},
#ifdef IPSEC
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_AH,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ah6_input,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_sysctl	= ah_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_ESP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= esp6_input,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_sysctl	= esp_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_IPCOMP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ipcomp6_input,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_sysctl	= ipcomp_sysctl
},
#endif /* IPSEC */
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_IPV4,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
#if NGIF > 0
  .pr_input	= in6_gif_input,
#else
  .pr_input	= ipip_input,
#endif
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,	/* XXX */
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_IPV6,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
#if NGIF > 0
  .pr_input	= in6_gif_input,
#else
  .pr_input	= ipip_input,
#endif
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,	/* XXX */
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
},
#if defined(MPLS) && NGIF > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_MPLS,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
#if NGIF > 0
  .pr_input	= in6_gif_input,
#else
  .pr_input	= ipip_input,
#endif
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,	/* XXX */
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
},
#endif /* MPLS */
#if NCARP > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_CARP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= carp6_proto_input,
  .pr_ctloutput = rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_sysctl	= carp_sysctl
},
#endif /* NCARP */
#if NPF > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_DIVERT,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= divert6_usrreq,
  .pr_attach	= divert6_attach,
  .pr_detach	= divert6_detach,
  .pr_init	= divert6_init,
  .pr_sysctl	= divert6_sysctl
},
#endif /* NPF > 0 */
#if NETHERIP > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_ETHERIP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ip6_etherip_input,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
},
#endif /* NETHERIP */
#if NGRE > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_protocol	= IPPROTO_GRE,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= gre_input6,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
},
#endif /* NGRE */
{
  /* raw wildcard */
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inet6domain,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= rip6_input,
  .pr_ctloutput	= rip6_ctloutput,
  .pr_usrreq	= rip6_usrreq,
  .pr_attach	= rip6_attach,
  .pr_detach	= rip6_detach,
  .pr_init	= rip6_init
}
};

struct domain inet6domain = {
  .dom_family = AF_INET6,
  .dom_name = "internet6",
  .dom_protosw = inet6sw,
  .dom_protoswNPROTOSW = &inet6sw[nitems(inet6sw)],
  .dom_rtoffset = offsetof(struct sockaddr_in6, sin6_addr),
  .dom_maxplen = 128,
  .dom_ifattach = in6_domifattach,
  .dom_ifdetach = in6_domifdetach
};

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
int	ip6_mcast_pmtu = 0;	/* enable pMTU discovery for multicast? */
int	ip6_neighborgcthresh = 2048; /* Threshold # of NDP entries for GC */
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
