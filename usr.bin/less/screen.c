/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * Routines which deal with the characteristics of the terminal.
 * Uses termcap to be as terminal-independent as possible.
 *
 * {{ Maybe someday this should be rewritten to use curses or terminfo. }}
 */

#include "less.h"
#include "cmd.h"

#if HAVE_TERMIOS_H && HAVE_TERMIOS_FUNCS
#include <termios.h>
#if HAVE_SYS_IOCTL_H && !defined(TIOCGWINSZ)
#include <sys/ioctl.h>
#endif
#else
#if HAVE_TERMIO_H
#include <termio.h>
#else
#include <sgtty.h>
#if HAVE_SYS_IOCTL_H && (defined(TIOCGWINSZ) || defined(TCGETA) || defined(TIOCGETP) || defined(WIOCGETD))
#include <sys/ioctl.h>
#endif
#endif
#endif
#if HAVE_TERMCAP_H
#include <termcap.h>
#endif

#ifndef TIOCGWINSZ
/*
 * For the Unix PC (ATT 7300 & 3B1):
 * Since WIOCGETD is defined in sys/window.h, we can't use that to decide
 * whether to include sys/window.h.  Use SIGPHONE from sys/signal.h instead.
 */
#include <sys/signal.h>
#ifdef SIGPHONE
#include <sys/window.h>
#endif
#endif

#if HAVE_SYS_STREAM_H
#include <sys/stream.h>
#endif
#if HAVE_SYS_PTEM_H
#include <sys/ptem.h>
#endif

#if OS2
#define	DEFAULT_TERM		"ansi"
#else
#define	DEFAULT_TERM		"unknown"
#endif

/*
 * Strings passed to tputs() to do various terminal functions.
 */
static char
	*sc_pad,		/* Pad string */
	*sc_home,		/* Cursor home */
	*sc_addline,		/* Add line, scroll down following lines */
	*sc_lower_left,		/* Cursor to last line, first column */
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
static int tty_fd = -1;

public int auto_wrap;		/* Terminal does \r\n when write past margin */
public int ignaw;		/* Terminal ignores \n immediately after wrap */
public int erase_char, kill_char; /* The user's erase and line-kill chars */
public int werase_char;		/* The user's word-erase char */
public int sc_width, sc_height;	/* Height & width of screen */
public int bo_s_width, bo_e_width;	/* Printing width of boldface seq */
public int ul_s_width, ul_e_width;	/* Printing width of underline seq */
public int so_s_width, so_e_width;	/* Printing width of standout seq */
public int bl_s_width, bl_e_width;	/* Printing width of blink seq */
public int above_mem, below_mem;	/* Memory retained above/below screen */
public int can_goto_line;		/* Can move cursor to any line */

static char *cheaper();

/*
 * These two variables are sometimes defined in,
 * and needed by, the termcap library.
 */
#if MUST_DEFINE_OSPEED
extern short ospeed;	/* Terminal output baud rate */
extern char PC;		/* Pad character */
#endif

extern int quiet;		/* If VERY_QUIET, use visual bell for bell */
extern int know_dumb;		/* Don't complain about a dumb terminal */
extern int back_scroll;
extern int swindow;
extern int no_init;
extern int quit_at_eof;
extern int more_mode;
#if HILITE_SEARCH
extern int hilite_search;
#endif

extern char *tgetstr();
extern char *tgoto();


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
	public void
