/*	$OpenBSD: if_san_te1.h,v 1.4 2004/12/07 06:10:24 mcbride Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Alex Feldman <al.feldman@sangoma.com> for SAN.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	__IF_SANTE1_H
#    define	__IF_SANTE1_H

#ifdef SDLA_TE1
# define EXTERN
#else
# define EXTERN extern
#endif

# include <dev/pci/if_san_front_end.h>


#define REG_GLOBAL_CFG			0x00
#define BIT_GLOBAL_PIO_OE		0x80
#define BIT_GLOBAL_PIO			0x40
#define BIT_GLOBAL_TRKEN		0x04
#define BIT_GLOBAL_E1			0x01

#define REG_RLPS_ALOS_DET_PER_0 0x01

#define REG_RECEIVE_OPT			0x02
#define BIT_RECEIVE_OPT_UNF		0x40

#define REG_TX_TIMING_OPT		0x06
#define BIT_TX_PLLREF1			0x08
#define BIT_TX_PLLREF0			0x04
#define BIT_TX_TXELSTBYP		0x01

#define REG_MASTER_DIAG			0x0A
#define BIT_MASTER_DIAG_PAYLB		0x20
#define BIT_MASTER_DIAG_LINELB		0x10
#define BIT_MASTER_DIAG_DDLB		0x04

#define REG_RESET			0x0E
#define BIT_RESET			0x01

#define REG_PRGD_CTRL			0x0F
#define BIT_PRGD_CTRL_HDLC3		0x80
#define BIT_PRGD_CTRL_HDLC2		0x40
#define BIT_PRGD_CTRL_HDLC1		0x20
#define BIT_PRGD_CTRL_Nx56k_GEN		0x10
#define BIT_PRGD_CTRL_Nx56k_DET		0x08
#define BIT_PRGD_CTRL_RXPATGEN		0x04
#define BIT_PRGD_CTRL_UNF_GEN		0x02
#define BIT_PRGD_CTRL_UNF_DET		0x01

#define REG_CDRC_CFG			0x10
#define BIT_CDRC_CFG_AMI		0x80
#define BIT_CDRC_CFG_LOS1		0x40
#define BIT_CDRC_CFG_LOS0		0x20

#define REG_CDRC_INT_STATUS		0x12
#define BIT_CDRC_INT_STATUS_LCVI	0x80
#define BIT_CDRC_INT_STATUS_LOSI	0x40
#define BIT_CDRC_INT_STATUS_LCSDI	0x20
#define BIT_CDRC_INT_STATUS_ZNDI	0x10
#define BIT_CDRC_INT_STATUS_LOSV	0x01

#define REG_ALTLOS_STATUS		0x13
#define BIT_ALTLOS_STATUS_ALTLOSE	0x80
#define BIT_ALTLOS_STATUS_ALTLOSI	0x40
#define BIT_ALTLOS_STATUS_ALTLOS	0x01

#define REG_RJAT_CFG			0x17
#define BIT_RJAT_CENT			0x10

#define REG_TJAT_CFG			0x1B
#define BIT_TJAT_CENT			0x10

#define REG_RX_ELST_CFG			0x1C
#define MASK_RX_ELST_CFG		0x03
#define BIT_RX_ELST_IR			0x02
#define BIT_RX_ELST_OR			0x01

#define REG_TX_ELST_CFG			0x20
#define MASK_TX_ELST_CFG		0x03
#define BIT_TX_ELST_IR			0x02
#define BIT_TX_ELST_OR			0x01

#define REG_BRIF_CFG			0x30
#define BIT_BRIF_NXDS0_1		0x80
#define BIT_BRIF_NXDS0_0		0x40
#define BIT_BRIF_CMODE			0x20
#define BIT_BRIF_RATE0			0x01

#define REG_BRIF_FR_PULSE_CFG		0x31
#define BIT_BRIF_FPMODE			0x20
#define BIT_BRIF_ROHM			0x08

#define REG_BRIF_DATA_CFG		0x32
#define BIT_BRIF_DATA_TRI_0		0x01

#define REG_BTIF_CFG			0x40
#define BIT_BTIF_NXDS0_1		0x80
#define BIT_BTIF_NXDS0_0		0x40
#define BIT_BTIF_CMODE			0x20
#define BIT_BTIF_DE			0x10
#define BIT_BTIF_FE			0x08
#define BIT_BTIF_RATE0			0x01

#define REG_BTIF_FR_PULSE_CFG		0x41
#define BIT_BTIF_FPMODE			0x01

#define REG_BTIF_CFG_STATUS		0x42
#define BIT_BTIF_CFG_STATUS_TPTYP	0x80
#define BIT_BTIF_CFG_STATUS_TPTYE	0x40
#define BIT_BTIF_CFG_STATUS_TDI		0x20
#define BIT_BTIF_CFG_STATUS_TSIGI	0x10
#define BIT_BTIF_CFG_STATUS_PTY_EXTD	0x08

#define REG_BTIF_BIT_OFF		0x44
#define BIT_BTIF_BIT_OFF_BOFF_EN	0x08
#define BIT_BTIF_BIT_OFF_BOFF_2		0x04
#define BIT_BTIF_BIT_OFF_BOFF_1		0x02
#define BIT_BTIF_BIT_OFF_BOFF_0		0x01

#define REG_T1_FRMR_CFG			0x48
#define BIT_T1_FRMR_ESF			0x20
#define BIT_T1_FRMR_ESFFA		0x10
#define BIT_T1_FRMR_FMS1		0x08
#define BIT_T1_FRMR_FMS0		0x04

#define REG_SIGX_CFG			0x50
#define BIT_SIGX_ESF			0x04
#define BIT_SIGX_IND			0x02
#define BIT_SIGX_PCCE			0x01
#define BIT_SIGX_SIGE			0x20
#define BIT_SIGX_COSS			0x40
#define MASK_SIGX_COSS_30_25		0x3F

#define REG_SIGX_TIMESLOT_IND_STATUS	0x51
#define BIT_SIGX_BUSY			0x80
#define REG_SIGX_TIMESLOT_IND_ACCESS	0x52
#define BIT_SIGX_TS_IND_ACCESS_READ	0x80
#define REG_SIGX_TIMESLOT_IND_DATA_BUFFER 0x53
#define REG_SIGX_SIGDATA		0x20
#define BIT_SIGX_SIGDATA_A		0x08
#define BIT_SIGX_SIGDATA_B		0x04
#define BIT_SIGX_SIGDATA_C		0x02
#define BIT_SIGX_SIGDATA_D		0x01

#define REG_SIGX_CHANCFG		0x40
#define BIT_SIGX_CHANCFG_RINV1		0x08
#define BIT_SIGX_CHANCFG_RINV0		0x04
#define BIT_SIGX_CHANCFG_RFIX		0x04
#define BIT_SIGX_CHANCFG_RPOL		0x02
#define BIT_SIGX_CHANCFG_RDEBE		0x01

#define REG_T1_XBAS_CFG			0x54
#define BIT_T1_XBAS_ZCS0		0x01
#define BIT_T1_XBAS_ZCS1		0x02
#define BIT_T1_XBAS_B8ZS		0x20
#define BIT_T1_XBAS_ESF			0x10

#define REG_T1_XBAS_ALARM_TX		0x55
#define BIT_T1_XBAS_ALARM_TX_XYEL	0x02
#define BIT_T1_XBAS_ALARM_TX_XAIS	0x01

/* PMON Framing Bit Error Count */
#define REG_PMON_BIT_ERROR		0x59
#define BITS_PMON_BIT_ERROR		0x7F
/* PMON OOF/COFA/Far End Block Error Count LSB */
#define REG_PMON_OOF_FEB_LSB_ERROR	0x5A
/* PMON OOF/COFA/Far End Block Error Count MSB */
#define REG_PMON_OOF_FEB_MSB_ERROR	0x5B
#define BITS_PMON_OOF_FEB_MSB_ERROR	0x03
/* PMON Bit Error/CRC Error Count LSB */
#define REG_PMON_BIT_CRC_LSB_ERROR	0x5C
/* PMON Bit Error/CRC Error Count MSB */
#define REG_PMON_BIT_CRC_MSB_ERROR	0x5D
#define BITS_PMON_BIT_CRC_MSB_ERROR	0x03
/* PMON LCV Count LSB */
#define REG_PMON_LCV_LSB_COUNT		0x5E
/* PMON LCV Count MSB */
#define REG_PMON_LCV_MSB_COUNT		0x5F
#define BITS_PMON_LCV_MSB_COUNT		0x1F

