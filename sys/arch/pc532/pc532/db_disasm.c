/*	$NetBSD: db_disasm.c,v 1.2 1994/10/26 08:24:55 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * Copyright (c) 1992 Helsinki University of Technology
 * Copyright (c) 1992 Bob Krause, Bruce Culbertson
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON AND HELSINKI UNIVERSITY OF TECHNOLOGY AND BOB
 * KRAUSE AND BRUCE CULBERTSON ALLOW FREE USE OF THIS SOFTWARE IN ITS
 * "AS IS" CONDITION.  CARNEGIE MELLON AND HELSINKI UNIVERSITY OF
 * TECHNOLOGY AND BOB KRAUSE AND BRUCE CULBERTSON DISCLAIM ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */
/*
 * From Bruce Culbertson's and Bob Krause's ROM monitor for the pc532 
 * adapted to pc532 Mach by Tero Kivinen and Tatu Ylonen at Helsinki 
 * University of Technology.
 */

/*
 * Adapted for 532bsd (386bsd port) by Phil Nelson.
 *
 * Not yet complete!
 *
 */

#define STATIC static

/* #include <mach/boolean.h> */
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <machine/pmap.h>
#include <vm/vm_map.h>
/* #include <kern/thread.h> */

struct operand {
	int	o_mode;		/* address mode */
	int	o_reg0;		/* first register */
	int	o_reg1;		/* second register */
	long	o_disp0;	/* first displacment value */
	long	o_disp1;	/* second displacement value */
	int	o_iscale;	/* scaled indexing factor */
	int	o_ireg;		/* scaled indexing register */
};

struct insn {
	int		i_format;	/* instruction format */
	int		i_op;		/* operation code */
	char		i_monic[8];	/* mneumonic string */
	unsigned char	i_iol;		/* integer operand length */
	struct	operand i_opr[4];	/* operands */
};

/* Addressing modes (not the same format as in a machine instruction) */
#define AMODE_NONE	0   /* operand not used by instruction */
#define AMODE_REG	1   /* register */
#define AMODE_RREL	2   /* register relative */
#define AMODE_MREL	3   /* memory relative */
#define AMODE_IMM	4   /* immediate */
#define AMODE_ABS	5   /* absolute */
#define AMODE_EXT	6   /* external */
#define AMODE_TOS	7   /* top of stack */
#define AMODE_MSPC	8   /* memory space */
#define AMODE_REGLIST	9   /* a register list */
#define AMODE_QUICK	10  /* quick integer (-8 .. +7) */
#define AMODE_INVALID	11  /* invalid address mode found */
#define AMODE_AREG	12  /* CPU dedicated register */
#define AMODE_BLISTB	13  /* byte length bit list */
#define AMODE_BLISTW	14  /* word length bit list */
#define AMODE_BLISTD	15  /* double-word length bit list */
#define AMODE_SOPT	16  /* options in string instructions */
#define AMODE_CFG	17  /* configuration bits */
#define AMODE_MREG	18  /* memory management register */
#define AMODE_CINV	19  /* cache invalidate options */

/* Instruction types */
#define ITYPE_FMT0  0
#define ITYPE_FMT1  1
#define ITYPE_FMT2  2
#define ITYPE_FMT3  3
#define ITYPE_FMT4  4
#define ITYPE_FMT5  5
#define ITYPE_FMT6  6
#define ITYPE_FMT7  7
#define ITYPE_FMT8  8
#define ITYPE_FMT9  9
#define ITYPE_NOP   10
#define ITYPE_FMT11 11
#define ITYPE_FMT12 12
#define ITYPE_UNDEF 13
#define ITYPE_FMT14 14
#define ITYPE_FMT15 15

/* Table to determine the 'condition' field of an instruction */
STATIC
char cond_table[16][3] = {
	"eq",	/* equal */
	"ne",	/* not equal */
	"cs",	/* carry set */
	"cc",	/* carry clear */
	"hi",	/* higher */
	"ls",	/* lower or same */
	"gt",	/* greater than */
	"le",	/* less than or equal */
	"fs",	/* flag set */
	"fc",	/* flag clear */
	"lo",	/* lower */
	"hs",	/* higher or same */
	"lt",	/* less than */
	"ge",	/* greater than or equal */
	"r",	/* unconditional branch */
	"??"	/* never branch (nop) */
};

#define IOL(x)	    (((x)&0x3) + 1)
#define IOL_BYTE	1			/* Byte (8-bits) */
#define IOL_WORD	2			/* Word (16-bits) */
#define IOL_NONE	3			/* Does not apply */
#define IOL_DOUBLE	4			/* Double Word (32-bits) */

STATIC
char iol_table[5][2] = {
	"?",				    /* Should never appear */
	"b",				    /* byte */
	"w",				    /* word */
	"?",				    /* undefined */
	"d"				    /* double word */
};

STATIC
char fol_table[2][2] = {
	"l",				    /* long floating-point */
	"f"				    /* standard floating-point */
};

STATIC
char scale_table[5][2] = {
	"?",				    /* no scaled indexing */
	"b",				    /* byte */
	"w",				    /* word */
	"d",				    /* double-word */
	"q"				    /* quad-word */
};

STATIC
char cfg_table[4][2] = {
	"i",				    /* vectored interrupt enable */
	"f",				    /* floating point enable */
	"m",				    /* memory management enable */
	"c"				    /* custom slave enable */
};

STATIC
char sopt_table[8][4] = {
	"",				    /* no options */
	"b",				    /* backward */
	"w",				    /* while match */
	"b,w",				    /* backward, while match */
	"?",				    /* undefined */
	"b,?",				    /* undefined */
	"u",				    /* until match */
	"b,u"				    /* backward, until match */
};

#define AREG_US	    0x0		/* user stack */
#define AREG_FP	    0x8		/* frame pointer */
#define AREG_SP	    0x9		/* stack pointer */
#define AREG_SB	    0xa		/* static base */
#define AREG_PSR    0xd		/* processor status register */
#define AREG_INTBASE	0xe	/* interrupt base */
#define AREG_MOD    0xf		/* module */

/* Floating-point operand length field masks */
#define FOL_SINGLE  0x1		/* Single Precision (32-bits) */
#define FOL_DOUBLE  0x0		/* Double Precision (64-bits) */

#define FMT0_COND(x)	(((x)&0xf0)>>4)		/* Condition code for fmt 0 */
#define FMT1_OP(x)	(((x)&0xf0)>>4)		/* Op code for fmt 1 */
#define FMT2_OP(x)	(((x)&0x70)>>4)		/* Op code for fmt 2 */
#define FMT2_COND(x, y) (((x)>>7) + (((y)&0x80)<<1)) /* Condition code for fmt 2 */
#define FMT3_OP(x)	((x)&0x7)		/* bits 2-4 of fmt 3 op code */
#define FMT4_OP(x)	(((x)&0x3c)>>2)		/* Op code for fmt 4 */
#define FMT5_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 5 */
#define FMT6_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 6 */
#define FMT7_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 7 */
#define FMT8_OP(x, y)	((((x)&0xc0)>>6) + ((y)&0x4))
#define FMT8_REG(x)	(((x)&0x38)>>3)		/* register operand for fmt 8 */
#define FMT8_SU		1			/* register value -> movsu */
#define FMT8_US		3			/* register value -> movus */
#define FMT9_OP(x)	(((x)&0x38)>>3)		/* op code for fmt 9 */
#define FMT9_F(x)	(((x)&0x4)>>2)		/* float type for fmt 9 */
#define FMT11_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 11 */
#define FMT12_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 12 */
#define FMT11_F(x)	((x)&01)		/* float type for fmt 11 */
#define FMT12_F(x)	((x)&01)		/* float type for fmt 12 */
#define FMT14_OP(x)	(((x)&0x3c)>>2)		/* op code for fmt 14 */

#define GEN_R0	    0	/* register 0 */
#define GEN_R1	    1	/* register 1 */
#define GEN_R2	    2	/* register 2 */
#define GEN_R3	    3	/* register 3 */
#define GEN_R4	    4	/* register 4 */
#define GEN_R5	    5	/* register 5 */
#define GEN_R6	    6	/* register 6 */
#define GEN_R7	    7	/* register 7 */
#define GEN_RR0	    8	/* register 0 relative */
#define GEN_RR1	    9	/* register 1 relative */
#define GEN_RR2	    10	/* register 2 relative */
#define GEN_RR3	    11	/* register 3 relative */
#define GEN_RR4	    12	/* register 4 relative */
#define GEN_RR5	    13	/* register 5 relative */
#define GEN_RR6	    14	/* register 6 relative */
#define GEN_RR7	    15	/* register 7 relative */
#define GEN_FRMR    16	/* frame memory relative */
#define GEN_SPMR    17	/* stack memory relative */
#define GEN_SBMR    18	/* static memory relative */
#define GEN_RES	    19	/* reserved for future use */
#define GEN_IMM	    20	/* immediate */
#define GEN_ABS	    21	/* absolute */
#define GEN_EXT	    22	/* external */
#define GEN_TOS	    23	/* top of stack */
#define GEN_FRM	    24	/* frame memory */
#define GEN_SPM	    25	/* stack memory */
#define GEN_SBM	    26	/* static memory */
#define GEN_PCM	    27	/* program memory */
#define GEN_SIB	    28	/* scaled index, bytes */
#define GEN_SIW	    29	/* scaled index, words */
#define GEN_SID	    30	/* scaled index, double words */
#define GEN_SIQ	    31	/* scaled index, quad words */

