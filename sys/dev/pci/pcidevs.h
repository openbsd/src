/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	NetBSD: pcidevs,v 1.3 1995/11/10 19:36:09 christos Exp 
 */

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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

#define	PCI_VENDOR_OLDCOMPAQ	0x0E11		/* Compaq (Old ID. see "COMPAQ" below.) */
#define	PCI_VENDOR_OLDNCR	0x1000		/* NCR (Old ID. see "NCR" below.) */
#define	PCI_VENDOR_ATI	0x1002		/* ATI */
#define	PCI_VENDOR_VLSI	0x1004		/* VLSI */
#define	PCI_VENDOR_ADL	0x1005		/* Advance Logic */
#define	PCI_VENDOR_NS	0x100B		/* NS */
#define	PCI_VENDOR_TSENG	0x100C		/* Tseng'Lab */
#define	PCI_VENDOR_WEITEK	0x100E		/* Weitek */
#define	PCI_VENDOR_DEC	0x1011		/* DEC */
#define	PCI_VENDOR_CIRRUS	0x1013		/* Cirrus Logic */
#define	PCI_VENDOR_IBM	0x1014		/* IBM */
#define	PCI_VENDOR_NCR	0x101A		/* NCR */
#define	PCI_VENDOR_WD	0x101C		/* Western Digital */
#define	PCI_VENDOR_AMD	0x1022		/* AMD */
#define	PCI_VENDOR_TRIDENT	0x1023		/* Trident */
#define	PCI_VENDOR_ACER	0x1025		/* Acer Incorporated */
#define	PCI_VENDOR_MATROX	0x102B		/* Matrox */
#define	PCI_VENDOR_CT	0x102C		/* Chips & Technologies */
#define	PCI_VENDOR_COMPAQ	0x1032		/* Compaq */
#define	PCI_VENDOR_NEC	0x1033		/* NEC */
#define	PCI_VENDOR_FD	0x1036		/* Future Domain */
#define	PCI_VENDOR_SIS	0x1039		/* Silicon Integrated Systems */
#define	PCI_VENDOR_HP	0x103C		/* Hewlett-Packard */
#define	PCI_VENDOR_KPC	0x1040		/* Kubota Pacific Corp. */
#define	PCI_VENDOR_PCTECH	0x1042		/* PCTECH */
#define	PCI_VENDOR_DPT	0x1044		/* DPT */
#define	PCI_VENDOR_OPTI	0x1045		/* OPTI */
#define	PCI_VENDOR_SGS	0x104A		/* SGS Thomson */
#define	PCI_VENDOR_BUSLOGIC	0x104B		/* BusLogic */
#define	PCI_VENDOR_TI	0x104C		/* Texas Instruments */
#define	PCI_VENDOR_SONY	0x104D		/* Sony */
#define	PCI_VENDOR_MOT	0x1057		/* Motorola */
#define	PCI_VENDOR_PROMISE	0x105A		/* Promise */
#define	PCI_VENDOR_N9	0x105D		/* Number Nine */
#define	PCI_VENDOR_UMC	0x1060		/* UMC */
#define	PCI_VENDOR_X	0x1061		/* X TECHNOLOGY */
#define	PCI_VENDOR_MYLEX	0x1069		/* Mylex */
#define	PCI_VENDOR_APPLE	0x106B		/* Apple */
#define	PCI_VENDOR_NEXGEN	0x1074		/* NexGen */
#define	PCI_VENDOR_QLOGIC	0x1077		/* Q Logic */
#define	PCI_VENDOR_LEADTEK	0x107D		/* Leadtek Research */
#define	PCI_VENDOR_CONTAQ	0x1080		/* Contaq */
#define	PCI_VENDOR_FOREX	0x1083		/* Forex */
#define	PCI_VENDOR_BIT3	0x108A		/* Bit3 Computer Corp. */
#define	PCI_VENDOR_QLICOM	0x108D		/* Qlicom */
#define	PCI_VENDOR_CMD	0x1095		/* CMD Technologies */
#define	PCI_VENDOR_VISION	0x1098		/* Vision */
#define	PCI_VENDOR_SIERRA	0x10A8		/* Sierra */
#define	PCI_VENDOR_ACC	0x10AA		/* ACC MICROELECTRONICS */
#define	PCI_VENDOR_WINBOND	0x10AD		/* Winbond */
#define	PCI_VENDOR_CABLETRON	0x10B1		/* Cabletron */
#define	PCI_VENDOR_3COM	0x10B7		/* 3Com */
#define	PCI_VENDOR_AL	0x10B9		/* Acer Labs */
#define	PCI_VENDOR_ASP	0x10CD		/* Advanced System Products */
#define	PCI_VENDOR_CERN	0x10DC		/* CERN (??? XXX) */
#define	PCI_VENDOR_ECP	0x10DC		/* ECP (??? XXX) */
#define	PCI_VENDOR_ECU	0x10DC		/* ECU (??? XXX) */
#define	PCI_VENDOR_IMS	0x10E0		/* IMS */
#define	PCI_VENDOR_TEKRAM2	0x10E1		/* Tekram */
#define	PCI_VENDOR_AMCC	0x10E8		/* AMCC */
#define	PCI_VENDOR_INTERG	0x10EA		/* Intergraphics */
#define	PCI_VENDOR_REALTEK	0x10EC		/* Realtek */
#define	PCI_VENDOR_INIT	0x1101		/* Initio Corp */
#define	PCI_VENDOR_VIA	0x1106		/* VIA Technologies */
#define	PCI_VENDOR_PROTEON	0x1108		/* Proteon */
#define	PCI_VENDOR_VORTEX	0x1119		/* VORTEX */
#define	PCI_VENDOR_EF	0x111A		/* Efficient Networks */
#define	PCI_VENDOR_FORE	0x1127		/* Fore Systems */
#define	PCI_VENDOR_IMAGINGTECH	0x112F		/* Imaging Technology */
#define	PCI_VENDOR_PLX	0x113C		/* PLX */
#define	PCI_VENDOR_ALLIANCE	0x1142		/* Alliance */
#define	PCI_VENDOR_MUTECH	0x1159		/* Mutech */
#define	PCI_VENDOR_ZEITNET	0x1193		/* ZeitNet */
#define	PCI_VENDOR_SPECIALIX	0x11CB		/* Specialix */
#define	PCI_VENDOR_CYCLADES	0x120E		/* Cyclades */
#define	PCI_VENDOR_SYMPHONY	0x1C1C		/* Symphony */
#define	PCI_VENDOR_TEKRAM	0x1DE1		/* Tekram */
#define	PCI_VENDOR_AVANCE	0x4005		/* Avance */
#define	PCI_VENDOR_S3	0x5333		/* S3 Inc. */
#define	PCI_VENDOR_INTEL	0x8086		/* Intel */
#define	PCI_VENDOR_ADP	0x9004		/* Adaptec */
#define	PCI_VENDOR_ATRONICS	0x907F		/* Atronics */
#define	PCI_VENDOR_HERCULES	0xEDD8		/* Hercules */

