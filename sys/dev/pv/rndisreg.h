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

/*
 * Status codes
 */

#define STATUS_SUCCESS				0x00000000
#define STATUS_UNSUCCESSFUL			0xC0000001
#define STATUS_PENDING				0x00000103
#define STATUS_INSUFFICIENT_RESOURCES		0xC000009A
#define STATUS_BUFFER_OVERFLOW			0x80000005
#define STATUS_NOT_SUPPORTED			0xC00000BB

#define RNDIS_STATUS_SUCCESS			0x00000000
#define RNDIS_STATUS_PENDING			0x00000103
#define RNDIS_STATUS_NOT_RECOGNIZED		0x00010001
#define RNDIS_STATUS_NOT_COPIED			0x00010002
#define RNDIS_STATUS_NOT_ACCEPTED		0x00010003
#define RNDIS_STATUS_CALL_ACTIVE		0x00010007

#define RNDIS_STATUS_ONLINE			0x40010003
#define RNDIS_STATUS_RESET_START		0x40010004
#define RNDIS_STATUS_RESET_END			0x40010005
#define RNDIS_STATUS_RING_STATUS		0x40010006
#define RNDIS_STATUS_CLOSED			0x40010007
#define RNDIS_STATUS_WAN_LINE_UP		0x40010008
#define RNDIS_STATUS_WAN_LINE_DOWN		0x40010009
#define RNDIS_STATUS_WAN_FRAGMENT		0x4001000A
#define RNDIS_STATUS_MEDIA_CONNECT		0x4001000B
#define RNDIS_STATUS_MEDIA_DISCONNECT		0x4001000C
#define RNDIS_STATUS_HARDWARE_LINE_UP		0x4001000D
#define RNDIS_STATUS_HARDWARE_LINE_DOWN		0x4001000E
#define RNDIS_STATUS_INTERFACE_UP		0x4001000F
#define RNDIS_STATUS_INTERFACE_DOWN		0x40010010
#define RNDIS_STATUS_MEDIA_BUSY			0x40010011
#define RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION	0x40010012
#define RNDIS_STATUS_WW_INDICATION	  	0x40010012
#define RNDIS_STATUS_LINK_SPEED_CHANGE		0x40010013

#define RNDIS_STATUS_OFFLOAD_CURRENT_CONFIG	0x40020006

#define RNDIS_STATUS_NOT_RESETTABLE		0x80010001
#define RNDIS_STATUS_SOFT_ERRORS		0x80010003
#define RNDIS_STATUS_HARD_ERRORS		0x80010004
#define RNDIS_STATUS_BUFFER_OVERFLOW		0x80000005

#define RNDIS_STATUS_FAILURE			0xC0000001
#define RNDIS_STATUS_RESOURCES			0xC000009A
#define RNDIS_STATUS_CLOSING			0xC0010002
#define RNDIS_STATUS_BAD_VERSION		0xC0010004
#define RNDIS_STATUS_BAD_CHARACTERISTICS	0xC0010005
#define RNDIS_STATUS_ADAPTER_NOT_FOUND		0xC0010006
#define RNDIS_STATUS_OPEN_FAILED		0xC0010007
#define RNDIS_STATUS_DEVICE_FAILED		0xC0010008
#define RNDIS_STATUS_MULTICAST_FULL		0xC0010009
#define RNDIS_STATUS_MULTICAST_EXISTS		0xC001000A
#define RNDIS_STATUS_MULTICAST_NOT_FOUND	0xC001000B
#define RNDIS_STATUS_REQUEST_ABORTED		0xC001000C
#define RNDIS_STATUS_RESET_IN_PROGRESS		0xC001000D
#define RNDIS_STATUS_CLOSING_INDICATING		0xC001000E
#define RNDIS_STATUS_NOT_SUPPORTED		0xC00000BB
#define RNDIS_STATUS_INVALID_PACKET		0xC001000F
#define RNDIS_STATUS_OPEN_LIST_FULL		0xC0010010
#define RNDIS_STATUS_ADAPTER_NOT_READY		0xC0010011
#define RNDIS_STATUS_ADAPTER_NOT_OPEN		0xC0010012
#define RNDIS_STATUS_NOT_INDICATING		0xC0010013
#define RNDIS_STATUS_INVALID_LENGTH		0xC0010014
#define RNDIS_STATUS_INVALID_DATA		0xC0010015
#define RNDIS_STATUS_BUFFER_TOO_SHORT		0xC0010016
#define RNDIS_STATUS_INVALID_OID		0xC0010017
#define RNDIS_STATUS_ADAPTER_REMOVED		0xC0010018
#define RNDIS_STATUS_UNSUPPORTED_MEDIA		0xC0010019
#define RNDIS_STATUS_GROUP_ADDRESS_IN_USE	0xC001001A
#define RNDIS_STATUS_FILE_NOT_FOUND		0xC001001B
#define RNDIS_STATUS_ERROR_READING_FILE		0xC001001C
#define RNDIS_STATUS_ALREADY_MAPPED		0xC001001D
#define RNDIS_STATUS_RESOURCE_CONFLICT		0xC001001E
#define RNDIS_STATUS_NO_CABLE			0xC001001F

