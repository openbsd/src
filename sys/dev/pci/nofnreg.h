/*	$OpenBSD: nofnreg.h,v 1.6 2003/06/02 19:08:58 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#define	NOFN_BAR0_REGS		0x10		/* main register set */
#define	NOFN_BAR1		0x14		/* memory space 1 */
#define	NOFN_BAR2		0x18		/* memory space 2 */
#define	NOFN_BAR3_PK		0x1c		/* public key unit */

#define	NOFN_MIPS_PCI_BASE_0	0x0000		/* mips pci base address 0 */
#define	NOFN_MIPS_PCI_WIN_SZ_0	0x0008		/* mips pci window size 0 */
#define	NOFN_PCI_XLAT_0		0x0010		/* pci translation 0 */
#define	NOFN_MIPS_PCI_BASE_1	0x0018		/* mips pci base address 1 */
#define	NOFN_MIPS_PCI_WIN_SZ_1	0x0020		/* mips pci window size 1 */
#define	NOFN_PCI_XLAT_1		0x0028		/* pci translation 1 */
#define	NOFN_MIPS_SDRAM_BASE	0x0030		/* mips sdram base addr */
#define	NOFN_MIPS_SDRAM_SZ	0x0038		/* mips sdram size */
#define	NOFN_PCI_BARM0_SHADOW	0x0040		/* pci mbar0 shadow */
#define	NOFN_PCI_BARM1_SHADOW	0x0050		/* pci mbar1 shadow */
#define	NOFN_PCI_BARM_SZ	0x0060		/* size of memory window */
#define	NOFN_XPARNT_MEM_EN	0x0068		/* enable transparent mem */
#define	NOFN_PCI_BARR_SHADOW	0x0070		/* pci bar0 shadow, s/c regs */
#define	NOFN_MIPS_REG_BASE	0x0078		/* mips register base */
#define	NOFN_MAX_FREE_IDX	0x0080		/* maximum free index */
#define	NOFN_TRAP_IN_Q		0x0090		/* enqueue core desc */
#define	NOFN_OUT_CMPLT		0x00a0		/* output completion */
#define	NOFN_TRAP_OUT_Q		0x00a8		/* dequeue core desc */
#define	NOFN_CHOKE_CORE_IN_Q	0x00b0		/* choke core on input */
#define	NOFN_DONE_BITMAP	0x00b8		/* done bitmap */
#define	NOFN_HEAD_DESC_PTR	0x00c0		/* last core descriptor */
#define	NOFN_TAIL_DESC_PTR	0x00c8		/* next core descriptor */
#define	NOFN_RSLT_POST_ADR	0x00d0		/* result post address */
#define	NOFN_SM_CTX_BASE	0x00d8		/* small session ctx base */
#define	NOFN_DESC_BASE		0x00e8		/* core descriptor base */
#define	NOFN_LG_CTX_BASE	0x00e0		/* large context base */
#define	NOFN_SESSION_NUM_MASK	0x00f0		/* session number mask */
#define	NOFN_HOST_MBOX_0	0x00f8		/* host mailbox 0 */
#define	NOFN_REVID		0x0098		/* revision id */
#define	NOFN_HOST_SIGNAL	0x0100		/* host signal */
#define	NOFN_MIPS_MBOX_0	0x0108		/* mips mailbox 0 */
#define	NOFN_MIPS_SIGNAL	0x0110		/* mips signal */
#define	NOFN_SPINLOCK_0		0x0118		/* spinlock 0 */
#define	NOFN_SPINLOCK_1		0x0120		/* spinlock 1 */
#define	NOFN_PCI_INT		0x0130		/* generate a pci intr */
#define	NOFN_PCI_INT_STAT	0x0138		/* pci interrupt status */
#define	NOFN_PCI_INT_MASK	0x0140		/* pci interrupt mask */
#define	NOFN_MIPS_INT		0x0148		/* mips interrupt trigger */
#define	NOFN_MIPS_INT_STAT	0x0150		/* mips interrupt status */
#define	NOFN_SDRAM_CFG		0x0168		/* sdram configuration */
#define	NOFN_ENDIAN_CFG		0x0170		/* endian configuration */
#define	NOFN_EPT		0x0178		/* endian private xfer */
#define	NOFN_MIPS_RST		0x0180		/* mips reset */
#define	NOFN_EEPROM_DATA_0	0x0188		/* eeprom data 0 */
#define	NOFN_EEPROM_DATA_1	0x0190		/* eeprom data 1 */
#define	NOFN_EEPROM_DATA_2	0x0198		/* eeprom data 2 */
#define	NOFN_EEPROM_DATA_3	0x01a0		/* eeprom data 3 */
#define	NOFN_CHIP_CFG		0x01d8		/* chip configuration */
#define	NOFN_SCRATCH_0		0x01e0		/* scratch 0 */
#define	NOFN_SCRATCH_1		0x01e8		/* scratch 1 */
#define	NOFN_SCRATCH_2		0x01f0		/* scratch 2 */
#define	NOFN_SCRATCH_3		0x01f8		/* scratch 3 */
#define	NOFN_CMD_RING_CTL	0x0200		/* command ring control */
#define	NOFN_CMD_RING_BASE	0x0208		/* command ring base */
#define	NOFN_CMD_RING_LEN	0x0210		/* command ring length */
#define	NOFN_CMD_RING_HEAD	0x0218		/* command ring head */
#define	NOFN_CMD_RING_TAIL	0x0220		/* command ring tail */
#define	NOFN_DST_RING_CTL	0x0230		/* dest ring control */
#define	NOFN_DST_RING_BASE	0x0238		/* dest ring base */
#define	NOFN_DST_RING_LEN	0x0240		/* dest ring length */
#define	NOFN_DST_RING_HEAD	0x0248		/* dest ring head */
#define	NOFN_DST_RING_TAIL	0x0250		/* dest ring tail */
#define	NOFN_RSLT_RING_CTL	0x0258		/* result ring control */
#define	NOFN_RSLT_RING_BASE	0x0260		/* result ring base */
#define	NOFN_RSLT_RING_LEN	0x0268		/* result ring length */
#define	NOFN_RSLT_RING_HEAD	0x0270		/* result ring head */
#define	NOFN_RSLT_RING_TAIL	0x0278		/* result ring tail */
#define	NOFN_SRC_RING_CTL	0x0288		/* src ring control */
#define	NOFN_SRC_RING_BASE	0x0290		/* src ring base */
#define	NOFN_SRC_RING_LEN	0x0298		/* src ring len */
#define	NOFN_SRC_RING_HEAD	0x02a0		/* src ring head */
#define	NOFN_SRC_RING_TAIL	0x02a8		/* src ring tail */
#define	NOFN_FREE_RING_CTL	0x02b8		/* free ring control */
#define	NOFN_FREE_RING_BASE	0x02c0		/* free ring base */
#define	NOFN_FREE_RING_LEN	0x02c8		/* free ring len */
#define	NOFN_FREE_RING_HEAD	0x02d0		/* free ring head */
#define	NOFN_FREE_RING_TAIL	0x02d8		/* free ring tail */
#define	NOFN_ECC_TEST		0x02e0		/* ecc test */
#define	NOFN_ECC_SNGL_ADR	0x02e8		/* ecc singlebit error addr */
#define	NOFN_ECC_SNGL_ECC	0x02f0		/* ecc singlebit error bits */
#define	NOFN_ECC_SNGL_CNT	0x02f8		/* ecc singlebit error count */
#define	NOFN_ECC_MULTI_ADR	0x0300		/* ecc multibit error addr */
#define	NOFN_ECC_MULTI_ECC	0x0308		/* ecc multibit error bits */
#define	NOFN_ECC_MULTI_DATAL	0x0310		/* ecc multibit err data lo */
#define	NOFN_ECC_MULTI_DATAH	0x0318		/* ecc multibit err data hi */
#define	NOFN_MIPS_ERR_ADR	0x0320		/* mips parity error addr */
#define	NOFN_MIPS_ERR_DATA	0x0328		/* mips parity error data */
#define	NOFN_MIPS_ERR_PAR	0x0330		/* mips parity error bits */
#define	NOFN_MIPS_PAR_TEST	0x0338		/* mips parity test */
#define	NOFN_MIPS_MBOX_1	0x0340		/* mips mailbox 1 */
#define	NOFN_MIPS_MBOX_2	0x0348		/* mips mailbox 2 */
#define	NOFN_MIPS_MBOX_3	0x0350		/* mips mailbox 3 */
#define	NOFN_HOST_MBOX_1	0x0358		/* host mailbox 1 */
#define	NOFN_HOST_MBOX_2	0x0360		/* host mailbox 2 */
#define	NOFN_HOST_MBOX_3	0x0368		/* host mailbox 3 */
#define	NOFN_MIPS_INT_0_MASK	0x0400		/* mips intr mask 0 */
#define	NOFN_MIPS_INT_1_MASK	0x0408		/* mips intr mask 1 */
#define	NOFN_MIPS_INT_2_MASK	0x0410		/* mips intr mask 2 */
#define	NOFN_MIPS_PCI_RD_ERRADR	0x0418		/* failed read address */
#define	NOFN_MIPS_PCI_WR_ERRADR	0x0420		/* failed write address */
#define	NOFN_MIPS_EP0_LMT	0x0428		/* mips private limit */
#define	NOFN_PCI_INIT_ERR_ADR	0x0430		/* pci failure address */
#define	NOFN_ECC_SNGL_DATAL	0x0438		/* ecc singlebit err dat lo */
#define	NOFN_ECC_SNGL_DATAH	0x0440		/* ecc singlebit err dat hi */
#define	NOFN_GPDMA_DATA_0	0x0480		/* gpdma data 0 */
#define	NOFN_GPDMA_DATA_1	0x0488		/* gpdma data 1 */
#define	NOFN_GPDMA_DATA_2	0x0490		/* gpdma data 2 */
#define	NOFN_GPDMA_DATA_3	0x0498		/* gpdma data 3 */
#define	NOFN_GPDMA_DATA_4	0x04a0		/* gpdma data 4 */
#define	NOFN_GPDMA_DATA_5	0x04a8		/* gpdma data 5 */
#define	NOFN_GPDMA_DATA_6	0x04b0		/* gpdma data 6 */
#define	NOFN_GPDMA_DATA_7	0x04b8		/* gpdma data 7 */
#define	NOFN_GPDMA_DATA_8	0x04c0		/* gpdma data 8 */
#define	NOFN_GPDMA_DATA_9	0x04c8		/* gpdma data 9 */
#define	NOFN_GPDMA_DATA_10	0x04d0		/* gpdma data 10 */
#define	NOFN_GPDMA_DATA_11	0x04d8		/* gpdma data 11 */
#define	NOFN_GPDMA_DATA_12	0x04e0		/* gpdma data 12 */
#define	NOFN_GPDMA_DATA_13	0x04e8		/* gpdma data 13 */
#define	NOFN_GPDMA_DATA_14	0x04f0		/* gpdma data 14 */
#define	NOFN_GPDMA_DATA_15	0x04f8		/* gpdma data 15 */
#define	NOFN_GPDMA_CTL		0x0500		/* gpdma control */
#define	NOFN_GPDMA_ADR		0x0508		/* gpdma address */
#define	NOFN_MIPS_PK_BASE	0x0510		/* mips pk base address */
#define	NOFN_PCI_BAR3_SHADOW	0x0518		/* pci bar3 shadow */
#define	NOFN_AES_ROUNDS		0x0520		/* number of rounds in AES */
#define	NOFN_MIPS_INIT		0x0530		/* mips init */
#define	NOFN_MIPS_INIT_CTL	0x0528		/* mips init control */
#define	NOFN_UART_TX		0x0538		/* uart tx data */
#define	NOFN_UART_RX		0x0540		/* uart rx data */
#define	NOFN_UART_CLK_LOW	0x0548		/* uart clock, low bits */
#define	NOFN_UART_CLK_HIGH	0x0550		/* uart clock, high bits */
#define	NOFN_UART_STAT		0x0558		/* uart status */
#define	NOFN_MIPS_FLASH_BASE	0x0560		/* mips flash base addr */
#define	NOFN_MIPS_FLASH_SZ	0x0568		/* mips flash size */
#define	NOFN_MIPS_FLASH_XLATE	0x0570		/* mips address translation */
#define	NOFN_MIPS_SRAM_BASE	0x0578		/* mips sram base address */
#define	NOFN_MIPS_SRAM_SZ	0x0580		/* mips sram size */
#define	NOFN_MIPS_SRAM_XLATE	0x0588		/* mips sram xlate */
#define	NOFN_MIPS_MEM_LOCK	0x0590		/* mips memory lock */
#define	NOFN_MIPS_SDRAM2_BASE	0x0598		/* mips sdram2 base addr */
#define	NOFN_MIPS_SDRAM2_SZ	0x05a0		/* mips sdram2 size */
#define	NOFN_MIPS_EPO2_LMT	0x05a8		/* address boundary */

