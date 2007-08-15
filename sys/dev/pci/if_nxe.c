/*	$OpenBSD: if_nxe.c,v 1.22 2007/08/15 02:40:15 dlg Exp $ */

/*
 * Copyright (c) 2007 David Gwynne <dlg@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#ifdef NXE_DEBUG
int nxedebug = 0;

#define DPRINTF(l, f...)	do { if (nxedebug & (l)) printf(f); } while (0)
#define DASSERT(_a)		assert(_a)
#else
#define DPRINTF(l, f...)
#define DASSERT(_a)
#endif

/* this driver likes firmwares around this version */
#define NXE_VERSION_MAJOR	3
#define NXE_VERSION_MINOR	4
#define NXE_VERSION_BUILD	31
#define NXE_VERSION \
    ((NXE_VERSION_MAJOR << 16)|(NXE_VERSION_MINOR << 8)|(NXE_VERSION_BUILD))


/*
 * PCI configuration space registers
 */

#define NXE_PCI_BAR_MEM		0x10 /* bar 0 */
#define NXE_PCI_BAR_MEM_128MB		(128 * 1024 * 1024)
#define NXE_PCI_BAR_DOORBELL	0x20 /* bar 4 */

/*
 * doorbell register space
 */

#define NXE_DB			0x00000000
#define  NXE_DB_PEGID			0x00000003
#define  NXE_DB_PEGID_RX		0x00000001 /* rx unit */
#define  NXE_DB_PEGID_TX		0x00000002 /* tx unit */
#define  NXE_DB_PRIVID			0x00000004 /* must be set */
#define  NXE_DB_COUNT(_c)		((_c)<<3) /* count */
#define  NXE_DB_CTXID(_c)		((_c)<<18) /* context id */
#define  NXE_DB_OPCODE_RX_PROD		0x00000000
#define  NXE_DB_OPCODE_RX_JUMBO_PROD	0x10000000
#define  NXE_DB_OPCODE_RX_LRO_PROD	0x20000000
#define  NXE_DB_OPCODE_CMD_PROD		0x30000000
#define  NXE_DB_OPCODE_UPD_CONS		0x40000000
#define  NXE_DB_OPCODE_RESET_CTX	0x50000000

/*
 * register space
 */

/* different PCI functions use different registers sometimes */
#define _F(_f)			((_f) * 0x20)

/*
 * driver ref section 4.2
 *
 * All the hardware registers are mapped in memory. Apart from the registers
 * for the individual hardware blocks, the memory map includes a large number
 * of software definable registers.
 *
 * The following table gives the memory map in the PCI address space.
 */

#define NXE_MAP_DDR_NET		0x00000000
#define NXE_MAP_DDR_MD		0x02000000
#define NXE_MAP_QDR_NET		0x04000000
#define NXE_MAP_DIRECT_CRB	0x04400000
#define NXE_MAP_OCM0		0x05000000
#define NXE_MAP_OCM1		0x05100000
#define NXE_MAP_CRB		0x06000000

/*
 * Since there are a large number of registers they do not fit in a single
 * PCI addressing range. Hence two windows are defined. The window starts at
 * NXE_MAP_CRB, and extends to the end of the register map. The window is set
 * using the NXE_REG_WINDOW_CRB register. The format of the NXE_REG_WINDOW_CRB
 * register is as follows:
 */

#define NXE_WIN_CRB(_f)		(0x06110210 + _F(_f))
#define  NXE_WIN_CRB_0			(0<<25)
#define  NXE_WIN_CRB_1			(1<<25)

/*
 * The memory map inside the register windows are divided into a set of blocks.
 * Each register block is owned by one hardware agent. The following table
 * gives the memory map of the various register blocks in window 0. These
 * registers are all in the CRB register space, so the offsets given here are
 * relative to the base of the CRB offset region (NXE_MAP_CRB).
 */

#define NXE_W0_PCIE		0x00100000 /* PCI Express */
#define NXE_W0_NIU		0x00600000 /* Network Interface Unit */
#define NXE_W0_PPE_0		0x01100000 /* Protocol Processing Engine 0 */
#define NXE_W0_PPE_1		0x01200000 /* Protocol Processing Engine 1 */
#define NXE_W0_PPE_2		0x01300000 /* Protocol Processing Engine 2 */
#define NXE_W0_PPE_3		0x01400000 /* Protocol Processing Engine 3 */
#define NXE_W0_PPE_D		0x01500000 /* PPE D-cache */
#define NXE_W0_PPE_I		0x01600000 /* PPE I-cache */

/*
 * These are the register blocks inside window 1.
 */

#define NXE_W1_PCIE		0x00100000
#define NXE_W1_SW		0x00200000
#define NXE_W1_SIR		0x01200000
#define NXE_W1_ROMUSB		0x01300000

/*
 * Global registers
 */
#define NXE_BOOTLD_START	0x00010000


/*
 * driver ref section 5
 *
 * CRB Window Register Descriptions
 */

/*
 * PCI Express Registers
 *
 * Despite being in the CRB window space, they can be accessed via both
 * windows. This means they are accessable "globally" without going relative
 * to the start of the CRB window space.
 */

/* Interrupts */
#define NXE_ISR_VECTOR		0x06110100 /* Interrupt Vector */
#define  NXE_ISR_VECTOR_FUNC(_f)	(0x08 << (_f))
#define NXE_ISR_MASK		0x06110104 /* Interrupt Mask */
#define NXE_ISR_TARGET_STATUS	0x06110118
#define NXE_ISR_TARGET_MASK	0x06110128