#define RNDIS_STATUS_INVALID_SAP		0xC0010020
#define RNDIS_STATUS_SAP_IN_USE			0xC0010021
#define RNDIS_STATUS_INVALID_ADDRESS		0xC0010022
#define RNDIS_STATUS_VC_NOT_ACTIVATED		0xC0010023
#define RNDIS_STATUS_DEST_OUT_OF_ORDER		0xC0010024
#define RNDIS_STATUS_VC_NOT_AVAILABLE		0xC0010025
#define RNDIS_STATUS_CELLRATE_NOT_AVAILABLE	0xC0010026
#define RNDIS_STATUS_INCOMPATABLE_QOS		0xC0010027
#define RNDIS_STATUS_AAL_PARAMS_UNSUPPORTED	0xC0010028
#define RNDIS_STATUS_NO_ROUTE_TO_DESTINATION	0xC0010029

#define RNDIS_STATUS_TOKEN_RING_OPEN_ERROR	0xC0011000


/*
 * Object Identifiers used by NdisRequest Query/Set Information
 */

/*
 * General Objects
 */

#define RNDIS_OID_GEN_SUPPORTED_LIST		0x00010101
#define RNDIS_OID_GEN_HARDWARE_STATUS		0x00010102
#define RNDIS_OID_GEN_MEDIA_SUPPORTED		0x00010103
#define RNDIS_OID_GEN_MEDIA_IN_USE		0x00010104
#define RNDIS_OID_GEN_MAXIMUM_LOOKAHEAD		0x00010105
#define RNDIS_OID_GEN_MAXIMUM_FRAME_SIZE	0x00010106
#define RNDIS_OID_GEN_LINK_SPEED		0x00010107
#define RNDIS_OID_GEN_TRANSMIT_BUFFER_SPACE	0x00010108
#define RNDIS_OID_GEN_RECEIVE_BUFFER_SPACE	0x00010109
#define RNDIS_OID_GEN_TRANSMIT_BLOCK_SIZE	0x0001010A
#define RNDIS_OID_GEN_RECEIVE_BLOCK_SIZE	0x0001010B
#define RNDIS_OID_GEN_VENDOR_ID			0x0001010C
#define RNDIS_OID_GEN_VENDOR_DESCRIPTION	0x0001010D
#define RNDIS_OID_GEN_CURRENT_PACKET_FILTER	0x0001010E
#define RNDIS_OID_GEN_CURRENT_LOOKAHEAD		0x0001010F
#define RNDIS_OID_GEN_DRIVER_VERSION		0x00010110
#define RNDIS_OID_GEN_MAXIMUM_TOTAL_SIZE	0x00010111
#define RNDIS_OID_GEN_PROTOCOL_OPTIONS		0x00010112
#define RNDIS_OID_GEN_MAC_OPTIONS		0x00010113
#define RNDIS_OID_GEN_MEDIA_CONNECT_STATUS	0x00010114
#define RNDIS_OID_GEN_MAXIMUM_SEND_PACKETS	0x00010115
#define RNDIS_OID_GEN_VENDOR_DRIVER_VERSION	0x00010116
#define RNDIS_OID_GEN_NETWORK_LAYER_ADDRESSES	0x00010118
#define RNDIS_OID_GEN_TRANSPORT_HEADER_OFFSET	0x00010119
#define RNDIS_OID_GEN_MACHINE_NAME		0x0001021A
#define RNDIS_OID_GEN_RNDIS_CONFIG_PARAMETER	0x0001021B

