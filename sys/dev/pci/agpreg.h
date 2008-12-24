/*	$OpenBSD: agpreg.h,v 1.12 2008/12/24 05:42:58 oga Exp $	*/
/*	$NetBSD: agpreg.h,v 1.1 2001/09/10 10:01:02 fvdl Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agpreg.h,v 1.3 2000/07/12 10:13:04 dfr Exp $
 */

#ifndef _PCI_AGPREG_H_
#define _PCI_AGPREG_H_

/*
 * Offsets for various AGP configuration registers.
 */
#define AGP_APBASE			0x10

/*
 * Offsets from the AGP Capability pointer.
 */
#define AGP_CAPID			0x02
#define AGP_CAPID_GET_MAJOR(x)		(((x) & 0x00f00000U) >> 20)
#define AGP_CAPID_GET_MINOR(x)		(((x) & 0x000f0000U) >> 16)
#define AGP_CAPID_GET_NEXT_PTR(x)	(((x) & 0x0000ff00U) >> 8)
#define AGP_CAPID_GET_CAP_ID(x)		(((x) & 0x000000ffU) >> 0)

#define AGP_STATUS			0x4
#define AGP_COMMAND			0x8

/*
 * Config offsets for Intel AGP chipsets.
 */
/* i845/855PM */
#define	AGP_I845_AGPMISC		0x51
#define AGPMISC_AAGN			(1U << 1)  /* Aperture AccessEN */

/* i840/850/850E */
#define AGP_I840_MCHCFG			0x50
#define MCHCFG_AAGN			(1U << 9)  /* Aperture AccessEN */

/* i82443LX/BX/GX */
#define AGP_INTEL_NBXCFG		0x50
#define AGP_INTEL_STS			0x90
#define NBXCFG_APAE			(1U << 10) /* AGPtoPCI AccessDIS */
#define NBXCFG_AAGN			(1U << 9)  /* Aperture AccessEN */

/* Error Status for i8XX Chipset */
#define	AGP_INTEL_I8XX_ERRSTS		0xc8

/* Common register */
#define	AGP_INTEL_ERRSTS		0x91	/* Not i8XX */
#define AGP_INTEL_AGPCMD		0xa8
#define AGPCMD_SBA			(1U << 9)
#define AGPCMD_AGPEN			(1U << 8)
#define AGPCMD_FWEN			(1U << 4)
#define AGPCMD_RATE_1X			(1U << 1)
#define AGPCMD_RATE_2X			(1U << 2)
#define AGPCMD_RATE_4X			(1U << 3)

#define AGP_INTEL_AGPCTRL		0xb0
#define AGPCTRL_AGPRSE			(1U << 13) /* AGPRSE (82443 only)*/
#define AGPCTRL_GTLB			(1U << 7)  /* GTLB EN */

#define AGP_INTEL_APSIZE		0xb4
#define APSIZE_MASK			0x3f

#define AGP_INTEL_ATTBASE		0xb8

/*
 * Config offsets for VIA AGP 2.x chipsets.
 */
#define AGP_VIA_GARTCTRL		0x80
#define AGP_VIA_APSIZE			0x84
#define AGP_VIA_ATTBASE			0x88

/*
 * Config offsets for VIA AGP 3.0 chipsets.
 */
#define AGP3_VIA_GARTCTRL		0x90
#define AGP3_VIA_APSIZE			0x94
#define AGP3_VIA_ATTBASE		0x98
#define AGP_VIA_AGPSEL			0xfd

/*
 * Config offsets for SiS AGP chipsets.
 */
#define AGP_SIS_ATTBASE			0x90
#define AGP_SIS_WINCTRL			0x94
#define AGP_SIS_TLBCTRL			0x97
#define AGP_SIS_TLBFLUSH		0x98

/*
 * Config offsets for Ali AGP chipsets.
 */
#define AGP_ALI_AGPCTRL			0xb8
#define AGP_ALI_ATTBASE			0xbc
#define AGP_ALI_TLBCTRL			0xc0