/* lock registers (semaphores between chipset and driver) */
#define NXE_SEM_ROM_LOCK	0x0611c010 /* ROM access lock */
#define NXE_SEM_ROM_UNLOCK	0x0611c014
#define NXE_SEM_PHY_LOCK	0x0611c018 /* PHY access lock */
#define NXE_SEM_PHY_UNLOCK	0x0611c01c
#define  NXE_SEM_DONE			0x1

/*
 * Network Interface Unit (NIU) Registers
 */

#define NXE_0_NIU_MODE		0x00600000
#define  NXE_0_NIU_MODE_XGE		(1<<2) /* XGE interface enabled */
#define  NXE_0_NIU_MODE_GBE		(1<<1) /* 4 GbE interfaces enabled */
#define NXE_0_NIU_SINGLE_TERM	0x00600004

#define NXE_0_NIU_RESET_XG	0x0060001c /* reset XG */
#define NXE_0_NIU_RESET_FIFO	0x00600088 /* reset sys fifos */

#define _P(_p)			((_p) * 0x10000)

#define NXE_0_XG_CFG0(_p)	(0x00670000 + _P(_p))
#define  NXE_0_XG_CFG0_TX_EN		(1<<0) /* TX enable */
#define  NXE_0_XG_CFG0_TX_SYNC		(1<<1) /* TX synced */
#define  NXE_0_XG_CFG0_RX_EN		(1<<2) /* RX enable */
#define  NXE_0_XG_CFG0_RX_SYNC		(1<<3) /* RX synced */
#define  NXE_0_XG_CFG0_TX_FLOWCTL	(1<<4) /* enable pause frame gen */
#define  NXE_0_XG_CFG0_RX_FLOWCTL	(1<<5) /* act on rxed pause frames */
#define  NXE_0_XG_CFG0_LOOPBACK		(1<<8) /* tx appears on rx */
#define  NXE_0_XG_CFG0_TX_RST_PB	(1<<15) /* reset frm tx proto block */
#define  NXE_0_XG_CFG0_RX_RST_PB	(1<<16) /* reset frm rx proto block */
#define  NXE_0_XG_CFG0_TX_RST_MAC	(1<<17) /* reset frm tx multiplexer */
#define  NXE_0_XG_CFG0_RX_RST_MAC	(1<<18) /* reset ctl frms and timers */
#define  NXE_0_XG_CFG0_SOFT_RST		(1<<31) /* soft reset */
#define NXE_0_XG_CFG1(_p)	(0x00670004 + _P(_p))
#define  NXE_0_XG_CFG1_REM_CRC		(1<<0) /* enable crc removal */
#define  NXE_0_XG_CFG1_CRC_EN		(1<<1) /* append crc to tx frames */
#define  NXE_0_XG_CFG1_NO_MAX		(1<<5) /* rx all frames despite size */
#define  NXE_0_XG_CFG1_WIRE_LO_ERR	(1<<6) /* recognize local err */
#define  NXE_0_XG_CFG1_PAUSE_FR_DIS	(1<<8) /* disable pause frame detect */
#define  NXE_0_XG_CFG1_SEQ_ERR_EN	(1<<10) /* enable seq err detection */
#define  NXE_0_XG_CFG1_MULTICAST	(1<<12) /* accept all multicast */
#define  NXE_0_XG_CFG1_PROMISC		(1<<13) /* accept all multicast */
#define NXE_0_XG_MAC_LO(_p)	(0x00670010 + _P(_p))
#define NXE_0_XG_MAC_HI(_p)	(0x0067000c + _P(_p))

/*
 * Software Defined Registers
 */

/* chipset state registers */
#define NXE_1_SW_ROM_LOCK_ID	0x00202100
#define  NXE_1_SW_ROM_LOCK_ID_DRV	0x0d417340
#define NXE_1_SW_PHY_LOCK_ID	0x00202120
#define  NXE_1_SW_PHY_LOCK_ID_DRV	0x44524956

/* firmware version */
#define NXE_1_SW_FWVER_MAJOR	0x00202150 /* Major f/w version */
#define NXE_1_SW_FWVER_MINOR	0x00202154 /* Minor f/w version */
#define NXE_1_SW_FWVER_BUILD	0x00202158 /* Build/Sub f/w version */

/* misc */
#define NXE_1_SW_CMD_ADDR_HI	0x00202218 /* cmd ring phys addr */
#define NXE_1_SW_CMD_ADDR_LO	0x0020221c /* cmd ring phys addr */
#define NXE_1_SW_CMD_SIZE	0x002022c8 /* entries in the cmd ring */
#define NXE_1_SW_DUMMY_ADDR_HI	0x0020223c /* hi address of dummy buf */
#define NXE_1_SW_DUMMY_ADDR_LO	0x00202240 /* lo address of dummy buf */
#define  NXE_1_SW_DUMMY_ADDR_LEN	1024

