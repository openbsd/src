/*	$OpenBSD: table.c,v 1.2 2008/04/11 20:45:52 stefan Exp $	*/
/*
 * Copyright (c) 2008 David Crawshaw <david@zentus.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include "pass2.h"

#define TS64   TLONG|TLONGLONG
#define TU64   TULONG|TULONGLONG|TPOINT
#define T64    TS64|TU64

struct optab table[] = {

{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },      /* empty */

{ PCONV,	INAREG,
	SAREG,	T64,
	SAREG,	T64,
		0,	RLEFT,
		"	! convert between word and pointer\n", },

/* Conversions. */

{ SCONV,	INAREG,
	SAREG,	T64|TUNSIGNED,
	SAREG,	TINT,
		NAREG|NASL,	RESC1,
		"	sra AL,0,A1	\t\t! (u)int64/32 -> (u)int32\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TUNSIGNED,
	SAREG,	TSHORT,
		NAREG|NASL,	RESC1,
		"	sll AL,16,A1	\t\t! (u)int64/32 -> int16\n"
		"	sra AL,16,A1\n"
		"	sra AL, 0,A1\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TUNSIGNED,
	SAREG,	TUSHORT,
		NAREG|NASL,	RESC1,
		"	sll AL,16,A1	\t\t! (u)int64/32 -> uint16\n"
		"	srl AL,16,A1\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TUNSIGNED|TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG|NASL,	RESC1,
		"	sll AL,24,A1	\t\t! (u)int64/32/16 -> int8\n"
		"	sra AL,24,A1\n"
		"	sra AL, 0,A1\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TUNSIGNED|TSHORT|TUSHORT,
	SAREG,	TUCHAR,
		NAREG|NASL,	RESC1,
		"	and AL,0xff,A1	\t\t! (u)int64/32/16 -> uint8\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TUNSIGNED|TSHORT|TUSHORT,
	SAREG,	T64,
		0,	RLEFT,
		"	              	\t\t! (u)int64...8 -> (u)int64\n", },

{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TINT,
		0,	RLEFT,
		"	              	\t\t! (u)int16/8 -> int32\n", },

{ SCONV,	INAREG,
	SAREG,	T64|TINT|TSHORT|TCHAR,
	SAREG,	TUNSIGNED,
		0,	RLEFT,
		"	srl AL, 0,A1	\t\t! int32/16/8 -> uint32\n", },

{ SCONV,	INAREG,
	SAREG,	TUSHORT|TUCHAR,
	SAREG,	TUNSIGNED,
		0,	RLEFT,
		"	              	\t\t! uint16/8 -> uint32\n", },

/* Floating-point conversions must be stored and loaded. */

{ SCONV,	INBREG,
	SOREG,	T64|TUNSIGNED,
	SBREG,	TFLOAT,
		NBREG,	RESC1,
		"	ld [AL],A1	\t\t! int64 -> float\n"
		"	fxtos A1,A1\n", },

{ SCONV,	INBREG,
	SOREG,	TINT|TSHORT|TCHAR,
	SBREG,	TFLOAT,
		NBREG,	RESC1,
		"	ld [AL],A1	\t\t! int32/16/8 -> float\n"
		"	fitos A1,A1\n", }, // XXX need 'lds', 'ldh', etc

{ SCONV,	INCREG,
	SOREG,	T64,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	ldd [AL],A1	\t\t! (u)int64 -> double\n"
		"	fxtod A1,A1\n", },

{ SCONV,	INCREG,
	SOREG,	TINT|TSHORT|TCHAR,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	mov AL,A1	\t\t! int32/16/8 -> double\n"
		"	fitod A1,A1\n", }, // XXX need 'lds' 'ldh' 'ld', etc.

{ SCONV,	INBREG,
	SCREG,	TDOUBLE,
	SBREG,	TFLOAT,
		NBREG,	RESC1,
		"	fdtos AL,A1 	\t\t! double -> float\n",},

{ SCONV,	INCREG,
	SBREG,	TFLOAT,
	SCREG,	TDOUBLE,
		NCREG,	RESC1,
		"	fstod AL,A1 	\t\t! float -> double\n",},


/* Multiplication and division */

{ MUL,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASR|NASL,	RESC1,
		"	mulx AL,AR,A1		! multiply\n", },

{ MUL,	INBREG,
	SBREG,	TFLOAT,
	SBREG,	TFLOAT,
		NBREG|NBSR|NBSL,	RESC1,
		"	fmuls AL,AR,A1		! multiply float\n", },

{ MUL,	INCREG,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG|NCSR|NCSL,	RESC1,
		"	fmuld AL,AR,A1		! multiply double\n", },

