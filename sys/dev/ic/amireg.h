/*	$OpenBSD: amireg.h,v 1.5 2004/12/26 00:11:24 marco Exp $	*/

/*
 * Copyright (c) 2000 Michael Shalayeff
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define	AMI_MAX_PDRIVES		(75)
#define	AMI_MAX_LDRIVES		8
#define	AMI_MAX_SPANDEPTH	4
#define	AMI_MAX_DEVDEPTH	8

#define	AMI_BIG_MAX_PDRIVES	(256)
#define	AMI_BIG_MAX_LDRIVES	40
#define	AMI_BIG_MAX_SPANDEPTH	8
#define	AMI_BIG_MAX_DEVDEPTH	32

#define	AMI_MAXCMDS	126	/* theoretical limit is 250 */
#define	AMI_SECTOR_SIZE	512
#define	AMI_MAXOFFSETS	26
#define	AMI_SGEPERCMD	32	/* to prevent page boundary crossing */
#define AMI_MAX_BUSYWAIT 10	/* wait up to 10 usecs */

#define	AMI_MAXFER	(AMI_MAXOFFSETS * PAGE_SIZE)

#define	AMI_QIDB	0x20
#define		AMI_QIDB_EXEC	0x01
#define		AMI_QIDB_ACK	0x02
#define	AMI_QODB	0x2c
#define		AMI_QODB_READY	0x10001234

#define	AMI_SCMD	0x10
#define		AMI_SCMD_EXEC	0x10
#define		AMI_SCMD_ACK	0x08
#define	AMI_SMBSTAT	0x10
#define		AMI_SMBST_BUSY	0x10
#define	AMI_SIEM	0x11
#define		AMI_SEIM_ENA	0xc0
#define	AMI_SMBADDR	0x14
#define	AMI_SMBENA	0x18
#define	AMI_ISTAT	0x1a
#define		AMI_ISTAT_PEND	0x40

