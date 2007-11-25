/*	$OpenBSD: table.c,v 1.1 2007/11/25 18:45:06 otto Exp $	*/
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

#if defined(ARM_HAS_FPA) || defined(ARM_HAS_VFP)
#define INFREG	INCREG
#define NFREG	NCREG
#define SFREG	SCREG
#define NFSL	NCSL
#define NFSR	NCSR
#define INXREG	INCREG
#define NXREG	NCREG
#define SXREG	SCREG
#define NXSL	NCSL
#define NXSR	NCSR
#else
#define INFREG	INAREG
#define NFREG	NAREG
#define SFREG	SAREG
#define NFSL	NASL
#define NFSR	NASR
#define INXREG	INBREG
#define NXREG	NBREG
#define SXREG	SBREG
#define NXSL	NBSL
#define NXSR	NBSR
#endif

#define COM	"	@ "

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
	INAREG,	TCHAR,
	INAREG,	TSWORD|TSHORT,
		NAREG|NASL,	RESC1,
		COM "convert char to short/int\n", },

{ SCONV,	INAREG,
	INAREG,	TCHAR,
	INAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert char to uchar/ushort/uint\n", },

{ SCONV,	INAREG,
	INAREG,	TUCHAR,
	INAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert uchar to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG,
	INAREG,	TUCHAR,
	INAREG,	TWORD|TSHORT|TUSHORT,
		0,	RLEFT,
		COM "convert uchar to (u)short/(u)int\n", },

{ SCONV,	INAREG,
	INAREG,	TSHORT,
	INAREG,	TSWORD,
		0,	RLEFT,
		COM "convert short to int\n", },

{ SCONV,	INAREG,
	INAREG,	TSHORT,
	INAREG,	TUWORD,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert short to uint\n"
		"	mov A1,AL,lsr #16\n", },

{ SCONV,	INAREG,
	INAREG,	TUSHORT,
	INAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert ushort to short\n"
		"	mov A1,A1,asr #16\n", },

{ SCONV,	INAREG,
	INAREG,	TSHORT|TUSHORT,
	INAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)short to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG,
	INAREG,	TSHORT|TUSHORT,
	INAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert (u)short to uchar\n", },

{ SCONV,	INAREG,
	INAREG,	TUSHORT,
	INAREG,	TWORD,
		NAREG|NASL,	RESC1,
		COM "convert ushort to (u)int\n", },

{ SCONV,	INAREG,
	INAREG,	TWORD,
	INAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)int to char\n"
		"	mov A1,A1,asr #24\n", },

{ SCONV,	INAREG,
	INAREG,	TWORD,
	INAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asl #16" COM "convert (u)int to short\n"
		"	mov A1,A1,asr #16\n", },

{ SCONV,	INAREG,
	INAREG,	TWORD,
	INAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	and A1,AL,#255" COM "convert uchar to char\n", },

{ SCONV,	INAREG,
	INAREG,	TWORD,
	INAREG,	TUSHORT,
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

{ SCONV,	INAREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR,
		NAREG,		RESC1,
		"	mov A1,AL,asl #24" COM "convert (u)longlong to char\n"
		"	mov A1,A1,asr #24\n", },

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
{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrsh A1,AL" COM "convert short to int/long\n", },

/* ushort */
{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"	ldrh A1,AL" COM "convert ushort to int/long\n", },

{ SCONV,	INAREG,
	SFREG,	TFLOAT,
	SAREG,	TSWORD,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fix AL,AR" COM "convert float to int\n", },
#elifdef ARM_HAS_VFP
		"	ftosis AL,AR" COM "convert float to int\n", },
#else
		"ZF", },
#endif

{ SCONV,	INAREG,
	SFREG,	TFLOAT,
	SAREG,	TUWORD,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fix AL,AR" COM "convert float to int\n", },
#elifdef ARM_HAS_VFP
		"	ftouis AL,AR" COM "convert float to int\n", },
#else
		"ZF", },
#endif

{ SCONV,	INBREG,
	SFREG,	TFLOAT,
	SBREG,	TULONGLONG,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INBREG,
	SFREG,	TFLOAT,
	SBREG,	TLONGLONG,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INAREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TSWORD,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fix AL,AR" COM "convert double/ldouble to int\n", },
#elifdef ARM_HAS_VFP
		"	ftosid AL,AR" COM "convert double/ldouble to int\n", },
#else
		"ZF", },
