/*	$OpenBSD: dptreg.h,v 1.3 2001/07/26 03:55:54 mickey Exp $	*/
/*	$NetBSD: dptreg.h,v 1.4 1999/10/19 20:16:48 ad Exp $	*/

/*
 * Copyright (c) 1999 Andy Doran <ad@NetBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _IC_DPTREG_H_
#define _IC_DPTREG_H_ 1

/* Hardware limits */
#define DPT_MAX_TARGETS		16
#define DPT_MAX_LUNS		8
#define DPT_MAX_CHANNELS	3

/* Software parameters */
#define	DPT_MAX_XFER		((DPT_SG_SIZE - 1) << PGSHIFT)
#define DPT_MAX_CCBS		256
#define DPT_SG_SIZE        	64
#define DPT_ABORT_TIMEOUT	2000	/* milliseconds */
#define DPT_MORE_TIMEOUT	1000	/* microseconds */

#ifdef _KERNEL

#if BYTE_ORDER == LITTLE_ENDIAN
#define SWAP32(x)	bswap32((x))
#define SWAP16(x)	bswap16((x))
#define RSWAP32(x)	(x)
#define RSWAP16(x)	(x)
#else
#define SWAP32(x)	(x)
#define SWAP16(x)	(x)
#define RSWAP32(x)	bswap32((x))
#define RSWAP16(x)	bswap16((x))
#endif

#ifdef __OpenBSD__
#define	bswap16	swap16
#define	bswap32	swap32
#endif

#define dpt_inb(x, o)	\
    bus_space_read_1((x)->sc_iot, (x)->sc_ioh, (o))

#define dpt_inw(x, o)	\
    RSWAP16(bus_space_read_2((x)->sc_iot, (x)->sc_ioh, (o)))

#define dpt_inl(x, o)	\
    RSWAP32(bus_space_read_4((x)->sc_iot, (x)->sc_ioh, (o)))

#define dpt_outb(x, o, d) \
    bus_space_write_1((x)->sc_iot, (x)->sc_ioh, (o), (d))

#define dpt_outw(x, o, d) \
    bus_space_write_2((x)->sc_iot, (x)->sc_ioh, (o), RSWAP16(d))

#define dpt_outl(x, o, d) \
    bus_space_write_4((x)->sc_iot, (x)->sc_ioh, (o), RSWAP32(d))

#endif	/* _KERNEL */
 
/*
 * HBA registers
 */
#define HA_BASE			0x10
#define HA_DATA			(HA_BASE + 0)
#define HA_ERROR		(HA_BASE + 1)
#define HA_DMA_BASE		(HA_BASE + 2)
#define HA_ICMD_CODE2	       	(HA_BASE + 4)
#define HA_ICMD_CODE1	       	(HA_BASE + 5)
#define HA_ICMD			(HA_BASE + 6)

/* EATA commands. There are many more the we don't define or use. */
#define HA_COMMAND		(HA_BASE + 7)
#define   CP_PIO_GETCFG		0xf0	/* Read configuration data, PIO */
#define   CP_PIO_CMD		0xf2	/* Execute command, PIO */
#define   CP_DMA_GETCFG		0xfd	/* Read configuration data, DMA */
#define   CP_DMA_CMD		0xff	/* Execute command, DMA */
#define   CP_PIO_TRUNCATE	0xf4	/* Truncate transfer command, PIO */
#define   CP_RESET		0xf9	/* Reset controller and SCSI bus */
#define   CP_REBOOT		0x06	/* Reboot controller (last resort) */
#define   CP_IMMEDIATE		0xfa	/* EATA immediate command */
#define     CPI_GEN_ABORT	0x00	/* Generic abort */
#define     CPI_SPEC_RESET	0x01	/* Specific reset */
#define     CPI_BUS_RESET	0x02	/* Bus reset */
#define     CPI_SPEC_ABORT	0x03	/* Specific abort */
#define     CPI_QUIET_INTR	0x04	/* ?? */
#define     CPI_ROM_DL_EN	0x05	/* ?? */
#define     CPI_COLD_BOOT	0x06	/* Cold boot HBA */
#define     CPI_FORCE_IO	0x07	/* ?? */
#define     CPI_BUS_OFFLINE	0x08	/* Set SCSI bus offline */
#define     CPI_RESET_MSKD_BUS	0x09	/* Reset masked bus */
#define     CPI_POWEROFF_WARN	0x0a	/* Power about to fail */

#define HA_STATUS		(HA_BASE + 7)
#define   HA_ST_ERROR		0x01
#define   HA_ST_MORE		0x02
#define   HA_ST_CORRECTD	0x04
#define   HA_ST_DRQ		0x08
#define   HA_ST_SEEK_COMPLETE	0x10
#define   HA_ST_WRT_FLT		0x20
#define   HA_ST_READY		0x40
#define   HA_ST_BUSY		0x80
#define   HA_ST_DATA_RDY	(HA_ST_SEEK_COMPLETE|HA_ST_READY|HA_ST_DRQ)

