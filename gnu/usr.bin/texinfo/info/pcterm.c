/* pcterm.c -- How to handle the PC terminal for Info under MS-DOS/MS-Windows.
   $Id: pcterm.c,v 1.1.1.3 2006/07/17 16:03:45 espie Exp $

   Copyright (C) 1998, 1999, 2003, 2004 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


/* WARNING WARNING WARNING!!!
   This probably won't work as is with anything but DJGPP!  However, Borland
   should come close, and other PC compilers will need minor modifications.  */

/* intl/libintl.h defines a macro `gettext' which
   conflicts with conio.h header.  */
#ifdef gettext
# undef gettext
# define gettext _gettext
#endif

#include <pc.h>
#include <keys.h>
#include <conio.h>

#include "variables.h"

extern int speech_friendly;	/* defined in info.c */

/* **************************************************************** */
/*                                                                  */
/*                PC Terminal Output Functions                      */
/*                                                                  */
/* **************************************************************** */

static struct text_info outside_info;  /* holds screen params outside Info */
static unsigned char    norm_attr, inv_attr;

static unsigned const char * find_sequence (int);

/* Turn on reverse video. */
static void
pc_begin_inverse (void)
{
  textattr (inv_attr);
}

/* Turn off reverse video. */
static void
pc_end_inverse (void)
{
  textattr (norm_attr);
}

/* Move the cursor up one line. */
static void
pc_up_line (void)
{
  int x, y;
  ScreenGetCursor (&y, &x);
  ScreenSetCursor (MAX (y-1, 0), x);
}

/* Move the cursor down one line. */
static void
pc_down_line (void)
{
  int x, y;
  ScreenGetCursor (&y, &x);
  ScreenSetCursor (MIN (screenheight-1, y+1), x);
}

/* Clear the entire terminal screen. */
static void
pc_clear_screen (void)
{
  ScreenClear ();
}

/* Clear from the current position of the cursor to the end of the line. */
static void
pc_clear_to_eol (void)
{
  clreol (); /* perhaps to be replaced by a loop */
}

/* Set the global variables SCREENWIDTH and SCREENHEIGHT. */
static void
pc_get_screen_size(void)
{
  /* Current screen dimensions are the default.  */
  if (!outside_info.screenheight)	/* paranoia */
    gettextinfo (&outside_info);
  screenwidth  = outside_info.screenwidth;
  screenheight = outside_info.screenheight;

  /* Environment variable "LINES" overrides the default.  */
  if (getenv ("LINES") != NULL)
    screenheight = atoi (getenv ("LINES"));

  /* Environment variable "INFO_LINES" overrides "LINES".  */
  if (getenv ("INFO_LINES") != NULL)
    screenheight = atoi (getenv ("INFO_LINES"));
}

/* Move the cursor to the terminal location of X and Y. */
static void
pc_goto_xy (x, y)
     int x, y;
{
  ScreenSetCursor (y, x); /* yes, pc.h says ScreenSetCursor (row, column) !! */
}

/* Print STRING to the terminal at the current position. */
static void
pc_put_text (string)
     char *string;
{
  if (speech_friendly)
    fputs (string, stdout);
  else
    cputs (string);
}

/* Ring the terminal bell.  The bell is rung visibly if the terminal is
   capable of doing that, and if terminal_use_visible_bell_p is non-zero. */
static void
pc_ring_bell(void)
{
  if (terminal_has_visible_bell_p && terminal_use_visible_bell_p)
    ScreenVisualBell ();
  else
    {
      printf ("%c",'\a');
      fflush (stdout);
    }
}

/* Print NCHARS from STRING to the terminal at the current position. */
static void
pc_write_chars (string, nchars)
    char *string;
    int nchars;
{
  if (!nchars)
    return;

  if (speech_friendly)
    printf ("%.*s",nchars, string);
  else
    cprintf ("%..*s",nchars, string);
}

