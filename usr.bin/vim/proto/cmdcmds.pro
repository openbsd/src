/*	$OpenBSD: cmdcmds.pro,v 1.2 1996/09/21 06:23:50 downsj Exp $	*/
/* cmdcmds.c */
void do_ascii __PARMS((void));
void do_align __PARMS((linenr_t start, linenr_t end, int width, int type));
void do_retab __PARMS((linenr_t start, linenr_t end, int new_ts, int forceit));
int do_move __PARMS((linenr_t line1, linenr_t line2, linenr_t n));
void do_copy __PARMS((linenr_t line1, linenr_t line2, linenr_t n));
void do_bang __PARMS((int addr_count, linenr_t line1, linenr_t line2, int forceit, char_u *arg, int do_in, int do_out));
void do_shell __PARMS((char_u *cmd));
int viminfo_error __PARMS((char *message, char_u *line));
int read_viminfo __PARMS((char_u *file, int want_info, int want_marks, int forceit));
void write_viminfo __PARMS((char_u *file, int forceit));
void viminfo_readstring __PARMS((char_u *p));
void viminfo_writestring __PARMS((FILE *fd, char_u *p));
void do_fixdel __PARMS((void));
void print_line __PARMS((linenr_t lnum, int use_number));
void do_file __PARMS((char_u *arg, int forceit));
