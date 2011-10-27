/*	$OpenBSD: ahci.c,v 1.184 2011/10/27 08:03:50 jmatthew Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2010 Conformal Systems LLC <info@conformal.com>
 * Copyright (c) 2010 Jonathan Matthew <jonathan@d14n.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/pool.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ata/atascsi.h>
#include <dev/ata/pmreg.h>

/* change to AHCI_DEBUG for dmesg spam */
#define NO_AHCI_DEBUG

#ifdef AHCI_DEBUG
#define DPRINTF(m, f...) do { if ((ahcidebug & (m)) == (m)) printf(f); } \
    while (0)
#define AHCI_D_TIMEOUT		0x00
#define AHCI_D_VERBOSE		0x01
#define AHCI_D_INTR		0x02
#define AHCI_D_XFER		0x08
int ahcidebug = AHCI_D_VERBOSE;
#else
#define DPRINTF(m, f...)
#endif

#define AHCI_PCI_BAR		0x24
#define AHCI_PCI_ATI_SB600_MAGIC	0x40
#define AHCI_PCI_ATI_SB600_LOCKED	0x01
#define AHCI_PCI_INTERFACE	0x01

#define AHCI_REG_CAP		0x000 /* HBA Capabilities */
#define  AHCI_REG_CAP_NP(_r)		(((_r) & 0x1f)+1) /* Number of Ports */
#define  AHCI_REG_CAP_SXS		(1<<5) /* External SATA */
#define  AHCI_REG_CAP_EMS		(1<<6) /* Enclosure Mgmt */
#define  AHCI_REG_CAP_CCCS		(1<<7) /* Cmd Coalescing */
#define  AHCI_REG_CAP_NCS(_r)		((((_r) & 0x1f00)>>8)+1) /* NCmds*/
#define  AHCI_REG_CAP_PSC		(1<<13) /* Partial State Capable */
#define  AHCI_REG_CAP_SSC		(1<<14) /* Slumber State Capable */
#define  AHCI_REG_CAP_PMD		(1<<15) /* PIO Multiple DRQ Block */
#define  AHCI_REG_CAP_FBSS		(1<<16) /* FIS-Based Switching */
#define  AHCI_REG_CAP_SPM		(1<<17) /* Port Multiplier */
#define  AHCI_REG_CAP_SAM		(1<<18) /* AHCI Only mode */
#define  AHCI_REG_CAP_SNZO		(1<<19) /* Non Zero DMA Offsets */
#define  AHCI_REG_CAP_ISS		(0xf<<20) /* Interface Speed Support */
#define  AHCI_REG_CAP_ISS_G1		(0x1<<20) /* Gen 1 (1.5 Gbps) */
#define  AHCI_REG_CAP_ISS_G1_2		(0x2<<20) /* Gen 1 and 2 (3 Gbps) */
#define  AHCI_REG_CAP_SCLO		(1<<24) /* Cmd List Override */
#define  AHCI_REG_CAP_SAL		(1<<25) /* Activity LED */
#define  AHCI_REG_CAP_SALP		(1<<26) /* Aggressive Link Pwr Mgmt */
#define  AHCI_REG_CAP_SSS		(1<<27) /* Staggered Spinup */
#define  AHCI_REG_CAP_SMPS		(1<<28) /* Mech Presence Switch */
#define  AHCI_REG_CAP_SSNTF		(1<<29) /* SNotification Register */
#define  AHCI_REG_CAP_SNCQ		(1<<30) /* Native Cmd Queuing */
#define  AHCI_REG_CAP_S64A		(1<<31) /* 64bit Addressing */
#define  AHCI_FMT_CAP		"\020" "\040S64A" "\037NCQ" "\036SSNTF" \
				    "\035SMPS" "\034SSS" "\033SALP" "\032SAL" \
				    "\031SCLO" "\024SNZO" "\023SAM" "\022SPM" \
				    "\021FBSS" "\020PMD" "\017SSC" "\016PSC" \
				    "\010CCCS" "\007EMS" "\006SXS"
#define AHCI_REG_GHC		0x004 /* Global HBA Control */
#define  AHCI_REG_GHC_HR		(1<<0) /* HBA Reset */
#define  AHCI_REG_GHC_IE		(1<<1) /* Interrupt Enable */
#define  AHCI_REG_GHC_MRSM		(1<<2) /* MSI Revert to Single Msg */
#define  AHCI_REG_GHC_AE		(1<<31) /* AHCI Enable */
#define AHCI_FMT_GHC		"\020" "\040AE" "\003MRSM" "\002IE" "\001HR"
#define AHCI_REG_IS		0x008 /* Interrupt Status */
#define AHCI_REG_PI		0x00c /* Ports Implemented */
#define AHCI_REG_VS		0x010 /* AHCI Version */
#define  AHCI_REG_VS_0_95		0x00000905 /* 0.95 */
#define  AHCI_REG_VS_1_0		0x00010000 /* 1.0 */
#define  AHCI_REG_VS_1_1		0x00010100 /* 1.1 */
#define  AHCI_REG_VS_1_2		0x00010200 /* 1.2 */
#define  AHCI_REG_VS_1_3		0x00010300 /* 1.3 */
#define AHCI_REG_CCC_CTL	0x014 /* Coalescing Control */
#define  AHCI_REG_CCC_CTL_INT(_r)	(((_r) & 0xf8) >> 3) /* CCC INT slot */
#define AHCI_REG_CCC_PORTS	0x018 /* Coalescing Ports */
#define AHCI_REG_EM_LOC		0x01c /* Enclosure Mgmt Location */
#define AHCI_REG_EM_CTL		0x020 /* Enclosure Mgmt Control */

#define AHCI_PORT_REGION(_p)	(0x100 + ((_p) * 0x80))
#define AHCI_PORT_SIZE		0x80

#define AHCI_PREG_CLB		0x00 /* Cmd List Base Addr */
#define AHCI_PREG_CLBU		0x04 /* Cmd List Base Hi Addr */
#define AHCI_PREG_FB		0x08 /* FIS Base Addr */
#define AHCI_PREG_FBU		0x0c /* FIS Base Hi Addr */
#define AHCI_PREG_IS		0x10 /* Interrupt Status */
#define  AHCI_PREG_IS_DHRS		(1<<0) /* Device to Host FIS */
#define  AHCI_PREG_IS_PSS		(1<<1) /* PIO Setup FIS */
#define  AHCI_PREG_IS_DSS		(1<<2) /* DMA Setup FIS */
#define  AHCI_PREG_IS_SDBS		(1<<3) /* Set Device Bits FIS */
#define  AHCI_PREG_IS_UFS		(1<<4) /* Unknown FIS */
#define  AHCI_PREG_IS_DPS		(1<<5) /* Descriptor Processed */
#define  AHCI_PREG_IS_PCS		(1<<6) /* Port Change */
#define  AHCI_PREG_IS_DMPS		(1<<7) /* Device Mechanical Presence */
#define  AHCI_PREG_IS_PRCS		(1<<22) /* PhyRdy Change */
#define  AHCI_PREG_IS_IPMS		(1<<23) /* Incorrect Port Multiplier */
#define  AHCI_PREG_IS_OFS		(1<<24) /* Overflow */
#define  AHCI_PREG_IS_INFS		(1<<26) /* Interface Non-fatal Error */
#define  AHCI_PREG_IS_IFS		(1<<27) /* Interface Fatal Error */
#define  AHCI_PREG_IS_HBDS		(1<<28) /* Host Bus Data Error */
#define  AHCI_PREG_IS_HBFS		(1<<29) /* Host Bus Fatal Error */
#define  AHCI_PREG_IS_TFES		(1<<30) /* Task File Error */
#define  AHCI_PREG_IS_CPDS		(1<<31) /* Cold Presence Detect */
#define AHCI_PFMT_IS		"\20" "\040CPDS" "\037TFES" "\036HBFS" \
				    "\035HBDS" "\034IFS" "\033INFS" "\031OFS" \
				    "\030IPMS" "\027PRCS" "\010DMPS" "\006DPS" \
				    "\007PCS" "\005UFS" "\004SDBS" "\003DSS" \
				    "\002PSS" "\001DHRS"
#define AHCI_PREG_IE		0x14 /* Interrupt Enable */
#define  AHCI_PREG_IE_DHRE		(1<<0) /* Device to Host FIS */
#define  AHCI_PREG_IE_PSE		(1<<1) /* PIO Setup FIS */
#define  AHCI_PREG_IE_DSE		(1<<2) /* DMA Setup FIS */
#define  AHCI_PREG_IE_SDBE		(1<<3) /* Set Device Bits FIS */
#define  AHCI_PREG_IE_UFE		(1<<4) /* Unknown FIS */
#define  AHCI_PREG_IE_DPE		(1<<5) /* Descriptor Processed */
#define  AHCI_PREG_IE_PCE		(1<<6) /* Port Change */
#define  AHCI_PREG_IE_DMPE		(1<<7) /* Device Mechanical Presence */
#define  AHCI_PREG_IE_PRCE		(1<<22) /* PhyRdy Change */
#define  AHCI_PREG_IE_IPME		(1<<23) /* Incorrect Port Multiplier */
#define  AHCI_PREG_IE_OFE		(1<<24) /* Overflow */
#define  AHCI_PREG_IE_INFE		(1<<26) /* Interface Non-fatal Error */
#define  AHCI_PREG_IE_IFE		(1<<27) /* Interface Fatal Error */
#define  AHCI_PREG_IE_HBDE		(1<<28) /* Host Bus Data Error */
#define  AHCI_PREG_IE_HBFE		(1<<29) /* Host Bus Fatal Error */
#define  AHCI_PREG_IE_TFEE		(1<<30) /* Task File Error */
#define  AHCI_PREG_IE_CPDE		(1<<31) /* Cold Presence Detect */
#define AHCI_PFMT_IE		"\20" "\040CPDE" "\037TFEE" "\036HBFE" \
				    "\035HBDE" "\034IFE" "\033INFE" "\031OFE" \
				    "\030IPME" "\027PRCE" "\010DMPE" "\007PCE" \
				    "\006DPE" "\005UFE" "\004SDBE" "\003DSE" \
				    "\002PSE" "\001DHRE"
#define AHCI_PREG_CMD		0x18 /* Command and Status */
#define  AHCI_PREG_CMD_ST		(1<<0) /* Start */
#define  AHCI_PREG_CMD_SUD		(1<<1) /* Spin Up Device */
#define  AHCI_PREG_CMD_POD		(1<<2) /* Power On Device */
#define  AHCI_PREG_CMD_CLO		(1<<3) /* Command List Override */
#define  AHCI_PREG_CMD_FRE		(1<<4) /* FIS Receive Enable */
#define  AHCI_PREG_CMD_CCS(_r)		(((_r) >> 8) & 0x1f) /* Curr CmdSlot# */
#define  AHCI_PREG_CMD_MPSS		(1<<13) /* Mech Presence State */
#define  AHCI_PREG_CMD_FR		(1<<14) /* FIS Receive Running */
#define  AHCI_PREG_CMD_CR		(1<<15) /* Command List Running */
#define  AHCI_PREG_CMD_CPS		(1<<16) /* Cold Presence State */
#define  AHCI_PREG_CMD_PMA		(1<<17) /* Port Multiplier Attached */
#define  AHCI_PREG_CMD_HPCP		(1<<18) /* Hot Plug Capable */
#define  AHCI_PREG_CMD_MPSP		(1<<19) /* Mech Presence Switch */
#define  AHCI_PREG_CMD_CPD		(1<<20) /* Cold Presence Detection */
#define  AHCI_PREG_CMD_ESP		(1<<21) /* External SATA Port */
#define  AHCI_PREG_CMD_ATAPI		(1<<24) /* Device is ATAPI */
#define  AHCI_PREG_CMD_DLAE		(1<<25) /* Drv LED on ATAPI Enable */
#define  AHCI_PREG_CMD_ALPE		(1<<26) /* Aggro Pwr Mgmt Enable */
#define  AHCI_PREG_CMD_ASP		(1<<27) /* Aggro Slumber/Partial */
#define  AHCI_PREG_CMD_ICC		0xf0000000 /* Interface Comm Ctrl */
#define  AHCI_PREG_CMD_ICC_SLUMBER	0x60000000
#define  AHCI_PREG_CMD_ICC_PARTIAL	0x20000000
#define  AHCI_PREG_CMD_ICC_ACTIVE	0x10000000
#define  AHCI_PREG_CMD_ICC_IDLE		0x00000000
#define  AHCI_PFMT_CMD		"\020" "\034ASP" "\033ALPE" "\032DLAE" \
				    "\031ATAPI" "\026ESP" "\025CPD" "\024MPSP" \
				    "\023HPCP" "\022PMA" "\021CPS" "\020CR" \
				    "\017FR" "\016MPSS" "\005FRE" "\004CLO" \
				    "\003POD" "\002SUD" "\001ST"
#define AHCI_PREG_TFD		0x20 /* Task File Data*/
#define  AHCI_PREG_TFD_STS		0xff
#define  AHCI_PREG_TFD_STS_ERR		(1<<0)
#define  AHCI_PREG_TFD_STS_DRQ		(1<<3)
#define  AHCI_PREG_TFD_STS_BSY		(1<<7)
#define  AHCI_PREG_TFD_ERR		0xff00
#define AHCI_PFMT_TFD_STS	"\20" "\010BSY" "\004DRQ" "\001ERR"
#define AHCI_PREG_SIG		0x24 /* Signature */
#define AHCI_PREG_SSTS		0x28 /* SATA Status */
#define  AHCI_PREG_SSTS_DET		0xf /* Device Detection */
#define  AHCI_PREG_SSTS_DET_NONE	0x0
#define  AHCI_PREG_SSTS_DET_DEV_NE	0x1
#define  AHCI_PREG_SSTS_DET_DEV		0x3
#define  AHCI_PREG_SSTS_DET_PHYOFFLINE	0x4
#define  AHCI_PREG_SSTS_SPD		0xf0 /* Current Interface Speed */
#define  AHCI_PREG_SSTS_SPD_NONE	0x00
#define  AHCI_PREG_SSTS_SPD_GEN1	0x10
#define  AHCI_PREG_SSTS_SPD_GEN2	0x20
#define  AHCI_PREG_SSTS_IPM		0xf00 /* Interface Power Management */
#define  AHCI_PREG_SSTS_IPM_NONE	0x000
#define  AHCI_PREG_SSTS_IPM_ACTIVE	0x100
#define  AHCI_PREG_SSTS_IPM_PARTIAL	0x200
#define  AHCI_PREG_SSTS_IPM_SLUMBER	0x600
#define AHCI_PREG_SCTL		0x2c /* SATA Control */
#define  AHCI_PREG_SCTL_DET		0xf /* Device Detection */
#define  AHCI_PREG_SCTL_DET_NONE	0x0
#define  AHCI_PREG_SCTL_DET_INIT	0x1
#define  AHCI_PREG_SCTL_DET_DISABLE	0x4
#define  AHCI_PREG_SCTL_SPD		0xf0 /* Speed Allowed */
#define  AHCI_PREG_SCTL_SPD_ANY		0x00
#define  AHCI_PREG_SCTL_SPD_GEN1	0x10
#define  AHCI_PREG_SCTL_SPD_GEN2	0x20
#define  AHCI_PREG_SCTL_IPM		0xf00 /* Interface Power Management */
#define  AHCI_PREG_SCTL_IPM_NONE	0x000
#define  AHCI_PREG_SCTL_IPM_NOPARTIAL	0x100
#define  AHCI_PREG_SCTL_IPM_NOSLUMBER	0x200
#define  AHCI_PREG_SCTL_IPM_DISABLED	0x300
#define AHCI_PREG_SERR		0x30 /* SATA Error */
#define  AHCI_PREG_SERR_ERR(_r)		((_r) & 0xffff)
#define  AHCI_PREG_SERR_ERR_I		(1<<0) /* Recovered Data Integrity */
#define  AHCI_PREG_SERR_ERR_M		(1<<1) /* Recovered Communications */
#define  AHCI_PREG_SERR_ERR_T		(1<<8) /* Transient Data Integrity */
#define  AHCI_PREG_SERR_ERR_C		(1<<9) /* Persistent Comm/Data */
#define  AHCI_PREG_SERR_ERR_P		(1<<10) /* Protocol */
#define  AHCI_PREG_SERR_ERR_E		(1<<11) /* Internal */
#define  AHCI_PFMT_SERR_ERR	"\020" "\014E" "\013P" "\012C" "\011T" "\002M" \
				    "\001I"
#define  AHCI_PREG_SERR_DIAG(_r)	(((_r) >> 16) & 0xffff)
#define  AHCI_PREG_SERR_DIAG_N		(1<<0) /* PhyRdy Change */
#define  AHCI_PREG_SERR_DIAG_I		(1<<1) /* Phy Internal Error */
#define  AHCI_PREG_SERR_DIAG_W		(1<<2) /* Comm Wake */
#define  AHCI_PREG_SERR_DIAG_B		(1<<3) /* 10B to 8B Decode Error */
#define  AHCI_PREG_SERR_DIAG_D		(1<<4) /* Disparity Error */
#define  AHCI_PREG_SERR_DIAG_C		(1<<5) /* CRC Error */
#define  AHCI_PREG_SERR_DIAG_H		(1<<6) /* Handshake Error */
#define  AHCI_PREG_SERR_DIAG_S		(1<<7) /* Link Sequence Error */
#define  AHCI_PREG_SERR_DIAG_T		(1<<8) /* Transport State Trans Err */
#define  AHCI_PREG_SERR_DIAG_F		(1<<9) /* Unknown FIS Type */
#define  AHCI_PREG_SERR_DIAG_X		(1<<10) /* Exchanged */
#define  AHCI_PFMT_SERR_DIAG	"\020" "\013X" "\012F" "\011T" "\010S" "\007H" \
				    "\006C" "\005D" "\004B" "\003W" "\002I" \
				    "\001N"
