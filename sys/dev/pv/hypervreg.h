/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
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

#ifndef _HYPERVREG_H_
#define _HYPERVREG_H_

/*
 *  hyperv.h
 */

/*
 * VMBUS version is 32 bit, upper 16 bit for major_number and lower
 * 16 bit for minor_number.
 *
 * 0.13  --  Windows Server 2008
 * 1.1   --  Windows 7
 * 2.4   --  Windows 8
 * 3.0   --  Windows 8.1
 */
#define HV_VMBUS_VERSION_WS2008		((0 << 16) | (13))
#define HV_VMBUS_VERSION_WIN7		((1 << 16) | (1))
#define HV_VMBUS_VERSION_WIN8		((2 << 16) | (4))
#define HV_VMBUS_VERSION_WIN8_1		((3 << 16) | (0))
#define HV_VMBUS_VERSION_INVALID	-1
#define HV_VMBUS_VERSION_CURRENT	HV_VMBUS_VERSION_WIN8_1

#define HV_CONNECTION_ID_MASK		0x1ffffff

/* Pipe modes */
#define HV_PIPE_TYPE_BYTE		0x00000000
#define HV_PIPE_TYPE_MESSAGE		0x00000004

/* The size of the user defined data buffer for non-pipe offers */
#define HV_MAX_USER_BYTES		120

/* The size of the user defined data buffer for pipe offers */
#define HV_MAX_PIPE_USER_BYTES		116

struct hv_guid {
	 unsigned char			data[16];
} __packed;

/*
 * This struct contains the fundamental information about an offer.
 */
struct hv_channel_offer {
	struct hv_guid			interface_type;
	struct hv_guid			interface_instance;
	uint64_t			interrupt_latency_in_100ns_units;
	uint32_t			interface_revision;
	uint32_t			server_context_area_size; /* in bytes */
	uint16_t			channel_flags;
	uint16_t			mmio_megabytes;	/* bytes * 1024*1024 */
	union {
		/*
		 * Non-pipes: The user has HV_MAX_USER_BYTES bytes.
		 */
		struct {
			uint8_t		user_defined[HV_MAX_USER_BYTES];
		} __packed standard;

		/*
		 * Pipes: The following structure is an integrated pipe
		 * protocol, which is implemented on top of standard user-
		 * defined data. pipe clients have HV_MAX_PIPE_USER_BYTES
		 * left for their own use.
		 */
		struct {
			uint32_t	pipe_mode;
			uint8_t		user_defined[HV_MAX_PIPE_USER_BYTES];
		} __packed pipe;
	} u;

	/*
	 * Sub_channel_index, newly added in Win8.
	 */
	uint16_t			sub_channel_index;
	uint16_t			padding;
} __packed;

struct hv_pktdesc {
	uint16_t			type;
	uint16_t			offset;
	uint16_t			length;
	uint16_t			flags;
	uint64_t			tid;
} __packed;

struct hv_transfer_page {
	uint32_t			byte_count;
	uint32_t			byte_offset;
} __packed;

struct hv_transfer_page_header {
	uint16_t			set_id;
	uint8_t				sender_owns_set;
	uint8_t				reserved;
	uint32_t			range_count;
	struct hv_transfer_page		range[0];
} __packed;

/*
 * This structure defines a range in guest physical space that
 * can be made to look virtually contiguous.
 */
struct hv_gpa_range {
	uint32_t			byte_count;
	uint32_t			byte_offset;
	uint64_t			pfn_array[0];
} __packed;

#define HV_PKT_INVALID				0x0
#define HV_PKT_SYNCH				0x1
#define HV_PKT_ADD_TRANSFER_PAGE_SET		0x2
#define HV_PKT_REMOVE_TRANSFER_PAGE_SET		0x3
#define HV_PKT_ESTABLISH_GPADL			0x4
#define HV_PKT_TEAR_DOWN_GPADL			0x5
#define HV_PKT_DATA_IN_BAND			0x6
#define HV_PKT_DATA_USING_TRANSFER_PAGES	0x7
#define HV_PKT_DATA_USING_GPADL			0x8
#define HV_PKT_DATA_USING_GPA_DIRECT		0x9
#define HV_PKT_CANCEL_REQUEST			0xa
#define HV_PKT_COMPLETION			0xb
#define HV_PKT_DATA_USING_ADDITIONAL_PACKETS	0xc
#define HV_PKT_ADDITIONAL_DATA			0xd

