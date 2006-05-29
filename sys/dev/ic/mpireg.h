/*	$OpenBSD: mpireg.h,v 1.5 2006/05/29 18:58:59 dlg Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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

/*
 * System Interface Register Set
 */

#define MPI_DOORBELL		0x00
/* doorbell read bits */
#define  MPI_DOORBELL_STATE		(0xf<<28) /* ioc state */
#define  MPI_DOORBELL_STATE_RESET	(0x0<<28)
#define  MPI_DOORBELL_STATE_READY	(0x1<<28)
#define  MPI_DOORBELL_STATE_OPER	(0x2<<28)
#define  MPI_DOORBELL_STATE_FAULT	(0x4<<28)
#define  MPI_DOORBELL_INUSE		(0x1<<27) /* doorbell used */
#define  MPI_DOORBELL_WHOINIT		(0x7<<24) /* last to reset ioc */
#define  MPI_DOORBELL_WHOINIT_NOONE	(0x0<<24) /* not initialized */
#define  MPI_DOORBELL_WHOINIT_SYSBIOS	(0x1<<24) /* system bios */
#define  MPI_DOORBELL_WHOINIT_ROMBIOS	(0x2<<24) /* rom bios */
#define  MPI_DOORBELL_WHOINIT_PCIPEER	(0x3<<24) /* pci peer */
#define  MPI_DOORBELL_WHOINIT_DRIVER	(0x4<<24) /* host driver */
#define  MPI_DOORBELL_WHOINIT_MANUFACT	(0x5<<24) /* manufacturing */
#define  MPI_DOORBELL_FAULT		(0xffff<<0) /* fault code */
#define  MPI_DOORBELL_FAULT_REQ_PCIPAR	0x8111 /* req msg pci parity err */
#define  MPI_DOORBELL_FAULT_REQ_PCIBUS	0x8112 /* req msg pci bus err */
#define  MPI_DOORBELL_FAULT_REP_PCIPAR	0x8113 /* reply msg pci parity err */
#define  MPI_DOORBELL_FAULT_REP_PCIBUS	0x8114 /* reply msg pci bus err */
#define  MPI_DOORBELL_FAULT_SND_PCIPAR	0x8115 /* data send pci parity err */
#define  MPI_DOORBELL_FAULT_SND_PCIBUS	0x8116 /* data send pci bus err */
#define  MPI_DOORBELL_FAULT_RCV_PCIPAR	0x8117 /* data recv pci parity err */
#define  MPI_DOORBELL_FAULT_RCV_PCIBUS	0x8118 /* data recv pci bus err */
/* doorbell write bits */
#define  MPI_DOORBELL_FUNCTION_SHIFT	24
#define  MPI_DOORBELL_FUNCTION_MASK	(0xff << MPI_DOORBELL_FUNCTION_SHIFT)
#define  MPI_DOORBELL_FUNCTION(x)	\
    (((x) << MPI_DOORBELL_FUNCTION_SHIFT) & MPI_DOORBELL_FUNCTION_MASK)
#define  MPI_DOORBELL_DWORDS_SHIFT	16
#define  MPI_DOORBELL_DWORDS_MASK	(0xff << MPI_DOORBELL_DWORDS_SHIFT)
#define  MPI_DOORBELL_DWORDS(x)		\
    (((x) << MPI_DOORBELL_DWORDS_SHIFT) & MPI_DOORBELL_DWORDS_MASK)
#define  MPI_DOORBELL_DATA_MASK		0xffff

#define MPI_WRITESEQ		0x04
#define  MPI_WRITESEQ_VALUE		0x0000000f /* key value */
#define  MPI_WRITESEQ_1			0x04
#define  MPI_WRITESEQ_2			0x0b
#define  MPI_WRITESEQ_3			0x02
#define  MPI_WRITESEQ_4			0x07
#define  MPI_WRITESEQ_5			0x0d

#define MPI_HOSTDIAG		0x08
#define  MPI_HOSTDIAG_CLEARFBS		(1<<10) /* clear flash bad sig */
#define  MPI_HOSTDIAG_POICB		(1<<9) /* prevent ioc boot */
#define  MPI_HOSTDIAG_DWRE		(1<<7) /* diag reg write enabled */
#define  MPI_HOSTDIAG_FBS		(1<<6) /* flash bad sig */
#define  MPI_HOSTDIAG_RESET_HIST	(1<<5) /* reset history */
#define  MPI_HOSTDIAG_DIAGWR_EN		(1<<4) /* diagnostic write enabled */
#define  MPI_HOSTDIAG_RESET_ADAPTER	(1<<2) /* reset adapter */
#define  MPI_HOSTDIAG_DISABLE_ARM	(1<<1) /* disable arm */
#define  MPI_HOSTDIAG_DIAGMEM_EN	(1<<0) /* diag mem enable */