/* commands */
#define	AMI_READ	0x01
#define	AMI_WRITE	0x02
#define	AMI_PASSTHRU	0x03	/* pass scsi cdb to the device */
#define	AMI_EINQUIRY	0x04	/* extended inquiry */
#define	AMI_INQUIRY	0x05	/* inquiry */
#define	AMI_CHSTATE	0x06	/* pad[0] -- state */
#define	AMI_RCONFIG	0x07	/* read configuration up to 4 spans */
#define	AMI_REBUILDPD	0x08	/* rebuild physical drive */
#define		AMI_STATE_ON	3
#define		AMI_STATE_FAIL	4
#define		AMI_STATE_SPARE	6
#define	AMI_CHECK	0x09	/* check consistency */
#define	AMI_FLUSH	0x0a
#define	AMI_ILDRIVE	0x0b	/* init logical drive */
#define	AMI_EINQUIRY3	0x0c
#define	AMI_DCHDR	0x14	/* get/set dedicated channel/drives */
#define	AMI_GRBLDPROGR	0x18	/* get rebuild progress */
#define	AMI_GCHECKPROGR	0x19	/* get check consistency progress */
#define	AMI_GILDRPROGR	0x1b	/* get init logical drive progress */
#define	AMI_WRCONFIG	0x20	/* write configuration up to 4 spans */
#define	AMI_RWRCONFIG	0x21	/* raid write config */
#define	AMI_RRDCONFIG	0x22	/* raid read config */
#define	AMI_GRBLDRATE	0x23	/* get rebuild rate */
#define	AMI_SRBLDRATE	0x24	/* set rebuild rate */
#define	AMI_UPLDCFGUT	0x25	/* upload config utility */
#define	AMI_UPLDRVPROP	0x26	/* update logical drive property */
#define	AMI_ABRTREBLD	0x28	/* abort rebuild */
#define	AMI_ABRTCHECK	0x29	/* abort check consistency */
#define	AMI_ABRTILDRV	0x2b	/* abort init logical drive */
#define	AMI_WRBLOCK	0x2c	/* flash write block */
#define	AMI_PRGFLASH	0x2d	/* flash program */
#define	AMI_SFLUSHINTV	0x2e	/* channel == cache flush interval */
#define	AMI_PCHIPSETVAL	0x2f	/* program chipset values */
#define		AMI_CS_NEPTUNE	0x61
#define		AMI_CS_OTHER	0xe1
#define		AMI_CS_TRITON	0xe2
#define	AMI_SNEG	0x30	/* scsi sync negotiation get/ena/dis */
#define		AMI_SNEG_GET	1
#define		AMI_SNEG_SET	2
#define	AMI_QTAG	0x31	/* scsi queue tag get/set */
#define		AMI_QTAG_GET	1
#define		AMI_QTAG_SET	2
#define	AMI_GSUPARAM	0x32	/* get spinup parameters */
#define	AMI_SSUPARAM	0x33	/* set spinup parameters */
#define	AMI_GDROAMINFO	0x34
#define	AMI_GMACHID	0x36	/* get machine id */
#define	AMI_BIOSPDATA	0x40	/* get bios private data */
#define	AMI_I2OCFGDLG	0x41	/* I2O config dialog */
#define	AMI_GCACHESTAT	0x50	/* get cache statistics */
#define	AMI_SPEAKER	0x51	/* speaker control */
#define		AMI_SPKR_OFF	0
#define		AMI_SPKR_ON	1
#define		AMI_SPKR_SHUT	2
#define		AMI_SPKR_GVAL	3
#define	AMI_GDUMP	0x52	/* get error condition in text */
#define	AMI_SENSEDUMPA	0x53	/* get SCSI sense dump area */
#define	AMI_STDIAG	0x54	/* start diagnostics -- 2.1 */
#define	AMI_FRAID_PF	0x55	/* get/set flexraid power fail */
#define		AMI_GFRAIDPF	1
#define		AMI_SFRAIDPF	2
#define	AMI_FRAIDVS	0x56	/* get/set flexraid virtual sizing */
#define		AMI_GFRAIDVS	1
#define		AMI_SFRAIDVS	2
#define	AMI_BBMANAGE	0x57	/* bad block manage */
#define	AMI_RECONSTRUCT	0x60	/* begin reconstruction */
#define	AMI_GRECONSTRUCT 0x61	/* get reconstruction progress */
#define	AMI_BIOSSTAT	0x62	/* enable/disable bios */
#define	AMI_RDCFGDSK	0x63	/* read configuration from disk */
#define	AMI_AREBUILD	0x64	/* get/set autorebuild/battery charge */
#define		AMI_GUCAP	1	/* get ultra capabilities */
#define		AMI_SUCAP	2	/* set ultra capability */
#define		AMI_GARBLD	3
#define		AMI_SARBLD	4
#define		AMI_GFCC	5	/* get fast charge counter */
#define		AMI_SFCC	6	/* set fast charge counter */
#define		AMI_GCUCAP	7	/* get channel ultra capabilities */
#define		AMI_SCUCAP	8	/* set channel ultra capabilities */
#define	AMI_SFD		0x66	/* set factory defaults */
#define	AMI_RDCONFIG8	0x67	/* read configuration up to 8 spans */
#define	AMI_WRCONFIG8	0x68	/* write config up to 8 spans */
#define	AMI_ESENSEDUMPA	0x69	/* extended scsi dump area */
#define	AMI_RERRC	0x6a	/* reset error counter */
#define	AMI_BOOTUP	0x6b	/* ena/dis physical drive boot up */
#define	AMI_ENCLOSURE	0x6c	/* get/set enclosure type */
#define	AMI_WRCFGD	0x6c	/* write config disk -- 2.1 */
#define	AMI_HAPIRRLD	0x6e
#define	AMI_LDRVRIGHTS	0x6f
#define	AMI_CLUSTERING	0x70
#define	AMI_GCHPROP	0x71	/* get channel properties */
#define	AMI_SCHTERM	0x72	/* set channel termination */
#define		AMI_TERM_DISABLE 0
#define		AMI_TERM_ENABLE	1
#define		AMI_TERM_HIGH	2
#define		AMI_TERM_WIDE	3
#define		AMI_TERM_DFLT	16
#define	AMI_QUIETCH	0x73	/* quiet channel */
#define	AMI_ACTIVATECH	0x74	/* activate channel */
#define	AMI_STARTU	0x75	/* start unit, pad[0] -- sync/async */
#define		AMI_STARTU_SYNC	1
#define		AMI_STARTU_ASYN	2
#define	AMI_STOPU	0x76	/* stop unit */
#define	AMI_GERRC	0x77	/* get error counter */
#define	AMI_GBTDS	0x78	/* get boot time drive status */
#define	AMI_FMTPROG	0x79
#define	AMI_RCAPCMD	0x7a	/* read capacity */
#define	AMI_WRCRX	0x7b
#define	AMI_RDCRX	0x7c
#define	AMI_GINID	0x7d	/* get initiator id */
#define	AMI_HAPICMD	0x7e
#define	AMI_SINID	0x7f	/* set initiator id */
#define	AMI_SMARTMSEL	0x80
#define	AMI_SPSTARTU	0x85	/* special start unit command */
#define	AMI_NVFAILHIST	0x90
#define	AMI_DCMDABRT	0x91
#define	AMI_GDRIVEHIST	0x92	/* get drive history */
#define	AMI_GESENSE	0x93	/* get extended sense data dump */
#define	AMI_ADAPTER	0x95	/* save/restore adapter params */
#define		AMI_ADP_SAVE	0
#define		AMI_ADP_LOAD	1
#define	AMI_RESET	0x96	/* adapter reset */
#define	AMI_PRGCLASS	0x97	/* program class code */
#define	AMI_UPHTML	0x98	/* upload html utility */
#define	AMI_NEWCFG	0x99
#define	AMI_NEWOP	0xa0
#define	AMI_FCOP	0xa1
#define		AMI_FC_PROCEED	0x02
#define		AMI_FC_DELLDRV	0x03
#define		AMI_FC_RDCONF	0x04
#define		AMI_FC_RDFCONF	0x05
#define		AMI_FC_GCONFDSK	0x06
#define		AMI_FC_CHLDNO	0x07
#define		AMI_FC_CMPCTCFG	0x08
#define		AMI_FC_DRVGRP	0x09
#define		AMI_FC_GLOOPINF	0x0a
#define		AMI_FC_CHLOOPID	0x0b
#define		AMI_FC_GNSCH	0x0c
#define		AMI_FC_WRCONF	0x0d
#define		AMI_FC_PRODINF	0x0e
#define		AMI_FC_EINQ3	0x0f
#define			AMI_FC_EINQ3_SOLICITED_NOTIFY	0x01
#define			AMI_FC_EINQ3_SOLICITED_FULL	0x02
#define			AMI_FC_EINQ3_UNSOLICITED	0x03
#define	AMI_MISC	0xa4
#define	AMI_CHFUNC	0xa9
#define	AMI_MANAGE	0xb0	/* manage functions */
#define		AMI_MGR_LUN	0x00
#define		AMI_MGR_THERM	0x01
#define		AMI_MGR_EEPROM	0x02
#define		AMI_MGR_LDNAMES	0x03
#define		AMI_MGR_FCWWN	0x04
#define		AMI_MGR_CFGACC	0x05
#define	AMI_HSPDIAG	0xb1
#define	AMI_GESENSEINFO	0xb2	/* get extended sense info */
#define	AMI_SYSFLUSH	0xfe	/* flush system */