#define HV_PKTFLAG_COMPLETION_REQUESTED		1

#define HV_CHANMSG_INVALID			0
#define HV_CHANMSG_OFFER_CHANNEL		1
#define HV_CHANMSG_RESCIND_CHANNEL_OFFER	2
#define HV_CHANMSG_REQUEST_OFFERS		3
#define HV_CHANMSG_ALL_OFFERS_DELIVERED		4
#define HV_CHANMSG_OPEN_CHANNEL			5
#define HV_CHANMSG_OPEN_CHANNEL_RESULT		6
#define HV_CHANMSG_CLOSE_CHANNEL		7
#define HV_CHANMSG_GPADL_HEADER			8
#define HV_CHANMSG_GPADL_BODY			9
#define HV_CHANMSG_GPADL_CREATED		10
#define HV_CHANMSG_GPADL_TEARDOWN		11
#define HV_CHANMSG_GPADL_TORNDOWN		12
#define HV_CHANMSG_REL_ID_RELEASED		13
#define HV_CHANMSG_INITIATED_CONTACT		14
#define HV_CHANMSG_VERSION_RESPONSE		15
#define HV_CHANMSG_UNLOAD			16
#define HV_CHANMSG_COUNT			17

struct hv_channel_msg_header {
	uint32_t			message_type;
	uint32_t			padding;
} __packed;

struct hv_channel_initiate_contact {
	struct hv_channel_msg_header	hdr;
	uint32_t			vmbus_version_requested;
	uint32_t			padding2;
	uint64_t			interrupt_page;
	uint64_t			monitor_page_1;
	uint64_t			monitor_page_2;
} __packed;

struct hv_channel_version_response {
	struct hv_channel_msg_header	header;
	uint8_t				version_supported;
} __packed;

/*
 * Common defines for Hyper-V ICs
 */
#define HV_ICMSGTYPE_NEGOTIATE			0
#define HV_ICMSGTYPE_HEARTBEAT			1
#define HV_ICMSGTYPE_KVPEXCHANGE		2
#define HV_ICMSGTYPE_SHUTDOWN			3
#define HV_ICMSGTYPE_TIMESYNC			4
#define HV_ICMSGTYPE_VSS			5

#define HV_ICMSGHDRFLAG_TRANSACTION		1
#define HV_ICMSGHDRFLAG_REQUEST			2
#define HV_ICMSGHDRFLAG_RESPONSE		4

struct hv_pipe_hdr {
	uint32_t			flags;
	uint32_t			msgsize;
} __packed;

struct hv_ic_version {
	uint16_t			major;
	uint16_t			minor;
} __packed;

struct hv_icmsg_hdr {
	struct hv_ic_version		icverframe;
	uint16_t			icmsgtype;
	struct hv_ic_version		icvermsg;
	uint16_t			icmsgsize;
	uint32_t			status;
	uint8_t				ictransaction_id;
	uint8_t				icflags;
	uint8_t				reserved[2];
} __packed;

struct hv_icmsg_negotiate {
	uint16_t			icframe_vercnt;
	uint16_t			icmsg_vercnt;
	uint32_t			reserved;
	struct hv_ic_version		icversion_data[1]; /* any size array */
} __packed;

struct hv_shutdown_msg {
	uint32_t			reason_code;
	uint32_t			timeout_seconds;
	uint32_t 			flags;
	uint8_t				display_message[2048];
} __packed;

struct hv_timesync_msg {
	uint64_t			parent_time;
	uint64_t			child_time;
	uint64_t			round_trip_time;
	uint8_t				flags;
#define  HV_TIMESYNC_PROBE		 0
#define  HV_TIMESYNC_SYNC		 1
#define  HV_TIMESYNC_SAMPLE		 2
} __packed;

struct hv_heartbeat_msg {
	uint64_t 			seq_num;
	uint32_t 			reserved[8];
} __packed;

struct hv_ring_buffer {
	/* Offset in bytes from the start of ring data below */
	volatile uint32_t		 write_index;
	/* Offset in bytes from the start of ring data below */
	volatile uint32_t		 read_index;
	/* Interrupt mask */
	volatile uint32_t		 interrupt_mask;
	/* Ring data starts on the next page */
	uint8_t				 reserved[4084];
	/* Data, doubles as interrupt mask */
	uint8_t				 buffer[0];
} __packed;

