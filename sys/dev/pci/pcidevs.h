/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: pcidevs,v 1.16 1997/03/20 20:29:55 chuck Exp 
 */
/*	$NetBSD: pcidevs,v 1.20 1996/10/19 13:01:49 jonathan Exp $	*/


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
 * List of known PCI vendors
 */

#define	PCI_VENDOR_OLDCOMPAQ	0x0e11		/* Compaq (Old ID. see "COMPAQ" below.) */
#define	PCI_VENDOR_OLDNCR	0x1000		/* NCR (Old ID. see "NCR" below.) */
#define	PCI_VENDOR_ATI	0x1002		/* ATI Technologies */
#define	PCI_VENDOR_ULSI	0x1003		/* ULSI Systems */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI Technology */
#define	PCI_VENDOR_AVANCE	0x1005		/* Avance Logic */
#define	PCI_VENDOR_REPLY	0x1006		/* Reply Group */
#define	PCI_VENDOR_NETFRAME	0x1007		/* NetFrame Systems */
#define	PCI_VENDOR_EPSON	0x1008		/* Epson */
#define	PCI_VENDOR_PHOENIX	0x100a		/* Phoenix Technologies */
#define	PCI_VENDOR_NS	0x100b		/* National Semiconductor */
#define	PCI_VENDOR_TSENG	0x100c		/* Tseng Labs */
#define	PCI_VENDOR_AST	0x100d		/* AST Research */
#define	PCI_VENDOR_WEITEK	0x100e		/* Weitek */
#define	PCI_VENDOR_VIDEOLOGIC	0x1010		/* Video Logic, Ltd. */
#define	PCI_VENDOR_DEC	0x1011		/* Digital Equipment */
#define	PCI_VENDOR_MICRONICS	0x1012		/* Micronics Computers */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_ICLPERSONAL	0x1016		/* ICL Personal Systems */
#define	PCI_VENDOR_SPEA	0x1017		/* SPEA Software */
#define	PCI_VENDOR_UNISYS	0x1018		/* Unisys Systems */
#define	PCI_VENDOR_ELITEGROUP	0x1019		/* Elitegroup Computer Systems */
#define	PCI_VENDOR_NCR	0x101a		/* NCR */
#define	PCI_VENDOR_VITESSE	0x101b		/* Vitesse Semiconductor */
#define	PCI_VENDOR_WD	0x101c		/* Western Digital */
#define	PCI_VENDOR_AMI	0x101e		/* American Megatrends */
#define	PCI_VENDOR_PICTURETEL	0x101f		/* PictureTel */
#define	PCI_VENDOR_HITACHICOMP	0x1020		/* Hitachi Computer Products */
#define	PCI_VENDOR_OKI	0x1021		/* OKI Electric Industry */
#define	PCI_VENDOR_AMD	0x1022		/* AMD */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident Microsystems */
#define	PCI_VENDOR_ZENITH	0x1024		/* Zenith Data Systems */
#define	PCI_VENDOR_ACER	0x1025		/* Acer */
#define	PCI_VENDOR_DELL	0x1028		/* Dell Computer */
#define	PCI_VENDOR_SIEMENS	0x1029		/* Siemens Nixdorf IS */
#define	PCI_VENDOR_LSILOGIC	0x102a		/* LSI Logic, Headland div. */
#define	PCI_VENDOR_MATROX	0x102b		/* Matrox */
#define	PCI_VENDOR_CHIPS	0x102c		/* Chips and Technologies */
#define	PCI_VENDOR_WYSE	0x102d		/* WYSE Technology */
#define	PCI_VENDOR_OLIVETTI	0x102e		/* Olivetti Advanced Technology */
#define	PCI_VENDOR_TOSHIBA	0x102f		/* Toshiba America */
#define	PCI_VENDOR_TMCRESEARCH	0x1030		/* TMC Research */
#define	PCI_VENDOR_MIRO	0x1031		/* Miro Computer Products */
#define	PCI_VENDOR_COMPAQ	0x1032		/* Compaq */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_BURNDY	0x1034		/* Burndy */
#define	PCI_VENDOR_COMPCOMM	0x1035		/* Comp. & Comm. Research Lab */
#define	PCI_VENDOR_FUTUREDOMAIN	0x1036		/* Future Domain */
#define	PCI_VENDOR_HITACHIMICRO	0x1037		/* Hitach Microsystems */
#define	PCI_VENDOR_AMP	0x1038		/* AMP */
#define	PCI_VENDOR_SIS	0x1039		/* Silicon Integrated System */
#define	PCI_VENDOR_SEIKOEPSON	0x103a		/* Seiko Epson */
#define	PCI_VENDOR_TATUNGAMERICA	0x103b		/* Tatung Co. of America */
#define	PCI_VENDOR_HP	0x103C		/* Hewlett-Packard */
#define	PCI_VENDOR_SOLLIDAY	0x103e		/* Solliday Engineering */
#define	PCI_VENDOR_LOGICMODELLING	0x103f		/* Logic Modeling */
#define	PCI_VENDOR_KPC	0x1040		/* Kubota Pacific */
#define	PCI_VENDOR_COMPUTREND	0x1041		/* Computrend */
#define	PCI_VENDOR_PCTECH	0x1042		/* PC Technology */
#define	PCI_VENDOR_ASUSTEK	0x1043		/* Asustek Computer */
#define	PCI_VENDOR_DPT	0x1044		/* Distributed Processing Technology */
#define	PCI_VENDOR_OPTI	0x1045		/* Opti */
#define	PCI_VENDOR_IPCCORP	0x1046		/* IPC Corporation */
#define	PCI_VENDOR_GENOA	0x1047		/* Genoa Systems */
#define	PCI_VENDOR_ELSA	0x1048		/* Elsa */
#define	PCI_VENDOR_FOUNTAINTECH	0x1049		/* Fountain Technology */
#define	PCI_VENDOR_SGSTHOMSON	0x104a		/* SGS Thomson Microelectric */
#define	PCI_VENDOR_BUSLOGIC	0x104b		/* BusLogic */
#define	PCI_VENDOR_TI	0x104C		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104D		/* Sony */
#define	PCI_VENDOR_OAKTECH	0x104e		/* Oak Technology */
#define	PCI_VENDOR_COTIME	0x104f		/* Co-time Computer */
#define	PCI_VENDOR_WINBOND	0x1050		/* Winbond Electronics */
#define	PCI_VENDOR_ANIGMA	0x1051		/* Anigma */
#define	PCI_VENDOR_YOUNGMICRO	0x1052		/* Young Micro Systems */
#define	PCI_VENDOR_HITACHI	0x1054		/* Hitachi */
#define	PCI_VENDOR_EFARMICRO	0x1055		/* Efar Microsystems */
#define	PCI_VENDOR_ICL	0x1056		/* ICL */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_ETR	0x1058		/* Electronics & Telec. RSH */
#define	PCI_VENDOR_TEKNOR	0x1059		/* Teknor Microsystems */
#define	PCI_VENDOR_PROMISE	0x105a		/* Promise Technology */
#define	PCI_VENDOR_FOXCONN	0x105b		/* Foxconn International */
#define	PCI_VENDOR_WIPRO	0x105c		/* Wipro Infotech */
#define	PCI_VENDOR_NUMBER9	0x105d		/* Number 9 Computer Company */
#define	PCI_VENDOR_VTECH	0x105e		/* Vtech Computers */
#define	PCI_VENDOR_INFOTRONIC	0x105f		/* Infotronic America */
#define	PCI_VENDOR_UMC	0x1060		/* United Microelectronics */
#define	PCI_VENDOR_ITT	0x1061		/* I. T. T. (or X technology?) */
#define	PCI_VENDOR_MASPAR	0x1062		/* Maspar Computer */
#define	PCI_VENDOR_OCEANOA	0x1063		/* Ocean Office Automation */
#define	PCI_VENDOR_ALCATEL	0x1064		/* Alcatel CIT */
#define	PCI_VENDOR_TEXASMICRO	0x1065		/* Texas Microsystems */
#define	PCI_VENDOR_PICOPOWER	0x1066		/* Picopower Technology */
#define	PCI_VENDOR_MITSUBISHI	0x1067		/* Mitsubishi */
#define	PCI_VENDOR_DIVERSIFIED	0x1068		/* Diversified Technology */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_ATEN	0x106a		/* Aten Research */
#define	PCI_VENDOR_APPLE	0x106B		/* Apple */
#define	PCI_VENDOR_HYUNDAI	0x106c		/* Hyundai Electronics America */
#define	PCI_VENDOR_SEQUENT	0x106d		/* Sequent */
#define	PCI_VENDOR_DFI	0x106e		/* DFI */
#define	PCI_VENDOR_CITYGATE	0x106f		/* City Gate Development */
#define	PCI_VENDOR_DAEWOO	0x1070		/* Daewoo Telecom */
#define	PCI_VENDOR_MITAC	0x1071		/* Mitac */
#define	PCI_VENDOR_GIT	0x1072		/* GIT Co. */
#define	PCI_VENDOR_YAMAHA	0x1073		/* Yamaha */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen Microsystems */
#define	PCI_VENDOR_AIR	0x1075		/* Advanced Integration Research */
#define	PCI_VENDOR_CHAINTECH	0x1076		/* Chaintech Computer */
#define	PCI_VENDOR_QLOGIC	0x1077		/* Q Logic */
#define	PCI_VENDOR_CYRIX	0x1078		/* Cyrix Corporation */
#define	PCI_VENDOR_IBUS	0x1079		/* I-Bus */
#define	PCI_VENDOR_NETWORTH	0x107a		/* NetWorth */
#define	PCI_VENDOR_GATEWAY	0x107b		/* Gateway 2000 */
#define	PCI_VENDOR_GOLDSTART	0x107c		/* Goldstar */
#define	PCI_VENDOR_LEADTEK	0x107d		/* LeadTek Research */
#define	PCI_VENDOR_INTERPHASE	0x107e		/* Interphase */
#define	PCI_VENDOR_DATATECH	0x107f		/* Data Technology Corporation */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq Microsystems */
#define	PCI_VENDOR_SUPERMAC	0x1081		/* Supermac Technology */
#define	PCI_VENDOR_EFA	0x1082		/* EFA Corporation of America */
#define	PCI_VENDOR_FOREX	0x1083		/* Forex Computer */
#define	PCI_VENDOR_PARADOR	0x1084		/* Parador */
#define	PCI_VENDOR_TULIP	0x1085		/* Tulip Computers */
#define	PCI_VENDOR_JBOND	0x1086		/* J. Bond Computer Systems */
#define	PCI_VENDOR_CACHECOMP	0x1087		/* Cache Computer */
#define	PCI_VENDOR_MICROCOMP	0x1088		/* Microcomputer Systems */
#define	PCI_VENDOR_DG	0x1089		/* Data General Corporation */
#define	PCI_VENDOR_BIT3	0x108A		/* Bit3 Computer Corp. */
#define	PCI_VENDOR_OAKLEIGH	0x108c		/* Oakleigh Systems */
#define	PCI_VENDOR_OLICOM	0x108d		/* Olicom */
#define	PCI_VENDOR_SYSTEMSOFT	0x108f		/* Systemsoft */
#define	PCI_VENDOR_ENCORE	0x1090		/* Encore Computer */
#define	PCI_VENDOR_INTERGRAPH	0x1091		/* Intergraph */
#define	PCI_VENDOR_DIAMOND	0x1092		/* Diamond Computer Systems */
#define	PCI_VENDOR_NATIONALINST	0x1093		/* National Instruments */
#define	PCI_VENDOR_FICOMP	0x1094		/* First Int'l Computers */
#define	PCI_VENDOR_CMDTECH	0x1095		/* CMD Technology */
#define	PCI_VENDOR_ALACRON	0x1096		/* Alacron */
#define	PCI_VENDOR_APPIAN	0x1097		/* Appian Technology */
#define	PCI_VENDOR_QUANTUMDESIGNS	0x1098		/* Quantum Designs (or Vision?) */
#define	PCI_VENDOR_SAMSUNGELEC	0x1099		/* Samsung Electronics */
#define	PCI_VENDOR_PACKARDBELL	0x109a		/* Packard Bell */
#define	PCI_VENDOR_GEMLIGHT	0x109b		/* Gemlight Computer */
#define	PCI_VENDOR_MEGACHIPS	0x109c		/* Megachips */
#define	PCI_VENDOR_ZIDA	0x109d		/* Zida Technologies */
#define	PCI_VENDOR_BROOKTREE	0x109e		/* Brooktree */
#define	PCI_VENDOR_TRIGEM	0x109f		/* Trigem Computer */
#define	PCI_VENDOR_MEIDENSHA	0x10a0		/* Meidensha */
#define	PCI_VENDOR_JUKO	0x10a1		/* Juko Electronics */
#define	PCI_VENDOR_QUANTUM	0x10a2		/* Quantum */
#define	PCI_VENDOR_EVEREX	0x10a3		/* Everex Systems */
#define	PCI_VENDOR_GLOBE	0x10a4		/* Globe Manufacturing Sales */
#define	PCI_VENDOR_RACAL	0x10a5		/* Racal Interlan */
#define	PCI_VENDOR_INFORMTECH	0x10a6		/* Informtech Industrial */
#define	PCI_VENDOR_BENCHMARQ	0x10a7		/* Benchmarq Microelectronics */
#define	PCI_VENDOR_SIERRA	0x10a8		/* Sierra Semiconductor */
#define	PCI_VENDOR_SGI	0x10a9		/* Silicon Graphics */
#define	PCI_VENDOR_ACC	0x10aa		/* ACC Microelectronics */
#define	PCI_VENDOR_DIGICOM	0x10ab		/* Digicom */
#define	PCI_VENDOR_HONEYWELL	0x10ac		/* Honeywell IASD */
#define	PCI_VENDOR_CORNERSTONE	0x10ae		/* Cornerstone Technology */
#define	PCI_VENDOR_MICROCOMPSON	0x10af		/* Micro Computer Sysytems (M) SON */
#define	PCI_VENDOR_CARDEXPER	0x10b0		/* CardExpert Technology */
#define	PCI_VENDOR_CABLETRON	0x10B1		/* Cabletron Systems */
#define	PCI_VENDOR_RAYETHON	0x10b2		/* Raytheon */
#define	PCI_VENDOR_DATABOOK	0x10b3		/* Databook */
#define	PCI_VENDOR_STB	0x10b4		/* STB Systems */
#define	PCI_VENDOR_PLX	0x10b5		/* PLX Technology */
#define	PCI_VENDOR_MADGE	0x10b6		/* Madge Networks */
#define	PCI_VENDOR_3COM	0x10b7		/* 3Com */
#define	PCI_VENDOR_SMC	0x10b8		/* Standard Microsystems */
#define	PCI_VENDOR_ALI	0x10b9		/* Acer Labs */
#define	PCI_VENDOR_MITSUBISHIELEC	0x10ba		/* Mitsubishi Electronics */
#define	PCI_VENDOR_DAPHA	0x10bb		/* Dapha Electronics */
#define	PCI_VENDOR_ALR	0x10bc		/* Advanced Logic Research */
#define	PCI_VENDOR_SURECOM	0x10bd		/* Surecom Technology */
#define	PCI_VENDOR_TSENGLABS	0x10be		/* Tseng Labs International */
#define	PCI_VENDOR_MOST	0x10bf		/* Most */
#define	PCI_VENDOR_BOCA	0x10c0		/* Boca Research */
#define	PCI_VENDOR_ICM	0x10c1		/* ICM */
#define	PCI_VENDOR_AUSPEX	0x10c2		/* Auspex Systems */
#define	PCI_VENDOR_SAMSUNGSEMI	0x10c3		/* Samsung Semiconductors */
#define	PCI_VENDOR_AWARD	0x10c4		/* Award Software Int'l */
#define	PCI_VENDOR_XEROX	0x10c5		/* Xerox */
#define	PCI_VENDOR_RAMBUS	0x10c6		/* Rambus */
#define	PCI_VENDOR_MEDIAVIS	0x10c7		/* Media Vision */
#define	PCI_VENDOR_NEOMAGIC	0x10c8		/* Neomagic */
#define	PCI_VENDOR_DATAEXPERT	0x10c9		/* Dataexpert */
#define	PCI_VENDOR_FUJITSU	0x10ca		/* Fujitsu */
#define	PCI_VENDOR_OMRON	0x10cb		/* Omron */
#define	PCI_VENDOR_MENTOR	0x10cc		/* Mentor ARC */
#define	PCI_VENDOR_ADVSYSPROD	0x10cd		/* Advanced System Products */
#define	PCI_VENDOR_RADIUS	0x10ce		/* Radius */
#define	PCI_VENDOR_CITICORP	0x10cf		/* Citicorp TTI */
#define	PCI_VENDOR_FUJUTSU	0x10d0		/* Fujitsu Limited */
#define	PCI_VENDOR_FUTUREPLUS	0x10d1		/* Future+ Systems */
#define	PCI_VENDOR_MOLEX	0x10d2		/* Molex */
#define	PCI_VENDOR_JABIL	0x10d3		/* Jabil Circuit */
#define	PCI_VENDOR_HAULON	0x10d4		/* Hualon Microelectronics */
#define	PCI_VENDOR_AUTOLOGIC	0x10d5		/* Autologic */
#define	PCI_VENDOR_CETIA	0x10d6		/* Cetia */
#define	PCI_VENDOR_BCM	0x10d7		/* BCM Advanced */
#define	PCI_VENDOR_APL	0x10d8		/* Advanced Peripherals Labs */
#define	PCI_VENDOR_MACRONIX	0x10d9		/* Macronix */
#define	PCI_VENDOR_THOMASCONRAD	0x10da		/* Thomas-Conrad */
#define	PCI_VENDOR_ROHM	0x10db		/* Rohm Research */
#define	PCI_VENDOR_CERN	0x10dc		/* CERN/ECP/EDU */
#define	PCI_VENDOR_ES	0x10dd		/* Evans & Sutherland */
#define	PCI_VENDOR_NVIDIA	0x10de		/* Nvidia Corporation */
#define	PCI_VENDOR_EMULEX	0x10df		/* Emulex */
#define	PCI_VENDOR_IMS	0x10e0		/* Integrated Micro Solutions */
#define	PCI_VENDOR_APTIX	0x10e2		/* Aptix Corporation */
#define	PCI_VENDOR_NEWBRIDGE	0x10e3		/* Newbridge Microsystems */
#define	PCI_VENDOR_TANDEM	0x10e4		/* Tandem Computers */
#define	PCI_VENDOR_MICROINDUSTRIES	0x10e5		/* Micro Industries */
#define	PCI_VENDOR_GAINBERY	0x10e6		/* Gainbery Computer Products */
#define	PCI_VENDOR_VADEM	0x10e7		/* Vadem */
#define	PCI_VENDOR_AMCIRCUITS	0x10e8		/* Applied Micro Circuits */
#define	PCI_VENDOR_ALPSELECTIC	0x10e9		/* Alps Electric */
#define	PCI_VENDOR_INTERGRAPHICS	0x10ea		/* Integraphics Systems */
#define	PCI_VENDOR_ARTISTSGRAPHICS	0x10eb		/* Artists Graphics */
#define	PCI_VENDOR_REALTEK	0x10ec		/* Realtek Semiconductor */
#define	PCI_VENDOR_ASCIICORP	0x10ed		/* ASCII Corporation */
#define	PCI_VENDOR_XILINX	0x10ee		/* Xilinx */
#define	PCI_VENDOR_RACORE	0x10ef		/* Racore Computer Products */
#define	PCI_VENDOR_PERITEK	0x10f0		/* Peritek */
#define	PCI_VENDOR_TYAN	0x10f1		/* Tyan Computer */
#define	PCI_VENDOR_ACHME	0x10f2		/* Achme Computer */
#define	PCI_VENDOR_ALARIS	0x10f3		/* Alaris */
#define	PCI_VENDOR_SMOS	0x10f4		/* S-MOS Systems */
#define	PCI_VENDOR_MKK	0x10f5		/* NKK Corporation */
#define	PCI_VENDOR_CREATIVE	0x10f6		/* Creative Electronic Systems */
#define	PCI_VENDOR_MATSUSHITA	0x10f7		/* Matsushita */
#define	PCI_VENDOR_ALTOS	0x10f8		/* Altos India */
#define	PCI_VENDOR_PCDIRECT	0x10f9		/* PC Direct */
#define	PCI_VENDOR_TRUEVISIO	0x10fa		/* Truevision */
#define	PCI_VENDOR_THESYS	0x10fb		/* Thesys Ges. F. Mikroelektronik */
#define	PCI_VENDOR_IODATA	0x10fc		/* I-O Data Device */
#define	PCI_VENDOR_SOYO	0x10fd		/* Soyo Technology */
#define	PCI_VENDOR_FAST	0x10fe		/* Fast Electronic */
#define	PCI_VENDOR_NCUBE	0x10ff		/* NCube */
#define	PCI_VENDOR_JAZZ	0x1100		/* Jazz Multimedia */
#define	PCI_VENDOR_INITIO	0x1101		/* Initio */
#define	PCI_VENDOR_CREATIVELABS	0x1102		/* Creative Labs */
#define	PCI_VENDOR_TRIONES	0x1103		/* Triones Technologies */
#define	PCI_VENDOR_RASTEROPS	0x1104		/* RasterOps */
#define	PCI_VENDOR_SIGMA	0x1105		/* Sigma Designs */
#define	PCI_VENDOR_VIATECH	0x1106		/* VIA Technologies */
#define	PCI_VENDOR_STRATIS	0x1107		/* Stratus Computer */
#define	PCI_VENDOR_PROTEON	0x1108		/* Proteon */
#define	PCI_VENDOR_COGENT	0x1109		/* Cogent Data Technologies */
#define	PCI_VENDOR_XENON	0x110b		/* Xenon Microsystems */
#define	PCI_VENDOR_MINIMAX	0x110c		/* Mini-Max Technology */
#define	PCI_VENDOR_ZNYX	0x110d		/* Znyx Advanced Systems */
#define	PCI_VENDOR_CPUTECH	0x110e		/* CPU Technology */
#define	PCI_VENDOR_ROSS	0x110f		/* Ross Technology */
#define	PCI_VENDOR_POWERHOUSE	0x1110		/* Powerhouse Systems */
#define	PCI_VENDOR_SCO	0x1111		/* Santa Cruz Operation */
#define	PCI_VENDOR_ROCKWELL	0x1112		/* Rockwell Network Systems */
#define	PCI_VENDOR_ACCTON	0x1113		/* Accton Technology */
#define	PCI_VENDOR_ATMEL	0x1114		/* Atmel */
#define	PCI_VENDOR_3DLABS	0x1115		/* 3D Labs */
#define	PCI_VENDOR_DATATRANSLATION	0x1116		/* Data Translation */
#define	PCI_VENDOR_DATACUBE	0x1117		/* Datacube */
#define	PCI_VENDOR_BERG	0x1118		/* Berg Electronics */
#define	PCI_VENDOR_VORTEX	0x1119		/* Vortex Computer Systems */
#define	PCI_VENDOR_EFFICIENTNETS	0x111a		/* Efficent Networks */
#define	PCI_VENDOR_TELEDYNE	0x111b		/* Teledyne Electronic Systems */
#define	PCI_VENDOR_TRICORD	0x111c		/* Tricord Systems */
#define	PCI_VENDOR_IDT	0x111d		/* IDT */
#define	PCI_VENDOR_ELDEC	0x111e		/* Eldec */
#define	PCI_VENDOR_PDI	0x111f		/* Prescision Digital Images */
#define	PCI_VENDOR_EMC	0x1120		/* Emc */
#define	PCI_VENDOR_ZILOG	0x1121		/* Zilog */
#define	PCI_VENDOR_MULTITECH	0x1122		/* Multi-tech Systems */
#define	PCI_VENDOR_LEUTRON	0x1124		/* Leutron Vision */
#define	PCI_VENDOR_EUROCORE	0x1125		/* Eurocore */
#define	PCI_VENDOR_VIGRA	0x1125		/* Vigra */
#define	PCI_VENDOR_FORE	0x1127		/* FORE Systems */
#define	PCI_VENDOR_FIRMWORKS	0x1129		/* Firmworks */
#define	PCI_VENDOR_HERMES	0x112a		/* Hermes Electronics */
#define	PCI_VENDOR_LINOTYPE	0x112b		/* Linotype */
#define	PCI_VENDOR_RAVICAD	0x112d		/* Ravicad */
#define	PCI_VENDOR_INFOMEDIA	0x112e		/* Infomedia Microelectronics */
#define	PCI_VENDOR_IMAGINGTECH	0x112f		/* Imaging Technlogy */
#define	PCI_VENDOR_COMPUTERVISION	0x1130		/* Computervision */
#define	PCI_VENDOR_PHILIPS	0x1131		/* Philips */
#define	PCI_VENDOR_MITEL	0x1132		/* Mitel */
#define	PCI_VENDOR_EICON	0x1133		/* Eicon Technology */
#define	PCI_VENDOR_MCS	0x1134		/* Mercury Computer Systems */
#define	PCI_VENDOR_FUJIXEROX	0x1135		/* Fuji Xerox */
#define	PCI_VENDOR_MOMENTUM	0x1136		/* Momentum Data Systems */
#define	PCI_VENDOR_CISCO	0x1137		/* Cisco Systems */
#define	PCI_VENDOR_ZIATECH	0x1138		/* Ziatech */
#define	PCI_VENDOR_DYNPIC	0x1139		/* Dynamic Pictures */
#define	PCI_VENDOR_FWB	0x113a		/* FWB */
#define	PCI_VENDOR_CYCLONE	0x113c		/* Cyclone Micro */
#define	PCI_VENDOR_LEADINGEDGE	0x113d		/* Leading Edge */
#define	PCI_VENDOR_SANYO	0x113e		/* Sanyo Electric */
#define	PCI_VENDOR_EQUINOX	0x113f		/* Equinox Systems */
#define	PCI_VENDOR_INTERVOICE	0x1140		/* Intervoice */
#define	PCI_VENDOR_CREST	0x1141		/* Crest Microsystem */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance Semiconductor */
#define	PCI_VENDOR_NETPOWER	0x1143		/* NetPower */
#define	PCI_VENDOR_CINMILACRON	0x1144		/* Cincinnati Milacron */
#define	PCI_VENDOR_WORKBIT	0x1145		/* Workbit */
#define	PCI_VENDOR_FORCE	0x1146		/* Force Computers */
#define	PCI_VENDOR_INTERFACE	0x1147		/* Interface */
#define	PCI_VENDOR_SCHNEIDERKOCH	0x1148		/* Schneider & Koch */
#define	PCI_VENDOR_WINSYSTEM	0x1149		/* Win System */
#define	PCI_VENDOR_VMIC	0x114a		/* VMIC */
#define	PCI_VENDOR_CANOPUS	0x114b		/* Canopus */
#define	PCI_VENDOR_ANNABOOKS	0x114c		/* Annabooks */
#define	PCI_VENDOR_IC	0x114d		/* IC Corporation */
#define	PCI_VENDOR_NIKON	0x114e		/* Nikon Systems */
#define	PCI_VENDOR_DIGIINTERNAT	0x114f		/* Digi International */
#define	PCI_VENDOR_TMC	0x1150		/* Thinking Machines */
#define	PCI_VENDOR_JAE	0x1151		/* JAE Electronics */
#define	PCI_VENDOR_MEGATEK	0x1152		/* Megatek */
#define	PCI_VENDOR_LANDWIN	0x1153		/* Land Win Electronic */
#define	PCI_VENDOR_MELCO	0x1154		/* Melco */
#define	PCI_VENDOR_PINETECH	0x1155		/* Pine Technology */
#define	PCI_VENDOR_PERISCOPE	0x1156		/* Periscope Engineering */
#define	PCI_VENDOR_AVSYS	0x1157		/* Avsys */
#define	PCI_VENDOR_VOARX	0x1158		/* Voarx R & D */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_HARLEQUIN	0x115a		/* Harlequin */
#define	PCI_VENDOR_PARALLAX	0x115b		/* Parallax Graphics */
#define	PCI_VENDOR_XIRCOM	0x115d		/* Xircom */
#define	PCI_VENDOR_PEERPROTO	0x115e		/* Peer Protocols */
#define	PCI_VENDOR_MAXTOR	0x115f		/* Maxtor */
#define	PCI_VENDOR_MEGASOFT	0x1160		/* Megasoft */
#define	PCI_VENDOR_PFU	0x1161		/* PFU Limited */
#define	PCI_VENDOR_OALAB	0x1162		/* OA Laboratory */
#define	PCI_VENDOR_SYNEMA	0x1163		/* Synema Corporation */
#define	PCI_VENDOR_APT	0x1164		/* Advanced Peripherals Technologies */
#define	PCI_VENDOR_IMAGRAPH	0x1165		/* Imagraph */
#define	PCI_VENDOR_PEQUR	0x1166		/* Pequr Technology */
#define	PCI_VENDOR_MUTOH	0x1167		/* Mutoh Industries */
#define	PCI_VENDOR_THINE	0x1168		/* Thine Electronics */
#define	PCI_VENDOR_CDAC	0x1169		/* Centre for Dev. of Advanced Computing */
#define	PCI_VENDOR_POLARIS	0x116a		/* Polaris Communications */
#define	PCI_VENDOR_CONNECTWARE	0x116b		/* Connectware */
#define	PCI_VENDOR_MARTINMARIETTA	0x116d		/* Martin-Marietta */
#define	PCI_VENDOR_WSTECH	0x116f		/* Workstation Technology */
#define	PCI_VENDOR_INVENTEC	0x1170		/* Inventec */
#define	PCI_VENDOR_ZEITNET	0x1193		/* ZeitNet */
#define	PCI_VENDOR_SPECIALIX	0x11cb		/* Specialix */
#define	PCI_VENDOR_CYCLADES	0x120e		/* Cyclades */
#define	PCI_VENDOR_ZEINET	0x1193		/* Zeinet */
#define	PCI_VENDOR_SYMPHONY	0x1c1c		/* Symphony Labs */
#define	PCI_VENDOR_TEKRAM	0x1de1		/* Tekram Technology */
#define	PCI_VENDOR_AVANCE2	0x4005		/* Avance Logic (mistyped? see 0x1005) */
#define	PCI_VENDOR_S3	0x5333		/* S3 */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ATRONICS	0x907f		/* Atronics */
#define	PCI_VENDOR_NETPOWERNEW	0xdead		/* NetPower */
#define	PCI_VENDOR_ARK	0xedd8		/* Ark Logic (or Arc? or Hercules?) */

