extern int fitpair proto((char *, int));
extern void  putpair proto((char *, datum, datum));
extern datum	getpair proto((char *, datum));
extern int  delpair proto((char *, datum));
extern int  chkpage proto((char *));
extern datum getnkey proto((char *, int));
extern void splpage proto((char *, char *, long));
#ifdef SEEDUPS
extern int duppair proto((char *, datum));
#endif
