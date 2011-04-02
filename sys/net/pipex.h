/*	$OpenBSD: pipex.h,v 1.7 2011/04/02 11:37:10 dlg Exp $	*/

/*
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef NET_PIPEX_H
#define NET_PIPEX_H 1

#define PIPEX_ENABLE			1
#define PIPEX_DISABLE			0

#define PIPEX_PROTO_L2TP		0x0001  /* protocol L2TP */
#define PIPEX_PROTO_PPTP		0x0002	/* protocol pptp */
#define PIPEX_PROTO_PPPOE		0x0003	/* protocol pppoe */
#define PIPEX_MAX_LISTREQ		128	/* list request size */
#define	PIPEX_MPPE_KEYLEN		16

/* pipex_mppe */
struct pipex_mppe_req {
	int16_t	stateless;			/* mppe key mode */
	int16_t	keylenbits;			/* mppe key length(in bits)*/
	u_char	master_key[PIPEX_MPPE_KEYLEN];	/* mppe mastter key */
};

/* pppac ip-extension forwarded statistics */
struct pipex_statistics {
	uint32_t ipackets;			/* tunnel to network */
	uint32_t ierrors;			/* tunnel to network */
	uint64_t ibytes;			/* tunnel to network */
	uint32_t opackets;			/* network to tunnel */
	uint32_t oerrors;			/* network to tunnel */
	uint64_t obytes;			/* network to tunnel */

	uint32_t idle_time;			/* idle timer */
};

struct pipex_session_req {
	int		pr_protocol;		/* tunnel protocol  */
/*	u_int		pr_rdomain;	*/	/* rdomain id */
	uint16_t	pr_session_id;		/* session-id */
	uint16_t	pr_peer_session_id;	/* peer's session-id */
	uint32_t	pr_ppp_flags;	/* PPP configuration flags */
#define	PIPEX_PPP_ACFC_ACCEPTED		0x00000001
#define	PIPEX_PPP_PFC_ACCEPTED		0x00000002
#define	PIPEX_PPP_ACFC_ENABLED		0x00000004
#define	PIPEX_PPP_PFC_ENABLED		0x00000008
#define	PIPEX_PPP_MPPE_ACCEPTED		0x00000010
#define	PIPEX_PPP_MPPE_ENABLED		0x00000020
#define	PIPEX_PPP_MPPE_REQUIRED		0x00000040
#define	PIPEX_PPP_HAS_ACF		0x00000080
#define	PIPEX_PPP_ADJUST_TCPMSS		0x00000100
	int8_t		pr_ccp_id;		/* CCP current packet id */
	int		pr_ppp_id;		/* PPP Id. */
	uint16_t	pr_peer_mru; 		/* Peer's MRU */
	uint16_t	pr_timeout_sec; 	/* Idle Timer */

	struct in_addr	pr_ip_srcaddr;		/* local framed IP-Address */
	struct in_addr	pr_ip_address;		/* framed IP-Address */
	struct in_addr	pr_ip_netmask;		/* framed IP-Netmask */
	struct sockaddr_in6 pr_ip6_address;	/* framed IPv6-Address */
	int		pr_ip6_prefixlen;	/* framed IPv6-Prefixlen */
	union {
		struct {
			uint32_t snd_nxt;	/* send next */
			uint32_t rcv_nxt;	/* receive next */
			uint32_t snd_una;	/* unacked */
			uint32_t rcv_acked;	/* recv acked */
			int winsz;		/* window size */
			int maxwinsz;		/* max window size */
			int peer_maxwinsz;	/* peer's max window size */
		} pptp;
		struct {
			/* select protocol options: 1 for enable */
			uint32_t option_flags;
#define	PIPEX_L2TP_USE_SEQUENCING	0x00000001

			/* session keys */
			uint16_t tunnel_id;	/* our tunnel-id */
			uint16_t peer_tunnel_id;/* peer's tunnel-id */
			uint32_t ns_nxt;	/* send next */
			uint32_t nr_nxt;	/* receive next */
			uint32_t ns_una;	/* unacked */
			uint32_t nr_acked;	/* recv acked */
		} l2tp;
		struct {
			char over_ifname[IF_NAMESIZE]; 	/* ethernet i/f name */
		} pppoe;
	} pr_proto;
	struct sockaddr_storage		peer_address;
	struct sockaddr_storage		local_address;
	struct pipex_mppe_req pr_mppe_recv;
	struct pipex_mppe_req pr_mppe_send;
};

