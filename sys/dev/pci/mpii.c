/* $OpenBSD: mpii.c,v 1.5 2009/12/01 00:09:03 bluhm Exp $ */
/*
 * Copyright (c) James Giannoules
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
 *
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/rwlock.h>
#include <sys/sensors.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#endif

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/biovar.h>

#define MPII_DOORBELL			(0x00)
/* doorbell read bits */
#define MPII_DOORBELL_STATE		(0xf<<28) /* ioc state */
#define  MPII_DOORBELL_STATE_RESET	(0x0<<28)
#define  MPII_DOORBELL_STATE_READY	(0x1<<28)
#define  MPII_DOORBELL_STATE_OPER	(0x2<<28)
#define  MPII_DOORBELL_STATE_FAULT	(0x4<<28)
#define  MPII_DOORBELL_INUSE		(0x1<<27) /* doorbell used */
#define MPII_DOORBELL_WHOINIT		(0x7<<24) /* last to reset ioc */
#define  MPII_DOORBELL_WHOINIT_NOONE	(0x0<<24) /* not initialized */
#define  MPII_DOORBELL_WHOINIT_SYSBIOS	(0x1<<24) /* system bios */
#define  MPII_DOORBELL_WHOINIT_ROMBIOS	(0x2<<24) /* rom bios */
#define  MPII_DOORBELL_WHOINIT_PCIPEER	(0x3<<24) /* pci peer */
#define  MPII_DOORBELL_WHOINIT_DRIVER	(0x4<<24) /* host driver */
#define  MPII_DOORBELL_WHOINIT_MANUFACT	(0x5<<24) /* manufacturing */
#define MPII_DOORBELL_FAULT		(0xffff<<0) /* fault code */
/* doorbell write bits */
#define MPII_DOORBELL_FUNCTION_SHIFT	(24)
#define MPII_DOORBELL_FUNCTION_MASK	(0xff << MPII_DOORBELL_FUNCTION_SHIFT)
#define MPII_DOORBELL_FUNCTION(x)	\
    (((x) << MPII_DOORBELL_FUNCTION_SHIFT) & MPII_DOORBELL_FUNCTION_MASK)
#define MPII_DOORBELL_DWORDS_SHIFT	16
#define MPII_DOORBELL_DWORDS_MASK	(0xff << MPII_DOORBELL_DWORDS_SHIFT)
#define MPII_DOORBELL_DWORDS(x)		\
    (((x) << MPII_DOORBELL_DWORDS_SHIFT) & MPII_DOORBELL_DWORDS_MASK)
#define MPII_DOORBELL_DATA_MASK		(0xffff)

#define MPII_WRITESEQ			(0x04)
#define  MPII_WRITESEQ_KEY_VALUE_MASK	(0x0000000f) /* key value */
#define  MPII_WRITESEQ_FLUSH		(0x00)
#define  MPII_WRITESEQ_1		(0x0f)
#define  MPII_WRITESEQ_2		(0x04)
#define  MPII_WRITESEQ_3		(0x0b)
#define  MPII_WRITESEQ_4		(0x02)
#define  MPII_WRITESEQ_5		(0x07)
#define	 MPII_WRITESEQ_6		(0x0d)	

#define MPII_HOSTDIAG			(0x08)
#define  MPII_HOSTDIAG_BDS_MASK		(0x00001800) /* boot device select */
#define   MPII_HOSTDIAG_BDS_DEFAULT 	(0<<11)	/* default address map, flash */
#define   MPII_HOSTDIAG_BDS_HCDW	(1<<11)	/* host code and data window */
#define  MPII_HOSTDIAG_CLEARFBS		(1<<10) /* clear flash bad sig */
#define  MPII_HOSTDIAG_FORCE_HCB_ONBOOT (1<<9)	/* force host controlled boot */
#define  MPII_HOSTDIAG_HCB_MODE		(1<<8)	/* host controlled boot mode */
#define  MPII_HOSTDIAG_DWRE		(1<<7) 	/* diag reg write enabled */
#define  MPII_HOSTDIAG_FBS		(1<<6) 	/* flash bad sig */
#define  MPII_HOSTDIAG_RESET_HIST	(1<<5) 	/* reset history */
#define  MPII_HOSTDIAG_DIAGWR_EN	(1<<4) 	/* diagnostic write enabled */
#define  MPII_HOSTDIAG_RESET_ADAPTER	(1<<2) 	/* reset adapter */
#define  MPII_HOSTDIAG_HOLD_IOC_RESET	(1<<1) 	/* hold ioc in reset */
#define  MPII_HOSTDIAG_DIAGMEM_EN	(1<<0) 	/* diag mem enable */

#define MPII_DIAGRWDATA			(0x10)

#define MPII_DIAGRWADDRLOW		(0x14)

#define MPII_DIAGRWADDRHIGH		(0x18)

#define MPII_INTR_STATUS		(0x30)
#define  MPII_INTR_STATUS_SYS2IOCDB	(1<<31) /* ioc written to by host */
#define  MPII_INTR_STATUS_RESET		(1<<30) /* physical ioc reset */
#define  MPII_INTR_STATUS_REPLY		(1<<3)	/* reply message interrupt */
#define  MPII_INTR_STATUS_IOC2SYSDB	(1<<0) 	/* ioc write to doorbell */

#define MPII_INTR_MASK			(0x34)
#define  MPII_INTR_MASK_RESET		(1<<30) /* ioc reset intr mask */
#define  MPII_INTR_MASK_REPLY		(1<<3) 	/* reply message intr mask */
#define  MPII_INTR_MASK_DOORBELL	(1<<0) 	/* doorbell interrupt mask */

#define MPII_DCR_DATA			(0x38)

#define MPII_DCR_ADDRESS		(0x3c)

#define MPII_REPLY_FREE_HOST_INDEX	(0x48)

#define MPII_REPLY_POST_HOST_INDEX	(0x6c)

#define MPII_HCB_SIZE			(0x74)

#define MPII_HCB_ADDRESS_LOW		(0x78)
#define MPII_HCB_ADDRESS_HIGH		(0x7c)

#define MPII_REQ_DESC_POST_LOW		(0xc0)
#define MPII_REQ_DESC_POST_HIGH		(0xc4)

/*
 * Scatter Gather Lists
 */

#define MPII_SGE_FL_LAST		(0x1<<31) /* last element in segment */
#define MPII_SGE_FL_EOB			(0x1<<30) /* last element of buffer */
#define MPII_SGE_FL_TYPE		(0x3<<28) /* element type */
 #define MPII_SGE_FL_TYPE_SIMPLE	(0x1<<28) /* simple element */
 #define MPII_SGE_FL_TYPE_CHAIN		(0x3<<28) /* chain element */
 #define MPII_SGE_FL_TYPE_XACTCTX	(0x0<<28) /* transaction context */
#define MPII_SGE_FL_LOCAL		(0x1<<27) /* local address */
#define MPII_SGE_FL_DIR			(0x1<<26) /* direction */
 #define MPII_SGE_FL_DIR_OUT		(0x1<<26)
 #define MPII_SGE_FL_DIR_IN		(0x0<<26)
#define MPII_SGE_FL_SIZE		(0x1<<25) /* address size */
 #define MPII_SGE_FL_SIZE_32		(0x0<<25)
 #define MPII_SGE_FL_SIZE_64		(0x1<<25)
#define MPII_SGE_FL_EOL			(0x1<<24) /* end of list */

struct mpii_sge {
	u_int32_t		sg_hdr;
	u_int32_t		sg_lo_addr;
	u_int32_t		sg_hi_addr;
} __packed;

struct mpii_fw_tce {
	u_int8_t		reserved1;
	u_int8_t		context_size;
	u_int8_t		details_length;
	u_int8_t		flags;

	u_int32_t		reserved2;

	u_int32_t		image_offset;

	u_int32_t		image_size;
} __packed;

/*
 * Messages
 */

/* functions */
#define MPII_FUNCTION_SCSI_IO_REQUEST			(0x00)
#define MPII_FUNCTION_SCSI_TASK_MGMT			(0x01)
#define MPII_FUNCTION_IOC_INIT				(0x02)
#define MPII_FUNCTION_IOC_FACTS				(0x03)
#define MPII_FUNCTION_CONFIG				(0x04)
#define MPII_FUNCTION_PORT_FACTS			(0x05)
#define MPII_FUNCTION_PORT_ENABLE			(0x06)
#define MPII_FUNCTION_EVENT_NOTIFICATION		(0x07)
#define MPII_FUNCTION_EVENT_ACK				(0x08)
#define MPII_FUNCTION_FW_DOWNLOAD			(0x09)
#define MPII_FUNCTION_TARGET_CMD_BUFFER_POST		(0x0a)
#define MPII_FUNCTION_TARGET_ASSIST			(0x0b)
#define MPII_FUNCTION_TARGET_STATUS_SEND		(0x0c)
#define MPII_FUNCTION_TARGET_MODE_ABORT			(0x0d)
#define MPII_FUNCTION_FW_UPLOAD				(0x12)

#define MPII_FUNCTION_RAID_ACTION			(0x15)
#define MPII_FUNCTION_RAID_SCSI_IO_PASSTHROUGH		(0x16)

#define MPII_FUNCTION_TOOLBOX				(0x17)

#define MPII_FUNCTION_SCSI_ENCLOSURE_PROCESSOR		(0x18)

#define MPII_FUNCTION_SMP_PASSTHROUGH			(0x1a)
#define MPII_FUNCTION_SAS_IO_UNIT_CONTROL		(0x1b)
#define MPII_FUNCTION_SATA_PASSTHROUGH			(0x1c)

#define MPII_FUNCTION_DIAG_BUFFER_POST			(0x1d)
#define MPII_FUNCTION_DIAG_RELEASE			(0x1e)

#define MPII_FUNCTION_TARGET_CMD_BUF_BASE_POST		(0x24)
#define MPII_FUNCTION_TARGET_CMD_BUF_LIST_POST		(0x25)

#define MPII_FUNCTION_IOC_MESSAGE_UNIT_RESET		(0x40)
#define MPII_FUNCTION_IO_UNIT_RESET			(0x41)
#define MPII_FUNCTION_HANDSHAKE				(0x42)

/* request flags fields for request descriptors */
#define MPII_REQ_DESCRIPTOR_FLAGS_TYPE_SHIFT		(0x01)
#define MPII_REQ_DESCRIPTOR_FLAGS_TYPE_MASK		(0x07 << \
					MPII_REQ_DESCRIPTOR_FLAGS_TYPE_SHIFT)
#define MPII_REQ_DESCRIPTOR_FLAGS_SCSI_IO		(0x00)
#define MPII_REQ_DESCRIPTOR_FLAGS_SCSI_TARGET		(0x02)
#define MPII_REQ_DESCRIPTOR_FLAGS_HIGH_PRIORITY		(0x06)
#define MPII_REQ_DESCRIPTOR_FLAGS_DEFAULT		(0x08)

#define MPII_REQ_DESCRIPTOR_FLAGS_IOC_FIFO_MARKER	(0x01)

/* reply flags */
#define MPII_REP_FLAGS_CONT			(1<<7) /* continuation reply */

#define MPII_REP_IOCSTATUS_AVAIL		(1<<15) /* log info available */
#define MPII_REP_IOCSTATUS			(0x7fff) /* status */

/* Common IOCStatus values for all replies */
#define  MPII_IOCSTATUS_SUCCESS				(0x0000)
#define  MPII_IOCSTATUS_INVALID_FUNCTION		(0x0001)
#define  MPII_IOCSTATUS_BUSY				(0x0002)
#define  MPII_IOCSTATUS_INVALID_SGL			(0x0003)
#define  MPII_IOCSTATUS_INTERNAL_ERROR			(0x0004)
#define  MPII_IOCSTATUS_INVALID_VPID			(0x0005)
#define  MPII_IOCSTATUS_INSUFFICIENT_RESOURCES		(0x0006)
#define  MPII_IOCSTATUS_INVALID_FIELD			(0x0007)
#define  MPII_IOCSTATUS_INVALID_STATE			(0x0008)
#define  MPII_IOCSTATUS_OP_STATE_NOT_SUPPORTED		(0x0009)
/* Config IOCStatus values */
#define  MPII_IOCSTATUS_CONFIG_INVALID_ACTION		(0x0020)
#define  MPII_IOCSTATUS_CONFIG_INVALID_TYPE		(0x0021)
#define  MPII_IOCSTATUS_CONFIG_INVALID_PAGE		(0x0022)
#define  MPII_IOCSTATUS_CONFIG_INVALID_DATA		(0x0023)
#define  MPII_IOCSTATUS_CONFIG_NO_DEFAULTS		(0x0024)
#define  MPII_IOCSTATUS_CONFIG_CANT_COMMIT		(0x0025)
/* SCSIIO Reply initiator values */
#define  MPII_IOCSTATUS_SCSI_RECOVERED_ERROR		(0x0040)
#define  MPII_IOCSTATUS_SCSI_INVALID_DEVHANDLE		(0x0042)
#define  MPII_IOCSTATUS_SCSI_DEVICE_NOT_THERE		(0x0043)
#define  MPII_IOCSTATUS_SCSI_DATA_OVERRUN		(0x0044)
#define  MPII_IOCSTATUS_SCSI_DATA_UNDERRUN		(0x0045)
#define  MPII_IOCSTATUS_SCSI_IO_DATA_ERROR		(0x0046)
#define  MPII_IOCSTATUS_SCSI_PROTOCOL_ERROR		(0x0047)
#define  MPII_IOCSTATUS_SCSI_TASK_TERMINATED		(0x0048)
#define  MPII_IOCSTATUS_SCSI_RESIDUAL_MISMATCH		(0x0049)
#define  MPII_IOCSTATUS_SCSI_TASK_MGMT_FAILED		(0x004a)
#define  MPII_IOCSTATUS_SCSI_IOC_TERMINATED		(0x004b)
#define  MPII_IOCSTATUS_SCSI_EXT_TERMINATED		(0x004c)
/* For use by SCSI Initiator and SCSI Target end-to-end data protection */
#define  MPII_IOCSTATUS_EEDP_GUARD_ERROR		(0x004d)
#define  MPII_IOCSTATUS_EEDP_REF_TAG_ERROR		(0x004e)
#define  MPII_IOCSTATUS_EEDP_APP_TAG_ERROR		(0x004f)
/* SCSI (SPI & FCP) target values */
#define  MPII_IOCSTATUS_TARGET_INVALID_IO_INDEX		(0x0062)
#define  MPII_IOCSTATUS_TARGET_ABORTED			(0x0063)
#define  MPII_IOCSTATUS_TARGET_NO_CONN_RETRYABLE	(0x0064)
#define  MPII_IOCSTATUS_TARGET_NO_CONNECTION		(0x0065)
#define  MPII_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH	(0x006a)
#define  MPII_IOCSTATUS_TARGET_DATA_OFFSET_ERROR	(0x006d)
#define  MPII_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA	(0x006e)
#define  MPII_IOCSTATUS_TARGET_IU_TOO_SHORT		(0x006f)
#define  MPII_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT		(0x0070)
#define  MPII_IOCSTATUS_TARGET_NAK_RECEIVED		(0x0071)
/* Serial Attached SCSI values */
#define  MPII_IOCSTATUS_SAS_SMP_REQUEST_FAILED		(0x0090)
#define  MPII_IOCSTATUS_SAS_SMP_DATA_OVERRUN		(0x0091)
/* Diagnostic Tools values */
#define  MPII_IOCSTATUS_DIAGNOSTIC_RELEASED		(0x00a0)

#define MPII_REP_IOCLOGINFO_TYPE			(0xf<<28) /* log info type */
#define MPII_REP_IOCLOGINFO_TYPE_NONE			(0x0<<28)
#define MPII_REP_IOCLOGINFO_TYPE_SCSI			(0x1<<28)
#define MPII_REP_IOCLOGINFO_TYPE_FC			(0x2<<28)
#define MPII_REP_IOCLOGINFO_TYPE_SAS			(0x3<<28)
#define MPII_REP_IOCLOGINFO_TYPE_ISCSI			(0x4<<28)
#define MPII_REP_IOCLOGINFO_DATA			(0x0fffffff) /* log info data */

/* event notification types */
#define MPII_EVENT_NONE					(0x00)
#define MPII_EVENT_LOG_DATA				(0x01)
#define MPII_EVENT_STATE_CHANGE				(0x02)
#define MPII_EVENT_HARD_RESET_RECEIVED			(0x05)
#define MPII_EVENT_EVENT_CHANGE				(0x0a)
#define MPII_EVENT_TASK_SET_FULL			(0x0e)
#define MPII_EVENT_SAS_DEVICE_STATUS_CHANGE		(0x0f)
#define MPII_EVENT_IR_OPERATION_STATUS			(0x14)
#define MPII_EVENT_SAS_DISCOVERY			(0x16)
#define MPII_EVENT_SAS_BROADCAST_PRIMITIVE		(0x17)
#define MPII_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE	(0x18)
#define MPII_EVENT_SAS_INIT_TABLE_OVERFLOW		(0x19)
#define MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST		(0x1c)
#define MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE	(0x1d)
#define MPII_EVENT_IR_VOLUME				(0x1e)
#define MPII_EVENT_IR_PHYSICAL_DISK			(0x1f)
#define MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST		(0x20)
#define MPII_EVENT_LOG_ENTRY_ADDED			(0x21)

/* messages */

#define MPII_WHOINIT_NOONE				(0x00)
#define MPII_WHOINIT_SYSTEM_BIOS			(0x01)
#define MPII_WHOINIT_ROM_BIOS				(0x02)
#define MPII_WHOINIT_PCI_PEER				(0x03)
#define MPII_WHOINIT_HOST_DRIVER			(0x04)
#define MPII_WHOINIT_MANUFACTURER			(0x05)

/* page address fields */
#define MPII_PAGE_ADDRESS_FC_BTID	(1<<24)	/* Bus Target ID */

/* default messages */

struct mpii_msg_request {
	u_int8_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved3;
	u_int8_t		reserved4;
	u_int8_t		reserved5;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved6;
} __packed;

struct mpii_msg_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_if;
	u_int16_t		reserved4;
	
	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

/* ioc init */

struct mpii_msg_iocinit_request {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;
	u_int8_t		hdr_version_unit;
	u_int8_t		hdr_version_dev;

	u_int32_t		reserved5;

	u_int32_t		reserved6;

	u_int16_t		reserved7;
	u_int16_t		system_request_frame_size;

	u_int16_t		reply_descriptor_post_queue_depth;
	u_int16_t		reply_free_queue_depth;

	u_int32_t		sense_buffer_address_high;

	u_int32_t		system_reply_address_high;

	u_int64_t		system_request_frame_base_address;

	u_int64_t		reply_descriptor_post_queue_address;

	u_int64_t		reply_free_queue_address;

	u_int64_t		timestamp;

} __packed;

struct mpii_msg_iocinit_reply {
	u_int8_t		whoinit;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_iocfacts_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;
} __packed;

struct mpii_msg_iocfacts_reply {
	u_int8_t		msg_version_min;
	u_int8_t		msg_version_maj;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		header_version_dev;
	u_int8_t		header_version_unit;
	u_int8_t		ioc_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int16_t		ioc_exceptions;
#define MPII_IOCFACTS_EXCEPT_CONFIG_CHECKSUM_FAIL	(1<<0)
#define MPII_IOCFACTS_EXCEPT_RAID_CONFIG_INVALID	(1<<1)
#define MPII_IOCFACTS_EXCEPT_FW_CHECKSUM_FAIL		(1<<2)
#define MPII_IOCFACTS_EXCEPT_MANUFACT_CHECKSUM_FAIL	(1<<3)
#define MPII_IOCFACTS_EXCEPT_METADATA_UNSUPPORTED	(1<<4)
#define MPII_IOCFACTS_EXCEPT_IR_FOREIGN_CONFIG_MAC	(1<<8)
	/* XXX JPG BOOT_STATUS in bits[7:5] */
	/* XXX JPG all these #defines need to be fixed up */
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		max_chain_depth;
	u_int8_t		whoinit;
	u_int8_t		number_of_ports;
	u_int8_t		reserved2;

	u_int16_t		request_credit;
	u_int16_t		product_id;

	u_int32_t		ioc_capabilities;
#define MPII_IOCFACTS_CAPABILITY_EVENT_REPLAY           (1<<13)
#define MPII_IOCFACTS_CAPABILITY_INTEGRATED_RAID        (1<<12)
#define MPII_IOCFACTS_CAPABILITY_TLR                    (1<<11)
#define MPII_IOCFACTS_CAPABILITY_MULTICAST              (1<<8)
#define MPII_IOCFACTS_CAPABILITY_BIDIRECTIONAL_TARGET   (1<<7)
#define MPII_IOCFACTS_CAPABILITY_EEDP                   (1<<6)
#define MPII_IOCFACTS_CAPABILITY_SNAPSHOT_BUFFER        (1<<4)
#define MPII_IOCFACTS_CAPABILITY_DIAG_TRACE_BUFFER      (1<<3)
#define MPII_IOCFACTS_CAPABILITY_TASK_SET_FULL_HANDLING (1<<2)

	u_int8_t		fw_version_dev;
	u_int8_t		fw_version_unit;
	u_int8_t		fw_version_min;
	u_int8_t		fw_version_maj;

	u_int16_t		ioc_request_frame_size;
	u_int16_t		reserved3;

	u_int16_t		max_initiators;
	u_int16_t		max_targets;

	u_int16_t		max_sas_expanders;
	u_int16_t		max_enclosures;

	u_int16_t		protocol_flags;
	u_int16_t		high_priority_credit;

	u_int16_t		max_reply_descriptor_post_queue_depth;
	u_int8_t		reply_frame_size;
	u_int8_t		max_volumes;

	u_int16_t		max_dev_handle;
	u_int16_t		max_persistent_entries;

	u_int32_t		reserved4;

} __packed;

struct mpii_msg_portfacts_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;
} __packed;

struct mpii_msg_portfacts_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		port_number;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int8_t		reserved5;
	u_int8_t		port_type;
#define MPII_PORTFACTS_PORTTYPE_INACTIVE		(0x00)
#define MPII_PORTFACTS_PORTTYPE_FC			(0x10)
#define MPII_PORTFACTS_PORTTYPE_ISCSI			(0x20)
#define MPII_PORTFACTS_PORTTYPE_SAS_PHYSICAL		(0x30)
#define MPII_PORTFACTS_PORTTYPE_SAS_VIRTUAL		(0x31)
	u_int16_t		reserved6;

	u_int16_t		max_posted_cmd_buffers;
	u_int16_t		reserved7;
} __packed;

