/*	$OpenBSD: hifn7751reg.h,v 1.6 2000/03/15 14:55:52 jason Exp $	*/

/*
 * Invertex AEON driver
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
#ifndef __AEON_H__
#define __AEON_H__

#include <machine/endian.h>

/*
 * Some PCI configuration space offset defines.  The names were made
 * identical to the names used by the Linux kernel.
 */
#define  AEON_BAR0		(PCI_MAPREG_START + 0)	/* PUC register map */
#define  AEON_BAR1		(PCI_MAPREG_START + 4)	/* DMA register map */

/*
 *  Some configurable values for the driver
 */
#define AEON_D_RSIZE	24
#define AEON_MAX_DEVICES	4

#define AEON_D_CMD_RSIZE	24
#define AEON_D_SRC_RSIZE	80
#define AEON_D_DST_RSIZE	80
#define AEON_D_RES_RSIZE	24

/*
 * The values below should multiple of 4 -- and be large enough to handle
 * any command the driver implements.
 */
#define AEON_MAX_COMMAND	120
#define AEON_MAX_RESULT		16

/*
 * aeon_desc_t
 *
 * Holds an individual descriptor for any of the rings.
 */
typedef struct aeon_desc {
	volatile u_int32_t l;		/* length and status bits */
	volatile u_int32_t p;
} aeon_desc_t;

/*
 * Masks for the "length" field of struct aeon_desc.
 */
#define AEON_D_MASKDONEIRQ	(0x1 << 25)
#define AEON_D_LAST		(0x1 << 29)
#define AEON_D_JUMP		(0x1 << 30)
#define AEON_D_VALID		(0x1 << 31)

/*
 * aeon_callback_t 
 *
 * Type for callback function when dest data is ready.
 */
typedef void (*aeon_callback_t)(aeon_command_t *);

/*
 * Data structure to hold all 4 rings and any other ring related data.
 */
struct aeon_dma {
	/*
	 *  Descriptor rings.  We add +1 to the size to accomidate the
	 *  jump descriptor.
	 */
	struct aeon_desc	cmdr[AEON_D_RSIZE+1];
	struct aeon_desc	srcr[AEON_D_RSIZE+1];
	struct aeon_desc	dstr[AEON_D_RSIZE+1];
	struct aeon_desc	resr[AEON_D_RSIZE+1];

	struct aeon_command	*aeon_commands[AEON_D_RSIZE];

	u_char	command_bufs[AEON_D_RSIZE][AEON_MAX_COMMAND];
	u_char	result_bufs[AEON_D_RSIZE][AEON_MAX_RESULT];

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
 * Holds data specific to a single AEON board.
 */
struct aeon_softc {
	struct device	sc_dv;		/* generic device */
	void *		sc_ih;		/* interrupt handler cookie */
	u_int32_t	sc_drammodel;	/* 1=dram, 0=sram */

	bus_space_handle_t	sc_sh0, sc_sh1;
	bus_space_tag_t		sc_st0, sc_st1;
	bus_dma_tag_t		sc_dmat;