#define RNDIS_OID_GEN_XMIT_OK			0x00020101
#define RNDIS_OID_GEN_RCV_OK			0x00020102
#define RNDIS_OID_GEN_XMIT_ERROR		0x00020103
#define RNDIS_OID_GEN_RCV_ERROR			0x00020104
#define RNDIS_OID_GEN_RCV_NO_BUFFER		0x00020105

#define RNDIS_OID_GEN_DIRECTED_BYTES_XMIT	0x00020201
#define RNDIS_OID_GEN_DIRECTED_FRAMES_XMIT	0x00020202
#define RNDIS_OID_GEN_MULTICAST_BYTES_XMIT	0x00020203
#define RNDIS_OID_GEN_MULTICAST_FRAMES_XMIT	0x00020204
#define RNDIS_OID_GEN_BROADCAST_BYTES_XMIT	0x00020205
#define RNDIS_OID_GEN_BROADCAST_FRAMES_XMIT	0x00020206
#define RNDIS_OID_GEN_DIRECTED_BYTES_RCV	0x00020207
#define RNDIS_OID_GEN_DIRECTED_FRAMES_RCV	0x00020208
#define RNDIS_OID_GEN_MULTICAST_BYTES_RCV	0x00020209
#define RNDIS_OID_GEN_MULTICAST_FRAMES_RCV	0x0002020A
#define RNDIS_OID_GEN_BROADCAST_BYTES_RCV	0x0002020B
#define RNDIS_OID_GEN_BROADCAST_FRAMES_RCV	0x0002020C

#define RNDIS_OID_GEN_RCV_CRC_ERROR		0x0002020D
#define RNDIS_OID_GEN_TRANSMIT_QUEUE_LENGTH	0x0002020E

#define RNDIS_OID_GEN_GET_TIME_CAPS		0x0002020F
#define RNDIS_OID_GEN_GET_NETCARD_TIME		0x00020210

/*
 * These are connection-oriented general OIDs.
 * These replace the above OIDs for connection-oriented media.
 */
#define RNDIS_OID_GEN_CO_SUPPORTED_LIST		0x00010101
#define RNDIS_OID_GEN_CO_HARDWARE_STATUS	0x00010102
#define RNDIS_OID_GEN_CO_MEDIA_SUPPORTED	0x00010103
#define RNDIS_OID_GEN_CO_MEDIA_IN_USE		0x00010104
#define RNDIS_OID_GEN_CO_LINK_SPEED		0x00010105
#define RNDIS_OID_GEN_CO_VENDOR_ID		0x00010106
#define RNDIS_OID_GEN_CO_VENDOR_DESCRIPTION	0x00010107
#define RNDIS_OID_GEN_CO_DRIVER_VERSION		0x00010108
#define RNDIS_OID_GEN_CO_PROTOCOL_OPTIONS	0x00010109
#define RNDIS_OID_GEN_CO_MAC_OPTIONS		0x0001010A
#define RNDIS_OID_GEN_CO_MEDIA_CONNECT_STATUS	0x0001010B
#define RNDIS_OID_GEN_CO_VENDOR_DRIVER_VERSION	0x0001010C
#define RNDIS_OID_GEN_CO_MINIMUM_LINK_SPEED	0x0001010D

#define RNDIS_OID_GEN_CO_GET_TIME_CAPS		0x00010201
#define RNDIS_OID_GEN_CO_GET_NETCARD_TIME	0x00010202

/*
 * These are connection-oriented statistics OIDs.
 */
