/*	$OpenBSD: tag.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* tag.c */
void do_tag __PARMS((char_u *tag, int type, int count));
void do_tags __PARMS((void));
int find_tags __PARMS((char_u *tag, regexp *prog, int *num_file, char_u ***file, int help_only));
