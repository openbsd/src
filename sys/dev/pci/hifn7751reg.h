/*	$OpenBSD: hifn7751reg.h,v 1.7 2000/03/16 20:33:48 deraadt Exp $	*/

/*
 * Invertex AEON / Hi/fn 7751 driver
 * Copyright (c) 1999 Invertex Inc. All rights reserved.
 *
 * Please send any comments, feedback, bug-fixes, or feature requests to
 * software@invertex.com.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
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
#ifndef __HIFN_H__
#define __HIFN_H__

#include <machine/endian.h>

/*
 * Some PCI configuration space offset defines.  The names were made
 * identical to the names used by the Linux kernel.
 */
#define  HIFN_BAR0		(PCI_MAPREG_START + 0)	/* PUC register map */
#define  HIFN_BAR1		(PCI_MAPREG_START + 4)	/* DMA register map */

/*
 *  Some configurable values for the driver
 */
#define HIFN_D_RSIZE	24
#define HIFN_MAX_DEVICES	4

#define HIFN_D_CMD_RSIZE	24
#define HIFN_D_SRC_RSIZE	80
#define HIFN_D_DST_RSIZE	80
#define HIFN_D_RES_RSIZE	24

/*
 * The values below should multiple of 4 -- and be large enough to handle
 * any command the driver implements.
 */
#define HIFN_MAX_COMMAND	120
#define HIFN_MAX_RESULT		16

/*
 * hifn_desc_t
 *
 * Holds an individual descriptor for any of the rings.
 */
typedef struct hifn_desc {
	volatile u_int32_t l;		/* length and status bits */
	volatile u_int32_t p;
} hifn_desc_t;

/*
 * Masks for the "length" field of struct hifn_desc.
 */
#define HIFN_D_MASKDONEIRQ	(0x1 << 25)
#define HIFN_D_LAST		(0x1 << 29)
#define HIFN_D_JUMP		(0x1 << 30)
#define HIFN_D_VALID		(0x1 << 31)

/*
 * hifn_callback_t 
 *
 * Type for callback function when dest data is ready.
 */
typedef void (*hifn_callback_t)(hifn_command_t *);

/*
 * Data structure to hold all 4 rings and any other ring related data.
 */
struct hifn_dma {
	/*
	 *  Descriptor rings.  We add +1 to the size to accomidate the
	 *  jump descriptor.
	 */
	struct hifn_desc	cmdr[HIFN_D_RSIZE+1];
	struct hifn_desc	srcr[HIFN_D_RSIZE+1];
	struct hifn_desc	dstr[HIFN_D_RSIZE+1];
	struct hifn_desc	resr[HIFN_D_RSIZE+1];

	struct hifn_command	*hifn_commands[HIFN_D_RSIZE];

	u_char	command_bufs[HIFN_D_RSIZE][HIFN_MAX_COMMAND];
	u_char	result_bufs[HIFN_D_RSIZE][HIFN_MAX_RESULT];

	/*
	 *  Our current positions for insertion and removal from the desriptor
	 *  rings. 
	 */
	int		cmdi, srci, dsti, resi;
	volatile int	cmdu, srcu, dstu, resu;

	u_int32_t wakeup_rpos;
	volatile u_int32_t slots_in_use;
};

/*
 * Holds data specific to a single HIFN board.
 */
struct hifn_softc {
	struct device	sc_dv;		/* generic device */
	void *		sc_ih;		/* interrupt handler cookie */
	u_int32_t	sc_drammodel;	/* 1=dram, 0=sram */

	bus_space_handle_t	sc_sh0, sc_sh1;
	bus_space_tag_t		sc_st0, sc_st1;
	bus_dma_tag_t		sc_dmat;

	struct hifn_dma *sc_dma;
};

/*
 * Processing Unit Registers (offset from BASEREG0)
 */