static const u_int32_t nxe_regmap[][4] = {
#define NXE_1_SW_CMD_PRODUCER(_f)	(nxe_regmap[0][(_f)])
    { 0x00202208, 0x002023ac, 0x002023b8, 0x002023d0 },
#define NXE_1_SW_CMD_CONSUMER(_f)	(nxe_regmap[1][(_f)])
    { 0x0020220c, 0x002023b0, 0x002023bc, 0x002023d4 },

#define NXE_1_SW_CONTEXT(_p)		(nxe_regmap[2][(_p)])
#define NXE_1_SW_CONTEXT_SIG(_p)	(0xdee0 | (_p))
    { 0x0020238c, 0x00202390, 0x0020239c, 0x002023a4 },
#define NXE_1_SW_CONTEXT_ADDR_LO(_p)	(nxe_regmap[3][(_p)])
    { 0x00202388, 0x00202390, 0x00202398, 0x002023a0 },
#define NXE_1_SW_CONTEXT_ADDR_HI(_p)	(nxe_regmap[4][(_p)])
    { 0x002023c0, 0x002023c4, 0x002023c8, 0x002023cc },

#define NXE_1_SW_INT_MASK(_p)		(nxe_regmap[5][(_p)])
    { 0x002023d8, 0x082023e0, 0x082023e4, 0x082023e8 },

#define NXE_1_SW_RX_PRODUCER(_c)	(nxe_regmap[6][(_c)])
    { 0x00202300, 0x00202344, 0x002023d8, 0x0020242c },
#define NXE_1_SW_RX_CONSUMER(_c)	(nxe_regmap[7][(_c)])
    { 0x00202304, 0x00202348, 0x002023dc, 0x00202430 },
#define NXE_1_SW_RX_RING(_c)		(nxe_regmap[8][(_c)])
    { 0x00202308, 0x0020234c, 0x002023f0, 0x00202434 },
#define NXE_1_SW_RX_SIZE(_c)		(nxe_regmap[9][(_c)])
    { 0x0020230c, 0x00202350, 0x002023f4, 0x00202438 },

#define NXE_1_SW_RX_JUMBO_PRODUCER(_c)	(nxe_regmap[10][(_c)])
    { 0x00202310, 0x00202354, 0x002023f8, 0x0020243c },
#define NXE_1_SW_RX_JUMBO_CONSUMER(_c)	(nxe_regmap[11][(_c)])
    { 0x00202314, 0x00202358, 0x002023fc, 0x00202440 },
#define NXE_1_SW_RX_JUMBO_RING(_c)	(nxe_regmap[12][(_c)])
    { 0x00202318, 0x0020235c, 0x00202400, 0x00202444 },
#define NXE_1_SW_RX_JUMBO_SIZE(_c)	(nxe_regmap[13][(_c)])
    { 0x0020231c, 0x00202360, 0x00202404, 0x00202448 },

#define NXE_1_SW_RX_LRO_PRODUCER(_c)	(nxe_regmap[14][(_c)])
    { 0x00202320, 0x00202364, 0x00202408, 0x0020244c },
#define NXE_1_SW_RX_LRO_CONSUMER(_c)	(nxe_regmap[15][(_c)])
    { 0x00202324, 0x00202368, 0x0020240c, 0x00202450 },
#define NXE_1_SW_RX_LRO_RING(_c)	(nxe_regmap[16][(_c)])
    { 0x00202328, 0x0020236c, 0x00202410, 0x00202454 },
#define NXE_1_SW_RX_LRO_SIZE(_c)	(nxe_regmap[17][(_c)])
    { 0x0020232c, 0x00202370, 0x00202414, 0x00202458 },

#define NXE_1_SW_STATUS_RING(_c)	(nxe_regmap[18][(_c)])
    { 0x00202330, 0x00202374, 0x00202418, 0x0020245c },
#define NXE_1_SW_STATUS_PRODUCER(_c)	(nxe_regmap[19][(_c)])
    { 0x00202334, 0x00202378, 0x0020241c, 0x00202460 },
#define NXE_1_SW_STATUS_CONSUMER(_c)	(nxe_regmap[20][(_c)])
    { 0x00202338, 0x0020237c, 0x00202420, 0x00202464 },
#define NXE_1_SW_STATUS_STATE(_c)	(nxe_regmap[21][(_c)])
    { 0x0020233c, 0x00202380, 0x00202424, 0x00202468 },
#define NXE_1_SW_STATUS_SIZE(_c)	(nxe_regmap[22][(_c)])
    { 0x00202340, 0x00202384, 0x00202428, 0x0020246c }
};


#define NXE_1_SW_BOOTLD_CONFIG	0x002021fc
#define  NXE_1_SW_BOOTLD_CONFIG_ROM	0x00000000
#define  NXE_1_SW_BOOTLD_CONFIG_RAM	0x12345678

#define NXE_1_SW_CMDPEG_STATE	0x00202250 /* init status */
#define  NXE_1_SW_CMDPEG_STATE_START	0xff00 /* init starting */
#define  NXE_1_SW_CMDPEG_STATE_DONE	0xff01 /* init complete */
#define  NXE_1_SW_CMDPEG_STATE_ACK	0xf00f /* init ack */
#define  NXE_1_SW_CMDPEG_STATE_ERROR	0xffff /* init failed */

#define NXE_1_SW_XG_STATE	0x00202294 /* phy state */
#define  NXE_1_SW_XG_STATE_PORT(_r, _p)	(((_r)>>8*(_p))&0xff)
#define  NXE_1_SW_XG_STATE_UP		(1<<4)
#define  NXE_1_SW_XG_STATE_DOWN		(1<<5)

#define NXE_1_SW_MPORT_MODE	0x002022c4
#define  NXE_1_SW_MPORT_MODE_SINGLE	0x1111
#define  NXE_1_SW_MPORT_MODE_MULTI	0x2222

#define NXE_1_SW_NIC_CAP_HOST	0x002023a8 /* host capabilities */
#define  NXE_1_SW_NIC_CAP_HOST_DEF	0x1 /* nfi */

#define  NXE_1_SW_DRIVER_VER	0x002024a0 /* host driver version */


#define NXE_1_SW_TEMP		0x002023b4 /* Temperature sensor */
#define  NXE_1_SW_TEMP_STATE(_x)	((_x)&0xffff) /* Temp state */
#define  NXE_1_SW_TEMP_STATE_NONE	0x0000
#define  NXE_1_SW_TEMP_STATE_OK		0x0001
#define  NXE_1_SW_TEMP_STATE_WARN	0x0002
#define  NXE_1_SW_TEMP_STATE_CRIT	0x0003
#define  NXE_1_SW_TEMP_VAL(_x)		(((_x)>>16)&0xffff) /* Temp value */