#define REG_T1_ALMI_CFG			0x60
#define BIT_T1_ALMI_CFG_ESF		0x10
#define BIT_T1_ALMI_CFG_FMS1		0x08
#define BIT_T1_ALMI_CFG_FMS0		0x04

#define REG_T1_ALMI_DET_STATUS		0x63
#define BIT_T1_ALMI_DET_STATUS_REDD	0x04
#define BIT_T1_ALMI_DET_STATUS_YELD	0x02
#define BIT_T1_ALMI_DET_STATUS_AISD	0x01

/* T1 XBOC Code */
#define REG_T1_XBOC_CODE		0x67
#define MASK_T1_XBOC_CODE		0x3F

/* T1 RBOC Enable */
#define REG_T1_RBOC_ENABLE		0x6A
#define BIT_T1_RBOC_ENABLE_IDLE		0x04
#define BIT_T1_RBOC_ENABLE_AVC		0x02
#define BIT_T1_RBOC_ENABLE_BOCE		0x01

/* T1 RBOC Code Status */
#define REG_T1_RBOC_CODE_STATUS		0x6B
#define BIT_T1_RBOC_CODE_STATUS_IDLEI	0x80
#define BIT_T1_RBOC_CODE_STATUS_BOCI	0x40
#define MASK_T1_RBOC_CODE_STATUS	0x3F

/* TPSC Indirect Register Access */
#define REG_TPSC_CFG					0x6C
#define MASK_TPSC_CFG					0x03
#define BIT_TPSC_IND					0x02
#define BIT_TPSC_PCCE					0x01
#define REG_TPSC_MICRO_ACCESS_STATUS			0x6D
#define BIT_TPSC_BUSY					0x80
#define REG_TPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL	0x6E
#define REG_TPSC_CHANNEL_INDIRECT_DATA_BUFFER		0x6F
#define REG_TPSC_DATA_CTRL_BYTE				0x20
#define MASK_TPSC_DATA_CTRL_BYTE			0xFC
#define BIT_TPSC_DATA_CTRL_BYTE_INVERT			0x80
#define BIT_TPSC_DATA_CTRL_BYTE_IDLE_DS0		0x40
#define BIT_TPSC_DATA_CTRL_BYTE_SIGNINV			0x10
#define BIT_TPSC_DATA_CTRL_BYTE_LOOP			0x04
#define BIT_TPSC_DATA_CTRL_BYTE_ZCS0			0x02
#define BIT_TPSC_DATA_CTRL_BYTE_ZCS1			0x01
#define REG_TPSC_IDLE_CODE_BYTE				0x40
#define REG_TPSC_SIGNALING_BYTE				0x60
#define REG_TPSC_E1_CTRL_BYTE				0x60
#define BIT_TPSC_E1_CTRL_BYTE_SUBS			0x80
#define BIT_TPSC_E1_CTRL_BYTE_DS0			0x40
#define BIT_TPSC_E1_CTRL_BYTE_DS1			0x20
#define BIT_TPSC_E1_CTRL_BYTE_A				0x08
#define BIT_TPSC_E1_CTRL_BYTE_B				0x04
#define BIT_TPSC_E1_CTRL_BYTE_C				0x02
#define BIT_TPSC_E1_CTRL_BYTE_D				0x01
#define BIT_TPSC_SIGNALING_BYTE_SIGC_0			0x80
#define BIT_TPSC_SIGNALING_BYTE_SIGC_1			0x40
#define BIT_TPSC_SIGNALING_BYTE_DS1			0x20
#define BIT_TPSC_SIGNALING_BYTE_A			0x08
#define BIT_TPSC_SIGNALING_BYTE_B			0x04
#define BIT_TPSC_SIGNALING_BYTE_C			0x02
#define BIT_TPSC_SIGNALING_BYTE_D			0x01