/* command structures */
#pragma pack(1)
struct ami_iocmd {
	u_int8_t	acc_cmd;
	u_int8_t	acc_id;
	union {
#define	acc_mbox	_._ami_mbox
		struct {
			u_int16_t	amb_nsect;
			u_int32_t	amb_lba;
			u_int32_t	amb_data;
			u_int8_t	amb_ldn;	/* logical drive no */
			u_int8_t	amb_nsge;
			u_int8_t	amb_reserved;
		} _ami_mbox;

#define	acc_io		_._ami_io
		struct {
			u_int8_t	aio_channel;
			u_int8_t	aio_param;
			u_int8_t	aio_pad[4];
			u_int32_t	aio_data;
			u_int8_t	aio_pad1[3];
		} _ami_io;

#define	acc_passthru	_._ami_passru
		struct {
			u_int16_t	apt_dummy0;
			u_int32_t	apt_dummy1;
			u_int32_t	apt_data;
			u_int8_t	apt_dummy2;
			u_int8_t	apt_dummy3;
			u_int8_t	apt_reserved;
		} _ami_passru;

#define	acc_ldrv	_._ami_ldrv
		struct {
			u_int16_t	ald_dummy0;
			u_int32_t	ald_dummy1;
			u_int32_t	ald_data;
			u_int8_t	ald_ldrv;
			u_int8_t	ald_dummy2;
			u_int8_t	ald_reserved;
		} _ami_ldrv;
	} _;
	u_int8_t	acc_busy;
	u_int8_t	acc_nstat;
	u_int8_t	acc_status;
#define	AMI_MAXSTATACK	0x2e
	u_int8_t	acc_cmplidl[AMI_MAXSTATACK];
	u_int8_t	acc_poll;
	u_int8_t	acc_ack;
	u_int8_t	acc_pad[0x3e];	/* pad to 128 bytes */
};