#define	REVID_7851_1		0x00140000	/* 7851, first silicon */
#define	REVID_7851_2		0x00140001	/* 7851, second silicon */
#define	REVID_7814_7854_1	0x00140002	/* 7814/7854, first silicon */
#define	REVID_8154_1		0x00180000	/* 8154, first silicon */
#define	REVID_8065_1		0x00160000	/* 8065, first silicon */
#define	REVID_8165_1		0x00170000	/* 8165, first silicon */

#define	PCIINTMASK_PK		0x80000000	/* pk processor idle */
#define	PCIINTMASK_RNGRDY	0x40000000	/* pk rng has 8 32bit words */
#define	PCIINTMASK_MIPSINT	0x00080000	/* mips interrupt */
#define	PCIINTMASK_ERR_OUT	0x00010000	/* err: outbound dma */
#define	PCIINTMASK_ERR_FREE	0x00008000	/* err: free descriptor */
#define	PCIINTMASK_ERR_DEST	0x00004000	/* err: dest descriptor */
#define	PCIINTMASK_ERR_RES	0x00002000	/* err: result cycle */
#define	PCIINTMASK_ERR_RESP	0x00001000	/* err: post address cycle */
#define	PCIINTMASK_DESCOVF	0x00000800	/* descriptor overflow */
#define	PCIINTMASK_RESDONE	0x00000400	/* result done */
#define	PCIINTMASK_DESTINV	0x00000200	/* destination invalidated */
#define	PCIINTMASK_POOLINV	0x00000100	/* free desc invalidated */
#define	PCIINTMASK_ERR_CMD	0x00000080	/* pci error fetching cmd */
#define	PCIINTMASK_ERR_SRC	0x00000040	/* pci error fetching src */
#define	PCIINTMASK_ERR_IN	0x00000020	/* pci error during input */
#define	PCIINTMASK_ERR_MULTI	0x00000010	/* multibit ecc error */
#define	PCIINTMASK_MIPSPAR	0x00000004	/* mips parity error */
#define	PCIINTMASK_MIPSPCI	0x00000002	/* write to pciint register */
#define	PCIINTMASK_FREE_E	0x00000001	/* free-pool empty */

