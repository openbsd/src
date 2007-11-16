/*	$OpenBSD: table.c,v 1.2 2007/11/16 09:00:13 otto Exp $	*/
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


# include "pass2.h"

# define TLL TLONGLONG|TULONGLONG
# define ANYSIGNED TINT|TLONG|TSHORT|TCHAR
# define ANYUSIGNED TUNSIGNED|TULONG|TUSHORT|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TUWORD TUNSIGNED|TULONG
# define TSWORD TINT|TLONG
# define TWORD TUWORD|TSWORD

struct optab table[] = {
{ -1, FORREW,SANY,TANY,SANY,TANY,REWRITE,-1,"", },
/*
 * A bunch of pointer conversions.
 * First pointer to integer.
 */
/* Convert char pointer to int */
{ SCONV,	INAREG,
	SAREG|SAREG,	TPTRTO|TCHAR|TUCHAR,
	SANY,	TWORD,
		NAREG,	RLEFT,
		"	lsh AL,2\n"
		"	move A1,AL\n"
		"	lsh A1,-040\n"
		"	trz A1,074\n"
		"	ior AL,A1\n"
		"	tlz AL,0740000\n", },

/* Convert short pointer to int */
{ SCONV,	INAREG,
	SAREG|SAREG,	TPTRTO|TSHORT|TUSHORT,
	SANY,	TWORD,
		NAREG,	RLEFT,
		"	lsh AL,2\n"
		"	move A1,AL\n"
		"	lsh A1,-041\n"
		"	trz A1,2\n"
		"	ior AL,A1\n"
		"	tlz AL,0740000\n", },

/* Convert int/unsigned/long/ulong/struct/union/func ptr to int */
{ SCONV,	INAREG,
	SAREG|SAREG,	TPTRTO|TWORD|TSTRUCT|TPOINT,
	SANY,		TWORD,
		0,	RLEFT,
		"	lsh AL,2\n", },

/*
 * Convert int/long to pointers.
 */
/* Convert int to char pointer */
{ PCONV,	INAREG,
	SAREG,	TWORD,
	SANY,	TPTRTO|TCHAR|TUCHAR,
		NAREG,	RLEFT,
		"	move A1,AL\n"
		"	lsh A1,036\n"
		"	tlo A1,0700000\n"
		"	tlz A1,0040000\n"
		"	lsh AL,-2\n"
		"	ior AL,A1\n", },

/* Convert int/long to short pointer */
{ PCONV,	INAREG,
	SAREG,	TWORD,
	SANY,	TPTRTO|TSHORT|TUSHORT,
		NAREG,	RLEFT,
		"	move A1,AL\n"
		"	lsh AL,-2\n"
		"	tlo AL,0750000\n"
		"	lsh A1,035\n"
		"	tlz A1,0760000\n"
		"	add AL,A1\n", },

/* Convert int/long to int/struct/multiple ptr */
{ PCONV,	INAREG,
	SAREG,	TWORD,
	SANY,	TPOINT|TWORD|TSTRUCT,
		0,	RLEFT,
		"	lsh AL,-2\n", },

/*
 * Pointer to pointer conversions.
 */
/* Convert char ptr to short ptr */
{ PCONV,	INAREG,
	SAREG,	TPTRTO|TCHAR|TUCHAR,
	SANY,	TPTRTO|TSHORT|TUSHORT,
		0,	RLEFT,
		"	tlo AL,050000\n"
		"	tlne AL,020000\n"
		"	tlz AL,010000\n", },

/* Convert char/short pointer to int/struct/multiple ptr */
{ PCONV,	INAREG,
	SAREG,	TPTRTO|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SANY,	TPOINT|TWORD|TSTRUCT,
		0,	RLEFT,
		"	tlz AL,0770000\n", },

/* Convert short pointer to char ptr */
{ PCONV,	INAREG,
	SAREG,	TPTRTO|TSHORT|TUSHORT,
	SANY,	TPTRTO|TCHAR|TUCHAR,
		0,	RLEFT,
		"	tlz AL,050000\n", },

/* Convert int/struct/foo pointer to char ptr */
{ PCONV,	INAREG,
	SAREG,	TPOINT|TWORD|TSTRUCT,
	SANY,	TPTRTO|TCHAR|TUCHAR,
		0,	RLEFT,
		"	tlo AL,0700000\n", },

/* Convert int/struct/foo pointer to short ptr */
{ PCONV,	INAREG,
	SAREG,	TPTRTO|TWORD|TSTRUCT,
	SANY,	TPTRTO|TSHORT|TUSHORT,
		0,	RLEFT,
		"	tlo AL,0750000\n", },

/*
 * A bunch conversions of integral<->integral types
 */

/* convert short/char to int. This is done when register is loaded */
{ SCONV,	INAREG,
	SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR|TWORD,
	SANY,	TWORD,
		0,	RLEFT,
		"", },

/* convert int to short/char. This is done when register is loaded */
{ SCONV,	INAREG,
	SAREG,	TWORD,
	SANY,	TSHORT|TUSHORT|TCHAR|TUCHAR|TWORD,
		0,	RLEFT,
		"", },

/* convert int/long to unsigned long long */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TULONGLONG,
		NAREG|NASL,	RESC1,
		"	move U1,AL\n"
		"	setz A1,\n"
		"	tlze U1,0400000\n"
		"	tro A1,01\n" , },

