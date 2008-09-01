/*	$OpenBSD: bhareg.h,v 1.4 2008/09/01 17:30:56 deraadt Exp $	*/
/*	$NetBSD: bhareg.h,v 1.12 1998/08/17 00:26:33 mycroft Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
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
 */

typedef u_int8_t physaddr[4];
typedef u_int8_t physlen[4];
#define	ltophys	_lto4l
#define	phystol	_4ltol

/*
 * I/O port offsets
 */
#define	BHA_CTRL_PORT		0	/* control (wo) */
#define	BHA_STAT_PORT		0	/* status (ro) */
#define	BHA_CMD_PORT		1	/* command (wo) */
#define	BHA_DATA_PORT		1	/* data (ro) */
#define	BHA_INTR_PORT		2	/* interrupt status (ro) */
#define	BHA_EXTGEOM_PORT	3	/* extended geometry (ro) */

/*
 * BHA_CTRL bits
 */
#define BHA_CTRL_HRST		0x80	/* Hardware reset */
#define BHA_CTRL_SRST		0x40	/* Software reset */
#define BHA_CTRL_IRST		0x20	/* Interrupt reset */
#define BHA_CTRL_SCRST		0x10	/* SCSI bus reset */

/*
 * BHA_STAT bits
 */
#define BHA_STAT_STST		0x80	/* Self test in Progress */
#define BHA_STAT_DIAGF		0x40	/* Diagnostic Failure */
#define BHA_STAT_INIT		0x20	/* Mbx Init required */
#define BHA_STAT_IDLE		0x10	/* Host Adapter Idle */
#define BHA_STAT_CDF		0x08	/* cmd/data out port full */
#define BHA_STAT_DF		0x04	/* Data in port full */
#define BHA_STAT_INVDCMD	0x01	/* Invalid command */
#define	BHA_STAT_BITS	"\020\1invcmd\3df\4cdf\5idle\6init\7diagf\10stst"

/*
 * BHA_CMD opcodes
 */
#define	BHA_NOP			0x00	/* No operation */
#define BHA_MBX_INIT		0x01	/* Mbx initialization */
#define BHA_START_SCSI		0x02	/* start scsi command */
#define	BHA_EXECUTE_BIOS_CMD	0x03	/* execute BIOS command */
#define BHA_INQUIRE_REVISION	0x04	/* Adapter Inquiry */
#define BHA_MBO_INTR_EN		0x05	/* Enable MBO available interrupt */
#define BHA_SEL_TIMEOUT_SET	0x06	/* set selection time-out */
#define BHA_BUS_ON_TIME_SET	0x07	/* set bus-on time */
#define BHA_BUS_OFF_TIME_SET	0x08	/* set bus-off time */
#define BHA_BUS_SPEED_SET	0x09	/* set bus transfer speed */
#define BHA_INQUIRE_DEVICES	0x0a	/* return installed devices 0-7 */
#define BHA_INQUIRE_CONFIG	0x0b	/* return configuration data */
#define BHA_TARGET_EN		0x0c	/* enable target mode */
#define BHA_INQUIRE_SETUP	0x0d	/* return setup data */
#define	BHA_WRITE_LRAM		0x1a	/* write adapter local RAM */
#define	BHA_READ_LRAM		0x1b	/* read adapter local RAM */
#define	BHA_WRITE_CHIP_FIFO	0x1c	/* write bus master chip FIFO */
#define	BHA_READ_CHIP_FIFO	0x1d	/* read bus master chip FIFO */
#define BHA_ECHO		0x1f	/* Echo command byte */
#define	BHA_ADAPTER_DIAGNOSTICS	0x20	/* host adapter diagnostics */
#define	BHA_SET_ADAPTER_OPTIONS	0x21	/* set adapter options */
#define BHA_INQUIRE_DEVICES_2	0x23	/* return installed devices 8-15 */
#define	BHA_INQUIRE_TARG_DEVS	0x24	/* inquire target devices */
#define	BHA_DISABLE_HAC_INTR	0x25	/* disable host adapter interrupt */
#define BHA_MBX_INIT_EXTENDED	0x81	/* Mbx initialization */
#define	BHA_EXECUTE_SCSI_CMD	0x83	/* execute SCSI command */
#define BHA_INQUIRE_REVISION_3	0x84	/* Get 3rd firmware version byte */
#define BHA_INQUIRE_REVISION_4	0x85	/* Get 4th firmware version byte */
#define	BHA_INQUIRE_PCI_INFO	0x86	/* get PCI host adapter information */
#define BHA_INQUIRE_MODEL	0x8b	/* Get hardware ID and revision */
#define	BHA_INQUIRE_PERIOD	0x8c	/* Get synchronous period */
#define BHA_INQUIRE_EXTENDED	0x8d	/* Adapter Setup Inquiry */
#define	BHA_ROUND_ROBIN		0x8f	/* Enable/Disable(default)
					   round robin */
