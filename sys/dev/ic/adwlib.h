/*      $NetBSD: adwlib.h,v 1.7 2000/02/03 20:29:16 dante Exp $        */

/*
 * Definitions for low level routines and data structures
 * for the Advanced Systems Inc. SCSI controllers chips.
 *
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Ported from:
 */
/*
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *     
 * Copyright (c) 1995-1996 Advanced System Products, Inc.
 * All Rights Reserved.
 *   
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that redistributions of source
 * code retain the above copyright notice and this comment without
 * modification.
 */

#ifndef	_ADVANSYS_WIDE_LIBRARY_H_
#define	_ADVANSYS_WIDE_LIBRARY_H_


/*
 * --- Adv Library Constants and Macros
 */

#define ADW_LIB_VERSION_MAJOR	5
#define ADW_LIB_VERSION_MINOR	2

/*
 * Define Adv Reset Hold Time grater than 25 uSec.
 * See AdvResetSCSIBus() for more info.
 */
#define ASC_SCSI_RESET_HOLD_TIME_US  60

/*
 * Define Adv EEPROM constants.
 */

#define ASC_EEP_DVC_CFG_BEGIN           (0x00)
#define ASC_EEP_DVC_CFG_END             (0x15)
#define ASC_EEP_DVC_CTL_BEGIN           (0x16)  /* location of OEM name */
#define ASC_EEP_MAX_WORD_ADDR           (0x1E)

#define ASC_EEP_DELAY_MS                100

/*
 * EEPROM bits reference by the RISC after initialization.
 */
#define ADW_EEPROM_BIG_ENDIAN          0x8000   /* EEPROM Bit 15 */
#define ADW_EEPROM_BIOS_ENABLE         0x4000   /* EEPROM Bit 14 */
#define ADW_EEPROM_TERM_POL            0x2000   /* EEPROM Bit 13 */

/*
 * EEPROM configuration format
 *
 * Field naming convention: 
 *
 *  *_enable indicates the field enables or disables the feature. The
 *  value is never reset.
 *
 *  *_able indicates both whether a feature should be enabled or disabled
 *  and whether a device isi capable of the feature. At initialization
 *  this field may be set, but later if a device is found to be incapable
 *  of the feature, the field is cleared.
 *
 * Default values are maintained in a_init.c in the structure
 * Default_EEPROM_Config.
 */
#define ADV_EEPROM_BIG_ENDIAN          0x8000   /* EEPROM Bit 15 */
#define ADV_EEPROM_BIOS_ENABLE         0x4000   /* EEPROM Bit 14 */
/*
 * For the ASC3550 Bit 13 is Termination Polarity control bit.
 * For later ICs Bit 13 controls whether the CIS (Card Information
 * Service Section) is loaded from EEPROM.
 */
#define ADV_EEPROM_TERM_POL            0x2000   /* EEPROM Bit 13 */
#define ADV_EEPROM_CIS_LD              0x2000   /* EEPROM Bit 13 */

typedef struct adw_eep_3550_config
{                              
						/* Word Offset, Description */

	u_int16_t	cfg_lsw;		/* 00 power up initialization */
						/*  bit 13 set - Term Polarity Control */
						/*  bit 14 set - BIOS Enable */
						/*  bit 15 set - Big Endian Mode */
	u_int16_t	cfg_msw;		/* 01 unused	   */
	u_int16_t	disc_enable;		/* 02 disconnect enable */
	u_int16_t	wdtr_able;		/* 03 Wide DTR able */
	u_int16_t	sdtr_able;		/* 04 Synchronous DTR able */
	u_int16_t	start_motor;		/* 05 send start up motor */
	u_int16_t	tagqng_able;		/* 06 tag queuing able */
	u_int16_t	bios_scan;		/* 07 BIOS device control */
	u_int16_t	scam_tolerant;		/* 08 no scam */

	u_int8_t	adapter_scsi_id;	/* 09 Host Adapter ID */
	u_int8_t	bios_boot_delay;	/*    power up wait */

	u_int8_t	scsi_reset_delay;	/* 10 reset delay */
	u_int8_t	bios_id_lun;		/*    first boot device scsi id & lun */
						/*    high nibble is lun */
						/*    low nibble is scsi id */

	u_int8_t	termination;  		 /* 11 0 - automatic */
						/*    1 - low off / high off */
						/*    2 - low off / high on */
						/*    3 - low on  / high on */
						/*    There is no low on  / high off */

	u_int8_t	reserved1;		/*    reserved byte (not used) */

	u_int16_t	bios_ctrl;		/* 12 BIOS control bits */
						/*  bit 0  set: BIOS don't act as initiator. */
						/*  bit 1  set: BIOS > 1 GB support */
						/*  bit 2  set: BIOS > 2 Disk Support */
						/*  bit 3  set: BIOS don't support removables */
						/*  bit 4  set: BIOS support bootable CD */
						/*  bit 5  set: */
						/*  bit 6  set: BIOS support multiple LUNs */
						/*  bit 7  set: BIOS display of message */
						/*  bit 8  set: */
						/*  bit 9  set: Reset SCSI bus during init. */
						/*  bit 10 set: */
						/*  bit 11 set: No verbose initialization. */
						/*  bit 12 set: SCSI parity enabled */
						/*  bit 13 set: */
						/*  bit 14 set: */
						/*  bit 15 set: */
	u_int16_t	ultra_able;		/* 13 ULTRA speed able */
	u_int16_t	reserved2;		/* 14 reserved */
	u_int8_t	max_host_qng;		/* 15 maximum host queuing */
	u_int8_t	max_dvc_qng;		/*    maximum per device queuing */
	u_int16_t	dvc_cntl;		/* 16 control bit for driver */
	u_int16_t	bug_fix;		/* 17 control bit for bug fix */
	u_int16_t	serial_number_word1;	/* 18 Board serial number word 1 */
	u_int16_t	serial_number_word2;	/* 19 Board serial number word 2 */
	u_int16_t	serial_number_word3;	/* 20 Board serial number word 3 */
	u_int16_t	check_sum;		/* 21 EEP check sum */
	u_int8_t	oem_name[16];		/* 22 OEM name */
	u_int16_t	dvc_err_code;		/* 30 last device driver error code */
	u_int16_t	adv_err_code;		/* 31 last uc and Adv Lib error code */
	u_int16_t	adv_err_addr;		/* 32 last uc error address */
	u_int16_t	saved_dvc_err_code;	/* 33 saved last dev. driver error code   */
	u_int16_t	saved_adv_err_code;	/* 34 saved last uc and Adv Lib error code */
	u_int16_t	saved_adv_err_addr;	/* 35 saved last uc error address	 */
	u_int16_t	num_of_err;		/* 36 number of error */
} ADW_EEP_3550_CONFIG; 

