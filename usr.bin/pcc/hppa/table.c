/*	$OpenBSD: table.c,v 1.3 2008/04/11 20:45:52 stefan Exp $	*/

/*
 * Copyright (c) 2007 Michael Shalayeff
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

#include "pass2.h"

#define	TLL		TLONGLONG|TULONGLONG
#define	ANYSIGNED	TINT|TLONG|TSHORT|TCHAR
#define	ANYUSIGNED	TUNSIGNED|TULONG|TUSHORT|TUCHAR
#define	ANYFIXED	ANYSIGNED|ANYUSIGNED
#define	TUWORD		TUNSIGNED|TULONG
#define	TSWORD		TINT|TLONG
#define	TWORD		TUWORD|TSWORD
#define	THWORD		TUSHORT|TSHORT
#define	TBYTE		TUCHAR|TCHAR

#define	SHINT	SAREG	/* char, short and int */
#define	ININT	INAREG
#define	SHLL	SBREG	/* shape for long long */
#define	INLL	INBREG
#define	SHFL	SCREG	/* shape for float */
#define	INFL	INCREG
#define	SHDBL	SDREG	/* shape for double */
#define	INDBL	INDREG

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* PCONVs are usually not necessary */
{ PCONV,	INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		"", },
/*
 * A bunch conversions of integral<->integral types
 * There are lots of them, first in table conversions to itself
 * and then conversions from each type to the others.
 */

/* itself to itself, including pointers */

/* convert int,short,char <-> int,short,char. */
{ SCONV,	ININT,
	SHINT,	TBYTE,
	SHINT,	TBYTE,
		0,	RLEFT,
		"", },

{ SCONV,	ININT,
	SHINT,	THWORD,
	SHINT,	THWORD,
		0,	RLEFT,
		"", },

{ SCONV,	ININT,
	SHINT,	TWORD,
	SHINT,	TWORD,
		0,	RLEFT,
		"", },

/* convert pointers to int. */
{ SCONV,	ININT,
	SHINT,	TWORD|TPOINT,
	SANY,	TWORD,
		0,	RLEFT,
		"", },

/* convert (u)longlong to (u)longlong. */
{ SCONV,	INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		0,	RLEFT,
		"", },

/* convert pointers to pointers. */
{ SCONV,	ININT,
	SHINT,	TPOINT,
	SANY,	TPOINT,
		0,	RLEFT,
		"", },

/* convert double <-> ldouble. nothing to do here (or support quads later) */
{ SCONV,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		0,	RLEFT,
		"", },

/* convert float -> double */
{ SCONV,	INFL,
	SHFL,	TFLOAT,
	SHDBL,	TDOUBLE|TLDOUBLE,
		0,	0,
		"\tfcnvff,sgl,dbl AL,AR\n", },

/* convert double -> float */
{ SCONV,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHFL,	TFLOAT,
		0,	0,
		"\tfcnvff,dbl,sgl\tAL,AR\n", },

/* convert int,short,char to (u)long long */
{ SCONV,	INLL,
	SHINT,	ANYSIGNED,
	SANY,	TLL,
		NBREG,	RESC1,
		"\tcopy\tAL,A1\n"
		"\textrs\tAL,0,1,U1\n", },

/* convert unsigned int,short,char to (u)long long */
{ SCONV,	INLL,
	SHINT,	TWORD,
	SANY,	TLL,
		NBREG,	RESC1,
		"\tcopy\tAL,A1\n"
		"\tcopy\t%r0,U1\n", },

/* convert int,short,char (in memory) to float */
{ SCONV,	INFL,
	SOREG,	ANYSIGNED,
	SHFL,	TFLOAT,
		NCREG,	RESC1,
		"\tfldws\tAL,A1\n"
		"\tfcnvxf,sgl,sgl\tA1,A1\n", },

/* convert int,short,char (in memory) to double */
{ SCONV,	INDBL,
	SOREG,	TSWORD,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG,	RESC1,
		"\tfldws\tAL,A1\n"
		"\tfcnvxf,sgl,dbl\tA1,A1\n", },

/* convert (u)long (in memory) to double */
{ SCONV,	INDBL,
	SOREG,	TLL,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG,	RESC1,
		"\tfldds\tAL,A1\n"
		"\tfcnvxf,dbl,dbl\tA1,A1\n", },

