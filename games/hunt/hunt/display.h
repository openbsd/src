/*	$OpenBSD: display.h,v 1.2 2002/02/16 21:27:10 millert Exp $	*/

void display_open(void);
void display_beep(void);
void display_refresh(void);
void display_clear_eol(void);
void display_put_ch(char);
void display_put_str(char *);
void display_clear_the_screen(void);
void display_move(int, int);
void display_getyx(int *, int *);
void display_end(void);
char display_atyx(int, int);
void display_redraw_screen(void);
int  display_iskillchar(char);
int  display_iserasechar(char);

extern int	cur_row, cur_col;