typedef struct adw_eep_38C0800_config
{
						/* Word Offset, Description */

	u_int16_t	cfg_lsw;		/* 00 power up initialization */
						/*  bit 13 set - Load CIS */
						/*  bit 14 set - BIOS Enable */
						/*  bit 15 set - Big Endian Mode */
	u_int16_t	cfg_msw;		/* 01 unused      */
	u_int16_t	disc_enable;		/* 02 disconnect enable */
	u_int16_t	wdtr_able;		/* 03 Wide DTR able */
	u_int16_t	sdtr_speed1;		/* 04 SDTR Speed TID 0-3 */
	u_int16_t	start_motor;		/* 05 send start up motor */
	u_int16_t	tagqng_able;		/* 06 tag queuing able */
	u_int16_t	bios_scan;		/* 07 BIOS device control */
	u_int16_t	scam_tolerant;		/* 08 no scam */

	u_int8_t	adapter_scsi_id;	/* 09 Host Adapter ID */
	u_int8_t	bios_boot_delay;	/*    power up wait */

	u_int8_t	scsi_reset_delay;	/* 10 reset delay */
	u_int8_t	bios_id_lun;		/*    first boot device scsi id & lun */
						/*    high nibble is lun */
						/*    low nibble is scsi id */

	u_int8_t	termination_se;		/* 11 0 - automatic */
						/*    1 - low off / high off */
						/*    2 - low off / high on */
						/*    3 - low on  / high on */
						/*    There is no low on  / high off */

	u_int8_t	termination_lvd;	/* 11 0 - automatic */
						/*    1 - low off / high off */
						/*    2 - low off / high on */
						/*    3 - low on  / high on */
						/*    There is no low on  / high off */

	u_int16_t	bios_ctrl;		/* 12 BIOS control bits */
						/*  bit 0  set: BIOS don't act as initiator. */
						/*  bit 1  set: BIOS > 1 GB support */
						/*  bit 2  set: BIOS > 2 Disk Support */
						/*  bit 3  set: BIOS don't support removables */
						/*  bit 4  set: BIOS support bootable CD */
						/*  bit 5  set: BIOS scan enabled */
						/*  bit 6  set: BIOS support multiple LUNs */
						/*  bit 7  set: BIOS display of message */
						/*  bit 8  set: */
						/*  bit 9  set: Reset SCSI bus during init. */
						/*  bit 10 set: */
						/*  bit 11 set: No verbose initialization. */
						/*  bit 12 set: SCSI parity enabled */
						/*  bit 13 set: */
						/*  bit 14 set: */
						/*  bit 15 set: */
	u_int16_t	sdtr_speed2;		/* 13 SDTR speed TID 4-7 */
	u_int16_t	sdtr_speed3;		/* 14 SDTR speed TID 8-11 */
	u_int8_t	max_host_qng;		/* 15 maximum host queueing */
	u_int8_t	max_dvc_qng;		/*    maximum per device queuing */
	u_int16_t	dvc_cntl;		/* 16 control bit for driver */
	u_int16_t	sdtr_speed4;		/* 17 SDTR speed 4 TID 12-15 */
	u_int16_t 	serial_number_word1;	/* 18 Board serial number word 1 */
	u_int16_t	serial_number_word2;	/* 19 Board serial number word 2 */
	u_int16_t	serial_number_word3;	/* 20 Board serial number word 3 */
	u_int16_t	check_sum;		/* 21 EEP check sum */
	u_int8_t	oem_name[16];		/* 22 OEM name */
	u_int16_t	dvc_err_code;		/* 30 last device driver error code */
	u_int16_t	adv_err_code;		/* 31 last uc and Adv Lib error code */
	u_int16_t	adv_err_addr;		/* 32 last uc error address */
	u_int16_t	saved_dvc_err_code;	/* 33 saved last dev. driver error code   */
	u_int16_t	saved_adv_err_code;	/* 34 saved last uc and Adv Lib error code */
	u_int16_t	saved_adv_err_addr;	/* 35 saved last uc error address         */
	u_int16_t	reserved36;		/* 36 reserved */
	u_int16_t	reserved37;		/* 37 reserved */
	u_int16_t	reserved38;	   	/* 38 reserved */
	u_int16_t	reserved39;	   	/* 39 reserved */
	u_int16_t	reserved40;	   	/* 40 reserved */
	u_int16_t	reserved41;	   	/* 41 reserved */
	u_int16_t	reserved42;	   	/* 42 reserved */
	u_int16_t	reserved43;	   	/* 43 reserved */
	u_int16_t	reserved44;	   	/* 44 reserved */
	u_int16_t	reserved45;	   	/* 45 reserved */
	u_int16_t	reserved46;	   	/* 46 reserved */
	u_int16_t	reserved47;	   	/* 47 reserved */
	u_int16_t	reserved48;	   	/* 48 reserved */
	u_int16_t	reserved49;	   	/* 49 reserved */
	u_int16_t	reserved50;	   	/* 50 reserved */
	u_int16_t	reserved51;	   	/* 51 reserved */
	u_int16_t	reserved52;	   	/* 52 reserved */
	u_int16_t	reserved53;	   	/* 53 reserved */
	u_int16_t	reserved54;	   	/* 54 reserved */
	u_int16_t	reserved55;	   	/* 55 reserved */
	u_int16_t	cisptr_lsw;	   	/* 56 CIS PTR LSW */
	u_int16_t	cisprt_msw;	   	/* 57 CIS PTR MSW */
	u_int16_t	subsysvid;		/* 58 SubSystem Vendor ID */
	u_int16_t	subsysid;		/* 59 SubSystem ID */
	u_int16_t	reserved60;	   	/* 60 reserved */
	u_int16_t	reserved61;	   	/* 61 reserved */
	u_int16_t	reserved62;	   	/* 62 reserved */
	u_int16_t	reserved63;	   	/* 63 reserved */
} ADW_EEP_38C0800_CONFIG;

/*
 * EEPROM Commands
 */
#define ASC_EEP_CMD_READ          0x80
#define ASC_EEP_CMD_WRITE         0x40
#define ASC_EEP_CMD_WRITE_ABLE    0x30
#define ASC_EEP_CMD_WRITE_DISABLE 0x00

#define ASC_EEP_CMD_DONE             0x0200
#define ASC_EEP_CMD_DONE_ERR         0x0001

/* cfg_word */
#define EEP_CFG_WORD_BIG_ENDIAN      0x8000

/* bios_ctrl */
#define BIOS_CTRL_BIOS               0x0001
#define BIOS_CTRL_EXTENDED_XLAT      0x0002
#define BIOS_CTRL_GT_2_DISK          0x0004
#define BIOS_CTRL_BIOS_REMOVABLE     0x0008
#define BIOS_CTRL_BOOTABLE_CD        0x0010
#define BIOS_CTRL_MULTIPLE_LUN       0x0040
#define BIOS_CTRL_DISPLAY_MSG        0x0080
#define BIOS_CTRL_NO_SCAM            0x0100
#define BIOS_CTRL_RESET_SCSI_BUS     0x0200
#define BIOS_CTRL_INIT_VERBOSE       0x0800
#define BIOS_CTRL_SCSI_PARITY        0x1000

#define ADV_3550_MEMSIZE             0x2000	/* 8 KB Internal Memory */
#define ADV_3550_IOLEN               0x40	/* I/O Port Range in bytes */

#define ADV_38C0800_MEMSIZE          0x4000	/* 16 KB Internal Memory */
#define ADV_38C0800_IOLEN            0x100	/* I/O Port Range in bytes */

#define ADV_38C1600_MEMSIZE          0x4000	/* 16 KB Internal Memory */
#define ADV_38C1600_IOLEN            0x100	/* I/O Port Range 256 bytes */
#define ADV_38C1600_MEMLEN           0x1000	/* Memory Range 4KB bytes */

/*
 * Byte I/O register address from base of 'iop_base'.
 */
#define IOPB_INTR_STATUS_REG    0x00
#define IOPB_CHIP_ID_1          0x01
#define IOPB_INTR_ENABLES       0x02
#define IOPB_CHIP_TYPE_REV      0x03
#define IOPB_RES_ADDR_4         0x04
#define IOPB_RES_ADDR_5         0x05
#define IOPB_RAM_DATA           0x06
#define IOPB_RES_ADDR_7         0x07
#define IOPB_FLAG_REG           0x08
#define IOPB_RES_ADDR_9         0x09
#define IOPB_RISC_CSR           0x0A
#define IOPB_RES_ADDR_B         0x0B
#define IOPB_RES_ADDR_C         0x0C
#define IOPB_RES_ADDR_D         0x0D
#define IOPB_SOFT_OVER_WR       0x0E
#define IOPB_RES_ADDR_F         0x0F
#define IOPB_MEM_CFG            0x10
#define IOPB_RES_ADDR_11        0x11
#define IOPB_GPIO_DATA          0x12
#define IOPB_RES_ADDR_13        0x13
#define IOPB_FLASH_PAGE         0x14
#define IOPB_RES_ADDR_15        0x15
#define IOPB_GPIO_CNTL          0x16
#define IOPB_RES_ADDR_17        0x17
#define IOPB_FLASH_DATA         0x18
#define IOPB_RES_ADDR_19        0x19
#define IOPB_RES_ADDR_1A        0x1A
#define IOPB_RES_ADDR_1B        0x1B
#define IOPB_RES_ADDR_1C        0x1C
#define IOPB_RES_ADDR_1D        0x1D
#define IOPB_RES_ADDR_1E        0x1E
#define IOPB_RES_ADDR_1F        0x1F
#define IOPB_DMA_CFG0           0x20
#define IOPB_DMA_CFG1           0x21
#define IOPB_TICKLE             0x22
#define IOPB_DMA_REG_WR         0x23
#define IOPB_SDMA_STATUS        0x24
#define IOPB_SCSI_BYTE_CNT      0x25
#define IOPB_HOST_BYTE_CNT      0x26
#define IOPB_BYTE_LEFT_TO_XFER  0x27
#define IOPB_BYTE_TO_XFER_0     0x28
#define IOPB_BYTE_TO_XFER_1     0x29
#define IOPB_BYTE_TO_XFER_2     0x2A
#define IOPB_BYTE_TO_XFER_3     0x2B
#define IOPB_ACC_GRP            0x2C
#define IOPB_RES_ADDR_2D        0x2D
#define IOPB_DEV_ID             0x2E
#define IOPB_RES_ADDR_2F        0x2F
#define IOPB_SCSI_DATA          0x30
#define IOPB_RES_ADDR_31        0x31
#define IOPB_RES_ADDR_32        0x32
#define IOPB_SCSI_DATA_HSHK     0x33
#define IOPB_SCSI_CTRL          0x34
#define IOPB_RES_ADDR_35        0x35
#define IOPB_RES_ADDR_36        0x36
#define IOPB_RES_ADDR_37        0x37
#define IOPB_RAM_BIST           0x38
#define IOPB_PLL_TEST           0x39
#define IOPB_PCI_INT_CFG        0x3A
#define IOPB_RES_ADDR_3B        0x3B
#define IOPB_RFIFO_CNT          0x3C
#define IOPB_RES_ADDR_3D        0x3D
#define IOPB_RES_ADDR_3E        0x3E
#define IOPB_RES_ADDR_3F        0x3F