/* convert int,short,char (in register) to float */
{ SCONV,	INFL,
	SHINT,	TSWORD,
	SHFL,	TFLOAT,
		NCREG,	RESC1,
		"\tstw,ma\tAL,4(%sp)\n"
		"\tfldws,ma\t-4(%sp),A1\n"
		"\tfcnvxf,sgl,sgl\tA1,A1\n", },

/* convert int,short,char (in register) to double */
{ SCONV,	INDBL,
	SHINT,	TSWORD,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG,	RESC1,
		"\tstw,ma\tAL,4(%sp)\n"
		"\tfldws,mb\t-4(%sp),AR\n"
		"\tfcnvxf,sgl,dbl\tA1,A1\n", },

/* convert (u)long (in register) to double */
{ SCONV,	INDBL,
	SHLL,	TLL,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG,	RESC1,
		"\tldo\t8(%sp),%sp\n"
		"\tstw\tAL,-8(%sp)\n"
		"\tstw\tUL,-4(%sp)\n"
		"\tfldds,mb\t-8(%sp),A1\n"
		"\tfcnvxf,dbl,dbl\tA1,A1\n", },

/* convert char to (unsigned) short/int. */
{ SCONV,	ININT,
	SAREG,	TCHAR,
	SAREG,	THWORD|TWORD,
		NASL|NAREG,	RESC1,
		"\textrs\tAL,31,8,A1\n", },

/* convert unsigned char to (unsigned) short/int. */
{ SCONV,	ININT,
	SAREG,	TUCHAR,
	SAREG,	THWORD|TWORD,
		NASL|NAREG,	RESC1,
		"\textru\tAL,31,8,A1\n", },

/* convert char to (unsigned) long long. */
{ SCONV,	INLL,
	SAREG,	TCHAR,
	SBREG,	TLL,
		NBSL|NBREG,	RESC1,
		"\textrs\tAL,31,8,A1\n\textrs\tA1,0,1,U1", },

/* convert unsigned char to (unsigned) long long. */
{ SCONV,	INLL,
	SAREG,	TUCHAR,
	SBREG,	TLL,
		NBSL|NBREG,	RESC1,
		"\textru\tAL,31,8,A1\n\tcopy\t%r0,U1", },

/* convert short to (unsigned) int. */
{ SCONV,	ININT,
	SAREG,	TSHORT,
	SAREG,	TWORD,
		NASL|NAREG,	RESC1,
		"\textrs\tAL,31,16,A1\n", },

/* convert unsigned short to (unsigned) int. */
{ SCONV,	ININT,
	SAREG,	TUSHORT,
	SAREG,	THWORD,
		NASL|NAREG,	RESC1,
		"\textru\tAL,31,16,A1\n", },

/* convert short to (unsigned) long long. */
{ SCONV,	INLL,
	SAREG,	TSHORT,
	SBREG,	TLL,
		NBSL|NBREG,	RESC1,
		"\textrs\tAL,31,16,A1\n\textrs\tA1,0,1,U1", },

/* convert unsigned short to (unsigned) long long. */
{ SCONV,	INLL,
	SAREG,	TUSHORT,
	SBREG,	TLL,
		NBSL|NBREG,	RESC1,
		"\textru\tAL,31,16,A1\n\tcopy\t%r0,U1", },

/* convert int,short,char (in memory) to int,short,char */
{ SCONV,	ININT,
	SOREG,	TBYTE,
	SHINT,	TBYTE|TPOINT,
		NAREG|NASL,	RESC1,
		"\tldb\tAL,A1\n", },

{ SCONV,	ININT,
	SOREG,	THWORD,
	SHINT,	THWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"\tldh\tAL,A1\n", },

{ SCONV,	ININT,
	SOREG,	TWORD,
	SHINT,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"\tldw\tAL,A1\n", },

/* convert (u)long long (in register) to int,short,char */
{ SCONV,	ININT,
	SHLL,	TLL,
	SHINT,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"\tcopy\tAL,A1\n", },

/* convert (u)long (in memory) to int,short,char */
{ SCONV,	ININT,
	SOREG,	TLL,
	SHINT,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"\tldw\tAL,A1\n", },

/* convert float (in register) to (u)int */
{ SCONV,	ININT,
	SHFL,	TFLOAT,
	SHINT,	TWORD,
		NAREG,	RESC1,
		"\tfcnvfxt,sgl,sgl\tAL,AL\n"
		"\tfstws,ma\tAL,4(%sp)\n"
		"\tldw,mb\t-1(%sp),A1\n", },

