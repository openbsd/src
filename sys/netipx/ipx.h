/*	$OpenBSD: ipx.h,v 1.12 2000/01/17 00:34:00 fgsch Exp $	*/

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
 *	@(#)ipx.h
 *
 * from FreeBSD Id: ipx.h,v 1.7 1996/01/30 22:58:48 mpp Exp
 */

#ifndef _NETIPX_IPX_H_
#define	_NETIPX_IPX_H_

/*
 * Constants and Structures
 */

/*
 * Protocols
 */
#define IPXPROTO_UNKWN		0	/* Unknown */
#define IPXPROTO_RI		1	/* RIP Routing Information */
#define IPXPROTO_PXP		4	/* IPX Packet Exchange Protocol */
#define IPXPROTO_SPX		5	/* SPX Sequenced Packet */
#define IPXPROTO_NCP		17	/* NCP NetWare Core */
#define IPXPROTO_NETBIOS	20	/* Propagated Packet */
#define IPXPROTO_RAW		255	/* Placemarker*/
#define IPXPROTO_MAX		256	/* Placemarker*/

/*
 * Port/Socket numbers: network standard functions
 */

#define IPXPORT_RI		1	/* NS RIP Routing Information */
#define IPXPORT_ECHO		2	/* NS Echo */
#define IPXPORT_RE		3	/* NS Router Error */
#define IPXPORT_FSP		0x0451	/* NW NCP Core Protocol */
#define IPXPORT_SAP		0x0452	/* NW SAP Service Advertising */
#define IPXPORT_RIP		0x0453	/* NW RIP Routing Information */
#define IPXPORT_NETBIOS		0x0455	/* NW NetBIOS */
#define IPXPORT_DIAGS		0x0456	/* NW Diagnostics */
#define IPXPORT_SERIAL		0x0457	/* NW Serialization */
#define IPXPORT_NLSP		0x9001	/* NW NLSP */
#define IPXPORT_WAN		0x9004	/* NW IPXWAN */
#define IPXPORT_PING		0x9086	/* NW IPX Ping */
#define IPXPORT_MOBILE		0x9088	/* NW Mobile IPX Socket */
/*
 * Ports < IPXPORT_RESERVED are reserved for privileged
 */
#define IPXPORT_RESERVED	0x4000
/*
 * Ports > IPXPORT_WELLKNOWN are reserved for privileged
 * processes (e.g. root).
 */
#define IPXPORT_WELLKNOWN	0x6000

/* flags passed to ipx_outputfl as last parameter */

#define IPX_FORWARDING		0x1	/* most of ipx header exists */
#define IPX_ROUTETOIF		0x10	/* same as SO_DONTROUTE */
#define IPX_ALLOWBROADCAST	SO_BROADCAST   /* can send broadcast packets */

#define IPX_MAXHOPS		15

/* flags passed to get/set socket option */
#define SO_HEADERS_ON_INPUT	1
#define SO_HEADERS_ON_OUTPUT	2
#define SO_DEFAULT_HEADERS	3
#define SO_LAST_HEADER		4
#define SO_IPXIP_ROUTE		5
#define SO_SEQNO		6
#define SO_ALL_PACKETS		7
#define SO_MTU			8
#define SO_IPXTUN_ROUTE		9
#define SO_IPX_CHECKSUM		10

/*
 * IPX addressing
 */
#define IPX_HOSTADDRLEN	6
#define IPX_NETADDRLEN	4
#define XXX	__attribute__((packed))

typedef
union ipx_host {
	u_int8_t	c_host[IPX_HOSTADDRLEN]		XXX;
	u_int16_t	s_host[IPX_HOSTADDRLEN/2]	XXX;
} ipx_host_t;

typedef
union ipx_net {
	u_int8_t	c_net[IPX_NETADDRLEN]		XXX;
	u_int16_t	s_net[IPX_NETADDRLEN/2]		XXX;
	u_int32_t	l_net				XXX;
} ipx_net_t;

typedef	u_int16_t	ipx_port_t;

typedef
struct ipx_addr {
	ipx_net_t	ipx_net				XXX;
	ipx_host_t	ipx_host			XXX;
	ipx_port_t	ipx_port			XXX;
} ipx_addr_t;

/*
 * Socket address
 */
struct sockaddr_ipx {
	u_int8_t	sipx_len;
	u_int8_t	sipx_family;
	u_int16_t	sipx_type;
	struct ipx_addr	sipx_addr;
};
#define sipx_net  sipx_addr.ipx_net
#define sipx_network  sipx_addr.ipx_net.l_net
#define sipx_node sipx_addr.ipx_host.c_host
#define sipx_port sipx_addr.ipx_port

/*
 * Definitions for IPX Internet Datagram Protocol
 */
struct ipx {
	u_int16_t	ipx_sum XXX;	/* Checksum */
	u_int16_t	ipx_len XXX;	/* Length, in bytes, including header */
	u_int8_t	ipx_tc  XXX;	/* Transport Control (i.e. hop count) */
	u_int8_t	ipx_pt  XXX;	/* Packet Type (i.e. lev 2 protocol) */
	ipx_addr_t	ipx_dna XXX;	/* Destination Network Address */
	ipx_addr_t	ipx_sna XXX;	/* Source Network Address */
};
#undef	XXX

#define ipx_neteqnn(a,b) \
	(((a).s_net[0]==(b).s_net[0]) && ((a).s_net[1]==(b).s_net[1]))
#define ipx_neteq(a,b) ipx_neteqnn((a).ipx_net, (b).ipx_net)
#define satoipx_addr(sa) (((struct sockaddr_ipx *)&(sa))->sipx_addr)
#define ipx_hosteqnh(s,t) ((s).s_host[0] == (t).s_host[0] && \
	(s).s_host[1] == (t).s_host[1] && (s).s_host[2] == (t).s_host[2])