STATIC
unsigned char fmttab[128] = {
/* This is really an array of 256 nibbles.  The index is the first
 * byte of an instruction opcode.  The value in the array is the
 * instruction format corresponding to that first byte.
 *
 * group    1st byte	    insn format	    insn type
 */
(4+	    /* 00000000 format 4    add .b    */
(4*16)),    /* 00000001 format 4    add .w    */
(1+	    /* 00000010 format 1    bsr	      */
(4*16)),    /* 00000011 format 4    add .l    */
(4+	    /* 00000100 format 4    cmp .b    */
(4*16)),    /* 00000101 format 4    cmp .w    */
(13+	    /* 00000110 format 19   undefined */
(4*16)),    /* 00000111 format 4    cmp .l    */
(4+	    /* 00001000 format 4    bic .b    */
(4*16)),    /* 00001001 format 4    bic .w    */
(0+	    /* 00001010 format 0    beq	      */
(4*16)),    /* 00001011 format 4    bic .w    */
(2+	    /* 00001100 format 2    addq .b   */
(2*16)),    /* 00001101 format 2    addq .w   */
(5+	    /* 00001110 format 5	      */
(2*16)),    /* 00001111 format 2    addq .l   */
(4+	    /* 00010000 format 4    addc .b   */
(4*16)),    /* 00010001 format 4    addc .w   */
(1+	    /* 00010010 format 1    ret	      */
(4*16)),    /* 00010011 format 4    addc .l   */
(4+	    /* 00010100 format 4    mov .b    */
(4*16)),    /* 00010101 format 4    mov .w    */
(15+	    /* 00010110 format 15	      */
(4*16)),    /* 00010111 format 4    mov .l    */
(4+	    /* 00011000 format 4    or .b     */
(4*16)),    /* 00011001 format 4    or .w     */
(0+	    /* 00011010 format 0    bne	      */
(4*16)),    /* 00011011 format 4    or .l     */
(2+	    /* 00011100 format 2    cmpq .b   */
(2*16)),    /* 00011101 format 2    cmpq .w   */
(14+	    /* 00011110 format 14	      */
(2*16)),    /* 00011111 format 2    cmpq .l   */
(4+	    /* 00100000 format 4    sub .b    */
(4*16)),    /* 00100001 format 4    sub .w    */
(1+	    /* 00100010 format 1    cxp	      */
(4*16)),    /* 00100011 format 4    sub .l    */
(4+	    /* 00100100 format 4    addr .b   */
(4*16)),    /* 00100101 format 4    addr .w   */
(13+	    /* 00100110 format 19   undefined */
(4*16)),    /* 00100111 format 4    addr .l   */
(4+	    /* 00101000 format 4    and .b    */
(4*16)),    /* 00101001 format 4    and .w    */
(0+	    /* 00101010 format 0    bcs	      */
(4*16)),    /* 00101011 format 4    and .l    */
(2+	    /* 00101100 format 2    spr .b    */
(2*16)),    /* 00101101 format 2    spr .w    */
(8+	    /* 00101110 format 8	      */
(2*16)),    /* 00101111 format 2    spr .l    */
(4+	    /* 00110000 format 4    subc .b   */
(4*16)),    /* 00110001 format 4    subc .w   */
(1+	    /* 00110010 format 1    rxp	      */
(4*16)),    /* 00110011 format 4    subc .l   */
(4+	    /* 00110100 format 4    tbit .b   */
(4*16)),    /* 00110101 format 4    tbit .w   */
(15+	    /* 00110110 format 15	      */
(4*16)),    /* 00110111 format 4    tbit .l   */
(4+	    /* 00111000 format 4    xor .b    */
(4*16)),    /* 00111001 format 4    xor .w    */
(0+	    /* 00111010 format 0    bcc	      */
(4*16)),    /* 00111011 format 4    xor .l    */
(2+	    /* 00111100 format 2    scond .b  */
(2*16)),    /* 00111101 format 2    scond .w  */
(9+	    /* 00111110 format 9	      */
(2*16)),    /* 00111111 format 2    scond .l  */
(4+	    /* 01000000 format 4    add .b    */
(4*16)),    /* 01000001 format 4    add .w    */
(1+	    /* 01000010 format 1    rett      */
(4*16)),    /* 01000011 format 4    add .l    */
(4+	    /* 01000100 format 4    cmp .b    */
(4*16)),    /* 01000101 format 4    cmp .w    */
(13+	    /* 01000110 format 19   undefined */
(4*16)),    /* 01000111 format 4    cmp .l    */
(4+	    /* 01001000 format 4    bic .b    */
(4*16)),    /* 01001001 format 4    bic .w    */
(0+	    /* 01001010 format 0    bhi	      */
(4*16)),    /* 01001011 format 4    bic .l    */
(2+	    /* 01001100 format 2    acb .b    */
(2*16)),    /* 01001101 format 2    acb .w    */
(6+	    /* 01001110 format 6	      */
(2*16)),    /* 01001111 format 2    acb .l    */
(4+	    /* 01010000 format 4    addc .b   */
(4*16)),    /* 01010001 format 4    addc .w   */
(1+	    /* 01010010 format 1    reti      */
(4*16)),    /* 01010011 format 4    addc .l   */
(4+	    /* 01010100 format 4    mov .b    */
(4*16)),    /* 01010101 format 4    mov .w    */
(15+	    /* 01010110 format 15	      */
(4*16)),    /* 01010111 format 4    mov .l    */
(4+	    /* 01011000 format 4    or .b     */
(4*16)),    /* 01011001 format 4    or .w     */
(0+	    /* 01011010 format 0    bis	      */
(4*16)),    /* 01011011 format 4    or .l     */
(2+	    /* 01011100 format 2    movq .b   */
(2*16)),    /* 01011101 format 2    movq .w   */
(13+	    /* 01011110 format 16   undefined */
(2*16)),    /* 01011111 format 2    movq .l   */
(4+	    /* 01100000 format 4    sub .b    */
(4*16)),    /* 01100001 format 4    sub .w    */
(1+	    /* 01100010 format 1    save      */
(4*16)),    /* 01100011 format 4    sub .l    */
(4+	    /* 01100100 format 4    addr .b   */
(4*16)),    /* 01100101 format 4    addr .w   */
(13+	    /* 01100110 format 19   undefined */
(4*16)),    /* 01100111 format 4    addr .l   */
(4+	    /* 01101000 format 4    and .b    */
(4*16)),    /* 01101001 format 4    and .w    */
(0+	    /* 01101010 format 0    bgt	      */
(4*16)),    /* 01101011 format 4    and .l    */
(2+	    /* 01101100 format 2    lpr .b    */
(2*16)),    /* 01101101 format 2    lpr .w    */
(8+	    /* 01101110 format 8	      */
(2*16)),    /* 01101111 format 2    lpr .l    */
(4+	    /* 01110000 format 4    subc .b   */
(4*16)),    /* 01110001 format 4    subc .w   */
(1+	    /* 01110010 format 1    restore   */
(4*16)),    /* 01110011 format 4    subc .l   */
(4+	    /* 01110100 format 4    tbit .b   */
(4*16)),    /* 01110101 format 4    tbit .w   */
(15+	    /* 01110110 format 15	      */
(4*16)),    /* 01110111 format 4    tbit .l   */
(4+	    /* 01111000 format 4    xor .b    */
(4*16)),    /* 01111001 format 4    xor .w    */
(0+	    /* 01111010 format 0    ble	      */
(4*16)),    /* 01111011 format 4    xor .l    */
(3+	    /* 01111100 format 3	      */
(3*16)),    /* 01111101 format 3	      */
(13+	    /* 01111110 format 10   undefined */
(3*16)),    /* 01111111 format 3	      */
(4+	    /* 10000000 format 4    add .b    */
(4*16)),    /* 10000001 format 4    add .w    */
(1+	    /* 10000010 format 1    enter     */
(4*16)),    /* 10000011 format 4    add .l    */
(4+	    /* 10000100 format 4    cmp .b    */
(4*16)),    /* 10000101 format 4    cmp .w    */
(13+	    /* 10000110 format 19   undefined */
(4*16)),    /* 10000111 format 4    cmp .l    */
(4+	    /* 10001000 format 4    bic .b    */
(4*16)),    /* 10001001 format 4    bic .w    */
(0+	    /* 10001010 format 0    bfs	      */
(4*16)),    /* 10001011 format 4    bic .l    */
(2+	    /* 10001100 format 2    addq .b   */
(2*16)),    /* 10001101 format 2    addq .w   */
(13+	    /* 10001110 format 18   undefined */
(2*16)),    /* 10001111 format 2    addq .l   */
(4+	    /* 10010000 format 4    addc .b   */
(4*16)),    /* 10010001 format 4    addc .w   */
(1+	    /* 10010010 format 1    exit      */
(4*16)),    /* 10010011 format 4    addc .l   */
(4+	    /* 10010100 format 4    mov .b    */
(4*16)),    /* 10010101 format 4    mov .w    */
(15+	    /* 10010110 format 15	      */
(4*16)),    /* 10010111 format 4    mov .l    */
(4+	    /* 10011000 format 4    or .b     */
(4*16)),    /* 10011001 format 4    or .w     */
(0+	    /* 10011010 format 0    bfc	      */
(4*16)),    /* 10011011 format 4    or .l     */
(2+	    /* 10011100 format 2    cmpq .b   */
(2*16)),    /* 10011101 format 2    cmpq .w   */
(13+	    /* 10011110 format 13   undefined */
(2*16)),    /* 10011111 format 2    cmpq .l   */
(4+	    /* 10100000 format 4    sub .b    */
(4*16)),    /* 10100001 format 4    sub .w    */
(10+	    /* 10100010 format 1    nop	      */
(4*16)),    /* 10100011 format 4    sub .l    */
(4+	    /* 10100100 format 4    addr .b   */
(4*16)),    /* 10100101 format 4    addr .w   */
(13+	    /* 10100110 format 19   undefined */
(4*16)),    /* 10100111 format 4    addr .l   */
(4+	    /* 10101000 format 4    and .b    */
(4*16)),    /* 10101001 format 4    and .w    */
(0+	    /* 10101010 format 0    blo	      */
(4*16)),    /* 10101011 format 4    and .l    */
(2+	    /* 10101100 format 2    spr .b    */
(2*16)),    /* 10101101 format 2    spr .w    */
(8+	    /* 10101110 format 8	      */
(2*16)),    /* 10101111 format 2    spr .l    */
(4+	    /* 10110000 format 4    subc .b   */
(4*16)),    /* 10110001 format 4    subc .w   */
(1+	    /* 10110010 format 1    wait      */
(4*16)),    /* 10110011 format 4    subc .l   */
(4+	    /* 10110100 format 4    tbit .b   */
(4*16)),    /* 10110101 format 4    tbit .w   */
(15+	    /* 10110110 format 15	      */
(4*16)),    /* 10110111 format 4    tbit .l   */
(4+	    /* 10111000 format 4    xor .b    */
(4*16)),    /* 10111001 format 4    xor .w    */
(0+	    /* 10111010 format 0    bhs	      */
(4*16)),    /* 10111011 format 4    xor .l    */
(2+	    /* 10111100 format 2    scond .b  */
(2*16)),    /* 10111101 format 2    scond .w  */
(11+	    /* 10111110 format 11	      */
(2*16)),    /* 10111111 format 2    scond .l  */
(4+	    /* 11000000 format 4    add .b    */
(4*16)),    /* 11000001 format 4    add .w    */
(1+	    /* 11000010 format 1    dia	      */
(4*16)),    /* 11000011 format 4    add .l    */
(4+	    /* 11000100 format 4    cmp .b    */
(4*16)),    /* 11000101 format 4    cmp .w    */
(13+	    /* 11000110 format 19   undefined */
(4*16)),    /* 11000111 format 4    cmp .l    */
(4+	    /* 11001000 format 4    bic .b    */
(4*16)),    /* 11001001 format 4    bic .w    */
(0+	    /* 11001010 format 0    blt	      */
(4*16)),    /* 11001011 format 4    bic .l    */
(2+	    /* 11001100 format 2    acb .b    */
(2*16)),    /* 11001101 format 2    acb .w    */
(7+	    /* 11001110 format 7	      */
(2*16)),    /* 11001111 format 2    acb .l    */
(4+	    /* 11010000 format 4    addc .b   */
(4*16)),    /* 11010001 format 4    addc .w   */
(1+	    /* 11010010 format 1    flag      */
(4*16)),    /* 11010011 format 4    addc .l   */
(4+	    /* 11010100 format 4    mov .b    */
(4*16)),    /* 11010101 format 4    mov .w    */
(15+	    /* 11010110 format 15	      */
(4*16)),    /* 11010111 format 4    mov .l    */
(4+	    /* 11011000 format 4    or .b     */
(4*16)),    /* 11011001 format 4    or .w     */
(0+	    /* 11011010 format 0    bqt	      */
(4*16)),    /* 11011011 format 4    or .l     */
(2+	    /* 11011100 format 2    movq .b   */
(2*16)),    /* 11011101 format 2    movq .w   */
(13+	    /* 11011110 format 17   undefined */
(2*16)),    /* 11011111 format 2    movq .l   */
(4+	    /* 11100000 format 4    sub .b    */
(4*16)),    /* 11100001 format 4    sub .w    */
(1+	    /* 11100010 format 1    svc	      */
(4*16)),    /* 11100011 format 4    sub .l    */
(4+	    /* 11100100 format 4    addr .b   */
(4*16)),    /* 11100101 format 4    addr .w   */
(13+	    /* 11100110 format 19   undefined */
(4*16)),    /* 11100111 format 4    addr .l   */
(4+	    /* 11101000 format 4    and .b    */
(4*16)),    /* 11101001 format 4    and .w    */
(0+	    /* 11101010 format 0    b	      */
(4*16)),    /* 11101011 format 4    and .l    */
(2+	    /* 11101100 format 2    lpr .b    */
(2*16)),    /* 11101101 format 2    lpr .w    */
(8+	    /* 11101110 format 8	      */
(2*16)),    /* 11101111 format 2    lpr .l    */
(4+	    /* 11110000 format 4    subc .b   */
(4*16)),    /* 11110001 format 4    subc .w   */
(1+	    /* 11110010 format 1    bpt	      */
(4*16)),    /* 11110011 format 4    subc .l   */
(4+	    /* 11110100 format 4    tbit .b   */
(4*16)),    /* 11110101 format 4    tbit .w   */
(15+	    /* 11110110 format 15	      */
(4*16)),    /* 11110111 format 4    tbit .l   */
(4+	    /* 11111000 format 4    xor .b    */
(4*16)),    /* 11111001 format 4    xor .w    */
(10+	    /* 11111010 format 0    nop	      */
(4*16)),    /* 11111011 format 4    xor .l    */
(13+	    /* 11111100 format 3    undefined */
(13*16)),   /* 11111101 format 3    undefined */
((12+	    /* 11111110 format 12   new 532   */
13*16)) };  /* 11111111 format 3    undefined */