#endif

{ SCONV,	INAREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SAREG,	TUWORD,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fix AL,AR" COM "convert double/ldouble to int\n", },
#elifdef ARM_HAS_VFP
		"	ftouid AL,AR" COM "convert double/ldouble to int\n", },
#else
		"ZF", },
#endif

{ SCONV,	INBREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TULONGLONG,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INBREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SBREG,	TLONGLONG,
		NSPECIAL|NAREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SAREG,	TSWORD,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	flts AL,AR" COM "convert int to float\n" },
#elifdef ARM_HAS_VFP
		"	fsitos AL,AR" COM "convert int to float\n" },
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SAREG,	TUWORD,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	flts AL,AR" COM "convert int to float\n" },
#elifdef ARM_HAS_VFP
		"	fuitos AL,AR" COM "convert int to float\n" },
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SBREG,	TLONGLONG,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SBREG,	TULONGLONG,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SAREG,	TSWORD,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fltd AL,AR" COM "convert int to double\n" },
#elifdef ARM_HAS_VFP
		"	fsitod AL,AR" COM "convert int to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SAREG,	TUWORD,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	fltd AL,AR" COM "convert int to double\n" },
#elifdef ARM_HAS_VFP
		"	fuitod AL,AR" COM "convert int to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SBREG,	TLONGLONG,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SBREG,	TULONGLONG,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SAREG,	TSWORD,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	flte AL,AR" COM "convert int to ldouble\n" },
#elifdef ARM_HAS_VFP
		"	fsitod AL,AR" COM "convert int to ldouble\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SAREG,	TUWORD,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
		"	flte AL,AR" COM "convert int to ldouble\n" },
#elifdef ARM_HAS_VFP
		"	fuitod AL,AR" COM "convert int to ldouble\n" },
#else
		"ZF", },
#endif


{ SCONV,	INXREG,
	SBREG,	TLONGLONG,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SBREG,	TULONGLONG,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SXREG,	TDOUBLE,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
		"	fcvtds AL,AR" COM "convert float to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INFREG,
	SXREG,	TLDOUBLE,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
		"	fcvtds AL,AR" COM "convert float to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SFREG,	TFLOAT,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
		"	fcvtsd AL,AR" COM "convert float to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SFREG,	TFLOAT,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,		RESC1,
#ifdef ARM_HAS_FPA
#elifdef ARM_HAS_VFP
		"	fcvtsd AL,AR" COM "convert float to double\n" },
#else
		"ZF", },
#endif

{ SCONV,	INXREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SXREG,	TDOUBLE|TLDOUBLE,
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

{ CALL,		INFREG,
	SCON|SNAME,	TANY,
	SFREG,		TFLOAT,
		NFREG|NASL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (args, result r0) to scon/sname (CL)\n"
		"ZC", },

{ CALL,		INXREG,
	SCON|SNAME,	TANY,
	SXREG,		TDOUBLE|TLDOUBLE,
		NXREG|NXSL,	RESC1,	/* should be 0 */
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

{ UCALL,	INFREG,
	SCON|SNAME,	TANY,
	SFREG,		TFLOAT,
		NFREG|NFSL,	RESC1,	/* should be 0 */
		"	bl CL" COM "call (no args, result in r0) to scon/sname (CL)\n", },

{ UCALL,	INXREG,
	SCON|SNAME,	TANY,
	SXREG,		TDOUBLE|TLDOUBLE,
		NXREG|NXSL,	RESC1,	/* should be 0 */
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

/* struct return */
{ USTCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		NAREG|NASL,	0,
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
		NAREG|NASL,	0,
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

{ PLUS,		INFREG,
	SFREG,	TFLOAT,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	adfs A1,AL,AR" COM "float add\n", },
#elifdef ARM_HAS_VFP
		"	fadds A1,AL,AR" COM "float add\n", },
#else
		"ZF", },
#endif

{ PLUS,		INXREG,
	SXREG,	TDOUBLE,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	adfd A1,AL,AR" COM "double add\n", },
#elifdef ARM_HAS_VFP
		"	faddd A1,AL,AR" COM "double add\n", },
#else
		"ZF", },
#endif

{ PLUS,		INXREG,
	SXREG,	TLDOUBLE,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	adfe A1,AL,AR" COM "ldouble add\n", },
#elifdef ARM_HAS_VFP
		"	faddd A1,AL,AR" COM "ldouble add\n", },
#else
		"ZF", },
#endif

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SCCON,	TANY,
		NAREG|NASL,	RESC1,
		"	sub A1,AL,AR" COM "subtraction of constant\n", },