/* RPSC Indirect Register Access */
#define REG_RPSC_CFG					0x70
#define MASK_RPSC_CFG					0x03
#define BIT_RPSC_IND					0x02
#define BIT_RPSC_PCCE					0x01
#define REG_RPSC_MICRO_ACCESS_STATUS			0x71
#define BIT_RPSC_BUSY					0x80
#define REG_RPSC_CHANNEL_INDIRECT_ADDRESS_CONTROL	0x72
#define REG_RPSC_CHANNEL_INDIRECT_DATA_BUFFER		0x73
#define REG_RPSC_DATA_CTRL_BYTE				0x20
#define MASK_RPSC_DATA_CTRL_BYTE			0xFC
#define BIT_RPSC_DATA_CTRL_BYTE_DTRKC			0x40
#define BIT_RPSC_DATA_CTRL_BYTE_SIGNINV			0x04
#define REG_RPSC_DATA_COND_BYTE				0x40
#define REG_RPSC_SIGNALING_BYTE				0x60
#define BIT_RPSC_SIGNALING_BYTE_A			0x08
#define BIT_RPSC_SIGNALING_BYTE_B			0x04
#define BIT_RPSC_SIGNALING_BYTE_C			0x02
#define BIT_RPSC_SIGNALING_BYTE_D			0x01

#define REG_E1_TRAN_CFG			0x80
#define BIT_E1_TRAN_AMI			0x80
#define BIT_E1_TRAN_GENCRC		0x10
#define BIT_E1_TRAN_FDIS		0x08
#define BIT_E1_TRAN_FEBEDIS		0x04
#define BIT_E1_TRAN_INDIS		0x02
#define BIT_E1_TRAN_XDIS		0x01

#define REG_E1_FRMR_CFG			0x90
#define BIT_E1_FRMR_CRCEN		0x80
#define BIT_E1_FRMR_CASDIS		0x40
#define BIT_E1_FRMR_REFCRCEN		0x02

/* E1 FRMR Maintenance Mode Options */
#define REG_E1_FRMR_MAINT_OPT		0x91
#define BIT_E1_FRMR_MAINT_OPT_AISC	0x02

/* E1 FRMR framing status */
#define REG_E1_FRMR_FR_STATUS		0x96
#define BIT_E1_FRMR_FR_STATUS_C2NCIWV	0x80
#define BIT_E1_FRMR_FR_STATUS_OOFV	0x40
#define BIT_E1_FRMR_FR_STATUS_OOSMFV	0x20
#define BIT_E1_FRMR_FR_STATUS_OOCMFV	0x10
#define BIT_E1_FRMR_FR_STATUS_OOOFV	0x08
#define BIT_E1_FRMR_FR_STATUS_RAICCRCV	0x04
#define BIT_E1_FRMR_FR_STATUS_CFEBEV	0x02
#define BIT_E1_FRMR_FR_STATUS_V52LINKV	0x01

/* E1 FRMR Maintenance/Alram Status */
#define REG_E1_FRMR_MAINT_STATUS	0x97
#define BIT_E1_FRMR_MAINT_STATUS_RAIV	0x80
#define BIT_E1_FRMR_MAINT_STATUS_RED	0x08
#define BIT_E1_FRMR_MAINT_STATUS_AIS	0x04

 /* TDPR Configuration */
#define REG_TDPR_CFG			0xA8
#define BIT_TDPR_CFG_EN			0x01

/* TDPR Transmit Data */
#define REG_TDPR_TX_DATA		0xAD

/* RDLC Configuration */
#define REG_RDLC_CFG			0xC0
#define BIT_RDLC_CFG_EN			0x01

/* RDLC Interrupt Control */
#define REG_RDLC_INT_CTRL		0xC1
#define BIT_RDLC_INT_CTRL_INTE		0x80

/* RDLC Status */
#define REG_RDLC_STATUS			0xC2
#define BIT_RDLC_STATUS_PKIN		0x10
#define BIT_RDLC_STATUS_INTR		0x01

/* RDLC Data */
#define REG_RDLC_DATA			0xC3

#define REG_CSU_CFG			0xD6
#define MASK_CSU_CFG			0xC7
#define BIT_CSU_MODE2			0x04
#define BIT_CSU_MODE1			0x02
#define BIT_CSU_MODE0			0x01

/* RLPS Equalization Indirect Data (MSB) */
#define REG_RLPS_IND_DATA_1		0xD8
/* RLPS Equalization Indirect Data */
#define REG_RLPS_IND_DATA_2		0xD9
/* RLPS Equalization Indirect Data */
#define REG_RLPS_IND_DATA_3		0xDA
/* RLPS Equalization Indirect Data (LSB) */
#define REG_RLPS_IND_DATA_4		0xDB

#define REG_EQ_VREF				0xDC

#define REG_RLPS_FUSE_CTRL_STAT	0xDD

#define REG_XLPG_LINE_CFG		0xF0
#define REG_PRGD_INT_STATUS_EN		0xE1
#define BIT_PRGD_INT_STATUS_EN_SYNCE	0x80
#define BIT_PRGD_INT_STATUS_EN_BEE	0x40
#define BIT_PRGD_INT_STATUS_EN_XFERE	0x20
#define BIT_PRGD_INT_STATUS_EN_SYNCV	0x10
#define BIT_PRGD_INT_STATUS_EN_SYNCI	0x08
#define BIT_PRGD_INT_STATUS_EN_BEI	0x04
#define BIT_PRGD_INT_STATUS_EN_XFERI	0x02
#define BIT_PRGD_INT_STATUS_EN_OVR	0x01

#define REG_XLPG_WAVEFORM_ADDR	0xF2

#define REG_XLPG_WAVEFORM_DATA	0xF3

#define REG_XLPG_TPC			0xF4
#define BIT_XLPG_TPC_0			0x01

#define REG_XLPG_TNC			0xF5
#define BIT_XLPG_TNC_0			0x01

#define REG_RLPS_CFG_STATUS		0xF8
#define BIT_RLPS_CFG_STATUS_ALOSI	0x80
#define BIT_RLPS_CFG_STATUS_ALOSV	0x40
#define BIT_RLPS_CFG_STATUS_ALOSE	0x20
#define BIT_RLPS_CFG_STATUS_LONGE	0x01

