/*	$OpenBSD: table.c,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
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
 *
 * For each deunsigned type, they look something like this:
 *
 * signed -> bigger signed	- nothing to do
 * signed -> bigger unsigned	- clear the top bits (of source type)
 *
 * signed -> smaller signed	- sign-extend the bits (to dest type)
 * signed -> smaller unsigned	- clear the top bits (of dest type)
 * unsigned -> smaller signed	- sign-extend top bits (to dest type)
 * unsigned -> smaller unsigned	- clear the top bits (of dest type)
 *
 * unsigned -> bigger		- nothing to do
 */

{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TSWORD|TSHORT,
		0,	RLEFT,
		COM "convert char to short/int\n", },

{ SCONV,	INAREG,
	SAREG,	TCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert char to uchar/ushort/uint\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TUCHAR,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sxtb A1,AL" COM "convert uchar to char\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert uchar to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG,
	SAREG,	TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT,
		0,	RLEFT,
		COM "convert uchar to (u)short/(u)int\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TSWORD,
		0,	RLEFT,
		COM "convert short to int\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TSHORT,
	SAREG,	TUWORD,
		NAREG|NASL,	RESC1,
		"	uxth A1,AL" COM "convert short to uint\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT,
	SAREG,	TUWORD,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert short to uint\n"
		"	mov A1,AL,lsr #16\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TUSHORT,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sxth A1,AL" COM "convert ushort to short\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert ushort to short\n"
		"	mov A1,A1,asr #16\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sxtb A1,AL" COM "convert (u)short to char\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)short to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sxtb A1,AL" COM "convert (u)short to char\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert (u)short to uchar\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT,
	SAREG,	TWORD,
		0,	RLEFT,
		COM "convert ushort to (u)int\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TWORD,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sxtb A1,AL" COM "convert (u)int to char\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)int to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TWORD,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sxth A1,AL" COM "convert (u)int to short\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert (u)int to short\n"
		"	mov A1,A1,asr #16\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert uchar to char\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SAREG,	TWORD,
	SAREG,	TUSHORT,
		NAREG|NASL,	RESC1,
		"	uxth A1,AL" COM "convert int to ushort\n", },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TUSHORT,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert int to ushort\n"
		"	mov A1,AL,lsr #16\n", },

{ SCONV,	INAREG,
	SAREG,	TPOINT|TWORD,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		COM "convert between pointers and words\n", },

{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0,	RLEFT,
		COM "convert (u)longlong to (u)longlong\n", },

/* convert (u)char/(u)short/(u)int to longlong */
{ SCONV,	INBREG,
	SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,		RESC1,
		"	mov A1,AL" COM "convert (u)char/(u)short/(u)int to (u)longlong\n"
		"	mov U1,AL,asr #31\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR,
		NAREG,		RESC1,
		"	sxtb A1,AL" COM "convert (u)longlong to char\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR,
		NAREG,		RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)longlong to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT,
		NAREG,		RESC1,
		"	sxth A1,AL" COM "convert (u)longlong to short\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TSHORT,
		NAREG,		RESC1,
		"	mov A1,AL,asl #16" COM "convert (u)longlong to short\n"
		"	mov A1,A1,asr #16\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TWORD,
		NAREG,		RESC1,
		"	mov A1,AL" COM "convert (u)longlong to (u)int\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUCHAR,
		NAREG,		RESC1,
		"	and A1,AL,#255" COM "convert (u)longlong to uchar\n", },

{ SCONV,	INAREG | FEATURE_EXTEND,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUSHORT,
		NAREG,		RESC1,
		"	uxth A1,AL" COM "convert (u)longlong to ushort\n", },

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TUSHORT,
		NAREG,		RESC1,
		"	mov A1,AL,asl #16" COM "convert (u)longlong to ushort\n"
		"	mov A1,A1,lsr #16\n", },

/* conversions on load from memory */