#define AHCI_PREG_SACT		0x34 /* SATA Active */
#define AHCI_PREG_CI		0x38 /* Command Issue */
#define  AHCI_PREG_CI_ALL_SLOTS	0xffffffff
#define AHCI_PREG_SNTF		0x3c /* SNotification */

#define AHCI_PREG_FBS		0x40 /* FIS-based Switching Control */
#define AHCI_PREG_FBS_DWE	0xf0000	/* Device With Error */
#define AHCI_PREG_FBS_ADO	0xf000	/* Active Device Optimization */
#define AHCI_PREG_FBS_DEV	0xf00	/* Device To Issue */
#define AHCI_PREG_FBS_SDE	(1<<2)	/* Single Device Error */
#define AHCI_PREG_FBS_DEC	(1<<1)	/* Device Error Clear */
#define AHCI_PREG_FBS_EN	(1<<0)	/* Enable */



struct ahci_cmd_hdr {
	u_int16_t		flags;
#define AHCI_CMD_LIST_FLAG_CFL		0x001f /* Command FIS Length */
#define AHCI_CMD_LIST_FLAG_A		(1<<5) /* ATAPI */
#define AHCI_CMD_LIST_FLAG_W		(1<<6) /* Write */
#define AHCI_CMD_LIST_FLAG_P		(1<<7) /* Prefetchable */
#define AHCI_CMD_LIST_FLAG_R		(1<<8) /* Reset */
#define AHCI_CMD_LIST_FLAG_B		(1<<9) /* BIST */
#define AHCI_CMD_LIST_FLAG_C		(1<<10) /* Clear Busy upon R_OK */
#define AHCI_CMD_LIST_FLAG_PMP		0xf000 /* Port Multiplier Port */
#define AHCI_CMD_LIST_FLAG_PMP_SHIFT	12
	u_int16_t		prdtl; /* sgl len */

	u_int32_t		prdbc; /* transferred byte count */

	u_int32_t		ctba_lo;
	u_int32_t		ctba_hi;

	u_int32_t		reserved[4];
} __packed;

struct ahci_rfis {
	u_int8_t		dsfis[28];
	u_int8_t		reserved1[4];
	u_int8_t		psfis[24];
	u_int8_t		reserved2[8];
	u_int8_t		rfis[24];
	u_int8_t		reserved3[4];
	u_int8_t		sdbfis[4];
	u_int8_t		ufis[64];
	u_int8_t		reserved4[96];
} __packed;

struct ahci_prdt {
	u_int32_t		dba_lo;
	u_int32_t		dba_hi;
	u_int32_t		reserved;
	u_int32_t		flags;
#define AHCI_PRDT_FLAG_INTR		(1<<31) /* interrupt on completion */
} __packed;

/* this makes ahci_cmd_table 512 bytes, supporting 128-byte alignment */
#define AHCI_MAX_PRDT		24

struct ahci_cmd_table {
	u_int8_t		cfis[64];	/* Command FIS */
	u_int8_t		acmd[16];	/* ATAPI Command */
	u_int8_t		reserved[48];

	struct ahci_prdt	prdt[AHCI_MAX_PRDT];
} __packed;

#define AHCI_MAX_PORTS		32

struct ahci_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};
#define AHCI_DMA_MAP(_adm)	((_adm)->adm_map)
#define AHCI_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define AHCI_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct ahci_softc;
struct ahci_port;

struct ahci_ccb {
	/* ATA xfer associated with this CCB.  Must be 1st struct member. */
	struct ata_xfer		ccb_xa;

	int			ccb_slot;
	struct ahci_port	*ccb_port;

	bus_dmamap_t		ccb_dmamap;
	struct ahci_cmd_hdr	*ccb_cmd_hdr;
	struct ahci_cmd_table	*ccb_cmd_table;

	void			(*ccb_done)(struct ahci_ccb *);

	TAILQ_ENTRY(ahci_ccb)	ccb_entry;
};

struct ahci_port {
	struct ahci_softc	*ap_sc;
	bus_space_handle_t	ap_ioh;

#ifdef AHCI_COALESCE
	int			ap_num;
#endif

	struct ahci_rfis	*ap_rfis;
	struct ahci_dmamem	*ap_dmamem_rfis;

	struct ahci_dmamem	*ap_dmamem_cmd_list;
	struct ahci_dmamem	*ap_dmamem_cmd_table;

	volatile u_int32_t	ap_active;
	volatile u_int32_t	ap_active_cnt;
	volatile u_int32_t	ap_sactive;
	volatile u_int32_t	ap_pmp_ncq_port;
	struct ahci_ccb		*ap_ccbs;

	TAILQ_HEAD(, ahci_ccb)	ap_ccb_free;
	TAILQ_HEAD(, ahci_ccb)	ap_ccb_pending;
	struct mutex		ap_ccb_mtx;
	struct ahci_ccb		*ap_ccb_err;

	u_int32_t		ap_state;
#define AP_S_NORMAL			0
#define AP_S_PMP_PROBE			1
#define AP_S_PMP_PORT_PROBE		2
#define AP_S_FATAL_ERROR		3

	int			ap_pmp_ports;
	int			ap_port;
	int			ap_pmp_ignore_ifs;

	/* For error recovery. */
#ifdef DIAGNOSTIC
	int			ap_err_busy;
#endif
	u_int32_t		ap_err_saved_sactive;
	u_int32_t		ap_err_saved_active;
	u_int32_t		ap_err_saved_active_cnt;

	u_int8_t		*ap_err_scratch;

#ifdef AHCI_DEBUG
	char			ap_name[16];
#define PORTNAME(_ap)	((_ap)->ap_name)
#else
#define PORTNAME(_ap)	DEVNAME((_ap)->ap_sc)
#endif
};

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;
	pci_chipset_tag_t	sc_pc;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	int			sc_flags;
#define AHCI_F_NO_NCQ			(1<<0)
#define AHCI_F_IPMS_PROBE		(1<<1)	/* IPMS on failed PMP probe */
#define AHCI_F_NO_PMP			(1<<2)	/* ignore PMP capability */

	u_int			sc_ncmds;

	struct ahci_port	*sc_ports[AHCI_MAX_PORTS];

	struct atascsi		*sc_atascsi;

	u_int32_t		sc_cap;

#ifdef AHCI_COALESCE
	u_int32_t		sc_ccc_mask;
	u_int32_t		sc_ccc_ports;
	u_int32_t		sc_ccc_ports_cur;
#endif
};
#define DEVNAME(_s)		((_s)->sc_dev.dv_xname)

struct ahci_device {
	pci_vendor_id_t		ad_vendor;
	pci_product_id_t	ad_product;
	int			(*ad_match)(struct pci_attach_args *);
	int			(*ad_attach)(struct ahci_softc *,
				    struct pci_attach_args *);
};

const struct ahci_device *ahci_lookup_device(struct pci_attach_args *);

int			ahci_no_match(struct pci_attach_args *);
int			ahci_vt8251_attach(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_ati_sb_idetoahci(struct ahci_softc *,
			    struct pci_attach_args *pa);
int			ahci_ati_sb600_attach(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_ati_sb700_attach(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_amd_hudson2_attach(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_intel_attach(struct ahci_softc *,
			    struct pci_attach_args *);

static const struct ahci_device ahci_devices[] = {
	{ PCI_VENDOR_AMD,	PCI_PRODUCT_AMD_HUDSON2_SATA,
	    NULL,		ahci_amd_hudson2_attach },

	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SB600_SATA,
	    NULL,		ahci_ati_sb600_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_1,
	    NULL,		ahci_ati_sb600_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_2,
	    NULL,		ahci_ati_sb700_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_3,
	    NULL,		ahci_ati_sb700_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_4,
	    NULL,		ahci_ati_sb700_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_5,
	    NULL,		ahci_ati_sb700_attach },
	{ PCI_VENDOR_ATI,	PCI_PRODUCT_ATI_SBX00_SATA_6,
	    NULL,		ahci_ati_sb700_attach },

	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_6SERIES_AHCI_1,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_6SERIES_AHCI_2,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_6321ESB_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801GR_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801GBM_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_AHCI_6P,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801H_AHCI_4P,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801HBM_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_AHCI_1,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_AHCI_2,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801I_AHCI_3,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801JD_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_82801JI_AHCI,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_3400_AHCI_1,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_3400_AHCI_2,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_3400_AHCI_3,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_3400_AHCI_4,
	    NULL,		ahci_intel_attach },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_EP80579_AHCI,
	    NULL,		ahci_intel_attach },

	{ PCI_VENDOR_VIATECH,	PCI_PRODUCT_VIATECH_VT8251_SATA,
	  ahci_no_match,	ahci_vt8251_attach }
};

int			ahci_pci_match(struct device *, void *, void *);
void			ahci_pci_attach(struct device *, struct device *,
			    void *);
int			ahci_pci_detach(struct device *, int);
int			ahci_pci_activate(struct device *, int);

struct cfattach ahci_pci_ca = {
	sizeof(struct ahci_softc),
	ahci_pci_match,
	ahci_pci_attach,
	ahci_pci_detach,
	ahci_pci_activate
};

struct cfattach ahci_jmb_ca = {
	sizeof(struct ahci_softc),
	ahci_pci_match,
	ahci_pci_attach,
	ahci_pci_detach
};

struct cfdriver ahci_cd = {
	NULL, "ahci", DV_DULL
};


int			ahci_map_regs(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_unmap_regs(struct ahci_softc *);
int			ahci_map_intr(struct ahci_softc *,
			    struct pci_attach_args *, pci_intr_handle_t);
void			ahci_unmap_intr(struct ahci_softc *);
void			ahci_enable_interrupts(struct ahci_port *);

int			ahci_init(struct ahci_softc *);
int			ahci_port_alloc(struct ahci_softc *, u_int);
void			ahci_port_free(struct ahci_softc *, u_int);
int			ahci_port_init(struct ahci_softc *, u_int);

int			ahci_port_start(struct ahci_port *, int);
int			ahci_port_stop(struct ahci_port *, int);
int			ahci_port_clo(struct ahci_port *);
int			ahci_port_softreset(struct ahci_port *);
int			ahci_port_portreset(struct ahci_port *, int);
int			ahci_port_signature(struct ahci_port *);
int			ahci_pmp_port_softreset(struct ahci_port *, int);
int			ahci_pmp_port_portreset(struct ahci_port *, int);
int			ahci_pmp_port_probe(struct ahci_port *ap, int pmp_port);

int			ahci_load_prdt(struct ahci_ccb *);
void			ahci_unload_prdt(struct ahci_ccb *);

int			ahci_poll(struct ahci_ccb *, int, void (*)(void *));
void			ahci_start(struct ahci_ccb *);

void			ahci_issue_pending_ncq_commands(struct ahci_port *);
void			ahci_issue_pending_commands(struct ahci_port *, int);

int			ahci_intr(void *);
u_int32_t		ahci_port_intr(struct ahci_port *, u_int32_t);

struct ahci_ccb		*ahci_get_ccb(struct ahci_port *);
void			ahci_put_ccb(struct ahci_ccb *);

struct ahci_ccb		*ahci_get_err_ccb(struct ahci_port *);
void			ahci_put_err_ccb(struct ahci_ccb *);

struct ahci_ccb		*ahci_get_pmp_ccb(struct ahci_port *);
void			ahci_put_pmp_ccb(struct ahci_ccb *);

int			ahci_port_read_ncq_error(struct ahci_port *, int *, int);

struct ahci_dmamem	*ahci_dmamem_alloc(struct ahci_softc *, size_t);
void			ahci_dmamem_free(struct ahci_softc *,
			    struct ahci_dmamem *);

u_int32_t		ahci_read(struct ahci_softc *, bus_size_t);
void			ahci_write(struct ahci_softc *, bus_size_t, u_int32_t);
int			ahci_wait_ne(struct ahci_softc *, bus_size_t,
			    u_int32_t, u_int32_t);

u_int32_t		ahci_pread(struct ahci_port *, bus_size_t);
void			ahci_pwrite(struct ahci_port *, bus_size_t, u_int32_t);
int			ahci_pwait_eq(struct ahci_port *, bus_size_t,
			    u_int32_t, u_int32_t, int);
void			ahci_flush_tfd(struct ahci_port *ap);
u_int32_t		ahci_active_mask(struct ahci_port *);
int			ahci_port_detect_pmp(struct ahci_port *);
void			ahci_pmp_probe_timeout(void *);

/* pmp operations */
int			ahci_pmp_read(struct ahci_port *, int, int,
			    u_int32_t *);
int			ahci_pmp_write(struct ahci_port *, int, int, u_int32_t);
int			ahci_pmp_phy_status(struct ahci_port *, int,
			    u_int32_t *);
int 			ahci_pmp_identify(struct ahci_port *, int *);


/* Wait for all bits in _b to be cleared */
#define ahci_pwait_clr(_ap, _r, _b, _n) \
   ahci_pwait_eq((_ap), (_r), (_b), 0, (_n))

/* Wait for all bits in _b to be set */
#define ahci_pwait_set(_ap, _r, _b, _n) \
   ahci_pwait_eq((_ap), (_r), (_b), (_b), (_n))



/* provide methods for atascsi to call */
int			ahci_ata_probe(void *, int, int);
void			ahci_ata_free(void *, int, int);
struct ata_xfer *	ahci_ata_get_xfer(void *, int);
void			ahci_ata_put_xfer(struct ata_xfer *);
void			ahci_ata_cmd(struct ata_xfer *);

struct atascsi_methods ahci_atascsi_methods = {
	ahci_ata_probe,
	ahci_ata_free,
	ahci_ata_get_xfer,
	ahci_ata_put_xfer,
	ahci_ata_cmd
};

/* ccb completions */
void			ahci_ata_cmd_done(struct ahci_ccb *);
void			ahci_pmp_cmd_done(struct ahci_ccb *);
void			ahci_ata_cmd_timeout(void *);
void			ahci_empty_done(struct ahci_ccb *);

const struct ahci_device *
ahci_lookup_device(struct pci_attach_args *pa)
{
	int				i;
	const struct ahci_device	*ad;

	for (i = 0; i < (sizeof(ahci_devices) / sizeof(ahci_devices[0])); i++) {
		ad = &ahci_devices[i];
		if (ad->ad_vendor == PCI_VENDOR(pa->pa_id) &&
		    ad->ad_product == PCI_PRODUCT(pa->pa_id))
			return (ad);
	}

	return (NULL);
}

int
ahci_no_match(struct pci_attach_args *pa)
{
	return (0);
}

int
ahci_vt8251_attach(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	sc->sc_flags |= AHCI_F_NO_NCQ;

	return (0);
}

void
ahci_ati_sb_idetoahci(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			magic;

	if (PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		magic = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    AHCI_PCI_ATI_SB600_MAGIC);
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    AHCI_PCI_ATI_SB600_MAGIC,
		    magic | AHCI_PCI_ATI_SB600_LOCKED);

		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG,
		    PCI_CLASS_MASS_STORAGE << PCI_CLASS_SHIFT |
		    PCI_SUBCLASS_MASS_STORAGE_SATA << PCI_SUBCLASS_SHIFT |
		    AHCI_PCI_INTERFACE << PCI_INTERFACE_SHIFT |
		    PCI_REVISION(pa->pa_class) << PCI_REVISION_SHIFT);

		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    AHCI_PCI_ATI_SB600_MAGIC, magic);
	}
}

int
ahci_ati_sb600_attach(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	ahci_ati_sb_idetoahci(sc, pa);

	sc->sc_flags |= AHCI_F_IPMS_PROBE;

	return (0);
}

int
ahci_ati_sb700_attach(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	sc->sc_flags |= AHCI_F_IPMS_PROBE;
	return (0);
}

int
ahci_amd_hudson2_attach(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	ahci_ati_sb_idetoahci(sc, pa);

	return (0);
}

int
ahci_intel_attach(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	sc->sc_flags |= AHCI_F_NO_PMP;
	return (0);
}

int
ahci_pci_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args		*pa = aux;
	const struct ahci_device	*ad;

	ad = ahci_lookup_device(pa);
	if (ad != NULL) {
		/* the device may need special checks to see if it matches */
		if (ad->ad_match != NULL)
			return (ad->ad_match(pa));

		return (2); /* match higher than pciide */
	}

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_SATA &&
	    PCI_INTERFACE(pa->pa_class) == AHCI_PCI_INTERFACE)
		return (2);

	return (0);
}

