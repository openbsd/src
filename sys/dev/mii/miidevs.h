/*	$OpenBSD: miidevs.h,v 1.49 2004/10/17 22:32:25 deraadt Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: miidevs,v 1.46 2004/10/17 22:32:15 deraadt Exp 
 */
/* $NetBSD: miidevs,v 1.3 1998/11/05 03:43:43 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * List of known MII OUIs
 */

#define	MII_OUI_3COM	0x00105a	/* 3com */
#define	MII_OUI_ALTIMA	0x0010a9	/* Altima Communications */
#define	MII_OUI_AMD	0x00001a	/* Advanced Micro Devices */
#define	MII_OUI_BROADCOM	0x001018	/* Broadcom Corporation */
#define	MII_OUI_ENABLESEMI	0x0010dd	/* Enable Semiconductor */
#define	MII_OUI_DAVICOM	0x00606e	/* Davicom Semiconductor */
#define	MII_OUI_MARVELL	0x005043	/* Marvell Semiconductor */
#define	MII_OUI_ICS	0x00a0be	/* Integrated Circuit Systems */
#define	MII_OUI_INTEL	0x00aa00	/* Intel */
#define	MII_OUI_LEVEL1	0x00207b	/* Level 1 */
#define	MII_OUI_MYSON	0x00c0b4	/* Myson Technology */
#define	MII_OUI_NATSEMI	0x080017	/* National Semiconductor */
#define	MII_OUI_PLESSEY	0x046b40	/* Plessey Semiconductor */
#define	MII_OUI_QUALSEMI	0x006051	/* Quality Semiconductor */
#define	MII_OUI_REALTEK	0x000020	/* Realtek Semiconductor */
#define	MII_OUI_SEEQ	0x00a07d	/* Seeq */
#define	MII_OUI_SIS	0x00e006	/* Silicon Integrated Systems */
#define	MII_OUI_TI	0x080028	/* Texas Instruments */
#define	MII_OUI_TSC	0x00c039	/* TDK Semiconductor */
#define	MII_OUI_XAQTI	0x00e0ae	/* XaQti Corp. */

/* in the 79c873, AMD uses another OUI (which matches Davicom!) */
#define	MII_OUI_xxALTIMA	0x000895	/* Altima Communications */
#define	MII_OUI_xxAMD	0x00606e	/* Advanced Micro Devices */

#define	MII_OUI_xxINTEL	0x00f800	/* Intel (alt) */

/* some vendors have the bits swapped within bytes
	(ie, ordered as on the wire) */
#define	MII_OUI_xxBROADCOM	0x000818	/* Broadcom Corporation */
#define	MII_OUI_xxICS	0x00057d	/* Integrated Circuit Systems */
#define	MII_OUI_xxSEEQ	0x0005be	/* Seeq */
#define	MII_OUI_xxSIS	0x000760	/* Silicon Integrated Systems */
#define	MII_OUI_xxTI	0x100014	/* Texas Instruments */
#define	MII_OUI_xxXAQTI	0x350700	/* XaQti Corp. */

/* Level 1 is completely different - from right to left.
	(Two bits get lost in the third OUI byte.) */
#define	MII_OUI_xxLEVEL1	0x1e0400	/* Level 1 */
#define	MII_OUI_xxLEVEL1a	0x0004de	/* Level 1 */

/* Don't know what's going on here. */
#define	MII_OUI_xxDAVICOM	0x006040	/* Davicom Semiconductor */

/* This is the OUI of the gigE PHY in the RealTek 8169S/8110S chips */
#define	MII_OUI_xxREALTEK	0x000732	/* Realtek Semiconductor */

/* Contrived vendor for dcphy */
#define	MII_OUI_xxDEC	0x040440	/* Digital Clone */

#define	MII_OUI_xxMARVELL	0x000ac2	/* Marvell Semiconductor */

/*
 * List of known models.  Grouped by oui.
 */

/* Advanced Micro Devices PHYs */
#define	MII_MODEL_xxAMD_79C873	0x0000
#define	MII_STR_xxAMD_79C873	"Am79C873 10/100 PHY"
#define	MII_MODEL_AMD_79C873phy	0x0036
#define	MII_STR_AMD_79C873phy	"Am79C873 internal PHY"
#define	MII_MODEL_AMD_79C875phy	0x0014
#define	MII_STR_AMD_79C875phy	"Am79C875 quad PHY"

/* Altima Communications PHYs */
#define	MII_MODEL_xxALTIMA_AC_UNKNOWN	0x0001
#define	MII_STR_xxALTIMA_AC_UNKNOWN	"AC_UNKNOWN 10/100 PHY"
#define	MII_MODEL_xxALTIMA_AC101	0x0021
#define	MII_STR_xxALTIMA_AC101	"AC101 10/100 PHY"
#define	MII_MODEL_xxALTIMA_AC101L	0x0012
#define	MII_STR_xxALTIMA_AC101L	"AC101L 10/100 PHY"