#define RNDIS_OID_GEN_CO_XMIT_PDUS_OK		0x00020101
#define RNDIS_OID_GEN_CO_RCV_PDUS_OK		0x00020102
#define RNDIS_OID_GEN_CO_XMIT_PDUS_ERROR	0x00020103
#define RNDIS_OID_GEN_CO_RCV_PDUS_ERROR		0x00020104
#define RNDIS_OID_GEN_CO_RCV_PDUS_NO_BUFFER	0x00020105


#define RNDIS_OID_GEN_CO_RCV_CRC_ERROR		0x00020201
#define RNDIS_OID_GEN_CO_TRANSMIT_QUEUE_LENGTH	0x00020202
#define RNDIS_OID_GEN_CO_BYTES_XMIT		0x00020203
#define RNDIS_OID_GEN_CO_BYTES_RCV		0x00020204
#define RNDIS_OID_GEN_CO_BYTES_XMIT_OUTSTANDING	0x00020205
#define RNDIS_OID_GEN_CO_NETCARD_LOAD		0x00020206

/*
 * These are objects for Connection-oriented media call-managers.
 */
#define RNDIS_OID_CO_ADD_PVC			0xFF000001
#define RNDIS_OID_CO_DELETE_PVC			0xFF000002
#define RNDIS_OID_CO_GET_CALL_INFORMATION	0xFF000003
#define RNDIS_OID_CO_ADD_ADDRESS		0xFF000004
#define RNDIS_OID_CO_DELETE_ADDRESS		0xFF000005
#define RNDIS_OID_CO_GET_ADDRESSES		0xFF000006
#define RNDIS_OID_CO_ADDRESS_CHANGE		0xFF000007
#define RNDIS_OID_CO_SIGNALING_ENABLED		0xFF000008
#define RNDIS_OID_CO_SIGNALING_DISABLED		0xFF000009


/*
 * 802.3 Objects (Ethernet)
 */

#define RNDIS_OID_802_3_PERMANENT_ADDRESS	0x01010101
#define RNDIS_OID_802_3_CURRENT_ADDRESS		0x01010102
#define RNDIS_OID_802_3_MULTICAST_LIST		0x01010103
#define RNDIS_OID_802_3_MAXIMUM_LIST_SIZE	0x01010104
#define RNDIS_OID_802_3_MAC_OPTIONS		0x01010105

/*
 *
 */
#define NDIS_802_3_MAC_OPTION_PRIORITY		0x00000001

#define RNDIS_OID_802_3_RCV_ERROR_ALIGNMENT	0x01020101
#define RNDIS_OID_802_3_XMIT_ONE_COLLISION	0x01020102
#define RNDIS_OID_802_3_XMIT_MORE_COLLISIONS	0x01020103

#define RNDIS_OID_802_3_XMIT_DEFERRED		0x01020201
#define RNDIS_OID_802_3_XMIT_MAX_COLLISIONS	0x01020202
#define RNDIS_OID_802_3_RCV_OVERRUN		0x01020203
#define RNDIS_OID_802_3_XMIT_UNDERRUN		0x01020204
#define RNDIS_OID_802_3_XMIT_HEARTBEAT_FAILURE	0x01020205
#define RNDIS_OID_802_3_XMIT_TIMES_CRS_LOST	0x01020206
#define RNDIS_OID_802_3_XMIT_LATE_COLLISIONS	0x01020207


/*
 * RNDIS MP custom OID for test
 */
#define OID_RNDISMP_GET_RECEIVE_BUFFERS		0xFFA0C90D


/*
 * Remote NDIS message types
 */
#define RNDIS_PACKET_MSG			0x00000001
#define RNDIS_INITIALIZE_MSG			0x00000002
#define RNDIS_HALT_MSG				0x00000003
#define RNDIS_QUERY_MSG				0x00000004
#define RNDIS_SET_MSG				0x00000005
#define RNDIS_RESET_MSG				0x00000006
#define RNDIS_INDICATE_STATUS_MSG		0x00000007
#define RNDIS_KEEPALIVE_MSG			0x00000008

