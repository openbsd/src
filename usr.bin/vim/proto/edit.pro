/*	$OpenBSD: edit.pro,v 1.1.1.1 1996/09/07 21:40:29 downsj Exp $	*/
/* edit.c */
int edit __PARMS((int initstr, int startln, long count));
int is_ctrl_x_key __PARMS((int c));
int add_completion_and_infercase __PARMS((char_u *str, int len, char_u *fname, int dir));
int get_literal __PARMS((void));
void insertchar __PARMS((unsigned c, int force_formatting, int second_indent));
void set_last_insert __PARMS((int c));
void beginline __PARMS((int flag));
int oneright __PARMS((void));
int oneleft __PARMS((void));
int cursor_up __PARMS((long n));
int cursor_down __PARMS((long n));
int screengo __PARMS((int dir, long dist));
int onepage __PARMS((int dir, long count));
void halfpage __PARMS((int flag, linenr_t Prenum));
int stuff_inserted __PARMS((int c, long count, int no_esc));
char_u *get_last_insert __PARMS((void));
void replace_push __PARMS((int c));
int replace_pop __PARMS((void));
void replace_flush __PARMS((void));
void fixthisline __PARMS((int (*get_the_indent)(void)));
int in_cinkeys __PARMS((int keytyped, int when, int line_is_empty));
int hkmap __PARMS((int c));