#define REG_RLPS_ALOS_DET_CLR_THR	0xF9
#define BIT_RLPS_ALOS_CLR_THR_2		0x40
#define BIT_RLPS_ALOS_CLR_THR_1		0x20
#define BIT_RLPS_ALOS_CLR_THR_0		0x10
#define BIT_RLPS_ALOS_DET_THR_2		0x04
#define BIT_RLPS_ALOS_DET_THR_1		0x02
#define BIT_RLPS_ALOS_DET_THR_0		0x01

#define REG_RLPS_ALOS_DET_PER	0xFA

#define REG_RLPS_ALOS_CLR_PER	0xFB
#define BIT_RLPS_ALOS_CLR_PER_0	0x01

/* RLPS Equalization Indirect Address    */
#define REG_RLPS_EQ_ADDR		0xFC

/* RLPS Equalization Read/Write Select	 */
#define REG_RLPS_EQ_RWB			0xFD
#define BIT_RLPS_EQ_RWB			0x80

#define REG_RLPS_EQ_STATUS		0xFE

#define REG_RLPS_EQ_CFG			0xFF
#define MASK_RLPS_EQ_CFG		0xC7
#define BIT_RLPS_EQ_RESERVED		0x08
#define BIT_RLPS_EQ_FREQ_1		0x02
#define BIT_RLPS_EQ_FREQ_0		0x01

/********************************************/
/************ Interrupt Register ************/
/********************************************/
#define REG_INT_SRC_1			0x07
#define BITS_TX_INT_SRC_1		0x0C
#define BITS_RX_INT_SRC_1		0xF3
#define BIT_INT_SRC_1_PMON		0x80
#define BIT_INT_SRC_1_PRGD		0x40
#define BIT_INT_SRC_1_FRMR		0x20
#define BIT_INT_SRC_1_SIGX		0x10
#define BIT_INT_SRC_1_APRM		0x08
#define BIT_INT_SRC_1_TJAT		0x04
#define BIT_INT_SRC_1_RJAT		0x02
#define BIT_INT_SRC_1_CDRC		0x01

#define REG_INT_SRC_2			0x08
#define BITS_TX_INT_SRC_2		0x0F
#define BITS_RX_INT_SRC_2		0xF0
#define BIT_INT_SRC_2_RX_ELST		0x80
#define BIT_INT_SRC_2_RDLC_3		0x40
#define BIT_INT_SRC_2_RDLC_2		0x20
#define BIT_INT_SRC_2_RDLC_1		0x10
#define BIT_INT_SRC_2_TX_ELST		0x08
#define BIT_INT_SRC_2_TDPR_3		0x04
#define BIT_INT_SRC_2_TDPR_2		0x02
#define BIT_INT_SRC_2_TDPR_1		0x01

#define REG_INT_SRC_3			0x09
#define BITS_TX_INT_SRC_3		0x15
#define BITS_RX_INT_SRC_3		0xEA
#define BIT_INT_SRC_3_IBCD		0x80
#define BIT_INT_SRC_3_PDVD		0x40
#define BIT_INT_SRC_3_RBOC		0x20
#define BIT_INT_SRC_3_XPDE		0x10
#define BIT_INT_SRC_3_ALMI		0x08
#define BIT_INT_SRC_3_TRAN		0x04
#define BIT_INT_SRC_3_RLPS		0x02
#define BIT_INT_SRC_3_BTIF		0x01

#define REG_CDRC_INT_EN			0x11
#define BIT_CDRC_INT_EN_LCVE		0x80
#define BIT_CDRC_INT_EN_LOSE		0x40
#define BIT_CDRC_INT_EN_LCSDE		0x20
#define BIT_CDRC_INT_EN_ZNDE		0x10

#define REG_CDRC_INT_STATUS		0x12
#define BIT_CDRC_INT_STATUS_LCVI	0x80
#define BIT_CDRC_INT_STATUS_LOSI	0x40
#define BIT_CDRC_INT_STATUS_LCSDI	0x20
#define BIT_CDRC_INT_STATUS_ZNDI	0x10
#define BIT_CDRC_INT_STATUS_LOSV	0x01

#define REG_RJAT_INT_STATUS		0x14
#define BIT_RJAT_INT_STATUS_OVRI	0x02
#define BIT_RJAT_INT_STATUS_UNDI	0x01

#define REG_TJAT_INT_STATUS		0x18
#define BIT_TJAT_INT_STATUS_OVRI	0x02
#define BIT_TJAT_INT_STATUS_UNDI	0x01

#define REG_RX_ELST_INT_EN_STATUS	0x1D
#define BIT_RX_ELST_INT_EN_STATUS_SLIPE	0x04
#define BIT_RX_ELST_INT_EN_STATUS_SLIPD	0x02
#define BIT_RX_ELST_INT_EN_STATUS_SLIPI	0x01

#define REG_TX_ELST_INT_EN_STATUS	0x21
#define BIT_TX_ELST_INT_EN_STATUS_SLIPE	0x04
#define BIT_TX_ELST_INT_EN_STATUS_SLIPD	0x02
#define BIT_TX_ELST_INT_EN_STATUS_SLIPI	0x01

#define REG_T1_FRMR_INT_EN		0x49
#define BIT_T1_FRMR_INT_EN_COFAE	0x20
#define BIT_T1_FRMR_INT_EN_FERE		0x10
#define BIT_T1_FRMR_INT_EN_BEEE		0x08
#define BIT_T1_FRMR_INT_EN_SFEE		0x04
#define BIT_T1_FRMR_INT_EN_MFPE		0x02
#define BIT_T1_FRMR_INT_EN_INFRE	0x01

#define REG_T1_FRMR_INT_STATUS		0x4A
#define BIT_T1_FRMR_INT_STATUS_COFAI	0x80
#define BIT_T1_FRMR_INT_STATUS_FERI	0x40
#define BIT_T1_FRMR_INT_STATUS_BEEI	0x20
#define BIT_T1_FRMR_INT_STATUS_SFEI	0x10
#define BIT_T1_FRMR_INT_STATUS_MFPI	0x08
#define BIT_T1_FRMR_INT_STATUS_INFRI	0x04
#define BIT_T1_FRMR_INT_STATUS_MFP	0x02
#define BIT_T1_FRMR_INT_STATUS_INFR	0x01