/*
 * List of known products.  Grouped by vendor.
 */

/* 3COM Products */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 10Mbps */
#define	PCI_PRODUCT_3COM_3C595TX	0x5950		/* 3c595 100Base-TX */
#define	PCI_PRODUCT_3COM_3C595T4	0x5951		/* 3c595 100Base-T4 */
#define	PCI_PRODUCT_3COM_3C595MII	0x5952		/* 3c595 10Mbps-MII */
#define	PCI_PRODUCT_3COM_3C900TPO	0x9000		/* 3c900 10Base-T */
#define	PCI_PRODUCT_3COM_3C900COMBO	0x9001		/* 3c900 10Mbps-Combo */
#define	PCI_PRODUCT_3COM_3C905TX	0x9050		/* 3c905 100Base-TX */
#define	PCI_PRODUCT_3COM_3C905T4	0x9051		/* 3c905 100Base-T4 */

/* Acer Labs products */
#define	PCI_PRODUCT_ALI_M1445	0x1445		/* M1445 */
#define	PCI_PRODUCT_ALI_M1449	0x1449		/* M1449 */
#define	PCI_PRODUCT_ALI_M1451	0x1451		/* M1451 */
#define	PCI_PRODUCT_ALI_M4803	0x5215		/* M4803 */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC7850	0x5078		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7855	0x5578		/* AIC-7855 */
#define	PCI_PRODUCT_ADP_AIC5900	0x5900		/* AIC-5900 ATM */
#define	PCI_PRODUCT_ADP_AIC5905	0x5905		/* AIC-5905 ATM */
#define	PCI_PRODUCT_ADP_AIC7860	0x6078		/* AIC-7860 */
#define	PCI_PRODUCT_ADP_2940AU	0x6178		/* AHA-2940A Ultra */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_2940	0x7178		/* AHA-2940 */
#define	PCI_PRODUCT_ADP_3940	0x7278		/* AHA-3940 */
#define	PCI_PRODUCT_ADP_2944	0x7478		/* AHA-2944 */
#define	PCI_PRODUCT_ADP_AIC7880	0x8078		/* AIC-7880 Ultra */
#define	PCI_PRODUCT_ADP_2940U	0x8178		/* AHA-2940 Ultra */
#define	PCI_PRODUCT_ADP_3940U	0x8278		/* AHA-3940 Ultra */
#define	PCI_PRODUCT_ADP_2944U	0x8478		/* AHA-2944 Ultra */

