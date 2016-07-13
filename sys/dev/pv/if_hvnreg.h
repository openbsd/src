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

#ifndef _IF_HVNREG_H_
#define _IF_HVNREG_H_

#define NVSP_PROTOCOL_VERSION_1			0x00002
#define NVSP_PROTOCOL_VERSION_2			0x30002
#define NVSP_PROTOCOL_VERSION_4			0x40000
#define NVSP_PROTOCOL_VERSION_5			0x50000

#define VERSION_4_OFFLOAD_SIZE                  22

#define NVSP_OPERATIONAL_STATUS_OK		0x00000000
#define NVSP_OPERATIONAL_STATUS_DEGRADED	0x00000001
#define NVSP_OPERATIONAL_STATUS_NONRECOVERABLE	0x00000002
#define NVSP_OPERATIONAL_STATUS_NO_CONTACT	0x00000003
#define NVSP_OPERATIONAL_STATUS_LOST_COMM	0x00000004

/*
 * Maximun number of transfer pages (packets) the VSP will use on a receive
 */
#define NVSP_MAX_PACKETS_PER_RECEIVE		375

enum nvsp_type {
	nvsp_type_none = 0,

	/*
	 * Init Messages
	 */
	nvsp_type_init = 1,
	nvsp_type_init_comp = 2,

	nvsp_type_version_start = 100,

	/*
	 * Version 1 Messages
	 */
	nvsp_type_send_ndis_vers = nvsp_type_version_start,

	nvsp_type_send_rx_buf,
	nvsp_type_send_rx_buf_comp,
	nvsp_type_revoke_rx_buf,

	nvsp_type_send_tx_buf,
	nvsp_type_send_tx_buf_comp,
	nvsp_type_revoke_tx_buf,

	nvsp_type_send_rndis_pkt,
	nvsp_type_send_rndis_pkt_comp,

	/*
	 * Version 2 Messages
	 */
	nvsp_type_send_chimney_delegated_buf,
	nvsp_type_send_chimney_delegated_buf_comp,
	nvsp_type_revoke_chimney_delegated_buf,

	nvsp_type_resume_chimney_rx_indication,

	nvsp_type_terminate_chimney,
	nvsp_type_terminate_chimney_comp,

	nvsp_type_indicate_chimney_event,

	nvsp_type_send_chimney_packet,
	nvsp_type_send_chimney_packet_comp,

	nvsp_type_post_chimney_rx_req,
	nvsp_type_post_chimney_rx_req_comp,

	nvsp_type_alloc_rx_buf,
	nvsp_type_alloc_rx_buf_comp,

	nvsp_type_free_rx_buf,

	nvsp_type_send_vmq_rndis_pkt,
	nvsp_type_send_vmq_rndis_pkt_comp,

	nvsp_type_send_ndis_config,

	nvsp_type_alloc_chimney_handle,
	nvsp_type_alloc_chimney_handle_comp,

	/*
	 * Version 4 Messages
	 */
	nvsp_type_send_vf_association,
	nvsp_type_switch_data_path,
	nvsp_type_uplink_connect_state_deprecated,

	/*
	 * Version 5 Messages
	 */
	nvsp_type_oid_query_ex,
	nvsp_type_oid_query_ex_comp,
	nvsp_type_subchannel,
	nvsp_type_send_indirection_table,
};

enum nvsp_status {
	nvsp_status_none = 0,
	nvsp_status_success,
	nvsp_status_failure,
	/* Deprecated */
	nvsp_status_prot_vers_range_too_new,
	/* Deprecated */
	nvsp_status_prot_vers_range_too_old,
	nvsp_status_invalid_rndis_pkt,
	nvsp_status_busy,
	nvsp_status_max,
};

/*
 * Init Messages
 */

/*
 * This message is used by the VSC to initialize the channel
 * after the channels has been opened. This message should
 * never include anything other then versioning (i.e. this
 * message will be the same for ever).
 *
 * Forever is a long time.  The values have been redefined
 * in Win7 to indicate major and minor protocol version
 * number.
 */
struct nvsp_init {
	uint32_t				protocol_version;
	uint32_t				protocol_version_2;
} __packed;

