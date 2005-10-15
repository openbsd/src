/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: pcidevs,v 1.942 2005/10/15 11:42:52 brad Exp 
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
#define	PCI_VENDOR_DYNALINK	0x0675		/* Dynalink */
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
#define	PCI_VENDOR_TI	0x104c		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104d		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 */
#define	PCI_VENDOR_UMC	0x1060		/* UMC */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. */
#define	PCI_VENDOR_PICOPOWER	0x1066		/* Picopower */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_APPLE	0x106b		/* Apple */
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
#define	PCI_VENDOR_MOLEX	0x10d2		/* Molex */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* Nvidia */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_TEKRAM	0x10e1		/* Tekram (1st ID) */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
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
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_PHILIPS	0x1131		/* Philips */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_CYCLONE	0x113c		/* Cyclone */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_RCC	0x1166		/* ServerWorks */
#define	PCI_VENDOR_TOSHIBA2	0x1179		/* Toshiba */
#define	PCI_VENDOR_RICOH	0x1180		/* Ricoh */
#define	PCI_VENDOR_DLINK	0x1186		/* D-Link Systems */
#define	PCI_VENDOR_COROLLARY	0x118c		/* Corrollary */
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
#define	PCI_VENDOR_STB2	0x12d2		/* NVidia/SGS-Thomson */
#define	PCI_VENDOR_AUREAL	0x12eb		/* Aureal */
#define	PCI_VENDOR_ADMTEK	0x1317		/* ADMtek */
#define	PCI_VENDOR_PE	0x1318		/* Packet Engines */
#define	PCI_VENDOR_FORTEMEDIA	0x1319		/* Forte Media */
#define	PCI_VENDOR_SIIG	0x131f		/* SIIG */
#define	PCI_VENDOR_DTCTECH	0x134a		/* DTC Tech */
#define	PCI_VENDOR_PCTEL	0x134d		/* PCTEL */
#define	PCI_VENDOR_CNET	0x1371		/* CNet */
#define	PCI_VENDOR_LMC	0x1376		/* LAN Media */
#define	PCI_VENDOR_NETGEAR	0x1385		/* Netgear */
#define	PCI_VENDOR_MOXA	0x1393		/* Moxa */
#define	PCI_VENDOR_LEVEL1	0x1394		/* Level 1 */
#define	PCI_VENDOR_HIFN	0x13a3		/* Hifn */
#define	PCI_VENDOR_TRIWARE	0x13c1		/* 3ware */
#define	PCI_VENDOR_ABOCOM	0x13d1		/* Abocom */
#define	PCI_VENDOR_SUNDANCE	0x13f0		/* Sundance */
#define	PCI_VENDOR_CMI	0x13f6		/* C-Media Electronics */
#define	PCI_VENDOR_LAVA	0x1407		/* Lava */
#define	PCI_VENDOR_SUNIX	0x1409		/* Sunix */
#define	PCI_VENDOR_MICROSOFT	0x1414		/* Microsoft */
#define	PCI_VENDOR_OXFORD2	0x1415		/* Oxford */
#define	PCI_VENDOR_CHELSIO	0x1425		/* Chelsio */
#define	PCI_VENDOR_TAMARACK	0x143d		/* Tamarack */
#define	PCI_VENDOR_ASKEY	0x144f		/* Askey */
#define	PCI_VENDOR_AVERMEDIA	0x1461		/* Avermedia */
#define	PCI_VENDOR_AIRONET	0x14b9		/* Aironet */
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
#define	PCI_VENDOR_NDC	0x15e8		/* National Datacomm */
#define	PCI_VENDOR_ZOLTRIX	0x15b0		/* Zoltrix */
#define	PCI_VENDOR_EUMITCOM	0x1638		/* Eumitcom */
#define	PCI_VENDOR_NETSEC	0x1660		/* NetSec */
#define	PCI_VENDOR_ZYDAS	0x167b		/* ZyDAS Technology */
#define	PCI_VENDOR_SAMSUNG	0x167d		/* Samsung */
#define	PCI_VENDOR_ATHEROS	0x168c		/* Atheros */
#define	PCI_VENDOR_GLOBALSUN	0x16ab		/* Global Sun */
#define	PCI_VENDOR_SAFENET	0x16ae		/* SafeNet */
#define	PCI_VENDOR_USR2	0x16ec		/* US Robotics */
#define	PCI_VENDOR_LINKSYS	0x1737		/* Linksys */
#define	PCI_VENDOR_NETOCTAVE	0x170b		/* Netoctave */
#define	PCI_VENDOR_ALTIMA	0x173b		/* Altima */
#define	PCI_VENDOR_ANTARES	0x1754		/* Antares Microsystems */
#define	PCI_VENDOR_CAVIUM	0x177d		/* Cavium */
#define	PCI_VENDOR_BELKIN2	0x1799		/* Belkin */
#define	PCI_VENDOR_HAWKING	0x17b3		/* Hawking Technology */
#define	PCI_VENDOR_NETERION	0x17d5		/* Neterion */
#define	PCI_VENDOR_INPROCOMM	0x17fe		/* INPROCOMM */
#define	PCI_VENDOR_LANERGY	0x1812		/* Lanergy */
#define	PCI_VENDOR_RALINK	0x1814		/* Ralink */
#define	PCI_VENDOR_SYMPHONY2	0x1c1c		/* Symphony Labs */
#define	PCI_VENDOR_TEKRAM2	0x1de1		/* Tekram */
#define	PCI_VENDOR_HINT	0x3388		/* Hint */
#define	PCI_VENDOR_3DLABS	0x3d3d		/* 3D Labs */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic */
#define	PCI_VENDOR_ADDTRON	0x4033		/* Addtron */
#define	PCI_VENDOR_INDCOMPSRC	0x494f		/* Industrial Computer Source */
#define	PCI_VENDOR_NETVIN	0x4a14		/* NetVin */
#define	PCI_VENDOR_GEMTEK	0x5046		/* Gemtek */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_C4T	0x6374		/* c't Magazin */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_WINBOND2	0x8c4a		/* Winbond */
#define	PCI_VENDOR_KTI	0x8e2e		/* KTI */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ADP2	0x9005		/* Adaptec */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_NETMOS	0x9710		/* NetMos */
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
#define	PCI_PRODUCT_O2MICRO_OZ6729	0x6729		/* OZ6729 */
#define	PCI_PRODUCT_O2MICRO_OZ6730	0x673a		/* OZ6730 */
#define	PCI_PRODUCT_O2MICRO_OZ6922	0x6825		/* OZ6922 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6832	0x6832		/* OZ6832 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6836	0x6836		/* OZ6836/OZ6860 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6872	0x6872		/* OZ68[17]2 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6933	0x6933		/* OZ6933 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ6972	0x6972		/* OZ69[17]2 CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7110	0x7110		/* OZ711Mx CardBus */
#define	PCI_PRODUCT_O2MICRO_OZ7114	0x7114		/* OZ711M1 CardBus */
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
#define	PCI_PRODUCT_3COM_GIG	0x80eb		/* Gig */
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
#define	PCI_PRODUCT_3DFX_VOODOO3	0x0005		/* Voodoo3 */
#define	PCI_PRODUCT_3DFX_VOODOO5	0x0009		/* Voodoo5 */

/* 3D Labs products */
#define	PCI_PRODUCT_3DLABS_300SX	0x0001		/* 300SX */
#define	PCI_PRODUCT_3DLABS_500TX	0x0002		/* 500TX */
#define	PCI_PRODUCT_3DLABS_DELTA	0x0003		/* Delta */
#define	PCI_PRODUCT_3DLABS_PERMEDIA	0x0004		/* Permedia */
#define	PCI_PRODUCT_3DLABS_500MX	0x0006		/* 500MX */
#define	PCI_PRODUCT_3DLABS_PERMEDIA2	0x0007		/* Permedia 2 */
#define	PCI_PRODUCT_3DLABS_OXYGEN_GVX1_CPU	0x0008		/* Oxygen GVX1 */
#define	PCI_PRODUCT_3DLABS_PERMEDIA3	0x0009		/* Permedia 3 */
#define	PCI_PRODUCT_3DLABS_OXYGEN_GVX1	0x000a		/* Oxygen GVX1 */

/* 3Ware products */
#define	PCI_PRODUCT_TRIWARE_ESCALADE	0x1000		/* Escalade IDE RAID */
#define	PCI_PRODUCT_TRIWARE_ESCALADE_ASIC	0x1001		/* Escalade IDE RAID */

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
#define	PCI_PRODUCT_ADDTRON_8139	0x1360		/* rtl8139 */
#define	PCI_PRODUCT_ADDTRON_RHINEII	0x1320		/* RhineII */
#define	PCI_PRODUCT_ADDTRON_AWA100	0x7001		/* AWA-100 */

/* Acer products */
#define	PCI_PRODUCT_ACER_M1435	0x1435		/* M1435 VL-PCI */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 VL-PCI */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 ISA */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 PCI */
#define	PCI_PRODUCT_ALI_M1461	0x1461		/* M1461 PCI */
#define	PCI_PRODUCT_ALI_M1489	0x1489		/* M1489 PCI */
#define	PCI_PRODUCT_ALI_M1521	0x1521		/* M1523 PCI */
#define	PCI_PRODUCT_ALI_M1523	0x1523		/* M1523 ISA */
#define	PCI_PRODUCT_ALI_M1531	0x1531		/* M1531 PCI */
#define	PCI_PRODUCT_ALI_M1533	0x1533		/* M1533 ISA */
#define	PCI_PRODUCT_ALI_M1535	0x1535		/* M1535 PCI */
#define	PCI_PRODUCT_ALI_M1543	0x1543		/* M1543 ISA */
#define	PCI_PRODUCT_ALI_M1541	0x1541		/* M1541 PCI */
#define	PCI_PRODUCT_ALI_M1621	0x1621		/* M1621 PCI */
#define	PCI_PRODUCT_ALI_M1631	0x1631		/* M1631 PCI */
#define	PCI_PRODUCT_ALI_M1644	0x1644		/* M1644 PCI */
#define	PCI_PRODUCT_ALI_M1647	0x1647		/* M1647 PCI */
#define	PCI_PRODUCT_ALI_M3309	0x3309		/* M3309 MPEG */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */
#define	PCI_PRODUCT_ALI_M5219	0x5219		/* M5219 UDMA IDE */
#define	PCI_PRODUCT_ALI_M5229	0x5229		/* M5229 UDMA IDE */
#define	PCI_PRODUCT_ALI_M5237	0x5237		/* M5237 USB */
#define	PCI_PRODUCT_ALI_M5247	0x5247		/* M5247 AGP/PCI-PCI */
#define	PCI_PRODUCT_ALI_M5243	0x5243		/* M5243 AGP/PCI-PCI */
#define	PCI_PRODUCT_ALI_M5451	0x5451		/* M5451 Audio */
#define	PCI_PRODUCT_ALI_M5457	0x5457		/* M5457 Modem */
#define	PCI_PRODUCT_ALI_M7101	0x7101		/* M7101 Power */

/* ADMtek products */
#define	PCI_PRODUCT_ADMTEK_AL981	0x0981		/* AL981 */
#define	PCI_PRODUCT_ADMTEK_AN983	0x0985		/* AN983 */
#define	PCI_PRODUCT_ADMTEK_AN985	0x1985		/* AN985 */
#define	PCI_PRODUCT_ADMTEK_ADM8211	0x8201		/* ADM8211 WLAN */

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

/* Alliance products */
#define	PCI_PRODUCT_ALLIANCE_AT22	0x6422		/* AT22 */
#define	PCI_PRODUCT_ALLIANCE_AT24	0x6424		/* AT24 */

/* Alteon products */
#define	PCI_PRODUCT_ALTEON_ACENIC	0x0001		/* Acenic */
#define	PCI_PRODUCT_ALTEON_ACENICT	0x0002		/* Acenic Copper */
#define	PCI_PRODUCT_ALTEON_BCM5700	0x0003		/* BCM5700 */
#define	PCI_PRODUCT_ALTEON_BCM5701	0x0004		/* BCM5701 */

/* Altima products */
#define	PCI_PRODUCT_ALTIMA_AC1000	0x03e8		/* AC1000 */
#define	PCI_PRODUCT_ALTIMA_AC1001	0x03e9		/* AC1001 */
#define	PCI_PRODUCT_ALTIMA_AC9100	0x03ea		/* AC9100 */

/* AMD products */
#define	PCI_PRODUCT_AMD_AMD64_HT	0x1100		/* AMD64 HyperTransport */
#define	PCI_PRODUCT_AMD_AMD64_ADDR	0x1101		/* AMD64 Address Map */
#define	PCI_PRODUCT_AMD_AMD64_DRAM	0x1102		/* AMD64 DRAM Cfg */
#define	PCI_PRODUCT_AMD_AMD64_MISC	0x1103		/* AMD64 Misc Cfg */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* 79c970 PCnet-PCI */
#define	PCI_PRODUCT_AMD_PCHOME_PCI	0x2001		/* 79c978 PChome-PCI */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* 53c974 PCscsi-PCI */
#define	PCI_PRODUCT_AMD_PCNETS_PCI	0x2040		/* 79C974 PCnet-PCI */
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
#define	PCI_PRODUCT_AMD_8111_USB	0x7464		/* 8111 USB */
#define	PCI_PRODUCT_AMD_PBC8111_LPC	0x7468		/* AMD8111 LPC */
#define	PCI_PRODUCT_AMD_8111_IDE	0x7469		/* 8111 IDE */
#define	PCI_PRODUCT_AMD_8111_SMB	0x746a		/* 8111 SMBus */
#define	PCI_PRODUCT_AMD_8111_ACPI	0x746b		/* 8111 ACPI */
#define	PCI_PRODUCT_AMD_8111_ACA	0x746d		/* 8111 AC97 */

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

/* ASIX Electronics products */
#define	PCI_PRODUCT_ASIX_AX88140A	0x1400		/* AX88140A/88141 */

/* Asustek products */
#define	PCI_PRODUCT_ASUSTEK_HFCPCI	0x0675		/* ISDN */

