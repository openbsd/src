/* $OpenBSD: smmureg.h,v 1.2 2022/09/11 10:18:54 patrick Exp $ */
/*
 * Copyright (c) 2021 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Global Register Space 0 */
#define SMMU_SCR0			0x000
#define  SMMU_SCR0_CLIENTPD			(1 << 0)
#define  SMMU_SCR0_GFRE				(1 << 1)
#define  SMMU_SCR0_GFIE				(1 << 2)
#define  SMMU_SCR0_EXIDENABLE			(1 << 3)
#define  SMMU_SCR0_GCFGFRE			(1 << 4)
#define  SMMU_SCR0_GCFGFIE			(1 << 5)
#define  SMMU_SCR0_USFCFG			(1 << 10)
#define  SMMU_SCR0_VMIDPNE			(1 << 11)
#define  SMMU_SCR0_PTM				(1 << 12)
#define  SMMU_SCR0_FB				(1 << 13)
#define  SMMU_SCR0_BSU_MASK			(0x3 << 14)
#define  SMMU_SCR0_VMID16EN			(1U << 31)
#define SMMU_SCR1			0x004
#define SMMU_SCR2			0x008
#define SMMU_SACR			0x010
#define  SMMU_SACR_MMU500_SMTNMB_TLBEN		(1 << 8)
#define  SMMU_SACR_MMU500_S2CRB_TLBEN		(1 << 10)
#define  SMMU_SACR_MMU500_CACHE_LOCK		(1 << 26)
#define SMMU_IDR0			0x020
#define  SMMU_IDR0_NUMSMRG(x)			(((x) >> 0) & 0xff)
#define  SMMU_IDR0_EXIDS			(1 << 8)
#define  SMMU_IDR0_NUMSIDB(x)			(((x) >> 9) & 0xf)
#define  SMMU_IDR0_BTM				(1 << 13)
#define  SMMU_IDR0_CCTM				(1 << 14)
#define  SMMU_IDR0_EXSMRGS			(1 << 15)
#define  SMMU_IDR0_NUMIRPT(x)			(((x) >> 16) & 0xff)
#define  SMMU_IDR0_PTFS(x)			(((x) >> 24) & 0x3)
#define  SMMU_IDR0_PTFS_AARCH32_SHORT_AND_LONG	0x0
#define  SMMU_IDR0_PTFS_AARCH32_ONLY_LONG	0x1
#define  SMMU_IDR0_PTFS_AARCH32_NO		0x2
#define  SMMU_IDR0_PTFS_AARCH32_RES		0x3
#define  SMMU_IDR0_ATOSNS			(1 << 26)
#define  SMMU_IDR0_SMS				(1 << 27)
#define  SMMU_IDR0_NTS				(1 << 28)
#define  SMMU_IDR0_S2TS				(1 << 29)
#define  SMMU_IDR0_S1TS				(1 << 30)
#define  SMMU_IDR0_SES				(1U << 31)
#define SMMU_IDR1			0x024
#define  SMMU_IDR1_NUMCB(x)			(((x) >> 0) & 0xff)
#define  SMMU_IDR1_NUMSSDNDXB(x)		(((x) >> 8) & 0xf)
#define  SMMU_IDR1_SSDTP(x)			(((x) >> 12) & 0x3)
#define  SMMU_IDR1_SSDTP_UNK			0x0
#define  SMMU_IDR1_SSDTP_IDX_NUMSSDNDXB		0x1
#define  SMMU_IDR1_SSDTP_RES			0x2
#define  SMMU_IDR1_SSDTP_IDX_16BIT		0x3
#define  SMMU_IDR1_SMCD				(1 << 15)
#define  SMMU_IDR1_NUMS2CB(x)			(((x) >> 16) & 0xff)
#define  SMMU_IDR1_HAFDBS(x)			(((x) >> 24) & 0x3)
#define  SMMU_IDR1_HAFDBS_NO			0x0
#define  SMMU_IDR1_HAFDBS_AF			0x1
#define  SMMU_IDR1_HAFDBS_RES			0x2
#define  SMMU_IDR1_HAFDBS_AFDB			0x3
#define  SMMU_IDR1_NUMPAGENDXB(x)		(((x) >> 28) & 0x7)
#define  SMMU_IDR1_PAGESIZE_4K			(0U << 31)
#define  SMMU_IDR1_PAGESIZE_64K			(1U << 31)
#define SMMU_IDR2			0x028
#define  SMMU_IDR2_IAS(x)			(((x) >> 0) & 0xf)
#define  SMMU_IDR2_IAS_32BIT			0x0
#define  SMMU_IDR2_IAS_36BIT			0x1
#define  SMMU_IDR2_IAS_40BIT			0x2
#define  SMMU_IDR2_IAS_42BIT			0x3
#define  SMMU_IDR2_IAS_44BIT			0x4
#define  SMMU_IDR2_IAS_48BIT			0x5
#define  SMMU_IDR2_OAS(x)			(((x) >> 4) & 0xf)
#define  SMMU_IDR2_OAS_32BIT			0x0
#define  SMMU_IDR2_OAS_36BIT			0x1
#define  SMMU_IDR2_OAS_40BIT			0x2
#define  SMMU_IDR2_OAS_42BIT			0x3
#define  SMMU_IDR2_OAS_44BIT			0x4
#define  SMMU_IDR2_OAS_48BIT			0x5
#define  SMMU_IDR2_UBS(x)			(((x) >> 8) & 0xf)
#define  SMMU_IDR2_UBS_32BIT			0x0
#define  SMMU_IDR2_UBS_36BIT			0x1
#define  SMMU_IDR2_UBS_40BIT			0x2
#define  SMMU_IDR2_UBS_42BIT			0x3
#define  SMMU_IDR2_UBS_44BIT			0x4
#define  SMMU_IDR2_UBS_49BIT			0x5
#define  SMMU_IDR2_UBS_64BIT			0xf
#define  SMMU_IDR2_PTFSV8_4KB			(1 << 12)
#define  SMMU_IDR2_PTFSV8_16KB			(1 << 13)
#define  SMMU_IDR2_PTFSV8_64KB			(1 << 14)
#define  SMMU_IDR2_VMID16S			(1 << 15)
#define  SMMU_IDR2_EXNUMSMRG			(((x) >> 16) & 0x7ff)
#define  SMMU_IDR2_E2HS				(1 << 27)
#define  SMMU_IDR2_HADS				(1 << 28)
#define  SMMU_IDR2_COMPINDEXS			(1 << 29)
#define  SMMU_IDR2_DIPANS			(1 << 30)
#define SMMU_IDR3			0x02c
#define SMMU_IDR4			0x030
#define SMMU_IDR5			0x034
#define SMMU_IDR6			0x038
#define SMMU_IDR7			0x03c
#define  SMMU_IDR7_MINOR(x)			(((x) >> 0) & 0xf)
#define  SMMU_IDR7_MAJOR(x)			(((x) >> 4) & 0xf)
#define SMMU_SGFSR			0x048
#define SMMU_SGFSYNR0			0x050
#define SMMU_SGFSYNR1			0x054
#define SMMU_SGFSYNR2			0x058
#define SMMU_TLBIVMID			0x064
#define SMMU_TLBIALLNSNH		0x068
#define SMMU_TLBIALLH			0x06c
#define SMMU_STLBGSYNC			0x070
#define SMMU_STLBGSTATUS		0x074
#define  SMMU_STLBGSTATUS_GSACTIVE		(1 << 0)
#define SMMU_SMR(x)			(0x800 + (x) * 0x4) /* 0 - 127 */
#define  SMMU_SMR_ID_SHIFT			0
#define  SMMU_SMR_ID_MASK			0x7fff
#define  SMMU_SMR_MASK_SHIFT			16
#define  SMMU_SMR_MASK_MASK			0x7fff
#define  SMMU_SMR_VALID				(1U << 31)
#define SMMU_S2CR(x)			(0xc00 + (x) * 0x4) /* 0 - 127 */
#define  SMMU_S2CR_EXIDVALID			(1 << 10)
#define  SMMU_S2CR_TYPE_TRANS			(0 << 16)
#define  SMMU_S2CR_TYPE_BYPASS			(1 << 16)
#define  SMMU_S2CR_TYPE_FAULT			(2 << 16)
#define  SMMU_S2CR_TYPE_MASK			(0x3 << 16)

