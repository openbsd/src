/*	$OpenBSD: tag.pro,v 1.2 1996/09/21 06:23:55 downsj Exp $	*/
/* tag.c */
void do_tag __PARMS((char_u *tag, int type, int count, int forceit));
void do_tags __PARMS((void));
int find_tags __PARMS((char_u *tag, regexp *prog, int *num_file, char_u ***file, int help_only, int forceit));