/* ATI Technologies */
#define	PCI_PRODUCT_ATI_RADEON_M241P	0x3150		/* Radeon Mobility X600 */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV380	0x3e50		/* Radeon X600 (RV380) */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV380_S	0x3e70		/* Radeon X600 (RV380) Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600PRO	0x4150		/* Radeon 9600 Pro */
#define	PCI_PRODUCT_ATI_RADEON_9600LE	0x4151		/* Radeon 9600 LE */
#define	PCI_PRODUCT_ATI_RADEON_9600XT	0x4152		/* Radeon 9600 XT */
#define	PCI_PRODUCT_ATI_RADEON_9550	0x4153		/* Radeon 9550 */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_RADEON_9600PRO_S	0x4170		/* Radeon 9600 Pro Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600LE_S	0x4171		/* Radeon 9600 LE Sec */
#define	PCI_PRODUCT_ATI_RADEON_9600XT_S	0x4172		/* Radeon 9600 XT Sec */
#define	PCI_PRODUCT_ATI_RADEON_9550_S	0x4173		/* Radeon 9550 Sec */
#define	PCI_PRODUCT_ATI_R200_BB	0x4242		/* Radeon 8500 BB */
#define	PCI_PRODUCT_ATI_RADEON_IGP320M	0x4336		/* Radeon IGP 320M */
#define	PCI_PRODUCT_ATI_MOBILITY_M6	0x4337		/* Mobility M6 */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_200	0x4341		/* IXP200 AC97 */
#define	PCI_PRODUCT_ATI_SB200_PPB	0x4342		/* SB200 PCI-PCI */
#define	PCI_PRODUCT_ATI_SB200_EHCI	0x4345		/* SB200 USB2 */
#define	PCI_PRODUCT_ATI_SB200_OHCI_1	0x4347		/* SB200 USB */
#define	PCI_PRODUCT_ATI_SB200_OHCI_2	0x4348		/* SB200 USB */
#define	PCI_PRODUCT_ATI_SB200_ISA	0x434c		/* SB200 PCI-ISA */
#define	PCI_PRODUCT_ATI_SB200_SMB	0x4353		/* SB200 SMBus */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_300	0x4361		/* IXP300 AC97 */
#define	PCI_PRODUCT_ATI_IXP_AUDIO_400	0x4370		/* IXP400 AC97 */
#define	PCI_PRODUCT_ATI_IXP_PCI_400	0x4371		/* IXP400 PCI */
#define	PCI_PRODUCT_ATI_IXP_SMBUS_400	0x4372		/* IXP400 SMBus */
#define	PCI_PRODUCT_ATI_IXP_EHCI_400	0x4373		/* IXP400 USB2 */
#define	PCI_PRODUCT_ATI_IXP_OHCI_400_1	0x4374		/* IXP400 USB */
#define	PCI_PRODUCT_ATI_IXP_OHCI_400_2	0x4375		/* IXP400 USB */
#define	PCI_PRODUCT_ATI_IXP_ISA_400	0x4377		/* IXP400 ISA */
#define	PCI_PRODUCT_ATI_IXP_MODEM_400	0x4378		/* IXP400 Modem */
#define	PCI_PRODUCT_ATI_IXP_SATA_400_1	0x4379		/* IXP400 SATA */
#define	PCI_PRODUCT_ATI_IXP_SATA_400_2	0x437a		/* IXP400 SATA */
#define	PCI_PRODUCT_ATI_IXP_IDE_200	0x4349		/* IXP200 IDE */
#define	PCI_PRODUCT_ATI_IXP_IDE_300	0x4369		/* IXP300 IDE */
#define	PCI_PRODUCT_ATI_IXP_IDE_400	0x4376		/* IXP400 IDE */
#define	PCI_PRODUCT_ATI_MACH64_ET	0x4554		/* Mach64 ET */
#define	PCI_PRODUCT_ATI_RAGEPRO	0x4742		/* Rage Pro */
#define	PCI_PRODUCT_ATI_MACH64_GD	0x4744		/* Mach64 GD */
#define	PCI_PRODUCT_ATI_MACH64_GI	0x4749		/* Mach64 GI */
#define	PCI_PRODUCT_ATI_MACH64_GL	0x474C		/* Mach64 GL */
#define	PCI_PRODUCT_ATI_MACH64_GM	0x474d		/* Mach64 GM */
#define	PCI_PRODUCT_ATI_MACH64_GN	0x474e		/* Mach64 GN */
#define	PCI_PRODUCT_ATI_MACH64_GO	0x474f		/* Mach64 GO */
#define	PCI_PRODUCT_ATI_MACH64_GP	0x4750		/* Mach64 GP */
#define	PCI_PRODUCT_ATI_MACH64_GQ	0x4751		/* Mach64 GQ */
#define	PCI_PRODUCT_ATI_RAGEXL	0x4752		/* Rage XL */
#define	PCI_PRODUCT_ATI_MACH64_GS	0x4753		/* Mach64 GS */
#define	PCI_PRODUCT_ATI_MACH64_GT	0x4754		/* Mach64 GT */
#define	PCI_PRODUCT_ATI_MACH64_GU	0x4755		/* Mach64 GU */
#define	PCI_PRODUCT_ATI_MACH64_GV	0x4756		/* Mach64 GV */
#define	PCI_PRODUCT_ATI_MACH64_GW	0x4757		/* Mach64 GW */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 GX */
#define	PCI_PRODUCT_ATI_MACH64_GY	0x4759		/* Mach64 GY */
#define	PCI_PRODUCT_ATI_MACH64_GZ	0x475a		/* Mach64 GZ */
#define	PCI_PRODUCT_ATI_RV250	0x4966		/* Radeon 9000 */
#define	PCI_PRODUCT_ATI_RV250_S	0x496e		/* Radeon 9000 Sec */
#define	PCI_PRODUCT_ATI_MACH64_LB	0x4c42		/* Mach64 LB */
#define	PCI_PRODUCT_ATI_MACH64_LD	0x4c44		/* Mach64 LD */
#define	PCI_PRODUCT_ATI_RAGE128_LE	0x4c45		/* Rage128 LE */
#define	PCI_PRODUCT_ATI_MOBILITY_M3	0x4c46		/* Mobility M3 */
#define	PCI_PRODUCT_ATI_MACH64_LG	0x4c47		/* Mach64 LG */
#define	PCI_PRODUCT_ATI_MACH64_LI	0x4c49		/* Mach64 LI */
#define	PCI_PRODUCT_ATI_MOBILITY_1	0x4c4d		/* Mobility 1 */
#define	PCI_PRODUCT_ATI_MACH64_LN	0x4c4e		/* Mach64 LN */
#define	PCI_PRODUCT_ATI_MACH64_LP	0x4c50		/* Mach64 LP */
#define	PCI_PRODUCT_ATI_MACH64_LQ	0x4c51		/* Mach64 LQ */
#define	PCI_PRODUCT_ATI_RAGE_PM	0x4c52		/* Rage P/M */
#define	PCI_PRODUCT_ATI_MACH64LS	0x4c53		/* Mach64 LS */
#define	PCI_PRODUCT_ATI_RADEON_M7LW	0x4c57		/* Radeon Mobility M7 LW */
#define	PCI_PRODUCT_ATI_RADEON_M6LY	0x4c59		/* Radeon Mobility M6 LY */
#define	PCI_PRODUCT_ATI_RADEON_M6LZ	0x4c5a		/* Radeon Mobility M6 LZ */
#define	PCI_PRODUCT_ATI_RADEON_M9Lf	0x4c66		/* Radeon Mobility M9 Lf */
#define	PCI_PRODUCT_ATI_RAGE128_MF	0x4d46		/* Rage 128 Mobility MF */
#define	PCI_PRODUCT_ATI_RAGE128_ML	0x4d4c		/* Rage 128 Mobility ML */
#define	PCI_PRODUCT_ATI_R300	0x4e44		/* Radeon 9500/9700 */
#define	PCI_PRODUCT_ATI_RADEON_9800XT	0x4e4a		/* Radeon 9800 XT */
#define	PCI_PRODUCT_ATI_R350	0x4e48		/* Radeon 9800 Pro */
#define	PCI_PRODUCT_ATI_RV350	0x4e50		/* Radeon Mobility M10 NP */
#define	PCI_PRODUCT_ATI_R300_S	0x4e64		/* Radeon 9500/9700 Sec */
#define	PCI_PRODUCT_ATI_R350_S	0x4e68		/* Radeon 9800 Pro Sec */
#define	PCI_PRODUCT_ATI_RAGE128_PD	0x5044		/* Rage 128 Pro PD */
#define	PCI_PRODUCT_ATI_RAGE_FURY	0x5046		/* Rage Fury */
#define	PCI_PRODUCT_ATI_RAGE128_PK	0x5052		/* Rage 128 PK */
#define	PCI_PRODUCT_ATI_RADEON_AIW	0x5144		/* AIW Radeon */
#define	PCI_PRODUCT_ATI_RADEON_QE	0x5145		/* Radeon QE */
#define	PCI_PRODUCT_ATI_RADEON_QF	0x5146		/* Radeon QF */
#define	PCI_PRODUCT_ATI_RADEON_QG	0x5147		/* Radeon QG */
#define	PCI_PRODUCT_ATI_R200_QL	0x514C		/* Radeon 8500 QL */
#define	PCI_PRODUCT_ATI_R200_QM	0x514D		/* Radeon 9100 QM */
#define	PCI_PRODUCT_ATI_R200_QN	0x514E		/* Radeon 8500 QN */
#define	PCI_PRODUCT_ATI_R200_QO	0x514F		/* Radeon 8500 QO */
#define	PCI_PRODUCT_ATI_RV200_QW	0x5157		/* Radeon 7500 QW */
#define	PCI_PRODUCT_ATI_RADEON_QY	0x5159		/* Radeon VE QY */
#define	PCI_PRODUCT_ATI_RADEON_QZ	0x515A		/* Radeon VE QZ */
#define	PCI_PRODUCT_ATI_R200_Ql	0x516C		/* Radeon 8500 Ql */
#define	PCI_PRODUCT_ATI_RAGE128_GL	0x5245		/* Rage 128 GL */
#define	PCI_PRODUCT_ATI_RAGE_MAGNUM	0x5246		/* Rage Magnum */
#define	PCI_PRODUCT_ATI_RAGE128_RG	0x5247		/* Rage 128 RG */
#define	PCI_PRODUCT_ATI_RAGE128_RK	0x524b		/* Rage 128 RK */
#define	PCI_PRODUCT_ATI_RAGE128_VR	0x524c		/* Rage 128 VR */
#define	PCI_PRODUCT_ATI_RAGE128_SH	0x5348		/* Rage 128 SH */
#define	PCI_PRODUCT_ATI_RAGE128_SK	0x534b		/* Rage 128 SK */
#define	PCI_PRODUCT_ATI_RAGE128_SL	0x534c		/* Rage 128 SL */
#define	PCI_PRODUCT_ATI_RAGE128_SM	0x534d		/* Rage 128 SM */
#define	PCI_PRODUCT_ATI_RAGE128	0x534e		/* Rage 128 */
#define	PCI_PRODUCT_ATI_RAGE128_TF	0x5446		/* Rage 128 Pro TF */
#define	PCI_PRODUCT_ATI_RAGE128_TL	0x544c		/* Rage 128 Pro TL */
#define	PCI_PRODUCT_ATI_RAGE128_TR	0x5452		/* Rage 128 Pro TR */
#define	PCI_PRODUCT_ATI_RADEON_M300_M22	0x5460		/* Radeon Mobility M300 M22 */
#define	PCI_PRODUCT_ATI_RADEON_X700_M26_1	0x5652		/* Radeon Mobility X700 M26 */
#define	PCI_PRODUCT_ATI_RADEON_X700_M26_2	0x5653		/* Radeon Mobility X700 M26 */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x5654		/* Mach64 VT */
#define	PCI_PRODUCT_ATI_MACH64_VU	0x5655		/* Mach64 VU */
#define	PCI_PRODUCT_ATI_MACH64_VV	0x5656		/* Mach64 VV */
#define	PCI_PRODUCT_ATI_RS300_HB	0x5831		/* RS300 Host */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100_HB	0x5833		/* Radeon IGP 9100 Host */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100	0x5835		/* Radeon Mobility IGP 9100 */
#define	PCI_PRODUCT_ATI_RADEON_IGP9100_AGP	0x5838		/* Radeon IGP 9100 AGP */
#define	PCI_PRODUCT_ATI_RADEON_RV280_PRO_S	0x5940		/* Radeon 9200 PRO Sec */
#define	PCI_PRODUCT_ATI_RADEON_RV280_S	0x5941		/* Radeon 9200 Sec */
#define	PCI_PRODUCT_ATI_RADEON_RS480_HB	0x5950		/* RS480 Host */
#define	PCI_PRODUCT_ATI_RADEON_RS480	0x5954		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS480_B	0x5955		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_RV280_PRO	0x5960		/* Radeon 9200 PRO */
#define	PCI_PRODUCT_ATI_RADEON_RV280	0x5961		/* Radeon 9200 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_B	0x5962		/* Radeon 9200 */
#define	PCI_PRODUCT_ATI_RADEON_RV280_SE_S	0x5964		/* Radeon 9200 SE Sec */
#define	PCI_PRODUCT_ATI_RADEON_RS482	0x5974		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS482_B	0x5975		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_RS480_PCIE	0x5a3f		/* RS480 PCIE */
#define	PCI_PRODUCT_ATI_RADEON_RS400	0x5a41		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RS400_B	0x5a42		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_RC410	0x5a61		/* Radeon XPRESS 200 */
#define	PCI_PRODUCT_ATI_RADEON_RC410_B	0x5a62		/* Radeon XPRESS 200M */
#define	PCI_PRODUCT_ATI_RADEON_X300	0x5b60		/* Radeon X300 */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV370	0x5b62		/* Radeon X600 (RV370) */
#define	PCI_PRODUCT_ATI_FIREGL_RV370	0x5b64		/* FireGL V3100 */
#define	PCI_PRODUCT_ATI_RADEON_X300_S	0x5b70		/* Radeon X300 Sec */
#define	PCI_PRODUCT_ATI_RADEON_X600_RV370_S	0x5b72		/* Radeon X600 (RV370) Sec */
#define	PCI_PRODUCT_ATI_FIREGL_RV370_S	0x5b74		/* FireGL V3100 Sec */
#define	PCI_PRODUCT_ATI_RADEON_RV280_M	0x5c61		/* Radeon Mobility 9200 */
#define	PCI_PRODUCT_ATI_RADEON_M9PLUS	0x5c63		/* Radeon Mobility 9200 (M9+) */
#define	PCI_PRODUCT_ATI_RADEON_RV280_SE	0x5d44		/* Radeon 9200 SE */
#define	PCI_PRODUCT_ATI_RS100_PCI	0x700f		/* RS100 PCI */
#define	PCI_PRODUCT_ATI_RS200_PCI	0x7010		/* RS200 PCI */
#define	PCI_PRODUCT_ATI_RS100_AGP	0xcab0		/* RS100 AGP */
#define	PCI_PRODUCT_ATI_RS200_AGP	0xcab2		/* RS200 AGP */

