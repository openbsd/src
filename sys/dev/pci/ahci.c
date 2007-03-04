/*	$OpenBSD: ahci.c,v 1.62 2007/03/04 14:40:41 pascoe Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
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
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ata/atascsi.h>

#define AHCI_DEBUG

#ifdef AHCI_DEBUG
#define DPRINTF(m, f...) do { if (ahcidebug & (m)) printf(f); } while (0)
#define AHCI_D_VERBOSE		0x01
int ahcidebug = AHCI_D_VERBOSE;
#else
#define DPRINTF(m, f...)
#endif

#define AHCI_PCI_BAR		0x24

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
#define  AHCI_REG_CAP_SALP		(1<<26) /* Aggresive Link Pwr Mgmt */
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
#define AHCI_REG_CCC_CTL	0x014 /* Coalescing Control */
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
#define  AHCI_PREG_IS_SDBS		(1<<3) /* Set Devince Bits FIS */
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
#define  AHCI_PREG_IE_SDBE		(1<<3) /* Set Devince Bits FIS */
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
#define  AHCI_PREG_CMD_CCS		0x1f00 /* Current Command Slot */
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
#define AHCI_PREG_ACT		0x34 /* SATA Active */
#define AHCI_PREG_CI		0x38 /* Command Issue */
#define AHCI_PREG_SNTF		0x3c /* SNotification */

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

/* this makes ahci_cmd_table 512 bytes, which is good for alignment */
#define AHCI_MAX_PRDT		24

struct ahci_cmd_table {
	u_int8_t		cfis[64];	/* Command FIS */
	u_int8_t		acmd[16];	/* ATAPI Command */
	u_int8_t		reserved[48];

	struct ahci_prdt	prdt[AHCI_MAX_PRDT];
} __packed;

#define AHCI_MAX_PORTS		32

static const struct pci_matchid ahci_devices[] = {
	{ PCI_VENDOR_JMICRON,	PCI_PRODUCT_JMICRON_JMB361 }
};

int			ahci_match(struct device *, void *, void *);
void			ahci_attach(struct device *, struct device *, void *);

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
	int			ccb_slot;
	struct ahci_port	*ccb_port;

	struct ata_xfer		*ccb_xa;

	struct ahci_cmd_table	*ccb_cmd_table;
	u_int64_t		ccb_cmd_table_dva;

	bus_dmamap_t		ccb_dmamap;

	TAILQ_ENTRY(ahci_ccb)	ccb_entry;
};

struct ahci_port {
	struct ahci_softc	*ap_sc;
	bus_space_handle_t	ap_ioh;

	struct ahci_rfis	*ap_rfis;
	struct ahci_dmamem	*ap_dmamem_rfis;

	struct ahci_cmd_hdr	*ap_cmd_list;
	struct ahci_dmamem	*ap_dmamem_cmd_list;
	struct ahci_dmamem	*ap_dmamem_cmd_table;

	struct ahci_ccb		*ap_ccbs;
	TAILQ_HEAD(, ahci_ccb)	ap_ccb_free;

#ifdef AHCI_DEBUG
	char			ap_name[16];
#define PORTNAME(_ap)	((_ap)->ap_name)
#endif
};

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	u_int			sc_ncmds;
	struct ahci_port	*sc_ports[AHCI_MAX_PORTS];

	struct atascsi		*sc_atascsi;
};
#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

struct cfattach ahci_ca = {
	sizeof(struct ahci_softc), ahci_match, ahci_attach
};

struct cfdriver ahci_cd = {
	NULL, "ahci", DV_DULL
};

