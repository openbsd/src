/*	$OpenBSD: opl3sa3reg.h,v 1.1 1999/10/06 12:48:32 fgsch Exp $	*/
/*	$NetBSD: opl3sa3reg.h,v 1.1 1999/10/05 03:38:17 itohy Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * YAMAHA YMF715x (OPL3 Single-chip Audio System 3; OPL3-SA3)
 * control register description
 *
 * Other ports (SBpro, WSS CODEC, MPU401, OPL3, etc.) are NOT listed here.
 */

/*
 * direct registers
 */

/* offset from the base address */
#define SA3_CTL_INDEX	0	/* Index port (R/W) */
#define SA3_CTL_DATA	1	/* Data register port (R/W) */

#define SA3_CTL_NPORT	2	/* number of ports */

/*
 * indirect registers
 */

#define SA3_PWR_MNG		0x01	/* Power management (R/W) */
#define   SA3_PWR_MNG_ADOWN	0x20	/* Analog Down */
#define   SA3_PWR_MNG_PSV	0x04	/* Power save */
#define   SA3_PWR_MNG_PDN	0x02	/* Power down */
#define   SA3_PWR_MNG_PDX	0x01	/* Oscillation stop */
#define SA3_PWR_MNG_DEFAULT	0x00	/* default value */

#define SA3_SYS_CTL		0x02	/* System control (R/W) */
#define   SA3_SYS_CTL_SBHE	0x80	/* 0: AT-bus, 1: XT-bus */
#define   SA3_SYS_CTL_YMODE	0x30	/* 3D Enhancement mode */
#define     SA3_SYS_CTL_YMODE0	0x00	/* Desktop mode  (speaker 5-12cm) */
#define     SA3_SYS_CTL_YMODE1	0x10	/* Notebook PC mode (1)  (3cm) */
#define     SA3_SYS_CTL_YMODE2	0x20	/* Notebook PC mode (2)  (1.5cm) */
#define     SA3_SYS_CTL_YMODE3	0x30	/* Hi-Fi mode            (16-38cm) */
#define   SA3_SYS_CTL_IDSEL	0x06	/* Specify DSP version of SBPro */
#define     SA3_SYS_CTL_IDSEL0	0x00	/* major 0x03, minor 0x01 */
#define     SA3_SYS_CTL_IDSEL1	0x02	/* major 0x02, minor 0x01 */
#define     SA3_SYS_CTL_IDSEL2	0x04	/* major 0x01, minor 0x05 */
#define     SA3_SYS_CTL_IDSEL3	0x06	/* major 0x00, minor 0x00 */
#define   SA3_SYS_CTL_VZE	0x01	/* ZV */
#define SA3_SYS_CTL_DEFAULT	0x00	/* default value */

#define SA3_IRQ_CONF		0x03	/* Interrupt Channel config (R/W) */
#define   SA3_IRQ_CONF_OPL3_B	0x80	/* OPL3 uses IRQ-B */
#define   SA3_IRQ_CONF_MPU_B	0x40	/* MPU401 uses IRQ-B */
#define   SA3_IRQ_CONF_SB_B	0x20	/* Sound Blaster uses IRQ-B */
#define   SA3_IRQ_CONF_WSS_B	0x10	/* WSS CODEC uses IRQ-B */
#define   SA3_IRQ_CONF_OPL3_A	0x08	/* OPL3 uses IRQ-A */
#define   SA3_IRQ_CONF_MPU_A	0x04	/* MPU401 uses IRQ-A */
#define   SA3_IRQ_CONF_SB_A	0x02	/* Sound Blaster uses IRQ-A */
#define   SA3_IRQ_CONF_WSS_A	0x01	/* WSS CODEC uses IRQ-A */
#define SA3_IRQ_CONF_DEFAULT	(SA3_IRQ_CONF_MPU_B | SA3_IRQ_CONF_SB_B | \
				 SA3_IRQ_CONF_OPL3_A | SA3_IRQ_CONF_WSS_A)

