/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/route.h>
#include <net/radix.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcpip.h>
#if __FreeBSD__ && defined(_NETINET_IN_PCB_H_)
#undef _NETINET_IN_PCB_H_
#include <netinet/tcp_var.h>
#define _NETINET_IN_PCB_H_
#else /* __FreeBSD__ */
#include <netinet/tcp_var.h>
#endif /* __FreeBSD__ */
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>

#if defined(IPSEC) || defined(NRL_IPSEC)
#include <netsec/ipsec.h>
#endif /* defined(IPSEC) || defined(NRL_IPSEC) */

#if __FreeBSD__
#include <sys/sysctl.h>
#endif /* __FreeBSD__ */

extern struct domain inet6domain;

#define CAST (void *)

#if !__FreeBSD__
struct protosw inet6sw[] = {
/* normal protocol switch */
  {
    0, &inet6domain, 0, 0,    /* NOTE:  This 0 is the same as IPPROTO_HOPOPTS,
				 but we specially demux IPPROTO_HOPOPTS
				 in ipv6_input(). */
    CAST ipv6_hop, CAST ipv6_output, 0, 0, /* Watch for hop-by-hop input! */
    0,
    ipv6_init, 0, ipv6_slowtimo, ipv6_drain, ipv6_sysctl
  },

  /* ICMPv6 entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_ICMPV6, PR_ATOMIC|PR_ADDR,
    CAST ipv6_icmp_input, CAST ipv6_icmp_output, 0, ripv6_ctloutput,
    ipv6_icmp_usrreq,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, ipv6_icmp_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

  /* IPv6-in-IPv6 tunnel entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_IPV6, PR_ATOMIC|PR_ADDR,
    CAST ipv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    ripv6_usrreq,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, ipv6_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

  /* IPv4-in-IPv6 tunnel entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_IPV4, PR_ATOMIC|PR_ADDR,
    CAST ipv4_input, 0, 0, 0,
    0,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, ip_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

  /* Fragment entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_FRAGMENT, PR_ATOMIC|PR_ADDR,
    CAST ipv6_reasm, 0, 0, 0,
    0,
    0, 0, 0, 0, 0
  },


  /* UDP entry */

  /*
   * Eventually, that ipv6_ctloutput() will have to be replaced with a
   * udp_ctloutput(), which knows whether or not to redirect things down to
   * IP or IPv6 appropriately.
   */