#define	BHA_STORE_LRAM		0x90	/* store host adapter local RAM */
#define	BHA_FETCH_LRAM		0x91	/* fetch host adapter local RAM */
#define	BHA_SAVE_TO_EEPROM	0x92	/* store local RAM data in EEPROM */
#define	BHA_UPLOAD_AUTOSCSI	0x94	/* upload AutoSCSI code */
#define BHA_MODIFY_IOPORT	0x95	/* change or disable I/O port */
#define	BHA_SET_CCB_FORMAT	0x96	/* set CCB format (legacy/wide lun) */
#define	BHA_WRITE_INQUIRY_BUF	0x9a	/* write inquiry buffer */
#define	BHA_READ_INQUIRY_BUF	0x9b	/* read inquiry buffer */
#define	BHA_FLASH_UP_DOWNLOAD	0xa7	/* flash upload/downlod */
#define	BHA_READ_SCAM_DATA	0xa8	/* read SCAM data */
#define	BHA_WRITE_SCAM_DATA	0xa9	/* write SCAM data */

/*
 * BHA_INTR bits
 */
#define BHA_INTR_ANYINTR	0x80	/* Any interrupt */
#define BHA_INTR_SCRD		0x08	/* SCSI reset detected */
#define BHA_INTR_HACC		0x04	/* Command complete */
#define BHA_INTR_MBOA		0x02	/* MBX out empty */
#define BHA_INTR_MBIF		0x01	/* MBX in full */

struct bha_mbx_out {
	physaddr ccb_addr;
	u_int8_t reserved[3];
	u_int8_t cmd;
} __packed;

struct bha_mbx_in {
	physaddr ccb_addr;
	u_int8_t host_stat;
	u_int8_t target_stat;
	u_int8_t reserved;
	u_int8_t comp_stat;
} __packed;

/*
 * mbo.cmd values
 */
#define BHA_MBO_FREE	0x0	/* MBO entry is free */
#define BHA_MBO_START	0x1	/* MBO activate entry */
#define BHA_MBO_ABORT	0x2	/* MBO abort entry */

/*
 * mbi.comp_stat values
 */
#define BHA_MBI_FREE	0x0	/* MBI entry is free */
#define BHA_MBI_OK	0x1	/* completed without error */
#define BHA_MBI_ABORT	0x2	/* aborted ccb */
#define BHA_MBI_UNKNOWN	0x3	/* Tried to abort invalid CCB */
#define BHA_MBI_ERROR	0x4	/* Completed with error */

#if	defined(BIG_DMA)
WARNING...THIS WON'T WORK(won't fit on 1 page)
#if 0
#define      BHA_NSEG 2048    /* Number of scatter gather segments - to much vm */
#endif
#define	BHA_NSEG	128
#else
#define	BHA_NSEG	33
#endif /* BIG_DMA */

struct bha_scat_gath {
	physlen seg_len;
	physaddr seg_addr;
} __packed;

struct bha_ccb {
	u_int8_t	opcode;
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t				:3,
			data_in			:1,
			data_out		:1,
			wide_tag_enable		:1, /* Wide Lun CCB format */
			wide_tag_type		:2; /* Wide Lun CCB format */
#else
	u_int8_t	wide_tag_type		:2, /* Wide Lun CCB format */
			wide_tag_enable		:1, /* Wide Lun CCB format */
			data_out		:1,
			data_in			:1,
						:3;
#endif
	u_int8_t	scsi_cmd_length;
	u_int8_t	req_sense_length;
	/*------------------------------------longword boundary */
	physlen data_length;
	/*------------------------------------longword boundary */
	physaddr data_addr;
	/*------------------------------------longword boundary */
	u_int8_t	reserved1[2];
	u_int8_t	host_stat;
	u_int8_t	target_stat;
	/*------------------------------------longword boundary */
	u_int8_t	target;
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t	lun			:5,
			tag_enable		:1,
			tag_type		:2;
#else
	u_int8_t	tag_type		:2,
			tag_enable		:1,
			lun			:5;
#endif
	struct scsi_generic scsi_cmd;
	u_int8_t	reserved2[1];
	u_int8_t	link_id;
	/*------------------------------------longword boundary */
	physaddr link_addr;
	/*------------------------------------longword boundary */
	physaddr sense_ptr;
/*-----end of HW fields-----------------------longword boundary */
	struct scsi_sense_data scsi_sense;
	/*------------------------------------longword boundary */
	struct bha_scat_gath scat_gath[BHA_NSEG];
	/*------------------------------------longword boundary */
	TAILQ_ENTRY(bha_ccb) chain;
	struct bha_ccb *nexthash;
	bus_addr_t	hashkey;

