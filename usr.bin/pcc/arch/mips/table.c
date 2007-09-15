/*	$OpenBSD: table.c,v 1.2 2007/09/15 22:04:38 ray Exp $	*/
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
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },


/*
 * A bunch conversions of integral<->integral types
 */






/* convert char to (u)short */
{ SCONV,	INTAREG,
	SOREG,  TCHAR,
	SAREG,	TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	lb A1, ZA\n	nop\n", },

/* convert uchar to (u)short */
{ SCONV,	INTAREG,
	SOREG,  TUCHAR,
	SAREG,	TSHORT|TUSHORT,
		NAREG,	RESC1,
		"	lbu A1, ZA\n	nop\n", },

/* convert char to (u)long */
{ SCONV,	INTAREG,
	SOREG,  TCHAR,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	lb A1, ZA\n	nop\n", },

/* convert uchar to (u)long */
{ SCONV,	INTAREG,
	SOREG,  TUCHAR,
	SAREG,	TWORD,
		NAREG,	RESC1,
      		"	lbu A1, ZA\n	nop\n", },

/* convert char to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TCHAR,
	SAREG,	TLL,
		NAREG,	RESC1,
      		"	lb U1, ZA\n"
      		"	nop\n"
      		"	sra A1, U1, 31\n"
      		"	sub A1, $zero, A1\n", },

/* convert uchar to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TUCHAR,
	SAREG,	TLL,
		NAREG,	RESC1,
		"	lbu U1, ZA\n"
      		"	move A1, $zero\n", },


    
    

/* convert (u)short to char */
{ SCONV,	INTAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1, ZA\n	nop\n", },

/* convert (u)short to uchar */
{ SCONV,	INTAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,  TUCHAR,
		NAREG,	RESC1,
		"	lbu A1, ZA\n	nop\n", },


/* convert short to (u)long */
{ SCONV,	INTAREG,
	SOREG,	TSHORT,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	lh A1, ZA\n	nop\n", },

/* convert ushort to (u)long */
{ SCONV,	INTAREG,
	SOREG,	TUSHORT,
	SAREG,	TWORD,
		NAREG,	RESC1,
		"	lhu A1, ZA\n	nop\n", },

/* convert short to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TSHORT,
	SAREG,	TLL,
		NAREG,	RESC1,
      		"	lh U1, ZA\n"
      		"	nop\n"
      		"	sra A1, U1, 31\n"
      		"	sub A1, $zero, A1\n", },

/* convert ushort to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TUSHORT,
	SAREG,	TLL,
		NAREG,	RESC1,
		"	lhu U1, ZA\n"
      		"	move A1, $zero\n", },





/* convert (u)long to char */
{ SCONV,	INTAREG,
	SOREG,	TWORD,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1, ZA\n	nop\n", },

/* convert (u)long to uchar */
{ SCONV,	INTAREG,
	SOREG,	TWORD,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1, ZA\n	nop\n", },
    
/* convert (u)long to short */
{ SCONV,	INTAREG,
	SOREG,	TWORD,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	lh A1, ZA\n	nop\n", },

/* convert (u)long to ushort */
{ SCONV,	INTAREG,
	SOREG,	TWORD,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1, ZA\n	nop\n", },

/* convert long to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TSWORD,
	SAREG,	TLL,
		NAREG,	RESC1,
      		"	lw U1, ZA\n"
      		"	nop\n"
      		"	sra A1, U1, 31\n"
      		"	sub A1, $zero, A1\n", },

/* convert ulong to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TUWORD,
	SAREG,	TLL,
		NAREG,	RESC1,
		"	lw U1, ZA\n"
      		"	move A1, $zero\n", },





/* convert (u)long long to char */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1, ZA\n	nop\n", },

/* convert (u)long long to uchar */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1, ZA\n	nop\n", },
    
/* convert (u)long long to short */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TSHORT,
		NAREG,	RESC1,
		"	lh A1, ZA\n	nop\n", },

/* convert (u)long long to ushort */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1, ZA\n	nop\n", },

/* convert (u)long long to long */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TSWORD,
		NAREG,	RESC1,
      		"	lw U1, ZA\n	nop\n", },

/* convert (u)long long to (u)long long */
{ SCONV,	INTAREG,
	SOREG,	TLL,
	SAREG,	TUWORD,
		NAREG,	RESC1,
      		"	lwu U1, ZA\n	nop\n", },





    

/* Register to register conversion with long long */

{ SCONV,	INTAREG,
	SAREG,	TLL,
	SAREG,	TLL,
		0,	0,
		"", },

{ SCONV,	INTAREG,
	SAREG,	TPOINT|TWORD|SHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TLL,
		NAREG,	0,
		"move A1, AR\n"
		"move U1, $zero\n", },

{ SCONV,	INTAREG,
	SAREG,	TLL,
	SAREG,	TPOINT|TWORD|SHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG,	0,
		"move A1, AL\n", },

    


/* For register to register conversion with bit length <= 32, do nothing */

{ SCONV,	INTAREG,
	SAREG,	TPOINT|TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TPOINT|TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		0,	0,
		"", },


    


    
/*
 * Multiplication and division
 */

{ MUL,	INAREG|FOREFF,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	mult AL, AR\n	mflo A1\n	nop\n	nop\n", },

{ MUL,	INAREG|FOREFF,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	multu AL, AR\n	mflo A1\n	nop\n	nop\n", },

{ DIV,	INAREG|FOREFF,
	SAREG,	TSWORD|TSHORT|TCHAR,
	SAREG,	TSWORD|TSHORT|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	div AL, AR\n	mflo A1\n	nop\n	nop\n", },

{ DIV,	INAREG|FOREFF,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	divu AL, AR\n	mflo A1\n	nop\n	nop\n", },

/*
 * Templates for unsigned values needs to come before OPSIMP 
 */

{ PLUS,	INAREG|FOREFF,
	SAREG,	TLL,
	SAREG,	TLL,
		3*NAREG,	RESC3,
      		"	addu A1, AL, AR\n"
      		"	sltu A2, A1, AR\n"
      		"	addu A3, UL, UR\n"
      		"	addu A3, A3, A2\n", },
    
{ PLUS,	INAREG|FOREFF,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TUSHORT|TSHORT|TCHAR|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	addiu A1, AL, AR\n", },
    
    /*
{ PLUS,	INAREG|FOREFF,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	addu A1, AL, AR\n", },
	
{ MINUS,	INAREG|FOREFF,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
	SAREG,	TUWORD|TUSHORT|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	subu A1, AL, AR\n", },
    */

{ MINUS,	INAREG|FOREFF,
	SAREG,	TLL,
	SAREG,	TLL,
		NAREG|NASR|NASL,	RESC1,
      		"	sltu A1, AL, AR\n"
      		"	subu AR, AL, AR\n"
      		"	subu UR, UL, UR\n"
      		"	subu UR, UR, A1\n", },
    
{ MINUS,	INAREG|FOREFF,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SCON,	TUSHORT|TSHORT|TCHAR|TUCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	subiu A1, AL, AR\n", },


{ UMINUS,	INAREG|FOREFF|INTAREG,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"	neg A1, AL\n", },

    
/* Simple 'op rd, rs, rt' or 'op rt, rs, imm' operations */

{ OPSIMP,	INAREG|FOREFF,
	SAREG,	TLL,
	SAREG,	TLL,
		NAREG|NASR|NASL,	RESC1,
      		"	O A1, AL, AR\n"
      		"	O U1, UL, UR\n", },
    
{ OPSIMP,	INAREG|FOREFF,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	O A1, AL, AR\n", },

{ OPSIMP,	INAREG|FOREFF,
	SAREG,	TWORD|TPOINT|TSHORT|TUSHORT|TUCHAR|TCHAR,
	SCON,	TSHORT|TUSHORT|TUCHAR|TCHAR,
		NAREG|NASR|NASL,	RESC1,
		"	Oi A1, AL, AR\n", },

/*
 * Shift instructions
 */

    /* order.c SPECIAL
{ RS,	INAREG|INTAREG|FOREFF,
	SAREG,	TWORD|TUSHORT|TSHORT|TCHAR|TUCHAR,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srl A1, AL, AR\n", },

{ LS,	INAREG|INTAREG|FOREFF,
	SAREG,	TWORD|TUSHORT|TSHORT|TCHAR|TUCHAR,
	SCON,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sll A1, AL, AR\n", },
    */
    
{ RS,	INAREG|INTAREG|FOREFF,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	srlv A1, AL, AR\n", },

{ LS,	INAREG|INTAREG|FOREFF,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
	SAREG,	TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
		NAREG|NASL,	RESC1,
		"	sllv A1, AL, AR\n", },	

/*
 * Rule for unary one's complement
 */

{ COMPL,        INTAREG,
        SAREG,  TWORD|TSHORT|TUSHORT|TCHAR|TUCHAR,
        SANY,   TANY,
                NAREG|NASL,   RESC1,
                "	not A1, AL\n", },
    
/*
 * The next rules takes care of assignments. "=".
 */

{ ASSIGN,	INTAREG,
	SOREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RRIGHT,
        	"	sw AR, AL\n", },

{ ASSIGN,	INTAREG,
	SOREG,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT,
		0,	RRIGHT,
        	"	sh AR, AL\n", },	

{ ASSIGN,	INTAREG,
	SOREG,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		0,	RRIGHT,
        	"	sb AR, AL\n", },	

{ ASSIGN,	INTAREG,
	SOREG,	TLL,
	SAREG,	TLL,
		0,	RRIGHT,
      		"	sw UR, UL\n"
      		"	sw AR, AL\n", },

{ ASSIGN,	INTAREG, // XXX: Funkar ej A1 == AR
	SNAME,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG,	RRIGHT,
        	"	la A1, AL\n	sw AR, 0(A1)\n", },

{ ASSIGN,	INTAREG,
	SNAME,	TSHORT|TUSHORT,
	SAREG,	TSHORT|TUSHORT,
		NAREG,	RRIGHT,
        	"	la A1, AL\n	sh AR, 0(A1)\n", },

{ ASSIGN,	INTAREG,
	SNAME,	TCHAR|TUCHAR,
	SAREG,	TCHAR|TUCHAR,
		NAREG,	RRIGHT,
        	"	la A1, AL\n	sb AR, 0(A1)\n", },	

{ ASSIGN,	INTAREG,
	SNAME,	TLL,
	SAREG,	TLL,
		0,	RRIGHT,
      		"	sw UR, UL\n"
      		"	sw AR, AL\n", },

{ ASSIGN,	INTAREG,
	SAREG,	TLL,
	SAREG,	TLL,
		0,	RRIGHT,
      		"	move UR, UL\n"
      		"	move AR, AL\n", },
    
{ ASSIGN,	INTAREG|FOREFF,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RLEFT,
        	"	move AL, AR\n", },

#if 0
/* XXX - Stupid rule, shouldn't exist */
{ ASSIGN,	INTAREG,
	SANY,	TANY,
	SAREG,	TANY,
		0,	RLEFT,
        	"	move AL, AR\n", },
#endif
    
/*
 * Compare instructions
 */

{ EQ,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RESCC,
		"	ZQ\n", },

{ NE,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		0,	RESCC,
		"	ZQ\n", },	

{ LE,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESCC,
		"	sub A1, AL, AR\n	ZQ\n", },	

{ LT,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESCC,
		"	sub A1, AL, AR\n	ZQ\n", },	

{ GE,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESCC,
		"	sub A1, AL, AR\n	ZQ\n", },

{ GT,	FORCC,
	SAREG,	TANY,
	SAREG,	TANY,
		NAREG|NASL,	RESCC,
		"	sub A1, AL, AR\n	ZQ\n", },

	
/*
 * Convert LTYPE to reg.
 */


/* from OREG to REG */

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TCHAR,
		NAREG,	RESC1,
		"	lb A1,AR\n	nop\n", },
	
{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TUCHAR,
		NAREG,	RESC1,
		"	lbu A1,AR\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TSHORT,
		NAREG,	RESC1,
		"	lh A1,AR\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TUSHORT,
		NAREG,	RESC1,
		"	lhu A1,AR\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	lw A1, AR\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SOREG,	TLL,
		NAREG,	RESC1,
		"	lw U1, UR\n"
		"	lw A1, AR\n"
      		"	nop\n", },

/* from NAME to REG */

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TCHAR,
		2*NAREG,	RESC1,
		"	la A2, AR\n	lb A1, 0(A2)\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TUCHAR,
		2*NAREG,	RESC1,
		"	la A2, AR\n	lbu A1, 0(A2)\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TSHORT,
		2*NAREG,	RESC1,
		"	la A2, AR\n	lh A1, 0(A2)\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TUSHORT,
		2*NAREG,	RESC1,
		"	la A2, AR\n	lhu A1, 0(A2)\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TWORD|TPOINT,
		2*NAREG,	RESC1,
		"	la A2, AR\n	lw A1, 0(A2)\n	nop\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SNAME,	TLL,
		2*NAREG,	RESC1,
		"	la A2, UR\n"
		"	lw U1, 0(A2)\n"
		"	la A2, AR\n"
		"	lw A1, 0(A2)\n"
      		"	nop\n", },

/* from CON to REG */
{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SCON,	TPOINT,
		NAREG,	RESC1,
		"	la A1, AR\n", },

{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SCON,	TANY,
		NAREG,	RESC1,
		"	li A1, AR\n", },

#if 0
/* Matches REG nodes. XXX - shouldn't be necessary? */
{ OPLTYPE,	INTAREG,
	SANY,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	move A1, AR\n", },
#endif
    
/*
 * Jumps.
 */
{ GOTO, 	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	j LL\n	nop\n", },

/*
 * Subroutine calls.
 */

{ UCALL,        INTAREG|FOREFF,
        SCON,   TANY,
        SANY,   TANY,
	  	NAREG,     RESC1,
                "	addi $sp, $sp, -16\n	jal AL\n	nop\nZC\n", },

/* struct return */
{ USTCALL,      INTAREG|FOREFF,
        SCON,   TANY,
        SANY,   TANY,
                NAREG|NASL,     RESC1,  /* should be 0 */
                "       call CL\nZC", },

/*
 *  Function arguments
 */
	
	
{ FUNARG,       FOREFF,
        SAREG,	TWORD|TPOINT,
        SANY,   TWORD|TPOINT,
                0,      RNULL,
                "	addi $sp, $sp, -4\n	sw AL, 0($sp)\n", },

{ FUNARG,       FOREFF,
        SAREG, TSHORT|TUSHORT,
        SANY,   TSHORT|TUSHORT,
                0,      RNULL,
                "	addi $sp, $sp, -4\n	sh AL, 0($sp)\n", },
{ FUNARG,       FOREFF,
        SAREG, TCHAR|TUCHAR,
        SANY,   TCHAR|TUCHAR,
                0,      RNULL,
                "	addi $sp, $sp, -4\n	sb AL, 0($sp)\n", },    


/*
 * Indirection operators.
 */
{ UMUL, INTAREG,
	SOREG,	TPOINT|TWORD|TPTRTO,
	SANY,	TPOINT|TWORD,
    		NAREG|NASL,     RESC1,
        	"	lw A1, AL\n	nop\n", },

{ UMUL, INTAREG,
	SOREG,	TSHORT|TUSHORT|TPTRTO,
	SANY,	TSHORT|TUSHORT,
    		NAREG|NASL,     RESC1,
        	"	lh A1, AL\n	nop\n", },

{ UMUL, INTAREG,
	SOREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,	TCHAR|TUCHAR,
    		NAREG|NASL,     RESC1,
        	"	lb A1, AL\n	nop\n", },
    
{ UMUL, INTAREG,
	SNAME,	TPOINT|TWORD|TPTRTO,
	SANY,	TPOINT|TWORD,
    		NAREG|NASL,     RESC1,
        	"	la A1, AL\n	lw A1, 0(A1)\n	nop\n", },

{ UMUL, INTAREG,
	SNAME,	TSHORT|TUSHORT|TPTRTO,
	SANY,	TSHORT|TUSHORT,
    		NAREG|NASL,     RESC1,
        	"	la A1, AL\n	lh A1, 0(A1)\n	nop\n", },

{ UMUL, INTAREG,
	SNAME,	TCHAR|TUCHAR|TPTRTO,
	SANY,	TCHAR|TUCHAR,
    		NAREG|NASL,     RESC1,
        	"	la A1, AL\n	lb A1, 0(A1)\n	nop\n", },

{ UMUL, INTAREG,
	SAREG,	TPOINT|TWORD|TPTRTO,
	SANY,	TPOINT|TWORD,
    		NAREG|NASL,     RESC1,
        	"	lw A1, 0(AL)\n	nop\n", },

{ UMUL, INTAREG,
	SAREG,	TSHORT|TUSHORT|TPTRTO,
	SANY,	TSHORT|TUSHORT,
    		NAREG|NASL,     RESC1,
        	"	lh A1, 0(AL)\n	nop\n", },

{ UMUL, INTAREG,
	SAREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,	TCHAR|TUCHAR,
    		NAREG|NASL,     RESC1,
        	"	lb A1, 0(AL)\n	nop\n", },
    
{ FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