/*
 * List of known products.  Grouped by vendor.
 */

/* Adaptec products */
#define	PCI_PRODUCT_ADP_AIC7850	0x7075		/* AIC-7850 */
#define	PCI_PRODUCT_ADP_AIC7870	0x7078		/* AIC-7870 */
#define	PCI_PRODUCT_ADP_AIC2940	0x7178		/* AIC-2940 */
#define	PCI_PRODUCT_ADP_AIC2940U	0x8178		/* AIC-2940 (\"Ultra\") */

/* ATI products */
#define	PCI_PRODUCT_ATI_MACH32	0x4158		/* Mach32 */
#define	PCI_PRODUCT_ATI_MACH64_CX	0x4358		/* Mach64-CX */
#define	PCI_PRODUCT_ATI_MACH64_GX	0x4758		/* Mach64-GX */

/* DEC products */
#define	PCI_PRODUCT_DEC_21050	0x0001		/* DECchip 21050 (\"PPB\") */
#define	PCI_PRODUCT_DEC_21040	0x0002		/* DECchip 21040 (\"Tulip\") */
#define	PCI_PRODUCT_DEC_21030	0x0004		/* DECchip 21030 (\"TGA\") */
#define	PCI_PRODUCT_DEC_NVRAM	0x0007		/* Zephyr NV-RAM */
#define	PCI_PRODUCT_DEC_KZPSA	0x0008		/* KZPSA */
#define	PCI_PRODUCT_DEC_21140	0x0009		/* DECchip 21140 (\"FasterNet\") */
#define	PCI_PRODUCT_DEC_DEFPA	0x000f		/* DEFPA */
/* product DEC ???	0x0010	UNSUPP	??? VME Interface */
#define	PCI_PRODUCT_DEC_21041	0x0014		/* DECchip 21041 (\"Tulip Pass 3\") */

/* Intel products */
/* XXX name? */
#define	PCI_PRODUCT_INTEL_PCEB	0x0482		/* 82375EB PCI-EISA Bridge */
#define	PCI_PRODUCT_INTEL_PCIB	0x0486		/* 82426EX PCI-ISA Bridge */
#define	PCI_PRODUCT_INTEL_PCMC	0x04a3		/* 82434LX PCI, Cache, and Memory controller */

/* XXX the following two Intel products are UNVERIFIED. */
#define	PCI_PRODUCT_INTEL_CDC	0x0483		/* 82424 Cache and DRAM controller */
/* XXX Supported on the Alpha. XXX unverified. XXX includes PCI-ISA bridge */
#define	PCI_PRODUCT_INTEL_SIO	0x0484		/* 82378 System I/O */

/* Mylex products */
#define	PCI_PRODUCT_MYLEX_960P	0x0001		/* RAID controller */

/* NCR/Symbios Logic products */
#define	PCI_PRODUCT_NCR_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_OLDNCR_810	0x0001		/* 53c810 */
#define	PCI_PRODUCT_NCR_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_OLDNCR_825	0x0003		/* 53c825 */
#define	PCI_PRODUCT_NCR_815	0x0004		/* 53c815 */
#define	PCI_PRODUCT_OLDNCR_815	0x0004		/* 53c815 */

/* QLogic products */
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020		/* ISP1020 */

/* S3 Products */
#define	PCI_PRODUCT_S3_VISION864	0x88c0		/* Vision 864 */

/* 3COM Products */
#define	PCI_PRODUCT_3COM_3C590	0x5900		/* 3c590 */
#define	PCI_PRODUCT_3COM_3C595	0x5950		/* 3c595 */

/* CMD Technologies Products */
#define	PCI_PRODUCT_CMD_PCI0640	0x0640		/* PCI to IDE Controller */
