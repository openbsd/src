/*	$OpenBSD: if_switch.h,v 1.12 2019/11/06 03:51:26 dlg Exp $	*/

/*
 * Copyright (c) 2016 Kazuya GODA <goda@openbsd.org>
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _NET_IF_SWITCH_H_
#define _NET_IF_SWITCH_H_

/* capabilities for switch(4) */
#define SWITCH_CAP_STP		0x0001
#define SWITCH_CAP_LEARNING	0x0002
#define SWITCH_CAP_OFP		0x0004

#ifdef _KERNEL

struct switch_field_tunnel {
	uint32_t	 tun_af;
	struct in_addr	 tun_ipv4_src;
	struct in_addr	 tun_ipv4_dst;
	struct in6_addr	 tun_ipv6_src;
	struct in6_addr	 tun_ipv6_dst;
	uint64_t	 tun_key;
};

struct switch_field_ether {
	uint8_t		 eth_src[ETHER_ADDR_LEN];
	uint8_t		 __pad_eth_src[2];
	uint8_t		 eth_dst[ETHER_ADDR_LEN];
	uint8_t		 __pad_eth_dst[2];
	uint16_t	 eth_type;
	uint8_t		 __pad_eth_type[2];
};

struct switch_field_vlan {
	uint16_t	 vlan_tpid;
	uint16_t	 vlan_vid;
	uint16_t	 vlan_pcp;
	uint8_t		 __pad_vlan_pcp[2];
};

struct switch_field_ipv4 {
	uint32_t	 ipv4_src;
	uint32_t	 ipv4_dst;
	uint8_t		 ipv4_proto;
	uint8_t		 ipv4_tos;
	uint8_t		 ipv4_ttl;
	uint8_t		 ipv4_frag;
};

struct switch_field_ipv6 {
	struct in6_addr	 ipv6_src;
	struct in6_addr	 ipv6_dst;
	uint32_t	 ipv6_flow_label;
	uint8_t		 ipv6_nxt;
	uint8_t		 ipv6_tclass;
	uint8_t		 ipv6_hlimit;
	uint8_t		 ipv6_frag;
};

struct switch_field_arp {
	uint16_t	 _arp_op;
	uint8_t		 __pad_arp_op[2];
	uint8_t		 arp_sha[ETHER_ADDR_LEN];
	uint8_t		 __pad_arp_sha[2];
	uint8_t		 arp_tha[ETHER_ADDR_LEN];
	uint8_t		 __pad_arp_tha[2];
	uint32_t	 arp_sip;
	uint32_t	 arp_tip;
};

struct switch_field_nd6 {
	struct in6_addr	 nd6_target;
	uint8_t		 nd6_lladdr[ETHER_ADDR_LEN];
	uint8_t		 __pad_nd6_lladdr[2];
};

struct switch_field_icmpv4 {
	uint8_t		 icmpv4_type;
	uint8_t		 icmpv4_code;
	uint8_t		 __pad[2];
};

struct switch_field_icmpv6 {
	uint8_t		 icmpv6_type;
	uint8_t		 icmpv6_code;
	uint8_t		 __pad[2];
};

struct switch_field_tcp {
	uint16_t	 tcp_src;
	uint16_t	 tcp_dst;
	uint8_t		 tcp_flags;
	uint8_t		 __pad[3];
};

struct switch_field_udp {
	uint16_t	 udp_src;
	uint16_t	 udp_dst;
};

struct switch_field_sctp {
	uint16_t	 sctp_src;
	uint16_t	 sctp_dst;
};

union switch_field {
	struct switch_field_tunnel	 swfcl_tunnel;
	struct switch_field_ether	 swfcl_ether;
	struct switch_field_vlan	 swfcl_vlan;
	struct switch_field_ipv4	 swfcl_ipv4;
	struct switch_field_ipv6	 swfcl_ipv6;
	struct switch_field_arp		 swfcl_arp;
	struct switch_field_nd6		 swfcl_nd6;
	struct switch_field_icmpv4	 swfcl_icmpv4;
	struct switch_field_icmpv6	 swfcl_icmpv6;
	struct switch_field_tcp		 swfcl_tcp;
	struct switch_field_udp		 swfcl_udp;
	struct switch_field_sctp	 swfcl_sctp;
};

