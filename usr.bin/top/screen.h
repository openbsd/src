/*	$OpenBSD: screen.h,v 1.2 1997/08/22 07:16:30 downsj Exp $	*/

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
extern void init_termcap __P((int));
extern void init_screen __P((void));
extern void end_screen __P((void));
extern void reinit_screen __P((void));
extern void get_screensize __P((void));
extern void standout __P((char *));
extern void clear __P((void));
extern int clear_eol __P((int));
extern void go_home __P((void));
extern int putstdout __P((int));
