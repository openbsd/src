/*	$OpenBSD: mark.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* mark.c */
int setmark __PARMS((int c));
void setpcmark __PARMS((void));
void checkpcmark __PARMS((void));
FPOS *movemark __PARMS((int count));
FPOS *getmark __PARMS((int c, int changefile));
void fmarks_check_names __PARMS((BUF *buf));
int check_mark __PARMS((FPOS *pos));
void clrallmarks __PARMS((BUF *buf));
char_u *fm_getname __PARMS((struct filemark *fmark));
void do_marks __PARMS((char_u *arg));
void do_jumps __PARMS((void));
void mark_adjust __PARMS((linenr_t line1, linenr_t line2, long amount, long amount_after));
void set_last_cursor __PARMS((WIN *win));
int read_viminfo_filemark __PARMS((char_u *line, FILE *fp, int force));
void write_viminfo_filemarks __PARMS((FILE *fp));
int write_viminfo_marks __PARMS((FILE *fp_out));
void copy_viminfo_marks __PARMS((char_u *line, FILE *fp_in, FILE *fp_out, int count, int eof));
