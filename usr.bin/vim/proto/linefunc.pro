/*	$OpenBSD: linefunc.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* linefunc.c */
int coladvance __PARMS((colnr_t wcol));
int inc_cursor __PARMS((void));
int inc __PARMS((register FPOS *lp));
int incl __PARMS((register FPOS *lp));
int dec_cursor __PARMS((void));
int dec __PARMS((register FPOS *lp));
int decl __PARMS((register FPOS *lp));
void adjust_cursor __PARMS((void));