/*
 * This message is used by the VSP to complete the initialization
 * of the channel. This message should never include anything other
 * then versioning (i.e. this message will be the same forever).
 */
struct nvsp_init_comp {
	/* Deprecated */
	uint32_t				negotiated_prot_vers;
	uint32_t				max_mdl_chain_len;
	uint32_t				status;
} __packed;

/*
 * Version 1 Messages
 */

/*
 * This message is used by the VSC to send the NDIS version
 * to the VSP.	The VSP can use this information when handling
 * OIDs sent by the VSC.
 */
struct nvsp_send_ndis_version {
	uint32_t				ndis_major_vers;
	/* Deprecated */
	uint32_t				ndis_minor_vers;
} __packed;

/*
 * This message is used by the VSC to send a receive buffer
 * to the VSP. The VSP can then use the receive buffer to
 * send data to the VSC.
 */
struct nvsp_send_rx_buf {
	uint32_t				gpadl_handle;
	uint16_t				id;
} __packed;

struct nvsp_rx_buf_section {
	uint32_t				offset;
	uint32_t				sub_allocation_size;
	uint32_t				num_sub_allocations;
	uint32_t				end_offset;
} __packed;

/*
 * This message is used by the VSP to acknowledge a receive
 * buffer send by the VSC.  This message must be sent by the
 * VSP before the VSP uses the receive buffer.
 */
struct nvsp_send_rx_buf_comp {
	uint32_t				status;
	uint32_t				num_sections;

	/*
	 * The receive buffer is split into two parts, a large
	 * suballocation section and a small suballocation
	 * section. These sections are then suballocated by a
	 * certain size.
	 *
	 * For example, the following break up of the receive
	 * buffer has 6 large suballocations and 10 small
	 * suballocations.
	 *
	 * |            Large Section          |  |   Small Section   |
	 * ------------------------------------------------------------
	 * |     |     |     |     |     |     |  | | | | | | | | | | |
	 * |                                      |
	 * LargeOffset                            SmallOffset
	 */
	struct nvsp_rx_buf_section		sections[1];
} __packed;

/*
 * This message is sent by the VSC to revoke the receive buffer.
 * After the VSP completes this transaction, the VSP should never
 * use the receive buffer again.
 */
struct nvsp_revoke_rx_buf {
	uint16_t				id;
} __packed;

/*
 * This message is used by the VSC to send a send buffer
 * to the VSP. The VSC can then use the send buffer to
 * send data to the VSP.
 */
struct nvsp_send_tx_buf {
	uint32_t				gpadl_handle;
	uint16_t				id;
} __packed;

/*
 * This message is used by the VSP to acknowledge a send
 * buffer sent by the VSC. This message must be sent by the
 * VSP before the VSP uses the sent buffer.
 */
struct nvsp_send_tx_buf_comp {
	uint32_t				status;

	/*
	 * The VSC gets to choose the size of the send buffer and
	 * the VSP gets to choose the sections size of the buffer.
	 * This was done to enable dynamic reconfigurations when
	 * the cost of GPA-direct buffers decreases.
	 */
	uint32_t				section_size;
} __packed;

/*
 * This message is sent by the VSC to revoke the send buffer.
 * After the VSP completes this transaction, the vsp should never
 * use the send buffer again.
 */
struct nvsp_revoke_tx_buf {
	uint16_t				id;
} __packed;

/*
 * This message is used by both the VSP and the VSC to send
 * an RNDIS message to the opposite channel endpoint.
 */
struct nvsp_send_rndis_pkt {
	/*
	 * This field is specified by RNIDS.  They assume there's
	 * two different channels of communication. However,
	 * the Network VSP only has one.  Therefore, the channel
	 * travels with the RNDIS packet.
	 */
	uint32_t				chan_type;

	/*
	 * This field is used to send part or all of the data
	 * through a send buffer. This values specifies an
	 * index into the send buffer.	If the index is
	 * 0xFFFFFFFF, then the send buffer is not being used
	 * and all of the data was sent through other VMBus
	 * mechanisms.
	 */
	uint32_t				send_buf_section_idx;
	uint32_t				send_buf_section_size;
} __packed;

