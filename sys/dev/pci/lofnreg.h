/*	$OpenBSD: lofnreg.h,v 1.15 2003/06/02 19:08:58 jason Exp $	*/

/*
 * Copyright (c) 2001-2002 Jason L. Wright (jason@thought.net)
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

#define	LOFN_BAR0		0x0010		/* base address register */

#define	LOFN_WIN_0		0x0000		/* 0 - rev byte, norm word */
#define	LOFN_WIN_1		0x2000		/* 1 - rev byte, rev word */
#define	LOFN_WIN_2		0x4000		/* 2 - norm byte, norm word */
#define	LOFN_WIN_3		0x6000		/* 3 - norm byte, rev word */

/* Bignum registers */
#define	LOFN_REL_DATA		0x0000		/* Data registers */
#define	LOFN_REL_DATA_END	0x07ff
/* Length registers */
#define	LOFN_REL_LEN		0x1000		/* Length tags */
#define	LOFN_REL_LEN_REGS	0x103f
/* RNG FIFO space */
#define	LOFN_REL_RNG		0x1080		/* RNG FIFO start */
#define	LOFN_REL_RNG_END	0x10bf		/* RNG FIFO end */
/* Instruction space */
#define	LOFN_REL_INSTR		0x1100		/* Instructions */
#define	LOFN_REL_INSTR_END	0x117f
/* Control and status registers, relative to window number */
#define	LOFN_REL_CR		0x1fd4		/* Command */
#define	LOFN_REL_SR		0x1fd8		/* Status */
#define	LOFN_REL_IER		0x1fdc		/* Interrupt enable */
#define	LOFN_REL_RNC		0x1fe0		/* RNG config */
#define	LOFN_REL_CFG1		0x1fe4		/* Config1 */
#define	LOFN_REL_CFG2		0x1fe8		/* Config2 */
#define	LOFN_REL_CHIPID		0x1fec		/* Chip ID */

/* Data register access */
#define	LOFN_REG_MASK		0x0f80		/* Register number mask */
#define	LOFN_REG_SHIFT		7
#define	LOFN_WORD_MASK		0x007c		/* Word index mask */
#define	LOFN_WORD_SHIFT		2

/* Command address register (LOFN_REL_CR) */
#define	LOFN_CR_ADDR_MASK	0x0000003f	/* Instruction addr offset */

/* Status register (LOFN_REL_SR) */
#define	LOFN_SR_CARRY		0x00000008	/* Carry from operation */
#define	LOFN_SR_RNG_UF		0x00001000	/* RNG underflow */
#define	LOFN_SR_RNG_RDY		0x00004000	/* RNG ready */
#define	LOFN_SR_DONE		0x00008000	/* Operation done */

/* Interrupt enable register (LOFN_REL_IER) */
#define	LOFN_IER_RDY		0x00004000	/* RNG ready */
#define	LOFN_IER_DONE		0x00008000	/* Operation done */

/* Random number configuration (LOFN_REL_RNC) */
#define	LOFN_RNC_OUTSCALE	0x00000080	/* Output prescalar */
#define	LOFN_RNC_1STSCALE	0x00000f00	/* First prescalar */

/* Config register 1 (LOFN_REL_CFG1) */
#define	LOFN_CFG1_RESET		0x00000001	/* Reset */
#define	LOFN_CFG1_MULTI		0x00000038	/* PLL multiple */
#define	LOFN_CFG1_MULTI_BYP	0x00000000	/*  PLL bypass */
#define	LOFN_CFG1_MULTI_1X	0x00000008	/*  1x CLK */
#define	LOFN_CFG1_MULTI_15X	0x00000010	/*  1.5x CLK */
#define	LOFN_CFG1_MULTI_2X	0x00000018	/*  2x CLK */
#define	LOFN_CFG1_MULTI_25X	0x00000020	/*  2.5x CLK */
#define	LOFN_CFG1_MULTI_3X	0x00000028	/*  3x CLK */
#define	LOFN_CFG1_MULTI_4X	0x00000030	/*  4x CLK */
#define	LOFN_CFG1_CLOCK		0x00000040	/* Clock select */

/* Config register 2 (LOFN_REL_CFG2) */
#define	LOFN_CFG2_RNGENA	0x00000001	/* RNG enable */
#define	LOFN_CFG2_PRCENA	0x00000002	/* Processor enable */

/* Chip identification (LOFN_REL_CHIPID) */
#define	LOFN_CHIPID_MASK	0x0000ffff	/* Chip ID */

#define	LOFN_REGADDR(win,r,idx) 			\
    ((win) |						\
     (((r) << LOFN_REG_SHIFT) & LOFN_REG_MASK) |	\
     (((idx) << LOFN_WORD_SHIFT) & LOFN_WORD_MASK))

#define	LOFN_LENADDR(win,r)				\
    ((win) | (((r) << 2) + LOFN_REL_LEN))

#define	LOFN_LENMASK		0x000007ff	/* mask for length space */

#define	OP_DONE			0x80000000	/* final instruction */
#define	OP_CODE_MASK		0x7c000000	/* opcode mask */
#define	OP_CODE_MODEXP		0x00000000	/*  modular exponentiation */
#define	OP_CODE_MODMUL		0x04000000	/*  modular multiplication */
#define	OP_CODE_MODRED		0x08000000	/*  modular reduction */
#define	OP_CODE_MODADD		0x0c000000	/*  modular addition */
#define	OP_CODE_MODSUB		0x10000000	/*  modular subtraction */
#define	OP_CODE_ADD		0x14000000	/*  addition */
#define	OP_CODE_SUB		0x18000000	/*  subtraction */
#define	OP_CODE_ADDC		0x1c000000	/*  addition with carry */
#define	OP_CODE_SUBC		0x20000000	/*  subtraction with carry */
#define	OP_CODE_MULT		0x24000000	/*  2048bit multiplication */
#define	OP_CODE_SR		0x28000000	/*  shift right */
#define	OP_CODE_SL		0x2c000000	/*  shift left */
#define	OP_CODE_INC		0x30000000	/*  increment */
#define	OP_CODE_DEC		0x34000000	/*  decrement */
#define	OP_CODE_TAG		0x38000000	/*  set length tag */
#define	OP_CODE_NOP		0x7c000000	/*  nop */

#define	OP_RD_SHIFT		21
#define	OP_RA_SHIFT		16
#define	OP_RB_SHIFT		11
#define	OP_RM_SHIFT		6
#define	OP_R_MASK		0x1f
#define	OP_LEN_MASK		0xffff

#define	LOFN_INSTR(done,op,rd,ra,rb,rm) 		\
    ((done) | (op) |					\
     (((rd) & OP_R_MASK) << OP_RD_SHIFT) |		\
     (((ra) & OP_R_MASK) << OP_RA_SHIFT) |		\
     (((rb) & OP_R_MASK) << OP_RB_SHIFT) |		\
     (((rm) & OP_R_MASK) << OP_RM_SHIFT))

#define	LOFN_INSTR2(done,op,rd,ra,len)			\
    ((done) | (op) |					\
     (((rd) & OP_R_MASK) << OP_RD_SHIFT) |		\
     (((ra) & OP_R_MASK) << OP_RA_SHIFT) |		\
     ((len) & OP_LEN_MASK))

/* registers are 1024 bits wide, but must be addressed by word. */
union lofn_reg {
	u_int8_t b[128];
	u_int32_t w[32];
};