/* Applied Micro Circuits products */
#define	PCI_PRODUCT_AMCIRCUITS_S5933	0x4750		/* S5933 PCI Matchmaker */
#define	PCI_PRODUCT_AMCIRCUITS_LANAI	0x8043		/* Myrinet LANai */

/* Atheros products */
#define	PCI_PRODUCT_ATHEROS_AR5210	0x0007		/* AR5210 */
#define	PCI_PRODUCT_ATHEROS_AR5311	0x0011		/* AR5211 */
#define	PCI_PRODUCT_ATHEROS_AR5211	0x0012		/* AR5211 */
#define	PCI_PRODUCT_ATHEROS_AR5212	0x0013		/* AR5212 */
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
#define	PCI_PRODUCT_AVLAB_LPPCI4S	0x2150		/* LP-PCI 4 Serial */

/* AVM products */
#define	PCI_PRODUCT_AVM_FRITZ_CARD	0x0a00		/* Fritz ISDN */
#define	PCI_PRODUCT_AVM_FRITZ_PCI_V2_ISDN	0x0e00		/* Fritz v2.0 ISDN */
#define	PCI_PRODUCT_AVM_B1	0x0700		/* BRI ISDN */
#define	PCI_PRODUCT_AVM_T1	0x1200		/* PRI T1 ISDN */

/* Belkin Components products */
#define	PCI_PRODUCT_BELKIN2_F5D6001	0x6001		/* F5D6001 */
#define	PCI_PRODUCT_BELKIN2_F5D6020V3	0x6020		/* F5D6020V3 */
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
#define	PCI_PRODUCT_BROADCOM_BCM5700	0x1644		/* BCM5700 */
#define	PCI_PRODUCT_BROADCOM_BCM5701	0x1645		/* BCM5701 */
#define	PCI_PRODUCT_BROADCOM_BCM5702	0x1646		/* BCM5702 */
#define	PCI_PRODUCT_BROADCOM_BCM5703	0x1647		/* BCM5703 */
#define	PCI_PRODUCT_BROADCOM_BCM5704C	0x1648		/* BCM5704C */
#define	PCI_PRODUCT_BROADCOM_BCM5704S_ALT	0x1649		/* BCM5704S_ALT */
#define	PCI_PRODUCT_BROADCOM_BCM5702FE	0x164d		/* BCM5702FE */
#define	PCI_PRODUCT_BROADCOM_BCM5705	0x1653		/* BCM5705 */
#define	PCI_PRODUCT_BROADCOM_BCM5705K	0x1654		/* BCM5705K */
#define	PCI_PRODUCT_BROADCOM_BCM5720	0x1658		/* BCM5720 */
#define	PCI_PRODUCT_BROADCOM_BCM5721	0x1659		/* BCM5721 */
#define	PCI_PRODUCT_BROADCOM_BCM5705M	0x165d		/* BCM5705M */
#define	PCI_PRODUCT_BROADCOM_BCM5705M_ALT	0x165e		/* BCM5705M_ALT */
#define	PCI_PRODUCT_BROADCOM_BCM5714C	0x1668		/* BCM5714C */
#define	PCI_PRODUCT_BROADCOM_BCM5780	0x166a		/* BCM5780 */
#define	PCI_PRODUCT_BROADCOM_BCM5780S	0x166b		/* BCM5780S */
#define	PCI_PRODUCT_BROADCOM_BCM5705F	0x166e		/* BCM5705F */
#define	PCI_PRODUCT_BROADCOM_BCM5750	0x1676		/* BCM5750 */
#define	PCI_PRODUCT_BROADCOM_BCM5751	0x1677		/* BCM5751 */
#define	PCI_PRODUCT_BROADCOM_BCM5750M	0x167c		/* BCM5750M */
#define	PCI_PRODUCT_BROADCOM_BCM5751M	0x167d		/* BCM5751M */
#define	PCI_PRODUCT_BROADCOM_BCM5751F	0x167e		/* BCM5751F */
#define	PCI_PRODUCT_BROADCOM_BCM5782	0x1696		/* BCM5782 */
#define	PCI_PRODUCT_BROADCOM_BCM5788	0x169c		/* BCM5788 */
#define	PCI_PRODUCT_BROADCOM_BCM5789	0x169d		/* BCM5789 */
#define	PCI_PRODUCT_BROADCOM_BCM5702X	0x16a6		/* BCM5702X */
#define	PCI_PRODUCT_BROADCOM_BCM5703X	0x16a7		/* BCM5703X */
#define	PCI_PRODUCT_BROADCOM_BCM5704S	0x16a8		/* BCM5704S */
#define	PCI_PRODUCT_BROADCOM_BCM5702_ALT	0x16c6		/* BCM5702_ALT */
#define	PCI_PRODUCT_BROADCOM_BCM5703_ALT	0x16c7		/* BCM5703_ALT */
#define	PCI_PRODUCT_BROADCOM_BCM5781	0x16dd		/* BCM5781 */
#define	PCI_PRODUCT_BROADCOM_BCM5753	0x16f7		/* BCM5753 */
#define	PCI_PRODUCT_BROADCOM_BCM5753M	0x16fd		/* BCM5753M */
#define	PCI_PRODUCT_BROADCOM_BCM5753F	0x16fe		/* BCM5753F */
#define	PCI_PRODUCT_BROADCOM_BCM4401B0	0x170c		/* BCM4401B0 */
#define	PCI_PRODUCT_BROADCOM_BCM5901	0x170d		/* BCM5901 */
#define	PCI_PRODUCT_BROADCOM_BCM5901A2	0x170e		/* BCM5901A2 */
#define	PCI_PRODUCT_BROADCOM_BCM4318	0x4318		/* BCM4318 */
#define	PCI_PRODUCT_BROADCOM_BCM4306	0x4320		/* BCM4306 */
#define	PCI_PRODUCT_BROADCOM_BCM4322	0x4322		/* BCM4322 */
#define	PCI_PRODUCT_BROADCOM_BCM4309	0x4324		/* BCM4309 */
#define	PCI_PRODUCT_BROADCOM_SERIAL	0x4333		/* Serial */
#define	PCI_PRODUCT_BROADCOM_BCM4401	0x4401		/* BCM4401 */
#define	PCI_PRODUCT_BROADCOM_5801	0x5801		/* 5801 */
#define	PCI_PRODUCT_BROADCOM_5802	0x5802		/* 5802 */
#define	PCI_PRODUCT_BROADCOM_5805	0x5805		/* 5805 */
#define	PCI_PRODUCT_BROADCOM_5820	0x5820		/* 5820 */
#define	PCI_PRODUCT_BROADCOM_5821	0x5821		/* 5821 */
#define	PCI_PRODUCT_BROADCOM_5822	0x5822		/* 5822 */
#define	PCI_PRODUCT_BROADCOM_5823	0x5823		/* 5823 */

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
#define	PCI_PRODUCT_CAVIUM_NITROX	0x0001		/* Nitrox XL */

/* CCUBE products */
#define	PCI_PRODUCT_CCUBE_CINEMASTER	0x8888		/* Cinemaster */

/* Chelsio products */
#define	PCI_PRODUCT_CHELSIO_Nx10	0x0006		/* Nx10 10GbE */

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
#define	PCI_PRODUCT_CMDTECH_3512	0x3512		/* SiI3512 SATA */
/* Fake product id for SiI3112 found on Adaptec 1210SA */
#define	PCI_PRODUCT_CMDTECH_AAR_1210SA	0x0240		/* AAR-1210SA */

/* CNet produts */
#define	PCI_PRODUCT_CNET_GIGACARD	0x434e		/* GigaCard */

/* Cogent Data Technologies products */
#define	PCI_PRODUCT_COGENT_EM110TX	0x1400		/* EX110TX */

/* Compaq products */
#define	PCI_PRODUCT_COMPAQ_PCI_EISA_BRIDGE	0x0001		/* EISA */
#define	PCI_PRODUCT_COMPAQ_PCI_ISA_BRIDGE	0x0002		/* ISA */
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
#define	PCI_PRODUCT_COMPAQ_CSA641	0x409a		/* Smart Array 641 */
#define	PCI_PRODUCT_COMPAQ_CSA642	0x409b		/* Smart Array 642 */
#define	PCI_PRODUCT_COMPAQ_CSA6400	0x409c		/* Smart Array 6400 */
#define	PCI_PRODUCT_COMPAQ_CSA6400EM	0x409d		/* Smart Array 6400 EM */
#define	PCI_PRODUCT_COMPAQ_CSA6422	0x409e		/* Smart Array 6422 */
#define	PCI_PRODUCT_COMPAQ_CSA64XX	0x0046		/* Smart Array 64xx */
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

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 VLB */
#define	PCI_PRODUCT_CONTAQ_82C693	0xc693		/* CY82C693U ISA */

/* Corega products */
#define	PCI_PRODUCT_COREGA_CB_TXD	0xa117		/* FEther CB-TXD 10/100 */
#define	PCI_PRODUCT_COREGA_2CB_TXD	0xa11e		/* FEther II CB-TXD 10/100 */
#define	PCI_PRODUCT_COREGA_CGLAPCIGT	0xc107		/* CG-LAPCIGT */

/* Corollary Products */
#define	PCI_PRODUCT_COROLLARY_CBUSII_PCIB	0x0014		/* C-Bus II-PCI */

/* Creative Labs */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE	0x0002		/* SoundBlaster Live */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGY	0x0004		/* SoundBlaster Audigy */
#define	PCI_PRODUCT_CREATIVELABS_SBLIVE2	0x0006		/* SoundBlaster Live */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGYLS	0x0007		/* SoundBlaster Audigy LS */
#define	PCI_PRODUCT_CREATIVELABS_FIWIRE	0x4001		/* Firewire */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY	0x7002		/* PCI Gameport Joystick */
#define	PCI_PRODUCT_CREATIVELABS_AUDIGIN	0x7003		/* SoundBlaster Audigy Digital */
#define	PCI_PRODUCT_CREATIVELABS_SBJOY2	0x7004		/* PCI Gameport Joystick */
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
#define	PCI_PRODUCT_CYRIX_GXMPCI	0x0001		/* GXm PCI */
#define	PCI_PRODUCT_CYRIX_GXMISA	0x0002		/* GXm ISA */
#define	PCI_PRODUCT_CYRIX_CX5530_PCIB	0x0100		/* Cx5530 South */
#define	PCI_PRODUCT_CYRIX_CX5530_SMI	0x0101		/* Cx5530 SMI/ACPI */
#define	PCI_PRODUCT_CYRIX_CX5530_IDE	0x0102		/* Cx5530 IDE */
#define	PCI_PRODUCT_CYRIX_CX5530_AUDIO	0x0103		/* Cx5530 XpressAUDIO */
#define	PCI_PRODUCT_CYRIX_CX5530_VIDEO	0x0104		/* Cx5530 Video */

/* Davicom Technologies */
#define	PCI_PRODUCT_DAVICOM_DM9100	0x9100		/* DM9100 */
#define	PCI_PRODUCT_DAVICOM_DM9102	0x9102		/* DM9102 */
#define	PCI_PRODUCT_DAVICOM_DM9009	0x9009		/* DM9009 */

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
#define	PCI_PRODUCT_DEC_21142	0x0019		/* 21142/3 */
/* Farallon apparently used DEC's vendor ID by mistake */
#define	PCI_PRODUCT_DEC_PN9000SX	0x001a		/* Farallon PN9000SX */
#define	PCI_PRODUCT_DEC_21052	0x0021		/* 21052 PCI-PCI */
#define	PCI_PRODUCT_DEC_21150	0x0022		/* 21150 PCI-PCI */
#define	PCI_PRODUCT_DEC_21150_BC	0x0023		/* 21150-BC PCI-PCI */
#define	PCI_PRODUCT_DEC_21152	0x0024		/* 21152 PCI-PCI */
#define	PCI_PRODUCT_DEC_21153	0x0025		/* 21153 PCI-PCI */
#define	PCI_PRODUCT_DEC_21154	0x0026		/* 21154 PCI-PCI */
#define	PCI_PRODUCT_DEC_CPQ42XX	0x0046		/* Compaq SMART RAID 42xx */
#define	PCI_PRODUCT_DEC_SWXCR	0x1065		/* SWXCR RAID */

/* Dell Computer products */
#define	PCI_PRODUCT_DELL_PERC_2SI	0x0001		/* PERC 2/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI	0x0002		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI	0x0003		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3SI_2	0x0004		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_2	0x0008		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3	0x000a		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_4DI	0x000e		/* PERC 4/Di i960 */
#define	PCI_PRODUCT_DELL_PERC_4DI_2	0x000f		/* PERC 4/Di Verde */
#define	PCI_PRODUCT_DELL_PERC_4EDI	0x0013		/* PERC 4e/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_2_SUB	0x00cf		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3SI_2_SUB	0x00d0		/* PERC 3/Si */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB2	0x00d1		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_SUB3	0x00d9		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB	0x0106		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB2	0x011b		/* PERC 3/Di */
#define	PCI_PRODUCT_DELL_PERC_3DI_3_SUB3	0x0121		/* PERC 3/Di */

/* Delta Electronics products */
#define	PCI_PRODUCT_DELTA_8139	0x1360		/* 8139 */
#define	PCI_PRODUCT_DELTA_RHINEII	0x1320		/* RhineII */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_VIPER	0x9001		/* Viper/PCI */