{ MINUS,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	sub A1,AR,AL" COM "subtraction\n", },

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

{ MINUS,	INFREG,
	SFREG,	TFLOAT,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	sufs A1,AL,AR" COM "float subtraction\n", },
#elifdef ARM_HAS_VFP
		"	fsubs A1,AL,AR" COM "float subtraction\n", },
#else
		"ZF", },
#endif

{ MINUS,	INXREG,
	SXREG,	TDOUBLE,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	sufd A1,AL,AR" COM "double subtraction\n", },
#elifdef ARM_HAS_VFP
		"	fsubd A1,AL,AR" COM "double subtraction\n", },
#else
		"ZF", },
#endif

{ MINUS,	INXREG,
	SXREG,	TLDOUBLE,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	sufe A1,AL,AR" COM "ldouble subtraction\n", },
#elifdef ARM_HAS_VFP
		"	fsubd A1,AL,AR" COM "double subtraction\n", },
#else
		"ZF", },
#endif

/*
 * The next rules handle all shift operators.
 */

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
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

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		NAREG|NASL,	RESC1,
		"	mov A1,AL,asr AR" COM "right shift\n", },

{ RS,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
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

{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		0,	RDEST,
		"	strh AR,AL" COM "assign (u)short\n", },

{ ASSIGN, 	FOREFF|INFREG,
	SOREG|SNAME,	TFLOAT,
	SFREG,		TFLOAT,
		0,	RDEST,
#ifdef ARM_HAS_FPA
		"	stfs AR,AL" COM "assign float\n", },
#elifdef ARM_HAS_VFP
#else
		"	str AR,AL" COM "assign float\n", },
#endif

{ ASSIGN, 	FOREFF|INXREG,
	SOREG|SNAME,	TDOUBLE,
	SXREG,		TDOUBLE,
		0,	RDEST,
#ifdef ARM_HAS_FPA
		"	stfd AR,AL" COM "assign double\n", },
#elifdef ARM_HAS_VFP
#else
		"	str AR,AL" COM "assign double\n"
		"	str UR,UL\n", },
#endif

{ ASSIGN, 	FOREFF|INXREG,
	SOREG|SNAME,	TLDOUBLE,
	SXREG,		TLDOUBLE,
		0,	RDEST,
#ifdef ARM_HAS_FPA
		"	stfe AR,AL" COM "assign ldouble\n", },
#elifdef ARM_HAS_VFP
#else
		"	str AR,AL" COM "assign ldouble\n"
		"	str UR,UL\n", },
#endif

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

{ ASSIGN,	FOREFF|INFREG,
	SFREG,	TFLOAT,
	SFREG,	TFLOAT,
		0,	RDEST,
#ifdef ARM_HAS_FPA
		"	mvf AL,AR" COM "assign float reg to float reg\n", },
#elifdef ARM_HAS_VFP
		"	fcpys AL,AR" COM "assign float reg to float reg\n", },
#else
		"	mov AL,AR" COM "assign float reg to float reg\n", },
#endif

{ ASSIGN,	FOREFF|INXREG,
	SXREG,	TDOUBLE|TLDOUBLE,
	SXREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
#ifdef ARM_HAS_FPA
		"	mvf AL,AR" COM "assign float reg to float reg\n", },
#elifdef ARM_HAS_VFP
		"	fcpyd AL,AR" COM "assign float reg to float reg\n", },