#define RCONDIS_MP_CREATE_VC_MSG		0x00008001
#define RCONDIS_MP_DELETE_VC_MSG		0x00008002
#define RCONDIS_MP_ACTIVATE_VC_MSG		0x00008005
#define RCONDIS_MP_DEACTIVATE_VC_MSG		0x00008006
#define RCONDIS_INDICATE_STATUS_MSG		0x00008007

/*
 * Remote NDIS message completion types
 */
#define RNDIS_INITIALIZE_CMPLT			0x80000002
#define RNDIS_QUERY_CMPLT			0x80000004
#define RNDIS_SET_CMPLT				0x80000005
#define RNDIS_RESET_CMPLT			0x80000006
#define RNDIS_KEEPALIVE_CMPLT			0x80000008

#define RCONDIS_MP_CREATE_VC_CMPLT		0x80008001
#define RCONDIS_MP_DELETE_VC_CMPLT		0x80008002
#define RCONDIS_MP_ACTIVATE_VC_CMPLT		0x80008005
#define RCONDIS_MP_DEACTIVATE_VC_CMPLT		0x80008006

/*
 * Reserved message type for private communication between
 * lower-layer host driver and remote device, if necessary.
 */
#define RNDIS_BUS_MSG				0xff000001

/*
 * Defines for DeviceFlags in rndis_initialize_comp
 */
#define RNDIS_DF_CONNECTIONLESS			0x00000001
#define RNDIS_DF_CONNECTION_ORIENTED		0x00000002
#define RNDIS_DF_RAW_DATA			0x00000004

/*
 * Remote NDIS medium types.
 */
#define RNDIS_MEDIUM_802_3			0x00000000
#define RNDIS_MEDIUM_802_5			0x00000001
#define RNDIS_MEDIUM_FDDI			0x00000002
#define RNDIS_MEDIUM_WAN			0x00000003
#define RNDIS_MEDIUM_LOCAL_TALK			0x00000004
#define RNDIS_MEDIUM_ARCNET_RAW			0x00000006
#define RNDIS_MEDIUM_ARCNET_878_2		0x00000007
#define RNDIS_MEDIUM_ATM			0x00000008
#define RNDIS_MEDIUM_WIRELESS_WAN		0x00000009
#define RNDIS_MEDIUM_IRDA			0x0000000a
#define RNDIS_MEDIUM_CO_WAN			0x0000000b
/* Not a real medium, defined as an upper bound */
#define RNDIS_MEDIUM_MAX			0x0000000d

/*
 * Remote NDIS medium connection states.
 */
#define RNDIS_MEDIA_STATE_CONNECTED		0x00000000
#define RNDIS_MEDIA_STATE_DISCONNECTED		0x00000001

/*
 * Remote NDIS version numbers
 */
#define RNDIS_MAJOR_VERSION			0x00000001
#define RNDIS_MINOR_VERSION			0x00000000

/*
 * Remote NDIS offload parameters
 */
#define RNDIS_OBJECT_TYPE_DEFAULT		0x80

#define RNDIS_OFFLOAD_PARAMS_REVISION_3		3
#define RNDIS_OFFLOAD_PARAMS_NO_CHANGE		0
#define RNDIS_OFFLOAD_PARAMS_LSOV2_DISABLED	1
#define RNDIS_OFFLOAD_PARAMS_LSOV2_ENABLED	2
#define RNDIS_OFFLOAD_PARAMS_LSOV1_ENABLED	2
#define RNDIS_OFFLOAD_PARAMS_RSC_DISABLED	1
#define RNDIS_OFFLOAD_PARAMS_RSC_ENABLED	2
#define RNDIS_OFFLOAD_PARAMS_TX_RX_DISABLED	1
#define RNDIS_OFFLOAD_PARAMS_TX_ENABLED_RX_DISABLED 2
#define RNDIS_OFFLOAD_PARAMS_RX_ENABLED_TX_DISABLED 3
#define RNDIS_OFFLOAD_PARAMS_TX_RX_ENABLED	4

#define RNDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE	1
#define RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV4	0
#define RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV6	1