/* char */
{ SCONV,	INAREG,
	SOREG,	TCHAR,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrsb A1,AL" COM "convert char to int/long\n", },

/* uchar */
{ SCONV,	INAREG,
	SOREG,	TUCHAR,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrb A1,AL" COM "convert uchar to int/long\n", },
 
/* short */
{ SCONV,	INAREG | FEATURE_HALFWORDS,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrsh A1,AL" COM "convert short to int/long\n", },

/* ushort */
{ SCONV,	INAREG | FEATURE_HALFWORDS,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrh A1,AL" COM "convert ushort to int/long\n", },

/* short */
{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TWORD,
		2*NAREG|NASL,	RESC1,
		"ZH", },

{ SCONV,	INAREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SAREG,	TWORD,
		NAREG,		RESC1,
		"	fix AL,AR" COM "convert float to int\n", },

{ SCONV,	INAREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SAREG,	TSWORD,
		NAREG,		RESC1,
		"	ftosis AL,AR" COM "convert float to int\n", },

{ SCONV,	INAREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SAREG,	TSWORD,
		NAREG,		RESC1,
		"	ftouis AL,AR" COM "convert float to int\n", },

{ SCONV,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TWORD,
		NSPECIAL|NAREG,		RESC1,
		"ZF", },

{ SCONV,	INBREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SBREG,	TULONGLONG|TLONGLONG,
		NBREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SBREG,	TULONGLONG|TLONGLONG,
		NBREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG,
	SAREG,	TFLOAT,
	SBREG,	TULONGLONG|TLONGLONG,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INAREG | FEATURE_FPA,
	SCREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TWORD,
		NAREG,		RESC1,
		"	fix AL,AR" COM "convert double/ldouble to int\n", },

{ SCONV,	INAREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TSWORD,
		NAREG,		RESC1,
		"	ftosid AL,AR" COM "convert double/ldouble to int\n", },

{ SCONV,	INAREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TUWORD,
		NAREG,		RESC1,
		"	ftouid AL,AR" COM "convert double/ldouble to int\n", },

{ SCONV,	INAREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TWORD,
		NSPECIAL|NAREG,		RESC1,
		"ZF", },

{ SCONV,	INBREG | FEATURE_FPA,
	SCREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TULONGLONG|TLONGLONG,
		NBREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TULONGLONG|TLONGLONG,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SAREG,	TWORD,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		"	flts AL,AR" COM "convert int to float\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TSWORD,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		"	fsitos AL,AR" COM "convert int to float\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TUWORD,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		"	fuitos AL,AR" COM "convert int to float\n" },

{ SCONV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SBREG,	TULONGLONG|TLONGLONG,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_VFP,
	SBREG,	TULONGLONG|TLONGLONG,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INAREG,
	SBREG,	TULONGLONG|TLONGLONG,
	SAREG,	TFLOAT,
		NAREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_FPA,
	SAREG,	TWORD,
	SCREG,	TDOUBLE,
		NCREG,		RESC1,
		"	fltd AL,AR" COM "convert int to double\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TSWORD,
	SCREG,	TDOUBLE,
		NCREG,		RESC1,
		"	fsitod AL,AR" COM "convert int to double\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TUWORD,
	SCREG,	TDOUBLE,
		NCREG,		RESC1,
		"	fuitod AL,AR" COM "convert int to double\n" },

{ SCONV,	INBREG,
	SAREG,	TWORD,
	SBREG,	TDOUBLE,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TDOUBLE,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_VFP,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TDOUBLE,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TDOUBLE,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SAREG,	TWORD,
	SCREG,	TLDOUBLE,
		NCREG,		RESC1,
		"	flte AL,AR" COM "convert int to ldouble\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TSWORD,
	SCREG,	TLDOUBLE,
		NCREG,		RESC1,
		"	fsitod AL,AR" COM "convert int to ldouble\n" },

{ SCONV,	INCREG | FEATURE_VFP,
	SAREG,	TUWORD,
	SCREG,	TLDOUBLE,
		NCREG,		RESC1,
		"	fuitod AL,AR" COM "convert uint to ldouble\n" },

{ SCONV,	INBREG,
	SAREG,	TWORD,
	SBREG,	TLDOUBLE,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TLDOUBLE,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_VFP,
	SBREG,	TLONGLONG|TULONGLONG,
	SCREG,	TLDOUBLE,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLDOUBLE,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TFLOAT,
		NCREG,		RESC1,
		"	fcvtds AL,AR" COM "convert float to double\n" },

{ SCONV,	INAREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,		RESC1,
		COM "unimplemented\n", },

{ SCONV,	INCREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TDOUBLE|TLDOUBLE,
		NCREG,		RESC1,
		"	fcvtsd AL,AR" COM "convert float to double\n" },

{ SCONV,	INBREG,
	SAREG,	TFLOAT,
	SBREG,	TDOUBLE|TLDOUBLE,
		NSPECIAL|NBREG,		RESC1,
		"ZF", },

{ SCONV,	INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,		RLEFT,
		COM "convert (l)double to (l)double", },

{ SCONV,	INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,		RLEFT,
		COM "convert (l)double to (l)double", },

{ SCONV,	INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		0,		RLEFT,
		COM "convert (l)double to (l)double", },

/*
 * Subroutine calls.
 */

{ CALL,		FOREFF,
	SCON|SNAME,	TANY,
	SANY,		TANY,
		0,	0,
		"	bl CL" COM "call (args, no result) to scon/sname (CL)\n"
		"ZC", },

{ UCALL,	FOREFF,
	SCON|SNAME,	TANY,
	SANY,		TANY,
		0,	0,
		"	bl CL" COM "call (no args, no result) to scon/sname (CL)\n", },

{ CALL,		INAREG,
	SCON|SNAME,	TANY,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r0) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INBREG,
	SCON|SNAME,	TANY,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r0:r1) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INCREG | FEATURE_FPA,
	SCON|SNAME,	TANY,
	SCREG,		TFLOAT,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result r0) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INCREG | FEATURE_FPA,
	SCON|SNAME,	TANY,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r0:r1) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INAREG,
	SCON|SNAME,	TANY,
	SAREG,		TFLOAT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result r0) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INBREG,
	SCON|SNAME,	TANY,
	SBREG,		TDOUBLE|TLDOUBLE,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result in r0:r1) to scon/sname (CL)\n"
		"ZC", },