#else
		"	mov AL,AR" COM "assign (l)double reg to (l)double reg\n"
		"	mov UL,UR\n", },
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

{ DIV,	INFREG,
	SFREG,		TFLOAT,
	SFREG,		TFLOAT,
		NSPECIAL|NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	dvfs A1,AL,AL" COM "fast (float) divide\n", },
#elifdef ARM_HAS_VFP
		"	fdivs A1,AL,AL" COM "fast (float) divide\n", },
#else
		"ZF", },
#endif

{ DIV,	INXREG,
	SXREG,		TDOUBLE,
	SXREG,		TDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	dvfd A1,AL,AL" COM "double divide\n", },
#elifdef ARM_HAS_VFP
		"	fdivd A1,AL,AL" COM "double divide\n", },
#else
		"ZF", },
#endif

{ DIV,	INXREG,
	SXREG,		TLDOUBLE,
	SXREG,		TLDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	dvfe A1,AL,AL" COM "ldouble divide\n", },
#elifdef ARM_HAS_VFP
		"	fdivd A1,AL,AL" COM "double divide\n", },
#else
		"ZF", },
#endif

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

{ MUL,	INAREG,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NSPECIAL|NAREG,	RESC1,
		"	mul A1,AL,AR\n", },

{ MUL,	INBREG,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
	SAREG,		TUWORD|TPOINT|TUSHORT|TUCHAR,
		NSPECIAL|NBREG,	RESC1,
#ifdef ARM_HAS_MULL
		"	smull U1,A1,AL,AR\n", },
#else
		"	mul A1,AL,AR\n"
		"	mul U1,AL,AR\n", },
#endif

{ MUL,	INBREG,
	SAREG,		TSWORD|TSHORT|TCHAR,
	SAREG,		TSWORD|TSHORT|TCHAR,
		NSPECIAL|NBREG,	RESC1,
#ifdef ARM_HAS_MULL
		"	umull U1,A1,AL,AR\n", },
#else
		"	mul A1,AL,AR\n"
		"	mul U1,AL,AR\n", },
#endif

{ MUL,	INBREG,
	SBREG,		TLONGLONG|TULONGLONG,
	SBREG,		TLONGLONG|TULONGLONG,
		NSPECIAL|NBREG,	RESC1,
#ifdef ARM_HAS_MULL
		"	umull U1,A1,AL,AR\n", },
#else
		"	mul A1,AL,AR\n"
		"	mul U1,AL,AR\n", },
#endif

{ MUL,	INFREG,
	SFREG,		TFLOAT,
	SFREG,		TFLOAT,
		NSPECIAL|NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	fmls A1,AL,AL" COM "fast (float) multiply\n", },
#elifdef ARM_HAS_VFP
		"	fmuls A1,AL,AL" COM "float multiply\n", },
#else
		"ZF", },
#endif

{ MUL,	INXREG,
	SXREG,		TDOUBLE|TLDOUBLE,
	SXREG,		TDOUBLE|TLDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	mufd A1,AL,AL" COM "fast (l)double multiply\n", },
#elifdef ARM_HAS_VFP
		"	muld A1,AL,AL" COM "(l)double multiply\n", },
#else
		"ZF", },
#endif

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

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG,	RESC1,
		"	ldrh A1,AL" COM "short load\n", },

{ UMUL,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG,	RESC1,
		"	ldrsh A1,AL" COM "short load\n", },

{ UMUL, INBREG,
	SANY,		TANY,
	SOREG|SNAME,	TLONGLONG|TULONGLONG,
		NBREG,	RESC1,
		"	ldr A1,AL" COM "64-bit load\n"
		"	ldr U1,UL\n", },

{ UMUL, INFREG,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfs A1,AL" COM "float load\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "float load\n", },
#endif

{ UMUL, INXREG,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfd AL" COM "double load\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "double load\n"
		"	ldr U1,UL\n", },
#endif

{ UMUL, INXREG,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfe AL" COM "long double load\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "long double load\n"
		"	ldr U1,UL\n", },
#endif

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

