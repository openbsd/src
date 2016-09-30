/*	$OpenBSD: ofp10.h,v 1.2 2016/09/30 12:48:27 reyk Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _NET_OFP10_H_
#define _NET_OFP10_H_

#include <net/ofp.h>

/* OpenFlow message type */
#define OFP10_T_HELLO			0	/* Hello */
#define OFP10_T_ERROR			1	/* Error */
#define OFP10_T_ECHO_REQUEST		2	/* Echo Request */
#define OFP10_T_ECHO_REPLY		3	/* Echo Reply */
#define OFP10_T_EXPERIMENTER		4	/* Vendor/Experimenter */
#define OFP10_T_FEATURES_REQUEST	5	/* Features Request (switch) */
#define OFP10_T_FEATURES_REPLY		6	/* Features Reply (switch) */
#define OFP10_T_GET_CONFIG_REQUEST	7	/* Get Config Request (switch) */
#define OFP10_T_GET_CONFIG_REPLY	8	/* Get Config Reply (switch) */
#define OFP10_T_SET_CONFIG		9	/* Set Config (switch) */
#define OFP10_T_PACKET_IN		10	/* Packet In (async) */
#define OFP10_T_FLOW_REMOVED		11	/* Flow Removed (async) */
#define OFP10_T_PORT_STATUS		12	/* Port Status (async) */
#define OFP10_T_PACKET_OUT		13	/* Packet Out (controller) */
#define OFP10_T_FLOW_MOD		14	/* Flow Mod (controller) */
#define OFP10_T_PORT_MOD		16	/* Port Mod (controller) */
#define OFP10_T_STATS_REQUEST		17	/* Stats Request */
#define OFP10_T_STATS_REPLY		18	/* Stats Reply */
#define OFP10_T_BARRIER_REQUEST		19	/* Barrier Request */
#define OFP10_T_BARRIER_REPLY		20	/* Barrier Reply */
#define OFP10_T_QUEUE_GET_CONFIG_REQUEST 21	/* Queue Get Config Request */
#define OFP10_T_QUEUE_GET_CONFIG_REPLY	22	/* Queue Get Config Reply */
#define	OFP10_T_TYPE_MAX		23

/* Ports */
#define OFP10_PORT_MAX			0xff00	/* Maximum number of physical ports */
#define	OFP10_PORT_INPUT		0xfff8 	/* Send back to input port */
#define OFP10_PORT_FLOWTABLE		0xfff9	/* Perform actions in flow table */
#define OFP10_PORT_NORMAL		0xfffa	/* Let switch decide */
#define OFP10_PORT_FLOOD		0xfffb	/* All non-block ports except input */
#define OFP10_PORT_ALL			0xfffc	/* All ports except input */
#define OFP10_PORT_CONTROLLER		0xfffd	/* Send to controller */
#define OFP10_PORT_LOCAL		0xfffe	/* Local virtual OpenFlow port */
#define OFP10_PORT_ANY			0xffff	/* No port */

/* Switch port description */
struct ofp10_phy_port {
	uint16_t	swp_number;
	uint8_t		swp_macaddr[ETHER_ADDR_LEN];
	char		swp_name[OFP_IFNAMSIZ];
	uint32_t	swp_config;		/* Configuration flags */
	uint32_t	swp_state;		/* State flags */
	uint32_t	swp_cur;		/* Current features */
	uint32_t	swp_advertised;		/* Advertised by the port */
	uint32_t	swp_supported;		/* Supported by the port */
	uint32_t	swp_peer;		/* Advertised by peer */
};

/* Packet-In Message */
struct ofp10_packet_in {
	struct ofp_header	pin_oh;		/* OpenFlow header */
	uint32_t		pin_buffer_id;
	uint16_t		pin_total_len;
	uint16_t		pin_port;
	uint8_t			pin_reason;
	uint8_t			pin_pad;
	uint8_t			pin_data[0];
} __packed;

/* Actions */
#define OFP10_ACTION_OUTPUT		0	/* Output to switch port */
#define OFP10_ACTION_SET_VLAN_VID	1	/* Set the 802.1q VLAN id */
#define OFP10_ACTION_SET_VLAN_PCP	2	/* Set the 802.1q priority */
#define OFP10_ACTION_STRIP_VLAN		3	/* Strip the 802.1q header */
#define OFP10_ACTION_SET_DL_SRC		4	/* Ethernet src address */
#define OFP10_ACTION_SET_DL_DST		5	/* Ethernet dst address */
#define OFP10_ACTION_SET_NW_SRC		6	/* IP src address */
#define OFP10_ACTION_SET_NW_DST		7	/* IP dst address */
#define OFP10_ACTION_SET_NW_TOS		8	/* IP TOS */
#define OFP10_ACTION_SET_TP_SRC		9	/* TCP/UDP src port */
#define OFP10_ACTION_SET_TP_DST		10	/* TCP/UDP dst port */
#define OFP10_ACTION_ENQUEUE		11	/* Output to queue */
#define OFP10_ACTION_EXPERIMENTER	0xffff	/* Vendor-specific action */