	struct scsi_xfer *xs;		/* the scsipi_xfer for this cmd */

	int flags;
#define	CCB_ALLOC	0x01
#define	CCB_ABORT	0x02
#ifdef BHADIAG
#define	CCB_SENDING	0x04
#endif
	int timeout;

	/*
	 * This DMA map maps the buffer involved in the transfer.
	 * Its contents are loaded into "scat_gath" above.
	 */
	bus_dmamap_t	dmamap_xfer;
} __packed;

/*
 * opcode fields
 */
#define BHA_INITIATOR_CCB	0x00	/* SCSI Initiator CCB */
#define BHA_TARGET_CCB		0x01	/* SCSI Target CCB */
#define BHA_INIT_SCAT_GATH_CCB	0x02	/* SCSI Initiator with S/G */
#define	BHA_INIT_RESID_CCB	0x03	/* SCSI Initiator w/ residual */
#define	BHA_INIT_RESID_SG_CCB	0x04	/* SCSI Initiator w/ residual and S/G */
#define BHA_RESET_CCB		0x81	/* SCSI Bus reset */

/*
 * bha_ccb.host_stat values
 */
#define BHA_OK		0x00	/* cmd ok */
#define BHA_LINK_OK	0x0a	/* Link cmd ok */
#define BHA_LINK_IT	0x0b	/* Link cmd ok + int */
#define	BHA_DATA_UNDRN		0x0c	/* data underrun error */
#define BHA_SEL_TIMEOUT	0x11	/* Selection time out */
#define BHA_OVER_UNDER	0x12	/* Data over/under run */
#define BHA_BUS_FREE	0x13	/* Bus dropped at unexpected time */
#define BHA_INV_BUS	0x14	/* Invalid bus phase/sequence */
#define BHA_BAD_MBO	0x15	/* Incorrect MBO cmd */
#define BHA_BAD_CCB	0x16	/* Incorrect ccb opcode */
#define BHA_BAD_LINK	0x17	/* Not same values of LUN for links */
#define BHA_INV_TARGET	0x18	/* Invalid target direction */
#define BHA_CCB_DUP	0x19	/* Duplicate CCB received */
#define BHA_INV_CCB	0x1a	/* Invalid CCB or segment list */
#define	BHA_AUTOSENSE_FAILED	0x1b	/* auto REQUEST SENSE failed */
#define	BHA_TAGGED_MSG_REJ	0x1c	/* tagged queueing message rejected */
#define	BHA_UNSUP_MSG_RECVD	0x1d	/* unsupported message received */
#define	BHA_HARDWARE_FAILURE	0x20	/* host adapter hardware failure */
#define	BHA_TARG_IGNORED_ATN	0x21	/* target ignored ATN signal */
#define	BHA_HA_SCSI_BUS_RESET	0x22	/* host adapter asserted RST */
#define	BHA_OTHER_SCSI_BUS_RESET 0x23	/* other device asserted RST */
#define	BHA_BAD_RECONNECT	0x24	/* target reconnected improperly */
#define	BHA_HA_BUS_DEVICE_RESET	0x25	/* host adapter performed BDR */
#define	BHA_ABORT_QUEUE		0x26	/* abort queue generated */
#define	BHA_SOFTWARE_FAILURE	0x27	/* host adapter software failure */
#define	BHA_HARDWARE_WATCHDOG	0x30	/* host adapter watchdog timer fired */
#define	BHA_SCSI_PARITY_ERROR	0x34	/* SCSI parity error detected */

struct bha_extended_inquire {
	struct {
		u_char	opcode;
		u_char	len;
	} __packed cmd;
	struct {
		u_char	bus_type;	/* Type of bus connected to */
#define	BHA_BUS_TYPE_24BIT	'A'	/* ISA bus */
#define	BHA_BUS_TYPE_32BIT	'E'	/* EISA/VLB/PCI bus */
#define	BHA_BUS_TYPE_MCA	'M'	/* MicroChannel bus */
		u_char	bios_address;	/* Address of adapter BIOS */
		u_short sg_limit;
		u_char	mbox_count;
		u_char	mbox_baseaddr[4]; /* packed/unaligned u_int32_t */
		u_char	intrflags;
#define	BHA_INTR_FASTEISA	0x04
#define BHA_INTR_LEVEL	0x40		/* bit 6: level-sensitive interrupt */
		u_char	firmware_level[3]; /* last 3 digits of firmware rev */
		u_char	scsi_flags;	/* supported SCSI  features */
#define BHA_SCSI_WIDE		0x01	/* host adapter is wide */
#define BHA_SCSI_DIFFERENTIAL	0x02	/* host adapter is differential */
#define BHA_SCSI_SCAM		0x04	/* host adapter supports SCAM */
#define BHA_SCSI_ULTRA		0x08	/* host adapter supports Ultra */
#define BHA_SCSI_TERMINATION	0x10	/* host adapter supports smart
					   termination */
	} __packed reply;
} __packed;