struct mpii_msg_portenable_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2;
	u_int8_t		port_flags;	
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;
} __packed;

struct mpii_msg_portenable_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2;
	u_int8_t		port_flags;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int16_t		reserved5;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_event_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved2;
	u_int8_t		reserved3;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved4;

	u_int32_t		reserved5;

	u_int32_t		reserved6;

	u_int32_t		event_masks[4];
	
	u_int16_t		sas_broadcase_primitive_masks;
	u_int16_t		reserved7;

	u_int32_t		reserved8;
} __packed;

struct mpii_msg_event_reply {
	u_int16_t		event_data_length;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		ack_required;
#define MPII_EVENT_ACK_REQUIRED				(0x01)
	u_int8_t		msg_flags;
#define MPII_EVENT_FLAGS_REPLY_KEPT			(1<<7)

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved2;

	u_int16_t		reserved3;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int16_t		event;
	u_int16_t		reserved4;

	u_int32_t		event_context;

	/* event data follows */
} __packed;

/* XXX JPG */
struct mpii_evt_change {
	u_int8_t		event_state;
	u_int8_t		reserved[3];
} __packed;

/* XXX JPG */
struct mpii_evt_sas_phy {
	u_int8_t		phy_num;
	u_int8_t		link_rates;
#define MPII_EVT_SASPHY_LINK_CUR(x)			((x) & 0xf0) >> 4
#define MPII_EVT_SASPHY_LINK_PREV(x)			(x) & 0x0f
#define MPII_EVT_SASPHY_LINK_ENABLED			(0x0)
#define MPII_EVT_SASPHY_LINK_DISABLED			(0x1)
#define MPII_EVT_SASPHY_LINK_NEGFAIL			(0x2)
#define MPII_EVT_SASPHY_LINK_SATAOOB			(0x3)
#define MPII_EVT_SASPHY_LINK_1_5GBPS			(0x8)
#define MPII_EVT_SASPHY_LINK_3_0GBPS			(0x9)
	u_int16_t		dev_handle;

	u_int64_t		sas_addr;
} __packed;

struct mpii_evt_sas_change {
	u_int16_t		task_tag;
	u_int8_t		reason;
#define MPII_EVT_SASCH_REASON_SMART_DATA		(0x05)
#define MPII_EVT_SASCH_REASON_UNSUPPORTED		(0x07)
#define MPII_EVT_SASCH_REASON_INTERNAL_RESET		(0x08)
#define MPII_EVT_SASCH_REASON_TASK_ABORT_INTERVAL	(0x09)
#define MPII_EVT_SASCH_REASON_ABORT_TASK_SET_INTERVAL	(0x0a)
#define MPII_EVT_SASCH_REASON_CLEAR_TASK_SET_INTERVAL	(0x0b)
#define MPII_EVT_SASCH_REASON_QUERY_TASK_INTERVAL	(0x0c)
#define MPII_EVT_SASCH_REASON_ASYNC_NOTIFICATION	(0x0d)
#define MPII_EVT_SASCH_REASON_CMP_INTERNAL_DEV_RESET	(0x0e)
#define MPII_EVT_SASCH_REASON_CMP_TASK_ABORT_INTERNAL	(0x0f)
#define MPII_EVT_SASCH_REASON_SATA_INIT_FAILURE		(0x10)
	u_int8_t		reserved1;

	u_int8_t		asc;
	u_int8_t		ascq;
	u_int16_t		dev_handle;

	u_int32_t		reserved2;

	u_int64_t		sas_addr;

	u_int16_t		lun[4];
} __packed;

struct mpii_msg_eventack_request {
	u_int16_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		event;
	u_int16_t		reserved4;

	u_int32_t		event_context;
} __packed;

struct mpii_msg_eventack_reply {
	u_int16_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;
} __packed;

struct mpii_msg_fwupload_request {
	u_int8_t		image_type;
#define MPII_FWUPLOAD_IMAGETYPE_IOC_FW			(0x00)
#define MPII_FWUPLOAD_IMAGETYPE_NV_FW			(0x01)
#define MPII_FWUPLOAD_IMAGETYPE_NV_BACKUP		(0x05)
#define MPII_FWUPLOAD_IMAGETYPE_NV_MANUFACTURING	(0x06)
#define MPII_FWUPLOAD_IMAGETYPE_NV_CONFIG_1		(0x07)
#define MPII_FWUPLOAD_IMAGETYPE_NV_CONFIG_2		(0x08)
#define MPII_FWUPLOAD_IMAGETYPE_NV_MEGARAID		(0x09)
#define MPII_FWUPLOAD_IMAGETYPE_NV_COMPLETE		(0x0a)
#define MPII_FWUPLOAD_IMAGETYPE_COMMON_BOOT_BLOCK	(0x0b)
	u_int8_t		reserved1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int32_t		reserved4;

	u_int32_t		reserved5;

	struct mpii_fw_tce	tce;

	/* followed by an sgl */
} __packed;

struct mpii_msg_fwupload_reply {
	u_int8_t		image_type;
	u_int8_t		reserved1;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int8_t		reserved2[3];
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int16_t		reserved4;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		actual_image_size;
} __packed;

struct mpii_msg_scsi_io {
	u_int16_t		dev_handle;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;
	
	u_int32_t		sense_buffer_low_address;

	u_int16_t		sgl_flags;
	u_int8_t		sense_buffer_length;
	u_int8_t		reserved4;

	u_int8_t		sgl_offset0;
	u_int8_t		sgl_offset1;
	u_int8_t		sgl_offset2;
	u_int8_t		sgl_offset3;

	u_int32_t		skip_count;

	u_int32_t		data_length;

	u_int32_t		bidirectional_data_length;

	u_int16_t		io_flags;
	u_int16_t		eedp_flags;

	u_int32_t		eedp_block_size;

	u_int32_t		secondary_reference_tag;

	u_int16_t		secondary_application_tag;
	u_int16_t		application_tag_translation_mask;

	u_int16_t		lun[4];

/* the following 16 bits are defined in MPI2 as the control field */
	u_int8_t		reserved5;
	u_int8_t		tagging;
#define MPII_SCSIIO_ATTR_SIMPLE_Q			(0x0)
#define MPII_SCSIIO_ATTR_HEAD_OF_Q			(0x1)
#define MPII_SCSIIO_ATTR_ORDERED_Q			(0x2)
#define MPII_SCSIIO_ATTR_ACA_Q				(0x4)
#define MPII_SCSIIO_ATTR_UNTAGGED			(0x5)
#define MPII_SCSIIO_ATTR_NO_DISCONNECT			(0x7)
	u_int8_t		reserved6;
	u_int8_t		direction;
#define MPII_SCSIIO_DIR_NONE				(0x0)
#define MPII_SCSIIO_DIR_WRITE				(0x1)
#define MPII_SCSIIO_DIR_READ				(0x2)

#define	MPII_CDB_LEN					(32)
	u_int8_t		cdb[MPII_CDB_LEN];

	/* followed by an sgl */
} __packed;

struct mpii_msg_scsi_io_error {
	u_int16_t		dev_handle;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		reserved1;
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved3;

	u_int8_t		scsi_status;
	/* XXX JPG validate this */
#if notyet
#define MPII_SCSIIO_ERR_STATUS_SUCCESS
#define MPII_SCSIIO_ERR_STATUS_CHECK_COND
#define MPII_SCSIIO_ERR_STATUS_BUSY
#define MPII_SCSIIO_ERR_STATUS_INTERMEDIATE
#define MPII_SCSIIO_ERR_STATUS_INTERMEDIATE_CONDMET
#define MPII_SCSIIO_ERR_STATUS_RESERVATION_CONFLICT
#define MPII_SCSIIO_ERR_STATUS_CMD_TERM
#define MPII_SCSIIO_ERR_STATUS_TASK_SET_FULL
#define MPII_SCSIIO_ERR_STATUS_ACA_ACTIVE
#endif
	u_int8_t		scsi_state;
#define MPII_SCSIIO_ERR_STATE_AUTOSENSE_VALID		(1<<0)
#define MPII_SCSIIO_ERR_STATE_AUTOSENSE_FAILED		(1<<1)
#define MPII_SCSIIO_ERR_STATE_NO_SCSI_STATUS		(1<<2)
#define MPII_SCSIIO_ERR_STATE_TERMINATED		(1<<3)
#define MPII_SCSIIO_ERR_STATE_RESPONSE_INFO_VALID	(1<<4)
#define MPII_SCSIIO_ERR_STATE_QUEUE_TAG_REJECTED	(0xffff)
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	u_int32_t		transfer_count;

	u_int32_t		sense_count;

	u_int32_t		response_info;

	u_int16_t		task_tag;
	u_int16_t		reserved4;

	u_int32_t		bidirectional_transfer_count;

	u_int32_t		reserved5;

	u_int32_t		reserved6;
} __packed;

struct mpii_request_descriptor {
	u_int8_t		request_flags;
	u_int8_t		vf_id;
	u_int16_t		smid;

	u_int16_t		lmid;
	/*
	 * the following field is descriptor type dependent
	 *    default - undefined 
	 *    scsi io - device handle
	 *    high priority - reserved
	 */
	u_int16_t		type_dependent;
} __packed;

struct mpii_reply_descriptor {
	u_int8_t		reply_flags;
#define MPII_REPLY_DESCR_FLAGS_TYPE_MASK               (0x0f)
#define MPII_REPLY_DESCR_FLAGS_SCSI_IO_SUCCESS         (0x00)
#define MPII_REPLY_DESCR_FLAGS_ADDRESS_REPLY           (0x01)
#define MPII_REPLY_DESCR_FLAGS_TARGETASSIST_SUCCESS    (0x02)
#define MPII_REPLY_DESCR_FLAGS_TARGET_COMMAND_BUFFER   (0x03)
#define MPII_REPLY_DESCR_FLAGS_UNUSED                  (0x0f)
	u_int8_t		vf_id;
	/*
	 * the following field is reply descriptor type dependent
	 *     default - undefined
	 *     scsi io success - smid 
	 *     address reply - smid 
	 */
	u_int16_t		type_dependent1;

	/*
	 * the following field is reply descriptor type dependent
	 *     default - undefined
	 *     scsi io success - bottom 16 bits is task tag 
	 *     address reply - reply frame address
	 */
	u_int32_t		type_dependent2;
} __packed;

struct mpii_request_header {
	u_int16_t		function_dependent1;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		function_dependent2;
	u_int8_t		function_dependent3;
	u_int8_t		message_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved;
} __packed;

/* XXX JPG delete this? */
struct mpii_msg_scsi_task_request {
	u_int8_t		target_id;
	u_int8_t		bus;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int8_t		reserved1;
	u_int8_t		task_type;
#define MPII_MSG_SCSI_TASK_TYPE_ABORT_TASK		(0x01)
#define MPII_MSG_SCSI_TASK_TYPE_ABRT_TASK_SET		(0x02)
#define MPII_MSG_SCSI_TASK_TYPE_TARGET_RESET		(0x03)
#define MPII_MSG_SCSI_TASK_TYPE_RESET_BUS		(0x04)
#define MPII_MSG_SCSI_TASK_TYPE_LOGICAL_UNIT_RESET	(0x05)
	u_int8_t		reserved2;
	u_int8_t		msg_flags;

	u_int32_t		msg_context;

	u_int16_t		lun[4];

	u_int32_t		reserved3[7];

	u_int32_t		target_msg_context;
} __packed;

/* XXX JPG delete this? */
struct mpii_msg_scsi_task_reply {
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

struct mpii_cfg_hdr {
	u_int8_t		page_version;
	u_int8_t		page_length;
	u_int8_t		page_number;
	u_int8_t		page_type;
#define MPII_CONFIG_REQ_PAGE_TYPE_ATTRIBUTE		(0xf0)
#define MPI2_CONFIG_PAGEATTR_READ_ONLY              	(0x00)
#define MPI2_CONFIG_PAGEATTR_CHANGEABLE             	(0x10)
#define MPI2_CONFIG_PAGEATTR_PERSISTENT             	(0x20)

#define MPII_CONFIG_REQ_PAGE_TYPE_MASK			(0x0f)
#define MPII_CONFIG_REQ_PAGE_TYPE_IO_UNIT		(0x00)
#define MPII_CONFIG_REQ_PAGE_TYPE_IOC			(0x01)
#define MPII_CONFIG_REQ_PAGE_TYPE_BIOS			(0x02)
#define MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL		(0x08)
#define MPII_CONFIG_REQ_PAGE_TYPE_MANUFACTURING		(0x09)
#define MPII_CONFIG_REQ_PAGE_TYPE_RAID_PD		(0x0a)
#define MPII_CONFIG_REQ_PAGE_TYPE_EXTENDED		(0x0f)
#define MPII_CONFIG_REQ_PAGE_TYPE_DRIVER_MAPPING	(0x17)
} __packed;

struct mpii_ecfg_hdr {
	u_int8_t		page_version;
	u_int8_t		reserved1;
	u_int8_t		page_number;
	u_int8_t		page_type;

	u_int16_t		ext_page_length;
	u_int8_t		ext_page_type;
	u_int8_t		reserved2;
} __packed;

struct mpii_msg_config_request {
	u_int8_t		action;
#define MPII_CONFIG_REQ_ACTION_PAGE_HEADER		(0x00)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_CURRENT	(0x01)
#define MPII_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT	(0x02)
#define MPII_CONFIG_REQ_ACTION_PAGE_DEFAULT		(0x03)
#define MPII_CONFIG_REQ_ACTION_PAGE_WRITE_NVRAM		(0x04)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_DEFAULT	(0x05)
#define MPII_CONFIG_REQ_ACTION_PAGE_READ_NVRAM		(0x06)
	u_int8_t		sgl_flags;
	u_int8_t		chain_offset;
	u_int8_t		function;

	u_int16_t		ext_page_len;
	u_int8_t		ext_page_type;
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_IO_UNIT	(0x10)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_EXPANDER	(0x11)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_DEVICE		(0x12)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_SAS_PHY		(0x13)
#define MPII_CONFIG_REQ_EXTPAGE_TYPE_LOG		(0x14)
#define MPI2_CONFIG_EXTPAGETYPE_ENCLOSURE            	(0x15)
#define MPI2_CONFIG_EXTPAGETYPE_RAID_CONFIG         	(0x16)
#define MPI2_CONFIG_EXTPAGETYPE_DRIVER_MAPPING      	(0x17)
#define MPI2_CONFIG_EXTPAGETYPE_SAS_PORT            	(0x18)
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int32_t		reserved2[2];

	struct mpii_cfg_hdr	config_header;

	u_int32_t		page_address;
/* XXX lots of defns here */

	struct mpii_sge		page_buffer;
} __packed;

struct mpii_msg_config_reply {
	u_int8_t		action;
	u_int8_t		sgl_flags;
	u_int8_t		msg_length;
	u_int8_t		function;

	u_int16_t		ext_page_length;
	u_int8_t		ext_page_type;
	u_int8_t		msg_flags;

	u_int8_t		vp_id;
	u_int8_t		vf_id;
	u_int16_t		reserved1;

	u_int16_t		reserved2;
	u_int16_t		ioc_status;

	u_int32_t		ioc_loginfo;

	struct mpii_cfg_hdr	config_header;
} __packed;

struct mpii_cfg_manufacturing_pg0 {
	struct mpii_cfg_hdr	config_header;

	char			chip_name[16];
	char			chip_revision[8];
	char			board_name[16];
	char			board_assembly[16];
	char			board_tracer_number[16];
} __packed;

struct mpii_cfg_ioc_pg1 {
	struct mpii_cfg_hdr     config_header;

	u_int32_t       flags;

	u_int32_t       coalescing_timeout;
#define	MPII_CFG_IOC_1_REPLY_COALESCING			(1<<0)

	u_int8_t        coalescing_depth;
	u_int8_t        pci_slot_num;
	u_int8_t        pci_bus_num;
	u_int8_t        pci_domain_segment;

	u_int32_t       reserved1;

	u_int32_t       reserved2;
} __packed;

/*
struct mpii_cfg_raid_vol_pg0 {
	u_int8_t		vol_id;
	u_int8_t		vol_bus;
	u_int8_t		vol_ioc;
	u_int8_t		vol_page;

	u_int8_t		vol_type;
#define MPII_CFG_RAID_TYPE_RAID_IS			(0x00)
#define MPII_CFG_RAID_TYPE_RAID_IME			(0x01)
#define MPII_CFG_RAID_TYPE_RAID_IM			(0x02)
#define MPII_CFG_RAID_TYPE_RAID_5			(0x03)
#define MPII_CFG_RAID_TYPE_RAID_6			(0x04)
#define MPII_CFG_RAID_TYPE_RAID_10			(0x05)
#define MPII_CFG_RAID_TYPE_RAID_50			(0x06)
	u_int8_t		flags;
#define MPII_CFG_RAID_VOL_INACTIVE	(1<<3)
	u_int16_t		reserved;
} __packed;
*/
struct mpii_cfg_ioc_pg3 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		no_phys_disks;
	u_int8_t		reserved[3];

	/* followed by a list of mpii_cfg_raid_physdisk structs */
} __packed;

struct mpii_cfg_ioc_pg8 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		num_devs_per_enclosure;
	u_int8_t		reserved1;
	u_int16_t		reserved2;

	u_int16_t		max_persistent_entries;
	u_int16_t		max_num_physical_mapped_ids;

	u_int16_t		flags;
#define	MPII_IOC_PG8_FLAGS_DA_START_SLOT_1		(1<<5)
#define MPII_IOC_PG8_FLAGS_RESERVED_TARGETID_0		(1<<4)
#define MPII_IOC_PG8_FLAGS_MAPPING_MODE_MASK		(0x0000000e)
#define MPII_IOC_PG8_FLAGS_DEVICE_PERSISTENCE_MAPPING	(0<<1)
#define MPII_IOC_PG8_FLAGS_ENCLOSURE_SLOT_MAPPING	(1<<1)
#define MPII_IOC_PG8_FLAGS_DISABLE_PERSISTENT_MAPPING	(1<<0)
#define	MPII_IOC_PG8_FLAGS_ENABLE_PERSISTENT_MAPPING	(0<<0)
	u_int16_t		reserved3;

	u_int16_t		ir_volume_mapping_flags;
#define	MPII_IOC_PG8_IRFLAGS_VOLUME_MAPPING_MODE_MASK	(0x00000003)
#define	MPII_IOC_PG8_IRFLAGS_LOW_VOLUME_MAPPING		(0<<0)
#define	MPII_IOC_PG8_IRFLAGS_HIGH_VOLUME_MAPPING	(1<<0)
	u_int16_t		reserved4;
	
	u_int32_t		reserved5;
} __packed;

struct mpii_cfg_raid_physdisk {
	u_int8_t		phys_disk_id;
	u_int8_t		phys_disk_bus;
	u_int8_t		phys_disk_ioc;
	u_int8_t		phys_disk_num;
} __packed;

struct mpii_cfg_fc_port_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int32_t		flags;

	u_int8_t		mpii_port_nr;
	u_int8_t		link_type;
	u_int8_t		port_state;
	u_int8_t		reserved1;

	u_int32_t		port_id;

	u_int64_t		wwnn;

	u_int64_t		wwpn;

	u_int32_t		supported_service_class;

	u_int32_t		supported_speeds;

	u_int32_t		current_speed;

	u_int32_t		max_frame_size;

	u_int64_t		fabric_wwnn;

	u_int64_t		fabric_wwpn;

	u_int32_t		discovered_port_count;

	u_int32_t		max_initiators;

	u_int8_t		max_aliases_supported;
	u_int8_t		max_hard_aliases_supported;
	u_int8_t		num_current_aliases;
	u_int8_t		reserved2;
} __packed;

struct mpii_cfg_fc_port_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int32_t		flags;

	u_int64_t		noseepromwwnn;

	u_int64_t		noseepromwwpn;

	u_int8_t		hard_alpa;
	u_int8_t		link_config;
	u_int8_t		topology_config;
	u_int8_t		alt_connector;

	u_int8_t		num_req_aliases;
	u_int8_t		rr_tov;
	u_int8_t		initiator_dev_to;
	u_int8_t		initiator_lo_pend_to;
} __packed;

struct mpii_cfg_fc_device_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int64_t		wwnn;

	u_int64_t		wwpn;

	u_int32_t		port_id;

	u_int8_t		protocol;
	u_int8_t		flags;
	u_int16_t		bb_credit;

	u_int16_t		max_rx_frame_size;
	u_int8_t		adisc_hard_alpa;
	u_int8_t		port_nr;

	u_int8_t		fc_ph_low_version;
	u_int8_t		fc_ph_high_version;
	u_int8_t		current_target_id;
	u_int8_t		current_bus;
} __packed;

struct mpii_cfg_raid_vol_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		volume_id;
	u_int8_t		volume_bus;
	u_int8_t		volume_ioc;
	u_int8_t		volume_type;

	u_int8_t		volume_status;
#define MPII_CFG_RAID_VOL_0_STATUS_ENABLED		(1<<0)
#define MPII_CFG_RAID_VOL_0_STATUS_QUIESCED		(1<<1)
#define MPII_CFG_RAID_VOL_0_STATUS_RESYNCING		(1<<2)
#define MPII_CFG_RAID_VOL_0_STATUS_ACTIVE		(1<<3)
#define MPII_CFG_RAID_VOL_0_STATUS_BADBLOCK_FULL	(1<<4)
	u_int8_t		volume_state;
#define MPII_CFG_RAID_VOL_0_STATE_OPTIMAL		(0x00)
#define MPII_CFG_RAID_VOL_0_STATE_DEGRADED		(0x01)
#define MPII_CFG_RAID_VOL_0_STATE_FAILED		(0x02)
#define MPII_CFG_RAID_VOL_0_STATE_MISSING		(0x03)
	u_int16_t		reserved1;

	u_int16_t		volume_settings;
#define MPII_CFG_RAID_VOL_0_SETTINGS_WRITE_CACHE_EN	(1<<0)
#define MPII_CFG_RAID_VOL_0_SETTINGS_OFFLINE_SMART_ERR	(1<<1)
#define MPII_CFG_RAID_VOL_0_SETTINGS_OFFLINE_SMART	(1<<2)
#define MPII_CFG_RAID_VOL_0_SETTINGS_AUTO_SWAP		(1<<3)
#define MPII_CFG_RAID_VOL_0_SETTINGS_HI_PRI_RESYNC	(1<<4)
#define MPII_CFG_RAID_VOL_0_SETTINGS_PROD_SUFFIX	(1<<5)
#define MPII_CFG_RAID_VOL_0_SETTINGS_FAST_SCRUB		(1<<6) /* obsolete */
#define MPII_CFG_RAID_VOL_0_SETTINGS_DEFAULTS		(1<<15)
	u_int8_t		hot_spare_pool;
	u_int8_t		reserved2;

	u_int32_t		max_lba;

	u_int32_t		reserved3;

	u_int32_t		stripe_size;

	u_int32_t		reserved4;

	u_int32_t		reserved5;

	u_int8_t		num_phys_disks;
	u_int8_t		data_scrub_rate;
	u_int8_t		resync_rate;
	u_int8_t		inactive_status;
