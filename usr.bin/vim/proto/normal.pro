/*	$OpenBSD: normal.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* normal.c */
void normal __PARMS((void));
void do_pending_operator __PARMS((register int c, int nchar, int finish_op, char_u *searchbuff, int *command_busy, int old_col, int gui_yank, int dont_adjust_op_end));
int do_mouse __PARMS((int c, int dir, long count, int fix_indent));
void start_visual_highlight __PARMS((void));
void end_visual_mode __PARMS((void));
int find_ident_under_cursor __PARMS((char_u **string, int find_type));
void clear_showcmd __PARMS((void));
int add_to_showcmd __PARMS((int c, int display_always));
void push_showcmd __PARMS((void));
void pop_showcmd __PARMS((void));