#define NXE_1_SW_V2P(_f)	(0x00202490+((_f)*4)) /* virtual to phys */

/*
 * ROMUSB Registers
 */
#define NXE_1_ROMUSB_STATUS	0x01300004 /* ROM Status */
#define  NXE_1_ROMUSB_STATUS_DONE	(1<<1)
#define NXE_1_ROMUSB_SW_RESET	0x01300008
#define NXE_1_ROMUSB_SW_RESET_DEF	0xffffffff
#define NXE_1_ROMUSB_SW_RESET_BOOT	0x0080000f

#define NXE_1_CASPER_RESET	0x01300038
#define  NXE_1_CASPER_RESET_ENABLE	0x1
#define  NXE_1_CASPER_RESET_DISABLE	0x1

#define NXE_1_GLB_PEGTUNE	0x0130005c /* reset register */
#define  NXE_1_GLB_PEGTUNE_DONE		0x00000001

#define NXE_1_GLB_CHIPCLKCTL	0x013000a8
#define NXE_1_GLB_CHIPCLKCTL_ON		0x00003fff

/* ROM Registers */
#define NXE_1_ROM_CONTROL	0x01310000
#define NXE_1_ROM_OPCODE	0x01310004
#define  NXE_1_ROM_OPCODE_READ		0x0000000b
#define NXE_1_ROM_ADDR		0x01310008
#define NXE_1_ROM_WDATA		0x0131000c
#define NXE_1_ROM_ABYTE_CNT	0x01310010
#define NXE_1_ROM_DBYTE_CNT	0x01310014 /* dummy byte count */
#define NXE_1_ROM_RDATA		0x01310018
#define NXE_1_ROM_AGT_TAG	0x0131001c
#define NXE_1_ROM_TIME_PARM	0x01310020
#define NXE_1_ROM_CLK_DIV	0x01310024
#define NXE_1_ROM_MISS_INSTR	0x01310028

/*
 * flash memory layout
 *
 * These are offsets of memory accessable via the ROM Registers above
 */
#define NXE_FLASH_CRBINIT	0x00000000 /* crb init section */
#define NXE_FLASH_BRDCFG	0x00004000 /* board config */
#define NXE_FLASH_INITCODE	0x00006000 /* pegtune code */
#define NXE_FLASH_BOOTLD	0x00010000 /* boot loader */
#define NXE_FLASH_IMAGE		0x00043000 /* compressed image */
#define NXE_FLASH_SECONDARY	0x00200000 /* backup image */
#define NXE_FLASH_PXE		0x003d0000 /* pxe image */
#define NXE_FLASH_USER		0x003e8000 /* user region for new boards */
#define NXE_FLASH_VPD		0x003e8c00 /* vendor private data */
#define NXE_FLASH_LICENSE	0x003e9000 /* firmware license */
#define NXE_FLASH_FIXED		0x003f0000 /* backup of crbinit */


/*
 * misc hardware details
 */
#define NXE_MAX_PORTS		4
#define NXE_MAX_PORT_LLADDRS	32
#define NXE_MAX_PKTLEN		(64 * 1024)


/*
 * hardware structures
 */

struct nxe_info {
	u_int32_t		ni_hdrver;
#define NXE_INFO_HDRVER_1		0x00000001

	u_int32_t		ni_board_mfg;
	u_int32_t		ni_board_type;
#define NXE_BRDTYPE_P1_BD		0x0000
#define NXE_BRDTYPE_P1_SB		0x0001
#define NXE_BRDTYPE_P1_SMAX		0x0002
#define NXE_BRDTYPE_P1_SOCK		0x0003
#define NXE_BRDTYPE_P2_SOCK_31		0x0008
#define NXE_BRDTYPE_P2_SOCK_35		0x0009
#define NXE_BRDTYPE_P2_SB35_4G		0x000a
#define NXE_BRDTYPE_P2_SB31_10G		0x000b
#define NXE_BRDTYPE_P2_SB31_2G		0x000c
#define NXE_BRDTYPE_P2_SB31_10G_IMEZ	0x000d
#define NXE_BRDTYPE_P2_SB31_10G_HMEZ	0x000e
#define NXE_BRDTYPE_P2_SB31_10G_CX4	0x000f
	u_int32_t		ni_board_num;

	u_int32_t		ni_chip_id;
	u_int32_t		ni_chip_minor;
	u_int32_t		ni_chip_major;
	u_int32_t		ni_chip_pkg;
	u_int32_t		ni_chip_lot;

	u_int32_t		ni_port_mask;
	u_int32_t		ni_peg_mask;
	u_int32_t		ni_icache;
	u_int32_t		ni_dcache;
	u_int32_t		ni_casper;

	u_int32_t		ni_lladdr0_low;
	u_int32_t		ni_lladdr1_low;
	u_int32_t		ni_lladdr2_low;
	u_int32_t		ni_lladdr3_low;

