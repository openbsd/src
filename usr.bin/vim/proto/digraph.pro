/*	$OpenBSD: digraph.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* digraph.c */
int do_digraph __PARMS((int c));
int getdigraph __PARMS((int char1, int char2, int meta));
void putdigraph __PARMS((char_u *str));
void listdigraphs __PARMS((void));