#define MPI_TESTBASE		0x0c

#define MPI_DIAGRWDATA		0x10

#define MPI_DIAGRWADDR		0x18

#define MPI_INTR_STATUS		0x30
#define  MPI_INTR_STATUS_IOCDOORBELL	(1<<31) /* ioc doorbell status */
#define  MPI_INTR_STATUS_REPLY		(1<<3) /* reply message interrupt */
#define  MPI_INTR_STATUS_DOORBELL	(1<<0) /* doorbell interrupt */

#define MPI_INTR_MASK		0x34
#define  MPI_INTR_MASK_REPLY		(1<<3) /* reply message intr mask */
#define  MPI_INTR_MASK_DOORBELL		(1<<0) /* doorbell interrupt mask */

#define MPI_REQ_QUEUE		0x40

#define MPI_REPLY_QUEUE		0x44
#define  MPI_REPLY_QUEUE_ADDRESS	(1<<31) /* address reply */
#define  MPI_REPLY_QUEUE_ADDRESS_MASK	0x7fffffff
#define  MPI_REPLY_QUEUE_TYPE_MASK	(3<<29)
#define  MPI_REPLY_QUEUE_TYPE_INIT	(0<<29) /* scsi initiator reply */
#define  MPI_REPLY_QUEUE_TYPE_TARGET	(1<<29) /* scsi target reply */
#define  MPI_REPLY_QUEUE_TYPE_LAN	(2<<29) /* lan reply */
#define  MPI_REPLY_QUEUE_CONTEXT	0x1fffffff /* not address and type */

#define MPI_PRIREQ_QUEUE	0x48

/*
 * Scatter Gather Lists
 */

#define MPI_SGE_FL_LAST			(0x1<<31) /* last element in segment */
#define MPI_SGE_FL_EOB			(0x1<<30) /* last element of buffer */
#define MPI_SGE_FL_TYPE			(0x3<<28) /* element type */
#define  MPI_SGE_FL_TYPE_SIMPLE		(0x1<<28) /* simple element */
#define  MPI_SGE_FL_TYPE_CHAIN		(0x3<<28) /* chain element */
#define  MPI_SGE_FL_TYPE_XACTCTX	(0x0<<28) /* transaction context */
#define MPI_SGE_FL_LOCAL		(0x1<<27) /* local address */
#define MPI_SGE_FL_DIR			(0x1<<26) /* direction */
#define  MPI_SGE_FL_DIR_OUT		(0x1<<26)
#define  MPI_SGE_FL_DIR_IN		(0x0<<26)
#define MPI_SGE_FL_SIZE			(0x1<<25) /* address size */
#define  MPI_SGE_FL_SIZE_32		(0x0<<25) /* address size */
#define  MPI_SGE_FL_SIZE_64		(0x1<<25) /* address size */
#define MPI_SGE_FL_EOL			(0x1<<24) /* end of list */

struct mpi_sge32 {
	u_int32_t		sg_hdr;
	u_int32_t		sg_addr;
} __packed;

struct mpi_sge64 {
	u_int32_t		sg_hdr;
	u_int32_t		sg_loaddr;
	u_int32_t		sg_hiaddr;
} __packed;

/* XXX */
struct mpi_sge {
	u_int32_t		sg_hdr;
	u_int32_t		sg_lo_addr;
	u_int32_t		sg_hi_addr;
} __packed;

struct mpi_sgl_ce {
	u_int32_t		sg_hdr;
	u_int32_t		sg_loaddr;
	u_int32_t		sg_hiaddr;
} __packed;

struct mpi_sgl_tce {
	u_int32_t		sg_hdr;
	u_int32_t		sg_loaddr;
	u_int32_t		sg_hiaddr;
} __packed;

/*
 * Messages
 */