/* Scroll an area of the terminal from START to (and excluding) END,
   AMOUNT lines.  If AMOUNT is negative, the lines are scrolled
   towards the top of the screen, else they are scrolled towards the
   bottom of the screen.  The lines of the old region which do not
   overlap the new region are cleared, to mimic terminal operation.  */
static void
pc_scroll_terminal (start, end, amount)
    int start, end, amount;
{
  int line_to_clear = amount > 0 ? start : end + amount;

  /* Move the text.  Note that `movetext' expects 1-based coordinates.  */
  movetext (1, start + 1, ScreenCols (), end, 1, start + amount + 1);

  /* Now clear the lines which were left unoccupied.  */
  if (amount < 0)
    amount = -amount;
  while (amount--)
    {
      ScreenSetCursor (line_to_clear++, 0);
      clreol ();
    }
}

/* Put the screen in the video mode and colors which Info will use.
   Prepare to start using the terminal to read characters singly.  */
static void
pc_prep_terminal (void)
{
  int tty;

  /* Do not set screen height if we already have it, because
     doing so erases the screen.  */
  if (screenheight != ScreenRows ())
    _set_screen_lines (screenheight);

  /* Don't fail if they asked for screen dimensions that their
     hardware cannot support.  */
  screenheight = ScreenRows ();
  screenwidth  = ScreenCols ();

  /* Try setting the colors user asked for.  */
  textattr (norm_attr);
  ScreenClear ();

  /* Switch console reads to binary mode.  */
  tty = fileno (stdin);
#ifdef __DJGPP__
  setmode (tty, O_BINARY);
  __djgpp_set_ctrl_c (1);	/* re-enable SIGINT generation by Ctrl-C */
#endif
}

/* Restore the tty settings back to what they were before we started using
   this terminal. */
static void
pc_unprep_terminal (void)
{
  int tty;

  textattr (outside_info.normattr);

  /* Do not set screen height if we already have it, because
     doing so erases the screen.  */
  if (outside_info.screenheight != ScreenRows ())
    {
      _set_screen_lines (outside_info.screenheight);
      textmode (LASTMODE);
    }
  else
    pc_clear_to_eol ();	/* for text attributes to really take effect */

  /* Switch back to text mode on stdin.  */
  tty = fileno (stdin);
#ifdef __DJGPP__
  setmode (tty, O_TEXT);
#endif
}

/* Initialize the terminal which is known as TERMINAL_NAME.  If this
   terminal doesn't have cursor addressability, `terminal_is_dumb_p'
   becomes nonzero.  The variables SCREENHEIGHT and SCREENWIDTH are set
   to the dimensions that this terminal actually has.  The variable
   TERMINAL_HAS_META_P becomes nonzero if this terminal supports a Meta
   key.  Finally, the terminal screen is cleared. */