int			ahci_map_regs(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_unmap_regs(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_init(struct ahci_softc *);
int			ahci_map_intr(struct ahci_softc *,
			    struct pci_attach_args *);
void			ahci_unmap_intr(struct ahci_softc *,
			    struct pci_attach_args *);
int			ahci_port_alloc(struct ahci_softc *, u_int);
void			ahci_port_free(struct ahci_softc *, u_int);

int			ahci_port_start(struct ahci_port *, int);
int			ahci_port_stop(struct ahci_port *, int);
int			ahci_port_clo(struct ahci_port *);
int			ahci_port_portreset(struct ahci_port *);
int			ahci_port_softreset(struct ahci_port *);

int			ahci_load_prdt(struct ahci_ccb *, struct ata_xfer *);
int			ahci_start(struct ahci_ccb *);

int			ahci_intr(void *);

struct ahci_ccb		*ahci_get_ccb(struct ahci_port *);
void			ahci_put_ccb(struct ahci_port *, struct ahci_ccb *);

struct ahci_dmamem	*ahci_dmamem_alloc(struct ahci_softc *, size_t);
void			ahci_dmamem_free(struct ahci_softc *,
			    struct ahci_dmamem *);

u_int32_t		ahci_read(struct ahci_softc *, bus_size_t);
void			ahci_write(struct ahci_softc *, bus_size_t, u_int32_t);
int			ahci_wait_eq(struct ahci_softc *, bus_size_t,
			    u_int32_t, u_int32_t);
int			ahci_wait_ne(struct ahci_softc *, bus_size_t,
			    u_int32_t, u_int32_t);

u_int32_t		ahci_pread(struct ahci_port *, bus_size_t);
void			ahci_pwrite(struct ahci_port *, bus_size_t, u_int32_t);
int			ahci_pwait_eq(struct ahci_port *, bus_size_t,
			    u_int32_t, u_int32_t);
int			ahci_pwait_ne(struct ahci_port *, bus_size_t,
			    u_int32_t, u_int32_t);

/* Wait for all bits in _b to be cleared */
#define ahci_pwait_clr(_ap, _r, _b) ahci_pwait_eq((_ap), (_r), (_b), 0)

/* Wait for all bits in _b to be set */
#define ahci_pwait_set(_ap, _r, _b) ahci_pwait_eq((_ap), (_r), (_b), (_b))

/* provide methods for atascsi to call */
int			ahci_ata_probe(void *, int);
int			ahci_ata_cmd(void *, struct ata_xfer *);

struct atascsi_methods ahci_atascsi_methods = {
	ahci_ata_probe,
	ahci_ata_cmd
};

int
ahci_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, ahci_devices,
	    sizeof(ahci_devices) / sizeof(ahci_devices[0])));
}

void
ahci_attach(struct device *parent, struct device *self, void *aux)
{
	struct ahci_softc		*sc = (struct ahci_softc *)self;
	struct pci_attach_args		*pa = aux;
	struct atascsi_attach_args	aaa;
	u_int32_t			reg;
	int				i;

	/* Switch JMICRON ports to AHCI mode */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_JMICRON) {
		u_int32_t ccr;

		ccr = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x40);
		ccr &= ~0x0000ff00;
		ccr |=  0x0000a100;
		pci_conf_write(pa->pa_pc, pa->pa_tag, 0x40, ccr);

		/* Only function 0 is SATA */
		if (pa->pa_function != 0)
			return;
	}

	if (ahci_map_regs(sc, pa) != 0) {
		/* error already printed by ahci_map_regs */
		return;
	}

	if (ahci_init(sc) != 0) {
		/* error already printed by ahci_init */
		goto unmap;
	}

	if (ahci_map_intr(sc, pa) != 0) {
		/* error already printed by ahci_map_intr */
		goto unmap;
	}

	printf("\n");

	sc->sc_dmat = pa->pa_dmat;

	reg = ahci_read(sc, AHCI_REG_CAP);
	sc->sc_ncmds = AHCI_REG_CAP_NCS(reg);