/* functions */
#define MPI_FUNCTION_SCSI_IO_REQUEST			(0x00)
#define MPI_FUNCTION_SCSI_TASK_MGMT			(0x01)
#define MPI_FUNCTION_IOC_INIT				(0x02)
#define MPI_FUNCTION_IOC_FACTS				(0x03)
#define MPI_FUNCTION_CONFIG				(0x04)
#define MPI_FUNCTION_PORT_FACTS				(0x05)
#define MPI_FUNCTION_PORT_ENABLE			(0x06)
#define MPI_FUNCTION_EVENT_NOTIFICATION			(0x07)
#define MPI_FUNCTION_EVENT_ACK				(0x08)
#define MPI_FUNCTION_FW_DOWNLOAD			(0x09)
#define MPI_FUNCTION_TARGET_CMD_BUFFER_POST		(0x0A)
#define MPI_FUNCTION_TARGET_ASSIST			(0x0B)
#define MPI_FUNCTION_TARGET_STATUS_SEND			(0x0C)
#define MPI_FUNCTION_TARGET_MODE_ABORT			(0x0D)
#define MPI_FUNCTION_TARGET_FC_BUF_POST_LINK_SRVC	(0x0E) /* obsolete */
#define MPI_FUNCTION_TARGET_FC_RSP_LINK_SRVC		(0x0F) /* obsolete */
#define MPI_FUNCTION_TARGET_FC_EX_SEND_LINK_SRVC	(0x10) /* obsolete */
#define MPI_FUNCTION_TARGET_FC_ABORT			(0x11) /* obsolete */
#define MPI_FUNCTION_FC_LINK_SRVC_BUF_POST		(0x0E)
#define MPI_FUNCTION_FC_LINK_SRVC_RSP			(0x0F)
#define MPI_FUNCTION_FC_EX_LINK_SRVC_SEND		(0x10)
#define MPI_FUNCTION_FC_ABORT				(0x11)
#define MPI_FUNCTION_FW_UPLOAD				(0x12)
#define MPI_FUNCTION_FC_COMMON_TRANSPORT_SEND		(0x13)
#define MPI_FUNCTION_FC_PRIMITIVE_SEND			(0x14)

#define MPI_FUNCTION_RAID_ACTION			(0x15)
#define MPI_FUNCTION_RAID_SCSI_IO_PASSTHROUGH		(0x16)

#define MPI_FUNCTION_TOOLBOX				(0x17)

#define MPI_FUNCTION_SCSI_ENCLOSURE_PROCESSOR		(0x18)

#define MPI_FUNCTION_MAILBOX				(0x19)

#define MPI_FUNCTION_LAN_SEND				(0x20)
#define MPI_FUNCTION_LAN_RECEIVE			(0x21)
#define MPI_FUNCTION_LAN_RESET				(0x22)

#define MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET		(0x40)
#define MPI_FUNCTION_IO_UNIT_RESET			(0x41)
#define MPI_FUNCTION_HANDSHAKE				(0x42)
#define MPI_FUNCTION_REPLY_FRAME_REMOVAL		(0x43)

/* reply flags */
#define MPI_REP_FLAGS_CONT		(1<<7) /* continuation reply */

#define MPI_REP_IOCSTATUS_AVAIL		(1<<15) /* logging info available */
#define MPI_REP_IOCSTATUS		(0x7fff) /* status */

