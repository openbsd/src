/*	$OpenBSD: termlib.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* termlib.c */
int tgetent __PARMS((char *tbuf, char *term));
int tgetflag __PARMS((char *id));
int tgetnum __PARMS((char *id));
char *tgetstr __PARMS((char *id, char **buf));
char *tgoto __PARMS((char *cm, int col, int line));
int tputs __PARMS((char *cp, int affcnt, void (*outc)(unsigned int)));