#ifdef AHCI_DEBUG
	if (ahcidebug & AHCI_D_VERBOSE) {
		const char *gen;

		switch (reg & AHCI_REG_CAP_ISS) {
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

		printf("%s: capabilities: 0x%b ports: %d ncmds: %d gen: %s\n",
		    DEVNAME(sc), reg, AHCI_FMT_CAP,
		    AHCI_REG_CAP_NP(reg), sc->sc_ncmds, gen);
	}
#endif

	reg = ahci_read(sc, AHCI_REG_PI);
	DPRINTF(AHCI_D_VERBOSE, "%s: ports implemented: 0x%08x\n",
	    DEVNAME(sc), reg);
	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		if (!ISSET(reg, 1 << i)) {
			/* dont allocate stuff if the port isnt implemented */
			continue;
		}

		if (ahci_port_alloc(sc, i) == ENOMEM)
			goto freeports;
	}

	bzero(&aaa, sizeof(aaa));
	aaa.aaa_cookie = sc;
	aaa.aaa_methods = &ahci_atascsi_methods;
	aaa.aaa_minphys = minphys;
	aaa.aaa_nports = AHCI_MAX_PORTS;
	aaa.aaa_ncmds = sc->sc_ncmds;

	sc->sc_atascsi = atascsi_attach(self, &aaa);

	return;

freeports:
	for (i = 0; i < AHCI_MAX_PORTS; i++)
		if (sc->sc_ports[i] != NULL)
			ahci_port_free(sc, i);
unmap:
	ahci_unmap_regs(sc, pa);
}

int
ahci_map_regs(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			maptype;

	maptype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, AHCI_PCI_BAR);
	if (pci_mapreg_map(pa, AHCI_PCI_BAR, maptype, 0, &sc->sc_iot,
	    &sc->sc_ioh, NULL, &sc->sc_ios, 0) != 0) {
		printf(": unable to map registers\n");
		return (1);
	}

	return (0);
}

void
ahci_unmap_regs(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
ahci_map_intr(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pci_intr_handle_t		ih;
	const char			*intrstr;

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		return (1);
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO,
	    ahci_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL) {
		printf(": unable to map interrupt%s%s\n",
		    intrstr == NULL ? "" : " at ",
		    intrstr == NULL ? "" : intrstr);
		return (1);
	}
	printf(": %s", intrstr);

	return (0);
}

void
ahci_unmap_intr(struct ahci_softc *sc, struct pci_attach_args *pa)
{
	pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
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
			printf(": unable to reset controller\n");
			return (1);
		}
	}

	/* restore parameters */
	ahci_write(sc, AHCI_REG_CAP, cap);
	ahci_write(sc, AHCI_REG_PI, pi);

	/* enable ahci (global interrupts disabled) */
	ahci_write(sc, AHCI_REG_GHC, AHCI_REG_GHC_AE);

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

	default:
		printf(": unsupported AHCI revision 0x%08x\n", reg);
		return (1);
	}

	printf(": AHCI %s", revision);

	return (0);
}