/* Common IOCStatus values for all replies */
#define  MPI_IOCSTATUS_SUCCESS				(0x0000)
#define  MPI_IOCSTATUS_INVALID_FUNCTION			(0x0001)
#define  MPI_IOCSTATUS_BUSY				(0x0002)
#define  MPI_IOCSTATUS_INVALID_SGL			(0x0003)
#define  MPI_IOCSTATUS_INTERNAL_ERROR			(0x0004)
#define  MPI_IOCSTATUS_RESERVED				(0x0005)
#define  MPI_IOCSTATUS_INSUFFICIENT_RESOURCES		(0x0006)
#define  MPI_IOCSTATUS_INVALID_FIELD			(0x0007)
#define  MPI_IOCSTATUS_INVALID_STATE			(0x0008)
#define  MPI_IOCSTATUS_OP_STATE_NOT_SUPPORTED		(0x0009)
/* Config IOCStatus values */
#define  MPI_IOCSTATUS_CONFIG_INVALID_ACTION		(0x0020)
#define  MPI_IOCSTATUS_CONFIG_INVALID_TYPE		(0x0021)
#define  MPI_IOCSTATUS_CONFIG_INVALID_PAGE		(0x0022)
#define  MPI_IOCSTATUS_CONFIG_INVALID_DATA		(0x0023)
#define  MPI_IOCSTATUS_CONFIG_NO_DEFAULTS		(0x0024)
#define  MPI_IOCSTATUS_CONFIG_CANT_COMMIT		(0x0025)
/* SCSIIO Reply (SPI & FCP) initiator values */
#define  MPI_IOCSTATUS_SCSI_RECOVERED_ERROR		(0x0040)
#define  MPI_IOCSTATUS_SCSI_INVALID_BUS			(0x0041)
#define  MPI_IOCSTATUS_SCSI_INVALID_TARGETID		(0x0042)
#define  MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE		(0x0043)
#define  MPI_IOCSTATUS_SCSI_DATA_OVERRUN		(0x0044)
#define  MPI_IOCSTATUS_SCSI_DATA_UNDERRUN		(0x0045)
#define  MPI_IOCSTATUS_SCSI_IO_DATA_ERROR		(0x0046)
#define  MPI_IOCSTATUS_SCSI_PROTOCOL_ERROR		(0x0047)
#define  MPI_IOCSTATUS_SCSI_TASK_TERMINATED		(0x0048)
#define  MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH		(0x0049)
#define  MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED		(0x004A)
#define  MPI_IOCSTATUS_SCSI_IOC_TERMINATED		(0x004B)
#define  MPI_IOCSTATUS_SCSI_EXT_TERMINATED		(0x004C)
/* For use by SCSI Initiator and SCSI Target end-to-end data protection */
#define  MPI_IOCSTATUS_EEDP_GUARD_ERROR			(0x004D)
#define  MPI_IOCSTATUS_EEDP_REF_TAG_ERROR		(0x004E)
#define  MPI_IOCSTATUS_EEDP_APP_TAG_ERROR		(0x004F)
/* SCSI (SPI & FCP) target values */
#define  MPI_IOCSTATUS_TARGET_PRIORITY_IO		(0x0060)
#define  MPI_IOCSTATUS_TARGET_INVALID_PORT		(0x0061)
#define  MPI_IOCSTATUS_TARGET_INVALID_IOCINDEX		(0x0062) /* obsolete */
#define  MPI_IOCSTATUS_TARGET_INVALID_IO_INDEX		(0x0062)
#define  MPI_IOCSTATUS_TARGET_ABORTED			(0x0063)
#define  MPI_IOCSTATUS_TARGET_NO_CONN_RETRYABLE		(0x0064)
#define  MPI_IOCSTATUS_TARGET_NO_CONNECTION		(0x0065)
#define  MPI_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH	(0x006A)
#define  MPI_IOCSTATUS_TARGET_STS_DATA_NOT_SENT		(0x006B)
#define  MPI_IOCSTATUS_TARGET_DATA_OFFSET_ERROR		(0x006D)
#define  MPI_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA	(0x006E)
#define  MPI_IOCSTATUS_TARGET_IU_TOO_SHORT		(0x006F)
/* Additional FCP target values */
#define  MPI_IOCSTATUS_TARGET_FC_ABORTED		(0x0066) /* obsolete */
#define  MPI_IOCSTATUS_TARGET_FC_RX_ID_INVALID		(0x0067) /* obsolete */
#define  MPI_IOCSTATUS_TARGET_FC_DID_INVALID		(0x0068) /* obsolete */
#define  MPI_IOCSTATUS_TARGET_FC_NODE_LOGGED_OUT	(0x0069) /* obsolete */
/* Fibre Channel Direct Access values */
#define  MPI_IOCSTATUS_FC_ABORTED			(0x0066)
#define  MPI_IOCSTATUS_FC_RX_ID_INVALID			(0x0067)
#define  MPI_IOCSTATUS_FC_DID_INVALID			(0x0068)
#define  MPI_IOCSTATUS_FC_NODE_LOGGED_OUT		(0x0069)
#define  MPI_IOCSTATUS_FC_EXCHANGE_CANCELED		(0x006C)
/* LAN values */
#define  MPI_IOCSTATUS_LAN_DEVICE_NOT_FOUND		(0x0080)
#define  MPI_IOCSTATUS_LAN_DEVICE_FAILURE		(0x0081)
#define  MPI_IOCSTATUS_LAN_TRANSMIT_ERROR		(0x0082)
#define  MPI_IOCSTATUS_LAN_TRANSMIT_ABORTED		(0x0083)
#define  MPI_IOCSTATUS_LAN_RECEIVE_ERROR		(0x0084)
#define  MPI_IOCSTATUS_LAN_RECEIVE_ABORTED		(0x0085)
#define  MPI_IOCSTATUS_LAN_PARTIAL_PACKET		(0x0086)
#define  MPI_IOCSTATUS_LAN_CANCELED			(0x0087)
/* Serial Attached SCSI values */
#define  MPI_IOCSTATUS_SAS_SMP_REQUEST_FAILED		(0x0090)
#define  MPI_IOCSTATUS_SAS_SMP_DATA_OVERRUN		(0x0091)
/* Inband values */
#define  MPI_IOCSTATUS_INBAND_ABORTED			(0x0098)
#define  MPI_IOCSTATUS_INBAND_NO_CONNECTION		(0x0099)
/* Diagnostic Tools values */
#define  MPI_IOCSTATUS_DIAGNOSTIC_RELEASED		(0x00A0)