#define REG_IBCD_CFG			0x4C
#define BIT_IBCD_CFG_DSEL1		0x08
#define BIT_IBCD_CFG_DSEL0		0x04
#define BIT_IBCD_CFG_ASEL1		0x02
#define BIT_IBCD_CFG_ASEL0		0x01

#define REG_IBCD_INT_EN_STATUS		0x4D
#define BIT_IBCD_INT_EN_STATUS_LBACP	0x80
#define BIT_IBCD_INT_EN_STATUS_LBDCP	0x40
#define BIT_IBCD_INT_EN_STATUS_LBAE	0x20
#define BIT_IBCD_INT_EN_STATUS_LBDE	0x10
#define BIT_IBCD_INT_EN_STATUS_LBAI	0x08
#define BIT_IBCD_INT_EN_STATUS_LBDI	0x04
#define BIT_IBCD_INT_EN_STATUS_LBA	0x02
#define BIT_IBCD_INT_EN_STATUS_LBD	0x01

#define REG_IBCD_ACTIVATE_CODE		0x4E
#define BIT_IBCD_ACTIVATE_CODE_ACT7	0x80
#define BIT_IBCD_ACTIVATE_CODE_ACT6	0x40
#define BIT_IBCD_ACTIVATE_CODE_ACT5	0x20
#define BIT_IBCD_ACTIVATE_CODE_ACT4	0x10
#define BIT_IBCD_ACTIVATE_CODE_ACT3	0x08
#define BIT_IBCD_ACTIVATE_CODE_ACT2	0x04
#define BIT_IBCD_ACTIVATE_CODE_ACT1	0x02
#define BIT_IBCD_ACTIVATE_CODE_ACT0	0x01

#define REG_IBCD_DEACTIVATE_CODE	0x4F
#define BIT_IBCD_DEACTIVATE_CODE_DACT7	0x80
#define BIT_IBCD_DEACTIVATE_CODE_DACT6	0x40
#define BIT_IBCD_DEACTIVATE_CODE_DACT5	0x20
#define BIT_IBCD_DEACTIVATE_CODE_DACT4	0x10
#define BIT_IBCD_DEACTIVATE_CODE_DACT3	0x08
#define BIT_IBCD_DEACTIVATE_CODE_DACT2	0x04
#define BIT_IBCD_DEACTIVATE_CODE_DACT1	0x02
#define BIT_IBCD_DEACTIVATE_CODE_DACT0	0x01

#define REG_PMON_INT_EN_STATUS		0x58
#define BIT_PMON_INT_EN_STATUS_INTE	0x04
#define BIT_PMON_INT_EN_STATUS_XFER	0x02
#define BIT_PMON_INT_EN_STATUS_OVR	0x01

#define REG_T1_ALMI_INT_EN		0x61
#define BIT_T1_ALMI_INT_EN_FASTD	0x10
#define BIT_T1_ALMI_INT_EN_ACCEL	0x08
#define BIT_T1_ALMI_INT_EN_YELE		0x04
#define BIT_T1_ALMI_INT_EN_REDE		0x02
#define BIT_T1_ALMI_INT_EN_AISE		0x01

#define REG_T1_ALMI_INT_STATUS		0x62
#define BIT_T1_ALMI_INT_STATUS_YELI	0x20
#define BIT_T1_ALMI_INT_STATUS_REDI	0x10
#define BIT_T1_ALMI_INT_STATUS_AISI	0x08
#define BIT_T1_ALMI_INT_STATUS_YEL	0x04
#define BIT_T1_ALMI_INT_STATUS_RED	0x02
#define BIT_T1_ALMI_INT_STATUS_AIS	0x01

#define REG_PDVD_INT_EN_STATUS		0x65
#define BIT_PDVD_INT_EN_STATUS_PDV	0x10
#define BIT_PDVD_INT_EN_STATUS_Z16DI	0x08
#define BIT_PDVD_INT_EN_STATUS_PDVI	0x04
#define BIT_PDVD_INT_EN_STATUS_Z16DE	0x02
#define BIT_PDVD_INT_EN_STATUS_PDVE	0x01

#define REG_XPDE_INT_EN_STATUS		0x69
#define BIT_XPDE_INT_EN_STATUS_STUFE	0x80
#define BIT_XPDE_INT_EN_STATUS_STUFF	0x40
#define BIT_XPDE_INT_EN_STATUS_STUFI	0x20
#define BIT_XPDE_INT_EN_STATUS_PDV	0x10
#define BIT_XPDE_INT_EN_STATUS_Z16DI	0x08
#define BIT_XPDE_INT_EN_STATUS_PDVI	0x04
#define BIT_XPDE_INT_EN_STATUS_Z16DE	0x02
#define BIT_XPDE_INT_EN_STATUS_PDVE	0x01

#define REG_T1_APRM_INT_STATUS		0x7A
#define BIT_T1_APRM_INT_STATUS_INTR	0x01

#define REG_E1_TRAN_INT_EN		0x84
#define BIT_E1_TRAN_INT_EN_SIGMFE	0x10
#define BIT_E1_TRAN_INT_EN_NFASE	0x08
#define BIT_E1_TRAN_INT_EN_MFE		0x04
#define BIT_E1_TRAN_INT_EN_SMFE		0x02
#define BIT_E1_TRAN_INT_EN_FRME		0x01

#define REG_E1_TRAN_INT_STATUS		0x85
#define BIT_E1_TRAN_INT_STATUS_SIGMFI	0x10
#define BIT_E1_TRAN_INT_STATUS_NFASI	0x08
#define BIT_E1_TRAN_INT_STATUS_MFI	0x04
#define BIT_E1_TRAN_INT_STATUS_SMFI	0x02
#define BIT_E1_TRAN_INT_STATUS_FRMI	0x01

