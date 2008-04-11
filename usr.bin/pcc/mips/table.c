/*	$OpenBSD: table.c,v 1.5 2008/04/11 20:45:52 stefan Exp $	*/
/*
 * Copyright (c) 2003 Anders Magnusson (ragge@ludd.luth.se).
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
 * 3. The name of the author may not be used to endorse or promote products
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

/*
 * MIPS port by Jan Enoksson (janeno-1@student.ltu.se) and
 * Simon Olsson (simols-1@student.ltu.se) 2005.
 *
 * It appears that the target machine was big endian.  The original
 * code contained many endian aspects which are now handled in
 * machine-independent code.
 * 
 * On MIPS, the assembler does an amazing amount of work for us.
 * We don't have to worry about PIC, nor about finding the address
 * of SNAMES.  Whenever possible, we defer the work to the assembler.
 */

#include "pass2.h"

#define TUWORD TUNSIGNED|TULONG
#define TSWORD TINT|TLONG
#define TWORD TUWORD|TSWORD

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* PCONVs are usually not necessary */
{ PCONV,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	# convert between word and pointer", },

/*
 * Conversions of integral<->integral types
 */

{ SCONV,	INAREG,
	SOREG,  TCHAR,
	SAREG,	TSWORD|TSHORT,
		NAREG,	RESC1,
		"	lb A1,AL	# convert oreg char to short/int\n"
		"	nop\n", },

{ SCONV, 	INAREG,
	SOREG,	TCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AL	# conver oreg char to uchar/ushort/uint\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,  TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	lbu A1,AL	# convert oreg uchar to (u)short/(u)int\n"
		"	nop\n", },

{ SCONV,	INBREG,
	SOREG,	TCHAR,
	SBREG,	TLONGLONG,
		NBREG,	RESC1,
      		"	lb A1,AL	# convert oreg char to longlong\n"
      		"	nop\n"
      		"	sra U1,A1,31\n", },

/* chor -> ulonglong handled later */

{ SCONV,	INBREG,
	SOREG,	TUCHAR,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lbu A1,AL	# convert oreg uchar to (u)longlong\n"
      		"	move U1,$zero\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1,AL	# convert oreg (u)short to char (endianness problem?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,  TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AL	# convert oreg (u)short to uchar (endianness problem?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TSWORD,
		NAREG,	RESC1,
		"	lh A1,AL	# convert oreg short to int\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TUWORD,
		NAREG,	RESC1,
		"	lhu A1,AL	# convert oreg short to uint\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TUSHORT,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	lhu A1,AL	# convert oreg ushort to (u)int\n"
		"	nop\n", },

{ SCONV,	INBREG,
	SOREG,	TSHORT,
	SBREG,	TLONGLONG,
		NBREG,	RESC1,
      		"	lh A1,AL	# convert oreg short to longlong\n"
      		"	nop\n"
      		"	sra U1,A1,31\n", },

{ SCONV,	INBREG,
	SOREG,	TSHORT,
	SBREG,	TULONGLONG,
		NBREG,	RESC1,
      		"	lhu A1,AL	# convert oreg short to ulonglong\n"
      		"	nop\n"
		"	move U1,$zero\n", },

{ SCONV,	INBREG,
	SOREG,	TUSHORT,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lhu A1,AL	# convert oreg ushort to (u)longlong\n"
      		"	move U1,$zero\n", },

{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1,AL	# convert oreg word to char (endianness problem here?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AL	# convert oreg word to uchar (endianness problem here?)\n"
		"	nop\n", },
    
{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	lh A1,AL	# convert oreg word to short (endianness problem here?)\n"
		"	nop\n", },

/* convert (u)long to ushort */
{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1,AL	# convert oreg word to ushort (endianness problem here?)\n"
		"	nop\n", },

{ SCONV,	INBREG,
	SOREG,	TSWORD,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
      		"	lw A1,AL	# convert oreg int/long to (u)llong (endianness problem here?)\n"
      		"	nop\n"
      		"	sra U1,A1,31\n" },

{ SCONV,	INBREG,
	SOREG,	TUWORD,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lw A1,AL	# convert oreg (u)int to (u)llong (endianness problem here?)\n"
      		"	move U1,$zero\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1,AL	# convert oreg (u)llong to char	(endianness problem here?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AL	# convert oreg (u)llong to uchar (endianness problem?)\n"
		"	nop\n", },
    
{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	lh A1,AL	# convert oreg (u)llong to short (endianness problem?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1,AL	# convert oreg (u)llong to ushort (endianness problem here?)\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD,
		NAREG,	RESC1,
      		"	lw A1,AL	# convert oreg (u)llong to (u)int (endianness problem here?)\n"
		"	nop\n", },

/*
 * Conversions of integral types (register-register)
 *
 * For each deunsigned type, they look something like this:
 *
 * signed -> bigger signed      - nothing to do
 * signed -> bigger unsigned    - clear the top bits (of source type)
 *
 * signed -> smaller signed     - sign-extend the bits (to dest type)
 * signed -> smaller unsigned   - clear the top bits (of dest type)
 * unsigned -> smaller signed   - sign-extend top bits (to dest type)
 * unsigned -> smaller unsigned - clear the top bits (of dest type)
 *
 * unsigned -> bigger           - nothing to do
 */

{ SCONV,	INAREG,
	SAREG,	TPOINT|TWORD,
	SAREG,	TPOINT|TWORD,
		0,	RLEFT,
		"	# convert int to int\n", },

{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0,	RLEFT,
		"	# convert (u)longlong to (u)longlong", },

{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TSWORD|TSHORT,
		0,	RLEFT,
		"	# convert char to short/int\n", },

{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,255	# convert char to uchar/ushort/uint\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,24	# convert uchar to char\n"
		"	sra A1,A1,24\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT,
		0,	RLEFT,
		"	# convert uchar to (u)short/(u)int\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,24	# convert short to char\n"
		"	sra A1,A1,24\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,255	# convert short to uchar\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TUWORD|TUSHORT,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,65535	# convert short to ushort\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TSWORD,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,16	# convert short to ushort\n"
		"	sra A1,A1,16\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,24	# convert short to char\n"
		"	sra A1,A1,24\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,255	# convert ushort to char\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,16	# convert short to ushort\n"
		"	sra A1,A1,16\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TWORD,
		0,	RDEST,
		"	# convert ushort to (u)int\n", },

{ SCONV,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,8	# convert int to char\n"
		"	sra A1,A1,8\n", },

{ SCONV,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,255	# convert int to uchar\n", },

{ SCONV,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,16	# convert int to short\n"
		"	sra A1,A1,16\n", },

{ SCONV,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TUSHORT,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,65535	# convert int to ushort\n", },

{ SCONV,	INAREG,
	SAREG,	TUWORD,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,24	# convert int to char\n"
		"	sra A1,A1,24\n", },

{ SCONV,	INAREG,
	SAREG,	TUWORD,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,255	# convert int to uchar\n", },

{ SCONV,	INAREG,
	SAREG,	TUWORD,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,16	# convert int to short\n"
		"	sra A1,A1,16\n", },

{ SCONV,	INAREG,
	SAREG,	TUWORD,
	SAREG,	TUSHORT,
		NAREG|NASL,	RESC1,
		"	andi A1,AL,65535	# convert int to ushort\n", },

{ SCONV,	INBREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SBREG,	TLONGLONG,
		NBREG,	RESC1,
		"	move A1,AL	# convert int/short/char to longlong\n"
		"	sra U1,AL,31\n", },

{ SCONV,	INBREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SBREG,	TULONGLONG,
		NBREG,	RESC1,
		"	move A1,AL	# convert int/short/char to ulonglong\n"
		"	move U1,$zero\n", },

{ SCONV,	INBREG,
	SAREG,	TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	move A1,AL	# convert (u)int/(u)short/(u)char to ulonglong\n"
		"	move U1,$zero\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	move A1,AL	# convert (u)longlong to int\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	sll A1,AL,16	# convert (u)longlong to short\n"
		"	sra A1,AL,16\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	sll A1,AL,24	# convert (u)longlong to char\n"
		"	sra A1,AL,24\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	andi A1,AL,65535	# convert (u)longlong to ushort\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	andi A1,AL,255	# convert (u)longlong to uchar\n", },

{ SCONV,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	cvt.d.s A1,AL	# convert float to (l)double\n", },

{ SCONV,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	cvt.s.d A1,AL	# convert (l)double to float\n", },

{ SCONV,	INCREG,
	SAREG,	TWORD,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	mtc1 AL,A1	# convert (u)int to float\n"
		"	nop\n"
		"	cvt.s.w A1,A1\n", },

{ SCONV,	INCREG,
	SOREG,	TWORD,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	l.s A1,AL	# convert (u)int to float\n"
		"	nop\n"
		"	cvt.s.w A1,A1\n", },

{ SCONV,	INCREG,
	SAREG,	TWORD,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	mtc1 AL,A1	# convert (u)int to (l)double\n"
		"	nop\n"
		"	cvt.d.w A1,A1\n", },

{ SCONV,	INCREG,
	SOREG,	TWORD,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	l.d A1,AL	# convert (u)int to (l)double\n"
		"	nop\n"
		"	cvt.d.w A1,A1\n", },

{ SCONV,	INAREG,
	SCREG,	TFLOAT,
	SAREG,	TWORD,
		NCREG|NAREG,	RESC1,
		"	cvt.w.s A2,AL	# convert float to (u)int\n"
		"	mfc1 A1,A2\n"
		"	nop\n", },

{ SCONV,	FOREFF,
	SCREG,	TFLOAT,
	SOREG,	TWORD,
		NCREG,	RDEST,
		"	cvt.w.s A1,AL	# convert float to (u)int\n"
		"	s.s A1,AR\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TWORD,
		NCREG|NAREG,	RESC1,
		"	cvt.w.d A2,AL	# convert (l)double to (u)int\n"
		"	mfc1 A1,A2\n"
		"	nop\n", },

{ SCONV,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		"	# convert between double and ldouble\n", },

{ SCONV,	INCREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TFLOAT,
		NSPECIAL|NCREG,	RESC1,
		"ZF", },

{ SCONV,	INCREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NCREG,	RESC1,
		"ZF", },

{ SCONV,	INBREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INBREG,
	SCREG,	TFLOAT,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

/*
 * Multiplication and division
 */

{ MUL,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	multu AL,AR	# unsigned multiply\n"
		"	mflo A1\n"
		"	nop\n"
		"	nop\n", },

/* this previous will match on unsigned/unsigned multiplication first */
{ MUL,	INAREG,
	SAREG,	TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
	SAREG,	TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	mult AL,AR	# signed multiply\n"
		"	mflo A1\n"
		"	nop\n"
		"	nop\n", },

{ MUL,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		2*NBREG,	RESC1,
		"	multu AL,AR\n"
		"	mfhi U1\n"
		"	mflo A1\n"
		"	mult AL,UR\n"
		"	mflo A2\n"
		"	nop\n"
		"	nop\n"
		"	addu A2,U1,A2\n"
		"	mult UL,AR\n"
		"	mflo U2\n"
		"	nop\n"
		"	nop\n"
		"	addu U1,A2,U2\n", },

{ MUL,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	mul.s A1,AL,AR		# floating-point multiply\n", },

{ MUL,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1, 
		"	mul.d	A1,AL,AR	# double-floating-point multiply\n", },

{ DIV,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	divu AL,AR	# unsigned division\n"
		"	mflo A1\n"
		"	nop\n"
		"	nop\n", },

/* the previous rule will match unsigned/unsigned first */
{ DIV,	INAREG,
	SAREG,	TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
	SAREG,	TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	div AL,AR	# signed division\n"
		"	mflo A1\n"
		"	nop\n"
		"	nop\n", },

{ DIV, INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ DIV,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	div.s A1,AL,AR		# floating-point division\n", },

{ DIV,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1, 
		"	div.d	A1,AL,AR	# double-floating-point division\n", },

{ MOD,  INAREG,
        SAREG,  TUWORD|TUSHORT|TUCHAR,
        SAREG,  TUWORD|TUSHORT|TUCHAR,
                NAREG,  RESC1,
                "       divu AL,AR	# signed modulo\n"
		"	mfhi A1\n"
		"	nop\n"
		"	nop\n", },

/* the previous rule will match unsigned%unsigned first */
{ MOD,  INAREG,
        SAREG,  TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
        SAREG,  TWORD|TUSHORT|TSHORT|TUCHAR|TCHAR,
                NAREG,  RESC1,
                "	div AL,AR	# signed modulo\n"
		"	mfhi A1\n"
		"	nop\n"
		"	nop\n", },

{ MOD,  INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
                NSPECIAL|NBREG,  RESC1,
                "ZE", },
    
/*
 * Templates for unsigned values needs to come before OPSIMP 
 */

{ PLUS,	INBREG,
	SBREG,	TULONGLONG|TLONGLONG,
	SBREG,	TULONGLONG|TLONGLONG,
		2*NBREG,	RESC1,
      		"	addu A1,AL,AR	# 64-bit addition\n"
      		"	sltu A2,A1,AR\n"
      		"	addu U1,UL,UR\n"
      		"	addu U1,U1,A2\n", },

{ PLUS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		NAREG|NASL,	RESC1,
      		"	add A1,AL,AR\n", },

{ PLUS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SSCON,	TWORD,
		NAREG|NASL,	RESC1,
		"	addi A1,AL,AR\n", },

{ PLUS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SSCON,	TWORD,
		NAREG|NASL,	RESC1,
		"	addiu A1,AL,AR\n", },

{ PLUS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
      		"	addu A1,AL,AR\n", },

{ PLUS,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"	add.s A1,AL,AR\n", },

{ PLUS,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,
		"	add.d A1,AL,AR\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		2*NBREG,	RESC1,
      		"	sltu A2,AL,AR	# 64-bit subtraction\n"
      		"	subu A1,AL,AR\n"
      		"	subu U1,UL,UR\n"
      		"	subu U1,U1,A2\n", },

{ MINUS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		NAREG|NASL,	RESC1,
      		"	sub A1,AL,AR\n", },

{ MINUS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
      		"	subu A1,AL,AR\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,	TANY,
		NAREG|NASL,	RESC1,
		"	subu A1,AL,AR\n", },

{ MINUS,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"	sub.s A1,AL,AR\n", },

{ MINUS,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,
		"	sub.d A1,AL,AR\n", },

{ UMINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	neg A1,AL\n", },

{ UMINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NBREG|NAREG|NBSL,	RESC1,
		"	subu A1,$zero,AL\n"
		"	subu U1,$zero,UL\n"
		"	sltu A2,$zero,A1\n"
		"	subu U1,U1,A2\n", },

{ UMINUS,	INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"	neg.s A1,AL\n", },

{ UMINUS,	INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,
		"	neg.d A1,AL\n", },

/* Simple 'op rd, rs, rt' or 'op rt, rs, imm' operations */

{ OPSIMP,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSR|NBSL,	RESC1,
      		"	O A1,AL,AR\n"
      		"	O U1,UL,UR\n", },
    
{ OPSIMP,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	O A1,AL,AR\n", },

{ OPSIMP,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
	SPCON,	TSHORT|TUSHORT|TUCHAR|TCHAR,
		NAREG|NASL,	RESC1,
		"	Oi A1,AL,AR\n", },

/*
 * Shift instructions
 */

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sra A1,AL,AR	# shift right by constant\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srl A1,AL,AR	# shift right by constant\n", },

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1,AL,AR	# shift left by constant\n", },
    
{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srav A1,AL,AR	# shift right by register\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srlv A1,AL,AR	# shift right by register\n", },

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sllv A1,AL,AR	# shift left by register\n", },	

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NBREG,	RESC1,
		"ZO", },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NBREG,	RESC1,
		"ZO", },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

/*
 * Rule for unary one's complement
 */

{ COMPL,        INAREG,
        SAREG,  TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SANY,   TANY,
                NAREG|NASL,   RESC1,
                "	nor A1,$zero,AL	# complement\n", },
    
{ COMPL,        INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
        SANY,   TANY,
                NBREG|NBSL,   RESC1,
                "	nor A1,$zero,AL	# complement\n"
                "	nor U1,$zero,UL\n", },
    
/*
 * The next rules takes care of assignments. "=".
 */

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		0,	RDEST,
		"	sw AR,AL		# store (u)int/(u)long\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		0,	RDEST,
        	"	sh AR,AL		# store (u)short\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		0,	RDEST,
        	"	sb AR,AL		# store (u)char\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INBREG,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		0,	RDEST,
      		"	sw UR,UL		# store (u)longlong\n"
		"	nop\n"
      		"	sw AR,AL\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		0,	RDEST,
      		"	move UL,UR		# register move\n"
      		"	move AL,AR\n", },
    
{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RDEST,
        	"	move AL,AR		# register move\n", },

{ ASSIGN,	FOREFF|INCREG,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		0,	RDEST,
        	"	mov.s AL,AR		# register move\n", },

{ ASSIGN,	FOREFF|INCREG,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
        	"	mov.d AL,AR		# register move\n", },

{ ASSIGN,	FOREFF|INCREG,
	SNAME|SOREG,	TFLOAT,
	SCREG,		TFLOAT,
		0,	RDEST,
		"	s.s AR,AL		# store floating-point reg to oreg/sname\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INCREG,
	SNAME|SOREG,	TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	s.d AR,AL		# store double floating-point reg to oreg/sname\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SFLD,		TANY,
	SOREG|SNAME,	TANY,
		3*NAREG,	RDEST,
		"	lw A1,AR		# bit-field assignment\n"
		"	li A3,M\n"
		"	lw A2,AL\n"
		"	sll A1,A1,H\n"
		"	and A1,A1,A3\n"
		"	nor A3,$zero,A3\n"
		"	and A2,A2,A3\n"
		"	or A2,A2,A1\n"
		"	sw A2,AL\n"
		"F	lw AD,AR\n"
		"F	nop\n"
		"F	sll AD,AD,32-S\n"
		"F	sra AD,AD,32-S\n", },

/* XXX we can optimise this away */
{ ASSIGN,	FOREFF|INAREG,
	SFLD,		TANY,
	SCON,		TANY,
		3*NAREG,	RDEST,
		"	li A1,AR		# bit-field assignment\n"
		"	lw A2,AL\n"
		"	li A3,M\n"
		"	sll A1,A1,H\n"
		"	and A1,A1,A3\n"
		"	nor A3,$zero,A3\n"
		"	and A2,A2,A3\n"
		"	or A2,A2,A1\n"
		"	sw A2,AL\n"
		"F	li AD,AR\n"
		"F	sll AD,AD,32-S\n"
		"F	sra AD,AD,32-S\n", },

{ ASSIGN,	FOREFF|INAREG,
	SFLD,		TANY,
	SAREG,		TANY,
		3*NAREG,	RDEST,
		"	move A1,AR		# bit-field assignment\n"
		"	lw A2,AL\n"
		"	li A3,M\n"
		"	sll A1,A1,H\n"
		"	and A1,A1,A3\n"
		"	nor A3,$zero,A3\n"
		"	and A2,A2,A3\n"
		"	or A2,A2,A1\n"
		"	sw A2,AL\n"
		"F	move AR,AD\n"
		"F	sll AD,AD,32-S\n"
		"F	sra AD,AD,32-S\n", },

{ STASG,        INAREG|FOREFF,
        SOREG|SNAME,	TANY,
        SAREG,  	TPTRTO|TANY,
                NSPECIAL,       RRIGHT,
                "ZQ", },

/*
 * Compare instructions
 */

{ EQ,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
                0,      RESCC,
                "	beq AL,AR,LC\n"
		"	nop\n", },

{ NE,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
                0,      RESCC,
                "	bne AL,AR,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SZERO,		TANY,
                0,      RESCC,
                "	O AL,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
                NAREG|NASL,     RESCC,
		"	sub A1,AL,AR\n"
                "	O A1,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SSCON,		TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
                NAREG|NASL,     RESCC,
		"	sub A1,AL,AR\n"
                "	O A1,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NAREG,	RESCC,
		"ZD", },

{ OPLOG,	FORCC,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		0,	RESCC,
		"ZG", },

/*
 * Convert LTYPE to reg.
 */

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TCHAR,
		NAREG,	RESC1,
		"	lb A1,AL	# load char to reg\n"
		"	nop\n", },
	
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AL	# load uchar to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG,	RESC1,
		"	lh A1,AL	# load short to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1,AL	# load ushort to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	lw A1,AL	# load (u)int/(u)long to reg\n"
		"	nop\n", },

{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lw U1,UL	# load (u)longlong to reg\n"
		"	nop\n"
		"	lw A1,AL\n"
      		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TPOINT,
		NAREG,	RESC1,
		"	la A1,AL	# load constant address to reg\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG,	RESC1,
		"	move A1,$zero	# load 0 to reg\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"	li A1,AL	# load constant to reg\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SZERO,	TANY,
		NBREG,	RESC1,
		"	move A1,$zero	# load 0 to reg\n"
		"	move U1,$zero\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SCON,	TANY,
		NBREG,	RESC1,
		"	li A1,AL	# load constant to reg\n"
		"	li U1,UL\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	move A1,AL\n", },

{ OPLTYPE,	INCREG,
	SANY,	TANY,
	SZERO,	TFLOAT,
		NCREG,	RESC1,
		"	mtc1 $zero,A1	# load 0 to float reg\n"
		"	nop\n", },

{ OPLTYPE,	INCREG,
	SANY,	TANY,
	SZERO,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	mtc1 $zero,A1	# load 0 to (l)double reg\n"
		"	mtc1 $zero,U1\n"
		"	nop\n", },

{ OPLTYPE,	INCREG,
	SANY,	TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		"	l.s A1,AL	# load into floating-point reg\n"
		"	nop\n", },

{ OPLTYPE,	INCREG,
	SANY,	TANY,
	OREG|SNAME,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	l.d A1,AL	# load into double floating-point reg\n"
		"	nop\n", },
    
/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	j LL		# goto label\n"
		"	nop\n"
		"	nop\n", },

/*
 * Subroutine calls.
 */

{ CALL,         FOREFF,
        SCON,		TANY,
        SANY,           TANY,
                0,      0,
                "	subu $sp,$sp,16	# call (args, no result) to scon/sname\n"
                "	jal CL\n"
		"	nop\n"
		"ZC", },

{ UCALL,        FOREFF,
        SCON,		TANY,
        SANY,           TANY,
                0,      0,
                "	jal CL			# call (no args, no result) to scon/sname\n"
		"	nop\n", },

{ CALL,         INAREG,
        SCON,		TANY,
        SAREG,          TANY,
                NAREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result in v0) to scon/sname\n"
		"	jal CL\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INAREG,
        SCON,		TANY,
        SAREG,          TANY,
                NAREG,     RESC1,  /* should be 0 */
                "	jal CL   # call (no args, result in v0) to scon/sname\n"
		"	nop\n",
 },

{ CALL,         INBREG,
        SCON,		TANY,
        SBREG,          TANY,
                NBREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result in v0:v1) to scon/sname\n"
		"	jal CL\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INBREG,
        SCON,		TANY,
        SBREG,          TANY,
                NBREG,     RESC1,  /* should be 0 */
                "	jal CL   # call (no args, result in v0:v1) to scon/sname\n"
		"	nop\n",
 },

{ CALL,         INCREG,
        SCON,		TANY,
        SCREG,          TANY,
                NCREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result in f0:f1) to scon/sname\n"
		"	jal CL\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INCREG,
        SCON,		TANY,
        SCREG,          TANY,
                NCREG,     RESC1,  /* should be 0 */
                "	jal CL   # call (no args, result in v0:v1) to scon/sname\n"
		"	nop\n",
 },

{ CALL,         FOREFF,
        SAREG,		TANY,
        SANY,		TANY,
                0,      0,
                "	subu $sp,$sp,16	# call (args, no result) to reg\n"
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },

{ UCALL,        FOREFF,
        SAREG,		TANY,
        SANY,		TANY,
                0,      0,
		"	move $25,AL\n"
                "	jal $25			# call (no args, no result) to reg\n"
		"	nop\n", },

{ CALL,         INAREG,
        SAREG,		TANY,
        SAREG,		TANY,
                NAREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result) to reg\n"
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INAREG,
        SAREG,		TANY,
        SAREG,		TANY,
                NAREG,     RESC1,  /* should be 0 */
		"	move $25,AL\n"
                "	jal $25		# call (no args, result) to reg\n"
		"	nop\n", },

{ CALL,         INBREG,
        SAREG,		TANY,
        SBREG,		TANY,
                NBREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result) to reg\n"
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INBREG,
        SAREG,		TANY,
        SBREG,		TANY,
                NBREG,     RESC1,  /* should be 0 */
		"	move $25,AL\n"
                "	jal $25			# call (no args, result) to reg\n"
		"	nop\n", },

{ CALL,         INCREG,
        SAREG,		TANY,
        SCREG,		TANY,
                NCREG,     RESC1,  /* should be 0 */
                "	subu $sp,$sp,16	# call (args, result) to reg\n"
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },

{ UCALL,        INCREG,
        SCREG,		TANY,
        SCREG,		TANY,
                NCREG,     RESC1,  /* should be 0 */
		"	move $25,AL\n"
                "	jal $25			# call (no args, result) to reg\n"
		"	nop\n", },


/* struct return */
{ USTCALL,      FOREFF,
	SCON|SNAME,	TANY,
	SANY,   	TANY,
		0,	0,
		"	jal CL\n"
		"	nop\n", },

{ USTCALL,      FOREFF,
	SAREG,		TANY,
	SANY,   	TANY,
		0,	0,
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n", },

{ USTCALL,      INAREG,
	SCON|SNAME,	TANY,
	SANY,   	TANY,
		NAREG|NASL,	RESC1,
		"	jal CL\n"
		"	nop\n", },

{ USTCALL,      INAREG,
	SAREG,		TANY,
	SANY,   	TANY,
		NAREG|NASL,	RESC1,
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n", },

{ STCALL,      FOREFF,
	SCON|SNAME,	TANY,
	SANY,   	TANY,
		0,	0,
		"	jal CL\n"
		"	nop\n"
		"ZC", },

{ STCALL,      FOREFF,
	SAREG,	TANY,
	SANY,   	TANY,
		0,	0,
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },

{ STCALL,      INAREG,
	SCON|SNAME,	TANY,
	SANY,   	TANY,
		NAREG|NASL,	RESC1,
		"	jal CL\n"
		"	nop\n"
		"ZC", },

{ STCALL,      INAREG,
	SAREG,	TANY,
	SANY,   	TANY,
		0,	0,
		"	move $25,AL\n"
                "	jal $25\n"
		"	nop\n"
		"ZC", },


/*
 *  Function arguments
 */

/* intentionally write out the register for (u)short/(u)char */
{ FUNARG,       FOREFF,
        SAREG,  TWORD|TPOINT|TUSHORT|TSHORT|TUCHAR|TCHAR,
        SANY,   TWORD|TPOINT|TUSHORT|TSHORT|TUCHAR|TCHAR,
                0,      0,
                "	subu $sp,$sp,4		# save function arg to stack\n"
		"	sw AL,($sp)\n"
		"	#nop\n", },

{ FUNARG,	FOREFF,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TLONGLONG|TULONGLONG,
		0,	0,
		"	addi $sp,$sp,-8		# save function arg to stack (endian problem here?\n"
		"	sw UL,4($sp)\n"
		"	sw AL,($sp)\n"
		"	#nop\n", },

{ FUNARG,	FOREFF,
	SCREG,	TFLOAT,
	SANY,	TFLOAT,
		0,	0,
		"	addi $sp,$sp,-4		# save function arg to stack\n"
		"	s.s AL,($sp)\n"
		"	#nop\n", },

{ FUNARG,	FOREFF,
	SCREG,	TDOUBLE|TLDOUBLE,
	SANY,	TDOUBLE|TLDOUBLE,
		0,	0,
		"	addi $sp,$sp,-8		# save function arg to stack\n"
		"	s.d AL,($sp)\n"
		"	#nop\n", },

{ STARG,	FOREFF,
	SAREG,		TANY,
	SANY,		TSTRUCT,
		NSPECIAL,	0,
		"ZH", },

/*
 * Indirection operators.
 */
{ UMUL, INAREG,
	SANY,	TPOINT|TWORD,
	SOREG,	TPOINT|TWORD,
    		NAREG,     RESC1,
        	"	lw A1,AL		# word load\n"
		"	nop\n", },

{ UMUL, INAREG,
	SANY,	TSHORT|TUSHORT,
	SOREG,	TSHORT|TUSHORT,
    		NAREG,     RESC1,
        	"	lh A1,AL		# (u)short load\n"
		"	nop\n", },

{ UMUL, INAREG,
	SANY,	TCHAR|TUCHAR,
	SOREG,	TCHAR|TUCHAR,
    		NAREG,     RESC1,
        	"	lb A1,AL		# (u)char load\n"
		"	nop\n", },

{ UMUL,	INBREG,
	SANY,	TLONGLONG|TULONGLONG,
	SOREG,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lw A1,AL		# (u)longlong load - endian problem here?\n"
		"	nop\n"
		"	lw U1,UL\n"
		"	nop\n", },

{ UMUL,	INCREG,
	SANY,	TFLOAT,
	SOREG,	TFLOAT,
		NCREG,	RESC1,
		"	l.s A1,AL		# float load\n"
		"	nop\n", },

{ UMUL,	INCREG,
	SANY,	TDOUBLE|TLDOUBLE,
	SOREG,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	l.d A1,AL		# float load\n"
		"	nop\n", },

#if 0
{ UMUL,	INCREG,
	SANY,	TDOUBLE|TLDOUBLE,
	SAREG,	TPOINT,
		NCREG,	RESC1,
		"	l.d A1,(AL)\n"
		"	nop\n", },
    
{ UMUL, INAREG,
	SANY,	TPOINT|TWORD,
	SNAME,	TPOINT|TWORD,
    		NAREG,     RESC1,
        	"	la A1,AL		# sname word load\n"
		"	lw A1,(A1)\n"
		"	nop\n", },

{ UMUL, INAREG,
	SANY,	TSHORT|TUSHORT,
	SNAME,	TSHORT|TUSHORT,
    		NAREG,     RESC1,
        	"	la A1,AL		# sname (u)short load\n"
		"	lh A1,(A1)\n"
		"	nop\n", },

{ UMUL, INAREG,
	SANY,	TCHAR|TUCHAR,
	SNAME,	TCHAR|TUCHAR,
    		NAREG,     RESC1,
        	"	la A1,AL		# sname (u)char load\n"
		"	lb A1,(A1)\n"
		"	nop\n", },

{ UMUL, INBREG,
	SANY,	TLONGLONG|TULONGLONG,
	SNAME,	TLONGLONG|TULONGLONG,
		NBREG|NAREG,	RESC1,
		"	la A2,AL		# sname (u)long long load - endian problems here?\n"
		"	lw A1,(A1)\n"
		"	nop\n"
		"	lw U1,4(A1)\n"
		"	nop\n", },
#endif

{ UMUL, INAREG,
	SANY,	TPOINT|TWORD,
	SAREG,	TPOINT|TWORD,
    		NAREG,     RESC1,
        	"	lw A1,(AL)		# word load\n"
		"	nop\n", },

#if 0
{ UMUL, INAREG,
	SANY,	TSHORT|TUSHORT,
	SAREG,	TPTRTO|TSHORT|TUSHORT,
    		NAREG,     RESC1,
        	"	lh A1,(AL)		# (u)short load\n"
		"	nop\n", },

{ UMUL, INAREG,
	SANY,	TCHAR|TUCHAR,
	SAREG,	TPTRTO|TCHAR|TUCHAR,
    		NAREG|NASL,     RESC1,
        	"	lb A1,(AL)		# (u)char load\n"
		"	nop\n", },

{ UMUL, INBREG,
	SANY,	TLONGLONG|TULONGLONG,
	SAREG,	TPTRTO|TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lw A1,(AL)		# (u)long long load - endianness problems?\n"
		"	nop\n"
		"	lw U1,4(AL)"
		"	nop\n", },
#endif

#define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

{ FLD, DF(FLD), },

{ FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