#define SA3_IRQA_STAT		0x04	/* Interrupt (IRQ-A) STATUS (RO) */
#define SA3_IRQB_STAT		0x05	/* Interrupt (IRQ-B) STATUS (RO) */
#define   SA3_IRQ_STAT_MV	0x40	/* Hardware Volume Interrupt */
#define   SA3_IRQ_STAT_OPL3	0x20	/* Internal FM-synthesizer timer */
#define   SA3_IRQ_STAT_MPU	0x10	/* MPU401 Interrupt */
#define   SA3_IRQ_STAT_SB	0x08	/* Sound Blaster Playback Interrupt */
#define   SA3_IRQ_STAT_TI	0x04	/* Timer Flag of CODEC */
#define   SA3_IRQ_STAT_CI	0x02	/* Recording Flag of CODEC */
#define   SA3_IRQ_STAT_PI	0x01	/* Playback Flag of CODEC */

#define SA3_DMA_CONF		0x06	/* DMA configuration (R/W) */
#define   SA3_DMA_CONF_SB_B	0x40	/* Sound Blaster playback uses DMA-B */
#define   SA3_DMA_CONF_WSS_R_B	0x20	/* WSS CODEC recording uses DMA-B */
#define   SA3_DMA_CONF_WSS_P_B	0x10	/* WSS CODEC playback uses DMA-B */
#define   SA3_DMA_CONF_SB_A	0x04	/* Sound Blaster playback uses DMA-A */
#define   SA3_DMA_CONF_WSS_R_A	0x02	/* WSS CODEC recording uses DMA-A */
#define   SA3_DMA_CONF_WSS_P_A	0x01	/* WSS CODEC playback uses DMA-A */
#define SA3_DMA_CONF_DEFAULT	(SA3_DMA_CONF_SB_B | SA3_DMA_CONF_WSS_R_B | \
				 SA3_DMA_CONF_WSS_P_A)

#define SA3_VOL_L		0x07	/* Master Volume Lch (R/W) */
#define SA3_VOL_R		0x08	/* Master Volume Rch (R/W) */
#define   SA3_VOL_MUTE		0x80	/* Mute the channel */
#define   SA3_VOL_MV		0x0f	/* Master Volume bits */
#define     SA3_VOL_MV_0	0x00	/*   0dB (maximum volume) */
#define     SA3_VOL_MV_2	0x01	/*  -2dB */
#define     SA3_VOL_MV_4	0x02	/*  -4dB */
#define     SA3_VOL_MV_6	0x03	/*  -6dB */
#define     SA3_VOL_MV_8	0x04	/*  -8dB */
#define     SA3_VOL_MV_10	0x05	/* -10dB */
#define     SA3_VOL_MV_12	0x06	/* -12dB */
#define     SA3_VOL_MV_14	0x07	/* -14dB (default) */
#define     SA3_VOL_MV_16	0x08	/* -16dB */
#define     SA3_VOL_MV_18	0x09	/* -18dB */
#define     SA3_VOL_MV_20	0x0a	/* -20dB */
#define     SA3_VOL_MV_22	0x0b	/* -22dB */
#define     SA3_VOL_MV_24	0x0c	/* -24dB */
#define     SA3_VOL_MV_26	0x0d	/* -26dB */
#define     SA3_VOL_MV_28	0x0e	/* -28dB */
#define     SA3_VOL_MV_30	0x0f	/* -30dB (minimum volume) */
#define SA3_VOL_DEFAULT		SA3_VOL_MV_14