/*
 * Word I/O register address from base of 'iop_base'.
 */
#define IOPW_CHIP_ID_0          0x00  /* CID0  */
#define IOPW_CTRL_REG           0x02  /* CC    */
#define IOPW_RAM_ADDR           0x04  /* LA    */
#define IOPW_RAM_DATA           0x06  /* LD    */
#define IOPW_RES_ADDR_08        0x08
#define IOPW_RISC_CSR           0x0A  /* CSR   */
#define IOPW_SCSI_CFG0          0x0C  /* CFG0  */
#define IOPW_SCSI_CFG1          0x0E  /* CFG1  */
#define IOPW_RES_ADDR_10        0x10
#define IOPW_SEL_MASK           0x12  /* SM    */
#define IOPW_RES_ADDR_14        0x14
#define IOPW_FLASH_ADDR         0x16  /* FA    */
#define IOPW_RES_ADDR_18        0x18
#define IOPW_EE_CMD             0x1A  /* EC    */
#define IOPW_EE_DATA            0x1C  /* ED    */
#define IOPW_SFIFO_CNT          0x1E  /* SFC   */
#define IOPW_RES_ADDR_20        0x20
#define IOPW_Q_BASE             0x22  /* QB    */
#define IOPW_QP                 0x24  /* QP    */
#define IOPW_IX                 0x26  /* IX    */
#define IOPW_SP                 0x28  /* SP    */
#define IOPW_PC                 0x2A  /* PC    */
#define IOPW_RES_ADDR_2C        0x2C
#define IOPW_RES_ADDR_2E        0x2E
#define IOPW_SCSI_DATA          0x30  /* SD    */
#define IOPW_SCSI_DATA_HSHK     0x32  /* SDH   */
#define IOPW_SCSI_CTRL          0x34  /* SC    */
#define IOPW_HSHK_CFG           0x36  /* HCFG  */
#define IOPW_SXFR_STATUS        0x36  /* SXS   */
#define IOPW_SXFR_CNTL          0x38  /* SXL   */
#define IOPW_SXFR_CNTH          0x3A  /* SXH   */
#define IOPW_RES_ADDR_3C        0x3C
#define IOPW_RFIFO_DATA         0x3E  /* RFD   */

/*
 * Doubleword I/O register address from base of 'iop_base'.
 */
#define IOPDW_RES_ADDR_0         0x00
#define IOPDW_RAM_DATA           0x04
#define IOPDW_RES_ADDR_8         0x08
#define IOPDW_RES_ADDR_C         0x0C
#define IOPDW_RES_ADDR_10        0x10
#define IOPDW_COMMA              0x14
#define IOPDW_COMMB              0x18
#define IOPDW_RES_ADDR_1C        0x1C
#define IOPDW_SDMA_ADDR0         0x20
#define IOPDW_SDMA_ADDR1         0x24
#define IOPDW_SDMA_COUNT         0x28
#define IOPDW_SDMA_ERROR         0x2C
#define IOPDW_RDMA_ADDR0         0x30
#define IOPDW_RDMA_ADDR1         0x34
#define IOPDW_RDMA_COUNT         0x38
#define IOPDW_RDMA_ERROR         0x3C

#define ADW_CHIP_ID_BYTE         0x25
#define ADW_CHIP_ID_WORD         0x04C1

#define ADW_SC_SCSI_BUS_RESET    0x2000

#define ADW_INTR_ENABLE_HOST_INTR                   0x01
#define ADW_INTR_ENABLE_SEL_INTR                    0x02
#define ADW_INTR_ENABLE_DPR_INTR                    0x04
#define ADW_INTR_ENABLE_RTA_INTR                    0x08
#define ADW_INTR_ENABLE_RMA_INTR                    0x10
#define ADW_INTR_ENABLE_RST_INTR                    0x20
#define ADW_INTR_ENABLE_DPE_INTR                    0x40
#define ADW_INTR_ENABLE_GLOBAL_INTR                 0x80

#define ADW_INTR_STATUS_INTRA            0x01
#define ADW_INTR_STATUS_INTRB            0x02
#define ADW_INTR_STATUS_INTRC            0x04

#define ADW_RISC_CSR_STOP           (0x0000)
#define ADW_RISC_TEST_COND          (0x2000)
#define ADW_RISC_CSR_RUN            (0x4000)
#define ADW_RISC_CSR_SINGLE_STEP    (0x8000)

#define ADW_CTRL_REG_HOST_INTR      0x0100
#define ADW_CTRL_REG_SEL_INTR       0x0200
#define ADW_CTRL_REG_DPR_INTR       0x0400
#define ADW_CTRL_REG_RTA_INTR       0x0800
#define ADW_CTRL_REG_RMA_INTR       0x1000
#define ADW_CTRL_REG_RES_BIT14      0x2000
#define ADW_CTRL_REG_DPE_INTR       0x4000
#define ADW_CTRL_REG_POWER_DONE     0x8000
#define ADW_CTRL_REG_ANY_INTR       0xFF00

#define ADW_CTRL_REG_CMD_RESET             0x00C6
#define ADW_CTRL_REG_CMD_WR_IO_REG         0x00C5
#define ADW_CTRL_REG_CMD_RD_IO_REG         0x00C4
#define ADW_CTRL_REG_CMD_WR_PCI_CFG_SPACE  0x00C3
#define ADW_CTRL_REG_CMD_RD_PCI_CFG_SPACE  0x00C2

#define ADV_TICKLE_NOP                      0x00
#define ADV_TICKLE_A                        0x01
#define ADV_TICKLE_B                        0x02
#define ADV_TICKLE_C                        0x03

#define ADW_SCSI_CTRL_RSTOUT        0x2000

#define ADW_IS_INT_PENDING(iot, ioh)  \
    (ADW_READ_WORD_REGISTER((iot), (ioh), IOPW_CTRL_REG) & ADW_CTRL_REG_HOST_INTR)

/*
 * SCSI_CFG0 Register bit definitions
 */
#define ADW_TIMER_MODEAB    0xC000  /* Watchdog, Second, and Select. Timer Ctrl. */
#define ADW_PARITY_EN       0x2000  /* Enable SCSI Parity Error detection */
#define ADW_EVEN_PARITY     0x1000  /* Select Even Parity */
#define ADW_WD_LONG         0x0800  /* Watchdog Interval, 1: 57 min, 0: 13 sec */
#define ADW_QUEUE_128       0x0400  /* Queue Size, 1: 128 byte, 0: 64 byte */
#define ADW_PRIM_MODE       0x0100  /* Primitive SCSI mode */
#define ADW_SCAM_EN         0x0080  /* Enable SCAM selection */
#define ADW_SEL_TMO_LONG    0x0040  /* Sel/Resel Timeout, 1: 400 ms, 0: 1.6 ms */
#define ADW_CFRM_ID         0x0020  /* SCAM id sel. confirm., 1: fast, 0: 6.4 ms */
#define ADW_OUR_ID_EN       0x0010  /* Enable OUR_ID bits */
#define ADW_OUR_ID          0x000F  /* SCSI ID */