#define RNDIS_OID_TCP_OFFLOAD_CURRENT_CONFIG	0xFC01020B /* query only */
#define RNDIS_OID_TCP_OFFLOAD_PARAMS		0xFC01020C /* set only */
#define RNDIS_OID_TCP_OFFLOAD_HARDWARE_CAPS	0xFC01020D/* query only */
#define RNDIS_OID_TCPCON_OFFLOAD_CURRENT_CONFIG	0xFC01020E /* query only */
#define RNDIS_OID_TCPCON_OFFLOAD_HARDWARE_CAPS	0xFC01020F /* query */
#define RNDIS_OID_OFFLOAD_ENCAPSULATION		0x0101010A /* set/query */

/*
 * NdisInitialize message
 */
struct rndis_init_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	uint32_t			major_version;
	uint32_t			minor_version;
	uint32_t			max_xfer_size;
};

/*
 * Response to NdisInitialize
 */
struct rndis_init_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
	uint32_t			major_version;
	uint32_t			minor_version;
	uint32_t			device_flags;
	/* RNDIS medium */
	uint32_t			medium;
	uint32_t			max_pkts_per_msg;
	uint32_t			max_xfer_size;
	uint32_t			pkt_align_factor;
	uint32_t			af_list_offset;
	uint32_t			af_list_size;
};

/*
 * Call manager devices only: Information about an address family
 * supported by the device is appended to the response to NdisInitialize.
 */
struct rndis_co_address_family {
	/* RNDIS AF */
	uint32_t			address_family;
	uint32_t			major_version;
	uint32_t			minor_version;
};

/*
 * NdisHalt message
 */
struct rndis_halt_req {
	/* RNDIS request ID */
	uint32_t			request_id;
};

/*
 * NdisQueryRequest message
 */
struct rndis_query_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS OID */
	uint32_t			oid;
	uint32_t			info_buffer_length;
	uint32_t			info_buffer_offset;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
};

/*
 * Response to NdisQueryRequest
 */
struct rndis_query_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
	uint32_t			info_buffer_length;
	uint32_t			info_buffer_offset;
};

/*
 * NdisSetRequest message
 */
struct rndis_set_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS OID */
	uint32_t			oid;
	uint32_t			info_buffer_length;
	uint32_t			info_buffer_offset;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
};

/*
 * Response to NdisSetRequest
 */
struct rndis_set_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * NdisReset message
 */
struct rndis_reset_req {
	uint32_t			reserved;
};

/*
 * Response to NdisReset
 */
struct rndis_reset_comp {
	/* RNDIS status */
	uint32_t			status;
	uint32_t			addressing_reset;
};

/*
 * NdisMIndicateStatus message
 */
struct rndis_indicate_status {
	/* RNDIS status */
	uint32_t			status;
	uint32_t			status_buf_length;
	uint32_t			status_buf_offset;
};

/*
 * Diagnostic information passed as the status buffer in
 * rndis_indicate_status messages signifying error conditions.
 */
struct rndis_diagnostic_info {
	/* RNDIS status */
	uint32_t			diag_status;
	uint32_t			error_offset;
};

/*
 * NdisKeepAlive message
 */
struct rndis_keepalive_req {
	/* RNDIS request ID */
	uint32_t			request_id;
};

/*
 * Response to NdisKeepAlive
 */
struct rndis_keepalive_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * Data message. All offset fields contain byte offsets from the
 * beginning of the rndis_pkt structure. All length fields are in
 * bytes.  VcHandle is set to 0 for connectionless data, otherwise
 * it contains the VC handle.
 */
struct rndis_pkt {
	uint32_t			data_offset;
	uint32_t			data_length;
	uint32_t			oob_data_offset;
	uint32_t			oob_data_length;
	uint32_t			num_oob_data_elements;
	uint32_t			pkt_info_offset;
	uint32_t			pkt_info_length;
	/* RNDIS handle */
	uint32_t			vc_handle;
	uint32_t			reserved;
};

