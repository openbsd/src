typedef union {
  char *name;
struct list *list ;

} YYSTYPE;
#define	NEWLINE	258
#define	VERBOSE	259
#define	FILENAME	260
#define	ADDLIB	261
#define	LIST	262
#define	ADDMOD	263
#define	CLEAR	264
#define	CREATE	265
#define	DELETE	266
#define	DIRECTORY	267
#define	END	268
#define	EXTRACT	269
#define	FULLDIR	270
#define	HELP	271
#define	QUIT	272
#define	REPLACE	273
#define	SAVE	274
#define	OPEN	275


extern YYSTYPE yylval;