static void
pc_initialize_terminal (term_name)
    char *term_name;
{
  char *info_colors;

  if (!term_name)
    {
      term_name = getenv ("TERM");
      if (!term_name)
	term_name = "pc-dos";	/* ``what's in a name?'' */
    }

  /* Get current video information, to be restored later.  */
  if (outside_info.screenwidth == 0)
    gettextinfo (&outside_info);

  /* Current screen colors are the default.  */
  norm_attr    = outside_info.normattr;
  inv_attr     = (((outside_info.normattr &    7) << 4) |
                  ((outside_info.normattr & 0x7f) >> 4));

  /* Does the user want non-default colors?  */
  info_colors = getenv ("INFO_COLORS");
  if ((info_colors != (char *)0) && !speech_friendly)
    {
      /* Decode a color from a string descriptor.
	 The descriptor string is a sequence of color specifiers separated
	 by a non-numeric character.  Each color specifier should represent
	 a small integer which fits into an unsigned char, and can be given
	 in any base supported by strtoul.  Examples of valid descriptors:

		"10 31"
		"0x13/0x45"
		"007.077"

	The separator between two color specifiers can be any character which
	cannot be used in a printed representation of an integer number.  */
      char *endp;
      unsigned long color_desc = strtoul (info_colors, &endp, 0);

      if (color_desc <= UCHAR_MAX)
	{
	  norm_attr = (unsigned char)color_desc;
	  color_desc = strtoul (endp + 1, &endp, 0);
	  if (color_desc <= UCHAR_MAX)
	    inv_attr = (unsigned char)color_desc;
	}
    }

  /* We can scroll.  */
  terminal_can_scroll = 1;

  /* We know how to produce a visible bell, if somebody's looking...  */
  if (!speech_friendly)
    terminal_has_visible_bell_p = 1;

  /* We have a Meta key.  */
  terminal_has_meta_p = 1;

  /* We are *certainly* NOT dumb!  */
  terminal_is_dumb_p = 0;

  pc_get_screen_size ();

  /* Store the arrow keys.  */
  term_ku = (char *)find_sequence (K_Up);
  term_kd = (char *)find_sequence (K_Down);
  term_kr = (char *)find_sequence (K_Right);
  term_kl = (char *)find_sequence (K_Left);

  term_kP = (char *)find_sequence (K_PageUp);
  term_kN = (char *)find_sequence (K_PageDown);

#if defined(INFOKEY)
  term_kh = (char *)find_sequence (K_Home);
  term_ke = (char *)find_sequence (K_End);
  term_ki = (char *)find_sequence (K_Insert);
  term_kx = (char *)find_sequence (K_Delete);
#endif

  /* Set all the hooks to our PC-specific functions.  */
  terminal_begin_inverse_hook       = pc_begin_inverse;
  terminal_end_inverse_hook         = pc_end_inverse;
  terminal_prep_terminal_hook       = pc_prep_terminal;
  terminal_unprep_terminal_hook     = pc_unprep_terminal;
  terminal_up_line_hook             = pc_up_line;
  terminal_down_line_hook           = pc_down_line;
  terminal_clear_screen_hook        = pc_clear_screen;
  terminal_clear_to_eol_hook        = pc_clear_to_eol;
  terminal_get_screen_size_hook     = pc_get_screen_size;
  terminal_goto_xy_hook             = pc_goto_xy;
  terminal_put_text_hook            = pc_put_text;
  terminal_ring_bell_hook           = pc_ring_bell;
  terminal_write_chars_hook         = pc_write_chars;
  terminal_scroll_terminal_hook     = pc_scroll_terminal;
}

/* **************************************************************** */
/*                                                                  */
/*            How to Read Characters From the PC Terminal           */
/*                                                                  */
/* **************************************************************** */

/* This will most certainly work ONLY with DJGPP.  */
#ifdef __DJGPP__

#include <errno.h>
#include <sys/fsext.h>
#include <dpmi.h>

/* Translation table for some special keys.
   Arrow keys which are standard on other keyboards are translated into
   standard ESC-sequences, in case somebody rebinds the simple keys
   (like C-f, C-b, C-n, etc.).

   The strange "\033\061" prefix in some keys is a numeric argument of
   one, which means ``do the next command once''.  It is here so that
   when the according PC key is pressed in the middle of an incremental
   search, Info doesn't see just an ASCII character like `n' or `B',
   and doesn't add it to the search string; instead, it will exit the
   incremental search and then perform the command.  */