#define HA_AUX_STATUS		(HA_BASE + 8)
#define   HA_AUX_BUSY		0x01
#define   HA_AUX_INTR		0x02

/*
 * Structure of an EATA command packet.
 */
struct eata_cp {
	u_int8_t	cp_scsireset	:1;	/* cause a bus reset */
	u_int8_t	cp_hbainit	:1;	/* cause HBA to reinitialize */
	u_int8_t	cp_autosense	:1;	/* auto request sense on err */
	u_int8_t	cp_scatter      :1;	/* doing SG I/O */
	u_int8_t	cp_quick	:1;	/* return no status packet */
	u_int8_t	cp_interpret	:1;	/* HBA interprets SCSI CDB */
	u_int8_t	cp_dataout	:1;	/* data out phase */
	u_int8_t	cp_datain	:1;	/* data in phase */
	u_int8_t	cp_senselen;		/* request sense length */
	u_int8_t	cp_unused0[3];		/* unused */
	u_int8_t	cp_tophys	:1;	/* send to RAID component */
	u_int8_t	cp_unused1	:7;	/* unused */
	u_int8_t	cp_physunit	:1;	/* phys unit on mirrored pair */
	u_int8_t	cp_noat		:1;	/* no address translation */
	u_int8_t	cp_nocache	:1;	/* no HBA caching */
	u_int8_t	cp_unused2	:5;	/* unused */
	u_int8_t	cp_id		:5;	/* SCSI device id of target */
	u_int8_t	cp_channel	:3;	/* SCSI channel id */
	u_int8_t	cp_lun		:3;	/* SCSI LUN id */
	u_int8_t	cp_unused3	:2;	/* unused */
	u_int8_t	cp_luntar	:1;	/* CP is for target ROUTINE */
	u_int8_t	cp_dispri	:1;	/* give disconnect privilege */
	u_int8_t	cp_identify	:1;	/* always true */
	u_int8_t	cp_msg[3];		/* message bytes 0-3 */

	/* Partial SCSI CDB ref */
	u_int8_t	cp_scsi_cmd;
	u_int8_t	cp_extent	:1;
	u_int8_t	cp_bytchk	:1;
	u_int8_t	cp_reladr	:1;
	u_int8_t	cp_cmplst	:1;
	u_int8_t	cp_fmtdata	:1;
	u_int8_t	cp_cdblun	:3;
	u_int8_t	cp_page;
	u_int8_t	cp_unused4;
	u_int8_t	cp_len;
	u_int8_t	cp_link		:1;
	u_int8_t	cp_flag		:1;
	u_int8_t	cp_unused5	:4;
	u_int8_t	cp_vendor	:2;
	u_int8_t	cp_cdbmore[6];

	u_int32_t	cp_datalen;	/* length in bytes of data/SG list */
	u_int32_t	cp_ccbid;	/* ID of software CCB */
	u_int32_t	cp_dataaddr;	/* address of data/SG list */
	u_int32_t	cp_stataddr;	/* addr for status packet */
	u_int32_t	cp_senseaddr;	/* addr of req. sense (err only) */
};

/*
 * EATA status packet as returned by controller upon command completion. It 
 * contains status, message info and a handle on the initiating CCB. 
 */
struct eata_sp {
	u_int8_t	sp_hba_status;		/* host adapter status */
	u_int8_t	sp_scsi_status;		/* SCSI bus status */
	u_int8_t	sp_reserved[2];		/* reserved */
	u_int32_t	sp_inv_residue;		/* bytes not transferred */
	u_int32_t	sp_ccbid;		/* ID of software CCB */
	u_int8_t	sp_id_message;
	u_int8_t	sp_que_message;
	u_int8_t	sp_tag_message;
	u_int8_t	sp_messages[9];
};

/* 
 * HBA status as returned by status packet. Bit 7 signals end of command. 
 */
#define HA_NO_ERROR             0x00    /* No error on command */
#define HA_ERROR_SEL_TO         0x01    /* Device selection timeout */
#define HA_ERROR_CMD_TO         0x02    /* Device command timeout */
#define HA_ERROR_RESET          0x03    /* SCSI bus was reset */
#define HA_INIT_POWERUP         0x04    /* Initial controller power up */
#define HA_UNX_BUSPHASE         0x05    /* Unexpected bus phase */
#define HA_UNX_BUS_FREE         0x06    /* Unexpected bus free */
#define HA_BUS_PARITY           0x07    /* SCSI bus parity error */
#define HA_SCSI_HUNG            0x08    /* SCSI bus hung */
#define HA_UNX_MSGRJCT          0x09    /* Unexpected message reject */
#define HA_RESET_STUCK          0x0A    /* SCSI bus reset stuck */
#define HA_RSENSE_FAIL          0x0B    /* Auto-request sense failed */
#define HA_PARITY               0x0C    /* HBA memory parity error */
#define HA_ABORT_NA             0x0D    /* CP aborted - not on bus */
#define HA_ABORTED              0x0E    /* CP aborted - was on bus */
#define HA_RESET_NA             0x0F    /* CP reset - not on bus */
#define HA_RESET                0x10    /* CP reset - was on bus */
#define HA_ECC                  0x11    /* HBA memory ECC error */
#define HA_PCI_PARITY           0x12    /* PCI parity error */
#define HA_PCI_MASTER           0x13    /* PCI master abort */
#define HA_PCI_TARGET           0x14    /* PCI target abort */
#define HA_PCI_SIGNAL_TARGET    0x15    /* PCI signalled target abort */
#define HA_ABORT                0x20    /* Software abort (too many retries) */

