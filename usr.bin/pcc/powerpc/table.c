/*-
 * $OpenBSD: table.c,v 1.3 2007/11/01 10:52:58 otto Exp $
 *
 * Copyright (c) 2007 Gregory McGarry <g.mcgarry@ieee.org>
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

/*
 * A template has five logical sections:
 *
 *	1) subtree (operator); goal to achieve (cookie)
 *	2) left node descendent of operator (node class; type)
 *	3) right node descendent of operator (node class; type)
 *	4) resource requirements (number of scratch registers);
 *	   subtree rewriting rule
 *	5) emitted instructions
 */

#include "pass2.h"

#define TUWORD	TUNSIGNED|TULONG
#define TSWORD	TINT|TLONG
#define TWORD	TUWORD|TSWORD

#ifdef ELFABI
#define HA16(x)	# x "@ha"
#define LO16(x)	# x "@l"
#define COM	"	# "
#else
#define HA16(x)	"ha16(" # x ")"
#define LO16(x)	"lo16(" # x ")"
#define COM	"	; "
#endif

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* PCONVs are not necessary */
{ PCONV,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		COM "pointer conversion\n", },

/*
 * Conversions of integral types
 */

/* convert (u)char to (u)char */
{ SCONV,	INAREG,
	INAREG,	TCHAR|TUCHAR,
	INAREG,	TCHAR|TUCHAR,
		0,	RLEFT,
		COM "convert between (u)char and (u)char\n", },

/* convert (u)short to (u)short */
{ SCONV,	INAREG,
	INAREG,	TSHORT|TUSHORT,
	INAREG,	TSHORT|TUSHORT,
		0,	RLEFT,
		COM "convert between (u)short and (u)short\n", },

/* convert pointers to (u)int/(u)long */
{ SCONV,	INAREG,
	SAREG,	TPOINT|TWORD,
	SAREG,	TWORD,
		0,	RLEFT,
		COM "convert a pointer/word to an int\n", },

/* convert pointers to pointers */
{ SCONV,	INAREG,
	SAREG,	TPOINT,
	SAREG,	TPOINT,
		0,	RLEFT,
		COM "convert pointers\n", },

/* convert (u)longlong to (u)longlong */
{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0,	RLEFT,
		COM "convert (u)longlong to (u)longlong\n", },


/* convert char to short */
{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TSHORT|TSWORD,
		NASL|NAREG,	RESC1,
		"	extsb A1,AL" COM "convert char to short/int\n", },

/* convert uchar to short */
{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TSHORT|TSWORD,
		NASL|NAREG,	RESC1,
		COM "convert uchar to short/int\n", },

/* convert uchar to ushort/uint/ulong */
{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TUSHORT|TUWORD,
		0,		RLEFT,
		COM "convert uchar (AL) to ushort/unsigned (A1)\n", },