/* Global Register Space 1 */
#define SMMU_CBAR(x)			(0x000 + (x) * 0x4)
#define  SMMU_CBAR_VMID_SHIFT			0
#define  SMMU_CBAR_BPSHCFG_RES			(0x0 << 8)
#define  SMMU_CBAR_BPSHCFG_OSH			(0x1 << 8)
#define  SMMU_CBAR_BPSHCFG_ISH			(0x2 << 8)
#define  SMMU_CBAR_BPSHCFG_NSH			(0x3 << 8)
#define  SMMU_CBAR_MEMATTR_WB			(0xf << 12)
#define  SMMU_CBAR_TYPE_S2_TRANS		(0x0 << 16)
#define  SMMU_CBAR_TYPE_S1_TRANS_S2_BYPASS	(0x1 << 16)
#define  SMMU_CBAR_TYPE_S1_TRANS_S2_FAULT	(0x2 << 16)
#define  SMMU_CBAR_TYPE_S1_TRANS_S2_TRANS	(0x3 << 16)
#define  SMMU_CBAR_TYPE_MASK			(0x3 << 16)
#define  SMMU_CBAR_IRPTNDX_SHIFT		24
#define SMMU_CBFRSYNRA(x)		(0x400 + (x) * 0x4)
#define SMMU_CBA2R(x)			(0x800 + (x) * 0x4)
#define  SMMU_CBA2R_VA64			(1 << 0)
#define  SMMU_CBA2R_MONC			(1 << 1)
#define  SMMU_CBA2R_VMID16_SHIFT		16