struct pipex_session_stat_req {
	int		psr_protocol;		/* tunnel protocol  */
	uint16_t	psr_session_id;		/* session-id */
	struct pipex_statistics psr_stat;	/* statistics */
};
#define pipex_session_close_req		pipex_session_stat_req
#define	pcr_protocol	psr_protocol
#define	pcr_session_id	psr_session_id
#define	pcr_stat	psr_stat

struct pipex_session_list_req {
	uint8_t	plr_flags;
#define	PIPEX_LISTREQ_NONE		0x00
#define	PIPEX_LISTREQ_MORE		0x01
	int	plr_ppp_id_count;		/* count of PPP id */
	int	plr_ppp_id[PIPEX_MAX_LISTREQ];	/* PPP id */
};

struct pipex_session_config_req {
	int		pcr_protocol;		/* tunnel protocol  */
	uint16_t	pcr_session_id;		/* session-id */
	int		pcr_ip_forward;		/* ip_forwarding on/off */
};

/* for pppx(4) */
struct pppx_hdr {
	u_int32_t	pppx_proto;
	u_int32_t	pppx_id;
};


/* PIPEX ioctls */
#define PIPEXSMODE	_IOW ('p',  1, int)
#define PIPEXGMODE	_IOR ('p',  2, int)
#define PIPEXASESSION	_IOW ('p',  3, struct pipex_session_req)
#define PIPEXDSESSION	_IOWR('p',  4, struct pipex_session_close_req)
#define PIPEXCSESSION	_IOW ('p',  5, struct pipex_session_config_req)
#define PIPEXGSTAT	_IOWR('p',  6, struct pipex_session_stat_req)
#define PIPEXGCLOSED	_IOR ('p',  7, struct pipex_session_list_req)

#ifdef _KERNEL

struct pipex_session;

/* pipex context for a interface. */
struct pipex_iface_context {
	struct	ifnet *ifnet_this;	/* outer interface */
	u_int	pipexmode;		/* pppac ipex mode */
	/* virtual pipex_session entry for multicast routing */
	struct pipex_session *multicast_session;
};
#include <sys/cdefs.h>
__BEGIN_DECLS
void                  pipex_init (void);
void                  pipex_iface_init (struct pipex_iface_context *, struct ifnet *);
void                  pipex_iface_start (struct pipex_iface_context *);
void                  pipex_iface_stop (struct pipex_iface_context *);

int                   pipex_notify_close_session(struct pipex_session *session);
int                   pipex_notify_close_session_all(void);

struct mbuf           *pipex_output (struct mbuf *, int, int, struct pipex_iface_context *);
struct pipex_session  *pipex_pppoe_lookup_session (struct mbuf *);
struct pipex_session  *pipex_pppoe_lookup_session (struct mbuf *);
struct mbuf           *pipex_pppoe_input (struct mbuf *, struct pipex_session *);
struct pipex_session  *pipex_pptp_lookup_session (struct mbuf *);
struct mbuf           *pipex_pptp_input (struct mbuf *, struct pipex_session *);
struct pipex_session  *pipex_pptp_userland_lookup_session_ipv4 (struct mbuf *, struct in_addr);
struct pipex_session  *pipex_pptp_userland_lookup_session_ipv6 (struct mbuf *, struct in6_addr);
struct mbuf           *pipex_pptp_userland_output (struct mbuf *, struct pipex_session *);
struct pipex_session  *pipex_l2tp_lookup_session (struct mbuf *, int);
struct mbuf           *pipex_l2tp_input (struct mbuf *, int off, struct pipex_session *);
struct pipex_session  *pipex_l2tp_userland_lookup_session_ipv4 (struct mbuf *, struct in_addr);
struct pipex_session  *pipex_l2tp_userland_lookup_session_ipv6 (struct mbuf *, struct in6_addr);
struct mbuf           *pipex_l2tp_userland_output (struct mbuf *, struct pipex_session *);
int                   pipex_ioctl (struct pipex_iface_context *, int, caddr_t);
__END_DECLS

#endif /* _KERNEL */
#endif
