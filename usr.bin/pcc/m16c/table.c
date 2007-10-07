/*	$OpenBSD: table.c,v 1.1 2007/10/07 17:58:51 otto Exp $	*/

#include "pass2.h"

# define ANYSIGNED TINT|TLONG|TCHAR
# define ANYUSIGNED TUNSIGNED|TULONG|TUCHAR
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define TL TLONG|TULONG
# define TWORD TUNSIGNED|TINT
# define TCH TCHAR|TUCHAR

struct optab table[] = {
/* First entry must be an empty entry */
{ -1, FOREFF, SANY, TANY, SANY, TANY, 0, 0, "", },

/* (signed) char -> int/pointer */
{ SCONV,	INAREG,
	SCREG,		TCHAR,
	SANY,		TINT|TPOINT,
		NAREG,	RESC1,
		"	mov.b AL, A1\n\texts.b A1\n", },

/* (unsigned) char -> int/pointer */
{ SCONV,	INAREG,
	SCREG,		TUCHAR,
	SANY,		TINT|TPOINT,
		NAREG,	RESC1,
		"	mov.b AL, A1\n", },
    
/* unsigned char -> long */
{ SCONV,	INAREG,
	SCREG,		TUCHAR,
	SANY,		TL,
		NAREG|NASL,	RESC1,
		"	mov.b AL, A1\n	mov.w #0,U1\n", },

/* int or pointer -> (unsigned) long */
{ SCONV,	INAREG,
	SAREG|SNAME,	TWORD|TPOINT,
	SANY,			TL,
		NAREG|NASL,	RESC1,
		"	mov.w AL,A1\n	mov.w #0,U1\n", },

/* char -> (signed) long */
{ SCONV,	INAREG,
	SAREG|SNAME,	TCHAR,
	SANY,			TLONG,
		NAREG|NASL,	RESC1,
		"	exts.b AL\n	exts.w AL\n", },

/* long -> ulong */
{ SCONV,	INAREG,
	SAREG,		TL,
	SANY,			TL,
		0,	RLEFT,
	"", },

/* long -> int or pointer */
{ SCONV,	INAREG,
	SAREG|SOREG|SNAME,	TL,
	SANY,			TWORD|TPOINT,
		NAREG|NASL,	RESC1,
		"	mov.w AL,A1\n", },

/* int -> char */
{ SCONV,	INCREG,
	SAREG,		TWORD,
	SANY,		TCH,
		NCREG,	RESC1,
		"	mov.b AL, A1\n", },

/* int -> long */
{ SCONV,	INAREG,
	SAREG,		TWORD,
	SANY,		TLONG,
		NAREG|NASL,	RESC1,
		"	exts.w AL", },

/* long -> char */
{ SCONV,	INAREG,
	SAREG,		TL,
	SANY,		TCH,
		NAREG|NASL,     RESC1,
		"", },

{ SCONV,	INAREG,
	SAREG,		TPOINT,
	SANY,		TWORD,
		0,	RLEFT,
		"", },

{ PLUS,	INAREG|FOREFF,
	SAREG,	TL,
	SCON|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	add.w AR,AL\n	adc.w UR,UL\n", },

{ MINUS,	INAREG|FOREFF,
	SAREG,	TL,
	SCON|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	sub.w AR,AL\n	sbb.w UR,UL\n", },

{ AND,		INAREG|FOREFF,
	SAREG,			TL,
	SAREG|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	and.w AR,AL\n	and.w UR,UL\n", },

{ ER,		INAREG|FOREFF,
	SAREG,			TL,
	SAREG|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	xor.w AR,AL\n	xor.w UR,UL\n", },

{ OR,		INAREG|FOREFF,
	SAREG,			TL,
	SAREG|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	xor.w AR,AL\n	xor.w UR,UL\n", },

{ COMPL,	INAREG|FOREFF,
	SAREG,			TL,
	SAREG|SNAME|SOREG,	TL,
		0,	RLEFT,
		"	not.w AR,AL\n	not.w UR,UL\n", },
	
{ OPSIMP,	INAREG|FOREFF,
	SAREG,			TWORD|TPOINT,
	SAREG|SNAME|SOREG|SCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	Ow AR,AL\n", },

/* XXX - Is this rule really correct? Having a SAREG shape seems kind of
   strange. Doesn't work. Gives a areg as A1. */
#if 0	
{ OPSIMP,	INBREG,
	SAREG,			TWORD|TPOINT,
	SAREG|SBREG|SNAME|SOREG|SCON,	TWORD|TPOINT,
		NBREG,	RESC1,
		"	++Ow AR,A1\n", },
#endif
	
{ OPSIMP,	INBREG,
	SBREG,			TWORD|TPOINT,
	SAREG|SBREG|SNAME|SOREG|SCON,	TWORD|TPOINT,
		0,	RLEFT,
		"	Ow AR,AL\n", },
	
{ OPSIMP,	INCREG|FOREFF,
	SCREG,			TCH,
	SCREG|SNAME|SOREG|SCON,	TCH,
		0,	RLEFT,
		"	Ob AR,AL\n", },
	
/* XXX - Do these work? check nspecial in order.c */
/* signed integer division */
{ DIV,		INAREG,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
	  /*2*NAREG|NASL|*/NSPECIAL,		RLEFT,
		"	div.w AR\n	mov.w r0,AL\n", },
      //		"	xor.w r2\n	div.w AR\n", },


/* signed integer/char division - separate entry for FOREFF */
{ DIV,		FOREFF,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
		0,		0,
		"", },

#if 0
/* signed char division */
{ DIV,		INCREG,
	SCREG,			TCHAR,
	SCREG|SNAME|SOREG,	TCH,
		2*NCREG|NCSL|NSPECIAL,		RLEFT,
		"	div.b AR\n\tmov.b r0l,AL\n", },
      //		"	xor.w r2\n	div.w AR\n", },
#endif
	
/* signed integer modulus, equal to above */
{ MOD,		INAREG,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
	  /*2*NAREG|NASL|*/NSPECIAL,		RLEFT,
		"	div.w AR\n\tmov r2,AL\n", },

/* signed integer modulus - separate entry for FOREFF */
{ MOD,		FOREFF,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
		0,		0,
		"", },

/* signed integer multiplication */
{ MUL,		INAREG,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
		2*NAREG|NASL|NSPECIAL,		RESC1,
		"	mul.w AL,AR\n", },

{ MUL,		FOREFF,
	SAREG,			TINT,
	SAREG|SNAME|SOREG,	TWORD,
		0,	0,
		"", },

#if 0
{ LS,		INAREG,
	SAREG,	TWORD,
	SCON,		TANY,
		0,	RLEFT,
		"	shl.w AR,AL\n", },
#endif

{ LS,		INAREG,
	SAREG,	TWORD,
	SAREG,	TWORD,
		0,	RLEFT,
		"	push.b r1h\n"
		"	mov.b AR,r1h\n"
		"	shl.w r1h,AL\n"
		"	pop.b r1h\n", },

{ LS,		INAREG,
	SAREG,	TL,
	SAREG,	TWORD,
		0,	RLEFT,
		"	push.b r1h\n"
		"	mov.b AR,r1h\n"
		"	shl.l r1h,ZG\n"
		"	pop.b r1h\n", },

{ RS,		INAREG,
	SAREG,	TWORD,
	SAREG,	TWORD,
		0,	RLEFT,
		"	push.b r1h\n"
		"	mov.b AR,r1h\n"
		"	neg.b r1h\n"
		"	shl.w r1h,AL\n"
		"	pop.b r1h\n", },

{ RS,		INAREG,
	SAREG,	TL,
	SAREG,	TWORD,
		0,	RLEFT,
		"	push.b r1h\n"
		"	mov.b AR,r1h\n"
		"	neg.b r1h\n"
		"	shl.l r1h,ZG\n"
		"	pop.b r1h\n", },

#if 0
{ RS,		INAREG,
	SAREG,	TUNSIGNED,
	SCON,		TANY,
		0,	RLEFT,
		"	shl ZA,AL\n", },

{ RS,		INAREG,
	SAREG,	TINT,
	SCON,		TANY,
		0,	RLEFT,
		"	sha ZA,AL\n", },
#endif

{ OPLOG,	FORCC,
	SAREG|SBREG|SOREG|SNAME,	TL,
	SAREG|SBREG|SOREG|SNAME,	TL,
		0,	0,
		"ZF", },

{ OPLOG,	FORCC,
	SBREG|SOREG,	TWORD|TPOINT,
	SCON,			TWORD|TPOINT,
		0,	RESCC,
		"	cmp.w AR,AL\n", },

{ OPLOG,	FORCC,
	SAREG|SBREG|SOREG|SNAME,	TWORD|TPOINT,
	SAREG|SBREG|SOREG|SNAME,	TWORD|TPOINT,
		0,	RESCC,
		"	cmp.w AR,AL\n", },

{ OPLOG,	FORCC,
	SCREG|SOREG|SNAME,	TCH,
	SCREG|SOREG|SNAME,	TCH,
		0,	RESCC,
		"	cmp.b AR,AL\n", },

{ OPLOG,	FORCC,
	SCREG|SOREG|SNAME,	TCH,
	SCREG|SOREG|SNAME,	TCH,
		0,	RESCC,
		"	cmp.b AR,AL\n", },

{ GOTO,		FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	RNOP,
		"	jmp.w ZC\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON|SNAME|SOREG|SAREG,	TL|TFTN,
		NAREG,	RESC1,
		"	mov.w AR,A1\n	mov.w UR,U1\n", },

{ OPLTYPE,	INAREG,
	SANY,	TANY,
	SCON|SNAME|SOREG|SAREG|SBREG,	TWORD|TPOINT,
		NAREG,	RESC1,
		"	mov.w AR,A1\n", },	

{ OPLTYPE,	INBREG,
	SANY,	TANY,
	SBREG|SCON|SNAME|SOREG|SAREG,	TWORD|TPOINT,
		NBREG,	RESC1,
		"	mov.w AR,A1\n", },	
    /*
{ OPLTYPE,	INAREG,
	SANY,		TANY,
	SCON|SNAME|SOREG,	TCH,
		NAREG,	RESC1,
		"	mov.b AR, A1\n", },

{ OPLTYPE,	INBREG,
	SANY,			TANY,
	SCON|SNAME|SOREG,	TCHAR|TUCHAR,
		NBREG,	RESC1,
		"	mov.b AR,A1\n", },
    */
    
{ OPLTYPE,	INCREG,
	SANY,			TANY,
	SCON|SNAME|SOREG,	TCHAR|TUCHAR,
		NCREG,	RESC1,
		"	mov.b AR,A1\n", },
    
{ COMPL,	INAREG,
	SAREG,	TWORD,
	SANY,		TANY,
		0,	RLEFT,
		"	not.w AL\n", },

{ COMPL,	INCREG,
	SCREG,	TCH,
	SANY,		TANY,
		0,	RLEFT,
		"	not.b AL\n", },
	
/* Push function address */
{ FUNARG,	FOREFF,
	SCON,	TFTN,
	SANY,	TANY,
		0,	RNULL,
		"ZH", },

{ FUNARG,	FOREFF,
	SOREG,	TFTN,
	SANY,	TANY,
		0,	RNULL,
		"ZI", },

{ FUNARG,	FOREFF,
	SNAME|SAREG,	TL|TFTN,
	SANY,	TANY,
		0,	RNULL,
		"	push.w UL\n	push.w AL\n", },

{ FUNARG,	FOREFF,
	SCON|SAREG|SNAME|SOREG,	TWORD|TPOINT,
	SANY,			TANY,
		0,	RNULL,
		"	push.w AL\n", },

{ FUNARG,	FOREFF,
	SAREG|SNAME|SOREG,	TCHAR|TUCHAR,
	SANY,				TANY,
		0,	RNULL,
		"	push.b AL\n", },

/* Match function pointers first */
#if 0
{ ASSIGN,	FOREFF,
	SFTN,	TWORD|TPOINT,
	SFTN,	TWORD|TPOINT,
		NAREG,	0,
		"ZD", },
#endif

{ ASSIGN,	INAREG,
	SAREG,	TFTN,
	SCON,	TFTN,
		0,	RLEFT,
		"ZD", },
    
{ ASSIGN,	INBREG,
	SBREG,	TFTN,
	SCON,	TFTN,
		0,	RLEFT,
		"ZD", },

{ ASSIGN,	INAREG,
	SAREG,	TFTN,
	SBREG|SAREG|SOREG|SNAME,	TFTN,
		0,	RLEFT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },

{ ASSIGN,	INBREG,
	SBREG,	TFTN,
	SBREG|SAREG|SOREG|SNAME,	TFTN,
		0,	RLEFT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },
    
{ ASSIGN,	INAREG,
	SBREG|SAREG|SOREG|SNAME,	TFTN,
	SAREG,	TFTN,
		0,	RRIGHT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },

{ ASSIGN,	INBREG,
	SBREG|SAREG|SOREG|SNAME,	TFTN,
	SBREG,	TFTN,
		0,	RRIGHT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },

/* a reg -> a reg */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RLEFT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	INAREG,
	SBREG|SAREG|SOREG|SNAME,	TL,
	SAREG,	TL,
		0,	RRIGHT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },

{ ASSIGN,	INBREG,
	SBREG|SAREG|SOREG|SNAME,	TL,
	SBREG,	TL,
		0,	RRIGHT,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },
    
{ ASSIGN,	FOREFF,
	SBREG|SAREG|SOREG|SNAME,	TL,
	SCON|SBREG|SAREG|SOREG|SNAME,	TL,
		0,	0,
		"	mov.w AR,AL\n	mov.w UR,UL\n", },

{ ASSIGN,	INAREG|FOREFF,
	SAREG,	TWORD|TPOINT,
	SCON,	TANY,
		0,	RLEFT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	INBREG|FOREFF,
	SBREG,	TWORD|TPOINT,
	SCON,	TANY,
		0,	RLEFT,
		"	mov.w AR,AL\n", },
    
{ ASSIGN,	FOREFF,
	SNAME|SOREG,	TWORD|TPOINT,
	SCON,		TANY,
		0,	0,
		"	mov.w AR,AL\n", },

/* char, oreg/name -> c reg */
{ ASSIGN,	FOREFF|INCREG,
	SCREG,	TCHAR|TUCHAR,
	SOREG|SNAME|SCON,	TCHAR|TUCHAR,
		0,	RLEFT,
		"	mov.b AR,AL\n", },

/* int, oreg/name -> a reg */
{ ASSIGN,	FOREFF|INAREG,
	SAREG,	TWORD|TPOINT,
	SOREG|SNAME,	TWORD|TPOINT,
		0,	RLEFT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	FOREFF|INBREG,
	SBREG,	TWORD|TPOINT,
	SOREG|SNAME,	TWORD|TPOINT,
		0,	RLEFT,
		"	mov.w AR,AL\n", },
    
{ ASSIGN,	FOREFF|INAREG,
	SOREG|SNAME,	TWORD|TPOINT,
	SAREG,	TWORD|TPOINT,
		0,	RRIGHT,
		"	mov.w AR,AL\n", },

{ ASSIGN,	FOREFF|INBREG,
	SOREG|SNAME,	TWORD|TPOINT,
	SBREG,	TWORD|TPOINT,
		0,	RRIGHT,
		"	mov.w AR,AL\n", },
    
{ ASSIGN,	FOREFF|INCREG,
	SOREG|SNAME,	TCHAR|TUCHAR,
	SCREG,	TCHAR|TUCHAR,
		0,	RRIGHT,
		"	mov.b AR,AL\n", },

{ ASSIGN,       FOREFF|INCREG,
        SCREG,    TCHAR|TUCHAR,
        SCREG,  TCHAR|TUCHAR,
                0,      RRIGHT,
                "	mov.b AR,AL\n", },

{ ASSIGN,       FOREFF|INBREG,
        SBREG,    TWORD|TPOINT,
        SBREG,  TWORD|TPOINT,
                0,      RRIGHT,
                "	mov.w AR,AL\n", },
	
    /*
{ MOVE,		FOREFF|INAREG,
	SAREG|SBREG,	TWORD|TPOINT,
	SAREG,			TWORD|TPOINT,
		NAREG,	RESC1,
		"	mov.w AL, AR\n", },
    */
    
{ UMUL, 	INAREG,
	SBREG,	TPOINT|TWORD,
	SANY,  	TFTN,
		NAREG,	RESC1,
		"	mov.w [AL],A1\n	mov.w 2[AL],U1\n", },

{ UMUL, 	INAREG,
	SBREG,	TPOINT|TWORD,
	SANY,  	TPOINT|TWORD,
		NAREG,	RESC1,
		"	mov.w [AL],A1\n", },

{ UMUL, 	INBREG,
	SBREG,	TPOINT|TWORD,
	SANY,  	TPOINT|TWORD,
		NBREG|NBSL,	RESC1,
		"	mov.w [AL],A1\n", },

{ UMUL,		INAREG,
	SBREG,	TCHAR|TUCHAR|TPTRTO,
	SANY,	TCHAR|TUCHAR,
		NAREG,	RESC1,
    		"	mov.b [AL], A1\n", },
    
{ UCALL,	FOREFF,
	SCON,	TANY,
	SANY,	TANY,
		0,	0,
		"	jsr.w CL\nZB", },
    
{ UCALL,	INAREG,
	SCON,	TANY,
	SANY,	TANY,
		NAREG,	RESC1,
		"	jsr.w CL\nZB", },

{ UCALL,        INAREG,
	SNAME|SOREG,	TANY,
	SANY,		TANY,
		NAREG|NASL,     RESC1,  /* should be 0 */
		"	jsri.a AL\nZB", },

{ UCALL,        FOREFF,
	SNAME|SOREG,	TANY,
	SANY,		TANY,
		0,     0,  
		"	jsri.a AL\nZB", },
    
{ UCALL,        INAREG,
	SBREG,   TANY,
	SANY,   TANY,
		NAREG|NASL,     RESC1,  /* should be 0 */
		"	jsri.a [AL]\nZB", },

{ UCALL,        FOREFF,
	SBREG,   TANY,
	SANY,   TANY,
		0,     0,
		"	jsri.a [AL]\nZB", },

    
{ FREE, FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	FREE,	"help; I'm in trouble\n" },
};

int tablesize = sizeof(table)/sizeof(table[0]);