/*
 * SCSI_CFG1 Register bit definitions
 */
#define ADW_BIG_ENDIAN      0x8000  /* Enable Big Endian Mode MIO:15, EEP:15 */
#define ADW_TERM_POL        0x2000  /* Terminator Polarity Ctrl. MIO:13, EEP:13 */
#define ADW_SLEW_RATE       0x1000  /* SCSI output buffer slew rate */
#define ADW_FILTER_SEL      0x0C00  /* Filter Period Selection */
#define  ADW_FLTR_DISABLE    0x0000  /* Input Filtering Disabled */
#define  ADW_FLTR_11_TO_20NS 0x0800  /* Input Filtering 11ns to 20ns */          
#define  ADW_FLTR_21_TO_39NS 0x0C00  /* Input Filtering 21ns to 39ns */          
#define ADW_ACTIVE_DBL      0x0200  /* Disable Active Negation */
#define ADW_DIFF_MODE       0x0100  /* SCSI differential Mode (Read-Only) */
#define ADW_DIFF_SENSE      0x0080  /* 1: No SE cables, 0: SE cable (Read-Only) */
#define ADW_TERM_CTL_SEL    0x0040  /* Enable TERM_CTL_H and TERM_CTL_L */
#define ADW_TERM_CTL        0x0030  /* External SCSI Termination Bits */
#define  ADW_TERM_CTL_H      0x0020  /* Enable External SCSI Upper Termination */
#define  ADW_TERM_CTL_L      0x0010  /* Enable External SCSI Lower Termination */
#define ADW_CABLE_DETECT    0x000F  /* External SCSI Cable Connection Status */

/*
 * Addendum for ASC-38C0800 Chip
 */
#define ADW_DIS_TERM_DRV    0x4000  /* 1: Read c_det[3:0], 0: cannot read */
#define ADW_HVD_LVD_SE      0x1C00  /* Device Detect Bits */
#define  ADW_HVD             0x1000  /* HVD Device Detect */
#define  ADW_LVD             0x0800  /* LVD Device Detect */
#define  ADW_SE              0x0400  /* SE Device Detect */
#define ADW_TERM_LVD        0x00C0  /* LVD Termination Bits */
#define  ADW_TERM_LVD_HI     0x0080  /* Enable LVD Upper Termination */
#define  ADW_TERM_LVD_LO     0x0040  /* Enable LVD Lower Termination */
#define ADW_TERM_SE         0x0030  /* SE Termination Bits */
#define  ADW_TERM_SE_HI      0x0020  /* Enable SE Upper Termination */
#define  ADW_TERM_SE_LO      0x0010  /* Enable SE Lower Termination */
#define ADW_C_DET_LVD       0x000C  /* LVD Cable Detect Bits */
#define  ADW_C_DET3          0x0008  /* Cable Detect for LVD External Wide */
#define  ADW_C_DET2          0x0004  /* Cable Detect for LVD Internal Wide */
#define ADW_C_DET_SE        0x0003  /* SE Cable Detect Bits */
#define  ADW_C_DET1          0x0002  /* Cable Detect for SE Internal Wide */
#define  ADW_C_DET0          0x0001  /* Cable Detect for SE Internal Narrow */


#define CABLE_ILLEGAL_A 0x7
    /* x 0 0 0  | on  on | Illegal (all 3 connectors are used) */

#define CABLE_ILLEGAL_B 0xB
    /* 0 x 0 0  | on  on | Illegal (all 3 connectors are used) */

/*
   The following table details the SCSI_CFG1 Termination Polarity,
   Termination Control and Cable Detect bits.

   Cable Detect | Termination
   Bit 3 2 1 0  | 5   4  | Notes
   _____________|________|____________________
       1 1 1 0  | on  on | Internal wide only
       1 1 0 1  | on  on | Internal narrow only
       1 0 1 1  | on  on | External narrow only
       0 x 1 1  | on  on | External wide only
       1 1 0 0  | on  off| Internal wide and internal narrow
       1 0 1 0  | on  off| Internal wide and external narrow
       0 x 1 0  | off off| Internal wide and external wide
       1 0 0 1  | on  off| Internal narrow and external narrow
       0 x 0 1  | on  off| Internal narrow and external wide
       1 1 1 1  | on  on | No devices are attached
       x 0 0 0  | on  on | Illegal (all 3 connectors are used)
       0 x 0 0  | on  on | Illegal (all 3 connectors are used)
  
       x means don't-care (either '0' or '1')
  
       If term_pol (bit 13) is '0' (active-low terminator enable), then:
           'on' is '0' and 'off' is '1'.
  
       If term_pol bit is '1' (meaning active-hi terminator enable), then:
           'on' is '1' and 'off' is '0'.
 */

/*
 * MEM_CFG Register bit definitions
 */
#define ADW_BIOS_EN         0x40    /* BIOS Enable MIO:14,EEP:14 */
#define ADW_FAST_EE_CLK     0x20    /* Diagnostic Bit */
#define ADW_RAM_SZ          0x1C    /* Specify size of RAM to RISC */
#define  ADW_RAM_SZ_2KB      0x00    /* 2 KB */
#define  ADW_RAM_SZ_4KB      0x04    /* 4 KB */
#define  ADW_RAM_SZ_8KB      0x08    /* 8 KB */
#define  ADW_RAM_SZ_16KB     0x0C    /* 16 KB */
#define  ADW_RAM_SZ_32KB     0x10    /* 32 KB */
#define  ADW_RAM_SZ_64KB     0x14    /* 64 KB */

/*
 * DMA_CFG0 Register bit definitions
 *
 * This register is only accessible to the host.
 */
#define BC_THRESH_ENB   0x80    /* PCI DMA Start Conditions */
#define FIFO_THRESH     0x70    /* PCI DMA FIFO Threshold */
#define  FIFO_THRESH_16B  0x00   /* 16 bytes */
#define  FIFO_THRESH_32B  0x20   /* 32 bytes */
#define  FIFO_THRESH_48B  0x30   /* 48 bytes */
#define  FIFO_THRESH_64B  0x40   /* 64 bytes */
#define  FIFO_THRESH_80B  0x50   /* 80 bytes (default) */
#define  FIFO_THRESH_96B  0x60   /* 96 bytes */
#define  FIFO_THRESH_112B 0x70   /* 112 bytes */
#define START_CTL       0x0C    /* DMA start conditions */
#define  START_CTL_TH    0x00    /* Wait threshold level (default) */
#define  START_CTL_ID    0x04    /* Wait SDMA/SBUS idle */
#define  START_CTL_THID  0x08    /* Wait threshold and SDMA/SBUS idle */
#define  START_CTL_EMFU  0x0C    /* Wait SDMA FIFO empty/full */
#define READ_CMD        0x03    /* Memory Read Method */
#define  READ_CMD_MR     0x00    /* Memory Read */
#define  READ_CMD_MRL    0x02    /* Memory Read Long */
#define  READ_CMD_MRM    0x03    /* Memory Read Multiple (default) */

/*
 * ASC-38C0800 RAM BIST Register bit definitions
 */
#define RAM_TEST_MODE         0x80
#define PRE_TEST_MODE         0x40
#define NORMAL_MODE           0x00
#define RAM_TEST_DONE         0x10
#define RAM_TEST_STATUS       0x0F
#define  RAM_TEST_HOST_ERROR   0x08
#define  RAM_TEST_INTRAM_ERROR 0x04
#define  RAM_TEST_RISC_ERROR   0x02
#define  RAM_TEST_SCSI_ERROR   0x01
#define  RAM_TEST_SUCCESS      0x00
#define PRE_TEST_VALUE        0x05
#define NORMAL_VALUE          0x00


/*
 * Adv Library Status Definitions
 */