/* AMD products */
#define	PCI_PRODUCT_AMD_PCNET_PCI	0x2000		/* 79c970 PCnet-PCI LANCE Ethernet */
#define	PCI_PRODUCT_AMD_PCSCSI_PCI	0x2020		/* 53c974 PCscsi-PCI SCSI */

/* ARK Logic products */
#define	PCI_PRODUCT_ARK_1000PV	0xa091		/* 1000PV */
#define	PCI_PRODUCT_ARK_2000PV	0xa099		/* 2000PV */

/* ATI products */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_MACH64_CT	0x4354		/* Mach64 CT */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64 CX */
#define	PCI_PRODUCT_ATI_MACH64_ET	0x4554		/* Mach64 ET */
#define	PCI_PRODUCT_ATI_MACH64_VT	0x4654		/* Mach64 VT */
#define	PCI_PRODUCT_ATI_MACH64_GT	0x4754		/* Mach64 GT */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64 GX */

/* Atronics products */
#define	PCI_PRODUCT_ATRONICS_IDE_2015PL	0x2015		/* IDE-2015PL */

/* Avance Logic products */
#define	PCI_PRODUCT_AVANCE_ALG2301	0x2301		/* ALG2301 */

/* BusLogic products */
#define	PCI_PRODUCT_BUSLOGIC_OLD946C	0x0140		/* 946C 01 */
#define	PCI_PRODUCT_BUSLOGIC_946C	0x1040		/* 946C 10 */

