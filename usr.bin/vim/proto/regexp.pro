/*	$OpenBSD: regexp.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* regexp.c */
char_u *skip_regexp __PARMS((char_u *p, int dirc));
regexp *vim_regcomp __PARMS((char_u *exp));
int vim_regexec __PARMS((register regexp *prog, register char_u *string, int at_bol));
int cstrncmp __PARMS((char_u *s1, char_u *s2, int n));
