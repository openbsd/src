/*	$OpenBSD: search.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* search.c */
regexp *myregcomp __PARMS((char_u *pat, int sub_cmd, int which_pat, int options));
void set_reg_ic __PARMS((char_u *pat));
int searchit __PARMS((FPOS *pos, int dir, char_u *str, long count, int options, int which_pat));
int do_search __PARMS((int dirc, char_u *str, long count, int options));
int search_for_exact_line __PARMS((FPOS *pos, int dir, char_u *pat));
int searchc __PARMS((int c, register int dir, int type, long count));
FPOS *findmatch __PARMS((int initc));
FPOS *findmatchlimit __PARMS((int initc, int flags, int maxtravel));
void showmatch __PARMS((void));
int findsent __PARMS((int dir, long count));
int findpar __PARMS((register int dir, long count, int what, int both));
int startPS __PARMS((linenr_t lnum, int para, int both));
int fwd_word __PARMS((long count, int type, int eol));
int bck_word __PARMS((long count, int type, int stop));
int end_word __PARMS((long count, int type, int stop, int empty));
int bckend_word __PARMS((long count, int type, int eol));
int current_word __PARMS((long count, int type));
int current_sent __PARMS((long count));
int current_block __PARMS((int what, long count));
int current_par __PARMS((int type, long count));
int linewhite __PARMS((linenr_t lnum));
void find_pattern_in_path __PARMS((char_u *ptr, int len, int whole, int skip_comments, int type, long count, int action, linenr_t start_lnum, linenr_t end_lnum));
int read_viminfo_search_pattern __PARMS((char_u *line, FILE *fp, int force));
void write_viminfo_search_pattern __PARMS((FILE *fp));