struct hv_page_buffer {
	int				length;
	int				offset;
	uint64_t			pfn;
} __packed;

#define HV_MAX_PAGE_BUFFERS		32

struct hv_gpadesc {
	uint16_t			type;
	uint16_t			offset;
	uint16_t			length;
	uint16_t			flags;
	uint64_t			tid;
	uint32_t			reserved;
	uint32_t			range_count;
} __packed;

/*
 * Channel Offer parameters
 */
struct hv_channel_offer_channel {
	struct hv_channel_msg_header	header;
	struct hv_channel_offer		offer;
	uint32_t			child_rel_id;
	uint8_t				monitor_id;
	/*
	 * This field has been split into a bit field on Win7 and higher.
	 */
	uint8_t				monitor_allocated:1;
	uint8_t				reserved1:7;
	/*
	 * Following fields were added in win7 and higher.
	 * Make sure to check the version before accessing these fields.
	 *
	 * If "is_dedicated_interrupt" is set, we must not set the
	 * associated bit in the channel bitmap while sending the
	 * interrupt to the host.
	 *
	 * connection_id is used in signaling the host.
	 */
	uint16_t			is_dedicated_interrupt:1;
	uint16_t			reserved2:15;
	uint32_t			connection_id;
} __packed;

/*
 * Open Channel parameters
 */
struct hv_channel_open {
	struct hv_channel_msg_header	header;
	/* Identifies the specific VMBus channel that is being opened */
	uint32_t			child_rel_id;
	/* ID making a particular open request at a channel offer unique */
	uint32_t			open_id;
	/* GPADL for the channel's ring buffer */
	uint32_t			ring_buffer_gpadl_handle;
	/*
	 * Before win8, all incoming channel interrupts are only delivered
	 * on cpu 0. Setting this value to 0 would preserve the earlier
	 * behavior.
	 */
	uint32_t			target_vcpu;
	/*
	 * The upstream ring buffer begins at offset zero in the memory
	 * described by ring_buffer_gpadl_handle. The downstream ring
	 * buffer follows it at this offset (in pages).
	 */
	uint32_t			downstream_ring_buffer_page_offset;
	/* User-specific data to be passed along to the server endpoint. */
	uint8_t				user_data[HV_MAX_USER_BYTES];
} __packed;

/*
 * Open Channel Result parameters
 */
struct hv_channel_open_result {
	struct hv_channel_msg_header	header;
	uint32_t			child_rel_id;
	uint32_t			open_id;
	uint32_t			status;
} __packed;

/*
 * Close channel parameters
 */
struct hv_channel_close {
	struct hv_channel_msg_header	header;
	uint32_t			child_rel_id;
} __packed;

/*
 * Channel Message GPADL
 */
#define HV_GPADL_TYPE_RING_BUFFER	1
#define HV_GPADL_TYPE_SERVER_SAVE_AREA	2
#define HV_GPADL_TYPE_TRANSACTION	8

/*
 * The number of PFNs in a GPADL message is defined by the number of
 * pages that would be spanned by byte_count and byte_offset. If the
 * implied number of PFNs won't fit in this packet, there will be a
 * follow-up packet that contains more.
 */

struct hv_gpadl_header {
	struct hv_channel_msg_header	header;
	uint32_t			child_rel_id;
	uint32_t			gpadl;
	uint16_t			range_buf_len;
	uint16_t			range_count;
	struct hv_gpa_range		range[0];
} __packed;

/* How many PFNs can be referenced by the header */
#define HV_NPFNHDR						\
	((HV_MESSAGE_PAYLOAD - sizeof(struct hv_gpadl_header) -	\
	    sizeof(struct hv_gpa_range)) / sizeof(uint64_t))

/*
 * This is the follow-up packet that contains more PFNs
 */
struct hv_gpadl_body {
	struct hv_channel_msg_header	header;
	uint32_t			message_number;
	uint32_t 			gpadl;
	uint64_t 			pfn[0];
} __packed;

/* How many PFNs can be referenced by the body */
#define HV_NPFNBODY						\
	((HV_MESSAGE_PAYLOAD - sizeof(struct hv_gpadl_body)) /	\
	    sizeof(uint64_t))

