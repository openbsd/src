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

struct pci_knowndev pci_knowndevs[] = {
	{
	    PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7850,
	    PCI_KNOWNDEV_UNSUPP,
	    "Adaptec",
	    "AIC-7850",
	},
	{
	    PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC7870,
	    PCI_KNOWNDEV_UNSUPP,
	    "Adaptec",
	    "AIC-7870",
	},
	{
	    PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC2940,
	    PCI_KNOWNDEV_UNSUPP,
	    "Adaptec",
	    "AIC-2940",
	},
	{
	    PCI_VENDOR_ADP, PCI_PRODUCT_ADP_AIC2940U,
	    PCI_KNOWNDEV_UNSUPP,
	    "Adaptec",
	    "AIC-2940 (\"Ultra\")",
	},
	{
	    PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH32,
	    PCI_KNOWNDEV_UNSUPP,
	    "ATI",
	    "Mach32",
	},
	{
	    PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_CX,
	    PCI_KNOWNDEV_UNSUPP,
	    "ATI",
	    "Mach64-CX",
	},
	{
	    PCI_VENDOR_ATI, PCI_PRODUCT_ATI_MACH64_GX,
	    PCI_KNOWNDEV_UNSUPP,
	    "ATI",
	    "Mach64-GX",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21050,
	    PCI_KNOWNDEV_UNSUPP,
	    "DEC",
	    "DECchip 21050 (\"PPB\")",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21040,
	    0,
	    "DEC",
	    "DECchip 21040 (\"Tulip\")",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21030,
	    PCI_KNOWNDEV_UNSUPP,
	    "DEC",
	    "DECchip 21030 (\"TGA\")",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_NVRAM,
	    PCI_KNOWNDEV_UNSUPP,
	    "DEC",
	    "Zephyr NV-RAM",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_KZPSA,
	    PCI_KNOWNDEV_UNSUPP,
	    "DEC",
	    "KZPSA",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21140,
	    0,
	    "DEC",
	    "DECchip 21140 (\"FasterNet\")",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_DEFPA,
	    PCI_KNOWNDEV_UNSUPP,
	    "DEC",
	    "DEFPA",
	},
	{
	    PCI_VENDOR_DEC, PCI_PRODUCT_DEC_21041,
	    0,
	    "DEC",
	    "DECchip 21041 (\"Tulip Pass 3\")",
	},
	{
	    PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PCEB,
	    PCI_KNOWNDEV_UNSUPP,
	    "Intel",
	    "82375EB PCI-EISA Bridge",
	},
	{
	    PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PCIB,
	    PCI_KNOWNDEV_UNSUPP,
	    "Intel",
	    "82426EX PCI-ISA Bridge",
	},
	{
	    PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_PCMC,
	    PCI_KNOWNDEV_UNSUPP,
	    "Intel",
	    "82434LX PCI, Cache, and Memory controller",
	},
	{
	    PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_CDC,
	    PCI_KNOWNDEV_UNSUPP,
	    "Intel",
	    "82424 Cache and DRAM controller",
	},
	{
	    PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_SIO,
	    PCI_KNOWNDEV_UNSUPP,
	    "Intel",
	    "82378 System I/O",
	},
	{
	    PCI_VENDOR_MYLEX, PCI_PRODUCT_MYLEX_960P,
	    PCI_KNOWNDEV_UNSUPP,
	    "Mylex",
	    "RAID controller",
	},
	{
	    PCI_VENDOR_NCR, PCI_PRODUCT_NCR_810,
	    0,
	    "NCR",
	    "53c810",
	},
	{
	    PCI_VENDOR_OLDNCR, PCI_PRODUCT_OLDNCR_810,
	    0,
	    "NCR",
	    "53c810",
	},
	{
	    PCI_VENDOR_NCR, PCI_PRODUCT_NCR_825,
	    0,
	    "NCR",
	    "53c825",
	},
	{
	    PCI_VENDOR_OLDNCR, PCI_PRODUCT_OLDNCR_825,
	    0,
	    "NCR",
	    "53c825",
	},
	{
	    PCI_VENDOR_NCR, PCI_PRODUCT_NCR_815,
	    0,
	    "NCR",
	    "53c815",
	},
	{
	    PCI_VENDOR_OLDNCR, PCI_PRODUCT_OLDNCR_815,
	    0,
	    "NCR",
	    "53c815",
	},
	{
	    PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1020,
	    PCI_KNOWNDEV_UNSUPP,
	    "Q Logic",
	    "ISP1020",
	},
	{
	    PCI_VENDOR_S3, PCI_PRODUCT_S3_VISION864,
	    PCI_KNOWNDEV_UNSUPP,
	    "S3 Inc.",
	    "Vision 864",
	},
	{
	    PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C590,
	    0,
	    "3Com",
	    "3c590",
	},
	{
	    PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C595,
	    0,
	    "3Com",
	    "3c595",
	},
	{
	    PCI_VENDOR_CMD, PCI_PRODUCT_CMD_PCI0640,
	    PCI_KNOWNDEV_UNSUPP,
	    "CMD Technologies",
	    "PCI to IDE Controller",
	},
	{
	    PCI_VENDOR_OLDCOMPAQ, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Compaq",
	    NULL,
	},
	{
	    PCI_VENDOR_OLDNCR, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "NCR",
	    NULL,
	},
	{
	    PCI_VENDOR_ATI, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "ATI",
	    NULL,
	},
	{
	    PCI_VENDOR_VLSI, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "VLSI",
	    NULL,
	},
	{
	    PCI_VENDOR_ADL, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Advance Logic",
	    NULL,
	},
	{
	    PCI_VENDOR_NS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "NS",
	    NULL,
	},
	{
	    PCI_VENDOR_TSENG, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Tseng'Lab",
	    NULL,
	},
	{
	    PCI_VENDOR_WEITEK, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Weitek",
	    NULL,
	},
	{
	    PCI_VENDOR_DEC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "DEC",
	    NULL,
	},
	{
	    PCI_VENDOR_CIRRUS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Cirrus Logic",
	    NULL,
	},
	{
	    PCI_VENDOR_IBM, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "IBM",
	    NULL,
	},
	{
	    PCI_VENDOR_NCR, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "NCR",
	    NULL,
	},
	{
	    PCI_VENDOR_WD, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Western Digital",
	    NULL,
	},
	{
	    PCI_VENDOR_AMD, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "AMD",
	    NULL,
	},
	{
	    PCI_VENDOR_TRIDENT, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Trident",
	    NULL,
	},
	{
	    PCI_VENDOR_ACER, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Acer Incorporated",
	    NULL,
	},
	{
	    PCI_VENDOR_MATROX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Matrox",
	    NULL,
	},
	{
	    PCI_VENDOR_CT, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Chips & Technologies",
	    NULL,
	},
	{
	    PCI_VENDOR_COMPAQ, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Compaq",
	    NULL,
	},
	{
	    PCI_VENDOR_NEC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "NEC",
	    NULL,
	},
	{
	    PCI_VENDOR_FD, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Future Domain",
	    NULL,
	},
	{
	    PCI_VENDOR_SIS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Silicon Integrated Systems",
	    NULL,
	},
	{
	    PCI_VENDOR_HP, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Hewlett-Packard",
	    NULL,
	},
	{
	    PCI_VENDOR_KPC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Kubota Pacific Corp.",
	    NULL,
	},
	{
	    PCI_VENDOR_PCTECH, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "PCTECH",
	    NULL,
	},
	{
	    PCI_VENDOR_DPT, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "DPT",
	    NULL,
	},
	{
	    PCI_VENDOR_OPTI, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "OPTI",
	    NULL,
	},
	{
	    PCI_VENDOR_SGS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "SGS Thomson",
	    NULL,
	},
	{
	    PCI_VENDOR_BUSLOGIC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "BusLogic",
	    NULL,
	},
	{
	    PCI_VENDOR_TI, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Texas Instruments",
	    NULL,
	},
	{
	    PCI_VENDOR_SONY, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Sony",
	    NULL,
	},
	{
	    PCI_VENDOR_MOT, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Motorola",
	    NULL,
	},
	{
	    PCI_VENDOR_PROMISE, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Promise",
	    NULL,
	},
	{
	    PCI_VENDOR_N9, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Number Nine",
	    NULL,
	},
	{
	    PCI_VENDOR_UMC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "UMC",
	    NULL,
	},
	{
	    PCI_VENDOR_X, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "X TECHNOLOGY",
	    NULL,
	},
	{
	    PCI_VENDOR_MYLEX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Mylex",
	    NULL,
	},
	{
	    PCI_VENDOR_APPLE, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Apple",
	    NULL,
	},
	{
	    PCI_VENDOR_NEXGEN, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "NexGen",
	    NULL,
	},
	{
	    PCI_VENDOR_QLOGIC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Q Logic",
	    NULL,
	},
	{
	    PCI_VENDOR_LEADTEK, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Leadtek Research",
	    NULL,
	},
	{
	    PCI_VENDOR_CONTAQ, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Contaq",
	    NULL,
	},
	{
	    PCI_VENDOR_FOREX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Forex",
	    NULL,
	},
	{
	    PCI_VENDOR_BIT3, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Bit3 Computer Corp.",
	    NULL,
	},
	{
	    PCI_VENDOR_QLICOM, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Qlicom",
	    NULL,
	},
	{
	    PCI_VENDOR_CMD, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "CMD Technologies",
	    NULL,
	},
	{
	    PCI_VENDOR_VISION, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Vision",
	    NULL,
	},
	{
	    PCI_VENDOR_SIERRA, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Sierra",
	    NULL,
	},
	{
	    PCI_VENDOR_ACC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "ACC MICROELECTRONICS",
	    NULL,
	},
	{
	    PCI_VENDOR_WINBOND, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Winbond",
	    NULL,
	},
	{
	    PCI_VENDOR_CABLETRON, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Cabletron",
	    NULL,
	},
	{
	    PCI_VENDOR_3COM, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "3Com",
	    NULL,
	},
	{
	    PCI_VENDOR_AL, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Acer Labs",
	    NULL,
	},
	{
	    PCI_VENDOR_ASP, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Advanced System Products",
	    NULL,
	},
	{
	    PCI_VENDOR_CERN, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "CERN",
	    NULL,
	},
	{
	    PCI_VENDOR_ECP, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "ECP",
	    NULL,
	},
	{
	    PCI_VENDOR_ECU, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "ECU",
	    NULL,
	},
	{
	    PCI_VENDOR_IMS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "IMS",
	    NULL,
	},
	{
	    PCI_VENDOR_TEKRAM2, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Tekram",
	    NULL,
	},
	{
	    PCI_VENDOR_AMCC, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "AMCC",
	    NULL,
	},
	{
	    PCI_VENDOR_INTERG, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Intergraphics",
	    NULL,
	},
	{
	    PCI_VENDOR_REALTEK, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Realtek",
	    NULL,
	},
	{
	    PCI_VENDOR_INIT, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Initio Corp",
	    NULL,
	},
	{
	    PCI_VENDOR_VIA, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "VIA Technologies",
	    NULL,
	},
	{
	    PCI_VENDOR_PROTEON, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Proteon",
	    NULL,
	},
	{
	    PCI_VENDOR_VORTEX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "VORTEX",
	    NULL,
	},
	{
	    PCI_VENDOR_EF, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Efficient Networks",
	    NULL,
	},
	{
	    PCI_VENDOR_FORE, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Fore Systems",
	    NULL,
	},
	{
	    PCI_VENDOR_IMAGINGTECH, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Imaging Technology",
	    NULL,
	},
	{
	    PCI_VENDOR_PLX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "PLX",
	    NULL,
	},
	{
	    PCI_VENDOR_ALLIANCE, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Alliance",
	    NULL,
	},
	{
	    PCI_VENDOR_MUTECH, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Mutech",
	    NULL,
	},
	{
	    PCI_VENDOR_ZEITNET, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "ZeitNet",
	    NULL,
	},
	{
	    PCI_VENDOR_SPECIALIX, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Specialix",
	    NULL,
	},
	{
	    PCI_VENDOR_CYCLADES, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Cyclades",
	    NULL,
	},
	{
	    PCI_VENDOR_SYMPHONY, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Symphony",
	    NULL,
	},
	{
	    PCI_VENDOR_TEKRAM, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Tekram",
	    NULL,
	},
	{
	    PCI_VENDOR_AVANCE, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Avance",
	    NULL,
	},
	{
	    PCI_VENDOR_S3, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "S3 Inc.",
	    NULL,
	},
	{
	    PCI_VENDOR_INTEL, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Intel",
	    NULL,
	},
	{
	    PCI_VENDOR_ADP, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Adaptec",
	    NULL,
	},
	{
	    PCI_VENDOR_ATRONICS, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Atronics",
	    NULL,
	},
	{
	    PCI_VENDOR_HERCULES, 0,
	    PCI_KNOWNDEV_UNSUPP | PCI_KNOWNDEV_NOPROD,
	    "Hercules",
	    NULL,
	},
	{ 0, 0, 0, NULL, NULL, }
};