#define MPI_REP_IOCLOGINFO_TYPE		(0xf<<28) /* logging info type */
#define MPI_REP_IOCLOGINFO_TYPE_NONE	(0x0<<28)
#define MPI_REP_IOCLOGINFO_TYPE_SCSI	(0x1<<28)
#define MPI_REP_IOCLOGINFO_TYPE_FC	(0x2<<28)
#define MPI_REP_IOCLOGINFO_TYPE_SAS	(0x3<<28)
#define MPI_REP_IOCLOGINFO_TYPE_ISCSI	(0x4<<28)
#define MPI_REP_IOCLOGINFO_DATA		(0x0fffffff) /* logging info data */

/* messages */

#define MPI_WHOINIT_NOONE		0x00
#define MPI_WHOINIT_SYSTEM_BIOS		0x01
#define MPI_WHOINIT_ROM_BIOS		0x02
#define MPI_WHOINIT_PCI_PEER		0x03
#define MPI_WHOINIT_HOST_DRIVER		0x04
#define MPI_WHOINIT_MANUFACTURER	0x05

/* default messages */

struct mpi_msg_request {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		reserved5;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;
} __packed;

struct mpi_msg_reply {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		reserved5;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int8_t		reserved6;
	u_int8_t		reserved7;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

/* ioc init */

struct mpi_msg_iocinit_request {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		flags;
#define MPI_IOCINIT_F_DISCARD_FW			(1<<0)
#define MPI_IOCINIT_F_ENABLE_HOST_FIFO			(1<<1)
#define MPI_IOCINIT_F_HOST_PG_BUF_PERSIST		(1<<2)
	u_int8_t		max_devices;
	u_int8_t		max_buses;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reply_frame_size;
	u_int16_t		reserved2;

	u_int32_t		host_mfa_hi_addr;

	u_int32_t		sense_buffer_hi_addr;

	u_int32_t		reply_fifo_host_signalling_addr;

	struct mpi_sge		host_page_buffer_sge;

	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;

	u_int8_t		hdr_version_unit;
	u_int8_t		hdr_version_dev;
} __packed;

struct mpi_msg_iocinit_reply {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		flags;
	u_int8_t		max_devices;
	u_int8_t		max_buses;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;


/* ioc facts */
struct mpi_msg_iocfacts_request {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		reserved5;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;
} __packed;

struct mpi_msg_iocfacts_reply {
	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		header_version_min;
	u_int8_t		header_version_maj;
	u_int8_t		ioc_number;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		ioc_exceptions;
#define MPI_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL	(1<<0)
#define MPI_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID		(1<<1)
#define MPI_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL		(1<<2)
#define MPI_IOCFACTS_EXCEPT_PERSISTENT_TABLE_FULL	(1<<3)
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		max_chain_depth;
	u_int8_t		whoinit;
	u_int8_t		block_size;
	u_int8_t		flags;
#define MPI_IOCFACTS_FLAGS_FW_DOWNLOAD_BOOT		(1<<0)
#define MPI_IOCFACTS_FLAGS_REPLY_FIFO_HOST_SIGNAL	(1<<1)
#define MPI_IOCFACTS_FLAGS_HOST_PAGE_BUFFER_PERSISTENT	(1<<2)

	u_int16_t		reply_queue_depth;
	u_int16_t		request_frame_size;

	u_int16_t		reserved1;
	u_int16_t		product_id;	/* product id */

	u_int32_t		current_host_mfa_hi_addr;

	u_int16_t		global_credits;
	u_int8_t		number_of_ports;
	u_int8_t		event_state;

	u_int32_t		current_sense_buffer_hi_addr;

	u_int16_t		current_reply_frame_size;
	u_int8_t		max_devices;
	u_int8_t		max_buses;

	u_int32_t		fw_image_size;