#define	HIFN_0_PUDATA		0x00	/* Processing Unit Data */
#define	HIFN_0_PUCTRL		0x04	/* Processing Unit Control */
#define	HIFN_0_PUISR		0x08	/* Processing Unit Interrupt Status */
#define	HIFN_0_PUCNFG		0x0c	/* Processing Unit Configuration */
#define	HIFN_0_PUIER		0x10	/* Processing Unit Interrupt Enable */
#define	HIFN_0_PUSTAT		0x14	/* Processing Unit Status/Chip ID */
#define	HIFN_0_FIFOSTAT		0x18	/* FIFO Status */
#define	HIFN_0_FIFOCNFG		0x1c	/* FIFO Configuration */
#define	HIFN_0_SPACESIZE	0x20	/* Register space size */

/* Processing Unit Control Register (HIFN_0_PUCTRL) */
#define	HIFN_PUCTRL_CLRSRCFIFO	0x0010	/* clear source fifo */
#define	HIFN_PUCTRL_STOP	0x0008	/* stop pu */
#define	HIFN_PUCTRL_LOCKRAM	0x0004	/* lock ram */
#define	HIFN_PUCTRL_DMAENA	0x0002	/* enable dma */
#define	HIFN_PUCTRL_RESET	0x0001	/* Reset processing unit */

/* Processing Unit Interrupt Status Register (HIFN_0_PUISR) */
#define	HIFN_PUISR_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUISR_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUISR_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUISR_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUISR_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUISR_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUISR_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUISR_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUISR_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUISR_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Configuration Register (HIFN_0_PUCNFG) */
#define	HIFN_PUCNFG_DRAMMASK	0xe000	/* DRAM size mask */
#define	HIFN_PUCNFG_DSZ_256K	0x0000	/* 256k dram */
#define	HIFN_PUCNFG_DSZ_512K	0x2000	/* 512k dram */
#define	HIFN_PUCNFG_DSZ_1M	0x4000	/* 1m dram */
#define	HIFN_PUCNFG_DSZ_2M	0x6000	/* 2m dram */
#define	HIFN_PUCNFG_DSZ_4M	0x8000	/* 4m dram */
#define	HIFN_PUCNFG_DSZ_8M	0xa000	/* 8m dram */
#define	HIFN_PUNCFG_DSZ_16M	0xc000	/* 16m dram */
#define	HIFN_PUCNFG_DSZ_32M	0xe000	/* 32m dram */
#define	HIFN_PUCNFG_DRAMREFRESH	0x1800	/* DRAM refresh rate mask */
#define	HIFN_PUCNFG_DRFR_512	0x0000	/* 512 divisor of ECLK */
#define	HIFN_PUCNFG_DRFR_256	0x0800	/* 256 divisor of ECLK */
#define	HIFN_PUCNFG_DRFR_128	0x1000	/* 128 divisor of ECLK */
#define	HIFN_PUCNFG_TCALLPHASES	0x0200	/* your guess is as good as mine... */
#define	HIFN_PUCNFG_TCDRVTOTEM	0x0100	/* your guess is as good as mine... */
#define	HIFN_PUCNFG_BIGENDIAN	0x0080	/* DMA big endian mode */
#define	HIFN_PUCNFG_BUS32	0x0040	/* Bus width 32bits */
#define	HIFN_PUCNFG_BUS16	0x0000	/* Bus width 16 bits */
#define	HIFN_PUCNFG_CHIPID	0x0020	/* Allow chipid from PUSTAT */
#define	HIFN_PUCNFG_DRAM	0x0010	/* Context RAM is DRAM */
#define	HIFN_PUCNFG_SRAM	0x0000	/* Context RAM is SRAM */
#define	HIFN_PUCNFG_COMPSING	0x0004	/* Enable single compression context */
#define	HIFN_PUCNFG_ENCCNFG	0x0002	/* Encryption configuration */