int
ahci_port_alloc(struct ahci_softc *sc, u_int port)
{
	struct ahci_port		*ap;
	struct ahci_ccb			*ccb;
	u_int8_t			*kva;
	u_int64_t			dva;
	int				i, rc = ENOMEM;
	u_int32_t			cmd;

	ap = malloc(sizeof(struct ahci_port), M_DEVBUF, M_NOWAIT);
	if (ap == NULL) {
		printf("%s: unable to allocate memory for port %d\n",
		    DEVNAME(sc), port);
		goto reterr;
	}
	bzero(ap, sizeof(struct ahci_port));

#ifdef AHCI_DEBUG
	snprintf(ap->ap_name, sizeof(ap->ap_name), "%s.%d",
	    DEVNAME(sc), port);
#endif
	sc->sc_ports[port] = ap;

	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    AHCI_PORT_REGION(port), AHCI_PORT_SIZE, &ap->ap_ioh) != 0) {
		printf("%s: unable to create register window for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}

	ap->ap_sc = sc;

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

	/* Allocate RFIS */
	ap->ap_dmamem_rfis = ahci_dmamem_alloc(sc, sizeof(struct ahci_rfis));
	if (ap->ap_dmamem_rfis == NULL)
		goto nomem;

	/* Setup RFIS base address */
	kva = AHCI_DMA_KVA(ap->ap_dmamem_rfis);
	dva = AHCI_DMA_DVA(ap->ap_dmamem_rfis);
	ap->ap_rfis = (struct ahci_rfis *)kva;
	ahci_pwrite(ap, AHCI_PREG_FBU, htole32((u_int32_t)(dva >> 32)));
	ahci_pwrite(ap, AHCI_PREG_FB, htole32((u_int32_t)dva));

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
	    M_NOWAIT);
	if (ap->ap_ccbs == NULL) {
		printf("%s: unable to allocate command list for port %d\n",
		    DEVNAME(sc), port);
		goto freeport;
	}
	bzero(ap->ap_ccbs, sizeof(struct ahci_ccb) * sc->sc_ncmds);

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
	kva = AHCI_DMA_KVA(ap->ap_dmamem_cmd_list);
	dva = AHCI_DMA_DVA(ap->ap_dmamem_cmd_list);
	ap->ap_cmd_list = (struct ahci_cmd_hdr *)kva;
	ahci_pwrite(ap, AHCI_PREG_CLBU, htole32((u_int32_t)(dva >> 32)));
	ahci_pwrite(ap, AHCI_PREG_CLB, htole32((u_int32_t)dva));

	/* Split CCB allocation into CCBs and assign command table entries */
	TAILQ_INIT(&ap->ap_ccb_free);
	kva = AHCI_DMA_KVA(ap->ap_dmamem_cmd_table);
	dva = AHCI_DMA_DVA(ap->ap_dmamem_cmd_table);
	for (i = 0; i < sc->sc_ncmds; i++) {
		ccb = &ap->ap_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS, AHCI_MAX_PRDT,
		    MAXPHYS, 0, 0, &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dmamap for port %d "
			    "ccb %d\n", DEVNAME(sc), port, i);
			goto freeport;
		}

		/*
		 * NB: ahci_start assumes a 1:1 mapping of CCB slot number
		 * to command slot and command table positions in its
		 * bus_dmasync_calls.
		 */
		ccb->ccb_slot = i;
		ccb->ccb_port = ap;
		ccb->ccb_cmd_table = (struct ahci_cmd_table *)
		    (kva + i * sizeof(struct ahci_cmd_table));
		ccb->ccb_cmd_table_dva =
		    dva + i * sizeof(struct ahci_cmd_table);

		ahci_put_ccb(ap, ccb);
	}

	/* Wait for ICC change to complete */
	ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_ICC);

	/* Reset port */
	rc = ahci_port_portreset(ap);
	switch (rc) {
	case ENODEV:
		printf("%s: ", DEVNAME(sc));
		switch (ahci_pread(ap, AHCI_PREG_SSTS) & AHCI_PREG_SSTS_DET) {
		case AHCI_PREG_SSTS_DET_DEV_NE:
			printf("device not communicating");
			break;
		case AHCI_PREG_SSTS_DET_PHYOFFLINE:
			printf("PHY offline");
			break;
		default:
			printf("no device detected");
		}
		printf(" on port %d, disabling.\n", port);
		goto freeport;

	case EBUSY:
		printf("%s: device on port %d didn't come ready, TFD: 0x%b\n",
		    DEVNAME(sc), port, ahci_pread(ap, AHCI_PREG_TFD),
		    AHCI_PFMT_TFD_STS);

		/* Try a soft reset to clear busy */
		rc = ahci_port_softreset(ap);
		if (rc) {
			printf("%s: unable to communicate with device on port "
			    "%d, disabling\n", DEVNAME(sc), port);
			goto freeport;
		}
		break;

	default:
		break;
	}
	printf("%s: detected device on port %d\n", DEVNAME(sc), port);

	/* Enable command transfers on port */
	if (ahci_port_start(ap, 0)) {
		printf("%s: failed to start command DMA on port %d, "
		    "disabling\n", DEVNAME(sc), port);
		rc = ENXIO;	/* couldn't start port */
	}

	/* Flush interrupts for port */
	ahci_pwrite(ap, AHCI_PREG_IS, ahci_pread(ap, AHCI_PREG_IS));
	ahci_write(sc, AHCI_REG_IS, 1 << port);
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

	/* bus_space(9) says we dont free the subregions handle */

	free(ap, M_DEVBUF);
	sc->sc_ports[port] = NULL;
}