struct rndis_pkt_ex {
	uint32_t			data_offset;
	uint32_t			data_length;
	uint32_t			oob_data_offset;
	uint32_t			oob_data_length;
	uint32_t			num_oob_data_elements;
	uint32_t			pkt_info_offset;
	uint32_t			pkt_info_length;
	/* RNDIS handle */
	uint32_t			vc_handle;
	uint32_t			reserved;
	uint64_t			data_buf_id;
	uint32_t			data_buf_offset;
	uint64_t			next_header_buf_id;
	uint32_t			next_header_byte_offset;
	uint32_t			next_header_byte_count;
};

/*
 * Optional Out of Band data associated with a Data message.
 */
struct rndis_oobd {
	uint32_t			size;
	/* RNDIS class ID */
	uint32_t			type;
	uint32_t			class_info_offset;
};

/*
 * Packet extension field contents associated with a Data message.
 */
struct rndis_pkt_info {
	uint32_t			size;
	uint32_t			type;
	uint32_t			pkt_info_offset;
};

enum ndis_pkt_info_type {
	tcpip_chksum_info,
	ipsec_info,
	tcp_large_send_info,
	classification_handle_info,
	ndis_reserved,
	sgl_info,
	ieee_8021q_info,
	original_pkt_info,
	pkt_cancel_id,
	original_netbuf_list,
	cached_netbuf_list,
	short_pkt_padding_info,
	max_perpkt_info
};

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

#define RNDIS_VLAN_PPI_SIZE	(sizeof(struct rndis_pkt_info) +	\
	sizeof(struct ndis_8021q_info))

#define RNDIS_CSUM_PPI_SIZE	(sizeof(struct rndis_pkt_info) +	\
	sizeof(struct rndis_tcp_ip_csum_info))

#define RNDIS_TSO_PPI_SIZE	(sizeof(struct rndis_pkt_info) +	\
	sizeof(struct rndis_tcp_tso_info))

/*
 * Format of Information buffer passed in a SetRequest for the OID
 * OID_GEN_RNDIS_CONFIG_PARAMETER.
 */
struct rndis_config_param_info {
	uint32_t			name_offset;
	uint32_t			name_length;
	uint32_t			param_type;
	uint32_t			value_offset;
	uint32_t			value_length;
};

/*
 * Values for ParameterType in rndis_config_param_info
 */
#define RNDIS_CONFIG_PARAM_TYPE_INTEGER	0
#define RNDIS_CONFIG_PARAM_TYPE_STRING	2


/*
 * CONDIS Miniport messages for connection oriented devices
 * that do not implement a call manager.
 */

/*
 * CoNdisMiniportCreateVc message
 */
struct rcondis_mp_create_vc {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS handle */
	uint32_t			ndis_vc_handle;
};

/*
 * Response to CoNdisMiniportCreateVc
 */
struct rcondis_mp_create_vc_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * CoNdisMiniportDeleteVc message
 */
struct rcondis_mp_delete_vc {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
};

/*
 * Response to CoNdisMiniportDeleteVc
 */
struct rcondis_mp_delete_vc_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * CoNdisMiniportQueryRequest message
 */
struct rcondis_mp_query_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS request type */
	uint32_t			request_type;
	/* RNDIS OID */
	uint32_t			oid;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
	uint32_t			info_buf_length;
	uint32_t			info_buf_offset;
};

/*
 * CoNdisMiniportSetRequest message
 */
struct rcondis_mp_set_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS request type */
	uint32_t			request_type;
	/* RNDIS OID */
	uint32_t			oid;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
	uint32_t			info_buf_length;
	uint32_t			info_buf_offset;
};

/*
 * CoNdisIndicateStatus message
 */
struct rcondis_indicate_status {
	/* RNDIS handle */
	uint32_t			ndis_vc_handle;
	/* RNDIS status */
	uint32_t			status;
	uint32_t			status_buf_length;
	uint32_t			status_buf_offset;
};

/*
 * CONDIS Call/VC parameters
 */

struct rcondis_specific_params {
	uint32_t			type;
	uint32_t			length;
	uint32_t			offset;
};

struct rcondis_media_params {
	uint32_t			flags;
	uint32_t			reserved1;
	uint32_t			reserved2;
	struct rcondis_specific_params	media_specific;
};