#define SA3_MIC_VOL		0x09	/* MIC Volume (R/W) */
#define   SA3_MIC_MUTE		0x80	/* Mute Mic Volume */
#define   SA3_MIC_MCV		0x1f	/* Mic volume bits */
#define     SA3_MIC_MCV12	0x00	/* +12.0dB (maximum volume) */
#define     SA3_MIC_MCV10_5	0x01	/* +10.5dB */
#define     SA3_MIC_MCV9	0x02	/*  +9.0dB */
#define     SA3_MIC_MCV7_5	0x03	/*  +7.5dB */
#define     SA3_MIC_MCV6	0x04	/*  +6.0dB */
#define     SA3_MIC_MCV4_5	0x05	/*  +4.5dB */
#define     SA3_MIC_MCV3	0x06	/*  +3.0dB */
#define     SA3_MIC_MCV1_5	0x07	/*  +1.5dB */
#define     SA3_MIC_MCV_0	0x08	/*   0.0dB (default) */
#define     SA3_MIC_MCV_1_5	0x09	/*  -1.5dB */
#define     SA3_MIC_MCV_3_0	0x0a	/*  -3.0dB */
#define     SA3_MIC_MCV_4_5	0x0b	/*  -4.5dB */
#define     SA3_MIC_MCV_6	0x0c	/*  -6.0dB */
#define     SA3_MIC_MCV_7_5	0x0d	/*  -7.5dB */
#define     SA3_MIC_MCV_9	0x0e	/*  -9.0dB */
#define     SA3_MIC_MCV_10_5	0x0f	/* -10.5dB */
#define     SA3_MIC_MCV_12	0x10	/* -12.0dB */
#define     SA3_MIC_MCV_13_5	0x11	/* -13.5dB */
#define     SA3_MIC_MCV_15	0x12	/* -15.0dB */
#define     SA3_MIC_MCV_16_5	0x13	/* -16.5dB */
#define     SA3_MIC_MCV_18	0x14	/* -18.0dB */
#define     SA3_MIC_MCV_19_5	0x15	/* -19.5dB */
#define     SA3_MIC_MCV_21	0x16	/* -21.0dB */
#define     SA3_MIC_MCV_22_5	0x17	/* -22.5dB */
#define     SA3_MIC_MCV_24	0x18	/* -24.0dB */
#define     SA3_MIC_MCV_25_5	0x19	/* -25.5dB */
#define     SA3_MIC_MCV_27	0x1a	/* -27.0dB */
#define     SA3_MIC_MCV_28_5	0x1b	/* -28.5dB */
#define     SA3_MIC_MCV_30	0x1c	/* -30.0dB */
#define     SA3_MIC_MCV_31_5	0x1d	/* -31.5dB */
#define     SA3_MIC_MCV_33	0x1e	/* -33.0dB */
#define     SA3_MIC_MCV_34_5	0x1f	/* -34.5dB (minimum volume) */
#define SA3_MIC_VOL_DEFAULT	(SA3_MIC_MUTE | SA3_MIC_MCV_0)

#define SA3_MISC		0x0a	/* Miscellaneous */
#define   SA3_MISC_VEN		0x80	/* Enable hardware volume control */
#define   SA3_MISC_MCSW		0x10	/* A/D is connected to  0: Rch of Mic,
					   1: loopback of monaural output */
#define   SA3_MISC_MODE		0x08	/* 0: SB mode, 1: WSS mode (RO) */
#define   SA3_MISC_VER		0x07	/* Version of OPL3-SA3 (RO) */
					/*	(4 or 5?) */
/*#define SA3_MISC_DEFAULT	(SA3_MISC_VEN | (4 or 5?)) */

/* WSS DMA Base counters (R/W) used for suspend/resume */
#define SA3_DMA_CNT_PLAY_LOW	0x0b	/* Playback Base Counter (Low) */
#define SA3_DMA_CNT_PLAY_HIGH	0x0c	/* Playback Base Counter (High) */
#define SA3_DMA_CNT_REC_LOW	0x0d	/* Recording Base Counter (Low) */
#define SA3_DMA_CNT_REC_HIGH	0x0e	/* Recording Base Counter (High) */

#define SA3_WSS_INT_SCAN	0x0f	/* WSS Interrupt Scan out/in (R/W) */
#define   SA3_WSS_INT_SCAN_STI	0x04	/* 1: TI = "1" and IRQ active */
#define   SA3_WSS_INT_SCAN_SCI	0x02	/* 1: CI = "1" and IRQ active */
#define   SA3_WSS_INT_SCAN_SPI	0x01	/* 1: PI = "1" and IRQ active */
#define SA3_WSS_INT_DEFAULT	0x00	/* default value */