/*
 * Config offsets for the AMD 751 chipset.
 */
#define AGP_AMD751_REGISTERS		0x14
#define AGP_AMD751_APCTRL		0xac
#define AGP_AMD751_MODECTRL		0xb0
#define AGP_AMD751_MODECTRL_SYNEN	0x80
#define AGP_AMD751_MODECTRL2		0xb2
#define AGP_AMD751_MODECTRL2_G1LM	0x01
#define AGP_AMD751_MODECTRL2_GPDCE	0x02
#define AGP_AMD751_MODECTRL2_NGSE	0x08

/*
 * Memory mapped register offsets for AMD 751 chipset.
 */
#define AGP_AMD751_CAPS			0x00
#define AGP_AMD751_CAPS_EHI		0x0800
#define AGP_AMD751_CAPS_P2P		0x0400
#define AGP_AMD751_CAPS_MPC		0x0200
#define AGP_AMD751_CAPS_VBE		0x0100
#define AGP_AMD751_CAPS_REV		0x00ff
#define AGP_AMD751_STATUS		0x02
#define AGP_AMD751_STATUS_P2PS		0x0800
#define AGP_AMD751_STATUS_GCS		0x0400
#define AGP_AMD751_STATUS_MPS		0x0200
#define AGP_AMD751_STATUS_VBES		0x0100
#define AGP_AMD751_STATUS_P2PE		0x0008
#define AGP_AMD751_STATUS_GCE		0x0004
#define AGP_AMD751_STATUS_VBEE		0x0001
#define AGP_AMD751_ATTBASE		0x04
#define AGP_AMD751_TLBCTRL		0x0c

/*
 * Config registers for i810 device 0
 */
#define AGP_I810_SMRAM			0x70
#define AGP_I810_SMRAM_GMS		0xc0
#define AGP_I810_SMRAM_GMS_DISABLED	0x00
#define AGP_I810_SMRAM_GMS_ENABLED_0	0x40
#define AGP_I810_SMRAM_GMS_ENABLED_512	0x80
#define AGP_I810_SMRAM_GMS_ENABLED_1024 0xc0
#define AGP_I810_MISCC			0x72
#define AGP_I810_MISCC_WINSIZE	 	0x0001
#define AGP_I810_MISCC_WINSIZE_64	0x0000
#define AGP_I810_MISCC_WINSIZE_32	0x0001
#define AGP_I810_MISCC_PLCK		0x0008
#define AGP_I810_MISCC_PLCK_UNLOCKED	0x0000
#define AGP_I810_MISCC_PLCK_LOCKED	0x0008
#define AGP_I810_MISCC_WPTC		0x0030
#define AGP_I810_MISCC_WPTC_NOLIMIT	0x0000
#define AGP_I810_MISCC_WPTC_62		0x0010
#define AGP_I810_MISCC_WPTC_50		0x0020
#define AGP_I810_MISCC_WPTC_37		0x0030
#define AGP_I810_MISCC_RPTC		0x00c0
#define AGP_I810_MISCC_RPTC_NOLIMIT	0x0000
#define AGP_I810_MISCC_RPTC_62		0x0040
#define AGP_I810_MISCC_RPTC_50		0x0080 
#define AGP_I810_MISCC_RPTC_37		0x00c0

/*
 * Config registers for i810 device 1
 */
#define AGP_I810_GMADR			0x10
#define AGP_I810_MMADR			0x14

/*
 * Memory mapped register offsets for i810 chipset.
 */
#define AGP_I810_PGTBL_CTL		0x2020
#define AGP_I810_PGTBL_SIZE_MASK	0x0000000e
#define AGP_I810_PGTBL_SIZE_512KB	(0 << 1)
#define AGP_I810_PGTBL_SIZE_256KB	(1 << 1)
#define AGP_I810_PGTBL_SIZE_128KB	(2 << 1)
#define AGP_I810_DRT			0x3000
#define AGP_I810_DRT_UNPOPULATED	0x00
#define AGP_I810_DRT_POPULATED		0x01
#define AGP_I810_GTT			0x10000

