typedef union 
  {
    char *str;
    int num;
    int processor;
    unsigned long val;
  } YYSTYPE;
#define	DREG	258
#define	CREG	259
#define	GREG	260
#define	IMMED	261
#define	ADDR	262
#define	INSN	263
#define	NUM	264
#define	ID	265
#define	NL	266
#define	PNUM	267


extern YYSTYPE yylval;