struct switch_flow_classify {
	uint64_t			 swfcl_flow_hash;

	/*
	 * Pipeline field on OpenFlow switch specific
	 */
	uint64_t			 swfcl_metadata;
	uint64_t			 swfcl_cookie;
	uint8_t				 swfcl_table_id;

	/*
	 * Classify field without protocol headers
	 */
	uint32_t			 swfcl_in_port;

	/*
	 * Classify field from protocol headers
	 */
	struct switch_field_tunnel	*swfcl_tunnel;
	struct switch_field_ether	*swfcl_ether;
	struct switch_field_vlan	*swfcl_vlan;
	struct switch_field_ipv4	*swfcl_ipv4;
	struct switch_field_ipv6	*swfcl_ipv6;
	struct switch_field_arp		*swfcl_arp;
	struct switch_field_nd6		*swfcl_nd6;
	struct switch_field_icmpv4	*swfcl_icmpv4;
	struct switch_field_icmpv6	*swfcl_icmpv6;
	struct switch_field_tcp		*swfcl_tcp;
	struct switch_field_udp		*swfcl_udp;
	struct switch_field_sctp	*swfcl_sctp;
};

struct switch_softc;

struct switch_port {
	TAILQ_ENTRY(switch_port)	 swpo_list_next;
	TAILQ_ENTRY(switch_port)	 swpo_fwdp_next;
	uint32_t			 swpo_port_no;
	uint32_t			 swpo_ifindex;
	struct timespec			 swpo_appended;
	struct switch_softc		*swpo_switch;
	uint32_t			 swpo_flags;
	uint32_t			 swpo_protected;
	struct task			 swpo_dtask;
	void				(*swop_bk_start)(struct ifnet *);
};

TAILQ_HEAD(switch_fwdp_queue, switch_port);

struct switch_dev {
	struct mbuf		*swdev_lastm;
	struct mbuf		*swdev_inputm;
	struct mbuf_queue	 swdev_outq;
	struct selinfo		 swdev_rsel;
	struct selinfo		 swdev_wsel;
	int			 swdev_waiting;
	void			(*swdev_init)(struct switch_softc *);
	int			(*swdev_input)(struct switch_softc *,
				    struct mbuf *);
	int			(*swdev_output)(struct switch_softc *,
				    struct mbuf *);
};

struct switch_softc {
	struct ifnet			 sc_if;
	int				 sc_unit;
	uint32_t			 sc_capabilities;
	struct switch_dev		*sc_swdev;		/* char device */
	struct bstp_state		*sc_stp;		/* STP state */
	struct swofp_ofs		*sc_ofs;		/* OpenFlow */
	caddr_t				 sc_ofbpf;		/* DLT_OPENFLOW */
	TAILQ_HEAD(,switch_port)	 sc_swpo_list;		/* port */
	LIST_ENTRY(switch_softc)	 sc_switch_next;	/* switch link */
	void				(*switch_process_forward)(
	    struct switch_softc *, struct switch_flow_classify *,
	    struct mbuf *);
};

/* if_switch.c */
struct switch_softc
	*switch_lookup(int);
void	 switch_port_egress(struct switch_softc *, struct switch_fwdp_queue *,
	    struct mbuf *);
int	 switch_swfcl_dup(struct switch_flow_classify *,
	    struct switch_flow_classify *);
void	 switch_swfcl_free(struct switch_flow_classify *);
struct mbuf
	*switch_flow_classifier(struct mbuf *, uint32_t,
	    struct switch_flow_classify *);
int	 switch_mtap(caddr_t, struct mbuf *, int, uint64_t);
int	 ofp_split_mbuf(struct mbuf *, struct mbuf **);

/* switchctl.c */
void	 switch_dev_destroy(struct switch_softc *);

/* in switchofp.c */
void	 swofp_attach(void);
int	 swofp_create(struct switch_softc *);
int	 swofp_init(struct switch_softc *);
void	 swofp_destroy(struct switch_softc *);
int	 swofp_ioctl(struct ifnet *, unsigned long, caddr_t);
uint32_t
	 swofp_assign_portno(struct switch_softc *, uint32_t);

#endif /* _KERNEL */
#endif /* _NET_IF_SWITCH_H_ */