struct hv_gpadl_created {
	struct hv_channel_msg_header	header;
	uint32_t			child_rel_id;
	uint32_t			gpadl;
	uint32_t			creation_status;
} __packed;

struct hv_gpadl_teardown {
	struct hv_channel_msg_header	header;
	uint32_t			child_rel_id;
	uint32_t			gpadl;
} __packed;

struct hv_gpadl_torndown {
	struct hv_channel_msg_header	header;
	uint32_t			gpadl;
} __packed;

/*
 *  hv_vmbus_priv.h
 */

#define HV_MESSAGE_SIZE			256
#define HV_MESSAGE_PAYLOAD		240

/*
 * Hypervisor message IDs ???
 */
#define HV_MESSAGE_CONNECTION_ID	1
#define HV_MESSAGE_PORT_ID		1
#define HV_EVENT_CONNECTION_ID		2
#define HV_EVENT_PORT_ID		2
#define HV_MONITOR_CONNECTION_ID	3
#define HV_MONITOR_PORT_ID		3
#define HV_MESSAGE_SINT			2

/*
 * Hypervisor message types
 */
#define HV_MESSAGE_TYPE_NONE		0x00000000
#define HV_MESSAGE_TIMER_EXPIRED	0x80000010

struct hv_monitor_trigger_state {
	uint32_t			group_enable :4;
	uint32_t			reserved :28;
};

struct hv_monitor_trigger_group {
	uint32_t			pending;
	uint32_t			armed;
};

struct hv_monitor_parameter {
	uint32_t			connection_id;
	uint16_t			flag_number;
	uint16_t			reserved;
};

/*
 * Monitor page Layout
 * ------------------------------------------------------
 * | 0   | trigger_state (4 bytes) | reserved1 (4 bytes) |
 * | 8   | trigger_group[0]                              |
 * | 10  | trigger_group[1]                              |
 * | 18  | trigger_group[2]                              |
 * | 20  | trigger_group[3]                              |
 * | 28  | reserved2[0]                                  |
 * | 30  | reserved2[1]                                  |
 * | 38  | reserved2[2]                                  |
 * | 40  | next_check_time[0][0] | next_check_time[0][1] |
 * | ...                                                 |
 * | 240 | latency[0][0..3]                              |
 * | 340 | reserved3[0]                                  |
 * | 440 | parameter[0][0]                               |
 * | 448 | parameter[0][1]                               |
 * | ...                                                 |
 * | 840 | reserved4[0]                                  |
 * ------------------------------------------------------
 */
struct hv_monitor_page {
	struct hv_monitor_trigger_state	trigger_state;
	uint32_t			reserved1;

	struct hv_monitor_trigger_group trigger_group[4];
	uint64_t			reserved2[3];

	int32_t				next_check_time[4][32];

	uint16_t			latency[4][32];
	uint64_t			reserved3[32];

	struct hv_monitor_parameter	parameter[4][32];

	uint8_t				reserved4[1984];
};

struct hv_input_post_message {
	uint32_t			connection_id;
	uint32_t			reserved;
	uint32_t			message_type;
	uint32_t			payload_size;
	uint8_t				payload[HV_MESSAGE_PAYLOAD];
} __packed;

/*
 * Synthetic interrupt controller event flags
 */
struct hv_synic_event_flags {
	uint32_t			flags[64];
} __packed;

#define HV_X64_MSR_GUEST_OS_ID		0x40000000
#define HV_X64_MSR_HYPERCALL		0x40000001
#define  HV_X64_MSR_HYPERCALL_ENABLED	 (1 << 0)
#define  HV_X64_MSR_HYPERCALL_PASHIFT	 12
#define HV_X64_MSR_VP_INDEX		0x40000002
#define HV_X64_MSR_TIME_REF_COUNT	0x40000020

#define HV_S_OK				0x00000000
#define HV_E_FAIL			0x80004005
#define HV_ERROR_NOT_SUPPORTED		0x80070032
#define HV_ERROR_MACHINE_LOCKED		0x800704f7

/*
 * Synthetic interrupt controller message header
 */