#define	PCIINTSTAT_PK		0x80000000	/* pk processor idle */
#define	PCIINTSTAT_RNGRDY	0x40000000	/* pk rng has 8 32bit words */
#define	PCIINTSTAT_MIPSINT	0x00080000	/* mips interrupt */
#define	PCIINTSTAT_ERR_OUT	0x00010000	/* err: outbound dma */
#define	PCIINTSTAT_ERR_FREE	0x00008000	/* err: free descriptor */
#define	PCIINTSTAT_ERR_DEST	0x00004000	/* err: dest descriptor */
#define	PCIINTSTAT_ERR_RES	0x00002000	/* err: result cycle */
#define	PCIINTSTAT_ERR_RESP	0x00001000	/* err: post address cycle */
#define	PCIINTSTAT_DESCOVF	0x00000800	/* descriptor overflow */
#define	PCIINTSTAT_RESDONE	0x00000400	/* result done */
#define	PCIINTSTAT_DESTINV	0x00000200	/* destination invalidated */
#define	PCIINTSTAT_POOLINV	0x00000100	/* free desc invalidated */
#define	PCIINTSTAT_ERR_CMD	0x00000080	/* pci error fetching cmd */
#define	PCIINTSTAT_ERR_SRC	0x00000040	/* pci error fetching src */
#define	PCIINTSTAT_ERR_IN	0x00000020	/* pci error during input */
#define	PCIINTSTAT_ERR_MULTI	0x00000010	/* multibit ecc error */
#define	PCIINTSTAT_MIPSPAR	0x00000004	/* mips parity error */
#define	PCIINTSTAT_MIPSPCI	0x00000002	/* write to pciint register */
#define	PCIINTSTAT_FREE_E	0x00000001	/* free-pool empty */

