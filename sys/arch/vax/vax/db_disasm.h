/*	$OpenBSD: db_disasm.h,v 1.5 2015/02/07 23:30:13 miod Exp $ */
/*	$NetBSD: db_disasm.h,v 1.1 1996/01/28 11:31:27 ragge Exp $ */
/*
 * Copyright (c) 2002, Miodrag Vallat.
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#define SIZE_BYTE	 1		/* Byte */
#define SIZE_WORD	 2		/* Word */
#define SIZE_LONG 	 4		/* Longword */
#define SIZE_QWORD	 8		/* Quadword */
#define SIZE_OWORD	16		/* Octaword */

/*
 * The VAX instruction set has a variable length instruction format which 
 * may be as short as one byte and as long as needed depending on the type 
 * of instruction. [...] Each instruction consists of an opcode followed 
 * by zero to six operand specifiers whose number and type depend on the 
 * opcode. All operand specifiers are, themselves, of the same format -- 
 * i.e. an address mode plus additional information.
 *
 * [VAX Architecture Handbook, p.52:  Instruction Format]
 * two-byte instruction set from
 * [VAX Architecture Reference Manual, appendix A: Opcode Assignments]
 */

/*
 * argdesc describes each arguments by two characters denoting
 * the access-type and the data-type.
 *
 * Arguments (Access-Types):
 *	r: operand is read only
 *	w: operand is written only
 *	m: operand is modified (both R and W)
 *	b: no operand reference. Branch displacement is specified. 
 *	a: calculate the address of the specified operand
 *	v: if not "Rn", same as a. If "RN," R[n+1]R[n]
 * Arguments (Data-Types):
 *	b: Byte
 *	w: Word
 *	l: Longword
 *	q: Quadword
 *	o: Octaword
 *	d: D_floating
 *	f: F_floating
 *	g: G_floating
 *	h: H_floating
 *	r: Register
 *	x: first data type specified by instruction
 *	y: second data type spcified by instructin
 *	-: no-args
 *	?: unknown (variable?)
 */

typedef struct vax_instr_t {
	char *mnemonic;
	char *argdesc;
} vax_instr_t;

/* one-byte instructions */
extern vax_instr_t vax_inst[];

/* two-byte instructions */

/*
 * reasonably simple macro to gather all the reserved two-byte opcodes
 * into only a few table entries...
 */
#define	INDEX_OPCODE(x)	\
	(((x) & 0xff00) == 0xfe00) ? 0 : \
	((x) < 0xfd30) ? 0 : \
	((x) < 0xfd80) ? (x) - 0xfd30 : \
	((x) == 0xfd98) ? 0x50 : \
	((x) == 0xfd99) ? 0x51 : \
	((x) == 0xfdf6) ? 0x52 : \
	((x) == 0xfdf7) ? 0x53 : \
	((x) == 0xfffd) ? 0x54 : \
	((x) == 0xfffe) ? 0x55 : 0

extern vax_instr_t vax_inst2[];