/* convert int/long to long long */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TLONGLONG,
		NAREG|NASL,	RESC1,
		"	move U1,AL\n"
		"	move A1,U1\n"
		"	ash A1,-043\n", },

/* convert uchar/ushort to (unsigned) long long */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TUCHAR|TUSHORT,
	SANY,				TLL,
		NAREG|NASL,	RESC1,
		"	move U1,AL\n"
		"	setz A1,\n", },

/* convert long long to int/long */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TLL,
	SANY,	TWORD,
		NAREG|NASL,	RESC1,
		"	move A1,UL\n", },

/* convert long long to unsigned char - XXX - signed char */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TLL,
	SANY,	TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	move A1,UL\n"
		"	andi A1,0777\n", },

/* convert long long to short - XXX - signed short */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TLL,
	SANY,	TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,
		"	move A1,UL\n"
		"	hrrz A1,A1\n", },

/* floating point conversions */
{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TDOUBLE|TFLOAT,
	SANY,	TWORD,
		NAREG|NASL,	RESC1,
		"	fix A1,AL\n", },

{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TFLOAT,
		NAREG|NASL,	RESC1,
		"	fltr A1,AL\n", },

{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TDOUBLE,
		NAREG|NASL,	RESC1,
		"	fltr A1,AL\n	setz U1,\n", },

{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TDOUBLE,
	SANY,	TFLOAT,
		NAREG|NASL,	RESC1,
		"	move A1,AL\n", },

{ SCONV,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TFLOAT,
	SANY,	TDOUBLE,
		NAREG|NASL,	RESC1,
		"	move A1,AL\n	setz U1,\n", },

/*
 * Subroutine calls.
 */

{ UCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,	/* should be 0 */
		"	pushj 017,AL\nZB", },

{ CALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,	/* should be 0 */
		"	pushj 017,AL\nZB", },

{ UCALL,	INAREG,
	SCON,	TANY,
	SANY,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT|TFLOAT|TDOUBLE|TLL|TPOINT,
		NAREG,	RESC1,	/* should be 0 */
		"	pushj 017,AL\nZB", },

{ UCALL,	INAREG,
	SAREG|SAREG,	TANY,
	SANY,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT|TFLOAT|TDOUBLE|TLL|TPOINT,
		NAREG|NASL,	RESC1,	/* should be 0 */
		"	pushj 017,(AL)\nZB", },

{ UCALL,	INAREG,
	SNAME|SOREG,	TANY,
	SANY,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT|TFLOAT|TDOUBLE|TLL|TPOINT,
		NAREG,	RESC1,	/* should be 0 */
		"	pushj 017,@AL\nZB", },

/*
 * MOVE nodes are usually inserted late (at register assignment).
 */
{ MOVE,		FOREFF,
	SANY,	TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RRIGHT,
		"	move AR,AL\n", },

{ MOVE,		FOREFF,
	SANY,	TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RRIGHT,
		"	dmove AR,AL\n", },