#define MPII_CFG_RAID_VOL_0_INACTIVE_UNKNOWN		(0x00)
#define MPII_CFG_RAID_VOL_0_INACTIVE_STALE_META		(0x01)
#define MPII_CFG_RAID_VOL_0_INACTIVE_FOREIGN_VOL	(0x02)
#define MPII_CFG_RAID_VOL_0_INACTIVE_NO_RESOURCES	(0x03)
#define MPII_CFG_RAID_VOL_0_INACTIVE_CLONED_VOL		(0x04)
#define MPII_CFG_RAID_VOL_0_INACTIVE_INSUF_META		(0x05)

	/* followed by a list of mpii_cfg_raid_vol_pg0_physdisk structs */
} __packed;

struct mpii_cfg_raid_vol_pg0_physdisk {
	u_int16_t		reserved;
	u_int8_t		phys_disk_map;
	u_int8_t		phys_disk_num;
} __packed;

struct mpii_cfg_raid_vol_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		volume_id;
	u_int8_t		volume_bus;
	u_int8_t		volume_ioc;
	u_int8_t		reserved1;

	u_int8_t		guid[24];

	u_int8_t		name[32];

	u_int64_t		wwid;

	u_int32_t		reserved2;

	u_int32_t		reserved3;
} __packed;

struct mpii_cfg_raid_physdisk_pg0 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		phys_disk_id;
	u_int8_t		phys_disk_bus;
	u_int8_t		phys_disk_ioc;
	u_int8_t		phys_disk_num;

	u_int8_t		enc_id;
	u_int8_t		enc_bus;
	u_int8_t		hot_spare_pool;
	u_int8_t		enc_type;
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_NONE		(0x0)
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_SAFTE		(0x1)
#define MPII_CFG_RAID_PHYDISK_0_ENCTYPE_SES		(0x2)

	u_int32_t		reserved1;

	u_int8_t		ext_disk_id[8];

	u_int8_t		disk_id[16];

	u_int8_t		vendor_id[8];

	u_int8_t		product_id[16];

	u_int8_t		product_rev[4];

	u_int8_t		info[32];

	u_int8_t		phys_disk_status;
#define MPII_CFG_RAID_PHYDISK_0_STATUS_OUTOFSYNC	(1<<0)
#define MPII_CFG_RAID_PHYDISK_0_STATUS_QUIESCED		(1<<1)
	u_int8_t		phys_disk_state;
#define MPII_CFG_RAID_PHYDISK_0_STATE_ONLINE		(0x00)
#define MPII_CFG_RAID_PHYDISK_0_STATE_MISSING		(0x01)
#define MPII_CFG_RAID_PHYDISK_0_STATE_INCOMPAT		(0x02)
#define MPII_CFG_RAID_PHYDISK_0_STATE_FAILED		(0x03)
#define MPII_CFG_RAID_PHYDISK_0_STATE_INIT		(0x04)
#define MPII_CFG_RAID_PHYDISK_0_STATE_OFFLINE		(0x05)
#define MPII_CFG_RAID_PHYDISK_0_STATE_HOSTFAIL		(0x06)
#define MPII_CFG_RAID_PHYDISK_0_STATE_OTHER		(0xff)
	u_int16_t		reserved2;

	u_int32_t		max_lba;

	u_int8_t		error_cdb_byte;
	u_int8_t		error_sense_key;
	u_int16_t		reserved3;

	u_int16_t		error_count;
	u_int8_t		error_asc;
	u_int8_t		error_ascq;

	u_int16_t		smart_count;
	u_int8_t		smart_asc;
	u_int8_t		smart_ascq;
} __packed;

struct mpii_cfg_raid_physdisk_pg1 {
	struct mpii_cfg_hdr	config_header;

	u_int8_t		num_phys_disk_paths;
	u_int8_t		phys_disk_num;
	u_int16_t		reserved1;

	u_int32_t		reserved2;

	/* followed by mpii_cfg_raid_physdisk_path structs */
} __packed;

struct mpii_cfg_raid_physdisk_path {
	u_int8_t		phys_disk_id;
	u_int8_t		phys_disk_bus;
	u_int16_t		reserved1;

	u_int64_t		wwwid;

	u_int64_t		owner_wwid;

	u_int8_t		ownder_id;
	u_int8_t		reserved2;
	u_int16_t		flags;
#define MPII_CFG_RAID_PHYDISK_PATH_INVALID	(1<<0)
#define MPII_CFG_RAID_PHYDISK_PATH_BROKEN	(1<<1)
} __packed;

#define MPII_CFG_SAS_DEV_ADDR_NEXT		(0<<28)
#define MPII_CFG_SAS_DEV_ADDR_BUS		(1<<28)
#define MPII_CFG_SAS_DEV_ADDR_HANDLE		(2<<28)

struct mpii_cfg_sas_dev_pg0 {
	struct mpii_ecfg_hdr	config_header;

	u_int16_t		slot;
	u_int16_t		enc_handle;

	u_int64_t		sas_addr;

	u_int16_t		parent_dev_handle;
	u_int8_t		phy_num;
	u_int8_t		access_status;

	u_int16_t		dev_handle;
	u_int8_t		target;
	u_int8_t		bus;

	u_int32_t		device_info;
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE			(0x7)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_NONE		(0x0)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_END		(0x1)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_EDGE_EXPANDER	(0x2)
#define MPII_CFG_SAS_DEV_0_DEVINFO_TYPE_FANOUT_EXPANDER	(0x3)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SATA_HOST		(1<<3)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SMP_INITIATOR	(1<<4)
#define MPII_CFG_SAS_DEV_0_DEVINFO_STP_INITIATOR	(1<<5)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SSP_INITIATOR	(1<<6)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SATA_DEVICE		(1<<7)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SMP_TARGET		(1<<8)
#define MPII_CFG_SAS_DEV_0_DEVINFO_STP_TARGET		(1<<9)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SSP_TARGET		(1<<10)
#define MPII_CFG_SAS_DEV_0_DEVINFO_DIRECT_ATTACHED	(1<<11)
#define MPII_CFG_SAS_DEV_0_DEVINFO_LSI_DEVICE		(1<<12)
#define MPII_CFG_SAS_DEV_0_DEVINFO_ATAPI_DEVICE		(1<<13)
#define MPII_CFG_SAS_DEV_0_DEVINFO_SEP_DEVICE		(1<<14)

	u_int16_t		flags;
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_PRESENT		(1<<0)
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_MAPPED		(1<<1)
#define MPII_CFG_SAS_DEV_0_FLAGS_DEV_MAPPED_PERSISTENT	(1<<2)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_PORT_SELECTOR	(1<<3)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_FUA		(1<<4)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_NCQ		(1<<5)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_SMART		(1<<6)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_LBA48		(1<<7)
#define MPII_CFG_SAS_DEV_0_FLAGS_UNSUPPORTED		(1<<8)
#define MPII_CFG_SAS_DEV_0_FLAGS_SATA_SETTINGS		(1<<9)
	u_int8_t		physical_port;
	u_int8_t		reserved;
} __packed;

struct mpii_cfg_bios_pg2 {
	struct mpii_cfg_hdr	config_header;

	u_int32_t		reserved1[6];

	u_int8_t		req_boot_device_form;
#define MPII_CFG_BIOS_PG2_DEVICE_FORM_MASK		(0xf)
#define	MPII_CFG_BIOS_PG2_DEVICE_FORM_NO_BOOT_DEVICE	(0x0)
#define MPII_CFG_BIOS_PG2_DEVICE_FORM_SAS_WWID		(0x5)
#define MPII_CFG_BIOS_PG2_DEVICE_FORM_ENCLOSURE_SLOT	(0x6)
#define MPII_CFG_BIOS_PG2_DEVICE_FORM_DEVICE_NAME	(0x7)
	u_int8_t		reserved2[3];

	u_int32_t		a;	

	u_int8_t		req_alt_boot_device_form;
	u_int8_t		reserved3[3];

	u_int32_t		b;

	u_int8_t		current_boot_device_form;
	u_int8_t		reserved4[3];

	u_int32_t		c;
} __packed;

struct mpii_cfg_raid_config_pg0 {
	struct	mpii_ecfg_hdr	config_header;

	u_int8_t		num_hot_spares;
	u_int8_t		num_phys_disks;
	u_int8_t		num_volumes;
	u_int8_t		config_num;

	u_int32_t		flags;
#define MPII_CFG_RAID_CONFIG_0_FLAGS_NATIVE		(0<<0)
#define MPII_CFG_RAID_CONFIG_0_FLAGS_FOREIGN		(1<<0)

	u_int32_t		config_guid[6];

	u_int32_t		reserved1;

	u_int8_t		num_elements;
	u_int8_t		reserved2[3];

	/* followed by struct mpii_raid_config_element structs */
} __packed;

struct mpii_raid_config_element {
	u_int16_t		element_flags;
#define MPII_RAID_CONFIG_ELEMENT_FLAG_VOLUME		(0x0)
#define MPII_RAID_CONFIG_ELEMENT_FLAG_VOLUME_PHYS_DISK	(0x1)
#define	MPII_RAID_CONFIG_ELEMENT_FLAG_HSP_PHYS_DISK	(0x2)
#define MPII_RAID_CONFIG_ELEMENT_ONLINE_CE_PHYS_DISK	(0x3)
	u_int16_t		vol_dev_handle;

	u_int8_t		hot_spare_pool;
	u_int8_t		phys_disk_num;
	u_int16_t		phys_disk_dev_handle;
} __packed;

struct mpii_cfg_dpm_pg0 {
	struct mpii_ecfg_hdr	config_header;
#define MPII_DPM_ADDRESS_FORM_MASK			(0xf0000000)
#define MPII_DPM_ADDRESS_FORM_ENTRY_RANGE		(0x00000000)
#define MPII_DPM_ADDRESS_ENTRY_COUNT_MASK		(0x0fff0000)
#define MPII_DPM_ADDRESS_ENTRY_COUNT_SHIFT		(16)
#define MPII_DPM_ADDRESS_START_ENTRY_MASK		(0x0000ffff)

	/* followed by struct mpii_dpm_entry structs */
} __packed;

struct mpii_dpm_entry {
	u_int64_t		physical_identifier;

	u_int16_t		mapping_information;
	u_int16_t		device_index;

	u_int32_t		physical_bits_mapping;

	u_int32_t		reserved1;
} __packed;

struct mpii_evt_sas_discovery {
	u_int8_t		flags;
#define	MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_MASK	(1<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_NO_CHANGE	(0<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DEV_CHANGE_CHANGE	(1<<1)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROG_MASK	(1<<0)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_NOT_IN_PROGRESS	(1<<0)
#define MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROGRESS	(0<<0)
	u_int8_t		reason_code;
#define MPII_EVENT_SAS_DISC_REASON_CODE_STARTED		(0x01)
#define	MPII_EVENT_SAS_DISC_REASON_CODE_COMPLETED	(0x02)
	u_int8_t		physical_port;
	u_int8_t		reserved1;

	u_int32_t		discovery_status;
/* lots of defines go in here */
} __packed;

struct mpii_evt_ir_physical_disk {
	u_int16_t		reserved1;
	u_int8_t		reason_code;
#define MPII_EVENT_IR_PD_RC_SETTINGS_CHANGED		(0x01)
#define MPII_EVENT_IR_PD_RC_STATUS_FLAGS_CHANGED	(0x02)
#define MPII_EVENT_IR_PD_RC_STATUS_CHANGED		(0x03)
	u_int8_t		phys_disk_num;

	u_int16_t		phys_disk_dev_handle;
	u_int16_t		reserved2;

	u_int16_t		slot;
	u_int16_t		enclosure_handle;

	u_int32_t		new_value;
	u_int32_t		previous_value;
} __packed;

struct mpii_evt_sas_topo_change_list {
	u_int16_t		enclosure_handle;
	u_int16_t		expander_dev_handle;

	u_int8_t		num_phys;
	u_int8_t		reserved1[3];

	u_int8_t		num_entries;
	u_int8_t		start_phy_num;
	u_int8_t		exp_status;
#define	MPII_EVENT_SAS_TOPO_ES_ADDED			(0x01)
#define MPII_EVENT_SAS_TOPO_ES_NOT_RESPONDING		(0x02)
#define MPII_EVENT_SAS_TOPO_ES_RESPONDING		(0x03)
#define MPII_EVENT_SAS_TOPO_ES_DELAY_NOT_RESPONDING	(0x04)
	u_int8_t		physical_port;

	/* followed by num_entries number of struct mpii_evt_phy_entry */
} __packed;

struct mpii_evt_phy_entry {
	u_int16_t		attached_dev_handle;
	u_int8_t		link_rate;
#define MPII_EVENT_SAS_TOPO_LR_CURRENT_LINK_RATE_MASK	(0xf0)
#define MPII_EVENT_SAS_TOPO_LR_CURRENT_LINK_RATE_SHIFT	(4)
#define MPII_EVENT_SAS_TOPO_LR_PREVIOUS_LINK_RATE_MASK	(0x0f)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_UNKNOWN_RATE	(0x0)
#define MPII_EVENT_SAS_TOPO_LR_PHY_DISABLE		(0x1)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_FAIL_SPEED_NEG	(0x2)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_SATA_OOB_COMPLETE	(0x3)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_SATA_PORT_SEL	(0x4)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_SMP_RESET_IN_PROG	(0x5)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_1_5_GBPS		(0x8)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_3_0_GBPS		(0x9)
#define MPII_EVENT_SAS_TOPO_LR_PHY_EN_6_0_GBPS		(0xa)
	u_int8_t		phy_status;
#define MPII_EVENT_SAS_TOPO_PS_RC_MASK			(0x0f)
#define	MPII_EVENT_SAS_TOPO_PS_RC_TARG_ADDED		(0x01)
#define MPII_EVENT_SAS_TOPO_PS_RC_TARG_NO_RESPOND_MISS	(0x02)
#define MPII_EVENT_SAS_TOPO_PS_RC_PHY_LINK_STATE_CHANGE	(0x03)
#define MPII_EVENT_SAS_TOPO_PS_RC_PHY_LINK_STATE_NOCHNG	(0x04)
#define MPII_EVENT_SAS_TOPO_PS_RC_WITHIN_DELAY_TIMER	(0x05)
#define MPII_EVENT_SAS_TOPO_PS_PHY_VACANT_MASK		(0x80)
#define MPII_EVENT_SAS_TOPO_PS_PHY_VACANT_NOT_VACANT	(0x00)
#define MPII_EVENT_SAS_TOOP_PS_PHY_VACANT_ACCESS_DENIED	(0x80)
} __packed;

struct mpii_evt_ir_cfg_change_list {
	u_int8_t		num_elements;
	u_int16_t		reserved;
	u_int8_t		config_num;

	u_int32_t		flags;
#define MPII_EVT_IR_CFG_CHANGE_LIST_FLAGS_CFG_MASK	(0x1)
#define MPII_EVT_IR_CFG_CHANGE_LIST_FLAGS_CFG_NATIVE	(0)
#define MPII_EVT_IR_CFG_CHANGE_LIST_FLAGS_CFG_FOREIGN	(1)
	
	/* followed by num_elements struct mpii_evt_ir_cfg_elements */
} __packed;

struct mpii_evt_ir_cfg_element {
	u_int16_t		element_flags;
#define MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_MASK	(0xf)
#define MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_VOLUME	(0x0)
#define MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_VOL_PD	(0x1)
#define MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_HSP_PD	(0x2)
	u_int16_t		vol_dev_handle;

	u_int8_t		reason_code;
#define MPII_EVT_IR_CFG_ELEMENT_RC_ADDED		(0x01)
#define MPII_EVT_IR_CFG_ELEMENT_RC_REMOVED		(0x02)
#define MPII_EVT_IR_CFG_ELEMENT_RC_NO_CHANGE		(0x03)
#define MPII_EVT_IR_CFG_ELEMENT_RC_HIDE			(0x04)
#define MPII_EVT_IR_CFG_ELEMENT_RC_UNHIDE		(0x05)
#define MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_CREATED	(0x06)
#define MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_DELETED	(0x07)
#define MPII_EVT_IR_CFG_ELEMENT_RC_PD_CREATED		(0x08)
#define MPII_EVT_IR_CFG_ELEMENT_RC_PD_DELETED		(0x09)
	u_int8_t		phys_disk_num;
	u_int16_t		phys_disk_dev_handle;
} __packed;

/* #define MPII_DEBUG */
#ifdef MPII_DEBUG
#define DPRINTF(x...)		do { if (mpii_debug) printf(x); } while(0)
#define DNPRINTF(n,x...)	do { if (mpii_debug & (n)) printf(x); } while(0)
#define	MPII_D_CMD		(0x0001)
#define	MPII_D_INTR		(0x0002)
#define	MPII_D_MISC		(0x0004)
#define	MPII_D_DMA		(0x0008)
#define	MPII_D_IOCTL		(0x0010)
#define	MPII_D_RW		(0x0020)
#define	MPII_D_MEM		(0x0040)
#define	MPII_D_CCB		(0x0080)
#define	MPII_D_PPR		(0x0100)
#define	MPII_D_RAID		(0x0200)
#define	MPII_D_EVT		(0x0400)
#define MPII_D_CFG		(0x0800)
#define MPII_D_MAP		(0x1000)

uint32_t  mpii_debug = 0
		| MPII_D_CMD
		| MPII_D_INTR
		| MPII_D_MISC
		| MPII_D_DMA
		| MPII_D_IOCTL
		| MPII_D_RW
		| MPII_D_MEM
		| MPII_D_CCB
		| MPII_D_PPR
		| MPII_D_RAID
		| MPII_D_EVT 
		| MPII_D_CFG
		| MPII_D_MAP
	;
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

#define MPII_REQUEST_SIZE	(512)
#define MPII_REPLY_SIZE		(128)
#define MPII_REPLY_COUNT	PAGE_SIZE / MPII_REPLY_SIZE

/*
 * this is the max number of sge's we can stuff in a request frame:
 * sizeof(scsi_io) + sizeof(sense) + sizeof(sge) * 32 = MPII_REQUEST_SIZE
 */
#define MPII_MAX_SGL			(32)

#define MPII_MAX_REQUEST_CREDIT		(500)
#define	MPII_MAX_REPLY_POST_QDEPTH	(128)

struct mpii_dmamem {
	bus_dmamap_t		mdm_map;
	bus_dma_segment_t	mdm_seg;
	size_t			mdm_size;
	caddr_t			mdm_kva;
};
#define MPII_DMA_MAP(_mdm)	(_mdm)->mdm_map
#define MPII_DMA_DVA(_mdm)	(_mdm)->mdm_map->dm_segs[0].ds_addr
#define MPII_DMA_KVA(_mdm)	(void *)(_mdm)->mdm_kva

struct mpii_ccb_bundle {
	struct mpii_msg_scsi_io	mcb_io; /* sgl must follow */
	struct mpii_sge		mcb_sgl[MPII_MAX_SGL];
	struct scsi_sense_data	mcb_sense;
} __packed;

struct mpii_softc;

struct mpii_rcb {
	void			*rcb_reply;
	u_int32_t		rcb_reply_dva;
};

struct mpii_device {

	RB_ENTRY(mpii_device)	rb_entry;

	u_int16_t		dev_handle;
	u_int8_t		phy_num;
	u_int8_t		type;
#define MPII_DEV_TYPE_PD	(0x00)
#define MPII_DEV_TYPE_VD_PD	(0x01)
#define MPII_DEV_TYPE_VD	(0x02)
	
	u_int16_t		enclosure_handle;

	u_int16_t		expander_dev_handle;
	u_int8_t		physical_port;

	u_int32_t		wwid;

	u_int32_t		flags;
#define MPII_DEV_HIDDEN		(1<<0)
#define MPII_DEV_INTERFACE_SAS	(1<<4)
#define MPII_DEV_INTERFACE_SATA	(1<<5)
#define MPII_DEV_SCSI_ML_ATTACH (1<<8)
#define MPII_DEV_UNUSED		(1<<31)
};

int mpii_dev_cmp(struct mpii_device *, struct mpii_device *);

int
mpii_dev_cmp(struct mpii_device *a, struct mpii_device *b)
{
	return (a->dev_handle < b->dev_handle ? -1 : 
	    a->dev_handle > b->dev_handle);
}

RB_HEAD(mpii_dev_tree, mpii_device) mpii_dev_head = 
    RB_INITIALIZER(&mpii_dev_head);
RB_PROTOTYPE(mpii_dev_tree, mpii_device, rb_entry, mpii_dev_cmp)
RB_GENERATE(mpii_dev_tree, mpii_device, rb_entry, mpii_dev_cmp)

struct mpii_ccb {
	struct mpii_softc	*ccb_sc;
	int			ccb_smid;

	struct scsi_xfer	*ccb_xs;
	bus_dmamap_t		ccb_dmamap;

	bus_addr_t		ccb_offset;
	void			*ccb_cmd;
	bus_addr_t		ccb_cmd_dva;
	u_int16_t		ccb_dev_handle;

	volatile enum {
		MPII_CCB_FREE,
		MPII_CCB_READY,
		MPII_CCB_QUEUED
	}			ccb_state;

	void			(*ccb_done)(struct mpii_ccb *);
	struct mpii_rcb		*ccb_rcb;

	TAILQ_ENTRY(mpii_ccb)	ccb_link;
};

TAILQ_HEAD(mpii_ccb_list, mpii_ccb);

struct mpii_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;

	int			sc_flags;
