/*	$OpenBSD: table.c,v 1.10 2008/04/11 20:45:52 stefan Exp $	*/
/*-
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

#if defined(ELFABI)
#define HA16(x)	# x "@ha"
#define LO16(x)	# x "@l"
#elif defined(MACHOABI)
#define HA16(x)	"ha16(" # x ")"
#define LO16(x)	"lo16(" # x ")"
#else
#error undefined ABI
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

{ SCONV,	INAREG,
	SAREG,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		0,	RLEFT,
		COM "convert between (u)char and (u)char\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT,
		0,	RLEFT,
		COM "convert between (u)short and (u)short\n", },

{ SCONV,	INAREG,
	SAREG,	TPOINT|TWORD,
	SAREG,	TWORD,
		0,	RLEFT,
		COM "convert a pointer/word to an int\n", },

{ SCONV,	INAREG,
	SAREG,	TPOINT,
	SAREG,	TPOINT,
		0,	RLEFT,
		COM "convert pointers\n", },

{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0,	RLEFT,
		COM "convert (u)longlong to (u)longlong\n", },

{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TSHORT|TSWORD,
		NASL|NAREG,	RESC1,
		"	extsb A1,AL" COM "convert char to short/int\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TSHORT|TSWORD,
		0,	RLEFT,
		COM "convert uchar to short/int\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TUSHORT|TUWORD,
		0,	RLEFT,
		COM "convert uchar to ushort/unsigned\n", },

/* XXX is this necessary? */
{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TUSHORT|TUWORD,
		NSPECIAL|NAREG|NASL,	RESC1,
		"	extsb A1,AL" COM "convert char to ushort/unsigned\n", },

{ SCONV,	INBREG | FEATURE_BIGENDIAN,
	SAREG,	TUCHAR|TUSHORT|TUNSIGNED,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,		RESC1,
		"	mr U1,AL" COM "convert uchar/ushort/uint to (u)longlong\n"
		"	li A1,0\n", },

{ SCONV,	INBREG,
	SAREG,	TUCHAR|TUSHORT|TUNSIGNED,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,		RESC1,
		"	mr A1,AL" COM "convert uchar/ushort/uint to (u)longlong\n"
		"	li U1,0\n", },

{ SCONV,	INBREG | FEATURE_BIGENDIAN,
	SAREG,	TCHAR|TSHORT|TSWORD,
	SBREG,	TULONGLONG|TLONGLONG,
		NBREG,		RESC1,
		"	mr U1,AL" COM "convert char/short/int to ulonglong\n"
		"	srawi A1,AL,31\n", },

{ SCONV,	INBREG,
	SAREG,	TCHAR|TSHORT|TSWORD,
	SBREG,	TULONGLONG|TLONGLONG,
		NBREG,		RESC1,
		"	mr A1,AL" COM "convert char/short/int to ulonglong\n"
		"	srawi U1,AL,31\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR|TUCHAR,
		NSPECIAL|NAREG|NASL,	RESC1,
		"	andi. A1,AL,255" COM "convert (u)short to (u)char\n", },

/* XXX is this really necessary? */
{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TWORD,
		NAREG|NASL,	RESC1,
		"	extsh A1,AL" COM "convert short to int\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TWORD,
		NSPECIAL|NAREG|NASL,	RESC1,
		COM "convert ushort to word\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	andi. A1,AL,255" COM "convert (u)int to (u)char\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TSHORT|TUSHORT,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	andi. A1,AL,65535" COM "convert (u)int to (u)short\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR|TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	andi. A1,AL,255" COM "(u)longlong to (u)char\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT|TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	andi. A1,AL,65535" COM "(u)longlong to (u)short\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD,
		NAREG,		RESC1,
		"	mr A1,AL" COM "convert (u)longlong to (u)int/long\n", },

/* conversions on load from memory */

{ SCONV,	INAREG,
	SOREG,	TCHAR,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "convert char to int/long\n"
		"	extsb A1,A1\n", },

{ SCONV,	INAREG,
	SOREG,	TUCHAR,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "convert uchar to int/long\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lha A1,AL" COM "convert short to int/long\n", },

