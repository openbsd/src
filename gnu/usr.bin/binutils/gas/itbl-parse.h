typedef union 
  {
    char *str;
    int num;
    int processor;
    unsigned long val;
  } YYSTYPE;
#define	DREG	257
#define	CREG	258
#define	GREG	259
#define	IMMED	260
#define	ADDR	261
#define	INSN	262
#define	NUM	263
#define	ID	264
#define	NL	265
#define	PNUM	266


extern YYSTYPE yylval;