/*
 * This message is used by both the VSP and the VSC to complete
 * a RNDIS message to the opposite channel endpoint.  At this
 * point, the initiator of this message cannot use any resources
 * associated with the original RNDIS packet.
 */
struct nvsp_send_rndis_pkt_comp {
	uint32_t				status;
} __packed;


/*
 * Version 2 Messages
 */

/*
 * This message is used by the VSC to send the NDIS version
 * to the VSP.	The VSP can use this information when handling
 * OIDs sent by the VSC.
 */
struct nvsp_netvsc_capabilities {
	union {
		uint64_t			as_uint64;
		struct {
			uint64_t		vmq	      : 1;
			uint64_t		chimney	      : 1;
			uint64_t		sriov	      : 1;
			uint64_t		ieee8021q     : 1;
			uint64_t		correlationid : 1;
			uint64_t		teaming	      : 1;
		} ;
	};
} __packed;

struct nvsp_send_ndis_config {
	uint32_t				mtu;
	uint32_t				reserved;
	struct nvsp_netvsc_capabilities
						capabilities;
} __packed;

/*
 * NvspMessage2TypeSendChimneyDelegatedBuffer
 */
struct nvsp_send_chimney_buf {
	/*
	 * On WIN7 beta, delegated_obj_max_size is defined as a uint32_t
	 * Since WIN7 RC, it was split into two uint16_t.  To have the same
	 * struct layout, delegated_obj_max_size shall be the first field.
	 */
	uint16_t				delegated_obj_max_size;

	/*
	 * The revision # of chimney protocol used between NVSC and NVSP.
	 *
	 * This revision is NOT related to the chimney revision between
	 * NDIS protocol and miniport drivers.
	 */
	uint16_t				revision;

	uint32_t				gpadl_handle;
} __packed;


/* Unsupported chimney revision 0 (only present in WIN7 beta) */
#define NVSP_CHIMNEY_REVISION_0			0

/* WIN7 Beta Chimney QFE */
#define NVSP_CHIMNEY_REVISION_1			1

/* The chimney revision since WIN7 RC */
#define NVSP_CHIMNEY_REVISION_2			2


/*
 * NvspMessage2TypeSendChimneyDelegatedBufferComplete
 */
struct nvsp_send_chimney_buf_comp {
	uint32_t				status;

	/*
	 * Maximum number outstanding sends and pre-posted receives.
	 *
	 * NVSC should not post more than SendQuota/ReceiveQuota packets.
	 * Otherwise, it can block the non-chimney path for an indefinite
	 * amount of time.
	 * (since chimney sends/receives are affected by the remote peer).
	 *
	 * Note: NVSP enforces the quota restrictions on a per-VMBCHANNEL
	 * basis.  It doesn't enforce the restriction separately for chimney
	 * send/receive.  If NVSC doesn't voluntarily enforce "SendQuota",
	 * it may kill its own network connectivity.
	 */
	uint32_t				tx_quota;
	uint32_t				rx_quota;
} __packed;

/*
 * NvspMessage2TypeRevokeChimneyDelegatedBuffer
 */
struct nvsp_revoke_chimney_buf {
	uint32_t				gpadl_handle;
} __packed;


#define NVSP_CHIMNEY_OBJECT_TYPE_NEIGHBOR	0
#define NVSP_CHIMNEY_OBJECT_TYPE_PATH4		1
#define NVSP_CHIMNEY_OBJECT_TYPE_PATH6		2
#define NVSP_CHIMNEY_OBJECT_TYPE_TCP		3

/*
 * NvspMessage2TypeAllocateChimneyHandle
 */
struct nvsp_alloc_chimney_handle {
	uint64_t				vsc_context;
	uint32_t				object_type;
} __packed;

/*
 * NvspMessage2TypeAllocateChimneyHandleComplete
 */
struct nvsp_alloc_chimney_handle_comp {
	uint32_t				vsp_handle;
} __packed;


/*
 * NvspMessage2TypeResumeChimneyRXIndication
 */
struct nvsp_resume_chimney_rx_indication {
	/*
	 * Handle identifying the offloaded connection
	 */
	uint32_t				vsp_tcp_handle;
} __packed;