/* convert double (in register) to (u)int */
{ SCONV,	ININT,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHINT,	TWORD,
		NCREG|NCSL|NAREG,	RESC2,
		"\tfcnvfxt,dbl,sgl\tAL,A1\n"
		"\tfstws,ma\tA1,4(%sp)\n"
		"\tldw,mb\t-1(%sp),A2\n", },

/* convert float (in register) to (u)long */
{ SCONV,	INLL,
	SHFL,	TFLOAT,
	SHLL,	TLL,
		NDREG|NDSL|NBREG,	RESC2,
		"\tfcnvfxt,sgl,dbl\tAL,A1\n"
		"\tfstds,ma\tA1,8(%sp)\n"
		"\tldw\t-8(%sp),A2\n"
		"\tldw\t-4(%sp),U2\n"
		"\tldo\t-8(%sp),%sp)\n", },

/* convert double (in register) to (u)long */
{ SCONV,	INLL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHLL,	TLL,
		NBREG,	RESC1,
		"\tfcnvfxt,dbl,dbl\tAL,AL\n"
		"\tfstds,ma\tAL,8(%sp)\n"
		"\tldw\t-8(%sp),A1\n"
		"\tldw\t-4(%sp),U1\n"
		"\tldo\t-8(%sp),%sp)\n", },

/*
 * Subroutine calls.
 */