	u_int32_t		ioc_capabilities;
#define MPI_IOCFACTS_CAPABILITY_HIGH_PRI_Q		(1<<0)
#define MPI_IOCFACTS_CAPABILITY_REPLY_HOST_SIGNAL	(1<<1)
#define MPI_IOCFACTS_CAPABILITY_QUEUE_FULL_HANDLING	(1<<2)
#define MPI_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER	(1<<3)
#define MPI_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER		(1<<4)
#define MPI_IOCFACTS_CAPABILITY_EXTENDED_BUFFER		(1<<5)
#define MPI_IOCFACTS_CAPABILITY_EEDP			(1<<6)
#define MPI_IOCFACTS_CAPABILITY_BIDIRECTIONAL		(1<<7)
#define MPI_IOCFACTS_CAPABILITY_MULTICAST		(1<<8)
#define MPI_IOCFACTS_CAPABILITY_SCSIIO32		(1<<9)
#define MPI_IOCFACTS_CAPABILITY_NO_SCSIIO16		(1<<10)

	u_int8_t		fw_version_dev;
	u_int8_t		fw_version_unit;
	u_int8_t		fw_version_min;
	u_int8_t		fw_version_maj;

	u_int16_t		hi_priority_queue_depth;
	u_int16_t		reserved2;

	struct mpi_sge		host_page_buffer_sge;

	u_int32_t		reply_fifo_host_signalling_addr;
} __packed;

struct mpi_msg_portfacts_request {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

} __packed;

struct mpi_msg_portfacts_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		reserved4;
	u_int8_t		port_type;
#define MPI_PORTFACTS_PORTTYPE_INACTIVE			0x00
#define MPI_PORTFACTS_PORTTYPE_SCSI			0x01
#define MPI_PORTFACTS_PORTTYPE_FC			0x10
#define MPI_PORTFACTS_PORTTYPE_ISCSI			0x20
#define MPI_PORTFACTS_PORTTYPE_SAS			0x30

	u_int16_t		max_devices;

	u_int16_t		port_scsi_id;
	u_int16_t		protocol_flags;
#define MPI_PORTFACTS_PROTOCOL_LOGBUSADDR		(1<<0)
#define MPI_PORTFACTS_PROTOCOL_LAN			(1<<1)
#define MPI_PORTFACTS_PROTOCOL_TARGET			(1<<2)
#define MPI_PORTFACTS_PROTOCOL_INITIATOR		(1<<3)

	u_int16_t		max_posted_cmd_buffers;
	u_int16_t		max_persistent_ids;

	u_int16_t		max_lan_buckets;
	u_int16_t		reserved5;

	u_int32_t		reserved6;
} __packed;

struct mpi_msg_portenable_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;
} __packed;

struct mpi_msg_portenable_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpi_msg_event_request {
	u_int8_t		ev_switch;
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int32_t		msg_context;
} __packed;

struct mpi_msg_event_reply {
	u_int16_t		data_length;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		ack_required;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		event;

	u_int32_t		event_context;

	/* event data follows */
} __packed;

struct mpi_msg_scsi_io {
	u_int8_t		target_id;
	u_int8_t		bus;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		cdb_length;
	u_int8_t		sense_buf_len;
	u_int8_t		reserved;
	u_int8_t		msg_flags;
#define MPI_SCSIIO_EEDP					0xf0
#define MPI_SCSIIO_CMD_DATA_DIR				(1<<2)
#define MPI_SCSIIO_SENSE_BUF_LOC			(1<<1)
#define MPI_SCSIIO_SENSE_BUF_ADDR_WIDTH			(1<<0)
#define  MPI_SCSIIO_SENSE_BUF_ADDR_WIDTH_32		(0<<0)
#define  MPI_SCSIIO_SENSE_BUF_ADDR_WIDTH_64		(1<<0)

	u_int32_t		msg_context;

	u_int16_t		lun[4];