/* Broadcom Corp. PHYs */
#define	MII_MODEL_xxBROADCOM_BCM5400	0x0004
#define	MII_STR_xxBROADCOM_BCM5400	"BCM5400 1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5401	0x0005
#define	MII_STR_xxBROADCOM_BCM5401	"BCM5401 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5411	0x0007
#define	MII_STR_xxBROADCOM_BCM5411	"BCM5411 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5421S	0x000e
#define	MII_STR_xxBROADCOM_BCM5421S	"BCM5421S 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5701	0x0011
#define	MII_STR_xxBROADCOM_BCM5701	"BCM5701 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5703	0x0016
#define	MII_STR_xxBROADCOM_BCM5703	"BCM5703 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5704	0x0019
#define	MII_STR_xxBROADCOM_BCM5704	"BCM5704 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5705	0x001a
#define	MII_STR_xxBROADCOM_BCM5705	"BCM5705 10/100/1000baseT PHY"
#define	MII_MODEL_xxBROADCOM_BCM5750	0x0018
#define	MII_STR_xxBROADCOM_BCM5750	"BCM5750 10/100/1000baseT PHY"
#define	MII_MODEL_BROADCOM_BCM5400	0x0004
#define	MII_STR_BROADCOM_BCM5400	"BCM5400 1000baseT PHY"
#define	MII_MODEL_BROADCOM_BCM5401	0x0005
#define	MII_STR_BROADCOM_BCM5401	"BCM5401 1000baseT PHY"
#define	MII_MODEL_BROADCOM_BCM5411	0x0007
#define	MII_STR_BROADCOM_BCM5411	"BCM5411 1000baseT PHY"
#define	MII_MODEL_BROADCOM_3C905B	0x0012
#define	MII_STR_BROADCOM_3C905B	"Broadcom 3C905B internal PHY"
#define	MII_MODEL_BROADCOM_3C905C	0x0017
#define	MII_STR_BROADCOM_3C905C	"Broadcom 3C905C internal PHY"
#define	MII_MODEL_BROADCOM_BCM5221	0x001e
#define	MII_STR_BROADCOM_BCM5221	"BCM5221 100baseTX PHY"
#define	MII_MODEL_BROADCOM_BCM5201	0x0021
#define	MII_STR_BROADCOM_BCM5201	"BCM5201 10/100 PHY"
#define	MII_MODEL_BROADCOM_BCM4401_0x0036	BCM4401
#define	MII_STR_BROADCOM_BCM4401_0x0036	"10/100baseTX PHY"

/* Davicom Semiconductor PHYs */
#define	MII_MODEL_xxDAVICOM_DM9101	0x0000
#define	MII_STR_xxDAVICOM_DM9101	"DM9101 10/100 PHY"
#define	MII_MODEL_DAVICOM_DM9102	0x0004
#define	MII_STR_DAVICOM_DM9102	"DM9102 10/100 PHY"
#define	MII_MODEL_DAVICOM_DM9601	0x000c
#define	MII_STR_DAVICOM_DM9601	"DM9601 10/100 PHY"

/* Enable Semiconductor PHYs */
#define	MII_MODEL_ENABLESEMI_88E1000	0x0005
#define	MII_STR_ENABLESEMI_88E1000	"Enable 88E1000"
#define	MII_MODEL_ENABLESEMI_88E1000S	0x0004
#define	MII_STR_ENABLESEMI_88E1000S	"Enable 88E1000S"

/* Marvell Semiconductor PHYs */
#define	MII_MODEL_MARVELL_E1000	0x0000
#define	MII_STR_MARVELL_E1000	"Marvell 88E1000* Gigabit PHY"
#define	MII_MODEL_MARVELL_E1011	0x0002
#define	MII_STR_MARVELL_E1011	"Marvell 88E1011 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_3	0x0003
#define	MII_STR_MARVELL_E1000_3	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_4	0x0004
#define	MII_STR_MARVELL_E1000_4	"Marvell 88E1000S Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_5	0x0005
#define	MII_STR_MARVELL_E1000_5	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_MARVELL_E1000_6	0x0006
#define	MII_STR_MARVELL_E1000_6	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1000_3	0x0003
#define	MII_STR_xxMARVELL_E1000_3	"Marvell 88E1000 Gigabit PHY"
#define	MII_MODEL_xxMARVELL_E1000_5	0x0005
#define	MII_STR_xxMARVELL_E1000_5	"Marvell 88E1000 Gigabit PHY"

/* Contrived vendor/model for dcphy */
#define	MII_MODEL_xxDEC_xxDC	0x0001
#define	MII_STR_xxDEC_xxDC	"DC"

/* Integrated Circuit Systems PHYs */
#define	MII_MODEL_xxICS_1890	0x0002
#define	MII_STR_xxICS_1890	"ICS1890 10/100 PHY"
#define	MII_MODEL_xxICS_1892	0x0003
#define	MII_STR_xxICS_1892	"ICS1892 10/100 PHY"
#define	MII_MODEL_xxICS_1893	0x0004
#define	MII_STR_xxICS_1893	"ICS1893 10/100 PHY"