struct ami_sgent {
	u_int32_t	asg_addr;
	u_int32_t	asg_len;
};

struct ami_iocmd64 {
	u_int8_t	acc_cmd;
	u_int8_t	acc_id;
	union {
		struct {
			u_int16_t	amb_nsect;
			u_int32_t	amb_lba;
			u_int32_t	amb_reserved1;
			u_int8_t	amb_ldn;	/* logical drive no */
			u_int8_t	amb_nsge;	/* high bit == 1 */
			u_int8_t	amb_reserved;
		} _ami_mbox;

		struct {
			u_int8_t	aio_channel;
			u_int8_t	aio_param;
			u_int8_t	aio_pad[4];
			u_int32_t	aio_data;
			u_int8_t	aio_pad1[3];
		} _ami_io;

		struct {
			u_int16_t	apt_dummy0;
			u_int32_t	apt_dummy1;
			u_int32_t	apt_data;
			u_int8_t	apt_dummy2;
			u_int8_t	apt_dummy3;
			u_int8_t	apt_reserved;
		} _ami_passru;

		struct {
			u_int16_t	ald_dummy0;
			u_int32_t	ald_dummy1;
			u_int32_t	ald_data;
			u_int8_t	ald_ldrv;
			u_int8_t	ald_dummy2;
			u_int8_t	ald_reserved;
		} _ami_ldrv;
	} _;
	u_int8_t	acc_busy;
	u_int32_t	acc_data_l;
	u_int32_t	acc_data_h;
	u_int32_t	acc_reserved;
	u_int8_t	acc_nstat;
	u_int8_t	acc_status;
	u_int8_t	acc_cmplidl[AMI_MAXSTATACK];
	u_int8_t	acc_poll;
	u_int8_t	acc_ack;
	u_int8_t	acc_pad[0x32];	/* pad to 128 bytes */
};

struct ami_sgent64 {
	u_int32_t	asg_addr_l;
	u_int32_t	asg_addr_h;
	u_int32_t	asg_len;
};