/* Processing Unit Interrupt Enable Register (HIFN_0_PUIER) */
#define	HIFN_PUIER_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUIER_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUIER_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUIER_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUIER_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUIER_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUIER_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUIER_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUIER_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUIER_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Status Register/Chip ID (HIFN_0_PUSTAT) */
#define	HIFN_PUSTAT_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	HIFN_PUSTAT_DATAERR	0x4000	/* Data error interrupt */
#define	HIFN_PUSTAT_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	HIFN_PUSTAT_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	HIFN_PUSTAT_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	HIFN_PUSTAT_SRCCMD	0x0080	/* Source command interrupt */
#define	HIFN_PUSTAT_SRCCTX	0x0040	/* Source context interrupt */
#define	HIFN_PUSTAT_SRCDATA	0x0020	/* Source data interrupt */
#define	HIFN_PUSTAT_DSTDATA	0x0010	/* Destination data interrupt */
#define	HIFN_PUSTAT_DSTRESULT	0x0004	/* Destination result interrupt */
#define	HIFN_PUSTAT_CHIPREV	0x00ff	/* Chip revision mask */
#define	HIFN_PUSTAT_CHIPENA	0xff00	/* Chip enabled mask */
#define	HIFN_PUSTAT_ENA_2	0x1100	/* Level 2 enabled */
#define	HIFN_PUSTAT_ENA_1	0x1000	/* Level 1 enabled */
#define	HIFN_PUSTAT_ENA_0	0x3000	/* Level 0 enabled */
#define	HIFN_PUSTAT_REV_2	0x0020	/* 7751 PT6/2 */
#define	HIFN_PUSTAT_REV_3	0x0030	/* 7751 PT6/3 */

/* FIFO Status Register (HIFN_0_FIFOSTAT) */
#define	HIFN_FIFOSTAT_SRC	0x7f00	/* Source FIFO available */
#define	HIFN_FIFOSTAT_DST	0x007f	/* Destination FIFO available */

/* FIFO Configuration Register (HIFN_0_FIFOCNFG) */
#define	HIFN_FIFOCNFG_THRESHOLD	0x0400	/* must be written as 1 */

/*
 * DMA Interface Registers (offset from BASEREG1)
 */
#define	HIFN_1_DMA_CRAR		0x0c	/* DMA Command Ring Address */
#define	HIFN_1_DMA_SRAR		0x1c	/* DMA Source Ring Address */
#define	HIFN_1_DMA_RRAR		0x2c	/* DMA Resultt Ring Address */
#define	HIFN_1_DMA_DRAR		0x3c	/* DMA Destination Ring Address */
#define	HIFN_1_DMA_CSR		0x40	/* DMA Status and Control */
#define	HIFN_1_DMA_IER		0x44	/* DMA Interrupt Enable */
#define	HIFN_1_DMA_CNFG		0x48	/* DMA Configuration */
#define	HIFN_1_REVID		0x98	/* Revision ID */

