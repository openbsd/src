/*	$OpenBSD: pcmciadevs.h,v 1.1 1998/09/11 10:47:15 fgsch Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *		OpenBSD
 */
/* $NetBSD: pcmciadevs,v 1.13 1998/08/17 23:10:12 thorpej Exp $ */

/*
 * Copyright (c) 1998, Christos Zoulas
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christos Zoulas
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * List of known PCMCIA vendors
 */

#define	PCMCIA_VENDOR_NEWMEDIA	0x0057	/* NewMedia Corporation */
#define	PCMCIA_VENDOR_IBM	0x00a4	/* IBM Corporation */
#define	PCMCIA_VENDOR_3COM	0x0101	/* 3Com Corporation */
#define	PCMCIA_VENDOR_MEGAHERTZ	0x0102	/* Megahertz Corporation */
#define	PCMCIA_VENDOR_SOCKET	0x0104	/* Socket Communications */
#define	PCMCIA_VENDOR_TDK	0x0105	/* TDK Corporation */
#define	PCMCIA_VENDOR_SMC	0x0108	/* Standard Microsystems Corporation */
#define	PCMCIA_VENDOR_MOTOROLA	0x0109	/* Motorola Corporation */
#define	PCMCIA_VENDOR_USROBOTICS	0x0115	/* US Robotics Corporation */
#define	PCMCIA_VENDOR_MEGAHERTZ2	0x0128	/* Megahertz Corporation */
#define	PCMCIA_VENDOR_ADAPTEC	0x012f	/* Adaptec Corporation */
#define	PCMCIA_VENDOR_LINKSYS	0x0149	/* Linksys Corporation */
#define	PCMCIA_VENDOR_SIMPLETECH	0x014d	/* Simple Technology */
#define	PCMCIA_VENDOR_DAYNA	0x0194	/* Dayna Corporation */
#define	PCMCIA_VENDOR_IODATA	0x01bf	/* I-O DATA */

/*
 * List of known products.  Grouped by vendor.
 */
/* Adaptec Products */
#define	PCMCIA_CIS_ADAPTEC_APA1460_1	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_ADAPTEC_APA1460_1	0x0001
#define	PCMCIA_STR_ADAPTEC_APA1460_1	"Adaptec APA-1460/A SCSI Host Adapter"
#define	PCMCIA_CIS_ADAPTEC_APA1460_2	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_ADAPTEC_APA1460_2	0x0002
#define	PCMCIA_STR_ADAPTEC_APA1460_2	"Adaptec APA-1460/B SCSI Host Adapter"

/* 3COM Products */
#define	PCMCIA_CIS_3COM_3C562	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C562	0x0562
#define	PCMCIA_STR_3COM_3C562	"3Com 3c562 33.6 Modem/10Mbps Ethernet"
#define	PCMCIA_CIS_3COM_3C589	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C589	0x0589
#define	PCMCIA_STR_3COM_3C589	"3Com 3c589 10Mbps Ethernet"
#define	PCMCIA_CIS_3COM_3C574	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_3COM_3C574	0x0574
#define	PCMCIA_STR_3COM_3C574	"3Com 3c574-TX 10/100Mbps Ethernet"

/* Dayna Products */
#define	PCMCIA_CIS_DAYNA_COMMUNICARD_E	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_DAYNA_COMMUNICARD_E	0x002d
#define	PCMCIA_STR_DAYNA_COMMUNICARD_E	"Dayna CommuniCard E"

/* Motorola Products */
#define	PCMCIA_CIS_MOTOROLA_POWER144	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MOTOROLA_POWER144	0x0105
#define	PCMCIA_STR_MOTOROLA_POWER144	"Motorola Power 14.4 Modem"
#define	PCMCIA_CIS_MOTOROLA_PM100C	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MOTOROLA_PM100C	0x0302
#define	PCMCIA_STR_MOTOROLA_PM100C	"Motorola Personal Messenger 100C CDPD Modem"
#define	PCMCIA_CIS_MOTOROLA_MONTANA_33.6	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MOTOROLA_MONTANA_33.6	0x0505
#define	PCMCIA_STR_MOTOROLA_MONTANA_33.6	"Motorola Montana 33.6 Fax/Modem"