	u_int32_t		control;
#define MPI_SCSIIO_ADDITIONAL_CDB_LEN			(0xf<<26)
#define MPI_SCSIIO_DATA_DIR				(0x3<<24)
#define MPI_SCSIIO_DATA_DIR_NONE			(0x0<<24)
#define MPI_SCSIIO_DATA_DIR_WRITE			(0x1<<24)
#define MPI_SCSIIO_DATA_DIR_READ			(0x2<<24)
#define MPI_SCSIIO_TASK_ATTR				(0x7<<8)
#define MPI_SCSIIO_TASK_ATTR_SIMPLE_Q			(0x0<<8)
#define MPI_SCSIIO_TASK_ATTR_HEAD_OF_Q			(0x1<<8)
#define MPI_SCSIIO_TASK_ATTR_ORDERED_Q			(0x2<<8)
#define MPI_SCSIIO_TASK_ATTR_ACA_Q			(0x4<<8)
#define MPI_SCSIIO_TASK_ATTR_UNTAGGED			(0x5<<8)
#define MPI_SCSIIO_TASK_ATTR_NO_DISCONNECT		(0x7<<8)

#define MPI_CDB_LEN					16
	u_int8_t		cdb[MPI_CDB_LEN];

	u_int32_t		data_length;

	u_int32_t		sense_buf_low_addr;

	/* followed by an sgl */
} __packed;

struct mpi_msg_scsi_io_error {
	u_int8_t		target_id;
	u_int8_t		bus;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		cdb_length;
	u_int8_t		sense_buf_len;
	u_int8_t		reserved1;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int8_t		scsi_status;
#if notyet
#define MPI_SCSIIO_ERR_STATUS_SUCCESS
#define MPI_SCSIIO_ERR_STATUS_CHECK_COND
#define MPI_SCSIIO_ERR_STATUS_BUSY
#define MPI_SCSIIO_ERR_STATUS_INTERMEDIATE
#define MPI_SCSIIO_ERR_STATUS_INTERMEDIATE_CONDMET
#define MPI_SCSIIO_ERR_STATUS_RESERVATION_CONFLICT
#define MPI_SCSIIO_ERR_STATUS_CMD_TERM
#define MPI_SCSIIO_ERR_STATUS_TASK_SET_FULL
#define MPI_SCSIIO_ERR_STATUS_ACA_ACTIVE
#endif
	u_int8_t		scsi_state;
#define MPI_SCSIIO_ERR_STATE_AUTOSENSE_VALID		(1<<0)
#define MPI_SCSIIO_ERR_STATE_AUTOSENSE_FAILED		(1<<2)
#define MPI_SCSIIO_ERR_STATE_NO_SCSI_STATUS		(1<<3)
#define MPI_SCSIIO_ERR_STATE_TERMINATED			(1<<4)
#define MPI_SCSIIO_ERR_STATE_RESPONSE_INFO_VALID	(1<<5)
#define MPI_SCSIIO_ERR_STATE_QUEUE_TAG_REJECTED		(1<<6)

	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		transfer_count;

	u_int32_t		sense_count;

	u_int32_t		response_info;

	u_int16_t		tag;
	u_int16_t		reserved2;
} __packed;

struct mpi_msg_scsi_task_request {
	u_int8_t		target_id;
	u_int8_t		bus;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved1;
	u_int8_t		task_type;
#define MPI_MSG_SCSI_TASK_TYPE_ABORT_TASK		(0x01)
#define MPI_MSG_SCSI_TASK_TYPE_ABRT_TASK_SET		(0x02)
#define MPI_MSG_SCSI_TASK_TYPE_TARGET_RESET		(0x03)
#define MPI_MSG_SCSI_TASK_TYPE_RESET_BUS		(0x04)
#define MPI_MSG_SCSI_TASK_TYPE_LOGICAL_UNIT_RESET	(0x05)
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		lun[4];

	u_int32_t		reserved3[7]; /* wtf? */

	u_int32_t		target_msg_context;
} __packed;

struct mpi_msg_scsi_task_reply {
	u_int8_t		target_id;
	u_int8_t		bus;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		response_code;
	u_int8_t		task_type;
	u_int8_t		reserved1;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		termination_count;
} __packed;