#define MPII_F_RAID		(1<<1)

	struct scsibus_softc	*sc_scsibus;

	struct mpii_device	**sc_mpii_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	u_int8_t		sc_porttype;
	int			sc_request_depth;
	int			sc_num_reply_frames;
	int			sc_reply_free_qdepth;
	int			sc_reply_post_qdepth;
	int			sc_maxchdepth;
	int			sc_first_sgl_len;
	int			sc_chain_len;
	int			sc_max_sgl_len;
	
	u_int8_t		sc_ir_firmware;
	u_int8_t		sc_ioc_event_replay;
	u_int16_t		sc_max_enclosures;
	u_int16_t		sc_max_expanders;
	u_int8_t		sc_max_volumes;
	u_int16_t		sc_max_devices;
	u_int16_t		sc_max_dpm_entries;
	u_int8_t		sc_dpm_enabled;
	u_int8_t		sc_num_reserved_entries;
	u_int8_t		sc_reserve_tid0;
	u_int16_t		sc_vd_id_low;
	u_int16_t		sc_vd_id_hi;
	u_int16_t		sc_pd_id_start;
	u_int16_t		sc_vd_count;
	u_int8_t		sc_num_channels;
	/* XXX not sure these below will stay */
	u_int8_t		sc_num_enclosure_table_entries;
	u_int32_t		sc_pending_map_events;
	u_int8_t		sc_tracking_map_events;
	int			sc_discovery_in_progress;
	/* end list of likely to be cut entries */
	int			sc_target;
	int			sc_ioc_number;
	u_int8_t		sc_vf_id;
	u_int8_t		sc_num_ports;

	struct mpii_ccb		*sc_ccbs;
	struct mpii_ccb_list	sc_ccb_free;

	struct mpii_dmamem	*sc_requests;

	struct mpii_dmamem	*sc_replies;
	struct mpii_rcb		*sc_rcbs;

	struct mpii_dmamem	*sc_reply_postq;
	struct mpii_reply_descriptor	*sc_reply_postq_kva;	
	int			sc_reply_post_host_index;

	struct mpii_dmamem	*sc_reply_freeq;
	int			sc_reply_free_host_index;
	
	size_t			sc_fw_len;
	struct mpii_dmamem	*sc_fw;

	/* scsi ioctl from sd device */
	int			(*sc_ioctl)(struct device *, u_long, caddr_t);

	struct rwlock		sc_lock;
	struct mpii_cfg_hdr	sc_cfg_hdr;
	struct mpii_cfg_ioc_pg2	*sc_vol_page;
	struct mpii_cfg_raid_vol *sc_vol_list;
	struct mpii_cfg_raid_vol_pg0 *sc_rpg0;
	struct mpii_cfg_dpm_pg0	*sc_dpm_pg0;
	struct mpii_cfg_bios_pg2 *sc_bios_pg2;

	struct ksensor		*sc_sensors;
	struct ksensordev	sc_sensordev;
};

int	mpii_attach(struct mpii_softc *);
void	mpii_detach(struct mpii_softc *);
int	mpii_intr(void *);

int	mpii_pci_match(struct device *, void *, void *);
void	mpii_pci_attach(struct device *, struct device *, void *);
int	mpii_pci_detach(struct device *, int);

struct mpii_pci_softc {
	struct mpii_softc	psc_mpii;

	pci_chipset_tag_t	psc_pc;
	pcitag_t		psc_tag;

	void			*psc_ih;
};

struct cfattach mpii_pci_ca = {
	sizeof(struct mpii_pci_softc), mpii_pci_match, mpii_pci_attach,
	mpii_pci_detach
};

#define PREAD(s, r)	pci_conf_read((s)->psc_pc, (s)->psc_tag, (r))
#define PWRITE(s, r, v)	pci_conf_write((s)->psc_pc, (s)->psc_tag, (r), (v))

static const struct pci_matchid mpii_devices[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2008 } 
};

int
mpii_pci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, mpii_devices, nitems(mpii_devices)));
}

void
mpii_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct mpii_pci_softc		*psc = (void *)self;
	struct mpii_softc		*sc = &psc->psc_mpii;
	struct pci_attach_args		*pa = aux;
	pcireg_t			memtype;
	int				r;
	pci_intr_handle_t		ih;
	const char			*intrstr;
#ifdef __sparc64__
	int node;
#endif

	psc->psc_pc = pa->pa_pc;
	psc->psc_tag = pa->pa_tag;
	psc->psc_ih = NULL;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_ios = 0;
	sc->sc_target = -1;

	/* find the appropriate memory base */
	for (r = PCI_MAPREG_START; r < PCI_MAPREG_END; r += sizeof(memtype)) {
		memtype = pci_mapreg_type(psc->psc_pc, psc->psc_tag, r);
		if ((memtype & PCI_MAPREG_TYPE_MASK) == PCI_MAPREG_TYPE_MEM)
			break;
	}
	if (r >= PCI_MAPREG_END) {
		printf(": unable to locate system interface registers\n");
		return;
	}

	if (pci_mapreg_map(pa, r, memtype, 0, &sc->sc_iot, &sc->sc_ioh,
	    NULL, &sc->sc_ios, 0xFF) != 0) {
		printf(": unable to map system interface registers\n");
		return;
	}

	/* disable the expansion rom */
	PWRITE(psc, PCI_ROM_REG, PREAD(psc, PCI_ROM_REG) & ~PCI_ROM_ENABLE);

	/* hook up the interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	intrstr = pci_intr_string(psc->psc_pc, ih);
	psc->psc_ih = pci_intr_establish(psc->psc_pc, ih, IPL_BIO,
	    mpii_intr, sc, sc->sc_dev.dv_xname);
	if (psc->psc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		goto unmap;
	}
	printf(": %s", intrstr);
	
#ifdef __sparc64__
		/*
		 * Walk up the Open Firmware device tree until we find a
		 * "scsi-initiator-id" property.
		 */
		node = PCITAG_NODE(pa->pa_tag);
		while (node) {
			if (OF_getprop(node, "scsi-initiator-id",
			    &sc->sc_target, sizeof(sc->sc_target)) ==
			    sizeof(sc->sc_target))
				break;
			node = OF_parent(node);
#endif
	
	if (mpii_attach(sc) != 0) {
		/* error printed by mpii_attach */
		goto deintr;
	}

	return;

deintr:
	pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
	psc->psc_ih = NULL;
unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
mpii_pci_detach(struct device *self, int flags)
{
	struct mpii_pci_softc		*psc = (struct mpii_pci_softc *)self;
	struct mpii_softc		*sc = &psc->psc_mpii;

	mpii_detach(sc);

	if (psc->psc_ih != NULL) {
		pci_intr_disestablish(psc->psc_pc, psc->psc_ih);
		psc->psc_ih = NULL;
	}
	if (sc->sc_ios != 0) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
		sc->sc_ios = 0;
	}

	return (0);
}

struct cfdriver mpii_cd = {
	NULL,
	"mpii",
	DV_DULL
};

int		mpii_scsi_cmd(struct scsi_xfer *);
void		mpii_scsi_cmd_done(struct mpii_ccb *);
void		mpii_minphys(struct buf *bp, struct scsi_link *sl);
int		mpii_scsi_probe(struct scsi_link *);
int		mpii_scsi_ioctl(struct scsi_link *, u_long, caddr_t,
		    int, struct proc *);

struct scsi_adapter mpii_switch = {
	mpii_scsi_cmd,
	mpii_minphys, 
	mpii_scsi_probe, /* XXX JPG scsi_probe may prove useful for mapping nonsense */
	NULL,
	NULL
};

struct scsi_device mpii_dev = {
	NULL,
	NULL,
	NULL,
	NULL
};

struct mpii_dmamem 	*mpii_dmamem_alloc(struct mpii_softc *, size_t);
void		mpii_dmamem_free(struct mpii_softc *,
		    struct mpii_dmamem *);
int		mpii_alloc_ccbs(struct mpii_softc *);
struct mpii_ccb *mpii_get_ccb(struct mpii_softc *);
void		mpii_put_ccb(struct mpii_softc *, struct mpii_ccb *);
int		mpii_alloc_replies(struct mpii_softc *);
int		mpii_alloc_queues(struct mpii_softc *);
int		mpii_alloc_dev(struct mpii_softc *);
void		mpii_push_reply(struct mpii_softc *, u_int32_t);
void		mpii_push_replies(struct mpii_softc *);

void		mpii_start(struct mpii_softc *, struct mpii_ccb *);
int		mpii_complete(struct mpii_softc *, struct mpii_ccb *, int);
int		mpii_poll(struct mpii_softc *, struct mpii_ccb *, int);
int		mpii_reply(struct mpii_softc *);

void		mpii_init_queues(struct mpii_softc *);

void		mpii_timeout_xs(void *);
int		mpii_load_xs(struct mpii_ccb *);

u_int32_t	mpii_read(struct mpii_softc *, bus_size_t);
void		mpii_write(struct mpii_softc *, bus_size_t, u_int32_t);
int		mpii_wait_eq(struct mpii_softc *, bus_size_t, u_int32_t,
		    u_int32_t);
int		mpii_wait_ne(struct mpii_softc *, bus_size_t, u_int32_t,
		    u_int32_t);

int		mpii_init(struct mpii_softc *);
int		mpii_reset_soft(struct mpii_softc *);
int		mpii_reset_hard(struct mpii_softc *);

int		mpii_handshake_send(struct mpii_softc *, void *, size_t);
int		mpii_handshake_recv_dword(struct mpii_softc *,
		    u_int32_t *);
int		mpii_handshake_recv(struct mpii_softc *, void *, size_t);

void		mpii_empty_done(struct mpii_ccb *);

int		mpii_iocinit(struct mpii_softc *);
int		mpii_iocfacts(struct mpii_softc *);
int		mpii_portfacts(struct mpii_softc *);
int		mpii_portenable(struct mpii_softc *);
int			mpii_cfg_coalescing(struct mpii_softc *);
/*
void		mpii_get_raid(struct mpii_softc *);
int		mpii_fwupload(struct mpii_softc *);
*/
int		mpii_eventnotify(struct mpii_softc *);
void		mpii_eventnotify_done(struct mpii_ccb *);
void		mpii_eventack(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
void		mpii_eventack_done(struct mpii_ccb *);
void		mpii_event_process(struct mpii_softc *, 
		    struct mpii_msg_reply *);
void		mpii_event_process_sas_discovery(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
void		mpii_event_process_sas_topo_change(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
void		mpii_event_process_ir_cfg_change_list(struct mpii_softc *, 
		    struct mpii_msg_event_reply *);
void		mpii_event_process_ir_phys_disk(struct mpii_softc *,
		    struct mpii_msg_event_reply *);
/*
void		mpii_evt_sas(void *, void *);
*/
int		mpii_req_cfg_header(struct mpii_softc *, u_int8_t,
		    u_int8_t, u_int32_t, int, void *);
int		mpii_req_cfg_page(struct mpii_softc *, u_int32_t, int,
		    void *, int, void *, size_t);

int		mpii_get_ioc_pg8(struct mpii_softc *);
void		mpii_get_raid_config_pg0(struct mpii_softc *);
int		mpii_get_dpm(struct mpii_softc *);
int		mpii_get_dpm_pg0(struct mpii_softc *, struct mpii_cfg_dpm_pg0 *);
int 		mpii_get_bios_pg2(struct mpii_softc *);

int		mpii_dev_reorder(struct mpii_softc *);
void		mpii_reorder_vds(struct mpii_softc *);
void		mpii_reorder_boot_device(struct mpii_softc *);

#if NBIO > 0
int		mpii_bio_get_pg0_raid(struct mpii_softc *, int);
int		mpii_ioctl(struct device *, u_long, caddr_t);
int		mpii_ioctl_inq(struct mpii_softc *, struct bioc_inq *);
int		mpii_ioctl_vol(struct mpii_softc *, struct bioc_vol *);
int		mpii_ioctl_disk(struct mpii_softc *, struct bioc_disk *);
int		mpii_ioctl_setstate(struct mpii_softc *, struct bioc_setstate *);
#ifndef SMALL_KERNEL
int		mpii_create_sensors(struct mpii_softc *);
void		mpii_refresh_sensors(void *);
#endif /* SMALL_KERNEL */
#endif /* NBIO > 0 */

#define DEVNAME(s)		((s)->sc_dev.dv_xname)

#define dwordsof(s)		(sizeof(s) / sizeof(u_int32_t))
#define dwordn(p, n)		(((u_int32_t *)(p))[(n)])

#define mpii_read_db(s)		mpii_read((s), MPII_DOORBELL)
#define mpii_write_db(s, v)	mpii_write((s), MPII_DOORBELL, (v))
#define mpii_read_intr(s)	mpii_read((s), MPII_INTR_STATUS)
#define mpii_write_intr(s, v)	mpii_write((s), MPII_INTR_STATUS, (v))
#define mpii_reply_waiting(s)	((mpii_read_intr((s)) & MPII_INTR_STATUS_REPLY)\
				    == MPII_INTR_STATUS_REPLY)

#define mpii_read_reply_free(s, v)	mpii_read((s), \
						MPII_REPLY_FREE_HOST_INDEX)
#define mpii_write_reply_free(s, v)	mpii_write((s), \
						MPII_REPLY_FREE_HOST_INDEX, (v))
#define mpii_read_reply_post(s, v)	mpii_read((s), \
						MPII_REPLY_POST_HOST_INDEX)
#define mpii_write_reply_post(s, v)	mpii_write((s), \
						MPII_REPLY_POST_HOST_INDEX, (v))

#define mpii_wait_db_int(s)	mpii_wait_ne((s), MPII_INTR_STATUS, \
				    MPII_INTR_STATUS_IOC2SYSDB, 0)
#define mpii_wait_db_ack(s)	mpii_wait_eq((s), MPII_INTR_STATUS, \
				    MPII_INTR_STATUS_SYS2IOCDB, 0)

#define MPII_PG_EXTENDED	(1<<0)
#define MPII_PG_POLL		(1<<1)
#define MPII_PG_FMT		"\020" "\002POLL" "\001EXTENDED"

#define mpii_cfg_header(_s, _t, _n, _a, _h) \
	mpii_req_cfg_header((_s), (_t), (_n), (_a), \
	    MPII_PG_POLL, (_h))
#define mpii_ecfg_header(_s, _t, _n, _a, _h) \
	mpii_req_cfg_header((_s), (_t), (_n), (_a), \
	    MPII_PG_POLL|MPII_PG_EXTENDED, (_h))

#define mpii_cfg_page(_s, _a, _h, _r, _p, _l) \
	mpii_req_cfg_page((_s), (_a), MPII_PG_POLL, \
	    (_h), (_r), (_p), (_l))
#define mpii_ecfg_page(_s, _a, _h, _r, _p, _l) \
	mpii_req_cfg_page((_s), (_a), MPII_PG_POLL|MPII_PG_EXTENDED, \
	    (_h), (_r), (_p), (_l))

int
mpii_attach(struct mpii_softc *sc)
{
	struct scsibus_attach_args	saa;
	struct mpii_ccb			*ccb;

	printf("\n");

	/* disable interrupts */
	mpii_write(sc, MPII_INTR_MASK, 
	    MPII_INTR_MASK_RESET | MPII_INTR_MASK_REPLY
	    | MPII_INTR_MASK_DOORBELL);

	if (mpii_init(sc) != 0) {
		printf("%s: unable to initialize ioc\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_iocfacts(sc) != 0) {
		printf("%s: unable to get iocfacts\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_alloc_ccbs(sc) != 0) {
		/* error already printed */
		return(1);
	}
	
	if (mpii_alloc_replies(sc) != 0) {
		printf("%s: unable to allocated reply space\n", DEVNAME(sc));
		goto free_ccbs;
	}

	if (mpii_alloc_queues(sc) != 0) {
		printf("%s: unable to allocate reply queues\n", DEVNAME(sc));
		goto free_replies;
	}

	if (mpii_iocinit(sc) != 0) {
		printf("%s: unable to send iocinit\n", DEVNAME(sc));
		goto free_queues;
	}

	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_OPER) != 0) {
		printf("%s: state: 0x%08x\n", DEVNAME(sc),
			mpii_read_db(sc) & MPII_DOORBELL_STATE);
		printf("%s: operational state timeout\n", DEVNAME(sc));
		goto free_queues;
	}

	mpii_push_replies(sc);
	mpii_init_queues(sc);

	if (mpii_portfacts(sc) != 0) {
		printf("%s: unable to get portfacts\n", DEVNAME(sc));
		goto free_queues;
	}	

	if (mpii_get_ioc_pg8(sc) != 0) {
		printf("%s: unable to get ioc page 8\n", DEVNAME(sc));
		goto free_queues;
	}

	if (mpii_get_dpm(sc) != 0) {
		printf("%s: unable to get driver persistent mapping\n",
		    DEVNAME(sc));
		goto free_queues;
	}

	if (mpii_cfg_coalescing(sc) != 0) {
		printf("%s: unable to configure coalescing\n", DEVNAME(sc));
		goto free_dpm;
	}

	/* XXX bail on unsupported porttype? */
	if ((sc->sc_porttype == MPII_PORTFACTS_PORTTYPE_SAS_PHYSICAL) || 
		(sc->sc_porttype == MPII_PORTFACTS_PORTTYPE_SAS_VIRTUAL)) {
		if (mpii_eventnotify(sc) != 0) {
			printf("%s: unable to enable events\n", DEVNAME(sc));
			goto free_dpm;
		}
	}

	if (mpii_alloc_dev(sc) != 0) {
		printf("%s: unable to allocate memory for mpii_dev\n", 
		    DEVNAME(sc));
		goto free_dpm;
	}
	
	/* enable interrupts */
	mpii_write(sc, MPII_INTR_MASK, MPII_INTR_MASK_DOORBELL 
	    | MPII_INTR_MASK_RESET);
	
	if (mpii_portenable(sc) != 0) {
		printf("%s: unable to enable port\n", DEVNAME(sc));
		goto free_dev;
	} /* assume all discovery events are complete by now */

	if (sc->sc_discovery_in_progress)
		printf("%s: warning: discovery still in progress\n", 
		    DEVNAME(sc));

	if (mpii_get_bios_pg2(sc) != 0) {
		printf("%s: unable to get bios page 2\n", DEVNAME(sc));
		goto free_dev;	
	}

	if (mpii_dev_reorder(sc) != 0) {
		/* error already printed */
		goto free_bios_pg2;
	}

	/* XXX
	if (mpii_fwupload(sc) != 0) {
		printf("%s: unabel to upload firmware\n", DEVNAME(sc));
		goto free_replies;
	}
	*/

	rw_init(&sc->sc_lock, "mpii_lock");

	/* we should be good to go now, attach scsibus */
	sc->sc_link.device = &mpii_dev;
	sc->sc_link.adapter = &mpii_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_target;
	sc->sc_link.adapter_buswidth = sc->sc_max_devices;
	sc->sc_link.openings = sc->sc_request_depth / sc->sc_max_devices;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	/* config_found() returns the scsibus attached to us */
	sc->sc_scsibus = (struct scsibus_softc *) config_found(&sc->sc_dev,
		&saa, scsiprint);

	return (0);

free_bios_pg2:
	if (sc->sc_bios_pg2)
		free(sc->sc_bios_pg2, M_TEMP);

free_dev:
	if (sc->sc_mpii_dev)
		free(sc->sc_mpii_dev, M_DEVBUF);

free_dpm:
	if (sc->sc_dpm_pg0)
		free(sc->sc_dpm_pg0, M_DEVBUF);

free_queues:
	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_freeq),
     	    0, sc->sc_reply_free_qdepth * 4, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_reply_freeq);

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq),
	    0, sc->sc_reply_post_qdepth * 8, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_reply_postq);

free_replies:
	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_replies),
		0, PAGE_SIZE, BUS_DMASYNC_POSTREAD);
	mpii_dmamem_free(sc, sc->sc_replies);