/*
 * NvspMessage2TypeTerminateChimney
 */
struct nvsp_terminate_chimney {
	/*
	* Handle identifying the offloaded object
	*/
	uint32_t				vsp_handle;

	/*
	 * Terminate Offload Flags
	 *     Bit 0:
	 *	   When set to 0, terminate the offload at the destination NIC
	 *     Bit 1-31:  Reserved, shall be zero
	 */
	uint32_t				flags;

	union {
		/*
		 * This field is valid only when bit 0 of flags is clear.
		 * It specifies the index into the premapped delegated
		 * object buffer.  The buffer was sent through the
		 * NvspMessage2TypeSendChimneyDelegatedBuffer
		 * message at initialization time.
		 *
		 * NVSP will write the delegated state into the delegated
		 * buffer upon upload completion.
		 */
		uint32_t			index;

		/*
		 * This field is valid only when bit 0 of flags is set.
		 *
		 * The seqence number of the most recently accepted RX
		 * indication when VSC sets its TCP context into
		 * "terminating" state.
		 *
		 * This allows NVSP to determines if there are any in-flight
		 * RX indications for which the acceptance state is still
		 * undefined.
		 */
		uint64_t			last_accepted_rx_seq_no;
	};
} __packed;

/*
 * NvspMessage2TypeTerminateChimneyComplete
 */
struct nvsp_terminate_chimney_comp {
	uint64_t				vsc_context;
	uint32_t				flags;
} __packed;

/*
 * NvspMessage2TypeIndicateChimneyEvent
 */
struct nvsp_indicate_chimney_event {
	/*
	 * When VscTcpContext is 0, event_type is an NDIS_STATUS;
	 * Otherwise, EventType is an TCP connection event (defined
	 * in NdisTcpOffloadEventHandler chimney DDK document).
	 */
	uint32_t				event_type;

	/*
	 * When VscTcpContext is 0, EventType is an NDIS_STATUS;
	 * Otherwise, EventType is an TCP connection event specific
	 * information (defined in NdisTcpOffloadEventHandler
	 * chimney DDK document).
	 */
	uint32_t				event_specific_info;

	/*
	 * If not 0, the event is per-TCP connection event.  This field
	 * contains the VSC's TCP context. If 0, the event indication is
	 * global.
	 */
	uint64_t				vsc_tcp_context;
} __packed;


#define NVSP_INVALID_OOB_INDEX			0xffffu
#define NVSP_INVALID_SECTION_INDEX		0xffffffff

/*
 * NvspMessage2TypeSendChimneyPacket
 */
struct nvsp_send_chimney_pkt {
	/*
	 * Identify the TCP connection for which this chimney send is
	 */
	uint32_t				vsp_tcp_handle;

	/*
	 * This field is used to send part or all of the data
	 * through a send buffer. This values specifies an
	 * index into the send buffer. If the index is
	 * 0xFFFF, then the send buffer is not being used
	 * and all of the data was sent through other VMBus
	 * mechanisms.
	 */
	uint16_t				send_buf_section_index;
	uint16_t				send_buf_section_size;

	/*
	 * OOB Data Index
	 * This an index to the OOB data buffer. If the index is 0xFFFFFFFF,
	 * then there is no OOB data.
	 *
	 * This field shall be always 0xFFFFFFFF for now. It is reserved for
	 * the future.
	 */
	uint16_t				oob_data_index;

	/*
	 * DisconnectFlags = 0
	 * Normal chimney send. See MiniportTcpOffloadSend for details.
	 *
	 * DisconnectFlags = TCP_DISCONNECT_GRACEFUL_CLOSE (0x01)
	 * Graceful disconnect. See MiniportTcpOffloadDisconnect for details.
	 *
	 * DisconnectFlags = TCP_DISCONNECT_ABORTIVE_CLOSE (0x02)
	 * Abortive disconnect. See MiniportTcpOffloadDisconnect for details.
	 */
	uint16_t				disconnect_flags;

	uint32_t				seq_no;
} __packed;

/*
 * NvspMessage2TypeSendChimneyPacketComplete
 */
struct nvsp_send_chimney_pkt_comp {
	/*
	 * The NDIS_STATUS for the chimney send
	 */
	uint32_t				status;