#define REG_E1_FRMR_FRM_STAT_INT_EN		0x92
#define BIT_E1_FRMR_FRM_STAT_INT_EN_C2NCIWE	0x80
#define BIT_E1_FRMR_FRM_STAT_INT_EN_OOFE	0x40
#define BIT_E1_FRMR_FRM_STAT_INT_EN_OOSMFE	0x20
#define BIT_E1_FRMR_FRM_STAT_INT_EN_OOCMFE	0x10
#define BIT_E1_FRMR_FRM_STAT_INT_EN_COFAE	0x08
#define BIT_E1_FRMR_FRM_STAT_INT_EN_FERE	0x04
#define BIT_E1_FRMR_FRM_STAT_INT_EN_SMFERE	0x02
#define BIT_E1_FRMR_FRM_STAT_INT_EN_CMFERE	0x01

#define REG_E1_FRMR_M_A_INT_EN		0x93
#define BIT_E1_FRMR_M_A_INT_EN_RAIE	0x80
#define BIT_E1_FRMR_M_A_INT_EN_RMAIE	0x40
#define BIT_E1_FRMR_M_A_INT_EN_AISDE	0x20
#define BIT_E1_FRMR_M_A_INT_EN_REDE	0x08
#define BIT_E1_FRMR_M_A_INT_EN_AISE	0x04
#define BIT_E1_FRMR_M_A_INT_EN_FEBEE	0x02
#define BIT_E1_FRMR_M_A_INT_EN_CRCEE	0x01

/* E1 FRMR Framing status Interrupt Indication */
#define REG_E1_FRMR_FRM_STAT_INT_IND		0x94
#define BIT_E1_FRMR_FRM_STAT_INT_IND_C2NCIWI	0x80
#define BIT_E1_FRMR_FRM_STAT_INT_IND_OOFI	0x40
#define BIT_E1_FRMR_FRM_STAT_INT_IND_OOSMFI	0x20
#define BIT_E1_FRMR_FRM_STAT_INT_IND_OOCMFI	0x10
#define BIT_E1_FRMR_FRM_STAT_INT_IND_COFAI	0x08
#define BIT_E1_FRMR_FRM_STAT_INT_IND_FERI	0x04
#define BIT_E1_FRMR_FRM_STAT_INT_IND_SMFERI	0x02
#define BIT_E1_FRMR_FRM_STAT_INT_IND_CMFERI	0x01

#define REG_E1_FRMR_M_A_INT_IND		0x95
#define BIT_E1_FRMR_M_A_INT_IND_RAII	0x80
#define BIT_E1_FRMR_M_A_INT_IND_FMAII	0x40
#define BIT_E1_FRMR_M_A_INT_IND_AISDI	0x20
#define BIT_E1_FRMR_M_A_INT_IND_REDI	0x08
#define BIT_E1_FRMR_M_A_INT_IND_AISI	0x04
#define BIT_E1_FRMR_M_A_INT_IND_FEBEI	0x02
#define BIT_E1_FRMR_M_A_INT_IND_CRCEI	0x01

#define REG_E1_FRMR_P_A_INT_EN		0x9E
#define BIT_E1_FRMR_P_A_INT_EN_OOOFE	0x80
#define BIT_E1_FRMR_P_A_INT_EN_RAICCRCE	0x40
#define BIT_E1_FRMR_P_A_INT_EN_CFEBEE	0x20
#define BIT_E1_FRMR_P_A_INT_EN_V52LINKE	0x10
#define BIT_E1_FRMR_P_A_INT_EN_IFPE	0x08
#define BIT_E1_FRMR_P_A_INT_EN_ICSMFPE	0x04
#define BIT_E1_FRMR_P_A_INT_EN_ICMFPE	0x02
#define BIT_E1_FRMR_P_A_INT_EN_ISMFPE	0x01

#define REG_E1_FRMR_P_A_INT_STAT		0x9F
#define BIT_E1_FRMR_P_A_INT_STAT_OOOFI		0x80
#define BIT_E1_FRMR_P_A_INT_STAT_RAICCRCI	0x40
#define BIT_E1_FRMR_P_A_INT_STAT_CFEBEI		0x20
#define BIT_E1_FRMR_P_A_INT_STAT_V52LINKI	0x10
#define BIT_E1_FRMR_P_A_INT_STAT_IFPI		0x08
#define BIT_E1_FRMR_P_A_INT_STAT_ICSMFPI	0x04
#define BIT_E1_FRMR_P_A_INT_STAT_ICMFPI		0x02
#define BIT_E1_FRMR_P_A_INT_STAT_ISMFPI		0x01


/*The line code */
#define WAN_LC_AMI           0x01
#define WAN_LC_B8ZS          0x02
#define WAN_LC_HDB3          0x03

/* Framing mode (T1/E1)/Unframed */
#define WAN_FR_ESF           0x01
#define WAN_FR_D4            0x02
#define WAN_FR_ESF_JAPAN     0x03
#define WAN_FR_CRC4          0x04
#define WAN_FR_NCRC4         0x05
#define WAN_FR_UNFRAMED	0x06

/* For T1 only */
#define WAN_T1_LBO_0_DB      0x01
#define WAN_T1_LBO_75_DB     0x02
#define WAN_T1_LBO_15_DB     0x03
#define WAN_T1_LBO_225_DB    0x04
#define WAN_T1_0_110         0x05
#define WAN_T1_110_220       0x06
#define WAN_T1_220_330       0x07
#define WAN_T1_330_440       0x08
#define WAN_T1_440_550       0x09
#define WAN_T1_550_660       0x0A

/* For E1 only */
#define WAN_E1_120           0x0B
#define WAN_E1_75            0x0C

/* Clocking Master/Normal */
#define WAN_NORMAL_CLK	0x01
#define WAN_MASTER_CLK	0x02

#define NUM_OF_T1_CHANNELS	24
#define NUM_OF_E1_TIMESLOTS	31
#define NUM_OF_E1_CHANNELS	32
#define ENABLE_ALL_CHANNELS	0xFFFFFFFF

#define E1_FRAMING_TIMESLOT	0
#define E1_SIGNALING_TIMESLOT	16