/* Output Action */
struct ofp10_action_output {
	uint16_t	ao_type;
	uint16_t	ao_len;
	uint16_t	ao_port;
	uint16_t	ao_max_len;
} __packed;

/* Packet-Out Message */
struct ofp10_packet_out {
	struct ofp_header	pout_oh;	/* OpenFlow header */
	uint32_t		pout_buffer_id;
	uint16_t		pout_port;
	uint16_t		pout_actions_len;
	struct ofp_action_header pout_actions[0];
	/* Followed by optional packet data if buffer_id == 0xffffffff */
} __packed;

/* Flow matching wildcards */
#define OFP10_WILDCARD_IN_PORT	0x00000001	/* Switch input port */
#define OFP10_WILDCARD_DL_VLAN	0x00000002	/* VLAN id */
#define OFP10_WILDCARD_DL_SRC	0x00000004	/* Ethernet src address */
#define OFP10_WILDCARD_DL_DST	0x00000008	/* Ethernet dst address */
#define OFP10_WILDCARD_DL_TYPE	0x00000010	/* Ethernet frame type */
#define OFP10_WILDCARD_NW_PROTO	0x00000020	/* IPv4 protocol */
#define OFP10_WILDCARD_TP_SRC	0x00000040	/* TCP/UDP source port */
#define OFP10_WILDCARD_TP_DST	0x00000080	/* TCP/UDP destination port */
#define OFP10_WILDCARD_NW_SRC	0x00003f00	/* IPv4 source address */
#define OFP10_WILDCARD_NW_SRC_S	8
#define OFP10_WILDCARD_NW_DST	0x000fc000	/* IPv4 destination address */
#define OFP10_WILDCARD_NW_DST_S	14
#define OFP10_WILDCARD_DL_VLANPCP 0x00100000	/* VLAN prio */
#define OFP10_WILDCARD_NW_TOS	0x00200000	/* IPv4 ToS/DSCP */
#define OFP10_WILDCARD_MASK	0x003fffff	/* All wildcard flags */

/* Flow matching */
struct ofp10_match {
	uint32_t	m_wildcards;			/* Wildcard options */
	uint16_t	m_in_port;			/* Switch port */
	uint8_t		m_dl_src[ETHER_ADDR_LEN];	/* Ether src addr */
	uint8_t		m_dl_dst[ETHER_ADDR_LEN];	/* Ether dst addr */
	uint16_t	m_dl_vlan;			/* Input VLAN id */
	uint8_t		m_dl_vlan_pcp;			/* Input VLAN prio */
	uint8_t		m_pad1[1];
	uint16_t	m_dl_type;			/* Ether type */
	uint8_t		m_nw_tos;			/* IPv4 ToS/DSCP */ 
	uint8_t		m_nw_proto;			/* IPv4 Proto */
	uint8_t		m_pad2[2];
	uint32_t	m_nw_src;			/* IPv4 source */
	uint32_t	m_nw_dst;			/* IPv4 destination */
	uint16_t	m_tp_src;			/* TCP/UDP src port */
	uint16_t	m_tp_dst;			/* TCP/UDP dst port */
} __packed;

/* Flow modification message */
struct ofp10_flow_mod {
	struct ofp_header	fm_oh;		/* OpenFlow header */
	struct ofp10_match	fm_match;
	uint64_t		fm_cookie;
	uint16_t		fm_command;
	uint16_t		fm_idle_timeout;
	uint16_t		fm_hard_timeout;
	uint16_t		fm_priority;
	uint32_t		fm_buffer_id;
	uint16_t		fm_port;
	uint16_t		fm_flags;
	struct ofp_action_header fm_actions[0];
} __packed;

/* Error types */
#define OFP10_ERRTYPE_HELLO_FAILED	0	/* Hello protocol failed */
#define OFP10_ERRTYPE_BAD_REQUEST	1	/* Request was not understood */
#define OFP10_ERRTYPE_BAD_ACTION	2	/* Error in action */
#define OFP10_ERRTYPE_FLOW_MOD_FAILED	3	/* Problem modifying flow */
#define OFP10_ERRTYPE_PORT_MOD_FAILED	4	/* Port mod request failed */
#define OFP10_ERRTYPE_QUEUE_OP_FAILED	5	/* Queue operation failed */

/* FLOW MOD error codes */
#define OFP10_ERRFLOWMOD_ALL_TABLES_FULL 0	/* Not added, full tables */
#define OFP10_ERRFLOWMOD_OVERLAP	1       /* Overlapping flow */
#define OFP10_ERRFLOWMOD_EPERM		2	/* Permissions error */
#define OFP10_ERRFLOWMOD_BAD_TIMEOUT	3	/* non-zero idle/hardtimeout */
#define OFP10_ERRFLOWMOD_BAD_COMMAND	4	/* Unknown command */
#define OFP10_ERRFLOWMOD_UNSUPPORTED	5	/* Unsupported action list */

#endif /* _NET_OPF_H_ */