int
ahci_port_start(struct ahci_port *ap, int fre_only)
{
	u_int32_t			r;

	/* Turn on FRE (and ST) */
	r = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	r |= AHCI_PREG_CMD_FRE;
	if (!fre_only)
		r |= AHCI_PREG_CMD_ST;
	ahci_pwrite(ap, AHCI_PREG_CMD, r);

	/* Wait for FR to come on */
	if (ahci_pwait_set(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_FR))
		return (2);

	/* Wait for CR to come on */
	if (!fre_only && ahci_pwait_set(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CR))
		return (1);

	return (0);
}

int
ahci_port_stop(struct ahci_port *ap, int stop_fis_rx)
{
	u_int32_t			r;

	/* Turn off ST (and FRE) */
	r = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;
	r &= ~AHCI_PREG_CMD_ST;
	if (stop_fis_rx)
		r &= ~AHCI_PREG_CMD_FRE;
	ahci_pwrite(ap, AHCI_PREG_CMD, r);

	/* Wait for CR to go off */
	if (ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CR))
		return (1);

	/* Wait for FR to go off */
	if (stop_fis_rx && ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_FR))
		return (2);

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
	if (ahci_pwait_clr(ap, AHCI_PREG_CMD, AHCI_PREG_CMD_CLO)) {
		printf("%s: CLO did not complete\n", PORTNAME(ap));
		return (1);
	}

	return (0);
}

/* AHCI soft reset, Section 10.4.1 */
int
ahci_port_softreset(struct ahci_port *ap)
{
	struct ahci_ccb			*ccb;
	struct ahci_cmd_hdr		*cmd_slot;
	u_int8_t			*fis;
	int				s, rc = EIO;
	u_int32_t			cmd;
	struct ata_xfer			xa;

	DPRINTF(AHCI_D_VERBOSE, "%s: soft reset\n", PORTNAME(ap));

	s = splbio();
	ccb = ahci_get_ccb(ap);
	splx(s);
	if (ccb == NULL)
		goto err;

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

	/* Restart port */
	if (ahci_port_start(ap, 0)) {
		printf("%s: failed to start port, cannot softreset\n",
		    PORTNAME(ap));
		goto err;
	}

	/* Check whether CLO worked */
	if (ahci_pwait_clr(ap, AHCI_PREG_TFD,
	    AHCI_PREG_TFD_STS_BSY | AHCI_PREG_TFD_STS_DRQ)) {
		printf("%s: CLO %s, need port reset\n", PORTNAME(ap),
		    ISSET(ahci_read(ap->ap_sc, AHCI_REG_CAP), AHCI_REG_CAP_SCLO)
		    ? "failed" : "unsupported");
		rc = EBUSY;
		goto err;
	}

	/* Prep first D2H command with SRST feature & clear busy/reset flags */
	bzero(&xa, sizeof(struct ata_xfer));
	xa.flags = ATA_F_POLL | ATA_F_WRITE;
	ccb->ccb_xa = &xa;
	cmd_slot = &ap->ap_cmd_list[ccb->ccb_slot];
	bzero(cmd_slot, sizeof(struct ahci_cmd_hdr));
	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));

	fis = ccb->ccb_cmd_table->cfis;
	fis[0] = 0x27;	/* Host to device */
	fis[15] = 0x04;	/* SRST DEVCTL */

	cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_C); /* Clear busy on OK */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_R); /* Reset */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_W); /* Write */
	cmd_slot->ctba_hi = htole32((u_int32_t)(ccb->ccb_cmd_table_dva >> 32));
	cmd_slot->ctba_lo = htole32((u_int32_t)ccb->ccb_cmd_table_dva);

	if (ahci_start(ccb) != ATA_COMPLETE)
		goto err;

	/* Prep second D2H command to read status and complete reset sequence */
	bzero(&xa, sizeof(struct ata_xfer));
	xa.flags = ATA_F_POLL | ATA_F_WRITE;
	ccb->ccb_xa = &xa;
	cmd_slot = &ap->ap_cmd_list[ccb->ccb_slot];
	bzero(cmd_slot, sizeof(struct ahci_cmd_hdr));
	bzero(ccb->ccb_cmd_table, sizeof(struct ahci_cmd_table));

	fis = ccb->ccb_cmd_table->cfis;
	fis[0] = 0x27;	/* Host to device */

	cmd_slot->flags = htole16(5);	/* FIS length: 5 DWORDS */
	cmd_slot->flags |= htole16(AHCI_CMD_LIST_FLAG_W);
	cmd_slot->ctba_hi = htole32((u_int32_t)(ccb->ccb_cmd_table_dva >> 32));
	cmd_slot->ctba_lo = htole32((u_int32_t)ccb->ccb_cmd_table_dva);

	if (ahci_start(ccb) != ATA_COMPLETE)
		goto err;

	if (ahci_pwait_clr(ap, AHCI_PREG_TFD, AHCI_PREG_TFD_STS_BSY |
	    AHCI_PREG_TFD_STS_DRQ | AHCI_PREG_TFD_STS_ERR)) {
		printf("%s: device didn't come ready after reset, TFD: 0x%b\n",
		    PORTNAME(ap), ahci_pread(ap, AHCI_PREG_TFD),
		    AHCI_PFMT_TFD_STS);
		goto err;
	}

	rc = 0;