/* Alram bit mask */
#define BIT_ALOS_ALARM		0x0001
#define BIT_LOS_ALARM		0x0002
#define BIT_ALTLOS_ALARM	0x0004
#define BIT_OOF_ALARM		0x0008
#define BIT_RED_ALARM		0x0010
#define BIT_AIS_ALARM		0x0020
#define BIT_OOSMF_ALARM		0x0040
#define BIT_OOCMF_ALARM		0x0080
#define BIT_OOOF_ALARM		0x0100
#define BIT_RAI_ALARM		0x0200
#define BIT_YEL_ALARM		0x0400
#define BIT_LOOPUP_CODE		0x2000
#define BIT_LOOPDOWN_CODE	0x4000
#define BIT_TE1_ALARM		0x8000	/* for Windows only */

/* Performamce monitor counter defines */
#define frm_bit_error		pmon1	/* E1/T1   */
#define oof_errors		pmon2	/* T1 only */
#define far_end_blk_errors	pmon2	/* E1 only */
#define bit_errors		pmon3	/* T1 only */
#define crc_errors		pmon3	/* E1 only */
#define lcv			pmon4	/* E1/T1   */

/* Line loopback modes */
#define WAN_TE1_LINELB_MODE	0x01
#define WAN_TE1_PAYLB_MODE	0x02
#define WAN_TE1_DDLB_MODE	0x03
#define WAN_TE1_TX_LB_MODE	0x04

/* Line loopback activate/deactive modes */
#define WAN_TE1_ACTIVATE_LB	0x01
#define WAN_TE1_DEACTIVATE_LB	0x02

/* Loopback commands (T1.107-1995 p.44) */
#define LINELB_TE1_TIMER	40	/* 40ms */
#define LINELB_CODE_CNT		10	/* no. of repetitions for lb_code */
#define LINELB_CHANNEL_CNT	10	/* no. of repetitions for channel */
#define LINELB_ACTIVATE_CODE	0x07
#define LINELB_DEACTIVATE_CODE	0x1C
#define LINELB_DS3LINE		0x1B
#define LINELB_DS1LINE_1	0x21
#define LINELB_DS1LINE_2	0x22
#define LINELB_DS1LINE_3	0x23
#define LINELB_DS1LINE_4	0x24
#define LINELB_DS1LINE_5	0x25
#define LINELB_DS1LINE_6	0x26
#define LINELB_DS1LINE_7	0x27
#define LINELB_DS1LINE_8	0x28
#define LINELB_DS1LINE_9	0x29
#define LINELB_DS1LINE_10	0x2A
#define LINELB_DS1LINE_11	0x2B
#define LINELB_DS1LINE_12	0x2C
#define LINELB_DS1LINE_13	0x2D
#define LINELB_DS1LINE_14	0x2E
#define LINELB_DS1LINE_15	0x2F
#define LINELB_DS1LINE_16	0x30
#define LINELB_DS1LINE_17	0x31
#define LINELB_DS1LINE_18	0x32
#define LINELB_DS1LINE_19	0x33
#define LINELB_DS1LINE_20	0x34
#define LINELB_DS1LINE_21	0x35
#define LINELB_DS1LINE_22	0x36
#define LINELB_DS1LINE_23	0x37
#define LINELB_DS1LINE_24	0x38
#define LINELB_DS1LINE_25	0x39
#define LINELB_DS1LINE_26	0x3A
#define LINELB_DS1LINE_27	0x3B
#define LINELB_DS1LINE_28	0x3C
#define LINELB_DS1LINE_ALL	0x13
#define LINELB_DS1LINE_MASK	0x1F

/* Interrupt polling delay */
#define POLLING_TE1_TIMER	1000	/* 1 sec */

/* TE1 critical flag */
#define TE_TIMER_RUNNING	0x01
#define TE_TIMER_KILL		0x02
#define LINELB_WAITING		0x03
#define LINELB_CODE_BIT		0x04
#define LINELB_CHANNEL_BIT	0x05
#define TE_CONFIGURED		0x06

#if 0
#define TE_TIMER_RUNNING	0x01
#define TE_TIMER_KILL		0x02
#define LINELB_WAITING		0x04
#define LINELB_CODE_BIT		0x08
#define LINELB_CHANNEL_BIT	0x10
#endif

/* TE1 timer flags */
#define TE_LINELB_TIMER		0x01
#define TE_LINKDOWN_TIMER	0x02
#define TE_SET_INTR		0x03
#define TE_ABCD_UPDATE		0x04
#define TE_LINKUP_TIMER		0x05

/* TE1 T1/E1 interrupt setting delay */
#define INTR_TE1_TIMER		150	/* 50 ms */

#define IS_T1(te_cfg)	((te_cfg)->media == WAN_MEDIA_T1)
#define IS_E1(te_cfg)	((te_cfg)->media == WAN_MEDIA_E1)

#define IS_TE1(te_cfg)	(IS_T1(te_cfg) || IS_E1(te_cfg))

#define IS_TE1_UNFRAMED(card)   ((card)->te_cfg.frame == WAN_FR_UNFRAMED)

#define GET_TE_CHANNEL_RANGE(card)				\
		(IS_T1(&card->te_cfg) ? NUM_OF_T1_CHANNELS :	\
		 IS_E1(&card->te_cfg) ? NUM_OF_E1_CHANNELS :0)

#define ALOS_ALARM(val)		(val & BIT_ALOS_ALARM) ? "ON" : "OFF"
#define LOS_ALARM(val)		(val & BIT_LOS_ALARM) ? "ON" : "OFF"
#define ALTLOS_ALARM(val)	(val & BIT_ALTLOS_ALARM) ? "ON" : "OFF"
#define OOF_ALARM(val)		(val & BIT_OOF_ALARM) ? "ON" : "OFF"
#define RED_ALARM(val)		(val & BIT_RED_ALARM) ? "ON" : "OFF"
#define AIS_ALARM(val)		(val & BIT_AIS_ALARM) ? "ON" : "OFF"
#define OOSMF_ALARM(val)	(val & BIT_OOSMF_ALARM) ? "ON" : "OFF"
#define OOCMF_ALARM(val)	(val & BIT_OOCMF_ALARM) ? "ON" : "OFF"
#define OOOF_ALARM(val)		(val & BIT_OOOF_ALARM) ? "ON" : "OFF"
#define RAI_ALARM(val)		(val & BIT_RAI_ALARM) ? "ON" : "OFF"
#define YEL_ALARM(val)		(val & BIT_YEL_ALARM) ? "ON" : "OFF"