{ DIV,	INAREG,
	SAREG,	TUNSIGNED|TUSHORT|TUCHAR|TU64,
	SAREG,	TUNSIGNED|TUSHORT|TUCHAR|TU64,
		NAREG|NASR|NASL,	RESC1,
		"	udivx AL,AR,A1		! unsigned division\n", },

{ DIV,	INAREG,
	SAREG,	TINT|TSHORT|TCHAR|TS64,
	SAREG,	TINT|TSHORT|TCHAR|TS64,
		NAREG|NASR|NASL,	RESC1,
		"	sdivx AL,AR,A1		! signed division\n", },

{ DIV,	INBREG,
	SBREG,	TFLOAT,
	SBREG,	TFLOAT,
		NBREG|NBSR|NBSL,	RESC1,
		"	fdivs AL,AR,A1		! divide float\n", },

{ DIV,	INCREG,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG|NCSR|NCSL,	RESC1,
		"	fdivd AL,AR,A1		! divide double\n", },

{ MOD,	INAREG,
	SAREG,	TUNSIGNED|TUSHORT|TUCHAR|TU64,
	SAREG,	TUNSIGNED|TUSHORT|TUCHAR|TU64,
		NAREG, RESC1,
		"	udivx AL,AR,A1		! unsigned modulo\n"
		"	mulx A1,AR,A1\n"
		"	sub AL,A1,A1\n", },

{ MOD,	INAREG,
	SAREG,	TINT|TSHORT|TCHAR|TS64,
	SAREG,	TINT|TSHORT|TCHAR|TS64,
		NAREG, RESC1,
		"	sdivx AL,AR,A1		! signed modulo\n"
		"	mulx A1,AR,A1\n"
		"	sub AL,A1,A1\n", },

{ PLUS,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
      		"	add AL,AR,A1\n", },

{ PLUS,	INBREG,
	SBREG,	TFLOAT,
	SBREG,	TFLOAT,
		NBREG|NBSL,	RESC1,
      		"	fadds AL,AR,A1\n", },

{ PLUS,	INCREG,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG|NCSL,	RESC1,
      		"	faddd AL,AR,A1\n", },

{ PLUS,	INAREG,
	SAREG,	TANY,
	SCON,	TANY,
		(3*NAREG)|NASL,	RESC1,
		"ZA", },

{ MINUS,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
		"	sub AL,AR,A1\n", },

{ MINUS,	INBREG,
	SBREG,	TANY,
	SBREG,	TANY,
		NBREG|NBSL|NBSR,	RESC1,
      		"	fsubs AL,AR,A1\n", },

{ MINUS,	INCREG,
	SCREG,	TANY,
	SCREG,	TANY,
		NCREG|NCSL|NBSR,	RESC1,
      		"	fsubd AL,AR,A1\n", },

{ MINUS,	INAREG,
	SAREG,	TANY,
	SCON,	TANY,
		(3*NAREG)|NASL,	RESC1,
		"ZB", },

