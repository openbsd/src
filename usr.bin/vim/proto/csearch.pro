/*	$OpenBSD: csearch.pro,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* csearch.c */
void do_sub __PARMS((linenr_t lp, linenr_t up, char_u *cmd, char_u **nextcommand, int use_old));
void do_glob __PARMS((int type, linenr_t lp, linenr_t up, char_u *cmd));
int read_viminfo_sub_string __PARMS((char_u *line, FILE *fp, int force));
void write_viminfo_sub_string __PARMS((FILE *fp));