/* Chips and Technologies products */
#define	PCI_PRODUCT_CHIPS_65545	0x00d8		/* 65545 */

/* Cirrus Logic products */
#define	PCI_PRODUCT_CIRRUS_GD_5430	0x00a0		/* GD 5430 */
#define	PCI_PRODUCT_CIRRUS_GD_5434_4	0x00a4		/* GD 5434-4 */
#define	PCI_PRODUCT_CIRRUS_GD_5434_8	0x00a8		/* GD 5434-8 */
#define	PCI_PRODUCT_CIRRUS_GD_5436	0x00ac		/* GD 5436 */
#define	PCI_PRODUCT_CIRRUS_CL_6729	0x1100		/* CL 6729 */
#define	PCI_PRODUCT_CIRRUS_CL_7542	0x1200		/* CL 7542 */

/* CMD Technology products */
#define	PCI_PRODUCT_CMDTECH_640A	0x0640		/* 640A */

/* Contaq Microsystems products */
#define	PCI_PRODUCT_CONTAQ_82C599	0x0600		/* 82C599 */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* DECchip 21050 PCI-PCI Bridge */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* DECchip 21040 (\"Tulip\") */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* DECchip 21030 (\"TGA\") */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* DECchip 21140 (\"FasterNet\") */
#define	PCI_PRODUCT_DEC_PBXGB	0x000d		/* TGA2 */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
/* product DEC ???	0x0010	??? VME Interface */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* DECchip 21041 (\"Tulip Pass 3\") */
#define	PCI_PRODUCT_DEC_DGLPB	0x0016		/* DGLPB (\"OPPO\") */

/* Diamond products */
#define	PCI_PRODUCT_DIAMOND_vIPER	0x9001		/* Viper/PCI */

/* Distributed Processing Technology products */
#define	PCI_PRODUCT_DPT_SC_RAID	0xa400		/* SmartCache/Raid */

/* FORE products */
#define	PCI_PRODUCT_FORE_PCA200	0x0210		/* ATM PCA-200 */
#define	PCI_PRODUCT_FORE_PCA200E	0x0300		/* ATM PCA-200e */

/* Future Domain products */
#define	PCI_PRODUCT_FUTUREDOMAIN_TMC_18C30	0x0000		/* TMC-18C30 (36C70) */

/* Efficient Networks products */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PF	0x0000		/* 155P-MF1 ATM (FPGA) */
#define	PCI_PRODUCT_EFFICIENTNETS_ENI155PA	0x0002		/* 155P-MF1 ATM (ASIC) */

/* Hewlett-Packard products */
#define	PCI_PRODUCT_HP_J2585A	0x1030		/* J2585A */

/* IBM products */
#define	PCI_PRODUCT_IBM_82351	0x0022		/* 82351 PCI-PCI Bridge */

/* Integrated Micro Solutions products */
#define	PCI_PRODUCT_IMS_8849	0x8849		/* 8849 */

/* Intel products */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB PCI-EISA Bridge */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424ZX Cache and DRAM controller */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378IB PCI-ISA Bridge (System I/O) */
#define	PCI_PRODUCT_INTEL_PCIB	0x0486		/* 82426EX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX PCI, Cache, and Memory Controller */
#define	PCI_PRODUCT_INTEL_SAA7116	0x1223		/* SAA7116 */
#define	PCI_PRODUCT_INTEL_82437	0x122d		/* 82437 Triton */
#define	PCI_PRODUCT_INTEL_82471	0x122e		/* 82471 Triton */
#define	PCI_PRODUCT_INTEL_82438	0x1230		/* 82438 */
#define	PCI_PRODUCT_INTEL_82440FX	0x1237		/* 82440FX Natoma, PCI and memory controller */
#define	PCI_PRODUCT_INTEL_82371SBa	0x7000		/* 82371SB PCI-ISA bridge */
#define	PCI_PRODUCT_INTEL_82371SBb	0x7010		/* 82371SB IDE interface */
#define	PCI_PRODUCT_INTEL_82450KX	0x84c4		/* 82450KX Orion, PCI memory controller */
#define	PCI_PRODUCT_INTEL_82454GX	0x84c5		/* 82454GX Orion, host to PCI bridge */

/* I. T. T. products */
#define	PCI_PRODUCT_ITT_AGX016	0x0001		/* AGX016 */

/* LeadTek Research */
#define	PCI_PRODUCT_LEADTEK_S3_805	0x0000		/* S3 805 */

/* Matrox products */
#define	PCI_PRODUCT_MATROX_ATLAS	0x0518		/* MGA-2 Atlas PX2085 */
#define	PCI_PRODUCT_MATROX_IMPRESSION	0x0d10		/* MGA Impression */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_960P	0x0001		/* RAID controller */

/* Mutech products */
#define	PCI_PRODUCT_MUTECH_MV1000	0x0001		/* MV1000 */

/* National Semiconductor products */
#define	PCI_PRODUCT_NS_87410	0xd001		/* 87410 */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_OLDNCR_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_OLDNCR_820	0x0002		/* 53c820 */
#define	PCI_PRODUCT_OLDNCR_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_OLDNCR_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_OLDNCR_810AP	0x0005		/* 53c810AP */
#define	PCI_PRODUCT_OLDNCR_860	0x0006		/* 53c860 */
#define	PCI_PRODUCT_OLDNCR_875	0x000f		/* 53c875 */
/* do the NCR chips use the new ID, as well? */

/* Number Nine products */
#define	PCI_PRODUCT_NUMBER9_IMAG128	0x2309		/* Imagine-128 */

/* Opti products */
#define	PCI_PRODUCT_OPTI_82C557	0xc557		/* 82C557 */
#define	PCI_PRODUCT_OPTI_82C558	0xc558		/* 82C558 */
#define	PCI_PRODUCT_OPTI_82C621	0xc621		/* 82C621 */
#define	PCI_PRODUCT_OPTI_82C822	0xc822		/* 82C822 */

/* Promise products */
#define	PCI_PRODUCT_PROMISE_DC5030	0x5300		/* DC5030 */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */
#define	PCI_PRODUCT_QLOGIC_ISP1022	0x1022		/* ISP1022 */

/* Quantum Designs products */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8500	0x0001		/* 8500 */
#define	PCI_PRODUCT_QUANTUMDESIGNS_8580	0x0002		/* 8580 */

/* Realtek (Creative Labs?) products */
#define	PCI_PRODUCT_REALTEK_RT8029	0x8029		/* Ethernet */

/* S3 products */
#define	PCI_PRODUCT_S3_TRIO64	0x8811		/* Trio32/64 */
#define	PCI_PRODUCT_S3_868	0x8880		/* 868 */
#define	PCI_PRODUCT_S3_928	0x88b0		/* 928 */
#define	PCI_PRODUCT_S3_864_0	0x88c0		/* 864-0 */
#define	PCI_PRODUCT_S3_864_1	0x88c1		/* 864-1 */
#define	PCI_PRODUCT_S3_964_0	0x88d0		/* 964-0 */
#define	PCI_PRODUCT_S3_964_1	0x88d1		/* 964-1 */
#define	PCI_PRODUCT_S3_968	0x88f0		/* 968 */

/* Silicon Integrated System products */
#define	PCI_PRODUCT_SIS_86C201	0x0001		/* 86C201 */
#define	PCI_PRODUCT_SIS_86C202	0x0002		/* 86C202 */
#define	PCI_PRODUCT_SIS_86C205	0x0005		/* 86C205 */
#define	PCI_PRODUCT_SIS_85C503	0x0008		/* 85C503 */
#define	PCI_PRODUCT_SIS_85C501	0x0406		/* 85C501 */
#define	PCI_PRODUCT_SIS_85C496	0x0496		/* 85C496 */
#define	PCI_PRODUCT_SIS_85C601	0x0601		/* 85C601 */

/* SMC products */
#define	PCI_PRODUCT_SMC_37C665	0x1000		/* FDC 37C665 */

/* Symphony Labs products */
#define	PCI_PRODUCT_SYMPHONY_82C101	0x0001		/* 82C101 */

/* Tekram Technology products */
#define	PCI_PRODUCT_TEKRAM_DC290	0xdc29		/* DC290 */

/* Trident products */
#define	PCI_PRODUCT_TRIDENT_TGUI_9320	0x9320		/* TGUI 9320 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9420	0x9420		/* TGUI 9420 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9440	0x9440		/* TGUI 9440 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9660	0x9660		/* TGUI 9660 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9680	0x9680		/* TGUI 9680 */
#define	PCI_PRODUCT_TRIDENT_TGUI_9682	0x9682		/* TGUI 9682 */

/* Tseng Labs products */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_A	0x3202		/* ET4000w32p rev A */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_B	0x3205		/* ET4000w32p rev B */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_C	0x3206		/* ET4000w32p rev C */
#define	PCI_PRODUCT_TSENG_ET4000_W32P_D	0x3207		/* ET4000w32p rev D */
#define	PCI_PRODUCT_TSENG_ET6000	0x3208		/* ET6000 */

/* UMC products */
#define	PCI_PRODUCT_UMC_UM8673F	0x0101		/* UM8673F */
#define	PCI_PRODUCT_UMC_UM8881F	0x8881		/* UM8881F PCI-Host bridge */
#define	PCI_PRODUCT_UMC_UM8886F	0x8886		/* UM8886F PCI-ISA bridge */
#define	PCI_PRODUCT_UMC_UM8886A	0x888a		/* UM8886A */
#define	PCI_PRODUCT_UMC_UM8891A	0x8891		/* UM8891A */

/* VIA Technologies products */
#define	PCI_PRODUCT_VIATECH_82C505	0x0505		/* 82C505 */
#define	PCI_PRODUCT_VIATECH_ALT_82C505	0x0561		/* 82C505 */
#define	PCI_PRODUCT_VIATECH_82C576	0x0505		/* 82C576 3V */

/* Vortex Computer Systems products */
#define	PCI_PRODUCT_VORTEX_GDT_6000B	0x0001		/* GDT 6000b */

/* VLSI products */
#define	PCI_PRODUCT_VLSI_82C592_FC1	0x0005		/* 82C592-FC1 */
#define	PCI_PRODUCT_VLSI_82C593_FC1	0x0006		/* 82C593-FC1 */

/* Weitek products */
#define	PCI_PRODUCT_WEITEK_P9000	0x9001		/* P9000 */
#define	PCI_PRODUCT_WEITEK_P9100	0x9100		/* P9100 */

/* Winbond Electronics products */
#define	PCI_PRODUCT_WINBOND_W83769F	0x0001		/* W83769F */

/* Zeinet products */
#define	PCI_PRODUCT_ZEINET_1221	0x0001		/* 1221 */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y above 1M */

/* Cyclades products */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_1	0x0100		/* Cyclom-Y below 1M */
#define	PCI_PRODUCT_CYCLADES_CYCLOMY_2	0x0101		/* Cyclom-Y above 1M */