#define ADW_TRUE        1
#define ADW_FALSE       0
#define ADW_NOERROR     1
#define ADW_SUCCESS     1
#define ADW_BUSY        0
#define ADW_ERROR       (-1)


/*
 * ASC_DVC_VAR 'warn_code' values
 */
#define ASC_WARN_BUSRESET_ERROR         0x0001 /* SCSI Bus Reset error */
#define ASC_WARN_EEPROM_CHKSUM          0x0002 /* EEP check sum error */
#define ASC_WARN_EEPROM_TERMINATION     0x0004 /* EEP termination bad field */
#define ASC_WARN_SET_PCI_CONFIG_SPACE   0x0080 /* PCI config space set error */
#define ASC_WARN_ERROR                  0xFFFF /* ADW_ERROR return */

#define ADW_MAX_TID                     15 /* max. target identifier */
#define ADW_MAX_LUN                     7  /* max. logical unit number */


/*
 * AscInitGetConfig() and AscInitAsc1000Driver() Definitions
 *
 * Error code values are set in ASC_DVC_VAR 'err_code'.
 */
#define ASC_IERR_WRITE_EEPROM       0x0001 /* write EEPROM error */
#define ASC_IERR_MCODE_CHKSUM       0x0002 /* micro code check sum error */
#define ASC_IERR_NO_CARRIER         0x0004 /* No more carrier memory. */
#define ASC_IERR_START_STOP_CHIP    0x0008 /* start/stop chip failed */
#define ASC_IERR_CHIP_VERSION       0x0040 /* wrong chip version */
#define ASC_IERR_SET_SCSI_ID        0x0080 /* set SCSI ID failed */
#define ASC_IERR_HVD_DEVICE         0x0100 /* HVD attached to LVD connector. */
#define ASC_IERR_BAD_SIGNATURE      0x0200 /* signature not found */
#define ASC_IERR_ILLEGAL_CONNECTION 0x0400 /* Illegal cable connection */
#define ASC_IERR_SINGLE_END_DEVICE  0x0800 /* Single-end used w/differential */
#define ASC_IERR_REVERSED_CABLE     0x1000 /* Narrow flat cable reversed */
#define ASC_IERR_BIST_PRE_TEST      0x2000 /* BIST pre-test error */
#define ASC_IERR_BIST_RAM_TEST      0x4000 /* BIST RAM test error */
#define ASC_IERR_BAD_CHIPTYPE       0x8000 /* Invalid 'chip_type' setting. */

/*
 * Fixed locations of microcode operating variables.
 */
#define ASC_MC_CODE_BEGIN_ADDR          0x0028 /* microcode start address */
#define ASC_MC_CODE_END_ADDR            0x002A /* microcode end address */
#define ASC_MC_CODE_CHK_SUM             0x002C /* microcode code checksum */
#define ASC_MC_VERSION_DATE             0x0038 /* microcode version */
#define ASC_MC_VERSION_NUM              0x003A /* microcode number */
#define ASC_MC_BIOSMEM                  0x0040 /* BIOS RISC Memory Start */
#define ASC_MC_BIOSLEN                  0x0050 /* BIOS RISC Memory Length */
#define ASC_MC_BIOS_SIGNATURE           0x0058 /* BIOS Signature 0x55AA */
#define ASC_MC_BIOS_VERSION             0x005A /* BIOS Version (2 bytes) */
#define ASC_MC_SDTR_SPEED1              0x0090 /* SDTR Speed for TID 0-3 */
#define ASC_MC_SDTR_SPEED2              0x0092 /* SDTR Speed for TID 4-7 */
#define ASC_MC_SDTR_SPEED3              0x0094 /* SDTR Speed for TID 8-11 */
#define ASC_MC_SDTR_SPEED4              0x0096 /* SDTR Speed for TID 12-15 */
#define ASC_MC_CHIP_TYPE                0x009A
#define ASC_MC_INTRB_CODE               0x009B
#define ASC_MC_WDTR_ABLE                0x009C
#define ASC_MC_SDTR_ABLE                0x009E
#define ASC_MC_TAGQNG_ABLE              0x00A0
#define ASC_MC_DISC_ENABLE              0x00A2
#define ASC_MC_IDLE_CMD_STATUS          0x00A4
#define ASC_MC_IDLE_CMD                 0x00A6
#define ASC_MC_IDLE_CMD_PARAMETER       0x00A8
#define ASC_MC_DEFAULT_SCSI_CFG0        0x00AC
#define ASC_MC_DEFAULT_SCSI_CFG1        0x00AE
#define ASC_MC_DEFAULT_MEM_CFG          0x00B0
#define ASC_MC_DEFAULT_SEL_MASK         0x00B2
#define ASC_MC_SDTR_DONE                0x00B6
#define ASC_MC_NUMBER_OF_QUEUED_CMD     0x00C0
#define ASC_MC_NUMBER_OF_MAX_CMD        0x00D0
#define ASC_MC_DEVICE_HSHK_CFG_TABLE    0x0100
#define ASC_MC_CONTROL_FLAG             0x0122 /* Microcode control flag. */
#define ASC_MC_WDTR_DONE                0x0124
#define ASC_MC_CAM_MODE_MASK            0x015E /* CAM mode TID bitmask. */
#define ASC_MC_ICQ                      0x0160
#define ASC_MC_IRQ                      0x0164

/*
 * BIOS LRAM variable absolute offsets.
 */
#define BIOS_CODESEG    0x54
#define BIOS_CODELEN    0x56
#define BIOS_SIGNATURE  0x58
#define BIOS_VERSION    0x5A

/*
 * Microcode Control Flags
 *
 * Flags set by the Adv Library in RISC variable 'control_flag' (0x122)
 * and handled by the microcode.
 */
#define CONTROL_FLAG_IGNORE_PERR        0x0001 /* Ignore DMA Parity Errors */

/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE microcode table or HSHK_CFG register format
 */
#define HSHK_CFG_WIDE_XFR       0x8000
#define HSHK_CFG_RATE           0x0F00
#define HSHK_CFG_OFFSET         0x001F

#define ASC_DEF_MAX_HOST_QNG    0xFD /* Max. number of host commands (253) */
#define ASC_DEF_MIN_HOST_QNG    0x10 /* Min. number of host commands (16) */
#define ASC_DEF_MAX_DVC_QNG     0x3F /* Max. number commands per device (63) */
#define ASC_DEF_MIN_DVC_QNG     0x04 /* Min. number commands per device (4) */

#define ASC_QC_DATA_CHECK  0x01 /* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_DATA_OUT    0x02 /* Data out DMA transfer. */
#define ASC_QC_START_MOTOR 0x04 /* Send auto-start motor before request. */
#define ASC_QC_NO_OVERRUN  0x08 /* Don't report overrun. */
#define ASC_QC_FREEZE_TIDQ 0x10 /* Freeze TID queue after request. XXX TBD */

#define ASC_QSC_NO_DISC     0x01 /* Don't allow disconnect for request. */
#define ASC_QSC_NO_TAGMSG   0x02 /* Don't allow tag queuing for request. */
#define ASC_QSC_NO_SYNC     0x04 /* Don't use Synch. transfer on request. */
#define ASC_QSC_NO_WIDE     0x08 /* Don't use Wide transfer on request. */
#define ASC_QSC_REDO_DTR    0x10 /* Renegotiate WDTR/SDTR before request. */
/*
 * Note: If a Tag Message is to be sent and neither ASC_QSC_HEAD_TAG or
 * ASC_QSC_ORDERED_TAG is set, then a Simple Tag Message (0x20) is used.
 */
#define ASC_QSC_HEAD_TAG    0x40 /* Use Head Tag Message (0x21). */
#define ASC_QSC_ORDERED_TAG 0x80 /* Use Ordered Tag Message (0x22). */

#define ADV_CHIP_ASC3550          0x01   /* Ultra-Wide IC */
#define ADV_CHIP_ASC38C0800       0x02   /* Ultra2-Wide/LVD IC */
#define ADV_CHIP_ASC38C1600       0x03   /* Ultra3-Wide/LVD2 IC */

/*
 * Adapter temporary configuration structure
 *
 * This structure can be discarded after initialization. Don't add
 * fields here needed after initialization.
 *
 * Field naming convention: 
 *
 *  *_enable indicates the field enables or disables a feature. The
 *  value of the field is never reset.
 */