struct hv_vmbus_msg_header {
	uint32_t			message_type;
	uint8_t				payload_size;
	uint8_t				message_flags;
#define  HV_SYNIC_MHF_PENDING		 0x0001
	uint8_t				reserved[2];
	union {
		uint64_t		sender;
		uint32_t		port;
	} u;
} __packed;

/*
 *  Define synthetic interrupt controller message format
 */
struct hv_vmbus_message {
	struct hv_vmbus_msg_header	header;
	uint64_t			payload[30];
} __packed;

/*
 *  Maximum channels is determined by the size of the interrupt
 *  page which is PAGE_SIZE. 1/2 of PAGE_SIZE is for
 *  send endpoint interrupt and the other is receive
 *  endpoint interrupt.
 *
 *   Note: (PAGE_SIZE >> 1) << 3 allocates 16348 channels
 */
#define HV_MAX_NUM_CHANNELS		((PAGE_SIZE >> 1) << 3)
#define HV_MAX_NUM_CHANNELS_SUPPORTED	256

/* Virtual APIC registers */
#define HV_X64_MSR_EOI			0x40000070
#define HV_X64_MSR_ICR			0x40000071
#define HV_X64_MSR_TPR			0x40000072
#define HV_X64_MSR_APIC_ASSIST_PAGE	0x40000073
#define  HV_APIC_ASSIST_PAGE_PASHIFT	 12

/*
 * Synthetic interrupt controller model specific registers
 */
/* Synthetic Interrupt Controll registers */
#define HV_X64_MSR_SCONTROL		0x40000080
#define  HV_X64_MSR_SCONTROL_ENABLED	 (1<<0)
#define HV_X64_MSR_SVERSION		0x40000081
/* Synthetic Interrupt Event Flags Page register */
#define HV_X64_MSR_SIEFP		0x40000082
#define  HV_X64_MSR_SIEFP_ENABLED	 (1<<0)
#define  HV_X64_MSR_SIEFP_PASHIFT	 12
/* Synthetic Interrupt Message Page register */
#define HV_X64_MSR_SIMP			0x40000083
#define  HV_X64_MSR_SIMP_ENABLED	 (1<<0)
#define  HV_X64_MSR_SIMP_PASHIFT	 12
#define HV_X64_MSR_EOM			0x40000084

#define HV_X64_MSR_SINT0		0x40000090
#define HV_X64_MSR_SINT1		0x40000091
#define HV_X64_MSR_SINT2		0x40000092
#define HV_X64_MSR_SINT3		0x40000093
#define HV_X64_MSR_SINT4		0x40000094
#define HV_X64_MSR_SINT5		0x40000095
#define HV_X64_MSR_SINT6		0x40000096
#define HV_X64_MSR_SINT7		0x40000097
#define HV_X64_MSR_SINT8		0x40000098
#define HV_X64_MSR_SINT9		0x40000099
#define HV_X64_MSR_SINT10		0x4000009A
#define HV_X64_MSR_SINT11		0x4000009B
#define HV_X64_MSR_SINT12		0x4000009C
#define HV_X64_MSR_SINT13		0x4000009D
#define HV_X64_MSR_SINT14		0x4000009E
#define HV_X64_MSR_SINT15		0x4000009F
#define  HV_X64_MSR_SINT_VECTOR		 0xff
#define  HV_X64_MSR_SINT_MASKED		 (1<<16)
#define  HV_X64_MSR_SINT_AUTOEOI	 (1<<17)

/*
 * Hypercalls
 */
#define HV_CALL_POST_MESSAGE		0x005c
#define HV_CALL_SIGNAL_EVENT		0x005d

/*
 * Hypercall status codes
 */
#define HV_STATUS_SUCCESS			0
#define HV_STATUS_INVALID_HYPERCALL_CODE	2
#define HV_STATUS_INVALID_HYPERCALL_INPUT	3
#define HV_STATUS_INVALID_ALIGNMENT		4
#define HV_STATUS_INSUFFICIENT_MEMORY		11
#define HV_STATUS_INVALID_CONNECTION_ID		18
#define HV_STATUS_INSUFFICIENT_BUFFERS		19

/*
 * XXX: Hypercall signal input structure
 */
struct hv_input_signal_event {
	uint32_t			connection_id;
	uint16_t			flag_number;
	uint16_t			reserved;
} __packed;

#endif	/* _HYPERVREG_H_ */