static struct
{
  int inkey;
  unsigned char const * const sequence;
} DJGPP_keytab[] = {		   /* these are for moving between nodes... */
  {K_Control_PageDown,  "\033\061n"},
  {K_Control_PageUp,    "\033\061p"},
  {K_Control_Up,        "\033\061u"},
  {K_Control_Down,      "\033\061m"},
  {K_Control_Center,    "\033\061l"},

#if defined(INFOKEY)
  {K_Home,              "\033[H"}, /* ...and these are for moving IN a node */
  {K_End,               "\033[F"}, /* they're Numeric-Keypad-Keys, so       */
#else
  {K_Home,              "\001"},
  {K_End,               "\005"},
#endif
  {K_Left,              "\033[D"}, /* NUMLOCK should be off !!              */
  {K_Right,             "\033[C"},
  {K_Down,              "\033[B"},
  {K_Up,                "\033[A"},
  {K_PageDown,          "\033[G"},
  {K_PageUp,            "\033[I"},
  {K_Control_Left,      "\033b"},
  {K_Control_Right,     "\033f"},
  {K_Control_Home,      "\033<"},
  {K_Control_End,       "\033>"},

#if defined(INFOKEY)
  {K_EHome,             "\033[H"}, /* these are also for moving IN a node */
  {K_EEnd,              "\033[F"}, /* they're the "extended" (Grey) keys  */
#else
  {K_EHome,             "\001"},
  {K_EEnd,              "\005"},
#endif
  {K_ELeft,             "\033[D"},
  {K_ERight,            "\033[C"},
  {K_EDown,             "\033[B"},
  {K_EUp,               "\033[A"},
  {K_EPageDown,         "\033[G"},
  {K_EPageUp,           "\033[I"},
  {K_Control_ELeft,     "\033b"},
  {K_Control_ERight,    "\033f"},
  {K_Control_EHome,     "\033<"},
  {K_Control_EEnd,      "\033>"},

  {K_BackTab,           "\033\011"},
  {K_F1,                "\10"},    /* YEAH, gimme that good old F-one-thing */
  {K_Delete,            "\177"},   /* to make Kp-Del be DEL (0x7f)          */
  {K_EDelete,           "\177"},   /* to make Delete be DEL (0x7f)          */
#if defined(INFOKEY)
  {K_Insert,            "\033[L"},
  {K_EInsert,           "\033[L"},
#endif

  /* These are here to map more Alt-X keys to ESC X sequences.  */
  {K_Alt_Q,             "\033q"},
  {K_Alt_W,             "\033w"},
  {K_Alt_E,             "\033e"},
  {K_Alt_R,             "\033r"},
  {K_Alt_T,             "\033t"},
  {K_Alt_Y,             "\033y"},
  {K_Alt_U,             "\033u"},
  {K_Alt_I,             "\033i"},
  {K_Alt_O,             "\033o"},
  {K_Alt_P,             "\033p"},
  {K_Alt_LBracket,      "\033["},
  {K_Alt_RBracket,      "\033]"},
  {K_Alt_Return,        "\033\015"},
  {K_Alt_A,             "\033a"},
  {K_Alt_S,             "\033s"},
  {K_Alt_D,             "\033d"},
  {K_Alt_F,             "\033f"},
  {K_Alt_G,             "\033g"},
  {K_Alt_H,             "\033h"},
  {K_Alt_J,             "\033j"},
  {K_Alt_K,             "\033k"},
  {K_Alt_L,             "\033l"},
  {K_Alt_Semicolon,     "\033;"},
  {K_Alt_Quote,         "\033'"},
  {K_Alt_Backquote,     "\033`"},
  {K_Alt_Backslash,     "\033\\"},
  {K_Alt_Z,             "\033z"},
  {K_Alt_X,             "\033x"},
  {K_Alt_C,             "\033c"},
  {K_Alt_V,             "\033v"},
  {K_Alt_B,             "\033b"},
  {K_Alt_N,             "\033n"},
  {K_Alt_M,             "\033m"},
  {K_Alt_Comma,         "\033<"}, /* our reader cannot distinguish between */
  {K_Alt_Period,        "\033>"}, /* Alt-. and Alt->, so we cheat a little */
  {K_Alt_Slash,         "\033?"}, /* ditto, to get Alt-?                   */
  {K_Alt_Backspace,     "\033\177"}, /* M-DEL, to delete word backwards */
  {K_Alt_1,             "\033\061"},
  {K_Alt_2,             "\033\062"},
  {K_Alt_3,             "\033\063"},
  {K_Alt_4,             "\033\064"},
  {K_Alt_5,             "\033\065"},
  {K_Alt_6,             "\033\066"},
  {K_Alt_7,             "\033\067"},
  {K_Alt_8,             "\033\070"},
  {K_Alt_9,             "\033\071"},
  {K_Alt_0,             "\033\060"},
  {K_Alt_Dash,          "\033\055"},
  {K_Alt_EPageUp,       "\033\033[I"},
  {K_Alt_EPageDown,     "\033\033[G"},
  {K_Alt_Equals,        "\033\075"},
  {K_Alt_EDelete,       "\033\177"},
  {K_Alt_Tab,           "\033\011"},
  {0, 0}
};

