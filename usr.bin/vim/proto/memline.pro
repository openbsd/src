/*	$OpenBSD: memline.pro,v 1.2 1996/09/21 06:23:52 downsj Exp $	*/
/* memline.c */
int ml_open __PARMS((void));
void ml_setname __PARMS((void));
void ml_open_files __PARMS((void));
void ml_close __PARMS((BUF *buf, int del_file));
void ml_close_all __PARMS((int del_file));
void ml_close_notmod __PARMS((void));
void ml_timestamp __PARMS((BUF *buf));
void ml_recover __PARMS((void));
int recover_names __PARMS((char_u **fname, int list, int nr));
void ml_sync_all __PARMS((int check_file, int check_char));
void ml_preserve __PARMS((BUF *buf, int message));
char_u *ml_get __PARMS((linenr_t lnum));
char_u *ml_get_pos __PARMS((FPOS *pos));
char_u *ml_get_curline __PARMS((void));
char_u *ml_get_cursor __PARMS((void));
char_u *ml_get_buf __PARMS((BUF *buf, linenr_t lnum, int will_change));
int ml_line_alloced __PARMS((void));
int ml_append __PARMS((linenr_t lnum, char_u *line, colnr_t len, int newfile));
int ml_replace __PARMS((linenr_t lnum, char_u *line, int copy));
int ml_delete __PARMS((linenr_t lnum, int message));
void ml_setmarked __PARMS((linenr_t lnum));
linenr_t ml_firstmarked __PARMS((void));
int ml_has_mark __PARMS((linenr_t lnum));
void ml_clearmarked __PARMS((void));
char_u *get_file_in_dir __PARMS((char_u *fname, char_u *dirname));