/* Context Bank Format */
#define SMMU_CB_SCTLR			0x000
#define  SMMU_CB_SCTLR_M			(1 << 0)
#define  SMMU_CB_SCTLR_TRE			(1 << 1)
#define  SMMU_CB_SCTLR_AFE			(1 << 2)
#define  SMMU_CB_SCTLR_CFRE			(1 << 5)
#define  SMMU_CB_SCTLR_CFIE			(1 << 6)
#define  SMMU_CB_SCTLR_ASIDPNE			(1 << 12)
#define SMMU_CB_ACTLR			0x004
#define  SMMU_CB_ACTLR_CPRE			(1 << 1)
#define SMMU_CB_TCR2			0x010
#define  SMMU_CB_TCR2_PASIZE_32BIT		(0x0 << 0)
#define  SMMU_CB_TCR2_PASIZE_36BIT		(0x1 << 0)
#define  SMMU_CB_TCR2_PASIZE_40BIT		(0x2 << 0)
#define  SMMU_CB_TCR2_PASIZE_42BIT		(0x3 << 0)
#define  SMMU_CB_TCR2_PASIZE_44BIT		(0x4 << 0)
#define  SMMU_CB_TCR2_PASIZE_48BIT		(0x5 << 0)
#define  SMMU_CB_TCR2_PASIZE_MASK		(0x7 << 0)
#define  SMMU_CB_TCR2_AS			(1 << 4)
#define  SMMU_CB_TCR2_SEP_UPSTREAM		(0x7 << 15)
#define SMMU_CB_TTBR0			0x020
#define SMMU_CB_TTBR1			0x028
#define  SMMU_CB_TTBR_ASID_SHIFT		48
#define SMMU_CB_TCR			0x030
#define  SMMU_CB_TCR_T0SZ(x)			((x) << 0)
#define  SMMU_CB_TCR_EPD0			(1 << 7)
#define  SMMU_CB_TCR_IRGN0_NC			(0x0 << 8)
#define  SMMU_CB_TCR_IRGN0_WBWA			(0x1 << 8)
#define  SMMU_CB_TCR_IRGN0_WT			(0x2 << 8)
#define  SMMU_CB_TCR_IRGN0_WB			(0x3 << 8)
#define  SMMU_CB_TCR_ORGN0_NC			(0x0 << 10)
#define  SMMU_CB_TCR_ORGN0_WBWA			(0x1 << 10)
#define  SMMU_CB_TCR_ORGN0_WT			(0x2 << 10)
#define  SMMU_CB_TCR_ORGN0_WB			(0x3 << 10)
#define  SMMU_CB_TCR_SH0_NSH			(0x0 << 12)
#define  SMMU_CB_TCR_SH0_OSH			(0x2 << 12)
#define  SMMU_CB_TCR_SH0_ISH			(0x3 << 12)
#define  SMMU_CB_TCR_TG0_4KB			(0x0 << 14)
#define  SMMU_CB_TCR_TG0_64KB			(0x1 << 14)
#define  SMMU_CB_TCR_TG0_16KB			(0x2 << 14)
#define  SMMU_CB_TCR_TG0_MASK			(0x3 << 14)
#define  SMMU_CB_TCR_T1SZ(x)			((x) << 16)
#define  SMMU_CB_TCR_EPD1			(1 << 23)
#define  SMMU_CB_TCR_IRGN1_NC			(0x0 << 24)
#define  SMMU_CB_TCR_IRGN1_WBWA			(0x1 << 24)
#define  SMMU_CB_TCR_IRGN1_WT			(0x2 << 24)
#define  SMMU_CB_TCR_IRGN1_WB			(0x3 << 24)
#define  SMMU_CB_TCR_ORGN1_NC			(0x0 << 26)
#define  SMMU_CB_TCR_ORGN1_WBWA			(0x1 << 26)
#define  SMMU_CB_TCR_ORGN1_WT			(0x2 << 26)
#define  SMMU_CB_TCR_ORGN1_WB			(0x3 << 26)
#define  SMMU_CB_TCR_SH1_NSH			(0x0 << 28)
#define  SMMU_CB_TCR_SH1_OSH			(0x2 << 28)
#define  SMMU_CB_TCR_SH1_ISH			(0x3 << 28)
#define  SMMU_CB_TCR_TG1_16KB			(0x1 << 30)
#define  SMMU_CB_TCR_TG1_4KB			(0x2 << 30)
#define  SMMU_CB_TCR_TG1_64KB			(0x3 << 30)
#define  SMMU_CB_TCR_TG1_MASK			(0x3 << 30)
#define  SMMU_CB_TCR_S2_SL0_4KB_L2		(0x0 << 6)
#define  SMMU_CB_TCR_S2_SL0_4KB_L1		(0x1 << 6)
#define  SMMU_CB_TCR_S2_SL0_4KB_L0		(0x2 << 6)
#define  SMMU_CB_TCR_S2_SL0_16KB_L3		(0x0 << 6)
#define  SMMU_CB_TCR_S2_SL0_16KB_L2		(0x1 << 6)
#define  SMMU_CB_TCR_S2_SL0_16KB_L1		(0x2 << 6)
#define  SMMU_CB_TCR_S2_SL0_64KB_L3		(0x0 << 6)
#define  SMMU_CB_TCR_S2_SL0_64KB_L2		(0x1 << 6)
#define  SMMU_CB_TCR_S2_SL0_64KB_L1		(0x2 << 6)
#define  SMMU_CB_TCR_S2_SL0_MASK		(0x3 << 6)
#define  SMMU_CB_TCR_S2_PASIZE_32BIT		(0x0 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_36BIT		(0x1 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_40BIT		(0x2 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_42BIT		(0x3 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_44BIT		(0x4 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_48BIT		(0x5 << 16)
#define  SMMU_CB_TCR_S2_PASIZE_MASK		(0x7 << 16)
#define SMMU_CB_MAIR0			0x038
#define SMMU_CB_MAIR1			0x03c
#define  SMMU_CB_MAIR_MAIR_ATTR(attr, idx)	((attr) << ((idx) * 8))
#define  SMMU_CB_MAIR_DEVICE_nGnRnE		0x00
#define  SMMU_CB_MAIR_DEVICE_nGnRE		0x04
#define  SMMU_CB_MAIR_DEVICE_NC			0x44
#define  SMMU_CB_MAIR_DEVICE_WB			0xff
#define  SMMU_CB_MAIR_DEVICE_WT			0x88
#define SMMU_CB_FSR			0x058
#define  SMMU_CB_FSR_TF				(1 << 1)
#define  SMMU_CB_FSR_AFF			(1 << 2)
#define  SMMU_CB_FSR_PF				(1 << 3)
#define  SMMU_CB_FSR_EF				(1 << 4)
#define  SMMU_CB_FSR_TLBMCF			(1 << 5)
#define  SMMU_CB_FSR_TLBLKF			(1 << 6)
#define  SMMU_CB_FSR_ASF			(1 << 7)
#define  SMMU_CB_FSR_UUT			(1 << 8)
#define  SMMU_CB_FSR_SS				(1 << 30)
#define  SMMU_CB_FSR_MULTI			(1U << 31)
#define  SMMU_CB_FSR_MASK			(SMMU_CB_FSR_TF | \
						 SMMU_CB_FSR_AFF | \
						 SMMU_CB_FSR_PF | \
						 SMMU_CB_FSR_EF | \
						 SMMU_CB_FSR_TLBMCF | \
						 SMMU_CB_FSR_TLBLKF | \
						 SMMU_CB_FSR_ASF | \
						 SMMU_CB_FSR_UUT | \
						 SMMU_CB_FSR_SS | \
						 SMMU_CB_FSR_MULTI)
#define SMMU_CB_FAR			0x060
#define SMMU_CB_FSYNR0			0x068
#define SMMU_CB_IPAFAR			0x070
#define SMMU_CB_TLBIVA			0x600
#define SMMU_CB_TLBIVAA			0x608
#define SMMU_CB_TLBIASID		0x610
#define SMMU_CB_TLBIALL			0x618
#define SMMU_CB_TLBIVAL			0x620
#define SMMU_CB_TLBIVAAL		0x628
#define SMMU_CB_TLBIIPAS2		0x630
#define SMMU_CB_TLBIIPAS2L		0x638
#define SMMU_CB_TLBSYNC			0x7f0
#define SMMU_CB_TLBSTATUS		0x7f4
#define  SMMU_CB_TLBSTATUS_SACTIVE		(1 << 0)
