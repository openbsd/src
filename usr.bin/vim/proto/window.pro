/*	$OpenBSD: window.pro,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* window.c */
void do_window __PARMS((int nchar, long Prenum));
int win_split __PARMS((int new_height, int redraw));
int win_count __PARMS((void));
int make_windows __PARMS((int count));
void win_equal __PARMS((WIN *next_curwin, int redraw));
void close_windows __PARMS((BUF *buf));
void close_window __PARMS((WIN *win, int free_buf));
void close_others __PARMS((int message));
void win_init __PARMS((WIN *wp));
void win_enter __PARMS((WIN *wp, int undo_sync));
WIN *win_alloc __PARMS((WIN *after));
void win_free __PARMS((WIN *wp));
int win_alloc_lsize __PARMS((WIN *wp));
void win_free_lsize __PARMS((WIN *wp));
void screen_new_rows __PARMS((void));
void win_setheight __PARMS((int height));
void win_drag_status_line __PARMS((int offset));
void win_comp_scroll __PARMS((WIN *wp));
void command_height __PARMS((void));
void last_status __PARMS((void));
char_u *file_name_at_cursor __PARMS((int options));
char_u *get_file_name_in_path __PARMS((char_u *ptr, int col, int options));
int min_rows __PARMS((void));
int only_one_window __PARMS((void));