void
ahci_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	struct pci_attach_args		*pa = aux;
	struct atascsi_attach_args	aaa;
	const struct ahci_device	*ad;
	pci_intr_handle_t		ih;
	u_int32_t			pi;
	int				i;

	sc->sc_pc = pa->pa_pc;

	ad = ahci_lookup_device(pa);
	if (ad != NULL && ad->ad_attach != NULL) {
		if (ad->ad_attach(sc, pa) != 0) {
			/* error should be printed by ad_attach */
			return;
		}
	}

	if (pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return;
	}
	printf(": %s,", pci_intr_string(pa->pa_pc, ih));

	if (ahci_map_regs(sc, pa) != 0) {
		/* error already printed by ahci_map_regs */
		return;
	}

	if (ahci_init(sc) != 0) {
		/* error already printed by ahci_init */
		goto unmap;
	}

	if (ahci_map_intr(sc, pa, ih) != 0) {
		/* error already printed by ahci_map_intr */
		goto unmap;
	}

	printf("\n");

	sc->sc_dmat = pa->pa_dmat;

	sc->sc_cap = ahci_read(sc, AHCI_REG_CAP);
	sc->sc_ncmds = AHCI_REG_CAP_NCS(sc->sc_cap);
#ifdef AHCI_DEBUG
	if (ahcidebug & AHCI_D_VERBOSE) {
		const char *gen;

		switch (sc->sc_cap & AHCI_REG_CAP_ISS) {
		case AHCI_REG_CAP_ISS_G1:
			gen = "1 (1.5Gbps)";
			break;
		case AHCI_REG_CAP_ISS_G1_2:
			gen = "1 (1.5Gbps) and 2 (3Gbps)";
			break;
		default:
			gen = "unknown";
			break;
		}

		printf("%s: capabilities 0x%b, %d ports, %d cmds, gen %s\n",
		    DEVNAME(sc), sc->sc_cap, AHCI_FMT_CAP,
		    AHCI_REG_CAP_NP(sc->sc_cap), sc->sc_ncmds, gen);
	}
#endif

	pi = ahci_read(sc, AHCI_REG_PI);
	DPRINTF(AHCI_D_VERBOSE, "%s: ports implemented: 0x%08x\n",
	    DEVNAME(sc), pi);

#ifdef AHCI_COALESCE
	/* Naive coalescing support - enable for all ports. */
	if (sc->sc_cap & AHCI_REG_CAP_CCCS) {
		u_int16_t		ccc_timeout = 20;
		u_int8_t		ccc_numcomplete = 12;
		u_int32_t		ccc_ctl;

		/* disable coalescing during reconfiguration. */
		ccc_ctl = ahci_read(sc, AHCI_REG_CCC_CTL);
		ccc_ctl &= ~0x00000001;
		ahci_write(sc, AHCI_REG_CCC_CTL, ccc_ctl);

		sc->sc_ccc_mask = 1 << AHCI_REG_CCC_CTL_INT(ccc_ctl);
		if (pi & sc->sc_ccc_mask) {
			/* A conflict with the implemented port list? */
			printf("%s: coalescing interrupt/implemented port list "
			    "conflict, PI: %08x, ccc_mask: %08x\n",
			    DEVNAME(sc), pi, sc->sc_ccc_mask);
			sc->sc_ccc_mask = 0;
			goto noccc;
		}

		/* ahci_port_start will enable each port when it starts. */
		sc->sc_ccc_ports = pi;
		sc->sc_ccc_ports_cur = 0;

		/* program thresholds and enable overall coalescing. */
		ccc_ctl &= ~0xffffff00;
		ccc_ctl |= (ccc_timeout << 16) | (ccc_numcomplete << 8);
		ahci_write(sc, AHCI_REG_CCC_CTL, ccc_ctl);
		ahci_write(sc, AHCI_REG_CCC_PORTS, 0);
		ahci_write(sc, AHCI_REG_CCC_CTL, ccc_ctl | 1);
	}
noccc:
#endif
	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (!ISSET(pi, 1 << i)) {
			/* dont allocate stuff if the port isnt implemented */
			continue;
		}

		if (ahci_port_alloc(sc, i) == ENOMEM)
			goto freeports;
	}

	bzero(&aaa, sizeof(aaa));
	aaa.aaa_cookie = sc;
	aaa.aaa_methods = &ahci_atascsi_methods;
	aaa.aaa_minphys = NULL;
	aaa.aaa_nports = AHCI_MAX_PORTS;
	aaa.aaa_ncmds = sc->sc_ncmds - 1;
	if (!(sc->sc_flags & AHCI_F_NO_NCQ) &&
	    (sc->sc_cap & AHCI_REG_CAP_SNCQ)) {
		aaa.aaa_capability |= ASAA_CAP_NCQ | ASAA_CAP_PMP_NCQ;
		/* XXX enabling ASAA_CAP_PMP_NCQ with FBS:
		 * - some error recovery work required (single device vs port
		 *   errors)
		 * - probably need to look at storing our active ccb queue
		 *   differently so we can group ncq and non-ncq commands
		 *   for different ports.  as long as we preserve the order for
		 *   each port, we can reorder commands to get more ncq 
		 *   commands to run in parallel.
		 */
	}

	sc->sc_atascsi = atascsi_attach(&sc->sc_dev, &aaa);

	/* Enable interrupts */
	ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE | AHCI_REG_GHC_IE);

	return;

freeports:
	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (sc->sc_ports[i] != NULL)
			ahci_port_free(sc, i);
	}
unmap:
	/* Disable controller */
	ahci_write(sc, AHCI_REG_GHC, 0);
	ahci_unmap_regs(sc);
	return;
}

int
ahci_pci_detach(struct device *self, int flags)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	int				 rv, i;

	if (sc->sc_atascsi != NULL) {
		rv = atascsi_detach(sc->sc_atascsi, flags);
		if (rv != 0)
			return (rv);
	}

	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (sc->sc_ports[i] != NULL)
			ahci_port_free(sc, i);
	}

	ahci_unmap_intr(sc);
	ahci_unmap_regs(sc);

	return (0);
}

int
ahci_pci_activate(struct device *self, int act)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	int				 i, rv = 0;

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		break;
	case DVACT_SUSPEND:
		rv = config_activate_children(self, act);
		for (i = 0; i < AHCI_MAX_PORTS; i++) {
			if (sc->sc_ports[i] != NULL)
				ahci_port_stop(sc->sc_ports[i], 1);
		}
		break;
	case DVACT_RESUME:
		/* enable ahci (global interrupts disabled) */
		ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE);

		/* restore BIOS initialised parameters */
		ahci_write(sc, AHCI_REG_CAP, sc->sc_cap);

		for (i = 0; i < AHCI_MAX_PORTS; i++) {
			if (sc->sc_ports[i] != NULL)
				ahci_port_init(sc, i);
		}

		/* Enable interrupts */
		ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE | AHCI_REG_GHC_IE);

		rv = config_activate_children(self, act);
		break;
	}
	return (rv);
}

int
ahci_map_regs(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			maptype;

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AHCI_PCI_BAR);
	if (pci_mapreg_map(pa, AHCI_PCI_BAR, maptype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(" unable to map registers\n");
		return (1);
	}

	return (0);
}

void
ahci_unmap_regs(struct ahci_softc *sc)
{
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
ahci_map_intr(struct ahci_softc *sc, struct pci_attach_args *pa,
    pci_intr_handle_t ih)
{
	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
	    ahci_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf("%s: unable to map interrupt\n", DEVNAME(sc));
		return (1);
	}

	return (0);
}

void
ahci_unmap_intr(struct ahci_softc *sc)
{
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
}

int
ahci_init(struct ahci_softc *sc)
{
	u_int32_t			reg, cap, pi;
	const char			*revision;

	DPRINTF(AHCI_D_VERBOSE, " GHC 0x%b", ahci_read(sc, AHCI_REG_GHC),
	    AHCI_FMT_GHC);

	/* save BIOS initialised parameters, enable staggered spin up */
	cap = ahci_read(sc, AHCI_REG_CAP);
	cap &= AHCI_REG_CAP_SMPS;
	cap |= AHCI_REG_CAP_SSS;
	pi = ahci_read(sc, AHCI_REG_PI);

	if (ISSET(AHCI_REG_GHC_AE, ahci_read(sc, AHCI_REG_GHC))) {
		/* reset the controller */
		ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_HR);
		if (ahci_wait_ne(sc, AHCI_REG_GHC, AHCI_REG_GHC_HR,
		    AHCI_REG_GHC_HR) != 0) {
			printf(" unable to reset controller\n");
			return (1);
		}
	}

	/* enable ahci (global interrupts disabled) */
	ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE);

	/* restore parameters */
	ahci_write(sc, AHCI_REG_CAP, cap);
	ahci_write(sc, AHCI_REG_PI, pi);

	/* check the revision */
	reg = ahci_read(sc, AHCI_REG_VS);
	switch (reg) {
	case AHCI_REG_VS_0_95:
		revision = "0.95";
		break;
	case AHCI_REG_VS_1_0:
		revision = "1.0";
		break;
	case AHCI_REG_VS_1_1:
		revision = "1.1";
		break;
	case AHCI_REG_VS_1_2:
		revision = "1.2";
		break;
	case AHCI_REG_VS_1_3:
		revision = "1.3";
		break;

	default:
		printf(" unsupported AHCI revision 0x%08x\n", reg);
		return (1);
	}

	printf(" AHCI %s", revision);

	return (0);
}

void
ahci_enable_interrupts(struct ahci_port *ap)
{
	ahci_pwrite(ap, AHCI_PREG_IE, AHCI_PREG_IE_TFEE | AHCI_PREG_IE_HBFE |
	    AHCI_PREG_IE_IFE | AHCI_PREG_IE_OFE | AHCI_PREG_IE_DPE |
	    AHCI_PREG_IE_UFE |
	    ((ap->ap_sc->sc_cap & AHCI_REG_CAP_SSNTF) ? AHCI_PREG_IE_IPME : 0) |
#ifdef AHCI_COALESCE
	    ((ap->ap_sc->sc_ccc_ports & (1 << ap->ap_port)) ? 0 :
	     (AHCI_PREG_IE_SDBE | AHCI_PREG_IE_DHRE))
#else
	    AHCI_PREG_IE_SDBE | AHCI_PREG_IE_DHRE
#endif
	    );
}