{ UCALL,	INAREG,
	SCON|SNAME,	TANY,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r0) to scon/sname (CL)\n", },

{ UCALL,	INBREG,
	SCON|SNAME,	TANY,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r0:r1) to scon/sname (CL)\n", },

{ UCALL,	INCREG | FEATURE_FPA,
	SCON|SNAME,	TANY,
	SCREG,		TFLOAT,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r0) to scon/sname (CL)\n", },

{ UCALL,	INCREG | FEATURE_FPA,
	SCON|SNAME,	TANY,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG|NCSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r0:r1) to scon/sname (CL)\n", },

{ CALL,		FOREFF,
	SAREG,	TANY,
	SANY,		TANY,
		0,	0,
		"	mov lr,pc\n"
		"	mov pc,AL\n"
		"ZC", },

{ UCALL,	FOREFF,
	SAREG,	TANY,
	SANY,		TANY,
		0,	0,
		"	mov lr,pc\n"
		"	mov pc,AL\n", },

{ CALL,		INAREG,
	SAREG,	TANY,
	SANY,		TANY,
		INAREG,	RESC1,
		"	mov lr,pc\n"
		"	mov pc,AL\n"
		"ZC", },

{ UCALL,	INAREG,
	SAREG,	TANY,
	SANY,		TANY,
		INAREG,	RESC1,
		"	mov lr,pc\n"
		"	mov pc,AL\n", },

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
	SNAME|SAREG,	TANY,
	SANY,		TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	mov lr,pc\n"
		"	mov pc,AL\n", },

{ STCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	bl CL\n"
		"ZC", },

{ STCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL\n"
		"ZC", },

{ STCALL,	INAREG,
	SNAME|SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	mov lr,pc\n"
		"	mov pc,AL\n"
		"ZC", },

/*
 * The next rules handle all binop-style operators.
 */

{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT,
	SCCON,	TANY,
		NAREG,	RESC1,
		"	add A1,AL,AR" COM "addition of constant\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SSCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	adds A1,AL,AR" COM "64-bit addition of constant\n"
		"	adc U1,UL,UR\n", },

