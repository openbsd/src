/*	$OpenBSD: regsub.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* regsub.c */
char_u *regtilde __PARMS((char_u *source, int magic));
int vim_regsub __PARMS((regexp *prog, char_u *source, char_u *dest, int copy, int magic));