	u_int32_t		ni_mnsync_mode;
	u_int32_t		ni_mnsync_shift_cclk;
	u_int32_t		ni_mnsync_shift_mclk;
	u_int32_t		ni_mnwb_enable;
	u_int32_t		ni_mnfreq_crystal;
	u_int32_t		ni_mnfreq_speed;
	u_int32_t		ni_mnorg;
	u_int32_t		ni_mndepth;
	u_int32_t		ni_mnranks0;
	u_int32_t		ni_mnranks1;
	u_int32_t		ni_mnrd_latency0;
	u_int32_t		ni_mnrd_latency1;
	u_int32_t		ni_mnrd_latency2;
	u_int32_t		ni_mnrd_latency3;
	u_int32_t		ni_mnrd_latency4;
	u_int32_t		ni_mnrd_latency5;
	u_int32_t		ni_mnrd_latency6;
	u_int32_t		ni_mnrd_latency7;
	u_int32_t		ni_mnrd_latency8;
	u_int32_t		ni_mndll[18];
	u_int32_t		ni_mnddr_mode;
	u_int32_t		ni_mnddr_extmode;
	u_int32_t		ni_mntiming0;
	u_int32_t		ni_mntiming1;
	u_int32_t		ni_mntiming2;

	u_int32_t		ni_snsync_mode;
	u_int32_t		ni_snpt_mode;
	u_int32_t		ni_snecc_enable;
	u_int32_t		ni_snwb_enable;
	u_int32_t		ni_snfreq_crystal;
	u_int32_t		ni_snfreq_speed;
	u_int32_t		ni_snorg;
	u_int32_t		ni_sndepth;
	u_int32_t		ni_sndll;
	u_int32_t		ni_snrd_latency;

	u_int32_t		ni_lladdr0_high;
	u_int32_t		ni_lladdr1_high;
	u_int32_t		ni_lladdr2_high;
	u_int32_t		ni_lladdr3_high;

	u_int32_t		ni_magic;
#define NXE_INFO_MAGIC			0x12345678

	u_int32_t		ni_mnrd_imm;
	u_int32_t		ni_mndll_override;
} __packed;

struct nxe_imageinfo {
	u_int32_t		nim_bootld_ver;
	u_int32_t		nim_bootld_size;

	u_int8_t		nim_img_ver_major;
	u_int8_t		nim_img_ver_minor;
	u_int16_t		nim_img_ver_build;

	u_int32_t		min_img_size;
} __packed;

struct nxe_lladdr {
	u_int8_t		pad[2];
	u_int8_t		lladdr[6];
} __packed;

struct nxe_userinfo {
	u_int8_t		nu_flash_md5[1024];

	struct nxe_imageinfo	nu_imageinfo;

	u_int32_t		nu_primary;
	u_int32_t		nu_secondary;

	u_int64_t		nu_lladdr[NXE_MAX_PORTS][NXE_MAX_PORT_LLADDRS];

	u_int32_t		nu_subsys_id;

	u_int8_t		nu_serial[32];

	u_int32_t		nu_bios_ver;
} __packed;

/*
 * driver definitions
 */

struct nxe_board {
	u_int32_t		brd_type;
	u_int			brd_mode;
};

struct nxe_dmamem {
	bus_dmamap_t		ndm_map;
	bus_dma_segment_t	ndm_seg;
	size_t			ndm_size;
	caddr_t			ndm_kva;
};
#define NXE_DMA_MAP(_ndm)	((_ndm)->ndm_map)
#define NXE_DMA_LEN(_ndm)	((_ndm)->ndm_size)
#define NXE_DMA_DVA(_ndm)	((_ndm)->ndm_map->dm_segs[0].ds_addr)
#define NXE_DMA_KVA(_ndm)	((void *)(_ndm)->ndm_kva)

/*
 * autoconf glue
 */

struct nxe_softc {
	struct device		sc_dev;

	bus_dma_tag_t		sc_dmat;

	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;
	bus_size_t		sc_mems;
	bus_space_handle_t	sc_crbh;
	bus_space_tag_t		sc_dbt;
	bus_space_handle_t	sc_dbh;
	bus_size_t		sc_dbs;

	void			*sc_ih;

	int			sc_function;
	int			sc_port;
	int			sc_window;

	const struct nxe_board	*sc_board;
	u_int			sc_fw_major;
	u_int			sc_fw_minor;
	u_int			sc_fw_build;

	struct arpcom		sc_ac;
	struct ifmedia		sc_media;

	/* allocations for the hw */
	struct nxe_dmamem	*sc_dummy_dma;

};

int			nxe_match(struct device *, void *, void *);
void			nxe_attach(struct device *, struct device *, void *);
int			nxe_intr(void *);

struct cfattach nxe_ca = {
	sizeof(struct nxe_softc),
	nxe_match,
	nxe_attach
};

struct cfdriver nxe_cd = {
	NULL,
	"nxe",
	DV_IFNET
};

/* init code */
int			nxe_pci_map(struct nxe_softc *,
			    struct pci_attach_args *);
void			nxe_pci_unmap(struct nxe_softc *);

int			nxe_board_info(struct nxe_softc *);
int			nxe_user_info(struct nxe_softc *);
int			nxe_init(struct nxe_softc *);
void			nxe_mountroot(void *);

/* wrapper around dmaable memory allocations */
struct nxe_dmamem	*nxe_dmamem_alloc(struct nxe_softc *, bus_size_t,
			    bus_size_t);
void			nxe_dmamem_free(struct nxe_softc *,
			    struct nxe_dmamem *);

/* low level hardware access goo */
u_int32_t		nxe_read(struct nxe_softc *, bus_size_t);
void			nxe_write(struct nxe_softc *, bus_size_t, u_int32_t);
int			nxe_wait(struct nxe_softc *, bus_size_t, u_int32_t,
			    u_int32_t, u_int);

int			nxe_crb_set(struct nxe_softc *, int);
u_int32_t		nxe_crb_read(struct nxe_softc *, bus_size_t);
void			nxe_crb_write(struct nxe_softc *, bus_size_t,
			    u_int32_t);
int			nxe_crb_wait(struct nxe_softc *, bus_size_t,
			    u_int32_t, u_int32_t, u_int);