/*
 * Scatter-gather list element.
 */
struct eata_sg {
	u_int32_t	sg_addr;
	u_int32_t	sg_len;
};

/*
 * EATA configuration data as returned by HBA. XXX this is bogus, some fields
 * don't *seem* to be filled on my SmartCache III. Also, it doesn't sync up 
 * with the structure FreeBSD uses. [ad]
 */
struct eata_cfg {
        u_int8_t  ec_devtype;
        u_int8_t  ec_pagecode;
        u_int8_t  ec_reserved0;
        u_int8_t  ec_cfglen;		/* Length in bytes after this field */
        u_int8_t  ec_eatasig[4];	/* EATA signature  */
        u_int8_t  ec_eataversion;	/* EATA version number */
	u_int8_t  ec_overlapcmds : 1;	/* Overlapped cmds supported */
	u_int8_t  ec_targetmode : 1;	/* Target mode supported */
	u_int8_t  ec_trunnotrec : 1;	/* Truncate cmd not supported */
	u_int8_t  ec_moresupported:1;	/* More cmd supported */
	u_int8_t  ec_dmasupported : 1;	/* DMA mode supported */
	u_int8_t  ec_dmanumvalid : 1;	/* DMA channel field is valid */
	u_int8_t  ec_atadev : 1;	/* This is an ATA device */
	u_int8_t  ec_hbavalid : 1;	/* HBA field is valid */
        u_int8_t  ec_padlength[2];	/* Pad bytes for PIO cmds */
        u_int8_t  ec_hba[4];		/* Host adapter SCSI IDs */
        u_int8_t  ec_cplen[4];		/* Command packet length */
        u_int8_t  ec_splen[4];		/* Status packet length */
        u_int8_t  ec_queuedepth[2];	/* Controller queue depth */
        u_int8_t  ec_reserved1[2];
        u_int8_t  ec_sglen[2];		/* Maximum scatter gather list size */
        u_int8_t  ec_irqnum : 4;	/* IRQ number */
        u_int8_t  ec_irqtrigger : 1;	/* IRQ trigger: 0 = edge, 1 = level */
        u_int8_t  ec_secondary : 1;	/* Controller not at address 0x170 */
        u_int8_t  ec_dmanum : 2; 	/* DMA channel index for ISA */
        u_int8_t  ec_irq;		/* IRQ address */
	u_int8_t  ec_iodisable : 1;	/* ISA I/O address disabled */
	u_int8_t  ec_forceaddr : 1;	/* PCI forced to an EISA/ISA addr */
	u_int8_t  ec_sg64k : 1;		/* 64K of SG space */
	u_int8_t  ec_sgunaligned : 1;	/* Can do unaligned SG, otherwise 4 */
	u_int8_t  ec_reserved2 : 4;	/* Reserved */
        u_int8_t  ec_maxtarget : 5;	/* Maximun SCSI target ID supported */
        u_int8_t  ec_maxchannel : 3;	/* Maximun channel number supported */
        u_int8_t  ec_maxlun;		/* Maximum LUN supported */
	u_int8_t  ec_reserved3 : 3;	/* Reserved field */
	u_int8_t  ec_autoterm : 1;	/* Support auto term (low byte) */
	u_int8_t  ec_pcim1 : 1;		/* PCI M1 chipset */
	u_int8_t  ec_bogusraidid : 1;	/* Raid ID may be questionable  */
	u_int8_t  ec_pci : 1;		/* PCI adapter */
	u_int8_t  ec_eisa : 1;		/* EISA adapter */
        u_int8_t  ec_raidnum;		/* RAID host adapter humber */
};

/*
 * How SCSI inquiry data breaks down for EATA boards.
 */
struct eata_inquiry_data {
	u_int8_t	ei_device;
	u_int8_t	ei_dev_qual2;
	u_int8_t	ei_version;
	u_int8_t 	ei_response_format;
	u_int8_t 	ei_additional_length;
	u_int8_t 	ei_unused[2];
	u_int8_t	ei_flags;	
	char		ei_vendor[8];	/* Vendor, e.g: DPT, NEC */
	char		ei_model[7];	/* Model number */
	char		ei_suffix[9];	/* Model number suffix */
	char		ei_fw[3];	/* Firmware */
	char		ei_fwrev[1];	/* Firmware revision */
	u_int8_t	ei_extra[8];
};

#endif	/* !defined _IC_DPTREG_H_ */