	/*
	 * Number of bytes that have been sent to the peer (and ACKed by the peer).
	 */
	uint32_t				bytes_transferred;
} __packed;

/*
 * NvspMessage2TypePostChimneyRecvRequest
 */
struct nvsp_post_chimney_rx_req {
	/*
	 * Identify the TCP connection which this chimney receive request
	 * is for.
	 */
	uint32_t				vsp_tcp_handle;

	/*
	 * OOB Data Index
	 * This an index to the OOB data buffer. If the index is 0xFFFFFFFF,
	 * then there is no OOB data.
	 *
	 * This field shall be always 0xFFFFFFFF for now. It is reserved for
	 * the future.
	 */
	uint32_t				oob_data_index;

	/*
	 * Bit 0
	 *	When it is set, this is a "no-push" receive.
	 *	When it is clear, this is a "push" receive.
	 *
	 * Bit 1-15:  Reserved and shall be zero
	 */
	uint16_t				flags;

	/*
	 * For debugging and diagnoses purpose.
	 * The SeqNo is per TCP connection and starts from 0.
	 */
	uint32_t				seq_no;
} __packed;

/*
 * NvspMessage2TypePostChimneyRecvRequestComplete
 */
struct nvsp_post_chimney_rx_req_comp {
	/*
	 * The NDIS_STATUS for the chimney send
	 */
	uint32_t				status;

	/*
	 * Number of bytes that have been sent to the peer (and ACKed by
	 * the peer).
	 */
	uint32_t				bytes_xferred;
} __packed;

/*
 * NvspMessage2TypeAllocateReceiveBuffer
 */
struct nvsp_alloc_rx_buf {
	/*
	 * Allocation ID to match the allocation request and response
	 */
	uint32_t				allocation_id;

	/*
	 * Length of the VM shared memory receive buffer that needs to
	 * be allocated
	 */
	uint32_t				length;
} __packed;

/*
 * NvspMessage2TypeAllocateReceiveBufferComplete
 */
struct nvsp_alloc_rx_buf_comp {
	/*
	 * The NDIS_STATUS code for buffer allocation
	 */
	uint32_t				status;

	/*
	 * Allocation ID from NVSP_MESSAGE_ALLOCATE_RECEIVE_BUFFER
	 */
	uint32_t				allocation_id;

	/*
	 * GPADL handle for the allocated receive buffer
	 */
	uint32_t				gpadl_handle;

	/*
	 * Receive buffer ID that is further used in
	 * NvspMessage2SendVmqRndisPacket
	 */
	uint64_t				rx_buf_id;
} __packed;

/*
 * NvspMessage2TypeFreeReceiveBuffer
 */
struct nvsp_free_rx_buf {
	/*
	 * Receive buffer ID previous returned in
	 * NvspMessage2TypeAllocateReceiveBufferComplete message
	 */
	uint64_t				rx_buf_id;
} __packed;

/*
 * This structure is used in defining the buffers in
 * NVSP_MESSAGE_SEND_VMQ_RNDIS_PACKET structure
 */
struct nvsp_xfer_page_range {
	/*
	 * Specifies the ID of the receive buffer that has the buffer. This
	 * ID can be the general receive buffer ID specified in
	 * NvspMessage1TypeSendReceiveBuffer or it can be the shared memory
	 * receive buffer ID allocated by the VSC and specified in
	 * NvspMessage2TypeAllocateReceiveBufferComplete message
	 */
	uint64_t				xfer_page_set_id;

	/*
	 * Number of bytes
	 */
	uint32_t				byte_count;

	/*
	 * Offset in bytes from the beginning of the buffer
	 */
	uint32_t				byte_offset;
} __packed;

/*
 * NvspMessage2SendVmqRndisPacket
 */
struct nvsp_send_vmq_rndis_pkt {
	/*
	 * This field is specified by RNIDS. They assume there's
	 * two different channels of communication. However,
	 * the Network VSP only has one. Therefore, the channel
	 * travels with the RNDIS packet. It must be RMC_DATA
	 */
	uint32_t				channel_type;

