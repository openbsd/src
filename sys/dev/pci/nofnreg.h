/*	$OpenBSD: nofnreg.h,v 1.3 2002/05/15 21:33:22 jason Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#define	NOFN_BAR0_REGS		0x10		/* main register set */
#define	NOFN_BAR1		0x14		/* memory space 1 */
#define	NOFN_BAR2		0x18		/* memory space 2 */
#define	NOFN_BAR3_PK		0x1c		/* public key unit */

#define	NOFN_REVID		0x0098		/* revision id */
#define	NOFN_SPINLOCK_0		0x0118		/* spinlock 0 */
#define	NOFN_SPINLOCK_1		0x0120		/* spinlock 1 */
#define	NOFN_PCI_INT		0x0130		/* generate a pci intr */
#define	NOFN_PCI_INT_STAT	0x0138		/* pci interrupt status */
#define	NOFN_PCI_INT_MASK	0x0140		/* pci interrupt mask */

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