  {
    SOCK_DGRAM, &inet6domain, IPPROTO_UDP, PR_ATOMIC|PR_ADDR,
    CAST udp_input, 0, CAST udp_ctlinput, ipv6_ctloutput,
    udp_usrreq,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, udp_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

  /* TCP entry */

  {
    SOCK_STREAM, &inet6domain, IPPROTO_TCP, PR_CONNREQUIRED|PR_WANTRCVD,
    CAST tcp_input, 0, CAST tcp_ctlinput, tcp_ctloutput,
    tcp_usrreq,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, tcp_sysctl /* init, fasttimo, etc. in v4 protosw already! */
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0  /* init, fasttimo, etc. in v4 protosw already! */
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

#if defined(IPSEC) || defined(NRL_IPSEC)
  /* IPv6 & IPv4 Authentication Header */
  {
    SOCK_RAW, &inet6domain, IPPROTO_AH, PR_ATOMIC|PR_ADDR,
    CAST ipsec_ah_input, 0, 0, 0,
    0,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, ipsec_ah_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },

#ifdef IPSEC_ESP
  /* IPv6 & IPv4 Encapsulating Security Payload Header */
  {
    SOCK_RAW, &inet6domain, IPPROTO_ESP, PR_ATOMIC|PR_ADDR,
    CAST ipsec_esp_input, 0, 0, 0,
    0,
#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
    0, 0, 0, 0, ipsec_esp_sysctl
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
    0, 0, 0, 0, 0
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
  },
#endif /* IPSEC_ESP */
#endif /* defined(IPSEC) || defined(NRL_IPSEC) */

  /* Unknown header. */

  {
    SOCK_RAW, &inet6domain, IPPROTO_RAW, PR_ATOMIC|PR_ADDR,
    CAST ripv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    ripv6_usrreq,
    0,0,0,0,0
  },

  /* Raw wildcard */
  {
    SOCK_RAW, &inet6domain, 0, PR_ATOMIC|PR_ADDR,
    CAST ripv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    ripv6_usrreq,
    ripv6_init,0,0,0,0
  },
};
#else /* !__FreeBSD__ */
extern struct pr_usrreqs nousrreqs;
struct protosw inet6sw[] = {
  {
    0, &inet6domain, 0, 0,    /* NOTE:  This 0 is the same as IPPROTO_HOPOPTS,
				 but we specially demux IPPROTO_HOPOPTS
				 in ipv6_input(). */
    CAST ipv6_hop, CAST ipv6_output, 0, 0, /* Watch for hop-by-hop input! */
    0,
    ipv6_init, 0, ipv6_slowtimo, ipv6_drain, 
    &nousrreqs
  },

  /* ICMPv6 entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_ICMPV6, PR_ATOMIC|PR_ADDR,
    CAST ipv6_icmp_input, CAST ipv6_icmp_output, 0, CAST ripv6_ctloutput,
    0,
    0, 0, 0, 0,
    &ipv6_icmp_usrreqs,
  },

  /* IPv6-in-IPv6 tunnel entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_IPV6, PR_ATOMIC|PR_ADDR,
    CAST ipv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    0,
    0, 0, 0, 0,
    &ripv6_usrreqs
  },

  /* IPv4-in-IPv6 tunnel entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_IPV4, PR_ATOMIC|PR_ADDR,
    CAST ipv4_input, 0, 0, 0,
    0,
    0, 0, 0, 0,
    &nousrreqs
  },

  /* Fragment entry */

  {
    SOCK_RAW, &inet6domain, IPPROTO_FRAGMENT, PR_ATOMIC|PR_ADDR,
    CAST ipv6_reasm, 0, 0, 0,
    0,
    0, 0, 0, 0,
    &nousrreqs
  },


  /* UDP entry */

  /*
   * Eventually, that ipv6_ctloutput() will have to be replaced with a
   * udp_ctloutput(), which knows whether or not to redirect things down to
   * IP or IPv6 appropriately.
   */

  {
    SOCK_DGRAM, &inet6domain, IPPROTO_UDP, PR_ATOMIC|PR_ADDR,
    CAST udp_input, 0, CAST udp_ctlinput, ipv6_ctloutput,
    0,
    udp_init, 0, 0, 0,
    &udp_usrreqs
  },

  /* TCP entry */

  {
    SOCK_STREAM, &inet6domain, IPPROTO_TCP, PR_CONNREQUIRED|PR_WANTRCVD,
    CAST tcp_input, 0, CAST tcp_ctlinput, tcp_ctloutput,
    0,
    tcp_init,	tcp_fasttimo,	tcp_slowtimo,	tcp_drain, 
    &tcp_usrreqs,
  },

#if defined(IPSEC) || defined(NRL_IPSEC)
  /* IPv6 & IPv4 Authentication Header */
  {
    SOCK_RAW, &inet6domain, IPPROTO_AH, PR_ATOMIC|PR_ADDR,
    CAST ipsec_ah_input, 0, 0, 0,
    0,
    0, 0, 0, 0,
    &nousrreqs
  },

#ifdef IPSEC_ESP
  /* IPv6 & IPv4 Encapsulating Security Payload Header */
  {
    SOCK_RAW, &inet6domain, IPPROTO_ESP, PR_ATOMIC|PR_ADDR,
    CAST ipsec_esp_input, 0, 0, 0,
    0,
    0, 0, 0, 0,
    &nousrreqs
  },
#endif /* IPSEC_ESP */
#endif /* defined(IPSEC) || defined(NRL_IPSEC) */

  /* Unknown header. */

  {
    SOCK_RAW, &inet6domain, IPPROTO_RAW, PR_ATOMIC|PR_ADDR,
    CAST ripv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    0,
    0,0,0,0,
    &ripv6_usrreqs
  },

  /* Raw wildcard */
  {
    SOCK_RAW, &inet6domain, 0, PR_ATOMIC|PR_ADDR,
    CAST ripv6_input, CAST ripv6_output, 0, ripv6_ctloutput,
    0,
    ripv6_init,0,0,0,
    &ripv6_usrreqs,
  },
};

#endif /* !__FreeBSD__ */

#if !__FreeBSD__
struct domain inet6domain =
{
  PF_INET6, "IPv6", 0, 0, 0,
  inet6sw, &inet6sw[sizeof(inet6sw)/sizeof(inet6sw[0])], 0,
  /*
   * FreeBSD's IPv4 replaces rn_inithead() with an IPv4-specific function.
   * Our IPv6 uses the ifa->ifa_rtrequest() function pointer to intercept
   * rtrequest()s.  The consequence of this is that we use the generic
   * rn_inithead().
   */
  rn_inithead, 64, sizeof(struct sockaddr_in6)
};
#else /* !__FreeBSD__ */
struct domain inet6domain =
{
  PF_INET6, "IPv6", 0, 0, 0,
  inet6sw, &inet6sw[sizeof(inet6sw)/sizeof(inet6sw[0])], 0,
  /*
   * FreeBSD's IPv4 replaces rn_inithead() with an IPv4-specific function.
   * Our IPv6 uses the ifa->ifa_rtrequest() function pointer to intercept
   * rtrequest()s.  The consequence of this is that we use the generic
   * rn_inithead().
   */
  rn_inithead, 64, sizeof(struct sockaddr_in6)
};

DOMAIN_SET(inet6);
#endif /* !__FreeBSD__ */

/* Eventually, make these go away -- if you want to be a router, twiddle the
   sysctls before bringing up your interfaces */

#ifndef IPV6FORWARDING
#ifdef IPV6GATEWAY
#define IPV6FORWARDING 1
#else
#define IPV6FORWARDING 0
#endif /* IPV6GATEWAY */
#endif /* IPV6FORWARDING */

#ifndef IPV6RSOLICIT
#if IPV6FORWARDING
#define IPV6RSOLICIT 0
#else /* IPV6FORWARDING */
#define IPV6RSOLICIT 1
#endif /* IPV6FORWARDING */
#endif /* IPV6RSOLICIT */

#ifndef	IFQMAXLEN
#define	IFQMAXLEN	IFQ_MAXLEN
#endif

int ipv6forwarding = IPV6FORWARDING;
int ipv6rsolicit = IPV6RSOLICIT;
int ipv6_defhoplmt = MAXHOPLIMIT;
int ipv6qmaxlen = IFQMAXLEN;

#if __FreeBSD__
SYSCTL_NODE(_net_inet, IPPROTO_IPV6,      ipv6,     CTLFLAG_RW, 0,  "IPV6");
SYSCTL_NODE(_net_inet, IPPROTO_ICMPV6,    icmpv6,   CTLFLAG_RW, 0,  "ICMPV6");
#endif /* __FreeBSD__ */