typedef struct adw_dvc_cfg {
	u_int16_t	disc_enable;	/* enable disconnection */
	u_int8_t	chip_version;	/* chip version */
	u_int8_t	termination;	/* Term. Ctrl. bits 6-5 of SCSI_CFG1 register */
	u_int16_t	pci_device_id;	/* PCI device code number */
	u_int16_t	lib_version;	/* Adv Library version number */
	u_int16_t	control_flag;	/* Microcode Control Flag */
	u_int16_t	mcode_date;	/* Microcode date */
	u_int16_t	mcode_version;	/* Microcode version */
	u_int16_t	pci_slot_info;	/* high byte device/function number */
					/* bits 7-3 device num., bits 2-0 function num. */
					/* low byte bus num. */
	u_int16_t	serial1;	/* EEPROM serial number word 1 */
	u_int16_t	serial2;	/* EEPROM serial number word 2 */
	u_int16_t	serial3;	/* EEPROM serial number word 3 */
} ADW_DVC_CFG; 


#define NO_OF_SG_PER_BLOCK              15

typedef struct adw_sg_block {
	u_int8_t	reserved1;
	u_int8_t	reserved2;
	u_int8_t	reserved3;
	u_int8_t	sg_cnt;			/* Valid entries in block. */
	u_int32_t	sg_ptr;			/* links to next sg block */
	struct {
		u_int32_t sg_addr;		/* SG element address */
		u_int32_t sg_count;		/* SG element count */
	} sg_list[NO_OF_SG_PER_BLOCK];
} ADW_SG_BLOCK;


/*
 * Adapter operation variable structure.
 *
 * One structure is required per host adapter.
 *
 * Field naming convention: 
 *
 *  *_able indicates both whether a feature should be enabled or disabled
 *  and whether a device is capable of the feature. At initialization
 *  this field may be set, but later if a device is found to be incapable
 *  of the feature, the field is cleared.
 */
#define	CCB_HASH_SIZE	32	/* hash table size for phystokv */
#define	CCB_HASH_SHIFT	9
#define CCB_HASH(x)	((((x)) >> CCB_HASH_SHIFT) & (CCB_HASH_SIZE - 1))

#define	CARRIER_HASH_SIZE	32	/* hash table size for phystokv */
#define	CARRIER_HASH_SHIFT	9
#define CARRIER_HASH(x)	((((x)) >> CARRIER_HASH_SHIFT) & (CARRIER_HASH_SIZE - 1))

typedef int (* ADW_CALLBACK) (int);

typedef struct adw_softc {

	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;
	bus_dmamap_t		sc_dmamap_control; /* maps the control structures */
	bus_dmamap_t		sc_dmamap_carrier; /* maps the carrier structures */
	void			*sc_ih;

	struct adw_control	*sc_control; /* control structures */

	struct adw_carrier	*sc_carrhash[CARRIER_HASH_SIZE];
	struct adw_ccb		*sc_ccbhash[CCB_HASH_SIZE];
	TAILQ_HEAD(, adw_ccb)	sc_free_ccb, sc_waiting_ccb;
	struct scsi_link	sc_link;     /* prototype for devs */
	struct scsi_adapter	sc_adapter;

	LIST_HEAD(, scsi_xfer)  sc_queue;
	struct scsi_xfer	*sc_queuelast;

	ADW_CALLBACK	isr_callback;	/* pointer to function, called in AdvISR() */
	ADW_CALLBACK	async_callback;	/* pointer to function, called in AdvISR() */
	u_int16_t	bios_ctrl;	/* BIOS control word, EEPROM word 12 */
	u_int16_t	wdtr_able;	/* try WDTR for a device */
	u_int16_t	sdtr_able;	/* try SDTR for a device */
	u_int16_t	ultra_able;	/* try SDTR Ultra speed for a device */
	u_int16_t	sdtr_speed1;	/* EEPROM SDTR Speed for TID 0-3   */
	u_int16_t	sdtr_speed2;	/* EEPROM SDTR Speed for TID 4-7   */
	u_int16_t	sdtr_speed3;	/* EEPROM SDTR Speed for TID 8-11  */
	u_int16_t	sdtr_speed4;	/* EEPROM SDTR Speed for TID 12-15 */
	u_int16_t	tagqng_able;	/* try tagged queuing with a device */
	u_int16_t	start_motor;	/* start motor command allowed */
	u_int8_t	max_dvc_qng;	/* maximum number of tagged commands per device */
	u_int8_t	scsi_reset_wait; /* delay in seconds after scsi bus reset */
	u_int8_t	chip_no; 	/* should be assigned by caller */
	u_int8_t	max_host_qng;	/* maximum number of Q'ed command allowed */
	u_int8_t	irq_no;  	/* IRQ number */
	u_int8_t	chip_type;	/* chip SCSI target ID */
	u_int16_t	no_scam; 	/* scam_tolerant of EEPROM */
	u_int32_t	drv_ptr; 	/* driver pointer to private structure */
	u_int8_t	chip_scsi_id;	/* chip SCSI target ID */
	u_int8_t	bist_err_code;
	u_int16_t	carr_pending_cnt;  /* Count of pending carriers. */
	struct adw_carrier	*carr_freelist;	/* Carrier free list. */
	struct adw_carrier	*icq_sp; /* Initiator command queue stopper pointer. */
	struct adw_carrier	*irq_sp; /* Initiator response queue stopper pointer. */
 /*
  * Note: The following fields will not be used after initialization. The
  * driver may discard the buffer after initialization is done.
  */
  ADW_DVC_CFG cfg; /* temporary configuration structure  */
} ADW_SOFTC; 


/*
 * ADW_SCSI_REQ_Q - microcode request structure
 *
 * All fields in this structure up to byte 60 are used by the microcode.
 * The microcode makes assumptions about the size and ordering of fields
 * in this structure. Do not change the structure definition here without
 * coordinating the change with the microcode.
 */
typedef struct adw_scsi_req_q {
	u_int8_t	cntl;		/* Ucode flags and state (ASC_MC_QC_*). */
	u_int8_t	target_cmd;
	u_int8_t	target_id;	/* Device target identifier. */
	u_int8_t	target_lun;	/* Device target logical unit number. */
	u_int32_t	data_addr;	/* Data buffer physical address. */
	u_int32_t	data_cnt;	/* Data count. Ucode sets to residual. */
	u_int32_t	sense_addr;	/* Sense buffer physical address. */
	u_int32_t	carr_pa;	/* Carrier p-address */
	u_int8_t	mflag;		/* Adv Library flag field. */
	u_int8_t	sense_len;	/* Auto-sense length. uCode sets to residual. */
	u_int8_t	cdb_len;	/* SCSI CDB length. */
	u_int8_t	scsi_cntl;
	u_int8_t	done_status;	/* Completion status. */
	u_int8_t	scsi_status;	/* SCSI status byte. (see below) */
	u_int8_t	host_status;	/* Ucode host status. */
	u_int8_t	sg_working_ix;	/* Ucode working SG variable. */
	u_int8_t	cdb[12];	/* SCSI command block. */
	u_int32_t	sg_real_addr;	/* SG list physical address. */
	u_int32_t	scsiq_rptr;	/* Iternal pointer to ADW_SCSI_REQ_Q */
	u_int32_t	sg_working_data_cnt;
	u_int32_t	ccb_ptr;	/* CCB Physical Address */
	u_int32_t	carr_va;	/* Carrier v-address (unused) */
	/*
	 * End of microcode structure - 60 bytes. The rest of the structure
	 * is used by the Adv Library and ignored by the microcode.
	 */
	struct scsi_sense_data *vsense_addr;	/* Sense buffer virtual address. */
	u_char		*vdata_addr;	/* Data buffer virtual address. */
	u_int8_t	orig_sense_len;	/* Original length of sense buffer. */
	u_int8_t	pads[3];	/* padding bytes (align to long) */
} ADW_SCSI_REQ_Q;

/*
 * Microcode idle loop commands
 */