#define MEDIA_DECODE(val)	(val == WAN_MEDIA_T1) ? "T1" :	\
				(val == WAN_MEDIA_E1) ? "E1" : "Unknown"

#define LCODE_DECODE(val)	(val == WAN_LC_AMI)  ? "AMI" :	\
				(val == WAN_LC_B8ZS) ? "B8ZS" :	\
				(val == WAN_LC_HDB3) ? "HDB3" : "Unknown"

#define FRAME_DECODE(val)	(val == WAN_FR_ESF)		? "ESF"  : \
				(val == WAN_FR_D4)		? "D4"   : \
				(val == WAN_FR_CRC4)		? "CRC4" : \
				(val == WAN_FR_NCRC4)	? "non-CRC4" :	\
				(val == WAN_FR_UNFRAMED)	? \
				    "Unframed" : "Unknown"

#define TECLK_DECODE(val)	(val == WAN_NORMAL_CLK) ? "Normal" :	\
				(val == WAN_MASTER_CLK) ? "Master" : \
				"Unknown"

#define LBO_DECODE(val)		\
	(val == WAN_T1_LBO_0_DB)	? "0db" :	\
	(val == WAN_T1_LBO_75_DB)	? "7.5db" :	\
	(val == WAN_T1_LBO_15_DB)	? "15dB" :	\
	(val == WAN_T1_LBO_225_DB)	? "22.5dB" :	\
	(val == WAN_T1_0_110)	? "0-110ft" :	\
	(val == WAN_T1_110_220)	? "110-220ft" :	\
	(val == WAN_T1_220_330)	? "220-330ft" :	\
	(val == WAN_T1_330_440)	? "330-440ft" :	\
	(val == WAN_T1_440_550)	? "440-550ft" :	\
	(val == WAN_T1_550_660)	? "550-660ft" : "Unknown"


/*
 * T1/E1 configuration structures.
 */
typedef struct sdla_te_cfg {
	unsigned char media;
	unsigned char lcode;
	unsigned char frame;
	unsigned char lbo;
	unsigned char te_clock;
	unsigned long active_ch;
	unsigned char high_impedance_mode;
} sdla_te_cfg_t;

/* Performamce monitor counters */
typedef struct pmc_pmon {
	unsigned long pmon1;
	unsigned long pmon2;
	unsigned long pmon3;
	unsigned long pmon4;
} pmc_pmon_t;

#ifdef _KERNEL

/* 
 * Constants for the SET_T1_E1_SIGNALING_CFG/READ_T1_E1_SIGNALING_CFG commands
 */

/* the structure for setting the signaling permission */
#pragma pack(1)
typedef struct {
	unsigned char time_slot[32];
} te_signaling_perm_t;
#pragma pack()

/* settings for the signaling permission structure */
#define TE_SIG_DISABLED		0x00 /* signaling is disabled */
#define TE_RX_SIG_ENABLED	0x01 /* receive signaling is enabled */
#define TE_TX_SIG_ENABLED	0x02 /* transmit signaling is enabled */
#define TE_SET_TX_SIG_BITS	0x80 /* a flag indicating that outgoing
					signaling bits should be set */

/* the structure used for the
 * SET_T1_E1_SIGNALING_CFG/READ_T1_E1_SIGNALING_CFG command
 */
#pragma pack(1)
typedef struct {
	/* signaling permission structure */
	te_signaling_perm_t sig_perm;
	/* loop signaling processing counter */
	unsigned char sig_processing_counter;
	/* pointer to the signaling permission structure */
	unsigned long ptr_te_sig_perm_struct;
	/* pointer to the receive signaling structure */
	unsigned long ptr_te_Rx_sig_struct;
	/* pointer to the transmit signaling structure */
	unsigned long ptr_te_Tx_sig_struct;
} te_signaling_cfg_t;
#pragma pack()

/* the structure used for reading and setting the signaling bits */
#pragma pack(1)
typedef struct {
	unsigned char time_slot[32];
} te_signaling_status_t;
#pragma pack()

typedef struct {
	unsigned char	SIGX_chg_30_25;
	unsigned char	SIGX_chg_24_17;
	unsigned char	SIGX_chg_16_9;
	unsigned char	SIGX_chg_8_1;

	unsigned long	ptr_te_sig_perm_off;
	unsigned long	ptr_te_Rx_sig_off;
	unsigned long	ptr_te_Tx_sig_off;

	sdla_te_cfg_t	te_cfg;		/* TE1 hw configuration */
	unsigned long	te_alarm;	/* TE1 alarm */
	pmc_pmon_t	te_pmon;	/* TE PMON counters */
	unsigned char	te_rx_lb_cmd;	/* Received LB cmd */
	unsigned long	te_rx_lb_time;	/* Time when LB cmd received */

	unsigned char	te_tx_lb_cmd;	/* Received LB cmd */
	unsigned long	te_tx_lb_cnt;	/* Time when LB cmd received */
	unsigned char	te_critical;	/* T1/E1 critical flag */
	struct timeout	te_timer;	/* Timer  */
	unsigned char	te_timer_cmd;
} sdla_te_softc_t;


EXTERN int sdla_te_defcfg(void *);
EXTERN int sdla_te_setcfg(void *, struct ifmedia *);
EXTERN void sdla_te_settimeslot(void *, unsigned long);
EXTERN unsigned long sdla_te_gettimeslot(void *);
EXTERN short sdla_te_config(void *);
EXTERN void sdla_te_unconfig(void *);
EXTERN unsigned long sdla_te_alarm(void *, int);
EXTERN void sdla_te_alarm_print(void *);
EXTERN void sdla_te_pmon(void *);
EXTERN void sdla_flush_te1_pmon(void *);
EXTERN void sdla_te_intr(void *);
EXTERN int sdla_set_te1_lb_modes(void *, unsigned char, unsigned char);
EXTERN void sdla_te_polling(void *);
EXTERN void sdla_te_timer(void *);
EXTERN int sdla_te_udp(void *, void *, unsigned char *);
EXTERN void aft_green_led_ctrl(void *, int);
#endif /* _KERNEL */

#undef EXTERN

#endif /* __IF_SANTE1_H */
