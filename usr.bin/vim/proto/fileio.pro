/*	$OpenBSD: fileio.pro,v 1.2 1996/09/21 06:23:51 downsj Exp $	*/
/* fileio.c */
void filemess __PARMS((BUF *buf, char_u *name, char_u *s));
int readfile __PARMS((char_u *fname, char_u *sfname, linenr_t from, int newfile, linenr_t lines_to_skip, linenr_t lines_to_read, int filtering));
int buf_write __PARMS((BUF *buf, char_u *fname, char_u *sfname, linenr_t start, linenr_t end, int append, int forceit, int reset_changed, int filtering));
char_u *modname __PARMS((char_u *fname, char_u *ext));
char_u *buf_modname __PARMS((BUF *buf, char_u *fname, char_u *ext));
int vim_fgets __PARMS((char_u *buf, int size, FILE *fp));
int vim_rename __PARMS((char_u *from, char_u *to));
void check_timestamps __PARMS((void));
void buf_check_timestamp __PARMS((BUF *buf));
void write_lnum_adjust __PARMS((linenr_t offset));
char_u *vim_tempname __PARMS((int extra_char));