#define IDLE_CMD_COMPLETED           0
#define IDLE_CMD_STOP_CHIP           0x0001
#define IDLE_CMD_STOP_CHIP_SEND_INT  0x0002
#define IDLE_CMD_SEND_INT            0x0004
#define IDLE_CMD_ABORT               0x0008
#define IDLE_CMD_DEVICE_RESET        0x0010
#define IDLE_CMD_SCSI_RESET_START    0x0020 /* Assert SCSI Bus Reset */
#define IDLE_CMD_SCSI_RESET_END      0x0040 /* Deassert SCSI Bus Reset */
#define IDLE_CMD_SCSIREQ             0x0080

#define IDLE_CMD_STATUS_SUCCESS      0x0001
#define IDLE_CMD_STATUS_FAILURE      0x0002

/*
 * AdvSendIdleCmd() flag definitions.
 */
#define ADW_NOWAIT     0x01

/*
 * Wait loop time out values.
 */
#define SCSI_WAIT_10_SEC             10UL    /* 10 seconds */
#define SCSI_WAIT_100_MSEC           100UL   /* 100 milliseconds */
#define SCSI_US_PER_MSEC             1000    /* microseconds per millisecond */
#define SCSI_MS_PER_SEC              1000UL  /* milliseconds per second */
#define SCSI_MAX_RETRY               10      /* retry count */

#define ADV_ASYNC_RDMA_FAILURE          0x01 /* Fatal RDMA failure. */
#define ADV_ASYNC_SCSI_BUS_RESET_DET    0x02 /* Detected SCSI Bus Reset. */
#define ADV_ASYNC_CARRIER_READY_FAILURE 0x03 /* Carrier Ready failure. */

#define ADV_HOST_SCSI_BUS_RESET      0x80 /* Host Initiated SCSI Bus Reset. */


/* Read byte from a register. */
#define ADW_READ_BYTE_REGISTER(iot, ioh, reg_off) \
	bus_space_read_1((iot), (ioh), (reg_off))

/* Write byte to a register. */
#define ADW_WRITE_BYTE_REGISTER(iot, ioh, reg_off, byte) \
	bus_space_write_1((iot), (ioh), (reg_off), (byte))

/* Read word (2 bytes) from a register. */
#define ADW_READ_WORD_REGISTER(iot, ioh, reg_off) \
	bus_space_read_2((iot), (ioh), (reg_off))

/* Write word (2 bytes) to a register. */
#define ADW_WRITE_WORD_REGISTER(iot, ioh, reg_off, word) \
	bus_space_write_2((iot), (ioh), (reg_off), (word))

/* Read byte from LRAM. */
#define ADW_READ_BYTE_LRAM(iot, ioh, addr, byte) \
do { \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr)); \
	(byte) = bus_space_read_1((iot), (ioh), IOPB_RAM_DATA); \
} while (0)

/* Write byte to LRAM. */
#define ADW_WRITE_BYTE_LRAM(iot, ioh, addr, byte) \
do { \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr)); \
	bus_space_write_1((iot), (ioh), IOPB_RAM_DATA, (byte)); \
} while (0)

/* Read word (2 bytes) from LRAM. */
#define ADW_READ_WORD_LRAM(iot, ioh, addr, word) \
do { \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr));  \
	(word) = bus_space_read_2((iot), (ioh), IOPW_RAM_DATA); \
} while (0)

/* Write word (2 bytes) to LRAM. */
#define ADW_WRITE_WORD_LRAM(iot, ioh, addr, word) \
do { \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr)); \
	bus_space_write_2((iot), (ioh), IOPW_RAM_DATA, (word)); \
} while (0)

/* Write double word (4 bytes) to LRAM */
/* Because of unspecified C language ordering don't use auto-increment. */
#define ADW_WRITE_DWORD_LRAM(iot, ioh, addr, dword) \
do { \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr)); \
	bus_space_write_2((iot), (ioh), IOPW_RAM_DATA, \
		(ushort) ((dword) & 0xFFFF)); \
	bus_space_write_2((iot), (ioh), IOPW_RAM_ADDR, (addr) + 2); \
	bus_space_write_2((iot), (ioh), IOPW_RAM_DATA, \
			(ushort) ((dword >> 16) & 0xFFFF)); \
} while (0)

/* Read word (2 bytes) from LRAM assuming that the address is already set. */
#define ADW_READ_WORD_AUTO_INC_LRAM(iot, ioh) \
	bus_space_read_2((iot), (ioh), IOPW_RAM_DATA) \

/* Write word (2 bytes) to LRAM assuming that the address is already set. */
#define ADW_WRITE_WORD_AUTO_INC_LRAM(iot, ioh, word) \
	bus_space_write_2((iot), (ioh), IOPW_RAM_DATA, (word))

/*
 * Define macro to check for Condor signature.
 *
 * Evaluate to ADW_TRUE if a Condor chip is found the specified port
 * address 'iop_base'. Otherwise evalue to ADW_FALSE.
 */
#define ADW_FIND_SIGNATURE(iot, ioh) \
	(((ADW_READ_BYTE_REGISTER((iot), (ioh), IOPB_CHIP_ID_1) == \
		ADW_CHIP_ID_BYTE) && \
		(ADW_READ_WORD_REGISTER((iot), (ioh), IOPW_CHIP_ID_0) == \
		ADW_CHIP_ID_WORD)) ?  ADW_TRUE : ADW_FALSE)

/*
 * Define macro to Return the version number of the chip at 'iop_base'.
 *
 * The second parameter 'bus_type' is currently unused.
 */
#define ADW_GET_CHIP_VERSION(iot, ioh, bus_type) \
	ADW_READ_BYTE_REGISTER((iot), (ioh), IOPB_CHIP_TYPE_REV)

/*
 * Abort an SRB in the chip's RISC Memory. The 'srb_ptr' argument must
 * match the ASC_SCSI_REQ_Q 'srb_ptr' field.
 * 
 * If the request has not yet been sent to the device it will simply be
 * aborted from RISC memory. If the request is disconnected it will be
 * aborted on reselection by sending an Abort Message to the target ID.
 *
 * Return value:
 *      ADW_TRUE(1) - Queue was successfully aborted.
 *      ADW_FALSE(0) - Queue was not found on the active queue list.
 */
#define ADW_ABORT_CCB(sc, ccb_ptr) \
	AdvSendIdleCmd((sc), (u_int16_t) IDLE_CMD_ABORT, (ccb_ptr)->hashkey)

/*
 * Send a Bus Device Reset Message to the specified target ID.
 *
 * All outstanding commands will be purged if sending the
 * Bus Device Reset Message is successful.
 *
 * Return Value:
 *      ADW_TRUE(1) - All requests on the target are purged.
 *      ADW_FALSE(0) - Couldn't issue Bus Device Reset Message; Requests
 *                     are not purged.
 */
#define ADW_RESET_DEVICE(sc, target_id) \
	AdvSendIdleCmd((sc), (u_int16_t) IDLE_CMD_DEVICE_RESET, (target_id), 0)

/*
 * SCSI Wide Type definition.
 */
#define ADW_SCSI_BIT_ID_TYPE   u_int16_t

/*
 * AdvInitScsiTarget() 'cntl_flag' options.
 */
#define ADW_SCAN_LUN           0x01
#define ADW_CAPINFO_NOLUN      0x02

/*
 * Convert target id to target id bit mask.
 */
#define ADW_TID_TO_TIDMASK(tid)   (0x01 << ((tid) & ADW_MAX_TID))

/*
 * ASC_SCSI_REQ_Q 'done_status' and 'host_status' return values.
 */

#define QD_NO_STATUS         0x00       /* Request not completed yet. */
#define QD_NO_ERROR          0x01
#define QD_ABORTED_BY_HOST   0x02
#define QD_WITH_ERROR        0x04