struct rndis_flowspec {
	uint32_t			token_rate;
	uint32_t			token_bucket_size;
	uint32_t			peak_bandwidth;
	uint32_t			latency;
	uint32_t			delay_variation;
	uint32_t			service_type;
	uint32_t			max_sdu_size;
	uint32_t			minimum_policed_size;
};

struct rcondis_call_manager_params {
	struct rndis_flowspec		transmit;
	struct rndis_flowspec		receive;
	struct rcondis_specific_params	call_mgr_specific;
};

/*
 * CoNdisMiniportActivateVc message
 */
struct rcondis_mp_activate_vc_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	uint32_t			flags;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
	uint32_t			media_params_offset;
	uint32_t			media_params_length;
	uint32_t			call_mgr_params_offset;
	uint32_t			call_mgr_params_length;
};

/*
 * Response to CoNdisMiniportActivateVc
 */
struct rcondis_mp_activate_vc_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * CoNdisMiniportDeactivateVc message
 */
struct rcondis_mp_deactivate_vc_req {
	/* RNDIS request ID */
	uint32_t			request_id;
	uint32_t			flags;
	/* RNDIS handle */
	uint32_t			device_vc_handle;
};

/*
 * Response to CoNdisMiniportDeactivateVc
 */
struct rcondis_mp_deactivate_vc_comp {
	/* RNDIS request ID */
	uint32_t			request_id;
	/* RNDIS status */
	uint32_t			status;
};

/*
 * Container with all of the RNDIS messages
 */
union rndis_msg_cont {
	struct rndis_pkt			pkt;
	struct rndis_init_req			init_req;
	struct rndis_halt_req			halt_req;
	struct rndis_query_req			query_req;
	struct rndis_set_req			set_req;
	struct rndis_reset_req			reset_req;
	struct rndis_keepalive_req		keepalive_req;
	struct rndis_indicate_status		indicate_status;
	struct rndis_init_comp			init_comp;
	struct rndis_query_comp			query_comp;
	struct rndis_set_comp			set_comp;
	struct rndis_reset_comp			reset_comp;
	struct rndis_keepalive_comp		keepalive_comp;
	struct rcondis_mp_create_vc		co_mp_create_vc;
	struct rcondis_mp_delete_vc		co_mp_delete_vc;
	struct rcondis_indicate_status		co_mp_status;
	struct rcondis_mp_activate_vc_req	co_mp_activate_vc;
	struct rcondis_mp_deactivate_vc_req	co_mp_deactivate_vc;
	struct rcondis_mp_create_vc_comp	co_mp_create_vc_comp;
	struct rcondis_mp_delete_vc_comp	co_mp_delete_vc_comp;
	struct rcondis_mp_activate_vc_comp	co_mp_activate_vc_comp;
	struct rcondis_mp_deactivate_vc_comp	co_mp_deactivate_vc_comp;
	struct rndis_pkt_ex			pkt_ex;
};

/*
 * Remote NDIS message format
 */
struct rndis {
	uint32_t			msg_type;

	/*
	 * Total length of this message, from the beginning
	 * of the struct, in bytes.
	 */
	uint32_t			msg_len;

	/* Actual message */
	union rndis_msg_cont		msg;
};

/*
 * get the size of an RNDIS message. Pass in the message type,
 * rndis_set_req, rndis_packet for example
 */
#define RNDIS_HEADER_SIZE 				8
#define RNDIS_MESSAGE_SIZE(message)			\
	(sizeof(message) + RNDIS_HEADER_SIZE)

#define NDIS_PACKET_TYPE_DIRECTED		0x00000001
#define NDIS_PACKET_TYPE_MULTICAST		0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST		0x00000004
#define NDIS_PACKET_TYPE_BROADCAST		0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING		0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS		0x00000020
#define NDIS_PACKET_TYPE_SMT			0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL		0x00000080
#define NDIS_PACKET_TYPE_GROUP			0x00000100
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL		0x00000200
#define NDIS_PACKET_TYPE_FUNCTIONAL		0x00000400
#define NDIS_PACKET_TYPE_MAC_FRAME		0x00000800

#endif	/* _DEV_PV_RNDISREG_H_ */