#define	NOFN_PK_WIN_0	0x0000	/* big endian byte and words */
#define	NOFN_PK_WIN_1	0x2000	/* big endian byte, little endian words */
#define	NOFN_PK_WIN_2	0x4000	/* little endian byte and words */
#define	NOFN_PK_WIN_3	0x6000	/* little endian byte, big endian words */

#define	NOFN_PK_REGADDR(win,r,i)	\
    ((win) | (((r) & 0xf) << 7) | (((i) & 0x1f) << 2))

#define	NOFN_PK_LENADDR(r)	(0x1000 + ((r) << 2))
#define	NOFN_PK_LENMASK		0x000007ff	/* mask of length bits */

#define	NOFN_PK_RNGFIFO_BEGIN	0x1080
#define	NOFN_PK_RNGFIFO_END	0x10bc
#define	NOFN_PK_INSTR_BEGIN	0x1100
#define	NOFN_PK_INSTR_END	0x12fc

#define	NOFN_PK_CR		0x1fd4		/* command */
#define	NOFN_PK_SR		0x1fd8		/* status */
#define	NOFN_PK_IER		0x1fdc		/* interrupt enable */
#define	NOFN_PK_RNC		0x1fe0		/* random number config */
#define	NOFN_PK_CFG1		0x1fe4		/* config1 */
#define	NOFN_PK_CFG2		0x1fe8		/* config2 */
#define	NOFN_PK_CHIPID		0x1fec		/* chipid */
#define	NOFN_PK_SCR		0x1ff0		/* stack content */

