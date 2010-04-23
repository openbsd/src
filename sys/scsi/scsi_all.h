/*	$OpenBSD: scsi_all.h,v 1.48 2010/04/23 01:39:05 dlg Exp $	*/
/*	$NetBSD: scsi_all.h,v 1.10 1996/09/12 01:57:17 thorpej Exp $	*/

/*
 * SCSI general  interface description
 */

/*
 * Largely written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with 
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#ifndef	_SCSI_SCSI_ALL_H
#define _SCSI_SCSI_ALL_H 1

/*
 * SCSI command format
 */

/*
 * Define some bits that are in ALL (or a lot of) scsi commands
 */
#define SCSI_CTL_LINK		0x01
#define SCSI_CTL_FLAG		0x02
#define SCSI_CTL_VENDOR		0xC0


/*
 * Some old SCSI devices need the LUN to be set in the top 3 bits of the
 * second byte of the CDB.
 */
#define	SCSI_CMD_LUN_MASK	0xe0
#define	SCSI_CMD_LUN_SHIFT	5


struct scsi_generic {
	u_int8_t opcode;
	u_int8_t bytes[15];
};

struct scsi_test_unit_ready {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_send_diag {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SSD_UOL		0x01
#define	SSD_DOL		0x02
#define	SSD_SELFTEST	0x04
#define	SSD_PF		0x10
	u_int8_t unused[1];
	u_int8_t paramlen[2];
	u_int8_t control;
};

struct scsi_sense {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_inquiry {
	u_int8_t opcode;
	u_int8_t flags;
#define SI_EVPD		0x01
	u_int8_t pagecode;
#define SI_PG_SUPPORTED	0x00
#define SI_PG_SERIAL	0x80
#define SI_PG_DEVID	0x83
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_mode_sense {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SMS_DBD				0x08	/* Disable Block Descriptors */
	u_int8_t page;
#define	SMS_PAGE_CODE 			0x3F
#define	SMS_PAGE_CTRL 			0xC0
#define	SMS_PAGE_CTRL_CURRENT 		0x00
#define	SMS_PAGE_CTRL_CHANGEABLE 	0x40
#define	SMS_PAGE_CTRL_DEFAULT 		0x80
#define	SMS_PAGE_CTRL_SAVED 		0xC0
	u_int8_t unused;
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_sense_big {
	u_int8_t opcode;
	u_int8_t byte2;				/* same bits as small version */
#define SMS_LLBAA			0x10	/*    plus: Long LBA Accepted */
	u_int8_t page;				/* same bits as small version */
	u_int8_t unused[4];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_mode_select {
	u_int8_t opcode;
	u_int8_t byte2;
#define	SMS_SP	0x01
#define	SMS_PF	0x10
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_select_big {
	u_int8_t opcode;
	u_int8_t byte2;		/* same bits as small version */
	u_int8_t unused[5];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_reserve {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_release {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_prevent {
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t how;
	u_int8_t control;
};
#define	PR_PREVENT 0x01
#define PR_ALLOW   0x00

struct scsi_report_luns {
	u_int8_t opcode;
	u_int8_t unused;
	u_int8_t selectreport;
#define	REPORT_NORMAL		0x00
#define	REPORT_WELLKNOWN	0x01
#define	REPORT_ALL		0x02
	u_int8_t unused2[3];
	u_int8_t length[4];
	u_int8_t unused4;
	u_int8_t control;
};

/*
 * Opcodes
 */
#define	TEST_UNIT_READY		0x00
#define REQUEST_SENSE		0x03
#define INQUIRY			0x12
#define MODE_SELECT		0x15
#define RESERVE			0x16
#define RELEASE			0x17
#define MODE_SENSE		0x1a
#define START_STOP		0x1b
#define RECEIVE_DIAGNOSTIC	0x1c
#define SEND_DIAGNOSTIC		0x1d
#define PREVENT_ALLOW		0x1e
#define POSITION_TO_ELEMENT	0x2b
#define WRITE_BUFFER		0x3b
#define READ_BUFFER		0x3c
#define	CHANGE_DEFINITION	0x40
#define	MODE_SELECT_BIG		0x55
#define	MODE_SENSE_BIG		0x5a
#define	REPORT_LUNS		0xa0

/*
 * Sort of an extra one, for SCSI_RESET.
 */
#define GENRETRY		1

/*
 * sense data format
 */
#define T_DIRECT	0
#define T_SEQUENTIAL	1
#define T_PRINTER	2
#define T_PROCESSOR	3
#define T_WORM		4
#define T_CDROM		5
#define T_SCANNER 	6
#define T_OPTICAL 	7
#define T_RDIRECT 	14
#define T_NODEVICE	0x1F

#define T_CHANGER	8
#define T_COMM		9
#define	T_ENCLOSURE	13

#define T_REMOV		1
#define	T_FIXED		0

struct scsi_inquiry_data {
	u_int8_t device;
#define	SID_TYPE	0x1F
#define	SID_QUAL	0xE0
#define	SID_QUAL_LU_OK	0x00
#define	SID_QUAL_LU_OFFLINE	0x20
#define	SID_QUAL_RSVD	0x40
#define	SID_QUAL_BAD_LU	0x60
	u_int8_t dev_qual2;
#define	SID_QUAL2	0x7F
#define	SID_REMOVABLE	0x80
	u_int8_t version;
#define SID_ANSII	0x07
#define SID_ECMA	0x38
#define SID_ISO		0xC0
	u_int8_t response_format;
	u_int8_t additional_length;
#define SID_INQUIRY_HDR	5	/* Bytes up to & including additional_length */
#define SID_SCSI2_ALEN	31	/* Additional bytes of basic SCSI2 info */
	u_int8_t unused[2];
	u_int8_t flags;
#define	SID_SftRe	0x01
#define	SID_CmdQue	0x02
#define	SID_Linked	0x08
#define	SID_Sync	0x10
#define	SID_WBus16	0x20
#define	SID_WBus32	0x40
#define	SID_RelAdr	0x80
	char	vendor[8];
	char	product[16];
	char	revision[4];
	u_int8_t extra[20];
	u_int8_t flags2;
#define SID_IUS		0x01
#define SID_QAS		0x02
#define SID_CLOCKING	0x0c /* 0 == ST only, 1 == DT only, 3 == both */
	u_int8_t reserved;
};

struct scsi_vpd_hdr {
	u_int8_t device;
	u_int8_t page_code;
	u_int8_t reserved;
	u_int8_t page_length;
};

struct scsi_vpd_serial {
	struct scsi_vpd_hdr hdr;
	char serial[32];
};

#define VPD_PROTO_ID_FC		0x0 /* Fibre Channel */
#define VPD_PROTO_ID_SPI	0x1 /* Parallel SCSI */
#define VPD_PROTO_ID_SSA	0x2
#define VPD_PROTO_ID_IEEE1394	0x3
#define VPD_PROTO_ID_SRP	0x4 /* SCSI RDMA Protocol */
#define VPD_PROTO_ID_ISCSI	0x5 /* Internet SCSI (iSCSI) */
#define VPD_PROTO_ID_SAS	0x6 /* Serial Attached SCSI */
#define VPD_PROTO_ID_ADT	0x7 /* Automation/Drive Interface Transport */
#define VPD_PROTO_ID_ATA	0x7 /* ATA/ATAPI */
#define VPD_PROTO_ID_NONE	0xf

struct scsi_vpd_devid_hdr {
	u_int8_t pi_code;
#define VPD_DEVID_PI(_f)	(((_f) >> 4) & 0x0f)
#define VPD_DEVID_CODE(_f)	(((_f) >> 0) & 0x0f)
#define VPD_DEVID_CODE_BINARY		0x1
#define VPD_DEVID_CODE_ASCII		0x2
#define VPD_DEVID_CODE_UTF8		0x3
	u_int8_t flags;
#define VPD_DEVID_PIV		0x80
#define VPD_DEVID_ASSOC(_f)	((_f) & 0x30)
#define VPD_DEVID_ASSOC_LU		0x00
#define VPD_DEVID_ASSOC_PORT		0x10
#define VPD_DEVID_ASSOC_TARG		0x20
#define VPD_DEVID_TYPE(_f)	((_f) & 0x0f)
#define VPD_DEVID_TYPE_VENDOR		0x0
#define VPD_DEVID_TYPE_T10		0x1
#define VPD_DEVID_TYPE_EUI64		0x2
#define VPD_DEVID_TYPE_NAA		0x3
#define VPD_DEVID_TYPE_RELATIVE		0x4
#define VPD_DEVID_TYPE_PORT		0x5
#define VPD_DEVID_TYPE_LU		0x6
#define VPD_DEVID_TYPE_MD5		0x7
#define VPD_DEVID_TYPE_NAME		0x8
	u_int8_t reserved;
	u_int8_t len;
};

struct scsi_sense_data_unextended {
/* 1*/	u_int8_t error_code;
/* 4*/	u_int8_t block[3];
};

struct scsi_sense_data {
/* 1*/	u_int8_t error_code;
#define	SSD_ERRCODE_CURRENT	0x70
#define	SSD_ERRCODE_DEFERRED	0x71
#define	SSD_ERRCODE		0x7F
#define	SSD_ERRCODE_VALID	0x80
/* 2*/	u_int8_t segment;
/* 3*/	u_int8_t flags;
#define	SSD_KEY		0x0F
#define	SSD_ILI		0x20
#define	SSD_EOM		0x40
#define	SSD_FILEMARK	0x80
/* 7*/	u_int8_t info[4];
/* 8*/	u_int8_t extra_len;
/*12*/	u_int8_t cmd_spec_info[4];
/*13*/	u_int8_t add_sense_code;
/*14*/	u_int8_t add_sense_code_qual;
/*15*/	u_int8_t fru;
/*16*/	u_int8_t sense_key_spec_1;
#define	SSD_SCS_VALID		0x80
#define SSD_SCS_CDB_ERROR	0x40
#define SSD_SCS_SEGMENT_DESC	0x20
#define SSD_SCS_VALID_BIT_INDEX	0x08
#define SSD_SCS_BIT_INDEX	0x07
/*17*/	u_int8_t sense_key_spec_2;
/*18*/	u_int8_t sense_key_spec_3;
/*32*/	u_int8_t extra_bytes[14];
};

#define SKEY_NO_SENSE		0x00
#define SKEY_RECOVERED_ERROR	0x01
#define SKEY_NOT_READY		0x02
#define SKEY_MEDIUM_ERROR	0x03
#define SKEY_HARDWARE_ERROR	0x04
#define SKEY_ILLEGAL_REQUEST	0x05
#define SKEY_UNIT_ATTENTION	0x06
#define SKEY_WRITE_PROTECT	0x07
#define SKEY_BLANK_CHECK	0x08
#define SKEY_VENDOR_UNIQUE	0x09
#define SKEY_COPY_ABORTED	0x0A
#define SKEY_ABORTED_COMMAND	0x0B
#define SKEY_EQUAL		0x0C
#define SKEY_VOLUME_OVERFLOW	0x0D
#define SKEY_MISCOMPARE		0x0E
#define SKEY_RESERVED		0x0F


/* Additional sense code info */
#define ASC_ASCQ(ssd)	((ssd->add_sense_code << 8) | ssd->add_sense_code_qual)

#define SENSE_NOT_READY_BECOMING_READY		0x0401
#define SENSE_NOT_READY_INIT_REQUIRED		0x0402
#define SENSE_NOT_READY_FORMAT			0x0404
#define SENSE_NOT_READY_REBUILD			0x0405
#define SENSE_NOT_READY_RECALC			0x0406
#define SENSE_NOT_READY_INPROGRESS		0x0407
#define SENSE_NOT_READY_LONGWRITE		0x0408
#define SENSE_NOT_READY_SELFTEST		0x0409
#define SENSE_POWER_RESET_OR_BUS		0x2900
#define SENSE_POWER_ON				0x2901
#define SENSE_BUS_RESET				0x2902
#define SENSE_BUS_DEVICE_RESET			0x2903
#define SENSE_DEVICE_INTERNAL_RESET		0x2904
#define SENSE_TSC_CHANGE_SE			0x2905
#define SENSE_TSC_CHANGE_LVD			0x2906
#define SENSE_IT_NEXUS_LOSS			0x2907
#define SENSE_BAD_MEDIUM			0x3000
#define SENSE_NR_MEDIUM_UNKNOWN_FORMAT		0x3001
#define SENSE_NR_MEDIUM_INCOMPATIBLE_FORMAT	0x3002
#define SENSE_NW_MEDIUM_UNKNOWN_FORMAT		0x3004
#define SENSE_NW_MEDIUM_INCOMPATIBLE_FORMAT	0x3005
#define SENSE_NF_MEDIUM_INCOMPATIBLE_FORMAT	0x3006
#define SENSE_NW_MEDIUM_AC_MISMATCH		0x3008
#define SENSE_NOMEDIUM				0x3A00
#define SENSE_NOMEDIUM_TCLOSED			0x3A01
#define SENSE_NOMEDIUM_TOPEN			0x3A02
#define SENSE_NOMEDIUM_LOADABLE			0x3A03
#define SENSE_NOMEDIUM_AUXMEM			0x3A04
#define SENSE_CARTRIDGE_FAULT			0x5200
#define SENSE_MEDIUM_REMOVAL_PREVENTED		0x5302

struct scsi_blk_desc {
	u_int8_t density;
	u_int8_t nblocks[3];
	u_int8_t reserved;
	u_int8_t blklen[3];
};

struct scsi_direct_blk_desc {
	u_int8_t nblocks[4];
	u_int8_t density;
	u_int8_t blklen[3];
};

struct scsi_blk_desc_big {
	u_int8_t nblocks[8];
	u_int8_t density;
	u_int8_t reserved[3];
	u_int8_t blklen[4];
};

struct scsi_mode_header {
	u_int8_t data_length;		/* Sense data length */
	u_int8_t medium_type;
	u_int8_t dev_spec;
	u_int8_t blk_desc_len;
};

struct scsi_mode_header_big {
	u_int8_t data_length[2];	/* Sense data length */
	u_int8_t medium_type;
	u_int8_t dev_spec;
	u_int8_t reserved;
#define LONGLBA	0x01
	u_int8_t reserved2;
	u_int8_t blk_desc_len[2];
};

union scsi_mode_sense_buf {
	struct scsi_mode_header hdr;
	struct scsi_mode_header_big hdr_big;
	u_char buf[254];	/* 255 & 256 bytes breaks some devices. */
				/* ahci doesn't like 255, various don't like */
				/* 256 because length must fit in 8 bits. */
} __packed;			/* Ensure sizeof() is 254! */

struct scsi_report_luns_data {
	u_int8_t length[4];	/* length of LUN inventory, in bytes */
	u_int8_t reserved[4];	/* unused */
	/*
	 * LUN inventory- we only support the type zero form for now.
	 */
#define RPL_LUNDATA_SIZE 8	/* Bytes per lun */
	struct {
		u_int8_t lundata[RPL_LUNDATA_SIZE];
	} luns[256];		/* scsi_link->luns is u_int8_t. */
};
#define	RPL_LUNDATA_T0LUN	1	/* Type 0 LUN is in lundata[1] */

/*
 * ATA PASS-THROUGH as per SAT2
 */

#define ATA_PASSTHRU_12		0xa1
#define ATA_PASSTHRU_16		0x85

#define ATA_PASSTHRU_PROTO_MASK		0x1e
#define ATA_PASSTHRU_PROTO_HW_RESET	0x00
#define ATA_PASSTHRU_PROTO_SW_RESET	0x02
#define ATA_PASSTHRU_PROTO_NON_DATA	0x06
#define ATA_PASSTHRU_PROTO_PIO_DATAIN	0x08
#define ATA_PASSTHRU_PROTO_PIO_DATAOUT	0x0a
#define ATA_PASSTHRU_PROTO_DMA		0x0c
#define ATA_PASSTHRU_PROTO_DMA_QUEUED	0x0e
#define ATA_PASSTHRU_PROTO_EXEC_DIAG	0x10
#define ATA_PASSTHRU_PROTO_NON_DATA_RST	0x12
#define ATA_PASSTHRU_PROTO_UDMA_DATAIN	0x14
#define ATA_PASSTHRU_PROTO_UDMA_DATAOUT	0x16
#define ATA_PASSTHRU_PROTO_FPDMA	0x18
#define ATA_PASSTHRU_PROTO_RESPONSE	0x1e

#define ATA_PASSTHRU_T_DIR_MASK		0x08
#define ATA_PASSTHRU_T_DIR_READ		0x08
#define ATA_PASSTHRU_T_DIR_WRITE	0x00

#define ATA_PASSTHRU_T_LEN_MASK		0x03
#define ATA_PASSTHRU_T_LEN_NONE		0x00
#define ATA_PASSTHRU_T_LEN_FEATURES	0x01
#define ATA_PASSTHRU_T_LEN_SECTOR_COUNT	0x02
#define ATA_PASSTHRU_T_LEN_TPSIU	0x03

struct scsi_ata_passthru_12 {
	u_int8_t opcode;
	u_int8_t count_proto;
	u_int8_t flags;
	u_int8_t features;
	u_int8_t sector_count;
	u_int8_t lba_low;
	u_int8_t lba_mid;
	u_int8_t lba_high;
	u_int8_t device;
	u_int8_t command;
	u_int8_t _reserved;
	u_int8_t control;
};

struct scsi_ata_passthru_16 {
	u_int8_t opcode;
	u_int8_t count_proto;
	u_int8_t flags;
	u_int8_t features[2];
	u_int8_t sector_count[2];
	u_int8_t lba_low[2];
	u_int8_t lba_mid[2];
	u_int8_t lba_high[2];
	u_int8_t device;
	u_int8_t command;
	u_int8_t control;
};

/*
 * SPI status information unit. See section 14.3.5 of SPI-3.
 */
struct scsi_status_iu_header {
/* 2*/	u_int8_t reserved[2];
/* 3*/	u_int8_t flags;
#define	SIU_SNSVALID 0x2
#define	SIU_RSPVALID 0x1
/* 4*/	u_int8_t status;
/* 8*/	u_int8_t sense_length[4];
/*12*/	u_int8_t pkt_failures_length[4];
	u_int8_t data[1]; /* <pkt failure list><sense data> OR <sense_data> */
};

#define SIU_PKTFAIL_CODE(siu)	((siu)->data[3])
#define		SIU_PFC_NONE			0x00
#define		SIU_PFC_CIU_FIELDS_INVALID	0x02
#define		SIU_PFC_TMF_NOT_SUPPORTED	0x04
#define		SIU_PFC_TMF_FAILED		0x05
#define		SIU_PFC_INVALID_TYPE_CODE	0x06
#define		SIU_PFC_ILLEGAL_REQUEST		0x07

#define SIU_SENSE_LENGTH(siu)	(_4btol((siu)->sense_length))
#define SIU_SENSE_DATA(siu)	(((siu)->flags & SIU_RSPVALID) ?	\
   &(siu)->data[_4btol((siu)->pkt_failures_length)] : &(siu)->data[0])

/*
 * Values for 'Task Management Flags' field of SPI command information unit.
 * See section 14.3.1 of SPI-3.
 */
#define	SIU_TASKMGMT_NONE		0x00
#define	SIU_TASKMGMT_ABORT_TASK		0x01
#define	SIU_TASKMGMT_ABORT_TASK_SET	0x02
#define	SIU_TASKMGMT_CLEAR_TASK_SET	0x04
#define	SIU_TASKMGMT_LUN_RESET		0x08
#define	SIU_TASKMGMT_TARGET_RESET	0x20
#define	SIU_TASKMGMT_CLEAR_ACA		0x40

/*
 * Status Byte
 */
#define SCSI_OK			0x00
#define SCSI_CHECK		0x02
#define SCSI_COND_MET		0x04
#define SCSI_BUSY		0x08
#define SCSI_INTERM		0x10
#define SCSI_INTERM_COND_MET	0x14
#define SCSI_RESV_CONFLICT	0x18
#define SCSI_TERMINATED		0x22
#define SCSI_QUEUE_FULL		0x28	/* Old (Pre SCSI-3) name */
#define SCSI_TASKSET_FULL	0x28	/* New (SCSI-3)     name */
#define SCSI_ACA_ACTIVE		0x30

#endif /* _SCSI_SCSI_ALL_H */
