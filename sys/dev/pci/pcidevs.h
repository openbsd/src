/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: pcidevs,v 1.1537 2010/03/23 23:42:47 deraadt Exp 
 */
/*	$NetBSD: pcidevs,v 1.30 1997/06/24 06:20:24 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
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
 * NOTE: a fairly complete list of PCI codes can be found at:
 *
 *	http://www.pcidatabase.com/
 *
 * There is a Vendor ID search engine available at:
 *
 *	http://www.pcisig.com/membership/vid_search/
 */

/*
 * List of known PCI vendors
 */

#define	PCI_VENDOR_MARTINMARIETTA	0x003d		/* Martin-Marietta */
#define	PCI_VENDOR_HAUPPAUGE	0x0070		/* Hauppauge */
#define	PCI_VENDOR_TTTECH	0x0357		/* TTTech */
#define	PCI_VENDOR_DYNALINK	0x0675		/* Dynalink */
#define	PCI_VENDOR_RHINO	0x0b0b		/* Rhino Equipment */
#define	PCI_VENDOR_COMPAQ	0x0e11		/* Compaq */
#define	PCI_VENDOR_SYMBIOS	0x1000		/* Symbios Logic */
#define	PCI_VENDOR_ATI	0x1002		/* ATI */
#define	PCI_VENDOR_ULSI	0x1003		/* ULSI Systems */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI */
#define	PCI_VENDOR_AVANCE	0x1005		/* Avance Logic */
#define	PCI_VENDOR_NS	0x100b		/* NS */
#define	PCI_VENDOR_TSENG	0x100c		/* Tseng Labs */
#define	PCI_VENDOR_WEITEK	0x100e		/* Weitek */
#define	PCI_VENDOR_DEC	0x1011		/* DEC */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_WD	0x101c		/* Western Digital */
#define	PCI_VENDOR_AMI	0x101e		/* AMI */
#define	PCI_VENDOR_AMD	0x1022		/* AMD */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident */
#define	PCI_VENDOR_ACER	0x1025		/* Acer */
#define	PCI_VENDOR_DELL	0x1028		/* Dell */
#define	PCI_VENDOR_SNI	0x1029		/* Siemens Nixdorf AG */
#define	PCI_VENDOR_MATROX	0x102b		/* Matrox */
#define	PCI_VENDOR_CHIPS	0x102c		/* Chips and Technologies */
#define	PCI_VENDOR_TOSHIBA	0x102f		/* Toshiba */
#define	PCI_VENDOR_MIRO	0x1031		/* Miro Computer Products AG */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_FUTUREDOMAIN	0x1036		/* Future Domain */
#define	PCI_VENDOR_SIS	0x1039		/* SiS */
#define	PCI_VENDOR_HP	0x103c		/* Hewlett-Packard */
#define	PCI_VENDOR_PCTECH	0x1042		/* PC Technology */
#define	PCI_VENDOR_ASUSTEK	0x1043		/* Asustek */
#define	PCI_VENDOR_DPT	0x1044		/* DPT */
#define	PCI_VENDOR_OPTI	0x1045		/* Opti */
#define	PCI_VENDOR_ELSA	0x1048		/* Elsa */
#define	PCI_VENDOR_SGSTHOMSON	0x104a		/* SGS Thomson */
#define	PCI_VENDOR_BUSLOGIC	0x104b		/* BusLogic */
#define	PCI_VENDOR_TI	0x104c		/* TI */
#define	PCI_VENDOR_SONY	0x104d		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond */
#define	PCI_VENDOR_HITACHI	0x1054		/* Hitachi */
#define	PCI_VENDOR_SMSC	0x1055		/* SMSC */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise */
#define	PCI_VENDOR_FOXCONN	0x105b		/* Foxconn */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 */
#define	PCI_VENDOR_UMC	0x1060		/* UMC */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. */
#define	PCI_VENDOR_PICOPOWER	0x1066		/* Picopower */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_APPLE	0x106b		/* Apple */
#define	PCI_VENDOR_MITAC	0x1071		/* Mitac */
#define	PCI_VENDOR_YAMAHA	0x1073		/* Yamaha */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen Microsystems */
#define	PCI_VENDOR_QLOGIC	0x1077		/* QLogic */
#define	PCI_VENDOR_CYRIX	0x1078		/* Cyrix */
#define	PCI_VENDOR_LEADTEK	0x107d		/* LeadTek Research */
#define	PCI_VENDOR_INTERPHASE	0x107e		/* Interphase */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq Microsystems */
#define	PCI_VENDOR_BIT3	0x108a		/* Bit3 */
#define	PCI_VENDOR_OLICOM	0x108d		/* Olicom */
#define	PCI_VENDOR_SUN	0x108e		/* Sun */
#define	PCI_VENDOR_INTERGRAPH	0x1091		/* Intergraph */
#define	PCI_VENDOR_DIAMOND	0x1092		/* Diamond Multimedia */
#define	PCI_VENDOR_NATINST	0x1093		/* National Instruments */
#define	PCI_VENDOR_CMDTECH	0x1095		/* CMD Technology */
#define	PCI_VENDOR_QUANTUMDESIGNS	0x1098		/* Quantum Designs */
#define	PCI_VENDOR_BROOKTREE	0x109e		/* Brooktree */
#define	PCI_VENDOR_SGI	0x10a9		/* SGI */
#define	PCI_VENDOR_ACC	0x10aa		/* ACC Microelectronics */
#define	PCI_VENDOR_SYMPHONY	0x10ad		/* Symphony Labs */
#define	PCI_VENDOR_STB	0x10b4		/* STB Systems */
#define	PCI_VENDOR_PLX	0x10b5		/* PLX */
#define	PCI_VENDOR_MADGE	0x10b6		/* Madge Networks */
#define	PCI_VENDOR_3COM	0x10b7		/* 3Com */
#define	PCI_VENDOR_SMC	0x10b8		/* SMC */
#define	PCI_VENDOR_ALI	0x10b9		/* Acer Labs */
#define	PCI_VENDOR_MITSUBISHIELEC	0x10ba		/* Mitsubishi Electronics */
#define	PCI_VENDOR_SURECOM	0x10bd		/* Surecom */
#define	PCI_VENDOR_NEOMAGIC	0x10c8		/* Neomagic */
#define	PCI_VENDOR_MENTOR	0x10cc		/* Mentor ARC */
#define	PCI_VENDOR_ADVSYS	0x10cd		/* Advansys */
#define	PCI_VENDOR_FUJITSU	0x10cf		/* Fujitsu */
#define	PCI_VENDOR_MOLEX	0x10d2		/* Molex */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* NVIDIA */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_TEKRAM	0x10e1		/* Tekram (1st ID) */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
#define	PCI_VENDOR_TVIA	0x10ea		/* Tvia */
#define	PCI_VENDOR_REALTEK	0x10ec		/* Realtek */
#define	PCI_VENDOR_NKK	0x10f5		/* NKK */
#define	PCI_VENDOR_IODATA	0x10fc		/* IO Data Device */
#define	PCI_VENDOR_INITIO	0x1101		/* Initio */
#define	PCI_VENDOR_CREATIVELABS	0x1102		/* Creative Labs */
#define	PCI_VENDOR_TRIONES	0x1103		/* HighPoint */
#define	PCI_VENDOR_SIGMA	0x1105		/* Sigma Designs */
#define	PCI_VENDOR_VIATECH	0x1106		/* VIA */
#define	PCI_VENDOR_COGENT	0x1109		/* Cogent Data */
#define	PCI_VENDOR_SIEMENS	0x110a		/* Siemens */
#define	PCI_VENDOR_ZNYX	0x110d		/* Znyx Networks */
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_PHILIPS	0x1131		/* Philips */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_CYCLONE	0x113c		/* Cyclone */
#define	PCI_VENDOR_EQUINOX	0x113f		/* Equinox */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_DIGI	0x114f		/* Digi */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_RENDITION	0x1163		/* Rendition */
#define	PCI_VENDOR_RCC	0x1166		/* ServerWorks */
#define	PCI_VENDOR_ALTERA	0x1172		/* Altera */
#define	PCI_VENDOR_TOSHIBA2	0x1179		/* Toshiba */
#define	PCI_VENDOR_RICOH	0x1180		/* Ricoh */
#define	PCI_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	PCI_VENDOR_COROLLARY	0x118c		/* Corollary */
#define	PCI_VENDOR_ACARD	0x1191		/* Acard */
#define	PCI_VENDOR_ZEINET	0x1193		/* Zeinet */
#define	PCI_VENDOR_OMEGA	0x119b		/* Omega Micro */
#define	PCI_VENDOR_MARVELL	0x11ab		/* Marvell */
#define	PCI_VENDOR_LITEON	0x11ad		/* Lite-On */
#define	PCI_VENDOR_V3	0x11b0		/* V3 Semiconductor */
#define	PCI_VENDOR_PINNACLE	0x11bd		/* Pinnacle Systems */
#define	PCI_VENDOR_LUCENT	0x11c1		/* AT&T/Lucent */
#define	PCI_VENDOR_DOLPHIN	0x11c8		/* Dolphin */
#define	PCI_VENDOR_MRTMAGMA	0x11c9		/* Mesa Ridge (MAGMA) */
#define	PCI_VENDOR_AD	0x11d4		/* Analog Devices */
#define	PCI_VENDOR_ZORAN	0x11de		/* Zoran */
#define	PCI_VENDOR_PIJNENBURG	0x11e3		/* Pijnenburg */
#define	PCI_VENDOR_COMPEX	0x11f6		/* Compex */
#define	PCI_VENDOR_CYCLADES	0x120e		/* Cyclades */
#define	PCI_VENDOR_ESSENTIAL	0x120f		/* Essential Communications */
#define	PCI_VENDOR_O2MICRO	0x1217		/* O2 Micro */
#define	PCI_VENDOR_3DFX	0x121a		/* 3DFX Interactive */
#define	PCI_VENDOR_ATML	0x121b		/* ATML */
#define	PCI_VENDOR_CCUBE	0x123f		/* C-Cube */
#define	PCI_VENDOR_AVM	0x1244		/* AVM */
#define	PCI_VENDOR_STALLION	0x124d		/* Stallion Technologies */
#define	PCI_VENDOR_COREGA	0x1259		/* Corega */
#define	PCI_VENDOR_ASIX	0x125b		/* ASIX */
#define	PCI_VENDOR_ESSTECH	0x125d		/* ESS */
#define	PCI_VENDOR_INTERSIL	0x1260		/* Intersil */
#define	PCI_VENDOR_NORTEL	0x126c		/* Nortel Networks */
#define	PCI_VENDOR_SMI	0x126f		/* Silicon Motion */
#define	PCI_VENDOR_ENSONIQ	0x1274		/* Ensoniq */
#define	PCI_VENDOR_TRANSMETA	0x1279		/* Transmeta */
#define	PCI_VENDOR_ROCKWELL	0x127a		/* Rockwell */
#define	PCI_VENDOR_DAVICOM	0x1282		/* Davicom */
#define	PCI_VENDOR_ITEXPRESS	0x1283		/* ITExpress */
#define	PCI_VENDOR_PLATFORM	0x1285		/* Platform */
#define	PCI_VENDOR_LUXSONOR	0x1287		/* LuxSonor */
#define	PCI_VENDOR_TRITECH	0x1292		/* TriTech Microelectronics */
#define	PCI_VENDOR_ALTEON	0x12ae		/* Alteon */
#define	PCI_VENDOR_USR	0x12b9		/* US Robotics */
#define	PCI_VENDOR_STB2	0x12d2		/* NVIDIA/SGS-Thomson */
#define	PCI_VENDOR_PERICOM	0x12d8		/* Pericom */
#define	PCI_VENDOR_AUREAL	0x12eb		/* Aureal */
#define	PCI_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	PCI_VENDOR_PE	0x1318		/* Packet Engines */
#define	PCI_VENDOR_FORTEMEDIA	0x1319		/* Forte Media */
#define	PCI_VENDOR_SIIG	0x131f		/* SIIG */
#define	PCI_VENDOR_DTCTECH	0x134a		/* DTC Tech */
#define	PCI_VENDOR_PCTEL	0x134d		/* PCTEL */
#define	PCI_VENDOR_MEINBERG	0x1360		/* Meinberg Funkuhren */
#define	PCI_VENDOR_CNET	0x1371		/* CNet */
#define	PCI_VENDOR_SILICOM	0x1374		/* Silicom */
#define	PCI_VENDOR_LMC	0x1376		/* LAN Media */
#define	PCI_VENDOR_NETGEAR	0x1385		/* Netgear */
#define	PCI_VENDOR_MOXA	0x1393		/* Moxa */
#define	PCI_VENDOR_LEVEL1	0x1394		/* Level 1 */
#define	PCI_VENDOR_HIFN	0x13a3		/* Hifn */
#define	PCI_VENDOR_EXAR	0x13a8		/* Exar */
#define	PCI_VENDOR_3WARE	0x13c1		/* 3ware */
#define	PCI_VENDOR_TECHSAN	0x13d0		/* Techsan Electronics */
#define	PCI_VENDOR_ABOCOM	0x13d1		/* Abocom */
#define	PCI_VENDOR_SUNDANCE	0x13f0		/* Sundance */
#define	PCI_VENDOR_CMI	0x13f6		/* C-Media Electronics */
#define	PCI_VENDOR_LAVA	0x1407		/* Lava */
#define	PCI_VENDOR_SUNIX	0x1409		/* Sunix */
#define	PCI_VENDOR_ICENSEMBLE	0x1412		/* IC Ensemble */
#define	PCI_VENDOR_MICROSOFT	0x1414		/* Microsoft */
#define	PCI_VENDOR_OXFORD2	0x1415		/* Oxford */
#define	PCI_VENDOR_CHELSIO	0x1425		/* Chelsio */
#define	PCI_VENDOR_EDIMAX	0x1432		/* Edimax */
#define	PCI_VENDOR_TAMARACK	0x143d		/* Tamarack */
#define	PCI_VENDOR_ASKEY	0x144f		/* Askey */
#define	PCI_VENDOR_AVERMEDIA	0x1461		/* Avermedia */
#define	PCI_VENDOR_MSI	0x1462		/* Micro Star International */
#define	PCI_VENDOR_AIRONET	0x14b9		/* Aironet */
#define	PCI_VENDOR_GLOBESPAN	0x14bc		/* Globespan */
#define	PCI_VENDOR_MYRICOM	0x14c1		/* Myricom */
#define	PCI_VENDOR_OXFORD	0x14d2		/* VScom */
#define	PCI_VENDOR_AVLAB	0x14db		/* Avlab */
#define	PCI_VENDOR_INVERTEX	0x14e1		/* Invertex */
#define	PCI_VENDOR_BROADCOM	0x14e4		/* Broadcom */
#define	PCI_VENDOR_PLANEX	0x14ea		/* Planex */
#define	PCI_VENDOR_CONEXANT	0x14f1		/* Conexant */
#define	PCI_VENDOR_DELTA	0x1500		/* Delta */
#define	PCI_VENDOR_MYSON	0x1516		/* Myson Century */
#define	PCI_VENDOR_TOPIC	0x151f		/* Topic/SmartLink */
#define	PCI_VENDOR_ENE	0x1524		/* ENE */
#define	PCI_VENDOR_ARALION	0x1538		/* Aralion */
#define	PCI_VENDOR_TERRATEC	0x153b		/* TerraTec */
#define	PCI_VENDOR_SYMBOL	0x1562		/* Symbol */
#define	PCI_VENDOR_SYBA	0x1592		/* Syba */
#define	PCI_VENDOR_BLUESTEEL	0x15ab		/* Bluesteel */
#define	PCI_VENDOR_VMWARE	0x15ad		/* VMware */
#define	PCI_VENDOR_ZOLTRIX	0x15b0		/* Zoltrix */
#define	PCI_VENDOR_MELLANOX	0x15b3		/* Mellanox */
#define	PCI_VENDOR_AGILENT	0x15bc		/* Agilent */
#define	PCI_VENDOR_QUICKNET	0x15e2		/* Quicknet Technologies */
#define	PCI_VENDOR_NDC	0x15e8		/* National Datacomm */
#define	PCI_VENDOR_PDC	0x15e9		/* Pacific Data */
#define	PCI_VENDOR_EUMITCOM	0x1638		/* Eumitcom */
#define	PCI_VENDOR_NETSEC	0x1660		/* NetSec */
#define	PCI_VENDOR_ZYDAS	0x167b		/* ZyDAS Technology */
#define	PCI_VENDOR_SAMSUNG	0x167d		/* Samsung */
#define	PCI_VENDOR_ATHEROS	0x168c		/* Atheros */
#define	PCI_VENDOR_GLOBALSUN	0x16ab		/* Global Sun */
#define	PCI_VENDOR_SAFENET	0x16ae		/* SafeNet */
#define	PCI_VENDOR_MICREL	0x16c6		/* Micrel */
#define	PCI_VENDOR_USR2	0x16ec		/* US Robotics */
#define	PCI_VENDOR_VITESSE	0x1725		/* Vitesse */
#define	PCI_VENDOR_LINKSYS	0x1737		/* Linksys */
#define	PCI_VENDOR_NETOCTAVE	0x170b		/* Netoctave */
#define	PCI_VENDOR_ALTIMA	0x173b		/* Altima */
#define	PCI_VENDOR_ANTARES	0x1754		/* Antares Microsystems */
#define	PCI_VENDOR_CAVIUM	0x177d		/* Cavium */
#define	PCI_VENDOR_BELKIN2	0x1799		/* Belkin */
#define	PCI_VENDOR_HAWKING	0x17b3		/* Hawking Technology */
#define	PCI_VENDOR_NETCHIP	0x17cc		/* NetChip Technology */
#define	PCI_VENDOR_I4	0x17cf		/* I4 */
#define	PCI_VENDOR_ARECA	0x17d3		/* Areca */
#define	PCI_VENDOR_NETERION	0x17d5		/* Neterion */
#define	PCI_VENDOR_RDC	0x17f3		/* RDC */
#define	PCI_VENDOR_INPROCOMM	0x17fe		/* INPROCOMM */
#define	PCI_VENDOR_LANERGY	0x1812		/* Lanergy */
#define	PCI_VENDOR_RALINK	0x1814		/* Ralink */
#define	PCI_VENDOR_XGI	0x18ca		/* XGI Technology */
#define	PCI_VENDOR_SILAN	0x1904		/* Silan */
#define	PCI_VENDOR_SANGOMA	0x1923		/* Sangoma */
#define	PCI_VENDOR_SOLARFLARE	0x1924		/* Solarflare */
#define	PCI_VENDOR_OPTION	0x1931		/* Option */
#define	PCI_VENDOR_FREESCALE	0x1957		/* Freescale */
#define	PCI_VENDOR_ATTANSIC	0x1969		/* Attansic Technology */
#define	PCI_VENDOR_AGEIA	0x1971		/* Ageia */
#define	PCI_VENDOR_JMICRON	0x197b		/* JMicron */
#define	PCI_VENDOR_PHISON	0x1987		/* Phison */
#define	PCI_VENDOR_ASPEED	0x1a03		/* ASPEED Technology */
#define	PCI_VENDOR_AWT	0x1a3b		/* AWT */
#define	PCI_VENDOR_QUMRANET	0x1af4		/* Qumranet */
#define	PCI_VENDOR_SYMPHONY2	0x1c1c		/* Symphony Labs */
#define	PCI_VENDOR_TEKRAM2	0x1de1		/* Tekram */
#define	PCI_VENDOR_TEHUTI	0x1fc9		/* Tehuti Networks */
#define	PCI_VENDOR_HINT	0x3388		/* Hint */
#define	PCI_VENDOR_3DLABS	0x3d3d		/* 3D Labs */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic */
#define	PCI_VENDOR_ADDTRON	0x4033		/* Addtron */
#define	PCI_VENDOR_NETXEN	0x4040		/* NetXen */
#define	PCI_VENDOR_INDCOMPSRC	0x494f		/* Industrial Computer Source */
#define	PCI_VENDOR_NETVIN	0x4a14		/* NetVin */
#define	PCI_VENDOR_GEMTEK	0x5046		/* Gemtek */
#define	PCI_VENDOR_TURTLEBEACH	0x5053		/* Turtle Beach */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_XENSOURCE	0x5853		/* XenSource */
#define	PCI_VENDOR_C4T	0x6374		/* c't Magazin */
#define	PCI_VENDOR_DCI	0x6666		/* Decision Computer */
#define	PCI_VENDOR_QUANCOM	0x8008		/* Quancom Informationssysteme */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_INNOTEK	0x80ee		/* InnoTek */
#define	PCI_VENDOR_SIGMATEL	0x8384		/* Sigmatel */
#define	PCI_VENDOR_WINBOND2	0x8c4a		/* Winbond */
#define	PCI_VENDOR_KTI	0x8e2e		/* KTI */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ADP2	0x9005		/* Adaptec */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_NETMOS	0x9710		/* NetMos */
#define	PCI_VENDOR_PARALLELS	0xaaaa		/* Parallels */
#define	PCI_VENDOR_3COM2	0xa727		/* 3Com */
#define	PCI_VENDOR_TIGERJET	0xe159		/* TigerJet Network */
#define	PCI_VENDOR_ENDACE	0xeace		/* Endace */
#define	PCI_VENDOR_BELKIN	0xec80		/* Belkin Components */
#define	PCI_VENDOR_ARC	0xedd8		/* ARC Logic */
#define	PCI_VENDOR_INVALID	0xffff		/* INVALID VENDOR ID */

/*
 * List of known products.  Grouped by vendor.
 */

/* O2 Micro */
#define	PCI_PRODUCT_O2MICRO_FIREWIRE	0x00f7		/* Firewire */
#define	PCI_PRODUCT_O2MICRO_OZ6729	0x6729		/* OZ6729 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6730	0x673a		/* OZ6730 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6922	0x6825		/* OZ6922 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6832	0x6832		/* OZ6832 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6836	0x6836		/* OZ6836/OZ6860 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6872	0x6872		/* OZ68[17]2 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6933	0x6933		/* OZ6933 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6972	0x6972		/* OZ69[17]2 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7110	0x7110		/* OZ711Mx Misc */
#define	PCI_PRODUCT_O2MICRO_OZ7113	0x7113		/* OZ711EC1 SmartCardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7114	0x7114		/* OZ711M1 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7120	0x7120		/* OZ711MP1 SDHC */
#define	PCI_PRODUCT_O2MICRO_OZ7130	0x7130		/* OZ711MP1 XDHC */
#define	PCI_PRODUCT_O2MICRO_OZ7134	0x7134		/* OZ711MP1 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7135	0x7135		/* OZ711EZ1 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7136	0x7136		/* OZ711SP1 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7223	0x7223		/* OZ711E0 CardBus */

/* 3Com Products */
#define	PCI_PRODUCT_3COM_3C985	0x0001		/* 3c985 */
#define	PCI_PRODUCT_3COM_3C996	0x0003		/* 3c996 */
#define	PCI_PRODUCT_3COM_3CRDAG675	0x0013		/* 3CRDAG675 (Atheros AR5212) */
#define	PCI_PRODUCT_3COM2_3CRPAG175	0x0013		/* 3CRPAG175 (Atheros AR5212) */
#define	PCI_PRODUCT_3COM_3C_MPCI_MODEM	0x1007		/* V.90 Modem */
#define	PCI_PRODUCT_3COM_3C940	0x1700		/* 3c940 */
#define	PCI_PRODUCT_3COM_3C339	0x3390		/* 3c339 */
#define	PCI_PRODUCT_3COM_3C359	0x3590		/* 3c359 */
#define	PCI_PRODUCT_3COM_3C450	0x4500		/* 3c450 */
#define	PCI_PRODUCT_3COM_3C555	0x5055		/* 3c555 100Base-TX */
#define	PCI_PRODUCT_3COM_3C575	0x5057		/* 3c575 */
#define	PCI_PRODUCT_3COM_3CCFE575BT	0x5157		/* 3CCFE575BT */
#define	PCI_PRODUCT_3COM_3CCFE575CT	0x5257		/* 3CCFE575CT */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 10Mbps */
#define	PCI_PRODUCT_3COM_3C595TX	0x5950		/* 3c595 100Base-TX */
#define	PCI_PRODUCT_3COM_3C595T4	0x5951		/* 3c595 100Base-T4 */
#define	PCI_PRODUCT_3COM_3C595MII	0x5952		/* 3c595 10Mbps-MII */
#define	PCI_PRODUCT_3COM_3CRSHPW796	0x6000		/* 3CRSHPW796 802.11b */
#define	PCI_PRODUCT_3COM_3CRWE154G72	0x6001		/* 3CRWE154G72 802.11g */
#define	PCI_PRODUCT_3COM_3C556	0x6055		/* 3c556 100Base-TX */
#define	PCI_PRODUCT_3COM_3C556B	0x6056		/* 3c556B 100Base-TX */
#define	PCI_PRODUCT_3COM_3CCFEM656	0x6560		/* 3CCFEM656 */
#define	PCI_PRODUCT_3COM_3CCFEM656B	0x6562		/* 3CCFEM656B */
#define	PCI_PRODUCT_3COM_MODEM56	0x6563		/* 56k Modem */
#define	PCI_PRODUCT_3COM_3CCFEM656C	0x6564		/* 3CCFEM656C */
#define	PCI_PRODUCT_3COM_GLOBALMODEM56	0x6565		/* 56k Global Modem */
#define	PCI_PRODUCT_3COM_3CSOHO100TX	0x7646		/* 3cSOHO-TX */
#define	PCI_PRODUCT_3COM_3CRWE777A	0x7770		/* 3crwe777a AirConnect */
#define	PCI_PRODUCT_3COM_3C940B	0x80eb		/* 3c940B */
#define	PCI_PRODUCT_3COM_3C900TPO	0x9000		/* 3c900 10Base-T */
#define	PCI_PRODUCT_3COM_3C900COMBO	0x9001		/* 3c900 10Mbps-Combo */
#define	PCI_PRODUCT_3COM_3C900B	0x9004		/* 3c900B 10Mbps */
#define	PCI_PRODUCT_3COM_3C900BCOMBO	0x9005		/* 3c900B 10Mbps-Combo */
#define	PCI_PRODUCT_3COM_3C900BTPC	0x9006		/* 3c900B 10Mbps-TPC */
#define	PCI_PRODUCT_3COM_3C900BFL	0x900a		/* 3c900B 10Mbps-FL */
#define	PCI_PRODUCT_3COM_3C905TX	0x9050		/* 3c905 100Base-TX */
#define	PCI_PRODUCT_3COM_3C905T4	0x9051		/* 3c905 100Base-T4 */
#define	PCI_PRODUCT_3COM_3C905BTX	0x9055		/* 3c905B 100Base-TX */
#define	PCI_PRODUCT_3COM_3C905BT4	0x9056		/* 3c905B 100Base-T4 */
#define	PCI_PRODUCT_3COM_3C905BCOMBO	0x9058		/* 3c905B 10/100Mbps-Combo */
#define	PCI_PRODUCT_3COM_3C905BFX	0x905a		/* 3c905B 100Base-FX */
#define	PCI_PRODUCT_3COM_3C905CTX	0x9200		/* 3c905C 100Base-TX */
#define	PCI_PRODUCT_3COM_3C9201	0x9201		/* 3c9201 100Base-TX */
#define	PCI_PRODUCT_3COM_3C920BEMBW	0x9202		/* 3c920B-EMB-WNM */
#define	PCI_PRODUCT_3COM_3CSHO100BTX	0x9300		/* 3cSOHO 100B-TX */
#define	PCI_PRODUCT_3COM_3C980TX	0x9800		/* 3c980 100Base-TX */
#define	PCI_PRODUCT_3COM_3C980CTX	0x9805		/* 3c980C 100Base-TX */
#define	PCI_PRODUCT_3COM_3CR990	0x9900		/* 3cr990 */
#define	PCI_PRODUCT_3COM_3CR990TX	0x9901		/* 3cr990-TX */
#define	PCI_PRODUCT_3COM_3CR990TX95	0x9902		/* 3cr990-TX-95 */
#define	PCI_PRODUCT_3COM_3CR990TX97	0x9903		/* 3cr990-TX-97 */
#define	PCI_PRODUCT_3COM_3C990BTXM	0x9904		/* 3c990b-TX-M */
#define	PCI_PRODUCT_3COM_3CR990FX	0x9905		/* 3cr990-FX */
#define	PCI_PRODUCT_3COM_3CR990SVR95	0x9908		/* 3cr990SVR95 */
#define	PCI_PRODUCT_3COM_3CR990SVR97	0x9909		/* 3cr990SVR97 */
#define	PCI_PRODUCT_3COM_3C990BSVR	0x990a		/* 3c990BSVR */

/* 3DFX Interactive */
#define	PCI_PRODUCT_3DFX_VOODOO	0x0001		/* Voodoo */
#define	PCI_PRODUCT_3DFX_VOODOO2	0x0002		/* Voodoo2 */
#define	PCI_PRODUCT_3DFX_BANSHEE	0x0003		/* Banshee */
#define	PCI_PRODUCT_3DFX_VOODOO32000	0x0004		/* Voodoo3 */
#define	PCI_PRODUCT_3DFX_VOODOO3	0x0005		/* Voodoo3 */
#define	PCI_PRODUCT_3DFX_VOODOO4	0x0007		/* Voodoo4 */
#define	PCI_PRODUCT_3DFX_VOODOO5	0x0009		/* Voodoo5 */
#define	PCI_PRODUCT_3DFX_VOODOO44200	0x000b		/* Voodoo4 */


/* 3D Labs products */
#define	PCI_PRODUCT_3DLABS_GLINT_300SX	0x0001		/* GLINT 300SX */
#define	PCI_PRODUCT_3DLABS_GLINT_500TX	0x0002		/* GLINT 500TX */
#define	PCI_PRODUCT_3DLABS_GLINT_DELTA	0x0003		/* GLINT Delta */
#define	PCI_PRODUCT_3DLABS_PERMEDIA	0x0004		/* Permedia */
#define	PCI_PRODUCT_3DLABS_GLINT_MX	0x0006		/* GLINT MX */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2	0x0007		/* Permedia 2 */
#define	PCI_PRODUCT_3DLABS_GLINT_GAMMA	0x0008		/* GLINT Gamma */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2V	0x0009		/* Permedia 2v */
#define	PCI_PRODUCT_3DLABS_PERMEDIA3	0x000a		/* Permedia 3 */
#define	PCI_PRODUCT_3DLABS_WILDCAT_6210	0x07a1		/* Wildcat III 6210 */
#define	PCI_PRODUCT_3DLABS_WILDCAT_5110	0x07a2		/* Wildcat 5110 */
#define	PCI_PRODUCT_3DLABS_WILDCAT_7210	0x07a3		/* Wildcat IV 7210 */

/* 3ware products */
#define	PCI_PRODUCT_3WARE_ESCALADE	0x1000		/* 5000/6000 series RAID */
#define	PCI_PRODUCT_3WARE_ESCALADE_ASIC	0x1001		/* 7000/8000 series RAID */
#define	PCI_PRODUCT_3WARE_9000	0x1002		/* 9000 series RAID */
#define	PCI_PRODUCT_3WARE_9500	0x1003		/* 9500 series RAID */

/* Abocom products */
#define	PCI_PRODUCT_ABOCOM_FE2500	0xab02		/* FE2500 10/100 */
#define	PCI_PRODUCT_ABOCOM_PCM200	0xab03		/* PCM200 10/100 */
#define	PCI_PRODUCT_ABOCOM_FE2000VX	0xab06		/* FE2000VX 10/100 */
#define	PCI_PRODUCT_ABOCOM_FE2500MX	0xab08		/* FE2500MX 10/100 */

/* Aironet Products */
#define	PCI_PRODUCT_AIRONET_PC4800_1	0x0001		/* PC4800 Wireless */
#define	PCI_PRODUCT_AIRONET_PCI352	0x0350		/* PCI35x WLAN */
#define	PCI_PRODUCT_AIRONET_PC4500	0x4500		/* PC4500 Wireless */
#define	PCI_PRODUCT_AIRONET_PC4800	0x4800		/* PC4800 Wireless */
#define	PCI_PRODUCT_AIRONET_MPI350	0xa504		/* MPI-350 Wireless */

/* ACC Products */
#define	PCI_PRODUCT_ACC_2188	0x0000		/* ACCM 2188 VL-PCI */
#define	PCI_PRODUCT_ACC_2051_HB	0x2051		/* 2051 PCI */
#define	PCI_PRODUCT_ACC_2051_ISA	0x5842		/* 2051 ISA */

/* Acard products */
#define	PCI_PRODUCT_ACARD_ATP850U	0x0005		/* ATP850U/UF */
#define	PCI_PRODUCT_ACARD_ATP860	0x0006		/* ATP860 */
#define	PCI_PRODUCT_ACARD_ATP860A	0x0007		/* ATP860-A */
#define	PCI_PRODUCT_ACARD_ATP865A	0x0008		/* ATP865-A */
#define	PCI_PRODUCT_ACARD_ATP865R	0x0009		/* ATP865-R */
#define	PCI_PRODUCT_ACARD_AEC6710	0x8002		/* AEC6710 */
#define	PCI_PRODUCT_ACARD_AEC6712UW	0x8010		/* AEC6712UW */
#define	PCI_PRODUCT_ACARD_AEC6712U	0x8020		/* AEC6712U */
#define	PCI_PRODUCT_ACARD_AEC6712S	0x8030		/* AEC6712S */
#define	PCI_PRODUCT_ACARD_AEC6710D	0x8040		/* AEC6710D */
#define	PCI_PRODUCT_ACARD_AEC6715UW	0x8050		/* AEC6715UW */

/* Accton products */
#define	PCI_PRODUCT_ACCTON_5030	0x1211		/* MPX 5030/5038 */
#define	PCI_PRODUCT_ACCTON_EN2242	0x1216		/* EN2242 */
#define	PCI_PRODUCT_ACCTON_EN1217	0x1217		/* EN1217 */

/* Addtron products */
#define	PCI_PRODUCT_ADDTRON_RHINEII	0x1320		/* RhineII */
#define	PCI_PRODUCT_ADDTRON_8139	0x1360		/* rtl8139 */
#define	PCI_PRODUCT_ADDTRON_AWA100	0x7001		/* AWA-100 */

/* Acer products */
#define	PCI_PRODUCT_ACER_M1435	0x1435		/* M1435 VL-PCI */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 VL-PCI */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 ISA */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 PCI */
#define	PCI_PRODUCT_ALI_M1461	0x1461		/* M1461 PCI */
#define	PCI_PRODUCT_ALI_M1489	0x1489		/* M1489 PCI */
#define	PCI_PRODUCT_ALI_M1511	0x1511		/* M1511 PCI */
#define	PCI_PRODUCT_ALI_M1513	0x1513		/* M1513 ISA */
#define	PCI_PRODUCT_ALI_M1521	0x1521		/* M1523 PCI */
#define	PCI_PRODUCT_ALI_M1523	0x1523		/* M1523 ISA */
#define	PCI_PRODUCT_ALI_M1531	0x1531		/* M1531 PCI */
#define	PCI_PRODUCT_ALI_M1533	0x1533		/* M1533 ISA */
#define	PCI_PRODUCT_ALI_M1535	0x1535		/* M1535 PCI */
#define	PCI_PRODUCT_ALI_M1541	0x1541		/* M1541 PCI */
#define	PCI_PRODUCT_ALI_M1543	0x1543		/* M1543 ISA */
#define	PCI_PRODUCT_ALI_M1563	0x1563		/* M1563 ISA */
#define	PCI_PRODUCT_ALI_M1573	0x1573		/* M1573 ISA */
#define	PCI_PRODUCT_ALI_M1575	0x1575		/* M1575 ISA */
#define	PCI_PRODUCT_ALI_M1621	0x1621		/* M1621 PCI */
#define	PCI_PRODUCT_ALI_M1631	0x1631		/* M1631 PCI */
#define	PCI_PRODUCT_ALI_M1644	0x1644		/* M1644 PCI */
#define	PCI_PRODUCT_ALI_M1647	0x1647		/* M1647 PCI */
#define	PCI_PRODUCT_ALI_M1689	0x1689		/* M1689 PCI */
#define	PCI_PRODUCT_ALI_M1695	0x1695		/* M1695 PCI */
#define	PCI_PRODUCT_ALI_M3309	0x3309		/* M3309 MPEG */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */
#define	PCI_PRODUCT_ALI_M5219	0x5219		/* M5219 UDMA IDE */
#define	PCI_PRODUCT_ALI_M5229	0x5229		/* M5229 UDMA IDE */
#define	PCI_PRODUCT_ALI_M5237	0x5237		/* M5237 USB */
#define	PCI_PRODUCT_ALI_M5239	0x5239		/* M5239 USB2 */
#define	PCI_PRODUCT_ALI_M5243	0x5243		/* M5243 AGP/PCI-PCI */
#define	PCI_PRODUCT_ALI_M5246	0x5246		/* M5246 AGP */
#define	PCI_PRODUCT_ALI_M5247	0x5247		/* M5247 AGP/PCI-PC */
#define	PCI_PRODUCT_ALI_M5249	0x5249		/* M5249 PCI-PCI */
#define	PCI_PRODUCT_ALI_M524B	0x524b		/* M524B PCIE */
#define	PCI_PRODUCT_ALI_M524C	0x524c		/* M524C PCIE */
#define	PCI_PRODUCT_ALI_M524D	0x524d		/* M524D PCIE */
#define	PCI_PRODUCT_ALI_M5261	0x5261		/* M5261 LAN */
#define	PCI_PRODUCT_ALI_M5263	0x5263		/* M5263 LAN */
#define	PCI_PRODUCT_ALI_M5281	0x5281		/* M5281 SATA */
#define	PCI_PRODUCT_ALI_M5287	0x5287		/* M5287 SATA */
#define	PCI_PRODUCT_ALI_M5288	0x5288		/* M5288 SATA */
#define	PCI_PRODUCT_ALI_M5289	0x5289		/* M5289 SATA */
#define	PCI_PRODUCT_ALI_M5451	0x5451		/* M5451 Audio */
#define	PCI_PRODUCT_ALI_M5455	0x5455		/* M5455 Audio */
#define	PCI_PRODUCT_ALI_M5457	0x5457		/* M5457 Modem */
#define	PCI_PRODUCT_ALI_M5461	0x5461		/* M5461 HD Audio */
#define	PCI_PRODUCT_ALI_M7101	0x7101		/* M7101 Power */

/* ADMtek products */
#define	PCI_PRODUCT_ADMTEK_AL981	0x0981		/* AL981 */
#define	PCI_PRODUCT_ADMTEK_AN983	0x0985		/* AN983 */
#define	PCI_PRODUCT_ADMTEK_AN985	0x1985		/* AN985 */
#define	PCI_PRODUCT_ADMTEK_ADM8211	0x8201		/* ADM8211 WLAN */
#define	PCI_PRODUCT_ADMTEK_ADM9511	0x9511		/* ADM9511 */
#define	PCI_PRODUCT_ADMTEK_ADM9513	0x9513		/* ADM9513 */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC7810	0x1078		/* AIC-7810 */
#define	PCI_PRODUCT_ADP_2940AU_CN	0x2178		/* AHA-2940AU/CN */
#define	PCI_PRODUCT_ADP_2930CU	0x3860		/* AHA-2930CU */
#define	PCI_PRODUCT_ADP_AIC7850	0x5078		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7855	0x5578		/* AIC-7855 */
#define	PCI_PRODUCT_ADP_AIC5900	0x5900		/* AIC-5900 ATM */
#define	PCI_PRODUCT_ADP_AIC5905	0x5905		/* AIC-5905 ATM */
#define	PCI_PRODUCT_ADP_1480	0x6075		/* APA-1480 */
#define	PCI_PRODUCT_ADP_AIC7860	0x6078		/* AIC-7860 */
#define	PCI_PRODUCT_ADP_2940AU	0x6178		/* AHA-2940AU */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_2940	0x7178		/* AHA-2940 */
#define	PCI_PRODUCT_ADP_3940	0x7278		/* AHA-3940 */
#define	PCI_PRODUCT_ADP_3985	0x7378		/* AHA-3985 */
#define	PCI_PRODUCT_ADP_2944	0x7478		/* AHA-2944 */
#define	PCI_PRODUCT_ADP_AIC7815	0x7815		/* AIC-7815 */
#define	PCI_PRODUCT_ADP_AIC7880	0x8078		/* AIC-7880 */
#define	PCI_PRODUCT_ADP_2940U	0x8178		/* AHA-2940U */
#define	PCI_PRODUCT_ADP_3940U	0x8278		/* AHA-3940U */
#define	PCI_PRODUCT_ADP_398XU	0x8378		/* AHA-398XU */
#define	PCI_PRODUCT_ADP_2944U	0x8478		/* AHA-2944U */
#define	PCI_PRODUCT_ADP_2940UWPro	0x8778		/* AHA-2940UWPro */
#define	PCI_PRODUCT_ADP_AIC6915	0x6915		/* AIC-6915 */
#define	PCI_PRODUCT_ADP_7895	0x7895		/* AIC-7895 */

#define	PCI_PRODUCT_ADP2_2940U2	0x0010		/* AHA-2940U2 U2 */
#define	PCI_PRODUCT_ADP2_2930U2	0x0011		/* AHA-2930U2 U2 */
#define	PCI_PRODUCT_ADP2_AAA131U2	0x0013		/* AAA-131U2 U2 */
#define	PCI_PRODUCT_ADP2_AIC7890	0x001f		/* AIC-7890/1 U2 */
#define	PCI_PRODUCT_ADP2_AIC7892	0x008f		/* AIC-7892 U160 */
#define	PCI_PRODUCT_ADP2_29160	0x0080		/* AHA-29160 U160 */
#define	PCI_PRODUCT_ADP2_19160B	0x0081		/* AHA-19160B U160 */
#define	PCI_PRODUCT_ADP2_3950U2B	0x0050		/* AHA-3950U2B U2 */
#define	PCI_PRODUCT_ADP2_3950U2D	0x0051		/* AHA-3950U2D U2 */
#define	PCI_PRODUCT_ADP2_AIC7896	0x005f		/* AIC-7896/7 U2 */
#define	PCI_PRODUCT_ADP2_3960D	0x00c0		/* AHA-3960D U160 */
#define	PCI_PRODUCT_ADP2_AIC7899B	0x00c1		/* AIC-7899B */
#define	PCI_PRODUCT_ADP2_AIC7899D	0x00c3		/* AIC-7899D */
#define	PCI_PRODUCT_ADP2_AIC7899F	0x00c5		/* AIC-7899F */
#define	PCI_PRODUCT_ADP2_AIC7899	0x00cf		/* AIC-7899 U160 */
#define	PCI_PRODUCT_ADP2_SERVERAID	0x0250		/* ServeRAID */
#define	PCI_PRODUCT_ADP2_AAC2622	0x0282		/* AAC-2622 */
#define	PCI_PRODUCT_ADP2_ASR2200S	0x0285		/* ASR-2200S */
#define	PCI_PRODUCT_ADP2_ASR2120S	0x0286		/* ASR-2120S */
#define	PCI_PRODUCT_ADP2_AAC364	0x0364		/* AAC-364 */
#define	PCI_PRODUCT_ADP2_AAC3642	0x0365		/* AAC-3642 */
#define	PCI_PRODUCT_ADP2_PERC_2QC	0x1364		/* Dell PERC 2/QC */
#define	PCI_PRODUCT_ADP2_AIC7901	0x800f		/* AIC-7901 U320 */
#define	PCI_PRODUCT_ADP2_AHA29320A	0x8000		/* AHA-29320A U320 */
#define	PCI_PRODUCT_ADP2_AHA29320LP	0x8017		/* AHA-29320LP U320 */
#define	PCI_PRODUCT_ADP2_AIC7901A	0x801e		/* AIC-7901A U320 */
#define	PCI_PRODUCT_ADP2_AHA29320	0x8012		/* AHA-29320 U320 */
#define	PCI_PRODUCT_ADP2_AHA29320B	0x8013		/* AHA-29320B U320 */
#define	PCI_PRODUCT_ADP2_AHA29320LP2	0x8014		/* AHA-29320LP U320 */
#define	PCI_PRODUCT_ADP2_AIC7902	0x801f		/* AIC-7902 U320 */
#define	PCI_PRODUCT_ADP2_AIC7902_B	0x801d		/* AIC-7902B U320 */
#define	PCI_PRODUCT_ADP2_AHA39320	0x8010		/* AHA-39320 U320 */
#define	PCI_PRODUCT_ADP2_AHA39320B	0x8015		/* AHA-39320B U320 */
#define	PCI_PRODUCT_ADP2_AHA39320A	0x8016		/* AHA-39320A U320 */
#define	PCI_PRODUCT_ADP2_AHA39320D	0x8011		/* AHA-39320D U320 */
#define	PCI_PRODUCT_ADP2_AHA39320DB	0x801c		/* AHA-39320DB U320 */

/* Advanced System Products */
#define	PCI_PRODUCT_ADVSYS_1200A	0x1100		/* 1200A */
#define	PCI_PRODUCT_ADVSYS_1200B	0x1200		/* 1200B */
#define	PCI_PRODUCT_ADVSYS_ULTRA	0x1300		/* ABP-930/40UA */
#define	PCI_PRODUCT_ADVSYS_WIDE	0x2300		/* ABP-940UW */
#define	PCI_PRODUCT_ADVSYS_U2W	0x2500		/* ASP-3940U2W */
#define	PCI_PRODUCT_ADVSYS_U3W	0x2700		/* ASP-3940U3W */

/* Advanced Telecommunications Modules */
#define	PCI_PRODUCT_ATML_WAIKATO	0x3200		/* Waikato Dag3.2 */
#define	PCI_PRODUCT_ATML_DAG35	0x3500		/* Endace Dag3.5 */
#define	PCI_PRODUCT_ATML_DAG422GE	0x422e		/* Endace Dag4.22GE */
#define	PCI_PRODUCT_ATML_DAG423	0x4230		/* Endace Dag4.23 */

/* Ageia */
#define	PCI_PRODUCT_AGEIA_PHYSX	0x1011		/* PhysX */

/* Alliance products */
#define	PCI_PRODUCT_ALLIANCE_AT22	0x6422		/* AT22 */
#define	PCI_PRODUCT_ALLIANCE_AT24	0x6424		/* AT24 */

/* Alteon products */
#define	PCI_PRODUCT_ALTEON_ACENIC	0x0001		/* Acenic */
#define	PCI_PRODUCT_ALTEON_ACENICT	0x0002		/* Acenic Copper */
#define	PCI_PRODUCT_ALTEON_BCM5700	0x0003		/* BCM5700 */
#define	PCI_PRODUCT_ALTEON_BCM5701	0x0004		/* BCM5701 */

/* Altera products */
#define	PCI_PRODUCT_ALTERA_EBUS	0x0000		/* EBus */

/* Altima products */
#define	PCI_PRODUCT_ALTIMA_AC1000	0x03e8		/* AC1000 */
#define	PCI_PRODUCT_ALTIMA_AC1001	0x03e9		/* AC1001 */
#define	PCI_PRODUCT_ALTIMA_AC9100	0x03ea		/* AC9100 */
#define	PCI_PRODUCT_ALTIMA_AC1003	0x03eb		/* AC1003 */

/* AMD products */
#define	PCI_PRODUCT_AMD_AMD64_0F_HT	0x1100		/* AMD64 0Fh HyperTransport */
#define	PCI_PRODUCT_AMD_AMD64_0F_ADDR	0x1101		/* AMD64 0Fh Address Map */
#define	PCI_PRODUCT_AMD_AMD64_0F_DRAM	0x1102		/* AMD64 0Fh DRAM Cfg */
#define	PCI_PRODUCT_AMD_AMD64_0F_MISC	0x1103		/* AMD64 0Fh Misc Cfg */
#define	PCI_PRODUCT_AMD_AMD64_10_HT	0x1200		/* AMD64 10h HyperTransport */
#define	PCI_PRODUCT_AMD_AMD64_10_ADDR	0x1201		/* AMD64 10h Address Map */
#define	PCI_PRODUCT_AMD_AMD64_10_DRAM	0x1202		/* AMD64 10h DRAM Cfg */
#define	PCI_PRODUCT_AMD_AMD64_10_MISC	0x1203		/* AMD64 10h Misc Cfg */
#define	PCI_PRODUCT_AMD_AMD64_10_LINK	0x1204		/* AMD64 10h Link Cfg */
#define	PCI_PRODUCT_AMD_AMD64_11_HT	0x1300		/* AMD64 11h HyperTransport */
#define	PCI_PRODUCT_AMD_AMD64_11_ADDR	0x1301		/* AMD64 11h Address Map */
#define	PCI_PRODUCT_AMD_AMD64_11_DRAM	0x1302		/* AMD64 11h DRAM Cfg */
#define	PCI_PRODUCT_AMD_AMD64_11_MISC	0x1303		/* AMD64 11h Misc Cfg */
#define	PCI_PRODUCT_AMD_AMD64_11_LINK	0x1304		/* AMD64 11h Link Cfg */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* 79c970 PCnet-PCI */
#define	PCI_PRODUCT_AMD_PCHOME_PCI	0x2001		/* 79c978 PChome-PCI */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* 53c974 PCscsi-PCI */
#define	PCI_PRODUCT_AMD_PCNETS_PCI	0x2040		/* 79C974 PCnet-PCI */
#define	PCI_PRODUCT_AMD_GEODE_LX_PCHB	0x2080		/* Geode LX */
#define	PCI_PRODUCT_AMD_GEODE_LX_VIDEO	0x2081		/* Geode LX Video */
#define	PCI_PRODUCT_AMD_GEODE_LX_CRYPTO	0x2082		/* Geode LX Crypto */
#define	PCI_PRODUCT_AMD_CS5536_PCISB	0x208f		/* CS5536 PCI */
#define	PCI_PRODUCT_AMD_CS5536_PCIB	0x2090		/* CS5536 ISA */
#define	PCI_PRODUCT_AMD_CS5536_AUDIO	0x2093		/* CS5536 Audio */
#define	PCI_PRODUCT_AMD_CS5536_OHCI	0x2094		/* CS5536 USB */
#define	PCI_PRODUCT_AMD_CS5536_EHCI	0x2095		/* CS5536 USB */
#define	PCI_PRODUCT_AMD_CS5536_IDE	0x209a		/* CS5536 IDE */
#define	PCI_PRODUCT_AMD_ELANSC520	0x3000		/* ElanSC520 PCI */
/* http://www.amd.com/products/cpg/athlon/techdocs/pdf/21910.pdf */
#define	PCI_PRODUCT_AMD_SC751_SC	0x7006		/* 751 System */
#define	PCI_PRODUCT_AMD_SC751_PPB	0x7007		/* 751 PCI-PCI */
/* http://www.amd.com/products/cpg/athlon/techdocs/pdf/24462.pdf */
#define	PCI_PRODUCT_AMD_762_PCHB	0x700c		/* 762 PCI */
#define	PCI_PRODUCT_AMD_762_PPB	0x700d		/* 762 PCI-PCI */
#define	PCI_PRODUCT_AMD_761_PCHB	0x700e		/* 761 PCI */
#define	PCI_PRODUCT_AMD_761_PPB	0x700f		/* 761 PCI-PCI */
#define	PCI_PRODUCT_AMD_755_ISA	0x7400		/* 755 ISA */
#define	PCI_PRODUCT_AMD_755_IDE	0x7401		/* 755 IDE */
#define	PCI_PRODUCT_AMD_755_PMC	0x7403		/* 755 Power */
#define	PCI_PRODUCT_AMD_755_USB	0x7404		/* 755 USB */
/* http://www.amd.com/products/cpg/athlon/techdocs/pdf/22548.pdf */
#define	PCI_PRODUCT_AMD_PBC756_ISA	0x7408		/* 756 ISA */
#define	PCI_PRODUCT_AMD_PBC756_IDE	0x7409		/* 756 IDE */
#define	PCI_PRODUCT_AMD_PBC756_PMC	0x740b		/* 756 Power */
#define	PCI_PRODUCT_AMD_PBC756_USB	0x740c		/* 756 USB Host */
#define	PCI_PRODUCT_AMD_766_ISA	0x7410		/* 766 ISA */
#define	PCI_PRODUCT_AMD_766_IDE	0x7411		/* 766 IDE */
#define	PCI_PRODUCT_AMD_766_USB	0x7412		/* 766 USB */
#define	PCI_PRODUCT_AMD_766_PMC	0x7413		/* 766 Power */
#define	PCI_PRODUCT_AMD_766_USB_HCI	0x7414		/* 766 USB OpenHCI */
#define	PCI_PRODUCT_AMD_PBC768_ISA	0x7440		/* 768 ISA */
#define	PCI_PRODUCT_AMD_PBC768_IDE	0x7441		/* 768 IDE */
#define	PCI_PRODUCT_AMD_PBC768_PMC	0x7443		/* 768 Power */
#define	PCI_PRODUCT_AMD_PBC768_ACA	0x7445		/* 768 AC97 */
#define	PCI_PRODUCT_AMD_PBC768_MD	0x7446		/* 768 Modem */
#define	PCI_PRODUCT_AMD_PBC768_PPB	0x7448		/* 768 PCI-PCI */
#define	PCI_PRODUCT_AMD_PBC768_USB	0x7449		/* 768 USB */
#define	PCI_PRODUCT_AMD_8131_PCIX	0x7450		/* 8131 PCIX */
#define	PCI_PRODUCT_AMD_8131_PCIX_IOAPIC	0x7451		/* 8131 PCIX IOAPIC */
#define	PCI_PRODUCT_AMD_8151_SC	0x7454		/* 8151 Sys Control */
#define	PCI_PRODUCT_AMD_8151_AGP	0x7455		/* 8151 AGP */
#define	PCI_PRODUCT_AMD_8132_PCIX	0x7458		/* 8132 PCIX */
#define	PCI_PRODUCT_AMD_8132_PCIX_IOAPIC	0x7459		/* 8132 PCIX IOAPIC */
#define	PCI_PRODUCT_AMD_8111_PPB	0x7460		/* 8111 PCI-PCI */
#define	PCI_PRODUCT_AMD_8111_ETHER	0x7462		/* 8111 Ether */
#define	PCI_PRODUCT_AMD_8111_EHCI	0x7463		/* 8111 USB */
#define	PCI_PRODUCT_AMD_8111_USB	0x7464		/* 8111 USB */
#define	PCI_PRODUCT_AMD_PBC8111_LPC	0x7468		/* 8111 LPC */
#define	PCI_PRODUCT_AMD_8111_IDE	0x7469		/* 8111 IDE */
#define	PCI_PRODUCT_AMD_8111_SMB	0x746a		/* 8111 SMBus */
#define	PCI_PRODUCT_AMD_8111_PMC	0x746b		/* 8111 Power */
#define	PCI_PRODUCT_AMD_8111_ACA	0x746d		/* 8111 AC97 */
#define	PCI_PRODUCT_AMD_HUDSON2_SATA	0x7800		/* Hudson-2 SATA */
#define	PCI_PRODUCT_AMD_HUDSON2_SMB	0x780b		/* Hudson-2 SMBus */
#define	PCI_PRODUCT_AMD_HUDSON2_IDE	0x780c		/* Hudson-2 IDE */
#define	PCI_PRODUCT_AMD_RS780_HB	0x9600		/* RS780 Host */
#define	PCI_PRODUCT_AMD_RS780_HB_2	0x9601		/* RS780 Host */
#define	PCI_PRODUCT_AMD_RS780_PCIE_1	0x9602		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_2	0x9603		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_3	0x9604		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_4	0x9605		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_5	0x9606		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_6	0x9607		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_8	0x9608		/* RS780 PCIE */
#define	PCI_PRODUCT_AMD_RS780_PCIE_7	0x9609		/* RS780 PCIE */

/* AMI */
#define	PCI_PRODUCT_AMI_MEGARAID	0x1960		/* MegaRAID */
#define	PCI_PRODUCT_AMI_MEGARAID428	0x9010		/* MegaRAID Series 428 */
#define	PCI_PRODUCT_AMI_MEGARAID434	0x9060		/* MegaRAID Series 434 */

/* Analog Devices */
#define	PCI_PRODUCT_AD_SP21535	0x1535		/* ADSP 21535 DSP */
#define	PCI_PRODUCT_AD_1889	0x1889		/* AD1889 Audio */
#define	PCI_PRODUCT_AD_SP2141	0x2f44		/* SafeNet ADSP 2141 */

/* Antares Microsystems products */
#define	PCI_PRODUCT_ANTARES_TC9021	0x1021		/* TC9021 */

/* Apple products */
#define	PCI_PRODUCT_APPLE_BANDIT	0x0001		/* Bandit */
#define	PCI_PRODUCT_APPLE_GC	0x0002		/* GC */
#define	PCI_PRODUCT_APPLE_OHARE	0x0007		/* OHare */
#define	PCI_PRODUCT_APPLE_HEATHROW	0x0010		/* Heathrow */
#define	PCI_PRODUCT_APPLE_PADDINGTON	0x0017		/* Paddington */
#define	PCI_PRODUCT_APPLE_UNINORTHETH	0x001e		/* Uni-N Eth */
#define	PCI_PRODUCT_APPLE_UNINORTH	0x001f		/* Uni-N */
#define	PCI_PRODUCT_APPLE_UNINORTHETH_FW	0x0018		/* Uni-N Eth Firewire */
#define	PCI_PRODUCT_APPLE_USB	0x0019		/* USB */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP	0x0020		/* Uni-N AGP */
#define	PCI_PRODUCT_APPLE_UNINORTHGMAC	0x0021		/* Uni-N GMAC */
#define	PCI_PRODUCT_APPLE_KEYLARGO	0x0022		/* Keylargo */
#define	PCI_PRODUCT_APPLE_PANGEA_GMAC	0x0024		/* Pangea GMAC */
#define	PCI_PRODUCT_APPLE_PANGEA_MACIO	0x0025		/* Pangea Macio */
#define	PCI_PRODUCT_APPLE_PANGEA_OHCI	0x0026		/* Pangea USB */
#define	PCI_PRODUCT_APPLE_PANGEA_AGP	0x0027		/* Pangea AGP */
#define	PCI_PRODUCT_APPLE_PANGEA	0x0028		/* Pangea */
#define	PCI_PRODUCT_APPLE_PANGEA_PCI	0x0029		/* Pangea PCI */
#define	PCI_PRODUCT_APPLE_UNINORTH2_AGP	0x002d		/* Uni-N2 AGP */
#define	PCI_PRODUCT_APPLE_UNINORTH2	0x002e		/* Uni-N2 Host */
#define	PCI_PRODUCT_APPLE_UNINORTH2ETH	0x002f		/* Uni-N2 Host */
#define	PCI_PRODUCT_APPLE_PANGEA_FW	0x0030		/* Pangea FireWire */
#define	PCI_PRODUCT_APPLE_UNINORTH_FW	0x0031		/* UniNorth Firewire */
#define	PCI_PRODUCT_APPLE_UNINORTH2GMAC	0x0032		/* Uni-N2 GMAC */
#define	PCI_PRODUCT_APPLE_UNINORTH_ATA	0x0033		/* Uni-N ATA */
#define	PCI_PRODUCT_APPLE_UNINORTH_AGP3	0x0034		/* UniNorth AGP */
#define	PCI_PRODUCT_APPLE_UNINORTH5	0x0035		/* UniNorth PCI */
#define	PCI_PRODUCT_APPLE_UNINORTH6	0x0036		/* UniNorth PCI */
#define	PCI_PRODUCT_APPLE_INTREPID_ATA	0x003b		/* Intrepid ATA */
#define	PCI_PRODUCT_APPLE_INTREPID	0x003e		/* Intrepid */
#define	PCI_PRODUCT_APPLE_INTREPID_OHCI	0x003f		/* Intrepid USB */
#define	PCI_PRODUCT_APPLE_K2_USB	0x0040		/* K2 USB */
#define	PCI_PRODUCT_APPLE_K2_MACIO	0x0041		/* K2 Macio */
#define	PCI_PRODUCT_APPLE_K2_FW	0x0042		/* K2 Firewire */
#define	PCI_PRODUCT_APPLE_K2_ATA	0x0043		/* K2 ATA */
#define	PCI_PRODUCT_APPLE_U3_PPB1	0x0045		/* U3 PCI-PCI */
#define	PCI_PRODUCT_APPLE_U3_PPB2	0x0046		/* U3 PCI-PCI */
#define	PCI_PRODUCT_APPLE_U3_PPB3	0x0047		/* U3 PCI-PCI */
#define	PCI_PRODUCT_APPLE_U3_PPB4	0x0048		/* U3 PCI-PCI */
#define	PCI_PRODUCT_APPLE_U3_PPB5	0x0049		/* U3 PCI-PCI */
#define	PCI_PRODUCT_APPLE_U3_AGP	0x004b		/* U3 AGP */
#define	PCI_PRODUCT_APPLE_K2_GMAC	0x004c		/* K2 GMAC */
#define	PCI_PRODUCT_APPLE_SHASTA	0x004f		/* Shasta */
#define	PCI_PRODUCT_APPLE_SHASTA_ATA	0x0050		/* Shasta ATA */
#define	PCI_PRODUCT_APPLE_SHASTA_GMAC	0x0051		/* Shasta GMAC */
#define	PCI_PRODUCT_APPLE_SHASTA_FW	0x0052		/* Shasta Firewire */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI1	0x0053		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI2	0x0054		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_PCI3	0x0055		/* Shasta PCI */
#define	PCI_PRODUCT_APPLE_SHASTA_HT	0x0056		/* Shasta HyperTransport */
#define	PCI_PRODUCT_APPLE_K2	0x0057		/* K2 */
#define	PCI_PRODUCT_APPLE_U3L_AGP	0x0058		/* U3L AGP */
#define	PCI_PRODUCT_APPLE_K2_AGP	0x0059		/* K2 AGP */
#define	PCI_PRODUCT_APPLE_INTREPID2_AGP	0x0066		/* Intrepid 2 AGP */
#define	PCI_PRODUCT_APPLE_INTREPID2_PCI1	0x0067		/* Intrepid 2 PCI */
#define	PCI_PRODUCT_APPLE_INTREPID2_PCI2	0x0068		/* Intrepid 2 PCI */
#define	PCI_PRODUCT_APPLE_INTREPID2_ATA	0x0069		/* Intrepid 2 ATA */
#define	PCI_PRODUCT_APPLE_INTREPID2_FW	0x006a		/* Intrepid 2 FireWire */
#define	PCI_PRODUCT_APPLE_INTREPID2_GMAC	0x006b		/* Intrepid 2 GMAC */
#define	PCI_PRODUCT_APPLE_BCM5701	0x1645		/* BCM5701 */

/* Aralion products */
#define	PCI_PRODUCT_ARALION_ARS106S	0x0301		/* ARS106S */
#define	PCI_PRODUCT_ARALION_ARS0303D	0x0303		/* ARS0303D */

/* ARC Logic products */
#define	PCI_PRODUCT_ARC_USB	0x0003		/* USB */
#define	PCI_PRODUCT_ARC_1000PV	0xa091		/* 1000PV */
#define	PCI_PRODUCT_ARC_2000PV	0xa099		/* 2000PV */
#define	PCI_PRODUCT_ARC_2000MT	0xa0a1		/* 2000MT */
#define	PCI_PRODUCT_ARC_2000MI	0xa0a9		/* 2000MI */

/* Areca products */
#define	PCI_PRODUCT_ARECA_ARC1110	0x1110		/* ARC-1110 */
#define	PCI_PRODUCT_ARECA_ARC1120	0x1120		/* ARC-1120 */
#define	PCI_PRODUCT_ARECA_ARC1130	0x1130		/* ARC-1130 */
#define	PCI_PRODUCT_ARECA_ARC1160	0x1160		/* ARC-1160 */
#define	PCI_PRODUCT_ARECA_ARC1170	0x1170		/* ARC-1170 */
#define	PCI_PRODUCT_ARECA_ARC1200	0x1200		/* ARC-1200 */
#define	PCI_PRODUCT_ARECA_ARC1200_B	0x1201		/* ARC-1200 rev B */
#define	PCI_PRODUCT_ARECA_ARC1202	0x1202		/* ARC-1202 */
#define	PCI_PRODUCT_ARECA_ARC1210	0x1210		/* ARC-1210 */
#define	PCI_PRODUCT_ARECA_ARC1220	0x1220		/* ARC-1220 */
#define	PCI_PRODUCT_ARECA_ARC1230	0x1230		/* ARC-1230 */
#define	PCI_PRODUCT_ARECA_ARC1260	0x1260		/* ARC-1260 */
#define	PCI_PRODUCT_ARECA_ARC1270	0x1270		/* ARC-1270 */
#define	PCI_PRODUCT_ARECA_ARC1280	0x1280		/* ARC-1280 */
#define	PCI_PRODUCT_ARECA_ARC1380	0x1380		/* ARC-1380 */
#define	PCI_PRODUCT_ARECA_ARC1381	0x1381		/* ARC-1381 */
#define	PCI_PRODUCT_ARECA_ARC1680	0x1680		/* ARC-1680 */
#define	PCI_PRODUCT_ARECA_ARC1681	0x1681		/* ARC-1681 */

/* ASIX Electronics products */
#define	PCI_PRODUCT_ASIX_AX88140A	0x1400		/* AX88140A/88141 */

/* Asustek products */
#define	PCI_PRODUCT_ASUSTEK_HFCPCI	0x0675		/* ISDN */

/* ATI Technologies */
#define	PCI_PRODUCT_ATI_RADEON_M241P	0x3150		/* Radeon M241P */
#define	PCI_PRODUCT_ATI_RADEON_X300M24	0x3150		/* Radeon X300 M24 */
#define	PCI_PRODUCT_ATI_FIREGL_M24GL	0x3154		/* FireGL M24 GL */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV380	0x3e50		/* Radeon X600 */
#define	PCI_PRODUCT_ATI_FIREGL_V3200	0x3e54		/* FireGL V3200 */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV380_S	0x3e70		/* Radeon X600 Sec */
#define	PCI_PRODUCT_ATI_RADEON_IGP320	0x4136		/* Radeon IGP 320 */
#define	PCI_PRODUCT_ATI_RADEON_IGP340	0x4137		/* Radeon IGP 340 */
#define	PCI_PRODUCT_ATI_RADEON_9500PRO	0x4144		/* Radeon 9500 Pro */
#define	PCI_PRODUCT_ATI_RADEON_AE9700PRO	0x4145		/* Radeon AE 9700 Pro */
#define	PCI_PRODUCT_ATI_RADEON_AF9600TX	0x4146		/* Radeon AF 9600TX */
#define	PCI_PRODUCT_ATI_FIREGL_AGZ1	0x4147		/* FireGL AGZ1 */
#define	PCI_PRODUCT_ATI_RADEON_AH_9800SE	0x4148		/* Radeon AH 9800 SE */
#define	PCI_PRODUCT_ATI_RADEON_AI_9800	0x4149		/* Radeon AI 9800 */
#define	PCI_PRODUCT_ATI_RADEON_AJ_9800	0x414a		/* Radeon AJ 9800 */
#define	PCI_PRODUCT_ATI_FIREGL_AKX2	0x414b		/* FireGL AK X2 */
#define	PCI_PRODUCT_ATI_RADEON_9600PRO	0x4150		/* Radeon 9600 Pro */
#define	PCI_PRODUCT_ATI_RADEON_9600LE	0x4151		/* Radeon 9600 */
#define	PCI_PRODUCT_ATI_RADEON_9600XT	0x4152		/* Radeon 9600 */
#define	PCI_PRODUCT_ATI_RADEON_9550	0x4153		/* Radeon 9550 */
#define	PCI_PRODUCT_ATI_FIREGL_ATT2	0x4154		/* FireGL */
#define	PCI_PRODUCT_ATI_RADEON_9650	0x4155		/* Radeon 9650 */
#define	PCI_PRODUCT_ATI_FIREGL_AVT2	0x4156		/* FireGL */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_RADEON_9500PRO_S	0x4164		/* Radeon 9500 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600PRO_S	0x4170		/* Radeon 9600 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600LE_S	0x4171		/* Radeon 9600 LE Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600XT_S	0x4172		/* Radeon 9600 XT Sec */
#define	PCI_PRODUCT_ATI_RADEON_9550_S	0x4173		/* Radeon 9550 Sec */
#define	PCI_PRODUCT_ATI_RADEON_IGP_RS250	0x4237		/* Radeon IGP */
#define	PCI_PRODUCT_ATI_R200_BB	0x4242		/* Radeon 8500 */
#define	PCI_PRODUCT_ATI_R200_BC	0x4243		/* Radeon BC R200 */
#define	PCI_PRODUCT_ATI_RADEON_IGP320M	0x4336		/* Radeon IGP 320M */
#define	PCI_PRODUCT_ATI_MOBILITY_M6	0x4337		/* Mobility M6 */
#define	PCI_PRODUCT_ATI_SB200_AUDIO	0x4341		/* SB200 AC97 */
#define	PCI_PRODUCT_ATI_SB200_PCI	0x4342		/* SB200 PCI */
#define	PCI_PRODUCT_ATI_SB200_EHCI	0x4345		/* SB200 USB2 */
#define	PCI_PRODUCT_ATI_SB200_OHCI_1	0x4347		/* SB200 USB */
#define	PCI_PRODUCT_ATI_SB200_OHCI_2	0x4348		/* SB200 USB */
#define	PCI_PRODUCT_ATI_SB200_IDE	0x4349		/* SB200 IDE */
#define	PCI_PRODUCT_ATI_SB200_ISA	0x434c		/* SB200 ISA */
#define	PCI_PRODUCT_ATI_SB200_MODEM	0x434d		/* SB200 Modem */
#define	PCI_PRODUCT_ATI_SB200_SMB	0x4353		/* SB200 SMBus */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_SB300_AUDIO	0x4361		/* SB300 AC97 */
#define	PCI_PRODUCT_ATI_SB300_PCI	0x4362		/* SB300 PCI */
#define	PCI_PRODUCT_ATI_SB300_SMB	0x4363		/* SB300 SMBus */
#define	PCI_PRODUCT_ATI_SB300_EHCI	0x4365		/* SB300 USB2 */
#define	PCI_PRODUCT_ATI_SB300_OHCI_1	0x4367		/* SB300 USB */
#define	PCI_PRODUCT_ATI_SB300_OHCI_2	0x4368		/* SB300 USB */
#define	PCI_PRODUCT_ATI_SB300_IDE	0x4369		/* SB300 IDE */
#define	PCI_PRODUCT_ATI_SB300_ISA	0x436c		/* SB300 ISA */
#define	PCI_PRODUCT_ATI_SB300_MODEM	0x436d		/* SB300 Modem */
#define	PCI_PRODUCT_ATI_SB300_SATA	0x436e		/* SB300 SATA */
#define	PCI_PRODUCT_ATI_SB400_AUDIO	0x4370		/* SB400 AC97 */
#define	PCI_PRODUCT_ATI_SB400_PCI	0x4371		/* SB400 PCI */
#define	PCI_PRODUCT_ATI_SB400_SMB	0x4372		/* SB400 SMBus */
#define	PCI_PRODUCT_ATI_SB400_EHCI	0x4373		/* SB400 USB2 */
#define	PCI_PRODUCT_ATI_SB400_OHCI_1	0x4374		/* SB400 USB */
#define	PCI_PRODUCT_ATI_SB400_OHCI_2	0x4375		/* SB400 USB */
#define	PCI_PRODUCT_ATI_SB400_IDE	0x4376		/* SB400 IDE */
#define	PCI_PRODUCT_ATI_SB400_ISA	0x4377		/* SB400 ISA */
#define	PCI_PRODUCT_ATI_SB400_MODEM	0x4378		/* SB400 Modem */
#define	PCI_PRODUCT_ATI_SB400_SATA_1	0x4379		/* SB400 SATA */
#define	PCI_PRODUCT_ATI_SB400_SATA_2	0x437a		/* SB400 SATA */
#define	PCI_PRODUCT_ATI_SB450_HDA	0x437b		/* SB450 HD Audio */
#define	PCI_PRODUCT_ATI_SB600_SATA	0x4380		/* SB600 SATA */
#define	PCI_PRODUCT_ATI_SB600_AUDIO	0x4382		/* SB600 AC97 */
#define	PCI_PRODUCT_ATI_SBX00_HDA	0x4383		/* SBx00 HD Audio */
#define	PCI_PRODUCT_ATI_SB600_PCI	0x4384		/* SB600 PCI */
#define	PCI_PRODUCT_ATI_SBX00_SMB	0x4385		/* SBx00 SMBus */
#define	PCI_PRODUCT_ATI_SB600_EHCI	0x4386		/* SB600 USB2 */
#define	PCI_PRODUCT_ATI_SB600_OHCI_1	0x4387		/* SB600 USB */
#define	PCI_PRODUCT_ATI_SB600_OHCI_2	0x4388		/* SB600 USB */
#define	PCI_PRODUCT_ATI_SB600_OHCI_3	0x4389		/* SB600 USB */
#define	PCI_PRODUCT_ATI_SB600_OHCI_4	0x438a		/* SB600 USB */
#define	PCI_PRODUCT_ATI_SB600_OHCI_5	0x438b		/* SB600 USB */
#define	PCI_PRODUCT_ATI_SB600_IDE	0x438c		/* SB600 IDE */
#define	PCI_PRODUCT_ATI_SB600_ISA	0x438d		/* SB600 ISA */
#define	PCI_PRODUCT_ATI_SB600_MODEM	0x438e		/* SB600 Modem */
#define	PCI_PRODUCT_ATI_SBX00_SATA_1	0x4390		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SBX00_SATA_2	0x4391		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SBX00_SATA_3	0x4392		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SBX00_SATA_4	0x4393		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SBX00_SATA_5	0x4394		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SBX00_SATA_6	0x4395		/* SBx00 SATA */
#define	PCI_PRODUCT_ATI_SB700_EHCI	0x4396		/* SB700 USB2 */
#define	PCI_PRODUCT_ATI_SB700_OHCI_1	0x4397		/* SB700 USB */
#define	PCI_PRODUCT_ATI_SB700_OHCI_2	0x4398		/* SB700 USB */
#define	PCI_PRODUCT_ATI_SB700_OHCI_3	0x4399		/* SB700 USB */
#define	PCI_PRODUCT_ATI_SB700_OHCI_4	0x439a		/* SB700 USB */
#define	PCI_PRODUCT_ATI_SB700_OHCI_5	0x439b		/* SB700 USB */
#define	PCI_PRODUCT_ATI_SB700_IDE	0x439c		/* SB700 IDE */
#define	PCI_PRODUCT_ATI_SB700_ISA	0x439d		/* SB700 ISA */
#define	PCI_PRODUCT_ATI_RADEON_MIGP_RS250	0x4437		/* Radeon Mobility IGP */
#define	PCI_PRODUCT_ATI_MACH64_ET	0x4554		/* Mach64 ET */
#define	PCI_PRODUCT_ATI_RAGEPRO	0x4742		/* Rage Pro */
#define	PCI_PRODUCT_ATI_MACH64_GD	0x4744		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GI	0x4749		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GL	0x474c		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GM	0x474d		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GN	0x474e		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GO	0x474f		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GP	0x4750		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GQ	0x4751		/* Mach64 */
#define	PCI_PRODUCT_ATI_RAGEXL	0x4752		/* Rage XL */
#define	PCI_PRODUCT_ATI_MACH64_GS	0x4753		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GT	0x4754		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GU	0x4755		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GV	0x4756		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GW	0x4757		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GY	0x4759		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_GZ	0x475a		/* Mach64 */
#define	PCI_PRODUCT_ATI_RV250	0x4966		/* Radeon 9000 */
#define	PCI_PRODUCT_ATI_RADEON_IG9000	0x4966		/* Radeon 9000 */
#define	PCI_PRODUCT_ATI_RV250_S	0x496e		/* Radeon 9000 Sec */
#define	PCI_PRODUCT_ATI_RADEON_JHX800	0x4a48		/* Radeon X800 */
#define	PCI_PRODUCT_ATI_RADEON_X800PRO	0x4a49		/* Radeon X800 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X800SE	0x4a4a		/* Radeon X800SE */
#define	PCI_PRODUCT_ATI_RADEON_X800XT	0x4a4b		/* Radeon X800XT */
#define	PCI_PRODUCT_ATI_RADEON_X800	0x4a4c		/* Radeon X800 */
#define	PCI_PRODUCT_ATI_FIREGL_X3256	0x4a4d		/* FireGL X3-256 */
#define	PCI_PRODUCT_ATI_MOBILITY_M18	0x4a4e		/* Radeon Mobility M18 */
#define	PCI_PRODUCT_ATI_RADEON_JOX800SE	0x4a4f		/* Radeon X800 SE */
#define	PCI_PRODUCT_ATI_RADEON_X800XTPE	0x4a50		/* Radeon X800 XT */
#define	PCI_PRODUCT_ATI_RADEON_AIW_X800VE	0x4a54		/* Radeon AIW X800 VE */
#define	PCI_PRODUCT_ATI_RADEON_X800PRO_S	0x4a69		/* Radeon X800 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_X850XT	0x4b49		/* Radeon X850 XT */
#define	PCI_PRODUCT_ATI_RADEON_X850SE	0x4b4a		/* Radeon X850 SE */
#define	PCI_PRODUCT_ATI_RADEON_X850PRO	0x4b4b		/* Radeon X850 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X850XTPE	0x4b4c		/* Radeon X850 XT PE */
#define	PCI_PRODUCT_ATI_MACH64_LB	0x4c42		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_LD	0x4c44		/* Mach64 */
#define	PCI_PRODUCT_ATI_RAGE128_LE	0x4c45		/* Rage128 */
#define	PCI_PRODUCT_ATI_MOBILITY_M3	0x4c46		/* Mobility M3 */
#define	PCI_PRODUCT_ATI_MACH64_LG	0x4c47		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_LI	0x4c49		/* Mach64 */
#define	PCI_PRODUCT_ATI_MOBILITY_1	0x4c4d		/* Mobility 1 */
#define	PCI_PRODUCT_ATI_MACH64_LN	0x4c4e		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_LP	0x4c50		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_LQ	0x4c51		/* Mach64 */
#define	PCI_PRODUCT_ATI_RAGE_PM	0x4c52		/* Rage P/M */
#define	PCI_PRODUCT_ATI_MACH64LS	0x4c53		/* Mach64 */
#define	PCI_PRODUCT_ATI_RADEON_M7LW	0x4c57		/* Radeon Mobility M7 */
#define	PCI_PRODUCT_ATI_FIREGL_M7	0x4c58		/* FireGL Mobility 7800 M7 */
#define	PCI_PRODUCT_ATI_RADEON_M6LY	0x4c59		/* Radeon Mobility M6 */
#define	PCI_PRODUCT_ATI_RADEON_M6LZ	0x4c5a		/* Radeon Mobility M6 */
#define	PCI_PRODUCT_ATI_RADEON_M9LD	0x4c64		/* Radeon Mobility M9 */
#define	PCI_PRODUCT_ATI_RADEON_M9Lf	0x4c66		/* Radeon Mobility M9 */
#define	PCI_PRODUCT_ATI_RADEON_M9Lg	0x4c66		/* Radeon Mobility M9 */
#define	PCI_PRODUCT_ATI_RAGE128_MF	0x4d46		/* Rage 128 Mobility */
#define	PCI_PRODUCT_ATI_RAGE128_ML	0x4d4c		/* Rage 128 Mobility */
#define	PCI_PRODUCT_ATI_R300	0x4e44		/* Radeon 9500/9700 */
#define	PCI_PRODUCT_ATI_RADEON9500_PRO	0x4e45		/* Radeon 9500 Pro */
#define	PCI_PRODUCT_ATI_RADEON9600TX	0x4e46		/* Radeon 9600 TX */
#define	PCI_PRODUCT_ATI_FIREGL_X1	0x4e47		/* FireGL X1 */
#define	PCI_PRODUCT_ATI_R350	0x4e48		/* Radeon 9800 Pro */
#define	PCI_PRODUCT_ATI_RADEON9800	0x4e49		/* RAdeon 9800 */
#define	PCI_PRODUCT_ATI_RADEON_9800XT	0x4e4a		/* Radeon 9800 XT */
#define	PCI_PRODUCT_ATI_FIREGL_X2	0x4e4b		/* FireGL X2 */
#define	PCI_PRODUCT_ATI_RV350	0x4e50		/* Radeon Mobility M10 */
#define	PCI_PRODUCT_ATI_RV350NQ	0x4e51		/* Radeon Mobility M10 */
#define	PCI_PRODUCT_ATI_RV350NR	0x4e52		/* Radeon Mobility M10 */
#define	PCI_PRODUCT_ATI_RV350NS	0x4e53		/* Radeon Mobility M10 */
#define	PCI_PRODUCT_ATI_RV350_WS	0x4e54		/* Radeon Mobility M10 */
#define	PCI_PRODUCT_ATI_MOBILITY_9550	0x4e56		/* Radeon Mobility 9550 */
#define	PCI_PRODUCT_ATI_R300_S	0x4e64		/* Radeon 9500/9700 Sec */
#define	PCI_PRODUCT_ATI_R350_S	0x4e68		/* Radeon 9800 Pro Sec */
#define	PCI_PRODUCT_ATI_RAGE128_PA	0x5041		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_PB	0x5042		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_PC	0x5043		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_PD	0x5044		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_PE	0x5045		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE_FURY	0x5046		/* Rage Fury */
#define	PCI_PRODUCT_ATI_RAGE128_PG	0x5047		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PH	0x5048		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PI	0x5049		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PJ	0x504a		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PK	0x504b		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PL	0x504c		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PM	0x504d		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PN	0x504e		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PO	0x504f		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PP	0x5050		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PQ	0x5051		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PR	0x5052		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PS	0x5053		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PT	0x5054		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PU	0x5055		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PV	0x5056		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PW	0x5057		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_PX	0x5058		/* Rage 128 PX */
#define	PCI_PRODUCT_ATI_RADEON_AIW	0x5144		/* AIW Radeon */
#define	PCI_PRODUCT_ATI_RADEON_QE	0x5145		/* Radeon */
#define	PCI_PRODUCT_ATI_RADEON_QF	0x5146		/* Radeon */
#define	PCI_PRODUCT_ATI_RADEON_QG	0x5147		/* Radeon */
#define	PCI_PRODUCT_ATI_RADEON_QH	0x5148		/* Radeon */
#define	PCI_PRODUCT_ATI_R200_QL	0x514c		/* Radeon 8500 */
#define	PCI_PRODUCT_ATI_R200_QM	0x514d		/* Radeon 9100 */
#define	PCI_PRODUCT_ATI_R200_QN	0x514e		/* Radeon 8500 */
#define	PCI_PRODUCT_ATI_R200_QO	0x514f		/* Radeon 8500 */
#define	PCI_PRODUCT_ATI_RV200_QW	0x5157		/* Radeon 7500 */
#define	PCI_PRODUCT_ATI_RV200_QX	0x5158		/* Radeon 7500 */
#define	PCI_PRODUCT_ATI_RADEON_QY	0x5159		/* Radeon VE */
#define	PCI_PRODUCT_ATI_RADEON_QZ	0x515a		/* Radeon VE */
#define	PCI_PRODUCT_ATI_ES1000	0x515e		/* ES1000 */
#define	PCI_PRODUCT_ATI_R200_Ql	0x516c		/* Radeon 8500 */
#define	PCI_PRODUCT_ATI_RAGE128_GL	0x5245		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE_MAGNUM	0x5246		/* Rage Magnum */
#define	PCI_PRODUCT_ATI_RAGE128_RG	0x5247		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_RK	0x524b		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_VR	0x524c		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_SH	0x5348		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_SK	0x534b		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_SL	0x534c		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_SM	0x534d		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128	0x534e		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_TF	0x5446		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_TL	0x544c		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RAGE128_TR	0x5452		/* Rage 128 Pro */
#define	PCI_PRODUCT_ATI_RADEON_M300_M22	0x5460		/* Radeon Mobility M300 M22 */
#define	PCI_PRODUCT_ATI_RADEON_X600_M24C	0x5462		/* Radeon Mobility X600 M24C */
#define	PCI_PRODUCT_ATI_FIREGL_M44	0x5464		/* FireGL M44 GL 5464 */
#define	PCI_PRODUCT_ATI_RADEON_X800_RV423	0x5548		/* Radeon X800 */
#define	PCI_PRODUCT_ATI_RADEON_X800PRORV423	0x5549		/* Radeon X800 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X800XT_RV423	0x554a		/* Radeon X800 XT PE */
#define	PCI_PRODUCT_ATI_RADEON_X800SE_RV423	0x554b		/* Radeon X800 SE */
#define	PCI_PRODUCT_ATI_RADEON_X800XTPRV430	0x554c		/* Radeon X800 XTP */
#define	PCI_PRODUCT_ATI_RADEON_X800XL_RV430	0x554d		/* Radeon X800 XL */
#define	PCI_PRODUCT_ATI_RADEON_X800SE_RV430	0x554e		/* Radeon X800 SE */
#define	PCI_PRODUCT_ATI_RADEON_X800_RV430	0x554f		/* Radeon X800 */
#define	PCI_PRODUCT_ATI_FIREGL_V7100_RV423	0x5550		/* FireGL V7100 */
#define	PCI_PRODUCT_ATI_FIREGL_V5100_RV423	0x5551		/* FireGL V5100 */
#define	PCI_PRODUCT_ATI_FIREGL_UR_RV423	0x5552		/* FireGL */
#define	PCI_PRODUCT_ATI_FIREGL_UT_RV423	0x5553		/* FireGL */
#define	PCI_PRODUCT_ATI_RADEON_X800_RV430_S	0x556d		/* Radeon X800 Sec */
#define	PCI_PRODUCT_ATI_FIREGL_V5000_M26	0x564a		/* Mobility FireGL V5000 M26 */
#define	PCI_PRODUCT_ATI_FIREGL_V5000_M26b	0x564b		/* Mobility FireGL V5000 M26 */
#define	PCI_PRODUCT_ATI_RADEON_X700XL_M26	0x564f		/* Radeon Mobility X700 XL M26 */
#define	PCI_PRODUCT_ATI_RADEON_X700_M26_1	0x5652		/* Radeon Mobility X700 M26 */
#define	PCI_PRODUCT_ATI_RADEON_X700_M26_2	0x5653		/* Radeon Mobility X700 M26 */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x5654		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_VU	0x5655		/* Mach64 */
#define	PCI_PRODUCT_ATI_MACH64_VV	0x5656		/* Mach64 */
#define	PCI_PRODUCT_ATI_RADEON_X550XTX	0x5657		/* Radeon X550XTX */
#define	PCI_PRODUCT_ATI_RADEON_X550XTX_S	0x5677		/* Radeon X550XTX Sec */
#define	PCI_PRODUCT_ATI_RS300_100_HB	0x5830		/* RS300_100 Host */
#define	PCI_PRODUCT_ATI_RS300_133_HB	0x5831		/* RS300_133 Host */
#define	PCI_PRODUCT_ATI_RS300_166_HB	0x5832		/* RS300_166 Host */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100_HB	0x5833		/* Radeon IGP 9100 Host */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100_IGP	0x5834		/* Radeon IGP 9100 */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100	0x5835		/* Radeon Mobility IGP 9100 */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100_AGP	0x5838		/* Radeon IGP 9100 AGP */
#define	PCI_PRODUCT_ATI_RADEON_RV280_PRO_S	0x5940		/* Radeon 9200 PRO Sec */
#define	PCI_PRODUCT_ATI_RADEON_RV280_S	0x5941		/* Radeon 9200 Sec */
#define	PCI_PRODUCT_ATI_RS480_HB	0x5950		/* RS480 Host */
#define	PCI_PRODUCT_ATI_RX480_HB	0x5951		/* RX480 Host */
#define	PCI_PRODUCT_ATI_RD580_HB	0x5952		/* RD580 Host */
#define	PCI_PRODUCT_ATI_RADEON_RS480	0x5954		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS480_B	0x5955		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RX780_HB	0x5957		/* RX780 Host */
#define	PCI_PRODUCT_ATI_RD780_HT_GFX	0x5958		/* RD780 HT-PCIE */
#define	PCI_PRODUCT_ATI_RADEON_RV280_PRO	0x5960		/* Radeon 9200 PRO */
#define	PCI_PRODUCT_ATI_RADEON_RV280	0x5961		/* Radeon 9200 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_B	0x5962		/* Radeon 9200 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_SE_S	0x5964		/* Radeon 9200 SE Sec */
#define	PCI_PRODUCT_ATI_FIREMV_2200	0x5965		/* FireMV 2200 */
#define	PCI_PRODUCT_ATI_ES1000_1	0x5969		/* ES1000 */
#define	PCI_PRODUCT_ATI_RADEON_RS482	0x5974		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS482_B	0x5975		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RD790_PCIE_1	0x5978		/* RD790 PCIE */
#define	PCI_PRODUCT_ATI_RD790_PCIE_3	0x597a		/* RD790 PCIE */
#define	PCI_PRODUCT_ATI_RD790_PCIE_2	0x597c		/* RD790 PCIE */
#define	PCI_PRODUCT_ATI_RD790_PCIE_4	0x597f		/* RD790 PCIE */
#define	PCI_PRODUCT_ATI_RX200_HB	0x5a33		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RX480_PCIE	0x5a34		/* RX480 PCIE */
#define	PCI_PRODUCT_ATI_RS480_PCIE_2	0x5a36		/* RS480 PCIE */
#define	PCI_PRODUCT_ATI_RS480_PCIE_3	0x5a37		/* RS480 PCIE */
#define	PCI_PRODUCT_ATI_RX480_PCIE_2	0x5a38		/* RX480 PCIE */
#define	PCI_PRODUCT_ATI_RX480_PCIE_3	0x5a39		/* RX480 PCIE */
#define	PCI_PRODUCT_ATI_RS480_PCIE_1	0x5a3f		/* RS480 PCIE */
#define	PCI_PRODUCT_ATI_RADEON_RS400	0x5a41		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS400_B	0x5a42		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_RC410	0x5a61		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RC410_B	0x5a62		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_X300	0x5b60		/* Radeon X300 */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV370	0x5b62		/* Radeon X600 (RV370) */
#define	PCI_PRODUCT_ATI_RADEON_X550	0x5b63		/* Radeon X550 */
#define	PCI_PRODUCT_ATI_FIREGL_RV370	0x5b64		/* FireGL V3100 */
#define	PCI_PRODUCT_ATI_FIREMV_2200_5B65	0x5b65		/* FireMV 2200 5B65 */
#define	PCI_PRODUCT_ATI_RADEON_X300_S	0x5b70		/* Radeon X300 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV370_S	0x5b72		/* Radeon X600 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X550_S	0x5b73		/* Radeon X550 Sec */
#define	PCI_PRODUCT_ATI_FIREGL_RV370_S	0x5b74		/* FireGL V3100 Sec */
#define	PCI_PRODUCT_ATI_FIREMV_2200_S	0x5b75		/* FireMV 2200 Sec */
#define	PCI_PRODUCT_ATI_RADEON_RV280_M	0x5c61		/* Radeon Mobility 9200 */
#define	PCI_PRODUCT_ATI_RADEON_M9PLUS	0x5c63		/* Radeon Mobility 9200 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_SE	0x5d44		/* Radeon 9200 SE */
#define	PCI_PRODUCT_ATI_RADEON_X800XT_M28	0x5d48		/* Radeon X800 XT M28 */
#define	PCI_PRODUCT_ATI_FIREGL_V5100_M28	0x5d49		/* FireGL V5100 M28 */
#define	PCI_PRODUCT_ATI_MOBILITY_X800_M28	0x5d4a		/* Radeon Mobility X800 M28 */
#define	PCI_PRODUCT_ATI_RADEON_X850_R480	0x5d4c		/* Radeon X850 */
#define	PCI_PRODUCT_ATI_RADEON_X850XTPER480	0x5d4d		/* Radeon X850 XT PE */
#define	PCI_PRODUCT_ATI_RADEON_X850SE_R480	0x5d4e		/* Radeon X850 SE */
#define	PCI_PRODUCT_ATI_RADEON_X800_GTO	0x5d4f		/* Radeon X800 */
#define	PCI_PRODUCT_ATI_FIREGL_R480	0x5d50		/* FireGL R480 */
#define	PCI_PRODUCT_ATI_RADEON_X850XT_R480	0x5d52		/* Radeon X850XT */
#define	PCI_PRODUCT_ATI_RADEON_X800XT_R423	0x5d57		/* Radeon X800XT */
#define	PCI_PRODUCT_ATI_RADEON_X800_GTO_S	0x5d6f		/* Radeon X800 GTO Sec */
#define	PCI_PRODUCT_ATI_FIREGL_V5000_R410	0x5e48		/* FireGL V5000 */
#define	PCI_PRODUCT_ATI_RADEON_X700XT_R410	0x5e4a		/* FireGL X700 XT */
#define	PCI_PRODUCT_ATI_RADEON_X700PRO_R410	0x5e4b		/* FireGL X700 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X700SE_R410	0x5e4c		/* FireGL X700 SE */
#define	PCI_PRODUCT_ATI_RADEON_X700_PCIE	0x5e4d		/* Radeon X700 PCIE */
#define	PCI_PRODUCT_ATI_RADEON_X700SE_PCIE	0x5e4f		/* Radeon X700 SE PCIE */
#define	PCI_PRODUCT_ATI_RADEON_X700_PCIE_S	0x5e6d		/* Radeon X700 PCIE Sec */
#define	PCI_PRODUCT_ATI_RADEON_X700_SE	0x5e4f		/* Radeon X700 SE */
#define	PCI_PRODUCT_ATI_RADEON_X700_SE_S	0x5e6f		/* Radeon X700 SE Sec */
#define	PCI_PRODUCT_ATI_RADEON_HD5800	0x6899		/* Radeon HD 5800 */
#define	PCI_PRODUCT_ATI_RADEON_HD5700	0x68b8		/* Radeon HD 5700 */
#define	PCI_PRODUCT_ATI_RS100_PCI	0x700f		/* RS100 PCI */
#define	PCI_PRODUCT_ATI_RS200_PCI	0x7010		/* RS200 PCI */
#define	PCI_PRODUCT_ATI_RADEON_X1800A	0x7100		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_RADEON_X1800XT	0x7101		/* Radeon X1800 XT */
#define	PCI_PRODUCT_ATI_MOBILITY_X1800	0x7102		/* Radeon Mobility X1800 */
#define	PCI_PRODUCT_ATI_FIREGL_M_V7200	0x7103		/* FireGL Mobility V7200 */
#define	PCI_PRODUCT_ATI_FIREGL_V7200	0x7104		/* FireGL V7200 */
#define	PCI_PRODUCT_ATI_FIREGL_V5300	0x7105		/* FireGL V5300 */
#define	PCI_PRODUCT_ATI_FIREGL_M_V7100	0x7106		/* FireGL Mobility V7100 */
#define	PCI_PRODUCT_ATI_RADEON_X1800B	0x7108		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_RADEON_X1800C	0x7109		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_RADEON_X1800D	0x710a		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_RADEON_X1800E	0x710b		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_RADEON_X1800F	0x710c		/* Radeon X1800 */
#define	PCI_PRODUCT_ATI_FIREGL_V7300	0x710e		/* FireGL V7300 */
#define	PCI_PRODUCT_ATI_FIREGL_V7350	0x710f		/* FireGL V7350 */
#define	PCI_PRODUCT_ATI_RADEON_X1600	0x7140		/* Radeon X1600 */
#define	PCI_PRODUCT_ATI_RV505_1	0x7141		/* RV505 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_X1550	0x7142		/* Radeon X1300/X1550 */
#define	PCI_PRODUCT_ATI_RADEON_X1550	0x7143		/* Radeon X1550 */
#define	PCI_PRODUCT_ATI_M54_GL	0x7144		/* M54-GL */
#define	PCI_PRODUCT_ATI_RADEON_X1400	0x7145		/* Radeon Mobility X1400 */
#define	PCI_PRODUCT_ATI_RADEON_X1550_X1300	0x7146		/* Radeon X1300/X1550 */
#define	PCI_PRODUCT_ATI_RADEON_X1550_64	0x7147		/* RADEON X1550 64-bit */
#define	PCI_PRODUCT_ATI_RADEON_X1300_M52	0x7149		/* Radeon Mobility X1300 M52-64 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1300_4A	0x714a		/* Radeon Mobility X1300 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1300_4B	0x714b		/* Radeon Mobility X1300 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1300_4C	0x714c		/* Radeon Mobility X1300 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_4D	0x714d		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_4E	0x714e		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RV505_2	0x714f		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RV505_3	0x7151		/* RV505 */
#define	PCI_PRODUCT_ATI_FIREGL_V3300	0x7152		/* FireGL V3300 */
#define	PCI_PRODUCT_ATI_FIREGL_V3350	0x7153		/* FireGL V3350 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_5E	0x715e		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RADEON_X1550_64_2	0x715f		/* Radeon X1550 */
#define	PCI_PRODUCT_ATI_RADEON_X1600_S	0x7160		/* Radeon X1600 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1300_X1550_S	0x7162		/* Radeon X1300/X1550 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1300X1550	0x7180		/* Radeon X1300/X1550 */
#define	PCI_PRODUCT_ATI_RADEON_X1600_81	0x7181		/* Radeon X1600 */
#define	PCI_PRODUCT_ATI_RADEON_X1300PRO	0x7183		/* Radeon X1300 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X1450	0x7186		/* Radeon X1450 */
#define	PCI_PRODUCT_ATI_RADEON_X1300	0x7187		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RADEON_X2300	0x7188		/* Radeon Mobility X2300 */
#define	PCI_PRODUCT_ATI_RADEON_X2300_2	0x718a		/* Radeon Mobility X2300 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1350	0x718b		/* Radeon Mobility X1350 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1350_2	0x718c		/* Radeon Mobility X1350 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1450	0x718d		/* Radeon Mobility X1450 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_8F	0x718f		/* Radeon X1300 */
#define	PCI_PRODUCT_ATI_RADEON_X1550_2	0x7193		/* Radeon X1550 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1350_3	0x7196		/* Radeon Mobility X1350 */
#define	PCI_PRODUCT_ATI_FIREMV_2250	0x719b		/* FireMV 2250 */
#define	PCI_PRODUCT_ATI_RADEON_X1550_64_3	0x719f		/* Radeon X1550 64-bit */
#define	PCI_PRODUCT_ATI_RADEON_X1300PRO_S	0x71a3		/* Radeon X1300 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1300_S	0x71a7		/* Radeon X1300 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1600_C0	0x71c0		/* Radeon X1600 */
#define	PCI_PRODUCT_ATI_RADEON_X1650	0x71c1		/* Radeon X1650 */
#define	PCI_PRODUCT_ATI_RADEON_X1600_PRO	0x71c2		/* Radeon X1600 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X1600_C3	0x71c3		/* Radeon X1600 */
#define	PCI_PRODUCT_ATI_FIREGL_V5200	0x71c4		/* FireGL V5200 */
#define	PCI_PRODUCT_ATI_RADEON_X1600_M	0x71c5		/* Radeon Mobility X1600 */
#define	PCI_PRODUCT_ATI_RADEON_X1650_PRO	0x71c6		/* Radeon X1650 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X1650_PRO2	0x71c7		/* Radeon X1650 Pro */
#define	PCI_PRODUCT_ATI_RADEON_X1600_CD	0x71cd		/* Radeon X1600 */
#define	PCI_PRODUCT_ATI_RADEON_X1300_XT	0x71ce		/* Radeon X1300 XT */
#define	PCI_PRODUCT_ATI_FIREGL_V3400	0x71d2		/* FireGL V3400 */
#define	PCI_PRODUCT_ATI_RV530_M56	0x71d4		/* Mobility FireGL V5250 */
#define	PCI_PRODUCT_ATI_RADEON_X1700	0x71d5		/* Radeon X1700 */
#define	PCI_PRODUCT_ATI_RADEON_X1700XT	0x71d6		/* Radeon X1700 XT */
#define	PCI_PRODUCT_ATI_FIREGL_V5200_1	0x71da		/* FireGL V5200 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1700	0x71de		/* Radeon Mobility X1700 */
#define	PCI_PRODUCT_ATI_RADEON_X1600_PRO_S	0x71e2		/* Radeon X1600 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1650_PRO_S	0x71e6		/* Radeon X1650 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_X1650_PRO2_S	0x71e7		/* Radeon X1650 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_X2300HD	0x7200		/* Radeon X2300HD */
#define	PCI_PRODUCT_ATI_MOBILITY_X2300HD	0x7210		/* Radeon Mobility X2300HD */
#define	PCI_PRODUCT_ATI_MOBILITY_X2300HD_1	0x7211		/* Radeon Mobility X2300HD */
#define	PCI_PRODUCT_ATI_RADEON_X1950_40	0x7240		/* Radeon X1950 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_43	0x7243		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1950_44	0x7244		/* Radeon X1950 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_45	0x7245		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_46	0x7246		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_47	0x7247		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_48	0x7248		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_49	0x7249		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_4A	0x724a		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_4B	0x724b		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_4C	0x724c		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1900_4D	0x724d		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_STREAM_PROCESSOR	0x724e		/* AMD Stream Processor */
#define	PCI_PRODUCT_ATI_RADEON_X1900_4F	0x724f		/* Radeon X1900 */
#define	PCI_PRODUCT_ATI_RADEON_X1950_PRO	0x7280		/* Radeon X1950 Pro */
#define	PCI_PRODUCT_ATI_RV560	0x7281		/* RV560 */
#define	PCI_PRODUCT_ATI_RV560_1	0x7283		/* RV560 */
#define	PCI_PRODUCT_ATI_MOBILITY_X1900	0x7284		/* Radeon Mobility X1900 */
#define	PCI_PRODUCT_ATI_RV560_2	0x7287		/* RV560 */
#define	PCI_PRODUCT_ATI_RADEON_X1950GT	0x7288		/* Radeon X1950 GT */
#define	PCI_PRODUCT_ATI_RV570	0x7289		/* RV570 */
#define	PCI_PRODUCT_ATI_RV570_2	0x728b		/* RV570 */
#define	PCI_PRODUCT_ATI_FIREGL_V7400	0x728c		/* FireGL V7400 */
#define	PCI_PRODUCT_ATI_RV560_3	0x7290		/* Rv560 */
#define	PCI_PRODUCT_ATI_RADEON_RX1650_XT	0x7291		/* Radeon RX1650 XT */
#define	PCI_PRODUCT_ATI_RADEON_X1650_1	0x7293		/* Radeon X1650 */
#define	PCI_PRODUCT_ATI_RV560_4	0x7297		/* RV560 */
#define	PCI_PRODUCT_ATI_RADEON_X1950_PRO_S	0x72a0		/* Radeon X1950 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_RX1650_XT_2	0x72b1		/* Radeon RX1650 XT Sec */
#define	PCI_PRODUCT_ATI_RADEON_9000IGP	0x7834		/* Radeon 9000/9100 IGP */
#define	PCI_PRODUCT_ATI_RADEON_RS350IGP	0x7835		/* Radeon RS350IGP */
#define	PCI_PRODUCT_ATI_RS690_HB	0x7910		/* RS690 Host */
#define	PCI_PRODUCT_ATI_RS740_HB	0x7911		/* RS740 Host */
#define	PCI_PRODUCT_ATI_RS690_PCIE_1	0x7912		/* RS690 PCIE */
#define	PCI_PRODUCT_ATI_RS690M_PCIE_1	0x7913		/* RS690M PCIE */
#define	PCI_PRODUCT_ATI_RS690_PCIE_2	0x7915		/* RS690 PCIE */
#define	PCI_PRODUCT_ATI_RS690_PCIE_4	0x7916		/* RS690 PCIE */
#define	PCI_PRODUCT_ATI_RS690_PCIE_5	0x7917		/* RS690 PCIE */
#define	PCI_PRODUCT_ATI_RS690_HDA	0x7919		/* RS690 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_X1250	0x791e		/* Radeon X1250 */
#define	PCI_PRODUCT_ATI_RADEON_X1250IGP	0x791f		/* Radeon X1250 IGP */
#define	PCI_PRODUCT_ATI_RADEON_2100	0x796e		/* Radeon 2100 */
#define	PCI_PRODUCT_ATI_RADEON_HD4870	0x9440		/* Radeon HD 4870 */
#define	PCI_PRODUCT_ATI_RADEON_HD4850	0x9442		/* Radeon HD 4850 */
#define	PCI_PRODUCT_ATI_RADEON_HD4890	0x9460		/* Radeon HD 4890 */
#define	PCI_PRODUCT_ATI_RADEON_HD4670	0x9490		/* Radeon HD 4670 */
#define	PCI_PRODUCT_ATI_RADEON_HD4650	0x9498		/* Radeon HD 4650 */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_XT	0x94c1		/* Radeon HD 2400 XT */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_PRO	0x94c3		/* Radeon HD 2400 Pro */
#define	PCI_PRODUCT_ATI_RADEON_HD2400_M72	0x94c9		/* Mobility Radeon HD 2400 */
#define	PCI_PRODUCT_ATI_RADEON_HD3870	0x9501		/* Radeon HD 3870 */
#define	PCI_PRODUCT_ATI_RADEON_HD3850	0x9505		/* Radeon HD 3850 */
#define	PCI_PRODUCT_ATI_RADEON_HD4550	0x9540		/* Radeon HD 4550 */
#define	PCI_PRODUCT_ATI_RADEON_HD4350	0x954f		/* Radeon HD 4350 */
#define	PCI_PRODUCT_ATI_RADEON_HD4500_M	0x9553		/* Mobility Radeon HD 4500 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_M76	0x9581		/* Mobility Radeon HD 2600 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600PROAGP	0x9587		/* Radeon HD 2600 Pro AGP */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_PRO	0x9589		/* Radeon HD 2600 Pro */
#define	PCI_PRODUCT_ATI_RADEON_HD3650_M	0x9591		/* Mobility Radeon HD 3650 */
#define	PCI_PRODUCT_ATI_RADEON_HD3650_AGP	0x9596		/* Radeon HD 3650 AGP */
#define	PCI_PRODUCT_ATI_RADEON_HD3650	0x9598		/* Radeon HD 3650 */
#define	PCI_PRODUCT_ATI_RADEON_HD3400_M82	0x95c4		/* Mobility Radeon HD 3400 */
#define	PCI_PRODUCT_ATI_RADEON_HD3450	0x95c5		/* Radeon HD 3450 */
#define	PCI_PRODUCT_ATI_RS780_HB	0x9600		/* RS780 Host */
#define	PCI_PRODUCT_ATI_RS780_PCIE_1	0x9602		/* RS780 PCIE */
#define	PCI_PRODUCT_ATI_RS780_PCIE_2	0x9609		/* RS780 PCIE */
#define	PCI_PRODUCT_ATI_RS780_HDA	0x960f		/* RS780 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD3200_1	0x9610		/* Radeon HD 3200 */
#define	PCI_PRODUCT_ATI_RADEON_HD3100	0x9611		/* Radeon HD 3100 */
#define	PCI_PRODUCT_ATI_RADEON_HD3200_2	0x9612		/* Radeon HD 3200 */
#define	PCI_PRODUCT_ATI_RADEON_HD3300	0x9614		/* Radeon HD 3300 */
#define	PCI_PRODUCT_ATI_RADEON_HD4200_HDA	0x970f		/* Radeon HD 4200 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD4200	0x9710		/* Radeon HD 4200 */
#define	PCI_PRODUCT_ATI_RADEON_HD2600_HDA	0xaa08		/* Radeon HD 2600 HD Audio */
#define	PCI_PRODUCT_ATI_RS690M_HDA	0xaa10		/* RS690M HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD3870_HDA	0x0018		/* Radeon HD 3870 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD3600_HDA	0xaa20		/* Radeon HD 3600 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD34xx_HDA	0xaa28		/* Radeon HD 34xx HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD48xx_HDA	0xaa30		/* Radeon HD 48xx HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD4000_HDA	0xaa38		/* Radeon HD 4000 HD Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD5800_HDA	0xaa50		/* Radeon HD 5800 Audio */
#define	PCI_PRODUCT_ATI_RADEON_HD5700_HDA	0xaa58		/* Radeon HD 5700 Audio */
#define	PCI_PRODUCT_ATI_RS100_AGP	0xcab0		/* RS100 AGP */
#define	PCI_PRODUCT_ATI_RS200_AGP	0xcab2		/* RS200 AGP */
#define	PCI_PRODUCT_ATI_RS250_AGP	0xcab3		/* RS250 AGP */
#define	PCI_PRODUCT_ATI_RS200M_AGP	0xcbb2		/* RS200M AGP */

/* Applied Micro Circuits products */
#define	PCI_PRODUCT_AMCIRCUITS_S5933	0x4750		/* S5933 PCI Matchmaker */
#define	PCI_PRODUCT_AMCIRCUITS_LANAI	0x8043		/* Myrinet LANai */

/* ASPEED Technology products */
#define	PCI_PRODUCT_ASPEED_AST2000	0x2000		/* AST2000 */

/* Atheros products */
#define	PCI_PRODUCT_ATHEROS_AR5210	0x0007		/* AR5210 */
#define	PCI_PRODUCT_ATHEROS_AR5311	0x0011		/* AR5311 */
#define	PCI_PRODUCT_ATHEROS_AR5211	0x0012		/* AR5211 */
#define	PCI_PRODUCT_ATHEROS_AR5212	0x0013		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_2	0x0014		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_3	0x0015		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_4	0x0016		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_5	0x0017		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_6	0x0018		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR5212_7	0x0019		/* AR5212 */
#define	PCI_PRODUCT_ATHEROS_AR2413	0x001a		/* AR2413 */
#define	PCI_PRODUCT_ATHEROS_AR5413	0x001b		/* AR5413 */
#define	PCI_PRODUCT_ATHEROS_AR5424	0x001c		/* AR5424 */
#define	PCI_PRODUCT_ATHEROS_AR5416	0x0023		/* AR5416 */
#define	PCI_PRODUCT_ATHEROS_AR5418	0x0024		/* AR5418 */
#define	PCI_PRODUCT_ATHEROS_AR9160	0x0027		/* AR9160 */
#define	PCI_PRODUCT_ATHEROS_AR9280	0x0029		/* AR9280 */
#define	PCI_PRODUCT_ATHEROS_AR9281	0x002a		/* AR9281 */
#define	PCI_PRODUCT_ATHEROS_AR9285	0x002b		/* AR9285 */
#define	PCI_PRODUCT_ATHEROS_AR2427	0x002c		/* AR2427 */
#define	PCI_PRODUCT_ATHEROS_AR9227	0x002d		/* AR9227 */
#define	PCI_PRODUCT_ATHEROS_AR9287	0x002e		/* AR9287 */
#define	PCI_PRODUCT_ATHEROS_AR5210_AP	0x0207		/* AR5210 (Early) */
#define	PCI_PRODUCT_ATHEROS_AR5212_IBM	0x1014		/* AR5212 (IBM MiniPCI) */
#define	PCI_PRODUCT_ATHEROS_AR5210_DEFAULT	0x1107		/* AR5210 (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_DEFAULT	0x1113		/* AR5212 (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5211_DEFAULT	0x1112		/* AR5211 (no eeprom) */
#define	PCI_PRODUCT_ATHEROS_AR5212_FPGA	0xf013		/* AR5212 (emulation board) */
#define	PCI_PRODUCT_ATHEROS_AR5211_FPGA11B	0xf11b		/* AR5211Ref */
#define	PCI_PRODUCT_ATHEROS_AR5211_LEGACY	0xff12		/* AR5211Ref */

/* Atronics products */
#define	PCI_PRODUCT_ATRONICS_IDE_2015PL	0x2015		/* IDE-2015PL */

/* Attansic Technology products */
#define	PCI_PRODUCT_ATTANSIC_L1E	0x1026		/* L1E */
#define	PCI_PRODUCT_ATTANSIC_L1	0x1048		/* L1 */
#define	PCI_PRODUCT_ATTANSIC_L2C	0x1062		/* L2C */
#define	PCI_PRODUCT_ATTANSIC_L1C	0x1063		/* L1C */
#define	PCI_PRODUCT_ATTANSIC_L2	0x2048		/* L2 */

/* Aureal products */
#define	PCI_PRODUCT_AUREAL_AU8820	0x0001		/* Vortex 1 */
#define	PCI_PRODUCT_AUREAL_AU8830	0x0002		/* Vortex 2 */
#define	PCI_PRODUCT_AUREAL_AU8810	0x0003		/* Vortex Advantage */

/* Avance Logic products */
#define	PCI_PRODUCT_AVANCE_AVL2301	0x2301		/* AVL2301 */
#define	PCI_PRODUCT_AVANCE_AVG2302	0x2302		/* AVG2302 */
#define	PCI_PRODUCT_AVANCE2_ALG2301	0x2301		/* ALG2301 */
#define	PCI_PRODUCT_AVANCE2_ALG2302	0x2302		/* ALG2302 */
#define	PCI_PRODUCT_AVANCE2_ALS4000	0x4000		/* ALS4000 */

/* AVlab products */
#define	PCI_PRODUCT_AVLAB_PCI2S	0x2130		/* Dual Serial */
#define	PCI_PRODUCT_AVLAB_LPPCI4S	0x2150		/* Quad Serial */
#define	PCI_PRODUCT_AVLAB_LPPCI4S_2	0x2152		/* Quad Serial */

/* AVM products */
#define	PCI_PRODUCT_AVM_B1	0x0700		/* BRI ISDN */
#define	PCI_PRODUCT_AVM_FRITZ_CARD	0x0a00		/* Fritz ISDN */
#define	PCI_PRODUCT_AVM_FRITZ_PCI_V2_ISDN	0x0e00		/* Fritz v2.0 ISDN */
#define	PCI_PRODUCT_AVM_T1	0x1200		/* PRI T1 ISDN */

/* AWT products */
#define	PCI_PRODUCT_AWT_RT2890	0x1059		/* RT2890 */

/* Belkin Components products */
#define	PCI_PRODUCT_BELKIN2_F5D6001	0x6001		/* F5D6001 */
#define	PCI_PRODUCT_BELKIN2_F5D6020V3	0x6020		/* F5D6020V3 */
#define	PCI_PRODUCT_BELKIN2_F5D7010	0x701f		/* F5D7010 */
#define	PCI_PRODUCT_BELKIN_F5D6000	0xec00		/* F5D6000 */

/* Bit3 products */
#define	PCI_PRODUCT_BIT3_PCIVME617	0x0001		/* VME 617 */
#define	PCI_PRODUCT_BIT3_PCIVME2706	0x0300		/* VME 2706 */

/* Bluesteel Networks */
#define	PCI_PRODUCT_BLUESTEEL_5501	0x0000		/* 5501 */
#define	PCI_PRODUCT_BLUESTEEL_5601	0x5601		/* 5601 */

/* Broadcom */
#define	PCI_PRODUCT_BROADCOM_BCM5752	0x1600		/* BCM5752 */
#define	PCI_PRODUCT_BROADCOM_BCM5752M	0x1601		/* BCM5752M */
#define	PCI_PRODUCT_BROADCOM_BCM5709	0x1639		/* BCM5709 */
#define	PCI_PRODUCT_BROADCOM_BCM5709S	0x163a		/* BCM5709S */
#define	PCI_PRODUCT_BROADCOM_BCM5716	0x163b		/* BCM5716 */
#define	PCI_PRODUCT_BROADCOM_BCM5716S	0x163c		/* BCM5716S */
#define	PCI_PRODUCT_BROADCOM_BCM5700	0x1644		/* BCM5700 */
#define	PCI_PRODUCT_BROADCOM_BCM5701	0x1645		/* BCM5701 */
#define	PCI_PRODUCT_BROADCOM_BCM5702	0x1646		/* BCM5702 */
#define	PCI_PRODUCT_BROADCOM_BCM5703	0x1647		/* BCM5703 */
#define	PCI_PRODUCT_BROADCOM_BCM5704C	0x1648		/* BCM5704C */
#define	PCI_PRODUCT_BROADCOM_BCM5704S_ALT	0x1649		/* BCM5704S Alt */
#define	PCI_PRODUCT_BROADCOM_BCM5706	0x164a		/* BCM5706 */
#define	PCI_PRODUCT_BROADCOM_BCM5708	0x164c		/* BCM5708 */
#define	PCI_PRODUCT_BROADCOM_BCM5702FE	0x164d		/* BCM5702FE */
#define	PCI_PRODUCT_BROADCOM_BCM57710	0x164e		/* BCM57710 */
#define	PCI_PRODUCT_BROADCOM_BCM57711	0x164f		/* BCM57711 */
#define	PCI_PRODUCT_BROADCOM_BCM57711E	0x1650		/* BCM57711E */
#define	PCI_PRODUCT_BROADCOM_BCM5705	0x1653		/* BCM5705 */
#define	PCI_PRODUCT_BROADCOM_BCM5705K	0x1654		/* BCM5705K */
#define	PCI_PRODUCT_BROADCOM_BCM5717	0x1655		/* BCM5717 */
#define	PCI_PRODUCT_BROADCOM_BCM5718	0x1656		/* BCM5718 */
#define	PCI_PRODUCT_BROADCOM_BCM5720	0x1658		/* BCM5720 */
#define	PCI_PRODUCT_BROADCOM_BCM5721	0x1659		/* BCM5721 */
#define	PCI_PRODUCT_BROADCOM_BCM5722	0x165a		/* BCM5722 */
#define	PCI_PRODUCT_BROADCOM_BCM5723	0x165b		/* BCM5723 */
#define	PCI_PRODUCT_BROADCOM_BCM5724	0x165c		/* BCM5724 */
#define	PCI_PRODUCT_BROADCOM_BCM5705M	0x165d		/* BCM5705M */
#define	PCI_PRODUCT_BROADCOM_BCM5705M_ALT	0x165e		/* BCM5705M Alt */
#define	PCI_PRODUCT_BROADCOM_BCM5714	0x1668		/* BCM5714 */
#define	PCI_PRODUCT_BROADCOM_BCM5714S	0x1669		/* BCM5714S */
#define	PCI_PRODUCT_BROADCOM_BCM5780	0x166a		/* BCM5780 */
#define	PCI_PRODUCT_BROADCOM_BCM5780S	0x166b		/* BCM5780S */
#define	PCI_PRODUCT_BROADCOM_BCM5705F	0x166e		/* BCM5705F */
#define	PCI_PRODUCT_BROADCOM_BCM5754M	0x1672		/* BCM5754M */
#define	PCI_PRODUCT_BROADCOM_BCM5755M	0x1673		/* BCM5755M */
#define	PCI_PRODUCT_BROADCOM_BCM5756	0x1674		/* BCM5756 */
#define	PCI_PRODUCT_BROADCOM_BCM5750	0x1676		/* BCM5750 */
#define	PCI_PRODUCT_BROADCOM_BCM5751	0x1677		/* BCM5751 */
#define	PCI_PRODUCT_BROADCOM_BCM5715	0x1678		/* BCM5715 */
#define	PCI_PRODUCT_BROADCOM_BCM5715S	0x1679		/* BCM5715S */
#define	PCI_PRODUCT_BROADCOM_BCM5754	0x167a		/* BCM5754 */
#define	PCI_PRODUCT_BROADCOM_BCM5755	0x167b		/* BCM5755 */
#define	PCI_PRODUCT_BROADCOM_BCM5750M	0x167c		/* BCM5750M */
#define	PCI_PRODUCT_BROADCOM_BCM5751M	0x167d		/* BCM5751M */
#define	PCI_PRODUCT_BROADCOM_BCM5751F	0x167e		/* BCM5751F */
#define	PCI_PRODUCT_BROADCOM_BCM5787F	0x167f		/* BCM5787F */
#define	PCI_PRODUCT_BROADCOM_BCM5761E	0x1680		/* BCM5761E */
#define	PCI_PRODUCT_BROADCOM_BCM5761	0x1681		/* BCM5761 */
#define	PCI_PRODUCT_BROADCOM_BCM5764	0x1684		/* BCM5764 */
#define	PCI_PRODUCT_BROADCOM_BCM5761S	0x1688		/* BCM5761S */
#define	PCI_PRODUCT_BROADCOM_BCM5761SE	0x1689		/* BCM5761SE */
#define	PCI_PRODUCT_BROADCOM_BCM57760	0x1690		/* BCM57760 */
#define	PCI_PRODUCT_BROADCOM_BCM57788	0x1691		/* BCM57788 */
#define	PCI_PRODUCT_BROADCOM_BCM57780	0x1692		/* BCM57780 */
#define	PCI_PRODUCT_BROADCOM_BCM5787M	0x1693		/* BCM5787M */
#define	PCI_PRODUCT_BROADCOM_BCM57790	0x1694		/* BCM57790 */
#define	PCI_PRODUCT_BROADCOM_BCM5782	0x1696		/* BCM5782 */
#define	PCI_PRODUCT_BROADCOM_BCM5784	0x1698		/* BCM5784 */
#define	PCI_PRODUCT_BROADCOM_BCM5785G	0x1699		/* BCM5785G */
#define	PCI_PRODUCT_BROADCOM_BCM5786	0x169a		/* BCM5786 */
#define	PCI_PRODUCT_BROADCOM_BCM5787	0x169b		/* BCM5787 */
#define	PCI_PRODUCT_BROADCOM_BCM5788	0x169c		/* BCM5788 */
#define	PCI_PRODUCT_BROADCOM_BCM5789	0x169d		/* BCM5789 */
#define	PCI_PRODUCT_BROADCOM_BCM5785F	0x16a0		/* BCM5785F */
#define	PCI_PRODUCT_BROADCOM_BCM5702X	0x16a6		/* BCM5702X */
#define	PCI_PRODUCT_BROADCOM_BCM5703X	0x16a7		/* BCM5703X */
#define	PCI_PRODUCT_BROADCOM_BCM5704S	0x16a8		/* BCM5704S */
#define	PCI_PRODUCT_BROADCOM_BCM5706S	0x16aa		/* BCM5706S */
#define	PCI_PRODUCT_BROADCOM_BCM5708S	0x16ac		/* BCM5708S */
#define	PCI_PRODUCT_BROADCOM_BCM57761	0x16b0		/* BCM57761 */
#define	PCI_PRODUCT_BROADCOM_BCM57781	0x16b1		/* BCM57781 */
#define	PCI_PRODUCT_BROADCOM_BCM57791	0x16b2		/* BCM57791 */
#define	PCI_PRODUCT_BROADCOM_BCM57765	0x16b4		/* BCM57765 */
#define	PCI_PRODUCT_BROADCOM_BCM57785	0x16b5		/* BCM57785 */
#define	PCI_PRODUCT_BROADCOM_BCM57795	0x16b6		/* BCM57795 */
#define	PCI_PRODUCT_BROADCOM_BCM5702_ALT	0x16c6		/* BCM5702 Alt */
#define	PCI_PRODUCT_BROADCOM_BCM5703_ALT	0x16c7		/* BCM5703 Alt */
#define	PCI_PRODUCT_BROADCOM_BCM5781	0x16dd		/* BCM5781 */
#define	PCI_PRODUCT_BROADCOM_BCM5753	0x16f7		/* BCM5753 */
#define	PCI_PRODUCT_BROADCOM_BCM5753M	0x16fd		/* BCM5753M */
#define	PCI_PRODUCT_BROADCOM_BCM5753F	0x16fe		/* BCM5753F */
#define	PCI_PRODUCT_BROADCOM_BCM5903M	0x16ff		/* BCM5903M */
#define	PCI_PRODUCT_BROADCOM_BCM4401B1	0x170c		/* BCM4401B1 */
#define	PCI_PRODUCT_BROADCOM_BCM5901	0x170d		/* BCM5901 */
#define	PCI_PRODUCT_BROADCOM_BCM5901A2	0x170e		/* BCM5901A2 */
#define	PCI_PRODUCT_BROADCOM_BCM5903F	0x170f		/* BCM5903F */
#define	PCI_PRODUCT_BROADCOM_BCM5906	0x1712		/* BCM5906 */
#define	PCI_PRODUCT_BROADCOM_BCM5906M	0x1713		/* BCM5906M */
#define	PCI_PRODUCT_BROADCOM_BCM4303	0x4301		/* BCM4303 */
#define	PCI_PRODUCT_BROADCOM_BCM4307	0x4307		/* BCM4307 */
#define	PCI_PRODUCT_BROADCOM_BCM4311	0x4311		/* BCM4311 */
#define	PCI_PRODUCT_BROADCOM_BCM4312	0x4312		/* BCM4312 */
#define	PCI_PRODUCT_BROADCOM_BCM4315	0x4315		/* BCM4315 */
#define	PCI_PRODUCT_BROADCOM_BCM4318	0x4318		/* BCM4318 */
#define	PCI_PRODUCT_BROADCOM_BCM4319	0x4319		/* BCM4319 */
#define	PCI_PRODUCT_BROADCOM_BCM4306	0x4320		/* BCM4306 */
#define	PCI_PRODUCT_BROADCOM_BCM4306_2	0x4321		/* BCM4306 */
#define	PCI_PRODUCT_BROADCOM_SERIAL_2	0x4322		/* Serial */
#define	PCI_PRODUCT_BROADCOM_BCM4309	0x4324		/* BCM4309 */
#define	PCI_PRODUCT_BROADCOM_BCM43XG	0x4325		/* BCM43XG */
#define	PCI_PRODUCT_BROADCOM_BCM4321	0x4328		/* BCM4321 */
#define	PCI_PRODUCT_BROADCOM_BCM4321_2	0x4329		/* BCM4321 */
#define	PCI_PRODUCT_BROADCOM_BCM4322	0x432b		/* BCM4322 */
#define	PCI_PRODUCT_BROADCOM_SERIAL	0x4333		/* Serial */
#define	PCI_PRODUCT_BROADCOM_SERIAL_GC	0x4344		/* Serial */
#define	PCI_PRODUCT_BROADCOM_BCM4401	0x4401		/* BCM4401 */
#define	PCI_PRODUCT_BROADCOM_BCM4401B0	0x4402		/* BCM4401B0 */
#define	PCI_PRODUCT_BROADCOM_5801	0x5801		/* 5801 */
#define	PCI_PRODUCT_BROADCOM_5802	0x5802		/* 5802 */
#define	PCI_PRODUCT_BROADCOM_5805	0x5805		/* 5805 */
#define	PCI_PRODUCT_BROADCOM_5820	0x5820		/* 5820 */
#define	PCI_PRODUCT_BROADCOM_5821	0x5821		/* 5821 */
#define	PCI_PRODUCT_BROADCOM_5822	0x5822		/* 5822 */
#define	PCI_PRODUCT_BROADCOM_5823	0x5823		/* 5823 */
#define	PCI_PRODUCT_BROADCOM_5825	0x5825		/* 5825 */
#define	PCI_PRODUCT_BROADCOM_5860	0x5860		/* 5860 */
#define	PCI_PRODUCT_BROADCOM_5861	0x5861		/* 5861 */
#define	PCI_PRODUCT_BROADCOM_5862	0x5862		/* 5862 */

/* Brooktree products */
#define	PCI_PRODUCT_BROOKTREE_BT848	0x0350		/* BT848 */
#define	PCI_PRODUCT_BROOKTREE_BT849	0x0351		/* BT849 */
#define	PCI_PRODUCT_BROOKTREE_BT878	0x036e		/* BT878 */
#define	PCI_PRODUCT_BROOKTREE_BT879	0x036f		/* BT879 */
#define	PCI_PRODUCT_BROOKTREE_BT878_AU	0x0878		/* BT878 Audio */
#define	PCI_PRODUCT_BROOKTREE_BT879_AU	0x0879		/* BT879 Audio */
#define	PCI_PRODUCT_BROOKTREE_BT8474	0x8474		/* Bt8474 HDLC */

/* BusLogic products */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER_NC	0x0140		/* MultiMaster NC */
#define	PCI_PRODUCT_BUSLOGIC_MULTIMASTER	0x1040		/* MultiMaster */
#define	PCI_PRODUCT_BUSLOGIC_FLASHPOINT	0x8130		/* FlashPoint */

/* c't Magazin products */
#define	PCI_PRODUCT_C4T_GPPCI	0x6773		/* GPPCI */

/* Cavium products */
#define	PCI_PRODUCT_CAVIUM_NITROX	0x0001		/* NITROX XL */
#define	PCI_PRODUCT_CAVIUM_NITROX_LITE	0x0003		/* NITROX Lite */
#define	PCI_PRODUCT_CAVIUM_NITROX_PX	0x0010		/* NITROX PX */

/* CCUBE products */
#define	PCI_PRODUCT_CCUBE_CINEMASTER	0x8888		/* Cinemaster */

/* Chelsio products */
#define	PCI_PRODUCT_CHELSIO_Nx10	0x0006		/* Nx10 10GbE */
#define	PCI_PRODUCT_CHELSIO_PE9000	0x0020		/* PE9000 10GbE */
#define	PCI_PRODUCT_CHELSIO_T302E	0x0021		/* T302E 10GbE */
#define	PCI_PRODUCT_CHELSIO_T310E	0x0022		/* T310E 10GbE */
#define	PCI_PRODUCT_CHELSIO_T320X	0x0023		/* T320X 10GbE */
#define	PCI_PRODUCT_CHELSIO_T302X	0x0024		/* T302X 10GbE */
#define	PCI_PRODUCT_CHELSIO_T320E	0x0025		/* T320E 10GbE */
#define	PCI_PRODUCT_CHELSIO_T310X	0x0026		/* T310X 10GbE */
#define	PCI_PRODUCT_CHELSIO_T3B10	0x0030		/* T3B10 10GbE */
#define	PCI_PRODUCT_CHELSIO_T3B20	0x0031		/* T3B20 10GbE */
#define	PCI_PRODUCT_CHELSIO_T3B02	0x0032		/* T3B02 10GbE */

/* Chips and Technologies products */
#define	PCI_PRODUCT_CHIPS_64310	0x00b8		/* 64310 */
#define	PCI_PRODUCT_CHIPS_65545	0x00d8		/* 65545 */
#define	PCI_PRODUCT_CHIPS_65548	0x00dc		/* 65548 */
#define	PCI_PRODUCT_CHIPS_65550	0x00e0		/* 65550 */
#define	PCI_PRODUCT_CHIPS_65554	0x00e4		/* 65554 */
#define	PCI_PRODUCT_CHIPS_65555	0x00e5		/* 65555 */
#define	PCI_PRODUCT_CHIPS_68554	0x00f4		/* 68554 */
#define	PCI_PRODUCT_CHIPS_69000	0x00c0		/* 69000 */
#define	PCI_PRODUCT_CHIPS_69030	0x0c30		/* 69030 */

/* Cirrus Logic products */
#define	PCI_PRODUCT_CIRRUS_CL_GD7548	0x0038		/* CL-GD7548 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5430	0x00a0		/* CL-GD5430 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_4	0x00a4		/* CL-GD5434-4 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5434_8	0x00a8		/* CL-GD5434-8 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5436	0x00ac		/* CL-GD5436 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5446	0x00b8		/* CL-GD5446 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5480	0x00bc		/* CL-GD5480 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5462	0x00d0		/* CL-GD5462 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5464	0x00d4		/* CL-GD5464 */
#define	PCI_PRODUCT_CIRRUS_CL_GD5465	0x00d6		/* CL-GD5465 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6729	0x1100		/* CL-PD6729 */
#define	PCI_PRODUCT_CIRRUS_CL_PD6832	0x1110		/* CL-PD6832 CardBus */
#define	PCI_PRODUCT_CIRRUS_CL_PD6833	0x1113		/* CL-PD6833 CardBus */
#define	PCI_PRODUCT_CIRRUS_CL_GD7542	0x1200		/* CL-GD7542 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7543	0x1202		/* CL-GD7543 */
#define	PCI_PRODUCT_CIRRUS_CL_GD7541	0x1204		/* CL-GD7541 */
#define	PCI_PRODUCT_CIRRUS_CS4610	0x6001		/* CS4610 SoundFusion */
#define	PCI_PRODUCT_CIRRUS_CS4615	0x6004		/* CS4615 */
#define	PCI_PRODUCT_CIRRUS_CS4280	0x6003		/* CS4280/46xx CrystalClear */
#define	PCI_PRODUCT_CIRRUS_CS4281	0x6005		/* CS4281 CrystalClear */

/* CMD Technology products -- info gleaned from www.cmd.com */
/* Fake product id for SiI3112 found on Adaptec 1210SA */
#define	PCI_PRODUCT_CMDTECH_AAR_1210SA	0x0240		/* AAR-1210SA */
/* Adaptec 1220SA is really a 3132 also */
#define	PCI_PRODUCT_CMDTECH_AAR_1220SA	0x0242		/* AAR-1220SA */
#define	PCI_PRODUCT_CMDTECH_AAR_1225SA	0x0244		/* AAR-1225SA */
#define	PCI_PRODUCT_CMDTECH_640	0x0640		/* PCI0640 */
#define	PCI_PRODUCT_CMDTECH_642	0x0642		/* PCI0642 */
#define	PCI_PRODUCT_CMDTECH_643	0x0643		/* PCI0643 */
#define	PCI_PRODUCT_CMDTECH_646	0x0646		/* PCI0646 */
#define	PCI_PRODUCT_CMDTECH_647	0x0647		/* PCI0647 */
#define	PCI_PRODUCT_CMDTECH_648	0x0648		/* PCI0648 */
#define	PCI_PRODUCT_CMDTECH_649	0x0649		/* PCI0649 */
/* Inclusion of 'A' in the following entry is probably wrong. */
/* No data on the CMD Tech. web site for the following as of Mar. 3 '98 */
#define	PCI_PRODUCT_CMDTECH_650A	0x0650		/* PCI0650A */
#define	PCI_PRODUCT_CMDTECH_670	0x0670		/* USB0670 */
#define	PCI_PRODUCT_CMDTECH_673	0x0673		/* USB0673 */
#define	PCI_PRODUCT_CMDTECH_680	0x0680		/* PCI0680 */
#define	PCI_PRODUCT_CMDTECH_3112	0x3112		/* SiI3112 SATA */
#define	PCI_PRODUCT_CMDTECH_3114	0x3114		/* SiI3114 SATA */
#define	PCI_PRODUCT_CMDTECH_3124	0x3124		/* SiI3124 SATA */
#define	PCI_PRODUCT_CMDTECH_3131	0x3131		/* SiI3131 SATA */
#define	PCI_PRODUCT_CMDTECH_3132	0x3132		/* SiI3132 SATA */
#define	PCI_PRODUCT_CMDTECH_3512	0x3512		/* SiI3512 SATA */
#define	PCI_PRODUCT_CMDTECH_3531	0x3531		/* SiI3531 SATA */

/* CNet produts */
#define	PCI_PRODUCT_CNET_GIGACARD	0x434e		/* GigaCard */

/* Cogent Data Technologies products */
#define	PCI_PRODUCT_COGENT_EM110TX	0x1400		/* EX110TX */

/* Compaq products */
#define	PCI_PRODUCT_COMPAQ_PCI_EISA_BRIDGE	0x0001		/* EISA */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE	0x0002		/* ISA */
#define	PCI_PRODUCT_COMPAQ_CSA64XX	0x0046		/* Smart Array 64xx */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX1	0x1000		/* Triflex PCI */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX2	0x2000		/* Triflex PCI */
#define	PCI_PRODUCT_COMPAQ_QVISION_V0	0x3032		/* QVision */
#define	PCI_PRODUCT_COMPAQ_QVISION_1280P	0x3033		/* QVision 1280/p */
#define	PCI_PRODUCT_COMPAQ_QVISION_V2	0x3034		/* QVision */
#define	PCI_PRODUCT_COMPAQ_TRIFLEX4	0x4000		/* Triflex PCI */
#define	PCI_PRODUCT_COMPAQ_CSA5300	0x4070		/* Smart Array 5300 */
#define	PCI_PRODUCT_COMPAQ_CSA5i	0x4080		/* Smart Array 5i */
#define	PCI_PRODUCT_COMPAQ_CSA532	0x4082		/* Smart Array 532 */
#define	PCI_PRODUCT_COMPAQ_CSA5312	0x4083		/* Smart Array 5312 */
#define	PCI_PRODUCT_COMPAQ_CSA6i	0x4091		/* Smart Array 6i */
#define	PCI_PRODUCT_COMPAQ_CSA641	0x409a		/* Smart Array 641 */
#define	PCI_PRODUCT_COMPAQ_CSA642	0x409b		/* Smart Array 642 */
#define	PCI_PRODUCT_COMPAQ_CSA6400	0x409c		/* Smart Array 6400 */
#define	PCI_PRODUCT_COMPAQ_CSA6400EM	0x409d		/* Smart Array 6400 EM */
#define	PCI_PRODUCT_COMPAQ_CSA6422	0x409e		/* Smart Array 6422 */
#define	PCI_PRODUCT_COMPAQ_HOTPLUG_PCI	0x6010		/* Hotplug PCI */
#define	PCI_PRODUCT_COMPAQ_USB	0x7020		/* USB */
#define	PCI_PRODUCT_COMPAQ_FXP	0xa0f0		/* Netelligent ASMC */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE1	0xa0f3		/* ISA */
#define	PCI_PRODUCT_COMPAQ_PCI_HOTPLUG	0xa0f7		/* PCI Hotplug */
#define	PCI_PRODUCT_COMPAQ_OHCI	0xa0f8		/* USB OpenHost */
#define	PCI_PRODUCT_COMPAQ_SMART2P	0xae10		/* SMART2P RAID */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE3	0xae29		/* ISA */
#define	PCI_PRODUCT_COMPAQ_PCI_ISAPNP	0xae2b		/* ISAPnP */
#define	PCI_PRODUCT_COMPAQ_N100TX	0xae32		/* Netelligent 10/100TX */
#define	PCI_PRODUCT_COMPAQ_IDE	0xae33		/* Netelligent IDE */
#define	PCI_PRODUCT_COMPAQ_N10T	0xae34		/* Netelligent 10 T */
#define	PCI_PRODUCT_COMPAQ_IntNF3P	0xae35		/* Integrated NetFlex 3/P */
#define	PCI_PRODUCT_COMPAQ_DPNet100TX	0xae40		/* DP Netelligent 10/100TX */
#define	PCI_PRODUCT_COMPAQ_IntPL100TX	0xae43		/* ProLiant Netelligent 10/100TX */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE2	0xae69		/* ISA */
#define	PCI_PRODUCT_COMPAQ_HOST_PCI_BRIDGE1	0xae6c		/* PCI */
#define	PCI_PRODUCT_COMPAQ_HOST_PCI_BRIDGE2	0xae6d		/* PCI */
#define	PCI_PRODUCT_COMPAQ_DP4000	0xb011		/* Embedded Netelligent 10/100TX */
#define	PCI_PRODUCT_COMPAQ_N10T2	0xb012		/* Netelligent 10 T/2 PCI */
#define	PCI_PRODUCT_COMPAQ_N10_TX_UTP	0xb030		/* Netelligent 10/100TX */
#define	PCI_PRODUCT_COMPAQ_CSA5300_2	0xb060		/* Smart Array 5300 rev.2 */
#define	PCI_PRODUCT_COMPAQ_CSA5i_2	0xb178		/* Smart Array 5i/532 rev.2 */
#define	PCI_PRODUCT_COMPAQ_ILO_1	0xb203		/* iLO */
#define	PCI_PRODUCT_COMPAQ_ILO_2	0xb204		/* iLO */
#define	PCI_PRODUCT_COMPAQ_NF3P	0xf130		/* NetFlex 3/P */
#define	PCI_PRODUCT_COMPAQ_NF3P_BNC	0xf150		/* NetFlex 3/PB */

/* Compex */
#define	PCI_PRODUCT_COMPEX_COMPEXE	0x1401		/* Compexe */
#define	PCI_PRODUCT_COMPEX_RL100ATX	0x2011		/* RL100-ATX 10/100 */
#define	PCI_PRODUCT_COMPEX_98713	0x9881		/* PMAC 98713 */

/* Conexant products */
#define	PCI_PRODUCT_CONEXANT_56K_WINMODEM	0x1033		/* 56k Winmodem */
#define	PCI_PRODUCT_CONEXANT_56K_WINMODEM2	0x1036		/* 56k Winmodem */
#define	PCI_PRODUCT_CONEXANT_RS7112	0x1803		/* 10/100 */
#define	PCI_PRODUCT_CONEXANT_56K_WINMODEM3	0x1804		/* 10/100 */
#define	PCI_PRODUCT_CONEXANT_SOFTK56_PCI	0x2443		/* SoftK56 PCI */
#define	PCI_PRODUCT_CONEXANT_HSF_56K_HSFI	0x2f00		/* HSF 56k HSFi */
#define	PCI_PRODUCT_CONEXANT_MUSYCC8478	0x8478		/* MUSYCC CN8478 */
#define	PCI_PRODUCT_CONEXANT_MUSYCC8474	0x8474		/* MUSYCC CN8474 */
#define	PCI_PRODUCT_CONEXANT_MUSYCC8472	0x8472		/* MUSYCC CN8472 */
#define	PCI_PRODUCT_CONEXANT_MUSYCC8471	0x8471		/* MUSYCC CN8471 */
#define	PCI_PRODUCT_CONEXANT_CX2388x	0x8800		/* CX2388x */
#define	PCI_PRODUCT_CONEXANT_CX2388x_AUDIO	0x8801		/* CX2388x Audio */
#define	PCI_PRODUCT_CONEXANT_CX2388x_MPEG	0x8802		/* CX2388x MPEG */
#define	PCI_PRODUCT_CONEXANT_CX2388x_IR	0x8804		/* CX2388x IR */
#define	PCI_PRODUCT_CONEXANT_CX2388x_AUDIO2	0x8811		/* CX2388x Audio */

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 VLB */
#define	PCI_PRODUCT_CONTAQ_82C693	0xc693		/* CY82C693U ISA */

/* Corega products */
#define	PCI_PRODUCT_COREGA_CB_TXD	0xa117		/* FEther CB-TXD 10/100 */
#define	PCI_PRODUCT_COREGA_2CB_TXD	0xa11e		/* FEther II CB-TXD 10/100 */
#define	PCI_PRODUCT_COREGA_CGLAPCIGT	0xc107		/* CG-LAPCIGT */

/* Corollary products */
#define	PCI_PRODUCT_COROLLARY_CBUSII_PCIB	0x0014		/* C-Bus II-PCI */
#define	PCI_PRODUCT_COROLLARY_CCF	0x1117		/* Cache Coherency Filter */

/* Creative Labs products */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE	0x0002		/* SoundBlaster Live */
#define	PCI_PRODUCT_CREATIVELABS_AWE64D	0x0003		/* SoundBlaster AWE64D */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGY	0x0004		/* SoundBlaster Audigy */
#define	PCI_PRODUCT_CREATIVELABS_XFI	0x0005		/* SoundBlaster X-Fi */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE2	0x0006		/* SoundBlaster Live (Dell) */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGYLS	0x0007		/* SoundBlaster Audigy LS */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGY2	0x0008		/* SoundBlaster Audigy 2 */
#define	PCI_PRODUCT_CREATIVELABS_XFI_XTREME	0x0009		/* SoundBlaster X-Fi Xtreme */
#define	PCI_PRODUCT_CREATIVELABS_FIWIRE	0x4001		/* Firewire */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY	0x7002		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGIN	0x7003		/* SoundBlaster Audigy Digital */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY2	0x7004		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY3	0x7005		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_PPB	0x7006		/* PCIE-PCI */
#define	PCI_PRODUCT_CREATIVELABS_EV1938	0x8938		/* Ectiva 1938 */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_1	0x0102		/* Cyclom-4Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM4Y_2	0x0103		/* Cyclom-4Y */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_1	0x0104		/* Cyclom-8Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOM8Y_2	0x0105		/* Cyclom-8Y */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_1	0x0200		/* Cyclom-Z below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMZ_2	0x0201		/* Cyclom-Z */

/* Cyclone Microsystems products */
#define	PCI_PRODUCT_CYCLONE_PCI_700	0x0700		/* IQ80310 */

/* Cyrix/National Semiconductor products */
#define	PCI_PRODUCT_CYRIX_CX5510	0x0000		/* Cx5510 */
#define	PCI_PRODUCT_CYRIX_GXMPCI	0x0001		/* GXm PCI */
#define	PCI_PRODUCT_CYRIX_GXMISA	0x0002		/* GXm ISA */
#define	PCI_PRODUCT_CYRIX_CX5530_PCIB	0x0100		/* Cx5530 South */
#define	PCI_PRODUCT_CYRIX_CX5530_SMI	0x0101		/* Cx5530 SMI */
#define	PCI_PRODUCT_CYRIX_CX5530_IDE	0x0102		/* Cx5530 IDE */
#define	PCI_PRODUCT_CYRIX_CX5530_AUDIO	0x0103		/* Cx5530 XpressAUDIO */
#define	PCI_PRODUCT_CYRIX_CX5530_VIDEO	0x0104		/* Cx5530 Video */

/* Davicom Technologies */
#define	PCI_PRODUCT_DAVICOM_DM9009	0x9009		/* DM9009 */
#define	PCI_PRODUCT_DAVICOM_DM9100	0x9100		/* DM9100 */
#define	PCI_PRODUCT_DAVICOM_DM9102	0x9102		/* DM9102 */
#define	PCI_PRODUCT_DAVICOM_DM9132	0x9132		/* DM9132 */

/* Decision Computer Inc */
#define	PCI_PRODUCT_DCI_APCI2	0x0004		/* PCCOM 2-port */
#define	PCI_PRODUCT_DCI_APCI4	0x0001		/* PCCOM 4-port */
#define	PCI_PRODUCT_DCI_APCI8	0x0002		/* PCCOM 8-port */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* 21050 PCI-PCI */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* 21040 */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* 21030 */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* 21140 */
#define	PCI_PRODUCT_DEC_PBXGB	0x000d		/* TGA2 */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* 21041 */
#define	PCI_PRODUCT_DEC_DGLPB	0x0016		/* DGLPB (OPPO) */
#define	PCI_PRODUCT_DEC_ZLXPL2	0x0017		/* ZLXP-L2 (Pixelvision) */
#define	PCI_PRODUCT_DEC_MC	0x0018		/* Memory Channel Cluster Controller */
#define	PCI_PRODUCT_DEC_21142	0x0019		/* 21142/3 */
/* Farallon apparently used DEC's vendor ID by mistake */
#define	PCI_PRODUCT_DEC_PN9000SX	0x001a		/* Farallon PN9000SX */
#define	PCI_PRODUCT_DEC_21052	0x0021		/* 21052 PCI-PCI */
#define	PCI_PRODUCT_DEC_21150	0x0022		/* 21150 PCI-PCI */
#define	PCI_PRODUCT_DEC_21150_BC	0x0023		/* 21150-BC PCI-PCI */
#define	PCI_PRODUCT_DEC_21152	0x0024		/* 21152 PCI-PCI */
#define	PCI_PRODUCT_DEC_21153	0x0025		/* 21153 PCI-PCI */
#define	PCI_PRODUCT_DEC_21154	0x0026		/* 21154 PCI-PCI */
#define	PCI_PRODUCT_DEC_21554	0x0046		/* 21554 PCI-PCI */
#define	PCI_PRODUCT_DEC_SWXCR	0x1065		/* SWXCR RAID */

/* Dell Computer products */
#define	PCI_PRODUCT_DELL_PERC_2SI	0x0001		/* PERC 2/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI	0x0002		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI	0x0003		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3SI_2	0x0004		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_DRAC_3_ADDIN	0x0007		/* DRAC 3 Add-in */
#define	PCI_PRODUCT_DELL_DRAC_3_VUART	0x0008		/* DRAC 3 Virtual UART */
#define	PCI_PRODUCT_DELL_DRAC_3_EMBD	0x0009		/* DRAC 3 Embedded/Optional */
#define	PCI_PRODUCT_DELL_PERC_3DI_3	0x000a		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_DRAC_4_EMBD	0x000c		/* DRAC 4 Embedded/Optional */
#define	PCI_PRODUCT_DELL_DRAC_3_OPT	0x000d		/* DRAC 3 Optional */
#define	PCI_PRODUCT_DELL_PERC_4DI	0x000e		/* PERC 4/Di i960 */
#define	PCI_PRODUCT_DELL_PERC_4DI_2	0x000f		/* PERC 4/Di Verde */
#define	PCI_PRODUCT_DELL_DRAC_4	0x0011		/* DRAC 4 */
#define	PCI_PRODUCT_DELL_DRAC_4_VUART	0x0012		/* DRAC 4 Virtual UART */
#define	PCI_PRODUCT_DELL_PERC_4EDI	0x0013		/* PERC 4e/Di */
#define	PCI_PRODUCT_DELL_DRAC_4_SMIC	0x0014		/* DRAC 4 SMIC */
#define	PCI_PRODUCT_DELL_PERC_3DI_2_SUB	0x00cf		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI_2_SUB	0x00d0		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB2	0x00d1		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB3	0x00d9		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB	0x0106		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB2	0x011b		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB3	0x0121		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC5	0x0015		/* PERC 5 */

/* Delta Electronics products */
#define	PCI_PRODUCT_DELTA_RHINEII	0x1320		/* RhineII */
#define	PCI_PRODUCT_DELTA_8139	0x1360		/* 8139 */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_VIPER	0x9001		/* Viper/PCI */

/* D-Link products */
#define	PCI_PRODUCT_DLINK_550TX	0x1002		/* 550TX */
#define	PCI_PRODUCT_DLINK_530TXPLUS	0x1300		/* 530TX+ */
#define	PCI_PRODUCT_DLINK_DFE690TXD	0x1340		/* DFE-690TXD */
#define	PCI_PRODUCT_DLINK_DRP32TXD	0x1561		/* DRP32TXD */
#define	PCI_PRODUCT_DLINK_DWL610	0x3300		/* DWL-610 */
#define	PCI_PRODUCT_DLINK_DGE550T	0x4000		/* DGE-550T */
#define	PCI_PRODUCT_DLINK_DGE550SX	0x4001		/* DGE-550SX */
#define	PCI_PRODUCT_DLINK_DGE528T	0x4300		/* DGE-528T */
#define	PCI_PRODUCT_DLINK_DGE560T	0x4b00		/* DGE-560T */
#define	PCI_PRODUCT_DLINK_DGE530T_B1	0x4b01		/* DGE-530T B1 */
#define	PCI_PRODUCT_DLINK_DGE560SX	0x4b02		/* DGE-560SX */
#define	PCI_PRODUCT_DLINK_DGE550T_B1	0x4b03		/* DGE-550T B1 */
#define	PCI_PRODUCT_DLINK_DGE530T_A1	0x4c00		/* DGE-530T A1 */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_MEMCTLR	0x1012		/* Memory Control */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/Raid */
#define	PCI_PRODUCT_DPT_I960_PPB	0xa500		/* PCI-PCI */
#define	PCI_PRODUCT_DPT_RAID_I2O	0xa501		/* SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_2005S	0xa511		/* SmartRAID 2005S */

/* Dolphin products */
#define	PCI_PRODUCT_DOLPHIN_PCISCI	0x0658		/* PCI-SCI */

/* DTC Technology Corp products */
#define	PCI_PRODUCT_DTCTECH_DMX3194U	0x0002		/* DMX3194U */

/* Dynalink products */
#define	PCI_PRODUCT_DYNALINK_IS64PH	0x1702		/* IS64PH ISDN */

/* Edimax products */
#define	PCI_PRODUCT_EDIMAX_RT2860_1	0x7708		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_2	0x7728		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_3	0x7758		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_4	0x7727		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_5	0x7738		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_6	0x7748		/* RT2860 */
#define	PCI_PRODUCT_EDIMAX_RT2860_7	0x7768		/* RT2860 */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_EFSS25	0x0005		/* 25SS-3010 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_SS1023	0x1023		/* SpeedStream 1023 */

/* ELSA products */
#define	PCI_PRODUCT_ELSA_QS1PCI	0x1000		/* QuickStep 1000 ISDN */

/* Emulex products */
#define	PCI_PRODUCT_EMULEX_LPFC	0x10df		/* LPFC */
#define	PCI_PRODUCT_EMULEX_LP6000	0x1ae5		/* LP6000 */
#define	PCI_PRODUCT_EMULEX_LPE121	0xf011		/* LPe121 */
#define	PCI_PRODUCT_EMULEX_LPE1250	0xf015		/* LPe1250 */
#define	PCI_PRODUCT_EMULEX_LP952	0xf095		/* LP952 */
#define	PCI_PRODUCT_EMULEX_LP982	0xf098		/* LP982 */
#define	PCI_PRODUCT_EMULEX_LP101	0xf0a1		/* LP101 */
#define	PCI_PRODUCT_EMULEX_LP1050	0xf0a5		/* LP1050 */
#define	PCI_PRODUCT_EMULEX_LP111	0xf0d1		/* LP111 */
#define	PCI_PRODUCT_EMULEX_LP1150	0xf0d5		/* LP1150 */
#define	PCI_PRODUCT_EMULEX_LPE111	0xf0e1		/* LPe111 */
#define	PCI_PRODUCT_EMULEX_LPE1150	0xf0e5		/* LPe1150 */
#define	PCI_PRODUCT_EMULEX_LPE1000	0xf0f5		/* LPe1000 */
#define	PCI_PRODUCT_EMULEX_LPE1000_SP	0xf0f6		/* LPe1000-SP */
#define	PCI_PRODUCT_EMULEX_LPE1002_SP	0xf0f7		/* LPe1002-SP */
#define	PCI_PRODUCT_EMULEX_LPE12000	0xf100		/* LPe12000 */
#define	PCI_PRODUCT_EMULEX_LPE12000_SP	0xf111		/* LPe12000-SP */
#define	PCI_PRODUCT_EMULEX_LPE12002_SP	0xf112		/* LPe12002-SP */
#define	PCI_PRODUCT_EMULEX_LP7000	0xf700		/* LP7000 */
#define	PCI_PRODUCT_EMULEX_LP8000	0xf800		/* LP8000 */
#define	PCI_PRODUCT_EMULEX_LP9000	0xf900		/* LP9000 */
#define	PCI_PRODUCT_EMULEX_LP9802	0xf980		/* LP9802 */
#define	PCI_PRODUCT_EMULEX_LP10000	0xfa00		/* LP10000 */
#define	PCI_PRODUCT_EMULEX_LPX10000	0xfb00		/* LPX10000 */
#define	PCI_PRODUCT_EMULEX_LP10000_S	0xfc00		/* LP10000-S */
#define	PCI_PRODUCT_EMULEX_LP11000_S	0xfc10		/* LP11000-S */
#define	PCI_PRODUCT_EMULEX_LPE11000_S	0xfc20		/* LPe11000-S */
#define	PCI_PRODUCT_EMULEX_LPE12000_S	0xfc40		/* LPe12000-S */
#define	PCI_PRODUCT_EMULEX_LP11000	0xfd00		/* LP11000 */
#define	PCI_PRODUCT_EMULEX_LP11000_SP	0xfd11		/* LP11000-SP */
#define	PCI_PRODUCT_EMULEX_LP11002_SP	0xfd12		/* LP11002-SP */
#define	PCI_PRODUCT_EMULEX_LPE11000	0xfe00		/* LPe11000 */
#define	PCI_PRODUCT_EMULEX_LPE11000_SP	0xfe11		/* LPe11000-SP */
#define	PCI_PRODUCT_EMULEX_LPE11002_SP	0xfe12		/* LPe11002-SP */

/* Endace Measurement Systems */
#define	PCI_PRODUCT_ENDACE_DAG35	0x3500		/* Endace Dag3.5 */
#define	PCI_PRODUCT_ENDACE_DAG36D	0x360d		/* Endace Dag3.6D */
#define	PCI_PRODUCT_ENDACE_DAG422GE	0x422e		/* Endace Dag4.22GE */
#define	PCI_PRODUCT_ENDACE_DAG423	0x4230		/* Endace Dag4.23 */
#define	PCI_PRODUCT_ENDACE_DAG423GE	0x423e		/* Endace Dag4.23GE */

/* ENE Technology products */
#define	PCI_PRODUCT_ENE_FLASH	0x0520		/* Flash memory */
#define	PCI_PRODUCT_ENE_MEMSTICK	0x0530		/* Memory Stick */
#define	PCI_PRODUCT_ENE_SDCARD	0x0550		/* SD Controller */
#define	PCI_PRODUCT_ENE_SDMMC	0x0551		/* SD/MMC */
#define	PCI_PRODUCT_ENE_CB1211	0x1211		/* CB-1211 CardBus */
#define	PCI_PRODUCT_ENE_CB1225	0x1225		/* CB-1225 CardBus */
#define	PCI_PRODUCT_ENE_CB1410	0x1410		/* CB-1410 CardBus */
#define	PCI_PRODUCT_ENE_CB710	0x1411		/* CB-710 CardBus */
#define	PCI_PRODUCT_ENE_CB712	0x1412		/* CB-712 CardBus */
#define	PCI_PRODUCT_ENE_CB1420	0x1420		/* CB-1420 CardBus */
#define	PCI_PRODUCT_ENE_CB720	0x1421		/* CB-720 CardBus */
#define	PCI_PRODUCT_ENE_CB722	0x1422		/* CB-722 CardBus */

/* Ensoniq products */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI97	0x1371		/* AudioPCI97 */
#define	PCI_PRODUCT_ENSONIQ_AUDIOPCI	0x5000		/* AudioPCI */
#define	PCI_PRODUCT_ENSONIQ_CT5880	0x5880		/* CT5880 */

/* ESS Technology products */
#define	PCI_PRODUCT_ESSTECH_ES336H	0x0000		/* ES366H Modem */
#define	PCI_PRODUCT_ESSTECH_MAESTROII	0x1968		/* Maestro II */
#define	PCI_PRODUCT_ESSTECH_SOLO1	0x1969		/* SOLO-1 AudioDrive */
#define	PCI_PRODUCT_ESSTECH_MAESTRO2E	0x1978		/* Maestro 2E */
#define	PCI_PRODUCT_ESSTECH_ES1989	0x1988		/* ES1989 */
#define	PCI_PRODUCT_ESSTECH_ES1989M	0x1989		/* ES1989 Modem */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3	0x1998		/* Maestro 3 */
#define	PCI_PRODUCT_ESSTECH_ES1983	0x1999		/* ES1983 Modem */
#define	PCI_PRODUCT_ESSTECH_MAESTRO3_2	0x199a		/* Maestro 3 Audio */
#define	PCI_PRODUCT_ESSTECH_ES336H_N	0x2808		/* ES366H Fax/Modem */
#define	PCI_PRODUCT_ESSTECH_SUPERLINK	0x2838		/* ES2838/2839 SuperLink Modem */
#define	PCI_PRODUCT_ESSTECH_2898	0x2898		/* ES2898 Modem */

/* Essential Communications products */
#define	PCI_PRODUCT_ESSENTIAL_RR_HIPPI	0x0001		/* RoadRunner HIPPI */
#define	PCI_PRODUCT_ESSENTIAL_RR_GIGE	0x0005		/* RoadRunner Gig-E */

/* Evans & Sutherland products */
#define	PCI_PRODUCT_ES_FREEDOM	0x0001		/* Freedom GBus */

/* Eumitcom Technology products */
#define	PCI_PRODUCT_EUMITCOM_WL11000P	0x1100		/* WL11000P */

/* Equinox Systems products */
#define	PCI_PRODUCT_EQUINOX_SST64	0x0808		/* SST-64P */
#define	PCI_PRODUCT_EQUINOX_SST128	0x1010		/* SST-128P */
#define	PCI_PRODUCT_EQUINOX_SST16A	0x80C0		/* SST-16P */
#define	PCI_PRODUCT_EQUINOX_SST16B	0x80C4		/* SST-16P */
#define	PCI_PRODUCT_EQUINOX_SST16C	0x80C8		/* SST-16P */
#define	PCI_PRODUCT_EQUINOX_SST4	0x8888		/* SST-4p */
#define	PCI_PRODUCT_EQUINOX_SST8	0x9090		/* SST-8p */

/* Exar products */
#define	PCI_PRODUCT_EXAR_XR17C152	0x0152		/* XR17C152 */
#define	PCI_PRODUCT_EXAR_XR17C154	0x0154		/* XR17C154 */
#define	PCI_PRODUCT_EXAR_XR17C158	0x0158		/* XR17C158 */

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Forte Media products */
#define	PCI_PRODUCT_FORTEMEDIA_FM801	0x0801		/* 801 Sound */

/* Freescale products */
#define	PCI_PRODUCT_FREESCALE_MPC8349E	0x0080		/* MPC8349E */
#define	PCI_PRODUCT_FREESCALE_MPC8349	0x0081		/* MPC8349 */
#define	PCI_PRODUCT_FREESCALE_MPC8347E_TBGA	0x0082		/* MPC8347E TBGA */
#define	PCI_PRODUCT_FREESCALE_MPC8347_TBGA	0x0083		/* MPC8347 TBGA */
#define	PCI_PRODUCT_FREESCALE_MPC8347E_PBGA	0x0084		/* MPC8347E PBGA */
#define	PCI_PRODUCT_FREESCALE_MPC8347_PBGA	0x0085		/* MPC8347 PBGA */
#define	PCI_PRODUCT_FREESCALE_MPC8343E	0x0086		/* MPC8343E */
#define	PCI_PRODUCT_FREESCALE_MPC8343	0x0087		/* MPC8343 */

/* Fujitsu products */
#define	PCI_PRODUCT_FUJITSU_PW008GE5	0x11a1		/* PW008GE5 */
#define	PCI_PRODUCT_FUJITSU_PW008GE4	0x11a2		/* PW008GE4 */
#define	PCI_PRODUCT_FUJITSU_PP250_450_LAN	0x11cc		/* PRIMEPOWER250/450 LAN */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* Global Sun Technology products */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P03	0x1100		/* GL24110P03 */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P	0x1101		/* GL24110P */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P02	0x1102		/* GL24110P02 */

/* Globespan products */
#define	PCI_PRODUCT_GLOBESPAN_G7370	0xd002		/* Pulsar G7370 ADSL */

/* Guillemot products */
#define	PCI_PRODUCT_GEMTEK_PR103	0x1001		/* PR103 */

/* Hauppauge Computer Works */
#define	PCI_PRODUCT_HAUPPAUGE_WINTV	0x13eb		/* WinTV */

/* Hawking products */
#define	PCI_PRODUCT_HAWKING_PN672TX	0xab08		/* PN672TX 10/100 */

/* Hewlett-Packard products */
#define	PCI_PRODUCT_HP_VISUALIZE_EG	0x1005		/* Visualize EG */
#define	PCI_PRODUCT_HP_VISUALIZE_FX6	0x1006		/* Visualize FX6 */
#define	PCI_PRODUCT_HP_VISUALIZE_FX4	0x1008		/* Visualize FX4 */
#define	PCI_PRODUCT_HP_VISUALIZE_FX2	0x100a		/* Visualize FX2 */
#define	PCI_PRODUCT_HP_TACH_TL	0x1028		/* Tach TL FibreChannel */
#define	PCI_PRODUCT_HP_TACH_XL2	0x1029		/* Tach XL2 FibreChannel */
#define	PCI_PRODUCT_HP_J2585A	0x1030		/* J2585A */
#define	PCI_PRODUCT_HP_J2585B	0x1031		/* J2585B */
#define	PCI_PRODUCT_HP_DIVA	0x1048		/* Diva Serial Multiport */
#define	PCI_PRODUCT_HP_ELROY	0x1054		/* Elroy Ropes-PCI */
#define	PCI_PRODUCT_HP_VISUALIZE_FXE	0x108b		/* Visualize FXe */
#define	PCI_PRODUCT_HP_TOPTOOLS	0x10c1		/* TopTools Communications Port */
#define	PCI_PRODUCT_HP_NETRAID_4M	0x10c2		/* NetRaid-4M */
#define	PCI_PRODUCT_HP_SMARTIRQ	0x10ed		/* NetServer SmartIRQ */
#define	PCI_PRODUCT_HP_82557B	0x1200		/* 82557B 10/100 NIC */
#define	PCI_PRODUCT_HP_PLUTO	0x1229		/* Pluto MIO */
#define	PCI_PRODUCT_HP_ZX1_IOC	0x122a		/* zx1 IOC */
#define	PCI_PRODUCT_HP_MERCURY	0x122e		/* Mercury Ropes-PCI */
#define	PCI_PRODUCT_HP_QUICKSILVER	0x12b4		/* QuickSilver Ropes-PCI */
#define	PCI_PRODUCT_HP_HPSAV100	0x3210		/* Smart Array V100 */
#define	PCI_PRODUCT_HP_HPSAE200I_1	0x3211		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200	0x3212		/* Smart Array E200 */
#define	PCI_PRODUCT_HP_HPSAE200I_2	0x3213		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_3	0x3214		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSAE200I_4	0x3215		/* Smart Array E200i */
#define	PCI_PRODUCT_HP_HPSA_1	0x3220		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_2	0x3222		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP800	0x3223		/* Smart Array P800 */
#define	PCI_PRODUCT_HP_HPSAP600	0x3225		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSA_3	0x3230		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_4	0x3231		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_5	0x3232		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAE500_1	0x3233		/* Smart Array E500 */
#define	PCI_PRODUCT_HP_HPSAP400	0x3234		/* Smart Array P400 */
#define	PCI_PRODUCT_HP_HPSAP400I	0x3235		/* Smart Array P400i */
#define	PCI_PRODUCT_HP_HPSA_6	0x3236		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAE500_2	0x3237		/* Smart Array E500 */
#define	PCI_PRODUCT_HP_HPSA_7	0x3238		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_8	0x3239		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_9	0x323a		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_10	0x323b		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSA_11	0x323c		/* Smart Array */
#define	PCI_PRODUCT_HP_HPSAP700M	0x323d		/* Smart Array P700m */
#define	PCI_PRODUCT_HP_HPSAP212	0x3241		/* Smart Array P212 */
#define	PCI_PRODUCT_HP_HPSAP410	0x3243		/* Smart Array P410 */
#define	PCI_PRODUCT_HP_HPSAP410I	0x3245		/* Smart Array P410i */
#define	PCI_PRODUCT_HP_HPSAP411	0x3247		/* Smart Array P411 */
#define	PCI_PRODUCT_HP_HPSAP812	0x3249		/* Smart Array P812 */
#define	PCI_PRODUCT_HP_HPSAP712M	0x324a		/* Smart Array P712m */
#define	PCI_PRODUCT_HP_HPSAP711M	0x324b		/* Smart Array P711m */
#define	PCI_PRODUCT_HP_USB	0x3300		/* USB */
#define	PCI_PRODUCT_HP_IPMI	0x3302		/* IPMI */

/* Hifn products */
#define	PCI_PRODUCT_HIFN_7751	0x0005		/* 7751 */
#define	PCI_PRODUCT_HIFN_6500	0x0006		/* 6500 */
#define	PCI_PRODUCT_HIFN_7811	0x0007		/* 7811 */
#define	PCI_PRODUCT_HIFN_7951	0x0012		/* 7951 */
#define	PCI_PRODUCT_HIFN_78XX	0x0014		/* 7814/7851/7854 */
#define	PCI_PRODUCT_HIFN_8065	0x0016		/* 8065 */
#define	PCI_PRODUCT_HIFN_8165	0x0017		/* 8165 */
#define	PCI_PRODUCT_HIFN_8154	0x0018		/* 8154 */
#define	PCI_PRODUCT_HIFN_7956	0x001d		/* 7956 */
#define	PCI_PRODUCT_HIFN_7955	0x0020		/* 7955/7954 */

/* Hint products */
#define	PCI_PRODUCT_HINT_HB6_1	0x0020		/* HB6 PCI-PCI */
#define	PCI_PRODUCT_HINT_HB6_2	0x0021		/* HB6 PCI-PCI */
#define	PCI_PRODUCT_HINT_HB4	0x0022		/* HB4 PCI-PCI */
#define	PCI_PRODUCT_HINT_VXPRO_II_HOST	0x8011		/* Host */
#define	PCI_PRODUCT_HINT_VXPRO_II_ISA	0x8012		/* ISA */
#define	PCI_PRODUCT_HINT_VXPRO_II_EIDE	0x8013		/* EIDE */

/* Hitachi products */
#define	PCI_PRODUCT_HITACHI_SWC	0x0101		/* MSVCC01 Video Capture */
#define	PCI_PRODUCT_HITACHI_SH7751	0x3505		/* SH7751 PCI */
#define	PCI_PRODUCT_HITACHI_SH7751R	0x350e		/* SH7751R PCI */

/* IBM products */
#define	PCI_PRODUCT_IBM_0x0002	0x0002		/* MCA */
#define	PCI_PRODUCT_IBM_0x0005	0x0005		/* CPU - Alta Lite */
#define	PCI_PRODUCT_IBM_0x0007	0x0007		/* CPU - Alta MP */
#define	PCI_PRODUCT_IBM_0x000a	0x000a		/* ISA w/PnP */
#define	PCI_PRODUCT_IBM_0x0017	0x0017		/* CPU */
#define	PCI_PRODUCT_IBM_0x0018	0x0018		/* Auto LANStreamer */
#define	PCI_PRODUCT_IBM_GXT150P	0x001b		/* GXT-150P */
#define	PCI_PRODUCT_IBM_82G2675	0x001d		/* 82G2675 */
#define	PCI_PRODUCT_IBM_MCA	0x0020		/* MCA */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI */
#define	PCI_PRODUCT_IBM_SERVERAID	0x002e		/* ServeRAID */
#define	PCI_PRODUCT_IBM_MIAMI	0x0036		/* Miami/PCI */
#define	PCI_PRODUCT_IBM_OLYMPIC	0x003e		/* Olympic */
#define	PCI_PRODUCT_IBM_I82557B	0x0057		/* i82557B 10/100 */
#define	PCI_PRODUCT_IBM_RSA	0x010f		/* RSA */
#define	PCI_PRODUCT_IBM_FIREGL2	0x0170		/* FireGL2 */
#define	PCI_PRODUCT_IBM_133PCIX	0x01a7		/* 133 PCIX-PCIX */
#define	PCI_PRODUCT_IBM_SERVERAID2	0x01bd		/* ServeRAID */
#define	PCI_PRODUCT_IBM_4810_BSP	0x0295		/* 4810 BSP */
#define	PCI_PRODUCT_IBM_4810_SCC	0x0297		/* 4810 SCC */
#define	PCI_PRODUCT_IBM_CALGARY_IOMMU	0x02a1		/* Calgary IOMMU */

/* IC Ensemble */
#define	PCI_PRODUCT_ICENSEMBLE_ICE1712	0x1712		/* Envy24 I/O Ctrlr */
#define	PCI_PRODUCT_ICENSEMBLE_VT172x	0x1724		/* Envy24PT/HT Audio */

/* IDT products */
#define	PCI_PRODUCT_IDT_77201	0x0001		/* 77201/77211 ATM (NICStAR) */
#define	PCI_PRODUCT_IDT_89HPES12N3A	0x8018		/* 89HPES12N3A */
#define	PCI_PRODUCT_IDT_89HPES24N3A	0x801c		/* 89HPES24N3A */
#define	PCI_PRODUCT_IDT_89HPES24T6	0x802e		/* 89HPES24T6 */

/* Industrial Computer Source */
#define	PCI_PRODUCT_INDCOMPSRC_WDT50x	0x22c0		/* WDT 50x Watchdog Timer */

/* Initio Corporation */
#define	PCI_PRODUCT_INITIO_INIC850	0x0850		/* INIC-850 (A100UW) */
#define	PCI_PRODUCT_INITIO_INIC1060	0x1060		/* INIC-1060 (A100U2W) */
#define	PCI_PRODUCT_INITIO_INIC940	0x9400		/* INIC-940 */
#define	PCI_PRODUCT_INITIO_INIC941	0x9401		/* INIC-941 */
#define	PCI_PRODUCT_INITIO_INIC950	0x9500		/* INIC-950 */

/* InnoTek Systemberatung GmbH */
#define	PCI_PRODUCT_INNOTEK_VBGA	0xbeef		/* VirtualBox Graphics Adapter */
#define	PCI_PRODUCT_INNOTEK_VBGS	0xcafe		/* VirtualBox Guest Service */

/* INPROCOMM products */
#define	PCI_PRODUCT_INPROCOMM_IPN2120	0x2120		/* IPN2120 */
#define	PCI_PRODUCT_INPROCOMM_IPN2220	0x2220		/* IPN2220 */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_5026	0x5026		/* 5026 */
#define	PCI_PRODUCT_IMS_5027	0x5027		/* 5027 */
#define	PCI_PRODUCT_IMS_5028	0x5028		/* 5028 */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */
#define	PCI_PRODUCT_IMS_8853	0x8853		/* 8853 */
#define	PCI_PRODUCT_IMS_TT128	0x9128		/* Twin Turbo 128 */
#define	PCI_PRODUCT_IMS_TT3D	0x9135		/* Twin Turbo 3D */

/* Intel products */
#define	PCI_PRODUCT_INTEL_EESISA	0x0008		/* EES ISA */
#define	PCI_PRODUCT_INTEL_21145	0x0039		/* 21145 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1000_1	0x0083		/* WiFi Link 1000 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_1000_2	0x0084		/* WiFi Link 1000 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_1	0x0087		/* Centrino Advanced-N 6250 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6050_2X2_2	0x0089		/* Centrino Advanced-N 6250 */
#define	PCI_PRODUCT_INTEL_80303	0x0309		/* 80303 IOP */
#define	PCI_PRODUCT_INTEL_80312	0x030d		/* 80312 I/O Companion */
#define	PCI_PRODUCT_INTEL_IOXAPIC_A	0x0326		/* IOxAPIC */
#define	PCI_PRODUCT_INTEL_IOXAPIC_B	0x0327		/* IOxAPIC */
#define	PCI_PRODUCT_INTEL_6700PXH_A	0x0329		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_6700PXH_B	0x032a		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_6702PXH	0x032c		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_IOP332_A	0x0330		/* IOP332 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_IOP332_B	0x0332		/* IOP332 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_IOP331	0x0335		/* IOP331 PCIX-PCIX */
#define	PCI_PRODUCT_INTEL_41210_A	0x0340		/* 41210 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_41210_B	0x0341		/* 41210 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_IOP333_A	0x0370		/* IOP333 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_IOP333_B	0x0372		/* IOP333 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB EISA */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424ZX Cache/DRAM */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378IB ISA */
#define	PCI_PRODUCT_INTEL_82426EX	0x0486		/* 82426EX ISA */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX/NX PCI/Cache/DRAM */
#define	PCI_PRODUCT_INTEL_GDT_RAID1	0x0600		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_GDT_RAID2	0x061f		/* GDT RAID */
#define	PCI_PRODUCT_INTEL_80960RP	0x0960		/* i960 RP PCI-PCI */
#define	PCI_PRODUCT_INTEL_80960RM	0x0962		/* i960 RM PCI-PCI */
#define	PCI_PRODUCT_INTEL_80960RN	0x0964		/* i960 RN PCI-PCI */
#define	PCI_PRODUCT_INTEL_82542	0x1000		/* PRO/1000 (82542) */
#define	PCI_PRODUCT_INTEL_82543GC_FIBER	0x1001		/* PRO/1000F (82543GC) */
#define	PCI_PRODUCT_INTEL_MODEM56	0x1002		/* 56k Modem */
#define	PCI_PRODUCT_INTEL_82543GC_COPPER	0x1004		/* PRO/1000T (82543GC) */
#define	PCI_PRODUCT_INTEL_82544EI_COPPER	0x1008		/* PRO/1000XT (82544EI) */
#define	PCI_PRODUCT_INTEL_82544EI_FIBER	0x1009		/* PRO/1000XF (82544EI) */
#define	PCI_PRODUCT_INTEL_82544GC_COPPER	0x100c		/* PRO/1000T (82544GC) */
#define	PCI_PRODUCT_INTEL_82544GC_LOM	0x100d		/* PRO/1000XT (82544GC) */
#define	PCI_PRODUCT_INTEL_82540EM	0x100e		/* PRO/1000MT (82540EM) */
#define	PCI_PRODUCT_INTEL_82545EM_COPPER	0x100f		/* PRO/1000MT (82545EM) */
#define	PCI_PRODUCT_INTEL_82546EB_COPPER	0x1010		/* PRO/1000MT (82546EB) */
#define	PCI_PRODUCT_INTEL_82545EM_FIBER	0x1011		/* PRO/1000MF (82545EM) */
#define	PCI_PRODUCT_INTEL_82546EB_FIBER	0x1012		/* PRO/1000MF (82546EB) */
#define	PCI_PRODUCT_INTEL_82541EI	0x1013		/* PRO/1000MT (82541EI) */
#define	PCI_PRODUCT_INTEL_82541ER_LOM	0x1014		/* PRO/1000MT (82541EI) */
#define	PCI_PRODUCT_INTEL_82540EM_LOM	0x1015		/* PRO/1000MT (82540EM) */
#define	PCI_PRODUCT_INTEL_82540EP_LOM	0x1016		/* PRO/1000MT (82540EP) */
#define	PCI_PRODUCT_INTEL_82540EP	0x1017		/* PRO/1000MT (82540EP) */
#define	PCI_PRODUCT_INTEL_82541EI_MOBILE	0x1018		/* PRO/1000MT Mobile (82541EI) */
#define	PCI_PRODUCT_INTEL_82547EI	0x1019		/* PRO/1000CT (82547EI) */
#define	PCI_PRODUCT_INTEL_82547EI_MOBILE	0x101a		/* PRO/1000CT Mobile (82547EI) */
#define	PCI_PRODUCT_INTEL_82546EB_QUAD_CPR	0x101d		/* PRO/1000MT QP (82546EB) */
#define	PCI_PRODUCT_INTEL_82540EP_LP	0x101e		/* PRO/1000MT (82540EP) */
#define	PCI_PRODUCT_INTEL_82545GM_COPPER	0x1026		/* PRO/1000MT (82545GM) */
#define	PCI_PRODUCT_INTEL_82545GM_FIBER	0x1027		/* PRO/1000MF (82545GM) */
#define	PCI_PRODUCT_INTEL_82545GM_SERDES	0x1028		/* PRO/1000MF (82545GM) */
#define	PCI_PRODUCT_INTEL_PRO_100	0x1029		/* PRO/100 */
#define	PCI_PRODUCT_INTEL_82559	0x1030		/* 82559 */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_0	0x1031		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_1	0x1032		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_0	0x1033		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_1	0x1034		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_0	0x1035		/* 82562EH HomePNA */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_1	0x1036		/* 82562EH HomePNA */
#define	PCI_PRODUCT_INTEL_82562EH_HPNA_2	0x1037		/* 82562EH HomePNA */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_2	0x1038		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_2	0x1039		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_82801DB_LAN	0x103a		/* 82801DB LAN */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_3	0x103b		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_4	0x103c		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_3	0x103d		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_5	0x103e		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_536EP	0x1040		/* V.92 Modem */
#define	PCI_PRODUCT_INTEL_PRO_WL_2100	0x1043		/* PRO/Wireless 2100 */
#define	PCI_PRODUCT_INTEL_82597EX	0x1048		/* PRO/10GbE (82597EX) */
#define	PCI_PRODUCT_INTEL_ICH8_IGP_M_AMT	0x1049		/* ICH8 IGP M AMT */
#define	PCI_PRODUCT_INTEL_ICH8_IGP_AMT	0x104a		/* ICH8 IGP AMT */
#define	PCI_PRODUCT_INTEL_ICH8_IGP_C	0x104b		/* ICH8 IGP C */
#define	PCI_PRODUCT_INTEL_ICH8_IFE	0x104c		/* ICH8 IFE */
#define	PCI_PRODUCT_INTEL_ICH8_IGP_M	0x104d		/* ICH8 IGP M */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_4	0x1050		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_5	0x1051		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_6	0x1052		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_7	0x1053		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_8	0x1054		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_9	0x1055		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_10	0x1056		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_11	0x1057		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_12	0x1058		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_M	0x1059		/* PRO/100 M */
#define	PCI_PRODUCT_INTEL_82571EB_COPPER	0x105e		/* PRO/1000 PT (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_FIBER	0x105f		/* PRO/1000 PF (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_SERDES	0x1060		/* PRO/1000 PB (82571EB) */
#define	PCI_PRODUCT_INTEL_82801FB_LAN_2	0x1064		/* 82801FB LAN */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_6	0x1065		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_13	0x1066		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_14	0x1067		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_82801FBM_LAN	0x1068		/* 82801FBM LAN */
#define	PCI_PRODUCT_INTEL_82801GB_LAN_2	0x1069		/* 82801GB LAN */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_7	0x106a		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_8	0x106b		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_82547GI	0x1075		/* PRO/1000CT (82547GI) */
#define	PCI_PRODUCT_INTEL_82541GI	0x1076		/* PRO/1000MT (82541GI) */
#define	PCI_PRODUCT_INTEL_82541GI_MOBILE	0x1077		/* PRO/1000MT Mobile (82541GI) */
#define	PCI_PRODUCT_INTEL_82541ER	0x1078		/* PRO/1000MT (82541ER) */
#define	PCI_PRODUCT_INTEL_82546GB_COPPER	0x1079		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82546GB_FIBER	0x107a		/* PRO/1000MF (82546GB) */
#define	PCI_PRODUCT_INTEL_82546GB_SERDES	0x107b		/* PRO/1000MF (82546GB) */
#define	PCI_PRODUCT_INTEL_82541GI_LF	0x107c		/* PRO/1000GT (82541GI) */
#define	PCI_PRODUCT_INTEL_82572EI_COPPER	0x107d		/* PRO/1000 PT (82572EI) */
#define	PCI_PRODUCT_INTEL_82572EI_FIBER	0x107e		/* PRO/1000 PF (82572EI) */
#define	PCI_PRODUCT_INTEL_82572EI_SERDES	0x107f		/* PRO/1000 PB (82572EI) */
#define	PCI_PRODUCT_INTEL_82546GB_PCIE	0x108a		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82573E	0x108b		/* PRO/1000MT (82573E) */
#define	PCI_PRODUCT_INTEL_82573E_IAMT	0x108c		/* PRO/1000MT (82573E) */
#define	PCI_PRODUCT_INTEL_82573E_IDE	0x108d		/* 82573E IDE */
#define	PCI_PRODUCT_INTEL_82573E_KCS	0x108e		/* 82573E KCS */
#define	PCI_PRODUCT_INTEL_82573E_SERIAL	0x108f		/* 82573E Serial */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_15	0x1091		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_16	0x1092		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_17	0x1093		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_18	0x1094		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_19	0x1095		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_80003ES2LAN_CPR_DPT	0x1096		/* PRO/1000 PT (80003ES2) */
#define	PCI_PRODUCT_INTEL_80003ES2LAN_SDS_DPT	0x1098		/* PRO/1000 PF (80003ES2) */
#define	PCI_PRODUCT_INTEL_82546GB_QUAD_CPR	0x1099		/* PRO/1000MT QP (82546GB) */
#define	PCI_PRODUCT_INTEL_82573L	0x109a		/* PRO/1000MT (82573L) */
#define	PCI_PRODUCT_INTEL_82546GB_2	0x109b		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82597EX_CX4	0x109e		/* PRO/10GbE CX4 (82597EX) */
#define	PCI_PRODUCT_INTEL_82571EB_AT	0x10a0		/* PRO/1000 AT (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_AF	0x10a1		/* PRO/1000 AF (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_CPR	0x10a4		/* PRO/1000 QP (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_FBR	0x10a5		/* PRO/1000 QP (82571EB) */
#define	PCI_PRODUCT_INTEL_82575EB_COPPER	0x10a7		/* PRO/1000 PT (82575EB) */
#define	PCI_PRODUCT_INTEL_82575EB_SERDES	0x10a9		/* PRO/1000 PF (82575EB) */
#define	PCI_PRODUCT_INTEL_82573L_PL_1	0x10b0		/* PRO/1000 PL (82573L) */
#define	PCI_PRODUCT_INTEL_82573V_PM	0x10b2		/* PRO/1000 PM (82573V) */
#define	PCI_PRODUCT_INTEL_82573E_PM	0x10b3		/* PRO/1000 PM (82573E) */
#define	PCI_PRODUCT_INTEL_82573L_PL_2	0x10b4		/* PRO/1000 PL (82573L) */
#define	PCI_PRODUCT_INTEL_82546GB_QUAD_CPR_K	0x10b5		/* PRO/1000MT QP (82546GB) */
#define	PCI_PRODUCT_INTEL_82598	0x10b6		/* 10GbE (82598) */
#define	PCI_PRODUCT_INTEL_82572EI	0x10b9		/* PRO/1000 PT (82572EI) */
#define	PCI_PRODUCT_INTEL_80003ES2LAN_CPR_SPT	0x10ba		/* PRO/1000 PT (80003ES2) */
#define	PCI_PRODUCT_INTEL_80003ES2LAN_SDS_SPT	0x10bb		/* PRO/1000 PF (80003ES2) */
#define	PCI_PRODUCT_INTEL_82571EB_QUAD_CPR_LP	0x10bc		/* PRO/1000 QP (82571EB) */
#define	PCI_PRODUCT_INTEL_ICH9_IGP_AMT	0x10bd		/* ICH9 IGP AMT */
#define	PCI_PRODUCT_INTEL_ICH9_IGP_M	0x10bf		/* ICH9 IGP M */
#define	PCI_PRODUCT_INTEL_ICH9_IFE	0x10c0		/* ICH9 IFE */
#define	PCI_PRODUCT_INTEL_ICH9_IFE_G	0x10c2		/* ICH9 IFE G */
#define	PCI_PRODUCT_INTEL_ICH9_IFE_GT	0x10c3		/* ICH9 IFE GT */
#define	PCI_PRODUCT_INTEL_ICH8_IFE_GT	0x10c4		/* ICH8 IFE GT */
#define	PCI_PRODUCT_INTEL_ICH8_IFE_G	0x10c5		/* ICH8 IFE G */
#define	PCI_PRODUCT_INTEL_82598AF_DUAL	0x10c6		/* 10GbE SR Dual (82598AF) */
#define	PCI_PRODUCT_INTEL_82598AF	0x10c7		/* 10GbE SR (82598AF) */
#define	PCI_PRODUCT_INTEL_82598AT	0x10c8		/* 10GbE (82598AT) */
#define	PCI_PRODUCT_INTEL_82576	0x10c9		/* PRO/1000 (82576) */
#define	PCI_PRODUCT_INTEL_ICH9_IGP_M_V	0x10cb		/* ICH9 IGP M V */
#define	PCI_PRODUCT_INTEL_ICH10_R_BM_LM	0x10cc		/* ICH10 R BM LM */
#define	PCI_PRODUCT_INTEL_ICH10_R_BM_LF	0x10cd		/* ICH10 R BM LF */
#define	PCI_PRODUCT_INTEL_ICH10_R_BM_V	0x10ce		/* ICH10 R BM V */
#define	PCI_PRODUCT_INTEL_82574L	0x10d3		/* PRO/1000 MT (82574L) */
#define	PCI_PRODUCT_INTEL_82571PT_QUAD_CPR	0x10d5		/* PRO/1000 QP (82571PT) */
#define	PCI_PRODUCT_INTEL_82575GB_QUAD_CPR	0x10d6		/* PRO/1000 QP (82575GB) */
#define	PCI_PRODUCT_INTEL_82598AT_DUAL	0x10d7		/* 10GbE Dual (82598AT) */
#define	PCI_PRODUCT_INTEL_82571EB_SDS_DUAL	0x10d9		/* PRO/1000 PT (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_SDS_QUAD	0x10da		/* PRO/1000 QP (82571EB) */
#define	PCI_PRODUCT_INTEL_82598EB_CX4	0x10dd		/* 10GbE CX4 (82598EB) */
#define	PCI_PRODUCT_INTEL_82598EB_SFP	0x10db		/* 10GbE SFP+ (82598EB) */
#define	PCI_PRODUCT_INTEL_ICH10_D_BM_LM	0x10de		/* ICH10 D BM LM */
#define	PCI_PRODUCT_INTEL_ICH10_D_BM_LF	0x10df		/* ICH10 D BM LF */
#define	PCI_PRODUCT_INTEL_82598_SR_DUAL_EM	0x10e1		/* 10GbE SR Dual (82598) */
#define	PCI_PRODUCT_INTEL_ICH9_BM	0x10e5		/* ICH9 BM */
#define	PCI_PRODUCT_INTEL_82576_FIBER	0x10e6		/* PRO/1000 FP (82576) */
#define	PCI_PRODUCT_INTEL_82576_SERDES	0x10e7		/* PRO/1000 FP (82576) */
#define	PCI_PRODUCT_INTEL_82576_QUAD_COPPER	0x10e8		/* PRO/1000 QP (82576) */
#define	PCI_PRODUCT_INTEL_82598EB_CX4_DUAL	0x10ec		/* 10GbE CX4 Dual (82598EB) */
#define	PCI_PRODUCT_INTEL_82578DM	0x10ef		/* 82578DM */
#define	PCI_PRODUCT_INTEL_82598_DA_DUAL	0x10f1		/* 10GbE DA Dual (82598) */
#define	PCI_PRODUCT_INTEL_82598EB_XF_LR	0x10f4		/* 10GbE LR (82598EB) */
#define	PCI_PRODUCT_INTEL_ICH9_IGP_M_AMT	0x10f5		/* ICH9 IGP M AMT */
#define	PCI_PRODUCT_INTEL_82599_KX4	0x10f7		/* 10GbE KX4 (82599) */
#define	PCI_PRODUCT_INTEL_82599_COMBO_BACKPLANE	0x10f8		/* 10GbE Backplane (82599) */
#define	PCI_PRODUCT_INTEL_82599_CX4	0x10f9		/* 10GbE CX4 (82599) */
#define	PCI_PRODUCT_INTEL_82599_SFP	0x10fb		/* 10GbE SFP+ (82599) */
#define	PCI_PRODUCT_INTEL_82599_XAUI	0x10fc		/* 10GbE XAUI (82599) */
#define	PCI_PRODUCT_INTEL_82552	0x10fe		/* 82552 */
#define	PCI_PRODUCT_INTEL_82815_HB	0x1130		/* 82815 Host */
#define	PCI_PRODUCT_INTEL_82815_AGP	0x1131		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_IGD	0x1132		/* 82815 Video */
#define	PCI_PRODUCT_INTEL_82806AA_APIC	0x1161		/* 82806AA APIC */
#define	PCI_PRODUCT_INTEL_82559ER	0x1209		/* 82559ER */
#define	PCI_PRODUCT_INTEL_82092AA	0x1222		/* 82092AA IDE */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82452_HB	0x1225		/* 82452KX/GX */
#define	PCI_PRODUCT_INTEL_82596	0x1226		/* EE Pro 10 PCI */
#define	PCI_PRODUCT_INTEL_EEPRO100	0x1227		/* EE Pro 100 */
#define	PCI_PRODUCT_INTEL_EEPRO100S	0x1228		/* EE Pro 100 Smart */
#define	PCI_PRODUCT_INTEL_8255x	0x1229		/* 8255x */
#define	PCI_PRODUCT_INTEL_82437FX	0x122d		/* 82437FX */
#define	PCI_PRODUCT_INTEL_82371FB_ISA	0x122e		/* 82371FB ISA */
#define	PCI_PRODUCT_INTEL_82371FB_IDE	0x1230		/* 82371FB IDE */
#define	PCI_PRODUCT_INTEL_82371MX	0x1234		/* 82371 ISA and IDE */
#define	PCI_PRODUCT_INTEL_82437MX	0x1235		/* 82437 PCI/Cache/DRAM */
#define	PCI_PRODUCT_INTEL_82441FX	0x1237		/* 82441FX */
#define	PCI_PRODUCT_INTEL_82380AB	0x123c		/* 82380AB Mobile ISA */
#define	PCI_PRODUCT_INTEL_82380FB	0x124b		/* 82380FB Mobile PCI-PCI */
#define	PCI_PRODUCT_INTEL_82439HX	0x1250		/* 82439HX */
#define	PCI_PRODUCT_INTEL_82806AA	0x1360		/* 82806AA */
#define	PCI_PRODUCT_INTEL_82870P2_PPB	0x1460		/* 82870P2 PCIX-PCIX */
#define	PCI_PRODUCT_INTEL_82870P2_IOxAPIC	0x1461		/* 82870P2 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82870P2_HPLUG	0x1462		/* 82870P2 Hot Plug */
#define	PCI_PRODUCT_INTEL_82599_SFP_EM	0x1507		/* 10GbE SFP EM (82599) */
#define	PCI_PRODUCT_INTEL_82598_BX	0x1508		/* 10GbE BX (82598) */
#define	PCI_PRODUCT_INTEL_82576_NS	0x150a		/* PRO/1000 (82576NS) */
#define	PCI_PRODUCT_INTEL_82599_KX4_MEZZ	0x1514		/* 10GbE KX4 (82599) */
#define	PCI_PRODUCT_INTEL_80960RP_ATU	0x1960		/* 80960RP ATU */
#define	PCI_PRODUCT_INTEL_82840_HB	0x1a21		/* 82840 Host */
#define	PCI_PRODUCT_INTEL_82840_AGP	0x1a23		/* 82840 AGP */
#define	PCI_PRODUCT_INTEL_82840_PCI	0x1a24		/* 82840 PCI */
#define	PCI_PRODUCT_INTEL_82845_HB	0x1a30		/* 82845 Host */
#define	PCI_PRODUCT_INTEL_82845_AGP	0x1a31		/* 82845 AGP */
#define	PCI_PRODUCT_INTEL_IOAT	0x1a38		/* I/OAT */
#define	PCI_PRODUCT_INTEL_82597EX_SR	0x1a48		/* PRO/10GbE SR (82597EX) */
#define	PCI_PRODUCT_INTEL_82597EX_LR	0x1b48		/* PRO/10GbE LR (82597EX) */
#define	PCI_PRODUCT_INTEL_82801AA_LPC	0x2410		/* 82801AA LPC */
#define	PCI_PRODUCT_INTEL_82801AA_IDE	0x2411		/* 82801AA IDE */
#define	PCI_PRODUCT_INTEL_82801AA_USB	0x2412		/* 82801AA USB */
#define	PCI_PRODUCT_INTEL_82801AA_SMB	0x2413		/* 82801AA SMBus */
#define	PCI_PRODUCT_INTEL_82801AA_ACA	0x2415		/* 82801AA AC97 */
#define	PCI_PRODUCT_INTEL_82801AA_ACM	0x2416		/* 82801AA Modem */
#define	PCI_PRODUCT_INTEL_82801AA_HPB	0x2418		/* 82801AA Hub-to-PCI */
#define	PCI_PRODUCT_INTEL_82801AB_LPC	0x2420		/* 82801AB LPC */
#define	PCI_PRODUCT_INTEL_82801AB_IDE	0x2421		/* 82801AB IDE */
#define	PCI_PRODUCT_INTEL_82801AB_USB	0x2422		/* 82801AB USB */
#define	PCI_PRODUCT_INTEL_82801AB_SMB	0x2423		/* 82801AB SMBus */
#define	PCI_PRODUCT_INTEL_82801AB_ACA	0x2425		/* 82801AB AC97 */
#define	PCI_PRODUCT_INTEL_82801AB_ACM	0x2426		/* 82801AB Modem */
#define	PCI_PRODUCT_INTEL_82801AB_HPB	0x2428		/* 82801AB Hub-to-PCI */
#define	PCI_PRODUCT_INTEL_82801BA_LPC	0x2440		/* 82801BA LPC */
#define	PCI_PRODUCT_INTEL_82801BA_USB	0x2442		/* 82801BA USB */
#define	PCI_PRODUCT_INTEL_82801BA_SMB	0x2443		/* 82801BA SMBus */
#define	PCI_PRODUCT_INTEL_82801BA_USB2	0x2444		/* 82801BA USB */
#define	PCI_PRODUCT_INTEL_82801BA_ACA	0x2445		/* 82801BA AC97 */
#define	PCI_PRODUCT_INTEL_82801BA_ACM	0x2446		/* 82801BA Modem */
#define	PCI_PRODUCT_INTEL_82801BAM_HPB	0x2448		/* 82801BAM Hub-to-PCI */
#define	PCI_PRODUCT_INTEL_82562	0x2449		/* 82562 */
#define	PCI_PRODUCT_INTEL_82801BAM_IDE	0x244a		/* 82801BAM IDE */
#define	PCI_PRODUCT_INTEL_82801BA_IDE	0x244b		/* 82801BA IDE */
#define	PCI_PRODUCT_INTEL_82801BAM_LPC	0x244c		/* 82801BAM LPC */
#define	PCI_PRODUCT_INTEL_82801BA_HPB	0x244e		/* 82801BA Hub-to-PCI */
#define	PCI_PRODUCT_INTEL_82801E_LPC	0x2450		/* 82801E LPC */
#define	PCI_PRODUCT_INTEL_82801E_USB	0x2452		/* 82801E USB */
#define	PCI_PRODUCT_INTEL_82801E_SMB	0x2453		/* 82801E SMBus */
#define	PCI_PRODUCT_INTEL_82801E_LAN_1	0x2459		/* 82801E LAN */
#define	PCI_PRODUCT_INTEL_82801E_LAN_2	0x245d		/* 82801E LAN */
#define	PCI_PRODUCT_INTEL_82801CA_LPC	0x2480		/* 82801CA LPC */
#define	PCI_PRODUCT_INTEL_82801CA_USB_1	0x2482		/* 82801CA/CAM USB */
#define	PCI_PRODUCT_INTEL_82801CA_SMB	0x2483		/* 82801CA/CAM SMBus */
#define	PCI_PRODUCT_INTEL_82801CA_USB_2	0x2484		/* 82801CA/CAM USB */
#define	PCI_PRODUCT_INTEL_82801CA_ACA	0x2485		/* 82801CA/CAM AC97 */
#define	PCI_PRODUCT_INTEL_82801CA_ACM	0x2486		/* 82801CA/CAM Modem */
#define	PCI_PRODUCT_INTEL_82801CA_USB_3	0x2487		/* 82801CA/CAM USB */
#define	PCI_PRODUCT_INTEL_82801CAM_IDE	0x248a		/* 82801CAM IDE */
#define	PCI_PRODUCT_INTEL_82801CA_IDE	0x248b		/* 82801CA IDE */
#define	PCI_PRODUCT_INTEL_82801CAM_LPC	0x248c		/* 82801CAM LPC */
#define	PCI_PRODUCT_INTEL_82801DB_LPC	0x24c0		/* 82801DB LPC */
#define	PCI_PRODUCT_INTEL_82801DBL_IDE	0x24c1		/* 82801DBL IDE */
#define	PCI_PRODUCT_INTEL_82801DB_USB_1	0x24c2		/* 82801DB USB */
#define	PCI_PRODUCT_INTEL_82801DB_SMB	0x24c3		/* 82801DB SMBus */
#define	PCI_PRODUCT_INTEL_82801DB_USB_2	0x24c4		/* 82801DB USB */
#define	PCI_PRODUCT_INTEL_82801DB_ACA	0x24c5		/* 82801DB AC97 */
#define	PCI_PRODUCT_INTEL_82801DB_ACM	0x24c6		/* 82801DB Modem */
#define	PCI_PRODUCT_INTEL_82801DB_USB_3	0x24c7		/* 82801DB USB */
#define	PCI_PRODUCT_INTEL_82801DBM_IDE	0x24ca		/* 82801DBM IDE */
#define	PCI_PRODUCT_INTEL_82801DB_IDE	0x24cb		/* 82801DB IDE */
#define	PCI_PRODUCT_INTEL_82801DBM_LPC	0x24cc		/* 82801DBM LPC */
#define	PCI_PRODUCT_INTEL_82801DB_USB_4	0x24cd		/* 82801DB USB */
#define	PCI_PRODUCT_INTEL_82801EB_LPC	0x24d0		/* 82801EB/ER LPC */
#define	PCI_PRODUCT_INTEL_82801EB_SATA	0x24d1		/* 82801EB SATA */
#define	PCI_PRODUCT_INTEL_82801EB_USB_1	0x24d2		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801EB_SMB	0x24d3		/* 82801EB/ER SMBus */
#define	PCI_PRODUCT_INTEL_82801EB_USB_2	0x24d4		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801EB_ACA	0x24d5		/* 82801EB/ER AC97 */
#define	PCI_PRODUCT_INTEL_82801EB_MODEM	0x24d6		/* 82801EB/ER Modem */
#define	PCI_PRODUCT_INTEL_82801EB_USB_3	0x24d7		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801EB_IDE	0x24db		/* 82801EB/ER IDE */
#define	PCI_PRODUCT_INTEL_82801EB_USB_5	0x24dd		/* 82801EB/ER USB2 */
#define	PCI_PRODUCT_INTEL_82801EB_USB_4	0x24de		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801ER_SATA	0x24df		/* 82801ER SATA */
#define	PCI_PRODUCT_INTEL_82820_HB	0x2501		/* 82820 Host */
#define	PCI_PRODUCT_INTEL_82820_AGP	0x250f		/* 82820 AGP */
#define	PCI_PRODUCT_INTEL_82850_HB	0x2530		/* 82850 Host */
#define	PCI_PRODUCT_INTEL_82860_HB	0x2531		/* 82860 Host */
#define	PCI_PRODUCT_INTEL_82850_AGP	0x2532		/* 82850/82860 AGP */
#define	PCI_PRODUCT_INTEL_82860_PCI1	0x2533		/* 82860 PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI2	0x2534		/* 82860 PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI3	0x2535		/* 82860 PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI4	0x2536		/* 82860 PCI */
#define	PCI_PRODUCT_INTEL_E7500_HB	0x2540		/* E7500 Host */
#define	PCI_PRODUCT_INTEL_E7500_ERR	0x2541		/* E7500 Error Reporting */
#define	PCI_PRODUCT_INTEL_E7500_PCI_B1	0x2543		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7500_PCI_B2	0x2544		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7500_PCI_C1	0x2545		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7500_PCI_C2	0x2546		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7500_PCI_D1	0x2547		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7500_PCI_D2	0x2548		/* E7500 PCI */
#define	PCI_PRODUCT_INTEL_E7501_HB	0x254c		/* E7501 Host */
#define	PCI_PRODUCT_INTEL_E7505_HB	0x2550		/* E7505 Host */
#define	PCI_PRODUCT_INTEL_E7505_ERR	0x2551		/* E7505 Error Reporting */
#define	PCI_PRODUCT_INTEL_E7505_AGP	0x2552		/* E7505 AGP */
#define	PCI_PRODUCT_INTEL_E7505_PCI_B1	0x2553		/* E7505 PCI */
#define	PCI_PRODUCT_INTEL_E7505_PCI_B2	0x2554		/* E7505 PCI */
#define	PCI_PRODUCT_INTEL_82845G_HB	0x2560		/* 82845G Host */
#define	PCI_PRODUCT_INTEL_82845G_AGP	0x2561		/* 82845G AGP */
#define	PCI_PRODUCT_INTEL_82845G_IGD	0x2562		/* 82845G Video */
#define	PCI_PRODUCT_INTEL_82865G_HB	0x2570		/* 82865G Host */
#define	PCI_PRODUCT_INTEL_82865G_AGP	0x2571		/* 82865G AGP */
#define	PCI_PRODUCT_INTEL_82865G_IGD	0x2572		/* 82865G Video */
#define	PCI_PRODUCT_INTEL_82865G_CSA	0x2573		/* 82865G CSA */
#define	PCI_PRODUCT_INTEL_82865G_OVF	0x2576		/* 82865G Overflow */
#define	PCI_PRODUCT_INTEL_82875P_HB	0x2578		/* 82875P Host */
#define	PCI_PRODUCT_INTEL_82875P_AGP	0x2579		/* 82875P AGP */
#define	PCI_PRODUCT_INTEL_82875P_CSA	0x257b		/* 82875P CSA */
#define	PCI_PRODUCT_INTEL_82915G_HB	0x2580		/* 82915G Host */
#define	PCI_PRODUCT_INTEL_82915G_PCIE	0x2581		/* 82915G PCIE */
#define	PCI_PRODUCT_INTEL_82915G_IGD_1	0x2582		/* 82915G Video */
#define	PCI_PRODUCT_INTEL_82925X_HB	0x2584		/* 82925X Host */
#define	PCI_PRODUCT_INTEL_82925X_PCIE	0x2585		/* 82925X PCIE */
#define	PCI_PRODUCT_INTEL_E7221_HB	0x2588		/* E7221 Host */
#define	PCI_PRODUCT_INTEL_E7221_PCIE	0x2589		/* E7221 PCIE */
#define	PCI_PRODUCT_INTEL_E7221_IGD	0x258a		/* E7221 Video */
#define	PCI_PRODUCT_INTEL_82915GM_HB	0x2590		/* 82915GM Host */
#define	PCI_PRODUCT_INTEL_82915GM_PCIE	0x2591		/* 82915GM PCIE */
#define	PCI_PRODUCT_INTEL_82915GM_IGD_1	0x2592		/* 82915GM Video */
#define	PCI_PRODUCT_INTEL_6300ESB_LPC	0x25a1		/* 6300ESB LPC */
#define	PCI_PRODUCT_INTEL_6300ESB_IDE	0x25a2		/* 6300ESB IDE */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA	0x25a3		/* 6300ESB SATA */
#define	PCI_PRODUCT_INTEL_6300ESB_SMB	0x25a4		/* 6300ESB SMBus */
#define	PCI_PRODUCT_INTEL_6300ESB_ACA	0x25a6		/* 6300ESB AC97 */
#define	PCI_PRODUCT_INTEL_6300ESB_ACM	0x25a7		/* 6300ESB Modem */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_1	0x25a9		/* 6300ESB USB */
#define	PCI_PRODUCT_INTEL_6300ESB_USB_2	0x25aa		/* 6300ESB USB */
#define	PCI_PRODUCT_INTEL_6300ESB_WDT	0x25ab		/* 6300ESB WDT */
#define	PCI_PRODUCT_INTEL_6300ESB_APIC	0x25ac		/* 6300ESB APIC */
#define	PCI_PRODUCT_INTEL_6300ESB_USB2	0x25ad		/* 6300ESB USB */
#define	PCI_PRODUCT_INTEL_6300ESB_PCIX	0x25ae		/* 6300ESB PCIX */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA2	0x25b0		/* 6300ESB SATA */
#define	PCI_PRODUCT_INTEL_5000X_HB	0x25c0		/* 5000X Host */
#define	PCI_PRODUCT_INTEL_5000Z_HB	0x25d0		/* 5000Z Host */
#define	PCI_PRODUCT_INTEL_5000V_HB	0x25d4		/* 5000V Host */
#define	PCI_PRODUCT_INTEL_5000P_HB	0x25d8		/* 5000P Host */
#define	PCI_PRODUCT_INTEL_5000_PCIE_1	0x25e2		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_PCIE_2	0x25e3		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_PCIE_3	0x25e4		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_PCIE_4	0x25e5		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_PCIE_5	0x25e6		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_PCIE_6	0x25e7		/* 5000 PCIE */
#define	PCI_PRODUCT_INTEL_5000_ERR	0x25f0		/* 5000 Error Reporting */
#define	PCI_PRODUCT_INTEL_5000_RESERVED_1	0x25f1		/* 5000 Reserved */
#define	PCI_PRODUCT_INTEL_5000_RESERVED_2	0x25f3		/* 5000 Reserved */
#define	PCI_PRODUCT_INTEL_5000_FBD_1	0x25f5		/* 5000 FBD */
#define	PCI_PRODUCT_INTEL_5000_FBD_2	0x25f6		/* 5000 FBD */
#define	PCI_PRODUCT_INTEL_5000_PCIE_7	0x25f7		/* 5000 PCIE x8 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_8	0x25f8		/* 5000 PCIE x8 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_9	0x25f9		/* 5000 PCIE x8 */
#define	PCI_PRODUCT_INTEL_5000_PCIE_10	0x25fa		/* 5000 PCIE x16 */
#define	PCI_PRODUCT_INTEL_E8500_HB	0x2600		/* E8500 Host */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_1	0x2601		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_2	0x2602		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_3	0x2603		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_4	0x2604		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_5	0x2605		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_6	0x2606		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_7	0x2607		/* E8500 PCIE */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_8	0x2608		/* E8500 PCIE x8 */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_9	0x2609		/* E8500 PCIE x8 */
#define	PCI_PRODUCT_INTEL_E8500_PCIE_10	0x260a		/* E8500 PCIE x8 */
#define	PCI_PRODUCT_INTEL_E8500_IMI	0x260c		/* E8500 IMI */
#define	PCI_PRODUCT_INTEL_E8500_FSBINT	0x2610		/* E8500 FSB/Boot/Interrupt */
#define	PCI_PRODUCT_INTEL_E8500_AM	0x2611		/* E8500 Address Mapping */
#define	PCI_PRODUCT_INTEL_E8500_RAS	0x2612		/* E8500 RAS */
#define	PCI_PRODUCT_INTEL_E8500_MISC_1	0x2613		/* E8500 Misc */
#define	PCI_PRODUCT_INTEL_E8500_MISC_2	0x2614		/* E8500 Misc */
#define	PCI_PRODUCT_INTEL_E8500_MISC_3	0x2615		/* E8500 Misc */
#define	PCI_PRODUCT_INTEL_E8500_RES_1	0x2617		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_2	0x2618		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_3	0x2619		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_4	0x261a		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_5	0x261b		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_6	0x261c		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_7	0x261d		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_RES_8	0x261e		/* E8500 Reserved */
#define	PCI_PRODUCT_INTEL_E8500_XMB_ID	0x2620		/* E8500 XMB */
#define	PCI_PRODUCT_INTEL_E8500_XMB_MISC	0x2621		/* E8500 XMB Misc */
#define	PCI_PRODUCT_INTEL_E8500_XMB_MAI	0x2622		/* E8500 XMB MAI */
#define	PCI_PRODUCT_INTEL_E8500_XMB_DDR	0x2623		/* E8500 XMB DDR */
#define	PCI_PRODUCT_INTEL_E8500_XMB_RES_1	0x2624		/* E8500 XMB Reserved */
#define	PCI_PRODUCT_INTEL_E8500_XMB_RES_2	0x2625		/* E8500 XMB Reserved */
#define	PCI_PRODUCT_INTEL_E8500_XMB_RES_3	0x2626		/* E8500 XMB Reserved */
#define	PCI_PRODUCT_INTEL_E8500_XMB_RES_4	0x2627		/* E8500 XMB Reserved */
#define	PCI_PRODUCT_INTEL_82801FB_LPC	0x2640		/* 82801FB LPC */
#define	PCI_PRODUCT_INTEL_82801FBM_LPC	0x2641		/* 82801FBM LPC */
#define	PCI_PRODUCT_INTEL_82801FB_SATA	0x2651		/* 82801FB SATA */
#define	PCI_PRODUCT_INTEL_82801FR_SATA	0x2652		/* 82801FR SATA */
#define	PCI_PRODUCT_INTEL_82801FBM_SATA	0x2653		/* 82801FBM SATA */
#define	PCI_PRODUCT_INTEL_82801FB_USB_1	0x2658		/* 82801FB USB */
#define	PCI_PRODUCT_INTEL_82801FB_USB_2	0x2659		/* 82801FB USB */
#define	PCI_PRODUCT_INTEL_82801FB_USB_3	0x265a		/* 82801FB USB */
#define	PCI_PRODUCT_INTEL_82801FB_USB_4	0x265b		/* 82801FB USB */
#define	PCI_PRODUCT_INTEL_82801FB_USB	0x265c		/* 82801FB USB */
#define	PCI_PRODUCT_INTEL_82801FB_PCIE_1	0x2660		/* 82801FB PCIE */
#define	PCI_PRODUCT_INTEL_82801FB_PCIE_2	0x2662		/* 82801FB PCIE */
#define	PCI_PRODUCT_INTEL_82801FB_PCIE_3	0x2664		/* 82801FB PCIE */
#define	PCI_PRODUCT_INTEL_82801FB_PCIE_4	0x2666		/* 82801FB PCIE */
#define	PCI_PRODUCT_INTEL_82801FB_HDA	0x2668		/* 82801FB HD Audio */
#define	PCI_PRODUCT_INTEL_82801FB_SMB	0x266a		/* 82801FB SMBus */
#define	PCI_PRODUCT_INTEL_82801FB_LAN	0x266c		/* 82801FB LAN */
#define	PCI_PRODUCT_INTEL_82801FB_ACM	0x266d		/* 82801FB Modem */
#define	PCI_PRODUCT_INTEL_82801FB_ACA	0x266e		/* 82801FB AC97 */
#define	PCI_PRODUCT_INTEL_82801FB_IDE	0x266f		/* 82801FB IDE */
#define	PCI_PRODUCT_INTEL_6321ESB_LPC	0x2670		/* 6321ESB LPC */
#define	PCI_PRODUCT_INTEL_6321ESB_SATA	0x2680		/* 6321ESB SATA */
#define	PCI_PRODUCT_INTEL_6321ESB_AHCI	0x2681		/* 6321ESB AHCI */
#define	PCI_PRODUCT_INTEL_6321ESB_RAID_1	0x2682		/* 6321ESB RAID */
#define	PCI_PRODUCT_INTEL_6321ESB_RAID_2	0x2683		/* 6321ESB RAID */
#define	PCI_PRODUCT_INTEL_6321ESB_USB_1	0x2688		/* 6321ESB USB */
#define	PCI_PRODUCT_INTEL_6321ESB_USB_2	0x2689		/* 6321ESB USB */
#define	PCI_PRODUCT_INTEL_6321ESB_USB_3	0x268a		/* 6321ESB USB */
#define	PCI_PRODUCT_INTEL_6321ESB_USB_4	0x268b		/* 6321ESB USB */
#define	PCI_PRODUCT_INTEL_6321ESB_USB_5	0x268c		/* 6321ESB USB */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_1	0x2690		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_2	0x2692		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_3	0x2694		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_4	0x2696		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_ACA	0x2698		/* 6321ESB AC97 */
#define	PCI_PRODUCT_INTEL_6321ESB_ACM	0x2699		/* 6321ESB Modem */
#define	PCI_PRODUCT_INTEL_6321ESB_HDA	0x269a		/* 6321ESB HD Audio */
#define	PCI_PRODUCT_INTEL_6321ESB_SMB	0x269b		/* 6321ESB SMBus */
#define	PCI_PRODUCT_INTEL_6321ESB_IDE	0x269e		/* 6321ESB IDE */
#define	PCI_PRODUCT_INTEL_82945G_HB	0x2770		/* 82945G Host */
#define	PCI_PRODUCT_INTEL_82945G_PCIE	0x2771		/* 82945G PCIE */
#define	PCI_PRODUCT_INTEL_82945G_IGD_1	0x2772		/* 82945G Video */
#define	PCI_PRODUCT_INTEL_82955X_HB	0x2774		/* 82955X Host */
#define	PCI_PRODUCT_INTEL_82955X_PCIE	0x2775		/* 82955X PCIE */
#define	PCI_PRODUCT_INTEL_82945G_IGD_2	0x2776		/* 82945G Video */
#define	PCI_PRODUCT_INTEL_E7230_HB	0x2778		/* E7230 Host */
#define	PCI_PRODUCT_INTEL_E7230_PCIE	0x2779		/* E7230 PCIE */
#define	PCI_PRODUCT_INTEL_82975X_PCIE_2	0x277a		/* 82975X PCIE */
#define	PCI_PRODUCT_INTEL_82975X_HB	0x277c		/* 82975X Host */
#define	PCI_PRODUCT_INTEL_82975X_PCIE	0x277d		/* 82975X PCIE */
#define	PCI_PRODUCT_INTEL_82915G_IGD_2	0x2782		/* 82915G Video */
#define	PCI_PRODUCT_INTEL_82915GM_IGD_2	0x2792		/* 82915GM Video */
#define	PCI_PRODUCT_INTEL_82945GM_HB	0x27a0		/* 82945GM Host */
#define	PCI_PRODUCT_INTEL_82945GM_PCIE	0x27a1		/* 82945GM PCIE */
#define	PCI_PRODUCT_INTEL_82945GM_IGD_1	0x27a2		/* 82945GM Video */
#define	PCI_PRODUCT_INTEL_82945GM_IGD_2	0x27a6		/* 82945GM Video */
#define	PCI_PRODUCT_INTEL_82945GME_HB	0x27ac		/* 82945GME Host */
#define	PCI_PRODUCT_INTEL_82945GME_IGD_1	0x27ae		/* 82945GME Video */
#define	PCI_PRODUCT_INTEL_82801GH_LPC	0x27b0		/* 82801GH LPC */
#define	PCI_PRODUCT_INTEL_82801GB_LPC	0x27b8		/* 82801GB LPC */
#define	PCI_PRODUCT_INTEL_82801GBM_LPC	0x27b9		/* 82801GBM LPC */
#define	PCI_PRODUCT_INTEL_TIGER_LPC	0x27bc		/* Tigerpoint LPC Controller */
#define	PCI_PRODUCT_INTEL_82801GHM_LPC	0x27bd		/* 82801GHM LPC */
#define	PCI_PRODUCT_INTEL_82801GB_SATA	0x27c0		/* 82801GB SATA */
#define	PCI_PRODUCT_INTEL_82801GR_AHCI	0x27c1		/* 82801GR AHCI */
#define	PCI_PRODUCT_INTEL_82801GR_RAID	0x27c3		/* 82801GR RAID */
#define	PCI_PRODUCT_INTEL_82801GBM_SATA	0x27c4		/* 82801GBM SATA */
#define	PCI_PRODUCT_INTEL_82801GBM_AHCI	0x27c5		/* 82801GBM AHCI */
#define	PCI_PRODUCT_INTEL_82801GHM_RAID	0x27c6		/* 82801GHM RAID */
#define	PCI_PRODUCT_INTEL_82801GB_USB_1	0x27c8		/* 82801GB USB */
#define	PCI_PRODUCT_INTEL_82801GB_USB_2	0x27c9		/* 82801GB USB */
#define	PCI_PRODUCT_INTEL_82801GB_USB_3	0x27ca		/* 82801GB USB */
#define	PCI_PRODUCT_INTEL_82801GB_USB_4	0x27cb		/* 82801GB USB */
#define	PCI_PRODUCT_INTEL_82801GB_USB_5	0x27cc		/* 82801GB USB */
#define	PCI_PRODUCT_INTEL_82801GB_PCIE_1	0x27d0		/* 82801GB PCIE */
#define	PCI_PRODUCT_INTEL_82801GB_PCIE_2	0x27d2		/* 82801GB PCIE */
#define	PCI_PRODUCT_INTEL_82801GB_PCIE_3	0x27d4		/* 82801GB PCIE */
#define	PCI_PRODUCT_INTEL_82801GB_PCIE_4	0x27d6		/* 82801GB PCIE */
#define	PCI_PRODUCT_INTEL_82801GB_HDA	0x27d8		/* 82801GB HD Audio */
#define	PCI_PRODUCT_INTEL_82801GB_SMB	0x27da		/* 82801GB SMBus */
#define	PCI_PRODUCT_INTEL_82801GB_LAN	0x27dc		/* 82801GB LAN */
#define	PCI_PRODUCT_INTEL_82801GB_ACM	0x27dd		/* 82801GB Modem */
#define	PCI_PRODUCT_INTEL_82801GB_ACA	0x27de		/* 82801GB AC97 */
#define	PCI_PRODUCT_INTEL_82801GB_IDE	0x27df		/* 82801GB IDE */
#define	PCI_PRODUCT_INTEL_82801G_PCIE_5	0x27e0		/* 82801G PCIE */
#define	PCI_PRODUCT_INTEL_82801G_PCIE_6	0x27e2		/* 82801G PCIE */
#define	PCI_PRODUCT_INTEL_82801H_LPC	0x2810		/* 82801H LPC */
#define	PCI_PRODUCT_INTEL_82801HEM_LPC	0x2811		/* 82801HEM LPC */
#define	PCI_PRODUCT_INTEL_82801HH_LPC	0x2812		/* 82801HH LPC */
#define	PCI_PRODUCT_INTEL_82801HO_LPC	0x2814		/* 82801HO LPC */
#define	PCI_PRODUCT_INTEL_82801HBM_LPC	0x2815		/* 82801HBM LPC */
#define	PCI_PRODUCT_INTEL_82801H_SATA_1	0x2820		/* 82801H SATA */
#define	PCI_PRODUCT_INTEL_82801H_AHCI_6P	0x2821		/* 82801H AHCI */
#define	PCI_PRODUCT_INTEL_82801H_RAID	0x2822		/* 82801H RAID */
#define	PCI_PRODUCT_INTEL_82801H_AHCI_4P	0x2824		/* 82801H AHCI */
#define	PCI_PRODUCT_INTEL_82801H_SATA_2	0x2825		/* 82801H SATA */
#define	PCI_PRODUCT_INTEL_82801HBM_SATA	0x2828		/* 82801HBM SATA */
#define	PCI_PRODUCT_INTEL_82801HBM_AHCI	0x2829		/* 82801HBM AHCI */
#define	PCI_PRODUCT_INTEL_82801HBM_RAID	0x282a		/* 82081HBM RAID */
#define	PCI_PRODUCT_INTEL_82801H_SMB	0x283e		/* 82801H SMBus */
#define	PCI_PRODUCT_INTEL_82801H_DMI	0x284f		/* 82801H DMI-PCI */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_1	0x2830		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_2	0x2831		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_3	0x2832		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_6	0x2833		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_4	0x2834		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_UHCI_5	0x2835		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_EHCI_1	0x2836		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_EHCI_2	0x283a		/* 82801H USB */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_1	0x283f		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_2	0x2841		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_3	0x2843		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_4	0x2845		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_5	0x2847		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_PCIE_6	0x2849		/* 82801H PCIE */
#define	PCI_PRODUCT_INTEL_82801H_HDA	0x284b		/* 82801H HD Audio */
#define	PCI_PRODUCT_INTEL_82801H_TS	0x284f		/* 82801H Thermal */
#define	PCI_PRODUCT_INTEL_82801HBM_IDE	0x2850		/* 82801HBM IDE */
#define	PCI_PRODUCT_INTEL_82801IH_LPC	0x2912		/* 82801IH LPC */
#define	PCI_PRODUCT_INTEL_82801IO_LPC	0x2914		/* 82801IO LPC */
#define	PCI_PRODUCT_INTEL_82801IR_LPC	0x2916		/* 82801IR LPC */
#define	PCI_PRODUCT_INTEL_82801IEM_LPC	0x2917		/* 82801IEM LPC */
#define	PCI_PRODUCT_INTEL_82801IB_LPC	0x2918		/* 82801IB LPC */
#define	PCI_PRODUCT_INTEL_82801IBM_LPC	0x2919		/* 82801IBM LPC */
#define	PCI_PRODUCT_INTEL_82801I_SATA_1	0x2920		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_SATA_2	0x2921		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_AHCI_1	0x2922		/* 82801I AHCI */
#define	PCI_PRODUCT_INTEL_82801I_AHCI_2	0x2923		/* 82801I AHCI */
#define	PCI_PRODUCT_INTEL_82801I_SATA_3	0x2926		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_SATA_4	0x2928		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_AHCI_3	0x2929		/* 82801I AHCI */
#define	PCI_PRODUCT_INTEL_82801I_RAID	0x292a		/* 82801I RAID */
#define	PCI_PRODUCT_INTEL_82801I_SATA_5	0x292d		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_SATA_6	0x292e		/* 82801I SATA */
#define	PCI_PRODUCT_INTEL_82801I_SMB	0x2930		/* 82801I SMBus */
#define	PCI_PRODUCT_INTEL_82801I_TS	0x2932		/* 82801I Thermal */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_1	0x2934		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_2	0x2935		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_3	0x2936		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_4	0x2937		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_5	0x2938		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_UHCI_6	0x2939		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_EHCI_1	0x293a		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_EHCI_2	0x293c		/* 82801I USB */
#define	PCI_PRODUCT_INTEL_82801I_HDA	0x293e		/* 82801I HD Audio */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_1	0x2940		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_2	0x2942		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_3	0x2944		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_4	0x2946		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_5	0x2948		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_82801I_PCIE_6	0x294a		/* 82801I PCIE */
#define	PCI_PRODUCT_INTEL_ICH9_IGP_C	0x294c		/* ICH9 IGP C */
#define	PCI_PRODUCT_INTEL_82946GZ_HB	0x2970		/* 82946GZ Host */
#define	PCI_PRODUCT_INTEL_82946GZ_PCIE	0x2971		/* 82946GZ PCIE */
#define	PCI_PRODUCT_INTEL_82946GZ_IGD_1	0x2972		/* 82946GZ Video */
#define	PCI_PRODUCT_INTEL_82946GZ_IGD_2	0x2973		/* 82946GZ Video */
#define	PCI_PRODUCT_INTEL_82946GZ_HECI_1	0x2974		/* 82946GZ HECI */
#define	PCI_PRODUCT_INTEL_82946GZ_HECI_2	0x2975		/* 82946GZ HECI */
#define	PCI_PRODUCT_INTEL_82946GZ_PT_IDER	0x2976		/* 82946GZ PT IDER */
#define	PCI_PRODUCT_INTEL_82946GZ_KT	0x2977		/* 82946GZ KT */
#define	PCI_PRODUCT_INTEL_82G35_HB	0x2980		/* 82G35 Host */
#define	PCI_PRODUCT_INTEL_82G35_PCIE	0x2981		/* 82G35 PCIE */
#define	PCI_PRODUCT_INTEL_82G35_IGD_1	0x2982		/* 82G35 Video */
#define	PCI_PRODUCT_INTEL_82G35_IGD_2	0x2983		/* 82G35 Video */
#define	PCI_PRODUCT_INTEL_82G35_HECI	0x2984		/* 82G35 HECI */
#define	PCI_PRODUCT_INTEL_82Q965_HB	0x2990		/* 82Q965 Host */
#define	PCI_PRODUCT_INTEL_82Q965_PCIE	0x2991		/* 82Q965 PCIE */
#define	PCI_PRODUCT_INTEL_82Q965_IGD_1	0x2992		/* 82Q965 Video */
#define	PCI_PRODUCT_INTEL_82Q965_IGD_2	0x2993		/* 82Q965 Video */
#define	PCI_PRODUCT_INTEL_82Q965_HECI_1	0x2994		/* 82Q965 HECI */
#define	PCI_PRODUCT_INTEL_82Q965_HECI_2	0x2995		/* 82Q965 HECI */
#define	PCI_PRODUCT_INTEL_82Q965_PT_IDER	0x2996		/* 82Q965 PT IDER */
#define	PCI_PRODUCT_INTEL_82Q965_KT	0x2997		/* 82Q965 KT */
#define	PCI_PRODUCT_INTEL_82G965_HB	0x29a0		/* 82G965 Host */
#define	PCI_PRODUCT_INTEL_82G965_PCIE	0x29a1		/* 82G965 PCIE */
#define	PCI_PRODUCT_INTEL_82G965_IGD_1	0x29a2		/* 82G965 Video */
#define	PCI_PRODUCT_INTEL_82G965_IGD_2	0x29a3		/* 82G965 Video */
#define	PCI_PRODUCT_INTEL_82G965_HECI_1	0x29a4		/* 82G965 HECI */
#define	PCI_PRODUCT_INTEL_82G965_HECI_2	0x29a5		/* 82G965 HECI */
#define	PCI_PRODUCT_INTEL_82G965_PT_IDER	0x29a6		/* 82G965 PT IDER */
#define	PCI_PRODUCT_INTEL_82G965_KT	0x29a7		/* 82G965 KT */
#define	PCI_PRODUCT_INTEL_82Q35_HB	0x29b0		/* 82Q35 Host */
#define	PCI_PRODUCT_INTEL_82Q35_PCIE	0x29b1		/* 82Q35 PCIE */
#define	PCI_PRODUCT_INTEL_82Q35_IGD_1	0x29b2		/* 82Q35 Video */
#define	PCI_PRODUCT_INTEL_82Q35_IGD_2	0x29b3		/* 82Q35 Video */
#define	PCI_PRODUCT_INTEL_82Q35_HECI_1	0x29b4		/* 82Q35 HECI */
#define	PCI_PRODUCT_INTEL_82Q35_HECI_2	0x29b5		/* 82Q35 HECI */
#define	PCI_PRODUCT_INTEL_82Q35_PT_IDER	0x29b6		/* 82Q35 PT IDER */
#define	PCI_PRODUCT_INTEL_82Q35_KT	0x29b7		/* 82Q35 KT */
#define	PCI_PRODUCT_INTEL_82G33_HB	0x29c0		/* 82G33 Host */
#define	PCI_PRODUCT_INTEL_82G33_PCIE	0x29c1		/* 82G33 PCIE */
#define	PCI_PRODUCT_INTEL_82G33_IGD_1	0x29c2		/* 82G33 Video */
#define	PCI_PRODUCT_INTEL_82G33_IGD_2	0x29c3		/* 82G33 Video */
#define	PCI_PRODUCT_INTEL_82G33_HECI_1	0x29c4		/* 82G33 HECI */
#define	PCI_PRODUCT_INTEL_82G33_HECI_2	0x29c5		/* 82G33 HECI */
#define	PCI_PRODUCT_INTEL_82G33_PT_IDER	0x29c6		/* 82G33 PT IDER */
#define	PCI_PRODUCT_INTEL_82G33_KT	0x29c7		/* 82G33 KT */
#define	PCI_PRODUCT_INTEL_82Q33_HB	0x29d0		/* 82Q33 Host */
#define	PCI_PRODUCT_INTEL_82Q33_PCIE	0x29d1		/* 82Q33 PCIE */
#define	PCI_PRODUCT_INTEL_82Q33_IGD_1	0x29d2		/* 82Q33 Video */
#define	PCI_PRODUCT_INTEL_82Q33_IGD_2	0x29d3		/* 82Q33 Video */
#define	PCI_PRODUCT_INTEL_82Q33_HECI_1	0x29d4		/* 82Q33 HECI */
#define	PCI_PRODUCT_INTEL_82Q33_HECI_2	0x29d5		/* 82Q33 HECI */
#define	PCI_PRODUCT_INTEL_82Q33_PT_IDER	0x29d6		/* 82Q33 PT IDER */
#define	PCI_PRODUCT_INTEL_82Q33_KT	0x29d7		/* 82Q33 KT */
#define	PCI_PRODUCT_INTEL_82X38_HB	0x29e0		/* 82X38 Host */
#define	PCI_PRODUCT_INTEL_82X38_PCIE_1	0x29e1		/* 82X38 PCIE */
#define	PCI_PRODUCT_INTEL_82X38_HECI_1	0x29e4		/* 82X38 HECI */
#define	PCI_PRODUCT_INTEL_82X38_HECI_2	0x29e5		/* 82X38 HECI */
#define	PCI_PRODUCT_INTEL_82X38_PT_IDER	0x29e6		/* 82X38 PT IDER */
#define	PCI_PRODUCT_INTEL_82X38_KT	0x29e7		/* 82X38 KT */
#define	PCI_PRODUCT_INTEL_82X38_PCIE_2	0x29e9		/* 82X38 PCIE */
#define	PCI_PRODUCT_INTEL_3200_HB	0x29f0		/* 3200/3210 Host */
#define	PCI_PRODUCT_INTEL_3200_PCIE	0x29f1		/* 3200/3210 PCIE */
#define	PCI_PRODUCT_INTEL_3210_PCIE	0x29f9		/* 3210 PCIE */
#define	PCI_PRODUCT_INTEL_82GM965_HB	0x2a00		/* GM965 Host */
#define	PCI_PRODUCT_INTEL_82GM965_PCIE	0x2a01		/* GM965 PCIE */
#define	PCI_PRODUCT_INTEL_82GM965_IGD_1	0x2a02		/* GM965 Video */
#define	PCI_PRODUCT_INTEL_82GM965_IGD_2	0x2a03		/* GM965 Video */
#define	PCI_PRODUCT_INTEL_82GM965_KT	0x2a07		/* GM965 KT */
#define	PCI_PRODUCT_INTEL_82GM965_PT_IDER	0x2a06		/* GM965 PT IDER */
#define	PCI_PRODUCT_INTEL_82GME965_HB	0x2a10		/* GME965 Host */
#define	PCI_PRODUCT_INTEL_82GME965_PCIE	0x2a11		/* GME965 PCIE */
#define	PCI_PRODUCT_INTEL_82GME965_IGD_1	0x2a12		/* GME965 Video */
#define	PCI_PRODUCT_INTEL_82GME965_IGD_2	0x2a13		/* GME965 Video */
#define	PCI_PRODUCT_INTEL_82GME965_HECI_1	0x2a14		/* GME965 HECI */
#define	PCI_PRODUCT_INTEL_82GME965_HECI_2	0x2a15		/* GME965 HECI */
#define	PCI_PRODUCT_INTEL_82GME965_PT_IDER	0x2a16		/* GME965 PT IDER */
#define	PCI_PRODUCT_INTEL_82GME965_KT	0x2a17		/* GME965 KT */
#define	PCI_PRODUCT_INTEL_82GM45_HB	0x2a40		/* GM45 Host */
#define	PCI_PRODUCT_INTEL_82GM45_PCIE	0x2a41		/* GM45 PCIE */
#define	PCI_PRODUCT_INTEL_82GM45_IGD_1	0x2a42		/* GM45 Video */
#define	PCI_PRODUCT_INTEL_82GM45_IGD_2	0x2a43		/* GM45 Video */
#define	PCI_PRODUCT_INTEL_82GM45_HECI_1	0x2a44		/* GM45 HECI */
#define	PCI_PRODUCT_INTEL_82GM45_HECI_2	0x2a45		/* GM45 HECI */
#define	PCI_PRODUCT_INTEL_82GM45_PT_IDER	0x2a46		/* GM45 PT IDER */
#define	PCI_PRODUCT_INTEL_82GM45_AMT_SOL	0x2a47		/* GM45 AMT SOL */
#define	PCI_PRODUCT_INTEL_82Q45_HB	0x2e10		/* Q45 Host */
#define	PCI_PRODUCT_INTEL_82Q45_PCIE	0x2e11		/* Q45 PCIE */
#define	PCI_PRODUCT_INTEL_82Q45_IGD_1	0x2e12		/* Q45 Video */
#define	PCI_PRODUCT_INTEL_82Q45_IGD_2	0x2e13		/* Q45 Video */
#define	PCI_PRODUCT_INTEL_82Q45_HECI_1	0x2e14		/* Q45 HECI */
#define	PCI_PRODUCT_INTEL_82Q45_HECI_2	0x2e15		/* Q45 HECI */
#define	PCI_PRODUCT_INTEL_82Q45_PT_IDER	0x2e16		/* Q45 PT IDER */
#define	PCI_PRODUCT_INTEL_82Q45_KT	0x2e17		/* Q45 KT */
#define	PCI_PRODUCT_INTEL_82G45_HB	0x2e20		/* G45 Host */
#define	PCI_PRODUCT_INTEL_82G45_PCIE	0x2e21		/* G45 PCIE */
#define	PCI_PRODUCT_INTEL_82G45_IGD_1	0x2e22		/* G45 Video */
#define	PCI_PRODUCT_INTEL_82G45_IGD_2	0x2e23		/* G45 Video */
#define	PCI_PRODUCT_INTEL_82G45_PCIE_1	0x2e29		/* G45 PCIE */
#define	PCI_PRODUCT_INTEL_82G41_HB	0x2e30		/* G41 Host */
#define	PCI_PRODUCT_INTEL_82G41_IGD_1	0x2e32		/* G41 Video */
#define	PCI_PRODUCT_INTEL_82G41_IGD_2	0x2e33		/* G41 Video */
#define	PCI_PRODUCT_INTEL_RCU32	0x3092		/* RCU32 I2O RAID */
#define	PCI_PRODUCT_INTEL_3124	0x3124		/* 3124 SATA */
#define	PCI_PRODUCT_INTEL_31244	0x3200		/* 31244 SATA */
#define	PCI_PRODUCT_INTEL_82855PM_HB	0x3340		/* 82855PM Host */
#define	PCI_PRODUCT_INTEL_82855PM_AGP	0x3341		/* 82855PM AGP */
#define	PCI_PRODUCT_INTEL_82855PM_PM	0x3342		/* 82855PM Power */
#define	PCI_PRODUCT_INTEL_5500_HB	0x3403		/* 5500 Host */
#define	PCI_PRODUCT_INTEL_82X58_HB	0x3405		/* X58 Host */
#define	PCI_PRODUCT_INTEL_825520_HB	0x3406		/* 5520 Host */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_1	0x3408		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_2	0x3409		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_3	0x340a		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_4	0x340b		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_5	0x340c		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_6	0x340d		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_7	0x340e		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_8	0x340f		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_9	0x3410		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_PCIE_10	0x3411		/* X58 PCIE */
#define	PCI_PRODUCT_INTEL_82X58_QP0_PHY	0x3418		/* 5520/X58 QuickPath */
#define	PCI_PRODUCT_INTEL_5520_QP1_PHY	0x3419		/* 5520 QuickPath */
#define	PCI_PRODUCT_INTEL_82X58_GPIO	0x3422		/* X58 GPIO */
#define	PCI_PRODUCT_INTEL_82X58_RAS	0x3423		/* X58 RAS */
#define	PCI_PRODUCT_INTEL_82X58_QP0_P0	0x3425		/* X58 QuickPath */
#define	PCI_PRODUCT_INTEL_82X58_QP0_P1	0x3426		/* X58 QuickPath */
#define	PCI_PRODUCT_INTEL_82X58_QP1_P0	0x3427		/* X58 QuickPath */
#define	PCI_PRODUCT_INTEL_82X58_QP1_P1	0x3428		/* X58 QuickPath */
#define	PCI_PRODUCT_INTEL_82X58_QD_0	0x3429		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_1	0x342a		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_2	0x342b		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_3	0x342c		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_IOXAPIC	0x342d		/* X58 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82X58_MISC	0x342e		/* X58 Misc */
#define	PCI_PRODUCT_INTEL_82X58_QD_4	0x3430		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_5	0x3431		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_6	0x3432		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_QD_7	0x3433		/* X58 QuickData */
#define	PCI_PRODUCT_INTEL_82X58_THROTTLE	0x3438		/* X58 Throttle */
#define	PCI_PRODUCT_INTEL_82X58_TXT	0x343f		/* X58 TXT */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_5	0x3500		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_IOXAPIC	0x3504		/* 6321ESB IOxAPIC */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIX	0x350c		/* 6321ESB PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_6	0x3510		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_7	0x3511		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_8	0x3514		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_9	0x3515		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_10	0x3518		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_6321ESB_PCIE_11	0x3519		/* 6321ESB PCIE */
#define	PCI_PRODUCT_INTEL_82830M_HB	0x3575		/* 82830M Host */
#define	PCI_PRODUCT_INTEL_82830M_AGP	0x3576		/* 82830M AGP */
#define	PCI_PRODUCT_INTEL_82830M_IGD	0x3577		/* 82830M Video */
#define	PCI_PRODUCT_INTEL_82855GM_HB	0x3580		/* 82855GM Host */
#define	PCI_PRODUCT_INTEL_82855GME_AGP	0x3581		/* 82855GME AGP */
#define	PCI_PRODUCT_INTEL_82855GM_IGD	0x3582		/* 82855GM Video */
#define	PCI_PRODUCT_INTEL_82855GM_MEM	0x3584		/* 82855GM Memory */
#define	PCI_PRODUCT_INTEL_82855GM_CFG	0x3585		/* 82855GM Config */
#define	PCI_PRODUCT_INTEL_82854_HB	0x358c		/* 82854 Host */
#define	PCI_PRODUCT_INTEL_82854_IGD	0x358e		/* 82854 Video */
#define	PCI_PRODUCT_INTEL_E7520_HB	0x3590		/* E7520 Host */
#define	PCI_PRODUCT_INTEL_E7520_ERR	0x3591		/* E7520 Error Reporting */
#define	PCI_PRODUCT_INTEL_E7320_HB	0x3592		/* E7320 Host */
#define	PCI_PRODUCT_INTEL_E7320_ERR	0x3593		/* E7320 Error Reporting */
#define	PCI_PRODUCT_INTEL_E7520_DMA	0x3594		/* E7520 DMA */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_A0	0x3595		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_A1	0x3596		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_B0	0x3597		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_B1	0x3598		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_C0	0x3599		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_PCIE_C1	0x359a		/* E7520 PCIE */
#define	PCI_PRODUCT_INTEL_E7520_CFG	0x359b		/* E7520 Config */
#define	PCI_PRODUCT_INTEL_E7525_HB	0x359e		/* E7525 Host */
#define	PCI_PRODUCT_INTEL_3100_HB	0x35b0		/* 3100 Host */
#define	PCI_PRODUCT_INTEL_3100_ERR	0x35b1		/* 3100 Error Reporting */
#define	PCI_PRODUCT_INTEL_3100_EDMA	0x35b6		/* 3100 EDMA */
#define	PCI_PRODUCT_INTEL_3100_PCIE_1	0x35b6		/* 3100 PCIE */
#define	PCI_PRODUCT_INTEL_3100_PCIE_2	0x35b7		/* 3100 PCIE */
#define	PCI_PRODUCT_INTEL_7300_HB	0x3600		/* 7300 Host */
#define	PCI_PRODUCT_INTEL_7300_PCIE_1	0x3604		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_2	0x3605		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_3	0x3606		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_4	0x3607		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_5	0x3608		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_6	0x3609		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_7300_PCIE_7	0x360a		/* 7300 PCIE */
#define	PCI_PRODUCT_INTEL_IOAT_CNB	0x360b		/* I/OAT CNB */
#define	PCI_PRODUCT_INTEL_7300_FSBINT	0x360c		/* 7300 E5400 FSB/Boot/Interrupt */
#define	PCI_PRODUCT_INTEL_7300_SNOOP	0x360d		/* 7300 Snoop Filter */
#define	PCI_PRODUCT_INTEL_7300_MISC	0x360e		/* 7300 Misc */
#define	PCI_PRODUCT_INTEL_7300_FBD_0	0x360f		/* 7300 FBD */
#define	PCI_PRODUCT_INTEL_7300_FBD_1	0x3610		/* 7300 FBD */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_1	0x3a00		/* 82801JD SATA */
#define	PCI_PRODUCT_INTEL_82801JD_AHCI	0x3a02		/* 82801JD AHCI */
#define	PCI_PRODUCT_INTEL_82801JD_RAID	0x3a05		/* 82801JD RAID */
#define	PCI_PRODUCT_INTEL_82801JD_SATA_2	0x3a06		/* 82801JD SATA */
#define	PCI_PRODUCT_INTEL_82801JDO_LPC	0x3a14		/* 82801JDO LPC */
#define	PCI_PRODUCT_INTEL_82801JIR_LPC	0x3a16		/* 82801JIR LPC */
#define	PCI_PRODUCT_INTEL_82801JIB_LPC	0x3a18		/* 82801JIB LPC */
#define	PCI_PRODUCT_INTEL_82801JD_LPC	0x3a1a		/* 82801JD LPC */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_1	0x3a20		/* 82801JI SATA */
#define	PCI_PRODUCT_INTEL_82801JI_AHCI	0x3a22		/* 82801JI AHCI */
#define	PCI_PRODUCT_INTEL_82801JI_RAID	0x3a25		/* 82801JI RAID */
#define	PCI_PRODUCT_INTEL_82801JI_SATA_2	0x3a26		/* 82801JI SATA */
#define	PCI_PRODUCT_INTEL_82801JI_SMB	0x3a30		/* 82801JI SMBus */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_1	0x3a34		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_2	0x3a35		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_3	0x3a36		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_4	0x3a37		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_5	0x3a38		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_UHCI_6	0x3a39		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_EHCI_1	0x3a3a		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_EHCI_2	0x3a3c		/* 82801JI USB */
#define	PCI_PRODUCT_INTEL_82801JI_HDA	0x3a3e		/* 82801JI HD Audio */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_1	0x3a40		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_2	0x3a42		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_3	0x3a44		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_4	0x3a46		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_5	0x3a48		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JI_PCIE_6	0x3a4a		/* 82801JI PCIE */
#define	PCI_PRODUCT_INTEL_82801JDO_VECI	0x3a51		/* 82801JDO VECI */
#define	PCI_PRODUCT_INTEL_82801JD_VSATA	0x3a55		/* 82801JD Virtual SATA */
#define	PCI_PRODUCT_INTEL_82801JD_SMB	0x3a60		/* 82801JD SMBus */
#define	PCI_PRODUCT_INTEL_82801JD_THERMAL	0x3a62		/* 82801JD Thermal */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_1	0x3a64		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_2	0x3a65		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_3	0x3a66		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_4	0x3a67		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_5	0x3a68		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_UHCI_6	0x3a69		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_EHCI_1	0x3a6a		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_EHCI_2	0x3a6c		/* 82801JD USB */
#define	PCI_PRODUCT_INTEL_82801JD_HDA	0x3a6e		/* 82801JD HD Audio */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_1	0x3a70		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_2	0x3a72		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_3	0x3a74		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_4	0x3a76		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_5	0x3a78		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_PCIE_6	0x3a7a		/* 82801JD PCIE */
#define	PCI_PRODUCT_INTEL_82801JD_LAN	0x3a7c		/* 82801JD LAN */
#define	PCI_PRODUCT_INTEL_P55_LPC	0x3b02		/* P55 LPC */
#define	PCI_PRODUCT_INTEL_3400_LPC	0x3b14		/* 3400 LPC */
#define	PCI_PRODUCT_INTEL_3400_SATA_1	0x3b20		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_2	0x3b21		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_1	0x3b22		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_AHCI_2	0x3b23		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_RAID_1	0x3b25		/* 3400 RAID */
#define	PCI_PRODUCT_INTEL_3400_SATA_3	0x3b26		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_4	0x3b28		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_3	0x3b29		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_RAID_2	0x3b2c		/* 3400 RAID */
#define	PCI_PRODUCT_INTEL_3400_SATA_5	0x3b2d		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_SATA_6	0x3b2e		/* 3400 SATA */
#define	PCI_PRODUCT_INTEL_3400_AHCI_4	0x3b2f		/* 3400 AHCI */
#define	PCI_PRODUCT_INTEL_3400_SMB	0x3b30		/* 3400 SMBus */
#define	PCI_PRODUCT_INTEL_3400_THERMAL	0x3b32		/* 3400 Thermal */
#define	PCI_PRODUCT_INTEL_3400_EHCI_1	0x3b34		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_1	0x3b36		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_2	0x3b37		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_3	0x3b38		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_4	0x3b39		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_5	0x3b3a		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_6	0x3b3b		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_EHCI_2	0x3b3c		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_7	0x3b3e		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_UHCI_8	0x3b3f		/* 3400 USB */
#define	PCI_PRODUCT_INTEL_3400_PCIE_1	0x3b42		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_2	0x3b44		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_3	0x3b46		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_4	0x3b48		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_5	0x3b4a		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_6	0x3b4c		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_7	0x3b4e		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_PCIE_8	0x3b50		/* 3400 PCIE */
#define	PCI_PRODUCT_INTEL_3400_HDA	0x3b56		/* 3400 HD Audio */
#define	PCI_PRODUCT_INTEL_E5400_HB	0x4000		/* E5400 Host */
#define	PCI_PRODUCT_INTEL_E5400A_HB	0x4001		/* E5400A Host */
#define	PCI_PRODUCT_INTEL_E5400B_HB	0x4003		/* E5400B Host */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_1	0x4021		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_2	0x4022		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_3	0x4023		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_4	0x4024		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_5	0x4025		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_6	0x4026		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_7	0x4027		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_8	0x4028		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_E5400_PCIE_9	0x4029		/* E5400 PCIE */
#define	PCI_PRODUCT_INTEL_IOAT_SNB	0x402f		/* I/OAT SNB */
#define	PCI_PRODUCT_INTEL_E5400_FSBINT	0x4030		/* E5400 FSB/Boot/Interrupt */
#define	PCI_PRODUCT_INTEL_E5400_CE	0x4031		/* E5400 Coherency Engine */
#define	PCI_PRODUCT_INTEL_E5400_IOAPIC	0x4032		/* E5400 IOAPIC */
#define	PCI_PRODUCT_INTEL_E5400_RAS_0	0x4035		/* E5400 RAS */
#define	PCI_PRODUCT_INTEL_E5400_RAS_1	0x4036		/* E5400 RAS */
#define	PCI_PRODUCT_INTEL_PRO_WL_2200BG	0x4220		/* PRO/Wireless 2200BG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2225BG	0x4221		/* PRO/Wireless 2225BG */
#define	PCI_PRODUCT_INTEL_PRO_WL_3945ABG_1	0x4222		/* PRO/Wireless 3945ABG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1	0x4223		/* PRO/Wireless 2915ABG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2	0x4224		/* PRO/Wireless 2915ABG */
#define	PCI_PRODUCT_INTEL_PRO_WL_3945ABG_2	0x4227		/* PRO/Wireless 3945ABG */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_1	0x4229		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_1	0x422b		/* Centrino Ultimate-N 6300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_1	0x422c		/* Centrino Advanced-N 6200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_4965_2	0x4230		/* Wireless WiFi Link 4965 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5100_1	0x4232		/* WiFi Link 5100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5300_1	0x4235		/* WiFi Link 5300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5300_2	0x4236		/* WiFi Link 5300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5100_2	0x4237		/* WiFi Link 5100 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_3X3_2	0x4238		/* Centrino Ultimate-N 6300 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_6000_IPA_2	0x4239		/* Centrino Advanced-N 6200 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5350_1	0x423a		/* WiFi Link 5350 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5350_2	0x423b		/* WiFi Link 5350 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5150_1	0x423c		/* WiFi Link 5150 */
#define	PCI_PRODUCT_INTEL_WIFI_LINK_5150_2	0x423d		/* WiFi Link 5150 */
#define	PCI_PRODUCT_INTEL_TURBO_MEMORY	0x444e		/* Turbo Memory */
#define	PCI_PRODUCT_INTEL_EP80579_HB	0x5020		/* EP80579 Host */
#define	PCI_PRODUCT_INTEL_EP80579_MEM	0x5021		/* EP80579 Memory */
#define	PCI_PRODUCT_INTEL_EP80579_EDMA	0x5023		/* EP80579 EDMA */
#define	PCI_PRODUCT_INTEL_EP80579_PCIE_1	0x5024		/* EP80579 PCIE */
#define	PCI_PRODUCT_INTEL_EP80579_PCIE_2	0x5025		/* EP80579 PCIE */
#define	PCI_PRODUCT_INTEL_EP80579_SATA	0x5028		/* EP80579 SATA */
#define	PCI_PRODUCT_INTEL_EP80579_AHCI	0x5029		/* EP80579 AHCI */
#define	PCI_PRODUCT_INTEL_EP80579_ASU	0x502c		/* EP80579 ASU */
#define	PCI_PRODUCT_INTEL_EP80579_RESERVED1	0x5030		/* EP80579 Reserved */
#define	PCI_PRODUCT_INTEL_EP80579_LPC	0x5031		/* EP80579 LPC */
#define	PCI_PRODUCT_INTEL_EP80579_SMBUS	0x5032		/* EP80579 SMBus */
#define	PCI_PRODUCT_INTEL_EP80579_UHCI	0x5033		/* EP80579 USB */
#define	PCI_PRODUCT_INTEL_EP80579_EHCI	0x5035		/* EP80579 USB */
#define	PCI_PRODUCT_INTEL_EP80579_PPB	0x5037		/* EP80579 PCI-PCI */
#define	PCI_PRODUCT_INTEL_EP80579_CAN_1	0x5039		/* EP80579 CANbus */
#define	PCI_PRODUCT_INTEL_EP80579_CAN_2	0x503a		/* EP80579 CANbus */
#define	PCI_PRODUCT_INTEL_EP80579_SERIAL	0x503b		/* EP80579 Serial */
#define	PCI_PRODUCT_INTEL_EP80579_1588	0x503c		/* EP80579 1588 */
#define	PCI_PRODUCT_INTEL_EP80579_LEB	0x503d		/* EP80579 LEB */
#define	PCI_PRODUCT_INTEL_EP80579_GCU	0x503e		/* EP80579 GCU */
#define	PCI_PRODUCT_INTEL_EP80579_RESERVED2	0x503f		/* EP80579 Reserved */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_1	0x5040		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_2	0x5044		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_EP80579_LAN_3	0x5048		/* EP80579 LAN */
#define	PCI_PRODUCT_INTEL_80960RD	0x5200		/* i960 RD PCI-PCI */
#define	PCI_PRODUCT_INTEL_PRO_100_SERVER	0x5201		/* PRO 100 Server */
#define	PCI_PRODUCT_INTEL_5100_HB	0x65c0		/* 5100 Host */
#define	PCI_PRODUCT_INTEL_5100_PCIE_2	0x65e2		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_3	0x65e3		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_4	0x65e4		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_5	0x65e5		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_6	0x65e6		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_7	0x65e7		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_FSB	0x65f0		/* 5100 FSB */
#define	PCI_PRODUCT_INTEL_5100_RESERVED_1	0x65f1		/* 5100 Reserved */
#define	PCI_PRODUCT_INTEL_5100_RESERVED_2	0x65f3		/* 5100 Reserved */
#define	PCI_PRODUCT_INTEL_5100_DDR	0x65f5		/* 5100 DDR */
#define	PCI_PRODUCT_INTEL_5100_DDR2	0x65f6		/* 5100 DDR */
#define	PCI_PRODUCT_INTEL_5100_PCIE_23	0x65f7		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_45	0x65f8		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_67	0x65f9		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_5100_PCIE_47	0x65fa		/* 5100 PCIE */
#define	PCI_PRODUCT_INTEL_IOAT_SCNB	0x65ff		/* I/OAT SCNB */
#define	PCI_PRODUCT_INTEL_82371SB_ISA	0x7000		/* 82371SB ISA */
#define	PCI_PRODUCT_INTEL_82371SB_IDE	0x7010		/* 82371SB IDE */
#define	PCI_PRODUCT_INTEL_82371USB	0x7020		/* 82371SB USB */
#define	PCI_PRODUCT_INTEL_82437VX	0x7030		/* 82437VX */
#define	PCI_PRODUCT_INTEL_82439TX	0x7100		/* 82439TX System */
#define	PCI_PRODUCT_INTEL_82371AB_ISA	0x7110		/* 82371AB PIIX4 ISA */
#define	PCI_PRODUCT_INTEL_82371AB_IDE	0x7111		/* 82371AB IDE */
#define	PCI_PRODUCT_INTEL_82371AB_USB	0x7112		/* 82371AB USB */
#define	PCI_PRODUCT_INTEL_82371AB_PM	0x7113		/* 82371AB Power */
#define	PCI_PRODUCT_INTEL_82810_HB	0x7120		/* 82810 Host */
#define	PCI_PRODUCT_INTEL_82810_IGD	0x7121		/* 82810 Video */
#define	PCI_PRODUCT_INTEL_82810_DC100_HB	0x7122		/* 82810-DC100 Host */
#define	PCI_PRODUCT_INTEL_82810_DC100_IGD	0x7123		/* 82810-DC100 Video */
#define	PCI_PRODUCT_INTEL_82810E_HB	0x7124		/* 82810E Host */
#define	PCI_PRODUCT_INTEL_82810E_IGD	0x7125		/* 82810E Video */
#define	PCI_PRODUCT_INTEL_82443LX	0x7180		/* 82443LX AGP */
#define	PCI_PRODUCT_INTEL_82443LX_AGP	0x7181		/* 82443LX AGP */
#define	PCI_PRODUCT_INTEL_82443BX	0x7190		/* 82443BX AGP */
#define	PCI_PRODUCT_INTEL_82443BX_AGP	0x7191		/* 82443BX AGP */
#define	PCI_PRODUCT_INTEL_82443BX_NOAGP	0x7192		/* 82443BX */
#define	PCI_PRODUCT_INTEL_82440MX_HB	0x7194		/* 82440MX Host */
#define	PCI_PRODUCT_INTEL_82440MX_ACA	0x7195		/* 82440MX AC97 */
#define	PCI_PRODUCT_INTEL_82440MX_ACM	0x7196		/* 82440MX Modem */
#define	PCI_PRODUCT_INTEL_82440MX_ISA	0x7198		/* 82440MX ISA */
#define	PCI_PRODUCT_INTEL_82440MX_IDE	0x7199		/* 82440MX IDE */
#define	PCI_PRODUCT_INTEL_82440MX_USB	0x719a		/* 82440MX USB */
#define	PCI_PRODUCT_INTEL_82440MX_PM	0x719b		/* 82440MX Power */
#define	PCI_PRODUCT_INTEL_82440BX	0x71a0		/* 82440BX AGP */
#define	PCI_PRODUCT_INTEL_82440BX_AGP	0x71a1		/* 82440BX AGP */
#define	PCI_PRODUCT_INTEL_82443GX	0x71a2		/* 82443GX */
#define	PCI_PRODUCT_INTEL_82372FB_IDE	0x7601		/* 82372FB IDE */
#define	PCI_PRODUCT_INTEL_82740	0x7800		/* 82740 AGP */
#define	PCI_PRODUCT_INTEL_US15W_HB	0x8100		/* US15W Host */
#define	PCI_PRODUCT_INTEL_US15L_HB	0x8101		/* US15L/UL11L Host */
#define	PCI_PRODUCT_INTEL_US15W_IGD	0x8108		/* US15W Video */
#define	PCI_PRODUCT_INTEL_US15L_IGD	0x8109		/* US15L/UL11L Video */
#define	PCI_PRODUCT_INTEL_SCH_PCIE_1	0x8110		/* SCH PCIE */
#define	PCI_PRODUCT_INTEL_SCH_PCIE_2	0x8112		/* SCH PCIE */
#define	PCI_PRODUCT_INTEL_SCH_UHCI_1	0x8114		/* SCH USB */
#define	PCI_PRODUCT_INTEL_SCH_UHCI_2	0x8115		/* SCH USB */
#define	PCI_PRODUCT_INTEL_SCH_UHCI_3	0x8116		/* SCH USB */
#define	PCI_PRODUCT_INTEL_SCH_EHCI	0x8117		/* SCH USB */
#define	PCI_PRODUCT_INTEL_SCH_USBCL	0x8118		/* SCH USB Client */
#define	PCI_PRODUCT_INTEL_SCH_LPC	0x8119		/* SCH LPC */
#define	PCI_PRODUCT_INTEL_SCH_IDE	0x811a		/* SCH IDE */
#define	PCI_PRODUCT_INTEL_SCH_HDA	0x811b		/* SCH HD Audio */
#define	PCI_PRODUCT_INTEL_SCH_SDMMC_1	0x811c		/* SCH SD/MMC */
#define	PCI_PRODUCT_INTEL_SCH_SDMMC_2	0x811d		/* SCH SD/MMC */
#define	PCI_PRODUCT_INTEL_SCH_SDMMC_3	0x811e		/* SCH SD/MMC */
#define	PCI_PRODUCT_INTEL_PCI450_PB	0x84c4		/* 82450KX/GX */
#define	PCI_PRODUCT_INTEL_PCI450_MC	0x84c5		/* 82450KX/GX Memory */
#define	PCI_PRODUCT_INTEL_82451NX	0x84ca		/* 82451NX Mem & IO */
#define	PCI_PRODUCT_INTEL_82454NX	0x84cb		/* 82454NX PXB */
#define	PCI_PRODUCT_INTEL_82802AC	0x89ac		/* 82802AC Firmware Hub 8Mbit */
#define	PCI_PRODUCT_INTEL_82802AB	0x89ad		/* 82802AB Firmware Hub 4Mbit */
#define	PCI_PRODUCT_INTEL_I2OPCIB	0x9620		/* I2O RAID PCI-PCI */
#define	PCI_PRODUCT_INTEL_RCU21	0x9621		/* RCU21 I2O RAID */
#define	PCI_PRODUCT_INTEL_RCUxx	0x9622		/* RCUxx I2O RAID */
#define	PCI_PRODUCT_INTEL_RCU31	0x9641		/* RCU31 I2O RAID */
#define	PCI_PRODUCT_INTEL_RCU31L	0x96a1		/* RCU31L I2O RAID */
#define	PCI_PRODUCT_INTEL_PINEVIEW_DMI	0xa000		/* Pineview DMI Bridge */
#define	PCI_PRODUCT_INTEL_PINEVIEW_IGC_1	0xa001		/* Pineview Integrated Graphics Controller */
#define	PCI_PRODUCT_INTEL_PINEVIEW_IGC_2	0xa002		/* Pineview Integrated Graphics Controller */
#define	PCI_PRODUCT_INTEL_21152	0xb152		/* S21152BB PCI-PCI */
#define	PCI_PRODUCT_INTEL_21154	0xb154		/* 21154AE/BE PCI-PCI */
#define	PCI_PRODUCT_INTEL_CORE_DMI_0	0xd130		/* Core DMI */
#define	PCI_PRODUCT_INTEL_CORE_DMI_1	0xd131		/* Core DMI */
#define	PCI_PRODUCT_INTEL_CORE_PCIE_1	0xd138		/* Core PCIE */
#define	PCI_PRODUCT_INTEL_CORE_PCIE_2	0xd139		/* Core PCIE */
#define	PCI_PRODUCT_INTEL_CORE_PCIE_3	0xd13a		/* Core PCIE */
#define	PCI_PRODUCT_INTEL_CORE_PCIE_4	0xd13b		/* Core PCIE */
#define	PCI_PRODUCT_INTEL_CORE_QPI_L	0xd150		/* Core QPI Link */
#define	PCI_PRODUCT_INTEL_CORE_QPI_R	0xd151		/* Core QPI Routing */
#define	PCI_PRODUCT_INTEL_CORE_DMI_2	0xd152		/* Core DMI */
#define	PCI_PRODUCT_INTEL_CORE_DMI_3	0xd153		/* Core DMI */
#define	PCI_PRODUCT_INTEL_CORE_DMI_4	0xd154		/* Core DMI */
#define	PCI_PRODUCT_INTEL_CORE_MANAGEMENT	0xd155		/* Core Management */
#define	PCI_PRODUCT_INTEL_CORE_SCRATCH	0xd156		/* Core Scratch */
#define	PCI_PRODUCT_INTEL_CORE_CONTROL	0xd157		/* Core Control */
#define	PCI_PRODUCT_INTEL_CORE_MISC	0xd158		/* Core Misc */

/* Intergraph products */
#define	PCI_PRODUCT_INTERGRAPH_4D50T	0x00e4		/* Powerstorm 4D50T */
#define	PCI_PRODUCT_INTERGRAPH_INTENSE3D	0x00eb		/* Intense 3D */
#define	PCI_PRODUCT_INTERGRAPH_EXPERT3D	0x07a0		/* Expert3D */

/* Interphase products */
#define	PCI_PRODUCT_INTERPHASE_5526	0x0004		/* 5526 FibreChannel */

/* Intersil products */
#define	PCI_PRODUCT_INTERSIL_ISL3872	0x3872		/* PRISM3 */
#define	PCI_PRODUCT_INTERSIL_MINI_PCI_WLAN	0x3873		/* PRISM2.5 */
#define	PCI_PRODUCT_INTERSIL_ISL3877	0x3877		/* Prism Indigo */
#define	PCI_PRODUCT_INTERSIL_ISL3886	0x3886		/* Prism Javelin/Xbow */
#define	PCI_PRODUCT_INTERSIL_ISL3890	0x3890		/* Prism GT/Duette */

/* Invertex */
#define	PCI_PRODUCT_INVERTEX_AEON	0x0005		/* AEON */

/* IO Data Device Inc products */
#define	PCI_PRODUCT_IODATA_GV_BCTV3	0x4020		/* GV-BCTV3 */

/* I. T. T. products */
#define	PCI_PRODUCT_ITT_AGX016	0x0001		/* AGX016 */
#define	PCI_PRODUCT_ITT_ITT3204	0x0002		/* ITT3204 MPEG Decoder */

/* ITExpress */
#define	PCI_PRODUCT_ITEXPRESS_IT8211F	0x8211		/* IT8211F */
#define	PCI_PRODUCT_ITEXPRESS_IT8212F	0x8212		/* IT8212F */
#define	PCI_PRODUCT_ITEXPRESS_IT8213F	0x8213		/* IT8213F */
#define	PCI_PRODUCT_ITEXPRESS_IT8330G	0x8330		/* IT8330G */
#define	PCI_PRODUCT_ITEXPRESS_IT8888F_ISA	0x8888		/* IT8888F ISA */

/* JMicron */
#define	PCI_PRODUCT_JMICRON_JMC250	0x0250		/* JMC250 */
#define	PCI_PRODUCT_JMICRON_JMC260	0x0260		/* JMC260 */
#define	PCI_PRODUCT_JMICRON_JMB360	0x2360		/* JMB360 SATA */
#define	PCI_PRODUCT_JMICRON_JMB361	0x2361		/* JMB361 IDE/SATA */
#define	PCI_PRODUCT_JMICRON_JMB362	0x2362		/* JMB362 SATA */
#define	PCI_PRODUCT_JMICRON_JMB363	0x2363		/* JMB363 IDE/SATA */
#define	PCI_PRODUCT_JMICRON_JMB365	0x2365		/* JMB365 IDE/SATA */
#define	PCI_PRODUCT_JMICRON_JMB366	0x2366		/* JMB366 IDE/SATA */
#define	PCI_PRODUCT_JMICRON_JMB368	0x2368		/* JMB368 IDE */
#define	PCI_PRODUCT_JMICRON_FIREWIRE	0x2380		/* FireWire */
#define	PCI_PRODUCT_JMICRON_SD	0x2381		/* SD Host Controller */
#define	PCI_PRODUCT_JMICRON_SDMMC	0x2382		/* SD/MMC */
#define	PCI_PRODUCT_JMICRON_MS	0x2383		/* Memory Stick */
#define	PCI_PRODUCT_JMICRON_XD	0x2384		/* xD */

/* KTI */
#define	PCI_PRODUCT_KTI_KTIE	0x3000		/* KTI */

/* LAN Media Corporation */
#define	PCI_PRODUCT_LMC_HSSI	0x0003		/* HSSI */
#define	PCI_PRODUCT_LMC_DS3	0x0004		/* DS3 */
#define	PCI_PRODUCT_LMC_SSI	0x0005		/* SSI */
#define	PCI_PRODUCT_LMC_DS1	0x0006		/* DS1 */
#define	PCI_PRODUCT_LMC_HSSIC	0x0007		/* HSSIc */

/* Lanergy */
#define	PCI_PRODUCT_LANERGY_APPIAN_PCI_LITE	0x0001		/* Appian Lite */

/* Lava */
#define	PCI_PRODUCT_LAVA_TWOSP_2S	0x0100		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_AB	0x0101		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_CD	0x0102		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_IOFLEX_2S_0	0x0110		/* Serial */
#define	PCI_PRODUCT_LAVA_IOFLEX_2S_1	0x0111		/* Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_AB2	0x0120		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_QUATTRO_CD2	0x0121		/* Dual Serial */
#define	PCI_PRODUCT_LAVA_OCTOPUS550_0	0x0180		/* Quad Serial */
#define	PCI_PRODUCT_LAVA_OCTOPUS550_1	0x0181		/* Quad Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_2	0x0200		/* Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_0	0x0201		/* Serial */
#define	PCI_PRODUCT_LAVA_LAVAPORT_1	0x0202		/* Serial */
#define	PCI_PRODUCT_LAVA_650	0x0600		/* Serial */
#define	PCI_PRODUCT_LAVA_TWOSP_1P	0x8000		/* Parallel */
#define	PCI_PRODUCT_LAVA_PARALLEL2	0x8001		/* Dual Parallel */
#define	PCI_PRODUCT_LAVA_PARALLEL2A	0x8002		/* Dual Parallel */
#define	PCI_PRODUCT_LAVA_PARALLELB	0x8003		/* Dual Parallel */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */
#define	PCI_PRODUCT_LEADTEK_WINFAST	0x6606		/* Leadtek WinFast TV 2000 */
#define	PCI_PRODUCT_LEADTEK_WINFAST_XP	0x6609		/* Leadtek WinFast TV 2000 XP */

/* Level 1 (Intel) */
#define	PCI_PRODUCT_LEVEL1_LXT1001	0x0001		/* LXT1001 */

/* Linksys products */
#define	PCI_PRODUCT_LINKSYS_EG1032	0x1032		/* EG1032 */
#define	PCI_PRODUCT_LINKSYS_EG1064	0x1064		/* EG1064 */
#define	PCI_PRODUCT_LINKSYS_PCMPC200	0xab08		/* PCMPC200 */
#define	PCI_PRODUCT_LINKSYS_PCM200	0xab09		/* PCM200 */

/* Lite-On Communications */
#define	PCI_PRODUCT_LITEON_PNIC	0x0002		/* PNIC */
#define	PCI_PRODUCT_LITEON_PNICII	0xc115		/* PNIC-II */

/* Longshine products */
#define	PCI_PRODUCT_GLOBALSUN_8031	0x1103		/* 8031 */

/* Lucent products */
#define	PCI_PRODUCT_LUCENT_LTMODEM	0x0440		/* K56flex DSVD LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0441	0x0441		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0442	0x0442		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0443	0x0443		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0444	0x0444		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0445	0x0445		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0446	0x0446		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0447	0x0447		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0448	0x0448		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0449	0x0449		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044A	0x044a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044B	0x044b		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044C	0x044c		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044D	0x044d		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_044E	0x044e		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0450	0x0450		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0451	0x0451		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0452	0x0452		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0453	0x0453		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0454	0x0454		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0455	0x0455		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0456	0x0456		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0457	0x0457		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0458	0x0458		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_0459	0x0459		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_045A	0x045a		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_045C	0x045c		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_LTMODEM_048c	0x048c		/* LTMODEM */
#define	PCI_PRODUCT_LUCENT_VENUSMODEM	0x0480		/* Venus Modem */
#define	PCI_PRODUCT_LUCENT_USBHC	0x5801		/* USB */
#define	PCI_PRODUCT_LUCENT_USBHC2	0x5802		/* USB 2-port */
#define	PCI_PRODUCT_LUCENT_USBQBUS	0x5803		/* USB QuadraBus */
#define	PCI_PRODUCT_LUCENT_FW322	0x5811		/* FW322 1394 */
#define	PCI_PRODUCT_LUCENT_FW643	0x5901		/* FW643 1394 */
#define	PCI_PRODUCT_LUCENT_ET1310_GBE	0xed00		/* ET1310 */
#define	PCI_PRODUCT_LUCENT_ET1310_FE	0xed01		/* ET1310 */

/* LuxSonor */
#define	PCI_PRODUCT_LUXSONOR_LS242	0x0020		/* LS242 DVD Decoder */

/* Macronix */
#define	PCI_PRODUCT_MACRONIX_MX98713	0x0512		/* PMAC 98713 */
#define	PCI_PRODUCT_MACRONIX_MX98715	0x0531		/* PMAC 98715 */
#define	PCI_PRODUCT_MACRONIX_MX98727	0x0532		/* PMAC 98727 */
#define	PCI_PRODUCT_MACRONIX_MX86250	0x8625		/* MX86250 */

/* Madge Networks products */
#define	PCI_PRODUCT_MADGE_SMARTRN	0x0001		/* Smart 16/4 PCI Ringnode */
#define	PCI_PRODUCT_MADGE_SMARTRN2	0x0002		/* Smart 16/4 PCI Ringnode Mk2 */
#define	PCI_PRODUCT_MADGE_SMARTRN3	0x0003		/* Smart 16/4 PCI Ringnode Mk3 */
#define	PCI_PRODUCT_MADGE_SMARTRN1	0x0004		/* Smart 16/4 PCI Ringnode Mk1 */
#define	PCI_PRODUCT_MADGE_164CB	0x0006		/* 16/4 Cardbus */
#define	PCI_PRODUCT_MADGE_PRESTO	0x0007		/* Presto PCI */
#define	PCI_PRODUCT_MADGE_SMARTHSRN100	0x0009		/* Smart 100/16/4 PCI-HS Ringnode */
#define	PCI_PRODUCT_MADGE_SMARTRN100	0x000a		/* Smart 100/16/4 PCI Ringnode */
#define	PCI_PRODUCT_MADGE_164CB2	0x000b		/* 16/4 CardBus Mk2 */
#define	PCI_PRODUCT_MADGE_COLLAGE25	0x1000		/* Collage 25 ATM */
#define	PCI_PRODUCT_MADGE_COLLAGE155	0x1001		/* Collage 155 ATM */

/* Martin-Marietta */
#define	PCI_PRODUCT_MARTINMARIETTA_I740	0x00d1		/* i740 PCI */

/* Marvell products */
#define	PCI_PRODUCT_MARVELL_88W8300_1	0x1fa6		/* Libertas 88W8300 */
#define	PCI_PRODUCT_MARVELL_88W8310	0x1fa7		/* Libertas 88W8310 */
#define	PCI_PRODUCT_MARVELL_88W8335_1	0x1faa		/* Libertas 88W8335 */
#define	PCI_PRODUCT_MARVELL_88W8335_2	0x1fab		/* Libertas 88W8335 */
#define	PCI_PRODUCT_MARVELL_88W8300_2	0x2a01		/* Libertas 88W8300 */
#define	PCI_PRODUCT_MARVELL_YUKON	0x4320		/* Yukon 88E8001/8003/8010 */
#define	PCI_PRODUCT_MARVELL_YUKON_8021CU	0x4340		/* Yukon 88E8021CU */
#define	PCI_PRODUCT_MARVELL_YUKON_8022CU	0x4341		/* Yukon 88E8022CU */
#define	PCI_PRODUCT_MARVELL_YUKON_8061CU	0x4342		/* Yukon 88E8061CU */
#define	PCI_PRODUCT_MARVELL_YUKON_8062CU	0x4343		/* Yukon 88E8062CU */
#define	PCI_PRODUCT_MARVELL_YUKON_8021X	0x4344		/* Yukon 88E8021X */
#define	PCI_PRODUCT_MARVELL_YUKON_8022X	0x4345		/* Yukon 88E8022X */
#define	PCI_PRODUCT_MARVELL_YUKON_8061X	0x4346		/* Yukon 88E8061X */
#define	PCI_PRODUCT_MARVELL_YUKON_8062X	0x4347		/* Yukon 88E8062X */
#define	PCI_PRODUCT_MARVELL_YUKON_8035	0x4350		/* Yukon 88E8035 */
#define	PCI_PRODUCT_MARVELL_YUKON_8036	0x4351		/* Yukon 88E8036 */
#define	PCI_PRODUCT_MARVELL_YUKON_8038	0x4352		/* Yukon 88E8038 */
#define	PCI_PRODUCT_MARVELL_YUKON_8039	0x4353		/* Yukon 88E8039 */
#define	PCI_PRODUCT_MARVELL_YUKON_8040	0x4354		/* Yukon 88E8040 */
#define	PCI_PRODUCT_MARVELL_YUKON_8040T	0x4355		/* Yukon 88E8040T */
#define	PCI_PRODUCT_MARVELL_YUKON_C033	0x4356		/* Yukon 88EC033 */
#define	PCI_PRODUCT_MARVELL_YUKON_8042	0x4357		/* Yukon 88E8042 */
#define	PCI_PRODUCT_MARVELL_YUKON_8048	0x435a		/* Yukon 88E8048 */
#define	PCI_PRODUCT_MARVELL_YUKON_8052	0x4360		/* Yukon 88E8052 */
#define	PCI_PRODUCT_MARVELL_YUKON_8050	0x4361		/* Yukon 88E8050 */
#define	PCI_PRODUCT_MARVELL_YUKON_8053	0x4362		/* Yukon 88E8053 */
#define	PCI_PRODUCT_MARVELL_YUKON_8055	0x4363		/* Yukon 88E8055 */
#define	PCI_PRODUCT_MARVELL_YUKON_8056	0x4364		/* Yukon 88E8056 */
#define	PCI_PRODUCT_MARVELL_YUKON_8070	0x4365		/* Yukon 88E8070 */
#define	PCI_PRODUCT_MARVELL_YUKON_C036	0x4366		/* Yukon 88EC036 */
#define	PCI_PRODUCT_MARVELL_YUKON_C032	0x4367		/* Yukon 88EC032 */
#define	PCI_PRODUCT_MARVELL_YUKON_C034	0x4368		/* Yukon 88EC034 */
#define	PCI_PRODUCT_MARVELL_YUKON_C042	0x4369		/* Yukon 88EC042 */
#define	PCI_PRODUCT_MARVELL_YUKON_8058	0x436a		/* Yukon 88E8058 */
#define	PCI_PRODUCT_MARVELL_YUKON_8071	0x436b		/* Yukon 88E8071 */
#define	PCI_PRODUCT_MARVELL_YUKON_8072	0x436c		/* Yukon 88E8072 */
#define	PCI_PRODUCT_MARVELL_YUKON_8055_2	0x436d		/* Yukon 88E8055 */
#define	PCI_PRODUCT_MARVELL_YUKON_8075	0x4370		/* Yukon 88E8075 */
#define	PCI_PRODUCT_MARVELL_YUKON_8057	0x4380		/* Yukon 88E8057 */
#define	PCI_PRODUCT_MARVELL_YUKON_8059	0x4381		/* Yukon 88E8059 */
#define	PCI_PRODUCT_MARVELL_YUKON_BELKIN	0x5005		/* Yukon (Belkin F5D5005) */
#define	PCI_PRODUCT_MARVELL_88SX5040	0x5040		/* 88SX5040 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5041	0x5041		/* 88SX5041 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5080	0x5080		/* 88SX5080 SATA */
#define	PCI_PRODUCT_MARVELL_88SX5081	0x5081		/* 88SX5081 SATA */
#define	PCI_PRODUCT_MARVELL_88SX6040	0x6040		/* 88SX6040 SATA */
#define	PCI_PRODUCT_MARVELL_88SX6041	0x6041		/* 88SX6041 SATA */
#define	PCI_PRODUCT_MARVELL_88SX6042	0x6042		/* 88SX6042 SATA */
#define	PCI_PRODUCT_MARVELL_88SX6080	0x6080		/* 88SX6080 SATA */
#define	PCI_PRODUCT_MARVELL_88SX6081	0x6081		/* 88SX6081 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6101	0x6101		/* 88SE6101 IDE */
#define	PCI_PRODUCT_MARVELL_88SE6111	0x6111		/* 88SE6111 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6120	0x6120		/* 88SE6120 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6121	0x6121		/* 88SE6121 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6122	0x6122		/* 88SE6122 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6140	0x6140		/* 88SE6140 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6141	0x6141		/* 88SE6141 SATA */
#define	PCI_PRODUCT_MARVELL_88SE6145	0x6145		/* 88SE6145 SATA */
#define	PCI_PRODUCT_MARVELL_88SX7042	0x7042		/* 88SX7042 SATA */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA PX2085 (Atlas) */
#define	PCI_PRODUCT_MATROX_MILLENIUM	0x0519		/* MGA Millenium 2064W (Storm) */
#define	PCI_PRODUCT_MATROX_MYSTIQUE_220	0x051a		/* MGA 1064SG 220MHz */
#define	PCI_PRODUCT_MATROX_MILLENNIUM_II	0x051b		/* MGA Millennium II 2164W */
#define	PCI_PRODUCT_MATROX_MILLENNIUM_IIAGP	0x051f		/* MGA Millennium II 2164WA-B AGP */
#define	PCI_PRODUCT_MATROX_MILL_II_G200_PCI	0x0520		/* MGA G200 PCI */
#define	PCI_PRODUCT_MATROX_MILL_II_G200_AGP	0x0521		/* MGA G200 AGP */
#define	PCI_PRODUCT_MATROX_G200E_SE	0x0522		/* MGA G200e (ServerEngines) */
#define	PCI_PRODUCT_MATROX_MILL_II_G400_AGP	0x0525		/* MGA G400/G450 AGP */
#define	PCI_PRODUCT_MATROX_G200EW	0x0532		/* MGA G200eW */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */
#define	PCI_PRODUCT_MATROX_PRODUCTIVA_PCI	0x1000		/* MGA G100 PCI */
#define	PCI_PRODUCT_MATROX_PRODUCTIVA_AGP	0x1001		/* MGA G100 AGP */
#define	PCI_PRODUCT_MATROX_MYSTIQUE	0x102b		/* MGA 1064SG */
#define	PCI_PRODUCT_MATROX_G400_TH	0x2179		/* MGA G400 Twin Head */
#define	PCI_PRODUCT_MATROX_MILL_II_G550_AGP	0x2527		/* MGA G550 AGP */
#define	PCI_PRODUCT_MATROX_MILL_P650_PCIE	0x2538		/* MGA P650 PCIe */
#define	PCI_PRODUCT_MATROX_MILL_G200_SD	0xff00		/* MGA Millennium G200 SD */
#define	PCI_PRODUCT_MATROX_PROD_G100_SD	0xff01		/* MGA Produktiva G100 SD */
#define	PCI_PRODUCT_MATROX_MYST_G200_SD	0xff02		/* MGA Mystique G200 SD */
#define	PCI_PRODUCT_MATROX_MILL_G200_SG	0xff03		/* MGA Millennium G200 SG */
#define	PCI_PRODUCT_MATROX_MARV_G200_SD	0xff04		/* MGA Marvel G200 SD */

/* Meinberg Funkuhren */
#define	PCI_PRODUCT_MEINBERG_PCI32	0x0101		/* PCI32 */
#define	PCI_PRODUCT_MEINBERG_PCI509	0x0102		/* PCI509 */
#define	PCI_PRODUCT_MEINBERG_PCI511	0x0104		/* PCI511 */
#define	PCI_PRODUCT_MEINBERG_PEX511	0x0105		/* PEX511 */
#define	PCI_PRODUCT_MEINBERG_GPS170PCI	0x0204		/* GPS170PCI */

/* Mellanox */
#define	PCI_PRODUCT_MELLANOX_CONNECTX_EN	0x6368		/* ConnectX EN */

/* Mentor */
#define	PCI_PRODUCT_MENTOR_PCI0660	0x0660		/* PCI */
#define	PCI_PRODUCT_MENTOR_PCI0661	0x0661		/* PCI-PCI */

/* Micrel products */
#define	PCI_PRODUCT_MICREL_KSZ8841	0x8841		/* KSZ8841 10/100 */
#define	PCI_PRODUCT_MICREL_KSZ8842	0x8842		/* KSZ8842 dual-port 10/100 switch */

/* Micro Star International products */
#define	PCI_PRODUCT_MSI_RT3090	0x891a		/* RT3090 */

/* Microsoft products */
#define	PCI_PRODUCT_MICROSOFT_MN120	0x0001		/* MN-120 10/100 */
#define	PCI_PRODUCT_MICROSOFT_MN130	0x0002		/* MN-130 10/100 */

/* Miro Computer Products AG */
#define	PCI_PRODUCT_MIRO_2IVDC	0x5607		/* 2IVDC-PCX1 */
#define	PCI_PRODUCT_MIRO_DC20	0x5601		/* MiroVIDEO DC20 */
#define	PCI_PRODUCT_MIRO_MEDIA3D	0x5631		/* Media 3D */
#define	PCI_PRODUCT_MIRO_DC10	0x6057		/* MiroVIDEO DC10/DC20 */

/* Mitsubishi Electronics */
#define	PCI_PRODUCT_MITSUBISHIELEC_4D30T	0x0301		/* Powerstorm 4D30T */
#define	PCI_PRODUCT_MITSUBISHIELEC_GUI	0x0304		/* GUI Accel */

/* Motorola products */
#define	PCI_PRODUCT_MOT_MPC105	0x0001		/* MPC105 PCI */
#define	PCI_PRODUCT_MOT_MPC106	0x0002		/* MPC106 PCI */
#define	PCI_PRODUCT_MOT_SM56	0x5600		/* SM56 */
#define	PCI_PRODUCT_MOT_RAVEN	0x4801		/* Raven PCI */

/* Moxa */
#define	PCI_PRODUCT_MOXA_CP114	0x1141		/* CP-114 */
#define	PCI_PRODUCT_MOXA_C104H	0x1040		/* C104H */
#define	PCI_PRODUCT_MOXA_CP104UL	0x1041		/* CP-104UL */
#define	PCI_PRODUCT_MOXA_CP104JU	0x1042		/* CP-104JU */
#define	PCI_PRODUCT_MOXA_C168H	0x1680		/* C168H */

/* Mesa Ridge Technologies (MAGMA) */
#define	PCI_PRODUCT_MRTMAGMA_DMA4	0x0011		/* DMA4 serial */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_960P_V2	0x0001		/* DAC960P V2 RAID */
#define	PCI_PRODUCT_MYLEX_960P_V3	0x0002		/* DAC960P V3 RAID */
#define	PCI_PRODUCT_MYLEX_960P_V4	0x0010		/* DAC960P V4 RAID */
#define	PCI_PRODUCT_MYLEX_960P_V5	0x0020		/* DAC960P V5 RAID */
#define	PCI_PRODUCT_MYLEX_ACCELERAID	0x0050		/* AcceleRAID */
#define	PCI_PRODUCT_MYLEX_EXTREMERAID	0xba56		/* eXtremeRAID */

/* Myricom */
#define	PCI_PRODUCT_MYRICOM_Z8E	0x0008		/* Z8E */
#define	PCI_PRODUCT_MYRICOM_LANAI_92	0x8043		/* Myrinet LANai 9.2 */

/* Myson Century products */
#define	PCI_PRODUCT_MYSON_MTD800	0x0800		/* MTD800 10/100 */
#define	PCI_PRODUCT_MYSON_MTD803	0x0803		/* MTD803 10/100 */
#define	PCI_PRODUCT_MYSON_MTD891	0x0891		/* MTD891 10/100/1000 */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

/* National Datacomm Corp products */
#define	PCI_PRODUCT_NDC_NCP130	0x0130		/* NCP130 */
#define	PCI_PRODUCT_NDC_NCP130A2	0x0131		/* NCP130 Rev A2 */

/* National Instruments */
#define	PCI_PRODUCT_NATINST_PCIGPIB	0xc801		/* PCI-GPIB */

/* NetChip Technology products */
#define	PCI_PRODUCT_NETCHIP_NET2282	0x2282		/* NET2282 USB */

/* NetXen Inc products */
#define	PCI_PRODUCT_NETXEN_NXB_10GXxR	0x0001		/* NXB-10GXxR (NX2031) */
#define	PCI_PRODUCT_NETXEN_NXB_10GCX4	0x0002		/* NXB-10GCX4 (NX2031) */
#define	PCI_PRODUCT_NETXEN_NXB_4GCU	0x0003		/* NXB-4GCU (NX2035) */
#define	PCI_PRODUCT_NETXEN_NXB_IMEZ	0x0004		/* IMEZ 10GbE */
#define	PCI_PRODUCT_NETXEN_NXB_HMEZ	0x0005		/* HMEZ 10GbE */
#define	PCI_PRODUCT_NETXEN_NXB_IMEZ_2	0x0024		/* IMEZ 10GbE Mgmt */
#define	PCI_PRODUCT_NETXEN_NXB_HMEZ_2	0x0025		/* HMEZ 10GbE Mgmt */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_DP83810	0x0001		/* DP83810 10/100 */
#define	PCI_PRODUCT_NS_PC87415	0x0002		/* PC87415 IDE */
#define	PCI_PRODUCT_NS_PC87560	0x000e		/* 87560 Legacy I/O */
#define	PCI_PRODUCT_NS_USB	0x0012		/* USB */
#define	PCI_PRODUCT_NS_DP83815	0x0020		/* DP83815 10/100 */
#define	PCI_PRODUCT_NS_DP83820	0x0022		/* DP83820 10/100/1000 */
#define	PCI_PRODUCT_NS_CS5535_HB	0x0028		/* CS5535 Host */
#define	PCI_PRODUCT_NS_CS5535_ISA	0x002b		/* CS5535 ISA */
#define	PCI_PRODUCT_NS_CS5535_IDE	0x002d		/* CS5535 IDE */
#define	PCI_PRODUCT_NS_CS5535_AUDIO	0x002e		/* CS5535 AUDIO */
#define	PCI_PRODUCT_NS_CS5535_USB	0x002f		/* CS5535 USB */
#define	PCI_PRODUCT_NS_CS5535_VIDEO	0x0030		/* CS5535 VIDEO */
#define	PCI_PRODUCT_NS_SATURN	0x0035		/* Saturn */
#define	PCI_PRODUCT_NS_SCx200_ISA	0x0500		/* SCx200 ISA */
#define	PCI_PRODUCT_NS_SCx200_SMI	0x0501		/* SCx200 SMI */
#define	PCI_PRODUCT_NS_SCx200_IDE	0x0502		/* SCx200 IDE */
#define	PCI_PRODUCT_NS_SCx200_AUDIO	0x0503		/* SCx200 AUDIO */
#define	PCI_PRODUCT_NS_SCx200_VIDEO	0x0504		/* SCx200 VIDEO */
#define	PCI_PRODUCT_NS_SCx200_XBUS	0x0505		/* SCx200 X-BUS */
#define	PCI_PRODUCT_NS_SC1100_ISA	0x0510		/* SC1100 ISA */
#define	PCI_PRODUCT_NS_SC1100_SMI	0x0511		/* SC1100 SMI */
#define	PCI_PRODUCT_NS_SC1100_XBUS	0x0515		/* SC1100 X-Bus */
#define	PCI_PRODUCT_NS_NS87410	0xd001		/* NS87410 */

/* NEC */
#define	PCI_PRODUCT_NEC_USB	0x0035		/* USB */
#define	PCI_PRODUCT_NEC_POWERVR2	0x0046		/* PowerVR PCX2 */
#define	PCI_PRODUCT_NEC_MARTH	0x0074		/* I/O */
#define	PCI_PRODUCT_NEC_PKUG	0x007d		/* I/O */
#define	PCI_PRODUCT_NEC_uPD72874	0x00f2		/* Firewire */
#define	PCI_PRODUCT_NEC_USB2	0x00e0		/* USB */
#define	PCI_PRODUCT_NEC_uPD720400	0x0125		/* PCIE-PCIX */
#define	PCI_PRODUCT_NEC_uPD720200	0x0194		/* PCIE-XHCI */
#define	PCI_PRODUCT_NEC_VERSAMAESTRO	0x8058		/* Versa Maestro */
#define	PCI_PRODUCT_NEC_VERSAPRONXVA26D	0x803c		/* Versa Va26D Maestro */

/* NeoMagic */
#define	PCI_PRODUCT_NEOMAGIC_NM2070	0x0001		/* Magicgraph NM2070 */
#define	PCI_PRODUCT_NEOMAGIC_128V	0x0002		/* Magicgraph 128V */
#define	PCI_PRODUCT_NEOMAGIC_128ZV	0x0003		/* Magicgraph 128ZV */
#define	PCI_PRODUCT_NEOMAGIC_NM2160	0x0004		/* Magicgraph NM2160 */
#define	PCI_PRODUCT_NEOMAGIC_NM2200	0x0005		/* Magicgraph NM2200 */
#define	PCI_PRODUCT_NEOMAGIC_NM2360	0x0006		/* Magicgraph NM2360 */
#define	PCI_PRODUCT_NEOMAGIC_NM2230	0x0025		/* MagicMedia 256AV+ */
#define	PCI_PRODUCT_NEOMAGIC_NM256XLP	0x0016		/* MagicMedia 256XL+ */
#define	PCI_PRODUCT_NEOMAGIC_NM256AV	0x8005		/* MagicMedia 256AV */
#define	PCI_PRODUCT_NEOMAGIC_NM256ZX	0x8006		/* MagicMedia 256ZX */

/* Neterion products */
#define	PCI_PRODUCT_NETERION_XFRAME	0x5831		/* Xframe */
#define	PCI_PRODUCT_NETERION_XFRAME_2	0x5832		/* Xframe II */

/* Netgear products */
#define	PCI_PRODUCT_NETGEAR_MA301	0x4100		/* MA301 */
#define	PCI_PRODUCT_NETGEAR_GA620	0x620a		/* GA620 */
#define	PCI_PRODUCT_NETGEAR_GA620T	0x630a		/* GA620T */

/* NetMos */
#define	PCI_PRODUCT_NETMOS_NM9805	0x9805		/* Nm9805 */
#define	PCI_PRODUCT_NETMOS_NM9835	0x9835		/* Nm9835 */
#define	PCI_PRODUCT_NETMOS_NM9845	0x9845		/* Nm9845 */
#define	PCI_PRODUCT_NETMOS_NM9901	0x9901		/* Nm9901 */

/* Network Security Technologies */
#define	PCI_PRODUCT_NETSEC_7751	0x7751		/* 7751 */

/* C-Media Electronics */
#define	PCI_PRODUCT_CMI_CMI8338A	0x0100		/* CMI8338A Audio */
#define	PCI_PRODUCT_CMI_CMI8338B	0x0101		/* CMI8338B Audio */
#define	PCI_PRODUCT_CMI_CMI8738	0x0111		/* CMI8738/C3DX Audio */
#define	PCI_PRODUCT_CMI_CMI8738B	0x0112		/* CMI8738B Audio */
#define	PCI_PRODUCT_CMI_HSP56	0x0211		/* HSP56 AMR */

/* Netoctave */
#define	PCI_PRODUCT_NETOCTAVE_NSP2K	0x0100		/* NSP2K */

/* NetVin */
#define	PCI_PRODUCT_NETVIN_NV5000	0x5000		/* NetVin 5000 */

/* Newbridge / Tundra products */
#define	PCI_PRODUCT_NEWBRIDGE_CA91CX42	0x0000		/* Universe VME */
#define	PCI_PRODUCT_NEWBRIDGE_TSI381	0x8111		/* Tsi381 PCIE-PCI */

/* SIIG products */
#define	PCI_PRODUCT_SIIG_1000	0x1000		/* I/O */
#define	PCI_PRODUCT_SIIG_1001	0x1001		/* I/O */
#define	PCI_PRODUCT_SIIG_1002	0x1002		/* I/O */
#define	PCI_PRODUCT_SIIG_1010	0x1010		/* I/O */
#define	PCI_PRODUCT_SIIG_1011	0x1011		/* I/O */
#define	PCI_PRODUCT_SIIG_1012	0x1012		/* I/O */
#define	PCI_PRODUCT_SIIG_1020	0x1020		/* I/O */
#define	PCI_PRODUCT_SIIG_1021	0x1021		/* I/O */
#define	PCI_PRODUCT_SIIG_1030	0x1030		/* I/O */
#define	PCI_PRODUCT_SIIG_1031	0x1031		/* I/O */
#define	PCI_PRODUCT_SIIG_1032	0x1032		/* I/O */
#define	PCI_PRODUCT_SIIG_1034	0x1034		/* I/O */
#define	PCI_PRODUCT_SIIG_1035	0x1035		/* I/O */
#define	PCI_PRODUCT_SIIG_1036	0x1036		/* I/O */
#define	PCI_PRODUCT_SIIG_1050	0x1050		/* I/O */
#define	PCI_PRODUCT_SIIG_1051	0x1051		/* I/O */
#define	PCI_PRODUCT_SIIG_1052	0x1052		/* I/O */
#define	PCI_PRODUCT_SIIG_2000	0x2000		/* I/O */
#define	PCI_PRODUCT_SIIG_2001	0x2001		/* I/O */
#define	PCI_PRODUCT_SIIG_2002	0x2002		/* I/O */
#define	PCI_PRODUCT_SIIG_2010	0x2010		/* I/O */
#define	PCI_PRODUCT_SIIG_2011	0x2011		/* I/O */
#define	PCI_PRODUCT_SIIG_2012	0x2012		/* I/O */
#define	PCI_PRODUCT_SIIG_2020	0x2020		/* I/O */
#define	PCI_PRODUCT_SIIG_2021	0x2021		/* I/O */
#define	PCI_PRODUCT_SIIG_2030	0x2030		/* I/O */
#define	PCI_PRODUCT_SIIG_2031	0x2031		/* I/O */
#define	PCI_PRODUCT_SIIG_2032	0x2032		/* I/O */
#define	PCI_PRODUCT_SIIG_2040	0x2040		/* I/O */
#define	PCI_PRODUCT_SIIG_2041	0x2041		/* I/O */
#define	PCI_PRODUCT_SIIG_2042	0x2042		/* I/O */
#define	PCI_PRODUCT_SIIG_2050	0x2050		/* I/O */
#define	PCI_PRODUCT_SIIG_2051	0x2051		/* I/O */
#define	PCI_PRODUCT_SIIG_2052	0x2052		/* I/O */
#define	PCI_PRODUCT_SIIG_2060	0x2060		/* I/O */
#define	PCI_PRODUCT_SIIG_2061	0x2061		/* I/O */
#define	PCI_PRODUCT_SIIG_2062	0x2062		/* I/O */
#define	PCI_PRODUCT_SIIG_2081	0x2081		/* I/O */
#define	PCI_PRODUCT_SIIG_2082	0x2082		/* I/O */

/* Solarflare products */
#define	PCI_PRODUCT_SOLARFLARE_FALCON_P	0x0703		/* Falcon P */
#define	PCI_PRODUCT_SOLARFLARE_FALCON_S	0x6703		/* Falcon S */
#define	PCI_PRODUCT_SOLARFLARE_EF1002	0xc101		/* EF1002 */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_SYMBIOS_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_SYMBIOS_820	0x0002		/* 53c820 */
#define	PCI_PRODUCT_SYMBIOS_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_SYMBIOS_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_SYMBIOS_810AP	0x0005		/* 53c810AP */
#define	PCI_PRODUCT_SYMBIOS_860	0x0006		/* 53c860 */
#define	PCI_PRODUCT_SYMBIOS_1510D	0x000a		/* 53c1510D */
#define	PCI_PRODUCT_SYMBIOS_896	0x000b		/* 53c896 */
#define	PCI_PRODUCT_SYMBIOS_895	0x000c		/* 53c895 */
#define	PCI_PRODUCT_SYMBIOS_885	0x000d		/* 53c885 */
#define	PCI_PRODUCT_SYMBIOS_875	0x000f		/* 53c875 */
#define	PCI_PRODUCT_SYMBIOS_1510	0x0010		/* 53c1510 */
#define	PCI_PRODUCT_SYMBIOS_895A	0x0012		/* 53c895A */
#define	PCI_PRODUCT_SYMBIOS_1010	0x0020		/* 53c1010-33 */
#define	PCI_PRODUCT_SYMBIOS_1010_2	0x0021		/* 53c1010-66 */
#define	PCI_PRODUCT_SYMBIOS_1030	0x0030		/* 53c1030 */
#define	PCI_PRODUCT_SYMBIOS_1030ZC	0x0031		/* 53c1030ZC */
#define	PCI_PRODUCT_SYMBIOS_1030_1035	0x0032		/* 53c1035 */
#define	PCI_PRODUCT_SYMBIOS_1030ZC_1035	0x0033		/* 53c1035 */
#define	PCI_PRODUCT_SYMBIOS_1035	0x0040		/* 53c1035 */
#define	PCI_PRODUCT_SYMBIOS_1035ZC	0x0041		/* 53c1035ZC */
#define	PCI_PRODUCT_SYMBIOS_SAS1064	0x0050		/* SAS1064 */
#define	PCI_PRODUCT_SYMBIOS_SAS1068	0x0054		/* SAS1068 */
#define	PCI_PRODUCT_SYMBIOS_SAS1068_2	0x0055		/* SAS1068 */
#define	PCI_PRODUCT_SYMBIOS_SAS1064E	0x0056		/* SAS1064E */
#define	PCI_PRODUCT_SYMBIOS_SAS1064E_2	0x0057		/* SAS1064E */
#define	PCI_PRODUCT_SYMBIOS_SAS1068E	0x0058		/* SAS1068E */
#define	PCI_PRODUCT_SYMBIOS_SAS1068E_2	0x0059		/* SAS1068E */
#define	PCI_PRODUCT_SYMBIOS_SAS1066E	0x005a		/* SAS1066E */
#define	PCI_PRODUCT_SYMBIOS_SAS1064A	0x005c		/* SAS1064A */
#define	PCI_PRODUCT_SYMBIOS_SAS1066	0x005e		/* SAS1066 */
#define	PCI_PRODUCT_SYMBIOS_SAS1078	0x0060		/* SAS1078 */
#define	PCI_PRODUCT_SYMBIOS_SAS1078_PCIE	0x0062		/* SAS1078 */
#define	PCI_PRODUCT_SYMBIOS_SAS1078DE	0x007c		/* SAS1078DE */
#define	PCI_PRODUCT_SYMBIOS_875J	0x008f		/* 53c875J */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320	0x0407		/* MegaRAID 320 */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_3202E	0x0408		/* MegaRAID 320-2E */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_SATA	0x0409		/* MegaRAID SATA 4x/8x */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS	0x0411		/* MegaRAID SAS 1064R */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR	0x0413		/* MegaRAID Verde ZCR */
#define	PCI_PRODUCT_SYMBIOS_FC909	0x0620		/* FC909 */
#define	PCI_PRODUCT_SYMBIOS_FC909A	0x0621		/* FC909A */
#define	PCI_PRODUCT_SYMBIOS_FC929	0x0622		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC929_1	0x0623		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC919	0x0624		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC919_1	0x0625		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC929X	0x0626		/* FC929X */
#define	PCI_PRODUCT_SYMBIOS_FC919X	0x0628		/* FC919X */
#define	PCI_PRODUCT_SYMBIOS_FC949X	0x0640		/* FC949X */
#define	PCI_PRODUCT_SYMBIOS_FC939X	0x0642		/* FC939X */
#define	PCI_PRODUCT_SYMBIOS_FC949E	0x0646		/* FC949E */
#define	PCI_PRODUCT_SYMBIOS_YELLOWFIN_1	0x0701		/* Yellowfin */
#define	PCI_PRODUCT_SYMBIOS_YELLOWFIN_2	0x0702		/* Yellowfin */
#define	PCI_PRODUCT_SYMBIOS_61C102	0x0901		/* 61C102 */
#define	PCI_PRODUCT_SYMBIOS_63C815	0x1000		/* 63C815 */
#define	PCI_PRODUCT_SYMBIOS_1030R	0x1030		/* 53c1030R */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID	0x1960		/* MegaRAID */
#define	PCI_PRODUCT_SYMBIOS_SAS2008	0x0072		/* SAS2008 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_1	0x0078		/* MegaRAID SAS2108 CRYPTO GEN2 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_2	0x0079		/* MegaRAID SAS2108 GEN2 */

/* Packet Engines products */
#define	PCI_PRODUCT_SYMBIOS_PE_GNIC	0x0702		/* Packet Engines G-NIC */

/* Pericom products */
#define	PCI_PRODUCT_PERICOM_PI7C21P100	0x01a7		/* PI7C21P100 PCIX-PCIX */
#define	PCI_PRODUCT_PERICOM_PPB_1	0x8140		/* PCI-PCI */
#define	PCI_PRODUCT_PERICOM_PPB_2	0x8150		/* PCI-PCI */

/* Planex products */
#define	PCI_PRODUCT_PLANEX_FNW_3603_TX	0xab06		/* FNW-3603-TX 10/100 */
#define	PCI_PRODUCT_PLANEX_FNW_3800_TX	0xab07		/* FNW-3800-TX 10/100 */

/* NexGen products */
#define	PCI_PRODUCT_NEXGEN_NX82C501	0x4e78		/* NX82C501 PCI */

/* NKK products */
#define	PCI_PRODUCT_NKK_NDR4600	0xa001		/* NDR4600 PCI */

/* Nortel Networks products */
#define	PCI_PRODUCT_NORTEL_BS21	0x1211		/* BS21 10/100 */
#define	PCI_PRODUCT_NORTEL_211818A	0x8030		/* E-mobility Wireless */

/* Number Nine products */
#define	PCI_PRODUCT_NUMBER9_I128	0x2309		/* Imagine-128 */
#define	PCI_PRODUCT_NUMBER9_I128_2	0x2339		/* Imagine-128 II */
#define	PCI_PRODUCT_NUMBER9_I128_T2R	0x493d		/* Imagine-128 T2R */
#define	PCI_PRODUCT_NUMBER9_I128_T2R4	0x5348		/* Imagine-128 T2R4 */

/* NVIDIA products */
#define	PCI_PRODUCT_NVIDIA_NV1	0x0008		/* NV1 */
#define	PCI_PRODUCT_NVIDIA_DAC64	0x0009		/* DAC64 */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT	0x0020		/* Riva TNT */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT2	0x0028		/* Riva TNT2 */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT2_ULTRA	0x0029		/* Riva TNT2 Ultra */
#define	PCI_PRODUCT_NVIDIA_VANTA1	0x002c		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_VANTA2	0x002d		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_MCP04_ISA	0x0030		/* MCP04 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP04_SMB	0x0034		/* MCP04 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP04_IDE	0x0035		/* MCP04 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA	0x0036		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN1	0x0037		/* MCP04 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN2	0x0038		/* MCP04 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP04_AC97	0x003a		/* MCP04 AC97 */
#define	PCI_PRODUCT_NVIDIA_MCP04_OHCI	0x003b		/* MCP04 USB */
#define	PCI_PRODUCT_NVIDIA_MCP04_ECHI	0x003c		/* MCP04 USB */
#define	PCI_PRODUCT_NVIDIA_MCP04_PPB	0x003d		/* MCP04 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_MCP04_SATA2	0x003e		/* MCP04 SATA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ISA1	0x0050		/* nForce4 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ISA2	0x0051		/* nForce4 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SMB	0x0052		/* nForce4 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ATA133	0x0053		/* nForce4 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA1	0x0054		/* nForce4 SATA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA2	0x0055		/* nForce4 SATA */
#define	PCI_PRODUCT_NVIDIA_CK804_LAN1	0x0056		/* CK804 LAN */
#define	PCI_PRODUCT_NVIDIA_CK804_LAN2	0x0057		/* CK804 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_AC	0x0059		/* nForce4 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_OHCI	0x005a		/* nForce4 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_EHCI	0x005b		/* nForce4 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PPB	0x005c		/* nForce4 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PPB2	0x005d		/* nForce4 PCIE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_MEM	0x005e		/* nForce4 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ISA	0x0060		/* nForce2 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_SMB	0x0064		/* nForce2 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_IDE	0x0065		/* nForce2 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_LAN	0x0066		/* nForce2 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_OHCI	0x0067		/* nForce2 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_EHCI	0x0068		/* nForce2 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ACA	0x006a		/* nForce2 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_APU	0x006b		/* nForce2 Audio */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB	0x006c		/* nForce2 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB2	0x006d		/* nForce2 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_FW	0x006e		/* nForce2 FireWire */
#define	PCI_PRODUCT_NVIDIA_MCP04_PPB2	0x007e		/* MCP04 PCIE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ISA	0x0080		/* nForce2 400 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SMB	0x0084		/* nForce2 400 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_IDE	0x0085		/* nForce2 400 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN2	0x0086		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_OHCI	0x0087		/* nForce2 400 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_EHCI	0x0088		/* nForce2 400 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ACA	0x008a		/* nForce2 400 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PPB	0x008b		/* nForce2 400 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN3	0x008c		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SATA	0x008e		/* nForce2 400 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7800GTX	0x0091		/* GeForce 7800 GTX */
#define	PCI_PRODUCT_NVIDIA_ITNT2	0x00a0		/* Aladdin TNT2 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6800GO	0x00c8		/* GeForce Go 6800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6800GO_U	0x00c9		/* GeForce Go 6800 Ultra */
#define	PCI_PRODUCT_NVIDIA_QUADROFXGO1400	0x00cc		/* Quadro FX Go1400 */
#define	PCI_PRODUCT_NVIDIA_QUADROFX1400	0x00ce		/* Quadro FX 1400 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_ISA	0x00d0		/* nForce3 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCHB	0x00d1		/* nForce3 PCI Host */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB2	0x00d2		/* nForce3 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_CK804_MEM	0x00d3		/* CK804 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_SMB	0x00d4		/* nForce3 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_IDE	0x00d5		/* nForce3 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN1	0x00d6		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_OHCI	0x00d7		/* nForce3 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_EHCI	0x00d8		/* nForce3 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_MODEM	0x00d9		/* nForce3 Modem */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_ACA	0x00da		/* nForce3 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB	0x00dd		/* nForce3 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN4	0x00df		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_ISA	0x00e0		/* nForce3 250 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PCHB	0x00e1		/* nForce3 250 PCI Host */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_AGP	0x00e2		/* nForce3 250 AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SATA	0x00e3		/* nForce3 250 SATA */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SMB	0x00e4		/* nForce3 250 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_IDE	0x00e5		/* nForce3 250 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN5	0x00e6		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_OHCI	0x00e7		/* nForce3 250 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_EHCI	0x00e8		/* nForce3 250 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_ACA	0x00ea		/* nForce3 250 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PPB	0x00ed		/* nForce3 250 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_SATA2	0x00ee		/* nForce3 250 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GTAGP	0x00f1		/* GeForce 6600 GT AGP */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600_3	0x00f2		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7800GS	0x00f5		/* GeForce 7800 GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6800GT	0x00f9		/* GeForce 6800 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE5300PCX	0x00fc		/* GeForce 5300 PCX */
#define	PCI_PRODUCT_NVIDIA_QUADROFX330	0x00fd		/* Quadro FX 330 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256	0x0100		/* GeForce256 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256_DDR	0x0101		/* GeForce256 DDR */
#define	PCI_PRODUCT_NVIDIA_QUADRO	0x0103		/* Quadro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX	0x0110		/* GeForce2 MX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX_100	0x0111		/* GeForce2 MX 100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GO	0x0112		/* GeForce2 Go */
#define	PCI_PRODUCT_NVIDIA_QUADRO2_MXR	0x0113		/* Quadro2 MXR */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GT	0x0140		/* GeForce 6600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600	0x0141		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600_2	0x0142		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GO	0x0144		/* GeForce 6600 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GO_2	0x0146		/* GeForce 6600 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GTS	0x0150		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2TI	0x0151		/* GeForce2 Ti */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2ULTRA	0x0152		/* GeForce2 Ultra */
#define	PCI_PRODUCT_NVIDIA_QUADRO2PRO	0x0153		/* Quadro2 Pro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200	0x0161		/* GeForce 6200 */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS285	0x0165		/* Quadro NVS 285 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEGO6200	0x0167		/* GeForce Go 6200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX460	0x0170		/* GeForce4 MX 460 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX440	0x0171		/* GeForce4 MX 440 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX420	0x0172		/* GeForce4 MX 420 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4440GO	0x0174		/* GeForce4 440 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4420GO	0x0175		/* GeForce4 420 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4420GOM32	0x0176		/* GeForce4 420 Go 32M */
#define	PCI_PRODUCT_NVIDIA_QUADRO4500XGL	0x0178		/* Quadro4 500XGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4440GOM64	0x0179		/* GeForce4 440 Go 64M */
#define	PCI_PRODUCT_NVIDIA_QUADRO4200	0x017a		/* Quadro4 200/400NVS */
#define	PCI_PRODUCT_NVIDIA_QUADRO4550XGL	0x017b		/* Quadro4 550XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4500GOGL	0x017c		/* Quadro4 GoGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX440AGP8	0x0181		/* GeForce4 MX 440 AGP */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX440SEAGP8	0x0182		/* GeForce4 MX 440SE AGP */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX420AGP8	0x0183		/* GeForce 4 MX 420 AGP */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MX4000	0x0185		/* GeForce4 MX 4000 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_580XGL	0x0188		/* Quadro4 580 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4NVS	0x018a		/* Quadro4 NVS */
#define	PCI_PRODUCT_NVIDIA_QUADRO4_380XGL	0x018b		/* Quadro4 380 XGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8800GTX	0x0191		/* GeForce 8800 GTX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8800GTS	0x0193		/* GeForce 8800 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2_11	0x01a0		/* GeForce2 Crush11 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_PCHB	0x01a4		/* nForce PCI Host */
#define	PCI_PRODUCT_NVIDIA_NFORCE_DDR2	0x01aa		/* nForce 220 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE_DDR	0x01ab		/* nForce 420 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MEM	0x01ac		/* nForce 220/420 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_MEM1	0x01ad		/* nForce 220/420 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_APU	0x01b0		/* nForce APU */
#define	PCI_PRODUCT_NVIDIA_NFORCE_ACA	0x01b1		/* nForce AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE_ISA	0x01b2		/* nForce ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE_SMB	0x01b4		/* nForce SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE_AGP	0x01b7		/* nForce AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE_PPB	0x01b8		/* nForce PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE_IDE	0x01bc		/* nForce IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE_OHCI	0x01c2		/* nForce USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE_LAN	0x01c3		/* nForce LAN */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7300LE	0x01d1		/* GeForce 7300 LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7200GS	0x01d3		/* GeForce 7200 GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7300GO	0x01d7		/* GeForce 7300 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7400GO	0x01d8		/* GeForce 7400 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7300GS	0x01df		/* GeForce 7300 GS */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PCHB	0x01e0		/* nForce2 PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_AGP	0x01e8		/* nForce2 AGP */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM0	0x01ea		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM1	0x01eb		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM2	0x01ec		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM3	0x01ed		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM4	0x01ee		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_MEM5	0x01ef		/* nForce2 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4MXNFORCE	0x01f0		/* GeForce4 MX nForce GPU */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3	0x0200		/* GeForce3 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3TI200	0x0201		/* GeForce3 Ti 200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE3TI500	0x0202		/* GeForce3 Ti 500 */
#define	PCI_PRODUCT_NVIDIA_QUADRO_DCC	0x0203		/* Quadro DCC */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6200_2	0x0221		/* GeForce 6200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6150	0x0240		/* GeForce 6150 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6150LE	0x0241		/* GeForce 6150 LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6100	0x0242		/* GeForce 6100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEGO6150	0x0244		/* GeForce Go 6150 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEGO6100	0x0247		/* GeForce Go 6100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4600	0x0250		/* GeForce4 Ti 4600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4400	0x0251		/* GeForce4 Ti 4400 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4200	0x0253		/* GeForce4 Ti 4200 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4900XGL	0x0258		/* Quadro4 900 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4750XGL	0x0259		/* Quadro4 750 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4700XGL	0x025b		/* Quadro4 700 XGL */
#define	PCI_PRODUCT_NVIDIA_MCP51_ISA1	0x0260		/* MCP51 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP51_ISA2	0x0261		/* MCP51 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP51_ISA3	0x0262		/* MCP51 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP51_ISA4	0x0263		/* MCP51 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP51_SMB	0x0264		/* MCP51 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP51_IDE	0x0265		/* MCP51 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP51_SATA	0x0266		/* MCP51 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP51_SATA2	0x0267		/* MCP51 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP51_LAN1	0x0268		/* MCP51 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP51_LAN2	0x0269		/* MCP51 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP51_ACA	0x026b		/* MCP51 AC97 */
#define	PCI_PRODUCT_NVIDIA_MCP51_HDA	0x026c		/* MCP51 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP51_OHCI	0x026d		/* MCP51 USB */
#define	PCI_PRODUCT_NVIDIA_MCP51_EHCI	0x026e		/* MCP51 USB */
#define	PCI_PRODUCT_NVIDIA_MCP51_PPB	0x026f		/* MCP51 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_MCP51_HB	0x0270		/* MCP51 Host */
#define	PCI_PRODUCT_NVIDIA_MCP51_PMU	0x0271		/* MCP51 PMU */
#define	PCI_PRODUCT_NVIDIA_MCP51_MEM	0x0272		/* MCP51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_2	0x027e		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_3	0x027f		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4800	0x0280		/* GeForce4 Ti 4800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4200_2	0x0281		/* GeForce4 Ti 4200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4200GO	0x0286		/* GeForce4 Ti 4200 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7900GT	0x0291		/* GeForce 7900 GT/GTO */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7950GTX	0x0297		/* GeForce Go 7950 GTX */
#define	PCI_PRODUCT_NVIDIA_QUADROFX3500	0x029d		/* Quadro FX 3500 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7600GT_2	0x02e0		/* GeForce 7600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7600GS_2	0x02e1		/* GeForce 7600 GS */
#define	PCI_PRODUCT_NVIDIA_C51_HB_1	0x02f0		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_2	0x02f1		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_3	0x02f2		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_4	0x02f3		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_5	0x02f4		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_6	0x02f5		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_7	0x02f6		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_HB_8	0x02f7		/* C51 Host */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_5	0x02f8		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_4	0x02f9		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_0	0x02fa		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_PCIE_0	0x02fb		/* C51 PCIE */
#define	PCI_PRODUCT_NVIDIA_C51_PCIE_1	0x02fc		/* C51 PCIE */
#define	PCI_PRODUCT_NVIDIA_C51_PCIE_2	0x02fd		/* C51 PCIE */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_1	0x02fe		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_C51_MEM_6	0x02ff		/* C51 Memory */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5800_U	0x0301		/* GeForce FX 5800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5800	0x0302		/* GeForce FX 5800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5600_U	0x0311		/* GeForce FX 5600 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5600	0x0312		/* GeForce FX 5600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5600	0x031a		/* GeForce FX Go 5600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5650	0x031b		/* GeForce FX Go 5650 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5200_U	0x0321		/* GeForce FX 5200 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5200	0x0322		/* GeForce FX 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5200	0x0324		/* GeForce FX Go 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5500	0x0326		/* GeForce FX 5500 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5100	0x0327		/* GeForce FX 5100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5200_3	0x0328		/* GeForce FX Go 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5200_2	0x0329		/* GeForce FX Go 5200 */
#define	PCI_PRODUCT_NVIDIA_QUADROFX500	0x032b		/* Quadro FX 500/600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5300	0x032c		/* GeForce FX Go 5300 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5100	0x032d		/* GeForce FX Go 5100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5900_U	0x0330		/* GeForce FX 5900 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5900	0x0331		/* GeForce FX 5900 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5950_U	0x0333		/* GeForce FX 5950 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5700LE	0x0343		/* GeForce FX 5700LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5700_2	0x0347		/* GeForce FX Go 5700 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5700	0x0348		/* GeForce FX Go 5700 */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA1	0x0360		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA2	0x0361		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA3	0x0362		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA4	0x0363		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA5	0x0364		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA6	0x0365		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA7	0x0366		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_ISA8	0x0367		/* MCP55 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP55_SMB	0x0368		/* MCP55 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP55_MEM1	0x0369		/* MCP55 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP55_MEM2	0x036a		/* MCP55 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP55_OHCI	0x036c		/* MCP55 USB */
#define	PCI_PRODUCT_NVIDIA_MCP55_EHCI	0x036d		/* MCP55 USB */
#define	PCI_PRODUCT_NVIDIA_MCP55_IDE	0x036e		/* MCP55 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_6	0x0370		/* MCP55 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_MCP55_HDA	0x0371		/* MCP55 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN1	0x0372		/* MCP55 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP55_LAN2	0x0373		/* MCP55 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_1	0x0374		/* MCP55 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_2	0x0375		/* MCP55 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_3	0x0376		/* MCP55 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_4	0x0377		/* MCP55 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP55_PPB_5	0x0378		/* MCP55 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP55_MEM3	0x037a		/* MCP55 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA	0x037e		/* MCP55 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP55_SATA2	0x037f		/* MCP55 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7600GT	0x0391		/* GeForce 7600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7600GS	0x0392		/* GeForce 7600 GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7300GT	0x0393		/* GeForce 7300 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7900GO	0x0398		/* GeForce 7600 Go */
#define	PCI_PRODUCT_NVIDIA_C55_HB_1	0x03a0		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_2	0x03a1		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_3	0x03a2		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_4	0x03a3		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_5	0x03a4		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_6	0x03a5		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_7	0x03a6		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_HB_8	0x03a7		/* C55 Host */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_1	0x03a8		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_2	0x03a9		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_3	0x03aa		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_4	0x03ab		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_5	0x03ac		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_6	0x03ad		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_7	0x03ae		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_8	0x03af		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_9	0x03b0		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_10	0x03b1		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_11	0x03b2		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_12	0x03b3		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_13	0x03b4		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_14	0x03b5		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_15	0x03b6		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_PCIE_0	0x03b7		/* C55 PCIE */
#define	PCI_PRODUCT_NVIDIA_C55_PCIE_1	0x03b8		/* C55 PCIE */
#define	PCI_PRODUCT_NVIDIA_C55_PCIE_2	0x03b9		/* C55 PCIE */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_16	0x03ba		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_C55_PCIE_3	0x03bb		/* C55 PCIE */
#define	PCI_PRODUCT_NVIDIA_C55_MEM_17	0x03bc		/* C55 Memory */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6100_430	0x03d0		/* GeForce 6100 nForce 430 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6100_405	0x03d1		/* GeForce 6100 nForce 405 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7025_630a	0x03d6		/* GeForce 7025 nForce 630a */
#define	PCI_PRODUCT_NVIDIA_MCP61_ISA	0x03e0		/* MCP61 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP61_ISA_2	0x03e1		/* MCP61 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP61_HDA_1	0x03e4		/* MCP61 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN1	0x03e5		/* MCP61 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN2	0x03e6		/* MCP61 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA	0x03e7		/* MCP61 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP61_PPB_1	0x03e8		/* MCP61 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP61_PPB_2	0x03e9		/* MCP61 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP61_MEM1	0x03ea		/* MCP61 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP61_SMB	0x03eb		/* MCP61 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP61_IDE	0x03ec		/* MCP61 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN3	0x03ee		/* MCP61 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP61_LAN4	0x03ef		/* MCP61 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP61_HDA_2	0x03f0		/* MCP61 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP61_OHCI	0x03f1		/* MCP61 USB */
#define	PCI_PRODUCT_NVIDIA_MCP61_EHCI	0x03f2		/* MPC61 USB */
#define	PCI_PRODUCT_NVIDIA_MCP61_PPB_3	0x03f3		/* MCP61 */
#define	PCI_PRODUCT_NVIDIA_MCP61_SMU	0x03f4		/* MCP61 SMU */
#define	PCI_PRODUCT_NVIDIA_MCP61_MEM2	0x03f5		/* MCP61 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA2	0x03f6		/* MCP61 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP61_SATA3	0x03f7		/* MCP61 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8600_GT	0x0402		/* GeForce 8600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8600M_GT	0x0407		/* GeForce 8600M GT */
#define	PCI_PRODUCT_NVIDIA_QUADROFX570M	0x040c		/* Quadro FX 570M */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8500_GT	0x0421		/* GeForce 8500 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8400_GS	0x0422		/* GeForce 8400 GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8400M_GS	0x0427		/* GeForce 8400M GS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE8400M_G	0x0428		/* GeForce 8400M G */
#define	PCI_PRODUCT_NVIDIA_MCP65_ISA1	0x0440		/* MCP65 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP65_ISA2	0x0441		/* MCP65 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP65_MEM1	0x0444		/* MCP65 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP65_MEM2	0x0445		/* MCP65 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP65_SMB	0x0446		/* MCP65 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP65_IDE	0x0448		/* MCP65 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_1	0x0449		/* MCP65 PCI */
#define	PCI_PRODUCT_NVIDIA_MCP65_HDA_1	0x044a		/* MCP65 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP65_HDA_2	0x044b		/* MCP65 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_1	0x044c		/* MCP65 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_2	0x044d		/* MCP65 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_3	0x044e		/* MCP65 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP65_AHCI_4	0x044f		/* MCP65 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN1	0x0450		/* MCP65 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN2	0x0451		/* MCP65 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN3	0x0452		/* MCP65 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP65_LAN4	0x0453		/* MCP65 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_1	0x0454		/* MCP65 USB */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_2	0x0455		/* MCP65 USB */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_3	0x0456		/* MCP65 USB */
#define	PCI_PRODUCT_NVIDIA_MCP65_USB_4	0x0457		/* MCP65 USB */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_2	0x0458		/* MCP65 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_3	0x0459		/* MCP65 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_4	0x045a		/* MCP65 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP65_PPB_5	0x045b		/* MCP65 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA	0x045c		/* MCP65 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA2	0x045d		/* MCP65 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA3	0x045e		/* MCP65 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP65_SATA4	0x045f		/* MCP65 SATA */
#define	PCI_PRODUCT_NVIDIA_GEFORCEGTX285	0x05e3		/* GeForce GTX 285 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7000M	0x0533		/* GeForce 7000M */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7050_PV	0x053b		/* GeForce 7050 PV */
#define	PCI_PRODUCT_NVIDIA_MCP67_MEM1	0x0541		/* MCP67 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP67_SMB	0x0542		/* MCP67 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP67_COPROC	0x0543		/* MCP67 Co-processor */
#define	PCI_PRODUCT_NVIDIA_MCP67_MEM2	0x0547		/* MCP67 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP67_ISA	0x0548		/* MCP67 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN1	0x054c		/* MCP67 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN2	0x054d		/* MCP67 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN3	0x054e		/* MCP67 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP67_LAN4	0x054f		/* MCP67 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA	0x0550		/* MCP67 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA2	0x0551		/* MCP67 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA3	0x0552		/* MCP67 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP67_SATA4	0x0553		/* MCP67 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_1	0x0554		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_2	0x0555		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_3	0x0556		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_4	0x0557		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_5	0x0558		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_6	0x0559		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_7	0x055a		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_AHCI_8	0x055b		/* MCP67 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_HDA_1	0x055c		/* MCP67 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP67_HDA_2	0x055d		/* MCP67 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP67_OHCI	0x055e		/* MCP67 USB */
#define	PCI_PRODUCT_NVIDIA_MCP67_EHCI	0x055f		/* MCP67 USB */
#define	PCI_PRODUCT_NVIDIA_MCP67_IDE	0x0560		/* MCP67 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP67_PPB_1	0x0561		/* MCP67 PCI */
#define	PCI_PRODUCT_NVIDIA_MCP67_PPB_2	0x0562		/* MCP67 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP67_PPB_3	0x0563		/* MCP67 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP77_MEM1	0x0568		/* MCP77 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_1	0x0569		/* MCP77 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP73_EHCI	0x056a		/* MCP73 USB */
#define	PCI_PRODUCT_NVIDIA_MCP73_IDE	0x056c		/* MCP73 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP73_PPB_1	0x056d		/* MCP73 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP73_PPB_2	0x056e		/* MCP73 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP73_PPB_3	0x056f		/* MCP73 PCIE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9800_GTX	0x0605		/* GeForce 9800 GTX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9300_GE_1	0x06e0		/* GeForce 9300 GE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8800_GT	0x0611		/* GeForce 8800 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9600_GT	0x0622		/* GeForce 9600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE9300M_GS	0x06e9		/* GeForce 9300M GS */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS150	0x06ea		/* Quadro NVS 150m */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS160	0x06eb		/* Quadro NVS 160m */
#define	PCI_PRODUCT_NVIDIA_MCP77_MEM2	0x0751		/* MCP77 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP77_SMB	0x0752		/* MCP77 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP77_COPROC	0x0753		/* MCP77 Co-processor */
#define	PCI_PRODUCT_NVIDIA_MCP77_MEM3	0x0754		/* MCP77 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP77_IDE	0x0759		/* MCP77 IDE */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_2	0x075a		/* MCP77 PCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_3	0x075b		/* MCP77 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP77_ISA1	0x075c		/* MCP77 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP77_ISA2	0x075d		/* MCP77 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP77_ISA3	0x075e		/* MCP77 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN1	0x0760		/* MCP77 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN2	0x0761		/* MCP77 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN3	0x0762		/* MCP77 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP77_LAN4	0x0763		/* MCP77 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_1	0x0774		/* MCP77 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_2	0x0775		/* MCP77 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_3	0x0776		/* MCP77 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP77_HDA_4	0x0777		/* MCP77 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_4	0x0778		/* MCP77 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_5	0x0779		/* MCP77 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP77_PPB_6	0x077a		/* MCP77 PCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_OHCI_1	0x077b		/* MCP77 USB */
#define	PCI_PRODUCT_NVIDIA_MCP77_EHCI_1	0x077c		/* MCP77 USB */
#define	PCI_PRODUCT_NVIDIA_MCP77_OHCI_2	0x077d		/* MCP77 USB */
#define	PCI_PRODUCT_NVIDIA_MCP77_EHCI_2	0x077e		/* MCP77 USB */
#define	PCI_PRODUCT_NVIDIA_MCP73_HB_1	0x07c0		/* MCP73 Host */
#define	PCI_PRODUCT_NVIDIA_MCP73_HB_2	0x07c1		/* MCP73 Host */
#define	PCI_PRODUCT_NVIDIA_MCP73_HB_3	0x07c2		/* MCP73 Host */
#define	PCI_PRODUCT_NVIDIA_MCP73_HB_4	0x07c3		/* MCP73 Host */
#define	PCI_PRODUCT_NVIDIA_MCP73_HB_5	0x07c5		/* MCP73 Host */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM11	0x07c8		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM1	0x07cb		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM2	0x07cd		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM3	0x07ce		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM4	0x07cf		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM5	0x07d0		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM6	0x07d1		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM7	0x07d2		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM8	0x07d3		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM9	0x07d6		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_ISA	0x07d7		/* MCP73 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP73_SMB	0x07d8		/* MCP73 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP73_MEM10	0x07d9		/* MCP73 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN1	0x07dc		/* MCP73 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN2	0x07dd		/* MCP73 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN3	0x07de		/* MCP73 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP73_LAN4	0x07df		/* MCP73 LAN */
#define	PCI_PRODUCT_NVIDIA_GEFORCE7050	0x07e3		/* GeForce 7050 */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_1	0x07f0		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_2	0x07f1		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_3	0x07f2		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_4	0x07f3		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_5	0x07f4		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_6	0x07f5		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_7	0x07f6		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_8	0x07f7		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_9	0x07f8		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_10	0x07f9		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_11	0x07fa		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_AHCI_12	0x07fb		/* MCP73 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP73_HDA_1	0x07fc		/* MCP73 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP73_HDA_2	0x07fd		/* MCP73 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP73_OHCI	0x07fe		/* MCP73 USB */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8200_G	0x0845		/* GeForce 8200m G */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9100	0x0847		/* GeForce 9100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_8200	0x0849		/* GeForce 8200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9300_GE_2	0x084b		/* GeForce 9300 GE */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9400	0x0861		/* GeForce 9400 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9400_2	0x0863		/* GeForce 9400m */
#define	PCI_PRODUCT_NVIDIA_GEFORCE_9300	0x086c		/* GeForce 9300 */
#define	PCI_PRODUCT_NVIDIA_ION_VGA	0x087d		/* ION VGA */
#define	PCI_PRODUCT_NVIDIA_GEFORCE210	0x0a65		/* GeForce 210 */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_1	0x0a80		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_2	0x0a81		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_3	0x0a82		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_4	0x0a83		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_5	0x0a84		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_6	0x0a85		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_7	0x0a86		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_HB_8	0x0a87		/* MCP79 Host */
#define	PCI_PRODUCT_NVIDIA_MCP79_MEM1	0x0a88		/* MCP79 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP79_MEM2	0x0a89		/* MCP79 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP7A_PPB_1	0x0aa0		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_SMB	0x0aa2		/* MCP79 SMBus */
#define	PCI_PRODUCT_NVIDIA_MCP79_COPROC	0x0aa3		/* MCP79 Co-processor */
#define	PCI_PRODUCT_NVIDIA_MCP79_MEM3	0x0aa4		/* MCP79 Memory */
#define	PCI_PRODUCT_NVIDIA_MCP7A_OHCI_1	0x0aa5		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP7A_EHCI_1	0x0aa6		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP79_OHCI_2	0x0aa7		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP79_OHCI_3	0x0aa8		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP79_EHCI_2	0x0aa9		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP79_EHCI_3	0x0aaa		/* MCP79 USB */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_2	0x0aab		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_ISA1	0x0aac		/* MCP79 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP79_ISA2	0x0aad		/* MCP79 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP79_ISA3	0x0aae		/* MCP79 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP79_ISA4	0x0aaf		/* MCP79 ISA */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN1	0x0ab0		/* MCP79 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN2	0x0ab1		/* MCP79 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN3	0x0ab2		/* MCP79 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP79_LAN4	0x0ab3		/* MCP79 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP79_SATA_1	0x0ab4		/* MCP79 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP79_SATA_2	0x0ab5		/* MCP79 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP79_SATA_3	0x0ab6		/* MCP79 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP79_SATA_4	0x0ab7		/* MCP79 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_1	0x0ab8		/* MCP79 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_2	0x0ab9		/* MCP79 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_3	0x0aba		/* MCP79 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP79_AHCI_4	0x0abb		/* MCP79 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP79_RAID_1	0x0abc		/* MCP79 RAID */
#define	PCI_PRODUCT_NVIDIA_MCP79_RAID_2	0x0abd		/* MCP79 RAID */
#define	PCI_PRODUCT_NVIDIA_MCP79_RAID_3	0x0abe		/* MCP79 RAID */
#define	PCI_PRODUCT_NVIDIA_MCP79_RAID_4	0x0abf		/* MCP79 RAID */
#define	PCI_PRODUCT_NVIDIA_MCP79_HDA_1	0x0ac0		/* MCP79 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP79_HDA_2	0x0ac1		/* MCP79 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP79_HDA_3	0x0ac2		/* MCP79 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP79_HDA_4	0x0ac3		/* MCP79 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_3	0x0ac4		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_4	0x0ac5		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_5	0x0ac6		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_6	0x0ac7		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP79_PPB_7	0x0ac8		/* MCP79 PCIE */
#define	PCI_PRODUCT_NVIDIA_MCP77_SATA_1	0x0ad0		/* MCP77 SATA */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_2	0x0ad1		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_3	0x0ad2		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_4	0x0ad3		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_5	0x0ad4		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_6	0x0ad5		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_7	0x0ad6		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_8	0x0ad7		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_9	0x0ad8		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_10	0x0ad9		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_11	0x0ada		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP77_AHCI_12	0x0adb		/* MCP77 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_LAN	0x0d7d		/* MCP89 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_1	0x0d84		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_2	0x0d85		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_3	0x0d86		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_4	0x0d87		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_5	0x0d88		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_6	0x0d89		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_7	0x0d8a		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_8	0x0d8b		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_9	0x0d8c		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_10	0x0d8d		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_11	0x0d8e		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_AHCI_12	0x0d8f		/* MCP89 AHCI */
#define	PCI_PRODUCT_NVIDIA_MCP89_HDA_1	0x0d94		/* MCP89 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP89_HDA_2	0x0d95		/* MCP89 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP89_HDA_3	0x0d96		/* MCP89 HD Audio */
#define	PCI_PRODUCT_NVIDIA_MCP89_HDA_4	0x0d97		/* MCP89 HD Audio */

/* Oak Technologies products */
#define	PCI_PRODUCT_OAKTECH_OTI1007	0x0107		/* OTI107 */

/* Olicom */
#define	PCI_PRODUCT_OLICOM_OC2325	0x0012		/* OC2325 */
#define	PCI_PRODUCT_OLICOM_OC2183	0x0013		/* OC2183 */
#define	PCI_PRODUCT_OLICOM_OC2326	0x0014		/* OC2326 */

/* Omega Micro products */
#define	PCI_PRODUCT_OMEGA_82C092G	0x1221		/* 82C092G */

/* Opti products */
#define	PCI_PRODUCT_OPTI_82C557	0xc557		/* 82C557 Host */
#define	PCI_PRODUCT_OPTI_82C558	0xc558		/* 82C558 ISA */
#define	PCI_PRODUCT_OPTI_82C568	0xc568		/* 82C568 IDE */
#define	PCI_PRODUCT_OPTI_82D568	0xd568		/* 82D568 IDE */
#define	PCI_PRODUCT_OPTI_82C621	0xc621		/* 82C621 IDE */
#define	PCI_PRODUCT_OPTI_82C700	0xc700		/* 82C700 */
#define	PCI_PRODUCT_OPTI_82C701	0xc701		/* 82C701 */
#define	PCI_PRODUCT_OPTI_82C822	0xc822		/* 82C822 */
#define	PCI_PRODUCT_OPTI_82C861	0xc861		/* 82C861 */

/* Option products */
#define	PCI_PRODUCT_OPTION_F32	0x000c		/* 3G+ UMTS HSDPA (F32) */

/* Oxford/ VScom */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI010L	0x8001		/* 010L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI100L	0x8010		/* 100L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI110L	0x8011		/* 110L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI200L	0x8020		/* 200L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI210L	0x8021		/* 210L */
#define	PCI_PRODUCT_MOLEX_VSCOM_PCI400L	0x8040		/* 400L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI800L	0x8080		/* 800L */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCIx10H	0xa000		/* x10H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI100H	0xa001		/* 100H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI200H	0xa005		/* 200H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI800H_0	0xa003		/* 400H/800H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI800H_1	0xa004		/* 800H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI200HV2	0xe020		/* 200HV2 */
#define	PCI_PRODUCT_OXFORD2_VSCOM_PCI011H	0x8403		/* 011H */
#define	PCI_PRODUCT_OXFORD2_OX16PCI954	0x9501		/* OX16PCI954 */
#define	PCI_PRODUCT_OXFORD2_OX16PCI954K	0x9504		/* OX16PCI954K */
#define	PCI_PRODUCT_OXFORD2_EXSYS_EX41092	0x950a		/* Exsys EX-41092 */
#define	PCI_PRODUCT_OXFORD2_OXCB950	0x950b		/* OXCB950 */
#define	PCI_PRODUCT_OXFORD2_OXMPCI954	0x950c		/* OXmPCI954 */
#define	PCI_PRODUCT_OXFORD2_OXMPCI954D	0x9510		/* OXmPCI954 Disabled */
#define	PCI_PRODUCT_OXFORD2_EXSYS_EX41098	0x9511		/* Exsys EX-41098 */
#define	PCI_PRODUCT_OXFORD2_OX16PCI954P	0x9513		/* OX16PCI954 Parallel */
#define	PCI_PRODUCT_OXFORD2_OX16PCI952	0x9521		/* OX16PCI952 */
#define	PCI_PRODUCT_OXFORD2_OX16PCI952P	0x9523		/* OX16PCI952 Parallel */

/* Pacific Data products */
#define	PCI_PRODUCT_PDC_QSTOR_SATA	0x2068		/* QStor SATA */

/* Packet Engines products */
#define	PCI_PRODUCT_PE_GNIC2	0x0911		/* PMC/GNIC2 */

/* Parallels products */
#define	PCI_PRODUCT_PARALLELS_TOOLS	0x1112		/* Tools */
#define	PCI_PRODUCT_PARALLELS_VIDEO	0x1121		/* Video */

/* PC Tech products */
#define	PCI_PRODUCT_PCTECH_RZ1000	0x1000		/* RZ1000 */

/* PCTEL */
#define	PCI_PRODUCT_PCTEL_MICROMODEM56	0x7879		/* HSP MicroModem 56 */
#define	PCI_PRODUCT_PCTEL_MICROMODEM56_1	0x7892		/* HSP MicroModem 56 */

/* Qumranet products */
#define	PCI_PRODUCT_QUMRANET_VIO_NET	0x1000		/* Virtio Network */
#define	PCI_PRODUCT_QUMRANET_VIO_BLOCK	0x1001		/* Virtio Storage */
#define	PCI_PRODUCT_QUMRANET_VIO_MEM	0x1002		/* Virtio Memory */
#define	PCI_PRODUCT_QUMRANET_VIO_CONS	0x1003		/* Virtio Console */

/* Ross -> Pequr -> ServerWorks -> Broadcom ServerWorks products */
#define	PCI_PRODUCT_RCC_CMIC_LE	0x0000		/* CMIC-LE */
#define	PCI_PRODUCT_RCC_CNB20_LE	0x0005		/* CNB20-LE Host */
#define	PCI_PRODUCT_RCC_CNB20HE_1	0x0006		/* CNB20HE Host */
#define	PCI_PRODUCT_RCC_CNB20_LE_2	0x0007		/* CNB20-LE Host */
#define	PCI_PRODUCT_RCC_CNB20HE_2	0x0008		/* CNB20HE Host */
#define	PCI_PRODUCT_RCC_CNB20LE	0x0009		/* CNB20LE Host */
#define	PCI_PRODUCT_RCC_CIOB30	0x0010		/* CIOB30 */
#define	PCI_PRODUCT_RCC_CMIC_HE	0x0011		/* CMIC-HE */
#define	PCI_PRODUCT_RCC_CMIC_WS_GC_LE	0x0012		/* CMIC-WS Host (GC-LE) */
#define	PCI_PRODUCT_RCC_CNB20_HE	0x0013		/* CNB20-HE Host */
#define	PCI_PRODUCT_RCC_CMIC_LE_GC_LE	0x0014		/* CNB20-HE Host (GC-LE) */
#define	PCI_PRODUCT_RCC_CMIC_GC_1	0x0015		/* CMIC-GC Host */
#define	PCI_PRODUCT_RCC_CMIC_GC_2	0x0016		/* CMIC-GC Host */
#define	PCI_PRODUCT_RCC_GCNB_LE	0x0017		/* GCNB-LE Host */
#define	PCI_PRODUCT_RCC_HT_1000_PCI	0x0036		/* HT-1000 PCI */
#define	PCI_PRODUCT_RCC_CIOB_X2	0x0101		/* CIOB-X2 PCIX */
#define	PCI_PRODUCT_RCC_PCIE_PCIX	0x0103		/* PCIE-PCIX */
#define	PCI_PRODUCT_RCC_HT_1000_PCIX	0x0104		/* HT-1000 PCIX */
#define	PCI_PRODUCT_RCC_CIOB_E	0x0110		/* CIOB-E */
#define	PCI_PRODUCT_RCC_HT_2000_PCIX	0x0130		/* HT-2000 PCIX */
#define	PCI_PRODUCT_RCC_HT_2000_PCIE	0x0132		/* HT-2000 PCIE */
#define	PCI_PRODUCT_RCC_HT_2100_PCIE_1	0x0140		/* HT-2100 PCIE */
#define	PCI_PRODUCT_RCC_HT_2100_PCIE_2	0x0141		/* HT-2100 PCIE */
#define	PCI_PRODUCT_RCC_HT_2100_PCIE_3	0x0142		/* HT-2100 PCIE */
#define	PCI_PRODUCT_RCC_HT_2100_PCIE_5	0x0144		/* HT-2100 PCIE */
#define	PCI_PRODUCT_RCC_OSB4	0x0200		/* OSB4 */
#define	PCI_PRODUCT_RCC_CSB5	0x0201		/* CSB5 */
#define	PCI_PRODUCT_RCC_CSB6	0x0203		/* CSB6 */
#define	PCI_PRODUCT_RCC_HT_1000	0x0205		/* HT-1000 */
#define	PCI_PRODUCT_RCC_IDE	0x0210		/* IDE */
#define	PCI_PRODUCT_RCC_OSB4_IDE	0x0211		/* OSB4 IDE */
#define	PCI_PRODUCT_RCC_CSB5_IDE	0x0212		/* CSB5 IDE */
#define	PCI_PRODUCT_RCC_CSB6_RAID_IDE	0x0213		/* CSB6 RAID/IDE */
#define	PCI_PRODUCT_RCC_HT_1000_IDE	0x0214		/* HT-1000 IDE */
#define	PCI_PRODUCT_RCC_CSB6_IDE	0x0217		/* CSB6 IDE */
#define	PCI_PRODUCT_RCC_USB	0x0220		/* OSB4/CSB5 USB */
#define	PCI_PRODUCT_RCC_CSB6_USB	0x0221		/* CSB6 USB */
#define	PCI_PRODUCT_RCC_HT_1000_USB	0x0223		/* HT-1000 USB */
#define	PCI_PRODUCT_RCC_CSB5_LPC_1	0x0225		/* CSB5 LPC */
#define	PCI_PRODUCT_RCC_GCLE_2	0x0227		/* GCLE-2 Host */
#define	PCI_PRODUCT_RCC_CSB5_LPC_2	0x0230		/* CSB5 LPC */
#define	PCI_PRODUCT_RCC_HT_1000_LPC	0x0234		/* HT-1000 LPC */
#define	PCI_PRODUCT_RCC_K2_SATA	0x0240		/* K2 SATA */
#define	PCI_PRODUCT_RCC_FRODO4_SATA	0x0241		/* Frodo4 SATA */
#define	PCI_PRODUCT_RCC_FRODO8_SATA	0x0242		/* Frodo8 SATA */
#define	PCI_PRODUCT_RCC_HT_1000_SATA_1	0x024a		/* HT-1000 SATA */
#define	PCI_PRODUCT_RCC_HT_1000_SATA_2	0x024b		/* HT-1000 SATA */
#define	PCI_PRODUCT_RCC_HT_1100	0x0408		/* HT-1100 */

/* Rendition products */
#define	PCI_PRODUCT_RENDITION_V1000	0x0001		/* Verite 1000 */
#define	PCI_PRODUCT_RENDITION_V2x00	0x2000		/* Verite V2x00 */

/* Rhino Equipment products */
#define	PCI_PRODUCT_RHINO_R1T1	0x0105		/* T1/E1/J1 */
#define	PCI_PRODUCT_RHINO_R2T1	0x0605		/* Dual T1/E1/J1 */
#define	PCI_PRODUCT_RHINO_R4T1	0x0305		/* Quad T1/E1/J1 */

/* Philips products */
#define	PCI_PRODUCT_PHILIPS_OHCI	0x1561		/* ISP156x USB */
#define	PCI_PRODUCT_PHILIPS_EHCI	0x1562		/* ISP156x USB */
#define	PCI_PRODUCT_PHILIPS_SAA7130	0x7130		/* SAA7130 TV */
#define	PCI_PRODUCT_PHILIPS_SAA7133	0x7133		/* SAA7133 TV */
#define	PCI_PRODUCT_PHILIPS_SAA7134	0x7134		/* SAA7134 TV */
#define	PCI_PRODUCT_PHILIPS_SAA7135	0x7135		/* SAA7135 TV */

/* Phison products */
#define	PCI_PRODUCT_PHISON_PS5000	0x5000		/* PS5000 */

/* Picopower */
#define	PCI_PRODUCT_PICOPOWER_PT80C826	0x0000		/* PT80C826 */
#define	PCI_PRODUCT_PICOPOWER_PT86C521	0x0001		/* PT86C521 */
#define	PCI_PRODUCT_PICOPOWER_PT86C523	0x0002		/* PT86C523 */
#define	PCI_PRODUCT_PICOPOWER_PC87550	0x0005		/* PC87550 */
#define	PCI_PRODUCT_PICOPOWER_PT86C523_2	0x8002		/* PT86C523_2 */

/* Pijnenburg */
#define	PCI_PRODUCT_PIJNENBURG_PCC_ISES	0x0001		/* PCC-ISES */
#define	PCI_PRODUCT_PIJNENBURG_PCWD_PCI	0x5030		/* PCI PC WD */

/* Platform */
#define	PCI_PRODUCT_PLATFORM_ES1849	0x0100		/* ES1849 */

/* PLX products */
#define	PCI_PRODUCT_PLX_1076	0x1076		/* I/O 1076 */
#define	PCI_PRODUCT_PLX_1077	0x1077		/* I/O 1077 */
#define	PCI_PRODUCT_PLX_PCI_6520	0x6520		/* PCI 6520 */
#define	PCI_PRODUCT_PLX_PEX_8112	0x8112		/* PEX 8112 */
#define	PCI_PRODUCT_PLX_PEX_8114	0x8114		/* PEX 8114 */
#define	PCI_PRODUCT_PLX_PEX_8517	0x8517		/* PEX 8517 */
#define	PCI_PRODUCT_PLX_PEX_8518	0x8518		/* PEX 8518 */
#define	PCI_PRODUCT_PLX_PEX_8524	0x8524		/* PEX 8524 */
#define	PCI_PRODUCT_PLX_PEX_8525	0x8525		/* PEX 8525 */
#define	PCI_PRODUCT_PLX_PEX_8532	0x8532		/* PEX 8532 */
#define	PCI_PRODUCT_PLX_PEX_8533	0x8533		/* PEX 8533 */
#define	PCI_PRODUCT_PLX_PEX_8547	0x8547		/* PEX 8547 */
#define	PCI_PRODUCT_PLX_PEX_8548	0x8548		/* PEX 8548 */
#define	PCI_PRODUCT_PLX_PEX_8648	0x8648		/* PEX 8648 */
#define	PCI_PRODUCT_PLX_9016	0x9016		/* I/O 9016 */
#define	PCI_PRODUCT_PLX_9050	0x9050		/* I/O 9050 */
#define	PCI_PRODUCT_PLX_9080	0x9080		/* I/O 9080 */
#define	PCI_PRODUCT_PLX_CRONYX_OMEGA	0xc001		/* Cronyx Omega */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_PDC20265	0x0d30		/* PDC20265 */
#define	PCI_PRODUCT_PROMISE_PDC20263	0x0d38		/* PDC20263 */
#define	PCI_PRODUCT_PROMISE_PDC20275	0x1275		/* PDC20275 */
#define	PCI_PRODUCT_PROMISE_PDC20318	0x3318		/* PDC20318 */
#define	PCI_PRODUCT_PROMISE_PDC20319	0x3319		/* PDC20319 */
#define	PCI_PRODUCT_PROMISE_PDC20371	0x3371		/* PDC20371 */
#define	PCI_PRODUCT_PROMISE_PDC20379	0x3372		/* PDC20379 */
#define	PCI_PRODUCT_PROMISE_PDC20378	0x3373		/* PDC20378 */
#define	PCI_PRODUCT_PROMISE_PDC20375	0x3375		/* PDC20375 */
#define	PCI_PRODUCT_PROMISE_PDC20376	0x3376		/* PDC20376 */
#define	PCI_PRODUCT_PROMISE_PDC20377	0x3377		/* PDC20377 */
#define	PCI_PRODUCT_PROMISE_PDC40719	0x3515		/* PDC40719 */
#define	PCI_PRODUCT_PROMISE_PDC40519	0x3519		/* PDC40519 */
#define	PCI_PRODUCT_PROMISE_PDC20771	0x3570		/* PDC20771 */
#define	PCI_PRODUCT_PROMISE_PDC20571	0x3571		/* PDC20571 */
#define	PCI_PRODUCT_PROMISE_PDC20579	0x3574		/* PDC20579 */
#define	PCI_PRODUCT_PROMISE_PDC40779	0x3577		/* PDC40779 */
#define	PCI_PRODUCT_PROMISE_PDC40718	0x3d17		/* PDC40718 */
#define	PCI_PRODUCT_PROMISE_PDC40518	0x3d18		/* PDC40518 */
#define	PCI_PRODUCT_PROMISE_PDC20775	0x3d73		/* PDC20775 */
#define	PCI_PRODUCT_PROMISE_PDC20575	0x3d75		/* PDC20575 */
#define	PCI_PRODUCT_PROMISE_PDC42819	0x3f20		/* PDC42819 */
#define	PCI_PRODUCT_PROMISE_PDC20267	0x4d30		/* PDC20267 */
#define	PCI_PRODUCT_PROMISE_PDC20246	0x4d33		/* PDC20246 */
#define	PCI_PRODUCT_PROMISE_PDC20262	0x4d38		/* PDC20262 */
#define	PCI_PRODUCT_PROMISE_PDC20268	0x4d68		/* PDC20268 */
#define	PCI_PRODUCT_PROMISE_PDC20269	0x4d69		/* PDC20269 */
#define	PCI_PRODUCT_PROMISE_PDC20276	0x5275		/* PDC20276 */
#define	PCI_PRODUCT_PROMISE_DC5030	0x5300		/* DC5030 */
#define	PCI_PRODUCT_PROMISE_PDC20268R	0x6268		/* PDC20268R */
#define	PCI_PRODUCT_PROMISE_PDC20271	0x6269		/* PDC20271 */
#define	PCI_PRODUCT_PROMISE_PDC20617	0x6617		/* PDC20617 */
#define	PCI_PRODUCT_PROMISE_PDC20620	0x6620		/* PDC20620 */
#define	PCI_PRODUCT_PROMISE_PDC20621	0x6621		/* PDC20621 */
#define	PCI_PRODUCT_PROMISE_PDC20618	0x6626		/* PDC20618 */
#define	PCI_PRODUCT_PROMISE_PDC20619	0x6629		/* PDC20619 */
#define	PCI_PRODUCT_PROMISE_PDC20277	0x7275		/* PDC20277 */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016		/* ISP10160 */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */
#define	PCI_PRODUCT_QLOGIC_ISP1022	0x1022		/* ISP1022 */
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080		/* ISP1080 */
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216		/* ISP12160 */
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240		/* ISP1240 */
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280		/* ISP1280 */
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100		/* ISP2100 */
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200		/* ISP2200 */
#define	PCI_PRODUCT_QLOGIC_ISP2300	0x2300		/* ISP2300 */
#define	PCI_PRODUCT_QLOGIC_ISP2312	0x2312		/* ISP2312 */
#define	PCI_PRODUCT_QLOGIC_ISP2322	0x2322		/* ISP2322 */
#define	PCI_PRODUCT_QLOGIC_ISP2422	0x2422		/* ISP2422 */
#define	PCI_PRODUCT_QLOGIC_ISP2432	0x2432		/* ISP2432 */
#define	PCI_PRODUCT_QLOGIC_ISP2512	0x2512		/* ISP2512 */
#define	PCI_PRODUCT_QLOGIC_ISP2522	0x2522		/* ISP2522 */
#define	PCI_PRODUCT_QLOGIC_ISP2532	0x2532		/* ISP2532 */
#define	PCI_PRODUCT_QLOGIC_ISP4010_TOE	0x3010		/* ISP4010 iSCSI TOE */
#define	PCI_PRODUCT_QLOGIC_ISP4022_TOE	0x3022		/* ISP4022 iSCSI TOE */
#define	PCI_PRODUCT_QLOGIC_ISP4032_TOE	0x3032		/* ISP4032 iSCSI TOE */
#define	PCI_PRODUCT_QLOGIC_ISP4010_HBA	0x4010		/* ISP4010 iSCSI HBA */
#define	PCI_PRODUCT_QLOGIC_ISP4022_HBA	0x4022		/* ISP4022 iSCSI HBA */
#define	PCI_PRODUCT_QLOGIC_ISP4032_HBA	0x4032		/* ISP4032 iSCSI HBA */
#define	PCI_PRODUCT_QLOGIC_ISP5422	0x5422		/* ISP5422 */
#define	PCI_PRODUCT_QLOGIC_ISP5432	0x5432		/* ISP5432 */
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312		/* ISP6312 */
#define	PCI_PRODUCT_QLOGIC_ISP6322	0x6322		/* ISP6322 */
#define	PCI_PRODUCT_QLOGIC_ISP8432	0x8432		/* ISP8432 */

/* Quancom products */
#define	PCI_PRODUCT_QUANCOM_PWDOG1	0x0010		/* PWDOG1 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* Ralink Technology Corporation */
#define	PCI_PRODUCT_RALINK_RT2460A	0x0101		/* RT2460A */
#define	PCI_PRODUCT_RALINK_RT2560	0x0201		/* RT2560 */
#define	PCI_PRODUCT_RALINK_RT2561S	0x0301		/* RT2561S */
#define	PCI_PRODUCT_RALINK_RT2561	0x0302		/* RT2561 */
#define	PCI_PRODUCT_RALINK_RT2661	0x0401		/* RT2661 */
#define	PCI_PRODUCT_RALINK_RT2860	0x0601		/* RT2860 */
#define	PCI_PRODUCT_RALINK_RT2890	0x0681		/* RT2890 */
#define	PCI_PRODUCT_RALINK_RT2760	0x0701		/* RT2760 */
#define	PCI_PRODUCT_RALINK_RT2790	0x0781		/* RT2790 */
#define	PCI_PRODUCT_RALINK_RT3090	0x3090		/* RT3090 */
#define	PCI_PRODUCT_RALINK_RT3091	0x3091		/* RT3091 */
#define	PCI_PRODUCT_RALINK_RT3092	0x3092		/* RT3092 */

/* RDC products */
#define	PCI_PRODUCT_RDC_R1010_IDE	0x1010		/* R1010 IDE */
#define	PCI_PRODUCT_RDC_R6021_HB	0x6021		/* R6021 Host */
#define	PCI_PRODUCT_RDC_R6031_ISA	0x6031		/* R6031 ISA */
#define	PCI_PRODUCT_RDC_R6040_ETHER	0x6040		/* R6040 Ethernet */
#define	PCI_PRODUCT_RDC_R6060_OHCI	0x6060		/* R6060 USB */
#define	PCI_PRODUCT_RDC_R6061_EHCI	0x6061		/* R6061 USB */

/* Realtek products */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* 8029 */
#define	PCI_PRODUCT_REALTEK_RT8139D	0x8039		/* 8139D */
#define	PCI_PRODUCT_REALTEK_RT8129	0x8129		/* 8129 */
#define	PCI_PRODUCT_REALTEK_RT8101E	0x8136		/* 8101E */
#define	PCI_PRODUCT_REALTEK_RT8138	0x8138		/* 8138 */
#define	PCI_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 */
#define	PCI_PRODUCT_REALTEK_RT8169SC	0x8167		/* 8169SC */
#define	PCI_PRODUCT_REALTEK_RT8168	0x8168		/* 8168 */
#define	PCI_PRODUCT_REALTEK_RT8169	0x8169		/* 8169 */
#define	PCI_PRODUCT_REALTEK_RT8180	0x8180		/* 8180 */
#define	PCI_PRODUCT_REALTEK_RT8185	0x8185		/* 8185 */
#define	PCI_PRODUCT_REALTEK_RTL8187SE	0x8199		/* 8187SE */

/* RICOH products */
#define	PCI_PRODUCT_RICOH_RF5C465	0x0465		/* 5C465 CardBus */
#define	PCI_PRODUCT_RICOH_RF5C466	0x0466		/* 5C466 CardBus */
#define	PCI_PRODUCT_RICOH_RF5C475	0x0475		/* 5C475 CardBus */
#define	PCI_PRODUCT_RICOH_RF5C476	0x0476		/* 5C476 CardBus */
#define	PCI_PRODUCT_RICOH_RF5C477	0x0477		/* 5C477 CardBus */
#define	PCI_PRODUCT_RICOH_RF5C478	0x0478		/* 5C478 CardBus */
#define	PCI_PRODUCT_RICOH_R5C521	0x0521		/* 5C521 Firewire */
#define	PCI_PRODUCT_RICOH_R5C551	0x0551		/* 5C551 Firewire */
#define	PCI_PRODUCT_RICOH_RL5C552	0x0552		/* 5C552 Firewire */
#define	PCI_PRODUCT_RICOH_R5C592	0x0592		/* 5C592 Memory Stick */
#define	PCI_PRODUCT_RICOH_R5C822	0x0822		/* 5C822 SD/MMC */
#define	PCI_PRODUCT_RICOH_R5C832	0x0832		/* 5C832 Firewire */
#define	PCI_PRODUCT_RICOH_R5C843	0x0843		/* 5C843 MMC */
#define	PCI_PRODUCT_RICOH_R5C852	0x0852		/* 5C852 xD */
#define	PCI_PRODUCT_RICOH_R5U230	0xe230		/* 5U230 Memory Stick */
#define	PCI_PRODUCT_RICOH_R5U822	0xe822		/* 5U822 SD/MMC */
#define	PCI_PRODUCT_RICOH_R5U832	0xe832		/* 5U832 Firewire */
#define	PCI_PRODUCT_RICOH_R5U852	0xe852		/* 5U852 SD/MMC */

/* Rockwell products */
#define	PCI_PRODUCT_ROCKWELL_RS56SP_PCI11P1	0x2005		/* RS56/SP-PCI11P1 Modem */

/* S3 products */
#define	PCI_PRODUCT_S3_VIRGE	0x5631		/* ViRGE */
#define	PCI_PRODUCT_S3_TRIO32	0x8810		/* Trio32 */
#define	PCI_PRODUCT_S3_TRIO64	0x8811		/* Trio32/64 */
#define	PCI_PRODUCT_S3_AURORA64P	0x8812		/* Aurora64V+ */
#define	PCI_PRODUCT_S3_TRIO64UVP	0x8814		/* Trio64UV+ */
#define	PCI_PRODUCT_S3_868	0x8880		/* 868 */
#define	PCI_PRODUCT_S3_VIRGE_VX	0x883d		/* ViRGE VX */
#define	PCI_PRODUCT_S3_928	0x88b0		/* 86C928 */
#define	PCI_PRODUCT_S3_864_0	0x88c0		/* 86C864-0 */
#define	PCI_PRODUCT_S3_864_1	0x88c1		/* 86C864-1 */
#define	PCI_PRODUCT_S3_864_2	0x88c2		/* 86C864-2 */
#define	PCI_PRODUCT_S3_864_3	0x88c3		/* 86C864-3 */
#define	PCI_PRODUCT_S3_964_0	0x88d0		/* 86C964-0 */
#define	PCI_PRODUCT_S3_964_1	0x88d1		/* 86C964-1 */
#define	PCI_PRODUCT_S3_964_2	0x88d2		/* 86C964-2 */
#define	PCI_PRODUCT_S3_964_3	0x88d3		/* 86C964-3 */
#define	PCI_PRODUCT_S3_968_0	0x88f0		/* 86C968-0 */
#define	PCI_PRODUCT_S3_968_1	0x88f1		/* 86C968-1 */
#define	PCI_PRODUCT_S3_968_2	0x88f2		/* 86C968-2 */
#define	PCI_PRODUCT_S3_968_3	0x88f3		/* 86C968-3 */
#define	PCI_PRODUCT_S3_TRIO64V2_DX	0x8901		/* Trio64V2/DX */
#define	PCI_PRODUCT_S3_PLATO	0x8902		/* Plato */
#define	PCI_PRODUCT_S3_TRIO3D_AGP	0x8904		/* Trio3D AGP */
#define	PCI_PRODUCT_S3_VIRGE_DX_GX	0x8a01		/* ViRGE DX/GX */
#define	PCI_PRODUCT_S3_VIRGE_GX2	0x8a10		/* ViRGE GX2 */
#define	PCI_PRODUCT_S3_TRIO3_DX2	0x8a13		/* Trio3 DX2 */
#define	PCI_PRODUCT_S3_SAVAGE3D	0x8a20		/* Savage 3D */
#define	PCI_PRODUCT_S3_SAVAGE3D_M	0x8a21		/* Savage 3DM */
#define	PCI_PRODUCT_S3_SAVAGE4	0x8a22		/* Savage 4 */
#define	PCI_PRODUCT_S3_SAVAGE4_2	0x8a23		/* Savage 4 */
#define	PCI_PRODUCT_S3_PROSAVAGE_PM133	0x8a25		/* ProSavage PM133 */
#define	PCI_PRODUCT_S3_PROSAVAGE_KM133	0x8a26		/* ProSavage KM133 */
#define	PCI_PRODUCT_S3_VIRGE_MX	0x8c01		/* ViRGE MX */
#define	PCI_PRODUCT_S3_VIRGE_MXP	0x8c03		/* ViRGE MXP */
#define	PCI_PRODUCT_S3_SAVAGE_MXMV	0x8c10		/* Savage/MX-MV */
#define	PCI_PRODUCT_S3_SAVAGE_MX	0x8c11		/* Savage/MX */
#define	PCI_PRODUCT_S3_SAVAGE_IXMV	0x8c12		/* Savage/IX-MV */
#define	PCI_PRODUCT_S3_SAVAGE_IX	0x8c13		/* Savage/IX */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_MX128	0x8c22		/* SuperSavage MX/128 */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_MX64	0x8c24		/* SuperSavage MX/64 */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_MX64C	0x8c26		/* SuperSavage MX/64C */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IX128SDR	0x8c2a		/* SuperSavage IX/128 SDR */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IX128DDR	0x8c2b		/* SuperSavage IX/128 DDR */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IX64SDR	0x8c2c		/* SuperSavage IX/64 SDR */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IX64DDR	0x8c2d		/* SuperSavage IX/64 DDR */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IXCSDR	0x8c2e		/* SuperSavage IX/C SDR */
#define	PCI_PRODUCT_S3_SUPERSAVAGE_IXCDDR	0x8c2f		/* SuperSavage IX/C DDR */
#define	PCI_PRODUCT_S3_TWISTER	0x8d01		/* Twister */
#define	PCI_PRODUCT_S3_TWISTER_K	0x8d02		/* Twister-K */
#define	PCI_PRODUCT_S3_PROSAVAGE_DDR	0x8d03		/* ProSavage DDR */
#define	PCI_PRODUCT_S3_PROSAVAGE_DDR_K	0x8d04		/* ProSavage DDR-K */
#define	PCI_PRODUCT_S3_SONICVIBES	0xca00		/* SonicVibes */

/* SafeNet products */
#define	PCI_PRODUCT_SAFENET_SAFEXCEL	0x1141		/* SafeXcel */

/* Samsung products */
#define	PCI_PRODUCT_SAMSUNG_SWL2210P	0xa000		/* MagicLAN SWL-2210P */

/* Sangoma products */
#define	PCI_PRODUCT_SANGOMA_A10X	0x0300		/* A10x */

/* Digi International */
#define	PCI_PRODUCT_DIGI_NEO4	0x00b0		/* Neo-4 */
#define	PCI_PRODUCT_DIGI_NEO8	0x00b1		/* Neo-8 */

/* Schneider & Koch (SysKonnect) */
#define	PCI_PRODUCT_SCHNEIDERKOCH_FDDI	0x4000		/* FDDI */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK98XX	0x4300		/* SK-98xx */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK98XX2	0x4320		/* SK-98xx v2.0 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9D21	0x4400		/* SK-9D21 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9Sxx	0x9000		/* SK-9Sxx */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9821	0x9821		/* SK-9821 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9843	0x9843		/* SK-9843 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9Exx	0x9e00		/* SK-9Exx */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9E21M	0x9e01		/* SK-9E21M */

/* SGI products */
#define	PCI_PRODUCT_SGI_IOC3	0x0003		/* IOC3 */
#define	PCI_PRODUCT_SGI_RAD1	0x0005		/* Rad1 */
#define	PCI_PRODUCT_SGI_TIGON	0x0009		/* Tigon */
#define	PCI_PRODUCT_SGI_IOC4	0x100a		/* IOC4 */

/* SGS Thomson products */
#define	PCI_PRODUCT_SGSTHOMSON_2000	0x0008		/* STG 2000X */
#define	PCI_PRODUCT_SGSTHOMSON_1764	0x0009		/* STG 1764 */
#define	PCI_PRODUCT_SGSTHOMSON_KYROII	0x0010		/* Kyro-II */
#define	PCI_PRODUCT_SGSTHOMSON_1764X	0x1746		/* STG 1764X */

/* Sigma Designs */
#define	PCI_PRODUCT_SIGMA_64GX	0x6401		/* 64GX */
#define	PCI_PRODUCT_SIGMA_DVDMAGICPRO	0x8300		/* DVDmagic-PRO */

/* Silan products */
#define	PCI_PRODUCT_SILAN_SC92301	0x2301		/* SC92301 */
#define	PCI_PRODUCT_SILAN_8139D	0x8139		/* 8139D */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 AGP */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 VGA */
#define	PCI_PRODUCT_SIS_648FX	0x0003		/* 648FX AGP */
#define	PCI_PRODUCT_SIS_PPB_1	0x0004		/* PCI-PCI */
#define	PCI_PRODUCT_SIS_86C205_1	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 System */
#define	PCI_PRODUCT_SIS_5595	0x0009		/* 5595 System */
#define	PCI_PRODUCT_SIS_PPB_2	0x000a		/* PCI-PCI */
#define	PCI_PRODUCT_SIS_85C503_ISA	0x0018		/* 85C503 ISA */
#define	PCI_PRODUCT_SIS_180	0x0180		/* 180 SATA */
#define	PCI_PRODUCT_SIS_181	0x0181		/* 181 SATA */
#define	PCI_PRODUCT_SIS_182	0x0182		/* 182 SATA */
#define	PCI_PRODUCT_SIS_183	0x0183		/* 183 SATA */
#define	PCI_PRODUCT_SIS_190	0x0190		/* 190 */
#define	PCI_PRODUCT_SIS_191	0x0191		/* 191 */
#define	PCI_PRODUCT_SIS_5597_VGA	0x0200		/* 5597/5598 VGA */
#define	PCI_PRODUCT_SIS_6215	0x0204		/* 6215 */
#define	PCI_PRODUCT_SIS_86C205_2	0x0205		/* 86C205 */
#define	PCI_PRODUCT_SIS_300	0x0300		/* 300/305/630 VGA */
#define	PCI_PRODUCT_SIS_315PRO_VGA	0x0325		/* 315 Pro VGA */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_85C596	0x0596		/* 85C596 */
#define	PCI_PRODUCT_SIS_530	0x0530		/* 530 PCI */
#define	PCI_PRODUCT_SIS_540	0x0540		/* 540 PCI */
#define	PCI_PRODUCT_SIS_550	0x0550		/* 550 PCI */
#define	PCI_PRODUCT_SIS_85C601	0x0601		/* 85C601 EIDE */
#define	PCI_PRODUCT_SIS_620	0x0620		/* 620 PCI */
#define	PCI_PRODUCT_SIS_630	0x0630		/* 630 PCI */
#define	PCI_PRODUCT_SIS_633	0x0633		/* 633 PCI */
#define	PCI_PRODUCT_SIS_635	0x0635		/* 635 PCI */
#define	PCI_PRODUCT_SIS_640	0x0640		/* 640 PCI */
#define	PCI_PRODUCT_SIS_645	0x0645		/* 645 PCI */
#define	PCI_PRODUCT_SIS_646	0x0646		/* 646 PCI */
#define	PCI_PRODUCT_SIS_648	0x0648		/* 648 PCI */
#define	PCI_PRODUCT_SIS_649	0x0649		/* 649 PCI */
#define	PCI_PRODUCT_SIS_650	0x0650		/* 650 PCI */
#define	PCI_PRODUCT_SIS_651	0x0651		/* 651 PCI */
#define	PCI_PRODUCT_SIS_652	0x0652		/* 652 PCI */
#define	PCI_PRODUCT_SIS_655	0x0655		/* 655 PCI */
#define	PCI_PRODUCT_SIS_656	0x0656		/* 656 PCI */
#define	PCI_PRODUCT_SIS_658	0x0658		/* 658 PCI */
#define	PCI_PRODUCT_SIS_661	0x0661		/* 661 PCI */
#define	PCI_PRODUCT_SIS_662	0x0662		/* 662 PCI */
#define	PCI_PRODUCT_SIS_671	0x0671		/* 671 PCI */
#define	PCI_PRODUCT_SIS_730	0x0730		/* 730 PCI */
#define	PCI_PRODUCT_SIS_733	0x0733		/* 733 PCI */
#define	PCI_PRODUCT_SIS_735	0x0735		/* 735 PCI */
#define	PCI_PRODUCT_SIS_740	0x0740		/* 740 PCI */
#define	PCI_PRODUCT_SIS_741	0x0741		/* 741 PCI */
#define	PCI_PRODUCT_SIS_745	0x0745		/* 745 PCI */
#define	PCI_PRODUCT_SIS_746	0x0746		/* 746 PCI */
#define	PCI_PRODUCT_SIS_748	0x0748		/* 748 PCI */
#define	PCI_PRODUCT_SIS_750	0x0750		/* 750 PCI */
#define	PCI_PRODUCT_SIS_751	0x0751		/* 751 PCI */
#define	PCI_PRODUCT_SIS_752	0x0752		/* 752 PCI */
#define	PCI_PRODUCT_SIS_755	0x0755		/* 755 PCI */
#define	PCI_PRODUCT_SIS_756	0x0756		/* 756 PCI */
#define	PCI_PRODUCT_SIS_760	0x0760		/* 760 PCI */
#define	PCI_PRODUCT_SIS_761	0x0761		/* 761 PCI */
#define	PCI_PRODUCT_SIS_900	0x0900		/* 900 10/100BaseTX */
#define	PCI_PRODUCT_SIS_961	0x0961		/* 961 ISA */
#define	PCI_PRODUCT_SIS_962	0x0962		/* 962 ISA */
#define	PCI_PRODUCT_SIS_963	0x0963		/* 963 ISA */
#define	PCI_PRODUCT_SIS_964	0x0964		/* 964 ISA */
#define	PCI_PRODUCT_SIS_965	0x0965		/* 965 ISA */
#define	PCI_PRODUCT_SIS_966	0x0966		/* 966 ISA */
#define	PCI_PRODUCT_SIS_968	0x0968		/* 968 ISA */
#define	PCI_PRODUCT_SIS_5300	0x5300		/* 540 VGA */
#define	PCI_PRODUCT_SIS_5315	0x5315		/* 530 VGA */
#define	PCI_PRODUCT_SIS_5511	0x5511		/* 5511 */
#define	PCI_PRODUCT_SIS_5512	0x5512		/* 5512 */
#define	PCI_PRODUCT_SIS_5513	0x5513		/* 5513 EIDE */
#define	PCI_PRODUCT_SIS_5518	0x5518		/* 5518 EIDE */
#define	PCI_PRODUCT_SIS_5571	0x5571		/* 5571 PCI */
#define	PCI_PRODUCT_SIS_5581	0x5581		/* 5581 */
#define	PCI_PRODUCT_SIS_5582	0x5582		/* 5582 */
#define	PCI_PRODUCT_SIS_5591	0x5591		/* 5591 PCI */
#define	PCI_PRODUCT_SIS_5596	0x5596		/* 5596 */
#define	PCI_PRODUCT_SIS_5597_HB	0x5597		/* 5597/5598 Host */
#define	PCI_PRODUCT_SIS_6204	0x6204		/* 6204 */
#define	PCI_PRODUCT_SIS_6205	0x6205		/* 6205 */
#define	PCI_PRODUCT_SIS_6300	0x6300		/* 6300 */
#define	PCI_PRODUCT_SIS_530_VGA	0x6306		/* 530 VGA */
#define	PCI_PRODUCT_SIS_650_VGA	0x6325		/* 650 VGA */
#define	PCI_PRODUCT_SIS_6326	0x6326		/* 6326 VGA */
#define	PCI_PRODUCT_SIS_6330	0x6330		/* 6330 VGA */
#define	PCI_PRODUCT_SIS_5597_USB	0x7001		/* 5597/5598 USB */
#define	PCI_PRODUCT_SIS_7002	0x7002		/* 7002 USB */
#define	PCI_PRODUCT_SIS_7007	0x7007		/* 7007 FireWire */
#define	PCI_PRODUCT_SIS_7012_ACA	0x7012		/* 7012 AC97 */
#define	PCI_PRODUCT_SIS_7013	0x7013		/* 7013 Modem */
#define	PCI_PRODUCT_SIS_7016	0x7016		/* 7016 10/100BaseTX */
#define	PCI_PRODUCT_SIS_7018	0x7018		/* 7018 Audio */
#define	PCI_PRODUCT_SIS_7019	0x7019		/* 7019 Audio */
#define	PCI_PRODUCT_SIS_7300	0x7300		/* 7300 VGA */
#define	PCI_PRODUCT_SIS_966_HDA	0x7502		/* 966 HD Audio */

/* SMC products */
#define	PCI_PRODUCT_SMC_83C170	0x0005		/* 83C170 (EPIC/100) */
#define	PCI_PRODUCT_SMC_83C175	0x0006		/* 83C175 (EPIC/100) */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC 37C665 */
#define	PCI_PRODUCT_SMC_37C922	0x1001		/* FDC 37C922 */

/* Silicon Motion products */
#define	PCI_PRODUCT_SMI_SM501	0x0501		/* Voyager GX */
#define	PCI_PRODUCT_SMI_SM710	0x0710		/* LynxEM */
#define	PCI_PRODUCT_SMI_SM712	0x0712		/* LynxEM+ */
#define	PCI_PRODUCT_SMI_SM720	0x0720		/* Lynx3DM */
#define	PCI_PRODUCT_SMI_SM810	0x0810		/* LynxE */
#define	PCI_PRODUCT_SMI_SM811	0x0811		/* LynxE+ */
#define	PCI_PRODUCT_SMI_SM820	0x0820		/* Lynx3D */
#define	PCI_PRODUCT_SMI_SM910	0x0910		/* 910 */

/* SMSC products */
#define	PCI_PRODUCT_SMSC_VICTORY66_IDE_1	0x9130		/* Victory66 IDE */
#define	PCI_PRODUCT_SMSC_VICTORY66_ISA	0x9460		/* Victory66 ISA */
#define	PCI_PRODUCT_SMSC_VICTORY66_IDE_2	0x9461		/* Victory66 IDE */
#define	PCI_PRODUCT_SMSC_VICTORY66_USB	0x9462		/* Victory66 USB */
#define	PCI_PRODUCT_SMSC_VICTORY66_PM	0x9463		/* Victory66 Power */

/* SNI products */
#define	PCI_PRODUCT_SNI_PIRAHNA	0x0002		/* Pirahna 2-port */
#define	PCI_PRODUCT_SNI_TCPMSE	0x0005		/* Tulip, power, switch extender */
#define	PCI_PRODUCT_SNI_FPGAIBUS	0x4942		/* FPGA I-Bus Tracer for MBD */
#define	PCI_PRODUCT_SNI_SZB6120	0x6120		/* SZB6120 */

/* Sony products */
#define	PCI_PRODUCT_SONY_CXD1947A	0x8009		/* CXD1947A FireWire */
#define	PCI_PRODUCT_SONY_CXD3222	0x8039		/* CXD3222 FireWire */
#define	PCI_PRODUCT_SONY_MEMSTICK_SLOT	0x808a		/* Memory Stick Slot */

/* Stallion Technologies products */
#define	PCI_PRODUCT_STALLION_EASYIO	0x0003		/* EasyIO */

/* STB products */
#define	PCI_PRODUCT_STB2_RIVA128	0x0018		/* Velocity128 */

/* Sun */
#define	PCI_PRODUCT_SUN_EBUS	0x1000		/* PCIO EBus2 */
#define	PCI_PRODUCT_SUN_HME	0x1001		/* HME */
#define	PCI_PRODUCT_SUN_RIO_EBUS	0x1100		/* RIO EBus */
#define	PCI_PRODUCT_SUN_ERINETWORK	0x1101		/* ERI Ether */
#define	PCI_PRODUCT_SUN_FIREWIRE	0x1102		/* FireWire */
#define	PCI_PRODUCT_SUN_USB	0x1103		/* USB */
#define	PCI_PRODUCT_SUN_GEMNETWORK	0x2bad		/* GEM */
#define	PCI_PRODUCT_SUN_SIMBA	0x5000		/* Simba PCI-PCI */
#define	PCI_PRODUCT_SUN_5821	0x5454		/* Crypto 5821 */
#define	PCI_PRODUCT_SUN_SCA1K	0x5455		/* Crypto 1K */
#define	PCI_PRODUCT_SUN_SCA6K	0x5ca0		/* Crypto 6K */
#define	PCI_PRODUCT_SUN_PSYCHO	0x8000		/* Psycho PCI */
#define	PCI_PRODUCT_SUN_MS_IIep	0x9000		/* microSPARC IIep PCI */
#define	PCI_PRODUCT_SUN_US_IIi	0xa000		/* UltraSPARC IIi PCI */
#define	PCI_PRODUCT_SUN_US_IIe	0xa001		/* UltraSPARC IIe PCI */
#define	PCI_PRODUCT_SUN_CASSINI	0xabba		/* Cassini */
#define	PCI_PRODUCT_SUN_NEPTUNE	0xabcd		/* Neptune */
#define	PCI_PRODUCT_SUN_SBBC	0xc416		/* SBBC */

/* Sundance products */
#define	PCI_PRODUCT_SUNDANCE_ST201_1	0x0200		/* ST201 */
#define	PCI_PRODUCT_SUNDANCE_ST201_2	0x0201		/* ST201 */
#define	PCI_PRODUCT_SUNDANCE_TC9021	0x1021		/* TC9021 */
#define	PCI_PRODUCT_SUNDANCE_ST1023	0x1023		/* ST1023 */
#define	PCI_PRODUCT_SUNDANCE_ST2021	0x2021		/* ST2021 */
#define	PCI_PRODUCT_SUNDANCE_TC9021_ALT	0x9021		/* TC9021 (alt ID) */

/* Sunix */
#define	PCI_PRODUCT_SUNIX_40XX	0x7168		/* 40XX */
#define	PCI_PRODUCT_SUNIX_4018A	0x7268		/* 4018A */

/* Surecom products */
#define	PCI_PRODUCT_SURECOM_NE34	0x0e34		/* NE-34 */

/* Syba */
#define	PCI_PRODUCT_SYBA_4S2P	0x0781		/* 4S2P */
#define	PCI_PRODUCT_SYBA_4S	0x0786		/* 4S */

/* Symbol */
#define	PCI_PRODUCT_SYMBOL_LA41X3	0x0001		/* Spectrum24 LA41X3 */

/* Symphony Labs products */
#define	PCI_PRODUCT_SYMPHONY_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_82C103	0x0103		/* 82C103 */
#define	PCI_PRODUCT_SYMPHONY_82C105	0x0105		/* 82C105 */
#define	PCI_PRODUCT_SYMPHONY2_82C101	0x0001		/* 82C101 */
#define	PCI_PRODUCT_SYMPHONY_82C565	0x0565		/* 82C565 ISA */

/* TTTech */
#define	PCI_PRODUCT_TTTECH_MC322	0x000a		/* MC322 */

/* Tamarack Microelectronics */
#define	PCI_PRODUCT_TAMARACK_TC9021	0x1021		/* TC9021 GigE */
#define	PCI_PRODUCT_TAMARACK_TC9021_ALT	0x9021		/* TC9021 GigE (alt ID) */

/* Techsan Electronics */
#define	PCI_PRODUCT_TECHSAN_B2C2_SKY2PC	0x2104		/* B2C2 Sky2PC */
#define	PCI_PRODUCT_TECHSAN_B2C2_SKY2PC_2	0x2200		/* B2C2 Sky2PC */

/* Tehuti Networks Ltd */
#define	PCI_PRODUCT_TEHUTI_TN3009	0x3009		/* TN3009 */
#define	PCI_PRODUCT_TEHUTI_TN3010	0x3010		/* TN3010 */
#define	PCI_PRODUCT_TEHUTI_TN3014	0x3014		/* TN3014 */

/* Tekram Technology products (1st ID)*/
#define	PCI_PRODUCT_TEKRAM_DC290	0xdc29		/* DC-290(M) */

/* Tekram Technology products(2) */
#define	PCI_PRODUCT_TEKRAM2_DC690C	0x690c		/* DC-690C */
#define	PCI_PRODUCT_TEKRAM2_DC3X5U	0x0391		/* DC-3x5U */

/* TerraTec Electronic Gmbh */
#define	PCI_PRODUCT_TERRATEC_TVALUE_PLUS	0x1127		/* Terratec TV+ */
#define	PCI_PRODUCT_TERRATEC_TVALUE	0x1134		/* Terratec TValue */
#define	PCI_PRODUCT_TERRATEC_TVALUER	0x1135		/* Terratec TValue Radio */

/* Texas Instruments products */
#define	PCI_PRODUCT_TI_TLAN	0x0500		/* TLAN */
#define	PCI_PRODUCT_TI_PERMEDIA	0x3d04		/* 3DLabs Permedia */
#define	PCI_PRODUCT_TI_PERMEDIA2	0x3d07		/* 3DLabs Permedia 2 */
#define	PCI_PRODUCT_TI_TSB12LV21	0x8000		/* TSB12LV21 FireWire */
#define	PCI_PRODUCT_TI_TSB12LV22	0x8009		/* TSB12LV22 FireWire */
#define	PCI_PRODUCT_TI_PCI4450_FW	0x8011		/* PCI4450 FireWire */
#define	PCI_PRODUCT_TI_PCI4410_FW	0x8017		/* PCI4410 FireWire */
#define	PCI_PRODUCT_TI_TSB12LV23	0x8019		/* TSB12LV23 FireWire */
#define	PCI_PRODUCT_TI_TSB12LV26	0x8020		/* TSB12LV26 FireWire */
#define	PCI_PRODUCT_TI_TSB43AA22	0x8021		/* TSB43AA22 FireWire */
#define	PCI_PRODUCT_TI_TSB43AB22	0x8023		/* TSB43AB22 FireWire */
#define	PCI_PRODUCT_TI_TSB43AB23	0x8024		/* TSB43AB23 FireWire */
#define	PCI_PRODUCT_TI_TSB82AA2	0x8025		/* TSB82AA2 FireWire */
#define	PCI_PRODUCT_TI_TSB43AB21	0x8026		/* TSB43AB21 FireWire */
#define	PCI_PRODUCT_TI_PCI4451_FW	0x8027		/* PCI4451 FireWire */
#define	PCI_PRODUCT_TI_PCI4510_FW	0x8029		/* PCI4510 FireWire */
#define	PCI_PRODUCT_TI_PCI4520_FW	0x802a		/* PCI4520 FireWire */
#define	PCI_PRODUCT_TI_PCI7410_FW	0x802b		/* PCI7(4-6)10 FireWire */
#define	PCI_PRODUCT_TI_PCI7420_FW	0x802e		/* PCI7x20 FireWire */
#define	PCI_PRODUCT_TI_PCI7XX1	0x8031		/* PCI7XX1 CardBus */
#define	PCI_PRODUCT_TI_PCI7XX1_FW	0x8032		/* PCI7XX1 FireWire */
#define	PCI_PRODUCT_TI_PCI7XX1_FLASH	0x8033		/* PCI7XX1 Flash */
#define	PCI_PRODUCT_TI_PCI7XX1_SD	0x8034		/* PCI7XX1 Secure Data */
#define	PCI_PRODUCT_TI_PCI7XX1_SM	0x8035		/* PCI7XX1 Smart Card */
#define	PCI_PRODUCT_TI_PCI6515	0x8036		/* PCI6515 CardBus */
#define	PCI_PRODUCT_TI_PCI6515SC	0x8038		/* PCI6515 CardBus (Smart Card mode) */
#define	PCI_PRODUCT_TI_PCIXX12	0x8039		/* PCIXX12 CardBus */
#define	PCI_PRODUCT_TI_PCIXX12_FW	0x803a		/* PCIXX12 FireWire */
#define	PCI_PRODUCT_TI_PCIXX12_MCR	0x803b		/* PCIXX12 Multimedia Card Reader */
#define	PCI_PRODUCT_TI_PCIXX12_SD	0x803c		/* PCIXX12 Secure Data */
#define	PCI_PRODUCT_TI_PCIXX12_SM	0x803d		/* PCIXX12 Smart Card */
#define	PCI_PRODUCT_TI_PCI1620_MISC	0x8201		/* PCI1620 Misc */
#define	PCI_PRODUCT_TI_XIO2000A	0x8231		/* XIO2000A PCIE-PCI */
#define	PCI_PRODUCT_TI_XIO3130	0x8231		/* XIO3130 PCIE-PCIE */
#define	PCI_PRODUCT_TI_ACX100A	0x8400		/* ACX100A */
#define	PCI_PRODUCT_TI_ACX100B	0x8401		/* ACX100B */
#define	PCI_PRODUCT_TI_ACX111	0x9066		/* ACX111 */
#define	PCI_PRODUCT_TI_PCI1130	0xac12		/* PCI1130 CardBus */
#define	PCI_PRODUCT_TI_PCI1031	0xac13		/* PCI1031 PCMCIA */
#define	PCI_PRODUCT_TI_PCI1131	0xac15		/* PCI1131 CardBus */
#define	PCI_PRODUCT_TI_PCI1250	0xac16		/* PCI1250 CardBus */
#define	PCI_PRODUCT_TI_PCI1220	0xac17		/* PCI1220 CardBus */
#define	PCI_PRODUCT_TI_PCI1221	0xac19		/* PCI1221 CardBus */
#define	PCI_PRODUCT_TI_PCI1210	0xac1a		/* PCI1210 CardBus */
#define	PCI_PRODUCT_TI_PCI1450	0xac1b		/* PCI1450 CardBus */
#define	PCI_PRODUCT_TI_PCI1225	0xac1c		/* PCI1225 CardBus */
#define	PCI_PRODUCT_TI_PCI1251	0xac1d		/* PCI1251 CardBus */
#define	PCI_PRODUCT_TI_PCI1211	0xac1e		/* PCI1211 CardBus */
#define	PCI_PRODUCT_TI_PCI1251B	0xac1f		/* PCI1251B CardBus */
#define	PCI_PRODUCT_TI_PCI2030	0xac20		/* PCI2030 PCI-PCI */
#define	PCI_PRODUCT_TI_PCI2031	0xac21		/* PCI2031 PCI-PCI */
#define	PCI_PRODUCT_TI_PCI2032	0xac22		/* PCI2032 PCI-PCI */
#define	PCI_PRODUCT_TI_PCI2250	0xac23		/* PCI2250 PCI-PCI */
#define	PCI_PRODUCT_TI_PCI2050	0xac28		/* PCI2050 PCI-PCI */
#define	PCI_PRODUCT_TI_PCI4450_CB	0xac40		/* PCI4450 CardBus */
#define	PCI_PRODUCT_TI_PCI4410_CB	0xac41		/* PCI4410 CardBus */
#define	PCI_PRODUCT_TI_PCI4451_CB	0xac42		/* PCI4451 CardBus */
#define	PCI_PRODUCT_TI_PCI4510_CB	0xac44		/* PCI4510 CardBus */
#define	PCI_PRODUCT_TI_PCI4520_CB	0xac46		/* PCI4520 CardBus */
#define	PCI_PRODUCT_TI_PCI7510_CB	0xac47		/* PCI7510 CardBus */
#define	PCI_PRODUCT_TI_PCI7610_CB	0xac48		/* PCI7610 CardBus */
#define	PCI_PRODUCT_TI_PCI7410_CB	0xac49		/* PCI7410 CardBus */
#define	PCI_PRODUCT_TI_PCI7610SM	0xac4a		/* PCI7610 CardBus (Smart Card mode) */
#define	PCI_PRODUCT_TI_PCI7410SD	0xac4b		/* PCI7[46]10 CardBus (SD/MMC mode) */
#define	PCI_PRODUCT_TI_PCI7410MS	0xac4c		/* PCI7[46]10 CardBus (Memory stick mode) */
#define	PCI_PRODUCT_TI_PCI1410	0xac50		/* PCI1410 CardBus */
#define	PCI_PRODUCT_TI_PCI1420	0xac51		/* PCI1420 CardBus */
#define	PCI_PRODUCT_TI_PCI1451	0xac52		/* PCI1451 CardBus */
#define	PCI_PRODUCT_TI_PCI1421	0xac53		/* PCI1421 CardBus */
#define	PCI_PRODUCT_TI_PCI1620	0xac54		/* PCI1620 CardBus */
#define	PCI_PRODUCT_TI_PCI1520	0xac55		/* PCI1520 CardBus */
#define	PCI_PRODUCT_TI_PCI1510	0xac56		/* PCI1510 CardBus */
#define	PCI_PRODUCT_TI_PCI1530	0xac57		/* PCI1530 CardBus */
#define	PCI_PRODUCT_TI_PCI1515	0xac58		/* PCI1515 CardBus */
#define	PCI_PRODUCT_TI_PCI2040	0xac60		/* PCI2040 DSP */
#define	PCI_PRODUCT_TI_PCI7420	0xac8e		/* PCI7420 CardBus */

/* TigerJet Network products */
#define	PCI_PRODUCT_TIGERJET_TIGER320	0x0001		/* PCI */

/* Topic */
#define	PCI_PRODUCT_TOPIC_5634PCV	0x0000		/* 5634PCV SurfRider */

/* Toshiba products */
#define	PCI_PRODUCT_TOSHIBA_R4x00	0x0009		/* R4x00 */
#define	PCI_PRODUCT_TOSHIBA_TC35856F	0x0020		/* TC35856F ATM (Meteor) */
#define	PCI_PRODUCT_TOSHIBA_R4X00	0x102f		/* R4x00 PCI */

/* Toshiba(2) products */
#define	PCI_PRODUCT_TOSHIBA2_THB	0x0601		/* PCI */
#define	PCI_PRODUCT_TOSHIBA2_ISA	0x0602		/* ISA */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95	0x0603		/* ToPIC95 CardBus-PCI */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC95B	0x060a		/* ToPIC95B CardBus */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC97	0x060f		/* ToPIC97 CardBus */
#define	PCI_PRODUCT_TOSHIBA2_ToPIC100	0x0617		/* ToPIC100 CardBus */
#define	PCI_PRODUCT_TOSHIBA2_TFIRO	0x0701		/* Fast Infrared Type O */
#define	PCI_PRODUCT_TOSHIBA2_SDCARD	0x0805		/* SD Controller */

/* Transmeta products */
#define	PCI_PRODUCT_TRANSMETA_NB	0x0295		/* Northbridge */
#define	PCI_PRODUCT_TRANSMETA_LONGRUN_NB	0x0395		/* LongRun Northbridge */
#define	PCI_PRODUCT_TRANSMETA_MEM1	0x0396		/* Mem1 */
#define	PCI_PRODUCT_TRANSMETA_MEM2	0x0397		/* Mem2 */

/* Trident products */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_DX	0x2000		/* 4DWAVE DX */
#define	PCI_PRODUCT_TRIDENT_4DWAVE_NX	0x2001		/* 4DWAVE NX */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEI7	0x8400		/* CyberBlade i7 */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEI7AGP	0x8420		/* CyberBlade i7 AGP */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEI1	0x8500		/* CyberBlade i1 */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEI1AGP	0x8520		/* CyberBlade i1 AGP */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEAI1	0x8600		/* CyberBlade Ai1 */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEAI1AGP	0x8620		/* CyberBlade Ai1 AGP */
#define	PCI_PRODUCT_TRIDENT_CYBERBLADEXPAI1	0x8820		/* CyberBlade XP/Ai1 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9320	0x9320		/* TGUI 9320 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9350	0x9350		/* TGUI 9350 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9360	0x9360		/* TGUI 9360 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9388	0x9388		/* TGUI 9388 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397	0x9397		/* CYBER 9397 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9397DVD	0x939a		/* CYBER 9397DVD */
#define	PCI_PRODUCT_TRIDENT_TGUI_9420	0x9420		/* TGUI 9420 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9440	0x9440		/* TGUI 9440 */
#define	PCI_PRODUCT_TRIDENT_CYBER_9525	0x9525		/* CYBER 9525 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9660	0x9660		/* TGUI 9660 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9680	0x9680		/* TGUI 9680 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9682	0x9682		/* TGUI 9682 */
#define	PCI_PRODUCT_TRIDENT_3DIMAGE_9750	0x9750		/* 3DImage 9750 */
#define	PCI_PRODUCT_TRIDENT_3DIMAGE_9850	0x9850		/* 3DImage 9850 */
#define	PCI_PRODUCT_TRIDENT_BLADE_3D	0x9880		/* Blade 3D */
#define	PCI_PRODUCT_TRIDENT_BLADE_XP	0x9910		/* CyberBlade XP */
#define	PCI_PRODUCT_TRIDENT_BLADE_XP2	0x9960		/* CyberBlade XP2 */

/* Triones/HighPoint Technologies products */
#define	PCI_PRODUCT_TRIONES_HPT343	0x0003		/* HPT343/345 IDE */
#define	PCI_PRODUCT_TRIONES_HPT366	0x0004		/* HPT36x/37x IDE */
#define	PCI_PRODUCT_TRIONES_HPT372A	0x0005		/* HPT372A IDE */
#define	PCI_PRODUCT_TRIONES_HPT302	0x0006		/* HPT302 IDE */
#define	PCI_PRODUCT_TRIONES_HPT371	0x0007		/* HPT371 IDE */
#define	PCI_PRODUCT_TRIONES_HPT374	0x0008		/* HPT374 IDE */

/* TriTech Microelectronics products*/
#define	PCI_PRODUCT_TRITECH_TR25202	0xfc02		/* Pyramid3D TR25202 */

/* Tseng Labs products */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_A	0x3202		/* ET4000w32p rev A */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_B	0x3205		/* ET4000w32p rev B */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_C	0x3206		/* ET4000w32p rev C */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_D	0x3207		/* ET4000w32p rev D */
#define	PCI_PRODUCT_TSENG_ET6000	0x3208		/* ET6000/ET6100 */
#define	PCI_PRODUCT_TSENG_ET6300	0x4702		/* ET6300 */

/* Tvia products */
#define	PCI_PRODUCT_TVIA_IGA1680	0x1680		/* IGA-1680 */
#define	PCI_PRODUCT_TVIA_IGA1682	0x1682		/* IGA-1682 */
#define	PCI_PRODUCT_TVIA_IGA1683	0x1683		/* IGA-1683 */
#define	PCI_PRODUCT_TVIA_CP2000	0x2000		/* CyberPro 2000 */
#define	PCI_PRODUCT_TVIA_CP2000A	0x2010		/* CyberPro 2010 */
#define	PCI_PRODUCT_TVIA_CP5000	0x5000		/* CyberPro 5000 */
#define	PCI_PRODUCT_TVIA_CP5050	0x5050		/* CyberPro 5050 */
#define	PCI_PRODUCT_TVIA_CP5202	0x5202		/* CyberPro 5202 */
#define	PCI_PRODUCT_TVIA_CP5252	0x5252		/* CyberPro 5252 */

/* Turtle Beach products */
#define	PCI_PRODUCT_TURTLEBEACH_SANTA_CRUZ	0x3357		/* Santa Cruz */

/* UMC products */
#define	PCI_PRODUCT_UMC_UM82C881	0x0001		/* UM82C881 486 */
#define	PCI_PRODUCT_UMC_UM82C886	0x0002		/* UM82C886 ISA */
#define	PCI_PRODUCT_UMC_UM8673F	0x0101		/* UM8673F EIDE */
#define	PCI_PRODUCT_UMC_UM8881	0x0881		/* UM8881 HB4 486 PCI */
#define	PCI_PRODUCT_UMC_UM82C891	0x0891		/* UM82C891 */
#define	PCI_PRODUCT_UMC_UM886A	0x1001		/* UM886A */
#define	PCI_PRODUCT_UMC_UM8886BF	0x673a		/* UM8886BF */
#define	PCI_PRODUCT_UMC_UM8710	0x8710		/* UM8710 */
#define	PCI_PRODUCT_UMC_UM8886	0x886a		/* UM8886 */
#define	PCI_PRODUCT_UMC_UM8881F	0x8881		/* UM8881F Host */
#define	PCI_PRODUCT_UMC_UM8886F	0x8886		/* UM8886F ISA */
#define	PCI_PRODUCT_UMC_UM8886A	0x888a		/* UM8886A */
#define	PCI_PRODUCT_UMC_UM8891A	0x8891		/* UM8891A */
#define	PCI_PRODUCT_UMC_UM9017F	0x9017		/* UM9017F */
#define	PCI_PRODUCT_UMC_UM8886E_OR_WHAT	0xe886		/* ISA */
#define	PCI_PRODUCT_UMC_UM8886N	0xe88a		/* UM8886N */
#define	PCI_PRODUCT_UMC_UM8891N	0xe891		/* UM8891N */

/* ULSI Systems products */
#define	PCI_PRODUCT_ULSI_US201	0x0201		/* US201 */

/* US Robotics */
#define	PCI_PRODUCT_USR2_USR997902	0x0116		/* USR997902 */
#define	PCI_PRODUCT_USR_3CP5610	0x1008		/* 3CP5610 */
#define	PCI_PRODUCT_USR2_WL11000P	0x3685		/* WL11000P */

/* V3 Semiconductor products */
#define	PCI_PRODUCT_V3_V961PBC	0x0002		/* V961PBC i960 PCI */
#define	PCI_PRODUCT_V3_V292PBC	0x0292		/* V292PBC AMD290x0 PCI */
#define	PCI_PRODUCT_V3_V960PBC	0x0960		/* V960PBC i960 PCI */
#define	PCI_PRODUCT_V3_V96DPC	0xc960		/* V96DPC i960 PCI */

/* VIA Technologies products */
#define	PCI_PRODUCT_VIATECH_K8M800_0	0x0204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_0	0x0238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PT880_0	0x0258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800_AGP	0x0259		/* PM800 AGP */
#define	PCI_PRODUCT_VIATECH_KT880_AGP	0x0269		/* KT880 AGP */
#define	PCI_PRODUCT_VIATECH_K8HTB_0	0x0282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8363	0x0305		/* VT8363 Host */
#define	PCI_PRODUCT_VIATECH_PT894	0x0308		/* PT894 Host */
#define	PCI_PRODUCT_VIATECH_CN700	0x0314		/* CN700 Host */
#define	PCI_PRODUCT_VIATECH_CX700	0x0324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_P4M890	0x0327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_0	0x0336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VT3351_HB	0x0351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VX800_0	0x0353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900	0x0364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_VT8371_HB	0x0391		/* VT8371 Host */
#define	PCI_PRODUCT_VIATECH_VT8501	0x0501		/* VT8501 */
#define	PCI_PRODUCT_VIATECH_VT82C505	0x0505		/* VT82C505 */
#define	PCI_PRODUCT_VIATECH_VT82C561	0x0561		/* VT82C561 */
#define	PCI_PRODUCT_VIATECH_VT82C571	0x0571		/* VT82C571 IDE */
#define	PCI_PRODUCT_VIATECH_VT82C576	0x0576		/* VT82C576 3V */
#define	PCI_PRODUCT_VIATECH_VX700_IDE	0x0581		/* VX700 IDE */
#define	PCI_PRODUCT_VIATECH_VT82C585	0x0585		/* VT82C585 ISA */
#define	PCI_PRODUCT_VIATECH_VT82C586_ISA	0x0586		/* VT82C586 ISA */
#define	PCI_PRODUCT_VIATECH_VT8237A_SATA	0x0591		/* VT8237A SATA */
#define	PCI_PRODUCT_VIATECH_VT82C595	0x0595		/* VT82C595 PCI */
#define	PCI_PRODUCT_VIATECH_VT82C596A	0x0596		/* VT82C596A ISA */
#define	PCI_PRODUCT_VIATECH_VT82C597PCI	0x0597		/* VT82C597 PCI */
#define	PCI_PRODUCT_VIATECH_VT82C598PCI	0x0598		/* VT82C598 PCI */
#define	PCI_PRODUCT_VIATECH_VT8601	0x0601		/* VT8601 PCI */
#define	PCI_PRODUCT_VIATECH_VT8605	0x0605		/* VT8605 PCI */
#define	PCI_PRODUCT_VIATECH_VT82C686A_ISA	0x0686		/* VT82C686 ISA */
#define	PCI_PRODUCT_VIATECH_VT82C691	0x0691		/* VT82C691 PCI */
#define	PCI_PRODUCT_VIATECH_VT82C693	0x0693		/* VT82C693 PCI */
#define	PCI_PRODUCT_VIATECH_VT86C926	0x0926		/* VT86C926 Amazon */
#define	PCI_PRODUCT_VIATECH_VT82C570M	0x1000		/* VT82C570M PCI */
#define	PCI_PRODUCT_VIATECH_VT82C570MV	0x1006		/* VT82C570M ISA */
#define	PCI_PRODUCT_VIATECH_CHROME9HC3	0x1122		/* Chrome9 HC3 IGP */
#define	PCI_PRODUCT_VIATECH_K8M800_1	0x1204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_1	0x1238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PT880_1	0x1258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800_ERRS	0x1259		/* PM800 Errors */
#define	PCI_PRODUCT_VIATECH_KT880_1	0x1269		/* KT880 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_1	0x1282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_PT894_2	0x1308		/* PT894 Host */
#define	PCI_PRODUCT_VIATECH_CN700_2	0x1314		/* CN700 Host */
#define	PCI_PRODUCT_VIATECH_CX700_1	0x1324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_1	0x1327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_1	0x1336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VT3351_2	0x1351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VX800_1	0x1353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_1	0x1364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_VT82C416	0x1571		/* VT82C416 IDE */
#define	PCI_PRODUCT_VIATECH_VT82C1595	0x1595		/* VT82C1595 PCI */
#define	PCI_PRODUCT_VIATECH_K8M800_2	0x2204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_2	0x2238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PT880_2	0x2258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800	0x2259		/* PM800 Host */
#define	PCI_PRODUCT_VIATECH_KT880_2	0x2269		/* KT880 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_2	0x2282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_PT894_3	0x2308		/* PT894 Host */
#define	PCI_PRODUCT_VIATECH_CN700_3	0x2314		/* CN700 Host */
#define	PCI_PRODUCT_VIATECH_CX700_2	0x2324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_2	0x2327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_2	0x2336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VT3351_3	0x2351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VX800_2	0x2353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_2	0x2364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_VT8251_PCI	0x287a		/* VT8251 PCI */
#define	PCI_PRODUCT_VIATECH_VT8251_PCIE_0	0x287b		/* VT8251 PCIE */
#define	PCI_PRODUCT_VIATECH_VT8251_PCIE_1	0x287c		/* VT8251 PCIE */
#define	PCI_PRODUCT_VIATECH_VT8251_PCIE_2	0x287d		/* VT8251 PCIE */
#define	PCI_PRODUCT_VIATECH_VT8251_VLINK	0x287e		/* VT8251 VLINK */
#define	PCI_PRODUCT_VIATECH_VT83C572	0x3038		/* VT83C572 USB */
#define	PCI_PRODUCT_VIATECH_VT82C586_PWR	0x3040		/* VT82C586 Power */
#define	PCI_PRODUCT_VIATECH_RHINE	0x3043		/* Rhine/RhineII */
#define	PCI_PRODUCT_VIATECH_VT6306	0x3044		/* VT6306 FireWire */
#define	PCI_PRODUCT_VIATECH_VT82C596	0x3050		/* VT82C596 Power */
#define	PCI_PRODUCT_VIATECH_VT82C596B_PM	0x3051		/* VT82C596B PM */
#define	PCI_PRODUCT_VIATECH_VT6105M	0x3053		/* VT6105M RhineIII */
#define	PCI_PRODUCT_VIATECH_VT82C686A_SMB	0x3057		/* VT82C686 SMBus */
#define	PCI_PRODUCT_VIATECH_VT82C686A_AC97	0x3058		/* VT82C686 AC97 */
#define	PCI_PRODUCT_VIATECH_VT8233_AC97	0x3059		/* VT8233 AC97 */
#define	PCI_PRODUCT_VIATECH_RHINEII_2	0x3065		/* RhineII-2 */
#define	PCI_PRODUCT_VIATECH_VT82C686A_ACM	0x3068		/* VT82C686 Modem */
#define	PCI_PRODUCT_VIATECH_VT8233_ISA	0x3074		/* VT8233 ISA */
#define	PCI_PRODUCT_VIATECH_VT8633	0x3091		/* VT8633 PCI */
#define	PCI_PRODUCT_VIATECH_VT8366	0x3099		/* VT8366 PCI */
#define	PCI_PRODUCT_VIATECH_VT8653_PCI	0x3101		/* VT8653 PCI */
#define	PCI_PRODUCT_VIATECH_VT6202	0x3104		/* VT6202 USB */
#define	PCI_PRODUCT_VIATECH_VT6105	0x3106		/* VT6105 RhineIII */
#define	PCI_PRODUCT_VIATECH_UNICHROME	0x3108		/* S3 Unichrome PRO IGP */
#define	PCI_PRODUCT_VIATECH_VT8361_PCI	0x3112		/* VT8361 PCI */
#define	PCI_PRODUCT_VIATECH_VT8101_PPB	0x3113		/* VT8101 VPX-64 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8375	0x3116		/* VT8375 PCI */
#define	PCI_PRODUCT_VIATECH_PM800_UNICHROME	0x3118		/* PM800 Unichrome S3 */
#define	PCI_PRODUCT_VIATECH_VT612x	0x3119		/* VT612x */
#define	PCI_PRODUCT_VIATECH_CLE266	0x3122		/* CLE266 */
#define	PCI_PRODUCT_VIATECH_VT8623	0x3123		/* VT8623 PCI */
#define	PCI_PRODUCT_VIATECH_VT8233A_ISA	0x3147		/* VT8233A ISA */
#define	PCI_PRODUCT_VIATECH_VT8751	0x3148		/* VT8751 PCI */
#define	PCI_PRODUCT_VIATECH_VT6420_SATA	0x3149		/* VT6420 SATA */
#define	PCI_PRODUCT_VIATECH_UNICHROME2_1	0x3157		/* S3 UniChrome Pro II IGP */
#define	PCI_PRODUCT_VIATECH_VT6410	0x3164		/* VT6410 IDE */
#define	PCI_PRODUCT_VIATECH_P4X400	0x3168		/* P4X400 Host */
#define	PCI_PRODUCT_VIATECH_VT8235_ISA	0x3177		/* VT8235 ISA */
#define	PCI_PRODUCT_VIATECH_P4N333	0x3178		/* P4N333 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB	0x3188		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8377	0x3189		/* VT8377 PCI */
#define	PCI_PRODUCT_VIATECH_K8M800	0x3204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_VT8378	0x3205		/* VT8378 PCI */
#define	PCI_PRODUCT_VIATECH_PT890	0x3208		/* PT890 Host */
#define	PCI_PRODUCT_VIATECH_K8T800M	0x3218		/* K8T800M Host */
#define	PCI_PRODUCT_VIATECH_VT8237_ISA	0x3227		/* VT8237 ISA */
#define	PCI_PRODUCT_VIATECH_DELTACHROME	0x3230		/* DeltaChrome Video */
#define	PCI_PRODUCT_VIATECH_K8T890_3	0x3238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_VT6421_SATA	0x3249		/* VT6421 SATA */
#define	PCI_PRODUCT_VIATECH_CX700_PPB_1	0x324a		/* CX700 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_CX700_3	0x324b		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_VX700_1	0x324e		/* VX700 Host */
#define	PCI_PRODUCT_VIATECH_VT6655	0x3253		/* VT6655 */
#define	PCI_PRODUCT_VIATECH_PT880_3	0x3258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800_DRAM	0x3259		/* PM800 DRAM */
#define	PCI_PRODUCT_VIATECH_KT880_3	0x3269		/* KT880 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_3	0x3282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8251_ISA	0x3287		/* VT8251 ISA */
#define	PCI_PRODUCT_VIATECH_HDA	0x3288		/* HD Audio */
#define	PCI_PRODUCT_VIATECH_CX700_4	0x3324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_3	0x3327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_3	0x3336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VT8237A_ISA	0x3337		/* VT8237A ISA */
#define	PCI_PRODUCT_VIATECH_UNICHROME_3	0x3343		/* S3 Unichrome PRO IGP */
#define	PCI_PRODUCT_VIATECH_UNICHROME_2	0x3344		/* S3 Unichrome PRO IGP */
#define	PCI_PRODUCT_VIATECH_VT8251_SATA	0x3349		/* VT8251 SATA */
#define	PCI_PRODUCT_VIATECH_VT3351_4	0x3351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VX800_DRAM	0x3353		/* VX800 DRAM */
#define	PCI_PRODUCT_VIATECH_P4M900_3	0x3364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_CHROME9_HC	0x3371		/* Chrome9 HC IGP */
#define	PCI_PRODUCT_VIATECH_VT8237S_ISA	0x3372		/* VT8237S ISA */
#define	PCI_PRODUCT_VIATECH_VT8237A_PPB_1	0x337a		/* VT8237A PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8237A_PPB_2	0x337b		/* VT8237A PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8M800_4	0x4204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_4	0x4238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PT880_4	0x4258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800_PMC	0x4259		/* PM800 PMC */
#define	PCI_PRODUCT_VIATECH_KT880_4	0x4269		/* KT880 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_4	0x4282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_PT894_4	0x4308		/* PT894 Host */
#define	PCI_PRODUCT_VIATECH_CN700_4	0x4314		/* CN700 Host */
#define	PCI_PRODUCT_VIATECH_CX700_5	0x4324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_4	0x4327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_4	0x4336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VT3351_5	0x4351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VX800_4	0x4353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_4	0x4364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_IOAPIC	0x5238		/* K8T890 IOAPIC */
#define	PCI_PRODUCT_VIATECH_PT894_IOAPIC	0x5308		/* PT894 IOAPIC */
#define	PCI_PRODUCT_VIATECH_CX700_IDE	0x5324		/* CX700 IDE */
#define	PCI_PRODUCT_VIATECH_P4M890_IOAPIC	0x5327		/* P4M890 IOAPIC */
#define	PCI_PRODUCT_VIATECH_K8M890_IOAPIC	0x5336		/* K8M890 IOAPIC */
#define	PCI_PRODUCT_VIATECH_VT8237A_SATA_2	0x5337		/* VT8237A SATA */
#define	PCI_PRODUCT_VIATECH_VT3351_IOAPIC	0x5351		/* VT3351 IOAPIC */
#define	PCI_PRODUCT_VIATECH_VX800_IOAPIC	0x5353		/* VX800 IOAPIC */
#define	PCI_PRODUCT_VIATECH_P4M900_IOAPIC	0x5364		/* P4M900 IOAPIC */
#define	PCI_PRODUCT_VIATECH_VT8237S_SATA	0x5372		/* VT8237S SATA */
#define	PCI_PRODUCT_VIATECH_RHINEII	0x6100		/* RhineII */
#define	PCI_PRODUCT_VIATECH_VT3351_6	0x6238		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VT8251_AHCI	0x6287		/* VT8251 AHCI */
#define	PCI_PRODUCT_VIATECH_K8M890_6	0x6290		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_6	0x6327		/* P4M890 Security */
#define	PCI_PRODUCT_VIATECH_VX800_6	0x6353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_6	0x6364		/* P4M900 Security */
#define	PCI_PRODUCT_VIATECH_K8M800_7	0x7204		/* K8M800 Host */
#define	PCI_PRODUCT_VIATECH_VT8378_VGA	0x7205		/* VT8378 VGA */
#define	PCI_PRODUCT_VIATECH_PT894_5	0x7308		/* PT894 Host */
#define	PCI_PRODUCT_VIATECH_P4M890_7	0x7327		/* P4M890 Host */
#define	PCI_PRODUCT_VIATECH_K8M890_7	0x7336		/* K8M890 Host */
#define	PCI_PRODUCT_VIATECH_VX800_7	0x7353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_7	0x7364		/* P4M900 Host */
#define	PCI_PRODUCT_VIATECH_K8T890_7	0x7238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PT880_7	0x7258		/* PT880 Host */
#define	PCI_PRODUCT_VIATECH_PM800_PCI	0x7259		/* PM800 PCI */
#define	PCI_PRODUCT_VIATECH_KT880_7	0x7269		/* KT880 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_7	0x7282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_CN700_7	0x7314		/* CN700 Host */
#define	PCI_PRODUCT_VIATECH_CX700_7	0x7324		/* CX700 Host */
#define	PCI_PRODUCT_VIATECH_VT3351_7	0x7351		/* VT3351 Host */
#define	PCI_PRODUCT_VIATECH_VT8231_ISA	0x8231		/* VT8231 ISA */
#define	PCI_PRODUCT_VIATECH_VT8231_PWR	0x8235		/* VT8231 PMG */
#define	PCI_PRODUCT_VIATECH_VT8363_AGP	0x8305		/* VT8363 AGP */
#define	PCI_PRODUCT_VIATECH_CX700_ISA	0x8324		/* CX700 ISA */
#define	PCI_PRODUCT_VIATECH_VX800_ISA	0x8353		/* VX800 ISA */
#define	PCI_PRODUCT_VIATECH_VT8371_PPB	0x8391		/* VT8371 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX855_ISA	0x8409		/* VX855 ISA */
#define	PCI_PRODUCT_VIATECH_VT8501_AGP	0x8501		/* VT8501 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C597AGP	0x8597		/* VT82C597 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C598AGP	0x8598		/* VT82C598 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C601	0x8601		/* VT82C601 AGP */
#define	PCI_PRODUCT_VIATECH_VT8605_AGP	0x8605		/* VT8605 AGP */
#define	PCI_PRODUCT_VIATECH_VX800_SDMMC	0x9530		/* VX800 SD/MMC */
#define	PCI_PRODUCT_VIATECH_VX800_SDIO	0x95d0		/* VX800 SDIO */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_A	0xa238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_P4M890_PPB_1	0xa327		/* P4M890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX800_A	0xa353		/* VX800 Host */
#define	PCI_PRODUCT_VIATECH_P4M900_PPB_1	0xa364		/* P4M900 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8633_AGP	0xb091		/* VT8633 AGP */
#define	PCI_PRODUCT_VIATECH_VT8366_AGP	0xb099		/* VT8366 AGP */
#define	PCI_PRODUCT_VIATECH_VT8361_AGP	0xb112		/* VT8361 AGP */
#define	PCI_PRODUCT_VIATECH_VT8101_IOAPIC	0xb113		/* VT8101 VPX-64 IOAPIC */
#define	PCI_PRODUCT_VIATECH_VT8363_PCI	0xb115		/* VT8363 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8235_AGP	0xb168		/* VT8235 AGP */
#define	PCI_PRODUCT_VIATECH_K8HTB_AGP	0xb188		/* K8HTB AGP */
#define	PCI_PRODUCT_VIATECH_VT8377_AGP	0xb198		/* VT8377 AGP */
#define	PCI_PRODUCT_VIATECH_VX800_PPB	0xb353		/* VX800 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_B	0xb999		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_C	0xc238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_P4M890_PPB_2	0xc327		/* P4M890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX800_PCIE_0	0xc353		/* VX800 PCIE */
#define	PCI_PRODUCT_VIATECH_P4M900_PPB_2	0xc364		/* P4M900 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX855_IDE	0xc409		/* VX855 IDE */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_D	0xd238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_E	0xe238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX800_PCIE_1	0xe353		/* VX800 PCIE */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_F	0xf238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VX800_PCIE_2	0xf353		/* VX800 PCIE */

/* Vitesse Semiconductor products */
#define	PCI_PRODUCT_VITESSE_VSC_7174	0x7174		/* VSC-7174 SATA */

/* Vortex Computer Systems products */
#define	PCI_PRODUCT_VORTEX_GDT_60x0	0x0000		/* GDT6000/6020/6050 */
#define	PCI_PRODUCT_VORTEX_GDT_6000B	0x0001		/* GDT6000B/6010 */
#define	PCI_PRODUCT_VORTEX_GDT_6x10	0x0002		/* GDT6110/6510 */
#define	PCI_PRODUCT_VORTEX_GDT_6x20	0x0003		/* GDT6120/6520 */
#define	PCI_PRODUCT_VORTEX_GDT_6530	0x0004		/* GDT6530 */
#define	PCI_PRODUCT_VORTEX_GDT_6550	0x0005		/* GDT6550 */
#define	PCI_PRODUCT_VORTEX_GDT_6x17	0x0006		/* GDT6x17 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27	0x0007		/* GDT6x27 */
#define	PCI_PRODUCT_VORTEX_GDT_6537	0x0008		/* GDT6537 */
#define	PCI_PRODUCT_VORTEX_GDT_6557	0x0009		/* GDT6557 */
#define	PCI_PRODUCT_VORTEX_GDT_6x15	0x000a		/* GDT6x15 */
#define	PCI_PRODUCT_VORTEX_GDT_6x25	0x000b		/* GDT6x25 */
#define	PCI_PRODUCT_VORTEX_GDT_6535	0x000c		/* GDT6535 */
#define	PCI_PRODUCT_VORTEX_GDT_6555	0x000d		/* GDT6555 */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP	0x0100		/* GDT6x17RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP	0x0101		/* GDT6x27RP */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP	0x0102		/* GDT6537RP */
#define	PCI_PRODUCT_VORTEX_GDT_6557RP	0x0103		/* GDT6557RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP	0x0104		/* GDT6x11RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP	0x0105		/* GDT6x21RP */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RD	0x0110		/* GDT6x17RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RD	0x0111		/* GDT6x27RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6537RD	0x0112		/* GDT6537RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6557RD	0x0113		/* GDT6557RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RD	0x0114		/* GDT6x11RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RD	0x0115		/* GDT6x21RP1 */
#define	PCI_PRODUCT_VORTEX_GDT_6x18RD	0x0118		/* GDT6x18RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RD	0x0119		/* GDT6x28RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RD	0x011a		/* GDT6x38RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RD	0x011b		/* GDT6x58RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x17RP2	0x0120		/* GDT6x17RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x27RP2	0x0121		/* GDT6x27RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6537RP2	0x0122		/* GDT6537RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6557RP2	0x0123		/* GDT6557RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x11RP2	0x0124		/* GDT6x11RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x21RP2	0x0125		/* GDT6x21RP2 */
#define	PCI_PRODUCT_VORTEX_GDT_6x13RS	0x0136		/* GDT6513RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x23RS	0x0137		/* GDT6523RS */
#define	PCI_PRODUCT_VORTEX_GDT_6518RS	0x0138		/* GDT6518RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x28RS	0x0139		/* GDT6x28RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x38RS	0x013a		/* GDT6x38RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x58RS	0x013b		/* GDT6x58RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x33RS	0x013c		/* GDT6x33RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x43RS	0x013d		/* GDT6x43RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x53RS	0x013e		/* GDT6x53RS */
#define	PCI_PRODUCT_VORTEX_GDT_6x63RS	0x013f		/* GDT6x63RS */
#define	PCI_PRODUCT_VORTEX_GDT_7x13RN	0x0166		/* GDT7x13RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x23RN	0x0167		/* GDT7x23RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x18RN	0x0168		/* GDT7x18RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x28RN	0x0169		/* GDT7x28RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x38RN	0x016a		/* GDT7x38RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x58RN	0x016b		/* GDT7x58RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x43RN	0x016d		/* GDT7x43RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x53RN	0x016e		/* GDT7x53RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x63RN	0x016f		/* GDT7x63RN */
#define	PCI_PRODUCT_VORTEX_GDT_4x13RZ	0x01d6		/* GDT4x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_4x23RZ	0x01d7		/* GDT4x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x13RZ	0x01f6		/* GDT8x13RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x23RZ	0x01f7		/* GDT8x23RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x33RZ	0x01fc		/* GDT8x33RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x43RZ	0x01fd		/* GDT8x43RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x53RZ	0x01fe		/* GDT8x53RZ */
#define	PCI_PRODUCT_VORTEX_GDT_8x63RZ	0x01ff		/* GDT8x63RZ */
#define	PCI_PRODUCT_VORTEX_GDT_6x19RD	0x0210		/* GDT6x19RD */
#define	PCI_PRODUCT_VORTEX_GDT_6x29RD	0x0211		/* GDT6x29RD */
#define	PCI_PRODUCT_VORTEX_GDT_7x19RN	0x0260		/* GDT7x19RN */
#define	PCI_PRODUCT_VORTEX_GDT_7x29RN	0x0261		/* GDT7x29RN */
#define	PCI_PRODUCT_VORTEX_GDT_8x22RZ	0x02f6		/* GDT8x22RZ */
#define	PCI_PRODUCT_VORTEX_GDT_ICP	0x0300		/* ICP */
#define	PCI_PRODUCT_VORTEX_GDT_ICP2	0x0301		/* ICP */

/* VLSI products */
#define	PCI_PRODUCT_VLSI_82C592	0x0005		/* 82C592 CPU */
#define	PCI_PRODUCT_VLSI_82C593	0x0006		/* 82C593 ISA */
#define	PCI_PRODUCT_VLSI_82C594	0x0007		/* 82C594 Wildcat */
#define	PCI_PRODUCT_VLSI_82C596597	0x0008		/* 82C596/597 Wildcat ISA */
#define	PCI_PRODUCT_VLSI_82C541	0x000c		/* 82C541 */
#define	PCI_PRODUCT_VLSI_82C543	0x000d		/* 82C543 */
#define	PCI_PRODUCT_VLSI_82C532	0x0101		/* 82C532 */
#define	PCI_PRODUCT_VLSI_82C534	0x0102		/* 82C534 */
#define	PCI_PRODUCT_VLSI_82C535	0x0104		/* 82C535 */
#define	PCI_PRODUCT_VLSI_82C147	0x0105		/* 82C147 */
#define	PCI_PRODUCT_VLSI_82C975	0x0200		/* 82C975 */
#define	PCI_PRODUCT_VLSI_82C925	0x0280		/* 82C925 */

/* VMware */
#define	PCI_PRODUCT_VMWARE_VIRTUAL2	0x0405		/* Virtual SVGA II */
#define	PCI_PRODUCT_VMWARE_VIRTUAL	0x0710		/* Virtual SVGA */
#define	PCI_PRODUCT_VMWARE_NET	0x0720		/* Virtual NIC */
#define	PCI_PRODUCT_VMWARE_MACHINE_2	0x0740		/* Virtual Machine Communication Interface */
#define	PCI_PRODUCT_VMWARE_EHCI	0x0770		/* Virtual EHCI */
#define	PCI_PRODUCT_VMWARE_PCI	0x0790		/* Virtual PCI-PCI */
#define	PCI_PRODUCT_VMWARE_PCIE	0x07a0		/* Virtual PCIE-PCIE */
#define	PCI_PRODUCT_VMWARE_MACHINE	0x0801		/* Virtual Machine Interface */

/* Weitek products */
#define	PCI_PRODUCT_WEITEK_P9000	0x9001		/* P9000 */
#define	PCI_PRODUCT_WEITEK_P9100	0x9100		/* P9100 */

/* Western Digital products */
#define	PCI_PRODUCT_WD_WD33C193A	0x0193		/* WD33C193A */
#define	PCI_PRODUCT_WD_WD33C196A	0x0196		/* WD33C196A */
#define	PCI_PRODUCT_WD_WD33C197A	0x0197		/* WD33C197A */
#define	PCI_PRODUCT_WD_WD7193	0x3193		/* WD7193 */
#define	PCI_PRODUCT_WD_WD7197	0x3197		/* WD7197 */
#define	PCI_PRODUCT_WD_WD33C296A	0x3296		/* WD33C296A */
#define	PCI_PRODUCT_WD_WD34C296	0x4296		/* WD34C296 */
#define	PCI_PRODUCT_WD_WD9710	0x9710		/* WD9610 */
#define	PCI_PRODUCT_WD_90C	0xc24a		/* 90C */

/* Winbond Electronics products */
#define	PCI_PRODUCT_WINBOND_W83769F	0x0001		/* W83769F */
#define	PCI_PRODUCT_WINBOND_W89C840F	0x0840		/* W89C840F 10/100 */
#define	PCI_PRODUCT_WINBOND_W89C940F	0x0940		/* Linksys EtherPCI II */
#define	PCI_PRODUCT_WINBOND_W83C553F_0	0x0565		/* W83C553F ISA */
#define	PCI_PRODUCT_WINBOND_W83C553F_1	0x0105		/* W83C553F */
#define	PCI_PRODUCT_WINBOND_W89C940F_1	0x5a5a		/* W89C940F */
#define	PCI_PRODUCT_WINBOND_W6692	0x6692		/* W6692 ISDN */

/* Winbond Electronics products (PCI products set 2) */
#define	PCI_PRODUCT_WINBOND2_W89C940	0x1980		/* Linksys EtherPCI */

/* XenSource products */
#define	PCI_PRODUCT_XENSOURCE_PLATFORMDEV	0x0001		/* Platform Device */

/* XGI Technology products */
#define	PCI_PRODUCT_XGI_VOLARI_Z7	0x0020		/* Volari Z7 */
#define	PCI_PRODUCT_XGI_VOLARI_Z9	0x0021		/* Volari Z9s/Z9m */
#define	PCI_PRODUCT_XGI_VOLARI_V3XT	0x0040		/* Volari V3XT */

/* Xircom products */
#define	PCI_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 */
#define	PCI_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 (21143) */
#define	PCI_PRODUCT_XIRCOM_CARDBUS_ETH_1	0x0005		/* CardBus Ethernet */
#define	PCI_PRODUCT_XIRCOM_CARDBUS_ETH_2	0x0007		/* CardBus Ethernet */
#define	PCI_PRODUCT_XIRCOM_CARDBUS_ETH_3	0x000b		/* CardBus Ethernet */
#define	PCI_PRODUCT_XIRCOM_MPCI_MODEM_V90	0x000c		/* Mini-PCI V.90 56k Modem */
#define	PCI_PRODUCT_XIRCOM_CARDBUS_ETH_4	0x000f		/* CardBus Ethernet */
#define	PCI_PRODUCT_XIRCOM_MPCI_MODEM_K56	0x00d4		/* Mini-PCI K56Flex Modem */
#define	PCI_PRODUCT_XIRCOM_MODEM_56K	0x0101		/* CardBus 56k Modem */
#define	PCI_PRODUCT_XIRCOM_MODEM56	0x0103		/* 56k Modem */
#define	PCI_PRODUCT_XIRCOM_CBEM56G	0x0105		/* CBEM56G Modem */

/* Yamaha products */
#define	PCI_PRODUCT_YAMAHA_YMF724	0x0004		/* 724 */
#define	PCI_PRODUCT_YAMAHA_YMF734	0x0005		/* 734 */
#define	PCI_PRODUCT_YAMAHA_YMF738_TEG	0x0006		/* 738 */
#define	PCI_PRODUCT_YAMAHA_YMF737	0x0008		/* 737 */
#define	PCI_PRODUCT_YAMAHA_YMF740	0x000a		/* 740 */
#define	PCI_PRODUCT_YAMAHA_YMF740C	0x000c		/* 740C */
#define	PCI_PRODUCT_YAMAHA_YMF724F	0x000d		/* 724F */
#define	PCI_PRODUCT_YAMAHA_YMF744	0x0010		/* 744 */
#define	PCI_PRODUCT_YAMAHA_YMF754	0x0012		/* 754 */
#define	PCI_PRODUCT_YAMAHA_YMF738	0x0020		/* 738 */

/* Zeinet products */
#define	PCI_PRODUCT_ZEINET_1221	0x0001		/* 1221 */

/* Ziatech products */
#define	PCI_PRODUCT_ZIATECH_ZT8905	0x8905		/* PCI-ST32 */

/* Zoltrix products */
#define	PCI_PRODUCT_ZOLTRIX_GENIE_TV_FM	0x400d		/* Genie TV/FM */

/* Zoran products */
#define	PCI_PRODUCT_ZORAN_ZR36057	0x6057		/* TV */
#define	PCI_PRODUCT_ZORAN_ZR36120	0x6120		/* DVD */

/* ZyDAS Technology products */
#define	PCI_PRODUCT_ZYDAS_ZD1201	0x2100		/* ZD1201 */
#define	PCI_PRODUCT_ZYDAS_ZD1202	0x2102		/* ZD1202 */
#define	PCI_PRODUCT_ZYDAS_ZD1205	0x2105		/* ZD1205 */