/* DMA Status and Control Register (HIFN_1_DMA_CSR) */
#define	HIFN_DMACSR_D_CTRLMASK	0xc0000000	/* Destinition Ring Control */
#define	HIFN_DMACSR_D_CTRL_NOP	0x00000000	/* Dest. Control: no-op */
#define	HIFN_DMACSR_D_CTRL_DIS	0x40000000	/* Dest. Control: disable */
#define	HIFN_DMACSR_D_CTRL_ENA	0x80000000	/* Dest. Control: enable */
#define	HIFN_DMACSR_D_ABORT	0x20000000	/* Destinition Ring PCIAbort */
#define	HIFN_DMACSR_D_DONE	0x10000000	/* Destinition Ring Done */
#define	HIFN_DMACSR_D_LAST	0x08000000	/* Destinition Ring Last */
#define	HIFN_DMACSR_D_WAIT	0x04000000	/* Destinition Ring Waiting */
#define	HIFN_DMACSR_D_OVER	0x02000000	/* Destinition Ring Overflow */
#define	HIFN_DMACSR_R_CTRL	0x00c00000	/* Result Ring Control */
#define	HIFN_DMACSR_R_CTRL_NOP	0x00000000	/* Result Control: no-op */
#define	HIFN_DMACSR_R_CTRL_DIS	0x00400000	/* Result Control: disable */
#define	HIFN_DMACSR_R_CTRL_ENA	0x00800000	/* Result Control: enable */
#define	HIFN_DMACSR_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	HIFN_DMACSR_R_DONE	0x00100000	/* Result Ring Done */
#define	HIFN_DMACSR_R_LAST	0x00080000	/* Result Ring Last */
#define	HIFN_DMACSR_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	HIFN_DMACSR_R_OVER	0x00020000	/* Result Ring Overflow */
#define	HIFN_DMACSR_S_CTRL	0x0000c000	/* Source Ring Control */
#define	HIFN_DMACSR_S_CTRL_NOP	0x00000000	/* Source Control: no-op */
#define	HIFN_DMACSR_S_CTRL_DIS	0x00004000	/* Source Control: disable */
#define	HIFN_DMACSR_S_CTRL_ENA	0x00008000	/* Source Control: enable */
#define	HIFN_DMACSR_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	HIFN_DMACSR_S_DONE	0x00001000	/* Source Ring Done */
#define	HIFN_DMACSR_S_LAST	0x00000800	/* Source Ring Last */
#define	HIFN_DMACSR_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	HIFN_DMACSR_S_OVER	0x00000200	/* Source Ring Overflow */
#define	HIFN_DMACSR_C_CTRL	0x000000c0	/* Command Ring Control */
#define	HIFN_DMACSR_C_CTRL_NOP	0x00000000	/* Command Control: no-op */
#define	HIFN_DMACSR_C_CTRL_DIS	0x00000040	/* Command Control: disable */
#define	HIFN_DMACSR_C_CTRL_ENA	0x00000080	/* Command Control: enable */
#define	HIFN_DMACSR_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	HIFN_DMACSR_C_DONE	0x00000010	/* Command Ring Done */
#define	HIFN_DMACSR_C_LAST	0x00000008	/* Command Ring Last */
#define	HIFN_DMACSR_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	HIFN_DMACSR_C_EIRQ	0x00000001	/* Command Ring Engine IRQ */

/* DMA Interrupt Enable Register (HIFN_1_DMA_IER) */
#define	HIFN_DMAIER_D_ABORT	0x20000000	/* Destination Ring PCIAbort */
#define	HIFN_DMAIER_D_DONE	0x10000000	/* Destination Ring Done */
#define	HIFN_DMAIER_D_LAST	0x08000000	/* Destination Ring Last */
#define	HIFN_DMAIER_D_WAIT	0x04000000	/* Destination Ring Waiting */
#define	HIFN_DMAIER_D_OVER	0x02000000	/* Destination Ring Overflow */
#define	HIFN_DMAIER_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	HIFN_DMAIER_R_DONE	0x00100000	/* Result Ring Done */
#define	HIFN_DMAIER_R_LAST	0x00080000	/* Result Ring Last */
#define	HIFN_DMAIER_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	HIFN_DMAIER_R_OVER	0x00020000	/* Result Ring Overflow */
#define	HIFN_DMAIER_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	HIFN_DMAIER_S_DONE	0x00001000	/* Source Ring Done */
#define	HIFN_DMAIER_S_LAST	0x00000800	/* Source Ring Last */
#define	HIFN_DMAIER_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	HIFN_DMAIER_S_OVER	0x00000200	/* Source Ring Overflow */
#define	HIFN_DMAIER_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	HIFN_DMAIER_C_DONE	0x00000010	/* Command Ring Done */
#define	HIFN_DMAIER_C_LAST	0x00000008	/* Command Ring Last */
#define	HIFN_DMAIER_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	HIFN_DMAIER_ENGINE	0x00000001	/* Engine IRQ */

/* DMA Configuration Register (HIFN_1_DMA_CNFG) */
#define	HIFN_DMACNFG_BIGENDIAN	0x10000000	/* big endian mode */
#define	HIFN_DMACNFG_POLLFREQ	0x00ff0000	/* Poll frequency mask */
#define	HIFN_DMACNFG_UNLOCK	0x00000800
#define	HIFN_DMACNFG_POLLINVAL	0x00000700	/* Invalid Poll Scalar */
#define	HIFN_DMACNFG_LAST	0x00000010	/* Host control LAST bit */
#define	HIFN_DMACNFG_MODE	0x00000004	/* DMA mode */
#define	HIFN_DMACNFG_DMARESET	0x00000002	/* DMA Reset # */
#define	HIFN_DMACNFG_MSTRESET	0x00000001	/* Master Reset # */