raw_mode(on)
	int on;
{
	static int curr_on = 0;

	if (on == curr_on)
		return;

	if (tty_fd == -1 && (tty_fd = open("/dev/tty", O_RDWR)) < 0)
		tty_fd = 2;

#if OS2
	signal(SIGINT, SIG_IGN);
	erase_char = '\b';
	kill_char = '\033';
#else
#if HAVE_TERMIOS_H && HAVE_TERMIOS_FUNCS
    {
	struct termios s;
	static struct termios save_term;

	if (on) 
	{
		/*
		 * Get terminal modes.
		 */
		if (tcgetattr(tty_fd, &s) == -1)
			return;

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		save_term = s;
#if HAVE_OSPEED
		switch (cfgetospeed(&s))
		{
#ifdef B0
		case B0: ospeed = 0; break;
#endif
#ifdef B50
		case B50: ospeed = 1; break;
#endif
#ifdef B75
		case B75: ospeed = 2; break;
#endif
#ifdef B110
		case B110: ospeed = 3; break;
#endif
#ifdef B134
		case B134: ospeed = 4; break;
#endif
#ifdef B150
		case B150: ospeed = 5; break;
#endif
#ifdef B200
		case B200: ospeed = 6; break;
#endif
#ifdef B300
		case B300: ospeed = 7; break;
#endif
#ifdef B600
		case B600: ospeed = 8; break;
#endif
#ifdef B1200
		case B1200: ospeed = 9; break;
#endif
#ifdef B1800
		case B1800: ospeed = 10; break;
#endif
#ifdef B2400
		case B2400: ospeed = 11; break;
#endif
#ifdef B4800
		case B4800: ospeed = 12; break;
#endif
#ifdef B9600
		case B9600: ospeed = 13; break;
#endif
#ifdef EXTA
		case EXTA: ospeed = 14; break;
#endif
#ifdef EXTB
		case EXTB: ospeed = 15; break;
#endif
#ifdef B57600
		case B57600: ospeed = 16; break;
#endif
#ifdef B115200
		case B115200: ospeed = 17; break;
#endif
		default: ;
		}
#endif
		erase_char = s.c_cc[VERASE];
		kill_char = s.c_cc[VKILL];
#ifdef VWERASE
		werase_char = s.c_cc[VWERASE];
#else
		werase_char = 0;
#endif

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(0
#ifdef ICANON
			| ICANON
#endif
#ifdef ECHO
			| ECHO
#endif
#ifdef ECHOE
			| ECHOE
#endif
#ifdef ECHOK
			| ECHOK
#endif
#if ECHONL
			| ECHONL
#endif
		);

		s.c_oflag |= (0
#ifdef XTABS
			| XTABS
#else
#ifdef TAB3
			| TAB3
#else
#ifdef OXTABS
			| OXTABS
#endif
#endif
#endif
#ifdef OPOST
			| OPOST
#endif
#ifdef ONLCR
			| ONLCR
#endif
		);

		s.c_oflag &= ~(0
#ifdef ONOEOT
			| ONOEOT
#endif
#ifdef OCRNL
			| OCRNL
#endif
#ifdef ONOCR
			| ONOCR
#endif
#ifdef ONLRET
			| ONLRET
#endif
		);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	if (tcsetattr(tty_fd, TCSANOW, &s) == -1)
		return;
    }
#else
#ifdef TCGETA
    {
	struct termio s;
	static struct termio save_term;

	if (on)
	{
		/*
		 * Get terminal modes.
		 */
		ioctl(tty_fd, TCGETA, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		save_term = s;
#if HAVE_OSPEED
		ospeed = s.c_cflag & CBAUD;
#endif
		erase_char = s.c_cc[VERASE];
		kill_char = s.c_cc[VKILL];
#ifdef VWERASE
		werase_char = s.c_cc[VWERASE];
#else
		werase_char = 0;
#endif

		/*
		 * Set the modes to the way we want them.
		 */
		s.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL);
		s.c_oflag |=  (OPOST|ONLCR|TAB3);
		s.c_oflag &= ~(OCRNL|ONOCR|ONLRET);
		s.c_cc[VMIN] = 1;
		s.c_cc[VTIME] = 0;
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	ioctl(tty_fd, TCSETAW, &s);
    }
#else
    {
	struct sgttyb s;
	static struct sgttyb save_term;

	if (on)
	{
		/*
		 * Get terminal modes.
		 */
		ioctl(tty_fd, TIOCGETP, &s);

		/*
		 * Save modes and set certain variables dependent on modes.
		 */
		save_term = s;
#if HAVE_OSPEED
		ospeed = s.sg_ospeed;
#endif
		erase_char = s.sg_erase;
		kill_char = s.sg_kill;
		werase_char = 0;

		/*
		 * Set the modes to the way we want them.
		 */
		s.sg_flags |= CBREAK;
		s.sg_flags &= ~(ECHO|XTABS);
	} else
	{
		/*
		 * Restore saved modes.
		 */
		s = save_term;
	}
	ioctl(tty_fd, TIOCSETN, &s);
    }
#endif
#endif
#endif
	curr_on = on;
}

	static void
cannot(s)
	char *s;
{
	PARG parg;

	if (know_dumb || more_mode)
		/* 
		 * User knows this is a dumb terminal, so don't tell him.
		 * more doesn't complain about these, either.
		 */
		return;

	parg.p_string = s;
	error("WARNING: terminal cannot %s", &parg);
}

/*
 * Get size of the output screen.
 */
#if OS2
	public void
scrsize()
{
	int s[2];

	_scrsize(s);
	sc_width = s[0];
	sc_height = s[1];
}

#else

	public void
scrsize()
{
	register char *s;
#ifdef TIOCGWINSZ
	struct winsize w;
#else
#ifdef WIOCGETD
	struct uwdata w;
#endif
#endif

	if (tty_fd == -1 && (tty_fd = open("/dev/tty", O_RDWR)) < 0)
		tty_fd = 2;

#ifdef TIOCGWINSZ
	if (ioctl(tty_fd, TIOCGWINSZ, &w) == 0 && w.ws_row > 0)
		sc_height = w.ws_row;
	else
#else
#ifdef WIOCGETD
	if (ioctl(tty_fd, WIOCGETD, &w) == 0 && w.uw_height > 0)
		sc_height = w.uw_height/w.uw_vs;
	else
#endif
#endif
	if ((s = getenv("LINES")) != NULL)
		sc_height = atoi(s);
	else
 		sc_height = tgetnum("li");

	if (sc_height <= 0)
		sc_height = 24;

#ifdef TIOCGWINSZ
 	if (ioctl(tty_fd, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)
		sc_width = w.ws_col;
	else
#ifdef WIOCGETD
	if (ioctl(tty_fd, WIOCGETD, &w) == 0 && w.uw_width > 0)
		sc_width = w.uw_width/w.uw_hs;
	else
#endif
#endif
	if ((s = getenv("COLUMNS")) != NULL)
		sc_width = atoi(s);
	else
 		sc_width = tgetnum("co");

 	if (sc_width <= 0)
  		sc_width = 80;
}
#endif /* OS2 */

/*
 * Take care of the "variable" keys.
 * Certain keys send escape sequences which differ on different terminals
 * (such as the arrow keys, INSERT, DELETE, etc.)
 * Construct the commands based on these keys.
 */
	public void
get_editkeys()
{
	char *sp;
	char *s;
	char tbuf[40];

	static char kfcmdtable[400];
	int sz_kfcmdtable = 0;
	static char kecmdtable[400];
	int sz_kecmdtable = 0;

#define	put_cmd(str,action,tbl,sz) { \
	strcpy(tbl+sz, str);	\
	sz += strlen(str) + 1;	\
	tbl[sz++] = action; }
#define	put_esc_cmd(str,action,tbl,sz) { \
	tbl[sz++] = ESC; \
	put_cmd(str,action,tbl,sz); }

#define	put_fcmd(str,action)	put_cmd(str,action,kfcmdtable,sz_kfcmdtable)
#define	put_ecmd(str,action)	put_cmd(str,action,kecmdtable,sz_kecmdtable)
#define	put_esc_fcmd(str,action) put_esc_cmd(str,action,kfcmdtable,sz_kfcmdtable)
#define	put_esc_ecmd(str,action) put_esc_cmd(str,action,kecmdtable,sz_kecmdtable)

	/*
	 * Look at some interesting keys and see what strings they send.
	 * Create commands (both command keys and line-edit keys).
	 */

	/* RIGHT ARROW */
	sp = tbuf;
	if ((s = tgetstr("kr", &sp)) != NULL)
	{
		put_ecmd(s, EC_RIGHT);
		put_esc_ecmd(s, EC_W_RIGHT);
	}
	
	/* LEFT ARROW */
	sp = tbuf;
	if ((s = tgetstr("kl", &sp)) != NULL)
	{
		put_ecmd(s, EC_LEFT);
		put_esc_ecmd(s, EC_W_LEFT);
	}
	
	/* UP ARROW */
	sp = tbuf;
	if ((s = tgetstr("ku", &sp)) != NULL) 
	{
		put_ecmd(s, EC_UP);
		put_fcmd(s, A_B_LINE);
	}
		
	/* DOWN ARROW */
	sp = tbuf;
	if ((s = tgetstr("kd", &sp)) != NULL) 
	{
		put_ecmd(s, EC_DOWN);
		put_fcmd(s, A_F_LINE);
	}

	/* PAGE UP */
	sp = tbuf;
	if ((s = tgetstr("kP", &sp)) != NULL) 
	{
		put_fcmd(s, A_B_SCREEN);
	}

	/* PAGE DOWN */
	sp = tbuf;
	if ((s = tgetstr("kN", &sp)) != NULL) 
	{
		put_fcmd(s, A_F_SCREEN);
	}
	
	/* HOME */
	sp = tbuf;
	if ((s = tgetstr("kh", &sp)) != NULL) 
	{
		put_ecmd(s, EC_HOME);
	}

	/* END */
	sp = tbuf;
	if ((s = tgetstr("@7", &sp)) != NULL) 
	{
		put_ecmd(s, EC_END);
	}

	/* DELETE */
	sp = tbuf;
	if ((s = tgetstr("kD", &sp)) == NULL) 
	{
		/* Use DEL (\177) if no "kD" termcap. */
		tbuf[1] = '\177';
		tbuf[2] = '\0';
		s = tbuf+1;
	}
	put_ecmd(s, EC_DELETE);
	put_esc_ecmd(s, EC_W_DELETE);
		
	/* BACKSPACE */
	tbuf[0] = ESC;
	tbuf[1] = erase_char;
	tbuf[2] = '\0';
	put_ecmd(tbuf, EC_W_BACKSPACE);

	if (werase_char != 0)
	{
		tbuf[0] = werase_char;
		tbuf[1] = '\0';
		put_ecmd(tbuf, EC_W_BACKSPACE);
	}

	/*
	 * Register the two tables.
	 */
	add_fcmd_table(kfcmdtable, sz_kfcmdtable);
	add_ecmd_table(kecmdtable, sz_kecmdtable);
}

#if DEBUG
	static void
get_debug_term()
{
	auto_wrap = 1;
	ignaw = 1;
	so_s_width = so_e_width = 0;
	bo_s_width = bo_e_width = 0;
	ul_s_width = ul_e_width = 0;
	bl_s_width = bl_e_width = 0;
	sc_s_keypad =	"(InitKey)";
	sc_e_keypad =	"(DeinitKey)";
	sc_init =	"(InitTerm)";
	sc_deinit =	"(DeinitTerm)";
	sc_eol_clear =	"(ClearEOL)";
	sc_eos_clear =	"(ClearEOS)";
	sc_clear =	"(ClearScreen)";
	sc_move =	"(Move<%d,%d>)";
	sc_s_in =	"(SO+)";
	sc_s_out =	"(SO-)";
	sc_u_in =	"(UL+)";
	sc_u_out =	"(UL-)";
	sc_b_in =	"(BO+)";
	sc_b_out =	"(BO-)";
	sc_bl_in =	"(BL+)";
	sc_bl_out =	"(BL-)";
	sc_visual_bell ="(VBell)";
	sc_backspace =	"(BS)";
	sc_home =	"(Home)";
	sc_lower_left =	"(LL)";
	sc_addline =	"(AddLine)";
}
#endif

/*
 * Get terminal capabilities via termcap.
 */
	public void
get_term()
{
	char *sp;
	register char *t1, *t2;
	register int hard;
	char *term;
	char termbuf[2048];

	static char sbuf[1024];

#ifdef OS2
	/*
	 * Make sure the termcap database is available.
	 */
	sp = getenv("TERMCAP");
	if (sp == NULL || *sp == '\0')
	{
		char *termcap;
		if ((sp = homefile("termcap.dat")) != NULL)
		{
			termcap = (char *) ecalloc(strlen(sp)+9, sizeof(char));
			sprintf(termcap, "TERMCAP=%s", sp);
			free(sp);
			putenv(termcap);
		}
	}
#endif
	/*
	 * Find out what kind of terminal this is.
	 */
 	if ((term = getenv("TERM")) == NULL)
 		term = DEFAULT_TERM;
 	if (tgetent(termbuf, term) <= 0)
 		strcpy(termbuf, "dumb:hc:");

 	hard = tgetflag("hc");

	/*
	 * Get size of the screen.
	 */
	scrsize();
	pos_init();

#if DEBUG
	if (strncmp(term,"LESSDEBUG",9) == 0)
	{
		get_debug_term();
		return;
	}
#endif /* DEBUG */

	auto_wrap = tgetflag("am");
	ignaw = tgetflag("xn");
	above_mem = tgetflag("da");
	below_mem = tgetflag("db");

	/*
	 * Assumes termcap variable "sg" is the printing width of:
	 * the standout sequence, the end standout sequence,
	 * the underline sequence, the end underline sequence,
	 * the boldface sequence, and the end boldface sequence.
	 */
	if ((so_s_width = tgetnum("sg")) < 0)
		so_s_width = 0;
	so_e_width = so_s_width;

	bo_s_width = bo_e_width = so_s_width;
	ul_s_width = ul_e_width = so_s_width;
	bl_s_width = bl_e_width = so_s_width;

#if HILITE_SEARCH
	if (so_s_width > 0 || so_e_width > 0)
		/*
		 * Disable highlighting by default on magic cookie terminals.
		 * Turning on highlighting might change the displayed width
		 * of a line, causing the display to get messed up.
		 * The user can turn it back on with -g, 
		 * but she won't like the results.
		 */
		hilite_search = 0;
#endif

	/*
	 * Get various string-valued capabilities.
	 */
	sp = sbuf;

#if HAVE_OSPEED
	sc_pad = tgetstr("pc", &sp);
	if (sc_pad != NULL)
		PC = *sc_pad;
#endif

	sc_s_keypad = tgetstr("ks", &sp);
	if (sc_s_keypad == NULL)
		sc_s_keypad = "";
	sc_e_keypad = tgetstr("ke", &sp);
	if (sc_e_keypad == NULL)
		sc_e_keypad = "";
		
	/*
	 * This loses for terminals with termcap entries with ti/te strings
	 * that switch to/from an alternate screen, and we're in quit_at_eof
	 * (eg, more(1)).
	 */
	if (!quit_at_eof && !more_mode) {
		sc_init = tgetstr("ti", &sp);
		sc_deinit = tgetstr("te", &sp);
	}
	if (sc_init == NULL)
		sc_init = "";
	if (sc_deinit == NULL)
		sc_deinit = "";

	sc_eol_clear = tgetstr("ce", &sp);
	if (hard || sc_eol_clear == NULL || *sc_eol_clear == '\0')
	{
		cannot("clear to end of line");
		sc_eol_clear = "";
	}

	sc_eos_clear = tgetstr("cd", &sp);
	if (below_mem && 
		(hard || sc_eos_clear == NULL || *sc_eos_clear == '\0'))
	{
		cannot("clear to end of screen");
		sc_eol_clear = "";
	}

	sc_clear = tgetstr("cl", &sp);
	if (hard || sc_clear == NULL || *sc_clear == '\0')
	{
		cannot("clear screen");
		sc_clear = "\n\n";
	}

	sc_move = tgetstr("cm", &sp);
	if (hard || sc_move == NULL || *sc_move == '\0')
	{
		/*
		 * This is not an error here, because we don't 
		 * always need sc_move.
		 * We need it only if we don't have home or lower-left.
		 */
		sc_move = "";
		can_goto_line = 0;
	} else
		can_goto_line = 1;

	sc_s_in = tgetstr("so", &sp);
	if (hard || sc_s_in == NULL)
		sc_s_in = "";

	sc_s_out = tgetstr("se", &sp);
	if (hard || sc_s_out == NULL)
		sc_s_out = "";

	sc_u_in = tgetstr("us", &sp);
	if (hard || sc_u_in == NULL)
		sc_u_in = sc_s_in;

	sc_u_out = tgetstr("ue", &sp);
	if (hard || sc_u_out == NULL)
		sc_u_out = sc_s_out;

	sc_b_in = tgetstr("md", &sp);
	if (hard || sc_b_in == NULL)
	{
		sc_b_in = sc_s_in;
		sc_b_out = sc_s_out;
	} else
	{
		sc_b_out = tgetstr("me", &sp);
		if (hard || sc_b_out == NULL)
			sc_b_out = "";
	}

	sc_bl_in = tgetstr("mb", &sp);
	if (hard || sc_bl_in == NULL)
	{
		sc_bl_in = sc_s_in;
		sc_bl_out = sc_s_out;
	} else
	{
		sc_bl_out = tgetstr("me", &sp);
		if (hard || sc_bl_out == NULL)
			sc_bl_out = "";
	}

	sc_visual_bell = tgetstr("vb", &sp);
	if (hard || sc_visual_bell == NULL)
		sc_visual_bell = "";

	if (tgetflag("bs"))
		sc_backspace = "\b";
	else
	{
		sc_backspace = tgetstr("bc", &sp);
		if (sc_backspace == NULL || *sc_backspace == '\0')
			sc_backspace = "\b";
	}

	/*
	 * Choose between using "ho" and "cm" ("home" and "cursor move")
	 * to move the cursor to the upper left corner of the screen.
	 */
	t1 = tgetstr("ho", &sp);
	if (hard || t1 == NULL)
		t1 = "";
	if (*sc_move == '\0')
		t2 = "";
	else
	{
		strcpy(sp, tgoto(sc_move, 0, 0));
		t2 = sp;
		sp += strlen(sp) + 1;
	}
	sc_home = cheaper(t1, t2, "home cursor", "|\b^");

	/*
	 * Choose between using "ll" and "cm"  ("lower left" and "cursor move")
	 * to move the cursor to the lower left corner of the screen.
	 */
	t1 = tgetstr("ll", &sp);
	if (hard || t1 == NULL)
		t1 = "";
	if (*sc_move == '\0')
		t2 = "";
	else
	{
		strcpy(sp, tgoto(sc_move, 0, sc_height-1));
		t2 = sp;
		sp += strlen(sp) + 1;
	}
	sc_lower_left = cheaper(t1, t2,
		"move cursor to lower left of screen", "\r");

	/*
	 * Choose between using "al" or "sr" ("add line" or "scroll reverse")
	 * to add a line at the top of the screen.
	 */
	t1 = tgetstr("al", &sp);
	if (hard || t1 == NULL)
		t1 = "";
	t2 = tgetstr("sr", &sp);
	if (hard || t2 == NULL)
		t2 = "";
#if OS2
	if (*t1 == '\0' && *t2 == '\0')
		sc_addline = "";
	else
#endif
	if (above_mem)
		sc_addline = t1;
	else
		sc_addline = cheaper(t1, t2, "scroll backwards", "");
	if (*sc_addline == '\0')
	{
		/*
		 * Force repaint on any backward movement.
		 */
		back_scroll = 0;
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

/*ARGSUSED*/
	static int
inc_costcount(c)
	int c;
{
	costcount++;
	return (c);
}

	static int
cost(t)
	char *t;
{
	costcount = 0;
	tputs(t, sc_height, inc_costcount);
	return (costcount);
}

/*
 * Return the "best" of the two given termcap strings.
 * The best, if both exist, is the one with the lower 
 * cost (see cost() function).
 */
	static char *
cheaper(t1, t2, doit, def)
	char *t1, *t2;
	char *doit;
	char *def;
{
	if (*t1 == '\0' && *t2 == '\0')
	{
		cannot(doit);
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


/*
 * Below are the functions which perform all the 
 * terminal-specific screen manipulation.
 */


/*
 * Initialize terminal
 */
	public void
init()
{
	if (no_init)
		return;
	tputs(sc_init, sc_height, putchr);
	tputs(sc_s_keypad, sc_height, putchr);
	init_done = 1;
}

/*
 * Deinitialize terminal
 */
	public void
deinit()
{
	if (no_init)
		return;
	if (!init_done)
		return;
	tputs(sc_e_keypad, sc_height, putchr);
	tputs(sc_deinit, sc_height, putchr);
	init_done = 0;
}

/*
 * Home cursor (move to upper left corner of screen).
 */
	public void
home()
{
	tputs(sc_home, 1, putchr);
}

/*
 * Add a blank line (called with cursor at home).
 * Should scroll the display down.
 */
	public void
add_line()
{
	tputs(sc_addline, sc_height, putchr);
}

/*
 * Move cursor to lower left corner of screen.
 */
	public void
lower_left()
{
	tputs(sc_lower_left, 1, putchr);
}

/*
 * Goto a specific line on the screen.
 */
	public void
goto_line(slinenum)
	int slinenum;
{
	char *sc_goto;

	sc_goto = tgoto(sc_move, 0, slinenum);
	tputs(sc_goto, 1, putchr);
}

/*
 * Ring the terminal bell.
 */
	public void
bell()
{
	if (quiet == VERY_QUIET)
		vbell();
	else
		putchr('\7');
}

/*
 * Output the "visual bell", if there is one.
 */
	public void
vbell()
{
	if (*sc_visual_bell == '\0')
		return;
	tputs(sc_visual_bell, sc_height, putchr);
}

/*
 * Clear the screen.
 */
	public void
clear()
{
	tputs(sc_clear, sc_height, putchr);
}

/*
 * Clear from the cursor to the end of the cursor's line.
 * {{ This must not move the cursor. }}
 */
	public void
clear_eol()
{
	tputs(sc_eol_clear, 1, putchr);
}

/*
 * Clear the bottom line of the display.
 * Leave the cursor at the beginning of the bottom line.
 */
	public void
clear_bot()
{
	lower_left();
	if (below_mem)
		tputs(sc_eos_clear, 1, putchr);
	else
		tputs(sc_eol_clear, 1, putchr);
}

/*
 * Begin "standout" (bold, underline, or whatever).
 */
	public void
so_enter()
{
	tputs(sc_s_in, 1, putchr);
}

/*
 * End "standout".
 */
	public void
so_exit()
{
	tputs(sc_s_out, 1, putchr);
}

/*
 * Begin "underline" (hopefully real underlining, 
 * otherwise whatever the terminal provides).
 */
	public void
ul_enter()
{
	tputs(sc_u_in, 1, putchr);
}

/*
 * End "underline".
 */
	public void
ul_exit()
{
	tputs(sc_u_out, 1, putchr);
}

/*
 * Begin "bold"
 */
	public void
bo_enter()
{
	tputs(sc_b_in, 1, putchr);
}

/*
 * End "bold".
 */
	public void
bo_exit()
{
	tputs(sc_b_out, 1, putchr);
}

/*
 * Begin "blink"
 */
	public void
bl_enter()
{
	tputs(sc_bl_in, 1, putchr);
}

/*
 * End "blink".
 */
	public void
bl_exit()
{
	tputs(sc_bl_out, 1, putchr);
}

/*
 * Erase the character to the left of the cursor 
 * and move the cursor left.
 */
	public void
backspace()
{
	/* 
	 * Try to erase the previous character by overstriking with a space.
	 */
	tputs(sc_backspace, 1, putchr);
	putchr(' ');
	tputs(sc_backspace, 1, putchr);
}

/*
 * Output a plain backspace, without erasing the previous char.
 */
	public void
putbs()
{
	tputs(sc_backspace, 1, putchr);
}
