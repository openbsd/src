/*	$OpenBSD: if_bridge.h,v 1.28 2005/10/09 19:44:22 reyk Exp $	*/

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#ifndef _NET_IF_BRIDGE_H_
#define _NET_IF_BRIDGE_H_

#include <net/pfvar.h>

/*
 * Bridge control request: add/delete member interfaces.
 */
struct ifbreq {
	char		ifbr_name[IFNAMSIZ];	/* bridge ifs name */
	char		ifbr_ifsname[IFNAMSIZ];	/* member ifs name */
	u_int32_t	ifbr_ifsflags;		/* member ifs flags */
	u_int8_t	ifbr_state;		/* member stp state */
	u_int8_t	ifbr_priority;		/* member stp priority */
	u_int8_t	ifbr_portno;		/* member port number */
	u_int32_t	ifbr_path_cost;		/* member stp path cost */
};
/* SIOCBRDGIFFLGS, SIOCBRDGIFFLGS */
#define	IFBIF_LEARNING		0x0001	/* ifs can learn */
#define	IFBIF_DISCOVER		0x0002	/* ifs sends packets w/unknown dest */
#define	IFBIF_BLOCKNONIP	0x0004	/* ifs blocks non-IP/ARP in/out */
#define	IFBIF_STP		0x0008	/* ifs participates in spanning tree */
#define	IFBIF_SPAN		0x0100	/* ifs is a span port (ro) */
#define	IFBIF_RO_MASK		0xff00	/* read only bits */
/* SIOCBRDGFLUSH */
#define	IFBF_FLUSHDYN	0x0	/* flush dynamic addresses only */
#define	IFBF_FLUSHALL	0x1	/* flush all addresses from cache */

/* port states */
#define	BSTP_IFSTATE_DISABLED	0
#define	BSTP_IFSTATE_LISTENING	1
#define	BSTP_IFSTATE_LEARNING	2
#define	BSTP_IFSTATE_FORWARDING	3
#define	BSTP_IFSTATE_BLOCKING	4

/*
 * Interface list structure
 */
struct ifbifconf {
	char		ifbic_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t	ifbic_len;		/* buffer size */
	union {
		caddr_t	ifbicu_buf;
		struct	ifbreq *ifbicu_req;
	} ifbic_ifbicu;
#define	ifbic_buf	ifbic_ifbicu.ifbicu_buf
#define	ifbic_req	ifbic_ifbicu.ifbicu_req
};

/*
 * Bridge address request
 */
struct ifbareq {
	char			ifba_name[IFNAMSIZ];	/* bridge name */
	char			ifba_ifsname[IFNAMSIZ];	/* destination ifs */
	u_int8_t		ifba_age;		/* address age */
	u_int8_t		ifba_flags;		/* address flags */
	struct ether_addr	ifba_dst;		/* destination addr */
};

#define	IFBAF_TYPEMASK		0x03		/* address type mask */
#define	IFBAF_DYNAMIC		0x00		/* dynamically learned */
#define	IFBAF_STATIC		0x01		/* static address */

struct ifbaconf {
	char			ifbac_name[IFNAMSIZ];	/* bridge ifs name */
	u_int32_t		ifbac_len;		/* buffer size */
	union {
		caddr_t	ifbacu_buf;			/* buffer */
		struct ifbareq *ifbacu_req;		/* request pointer */
	} ifbac_ifbacu;
#define	ifbac_buf	ifbac_ifbacu.ifbacu_buf
#define	ifbac_req	ifbac_ifbacu.ifbacu_req
};