#define QHSTA_NO_ERROR              0x00
#define QHSTA_M_SEL_TIMEOUT         0x11
#define QHSTA_M_DATA_OVER_RUN       0x12
#define QHSTA_M_UNEXPECTED_BUS_FREE 0x13
#define QHSTA_M_QUEUE_ABORTED       0x15
#define QHSTA_M_SXFR_SDMA_ERR       0x16 /* SXFR_STATUS SCSI DMA Error */
#define QHSTA_M_SXFR_SXFR_PERR      0x17 /* SXFR_STATUS SCSI Bus Parity Error */
#define QHSTA_M_RDMA_PERR           0x18 /* RISC PCI DMA parity error */
#define QHSTA_M_SXFR_OFF_UFLW       0x19 /* SXFR_STATUS Offset Underflow */
#define QHSTA_M_SXFR_OFF_OFLW       0x20 /* SXFR_STATUS Offset Overflow */
#define QHSTA_M_SXFR_WD_TMO         0x21 /* SXFR_STATUS Watchdog Timeout */
#define QHSTA_M_SXFR_DESELECTED     0x22 /* SXFR_STATUS Deselected */
/* Note: QHSTA_M_SXFR_XFR_OFLW is identical to QHSTA_M_DATA_OVER_RUN. */
#define QHSTA_M_SXFR_XFR_OFLW       0x12 /* SXFR_STATUS Transfer Overflow */
#define QHSTA_M_SXFR_XFR_PH_ERR     0x24 /* SXFR_STATUS Transfer Phase Error */
#define QHSTA_M_SXFR_UNKNOWN_ERROR  0x25 /* SXFR_STATUS Unknown Error */
#define QHSTA_M_SCSI_BUS_RESET      0x30 /* Request aborted from SBR */
#define QHSTA_M_SCSI_BUS_RESET_UNSOL 0x31 /* Request aborted from unsol. SBR */
#define QHSTA_M_BUS_DEVICE_RESET    0x32 /* Request aborted from BDR */
#define QHSTA_M_DIRECTION_ERR       0x35 /* Data Phase mismatch */
#define QHSTA_M_DIRECTION_ERR_HUNG  0x36 /* Data Phase mismatch and bus hang */
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   0x43
#define QHSTA_M_AUTO_REQ_SENSE_FAIL 0x44
#define QHSTA_M_INVALID_DEVICE      0x45 /* Bad target ID */
#define QHSTA_M_FROZEN_TIDQ         0x46 /* TID Queue frozen. */
#define QHSTA_M_SGBACKUP_ERROR      0x47 /* Scatter-Gather backup error */

/*
 * SCSI Iquiry structure
 */

typedef struct {
	u_int8_t	peri_dvc_type	: 5;	/* peripheral device type */
	u_int8_t	peri_qualifier  : 3;	/* peripheral qualifier */
	u_int8_t	dvc_type_modifier : 7;	/* device type modifier (for SCSI I) */
	u_int8_t	rmb	 : 1;		/* RMB - removable medium bit */
	u_int8_t	ansi_apr_ver : 3;	/* ANSI approved version */
	u_int8_t	ecma_ver : 3;		/* ECMA version */
	u_int8_t	iso_ver  : 2;		/* ISO version */
	u_int8_t	rsp_data_fmt : 4;	/* response data format */
						/* 0 SCSI 1 */
						/* 1 CCS */
						/* 2 SCSI-2 */
						/* 3-F reserved */
	u_int8_t	res1	 : 2;	     	/* reserved */
	u_int8_t	TemIOP   : 1;	     	/* terminate I/O process bit (see 5.6.22) */
	u_int8_t	aenc	 : 1;	     	/* asynch. event notification (processor) */
	u_int8_t	add_len;		/* additional length */
	u_int8_t	res2;			/* reserved */
	u_int8_t	res3;			/* reserved */
	u_int8_t	StfRe	: 1;	    	/* soft reset implemented */
	u_int8_t	CmdQue  : 1;	    	/* command queuing */
	u_int8_t	res4	: 1;	    	/* reserved */
	u_int8_t	Linked  : 1;	    	/* linked command for this logical unit */
	u_int8_t	Sync	: 1;	    	/* synchronous data transfer */
	u_int8_t	WBus16  : 1;	    	/* wide bus 16 bit data transfer */
	u_int8_t	WBus32  : 1;	    	/* wide bus 32 bit data transfer */
	u_int8_t	RelAdr  : 1;	    	/* relative addressing mode */
	u_int8_t	vendor_id[8];		/* vendor identification */
	u_int8_t	product_id[16];		/* product identification */
	u_int8_t	product_rev_level[4];	/* product revision level */
	u_int8_t	vendor_specific[20];	/* vendor specific */
	u_int8_t	IUS	 : 1;		/* information unit supported */
	u_int8_t	QAS	 : 1;		/* quick arbitrate supported */
	u_int8_t	Clocking : 2;		/* clocking field */
	u_int8_t	res5	 : 4;		/* reserved */
	u_int8_t	res6;			/* reserved */
} ADW_SCSI_INQUIRY; /* 58 bytes */

#define SS_GOOD              0x00
#define SS_CHK_CONDITION     0x02
#define SS_CONDITION_MET     0x04
#define SS_TARGET_BUSY       0x08
#define SS_INTERMID          0x10
#define SS_INTERMID_COND_MET 0x14
#define SS_RSERV_CONFLICT    0x18
#define SS_CMD_TERMINATED    0x22
#define SS_QUEUE_FULL        0x28
#define MS_CMD_DONE    0x00
#define MS_EXTEND      0x01
#define MS_SDTR_LEN    0x03
#define MS_SDTR_CODE   0x01
#define MS_WDTR_LEN    0x02
#define MS_WDTR_CODE   0x03
#define MS_MDP_LEN    0x05
#define MS_MDP_CODE   0x00
#define M1_SAVE_DATA_PTR        0x02
#define M1_RESTORE_PTRS         0x03
#define M1_DISCONNECT           0x04
#define M1_INIT_DETECTED_ERR    0x05
#define M1_ABORT                0x06
#define M1_MSG_REJECT           0x07
#define M1_NO_OP                0x08
#define M1_MSG_PARITY_ERR       0x09
#define M1_LINK_CMD_DONE        0x0A
#define M1_LINK_CMD_DONE_WFLAG  0x0B
#define M1_BUS_DVC_RESET        0x0C
#define M1_ABORT_TAG            0x0D
#define M1_CLR_QUEUE            0x0E
#define M1_INIT_RECOVERY        0x0F
#define M1_RELEASE_RECOVERY     0x10
#define M1_KILL_IO_PROC         0x11
#define M2_QTAG_MSG_SIMPLE      0x20
#define M2_QTAG_MSG_HEAD        0x21
#define M2_QTAG_MSG_ORDERED     0x22
#define M2_IGNORE_WIDE_RESIDUE  0x23


#define ASC_MAX_SENSE_LEN   32
#define ASC_MIN_SENSE_LEN   14

typedef struct asc_req_sense {
	u_int8_t	err_code:7;
	u_int8_t	info_valid:1;
	u_int8_t	segment_no;
	u_int8_t	sense_key:4;
	u_int8_t	reserved_bit:1;
	u_int8_t	sense_ILI:1;
	u_int8_t	sense_EOM:1;
	u_int8_t	file_mark:1;
	u_int8_t	info1[4];
	u_int8_t	add_sense_len;
	u_int8_t	cmd_sp_info[4];
	u_int8_t	asc;
	u_int8_t	ascq;
	u_int8_t	fruc;
	u_int8_t	sks_byte0:7;
	u_int8_t	sks_valid:1;
	u_int8_t	sks_bytes[2];
	u_int8_t	notused[2];
	u_int8_t	ex_sense_code;
	u_int8_t	info2[4];
} ASC_REQ_SENSE;


/*
 * Adv Library functions available to drivers.
 */

int	AdvInitAsc3550Driver __P((ADW_SOFTC *));
int	AdvInitAsc38C0800Driver __P((ADW_SOFTC *));
int	AdvInitFrom3550EEP __P((ADW_SOFTC *));
int	AdvInitFrom38C0800EEP __P((ADW_SOFTC *));
int	AdvExeScsiQueue __P((ADW_SOFTC *, ADW_SCSI_REQ_Q *));
int	AdvISR __P((ADW_SOFTC *));
void	AdvResetChip __P((bus_space_tag_t, bus_space_handle_t));
int	AdvSendIdleCmd __P((ADW_SOFTC *, u_int16_t, u_int32_t));
int	AdvResetSCSIBus __P((ADW_SOFTC *));
int	AdvResetCCB __P((ADW_SOFTC *));

#define offsetof(type, member) ((size_t)(&((type *)0)->member))

#endif	/* _ADVANSYS_WIDE_LIBRARY_H_ */
