/*	$OpenBSD: perl_extern.h,v 1.5 2002/02/16 21:27:58 millert Exp $	*/

int perl_end(GS *);
int perl_init(SCR *);
int perl_screen_end(SCR*);
int perl_ex_perl(SCR*, CHAR_T *, size_t, recno_t, recno_t);
int perl_ex_perldo(SCR*, CHAR_T *, size_t, recno_t, recno_t);
#ifdef USE_SFIO
Sfdisc_t* sfdcnewnvi(SCR*);
#endif