STATIC
char fmt1_table[16][8] = {
    "bsr",	/* branch to subroutine */
    "ret",	/* return from subroutine */
    "cxp",	/* call external procedure */
    "rxp",	/* return from external procedure */
    "rett",	/* return from trap */
    "reti",	/* return from interrupt */
    "save",	/* save general purpose registers */
    "restore",	/* restore general purpose registers */
    "enter",	/* enter new procedure context */
    "exit",	/* exit procedure context */
    "nop",	/* no operation */
    "wait",	/* wait fro interrupt */
    "dia",	/* diagnose */
    "flag",	/* flag trap */
    "svc",	/* supervisor call trap */
    "bpt"	/* breakpoint trap */
};

#define FMT1_BSR    0x0 /* branch to subroutine */
#define FMT1_RET    0x1 /* return from subroutine */
#define FMT1_CXP    0x2 /* call external procedure */
#define FMT1_RXP    0x3 /* return from external procedure */
#define FMT1_RETT   0x4 /* return from trap */
#define FMT1_RETI   0x5 /* return from interrupt */
#define FMT1_SAVE   0x6 /* save general purpose registers */
#define FMT1_RESTORE	0x7 /* restore general purpose registers */
#define FMT1_ENTER  0x8 /* enter new procedure context */
#define FMT1_EXIT   0x9 /* exit procedure context */
#define FMT1_NOP    0xa /* no operation */
#define FMT1_WAIT   0xb /* wait fro interrupt */
#define FMT1_DIA    0xc /* diagnose */
#define FMT1_FLAG   0xd /* flag trap */
#define FMT1_SVC    0xe /* supervisor call trap */
#define FMT1_BPT    0xf /* breakpoint trap */

STATIC
char fmt2_table[7][5] = {
    "addq",	/* add quick */
    "cmpq",	/* compare quick */
    "spr",	/* save processor register */
    "s",	/* save condition as boolean */
    "acb",	/* add, compare and branch */
    "movq",	/* move quick */
    "lpr"	/* load processor register */
};

#define FMT2_ADDQ   0x0 /* add quick */
#define FMT2_CMPQ   0x1 /* compare quick */
#define FMT2_SPR    0x2 /* store processr oregister */
#define FMT2_SCOND  0x3 /* save condition as boolean */
#define FMT2_ACB    0x4 /* add, compare and branch */
#define FMT2_MOVQ   0x5 /* move quick */
#define FMT2_LPR    0x6 /* load processor register */

