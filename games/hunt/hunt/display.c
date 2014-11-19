/*	$OpenBSD: display.c,v 1.6 2014/11/19 18:55:09 krw Exp $	*/

/*
 * Display abstraction.
 * David Leonard <d@openbsd.org>, 1999. Public domain.
 */

#include <curses.h>

#include "display.h"
#include "hunt.h"

void
display_open()
{
        initscr();
        (void) noecho();
        (void) cbreak();
}

void
display_beep()
{
	beep();
}

void
display_refresh()
{
	refresh();
}

void
display_clear_eol()
{
	clrtoeol();
}

void
display_put_ch(c)
	char c;
{
	addch(c);
}

void
display_put_str(s)
	char *s;
{
	addstr(s);
}

void
display_clear_the_screen()
{
        clear();
        move(0, 0);
        display_refresh();
}

void
display_move(y, x)
	int y, x;
{
	move(y, x);
}

void
display_getyx(yp, xp)
	int *yp, *xp;
{
	getyx(stdscr, *yp, *xp);
}

void
display_end()
{
	endwin();
}

char
display_atyx(y, x)
	int y, x;
{
	int oy, ox;
	char c;

	display_getyx(&oy, &ox);
	c = mvwinch(stdscr, y, x) & 0x7f;
	display_move(oy, ox);
	return (c);
}

void
display_redraw_screen()
{
	clearok(stdscr, TRUE);
	touchwin(stdscr);
}

int
display_iserasechar(ch)
	char ch;
{
	return ch == erasechar();
}

int
display_iskillchar(ch)
	char ch;
{
	return ch == killchar();
}