int			nxe_rom_lock(struct nxe_softc *);
void			nxe_rom_unlock(struct nxe_softc *);
int			nxe_rom_read(struct nxe_softc *, u_int32_t,
			    u_int32_t *);
int			nxe_rom_read_region(struct nxe_softc *, u_int32_t,
			    void *, size_t);


/* misc bits */
#define DEVNAME(_sc)	((_sc)->sc_dev.dv_xname)
#define sizeofa(_a)	(sizeof(_a) / sizeof((_a)[0]))

/* let's go! */

const struct pci_matchid nxe_devices[] = {
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GXxR },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_10GCX4 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_4GCU },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_IMEZ_2 },
	{ PCI_VENDOR_NETXEN, PCI_PRODUCT_NETXEN_NXB_HMEZ_2 }
};

const struct nxe_board nxe_boards[] = {
	{ NXE_BRDTYPE_P2_SB35_4G,	NXE_0_NIU_MODE_GBE },
	{ NXE_BRDTYPE_P2_SB31_10G,	NXE_0_NIU_MODE_XGE },
	{ NXE_BRDTYPE_P2_SB31_2G,	NXE_0_NIU_MODE_GBE },
	{ NXE_BRDTYPE_P2_SB31_10G_IMEZ,	NXE_0_NIU_MODE_XGE },
	{ NXE_BRDTYPE_P2_SB31_10G_HMEZ,	NXE_0_NIU_MODE_XGE },
	{ NXE_BRDTYPE_P2_SB31_10G_CX4,	NXE_0_NIU_MODE_XGE }
};

int
nxe_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args		*pa = aux;

	if (PCI_CLASS(pa->pa_class) != PCI_CLASS_NETWORK)
		return (0);

	return (pci_matchbyid(pa, nxe_devices, sizeofa(nxe_devices)));
}

void
nxe_attach(struct device *parent, struct device *self, void *aux)
{
	struct nxe_softc		*sc = (struct nxe_softc *)self;
	struct pci_attach_args		*pa = aux;

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_function = pa->pa_function;
	sc->sc_window = -1;

	if (nxe_pci_map(sc, pa) != 0) {
		/* error already printed by nxe_pci_map() */
		return;
	}

	nxe_crb_set(sc, 1);

	if (nxe_board_info(sc) != 0) {
		/* error already printed by nxe_board_info() */
		goto unmap;
	}

	if (nxe_user_info(sc) != 0) {
		/* error already printed by nxe_board_info() */
		goto unmap;
	}

	if (nxe_init(sc) != 0) {
		/* error already printed by nxe_init() */
		goto unmap;
	}

	printf(": firmware %d.%d.%d address %s\n",
	    sc->sc_fw_major, sc->sc_fw_minor, sc->sc_fw_build,
	    ether_sprintf(sc->sc_ac.ac_enaddr));
	return;
unmap:
	nxe_pci_unmap(sc);
}

int
nxe_pci_map(struct nxe_softc *sc, struct pci_attach_args *pa)
{
	pcireg_t			memtype;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, NXE_PCI_BAR_MEM);
	if (pci_mapreg_map(pa, NXE_PCI_BAR_MEM, memtype, 0, &sc->sc_memt,
	    &sc->sc_memh, NULL, &sc->sc_mems, 0) != 0) {
		printf(": unable to map host registers\n");
		return (1);
	}
	if (sc->sc_mems != NXE_PCI_BAR_MEM_128MB) {
		printf(": unexpected register map size\n");
		goto unmap_mem;
	}

	/* set up the CRB window */
	if (bus_space_subregion(sc->sc_memt, sc->sc_memh, NXE_MAP_CRB,
	    sc->sc_mems - NXE_MAP_CRB, &sc->sc_crbh) != 0) {
		printf(": unable to create CRB window\n");
		goto unmap_mem;
	}

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, NXE_PCI_BAR_DOORBELL);
	if (pci_mapreg_map(pa, NXE_PCI_BAR_DOORBELL, memtype, 0, &sc->sc_dbt,
	    &sc->sc_dbh, NULL, &sc->sc_dbs, 0) != 0) {
		printf(": unable to map doorbell registers\n");
		/* bus_space(9) says i dont have to unmap subregions */
		goto unmap_mem;
	}

	mountroothook_establish(nxe_mountroot, sc);
	return (0);

unmap_mem:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
	return (1);
}