/* IBM Products */
#define	PCMCIA_CIS_IBM_INFOMOVER	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_INFOMOVER	0x0002
#define	PCMCIA_STR_IBM_INFOMOVER	"National Semiconductor InfoMover"
#define	PCMCIA_CIS_IBM_HOME_AND_AWAY	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IBM_HOME_AND_AWAY	0x002e
#define	PCMCIA_STR_IBM_HOME_AND_AWAY	"IBM Home and Away Modem"

/* I-O DATA */
#define	PCMCIA_CIS_IODATA_PCLAT	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_IODATA_PCLAT	0x2216
#define	PCMCIA_STR_IODATA_PCLAT	"I-O DATA PCLA/T"

/* Linksys corporation */
#define	PCMCIA_CIS_LINKSYS_ECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_ECARD	0x0265
#define	PCMCIA_STR_LINKSYS_ECARD	"Linksys EthernetCard"
#define	PCMCIA_CIS_LINKSYS_COMBO_ECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_COMBO_ECARD	0xc1ab
#define	PCMCIA_STR_LINKSYS_COMBO_ECARD	"Linksys Combo EthernetCard"
#define	PCMCIA_CIS_LINKSYS_TRUST_COMBO_ECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_LINKSYS_TRUST_COMBO_ECARD	0x021b
#define	PCMCIA_STR_LINKSYS_TRUST_COMBO_ECARD	"Trust (Linksys) Combo EthernetCard"

/* Megahertz Products */
#define	PCMCIA_CIS_MEGAHERTZ_XJ4288	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJ4288	0x0023
#define	PCMCIA_STR_MEGAHERTZ_XJ4288	"Megahertz XJ4288 Modem"
#define	PCMCIA_CIS_MEGAHERTZ2_XJEM1144	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ2_XJEM1144	0x0101
#define	PCMCIA_STR_MEGAHERTZ2_XJEM1144	"Megahertz XJEM1144 Ethernet"
#define	PCMCIA_CIS_MEGAHERTZ2_XJACK	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ2_XJACK	0x0103
#define	PCMCIA_STR_MEGAHERTZ2_XJACK	"Megahertz X-JACK Ethernet"

/* US Robotics Products */
#define	PCMCIA_CIS_USROBOTICS_WORLDPORT144	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_USROBOTICS_WORLDPORT144	0x3330
#define	PCMCIA_STR_USROBOTICS_WORLDPORT144	"US Robotics WorldPort 14.4 Modem"

/* Simple Technology Products */
#define	PCMCIA_CIS_SIMPLETECH_COMMUNICATOR288	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SIMPLETECH_COMMUNICATOR288	0x0100
#define	PCMCIA_STR_SIMPLETECH_COMMUNICATOR288	"Simple Technology 28.8 Communicator"

/* Socket Communications Products */
#define	PCMCIA_CIS_SOCKET_PAGECARD	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SOCKET_PAGECARD	0x0003
#define	PCMCIA_STR_SOCKET_PAGECARD	"Socket Communications PageCard"
#define	PCMCIA_CIS_SOCKET_DUAL_RS232	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SOCKET_DUAL_RS232	0x0012
#define	PCMCIA_STR_SOCKET_DUAL_RS232	"Socket Communications Dual RS232"

/* TDK Products */
#define	PCMCIA_CIS_TDK_LAK_CD021BX	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_LAK_CD021BX	0x0200
#define	PCMCIA_STR_TDK_LAK_CD021BX	"TDK LAK-CD021BX Ethernet"
#define	PCMCIA_CIS_TDK_DFL9610	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_TDK_DFL9610	0x0d0a
#define	PCMCIA_STR_TDK_DFL9610	"TDK DFL9610 Ethernet & Digital Cellular"