int
ahci_port_alloc(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap;
	struct ahci_ccb			*ccb;
	u_int64_t			dva;
	u_int32_t			cmd;
	struct ahci_cmd_hdr		*hdr;
	struct ahci_cmd_table		*table;
	int				i, rc = ENOMEM;

	ap = malloc(sizeof(*ap), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ap == NULL) {
		printf("%s: unable to allocate memory for port %d\n",
		    DEVNAME(sc), port);
		goto reterr;
	}
	ap->ap_err_scratch = dma_alloc(DEV_BSIZE, PR_NOWAIT | PR_ZERO);
	if (ap->ap_err_scratch == NULL) {
		printf("%s: unable to allocate DMA scratch buf for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}

#ifdef AHCI_DEBUG
	snprintf(ap->ap_name, sizeof(ap->ap_name), "%s.%d",
	    DEVNAME(sc), port);
#endif
	ap->ap_port = port;
	sc->sc_ports[port] = ap;

	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    AHCI_PORT_REGION(port), AHCI_PORT_SIZE, &ap->ap_ioh) != 0) {
		printf("%s: unable to create register window for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}

	ap->ap_sc = sc;
#ifdef AHCI_COALESCE
	ap->ap_num = port;
#endif
	TAILQ_INIT(&ap->ap_ccb_free);
	TAILQ_INIT(&ap->ap_ccb_pending);
	mtx_init(&ap->ap_ccb_mtx, IPL_BIO);

	/* Disable port interrupts */
	ahci_pwrite(ap, AHCI_PREG_IE, 0);

	/* Sec 10.1.2 - deinitialise port if it is already running */
	cmd = ahci_pread(ap, AHCI_PREG_CMD);
	if (ISSET(cmd, (AHCI_PREG_CMD_ST | AHCI_PREG_CMD_CR |
	    AHCI_PREG_CMD_FRE | AHCI_PREG_CMD_FR)) ||
	    ISSET(ahci_pread(ap, AHCI_PREG_SCTL), AHCI_PREG_SCTL_DET)) {
		int r;

		r = ahci_port_stop(ap, 1);
		if (r) {
			printf("%s: unable to disable %s, ignoring port %d\n",
			    DEVNAME(sc), r == 2 ? "CR" : "FR", port);
			rc = ENXIO;
			goto freeport;
		}

		/* Write DET to zero */
		ahci_pwrite(ap, AHCI_PREG_SCTL, 0);
	}

	/* XXX FBS - need to allocate 16x ahci_rfis struct? - but we don't
	 * know if there's a PMP attached or if the HBA supports FBS yet..
	 * reallocate when we enable FBS?
	 */

	/* Allocate RFIS */
	ap->ap_dmamem_rfis = ahci_dmamem_alloc(sc, sizeof(struct ahci_rfis));
	if (ap->ap_dmamem_rfis == NULL)
		goto nomem;

	/* Setup RFIS base address */
	ap->ap_rfis = (struct ahci_rfis *) AHCI_DMA_KVA(ap->ap_dmamem_rfis);
	dva = AHCI_DMA_DVA(ap->ap_dmamem_rfis);
	ahci_pwrite(ap, AHCI_PREG_FBU, (u_int32_t)(dva >> 32));
	ahci_pwrite(ap, AHCI_PREG_FB, (u_int32_t)dva);

	/* Enable FIS reception and activate port. */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	cmd |= AHCI_PREG_CMD_FRE | AHCI_PREG_CMD_POD | AHCI_PREG_CMD_SUD;
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd | AHCI_PREG_CMD_ICC_ACTIVE);

	/* Check whether port activated.  Skip it if not. */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	if (!ISSET(cmd, AHCI_PREG_CMD_FRE)) {
		rc = ENXIO;
		goto freeport;
	}

	/* Allocate a CCB for each command slot */
	ap->ap_ccbs = malloc(sizeof(struct ahci_ccb) * sc->sc_ncmds, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (ap->ap_ccbs == NULL) {
		printf("%s: unable to allocate command list for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}

	/* Command List Structures and Command Tables */
	ap->ap_dmamem_cmd_list = ahci_dmamem_alloc(sc,
	    sc->sc_ncmds * sizeof(struct ahci_cmd_hdr));
	ap->ap_dmamem_cmd_table = ahci_dmamem_alloc(sc,
	    sc->sc_ncmds * sizeof(struct ahci_cmd_table));
	if (ap->ap_dmamem_cmd_table == NULL || ap->ap_dmamem_cmd_list == NULL) {
nomem:
		printf("%s: unable to allocate DMA memory for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}

	/* Setup command list base address */
	dva = AHCI_DMA_DVA(ap->ap_dmamem_cmd_list);
	ahci_pwrite(ap, AHCI_PREG_CLBU, (u_int32_t)(dva >> 32));
	ahci_pwrite(ap, AHCI_PREG_CLB, (u_int32_t)dva);

	/* Split CCB allocation into CCBs and assign to command header/table */
	hdr = AHCI_DMA_KVA(ap->ap_dmamem_cmd_list);
	table = AHCI_DMA_KVA(ap->ap_dmamem_cmd_table);
	for (i = 0; i < sc->sc_ncmds; i++) {
		ccb = &ap->ap_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, AHCI_MAX_PRDT,
		    (4 * 1024 * 1024), 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dmamap for port %d "
			    "ccb %d\n", DEVNAME(sc), port, i);
			goto freeport;
		}

		ccb->ccb_slot = i;
		ccb->ccb_port = ap;
		ccb->ccb_cmd_hdr = &hdr[i];
		ccb->ccb_cmd_table = &table[i];
		dva = AHCI_DMA_DVA(ap->ap_dmamem_cmd_table) +
		    ccb->ccb_slot * sizeof(struct ahci_cmd_table);
		ccb->ccb_cmd_hdr->ctba_hi = htole32((u_int32_t)(dva >> 32));
		ccb->ccb_cmd_hdr->ctba_lo = htole32((u_int32_t)dva);

		ccb->ccb_xa.fis =
		    (struct ata_fis_h2d *)ccb->ccb_cmd_table->cfis;
		ccb->ccb_xa.packetcmd = ccb->ccb_cmd_table->acmd;
		ccb->ccb_xa.tag = i;

		ccb->ccb_xa.state = ATA_S_COMPLETE;
		ahci_put_ccb(ccb);
	}

	/* grab a ccb for use during error recovery */
	ap->ap_ccb_err = &ap->ap_ccbs[sc->sc_ncmds - 1];
	TAILQ_REMOVE(&ap->ap_ccb_free, ap->ap_ccb_err, ccb_entry);
	ap->ap_ccb_err->ccb_xa.state = ATA_S_COMPLETE;

	/* Wait for ICC change to complete */
	ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_ICC, 1);

	/* Reset port */
	rc = ahci_port_portreset(ap, 1);

	switch (rc) {
	case ENODEV:
		switch (ahci_pread(ap, AHCI_PREG_SSTS) & AHCI_PREG_SSTS_DET) {
		case AHCI_PREG_SSTS_DET_DEV_NE:
			printf("%s: device not communicating on port %d\n",
			    DEVNAME(sc), port);
			break;
		case AHCI_PREG_SSTS_DET_PHYOFFLINE:
			printf("%s: PHY offline on port %d\n", DEVNAME(sc),
			    port);
			break;
		default:
			DPRINTF(AHCI_D_VERBOSE, "%s: no device detected "
			    "on port %d\n", DEVNAME(sc), port);
			break;
		}
		goto freeport;

	case EBUSY:
		printf("%s: device on port %d didn't come ready, "
		    "TFD: 0x%b\n", DEVNAME(sc), port,
		    ahci_pread(ap, AHCI_PREG_TFD), AHCI_PFMT_TFD_STS);

		/* Try a soft reset to clear busy */
		rc = ahci_port_softreset(ap);
		if (rc) {
			printf("%s: unable to communicate "
			    "with device on port %d\n", DEVNAME(sc), port);
			goto freeport;
		}
		break;

	default:
		break;
	}

	DPRINTF(AHCI_D_VERBOSE, "%s: detected device on port %d; %d\n",
	    DEVNAME(sc), port, rc);

	/* Enable command transfers on port */
	if (ahci_port_start(ap, 0)) {
		printf("%s: failed to start command DMA on port %d, "
		    "disabling\n", DEVNAME(sc), port);
		rc = ENXIO;	/* couldn't start port */
	}

	/* Flush interrupts for port */
	ahci_pwrite(ap, AHCI_PREG_IS, ahci_pread(ap, AHCI_PREG_IS));
	ahci_write(sc, AHCI_REG_IS, 1 << port);

	ahci_enable_interrupts(ap);

freeport:
	if (rc != 0)
		ahci_port_free(sc, port);
reterr:
	return (rc);
}

void
ahci_port_free(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap = sc->sc_ports[port];
	struct ahci_ccb			*ccb;

	/* Ensure port is disabled and its interrupts are flushed */
	if (ap->ap_sc) {
		ahci_pwrite(ap, AHCI_PREG_CMD, 0);
		ahci_pwrite(ap, AHCI_PREG_IE, 0);
		ahci_pwrite(ap, AHCI_PREG_IS, ahci_pread(ap, AHCI_PREG_IS));
		ahci_write(sc, AHCI_REG_IS, 1 << port);
	}

	if (ap->ap_ccb_err)
		ahci_put_ccb(ap->ap_ccb_err);

	if (ap->ap_ccbs) {
		while ((ccb = ahci_get_ccb(ap)) != NULL)
			bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
		free(ap->ap_ccbs, M_DEVBUF);
	}

	if (ap->ap_dmamem_cmd_list)
		ahci_dmamem_free(sc, ap->ap_dmamem_cmd_list);
	if (ap->ap_dmamem_rfis)
		ahci_dmamem_free(sc, ap->ap_dmamem_rfis);
	if (ap->ap_dmamem_cmd_table)
		ahci_dmamem_free(sc, ap->ap_dmamem_cmd_table);
	if (ap->ap_err_scratch)
		dma_free(ap->ap_err_scratch, DEV_BSIZE);

	/* bus_space(9) says we dont free the subregions handle */

	free(ap, M_DEVBUF);
	sc->sc_ports[port] = NULL;
}

int
ahci_port_init(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap;
	u_int64_t			dva;
	u_int32_t			cmd;
	int				rc = ENOMEM;

#ifdef AHCI_DEBUG
	snprintf(ap->ap_name, sizeof(ap->ap_name), "%s.%d",
	    DEVNAME(sc), port);
#endif
	ap = sc->sc_ports[port];

	/* Disable port interrupts */
	ahci_pwrite(ap, AHCI_PREG_IE, 0);

	/* Sec 10.1.2 - deinitialise port if it is already running */
	cmd = ahci_pread(ap, AHCI_PREG_CMD);
	if (ISSET(cmd, (AHCI_PREG_CMD_ST | AHCI_PREG_CMD_CR |
	    AHCI_PREG_CMD_FRE | AHCI_PREG_CMD_FR)) ||
	    ISSET(ahci_pread(ap, AHCI_PREG_SCTL), AHCI_PREG_SCTL_DET)) {
		int r;

		r = ahci_port_stop(ap, 1);
		if (r) {
			printf("%s: unable to disable %s, ignoring port %d\n",
			    DEVNAME(sc), r == 2 ? "CR" : "FR", port);
			rc = ENXIO;
			goto reterr;
		}

		/* Write DET to zero */
		ahci_pwrite(ap, AHCI_PREG_SCTL, 0);
	}

	/* Setup RFIS base address */
	ap->ap_rfis = (struct ahci_rfis *) AHCI_DMA_KVA(ap->ap_dmamem_rfis);
	dva = AHCI_DMA_DVA(ap->ap_dmamem_rfis);
	ahci_pwrite(ap, AHCI_PREG_FBU, (u_int32_t)(dva >> 32));
	ahci_pwrite(ap, AHCI_PREG_FB, (u_int32_t)dva);

	/* Enable FIS reception and activate port. */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	cmd |= AHCI_PREG_CMD_FRE | AHCI_PREG_CMD_POD | AHCI_PREG_CMD_SUD;
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd | AHCI_PREG_CMD_ICC_ACTIVE);

	/* Check whether port activated.  Skip it if not. */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	if (!ISSET(cmd, AHCI_PREG_CMD_FRE)) {
		rc = ENXIO;
		goto reterr;
	}

	/* Setup command list base address */
	dva = AHCI_DMA_DVA(ap->ap_dmamem_cmd_list);
	ahci_pwrite(ap, AHCI_PREG_CLBU, (u_int32_t)(dva >> 32));
	ahci_pwrite(ap, AHCI_PREG_CLB, (u_int32_t)dva);

	/* Wait for ICC change to complete */
	ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_ICC, 1);

	/* Reset port */
	rc = ahci_port_portreset(ap, 1);
	switch (rc) {
	case ENODEV:
		switch (ahci_pread(ap, AHCI_PREG_SSTS) & AHCI_PREG_SSTS_DET) {
		case AHCI_PREG_SSTS_DET_DEV_NE:
			printf("%s: device not communicating on port %d\n",
			    DEVNAME(sc), port);
			break;
		case AHCI_PREG_SSTS_DET_PHYOFFLINE:
			printf("%s: PHY offline on port %d\n", DEVNAME(sc),
			    port);
			break;
		default:
			DPRINTF(AHCI_D_VERBOSE, "%s: no device detected "
			    "on port %d\n", DEVNAME(sc), port);
			break;
		}
		goto reterr;

	case EBUSY:
		printf("%s: device on port %d didn't come ready, "
		    "TFD: 0x%b\n", DEVNAME(sc), port,
		    ahci_pread(ap, AHCI_PREG_TFD), AHCI_PFMT_TFD_STS);

		/* Try a soft reset to clear busy */
		rc = ahci_port_softreset(ap);
		if (rc) {
			printf("%s: unable to communicate "
			    "with device on port %d\n", DEVNAME(sc), port);
			goto reterr;
		}
		break;

	default:
		break;
	}
	DPRINTF(AHCI_D_VERBOSE, "%s: detected device on port %d\n",
	    DEVNAME(sc), port);

	if (ap->ap_pmp_ports > 0) {
		int p;

		for (p = 0; p < ap->ap_pmp_ports; p++) {
			int sig;

			/* might need to do a portreset first here? */

			/* softreset the port */
			if (ahci_pmp_port_softreset(ap, p)) {
				printf("%s.%d: unable to probe PMP port due to"
				    " softreset failure\n", PORTNAME(ap), p);
				continue;
			}

			sig = ahci_port_signature(ap);
			printf("%s.%d: port signature returned %d\n",
			    PORTNAME(ap), p, sig);
		}
	}

	/* Enable command transfers on port */
	if (ahci_port_start(ap, 0)) {
		printf("%s: failed to start command DMA on port %d, "
		    "disabling\n", DEVNAME(sc), port);
		rc = ENXIO;	/* couldn't start port */
	}

	/* Flush interrupts for port */
	ahci_pwrite(ap, AHCI_PREG_IS, ahci_pread(ap, AHCI_PREG_IS));
	ahci_write(sc, AHCI_REG_IS, 1 << port);

	ahci_enable_interrupts(ap);

reterr:
	return (rc);
}

int
ahci_port_start(struct ahci_port *ap, int fre_only)
{
	u_int32_t			r;

	/* XXX FBS: possibly turn FBS on here */

	/* Turn on FRE (and ST) */
	r = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	r |= AHCI_PREG_CMD_FRE;
	if (!fre_only)
		r |= AHCI_PREG_CMD_ST;
	ahci_pwrite(ap, AHCI_PREG_CMD, r);

#ifdef AHCI_COALESCE
	/* (Re-)enable coalescing on the port. */
	if (ap->ap_sc->sc_ccc_ports & (1 << ap->ap_num)) {
		ap->ap_sc->sc_ccc_ports_cur |= (1 << ap->ap_num);
		ahci_write(ap->ap_sc, AHCI_REG_CCC_PORTS,
		    ap->ap_sc->sc_ccc_ports_cur);
	}
#endif

	/* Wait for CR to come on */
	if (!fre_only &&
	    ahci_pwait_set(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CR, 1))
		return (1);

	return (0);
}

int
ahci_port_stop(struct ahci_port *ap, int stop_fis_rx)
{
	u_int32_t			r;

#ifdef AHCI_COALESCE
	/* Disable coalescing on the port while it is stopped. */
	if (ap->ap_sc->sc_ccc_ports & (1 << ap->ap_num)) {
		ap->ap_sc->sc_ccc_ports_cur &= ~(1 << ap->ap_num);
		ahci_write(ap->ap_sc, AHCI_REG_CCC_PORTS,
		    ap->ap_sc->sc_ccc_ports_cur);
	}
#endif

	/* Turn off ST (and FRE) */
	r = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	r &= ~AHCI_PREG_CMD_ST;
	if (stop_fis_rx)
		r &= ~AHCI_PREG_CMD_FRE;
	ahci_pwrite(ap, AHCI_PREG_CMD, r);

	/* Wait for CR to go off */
	if (ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CR, 1))
		return (1);

	/* Wait for FR to go off */
	if (stop_fis_rx &&
	    ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_FR, 1))
		return (2);

	/* XXX FBS: possibly disable FBS here? */

	return (0);
}

/* AHCI command list override -> forcibly clear TFD.STS.{BSY,DRQ} */
int
ahci_port_clo(struct ahci_port *ap)
{
	struct ahci_softc		*sc = ap->ap_sc;
	u_int32_t			cmd;

	/* Only attempt CLO if supported by controller */
	if (!ISSET(ahci_read(sc, AHCI_REG_CAP), AHCI_REG_CAP_SCLO))
		return (1);

	/* Issue CLO */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
#ifdef DIAGNOSTIC
	if (ISSET(cmd, AHCI_PREG_CMD_ST))
		printf("%s: CLO requested while port running\n", PORTNAME(ap));
#endif
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd | AHCI_PREG_CMD_CLO);

	/* Wait for completion */
	if (ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CLO, 1)) {
		printf("%s: CLO did not complete\n", PORTNAME(ap));
		return (1);
	}

	return (0);
}

/* AHCI soft reset, Section 10.4.1 */
int
ahci_port_softreset(struct ahci_port *ap)
{
	struct ahci_ccb			*ccb = NULL;
	struct ahci_cmd_hdr		*cmd_slot;
	u_int8_t			*fis;
	int				s, rc = EIO;
	u_int32_t			cmd;

	DPRINTF(AHCI_D_VERBOSE, "%s: soft reset\n", PORTNAME(ap));

	s = splbio();

	/* Save previous command register state */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;

	/* Idle port */
	if (ahci_port_stop(ap, 0)) {
		printf("%s: failed to stop port, cannot softreset\n",
		    PORTNAME(ap));
		goto err;
	}

	/* Request CLO if device appears hung */
	if (ISSET(ahci_pread(ap, AHCI_PREG_TFD), AHCI_PREG_TFD_STS_BSY |
	    AHCI_PREG_TFD_STS_DRQ))
		ahci_port_clo(ap);

	/* Clear port errors to permit TFD transfer */
	ahci_pwrite(ap, AHCI_PREG_SERR, ahci_pread(ap, AHCI_PREG_SERR));

	/* XXX FBS - need to ensure we don't enable FBS here, since we're
	 * resetting stuff
	 * (AHCI spec 9.3.8)
	 */
	/* Restart port */
	if (ahci_port_start(ap, 0)) {
		printf("%s: failed to start port, cannot softreset\n",
		    PORTNAME(ap));
		goto err;
	}

	/* Check whether CLO worked */
	if (ahci_pwait_clr(ap, AHCI_PREG_TFD,
	    AHCI_PREG_TFD_STS_BSY | AHCI_PREG_TFD_STS_DRQ, 1)) {
		printf("%s: CLO %s, need port reset\n", PORTNAME(ap),
		    ISSET(ahci_read(ap->ap_sc, AHCI_REG_CAP), AHCI_REG_CAP_SCLO)
		    ? "failed" : "unsupported");
		rc = EBUSY;
		goto err;
	}

	/* Prep first D2H command with SRST feature & clear busy/reset flags */
	ccb = ahci_get_err_ccb(ap);
	cmd_slot = ccb->ccb_cmd_hdr;
	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));

	fis = ccb->ccb_cmd_table->cfis;
	fis[0] = ATA_FIS_TYPE_H2D;
	fis[15] = ATA_FIS_CONTROL_SRST;

	cmd_slot->prdtl = 0;
	cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_C); /* Clear busy on OK */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_R); /* Reset */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_W); /* Write */

	ccb->ccb_xa.state = ATA_S_PENDING;
	if (ahci_poll(ccb, 1000, NULL) != 0)
		goto err;

	/* Prep second D2H command to read status and complete reset sequence */
	fis[0] = ATA_FIS_TYPE_H2D;
	fis[15] = 0;

	cmd_slot->prdtl = 0;
	cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_W);

	ccb->ccb_xa.state = ATA_S_PENDING;
	if (ahci_poll(ccb, 1000, NULL) != 0)
		goto err;

	if (ahci_pwait_clr(ap, AHCI_PREG_TFD, AHCI_PREG_TFD_STS_BSY |
	    AHCI_PREG_TFD_STS_DRQ | AHCI_PREG_TFD_STS_ERR, 1)) {
		printf("%s: device didn't come ready after reset, TFD: 0x%b\n",
		    PORTNAME(ap), ahci_pread(ap, AHCI_PREG_TFD),
		    AHCI_PFMT_TFD_STS);
		rc = EBUSY;
		goto err;
	}

	rc = 0;
err:
	if (ccb != NULL) {
		/* Abort our command, if it failed, by stopping command DMA. */
		if (rc != 0 && ISSET(ap->ap_active, 1 << ccb->ccb_slot)) {
			printf("%s: stopping the port, softreset slot %d was "
			    "still active.\n", PORTNAME(ap), ccb->ccb_slot);
			ahci_port_stop(ap, 0);
		}
		ccb->ccb_xa.state = ATA_S_ERROR;
		ahci_put_err_ccb(ccb);
	}

	/* Restore saved CMD register state */
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd);

	splx(s);

	return (rc);
}

int
ahci_pmp_port_softreset(struct ahci_port *ap, int pmp_port)
{
	struct ahci_ccb		*ccb = NULL;
	u_int32_t		data;
	int			count;
	int			rc;
	int			s;
	struct ahci_cmd_hdr	*cmd_slot;
	u_int8_t		*fis;

	/* XXX FBS: ensure fbs is disabled on ap, since we're resetting
	 * devices (AHCI section 9.3.8)
	 */

	s = splbio();
	/* ignore spurious IFS errors while resetting */
	DPRINTF(AHCI_D_VERBOSE, "%s: now ignoring IFS\n", PORTNAME(ap));
	ap->ap_pmp_ignore_ifs = 1;

	count = 2;
	rc = 0;
	do {
		if (ccb != NULL) {
			ahci_put_pmp_ccb(ccb);
			ccb = NULL;
		}

		if (ahci_pmp_phy_status(ap, pmp_port, &data)) {
			printf("%s.%d: unable to clear PHY status\n",
			    PORTNAME(ap), pmp_port);
		}
		ahci_pwrite(ap, AHCI_PREG_SERR, -1);
		/* maybe don't do this on the first loop: */
		ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_IFS);
		ahci_pmp_write(ap, pmp_port, SATA_PMREG_SERR, -1);

		/* send first softreset FIS */
		ccb = ahci_get_pmp_ccb(ap);
		cmd_slot = ccb->ccb_cmd_hdr;
		bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));

		fis = ccb->ccb_cmd_table->cfis;
		fis[0] = ATA_FIS_TYPE_H2D;
		fis[1] = pmp_port;
		fis[15] = ATA_FIS_CONTROL_SRST | ATA_FIS_CONTROL_4BIT;

		cmd_slot->prdtl = 0;
		cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_C);
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_R);
		cmd_slot->flags |= htole16(pmp_port <<
		    AHCI_CMD_LIST_FLAG_PMP_SHIFT);

		ccb->ccb_xa.state = ATA_S_PENDING;

		DPRINTF(AHCI_D_VERBOSE, "%s.%d: sending PMP softreset cmd\n",
		    PORTNAME(ap), pmp_port);
		if (ahci_poll(ccb, 1000, ahci_pmp_probe_timeout) != 0) {
			printf("%s.%d: PMP port softreset cmd failed\n",
			       PORTNAME(ap), pmp_port);
			rc = EBUSY;
			if (count > 0) {
				/* probably delay a while to allow
				 * it to settle down?
				 */
			}
			continue;
		}

		/* send signature FIS */
		bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));
		fis[0] = ATA_FIS_TYPE_H2D;
		fis[1] = pmp_port;
		fis[15] = ATA_FIS_CONTROL_4BIT;

		cmd_slot->prdtl = 0;
		cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
		cmd_slot->flags |= htole16(pmp_port <<
		    AHCI_CMD_LIST_FLAG_PMP_SHIFT);

		DPRINTF(AHCI_D_VERBOSE, "%s.%d: sending PMP probe status cmd\n",
		    PORTNAME(ap), pmp_port);
		ccb->ccb_xa.state = ATA_S_PENDING;
		if (ahci_poll(ccb, 5000, ahci_pmp_probe_timeout) != 0) {
			DPRINTF(AHCI_D_VERBOSE, "%s.%d: PMP probe status cmd "
			    "failed\n", PORTNAME(ap), pmp_port);
			rc = EBUSY;
			if (count > 0) {
				/* sleep a while? */
			}
			continue;
		}

		fis[15] = 0;
		break;
	} while (count--);

	if (ccb != NULL) {
		ahci_put_pmp_ccb(ccb);
		ccb = NULL;
	}

	/* clean up a bit */
	ahci_pmp_write(ap, pmp_port, SATA_PMREG_SERR, -1);
	ahci_pwrite(ap, AHCI_PREG_SERR, -1);
	ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_IFS);
	ap->ap_pmp_ignore_ifs = 0;
	DPRINTF(AHCI_D_VERBOSE, "%s: no longer ignoring IFS\n", PORTNAME(ap));
	splx(s);

	return (rc);
}