void
nxe_pci_unmap(struct nxe_softc *sc)
{
	bus_space_unmap(sc->sc_dbt, sc->sc_dbh, sc->sc_dbs);
	sc->sc_dbs = 0;
	/* bus_space(9) says i dont have to unmap the crb subregion */
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

int
nxe_intr(void *xsc)
{
	return (0);
}

int
nxe_board_info(struct nxe_softc *sc)
{
	struct nxe_info			*ni;
	int				rv = 1;
	int				i;

	ni = malloc(sizeof(struct nxe_info), M_NOWAIT, M_TEMP);
	if (ni == NULL) {
		printf(": unable to allocate temporary memory\n");
		return (1);
	}

	if (nxe_rom_read_region(sc, NXE_FLASH_BRDCFG, ni,
	    sizeof(struct nxe_info)) != 0) {
		printf(": unable to read board info\n");
		goto out;
	}

	if (ni->ni_hdrver != NXE_INFO_HDRVER_1) {
		printf(": unexpected board info header version 0x%08x\n",
		    ni->ni_hdrver);
		goto out;
	}
	if (ni->ni_magic != NXE_INFO_MAGIC) {
		printf(": board info magic is invalid\n");
		goto out;
	}

	for (i = 0; i < sizeofa(nxe_boards); i++) {
		if (ni->ni_board_type == nxe_boards[i].brd_type) {
			sc->sc_board = &nxe_boards[i];
			break;
		}
	}
	if (sc->sc_board == NULL) {
		printf(": unknown board type %04x\n", ni->ni_board_type);
		goto out;
	}

	rv = 0;
out:
	free(ni, M_TEMP);
	return (rv);
}

int
nxe_user_info(struct nxe_softc *sc)
{
	struct nxe_userinfo		*nu;
	u_int64_t			lladdr;
	struct nxe_lladdr		*la;
	int				rv = 1;

	nu = malloc(sizeof(struct nxe_userinfo), M_NOWAIT, M_TEMP);
	if (nu == NULL) {
		printf(": unable to allocate temp memory\n");
		return (1);
	}
	if (nxe_rom_read_region(sc, NXE_FLASH_USER, nu,
	    sizeof(struct nxe_userinfo)) != 0) {
		printf(": unable to read user info\n");
		goto out;
	}

	sc->sc_fw_major = nu->nu_imageinfo.nim_img_ver_major;
	sc->sc_fw_minor = nu->nu_imageinfo.nim_img_ver_minor;
	sc->sc_fw_build = letoh16(nu->nu_imageinfo.nim_img_ver_build);

	if (sc->sc_fw_major > NXE_VERSION_MAJOR ||
	    sc->sc_fw_major < NXE_VERSION_MAJOR ||
	    sc->sc_fw_minor > NXE_VERSION_MINOR ||
	    sc->sc_fw_minor < NXE_VERSION_MINOR) {
		printf(": firmware %d.%d.%d is unsupported by this driver\n",
		    sc->sc_fw_major, sc->sc_fw_minor, sc->sc_fw_build);
		goto out;
	}

	lladdr = swap64(nu->nu_lladdr[sc->sc_function][0]);
	la = (struct nxe_lladdr *)&lladdr;
	bcopy(la->lladdr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	rv = 0;
out:
	free(nu, M_TEMP);
	return (rv);
}

int
nxe_init(struct nxe_softc *sc)
{
	u_int64_t			dva;
	u_int32_t			r;

	/* stop the chip from processing */
	nxe_crb_write(sc, NXE_1_SW_CMD_PRODUCER(sc->sc_function), 0);
        nxe_crb_write(sc, NXE_1_SW_CMD_CONSUMER(sc->sc_function), 0);
        nxe_crb_write(sc, NXE_1_SW_CMD_ADDR_HI, 0);
        nxe_crb_write(sc, NXE_1_SW_CMD_ADDR_LO, 0);

	/*
	 * if this is the first port on the device it needs some special
	 * treatment to get things going.
	 */
	if (sc->sc_function == 0) {
		/* init adapter offload */
		sc->sc_dummy_dma = nxe_dmamem_alloc(sc,
		    NXE_1_SW_DUMMY_ADDR_LEN, PAGE_SIZE);
		if (sc->sc_dummy_dma == NULL) {
			printf(": unable to allocate dummy memory\n");
			return (1);
		}

		bus_dmamap_sync(sc->sc_dmat, NXE_DMA_MAP(sc->sc_dummy_dma),
		    0, NXE_DMA_LEN(sc->sc_dummy_dma), BUS_DMASYNC_PREREAD);

		dva = NXE_DMA_DVA(sc->sc_dummy_dma);
		nxe_crb_write(sc, NXE_1_SW_DUMMY_ADDR_HI, dva >> 32);
		nxe_crb_write(sc, NXE_1_SW_DUMMY_ADDR_LO, dva);

		r = nxe_crb_read(sc, NXE_1_SW_BOOTLD_CONFIG);
		if (r == 0x55555555) {
			r = nxe_crb_read(sc, NXE_1_ROMUSB_SW_RESET);
			if (r != NXE_1_ROMUSB_SW_RESET_BOOT) {
				printf(": unexpected boot state\n");
				goto err;
			}

			/* clear */
			nxe_crb_write(sc, NXE_1_SW_BOOTLD_CONFIG, 0);
		}

		/* start the device up */
		nxe_crb_write(sc, NXE_1_SW_DRIVER_VER, NXE_VERSION);
		nxe_crb_write(sc, NXE_1_GLB_PEGTUNE, NXE_1_GLB_PEGTUNE_DONE);

		/*
		 * the firmware takes a long time to boot, so we'll check
		 * it later on, and again when we want to bring a port up.
		 */
	}

	return (0);

err:
	bus_dmamap_sync(sc->sc_dmat, NXE_DMA_MAP(sc->sc_dummy_dma),
	    0, NXE_DMA_LEN(sc->sc_dummy_dma), BUS_DMASYNC_POSTREAD);
	nxe_dmamem_free(sc, sc->sc_dummy_dma);
	return (1);
}

void
nxe_mountroot(void *arg)
{
	struct nxe_softc		*sc = arg;

	DASSERT(sc->sc_window == 1);

	if (!nxe_crb_wait(sc, NXE_1_SW_CMDPEG_STATE, 0xffffffff,
	    NXE_1_SW_CMDPEG_STATE_DONE, 10000)) {
		printf("%s: firmware bootstrap failed, code 0x%08x\n",
		    DEVNAME(sc), nxe_crb_read(sc, NXE_1_SW_CMDPEG_STATE));
                return;
        }

	sc->sc_port = nxe_crb_read(sc, NXE_1_SW_V2P(sc->sc_function));
	if (sc->sc_port == 0x55555555)
		sc->sc_port = sc->sc_function;

	nxe_crb_write(sc, NXE_1_SW_NIC_CAP_HOST, NXE_1_SW_NIC_CAP_HOST_DEF);
	nxe_crb_write(sc, NXE_1_SW_MPORT_MODE, NXE_1_SW_MPORT_MODE_MULTI);
	nxe_crb_write(sc, NXE_1_SW_CMDPEG_STATE, NXE_1_SW_CMDPEG_STATE_ACK);
}

struct nxe_dmamem *
nxe_dmamem_alloc(struct nxe_softc *sc, bus_size_t size, bus_size_t align)
{
	struct nxe_dmamem		*ndm;
	int				nsegs;

	ndm = malloc(sizeof(struct nxe_dmamem), M_DEVBUF, M_WAITOK);
	bzero(ndm, sizeof(struct nxe_dmamem));
	ndm->ndm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &ndm->ndm_map) != 0)
		goto ndmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, align, 0, &ndm->ndm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &ndm->ndm_seg, nsegs, size,
	    &ndm->ndm_kva, BUS_DMA_WAITOK) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, ndm->ndm_map, ndm->ndm_kva, size,
	    NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	bzero(ndm->ndm_kva, size);

	return (ndm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, ndm->ndm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &ndm->ndm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, ndm->ndm_map);