/*
 * Config registers for i830MG device 0
 */
#define AGP_I830_GCC0                   0x50
#define AGP_I830_GCC1                   0x52
#define AGP_I830_GCC1_DEV2              0x08
#define AGP_I830_GCC1_DEV2_ENABLED      0x00
#define AGP_I830_GCC1_DEV2_DISABLED     0x08
#define AGP_I830_GCC1_GMS               0xf0
#define AGP_I830_GCC1_GMS_STOLEN_512    0x20
#define AGP_I830_GCC1_GMS_STOLEN_1024   0x30
#define AGP_I830_GCC1_GMS_STOLEN_8192   0x40
#define AGP_I830_GCC1_GMASIZE           0x01
#define AGP_I830_GCC1_GMASIZE_64        0x01
#define AGP_I830_GCC1_GMASIZE_128       0x00


/*
 * Config registers for 852GM/855GM/865G device 0
 */
#define AGP_I855_GCC1			0x50
#define AGP_I855_GCC1_DEV2		0x08
#define AGP_I855_GCC1_DEV2_ENABLED	0x00
#define AGP_I855_GCC1_DEV2_DISABLED	0x08
#define AGP_I855_GCC1_GMS		0xf0
#define AGP_I855_GCC1_GMS_STOLEN_0M	0x00
#define AGP_I855_GCC1_GMS_STOLEN_1M	0x10
#define AGP_I855_GCC1_GMS_STOLEN_4M	0x20
#define AGP_I855_GCC1_GMS_STOLEN_8M	0x30
#define AGP_I855_GCC1_GMS_STOLEN_16M	0x40
#define AGP_I855_GCC1_GMS_STOLEN_32M	0x50

/*
 * 915G registers
 */
#define AGP_I915_GMADR			0x18
#define AGP_I915_MMADR			0x10
#define AGP_I915_GTTADR			0x1C
#define AGP_I915_GCC1_GMS_STOLEN_48M	0x60
#define AGP_I915_GCC1_GMS_STOLEN_64M	0x70
#define AGP_I915_DEVEN			0x54
#define AGP_I915_DEVEN_D2F0		0x08
#define AGP_I915_DEVEN_D2F0_ENABLED	0x08
#define AGP_I915_DEVEN_D2F0_DISABLED	0x00
#define AGP_I915_MSAC			0x62
#define AGP_I915_MSAC_GMASIZE		0x02
#define AGP_I915_MSAC_GMASIZE_128	0x02
#define AGP_I915_MSAC_GMASIZE_256	0x00

/*
 * G965 registers
 */
#define AGP_I965_GMADR			0x18
#define AGP_I965_MMADR			0x10
#define AGP_I965_MSAC			0x62
#define AGP_I965_MSAC_GMASIZE		0x06
#define AGP_I965_MSAC_GMASIZE_128	0x00
#define AGP_I965_MSAC_GMASIZE_256	0x02
#define AGP_I965_MSAC_GMASIZE_512	0x06
#define AGP_I965_GTT			0x80000

/*
 * G33 registers
 */
#define AGP_G33_GCC1_GMS_STOLEN_128M	0x80
#define AGP_G33_GCC1_GMS_STOLEN_256M	0x90
#define AGP_G33_PGTBL_SIZE_MASK		(3U << 8)
#define AGP_G33_PGTBL_SIZE_1M		(1U << 8)
#define AGP_G33_PGTBL_SIZE_2M		(2U << 8)

/*
 * Intel 4-series registers and values
 */
#define AGP_INTEL_GMCH_GMS_STOLEN_96M	0xa0
#define AGP_INTEL_GMCH_GMS_STOLEN_160M	0xb0
#define AGP_INTEL_GMCH_GMS_STOLEN_224M	0xc0
#define AGP_INTEL_GMCH_GMS_STOLEN_352M	0xd0
#define	AGP_G4X_GTT			0x200000

#endif /* !_PCI_AGPREG_H_ */