struct ami_passthrough {
	u_int8_t	apt_param;
#define	AMI_PTPARAM(t,a,l)	(((l) << 7) | (((a) & 1) << 3) | ((t) & 3))
#define	AMI_TIMEOUT_6	0
#define	AMI_TIMEOUT_60	1
#define	AMI_TIMEOUT_10m	2
#define	AMI_TIMEOUT_3h	3
	u_int8_t	apt_ldn;
	u_int8_t	apt_channel;
	u_int8_t	apt_target;
	u_int8_t	apt_qtag;
	u_int8_t	apt_qact;
#define	AMI_MAX_CDB	10
	u_int8_t	apt_cdb[AMI_MAX_CDB];
	u_int8_t	apt_ncdb;
	u_int8_t	apt_nsense;
#define	AMI_MAX_SENSE	32
	u_int8_t	apt_sense[AMI_MAX_SENSE];
	u_int8_t	apt_nsg;
	u_int8_t	apt_scsistat;
	u_int32_t	apt_data;
	u_int32_t	apt_datalen;
};

struct ami_inquiry {
	u_int8_t	ain_maxcmd;
	u_int8_t	ain_rbldrate;	/* rebuild rate %% */
	u_int8_t	ain_targets;	/* max targets per channel */
	u_int8_t	ain_channels;
	u_int8_t	ain_fwver[4];
	u_int16_t	ain_flashage;
	u_int8_t	ain_chipset;	/* parity generation policy */
	u_int8_t	ain_ramsize;
	u_int8_t	ain_flushintv;
	u_int8_t	ain_biosver[4];
	u_int8_t	ain_brdtype;
	u_int8_t	ain_scsisensealert;
	u_int8_t	ain_wrcfgcnt;	/* write config count */
	u_int8_t	ain_drvinscnt;	/* drive insertion count */
	u_int8_t	ain_insdrv;	/* inserted drive */
	u_int8_t	ain_battery;	/* battery status */
	u_int8_t	ain_reserved;

	u_int8_t	ain_nlogdrv;
	u_int8_t	ain_reserved1[3];
	u_int32_t	ain_ldsize[AMI_MAX_LDRIVES];
	u_int8_t	ain_ldprop[AMI_MAX_LDRIVES];
	u_int8_t	ain_ldstat[AMI_MAX_LDRIVES];

	u_int8_t	ain_pdstat[AMI_MAX_PDRIVES];
	u_int8_t	ain_predictivefailure;

	u_int8_t	ain_pdfmtinp[AMI_MAX_PDRIVES];
	u_int8_t	ain_reserved2[AMI_MAX_PDRIVES];

	u_int32_t	ain_esize;	/* extended data size */
	u_int16_t	ain_ssid;	/* subsystem id */
	u_int16_t	ain_ssvid;	/* subsystem vendor id */
	u_int32_t	ain_signature;
#define	AMI_SIGN431	0xfffe0001
#define	AMI_SIGN438	0xfffd0002
#define	AMI_SIGN762	0xfffc0003
#define	AMI_SIGNT5	0xfffb0004
#define	AMI_SIGN466	0xfffa0005
};

struct ami_fc_einquiry {
	u_int32_t	ain_size;	/* size of this structure */

	/* notify */
	u_int8_t	ain_notify[0x80];

	u_int8_t	ain_rbldrate;	/* rebuild rate %% */
	u_int8_t	ain_flushintvl;
	u_int8_t	ain_sensealert;
	u_int8_t	ain_drvinscnt;	/* drive insertion count */
	u_int8_t	ain_battery;	/* battery status */

	u_int8_t	ain_nlogdrv;
	u_int8_t	ain_recon[AMI_BIG_MAX_LDRIVES / 8];
	u_int16_t	ain_stat[AMI_BIG_MAX_LDRIVES / 8];

	u_int32_t	ain_ldsize[AMI_BIG_MAX_LDRIVES];
	u_int8_t	ain_ldprop[AMI_BIG_MAX_LDRIVES];
	u_int8_t	ain_ldstat[AMI_BIG_MAX_LDRIVES];

	u_int16_t	ain_pdfmtinp[AMI_BIG_MAX_PDRIVES];
	u_int8_t	ain_pdrates [80];	/* pdrv xfer rates */
};