ndmfree:
	free(ndm, M_DEVBUF);

	return (NULL);
}

void
nxe_dmamem_free(struct nxe_softc *sc, struct nxe_dmamem *ndm)
{
	bus_dmamem_unmap(sc->sc_dmat, ndm->ndm_kva, ndm->ndm_size);
	bus_dmamem_free(sc->sc_dmat, &ndm->ndm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, ndm->ndm_map);
	free(ndm, M_DEVBUF);
}

u_int32_t
nxe_read(struct nxe_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_memh, r));
}

void
nxe_write(struct nxe_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_memt, sc->sc_memh, r, v);
	bus_space_barrier(sc->sc_memt, sc->sc_memh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
nxe_wait(struct nxe_softc *sc, bus_size_t r, u_int32_t m, u_int32_t v,
    u_int timeout)
{
	while ((nxe_read(sc, r) & m) != v) {
		if (timeout == 0)
			return (0);

		delay(1000);
		timeout--;
	}

	return (1);
}

int
nxe_crb_set(struct nxe_softc *sc, int window)
{
	int			oldwindow = sc->sc_window;

	if (sc->sc_window != window) {
		sc->sc_window = window;

		nxe_write(sc, NXE_WIN_CRB(sc->sc_function),
		    window ? NXE_WIN_CRB_1 : NXE_WIN_CRB_0);
	}

	return (oldwindow);
}

u_int32_t
nxe_crb_read(struct nxe_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_memt, sc->sc_crbh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_memt, sc->sc_crbh, r));
}

void
nxe_crb_write(struct nxe_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_memt, sc->sc_crbh, r, v);
	bus_space_barrier(sc->sc_memt, sc->sc_crbh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
nxe_crb_wait(struct nxe_softc *sc, bus_size_t r, u_int32_t m, u_int32_t v,
    u_int timeout)
{
	while ((nxe_crb_read(sc, r) & m) != v) {
		if (timeout == 0)
			return (0);

		delay(1000);
		timeout--;
	}

	return (1);
}

int
nxe_rom_lock(struct nxe_softc *sc)
{
	if (!nxe_wait(sc, NXE_SEM_ROM_LOCK, 0xffffffff,
	    NXE_SEM_DONE, 10000))
		return (1);
	nxe_crb_write(sc, NXE_1_SW_ROM_LOCK_ID, NXE_1_SW_ROM_LOCK_ID);

	return (0);
}

void
nxe_rom_unlock(struct nxe_softc *sc)
{
	nxe_read(sc, NXE_SEM_ROM_UNLOCK);
}

int
nxe_rom_read(struct nxe_softc *sc, u_int32_t r, u_int32_t *v)
{
	int			rv = 1;

	DASSERT(sc->sc_window == 1);

	if (nxe_rom_lock(sc) != 0)
		return (1);

	/* set the rom address */
	nxe_crb_write(sc, NXE_1_ROM_ADDR, r);

	/* set the xfer len */
	nxe_crb_write(sc, NXE_1_ROM_ABYTE_CNT, 3);
	delay(100); /* used to prevent bursting on the chipset */
	nxe_crb_write(sc, NXE_1_ROM_DBYTE_CNT, 0);

	/* set opcode and wait for completion */
	nxe_crb_write(sc, NXE_1_ROM_OPCODE, NXE_1_ROM_OPCODE_READ);
	if (!nxe_crb_wait(sc, NXE_1_ROMUSB_STATUS, NXE_1_ROMUSB_STATUS_DONE,
	    NXE_1_ROMUSB_STATUS_DONE, 100))
		goto err;

	/* reset counters */
	nxe_crb_write(sc, NXE_1_ROM_ABYTE_CNT, 0);
	delay(100);
	nxe_crb_write(sc, NXE_1_ROM_DBYTE_CNT, 0);

	*v = nxe_crb_read(sc, NXE_1_ROM_RDATA);

	rv = 0;
err:
	nxe_rom_unlock(sc);
	return (rv);
}

int
nxe_rom_read_region(struct nxe_softc *sc, u_int32_t r, void *buf,
    size_t buflen)
{
	u_int32_t		*databuf = buf;
	int			i;

#ifdef NXE_DEBUG
	if ((buflen % 4) != 0)
		panic("nxe_read_rom_region: buflen is wrong (%d)", buflen);
#endif

	buflen = buflen / 4;
	for (i = 0; i < buflen; i++) {
		if (nxe_rom_read(sc, r, &databuf[i]) != 0)
			return (1);

		r += sizeof(u_int32_t);
	}

	return (0);
}