/* D-Link products */
#define	PCI_PRODUCT_DLINK_550TX	0x1002		/* 550TX */
#define	PCI_PRODUCT_DLINK_530TXPLUS	0x1300		/* 530TX+ */
#define	PCI_PRODUCT_DLINK_DFE690TXD	0x1340		/* DFE-690TXD */
#define	PCI_PRODUCT_DLINK_DWL610	0x3300		/* DWL-610 */
#define	PCI_PRODUCT_DLINK_DGE550T	0x4000		/* DGE-550T */
#define	PCI_PRODUCT_DLINK_DGE528T	0x4300		/* DGE-528T */
#define	PCI_PRODUCT_DLINK_DGE560T	0x4b00		/* DGE-560T */
#define	PCI_PRODUCT_DLINK_DGE560T_2	0x4b01		/* DGE-560T_2 */
#define	PCI_PRODUCT_DLINK_DGE530T	0x4c00		/* DGE-530T */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/Raid */
#define	PCI_PRODUCT_DPT_I960_PPB	0xa500		/* PCI-PCI */
#define	PCI_PRODUCT_DPT_RAID_I2O	0xa501		/* SmartRAID (I2O) */
#define	PCI_PRODUCT_DPT_2005S	0xa511		/* SmartRAID 2005S */
#define	PCI_PRODUCT_DPT_MEMCTLR	0x1012		/* Memory Control */

/* Dolphin products */
#define	PCI_PRODUCT_DOLPHIN_PCISCI	0x0658		/* PCI-SCI */

/* DTC Technology Corp products */
#define	PCI_PRODUCT_DTCTECH_DMX3194U	0x0002		/* DMX3194U */

/* Dynalink products */
#define	PCI_PRODUCT_DYNALINK_IS64PH	0x1702		/* IS64PH ISDN */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_EFSS25	0x0005		/* 25SS-3010 ATM (ASIC) */
#define	PCI_PRODUCT_EFFICIENTNETS_SS1023	0x1023		/* SpeedStream 1023 */

/* ELSA products */
#define	PCI_PRODUCT_ELSA_QS1PCI	0x1000		/* QuickStep 1000 ISDN */

/* Emulex products */
#define	PCI_PRODUCT_EMULEX_LPPFC	0x10df		/* Light Pulse FibreChannel */
#define	PCI_PRODUCT_EMULEX_LP8000	0xf800		/* Light Pulse 8000 */

/* Endace Measurement Systems */
#define	PCI_PRODUCT_ENDACE_DAG35	0x3500		/* Endace Dag3.5 */
#define	PCI_PRODUCT_ENDACE_DAG36D	0x360d		/* Endace Dag3.6D */
#define	PCI_PRODUCT_ENDACE_DAG422GE	0x422e		/* Endace Dag4.22GE */
#define	PCI_PRODUCT_ENDACE_DAG423	0x4230		/* Endace Dag4.23 */
#define	PCI_PRODUCT_ENDACE_DAG423GE	0x423e		/* Endace Dag4.23GE */

/* ENE Technology products */
#define	PCI_PRODUCT_ENE_CB1410	0x1410		/* CB-1410 CardBus */

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

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Forte Media products */
#define	PCI_PRODUCT_FORTEMEDIA_FM801	0x0801		/* 801 Sound */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* Global Sun Technology products */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P03	0x1100		/* GL24110P03 */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P	0x1101		/* GL24110P */
#define	PCI_PRODUCT_GLOBALSUN_GL24110P02	0x1102		/* GL24110P02 */

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
#define	PCI_PRODUCT_HP_HPSAP800	0x3223		/* Smart Array P800 */
#define	PCI_PRODUCT_HP_HPSAP600	0x3225		/* Smart Array P600 */
#define	PCI_PRODUCT_HP_HPSAE400	0x3231		/* Smart Array E400 */

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

/* Hint */
#define	PCI_PRODUCT_HINT_VXPRO_II_HOST	0x8011		/* Host */
#define	PCI_PRODUCT_HINT_VXPRO_II_ISA	0x8012		/* ISA */
#define	PCI_PRODUCT_HINT_VXPRO_II_EIDE	0x8013		/* EIDE */

/* IBM products */
#define	PCI_PRODUCT_IBM_0x0002	0x0002		/* MCA */
#define	PCI_PRODUCT_IBM_0x0005	0x0005		/* CPU - Alta Lite */
#define	PCI_PRODUCT_IBM_0x0007	0x0007		/* CPU - Alta MP */
#define	PCI_PRODUCT_IBM_0x000a	0x000a		/* ISA w/PnP */
#define	PCI_PRODUCT_IBM_0x0017	0x0017		/* CPU */
#define	PCI_PRODUCT_IBM_0x0018	0x0018		/* Auto LANStreamer */
#define	PCI_PRODUCT_IBM_GXT150P	0x001b		/* GXT-150P */
#define	PCI_PRODUCT_IBM_82G2675	0x001d		/* 82G2675 */
#define	PCI_PRODUCT_IBM_0x0020	0x0020		/* MCA */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI */
#define	PCI_PRODUCT_IBM_SERVERAID	0x002e		/* ServeRAID */
#define	PCI_PRODUCT_IBM_0x0036	0x0036		/* Miami/PCI */
#define	PCI_PRODUCT_IBM_OLYMPIC	0x003e		/* Olympic */
#define	PCI_PRODUCT_IBM_I82557B	0x0057		/* i82557B 10/100 */
#define	PCI_PRODUCT_IBM_FIREGL2	0x0170		/* FireGL2 */
#define	PCI_PRODUCT_IBM_PCIX	0x01a7		/* PCIX-PCIX */
#define	PCI_PRODUCT_IBM_SERVERAID2	0x01bd		/* ServeRAID */

/* IDT products */
#define	PCI_PRODUCT_IDT_77201	0x0001		/* 77201/77211 ATM (NICStAR) */

/* Industrial Computer Source */
#define	PCI_PRODUCT_INDCOMPSRC_WDT50x	0x22c0		/* WDT 50x Watchdog Timer */

/* Initio Corporation */
#define	PCI_PRODUCT_INITIO_INIC850	0x0850		/* INIC-850 (A100UW) */
#define	PCI_PRODUCT_INITIO_INIC1060	0x1060		/* INIC-1060 (A100U2W) */
#define	PCI_PRODUCT_INITIO_INIC940	0x9400		/* INIC-940 */
#define	PCI_PRODUCT_INITIO_INIC941	0x9401		/* INIC-941 */
#define	PCI_PRODUCT_INITIO_INIC950	0x9500		/* INIC-950 */

/* INPROCOMM products */
#define	PCI_PRODUCT_INPROCOMM_IPN2120	0x2120		/* IPN2120 */
#define	PCI_PRODUCT_INPROCOMM_IPN2220	0x2220		/* IPN2220 */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */

/* Intel products */
#define	PCI_PRODUCT_INTEL_EESISA	0x0008		/* EES ISA */
#define	PCI_PRODUCT_INTEL_21145	0x0039		/* 21145 */
#define	PCI_PRODUCT_INTEL_80303	0x0309		/* 80303 IOP */
#define	PCI_PRODUCT_INTEL_80312	0x030d		/* 80312 I/O Companion */
#define	PCI_PRODUCT_INTEL_6700PXH_A	0x0329		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_6700PXH_B	0x032a		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_6702PXH	0x032c		/* PCIE-PCIE */
#define	PCI_PRODUCT_INTEL_IOP331_C0	0x0330		/* IOP331 Channel 0 */
#define	PCI_PRODUCT_INTEL_IOP331_C1	0x0332		/* IOP331 Channel 1 */
#define	PCI_PRODUCT_INTEL_41210_A	0x0340		/* 41210 PCIE-PCIX */
#define	PCI_PRODUCT_INTEL_41210_B	0x0341		/* 41210 PCIE-PCIX */
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
#define	PCI_PRODUCT_INTEL_82546EB_QUAD_COPPER	0x101d		/* PRO/1000MT QP (82546EB) */
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
#define	PCI_PRODUCT_INTEL_PRO_100_VE_3	0x103a		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_3	0x103b		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_4	0x103c		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_4	0x103d		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VM_5	0x103e		/* PRO/100 VM */
#define	PCI_PRODUCT_INTEL_536EP	0x1040		/* V.92 Modem */
#define	PCI_PRODUCT_INTEL_PRO_WL_2100	0x1043		/* PRO/Wireless 2100 */
#define	PCI_PRODUCT_INTEL_82597EX	0x1048		/* PRO/10GbE LR (82597EX) */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_5	0x1050		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_6	0x1051		/* PRO/100 VE */
#define	PCI_PRODUCT_INTEL_PRO_100_M	0x1059		/* PRO/100 M */
#define	PCI_PRODUCT_INTEL_82571EB_COPPER	0x105E		/* PRO/1000 PT (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_FIBER	0x105F		/* PRO/1000 PF (82571EB) */
#define	PCI_PRODUCT_INTEL_82571EB_SERDES	0x1060		/* PRO/1000 PB (82571EB) */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_7	0x1064		/* PRO/100 VE (82562EZ) */
#define	PCI_PRODUCT_INTEL_PRO_100_VE_8	0x1068		/* PRO/100 VE (82801FBM) */
#define	PCI_PRODUCT_INTEL_82547GI	0x1075		/* PRO/1000CT (82547GI) */
#define	PCI_PRODUCT_INTEL_82541GI	0x1076		/* PRO/1000MT (82541GI) */
#define	PCI_PRODUCT_INTEL_82541GI_MOBILE	0x1077		/* PRO/1000MT Mobile (82541GI) */
#define	PCI_PRODUCT_INTEL_82541ER	0x1078		/* PRO/1000MT (82541ER) */
#define	PCI_PRODUCT_INTEL_82546GB_COPPER	0x1079		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82546GB_FIBER	0x107A		/* PRO/1000MF (82546GB) */
#define	PCI_PRODUCT_INTEL_82546GB_SERDES	0x107B		/* PRO/1000MF (82546GB) */
#define	PCI_PRODUCT_INTEL_82541GI_LF	0x107C		/* PRO/1000GT (82541GI) */
#define	PCI_PRODUCT_INTEL_82572EI_COPPER	0x107D		/* PRO/1000 PT (82572EI) */
#define	PCI_PRODUCT_INTEL_82572EI_FIBER	0x107E		/* PRO/1000 PF (82572EI) */
#define	PCI_PRODUCT_INTEL_82572EI_SERDES	0x107F		/* PRO/1000 PB (82572EI) */
#define	PCI_PRODUCT_INTEL_82546GB_PCIE	0x108A		/* PRO/1000MT (82546GB) */
#define	PCI_PRODUCT_INTEL_82573E	0x108B		/* PRO/1000MT (82573E) */
#define	PCI_PRODUCT_INTEL_82573E_IAMT	0x108C		/* PRO/1000MT (82573E) */
#define	PCI_PRODUCT_INTEL_82573L	0x109A		/* PRO/1000MT (82573L) */
#define	PCI_PRODUCT_INTEL_82546GB_QUAD_COPPER	0x1099		/* PRO/1000MT QP (82546GB) */
#define	PCI_PRODUCT_INTEL_82815_DC100_HUB	0x1100		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_DC100_AGP	0x1101		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_DC100_GRAPH	0x1102		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_HUB	0x1110		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOAGP_GRAPH	0x1112		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_HUB	0x1120		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_NOGRAPH_AGP	0x1121		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_HUB	0x1130		/* 82815 Hub */
#define	PCI_PRODUCT_INTEL_82815_FULL_AGP	0x1131		/* 82815 AGP */
#define	PCI_PRODUCT_INTEL_82815_FULL_GRAPH	0x1132		/* 82815 Graphics */
#define	PCI_PRODUCT_INTEL_82806AA_APIC	0x1161		/* 82806AA APIC */
#define	PCI_PRODUCT_INTEL_82559ER	0x1209		/* 82559ER */
#define	PCI_PRODUCT_INTEL_82092AA	0x1222		/* 82092AA IDE */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82596	0x1226		/* EE Pro 10 PCI */
#define	PCI_PRODUCT_INTEL_EEPRO100	0x1227		/* EE Pro 100 */
#define	PCI_PRODUCT_INTEL_EEPRO100S	0x1228		/* EE Pro 100 Smart */
#define	PCI_PRODUCT_INTEL_82557	0x1229		/* 82557 */
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
#define	PCI_PRODUCT_INTEL_82870P2_PPB	0x1460		/* 82870P2 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82870P2_IOxAPIC	0x1461		/* 82870P2 IOxAPIC */
#define	PCI_PRODUCT_INTEL_82870P2_HPLUG	0x1462		/* 82870P2 Hot Plug */
#define	PCI_PRODUCT_INTEL_80960RP_ATU	0x1960		/* 80960RP ATU */
#define	PCI_PRODUCT_INTEL_82840_HB	0x1a21		/* 82840 Host */
#define	PCI_PRODUCT_INTEL_82840_AGP	0x1a23		/* 82840 AGP */
#define	PCI_PRODUCT_INTEL_82840_PCI	0x1a24		/* 82840 PCI */
#define	PCI_PRODUCT_INTEL_82845_HB	0x1a30		/* 82845 Host */
#define	PCI_PRODUCT_INTEL_82845_AGP	0x1a31		/* 82845 AGP */
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
#define	PCI_PRODUCT_INTEL_82801BA_AGP	0x244e		/* 82801BA AGP */
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
#define	PCI_PRODUCT_INTEL_82801EB_USB_5	0x24dd		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801EB_USB_4	0x24de		/* 82801EB/ER USB */
#define	PCI_PRODUCT_INTEL_82801ER_SATA	0x24df		/* 82801ER SATA */
#define	PCI_PRODUCT_INTEL_82820_MCH	0x2501		/* 82820 MCH */
#define	PCI_PRODUCT_INTEL_82820_AGP	0x250f		/* 82820 AGP */
#define	PCI_PRODUCT_INTEL_82850_HB	0x2530		/* 82850 Host */
#define	PCI_PRODUCT_INTEL_82860_HB	0x2531		/* 82860 Host */
#define	PCI_PRODUCT_INTEL_82850_AGP	0x2532		/* 82850/82860 AGP */
#define	PCI_PRODUCT_INTEL_82860_PCI1	0x2533		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI2	0x2534		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI3	0x2535		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_82860_PCI4	0x2536		/* 82860 PCI-PCI */
#define	PCI_PRODUCT_INTEL_E7500_HB	0x2540		/* E7500 MCH Host */
#define	PCI_PRODUCT_INTEL_E7500_DRAM	0x2541		/* E7500 DRAM */
#define	PCI_PRODUCT_INTEL_E7500_HI_B1	0x2543		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7500_HI_B2	0x2544		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7500_HI_C1	0x2545		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7500_HI_C2	0x2546		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7500_HI_D1	0x2547		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7500_HI_D2	0x2548		/* E7500 MCH */
#define	PCI_PRODUCT_INTEL_E7501_HB	0x254c		/* E7501 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_HB	0x2550		/* E7505 MCH Host */
#define	PCI_PRODUCT_INTEL_E7505_AGP	0x2552		/* E7505 MCH AGP */
#define	PCI_PRODUCT_INTEL_E7505_HI_B1	0x2553		/* E7505 MCH PCI-PCI */
#define	PCI_PRODUCT_INTEL_E7505_HI_B2	0x2554		/* E7505 MCH PCI-PCI */
#define	PCI_PRODUCT_INTEL_82845G	0x2560		/* 82845G/GL */
#define	PCI_PRODUCT_INTEL_82845G_AGP	0x2561		/* 82845G/GL/GV/GE/PE AGP */
#define	PCI_PRODUCT_INTEL_82845G_IGD	0x2562		/* 82845G/GL Video */
#define	PCI_PRODUCT_INTEL_82865_IO_1	0x2570		/* 82865G/PE/P CPU-I/0-1 */
#define	PCI_PRODUCT_INTEL_82865_AGP	0x2571		/* 82865G/PE/P CPU-AGP */
#define	PCI_PRODUCT_INTEL_82865_IGD	0x2572		/* 82865G Video */
#define	PCI_PRODUCT_INTEL_82865_CSA	0x2573		/* 82865G/PE/P CPU-CSA */
#define	PCI_PRODUCT_INTEL_82865_IO_2	0x2576		/* 82865G/PE/P CPU-I/0-2 */
#define	PCI_PRODUCT_INTEL_82875P_HB	0x2578		/* 82875P Host */
#define	PCI_PRODUCT_INTEL_82875P_AGP	0x2579		/* 82875P AGP */
#define	PCI_PRODUCT_INTEL_82875P_CSA	0x257b		/* 82875P PCI-CSA */
#define	PCI_PRODUCT_INTEL_82915G_HB	0x2580		/* 82915G/P/GV Host */
#define	PCI_PRODUCT_INTEL_82915G_PCIE	0x2581		/* 82915G/P/GV PCIE */
#define	PCI_PRODUCT_INTEL_82915G_IV	0x2582		/* 82915G/P/GV Video */
#define	PCI_PRODUCT_INTEL_82925X_HB	0x2584		/* 82925X MCH Host */
#define	PCI_PRODUCT_INTEL_82925X_PCIE	0x2585		/* 82925X PCIE */
#define	PCI_PRODUCT_INTEL_E7221_HB	0x2588		/* E7221 MCH Host */
#define	PCI_PRODUCT_INTEL_E7221_PCIE	0x2589		/* E7221 PCIE */
#define	PCI_PRODUCT_INTEL_E7221_IGD	0x258a		/* E7221 Video */
#define	PCI_PRODUCT_INTEL_82915GM_HB	0x2590		/* 82915GM/PM/GMS Host */
#define	PCI_PRODUCT_INTEL_82915GM_PCIE	0x2591		/* 82915PM/GM PCIE */
#define	PCI_PRODUCT_INTEL_82915GM_IGD	0x2592		/* 82915GM/GMS Video */
#define	PCI_PRODUCT_INTEL_6300ESB_LPC	0x25a1		/* 6300ESB LPC */
#define	PCI_PRODUCT_INTEL_6300ESB_IDE	0x25a2		/* 6300ESB IDE */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA	0x25a3		/* 6300ESB SATA */
#define	PCI_PRODUCT_INTEL_6300ESB_SMBUS	0x25a4		/* 6300ESB SMBus */
#define	PCI_PRODUCT_INTEL_6300ESB_ACA	0x25a6		/* 6300ESB AC97 */
#define	PCI_PRODUCT_INTEL_6300ESB_ACM	0x25a7		/* 6300ESB Modem */
#define	PCI_PRODUCT_INTEL_6300ESB_USB	0x25a9		/* 6300ESB USB */
#define	PCI_PRODUCT_INTEL_5300ESB_USB	0x25aa		/* 5300ESB USB */
#define	PCI_PRODUCT_INTEL_6300ESB_WDT	0x25ab		/* 6300ESB WDT */
#define	PCI_PRODUCT_INTEL_6300ESB_APIC	0x25ac		/* 6300ESB APIC */
#define	PCI_PRODUCT_INTEL_6300ESB_USB2	0x25ad		/* 6300ESB USB */
#define	PCI_PRODUCT_INTEL_6300ESB_PCIX	0x25ae		/* 6300ESB PCIX */
#define	PCI_PRODUCT_INTEL_6300ESB_SATA2	0x25b0		/* 6300ESB SATA */
#define	PCI_PRODUCT_INTEL_82801FB_LPC	0x2640		/* 82801FB LPC */
#define	PCI_PRODUCT_INTEL_82801FBM_LPC	0x2641		/* 82801FBM LPC */
#define	PCI_PRODUCT_INTEL_82801FB_SATA	0x2651		/* 82801FB SATA */
#define	PCI_PRODUCT_INTEL_82801FR_SATA	0x2652		/* 82801FR SATA */
#define	PCI_PRODUCT_INTEL_82801FBM_IDE	0x2653		/* 82801FBM IDE */
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
#define	PCI_PRODUCT_INTEL_82801FB_SMBUS	0x266a		/* 82801FB SMBus */
#define	PCI_PRODUCT_INTEL_82801FB_ACM	0x266d		/* 82801FB Modem */
#define	PCI_PRODUCT_INTEL_82801FB_ACA	0x266e		/* 82801FB AC97 */
#define	PCI_PRODUCT_INTEL_82801FB_IDE	0x266f		/* 82801FB IDE */
#define	PCI_PRODUCT_INTEL_82945GP_MCH	0x2770		/* 82945GP */
#define	PCI_PRODUCT_INTEL_82945GP_PCIE	0x2771		/* 82945GP PCIE */
#define	PCI_PRODUCT_INTEL_82955X_HB	0x2774		/* 82955X MCH */
#define	PCI_PRODUCT_INTEL_82955X_PCIE	0x2775		/* 82955X PCIE */
#define	PCI_PRODUCT_INTEL_E7230_MCH	0x2778		/* E7230 MCH */
#define	PCI_PRODUCT_INTEL_E7230_PCIE	0x2779		/* E7230 PCIE */
#define	PCI_PRODUCT_INTEL_82915G_IGD	0x2782		/* 82915G Video */
#define	PCI_PRODUCT_INTEL_82915GM_IGD_S	0x2792		/* 82915GM/GMS Video */
#define	PCI_PRODUCT_INTEL_82801GB_LPC	0x27b8		/* 82801GB LPC */
#define	PCI_PRODUCT_INTEL_82801GB_SATA_1	0x27c0		/* 82801GB SATA */
#define	PCI_PRODUCT_INTEL_82801GB_SATA_2	0x27c1		/* 82801GB AHCI SATA */
#define	PCI_PRODUCT_INTEL_82801GB_SATA_3	0x27c3		/* 82801GB SATA */
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
#define	PCI_PRODUCT_INTEL_82801GB_SMBUS	0x27da		/* 82801GB SMBus */
#define	PCI_PRODUCT_INTEL_82801GB_LAN	0x27dc		/* 82801GB LAN */
#define	PCI_PRODUCT_INTEL_82801GB_ACM	0x27dd		/* 82801GB Modem */
#define	PCI_PRODUCT_INTEL_82801GB_ACA	0x27de		/* 82801GB AC97 */
#define	PCI_PRODUCT_INTEL_82801GB_IDE	0x27df		/* 82801GB IDE */
#define	PCI_PRODUCT_INTEL_82801G_PCIE_5	0x27e0		/* 82801G PCIE */
#define	PCI_PRODUCT_INTEL_82801G_PCIE_6	0x27e2		/* 82801G PCIE */
#define	PCI_PRODUCT_INTEL_RCU32	0x3092		/* RCU32 I2O RAID */
#define	PCI_PRODUCT_INTEL_31244	0x3200		/* 31244 SATA */
#define	PCI_PRODUCT_INTEL_82855PE	0x3340		/* 82855PE Hub */
#define	PCI_PRODUCT_INTEL_82855PE_AGP	0x3341		/* 82855PE AGP */
#define	PCI_PRODUCT_INTEL_82830MP_IO_1	0x3575		/* 82830MP CPU-I/O-1 */
#define	PCI_PRODUCT_INTEL_82830MP_AGP	0x3576		/* 82830MP CPU-AGP */
#define	PCI_PRODUCT_INTEL_82830MP_IV	0x3577		/* 82830MP Integrated Video */
#define	PCI_PRODUCT_INTEL_82830MP_IO_2	0x3578		/* 82830MP CPU-I/O-2 */
#define	PCI_PRODUCT_INTEL_82852GM_HPB	0x3580		/* 82852GM Hub-PCI */
#define	PCI_PRODUCT_INTEL_82852_AGP	0x3581		/* 82852/82855 AGP */
#define	PCI_PRODUCT_INTEL_82852GM_AGP	0x3582		/* 82852GM AGP */
#define	PCI_PRODUCT_INTEL_82852GM_MEM	0x3584		/* 82852GM Memory */
#define	PCI_PRODUCT_INTEL_82852GM_CFG	0x3585		/* 82852GM Configuration */
#define	PCI_PRODUCT_INTEL_E7710_SMCH	0x3590		/* E7710 SMCH */
#define	PCI_PRODUCT_INTEL_E7710_MCH_ERR	0x3591		/* E7710 MCH ERR */
#define	PCI_PRODUCT_INTEL_E7710_MCH_DMA	0x3594		/* E7710 MCH DMA */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEA0	0x3595		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEA1	0x3596		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEB0	0x3597		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEB1	0x3598		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEC0	0x3599		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_MCH_PCIEC1	0x359a		/* E7710 MCH PCIE */
#define	PCI_PRODUCT_INTEL_E7710_WMCH	0x359e		/* E7710 WMCH */
#define	PCI_PRODUCT_INTEL_PRO_WL_2200BG	0x4220		/* PRO/Wireless 2200BG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2225BG	0x4221		/* PRO/Wireless 2225BG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_1	0x4223		/* PRO/Wireless 2915ABG */
#define	PCI_PRODUCT_INTEL_PRO_WL_2915ABG_2	0x4224		/* PRO/Wireless 2915ABG */
#define	PCI_PRODUCT_INTEL_80960RD	0x5200		/* i960 RD PCI-PCI */
#define	PCI_PRODUCT_INTEL_PRO_100_SERVER	0x5201		/* PRO 100 Server */
#define	PCI_PRODUCT_INTEL_82371SB_ISA	0x7000		/* 82371SB ISA */
#define	PCI_PRODUCT_INTEL_82371SB_IDE	0x7010		/* 82371SB IDE */
#define	PCI_PRODUCT_INTEL_82371USB	0x7020		/* 82371SB USB */
#define	PCI_PRODUCT_INTEL_82437VX	0x7030		/* 82437VX */
#define	PCI_PRODUCT_INTEL_82439TX	0x7100		/* 82439TX System */
#define	PCI_PRODUCT_INTEL_82371AB_ISA	0x7110		/* 82371AB PIIX4 ISA */
#define	PCI_PRODUCT_INTEL_82371AB_IDE	0x7111		/* 82371AB IDE */
#define	PCI_PRODUCT_INTEL_82371AB_USB	0x7112		/* 82371AB USB */
#define	PCI_PRODUCT_INTEL_82371AB_PMC	0x7113		/* 82371AB Power */
#define	PCI_PRODUCT_INTEL_82810_MCH	0x7120		/* 82810 */
#define	PCI_PRODUCT_INTEL_82810_GC	0x7121		/* 82810 Graphics */
#define	PCI_PRODUCT_INTEL_82810_DC100_MCH	0x7122		/* 82810-DC100 */
#define	PCI_PRODUCT_INTEL_82810_DC100_GC	0x7123		/* 82810-DC100 Graphics */
#define	PCI_PRODUCT_INTEL_82810E_MCH	0x7124		/* 82810E */
#define	PCI_PRODUCT_INTEL_82810E_GC	0x7125		/* 82810E Graphics */
#define	PCI_PRODUCT_INTEL_82443LX	0x7180		/* 82443LX AGP */
#define	PCI_PRODUCT_INTEL_82443LX_AGP	0x7181		/* 82443LX AGP */
#define	PCI_PRODUCT_INTEL_82443BX	0x7190		/* 82443BX AGP */
#define	PCI_PRODUCT_INTEL_82443BX_AGP	0x7191		/* 82443BX AGP */
#define	PCI_PRODUCT_INTEL_82443BX_NOAGP	0x7192		/* 82443BX */
#define	PCI_PRODUCT_INTEL_82440MX	0x7194		/* 82440MX Host */
#define	PCI_PRODUCT_INTEL_82440MX_ACA	0x7195		/* 82440MX AC97 */
#define	PCI_PRODUCT_INTEL_82440MX_ACM	0x7196		/* 82440MX Modem */
#define	PCI_PRODUCT_INTEL_82440MX_ISA	0x7198		/* 82440MX ISA */
#define	PCI_PRODUCT_INTEL_82440MX_IDE	0x7199		/* 82440MX IDE */
#define	PCI_PRODUCT_INTEL_82440MX_USB	0x719a		/* 82440MX USB */
#define	PCI_PRODUCT_INTEL_82440MX_PM	0x719b		/* 82440MX Power */
#define	PCI_PRODUCT_INTEL_82440BX	0x71a0		/* 82440BX AGP */
#define	PCI_PRODUCT_INTEL_82440BX_AGP	0x71a1		/* 82440BX AGP */
#define	PCI_PRODUCT_INTEL_82443GX	0x71a2		/* 82443GX */
#define	PCI_PRODUCT_INTEL_82740	0x7800		/* 82740 AGP */
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
#define	PCI_PRODUCT_INTEL_21152	0xb152		/* S21152BB PCI-PCI */
#define	PCI_PRODUCT_INTEL_21154	0xb154		/* S21154AE/BE PCI-PCI */

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
#define	PCI_PRODUCT_ITEXPRESS_IT8212F	0x8212		/* IT8212F */
#define	PCI_PRODUCT_ITEXPRESS_IT8330G	0x8330		/* IT8330G */

/* KTI */
#define	PCI_PRODUCT_KTI_KTIE	0x3000		/* KTI */