/* XXX is this necessary? */
/* convert char to ushort/uint/ulong */
{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TUSHORT|TUWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	andi. A1,AL,255" COM "convert char (AL) to ushort/unsigned (A1)\n", },

/* convert uchar/ushort/uint to (u)longlong */
{ SCONV,	INBREG,
	SAREG,	TUCHAR|TUSHORT|TUNSIGNED,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,		RESC1,
		"	mr A1,AL" COM "convert uchar/ushort/uint to (u)longlong\n"
		"	li U1,0\n", },

/* convert char/short/int to ulonglong */
{ SCONV,	INBREG,
	SAREG,	TCHAR|TSHORT|TSWORD,
	SBREG,	TULONGLONG,
		NBREG,		RESC1,
		"	andi. A1,AL,255" COM "convert char/short/int to ulonglong\n"
		"	li U1,0\n", },

/* convert char/short/int to longlong */
{ SCONV,	INBREG,
	SAREG,	TCHAR|TSHORT|TSWORD,
	SBREG,	TLONGLONG,
		NBREG|NBSL,		RESC1,
		"	mr A1,AL" COM "convert char/short/int to longlong\n"
		"	srawi U1,AL,31\n", },

/* convert (u)short to (u)char  */
{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR|TUCHAR,
		NSPECIAL|NAREG|NASL,	RESC1,
		"	andi. A1,AL,255" COM "convert (u)short to (u)char\n", },

/* XXX is this really necessary? */
/* convert short to int */
{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TWORD,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	andi. A1,AL,65535" COM "convert short to int\n", },

/* convert ushort to (u)int. */
{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		COM "convert ushort to word\n", },

/* convert (u)int to (u)char */
{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	andi. A1,AL,255" COM "convert (u)int to (u)char", },

/* convert (u)int to (u)short */
{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TSHORT|TUSHORT,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	andi. A1,AL,65535" COM "convert (u)int to (u)short\n", },

/* conversions on load from memory */

/* char */
{ SCONV,	INAREG,
	SOREG,	TCHAR,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "convert char to int/long\n"
		"	extsb A1,A1\n", },

/* uchar */
{ SCONV,	INAREG,
	SOREG,	TUCHAR,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "convert uchar to int/long\n", },

/* short, ushort */
{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lha A1,AL" COM "convert (u)short to int/long\n", },

/*
 * Subroutine calls.
 */

{ CALL,		FOREFF,
	SCON|SNAME,	TANY,
	SANY,		TANY,
		0,	0,
		"	bl CL" COM "call (args, no result) to scon/sname (CL)\n", },

{ UCALL,	FOREFF,
	SCON|SNAME,	TANY,
	SANY,		TANY,
		0,	0,
		"	bl CL" COM "call (no args, no result) to scon/sname (CL)\n", },

{ CALL,		INAREG,
	SCON|SNAME,	TANY,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r3) to scon/sname (CL)\n", },

{ CALL,		INBREG,
	SCON|SNAME,	TANY,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r3:r4) to scon/sname (CL)\n", },

{ UCALL,	INAREG,
	SCON|SNAME,	TANY,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r3) to scon/sname (CL)\n", },

{ UCALL,	INBREG,
	SCON|SNAME,	TANY,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r3:r4) to scon/sname (CL)\n", },

/* struct return */
{ USTCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	0,
		"ZP	call CL\n", },

{ USTCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"ZP	call CL\n", },

{ USTCALL,	INAREG,
	SNAME|SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"ZP	call *AL\n", },

{ STCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	0,
		"ZP	call CL\n", },

{ STCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"ZP	call CL\n", },

{ STCALL,	INAREG,
	SNAME|SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"ZP	call *AL\n", },

/*
 * The next rules handle all binop-style operators.
 */

/* XXX AL cannot be R0 */
{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	addi A1,AL,AR" COM "addition of constant\n", },

/* XXX AL cannot be R0 */
{ PLUS,		INAREG|FORCC,
	SAREG,	TWORD|TPOINT,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	addic. A1,AL,AR" COM "addition of constant\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	addic A1,AL,AR" COM "64-bit addition of constant\n"
		"	addze U1,UL", },

{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	add A1,AL,AR\n", },

{ PLUS,		INAREG|FORCC,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	add. A1,AL,AR\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	addc A1,AL,AR" COM "64-bit add\n"
		"	adde U1,UL,UR\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	addi A1,AL,-AR\n", },

{ MINUS,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	addi. A1,AL,-AR\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	addic A1,AL,-AR\n"
		"	addme U1,UL\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	subf A1,AR,AL\n", },

{ MINUS,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	subf. A1,AR,AL\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	subfc A1,AR,AL" COM "64-bit subtraction\n"
		"	subfe U1,UR,UL\n", },

/*
 * The next rules handle all shift operators.
 */

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	slw A1,AL,AR" COM "left shift\n", },

{ LS,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	slw. A1,AL,AR" COM "left shift\n", },

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	slwi A1,AL,AR" COM "left shift by constant\n", },

{ LS,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	slwi. A1,AL,AR" COM "left shift by constant\n", },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZO" },

{ RS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srw A1,AL,AR" COM "right shift\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srw. A1,AL,AR" COM "right shift\n", },

{ RS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srwi A1,AL,AR" COM "right shift by constant\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srwi. A1,AL,AR" COM "right shift by constant\n", },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZO" },

/*
 * The next rules takes care of assignments. "=".
 */

/* assign 16-bit constant to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TANY,
	SSCON,		TANY,
		0,	RDEST,
		"	li AL,AR\n", },

/* assign 16-bit constant to register */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TANY,
	SSCON,		TANY,
		0,	RDEST,
		"	li AL,AR\n"
		"	li UL,0\n", },

/* assign constant to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TANY,
	SCON,		TANY,
		0,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	addi AL,AL," LO16(AR) "\n", },

/* assign constant to register */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TANY,
	SCON,		TANY,
		0,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	addi AL,AL," LO16(AR) "\n"
		"	lis UL," HA16(UR) "\n"\
		"	addi UL,UL," LO16(UR) "\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SOREG,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	lwz AL,AR\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SNAME,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign sname to reg\n"
		"	lwz AL," LO16(AR) "(AL)\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SOREG,		TLONGLONG|TULONGLONG,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "assign llong to reg\n"
		"	lwz UL,UR\n" },

{ ASSIGN,	FOREFF|INAREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SNAME,		TLONGLONG|TULONGLONG,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign 64-bit sname to reg\n"
		"	lwz AL," LO16(AR) "(AL)\n"
		"	lis UL," HA16(UR) "\n"
		"	lwz UL," LO16(UR) "(UL)\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SOREG,		TSWORD,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "load int/pointer into llong\n"
		"	srawi UL,AR,31\n" },

/* assign memory to register */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SOREG,		TUNSIGNED|TPOINT,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "load uint/pointer into (u)llong\n"
		"	li UL, 0\n" },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUCHAR,
	SOREG,		TUCHAR,
		NSPECIAL,	RDEST,
		"	lbz AL,AR\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUCHAR,
	SNAME,		TUCHAR,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign uchar sname to reg\n"
		"	lbz AL," LO16(AR) "(AL)\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TCHAR,
	SOREG,		TCHAR,
		NSPECIAL,	RDEST,
		"	lbz AL,AR\n"
		"	extsb AL,AL\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TCHAR,
	SNAME,		TCHAR,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign char sname to reg\n"
		"	lbz AL," LO16(AR) "(AL)\n"
		"	extsb AL,AL\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SOREG,		TSHORT|TUSHORT,
		NSPECIAL,	RDEST,
		"	lha AL,AR\n", },

/* assign memory to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SNAME,		TSHORT|TUSHORT,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	lha AL," LO16(AR) "(AL)\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	stw AR,AL\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) COM "assign reg to sname\n"
		"	stw AR," LO16(AL) "(A1)\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INBREG,
	SOREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL,	RDEST,
		"	stw AR,AL" COM "store 64-bit value\n"
		"	stw UR,UL\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INBREG,
	SNAME,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) COM "assign reg to 64-bit sname\n"
		"	stw AR," LO16(AL) "(A1)\n"
		"	lis U1," HA16(UL) "\n"
		"	stw UR," LO16(UL) "(U1)\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		NSPECIAL,	RDEST,
		"	stb AR,AL\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL)
		"	stb AR," LO16(AL) "(A1)\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		NSPECIAL,	RDEST,
		"	sth AR,AL\n", },

/* assign register to memory */
{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	sth AR," LO16(AL) "(A1)\n", },

/* assign register to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RDEST,
		"	mr AL,AR" COM "assign AR to AL\n", },

{ ASSIGN,      FOREFF|INBREG,
        SBREG,	TLONGLONG|TULONGLONG,
        SBREG,	TLONGLONG|TULONGLONG,
                0,  RDEST,
		"	mr AL,AR" COM "assign UR:AR to UL:AL\n"
                "	mr UL,UR\n", },

#if 0
/* assign register to memory */
{ ASSIGN,	FOREFF,
	SAREG,		TPOINT,
	SAREG,		TWORD,
		0,	RDEST,
		"	stw AR,0(AL)" COM "indirect assign\n", },
#endif

#if 0
{ ASSIGN,	FOREFF|INAREG,
	SFLD,	TANY,
	SAREG,	TANY,
		NAREG,	RDEST,
		"ZE", },

{ ASSIGN,	FOREFF,
	SFLD,	TANY,
	SAREG,	TANY,
		NAREG,	0,
		"ZE", },
#endif

/* Do not generate memcpy if return from funcall */
#if 0
{ STASG,	INAREG|FOREFF,
	SOREG|SNAME|SAREG,	TPTRTO|TSTRUCT,
	SFUNCALL,	TPTRTO|TSTRUCT,
		0,	RRIGHT,
		"", },
#endif

{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG|SOREG|SNAME,	TPTRTO|TANY,
		NSPECIAL,	RRIGHT,
		"ZQ", },

/*
 * DIV/MOD/MUL 
 */

{ DIV,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TWORD,
		NAREG|NASL,	RESC1,
		"	divw A1,AL,AR\n", },

{ DIV,	INAREG,
	SAREG,	TUWORD|TPOINT,
	SAREG,	TUWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	divwu A1,AL,AR\n", },

{ MOD,	INAREG,
	SAREG,	TSWORD,
	SAREG,	TSWORD,
		NAREG,	RESC1,
		"	divw A1,AL,AR" COM "signed modulo\n"
		"	mullw A1,A1,AR\n"
		"	subf A1,A1,AL\n", },

{ MOD,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TUWORD|TPOINT,
		NAREG,	RESC1,
		"	divwu A1,AL,AR" COM "unsigned modulo\n"
		"	mullw A1,A1,AR\n"
		"	subf A1,A1,AL\n", },

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,		TANY,
		NAREG|NASL,	RESC1,
		"	mulli A1,AL,AR\n", },

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	mullw A1,AL,AR\n", },

{ MUL,	INBREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NBREG,	RESC1,
		"	mullw A1,AL,AR\n"
		"	mulhw U1,AL,AR\n", },

{ MUL,	INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	mullw A1,AL,AR\n"
		"	mulhw U1,AL,AR\n", },

/*
 * Indirection operators.
 */

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG,		TWORD|TPOINT,
		NAREG|NSPECIAL,	RESC1,
		"	lwz A1,AL" COM "word load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG,		TCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "char load\n"
		"	extsb A1,A1\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG,		TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "uchar load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG,		TSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lhz A1,AL" COM "short load\n"
		"	extsh A1,A1\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG,		TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lhz A1,AL" COM "ushort load\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG,		TLONGLONG|TULONGLONG,
		NBREG, RESC1,
		"	lwz A1,AL" COM "64-bit load\n"
		"	lwz U1,UL\n", },

/*
 * Logical/branching operators
 */

/* compare with constant */
{ OPLOG,	FORCC,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SSCON,	TANY,
		0, 	RESCC,
		"	cmpwi AL,AR\n", },

/* compare with constant */
{ OPLOG,	FORCC,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SSCON,	TANY,
		0, 	RESCC,
		"	cmplwi AL,AR\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		0, 	RESCC,
		"	cmpw AL,AR\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
		0, 	RESCC,
		"	cmplw AL,AR\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0, 	0,
		"ZD", },

{ OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"diediedie!", },

/* AND/OR/ER */
{ AND,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	and A1,AL,AR\n", },

{ AND,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	and. A1,AL,AR\n", },

/* AR must be positive */
{ AND,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TCHAR,
	SPCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	andi. A1,AL,AR\n", },

{ AND,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	and A1,AL,AR" COM "64-bit and\n"
		"	and U1,UL,UR\n" },

{ AND,	INBREG|FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SPCON,	TANY,
		NBREG|NBSL,	RESC1|RESCC,
		"	andi. A1,AL,AR" COM "64-bit and with constant\n"
		"	li U1,0\n" },

{ OR,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	or A1,AL,AR\n", },

{ OR,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	or. A1,AL,AR\n", },

{ OR,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	ori A1,AL,AR\n", },

{ OR,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	ori. A1,AL,AR\n", },

{ OR,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	or A1,AL,AR" COM "64-bit or\n"
		"	or U1,UL,UR\n" },

{ OR,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	ori A1,AL,AR" COM "64-bit or with constant\n" },

{ OR,	INBREG|FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1|RESCC,
		"	ori. A1,AL,AR" COM "64-bit or with constant\n" },

{ ER,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	xor A1,AL,AR\n", },

{ ER,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	xor. A1,AL,AR\n", },

{ ER,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	xori A1,AL,AR\n", },

{ ER,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	xori. A1,AL,AR\n", },

{ ER,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	xor A1,AL,AR" COM "64-bit xor\n"
		"	xor U1,UL,UR\n" },

{ ER,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	xori A1,AL,AR" COM "64-bit xor with constant\n" },

{ ER,	INBREG|FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1|RESCC,
		"	xori. A1,AL,AR" COM "64-bit xor with constant\n" },

/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	ba LL\n", },

{ GOTO, 	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	mtctr AL\n"
		"	bctr\n", },

/*
 * Convert LTYPE to reg.
 */

{ OPLTYPE,      INBREG,
        SANY,   	TANY,
        SOREG,		TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
                "	lwz A1,AL" COM "load long from memory\n"
		"	lwz U1,UL\n", },

{ OPLTYPE,      INBREG,
        SANY,   	TANY,
        SNAME,		TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
		"	lis A1," HA16(AL) COM "load long from sname\n"
		"	lwz A1," LO16(AL) "(A1)\n"
		"	lis U1," HA16(UL) "\n"
		"	lwz U1," LO16(UL) "(U1)\n", },

/* load word from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	lwz A1,AL" COM "load word from memory\n", },

/* load word from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TWORD|TPOINT,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load word from sname\n"
		"	lwz A1," LO16(AL) "(A1)\n", },

/* load char from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TCHAR,
		NAREG,	RESC1,
		"	lbz A1,AL" COM "load char from memory\n" },

/* load char from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TCHAR|TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load (u)char from sname\n"
		"	lbz A1," LO16(AL) "(A1)\n", },

/* load uchar from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	lbz A1,AL" COM "load (u)char from memory\n", },

/* load short from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	lha A1,AL" COM "load (u)short from memory\n", },

/* load short from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TSHORT|TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load (u)short from sname\n"
		"	lha A1," LO16(AL) "(A1)\n", },

/* load from 16-bit constant */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SSCON,		TANY,
		NAREG,	RESC1,
		"	li A1,AL" COM "load 16-bit constant\n", },

/* load from 16-bit constant */
{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SSCON,	TANY,
		NBREG,	RESC1,
		"	li A1,AL" COM "load 16-bit constant\n"
		"	li U1,0\n", },

/* load from constant */
{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load constant into register\n"
		"	addi A1,A1," LO16(AL) "\n", },

/* load from constant */
{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SCON,	TANY,
		NBREG,	RESC1,
		"	lis A1," HA16(AL) COM "load constant into register\n"
		"	addi A1,A1," LO16(AL) "\n"
		"	lis U1," HA16(UL) "\n"
		"	addi U1,U1," LO16(UL) "\n", },

/* load from register */
{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	TANY,
		NAREG,	RESC1,
		"	mr A1,AL" COM "load AL into A1\n" },

/* load from register */
{ OPLTYPE,      INBREG,
        SANY,   TANY,
        SBREG,	TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
		"	mr A1,AL" COM "load UL:AL into U1:A1\n"
                "       mr U1,UL\n", },

/*
 * Negate a word.
 */

{ UMINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	neg A1,AL\n", },

{ UMINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	subfic A1,AL,0\n"
		"	subfze U1,UL\n", },

{ COMPL,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	not A1,AL\n", },

{ COMPL,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	not A1,AL"
		"	not U1,UL\n", },

/*
 * Arguments to functions.
 */

#if 0
{ FUNARG,	FOREFF,
	SCON|SAREG|SNAME|SOREG,	TWORD|TPOINT,
	SANY,	TWORD|TPOINT,
		0,	RNULL,
		"	pushl AL\n", },

{ FUNARG,	FOREFF,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RNULL,
		"	pushl AL\n", },

{ FUNARG,	FOREFF,
	SAREG|SNAME|SOREG,	TSHORT,
	SANY,	TSHORT,
		NAREG,	0,
		"	movswl AL,ZN\n	pushl ZN\n", },
#endif

{ FUNARG,	FOREFF,
	SAREG|SNAME|SOREG,	TUSHORT,
	SANY,	TUSHORT,
		NAREG,	0,
		"	movzwl AL,ZN\n	pushl ZN\n", },

#if 0
{ FUNARG,	FOREFF,
	SHCH|SNAME|SOREG,	TCHAR,
	SANY,			TCHAR,
		NAREG,	0,
		"	movsbl AL,A1\n	pushl A1\n", },
#endif

#if 0
{ FUNARG,	FOREFF,
	SHCH|SNAME|SOREG,	TUCHAR,
	SANY,	TUCHAR,
		NAREG,	0,
		"	movzbl AL,A1\n	pushl A1\n", },
#endif

#if 0
{ STARG,	FOREFF,
	SAREG|SOREG|SNAME|SCON,	TANY,
	SANY,	TSTRUCT,
		NSPECIAL|NAREG,	0,
		"ZF", },
#endif

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

{ UMUL, DF( UMUL ), },

{ ASSIGN, DF(ASSIGN), },

{ STASG, DF(STASG), },

{ FLD, DF(FLD), },

{ OPLEAF, DF(NAME), },

/* { INIT, DF(INIT), }, */

{ OPUNARY, DF(UMINUS), },

{ OPANY, DF(BITYPE), },

{ FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
