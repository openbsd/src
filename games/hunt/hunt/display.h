/*	$OpenBSD: display.h,v 1.1 1999/01/29 07:30:33 d Exp $	*/

void display_open __P((void));
void display_beep __P((void));
void display_refresh __P((void));
void display_clear_eol __P((void));
void display_put_ch __P((char));
void display_put_str __P((char *));
void display_clear_the_screen __P((void));
void display_move __P((int, int));
void display_getyx __P((int *, int *));
void display_end __P((void));
char display_atyx __P((int, int));
void display_redraw_screen __P((void));
int  display_iskillchar __P((char));
int  display_iserasechar __P((char));

extern int	cur_row, cur_col;