/* LAN Media Corporation */
#define	PCI_PRODUCT_LMC_HSSI	0x0003		/* HSSI */
#define	PCI_PRODUCT_LMC_DS3	0x0004		/* DS3 */
#define	PCI_PRODUCT_LMC_SSI	0x0005		/* SSI */
#define	PCI_PRODUCT_LMC_DS1	0x0006		/* DS1 */

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
#define	PCI_PRODUCT_LAVA_PARALLELB	0x8003		/* Dual Parallel B */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */
#define	PCI_PRODUCT_LEADTEK_WINFAST	0x6606		/* Leadtek WinFast TV 2000 */

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
#define	PCI_PRODUCT_MARVELL_88W8310_1	0x1fa6		/* Libertas 88W8310 */
#define	PCI_PRODUCT_MARVELL_88W8310_2	0x1fa7		/* Libertas 88W8310 */
#define	PCI_PRODUCT_MARVELL_SK_V2	0x4320		/* SKv2 */
#define	PCI_PRODUCT_MARVELL_YUKON_1	0x4340		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_2	0x4341		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_3	0x4342		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_4	0x4343		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_5	0x4344		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_6	0x4345		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_7	0x4346		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_8	0x4347		/* Yukon */
#define	PCI_PRODUCT_MARVELL_YUKON_8035	0x4350		/* Yukon 8035 */
#define	PCI_PRODUCT_MARVELL_YUKON_8036	0x4351		/* Yukon 8036 */
#define	PCI_PRODUCT_MARVELL_YUKON_8052	0x4360		/* Yukon 8052 */
#define	PCI_PRODUCT_MARVELL_YUKON_8050	0x4361		/* Yukon 8050 */
#define	PCI_PRODUCT_MARVELL_YUKON_8053	0x4362		/* Yukon 8053 */
#define	PCI_PRODUCT_MARVELL_SK_V2_BELKIN	0x5005		/* Belkin F5D5005 */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA PX2085 (Atlas) */
#define	PCI_PRODUCT_MATROX_MILLENIUM	0x0519		/* MGA Millenium 2064W (Storm) */
#define	PCI_PRODUCT_MATROX_MYSTIQUE_220	0x051a		/* MGA 1064SG 220MHz */
#define	PCI_PRODUCT_MATROX_MILLENNIUM_II	0x051b		/* MGA Millennium II 2164W */
#define	PCI_PRODUCT_MATROX_MILLENNIUM_IIAGP	0x051f		/* MGA Millennium II 2164WA-B AGP */
#define	PCI_PRODUCT_MATROX_MILL_II_G200_PCI	0x0520		/* MGA G200 PCI */
#define	PCI_PRODUCT_MATROX_MILL_II_G200_AGP	0x0521		/* MGA G200 AGP */
#define	PCI_PRODUCT_MATROX_MILL_II_G400_AGP	0x0525		/* MGA G400/G450 AGP */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */
#define	PCI_PRODUCT_MATROX_PRODUCTIVA_PCI	0x1000		/* MGA G100 PCI */
#define	PCI_PRODUCT_MATROX_PRODUCTIVA_AGP	0x1001		/* MGA G100 AGP */
#define	PCI_PRODUCT_MATROX_MYSTIQUE	0x102b		/* MGA 1064SG */
#define	PCI_PRODUCT_MATROX_G400_TH	0x2179		/* MGA G400 Twin Head */
#define	PCI_PRODUCT_MATROX_MILL_II_G550_AGP	0x2527		/* MGA G550 AGP */
#define	PCI_PRODUCT_MATROX_MILL_G200_SD	0xff00		/* MGA Millennium G200 SD */
#define	PCI_PRODUCT_MATROX_PROD_G100_SD	0xff01		/* MGA Produktiva G100 SD */
#define	PCI_PRODUCT_MATROX_MYST_G200_SD	0xff02		/* MGA Mystique G200 SD */
#define	PCI_PRODUCT_MATROX_MILL_G200_SG	0xff03		/* MGA Millennium G200 SG */
#define	PCI_PRODUCT_MATROX_MARV_G200_SD	0xff04		/* MGA Marvel G200 SD */

/* Mentor */
#define	PCI_PRODUCT_MENTOR_PCI0660	0x0660		/* PCI */
#define	PCI_PRODUCT_MENTOR_PCI0661	0x0661		/* PCI-PCI */

/* Microsoft products */
#define	PCI_PRODUCT_MICROSOFT_MN120	0x0001		/* MN-120 10/100 */
#define	PCI_PRODUCT_MICROSOFT_MN130	0x0002		/* MN-130 10/100 */

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
#define	PCI_PRODUCT_MOXA_CP104	0x1041		/* CP-104UL */
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
#define	PCI_PRODUCT_MYRICOM_LANAI_92	0x8043		/* MyriNet LANai 9.2 */

/* Myson Century products */
#define	PCI_PRODUCT_MYSON_MTD800	0x0800		/* MTD800 10/100 */
#define	PCI_PRODUCT_MYSON_MTD803	0x0803		/* MTD803 10/100 */
#define	PCI_PRODUCT_MYSON_MTD891	0x0891		/* MTD891 10/100/1000 */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

/* National Datacomm Corp products */
#define	PCI_PRODUCT_NDC_NCP130	0x0130		/* NCP130 */
#define	PCI_PRODUCT_NDC_NCP130A2	0x0131		/* NCP130 Rev A2 */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_DP83810	0x0001		/* DP83810 10/100 */
#define	PCI_PRODUCT_NS_PC87415	0x0002		/* PC87415 IDE */
#define	PCI_PRODUCT_NS_PC87560	0x000e		/* 87560 Legacy I/O */
#define	PCI_PRODUCT_NS_USB	0x0012		/* USB */
#define	PCI_PRODUCT_NS_DP83815	0x0020		/* DP83815 10/100 */
#define	PCI_PRODUCT_NS_DP83820	0x0022		/* DP83820 10/100/1000 */
#define	PCI_PRODUCT_NS_SCx200_ISA	0x0500		/* SCx200 ISA */
#define	PCI_PRODUCT_NS_SCx200_SMI	0x0501		/* SCx200 SMI */
#define	PCI_PRODUCT_NS_SCx200_IDE	0x0502		/* SCx200 IDE */
#define	PCI_PRODUCT_NS_SCx200_AUDIO	0x0503		/* SCx200 AUDIO */
#define	PCI_PRODUCT_NS_SCx200_VIDEO	0x0504		/* SCx200 VIDEO */
#define	PCI_PRODUCT_NS_SCx200_XBUS	0x0505		/* SCx200 X-BUS */
#define	PCI_PRODUCT_NS_SC1100_ISA	0x0510		/* SC1100 ISA */
#define	PCI_PRODUCT_NS_SC1100_SMI	0x0511		/* SC1100 SMI/ACPI */
#define	PCI_PRODUCT_NS_SC1100_XBUS	0x0515		/* SC1100 X-Bus */
#define	PCI_PRODUCT_NS_NS87410	0xd001		/* NS87410 */

/* NEC */
#define	PCI_PRODUCT_NEC_USB	0x0035		/* USB */
#define	PCI_PRODUCT_NEC_POWERVR2	0x0046		/* PowerVR PCX2 */
#define	PCI_PRODUCT_NEC_MARTH	0x0074		/* I/O */
#define	PCI_PRODUCT_NEC_PKUG	0x007d		/* I/O */
#define	PCI_PRODUCT_NEC_uPD72874	0x00f2		/* Firewire */
#define	PCI_PRODUCT_NEC_USB2	0x00e0		/* USB */
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
#define	PCI_PRODUCT_NETERION_XFRAME	0x5831		/* Xframe 10GbE */

/* Netgear products */
#define	PCI_PRODUCT_NETGEAR_MA301	0x4100		/* MA301 */
#define	PCI_PRODUCT_NETGEAR_GA620	0x620a		/* GA620 */
#define	PCI_PRODUCT_NETGEAR_GA620T	0x630a		/* GA620T */

/* NetMos */
#define	PCI_PRODUCT_NETMOS_1P	0x9805		/* 1P */
#define	PCI_PRODUCT_NETMOS_2S1P	0x9835		/* 2S1P */
#define	PCI_PRODUCT_NETMOS_4S	0x9845		/* 4S */

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
#define	PCI_PRODUCT_SYMBIOS_875J	0x008f		/* 53c875J */
#define	PCI_PRODUCT_SYMBIOS_FC909	0x0620		/* FC909 */
#define	PCI_PRODUCT_SYMBIOS_FC909A	0x0621		/* FC909A */
#define	PCI_PRODUCT_SYMBIOS_FC929	0x0622		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC929_1	0x0623		/* FC929 */
#define	PCI_PRODUCT_SYMBIOS_FC919	0x0624		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_FC919_1	0x0625		/* FC919 */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID	0x1960		/* MegaRAID */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_320	0x0407		/* MegaRAID 320 */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_3202E	0x0408		/* MegaRAID 320-2E */
#define	PCI_PRODUCT_SYMBIOS_SATA8	0x0409		/* MegaRAID SATA 8x */

/* Packet Engines products */
#define	PCI_PRODUCT_SYMBIOS_PE_GNIC	0x0702		/* Packet Engines G-NIC */

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

/* NVidia products */
#define	PCI_PRODUCT_NVIDIA_NV1	0x0008		/* NV1 */
#define	PCI_PRODUCT_NVIDIA_DAC64	0x0009		/* DAC64 */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT	0x0020		/* Riva TNT */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT2	0x0028		/* Riva TNT2 */
#define	PCI_PRODUCT_NVIDIA_RIVA_TNT2_ULTRA	0x0029		/* Riva TNT2 Ultra */
#define	PCI_PRODUCT_NVIDIA_VANTA1	0x002c		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_VANTA2	0x002d		/* Vanta */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN1	0x0037		/* MCP04 LAN */
#define	PCI_PRODUCT_NVIDIA_MCP04_LAN2	0x0038		/* MCP04 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ISA	0x0050		/* nForce4 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ISA2	0x0051		/* nForce4 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SMBUS	0x0052		/* nForce4 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_ATA133	0x0053		/* nForce4 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA1	0x0054		/* nForce4 SATA 1 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_SATA2	0x0055		/* nForce4 SATA 2 */
#define	PCI_PRODUCT_NVIDIA_CK804_LAN1	0x0056		/* CK804 LAN */
#define	PCI_PRODUCT_NVIDIA_CK804_LAN2	0x0057		/* CK804 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_AC	0x0059		/* nForce4 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB	0x005a		/* nForce4 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_USB2	0x005b		/* nForce4 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PPB	0x005c		/* nForce4 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_PPB2	0x005d		/* nForce4 PCIE */
#define	PCI_PRODUCT_NVIDIA_NFORCE4_MEM	0x005e		/* nForce4 DDR */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ISA	0x0060		/* nForce2 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_SMB	0x0064		/* nForce2 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_IDE	0x0065		/* nForce2 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_LAN	0x0066		/* nForce2 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_USB	0x0067		/* nForce2 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_USB2	0x0068		/* nForce2 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_ACA	0x006a		/* nForce2 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_APU	0x006b		/* nForce2 Audio */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB	0x006c		/* nForce2 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_PPB2	0x006d		/* nForce2 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_FW	0x006e		/* nForce2 FireWire */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ISA	0x0080		/* nForce2 400 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SMB	0x0084		/* nForce2 400 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_IDE	0x0085		/* nForce2 400 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN2	0x0086		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_USB	0x0087		/* nForce2 400 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_USB2	0x0088		/* nForce2 400 USB2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_ACA	0x008a		/* nForce2 400 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_PPB	0x008b		/* nForce2 400 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN3	0x008c		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE2_400_SATA	0x008e		/* nForce2 400 SATA */
#define	PCI_PRODUCT_NVIDIA_ITNT2	0x00a0		/* Aladdin TNT2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_ISA	0x00d0		/* nForce3 ISA */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PCHB	0x00d1		/* nForce3 PCI Host */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_PPB2	0x00d2		/* nForce3 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_SMB	0x00d4		/* nForce3 SMBus */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_IDE	0x00d5		/* nForce3 IDE */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_LAN1	0x00d6		/* nForce3 LAN */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_USB	0x00d7		/* nForce3 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_USB2	0x00d8		/* nForce3 USB */
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
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_USB	0x00e7		/* nForce3 250 USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_USB2	0x00e8		/* nForce3 250 USB2 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_ACA	0x00ea		/* nForce3 250 AC97 */
#define	PCI_PRODUCT_NVIDIA_NFORCE3_250_PPB	0x00ed		/* nForce3 250 PCI-PCI */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GTAGP	0x00f1		/* GeForce 6600 GT AGP */
#define	PCI_PRODUCT_NVIDIA_QUADROFX330	0x00fd		/* Quadro FX 330 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256	0x0100		/* GeForce256 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE256_DDR	0x0101		/* GeForce256 DDR */
#define	PCI_PRODUCT_NVIDIA_QUADOR	0x0103		/* Quadro */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX	0x0110		/* GeForce2 MX */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2MX_100	0x0111		/* GeForce2 MX 100 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GO	0x0112		/* GeForce2 Go */
#define	PCI_PRODUCT_NVIDIA_QUADRO2_MXR	0x0113		/* Quadro2 MXR */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600GT	0x0140		/* GeForce 6600 GT */
#define	PCI_PRODUCT_NVIDIA_GEFORCE6600	0x0141		/* GeForce 6600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2GTS	0x0150		/* GeForce2 GTS */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2TI	0x0151		/* GeForce2 Ti */
#define	PCI_PRODUCT_NVIDIA_GEFORCE2ULTRA	0x0152		/* GeForce2 Ultra */
#define	PCI_PRODUCT_NVIDIA_QUADRO2PRO	0x0153		/* Quadro2 Pro */
#define	PCI_PRODUCT_NVIDIA_QUADRONVS285	0x0165		/* Quadro NVS 285 */
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
#define	PCI_PRODUCT_NVIDIA_NFORCE_USB	0x01c2		/* nForce USB */
#define	PCI_PRODUCT_NVIDIA_NFORCE_LAN	0x01c3		/* nForce LAN */
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
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4600	0x0250		/* GeForce4 Ti 4600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4400	0x0251		/* GeForce4 Ti 4400 */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4200	0x0253		/* GeForce4 Ti 4200 */
#define	PCI_PRODUCT_NVIDIA_QUADRO4900XGL	0x0258		/* Quadro4 900 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4750XGL	0x0259		/* Quadro4 750 XGL */
#define	PCI_PRODUCT_NVIDIA_QUADRO4700XGL	0x025B		/* Quadro4 700 XGL */
#define	PCI_PRODUCT_NVIDIA_GEFORCE4TI4200GO	0x0286		/* GeForce4 Ti 4200 Go */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5800_U	0x0301		/* GeForce FX 5800 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5800	0x0302		/* GeForce FX 5800 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5600_U	0x0311		/* GeForce FX 5600 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5600	0x0312		/* GeForce FX 5600 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5200_U	0x0321		/* GeForce FX 5200 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5200	0x0322		/* GeForce FX 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5200	0x0324		/* GeForce FX Go 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5500	0x0326		/* GeForce FX 5500 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5200_2	0x0329		/* GeForce FX Go 5200 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5900_U	0x0330		/* GeForce FX 5900 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5900	0x0331		/* GeForce FX 5900 */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5950_U	0x0333		/* GeForce FX 5950 Ultra */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFX5700LE	0x0343		/* GeForce FX 5700LE */
#define	PCI_PRODUCT_NVIDIA_GEFORCEFXGO5700	0x0348		/* GeForce FX Go 5700 */

