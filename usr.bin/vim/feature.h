/*	$OpenBSD: feature.h,v 1.1.1.1 1996/09/07 21:40:28 downsj Exp $	*/
/* vi:set ts=8 sw=8:
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */
/*
 * feature.h: Defines for optional code and preferences
 *
 * Edit this file to include/exclude parts of Vim, before compiling.
 * The only other file that may be edited is Makefile, it contains machine
 * specific options.
 *
 * When you want to include a define, change the "#if 0" into "#if 1".
 */

/*
 * Optional code:
 * ==============
 */

/*
 * DIGRAPHS		When defined: Include digraph support.
 * 			In insert mode and on the command line you will be
 * 			able to use digraphs. The CTRL-K command will work.
 */
#define DIGRAPHS

/*
 * HAVE_LANGMAP		When defined: Include support for 'langmap' option.
 * 			Only useful when you put your keyboard in a special
 * 			language mode, e.g. for typing greek.
 */
#undef HAVE_LANGMAP

/*
 * INSERT_EXPAND	When defined: Support for CTRL-N/CTRL-P/CTRL-X in
 *			insert mode. Takes about 4Kbyte of code.
 */
#define INSERT_EXPAND

/*
 * RIGHTLEFT		When defined: Right-to-left typing and Hebrew support
 * 			Takes some code.
 */
#define RIGHTLEFT

/*
 * EMACS_TAGS		When defined: Include support for emacs style
 *			TAGS file. Takes some code.
 */
#define EMACS_TAGS

/*
 * AUTOCMD		When defined: Include support for ":autocmd"
 */
#define AUTOCMD

/*
 * VIMINFO		When defined: Include support for reading/writing
 *			the viminfo file. Takes about 8Kbyte of code.
 */
#define VIMINFO

/*
 * Choose one out of the following four:
 *
 * NO_BUILTIN_TCAPS	When defined: Do not include any builtin termcap
 *			entries (used only with HAVE_TGETENT defined).
 *
 * (nothing)		Machine specific termcap entries will be included.
 *
 * SOME_BUILTIN_TCAPS	When defined: Include most useful builtin termcap
 *			entries (used only with NO_BUILTIN_TCAPS not defined).
 *			This is the default.
 *
 * ALL_BUILTIN_TCAPS	When defined: Include all builtin termcap entries
 * 			(used only with NO_BUILTIN_TCAPS not defined).
 */
#define NO_BUILTIN_TCAPS

#ifndef NO_BUILTIN_TCAPS
# if 0
#  define ALL_BUILTIN_TCAPS
# else
#  if 1
#   define SOME_BUILTIN_TCAPS		/* default */
#  endif
# endif
#endif

/*
 * LISPINDENT		When defined: Include lisp indenting (From Eric
 *			Fischer). Doesn't completely work like vi (yet).
 * CINDENT		When defined: Include C code indenting (From Eric
 *			Fischer).
 * SMARTINDENT		When defined: Do smart C code indenting when the 'si'
 *			option is set. It's not as good as CINDENT, only
 *			included to keep the old code.
 *
 * These two need to be defined when making prototypes.
 */
#define LISPINDENT

#define CINDENT

#define SMARTINDENT

/*
 * Preferences:
 * ============
 */

/*
 * COMPATIBLE		When defined: Start in vi-compatible mode.
 *			Sets all option defaults to their vi-compatible value.
 */
#undef COMPATIBLE

/*
 * WRITEBACKUP		When defined: 'writebackup' is default on: Use
 *			a backup file while overwriting a file.
 */
#undef WRITEBACKUP

/*
 * SAVE_XTERM_SCREEN	When defined: The t_ti and t_te entries for the
 *			builtin xterm will be set to save the screen when
 *			starting Vim and restoring it when exiting.
 */
#define SAVE_XTERM_SCREEN

/*
 * DEBUG		When defined: Output a lot of debugging garbage.
 */
#undef DEBUG

/*
 * VIMRC_FILE		Name of the .vimrc file in current dir.
 */
#define VIMRC_FILE  	".vimrc"

/*
 * EXRC_FILE		Name of the .exrc file in current dir.
 */
#define EXRC_FILE	".exrc"

/*
 * GVIMRC_FILE		Name of the .gvimrc file in current dir.
 */
#define GVIMRC_FILE	".gvimrc"

/*
 * USR_VIMRC_FILE	Name of the user .vimrc file.
 */
#define USR_VIMRC_FILE		"$HOME/.vimrc"

/*
 * USR_EXRC_FILE	Name of the user .exrc file.
 */
#define USR_EXRC_FILE		"$HOME/.exrc"

/*
 * USR_GVIMRC_FILE	Name of the user .gvimrc file.
 */
#define USR_GVIMRC_FILE		"$HOME/.gvimrc"

/*
 * SYS_VIMRC_FILE	Name of the system-wide .vimrc file.
 */
#define SYS_VIMRC_FILE		"/etc/vimrc"

/*
 * SYS_COMPATRC_FILE	Name of the system-wide .vimrc file for compat mode.
 */
#define SYS_COMPATRC_FILE	"/etc/virc"

/*
 * SYS_GVIMRC_FILE	Name of the system-wide .gvimrc file.
 */
#define SYS_GVIMRC_FILE		"/etc/gvimrc"
  
/*
 * VIM_HLP		Name of the help file.
 */
#define VIM_HLP		"/usr/share/vim/vim_help.txt"


/*
 * Machine dependent:
 * ==================
 */

/*
 * USE_SYSTEM		Unix only. When defined: Use system() instead of
 *			fork/exec for starting a shell.
 */
#undef USE_SYSTEM

/*
 * WANT_X11		Unix only. When defined: Include code for xterm title
 *			saving. Only works if HAVE_X11 is also defined.
 */
#undef WANT_X11

/*
 * WANT_GUI		Would be nice, but that doesn't work. To compile Vim
 *			with the GUI (gvim) you have to edit Makefile.
 */

/*
 * NO_ARP		Amiga only. When defined: Do not use arp.library, DOS
 *			2.0 required. 
 */
#undef NO_ARP
