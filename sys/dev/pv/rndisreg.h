/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _DEV_PV_RNDISREG_H_
#define _DEV_PV_RNDISREG_H_

/*
 * NDIS protocol version numbers
 */
#define NDIS_VERSION_5_0			0x00050000
#define NDIS_VERSION_5_1			0x00050001
#define NDIS_VERSION_6_0			0x00060000
#define NDIS_VERSION_6_1			0x00060001
#define NDIS_VERSION_6_30			0x0006001e

#define RNDIS_STATUS_OFFLOAD_CURRENT_CONFIG	0x40020006

struct ndis_8021q_info {
	union {
		struct {
			uint32_t	user_pri : 3;  /* User Priority */
			uint32_t	cfi	 : 1;  /* Canonical Format ID */
			uint32_t	vlan_id  : 12;
			uint32_t	reserved : 16;
		};
		uint32_t		value;
	};
} ndis_8021q_info;

struct rndis_objhdr {
	uint8_t				type;
	uint8_t				revision;
	uint16_t			size;
};

struct rndis_offload_params {
	struct rndis_objhdr		header;
	uint8_t				ipv4_csum;
	uint8_t				tcp_ipv4_csum;
	uint8_t				udp_ipv4_csum;
	uint8_t				tcp_ipv6_csum;
	uint8_t				udp_ipv6_csum;
	uint8_t				lso_v1;
	uint8_t				ip_sec_v1;
	uint8_t				lso_v2_ipv4;
	uint8_t				lso_v2_ipv6;
	uint8_t				tcp_connection_ipv4;
	uint8_t				tcp_connection_ipv6;
	uint32_t			flags;
	uint8_t				ip_sec_v2;
	uint8_t				ip_sec_v2_ipv4;
	struct {
		uint8_t			rsc_ipv4;
		uint8_t			rsc_ipv6;
	};
	struct {
		uint8_t			encap_packet_task_offload;
		uint8_t			encap_types;
	};
};

struct rndis_tcp_ip_csum_info {
	union {
		struct {
			uint32_t	is_ipv4:1;
			uint32_t	is_ipv6:1;
			uint32_t	tcp_csum:1;
			uint32_t	udp_csum:1;
			uint32_t	ip_header_csum:1;
			uint32_t	reserved:11;
			uint32_t	tcp_header_offset:10;
		} xmit;
		struct {
			uint32_t	tcp_csum_failed:1;
			uint32_t	udp_csum_failed:1;
			uint32_t	ip_csum_failed:1;
			uint32_t	tcp_csum_succeeded:1;
			uint32_t	udp_csum_succeeded:1;
			uint32_t	ip_csum_succeeded:1;
			uint32_t	loopback:1;
			uint32_t	tcp_csum_value_invalid:1;
			uint32_t	ip_csum_value_invalid:1;
		} recv;
		uint32_t		value;
	};
};

struct rndis_tcp_tso_info {
	union {
		struct {
			uint32_t	unused:30;
			uint32_t	type:1;
			uint32_t	reserved2:1;
		} xmit;
		struct {
			uint32_t	mss:20;
			uint32_t	tcp_header_offset:10;
			uint32_t	type:1;
			uint32_t	reserved2:1;
		} lso_v1_xmit;
		struct	{
			uint32_t	tcp_payload:30;
			uint32_t	type:1;
			uint32_t	reserved2:1;
		} lso_v1_xmit_comp;
		struct	{
			uint32_t	mss:20;
			uint32_t	tcp_header_offset:10;
			uint32_t	type:1;
			uint32_t	ip_version:1;
		} lso_v2_xmit;
		struct	{
			uint32_t	reserved:30;
			uint32_t	type:1;
			uint32_t	reserved2:1;
		} lso_v2_xmit_comp;
		uint32_t		value;
	};
};

#define RNDIS_VLAN_PPI_SIZE	(sizeof(struct rndis_pktinfo) +	\
	sizeof(struct ndis_8021q_info))

#define RNDIS_CSUM_PPI_SIZE	(sizeof(struct rndis_pktinfo) +	\
	sizeof(struct rndis_tcp_ip_csum_info))

#define RNDIS_TSO_PPI_SIZE	(sizeof(struct rndis_pktinfo) +	\
	sizeof(struct rndis_tcp_tso_info))

#endif	/* _DEV_PV_RNDISREG_H_ */