/* Oak Technologies products */
#define	PCI_PRODUCT_OAKTECH_OTI1007	0x0107		/* OTI107 */

/* Olicom */
#define	PCI_PRODUCT_OLICOM_OC2183	0x0013		/* OC2183 */
#define	PCI_PRODUCT_OLICOM_OC2325	0x0012		/* OC2325 */
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
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI200HV2	0xe020		/* 200HV2 */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI800H_0	0xa003		/* 400H/800H */
#define	PCI_PRODUCT_OXFORD_VSCOM_PCI800H_1	0xa004		/* 800H */
#define	PCI_PRODUCT_OXFORD2_VSCOM_PCI011H	0x8403		/* 011H */
#define	PCI_PRODUCT_OXFORD2_EXSYS_EX41098	0x9511		/* Exsys EX-41098 */

/* Packet Engines products */
#define	PCI_PRODUCT_PE_GNIC2	0x0911		/* PMC/GNIC2 */

/* PC Tech products */
#define	PCI_PRODUCT_PCTECH_RZ1000	0x1000		/* RZ1000 */

/* PCTEL */
#define	PCI_PRODUCT_PCTEL_MICROMODEM56	0x7879		/* HSP MicroModem 56 */

/* Ross -> Pequr -> ServerWorks -> Broadcom products */
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
#define	PCI_PRODUCT_RCC_CIOB_X2	0x0101		/* CIOB-X2 PCIX */
#define	PCI_PRODUCT_RCC_CIOB_E	0x0110		/* CIOB-E */
#define	PCI_PRODUCT_RCC_OSB4	0x0200		/* OSB4 */
#define	PCI_PRODUCT_RCC_CSB5	0x0201		/* CSB5 */
#define	PCI_PRODUCT_RCC_CSB6	0x0203		/* CSB6 */
#define	PCI_PRODUCT_RCC_OSB4_IDE	0x0211		/* OSB4 IDE */
#define	PCI_PRODUCT_RCC_CSB5_IDE	0x0212		/* CSB5 IDE */
#define	PCI_PRODUCT_RCC_CSB6_RAID_IDE	0x0213		/* CSB6 RAID/IDE */
#define	PCI_PRODUCT_RCC_HT_1000_IDE	0x0214		/* HT-1000 IDE */
#define	PCI_PRODUCT_RCC_CSB6_IDE	0x0217		/* CSB6 IDE */
#define	PCI_PRODUCT_RCC_USB	0x0220		/* OSB4/CSB5 USB */
#define	PCI_PRODUCT_RCC_CSB6_USB	0x0221		/* CSB6 USB */
#define	PCI_PRODUCT_RCC_CSB5_LPC_1	0x0225		/* CSB5 LPC */
#define	PCI_PRODUCT_RCC_GCLE_2	0x0227		/* GCLE-2 Host */
#define	PCI_PRODUCT_RCC_CSB5_LPC_2	0x0230		/* CSB5 LPC */
#define	PCI_PRODUCT_RCC_K2_SATA	0x0240		/* K2 SATA */
#define	PCI_PRODUCT_RCC_FRODO4_SATA	0x0241		/* Frodo4 SATA */
#define	PCI_PRODUCT_RCC_FRODO8_SATA	0x0242		/* Frodo8 SATA */
#define	PCI_PRODUCT_RCC_HT_1000_SATA	0x024a		/* HT-1000 SATA */

/* Philips products */
#define	PCI_PRODUCT_PHILIPS_SAA7130	0x7130		/* SAA7130 */

/* Picopower */
#define	PCI_PRODUCT_PICOPOWER_PT80C826	0x0000		/* PT80C826 */
#define	PCI_PRODUCT_PICOPOWER_PT86C521	0x0001		/* PT86C521 */
#define	PCI_PRODUCT_PICOPOWER_PT86C523	0x0002		/* PT86C523 */
#define	PCI_PRODUCT_PICOPOWER_PC87550	0x0005		/* PC87550 */
#define	PCI_PRODUCT_PICOPOWER_PT86C523_2	0x8002		/* PT86C523_2 */

/* Pijnenburg */
#define	PCI_PRODUCT_PIJNENBURG_PCC_ISES	0x0001		/* PCC-ISES */

/* Platform */
#define	PCI_PRODUCT_PLATFORM_ES1849	0x0100		/* ES1849 */

/* PLX products */
#define	PCI_PRODUCT_PLX_1076	0x1076		/* I/O */
#define	PCI_PRODUCT_PLX_1077	0x1077		/* I/O */
#define	PCI_PRODUCT_PLX_9050	0x9050		/* I/O 9050 */
#define	PCI_PRODUCT_PLX_9080	0x9080		/* I/O 9080 */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_DC5030	0x5300		/* DC5030 */
#define	PCI_PRODUCT_PROMISE_PDC20246	0x4d33		/* PDC20246 */
#define	PCI_PRODUCT_PROMISE_PDC20262	0x4d38		/* PDC20262 */
#define	PCI_PRODUCT_PROMISE_PDC20263	0x0d38		/* PDC20263 */
#define	PCI_PRODUCT_PROMISE_PDC20265	0x0d30		/* PDC20265 */
#define	PCI_PRODUCT_PROMISE_PDC20267	0x4d30		/* PDC20267 */
#define	PCI_PRODUCT_PROMISE_PDC20268	0x4d68		/* PDC20268 */
#define	PCI_PRODUCT_PROMISE_PDC20268R	0x6268		/* PDC20268R */
#define	PCI_PRODUCT_PROMISE_PDC20269	0x4d69		/* PDC20269 */
#define	PCI_PRODUCT_PROMISE_PDC20271	0x6269		/* PDC20271 */
#define	PCI_PRODUCT_PROMISE_PDC20275	0x1275		/* PDC20275 */
#define	PCI_PRODUCT_PROMISE_PDC20276	0x5275		/* PDC20276 */
#define	PCI_PRODUCT_PROMISE_PDC20277	0x7275		/* PDC20277 */
#define	PCI_PRODUCT_PROMISE_PDC20318	0x3318		/* PDC20318 */
#define	PCI_PRODUCT_PROMISE_PDC20319	0x3319		/* PDC20319 */
#define	PCI_PRODUCT_PROMISE_PDC20371	0x3371		/* PDC20371 */
#define	PCI_PRODUCT_PROMISE_PDC20375	0x3375		/* PDC20375 */
#define	PCI_PRODUCT_PROMISE_PDC20376	0x3376		/* PDC20376 */
#define	PCI_PRODUCT_PROMISE_PDC20377	0x3377		/* PDC20377 */
#define	PCI_PRODUCT_PROMISE_PDC20378	0x3373		/* PDC20378 */
#define	PCI_PRODUCT_PROMISE_PDC20379	0x3372		/* PDC20379 */
#define	PCI_PRODUCT_PROMISE_PDC20617	0x6617		/* PDC20617 */
#define	PCI_PRODUCT_PROMISE_PDC20618	0x6626		/* PDC20618 */
#define	PCI_PRODUCT_PROMISE_PDC20619	0x6629		/* PDC20619 */
#define	PCI_PRODUCT_PROMISE_PDC20620	0x6620		/* PDC20620 */
#define	PCI_PRODUCT_PROMISE_PDC20621	0x6621		/* PDC20621 */

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
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312		/* ISP6312 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* Ralink Technology Corporation */
#define	PCI_PRODUCT_RALINK_RT2460A	0x0101		/* RT2460A */
#define	PCI_PRODUCT_RALINK_RT2560	0x0201		/* RT2560 */

/* Realtek (Creative Labs?) products */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* 8029 */
#define	PCI_PRODUCT_REALTEK_RT8129	0x8129		/* 8129 */
#define	PCI_PRODUCT_REALTEK_RT8138	0x8138		/* 8138 */
#define	PCI_PRODUCT_REALTEK_RT8139	0x8139		/* 8139 */
#define	PCI_PRODUCT_REALTEK_RT8169	0x8169		/* 8169 */
#define	PCI_PRODUCT_REALTEK_RT8180	0x8180		/* 8180 */
#define	PCI_PRODUCT_REALTEK_RT8185	0x8185		/* 8185 */

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
#define	PCI_PRODUCT_S3_PROSAVAGE_PM133	0x8a25		/* ProSavage PM133 */
#define	PCI_PRODUCT_S3_VIRGE_MX	0x8c01		/* ViRGE MX */
#define	PCI_PRODUCT_S3_VIRGE_MXP	0x8c03		/* ViRGE MXP */
#define	PCI_PRODUCT_S3_SAVAGE_MXMV	0x8c10		/* Savage/MX-MV */
#define	PCI_PRODUCT_S3_SAVAGE_MX	0x8c11		/* Savage/MX */
#define	PCI_PRODUCT_S3_SAVAGE_IXMV	0x8c12		/* Savage/IX-MV */
#define	PCI_PRODUCT_S3_SAVAGE_IX	0x8c13		/* Savage/IX */
#define	PCI_PRODUCT_S3_SUPERSAVAGE	0x8c2e		/* SuperSavage */
#define	PCI_PRODUCT_S3_TWISTER	0x8d01		/* Twister */
#define	PCI_PRODUCT_S3_TWISTER_K	0x8d02		/* Twister-K */
#define	PCI_PRODUCT_S3_PROSAVAGE_DDR	0x8d04		/* ProSavage DDR */
#define	PCI_PRODUCT_S3_SONICVIBES	0xca00		/* SonicVibes */

/* SafeNet products */
#define	PCI_PRODUCT_SAFENET_SAFEXCEL	0x1141		/* SafeXcel */

/* Samsung products */
#define	PCI_PRODUCT_SAMSUNG_SWL2210P	0xa000		/* MagicLAN SWL-2210P */

/* Schneider & Koch (SysKonnect) */
#define	PCI_PRODUCT_SCHNEIDERKOCH_FDDI	0x4000		/* FDDI */
#define	PCI_PRODUCT_SCHNEIDERKOCH_GE	0x4300		/* 984x GE */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2	0x4320		/* SK-9821 v2.0 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9D21	0x4400		/* SK-9D21 */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9Sxx	0x9000		/* SK-9Sxx */
#define	PCI_PRODUCT_SCHNEIDERKOCH_SK9Exx	0x9E00		/* SK-9Exx */

/* SGI products */
#define	PCI_PRODUCT_SGI_TIGON	0x0009		/* Tigon */

/* SGS Thomson products */
#define	PCI_PRODUCT_SGSTHOMSON_2000	0x0008		/* STG 2000X */
#define	PCI_PRODUCT_SGSTHOMSON_1764	0x0009		/* STG 1764 */
#define	PCI_PRODUCT_SGSTHOMSON_KYROII	0x0010		/* Kyro-II */
#define	PCI_PRODUCT_SGSTHOMSON_1764X	0x1746		/* STG 1764X */

/* Sigma Designs */
#define	PCI_PRODUCT_SIGMA_64GX	0x6401		/* 64GX */
#define	PCI_PRODUCT_SIGMA_DVDMAGICPRO	0x8300		/* DVDmagic-PRO */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 AGP */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 VGA */
#define	PCI_PRODUCT_SIS_648FX	0x0003		/* 648FX AGP */
#define	PCI_PRODUCT_SIS_86C205_1	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 System */
#define	PCI_PRODUCT_SIS_5595	0x0009		/* 5595 System */
#define	PCI_PRODUCT_SIS_85C503_ISA	0x0018		/* 85C503 ISA */
#define	PCI_PRODUCT_SIS_180	0x0180		/* 180 SATA */
#define	PCI_PRODUCT_SIS_181	0x0181		/* 181 SATA */
#define	PCI_PRODUCT_SIS_182	0x0182		/* 182 SATA */
#define	PCI_PRODUCT_SIS_5597_VGA	0x0200		/* 5597/5598 VGA */
#define	PCI_PRODUCT_SIS_6215	0x0204		/* 6215 */
#define	PCI_PRODUCT_SIS_86C205_2	0x0205		/* 86C205 */
#define	PCI_PRODUCT_SIS_300	0x0300		/* 300/305/630 VGA */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_85C596	0x0596		/* SiS596 */
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
#define	PCI_PRODUCT_SIS_650	0x0650		/* 650 PCI */
#define	PCI_PRODUCT_SIS_651	0x0651		/* 651 PCI */
#define	PCI_PRODUCT_SIS_652	0x0652		/* 652 PCI */
#define	PCI_PRODUCT_SIS_655	0x0655		/* 655 PCI */
#define	PCI_PRODUCT_SIS_658	0x0658		/* 658 PCI */
#define	PCI_PRODUCT_SIS_661	0x0661		/* 661 PCI */
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
#define	PCI_PRODUCT_SIS_760	0x0760		/* 760 PCI */
#define	PCI_PRODUCT_SIS_900	0x0900		/* 900 10/100BaseTX */
#define	PCI_PRODUCT_SIS_961	0x0961		/* 961 PCI */
#define	PCI_PRODUCT_SIS_962	0x0962		/* 962 PCI */
#define	PCI_PRODUCT_SIS_963	0x0963		/* 963 PCI */
#define	PCI_PRODUCT_SIS_964	0x0964		/* 964 PCI */
#define	PCI_PRODUCT_SIS_965	0x0965		/* 965 PCI */
#define	PCI_PRODUCT_SIS_5511	0x5511		/* 5511 */
#define	PCI_PRODUCT_SIS_5512	0x5512		/* 5512 */
#define	PCI_PRODUCT_SIS_5513	0x5513		/* 5513 EIDE */
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