{ CALL,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ UCALL,	FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	0,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ CALL,		ININT,
	SAREG,	TANY,
	SHINT,	ANYFIXED|TPOINT,
		NAREG|NASL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ UCALL,	ININT,
	SAREG,	TANY,
	SHINT,	ANYFIXED|TPOINT,
		NAREG|NASL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ CALL,		INLL,
	SAREG,	TANY,
	SHLL,	TLL,
		NBREG|NBSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ UCALL,	INLL,
	SAREG,	TANY,
	SHLL,	TLL,
		NBREG|NBSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ CALL,		INFL,
	SAREG,	TANY,
	SHFL,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ UCALL,	INFL,
	SAREG,	TANY,
	SHFL,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ CALL,		INDBL,
	SAREG,	TANY,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

{ UCALL,	INDBL,
	SAREG,	TANY,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL,	RESC1,
		"ZP\tblr\t%r0, %rp\n"
		"\tbv,n\t%r0(AL)\n"
		"\tnop\nZC",	},

/*
 * The next rules handle all binop-style operators.
 */
/* TODO fix char/short overflows */

{ PLUS,		INLL,
	SHLL,	TLL,
	SPICON,	TANY,
		NBREG|NBSL,	RESC1,
		"\taddi\tAL,AR,A1\n"
		"\taddc\tUL,%r0,U1\n", },

{ PLUS,		INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		NBREG|NBSL|NBSR,	RESC1,
		"\tadd\tAL,AR,A1\n"
		"\taddc\tUL,UR,U1\n", },

{ PLUS,		INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG|NCSL|NCSR,	RESC1,
		"\tfadd,sgl\tAL,AR,A1\n", },

{ PLUS,		INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL|NDSR,	RESC1,
		"\tfadd,dbl\tAL,AR,A1\n", },

{ PLUS,		ININT,
	SHINT,	ANYFIXED|TPOINT,
	SONE,	TANY,
		NAREG|NASL,	RESC1,
		"\tldo\t1(AL),A1\n", },

{ PLUS,		ININT,
	SHINT,	ANYFIXED|TPOINT,
	SPCON,	TANY,
		NAREG|NASL,	RESC1,
		"\tldo\tCR(AL),A1\n", },

{ MINUS,	INLL,
	SHLL,	TLL,
	SPICON,	TANY,
		NBREG|NBSL|NBSR,	RESC1,
		"\tsubi\tAL,AR,A1\n"
		"\tsubb\tUL,%r0,U1\n", },

{ PLUS,		ININT,
	SHINT,	TANY|TPOINT,
	SPNAME,	TANY,
		NAREG|NASL,	RESC1,
		"\tldo\tAR(AL),A1\n", },

{ MINUS,	INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		NBREG|NBSL|NBSR,	RESC1,
		"\tsub\tAL,AR,A1\n"
		"\tsubb\tUL,UR,U1\n", },

{ MINUS,	INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG|NCSL|NCSR,	RESC1,
		"\tfsub,sgl\tAL,AR,A1\n", },

{ MINUS,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL|NDSR,	RESC1,
		"\tfsub,dbl\tAL,AR,A1\n", },

{ MINUS,	ININT,
	SHINT,	ANYFIXED|TPOINT,
	SONE,	TANY,
		NAREG|NASL,	RESC1,
		"\tldo\t-1(AL),A1\n", },

{ MINUS,	ININT,
	SHINT,	ANYFIXED|TPOINT,
	SPCON,	TANY,
		NAREG|NASL,	RESC1,
		"\tldo\t-CR(AL),A1\n", },

/* Simple reg->reg ops */
{ OPSIMP,	ININT,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"\tO\tAL,AR,A1\n", },

{ OPSIMP,	INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		NBREG|NBSL|NBSR,	RESC1,
		"\tO\tAL,AR,A1\n"
		"\tO\tUL,UR,U1\n", },

/*
 * The next rules handle all shift operators.
 */
{ LS,	ININT,
	SHINT,	ANYFIXED,
	SCON,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"\tzdep\tAL,31-AR,32-AR,A1\n", },

{ LS,	ININT,
	SHINT,	ANYFIXED,
	SHINT,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"\tsubi\t31,AR,A1\n"
		"\tmtsar\tA1\n"
		"\tzvdep\tAL,32,A1\n", },

{ RS,	INLL,
	SHLL,	TLONGLONG,
	SCON,	ANYFIXED,
		NBREG|NBSL,	RESC1,
		"\tshd\tUL,AL,31-AR,A1\n"
		"\textrs\tUL,31-AR,32,U1\n", },

{ RS,	INLL,
	SHLL,	TULONGLONG,
	SCON,	ANYFIXED,
		NBREG|NBSL,	RESC1,
		"\tshd\tUL,AL,AR,A1\n"
		"\textru\tUL,31-AR,32,U1\n", },

{ RS,	ININT,
	SHINT,	ANYSIGNED,
	SCON,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"\textrs\tAL,31-AR,32,A1\n", },

{ RS,	ININT,
	SHINT,	ANYUSIGNED,
	SCON,	ANYFIXED,
		NAREG|NASL,	RESC1,
		"\textru\tAL,31-AR,32,A1\n", },

/* TODO the following should be split into mtsar and actual shift parts */
{ RS,	ININT,
	SHINT,	ANYSIGNED,
	SHINT,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"\tsubi\t31,AR,A1\n"
		"\tmtsar\tA1\n"
		"\tvextrs\tAL,32,A1\n", },

{ RS,	ININT,
	SHINT,	ANYUSIGNED,
	SHINT,	ANYFIXED,
		NAREG|NASR,	RESC1,
		"\tsubi\t31,AR,A1\n"
		"\tmtsar\tA1\n"
		"\tvextru\tAL,32,A1\n", },

/*
 * The next rules takes care of assignments. "=".
 */

{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TWORD|TPOINT,
	SPCON,	TANY,
		0,	RDEST,
		"\tldi\tAR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	TBYTE,
	SHINT,	TBYTE,
		0,	RDEST,
		"\tstb\tAR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	THWORD,
	SHINT,	THWORD,
		0,	RDEST,
		"\tsth\tAR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SOREG,	TWORD|TPOINT,
	SHINT,	TWORD|TPOINT,
		0,	RDEST,
		"\tstw\tAR,AL\n", },

{ ASSIGN,	FOREFF|INLL,
	SOREG,	TLL,
	SHLL,	TLL,
		0,	RDEST,
		"\tstw\tAR,AL\n"
		"\tstw\tUR,UL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SHINT,	TBYTE,
	SOREG,	TBYTE,
		0,	RDEST,
		"\tldb\tAR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SHINT,	THWORD,
	SOREG,	THWORD,
		0,	RDEST,
		"\tldh\tAR,AL\n", },

{ ASSIGN,	FOREFF|INAREG,
	SHINT,	TWORD|TPOINT,
	SOREG,	TWORD|TPOINT,
		0,	RDEST,
		"\tldw\tAR,AL\n", },

{ ASSIGN,	FOREFF|INLL,
	SHLL,	TLL,
	SOREG,	TLL,
		0,	RDEST,
		"\tldw\tAR,AL\n"
		"\tldw\tUR,UL\n", },

{ ASSIGN,	FOREFF|ININT,
	SHINT,	TWORD|TPOINT,
	SHINT,	TWORD|TPOINT,
		0,	RDEST,
		"\tcopy\tAR,AL\n", },

{ ASSIGN,	FOREFF|ININT,
	SHINT,	ANYFIXED,
	SHINT,	ANYFIXED,
		0,	RDEST,
		"\tcopy\tAR,AL\n", },

{ ASSIGN,	FOREFF|ININT,
	SFLD,	TANY,
	SPIMM,	TANY,
		0,	RDEST,
		"\tdepi\tAR,31-H,S,AL\n", },

{ ASSIGN,	FOREFF|ININT,
	SFLD,	TANY,
	SHINT,	TANY,
		0,	RDEST,
		"\tdep\tAR,31-H,S,AL\n", },

{ ASSIGN,	FOREFF|INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		0,	RDEST,
		"\tcopy\tAR,AL\n"
		"\tcopy\tUR,UL\n", },

{ ASSIGN,	FOREFF|INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		0,	RDEST,
		"\tfcpy,sgl\tAR,AL\n", },

{ ASSIGN,	FOREFF|INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"\tfcpy,dbl\tAR,AL\n", },

{ ASSIGN,	FOREFF|INFL,
	SHFL,	TFLOAT,
	SOREG,	TFLOAT,
		0,	RDEST,
		"\tfldws\tAR,AL\n", },

{ ASSIGN,	FOREFF|INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SOREG,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"\tfldds\tAR,AL\n", },

{ ASSIGN,	FOREFF|INFL,
	SOREG,	TFLOAT,
	SHFL,	TFLOAT,
		0,	RDEST,
		"\tfstws\tAR,AL\n", },

{ ASSIGN,	FOREFF|INDBL,
	SOREG,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		0,	RDEST,
		"\tfstds\tAR,AL\n", },

/*
 * DIV/MOD/MUL
 */
{ DIV,	INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG|NCSL|NCSR,	RESC1,
		"\tfdiv,sgl\tAL,AR,A1\n", },

{ DIV,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL|NDSR,	RESC1,
		"\tfdiv,dbl\tAL,AR,A1\n", },

{ MUL,	INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG|NCSL|NCSR,	RESC1,
		"\tfmul,sgl\tAL,AR,A1\n", },

{ MUL,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL|NDSR,	RESC1,
		"\tfmul,dbl\tAL,AR,A1\n", },

/*
 * Indirection operators.
 */
{ UMUL,	INLL,
	SANY,	TANY,
	SOREG,	TLL,
		NBREG,	RESC1,
		"\tldw\tAL,A1\n"
		"\tldw\tUL,U1\n", },

{ UMUL,	ININT,
	SANY,	TPOINT|TWORD,
	SOREG,	TPOINT|TWORD,
		NAREG|NASL,	RESC1,
		"\tldw\tAL,A1\n", },

{ UMUL,	ININT,
	SANY,	TANY,
	SOREG,	THWORD,
		NAREG|NASL,	RESC1,
		"\tldh\tAL,A1\n", },

{ UMUL,	ININT,
	SANY,	TANY,
	SOREG,	TBYTE,
		NAREG|NASL,	RESC1,
		"\tldb\tAL,A1\n", },

{ UMUL,	INDBL,
	SANY,	TANY,
	SOREG,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL,	RESC1,
		"\tfldds\tAL,A1\n", },

{ UMUL,	INFL,
	SANY,	TANY,
	SOREG,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"\tfldws\tAL,A1\n", },

/*
 * Logical/branching operators
 */
{ OPLOG,	FORCC,
	SHLL,	TLL,
	SHLL,	TLL,
		0,	0,
		"ZD", },

{ OPLOG,	FORCC,
	SHINT,	ANYFIXED|TPOINT,
	SPIMM,	ANYFIXED|TPOINT,
		0,	0,
		"\tcomib,O\tAR,AL,LC\n\tnop\n", },

{ OPLOG,	FORCC,
	SHINT,	ANYFIXED|TPOINT,
	SHINT,	ANYFIXED|TPOINT,
		0,	0,
		"\tcomb,O\tAR,AL,LC\n\tnop\n", },

{ OPLOG,	FORCC,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		0,	RESCC,
		"\tfcmp,sgl,!O\tAR,AL\n"
		"\tftest\n"
		"\tb\tLC\n"
		"\tnop", },

{ OPLOG,	FORCC,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		0,	RESCC,
		"\tfcmp,dbl,!O\tAR,AL\n"
		"\tftest\n"
		"\tb\tLC\n"
		"\tnop", },

/*
 * Jumps.
 */
{ GOTO,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"\tb\tLL\n\tnop\n", },

#ifdef GCC_COMPAT
{ GOTO,		FOREFF,
	SAREG,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"\tbv\t%r0(AL)\n\tnop\n", },
#endif


/*
 * Convert LTYPE to reg.
 */

{ OPLTYPE,	INAREG,
	SAREG,	TANY,
	SNAME,	TANY,
		0,	RDEST,
		"\taddil\tUR,gp\n", },

{ OPLTYPE,	INLL,
	SANY,	TANY,
	SOREG,	TLL,
		NBREG|NBSL,	RESC1,
		"\tldw\tAL,A1\n"
		"\tldw\tUL,U1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SOREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"\tldw\tAL,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SAREG,	TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"\tcopy\tAL,A1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SPCON,	ANYFIXED,
		NAREG,		RESC1,
		"\tldi\tAR,A1\n", },

{ OPLTYPE,	INLL,
	SANY,	TANY,
	SPCON,	TLL,
		NAREG,		RESC1,
		"\tldi\tAR,A1\n"
		"\tcopy\t%r0,U1\n", },

{ OPLTYPE,	ININT,
	SANY,	TANY,
	SCON,	TWORD|TPOINT,
		NAREG,		RESC1,
		"\tldil\tUR,A1\n"
		"\tldo\tAR(A1),A1\n", },

{ OPLTYPE,	INLL,
	SHLL,	TLL,
	SCON,	TLL,
		NBREG,		RESC1,
		"\tldil\tUR,A1\n"
		"\tldo\tAR(A1),A1\n"
		"\tldil\tUR>>32,U1\n"
		"\tldo\tAR>>32(A1),U1\n", },

{ OPLTYPE,	INCREG,
	SANY,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG,	RESC1,
		"\tfldws\tAL,A1\n", },

{ OPLTYPE,	INDREG,
	SANY,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG,	RESC1,
		"\tfldds\tAL,A1\n", },

/*
 * Negate a word.
 */
{ UMINUS,	INLL,
	SHLL,	TLL,
	SHLL,	TLL,
		NBREG|NBSL,	RESC1,
		"\tsub\t%r0,AL,A1\n"
		"\tsubb\t%r0,UL,A1\n", },

{ UMINUS,	ININT,
	SHINT,	TWORD,
	SHINT,	TWORD,
		NAREG|NASL,	RESC1,
		"\tsub\t%r0,AL,A1\n", },

{ UMINUS,	INFL,
	SHFL,	TFLOAT,
	SHFL,	TFLOAT,
		NCREG|NCSL,	RESC1,
		"\tfsub,sgl\t%fr0,AL,A1\n", },

{ UMINUS,	INDBL,
	SHDBL,	TDOUBLE|TLDOUBLE,
	SHDBL,	TDOUBLE|TLDOUBLE,
		NDREG|NDSL,	RESC1,
		"\tfsub,dbl\t%fr0,AL,A1\n", },

{ COMPL,	INLL,
	SHLL,	TLL,
	SANY,	TANY,
		NBREG|NBSL,	RESC1,
		"\tuaddcm\t%r0,AL,A1\n"
		"\tuaddcm\t%r0,UL,U1\n", },

{ COMPL,	ININT,
	SHINT,	ANYFIXED,
	SANY,	TANY,
		NAREG|NASL,	RESC1,
		"\tuaddcm\t%r0,AL,A1\n", },

/*
 * Arguments to functions.
 */

{ STARG,	FOREFF,
	SAREG|SOREG|SNAME|SCON,	TANY,
	SANY,	TSTRUCT,
		NSPECIAL|NAREG,	0,
		"ZF", },

/*
 * struct field ops
 */
{ FLD,	ININT,
	SHINT,	TANY,
	SFLD,	ANYSIGNED,
		NAREG|NASL,	RESC1,
		"\textrs\tAL,31-H,S,A1\n", },

{ FLD,	ININT,
	SHINT,	TANY,
	SFLD,	ANYUSIGNED,
		NAREG|NASL,	RESC1,
		"\textru\tAL,31-H,S,A1\n", },

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

{ UMUL, DF( UMUL ), },

{ ASSIGN, DF(ASSIGN), },

{ STASG, DF(STASG), },

{ FLD, DF(FLD), },

{ OPLEAF, DF(NAME), },

/* { INIT, DF(INIT), }, */

{ OPUNARY, DF(UMINUS), },

{ OPANY, DF(BITYPE), },

{ FREE,		FREE,
	FREE,	FREE,
	FREE,	FREE,
		FREE,	FREE,
		"HELP; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);