#define SA3_SB_SCAN		0x10	/* SB Internal State Scan out/in (R/W)*/
#define   SA3_SB_SCAN_SBPDA	0x80	/* Sound Blaster Power Down ack */
#define   SA3_SB_SCAN_SS	0x08	/* Scan Select */
#define   SA3_SB_SCAN_SM	0x04	/* Scan Mode 1: read out, 0: write in */
#define   SA3_SB_SCAN_SE	0x02	/* Scan Enable */
#define   SA3_SB_SCAN_SBPDR	0x01	/* Sound Blaster Power Down Request */
#define SA3_SB_SCAN_DEFAULT	0x00	/* default value */

#define SA3_SB_SCAN_DATA	0x11	/* SB Internal State Scan Data (R/W)*/

#define SA3_DPWRDWN		0x12	/* Digital Partial Power Down (R/W) */
#define   SA3_DPWRDWN_JOY	0x80	/* Joystick power down */
#define   SA3_DPWRDWN_MPU	0x40	/* MPU401 power down */
#define   SA3_DPWRDWN_MCLKO	0x20	/* Master Clock disable */
#define   SA3_DPWRDWN_FM	0x10	/* FM (OPL3) power down */
#define   SA3_DPWRDWN_WSS_R	0x08	/* WSS recording power down */
#define   SA3_DPWRDWN_WSS_P	0x04	/* WSS playback power down */
#define   SA3_DPWRDWN_SB	0x02	/* Sound Blaster power down */
#define   SA3_DPWRDWN_PNP	0x01	/* PnP power down */
#define SA3_DPWRDWN_DEFAULT	0x00	/* default value */

#define SA3_APWRDWN		0x13	/* Analog Partial Power Down (R/W) */
#define   SA3_APWRDWN_FMDAC	0x10	/* FMDAC for OPL3 power down */ 
#define   SA3_APWRDWN_AD	0x08	/* A/D for WSS recording power down */
#define   SA3_APWRDWN_DA	0x04	/* D/A for WSS playback power down */
#define   SA3_APWRDWN_SBDAC	0x02	/* D/A for SB power down */
#define   SA3_APWRDWN_WIDE	0x01	/* Wide Stereo power down */
#define SA3_APWRDWN_DEFAULT	0x00	/* default value */

#define SA3_3D_WIDE		0x14	/* 3D Enhanced control (WIDE) (R/W) */
#define   SA3_3D_WIDE_WIDER	0x70	/* Rch of wide 3D enhanced control */
#define   SA3_3D_WIDE_WIDEL	0x07	/* Lch of wide 3D enhanced control */
#define SA3_3D_WIDE_DEFAULT	0x00	/* default value */

#define SA3_3D_BASS		0x15	/* 3D Enhanced control (BASS) (R/W) */
#define   SA3_3D_BASS_BASSR	0x70	/* Rch of bass 3D enhanced control */
#define   SA3_3D_BASS_BASSL	0x07	/* Lch of bass 3D enhanced control */
#define SA3_3D_BASS_DEFAULT	0x00	/* default value */

#define SA3_3D_TREBLE		0x16	/* 3D Enhanced control (TREBLE) (R/W) */
#define   SA3_3D_TREBLE_TRER	0x70	/* Rch of treble 3D enhanced control */
#define   SA3_3D_TREBLE_TREL	0x07	/* Lch of treble 3D enhanced control */
#define SA3_3D_TREBLE_DEFAULT	0x00	/* default value */

/* common to the 3D enhance registers */
#define   SA3_3D_BITS		0x07
#define   SA3_3D_LSHIFT		0
#define   SA3_3D_RSHIFT		4

#define SA3_HVOL_INTR_CNF	0x17	/* Hardware Volume Intr Channel (R/W) */
#define   SA3_HVOL_INTR_CNF_B	0x20	/* Hardware Volume uses IRQ-B */
#define   SA3_HVOL_INTR_CNF_A	0x10	/* Hardware Volume uses IRQ-A */
#define SA3_HVOL_INTR_CNF_DEFAULT	0x00

#define SA3_MULTI_STAT		0x18	/* Multi-purpose Select Pin Stat (RO) */
#define   SA3_MULTI_STAT_SEL	0x70	/* State of SEL2-0 pins */