STATIC
char fmt3_table[8][7] = {
    "cxpd",	/* call external procedure with descriptor */
    "bicpsr",	/* bit clear in PSR */
    "jump",	/* jump */
    "bispsr",	/* bit set in PSR */
    "??3??",	/* UNDEFINED */
    "adjsp",	/* adjust stack pointer */
    "jsr",	/* jump to subroutine */
    "case"	/* case branch */
};

#define FMT3_CXPD   0x0 /* call external procedure with descriptor */
#define FMT3_BICPSR 0x1 /* bit clear in PSR */
#define FMT3_JUMP   0x2 /* jump */
#define FMT3_BISPSR 0x3 /* bit set in PSR */
#define FMT3_UNDEF  0x4 /* UNDEFINED */
#define FMT3_ADJSP  0x5 /* adjust stack pointer */
#define FMT3_JSR    0x6 /* jump to subroutine */
#define FMT3_CASE   0x7 /* case branch */

STATIC
char fmt4_table[16][5] = {
    "add",	/* add */
    "cmp",	/* compare */
    "bic",	/* bit clear */
    "?4?",	/* UNDEFINED */
    "addc",	/* add with carry */
    "mov",	/* move */
    "or",	/* or */
    "?4?",	/* UNDEFINED */
    "sub",	/* subtract */
    "addr",	/* compute effective address */
    "and",	/* and */
    "?4?",	/* UNDEFINED */
    "subc",	/* subtract with carry */
    "tbit",	/* test bit */
    "xor",	/* exclusive or */
    "?4?"	/* UNDEFINED */
};

#define FMT4_ADD    0x0 /* add */
#define FMT4_CMP    0x1 /* compare */
#define FMT4_BIC    0x2 /* bit clear */
#define FMT4_ADDC   0x4 /* add with carry */
#define FMT4_MOV    0x5 /* move */
#define FMT4_OR	    0x6 /* or */
#define FMT4_SUB    0x8 /* subtract */
#define FMT4_ADDR   0x9 /* compute effective address */
#define FMT4_AND    0xa /* and */
#define FMT4_SUBC   0xc /* subtract with carry */
#define FMT4_TBIT   0xd /* test bit */
#define FMT4_XOR    0xe /* exclusive or */

STATIC
char fmt5_table[4][7] = {
    "movs",	/* move string */
    "cmps",	/* compare string */
    "setcfg",	/* set configuration register */
    "skps"	/* skip string */
};

#define FMT5_MOVS   0x0 /* move string */
#define FMT5_CMPS   0x1 /* compare string */
#define FMT5_SETCFG 0x2 /* set configuration register */
#define FMT5_SKPS   0x3 /* skip string */

STATIC
char fmt6_table[16][6] = {
    "rot",	/* rotate */
    "ash",	/* arithmetic shift */
    "cbit",	/* clear bit */
    "cbiti",	/* clear bit interlocked */
    "??6??",	/* undefined */
    "lsh",	/* logical shift */
    "sbit",	/* set bit */
    "sbiti",	/* set bit interlocked */
    "neg",	/* negate */
    "not",	/* not */
    "??6??",	/* undefined */
    "subp",	/* subtract packed decimal */
    "abs",	/* absolute value */
    "com",	/* complement */
    "ibit",	/* invert bit */
    "addp"	/* add packed decimal */
};

#define FMT6_ROT    0x0 /* rotate */
#define FMT6_ASH    0x1 /* arithmetic shift */
#define FMT6_CBIT   0x2 /* clear bit */
#define FMT6_CBITI  0x3 /* clear bit interlocked */
#define FMT6_UNDEF1 0x4 /* undefined */
#define FMT6_LSH    0x5 /* logical shift */
#define FMT6_SBIT   0x6 /* s#define FMT6_NOT	0x9 /* not */
#define FMT6_UNDEF2 0xa /* undefined */
#define FMT6_SUBP   0xb /* subtract packed decimal */
#define FMT6_ABS    0xc /* absolute value */
#define FMT6_COM    0xd /* complement */
#define FMT6_IBIT   0xe /* invert bit */
#define FMT6_ADDP   0xf /* add packed decimal */

STATIC
char fmt7_table[16][7] = {
    "movm",	/* move multiple */
    "cmpm",	/* compare multiple */
    "inss",	/* insert field short */
    "exts",	/* extract field short */
    "movxb",	    /* move with sign-extention byte to word */
    "movzb",	    /* move with zero-extention byte to word */
    "movz",	    /* move with zero extention i to double */
    "movx",	    /* move with sign-extention i to double */
    "mul",	/* multiply */
    "mei",	/* multiply extended integer */
    "?7?",	/* undefined */
    "dei",	/* divide extended integer */
    "quo",	/* quotient */
    "rem",	/* remainder */
    "mod",	/* modulus */
    "div"	/* divide */
};

#define FMT7_MOVM   0x0 /* move multiple */
#define FMT7_CMPM   0x1 /* compare multiple */
#define FMT7_INSS   0x2 /* insert field short */
#define FMT7_EXTS   0x3 /* extract field short */
#define FMT7_MOVXBW	0x4	  /* move with sign-extention byte to word */
#define FMT7_MOVZBW	0x5	  /* move with zero-extention byte to word */
#define FMT7_MOVZD	0x6	  /* move with zero extention i to double */
#define FMT7_MOVXD  0x7 /* move with sign-extention i to double */
#define FMT7_MUL    0x8 /* multiply */
#define FMT7_MEI    0x9 /* multiply extended integer */
#define FMT7_UNDEF  0xa /* undefined */
#define FMT7_DEI    0xb /* divide extended integer */
#define FMT7_QUO    0xc /* quotient */
#define FMT7_REM    0xd /* remainder */
#define FMT7_MOD    0xe /* modulus */
#define FMT7_DIV    0xf /* divide */

STATIC
char fmt8_table[8][6] = {
    "ext",	/* extract field */
    "cvtp",	/* convert to bit pointer */
    "ins",	/* insert field */
    "check",	/* bounds check */
    "index",	/* calculate index */
    "ffs",	/* find first set bit */
    "mov",	/* move supervisor to/from user space */
    "??8??"	/* undefined */
};

#define FMT8_EXT    0x0 /* extract field */
#define FMT8_CVTP   0x1 /* convert to bit pointer */
#define FMT8_INS    0x2 /* insert field */
#define FMT8_CHECK  0x3 /* bounds check */
#define FMT8_INDEX  0x4 /* calculate index */
#define FMT8_FFS    0x5 /* find first set bit */
#define FMT8_MOV    0x6 /* move supervisor to/from user space */
#define FMT8_UNDEF  0x7 /* undefined */

STATIC
char fmt9_table[8][6] = {
    "mov",	/* move converting integer to floating point */
    "lfsr",	/* load floating-point status register */
    "movlf",	/* move long floating to floating */
    "movfl",	/* move floating to long floating */
    "round",	/* round floating to integer */
    "trunc",	/* truncate floating to integer */
    "sfsr",	/* store floating-point status register */
    "floor"	/* floor floating to integer */
};

#define FMT9_MOV    0x0 /* move converting integer to floating point */
#define FMT9_LFSR   0x1 /* load floating-point status register */
#define FMT9_MOVLF  0x2 /* move long floating to floating */
#define FMT9_MOVFL  0x3 /* move floating to long floating */
#define FMT9_ROUND  0x4 /* round floating to integer */
#define FMT9_TRUNC  0x5 /* truncate floating to integer */
#define FMT9_SFSR   0x6 /* store floating-point status register */
#define FMT9_FLOOR	0x7	/* floor floating to integer */

#define NOP		0xff;	/* catch all nop instruction */

STATIC
char fmt11_table[16][4] = {
    "add",	/* add floating */
    "mov",	/* move floating */
    "cmp",	/* compare floating */
    "?f?",	/* undefined */
    "sub",	/* subtract floating */
    "neg",	/* negate floating */
    "?f?",	/* undefined */
    "?f?",	/* undefined */
    "div",	/* divide floating */
    "?f?",	/* undefined */
    "?f?",	/* undefined */
    "?f?",	/* undefined */
    "mul",	/* multiply floating */
    "abs",	/* absolute value floating */
    "?f?",	/* undefined */
    "?f?"	/* undefined */
};

STATIC
char fmt12_table[16][6] = {
    "?f?",	/* 0 undefined */
    "?f?",	/* 1 undefined */
    "poly",	/* 2 */
    "dot",	/* 3 */
    "scalb",	/* 4 */
    "logb",	/* 5 */
    "?f?",	/* 6 undefined */
    "?f?",	/* 7 undefined */
    "?f?",	/* 8 undefined */
    "?f?",	/* 9 undefined */
    "?f?",	/* 10 undefined */
    "?f?",	/* 11 undefined */
    "?f?",	/* 12 undefined */
    "?f?",	/* 13 undefined */
    "?f?",	/* 14 undefined */
    "?f?",	/* 15 undefined */
};

