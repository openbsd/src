/*	$OpenBSD: in_proto.c,v 1.91 2018/11/19 10:15:04 claudio Exp $	*/
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
#include <net/if_var.h>
#include <net/route.h>
#include <net/rtable.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/in_pcb.h>

#ifdef INET6
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
#include <net/if_gif.h>
#endif

#ifdef INET6
#include <netinet6/ip6_var.h>
#endif /* INET6 */

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#endif

#include <netinet/ip_ether.h>
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

#include "pf.h"
#if NPF > 0
#include <netinet/ip_divert.h>
#endif

#include "etherip.h"
#if NETHERIP > 0
#include <net/if_etherip.h>
#endif

#include "mobileip.h"
#if NMOBILEIP > 0
#include <net/if_mobileip.h>
#endif

u_char ip_protox[IPPROTO_MAX];

const struct protosw inetsw[] = {
{
  .pr_domain	= &inetdomain,
  .pr_init	= ip_init,
  .pr_slowtimo	= ip_slowtimo,
  .pr_sysctl	= ip_sysctl
},
{
  .pr_type	= SOCK_DGRAM,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_UDP,
  .pr_flags	= PR_ATOMIC|PR_ADDR|PR_SPLICE,
  .pr_input	= udp_input,
  .pr_ctlinput	= udp_ctlinput,
  .pr_ctloutput	= ip_ctloutput,
  .pr_usrreq	= udp_usrreq,
  .pr_attach	= udp_attach,
  .pr_detach	= udp_detach,
  .pr_init	= udp_init,
  .pr_sysctl	= udp_sysctl
},
{
  .pr_type	= SOCK_STREAM,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_TCP,
  .pr_flags	= PR_CONNREQUIRED|PR_WANTRCVD|PR_ABRTACPTDIS|PR_SPLICE,
  .pr_input	= tcp_input,
  .pr_ctlinput	= tcp_ctlinput,
  .pr_ctloutput	= tcp_ctloutput,
  .pr_usrreq	= tcp_usrreq,
  .pr_attach	= tcp_attach,
  .pr_detach	= tcp_detach,
  .pr_init	= tcp_init,
  .pr_slowtimo	= tcp_slowtimo,
  .pr_sysctl	= tcp_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_RAW,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= rip_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_ICMP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= icmp_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_init	= icmp_init,
  .pr_sysctl	= icmp_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_IPV4,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
#if NGIF > 0
  .pr_input	= in_gif_input,
#else
  .pr_input	= ipip_input,
#endif
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= ipip_sysctl,
  .pr_init	= ipip_init
},
#ifdef INET6
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_IPV6,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
#if NGIF > 0
  .pr_input	= in_gif_input,
#else
  .pr_input	= ipip_input,
#endif
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq, /* XXX */
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
},
#endif
#if defined(MPLS) && NGIF > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_MPLS,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= in_gif_input,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
},
#endif /* MPLS && GIF */
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_IGMP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= igmp_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_init	= igmp_init,
  .pr_fasttimo	= igmp_fasttimo,
  .pr_slowtimo	= igmp_slowtimo,
  .pr_sysctl	= igmp_sysctl
},
#ifdef IPSEC
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_AH,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ah4_input,
  .pr_ctlinput	= ah4_ctlinput,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= ah_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_ESP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= esp4_input,
  .pr_ctlinput	= esp4_ctlinput,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= esp_sysctl
},
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_IPCOMP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ipcomp4_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= ipcomp_sysctl
},
#endif /* IPSEC */
#if NGRE > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_GRE,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= gre_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= gre_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= gre_sysctl
},
#endif /* NGRE > 0 */
#if NMOBILEIP > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_MOBILE,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= mobileip_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= mobileip_sysctl
},
#endif /* NMOBILEIP > 0 */
#if NCARP > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_CARP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= carp_proto_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= carp_sysctl
},
#endif /* NCARP > 0 */
#if NPFSYNC > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_PFSYNC,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= pfsync_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= pfsync_sysctl
},
#endif /* NPFSYNC > 0 */
#if NPF > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_DIVERT,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= divert_usrreq,
  .pr_attach	= divert_attach,
  .pr_detach	= divert_detach,
  .pr_init	= divert_init,
  .pr_sysctl	= divert_sysctl
},
#endif /* NPF > 0 */
#if NETHERIP > 0
{
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_protocol	= IPPROTO_ETHERIP,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= ip_etherip_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_sysctl	= etherip_sysctl
},
#endif /* NETHERIP */
{
  /* raw wildcard */
  .pr_type	= SOCK_RAW,
  .pr_domain	= &inetdomain,
  .pr_flags	= PR_ATOMIC|PR_ADDR,
  .pr_input	= rip_input,
  .pr_ctloutput	= rip_ctloutput,
  .pr_usrreq	= rip_usrreq,
  .pr_attach	= rip_attach,
  .pr_detach	= rip_detach,
  .pr_init	= rip_init
}
};

struct domain inetdomain = {
  .dom_family = AF_INET,
  .dom_name = "internet",
  .dom_protosw = inetsw,
  .dom_protoswNPROTOSW = &inetsw[nitems(inetsw)],
  .dom_rtoffset = offsetof(struct sockaddr_in, sin_addr),
  .dom_maxplen = 32
};