int
ahci_pmp_port_probe(struct ahci_port *ap, int pmp_port)
{
	int sig;
	
	ap->ap_state = AP_S_PMP_PORT_PROBE;

	DPRINTF(AHCI_D_VERBOSE, "%s.%d: probing pmp port\n", PORTNAME(ap),
	    pmp_port);
	if (ahci_pmp_port_portreset(ap, pmp_port)) {
		printf("%s.%d: unable to probe PMP port; portreset failed\n",
		    PORTNAME(ap), pmp_port);
		ap->ap_state = AP_S_NORMAL;
		return (ATA_PORT_T_NONE);
	}

	if (ahci_pmp_port_softreset(ap, pmp_port)) {
		printf("%s.%d: unable to probe PMP port due to softreset "
		    "failure\n", PORTNAME(ap), pmp_port);
		ap->ap_state = AP_S_NORMAL;
		return (ATA_PORT_T_NONE);
	}

	sig = ahci_port_signature(ap);
	DPRINTF(AHCI_D_VERBOSE, "%s.%d: port signature returned %d\n",
	    PORTNAME(ap), pmp_port, sig);
	ap->ap_state = AP_S_NORMAL;
	return (sig);
}


void
ahci_flush_tfd(struct ahci_port *ap)
{
	u_int32_t r;

	r = ahci_pread(ap, AHCI_PREG_SERR);
	if (r & AHCI_PREG_SERR_DIAG_X)
		ahci_pwrite(ap, AHCI_PREG_SERR, AHCI_PREG_SERR_DIAG_X);
}

u_int32_t
ahci_active_mask(struct ahci_port *ap)
{
	u_int32_t mask;

	mask = ahci_pread(ap, AHCI_PREG_CI);
	if (ap->ap_sc->sc_cap & AHCI_REG_CAP_SNCQ)
		mask |= ahci_pread(ap, AHCI_PREG_SACT);
	return mask;
}

void
ahci_pmp_probe_timeout(void *cookie)
{
	struct ahci_ccb *ccb = cookie;
	struct ahci_port *ap = ccb->ccb_port;
	u_int32_t mask;

	DPRINTF(AHCI_D_VERBOSE, "%s: PMP probe cmd timed out\n", PORTNAME(ap));
	switch (ccb->ccb_xa.state) {
	case ATA_S_PENDING:
		TAILQ_REMOVE(&ap->ap_ccb_pending, ccb, ccb_entry);
		ccb->ccb_xa.state = ATA_S_TIMEOUT;
		break;

	case ATA_S_ONCHIP:
	case ATA_S_ERROR:  /* currently mostly here for the ATI SBx00 quirk */
		/* clear the command on-chip */
		KASSERT(ap->ap_active == (1 << ccb->ccb_slot) &&
		    ap->ap_sactive == 0);
		ahci_port_stop(ap, 0);
		ahci_port_start(ap, 0);

		if (ahci_active_mask(ap) != 0) {
			ahci_port_stop(ap, 0);
			ahci_port_start(ap, 0);
			mask = ahci_active_mask(ap);
			if (mask != 0) {
				printf("%s: ahci_pmp_probe_timeout: failed to "
				    "clear active cmds: %08x\n", PORTNAME(ap),
				    mask);
			}
		}

		ccb->ccb_xa.state = ATA_S_TIMEOUT;
		ap->ap_active &= ~(1 << ccb->ccb_slot);
		KASSERT(ap->ap_active_cnt > 0);
		--ap->ap_active_cnt;
		DPRINTF(AHCI_D_VERBOSE, "%s: timed out %d, active %x, count %d\n",
		    PORTNAME(ap), ccb->ccb_slot, ap->ap_active, ap->ap_active_cnt);
		break;

	default:
		panic("%s: ahci_pmp_probe_timeout: ccb in bad state %d",
			PORTNAME(ap), ccb->ccb_xa.state);
	}
}

int
ahci_port_signature(struct ahci_port *ap)
{
	u_int32_t sig;

	sig = ahci_pread(ap, AHCI_PREG_SIG);
	if ((sig & 0xffff0000) == (SATA_SIGNATURE_ATAPI & 0xffff0000))
		return (ATA_PORT_T_ATAPI);
	else if ((sig & 0xffff0000) == (SATA_SIGNATURE_PORT_MULTIPLIER &
	    0xffff0000))
		return (ATA_PORT_T_PM);
	else
		return (ATA_PORT_T_DISK);
}

int
ahci_pmp_port_portreset(struct ahci_port *ap, int pmp_port)
{
	u_int32_t cmd, data;
	int loop;
	int rc = 1;
	int s;

	s = splbio();
	DPRINTF(AHCI_D_VERBOSE, "%s.%d: PMP port reset\n", PORTNAME(ap),
	    pmp_port);

	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;

	/* turn off power management and disable the PHY */
	data = AHCI_PREG_SCTL_IPM_DISABLED;
	/* maybe add AHCI_PREG_SCTL_DET_DISABLE */
	if (ahci_pmp_write(ap, pmp_port, SATA_PMREG_SERR, -1))
		goto err;
	if (ahci_pmp_write(ap, pmp_port, SATA_PMREG_SCTL, data))
		goto err;
	delay(10000);

	/* start COMRESET */
	data = AHCI_PREG_SCTL_IPM_DISABLED | AHCI_PREG_SCTL_DET_INIT;
	if ((ap->ap_sc->sc_dev.dv_cfdata->cf_flags & 0x01) != 0) {
		DPRINTF(AHCI_D_VERBOSE, "%s.%d: forcing GEN1\n", PORTNAME(ap),
		    pmp_port);
		data |= AHCI_PREG_SCTL_SPD_GEN1;
	} else
		data |= AHCI_PREG_SCTL_SPD_ANY;
	
	if (ahci_pmp_write(ap, pmp_port, SATA_PMREG_SCTL, data))
		goto err;

	/* give it a while to settle down */
	delay(100000);

	if (ahci_pmp_phy_status(ap, pmp_port, &data)) {
		printf("%s.%d: cannot clear PHY status\n", PORTNAME(ap),
		    pmp_port);
	}

	/* start trying to negotiate */
	ahci_pmp_write(ap, pmp_port, SATA_PMREG_SERR, -1);
	data = AHCI_PREG_SCTL_IPM_DISABLED | AHCI_PREG_SCTL_DET_NONE;
	if (ahci_pmp_write(ap, pmp_port, SATA_PMREG_SCTL, data))
		goto err;

	/* give it a while to detect */
	for (loop = 3; loop; --loop) {
		if (ahci_pmp_read(ap, pmp_port, SATA_PMREG_SSTS, &data))
			goto err;
		if (data & AHCI_PREG_SSTS_DET)
			break;
		delay(100000);
	}
	if (loop == 0) {
		printf("%s.%d: port is unplugged\n", PORTNAME(ap), pmp_port);
		goto err;
	}

	/* give it even longer to fully negotiate */
	for (loop = 30; loop; --loop) {
		if (ahci_pmp_read(ap, pmp_port, SATA_PMREG_SSTS, &data))
			goto err;
		if ((data & AHCI_PREG_SSTS_DET) == AHCI_PREG_SSTS_DET_DEV)
			break;
		delay(100000);
	}

	if (loop == 0) {
		printf("%s.%d: device is not negotiating\n", PORTNAME(ap),
		    pmp_port);
		goto err;
	}

	/* device detected */
	DPRINTF(AHCI_D_VERBOSE, "%s.%d: device detected\n", PORTNAME(ap),
	    pmp_port);

	/* clean up a bit */
	delay(100000);
	ahci_pmp_write(ap, pmp_port, SATA_PMREG_SERR, -1);
	ahci_pwrite(ap, AHCI_PREG_SERR, -1);
	ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_IFS);

	rc = 0;
err:
	splx(s);
	return (rc);
}

/* AHCI port reset, Section 10.4.2 */
int
ahci_port_portreset(struct ahci_port *ap, int pmp)
{
	u_int32_t			cmd, r;
	int				rc, s;

	s = splbio();
	DPRINTF(AHCI_D_VERBOSE, "%s: port reset\n", PORTNAME(ap));

	/* Save previous command register state */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;

	/* Clear ST, ignoring failure */
	ahci_port_stop(ap, 0);

	/* Perform device detection */
	ahci_pwrite(ap, AHCI_PREG_SCTL, 0);
	delay(10000);
	r = AHCI_PREG_SCTL_IPM_DISABLED | AHCI_PREG_SCTL_DET_INIT;
	if ((ap->ap_sc->sc_dev.dv_cfdata->cf_flags & 0x01) != 0) {
		DPRINTF(AHCI_D_VERBOSE, "%s: forcing GEN1\n", PORTNAME(ap));
		r |= AHCI_PREG_SCTL_SPD_GEN1;
	} else
		r |= AHCI_PREG_SCTL_SPD_ANY;
	ahci_pwrite(ap, AHCI_PREG_SCTL, r);
	delay(10000);	/* wait at least 1ms for COMRESET to be sent */
	r &= ~AHCI_PREG_SCTL_DET_INIT;
	r |= AHCI_PREG_SCTL_DET_NONE;
	ahci_pwrite(ap, AHCI_PREG_SCTL, r);
	delay(10000);

	/* Wait for device to be detected and communications established */
	if (ahci_pwait_eq(ap, AHCI_PREG_SSTS, AHCI_PREG_SSTS_DET,
	    AHCI_PREG_SSTS_DET_DEV, 1)) {
		rc = ENODEV;
		if (ahci_pread(ap, AHCI_PREG_SSTS) & AHCI_PREG_SSTS_DET) {
			/* this may be a port multiplier with no device
			 * on port 0, so still do the pmp check if requested.
			 */
		} else {
			goto err;
		}
	} else {
		/* Clear SERR (incl X bit), so TFD can update */
		ahci_pwrite(ap, AHCI_PREG_SERR, ahci_pread(ap, AHCI_PREG_SERR));

		/* Wait for device to become ready */
		if (ahci_pwait_clr(ap, AHCI_PREG_TFD, AHCI_PREG_TFD_STS_BSY |
		    AHCI_PREG_TFD_STS_DRQ | AHCI_PREG_TFD_STS_ERR, 3)) {
			/* even if the device doesn't wake up, check if there's
			 * a port multiplier there
			 */
			rc = EBUSY;
		} else {
			rc = 0;
		}
	}

	if (pmp != 0) {
		if (ahci_port_detect_pmp(ap) != 0) {
			rc = EBUSY;
		}
	}

err:
	/* Restore preserved port state */
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd);
	splx(s);

	return (rc);
}

int
ahci_port_detect_pmp(struct ahci_port *ap)
{
	int				 count, pmp_rc, rc;
	u_int32_t			 r, cmd;
	struct ahci_cmd_hdr		*cmd_slot;
	struct ahci_ccb			*ccb = NULL;
	u_int8_t			*fis = NULL;

	if ((ap->ap_sc->sc_flags & AHCI_F_NO_PMP) ||
	    !ISSET(ahci_read(ap->ap_sc, AHCI_REG_CAP), AHCI_REG_CAP_SPM)) {
		return 0;
	}

	rc = 0;
	pmp_rc = 0;
	count = 2;
	do {
		DPRINTF(AHCI_D_VERBOSE, "%s: PMP probe %d\n", PORTNAME(ap),
		    count);
		if (ccb != NULL) {
			ahci_put_pmp_ccb(ccb);
			ccb = NULL;
		}
		ahci_port_stop(ap, 0);
		ap->ap_state = AP_S_PMP_PROBE;

		/* set PMA in cmd reg */
		cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
		if ((cmd & AHCI_PREG_CMD_PMA) == 0) {
			cmd |= AHCI_PREG_CMD_PMA;
			ahci_pwrite(ap, AHCI_PREG_CMD, cmd);
		}

		/* Flush errors and request CLO unconditionally,
		 * then start the port
		 */
		r = ahci_pread(ap, AHCI_PREG_SERR);
		if (r & AHCI_PREG_SERR_DIAG_X)
			ahci_pwrite(ap, AHCI_PREG_SERR,
			    AHCI_PREG_SERR_DIAG_X);
		
		/* Request CLO */
		ahci_port_clo(ap);

		/* Clear port errors to permit TFD transfer */
		r = ahci_pread(ap, AHCI_PREG_SERR);
		ahci_pwrite(ap, AHCI_PREG_SERR, r);

		/* XXX FBS: ensure we don't enable FBS here, since
		 * we're resetting the port
		 * (AHCI section 9.3.8)
		 */
		/* Restart port */
		if (ahci_port_start(ap, 0)) {
			rc = EBUSY;
			printf("%s: failed to start port, cannot probe PMP\n",
			    PORTNAME(ap));
			break;
		}

		/* Check whether CLO worked */
		if (ahci_pwait_clr(ap, AHCI_PREG_TFD,
		    AHCI_PREG_TFD_STS_BSY | AHCI_PREG_TFD_STS_DRQ, 1)) {
			u_int32_t cap;

			cap = ahci_read(ap->ap_sc, AHCI_REG_CAP);
			printf("%s: CLO %s, need port reset\n",
			    PORTNAME(ap),
			    ISSET(cap, AHCI_REG_CAP_SCLO)
			    ? "failed" : "unsupported");
			pmp_rc = EBUSY;
			break;
		}

		/* Prep first command with SRST feature &
		 * clear busy/reset flags
		 */
		ccb = ahci_get_pmp_ccb(ap);
		cmd_slot = ccb->ccb_cmd_hdr;
		bzero(ccb->ccb_cmd_table,
		    sizeof(struct ahci_cmd_table));

		fis = ccb->ccb_cmd_table->cfis;
		fis[0] = ATA_FIS_TYPE_H2D;
		fis[1] = SATA_PMP_CONTROL_PORT;
		fis[15] = ATA_FIS_CONTROL_SRST | ATA_FIS_CONTROL_4BIT;

		cmd_slot->prdtl = 0;
		cmd_slot->flags = htole16(5); /* FIS length: 5 DWORDS */
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_C);
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_R);
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_PMP);

		DPRINTF(AHCI_D_VERBOSE, "%s: sending PMP reset cmd\n",
		    PORTNAME(ap));
		ccb->ccb_xa.state = ATA_S_PENDING;
		if (ahci_poll(ccb, 1000, ahci_pmp_probe_timeout) != 0) {
			DPRINTF(AHCI_D_VERBOSE, "%s: PMP reset cmd failed\n",
			    PORTNAME(ap));
			pmp_rc = EBUSY;
			continue;
		}

		if (ahci_pwait_clr(ap, AHCI_PREG_TFD,
		    AHCI_PREG_TFD_STS_BSY | AHCI_PREG_TFD_STS_DRQ, 1)) {
			printf("%s: port busy after first PMP probe FIS\n",
			    PORTNAME(ap));
		}

		/* clear errors in case the device
		 * didn't reset cleanly
		 */
		ahci_flush_tfd(ap);
		r = ahci_pread(ap, AHCI_PREG_SERR);
		ahci_pwrite(ap, AHCI_PREG_SERR, r);

		/* Prep second command to read status and
		 * complete reset sequence
		 */
		bzero(ccb->ccb_cmd_table,
		    sizeof(struct ahci_cmd_table));
		fis[0] = ATA_FIS_TYPE_H2D;
		fis[1] = SATA_PMP_CONTROL_PORT;
		fis[15] = ATA_FIS_CONTROL_4BIT;

		cmd_slot->prdtl = 0;
		cmd_slot->flags = htole16(5); /* FIS length: 5 DWORDS */
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_PMP);

		DPRINTF(AHCI_D_VERBOSE, "%s: sending PMP probe status cmd\n",
		    PORTNAME(ap));
		ccb->ccb_xa.state = ATA_S_PENDING;
		if (ahci_poll(ccb, 5000, ahci_pmp_probe_timeout) != 0) {
			DPRINTF(AHCI_D_VERBOSE, "%s: PMP probe status "
			    "cmd failed\n", PORTNAME(ap));
			pmp_rc = EBUSY;
			continue;
		}

		/* apparently we need to retry at least once
		 * to get the right signature
		 */
		fis[15] = 0;
		pmp_rc = 0;
	} while (--count);

	if (ccb != NULL) {
		ahci_put_pmp_ccb(ccb);
		ccb = NULL;
	}

	if (ap->ap_state == AP_S_PMP_PROBE) {
		ap->ap_state = AP_S_NORMAL;
	}

	if (pmp_rc == 0) {
		if (ahci_port_signature(ap) != ATA_PORT_T_PM) {
			DPRINTF(AHCI_D_VERBOSE, "%s: device is not a PMP\n",
			    PORTNAME(ap));
			pmp_rc = EBUSY;
		} else {
			DPRINTF(AHCI_D_VERBOSE, "%s: PMP found\n",
			    PORTNAME(ap));
		}
	}

	if (pmp_rc == 0) {
		if (ahci_pmp_identify(ap, &ap->ap_pmp_ports)) {
			pmp_rc = EBUSY;
		} else {
			/* XXX enable FBS if available */
			rc = 0;
		}
	}

	/* if PMP detection failed, so turn off the PMA bit and
	 * reset the port again
	 */
	if (pmp_rc != 0) {
		DPRINTF(AHCI_D_VERBOSE, "%s: no PMP found, resetting "
		    "the port\n", PORTNAME(ap));
		ahci_port_stop(ap, 0);
		ahci_port_clo(ap);
		cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
		cmd &= ~AHCI_PREG_CMD_PMA;
		ahci_pwrite(ap, AHCI_PREG_CMD, cmd);

		ahci_pwrite(ap, AHCI_PREG_IE, 0);
		ahci_port_stop(ap, 0);
		if (ap->ap_sc->sc_cap & AHCI_REG_CAP_SSNTF)
			ahci_pwrite(ap, AHCI_PREG_SNTF, -1);
		ahci_flush_tfd(ap);
		ahci_pwrite(ap, AHCI_PREG_SERR, -1);

		ahci_pwrite(ap, AHCI_PREG_IS, -1);

		ahci_enable_interrupts(ap);

		ahci_port_portreset(ap, 0);
	}

	return (rc);
}