/* Intel PHYs */
#define	MII_MODEL_xxINTEL_I82553	0x0000
#define	MII_STR_xxINTEL_I82553	"i82553 10/100 PHY"
#define	MII_MODEL_INTEL_I82555	0x0015
#define	MII_STR_INTEL_I82555	"i82555 10/100 PHY"
#define	MII_MODEL_INTEL_I82562EM	0x0032
#define	MII_STR_INTEL_I82562EM	"i82562EM 10/100 PHY"
#define	MII_MODEL_INTEL_I82562ET	0x0033
#define	MII_STR_INTEL_I82562ET	"i82562ET 10/100 PHY"
#define	MII_MODEL_INTEL_I82553	0x0035
#define	MII_STR_INTEL_I82553	"i82553 10/100 PHY"

/* Level 1 PHYs */
#define	MII_MODEL_xxLEVEL1_LXT970	0x0000
#define	MII_STR_xxLEVEL1_LXT970	"LXT970 10/100 PHY"
#define	MII_MODEL_xxLEVEL1a_LXT971	0x000e
#define	MII_STR_xxLEVEL1a_LXT971	"LXT971 10/100 PHY"

/* Myson Technology PHYs */
#define	MII_MODEL_MYSON_MTD972	0x0000
#define	MII_STR_MYSON_MTD972	"MTD972 10/100 PHY"

/* National Semiconductor PHYs */
#define	MII_MODEL_NATSEMI_DP83840	0x0000
#define	MII_STR_NATSEMI_DP83840	"DP83840 10/100 PHY"
#define	MII_MODEL_NATSEMI_DP83843	0x0001
#define	MII_STR_NATSEMI_DP83843	"DP83843 10/100 PHY"
#define	MII_MODEL_NATSEMI_DP83815	0x0002
#define	MII_STR_NATSEMI_DP83815	"DP83815 10/100 PHY"
#define	MII_MODEL_NATSEMI_DP83891	0x0005
#define	MII_STR_NATSEMI_DP83891	"DP83891 10/100/1000 PHY"
#define	MII_MODEL_NATSEMI_DP83861	0x0006
#define	MII_STR_NATSEMI_DP83861	"DP83861 10/100/1000 PHY"

/* Plessey Semiconductor PHYs */
#define	MII_MODEL_PLESSEY_NWK914	0x0000
#define	MII_STR_PLESSEY_NWK914	"NWK914 10/100 PHY"

/* Quality Semiconductor PHYs */
#define	MII_MODEL_QUALSEMI_QS6612	0x0000
#define	MII_STR_QUALSEMI_QS6612	"QS6612 10/100 PHY"

/* Realtek Semiconductor PHYs */
#define	MII_MODEL_REALTEK_RTL8201L	0x0020
#define	MII_STR_REALTEK_RTL8201L	"RTL8201L 10/100 PHY"
#define	MII_MODEL_xxREALTEK_RTL8169S	0x0011
#define	MII_STR_xxREALTEK_RTL8169S	"RTL8169S/8110S PHY"

/* Seeq PHYs */
#define	MII_MODEL_xxSEEQ_80220	0x0003
#define	MII_STR_xxSEEQ_80220	"Seeq 80220 10/100 PHY"
#define	MII_MODEL_xxSEEQ_84220	0x0004
#define	MII_STR_xxSEEQ_84220	"Seeq 84220 10/100 PHY"

/* Silicon Integrated Systems PHYs */
#define	MII_MODEL_xxSIS_900	0x0000
#define	MII_STR_xxSIS_900	"SiS 900 10/100 PHY"

/* Texas Instruments PHYs */
#define	MII_MODEL_xxTI_TLAN10T	0x0001
#define	MII_STR_xxTI_TLAN10T	"ThunderLAN 10baseT PHY"
#define	MII_MODEL_xxTI_100VGPMI	0x0002
#define	MII_STR_xxTI_100VGPMI	"ThunderLAN 100VG-AnyLan PHY"
#define	MII_MODEL_xxTI_TNETE2101	0x0003
#define	MII_STR_xxTI_TNETE2101	"TNETE2101 PHY"

/* TDK Semiconductor PHYs */
#define	MII_MODEL_TSC_78Q2120	0x0014
#define	MII_STR_TSC_78Q2120	"78Q2120 10/100 PHY"
#define	MII_MODEL_TSC_78Q2121	0x0015
#define	MII_STR_TSC_78Q2121	"78Q2121 100baseTX PHY"

/* XaQti Corp. PHYs */
#define	MII_MODEL_XAQTI_XMACII	0x0000
#define	MII_STR_XAQTI_XMACII	"XaQti Corp. XMAC II Gigabit PHY"