struct ami_fc_prodinfo {
	u_int32_t	api_size;	/* size of this structure */
	u_int32_t	api_config;
	u_int8_t	api_fwver[16];
	u_int8_t	api_biosver[16];
	u_int8_t	api_product[80];
	u_int8_t	api_maxcmd;
	u_int8_t	api_channels;
	u_int8_t	api_fcloops;
	u_int8_t	api_memtype;
	u_int32_t	api_signature;
	u_int16_t	api_ramsize;
	u_int16_t	api_ssid;
	u_int16_t	api_ssvid;
	u_int8_t	api_nnotify;
};

struct ami_diskarray {
	u_int8_t	ada_nld;
	u_int8_t	ada_pad[3];
	struct {
		u_int8_t	adl_spandepth;
		u_int8_t	adl_raidlvl;
		u_int8_t	adl_rdahead;
		u_int8_t	adl_stripesz;
		u_int8_t	adl_status;
		u_int8_t	adl_wrpolicy;
		u_int8_t	adl_directio;
		u_int8_t	adl_nstripes;
		struct {
			u_int32_t	ads_start;
			u_int32_t	ads_length;	/* blocks */
			struct {
				u_int8_t	add_channel;
				u_int8_t	add_target;
			} ads_devs[AMI_MAX_DEVDEPTH];
		} adl_spans[AMI_MAX_SPANDEPTH];
	} ada_ldrv[AMI_MAX_LDRIVES];
	struct {
		u_int8_t	adp_type;	/* SCSI device type */
		u_int8_t	adp_ostatus;	/* status during config */
		u_int8_t	adp_tagdepth;	/* level of tagging */
		u_int8_t	adp_sneg;	/* sync negotiation */
		u_int32_t	adp_size;
	} ada_pdrv[AMI_MAX_PDRIVES];
};

struct ami_scsisense {
	u_int8_t	ase_end;
	struct {
		u_int8_t	asd_channel;
		u_int8_t	asd_target;
		u_int16_t	asd_errcode;
		u_int16_t	asd_sense;
		u_int16_t	asd_addarea1;
		u_int16_t	asd_addarea2;
		u_int16_t	asd_cmdspec0;
		u_int16_t	asd_cmdspec1;
		u_int16_t	asd_asc_ascq;
	} ase_dump[5];
};

struct ami_escsisense {
	u_int8_t	ase_end;
	struct {
		u_int8_t	asd_channel;
		u_int8_t	asd_target;
		u_int16_t	asd_errcode;
		u_int16_t	asd_sense;
		u_int16_t	asd_addarea1;
		u_int16_t	asd_addarea2;
		u_int16_t	asd_cmdspec0;
		u_int16_t	asd_cmdspec1;
		u_int16_t	asd_asc_ascq;
		u_int16_t	asd_extarea;
	} ase_dump[5];
};

struct ami_cachestats {
	u_int32_t	acs_total;
	u_int32_t	acs_hits;
};

struct ami_drivehistory {
	struct {
		u_int8_t	adh_error;
#define	AMI_ADHERR_TIMEOUT(e)	((e) & 15)
#define	AMI_ADHERR_PARITY(e)	(((e) >> 4) & 15)
		u_int8_t	adh_throttle;
	} adh_err[3][16];	/* channels * drives */
	u_int8_t	adh_failidx;
	struct {
		u_int8_t	adh_tag;
#define	AMI_ADHTAG_CH(t)	((t) & 7)
#define	AMI_ADHTAG_TARG(t)	(((t) >> 3) & 15)
#define	AMI_ADHTAG_VALID(t)	((t) & 0x80)
		u_int8_t	reason;
#define	AMI_ADHERR_MEDIA	1
#define	AMI_ADHERR_NMEDIA	2
#define	AMI_ADHERR_CMDTMO	3
#define	AMI_ADHERR_SELTMO	4
#define	AMI_ADHERR_HAFAIL	5
#define	AMI_ADHERR_REASSIGN	6
#define	AMI_ADHERR_CMDFAIL	7
#define	AMI_ADHERR_OTHER	8

#define	AMI_FAILHISTORY		10
	} adh_fail[AMI_FAILHISTORY];
};

#pragma pack()