/* NewMedia Products */
#define	PCMCIA_CIS_NEWMEDIA_BASICS	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_NEWMEDIA_BASICS	0x0019
#define	PCMCIA_STR_NEWMEDIA_BASICS	"NewMedia BASICS Ethernet"

/* Standard Microsystems Corporation Products */
#define	PCMCIA_CIS_SMC_8016	{ NULL, NULL, NULL, NULL }
#define	PCMCIA_PRODUCT_SMC_8016	0x0105
#define	PCMCIA_STR_SMC_8016	"SMC 8016 EtherCard"

/* Cards we know only by their cis */
#define	PCMCIA_VENDOR_PREMAX	-1	/* Premax */
#define	PCMCIA_VENDOR_PLANET	-1	/* Planet */
#define	PCMCIA_VENDOR_DLINK	-1	/* D-Link */
#define	PCMCIA_VENDOR_RPTI	-1	/* RPTI */
#define	PCMCIA_VENDOR_ACCTON	-1	/* ACCTON */
#define	PCMCIA_VENDOR_YEDATA	-1	/* Y-E DATA */
#define	PCMCIA_VENDOR_DIGITAL	-1	/* Digital Equipment Corporation */

#define	PCMCIA_CIS_MEGAHERTZ_XJ2288	{ "MEGAHERTZ", "MODEM XJ2288", NULL, NULL }
#define	PCMCIA_PRODUCT_MEGAHERTZ_XJ2288	-1
#define	PCMCIA_STR_MEGAHERTZ_XJ2288	"Megahertz XJ2288 Modem"
#define	PCMCIA_CIS_PREMAX_PE200	{ "PMX   ", "PE-200", NULL, NULL }
#define	PCMCIA_PRODUCT_PREMAX_PE200	-1
#define	PCMCIA_STR_PREMAX_PE200	"PreMax PE-200"
#define	PCMCIA_CIS_PLANET_SMARTCOM2000	{ "PCMCIA", "UE2212", NULL, NULL }
#define	PCMCIA_PRODUCT_PLANET_SMARTCOM2000	-1
#define	PCMCIA_STR_PLANET_SMARTCOM2000	"Planet SmartCOM 2000"
#define	PCMCIA_CIS_DLINK_DE650	{ "D-Link", "DE-650", NULL, NULL }
#define	PCMCIA_PRODUCT_DLINK_DE650	-1
#define	PCMCIA_STR_DLINK_DE650	"D-Link DE-650"
#define	PCMCIA_CIS_DLINK_DE660	{ "D-Link", "DE-660", NULL, NULL }
#define	PCMCIA_PRODUCT_DLINK_DE660	-1
#define	PCMCIA_STR_DLINK_DE660	"D-Link DE-660"
#define	PCMCIA_CIS_RPTI_EP401	{ "RPTI", "EP401 Ethernet NE2000 Compatible", NULL, NULL }
#define	PCMCIA_PRODUCT_RPTI_EP401	-1
#define	PCMCIA_STR_RPTI_EP401	"RPTI EP401"
#define	PCMCIA_CIS_ACCTON_EN2212	{ "ACCTON", "EN2212", NULL, NULL }
#define	PCMCIA_PRODUCT_ACCTON_EN2212	-1
#define	PCMCIA_STR_ACCTON_EN2212	"Accton EN2212"
#define	PCMCIA_CIS_YEDATA_EXTERNAL_FDD	{ "Y-E DATA", "External FDD", NULL, NULL }
#define	PCMCIA_PRODUCT_YEDATA_EXTERNAL_FDD	-1
#define	PCMCIA_STR_YEDATA_EXTERNAL_FDD	"Y-E DATA External FDD"
#define	PCMCIA_CIS_DIGITAL_DEPCMXX	{ "DIGITAL", "DEPCM-XX", NULL, NULL }
#define	PCMCIA_PRODUCT_DIGITAL_DEPCMXX	-1
#define	PCMCIA_STR_DIGITAL_DEPCMXX	"DEC DEPCM-BA"