#define WRITE_REG_0(sc,reg,val) \
    bus_space_write_4((sc)->sc_st0, (sc)->sc_sh0, reg, val)
#define READ_REG_0(sc,reg) \
    bus_space_read_4((sc)->sc_st0, (sc)->sc_sh0, reg)

/*
 * Register offsets in register set 1
 */

#define	HIFN_UNLOCK_SECRET1	0xf4
#define	HIFN_UNLOCK_SECRET2	0xfc

#define WRITE_REG_1(sc,reg,val)	\
    bus_space_write_4((sc)->sc_st1, (sc)->sc_sh1, reg, val)
#define READ_REG_1(sc,reg) \
    bus_space_read_4((sc)->sc_st1, (sc)->sc_sh1, reg)

/*********************************************************************
 * Structs for board commands 
 *
 *********************************************************************/

/*
 * Structure to help build up the command data structure.
 */
typedef struct hifn_base_command {
	u_int16_t masks;
	u_int16_t session_num;
	u_int16_t total_source_count;
	u_int16_t total_dest_count;
} hifn_base_command_t;

#define HIFN_BASE_CMD_MAC    (0x1 << 10)
#define HIFN_BASE_CMD_CRYPT  (0x1 << 11)
#define HIFN_BASE_CMD_DECODE (0x1 << 13)

/*
 * Structure to help build up the command data structure.
 */
typedef struct hifn_crypt_command {
	u_int16_t masks;               
	u_int16_t header_skip;
	u_int32_t source_count;
} hifn_crypt_command_t;

#define HIFN_CRYPT_CMD_ALG_MASK  (0x3 << 0)
#define HIFN_CRYPT_CMD_ALG_DES   (0x0 << 0)
#define HIFN_CRYPT_CMD_ALG_3DES  (0x1 << 0)
#define HIFN_CRYPT_CMD_MODE_CBC  (0x1 << 3)
#define HIFN_CRYPT_CMD_NEW_KEY   (0x1 << 11)
#define HIFN_CRYPT_CMD_NEW_IV    (0x1 << 12)

/*
 * Structure to help build up the command data structure.
 */
typedef struct hifn_mac_command {
	u_int16_t masks;  
	u_int16_t header_skip;
	u_int32_t source_count;
} hifn_mac_command_t;

#define HIFN_MAC_CMD_ALG_MD5    (0x1 << 0)
#define HIFN_MAC_CMD_ALG_SHA1   (0x0 << 0)
#define HIFN_MAC_CMD_MODE_HMAC  (0x0 << 2)
#define HIFN_MAC_CMD_TRUNC      (0x1 << 4)
#define HIFN_MAC_CMD_APPEND     (0x1 << 6)
/*
 * MAC POS IPSec initiates authentication after encryption on encodes
 * and before decryption on decodes.
 */
#define HIFN_MAC_CMD_POS_IPSEC  (0x2 << 8)
#define HIFN_MAC_CMD_NEW_KEY    (0x1 << 11)

/*
 * Structure with all fields necessary to write the command buffer.
 * We build it up while interrupts are on, then use it to write out
 * the command buffer quickly while interrupts are off.
 */
typedef struct hifn_command_buf_data {
	hifn_base_command_t base_cmd;
	hifn_mac_command_t mac_cmd;
	hifn_crypt_command_t crypt_cmd;
	const u_int8_t *mac;
	const u_int8_t *ck;
	const u_int8_t *iv;
} hifn_command_buf_data_t;

/*
 * The poll frequency and poll scalar defines are unshifted values used
 * to set fields in the DMA Configuration Register.
 */
#ifndef HIFN_POLL_FREQUENCY
#define HIFN_POLL_FREQUENCY  0x1
#endif

#ifndef HIFN_POLL_SCALAR
#define HIFN_POLL_SCALAR    0x0
#endif

#endif /* __HIFN_H__ */