#define FMT11_ADD   0x0 /* add floating */
#define FMT11_MOV   0x1 /* move floating */
#define FMT11_CMP   0x2 /* compare floating */
#define FMT11_UNDEF1	0x3 /* undefined */
#define FMT11_SUB   0x4 /* subtract floating */
#define FMT11_NEG   0x5 /* negate floating */
#define FMT11_UNDEF2	0x6 /* undefined */
#define FMT11_UNDEF3	0x7 /* undefined */
#define FMT11_DIV   0x8 /* divide floating */
#define FMT11_UNDEF4	0x9 /* undefined */
#define FMT11_UNDEF5	0xa /* undefined */
#define FMT11_UNDEF6	0xb /* undefined */
#define FMT11_MUL   0xc /* multiply floating */
#define FMT11_ABS   0xd /* absolute value floating */
#define FMT11_UNDEF7	0xe /* undefined */
#define FMT11_UNDEF8	0xf /* undefined */

#define FMT12_POLY	2
#define FMT12_DOT	3
#define FMT12_SCALB	4
#define FMT12_LOGB	5

STATIC
char fmt14_table[][6] = {
    "rdval",	/* validate address for reading */
    "wrval",	/* validate address for writing */
    "lmr",	/* load memory managemnet register */
    "smr",	/* store memory management register */
    "???",
    "???",
    "???",
    "???",
    "???",
    "cinv",
};

#define FMT14_RDVAL 0x0 /* validate address for reading */
#define FMT14_WRVAL 0x1 /* validate address for writing */
#define FMT14_LMR   0x2 /* load memory managemnet register */
#define FMT14_SMR   0x3 /* store memory management register */
#define FMT14_CINV  0x9 /* cache invalidate */

/*
 * These are indices into regTable.  Keep in sync!
 */
#define REG_R0		0			/* General Register 0 */
#define REG_F0		8			/* Floating Point  0 */
#define REG_PC		32			/* Program counter */
#define REG_USP		(REG_PC+1)
#define REG_ISP		(REG_PC+2)
#define REG_FP		(REG_PC+3)
#define REG_SB		(REG_PC+4)
#define REG_INTBASE	(REG_PC+5)
#define REG_MOD		(REG_PC+6)
#define REG_PSR		(REG_PC+7)
#define REG_UPSR	(REG_PC+8)
#define REG_DCR		(REG_PC+9)
#define REG_DSR		(REG_PC+10)
#define REG_CAR		(REG_PC+11)
#define REG_BPC		(REG_PC+12)
#define REG_CFG		(REG_PC+13)

#define REG_PTB0	46
#define REG_PTB1	(REG_PTB0+1)
#define REG_IVAR0	(REG_PTB0+2)
#define REG_IVAR1	(REG_PTB0+3)
#define REG_TEAR	(REG_PTB0+4)
#define REG_MCR		(REG_PTB0+5)
#define REG_MSR		(REG_PTB0+6)

#define REG_FSR		   53
#define REG_SP		(REG_FSR+1)
#define REG_NIL		(REG_FSR+2)

struct regTable {
    char *name;
};

STATIC
struct regTable regTable[] = {
  {"r0"},			/* General Register 0 */
  {"r1"},			/* General Register 1 */
  {"r2"},			/* General Register 2 */
  {"r3"},			/* General Register 3 */
  {"r4"},			/* General Register 4 */
  {"r5"},			/* General Register 5 */
  {"r6"},			/* General Register 6 */
  {"r7"},			/* General Register 7 */
  {"f0"},			/* Floating Point  0 */
  {"f1"},			/* Floating Point  1 */
  {"f2"},			/* Floating Point  2 */
  {"f3"},			/* Floating Point  3 */
  {"f4"},			/* Floating Point  4 */
  {"f5"},			/* Floating Point  5 */
  {"f6"},			/* Floating Point  6 */
  {"f7"},			/* Floating Point  7 */
  {"l1l"},			/* Floating Point L1 */
  {"l1h"},			/* Floating Point L1 */
  {"l3l"},			/* Floating Point L3 */
  {"l3h"},			/* Floating Point L3 */
  {"l5l"},			/* Floating Point L5 */
  {"l5h"},			/* Floating Point L5 */
  {"l7l"},			/* Floating Point L7 */
  {"l7h"},			/* Floating Point L7 */
  {"l0"},
  {"l1"},
  {"l2"},
  {"l3"},
  {"l4"},
  {"l5"},
  {"l6"},
  {"l7"},
  {"pc"},			/* Program counter */
  {"usp"},			/* 532 */
  {"isp"},			/* 532 */
  {"fp"},			/* Frame Pointer */
  {"sb"},			/* Static Base */
  {"intbase"},			/* Interrupt Base */
  {"mod"},			/* Module Register */
  {"psr"},			/* Processor Status */
  {"upsr"},			/* Processor Status */
  {"dcr"},			/* 532 */
  {"dsr"},			/* 532 */
  {"car"},			/* 532 */
  {"bpc"},			/* 532 */
  {"cfg"},			/* 532 */
  {"ptb0"},			/* Page Table Base 0 */
  {"ptb1"},			/* Page Table Base 1 */
  {"ivar0"},			/* 532 */
  {"ivar1"},			/* 532 */
  {"tear"},			/* 532 */
  {"mcr"},			/* 532 */
  {"msr"},			/* Memory Management Status */
  {"fsr"},			/* Floating Point Status */
  {"sp"},			/* x(x(sp)) adr mode */
  {"???"},			/* unknown reg */
};

#define REGTABLESZ ((sizeof regTable) / (sizeof (struct regTable)))
    
#define GetShort(x, y)	   ((((x) & 0x80) >> 7) + (((y) & 0x7) << 1))
#define GetGen0(x)	   (((x) & 0xf8) >> 3)
#define GetGen1(x,y)	   ((((x) & 0xc0) >> 6) + (((y) & 0x7) << 2))
#define GetGenSI(x)	  (int)((((x) & 0x1c) == 0x1c) ? (((x) & 0x3) + 1) : 0)
#define ScaledFields(input, reg, gen) \
	    ((reg) = ((input) & 0x7), \
	     (gen) = (((input) & 0xf8) >> 3))

STATIC unsigned char cpuRegTable [] = {
    REG_UPSR,  REG_DCR,	REG_BPC,     REG_DSR,
    REG_CAR,   REG_NIL,	REG_NIL,     REG_NIL,
    REG_FP,    REG_SP,	REG_SB,	     REG_USP,
    REG_CFG,   REG_PSR,	REG_INTBASE, REG_MOD,
};

STATIC unsigned char mmuRegTable [] = {
    REG_NIL,   REG_NIL,	REG_NIL,     REG_NIL,
    REG_NIL,   REG_NIL,	REG_NIL,     REG_NIL,
    REG_NIL,   REG_MCR,	REG_MSR,     REG_TEAR,
    REG_PTB0,  REG_PTB1,REG_IVAR0,   REG_IVAR1,
};

void db_reverseBits();

#define get_byte(l,t) ((unsigned char) db_get_task_value(l, 1, FALSE, t))

void db_formatOperand(operand, loc, task)
	struct operand *operand;
	db_addr_t loc;
	task_t task;
{
	int need_comma, i, mask, textlen;
	
	switch (operand->o_mode) {
		
	      case AMODE_REG:
	      case AMODE_AREG:
	      case AMODE_MREG:
		db_printf("%s", regTable[operand->o_reg0].name);
		break;
		
	      case AMODE_MREL:
		db_task_printsym((db_addr_t) operand->o_disp1,
				 DB_STGY_ANY, task);
		db_printf("(");
		db_task_printsym((db_addr_t) operand->o_disp0,
				 DB_STGY_ANY, task);
		db_printf("(%s))",regTable[operand->o_reg0].name);
		break;
		
	      case AMODE_QUICK:
	      case AMODE_IMM:
		db_task_printsym((db_addr_t) operand->o_disp0,
				 DB_STGY_ANY, task);
		break;
		
	      case AMODE_ABS:
		db_printf("@");
		db_task_printsym((db_addr_t) operand->o_disp0,
				 DB_STGY_ANY, task);
		break;
		
	      case AMODE_EXT:
		db_printf("ext(%ld)", operand->o_disp0);
		if (operand->o_disp1)
		    db_printf("+%ld", operand->o_disp1);
		break;
		
	      case AMODE_TOS:
		db_printf("tos");
		break;
		
	      case AMODE_RREL:
	      case AMODE_MSPC:
		db_task_printsym((db_addr_t) operand->o_disp0,
				 DB_STGY_XTRN, task);
		db_printf("(%s)",regTable[operand->o_reg0].name);
		break;
		
	      case AMODE_REGLIST:
		db_printf("[");
		need_comma = 0;
		for (i = 0; i < 8; i++) {
			mask = (1<<i);
			if (operand->o_reg0&mask) {
				if (need_comma)
				    db_printf(",");
				db_printf("r%d", i);
				need_comma = 1;
			}
		}
		db_printf("]");
		break;
		
	      case AMODE_BLISTB:
		i = 7;
		goto bitlist;
		
	      case AMODE_BLISTW:
		i = 15;
		goto bitlist;
		
	      case AMODE_BLISTD:
		i = 31;
		
bitlist:
		db_printf("B'");
		for (; i >= 0; i--, textlen++) {
			mask = (1<<i);
			if (operand->o_disp0&mask)
			    db_printf("1");
			else
			    db_printf("0");
		}
		break;
		
	      case AMODE_SOPT:
		i = operand->o_disp0>>1;
		db_printf("%s", sopt_table[i]);
		break;
		
	      case AMODE_CFG:
		db_printf("[");
		need_comma = 0;
		for (i = 0;i < 3;i++) {
			mask = 1<<i;
			if (operand->o_disp0&mask) {
				if (need_comma)
				    db_printf(",");
				db_printf("%s",cfg_table[i]);
				need_comma = 1;
			}
		}
		db_printf("]");
		break;
		
	      case AMODE_CINV:
		{			/* print cache invalidate flags */
			char *p;
			int i, need_comma = 0;
			
			for (i = 4, p = "AID"; i; i >>= 1, ++p)
			    if (i & operand->o_disp0) {
				    if (need_comma)
					db_printf(",");
				    db_printf("%c",*p);
				    need_comma = 1;
			    }
		}
		break;
		
	      case AMODE_INVALID:
		db_printf("?");
		break;
	}
	if (operand->o_iscale) {
		db_printf("[r%d:", operand->o_ireg);
		db_printf("%s]",scale_table[operand->o_iscale]);
	}
}