free_ccbs:
	while ((ccb = mpii_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	mpii_dmamem_free(sc, sc->sc_requests);
	free(sc->sc_ccbs, M_DEVBUF);

	return(1);
}

void
mpii_detach(struct mpii_softc *sc)
{

}

int
mpii_intr(void *arg)
{
	struct mpii_softc	*sc = arg;

	if (mpii_reply(sc) < 0)
		return (0);

	while (mpii_reply(sc) >= 0)
		;
	
	mpii_write_reply_post(sc, sc->sc_reply_post_host_index);

	return (1);
}

void
mpii_timeout_xs(void *arg)
{
/* XXX */
}

int
mpii_load_xs(struct mpii_ccb *ccb)
{
	struct mpii_softc	*sc = ccb->ccb_sc;
	struct scsi_xfer	*xs = ccb->ccb_xs;
	struct mpii_ccb_bundle	*mcb = ccb->ccb_cmd;
	struct mpii_msg_scsi_io	*io = &mcb->mcb_io;
	struct mpii_sge		*sge, *nsge = &mcb->mcb_sgl[0];
	struct mpii_sge		*ce = NULL, *nce;
	u_int64_t		ce_dva;
	bus_dmamap_t		dmap = ccb->ccb_dmamap;
	u_int32_t		addr, flags;
	int			i, error;

	/* zero length transfer still requires an SGE */
	if (xs->datalen == 0) {
		nsge->sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
		    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL);
		return (0);
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xs->data, xs->datalen, NULL,
	    (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: error %d loading dmamap\n", DEVNAME(sc), error);
		return (1);
	}

	/* safe default staring flags */
	flags = MPII_SGE_FL_TYPE_SIMPLE | MPII_SGE_FL_SIZE_64;
	/* if data out */
	if (xs->flags & SCSI_DATA_OUT)
		flags |= MPII_SGE_FL_DIR_OUT;

	/* we will have to exceed the SGEs we can cram into the request frame */
	if (dmap->dm_nsegs > sc->sc_first_sgl_len) {
		ce = &mcb->mcb_sgl[sc->sc_first_sgl_len - 1];
		io->chain_offset = ((u_int8_t *)ce - (u_int8_t *)io) / 4;
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {

		if (nsge == ce) {
			nsge++;
			sge->sg_hdr |= htole32(MPII_SGE_FL_LAST);

			DNPRINTF(MPII_D_DMA, "%s:   - 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), sge->sg_hdr,
			    sge->sg_hi_addr, sge->sg_lo_addr);

			if ((dmap->dm_nsegs - i) > sc->sc_chain_len) {
				nce = &nsge[sc->sc_chain_len - 1];
				addr = ((u_int8_t *)nce - (u_int8_t *)nsge) / 4;
				addr = addr << 16 |
				    sizeof(struct mpii_sge) * sc->sc_chain_len;
			} else {
				nce = NULL;
				addr = sizeof(struct mpii_sge) *
				    (dmap->dm_nsegs - i);
			}

			ce->sg_hdr = htole32(MPII_SGE_FL_TYPE_CHAIN |
			    MPII_SGE_FL_SIZE_64 | addr);

			ce_dva = ccb->ccb_cmd_dva +
			    ((u_int8_t *)nsge - (u_int8_t *)mcb);

			addr = (u_int32_t)(ce_dva >> 32);
			ce->sg_hi_addr = htole32(addr);
			addr = (u_int32_t)ce_dva;
			ce->sg_lo_addr = htole32(addr);

			DNPRINTF(MPII_D_DMA, "%s:  ce: 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), ce->sg_hdr, ce->sg_hi_addr,
			    ce->sg_lo_addr);

			ce = nce;
		}

		DNPRINTF(MPII_D_DMA, "%s:  %d: %d 0x%016llx\n", DEVNAME(sc),
		    i, dmap->dm_segs[i].ds_len,
		    (u_int64_t)dmap->dm_segs[i].ds_addr);

		sge = nsge;

		sge->sg_hdr = htole32(flags | dmap->dm_segs[i].ds_len);
		addr = (u_int32_t)((u_int64_t)dmap->dm_segs[i].ds_addr >> 32);
		sge->sg_hi_addr = htole32(addr);
		addr = (u_int32_t)dmap->dm_segs[i].ds_addr;
		sge->sg_lo_addr = htole32(addr);

		DNPRINTF(MPII_D_DMA, "%s:  %d: 0x%08x 0x%08x 0x%08x\n",
		    DEVNAME(sc), i, sge->sg_hdr, sge->sg_hi_addr,
		    sge->sg_lo_addr);

		nsge = sge + 1;
	}

	/* terminate list */
	sge->sg_hdr |= htole32(MPII_SGE_FL_LAST | MPII_SGE_FL_EOB |
	    MPII_SGE_FL_EOL);

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

void
mpii_minphys(struct buf *bp, struct scsi_link *sl)
{
	/* XXX */
	minphys(bp);
}

int
mpii_scsi_probe(struct scsi_link *link)
{
	struct mpii_softc	*sc = link->adapter_softc;
	struct mpii_device	*device;
	u_int8_t		flags;

	if ((sc->sc_porttype != MPII_PORTFACTS_PORTTYPE_SAS_PHYSICAL) && 
		(sc->sc_porttype != MPII_PORTFACTS_PORTTYPE_SAS_VIRTUAL)) 
		return (1);

	if (sc->sc_mpii_dev[link->target] == NULL)
		return (1);
		
	device = (struct mpii_device *)sc->sc_mpii_dev[link->target];
	flags = device->flags;	
	if ((flags & MPII_DEV_UNUSED) || (flags & MPII_DEV_HIDDEN))
		return (1);

	sc->sc_mpii_dev[link->target]->flags |= MPII_DEV_SCSI_ML_ATTACH;

	return (0);
}

u_int32_t
mpii_read(struct mpii_softc *sc, bus_size_t r)
{
	u_int32_t			rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MPII_D_RW, "%s: mpii_read %#x %#x\n", DEVNAME(sc), r, rv);

	return (rv);
}

void
mpii_write(struct mpii_softc *sc, bus_size_t r, u_int32_t v)
{
	DNPRINTF(MPII_D_RW, "%s: mpii_write %#x %#x\n", DEVNAME(sc), r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}


int
mpii_wait_eq(struct mpii_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int			i;

	DNPRINTF(MPII_D_RW, "%s: mpii_wait_eq %#x %#x %#x\n", DEVNAME(sc), r,
	    mask, target);

	for (i = 0; i < 15000; i++) {
		if ((mpii_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
mpii_wait_ne(struct mpii_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int			i;

	DNPRINTF(MPII_D_RW, "%s: mpii_wait_ne %#x %#x %#x\n", DEVNAME(sc), r,
	    mask, target);

	for (i = 0; i < 15000; i++) {
		if ((mpii_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}


int
mpii_init(struct mpii_softc *sc)
{
	u_int32_t		db;
	int			i;

	/* spin until the ioc leaves the reset state */
	if (mpii_wait_ne(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_RESET) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_init timeout waiting to leave "
		    "reset state\n", DEVNAME(sc));
		return (1);
	}

	/* check current ownership */
	db = mpii_read_db(sc);
	if ((db & MPII_DOORBELL_WHOINIT) == MPII_DOORBELL_WHOINIT_PCIPEER) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_init initialised by pci peer\n",
		    DEVNAME(sc));
		return (0);
	}

	for (i = 0; i < 5; i++) {
		switch (db & MPII_DOORBELL_STATE) {
		case MPII_DOORBELL_STATE_READY:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is ready\n",
			    DEVNAME(sc));
			return (0);

		case MPII_DOORBELL_STATE_OPER:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is oper\n",
			    DEVNAME(sc));
			if (sc->sc_ioc_event_replay)
				mpii_reset_soft(sc);
			else
				mpii_reset_hard(sc);
			break;

		case MPII_DOORBELL_STATE_FAULT:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init ioc is being "
			    "reset hard\n" , DEVNAME(sc));
			mpii_reset_hard(sc);
			break;

		case MPII_DOORBELL_STATE_RESET:
			DNPRINTF(MPII_D_MISC, "%s: mpii_init waiting to come "
			    "out of reset\n", DEVNAME(sc));
			if (mpii_wait_ne(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
			    MPII_DOORBELL_STATE_RESET) != 0)
				return (1);
			break;
		}
		db = mpii_read_db(sc);
	}

	return (1);
}

int
mpii_reset_soft(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s: mpii_reset_soft\n", DEVNAME(sc));

	if (mpii_read_db(sc) & MPII_DOORBELL_INUSE) {
		return (1);
	}

	mpii_write_db(sc,
	    MPII_DOORBELL_FUNCTION(MPII_FUNCTION_IOC_MESSAGE_UNIT_RESET));
	
	/* XXX LSI waits 15 sec */
	if (mpii_wait_db_ack(sc) != 0)
		return (1);

	/* XXX LSI waits 15 sec */
	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_STATE,
	    MPII_DOORBELL_STATE_READY) != 0)
		return (1);

	/* XXX wait for Sys2IOCDB bit to clear in HIS?? */

	return (0);
}

int
mpii_reset_hard(struct mpii_softc *sc)
{
	u_int16_t		i;

	DNPRINTF(MPII_D_MISC, "%s: mpii_reset_hard\n", DEVNAME(sc));

	mpii_write_intr(sc, 0);

	/* enable diagnostic register */
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_FLUSH);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_1);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_2);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_3);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_4);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_5);
	mpii_write(sc, MPII_WRITESEQ, MPII_WRITESEQ_6);

	delay(100);

	if ((mpii_read(sc, MPII_HOSTDIAG) & MPII_HOSTDIAG_DWRE) == 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_reset_hard failure to enable "
		    "diagnostic read/write\n", DEVNAME(sc));
		return(1);
	}

	/* reset ioc */
	mpii_write(sc, MPII_HOSTDIAG, MPII_HOSTDIAG_RESET_ADAPTER);

	/* 200 milliseconds */
	delay(200000);


	/* XXX this whole function should be more robust */
	
	/* XXX  read the host diagnostic reg until reset adapter bit clears ? */
	for (i = 0; i < 30000; i++) {
		if ((mpii_read(sc, MPII_HOSTDIAG) & 
		    MPII_HOSTDIAG_RESET_ADAPTER) == 0)
			break;
		delay(10000);
	}

	/* disable diagnostic register */
	mpii_write(sc, MPII_WRITESEQ, 0xff);

	/* XXX what else? */

	DNPRINTF(MPII_D_MISC, "%s: done with mpii_reset_hard\n", DEVNAME(sc));

	return(0);
}

int
mpii_handshake_send(struct mpii_softc *sc, void *buf, size_t dwords)
{
	u_int32_t		*query = buf;
	int			i;

	/* make sure the doorbell is not in use. */
	if (mpii_read_db(sc) & MPII_DOORBELL_INUSE)
		return (1);

	/* clear pending doorbell interrupts */
	if (mpii_read_intr(sc) & MPII_INTR_STATUS_IOC2SYSDB)
		mpii_write_intr(sc, 0);

	/*
	 * first write the doorbell with the handshake function and the
	 * dword count.
	 */
	mpii_write_db(sc, MPII_DOORBELL_FUNCTION(MPII_FUNCTION_HANDSHAKE) |
	    MPII_DOORBELL_DWORDS(dwords));

	/*
	 * the doorbell used bit will be set because a doorbell function has
	 * started. wait for the interrupt and then ack it.
	 */
	if (mpii_wait_db_int(sc) != 0)
		return (1);
	mpii_write_intr(sc, 0);

	/* poll for the acknowledgement. */
	if (mpii_wait_db_ack(sc) != 0)
		return (1);

	/* write the query through the doorbell. */
	for (i = 0; i < dwords; i++) {
		mpii_write_db(sc, htole32(query[i]));
		if (mpii_wait_db_ack(sc) != 0)
			return (1);
	}

	return (0);
}

int
mpii_handshake_recv_dword(struct mpii_softc *sc, u_int32_t *dword)
{
	u_int16_t		*words = (u_int16_t *)dword;
	int			i;

	for (i = 0; i < 2; i++) {
		if (mpii_wait_db_int(sc) != 0)
			return (1);
		words[i] = letoh16(mpii_read_db(sc) & MPII_DOORBELL_DATA_MASK);
		mpii_write_intr(sc, 0);
	}

	return (0);
}

int
mpii_handshake_recv(struct mpii_softc *sc, void *buf, size_t dwords)
{
	struct mpii_msg_reply	*reply = buf;
	u_int32_t		*dbuf = buf, dummy;
	int			i;

	/* get the first dword so we can read the length out of the header. */
	if (mpii_handshake_recv_dword(sc, &dbuf[0]) != 0)
		return (1);

	DNPRINTF(MPII_D_CMD, "%s: mpii_handshake_recv dwords: %d reply: %d\n",
	    DEVNAME(sc), dwords, reply->msg_length);

	/*
	 * the total length, in dwords, is in the message length field of the
	 * reply header.
	 */
	for (i = 1; i < MIN(dwords, reply->msg_length); i++) {
		if (mpii_handshake_recv_dword(sc, &dbuf[i]) != 0)
			return (1);
	}

	/* if there's extra stuff to come off the ioc, discard it */
	while (i++ < reply->msg_length) {
		if (mpii_handshake_recv_dword(sc, &dummy) != 0)
			return (1);
		DNPRINTF(MPII_D_CMD, "%s: mpii_handshake_recv dummy read: "
		    "0x%08x\n", DEVNAME(sc), dummy);
	}

	/* wait for the doorbell used bit to be reset and clear the intr */
	if (mpii_wait_db_int(sc) != 0)
		return (1);
	
	if (mpii_wait_eq(sc, MPII_DOORBELL, MPII_DOORBELL_INUSE, 0) != 0)
		return (1);
	 
	mpii_write_intr(sc, 0);

	return (0);
}

void
mpii_empty_done(struct mpii_ccb *ccb)
{
	/* nothing to do */
}

int
mpii_iocfacts(struct mpii_softc *sc)
{
	struct mpii_msg_iocfacts_request	ifq;
	struct mpii_msg_iocfacts_reply		ifp;

	DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts\n", DEVNAME(sc));

	bzero(&ifq, sizeof(ifq));
	bzero(&ifp, sizeof(ifp));

	ifq.function = MPII_FUNCTION_IOC_FACTS;

	if (mpii_handshake_send(sc, &ifq, dwordsof(ifq)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpii_handshake_recv(sc, &ifp, dwordsof(ifp)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocfacts recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s:  func: 0x%02x length: %d msgver: %d.%d\n",
	    DEVNAME(sc), ifp.function, ifp.msg_length,
	    ifp.msg_version_maj, ifp.msg_version_min);
	DNPRINTF(MPII_D_MISC, "%s:  msgflags: 0x%02x iocnumber: 0x%02x "
	    "headerver: %d.%d\n", DEVNAME(sc), ifp.msg_flags,
	    ifp.ioc_number, ifp.header_version_unit,
	    ifp.header_version_dev);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    ifp.vp_id, ifp.vf_id);
	DNPRINTF(MPII_D_MISC, "%s:  iocstatus: 0x%04x ioexceptions: 0x%04x\n",
	    DEVNAME(sc), letoh16(ifp.ioc_status),
	    letoh16(ifp.ioc_exceptions));
	DNPRINTF(MPII_D_MISC, "%s:  iocloginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  numberofports: 0x%02x whoinit: 0x%02x "
	    "maxchaindepth: %d\n", DEVNAME(sc), ifp.number_of_ports,
	    ifp.whoinit, ifp.max_chain_depth);
	DNPRINTF(MPII_D_MISC, "%s:  productid: 0x%04x requestcredit: 0x%04x\n", 
	    DEVNAME(sc), letoh16(ifp.product_id), letoh16(ifp.request_credit));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_capabilities: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.ioc_capabilities));
	DNPRINTF(MPII_D_MISC, "%s:  fw_version: %d.%d fw_version_unit: 0x%02x "
	    "fw_version_dev: 0x%02x\n", DEVNAME(sc),
	    ifp.fw_version_maj, ifp.fw_version_min,
	    ifp.fw_version_unit, ifp.fw_version_dev);
	DNPRINTF(MPII_D_MISC, "%s:  iocrequestframesize: 0x%04x\n",
	    DEVNAME(sc), letoh16(ifp.ioc_request_frame_size));
	DNPRINTF(MPII_D_MISC, "%s:  maxtargets: 0x%04x " 
	    "maxinitiators: 0x%04x\n", DEVNAME(sc),
	    letoh16(ifp.max_targets), letoh16(ifp.max_initiators));
	DNPRINTF(MPII_D_MISC, "%s:  maxenclosures: 0x%04x "
	    "maxsasexpanders: 0x%04x\n", DEVNAME(sc),
	    letoh16(ifp.max_enclosures), letoh16(ifp.max_sas_expanders));
	DNPRINTF(MPII_D_MISC, "%s:  highprioritycredit: 0x%04x "
	    "protocolflags: 0x%02x\n", DEVNAME(sc),
	    letoh16(ifp.high_priority_credit), letoh16(ifp.protocol_flags));
	DNPRINTF(MPII_D_MISC, "%s:  maxvolumes: 0x%02x replyframesize: 0x%02x "
	    "mrdpqd: 0x%04x\n", DEVNAME(sc), ifp.max_volumes,
	    ifp.reply_frame_size,
	    letoh16(ifp.max_reply_descriptor_post_queue_depth));
	DNPRINTF(MPII_D_MISC, "%s:  maxpersistententries: 0x%04x "
	    "maxdevhandle: 0x%02x\n", DEVNAME(sc),
	    letoh16(ifp.max_persistent_entries), letoh16(ifp.max_dev_handle));

	sc->sc_maxchdepth = ifp.max_chain_depth;
	sc->sc_ioc_number = ifp.ioc_number;
	sc->sc_vf_id = ifp.vf_id;
	
	sc->sc_num_ports = ifp.number_of_ports;
	sc->sc_ir_firmware = (letoh32(ifp.ioc_capabilities) & 
	    MPII_IOCFACTS_CAPABILITY_INTEGRATED_RAID) ? 1 : 0;    
	sc->sc_ioc_event_replay = (letoh32(ifp.ioc_capabilities) &
	    MPII_IOCFACTS_CAPABILITY_EVENT_REPLAY) ? 1 : 0;
	sc->sc_max_enclosures = letoh16(ifp.max_enclosures);
	sc->sc_max_expanders = letoh16(ifp.max_sas_expanders);
	sc->sc_max_volumes = ifp.max_volumes;
	sc->sc_max_devices = ifp.max_volumes + letoh16(ifp.max_targets);
	sc->sc_num_channels = 1;
	
	sc->sc_request_depth = MIN(letoh16(ifp.request_credit),
	    MPII_MAX_REQUEST_CREDIT);

	/* should not be multiple of 16 */ 
	sc->sc_num_reply_frames = sc->sc_request_depth + 32;
	if (!(sc->sc_num_reply_frames % 16))
		sc->sc_num_reply_frames--;

	/* must be multiple of 16 */
	sc->sc_reply_free_qdepth = sc->sc_num_reply_frames + 
	    (16 - (sc->sc_num_reply_frames % 16));

	sc->sc_reply_post_qdepth = sc->sc_request_depth +
	    sc->sc_num_reply_frames + 1;

	if (sc->sc_reply_post_qdepth > MPII_MAX_REPLY_POST_QDEPTH)
		sc->sc_reply_post_qdepth = MPII_MAX_REPLY_POST_QDEPTH;
	
	if (sc->sc_reply_post_qdepth > 
	    ifp.max_reply_descriptor_post_queue_depth)
		sc->sc_reply_post_qdepth = 
		    ifp.max_reply_descriptor_post_queue_depth;

	/* XXX JPG temporary override of calculated values.
	 *         need to think this through as the specs
	 *         and other existing drivers contradict
	 */
	sc->sc_reply_post_qdepth = 128;
	sc->sc_request_depth = 128;
	sc->sc_num_reply_frames = 63;
	sc->sc_reply_free_qdepth = 64;
	
	DNPRINTF(MPII_D_MISC, "%s: sc_request_depth: %d "
	    "sc_num_reply_frames: %d sc_reply_free_qdepth: %d "
	    "sc_reply_post_qdepth: %d\n", DEVNAME(sc), sc->sc_request_depth,
	    sc->sc_num_reply_frames, sc->sc_reply_free_qdepth,
	    sc->sc_reply_post_qdepth);

	/*
	 * you can fit sg elements on the end of the io cmd if they fit in the
	 * request frame size.
	 */

	sc->sc_first_sgl_len = ((letoh16(ifp.ioc_request_frame_size) * 4) -
	    sizeof(struct mpii_msg_scsi_io)) / sizeof(struct mpii_sge);
	DNPRINTF(MPII_D_MISC, "%s:   first sgl len: %d\n", DEVNAME(sc),
	    sc->sc_first_sgl_len);

	sc->sc_chain_len = (letoh16(ifp.ioc_request_frame_size) * 4) /
	    sizeof(struct mpii_sge);
	DNPRINTF(MPII_D_MISC, "%s:   chain len: %d\n", DEVNAME(sc),
	    sc->sc_chain_len);

	/* the sgl tailing the io cmd loses an entry to the chain element. */
	sc->sc_max_sgl_len = MPII_MAX_SGL - 1;
	/* the sgl chains lose an entry for each chain element */
	sc->sc_max_sgl_len -= (MPII_MAX_SGL - sc->sc_first_sgl_len) /
	    sc->sc_chain_len;
	DNPRINTF(MPII_D_MISC, "%s:   max sgl len: %d\n", DEVNAME(sc),
	    sc->sc_max_sgl_len);

	/* XXX we're ignoring the max chain depth */

	return(0);

}

int
mpii_iocinit(struct mpii_softc *sc)
{
	struct mpii_msg_iocinit_request		iiq;
	struct mpii_msg_iocinit_reply		iip;
	u_int32_t				hi_addr;

	DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit\n", DEVNAME(sc));

	bzero(&iiq, sizeof(iiq));
	bzero(&iip, sizeof(iip));

	iiq.function = MPII_FUNCTION_IOC_INIT;
	iiq.whoinit = MPII_WHOINIT_HOST_DRIVER;
	
	/* XXX JPG do something about vf_id */
	iiq.vf_id = 0;

	iiq.msg_version_maj = 0x02;
	iiq.msg_version_min = 0x00;

	/* XXX JPG ensure compliance with some level and hard-code? */
	iiq.hdr_version_unit = 0x00;
	iiq.hdr_version_dev = 0x00;

	iiq.system_request_frame_size = htole16(MPII_REQUEST_SIZE / 4);

	iiq.reply_descriptor_post_queue_depth = 
	    htole16(sc->sc_reply_post_qdepth);

	iiq.reply_free_queue_depth = htole16(sc->sc_reply_free_qdepth);
	
	hi_addr = (u_int32_t)((u_int64_t)MPII_DMA_DVA(sc->sc_requests) >> 32);
	iiq.sense_buffer_address_high = htole32(hi_addr);

	hi_addr = (u_int32_t)
	    ((u_int64_t)MPII_DMA_DVA(sc->sc_replies) >> 32);
	iiq.system_reply_address_high = hi_addr;

	iiq.system_request_frame_base_address = 
	    (u_int64_t)MPII_DMA_DVA(sc->sc_requests);

	iiq.reply_descriptor_post_queue_address =
	    (u_int64_t)MPII_DMA_DVA(sc->sc_reply_postq);

	iiq.reply_free_queue_address =
	    (u_int64_t)MPII_DMA_DVA(sc->sc_reply_freeq);

	if (mpii_handshake_send(sc, &iiq, dwordsof(iiq)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpii_handshake_recv(sc, &iip, dwordsof(iip)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_iocinit recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s:  function: 0x%02x msg_length: %d "
	    "whoinit: 0x%02x\n", DEVNAME(sc), iip.function,
	    iip.msg_length, iip.whoinit);
	DNPRINTF(MPII_D_MISC, "%s:  msg_flags: 0x%02x\n", DEVNAME(sc), 
	    iip.msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vf_id: 0x%02x vp_id: 0x%02x\n", DEVNAME(sc),
	    iip.vf_id, iip.vp_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(iip.ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(iip.ioc_loginfo));

	if ((iip.ioc_status != MPII_IOCSTATUS_SUCCESS) || (iip.ioc_loginfo))
		return (1);

	return (0);
}

void
mpii_push_reply(struct mpii_softc *sc, u_int32_t rdva)
{
	u_int32_t		*rfp;

	rfp = MPII_DMA_KVA(sc->sc_reply_freeq);
	rfp[sc->sc_reply_free_host_index] = rdva;

	sc->sc_reply_free_host_index = (sc->sc_reply_free_host_index + 1) %
	    sc->sc_reply_free_qdepth;

	mpii_write_reply_free(sc, sc->sc_reply_free_host_index);
}

int
mpii_portfacts(struct mpii_softc *sc)
{
	struct mpii_msg_portfacts_request	*pfq;
	struct mpii_msg_portfacts_reply		*pfp;
	int			rv = 1, s;
	struct mpii_ccb		*ccb;

	DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts\n", DEVNAME(sc));

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts mpii_get_ccb fail\n",
		    DEVNAME(sc));
		return (rv);
	}

	ccb->ccb_done = mpii_empty_done;
	pfq = ccb->ccb_cmd;
	
	bzero(pfq, sizeof(struct mpii_msg_portfacts_request));

	pfq->function = MPII_FUNCTION_PORT_FACTS;
	pfq->chain_offset = 0;
	pfq->msg_flags = 0;
	pfq->port_number = 0;
	pfq->vp_id = 0;
	pfq->vf_id = 0;

	if (mpii_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portfacts poll\n", DEVNAME(sc));
		goto err;
	}

	if (ccb->ccb_rcb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: empty portfacts reply\n",
		    DEVNAME(sc));
		goto err;
	}
	
	pfp = ccb->ccb_rcb->rcb_reply;
	DNPRINTF(MPII_D_MISC, "%s   pfp: 0x%04x\n", DEVNAME(sc), pfp);

	DNPRINTF(MPII_D_MISC, "%s:  function: 0x%02x msg_length: %d\n",
	    DEVNAME(sc), pfp->function, pfp->msg_length);
	DNPRINTF(MPII_D_MISC, "%s:  msg_flags: 0x%02x port_number: %d\n",
	    DEVNAME(sc), pfp->msg_flags, pfp->port_number);
	DNPRINTF(MPII_D_MISC, "%s:  vf_id: 0x%02x vp_id: 0x%02x\n", DEVNAME(sc),
	    pfp->vf_id, pfp->vp_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(pfp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(pfp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  port_type: 0x%02x\n", DEVNAME(sc), 
	    pfp->port_type);
	DNPRINTF(MPII_D_MISC, "%s:  max_posted_cmd_buffers: %d\n", DEVNAME(sc),
	    letoh16(pfp->max_posted_cmd_buffers));

	sc->sc_porttype = pfp->port_type;
	/* XXX JPG no such field in MPI2 .... but the dilemma is what
	 * to return to sc_link.adapter_target ... is -1 acceptable? fake 255?
	if (sc->sc_target == -1)
		sc->sc_target = letoh16(pfp->port_scsi_id);
	*/

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	rv = 0;
err:
	mpii_put_ccb(sc, ccb);

	return (rv);
}

void
mpii_eventack(struct mpii_softc *sc, struct mpii_msg_event_reply *enp)
{
	struct mpii_ccb				*ccb;
	struct mpii_msg_eventack_request	*eaq;

	ccb = mpii_get_ccb(sc);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_EVT, "%s: mpii_eventack ccb_get\n", DEVNAME(sc));
		return;
	}

	ccb->ccb_done = mpii_eventack_done;
	eaq = ccb->ccb_cmd;

	eaq->function = MPII_FUNCTION_EVENT_ACK;

	eaq->event = enp->event;
	eaq->event_context = enp->event_context;

	mpii_start(sc, ccb);
	return;
}

