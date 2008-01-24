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

#define TUWORD TUNSIGNED|TULONG
#define TSWORD TINT|TLONG
#define TWORD TUWORD|TSWORD

struct optab table[] = {

{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },      /* empty */

{ PCONV,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	! convert between word and pointer\n", },

/* Conversions. TODO: check zeroing on down conversions and signed/unsigned */

{ SCONV,	INAREG,
	SOREG,  TCHAR,
	SAREG,	TSWORD|TSHORT,
		NAREG,	RESC1,
		"	ldsb [AL],A1	! int8->int16/int32\n"
		"	nop\n", },

{ SCONV, 	INAREG,
	SOREG,	TCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG,	RESC1,
		"	ldub [AL],A1	! int8 -> uint16/uint32\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,  TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	ldub [AL],A1	! int8 -> (u)int16/(u)int32\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TCHAR,
	SAREG,	TLONGLONG,
		NAREG,	RESC1,
		"	ldsb [AL],A1	! int8 -> int64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TUCHAR,
	SAREG,	TLONGLONG|TULONGLONG,
		NAREG,	RESC1,
		"	ldub [AL],A1	! uint8 -> (u)int64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	ldsh [AL],A1 	! (u)int16 -> int8\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,  TUCHAR,
		NAREG,	RESC1,
		"	ldsh [AL],A1 	! (u)int16 -> uint8\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TSWORD,
		NAREG,	RESC1,
		"	ldsh [AL],A1 	! int16 -> int32\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TUWORD,
		NAREG,	RESC1,
		"	lduh [AL],A1 	! int16 -> uint32\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TUSHORT,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	lduh [AL],A1 	! uint16 -> int32\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TLONGLONG,
		NAREG,	RESC1,
		"	ldsh [AL],A1 	! int16 -> int64\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSHORT,
	SAREG,	TULONGLONG,
		NAREG,	RESC1,
		"	lduh [AL],A1 	! int16 -> uint64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TUSHORT,
	SAREG,	TLONGLONG|TULONGLONG,
		NAREG,	RESC1,
		"	lduh [AL],A1 	! uint16 -> uint64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	ldsw [AL],A1 	! int32 -> int8\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	lduw [AL],A1 	! int32 -> uint8\n"
		"	nop\n", },
    
{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	ldsw [AL],A1 	! int32 -> int16\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TWORD,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	lduw [AL],A1 	! int32 -> uint16\n"
		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TSWORD,
	SAREG,	TLONGLONG|TULONGLONG,
		NAREG,	RESC1,
		"	ldsw [AL],A1 	! int32 -> (u)int64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TUWORD,
	SAREG,	TLONGLONG|TULONGLONG,
		NAREG,	RESC1,
		"	lduw [AL],A1		! int32 -> (u)int64\n"
      		"	nop\n", },

{ SCONV,	INAREG,
	SOREG,	TLONGLONG|TULONGLONG,
	SAREG,	TCHAR|TUCHAR|TSHORT|TUSHORT|TWORD,
		NAREG,	RESC1,
		"	ldx [AL],A1		! int64 -> (u)int8/16/32\n"
		"	nop\n", },

/* XXX This op is catching all register-to-register conversions. Some of these
 * need special handling. */
		
{ SCONV,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RLEFT,
		"			\t\t! XXX in-register convert\n", },


/* Multiplication and division */

{ MUL,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASR|NASL,	RESC1,
		"	mulx AL,AR,AR		! multiply\n", },

{ DIV,	INAREG,
	SAREG,	TUWORD|TUSHORT|TUCHAR|TULONGLONG,
	SAREG,	TUWORD|TUSHORT|TUCHAR|TULONGLONG,
		NAREG|NASR|NASL,	RESC1,
		"	udivx AL,AR,AR		! unsigned division\n", },

{ DIV,	INAREG,
	SAREG,	TWORD|TSHORT|TCHAR|TLONGLONG,
	SAREG,	TWORD|TSHORT|TCHAR|TLONGLONG,
		NAREG|NASR|NASL,	RESC1,
		"	sdivx AL,AR,AR		! signed division\n", },

		/* TODO MOD */

{ PLUS,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
      		"	add A1,AL,AR\n", },

{ PLUS,	INAREG,
	SAREG,	TANY,
	SCCON,	TWORD,
		NAREG|NASL,	RESC1,
		"	add AL,AR,A1		! add constant to reg\n", },

{ MINUS,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESC1,
      		"	sub AL,AR,AR\n", },

{ MINUS,	INAREG,
	SAREG,	TANY,
	SSCON,	TANY,
		NAREG|NASL,	RESC1,
		"ZB\n", },

{ UMINUS,	INAREG,
	SAREG,	TANY,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sub A1,AL,A1\n", },

/* Shifts */

{ RS,	INAREG,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sra AL,AR,AL			! shift right\n", },

{ RS,	INAREG,
	SAREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	srax AL,AR,AL			! shift right\n", },

{ LS,	INAREG,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sll AL,AR,AL			! shift left\n", },

{ LS,	INAREG,
	SAREG,	TLONGLONG|TULONGLONG,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	sllx AL,AR,AL			! shift left\n", },


/* Assignments */

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TWORD,
	SAREG,		TWORD,
		0,	RDEST,
		"	stw AR,[AL]		! store (u)int32\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TSHORT|TUSHORT,
	SAREG,		TSHORT|TUSHORT,
		0,	RDEST,
        	"	sth AR,[AL]		! store (u)int16\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TCHAR|TUCHAR,
	SAREG,		TCHAR|TUCHAR,
		0,	RDEST,
        	"	stb AR,[AL]		! store (u)int8\n"
		"	nop\n", },	

{ ASSIGN,	FOREFF|INAREG,
	SOREG,		TLONGLONG|TULONGLONG|TPOINT,
	SAREG,		TLONGLONG|TULONGLONG|TPOINT,
		0,	RDEST,
		"	stx AR,[AL] 		! store (u)int64\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SNAME,	TWORD,
	SAREG,	TWORD,
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
	SNAME,	TLONGLONG|TULONGLONG|TPOINT,
	SAREG,	TLONGLONG|TULONGLONG|TPOINT,
		NAREG,	RDEST,
		"	sethi %h44(AL),A1	\t! store (u)int64 into sname\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	stx AR,[A1+%l44(AL)]\n"
		"	nop\n", },

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RDEST,
		"	mov AL, AR		! register move\n", },

/* Comparisons. */

{ EQ,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
                0,      RESCC,
		"	cmp AL,AR\n"
		"	be LC\n"
		"	nop\n", },

{ NE,	FORCC,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SAREG,		TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
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
		"	sub AL,AR,A1		! oplog\n"
		"	O A1,LC\n"
		"	nop\n", },
{ OPLOG,	FORCC,
	SAREG,	TANY,
	SCCON,	TANY,
		NAREG|NASL,	RESCC,
		"	sub AL,AR,A1			! oplog sccon\n"
		"	O A1,LC\n"
		"	nop\n", },


/* Load constants to register. */

{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TLONGLONG|TULONGLONG|TPOINT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const (u)int64 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldx [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TWORD,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const int32 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsw [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUWORD,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const uint32 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	lduw [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TSHORT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const int16 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsh [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUSHORT,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t	! load const uint16 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	lduh [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TCHAR,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t\t! load const int8 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldsb [A1+%l44(AL)],A1\n", },
{ OPLTYPE,	INAREG,
	SCON,		TANY,
	SNAME,		TUCHAR,
		NAREG,	RESC1,
		"	sethi %h44(AL),A1\t! load const uint8 to reg\n"
		"	or A1,%m44(AL),A1\n"
		"	sllx A1,12,A1\n"
		"	ldub [A1+%l44(AL)],A1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"ZA" },


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
	SOREG,	TWORD,
		NAREG,	RESC1,
		"	ldsw [AL],A1		! load int32 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TWORD,
		NAREG,	RESC1,
		"	lduw [AL],A1		! load uint32 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SOREG,	TLONGLONG|TULONGLONG|TPOINT,
		NAREG,	RESC1,
		"	ldx [AL],A1		! load (u)int64 to reg\n"
		"	nop\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SZERO,	TANY,
		NAREG,	RESC1,
		"	mov \%g0,A1\t		! load 0 to reg\n", },


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


/* Indirection. */

{ OPSIMP,	INAREG,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASR|NASL,	RESC1,
		"	O AL,AR,A1\n", },

{ UMUL, INAREG,
	SANY,	TLONGLONG|TULONGLONG|TPOINT,
	SOREG,	TLONGLONG|TULONGLONG|TPOINT,
		NAREG,		RESC1,
		"	ldx [AL],A1		! (u)int64 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TWORD,
	SOREG,	TWORD,
		NAREG,		RESC1,
		"	ldsw [AL],A1		! int32 load\n"
		"	nop\n", },
{ UMUL, INAREG,
	SANY,	TUWORD,
	SOREG,	TUWORD,
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

{ FREE,FREE,FREE,FREE,FREE,FREE,FREE,FREE, "ERR: printing free op\n" },

};

int tablesize = sizeof(table)/sizeof(table[0]);