{ SCONV,	INAREG,
	SOREG,	TUSHORT,
	SAREG,	TWORD,
		NASL|NAREG|NSPECIAL,	RESC1,
		"	lhz A1,AL" COM "convert ushort to int/long\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR|TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lwz A1,AL" COM "(u)longlong to (u)char\n"
		"	andi. A1,A1,255\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT|TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lwz A1,AL" COM "(u)longlong to (u)short\n"
		"	andi. A1,A1,65535\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD,
		NAREG|NSPECIAL,	RESC1,
		"	lwz A1,AL" COM "(u)longlong to (u)int\n", },

/*
 * floating-point conversions
 *
 * There doesn't appear to be an instruction to move values between
 * the floating-point registers and the general-purpose registers.
 * So values are bounced into memory...
 */

{ SCONV,	INCREG | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		COM "convert float to (l)double\n", },

/* soft-float */
{ SCONV,	INBREG,
	SAREG,	TFLOAT,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_HARDFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	frsp A1,AL" COM "convert (l)double to float\n", },

/* soft-float */
{ SCONV,	INAREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_HARDFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		COM "convert (l)double to (l)double\n", },

/* soft-float */
{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		COM "convert (l)double to (l)double (soft-float)\n", },

{ SCONV,	INCREG | FEATURE_HARDFLOAT,
	SAREG,	TWORD,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		2*NCREG|NAREG,	RESC3,
		"ZC", },

/* soft-float */
{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INBREG,
	SAREG,	TWORD,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ SCONV,	INAREG | FEATURE_HARDFLOAT,
	SOREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SAREG,	TWORD,
		2*NCREG|NAREG,	RESC1,
		"ZC", },

/* soft-float */
{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TWORD,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INAREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TWORD,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },
	
{ SCONV,	INCREG | FEATURE_HARDFLOAT,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		NSPECIAL|NCREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ SCONV,	INBREG | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INBREG,
	SAREG,	TFLOAT,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

/* soft-float */
{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

/*
 * Subroutine calls.
 */

{ CALL,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL" COM "call (args, no result) to scon\n", },

{ UCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL" COM "call (no args, no result) to scon\n", },

{ CALL,		INAREG,
	SCON,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result) to scon\n", },

{ UCALL,	INAREG,
	SCON,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result) to scon\n", },

{ CALL,		INBREG,
	SCON,	TANY,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result) to scon\n", },

{ UCALL,	INBREG,
	SCON,	TANY,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result) to scon\n", },

{ CALL,		INCREG | FEATURE_HARDFLOAT,
	SCON,	TANY,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result) to scon\n", },

{ UCALL,	INCREG | FEATURE_HARDFLOAT,
	SCON,	TANY,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result) to scon\n", },

{ CALL,		INAREG,
	SCON,	TANY,
	SAREG,	TFLOAT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result) to scon\n", },

{ UCALL,	INAREG,
	SCON,	TANY,
	SAREG,	TFLOAT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result) to scon\n", },

{ CALL,		INBREG,
	SCON,	TANY,
	SBREG,	TDOUBLE|TLDOUBLE,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result) to scon\n", },

{ UCALL,	INBREG,
	SCON,	TANY,
	SBREG,	TDOUBLE|TLDOUBLE,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result) to scon\n", },



{ CALL,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	mtctr AL" COM "call (args, no result) to reg\n"
		"	bctrl\n", },

{ UCALL,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"	mtctr AL" COM "call (no args, no result) to reg\n"
		"	bctrl\n", },

{ CALL,		INAREG,
	SAREG,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	mtctr AL" COM "call (args, result) to reg\n"
		"	bctrl\n", },

{ UCALL,	INAREG,
	SAREG,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	mtctr AL" COM "call (no args, result) to reg\n"
		"	bctrl\n", },

/* struct return */
{ USTCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL\n", },

{ USTCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ USTCALL,	INAREG,
	SAREG,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	mtctr AL"
		"	bctrl\n", },

{ STCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL\n", },

{ STCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL\n", },

{ STCALL,	INAREG,
	SAREG,	TANY,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	mtctr AL"
		"	bctrl\n", },

/*
 * The next rules handle all binop-style operators.
 */

/* XXX AL cannot be R0 */
{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	addi A1,AL,AR" COM "addition of constant\n", },

/* XXX AL cannot be R0 */
{ PLUS,		INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	addic. A1,AL,AR" COM "addition of constant\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	addic A1,AL,AR" COM "64-bit addition of constant\n"
		"	addze U1,UL\n", },

{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	add A1,AL,AR\n", },

{ PLUS,		INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	add. A1,AL,AR\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	addc A1,AL,AR" COM "64-bit add\n"
		"	adde U1,UL,UR\n", },

{ PLUS,		INCREG | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		"	fadds A1,AL,AR" COM "float add\n", },

{ PLUS,		INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ PLUS,		INCREG | FEATURE_HARDFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,
		"	fadd A1,AL,AR" COM "(l)double add\n", },

/* soft-float */
{ PLUS,		INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG|NBSL,	RESC1,
		"ZF", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	addi A1,AL,-AR\n", },

{ MINUS,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	addic. A1,AL,-AR\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	addic A1,AL,-AR\n"
		"	addme U1,UL\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	subf A1,AR,AL\n", },

{ MINUS,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	subf. A1,AR,AL\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	subfc A1,AR,AL" COM "64-bit subtraction\n"
		"	subfe U1,UR,UL\n", },

{ MINUS,	INCREG | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	fsubs A1,AL,AR\n", },

{ MINUS,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ MINUS,		INCREG | FEATURE_HARDFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,
		"	fsub A1,AL,AR" COM "(l)double sub\n", },

/* soft-float */
{ MINUS,		INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG|NBSL,	RESC1,
		"ZF", },


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
		"ZO", },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TANY,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srw A1,AL,AR" COM "right shift\n", },

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sraw A1,AL,AR" COM "arithmetic right shift\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srw. A1,AL,AR" COM "right shift\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sraw. A1,AL,AR" COM "arithmetic right shift\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srwi A1,AL,AR" COM "right shift by constant\n", },

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srawi A1,AL,AR" COM "arithmetic right shift by constant\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srwi. A1,AL,AR" COM "right shift by constant\n", },

{ RS,	INAREG|FORCC,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	srawi. A1,AL,AR" COM "right shift by constant\n", },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZO" },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TANY,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

/*
 * The next rules takes care of assignments. "=".
 */

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,		TANY,
		0,	RDEST,
		"	li AL,AR\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SSCON,		TANY,
		0,	RDEST,
		"	li AL,AR\n"
		"	li UL,UR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,		TANY,
		0,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	addi AL,AL," LO16(AR) "\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SCON,		TANY,
		0,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	addi AL,AL," LO16(AR) "\n"
		"	lis UL," HA16(UR) "\n"\
		"	addi UL,UL," LO16(UR) "\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SOREG,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "assign oreg to reg\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SNAME,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign sname to reg\n"
		"	lwz AL," LO16(AR) "(AL)\n", },

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

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SOREG,		TSWORD,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "load int/pointer into llong\n"
		"	srawi UL,AR,31\n" },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SOREG,		TUNSIGNED|TPOINT,
		NSPECIAL,	RDEST,
		"	lwz AL,AR" COM "load uint/pointer into (u)llong\n"
		"	li UL,0\n" },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUCHAR,
	SOREG,		TUCHAR,
		NSPECIAL,	RDEST,
		"	lbz AL,AR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TUCHAR,
	SNAME,		TUCHAR,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign uchar sname to reg\n"
		"	lbz AL," LO16(AR) "(AL)\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TCHAR,
	SOREG,		TCHAR,
		NSPECIAL,	RDEST,
		"	lbz AL,AR\n"
		"	extsb AL,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TCHAR,
	SNAME,		TCHAR,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) COM "assign char sname to reg\n"
		"	lbz AL," LO16(AR) "(AL)\n"
		"	extsb AL,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SOREG,		TSHORT,
		NSPECIAL,	RDEST,
		"	lha AL,AR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT,
	SOREG,		TUSHORT,
		NSPECIAL,	RDEST,
		"	lhz AL,AR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD,
	SNAME,		TSHORT,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	lha AL," LO16(AR) "(AL)\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD,
	SNAME,		TUSHORT,
		NSPECIAL,	RDEST,
		"	lis AL," HA16(AR) "\n"
		"	lhz AL," LO16(AR) "(AL)\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		NSPECIAL,	RDEST,
		"	stw AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) COM "assign reg to sname\n"
		"	stw AR," LO16(AL) "(A1)\n", },

{ ASSIGN,	FOREFF|INBREG,
	SOREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL,	RDEST,
		"	stw AR,AL" COM "store 64-bit value\n"
		"	stw UR,UL\n", },

{ ASSIGN,	FOREFF|INBREG,
	SNAME,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) COM "assign reg to 64-bit sname\n"
		"	stw AR," LO16(AL) "(A1)\n"
		"	lis U1," HA16(UL) "\n"
		"	stw UR," LO16(UL) "(U1)\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		NSPECIAL,	RDEST,
		"	stb AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	stb AR," LO16(AL) "(A1)\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		NSPECIAL,	RDEST,
		"	sth AR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		NAREG|NSPECIAL,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	sth AR," LO16(AL) "(A1)\n", },

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

{ ASSIGN,	FOREFF|INAREG,
	SFLD,		TANY,
	SAREG,		TANY,
		3*NAREG,	RDEST,
		"	lis A3," HA16(M) COM "bit-field assignment\n"
		"	addi A3,A3," LO16(M) "\n"
		"	lwz A2,AL\n"
		"	slwi A1,AR,H\n"
		"	and A1,A1,A3\n"
		"	not A3,A3\n"
		"	and A2,A2,A3\n"
		"	or A2,A2,A1\n"
		"	stw A2,AL\n"
		"F	mr AD,AR\n"
		"F	slwi AD,AD,32-S\n"
		"F	srwi AD,AD,32-S\n", },

{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG,		TPTRTO|TANY,
		NSPECIAL,	RRIGHT,
		"ZQ", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SOREG,		TFLOAT,
	SCREG,		TFLOAT,
		0,	RDEST,
		"	stfs AR,AL" COM "store float\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TFLOAT,
	SAREG,		TFLOAT,
		0,	RDEST,
		"	stw AR,AL" COM "store float (soft-float)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SNAME,		TFLOAT,
	SCREG,		TFLOAT,
		NAREG,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	stfs AR," LO16(AL) "(A1)\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INAREG,
	SNAME,		TFLOAT,
	SAREG,		TFLOAT,
		NAREG,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	stw AR," LO16(AL) "(A1)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TFLOAT,
	SOREG,		TFLOAT,
		0,	RDEST,
		"	lfs AL,AR" COM "load float\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TFLOAT,
	SOREG,		TFLOAT,
		0,	RDEST,
		"	lwz AL,AR" COM "load float (soft-float)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TFLOAT,
	SNAME,		TFLOAT,
		NAREG,	RDEST,
		"	lis A1," HA16(AR) "\n"
		"	lfs AL," LO16(AR) "(A1)\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TFLOAT,
	SNAME,		TFLOAT,
		NAREG,	RDEST,
		"	lis A1," HA16(AR) "\n"
		"	lwz AL," LO16(AR) "(A1)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		0,	RDEST,
		"	fmr AL,AR" COM "assign AR to AL\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TFLOAT,
	SAREG,		TFLOAT,
		0,	RDEST,
		"	mr AL,AR" COM "assign AR to AL\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SOREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	stfd AR,AL" COM "store (l)double\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INBREG,
	SOREG,		TDOUBLE|TLDOUBLE,
	SBREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	stw AR,AL" COM "store (l)double (soft-float)\n"
		"	stw UR,UL\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SNAME,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		NAREG,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	stfd AR," LO16(AL) "(A1)\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INBREG,
	SNAME,		TDOUBLE|TLDOUBLE,
	SBREG,		TDOUBLE|TLDOUBLE,
		NAREG,	RDEST,
		"	lis A1," HA16(AL) "\n"
		"	stw AR," LO16(AL) "(A1)\n"
		"	lis A1," HA16(UL) "\n"
		"	stw UR," LO16(UL) "(A1)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TDOUBLE|TLDOUBLE,
	SOREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	lfd AL,AR" COM "load (l)double\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TDOUBLE|TLDOUBLE,
	SOREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	lwz AL,AR" COM "load (l)double (soft-float)\n"
		"	lwz UL,UR\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TDOUBLE|TLDOUBLE,
	SNAME,		TDOUBLE|TLDOUBLE,
		NAREG,	RDEST,
		"	lis A1," HA16(AR) "\n"
		"	lfd AL," LO16(AR) "(A1)\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TDOUBLE|TLDOUBLE,
	SNAME,		TDOUBLE|TLDOUBLE,
		NAREG,	RDEST,
		"	lis A1," HA16(AR) "\n"
		"	lwz AL," LO16(AR) "(A1)\n"
		"	lis A1," HA16(UR) "\n"
		"	lwz UL," LO16(UR) "(A1)\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_HARDFLOAT,
	SCREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	fmr AL,AR" COM "assign AR to AL\n", },

/* soft-float */
{ ASSIGN,	FOREFF|INBREG,
	SBREG,		TDOUBLE|TLDOUBLE,
	SBREG,		TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	mr AL,AR" COM "assign AR to AL\n"
		"	mr UL,UR\n", },

/*
 * DIV/MOD/MUL 
 */

{ DIV,	INAREG,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
		"	divwu A1,AL,AR\n", },

{ DIV,	INAREG|FORCC,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1|RESCC,
		"	divwu. A1,AL,AR\n", },

{ DIV,	INAREG,
	SAREG,	TWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TCHAR,
		NAREG|NASL,	RESC1,
		"	divw A1,AL,AR\n", },

{ DIV,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TCHAR,
		NAREG|NASL,	RESC1|RESCC,
		"	divw. A1,AL,AR\n", },

{ DIV,	INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ DIV, INCREG | FEATURE_HARDFLOAT,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG|NCSR,	RESC1,
		"	fdivs A1,AL,AR" COM "float divide\n", },

/* soft-float */
{ DIV, INAREG,
	SAREG,		TFLOAT,
	SAREG,		TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ DIV, INCREG | FEATURE_HARDFLOAT,
	SCREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG|NCSR,	RESC1,
		"	fdiv A1,AL,AR" COM "(l)double divide\n", },

/* soft-float */
{ DIV, INBREG,
	SBREG,		TDOUBLE|TLDOUBLE,
	SBREG,		TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ MOD,	INAREG,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
		NAREG,	RESC1,
		"	divwu A1,AL,AR" COM "unsigned modulo\n"
		"	mullw A1,A1,AR\n"
		"	subf A1,A1,AL\n", },

{ MOD,	INAREG,
	SAREG,	TWORD|TSHORT|TCHAR,
	SAREG,	TWORD|TSHORT|TCHAR,
		NAREG,	RESC1,
		"	divw A1,AL,AR" COM "signed modulo\n"
		"	mullw A1,A1,AR\n"
		"	subf A1,A1,AL\n", },

{ MOD,	INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,		TANY,
		NAREG|NASL,	RESC1,
		"	mulli A1,AL,AR\n", },

{ MUL,	INAREG|FORCC,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,		TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	mulli. A1,AL,AR\n", },

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	mullw A1,AL,AR\n", },

{ MUL,	INAREG|FORCC,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1|RESCC,
		"	mullw. A1,AL,AR\n", },

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

{ MUL, INCREG | FEATURE_HARDFLOAT,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG|NCSR,	RESC1,
		"	fmuls A1,AL,AR" COM "float multiply\n", },

/* soft-float */
{ MUL, INAREG,
	SAREG,		TFLOAT,
	SAREG,		TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ MUL, INCREG | FEATURE_HARDFLOAT,
	SCREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG|NCSR,	RESC1,
		"	fmul A1,AL,AR" COM "(l)double multiply\n", },

/* soft-float */
{ MUL, INBREG,
	SBREG,		TDOUBLE|TLDOUBLE,
	SBREG,		TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

/*
 * Indirection operators.
 */

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TWORD|TPOINT,
		NAREG|NSPECIAL,	RESC1,
		"	lwz A1,AL" COM "word load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "char load\n"
		"	extsb A1,A1\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lbz A1,AL" COM "uchar load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lha A1,AL" COM "short load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lhz A1,AL" COM "ushort load\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	lwz A1,AL" COM "64-bit load\n"
		"	lwz U1,UL\n", },

{ UMUL, INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		"	lfs A1,AL" COM "float load\n", },

{ UMUL, INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NAREG,	RESC1,
		"	lwz A1,AL" COM "float load (soft-float)\n", },

{ UMUL, INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	lfd A1,AL" COM "(l)double load\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"	lwz A1,AL" COM "(l)double load (soft-float)\n"
		"	lwz U1,UL\n", },

#if 0
{ UMUL, INAREG,
	SANY,		TANY,
	SAREG,		TWORD|TPOINT,
		NAREG,	RESC1,
		"	lwz A1,(AL)" COM "word load\n", },
#endif

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
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		0, 	RESCC,
		"	cmplw AL,AR\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0, 	RESCC,
		"ZD", },

/* compare with register */
{ OPLOG,	FORCC | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
		0,	RESCC,
		"	fcmpu 0,AL,AR\n", },

/* soft-float */
{ OPLOG,	FORCC,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL,	RESCC,
		"ZF\n", },

/* soft-float */
{ OPLOG,	FORCC,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL,	RESCC,
		"ZF", },

{ OPLOG,	FORCC,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"diediedie!", },

/* AND/OR/ER */
{ AND,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	and A1,AL,AR\n", },

{ AND,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	and. A1,AL,AR\n", },

/* AR must be positive */
{ AND,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
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
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	or A1,AL,AR\n", },

{ OR,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	or. A1,AL,AR\n", },

{ OR,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SPCON,	TANY,
		NAREG|NASL,	RESC1,
		"	ori A1,AL,AR\n", },

{ OR,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SPCON,	TANY,
		NAREG|NASL,	RESC1|RESCC,
		"	ori. A1,AL,AR\n", },

{ OR,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	or A1,AL,AR" COM "64-bit or\n"
		"	or U1,UL,UR\n" },

{ OR,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SPCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	ori A1,AL,AR" COM "64-bit or with constant\n" },

{ OR,	INBREG|FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SPCON,	TANY,
		NBREG|NBSL,	RESC1|RESCC,
		"	ori. A1,AL,AR" COM "64-bit or with constant\n" },

{ ER,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	xor A1,AL,AR\n", },

{ ER,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL|NSPECIAL,	RESC1|RESCC,
		"	xor. A1,AL,AR\n", },

{ ER,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SPCON,	TANY,
		NAREG|NASL|NSPECIAL,	RESC1,
		"	xori A1,AL,AR\n", },

{ ER,	INAREG|FORCC,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SPCON,	TANY,
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
	SPCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	xori A1,AL,AR" COM "64-bit xor with constant\n" },

{ ER,	INBREG|FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SPCON,	TANY,
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

#if defined(ELFABI)
{ OPLTYPE,	INAREG | FEATURE_PIC,
	SANY,		TANY,
	SNAME,		TANY,
		NAREG,	RESC1,
		"	lwz A1,AL" COM "elfabi pic load\n", },
#endif

{ OPLTYPE,      INBREG,
        SANY,   	TANY,
        SOREG,		TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
                "	lwz A1,AL" COM "load llong from memory\n"
		"	lwz U1,UL\n", },

{ OPLTYPE,      INBREG,
        SANY,   	TANY,
        SNAME,		TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
		"	lis A1," HA16(AL) COM "load llong from sname\n"
		"	lwz A1," LO16(AL) "(A1)\n"
		"	lis U1," HA16(UL) "\n"
		"	lwz U1," LO16(UL) "(U1)\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TWORD|TPOINT,
		NAREG,	RESC1,
		"	lwz A1,AL" COM "load word from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TWORD|TPOINT,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load word from sname\n"
		"	lwz A1," LO16(AL) "(A1)\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TCHAR,
		NAREG,	RESC1,
		"	lbz A1,AL" COM "load char from memory\n"
		"	extsb A1,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load char from sname\n"
		"	lbz A1," LO16(AL) "(A1)\n"
		"	extsb A1,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TUCHAR,
		NAREG,	RESC1,
		"	lbz A1,AL" COM "load uchar from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TUCHAR,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load uchar from sname\n"
		"	lbz A1," LO16(AL) "(A1)\n", },

/* load short from memory */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TSHORT,
		NAREG,	RESC1,
		"	lha A1,AL" COM "load short from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TUSHORT,
		NAREG,	RESC1,
		"	lhz A1,AL" COM "load ushort from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load short from sname\n"
		"	lha A1," LO16(AL) "(A1)\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TUSHORT,
		NAREG|NSPECIAL,	RESC1,
		"	lis A1," HA16(AL) COM "load ushort from sname\n"
		"	lhz A1," LO16(AL) "(A1)\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SSCON,		TANY,
		NAREG,	RESC1,
		"	li A1,AL" COM "load 16-bit constant\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SSCON,	TANY,
		NBREG,	RESC1,
		"	li A1,AL" COM "load 16-bit constant\n"
		"	li U1,UL\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANY,
		NAREG|NASL,	RESC1,
		"	lis A1," HA16(AL) COM "load constant into register\n"
		"	addi A1,A1," LO16(AL) "\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SCON,	TANY,
		NBREG,	RESC1,
		"	lis A1," HA16(AL) COM "load constant into register\n"
		"	addi A1,A1," LO16(AL) "\n"
		"	lis U1," HA16(UL) "\n"
		"	addi U1,U1," LO16(UL) "\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	TANY,
		NAREG,	RESC1,
		"	mr A1,AL" COM "load AL into A1\n" },

{ OPLTYPE,      INBREG,
        SANY,   TANY,
        SBREG,	TANY,
                NBREG,  RESC1,
		"	mr A1,AL" COM "load UL:AL into U1:A1\n"
                "       mr U1,UL\n", },

{ OPLTYPE,      INCREG,
        SANY,   TANY,
        SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
                NCREG,  RESC1,
		"	fmr A1,AL" COM "load AL into A1\n", },

{ OPLTYPE,	INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SOREG,		TFLOAT,
		NCREG,	RESC1,
		"	lfs A1,AL" COM "load float\n", },

/* soft-float */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG,		TFLOAT,
		NAREG,	RESC1,
		"	lwz A1,AL" COM "load float (soft-float)\n", },

{ OPLTYPE,	INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SNAME,		TFLOAT,
		NCREG|NAREG,	RESC2,
		"	lis A1," HA16(AL) COM "load sname\n"
		"	lfs A2," LO16(AL) "(A1)\n", },

/* soft-float */
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SNAME,		TFLOAT,
		NAREG,	RESC1,
		"	lis A1," HA16(AL) COM "load sname (soft-float)\n"
		"	lwz A1," LO16(AL) "(A1)\n", },

{ OPLTYPE,	INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SOREG,		TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	lfd A1,AL" COM "load (l)double\n", },

/* soft-float */
{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SOREG,		TDOUBLE|TLDOUBLE,
		NBREG,	RESC1,
		"	lwz A1,AL" COM "load (l)double (soft-float)\n"
		"	lwz U1,UL\n", },

{ OPLTYPE,	INCREG | FEATURE_HARDFLOAT,
	SANY,		TANY,
	SNAME,		TDOUBLE|TLDOUBLE,
		NCREG|NAREG,	RESC2,
		"	lis A1," HA16(AL) COM "load sname\n"
		"	lfd A2," LO16(AL) "(A1)\n", },

{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SNAME,		TDOUBLE|TLDOUBLE,
		NBREG,	RESC1,
		"	lis A1," HA16(AL) COM "load sname (soft-float)\n"
		"	lwz A1," LO16(AL) "(A1)\n"
		"	lis U1," HA16(UL) "\n"
		"	lwz U1," LO16(UL) "(U1)\n", },


/*
 * Negate a word.
 */

{ UMINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	neg A1,AL\n", },

{ UMINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	subfic A1,AL,0\n"
		"	subfze U1,UL\n", },

{ UMINUS,	INCREG | FEATURE_HARDFLOAT,
	SCREG,	TFLOAT|TDOUBLE|TLDOUBLE,
	SANY,	TANY,
		NCREG|NCSL,	RESC1,
		"	fneg A1,AL\n", },

{ UMINUS,	INAREG,
	SAREG,	TFLOAT,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	xoris A1,AL,0x8000" COM "(soft-float)\n", },

{ UMINUS,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	xoris U1,UL,0x8000" COM "(soft-float)\n"
		"	mr A1,AL\n", },

{ COMPL,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	not A1,AL\n", },

{ COMPL,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	not A1,AL\n"
		"	not U1,UL\n", },

/*
 * Arguments to functions.
 */

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
