/*	$OpenBSD: screen.h,v 1.3 2002/02/16 21:27:55 millert Exp $	*/

/*
 *  top - a top users display for Unix 4.2
 *
 *  This file contains all the definitions necessary to use the hand-written
 *  screen package in "screen.c"
 */

#define TCputs(str)	tputs(str, 1, putstdout)
#define putcap(str)	(void)((str) != NULL ? TCputs(str) : 0)
#define Move_to(x, y)	TCputs(tgoto(cursor_motion, x, y))

extern char ch_erase;		/* set to the user's erase character */
extern char ch_kill;		/* set to the user's kill  character */
extern char smart_terminal;     /* set if the terminal has sufficient termcap
				   capabilities for normal operation */

/* These are some termcap strings for use outside of "screen.c" */
extern char *cursor_motion;
extern char *clear_line;
extern char *clear_to_end;

/* rows and columns on the screen according to termcap */
extern int  screen_length;
extern int  screen_width;

/* prototypes from screen.c */
extern void init_termcap(int);
extern void init_screen(void);
extern void end_screen(void);
extern void reinit_screen(void);
extern void get_screensize(void);
extern void standout(char *);
extern void clear(void);
extern int clear_eol(int);
extern void go_home(void);
extern int putstdout(int);