#define	PK_CR_OFFSET_M		0x000001fc	/* instruction offset mask */
#define	PK_CR_OFFSET_S		2		/* instruction offset shift */

#define	PK_SR_DONE		0x00008000	/* proc is idle */
#define	PK_SR_RRDY		0x00004000	/* random number ready */
#define	PK_SR_UFLOW		0x00001000	/* random number underflow */
#define	PK_SR_CARRY		0x00000008	/* alu carry bit */

#define	PK_IER_DONE		0x00008000	/* intr when alu is idle */
#define	PK_IER_RRDY		0x00004000	/* intr when rng ready */

#define	PK_RNC_FST_SCALER	0x00000f00	/* first prescaler */
#define	PK_RNC_OUT_SCALER	0x00000080	/* output prescaler */

#define	PK_CFG1_RESET		0x00000001	/* reset pk unit */

#define	PK_CFG2_ALU_ENA		0x00000002	/* enable alu */
#define	PK_CFG2_RNG_ENA		0x00000001	/* enable rng */

/* alu registers are 1024 bits wide, but must be addressed by word. */
union nofn_pk_reg {
	u_int8_t b[128];
	u_int32_t w[32];
};

#define	PK_OP_DONE		0x80000000	/* end of program */
#define	PK_OPCODE_MASK		0x7c000000	/* opcode mask */
#define	PK_OPCODE_MODEXP	0x00000000	/*  modular exponentiation */
#define	PK_OPCODE_MODMUL	0x04000000	/*  modular multiplication */
#define	PK_OPCODE_MODRED	0x08000000	/*  modular reduction */
#define	PK_OPCODE_MODADD	0x0c000000	/*  modular addition */
#define	PK_OPCODE_MODSUB	0x10000000	/*  modular subtraction */
#define	PK_OPCODE_ADD		0x14000000	/*  addition */
#define	PK_OPCODE_SUB		0x18000000	/*  subtraction */
#define	PK_OPCODE_ADDC		0x1c000000	/*  addition with carry */
#define	PK_OPCODE_SUBC		0x20000000	/*  subtraction with carry */
#define	PK_OPCODE_MULT		0x24000000	/*  2048bit multiplication */
#define	PK_OPCODE_SR		0x28000000	/*  shift right */
#define	PK_OPCODE_SL		0x2c000000	/*  shift left */
#define	PK_OPCODE_INC		0x30000000	/*  increment */
#define	PK_OPCODE_DEC		0x34000000	/*  decrement */
#define	PK_OPCODE_TAG		0x38000000	/*  set length tag */
#define	PK_OPCODE_BRANCH	0x3c000000	/*  jump to insn */
#define	PK_OPCODE_CALL		0x40000000	/*  push addr and jump */
#define	PK_OPCODE_RETURN	0x44000000	/*  pop addr and return */

#define	PK_OP_RD_SHIFT		21
#define	PK_OP_RA_SHIFT		16
#define	PK_OP_RB_SHIFT		11
#define	PK_OP_RM_SHIFT		6
#define	PK_OP_R_MASK		0x1f
#define	PK_OP_LEN_MASK		0xffff

#define	NOFN_PK_INSTR(done,op,rd,ra,rb,rm) 		\
    ((done) | (op) |					\
     (((rd) & PK_OP_R_MASK) << PK_OP_RD_SHIFT) |	\
     (((ra) & PK_OP_R_MASK) << PK_OP_RA_SHIFT) |	\
     (((rb) & PK_OP_R_MASK) << PK_OP_RB_SHIFT) |	\
     (((rm) & PK_OP_R_MASK) << PK_OP_RM_SHIFT))

/* shift left, shift right, tag */
#define	NOFN_PK_INSTR2(done,op,rd,ra,len)		\
    ((done) | (op) |					\
     (((rd) & PK_OP_R_MASK) << PK_OP_RD_SHIFT) |	\
     (((ra) & PK_OP_R_MASK) << PK_OP_RA_SHIFT) |	\
     ((len) & PK_OP_LEN_MASK))