/* Given a key, return the sequence of characters which
   our keyboard driver generates.  */
static unsigned const char *
find_sequence (int key)
{
  int i;

  for (i = 0; DJGPP_keytab[i].inkey; i++)
    if (key == DJGPP_keytab[i].inkey)
      return DJGPP_keytab[i].sequence;

  return (unsigned const char *)NULL;
}

/* Return zero if a key is pending in the
   keyboard buffer, non-zero otherwise.  */
static int
kbd_buffer_empty (void)
{
  __dpmi_regs r;
  int retval;

  r.h.ah = 0x11;	/* Get enhanced keyboard status */
  __dpmi_int (0x16, &r);

  /* If the keyboard buffer is empty, the Zero Flag will be set.  */
  return (r.x.flags & 0x40) == 0x40;
}

/* The buffered characters pending to be read.
   Actually, Info usually reads a single character, but when we
   translate a key into a sequence of characters, we keep them here.  */
static unsigned char buffered[512];

/* Index of the next buffered character to be returned.  */
static int buf_idx;

/* Return the number of characters waiting to be read.  */
long
pc_term_chars_avail (void)
{
  if (buf_idx >= sizeof (buffered)) /* paranoia */
    {
      buf_idx = 0;
      buffered[buf_idx] = '\0';
      return 0;
    }
  else
    return (long)strlen (buffered + buf_idx);
}

/* Our special terminal keyboard reader.  It will be called by
   low-level libc functions when the application calls `read' or
   the ANSI-standard stream-oriented read functions.  If the
   caller wants to read the terminal, we redirect the call to
   the BIOS keyboard functions, since that lets us recognize more
   keys than DOS does.  */
static int
keyboard_read (__FSEXT_Fnumber func, int *retval, va_list rest_args)
{
  /* When we are called, REST_ARGS are: file_descriptor, buf, nbytes.  */
  unsigned char *buf;
  size_t nbytes, nread = 0;
  int fd = va_arg (rest_args, int);

  /* Is this call for us?  */
  if (func != __FSEXT_read || !isatty (fd))
    return 0;	/* and the usual DOS call will be issued */

  buf = va_arg (rest_args, unsigned char *);
  nbytes = va_arg (rest_args, size_t);

  if (!buf)
    {
      errno = EINVAL;
      *retval = -1;
      return 1;
    }
  if (!nbytes)
    {
      *retval = 0;
      return 1;
    }

  /* Loop here until enough bytes has been read.  */
  do
    {
      int key;

      /* If any ``buffered characters'' are left, return as much
	 of them as the caller wanted.  */
      while (buffered[buf_idx] && nbytes)
	{
	  *buf++ = buffered[buf_idx++];
	  nread++;
	  nbytes--;
	}

      if (nbytes <= 0)
	break;

      /* Wait for another key.
	 We do that in a busy-waiting loop so we don't get parked
	 inside a BIOS call, which will effectively disable signals.
         While we wait for them to type something, we repeatedly
         release the rest of our time slice, so that other programs
         in a multitasking environment, such as Windows, get more cycles.  */
      while (kbd_buffer_empty ())
	__dpmi_yield ();

      key = getxkey ();

      /* Translate the key if necessary.
	 Untranslated non-ASCII keys are silently ignored.  */
      if ((key & 0x300) != 0)
	{
	  unsigned char const * key_sequence = find_sequence (key);

	  if (key_sequence != NULL)
	    {
	      strcpy (buffered, key_sequence);
	      buf_idx = 0;
	    }
	}
      else if (key == K_Control_Z)
	raise (SIGUSR1);	/* we don't have SIGTSTP, so simulate it */
      else if (key <= 0xff)
	{
	  *buf++ = key;
	  nbytes--;
	  nread++;
	}
    }
  while (nbytes > 0);

  *retval = nread;
  return 1;	/* meaning that we handled the call */
}