int
ahci_load_prdt(struct ahci_ccb *ccb)
{
	struct ahci_port		*ap = ccb->ccb_port;
	struct ahci_softc		*sc = ap->ap_sc;
	struct ata_xfer			*xa = &ccb->ccb_xa;
	struct ahci_prdt		*prdt = ccb->ccb_cmd_table->prdt, *prd;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	struct ahci_cmd_hdr		*cmd_slot = ccb->ccb_cmd_hdr;
	u_int64_t			addr;
	int				i, error;

	if (xa->datalen == 0) {
		ccb->ccb_cmd_hdr->prdtl = 0;
		return (0);
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap, xa->data, xa->datalen, NULL,
	    (xa->flags & ATA_F_NOWAIT) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: error %d loading dmamap\n", PORTNAME(ap), error);
		return (1);
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		prd = &prdt[i];

		addr = dmap->dm_segs[i].ds_addr;
		prd->dba_hi = htole32((u_int32_t)(addr >> 32));
		prd->dba_lo = htole32((u_int32_t)addr);
#ifdef DIAGNOSTIC
		if (addr & 1) {
			printf("%s: requested DMA at an odd address %llx\n",
			    PORTNAME(ap), (unsigned long long)addr);
			goto diagerr;
		}
		if (dmap->dm_segs[i].ds_len & 1) {
			printf("%s: requested DMA length %d is not even\n",
			    PORTNAME(ap), (int)dmap->dm_segs[i].ds_len);
			goto diagerr;
		}
#endif
		prd->flags = htole32(dmap->dm_segs[i].ds_len - 1);
	}
	if (xa->flags & ATA_F_PIO)
		prd->flags |= htole32(AHCI_PRDT_FLAG_INTR);

	cmd_slot->prdtl = htole16(dmap->dm_nsegs);

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xa->flags & ATA_F_READ) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);

#ifdef DIAGNOSTIC
diagerr:
	bus_dmamap_unload(sc->sc_dmat, dmap);
	return (1);
#endif
}

void
ahci_unload_prdt(struct ahci_ccb *ccb)
{
	struct ahci_port		*ap = ccb->ccb_port;
	struct ahci_softc		*sc = ap->ap_sc;
	struct ata_xfer			*xa = &ccb->ccb_xa;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;

	if (xa->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    (xa->flags & ATA_F_READ) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, dmap);

		if (ccb->ccb_xa.flags & ATA_F_NCQ)
			xa->resid = 0;
		else
			xa->resid = xa->datalen -
			    letoh32(ccb->ccb_cmd_hdr->prdbc);
	}
}

int
ahci_poll(struct ahci_ccb *ccb, int timeout, void (*timeout_fn)(void *))
{
	struct ahci_port		*ap = ccb->ccb_port;
	int				s;

	s = splbio();
	ahci_start(ccb);
	do {
		if (ISSET(ahci_port_intr(ap, AHCI_PREG_CI_ALL_SLOTS),
		    1 << ccb->ccb_slot)) {
			splx(s);
			return (0);
		}
		if (ccb->ccb_xa.state == ATA_S_ERROR) {
			DPRINTF(AHCI_D_VERBOSE, "%s: ccb in slot %d errored\n",
			    PORTNAME(ap), ccb->ccb_slot);
			/* pretend it timed out? */
			if (timeout_fn != NULL) {
				timeout_fn(ccb);
			}
			splx(s);
			return (1);
		}

		delay(1000);
	} while (--timeout > 0);

	/* Run timeout while at splbio, otherwise ahci_intr could interfere. */
	if (timeout_fn != NULL)
		timeout_fn(ccb);

	splx(s);

	return (1);
}

void
ahci_start(struct ahci_ccb *ccb)
{
	struct ahci_port		*ap = ccb->ccb_port;
	struct ahci_softc		*sc = ap->ap_sc;

	/* Zero transferred byte count before transfer */
	ccb->ccb_cmd_hdr->prdbc = 0;

	/* Sync command list entry and corresponding command table entry */
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_cmd_list),
	    ccb->ccb_slot * sizeof(struct ahci_cmd_hdr),
	    sizeof(struct ahci_cmd_hdr), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_cmd_table),
	    ccb->ccb_slot * sizeof(struct ahci_cmd_table),
	    sizeof(struct ahci_cmd_table), BUS_DMASYNC_PREWRITE);

	/* Prepare RFIS area for write by controller */
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_rfis), 0,
	    sizeof(struct ahci_rfis), BUS_DMASYNC_PREREAD);

	/* XXX FBS: need to figure out whether we still need to keep NCQ and
	 * non-queued commands separate when FBS is in use.  I guess probably
	 * not?  it's not particularly clear from the spec..
	 */

	if (ccb->ccb_xa.flags & ATA_F_NCQ) {
		/* Issue NCQ commands only when there are no outstanding
		 * standard commands. */
		if (ap->ap_active != 0 || !TAILQ_EMPTY(&ap->ap_ccb_pending) ||
		    (ap->ap_sactive != 0 &&
		     ap->ap_pmp_ncq_port != ccb->ccb_xa.pmp_port)) {
			TAILQ_INSERT_TAIL(&ap->ap_ccb_pending, ccb, ccb_entry);
		} else {
			/* XXX FBS: if using FBS, set AHCI_PREG_FBS_DEV
			 * to the port number
			 */

			KASSERT(ap->ap_active_cnt == 0);
			ap->ap_sactive |= (1 << ccb->ccb_slot);
			ccb->ccb_xa.state = ATA_S_ONCHIP;
			ahci_pwrite(ap, AHCI_PREG_SACT, 1 << ccb->ccb_slot);
			ahci_pwrite(ap, AHCI_PREG_CI, 1 << ccb->ccb_slot);
			ap->ap_pmp_ncq_port = ccb->ccb_xa.pmp_port;
		}
	} else {
		/* Wait for all NCQ commands to finish before issuing standard
		 * command. */
		if (ap->ap_sactive != 0 || ap->ap_active_cnt == 2)
			TAILQ_INSERT_TAIL(&ap->ap_ccb_pending, ccb, ccb_entry);
		else if (ap->ap_active_cnt < 2) {
			/* XXX FBS: if using FBS, set AHCI_PREG_FBS_DEV to the
			 * port number
			 */

			ap->ap_active |= 1 << ccb->ccb_slot;
			ccb->ccb_xa.state = ATA_S_ONCHIP;
			ahci_pwrite(ap, AHCI_PREG_CI, 1 << ccb->ccb_slot);
			ap->ap_active_cnt++;
		}
	}
}

void
ahci_issue_pending_ncq_commands(struct ahci_port *ap)
{
	struct ahci_ccb			*nextccb;
	u_int32_t			sact_change = 0;

	KASSERT(ap->ap_active_cnt == 0);

	nextccb = TAILQ_FIRST(&ap->ap_ccb_pending);
	if (nextccb == NULL || !(nextccb->ccb_xa.flags & ATA_F_NCQ))
		return;

	/* XXX FBS:
	 * - set AHCI_PREG_FBS_DEV for each command
	 * - one write to AHCI_PREG_CI per command
	 */

	/* Start all the NCQ commands at the head of the pending list.
	 * If a port multiplier is attached to the port, we can only
	 * issue commands for one of its ports at a time.
	 */
	if (ap->ap_sactive != 0 &&
	    ap->ap_pmp_ncq_port != nextccb->ccb_xa.pmp_port) {
		return;
	}

	ap->ap_pmp_ncq_port = nextccb->ccb_xa.pmp_port;
	do {
		TAILQ_REMOVE(&ap->ap_ccb_pending, nextccb, ccb_entry);
		sact_change |= 1 << nextccb->ccb_slot;
		nextccb->ccb_xa.state = ATA_S_ONCHIP;
		nextccb = TAILQ_FIRST(&ap->ap_ccb_pending);
	} while (nextccb && (nextccb->ccb_xa.flags & ATA_F_NCQ) &&
	    (nextccb->ccb_xa.pmp_port == ap->ap_pmp_ncq_port));

	ap->ap_sactive |= sact_change;
	ahci_pwrite(ap, AHCI_PREG_SACT, sact_change);
	ahci_pwrite(ap, AHCI_PREG_CI, sact_change);
}

void
ahci_issue_pending_commands(struct ahci_port *ap, int last_was_ncq)
{
	struct ahci_ccb			*nextccb;

	nextccb = TAILQ_FIRST(&ap->ap_ccb_pending);
	if (nextccb && (nextccb->ccb_xa.flags & ATA_F_NCQ)) {
		if (last_was_ncq) {
			KASSERT(nextccb->ccb_xa.pmp_port !=
			    ap->ap_pmp_ncq_port);
			/* otherwise it should have been started already */
		} else {
			ap->ap_active_cnt--;
		}

		/* Issue NCQ commands only when there are no outstanding
		 * standard commands, and previous NCQ commands for other
		 * PMP ports have finished.
		 */
		if (ap->ap_active == 0)
			ahci_issue_pending_ncq_commands(ap);
		else
			KASSERT(ap->ap_active_cnt == 1);
	} else if (nextccb) {
		if (ap->ap_sactive != 0 || last_was_ncq)
			KASSERT(ap->ap_active_cnt == 0);

		/* Wait for all NCQ commands to finish before issuing standard
		 * command. */
		if (ap->ap_sactive != 0)
			return;

		/* Keep up to 2 standard commands on-chip at a time. */
		do {
			TAILQ_REMOVE(&ap->ap_ccb_pending, nextccb, ccb_entry);
			ap->ap_active |= 1 << nextccb->ccb_slot;
			nextccb->ccb_xa.state = ATA_S_ONCHIP;
			/* XXX FBS: set AHCI_PREG_FBS_DEV here */
			ahci_pwrite(ap, AHCI_PREG_CI, 1 << nextccb->ccb_slot);
			if (last_was_ncq)
				ap->ap_active_cnt++;
			if (ap->ap_active_cnt == 2)
				break;
			KASSERT(ap->ap_active_cnt == 1);
			nextccb = TAILQ_FIRST(&ap->ap_ccb_pending);
		} while (nextccb && !(nextccb->ccb_xa.flags & ATA_F_NCQ));
	} else if (!last_was_ncq) {
		KASSERT(ap->ap_active_cnt == 1 || ap->ap_active_cnt == 2);

		/* Standard command finished, none waiting to start. */
		ap->ap_active_cnt--;
	} else {
		KASSERT(ap->ap_active_cnt == 0);

		/* NCQ command finished. */
	}
}

int
ahci_intr(void *arg)
{
	struct ahci_softc		*sc = arg;
	u_int32_t			is, ack = 0;
	int				port;

	/* Read global interrupt status */
	is = ahci_read(sc, AHCI_REG_IS);
	if (is == 0 || is == 0xffffffff)
		return (0);
	ack = is;

#ifdef AHCI_COALESCE
	/* Check coalescing interrupt first */
	if (is & sc->sc_ccc_mask) {
		DPRINTF(AHCI_D_INTR, "%s: command coalescing interrupt\n",
		    DEVNAME(sc));
		is &= ~sc->sc_ccc_mask;
		is |= sc->sc_ccc_ports_cur;
	}
#endif

	/* Process interrupts for each port */
	while (is) {
		port = ffs(is) - 1;
		if (sc->sc_ports[port])
			ahci_port_intr(sc->sc_ports[port],
			    AHCI_PREG_CI_ALL_SLOTS);
		is &= ~(1 << port);
	}

	/* Finally, acknowledge global interrupt */
	ahci_write(sc, AHCI_REG_IS, ack);

	return (1);
}

u_int32_t
ahci_port_intr(struct ahci_port *ap, u_int32_t ci_mask)
{
	struct ahci_softc		*sc = ap->ap_sc;
	u_int32_t			is, ci_saved, ci_masked, processed = 0;
	int				slot, need_restart = 0;
	int				process_error = 0;
	struct ahci_ccb			*ccb;
	volatile u_int32_t		*active;
#ifdef DIAGNOSTIC
	u_int32_t			tmp;
#endif

	is = ahci_pread(ap, AHCI_PREG_IS);

	/* Ack port interrupt only if checking all command slots. */
	if (ci_mask == AHCI_PREG_CI_ALL_SLOTS)
		ahci_pwrite(ap, AHCI_PREG_IS, is);

	if (is)
		DPRINTF(AHCI_D_INTR, "%s: interrupt: %b\n", PORTNAME(ap),
		    is, AHCI_PFMT_IS);

	if (ap->ap_sactive) {
		/* Active NCQ commands - use SActive instead of CI */
		KASSERT(ap->ap_active == 0);
		KASSERT(ap->ap_active_cnt == 0);
		ci_saved = ahci_pread(ap, AHCI_PREG_SACT);
		active = &ap->ap_sactive;
	} else {
		/* Save CI */
		ci_saved = ahci_pread(ap, AHCI_PREG_CI);
		active = &ap->ap_active;
	}

	if (is & AHCI_PREG_IS_TFES) {
		process_error = 1;
	} else if (is & AHCI_PREG_IS_DHRS) {
		u_int32_t tfd;
		u_int32_t cmd;
		u_int32_t serr;

		tfd = ahci_pread(ap, AHCI_PREG_TFD);
		cmd = ahci_pread(ap, AHCI_PREG_CMD);
		serr = ahci_pread(ap, AHCI_PREG_SERR);
		if ((tfd & AHCI_PREG_TFD_STS_ERR) &&
		    (cmd & AHCI_PREG_CMD_CR) == 0) {
			DPRINTF(AHCI_D_VERBOSE, "%s: DHRS error, TFD: %b, SERR:"
			    " %b, DIAG: %b\n", PORTNAME(ap), tfd,
			    AHCI_PFMT_TFD_STS, AHCI_PREG_SERR_ERR(serr),
			    AHCI_PFMT_SERR_ERR, AHCI_PREG_SERR_DIAG(serr),
			    AHCI_PFMT_SERR_DIAG);
			process_error = 1;
		} else {
			/* rfis copy back is in the normal execution path */
			ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_DHRS);
		}
	}

	/* Command failed.  See AHCI 1.1 spec 6.2.2.1 and 6.2.2.2. */
	if (process_error) {
		u_int32_t		tfd, serr;
		int			err_slot;

		tfd = ahci_pread(ap, AHCI_PREG_TFD);
		serr = ahci_pread(ap, AHCI_PREG_SERR);

		if (ap->ap_sactive == 0) {
			/* Errored slot is easy to determine from CMD. */
			err_slot = AHCI_PREG_CMD_CCS(ahci_pread(ap,
			    AHCI_PREG_CMD));

			if ((ci_saved & (1 << err_slot)) == 0) {
				/*
				 * Hardware doesn't seem to report correct
				 * slot number. If there's only one
				 * outstanding command we can cope,
				 * otherwise fail all active commands.
				 */
				if (ap->ap_active_cnt == 1)
					err_slot = ffs(ap->ap_active) - 1;
				else
					goto failall;
			}

			ccb = &ap->ap_ccbs[err_slot];

			/* Preserve received taskfile data from the RFIS. */
			memcpy(&ccb->ccb_xa.rfis, ap->ap_rfis->rfis,
			    sizeof(struct ata_fis_d2h));
		} else
			err_slot = -1;	/* Must extract error from log page */

		DPRINTF(AHCI_D_VERBOSE, "%s: errored slot %d, TFD: %b, SERR:"
		    " %b, DIAG: %b\n", PORTNAME(ap), err_slot, tfd,
		    AHCI_PFMT_TFD_STS, AHCI_PREG_SERR_ERR(serr),
		    AHCI_PFMT_SERR_ERR, AHCI_PREG_SERR_DIAG(serr),
		    AHCI_PFMT_SERR_DIAG);

		/* Turn off ST to clear CI and SACT. */
		ahci_port_stop(ap, 0);
		need_restart = 1;

		/* Clear SERR to enable capturing new errors. */
		ahci_pwrite(ap, AHCI_PREG_SERR, serr);

		/* Acknowledge the interrupts we can recover from. */
		ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_TFES |
		    AHCI_PREG_IS_IFS);
		is = ahci_pread(ap, AHCI_PREG_IS);

		/* If device hasn't cleared its busy status, try to idle it. */
		if (ISSET(tfd, AHCI_PREG_TFD_STS_BSY | AHCI_PREG_TFD_STS_DRQ)) {

			if (ap->ap_state == AP_S_PMP_PORT_PROBE) {
				/* can't reset the port here, just make sure
				 * the probe fails and the port still works.
				 */
			} else if (ap->ap_pmp_ports != 0 && err_slot != -1) {
				printf("%s: error on PMP port %d, idling "
				    "device\n", PORTNAME(ap),
				    ccb->ccb_xa.pmp_port);
				if (ahci_pmp_port_softreset(ap,
				        ccb->ccb_xa.pmp_port) == 0) {
					printf("%s: unable to softreset port "
					    "%d\n", PORTNAME(ap),
					    ccb->ccb_xa.pmp_port);
					if (ahci_pmp_port_portreset(ap,
						ccb->ccb_xa.pmp_port)) {
						printf("%s: failed to port "
						    " reset %d, giving up on "
						    "it\n", PORTNAME(ap),
						    ccb->ccb_xa.pmp_port);
						goto fatal;
					}
				}
			} else {
				printf("%s: attempting to idle device\n",
				    PORTNAME(ap));
				if (ahci_port_softreset(ap)) {
					printf("%s: failed to soft reset "
					    "device\n", PORTNAME(ap));
					if (ahci_port_portreset(ap, 0)) {
						printf("%s: failed to port "
						    "reset device, give up on "
						    "it\n", PORTNAME(ap));
						goto fatal;
					}
				}
			}

			/* Had to reset device, can't gather extended info. */
		} else if (ap->ap_sactive) {
			/* Recover the NCQ error from log page 10h.
			 * XXX FBS: need to do things to figure out where the
			 * error came from.  without FBS, we know the PMP port
			 * responsible because we can only have queued commands
			 * active for one port at a time.
			 */
			ahci_port_read_ncq_error(ap, &err_slot,
			    ap->ap_pmp_ncq_port);
			if (err_slot < 0)
				goto failall;

			DPRINTF(AHCI_D_VERBOSE, "%s: NCQ errored slot %d\n",
				PORTNAME(ap), err_slot);

			ccb = &ap->ap_ccbs[err_slot];
		} else {
			/* Didn't reset, could gather extended info from log. */
		}

		/*
		 * If we couldn't determine the errored slot, reset the port
		 * and fail all the active slots.
		 */
		if (err_slot == -1) {
			if (ahci_port_softreset(ap) != 0 &&
			    ahci_port_portreset(ap, 0) != 0) {
				printf("%s: couldn't reset after NCQ error, "
				    "disabling device.\n", PORTNAME(ap));
				goto fatal;
			}
			printf("%s: couldn't recover NCQ error, failing "
			    "all outstanding commands.\n", PORTNAME(ap));
			goto failall;
		}

		/* Clear the failed command in saved CI so completion runs. */
		ci_saved &= ~(1 << err_slot);

		/* Note the error in the ata_xfer. */
		KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
		ccb->ccb_xa.state = ATA_S_ERROR;