#define ipx_hosteq(s,t) (ipx_hosteqnh((s).ipx_host,(t).ipx_host))
#define ipx_nullnet(x) \
	(((x).ipx_net.s_net[0]==0) && ((x).ipx_net.s_net[1]==0))
#define ipx_nullhost(x) (((x).ipx_host.s_host[0]==0) && \
	((x).ipx_host.s_host[1]==0) && ((x).ipx_host.s_host[2]==0))
#define ipx_wildnet(x) (((x).ipx_net.s_net[0]==0xffff) && \
	((x).ipx_net.s_net[1]==0xffff))
#define ipx_wildhost(x) (((x).ipx_host.s_host[0]==0xffff) && \
	((x).ipx_host.s_host[1]==0xffff) && ((x).ipx_host.s_host[2]==0xffff))

/*
 * Definitions for inet sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */
#define IPXPROTO_MAXID	(IPXPROTO_SPX + 1)	/* don't list to IPPROTO_MAX */

#define CTL_IPXPROTO_NAMES { \
	{ "ipx", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "spx", CTLTYPE_NODE }, \
};

#ifdef _KERNEL

#define	satosipx(a)	((struct sockaddr_ipx *)(a))
#define	sipxtosa(a)	((struct sockaddr *)(a))

extern int ipxcksum;
extern int ipxforwarding;
extern int ipxnetbios;
extern struct domain ipxdomain;
extern struct sockaddr_ipx ipx_netmask;
extern struct sockaddr_ipx ipx_hostmask;

extern union ipx_net	ipx_zeronet;
extern union ipx_host	ipx_zerohost;
extern union ipx_net	ipx_broadnet;
extern union ipx_host	ipx_broadhost;

extern u_long ipx_pexseq;
extern u_char ipxctlerrmap[];

struct route;
struct sockaddr;
struct socket;
struct ipxpcb;
void	ipx_abort __P((struct ipxpcb *ipxp));
u_short	ipx_cksum __P((struct mbuf *m, int len));
int	ipx_control __P((struct socket *so, u_long cmd, caddr_t data,
			 struct ifnet *ifp));
void	*ipx_ctlinput __P((int cmd, struct sockaddr *arg_as_sa, void *dummy));
int	ipx_ctloutput __P((int req, struct socket *so, int level, int name,
			   struct mbuf **value));
int	ipx_do_route __P((struct ipx_addr *src, struct route *ro));
void	ipx_drop __P((struct ipxpcb *ipxp, int errno));
void	ipx_forward __P((struct mbuf *m));
void	ipx_init __P((void));
void	ipx_input __P((struct mbuf *, ...));
void	ipxintr __P((void));
int	ipx_output __P((struct mbuf *m0, ...));
int	ipx_outputfl __P((struct mbuf *m0, struct route *ro, int flags));
int	ipx_output_type20 __P((struct mbuf *m));
int	ipx_raw_usrreq __P((struct socket *so, int req, struct mbuf *m,
			    struct mbuf *nam, struct mbuf *control));
void	ipx_undo_route __P((struct route *ro));
int	ipx_usrreq __P((struct socket *so, int req, struct mbuf *m,
			struct mbuf *nam, struct mbuf *control));
void	ipx_watch_output __P((struct mbuf *m, struct ifnet *ifp));
int	ipx_sysctl __P((int *, u_int, void *, size_t *, void *, size_t));
void	ipx_printhost __P((struct ipx_addr *addr));

#endif /* _KERNEL */

#endif /* !_NETIPX_IPX_H_ */