#ifdef notyet
/*
 * INCR can be slightly optimized.
 */
{ INCR,		INAREG,
	SAREG|SAREG|SNAME|SOREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TPTRTO,
	SONE,	TANY,
		NAREG,	RESC1,
		"	move A1,AL\n"
		"	ibp AL\n", },

/* Fix check of return value */
{ INCR,		FOREFF,
	SAREG|SAREG|SNAME|SOREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TPTRTO,
	SONE,	TANY,
		0,	0,
		"	ibp AL\n", },
#endif

/*
 * PLUS operators.
 */
/* Add a value to a char/short pointer */
{ PLUS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG|SNAME|SOREG,	TPTRTO|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,			TWORD,
		0,	RRIGHT,
		"	adjbp AR,AL\n", },

/* No more search for char/short pointer addition */
{ PLUS,	INAREG|INAREG|FOREFF,
	SANY,	TPTRTO|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SANY,	TANY,
		REWRITE, 0,
		"DIEDIEDIE!\n", },

/* Add char/short/int to register */
{ PLUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	add AL,AR\n", },

/* Add char/short/int to memory */
{ PLUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SAREG|SAREG,			TWORD,
		0,	RLEFT,
		"	addm AR,AL\n", },

/* Add a small constant to a register */
{ PLUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD|TPOINT,
	SUSHCON,	TWORD,
		0,	RLEFT,
		"	addi AL,AR\n", },

/* Add a larger constant to a register */
{ PLUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD|TPOINT,
	SCON,	TWORD,
		0,	RLEFT,
		"	add AL,[ .long AR ]\n", },

/* Add long long to register */
{ PLUS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,			TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	dadd AL,AR\n", },

/* Add int (or int pointer) to register */
{ PLUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TWORD|TPOINT,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	add AL,AR # foo \n", },

/* char/short are allowed to be added if they are in registers */
{ PLUS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	add AL,AR\n", },

/* get address of an memory position into a register */
{ PLUS,	INAREG|INAREG,
	SAREG|SAREG,	TWORD|TPTRTO,
	SCON,		TANY,
		NAREG,	RESC1,
		"	xmovei A1,AR(AL)\n", },

/* Safety belt for plus */
{ PLUS,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/*
 * MINUS operators.
 */
/* Rewrite subtracts from char/short pointers (to negative adds) */
{ MINUS,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TCHAR|TUCHAR|TSHORT|TUSHORT|TPTRTO,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/* Subtract char/short/int word in memory from reg */
{ MINUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TWORD|TPOINT,
	SAREG|SAREG|SNAME|SOREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	sub AL,AR\n", },

/* Subtract a small constant from reg */
{ MINUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TWORD|TPOINT,
	SUSHCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	subi AL,AR\n", },

/* Subtract a large constant from reg */
{ MINUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TWORD|TPOINT,
	SCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	sub AL,[ .long AR ]\n", },

/* Subtract char/short/int word in memory from reg, save in memory */
{ MINUS,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RRIGHT,
		"	subm AL,AR\n", },

/* Subtract long long from register */
{ MINUS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,			TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	dsub AL,AR\n", },

/* char/short are allowed to be subtracted if they are in registers */
{ MINUS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	sub AL,AR\n", },

/* Safety belt for plus */
{ MINUS,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/*
 * AND/OR/ER operators.
 * Simpler that the ops above in that they only work on integral types.
 */
/* And char/short/int with integer memory */
{ AND,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	and AL,AR\n", },

/* And char/short/int with register */
{ AND,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	and AL,AR\n", },

/* And char/short/int with small constant */
{ AND,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SUSHCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	andi AL,AR\n", },

/* And char/short/int with large constant */
{ AND,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	and AL,[ .long AR ]\n", },

/* long long AND */
{ AND,	INAREG|FOREFF,
	SAREG|SAREG,			TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	and AL,AR\n"
		"	and UL,UR\n", },

/* Safety belt for AND */
{ AND,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },


/* OR char/short/int with integer memory */
{ OR,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	ior AL,AR\n", },

/* OR char/short/int with register */
{ OR,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	ior AL,AR\n", },

/* OR char/short/int with small constant */
{ OR,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SUSHCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	iori AL,AR\n", },

/* OR char/short/int with large constant */
{ OR,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	ior AL,[ .long AR ]\n", },

/* long long OR */
{ OR,	INAREG|FOREFF,
	SAREG|SAREG,			TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	ior AL,AR\n"
		"	ior UL,UR\n", },

/* Safety belt for OR */
{ OR,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },


/* ER char/short/int with integer memory */
{ ER,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	xor AL,AR\n", },

/* ER char/short/int with register */
{ ER,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SAREG|SAREG,			TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	xor AL,AR\n", },

/* ER char/short/int with small constant */
{ ER,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SUSHCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	xori AL,AR\n", },

/* ER char/short/int with large constant */
{ ER,	FOREFF|INAREG|INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
	SCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		0,	RLEFT,
		"	xor AL,[ .long AR ]\n", },

/* long long ER */
{ ER,	INAREG|FOREFF,
	SAREG|SAREG,			TLL,
	SAREG|SAREG|SNAME|SOREG,	TLL,
		0,	RLEFT,
		"	xor AL,AR\n"
		"	xor UL,UR\n", },

/* Safety belt for ER */
{ ER,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/*
 * The next rules handle all shift operators.
 */
{ LS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
		0,	RLEFT,
		"	lsh AL,(AR)\n", },

{ LS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SNAME|SOREG,	TWORD,
		0,	RLEFT,
		"	lsh AL,@AR\n", },

{ LS,       INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TLL,
	SCON,		TANY,
		0,	RLEFT,
		"	ashc AL,ZH\n", },

{ LS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TLL,
	SAREG|SAREG /* |SNAME|SOREG */,	TANY,
		0,	RLEFT,
		"	ashc AL,(AR)\n", },

{ RS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TSWORD,
	SCON,		TWORD,
		0,	RLEFT,
		"	ash AL,-ZH\n", },

{ RS,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TUWORD,
	SCON,		TWORD,
		0,	RLEFT,
		"	lsh AL,-ZH\n", },

/* Safety belt for LS/RS */
{ LS,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

{ RS,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/*
 * The next rules takes care of assignments. "=".
 */
/* Match zeroed registers first */
{ ASSIGN,	INAREG|FOREFF,
	SAREG,	TUCHAR|TUSHORT|TCHAR|TSHORT|TWORD|TPOINT,
	SZERO,	TANY,
		0,	RDEST,
		"	setz AL,\n", },

{ ASSIGN,	FOREFF,
	SAREG|SNAME|SOREG,	TWORD|TPOINT,
	SZERO,	TANY,
		0,	0,
		"	setzm AL\n", },

{ ASSIGN,	INAREG|FOREFF,
	SAREG|SAREG,	TUCHAR|TUSHORT|TCHAR|TSHORT|TWORD|TPOINT,
	SMONE,	TANY,
		0,	RDEST,
		"	setom AL\n", },

{ ASSIGN,	FOREFF,
	SAREG|SNAME|SOREG,	TWORD|TPOINT,
	SMONE,	TANY,
		0,	0,
		"	setom AL\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,		TWORD|TPOINT,
	SCON,		TWORD|TPOINT,
		0,	RDEST,
		"	ZC\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SNAME|SOREG,	TWORD|TPOINT|TFLOAT,
	SAREG|SAREG,		TUCHAR|TUSHORT|TWORD|TPOINT|TFLOAT,
		0,	RDEST,
		"	movem AR,AL\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SNAME|SOREG,	TWORD|TPOINT|TFLOAT,
	SAREG|SAREG,		TSHORT,
		0,	RDEST,
		"	hrrem AR,AL\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TUCHAR|TUSHORT|TCHAR|TSHORT|TWORD|TPOINT,
	SAREG|SAREG|SNAME|SOREG,	TWORD|TPOINT,
		0,	RDEST,
		"	move AL,AR\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TUCHAR|TUSHORT|TCHAR|TSHORT,
	SAREG|SAREG,	TUCHAR|TUSHORT|TCHAR|TSHORT,
		0,	RDEST,
		"	move AL,AR\n", },

{ ASSIGN,	INBREG|FOREFF,
	SBREG|SNAME|SOREG,	TLL|TDOUBLE,
	SBREG,		TLL|TDOUBLE,
		0,	RDEST,
		"	dmovem AR,AL\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SOREG|SNAME,	TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG|SAREG,	TANY,
		0,	RDEST,
		"ZV", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TUSHORT|TUCHAR,
	SOREG,		TANY,
		0,	RDEST,
		"	ldb AL,Zg\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
	SSCON,		TANY,
		0,	RDEST,
		"	movei AL,AR\n", },

{ ASSIGN,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,		TANY,
		0,	RDEST,
		"	move AL,[ .long AR]\n", },

/*
 * DIV/MOD/MUL 
 * These can be done way more efficient.
 */
/* long long div. XXX - work only with unsigned */
{ DIV,	INBREG,
	SBREG|SNAME|SOREG,	TLL,
	SBREG|SNAME|SOREG,	TLL,
		(2*NBREG)|NBSL,	RESC1,
		"	dmove A2,AL ; dmove A1,[ .long 0,0 ]\n"
		"	ddiv A1,AR\n", },

/* long long div. with constant. XXX - work only with unsigned */
{ DIV,	INBREG,
	SBREG|SNAME|SOREG,	TLL,
	SCON,	TLL,
		(2*NBREG)|NBSL,	RESC1,
		"	dmove A2,AL ; dmove A1,[ .long 0,0 ]\n"
		"	ddiv A1,ZP\n", },

/* Simple divide. XXX - fix so next reg can be free */
{ DIV,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
		0,	RRIGHT,
		"	idivm AL,AR\n", },

/* Safety belt for DIV */
{ DIV,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/* long long MOD */
{ MOD,	INBREG,
	SBREG|SNAME|SOREG,	TLL,
	SBREG|SNAME|SOREG,	TLL,
		2*NBREG|NBSL,	RESC2,
		"	dmove A2,AL ; dmove A1,[ .long 0,0 ]\n"
		"	ddiv A1,AR\n", },

/* integer MOD */
{ MOD,	INAREG,
	SAREG|SNAME|SOREG,	TWORD,
	SAREG|SNAME|SOREG,	TWORD,
		2*NAREG|NASL,	RESC2,
		"	move A2,AL\n"
		"	setz A1,\n"
		"	idiv A1,AR\n", },

/* integer MOD for char/short */
{ MOD,	INAREG,
	SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
		2*NAREG|NASL,	RESC2,
		"	move A2,AL\n"
		"	setz A1,\n"
		"	idiv A1,AR\n", },

/* Safety belt for MOD */
{ MOD,	FOREFF,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/* long long MUL */
{ MUL,	INBREG,
	SBREG|SNAME|SOREG,	TLL,
	SBREG|SNAME|SOREG,	TLL,
		2*NBREG|NBSL,	RESC2,
		"	dmove A1,AL\n"
		"	dmul A1,AR\n", },

/* integer multiply to memory*/
{ MUL,	INAREG|INAREG|FOREFF,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SAREG|SAREG,			TWORD,
		0,		RLEFT,
		"	imulm AR,AL\n", },

/* integer multiply */
{ MUL,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,			TWORD,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
		0,		RLEFT,
		"	imul AL,AR\n", },

/* integer multiply for char/short */
{ MUL,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,	TWORD|TCHAR|TUCHAR|TSHORT|TUSHORT,
		0,		RLEFT,
		"	imul AL,AR\n", },

/* integer multiply with small constant */
{ MUL,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD,
	SUSHCON,	TWORD,
		0,		RLEFT,
		"	imuli AL,AR\n", },

/* integer multiply with large constant */
{ MUL,	INAREG|INAREG|FOREFF,
	SAREG|SAREG,	TWORD,
	SCON,		TWORD,
		0,		RLEFT,
		"	imul AL,[ .long AR ]\n", },

/* Safety belt for MUL */
{ MUL,	FORREW|FOREFF|INAREG|INAREG,
	SANY,	TANY,
	SANY,	TANY,
		REWRITE,	0,
		"DIEDIEDIE", },

/* read an indirect long long value into register */
{ UMUL,	INAREG,
	SAREG|SAREG,	TPTRTO|TLL|TWORD,
	SANY,		TLL,
		NAREG|NASL,	RESC1,
		"	dmove A1,(AL)\n", },

/* read an indirect integer value into register */
{ UMUL,	INAREG,
	SAREG|SAREG,	TWORD|TPOINT,
	SANY,		TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	move A1,(AL)\n", },

/* read an indirect value into register */
{ UMUL,	INAREG,
	SOREG,	TWORD|TPOINT,
	SANY,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	move A1,@AL\n", },

/* read an indirect value into register */
{ UMUL,	INAREG,
	SAREG|SAREG|SOREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TPTRTO,
	SANY,	TCHAR|TUCHAR|TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,
		"	ldb A1,AL\n", },

#ifdef notyet
/* Match tree shape for ildb */
{ UMUL,	INAREG,
	SANY,	TANY,
	SILDB,	TUCHAR|TCHAR|TPTRTO,
		NAREG,	RESC1,
		"	ildb A1,ZA\n", },
#endif

/* Match char/short pointers first, requires special handling */
{ OPLOG,	FORCC,
	SAREG|SAREG,	TPTRTO|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,	TPTRTO|TCHAR|TUCHAR|TSHORT|TUSHORT,
		0, 	RESCC,
		"ZZ", },

/* Can check anything by just comparing if EQ/NE */
{ OPLOG,	FORCC,
	SAREG|SAREG,	TWORD|TPOINT|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SZERO,	TANY,
		0, 	RESCC,
		"	jumpZe AL,LC # bu\n", },

{ EQ,		FORCC,
	SAREG|SAREG,	TWORD|TPOINT|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG|SOREG|SNAME|SCON,	TWORD|TPOINT,
		0, 	RESCC,
		"ZR", },

{ NE,		FORCC,
	SAREG|SAREG,	TWORD|TPOINT|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG|SOREG|SNAME|SCON,	TWORD|TPOINT,
		0, 	RESCC,
		"ZR", },

{ OPLOG,	FORCC,
	SAREG|SAREG,	TWORD,
	SAREG|SAREG|SOREG|SNAME|SCON,	TSWORD,
		0, 	RESCC,
		"ZR", },

{ OPLOG,	FORCC,
	SAREG|SAREG,	TCHAR|TUCHAR,
	SCON,		TANY,
		0, 	RESCC,
		"ZR", },

{ OPLOG,	FORCC,
	SAREG|SAREG,	TWORD|TPOINT|TFLOAT,
	SAREG|SAREG|SOREG|SNAME|SCON,	TWORD|TPOINT|TFLOAT,
		0, 	RESCC,
		"ZR", },

{ OPLOG,	FORCC,
	SAREG|SAREG,	TWORD|TPOINT|TCHAR|TUCHAR|TSHORT|TUSHORT,
	SAREG|SAREG,	TWORD|TPOINT|TCHAR|TUCHAR|TSHORT|TUSHORT,
		0, 	RESCC,
		"ZR", },

{ OPLOG,	FORCC,  
	SAREG|SAREG,	TLL|TDOUBLE, /* XXX - does double work here? */
	SAREG|SAREG|SOREG|SNAME,	TLL|TDOUBLE,
		0,	RESCC,
		"ZQ", },

/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jrst LL\n", },

/*
 * Convert LTYPE to reg.
 */
{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SMONE,	TLL,
		NBREG,	RESC1,
		"	seto A1,\n	seto U1,\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SMONE,	TANY,
		NAREG,	RESC1,
		"	seto A1,\n", },

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SZERO,	TLL,
		NBREG,	RESC1,
		"	setz A1,\n	setz U1,\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG,	RESC1,
		"	setz A1,\n", },

{ OPLTYPE,	INBREG,
	SANY,		TANY,
	SUSHCON,	TLL,
		NBREG,	RESC1,
		"	setz A1,\n	movei U1,AR\n", },

{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SUSHCON,	ANYFIXED,
		NAREG,	RESC1,
		"	movei A1,AR\n", },

{ OPLTYPE,	INAREG,
	SANY,	ANYFIXED,
	SNSHCON,	ANYFIXED,
		NAREG,	RESC1,
		"	hrroi A1,AR\n", },

{ OPLTYPE,	INAREG,
	SANY,	ANYFIXED,
	SCON,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"	ZD A1,ZE	# suspekt\n", },

{ OPLTYPE,	INAREG,
	SANY,	TWORD|TPOINT|TFLOAT,
	SAREG|SAREG|SOREG|SNAME,	TWORD|TPOINT|TFLOAT,
		NAREG|NASR,	RESC1,
		"	move A1,AR\n", },

{ OPLTYPE,	INBREG,
	SANY,	TLL,
	SCON,	TLL,
		NBREG,	RESC1,
		"	dmove A1,ZO\n", },

{ OPLTYPE,	INBREG,
	SANY,	TLL|TDOUBLE,
	SANY,	TLL|TDOUBLE,
		NBREG|NBSR,	RESC1,
		"	dmove A1,AR\n", },

{ OPLTYPE,	INAREG,
	SOREG,		TSHORT|TUSHORT|TCHAR|TUCHAR,
	SOREG,		TSHORT|TUSHORT|TCHAR|TUCHAR,
		NASR,	RESC1,
		"ZU", },

{ OPLTYPE,	INAREG,
	SNAME,	TUCHAR,
	SNAME,	TUCHAR,
		NAREG|NASR,	RESC1,
		"	ldb A1,[ .long AL ]\n" },

{ OPLTYPE,	INAREG,
	SNAME,	TCHAR,
	SNAME,	TCHAR,
		NAREG|NASR,	RESC1,
		"	ldb A1,[ .long AL ]\n"
		"	ash A1,033\n"
		"	ash A1,-033\n", },
		
{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SNAME,	TSHORT|TUSHORT,
		NAREG|NASR,	RESC1,
		"Zi", },

{ OPLTYPE,	INAREG,
	SANY,	TWORD|TPOINT,
	SCON,	TWORD|TPOINT,
		NAREG|NASR,	RESC1,
		"Zc", },

{ OPLTYPE,	INAREG,
	SAREG|SAREG,	TUSHORT|TUCHAR,
	SAREG|SAREG,	TUSHORT|TUCHAR|TWORD,
		NAREG,	RESC1,
		"	move A1,AL\n", },

/*
 * Negate a word.
 */
{ UMINUS,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TWORD,
		NAREG|NASL,	RESC1,
		"	movn A1,AL\n", },

{ UMINUS,	INAREG,
	SAREG|SAREG,	TWORD,
	SANY,	TCHAR|TUCHAR|TSHORT|TUSHORT,
		0,	RLEFT,
		"	movn AL,AL\n", },

{ UMINUS,	INAREG,
	SAREG|SNAME|SOREG,	TLL,
	SANY,	TLL,
		NAREG|NASR,	RESC1,
		"	dmovn A1,AL\n", },

{ COMPL,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TLL,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	setcm A1,AL\n"
		"	setcm U1,UL\n", },

{ COMPL,	INAREG,
	SAREG|SAREG|SNAME|SOREG,	TWORD,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	setcm A1,AL\n", },

{ COMPL,	INAREG,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT,
	SANY,	TCHAR|TUCHAR|TSHORT|TUSHORT,
		NAREG|NASL,	RESC1,
		"	setcm A1,AL\n", },

/*
 * Arguments to functions.
 */
{ FUNARG,	FOREFF,
	SAREG|SNAME|SOREG,	TWORD|TPOINT|TFLOAT,
	SANY,	TANY,
		0,	RNULL,
		"	push 017,AL\n", },

{ FUNARG,	FOREFF,
	SAREG|SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT,
	SANY,	TANY,
		0,	RNULL,
		"	push 017,AL\n", },

{ FUNARG,	FOREFF,
	SCON,	TCHAR|TUCHAR|TSHORT|TUSHORT|TPOINT|TWORD,
	SANY,	TANY,
		0,	RNULL,
		"	push 017,[ .long AL]\n", },

{ FUNARG,	FOREFF,
	SBREG,	TLL|TDOUBLE,
	SANY,		TANY,
		0,	RNULL,
		"	push 017,AL\n	push 017,UL\n", },

{ STARG,	FOREFF,
	SAREG|SOREG|SNAME|SCON, TANY, 
	SANY,   TSTRUCT,
		0, 0, 
		"ZG", },


# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

{ UMUL, DF( UMUL ), },

{ ASSIGN, DF(ASSIGN), },

{ OPLEAF, DF(NAME), },

{ OPUNARY, DF(UMINUS), },

{ FREE, FREE, FREE,	FREE, FREE, FREE, FREE, FREE, "help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