void db_formatAsm(insn, loc, task, altfmt)
	struct insn *insn;
	db_addr_t loc;
	task_t task;
	boolean_t altfmt;
{
	int i, j;
	
	db_printf("%s\t", &insn->i_monic[0]);

	for (i = 0; i < 4 && insn->i_opr[i].o_mode != AMODE_NONE; i++) {
		if (i != 0) {
			db_printf(",");
		}
		db_formatOperand(&insn->i_opr[i], loc, task);
	}
	j = 0;
	for (i = 0; i < 4 && insn->i_opr[i].o_mode != AMODE_NONE; i++) {
		if (insn->i_opr[i].o_mode == AMODE_MSPC ||
		    insn->i_opr[i].o_mode == AMODE_RREL) {
			register struct db_variable *regp;
			db_expr_t	value;
			
			if (strcmp(db_regs->name, "pc") == 0) {
				value = loc;
			} else {
				if (!altfmt) {
					continue;
				}
				for (regp = db_regs; regp < db_eregs; regp++) {
					if (strcmp(regp->name,
						   regTable[insn->
							    i_opr[i].o_reg0].
						   name) == 0) {
						break;
					}
				}
				db_read_write_variable(regp, &value,
						       DB_VAR_GET, 0);
			}
			if (j != 0) {
				db_printf(",");
			} else {
				db_printf("\t<");
			}
			db_task_printsym((db_addr_t)
					 insn->i_opr[i].o_disp0 + value,
					 DB_STGY_XTRN, task);
			j++;
		}
	}
	if (j != 0) {
		db_printf(">");
	}
}

void db_initInsn (insn)
	struct insn *insn;
{
	insn->i_opr[0].o_mode = AMODE_NONE;
	insn->i_opr[0].o_iscale = 0;
	insn->i_opr[1].o_mode = AMODE_NONE;
	insn->i_opr[1].o_iscale = 0;
	insn->i_opr[2].o_mode = AMODE_NONE;
	insn->i_opr[2].o_iscale = 0;
	insn->i_opr[3].o_mode = AMODE_NONE;
	insn->i_opr[3].o_iscale = 0;
}

int db_disp(loc, result, task)
	db_addr_t loc;
	long *result;
	task_t task;
{
	unsigned int b;
	
	b = get_byte(loc, task);
	if (!(b & 0x80)) {			/* one byte */
		*result = ((b & 0x40) ? 0xffffffc0L : 0) | (b & 0x3f);
		return(1);
	} else if (!(b & 0x40)) {		/* two byte */
		*result =
		    ((b & 0x20) ? 0xffffe000L : 0) | ((b & 0x1f) << 8);
		b = get_byte(loc + 1, task);
		*result |= b;
		return 2;
	} else {					/* four byte */
		*result = 
		    ((b & 0x20) ? 0xe0000000L : 0) |	/* bug fix 8/28 */
			((b & 0x1f) << 24);		/* bug fix 7/21 */
		b = get_byte(loc + 1, task);
		*result |= (b << 16);
		b = get_byte(loc + 2, task);
		*result |= (b << 8);
		b = get_byte(loc + 3, task);
		*result |= b;
		return(4);
	}
}

int db_decode_operand(loc, byte, operand, iol, task)
	db_addr_t loc;
	unsigned char byte;
	struct operand *operand;
	unsigned char iol;
	task_t task;
{
	register int i, consumed = 0;
	unsigned long value;
	
	switch (byte) {
		
	      case GEN_R0:
	      case GEN_R1:
	      case GEN_R2:
	      case GEN_R3:
	      case GEN_R4:
	      case GEN_R5:
	      case GEN_R6:
	      case GEN_R7:
		operand->o_mode = AMODE_REG;
		operand->o_reg0 = REG_R0 + byte;
		break;
		
	      case GEN_RR0:
	      case GEN_RR1:
	      case GEN_RR2:
	      case GEN_RR3:
	      case GEN_RR4:
	      case GEN_RR5:
	      case GEN_RR6:
	      case GEN_RR7:
		operand->o_mode = AMODE_RREL;
		operand->o_reg0 = REG_R0 + (byte&0x7);
		goto one_disp;
		
	      case GEN_FRMR:
		operand->o_reg0 = REG_FP;
		operand->o_mode = AMODE_MREL;
		goto two_disp;
		
	      case GEN_SPMR:
		operand->o_reg0 = REG_SP;
		operand->o_mode = AMODE_MREL;
		goto two_disp;
		
	      case GEN_SBMR:
		operand->o_reg0 = REG_SB;
		operand->o_mode = AMODE_MREL;
		goto two_disp;
		
	      case GEN_IMM:
		operand->o_mode = AMODE_IMM;
		/* fix to sign extend */
		value = (get_byte(loc, task) & 0x80)? 0xffffffff: 0;
		for (i = 0; i < iol; i++) {
			value = (value << 8) + get_byte(loc + i, task);
		}
		operand->o_disp0 = value;
		consumed = iol;
		break;
		
	      case GEN_ABS:
		operand->o_mode = AMODE_ABS;
		goto one_disp;
		
	      case GEN_EXT:
		operand->o_mode = AMODE_EXT;
		goto two_disp;
		
	      case GEN_TOS:
		operand->o_mode = AMODE_TOS;
		break;
		
	      case GEN_FRM:
		operand->o_mode = AMODE_MSPC;
		operand->o_reg0 = REG_FP;
		goto one_disp;
		
	      case GEN_SPM:
		operand->o_mode = AMODE_MSPC;
		operand->o_reg0 = REG_SP;
		goto one_disp;
		
	      case GEN_SBM:
		operand->o_mode = AMODE_MSPC;
		operand->o_reg0 = REG_SB;
		goto one_disp;
		
	      case GEN_PCM:
		operand->o_mode = AMODE_MSPC;
		operand->o_reg0 = REG_PC;
		goto one_disp;
		
	      default:
		operand->o_mode = AMODE_INVALID;
		break;
		
two_disp:
		consumed = db_disp(loc, &operand->o_disp0, task);
		consumed += db_disp(loc + consumed, &operand->o_disp1, task);
		break;
	
one_disp:
		consumed = db_disp(loc, &operand->o_disp0, task);
		break;
	}
	return(consumed);
}

int db_gen(insn, loc, mask, byte0, byte1, task)
	struct insn *insn;
	db_addr_t loc;
	int mask;		/* 1 to get gen1, 2 to get gen2 */
	unsigned char byte0, byte1;
	task_t task;
{
	int opr = 0, opr2, consumed = 0;
	unsigned char gen0, gen1;
	
	gen0 = GetGen0(byte1);		/* mask and shift gen fields */
	gen1 = GetGen1(byte0, byte1);	/* gen0 is really 1, gen1 is 2 */
	
	while (insn->i_opr[opr].o_mode != AMODE_NONE)
	    opr++;
	
	if (mask & 0x1) {
		if (insn->i_opr[opr].o_iscale = GetGenSI(gen0)) {
			ScaledFields(get_byte(loc, task),
				     insn->i_opr[opr].o_ireg, gen0);
			consumed++;
		}
		opr2 = opr + 1;
	} else {
		opr2 = opr;
	}
	
	if (mask & 0x2 &&
	    (insn->i_opr[opr2].o_iscale = GetGenSI(gen1))) {
		ScaledFields(get_byte(loc + consumed, task),
			     insn->i_opr[opr2].o_ireg, gen1);
		consumed++;
	}
	
	if (mask & 0x1) {
		consumed += db_decode_operand(loc + consumed, gen0,
					   &insn->i_opr[opr], insn->i_iol,
					   task);
	}
	if (mask & 0x2) {
		consumed += db_decode_operand(loc + consumed, gen1,
					   &insn->i_opr[opr2], insn->i_iol,
					   task);
	}
	return(consumed);
}	