struct ifbrparam {
	char			ifbrp_name[IFNAMSIZ];
	union {
		u_int32_t	ifbrpu_csize;		/* cache size */
		int		ifbrpu_ctime;		/* cache time (sec) */
		u_int16_t	ifbrpu_prio;		/* bridge priority */
		u_int8_t	ifbrpu_hellotime;	/* hello time (sec) */
		u_int8_t	ifbrpu_fwddelay;	/* fwd delay (sec) */
		u_int8_t	ifbrpu_maxage;		/* max age (sec) */
	} ifbrp_ifbrpu;
};
#define	ifbrp_csize	ifbrp_ifbrpu.ifbrpu_csize
#define	ifbrp_ctime	ifbrp_ifbrpu.ifbrpu_ctime
#define	ifbrp_prio	ifbrp_ifbrpu.ifbrpu_prio
#define	ifbrp_hellotime	ifbrp_ifbrpu.ifbrpu_hellotime
#define	ifbrp_fwddelay	ifbrp_ifbrpu.ifbrpu_fwddelay
#define	ifbrp_maxage	ifbrp_ifbrpu.ifbrpu_maxage

/*
 * Bridge mac rules
 */
struct ifbrlreq {
	char			ifbr_name[IFNAMSIZ];	/* bridge ifs name */
	char			ifbr_ifsname[IFNAMSIZ];	/* member ifs name */
	u_int8_t		ifbr_action;		/* disposition */
	u_int8_t		ifbr_flags;		/* flags */
	struct ether_addr	ifbr_src;		/* source mac */
	struct ether_addr	ifbr_dst;		/* destination mac */
	char			ifbr_tagname[PF_TAG_NAME_SIZE];	/* pf tagname */
};
#define	BRL_ACTION_BLOCK	0x01			/* block frame */
#define	BRL_ACTION_PASS		0x02			/* pass frame */
#define	BRL_FLAG_IN		0x08			/* input rule */
#define	BRL_FLAG_OUT		0x04			/* output rule */
#define	BRL_FLAG_SRCVALID	0x02			/* src valid */
#define	BRL_FLAG_DSTVALID	0x01			/* dst valid */

struct ifbrlconf {
	char		ifbrl_name[IFNAMSIZ];	/* bridge ifs name */
	char		ifbrl_ifsname[IFNAMSIZ];/* member ifs name */
	u_int32_t	ifbrl_len;		/* buffer size */
	union {
		caddr_t	ifbrlu_buf;
		struct	ifbrlreq *ifbrlu_req;
	} ifbrl_ifbrlu;
#define	ifbrl_buf	ifbrl_ifbrlu.ifbrlu_buf
#define	ifbrl_req	ifbrl_ifbrlu.ifbrlu_req
};

#ifdef _KERNEL
/*
 * Bridge filtering rules
 */
SIMPLEQ_HEAD(brl_head, brl_node);

struct brl_node {
	SIMPLEQ_ENTRY(brl_node)	brl_next;	/* next rule */
	struct ether_addr	brl_src;	/* source mac address */
	struct ether_addr	brl_dst;	/* destination mac address */
	u_int16_t		brl_tag;	/* pf tag ID */
	u_int8_t		brl_action;	/* what to do with match */
	u_int8_t		brl_flags;	/* comparision flags */
};

struct bridge_timer {
	u_int16_t active;
	u_int16_t value;
};

struct bstp_config_unit {
	u_int64_t	cu_rootid;
	u_int64_t	cu_bridge_id;
	u_int32_t	cu_root_path_cost;
	u_int16_t	cu_message_age;
	u_int16_t	cu_max_age;
	u_int16_t	cu_hello_time;
	u_int16_t	cu_forward_delay;
	u_int16_t	cu_port_id;
	u_int8_t	cu_message_type;
	u_int8_t	cu_topology_change_acknowledgment;
	u_int8_t	cu_topology_change;
};

struct bstp_tcn_unit {
	u_int8_t	tu_message_type;
};

/*
 * Bridge interface list
 */