#ifdef DIAGNOSTIC
		/* There may only be one outstanding standard command now. */
		if (ap->ap_sactive == 0) {
			tmp = ci_saved;
			if (tmp) {
				slot = ffs(tmp) - 1;
				tmp &= ~(1 << slot);
				KASSERT(tmp == 0);
			}
		}
#endif
	}

	/* ATI SBx00 AHCI controllers respond to PMP probes with IPMS interrupts
	 * when there's a normal SATA device attached.
	 */
	if ((ap->ap_state == AP_S_PMP_PROBE) &&
	    (ap->ap_sc->sc_flags & AHCI_F_IPMS_PROBE) &&
	    (is & AHCI_PREG_IS_IPMS)) {
		slot = AHCI_PREG_CMD_CCS(ahci_pread(ap, AHCI_PREG_CMD));
		DPRINTF(AHCI_D_INTR, "%s: slot %d received IPMS\n",
		    PORTNAME(ap), slot);

		ccb = &ap->ap_ccbs[slot];
		ccb->ccb_xa.state = ATA_S_ERROR;

		ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_IPMS);
		is &= ~AHCI_PREG_IS_IPMS;
	}

	/* ignore IFS errors while resetting a PMP port */
	if ((is & AHCI_PREG_IS_IFS) /*&& ap->ap_pmp_ignore_ifs*/) {
		DPRINTF(AHCI_D_INTR, "%s: ignoring IFS while resetting PMP "
		    "port\n", PORTNAME(ap));

		need_restart = 1;
		ahci_pwrite(ap, AHCI_PREG_SERR, -1);
		ahci_pwrite(ap, AHCI_PREG_IS, AHCI_PREG_IS_IFS);
		is &= ~AHCI_PREG_IS_IFS;
		goto failall;
	}

	/* Check for remaining errors - they are fatal. */
	if (is & (AHCI_PREG_IS_TFES | AHCI_PREG_IS_HBFS | AHCI_PREG_IS_IFS |
	    AHCI_PREG_IS_OFS | AHCI_PREG_IS_UFS)) {
		printf("%s: unrecoverable errors (IS: %b), disabling port.\n",
		    PORTNAME(ap), is, AHCI_PFMT_IS);

		/* XXX try recovery first */
		goto fatal;
	}

	/* Fail all outstanding commands if we know the port won't recover. */
	if (ap->ap_state == AP_S_FATAL_ERROR) {
fatal:
		ap->ap_state = AP_S_FATAL_ERROR;
failall:

		/* Ensure port is shut down. */
		ahci_port_stop(ap, 1);

		/* Error all the active slots. */
		ci_masked = ci_saved & *active;
		while (ci_masked) {
			slot = ffs(ci_masked) - 1;
			ccb = &ap->ap_ccbs[slot];
			ci_masked &= ~(1 << slot);
			ccb->ccb_xa.state = ATA_S_ERROR;
		}

		/* Run completion for all active slots. */
		ci_saved &= ~*active;

		/* Don't restart the port if our problems were deemed fatal. */
		if (ap->ap_state == AP_S_FATAL_ERROR)
			need_restart = 0;
	}

	/*
	 * CCB completion is detected by noticing its slot's bit in CI has
	 * changed to zero some time after we activated it.
	 * If we are polling, we may only be interested in particular slot(s).
	 */
	ci_masked = ~ci_saved & *active & ci_mask;
	while (ci_masked) {
		slot = ffs(ci_masked) - 1;
		ccb = &ap->ap_ccbs[slot];
		ci_masked &= ~(1 << slot);

		DPRINTF(AHCI_D_INTR, "%s: slot %d is complete%s\n",
		    PORTNAME(ap), slot, ccb->ccb_xa.state == ATA_S_ERROR ?
		    " (error)" : "");

		bus_dmamap_sync(sc->sc_dmat,
		    AHCI_DMA_MAP(ap->ap_dmamem_cmd_list),
		    ccb->ccb_slot * sizeof(struct ahci_cmd_hdr),
		    sizeof(struct ahci_cmd_hdr), BUS_DMASYNC_POSTWRITE);

		bus_dmamap_sync(sc->sc_dmat,
		    AHCI_DMA_MAP(ap->ap_dmamem_cmd_table),
		    ccb->ccb_slot * sizeof(struct ahci_cmd_table),
		    sizeof(struct ahci_cmd_table), BUS_DMASYNC_POSTWRITE);

		bus_dmamap_sync(sc->sc_dmat,
		    AHCI_DMA_MAP(ap->ap_dmamem_rfis), 0,
		    sizeof(struct ahci_rfis), BUS_DMASYNC_POSTREAD);

		*active &= ~(1 << ccb->ccb_slot);
		/* Copy the rfis into the ccb if we were asked for it */
		if (ccb->ccb_xa.state == ATA_S_ONCHIP &&
		    ccb->ccb_xa.flags & ATA_F_GET_RFIS) {
			memcpy(&ccb->ccb_xa.rfis,
			       ap->ap_rfis->rfis,
			       sizeof(struct ata_fis_d2h));
		}

		ccb->ccb_done(ccb);

		processed |= 1 << ccb->ccb_slot;
	}

	if (need_restart) {
		/* Restart command DMA on the port */
		ahci_port_start(ap, 0);

		/* Re-enable outstanding commands on port. */
		if (ci_saved) {
#ifdef DIAGNOSTIC
			tmp = ci_saved;
			while (tmp) {
				slot = ffs(tmp) - 1;
				tmp &= ~(1 << slot);
				ccb = &ap->ap_ccbs[slot];
				KASSERT(ccb->ccb_xa.state == ATA_S_ONCHIP);
				KASSERT((!!(ccb->ccb_xa.flags & ATA_F_NCQ)) ==
				    (!!ap->ap_sactive));
			}
#endif
			DPRINTF(AHCI_D_VERBOSE, "%s: ahci_port_intr "
			    "re-enabling%s slots %08x\n", PORTNAME(ap),
			    ap->ap_sactive ? " NCQ" : "", ci_saved);

			/* XXX FBS:
			 * - need to set AHCI_PREG_FBS_DEV for each command
			 * - can't do multiple commands with a single write to
			 *   AHCI_PREG_CI
			 */
			if (ap->ap_sactive)
				ahci_pwrite(ap, AHCI_PREG_SACT, ci_saved);
			ahci_pwrite(ap, AHCI_PREG_CI, ci_saved);
		}
	}

	return (processed);
}

struct ahci_ccb *
ahci_get_ccb(struct ahci_port *ap)
{
	struct ahci_ccb			*ccb;

	mtx_enter(&ap->ap_ccb_mtx);
	ccb = TAILQ_FIRST(&ap->ap_ccb_free);
	if (ccb != NULL) {
		KASSERT(ccb->ccb_xa.state == ATA_S_PUT);
		TAILQ_REMOVE(&ap->ap_ccb_free, ccb, ccb_entry);
		ccb->ccb_xa.state = ATA_S_SETUP;
	}
	mtx_leave(&ap->ap_ccb_mtx);

	return (ccb);
}

void
ahci_put_ccb(struct ahci_ccb *ccb)
{
	struct ahci_port		*ap = ccb->ccb_port;

#ifdef DIAGNOSTIC
	if (ccb->ccb_xa.state != ATA_S_COMPLETE &&
	    ccb->ccb_xa.state != ATA_S_TIMEOUT &&
	    ccb->ccb_xa.state != ATA_S_ERROR) {
		printf("%s: invalid ata_xfer state %02x in ahci_put_ccb, "
		    "slot %d\n", PORTNAME(ccb->ccb_port), ccb->ccb_xa.state,
		    ccb->ccb_slot);
	}
#endif

	ccb->ccb_xa.state = ATA_S_PUT;
	mtx_enter(&ap->ap_ccb_mtx);
	TAILQ_INSERT_TAIL(&ap->ap_ccb_free, ccb, ccb_entry);
	mtx_leave(&ap->ap_ccb_mtx);
}

struct ahci_ccb *
ahci_get_err_ccb(struct ahci_port *ap)
{
	struct ahci_ccb *err_ccb;
	u_int32_t sact;

	splassert(IPL_BIO);

	/* No commands may be active on the chip. */
	sact = ahci_pread(ap, AHCI_PREG_SACT);
	if (sact != 0)
		printf("ahci_get_err_ccb but SACT %08x != 0?\n", sact);
	KASSERT(ahci_pread(ap, AHCI_PREG_CI) == 0);

#ifdef DIAGNOSTIC
	KASSERT(ap->ap_err_busy == 0);
	ap->ap_err_busy = 1;
#endif
	/* Save outstanding command state. */
	ap->ap_err_saved_active = ap->ap_active;
	ap->ap_err_saved_active_cnt = ap->ap_active_cnt;
	ap->ap_err_saved_sactive = ap->ap_sactive;

	/*
	 * Pretend we have no commands outstanding, so that completions won't
	 * run prematurely.
	 */
	ap->ap_active = ap->ap_active_cnt = ap->ap_sactive = 0;

	/*
	 * Grab a CCB to use for error recovery.  This should never fail, as
	 * we ask atascsi to reserve one for us at init time.
	 */
	err_ccb = ap->ap_ccb_err;
	err_ccb->ccb_xa.flags = 0;
	err_ccb->ccb_done = ahci_empty_done;

	return (err_ccb);
}

void
ahci_put_err_ccb(struct ahci_ccb *ccb)
{
	struct ahci_port *ap = ccb->ccb_port;
	u_int32_t sact;

	splassert(IPL_BIO);

#ifdef DIAGNOSTIC
	KASSERT(ap->ap_err_busy);
#endif
	/* No commands may be active on the chip */
	sact = ahci_pread(ap, AHCI_PREG_SACT);
	if (sact != 0)
		printf("ahci_port_err_ccb_restore but SACT %08x != 0?\n", sact);
	KASSERT(ahci_pread(ap, AHCI_PREG_CI) == 0);

#ifdef DIAGNOSTIC
	/* Done with the CCB */
	KASSERT(ccb == ap->ap_ccb_err);
#endif

	/* Restore outstanding command state */
	ap->ap_sactive = ap->ap_err_saved_sactive;
	ap->ap_active_cnt = ap->ap_err_saved_active_cnt;
	ap->ap_active = ap->ap_err_saved_active;

#ifdef DIAGNOSTIC
	ap->ap_err_busy = 0;
#endif
}

struct ahci_ccb *
ahci_get_pmp_ccb(struct ahci_port *ap)
{
	struct ahci_ccb *ccb;
	u_int32_t sact;

	/* some PMP commands need to be issued on slot 1,
	 * particularly the command that clears SRST and
	 * fetches the device signature.
	 *
	 * ensure the chip is idle and ccb 1 is available.
	 */
	splassert(IPL_BIO);

	sact = ahci_pread(ap, AHCI_PREG_SACT);
	if (sact != 0)
		printf("ahci_get_pmp_ccb; SACT %08x != 0\n", sact);
	KASSERT(ahci_pread(ap, AHCI_PREG_CI) == 0);

	ccb = &ap->ap_ccbs[1];
	KASSERT(ccb->ccb_xa.state == ATA_S_PUT);
	ccb->ccb_xa.flags = 0;
	ccb->ccb_done = ahci_pmp_cmd_done;

	mtx_enter(&ap->ap_ccb_mtx);
	TAILQ_REMOVE(&ap->ap_ccb_free, ccb, ccb_entry);
	mtx_leave(&ap->ap_ccb_mtx);

	return ccb;
}

void
ahci_put_pmp_ccb(struct ahci_ccb *ccb)
{
	struct ahci_port *ap = ccb->ccb_port;
	u_int32_t sact;
	
	/* make sure this is the right ccb */
	KASSERT(ccb == &ap->ap_ccbs[1]);

	/* No commands may be active on the chip */
	sact = ahci_pread(ap, AHCI_PREG_SACT);
	if (sact != 0)
		printf("ahci_port_err_ccb_restore but SACT %08x != 0?\n", sact);
	KASSERT(ahci_pread(ap, AHCI_PREG_CI) == 0);

	ccb->ccb_xa.state = ATA_S_PUT;
	mtx_enter(&ap->ap_ccb_mtx);
	TAILQ_INSERT_TAIL(&ap->ap_ccb_free, ccb, ccb_entry);
	mtx_leave(&ap->ap_ccb_mtx);
}

int
ahci_port_read_ncq_error(struct ahci_port *ap, int *err_slotp, int pmp_port)
{
	struct ahci_ccb			*ccb;
	struct ahci_cmd_hdr		*cmd_slot;
	u_int32_t			cmd;
	struct ata_fis_h2d		*fis;
	int				rc = EIO;

	DPRINTF(AHCI_D_VERBOSE, "%s: read log page\n", PORTNAME(ap));

	/* Save command register state. */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;

	/* Port should have been idled already.  Start it. */
	KASSERT((cmd & AHCI_PREG_CMD_CR) == 0);
	ahci_port_start(ap, 0);

	/* Prep error CCB for READ LOG EXT, page 10h, 1 sector. */
	ccb = ahci_get_err_ccb(ap);
	ccb->ccb_xa.flags = ATA_F_NOWAIT | ATA_F_READ | ATA_F_POLL;
	ccb->ccb_xa.data = ap->ap_err_scratch;
	ccb->ccb_xa.datalen = 512;
	cmd_slot = ccb->ccb_cmd_hdr;
	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));

	fis = (struct ata_fis_h2d *)ccb->ccb_cmd_table->cfis;
	fis->type = ATA_FIS_TYPE_H2D;
	fis->flags = ATA_H2D_FLAGS_CMD | pmp_port;
	fis->command = ATA_C_READ_LOG_EXT;
	fis->lba_low = 0x10;		/* queued error log page (10h) */
	fis->sector_count = 1;		/* number of sectors (1) */
	fis->sector_count_exp = 0;
	fis->lba_mid = 0;		/* starting offset */
	fis->lba_mid_exp = 0;
	fis->device = 0;

	cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
	cmd_slot->flags |= htole16(pmp_port << AHCI_CMD_LIST_FLAG_PMP_SHIFT);

	if (ahci_load_prdt(ccb) != 0) {
		rc = ENOMEM;	/* XXX caller must abort all commands */
		goto err;
	}

	ccb->ccb_xa.state = ATA_S_PENDING;
	if (ahci_poll(ccb, 1000, NULL) != 0)
		goto err;

	rc = 0;