{ OPLOG,	FORCC,
	SFREG,	TFLOAT,
	SFREG,	TFLOAT,
		NSPECIAL,	RESCC,
#ifdef ARM_HAS_FPA
		"	cmfs AL,AR" COM "float compare\n", },
#elifdef ARM_HAS_VFP
		"	fcmps AL,AR" COM "float compare\n", },
#else
		"ZF", },
#endif

{ OPLOG,	FORCC,
	SXREG,	TDOUBLE,
	SXREG,	TDOUBLE,
		NSPECIAL,	RESCC,
#ifdef ARM_HAS_FPA
		"	cmfd AL,AR" COM "double compare\n", },
#elifdef ARM_HAS_VFP
		"	fcmpd AL,AR" COM "double compare\n", },
#else
		"ZF", },
#endif

{ OPLOG,	FORCC,
	SXREG,	TLDOUBLE,
	SXREG,	TLDOUBLE,
		NSPECIAL,	RESCC,
#ifdef ARM_HAS_FPA
		"	cmfe AL,AR" COM "ldouble compare\n", },
#elifdef ARM_HAS_VFP
		"	fcmpd AL,AR" COM "double compare\n", },
#else
		"ZF", },
#endif

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

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TSHORT,
		NAREG,	RESC1,
		"	ldrsh A1,AL" COM "load short from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SOREG|SNAME,	TUSHORT,
		NAREG,	RESC1,
		"	ldrh A1,AL" COM "load ushort from memory\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SCON,		TANY,
		NAREG,	RESC1,
		"	ldr A1,ZI" COM "load integer constant\n", },

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

{ OPLTYPE,	INFREG,
	SANY,		TANY,
	SOREG|SNAME,	TFLOAT,
		NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfs A1,AL" COM "load float\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "load float\n", },
#endif

{ OPLTYPE,	INXREG,
	SANY,		TANY,
	SOREG|SNAME,	TDOUBLE,
		NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfd A1,AL" COM "load double\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "load double\n"
		"	ldr U1,UL\n", },
#endif

{ OPLTYPE,	INXREG,
	SANY,		TANY,
	SOREG|SNAME,	TLDOUBLE,
		NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	ldfe A1,AL" COM "load ldouble\n", },
#elifdef ARM_HAS_VFP
#else
		"	ldr A1,AL" COM "load ldouble\n"
		"	ldr U1,UL\n", },
#endif

/*
 * Negate a word.
 */

{ UMINUS,	INAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	neg A1,AL" COM "negation\n", },

{ UMINUS,	INBREG,
	SBREG,	TLONGLONG|TULONGLONG,
	SBREG,	TLONGLONG|TULONGLONG,
		NBREG|NBSL,	RESC1,
		"	rsbs A1,AL,#0" COM "64-bit negation\n"
		"	rsc U1,UL,#0\n", },

{ UMINUS,	INFREG,
	SFREG,	TFLOAT,
	SFREG,	TFLOAT,
		NSPECIAL|NFREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	mvfs A1,AL" COM "float negation\n", },
#elifdef ARM_HAS_VFP
		"	negs A1,AL" COM "float negation\n", },
#else
		"ZF", },
#endif

{ UMINUS,	INXREG,
	SXREG,	TDOUBLE,
	SXREG,	TDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	mvfd A1,AL" COM "double negation\n", },
#elifdef ARM_HAS_VFP
		"	negd A1,AL" COM "double negation\n", },
#else
		"ZF", },
#endif

{ UMINUS,	INXREG,
	SXREG,	TLDOUBLE,
	SXREG,	TLDOUBLE,
		NSPECIAL|NXREG,	RESC1,
#ifdef ARM_HAS_FPA
		"	mvfe A1,AL" COM "ldouble negation\n", },
#elifdef ARM_HAS_VFP
		"	negd A1,AL" COM "ldouble negation\n", },
#else
		"ZF", },
#endif

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
	SFREG,	TFLOAT,
	SANY,	TFLOAT,
		0,	0,
		"	stmfd sp!,{AL}" COM "save function arg to stack\n", },

{ FUNARG,       FOREFF,
        SXREG,  TDOUBLE|TLDOUBLE,
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