/* SMC products */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC 37C665 */
#define	PCI_PRODUCT_SMC_37C922	0x1001		/* FDC 37C922 */
#define	PCI_PRODUCT_SMC_83C170	0x0005		/* 83C170 (EPIC/100) */
#define	PCI_PRODUCT_SMC_83C175	0x0006		/* 83C175 (EPIC/100) */

/* Silicon Motion products */
#define	PCI_PRODUCT_SMI_SM710	0x0710		/* LynxEM */
#define	PCI_PRODUCT_SMI_SM712	0x0712		/* LynxEM+ */
#define	PCI_PRODUCT_SMI_SM720	0x0720		/* Lynx3DM */
#define	PCI_PRODUCT_SMI_SM810	0x0810		/* LynxE */
#define	PCI_PRODUCT_SMI_SM811	0x0811		/* LynxE+ */
#define	PCI_PRODUCT_SMI_SM820	0x0820		/* Lynx3D */
#define	PCI_PRODUCT_SMI_SM910	0x0910		/* 910 */

/* SNC products */
#define	PCI_PRODUCT_SNI_PIRAHNA	0x0002		/* Pirahna 2-port */
#define	PCI_PRODUCT_SNI_TCPMSE	0x0005		/* Tulip, power, switch extender */
#define	PCI_PRODUCT_SNI_FPGAIBUS	0x4942		/* FPGA I-Bus Tracer for MBD */
#define	PCI_PRODUCT_SNI_SZB6120	0x6120		/* SZB6120 */

/* Sony products */
#define	PCI_PRODUCT_SONY_CXD1947A	0x8009		/* CXD1947A FireWire */
#define	PCI_PRODUCT_SONY_CXD3222	0x8039		/* CXD3222 FireWire */
#define	PCI_PRODUCT_SONY_MEMSTICK_SLOT	0x808a		/* Memory Stick Slot */

/* STB products */
#define	PCI_PRODUCT_STB2_RIVA128	0x0018		/* Velocity128 */

/* Sun */
#define	PCI_PRODUCT_SUN_EBUS	0x1000		/* PCIO Ebus2 */
#define	PCI_PRODUCT_SUN_HME	0x1001		/* HME */
#define	PCI_PRODUCT_SUN_EBUSIII	0x1100		/* PCIO Ebus2 (US III) */
#define	PCI_PRODUCT_SUN_ERINETWORK	0x1101		/* ERI Ether */
#define	PCI_PRODUCT_SUN_FIREWIRE	0x1102		/* FireWire */
#define	PCI_PRODUCT_SUN_USB	0x1103		/* USB */
#define	PCI_PRODUCT_SUN_GEMNETWORK	0x2bad		/* GEM */
#define	PCI_PRODUCT_SUN_SIMBA	0x5000		/* Simba PCI-PCI */
#define	PCI_PRODUCT_SUN_5821	0x5454		/* Crypto 5821 */
#define	PCI_PRODUCT_SUN_SCA1K	0x5455		/* Crypto 1K */
#define	PCI_PRODUCT_SUN_PSYCHO	0x8000		/* Psycho PCI */
#define	PCI_PRODUCT_SUN_MS_IIep	0x9000		/* microSPARC IIep PCI */
#define	PCI_PRODUCT_SUN_US_IIi	0xa000		/* UltraSPARC IIi PCI */
#define	PCI_PRODUCT_SUN_US_IIe	0xa001		/* UltraSPARC IIe PCI */
#define	PCI_PRODUCT_SUN_CE	0xabba		/* Cassini Ether */

/* Sundance products */
#define	PCI_PRODUCT_SUNDANCE_ST201	0x0201		/* ST201 */
#define	PCI_PRODUCT_SUNDANCE_ST1023	0x1023		/* ST1023 */
#define	PCI_PRODUCT_SUNDANCE_ST2021	0x2021		/* ST2021 */

/* Sunix */
#define	PCI_PRODUCT_SUNIX_40XX	0x7168		/* 40XX */
#define	PCI_PRODUCT_SUNIX_4018A	0x7268		/* 4018A */

/* Surecom products */
#define	PCI_PRODUCT_SURECOM_NE34	0x0e34		/* Surecom NE-34 */

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

/* Tamarack Microelectronics */
#define	PCI_PRODUCT_TAMARACK_TC9021	0x1021		/* TC9021 GigE */
#define	PCI_PRODUCT_TAMARACK_TC9021_ALT	0x9021		/* TC9021 GigE (alt ID) */

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
#define	PCI_PRODUCT_TI_TSB43AB21	0x8026		/* TSB43AB21 FireWire */
#define	PCI_PRODUCT_TI_PCI4451_FW	0x8027		/* PCI4451 FireWire */
#define	PCI_PRODUCT_TI_PCI4510_FW	0x8029		/* PCI4510 FireWire */
#define	PCI_PRODUCT_TI_PCI4520_FW	0x802A		/* PCI4520 FireWire */
#define	PCI_PRODUCT_TI_PCI7410_FW	0x802b		/* PCI7(4-6)10 FireWire */
#define	PCI_PRODUCT_TI_PCI7420_FW	0x802e		/* PCI7x20 FireWire */
#define	PCI_PRODUCT_TI_PCI7XX1	0x8031		/* PCI7XX1 CardBus */
#define	PCI_PRODUCT_TI_PCI7XX1_FW	0x8032		/* PCI7XX1 FireWire */
#define	PCI_PRODUCT_TI_PCI7XX1_FLASH	0x8033		/* PCI7XX1 Flash */
#define	PCI_PRODUCT_TI_PCI7XX1_SD	0x8034		/* PCI7XX1 Secure Data */
#define	PCI_PRODUCT_TI_PCI7XX1_SM	0x8035		/* PCI7XX1 Smart Card */
#define	PCI_PRODUCT_TI_PCI6515	0x8036		/* PCI6515 CardBus */
#define	PCI_PRODUCT_TI_PCI6515SC	0x8038		/* PCI6515 CardBus (Smart Card mode) */
#define	PCI_PRODUCT_TI_PCI1620_MISC	0x8201		/* PCI1620 Misc */
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
#define	PCI_PRODUCT_TIGERJET_TIGER320	0x0001		/* PCI interface */

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
#define	PCI_PRODUCT_TRANSMETA_NORTHBRIDGE	0x0395		/* Virtual Northbridge */
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

/* US Rebotics */
#define	PCI_PRODUCT_USR_3CP5610	0x1008		/* 3CP5610 */
#define	PCI_PRODUCT_USR2_USR997902	0x0116		/* USR997902 */
#define	PCI_PRODUCT_USR2_WL11000P	0x3685		/* WL11000P */

/* V3 Semiconductor products */
#define	PCI_PRODUCT_V3_V961PBC	0x0002		/* V961PBC i960 PCI */
#define	PCI_PRODUCT_V3_V292PBC	0x0292		/* V292PBC AMD290x0 PCI */
#define	PCI_PRODUCT_V3_V960PBC	0x0960		/* V960PBC i960 PCI */
#define	PCI_PRODUCT_V3_V96DPC	0xc960		/* V96DPC i960 PCI */

/* VIA Technologies products */
#define	PCI_PRODUCT_VIATECH_K8T890_0	0x0238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800_AGP	0x0259		/* PM800 AGP */
#define	PCI_PRODUCT_VIATECH_K8HTB_0	0x0282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8363	0x0305		/* VT8363 Host */
#define	PCI_PRODUCT_VIATECH_VT8371_HB	0x0391		/* VT8371 Host */
#define	PCI_PRODUCT_VIATECH_VT8501	0x0501		/* VT8501 */
#define	PCI_PRODUCT_VIATECH_VT82C505	0x0505		/* VT82C505 */
#define	PCI_PRODUCT_VIATECH_VT82C561	0x0561		/* VT82C561 */
#define	PCI_PRODUCT_VIATECH_VT82C571	0x0571		/* VT82C571 IDE */
#define	PCI_PRODUCT_VIATECH_VT82C576	0x0576		/* VT82C576 3V */
#define	PCI_PRODUCT_VIATECH_VT82C585	0x0585		/* VT82C585 ISA */
#define	PCI_PRODUCT_VIATECH_VT82C586_ISA	0x0586		/* VT82C586 ISA */
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
#define	PCI_PRODUCT_VIATECH_K8T890_1	0x1238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800_ERRS	0x1259		/* PM800 Errors */
#define	PCI_PRODUCT_VIATECH_K8HTB_1	0x1282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT82C416	0x1571		/* VT82C416 IDE */
#define	PCI_PRODUCT_VIATECH_VT82C1595	0x1595		/* VT82C1595 PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_2	0x2238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800	0x2259		/* PM800 Host */
#define	PCI_PRODUCT_VIATECH_K8HTB_2	0x2282		/* K8HTB Host */
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
#define	PCI_PRODUCT_VIATECH_VT8366_ISA	0x3074		/* VT8366 ISA */
#define	PCI_PRODUCT_VIATECH_VT8633	0x3091		/* VT8633 PCI */
#define	PCI_PRODUCT_VIATECH_VT8366	0x3099		/* VT8366 PCI */
#define	PCI_PRODUCT_VIATECH_VT8653_PCI	0x3101		/* VT8653 PCI */
#define	PCI_PRODUCT_VIATECH_VT6202	0x3104		/* VT6202 USB */
#define	PCI_PRODUCT_VIATECH_VT6105	0x3106		/* VT6105 RhineIII */
#define	PCI_PRODUCT_VIATECH_UNICHROME	0x3108		/* S3 Unichrome PRO IGP */
#define	PCI_PRODUCT_VIATECH_VT8361_PCI	0x3112		/* VT8361 PCI */
#define	PCI_PRODUCT_VIATECH_VT8375	0x3116		/* VT8375 PCI */
#define	PCI_PRODUCT_VIATECH_PM800_UNICHROME	0x3118		/* PM800 Unichrome S3 */
#define	PCI_PRODUCT_VIATECH_VT612x	0x3119		/* VT612x */
#define	PCI_PRODUCT_VIATECH_CLE266	0x3122		/* CLE266 */
#define	PCI_PRODUCT_VIATECH_VT8623	0x3123		/* VT8623 PCI */
#define	PCI_PRODUCT_VIATECH_VT8233_ISA	0x3147		/* VT8233 ISA */
#define	PCI_PRODUCT_VIATECH_VT8751	0x3148		/* VT8751 PCI */
#define	PCI_PRODUCT_VIATECH_VT8237_SATA	0x3149		/* VT8237 SATA */
#define	PCI_PRODUCT_VIATECH_VT8235_ISA	0x3177		/* VT8235 ISA */
#define	PCI_PRODUCT_VIATECH_K8HTB	0x3188		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8377	0x3189		/* VT8377 PCI */
#define	PCI_PRODUCT_VIATECH_K8M400	0x3204		/* K8M400 Host */
#define	PCI_PRODUCT_VIATECH_VT8378	0x3205		/* VT8378 PCI */
#define	PCI_PRODUCT_VIATECH_VT8237_ISA	0x3227		/* VT8237 ISA */
#define	PCI_PRODUCT_VIATECH_K8T890_3	0x3238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800_DRAM	0x3259		/* PM800 DRAM */
#define	PCI_PRODUCT_VIATECH_K8HTB_3	0x3282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_K8T890_4	0x4238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800_PMC	0x4259		/* PM800 PMC */
#define	PCI_PRODUCT_VIATECH_K8HTB_4	0x4282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_RHINEII	0x6100		/* RhineII */
#define	PCI_PRODUCT_VIATECH_VT8378_VGA	0x7205		/* VT8378 VGA */
#define	PCI_PRODUCT_VIATECH_K8T890_7	0x7238		/* K8T890 Host */
#define	PCI_PRODUCT_VIATECH_PM800_PCI	0x7259		/* PM800 PCI */
#define	PCI_PRODUCT_VIATECH_K8HTB_7	0x7282		/* K8HTB Host */
#define	PCI_PRODUCT_VIATECH_VT8231_ISA	0x8231		/* VT8231 ISA */
#define	PCI_PRODUCT_VIATECH_VT8231_PWR	0x8235		/* VT8231 PMG */
#define	PCI_PRODUCT_VIATECH_VT8363_AGP	0x8305		/* VT8363 AGP */
#define	PCI_PRODUCT_VIATECH_VT8371_PPB	0x8391		/* VT8371 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8501_AGP	0x8501		/* VT8501 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C597AGP	0x8597		/* VT82C597 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C598AGP	0x8598		/* VT82C598 AGP */
#define	PCI_PRODUCT_VIATECH_VT82C601	0x8601		/* VT82C601 AGP */
#define	PCI_PRODUCT_VIATECH_VT8605_AGP	0x8605		/* VT8605 AGP */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_A	0xa238		/* K8T890 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_VT8633_AGP	0xb091		/* VT8633 AGP */
#define	PCI_PRODUCT_VIATECH_VT8366_AGP	0xb099		/* VT8366 AGP */
#define	PCI_PRODUCT_VIATECH_VT8361_AGP	0xb112		/* VT8361 AGP */
#define	PCI_PRODUCT_VIATECH_VT8235_AGP	0xb168		/* VT8235 AGP */
#define	PCI_PRODUCT_VIATECH_K8HTB_AGP	0xb188		/* K8HTB AGP */
#define	PCI_PRODUCT_VIATECH_VT8377_AGP	0xb198		/* VT8377 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_C	0xc238		/* K8T390 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_D	0xd238		/* K8T390 PCI-PCI */
#define	PCI_PRODUCT_VIATECH_K8T890_PPB_E	0xe238		/* K8T390 PCI-PCI */

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
#define	PCI_PRODUCT_VMWARE_VIRTUAL	0x0710		/* Virtual SVGA */
#define	PCI_PRODUCT_VMWARE_VIRTUAL2	0x0405		/* Virtual SVGA II */

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

/* Xircom products */
#define	PCI_PRODUCT_XIRCOM_X3201_3	0x0002		/* X3201-3 */
#define	PCI_PRODUCT_XIRCOM_X3201_3_21143	0x0003		/* X3201-3 (21143) */
#define	PCI_PRODUCT_XIRCOM_MPCI_MODEM	0x000c		/* Modem */
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