	struct aeon_dma *sc_dma;
};

/*
 * Processing Unit Registers (offset from BASEREG0)
 */
#define	AEON_0_PUDATA		0x00	/* Processing Unit Data */
#define	AEON_0_PUCTRL		0x04	/* Processing Unit Control */
#define	AEON_0_PUISR		0x08	/* Processing Unit Interrupt Status */
#define	AEON_0_PUCNFG		0x0c	/* Processing Unit Configuration */
#define	AEON_0_PUIER		0x10	/* Processing Unit Interrupt Enable */
#define	AEON_0_PUSTAT		0x14	/* Processing Unit Status/Chip ID */
#define	AEON_0_FIFOSTAT		0x18	/* FIFO Status */
#define	AEON_0_FIFOCNFG		0x1c	/* FIFO Configuration */
#define	AEON_0_SPACESIZE	0x20	/* Register space size */

/* Processing Unit Control Register (AEON_0_PUCTRL) */
#define	AEON_PUCTRL_CLRSRCFIFO	0x0010	/* clear source fifo */
#define	AEON_PUCTRL_STOP	0x0008	/* stop pu */
#define	AEON_PUCTRL_LOCKRAM	0x0004	/* lock ram */
#define	AEON_PUCTRL_DMAENA	0x0002	/* enable dma */
#define	AEON_PUCTRL_RESET	0x0001	/* Reset processing unit */

/* Processing Unit Interrupt Status Register (AEON_0_PUISR) */
#define	AEON_PUISR_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	AEON_PUISR_DATAERR	0x4000	/* Data error interrupt */
#define	AEON_PUISR_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	AEON_PUISR_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	AEON_PUISR_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	AEON_PUISR_SRCCMD	0x0080	/* Source command interrupt */
#define	AEON_PUISR_SRCCTX	0x0040	/* Source context interrupt */
#define	AEON_PUISR_SRCDATA	0x0020	/* Source data interrupt */
#define	AEON_PUISR_DSTDATA	0x0010	/* Destination data interrupt */
#define	AEON_PUISR_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Configuration Register (AEON_0_PUCNFG) */
#define	AEON_PUCNFG_DRAMMASK	0xe000	/* DRAM size mask */
#define	AEON_PUCNFG_DSZ_256K	0x0000	/* 256k dram */
#define	AEON_PUCNFG_DSZ_512K	0x2000	/* 512k dram */
#define	AEON_PUCNFG_DSZ_1M	0x4000	/* 1m dram */
#define	AEON_PUCNFG_DSZ_2M	0x6000	/* 2m dram */
#define	AEON_PUCNFG_DSZ_4M	0x8000	/* 4m dram */
#define	AEON_PUCNFG_DSZ_8M	0xa000	/* 8m dram */
#define	AEON_PUNCFG_DSZ_16M	0xc000	/* 16m dram */
#define	AEON_PUCNFG_DSZ_32M	0xe000	/* 32m dram */
#define	AEON_PUCNFG_DRAMREFRESH	0x1800	/* DRAM refresh rate mask */
#define	AEON_PUCNFG_DRFR_512	0x0000	/* 512 divisor of ECLK */
#define	AEON_PUCNFG_DRFR_256	0x0800	/* 256 divisor of ECLK */
#define	AEON_PUCNFG_DRFR_128	0x1000	/* 128 divisor of ECLK */
#define	AEON_PUCNFG_TCALLPHASES	0x0200	/* your guess is as good as mine... */
#define	AEON_PUCNFG_TCDRVTOTEM	0x0100	/* your guess is as good as mine... */
#define	AEON_PUCNFG_BIGENDIAN	0x0080	/* DMA big endian mode */
#define	AEON_PUCNFG_BUS32	0x0040	/* Bus width 32bits */
#define	AEON_PUCNFG_BUS16	0x0000	/* Bus width 16 bits */
#define	AEON_PUCNFG_CHIPID	0x0020	/* Allow chipid from PUSTAT */
#define	AEON_PUCNFG_DRAM	0x0010	/* Context RAM is DRAM */
#define	AEON_PUCNFG_SRAM	0x0000	/* Context RAM is SRAM */
#define	AEON_PUCNFG_COMPSING	0x0004	/* Enable single compression context */
#define	AEON_PUCNFG_ENCCNFG	0x0002	/* Encryption configuration */

/* Processing Unit Interrupt Enable Register (AEON_0_PUIER) */
#define	AEON_PUIER_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	AEON_PUIER_DATAERR	0x4000	/* Data error interrupt */
#define	AEON_PUIER_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	AEON_PUIER_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	AEON_PUIER_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	AEON_PUIER_SRCCMD	0x0080	/* Source command interrupt */
#define	AEON_PUIER_SRCCTX	0x0040	/* Source context interrupt */
#define	AEON_PUIER_SRCDATA	0x0020	/* Source data interrupt */
#define	AEON_PUIER_DSTDATA	0x0010	/* Destination data interrupt */
#define	AEON_PUIER_DSTRESULT	0x0004	/* Destination result interrupt */

/* Processing Unit Status Register/Chip ID (AEON_0_PUSTAT) */
#define	AEON_PUSTAT_CMDINVAL	0x8000	/* Invalid command interrupt */
#define	AEON_PUSTAT_DATAERR	0x4000	/* Data error interrupt */
#define	AEON_PUSTAT_SRCFIFO	0x2000	/* Source FIFO ready interrupt */
#define	AEON_PUSTAT_DSTFIFO	0x1000	/* Destination FIFO ready interrupt */
#define	AEON_PUSTAT_DSTOVER	0x0200	/* Destination overrun interrupt */
#define	AEON_PUSTAT_SRCCMD	0x0080	/* Source command interrupt */
#define	AEON_PUSTAT_SRCCTX	0x0040	/* Source context interrupt */
#define	AEON_PUSTAT_SRCDATA	0x0020	/* Source data interrupt */
#define	AEON_PUSTAT_DSTDATA	0x0010	/* Destination data interrupt */
#define	AEON_PUSTAT_DSTRESULT	0x0004	/* Destination result interrupt */
#define	AEON_PUSTAT_CHIPREV	0x00ff	/* Chip revision mask */
#define	AEON_PUSTAT_CHIPENA	0xff00	/* Chip enabled mask */
#define	AEON_PUSTAT_ENA_2	0x1100	/* Level 2 enabled */
#define	AEON_PUSTAT_ENA_1	0x1000	/* Level 1 enabled */
#define	AEON_PUSTAT_ENA_0	0x3000	/* Level 0 enabled */
#define	AEON_PUSTAT_REV_2	0x0020	/* 7751 PT6/2 */
#define	AEON_PUSTAT_REV_3	0x0030	/* 7751 PT6/3 */

/* FIFO Status Register (AEON_0_FIFOSTAT) */
#define	AEON_FIFOSTAT_SRC	0x7f00	/* Source FIFO available */
#define	AEON_FIFOSTAT_DST	0x007f	/* Destination FIFO available */

/* FIFO Configuration Register (AEON_0_FIFOCNFG) */
#define	AEON_FIFOCNFG_THRESHOLD	0x0400	/* must be written as 1 */

/*
 * DMA Interface Registers (offset from BASEREG1)
 */
#define	AEON_1_DMA_CRAR		0x0c	/* DMA Command Ring Address */
#define	AEON_1_DMA_SRAR		0x1c	/* DMA Source Ring Address */
#define	AEON_1_DMA_RRAR		0x2c	/* DMA Resultt Ring Address */
#define	AEON_1_DMA_DRAR		0x3c	/* DMA Destination Ring Address */
#define	AEON_1_DMA_CSR		0x40	/* DMA Status and Control */
#define	AEON_1_DMA_IER		0x44	/* DMA Interrupt Enable */
#define	AEON_1_DMA_CNFG		0x48	/* DMA Configuration */
#define	AEON_1_REVID		0x98	/* Revision ID */

/* DMA Status and Control Register (AEON_1_DMA_CSR) */
#define	AEON_DMACSR_D_CTRLMASK	0xc0000000	/* Destinition Ring Control */
#define	AEON_DMACSR_D_CTRL_NOP	0x00000000	/* Dest. Control: no-op */
#define	AEON_DMACSR_D_CTRL_DIS	0x40000000	/* Dest. Control: disable */
#define	AEON_DMACSR_D_CTRL_ENA	0x80000000	/* Dest. Control: enable */
#define	AEON_DMACSR_D_ABORT	0x20000000	/* Destinition Ring PCIAbort */
#define	AEON_DMACSR_D_DONE	0x10000000	/* Destinition Ring Done */
#define	AEON_DMACSR_D_LAST	0x08000000	/* Destinition Ring Last */
#define	AEON_DMACSR_D_WAIT	0x04000000	/* Destinition Ring Waiting */
#define	AEON_DMACSR_D_OVER	0x02000000	/* Destinition Ring Overflow */
#define	AEON_DMACSR_R_CTRL	0x00c00000	/* Result Ring Control */
#define	AEON_DMACSR_R_CTRL_NOP	0x00000000	/* Result Control: no-op */
#define	AEON_DMACSR_R_CTRL_DIS	0x00400000	/* Result Control: disable */
#define	AEON_DMACSR_R_CTRL_ENA	0x00800000	/* Result Control: enable */
#define	AEON_DMACSR_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	AEON_DMACSR_R_DONE	0x00100000	/* Result Ring Done */
#define	AEON_DMACSR_R_LAST	0x00080000	/* Result Ring Last */
#define	AEON_DMACSR_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	AEON_DMACSR_R_OVER	0x00020000	/* Result Ring Overflow */
#define	AEON_DMACSR_S_CTRL	0x0000c000	/* Source Ring Control */
#define	AEON_DMACSR_S_CTRL_NOP	0x00000000	/* Source Control: no-op */
#define	AEON_DMACSR_S_CTRL_DIS	0x00004000	/* Source Control: disable */
#define	AEON_DMACSR_S_CTRL_ENA	0x00008000	/* Source Control: enable */
#define	AEON_DMACSR_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	AEON_DMACSR_S_DONE	0x00001000	/* Source Ring Done */
#define	AEON_DMACSR_S_LAST	0x00000800	/* Source Ring Last */
#define	AEON_DMACSR_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	AEON_DMACSR_S_OVER	0x00000200	/* Source Ring Overflow */
#define	AEON_DMACSR_C_CTRL	0x000000c0	/* Command Ring Control */
#define	AEON_DMACSR_C_CTRL_NOP	0x00000000	/* Command Control: no-op */
#define	AEON_DMACSR_C_CTRL_DIS	0x00000040	/* Command Control: disable */
#define	AEON_DMACSR_C_CTRL_ENA	0x00000080	/* Command Control: enable */
#define	AEON_DMACSR_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	AEON_DMACSR_C_DONE	0x00000010	/* Command Ring Done */
#define	AEON_DMACSR_C_LAST	0x00000008	/* Command Ring Last */
#define	AEON_DMACSR_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	AEON_DMACSR_C_EIRQ	0x00000001	/* Command Ring Engine IRQ */

/* DMA Interrupt Enable Register (AEON_1_DMA_IER) */
#define	AEON_DMAIER_D_ABORT	0x20000000	/* Destination Ring PCIAbort */
#define	AEON_DMAIER_D_DONE	0x10000000	/* Destination Ring Done */
#define	AEON_DMAIER_D_LAST	0x08000000	/* Destination Ring Last */
#define	AEON_DMAIER_D_WAIT	0x04000000	/* Destination Ring Waiting */
#define	AEON_DMAIER_D_OVER	0x02000000	/* Destination Ring Overflow */
#define	AEON_DMAIER_R_ABORT	0x00200000	/* Result Ring PCI Abort */
#define	AEON_DMAIER_R_DONE	0x00100000	/* Result Ring Done */
#define	AEON_DMAIER_R_LAST	0x00080000	/* Result Ring Last */
#define	AEON_DMAIER_R_WAIT	0x00040000	/* Result Ring Waiting */
#define	AEON_DMAIER_R_OVER	0x00020000	/* Result Ring Overflow */
#define	AEON_DMAIER_S_ABORT	0x00002000	/* Source Ring PCI Abort */
#define	AEON_DMAIER_S_DONE	0x00001000	/* Source Ring Done */
#define	AEON_DMAIER_S_LAST	0x00000800	/* Source Ring Last */
#define	AEON_DMAIER_S_WAIT	0x00000400	/* Source Ring Waiting */
#define	AEON_DMAIER_S_OVER	0x00000200	/* Source Ring Overflow */
#define	AEON_DMAIER_C_ABORT	0x00000020	/* Command Ring PCI Abort */
#define	AEON_DMAIER_C_DONE	0x00000010	/* Command Ring Done */
#define	AEON_DMAIER_C_LAST	0x00000008	/* Command Ring Last */
#define	AEON_DMAIER_C_WAIT	0x00000004	/* Command Ring Waiting */
#define	AEON_DMAIER_ENGINE	0x00000001	/* Engine IRQ */

/* DMA Configuration Register (AEON_1_DMA_CNFG) */
#define	AEON_DMACNFG_BIGENDIAN	0x10000000	/* big endian mode */
#define	AEON_DMACNFG_POLLFREQ	0x00ff0000	/* Poll frequency mask */
#define	AEON_DMACNFG_UNLOCK	0x00000800
#define	AEON_DMACNFG_POLLINVAL	0x00000700	/* Invalid Poll Scalar */
#define	AEON_DMACNFG_LAST	0x00000010	/* Host control LAST bit */
#define	AEON_DMACNFG_MODE	0x00000004	/* DMA mode */
#define	AEON_DMACNFG_DMARESET	0x00000002	/* DMA Reset # */
#define	AEON_DMACNFG_MSTRESET	0x00000001	/* Master Reset # */

#define WRITE_REG_0(sc,reg,val) \
    bus_space_write_4((sc)->sc_st0, (sc)->sc_sh0, reg, val)
#define READ_REG_0(sc,reg) \
    bus_space_read_4((sc)->sc_st0, (sc)->sc_sh0, reg)

/*
 * Register offsets in register set 1
 */

#define	AEON_UNLOCK_SECRET1	0xf4
#define	AEON_UNLOCK_SECRET2	0xfc

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
typedef struct aeon_base_command {
	u_int16_t masks;
	u_int16_t session_num;
	u_int16_t total_source_count;
	u_int16_t total_dest_count;
} aeon_base_command_t;

#define AEON_BASE_CMD_MAC    (0x1 << 10)
#define AEON_BASE_CMD_CRYPT  (0x1 << 11)
#define AEON_BASE_CMD_DECODE (0x1 << 13)

/*
 * Structure to help build up the command data structure.
 */
typedef struct aeon_crypt_command {
	u_int16_t masks;               
	u_int16_t header_skip;
	u_int32_t source_count;
} aeon_crypt_command_t;

#define AEON_CRYPT_CMD_ALG_MASK  (0x3 << 0)
#define AEON_CRYPT_CMD_ALG_DES   (0x0 << 0)
#define AEON_CRYPT_CMD_ALG_3DES  (0x1 << 0)
#define AEON_CRYPT_CMD_MODE_CBC  (0x1 << 3)
#define AEON_CRYPT_CMD_NEW_KEY   (0x1 << 11)
#define AEON_CRYPT_CMD_NEW_IV    (0x1 << 12)

/*
 * Structure to help build up the command data structure.
 */
typedef struct aeon_mac_command {
	u_int16_t masks;  
	u_int16_t header_skip;
	u_int32_t source_count;
} aeon_mac_command_t;

#define AEON_MAC_CMD_ALG_MD5    (0x1 << 0)
#define AEON_MAC_CMD_ALG_SHA1   (0x0 << 0)
#define AEON_MAC_CMD_MODE_HMAC  (0x0 << 2)
#define AEON_MAC_CMD_TRUNC      (0x1 << 4)
#define AEON_MAC_CMD_APPEND     (0x1 << 6)
/*
 * MAC POS IPSec initiates authentication after encryption on encodes
 * and before decryption on decodes.
 */
#define AEON_MAC_CMD_POS_IPSEC  (0x2 << 8)
#define AEON_MAC_CMD_NEW_KEY    (0x1 << 11)

/*
 * Structure with all fields necessary to write the command buffer.
 * We build it up while interrupts are on, then use it to write out
 * the command buffer quickly while interrupts are off.
 */
typedef struct aeon_command_buf_data {
	aeon_base_command_t base_cmd;
	aeon_mac_command_t mac_cmd;
	aeon_crypt_command_t crypt_cmd;
	const u_int8_t *mac;
	const u_int8_t *ck;
	const u_int8_t *iv;
} aeon_command_buf_data_t;

/*
 * The poll frequency and poll scalar defines are unshifted values used
 * to set fields in the DMA Configuration Register.
 */
#ifndef AEON_POLL_FREQUENCY
#define AEON_POLL_FREQUENCY  0x1
#endif

#ifndef AEON_POLL_SCALAR
#define AEON_POLL_SCALAR    0x0
#endif

#endif /* __AEON_H__ */