err:
	if (ccb != NULL)
		ahci_put_ccb(ap, ccb);

	/* Restore saved CMD register state */
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd);

	return (rc);
}

/* AHCI port reset, Section 10.4.2 */
int
ahci_port_portreset(struct ahci_port *ap)
{
	u_int32_t			cmd, r;
	int				rc;

	DPRINTF(AHCI_D_VERBOSE, "%s: port reset\n", PORTNAME(ap));

	/* Save previous command register state */
	cmd = ahci_pread(ap, AHCI_PREG_CMD) & ~AHCI_PREG_CMD_ICC;

	/* Clear ST, ignoring failure */
	ahci_port_stop(ap, 0);

	/* Perform device detection */
	ahci_pwrite(ap, AHCI_PREG_SCTL, 0);
	r = AHCI_PREG_SCTL_IPM_DISABLED | AHCI_PREG_SCTL_SPD_ANY |
	    AHCI_PREG_SCTL_DET_INIT;
	ahci_pwrite(ap, AHCI_PREG_SCTL, r);
	delay(2000);	/* wait at least 1ms for COMRESET to be sent */
	r &= ~AHCI_PREG_SCTL_DET_INIT;
	r |= AHCI_PREG_SCTL_DET_NONE;
	ahci_pwrite(ap, AHCI_PREG_SCTL, r);
	delay(2000);

	/* Wait for device to be detected and communications established */
	if (ahci_pwait_eq(ap, AHCI_PREG_SSTS, AHCI_PREG_SSTS_DET,
	    AHCI_PREG_SSTS_DET_DEV)) {
		rc = ENODEV;
		goto err;
	}

	/* Clear SERR (incl X bit), so TFD can update */
	ahci_pwrite(ap, AHCI_PREG_SERR, ahci_pread(ap, AHCI_PREG_SERR));

	/* Wait for device to become ready */
	/* XXX maybe more than the default wait is appropriate here? */
	if (ahci_pwait_clr(ap, AHCI_PREG_TFD, AHCI_PREG_TFD_STS_BSY |
	    AHCI_PREG_TFD_STS_DRQ | AHCI_PREG_TFD_STS_ERR)) {
		rc = EBUSY;
		goto err;
	}

	rc = 0;
err:
	/* Restore preserved port state */
	ahci_pwrite(ap, AHCI_PREG_CMD, cmd);

	return (rc);
}

