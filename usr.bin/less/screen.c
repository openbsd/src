/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * Routines which deal with the characteristics of the terminal.
 * Uses termcap to be as terminal-independent as possible.
 */

#include <sys/ioctl.h>

#include <err.h>
#include <term.h>
#include <termios.h>

#include "cmd.h"
#include "less.h"

#define	DEFAULT_TERM		"unknown"

/*
 * Strings passed to tputs() to do various terminal functions.
 */
static char
	*sc_home,		/* Cursor home */
	*sc_addline,		/* Add line, scroll down following lines */
	*sc_lower_left,		/* Cursor to last line, first column */
	*sc_return,		/* Cursor to beginning of current line */
	*sc_move,		/* General cursor positioning */
	*sc_clear,		/* Clear screen */
	*sc_eol_clear,		/* Clear to end of line */
	*sc_eos_clear,		/* Clear to end of screen */
	*sc_s_in,		/* Enter standout (highlighted) mode */
	*sc_s_out,		/* Exit standout mode */
	*sc_u_in,		/* Enter underline mode */
	*sc_u_out,		/* Exit underline mode */
	*sc_b_in,		/* Enter bold mode */
	*sc_b_out,		/* Exit bold mode */
	*sc_bl_in,		/* Enter blink mode */
	*sc_bl_out,		/* Exit blink mode */
	*sc_visual_bell,	/* Visual bell (flash screen) sequence */
	*sc_backspace,		/* Backspace cursor */
	*sc_s_keypad,		/* Start keypad mode */
	*sc_e_keypad,		/* End keypad mode */
	*sc_init,		/* Startup terminal initialization */
	*sc_deinit;		/* Exit terminal de-initialization */

static int init_done = 0;

int auto_wrap;			/* Terminal does \r\n when write past margin */
int ignaw;			/* Terminal ignores \n immediately after wrap */
int erase_char;			/* The user's erase char */
int erase2_char;		/* The user's other erase char */
int kill_char;			/* The user's line-kill char */
int werase_char;		/* The user's word-erase char */
int sc_width, sc_height;	/* Height & width of screen */
int bo_s_width, bo_e_width;	/* Printing width of boldface seq */
int ul_s_width, ul_e_width;	/* Printing width of underline seq */
int so_s_width, so_e_width;	/* Printing width of standout seq */
int bl_s_width, bl_e_width;	/* Printing width of blink seq */
int can_goto_line;		/* Can move cursor to any line */
int missing_cap = 0;		/* Some capability is missing */
static int above_mem;		/* Memory retained above screen */
static int below_mem;		/* Memory retained below screen */

static int attrmode = AT_NORMAL;
extern int binattr;

static char *cheaper(char *, char *, char *);
static void tmodes(char *, char *, char **, char **, char *, char *);

extern int quiet;		/* If VERY_QUIET, use visual bell for bell */
extern int no_back_scroll;
extern int swindow;
extern int no_init;
extern int no_keypad;
extern int wscroll;
extern int screen_trashed;
extern int tty;
extern int top_scroll;
extern int oldbot;
extern int hilite_search;

/*
 * Change terminal to "raw mode", or restore to "normal" mode.
 * "Raw mode" means
 *	1. An outstanding read will complete on receipt of a single keystroke.
 *	2. Input is not echoed.
 *	3. On output, \n is mapped to \r\n.
 *	4. \t is NOT expanded into spaces.
 *	5. Signal-causing characters such as ctrl-C (interrupt),
 *	   etc. are NOT disabled.
 * It doesn't matter whether an input \n is mapped to \r, or vice versa.
 */