void
mpii_eventack_done(struct mpii_ccb *ccb)
{
	struct mpii_softc			*sc = ccb->ccb_sc;

	DNPRINTF(MPII_D_EVT, "%s: event ack done\n", DEVNAME(sc));

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);
}

int
mpii_portenable(struct mpii_softc *sc)
{
	struct mpii_msg_portenable_request	*peq;
	struct mpii_msg_portenable_repy		*pep;
	struct mpii_ccb		*ccb;
	int			s;

	DNPRINTF(MPII_D_MISC, "%s: mpii_portenable\n", DEVNAME(sc));

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portenable ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpii_empty_done;
	peq = ccb->ccb_cmd;

	peq->function = MPII_FUNCTION_PORT_ENABLE;
	peq->vf_id = sc->sc_vf_id;

	if (mpii_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_portenable poll\n", DEVNAME(sc));
		return (1);
	}

	if (ccb->ccb_rcb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: empty portenable reply\n",
		    DEVNAME(sc));
		return (1);
	}
	pep = ccb->ccb_rcb->rcb_reply;

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);

	return (0);
}

int
mpii_dev_reorder(struct mpii_softc *sc)
{
	if ((sc->sc_ir_firmware) && (sc->sc_vd_count))
		mpii_reorder_vds(sc);

	if (sc->sc_reserve_tid0)
		mpii_reorder_boot_device(sc);

	return (0);
}

void
mpii_reorder_vds(struct mpii_softc *sc)
{
	/* XXX need to implement
	 * ideas: VD creation order (still not sure what data to key off of),
	 *        member PD slot IDs,
	 *	  random fun
	 */
}

void
mpii_reorder_boot_device(struct mpii_softc *sc)
{
	/* mpii_get_boot_dev(sc) */
}

int
mpii_cfg_coalescing(struct mpii_softc *sc)
{
	struct mpii_cfg_hdr	hdr;
	struct mpii_cfg_ioc_pg1	pg;
	u_int32_t		flags;
	
	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_IOC, 1, 0, &hdr) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch IOC page 1 header\n",
		    DEVNAME(sc));
		return (1);
	}
	
	if (mpii_cfg_page(sc, 0, &hdr, 1, &pg, sizeof(pg)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to fetch IOC page 1\n"
		    "page 1\n", DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPII_D_MISC, "%s: IOC page 1\n", DEVNAME(sc));
	DNPRINTF(MPII_D_MISC, "%s:  flags: 0x08%x\n", DEVNAME(sc),
	    letoh32(pg.flags));
	DNPRINTF(MPII_D_MISC, "%s:  coalescing_timeout: %d\n", DEVNAME(sc),
	    letoh32(pg.coalescing_timeout));
	DNPRINTF(MPII_D_MISC, "%s:  coalescing_depth: %d pci_slot_num: %d\n",
	    DEVNAME(sc), pg.coalescing_timeout, pg.pci_slot_num);

	flags = letoh32(pg.flags);
	if (!ISSET(flags, MPII_CFG_IOC_1_REPLY_COALESCING))
		return (0);

	CLR(pg.flags, htole32(MPII_CFG_IOC_1_REPLY_COALESCING));
	if (mpii_cfg_page(sc, 0, &hdr, 0, &pg, sizeof(pg)) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: unable to clear coalescing\n",
		    DEVNAME(sc));
		return (1);
	}

	return (0);
}

int
mpii_eventnotify(struct mpii_softc *sc)
{
	struct mpii_ccb				*ccb;
	struct mpii_msg_event_request		*enq;
	int					s;

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_eventnotify ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpii_eventnotify_done;
	enq = ccb->ccb_cmd;

	enq->function = MPII_FUNCTION_EVENT_NOTIFICATION;

	/* enable reporting of the following events:	
	 *
	 * MPII_EVENT_SAS_DISCOVERY
	 * XXX remove MPII_EVENT_SAS_BROADCAST_PRIMITIVE
	 * MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST
	 * MPII_EVENT_SAS_DEVICE_STATUS_CHANGE
	 * MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE
	 * MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST
	 * MPII_EVENT_IR_VOLUME
	 * MPII_EVENT_IR_PHYSICAL_DISK
	 * MPII_EVENT_IR_OPERATION_STATUS
	 * MPII_EVENT_TASK_SET_FULL
	 * MPII_EVENT_LOG_ENTRY_ADDED
	 */

	/* XXX dynamically build event mask */
	enq->event_masks[0] = htole32(0x0f2f3fff);
	enq->event_masks[1] = htole32(0xfffffffc);
	enq->event_masks[2] = htole32(0xffffffff);
	enq->event_masks[3] = htole32(0xffffffff);
	
	mpii_start(sc, ccb);

	return (0);
}

void
mpii_event_process_ir_cfg_change_list(struct mpii_softc *sc, 
    struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_ir_cfg_change_list	*ccl;
	struct mpii_evt_ir_cfg_element	*ce;
	struct mpii_device	*device; //, n;
	u_int16_t		type;
	u_int32_t		flags;
	int			i, volid;

	ccl = (struct mpii_evt_ir_cfg_change_list *)(enp + 1);

	flags = letoh32(ccl->flags);

	DNPRINTF(MPII_D_MAP, "%s: mpii_event_process_ir_cfg_change\n", 
	    DEVNAME(sc));
	DNPRINTF(MPII_D_MAP," %s:  flags: 0x%08x num_elements: %d\n",
	    DEVNAME(sc), flags, ccl->num_elements);

	if ((flags & MPII_EVT_IR_CFG_CHANGE_LIST_FLAGS_CFG_MASK) == 
	    MPII_EVT_IR_CFG_CHANGE_LIST_FLAGS_CFG_FOREIGN)
		/* bail on foreign configurations */
		return;

	if (ccl->num_elements == 0)
		return;

	ce = (struct mpii_evt_ir_cfg_element *)(ccl + 1);

	for (i = 0; i < ccl->num_elements; i++, ce++) {

		for (volid = 0; volid < sc->sc_max_devices; volid++)

		type = (letoh16(ce->element_flags) & 
		    MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_MASK);
		
		switch (type) {
		case MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_VOLUME:
			switch (ce->reason_code) {
			case MPII_EVT_IR_CFG_ELEMENT_RC_ADDED:
			case MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_CREATED:
				/* XXX check for exceeding boundaries */
				volid = sc->sc_vd_id_low + sc->sc_vd_count;

				/* do magic to map vol dev_handle to TID */
				DNPRINTF(MPII_D_MAP, "%s: mapping: attaching "
				    "vol_dev_handle: 0x%04x to volid %d\n", 
				    DEVNAME(sc), letoh16(ce->vol_dev_handle),
				    volid);
			
				if (sc->sc_mpii_dev[volid] == NULL) {
					device = malloc(sizeof(struct mpii_device), 
				    	    M_DEVBUF, M_ZERO);
					if (device == NULL) {
						printf("%s: mpii_event_ir_cfg_change_list "
					    	    "unable to allocate mpii_device\n",
					    	DEVNAME(sc));
						continue;
					}
				
					sc->sc_vd_count++;
					device->type = MPII_DEV_TYPE_VD;
					device->dev_handle = letoh16(ce->vol_dev_handle);
					/* XXX bunch more fields */

					sc->sc_mpii_dev[volid] = device;
					RB_INSERT(mpii_dev_tree, &mpii_dev_head, sc->sc_mpii_dev[volid]);
				} else {
					if (!(sc->sc_mpii_dev[volid]->flags & MPII_DEV_UNUSED)) {
						printf("%s: mpii_evt_ir_cfg_change_list "
					    	    "volume id collision - volid %d\n", 
					    	    DEVNAME(sc), volid);
						continue;
					} else {
						/* sc->sc_mpii_dev[volid] already
						 * exists and is not flagged as
						 * UNUSED so we gracefully (sic)
						 * do nothing at this time.
						 */
					}
				}
				break;

			case MPII_EVT_IR_CFG_ELEMENT_RC_REMOVED:
			case MPII_EVT_IR_CFG_ELEMENT_RC_VOLUME_DELETED:
				/* do magic to remove volume */
				break;
	
			case MPII_EVT_IR_CFG_ELEMENT_RC_NO_CHANGE:
			default:
				break;
			}
			break;

		case MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_VOL_PD:
			switch (ce->reason_code) {		
			case MPII_EVT_IR_CFG_ELEMENT_RC_HIDE:
			case MPII_EVT_IR_CFG_ELEMENT_RC_PD_CREATED:
				/* do magic to hide/tag from OS view */
				//DNPRINTF(MPII_D_MAP, "%s: mapping: hiding "
				 //   "pd_dev_handle: 0x%04x\n", DEVNAME(sc),
				//	letoh16(ce->phys_disk_dev_handle));
				// n.dev_handle = letoh16(ce->phys_disk_dev_handle);
				// device = RB_FIND(mpii_dev_tree, &mpii_dev_head, &n);
				//device = NULL;
				//if (device)
				//	device->flags |= MPII_DEV_HIDDEN;
				break;

			case MPII_EVT_IR_CFG_ELEMENT_RC_UNHIDE:
			case MPII_EVT_IR_CFG_ELEMENT_RC_PD_DELETED:
				/* 
				 * XXX remove evidence of this disk (and no longer
				 * bother to hide it
				 */
				 break;

			case MPII_EVT_IR_CFG_ELEMENT_RC_NO_CHANGE:
			default:
				break;

			}
			break;

		case MPII_EVT_IR_CFG_ELEMENT_EF_ELEMENT_TYPE_HSP_PD:
			break;

		default:
			/* XXX oops */
			break;
		}
	}
}

void
mpii_event_process_sas_topo_change(struct mpii_softc *sc, 
    struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_sas_topo_change_list	*tcl;
	struct mpii_evt_phy_entry	*pe;
	u_int8_t		reason_code;
	struct mpii_device	*device;
	int			i, slotid;

	tcl = (struct mpii_evt_sas_topo_change_list *)(enp + 1);

	DNPRINTF(MPII_D_MAP, "%s: sas_topo_change: start_phy_num: %d "
	    "num_entries: %d physical_port %d\n", DEVNAME(sc), 
	    tcl->start_phy_num, tcl->num_entries, tcl->physical_port);

	if (tcl->num_entries <= 0)
		return;

	pe = (struct mpii_evt_phy_entry *)(tcl + 1);

	for (i = 0; i < tcl->num_entries; i++, pe++) {
		reason_code = (pe->phy_status & MPII_EVENT_SAS_TOPO_PS_RC_MASK);

		switch (reason_code) {
		case MPII_EVENT_SAS_TOPO_PS_RC_TARG_ADDED:
			/* XXX begging to move this into a f() */
			DNPRINTF(MPII_D_MAP, "%s: mapping: attached_dev_handle: "
			    "0x%04x  link_rate: 0x%02x phy_status: 0x%02x\n", 
			    DEVNAME(sc), letoh16(pe->attached_dev_handle),
		    	    pe->link_rate, pe->phy_status);
			
			/* XXX check for exceeding boundaries */
			slotid = sc->sc_pd_id_start + tcl->start_phy_num + i;
			
			if (sc->sc_mpii_dev[slotid])
				if (!(sc->sc_mpii_dev[slotid]->flags & MPII_DEV_UNUSED)) {
					printf("%s: mpii_evt_sas_topo_change "
					    "slot id collision - slot %d\n", 
					    DEVNAME(sc), slotid);
					continue;
				}	

			device = malloc(sizeof(struct mpii_device), 
			    M_DEVBUF, M_ZERO);
			if (device == NULL) {
				printf("%s: mpii_event_ir_cfg_change_list "
				    "unable to allocate mpii_device\n",
				    DEVNAME(sc));
				break;
			}
			
			device->dev_handle = letoh16(pe->attached_dev_handle);
			device->phy_num = tcl->start_phy_num + i;
			device->type = MPII_DEV_TYPE_PD;
			if (tcl->enclosure_handle)
				device->physical_port = tcl->physical_port;
			device->enclosure_handle = letoh16(tcl->enclosure_handle);
			device->expander_dev_handle = letoh16(tcl->expander_dev_handle);
			/* XXX
			device->wwid = 
			device->flags =
			*/
			sc->sc_mpii_dev[slotid] = device;
			RB_INSERT(mpii_dev_tree, &mpii_dev_head, device);
			break;

		case MPII_EVENT_SAS_TOPO_PS_RC_TARG_NO_RESPOND_MISS:
			/* according to MPI2 we need to:
			 * a. abort all outstanding I/O operations
			 * b. send a SAS IO unit control request w/ operation
			 *    set to MPII_SAS_OP_REMOVE_DEVICE to remove all IOC
			 *    resources associated with the device.
			 */
		case MPII_EVENT_SAS_TOPO_PS_RC_PHY_LINK_STATE_CHANGE:
		case MPII_EVENT_SAS_TOPO_PS_RC_PHY_LINK_STATE_NOCHNG:
		case MPII_EVENT_SAS_TOPO_PS_RC_WITHIN_DELAY_TIMER:
		default:
			DNPRINTF(MPII_D_EVT, "%s: unhandled "
			    "mpii_event_sas_topo_change reason code\n", 
			    DEVNAME(sc));
			break;
		}
	}
}

void
mpii_event_process_sas_discovery(struct mpii_softc *sc, struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_sas_discovery	*ds;

	ds = (struct mpii_evt_sas_discovery *)(enp + 1);

	if ((ds->flags & MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROG_MASK) == 
	    MPII_EVENT_SAS_DISC_FLAGS_DISC_IN_PROGRESS)
		sc->sc_discovery_in_progress = 1;
	else
		sc->sc_discovery_in_progress = 0;

	switch (ds->reason_code) {
	case MPII_EVENT_SAS_DISC_REASON_CODE_STARTED:
		break;
	case MPII_EVENT_SAS_DISC_REASON_CODE_COMPLETED:
		/* XXX look at ds->discovery_status */
		break;
	default:
		DNPRINTF(MPII_D_EVT, "%s: unknown reason for SAS device status "
		    "change: 0x%02x\n", DEVNAME(sc), ds->reason_code);
		break;
	}
}

void
mpii_event_process_ir_phys_disk(struct mpii_softc *sc, struct mpii_msg_event_reply *enp)
{
	struct mpii_evt_ir_physical_disk	*ipd;

	ipd = (struct mpii_evt_ir_physical_disk *)(enp + 1);

	/* XXX TODO:
	 *     process the event fully
	 *     understand that this is necesarily a IR_PD
	 */
}

void
mpii_event_process(struct mpii_softc *sc, struct mpii_msg_reply *prm)
{
	struct mpii_msg_event_reply		*enp = (struct mpii_msg_event_reply *)prm;

	DNPRINTF(MPII_D_EVT, "%s: mpii_event_process\n", DEVNAME(sc));

	DNPRINTF(MPII_D_EVT, "%s:  function: 0x%02x msg_length: %d "
	    "event_data_length: %d\n", DEVNAME(sc), enp->function, 
	    enp->msg_length, letoh16(enp->event_data_length));
	DNPRINTF(MPII_D_EVT, "%s:  ack_required: %d msg_flags 0x%02x\n",
	    DEVNAME(sc), enp->ack_required, enp->msg_flags);
	DNPRINTF(MPII_D_EVT, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(enp->ioc_status));
	DNPRINTF(MPII_D_EVT, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(enp->ioc_loginfo));
	DNPRINTF(MPII_D_EVT, "%s:  event: 0x%08x\n", DEVNAME(sc),
	    letoh32(enp->event));
	DNPRINTF(MPII_D_EVT, "%s:  event_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(enp->event_context));
	
	switch (letoh32(enp->event)) {
	case MPII_EVENT_SAS_DISCOVERY:
		mpii_event_process_sas_discovery(sc, enp);
		break;

	case MPII_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
		mpii_event_process_sas_topo_change(sc, enp);
		break;

	case MPII_EVENT_SAS_DEVICE_STATUS_CHANGE:
		/* XXX */
		break;

	case MPII_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
		/* XXX track enclosures coming/going */
		break;

	case MPII_EVENT_IR_VOLUME:
		/* XXX */
		break;

	case MPII_EVENT_IR_PHYSICAL_DISK:
		mpii_event_process_ir_phys_disk(sc, enp);
		break;

	case MPII_EVENT_IR_CONFIGURATION_CHANGE_LIST:
		mpii_event_process_ir_cfg_change_list(sc, enp);
		break;

	case MPII_EVENT_IR_OPERATION_STATUS:
	case MPII_EVENT_TASK_SET_FULL:
	case MPII_EVENT_LOG_ENTRY_ADDED:

	default:
		DNPRINTF(MPII_D_EVT, "%s:  unhandled event 0x%02x\n",
		    DEVNAME(sc), letoh32(enp->event));
		break;
	}

#ifdef MPII_DEBUG
	int i;
	for (i = 0; i < enp->event_data_length; i++) {
		DNPRINTF(MPII_D_EVT, "%s:  event_data: 0x%08x\n", DEVNAME(sc),
		    dwordn((enp+1), i));
	}
#endif /* MPII_DEBUG */

	if (enp->ack_required)
		mpii_eventack(sc, enp);
}	

void
mpii_eventnotify_done(struct mpii_ccb *ccb)
{
	struct mpii_softc			*sc = ccb->ccb_sc;

	DNPRINTF(MPII_D_EVT, "%s: mpii_eventnotify_done\n", DEVNAME(sc));

	mpii_event_process(sc, ccb->ccb_rcb->rcb_reply);

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);
}

int
mpii_get_bios_pg2(struct mpii_softc *sc)
{
	struct mpii_cfg_hdr	hdr;
	size_t			pagelen;
	int			rv = 0;

	DNPRINTF(MPII_D_RAID, "%s: mpii_get_bios_pg2\n", DEVNAME(sc));

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_BIOS, 2, 0, &hdr) != 0) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_bios_pg2 unable to fetch "
		    "header for BIOS page 2\n", DEVNAME(sc));
		return (1);
	}

	pagelen = hdr.page_length * 4;
	sc->sc_bios_pg2 = malloc(pagelen, M_TEMP, M_WAITOK|M_CANFAIL);
	if (sc->sc_bios_pg2 == NULL) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_bios_pg2 unable to "
		    "allocate space for BIOS page 2\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_cfg_page(sc, 0, &hdr, 1, sc->sc_bios_pg2, pagelen) != 0) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_bios_pg2 unable to "
		    "fetch BIOS config page 2\n", DEVNAME(sc));
		rv = 1;
		goto out;
	}

	DNPRINTF(MPII_D_RAID, "%s: req_boot_device_form: 0x%02x "
	    "req_alt_boot_device_form: 0x%02x current_boot_device_form: "
	    "0x%02x\n", DEVNAME(sc), sc->sc_bios_pg2->req_boot_device_form, 
	    sc->sc_bios_pg2->req_alt_boot_device_form, 
	    sc->sc_bios_pg2->current_boot_device_form);

out:
	free(sc->sc_bios_pg2, M_TEMP);

	return (rv);
}