int
ahci_load_prdt(struct ahci_ccb *ccb, struct ata_xfer *xa)
{
	struct ahci_port		*ap = ccb->ccb_port;
	struct ahci_softc		*sc = ap->ap_sc;
	struct ahci_prdt		*prdt = ccb->ccb_cmd_table->prdt, *prd;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	struct ahci_cmd_hdr		*cmd_slot;
	u_int64_t			addr;
	int				i, error;

	if (xa->datalen == 0)
		return (0);

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xa->data, xa->datalen, NULL,
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

		prd->flags = htole32(dmap->dm_segs[i].ds_len - 1);
	}
	prd->flags |= htole32(AHCI_PRDT_FLAG_INTR);

	cmd_slot = &ap->ap_cmd_list[ccb->ccb_slot];
	cmd_slot->prdtl = htole16(ccb->ccb_dmamap->dm_nsegs);
	cmd_slot->prdbc = 0;

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xa->flags & ATA_F_READ) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

int
ahci_start(struct ahci_ccb *ccb)
{
	struct ahci_port		*ap = ccb->ccb_port;
	struct ahci_softc		*sc = ap->ap_sc;
	int				rc = ATA_QUEUED;
	int				s;

	/* Sync command list entry and corresponding command table entry */
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_cmd_list),
	    ccb->ccb_slot * sizeof(struct ahci_cmd_hdr),
	    sizeof(struct ahci_cmd_hdr), BUS_DMASYNC_PREWRITE);
	/* NB this assumes a 1:1 mapping of ccb slot to command table entry */
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_cmd_table),
	    ccb->ccb_slot * sizeof(struct ahci_cmd_table),
	    sizeof(struct ahci_cmd_table), BUS_DMASYNC_PREWRITE);

	/* Prepare RFIS area for write by controller */
	bus_dmamap_sync(sc->sc_dmat, AHCI_DMA_MAP(ap->ap_dmamem_rfis), 0,
	    sizeof(struct ahci_rfis), BUS_DMASYNC_PREREAD);

	s = splbio();
	ahci_pwrite(ap, AHCI_PREG_CI, 1 << ccb->ccb_slot);
	if (ccb->ccb_xa->flags & ATA_F_POLL) {
		if (ahci_pwait_clr(ap, AHCI_PREG_CI, 1 << ccb->ccb_slot)) {
			/* Command didn't go inactive.  XXX: wait longer? */
			printf("%s: polled command didn't go inactive\n",
			    PORTNAME(ap));
			/* Shutdown port.  XXX: recover via port reset? */
			ahci_port_stop(ap, 0);
			rc = ATA_ERROR;
		} else
			rc = ATA_COMPLETE;
	}
	splx(s);

	return (rc);
}

int
ahci_intr(void *arg)
{
	return (0);
}

int
ahci_ata_probe(void *xsc, int port)
{
	return (0);
}

int
ahci_ata_cmd(void *xsc, struct ata_xfer *xa)
{
	return (0);
}

struct ahci_ccb *
ahci_get_ccb(struct ahci_port *ap)
{
	struct ahci_ccb			*ccb;

	ccb = TAILQ_FIRST(&ap->ap_ccb_free);
	if (ccb != NULL)
		TAILQ_REMOVE(&ap->ap_ccb_free, ccb, ccb_entry);

	return (ccb);
}

void
ahci_put_ccb(struct ahci_port *ap, struct ahci_ccb *ccb)
{
	/* scrub bits */
	TAILQ_INSERT_TAIL(&ap->ap_ccb_free, ccb, ccb_entry);
}

struct ahci_dmamem *
ahci_dmamem_alloc(struct ahci_softc *sc, size_t size)
{
	struct ahci_dmamem		*adm;
	int				nsegs;

	adm = malloc(sizeof(struct ahci_dmamem), M_DEVBUF, M_NOWAIT);
	if (adm == NULL)
		return (NULL);

	bzero(adm, sizeof(struct ahci_dmamem));
	adm->adm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &adm->adm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, adm->adm_map, adm->adm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(adm->adm_kva, size);

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
ahci_wait_eq(struct ahci_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
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
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_pread(ap, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
ahci_pwait_ne(struct ahci_port *ap, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	for (i = 0; i < 1000; i++) {
		if ((ahci_pread(ap, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}