struct bridge_iflist {
	LIST_ENTRY(bridge_iflist)	next;		/* next in list */
	u_int64_t			bif_designated_root;
	u_int64_t			bif_designated_bridge;
	u_int32_t			bif_path_cost;
	u_int32_t			bif_designated_cost;
	struct bridge_timer		bif_hold_timer;
	struct bridge_timer		bif_message_age_timer;
	struct bridge_timer		bif_forward_delay_timer;
	struct bstp_config_unit		bif_config_bpdu;
	u_int16_t			bif_port_id;
	u_int16_t			bif_designated_port;
	u_int8_t			bif_state;
	u_int8_t			bif_topology_change_acknowledge;
	u_int8_t			bif_config_pending;
	u_int8_t			bif_change_detection_enabled;
	u_int8_t			bif_priority;
	struct brl_head			bif_brlin;	/* input rules */
	struct brl_head			bif_brlout;	/* output rules */
	struct				ifnet *ifp;	/* member interface */
	u_int32_t			bif_flags;	/* member flags */
};

/*
 * Bridge route node
 */
struct bridge_rtnode {
	LIST_ENTRY(bridge_rtnode)	brt_next;	/* next in list */
	struct				ifnet *brt_if;	/* destination ifs */
	u_int8_t			brt_flags;	/* address flags */
	u_int8_t			brt_age;	/* age counter */
	struct				ether_addr brt_addr;	/* dst addr */
};

#ifndef BRIDGE_RTABLE_SIZE
#define BRIDGE_RTABLE_SIZE	1024
#endif
#define BRIDGE_RTABLE_MASK	(BRIDGE_RTABLE_SIZE - 1)

/*
 * Software state for each bridge
 */
struct bridge_softc {
	struct				ifnet sc_if;	/* the interface */
	LIST_ENTRY(bridge_softc)	sc_list;	/* all bridges */
	u_int64_t			sc_designated_root;
	u_int64_t			sc_bridge_id;
	struct bridge_iflist		*sc_root_port;
	u_int32_t			sc_root_path_cost;
	u_int16_t			sc_max_age;
	u_int16_t			sc_hello_time;
	u_int16_t			sc_forward_delay;
	u_int16_t			sc_bridge_max_age;
	u_int16_t			sc_bridge_hello_time;
	u_int16_t			sc_bridge_forward_delay;
	u_int16_t			sc_topology_change_time;
	u_int16_t			sc_hold_time;
	u_int16_t			sc_bridge_priority;
	u_int8_t			sc_topology_change_detected;
	u_int8_t			sc_topology_change;
	struct bridge_timer		sc_hello_timer;
	struct bridge_timer		sc_topology_change_timer;
	struct bridge_timer		sc_tcn_timer;
	u_int32_t			sc_brtmax;	/* max # addresses */
	u_int32_t			sc_brtcnt;	/* current # addrs */
	int				sc_brttimeout;	/* timeout ticks */
	u_int32_t			sc_hashkey;	/* hash key */
	struct timeout			sc_brtimeout;	/* timeout state */
	struct timeout			sc_bstptimeout;	/* stp timeout */
	LIST_HEAD(, bridge_iflist)	sc_iflist;	/* interface list */
	LIST_HEAD(, bridge_rtnode)	sc_rts[BRIDGE_RTABLE_SIZE];	/* hash table */
	LIST_HEAD(, bridge_iflist)	sc_spanlist;	/* span ports */
};

extern u_int8_t bstp_etheraddr[];

void	bridge_ifdetach(struct ifnet *);
struct mbuf *bridge_input(struct ifnet *, struct ether_header *,
    struct mbuf *);
int	bridge_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct rtentry *);
void	bridge_update(struct ifnet *, struct ether_addr *, int);
struct mbuf *bstp_input(struct bridge_softc *, struct ifnet *,
    struct ether_header *, struct mbuf *);
void	bstp_initialization(struct bridge_softc *);
int	bstp_ioctl(struct ifnet *, u_long, caddr_t);
void	bridge_rtdelete(struct bridge_softc *, struct ifnet *, int);
#endif /* _KERNEL */
#endif /* _NET_IF_BRIDGE_H_ */