{ UMINUS,	INAREG,
	SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sub %g0,AL,A1\n", },

{ UMINUS,	INBREG,
	SBREG,	TANY,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"	fsubs %g0,AL,A1\n", },

{ UMINUS,	INCREG,
	SCREG,	TANY,
	SANY,	TANY,
		NCREG|NCSL,	RESC1,
		"	fsubd %g0,AL,A1\n", },

/* Shifts */

{ RS,	INAREG,
	SAREG,	TINT|TUNSIGNED|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sra AL,AR,A1			! shift right\n", },

{ RS,	INAREG,
	SAREG,	T64,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	srax AL,AR,A1			! shift right\n", },

{ LS,	INAREG,
	SAREG,	TINT|TUNSIGNED|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sll AL,AR,A1			! shift left\n", },

{ LS,	INAREG,
	SAREG,	T64,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sllx AL,AR,A1			! shift left\n", },

{ COMPL,	INAREG,
	SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	orn AL,%g0,A1			! complement\n", },

/* Assignments */

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	TINT|TUNSIGNED,
	SAREG,	TINT|TUNSIGNED,
		0,	RDEST,
		"	stw AR,[AL]		! store (u)int32\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT,
		0,	RDEST,
        	"	sth AR,[AL]		! store (u)int16\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		0,	RDEST,
        	"	stb AR,[AL]		! store (u)int8\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	T64,
	SAREG,	T64,
		0,	RDEST,
		"	stx AR,[AL] 		! store (u)int64\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INBREG,
	SOREG,	TFLOAT,
	SBREG,	TFLOAT,
		0,	RDEST,
		"	st AR,[AL] 		! store float\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INCREG,
	SOREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		0,	RDEST,
		"	std AR,[AL] 		! store double\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,	TINT|TUNSIGNED,
	SAREG,	TINT|TUNSIGNED,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store (u)int32 into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	stw AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store (u)int16 into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	sth AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store (u)int8 into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	stb AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,	T64,
	SAREG,	T64,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store (u)int64 into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	stx AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INBREG,
	SNAME,	TFLOAT,
	SBREG,	TFLOAT,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store float into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	st AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INBREG,
	SNAME,	TDOUBLE,
	SCREG,	TDOUBLE,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store double into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	std AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RDEST,
		"	mov AR,AL			! register move\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,	TANY,
	SBREG,	TANY,
		0,	RDEST,
		"	fmovs AR,AL			! move float\n", },

{ ASSIGN,	FOREFF|INCREG,
	SCREG,	TANY,
	SCREG,	TANY,
		0,	RDEST,
		"	fmovd AR,AL			! move double\n", },

/* Structure assignment. */

{ STASG,	INAREG|FOREFF,
	SOREG|SNAME,	TANY,
	SAREG,		TPTRTO|TANY,
		NSPECIAL,	RRIGHT,
		"ZQ", },

/* Comparisons. */

{ EQ,	FORCC,
        SAREG,	TANY,
        SAREG,	TANY,
                0,      RESCC,
		"	cmp AL,AR\n"
		"	be LC\n"
		"	nop\n", },

{ NE,	FORCC,
        SAREG,	TANY,
        SAREG,	TANY,
                0,      RESCC,
		"	cmp AL,AR\n"
                "	bne LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
	SAREG,	TANY,
	SZERO,	TANY,
		0,	RESCC,
		"	O AL,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESCC,
		"	sub AL,AR,A1			! oplog\n"
		"	O A1,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
	SAREG,	TANY,
	SCCON,	TANY,
		NAREG|NASL,	RESCC,
		"	sub AL,AR,A1			! oplog sccon\n"
		"	O A1,LC\n"
		"	nop\n", },

{ OPLOG,	FORCC,
	SBREG,	TFLOAT,
	SBREG,	TFLOAT,
		NBREG, RESCC,
		"	fcmps AL,AR			! oplog float\n"
		"	ZF LC\n", },

{ OPLOG,	FORCC,
	SOREG,	TFLOAT,
	SBREG,	TFLOAT,
		NBREG, RESCC,
		"	ld [AL], A1    			! oplog float oreg\n"
		"	nop\n"
		"	fcmps A1,AR\n"
		"	ZF LC\n", },

{ OPLOG,	FORCC,
	SCREG,	TDOUBLE,
	SCREG,	TDOUBLE,
		NCREG, RESCC,
		"	XXX double oplog\n", },


/* Load constants to register. */

{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		T64,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const (u)int64 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldx [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TINT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const int32 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsw [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUNSIGNED,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const uint32 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	lduw [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TSHORT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const int16 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsh [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUSHORT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const uint16 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	lduh [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TCHAR,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t\t! load const int8 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsb [A1+%l44(AL)],A1\n"
		"	nop\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUCHAR,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const uint8 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldub [A1+%l44(AL)],A1\n"
		"	nop\n", },

{ OPLTYPE,	INBREG,
	SBREG,	TANY,
	SNAME,	TANY,
		NAREG|NBREG,	RESC2,
		"	sethi %h44(AL),A1\t! load const float to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ld [A1+%l44(AL)],A2\n"
		"	nop\n", },

{ OPLTYPE,	INCREG,
	SCREG,	TANY,
	SNAME,	TANY,
		NAREG|NCREG,	RESC2,
		"	sethi %h44(AL),A1\t! load const double to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldd [A1+%l44(AL)],A2\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANY,
		(2*NAREG),	RESC1,
		"ZC" },

/* Convert LTYPE to reg. */

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TCHAR,
		NAREG,	RESC1,
		"	ldsb [AL],A1		! load int8 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TUCHAR,
		NAREG,	RESC1,
		"	ldub [AL],A1		! load uint8 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TSHORT,
		NAREG,	RESC1,
		"	ldsh [AL],A1		! load int16 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TUSHORT,
		NAREG,	RESC1,
		"	lduh [AL],A1		! load uint16 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TINT,
		NAREG,	RESC1,
		"	ldsw [AL],A1		! load int32 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TUNSIGNED,
		NAREG,	RESC1,
		"	lduw [AL],A1		! load uint32 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	T64,
		NAREG,	RESC1,
		"	ldx [AL],A1		! load (u)int64 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG,	RESC1,
		"	mov \%g0,A1\t		! load 0 to reg\n", },

{ OPLTYPE,	INBREG,
	SBREG,	TFLOAT,
	SOREG,	TFLOAT,
		NBREG,	RESC1,
		"	ld [AL],A1  		! load float to reg\n"
		"	nop\n", },

{ OPLTYPE,	INCREG,
	SCREG,	TDOUBLE,
	SOREG,	TDOUBLE,
		NCREG,	RESC1,
		"	ldd [AL],A1  		! load double to reg\n"
		"	nop\n", },

/* Jumps. */

{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	call LL		 	! goto LL\n"
		"	nop\n", },

{ UCALL,	FOREFF,
	SCON,		TANY,
	SANY,		TANY,
		0,	0,
		"	call CL			! void CL()\n"
		"	nop\n", },

{ UCALL,         INAREG,
        SCON,		TANY,
        SAREG,          TANY,
                NAREG,     RESC1,
		"	call CL			! = CL()\n"
		" 	nop\n", },

{ CALL,		FOREFF,
	SCON,		TANY,
	SANY,		TANY,
		0,	0,
		"	call CL			! void CL(constant)\n"
		"	nop\n", },

{ CALL,		INAREG,
	SCON,		TANY,
	SAREG,		TANY,
		NAREG,		RESC1,
		"	call CL			! = CL(constant)\n"
		"	nop\n", },

{ CALL,         INAREG,
        SAREG,		TANY,
        SAREG,		TANY,
                NAREG,     RESC1,
		"	call AL			! = AL(args)\n"
		"	nop\n", },

{ CALL,		FOREFF,
	SAREG,		TANY,
	SANY,		TANY,
		0,		0,
		"	call AL			! void AL(args)\n"
		"	nop\n", },

{ UCALL,	FOREFF,
	SAREG,		TANY,
	SANY,		TANY,
		0,	0,
		"	call AL			! (*AL)()\n"
		"	nop\n", },

{ UCALL,	INAREG,
	SAREG,		TANY,
	SAREG,		TANY,
		NAREG,		RESC1,
		"	call AL			! = (*AL)()\n"
		"	nop\n", },

{ CALL,		INAREG,
	SAREG,		TANY,
	SAREG,		TANY,
		NAREG,		RESC1,
		"	call AL			! = (*AL)(args)\n"
		"	nop\n", },

/* Function arguments. */

{ FUNARG,       FOREFF,
        SAREG,  T64,
        SANY,   TANY,
                0,      0,
                "	stx AL,[%sp+AR]   	\t! save func arg to stack\n"
		"	nop\n", },

{ FUNARG,       FOREFF,
        SAREG,  TINT|TUNSIGNED,
        SANY,   TANY,
                0,      0,
                "	stw AL,[%sp+AR]   	\t! save func arg to stack\n"
		"	nop\n", },

{ FUNARG,       FOREFF,
        SAREG,  TSHORT|TUSHORT,
        SANY,   TANY,
                0,      0,
                "	sth AL,[%sp+AR]   	\t! save func arg to stack\n"
		"	nop\n", },

{ FUNARG,       FOREFF,
        SAREG,  TCHAR|TUCHAR,
        SANY,   TANY,
                0,      0,
                "	stb AL,[%sp+AR]  	\t! save func arg to stack\n"
		"	nop\n", },


/* Indirection. */

{ OPSIMP,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASR|NASL,	RESC1,
		"	O AL,AR,A1\n", },

{ UMUL, INAREG,
	SANY,	T64,
	SOREG,	T64,
		NAREG,		RESC1,
		"	ldx [AL],A1		! (u)int64 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TINT,
	SOREG,	TINT,
		NAREG,		RESC1,
		"	ldsw [AL],A1		! int32 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TUNSIGNED,
	SOREG,	TUNSIGNED,
		NAREG,		RESC1,
		"	lduw [AL],A1		! uint32 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TCHAR,
	SOREG,	TCHAR,
		NAREG,		RESC1,
		"	ldsb [AL],A1		! int8 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TUCHAR,
	SOREG,	TUCHAR,
		NAREG,		RESC1,
		"	ldub [AL],A1		! uint8 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TSHORT,
	SOREG,	TSHORT,
		NAREG,		RESC1,
		"	ldsh [AL],A1		! int16 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TUSHORT,
	SOREG,	TUSHORT,
		NAREG,		RESC1,
		"	lduh [AL],A1		! uint16 load\n"
		"	nop\n", },

{ UMUL, INBREG,
	SANY,	TFLOAT,
	SOREG,	TFLOAT,
		NBREG,		RESC1,
		"	ld [AL],A1		! load float\n"
		"	nop\n", },

{ UMUL, INCREG,
	SANY,	TDOUBLE,
	SOREG,	TDOUBLE,
		NCREG,		RESC1,
		"	ldd [AL],A1		! load double\n"
		"	nop\n", },

{ FREE,FREE,FREE,FREE,FREE,FREE,FREE,FREE, "ERR: printing free op\n" },

};

int tablesize = sizeof(table)/sizeof(table[0]);