	/*
	 * Only the Range element corresponding to the RNDIS header of
	 * the first RNDIS message in the multiple RNDIS messages sent
	 * in one NVSP message.	 Information about the data portions as well
	 * as the subsequent RNDIS messages in the same NVSP message are
	 * embedded in the RNDIS header itself
	 */
	struct nvsp_xfer_page_range		range;
} __packed;

/*
 * This message is used by the VSC to complete
 * a RNDIS VMQ message to the VSP.  At this point,
 * the initiator of this message can use any resources
 * associated with the original RNDIS VMQ packet.
 */
struct nvsp_send_vmq_rndis_pkt_comp {
	uint32_t				status;
} __packed;

/*
 * Version 5 messages
 */
enum nvsp_subchannel_operation {
	NVSP_SUBCHANNEL_NONE = 0,
	NVSP_SUBCHANNE_ALLOCATE,
	NVSP_SUBCHANNE_MAX
};

struct nvsp_subchannel_req {
	uint32_t				op;
	uint32_t				num_subchannels;
} __packed;

struct nvsp_subchannel_comp {
	uint32_t				status;
	/* Actual number of subchannels allocated */
	uint32_t				num_subchannels;
} __packed;

struct nvsp_send_indirect_table {
	/* The number of entries in the send indirection table */
	uint32_t				count;
	/*
	 * The offset of the send indireciton table from top of
	 * this struct. The send indirection table tells which channel
	 * to put the send traffic on. Each entry is a channel number.
	 */
	uint32_t				offset;
} __packed;

/*
 * All messages
 */
struct nvsp {
	uint32_t				msg_type;

	union {
	struct nvsp_init			init;
	struct nvsp_init_comp			init_compl;

	/* Version 1 */
	struct nvsp_send_ndis_version		send_ndis_vers;

	struct nvsp_send_rx_buf			send_rx_buf;
	struct nvsp_send_rx_buf_comp		send_rx_buf_comp;
	struct nvsp_revoke_rx_buf		revoke_rx_buf;

	struct nvsp_send_tx_buf			send_tx_buf;
	struct nvsp_send_tx_buf_comp		send_tx_buf_comp;
	struct nvsp_revoke_tx_buf		revoke_tx_buf;

	struct nvsp_send_rndis_pkt		send_rndis_pkt;
	struct nvsp_send_rndis_pkt_comp		send_rndis_pkt_comp;

	/* Version 2 */
	struct nvsp_send_ndis_config		send_ndis_config;

	struct nvsp_send_chimney_buf		send_chimney_buf;
	struct nvsp_send_chimney_buf_comp	send_chimney_buf_comp;
	struct nvsp_revoke_chimney_buf		revoke_chimney_buf;

	struct nvsp_resume_chimney_rx_indication resume_chimney_rx_indication;
	struct nvsp_terminate_chimney		terminate_chimney;
	struct nvsp_terminate_chimney_comp	terminate_chimney_comp;
	struct nvsp_indicate_chimney_event	indicate_chimney_event;

	struct nvsp_send_chimney_pkt		send_chimney_packet;
	struct nvsp_send_chimney_pkt_comp	send_chimney_packet_comp;
	struct nvsp_post_chimney_rx_req		post_chimney_rx_req;
	struct nvsp_post_chimney_rx_req_comp	post_chimney_rx_req_comp;

	struct nvsp_alloc_rx_buf		alloc_rx_buffer;
	struct nvsp_alloc_rx_buf_comp		alloc_rx_buffer_comp;
	struct nvsp_free_rx_buf			free_rx_buffer;

	struct nvsp_send_vmq_rndis_pkt		send_vmq_rndis_pkt;
	struct nvsp_send_vmq_rndis_pkt_comp	send_vmq_rndis_pkt_comp;
	struct nvsp_alloc_chimney_handle	alloc_chimney_handle;
	struct nvsp_alloc_chimney_handle_comp	alloc_chimney_handle_comp;

	/* Version 5 */
	struct nvsp_subchannel_req		subchannel_req;
	struct nvsp_subchannel_comp		subchn_comp;
	struct nvsp_send_indirect_table		send_table;
	}					msg;
} __packed;

#endif	/* _IF_HVNREG_H_ */