{ PLUS,		INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	add A1,AL,AR" COM "addition\n", },

{ PLUS,		INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	adds A1,AL,AR" COM "64-bit addition\n"
		"	adc U1,UL,UR\n", },

{ PLUS,		INCREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	adfs A1,AL,AR" COM "float add\n", },

{ PLUS,		INCREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	fadds A1,AL,AR" COM "float add\n", },

{ PLUS,		INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ PLUS,		INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	adfd A1,AL,AR" COM "double add\n", },

{ PLUS,		INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	faddd A1,AL,AR" COM "double add\n", },

{ PLUS,		INBREG,
	SBREG,	TDOUBLE,
	SBREG,	TDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ PLUS,		INCREG | FEATURE_FPA,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	adfe A1,AL,AR" COM "ldouble add\n", },

{ PLUS,		INCREG | FEATURE_VFP,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	faddd A1,AL,AR" COM "ldouble add\n", },

{ PLUS,		INBREG,
	SBREG,	TLDOUBLE,
	SBREG,	TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SCCON,	TANY,
		NAREG|NASL,	RESC1,
		"	sub A1,AL,AR" COM "subtraction of constant\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	sub A1,AL,AR" COM "subtraction\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCCON,	TANY,
		NBREG|NBSL,	RESC1,
		"	subs A1,AL,AR" COM "64-bit subtraction of constant\n"
		"	rsc  U1,UL,AR\n", },

{ MINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	subs A1,AL,AR" COM "64-bit subtraction\n"
		"	sbc  U1,UL,AR\n", },

{ MINUS,	INCREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	sufs A1,AL,AR" COM "float subtraction\n", },

{ MINUS,	INCREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	fsubs A1,AL,AR" COM "float subtraction\n", },

{ MINUS,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ MINUS,	INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	sufd A1,AL,AR" COM "double subtraction\n", },

{ MINUS,	INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	fsubd A1,AL,AR" COM "double subtraction\n", },

{ MINUS,	INBREG,
	SBREG,	TDOUBLE,
	SBREG,	TDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ MINUS,	INCREG | FEATURE_FPA,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	sufe A1,AL,AR" COM "ldouble subtraction\n", },

{ MINUS,	INCREG | FEATURE_VFP,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	fsubd A1,AL,AR" COM "double subtraction\n", },

{ MINUS,	INBREG,
	SBREG,	TLDOUBLE,
	SBREG,	TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

/*
 * The next rules handle all shift operators.
 */

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl AR" COM "left shift\n", },

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCCON,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl AR" COM "left shift by constant\n", },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZO" },

{ LS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TANY,
		NSPECIAL|NBREG,	RESC1,
		"ZE" },

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asr AR" COM "right shift\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,lsr AR" COM "right shift\n", },

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SCCON,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asr AR" COM "right shift by constant\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SCCON,	TANY,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,lsr AR" COM "right shift by constant\n", },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZO" },

{ RS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TANY,
		NSPECIAL|NBREG,	RESC1,
		"ZE" },


/*
 * The next rules takes care of assignments. "=".
 */

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TWORD|TPOINT,
	SAREG,		TWORD|TPOINT,
		0,	RDEST,
		"	str AR,AL" COM "assign word\n", },

{ ASSIGN,	FOREFF|INBREG,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		0,	RDEST,
		"	str AR,AL" COM "assign 64-bit value\n"
		"	str UR,UL\n", },

/* XXX don't know if this works */
{ ASSIGN,	FOREFF|INBREG,
	SAREG,		TPTRTO|TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		0,	RDEST,
		"	stmdb AL,{AR-UR}" COM "assign 64-bit value\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		0,	RDEST,
		"	strb AR,AL" COM "assign (u)char\n", },

{ ASSIGN,	FOREFF|INAREG | FEATURE_HALFWORDS,
	SOREG|SNAME,	TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		0,	RDEST,
		"	strh AR,AL" COM "assign (u)short\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		NAREG|NASL,	RDEST,
		"ZH", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_FPA,
	SOREG|SNAME,	TFLOAT,
	SCREG,		TFLOAT,
		0,	RDEST,
		"	stfs AR,AL" COM "assign float\n", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_VFP,
	SOREG|SNAME,	TFLOAT,
	SCREG,		TFLOAT,
		0,	RDEST,
		COM "unimplemented\n", },

{ ASSIGN, 	FOREFF|INAREG,
	SOREG|SNAME,	TFLOAT,
	SAREG,		TFLOAT,
		0,	RDEST,
		"	str AR,AL" COM "assign float (soft-float)\n", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_FPA,
	SOREG|SNAME,	TDOUBLE,
	SCREG,		TDOUBLE,
		0,	RDEST,
		"	stfd AR,AL" COM "assign double\n", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_VFP,
	SOREG|SNAME,	TDOUBLE,
	SCREG,		TDOUBLE,
		0,	RDEST,
		COM "unimplemented\n", },

{ ASSIGN, 	FOREFF|INBREG,
	SOREG|SNAME,	TDOUBLE,
	SBREG,		TDOUBLE,
		0,	RDEST,
		"	str AR,AL" COM "assign double (soft-float)\n"
		"	str UR,UL\n", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_FPA,
	SOREG|SNAME,	TLDOUBLE,
	SCREG,		TLDOUBLE,
		0,	RDEST,
		"	stfe AR,AL" COM "assign ldouble\n", },

{ ASSIGN, 	FOREFF|INCREG | FEATURE_VFP,
	SOREG|SNAME,	TLDOUBLE,
	SCREG,		TLDOUBLE,
		0,	RDEST,
		COM "not implemented", },

{ ASSIGN, 	FOREFF|INBREG,
	SOREG|SNAME,	TLDOUBLE,
	SBREG,		TLDOUBLE,
		0,	RDEST,
		"	str AR,AL" COM "assign ldouble (soft-float)\n"
		"	str UR,UL\n", },

/* assign register to register */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	RDEST,
		"	mov AL,AR" COM "assign AR to AL\n", },

{ ASSIGN,      FOREFF|INBREG,
        SBREG,	TLONGLONG|TULONGLONG,
        SBREG,	TLONGLONG|TULONGLONG,
                0,	RDEST,
		"	mov AL,AR" COM "assign UR:AR to UL:AL\n"
                "	mov UL,UR\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		0,	RDEST,
		"	mvf AL,AR" COM "assign float reg to float reg\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		0,	RDEST,
		"	fcpys AL,AR" COM "assign float reg to float reg\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		0,	RDEST,
		"	mov AL,AR" COM "assign float reg to float reg\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	mvf AL,AR" COM "assign float reg to float reg\n", },

{ ASSIGN,	FOREFF|INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE|TLDOUBLE,
	SCREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	fcpyd AL,AR" COM "assign float reg to float reg\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"	mov AL,AR" COM "assign (l)double reg to (l)double reg\n"
		"	mov UL,UR\n", },

{ ASSIGN,	FOREFF|INAREG,
	SFLD,		TANY,
	SOREG|SNAME,	TANY,
		3*NAREG,	RDEST,
		"	ldr A1,AR" COM "bit-field assignment\n"
		"	ldr A2,AL\n"
		"	ldr A3,=M\n"
		"	mov A1,A1,asl H\n"
		"	and A1,A1,A3\n"
		"	bic A2,A2,A3\n"
		"	orr A3,A2,A1\n"
		"	str A3,AL\n"
		"F	ldr AD,AR\n"
		"FZB", },

{ ASSIGN,	FOREFF|INAREG,
	SFLD,	TANY,
	SAREG,	TANY,
		3*NAREG,	RDEST,
		"	ldr A2,AL" COM "bit-field assignment\n"
		"	ldr A3,=M\n"
		"	mov A1,AR,asl H\n"
		"	and A1,A1,A3\n"
		"	bic A2,A2,A3\n"
		"	orr A3,A2,A1\n"
		"	str A3,AL\n"
		"F	mov AD,AR\n"
		"FZB", },

{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG,	TPTRTO|TANY,
		NSPECIAL,	RRIGHT,
		"ZQ", },

/*
 * DIV/MOD/MUL 
 */

{ DIV,	INAREG,
	SAREG,	TWORD,
	SAREG,	TWORD,
		NSPECIAL|NAREG|NASL,	RESC1,
		"ZE", },

{ DIV,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG|NBSL,	RESC1,
		"ZE", },

{ DIV,	INCREG | FEATURE_FPA,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG,	RESC1,
		"	dvfs A1,AL,AL" COM "fast (float) divide\n", },

{ DIV,	INCREG | FEATURE_VFP,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG,	RESC1,
		"	fdivs A1,AL,AL" COM "fast (float) divide\n", },

{ DIV,	INAREG,
	SAREG,		TFLOAT,
	SAREG,		TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ DIV,	INCREG | FEATURE_FPA,
	SCREG,		TDOUBLE,
	SCREG,		TDOUBLE,
		NCREG,	RESC1,
		"	dvfd A1,AL,AL" COM "double divide\n", },

{ DIV,	INCREG | FEATURE_VFP,
	SCREG,		TDOUBLE,
	SCREG,		TDOUBLE,
		NCREG,	RESC1,
		"	fdivd A1,AL,AL" COM "double divide\n", },

{ DIV,	INBREG,
	SBREG,		TDOUBLE,
	SBREG,		TDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ DIV,	INCREG | FEATURE_FPA,
	SCREG,		TLDOUBLE,
	SCREG,		TLDOUBLE,
		NCREG,	RESC1,
		"	dvfe A1,AL,AR" COM "long double load\n", },

{ DIV,	INCREG | FEATURE_VFP,
	SCREG,		TLDOUBLE,
	SCREG,		TLDOUBLE,
		NCREG,	RESC1,
		"	fdivd A1,AL,AL" COM "double divide\n", },

{ DIV,	INBREG,
	SBREG,		TLDOUBLE,
	SBREG,		TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ MOD,	INAREG,
	SAREG,	TWORD,
	SAREG,	TWORD,
		NSPECIAL|NAREG,	RESC1,
		"ZE", },

{ MOD,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ MUL,	INAREG | FEATURE_MUL,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG,	RESC1,
		"	mul A1,AL,AR\n", },

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NSPECIAL|NAREG,	RESC1,
		"ZE", },

{ MUL,	INBREG | FEATURE_MULL,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
		NBREG,	RESC1,
		"	smull U1,A1,AL,AR\n", },

{ MUL,	INBREG | FEATURE_MUL,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
		NBREG,	RESC1,
		"	mul A1,AL,AR\n"
		"	mov U1,A1,asr #31\n", },

{ MUL,	INBREG,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ MUL,	INBREG | FEATURE_MULL,
	SAREG,		TSWORD|TSHORT|TCHAR,
	SAREG,		TSWORD|TSHORT|TCHAR,
		NBREG,	RESC1,
		"	umull U1,A1,AL,AR\n", },

{ MUL,	INBREG | FEATURE_MUL,
	SAREG,		TSWORD|TSHORT|TCHAR,
	SAREG,		TSWORD|TSHORT|TCHAR,
		NBREG,	RESC1,
		"	mul A1,AL,AR\n"
		"	mov U1,#0\n", },

{ MUL,	INBREG,
	SAREG,		TSWORD|TSHORT|TCHAR,
	SAREG,		TSWORD|TSHORT|TCHAR,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ MUL,	INBREG | FEATURE_MULL,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	umull U1,A1,AL,AR\n", },

{ MUL,	INBREG | FEATURE_MUL,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	mul A1,AL,AR\n"
		"	mov U1,A1,asr #31\n", },

{ MUL,	INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
		"ZE", },

{ MUL,	INCREG | FEATURE_FPA,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG,	RESC1,
		"	fmls A1,AL,AL" COM "fast (float) multiply\n", },

{ MUL,	INCREG | FEATURE_VFP,
	SCREG,		TFLOAT,
	SCREG,		TFLOAT,
		NCREG,	RESC1,
		"	fmuls A1,AL,AL" COM "float multiply\n", },

{ MUL,	INAREG,
	SAREG,		TFLOAT,
	SAREG,		TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ MUL,	INCREG | FEATURE_FPA,
	SCREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	mufd A1,AL,AL" COM "fast (l)double multiply\n", },

{ MUL,	INCREG | FEATURE_VFP,
	SCREG,		TDOUBLE|TLDOUBLE,
	SCREG,		TDOUBLE|TLDOUBLE,
		NCREG,	RESC1,
		"	muld A1,AL,AL" COM "(l)double multiply\n", },

{ MUL,	INBREG,
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
		NAREG,	RESC1,
		"	ldr A1,AL" COM "word load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TCHAR,
		NAREG,	RESC1,
		"	ldrsb A1,AL" COM "char load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUCHAR,
		NAREG,	RESC1,
		"	ldrb A1,AL" COM "uchar load\n", },

{ UMUL,	INAREG | FEATURE_HALFWORDS,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG,	RESC1,
		"	ldrh A1,AL" COM "short load\n", },

{ UMUL,	INAREG | FEATURE_HALFWORDS,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG,	RESC1,
		"	ldrsh A1,AL" COM "short load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT|TUSHORT,
		2*NAREG|NASL,	RESC1,
		"ZH", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "64-bit load\n"
		"	ldr U1,UL\n", },

{ UMUL, INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		"	ldfs A1,AL" COM "float load\n", },

{ UMUL, INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		COM "not implemented\n", },

{ UMUL, INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NAREG,	RESC1,
		"	ldr A1,AL" COM "float load\n", },

{ UMUL, INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NCREG,	RESC1,
		"	ldfd A1,AL" COM "double load\n", },

{ UMUL, INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NCREG,	RESC1,
		COM "not implemented\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "double load\n"
		"	ldr U1,UL\n", },

{ UMUL, INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NCREG,	RESC1,
		"	ldfe A1,AL" COM "long double load\n", },

{ UMUL, INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NCREG,	RESC1,
		COM "not implemented\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "long double load (soft-float)\n"
		"	ldr U1,UL\n", },

/*
 * Logical/branching operators
 */

/* compare with register */
{ OPLOG,	FORCC,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		0, 	RESCC,
		"	cmp AL,AR" COM "AR-AL (sets flags)\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TPOINT|TUSHORT|TUCHAR,
		0, 	RESCC,
		"	cmp AL,AR" COM "AR-AL (sets flags)\n", },

/* compare with register */
{ OPLOG,	FORCC,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		0, 	RESCC,
		"ZD", },

{ OPLOG,	FORCC | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NSPECIAL,	RESCC,
		"	cmfs AL,AR" COM "float compare\n", },

{ OPLOG,	FORCC | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		0,	RESCC,
		"	fcmps AL,AR" COM "float compare\n", },

{ OPLOG,	FORCC,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL,	RESCC,
		"ZF", },

{ OPLOG,	FORCC | FEATURE_FPA,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NSPECIAL,	RESCC,
		"	cmfd AL,AR" COM "double compare\n", },

{ OPLOG,	FORCC | FEATURE_VFP,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		0,	RESCC,
		"	fcmpd AL,AR" COM "double compare\n", },

{ OPLOG,	FORCC,
	SBREG,	TDOUBLE,
	SBREG,	TDOUBLE,
		NSPECIAL,	RESCC,
		"ZF", },

{ OPLOG,	FORCC | FEATURE_FPA,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NSPECIAL,	RESCC,
		"	cmfe AL,AR" COM "ldouble compare\n", },

{ OPLOG,	FORCC | FEATURE_VFP,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		0,	RESCC,
		"	fcmpd AL,AR" COM "double compare\n", },

{ OPLOG,	FORCC,
	SBREG,	TLDOUBLE,
	SBREG,	TLDOUBLE,
		NSPECIAL,	RESCC,
		"ZF", },

/* AND/OR/ER */
{ AND,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1|RESCC,
		"	and A1,AL,AR" COM "64-bit and\n"
		"	and U1,UL,UR\n", },

{ OR,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	orr A1,AL,AR" COM "64-bit or\n"
		"	orr U1,UL,UR\n" },

{ ER,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	eor A1,AL,AR" COM "64-bit xor\n"
		"	eor U1,UL,UR\n" },

{ OPSIMP,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1|RESCC,
		"	O A1,AL,AR\n", },

{ OPSIMP,	INAREG|FORCC,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	Os A1,AL,AR\n", },


/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	b LL\n", },

#if 0
{ GOTO, 	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	mov pc,AL\n", },
#endif

/*
 * Convert LTYPE to reg.
 */

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	ldr A1,AL" COM "load word from memory\n", },

{ OPLTYPE,      INBREG,
        SANY,   	TANY,
        SOREG|SNAME,	TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
                "	ldr A1,AL" COM "load long long from memory\n"
		"	ldr U1,UL\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TCHAR,
		NAREG,	RESC1,
		"	ldrsb A1,AL" COM "load char from memory\n" },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUCHAR,
		NAREG,	RESC1,
		"	ldrb A1,AL" COM "load uchar from memory\n", },

{ OPLTYPE,	INAREG | FEATURE_HALFWORDS,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG,	RESC1,
		"	ldrsh A1,AL" COM "load short from memory\n", },

{ OPLTYPE,	INAREG | FEATURE_HALFWORDS,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG,	RESC1,
		"	ldrh A1,AL" COM "load ushort from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT|TUSHORT,
		2*NAREG,	RESC1,
		"ZH", },

#if 0
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SCON,		TPOINT,
		NAREG,	RESC1,
		"	ldr A1,AL" COM "load integer constant\n", },
#endif

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SCON,		TANY,
		NAREG,	RESC1,
		"ZI", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SCON,	TANY,
		NBREG,	RESC1,
		"ZJ", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	TANY,
		NAREG,	RESC1,
		"	mov A1,AL" COM "load AL into A1\n" },

{ OPLTYPE,      INBREG,
        SANY,   TANY,
        SBREG,	TLONGLONG|TULONGLONG,
                NBREG,  RESC1,
		"	mov A1,AL" COM "load UL:AL into U1:A1\n"
                "       mov U1,UL\n", },

{ OPLTYPE,	INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		"	ldfs A1,AL" COM "load float\n", },

{ OPLTYPE,	INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NCREG,	RESC1,
		COM "not implemented\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NAREG,	RESC1,
		"	ldr A1,AL" COM "load float (soft-float)\n", },

{ OPLTYPE,	INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NCREG,	RESC1,
		"	ldfd A1,AL" COM "load double\n", },

{ OPLTYPE,	INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NCREG,	RESC1,
		COM "not implemented\n" },

{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "load double (soft-float)\n"
		"	ldr U1,UL\n", },

{ OPLTYPE,	INCREG | FEATURE_FPA,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NCREG,	RESC1,
		"	ldfe A1,AL" COM "load ldouble\n", },

{ OPLTYPE,	INCREG | FEATURE_VFP,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NCREG,	RESC1,
		COM "not implemented\n", },

{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "load ldouble (soft-float)\n"
		"	ldr U1,UL\n", },

/*
 * Negate a word.
 */

{ UMINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	rsb A1,AL,#0" COM "negation\n", },

{ UMINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	rsbs A1,AL,#0" COM "64-bit negation\n"
		"	rsc U1,UL,#0\n", },

{ UMINUS,	INCREG | FEATURE_FPA,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	mvfs A1,AL" COM "float negation\n", },

{ UMINUS,	INCREG | FEATURE_VFP,
	SCREG,	TFLOAT,
	SCREG,	TFLOAT,
		NCREG,	RESC1,
		"	negs A1,AL" COM "float negation\n", },

{ UMINUS,	INAREG,
	SAREG,	TFLOAT,
	SAREG,	TFLOAT,
		NSPECIAL|NAREG,	RESC1,
		"ZF", },

{ UMINUS,	INCREG | FEATURE_FPA,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	mvfd A1,AL" COM "double negation\n", },

{ UMINUS,	INCREG | FEATURE_VFP,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	negd A1,AL" COM "double negation\n", },

{ UMINUS,	INBREG,
	SBREG,	TDOUBLE,
	SBREG,	TDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ UMINUS,	INCREG | FEATURE_FPA,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	mvfe A1,AL" COM "ldouble negation\n", },

{ UMINUS,	INCREG | FEATURE_VFP,
	SCREG,	TLDOUBLE,
	SCREG,	TLDOUBLE,
		NCREG,	RESC1,
		"	negd A1,AL" COM "ldouble negation\n", },

{ UMINUS,	INBREG,
	SBREG,	TLDOUBLE,
	SBREG,	TLDOUBLE,
		NSPECIAL|NBREG,	RESC1,
		"ZF", },

{ COMPL,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	mvn A1,AL" COM "complement\n", },

{ COMPL,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	mvn A1,AL" COM "64-bit complement\n"
		"	mvn U1,UL\n", },

/*
 * Arguments to functions.
 */

{ FUNARG,       FOREFF,
        SAREG,  TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SANY,   TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
                0,      0,
		"	stmfd sp!,{AL}" COM "save function arg to stack\n", },

{ FUNARG,       FOREFF,
        SBREG,  TLONGLONG|TULONGLONG,
        SANY,	TLONGLONG|TULONGLONG,
                0,      0,
		"	stmfd sp!,{AL,UL}" COM "save function arg to stack (endianness problem here?)\n", },

{ FUNARG,	FOREFF,
	SCREG,	TFLOAT,
	SANY,	TFLOAT,
		0,	0,
		"	stmfd sp!,{AL}" COM "save function arg to stack\n", },

{ FUNARG,       FOREFF,
        SCREG,  TDOUBLE|TLDOUBLE,
        SANY,  TDOUBLE|TLDOUBLE,
                0,      0,
		"	stmfd sp!,{AL,UL}" COM "save function arg to stack (endianness problem here?)\n", },

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