err:
	/* Abort our command, if it failed, by stopping command DMA. */
	if (rc != 0 && ISSET(ap->ap_active, 1 << ccb->ccb_slot)) {
		printf("%s: log page read failed, slot %d was still active.\n",
		    PORTNAME(ap), ccb->ccb_slot);
		ahci_port_stop(ap, 0);
	}

	/* Done with the error CCB now. */
	ahci_unload_prdt(ccb);
	ahci_put_err_ccb(ccb);

	/* Extract failed register set and tags from the scratch space. */
	if (rc == 0) {
		struct ata_log_page_10h		*log;
		int				err_slot;

		log = (struct ata_log_page_10h *)ap->ap_err_scratch;
		if (ISSET(log->err_regs.type, ATA_LOG_10H_TYPE_NOTQUEUED)) {
			/* Not queued bit was set - wasn't an NCQ error? */
			printf("%s: read NCQ error page, but not an NCQ "
			    "error?\n", PORTNAME(ap));
			rc = ESRCH;
		} else {
			/* Copy back the log record as a D2H register FIS. */
			*err_slotp = err_slot = log->err_regs.type &
			    ATA_LOG_10H_TYPE_TAG_MASK;

			ccb = &ap->ap_ccbs[err_slot];
			memcpy(&ccb->ccb_xa.rfis, &log->err_regs,
			    sizeof(struct ata_fis_d2h));
			ccb->ccb_xa.rfis.type = ATA_FIS_TYPE_D2H;
			ccb->ccb_xa.rfis.flags = 0;
		}
	}

	/* Restore saved CMD register state */
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd);

	return (rc);
}

struct ahci_dmamem *
ahci_dmamem_alloc(struct ahci_softc *sc, size_t size)
{
	struct ahci_dmamem		*adm;
	int				nsegs;

	adm = malloc(sizeof(*adm), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (adm == NULL)
		return (NULL);

	adm->adm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &adm->adm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, adm->adm_map, adm->adm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (adm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
admfree:
	free(adm, M_DEVBUF);

	return (NULL);
}

void
ahci_dmamem_free(struct ahci_softc *sc, struct ahci_dmamem *adm)
{
	bus_dmamap_unload(sc->sc_dmat, adm->adm_map);
	bus_dmamem_unmap(sc->sc_dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(sc->sc_dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, adm->adm_map);
	free(adm, M_DEVBUF);
}

u_int32_t
ahci_read(struct ahci_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, r));
}

void
ahci_write(struct ahci_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
ahci_wait_ne(struct ahci_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}

u_int32_t
ahci_pread(struct ahci_port *ap, bus_size_t r)
{
	bus_space_barrier(ap->ap_sc->sc_iot, ap->ap_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(ap->ap_sc->sc_iot, ap->ap_ioh, r));
}

void
ahci_pwrite(struct ahci_port *ap, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(ap->ap_sc->sc_iot, ap->ap_ioh, r, v);
	bus_space_barrier(ap->ap_sc->sc_iot, ap->ap_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
ahci_pwait_eq(struct ahci_port *ap, bus_size_t r, u_int32_t mask,
    u_int32_t target, int n)
{
	int				i;

	for (i = 0; i < n * 1000; i++) {
		if ((ahci_pread(ap, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
ahci_ata_probe(void *xsc, int port, int lun)
{
	struct ahci_softc		*sc = xsc;
	struct ahci_port		*ap = sc->sc_ports[port];

	if (ap == NULL)
		return (ATA_PORT_T_NONE);

	if (lun != 0) {
		int pmp_port = lun - 1;
		if (pmp_port >= ap->ap_pmp_ports) {
			return (ATA_PORT_T_NONE);
		}
		return (ahci_pmp_port_probe(ap, pmp_port));
	} else {
		return (ahci_port_signature(ap));
	}
}

void
ahci_ata_free(void *xsc, int port, int lun)
{

}

struct ata_xfer *
ahci_ata_get_xfer(void *aaa_cookie, int port)
{
	struct ahci_softc		*sc = aaa_cookie;
	struct ahci_port		*ap = sc->sc_ports[port];
	struct ahci_ccb			*ccb;

	ccb = ahci_get_ccb(ap);
	if (ccb == NULL) {
		DPRINTF(AHCI_D_XFER, "%s: ahci_ata_get_xfer: NULL ccb\n",
		    PORTNAME(ap));
		return (NULL);
	}

	DPRINTF(AHCI_D_XFER, "%s: ahci_ata_get_xfer got slot %d\n",
	    PORTNAME(ap), ccb->ccb_slot);

	return ((struct ata_xfer *)ccb);
}

void
ahci_ata_put_xfer(struct ata_xfer *xa)
{
	struct ahci_ccb			*ccb = (struct ahci_ccb *)xa;

	DPRINTF(AHCI_D_XFER, "ahci_ata_put_xfer slot %d\n", ccb->ccb_slot);

	ahci_put_ccb(ccb);
}

void
ahci_ata_cmd(struct ata_xfer *xa)
{
	struct ahci_ccb			*ccb = (struct ahci_ccb *)xa;
	struct ahci_cmd_hdr		*cmd_slot;
	int				s;

	if (ccb->ccb_port->ap_state == AP_S_FATAL_ERROR)
		goto failcmd;

	ccb->ccb_done = ahci_ata_cmd_done;

	cmd_slot = ccb->ccb_cmd_hdr;
	cmd_slot->flags = htole16(5); /* FIS length (in DWORDs) */
	cmd_slot->flags |= htole16(xa->pmp_port <<
	    AHCI_CMD_LIST_FLAG_PMP_SHIFT);

	if (xa->flags & ATA_F_WRITE)
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_W);

	if (xa->flags & ATA_F_PACKET)
		cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_A);

	if (ahci_load_prdt(ccb) != 0)
		goto failcmd;

	timeout_set(&xa->stimeout, ahci_ata_cmd_timeout, ccb);

	xa->state = ATA_S_PENDING;

	if (xa->flags & ATA_F_POLL)
		ahci_poll(ccb, xa->timeout, ahci_ata_cmd_timeout);
	else {
		timeout_add_msec(&xa->stimeout, xa->timeout);

		s = splbio();
		ahci_start(ccb);
		splx(s);
	}

	return;

failcmd:
	s = splbio();
	xa->state = ATA_S_ERROR;
	ata_complete(xa);
	splx(s);
}

void
ahci_pmp_cmd_done(struct ahci_ccb *ccb)
{
	struct ata_xfer			*xa = &ccb->ccb_xa;

	if (xa->state == ATA_S_ONCHIP || xa->state == ATA_S_ERROR)
		ahci_issue_pending_commands(ccb->ccb_port,
		    xa->flags & ATA_F_NCQ);

	xa->state = ATA_S_COMPLETE;
}


void
ahci_ata_cmd_done(struct ahci_ccb *ccb)
{
	struct ata_xfer			*xa = &ccb->ccb_xa;

	timeout_del(&xa->stimeout);

	if (xa->state == ATA_S_ONCHIP || xa->state == ATA_S_ERROR)
		ahci_issue_pending_commands(ccb->ccb_port,
		    xa->flags & ATA_F_NCQ);

	ahci_unload_prdt(ccb);

	if (xa->state == ATA_S_ONCHIP)
		xa->state = ATA_S_COMPLETE;
#ifdef DIAGNOSTIC
	else if (xa->state != ATA_S_ERROR && xa->state != ATA_S_TIMEOUT)
		printf("%s: invalid ata_xfer state %02x in ahci_ata_cmd_done, "
		    "slot %d\n", PORTNAME(ccb->ccb_port), xa->state,
		    ccb->ccb_slot);
#endif
	if (xa->state != ATA_S_TIMEOUT)
		ata_complete(xa);
}

void
ahci_ata_cmd_timeout(void *arg)
{
	struct ahci_ccb			*ccb = arg;
	struct ata_xfer			*xa = &ccb->ccb_xa;
	struct ahci_port		*ap = ccb->ccb_port;
	int				s, ccb_was_started, ncq_cmd;
	volatile u_int32_t		*active;

	s = splbio();

	ncq_cmd = (xa->flags & ATA_F_NCQ);
	active = ncq_cmd ? &ap->ap_sactive : &ap->ap_active;

	if (ccb->ccb_xa.state == ATA_S_PENDING) {
		DPRINTF(AHCI_D_TIMEOUT, "%s: command for slot %d timed out "
		    "before it got on chip\n", PORTNAME(ap), ccb->ccb_slot);
		TAILQ_REMOVE(&ap->ap_ccb_pending, ccb, ccb_entry);
		ccb_was_started = 0;
	} else if (ccb->ccb_xa.state == ATA_S_ONCHIP && ahci_port_intr(ap,
	    1 << ccb->ccb_slot)) {
		DPRINTF(AHCI_D_TIMEOUT, "%s: final poll of port completed "
		    "command in slot %d\n", PORTNAME(ap), ccb->ccb_slot);
		goto ret;
	} else if (ccb->ccb_xa.state != ATA_S_ONCHIP) {
		DPRINTF(AHCI_D_TIMEOUT, "%s: command slot %d already "
		    "handled%s\n", PORTNAME(ap), ccb->ccb_slot,
		    ISSET(*active, 1 << ccb->ccb_slot) ?
		    " but slot is still active?" : ".");
		goto ret;
	} else if (!ISSET(ahci_pread(ap, ncq_cmd ? AHCI_PREG_SACT :
	    AHCI_PREG_CI), 1 << ccb->ccb_slot) && ISSET(*active,
	    1 << ccb->ccb_slot)) {
		DPRINTF(AHCI_D_TIMEOUT, "%s: command slot %d completed but "
		    "IRQ handler didn't detect it.  Why?\n", PORTNAME(ap),
		    ccb->ccb_slot);
		*active &= ~(1 << ccb->ccb_slot);
		ccb->ccb_done(ccb);
		goto ret;
	} else {
		ccb_was_started = 1;
	}

	/* Complete the slot with a timeout error. */
	ccb->ccb_xa.state = ATA_S_TIMEOUT;
	*active &= ~(1 << ccb->ccb_slot);
	DPRINTF(AHCI_D_TIMEOUT, "%s: run completion (1)\n", PORTNAME(ap));
	ccb->ccb_done(ccb);	/* This won't issue pending commands or run the
				   atascsi completion. */

	/* Reset port to abort running command. */
	if (ccb_was_started) {
		DPRINTF(AHCI_D_TIMEOUT, "%s: resetting port to abort%s command "
		    "in slot %d, pmp port %d, active %08x\n", PORTNAME(ap),
		    ncq_cmd ? " NCQ" : "", ccb->ccb_slot, xa->pmp_port, *active);
		if (ahci_port_softreset(ap) != 0 && ahci_port_portreset(ap, 0)
		    != 0) {
			printf("%s: failed to reset port during timeout "
			    "handling, disabling it\n", PORTNAME(ap));
			ap->ap_state = AP_S_FATAL_ERROR;
		}

		/* Restart any other commands that were aborted by the reset. */
		if (*active) {
			DPRINTF(AHCI_D_TIMEOUT, "%s: re-enabling%s slots "
			    "%08x\n", PORTNAME(ap), ncq_cmd ? " NCQ" : "",
			    *active);
			if (ncq_cmd)
				ahci_pwrite(ap, AHCI_PREG_SACT, *active);
			ahci_pwrite(ap, AHCI_PREG_CI, *active);
		}
	}

	/* Issue any pending commands now. */
	DPRINTF(AHCI_D_TIMEOUT, "%s: issue pending\n", PORTNAME(ap));
	if (ccb_was_started)
		ahci_issue_pending_commands(ap, ncq_cmd);
	else if (ap->ap_active == 0)
		ahci_issue_pending_ncq_commands(ap);

	/* Complete the timed out ata_xfer I/O (may generate new I/O). */
	DPRINTF(AHCI_D_TIMEOUT, "%s: run completion (2)\n", PORTNAME(ap));
	ata_complete(xa);

	DPRINTF(AHCI_D_TIMEOUT, "%s: splx\n", PORTNAME(ap));
ret:
	splx(s);
}

void
ahci_empty_done(struct ahci_ccb *ccb)
{
	ccb->ccb_xa.state = ATA_S_COMPLETE;
}

int
ahci_pmp_read(struct ahci_port *ap, int target, int which, u_int32_t *datap)
{
	struct ahci_ccb	*ccb;
	struct ata_fis_h2d *fis;
	int error;

	ccb = ahci_get_pmp_ccb(ap);
	if (ccb == NULL) {
		printf("%s: NULL ccb!\n", PORTNAME(ap));
		return (1);
	}
	ccb->ccb_xa.flags = ATA_F_POLL | ATA_F_GET_RFIS;
	ccb->ccb_xa.pmp_port = SATA_PMP_CONTROL_PORT;
	ccb->ccb_xa.state = ATA_S_PENDING;
	
	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));
	fis = (struct ata_fis_h2d *)ccb->ccb_cmd_table->cfis;
	fis->type = ATA_FIS_TYPE_H2D;
	fis->flags = ATA_H2D_FLAGS_CMD | SATA_PMP_CONTROL_PORT;
	fis->command = ATA_C_READ_PM;
	fis->features = which;
	fis->device = target | ATA_H2D_DEVICE_LBA;
	fis->control = ATA_FIS_CONTROL_4BIT;

	if (ahci_poll(ccb, 1000, ahci_pmp_probe_timeout) != 0) {
		error = 1;
	} else {
		*datap = ccb->ccb_xa.rfis.sector_count |
		    (ccb->ccb_xa.rfis.lba_low << 8) |
		    (ccb->ccb_xa.rfis.lba_mid << 16) |
		    (ccb->ccb_xa.rfis.lba_high << 24);
		error = 0;
	}
	ahci_put_pmp_ccb(ccb);
	return (error);
}

int
ahci_pmp_write(struct ahci_port *ap, int target, int which, u_int32_t data)
{
	struct ahci_ccb	*ccb;
	struct ata_fis_h2d *fis;
	int error;

	ccb = ahci_get_pmp_ccb(ap);
	if (ccb == NULL) {
		printf("%s: NULL ccb!\n", PORTNAME(ap));
		return (1);
	}
	ccb->ccb_xa.flags = ATA_F_POLL;
	ccb->ccb_xa.pmp_port = SATA_PMP_CONTROL_PORT;
	ccb->ccb_xa.state = ATA_S_PENDING;

	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));
	fis = (struct ata_fis_h2d *)ccb->ccb_cmd_table->cfis;
	fis->type = ATA_FIS_TYPE_H2D;
	fis->flags = ATA_H2D_FLAGS_CMD | SATA_PMP_CONTROL_PORT;
	fis->command = ATA_C_WRITE_PM;
	fis->features = which;
	fis->device = target | ATA_H2D_DEVICE_LBA;
	fis->sector_count = (u_int8_t)data;
	fis->lba_low = (u_int8_t)(data >> 8);
	fis->lba_mid = (u_int8_t)(data >> 16);
	fis->lba_high = (u_int8_t)(data >> 24);
	fis->control = ATA_FIS_CONTROL_4BIT;

	error = ahci_poll(ccb, 1000, ahci_pmp_probe_timeout);
	ahci_put_pmp_ccb(ccb);
	return (error);
}

int
ahci_pmp_phy_status(struct ahci_port *ap, int target, u_int32_t *datap)
{
	int error;

	error = ahci_pmp_read(ap, target, SATA_PMREG_SSTS, datap);
	if (error == 0)
		error = ahci_pmp_write(ap, target, SATA_PMREG_SERR, -1);
	if (error)
		*datap = 0;

	return (error);
}

int
ahci_pmp_identify(struct ahci_port *ap, int *ret_nports)
{
	u_int32_t chipid;
	u_int32_t rev;
	u_int32_t nports;
	u_int32_t features;
	u_int32_t enabled;
	int s;

	s = splbio();

	if (ahci_pmp_read(ap, 15, 0, &chipid) ||
	    ahci_pmp_read(ap, 15, 1, &rev) ||
	    ahci_pmp_read(ap, 15, 2, &nports) ||
	    ahci_pmp_read(ap, 15, SATA_PMREG_FEA, &features) ||
	    ahci_pmp_read(ap, 15, SATA_PMREG_FEAEN, &enabled)) {
		printf("%s: port multiplier identification failed\n",
		    PORTNAME(ap));
		splx(s);
		return (1);
	}
	splx(s);

	nports &= 0x0F;

	/* ignore SEMB port on SiI3726 port multiplier chips */
	if (chipid == 0x37261095) {
		nports--;
	}

	printf("%s: port multiplier found: chip=%08x rev=0x%b nports=%d, "
	    "features: 0x%b, enabled: 0x%b\n", PORTNAME(ap), chipid, rev,
	    SATA_PFMT_PM_REV, nports, features, SATA_PFMT_PM_FEA, enabled,
	    SATA_PFMT_PM_FEA);

	*ret_nports = nports;
	return (0);
}