struct bha_config {
	struct {
		u_char	opcode;
	} __packed cmd;
	struct {
		u_char  chan;
		u_char  intr;
#if BYTE_ORDER == LITTLE_ENDIAN
		u_char  scsi_dev :3,
				 :5;
#else
		u_char		 :5,
			scsi_dev :3;
#endif
	} __packed reply;
} __packed;

struct bha_toggle {
	struct {
		u_char	opcode;
		u_char	enable;
	} __packed cmd;
} __packed;

struct bha_mailbox {
	struct {
		u_char	opcode;
		u_char	nmbx;
		physaddr addr;
	} __packed cmd;
} __packed;

struct bha_model {
	struct {
		u_char	opcode;
		u_char	len;
	} __packed cmd;
	struct {
		u_char	id[4];		/* i.e bt742a -> '7','4','2','A' */
		u_char	version[2];	/* i.e Board Revision 'H' -> 'H', 0x00 */
	} __packed reply;
} __packed;

struct bha_revision {
	struct {
		u_char	opcode;
	} __packed cmd;
	struct {
		u_char  board_type;
		u_char  custom_feature;
		char    firm_revision;
		u_char  firm_version;
	} __packed reply;
} __packed;

struct bha_digit {
	struct {
		u_char	opcode;
	} __packed cmd;
	struct {
		u_char  digit;
	} __packed reply;
} __packed;

struct bha_devices {
	struct {
		u_char	opcode;
	} __packed cmd;
	struct {
		u_char	lun_map[8];
	} __packed reply;
} __packed;

struct bha_sync {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char	offset	:4,
		period	:3,
		valid	:1;
#else
	u_char	valid	:1,
		period	:3,
		offset	:4;
#endif
} __packed;

struct bha_setup_reply {
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int8_t	sync_neg	:1,
			parity		:1,
					:6;
#else
	u_int8_t			:6,
			parity		:1,
			sync_neg	:1;
#endif
	u_int8_t	speed;
	u_int8_t	bus_on;
	u_int8_t	bus_off;
	u_int8_t	num_mbx;
	u_int8_t	mbx[3];		/*XXX */
	/* doesn't make sense with 32bit addresses */
	struct bha_sync	sync[8];
	u_int8_t	disc_sts;
} __packed;

/* additional reply data supplied by wide controllers */
struct bha_setup_reply_wide {
	u_int8_t	signature;
	u_int8_t	letter_d;
	u_int8_t	ha_type;
	u_int8_t	low_wide_allowed;
	u_int8_t	low_wide_active;
	struct bha_sync	sync_high[8];
	u_int8_t	high_disc_info;
	u_int8_t	reserved;
	u_int8_t	high_wide_allowed;
	u_int8_t	high_wide_active;
} __packed;

struct bha_setup {
	struct {
		u_char	opcode;
		u_char	len;
	} __packed cmd;
	struct bha_setup_reply reply;
	struct bha_setup_reply_wide reply_w;	/* for wide controllers */
} __packed;

struct bha_period_reply {
	u_char	period[8];
} __packed;

struct bha_period {
	struct {
		u_char	opcode;
		u_char	len;
	} __packed cmd;
	struct bha_period_reply reply;
	struct bha_period_reply reply_w;	/* for wide controllers */
} __packed;

struct bha_isadisable {
	struct {
		u_char	opcode;
		u_char	modifier;
	} __packed cmd;
} __packed;

/*
 * bha_isadisable.modifier parameters
 */
#define BHA_IOMODIFY_330	0x00
#define BHA_IOMODIFY_334	0x01
#define BHA_IOMODIFY_DISABLE1	0x06
#define BHA_IOMODIFY_DISABLE2	0x07

#define INT9	0x01
#define INT10	0x02
#define INT11	0x04
#define INT12	0x08
#define INT14	0x20
#define INT15	0x40

#define EISADMA	0x00
#define CHAN0	0x01
#define CHAN5	0x20
#define CHAN6	0x40
#define CHAN7	0x80