void
mpii_get_raid_config_pg0(struct mpii_softc *sc)
{
	struct mpii_cfg_raid_config_pg0	*config_page;
	struct mpii_raid_config_element	*element_list, *element;
	struct mpii_ecfg_hdr	hdr;
	size_t			pagelen;
	struct scsi_link	*link;
	int			i;

	DNPRINTF(MPII_D_RAID, "%s: mpii_get_raid_config_pg0\n", DEVNAME(sc));

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_RAID_VOL, 0, 0, &hdr) != 0) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_raid_config_pg0 unable to "
		    "fetch header for IOC page 2\n", DEVNAME(sc));
		return;
	}

	pagelen = hdr.ext_page_length * 4; /* dwords to bytes */
	config_page = malloc(pagelen, M_TEMP, M_WAITOK|M_CANFAIL);
	if (config_page == NULL) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_raid_config_pg0 unable to "
		    "allocate space for raid config page 0\n", DEVNAME(sc));
		return;
	}
	element_list = (struct mpii_raid_config_element *)(config_page + 1);

	if (mpii_cfg_page(sc, 0, &hdr, 1, config_page, pagelen) != 0) {
		DNPRINTF(MPII_D_RAID, "%s: mpii_get_raid_config_pg0 unable to "
		    "fetch raid config page 0\n", DEVNAME(sc));
		goto out;
	}

	DNPRINTF(MPII_D_RAID, "%s:  numhotspares: 0x%2x numphysdisks: 0x%02x "
	    "numvolumes: 0x%02x confignum: 0x%02x\n", DEVNAME(sc), 
	    config_page->num_hot_spares, config_page->num_phys_disks, 
	    config_page->num_volumes, config_page->config_num);
	DNPRINTF(MPII_D_RAID, "%s:  flags: 0x%08x\n", DEVNAME(sc),
	    config_page->flags);
	DNPRINTF(MPII_D_RAID, "%s:  configguid: 0x%08x\n", DEVNAME(sc),
	    config_page->config_guid[0]);
	DNPRINTF(MPII_D_RAID, "%s:              0x%08x\n", DEVNAME(sc),
	    config_page->config_guid[1]);
	DNPRINTF(MPII_D_RAID, "%s:              0x%08x\n", DEVNAME(sc),
	    config_page->config_guid[2]);
	DNPRINTF(MPII_D_RAID, "%s:              0x%08x\n", DEVNAME(sc),
	    config_page->config_guid[3]);
	DNPRINTF(MPII_D_RAID, "%s  numelements: 0x%02x\n", DEVNAME(sc),
	    config_page->num_elements);

	/* don't walk list if there are no RAID capability */
	/* XXX anything like this for MPI2?
	if (capabilities == 0xdeadbeef) {
		printf("%s: deadbeef in raid configuration\n", DEVNAME(sc));
		goto out;
	}
	*/

	if (config_page->num_volumes == 0)
		goto out;

	sc->sc_flags |= MPII_F_RAID;

	for (i = 0; i < config_page->num_volumes; i++) {
		element = &element_list[i];

		DNPRINTF(MPII_D_RAID, "%s:   elementflags: 0x%04x voldevhandle: "
		    "0x%04x\n", DEVNAME(sc), letoh16(element->element_flags),
		    letoh16(element->vol_dev_handle));
		DNPRINTF(MPII_D_RAID, "%s:   hotsparepool: 0x%02x physdisknum: "
		    "0x%02x physdiskdevhandle: 0x%04x\n", DEVNAME(sc),
		    element->hot_spare_pool, element->phys_disk_num,
		    letoh16(element->phys_disk_dev_handle));

		link = sc->sc_scsibus->sc_link[element->vol_dev_handle][0];
		if (link == NULL)
			continue;

		link->flags |= SDEV_VIRTUAL;
	}

out:
	free(config_page, M_TEMP);
}

int
mpii_get_dpm(struct mpii_softc *sc)
{
	/* XXX consider expanding (or is that reducing) functionality to support
	 * non-dpm mode of operation when the following fails, but iocfacts
	 * indicated that the ioc supported dmp
	 */
	if (!sc->sc_dpm_enabled) 
		return (1);

	if (mpii_get_dpm_pg0(sc, sc->sc_dpm_pg0) != 0) 
		return (1);
	
	return (0);
}

int
mpii_get_dpm_pg0(struct mpii_softc *sc, struct mpii_cfg_dpm_pg0 *dpm_page)
{
	struct mpii_msg_config_request		*cq;
	struct mpii_msg_config_reply		*cp;
	struct mpii_ccb				*ccb;
	struct mpii_dpm_entry	*dpm_entry;
	struct mpii_ecfg_hdr	ehdr;
	struct mpii_dmamem	*dmapage;
	u_int64_t		dva;
	char			*kva;
	size_t			pagelen;
	u_int32_t		address;
	int			rv = 0, s;

	DNPRINTF(MPII_D_RAID, "%s: mpii_get_dpm_pg0\n", DEVNAME(sc));

	if (mpii_ecfg_header(sc, 
	    MPII_CONFIG_REQ_PAGE_TYPE_DRIVER_MAPPING, 0, 0, &ehdr)) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_get_dpm_pg0 unable to fetch"
		    " header for driver mapping page 0\n", DEVNAME(sc));
		return (1);
	}

	pagelen = sizeof(struct mpii_ecfg_hdr) + sc->sc_max_dpm_entries * 
	    sizeof(struct mpii_dpm_entry);
	
	dpm_page = malloc(pagelen, M_TEMP, M_WAITOK|M_CANFAIL);
	if (dpm_page == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_get_dpm_pg0 unable to allocate "
		    "space for device persistence mapping page 0\n", DEVNAME(sc));
		return (1);
	}
	
	dmapage = mpii_dmamem_alloc(sc, pagelen);
	if (dmapage == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_get_dpm_pg0 unable to allocate "
		    "space for dpm dma buffer\n", DEVNAME(sc));
		return (1);
	}

	/* the current mpii_req_cfg_page() (and associated macro wrappers) do
	 * not allow large page length nor is is flexible in allowing actions
	 * other that READ_CURRENT or WRITE_CURRENT. since we need to READ_NVRAM
	 * and we need a pagelength around 2580 bytes this function currently
	 * manually creates and drives the config request.
	 *  
	 * XXX modify the mpii_req_cfg_page() to allow the caller to specify:
	 *     - alternate actions
	 *     - use passed-in dma-able buffer OR allocate within function if
	 *     the page length exceeds the left over space in the request frame
	 */

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_get_dpm_pg0 ccb_get\n", DEVNAME(sc));
		return (1);
	}

	address = sc->sc_max_dpm_entries << MPII_DPM_ADDRESS_ENTRY_COUNT_SHIFT;

	cq = ccb->ccb_cmd;

	cq->function = MPII_FUNCTION_CONFIG;
	cq->action = MPII_CONFIG_REQ_ACTION_PAGE_READ_NVRAM;
	
	cq->config_header.page_version = ehdr.page_version;
	cq->config_header.page_number = ehdr.page_number;
	cq->config_header.page_type = ehdr.page_type;
	cq->ext_page_len = htole16(pagelen / 4);
	cq->ext_page_type = ehdr.ext_page_type;
	cq->config_header.page_type &= MPII_CONFIG_REQ_PAGE_TYPE_MASK;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
	    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL |
	    (pagelen * 4) | MPII_SGE_FL_DIR_IN);
	 
	dva = MPII_DMA_DVA(dmapage);
	cq->page_buffer.sg_hi_addr = htole32((u_int32_t)(dva >> 32));
	cq->page_buffer.sg_lo_addr = htole32((u_int32_t)dva);

/*	kva = ccb->ccb_cmd;
	kva += sizeof(struct mpii_msg_config_request);
*/
	//kva = MPII_DMA_KVA(page);

  	ccb->ccb_done = mpii_empty_done; 
	if (mpii_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header poll\n",
		    DEVNAME(sc));
		return (1);
	}

	if (ccb->ccb_rcb == NULL) {
		mpii_put_ccb(sc, ccb);
		return (1);
	}
	cp = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_MISC, "%s:  action: 0x%02x sglflags: 0x%02x "
	    "msg_length: %d function: 0x%02x\n", DEVNAME(sc), cp->action, 
	    cp->msg_length, cp->function);
	DNPRINTF(MPII_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    letoh16(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    cp->vp_id, cp->vf_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(cp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version,
	    cp->config_header.page_length,
	    cp->config_header.page_number,
	    cp->config_header.page_type);

	kva = MPII_DMA_KVA(dmapage);

#ifdef MPII_DEBUG
	int i, j;
	for (i = 0; i < sc->sc_max_dpm_entries; i++) {
		printf(" (%d) ", i);
		for (j = 0; j < sizeof(struct mpii_dpm_entry); j++)
			printf("%x", (char *)(kva + j + (i * sizeof(struct mpii_dpm_entry))));
		printf("\n");
	}
#endif

	if (letoh16(cp->ioc_status) != MPII_IOCSTATUS_SUCCESS)
		rv = 1;
	/*else
		bcopy(kva, page, pagelen);
*/
	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);

	dpm_entry = (struct mpii_dpm_entry *)(kva + sizeof(struct mpii_ecfg_hdr));
	
//	bcopy(dpm_page, cp, pagelen * 4);

#ifdef MPII_DEBUG 
	for (i = 0; i < sc->sc_max_dpm_entries; i++, dpm_entry++)
		printf("%s:  [%d] physid: 0x%lx mappinginfo: 0x%04x devindex: 0x%04x "
		    "physicabitsmapping: 0x%08x\n", DEVNAME(sc), i,
		    letoh64(dpm_entry->physical_identifier), 
		    letoh16(dpm_entry->mapping_information),
		    letoh16(dpm_entry->device_index), 
		    letoh32(dpm_entry->physical_bits_mapping));
#endif

	free(dpm_page, M_TEMP);
	mpii_dmamem_free(sc, dmapage);
	return (rv);
}

int
mpii_get_ioc_pg8(struct mpii_softc *sc)
{
	struct mpii_cfg_hdr	hdr;
	struct mpii_cfg_ioc_pg8	*page;
	size_t			pagelen;
	u_int16_t		flags;
	int			rv = 0;

	DNPRINTF(MPII_D_RAID, "%s: mpii_get_ioc_pg8\n", DEVNAME(sc));

	if (mpii_cfg_header(sc, MPII_CONFIG_REQ_PAGE_TYPE_IOC, 8, 0, &hdr) != 0) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_ioc_pg8 unable to fetch header"
		    "for IOC page 8\n", DEVNAME(sc));
		return (1);
	}

	pagelen = hdr.page_length * 4; /* dwords to bytes */
	
	page = malloc(pagelen, M_TEMP, M_WAITOK|M_CANFAIL);
	if (page == NULL) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_ioc_pg8 unable to allocate "
		    "space for ioc config page 8\n", DEVNAME(sc));
		return (1);
	}

	if (mpii_cfg_page(sc, 0, &hdr, 1, page, pagelen) != 0) {
		DNPRINTF(MPII_D_CFG, "%s: mpii_get_raid unable to fetch IOC "
		    "page 8\n", DEVNAME(sc));
		rv = 1;
		goto out;
	}

	DNPRINTF(MPII_D_CFG, "%s:  numdevsperenclosure: 0x%02x\n", DEVNAME(sc),
	    page->num_devs_per_enclosure);
	DNPRINTF(MPII_D_CFG, "%s:  maxpersistententries: 0x%04x "
	    "maxnumphysicalmappedids: 0x%04x\n", DEVNAME(sc),
	    letoh16(page->max_persistent_entries), 
	    letoh16(page->max_num_physical_mapped_ids));
	DNPRINTF(MPII_D_CFG, "%s:  flags: 0x%04x\n", DEVNAME(sc),
	    letoh16(page->flags));
	DNPRINTF(MPII_D_CFG, "%s:  irvolumemappingflags: 0x%04x\n", 
	    DEVNAME(sc), letoh16(page->ir_volume_mapping_flags));

	if (!(page->flags & MPII_IOC_PG8_FLAGS_ENCLOSURE_SLOT_MAPPING))
		/* XXX we don't currently handle persistent mapping mode */
		printf("%s: warning: controller requested device persistence "
		    "mapping mode is not supported.\n");

	sc->sc_max_dpm_entries = page->max_persistent_entries;
	sc->sc_dpm_enabled = (sc->sc_max_dpm_entries) ? 1 : 0;

	if (page->flags & MPII_IOC_PG8_FLAGS_DISABLE_PERSISTENT_MAPPING)
		sc->sc_dpm_enabled = 0;

	sc->sc_pd_id_start = 0;

	if (page->flags & MPII_IOC_PG8_FLAGS_RESERVED_TARGETID_0) {
		sc->sc_reserve_tid0 = 1;
		sc->sc_num_reserved_entries = 1;
		sc->sc_pd_id_start = 1;
	} else
		sc->sc_num_reserved_entries = 0;

	flags = page->ir_volume_mapping_flags && 
	    MPII_IOC_PG8_IRFLAGS_VOLUME_MAPPING_MODE_MASK;
	if (sc->sc_ir_firmware) {
		if (flags == MPII_IOC_PG8_IRFLAGS_LOW_VOLUME_MAPPING) {
			sc->sc_num_reserved_entries += sc->sc_max_volumes;
			sc->sc_vd_id_low = 0;
			if (page->flags & MPII_IOC_PG8_FLAGS_RESERVED_TARGETID_0)
				sc->sc_vd_id_low = 1;
		} else
			sc->sc_vd_id_low = sc->sc_max_devices - sc->sc_max_volumes;
		sc->sc_vd_id_hi = sc->sc_vd_id_low + sc->sc_max_volumes - 1;
		sc->sc_pd_id_start = sc->sc_vd_id_hi + 1;
	}

	DNPRINTF(MPII_D_MAP, "%s: mpii_get_ioc_pg8 mapping: sc_pd_id_start: %d "
	    "sc_vd_id_low: %d sc_vd_id_hi: %d\n", DEVNAME(sc), 
	    sc->sc_pd_id_start, sc->sc_vd_id_low, sc->sc_vd_id_hi);

out:
	free(page, M_TEMP);

	return(rv);
}

int
mpii_req_cfg_header(struct mpii_softc *sc, u_int8_t type, u_int8_t number,
    u_int32_t address, int flags, void *p)
{
	struct mpii_msg_config_request		*cq;
	struct mpii_msg_config_reply		*cp;
	struct mpii_cfg_hdr	*hdr = p;
	struct mpii_ccb		*ccb;
	struct mpii_ecfg_hdr	*ehdr = p;
	int			etype = 0;
	int			rv = 0;
	int			s;

	DNPRINTF(MPII_D_MISC, "%s: mpii_req_cfg_header type: %#x number: %x "
	    "address: 0x%08x flags: 0x%b\n", DEVNAME(sc), type, number,
	    address, flags, MPII_PG_FMT);

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	if (ISSET(flags, MPII_PG_EXTENDED)) {
		etype = type;
		type = MPII_CONFIG_REQ_PAGE_TYPE_EXTENDED;
	}

	cq = ccb->ccb_cmd;

	cq->function = MPII_FUNCTION_CONFIG;

	cq->action = MPII_CONFIG_REQ_ACTION_PAGE_HEADER;

	cq->config_header.page_number = number;
	cq->config_header.page_type = type;
	cq->ext_page_type = etype;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
	    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL);

	if (ISSET(flags, MPII_PG_POLL)) {
		ccb->ccb_done = mpii_empty_done;
		if (mpii_poll(sc, ccb, 50000) != 0) {
			DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header poll\n",
			    DEVNAME(sc));
			return (1);
		}
	} else {
		ccb->ccb_done = (void (*)(struct mpii_ccb *))wakeup;
		s = splbio();
		mpii_start(sc, ccb);
		while (ccb->ccb_state != MPII_CCB_READY)
			tsleep(ccb, PRIBIO, "mpiipghdr", 0);
		splx(s);
	}

	if (ccb->ccb_rcb == NULL)
		panic("%s: unable to fetch config header\n", DEVNAME(sc));
	cp = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_MISC, "%s:  action: 0x%02x sgl_flags: 0x%02x "
	    "msg_length: %d function: 0x%02x\n", DEVNAME(sc), cp->action, 
	    cp->sgl_flags, cp->msg_length, cp->function);
	DNPRINTF(MPII_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    letoh16(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    cp->vp_id, cp->vf_id);	
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(cp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version,
	    cp->config_header.page_length,
	    cp->config_header.page_number,
	    cp->config_header.page_type);

	if (letoh16(cp->ioc_status) != MPII_IOCSTATUS_SUCCESS)
		rv = 1;
	else if (ISSET(flags, MPII_PG_EXTENDED)) {
		bzero(ehdr, sizeof(*ehdr));
		ehdr->page_version = cp->config_header.page_version;
		ehdr->page_number = cp->config_header.page_number;
		ehdr->page_type = cp->config_header.page_type;
		ehdr->ext_page_length = cp->ext_page_length;
		ehdr->ext_page_type = cp->ext_page_type;
	} else
		*hdr = cp->config_header;

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);

	return (rv);
}

int
mpii_req_cfg_page(struct mpii_softc *sc, u_int32_t address, int flags,
    void *p, int read, void *page, size_t len)
{
	struct mpii_msg_config_request		*cq;
	struct mpii_msg_config_reply		*cp;
	struct mpii_cfg_hdr	*hdr = p;
	struct mpii_ccb		*ccb;
	struct mpii_ecfg_hdr	*ehdr = p;
	u_int64_t		dva;
	char			*kva;
	int			page_length;
	int			rv = 0;
	int			s;

	DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_page address: %d read: %d type: %x\n",
	    DEVNAME(sc), address, read, hdr->page_type);

	page_length = ISSET(flags, MPII_PG_EXTENDED) ?
	    letoh16(ehdr->ext_page_length) : hdr->page_length;

	if (len > MPII_REQUEST_SIZE - sizeof(struct mpii_msg_config_request) ||
    	    len < page_length * 4)
		return (1);

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_page ccb_get\n", DEVNAME(sc));
		return (1);
	}

	cq = ccb->ccb_cmd;

	cq->function = MPII_FUNCTION_CONFIG;

	cq->action = (read ? MPII_CONFIG_REQ_ACTION_PAGE_READ_CURRENT :
	    MPII_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT);

	if (ISSET(flags, MPII_PG_EXTENDED)) {
		cq->config_header.page_version = ehdr->page_version;
		cq->config_header.page_number = ehdr->page_number;
		cq->config_header.page_type = ehdr->page_type;
		cq->ext_page_len = ehdr->ext_page_length;
		cq->ext_page_type = ehdr->ext_page_type;
	} else
		cq->config_header = *hdr;
	cq->config_header.page_type &= MPII_CONFIG_REQ_PAGE_TYPE_MASK;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPII_SGE_FL_TYPE_SIMPLE |
	    MPII_SGE_FL_LAST | MPII_SGE_FL_EOB | MPII_SGE_FL_EOL |
	    (page_length * 4) |
	    (read ? MPII_SGE_FL_DIR_IN : MPII_SGE_FL_DIR_OUT));

	/* bounce the page via the request space to avoid more bus_dma games */
	dva = ccb->ccb_cmd_dva + sizeof(struct mpii_msg_config_request);

	cq->page_buffer.sg_hi_addr = htole32((u_int32_t)(dva >> 32));
	cq->page_buffer.sg_lo_addr = htole32((u_int32_t)dva);

	kva = ccb->ccb_cmd;
	kva += sizeof(struct mpii_msg_config_request);

	if (!read)
		bcopy(page, kva, len);

	if (ISSET(flags, MPII_PG_POLL)) {
		ccb->ccb_done = mpii_empty_done;
		if (mpii_poll(sc, ccb, 50000) != 0) {
			DNPRINTF(MPII_D_MISC, "%s: mpii_cfg_header poll\n",
			    DEVNAME(sc));
			return (1);
		}
	} else {
		ccb->ccb_done = (void (*)(struct mpii_ccb *))wakeup;
		s = splbio();
		mpii_start(sc, ccb);
		while (ccb->ccb_state != MPII_CCB_READY)
			tsleep(ccb, PRIBIO, "mpiipghdr", 0);
		splx(s);
	}

	if (ccb->ccb_rcb == NULL) {
		mpii_put_ccb(sc, ccb);
		return (1);
	}
	cp = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_MISC, "%s:  action: 0x%02x sglflags: 0x%02x "
	    "msg_length: %d function: 0x%02x\n", DEVNAME(sc), cp->action, 
	    cp->msg_length, cp->function);
	DNPRINTF(MPII_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    letoh16(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);
	DNPRINTF(MPII_D_MISC, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    cp->vp_id, cp->vf_id);
	DNPRINTF(MPII_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(cp->ioc_status));
	DNPRINTF(MPII_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->ioc_loginfo));
	DNPRINTF(MPII_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version,
	    cp->config_header.page_length,
	    cp->config_header.page_number,
	    cp->config_header.page_type);
	
	if (letoh16(cp->ioc_status) != MPII_IOCSTATUS_SUCCESS)
		rv = 1;
	else if (read)
		bcopy(kva, page, len);

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);

	return (rv);
}

int
mpii_reply(struct mpii_softc *sc)
{
	struct mpii_reply_descriptor	*rdp;
	struct mpii_ccb		*ccb = NULL;
	struct mpii_rcb		*rcb = NULL;
	struct mpii_msg_reply	*reply = NULL;
	u_int8_t		reply_flags;
	u_int32_t		reply_dva, i;
	int			smid;


	DNPRINTF(MPII_D_INTR, "%s: mpii_reply\n", DEVNAME(sc));

	/* XXX need to change to to be just the reply we expect to read */
	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq), 0, 
	    8 * sc->sc_reply_post_qdepth, BUS_DMASYNC_POSTWRITE);
		 
	rdp = &sc->sc_reply_postq_kva[sc->sc_reply_post_host_index];

	reply_flags = (u_int8_t)(rdp->reply_flags) & 
	    MPII_REPLY_DESCR_FLAGS_TYPE_MASK;
	
	if ((reply_flags == MPII_REPLY_DESCR_FLAGS_UNUSED))
		return (-1);

	if (dwordn(rdp, 1) == 0xffffffff)
		/*
		 * ioc is still writing to the reply post queue
		 * race condition - bail!
		 */
		 return (-1);
		 
	DNPRINTF(MPII_D_INTR, "%s:  dword[0]: 0x%08x\n", DEVNAME(sc), 
	    letoh32(dwordn(rdp, 0)));
	DNPRINTF(MPII_D_INTR, "%s:  dword[1]: 0x%08x\n", DEVNAME(sc), 
	    letoh32(dwordn(rdp, 1)));

	switch (reply_flags) {
	case MPII_REPLY_DESCR_FLAGS_ADDRESS_REPLY:
		/* reply frame address */
		reply_dva = letoh32(rdp->type_dependent2);
		i = (reply_dva - (u_int32_t)MPII_DMA_DVA(sc->sc_replies)) /
			MPII_REPLY_SIZE;
		
		bus_dmamap_sync(sc->sc_dmat,
		    MPII_DMA_MAP(sc->sc_replies), MPII_REPLY_SIZE * i,
		    MPII_REPLY_SIZE, BUS_DMASYNC_POSTREAD);
		
		rcb = &sc->sc_rcbs[i];
		reply = rcb->rcb_reply;
		/* fall through */
	default:
		/* smid */
		 smid = letoh16(rdp->type_dependent1);
	}

	DNPRINTF(MPII_D_INTR, "%s: mpii_reply reply_flags: %d smid: %d reply: %p\n",
	    DEVNAME(sc), reply_flags, smid, reply);

	if (smid)  {
		ccb = &sc->sc_ccbs[smid - 1];

		/* XXX why is this necessary ? */
		bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, MPII_REQUEST_SIZE,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		ccb->ccb_state = MPII_CCB_READY;
		ccb->ccb_rcb = rcb;
	}

	DNPRINTF(MPII_D_INTR, "    rcb: 0x%04x\n", rcb);

	dwordn(rdp, 0) = 0xffffffff;
	dwordn(rdp, 1) = 0xffffffff;

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_reply_postq), 
	    8 * sc->sc_reply_post_host_index, 8, 
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		
	sc->sc_reply_post_host_index = (sc->sc_reply_post_host_index + 1) %
	    sc->sc_reply_post_qdepth;

	if (smid)
		ccb->ccb_done(ccb);
	else
		mpii_event_process(sc, rcb->rcb_reply);

	return (smid);
}