/* Install our keyboard handler.
   This is called by the startup code before `main'.  */
static void __attribute__((constructor))
install_keyboard_handler (void)
{
  __FSEXT_set_function (fileno (stdin), keyboard_read);

  /* We need to set this single hook here; the rest
     will be set by pc_initialize_terminal when it is called.  */
  terminal_initialize_terminal_hook = pc_initialize_terminal;
}

#endif /* __DJGPP__ */

/* **************************************************************** */
/*                                                                  */
/*                Emulation of SIGTSTP on Ctrl-Z                    */
/*                                                                  */
/* **************************************************************** */

#include <limits.h>
#include "signals.h"
#include "session.h"

#ifndef PATH_MAX
# define PATH_MAX 512
#endif

/* Effectively disable signals which aren't defined
   (assuming no signal can ever be zero).
   SIGINT is ANSI, so we expect it to be always defined.  */
#ifndef SIGUSR1
# define SIGUSR1 0
#endif
#ifndef SIGQUIT
# define SIGQUIT 0
#endif

int
kill (pid_t pid, int sig)
{
  static char interrupted_msg[] = "Interrupted\r\n";
  static char stopped_msg[] = "Stopped.  Type `exit RET' to return.\r\n";
  char cwd[PATH_MAX + 1];

  if (pid == getpid ()
      || pid == 0
      || pid == -1
      || pid == -getpid ())
    {
      switch (sig)
	{
	RETSIGTYPE (*old_INT)(int), (*old_QUIT)(int);

	case SIGINT:
#ifdef __DJGPP__
	  /* If SIGINT was generated by a readable key, we want to remove
	     it from the PC keyboard buffer, so that DOS and other
	     programs never see it.  DJGPP signal-handling mechanism
	     doesn't remove the INT key from the keyboard buffer.  */
	  if (!kbd_buffer_empty ())
	    getxkey ();
#endif
	  pc_write_chars (interrupted_msg, sizeof (interrupted_msg) - 1);
	  xexit (1);
	case SIGUSR1:
	  /* Simulate SIGTSTP by invoking a subsidiary shell.  */
	  pc_goto_xy (0, outside_info.screenheight - 1);
	  pc_clear_to_eol ();
	  pc_write_chars (stopped_msg, sizeof (stopped_msg) - 1);

	  /* The child shell can change the working directory, so
	     we need to save and restore it, since it is global.  */
	  if (!getcwd (cwd, PATH_MAX)) /* should never happen */
	    cwd[0] = '\0';

	  /* We don't want to get fatal signals while the subshell runs.  */
	  old_INT = signal (SIGINT, SIG_IGN);
	  old_QUIT = signal (SIGQUIT, SIG_IGN);
	  system ("");
	  if (*cwd)
	    chdir (cwd);
	  signal (SIGINT, old_INT);
	  signal (SIGQUIT, old_QUIT);
	  break;
	default:
	  if (sig)
	    raise (sig);
	  break;
	}
      return 0;
    }
  else
    return -1;
}

/* These should never be called, but they make the linker happy.  */

void       tputs (char *a, int b, int (*c)())
{
  perror ("tputs");
}

char*     tgoto (char*a, int b, int c)
{
  perror ("tgoto"); return 0; /* here and below, added dummy retvals */
}

int       tgetnum (char*a)
{
  perror ("tgetnum"); return 0;
}

int       tgetflag (char*a)
{
  perror ("tgetflag"); return 0;
}

char*     tgetstr (char *a, char **b)
{
  perror ("tgetstr"); return 0;
}

int       tgetent (char*a, char*b)
{
  perror ("tgetent"); return 0;
}

int	tcgetattr(int fildes, struct termios *termios_p)
{
  perror ("tcgetattr"); return 0;
}

int	tcsetattr(int fd, int opt_actions, const struct termios *termios_p)
{
  perror ("tcsetattr"); return 0;
}