struct mpi_cfg_hdr {
	u_int8_t		page_version;
	u_int8_t		page_length;
	u_int8_t		page_number;
	u_int8_t		page_type;
#define MPI_CONFIG_REQ_PAGE_TYPE_ATTRIBUTE		(0xf0)
#define MPI_CONFIG_REQ_PAGE_TYPE_MASK			(0x0f)
#define MPI_CONFIG_REQ_PAGE_TYPE_IO_UNIT		(0x00)
#define MPI_CONFIG_REQ_PAGE_TYPE_IOC			(0x01)
#define MPI_CONFIG_REQ_PAGE_TYPE_BIOS			(0x02)
#define MPI_CONFIG_REQ_PAGE_TYPE_SCSI_SPI_PORT		(0x03)
#define MPI_CONFIG_REQ_PAGE_TYPE_SCSI_SPI_DEV		(0x04)
#define MPI_CONFIG_REQ_PAGE_TYPE_FC_PORT		(0x05)
#define MPI_CONFIG_REQ_PAGE_TYPE_FC_DEV			(0x06)
#define MPI_CONFIG_REQ_PAGE_TYPE_LAN			(0x07)
#define MPI_CONFIG_REQ_PAGE_TYPE_RAID_VOL		(0x08)
#define MPI_CONFIG_REQ_PAGE_TYPE_MANUFACTURING		(0x09)
#define MPI_CONFIG_REQ_PAGE_TYPE_RAID_PD		(0x0A)
#define MPI_CONFIG_REQ_PAGE_TYPE_INBAND			(0x0B)
#define MPI_CONFIG_REQ_PAGE_TYPE_EXTENDED		(0x0F)
} __packed;

struct mpi_msg_config_request {
	u_int8_t		action;
#define MPI_CONFIG_REQ_ACTION_PAGE_HEADER		(0x00)
#define MPI_CONFIG_REQ_ACTION_PAGE_READ_CURRENT		(0x01)
#define MPI_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT	(0x02)
#define MPI_CONFIG_REQ_ACTION_PAGE_DEFAULT		(0x03)
#define MPI_CONFIG_REQ_ACTION_PAGE_WRITE_NVRAM		(0x04)
#define MPI_CONFIG_REQ_ACTION_PAGE_READ_DEFAULT		(0x05)
#define MPI_CONFIG_REQ_ACTION_PAGE_READ_NVRAM		(0x06)
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		ext_page_len;
	u_int8_t		ext_page_type;
#define MPI_CONFIG_REQ_EXTPAGE_TYPE_SAS_IO_UNIT		(0x10)
#define MPI_CONFIG_REQ_EXTPAGE_TYPE_SAS_EXPANDER	(0x11)
#define MPI_CONFIG_REQ_EXTPAGE_TYPE_SAS_DEVICE		(0x12)
#define MPI_CONFIG_REQ_EXTPAGE_TYPE_SAS_PHY		(0x13)
#define MPI_CONFIG_REQ_EXTPAGE_TYPE_LOG			(0x14)
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int32_t		reserved2[2];

	struct mpi_cfg_hdr	config_header;

	u_int32_t		page_address;
/* XXX lots of defns here */

	struct mpi_sge		page_buffer;
} __packed;

struct mpi_msg_config_reply {
	u_int8_t		action;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		ext_page_length;
	u_int8_t		ext_page_type;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	struct mpi_cfg_hdr	config_header;
} __packed;

struct mpi_cfg_spi_port_pg0 {
	struct mpi_cfg_hdr	config_header;

	u_int32_t		capabilities;
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_PACKETIZED	(1<<0)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_DT		(1<<1)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_QAS		(1<<2)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_MIN_STP		(0x0000ff00)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_MAX_STP		(0x00ff0000)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_IDP		(1<<27)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_WIDTH		(1<<29)
#define  MPI_CFG_SPI_PORT_0_CAPABILITIES_WIDTH_NARROW	(0<<29)
#define  MPI_CFG_SPI_PORT_0_CAPABILITIES_WIDTH_WIDE	(1<<29)
#define MPI_CFG_SPI_PORT_0_CAPABILITIES_AIP		(1<<31)

        u_int32_t               physical_interface;
#define MPI_CFG_SPI_PORT_0_PHYS_SIGNAL			(0x3<<0)
#define  MPI_CFG_SPI_PORT_0_PHYS_SIGNAL_HVD		(0x1<<0)
#define  MPI_CFG_SPI_PORT_0_PHYS_SIGNAL_SE		(0x2<<0)
#define  MPI_CFG_SPI_PORT_0_PHYS_SIGNAL_LVD		(0x3<<0)
#define MPI_CFG_SPI_PORT_0_PHYS_CONNECTEDID		(0xff<<24)
#define  MPI_CFG_SPI_PORT_0_PHYS_CONNECTEDID_BUSFREE	(0xfe<<24)
#define  MPI_CFG_SPI_PORT_0_PHYS_CONNECTEDID_UNKNOWN	(0xff<<24)
} __packed;

struct mpi_cfg_manufacturing_pg0 {
	struct mpi_cfg_hdr	config_header;

	char			chip_name[16];
	char			chip_revision[8];
	char			board_name[16];
	char			board_assembly[16];
	char			board_tracer_number[16];
} __packed;