int db_dasm_ns32k(insn, loc, task)
	struct insn *insn;
	db_addr_t loc;			/* start addr of this insn */
	task_t task;
{
	unsigned char byte0, byte1, byte2;
	int i, j;
	int consumed;
	
	insn->i_iol = IOL_NONE;	    /* Don't assume any operand length */
	byte0 = get_byte(loc, task);    /* look at first byte in insn */
	consumed = 1;
	i = byte0 / 2;	    /* get index into fmttab */
	if (byte0 % 2)
	    j = ((fmttab[i] & 0xf0) >> 4);
	else
	    j = (fmttab[i]) & 0x0f;
	
	insn->i_format = j;	    /* set insn type */
	
	switch (j) {
	      case ITYPE_FMT0:
		insn->i_op = FMT0_COND(byte0);	 /* condition code field */
		insn->i_monic[0] = 'b';
		insn->i_monic[1] = cond_table[insn->i_op][0];
		insn->i_monic[2] = cond_table[insn->i_op][1];
		insn->i_monic[3] = '\0';
		insn->i_opr[0].o_mode = AMODE_IMM;	/* MSPC implied */
		insn->i_opr[0].o_reg0 = REG_PC;
		consumed += db_disp(loc + consumed, &insn->i_opr[0].o_disp0,
				 task);
		insn->i_opr[0].o_disp0 += loc;
		break;
		
	      case ITYPE_FMT1:
		insn->i_op = FMT1_OP(byte0);
		strcpy(insn->i_monic, fmt1_table[insn->i_op]);
		switch (insn->i_op) {
		      case FMT1_CXP:
		      case FMT1_RXP:
		      case FMT1_RET:
		      case FMT1_RETT:
			insn->i_opr[0].o_mode = AMODE_IMM;
			consumed += db_disp(loc + consumed,
					 &insn->i_opr[0].o_disp0, task);
			break;
			
		      case FMT1_BSR:
			insn->i_opr[0].o_mode = AMODE_IMM; /* MSPC implied */
			insn->i_opr[0].o_reg0 = REG_PC;
			consumed += db_disp(loc + consumed,
					 &insn->i_opr[0].o_disp0, task);
			insn->i_opr[0].o_disp0 += loc;
			break;
			
		      case FMT1_SAVE:
		      case FMT1_RESTORE:
		      case FMT1_ENTER:
		      case FMT1_EXIT:
			insn->i_opr[0].o_mode = AMODE_REGLIST;
			insn->i_opr[0].o_reg0 = get_byte(loc + consumed, task);
			consumed++;
			if (insn->i_op == FMT1_EXIT ||
			    insn->i_op == FMT1_RESTORE) /* WBC bug fix */
			    db_reverseBits (&(insn->i_opr[0].o_reg0));
			if (insn->i_op == FMT1_ENTER) {
				/* MSPC implied */
				insn->i_opr[1].o_mode = AMODE_IMM;
				insn->i_opr[1].o_reg0 = REG_PC;
				consumed += db_disp(loc + consumed,
						 &insn->i_opr[1].o_disp0,
						 task);
			}
			break;
		}
		break;
		
	      case ITYPE_FMT2:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT2_OP(byte0);
		insn->i_iol = IOL(byte0);
		i = GetShort(byte0, byte1);
		strcpy(insn->i_monic, fmt2_table[insn->i_op]);
		switch (insn->i_op) {
			
		      case FMT2_SCOND:
			strcat(insn->i_monic, cond_table[i]);
			break;
			
		      case FMT2_ADDQ:
		      case FMT2_CMPQ:
		      case FMT2_ACB:
		      case FMT2_MOVQ:
			if (i&0x08) {	    /* negative quick value */
				insn->i_opr[0].o_disp0 = i;
				insn->i_opr[0].o_disp0 |= 0xfffffff0;
			} else {
				insn->i_opr[0].o_disp0 = i;
			}
			insn->i_opr[0].o_mode = AMODE_QUICK;
			break;
			
		      case FMT2_LPR:
		      case FMT2_SPR:
			insn->i_opr[0].o_reg0 = cpuRegTable [i];
			insn->i_opr[0].o_mode = AMODE_AREG;
			break;
		}
		consumed += db_gen(insn, loc + consumed, 0x1, 0, byte1, task);
		strcat(insn->i_monic, iol_table[insn->i_iol]);
		if (insn->i_op == FMT2_ACB) {
			consumed += db_disp(loc + consumed,
					 &insn->i_opr[2].o_disp0, task);
			insn->i_opr[2].o_disp0 += loc;
			insn->i_opr[2].o_mode = AMODE_IMM;
		}
		break;
		
	      case ITYPE_FMT3:
		insn->i_format = ITYPE_FMT3;
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT3_OP(byte1);
		insn->i_iol = IOL(byte0);
		strcpy(insn->i_monic, fmt3_table[insn->i_op]);
		consumed += db_gen(insn, loc + consumed, 0x1, 0, byte1, task);
		switch (insn->i_op) {
			
		      case FMT3_CXPD:
		      case FMT3_JUMP:
		      case FMT3_JSR:
			if (insn->i_iol != IOL_DOUBLE)
			    insn->i_format = ITYPE_UNDEF;
			break;
			
		      case FMT3_BICPSR:
		      case FMT3_BISPSR:
			if (insn->i_iol == IOL_DOUBLE) {
				insn->i_format = ITYPE_UNDEF;
				break;
			}
			if (insn->i_opr[0].o_mode == AMODE_IMM) {
				if (insn->i_iol == IOL_BYTE)
				    insn->i_opr[0].o_mode = AMODE_BLISTB;
				else
				    insn->i_opr[0].o_mode = AMODE_BLISTW;
			}
			/* fall through */
			
		      case FMT3_CASE:
		      case FMT3_ADJSP:
			strcat(insn->i_monic, iol_table[insn->i_iol]);
			break;
			
		      case FMT3_UNDEF:
			insn->i_format = ITYPE_UNDEF;
			break;
			
		}
		break;
		
	      case ITYPE_FMT4:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT4_OP(byte0);
		insn->i_iol = IOL(byte0);
		strcpy(insn->i_monic, fmt4_table[insn->i_op]);
		consumed += db_gen(insn, loc + consumed, 0x3, byte0,
				byte1, task);
		if (insn->i_op == FMT4_ADDR) {
			if (insn->i_iol != IOL_DOUBLE)
			    insn->i_format = ITYPE_UNDEF;
		}
		else
		    strcat(insn->i_monic, iol_table[insn->i_iol]);
		break;
		
	      case ITYPE_FMT5:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT5_OP(byte1);
		if (insn->i_op > FMT5_SKPS) {
			insn->i_format = ITYPE_UNDEF;
			break;
		}
		strcpy(insn->i_monic, fmt5_table[insn->i_op]);
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_opr[0].o_disp0 = GetShort(byte1, byte2);
		insn->i_iol = IOL(byte1);
		switch (insn->i_op) {
			
		      case FMT5_MOVS:
		      case FMT5_CMPS:
		      case FMT5_SKPS:
			if (insn->i_opr[0].o_disp0&0x1)
			    strcat(insn->i_monic, "t");
			else
			    strcat(insn->i_monic,
				   iol_table[insn->i_iol]);
			insn->i_opr[0].o_mode = AMODE_SOPT;
			break;
			
		      case FMT5_SETCFG:
			insn->i_opr[0].o_mode = AMODE_CFG;
			break;
		}
		break;
		
	      case ITYPE_FMT6:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT6_OP(byte1);
		insn->i_iol = IOL(byte1);
		strcpy(insn->i_monic, fmt6_table[insn->i_op]);
		strcat(insn->i_monic, iol_table[insn->i_iol]);
		if (insn->i_op == FMT6_ROT ||
		    insn->i_op == FMT6_ASH ||
		    insn->i_op == FMT6_LSH)
		{
			insn->i_iol = 1; /* shift and rotate special case */
		}
		if (insn->i_op == FMT6_UNDEF1 ||
		    insn->i_op == FMT6_UNDEF2) {
			insn->i_format = ITYPE_UNDEF;
			break;
		}
		consumed += db_gen(insn, loc + consumed, 0x3, byte1, byte2,
				   task);
		break;
		
	      case ITYPE_FMT7:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT7_OP(byte1);
		strcpy(insn->i_monic, fmt7_table[insn->i_op]);
		insn->i_iol = IOL(byte1);
		strcat(insn->i_monic, iol_table[insn->i_iol]);
		consumed += db_gen(insn, loc + consumed, 0x3, byte1, byte2,
				   task);
		switch (insn->i_op) {
			
		      case FMT7_MOVM:
		      case FMT7_CMPM:
			consumed += db_disp(loc + consumed,
					 &insn->i_opr[2].o_disp0, task);
			/* WBC bug fix */
			insn->i_opr[2].o_mode = AMODE_IMM;
			break;
			
		      case FMT7_INSS:
		      case FMT7_EXTS:
			byte2 = get_byte(loc + consumed, task);
			consumed++;
			insn->i_opr[2].o_disp0 = ((byte2&0xe0)>>5);
			insn->i_opr[2].o_mode = AMODE_IMM;
			insn->i_opr[3].o_disp0 = ((byte2&0x1f) + 1);
			insn->i_opr[3].o_mode = AMODE_IMM;
			break;
			
		      case FMT7_MOVZD:
		      case FMT7_MOVXD:
			strcat(insn->i_monic, "d");
			break;
			
		      case FMT7_UNDEF:
			insn->i_format = ITYPE_UNDEF;
			break;
		}
		break;
		
	      case ITYPE_FMT8:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT8_OP(byte0, byte1);
		strcpy(insn->i_monic, fmt8_table[insn->i_op]);
		insn->i_iol = IOL(byte1);
		switch (insn->i_op) {
			
		      case FMT8_MOV:
			if (FMT8_REG(byte1) == FMT8_SU)
			    strcat(insn->i_monic, "su");
			else if (FMT8_REG(byte1) == FMT8_US)
			    strcat(insn->i_monic, "us");
			else
			    strcat(insn->i_monic, "??");
			/* fall through */
			
		      case FMT8_EXT:
		      case FMT8_INS:
		      case FMT8_CHECK:
		      case FMT8_INDEX:
		      case FMT8_FFS:
			strcat(insn->i_monic, iol_table[insn->i_iol]);
			/* fall through */
			
		      case FMT8_CVTP:
			if (insn->i_op != FMT8_FFS && insn->i_op != FMT8_MOV) {
				insn->i_opr[0].o_reg0 =
				    REG_R0 + FMT8_REG(byte1);
				insn->i_opr[0].o_mode = AMODE_REG;
			}
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_op == FMT8_EXT ||
			    insn->i_op == FMT8_INS) {
				consumed += db_disp(loc + consumed,
						 &insn->i_opr[3].o_disp0,
						 task);
				insn->i_opr[3].o_mode = AMODE_IMM;
			}
			break;
			
		      default:
			insn->i_format = ITYPE_UNDEF;
			break;
		}   
		break;
		
	      case ITYPE_FMT9:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT9_OP(byte1);
		strcpy(insn->i_monic, fmt9_table[insn->i_op]);
		insn->i_iol = IOL(byte1);
		i = FMT9_F(byte1);
		switch (insn->i_op) {
			
		      case FMT9_MOV:
			strcat(insn->i_monic, iol_table[insn->i_iol]);
			strcat(insn->i_monic, fol_table[i]);
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_opr[1].o_mode == AMODE_REG)
			    insn->i_opr[1].o_reg0 =
				REG_F0 + (insn->i_opr[1].o_reg0 - REG_R0);
			break;
			
		      case FMT9_LFSR:
			consumed += db_gen(insn, loc + consumed, 0x1,
					byte1, byte2, task);
			break;
			
		      case FMT9_SFSR:
			consumed += db_gen(insn, loc + consumed, 0x2,
					byte1, byte2, task);
			break;
			
		      case FMT9_MOVLF:
		      case FMT9_MOVFL:
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_opr[0].o_mode == AMODE_REG)
			    insn->i_opr[0].o_reg0 =
				REG_F0 + (insn->i_opr[0].o_reg0 - REG_R0);
			if (insn->i_opr[1].o_mode == AMODE_REG)
			    insn->i_opr[1].o_reg0 =
				REG_F0 + (insn->i_opr[1].o_reg0 - REG_R0);
			break;
			
		      case FMT9_ROUND:
		      case FMT9_TRUNC:
		      case FMT9_FLOOR:
			strcat(insn->i_monic, fol_table[i]);
			strcat(insn->i_monic, iol_table[insn->i_iol]);
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_opr[0].o_mode == AMODE_REG)
			    insn->i_opr[0].o_reg0 =
				REG_F0 + (insn->i_opr[0].o_reg0 - REG_R0);
			break;
		}
		break;

	      case ITYPE_NOP:
		insn->i_op = NOP;
		strcpy(insn->i_monic, "nop");
		break;
		
	      case ITYPE_FMT11:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT11_OP(byte1);
		switch (insn->i_op) {
			
		      case FMT11_ADD:
		      case FMT11_MOV:
		      case FMT11_CMP:
		      case FMT11_SUB:
		      case FMT11_NEG:
		      case FMT11_DIV:
		      case FMT11_MUL:
		      case FMT11_ABS:
			strcpy(insn->i_monic, fmt11_table[insn->i_op]);
			strcat(insn->i_monic, fol_table[FMT11_F(byte1)]);
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_opr[0].o_mode == AMODE_REG)
			    insn->i_opr[0].o_reg0 =
				REG_F0 + (insn->i_opr[0].o_reg0 - REG_R0);
			if (insn->i_opr[1].o_mode == AMODE_REG)
			    insn->i_opr[1].o_reg0 =
				REG_F0 + (insn->i_opr[1].o_reg0 - REG_R0);
			break;

		      default:
			insn->i_format = ITYPE_UNDEF;
			break;
			
		}
		break;
		
	      case ITYPE_FMT12:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT12_OP(byte1);
		switch (insn->i_op) {
			
		      case FMT12_POLY:
		      case FMT12_DOT:
		      case FMT12_SCALB:
		      case FMT12_LOGB:
			strcpy(insn->i_monic, fmt12_table[insn->i_op]);
			strcat(insn->i_monic, fol_table[FMT12_F(byte1)]);
			consumed += db_gen(insn, loc + consumed, 0x3, byte1,
					byte2, task);
			if (insn->i_opr[0].o_mode == AMODE_REG)
			    insn->i_opr[0].o_reg0 =
				REG_F0 + (insn->i_opr[0].o_reg0 - REG_R0);
			if (insn->i_opr[1].o_mode == AMODE_REG)
			    insn->i_opr[1].o_reg0 =
				REG_F0 + (insn->i_opr[1].o_reg0 - REG_R0);
			break;
			
		      default:
			insn->i_format = ITYPE_UNDEF;
			break;
			
		}
		break;
		
	      case ITYPE_UNDEF:
		strcpy(insn->i_monic, "???");
		break;
		
	      case ITYPE_FMT14:
		byte1 = get_byte(loc + consumed, task);
		consumed++;
		byte2 = get_byte(loc + consumed, task);
		consumed++;
		insn->i_op = FMT14_OP(byte1);
		insn->i_iol = 4;
		strcpy(insn->i_monic, fmt14_table[insn->i_op]);
		switch (insn->i_op) {
			
		      case FMT14_CINV:
			insn->i_opr[0].o_disp0 = GetShort(byte1, byte2);
			insn->i_opr[0].o_mode = AMODE_CINV;
			consumed += db_gen(insn, loc + consumed, 0x1,
					byte1/* was 0*/, byte2, task);
			break;
			
		      case FMT14_LMR:
		      case FMT14_SMR:
			insn->i_opr[0].o_reg0 =
			    mmuRegTable[GetShort(byte1, byte2)];
			insn->i_opr[0].o_mode = AMODE_REG;
			/* fall through */
			
		      case FMT14_RDVAL:
		      case FMT14_WRVAL:
			consumed += db_gen(insn, loc + consumed, 0x1,
					byte1/* was 0*/, byte2, task);
			break;
		}
		break;
		
	      default:
		insn->i_format = ITYPE_UNDEF;
		strcpy(insn->i_monic, "???");
		break;
	}
	return(consumed);
}

/*
 * Reverse the order of the bits in the LS byte of *ip.
 */
void db_reverseBits (ip)
	int *ip;
{
	int i, src, dst;
	
	src = *ip;
	dst = 0;
	for (i = 0; i < 8; ++i) {
		dst = (dst << 1) | (src & 1);
		src >>= 1;
	}
	*ip = dst;
}

/*
 * Disassemble instruction at 'loc'.  'altfmt' specifies an
 * (optional) alternate format.	 Return address of start of
 * next instruction.
 */
db_addr_t
db_disasm(loc, altfmt, task)
	db_addr_t	loc;
	boolean_t	altfmt;
	task_t		task;
{
	int ate;
	struct insn insn;
	
	db_initInsn(&insn);
	ate =  db_dasm_ns32k(&insn, loc, task);
	if (altfmt) {
		int i;
		
		for(i = 0; i < ate; i++) {
			db_printf("%02x",
				  db_get_task_value(loc + i, 1, FALSE,
						    task) & 0xff);
		}
		if (i < 4)
		    db_printf("\t");
		db_printf("\t");
	}
	db_formatAsm(&insn, loc, task, altfmt);
	db_printf("\n");
	return loc + ate;
}