void
raw_mode(int on)
{
	static int curr_on = 0;
	struct termios s;
	static struct termios save_term;
	static int saved_term = 0;

	if (on == curr_on)
		return;
	erase2_char = '\b'; /* in case OS doesn't know about erase2 */

	if (on) {
		/*
		 * Get terminal modes.
		 */
		(void) tcgetattr(tty, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		if (!saved_term) {
			save_term = s;
			saved_term = 1;
		}

		erase_char = s.c_cc[VERASE];
#ifdef VERASE2
		erase2_char = s.c_cc[VERASE2];
#endif
		kill_char = s.c_cc[VKILL];
#ifdef VWERASE
		werase_char = s.c_cc[VWERASE];
#endif

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL);

#ifndef	TAB3
#define	TAB3	0	/* Its a lie, but TAB3 isn't defined by POSIX. */
#endif
		s.c_oflag |= (TAB3 | OPOST | ONLCR);
		s.c_oflag &= ~(OCRNL | ONOCR | ONLRET);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
#ifdef VLNEXT
		s.c_cc[VLNEXT] = 0;
#endif
#ifdef VDSUSP
		s.c_cc[VDSUSP] = 0;
#endif
	} else {
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	(void) tcsetattr(tty, TCSASOFT | TCSADRAIN, &s);
	curr_on = on;
}

/*
 * Some glue to prevent calling termcap functions if tgetent() failed.
 */
static int hardcopy;

/*
 * Get size of the output screen.
 */
static void
scrsize(void)
{
	int sys_height = 0, sys_width = 0, n;
	struct winsize w;
	char *s;

#define	DEF_SC_WIDTH	80
#define	DEF_SC_HEIGHT	24

	if (ioctl(2, TIOCGWINSZ, &w) == 0) {
		if (w.ws_row > 0)
			sys_height = w.ws_row;
		if (w.ws_col > 0)
			sys_width = w.ws_col;
	}

	if (sys_height > 0)
		sc_height = sys_height;
	else if ((s = lgetenv("LINES")) != NULL)
		sc_height = atoi(s);
	else if (!hardcopy && (n = lines) > 0)
		sc_height = n;
	if (sc_height <= 0)
		sc_height = DEF_SC_HEIGHT;

	if (sys_width > 0)
		sc_width = sys_width;
	else if ((s = lgetenv("COLUMNS")) != NULL)
		sc_width = atoi(s);
	else if (!hardcopy && (n = columns) > 0)
		sc_width = n;
	if (sc_width <= 0)
		sc_width = DEF_SC_WIDTH;
}

/*
 * Return the characters actually input by a "special" key.
 */
char *
special_key_str(int key)
{
	char *s;
	static char ctrlk[] = { CONTROL('K'), 0 };

	if (hardcopy)
		return (NULL);

	switch (key) {
	case SK_RIGHT_ARROW:
		s = key_right;
		break;
	case SK_LEFT_ARROW:
		s = key_left;
		break;
	case SK_UP_ARROW:
		s = key_up;
		break;
	case SK_DOWN_ARROW:
		s = key_down;
		break;
	case SK_PAGE_UP:
		s = key_ppage;
		break;
	case SK_PAGE_DOWN:
		s = key_npage;
		break;
	case SK_HOME:
		s = key_home;
		break;
	case SK_END:
		s = key_end;
		break;
	case SK_DELETE:
		s = key_dc;
		if (s == NULL) {
			s = "\177\0";
		}
		break;
	case SK_CONTROL_K:
		s = ctrlk;
		break;
	default:
		return (NULL);
	}
	return (s);
}

/*
 * Get terminal capabilities via termcap.
 */
void
get_term(void)
{
	char *t1, *t2;
	char *term;
	int  err;

	/*
	 * Find out what kind of terminal this is.
	 */
	if ((term = lgetenv("TERM")) == NULL)
		term = DEFAULT_TERM;
	hardcopy = 0;

	if (setupterm(term, 1, &err) < 0) {
		if (err == 1)
			hardcopy = 1;
		else
			errx(1, "%s: unknown terminal type", term);
	}
	if (hard_copy == 1)
		hardcopy = 1;

	/*
	 * Get size of the screen.
	 */
	scrsize();
	pos_init();

	auto_wrap = hardcopy ? 0 : auto_right_margin;
	ignaw = hardcopy ? 0 : eat_newline_glitch;
	above_mem = hardcopy ? 0 : memory_above;
	below_mem = hardcopy ? 0 : memory_below;

	/*
	 * Assumes termcap variable "sg" is the printing width of:
	 * the standout sequence, the end standout sequence,
	 * the underline sequence, the end underline sequence,
	 * the boldface sequence, and the end boldface sequence.
	 */
	if (hardcopy || (so_s_width = magic_cookie_glitch) < 0)
		so_s_width = 0;
	so_e_width = so_s_width;

	bo_s_width = bo_e_width = so_s_width;
	ul_s_width = ul_e_width = so_s_width;
	bl_s_width = bl_e_width = so_s_width;

	if (so_s_width > 0 || so_e_width > 0)
		/*
		 * Disable highlighting by default on magic cookie terminals.
		 * Turning on highlighting might change the displayed width
		 * of a line, causing the display to get messed up.
		 * The user can turn it back on with -g,
		 * but she won't like the results.
		 */
		hilite_search = 0;

	/*
	 * Get various string-valued capabilities.
	 */

	sc_s_keypad = keypad_xmit;
	if (hardcopy || sc_s_keypad == NULL)
		sc_s_keypad = "";
	sc_e_keypad = keypad_local;
	if (hardcopy || sc_e_keypad == NULL)
		sc_e_keypad = "";

	sc_init = enter_ca_mode;
	if (hardcopy || sc_init == NULL)
		sc_init = "";

	sc_deinit = exit_ca_mode;
	if (hardcopy || sc_deinit == NULL)
		sc_deinit = "";

	sc_eol_clear = clr_eol;
	if (hardcopy || sc_eol_clear == NULL || *sc_eol_clear == '\0') {
		missing_cap = 1;
		sc_eol_clear = "";
	}

	sc_eos_clear = clr_eos;
	if (below_mem &&
	    (hardcopy || sc_eos_clear == NULL || *sc_eos_clear == '\0')) {
		missing_cap = 1;
		sc_eos_clear = "";
	}

	sc_clear = clear_screen;
	if (hardcopy || sc_clear == NULL || *sc_clear == '\0') {
		missing_cap = 1;
		sc_clear = "\n\n";
	}

	sc_move = cursor_address;
	if (hardcopy || sc_move == NULL || *sc_move == '\0') {
		/*
		 * This is not an error here, because we don't
		 * always need sc_move.
		 * We need it only if we don't have home or lower-left.
		 */
		sc_move = "";
		can_goto_line = 0;
	} else {
		can_goto_line = 1;
	}

	tmodes(enter_standout_mode, exit_standout_mode, &sc_s_in, &sc_s_out,
	    "", "");
	tmodes(enter_underline_mode, exit_underline_mode, &sc_u_in, &sc_u_out,
	    sc_s_in, sc_s_out);
	tmodes(enter_bold_mode, exit_attribute_mode, &sc_b_in, &sc_b_out,
	    sc_s_in, sc_s_out);
	tmodes(enter_blink_mode, exit_attribute_mode, &sc_bl_in, &sc_bl_out,
	    sc_s_in, sc_s_out);

	sc_visual_bell = flash_screen;
	if (hardcopy || sc_visual_bell == NULL)
		sc_visual_bell = "";

	sc_backspace = "\b";

	/*
	 * Choose between using "ho" and "cm" ("home" and "cursor move")
	 * to move the cursor to the upper left corner of the screen.
	 */
	t1 = cursor_home;
	if (hardcopy || t1 == NULL)
		t1 = "";
	if (*sc_move == '\0') {
		t2 = "";
	} else {
		t2 = estrdup(tparm(sc_move, 0, 0, 0, 0, 0, 0, 0, 0, 0));
	}
	sc_home = cheaper(t1, t2, "|\b^");

	/*
	 * Choose between using "ll" and "cm"  ("lower left" and "cursor move")
	 * to move the cursor to the lower left corner of the screen.
	 */
	t1 = cursor_to_ll;
	if (hardcopy || t1 == NULL)
		t1 = "";
	if (*sc_move == '\0') {
		t2 = "";
	} else {
		t2 = estrdup(tparm(sc_move, sc_height-1,
		    0, 0, 0, 0, 0, 0, 0, 0));
	}
	sc_lower_left = cheaper(t1, t2, "\r");

	/*
	 * Get carriage return string.
	 */
	sc_return = carriage_return;
	if (hardcopy || sc_return == NULL)
		sc_return = "\r";

	/*
	 * Choose between using insert_line or scroll_reverse
	 * to add a line at the top of the screen.
	 */
	t1 = insert_line;
	if (hardcopy || t1 == NULL)
		t1 = "";
	t2 = scroll_reverse;
	if (hardcopy || t2 == NULL)
		t2 = "";
	if (above_mem)
		sc_addline = t1;
	else
		sc_addline = cheaper(t1, t2, "");
	if (*sc_addline == '\0') {
		/*
		 * Force repaint on any backward movement.
		 */
		no_back_scroll = 1;
	}
}

/*
 * Return the cost of displaying a termcap string.
 * We use the trick of calling tputs, but as a char printing function
 * we give it inc_costcount, which just increments "costcount".
 * This tells us how many chars would be printed by using this string.
 * {{ Couldn't we just use strlen? }}
 */
static int costcount;

static int
inc_costcount(int c)
{
	costcount++;
	return (c);
}

static int
cost(char *t)
{
	costcount = 0;
	(void) tputs(t, sc_height, inc_costcount);
	return (costcount);
}

/*
 * Return the "best" of the two given termcap strings.
 * The best, if both exist, is the one with the lower
 * cost (see cost() function).
 */
static char *
cheaper(char *t1, char *t2, char *def)
{
	if (*t1 == '\0' && *t2 == '\0') {
		missing_cap = 1;
		return (def);
	}
	if (*t1 == '\0')
		return (t2);
	if (*t2 == '\0')
		return (t1);
	if (cost(t1) < cost(t2))
		return (t1);
	return (t2);
}

static void
tmodes(char *incap, char *outcap, char **instr, char **outstr,
    char *def_instr, char *def_outstr)
{
	if (hardcopy) {
		*instr = "";
		*outstr = "";
		return;
	}

	*instr = incap;
	*outstr = outcap;

	if (*instr == NULL) {
		/* Use defaults. */
		*instr = def_instr;
		*outstr = def_outstr;
		return;
	}

	if (*outstr == NULL)
		/* No specific out capability; use exit_attribute_mode. */
		*outstr = exit_attribute_mode;
	if (*outstr == NULL)
		/* Don't even have that, use an empty string */
		*outstr = "";
}

/*
 * Below are the functions which perform all the
 * terminal-specific screen manipulation.
 */

/*
 * Initialize terminal
 */
void
init(void)
{
	if (!no_init)
		(void) tputs(sc_init, sc_height, putchr);
	if (!no_keypad)
		(void) tputs(sc_s_keypad, sc_height, putchr);
	if (top_scroll) {
		int i;

		/*
		 * This is nice to terminals with no alternate screen,
		 * but with saved scrolled-off-the-top lines.  This way,
		 * no previous line is lost, but we start with a whole
		 * screen to ourself.
		 */
		for (i = 1; i < sc_height; i++)
			(void) putchr('\n');
	} else
		line_left();
	init_done = 1;
}

/*
 * Deinitialize terminal
 */
void
deinit(void)
{
	if (!init_done)
		return;
	if (!no_keypad)
		(void) tputs(sc_e_keypad, sc_height, putchr);
	if (!no_init)
		(void) tputs(sc_deinit, sc_height, putchr);
	init_done = 0;
}

/*
 * Home cursor (move to upper left corner of screen).
 */
void
home(void)
{
	(void) tputs(sc_home, 1, putchr);
}

/*
 * Add a blank line (called with cursor at home).
 * Should scroll the display down.
 */
void
add_line(void)
{
	(void) tputs(sc_addline, sc_height, putchr);
}

/*
 * Move cursor to lower left corner of screen.
 */
void
lower_left(void)
{
	(void) tputs(sc_lower_left, 1, putchr);
}

/*
 * Move cursor to left position of current line.
 */
void
line_left(void)
{
	(void) tputs(sc_return, 1, putchr);
}

/*
 * Goto a specific line on the screen.
 */
void
goto_line(int slinenum)
{
	(void) tputs(tparm(sc_move, slinenum, 0, 0, 0, 0, 0, 0, 0, 0), 1,
	    putchr);
}

/*
 * Output the "visual bell", if there is one.
 */
void
vbell(void)
{
	if (*sc_visual_bell == '\0')
		return;
	(void) tputs(sc_visual_bell, sc_height, putchr);
}

/*
 * Make a noise.
 */
static void
beep(void)
{
	(void) putchr(CONTROL('G'));
}

/*
 * Ring the terminal bell.
 */
void
ring_bell(void)
{
	if (quiet == VERY_QUIET)
		vbell();
	else
		beep();
}

/*
 * Clear the screen.
 */
void
do_clear(void)
{
	(void) tputs(sc_clear, sc_height, putchr);
}

/*
 * Clear from the cursor to the end of the cursor's line.
 * {{ This must not move the cursor. }}
 */
void
clear_eol(void)
{
	(void) tputs(sc_eol_clear, 1, putchr);
}

/*
 * Clear the current line.
 * Clear the screen if there's off-screen memory below the display.
 */
static void
clear_eol_bot(void)
{
	if (below_mem)
		(void) tputs(sc_eos_clear, 1, putchr);
	else
		(void) tputs(sc_eol_clear, 1, putchr);
}

/*
 * Clear the bottom line of the display.
 * Leave the cursor at the beginning of the bottom line.
 */
void
clear_bot(void)
{
	/*
	 * If we're in a non-normal attribute mode, temporarily exit
	 * the mode while we do the clear.  Some terminals fill the
	 * cleared area with the current attribute.
	 */
	if (oldbot)
		lower_left();
	else
		line_left();

	if (attrmode == AT_NORMAL)
		clear_eol_bot();
	else
	{
		int saved_attrmode = attrmode;

		at_exit();
		clear_eol_bot();
		at_enter(saved_attrmode);
	}
}

void
at_enter(int attr)
{
	attr = apply_at_specials(attr);

	/* The one with the most priority is last.  */
	if (attr & AT_UNDERLINE)
		(void) tputs(sc_u_in, 1, putchr);
	if (attr & AT_BOLD)
		(void) tputs(sc_b_in, 1, putchr);
	if (attr & AT_BLINK)
		(void) tputs(sc_bl_in, 1, putchr);
	if (attr & AT_STANDOUT)
		(void) tputs(sc_s_in, 1, putchr);

	attrmode = attr;
}

void
at_exit(void)
{
	/* Undo things in the reverse order we did them.  */
	if (attrmode & AT_STANDOUT)
		(void) tputs(sc_s_out, 1, putchr);
	if (attrmode & AT_BLINK)
		(void) tputs(sc_bl_out, 1, putchr);
	if (attrmode & AT_BOLD)
		(void) tputs(sc_b_out, 1, putchr);
	if (attrmode & AT_UNDERLINE)
		(void) tputs(sc_u_out, 1, putchr);

	attrmode = AT_NORMAL;
}

void
at_switch(int attr)
{
	int new_attrmode = apply_at_specials(attr);
	int ignore_modes = AT_ANSI;

	if ((new_attrmode & ~ignore_modes) != (attrmode & ~ignore_modes)) {
		at_exit();
		at_enter(attr);
	}
}

int
is_at_equiv(int attr1, int attr2)
{
	attr1 = apply_at_specials(attr1);
	attr2 = apply_at_specials(attr2);

	return (attr1 == attr2);
}

int
apply_at_specials(int attr)
{
	if (attr & AT_BINARY)
		attr |= binattr;
	if (attr & AT_HILITE)
		attr |= AT_STANDOUT;
	attr &= ~(AT_BINARY|AT_HILITE);

	return (attr);
}

/*
 * Output a plain backspace, without erasing the previous char.
 */
void
putbs(void)
{
	(void) tputs(sc_backspace, 1, putchr);
}
