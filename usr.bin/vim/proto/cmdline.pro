/*	$OpenBSD: cmdline.pro,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* cmdline.c */
void add_to_history __PARMS((int histype, char_u *new_entry));
char_u *getcmdline __PARMS((int firstc, long count));
int put_on_cmdline __PARMS((char_u *str, int len, int redraw));
void alloc_cmdbuff __PARMS((int len));
int realloc_cmdbuff __PARMS((int len));
void redrawcmdline __PARMS((void));
void compute_cmdrow __PARMS((void));
int do_cmdline __PARMS((char_u *cmdline, int sourcing, int repeating));
int autowrite __PARMS((BUF *buf));
void autowrite_all __PARMS((void));
int do_ecmd __PARMS((int fnum, char_u *fname, char_u *sfname, char_u *command, int hide, linenr_t newlnum, int set_help));
void check_arg_idx __PARMS((void));
void gotocmdline __PARMS((int clr));
int check_fname __PARMS((void));
int getfile __PARMS((int fnum, char_u *fname, char_u *sfname, int setpm, linenr_t lnum));
char_u *ExpandOne __PARMS((char_u *str, char_u *orig, int options, int mode));
char_u *addstar __PARMS((char_u *fname, int len));
int do_source __PARMS((register char_u *fname, int check_other));
void prepare_viminfo_history __PARMS((int len));
int read_viminfo_history __PARMS((char_u *line, FILE *fp));
void finish_viminfo_history __PARMS((void));
void write_viminfo_history __PARMS((FILE *fp));