struct mpii_dmamem *
mpii_dmamem_alloc(struct mpii_softc *sc, size_t size)
{
	struct mpii_dmamem	*mdm;
	int			nsegs;

	mdm = malloc(sizeof(struct mpii_dmamem), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mdm == NULL)
		return (NULL);

	mdm->mdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mdm->mdm_map) != 0)
		goto mdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mdm->mdm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mdm->mdm_seg, nsegs, size,
	    &mdm->mdm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mdm->mdm_map, mdm->mdm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(mdm->mdm_kva, size);

	DNPRINTF(MPII_D_MEM, "%s: mpii_dmamem_alloc size: %d mdm: %#x "
	    "map: %#x nsegs: %d segs: %#x kva: %x\n",
	    DEVNAME(sc), size, mdm->mdm_map, nsegs, mdm->mdm_seg, mdm->mdm_kva);

	return (mdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
mdmfree:
	free(mdm, M_DEVBUF);

	return (NULL);
}

void
mpii_dmamem_free(struct mpii_softc *sc, struct mpii_dmamem *mdm)
{
	DNPRINTF(MPII_D_MEM, "%s: mpii_dmamem_free %#x\n", DEVNAME(sc), mdm);

	bus_dmamap_unload(sc->sc_dmat, mdm->mdm_map);
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, mdm->mdm_size);
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
	free(mdm, M_DEVBUF);
}

int
mpii_alloc_dev(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MEM, "%s: mpii_alloc_dev: sc_max_devices: %d "
	    "sizeof(sc_mpii_dev): %d\n", DEVNAME(sc), sc->sc_max_devices,
	    sizeof(struct mpii_device *) * sc->sc_max_devices);

	sc->sc_mpii_dev = malloc(sc->sc_max_devices * 
	    sizeof(struct mpii_device *), M_DEVBUF, 
	    M_WAITOK | M_CANFAIL | M_ZERO);

	if (sc->sc_mpii_dev == NULL) 
		return (1);

	return (0);
}

int
mpii_alloc_ccbs(struct mpii_softc *sc)
{
	struct mpii_ccb		*ccb;
	u_int8_t		*cmd;
	int			i;

	TAILQ_INIT(&sc->sc_ccb_free);

	sc->sc_ccbs = malloc(sizeof(struct mpii_ccb) * (sc->sc_request_depth-1),
	    M_DEVBUF, M_WAITOK | M_CANFAIL | M_ZERO);
	if (sc->sc_ccbs == NULL) {
		printf("%s: unable to allocate ccbs\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_requests = mpii_dmamem_alloc(sc,
	    MPII_REQUEST_SIZE * sc->sc_request_depth);
	if (sc->sc_requests == NULL) {
		printf("%s: unable to allocate ccb dmamem\n", DEVNAME(sc));
		goto free_ccbs;
	}
	cmd = MPII_DMA_KVA(sc->sc_requests);
	bzero(cmd, MPII_REQUEST_SIZE * sc->sc_request_depth);

	/* 
	 * we have sc->sc_request_depth system request message 
	 * frames, but smid zero cannot be used. so we then 
	 * have (sc->sc_request_depth - 1) number of ccbs
	 */
	for (i = 1; i < sc->sc_request_depth; i++) {
		ccb = &sc->sc_ccbs[i - 1];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS,
		    sc->sc_max_sgl_len, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dma map\n", DEVNAME(sc));
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_smid = i;
		ccb->ccb_offset = MPII_REQUEST_SIZE * i;

		ccb->ccb_cmd = &cmd[ccb->ccb_offset];
		ccb->ccb_cmd_dva = (u_int32_t)MPII_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_offset;

		DNPRINTF(MPII_D_CCB, "%s: mpii_alloc_ccbs(%d) ccb: %#x map: %#x "
		    "sc: %#x smid: %#x offs: %#x cmd: %#x dva: %#x\n",
		    DEVNAME(sc), i, ccb, ccb->ccb_dmamap, ccb->ccb_sc,
		    ccb->ccb_smid, ccb->ccb_offset, ccb->ccb_cmd,
		    ccb->ccb_cmd_dva);

		mpii_put_ccb(sc, ccb);
	}

	return (0);

free_maps:
	while ((ccb = mpii_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	mpii_dmamem_free(sc, sc->sc_requests);
free_ccbs:
	free(sc->sc_ccbs, M_DEVBUF);

	return (1);
}

void
mpii_put_ccb(struct mpii_softc *sc, struct mpii_ccb *ccb)
{
	DNPRINTF(MPII_D_CCB, "%s: mpii_put_ccb %#x\n", DEVNAME(sc), ccb);

	ccb->ccb_state = MPII_CCB_FREE;
	ccb->ccb_xs = NULL;
	ccb->ccb_done = NULL;
	bzero(ccb->ccb_cmd, MPII_REQUEST_SIZE);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_free, ccb, ccb_link);
}

struct mpii_ccb *
mpii_get_ccb(struct mpii_softc *sc)
{
	struct mpii_ccb		*ccb;

	ccb = TAILQ_FIRST(&sc->sc_ccb_free);
	if (ccb == NULL) {
		DNPRINTF(MPII_D_CCB, "%s: mpii_get_ccb == NULL\n", DEVNAME(sc));
		return (NULL);
	}

	TAILQ_REMOVE(&sc->sc_ccb_free, ccb, ccb_link);

	ccb->ccb_state = MPII_CCB_READY;

	DNPRINTF(MPII_D_CCB, "%s: mpii_get_ccb %#x\n", DEVNAME(sc), ccb);

	return (ccb);
}

int
mpii_alloc_replies(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s: mpii_alloc_replies\n", DEVNAME(sc));

	sc->sc_rcbs = malloc(sc->sc_num_reply_frames * sizeof(struct mpii_rcb),
	    M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (sc->sc_rcbs == NULL)
		return (1);

	sc->sc_replies = mpii_dmamem_alloc(sc, 
	    MPII_REPLY_SIZE * sc->sc_num_reply_frames);
	if (sc->sc_replies == NULL) {
		free(sc->sc_rcbs, M_DEVBUF);
		return (1);
	}

	return (0);
}

void
mpii_push_replies(struct mpii_softc *sc)
{
	struct mpii_rcb		*rcb;
	char			*kva = MPII_DMA_KVA(sc->sc_replies);
	int			i;

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_replies),
	    0, MPII_REPLY_SIZE * sc->sc_num_reply_frames, BUS_DMASYNC_PREREAD);

	for (i = 0; i < sc->sc_num_reply_frames; i++) {
		rcb = &sc->sc_rcbs[i];

		rcb->rcb_reply = kva + MPII_REPLY_SIZE * i;
		rcb->rcb_reply_dva = (u_int32_t)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i;
		mpii_push_reply(sc, rcb->rcb_reply_dva);
	}
}
void 
mpii_start(struct mpii_softc *sc, struct mpii_ccb *ccb)
{	
	struct mpii_request_header	*rhp;
	struct mpii_request_descriptor	descriptor;

	DNPRINTF(MPII_D_RW, "%s: mpii_start %#x\n", DEVNAME(sc),
	    ccb->ccb_cmd_dva);

	rhp = ccb->ccb_cmd;

	bzero(&descriptor, sizeof(struct mpii_request_descriptor));

	switch (rhp->function) {
	case	MPII_FUNCTION_SCSI_IO_REQUEST:
			descriptor.request_flags = 
			    MPII_REQ_DESCRIPTOR_FLAGS_SCSI_IO;
			/* device handle */
			descriptor.type_dependent = 
			    htole16(ccb->ccb_dev_handle);
			break;
	case	MPII_FUNCTION_SCSI_TASK_MGMT:
			descriptor.request_flags =
			    MPII_REQ_DESCRIPTOR_FLAGS_HIGH_PRIORITY;
			break;
	default:
			descriptor.request_flags = 
			    MPII_REQ_DESCRIPTOR_FLAGS_DEFAULT;
	}

	descriptor.vf_id = sc->sc_vf_id;
	descriptor.smid = htole16(ccb->ccb_smid);

	bus_dmamap_sync(sc->sc_dmat, MPII_DMA_MAP(sc->sc_requests),
	    ccb->ccb_offset, MPII_REQUEST_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	ccb->ccb_state = MPII_CCB_QUEUED;

	DNPRINTF(MPII_D_RW, "%s:   MPII_REQ_DESC_POST_LOW (0x%08x) write "
	    "0x%08x\n", DEVNAME(sc), MPII_REQ_DESC_POST_LOW, 
	    dwordn(&descriptor, 0));

	DNPRINTF(MPII_D_RW, "%s:   MPII_REQ_DESC_POST_HIGH (0x%08x) write "
	    "0x%08x\n", DEVNAME(sc), MPII_REQ_DESC_POST_HIGH, 
	    dwordn(&descriptor, 1));

	/* XXX make this 64 bit? */
	mpii_write(sc, MPII_REQ_DESC_POST_LOW, htole32(dwordn(&descriptor, 0)));
	mpii_write(sc, MPII_REQ_DESC_POST_HIGH, 
	    htole32(dwordn(&descriptor, 1)));
}

int
mpii_complete(struct mpii_softc *sc, struct mpii_ccb *ccb, int timeout)
{
	int			smid = -1;
	
	DNPRINTF(MPII_D_INTR, "%s: mpii_complete timeout %d\n", DEVNAME(sc),
	    timeout); 
	
	do {
		/* avoid excessive polling */
		if (!mpii_reply_waiting(sc)) {
			if (timeout-- == 0)
				return (1);

			delay(1000);
			continue;
		}
		
		smid = mpii_reply(sc);
	
		/* generates PCI write every completed reply, but
		 * prevents deadlock waiting for specific smid
		 */
		mpii_write_reply_post(sc, sc->sc_reply_post_host_index);
		
		DNPRINTF(MPII_D_INTR, "%s: mpii_complete call to mpii_reply returned: %d\n",
		    DEVNAME(sc), smid);

	} while (ccb->ccb_smid != smid);

	return (0);
}

int
mpii_alloc_queues(struct mpii_softc *sc)
{
	u_int32_t		*kva;
	u_int64_t		*kva64;
	int			i; 
	
	DNPRINTF(MPII_D_MISC, "%s: mpii_alloc_queues\n", DEVNAME(sc));

	sc->sc_reply_freeq = mpii_dmamem_alloc(sc,
	    sc->sc_reply_free_qdepth * 4);
	if (sc->sc_reply_freeq == NULL)
		return (1);
	
	kva = MPII_DMA_KVA(sc->sc_reply_freeq);
	for (i = 0; i < sc->sc_num_reply_frames; i++) {
		kva[i] = (u_int32_t)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i;

		DNPRINTF(MPII_D_MISC, "%s:   %d:  0x%08x = 0x%08x\n",
		    DEVNAME(sc), i,
		    &kva[i], (u_int32_t)MPII_DMA_DVA(sc->sc_replies) +
		    MPII_REPLY_SIZE * i);
	}

	sc->sc_reply_postq = 
	    mpii_dmamem_alloc(sc, sc->sc_reply_post_qdepth * 8);
	if (sc->sc_reply_postq == NULL) 
		goto free_reply_freeq;
	sc->sc_reply_postq_kva = MPII_DMA_KVA(sc->sc_reply_postq);
	
	DNPRINTF(MPII_D_MISC, "%s:  populating reply post descriptor queue\n", 
	    DEVNAME(sc));
	kva64 = (u_int64_t *)MPII_DMA_KVA(sc->sc_reply_postq);
	for (i = 0; i < sc->sc_reply_post_qdepth; i++) {
		kva64[i] = 0xffffffffffffffffllu;
		DNPRINTF(MPII_D_MISC, "%s:    %d:  0x%08x = 0x%lx\n", 
		    DEVNAME(sc), i, &kva64[i], kva64[i]); 
	}

	return (0);

free_reply_freeq:

	mpii_dmamem_free(sc, sc->sc_reply_freeq);
	return (1);
}

void
mpii_init_queues(struct mpii_softc *sc)
{
	DNPRINTF(MPII_D_MISC, "%s:  mpii_init_queues\n", DEVNAME(sc));

	sc->sc_reply_free_host_index = sc->sc_reply_free_qdepth - 1;
	sc->sc_reply_post_host_index = 0;
	mpii_write_reply_free(sc, sc->sc_reply_free_host_index);
	mpii_write_reply_post(sc, sc->sc_reply_post_host_index);
}

int
mpii_poll(struct mpii_softc *sc, struct mpii_ccb *ccb, int timeout)
{
	int			error;
	int			s;

	DNPRINTF(MPII_D_CMD, "%s: mpii_poll\n", DEVNAME(sc));

	DNPRINTF(MPII_D_CMD, "   ccb->ccb_cmd: 0x%08x\n", DEVNAME(sc), ccb->ccb_cmd);
	s = splbio();
	mpii_start(sc, ccb);
	error = mpii_complete(sc, ccb, timeout);
	splx(s);

	return (error);
}

int
mpii_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mpii_softc	*sc = link->adapter_softc;
	struct mpii_ccb		*ccb;
	struct mpii_ccb_bundle	*mcb;
	struct mpii_msg_scsi_io	*io;
	int			s;

	DNPRINTF(MPII_D_CMD, "%s: mpii_scsi_cmd\n", DEVNAME(sc));

	if (xs->cmdlen > MPII_CDB_LEN) {
		DNPRINTF(MPII_D_CMD, "%s: CBD too big %d\n",
		    DEVNAME(sc), xs->cmdlen);
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20;
		xs->error = XS_SENSE;
		xs->flags |= ITSDONE;
		s = splbio();
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	s = splbio();
	ccb = mpii_get_ccb(sc);
	splx(s);
	if (ccb == NULL)
		return (NO_CCB);
	
	/* XXX */
	if (sc->sc_mpii_dev[link->target] == NULL) {
		DNPRINTF(MPII_D_MAP, "%s: mpii_scsi_cmd nonexistent tid %d\n", 
		    DEVNAME(sc), link->target);
		return (99);
	}

	/* XXX */
	if (sc->sc_mpii_dev[link->target]->flags & MPII_DEV_UNUSED) {
		DNPRINTF(MPII_D_MAP, "%s: mpii_scsi_cmd tid %d is "
		    "MPII_DEV_UNUSED\n", DEVNAME(sc), link->target);
		return (99);
	}

	/* XXX */
	if (sc->sc_mpii_dev[link->target]->flags & MPII_DEV_HIDDEN) {
		DNPRINTF(MPII_D_MAP, "%s: mpii_scsi_cmd tid %d is "
		    "MPII_DEV_HIDDEN\n", DEVNAME(sc), link->target);
		return (99);
	}

	DNPRINTF(MPII_D_CMD, "%s: ccb_smid: %d xs->flags: 0x%x\n",
	    DEVNAME(sc), ccb->ccb_smid, xs->flags);

	ccb->ccb_xs = xs;
	ccb->ccb_done = mpii_scsi_cmd_done;

	mcb = ccb->ccb_cmd;
	io = &mcb->mcb_io;

	io->function = MPII_FUNCTION_SCSI_IO_REQUEST;
	io->sense_buffer_length = sizeof(xs->sense);
	io->sgl_offset0 = 24; /* XXX fix this */
	io->io_flags = htole16(xs->cmdlen);
	io->dev_handle = htole16(sc->sc_mpii_dev[link->target]->dev_handle);
	io->lun[0] = htobe16(link->lun);

	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		io->direction = MPII_SCSIIO_DIR_READ;
		break;
	case SCSI_DATA_OUT:
		io->direction = MPII_SCSIIO_DIR_WRITE;
		break;
	default:
		io->direction = MPII_SCSIIO_DIR_NONE;
		break;
	}

	io->tagging = MPII_SCSIIO_ATTR_SIMPLE_Q;

	bcopy(xs->cmd, io->cdb, xs->cmdlen);

	io->data_length = htole32(xs->datalen);

	io->sense_buffer_low_address = htole32(ccb->ccb_cmd_dva +
	    ((u_int8_t *)&mcb->mcb_sense - (u_int8_t *)mcb));

	if (mpii_load_xs(ccb) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		s = splbio();
		mpii_put_ccb(sc, ccb);
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}

	timeout_set(&xs->stimeout, mpii_timeout_xs, ccb);

	DNPRINTF(MPII_D_CMD, "%s:  sizeof(mpii_msg_scsi_io): %d "
	    "sizeof(mpii_ccb_bundle): %d sge offset: 0x%02x\n",
	    DEVNAME(sc), sizeof(struct mpii_msg_scsi_io),
	    sizeof(struct mpii_ccb_bundle), 
	    (u_int8_t *)&mcb->mcb_sgl[0] - (u_int8_t *)mcb);
	
	DNPRINTF(MPII_D_CMD, "%s   sgl[0]: 0x%04x 0%04x 0x%04x\n",
	    DEVNAME(sc), mcb->mcb_sgl[0].sg_hdr, mcb->mcb_sgl[0].sg_lo_addr,
	    mcb->mcb_sgl[0].sg_hi_addr);

	DNPRINTF(MPII_D_CMD, "%s:  Offset0: 0x%02x\n", DEVNAME(sc),
	    io->sgl_offset0); 

	if (xs->flags & SCSI_POLL) {
		if (mpii_poll(sc, ccb, xs->timeout) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
			xs->flags |= ITSDONE;
			s = splbio();
			scsi_done(xs);
			splx(s);
		}
		return (COMPLETE);
	}

	DNPRINTF(MPII_D_CMD, "%s:    mpii_scsi_cmd(): opcode: %02x datalen: %d "
	    "req_sense_len: %d\n", DEVNAME(sc), xs->cmd->opcode,
	    xs->datalen, xs->req_sense_length);

	s = splbio();
	mpii_start(sc, ccb);
	splx(s);
	return (SUCCESSFULLY_QUEUED);
}

void
mpii_scsi_cmd_done(struct mpii_ccb *ccb)
{
	struct mpii_msg_scsi_io_error	*sie;
	struct mpii_softc	*sc = ccb->ccb_sc;
	struct scsi_xfer	*xs = ccb->ccb_xs;
	struct mpii_ccb_bundle	*mcb = ccb->ccb_cmd;
	bus_dmamap_t		dmap = ccb->ccb_dmamap;

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, dmap);
	}

	/* timeout_del */
	xs->error = XS_NOERROR;
	xs->resid = 0;
	xs->flags |= ITSDONE;

	if (ccb->ccb_rcb == NULL) {
		/* no scsi error, we're ok so drop out early */
		xs->status = SCSI_OK;
		mpii_put_ccb(sc, ccb);
		scsi_done(xs);
		return;
	}

	sie = ccb->ccb_rcb->rcb_reply;

	DNPRINTF(MPII_D_CMD, "%s: mpii_scsi_cmd_done xs cmd: 0x%02x len: %d "
	    "flags 0x%x\n", DEVNAME(sc), xs->cmd->opcode, xs->datalen,
	    xs->flags);
	DNPRINTF(MPII_D_CMD, "%s:  dev_handle: %d msg_length: %d "
	    "function: 0x%02x\n", DEVNAME(sc), letoh16(sie->dev_handle), 
	    sie->msg_length, sie->function);
	DNPRINTF(MPII_D_CMD, "%s:  vp_id: 0x%02x vf_id: 0x%02x\n", DEVNAME(sc),
	    sie->vp_id, sie->vf_id);
	DNPRINTF(MPII_D_CMD, "%s:  scsi_status: 0x%02x scsi_state: 0x%02x "
	    "ioc_status: 0x%04x\n", DEVNAME(sc), sie->scsi_status,
	    sie->scsi_state, letoh16(sie->ioc_status));
	DNPRINTF(MPII_D_CMD, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(sie->ioc_loginfo));
	DNPRINTF(MPII_D_CMD, "%s:  transfer_count: %d\n", DEVNAME(sc),
	    letoh32(sie->transfer_count));
	DNPRINTF(MPII_D_CMD, "%s:  sense_count: %d\n", DEVNAME(sc),
	    letoh32(sie->sense_count));
	DNPRINTF(MPII_D_CMD, "%s:  response_info: 0x%08x\n", DEVNAME(sc),
	    letoh32(sie->response_info));
	DNPRINTF(MPII_D_CMD, "%s:  task_tag: 0x%04x\n", DEVNAME(sc),
	    letoh16(sie->task_tag));
	DNPRINTF(MPII_D_CMD, "%s:  bidirectional_transfer_count: 0x%08x\n",
	    DEVNAME(sc), letoh32(sie->bidirectional_transfer_count));
	
	xs->status = sie->scsi_status;
	switch (letoh16(sie->ioc_status)) {
	case MPII_IOCSTATUS_SCSI_DATA_UNDERRUN:
		xs->resid = xs->datalen - letoh32(sie->transfer_count);
		if (sie->scsi_state & MPII_SCSIIO_ERR_STATE_NO_SCSI_STATUS) {
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/* FALLTHROUGH */
	case MPII_IOCSTATUS_SUCCESS:
	case MPII_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (xs->status) {
		case SCSI_OK:
			xs->resid = 0;
			break;

		case SCSI_CHECK:
			xs->error = XS_SENSE;
			break;

		case SCSI_BUSY:
		case SCSI_QUEUE_FULL:
			xs->error = XS_BUSY;
			break;

		default:
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case MPII_IOCSTATUS_BUSY:
	case MPII_IOCSTATUS_INSUFFICIENT_RESOURCES:
		xs->error = XS_BUSY;
		break;

	case MPII_IOCSTATUS_SCSI_INVALID_DEVHANDLE:
	case MPII_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		xs->error = XS_SELTIMEOUT;
		break;

	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (sie->scsi_state & MPII_SCSIIO_ERR_STATE_AUTOSENSE_VALID)
		bcopy(&mcb->mcb_sense, &xs->sense, sizeof(xs->sense));


	DNPRINTF(MPII_D_CMD, "%s:  xs err: 0x%02x status: %d\n", DEVNAME(sc),
	    xs->error, xs->status);

	mpii_push_reply(sc, ccb->ccb_rcb->rcb_reply_dva);
	mpii_put_ccb(sc, ccb);
	scsi_done(xs);
}
